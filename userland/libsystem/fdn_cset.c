/*
 * CharacterSet do overlay Swift do Foundation. Não é @frozen: o app passa o valor por endereço e
 * conhece o tamanho pela metadata (fdn_meta.S). Por dentro o valor é um ponteiro para um bloco nosso
 * com contagem de referências (a tabela fdn_vwt_cset copia retendo e destrói soltando; métodos
 * mutating copiam o bloco antes de escrever se ele está compartilhado).
 *
 * O conjunto é uma lista ordenada de intervalos [lo, hi] de escalares Unicode, sem sobreposição.
 * Os conjuntos prontos seguem as categorias do Unicode que a Apple documenta para cada um; os de
 * URL (urlQueryAllowed...) são os conjuntos ASCII exatos do NSCharacterSet. As tabelas de letras,
 * dígitos, pontuação e símbolos cobrem ASCII e Latin-1 exatamente e o resto por blocos do Unicode
 * (aproximação: dentro de um bloco nem todo ponto é da categoria).
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include "fdn_tipos.h"

void qsort(void *b, size_t n, size_t s, int (*cmp)(const void *, const void *));

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

#define UMAX 0x10FFFFu

typedef struct { uint32_t lo, hi; } rng;
typedef struct fdn_cset { long rc; int n, cap; rng r[]; } fdn_cset;

/* ---------------------------------------------------------------- bloco */

static fdn_cset *cs_new(int cap) {
    fdn_cset *c;
    if (cap < 4) cap = 4;
    c = malloc(sizeof *c + (size_t)cap * sizeof(rng));
    if (!c) return 0;
    c->rc = 1;
    c->n = 0;
    c->cap = cap;
    return c;
}
static void cs_retain(fdn_cset *c) { if (c) __atomic_add_fetch(&c->rc, 1, __ATOMIC_RELAXED); }
static void cs_release(fdn_cset *c) { if (c && __atomic_sub_fetch(&c->rc, 1, __ATOMIC_ACQ_REL) == 0) free(c); }

static fdn_cset *cs_push(fdn_cset *c, uint32_t lo, uint32_t hi) {
    if (!c) return 0;
    if (c->n && lo <= c->r[c->n - 1].hi + 1) {          /* encosta no último: estende */
        if (hi > c->r[c->n - 1].hi) c->r[c->n - 1].hi = hi;
        return c;
    }
    if (c->n == c->cap) {
        fdn_cset *d = realloc(c, sizeof *c + (size_t)c->cap * 2 * sizeof(rng));
        if (!d) return c;
        c = d;
        c->cap *= 2;
    }
    c->r[c->n].lo = lo;
    c->r[c->n].hi = hi;
    c->n++;
    return c;
}

static int cmp_rng(const void *a, const void *b) {
    const rng *x = a, *y = b;
    return x->lo < y->lo ? -1 : x->lo > y->lo;
}

/* lista qualquer de intervalos -> conjunto normalizado */
fdn_cset *fdn_cset_from(const rng *r, int n) {
    rng *t = malloc((size_t)(n ? n : 1) * sizeof *t);
    fdn_cset *c = cs_new(n);
    if (!t || !c) { free(t); return c; }
    memcpy(t, r, (size_t)n * sizeof *t);
    qsort(t, (size_t)n, sizeof *t, cmp_rng);
    for (int i = 0; i < n; i++) c = cs_push(c, t[i].lo, t[i].hi);
    free(t);
    return c;
}

int fdn_cset_contains(const fdn_cset *c, uint32_t u) {
    int a = 0, b = c ? c->n - 1 : -1;
    while (a <= b) {
        int m = (a + b) / 2;
        if (u < c->r[m].lo) b = m - 1;
        else if (u > c->r[m].hi) a = m + 1;
        else return 1;
    }
    return 0;
}

fdn_cset *fdn_cset_union(const fdn_cset *x, const fdn_cset *y) {
    int nx = x ? x->n : 0, ny = y ? y->n : 0, i = 0, j = 0;
    fdn_cset *c = cs_new(nx + ny);
    while (c && (i < nx || j < ny)) {
        const rng *p = (j >= ny || (i < nx && x->r[i].lo <= y->r[j].lo)) ? &x->r[i++] : &y->r[j++];
        c = cs_push(c, p->lo, p->hi);
    }
    return c;
}

