/*
 * mOS libSystem — dispatch_data_*.
 * No macOS o dispatch_data_t é ponte de NSData; aqui também: cada objeto de dados é um NSData (CFData) imutável,
 * com uma região contígua. dispatch_data_empty é um objeto estático com isa OS_dispatch_data (swiftobjc.c dá a ele
 * -length 0 e -bytes NULL). O destrutor do criador roda quando o conteúdo já foi copiado (o buffer não é mais lido).
 */
#ifdef MOS_HOST_TEST
#include "dispatch_plat.h"
/* no teste de host o NSData é um registro simples com contagem */
typedef struct hdata { long ref; size_t n; unsigned char b[]; } hdata_t;
static void *nd_new(const void *b, size_t n) { hdata_t *d = malloc(sizeof *d + n + 1); d->ref = 1; d->n = n; if (n) memcpy(d->b, b, n); return d; }
static size_t nd_len(void *d) { return ((hdata_t *)d)->n; }
static const unsigned char *nd_bytes(void *d) { return ((hdata_t *)d)->b; }
void *dp_host_data_retain(void *d) { __sync_fetch_and_add(&((hdata_t *)d)->ref, 1); return d; }
void dp_host_data_release(void *d) { if (__sync_sub_and_fetch(&((hdata_t *)d)->ref, 1) == 0) free(d); }
#define ND_RETAIN dp_host_data_retain
#define ND_RELEASE dp_host_data_release
extern char OS_dispatch_data_c[] __asm__("OBJC_CLASS_$_OS_dispatch_data");
#else
#include "dispatch_plat.h"
void *CFDataCreate(void *alloc, const u8 *bytes, long n);
long CFDataGetLength(void *d);
const u8 *CFDataGetBytePtr(void *d);
void *objc_retain(void *o);
void objc_release(void *o);
static void *nd_new(const void *b, size_t n) { return CFDataCreate(0, b, (long)n); }
static size_t nd_len(void *d) { return (size_t)CFDataGetLength(d); }
static const unsigned char *nd_bytes(void *d) { return CFDataGetBytePtr(d); }
#define ND_RETAIN objc_retain
#define ND_RELEASE objc_release
extern char OS_dispatch_data_c[] __asm__("OBJC_CLASS_$_OS_dispatch_data");
#endif

typedef void (*dispatch_function_t)(void *);
void *dispatch_get_main_queue(void);

/* DISPATCH_DATA_DESTRUCTOR_FREE / _MUNMAP: o endereço destas variáveis é o valor (no Mach-O: __dispatch_data_destructor_*) */
const void *_dispatch_data_destructor_free = (const void *)1;
const void *_dispatch_data_destructor_munmap = (const void *)2;
const void *_dispatch_data_destructor_none = (const void *)3;
/* dispatch_data_empty = &_dispatch_data_empty */
dp_obj_t _dispatch_data_empty = {OS_dispatch_data_c, DP_MAGIC, DK_DATA, -1, 0, 0, 0, 0, 0, 0};
#define EMPTY ((void *)&_dispatch_data_empty)

int munmap(void *a, size_t n);
static void run_destructor(void *b) { DP_RUN_BLOCK(b); _Block_release(b); }
static void destroy(const void *buf, size_t n, void *queue, void *destructor) {
    if (!destructor || destructor == (void *)&_dispatch_data_destructor_none || destructor == (void *)3) return;
    if (destructor == (void *)&_dispatch_data_destructor_free || destructor == (void *)1) { free((void *)buf); return; }
    if (destructor == (void *)&_dispatch_data_destructor_munmap || destructor == (void *)2) { munmap((void *)buf, n); return; }
    void *b = _Block_copy(destructor);
    if (queue) dispatch_async_f(queue, b, run_destructor);
    else run_destructor(b);
}
static int is_empty(void *d) { return !d || d == EMPTY || nd_len(d) == 0; }
static size_t dsize(void *d) { return d == EMPTY || !d ? 0 : nd_len(d); }
static const unsigned char *dbytes(void *d) { return d == EMPTY || !d ? 0 : nd_bytes(d); }

void *dispatch_data_create(const void *buf, size_t n, void *queue, void *destructor) {
    if (!buf || !n) { destroy(buf, n, queue, destructor); return EMPTY; }
    void *d = nd_new(buf, n);
    destroy(buf, n, queue, destructor);
    return d ? d : EMPTY;
}
size_t dispatch_data_get_size(void *d) { return dsize(d); }
/* uma região contígua: o próprio objeto (retido) mapeia os bytes */
void *dispatch_data_create_map(void *d, const void **buf, size_t *n) {
    if (is_empty(d)) { if (buf) *buf = 0; if (n) *n = 0; return EMPTY; }
    if (buf) *buf = dbytes(d);
    if (n) *n = dsize(d);
    return ND_RETAIN(d);
}
void *dispatch_data_create_concat(void *a, void *b) {
    size_t na = dsize(a), nb = dsize(b);
    if (!na && !nb) return EMPTY;
    if (!nb) return ND_RETAIN(a);
    if (!na) return ND_RETAIN(b);
    unsigned char *t = malloc(na + nb);
    if (!t) abort();
    memcpy(t, dbytes(a), na);
    memcpy(t + na, dbytes(b), nb);
    void *r = nd_new(t, na + nb);
    free(t);
    return r;
}
void *dispatch_data_create_subrange(void *d, size_t off, size_t len) {
    size_t n = dsize(d);
    if (off >= n || !len) return EMPTY;
    if (len > n - off) len = n - off;
    if (off == 0 && len == n) return ND_RETAIN(d);
    return nd_new(dbytes(d) + off, len);
}
/* applier(region, offset, buffer, size) -> bool: uma região só */
typedef struct { void *isa; int flags, reserved; _Bool (*invoke)(void *, void *, size_t, const void *, size_t); } dd_apply_blk_t;
_Bool dispatch_data_apply(void *d, void *applier) {
    if (is_empty(d)) return 1;
    dd_apply_blk_t *b = applier;
    return b->invoke(b, d, 0, dbytes(d), dsize(d));
}
_Bool dispatch_data_apply_f(void *d, void *ctx, _Bool (*fn)(void *, void *, size_t, const void *, size_t)) {
    if (is_empty(d)) return 1;
    return fn(ctx, d, 0, dbytes(d), dsize(d));
}
void *dispatch_data_copy_region(void *d, size_t location, size_t *offset) {
    if (is_empty(d) || location >= dsize(d)) { if (offset) *offset = 0; return EMPTY; }
    if (offset) *offset = 0;
    return ND_RETAIN(d);
}
