/* mOS — compara o PCM original com o decodificado pelo ffmpeg: acha o atraso e mede ganho e SNR */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static short *load(const char *p, long *n) {
    FILE *f = fopen(p, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); *n = ftell(f) / 2; fseek(f, 0, SEEK_SET);
    short *b = malloc(*n * 2);
    fread(b, 2, *n, f);
    fclose(f);
    return b;
}

int main(int argc, char **argv) {
    long na, nb;
    short *a = load(argv[1], &na), *b = load(argv[2], &nb);
    int ch = atoi(argv[3]);
    if (!a || !b) return 2;
    long frames = (na < nb ? na : nb) / ch - 400;
    int worst_ok = 1;
    for (int c = 0; c < ch; c++) {
        int best = 0;
        double bestc = -1e30;
        for (int d = 0; d < 300; d++) {
            double s = 0;
            for (long i = 1000; i < frames; i += 7) s += (double)a[i * ch + c] * b[(i + d) * ch + c];
            if (s > bestc) { bestc = s; best = d; }
        }
        double sa = 0, sab = 0;
        for (long i = 1000; i < frames; i++) { sa += (double)a[i * ch + c] * a[i * ch + c]; sab += (double)a[i * ch + c] * b[(i + best) * ch + c]; }
        double g = sab / sa, err = 0;
        for (long i = 1000; i < frames; i++) { double e = b[(i + best) * ch + c] - g * a[i * ch + c]; err += e * e; }
        double snr = 10 * log10(sa * g * g / (err + 1e-9));
        printf("  canal %d: atraso %d amostras, ganho %.3f, SNR %.1f dB\n", c, best, g, snr);
        if (snr < 30 || g < 0.9 || g > 1.1) worst_ok = 0;
    }
    printf(worst_ok ? "  OK\n" : "  FALHOU (esperado ganho ~1 e SNR >= 30 dB)\n");
    return !worst_ok;
}