fdn_cset *fdn_cset_invert(const fdn_cset *x) {
    fdn_cset *c = cs_new((x ? x->n : 0) + 1);
    uint32_t next = 0;
    for (int i = 0; c && x && i < x->n; i++) {
        if (x->r[i].lo > next) c = cs_push(c, next, x->r[i].lo - 1);
        next = x->r[i].hi + 1;
    }
    if (c && next <= UMAX) c = cs_push(c, next, UMAX);
    return c;
}

fdn_cset *fdn_cset_subtract(const fdn_cset *x, const fdn_cset *y) {
    fdn_cset *iy = fdn_cset_invert(y), *c = cs_new(x ? x->n : 1);
    int i = 0, j = 0;
    /* interseção de x com o complemento de y */
    while (c && iy && x && i < x->n && j < iy->n) {
        uint32_t lo = x->r[i].lo > iy->r[j].lo ? x->r[i].lo : iy->r[j].lo;
        uint32_t hi = x->r[i].hi < iy->r[j].hi ? x->r[i].hi : iy->r[j].hi;
        if (lo <= hi) c = cs_push(c, lo, hi);
        if (x->r[i].hi < iy->r[j].hi) i++; else j++;
    }
    free(iy);
    return c;
}

int fdn_cset_superset(const fdn_cset *x, const fdn_cset *y) {
    fdn_cset *d = fdn_cset_subtract(y, x);
    int ok = d && d->n == 0;
    free(d);
    return ok;
}

/* escalares de um texto UTF-8 (sequência inválida: U+FFFD, como o String do Swift) */
fdn_cset *fdn_cset_from_utf8(const char *s, long n) {
    rng *t = malloc((size_t)(n > 0 ? n : 1) * sizeof *t);
    int k = 0;
    fdn_cset *c;
    const unsigned char *p = (const unsigned char *)s, *e = p + n;
    if (!t) return cs_new(1);
    while (p < e) {
        uint32_t u = *p, need = 0;
        if (u < 0x80) need = 0;
        else if ((u & 0xE0) == 0xC0) { u &= 0x1F; need = 1; }
        else if ((u & 0xF0) == 0xE0) { u &= 0x0F; need = 2; }
        else if ((u & 0xF8) == 0xF0) { u &= 0x07; need = 3; }
        else { u = 0xFFFD; }
        p++;
        while (need && p < e && (*p & 0xC0) == 0x80) { u = (u << 6) | (*p & 0x3F); p++; need--; }
        if (need) u = 0xFFFD;
        t[k].lo = t[k].hi = u;
        k++;
    }
    c = fdn_cset_from(t, k);
    free(t);
    return c;
}

/* ---------------------------------------------------------------- tabelas */

static const rng T_WS[] = { { 0x09, 0x09 }, { 0x20, 0x20 }, { 0xA0, 0xA0 }, { 0x1680, 0x1680 }, { 0x2000, 0x200A },
                            { 0x202F, 0x202F }, { 0x205F, 0x205F }, { 0x3000, 0x3000 } };
static const rng T_NL[] = { { 0x0A, 0x0D }, { 0x85, 0x85 }, { 0x2028, 0x2029 } };
static const rng T_DIGIT[] = { { '0', '9' }, { 0x660, 0x669 }, { 0x6F0, 0x6F9 }, { 0x7C0, 0x7C9 }, { 0x966, 0x96F },
                               { 0x9E6, 0x9EF }, { 0xA66, 0xA6F }, { 0xAE6, 0xAEF }, { 0xB66, 0xB6F }, { 0xBE6, 0xBEF },
                               { 0xC66, 0xC6F }, { 0xCE6, 0xCEF }, { 0xD66, 0xD6F }, { 0xE50, 0xE59 }, { 0xED0, 0xED9 },
                               { 0xF20, 0xF29 }, { 0x1040, 0x1049 }, { 0x17E0, 0x17E9 }, { 0x1810, 0x1819 },
                               { 0xFF10, 0xFF19 }, { 0x1D7CE, 0x1D7FF } };
