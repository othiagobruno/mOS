/*
 * mOS libSystem — núcleo: partida, exit, malloc, string, ctype (formato do macOS), stdlib,
 * matemática, tempo e chamadas POSIX básicas. Roda em ring 3; fala com o kernel via sys.h.
 *
 * Os nomes seguem os símbolos do libSystem do macOS sem o "_" inicial do Mach-O
 * (ex.: o import "_printf" liga em printf, "___error" em __error).
 */
#include "sys.h"
#include "libc.h"

/* ================================================================ estado global */
char **environ;
int NXArgc;
char **NXArgv;
const char *__progname;
unsigned long __stack_chk_guard = 0x5f6d4f53434f4d00ULL;
unsigned int mach_task_self_ = 0x103;
u64 cx_image_base;

/* errno da thread: o int fica dentro do próprio slot TSD_ERRNO (sem FSGSBASE, cx_tsd devolve a tabela
 * global e o errno volta a ser um só, como antes) */
#define TSD_ERRNO 103
void **cx_tsd(void);
int *__error(void) { return (int *)&cx_tsd()[TSD_ERRNO]; }
int *__errno_location(void) { return __error(); }

static long ret_errno(long r) {
    if (r < 0) { cx_errno = (int)-r; return -1; }
    return r;
}

/* ================================================================ exit / atexit */
#define MAX_ATEXIT 64
static struct { void (*fn)(void *); void *arg; int kind; } atexit_tab[MAX_ATEXIT];
static int atexit_n;

int atexit(void (*fn)(void)) {
    if (atexit_n >= MAX_ATEXIT) return -1;
    atexit_tab[atexit_n].fn = (void (*)(void *))fn;
    atexit_tab[atexit_n].kind = 0;
    atexit_n++;
    return 0;
}

int __cxa_atexit(void (*fn)(void *), void *arg, void *dso) {
    (void)dso;
    if (atexit_n >= MAX_ATEXIT) return -1;
    atexit_tab[atexit_n].fn = fn;
    atexit_tab[atexit_n].arg = arg;
    atexit_tab[atexit_n].kind = 1;
    atexit_n++;
    return 0;
}

__attribute__((noreturn)) void _exit(int code) {
    SYS1(SYS_EXIT, code);
    for (;;) {}
}
__attribute__((noreturn)) void _Exit(int code) { _exit(code); }

__attribute__((noreturn)) void exit(int code) {
    while (atexit_n > 0) {
        atexit_n--;
        if (atexit_tab[atexit_n].kind) atexit_tab[atexit_n].fn(atexit_tab[atexit_n].arg);
        else ((void (*)(void))atexit_tab[atexit_n].fn)();
    }
    cx_stdio_flush_all();
    _exit(code);
}

__attribute__((noreturn)) void abort(void) {
    cx_fdprintf(2, "Abort trap: 6 (chamado de %p)\n", __builtin_return_address(0));
    cx_stdio_flush_all();
    _exit(134);
}

__attribute__((noreturn)) void __stack_chk_fail(void) {
    cx_puts_fd(2, "*** stack smashing detected ***: terminated\n");
    _exit(134);
}

__attribute__((noreturn)) void __assert_rtn(const char *func, const char *file, int line, const char *expr) {
    cx_fdprintf(2, "Assertion failed: (%s), function %s, file %s, line %d.\n", expr, func, file, line);
    abort();
}

/* chamado por stubs gerados pelo loader para símbolos que não existem aqui */
__attribute__((noreturn)) void cx_unresolved(const char *name) {
    cx_stdio_flush_all();
    cx_fdprintf(2, "dyld[mOS]: símbolo não implementado no libSystem do mOS: %s\n", name);
    _exit(127);
}

__attribute__((noreturn)) void dyld_stub_binder(void) { cx_unresolved("dyld_stub_binder"); }

/* ================================================================ partida */
typedef int (*main_fn)(int, char **, char **, char **);
typedef void (*init_fn)(int, char **, char **, char **, void *);

__attribute__((noreturn)) void cx_start(cx_start_t *s) {
    int argc = (int)s->argc;
    char **argv = (char **)s->argv, **envp = (char **)s->envp, **apple = (char **)s->apple;
    environ = envp;
    cx_image_base = s->image_base;
    /* no macOS o processo já começa com o TSD da thread principal no GS (gs:[0] = pthread_self); o Bun (Claude
     * Code) lê gs:[0] direto antes de qualquer chamada de pthread. cx_tsd (extra.c) cria e liga a tabela. */
    cx_libc_init();
    { extern void **cx_tsd(void); cx_tsd(); }                   /* depois do malloc pronto: cx_tsd usa calloc */
    NXArgc = argc;
    NXArgv = argv;
    __progname = argc > 0 ? argv[0] : "app";
    for (const char *p = __progname; *p; p++) if (*p == '/') __progname = p + 1;
    cx_stdio_init();
    if (s->n_objc_sects) { extern void cx_objc_boot(u64, u64); cx_objc_boot(s->objc_sects, s->n_objc_sects); }
    u64 *funcs = (u64 *)s->init_funcs;
    for (u64 i = 0; i < s->n_init_funcs; i++) ((init_fn)funcs[i])(argc, argv, envp, apple, 0);
    u32 *offs = (u32 *)s->init_offsets;
    for (u64 i = 0; i < s->n_init_offsets; i++) ((init_fn)(s->image_base + offs[i]))(argc, argv, envp, apple, 0);
    exit(((main_fn)s->main)(argc, argv, envp, apple));
}

/* ================================================================ malloc (brk) */
typedef struct blk { size_t size; struct blk *next; } blk_t;   /* size inclui o cabeçalho */
#define HDR 16
#define MAGIC_USED 0x00A110CEull
#define MAGIC_ALIGNED 0x0A11619Eull   /* bloco alinhado: cabeçalho = {ponteiro bruto, MAGIC_ALIGNED} */
static blk_t *free_list;
static char *heap_base, *heap_end;

static void *more_core(size_t bytes) {
    if (!heap_end) heap_base = heap_end = (char *)SYS1(SYS_BRK, 0);
    size_t grow = (bytes + 0xFFFF) & ~(size_t)0xFFFF;
    char *old = heap_end;
    long r = SYS1(SYS_BRK, old + grow);
    if (r < 0 || (char *)r < old + grow) return 0;
    heap_end = (char *)r;
    blk_t *b = (blk_t *)old;
    b->size = grow;
    return b;
}

static void insert_free(blk_t *b) {
    blk_t **pp = &free_list, *prev = 0;
    while (*pp && *pp < b) { prev = *pp; pp = &(*pp)->next; }
    b->next = *pp;
    *pp = b;
    if (b->next && (char *)b + b->size == (char *)b->next) { b->size += b->next->size; b->next = b->next->next; }
    if (prev && (char *)prev + prev->size == (char *)b) { prev->size += b->size; prev->next = b->next; }
}

/* o heap é compartilhado pelas threads (preemptivas): uma trava, no mesmo protocolo do pthread_mutex (extra.c):
 * 0 livre, 1 travada, 2 travada com alguém esperando no futex. Girar com SYS_YIELD fazia comboio: com o dono
 * preemptado no meio da varredura da lista livre, até 6 threads do VS Code ficavam prontas só esperando a vez. */
static volatile int heap_lock;
int cx_futex_wait(volatile int *w, int expected, long ms);
int cx_futex_wake(volatile int *w, int n);
static void hl(void) {
    int c = __sync_val_compare_and_swap(&heap_lock, 0, 1);
    if (!c) return;
    if (c != 2) c = __sync_lock_test_and_set(&heap_lock, 2);
    while (c) { cx_futex_wait(&heap_lock, 2, -1); c = __sync_lock_test_and_set(&heap_lock, 2); }
}
static void hu(void) { if (__sync_fetch_and_sub(&heap_lock, 1) != 1) { heap_lock = 0; cx_futex_wake(&heap_lock, 1); } }
static void *malloc_unlocked(size_t n);
static void free_unlocked(void *p);
void *malloc(size_t n) { hl(); void *p = malloc_unlocked(n); hu(); return p; }
/* Ponteiro fora do heap (brk): como no macOS, free/malloc_size/realloc procuram a zona dona (mzone.c). O Chromium troca
 * as funções da zona padrão pelas do PartitionAlloc e o Skia aloca por ela (base::UncheckedMalloc) mas chama
 * malloc_size()/free() do libSystem: ler o "cabeçalho" de um bloco do PartitionAlloc derrubava o renderer (falta de
 * página em malloc_size com um ponteiro alinhado à página). As versões cx_heap_* só conhecem o heap e são as que a zona
 * padrão usa: o shim do Chromium devolve à zona original o que não é dele, e passar de novo pelas zonas seria recursão. */
static int in_heap(const void *p) { return heap_base && (const char *)p >= heap_base && (const char *)p < heap_end; }
size_t cx_zone_size(const void *p);
int cx_zone_free(void *p);
void *cx_zone_realloc(void *p, size_t n, int *done);
static void not_allocated(const void *p) { cx_fdprintf(2, "malloc: *** error for object %p: pointer being freed was not allocated\n", p); abort(); }
void cx_heap_free(void *p) { if (!p) return; if (!in_heap(p)) not_allocated(p); hl(); free_unlocked(p); hu(); }
void free(void *p) {
    if (!p) return;
    if (!in_heap(p)) { if (!cx_zone_free(p)) not_allocated(p); return; }
    hl(); free_unlocked(p); hu();
}

/* caixas por tamanho para blocos pequenos (até SMALL_MAX bytes com o cabeçalho): listas LIFO por tamanho exato,
 * malloc e free em O(1), sem coalescer. A lista livre geral (ordenada por endereço, first-fit) fica só com os
 * blocos grandes: antes, cada malloc e cada free do Chromium/V8/Node percorria milhares de blocos com a trava
 * do heap presa. O cabeçalho é o mesmo (size + MAGIC_USED): malloc_size, realloc e posix_memalign não mudam,
 * e um free duplo continua sendo pego (o next de um bloco na caixa não é MAGIC_USED). */
#define SMALL_MAX 1024
#define NBINS (SMALL_MAX / 16 + 1)
static blk_t *bins[NBINS];
static void *general_alloc(size_t need);
/* reabastece a caixa de `need`: um pedaço da lista geral cortado em até 32 blocos desse tamanho */
static void bin_refill(size_t need) {
    char *c = general_alloc(need * 32);
    if (!c) return;
    blk_t *big = (blk_t *)(c - HDR);
    size_t total = big->size, k = total / need, rest = total - k * need;
    char *base = (char *)big;
    for (size_t i = 0; i < k; i++) {
        blk_t *b = (blk_t *)(base + i * need);
        b->size = need + (i == k - 1 ? rest : 0);    /* a sobra (< 64, múltiplo de 16) vai no último */
        size_t idx = b->size / 16;
        if (b->size <= SMALL_MAX) { b->next = bins[idx]; bins[idx] = b; }
        else { b->next = (blk_t *)MAGIC_USED; insert_free(b); }
    }
}
static void *malloc_unlocked(size_t n) {
    size_t need = (n + HDR + 15) & ~(size_t)15;
    if (need < 32) need = 32;
    if (need <= SMALL_MAX) {
        blk_t *b = bins[need / 16];
        if (!b) { bin_refill(need); b = bins[need / 16]; }
        if (b) {
            bins[need / 16] = b->next;
            b->next = (blk_t *)MAGIC_USED;
            return (char *)b + HDR;
        }
    }
    return general_alloc(need);
}
static void *general_alloc(size_t need) {
    for (int pass = 0; pass < 2; pass++) {
        for (blk_t **pp = &free_list; *pp; pp = &(*pp)->next) {
            blk_t *b = *pp;
            if (b->size < need) continue;
            if (b->size - need >= 64) {
                blk_t *rest = (blk_t *)((char *)b + need);
                rest->size = b->size - need;
                rest->next = b->next;
                *pp = rest;
                b->size = need;
            } else *pp = b->next;
            b->next = (blk_t *)MAGIC_USED;
            return (char *)b + HDR;
        }
        blk_t *nb = more_core(need);
        if (!nb) break;
        insert_free(nb);
    }
    cx_errno = CX_ENOMEM;
    return 0;
}

static void free_unlocked(void *p) {
    if (!p) return;
    blk_t *b = (blk_t *)((char *)p - HDR);
    if (b->next == (blk_t *)MAGIC_ALIGNED) { free_unlocked((void *)b->size); return; }
    if (b->next != (blk_t *)MAGIC_USED) { hu(); cx_fdprintf(2, "malloc: *** error for object %p: pointer being freed was not allocated\n", p); abort(); }
    if (b->size <= SMALL_MAX) { size_t idx = b->size / 16; b->next = bins[idx]; bins[idx] = b; return; }
    insert_free(b);
}

