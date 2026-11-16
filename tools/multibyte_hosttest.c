/*
 * mOS — teste de host de userland/libsystem/multibyte.c. O objeto freestanding entra com o prefixo mos_
 * e os símbolos são comparados com a libc do host (en_US.UTF-8). Cobre ida e volta de todo o espaço Unicode,
 * rejeição de codificação longa demais / substitutos / fora de faixa, e a decodificação byte a byte com estado.
 * Rodar: tools/multibyte_hosttest.sh
 */
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <stdlib.h>

int *__error(void) { static int e; return &e; }   /* cx_errno do multibyte.c */

typedef int wchar_t_;
typedef struct { int need, len; unsigned acc; } mos_mbstate;
size_t mos_mbrtowc(wchar_t_ *, const char *, size_t, void *);
int mos_mbtowc(wchar_t_ *, const char *, size_t);
int mos_mblen(const char *, size_t);
size_t mos_mbrlen(const char *, size_t, void *);
int mos_wctomb(char *, wchar_t_);
size_t mos_wcrtomb(char *, wchar_t_, void *);
int mos_mbsinit(const void *);

static int falhas, total;
#define CHECK(c, ...) do { total++; if (!(c)) { falhas++; printf("FALHA %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* codificador UTF-8 de referência (independe do locale do host): devolve o número de bytes */
static int ref_utf8(char *b, unsigned v) {
    if (v < 0x80) { b[0] = (char)v; return 1; }
    if (v < 0x800) { b[0] = (char)(0xC0 | v >> 6); b[1] = (char)(0x80 | (v & 0x3F)); return 2; }
    if (v < 0x10000) { b[0] = (char)(0xE0 | v >> 12); b[1] = (char)(0x80 | ((v >> 6) & 0x3F)); b[2] = (char)(0x80 | (v & 0x3F)); return 3; }
    b[0] = (char)(0xF0 | v >> 18); b[1] = (char)(0x80 | ((v >> 12) & 0x3F));
    b[2] = (char)(0x80 | ((v >> 6) & 0x3F)); b[3] = (char)(0x80 | (v & 0x3F)); return 4;
}