/* L* e M* (letters inclui marcas) */
static const rng T_LETTER[] = { { 'A', 'Z' }, { 'a', 'z' }, { 0xAA, 0xAA }, { 0xB5, 0xB5 }, { 0xBA, 0xBA }, { 0xC0, 0xD6 },
                                { 0xD8, 0xF6 }, { 0xF8, 0x2C1 }, { 0x2C6, 0x2D1 }, { 0x2E0, 0x2E4 }, { 0x2EC, 0x2EC },
                                { 0x2EE, 0x2EE }, { 0x300, 0x374 }, { 0x376, 0x377 }, { 0x37A, 0x37D }, { 0x37F, 0x37F },
                                { 0x386, 0x386 }, { 0x388, 0x3FF }, { 0x400, 0x481 }, { 0x483, 0x52F }, { 0x531, 0x556 },
                                { 0x560, 0x588 }, { 0x591, 0x5C7 }, { 0x5D0, 0x5EA }, { 0x610, 0x61A }, { 0x620, 0x65F },
                                { 0x66E, 0x6D3 }, { 0x6D5, 0x6DC }, { 0x6DF, 0x6E8 }, { 0x6EA, 0x6EF }, { 0x6FA, 0x6FF },
                                { 0x710, 0x7B1 }, { 0x900, 0x963 }, { 0x971, 0x9E5 }, { 0xA00, 0xA65 }, { 0xA70, 0xAE5 },
                                { 0xB00, 0xB65 }, { 0xB70, 0xBE5 }, { 0xC00, 0xC65 }, { 0xC80, 0xCE5 }, { 0xD00, 0xD65 },
                                { 0xE01, 0xE4E }, { 0xE81, 0xECF }, { 0xF00, 0xF00 }, { 0xF40, 0xFBC }, { 0x1000, 0x103F },
                                { 0x10A0, 0x10FF }, { 0x1100, 0x11FF }, { 0x1200, 0x135F }, { 0x13A0, 0x13FF },
                                { 0x1780, 0x17D3 }, { 0x1E00, 0x1FFF }, { 0x2071, 0x2071 }, { 0x207F, 0x207F },
                                { 0x2090, 0x209C }, { 0x20D0, 0x20F0 }, { 0x2C00, 0x2CE4 }, { 0x3005, 0x3006 },
                                { 0x3031, 0x3035 }, { 0x3041, 0x3096 }, { 0x3099, 0x309F }, { 0x30A1, 0x30FA },
                                { 0x30FC, 0x30FF }, { 0x3105, 0x312F }, { 0x3131, 0x318E }, { 0x3400, 0x4DBF },
                                { 0x4E00, 0x9FFF }, { 0xA000, 0xA48C }, { 0xAC00, 0xD7A3 }, { 0xF900, 0xFAFF },
                                { 0xFB00, 0xFB4F }, { 0xFB50, 0xFDFB }, { 0xFE70, 0xFEFC }, { 0xFF21, 0xFF3A },
                                { 0xFF41, 0xFF5A }, { 0xFF66, 0xFFDC }, { 0x20000, 0x2FA1F } };