size_t cx_heap_size(const void *p) {
    if (!p || !in_heap(p)) return 0;
    blk_t *b = (blk_t *)((char *)p - HDR);
    if (b->next == (blk_t *)MAGIC_ALIGNED) return cx_heap_size((void *)b->size) - (size_t)((char *)p - (char *)b->size);
    if (b->next != (blk_t *)MAGIC_USED) return 0;            /* não é início de bloco em uso */
    return b->size - HDR;
}
size_t malloc_size(const void *p) {
    if (!p) return 0;
    return in_heap(p) ? cx_heap_size(p) : cx_zone_size(p);
}
size_t malloc_good_size(size_t n) { return (n + 15) & ~(size_t)15; }

void *calloc(size_t n, size_t s) {
    if (s && n > (size_t)-1 / s) { cx_errno = CX_ENOMEM; return 0; }
    void *p = malloc(n * s);
    if (p) memset(p, 0, n * s);
    return p;
}

void *cx_heap_realloc(void *p, size_t n) {
    if (!p) return malloc(n);
    if (!in_heap(p)) not_allocated(p);
    if (!n) { cx_heap_free(p); return 0; }
    size_t have = cx_heap_size(p);
    if (have >= n) return p;
    void *q = malloc(n);
    if (q) { memcpy(q, p, have); cx_heap_free(p); }
    return q;
}
void *realloc(void *p, size_t n) {
    if (p && !in_heap(p)) {
        int done = 0;
        void *q = cx_zone_realloc(p, n, &done);
        if (!done) not_allocated(p);
        return q;
    }
    return cx_heap_realloc(p, n);
}

void *reallocf(void *p, size_t n) { void *q = realloc(p, n); if (!q) free(p); return q; }
void *valloc(size_t n) { return malloc(n); }

int posix_memalign(void **out, size_t align, size_t n) {
    if (align <= 16) { *out = malloc(n); return *out ? 0 : CX_ENOMEM; }
    if (align & (align - 1)) return CX_EINVAL;
    char *raw = malloc(n + align + HDR);
    if (!raw) return CX_ENOMEM;
    char *al = (char *)(((u64)raw + HDR + align - 1) & ~(u64)(align - 1));
    blk_t *h = (blk_t *)(al - HDR);
    h->size = (size_t)raw;
    h->next = (blk_t *)MAGIC_ALIGNED;
    *out = al;
    return 0;
}
void *aligned_alloc(size_t align, size_t n) { void *p; return posix_memalign(&p, align, n) ? 0 : p; }
void *memalign(size_t align, size_t n) { return aligned_alloc(align, n); }

/* ================================================================ memória e strings */
#if defined(__aarch64__)
/* arm64: memcpy/memmove/memset/bzero em libc_arm64.S (laços de 64 bytes com ldp/stp de q, como os da Apple) */
#else
void *memcpy(void *d, const void *s, size_t n) {
    void *r = d;
    __asm__ volatile("rep movsb" : "+D"(d), "+S"(s), "+c"(n) :: "memory");
    return r;
}

void *memmove(void *d, const void *s, size_t n) {
    u8 *a = d;
    const u8 *b = s;
    if (a == b || !n) return d;
    if (a < b || a >= b + n) return memcpy(d, s, n);
    const u8 *se = b + n - 1;
    u8 *de = a + n - 1;
    __asm__ volatile("std; rep movsb; cld" : "+D"(de), "+S"(se), "+c"(n) :: "memory");
    return d;
}

void *memset(void *d, int c, size_t n) {
    void *r = d;
    __asm__ volatile("rep stosb" : "+D"(d), "+c"(n) : "a"(c) : "memory");
    return r;
}
#endif

/* o clang da Apple troca memset(p, 0, n) por __bzero(p, n) e usa o retorno como o do memset: no macOS o
 * __bzero deixa o destino em rax. Como void, o rep stosb inlinado deixava rax = 0 (o git recebia NULL). */
void *bzero(void *d, size_t n) { return memset(d, 0, n); }
void *__bzero(void *d, size_t n) { return memset(d, 0, n); }
void bcopy(const void *s, void *d, size_t n) { memmove(d, s, n); }
void memset_pattern16(void *d, const void *pat, size_t n) {
    u8 *a = d;
    for (size_t i = 0; i < n; i++) a[i] = ((const u8 *)pat)[i & 15];
}

int memcmp(const void *a, const void *b, size_t n) {
    const u8 *x = a, *y = b;
    for (size_t i = 0; i < n; i++) if (x[i] != y[i]) return x[i] - y[i];
    return 0;
}
int bcmp(const void *a, const void *b, size_t n) { return memcmp(a, b, n); }

void *memchr(const void *s, int c, size_t n) {
    const u8 *p = s;
    for (size_t i = 0; i < n; i++) if (p[i] == (u8)c) return (void *)(p + i);
    return 0;
}

void *memrchr(const void *s, int c, size_t n) {
    const u8 *p = s;
    while (n--) if (p[n] == (u8)c) return (void *)(p + n);
    return 0;
}

size_t strlen(const char *s) { const char *p = s; while (*p) p++; return (size_t)(p - s); }
size_t strnlen(const char *s, size_t n) { size_t i = 0; while (i < n && s[i]) i++; return i; }

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (u8)*a - (u8)*b;
}

int strncmp(const char *a, const char *b, size_t n) {
    for (; n; n--, a++, b++) {
        if (*a != *b) return (u8)*a - (u8)*b;
        if (!*a) return 0;
    }
    return 0;
}

int strcasecmp(const char *a, const char *b) {
    for (;; a++, b++) {
        int x = tolower((u8)*a), y = tolower((u8)*b);
        if (x != y || !x) return x - y;
    }
}

int strncasecmp(const char *a, const char *b, size_t n) {
    for (; n; n--, a++, b++) {
        int x = tolower((u8)*a), y = tolower((u8)*b);
        if (x != y || !x) return x - y;
    }
    return 0;
}

int strcoll(const char *a, const char *b) { return strcmp(a, b); }
size_t strxfrm(char *d, const char *s, size_t n) { size_t l = strlen(s); if (n) { size_t k = l < n - 1 ? l : n - 1; memcpy(d, s, k); d[k] = 0; } return l; }

char *strcpy(char *d, const char *s) { char *r = d; while ((*d++ = *s++)) {} return r; }
char *stpcpy(char *d, const char *s) { while ((*d = *s++)) d++; return d; }

char *strncpy(char *d, const char *s, size_t n) {
    size_t i = 0;
    for (; i < n && s[i]; i++) d[i] = s[i];
    for (; i < n; i++) d[i] = 0;
    return d;
}

char *strcat(char *d, const char *s) { strcpy(d + strlen(d), s); return d; }

char *strncat(char *d, const char *s, size_t n) {
    char *e = d + strlen(d);
    size_t i = 0;
    for (; i < n && s[i]; i++) e[i] = s[i];
    e[i] = 0;
    return d;
}

size_t strlcpy(char *d, const char *s, size_t n) {
    size_t l = strlen(s);
    if (n) { size_t k = l < n - 1 ? l : n - 1; memcpy(d, s, k); d[k] = 0; }
    return l;
}

size_t strlcat(char *d, const char *s, size_t n) {
    size_t dl = strnlen(d, n);
    if (dl == n) return n + strlen(s);
    return dl + strlcpy(d + dl, s, n - dl);
}

char *strchr(const char *s, int c) {
    for (;; s++) {
        if (*s == (char)c) return (char *)s;
        if (!*s) return 0;
    }
}

char *strrchr(const char *s, int c) {
    const char *r = 0;
    for (;; s++) {
        if (*s == (char)c) r = s;
        if (!*s) return (char *)r;
    }
}

char *strchrnul(const char *s, int c) { while (*s && *s != (char)c) s++; return (char *)s; }
char *index(const char *s, int c) { return strchr(s, c); }
char *rindex(const char *s, int c) { return strrchr(s, c); }

char *strstr(const char *h, const char *n) {
    size_t nl = strlen(n);
    if (!nl) return (char *)h;
    for (; *h; h++) if (*h == *n && !strncmp(h, n, nl)) return (char *)h;
    return 0;
}

char *strcasestr(const char *h, const char *n) {
    size_t nl = strlen(n);
    if (!nl) return (char *)h;
    for (; *h; h++) if (!strncasecmp(h, n, nl)) return (char *)h;
    return 0;
}

char *strnstr(const char *h, const char *n, size_t len) {
    size_t nl = strlen(n);
    if (!nl) return (char *)h;
    for (size_t i = 0; i + nl <= len && h[i]; i++) if (!strncmp(h + i, n, nl)) return (char *)h + i;
    return 0;
}

size_t strspn(const char *s, const char *a) { size_t i = 0; while (s[i] && strchr(a, s[i])) i++; return i; }
size_t strcspn(const char *s, const char *r) { size_t i = 0; while (s[i] && !strchr(r, s[i])) i++; return i; }
char *strpbrk(const char *s, const char *a) { s += strcspn(s, a); return *s ? (char *)s : 0; }

char *strtok_r(char *s, const char *d, char **save) {
    if (!s) s = *save;
    if (!s) return 0;
    s += strspn(s, d);
    if (!*s) { *save = 0; return 0; }
    char *e = s + strcspn(s, d);
    if (*e) { *e = 0; *save = e + 1; } else *save = 0;
    return s;
}
char *strtok(char *s, const char *d) { static char *save; return strtok_r(s, d, &save); }

char *strsep(char **sp, const char *d) {
    char *s = *sp;
    if (!s) return 0;
    char *e = s + strcspn(s, d);
    if (*e) { *e = 0; *sp = e + 1; } else *sp = 0;
    return s;
}

char *strdup(const char *s) { size_t n = strlen(s) + 1; char *d = malloc(n); if (d) memcpy(d, s, n); return d; }
char *strndup(const char *s, size_t n) { size_t l = strnlen(s, n); char *d = malloc(l + 1); if (d) { memcpy(d, s, l); d[l] = 0; } return d; }

char *strerror(int e) {
    switch (e) {
    case 0: return "Undefined error: 0";
    case CX_EPERM: return "Operation not permitted";
    case CX_ENOENT: return "No such file or directory";
    case 3: return "No such process";                    /* ESRCH (os seis daqui: textos do macOS, que um terminal mostra) */
    case 4: return "Interrupted system call";            /* EINTR */
    case 6: return "Device not configured";              /* ENXIO: /dev/tty sem terminal de controle */
    case 10: return "No child processes";                /* ECHILD */
    case 32: return "Broken pipe";                       /* EPIPE: "history 1 | sed" com o sed ausente dava "Unknown error" */
    case 35: return "Resource temporarily unavailable";  /* EAGAIN */
    case CX_EIO: return "Input/output error";
    case CX_EBADF: return "Bad file descriptor";
    case CX_ENOMEM: return "Cannot allocate memory";
    case CX_EACCES: return "Permission denied";
    case CX_EEXIST: return "File exists";
    case CX_ENOTDIR: return "Not a directory";
    case CX_EISDIR: return "Is a directory";
    case CX_EINVAL: return "Invalid argument";
    case CX_EMFILE: return "Too many open files";
    case CX_ENOTTY: return "Inappropriate ioctl for device";
    case CX_ENOSPC: return "No space left on device";
    case CX_ESPIPE: return "Illegal seek";
    case CX_EROFS: return "Read-only file system";
    case CX_ERANGE: return "Result too large";
    case CX_ENOSYS: return "Function not implemented";
    }
    return "Unknown error";
}

int strerror_r(int e, char *buf, size_t n) { strlcpy(buf, strerror(e), n); return 0; }

/* variantes "fortify" (_FORTIFY_SOURCE): checam o tamanho do destino */
static __attribute__((noreturn)) void chk_fail(void) {
    cx_puts_fd(2, "detected buffer overflow\n");
    abort();
}
void *__memcpy_chk(void *d, const void *s, size_t n, size_t dl) { if (n > dl) chk_fail(); return memcpy(d, s, n); }
void *__memmove_chk(void *d, const void *s, size_t n, size_t dl) { if (n > dl) chk_fail(); return memmove(d, s, n); }
void *__memset_chk(void *d, int c, size_t n, size_t dl) { if (n > dl) chk_fail(); return memset(d, c, n); }
char *__strcpy_chk(char *d, const char *s, size_t dl) { if (strlen(s) >= dl) chk_fail(); return strcpy(d, s); }
char *__stpcpy_chk(char *d, const char *s, size_t dl) { if (strlen(s) >= dl) chk_fail(); return stpcpy(d, s); }
char *__strcat_chk(char *d, const char *s, size_t dl) { if (strlen(d) + strlen(s) >= dl) chk_fail(); return strcat(d, s); }
char *__strncpy_chk(char *d, const char *s, size_t n, size_t dl) { if (n > dl) chk_fail(); return strncpy(d, s, n); }
char *__strncat_chk(char *d, const char *s, size_t n, size_t dl) { (void)dl; return strncat(d, s, n); }
size_t __strlcpy_chk(char *d, const char *s, size_t n, size_t dl) { if (n > dl) chk_fail(); return strlcpy(d, s, n); }
size_t __strlcat_chk(char *d, const char *s, size_t n, size_t dl) { if (n > dl) chk_fail(); return strlcat(d, s, n); }

