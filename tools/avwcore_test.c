/* Teste de host do avwcore.c + mp4mux.c (núcleo do AVAssetWriter). Gera H.264 de verdade aqui mesmo (IDR com
 * macroblocos I_PCM e quadros P só de skip: a imagem decodificada é conhecida, sem codificador nem ffmpeg gerando vídeo)
 * e LPCM, grava os casos abaixo em DIR e escreve o esperado para o tools/avwriter_test.sh comparar com o ffprobe/ffmpeg.
 *  a.mov: LPCM desde o começo da sessão + vídeo que começa 0,5 s depois (faixa criada depois das amostras de áudio),
 *         rotação de 90° na matriz; esperado: a_v.txt (pacotes), a.yuv, a.pcm
 *  b.mp4: vídeo em ns (timescale automático 90000), sessão [1,2 s, 1,6 s) dentro do vídeo: corte e limite na edit list
 *  d.mp4: quadros B (pts fora de ordem, dts crescente): ctts e edit list pelo menor pts; esperado d_v.txt
 *  erros: LPCM em MP4, vídeo em M4A, dts que não cresce, amostra antes da sessão.
 * uso: avwcore_test DIR → "avwcore ok" e os arquivos */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "avwcore.h"
#include "mp4mux.h"
#include "mp4demux.h"
#include "h264synth.h"

#define W 64
#define H 48
static uint8_t sps[64], pps[16];
static uint32_t sps_n, pps_n;
static void make_params(void) { hs_params(W, H, sps, &sps_n, pps, &pps_n); }
static uint32_t avcc(uint8_t *o) { return hs_avcc(o, sps, sps_n, pps, pps_n); }
static uint8_t px_y(int x, int y, int k) { return (uint8_t)(16 + (x * 3 + y * 5 + k * 11) % 200); }
static uint8_t px_u(int x, int y, int k) { (void)y; return (uint8_t)(64 + (x + k * 3) % 128); }
static uint8_t px_v(int x, int y, int k) { (void)x; return (uint8_t)(64 + (y * 2 + k) % 128); }
/* IDR com todos os macroblocos I_PCM mostrando o padrão k; P só de skip */
static uint32_t idr(uint8_t *out, int k, int idr_id) {
    static uint8_t y[W * H], u[W * H / 4], v[W * H / 4];
    for (int r = 0; r < H; r++) for (int c = 0; c < W; c++) y[r * W + c] = px_y(c, r, k);
    for (int r = 0; r < H / 2; r++) for (int c = 0; c < W / 2; c++) { u[r * W / 2 + c] = px_u(c, r, k); v[r * W / 2 + c] = px_v(c, r, k); }
    return hs_idr(out, W, H, y, u, v, idr_id);
}
static uint32_t pskip(uint8_t *out, int frame_num) { return hs_pskip(out, W, H, frame_num); }
static void yuv_frame(FILE *f, int k) {
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) fputc(px_y(x, y, k), f);
    for (int y = 0; y < H / 2; y++) for (int x = 0; x < W / 2; x++) fputc(px_u(x, y, k), f);
    for (int y = 0; y < H / 2; y++) for (int x = 0; x < W / 2; x++) fputc(px_v(x, y, k), f);
}
static avw_time T(int64_t v, int32_t ts) { return (avw_time){v, ts}; }
static const avw_time NONE = {0, 0};
static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("avwcore FALHA: " __VA_ARGS__); puts(""); fails++; } } while (0)
static char dir[512];
static const char *P(const char *n) { static char b[4][600]; static int i; i = (i + 1) & 3; snprintf(b[i], 600, "%s/%s", dir, n); return b[i]; }
static uint8_t *slurp(const char *p, long *n) {
    FILE *f = fopen(p, "rb"); if (!f) return 0;
    fseek(f, 0, SEEK_END); *n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc((size_t)*n); if (fread(b, 1, (size_t)*n, f) != (size_t)*n) { fclose(f); return 0; } fclose(f); return b;
}

