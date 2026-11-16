/*
 * mOS libSystem — conversão entre multibyte (UTF-8) e caracteres largos (wchar_t de 32 bits, como no macOS).
 * Decodificador restartável e estrito: rejeita codificação longa demais, substitutos UTF-16 (D800..DFFF) e code
 * points acima de 0x10FFFF, e guarda a sequência pela metade no mbstate_t (decodificação byte a byte). As funções
 * de string (mbsrtowcs/wcsrtombs) e wcwidth ficam em posixmore.c, sobre estas. As variantes _l ignoram o locale
 * (o mOS tem só o "C"/UTF-8). Teste: tools/multibyte_hosttest.sh.
 */
#include "sys.h"

typedef int wchar_t_;
/* mbstate_t do macOS: 128 bytes opacos; usamos só o início. need = bytes de continuação ainda esperados,
 * len = tamanho total da sequência, acc = code point parcial já montado. */
typedef struct { int need, len; u32 acc; } cx_mbstate_t;
#define CX_EILSEQ 92                                 /* EILSEQ no Darwin */

/* valor mínimo válido para um UTF-8 de `len` bytes (rejeita codificação longa demais) */
static unsigned utf8_lower(int len) { return len == 2 ? 0x80 : len == 3 ? 0x800 : 0x10000; }
/* um code point é inválido: substituto UTF-16 (D800..DFFF) ou acima de 0x10FFFF */
static int cp_bad(unsigned v, int len) { return v < utf8_lower(len) || (v >= 0xD800 && v <= 0xDFFF) || v > 0x10FFFF; }

/* núcleo restartável: decodifica a partir do estado st, consumindo de s[0..n). Devolve os bytes consumidos
 * NESTA chamada ao completar o caractere (0 se foi o NUL), -2 incompleto (estado salvo), -1 inválido (EILSEQ). */
static long mb_pull(wchar_t_ *out, const unsigned char *s, size_t n, cx_mbstate_t *st) {
    size_t i = 0;
    if (st->need == 0) {                             /* começo de um caractere */
        unsigned c = s[0];
        int len = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 0;
        if (!len) { cx_errno = CX_EILSEQ; return -1; }
        if (len == 1) { if (out) *out = (wchar_t_)c; return c ? 1 : 0; }
        st->len = len; st->need = len - 1; st->acc = c & (0x7Fu >> len);
        i = 1;
    }
    while (st->need && i < n) {
        unsigned b = s[i];
        if ((b & 0xC0) != 0x80) { st->need = 0; cx_errno = CX_EILSEQ; return -1; }
        st->acc = st->acc << 6 | (b & 0x3F);
        st->need--; i++;
    }
    if (st->need) return -2;                          /* faltam bytes: estado salvo em st */
    unsigned v = st->acc;
    int len = st->len;
    st->need = 0;
    if (cp_bad(v, len)) { cx_errno = CX_EILSEQ; return -1; }
    if (out) *out = (wchar_t_)v;
    return (long)i;
}

int mbtowc(wchar_t_ *pwc, const char *s, size_t n) {
    if (!s) return 0;                               /* UTF-8 não depende de estado */
    if (!n) return -1;
    cx_mbstate_t st = {0, 0, 0};
    long r = mb_pull(pwc, (const unsigned char *)s, n, &st);
    return r < 0 ? -1 : (int)r;                      /* incompleto também é -1 em mbtowc (sem estado) */
}
int mbtowc_l(wchar_t_ *pwc, const char *s, size_t n, void *loc) { (void)loc; return mbtowc(pwc, s, n); }

size_t mbrtowc(wchar_t_ *pwc, const char *s, size_t n, void *ps) {
    static cx_mbstate_t internal;
    cx_mbstate_t *st = ps ? (cx_mbstate_t *)ps : &internal;
    if (!s) {                                        /* reset; sequência pela metade vira EILSEQ, como no musl */
        if (st->need) { st->need = 0; cx_errno = CX_EILSEQ; return (size_t)-1; }
        return 0;
    }
    if (!n) return (size_t)-2;
    long r = mb_pull(pwc, (const unsigned char *)s, n, st);
    return r == -1 ? (size_t)-1 : r == -2 ? (size_t)-2 : (size_t)r;
}
size_t mbrtowc_l(wchar_t_ *pwc, const char *s, size_t n, void *ps, void *loc) { (void)loc; return mbrtowc(pwc, s, n, ps); }
int mblen(const char *s, size_t n) {
    if (!s) return 0;
    if (!n) return -1;
    cx_mbstate_t st = {0, 0, 0};
    long r = mb_pull(0, (const unsigned char *)s, n, &st);
    return r < 0 ? -1 : (int)r;
}
int mblen_l(const char *s, size_t n, void *loc) { (void)loc; return mblen(s, n); }
size_t mbrlen(const char *s, size_t n, void *ps) {
    static cx_mbstate_t internal;
    return mbrtowc(0, s, n, ps ? ps : &internal);
}
size_t mbrlen_l(const char *s, size_t n, void *ps, void *loc) { (void)loc; return mbrlen(s, n, ps); }

int wctomb(char *s, wchar_t_ wc) {
    if (!s) return 0;                               /* codificação não depende de estado */
    unsigned v = (unsigned)wc;
    if (v >= 0xD800 && v <= 0xDFFF) { cx_errno = CX_EILSEQ; return -1; }   /* substituto não se codifica */
    if (v < 0x80) { s[0] = (char)v; return 1; }
    if (v < 0x800) { s[0] = (char)(0xC0 | v >> 6); s[1] = (char)(0x80 | (v & 0x3F)); return 2; }
    if (v < 0x10000) { s[0] = (char)(0xE0 | v >> 12); s[1] = (char)(0x80 | ((v >> 6) & 0x3F)); s[2] = (char)(0x80 | (v & 0x3F)); return 3; }
    if (v < 0x110000) {
        s[0] = (char)(0xF0 | v >> 18); s[1] = (char)(0x80 | ((v >> 12) & 0x3F));
        s[2] = (char)(0x80 | ((v >> 6) & 0x3F)); s[3] = (char)(0x80 | (v & 0x3F));
        return 4;
    }
    cx_errno = CX_EILSEQ;
    return -1;
}
int wctomb_l(char *s, wchar_t_ wc, void *loc) { (void)loc; return wctomb(s, wc); }
size_t wcrtomb(char *s, wchar_t_ wc, void *ps) { (void)ps; char b[4]; int r = wctomb(s ? s : b, wc); return r < 0 ? (size_t)-1 : (size_t)r; }
size_t wcrtomb_l(char *s, wchar_t_ wc, void *ps, void *loc) { (void)loc; return wcrtomb(s, wc, ps); }
/* mbsinit: estado inicial quando não há sequência pela metade */
int mbsinit(const void *ps) { return !ps || ((const cx_mbstate_t *)ps)->need == 0; }
