/*
 * mOS libSystem — busca e ordenação da libc da Apple que faltavam: bsearch_b, heapsort(_b), mergesort(_b) e
 * psort(_b/_r) e qsort_r/qsort_b. As variantes _b recebem um bloco: invoke(bloco, a, b)
 * (o WhatsApp arm64 parava em _bsearch_b).
 * heapsort não usa memória extra; qsort_r e qsort_b são esse heapsort (O(n log n), sem buffer e sem
 * variável global). Não são estáveis. mergesort é o estável (buffer de n elementos; -1/ENOMEM sem memória,
 * -1/EINVAL com largura menor que sizeof(void *) / 2, como o BSD). psort é o qsort (a Apple paraleliza, aqui é sequencial).
 */
#include "sys.h"

void *malloc(size_t);
void free(void *);
void *memcpy(void *, const void *, size_t);
void qsort(void *, size_t, size_t, int (*)(const void *, const void *));

typedef struct { void *isa; int flags, reserved; int (*invoke)(void *, const void *, const void *); } sb_block_t;
typedef int (*sb_cmp_t)(void *ctx, const void *, const void *);
static int via_fn(void *ctx, const void *a, const void *b) { return ((int (*)(const void *, const void *))ctx)(a, b); }
static int via_block(void *ctx, const void *a, const void *b) { sb_block_t *k = ctx; return k->invoke(k, a, b); }

static void *bsearch_g(const void *key, const void *base, size_t n, size_t sz, sb_cmp_t cmp, void *ctx) {
    size_t lo = 0, hi = n;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        const void *e = (const u8 *)base + mid * sz;
        int c = cmp(ctx, key, e);
        if (!c) return (void *)e;
        if (c < 0) hi = mid; else lo = mid + 1;
    }
    return 0;
}
void *bsearch_b(const void *key, const void *base, size_t n, size_t sz, void *block) { return bsearch_g(key, base, n, sz, via_block, block); }

static void swap_(u8 *a, u8 *b, size_t sz) { while (sz--) { u8 t = *a; *a++ = *b; *b++ = t; } }
static void sift(u8 *a, size_t root, size_t n, size_t sz, sb_cmp_t cmp, void *ctx) {
    for (;;) {
        size_t c = 2 * root + 1;
        if (c >= n) return;
        if (c + 1 < n && cmp(ctx, a + c * sz, a + (c + 1) * sz) < 0) c++;
        if (cmp(ctx, a + root * sz, a + c * sz) >= 0) return;
        swap_(a + root * sz, a + c * sz, sz);
        root = c;
    }
}
static int heapsort_g(void *base, size_t n, size_t sz, sb_cmp_t cmp, void *ctx) {
    if (!sz) { cx_errno = CX_EINVAL; return -1; }
    u8 *a = base;
    for (size_t i = n / 2; i-- > 0;) sift(a, i, n, sz, cmp, ctx);
    for (size_t end = n; end > 1; end--) { swap_(a, a + (end - 1) * sz, sz); sift(a, 0, end - 1, sz, cmp, ctx); }
    return 0;
}
int heapsort(void *base, size_t n, size_t sz, int (*cmp)(const void *, const void *)) { return heapsort_g(base, n, sz, via_fn, (void *)cmp); }
int heapsort_b(void *base, size_t n, size_t sz, void *block) { return heapsort_g(base, n, sz, via_block, block); }

void qsort_r(void *base, size_t n, size_t sz, void *thunk, int (*cmp)(void *, const void *, const void *)) {
    /* heapsort: O(n log n) no pior caso, sem malloc e sem estado global. Não é estável. */
    if (n < 2 || !sz || !cmp) return;
    u8 *a = base;
    for (size_t i = n / 2; i-- > 0;) sift(a, i, n, sz, cmp, thunk);
    for (size_t end = n; end > 1; end--) {
        swap_(a, a + (end - 1) * sz, sz);
        sift(a, 0, end - 1, sz, cmp, thunk);
    }
}
void qsort_b(void *base, size_t n, size_t sz, void *block) { qsort_r(base, n, sz, block, via_block); }

static void merge_rec(u8 *a, u8 *tmp, size_t n, size_t sz, sb_cmp_t cmp, void *ctx) {
    if (n < 2) return;
    size_t h = n / 2;
    merge_rec(a, tmp, h, sz, cmp, ctx);
    merge_rec(a + h * sz, tmp, n - h, sz, cmp, ctx);
    if (cmp(ctx, a + (h - 1) * sz, a + h * sz) <= 0) return;      /* já em ordem */
    memcpy(tmp, a, h * sz);
    size_t i = 0, j = h, k = 0;
    while (i < h && j < n) {
        if (cmp(ctx, tmp + i * sz, a + j * sz) <= 0) memcpy(a + k++ * sz, tmp + i++ * sz, sz);   /* estável */
        else memcpy(a + k++ * sz, a + j++ * sz, sz);
    }
    while (i < h) memcpy(a + k++ * sz, tmp + i++ * sz, sz);
}
static int mergesort_g(void *base, size_t n, size_t sz, sb_cmp_t cmp, void *ctx) {
    if (sz < sizeof(void *) / 2) { cx_errno = CX_EINVAL; return -1; }
    if (n < 2) return 0;
    u8 *tmp = malloc((n / 2 + 1) * sz);
    if (!tmp) { cx_errno = CX_ENOMEM; return -1; }
    merge_rec(base, tmp, n, sz, cmp, ctx);
    free(tmp);
    return 0;
}
int mergesort(void *base, size_t n, size_t sz, int (*cmp)(const void *, const void *)) { return mergesort_g(base, n, sz, via_fn, (void *)cmp); }
int mergesort_b(void *base, size_t n, size_t sz, void *block) { return mergesort_g(base, n, sz, via_block, block); }

void psort(void *base, size_t n, size_t sz, int (*cmp)(const void *, const void *)) { qsort(base, n, sz, cmp); }
void psort_r(void *base, size_t n, size_t sz, void *thunk, int (*cmp)(void *, const void *, const void *)) { qsort_r(base, n, sz, thunk, cmp); }
void psort_b(void *base, size_t n, size_t sz, void *block) {
    /* estável. qsort_b é o heapsort (não estável) e não tem mais trava global. */
    if (mergesort_g(base, n, sz, via_block, block) < 0) heapsort_g(base, n, sz, via_block, block);
}
