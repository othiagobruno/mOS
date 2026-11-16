/* Teste de host do decodificador JPEG 2000 (userland/libsystem/iio_jpx.c): decodifica um .jp2/.j2k e grava
 * "w h nc cs alpha" + precisões + os planos em int32 para tools/iio_jpx_host.py comparar com o openjpeg.
 *   gcc -O1 -g -fsanitize=address,undefined -DJPX_HOST -Iuserland/libsystem tools/iio_jpx_host.c \
 *       userland/libsystem/iio_jpx.c -lm -o /tmp/mariana-jpx/jpxhost */
#include <stdio.h>
#include <stdlib.h>
#include "iio_jpx.h"
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "uso: entrada saida\n"); return 2; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) return 2;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *d = malloc((size_t)n);
    if (fread(d, 1, (size_t)n, f) != (size_t)n) return 2;
    fclose(f);
    jpx_image im;
    if (jpx_decode(d, (size_t)n, &im)) { fprintf(stderr, "falhou\n"); return 1; }
    FILE *o = fopen(argv[2], "wb");
    fprintf(o, "%d %d %d %d %d", im.w, im.h, im.nc, im.cs, im.alpha);
    for (int c = 0; c < im.nc; c++) fprintf(o, " %d", im.prec[c]);
    fprintf(o, "\n");
    for (int c = 0; c < im.nc; c++) fwrite(im.plane[c], sizeof(int), (size_t)im.w * im.h, o);
    fclose(o);
    jpx_free(&im);
    free(d);
    return 0;
}
