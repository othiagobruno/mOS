/* Compara o cx_audiodec com o PCM de referência do ffmpeg (s16le).
 * uso: midia_audio_cmp arquivo ref.raw taxa canais [tolerância de quadros]
 * Codecs com perda (AAC, Opus, MP3) não são bit-exatos entre decodificadores: o critério é o
 * alinhamento (atraso 0), a correlação >= 0.999 e o SNR >= 30 dB, e a duração dentro da tolerância. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct { int kind, status; const char *limit; int rate, channels, frames; short *pcm; } cx_audiodec_out;
int cx_audiodec(const void *b, unsigned long long n, cx_audiodec_out *o);
void cx_audiodec_free(cx_audiodec_out *o);
static unsigned char *load(const char *p, long *n) {
    FILE *f = fopen(p, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END); *n = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *b = malloc((size_t)*n + 1);
    if (fread(b, 1, (size_t)*n, f) != (size_t)*n) { fclose(f); return 0; }
    fclose(f);
    return b;
}
int main(int argc, char **argv) {
    if (argc < 5) return 2;
    long n = 0, rn = 0;
    unsigned char *b = load(argv[1], &n), *rb = load(argv[2], &rn);
    int rate = atoi(argv[3]), ch = atoi(argv[4]), tol = argc > 5 ? atoi(argv[5]) : 0;
    if (!b || !rb) { printf("FALHA leitura\n"); return 1; }
    cx_audiodec_out o;
    if (!cx_audiodec(b, (unsigned long long)n, &o) || o.status) { printf("FALHA status %d kind %d %s\n", o.status, o.kind, o.limit ? o.limit : ""); return 1; }
    const short *ref = (const short *)rb;
    int rf = (int)(rn / 2 / ch);
    int fail = 0;
    if (o.rate != rate || o.channels != ch) { printf("FALHA formato %d Hz %d ch (esperado %d/%d)\n", o.rate, o.channels, rate, ch); fail = 1; }
    if (o.frames > rf + 1 || rf - o.frames > tol) { printf("FALHA duração %d quadros, ffmpeg %d\n", o.frames, rf); fail = 1; }
    int m = o.frames < rf ? o.frames : rf;
    /* atraso: procura em ±2048 quadros no trecho central */
    int st = m / 4, len = m / 2 > 8192 ? 8192 : m / 2, best_at = 0;
    double best = -2;
    for (int off = -2048; off <= 2048; off++) {
        double d = 0, eg = 0, er = 0;
        for (int i = st; i < st + len; i += 2) {
            int j = i + off;
            if (j < 0 || j >= rf) continue;
            double g = o.pcm[i * o.channels], r = ref[j * ch];
            d += g * r; eg += g * g; er += r * r;
        }
        double c = eg > 1 && er > 1 ? d / sqrt(eg * er) : -2;
        if (c > best) { best = c; best_at = off; }
    }
    double sig = 0, err = 0;
    for (int i = 0; i < m * ch && ch == o.channels; i++) {
        int j = i + best_at * ch;
        if (j < 0 || j >= rf * ch) continue;
        double r = ref[j], g = o.pcm[i];
        sig += r * r; err += (g - r) * (g - r);
    }
    double snr = err > 0 ? 10 * log10(sig / err) : 200;
    printf("%d Hz %d ch %d quadros (ffmpeg %d) atraso %d corr %.5f snr %.1f dB\n", o.rate, o.channels, o.frames, rf, best_at, best, snr);
    if (best_at != 0 || best < 0.999 || snr < 30) fail = 1;
    cx_audiodec_free(&o);
    puts(fail ? "FALHA" : "ok");
    return fail;
}