/* ================================================================ ctype (layout do macOS) */
/* _RuneLocale: magic[8], encoding[32], sgetrune, sputrune, invalid_rune, runetype[256],
 * maplower[256], mapupper[256], ... — os macros de <ctype.h> leem estas tabelas diretamente. */
#define _A 0x00000100u
#define _C 0x00000200u
#define _D 0x00000400u
#define _G 0x00000800u
#define _L 0x00001000u
#define _P 0x00002000u
#define _S 0x00004000u
#define _U 0x00008000u
#define _X 0x00010000u
#define _B 0x00020000u
#define _R 0x00040000u

typedef struct {
    char magic[8];
    char encoding[32];
    void *sgetrune, *sputrune;
    int invalid_rune;
    u32 runetype[256];
    int maplower[256];
    int mapupper[256];
    u8 rest[256];   /* runetype_ext etc.: zerado */
} rune_locale_t;

rune_locale_t _DefaultRuneLocale;
rune_locale_t *_CurrentRuneLocale = &_DefaultRuneLocale;

static void ctype_init(void) {
    rune_locale_t *r = &_DefaultRuneLocale;
    memcpy(r->magic, "RuneMagA", 8);
    memcpy(r->encoding, "NONE", 5);
    r->invalid_rune = 0xFFFD;
    for (int c = 0; c < 256; c++) {
        u32 t = 0;
        if (c < 0x20 || c == 0x7F) t |= _C;
        if (c == ' ' || (c >= '\t' && c <= '\r')) t |= _S;
        if (c == ' ' || c == '\t') t |= _B;
        if (c >= '0' && c <= '9') t |= _D | _X | _G | _R | (u32)(c - '0');
        if (c >= 'a' && c <= 'z') t |= _L | _A | _G | _R;
        if (c >= 'A' && c <= 'Z') t |= _U | _A | _G | _R;
        if ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) t |= _X | (u32)(10 + ((c | 0x20) - 'a'));
        if (c > 0x20 && c < 0x7F && !(t & (_A | _D))) t |= _P | _G | _R;
        if (c == ' ') t |= _R;
        r->runetype[c] = c < 0x80 ? t : 0;
        r->maplower[c] = (c >= 'A' && c <= 'Z') ? c + 32 : c;
        r->mapupper[c] = (c >= 'a' && c <= 'z') ? c - 32 : c;
    }
}

int __maskrune(int c, unsigned long mask) { return (c >= 0 && c < 256) ? (int)(_DefaultRuneLocale.runetype[c] & mask) : 0; }
int __maskrune_l(int c, unsigned long mask, void *l) { (void)l; return __maskrune(c, mask); }
int __istype(int c, unsigned long mask) { return !!__maskrune(c, mask); }
int __isctype(int c, unsigned long mask) { return !!__maskrune(c, mask); }
int __tolower(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }
int __toupper(int c) { return (c >= 'a' && c <= 'z') ? c - 32 : c; }
int ___tolower(int c) { return __tolower(c); }
int ___toupper(int c) { return __toupper(c); }
int tolower(int c) { return __tolower(c); }
int toupper(int c) { return __toupper(c); }
int isalpha(int c) { return (c | 32) >= 'a' && (c | 32) <= 'z'; }
int isdigit(int c) { return c >= '0' && c <= '9'; }
int isalnum(int c) { return isalpha(c) || isdigit(c); }
int isspace(int c) { return c == ' ' || (c >= '\t' && c <= '\r'); }
int isupper(int c) { return c >= 'A' && c <= 'Z'; }
int islower(int c) { return c >= 'a' && c <= 'z'; }
int isxdigit(int c) { return isdigit(c) || ((c | 32) >= 'a' && (c | 32) <= 'f'); }
int isprint(int c) { return c >= 0x20 && c < 0x7F; }
int isgraph(int c) { return c > 0x20 && c < 0x7F; }
int ispunct(int c) { return isgraph(c) && !isalnum(c); }
int iscntrl(int c) { return (c >= 0 && c < 0x20) || c == 0x7F; }
int isblank(int c) { return c == ' ' || c == '\t'; }
int isascii(int c) { return c >= 0 && c < 0x80; }
int toascii(int c) { return c & 0x7F; }
int digittoint(int c) { return isdigit(c) ? c - '0' : isxdigit(c) ? 10 + ((c | 32) - 'a') : 0; }

/* ================================================================ stdlib */
int abs(int x) { return x < 0 ? -x : x; }
long labs(long x) { return x < 0 ? -x : x; }
long long llabs(long long x) { return x < 0 ? -x : x; }
typedef struct { int quot, rem; } div_t;
typedef struct { long quot, rem; } ldiv_t;
div_t div(int a, int b) { div_t r = {a / b, a % b}; return r; }
ldiv_t ldiv(long a, long b) { ldiv_t r = {a / b, a % b}; return r; }

static unsigned long long parse_ull(const char *s, char **end, int base, int *neg, int *ok) {
    const char *p = s;
    while (isspace((u8)*p)) p++;
    *neg = 0;
    if (*p == '+' || *p == '-') { *neg = *p == '-'; p++; }
    if ((base == 0 || base == 16) && p[0] == '0' && (p[1] | 32) == 'x' && isxdigit((u8)p[2])) { p += 2; base = 16; }
    else if (base == 0 && p[0] == '0') base = 8;
    else if (base == 0) base = 10;
    unsigned long long v = 0;
    const char *start = p;
    *ok = 1;
    for (;; p++) {
        int d;
        if (isdigit((u8)*p)) d = *p - '0';
        else if (isalpha((u8)*p)) d = (*p | 32) - 'a' + 10;
        else break;
        if (d >= base) break;
        unsigned long long nv = v * base + d;
        if (nv / base != v && *ok) { *ok = 0; }
        v = nv;
    }
    if (end) *end = (char *)(p == start ? s : p);
    return v;
}

long long strtoll(const char *s, char **end, int base) {
    int neg, ok;
    unsigned long long v = parse_ull(s, end, base, &neg, &ok);
    if (!ok || v > (neg ? 0x8000000000000000ULL : 0x7FFFFFFFFFFFFFFFULL)) { cx_errno = CX_ERANGE; return neg ? (-0x7FFFFFFFFFFFFFFFLL - 1) : 0x7FFFFFFFFFFFFFFFLL; }
    return neg ? -(long long)v : (long long)v;
}
long strtol(const char *s, char **end, int base) { return (long)strtoll(s, end, base); }
unsigned long long strtoull(const char *s, char **end, int base) {
    int neg, ok;
    unsigned long long v = parse_ull(s, end, base, &neg, &ok);
    if (!ok) { cx_errno = CX_ERANGE; return ~0ULL; }
    return neg ? -v : v;
}
unsigned long strtoul(const char *s, char **end, int base) { return (unsigned long)strtoull(s, end, base); }
long long strtoq(const char *s, char **end, int base) { return strtoll(s, end, base); }
unsigned long long strtouq(const char *s, char **end, int base) { return strtoull(s, end, base); }
long long strtoimax(const char *s, char **end, int base) { return strtoll(s, end, base); }
unsigned long long strtoumax(const char *s, char **end, int base) { return strtoull(s, end, base); }
int atoi(const char *s) { return (int)strtol(s, 0, 10); }
long atol(const char *s) { return strtol(s, 0, 10); }
long long atoll(const char *s) { return strtoll(s, 0, 10); }

double strtod(const char *s, char **end) {
    const char *p = s;
    while (isspace((u8)*p)) p++;
    int neg = 0;
    if (*p == '+' || *p == '-') { neg = *p == '-'; p++; }
    if (!strncasecmp(p, "inf", 3)) { if (end) *end = (char *)p + (strncasecmp(p, "infinity", 8) ? 3 : 8); return neg ? -__builtin_inf() : __builtin_inf(); }
    if (!strncasecmp(p, "nan", 3)) { if (end) *end = (char *)p + 3; return __builtin_nan(""); }
    double v = 0;
    int digits = 0, exp10 = 0;
    while (isdigit((u8)*p)) { v = v * 10 + (*p++ - '0'); digits++; }
    if (*p == '.') {
        p++;
        while (isdigit((u8)*p)) { v = v * 10 + (*p++ - '0'); exp10--; digits++; }
    }
    if (!digits) { if (end) *end = (char *)s; return 0; }
    if ((*p | 32) == 'e') {
        const char *q = p + 1;
        int eneg = 0, e = 0;
        if (*q == '+' || *q == '-') { eneg = *q == '-'; q++; }
        if (isdigit((u8)*q)) {
            while (isdigit((u8)*q)) { if (e < 10000) e = e * 10 + (*q - '0'); q++; }
            exp10 += eneg ? -e : e;
            p = q;
        }
    }
    if (end) *end = (char *)p;
    double scale = 1, base = 10;
    int e = exp10 < 0 ? -exp10 : exp10;
    while (e) { if (e & 1) scale *= base; base *= base; e >>= 1; }
    v = exp10 < 0 ? v / scale : v * scale;
    return neg ? -v : v;
}
float strtof(const char *s, char **end) { return (float)strtod(s, end); }
long double strtold(const char *s, char **end) { return strtod(s, end); }
double atof(const char *s) { return strtod(s, 0); }

static void swap_bytes(u8 *a, u8 *b, size_t n) { while (n--) { u8 t = *a; *a++ = *b; *b++ = t; } }

void qsort(void *base, size_t n, size_t sz, int (*cmp)(const void *, const void *)) {
    /* heapsort: O(n log n) garantido, sem recursão */
    u8 *a = base;
    if (n < 2) return;
    for (size_t start = n / 2; start-- > 0;) {
        size_t root = start;
        for (;;) {
            size_t child = root * 2 + 1;
            if (child >= n) break;
            if (child + 1 < n && cmp(a + child * sz, a + (child + 1) * sz) < 0) child++;
            if (cmp(a + root * sz, a + child * sz) >= 0) break;
            swap_bytes(a + root * sz, a + child * sz, sz);
            root = child;
        }
    }
    for (size_t end = n - 1; end > 0; end--) {
        swap_bytes(a, a + end * sz, sz);
        size_t root = 0;
        for (;;) {
            size_t child = root * 2 + 1;
            if (child >= end) break;
            if (child + 1 < end && cmp(a + child * sz, a + (child + 1) * sz) < 0) child++;
            if (cmp(a + root * sz, a + child * sz) >= 0) break;
            swap_bytes(a + root * sz, a + child * sz, sz);
            root = child;
        }
    }
}

void qsort_r(void *base, size_t n, size_t sz, void *thunk, int (*cmp)(void *, const void *, const void *)) {
    /* inserção estável: suficiente para o uso típico */
    u8 *a = base, *tmp = malloc(sz);
    if (!tmp) return;
    for (size_t i = 1; i < n; i++) {
        memcpy(tmp, a + i * sz, sz);
        size_t j = i;
        while (j > 0 && cmp(thunk, a + (j - 1) * sz, tmp) > 0) { memcpy(a + j * sz, a + (j - 1) * sz, sz); j--; }
        memcpy(a + j * sz, tmp, sz);
    }
    free(tmp);
}

void *bsearch(const void *key, const void *base, size_t n, size_t sz, int (*cmp)(const void *, const void *)) {
    size_t lo = 0, hi = n;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        const void *e = (const u8 *)base + mid * sz;
        int c = cmp(key, e);
        if (!c) return (void *)e;
        if (c < 0) hi = mid; else lo = mid + 1;
    }
    return 0;
}

static u64 rng_state = 1;
void srand(unsigned s) { rng_state = s ? s : 1; }
int rand(void) { rng_state = rng_state * 1103515245 + 12345; return (int)((rng_state >> 16) & 0x7FFFFFFF); }
void srandom(unsigned s) { srand(s); }
long random(void) { return rand(); }
static u64 xs = 0x9E3779B97F4A7C15ULL;
u32 arc4random(void) {
    if (xs == 0x9E3779B97F4A7C15ULL) xs ^= (u64)SYS0(SYS_TIME) * 0x2545F4914F6CDD1DULL + (u64)SYS0(SYS_UNIXTIME);
    xs ^= xs << 13; xs ^= xs >> 7; xs ^= xs << 17;
    return (u32)(xs >> 16);
}
u32 arc4random_uniform(u32 n) { return n ? arc4random() % n : 0; }
void arc4random_buf(void *buf, size_t n) { u8 *b = buf; while (n--) *b++ = (u8)arc4random(); }

