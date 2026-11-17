/* Teste de host de userland/libsystem/textfit_core.c (fonte de largura fixa: 10 por caractere; "…" vale 10).
 * gcc -O1 -Wall -Wextra -I userland/libsystem userland/tests-appkit/textfit_host.c userland/libsystem/textfit_core.c */
#include <stdio.h>
#include <string.h>
#include "textfit_core.h"

static int fails, total;
#define CHECK(c, m) do { total++; if (!(c)) { fails++; printf("FALHOU: %s\n", m); } } while (0)

static double mono(const char *s, int n, void *ctx) {
    (void)ctx;
    int chars = 0;
    for (int i = 0; i < n; i++) if (((unsigned char)s[i] & 0xC0) != 0x80) chars++;
    return chars * 10.0;
}
static int brk(const char *s, double w, int mode, int *skip) { return tfit_break(s, w, mode, mono, 0, skip); }

int main(void) {
    int skip, k;
    char out[128];

    k = brk("abc def", 100, TFIT_WORD, &skip);
    CHECK(k == 7 && skip == 0, "linha que cabe inteira");
    k = brk("abc def ghi", 75, TFIT_WORD, &skip);
    CHECK(k == 7 && skip == 1, "quebra por palavra no último espaço que cabe");
    k = brk("abcdefghij xyz", 45, TFIT_WORD, &skip);
    CHECK(k == 4 && skip == 0, "palavra maior que a linha quebra por caractere");
    k = brk("abc def", 45, TFIT_CHAR, &skip);
    CHECK(k == 4 && skip == 0, "quebra por caractere");
    k = brk("ab\ncd", 100, TFIT_WORD, &skip);
    CHECK(k == 2 && skip == 1, "\\n termina a linha e é pulado");
    k = brk("ãé õ", 25, TFIT_CHAR, &skip);
    CHECK(k == 4, "UTF-8: não corta no meio do caractere (2 caracteres = 4 bytes)");
    k = brk("x", 3, TFIT_CHAR, &skip);
    CHECK(k == 1, "pelo menos um caractere por linha");

    CHECK(tfit_lines("abc def ghi", 75, TFIT_WORD, mono, 0) == 2, "2 linhas por palavra");
    CHECK(tfit_lines("abc def ghi", 1000, TFIT_WORD, mono, 0) == 1, "1 linha quando cabe");
    CHECK(tfit_lines("", 50, TFIT_WORD, mono, 0) == 1, "texto vazio ocupa 1 linha");
    CHECK(tfit_lines("a\nb", 50, TFIT_WORD, mono, 0) == 2, "\\n separa linhas");
    CHECK(tfit_lines("a\n", 50, TFIT_WORD, mono, 0) == 2, "\\n no fim abre linha vazia");
    CHECK(tfit_lines("abcdefghij", 30, TFIT_CHAR, mono, 0) == 4, "10 caracteres em 3 por linha = 4 linhas");

    tfit_truncate("abcdefghij", 10, 1000, TFIT_TAIL, mono, 0, out, sizeof out);
    CHECK(!strcmp(out, "abcdefghij"), "cabe: sem reticências");
    tfit_truncate("abcdefghij", 10, 60, TFIT_TAIL, mono, 0, out, sizeof out);
    CHECK(!strcmp(out, "abcde\xE2\x80\xA6"), "fim: abcde…");
    tfit_truncate("abcdefghij", 10, 60, TFIT_HEAD, mono, 0, out, sizeof out);
    CHECK(!strcmp(out, "\xE2\x80\xA6" "fghij"), "início: …fghij");
    tfit_truncate("abcdefghij", 10, 60, TFIT_MIDDLE, mono, 0, out, sizeof out);
    CHECK(!strcmp(out, "ab\xE2\x80\xA6hij"), "meio: ab…hij");
    tfit_truncate("abc defghij", 11, 55, TFIT_TAIL, mono, 0, out, sizeof out);
    CHECK(!strcmp(out, "abc\xE2\x80\xA6"), "fim: sem espaço antes das reticências");
    tfit_truncate("abcdefghij", 10, 40, TFIT_CLIP, mono, 0, out, sizeof out);
    CHECK(!strcmp(out, "abcdefghij"), "recorte: o texto vai inteiro (quem desenha recorta)");
    tfit_truncate("ãããããã", 12, 40, TFIT_TAIL, mono, 0, out, sizeof out);
    CHECK(!strcmp(out, "ããã\xE2\x80\xA6"), "UTF-8 no truncamento");
    CHECK(mono(out, (int)strlen(out), 0) <= 40, "o resultado cabe na largura");

    printf("textfit_host: %d/%d\n", total - fails, total);
    return fails != 0;
}