static void case_a(void) {
    static uint8_t buf[1 << 16], cfg[128];
    avw_t *w = avw_open(P("a.mov"), MP4W_MOV, 0);
    CHECK(w, "a: abrir");
    if (!w) return;
    int32_t rot[9] = {0, 0x10000, 0, -0x10000, 0, 0, H << 16, 0, 0x40000000};
    int ia = avw_add_input(w, AVW_AUDIO, 0, 0), iv = avw_add_input(w, AVW_VIDEO, 0, rot);
    CHECK(ia == 0 && iv == 1, "a: entradas %d %d", ia, iv);
    CHECK(avw_set_lpcm(w, ia, 44100, 2, 16) == 0, "a: lpcm");
    CHECK(avw_append(w, ia, (uint8_t *)"\0\0\0\0", 4, T(22050, 44100), NONE, NONE, 1, 1) == AVW_ERR_SESSION, "a: amostra antes da sessão");
    CHECK(avw_start_session(w, T(300, 600)) == 0, "a: sessão");
    FILE *pcm = fopen(P("a.pcm"), "wb");
    int16_t blk[1024 * 2];
    int64_t at = 22050;   /* 0,5 s */
    for (int b = 0; b < 43; b++) {
        for (int i = 0; i < 1024; i++) {
            double t = (double)(b * 1024 + i) / 44100;
            blk[2 * i] = (int16_t)(12000 * sin(2 * M_PI * 440 * t));
            blk[2 * i + 1] = (int16_t)(9000 * sin(2 * M_PI * 1000 * t));
        }
        fwrite(blk, 1, sizeof blk, pcm);
        CHECK(avw_append(w, ia, (uint8_t *)blk, sizeof blk, T(at, 44100), NONE, NONE, 1024, 1) == 0, "a: áudio %d", b);
        at += 1024;
        if (b == 2) {   /* o formato do vídeo só chega agora (como o avcC do codificador) */
            CHECK(avw_set_h264(w, iv, W, H, cfg, avcc(cfg)) == 0, "a: avcC tardio");
        }
    }
    fclose(pcm);
    FILE *yuv = fopen(P("a.yuv"), "wb"), *ex = fopen(P("a_v.txt"), "w");
    int last_idr = 0;
    for (int k = 0; k < 25; k++) {
        uint32_t n = k % 10 == 0 ? idr(buf, k, k / 10 % 2) : pskip(buf, k % 10);
        if (k % 10 == 0) last_idr = k;
        int64_t pts = 600 + 20 * k;   /* 1,0 s: 0,5 s depois do começo da sessão */
        CHECK(avw_append(w, iv, buf, n, T(pts, 600), T(pts, 600), T(20, 600), 0, k % 10 == 0) == 0, "a: vídeo %d", k);
        yuv_frame(yuv, last_idr);
        fprintf(ex, "%lld %lld %u %d\n", (long long)(300 + 20 * k), (long long)(300 + 20 * k), n, k % 10 == 0);
    }
    fclose(yuv); fclose(ex);
    CHECK(avw_append(w, iv, buf, 10, T(600, 600), T(600, 600), NONE, 0, 0) == AVW_ERR_ORDER, "a: dts repetido aceito");
    CHECK(avw_finish(w) == 0, "a: fechar");
    /* o próprio demuxer do mOS: vazio de 0,5 s no vídeo, matriz, LPCM quadro a quadro */
    long n; uint8_t *b = slurp(P("a.mov"), &n);
    mp4_file f;
    CHECK(b && !mp4_open_mem(&f, b, (uint64_t)n), "a: mp4demux");
    if (!b) return;
    int tv = mp4_find_track(&f, MP4_FOURCC('v', 'i', 'd', 'e')), ta = mp4_find_track(&f, MP4_FOURCC('s', 'o', 'u', 'n'));
    CHECK(tv >= 0 && ta >= 0, "a: faixas");
    if (tv >= 0 && ta >= 0) {
        mp4_track *v = &f.t[tv], *a = &f.t[ta];
        CHECK(v->edit_empty == 500 && v->edit_media_time == 0, "a: edit do vídeo vazio=%llu media=%lld", (unsigned long long)v->edit_empty, (long long)v->edit_media_time);
        CHECK(v->matrix[1] == 0x10000 && v->matrix[3] == -0x10000, "a: matriz");
        CHECK(v->nsamples == 25 && v->timescale == 600 && v->s[24].dur == 20, "a: amostras de vídeo %u ts %u", v->nsamples, v->timescale);
        CHECK(a->duration == 43 * 1024 && a->codec == MP4_FOURCC('s', 'o', 'w', 't'), "a: LPCM %llu quadros", (unsigned long long)a->duration);
        CHECK(f.duration == 1333, "a: duração do filme %llu (esperado 0,5 s vazio + 25 quadros de 20/600 = 1333 ms)", (unsigned long long)f.duration);
        mp4_close(&f);
    }
    free(b);
}

