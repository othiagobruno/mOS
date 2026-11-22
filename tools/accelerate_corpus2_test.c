/*
 * mOS — teste de host de userland/libsystem/accelerate_corpus2.c (com o resto do Accelerate do mOS, prefixo mos_).
 * Rodar: tools/accelerate_corpus2_test.sh
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { void *data; unsigned long height, width, rowBytes; } vimg_t;
typedef struct { uint32_t bpc, bpp; void *space; uint32_t bitmap, version; const double *decode; int intent; } vimg_fmt_t;
long mos_vImageRotate90_Planar8(const vimg_t *, const vimg_t *, uint8_t, uint8_t, unsigned);
long mos_vImageRotate_ARGB8888(const vimg_t *, const vimg_t *, void *, float, const uint8_t *, unsigned);
long mos_vImageHorizontalReflect_Planar8(const vimg_t *, const vimg_t *, unsigned);
long mos_vImageVerticalReflect_Planar16U(const vimg_t *, const vimg_t *, unsigned);
long mos_vImagePremultipliedAlphaBlend_BGRA8888(const vimg_t *, const vimg_t *, const vimg_t *, unsigned);
long mos_vImagePremultipliedAlphaBlendMultiply_RGBA8888(const vimg_t *, const vimg_t *, const vimg_t *, unsigned);
long mos_vImagePremultipliedAlphaBlendScreen_RGBA8888(const vimg_t *, const vimg_t *, const vimg_t *, unsigned);
long mos_vImageMax_ARGB8888(const vimg_t *, const vimg_t *, void *, unsigned long, unsigned long, unsigned long, unsigned long, unsigned);
long mos_vImageConvolve_ARGB8888(const vimg_t *, const vimg_t *, void *, unsigned long, unsigned long, const int16_t *, uint32_t, uint32_t, int32_t, const uint8_t *, unsigned);
long mos_vImageConvert_RGB888toBGRA8888(const vimg_t *, const vimg_t *, uint8_t, const vimg_t *, int, unsigned);
long mos_vImageConvert_Planar16FtoPlanar8(const vimg_t *, const vimg_t *, unsigned);
long mos_vImageConvert_ARGB16UToARGB8888(const vimg_t *, const vimg_t *, const uint8_t *, uint8_t, const uint8_t *, unsigned);
long mos_vImageAffineWarpCG_Planar8(const vimg_t *, const vimg_t *, void *, const double *, uint8_t, unsigned);
long mos_vImageBuffer_InitWithCVPixelBuffer(vimg_t *, const vimg_fmt_t *, void *, void *, const double *, unsigned);
long mos_vImageBuffer_CopyToCVPixelBuffer(const vimg_t *, const vimg_fmt_t *, void *, void *, const double *, unsigned);
typedef struct { float *realp, *imagp; } split_t;
typedef struct { double *realp, *imagp; } splitd_t;
void mos_vDSP_fft_zip(void *, const split_t *, long, unsigned long, int);
void mos_vDSP_fft_zopD(void *, const splitd_t *, long, const splitd_t *, long, unsigned long, int);
void mos_vDSP_vsubD(const double *, long, const double *, long, double *, long, unsigned long);
void mos_vDSP_mmovD(const double *, double *, unsigned long, unsigned long, unsigned long, unsigned long);
void mos_vDSP_distancesq(const float *, long, const float *, long, float *, unsigned long);
void mos_vDSP_minmgv(const float *, long, float *, unsigned long);

/* CVPixelBuffer de mentira: 'BGRA' 3x2 */
static uint8_t pbmem[2 * 16];
void *CVPixelBufferGetBaseAddress(void *p) { (void)p; return pbmem; }
int CVPixelBufferLockBaseAddress(void *p, uint64_t f) { (void)p; (void)f; return 0; }
int CVPixelBufferUnlockBaseAddress(void *p, uint64_t f) { (void)p; (void)f; return 0; }
uint64_t CVPixelBufferGetBytesPerRow(void *p) { (void)p; return 16; }
uint64_t CVPixelBufferGetWidth(void *p) { (void)p; return 3; }
uint64_t CVPixelBufferGetHeight(void *p) { (void)p; return 2; }
uint32_t CVPixelBufferGetPixelFormatType(void *p) { (void)p; return 0x42475241; }
void *CGColorSpaceRetain(void *s) { return s; }
void CGColorSpaceRelease(void *s) { (void)s; }
void *CGColorSpaceCreateDeviceRGB(void) { return 0; }

static int fails, total;
#define CK(c, m) do { total++; if (!(c)) { fails++; printf("FALHA: %s\n", m); } } while (0)

