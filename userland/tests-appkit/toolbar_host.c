/* Teste de host do layout da NSToolbar. Roda no Linux, sem VM:
 *   gcc -std=c11 -Wall -Wextra -DCX_TBK_HOST -Iuserland/libsystem \
 *       userland/libsystem/appkit_toolbar_core.c userland/tests-appkit/toolbar_host.c \
 *       -o /tmp/appkit-toolbar-test && /tmp/appkit-toolbar-test */
#include "appkit_toolbar_core.h"
#include <stdio.h>

static int ok, total;
static void check(int c, const char *w) { total++; if (c) ok++; printf("%s %s\n", c ? "  ok  " : "  FALHA", w); }
static int near(double a, double b) { return a - b < 0.01 && b - a < 0.01; }

int main(void) {
    tbk_out o[8];
    double cx;
    tbk_item a[3] = {{40, 0, 0, 0, 0}, {0, 0, 1, 0, 0}, {60, 0, 0, 0, 0}};
    check(tbk_layout(a, 3, 300, 24, o, &cx) == 0 && cx < 0, "cabe: sem »");
    check(near(o[1].w, 200) && near(o[2].x, 240), "flexível pega a sobra e empurra o último para a direita");
    tbk_item b[3] = {{40, 140, 0, 0, 0}, {0, 0, 1, 0, 0}, {60, 0, 0, 0, 0}};
    tbk_layout(b, 3, 300, 24, o, &cx);
    check(near(o[0].w, 140) && near(o[1].w, 100), "item com máximo cresce antes do flexível");
    tbk_item c[4] = {{100, 0, 0, 0, 0}, {100, 0, 0, -1000, 0}, {100, 0, 0, 1000, 0}, {100, 0, 0, 0, 0}};
    int ov = tbk_layout(c, 4, 330, 24, o, &cx);
    check(ov && !o[1].visible && o[0].visible && o[2].visible && o[3].visible && near(cx, 306), "transborda: sai o de prioridade baixa");
    tbk_layout(c, 4, 230, 24, o, &cx);
    check(!o[1].visible && !o[3].visible && o[2].visible && o[0].visible, "mais apertado: depois o padrão mais à direita");
    tbk_layout(c, 4, 90, 24, o, &cx);
    check(!o[0].visible && !o[2].visible, "nada cabe: até o de prioridade alta sai");
    check(tbk_hit(o, 4, 10) == -1, "item oculto não recebe clique");
    tbk_item d[5] = {{40, 0, 0, 0, 0}, {0, 0, 1, 0, 0}, {80, 0, 0, 0, 1}, {0, 0, 1, 0, 0}, {40, 0, 0, 0, 0}};
    tbk_layout(d, 5, 400, 24, o, &cx);
    check(near(o[2].x, 160) && near(o[4].x, 360), "centrado no meio, vizinhos nas pontas");
    tbk_item e[3] = {{300, 0, 0, 0, 0}, {80, 0, 0, 0, 1}, {10, 0, 0, 0, 0}};
    tbk_layout(e, 3, 400, 24, o, &cx);
    check(near(o[1].x, 300) && near(o[2].x, 390), "centrado não invade o vizinho da esquerda");
    check(tbk_hit(o, 3, 305) == 1 && tbk_hit(o, 3, 395) == 2, "acerto por x");
    tbk_item f[1] = {{0, 0, 0, 0, 0}};
    tbk_layout(f, 1, 100, 24, o, &cx);
    check(near(o[0].w, 32), "sem tamanho: 32");
    printf("appkit-toolbar: %d/%d ok\n", ok, total);
    return ok == total ? 0 : 1;
}