char *getenv(const char *name) {
    size_t n = strlen(name);
    for (char **e = environ; e && *e; e++)
        if (!strncmp(*e, name, n) && (*e)[n] == '=') return *e + n + 1;
    return 0;
}

int setenv(const char *name, const char *val, int overwrite) {
    if (getenv(name) && !overwrite) return 0;
    size_t cnt = 0;
    while (environ && environ[cnt]) cnt++;
    char **ne = malloc((cnt + 2) * sizeof(char *));
    if (!ne) return -1;
    size_t k = 0, nl = strlen(name);
    for (size_t i = 0; i < cnt; i++) if (strncmp(environ[i], name, nl) || environ[i][nl] != '=') ne[k++] = environ[i];
    char *kv = malloc(nl + strlen(val) + 2);
    if (!kv) return -1;
    strcpy(kv, name); kv[nl] = '='; strcpy(kv + nl + 1, val);
    ne[k++] = kv;
    ne[k] = 0;
    environ = ne;
    return 0;
}
int unsetenv(const char *name) {
    size_t nl = strlen(name), k = 0;
    for (size_t i = 0; environ && environ[i]; i++) if (strncmp(environ[i], name, nl) || environ[i][nl] != '=') environ[k++] = environ[i];
    if (environ) environ[k] = 0;
    return 0;
}
char ***_NSGetEnviron(void) { return &environ; }
int *_NSGetArgc(void) { return &NXArgc; }
char ***_NSGetArgv(void) { return &NXArgv; }
const char *getprogname(void) { return __progname; }
void setprogname(const char *p) { __progname = p; }
int system(const char *cmd) { (void)cmd; cx_errno = CX_ENOSYS; return -1; }

/* ================================================================ POSIX básico */
/* sockets (sockets.c) vivem em descritores do libSystem 768-831 (abaixo do FD_SETSIZE do select) */
int cx_sock_is(int fd) __attribute__((weak));
long cx_sock_read(int fd, void *b, size_t n) __attribute__((weak));
long cx_sock_write(int fd, const void *b, size_t n) __attribute__((weak));
void cx_sock_set_nonblock(int fd, int on) __attribute__((weak));
int cx_sock_get_nonblock(int fd) __attribute__((weak));
#define IS_SOCK(fd) ((fd) >= 768 && (fd) < 1024 && cx_sock_is && cx_sock_is(fd))
/* diagnóstico do porte do VS Code: o Chromium registra as recusas do Mojo ("Invalid message: ...") sem dizer qual
 * campo falhou. Quando esse texto passa por aqui, a cadeia de frames (rbp; o Chromium do Mac mantém o ponteiro de
 * frame) vai junto para a serial, como imagem+deslocamento: o chamador de ReportValidationError é o Accept do stub,
 * e é dali que se acha o StructTraits::Read que devolveu false. */
typedef struct { const char *dli_fname; void *dli_fbase; const char *dli_sname; void *dli_saddr; } cx_dlinfo_t;
int dladdr(const void *addr, cx_dlinfo_t *info);
int snprintf(char *b, size_t n, const char *fmt, ...);
static void diag_mojo_backtrace(const void *b, size_t n) {
    static const char pat[] = "Invalid message:";
    const char *s = b;
    int hit = 0;
    for (size_t i = 0; i + sizeof pat - 1 <= n && !hit; i++) if (s[i] == 'I' && !memcmp(s + i, pat, sizeof pat - 1)) hit = 1;
    if (!hit) return;
    u64 *fp = (u64 *)__builtin_frame_address(0);
    char line[1400];
    int o = snprintf(line, sizeof line, "diag[mojo]: pilha:");
    for (int i = 0; i < 24 && fp; i++) {
        u64 next = fp[0], ret = fp[1];
        if (!ret) break;
        cx_dlinfo_t di;
        const char *img = "?";
        u64 off = ret;
        if (dladdr((void *)ret, &di) && di.dli_fbase) {
            img = di.dli_fname ? di.dli_fname : "?";
            const char *sl = strrchr(img, '/');
            if (sl) img = sl + 1;
            off = ret - (u64)di.dli_fbase;
        }
        o += snprintf(line + o, sizeof line - (size_t)o, " %s+0x%llx", img, (unsigned long long)off);
        if (o > (int)sizeof line - 80) break;
        if (next <= (u64)fp || next - (u64)fp > (8u << 20) || (next & 7)) break;
        fp = (u64 *)next;
    }
    line[o++] = '\n';
    SYS3(SYS_WRITE, 2, line, o);
}
/* O_NONBLOCK nos fds do kernel (pipes): o kernel só bloqueia; aqui a leitura de pipe vazio e a escrita em pipe
 * cheio viram EAGAIN quando o fd está marcado. O curl (par de "despertar" num socketpair = pipe) drena o pipe
 * lendo até EAGAIN; sem isso a segunda leitura esperava para sempre. Herdado no fork, como a tabela do kernel. */
#define CX_NBMAX 256
static unsigned char cx_nb[CX_NBMAX];
static int nb_on(int fd) { return fd >= 0 && fd < CX_NBMAX && cx_nb[fd]; }
static void nb_set(int fd, int on) { if (fd >= 0 && fd < CX_NBMAX) cx_nb[fd] = (unsigned char)(on != 0); }
/* modo de acesso de cada fd (O_RDONLY/O_WRONLY/O_RDWR + 1; 0 = não sei = O_RDWR) para o F_GETFL. O Chrome confere
 * (fcntl(fd, F_GETFL) & O_ACCMODE) == O_RDONLY antes de passar um arquivo a outro processo, e caía com O_RDWR fixo. */
static unsigned char cx_acc[CX_NBMAX];
static void acc_set(int fd, int mode) { if (fd >= 0 && fd < CX_NBMAX) cx_acc[fd] = (unsigned char)(mode < 0 ? 0 : (mode & 3) + 1); }
/* sem registro aqui (fd de fileport, herdado...): pergunta ao kernel o modo da abertura (SYS_FDFLAGS com b = 2) */
static int acc_get(int fd) {
    if (fd >= 0 && fd < CX_NBMAX && cx_acc[fd]) return cx_acc[fd] - 1;
    long r = fd > 2 && fd < 0x4000 ? SYS3(SYS_FDFLAGS, fd, 2, 0) : -1;
    return r >= 0 && r <= 2 ? (int)r : 2;
}
ssize_t write(int fd, const void *b, size_t n) {
    if (IS_SOCK(fd)) return cx_sock_write(fd, b, n);
    if (fd == 2 && b && n >= 16) diag_mojo_backtrace(b, n);
    if (nb_on(fd) && n) {
        mos_stat_t st;   /* pipe: is_dir 3 = ponta de escrita, size = espaço livre, mtime = 1 se não há leitor */
        if (SYS2(SYS_FSTAT, fd, &st) == 0 && st.is_dir == 3 && st.size == 0 && !st.mtime) { cx_errno = 35; return -1; }   /* EAGAIN */
    }
    return ret_errno(SYS3(SYS_WRITE, fd, b, n));
}
ssize_t read(int fd, void *b, size_t n) {
    if (IS_SOCK(fd)) return cx_sock_read(fd, b, n);
    if (nb_on(fd) && n) {
        mos_stat_t st;   /* pipe: is_dir 2 = ponta de leitura, size = bytes prontos, mtime = 1 se o escritor fechou */
        if (SYS2(SYS_FSTAT, fd, &st) == 0 && st.is_dir == 2 && st.size == 0 && !st.mtime) { cx_errno = 35; return -1; }   /* EAGAIN */
    }
    return ret_errno(SYS3(SYS_READ, fd, b, n));
}
int close(int fd) {
    /* descritores que não são do kernel (kqueue, sockets: sockets.c / kqueue.c) */
    extern int cx_fake_fd_close(int fd) __attribute__((weak));
    if ((fd >= 0x4000 || IS_SOCK(fd)) && cx_fake_fd_close) return cx_fake_fd_close(fd);
    acc_set(fd, -1);
    nb_set(fd, 0);
    return (int)ret_errno(SYS1(SYS_CLOSE, fd));
}
off_t lseek(int fd, off_t off, int wh) { return ret_errno(SYS3(SYS_LSEEK, fd, off, wh)); }
/* diretório atual: mantido no próprio processo (PWD), usado para caminhos relativos */
static char cx_cwd[1024] = "/";
const char *cx_path(const char *in, char *out, size_t cap) {
    if (!in) return in;
    if (in[0] == '/') { strlcpy(out, in, cap); return out; }
    size_t n = strlcpy(out, cx_cwd, cap);
    if (n && out[n - 1] != '/') strlcat(out, "/", cap);
    while (in[0] == '.' && in[1] == '/') in += 2;
    if (!strcmp(in, ".")) in = "";
    strlcat(out, in, cap);
    return out;
}
#define PATH(var, in) char var##_buf[1024]; const char *var = cx_path(in, var##_buf, sizeof var##_buf)

static int link_step(char *abs, size_t cap, long *tlen);
int open(const char *path, int flags, ...) {
    int k = 0;
    if ((flags & 3) == MAC_O_WRONLY) k |= MOS_O_WRONLY;
    if ((flags & 3) == MAC_O_RDWR) k |= MOS_O_RDWR;
    if (flags & MAC_O_CREAT) k |= MOS_O_CREAT;
    if (flags & MAC_O_TRUNC) k |= MOS_O_TRUNC;
    if (flags & MAC_O_APPEND) k |= MOS_O_APPEND;
    if (flags & 0x1000000) k |= MOS_O_CLOEXEC;            /* O_CLOEXEC do macOS: o execve do kernel fecha */
    if (flags & 0x20000) k |= MOS_O_NOCTTY;               /* O_NOCTTY: abrir um escravo de pty não o adota como terminal */
    if (flags & 0x4) k |= MOS_O_NONBLOCK;                 /* O_NONBLOCK: nos terminais o kernel devolve EAGAIN */
    PATH(p, path);
    if ((flags & MAC_O_EXCL) && (flags & MAC_O_CREAT)) {
        mos_stat_t st;
        if (SYS2(SYS_STAT, p, &st) == 0) { cx_errno = CX_EEXIST; return -1; }
    }
    if (flags & 0x100000) {                               /* O_DIRECTORY: o cp do GNU testa se o destino é pasta assim */
        mos_stat_t ds;
        if (SYS2(SYS_STAT, p, &ds) == 0 && ds.is_dir != 1) {
            char rp[1024]; strlcpy(rp, p, sizeof rp);           /* pasta-link (opt/<f>) conta como pasta */
            for (int hop = 0; hop < 8 && link_step(rp, sizeof rp, 0); hop++) {}
            if (SYS2(SYS_STAT, rp, &ds) != 0 || ds.is_dir != 1) { cx_errno = CX_ENOTDIR; return -1; }
        }
    }
    int fd = (int)ret_errno(SYS2(SYS_OPEN, p, k));
    if (fd >= 0) nb_set(fd, (flags & 0x4) != 0);          /* O_NONBLOCK */
    if (fd >= 0) acc_set(fd, flags & 3);
    return fd;
}
int creat(const char *path, int mode) { return open(path, 0x0601, mode); }
/* o SYS_UNLINK do kernel apaga pastas inteiras; unlink/rmdir seguem o POSIX por cima dele: unlink de pasta é
 * EPERM e rmdir só apaga pasta vazia (o Homebrew poda pastas "vazias" com rmdir; recursivo, apagava o Homebrew).
 * cx_remove_tree é o recursivo explícito (NSFileManager removeItemAtPath:). */