/* P* */
static const rng T_PUNCT[] = { { 0x21, 0x23 }, { 0x25, 0x2A }, { 0x2C, 0x2F }, { 0x3A, 0x3B }, { 0x3F, 0x40 },
                               { 0x5B, 0x5D }, { 0x5F, 0x5F }, { 0x7B, 0x7B }, { 0x7D, 0x7D }, { 0xA1, 0xA1 },
                               { 0xA7, 0xA7 }, { 0xAB, 0xAB }, { 0xB6, 0xB7 }, { 0xBB, 0xBB }, { 0xBF, 0xBF },
                               { 0x37E, 0x37E }, { 0x387, 0x387 }, { 0x55A, 0x55F }, { 0x589, 0x58A }, { 0x5BE, 0x5BE },
                               { 0x5C0, 0x5C0 }, { 0x5C3, 0x5C3 }, { 0x5C6, 0x5C6 }, { 0x5F3, 0x5F4 }, { 0x609, 0x60A },
                               { 0x60C, 0x60D }, { 0x61B, 0x61B }, { 0x61D, 0x61F }, { 0x66A, 0x66D }, { 0x6D4, 0x6D4 },
                               { 0x964, 0x965 }, { 0x970, 0x970 }, { 0xE4F, 0xE4F }, { 0xE5A, 0xE5B }, { 0x2010, 0x2027 },
                               { 0x2030, 0x2043 }, { 0x2045, 0x2051 }, { 0x2053, 0x205E }, { 0x207D, 0x207E },
                               { 0x208D, 0x208E }, { 0x2308, 0x230B }, { 0x2329, 0x232A }, { 0x2768, 0x2775 },
                               { 0x27C5, 0x27C6 }, { 0x27E6, 0x27EF }, { 0x2983, 0x2998 }, { 0x29D8, 0x29DB },
                               { 0x29FC, 0x29FD }, { 0x2E00, 0x2E2E }, { 0x2E30, 0x2E4F }, { 0x3001, 0x3003 },
                               { 0x3008, 0x3011 }, { 0x3014, 0x301F }, { 0x3030, 0x3030 }, { 0x303D, 0x303D },
                               { 0x30A0, 0x30A0 }, { 0x30FB, 0x30FB }, { 0xFE10, 0xFE19 }, { 0xFE30, 0xFE52 },
                               { 0xFE54, 0xFE61 }, { 0xFE63, 0xFE63 }, { 0xFE68, 0xFE68 }, { 0xFE6A, 0xFE6B },
                               { 0xFF01, 0xFF03 }, { 0xFF05, 0xFF0A }, { 0xFF0C, 0xFF0F }, { 0xFF1A, 0xFF1B },
                               { 0xFF1F, 0xFF20 }, { 0xFF3B, 0xFF3D }, { 0xFF3F, 0xFF3F }, { 0xFF5B, 0xFF5B },
                               { 0xFF5D, 0xFF5D }, { 0xFF5F, 0xFF65 } };
/* S* */
static const rng T_SYMBOL[] = { { 0x24, 0x24 }, { 0x2B, 0x2B }, { 0x3C, 0x3E }, { 0x5E, 0x5E }, { 0x60, 0x60 },
                                { 0x7C, 0x7C }, { 0x7E, 0x7E }, { 0xA2, 0xA6 }, { 0xA8, 0xA9 }, { 0xAC, 0xAC },
                                { 0xAE, 0xB1 }, { 0xB4, 0xB4 }, { 0xB8, 0xB8 }, { 0xD7, 0xD7 }, { 0xF7, 0xF7 },
                                { 0x2C2, 0x2C5 }, { 0x2D2, 0x2DF }, { 0x2E5, 0x2EB }, { 0x2ED, 0x2ED }, { 0x2EF, 0x2FF },
                                { 0x375, 0x375 }, { 0x384, 0x385 }, { 0x3F6, 0x3F6 }, { 0x482, 0x482 }, { 0x58D, 0x58F },
                                { 0x606, 0x608 }, { 0x60B, 0x60B }, { 0x60E, 0x60F }, { 0x2044, 0x2044 }, { 0x2052, 0x2052 },
                                { 0x207A, 0x207C }, { 0x208A, 0x208C }, { 0x20A0, 0x20C0 }, { 0x2100, 0x2101 },
                                { 0x2103, 0x2106 }, { 0x2108, 0x2109 }, { 0x2114, 0x2114 }, { 0x2116, 0x2118 },
                                { 0x211E, 0x2123 }, { 0x2125, 0x2125 }, { 0x2127, 0x2127 }, { 0x2129, 0x2129 },
                                { 0x212E, 0x212E }, { 0x213A, 0x213B }, { 0x2140, 0x2144 }, { 0x214A, 0x214D },
                                { 0x214F, 0x214F }, { 0x218A, 0x218B }, { 0x2190, 0x2307 }, { 0x230C, 0x2328 },
                                { 0x232B, 0x2426 }, { 0x2440, 0x244A }, { 0x249C, 0x24E9 }, { 0x2500, 0x2767 },
                                { 0x2794, 0x27C4 }, { 0x27C7, 0x27E5 }, { 0x27F0, 0x2982 }, { 0x2999, 0x29D7 },
                                { 0x29DC, 0x29FB }, { 0x29FE, 0x2B73 }, { 0x2B76, 0x2BFF }, { 0x2CE5, 0x2CEA },
                                { 0x3004, 0x3004 }, { 0x3012, 0x3013 }, { 0x3020, 0x3020 }, { 0x309B, 0x309C },
                                { 0xFB29, 0xFB29 }, { 0xFE62, 0xFE62 }, { 0xFE64, 0xFE66 }, { 0xFE69, 0xFE69 },
                                { 0xFF04, 0xFF04 }, { 0xFF0B, 0xFF0B }, { 0xFF1C, 0xFF1E }, { 0xFF3E, 0xFF3E },
                                { 0xFF40, 0xFF40 }, { 0xFF5C, 0xFF5C }, { 0xFF5E, 0xFF5E }, { 0xFFE0, 0xFFE6 },
                                { 0xFFE8, 0xFFEE }, { 0xFFFC, 0xFFFD }, { 0x1F000, 0x1FAFF } };
