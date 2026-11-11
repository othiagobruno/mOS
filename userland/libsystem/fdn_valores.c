/*
 * DateInterval, IndexPath e URLQueryItem do overlay Swift do Foundation. Nenhum é @frozen: o app passa
 * por endereço e lê o tamanho na metadata (fdn_meta.S).
 *   DateInterval   start (Double desde 2001) e duration (Double), 16 bytes POD
 *   IndexPath      caixa com contagem de referências: n índices Int
 *   URLQueryItem   caixa: name e value (C strings; value pode faltar)
 * A caixa (fdn_box) começa com a contagem e o destrutor; a tabela fdn_vwt_box retém ao copiar e
 * solta ao destruir. Nada aqui altera uma caixa compartilhada: as operações devolvem caixa nova.
 *
 * String que o app passa num init é +1 e não é solta aqui (o libSystem não sabe soltar a String
 * nativa do runtime real): a cópia vai para C e a String original vaza uma referência.
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include "fdn_tipos.h"

#define SW __attribute__((swiftcall))
#define CTX __attribute__((swift_context))
#define USED __attribute__((used))
#if defined(__x86_64__)
#define SRET(sym, impl) __asm__(".text\n.p2align 4\n.globl \"" sym "\"\n\"" sym "\":\n" \
    "  movq %r8, %r9\n  movq %rcx, %r8\n  movq %rdx, %rcx\n  movq %rsi, %rdx\n" \
    "  movq %rdi, %rsi\n  movq %rax, %rdi\n  jmp " #impl "\n")
#else
#define SRET(sym, impl)
#endif

/* ---------------------------------------------------------------- caixa */

void fdn_box_retain(void *b) { if (b) __atomic_add_fetch(&((fdn_box *)b)->rc, 1, __ATOMIC_RELAXED); }
void fdn_box_release(void *b) {
    fdn_box *x = b;
    if (x && __atomic_sub_fetch(&x->rc, 1, __ATOMIC_ACQ_REL) == 0) {
        if (x->dtor) x->dtor(x);
        free(x);
    }
}
void *fdn_box_new(size_t size, void (*dtor)(void *)) {
    fdn_box *b = calloc(1, size);
    if (!b) return 0;
    b->rc = 1;
    b->dtor = dtor;
    return b;
}

static SW void *bx_copy(void *d, void *s, const void *md) { void *b = *(void **)s; (void)md; fdn_box_retain(b); *(void **)d = b; return d; }
static SW void bx_destroy(void *p, const void *md) { (void)md; fdn_box_release(*(void **)p); }
static SW void *bx_assign_copy(void *d, void *s, const void *md) {
    void *n = *(void **)s, *o = *(void **)d;
    (void)md;
    fdn_box_retain(n);
    *(void **)d = n;
    fdn_box_release(o);
    return d;
}
static SW void *bx_take(void *d, void *s, const void *md) { (void)md; *(void **)d = *(void **)s; return d; }
static SW void *bx_assign_take(void *d, void *s, const void *md) {
    void *o = *(void **)d;
    (void)md;
    *(void **)d = *(void **)s;
    fdn_box_release(o);
    return d;
}
static SW unsigned bx_get_tag(const void *v, unsigned empty, const void *md) { (void)md; return fdn_enum_tag_get(v, empty, 8); }
static SW void bx_store_tag(void *v, unsigned which, unsigned empty, const void *md) { (void)md; fdn_enum_tag_store(v, which, empty, 8); }

const fdn_vwt fdn_vwt_box = {
    bx_copy, bx_destroy, bx_copy, bx_assign_copy, bx_take, bx_assign_take, bx_get_tag, bx_store_tag,
    8, 8, 7u | 0x00010000u, 0
};

static char *dup_str(fdn_str s, int *ok) {
    long cap = 256, n;
    char *b = malloc((size_t)cap);
    while (b) {
        n = fdn_str_read(s, b, cap);
        if (n < 0) { free(b); *ok = 0; return 0; }
        if (n < cap - 1) { *ok = 1; return b; }
        cap *= 4;
        free(b);
        b = malloc((size_t)cap);
    }
    *ok = 0;
    return 0;
}

static fdn_str opt_str(const char *s) {
    fdn_str z = { 0, 0 };
    return s ? fdn_str_make(s, -1) : z;
}

/* ---------------------------------------------------------------- DateInterval */

typedef struct { double start, duration; } dint;