static void case_b(void) {
    static uint8_t buf[1 << 16], cfg[128];
    avw_t *w = avw_open(P("b.mp4"), MP4W_MP4, 1);
    CHECK(w, "b: abrir");
    if (!w) return;
    int iv = avw_add_input(w, AVW_VIDEO, 0, 0);
    avw_set_h264(w, iv, W, H, cfg, avcc(cfg));
    CHECK(avw_start_session(w, T(1200000000, 1000000000)) == 0, "b: sessão");
    for (int k = 0; k < 30; k++) {
        uint32_t n = k % 10 == 0 ? idr(buf, k, k / 10 % 2) : pskip(buf, k % 10);
        int64_t pts = 1000000000 + (int64_t)k * 1000000000 / 30;
        CHECK(avw_append(w, iv, buf, n, T(pts, 1000000000), NONE, NONE, 0, k % 10 == 0) == 0, "b: vídeo %d", k);
    }
    CHECK(avw_end_session(w, T(1600000000, 1000000000)) == 0, "b: fim da sessão");
    CHECK(avw_finish(w) == 0, "b: fechar");
    long n; uint8_t *b = slurp(P("b.mp4"), &n);
    mp4_file f;
    CHECK(b && !mp4_open_mem(&f, b, (uint64_t)n), "b: mp4demux");
    if (!b) return;
    mp4_track *v = &f.t[0];
    CHECK(v->timescale == 90000 && v->nsamples == 30 && v->s[0].dur == 3000 && v->s[29].dur == 3000, "b: ts %u, %u amostras, dur %u", v->timescale, v->nsamples, v->s[0].dur);
    CHECK(v->edit_media_time == 18000 && v->edit_empty == 0, "b: corte %lld", (long long)v->edit_media_time);
    CHECK(f.duration == 400, "b: duração %llu (esperado 400 ms)", (unsigned long long)f.duration);
    CHECK(b[4] == 'f' && b[36] == 'm' && b[37] == 'o', "b: faststart (moov antes do mdat)");
    mp4_close(&f);
    free(b);
}

static void case_d(void) {
    static uint8_t buf[1 << 16], cfg[128];
    avw_t *w = avw_open(P("d.mp4"), MP4W_MP4, 0);
    int iv = avw_add_input(w, AVW_VIDEO, 0, 0);
    avw_set_h264(w, iv, W, H, cfg, avcc(cfg));
    avw_start_session(w, T(1020, 600));
    static const int disp[10] = {0, 3, 1, 2, 6, 4, 5, 9, 7, 8};
    FILE *ex = fopen(P("d_v.txt"), "w");
    for (int k = 0; k < 10; k++) {
        uint32_t n = k == 0 ? idr(buf, 0, 0) : pskip(buf, k);
        int64_t dts = 1000 + 20 * k, pts = 1000 + 20 * (disp[k] + 1);
        CHECK(avw_append(w, iv, buf, n, T(pts, 600), T(dts, 600), T(20, 600), 0, k == 0) == 0, "d: vídeo %d", k);
        fprintf(ex, "%d %d %u %d\n", 20 * disp[k], 20 * k - 20, n, k == 0);
    }
    fclose(ex);
    CHECK(avw_finish(w) == 0, "d: fechar");
}

static void erros(void) {
    avw_t *w = avw_open(P("e.mp4"), MP4W_MP4, 0);
    int ia = avw_add_input(w, AVW_AUDIO, 0, 0);
    CHECK(avw_set_lpcm(w, ia, 44100, 2, 16) == AVW_ERR_FORMAT, "e: LPCM em MP4 aceito");
    avw_cancel(w);
    FILE *f = fopen(P("e.mp4"), "rb");
    CHECK(!f, "e: cancel deixou o arquivo");
    if (f) fclose(f);
    w = avw_open(P("e.m4a"), MP4W_M4A, 0);
    CHECK(avw_add_input(w, AVW_VIDEO, 0, 0) < 0, "e: vídeo em M4A aceito");
    avw_cancel(w);
}

int main(int argc, char **argv) {
    if (argc < 2) return 2;
    snprintf(dir, sizeof dir, "%s", argv[1]);
    make_params();
    case_a();
    case_b();
    case_d();
    erros();
    if (!fails) puts("avwcore ok");
    return fails != 0;
}