int main(void) {
    /* rotação de 90° horária: 2x3 -> 3x2 */
    uint8_t a[6] = {1, 2, 3, 4, 5, 6}, b[6];
    vimg_t s = {a, 2, 3, 3}, d = {b, 3, 2, 2};
    mos_vImageRotate90_Planar8(&s, &d, 1, 0, 0);
    /* 1 2 3 / 4 5 6  -> horário: 4 1 / 5 2 / 6 3 */
    CK(b[0] == 4 && b[1] == 1 && b[2] == 5 && b[3] == 2 && b[4] == 6 && b[5] == 3, "Rotate90 Planar8 horário");

    /* espelho horizontal em lugar */
    uint8_t h[5] = {1, 2, 3, 4, 5};
    vimg_t hs = {h, 1, 5, 5};
    mos_vImageHorizontalReflect_Planar8(&hs, &hs, 0);
    CK(h[0] == 5 && h[2] == 3 && h[4] == 1, "HorizontalReflect Planar8 em lugar");
    uint16_t v16[3] = {10, 20, 30};
    vimg_t vs = {v16, 3, 1, 2};
    mos_vImageVerticalReflect_Planar16U(&vs, &vs, 0);
    CK(v16[0] == 30 && v16[1] == 20 && v16[2] == 10, "VerticalReflect Planar16U");

    /* rotação de 0 rad = cópia */
    uint8_t r4[16], r4o[16];
    for (int i = 0; i < 16; i++) r4[i] = (uint8_t)(i * 10);
    vimg_t rs = {r4, 2, 2, 8}, rd = {r4o, 2, 2, 8};
    mos_vImageRotate_ARGB8888(&rs, &rd, 0, 0, 0, 0);
    CK(!memcmp(r4, r4o, 16), "Rotate ARGB8888 0 rad = cópia");
    /* 180°: o primeiro pixel vai para o último */
    mos_vImageRotate_ARGB8888(&rs, &rd, 0, (float)M_PI, 0, 0);
    CK(!memcmp(r4o, r4 + 12, 4) && !memcmp(r4o + 12, r4, 4), "Rotate ARGB8888 180°");

    /* composição: topo opaco cobre; topo transparente deixa o fundo */
    uint8_t top[8] = {10, 20, 30, 255, 0, 0, 0, 0}, bot[8] = {100, 100, 100, 255, 50, 60, 70, 255}, out[8];
    vimg_t ts = {top, 1, 2, 8}, bs = {bot, 1, 2, 8}, os_ = {out, 1, 2, 8};
    mos_vImagePremultipliedAlphaBlend_BGRA8888(&ts, &bs, &os_, 0);
    CK(!memcmp(out, top, 4) && !memcmp(out + 4, bot + 4, 4), "AlphaBlend BGRA8888");
    uint8_t mt[4] = {128, 255, 0, 255}, mb[4] = {255, 128, 255, 255}, mo[4];
    vimg_t mts = {mt, 1, 1, 4}, mbs = {mb, 1, 1, 4}, mos = {mo, 1, 1, 4};
    mos_vImagePremultipliedAlphaBlendMultiply_RGBA8888(&mts, &mbs, &mos, 0);
    CK(mo[0] == 128 && mo[1] == 128 && mo[2] == 0 && mo[3] == 255, "AlphaBlend Multiply (opacos = produto)");
    mos_vImagePremultipliedAlphaBlendScreen_RGBA8888(&mts, &mbs, &mos, 0);
    CK(mo[0] == 255 && mo[1] == 255 && mo[2] == 255, "AlphaBlend Screen");

    /* máximo 3x3 e convolução de média */
    uint8_t g[9 * 4] = {0}, go[9 * 4];
    g[4 * 4 + 1] = 90;                                 /* canal R do pixel do meio */
    vimg_t gs = {g, 3, 3, 12}, gd = {go, 3, 3, 12};
    mos_vImageMax_ARGB8888(&gs, &gd, 0, 0, 0, 3, 3, 0);
    int all = 1;
    for (int i = 0; i < 9; i++) if (go[i * 4 + 1] != 90) all = 0;
    CK(all, "Max ARGB8888 3x3 espalha o pico");
    int16_t k[9] = {1, 1, 1, 1, 1, 1, 1, 1, 1};
    mos_vImageConvolve_ARGB8888(&gs, &gd, 0, 0, 0, k, 3, 3, 9, 0, 8 /* EdgeExtend */);
    CK(go[4 * 4 + 1] == 10, "Convolve ARGB8888 média 3x3");

    /* RGB888 -> BGRA com alfa constante */
    uint8_t rgb[3] = {1, 2, 3}, bgra[4];
    vimg_t rgbs = {rgb, 1, 1, 3}, bgras = {bgra, 1, 1, 4};
    mos_vImageConvert_RGB888toBGRA8888(&rgbs, 0, 200, &bgras, 0, 0);
    CK(bgra[0] == 3 && bgra[1] == 2 && bgra[2] == 1 && bgra[3] == 200, "RGB888 -> BGRA8888");
    uint16_t hf[2] = {0x3c00, 0x3800};                 /* 1.0, 0.5 */
    uint8_t h8[2];
    vimg_t hfs = {hf, 1, 2, 4}, h8s = {h8, 1, 2, 2};
    mos_vImageConvert_Planar16FtoPlanar8(&hfs, &h8s, 0);
    CK(h8[0] == 255 && h8[1] == 128, "Planar16F -> Planar8");
    uint16_t a16[4] = {65535, 0, 32768, 65535};
    uint8_t a8[4];
    vimg_t a16s = {a16, 1, 1, 8}, a8s = {a8, 1, 1, 4};
    uint8_t map[4] = {0, 1, 2, 3};
    mos_vImageConvert_ARGB16UToARGB8888(&a16s, &a8s, map, 0, 0, 0);
    CK(a8[0] == 255 && a8[1] == 0 && a8[2] == 128 && a8[3] == 255, "ARGB16U -> ARGB8888");

    /* warp afim: translação de +1 em x */
    uint8_t w[4] = {10, 20, 30, 40}, wo[4];
    vimg_t ws = {w, 1, 4, 4}, wd = {wo, 1, 4, 4};
    double t[6] = {1, 0, 0, 1, 1, 0};
    mos_vImageAffineWarpCG_Planar8(&ws, &wd, 0, t, 7, 0);
    CK(wo[0] == 7 && wo[1] == 10 && wo[3] == 30, "AffineWarpCG translação");

    /* CVPixelBuffer BGRA -> buffer RGBA (pré-multiplicado último, big) e de volta */
    for (int y = 0; y < 2; y++) for (int x = 0; x < 3; x++) { uint8_t *p = pbmem + y * 16 + x * 4; p[0] = 10; p[1] = 20; p[2] = 30; p[3] = 255; }
    vimg_fmt_t rgba = {8, 32, 0, 1, 0, 0, 0};
    vimg_t cv = {0};
    long e = mos_vImageBuffer_InitWithCVPixelBuffer(&cv, &rgba, (void *)1, 0, 0, 0);
    CK(e == 0 && cv.width == 3 && cv.height == 2, "InitWithCVPixelBuffer aloca");
    uint8_t *p0 = cv.data;
    CK(e == 0 && p0[0] == 30 && p0[1] == 20 && p0[2] == 10 && p0[3] == 255, "InitWithCVPixelBuffer BGRA -> RGBA");
    if (!e) { p0[0] = 99; e = mos_vImageBuffer_CopyToCVPixelBuffer(&cv, &rgba, (void *)1, 0, 0, 0); }
    CK(e == 0 && pbmem[2] == 99 && pbmem[0] == 10, "CopyToCVPixelBuffer RGBA -> BGRA");
    free(cv.data);

    /* FFT complexa: impulso -> constante; ida e volta = N * x */
    float re[8] = {1, 0, 0, 0, 0, 0, 0, 0}, im[8] = {0};
    split_t z = {re, im};
    mos_vDSP_fft_zip(0, &z, 1, 3, 1);
    int flat = 1;
    for (int i = 0; i < 8; i++) if (fabsf(re[i] - 1) > 1e-5f || fabsf(im[i]) > 1e-5f) flat = 0;
    CK(flat, "fft_zip impulso -> espectro plano");
    double dr[4] = {1, 2, 3, 4}, di[4] = {0}, orr[4], oi[4];
    splitd_t za = {dr, di}, zc = {orr, oi};
    mos_vDSP_fft_zopD(0, &za, 1, &zc, 1, 2, 1);
    CK(fabs(orr[0] - 10) < 1e-9 && fabs(orr[2] + 2) < 1e-9 && fabs(orr[1] + 2) < 1e-9 && fabs(oi[1] - 2) < 1e-9, "fft_zopD de [1 2 3 4]");
    mos_vDSP_fft_zopD(0, &zc, 1, &zc, 1, 2, -1);
    CK(fabs(orr[3] / 4 - 4) < 1e-9 && fabs(oi[3]) < 1e-9, "fft_zopD ida e volta");

    double x[3] = {5, 7, 9}, y[3] = {1, 2, 3}, zz[3];
    mos_vDSP_vsubD(y, 1, x, 1, zz, 1, 3);
    CK(zz[0] == 4 && zz[2] == 6, "vsubD = A - B");
    double m[6] = {1, 2, 3, 4, 5, 6}, mc[4];
    mos_vDSP_mmovD(m, mc, 2, 2, 3, 2);
    CK(mc[0] == 1 && mc[1] == 2 && mc[2] == 4 && mc[3] == 5, "mmovD submatriz");
    float fa[2] = {1, -5}, fb[2] = {4, -1}, fd;
    mos_vDSP_distancesq(fa, 1, fb, 1, &fd, 2);
    CK(fd == 25, "distancesq");
    mos_vDSP_minmgv(fa, 1, &fd, 2);
    CK(fd == 1, "minmgv");

    printf("accelerate_corpus2: %d/%d ok\n", total - fails, total);
    return fails != 0;
}
