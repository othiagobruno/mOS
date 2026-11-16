/* Teste de host da saída com escala/formatos extras do VTDecompressionSession (cx_vt_convert_frame em vscode_media.c). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct { unsigned fmt; int w, h, np; unsigned char *p[3]; unsigned long long bpr[3]; } pb_t;
int CVPixelBufferCreateWithBytes(void *a, unsigned long long w, unsigned long long h, unsigned fmt, void *base, unsigned long long bpr, void *rel, void *ctx, void *at, void **o) {
    (void)a; (void)rel; (void)ctx; (void)at; pb_t *p = calloc(1, sizeof *p); p->fmt = fmt; p->w = (int)w; p->h = (int)h; p->np = 1; p->p[0] = base; p->bpr[0] = bpr; *o = p; return 0;
}
int CVPixelBufferCreateWithPlanarBytes(void *a, unsigned long long w, unsigned long long h, unsigned fmt, void *d, unsigned long long ds, unsigned long long np,
                                       void **pb, unsigned long long *pw, unsigned long long *ph, unsigned long long *pr, void *rel, void *ctx, void *at, void **o) {
    (void)a; (void)d; (void)ds; (void)pw; (void)ph; (void)rel; (void)ctx; (void)at;
    pb_t *p = calloc(1, sizeof *p); p->fmt = fmt; p->w = (int)w; p->h = (int)h; p->np = (int)np;
    for (unsigned i = 0; i < np; i++) { p->p[i] = pb[i]; p->bpr[i] = pr[i]; }
    *o = p; return 0;
}
static int matrix_att;
void CVBufferSetAttachment(void *p, void *k, void *v, unsigned m) { (void)p; (void)k; (void)m; if (v && !strcmp((const char *)v, "601")) matrix_att = 1; }
const void *CVYCbCrMatrixGetStringForIntegerCodePoint(int c) { return c == 6 ? "601" : 0; }
const void *CVColorPrimariesGetStringForIntegerCodePoint(int c) { (void)c; return 0; }
const void *CVTransferFunctionGetStringForIntegerCodePoint(int c) { (void)c; return 0; }
const void *kCVImageBufferYCbCrMatrixKey = "M", *kCVImageBufferColorPrimariesKey = "P", *kCVImageBufferTransferFunctionKey = "T";
void *cx_vt_convert_frame(unsigned fmt, int dw, int dh, const unsigned char *y, const unsigned char *u, const unsigned char *v, int sy, int suv, int w, int h, int full, int matrix);
static int fails;
#define CHECK(c) do { if (!(c)) { printf("FALHOU %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
static int near(int a, int b, int t) { return a - b <= t && b - a <= t; }
int main(void) {
    /* I420 8x4: metade esquerda branca, direita preta (faixa de vídeo) */
    unsigned char y[32], u[8], v[8];
    for (int r = 0; r < 4; r++) for (int x = 0; x < 8; x++) y[r * 8 + x] = x < 4 ? 235 : 16;
    memset(u, 128, 8); memset(v, 128, 8);
    pb_t *p = cx_vt_convert_frame(0x42475241u, 4, 2, y, u, v, 8, 4, 8, 4, 0, 6);   /* BGRA reduzido a 4x2 */
    CHECK(p && p->fmt == 0x42475241u && p->w == 4 && p->h == 2 && p->np == 1);
    CHECK(p && near(p->p[0][0], 255, 2) && near(p->p[0][3 * 4], 0, 2) && p->p[0][3] == 255);
    p = cx_vt_convert_frame(0x32767579u, 0, 0, y, u, v, 8, 4, 8, 4, 0, 6);   /* 2vuy no tamanho original */
    CHECK(p && p->fmt == 0x32767579u && p->w == 8 && near(p->p[0][1], 235, 2) && near(p->p[0][0], 128, 1) && near(p->p[0][15], 16, 2) && matrix_att);
    p = cx_vt_convert_frame(0x79343230u, 0, 0, y, u, v, 8, 4, 8, 4, 0, 6);   /* y420: 3 planos */
    CHECK(p && p->np == 3 && p->bpr[1] == 4 && near(p->p[0][0], 235, 2) && near(p->p[1][0], 128, 1) && near(p->p[2][3], 128, 1));
    p = cx_vt_convert_frame(0x34323076u, 16, 8, y, u, v, 8, 4, 8, 4, 0, 6);   /* 420v ampliado */
    CHECK(p && p->np == 2 && p->w == 16 && p->h == 8 && near(p->p[0][0], 235, 2) && near(p->p[0][15], 16, 2));
    printf(fails ? "vt_convert: %d falhas\n" : "vt_convert: ok\n", fails);
    return fails != 0;
}
