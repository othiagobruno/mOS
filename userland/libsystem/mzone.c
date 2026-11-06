/*
 * mOS libSystem — zonas do malloc (malloc_zone_t, como no macOS), por cima do malloc do libc.c.
 *
 * O Chromium no macOS pega a zona padrão (malloc_default_zone / malloc_get_all_zones), confere a
 * versão e troca os ponteiros de função dela pelos do PartitionAlloc. Por isso a zona precisa ter o
 * layout real, ficar numa página própria (ele faz mprotect nela) e ter versão >= 13.
 */
#include "sys.h"

void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void *realloc(void *p, size_t n);
void free(void *p);
size_t malloc_size(const void *p);
int posix_memalign(void **out, size_t align, size_t n);

typedef struct malloc_introspection {
    void *enumerator, *good_size, *check, *print, *log, *force_lock, *force_unlock, *statistics,
         *zone_locked, *enable_discharge_checking, *disable_discharge_checking, *discharge,
         *enumerate_discharged_pointers, *reinit_lock, *print_task, *task_statistics;
} malloc_introspection_t;

typedef struct malloc_zone {
    void *reserved1, *reserved2;
    size_t (*size)(struct malloc_zone *, const void *);
    void *(*malloc)(struct malloc_zone *, size_t);
    void *(*calloc)(struct malloc_zone *, size_t, size_t);
    void *(*valloc)(struct malloc_zone *, size_t);
    void (*free)(struct malloc_zone *, void *);
    void *(*realloc)(struct malloc_zone *, void *, size_t);
    void (*destroy)(struct malloc_zone *);
    const char *zone_name;
    unsigned (*batch_malloc)(struct malloc_zone *, size_t, void **, unsigned);
    void (*batch_free)(struct malloc_zone *, void **, unsigned);
    malloc_introspection_t *introspect;
    unsigned version;
    void *(*memalign)(struct malloc_zone *, size_t, size_t);
    void (*free_definite_size)(struct malloc_zone *, void *, size_t);
    size_t (*pressure_relief)(struct malloc_zone *, size_t);
    int (*claimed_address)(struct malloc_zone *, void *);
    void (*try_free_default)(struct malloc_zone *, void *);
} malloc_zone_t;

static size_t z_size(malloc_zone_t *z, const void *p) { (void)z; return p ? malloc_size(p) : 0; }
static void *z_malloc(malloc_zone_t *z, size_t n) { (void)z; return malloc(n); }
static void *z_calloc(malloc_zone_t *z, size_t n, size_t s) { (void)z; return calloc(n, s); }
static void *z_valloc(malloc_zone_t *z, size_t n) { (void)z; void *p; return posix_memalign(&p, 4096, n) ? 0 : p; }
static void z_free(malloc_zone_t *z, void *p) { (void)z; free(p); }
static void *z_realloc(malloc_zone_t *z, void *p, size_t n) { (void)z; return realloc(p, n); }
static void z_destroy(malloc_zone_t *z) { (void)z; }
static unsigned z_batch_malloc(malloc_zone_t *z, size_t n, void **out, unsigned num) {
    unsigned i = 0;
    for (; i < num; i++) if (!(out[i] = z->malloc(z, n))) break;
    return i;
}
static void z_batch_free(malloc_zone_t *z, void **ps, unsigned num) { for (unsigned i = 0; i < num; i++) z->free(z, ps[i]); }
static void *z_memalign(malloc_zone_t *z, size_t a, size_t n) { (void)z; void *p; return posix_memalign(&p, a < 16 ? 16 : a, n) ? 0 : p; }
static void z_free_definite(malloc_zone_t *z, void *p, size_t n) { (void)n; z->free(z, p); }
static size_t z_pressure(malloc_zone_t *z, size_t goal) { (void)z; (void)goal; return 0; }
static int z_claimed(malloc_zone_t *z, void *p) { (void)z; return p != 0; }
static void z_try_free_default(malloc_zone_t *z, void *p) { z->free(z, p); }