/* Cc + Cf */
static const rng T_CONTROL[] = { { 0x00, 0x1F }, { 0x7F, 0x9F }, { 0xAD, 0xAD }, { 0x600, 0x605 }, { 0x61C, 0x61C },
                                 { 0x6DD, 0x6DD }, { 0x70F, 0x70F }, { 0x180E, 0x180E }, { 0x200B, 0x200F },
                                 { 0x202A, 0x202E }, { 0x2060, 0x2064 }, { 0x2066, 0x206F }, { 0xFEFF, 0xFEFF },
                                 { 0xFFF9, 0xFFFB }, { 0xE0001, 0xE0001 }, { 0xE0020, 0xE007F } };
/* não atribuídos e não-caracteres mais comuns: substitutos, U+FFFE/FFFF e os não-caracteres do FDD0 */
static const rng T_ILLEGAL[] = { { 0xD800, 0xDFFF }, { 0xFDD0, 0xFDEF }, { 0xFFFE, 0xFFFF }, { 0x1FFFE, 0x1FFFF },
                                 { 0x2FFFE, 0x2FFFF }, { 0x10FFFE, 0x10FFFF } };
/* conjuntos de URL do NSCharacterSet (ASCII): alfanuméricos mais estes */
static const char URL_QUERY[] = "!$&'()*+,-./:;=?@_~";
static const char URL_PATH[] = "!$&'()*+,-./:=@_~";
static const char URL_HOST[] = "!$&'()*+,-.:;=[]_~";

static fdn_cset *cs_table(const rng *t, size_t n) { return fdn_cset_from(t, (int)n); }
#define TAB(t) cs_table(t, sizeof t / sizeof t[0])

static fdn_cset *cs_url(const char *extra) {
    rng t[80];
    int k = 0;
    t[k++] = (rng){ '0', '9' };
    t[k++] = (rng){ 'A', 'Z' };
    t[k++] = (rng){ 'a', 'z' };
    for (const char *p = extra; *p; p++) { t[k].lo = t[k].hi = (unsigned char)*p; k++; }
    return fdn_cset_from(t, k);
}

/* ---------------------------------------------------------------- value witnesses */

