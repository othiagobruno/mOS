/* gcc -std=c11 -Wall -Wextra userland/tests-appkit/apps_ui_host.c -o /tmp/apps-ui-test && /tmp/apps-ui-test */
#include "../libsystem/apps_ui_geom.h"
#include <stdio.h>

static int ok, total;
static void check(int cond, const char *what) {
    total++;
    if (cond) ok++;
    printf("%s %s\n", cond ? "  ok  " : "  FALHA", what);
}

int main(void) {
    ui_rect box[8];
    int n = cx_flow_place(5, 100, 40, 10, 8, 4, 0, box, 8);
    check(n == 5, "coluna única com largura 0");
    check(box[1].x == box[0].x && box[1].y > box[0].y, "o segundo item desce");
    n = cx_flow_place(4, 100, 40, 10, 8, 0, 220, box, 8);
    check(n == 4 && box[1].x > box[0].x && box[2].y > box[0].y, "duas colunas em 220");
    double x, y, w, h;
    cx_sheet_frame(800, 500, 400, 200, &x, &y, &w, &h);
    check(x == 200 && y == 300 && w == 400 && h == 200, "folha centrada no topo");
    cx_sheet_frame(300, 100, 900, 400, &x, &y, &w, &h);
    check(w == 300 && h == 100 && x == 0 && y == 0, "folha não passa da janela");
    ui_rect parts[9];
    cx_nine_rects((ui_rect){0, 0, 100, 80}, 10, 12, parts);
    check(parts[4].w == 80 && parts[4].h == 56, "miolo esticado");
    check(parts[0].y == 68 && parts[6].y == 0, "canto de cima e de baixo");
    check(cx_depth_bits(0) == 0 && cx_depth_bits(8) == 8 && cx_depth_bits(24) == 24 && cx_depth_bits(100) == 32, "bits por profundidade");
    printf("apps-ui: %d/%d %s\n", ok, total, ok == total ? "ok" : "FALHA");
    return ok == total ? 0 : 1;
}
