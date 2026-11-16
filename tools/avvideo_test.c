/* Teste de host de userland/libsystem/avvideo.c (MP4 -> quadro no tempo t) contra o ffmpeg. Rodado por tools/h264_test.sh.
 * uso: avvideo_test video.mp4 referencia.yuv (I420 na ordem de exibição)  → "avvideo ok ..." */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "avvideo.h"
#include "mp4demux.h"
static int cmpd(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }
int main(int argc, char **argv) {
    int err = 0;
    avv_t *v = avv_open_path(argv[1], &err);
    if (!v) { printf("avvideo FALHA abrir (%d)\n", err); return 1; }
    int w, h; double dur, fps; uint32_t ns;
    avv_info(v, &w, &h, &dur, &fps, &ns);
    FILE *f = fopen(argv[2], "rb"); fseek(f, 0, SEEK_END); long rn = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *ref = malloc(rn); if (fread(ref, 1, rn, f) != (size_t)rn) return 2; fclose(f);
    long fs = (long)w * h * 3 / 2;
    if (rn != fs * ns) { printf("avvideo FALHA referência %ld bytes para %u quadros %dx%d\n", rn, ns, w, h); return 1; }
    /* pts de exibição pelo próprio demuxer (a mesma edit list), em ordem */
    FILE *g = fopen(argv[1], "rb"); fseek(g, 0, SEEK_END); long n = ftell(g); fseek(g, 0, SEEK_SET);
    unsigned char *b = malloc(n); if (fread(b, 1, n, g) != (size_t)n) return 2; fclose(g);
    mp4_file m; mp4_open_mem(&m, b, n);
    mp4_track *t = &m.t[mp4_find_track(&m, MP4_FOURCC('v', 'i', 'd', 'e'))];
    double *pts = malloc(ns * sizeof *pts);
    for (uint32_t i = 0; i < ns; i++) pts[i] = mp4_pts(t, i);
    qsort(pts, ns, sizeof *pts, cmpd);
    int bad = 0, checks = 0;
    #define CHECKAT(tt, k) do { avv_frame fr; checks++; \
        if (avv_frame_at(v, (tt), &fr) || fr.w != w || fr.h != h) { bad++; printf("falhou em t=%.4f\n", (double)(tt)); } \
        else { const unsigned char *R = ref + (long)(k) * fs; int ok = 1; \
            for (int r = 0; r < h && ok; r++) ok = !memcmp(fr.y + r * fr.sy, R + r * w, w); \
            for (int r = 0; r < h / 2 && ok; r++) ok = !memcmp(fr.u + r * fr.suv, R + w * h + r * w / 2, w / 2) && !memcmp(fr.v + r * fr.suv, R + w * h * 5 / 4 + r * w / 2, w / 2); \
            if (!ok) { bad++; if (bad < 5) printf("quadro errado em t=%.4f (esperado %d)\n", (double)(tt), (int)(k)); } } } while (0)
    for (uint32_t k = 0; k < ns; k++) CHECKAT(pts[k], k);                                   /* cada quadro no próprio pts */
    for (uint32_t k = 0; k + 1 < ns; k++) CHECKAT((pts[k] + pts[k + 1]) / 2, k);            /* entre quadros: o anterior */
    unsigned seed = 12345;
    for (int i = 0; i < 200; i++) {                                                       /* acesso aleatório, para os dois lados */
        seed = seed * 1103515245u + 12345u;
        uint32_t k = (seed >> 8) % ns;
        double off = ((seed >> 4) & 7) / 8.0 * (k + 1 < ns ? pts[k + 1] - pts[k] : 0);
        CHECKAT(pts[k] + off, k);
    }
    CHECKAT(-1.0, 0);                                                                      /* antes do começo: o primeiro */
    CHECKAT(dur + 5, ns - 1);                                                              /* depois do fim: o último */
    /* leitura sequencial (AVAssetReader decodificado): todos os quadros, na ordem de exibição, iguais ao ffmpeg */
    {
        avv_t *q = avv_open_path(argv[1], &err);
        avv_frame fr; int k = 0, seqbad = 0;
        while (avv_next_frame(q, &fr) == 1) {
            const unsigned char *R = ref + (long)k * fs;
            for (int r = 0; r < h; r++) if (memcmp(fr.y + r * fr.sy, R + r * w, w)) { seqbad++; break; }
            if (fr.pts != pts[k]) seqbad++;
            k++;
        }
        if (seqbad || k != (int)ns) { printf("avvideo FALHA sequência: %d quadros de %u, %d errados\n", k, ns, seqbad); bad++; }
        /* amostras comprimidas (AVAssetReader sem outputSettings): todas, a primeira é chave, dts <= pts, dts crescente */
        double last_dts = -1e9; int rawbad = 0, syncs = 0;
        for (uint32_t i = 0; i < ns; i++) {
            const uint8_t *d; uint32_t sz; double p, dt, du; int sy;
            if (avv_raw_sample(q, i, &d, &sz, &p, &dt, &du, &sy) || !sz || dt > p + 1e-9 || dt <= last_dts || du <= 0) rawbad++;
            if (i == 0 && !sy) rawbad++;
            syncs += sy; last_dts = dt;
        }
        uint32_t an; const uint8_t *avcc = avv_avcc(q, &an);
        if (rawbad || !avcc || an < 7 || avcc[0] != 1) { printf("avvideo FALHA amostras comprimidas: %d problemas\n", rawbad); bad++; }
        avv_close(q);
    }
    /* BGRA, rotação e redução */
    {
        avv_frame fr;
        avv_frame_at(v, pts[ns / 2], &fr);
        uint8_t *b0 = 0, *b90 = 0, *b180 = 0, *b270 = 0, *bh = 0;
        int ow, oh, w90, h90, wh, hh, w2, h2, w3, h3;
        avv_to_bgra(&fr, 0, 0, 0, &b0, &ow, &oh);
        avv_to_bgra(&fr, 90, 0, 0, &b90, &w90, &h90);
        avv_to_bgra(&fr, 180, 0, 0, &b180, &w2, &h2);
        avv_to_bgra(&fr, 270, 0, 0, &b270, &w3, &h3);
        avv_to_bgra(&fr, 0, w / 2, h / 2, &bh, &wh, &hh);
        int mx = 0, rotbad = 0, scbad = 0;
        double kr = h >= 720 ? 0.2126 : 0.299, kb = h >= 720 ? 0.0722 : 0.114;
        for (int r = 0; r < h; r++) for (int x = 0; x < w; x++) {
            double yy = (fr.y[r * fr.sy + x] - 16) * 255.0 / 219, cb = (fr.u[(r / 2) * fr.suv + x / 2] - 128) * 255.0 / 224, cr = (fr.v[(r / 2) * fr.suv + x / 2] - 128) * 255.0 / 224;
            double R = yy + 2 * (1 - kr) * cr, B = yy + 2 * (1 - kb) * cb, G = (yy - kr * R - kb * B) / (1 - kr - kb);
            int e[3] = {(int)(B < 0 ? 0 : B > 255 ? 255 : B + 0.5), (int)(G < 0 ? 0 : G > 255 ? 255 : G + 0.5), (int)(R < 0 ? 0 : R > 255 ? 255 : R + 0.5)};
            for (int k = 0; k < 3; k++) { int d = abs(b0[(r * w + x) * 4 + k] - e[k]); if (d > mx) mx = d; }
        }
        if (w90 != h || h90 != w || w3 != h || h3 != w || w2 != w || h2 != h) rotbad = 1;
        for (int y = 0; y < h90 && !rotbad; y++) for (int x = 0; x < w90; x++) {
            if (memcmp(b90 + (y * w90 + x) * 4, b0 + ((h - 1 - x) * w + y) * 4, 4)) { rotbad = 1; break; }
            if (memcmp(b270 + (y * w90 + x) * 4, b0 + (x * w + (w - 1 - y)) * 4, 4)) { rotbad = 1; break; }
        }
        for (int y = 0; y < h && !rotbad; y++) for (int x = 0; x < w; x++)
            if (memcmp(b180 + (y * w + x) * 4, b0 + ((h - 1 - y) * w + (w - 1 - x)) * 4, 4)) { rotbad = 1; break; }
        if (wh != w / 2 || hh != h / 2) scbad = 1;
        for (int y = 0; y < hh && !scbad; y++) for (int x = 0; x < wh; x++) for (int k = 0; k < 4; k++) {
            int m = (b0[((2 * y) * w + 2 * x) * 4 + k] + b0[((2 * y) * w + 2 * x + 1) * 4 + k] + b0[((2 * y + 1) * w + 2 * x) * 4 + k] + b0[((2 * y + 1) * w + 2 * x + 1) * 4 + k] + 2) / 4;
            if (bh[(y * wh + x) * 4 + k] != m) { scbad = 1; break; }
        }
        if (mx > 1 || rotbad || scbad) { printf("avvideo FALHA BGRA dif %d, rotação %d, redução %d\n", mx, rotbad, scbad); bad++; }
        free(b0); free(b90); free(b180); free(b270); free(bh);
    }
    avv_close(v);
    if (bad) { printf("avvideo FALHA %d de %d consultas\n", bad, checks); return 1; }
    printf("avvideo ok %u quadros %dx%d, %.2f s, %.2f fps: %d consultas (sequência, entre quadros, aleatórias, bordas) iguais ao ffmpeg; leitura sequencial e amostras comprimidas; BGRA, rotação e redução\n", ns, w, h, dur, fps, checks);
    return 0;
}
