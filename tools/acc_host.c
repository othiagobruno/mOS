/* Referência calculada aqui, sem chamar de novo a função sob teste. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef long vstride;
typedef unsigned long vlen;
typedef struct { float *realp, *imagp; } dsp_split_t;
typedef struct { void *data; unsigned long height, width, rowBytes; } vimg_t;
typedef struct { uint32_t bpc, bpp; void *space; uint32_t bitmap; uint32_t version; const double *decode; int intent; } vimg_fmt_t;

void vDSP_vclr(float *, vstride, vlen);
void vDSP_vfill(const float *, float *, vstride, vlen);
void vDSP_vneg(const float *, vstride, float *, vstride, vlen);
void vDSP_vabs(const float *, vstride, float *, vstride, vlen);
void vDSP_vdiv(const float *, vstride, const float *, vstride, float *, vstride, vlen);
void vDSP_vsdiv(const float *, vstride, const float *, float *, vstride, vlen);
void vDSP_vma(const float *, vstride, const float *, vstride, const float *, vstride, float *, vstride, vlen);
void vDSP_vmax(const float *, vstride, const float *, vstride, float *, vstride, vlen);
void vDSP_vfltu8(const uint8_t *, vstride, float *, vstride, vlen);
void vDSP_sve(const float *, vstride, float *, vlen);
void vDSP_sveD(const double *, vstride, double *, vlen);
void vDSP_meanv(const float *, vstride, float *, vlen);
void vDSP_svemg(const float *, vstride, float *, vlen);
void vDSP_rmsqv(const float *, vstride, float *, vlen);
void vDSP_maxvi(const float *, vstride, float *, vlen *, vlen);
void vDSP_vramp(const float *, const float *, float *, vstride, vlen);
void vDSP_vgen(const float *, const float *, float *, vstride, vlen);
void vDSP_vdbcon(const float *, vstride, const float *, float *, vstride, vlen, unsigned);
void vDSP_hann_window(float *, vlen, int);
void vDSP_mmov(const float *, float *, vlen, vlen, vlen, vlen);
void vDSP_mtrans(const float *, vstride, float *, vstride, vlen, vlen);
void vDSP_zvadd(const dsp_split_t *, vstride, const dsp_split_t *, vstride, const dsp_split_t *, vstride, vlen);
void vDSP_zvsub(const dsp_split_t *, vstride, const dsp_split_t *, vstride, const dsp_split_t *, vstride, vlen);
void vDSP_zvdiv(const dsp_split_t *, vstride, const dsp_split_t *, vstride, const dsp_split_t *, vstride, vlen);
void vDSP_zvconj(const dsp_split_t *, vstride, const dsp_split_t *, vstride, vlen);
void vDSP_zvabs(const dsp_split_t *, vstride, float *, vstride, vlen);
void vDSP_zvmags(const dsp_split_t *, vstride, float *, vstride, vlen);
void *vDSP_DFT_zop_CreateSetup(void *, vlen, int);
void vDSP_DFT_DestroySetup(void *);
void vDSP_DFT_Execute(void *, const float *, const float *, float *, float *);
void vDSP_fft_zript(void *, const dsp_split_t *, vstride, const dsp_split_t *, vstride, unsigned long, int);
void vvrecf(float *, const float *, const int *);
void vvsqrtf(float *, const float *, const int *);
void vvexpf(float *, const float *, const int *);
void vvlog10f(float *, const float *, const int *);
void vvsinf(float *, const float *, const int *);
void vvcosf(float *, const float *, const int *);
void vvpowf(float *, const float *, const float *, const int *);
void cblas_sgemm(int, int, int, int, int, int, float, const float *, int, const float *, int, float, float *, int);
void cblas_dgemm(int, int, int, int, int, int, double, const double *, int, const double *, int, double, double *, int);
long vImageBoxConvolve_ARGB8888(const vimg_t *, const vimg_t *, void *, unsigned long, unsigned long, unsigned, unsigned, const uint8_t *, unsigned);
long vImageBuffer_Init(vimg_t *, unsigned long, unsigned long, unsigned, unsigned);
long vImageCopyBuffer(const vimg_t *, const vimg_t *, unsigned long, unsigned);
long vImageConvolve_PlanarF(const vimg_t *, const vimg_t *, void *, unsigned long, unsigned long, const float *, unsigned long, unsigned long, float, unsigned);
long vImageScale_Planar8(const vimg_t *, const vimg_t *, void *, unsigned);
long vImageScale_PlanarF(const vimg_t *, const vimg_t *, void *, unsigned);
long vImageScale_Planar16U(const vimg_t *, const vimg_t *, void *, unsigned);
long vImageScale_ARGB8888(const vimg_t *, const vimg_t *, void *, unsigned);
long vImageScale_CbCr8(const vimg_t *, const vimg_t *, void *, unsigned);
long vImageScale_CbCr16U(const vimg_t *, const vimg_t *, void *, unsigned);
long vImageConvert_ARGB8888toPlanar8(const vimg_t *, const vimg_t *, unsigned);
long vImageConvert_Planar8toPlanarF(const vimg_t *, const vimg_t *, unsigned);
long vImageConvert_PlanarFtoPlanar8(const vimg_t *, const vimg_t *, unsigned);
long vImageMatrixMultiply_ARGB8888(const vimg_t *, const vimg_t *, const int16_t *, int32_t, const int16_t *, const int32_t *, unsigned);
long vImageUnpremultiplyData_RGBA8888(const vimg_t *, const vimg_t *, unsigned);
long vImageRotate90_ARGB8888(const vimg_t *, const vimg_t *, uint8_t, const uint8_t *, unsigned);
long vImageBuffer_InitWithCGImage(vimg_t *, const vimg_fmt_t *, const void *, void *, unsigned);
void *vImageCreateCGImageFromBuffer(const vimg_t *, const vimg_fmt_t *, void (*)(void *, const void *), void *, unsigned, long *);

static const double PI = 3.14159265358979323846;
static int fails;
static void ck(int cond, const char *m) { if (!cond) { fprintf(stderr, "falhou: %s\n", m); fails++; } }
static int nearf(float a, float b) { float d = a - b; if (d < 0) d = -d; return d < 1e-4f || d < 1e-3f * (b < 0 ? -b : b); }

static uint8_t cg_px[16];
static int cg_w, cg_h, cg_bpr;
int cx_cgimage_src(const void *img, const uint8_t **px, long *bpr, int *w, int *h, int *premul) {
    (void)img; if (px) *px = cg_px; if (bpr) *bpr = cg_bpr; if (w) *w = cg_w; if (h) *h = cg_h; if (premul) *premul = 0; return 1;
}
static uint8_t seen[64];
static size_t seen_n, seen_w, seen_bpp;
static void *(*rel_fn)(void);
void *CGDataProviderCreateWithData(void *info, const void *data, size_t size, void (*release)(void *, const void *, size_t)) {
    (void)info; seen_n = size; if (size > sizeof seen) size = sizeof seen; memcpy(seen, data, size); rel_fn = (void *)release; (void)data; return (void *)1;
}
void CGDataProviderRelease(void *p) { (void)p; }
void *CGImageCreate(size_t w, size_t h, size_t bpc, size_t bpp, size_t bpr, const void *space, uint32_t info, void *provider, const void *decode, int si, int intent) {
    (void)h; (void)bpc; (void)bpr; (void)space; (void)info; (void)provider; (void)decode; (void)si; (void)intent;
    seen_w = w; seen_bpp = bpp; return (void *)2;
}

int main(void) {
    float a[4] = {1, -2, 3, 4}, b[4] = {4, 1, 0.5f, 2}, c[4];
    vDSP_vclr(c, 1, 4); ck(c[0] == 0 && c[3] == 0, "vclr");
    float five = 5; vDSP_vfill(&five, c, 1, 4); ck(c[2] == 5, "vfill");
    vDSP_vneg(a, 1, c, 1, 4); ck(c[0] == -1 && c[1] == 2, "vneg");
    vDSP_vabs(a, 1, c, 1, 4); ck(c[1] == 2 && c[0] == 1, "vabs");
    vDSP_vdiv(b, 1, a, 1, c, 1, 4); ck(nearf(c[0], 1.f / 4) && nearf(c[1], -2), "vdiv");
    float two = 2; vDSP_vsdiv(a, 1, &two, c, 1, 4); ck(c[2] == 1.5f, "vsdiv");
    vDSP_vma(a, 1, b, 1, a, 1, c, 1, 4); ck(nearf(c[0], 1 * 4 + 1) && nearf(c[1], -2 * 1 + -2), "vma");
    vDSP_vmax(a, 1, b, 1, c, 1, 4); ck(c[0] == 4 && c[1] == 1, "vmax");
    uint8_t u[3] = {0, 9, 255}; vDSP_vfltu8(u, 1, c, 1, 3); ck(c[1] == 9 && c[2] == 255, "vfltu8");
    float s = 0; vDSP_sve(a, 1, &s, 4); ck(s == 6, "sve");
    double ad[3] = {1.5, 2.5, -1}; double sd = 0; vDSP_sveD(ad, 1, &sd, 3); ck(sd == 3, "sveD");
    vDSP_meanv(a, 1, &s, 4); ck(s == 1.5f, "meanv");
    vDSP_svemg(a, 1, &s, 4); ck(s == 10, "svemg");
    vDSP_rmsqv(a, 1, &s, 4); ck(nearf(s, sqrtf((1 + 4 + 9 + 16) / 4.f)), "rmsqv");
    vlen ix = 9; vDSP_maxvi(a, 1, &s, &ix, 4); ck(s == 4 && ix == 3, "maxvi");
    float z0 = 1, st = 2; vDSP_vramp(&z0, &st, c, 1, 4);
    ck(c[0] == 1 && c[1] == 3 && c[3] == 7, "vramp");
    float g0 = 0, g1 = 3; vDSP_vgen(&g0, &g1, c, 1, 4); ck(c[0] == 0 && nearf(c[3], 3) && nearf(c[1], 1), "vgen");
    float dbref = 2; float srcv[2] = {10, 100}; vDSP_vdbcon(srcv, 1, &dbref, c, 1, 2, 1);
    ck(nearf(c[0], 20 * log10f(5)) && nearf(c[1], 20 * log10f(50)), "vdbcon");
    vDSP_hann_window(c, 4, 0);
    ck(nearf(c[0], 0) && nearf(c[1], (float)(0.5 * (1 - cos(2 * PI / 3)))), "hann");
    float M[6] = {1, 2, 3, 4, 5, 6}, T[6];
    vDSP_mmov(M, T, 3, 2, 3, 3); ck(T[0] == 1 && T[3] == 4 && T[5] == 6, "mmov");
    vDSP_mtrans(M, 3, T, 2, 3, 2);
    ck(T[0] == 1 && T[1] == 4 && T[2] == 2 && T[3] == 5 && T[4] == 3 && T[5] == 6, "mtrans");
    float ar[2] = {1, 2}, ai[2] = {3, -1}, br[2] = {4, 0}, bi[2] = {-1, 2};
    float cr[2], ci[2];
    dsp_split_t A = {ar, ai}, B = {br, bi}, C = {cr, ci};
    vDSP_zvadd(&A, 1, &B, 1, &C, 1, 2); ck(cr[0] == 5 && ci[0] == 2 && cr[1] == 2 && ci[1] == 1, "zvadd");
    vDSP_zvsub(&A, 1, &B, 1, &C, 1, 2); ck(cr[0] == -3 && ci[0] == 4, "zvsub");
    vDSP_zvdiv(&B, 1, &A, 1, &C, 1, 1);
    float den = 16 + 1; ck(nearf(cr[0], (1 * 4 + 3 * -1) / den) && nearf(ci[0], (3 * 4 - 1 * -1) / den), "zvdiv");
    vDSP_zvconj(&A, 1, &C, 1, 2); ck(cr[1] == 2 && ci[1] == 1, "zvconj");
    float mag[2]; vDSP_zvabs(&A, 1, mag, 1, 2); ck(nearf(mag[0], sqrtf(1 + 9)), "zvabs");
    vDSP_zvmags(&A, 1, mag, 1, 2); ck(mag[0] == 10 && mag[1] == 5, "zvmags");
    float ir[4] = {1, 0, -1, 0}, ii[4] = {0, 0, 0, 0}, or_[4], oi[4];
    void *setup = vDSP_DFT_zop_CreateSetup(0, 4, 1);
    vDSP_DFT_Execute(setup, ir, ii, or_, oi);
    for (int k = 0; k < 4; k++) {
        double sr = 0, si = 0;
        for (int t = 0; t < 4; t++) { double ang = -2 * PI * k * t / 4; sr += ir[t] * cos(ang); si += ir[t] * sin(ang); }
        ck(nearf(or_[k], (float)sr) && nearf(oi[k], (float)si), "dft");
    }
    vDSP_DFT_DestroySetup(setup);
    float pr[4] = {1, 2, 3, 4}, pi[4] = {0, 0, 0, 0}, tr[4], ti[4];
    dsp_split_t P = {pr, pi}, TMP = {tr, ti};
    vDSP_fft_zript(0, &P, 1, &TMP, 1, 3, 1);
    /* N = 8, empacotado: x[2k]=realp[k], x[2k+1]=imagp[k] antes da transformada. Direta * 2. */
    double xr[8] = {1, 0, 2, 0, 3, 0, 4, 0};
    for (int k = 0; k < 4; k++) {
        double sr = 0, si = 0;
        for (int t = 0; t < 8; t++) { double ang = -2 * PI * k * t / 8; sr += xr[t] * cos(ang); si += xr[t] * sin(ang); }
        float er = (float)(2 * sr), ei = (k == 0 ? (float)(2 * sr) : (float)(2 * si));
        if (k == 0) ck(nearf(pr[0], er) && nearf(pi[0], (float)(2 * (xr[0] - xr[1] + xr[2] - xr[3] + xr[4] - xr[5] + xr[6] - xr[7]))), "zrip nyquist");
        else ck(nearf(pr[k], er) && nearf(pi[k], ei), "zrip");
    }
    int n = 3; float x[3] = {2, 4, 9}, y[3];
    vvrecf(y, x, &n); ck(nearf(y[0], 0.5f) && nearf(y[2], 1.f / 9), "vvrecf");
    vvsqrtf(y, x, &n); ck(nearf(y[1], 2), "vvsqrtf");
    float e[2] = {0, 1}; n = 2; vvexpf(y, e, &n); ck(nearf(y[0], 1) && nearf(y[1], expf(1)), "vvexpf");
    float lg[2] = {10, 100}; vvlog10f(y, lg, &n); ck(nearf(y[0], 1) && nearf(y[1], 2), "vvlog10f");
    float ang[2] = {0, (float)(PI / 2)}; vvsinf(y, ang, &n); ck(nearf(y[0], 0) && nearf(y[1], 1), "vvsinf");
    vvcosf(y, ang, &n); ck(nearf(y[0], 1) && nearf(y[1], 0), "vvcosf");
    float base[2] = {2, 9}, expn[2] = {3, 0.5f}; vvpowf(y, expn, base, &n); ck(nearf(y[0], 8) && nearf(y[1], 3), "vvpowf");
    float Af[4] = {1, 2, 3, 4}, Bf[4] = {1, 0, 0, 1}, Cf[4] = {1, 1, 1, 1};
    cblas_sgemm(101, 111, 111, 2, 2, 2, 1, Af, 2, Bf, 2, 1, Cf, 2);
    ck(Cf[0] == 2 && Cf[1] == 3 && Cf[2] == 4 && Cf[3] == 5, "sgemm");
    double Ad[4] = {1, 2, 0, 1}, Bd[2] = {3, 4}, Cd[2] = {0, 0}, one = 1, zero = 0;
    cblas_dgemm(102, 111, 111, 2, 1, 2, one, Ad, 2, Bd, 1, zero, Cd, 1);
    /* col major A is 2x2 stored column-wise: columns (1,0) and (2,1)? Ad lda 2: A[0]=1 A[1]=0 A[2]=2 A[3]=1 so columns [1,0] and [2,1]. B is [3,4]. C = A*B = [1*3+2*4, 0*3+1*4] = [11, 4] */
    ck(Cd[0] == 3 && Cd[1] == 10, "dgemm");
    uint8_t pix[16] = {1, 2, 3, 4,  5, 6, 7, 8,  9, 10, 11, 12,  13, 14, 15, 16};
    uint8_t outb[16];
    vimg_t src = {pix, 2, 2, 8}, dst = {outb, 2, 2, 8};
    uint8_t bgc[4] = {0, 0, 0, 0};
    ck(vImageBoxConvolve_ARGB8888(&src, &dst, 0, 0, 0, 1, 1, bgc, 0) == 0, "box st");
    ck(outb[0] == 1 && outb[4] == 5 && outb[12] == 13, "box 1x1");
    uint8_t p8[4] = {0, 10, 20, 30}, o8[1];
    vimg_t s8 = {p8, 2, 2, 2}, d8 = {o8, 1, 1, 1};
    ck(vImageScale_Planar8(&s8, &d8, 0, 0) == 0, "scale8 st");
    /* centro do pixel de destino: (0.5*2-0.5, 0.5*2-0.5) = (0.5, 0.5) */
    float r = 0 * 0.5f * 0.5f + 10 * 0.5f * 0.5f + 20 * 0.5f * 0.5f + 30 * 0.5f * 0.5f;
    ck(o8[0] == (uint8_t)(r + 0.5f), "scale8");
    float pf[4] = {0, 10, 20, 30}, of[1];
    vimg_t sf = {pf, 2, 2, 8}, df = {of, 1, 1, 4};
    vImageScale_PlanarF(&sf, &df, 0, 0); ck(nearf(of[0], 15), "scaleF");
    uint16_t p16[4] = {0, 10, 20, 30}, o16[1];
    vimg_t s16 = {p16, 2, 2, 4}, d16 = {o16, 1, 1, 2};
    vImageScale_Planar16U(&s16, &d16, 0, 0); ck(o16[0] == (uint16_t)(r + 0.5f), "scale16");
    uint8_t argb[16], oargb[4];
    for (int i = 0; i < 16; i++) argb[i] = (uint8_t)i;
    vimg_t sa = {argb, 2, 2, 8}, da = {oargb, 1, 1, 4};
    vImageScale_ARGB8888(&sa, &da, 0, 0);
    ck(oargb[0] == (uint8_t)((0 + 4 + 8 + 12) / 4.0 + 0.5), "scale argb");
    uint8_t cb[8] = {1, 2, 3, 4, 5, 6, 7, 8}, ocb[2];
    vimg_t sc = {cb, 2, 2, 4}, dc = {ocb, 1, 1, 2};
    vImageScale_CbCr8(&sc, &dc, 0, 0);
    ck(ocb[0] == (uint8_t)((1 + 3 + 5 + 7) / 4.0 + 0.5) && ocb[1] == (uint8_t)((2 + 4 + 6 + 8) / 4.0 + 0.5), "cbcr8");
    uint16_t cb16[8] = {1, 2, 3, 4, 5, 6, 7, 8}, ocb16[2];
    vimg_t sc16 = {cb16, 2, 2, 8}, dc16 = {ocb16, 1, 1, 4};
    vImageScale_CbCr16U(&sc16, &dc16, 0, 0); ck(ocb16[0] == 4 && ocb16[1] == 5, "cbcr16");
    uint8_t planes[4][2]; vimg_t dp[4];
    for (int i = 0; i < 4; i++) { dp[i].data = planes[i]; dp[i].height = 1; dp[i].width = 2; dp[i].rowBytes = 2; }
    vimg_t sp = {argb, 1, 2, 8};
    vImageConvert_ARGB8888toPlanar8(&sp, dp, 0);
    ck(planes[0][0] == 0 && planes[1][0] == 1 && planes[3][1] == 7, "planar8");
    float pf2[2]; vimg_t df2 = {pf2, 1, 2, 8}; vimg_t su = {planes[1], 1, 2, 2};
    vImageConvert_Planar8toPlanarF(&su, &df2, 0); ck(nearf(pf2[0], 1.f / 255) && nearf(pf2[1], 5.f / 255), "p8tof");
    uint8_t back[2]; vimg_t db = {back, 1, 2, 2};
    vImageConvert_PlanarFtoPlanar8(&df2, &db, 0); ck(back[0] == 1 && back[1] == 5, "fto8");
    float ker[1] = {2}; float srcf[1] = {3}, dstf[1];
    vimg_t sk = {srcf, 1, 1, 4}, dk = {dstf, 1, 1, 4};
    vImageConvolve_PlanarF(&sk, &dk, 0, 0, 0, ker, 1, 1, 2, 0); ck(dstf[0] == 6, "conv");
    uint8_t srcp[4] = {9, 8, 7, 6}, dstp[8] = {0};
    vimg_t scp = {srcp, 1, 2, 2}, dcp = {dstp, 1, 2, 4};
    vImageCopyBuffer(&scp, &dcp, 1, 0); ck(dstp[0] == 9 && dstp[1] == 8, "copy");
    vimg_t ini; ck(vImageBuffer_Init(&ini, 2, 3, 8, 0) == 0 && ini.rowBytes == 16 && ini.data, "init");
    free(ini.data);
    int16_t mat[16] = {0}; mat[0] = mat[5] = mat[10] = mat[15] = 1;
    uint8_t mx[4] = {4, 5, 6, 7}, my[4];
    vimg_t sm = {mx, 1, 1, 4}, dm = {my, 1, 1, 4};
    vImageMatrixMultiply_ARGB8888(&sm, &dm, mat, 1, 0, 0, 0); ck(my[0] == 4 && my[3] == 7, "matrix");
    uint8_t pre[8] = {10, 20, 30, 128,  0, 0, 0, 0}, up[8];
    vimg_t spr = {pre, 1, 2, 8}, dpr = {up, 1, 2, 8};
    vImageUnpremultiplyData_RGBA8888(&spr, &dpr, 0);
    ck(up[0] == (uint8_t)((10 * 255 + 64) / 128) && up[3] == 128 && up[4] == 0, "unpre");
    uint8_t rot[16], rotd[16];
    for (int i = 0; i < 16; i++) rot[i] = (uint8_t)(i + 1);
    vimg_t sr = {rot, 2, 2, 8}, dr = {rotd, 2, 2, 8};
    vImageRotate90_ARGB8888(&sr, &dr, 1, bgc, 0);
    /* 90 horário: dest(x,y) = src(y, H-1-x). dest(0,0) = src(0,1) = pixel na segunda linha = bytes 8.. */
    ck(rotd[0] == 9 && rotd[1] == 10 && rotd[4] == 1, "rot90");
    cg_w = 2; cg_h = 1; cg_bpr = 8;
    for (int i = 0; i < 8; i++) cg_px[i] = (uint8_t)(10 + i);
    vimg_t fromcg;
    ck(vImageBuffer_InitWithCGImage(&fromcg, 0, 0, (void *)1, 0) == 0, "cg init");
    ck(((uint8_t *)fromcg.data)[0] == 10 && ((uint8_t *)fromcg.data)[4] == 14, "cg bytes");
    long err = 9;
    void *img = vImageCreateCGImageFromBuffer(&fromcg, 0, 0, 0, 0, &err);
    ck(img && err == 0 && seen_w == 2 && seen_bpp == 32 && seen[0] == 10, "cg create");
    free(fromcg.data);
    if (fails) { fprintf(stderr, "%d falhas\n", fails); return 1; }
    printf("acc_host ok\n");
    return 0;
}
