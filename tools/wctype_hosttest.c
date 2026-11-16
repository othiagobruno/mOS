#define _GNU_SOURCE
/*
 * mOS — teste de host de userland/libsystem/wctype_wide.c (ctype largo com tabelas Unicode do musl). Checa valores
 * conhecidos (acentos, grego, cirílico, CJK, largura), a consistência de ___runetype com os isw*, e faz uma
 * varredura informativa contra a glibc (pt_BR.utf8) — as diferenças de versão do Unicode são só reportadas.
 * Rodar: tools/wctype_hosttest.sh
 */
#include <locale.h>
#include <stdio.h>
#include <wchar.h>
#include <wctype.h>

int mos_iswalpha(int), mos_iswdigit(int), mos_iswalnum(int), mos_iswpunct(int), mos_iswspace(int),
    mos_iswcntrl(int), mos_iswprint(int), mos_iswgraph(int), mos_iswblank(int), mos_iswxdigit(int),
    mos_iswlower(int), mos_iswupper(int), mos_wcwidth(int);
int mos_towlower(int), mos_towupper(int);
unsigned long mos_wctype(const char *);
int mos_iswctype(int, unsigned long);
unsigned long mos____runetype(int);

/* _CTYPE_* da Apple */
#define A 0x100u
#define D 0x400u
#define L 0x1000u
#define U 0x8000u
#define X 0x10000u

