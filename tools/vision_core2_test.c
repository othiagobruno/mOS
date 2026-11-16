/* Teste de host do userland/libsystem/vision_core2.c. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../userland/libsystem/vision_core2.h"
static int fails;
#define CHECK(c) do { if (!(c)) { printf("FALHOU %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
static void rect(uint8_t *im, int w, int x0, int y0, int x1, int y1, uint8_t v) { for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) im[y * w + x] = v; }
int main(void) {
    int W = 64, H = 48;
    uint8_t *im = malloc(W * H);
    memset(im, 230, W * H);
    rect(im, W, 5, 5, 30, 40, 20);     /* quadrado escuro com buraco claro e um ponto escuro dentro do buraco */
    rect(im, W, 10, 10, 25, 35, 230);
    rect(im, W, 15, 20, 19, 24, 20);
    rect(im, W, 40, 10, 55, 20, 20);   /* outro objeto */
    vc_contours cs;
    CHECK(vc_find_contours(im, W, H, 1, 1, &cs) == 0);
    int top = 0, holes = 0, nested = 0;
    for (int i = 0; i < cs.n; i++) {
        if (cs.c[i].parent < 0) top++;
        if (cs.c[i].is_hole) { holes++; CHECK(cs.c[i].parent >= 0 && !cs.c[cs.c[i].parent].is_hole); }
        if (!cs.c[i].is_hole && cs.c[i].parent >= 0) { nested++; CHECK(cs.c[cs.c[i].parent].is_hole); }
    }
    CHECK(cs.n == 4 && top == 2 && holes == 1 && nested == 1);
    /* a borda do quadrado: 4 cantos depois de simplificar */
    int big = -1;
    for (int i = 0; i < cs.n; i++) if (cs.c[i].parent < 0 && cs.c[i].npts > 80) big = i;
    CHECK(big >= 0);
    if (big >= 0) {
        float *o = malloc(sizeof(float) * 2 * cs.c[big].npts);
        int m = vc_simplify(cs.c[big].pts, cs.c[big].npts, 1.5f, o);
        CHECK(m == 4);
        free(o);
    }
    vc_free_contours(&cs);
    /* claro sobre escuro inverte */
    CHECK(vc_find_contours(im, W, H, 0, 1, &cs) == 0 && cs.n >= 2);
    vc_free_contours(&cs);
    /* saliência: o objeto escuro em fundo claro */
    uint8_t *s2 = malloc(W * H);
    memset(s2, 200, W * H);
    rect(s2, W, 32, 8, 48, 24, 10);
    float sal[16 * 12], box[4];
    vc_saliency(s2, W, H, sal, 16, 12, box);
    CHECK(sal[(16 / 48 * 12) * 16 + 10] > 0.9f || sal[3 * 16 + 10] > 0.9f);
    CHECK(box[0] > 0.45f && box[0] < 0.55f && box[2] > 0.2f && box[2] < 0.3f && box[1] > 0.45f && box[1] < 0.55f);
    /* translação: alvo deslocado de (+7, -3) */
    int TW = 128, TH = 96;
    uint8_t *ref = malloc(TW * TH), *tgt = malloc(TW * TH);
    srand(3);
    for (int i = 0; i < TW * TH; i++) ref[i] = (uint8_t)(rand() & 255);
    for (int y = 0; y < TH; y++) for (int x = 0; x < TW; x++) { int sx = x - 7, sy = y + 3; tgt[y * TW + x] = sx >= 0 && sy < TH ? ref[sy * TW + sx] : 0; }
    int dx = 0, dy = 0;
    CHECK(vc_translation(ref, tgt, TW, TH, &dx, &dy) == 0 && dx == -7 && dy == 3);
    /* rastreamento: modelo 8x8 achado 5 px adiante */
    uint8_t tm[64];
    for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) tm[y * 8 + x] = ref[(40 + y) * TW + 50 + x];
    int bx, by;
    float sc = vc_track(ref, TW, TH, tm, 8, 8, 45, 38, 8, &bx, &by);
    CHECK(sc > 0.99f && bx == 50 && by == 40);
    free(im); free(s2); free(ref); free(tgt);
    printf(fails ? "vision_core2: %d falhas\n" : "vision_core2: ok\n", fails);
    return fails != 0;
}
