/* Teste de host de userland/libsystem/avexport.c (núcleo do AVAssetExportSession). Monta src.mp4 pelo avwcore (vídeo
 * H.264 sintético I_PCM/P-skip, 25 fps, 2 s, IDR a cada 10 quadros + o AAC de um M4A do ffmpeg, com o priming na edit
 * list) e exporta: inteiro para out1.mov, [0,5 s, 1,3 s) para out2.mp4, só o áudio para out3.m4a, e um cancelamento.
 * uso: avexport_test DIR in.m4a → "avexport ok"; o tools/avwriter_test.sh decodifica e compara com o ffmpeg. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "avexport.h"
#include "avwcore.h"
#include "mp4mux.h"
#include "mp4demux.h"
#include "h264synth.h"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("avexport FALHA: " __VA_ARGS__); puts(""); fails++; } } while (0)
static char dir[512];
static const char *P(const char *n) { static char b[4][600]; static int i; i = (i + 1) & 3; snprintf(b[i], 600, "%s/%s", dir, n); return b[i]; }
static uint8_t *slurp(const char *p, long *n) {
    FILE *f = fopen(p, "rb"); if (!f) return 0;
    fseek(f, 0, SEEK_END); *n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc((size_t)*n); if (fread(b, 1, (size_t)*n, f) != (size_t)*n) { fclose(f); return 0; } fclose(f); return b;
}
static int exists(const char *p) { FILE *f = fopen(p, "rb"); if (f) fclose(f); return f != 0; }

static int make_src(const char *m4a, const char *name, int off) {
    long n; uint8_t *b = slurp(m4a, &n);
    mp4_file a;
    if (!b || mp4_open_mem(&a, b, (uint64_t)n)) return -1;
    mp4_track *at = &a.t[mp4_find_track(&a, MP4_FOURCC('s', 'o', 'u', 'n'))];
    avw_t *w = avw_open(P(name), MP4W_MP4, 0);
    int iv = avw_add_input(w, AVW_VIDEO, 0, 0), ia = avw_add_input(w, AVW_AUDIO, 0, 0);
    static uint8_t sps[64], pps[16], cfg[128], out[1 << 16];
    uint32_t sn, pn;
    hs_params(64, 48, sps, &sn, pps, &pn);
    avw_set_h264(w, iv, 64, 48, cfg, hs_avcc(cfg, sps, sn, pps, pn));
    avw_set_aac(w, ia, (int)at->sample_rate, (int)at->channels, at->cfg, at->cfg_len, 0, 0);
    avw_time z = {0, 1};
    avw_start_session(w, z);
    int64_t E = at->edit_media_time > 0 ? at->edit_media_time : 0;
    uint32_t ai = 0;
    static uint8_t y[64 * 48], u[32 * 24], v[32 * 24];
    for (int k = 0; k < 50; k++) {
        /* áudio até o tempo deste quadro */
        while (ai < at->nsamples && (double)(at->s[ai].dts - E) / at->timescale < k * 0.04) {
            uint8_t pk[8192];
            mp4_read_sample(&a, at, ai, pk);
            avw_time p = {at->s[ai].dts - E, (int32_t)at->timescale}, none = {0, 0};
            if (avw_append(w, ia, pk, at->s[ai].size, p, none, none, at->s[ai].dur, 1)) return -2;
            ai++;
        }
        uint32_t len;
        if (k % 10 == 0) {
            for (int i = 0; i < 64 * 48; i++) y[i] = (uint8_t)(16 + (i * 3 + k * 17 + off) % 200);
            for (int i = 0; i < 32 * 24; i++) { u[i] = (uint8_t)(60 + (i + k * 5) % 120); v[i] = (uint8_t)(70 + (i * 2 + k) % 110); }
            len = hs_idr(out, 64, 48, y, u, v, k / 10 & 1);
        } else len = hs_pskip(out, 64, 48, k % 10);
        avw_time p = {k, 25}, d = {1, 25};
        if (avw_append(w, iv, out, len, p, p, d, 0, k % 10 == 0)) return -3;
    }
    while (ai < at->nsamples) {
        uint8_t pk[8192];
        mp4_read_sample(&a, at, ai, pk);
        avw_time p = {at->s[ai].dts - E, (int32_t)at->timescale}, none = {0, 0};
        if (avw_append(w, ia, pk, at->s[ai].size, p, none, none, at->s[ai].dur, 1)) return -2;
        ai++;
    }
    return avw_finish(w);
}