static int i_zero(void) { return 0; }
static void i_void(void) {}
static size_t i_good_size(malloc_zone_t *z, size_t n) { (void)z; return (n + 15) & ~(size_t)15; }

static malloc_introspection_t default_introspect = {
    .enumerator = (void *)i_zero, .good_size = (void *)i_good_size, .check = (void *)i_zero, .print = (void *)i_void,
    .log = (void *)i_void, .force_lock = (void *)i_void, .force_unlock = (void *)i_void, .statistics = (void *)i_void,
    .zone_locked = (void *)i_zero, .reinit_lock = (void *)i_void, .print_task = (void *)i_void, .task_statistics = (void *)i_void,
};

/* página própria: quem troca os ponteiros faz mprotect(zona, 4096, ...) */
static union { malloc_zone_t z; char page[4096]; } default_zone_page __attribute__((aligned(4096))) = {.z = {
    .size = z_size, .malloc = z_malloc, .calloc = z_calloc, .valloc = z_valloc, .free = z_free, .realloc = z_realloc,
    .destroy = z_destroy, .zone_name = "DefaultMallocZone", .batch_malloc = z_batch_malloc, .batch_free = z_batch_free,
    .introspect = &default_introspect, .version = 13, .memalign = z_memalign, .free_definite_size = z_free_definite,
    .pressure_relief = z_pressure, .claimed_address = z_claimed, .try_free_default = z_try_free_default,
}};
#define DZ (&default_zone_page.z)

static malloc_zone_t *zones[8] = {DZ};
static unsigned nzones = 1;

malloc_zone_t *malloc_default_zone(void) { return DZ; }
malloc_zone_t *malloc_default_purgeable_zone(void) { return DZ; }
malloc_zone_t *malloc_zone_from_ptr(const void *p) { return p ? DZ : 0; }

/* kern_return_t malloc_get_all_zones(task, reader, vm_address_t **addresses, unsigned *count) */
int malloc_get_all_zones(unsigned task, void *reader, u64 **addresses, unsigned *count) {
    (void)task; (void)reader;
    static u64 addrs[8];
    for (unsigned i = 0; i < nzones; i++) addrs[i] = (u64)zones[i];
    if (addresses) *addresses = addrs;
    if (count) *count = nzones;
    return 0;
}

void malloc_zone_register(malloc_zone_t *z) { if (nzones < 8) zones[nzones++] = z; }
void malloc_zone_unregister(malloc_zone_t *z) {
    for (unsigned i = 0; i < nzones; i++)
        if (zones[i] == z) { zones[i] = zones[--nzones]; break; }
}
malloc_zone_t *malloc_create_zone(size_t start, unsigned flags) { (void)start; (void)flags; return DZ; }
void malloc_destroy_zone(malloc_zone_t *z) { (void)z; }
void malloc_set_zone_name(malloc_zone_t *z, const char *name) { (void)z; (void)name; }
const char *malloc_get_zone_name(malloc_zone_t *z) { return z ? z->zone_name : 0; }

void *malloc_zone_malloc(malloc_zone_t *z, size_t n) { return (z ? z : DZ)->malloc(z ? z : DZ, n); }
void *malloc_zone_calloc(malloc_zone_t *z, size_t n, size_t s) { return (z ? z : DZ)->calloc(z ? z : DZ, n, s); }
void *malloc_zone_valloc(malloc_zone_t *z, size_t n) { return (z ? z : DZ)->valloc(z ? z : DZ, n); }
void *malloc_zone_realloc(malloc_zone_t *z, void *p, size_t n) { return (z ? z : DZ)->realloc(z ? z : DZ, p, n); }
void *malloc_zone_memalign(malloc_zone_t *z, size_t a, size_t n) { return (z ? z : DZ)->memalign(z ? z : DZ, a, n); }
void malloc_zone_free(malloc_zone_t *z, void *p) { (z ? z : DZ)->free(z ? z : DZ, p); }
size_t malloc_zone_pressure_relief(malloc_zone_t *z, size_t goal) { (void)z; (void)goal; return 0; }
int malloc_zone_claimed_address(malloc_zone_t *z, void *p) { (void)z; return p != 0; }