int cx_remove_tree(const char *path) { PATH(p, path); return (int)ret_errno(SYS1(SYS_UNLINK, p)); }
int unlink(const char *path) {
    mos_stat_t st;
    PATH(p, path);
    if (SYS2(SYS_STAT, p, &st) == 0 && st.is_dir == 1) { cx_errno = 1; return -1; }   /* EPERM */
    return (int)ret_errno(SYS1(SYS_UNLINK, p));
}
int rmdir(const char *path);
int remove(const char *path) { mos_stat_t st; PATH(p, path); return SYS2(SYS_STAT, p, &st) == 0 && st.is_dir == 1 ? rmdir(path) : unlink(path); }
void cx_perm_set(const char *path, unsigned mode);
int mkdir(const char *path, int mode) {
    PATH(p, path);
    int r = (int)ret_errno(SYS1(SYS_MKDIR, p));
    if (r == 0) cx_perm_set(p, (unsigned)mode & 0777 & ~022u);   /* umask padrão do macOS */
    return r;
}
int rename(const char *a, const char *b) { PATH(pa, a); PATH(pb, b); return (int)ret_errno(SYS2(SYS_RENAME, pa, pb)); }
int rmdir(const char *path) {
    mos_stat_t st;
    char name[256];
    PATH(p, path);
    if (ret_errno(SYS2(SYS_STAT, p, &st)) < 0) return -1;
    if (st.is_dir != 1) { cx_errno = CX_ENOTDIR; return -1; }
    if (SYS4(SYS_READDIR, p, 0, name, sizeof name) == 1) { cx_errno = 66; return -1; }   /* ENOTEMPTY */
    return (int)ret_errno(SYS1(SYS_UNLINK, p));
}
/* isatty: em pty.c (pseudoterminais do kernel; o console do mOS segue a regra do TERM) */
int isatty(int fd);
int getpid(void) { return (int)SYS0(SYS_GETPID); }
/* getppid: o kernel guarda o pai e o informa em SYS_PROCLIST (índice sobre os processos vivos, sem
 * buracos; 0 = fim). Pai morto (ppid 0/-1) → 1, como no Unix. Resultado guardado por ~1 s e refeito se
 * o pid mudar (fork). Necessário para o rendezvous Mach do Chromium ("...RendezvousServer.<ppid>"). */
int getppid(void) {
    static int cached_pid, cached_ppid;
    static long cached_ms;
    int me = (int)SYS0(SYS_GETPID);
    long now = (long)SYS0(SYS_TIME);
    if (cached_pid == me && now - cached_ms < 1000) return cached_ppid;
    int pp = 1;
    mos_procinfo_t pi;
    for (long i = 0; SYS2(SYS_PROCLIST, i, &pi) == 1; i++)
        if ((int)pi.pid == me) { pp = (int)pi.ppid > 0 ? (int)pi.ppid : 1; break; }
    cached_pid = me; cached_ppid = pp; cached_ms = now;
    return pp;
}
unsigned getuid(void) { return 501; }
unsigned geteuid(void) { return 501; }
unsigned getgid(void) { return 20; }
unsigned getegid(void) { return 20; }
char *getcwd(char *b, size_t n) {
    if (!b) return strdup(cx_cwd);
    if (strlen(cx_cwd) >= n) { cx_errno = CX_ERANGE; return 0; }
    return strcpy(b, cx_cwd);
}
static int link_step(char *abs, size_t cap, long *tlen);
int chdir(const char *path) {
    mos_stat_t st;
    PATH(p, path);
    char rp[1024];                                       /* a pasta pode ser um link (opt/<f>, portable-ruby/current) */
    strlcpy(rp, p, sizeof rp);
    for (int hop = 0; hop < 8 && link_step(rp, sizeof rp, 0); hop++) {}
    if (ret_errno(SYS2(SYS_STAT, rp, &st)) < 0) return -1;
    if (!st.is_dir) { cx_errno = CX_ENOTDIR; return -1; }
    strlcpy(cx_cwd, p, sizeof cx_cwd);
    return 0;
}
int access(const char *path, int mode) { mos_stat_t st; (void)mode; PATH(p, path); return ret_errno(SYS2(SYS_STAT, p, &st)) < 0 ? -1 : 0; }
int fsync(int fd) { (void)fd; return 0; }
/* dup/dup2 de verdade (SYS_DUP2): o bash redireciona com eles. Os fds 0-2 do console não existem
 * na tabela do kernel; duplicá-los continua sendo aceito sem efeito, como antes. */
int dup2(int a, int b) {
    if (a == b) return b;
    long r = SYS2(SYS_DUP2, a, b);
    if (r < 0 && a >= 0 && a <= 2) return b;
    if (r >= 0 && a >= 0 && a < CX_NBMAX && b >= 0 && b < CX_NBMAX) cx_acc[b] = cx_acc[a];   /* o duplicado herda o modo de acesso */
    return (int)ret_errno(r);
}
/* dup/F_DUPFD: o kernel escolhe o número (o menor livre >= min, até os 256 da tabela) e copia na mesma syscall
 * (SYS_DUP2 com MOS_DUP_ANY). Antes o libSystem procurava um número livre e chamava dup2 em seguida: no meio, outra
 * thread podia abrir um arquivo e receber aquele número, e o dup2 o trocava em silêncio. Os dois donos fechavam o mesmo
 * fd, e o processo principal do Chrome caía no CHECK do ScopedFD depois de o fileport_makeport do Mojo achar o fd
 * fechado (base::File::Duplicate usa dup). */
static int cx_dup_min(int fd, long min) {
    mos_stat_t st;
    if (fd >= 0 && fd <= 2 && SYS2(SYS_FSTAT, fd, &st) < 0) return fd;   /* console: igual a antes */
    if (fd >= 0x4000) {                                  /* kqueue do libSystem (kqueue.c): o kernel não o conhece */
        extern int cx_kqueue_dup(int fd);
        int k = cx_kqueue_dup(fd);
        if (k >= 0) return k;
        if (k == -2) return -1;                          /* EMFILE já posto */
    }
    long r = SYS3(SYS_DUP2, fd, MOS_DUP_ANY, min < 3 ? 3 : min);
    if (r >= 0 && fd >= 0 && fd < CX_NBMAX && r < CX_NBMAX) cx_acc[r] = cx_acc[fd];   /* o duplicado herda o modo de acesso */
    return (int)ret_errno(r);
}
int dup(int fd) { return cx_dup_min(fd, 3); }
int pipe(int *fds) {                                                   /* o kernel tem pipes (SYS_PIPE) */
    int r = (int)ret_errno(SYS1(SYS_PIPE, fds));
    if (r == 0) { nb_set(fds[0], 0); nb_set(fds[1], 0); acc_set(fds[0], 0); acc_set(fds[1], 1); }
    return r;
}
/* socketpair (sockets.c): par bidirecional do kernel (SYS_PIPE com MOS_PIPE_SOCKPAIR): cada ponta lê e escreve */
int cx_socketpair_fds(int *fds) {
    int r = (int)ret_errno(SYS2(SYS_PIPE, fds, MOS_PIPE_SOCKPAIR));
    if (r == 0) { nb_set(fds[0], 0); nb_set(fds[1], 0); acc_set(fds[0], 2); acc_set(fds[1], 2); }
    return r;
}
/* pthread_atfork: prepare em LIFO, parent e child em FIFO (POSIX). A trava do heap entra no meio,
 * depois dos prepare do usuário e antes dos parent/child, para o filho não herdar a trava presa
 * por uma thread que não existe mais e para os handlers poderem chamar malloc. */
#define ATFORK_MAX 256
static struct { void (*pre)(void); void (*parent)(void); void (*child)(void); } atfork_tab[ATFORK_MAX];
static int atfork_n;
static volatile int atfork_lock;
static int fork_depth;
static void af_lock(void) {
    while (__sync_lock_test_and_set(&atfork_lock, 1))
        while (atfork_lock) __builtin_ia32_pause();
}
static void af_unlock(void) { __sync_lock_release(&atfork_lock); }
static void atfork_run(int which) {
    void (*fns[ATFORK_MAX])(void);
    af_lock();
    int n = atfork_n;
    for (int i = 0; i < n; i++)
        fns[i] = which == 0 ? atfork_tab[i].pre : which == 1 ? atfork_tab[i].parent : atfork_tab[i].child;
    af_unlock();
    if (which == 0) { for (int i = n - 1; i >= 0; i--) if (fns[i]) fns[i](); }
    else { for (int i = 0; i < n; i++) if (fns[i]) fns[i](); }
}
int pthread_atfork(void (*pre)(void), void (*parent)(void), void (*child)(void)) {
    af_lock();
    if (atfork_n >= ATFORK_MAX) { af_unlock(); return 12; }   /* ENOMEM */
    atfork_tab[atfork_n].pre = pre;
    atfork_tab[atfork_n].parent = parent;
    atfork_tab[atfork_n].child = child;
    atfork_n++;
    af_unlock();
    return 0;
}
int fork(void) {
    int top = fork_depth == 0;
    fork_depth++;
    if (top) { atfork_run(0); hl(); }
    long r = SYS0(SYS_FORK);
    if (r == 0) {
        fork_depth = 0;
        __atomic_store_n(&heap_lock, 0, __ATOMIC_RELEASE);
        if (top) atfork_run(2);
        return 0;
    }
    if (top) { hu(); atfork_run(1); }
    fork_depth--;
    return (int)ret_errno(r);
}
/* execve/execvp ficam em extra.c (precisam do posix_spawnp e do atalho do sandbox-exec) */
int ioctl(int fd, unsigned long req, ...) {
    /* FIONBIO / FIOCLEX / FIONCLEX (libuv): o mOS ainda não distingue E/S não bloqueante nem
     * close-on-exec por descritor; aceitar mantém pipes e sockets utilizáveis */
    if (req == 0x8004667eUL && IS_SOCK(fd)) {              /* FIONBIO num socket: liga/desliga o modo não-bloqueante */
        __builtin_va_list ap; __builtin_va_start(ap, req); int *on = __builtin_va_arg(ap, int *); __builtin_va_end(ap);
        cx_sock_set_nonblock(fd, on && *on);
        return 0;
    }
    if (req == 0x8004667eUL) {                               /* FIONBIO num fd do kernel (pipe): o O_NONBLOCK daqui */
        __builtin_va_list ap; __builtin_va_start(ap, req); int *on = __builtin_va_arg(ap, int *); __builtin_va_end(ap);
        nb_set(fd, on && *on);
        if (fd < 0x4000) SYS3(SYS_TTY, MOS_TTY_SETFL, fd, on && *on);   /* terminais: o kernel decide o EAGAIN */
        return 0;
    }
    if (req == 0x20006601UL || req == 0x20006602UL) {          /* FIOCLEX / FIONCLEX */
        if (!IS_SOCK(fd) && fd < 0x4000) SYS3(SYS_FDFLAGS, fd, 1, req == 0x20006601UL);
        return 0;
    }
    /* o resto é de terminal (TIOCGETA, TIOCSWINSZ, TIOCPTYGNAME, FIONREAD...): pty.c, com o SYS_TTY do kernel */
    extern int cx_tty_ioctl(int fd, unsigned long req, void *arg);
    __builtin_va_list ap; __builtin_va_start(ap, req); void *arg = __builtin_va_arg(ap, void *); __builtin_va_end(ap);
    return cx_tty_ioctl(fd, req, arg);
}
/* fcntl: em sockets, F_GETFL/F_SETFL guardam O_NONBLOCK (0x4) de verdade; no resto, aceita (como antes) */
int fcntl(int fd, int cmd, ...) {
    /* F_DUPFD (0) / F_DUPFD_CLOEXEC (67): como dup() acima, o mesmo descritor. Antes devolvia 0 (= stdin), e o
     * tokio/mio, que clona o kqueue assim (Registry::try_clone), passava a fazer kevent no fd 0 -> EBADF */
    if (cmd == 0 || cmd == 67) {
        __builtin_va_list ap; __builtin_va_start(ap, cmd); long min = __builtin_va_arg(ap, long); __builtin_va_end(ap);
        int nfd = cx_dup_min(fd, min);
        if (cmd == 67 && nfd >= 0 && !IS_SOCK(nfd)) SYS3(SYS_FDFLAGS, nfd, 1, 1);   /* F_DUPFD_CLOEXEC */
        return nfd;
    }
    /* F_GETFD (1) / F_SETFD (2): FD_CLOEXEC no kernel (SYS_FDFLAGS). O git cria um pipe de aviso com
     * FD_CLOEXEC e espera EOF nele para saber que o exec do filho deu certo; ignorado, o pai esperava para sempre. */
    if ((cmd == 1 || cmd == 2) && !IS_SOCK(fd) && fd < 0x4000) {
        long a = 0;
        if (cmd == 2) { __builtin_va_list ap; __builtin_va_start(ap, cmd); a = __builtin_va_arg(ap, long); __builtin_va_end(ap); }
        long r = SYS3(SYS_FDFLAGS, fd, cmd == 2, a & 1);
        if (r < 0) { if (fd <= 2) return 0; cx_errno = (int)-r; return -1; }   /* 0-2 do console: sem entrada no kernel */
        return cmd == 1 ? (int)r : 0;
    }
    if (IS_SOCK(fd) && (cmd == 3 || cmd == 4)) {
        if (cmd == 3) return 0x2 | (cx_sock_get_nonblock(fd) ? 0x4 : 0);   /* O_RDWR [| O_NONBLOCK] */
        __builtin_va_list ap; __builtin_va_start(ap, cmd); long fl = __builtin_va_arg(ap, long); __builtin_va_end(ap);
        cx_sock_set_nonblock(fd, (fl & 0x4) != 0);
        return 0;
    }
    if (cmd == 3) return acc_get(fd) | (nb_on(fd) ? 0x4 : 0);   /* F_GETFL: modo de acesso [| O_NONBLOCK] */
    if (cmd == 4) {                                     /* F_SETFL: só O_NONBLOCK tem efeito */
        __builtin_va_list ap; __builtin_va_start(ap, cmd); long fl = __builtin_va_arg(ap, long); __builtin_va_end(ap);
        nb_set(fd, (fl & 0x4) != 0);
        if (fd >= 0 && fd < 0x4000) SYS3(SYS_TTY, MOS_TTY_SETFL, fd, (fl & 0x4) != 0);   /* terminais: o kernel decide o EAGAIN */
        return 0;
    }
    if (cmd == 50) {                                    /* F_GETPATH: caminho do fd (SYS_FDPATH) */
        __builtin_va_list ap; __builtin_va_start(ap, cmd); char *buf = __builtin_va_arg(ap, char *); __builtin_va_end(ap);
        return ret_errno(SYS3(SYS_FDPATH, fd, buf, 1024)) < 0 ? -1 : 0;
    }
    /* travas POSIX (struct flock do macOS: {off_t l_start, l_len; pid_t l_pid; short l_type, l_whence}). O mOS não tem
     * travas entre processos: F_SETLK/F_SETLKW (8/9) sempre conseguem e F_GETLK (7) responde "ninguém trava" (l_type =
     * F_UNLCK). Antes F_GETLK devolvia 0 sem preencher a struct: o SQLite lia lixo como trava de outro processo e o
     * commit falhava ("Unable to commit a transaction" no perfil do Chrome). */
    if (cmd == 7) {
        __builtin_va_list ap; __builtin_va_start(ap, cmd); u8 *fl = __builtin_va_arg(ap, u8 *); __builtin_va_end(ap);
        if (fl) *(short *)(fl + 20) = 2;                /* l_type = F_UNLCK */
        return 0;
    }
    return 0;
}
long sysconf(int name) {
    switch (name) {
    case 29: { extern u64 vm_page_size; return (long)vm_page_size; }   /* _SC_PAGESIZE (16 KiB no arm64) */
    case 57: case 58: return 1; /* _SC_NPROCESSORS_* */
    }
    return -1;
}
int getpagesize(void) { extern u64 vm_page_size; return (int)vm_page_size; }
/* sinais sobre SYS_SIGACTION/SYS_SIGKILL/SYS_SIGRETURN. O kernel numera como o Linux (USR1 10, USR2 12,
 * CHLD 17); o macOS usa BUS 10, SYS 12, STOP 17, CHLD 20, USR1 30, USR2 31. As trocas são uma bijeção. */
