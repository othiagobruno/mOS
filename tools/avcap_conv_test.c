/* Teste de host do userland/libsystem/avcap_conv.c. gcc -std=gnu11 -O1 tools/avcap_conv_test.c userland/libsystem/avcap_conv.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../userland/libsystem/avcap_conv.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FALHOU %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
static int near(int a, int b, int t) { return a - b <= t && b - a <= t; }

int main(void) {
    size_t bpr[3], rows[3];
    CHECK(avc_planes(AVC_FMT_BGRA, 5, 3, bpr, rows) == 1 && bpr[0] == 20 && rows[0] == 3);
    CHECK(avc_planes(AVC_FMT_2VUY, 5, 3, bpr, rows) == 1 && bpr[0] == 12);
    CHECK(avc_planes(AVC_FMT_420V, 5, 3, bpr, rows) == 2 && bpr[1] == 6 && rows[1] == 2);
    CHECK(avc_planes(AVC_FMT_Y420, 4, 4, bpr, rows) == 3 && bpr[2] == 2 && rows[2] == 2);
    CHECK(avc_planes(0x12345678, 4, 4, bpr, rows) == 0);

    /* YUY2 4x2: branco, preto, vermelho (Y 81 U 90 V 240), cinza */
    uint8_t yuy2[16] = {235, 128, 16, 128, 81, 90, 81, 240, 126, 128, 126, 128, 235, 128, 235, 128};
    uint8_t bgra[4 * 2 * 4];
    uint8_t *pl[3] = {bgra};
    size_t b4[1] = {16};
    CHECK(avc_convert(AVC_SRC_YUY2, yuy2, 16, 4, 2, AVC_FMT_BGRA, pl, b4) == 0);
    CHECK(bgra[0] == 255 && bgra[1] == 255 && bgra[2] == 255 && bgra[3] == 255);   /* branco */
    CHECK(bgra[4] == 0 && bgra[5] == 0 && bgra[6] == 0);                            /* preto */
    CHECK(near(bgra[10], 255, 3) && near(bgra[9], 0, 3) && near(bgra[8], 0, 3));      /* vermelho */
    CHECK(near(bgra[16], 128, 2) && near(bgra[17], 128, 2) && near(bgra[18], 128, 2)); /* cinza */
    CHECK(avc_convert(AVC_SRC_YUY2, yuy2, 15, 4, 2, AVC_FMT_BGRA, pl, b4) == -1);

    /* YUY2 -> 2vuy e yuvs: só reordena */
    uint8_t o2[16]; pl[0] = o2; size_t b8[1] = {8};
    CHECK(avc_convert(AVC_SRC_YUY2, yuy2, 16, 4, 2, AVC_FMT_YUVS, pl, b8) == 0 && !memcmp(o2, yuy2, 16));
    CHECK(avc_convert(AVC_SRC_YUY2, yuy2, 16, 4, 2, AVC_FMT_2VUY, pl, b8) == 0 && o2[0] == 128 && o2[1] == 235 && o2[2] == 128 && o2[3] == 16);

    /* YUY2 -> 420v: Y igual, croma média das duas linhas */
    uint8_t y[8], uv[4]; pl[0] = y; pl[1] = uv; size_t bb[2] = {4, 4};
    CHECK(avc_convert(AVC_SRC_YUY2, yuy2, 16, 4, 2, AVC_FMT_420V, pl, bb) == 0);
    CHECK(y[0] == 235 && y[1] == 16 && y[2] == 81 && y[4] == 126 && y[7] == 235);
    CHECK(uv[0] == 128 && uv[1] == 128 && uv[2] == 109 && uv[3] == 184);
    /* 420f: faixa cheia */
    CHECK(avc_convert(AVC_SRC_YUY2, yuy2, 16, 4, 2, AVC_FMT_420F, pl, bb) == 0 && y[0] == 255 && y[1] == 0);

    /* NV12 -> BGRA e ida e volta BGRA -> 420v -> BGRA com erro pequeno */
    int W = 6, H = 4;
    uint8_t *src = malloc((size_t)W * H * 4), *back = malloc((size_t)W * H * 4);
    for (int i = 0; i < W * H; i++) { src[i * 4] = (uint8_t)(40 + i * 3); src[i * 4 + 1] = 120; src[i * 4 + 2] = 200; src[i * 4 + 3] = 255; }
    /* bloco 2x2 com a mesma cor: a ida e volta em 4:2:0 preserva */
    for (int yy = 0; yy < H; yy++) for (int x = 0; x < W; x++) { uint8_t *p = src + (yy * W + x) * 4; p[0] = (uint8_t)(30 + (x / 2) * 60); p[1] = (uint8_t)(50 + (yy / 2) * 90); p[2] = 160; }
    uint8_t *nv = malloc((size_t)W * H * 3 / 2);
    pl[0] = nv; pl[1] = nv + W * H; size_t bn[2] = {(size_t)W, (size_t)W};
    CHECK(avc_convert(AVC_SRC_BGRA, src, (size_t)W * H * 4, W, H, AVC_FMT_420V, pl, bn) == 0);
    pl[0] = back; size_t bw[1] = {(size_t)W * 4};
    CHECK(avc_convert(AVC_SRC_NV12, nv, (size_t)W * H * 3 / 2, W, H, AVC_FMT_BGRA, pl, bw) == 0);
    int maxd = 0;
    for (int i = 0; i < W * H * 4; i++) { int dd = abs(src[i] - back[i]); if (dd > maxd) maxd = dd; }
    CHECK(maxd <= 4);
    /* BGRA -> BGRA copia linha a linha com bytesPerRow maior */
    uint8_t *wide = calloc((size_t)(W * 4 + 8) * H, 1); pl[0] = wide; size_t bww[1] = {(size_t)W * 4 + 8};
    CHECK(avc_convert(AVC_SRC_BGRA, src, (size_t)W * H * 4, W, H, AVC_FMT_BGRA, pl, bww) == 0 && !memcmp(wide + bww[0], src + W * 4, W * 4));
    /* y420: três planos */
    uint8_t py[24], pu[6], pv[6]; pl[0] = py; pl[1] = pu; pl[2] = pv; size_t b3[3] = {6, 3, 3};
    CHECK(avc_convert(AVC_SRC_NV12, nv, (size_t)W * H * 3 / 2, W, H, AVC_FMT_Y420, pl, b3) == 0);
    CHECK(pu[0] == nv[W * H] && pv[0] == nv[W * H + 1] && pu[4] == nv[W * H + 8] && py[7] == nv[7]);
    /* largura ímpar */
    uint8_t odd[3 * 3 * 4]; memset(odd, 200, sizeof odd);
    uint8_t oy[9], ouv[4]; pl[0] = oy; pl[1] = ouv; size_t bo[2] = {3, 4};
    CHECK(avc_convert(AVC_SRC_BGRA, odd, sizeof odd, 3, 3, AVC_FMT_420V, pl, bo) == 0 && near(oy[8], 188, 2));
    /* espelho: BGRA, yuvs (Y trocam dentro do macro-pixel) e 420v */
    uint8_t mb[3 * 4] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}; pl[0] = mb; size_t bm[1] = {12};
    avc_mirror(AVC_FMT_BGRA, 3, 1, pl, bm);
    CHECK(mb[0] == 9 && mb[4] == 5 && mb[8] == 1 && mb[11] == 4);
    uint8_t ys[8] = {10, 100, 20, 200, 30, 101, 40, 201}; pl[0] = ys; size_t bys[1] = {8};
    avc_mirror(AVC_FMT_YUVS, 4, 1, pl, bys);
    CHECK(ys[0] == 40 && ys[1] == 101 && ys[2] == 30 && ys[3] == 201 && ys[4] == 20 && ys[6] == 10 && ys[7] == 200);
    uint8_t vy[4] = {1, 2, 3, 4}, vuv[4] = {50, 60, 70, 80}; pl[0] = vy; pl[1] = vuv; size_t bv[2] = {4, 4};
    avc_mirror(AVC_FMT_420V, 4, 1, pl, bv);
    CHECK(vy[0] == 4 && vy[3] == 1 && vuv[0] == 70 && vuv[1] == 80 && vuv[2] == 50);

    /* escolha do modo pelo preset */
    avc_mode md[5] = {
        {0x32595559, 640, 480, 333333}, {0x32595559, 1280, 720, 1000000}, {0x47504a4d, 1280, 720, 333333},
        {0x47504a4d, 1920, 1080, 333333}, {0x47504a4d, 3840, 2160, 2000000},
    };
    CHECK(avc_pick_mode(md, 5, AVC_PICK_SIZE, 1280, 720) == 2);   /* mesmo tamanho: o MJPEG tem 30 qps */
    CHECK(avc_pick_mode(md, 5, AVC_PICK_SIZE, 352, 288) == 0);
    CHECK(avc_pick_mode(md, 5, AVC_PICK_HIGH, 0, 0) == 3);
    CHECK(avc_pick_mode(md, 5, AVC_PICK_PHOTO, 0, 0) == 4);
    md[1].min_interval = 333333;
    CHECK(avc_pick_mode(md, 5, AVC_PICK_SIZE, 1280, 720) == 1);   /* empate em qps: o descompactado */
    avc_mode slow[1] = {{0x32595559, 640, 480, 2000000}};
    CHECK(avc_pick_mode(slow, 1, AVC_PICK_HIGH, 0, 0) == 0 && avc_pick_mode(slow, 0, AVC_PICK_HIGH, 0, 0) == -1);
    free(src); free(back); free(nv); free(wide);
    printf(fails ? "avcap_conv: %d falhas\n" : "avcap_conv: ok\n", fails);
    return fails != 0;
}
