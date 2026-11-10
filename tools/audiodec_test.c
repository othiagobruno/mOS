/* Teste de host dos decodificadores. Gera os arquivos com o ffmpeg e compara.
 * Não é teste de VM e não exercita microfone de hardware.
 *   clang -std=c11 -O1 -o /tmp/audiodec_test tools/audiodec_test.c userland/host-audio/audiodec.c -lm
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

typedef struct {
    int kind, status;
    const char *limit;
    int rate, channels, frames;
    short *pcm;
} cx_audiodec_out;
int cx_audiodec(const void *b, unsigned long long n, cx_audiodec_out *o);
void cx_audiodec_free(cx_audiodec_out *o);
int cx_media_tcc(const char *service);
int cx_pcm_s16_resample(const short *src, int nsrc, int src_rate, short *dst, int ndst, int dst_rate, int ch);

static int g_fail;
static void expect_i(const char *w, int got, int exp) {
    if (got != exp) { printf("FALHA %s: obtido %d esperado %d\n", w, got, exp); g_fail++; }
    else printf("ok %s = %d\n", w, got);
}
static unsigned char *loadf(const char *p, unsigned long long *n) {
    FILE *f = fopen(p, "rb");
    if (!f) { printf("FALHA não abriu %s\n", p); g_fail++; return 0; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *b = malloc((size_t)sz);
    if (!b || fread(b, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); free(b); g_fail++; return 0; }
    fclose(f); *n = (unsigned long long)sz; return b;
}
static int sh(const char *c) { int r = system(c); if (r) { printf("FALHA %s\n", c); g_fail++; } return r == 0; }

static void cmp_pcm(const char *tag, const short *got, int ng, const char *refpath, int ch) {
    unsigned long long n = 0;
    unsigned char *rb = loadf(refpath, &n);
    if (!rb) return;
    int nr = (int)(n / 2);
    int nf = ng * ch;
    int m = nf < nr ? nf : nr;
    int diff = 0, max = 0;
    for (int i = 0; i < m; i++) {
        int d = (int)got[i] - (int)((short *)rb)[i];
        if (d < 0) d = -d;
        if (d) diff++;
        if (d > max) max = d;
    }
    printf("%s amostras %d ref %d diffs %d max %d\n", tag, nf, nr, diff, max);
    if (max != 0 || nf != nr) { printf("FALHA %s não bate com o ffmpeg\n", tag); g_fail++; }
    else printf("ok %s bit-exato\n", tag);
    free(rb);
}
/* MP3 é com perda e o lame atrasa o sinal em cerca de dois quadros.
 * A comparação é a correlação no meio do sinal, depois de achar o atraso. */
static void cmp_mp3(const short *got, int frames, int ch, const char *refpath) {
    unsigned long long n = 0;
    unsigned char *rb = loadf(refpath, &n);
    if (!rb) return;
    const short *ref = (const short *)rb;
    int nr = (int)(n / 2 / (unsigned)(ch ? ch : 1));
    int start = frames / 4;
    int len = frames / 2;
    if (len > 4000) len = 4000;
    if (start + len > frames) len = frames - start;
    double best = -2; int best_at = 0;
    for (int off = -4000; off <= 4000; off++) {
        double dot = 0, eg = 0, er = 0;
        for (int i = start; i < start + len; i++) {
            int ri = i + off;
            if (ri < 0 || ri >= nr) continue;
            double g = got[i * ch], r = ref[ri * ch];
            dot += g * r; eg += g * g; er += r * r;
        }
        double c = (eg > 1 && er > 1) ? dot / sqrt(eg * er) : -2;
        if (c > best) { best = c; best_at = off; }
    }
    long abserr = 0; int used = 0; int peak = 0;
    for (int i = start; i < start + len; i++) {
        int ri = i + best_at;
        if (ri < 0 || ri >= nr) continue;
        int d = (int)got[i * ch] - (int)ref[ri * ch];
        if (d < 0) d = -d;
        abserr += d; used++;
        if (d > peak) peak = d;
    }
    printf("mp3 atraso %d correlação %g erro médio %ld pico %d em %d amostras\n",
           best_at, best, used ? abserr / used : 0, peak, used);
    if (best < 0.98) { printf("FALHA mp3 correlação baixa\n"); g_fail++; }
    else printf("ok mp3\n");
    free(rb);
}

