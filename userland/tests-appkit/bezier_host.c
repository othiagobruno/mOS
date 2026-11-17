/* teste de host do bezier_core (NSBezierPath)
 * gcc -std=c11 -Wall -Wextra -Iuserland/libsystem userland/libsystem/bezier_core.c userland/tests-appkit/bezier_host.c -lm -o /tmp/bz-test && /tmp/bz-test */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "bezier_core.h"
void *bz_alloc(unsigned long n) { return malloc(n); }
void *bz_realloc(void *p, unsigned long n) { return realloc(p, n); }
void bz_free(void *p) { free(p); }
static int ok, total;
static void check(int c, const char *m) { total++; if (c) ok++; printf("%s %s\n", c ? "  ok  " : "FALHOU", m); }
static int near(double a, double b, double e) { return fabs(a - b) < e; }
/* soma de voltas do polígono único (como o window server, regra não-zero) */
static int poly_wind(const float *o, int n, double x, double y) {
    int w = 0;
    for (int i = 0; i < n; i++) {
        double x0 = o[i * 2], y0 = o[i * 2 + 1], x1 = o[((i + 1) % n) * 2], y1 = o[((i + 1) % n) * 2 + 1];
        if (y0 <= y) { if (y1 > y && (x1 - x0) * (y - y0) - (x - x0) * (y1 - y0) > 0) w++; }
        else if (y1 <= y && (x1 - x0) * (y - y0) - (x - x0) * (y1 - y0) < 0) w--;
    }
    return w;
}
int main(void) {
    bz_path p = {0, 0, 0}, q = {0, 0, 0};
    bz_rect_path(&p, (bz_rect){0, 0, 10, 10});
    check(p.n == 5 && p.el[0].kind == BZ_MOVE && p.el[4].kind == BZ_CLOSE, "retângulo: move, 3 linhas, close");
    double x, y;
    check(bz_current(&p, &x, &y) && x == 0 && y == 0, "ponto atual depois de close = começo");
    check(bz_contains(&p, 5, 5, 0) && !bz_contains(&p, 15, 5, 0), "contém");
    /* buraco: anel interno no mesmo sentido */
    bz_rect_path(&p, (bz_rect){3, 3, 4, 4});
    check(bz_contains(&p, 5, 5, 1) == 0 && bz_contains(&p, 5, 5, 0) == 1 && bz_contains(&p, 1, 1, 1), "par-ímpar faz buraco; não-zero não");
    bz_flat f = {0, 0, 0, 0, 0, 0, 0};
    bz_flatten(&p, 0.25, &f);
    float *o;
    int n = bz_fill_polygon(&f, 1, &o);
    check(n > 0 && poly_wind(o, n, 5, 5) == 0 && poly_wind(o, n, 1, 1) != 0 && poly_wind(o, n, 20, 20) == 0, "polígono único par-ímpar com buraco");
    free(o);
    n = bz_fill_polygon(&f, 0, &o);
    check(poly_wind(o, n, 5, 5) != 0, "polígono único não-zero sem buraco");
    free(o);
    bz_flat_release(&f);
    bz_clear(&p);
    bz_oval(&p, (bz_rect){0, 0, 20, 10});
    bz_rect b = bz_bounds(&p);
    check(near(b.x, 0, 0.05) && near(b.w, 20, 0.05) && near(b.h, 10, 0.05), "oval: caixa");
    check(bz_contains(&p, 10, 5, 0) && !bz_contains(&p, 0.5, 0.5, 0), "oval: contém o centro, não o canto");
    bz_clear(&p);
    bz_arc(&p, 0, 0, 10, 0, 90, 0);
    check(bz_current(&p, &x, &y) && near(x, 0, 1e-9) && near(y, 10, 1e-9), "arco anti-horário 0→90 termina em (0,10)");
    bz_clear(&p);
    bz_arc(&p, 0, 0, 10, 0, 90, 1);
    check(bz_current(&p, &x, &y) && near(x, 0, 1e-9) && near(y, 10, 1e-9) && p.n == 4, "arco horário 0→90 dá a volta de 270° (3 curvas)");
    bz_clear(&p);
    bz_move(&p, 0, 0);
    bz_arc_to(&p, 10, 0, 10, 10, 2);
    check(bz_current(&p, &x, &y) && near(x, 10, 1e-6) && near(y, 2, 1e-6), "arco tangente termina na segunda reta");
    bz_clear(&p);
    bz_move(&p, 0, 0); bz_line(&p, 10, 0); bz_curve(&p, 12, 0, 12, 5, 10, 5);
    bz_reverse(&q, &p);
    check(q.n == 3 && q.el[0].p[0] == 10 && q.el[0].p[1] == 5 && q.el[1].kind == BZ_CURVE && q.el[1].p[0] == 12 && q.el[1].p[1] == 5 &&
          q.el[2].p[0] == 0 && q.el[2].p[1] == 0, "inverte (curva com controles trocados)");
    double m[6] = {2, 0, 0, 2, 1, 1};
    bz_transform(&p, m);
    check(p.el[1].p[0] == 21 && p.el[1].p[1] == 1, "transformação afim");
    b = bz_control_bounds(&p);
    check(b.x == 1 && b.w == 24 && b.h == 10, "caixa dos pontos de controle");
    /* tracejado de uma reta de 10 com 2 ligado / 3 desligado: [0,2] [5,7] */
    bz_clear(&p);
    bz_move(&p, 0, 0); bz_line(&p, 10, 0);
    bz_flatten(&p, 0.25, &f);
    struct { int n; double a[8]; } got = {0, {0}};
    void emit(void *c, const double *xy, int k);
    double pat[2] = {2, 3};
    bz_dash(&f, pat, 2, 0, emit, &got);
    check(got.n == 2 && near(got.a[0], 0, 1e-9) && near(got.a[1], 2, 1e-9) && near(got.a[2], 5, 1e-9) && near(got.a[3], 7, 1e-9), "tracejado 2/3");
    got.n = 0;
    bz_dash(&f, pat, 2, 1, emit, &got);
    check(got.n == 3 && near(got.a[1], 1, 1e-9) && near(got.a[2], 4, 1e-9), "tracejado com fase");
    bz_flat_release(&f);
    bz_release(&p); bz_release(&q);
    printf("%d/%d\n", ok, total);
    return ok == total ? 0 : 1;
}
void emit(void *c, const double *xy, int k) {
    struct { int n; double a[8]; } *g = c;
    if (g->n < 4) { g->a[g->n * 2] = xy[0]; g->a[g->n * 2 + 1] = xy[(k - 1) * 2]; }
    g->n++;
}