SRET("$s10Foundation12DateIntervalV5start3endAcA0B0V_AGtcfC", di_init_end);
static SW USED void di_init_end(dint *out, const double *start, const double *end) {
    out->start = *start;
    out->duration = *end >= *start ? *end - *start : 0;   /* a Apple para no precondition; aqui fica vazio */
}
SRET("$s10Foundation12DateIntervalV5start8durationAcA0B0V_SdtcfC", di_init_dur);
static SW USED void di_init_dur(dint *out, const double *start, double dur) {
    out->start = *start;
    out->duration = dur >= 0 ? dur : 0;
}
SRET("$s10Foundation12DateIntervalVACycfC", di_init);
static SW USED void di_init(dint *out) { out->start = fdn_ns_now1970() - 978307200.0; out->duration = 0; }
SRET("$s10Foundation12DateIntervalV5startAA0B0Vvg", di_start);
static SW USED void di_start(double *out, const dint *self CTX) { *out = self->start; }
SRET("$s10Foundation12DateIntervalV3endAA0B0Vvg", di_end);
static SW USED void di_end(double *out, const dint *self CTX) { *out = self->start + self->duration; }
SW double di_duration(const dint *self CTX) __asm__("$s10Foundation12DateIntervalV8durationSdvg");
SW double di_duration(const dint *self CTX) { return self->duration; }
SW _Bool di_contains(const double *d, const dint *self CTX) __asm__("$s10Foundation12DateIntervalV8containsySbAA0B0VF");
SW _Bool di_contains(const double *d, const dint *self CTX) { return *d >= self->start && *d <= self->start + self->duration; }
SW _Bool di_eq(const dint *a, const dint *b) __asm__("$s10Foundation12DateIntervalV2eeoiySbAC_ACtFZ");
SW _Bool di_eq(const dint *a, const dint *b) { return a->start == b->start && a->duration == b->duration; }

/* ---------------------------------------------------------------- IndexPath */

typedef struct { fdn_box h; long n; long idx[]; } ipath;

static ipath *ip_new(long n) {
    ipath *p = fdn_box_new(sizeof(ipath) + (size_t)(n > 0 ? n : 0) * sizeof(long), 0);
    if (p) p->n = n > 0 ? n : 0;
    return p;
}
static long ip_cmp(const ipath *a, const ipath *b) {
    long n = a->n < b->n ? a->n : b->n;
    for (long i = 0; i < n; i++)
        if (a->idx[i] != b->idx[i]) return a->idx[i] < b->idx[i] ? -1 : 1;
    return a->n < b->n ? -1 : a->n > b->n;
}

SRET("$s10Foundation9IndexPathVACycfC", ip_init);
static SW USED void ip_init(ipath **out) { *out = ip_new(0); }
SRET("$s10Foundation9IndexPathV5indexACSi_tcfC", ip_init_index);
static SW USED void ip_init_index(ipath **out, long i) {
    ipath *p = ip_new(1);
    if (p) p->idx[0] = i;
    *out = p;
}
/*
 * init(arrayLiteral: Int...): o variádico chega como Array<Int> nativo
 * (_ContiguousArrayStorage: contagem em +16, elementos a partir de +32).
 */
