/* teste de host do gradient_core (NSGradient)
 * gcc -std=c11 -Wall -Wextra -Iuserland/libsystem userland/libsystem/gradient_core.c userland/tests-appkit/gradient_host.c -lm -o /tmp/grad-test && /tmp/grad-test */
#include <stdio.h>
#include <math.h>
#include "gradient_core.h"
static int ok, total;
static void check(int c, const char *m) { total++; if (c) ok++; printf("%s %s\n", c ? "  ok  " : "FALHOU", m); }
static double area(const double *p, int n) { double a = 0; for (int i = 0; i < n; i++) { int j = (i + 1) % n; a += p[i*2]*p[j*2+1] - p[j*2]*p[i*2+1]; } return fabs(a) / 2; }
int main(void) {
    gc_stops s = {3, {{1,0,0,1},{0,0,1,1},{0,1,0,1}}, {1.0, 0.0, 0.5}};
    gc_normalize(&s);
    check(s.loc[0] == 0 && s.c[0].b == 1 && s.loc[2] == 1 && s.c[2].r == 1, "ordena as paradas por posição");
    gc_color c = gc_color_at(&s, 0.25);
    check(fabs(c.b - 0.5) < 1e-9 && fabs(c.g - 0.5) < 1e-9, "interpola entre paradas");
    check(gc_color_at(&s, -1).b == 1 && gc_color_at(&s, 2).r == 1, "fora da faixa fica na ponta");
    /* faixas cobrem o retângulo inteiro sem sobrar nem faltar */
    double rect[8] = {0,0, 100,0, 100,50, 0,50}, bbox[4] = {0,0,100,50}, q[8], out[2048];
    double sum = 0;
    for (int k = 0; k < 20; k++) { gc_band(0, 0, 100, 0, k, 20, bbox, q); int n = gc_clip(rect, 4, q, 4, out, 1024); sum += area(out, n); }
    check(fabs(sum - 5000) < 1e-6, "faixas horizontais cobrem o retângulo (soma das áreas)");
    sum = 0;
    for (int k = 0; k < 30; k++) { gc_band(0, 0, 100, 50, k, 30, bbox, q); int n = gc_clip(rect, 4, q, 4, out, 1024); sum += area(out, n); }
    check(fabs(sum - 5000) < 1e-6, "faixas inclinadas cobrem o retângulo");
    /* forma côncava (L) recortada pela faixa */
    double L[12] = {0,0, 60,0, 60,20, 20,20, 20,60, 0,60}, bb2[4] = {0,0,60,60};
    sum = 0;
    for (int k = 0; k < 10; k++) { gc_band(0, 0, 0, 60, k, 10, bb2, q); int n = gc_clip(L, 6, q, 4, out, 1024); sum += area(out, n); }
    check(fabs(sum - 2000) < 1e-6, "forma côncava: área preservada");
    printf("%d/%d\n", ok, total);
    return ok == total ? 0 : 1;
}
