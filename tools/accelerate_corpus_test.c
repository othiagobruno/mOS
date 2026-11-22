/*
 * mOS — teste de host de userland/libsystem/accelerate_corpus.c (com accelerate_lapack.c e accelerate_vimage.c, prefixo
 * mos_). Rodar: tools/accelerate_corpus_test.sh
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

typedef struct { void *data; unsigned long height, width, rowBytes; } vimg_t;
long mos_vImagePremultiplyData_RGBA8888(const vimg_t *, const vimg_t *, unsigned);
long mos_vImageVerticalReflect_ARGB8888(const vimg_t *, const vimg_t *, unsigned);
long mos_vImageConvert_PlanarFtoPlanar16F(const vimg_t *, const vimg_t *, unsigned);
long mos_vImageConvert_Planar16FtoPlanarF(const vimg_t *, const vimg_t *, unsigned);
typedef struct { int32_t Yp_bias, CbCr_bias, YpRangeMax, CbCrRangeMax, YpMax, YpMin, CbCrMax, CbCrMin; } ycc_range_t;
extern const void *const mos_kvImage_ARGBToYpCbCrMatrix_ITU_R_601_4, *const mos_kvImage_YpCbCrToARGBMatrix_ITU_R_601_4;
long mos_vImageConvert_ARGBToYpCbCr_GenerateConversion(const void *, const ycc_range_t *, void *, int, int, unsigned);
long mos_vImageConvert_YpCbCrToARGB_GenerateConversion(const void *, const ycc_range_t *, void *, int, int, unsigned);
long mos_vImageConvert_ARGB8888To420Yp8_CbCr8(const vimg_t *, const vimg_t *, const vimg_t *, const void *, const uint8_t *, unsigned);
long mos_vImageConvert_420Yp8_CbCr8ToARGB8888(const vimg_t *, const vimg_t *, const vimg_t *, const void *, const uint8_t *, uint8_t, unsigned);
long mos_vImageHistogramCalculation_ARGB8888(const vimg_t *, unsigned long *[4], unsigned);
long mos_vImageMatrixMultiply_ARGB8888ToPlanar8(const vimg_t *, const vimg_t *, const int16_t *, int32_t, const int16_t *, int32_t, unsigned);
void mos_vDSP_mmul(const float *, long, const float *, long, float *, long, unsigned long, unsigned long, unsigned long);
typedef struct { uint32_t w[4]; } vU128;
typedef struct { uint32_t w[8]; } vU256;
void mos_vU256Add(const vU256 *, const vU256 *, vU256 *);
void mos_vU128FullMultiply(const vU128 *, const vU128 *, vU256 *);
void mos_vU256Divide(const vU256 *, const vU256 *, vU256 *, vU256 *);
void mos_dposv_(const char *, const int *, const int *, double *, const int *, double *, const int *, int *);
void mos_dgels_(const char *, const int *, const int *, const int *, double *, const int *, double *, const int *, double *, const int *, int *);
void mos_sgels_(const char *, const int *, const int *, const int *, float *, const int *, float *, const int *, float *, const int *, int *);

static int fails, total;
#define CK(c, m) do { total++; if (!(c)) { fails++; printf("FALHA: %s\n", m); } } while (0)

int main(void) {
    uint8_t px[8] = {200, 100, 50, 128, 10, 20, 30, 255}, out[8];
    vimg_t s = {px, 1, 2, 8}, d = {out, 1, 2, 8};
    mos_vImagePremultiplyData_RGBA8888(&s, &d, 0);
    CK(out[0] == 100 && out[1] == 50 && out[2] == 25 && out[3] == 128 && out[4] == 10 && out[7] == 255, "premultiply RGBA");
    uint8_t img[3 * 4] = {1,1,1,1, 2,2,2,2, 3,3,3,3};
    vimg_t r = {img, 3, 1, 4};
    mos_vImageVerticalReflect_ARGB8888(&r, &r, 0);
    CK(img[0] == 3 && img[4] == 2 && img[8] == 1, "espelho vertical em lugar");
    float fv[4] = {1.0f, -2.5f, 65504.0f, 0.333333f}, fb[4];
    uint16_t hv[4];
    vimg_t fs = {fv, 1, 4, 16}, hs = {hv, 1, 4, 8}, fo = {fb, 1, 4, 16};
    mos_vImageConvert_PlanarFtoPlanar16F(&fs, &hs, 0);
    mos_vImageConvert_Planar16FtoPlanarF(&hs, &fo, 0);
    CK(hv[0] == 0x3c00 && hv[1] == 0xc100 && hv[2] == 0x7bff && fb[1] == -2.5f && fabsf(fb[3] - 0.333333f) < 1e-3f, "float <-> half");
    /* ARGB -> NV12 -> ARGB (601, faixa de vídeo): a cor volta perto */
    ycc_range_t rg = {16, 128, 235, 240, 235, 16, 240, 16};
    uint8_t a2y[128], y2a[128];
    CK(mos_vImageConvert_ARGBToYpCbCr_GenerateConversion(mos_kvImage_ARGBToYpCbCrMatrix_ITU_R_601_4, &rg, a2y, 0, 0, 0) == 0 &&
       mos_vImageConvert_YpCbCrToARGB_GenerateConversion(mos_kvImage_YpCbCrToARGBMatrix_ITU_R_601_4, &rg, y2a, 0, 0, 0) == 0, "GenerateConversion");
    uint8_t argb[2 * 2 * 4], Y[4], CbCr[2], back[16];
    for (int i = 0; i < 4; i++) { argb[i * 4] = 255; argb[i * 4 + 1] = 200; argb[i * 4 + 2] = 120; argb[i * 4 + 3] = 40; }
    vimg_t as = {argb, 2, 2, 8}, ys = {Y, 2, 2, 2}, cs = {CbCr, 1, 1, 2}, bk = {back, 2, 2, 8};
    CK(mos_vImageConvert_ARGB8888To420Yp8_CbCr8(&as, &ys, &cs, a2y, 0, 0) == 0, "ARGB -> 420Yp8_CbCr8");
    CK(mos_vImageConvert_420Yp8_CbCr8ToARGB8888(&ys, &cs, &bk, y2a, 0, 255, 0) == 0, "420Yp8_CbCr8 -> ARGB");
    CK(abs(back[1] - 200) <= 3 && abs(back[2] - 120) <= 3 && abs(back[3] - 40) <= 3 && back[0] == 255, "ida e volta da cor");
    unsigned long h0[256], h1[256], h2[256], h3[256], *hist[4] = {h0, h1, h2, h3};
    mos_vImageHistogramCalculation_ARGB8888(&as, hist, 0);
    CK(h0[255] == 4 && h1[200] == 4 && h3[40] == 4 && h2[0] == 0, "histograma");
    int16_t mm[4] = {0, 1, 1, 1};
    uint8_t gray[4];
    vimg_t gs = {gray, 2, 2, 2};
    mos_vImageMatrixMultiply_ARGB8888ToPlanar8(&as, &gs, mm, 3, 0, 0, 0);
    CK(gray[0] == 120, "matriz ARGB -> Planar8 (média de RGB)");
    float A[6] = {1, 2, 3, 4, 5, 6}, B[6] = {7, 8, 9, 10, 11, 12}, C[4];
    mos_vDSP_mmul(A, 1, B, 1, C, 1, 2, 2, 3);
    CK(C[0] == 58 && C[1] == 64 && C[2] == 139 && C[3] == 154, "vDSP_mmul");
    vU256 x = {{0xffffffff, 0xffffffff, 0, 0, 0, 0, 0, 0}}, y = {{1, 0, 0, 0, 0, 0, 0, 0}}, z;
    mos_vU256Add(&x, &y, &z);
    CK(z.w[0] == 0 && z.w[1] == 0 && z.w[2] == 1, "vU256Add com vai-um");
    vU128 p = {{0, 0, 0, 1}}, q = {{0, 0, 0, 1}};
    mos_vU128FullMultiply(&p, &q, &z);
    CK(z.w[6] == 1 && z.w[0] == 0 && z.w[7] == 0, "vU128FullMultiply 2^96 * 2^96 = 2^192");
    vU256 num = {{1000003, 7, 0, 0, 0, 0, 0, 0}}, den = {{97, 0, 0, 0, 0, 0, 0, 0}}, qq, rr;
    mos_vU256Divide(&num, &den, &qq, &rr);
    unsigned long long nv = ((unsigned long long)7 << 32) | 1000003ull;
    CK((((unsigned long long)qq.w[1] << 32) | qq.w[0]) == nv / 97 && rr.w[0] == nv % 97, "vU256Divide");
    /* posv: [[4,2],[2,3]] x = [2,1] -> x = [0.5, 0] */
    double M[4] = {4, 2, 2, 3}, b[2] = {2, 1};
    int n = 2, one = 1, info = -9;
    mos_dposv_("U", &n, &one, M, &n, b, &n, &info);
    CK(info == 0 && fabs(b[0] - 0.5) < 1e-12 && fabs(b[1]) < 1e-12, "dposv");
    /* gels: reta y = 1 + 2x pelos pontos (0,1) (1,3) (2,5) (3,7.1) */
    double G[8] = {1, 1, 1, 1, 0, 1, 2, 3}, yv[4] = {1, 3, 5, 7.1}, wq;
    int m4 = 4, n2 = 2, lw = -1;
    mos_dgels_("N", &m4, &n2, &one, G, &m4, yv, &m4, &wq, &lw, &info);
    lw = (int)wq;
    double wbuf[256];
    mos_dgels_("N", &m4, &n2, &one, G, &m4, yv, &m4, wbuf, &lw, &info);
    CK(info == 0 && fabs(yv[0] - 0.98) < 1e-9 && fabs(yv[1] - 2.03) < 1e-9, "dgels mínimos quadrados");
    float Gf[8] = {1, 1, 1, 1, 0, 1, 2, 3}, yf[4] = {1, 3, 5, 7}, wf[256];
    lw = 256;
    mos_sgels_("N", &m4, &n2, &one, Gf, &m4, yf, &m4, wf, &lw, &info);
    CK(info == 0 && fabsf(yf[0] - 1) < 1e-4f && fabsf(yf[1] - 2) < 1e-4f, "sgels");
    printf("accelerate-corpus: %d/%d ok\n", total - fails, total);
    return fails ? 1 : 0;
}
