/* Decodifica um Annex B Baseline gerado pelo ffmpeg e compara com rawvideo.
 *   clang -std=c11 -O2 -I userland/third_party/h264bsd -o /tmp/h264base \
 *     tools/h264base_test.c userland/libsystem/h264bsd_glue.c e os .c do h264bsd. -lm
 */
#include "../userland/libsystem/h264sw.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static double psnr(const uint8_t *a, const uint8_t *b, int n) {
    double s = 0;
    int i;
    for (i = 0; i < n; i++) {
        double d = (int)a[i] - (int)b[i];
        s += d * d;
    }
    if (s <= 0.0) return 99.0;
    return 10.0 * log10(255.0 * 255.0 * n / s);
}

static uint8_t *slurp(const char *path, int *n) {
    FILE *f = fopen(path, "rb");
    uint8_t *b;
    long sz;
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    b = malloc((size_t)sz);
    if (!b || fread(b, 1, (size_t)sz, f) != (size_t)sz) { free(b); fclose(f); return 0; }
    fclose(f);
    *n = (int)sz;
    return b;
}

int main(int argc, char **argv) {
    const char *ann = argc > 1 ? argv[1] : "/tmp/base.264";
    const char *raw = argc > 2 ? argv[2] : "/tmp/base.yuv";
    int an = 0, rn = 0, frames = 0, w = 0, h = 0, i, sz, bad = 0;
    uint8_t *ab = slurp(ann, &an);
    uint8_t *rb = slurp(raw, &rn);
    uint8_t *yuv = 0;
    if (!ab || !rb) { fprintf(stderr, "sem arquivo\n"); return 1; }
    if (h264_bsd_decode_all(ab, an, &yuv, &frames, &w, &h) || frames < 2) {
        fprintf(stderr, "decode falhou frames=%d\n", frames);
        return 2;
    }
    sz = w * h * 3 / 2;
    if (rn < frames * sz) { fprintf(stderr, "raw curto %d\n", rn); return 3; }
    printf("frames %d %dx%d\n", frames, w, h);
    for (i = 0; i < frames; i++) {
        const uint8_t *a = yuv + (size_t)i * sz;
        const uint8_t *b = rb + (size_t)i * sz;
        double py = psnr(a, b, w * h);
        double pu = psnr(a + w * h, b + w * h, w * h / 4);
        double pv = psnr(a + w * h + w * h / 4, b + w * h + w * h / 4, w * h / 4);
        printf("f%d psnr y %.2f u %.2f v %.2f\n", i, py, pu, pv);
        if (py < 40.0 || pu < 40.0 || pv < 40.0) bad++;
    }
    free(ab); free(rb); free(yuv);
    return bad ? 4 : 0;
}