int main(int argc, char **argv) {
    if (argc < 3) return 2;
    snprintf(dir, sizeof dir, "%s", argv[1]);
    CHECK(make_src(argv[2], "src.mp4", 0) == 0 && make_src(argv[2], "src2.mp4", 77) == 0, "origens");
    avx_info inf;
    CHECK(avx_probe(P("src.mp4"), &inf) == 0 && inf.has_video && inf.has_audio && inf.video_copyable && inf.audio_copyable && inf.audio_is_aac &&
          inf.width == 64 && inf.height == 48, "probe");
    const char *why = 0;
    avx_ctl ctl = {0, 0};
    int rc = avx_passthrough(P("src.mp4"), P("out1.mov"), MP4W_MOV, 0, 0, -1, AVX_VIDEO | AVX_AUDIO, &ctl, &why);
    CHECK(rc == 0 && ctl.progress == 1, "inteiro: %d %s", rc, why ? why : "");
    rc = avx_passthrough(P("src.mp4"), P("out2.mp4"), MP4W_MP4, 1, 0.5, 0.8, AVX_VIDEO | AVX_AUDIO, 0, &why);
    CHECK(rc == 0, "cortado: %d %s", rc, why ? why : "");
    rc = avx_passthrough(P("src.mp4"), P("out3.m4a"), MP4W_M4A, 0, 0, -1, AVX_VIDEO | AVX_AUDIO, 0, &why);
    CHECK(rc == 0, "M4A: %d %s", rc, why ? why : "");
    /* o corte começa no quadro-chave de 0,4 s (amostra 10) e acaba no último quadro antes de 1,3 s (32) */
    long n; uint8_t *b = slurp(P("out2.mp4"), &n);
    mp4_file m;
    CHECK(b && !mp4_open_mem(&m, b, (uint64_t)n), "out2 legível");
    if (b) {
        int tv = mp4_find_track(&m, MP4_FOURCC('v', 'i', 'd', 'e'));
        CHECK(tv >= 0 && m.t[tv].nsamples == 23 && m.t[tv].s[0].sync, "out2: %u quadros (esperado 23: 10..32)", tv >= 0 ? m.t[tv].nsamples : 0);
        CHECK(m.duration == 800, "out2: duração %llu ms", (unsigned long long)m.duration);
        mp4_close(&m);
    }
    ctl.cancel = 1;
    rc = avx_passthrough(P("src.mp4"), P("out4.mp4"), MP4W_MP4, 0, 0, -1, AVX_VIDEO | AVX_AUDIO, &ctl, &why);
    CHECK(rc == AVX_ERR_CANCEL && !exists(P("out4.mp4")), "cancelamento: %d", rc);
    rc = avx_passthrough(P("src.mp4"), P("out5.mp4"), MP4W_MP4, 0, 5, 1, AVX_VIDEO | AVX_AUDIO, 0, &why);
    CHECK(rc == AVX_ERR_EMPTY && !exists(P("out5.mp4")), "intervalo depois do fim: %d", rc);
    /* composição: vídeo = A[0,4 s, 0,8 s) + B[0,2 s, 0,6 s); áudio = A[0, 0,8 s) */
    {
        char a[600], b2[600];
        snprintf(a, sizeof a, "%s", P("src.mp4")); snprintf(b2, sizeof b2, "%s", P("src2.mp4"));
        int vi = 0, ai = 0;   /* índices das faixas na origem (o avwcore cria a faixa na 1ª amostra: o áudio vem antes) */
        { long sn; uint8_t *sb = slurp(P("src.mp4"), &sn); mp4_file sm;
          if (sb && !mp4_open_mem(&sm, sb, (uint64_t)sn)) { vi = mp4_find_track(&sm, MP4_FOURCC('v', 'i', 'd', 'e')); ai = mp4_find_track(&sm, MP4_FOURCC('s', 'o', 'u', 'n')); mp4_close(&sm); } }
        avx_segment vs[2] = {{a, vi, 0.4, 0.4, 0}, {b2, vi, 0.2, 0.4, 0.4}}, as[1] = {{a, ai, 0, 0.8, 0}};
        avx_ctrack ct[2] = {{AVX_VIDEO, vs, 2}, {AVX_AUDIO, as, 1}};
        rc = avx_compose(P("comp.mp4"), MP4W_MP4, 0, ct, 2, 0, &why);
        CHECK(rc == 0, "composição: %d %s", rc, why ? why : "");
        long cn; uint8_t *cb = slurp(P("comp.mp4"), &cn);
        mp4_file cm;
        CHECK(cb && !mp4_open_mem(&cm, cb, (uint64_t)cn) && cm.duration == 800 && cm.ntracks == 2, "composição legível, 0,8 s");
        avx_segment bad[1] = {{a, vi, 0, 0.2, 0}};
        avx_ctrack wrong[1] = {{AVX_AUDIO, bad, 1}};
        CHECK(avx_compose(P("bad.mp4"), MP4W_MP4, 0, wrong, 1, 0, &why) == AVX_ERR_CODEC && !exists(P("bad.mp4")), "faixa de vídeo pedida como áudio é recusada");
    }
    if (!fails) puts("avexport ok");
    return fails != 0;
}