int main(void) {
    /* ida e volta de todo code point válido: wctomb do mOS gera os bytes do codificador de referência, e mbtowc lê de volta */
    long roundtrip = 0;
    for (unsigned cp = 1; cp <= 0x10FFFF; cp++) {
        if (cp >= 0xD800 && cp <= 0xDFFF) {                 /* substituto: wctomb recusa */
            char b[8];
            CHECK(mos_wctomb(b, (int)cp) == -1, "wctomb aceitou substituto U+%04X", cp);
            continue;
        }
        char mb_mos[8] = {0}, mb_ref[8] = {0};
        int n_mos = mos_wctomb(mb_mos, (int)cp);
        int n_ref = ref_utf8(mb_ref, cp);
        if (n_mos != n_ref || memcmp(mb_mos, mb_ref, n_mos)) { CHECK(0, "wctomb U+%04X: %d vs %d", cp, n_mos, n_ref); break; }
        wchar_t_ back = 0;
        int r = mos_mbtowc(&back, mb_mos, n_mos);
        if (r != n_mos || back != (wchar_t_)cp) { CHECK(0, "mbtowc U+%04X: r=%d back=U+%04X", cp, r, (unsigned)back); break; }
        roundtrip++;
    }
    CHECK(roundtrip > 0x10F000, "ida e volta de %ld code points", roundtrip);

    /* codificação longa demais: 0xC0 0x80 (NUL), 0xE0 0x80 0x80, 0xF0 0x80..  -> EILSEQ */
    CHECK(mos_mbtowc(0, "\xC0\x80", 2) == -1, "overlong C0 80 aceito");
    CHECK(mos_mbtowc(0, "\xC1\xBF", 2) == -1, "overlong C1 BF aceito");
    CHECK(mos_mbtowc(0, "\xE0\x80\x80", 3) == -1, "overlong E0 80 80 aceito");
    CHECK(mos_mbtowc(0, "\xE0\x9F\xBF", 3) == -1, "overlong E0 9F BF aceito");
    CHECK(mos_mbtowc(0, "\xF0\x80\x80\x80", 4) == -1, "overlong F0 80 80 80 aceito");
    /* substituto codificado em UTF-8 (CESU-8): ED A0 80 = U+D800 -> EILSEQ */
    CHECK(mos_mbtowc(0, "\xED\xA0\x80", 3) == -1, "substituto ED A0 80 aceito");
    /* acima de 0x10FFFF: F4 90 80 80 = U+110000 -> EILSEQ */
    CHECK(mos_mbtowc(0, "\xF4\x90\x80\x80", 4) == -1, "fora de faixa F4 90 80 80 aceito");
    /* byte de continuação solto, e lead sem continuação */
    CHECK(mos_mbtowc(0, "\x80", 1) == -1, "continuação solta aceita");
    CHECK(mos_mbtowc(0, "\xE2\x28\xA1", 3) == -1, "continuação inválida aceita");

    /* casos válidos nos limites */
    wchar_t_ wc;
    CHECK(mos_mbtowc(&wc, "\xC2\x80", 2) == 2 && wc == 0x80, "U+0080 mínimo de 2 bytes");
    CHECK(mos_mbtowc(&wc, "\xE0\xA0\x80", 3) == 3 && wc == 0x800, "U+0800 mínimo de 3 bytes");
    CHECK(mos_mbtowc(&wc, "\xF0\x90\x80\x80", 4) == 4 && wc == 0x10000, "U+10000 mínimo de 4 bytes");
    CHECK(mos_mbtowc(&wc, "\xF4\x8F\xBF\xBF", 4) == 4 && wc == 0x10FFFF, "U+10FFFF máximo");
    CHECK(mos_mbtowc(&wc, "A", 1) == 1 && wc == 'A', "ASCII");
    CHECK(mos_mbtowc(&wc, "\x00", 1) == 0, "NUL devolve 0");

    /* decodificação byte a byte com estado (iostream alimenta um byte de cada vez): "é" = C3 A9 */
    mos_mbstate st = {0, 0, 0};
    CHECK(mos_mbsinit(&st), "estado inicial");
    size_t r1 = mos_mbrtowc(&wc, "\xC3", 1, &st);
    CHECK(r1 == (size_t)-2 && !mos_mbsinit(&st), "primeiro byte: incompleto, estado pendente");
    size_t r2 = mos_mbrtowc(&wc, "\xA9", 1, &st);
    CHECK(r2 == 1 && wc == 0xE9 && mos_mbsinit(&st), "segundo byte: completa U+00E9");
    /* emoji 😀 = F0 9F 98 80 em três pedaços */
    st = (mos_mbstate){0, 0, 0};
    CHECK(mos_mbrtowc(&wc, "\xF0\x9F", 2, &st) == (size_t)-2, "emoji: 2 de 4 bytes");
    CHECK(mos_mbrtowc(&wc, "\x98", 1, &st) == (size_t)-2, "emoji: 3 de 4 bytes");
    CHECK(mos_mbrtowc(&wc, "\x80", 1, &st) == 1 && wc == 0x1F600, "emoji: completa U+1F600");
    /* mbrtowc(s=0) com estado pendente -> EILSEQ; depois reseta */
    CHECK(mos_mbrtowc(&wc, "\xC3", 1, &st) == (size_t)-2, "pendente de novo");
    CHECK(mos_mbrtowc(0, 0, 0, &st) == (size_t)-1, "reset com pendência vira EILSEQ");
    /* n==0 -> incompleto */
    st = (mos_mbstate){0, 0, 0};
    CHECK(mos_mbrtowc(&wc, "A", 0, &st) == (size_t)-2, "n=0 incompleto");
    /* mbrlen conta bytes sem escrever o caractere */
    CHECK(mos_mbrlen("\xE2\x82\xAC", 3, 0) == 3, "mbrlen do euro");

    printf("multibyte: %d/%d ok\n", total - falhas, total);
    return falhas != 0;
}
