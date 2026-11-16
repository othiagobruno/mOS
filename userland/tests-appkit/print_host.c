/* Teste de host da paginação da impressão. Roda no Linux, sem VM:
 *   gcc -std=c11 -Wall -Wextra -DCX_PRN_HOST -Iuserland/libsystem \
 *       userland/libsystem/appkit_print_core.c userland/tests-appkit/print_host.c -lm \
 *       -o /tmp/appkit-print-test && /tmp/appkit-print-test */
#include "appkit_print_core.h"
#include <stdio.h>

static int ok, total;
static void check(int c, const char *w) { total++; if (c) ok++; printf("%s %s\n", c ? "  ok  " : "  FALHA", w); }
static int near(double a, double b) { return a - b < 0.01 && b - a < 0.01; }

int main(void) {
    prn_setup_t s = {595, 842, 72, 72, 72, 72, 1, PRN_AUTO, PRN_AUTO, 0, 0};   /* A4, margens de 1 polegada */
    prn_page_t p[64];
    int n = prn_paginate(&s, 400, 600, 1, p, 64);
    check(n == 1 && near(p[0].w, 400) && near(p[0].h, 600), "cabe numa página");
    check(near(p[0].ox, 72) && near(p[0].oy, 72 + 698 - 600), "encosta no canto de cima à esquerda da área útil");
    n = prn_paginate(&s, 400, 1500, 1, p, 64);
    check(n == 3 && near(p[0].y, 0) && near(p[1].y, 698) && near(p[2].h, 1500 - 2 * 698), "view alta: 3 páginas de cima para baixo (flipped)");
    n = prn_paginate(&s, 400, 1500, 0, p, 64);
    check(n == 3 && near(p[0].y, 1500 - 698) && near(p[2].y, 0), "view não flipped: a 1ª página é a de cima (y alto)");
    n = prn_paginate(&s, 1000, 600, 1, p, 64);
    check(n == 3 && near(p[0].w, 451) && near(p[2].x, 902) && near(p[2].w, 98), "view larga: 3 colunas");
    s.hpag = PRN_FIT;
    n = prn_paginate(&s, 1000, 600, 1, p, 64);
    check(n == 1 && near(p[0].scale, 0.451) && near(p[0].w, 1000), "FIT na largura: uma página, escala 451/1000");
    s.hpag = PRN_AUTO; s.vpag = PRN_CLIP;
    n = prn_paginate(&s, 400, 1500, 1, p, 64);
    check(n == 1 && near(p[0].h, 698), "CLIP: só a primeira página, cortada");
    s.vpag = PRN_AUTO; s.scale = 0.5;
    n = prn_paginate(&s, 400, 1500, 1, p, 64);
    check(n == 2 && near(p[0].h, 1396), "escala 50%: cabe o dobro por página");
    s.scale = 1; s.hcenter = 1; s.vcenter = 1;
    n = prn_paginate(&s, 200, 100, 1, p, 64);
    check(near(p[0].ox, 72 + (451 - 200) / 2.0) && near(p[0].oy, 72 + (698 - 100) / 2.0), "centralizado nos dois eixos");
    s.hcenter = s.vcenter = 0;
    n = prn_paginate(&s, 1000, 1500, 1, p, 64);
    check(n == 9 && near(p[1].x, 451) && near(p[3].y, 698), "grade 3×3 em ordem de linhas");
    check(prn_paginate(&s, 0, 100, 1, p, 64) == 0, "view vazia não imprime");
    check(prn_paginate(&s, 1000, 1500, 1, p, 4) == 4, "teto de páginas");
    double w, h;
    check(prn_paper_size("A4", &w, &h) && near(w, 595) && near(h, 842) && prn_paper_size("na-letter", &w, &h) && near(w, 612), "papéis");
    check(!prn_paper_size("xyz", &w, &h), "papel desconhecido");
    int a, b;
    check(prn_clamp_range(5, 1, 0, 0, &a, &b) && a == 1 && b == 5, "todas as páginas");
    check(prn_clamp_range(5, 0, 2, 9, &a, &b) && a == 2 && b == 5, "intervalo cortado no fim");
    check(!prn_clamp_range(5, 0, 4, 2, &a, &b), "intervalo invertido é vazio");
    printf("appkit-print: %d/%d ok\n", ok, total);
    return ok == total ? 0 : 1;
}