static SW void *cv_copy(void *d, void *s, const void *md) { fdn_cset *c = *(fdn_cset **)s; (void)md; cs_retain(c); *(fdn_cset **)d = c; return d; }
static SW void cv_destroy(void *p, const void *md) { (void)md; cs_release(*(fdn_cset **)p); }
static SW void *cv_assign_copy(void *d, void *s, const void *md) {
    fdn_cset *n = *(fdn_cset **)s, *o = *(fdn_cset **)d;
    (void)md;
    cs_retain(n);
    *(fdn_cset **)d = n;
    cs_release(o);
    return d;
}
static SW void *cv_take(void *d, void *s, const void *md) { (void)md; *(void **)d = *(void **)s; return d; }
static SW void *cv_assign_take(void *d, void *s, const void *md) {
    fdn_cset *o = *(fdn_cset **)d;
    (void)md;
    *(void **)d = *(void **)s;
    cs_release(o);
    return d;
}
static SW unsigned cv_get_tag(const void *v, unsigned empty, const void *md) { (void)md; return fdn_enum_tag_get(v, empty, 8); }
static SW void cv_store_tag(void *v, unsigned which, unsigned empty, const void *md) { (void)md; fdn_enum_tag_store(v, which, empty, 8); }

const fdn_vwt fdn_vwt_cset = {
    cv_copy, cv_destroy, cv_copy, cv_assign_copy, cv_take, cv_assign_take, cv_get_tag, cv_store_tag,
    8, 8, 7u | 0x00010000u, 0
};

/* ---------------------------------------------------------------- símbolos */

/* copy-on-write: o bloco que o valor aponta passa a ser só dele */
static void cs_set(fdn_cset **self, fdn_cset *novo) {
    fdn_cset *velho = *self;
    *self = novo;
    cs_release(velho);
}

static fdn_cset *cs_from_str(fdn_str s) {
    char small[64];
    long n = fdn_str_read(s, small, (long)sizeof small);
    if (n >= 0 && n < (long)sizeof small - 1) return fdn_cset_from_utf8(small, n);
    {
        /* maior: lê de novo num buffer do tamanho certo */
        long cap = 1 << 16;
        char *b = malloc((size_t)cap);
        fdn_cset *c;
        if (!b) return cs_new(1);
        n = fdn_str_read(s, b, cap);
        c = fdn_cset_from_utf8(b, n > 0 ? n : 0);
        free(b);
        return c;
    }
}

static fdn_cset *union_free(fdn_cset *a, fdn_cset *b) {
    fdn_cset *c = fdn_cset_union(a, b);
    free(a);
    free(b);
    return c;
}

#define FIXO(nome, sym, expr) \
    SRET(sym, nome); \
    static SW USED void nome(fdn_cset **out) { *out = (expr); }

FIXO(cs_vazio, "$s10Foundation12CharacterSetVACycfC", cs_new(4))
FIXO(cs_ws, "$s10Foundation12CharacterSetV11whitespacesACvgZ", TAB(T_WS))
FIXO(cs_nl, "$s10Foundation12CharacterSetV8newlinesACvgZ", TAB(T_NL))
FIXO(cs_wsnl, "$s10Foundation12CharacterSetV22whitespacesAndNewlinesACvgZ", union_free(TAB(T_WS), TAB(T_NL)))
FIXO(cs_digits, "$s10Foundation12CharacterSetV13decimalDigitsACvgZ", TAB(T_DIGIT))
FIXO(cs_letters, "$s10Foundation12CharacterSetV7lettersACvgZ", TAB(T_LETTER))
FIXO(cs_alnum, "$s10Foundation12CharacterSetV13alphanumericsACvgZ", union_free(TAB(T_LETTER), TAB(T_DIGIT)))
FIXO(cs_punct, "$s10Foundation12CharacterSetV21punctuationCharactersACvgZ", TAB(T_PUNCT))
FIXO(cs_symbols, "$s10Foundation12CharacterSetV7symbolsACvgZ", TAB(T_SYMBOL))
FIXO(cs_control, "$s10Foundation12CharacterSetV17controlCharactersACvgZ", TAB(T_CONTROL))
FIXO(cs_illegal, "$s10Foundation12CharacterSetV17illegalCharactersACvgZ", TAB(T_ILLEGAL))
FIXO(cs_query, "$s10Foundation12CharacterSetV15urlQueryAllowedACvgZ", cs_url(URL_QUERY))
FIXO(cs_path, "$s10Foundation12CharacterSetV14urlPathAllowedACvgZ", cs_url(URL_PATH))
FIXO(cs_host, "$s10Foundation12CharacterSetV14urlHostAllowedACvgZ", cs_url(URL_HOST))