static int sig_to_mos(int s) {
    switch (s) { case 10: return 30; case 12: return 31; case 30: return 10; case 31: return 12; case 20: return 17; case 17: return 20; }
    return s;
}
static int sig_from_mos(int s) { return sig_to_mos(s); }   /* a bijeção é a própria inversa */
int cx_sig_to_mos(int s) { return sig_to_mos(s); }          /* para o resto do libSystem (waitpid, pty.c) */
int cx_sig_from_mos(int s) { return sig_from_mos(s); }
/* sigset_t do macOS: bit (s - 1) do número do macOS; o kernel usa bit (s) do número dele */
u32 cx_sigmask_to_mos(u32 m) {
    u32 r = 0;
    for (int s = 1; s < 32; s++) if (m & (1u << (s - 1))) r |= 1u << sig_to_mos(s);
    return r;
}
u32 cx_sigmask_from_mos(u32 m) {
    u32 r = 0;
    for (int s = 1; s < 32; s++) if (m & (1u << s)) r |= 1u << (sig_from_mos(s) - 1);
    return r;
}
/* Sinais de verdade (kernel/proc/process.c, SYS_SIG): o kernel guarda tratador, sa_mask e sa_flags; na entrega
 * monta siginfo/ucontext/mcontext do Darwin na pilha e chama cx_sigtramp, como o _sigtramp do macOS. O trampolim
 * traduz o número e a máscara para os do macOS, chama o tratador e volta pelo MOS_SIGOP_RETURN, que restaura o
 * contexto (inclusive o que o tratador tiver mudado no mcontext, ex.: __ss.__rip). */
typedef struct { void *handler; u32 mask; int flags; } mac_sigaction_t;
static __attribute__((used, noreturn)) void cx_sigtramp(void *h, int style, int mossig, mos_siginfo_t *si, mos_ucontext_t *uc) {
    (void)style;
    int s = sig_from_mos(mossig);
    int saved_errno = cx_errno;
    si->si_signo = s;
    if (s == 20 && si->si_code == MOS_CLD_KILLED) si->si_status = sig_from_mos(si->si_status);   /* SIGCHLD de filho morto por sinal */
    uc->uc_sigmask = cx_sigmask_from_mos(uc->uc_sigmask);
    ((void (*)(int, void *, void *))h)(s, si, uc);     /* o macOS passa (sinal, siginfo, ucontext) nos dois estilos */
    uc->uc_sigmask = cx_sigmask_to_mos(uc->uc_sigmask);
    cx_errno = saved_errno;
    SYS2(SYS_SIG, MOS_SIGOP_RETURN, uc);
    for (;;) __builtin_trap();
}
int sigaction(int s, const mac_sigaction_t *a, mac_sigaction_t *o) {
    if (s <= 0 || s >= 32 || ((s == 9 || s == 17) && a)) { cx_errno = CX_EINVAL; return -1; }   /* KILL e STOP fixos */
    mos_sigaction_t n, old;
    memset(&old, 0, sizeof old);
    if (a) {
        n.handler = (u64)a->handler;
        n.tramp = (u64)cx_sigtramp;
        n.mask = cx_sigmask_to_mos(a->mask);
        n.flags = (u32)a->flags;
    }
    long r = SYS4(SYS_SIG, MOS_SIGOP_ACTION, sig_to_mos(s), a ? &n : 0, &old);
    if (r < 0) { cx_errno = (int)-r; return -1; }
    if (o) { o->handler = (void *)old.handler; o->mask = cx_sigmask_from_mos(old.mask); o->flags = (int)old.flags; }
    return 0;
}
/* signal() do BSD/macOS: a syscall interrompida é refeita (SA_RESTART), salvo depois de siginterrupt(s, 1) */
static u32 sig_interrupt_set;                               /* bits (s - 1), números do macOS */
void *signal(int s, void *h) {
    mac_sigaction_t a = {h, 0, (sig_interrupt_set & (1u << (s - 1))) ? 0 : MOS_SA_RESTART}, o = {0, 0, 0};
    if (sigaction(s, &a, &o) < 0) return (void *)-1;     /* SIG_ERR */
    return o.handler;
}
int siginterrupt(int s, int flag) {
    mac_sigaction_t a;
    if (sigaction(s, 0, &a) < 0) return -1;
    if (flag) { sig_interrupt_set |= 1u << (s - 1); a.flags &= ~MOS_SA_RESTART; }
    else { sig_interrupt_set &= ~(1u << (s - 1)); a.flags |= MOS_SA_RESTART; }
    return sigaction(s, &a, 0);
}
int kill(int pid, int s) {
    extern int cx_kill_group(int pg, int sig);             /* pty.c: grupos de processos do kernel (SYS_TTY) */
    if (pid == 0) return cx_kill_group(0, s);              /* o grupo de quem chama (^C do bash, kill 0) */
    if (pid < -1) return cx_kill_group(-pid, s);           /* kill(-pgrp): o grupo inteiro */
    if (pid == -1) pid = getpid();                         /* "todos os processos": no mOS, só o próprio */
    long r = SYS2(SYS_SIGKILL, pid, s ? sig_to_mos(s) : 0);
    if (r < 0) { cx_errno = CX_ESRCH; return -1; }
    return 0;
}
/* raise = pthread_kill(pthread_self()): o sinal vai para a thread que chamou e sai na volta da syscall */
int raise(int s) {
    if (s <= 0 || s >= 32) { cx_errno = CX_EINVAL; return -1; }
    long r = SYS3(SYS_SIG, MOS_SIGOP_TKILL, SYS0(SYS_GETTID), sig_to_mos(s));
    if (r < 0) return kill(getpid(), s);
    return 0;
}
int cx_pthread_kill_tid(long tid, int s) {                 /* vscode_libc.c: pthread_kill */
    if (s < 0 || s >= 32) return CX_EINVAL;
    long r = SYS3(SYS_SIG, MOS_SIGOP_TKILL, tid, s ? sig_to_mos(s) : 0);
    return r < 0 ? (int)-r : 0;
}
int sigemptyset(void *set) { *(u32 *)set = 0; return 0; }
int sigaddset(void *set, int s) {
    if (s < 1 || s > 31) { cx_errno = CX_EINVAL; return -1; }
    *(u32 *)set |= 1u << (s - 1);
    return 0;
}
/* máscara da thread (o kernel não tem máscara por processo: sigprocmask = pthread_sigmask, como no macOS) */
int pthread_sigmask(int how, const void *s, void *o) {
    u32 set = s ? cx_sigmask_to_mos(*(const u32 *)s) : 0, old = 0;
    long r = SYS4(SYS_SIG, MOS_SIGOP_MASK, how, s ? &set : 0, &old);
    if (r < 0) return (int)-r;
    if (o) *(u32 *)o = cx_sigmask_from_mos(old);
    return 0;
}
int sigprocmask(int how, const void *s, void *o) {
    int e = pthread_sigmask(how, s, o);
    if (e) { cx_errno = e; return -1; }
    return 0;
}
int sigpending(void *set) {
    u32 k = 0;
    long r = SYS2(SYS_SIG, MOS_SIGOP_PENDING, &k);
    if (r < 0) { cx_errno = (int)-r; return -1; }
    *(u32 *)set = cx_sigmask_from_mos(k);
    return 0;
}
/* sigsuspend: troca a máscara e dorme até um tratador rodar; sempre -1/EINTR */
int sigsuspend(const unsigned *m) {
    long r = SYS2(SYS_SIG, MOS_SIGOP_SUSPEND, cx_sigmask_to_mos(*m));
    cx_errno = r < 0 ? (int)-r : MOS_EINTR;
    return -1;
}
int pause(void) {
    u32 cur = 0;
    pthread_sigmask(MOS_SIG_BLOCK, 0, &cur);
    return sigsuspend(&cur);
}
int sigwait(const unsigned *set, int *sig) {
    for (;;) {
        long r = SYS4(SYS_SIG, MOS_SIGOP_WAIT, cx_sigmask_to_mos(*set), 0, ~0UL);
        if (r == -MOS_EINTR) continue;                     /* um tratador rodou no meio: o macOS também volta a esperar */
        if (r < 0) return (int)-r;
        if (sig) *sig = sig_from_mos((int)r);
        return 0;
    }
}
/* stack_t do macOS = mos_sigstack_t (ss_sp, ss_size, ss_flags) */
int sigaltstack(const void *ss, void *old) {
    long r = SYS3(SYS_SIG, MOS_SIGOP_ALTSTACK, ss, old);
    if (r < 0) { cx_errno = (int)-r; return -1; }
    return 0;
}
int setlocale_dummy;
char *setlocale(int cat, const char *loc) { (void)cat; (void)loc; return "C"; }

/* stat do macOS ($INODE64): 144 bytes */
typedef struct {
    u32 st_dev; u16 st_mode, st_nlink; u64 st_ino; u32 st_uid, st_gid, st_rdev;
    i64 atime, atime_ns, mtime, mtime_ns, ctime, ctime_ns, btime, btime_ns;
    i64 st_size, st_blocks; i32 st_blksize; u32 st_flags, st_gen; i32 lspare; i64 qspare[2];
} mac_stat_t;

static void to_mac_stat(const mos_stat_t *s, mac_stat_t *m) {
    memset(m, 0, sizeof *m);
    /* is_dir: 0 arquivo, 1 pasta, 2/3 pipe (ponta de leitura/escrita), 4 dispositivo (/dev/null), 5 terminal (pty) */
    m->st_mode = s->is_dir == 5 ? 0020620 : s->is_dir == 4 ? 0020666 : s->is_dir >= 2 ? 0010600 : s->is_dir ? 0040755 : 0100644;
    m->st_dev = 0x1000002;      /* um só volume; o st_ino (hash do caminho) separa os arquivos */
    m->st_nlink = 1;
    m->st_uid = 501;
    m->st_gid = 20;
    m->atime = m->mtime = m->ctime = m->btime = s->mtime;
    m->st_size = (i64)s->size;
    m->st_blocks = (i64)((s->size + 511) / 512);
    m->st_blksize = 4096;
}