static int falhas, total;
#define CHECK(c, ...) do { total++; if (!(c)) { falhas++; printf("FALHA %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main(void) {
    /* ---- troca de caixa conhecida (mapeamento simples do Unicode) */
    CHECK(mos_towupper(0xE9) == 0xC9, "é->É");                 /* U+00E9 -> U+00C9 */
    CHECK(mos_towlower(0xC9) == 0xE9, "É->é");
    CHECK(mos_towupper(0xE7) == 0xC7, "ç->Ç");
    CHECK(mos_towupper(0x3B1) == 0x391, "α->Α (grego)");
    CHECK(mos_towlower(0x391) == 0x3B1, "Α->α");
    CHECK(mos_towupper(0x44F) == 0x42F, "я->Я (cirílico)");
    CHECK(mos_towlower(0x42F) == 0x44F, "Я->я");
    CHECK(mos_towupper(0x1F600) == 0x1F600, "emoji sem caixa");
    CHECK(mos_towupper('a') == 'A' && mos_towlower('Z') == 'z', "ASCII");
    CHECK(mos_towupper(0x4E2D) == 0x4E2D, "CJK sem caixa");

    /* ---- classificação conhecida */
    CHECK(mos_iswalpha(0xE9) && mos_iswalpha(0x3B1) && mos_iswalpha(0x44F) && mos_iswalpha(0x4E2D) && mos_iswalpha(0x5D0), "letras não-ASCII (latim/grego/cirílico/CJK/hebraico)");
    CHECK(!mos_iswalpha(0x30) && !mos_iswalpha(' ') && !mos_iswalpha(0x3000), "não-letras");
    CHECK(mos_iswlower(0xE9) && !mos_iswupper(0xE9) && mos_iswupper(0xC9) && !mos_iswlower(0xC9), "caixa de é/É");
    CHECK(mos_iswupper(0x391) && mos_iswlower(0x3B1), "caixa grega");
    CHECK(mos_iswspace(0x3000) && mos_iswspace(0x2009) && mos_iswspace(' '), "espaços Unicode");
    CHECK(!mos_iswspace(0xA0), "espaço sem quebra não é space (como no musl/Apple)");
    CHECK(mos_iswpunct(0x2026) && mos_iswpunct('!'), "reticências e ! são punct");
    CHECK(mos_iswcntrl(0x85) && mos_iswcntrl(0x7F) && mos_iswcntrl(0), "controles");
    CHECK(mos_iswdigit('7') && !mos_iswdigit(0x0660), "iswdigit só ASCII (C locale)");
    CHECK(mos_iswxdigit('F') && mos_iswxdigit('a') && !mos_iswxdigit('g'), "xdigit");
    CHECK(mos_iswalnum(0x3B1) && mos_iswalnum('5'), "alnum");

    /* ---- largura na tela */
    CHECK(mos_wcwidth('A') == 1 && mos_wcwidth(0) == 0, "ASCII/NUL");
    CHECK(mos_wcwidth(1) == -1, "controle = -1");
    CHECK(mos_wcwidth(0x300) == 0, "combinante = 0");               /* U+0300 */
    CHECK(mos_wcwidth(0x200B) == 0, "zero-width space");
    CHECK(mos_wcwidth(0x4E2D) == 2 && mos_wcwidth(0xAC00) == 2, "CJK/hangul = 2");
    CHECK(mos_wcwidth(0x1F600) == 2, "emoji = 2");
    CHECK(mos_wcwidth(0xE9) == 1, "é = 1");

    /* ---- wctype / iswctype / ___runetype */
    CHECK(mos_iswctype(0x3B1, mos_wctype("alpha")) && mos_iswctype('5', mos_wctype("digit")), "iswctype por nome");
    CHECK(mos_wctype("inexistente") == 0, "wctype desconhecido = 0");
    unsigned long rt = mos____runetype(0xE9);
    CHECK((rt & A) && (rt & L) && !(rt & U), "___runetype(é): alpha+lower");
    rt = mos____runetype('A');
    CHECK((rt & A) && (rt & U) && (rt & X), "___runetype(A): alpha+upper+xdigit");
    rt = mos____runetype('7');
    CHECK((rt & D) && (rt & X) && (rt & 0xFF) == 7, "___runetype(7): digit, valor 7 no byte baixo");

    /* ---- consistência: ___runetype carrega os mesmos bits que os isw* para todo o BMP */
    long inconsist = 0;
    for (int c = 1; c < 0x10000; c++) {
        unsigned long t = mos____runetype(c);
        if (!!(t & A) != !!mos_iswalpha(c)) inconsist++;
        if (!!(t & U) != !!mos_iswupper(c)) inconsist++;
        if (!!(t & L) != !!mos_iswlower(c)) inconsist++;
    }
    CHECK(inconsist == 0, "___runetype consistente com isw* (%ld divergências)", inconsist);

    /* ---- caixa: upper(lower(x)) == upper(x) para quase todo x. As exceções (µ, ı, İ, ſ, K/Å de símbolo...) são
     * comportamento correto do Unicode, não bug; só reportamos a contagem. */
    long nonidem = 0;
    for (int c = 1; c < 0x10000; c++) if (mos_towupper(mos_towlower(c)) != mos_towupper(c)) nonidem++;
    printf("  caixa não-idempotente em %ld code points (especiais do Unicode: µ, ı, İ, ...)\n", nonidem);
    CHECK(nonidem < 20, "poucas exceções de caixa (%ld)", nonidem);

    /* ---- varredura informativa contra a glibc (pode divergir pela versão do Unicode) */
    if (setlocale(LC_CTYPE, "pt_BR.utf8") || setlocale(LC_CTYPE, "C.utf8")) {
        long da = 0, du = 0, dw = 0;
        for (int c = 0x80; c < 0x10000; c++) {
            if (c >= 0xD800 && c <= 0xDFFF) continue;
            if (!!mos_iswalpha(c) != !!iswalpha(c)) da++;
            if (mos_towupper(c) != (int)towupper(c)) du++;
            if (mos_wcwidth(c) != wcwidth(c)) dw++;
        }
        printf("  vs glibc (BMP>=0x80): iswalpha %ld, towupper %ld, wcwidth %ld divergências (versão do Unicode)\n", da, du, dw);
    } else {
        printf("  (sem locale UTF-8 no host; varredura contra a glibc pulada)\n");
    }

    printf("wctype_wide: %d/%d ok\n", total - falhas, total);
    return falhas != 0;
}
