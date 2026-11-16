/* Teste de host do layout de linhas do TextKit. Roda no Linux, sem VM:
 *   gcc -std=c11 -Wall -Wextra -Iuserland/libsystem userland/libsystem/appkit_tk_core.c \
 *       userland/tests-appkit/tk_host.c -o /tmp/appkit-tk-test && /tmp/appkit-tk-test */
#include "appkit_tk_core.h"
#include <stdio.h>
static int ok, total;
static void check(int c, const char *w) { total++; if (c) ok++; printf("%s %s\n", c ? "  ok  " : "  FALHA", w); }
static int near(double a, double b) { return a - b < 0.001 && b - a < 0.001; }
int main(void) {
    tk_para p; tk_para_default(&p);
    check(near(tk_avail_width(&p, 300, 5, 1), 290), "largura disponível desconta o padding dos dois lados");
    p.first_indent = 20; p.head_indent = 10; p.tail_indent = -30;
    check(near(tk_avail_width(&p, 300, 5, 1), 240) && near(tk_avail_width(&p, 300, 5, 0), 250), "recuos da primeira linha, das outras e da cauda");
    p.tail_indent = 200;
    check(near(tk_avail_width(&p, 300, 5, 0), 190), "cauda positiva conta da esquerda");
    tk_para_default(&p);
    tk_line L = tk_place(&p, 300, 5, 1, 1, 1, 40, 12, 4, 2, 100, 0);
    check(near(L.y, 40) && near(L.h, 18) && near(L.baseline, 12) && near(L.used_x, 5), "linha simples: altura = ascent + descent + leading");
    p.alignment = 1;
    L = tk_place(&p, 300, 5, 1, 1, 1, 0, 12, 4, 0, 100, 10);
    check(near(L.used_x, 5 + 290 - 90), "direita ignora o espaço do fim");
    p.alignment = 2;
    L = tk_place(&p, 300, 5, 1, 1, 1, 0, 12, 4, 0, 90, 0);
    check(near(L.used_x, 5 + 100), "centro");
    tk_para_default(&p);
    p.multiple = 2;
    L = tk_place(&p, 300, 5, 1, 1, 1, 0, 12, 4, 0, 50, 0);
    check(near(L.h, 32) && near(L.baseline, 28), "múltiplo: altura dobra e o extra fica acima do texto");
    tk_para_default(&p);
    p.min_h = 30; p.para_spacing = 6; p.para_spacing_before = 3; p.line_spacing = 2;
    L = tk_place(&p, 300, 5, 1, 1, 1, 0, 12, 4, 0, 50, 0);
    check(near(L.h, 3 + 30 + 6 + 2) && near(L.baseline, 3 + 14 + 12), "mínimo, espaços antes/depois do parágrafo e entre linhas");
    L = tk_place(&p, 300, 5, 0, 0, 0, 0, 12, 4, 0, 50, 0);
    check(near(L.h, 30 + 2), "linha do meio: só o espaço entre linhas");
    p.max_h = 10; p.min_h = 0;
    L = tk_place(&p, 300, 5, 0, 0, 0, 0, 12, 4, 0, 50, 0);
    check(near(L.h, 12), "máximo corta a altura");
    printf("%d/%d\n", ok, total);
    return ok == total ? 0 : 1;
}
