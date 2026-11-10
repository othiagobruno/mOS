/* Teste de host do H.264 em software. Sem QEMU.
 *   clang -std=c11 -O2 -o /tmp/vtcodec_test tools/vtcodec_test.c userland/libsystem/h264sw.c -lm
 * Confere o nosso decodificador (PSNR) e o ffmpeg como decodificador de referência.
 */
#include "../userland/libsystem/h264sw.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail;
static void fail(const char *m) { printf("FALHA %s\n", m); g_fail++; }

static double psnr(const uint8_t *a, const uint8_t *b, int n) {
    double s = 0;
    for (int i = 0; i < n; i++) { double d = (int)a[i] - (int)b[i]; s += d * d; }
    if (s <= 0.0) return 99.0;
    return 10.0 * log10(255.0 * 255.0 * n / s);
}

static void fill_bgra(uint8_t *p, int w, int h) {
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        uint8_t *c = p + (y * w + x) * 4;
        c[0] = (uint8_t)(40 + (x * 3) % 180);
        c[1] = (uint8_t)(20 + (y * 5) % 200);
        c[2] = (uint8_t)((x * 2 + y) % 255);
        c[3] = 255;
        if (x > w / 3 && x < 2 * w / 3 && y > h / 4 && y < 3 * h / 4) { c[0] = 20; c[1] = 180; c[2] = 220; }
    }
}

int main(void) {
    const int w = 64, h = 48, qp = 16;
    uint8_t *bgra = malloc((size_t)w * h * 4);
    uint8_t *y = malloc((size_t)w * h), *u = malloc((size_t)w * h / 4), *v = malloc((size_t)w * h / 4);
    uint8_t *y2 = malloc((size_t)w * h), *u2 = malloc((size_t)w * h / 4), *v2 = malloc((size_t)w * h / 4);
    uint8_t *out = malloc((size_t)w * h * 4);
    fill_bgra(bgra, w, h);
    h264_bgra_to_i420(bgra, w, h, w * 4, y, u, v, w, w / 2, w / 2);
    h264_au au;
    int erc = h264_encode_i420(y, u, v, w, h, w, w / 2, w / 2, qp, &au);
    if (erc) { printf("encode %d\n", erc); fail("encode"); return 1; }
    printf("annexb %d sample %d sps %d pps %d\n", au.annexb_n, au.sample_n, au.sps_n, au.pps_n);
    int dw = 0, dh = 0;
    int drc = h264_decode_avcc(au.sps, au.sps_n, au.pps, au.pps_n, au.sample, au.sample_n, y2, u2, v2, w, w / 2, w / 2, &dw, &dh);
    if (drc) { printf("decode %d\n", drc); fail("decode"); }
    if (dw != w || dh != h) fail("tamanho");
    double py = psnr(y, y2, w * h), pu = psnr(u, u2, w * h / 4), pv = psnr(v, v2, w * h / 4);
    printf("psnr nosso Y %.2f U %.2f V %.2f\n", py, pu, pv);
    if (py < 30 || pu < 30 || pv < 30) fail("psnr nosso");
    FILE *f = fopen("/tmp/vtcodec.264", "wb");
    if (!f || fwrite(au.annexb, 1, (size_t)au.annexb_n, f) != (size_t)au.annexb_n) fail("grava");
    if (f) fclose(f);
    int fr = system("ffmpeg -y -hide_banner -loglevel error -f h264 -i /tmp/vtcodec.264 -f rawvideo -pix_fmt yuv420p /tmp/vtcodec.yuv 2>/tmp/vtcodec.err");
    FILE *ef = fopen("/tmp/vtcodec.err", "r");
    char eline[256];
    int elog = 0;
    if (ef) {
        while (fgets(eline, sizeof eline, ef)) if (strstr(eline, "error") || strstr(eline, "Invalid")) elog = 1;
        fclose(ef);
    }
    if (fr != 0 || elog) fail("ffmpeg");
    else {
        FILE *rf = fopen("/tmp/vtcodec.yuv", "rb");
        long need = (long)w * h * 3 / 2;
        uint8_t *ref = malloc((size_t)need);
        if (!rf || !ref || fread(ref, 1, (size_t)need, rf) != (size_t)need) fail("yuv");
        else {
            double fy = psnr(y, ref, w * h);
            double fu = psnr(u, ref + w * h, w * h / 4);
            double fv = psnr(v, ref + w * h + w * h / 4, w * h / 4);
            printf("psnr ffmpeg Y %.2f U %.2f V %.2f\n", fy, fu, fv);
            if (fy < 30 || fu < 30 || fv < 30) fail("psnr ffmpeg");
        }
        if (rf) fclose(rf);
        free(ref);
    }
    h264_i420_to_bgra(y2, u2, v2, w, h, w, w / 2, w / 2, out, w * 4);
    uint8_t *y3 = malloc((size_t)w * h), *u3 = malloc((size_t)w * h / 4), *v3 = malloc((size_t)w * h / 4);
    uint8_t *nv_y = malloc((size_t)w * h), *nv_uv = malloc((size_t)w * h / 2);
    h264_i420_to_nv12(y, u, v, w, h, w, w / 2, w / 2, nv_y, w, nv_uv, w);
    h264_nv12_to_i420(nv_y, w, nv_uv, w, w, h, y3, u3, v3, w, w / 2, w / 2);
    if (memcmp(y, y3, (size_t)w * h) || memcmp(u, u3, (size_t)w * h / 4)) fail("nv12");
    else printf("ok nv12\n");
    int sw = 0, sh = 0;
    if (h264_sps_wh(au.sps, au.sps_n, &sw, &sh) || sw != w || sh != h) fail("sps");
    else printf("ok sps %dx%d\n", sw, sh);
    h264_au_free(&au);
    free(bgra); free(y); free(u); free(v); free(y2); free(u2); free(v2); free(out); free(y3); free(u3); free(v3); free(nv_y); free(nv_uv);
    printf(g_fail ? "falhas %d\n" : "ok\n", g_fail);
    return g_fail ? 1 : 0;
}