SRET("$s10Foundation12CharacterSetV12charactersInACSSh_tcfC", cs_init_str);
static SW USED void cs_init_str(fdn_cset **out, fdn_str s) { *out = cs_from_str(s); }
/* ClosedRange<Unicode.Scalar>: dois UInt32 num registrador */
typedef struct { uint32_t lo, hi; } scalar_range;
SRET("$s10Foundation12CharacterSetV12charactersInACSNys7UnicodeO6ScalarVG_tcfC", cs_init_range);
static SW USED void cs_init_range(fdn_cset **out, scalar_range r) {
    rng t = { r.lo, r.hi };
    *out = fdn_cset_from(&t, r.lo <= r.hi ? 1 : 0);
}

SW _Bool cs_contains(uint32_t u, fdn_cset *const *self CTX) __asm__("$s10Foundation12CharacterSetV8containsySbs7UnicodeO6ScalarVF");
SW _Bool cs_contains(uint32_t u, fdn_cset *const *self CTX) { return fdn_cset_contains(*self, u) != 0; }
SW _Bool cs_superset(fdn_cset *const *other, fdn_cset *const *self CTX) __asm__("$s10Foundation12CharacterSetV10isSuperset2ofSbAC_tF");
SW _Bool cs_superset(fdn_cset *const *other, fdn_cset *const *self CTX) { return fdn_cset_superset(*self, *other) != 0; }

SRET("$s10Foundation12CharacterSetV5unionyA2CF", cs_union);
static SW USED void cs_union(fdn_cset **out, fdn_cset *const *other, fdn_cset *const *self CTX) { *out = fdn_cset_union(*self, *other); }
SRET("$s10Foundation12CharacterSetV11subtractingyA2CF", cs_subtracting);
static SW USED void cs_subtracting(fdn_cset **out, fdn_cset *const *other, fdn_cset *const *self CTX) { *out = fdn_cset_subtract(*self, *other); }
SRET("$s10Foundation12CharacterSetV8invertedACvg", cs_inverted);
static SW USED void cs_inverted(fdn_cset **out, fdn_cset *const *self CTX) { *out = fdn_cset_invert(*self); }

SW void cs_form_union(fdn_cset *const *other, fdn_cset **self CTX) __asm__("$s10Foundation12CharacterSetV9formUnionyyACF");
SW void cs_form_union(fdn_cset *const *other, fdn_cset **self CTX) { cs_set(self, fdn_cset_union(*self, *other)); }
SW void cs_invert(fdn_cset **self CTX) __asm__("$s10Foundation12CharacterSetV6invertyyF");
SW void cs_invert(fdn_cset **self CTX) { cs_set(self, fdn_cset_invert(*self)); }
SW void cs_insert_str(fdn_str s, fdn_cset **self CTX) __asm__("$s10Foundation12CharacterSetV6insert12charactersInySS_tF");
SW void cs_insert_str(fdn_str s, fdn_cset **self CTX) {
    fdn_cset *a = cs_from_str(s);
    cs_set(self, fdn_cset_union(*self, a));
    free(a);
}
SW void cs_insert_range(scalar_range r, fdn_cset **self CTX) __asm__("$s10Foundation12CharacterSetV6insert12charactersInySNys7UnicodeO6ScalarVG_tF");
SW void cs_insert_range(scalar_range r, fdn_cset **self CTX) {
    rng t = { r.lo, r.hi };
    fdn_cset *a = fdn_cset_from(&t, r.lo <= r.hi ? 1 : 0);
    cs_set(self, fdn_cset_union(*self, a));
    free(a);
}
SW void cs_remove_str(fdn_str s, fdn_cset **self CTX) __asm__("$s10Foundation12CharacterSetV6remove12charactersInySS_tF");
SW void cs_remove_str(fdn_str s, fdn_cset **self CTX) {
    fdn_cset *a = cs_from_str(s);
    cs_set(self, fdn_cset_subtract(*self, a));
    free(a);
}