/* links simbólicos são arquivos que começam com "#mOS-link:" (symlink em sockets.c; o execve do
 * kernel também os segue). Executáveis (Mach-O, ELF, "#!") aparecem com 0755: o bash procura
 * comandos no PATH pelo bit de execução. */
#define LINK_MAGIC "#mOS-link:"
static long peek_file(const char *abs, char *buf, long n) {
    /* dispositivos não são links nem executáveis, e abrir já tem efeito: /dev/ptmx cria um par de terminais e ler
     * um escravo de pty espera digitação (o stat de /dev/ttysNNN travava assim) */
    if (abs[0] == '/' && abs[1] == 'd' && abs[2] == 'e' && abs[3] == 'v' && abs[4] == '/') return -1;
    long fd = SYS2(SYS_OPEN, abs, 0);
    if (fd < 0) return -1;
    long r = SYS3(SYS_READ, fd, buf, n);
    SYS1(SYS_CLOSE, fd);
    return r;
}
/* se abs é um link, troca abs pelo alvo (relativo à pasta do link) e devolve 1 */
static int link_step(char *abs, size_t cap, long *tlen) {
    char buf[1100];
    long n = peek_file(abs, buf, (long)sizeof buf - 1);
    long ml = (long)sizeof LINK_MAGIC - 1;
    if (n <= ml || memcmp(buf, LINK_MAGIC, (size_t)ml)) return 0;
    buf[n] = 0;
    char *t = buf + ml;
    if (tlen) *tlen = n - ml;
    char next[1024];
    if (t[0] == '/') strlcpy(next, t, sizeof next);
    else {
        strlcpy(next, abs, sizeof next);
        char *sl = strrchr(next, '/');
        if (sl) sl[1] = 0;
        strlcat(next, t, sizeof next);
    }
    strlcpy(abs, next, cap);
    return 1;
}
static void exec_bits(const char *abs, mac_stat_t *m) {
    if ((m->st_mode & 0170000) != 0100000 || m->st_size < 4) return;
    unsigned char h[4];
    if (peek_file(abs, (char *)h, 4) != 4) return;
    u32 mg = (u32)h[0] | (u32)h[1] << 8 | (u32)h[2] << 16 | (u32)h[3] << 24;
    if (mg == 0xFEEDFACFu || mg == 0xBEBAFECAu || mg == 0xCAFEBABEu || mg == 0x464C457Fu || (h[0] == '#' && h[1] == '!'))
        m->st_mode = 0100755;
}
/* st_ino: o VFS não tem números de inode; um hash do caminho absoluto separa os arquivos (o grep, o tar e o
 * cp comparam dev+ino para não ler a própria saída). Bit 63 livre para pipes e o console. */
static u64 path_ino(const char *abs) {
    /* o hash é do caminho normalizado ("a/b/.." = "a", "./" e barras repetidas somem): o fts do coreutils volta
     * das subpastas por openat(fd, "..") e confere dev/ino com o da pasta-mãe; sem isso, chmod -R dava ENOENT */
    char n[1024];
    usize o = 0;
    for (const char *s = abs; *s;) {
        while (*s == '/') s++;
        if (!*s) break;
        const char *e = s;
        while (*e && *e != '/') e++;
        usize k = (usize)(e - s);
        if (k == 1 && s[0] == '.') {}
        else if (k == 2 && s[0] == '.' && s[1] == '.') { while (o > 0 && n[o - 1] != '/') o--; if (o > 0) o--; }
        else if (o + k + 2 < sizeof n) { n[o++] = '/'; memcpy(n + o, s, k); o += k; }
        s = e;
    }
    if (!o) n[o++] = '/';
    n[o] = 0;
    u64 h = 1469598103934665603ULL;
    for (const unsigned char *p = (const unsigned char *)n; *p; p++) h = (h ^ *p) * 1099511628211ULL;
    h &= 0x7FFFFFFFFFFFFFFFULL;
    return h ? h : 1;
}
/* permissões: o FAT não as guarda. O processo lembra o que pediu em mkdir/chmod/fchmodat (com a umask 022 do
 * macOS) e o stat devolve isso: quem cria uma pasta 0700 (mkdtemp) lê 0700 de volta — o Chrome confere
 * (CHECK) que a pasta do socket do perfil é 0700. Vale dentro do processo e nos filhos de fork. */
#define MAX_PERMS 128
static struct { u64 ino; u16 perm; } perms[MAX_PERMS];
static int nperms;
void cx_perm_set(const char *path, unsigned mode) {
    char abs[1024];
    cx_path(path, abs, sizeof abs);
    u64 ino = path_ino(abs);
    int i = 0;
    while (i < nperms && perms[i].ino != ino) i++;
    if (i == nperms) { if (nperms == MAX_PERMS) i = (int)(ino % MAX_PERMS); else nperms++; }
    perms[i].ino = ino;
    perms[i].perm = (u16)(mode & 07777);
}
static void apply_perm(mac_stat_t *m) {
    for (int i = 0; i < nperms; i++)
        if (perms[i].ino == m->st_ino) { m->st_mode = (u16)((m->st_mode & ~07777) | perms[i].perm); return; }
}

int stat(const char *path, mac_stat_t *m) {
    mos_stat_t s;
    char abs[1024];
    cx_path(path, abs, sizeof abs);
    for (int hop = 0; hop < 8 && link_step(abs, sizeof abs, 0); hop++) {}
    if (ret_errno(SYS2(SYS_STAT, abs, &s)) < 0) return -1;
    to_mac_stat(&s, m);
    m->st_ino = path_ino(abs);
    exec_bits(abs, m);
    apply_perm(m);
    return 0;
}
int lstat(const char *path, mac_stat_t *m) {
    mos_stat_t s;
    char abs[1024], tgt[1024];
    cx_path(path, abs, sizeof abs);
    if (ret_errno(SYS2(SYS_STAT, abs, &s)) < 0) return -1;
    to_mac_stat(&s, m);
    m->st_ino = path_ino(abs);
    long tl = 0;
    strlcpy(tgt, abs, sizeof tgt);
    if (!s.is_dir && link_step(tgt, sizeof tgt, &tl)) {
        m->st_mode = 0120755;
        m->st_size = tl;
        m->st_blocks = 0;
        return 0;
    }
    exec_bits(abs, m);
    apply_perm(m);
    return 0;
}
int fstat(int fd, mac_stat_t *m) {
    mos_stat_t s;
    long r = SYS2(SYS_FSTAT, fd, &s);
    if (r < 0 && fd >= 0 && fd <= 2) {               /* 0-2 sem redirecionar: o console (dispositivo de caracteres) */
        memset(m, 0, sizeof *m);
        m->st_mode = 0020620;
        m->st_dev = 0x1000002;
        m->st_ino = 0x8000000000000000ULL | 0x100 | (u64)fd;
        m->st_nlink = 1;
        return 0;
    }
    if (ret_errno(r) < 0) return -1;
    to_mac_stat(&s, m);
    char p[1024];
    if (s.is_dir == 2 || s.is_dir == 3) m->st_ino = 0x8000000000000000ULL | ((u64)getpid() << 8) | (u64)fd;   /* pipe */
    else if (SYS3(SYS_FDPATH, fd, p, sizeof p) >= 0) { m->st_ino = path_ino(p); exec_bits(p, m); }
    return 0;
}

/* diretórios: DIR com entradas do macOS (struct dirent $INODE64: 1048 bytes) */
typedef struct {
    u64 d_ino, d_seekoff; u16 d_reclen, d_namlen; u8 d_type; char d_name[1024];
} mac_dirent_t;
typedef struct { char path[1024]; long index; mac_dirent_t ent; int fd1; } cx_dir_t;   /* fd1 = fd + 1 (0: ainda sem fd) */

cx_dir_t *opendir(const char *path) {
    mos_stat_t st;
    PATH(p, path);
    char rp[1024];                                       /* segue o link da última parte (pasta-link do Homebrew) */
    strlcpy(rp, p, sizeof rp);
    for (int hop = 0; hop < 8 && link_step(rp, sizeof rp, 0); hop++) {}
    p = rp;
    if (ret_errno(SYS2(SYS_STAT, p, &st)) < 0) return 0;
    if (!st.is_dir) { cx_errno = CX_ENOTDIR; return 0; }
    cx_dir_t *d = calloc(1, sizeof *d);
    if (d) strlcpy(d->path, p, sizeof d->path);
    return d;
}
mac_dirent_t *readdir(cx_dir_t *d) {
    mos_stat_t st = {0};
    if (SYS4(SYS_READDIR, d->path, d->index, d->ent.d_name, sizeof d->ent.d_name) != 1) return 0;
    char full[2048];
    strlcpy(full, d->path, sizeof full);
    if (full[strlen(full) - 1] != '/') strlcat(full, "/", sizeof full);
    strlcat(full, d->ent.d_name, sizeof full);
    SYS2(SYS_STAT, full, &st);
    d->index++;
    d->ent.d_ino = (u64)d->index;
    d->ent.d_seekoff = (u64)d->index;
    d->ent.d_namlen = (u16)strlen(d->ent.d_name);
    d->ent.d_reclen = sizeof d->ent;
    d->ent.d_type = st.is_dir ? 4 : 8;
    return &d->ent;
}
int closedir(cx_dir_t *d) { if (d->fd1) SYS1(SYS_CLOSE, d->fd1 - 1); free(d); return 0; }
/* dirfd: abre a pasta na primeira vez (o kernel aceita open de pasta só para leitura) */
int dirfd(cx_dir_t *d) {
    if (!d->fd1) { long fd = SYS2(SYS_OPEN, d->path, 0); if (fd < 0) { cx_errno = (int)-fd; return -1; } d->fd1 = (int)fd + 1; }
    return d->fd1 - 1;
}
/* fdopendir: o caminho do fd vem do kernel (SYS_FDPATH); o DIR passa a ser dono do fd */
cx_dir_t *fdopendir(int fd) {
    char p[1024];
    if (ret_errno(SYS3(SYS_FDPATH, fd, p, sizeof p)) < 0) return 0;
    cx_dir_t *d = opendir(p);
    if (d) d->fd1 = fd + 1;
    return d;
}
long telldir(cx_dir_t *d) { return d->index; }
void seekdir(cx_dir_t *d, long pos) { d->index = pos; }
void rewinddir(cx_dir_t *d) { d->index = 0; }

/* ================================================================ tempo */
typedef struct { long tv_sec; int tv_usec; } mac_timeval_t;
typedef struct { long tv_sec; long tv_nsec; } mac_timespec_t;

long time(long *t) { long v = SYS0(SYS_UNIXTIME); if (t) *t = v; return v; }
int gettimeofday(mac_timeval_t *tv, void *tz) {
    (void)tz;
    u64 ms = (u64)SYS0(SYS_TIME);
    if (tv) { tv->tv_sec = SYS0(SYS_UNIXTIME); tv->tv_usec = (int)(ms % 1000) * 1000; }
    return 0;
}
int clock_gettime(int clk, mac_timespec_t *ts) {
    u64 ms = (u64)SYS0(SYS_TIME);
    if (clk == 0) { ts->tv_sec = SYS0(SYS_UNIXTIME); ts->tv_nsec = (long)(ms % 1000) * 1000000; }
    else { ts->tv_sec = (long)(ms / 1000); ts->tv_nsec = (long)(ms % 1000) * 1000000; }
    return 0;
}
u64 clock_gettime_nsec_np(int clk) { mac_timespec_t t; clock_gettime(clk, &t); return (u64)t.tv_sec * 1000000000ull + (u64)t.tv_nsec; }
unsigned long clock(void) { return (unsigned long)SYS0(SYS_TIME) * 1000; }   /* CLOCKS_PER_SEC = 10^6 */
u64 mach_absolute_time(void) { return (u64)SYS0(SYS_TIME) * 1000000ull; }
u64 mach_continuous_time(void) { return mach_absolute_time(); }
int mach_timebase_info(u32 *info) { info[0] = 1; info[1] = 1; return 0; }
unsigned sleep(unsigned s) { SYS1(SYS_SLEEP, (long)s * 1000); return 0; }
int usleep(unsigned us) { SYS1(SYS_SLEEP, (us + 999) / 1000); return 0; }
int nanosleep(const mac_timespec_t *r, mac_timespec_t *rem) {
    SYS1(SYS_SLEEP, r->tv_sec * 1000 + (r->tv_nsec + 999999) / 1000000);
    if (rem) { rem->tv_sec = 0; rem->tv_nsec = 0; }
    return 0;
}

