/*
 * Teste de host dos codecs do ImageIO (userland/libsystem/imageio.c, só a camada iio_*).
 *   gcc -O1 -DIIO_HOST -Iuserland/libsystem tools/imageio_host.c userland/libsystem/imageio.c -lz -lm -o /tmp/imageio_host
 *   python3 tools/imageio_host.py      (gera os arquivos com o PIL, roda o binário e confere)
 * Uso do binário:  decode <arq> <saida.rgba>   → grava "W H N\n" + quadros ARGB crus
 *                  encode <uti> <entrada.rgba> <saida> [qualidade] [compressão TIFF]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "iio.h"

static unsigned char *slurp(const char *p, size_t *n) {
    FILE *f = fopen(p, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long s = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *b = malloc((size_t)s + 1);
    if (fread(b, 1, (size_t)s, f) != (size_t)s) { fclose(f); free(b); return NULL; }
    fclose(f);
    *n = (size_t)s;
    return b;
}

int main(int argc, char **argv) {
    if (argc >= 4 && !strcmp(argv[1], "decode")) {
        size_t n;
        unsigned char *d = slurp(argv[2], &n);
        iio_image im;
        if (!d || iio_decode(d, n, &im)) { fprintf(stderr, "decode falhou: %s\n", argv[2]); return 2; }
        FILE *o = fopen(argv[3], "wb");
        fprintf(o, "%d %d %d %s %d %d %.3f %d\n", im.f[0].w, im.f[0].h, im.nframes, im.uti, im.loop_count, im.f[0].orientation,
                im.f[0].dpi_x, im.f[0].delay_ms);
        for (int i = 0; i < im.nframes; i++) fwrite(im.f[i].px, 4, (size_t)im.f[i].w * im.f[i].h, o);
        fclose(o);
        iio_image_free(&im);
        free(d);
        return 0;
    }
    if (argc >= 5 && !strcmp(argv[1], "encode")) {
        size_t n;
        unsigned char *d = slurp(argv[3], &n);
        int w, h, nf, k = 0;
        if (!d || sscanf((char *)d, "%d %d %d%n", &w, &h, &nf, &k) != 3) return 2;
        iio_enc_frame fr[64];
        if (nf > 64) nf = 64;
        for (int i = 0; i < nf; i++) fr[i] = (iio_enc_frame){(const uint32_t *)(d + k + 1 + (size_t)i * w * h * 4), w, h, 100, 0};
        iio_enc_opts op = {argc > 5 ? atof(argv[5]) : -1, 1, 144, 144, 0, 0xFFFFFFFFu, argc > 6 ? atoi(argv[6]) : 1};
        unsigned char *out;
        size_t on;
        if (iio_encode(argv[2], fr, nf, &op, &out, &on)) { fprintf(stderr, "encode falhou\n"); return 3; }
        FILE *o = fopen(argv[4], "wb");
        fwrite(out, 1, on, o);
        fclose(o);
        free(out);
        free(d);
        return 0;
    }
    return 1;
}
