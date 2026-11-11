/* Rasterizador do CGBitmapContext (userland/libsystem/cg_raster.c): áreas conhecidas.
 *   zig cc -std=c11 -Wall -Wextra -Werror -D_DEFAULT_SOURCE -Iuserland/libsystem tools/cg_raster_test.c \
 *     userland/libsystem/cg_raster.c -lm -o /tmp/cg_raster_test && /tmp/cg_raster_test
 */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "cg_raster.h"

static int fails;
#define CHECK(c, m) do { if (!(c)) { fprintf(stderr, "FAIL %s\n", m); fails++; } else printf("ok %s\n", m); } while (0)

#define W 128
static unsigned char img[W][W];
static void emit(void *ctx, int y, int x0, int x1, const uint8_t *cov) {
    (void)ctx;
    for (int x = x0; x < x1; x++) img[y][x] = cov[x - x0];
}
static double area(void) {
    double s = 0;
    for (int y = 0; y < W; y++) for (int x = 0; x < W; x++) s += img[y][x] / 255.0;
    return s;
}
static double fill(const cgr_poly *p, int eo) {
    memset(img, 0, sizeof img);
    cgr_fill(p, eo, 0, 0, W, W, emit, 0);
    return area();
}
static void rect(cgr_poly *p, double x, double y, double w, double h, int ccw) {
    cgr_move(p, x, y);
    if (ccw) { cgr_line(p, x + w, y); cgr_line(p, x + w, y + h); cgr_line(p, x, y + h); }
    else { cgr_line(p, x, y + h); cgr_line(p, x + w, y + h); cgr_line(p, x + w, y); }
    cgr_close(p);
}
static double stroke(cgr_poly *in, double w, int cap, int join, const double *dash, int nd) {
    cgr_poly out;
    cgr_stroke_t st;
    cgr_init(&out);
    memset(&st, 0, sizeof st);
    st.width = w; st.cap = cap; st.join = join; st.miter = 10; st.dash = dash; st.ndash = nd; st.tol = 0.05;
    cgr_stroke(in, &st, &out);
    double a = fill(&out, 0);
    cgr_free(&out);
    return a;
}

int main(void) {
    cgr_poly p;
    cgr_init(&p);

    rect(&p, 2, 2, 6, 6, 1);
    double a = fill(&p, 0);
    CHECK(fabs(a - 36) < 0.05 && img[4][4] == 255 && img[1][1] == 0 && img[8][8] == 0, "quadrado 6x6: área 36, dentro 255, fora 0");

    cgr_clear(&p);
    rect(&p, 10.5, 10, 4, 4, 1);
    a = fill(&p, 0);
    CHECK(fabs(a - 16) < 0.05 && img[11][10] > 120 && img[11][10] < 135 && img[11][14] > 120 && img[11][14] < 135, "meio pixel: borda com ~50% de cobertura");

    cgr_clear(&p);
    rect(&p, 20, 20, 40, 40, 1);
    rect(&p, 30, 30, 20, 20, 1);
    CHECK(fabs(fill(&p, 1) - 1200) < 0.5, "even-odd: furo no meio (1600 - 400)");
    CHECK(fabs(fill(&p, 0) - 1600) < 0.5, "nonzero, mesmo sentido: sem furo");
    cgr_clear(&p);
    rect(&p, 20, 20, 40, 40, 1);
    rect(&p, 30, 30, 20, 20, 0);
    CHECK(fabs(fill(&p, 0) - 1200) < 0.5, "nonzero, sentido oposto: furo");

    cgr_clear(&p);
    cgr_arc(&p, 64, 64, 30, 0, 2 * M_PI, 0, 0.05);
    cgr_close(&p);
    a = fill(&p, 0);
    printf("  círculo r=30: %.1f (π r² = %.1f)\n", a, M_PI * 900);
    CHECK(fabs(a - M_PI * 900) / (M_PI * 900) < 0.005, "círculo: área π r² (erro < 0,5%)");

    cgr_clear(&p);
    cgr_move(&p, 10, 20);
    cgr_cubic(&p, 10, 20, 50, 20, 50, 20, 0.05);
    a = stroke(&p, 4, 0, 0, 0, 0);
    CHECK(fabs(a - 160) < 1, "traço 40x4 butt: área 160");
    a = stroke(&p, 4, 2, 0, 0, 0);
    CHECK(fabs(a - 176) < 1, "traço square: +2 em cada ponta (176)");
    a = stroke(&p, 4, 1, 0, 0, 0);
    CHECK(fabs(a - (160 + M_PI * 4)) < 1.5, "traço round: + um círculo de raio 2");

    cgr_clear(&p);
    rect(&p, 30, 30, 20, 20, 1);
    a = stroke(&p, 2, 0, 0, 0, 0);
    CHECK(fabs(a - 160) < 1, "moldura 20x20, largura 2, miter: 22² - 18² = 160");
    a = stroke(&p, 2, 0, 2, 0, 0);
    CHECK(a < 160 && a > 157, "bevel corta os 4 cantos (meio pixel² cada)");

    cgr_clear(&p);
    cgr_move(&p, 10, 60);
    cgr_line(&p, 110, 60);
    double dash[2] = {10, 10};
    a = stroke(&p, 2, 0, 0, dash, 2);
    CHECK(fabs(a - 100) < 1, "tracejado 10/10 em 100: metade (área 100)");

    {
        double t;
        CHECK(cgr_linear_t(15, 7, 10, 0, 20, 0, 0, &t) && fabs(t - 0.5) < 1e-9, "linear: t no meio");
        CHECK(!cgr_linear_t(5, 0, 10, 0, 20, 0, 0, &t) && cgr_linear_t(5, 0, 10, 0, 20, 0, 1, &t) && t == 0, "linear: antes do início só com a flag");
        CHECK(cgr_radial_t(55, 50, 50, 50, 0, 50, 50, 10, 0, &t) && fabs(t - 0.5) < 1e-9, "radial concêntrico: raio 5 de 10");
        CHECK(!cgr_radial_t(70, 50, 50, 50, 0, 50, 50, 10, 0, &t) && cgr_radial_t(70, 50, 50, 50, 0, 50, 50, 10, 2, &t) && t == 1, "radial: fora só com a flag de depois do fim");
        double loc[2] = {0, 1}, rgba[8] = {1, 0, 0, 1, 0, 0, 1, 1};
        cgr_grad g = {2, loc, rgba};
        uint32_t c = cgr_grad_at(&g, 0.5);
        CHECK(c == 0xFF800080u, "cor no meio: vermelho + azul (0xFF800080)");
        double rgba2[8] = {1, 1, 1, 0, 1, 1, 1, 1};
        cgr_grad g2 = {2, loc, rgba2};
        CHECK(cgr_grad_at(&g2, 0.5) == 0x80808080u, "alfa 0,5 sai pré-multiplicado");
    }
    cgr_free(&p);
    if (fails) { fprintf(stderr, "cg_raster: %d falhas\n", fails); return 1; }
    printf("cg_raster: ok (0 falhas)\n");
    return 0;
}