int main(void) {
    system("mkdir -p /tmp/airtry/media");
    if (!sh("ffmpeg -y -f lavfi -i sine=frequency=440:sample_rate=44100:duration=0.25 -c:a libmp3lame -q:a 4 /tmp/airtry/media/t.mp3 -loglevel error") ||
        !sh("ffmpeg -y -i /tmp/airtry/media/t.mp3 -f s16le /tmp/airtry/media/t.mp3.pcm -loglevel error") ||
        !sh("ffmpeg -y -f lavfi -i sine=frequency=440:sample_rate=44100:duration=0.2 -c:a alac -sample_fmt s16p /tmp/airtry/media/t.caf -loglevel error") ||
        !sh("ffmpeg -y -i /tmp/airtry/media/t.caf -f s16le /tmp/airtry/media/t.caf.pcm -loglevel error") ||
        !sh("ffmpeg -y -f lavfi -i sine=frequency=440:sample_rate=44100:duration=0.1 -c:a alac -sample_fmt s16p -f ipod /tmp/airtry/media/t.m4a -loglevel error") ||
        !sh("ffmpeg -y -i /tmp/airtry/media/t.m4a -f s16le /tmp/airtry/media/t.m4a.pcm -loglevel error") ||
        !sh("ffmpeg -y -f lavfi -i sine=frequency=440:sample_rate=48000:duration=0.1 -c:a aac /tmp/airtry/media/t.aac -loglevel error") ||
        !sh("ffmpeg -y -f lavfi -i sine=frequency=440:sample_rate=48000:duration=0.1 -c:a libopus /tmp/airtry/media/t.opus -loglevel error")) {
        printf("FALHOU (ffmpeg, %d)\n", g_fail);
        return 1;
    }
    unsigned long long n = 0; unsigned char *b;
    cx_audiodec_out o;
    b = loadf("/tmp/airtry/media/t.caf", &n);
    expect_i("caf reconhecido", b && cx_audiodec(b, n, &o), 1);
    expect_i("caf status", o.status, 0);
    expect_i("caf taxa", o.rate, 44100);
    if (o.pcm) cmp_pcm("alac caf", o.pcm, o.frames, "/tmp/airtry/media/t.caf.pcm", o.channels);
    cx_audiodec_free(&o); free(b);

    b = loadf("/tmp/airtry/media/t.m4a", &n);
    expect_i("m4a reconhecido", b && cx_audiodec(b, n, &o), 1);
    expect_i("m4a status", o.status, 0);
    if (o.pcm) cmp_pcm("alac m4a", o.pcm, o.frames, "/tmp/airtry/media/t.m4a.pcm", o.channels);
    cx_audiodec_free(&o); free(b);

    b = loadf("/tmp/airtry/media/t.mp3", &n);
    expect_i("mp3 reconhecido", b && cx_audiodec(b, n, &o), 1);
    expect_i("mp3 status", o.status, 0);
    expect_i("mp3 taxa", o.rate, 44100);
    if (o.pcm) cmp_mp3(o.pcm, o.frames, o.channels, "/tmp/airtry/media/t.mp3.pcm");
    cx_audiodec_free(&o); free(b);

    b = loadf("/tmp/airtry/media/t.aac", &n);
    expect_i("aac reconhecido", b && cx_audiodec(b, n, &o), 1);
    expect_i("aac sem PCM", o.status, 1);
    expect_i("aac sem buffer", o.pcm ? 1 : 0, 0);
    if (o.limit) printf("aac limitação: %s\n", o.limit);
    cx_audiodec_free(&o); free(b);

    b = loadf("/tmp/airtry/media/t.opus", &n);
    expect_i("opus reconhecido", b && cx_audiodec(b, n, &o), 1);
    expect_i("opus sem PCM", o.status, 1);
    expect_i("opus sem buffer", o.pcm ? 1 : 0, 0);
    if (o.limit) printf("opus limitação: %s\n", o.limit);
    cx_audiodec_free(&o); free(b);

    expect_i("tcc ausente", cx_media_tcc("kTCCServiceMicrophone"), -1);

    short src[8] = {1000, -1000, 3000, -3000, 0, 0, 0, 0};
    short dst[8];
    int nw = cx_pcm_s16_resample(src, 2, 48000, dst, 2, 48000, 2);
    expect_i("resample identidade", nw, 2);
    expect_i("resample L0", dst[0], 1000);
    expect_i("resample R1", dst[3], -3000);
    short mono[2];
    nw = cx_pcm_s16_resample(src, 2, 48000, mono, 2, 48000, 1);
    expect_i("resample mono", mono[0], 0);

    printf("%s (%d falhas)\n", g_fail ? "FALHOU" : "PASSOU", g_fail);
    return g_fail ? 1 : 0;
}