/* struct tm do macOS: 9 ints + long tm_gmtoff + char *tm_zone */
typedef struct { int sec, min, hour, mday, mon, year, wday, yday, isdst; long gmtoff; const char *zone; } mac_tm_t;
static mac_tm_t tm_buf;

mac_tm_t *gmtime_r(const long *t, mac_tm_t *tm) {
    long s = *t, days = s / 86400, rem = s % 86400;
    if (rem < 0) { rem += 86400; days--; }
    tm->hour = (int)(rem / 3600); tm->min = (int)(rem % 3600 / 60); tm->sec = (int)(rem % 60);
    tm->wday = (int)((days + 4) % 7); if (tm->wday < 0) tm->wday += 7;
    long y = 1970;
    for (;;) {
        int leap = (y % 4 == 0 && y % 100) || y % 400 == 0;
        long n = leap ? 366 : 365;
        if (days < n) break;
        days -= n; y++;
    }
    tm->year = (int)(y - 1900); tm->yday = (int)days;
    static const int md[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int leap = (y % 4 == 0 && y % 100) || y % 400 == 0, m = 0;
    while (m < 12) { int n = md[m] + (m == 1 && leap); if (days < n) break; days -= n; m++; }
    tm->mon = m; tm->mday = (int)days + 1; tm->isdst = 0; tm->gmtoff = 0; tm->zone = "UTC";
    return tm;
}
mac_tm_t *gmtime(const long *t) { return gmtime_r(t, &tm_buf); }
mac_tm_t *localtime_r(const long *t, mac_tm_t *tm) { return gmtime_r(t, tm); }
mac_tm_t *localtime(const long *t) { return gmtime_r(t, &tm_buf); }
long mktime(mac_tm_t *tm) {
    long y = tm->year + 1900, days = 0;
    for (long i = 1970; i < y; i++) days += ((i % 4 == 0 && i % 100) || i % 400 == 0) ? 366 : 365;
    static const int md[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    for (int m = 0; m < tm->mon; m++) days += md[m] + (m == 1 && ((y % 4 == 0 && y % 100) || y % 400 == 0));
    return ((days + tm->mday - 1) * 24 + tm->hour) * 3600L + tm->min * 60L + tm->sec;
}
long timegm(mac_tm_t *tm) { return mktime(tm); }
double difftime(long a, long b) { return (double)(a - b); }

size_t strftime(char *s, size_t max, const char *fmt, const mac_tm_t *tm) {
    static const char *wd[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char *mn[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    size_t o = 0;
    char tmp[64];
    for (; *fmt && o + 1 < max; fmt++) {
        if (*fmt != '%') { s[o++] = *fmt; continue; }
        fmt++;
        tmp[0] = 0;
        switch (*fmt) {
        case 'Y': cx_snprintf(tmp, sizeof tmp, "%d", tm->year + 1900); break;
        case 'y': cx_snprintf(tmp, sizeof tmp, "%02d", tm->year % 100); break;
        case 'm': cx_snprintf(tmp, sizeof tmp, "%02d", tm->mon + 1); break;
        case 'd': cx_snprintf(tmp, sizeof tmp, "%02d", tm->mday); break;
        case 'e': cx_snprintf(tmp, sizeof tmp, "%2d", tm->mday); break;
        case 'H': cx_snprintf(tmp, sizeof tmp, "%02d", tm->hour); break;
        case 'I': cx_snprintf(tmp, sizeof tmp, "%02d", tm->hour % 12 ? tm->hour % 12 : 12); break;
        case 'M': cx_snprintf(tmp, sizeof tmp, "%02d", tm->min); break;
        case 'S': cx_snprintf(tmp, sizeof tmp, "%02d", tm->sec); break;
        case 'p': strcpy(tmp, tm->hour < 12 ? "AM" : "PM"); break;
        case 'a': strcpy(tmp, wd[tm->wday % 7]); break;
        case 'b': case 'h': strcpy(tmp, mn[tm->mon % 12]); break;
        case 'j': cx_snprintf(tmp, sizeof tmp, "%03d", tm->yday + 1); break;
        case 'Z': strcpy(tmp, "UTC"); break;
        case 'z': strcpy(tmp, "+0000"); break;
        case 'F': cx_snprintf(tmp, sizeof tmp, "%d-%02d-%02d", tm->year + 1900, tm->mon + 1, tm->mday); break;
        case 'T': cx_snprintf(tmp, sizeof tmp, "%02d:%02d:%02d", tm->hour, tm->min, tm->sec); break;
        case 'c': cx_snprintf(tmp, sizeof tmp, "%s %s %2d %02d:%02d:%02d %d", wd[tm->wday % 7], mn[tm->mon % 12], tm->mday, tm->hour, tm->min, tm->sec, tm->year + 1900); break;
        case '%': strcpy(tmp, "%"); break;
        default: tmp[0] = '%'; tmp[1] = *fmt; tmp[2] = 0;
        }
        for (char *t = tmp; *t && o + 1 < max; t++) s[o++] = *t;
    }
    if (max) s[o] = 0;
    return o;
}
char *asctime(const mac_tm_t *tm) { static char b[32]; strftime(b, sizeof b, "%c\n", tm); return b; }
char *ctime(const long *t) { return asctime(localtime(t)); }
void tzset(void) {}
char *tzname[2] = {"UTC", "UTC"};

/* ================================================================ matemática (x87/SSE) */
double fabs(double x) { return __builtin_fabs(x); }
float fabsf(float x) { return __builtin_fabsf(x); }
#if defined(__aarch64__)
/* arm64: instruções diretas (fsqrt, frintm/p/z/x); sin/cos/tan/fmod/log2/exp2 vêm do compiler-rt do zig e atan2 de
 * libm_arm64.c (tools/build-libsystem-arm64.ps1) */
double sqrt(double x) { return __builtin_sqrt(x); }
float sqrtf(float x) { return __builtin_sqrtf(x); }
double floor(double x) { return __builtin_floor(x); }
double ceil(double x) { return __builtin_ceil(x); }
double trunc(double x) { return __builtin_trunc(x); }
double sin(double), cos(double), tan(double), fmod(double, double), log2(double), exp2(double), atan2(double, double);
#else
double sqrt(double x) { double r; __asm__("sqrtsd %1, %0" : "=x"(r) : "x"(x)); return r; }
float sqrtf(float x) { float r; __asm__("sqrtss %1, %0" : "=x"(r) : "x"(x)); return r; }
double floor(double x) { double r; __asm__("roundsd $9, %1, %0" : "=x"(r) : "x"(x)); return r; }
double ceil(double x) { double r; __asm__("roundsd $10, %1, %0" : "=x"(r) : "x"(x)); return r; }
double trunc(double x) { double r; __asm__("roundsd $11, %1, %0" : "=x"(r) : "x"(x)); return r; }
#endif
double round(double x) { return x < 0 ? -floor(-x + 0.5) : floor(x + 0.5); }
long lround(double x) { return (long)round(x); }
#if defined(__aarch64__)
double rint(double x) { return __builtin_rint(x); }
#else
double rint(double x) { double r; __asm__("roundsd $4, %1, %0" : "=x"(r) : "x"(x)); return r; }
#endif
double nearbyint(double x) { return rint(x); }
float floorf(float x) { return (float)floor(x); }
float ceilf(float x) { return (float)ceil(x); }
float roundf(float x) { return (float)round(x); }
#if !defined(__aarch64__)
double fmod(double a, double b) { double r; __asm__("1: fprem; fnstsw %%ax; sahf; jp 1b" : "=t"(r) : "0"(a), "u"(b) : "ax", "cc"); return r; }
double sin(double x) { double r; __asm__("fsin" : "=t"(r) : "0"(x)); return r; }
double cos(double x) { double r; __asm__("fcos" : "=t"(r) : "0"(x)); return r; }
double tan(double x) { double r, one; __asm__("fptan" : "=t"(one), "=u"(r) : "0"(x)); return r; }
double atan2(double y, double x) { double r; __asm__("fpatan" : "=t"(r) : "0"(x), "u"(y) : "st(1)"); return r; }
#endif
double atan(double x) { return atan2(x, 1.0); }
double asin(double x) { return atan2(x, sqrt(1 - x * x)); }
double acos(double x) { return atan2(sqrt(1 - x * x), x); }
#if !defined(__aarch64__)
double log2(double x) { double r; __asm__("fld1; fxch; fyl2x" : "=t"(r) : "0"(x)); return r; }
#endif
double log(double x) { return log2(x) * 0.69314718055994530942; }
double log10(double x) { return log2(x) * 0.30102999566398119521; }
double log1p(double x) { return log(1 + x); }
/* 2^x = 2^n * 2^f, n = rint(x), f em [-0.5, 0.5] (domínio do f2xm1). A versão antiga, toda em x87, fazia
 * "fsub %st, %st(1)", que na sintaxe AT&T dá n - x: a fração saía com o sinal trocado (exp2(-0.5) = 1.414,
 * exp2(0.7) = 2.46) e com ela exp/pow/powf de todo app; o skcms do Chrome caía no CHECK de cor finita. */
#if !defined(__aarch64__)
double exp2(double x) {
    if (x != x) return x;
    if (x >= 1024) return __builtin_inf();
    if (x < -1100) return 0;
    double n = rint(x), f = x - n, r;
    __asm__("f2xm1; fld1; faddp" : "=t"(r) : "0"(f));
    __asm__("fscale" : "=t"(r) : "0"(r), "u"(n));
    return r;
}
#endif
double exp(double x) { return exp2(x * 1.44269504088896340736); }
double expm1(double x) { return exp(x) - 1; }
double pow(double b, double e) {
    if (e == 0) return 1;
    if (b == 0) return e > 0 ? 0 : __builtin_inf();
    if (e == (double)(long)e && e > -1e9 && e < 1e9) {
        long n = (long)e;
        int neg = n < 0;
        if (neg) n = -n;
        double r = 1, x = b;
        while (n) { if (n & 1) r *= x; x *= x; n >>= 1; }
        return neg ? 1 / r : r;
    }
    if (b < 0) return __builtin_nan("");
    return exp2(e * log2(b));
}
float powf(float b, float e) { return (float)pow(b, e); }
float sinf(float x) { return (float)sin(x); }
float cosf(float x) { return (float)cos(x); }
float expf(float x) { return (float)exp(x); }
float logf(float x) { return (float)log(x); }
double sinh(double x) { return (exp(x) - exp(-x)) / 2; }
double cosh(double x) { return (exp(x) + exp(-x)) / 2; }
double tanh(double x) { double e = exp(2 * x); return (e - 1) / (e + 1); }
double hypot(double a, double b) { return sqrt(a * a + b * b); }
double cbrt(double x) { return x < 0 ? -pow(-x, 1.0 / 3) : pow(x, 1.0 / 3); }
double ldexp(double x, int e) { return x * exp2(e); }
double frexp(double x, int *e) {
    if (x == 0) { *e = 0; return 0; }
    u64 b; memcpy(&b, &x, 8);
    int ex = (int)((b >> 52) & 0x7FF);
    if (!ex) { x *= 18446744073709551616.0; memcpy(&b, &x, 8); ex = (int)((b >> 52) & 0x7FF) - 64; }
    *e = ex - 1022;
    b = (b & ~(0x7FFull << 52)) | (1022ull << 52);
    memcpy(&x, &b, 8);
    return x;
}
double modf(double x, double *ip) { double t = trunc(x); *ip = t; return x - t; }
double fmax(double a, double b) { return a > b ? a : b; }
double fmin(double a, double b) { return a < b ? a : b; }
int __isnand(double x) { return x != x; }
int __isinfd(double x) { return x == __builtin_inf() || x == -__builtin_inf(); }
int __isfinited(double x) { return !__isnand(x) && !__isinfd(x); }
int __signbitd(double x) { u64 b; memcpy(&b, &x, 8); return (int)(b >> 63); }
int __fpclassifyd(double x) {
    if (x != x) return 1;                       /* FP_NAN */
    if (__isinfd(x)) return 2;                  /* FP_INFINITE */
    if (x == 0) return 3;                       /* FP_ZERO */
    return fabs(x) < 2.2250738585072014e-308 ? 5 : 4; /* FP_SUBNORMAL : FP_NORMAL */
}

/* ================================================================ inicialização */
void cx_libc_init(void) {
    ctype_init();
    const char *pwd = getenv("PWD");
    if (pwd && pwd[0] == '/') strlcpy(cx_cwd, pwd, sizeof cx_cwd);
}