SRET("$s10Foundation9IndexPathV12arrayLiteralACSid_tcfC", ip_init_array);
static SW USED void ip_init_array(ipath **out, const uint8_t *arr) {
    long n = arr ? *(const long *)(arr + 16) : 0;
    ipath *p = ip_new(n);
    if (p && n) memcpy(p->idx, arr + 32, (size_t)n * sizeof(long));
    *out = p;
}
SW long ip_count(ipath *const *self CTX) __asm__("$s10Foundation9IndexPathV5countSivg");
SW long ip_count(ipath *const *self CTX) { return (*self)->n; }
SW long ip_start(ipath *const *self CTX) __asm__("$s10Foundation9IndexPathV05startB0Sivg");
SW long ip_start(ipath *const *self CTX) { return 0; }
SW long ip_end(ipath *const *self CTX) __asm__("$s10Foundation9IndexPathV03endB0Sivg");
SW long ip_end(ipath *const *self CTX) { return (*self)->n; }
/* subscript(Int) -> Int; fora do intervalo a Apple aborta: aqui devolve 0 */
SW long ip_get(long i, ipath *const *self CTX) __asm__("$s10Foundation9IndexPathVyS2icig");
SW long ip_get(long i, ipath *const *self CTX) { return i >= 0 && i < (*self)->n ? (*self)->idx[i] : 0; }
SRET("$s10Foundation9IndexPathV9appendingyACSiF", ip_appending);
static SW USED void ip_appending(ipath **out, long v, ipath *const *self CTX) {
    ipath *p = ip_new((*self)->n + 1);
    if (p) {
        memcpy(p->idx, (*self)->idx, (size_t)(*self)->n * sizeof(long));
        p->idx[(*self)->n] = v;
    }
    *out = p;
}
SRET("$s10Foundation9IndexPathV8dropLastACyF", ip_drop_last);
static SW USED void ip_drop_last(ipath **out, ipath *const *self CTX) {
    long n = (*self)->n ? (*self)->n - 1 : 0;
    ipath *p = ip_new(n);
    if (p && n) memcpy(p->idx, (*self)->idx, (size_t)n * sizeof(long));
    *out = p;
}
SW long ip_compare(ipath *const *other, ipath *const *self CTX) __asm__("$s10Foundation9IndexPathV7compareySo18NSComparisonResultVACF");
SW long ip_compare(ipath *const *other, ipath *const *self CTX) { return ip_cmp(*self, *other); }
SW _Bool ip_eq(ipath *const *a, ipath *const *b) __asm__("$s10Foundation9IndexPathV2eeoiySbAC_ACtFZ");
SW _Bool ip_eq(ipath *const *a, ipath *const *b) { return ip_cmp(*a, *b) == 0; }
SW _Bool ip_lt(ipath *const *a, ipath *const *b) __asm__("$s10Foundation9IndexPathV1loiySbAC_ACtFZ");
SW _Bool ip_lt(ipath *const *a, ipath *const *b) { return ip_cmp(*a, *b) < 0; }
SW _Bool ip_le(ipath *const *a, ipath *const *b) __asm__("$s10Foundation9IndexPathV2leoiySbAC_ACtFZ");
SW _Bool ip_le(ipath *const *a, ipath *const *b) { return ip_cmp(*a, *b) <= 0; }

/* ---------------------------------------------------------------- URLQueryItem */

typedef struct { fdn_box h; char *name; char *value; } qitem;

static void qi_free(void *p) {
    qitem *q = p;
    free(q->name);
    free(q->value);
}
void *fdn_query_item_new(const char *name, const char *value) {
    qitem *q = fdn_box_new(sizeof *q, qi_free);
    if (!q) return 0;
    q->name = malloc(strlen(name) + 1);
    if (q->name) strcpy(q->name, name);
    if (value) {
        q->value = malloc(strlen(value) + 1);
        if (q->value) strcpy(q->value, value);
    }
    return q;
}
const char *fdn_query_item_name(void *q) { return q ? ((qitem *)q)->name : 0; }
const char *fdn_query_item_value(void *q) { return q ? ((qitem *)q)->value : 0; }

/* init(name: String, value: String?): value nil = os dois words zerados */
SRET("$s10Foundation12URLQueryItemV4name5valueACSSh_SSSghtcfC", qi_init);
static SW USED void qi_init(void **out, fdn_str name, fdn_str value) {
    int ok1 = 0, ok2 = 0;
    char *n = dup_str(name, &ok1);
    char *v = (value.w0 || value.w1) ? dup_str(value, &ok2) : 0;
    *out = fdn_query_item_new(ok1 && n ? n : "", ok2 ? v : 0);
    free(n);
    free(v);
}
SW fdn_str qi_name(void *const *self CTX) __asm__("$s10Foundation12URLQueryItemV4nameSSvg");
SW fdn_str qi_name(void *const *self CTX) { const char *s = fdn_query_item_name(*self); return fdn_str_make(s ? s : "", -1); }
SW fdn_str qi_value(void *const *self CTX) __asm__("$s10Foundation12URLQueryItemV5valueSSSgvg");
SW fdn_str qi_value(void *const *self CTX) { return opt_str(fdn_query_item_value(*self)); }
SW _Bool qi_eq(void *const *a, void *const *b) __asm__("$s10Foundation12URLQueryItemV2eeoiySbAC_ACtFZ");
SW _Bool qi_eq(void *const *a, void *const *b) {
    const char *va = fdn_query_item_value(*a), *vb = fdn_query_item_value(*b);
    return strcmp(fdn_query_item_name(*a), fdn_query_item_name(*b)) == 0 && ((!va && !vb) || (va && vb && strcmp(va, vb) == 0));
}
