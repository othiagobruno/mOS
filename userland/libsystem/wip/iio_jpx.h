/* mOS — JPEG 2000 (ISO/IEC 15444-1) sem CoreFoundation (iio_jpx.c): JP2 e codestream puro. */
#ifndef MOS_IIO_JPX_H
#define MOS_IIO_JPX_H
#include <stddef.h>
#define JPX_MAXC 16
typedef struct {
    int w, h, nc;                 /* tamanho na grade de referência e componentes de saída (paleta já aplicada) */
    int prec[JPX_MAXC], sgnd[JPX_MAXC];
    int *plane[JPX_MAXC];         /* w × h por componente (subamostradas já esticadas); com sinal se sgnd */
    int cs;                       /* enumcs do JP2 (16 sRGB, 17 cinza, 18 sYCC já convertido, 12 CMYK...), 0 sem */
    unsigned char *icc;           /* perfil ICC do JP2 (colr método 2/3) */
    size_t icc_n;
    int alpha;                    /* componente de opacidade (cdef), -1 sem */
    int premul;                   /* opacidade pré-multiplicada (cdef tipo 2) */
} jpx_image;
int jpx_decode(const unsigned char *d, size_t n, jpx_image *out);  /* 0 = ok */
void jpx_free(jpx_image *img);
int jpx_sniff(const unsigned char *d, size_t n);                   /* 1 JP2, 2 codestream, 0 não */
#endif
