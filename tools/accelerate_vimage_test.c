/* Host: userland/libsystem/accelerate_vimage.c contra valores feitos à mão. → "accelerate_vimage: N/N ok" */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
typedef struct { void *data; unsigned long height, width, rowBytes; } vb;
typedef struct { uint32_t bpc, bpp; void *space; uint32_t bitmap, version; const double *decode; int intent; } vf;
typedef struct { double a, b, c, d, tx, ty; } vaff;
typedef struct { float Yp, Cr_R, Cr_G, Cb_G, Cb_B; } ycm;
typedef struct { int32_t Yp_bias, CbCr_bias, YpRangeMax, CbCrRangeMax, YpMax, YpMin, CbCrMax, CbCrMin; } ycr;
long vImageBufferFill_ARGB8888(const vb *, const uint8_t *, unsigned), vImagePermuteChannels_ARGB8888(const vb *, const vb *, const uint8_t *, unsigned);
long vImageHorizontalReflect_ARGB8888(const vb *, const vb *, unsigned), vImagePremultiplyData_ARGB8888(const vb *, const vb *, unsigned);
long vImageUnpremultiplyData_ARGB8888(const vb *, const vb *, unsigned), vImagePremultiplyData_Planar8(const vb *, const vb *, const vb *, unsigned);
long vImageLookupTable_Planar8toPlanarF(const vb *, const vb *, const float *, unsigned), vImageConvert_16Uto16F(const vb *, const vb *, unsigned);
long vImageConvolve_Planar8(const vb *, const vb *, void *, unsigned long, unsigned long, const int16_t *, uint32_t, uint32_t, int32_t, uint8_t, unsigned);
long vImageAffineWarpCG_ARGB8888(const vb *, const vb *, void *, const vaff *, const uint8_t *, unsigned);
long vImageConvert_YpCbCrToARGB_GenerateConversion(const ycm *, const ycr *, void *, int, int, unsigned);
long vImageConvert_420Yp8_Cb8_Cr8ToARGB8888(const vb *, const vb *, const vb *, const vb *, const void *, const uint8_t *, uint8_t, unsigned);
extern const ycm *const kvImage_YpCbCrToARGBMatrix_ITU_R_601_4;
void *vImageConverter_CreateWithCGImageFormat(const vf *, const vf *, const double *, unsigned, long *);
long vImageConvert_AnyToAny(void *, const vb *, const vb *, void *, unsigned);
static int fails, total;
static void expect(int c, const char *m) { total++; if (!c) { printf("FALHOU: %s\n", m); fails++; } }
static vb buf(unsigned w, unsigned h, unsigned bpp) { vb b = {calloc(h, w * bpp), h, w, w * bpp}; return b; }
int main(void) {
    vb a = buf(2, 1, 4);
    uint8_t col[4] = {255, 10, 20, 30};
    vImageBufferFill_ARGB8888(&a, col, 0);
    uint8_t map[4] = {3, 2, 1, 0};
    ((uint8_t *)a.data)[4] = 128;
    vImagePermuteChannels_ARGB8888(&a, &a, map, 0);
    uint8_t *p = a.data;
    expect(p[0] == 30 && p[1] == 20 && p[2] == 10 && p[3] == 255 && p[7] == 128, "preencher e permutar em lugar");
    vImageHorizontalReflect_ARGB8888(&a, &a, 0);
    expect(p[3] == 128 && p[7] == 255, "espelhar");
    uint8_t px[4] = {128, 200, 100, 50};
    vb one = {px, 1, 1, 4};
    vImagePremultiplyData_ARGB8888(&one, &one, 0);
    expect(px[0] == 128 && px[1] == (200 * 128 + 127) / 255 && px[3] == (50 * 128 + 127) / 255, "pré-multiplicar ARGB");
    vImageUnpremultiplyData_ARGB8888(&one, &one, 0);
    expect(abs(px[1] - 200) <= 1 && abs(px[3] - 50) <= 2, "desfazer a pré-multiplicação");
    uint8_t pl[2] = {200, 100}, al[2] = {255, 51}, po[2];
    vb ps = {pl, 1, 2, 2}, pa = {al, 1, 2, 2}, pd = {po, 1, 2, 2};
    vImagePremultiplyData_Planar8(&ps, &pa, &pd, 0);
    expect(po[0] == 200 && po[1] == 20, "pré-multiplicar Planar8");
    float tab[256]; for (int i = 0; i < 256; i++) tab[i] = i * 0.5f;
    float fo[2]; vb fd = {fo, 1, 2, 8};
    vImageLookupTable_Planar8toPlanarF(&ps, &fd, tab, 0);
    expect(fo[0] == 100 && fo[1] == 50, "tabela Planar8 -> PlanarF");
    uint16_t u[4] = {1, 1000, 2048, 65535}, h[4];
    vb us = {u, 1, 4, 8}, hd = {h, 1, 4, 8};
    vImageConvert_16Uto16F(&us, &hd, 0);
    expect(h[0] == 0x3c00 && h[1] == 0x63d0 && h[2] == 0x6800 && h[3] == 0x7c00, "16U -> 16F (65535 vira infinito)");
    vb g = buf(4, 4, 1), go = buf(4, 4, 1);
    for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) ((uint8_t *)g.data)[y * 4 + x] = (uint8_t)(10 * x + 40 * y);
    int16_t box[9] = {1, 1, 1, 1, 1, 1, 1, 1, 1};
    vImageConvolve_Planar8(&g, &go, 0, 0, 0, box, 3, 3, 9, 0, 8);
    uint8_t *o = go.data;
    expect(o[1 * 4 + 1] == 50 && o[0] == 17, "convolução 3x3 (interior = média; canto com borda estendida)");
    vImageConvolve_Planar8(&g, &go, 0, 0, 0, box, 3, 3, 9, 90, 4);
    expect(o[0] == (90 * 5 + 0 + 10 + 40 + 50 + 4) / 9, "convolução com cor de fundo");
    vb src = buf(3, 1, 4), dst = buf(3, 1, 4);
    for (int i = 0; i < 12; i++) ((uint8_t *)src.data)[i] = (uint8_t)(i * 10);
    vaff tr = {1, 0, 0, 1, 1, 0};
    uint8_t bg[4] = {9, 9, 9, 9};
    vImageAffineWarpCG_ARGB8888(&src, &dst, 0, &tr, bg, 4);
    uint8_t *dp = dst.data;
    expect(dp[0] == 9 && dp[4] == 0 && dp[8] == 40 && dp[11] == 70, "warp afim: translação de 1 pixel com fundo");
    ycr video = {16, 128, 235, 240, 235, 16, 240, 16};
    uint8_t info[128];
    vImageConvert_YpCbCrToARGB_GenerateConversion(kvImage_YpCbCrToARGBMatrix_ITU_R_601_4, &video, info, 0, 0, 0);
    uint8_t Y[4] = {235, 16, 81, 81}, Cb[1] = {128}, Cr[1] = {128}, out[16];
    vb yb = {Y, 2, 2, 2}, cbb = {Cb, 1, 1, 1}, crb = {Cr, 1, 1, 1}, ob = {out, 2, 2, 8};
    uint8_t argb[4] = {0, 1, 2, 3};
    vImageConvert_420Yp8_Cb8_Cr8ToARGB8888(&yb, &cbb, &crb, &ob, info, argb, 255, 0);
    expect(out[0] == 255 && out[1] == 255 && out[2] == 255 && out[3] == 255 && out[5] == 0 && out[7] == 0, "YpCbCr 601 vídeo: branco e preto");
    Cb[0] = 90; Cr[0] = 240;
    vImageConvert_420Yp8_Cb8_Cr8ToARGB8888(&yb, &cbb, &crb, &ob, info, argb, 255, 0);
    expect(out[9] >= 250 && out[10] <= 5 && out[11] <= 5, "YpCbCr 601 vídeo: vermelho (81, 90, 240)");
    /* tom médio, onde o croma não satura: (Y, Cb, Cr) = (132, 81, 169) é RGB (200, 120, 40) no BT.601 de vídeo */
    Y[2] = 132; Cb[0] = 81; Cr[0] = 169;
    vImageConvert_420Yp8_Cb8_Cr8ToARGB8888(&yb, &cbb, &crb, &ob, info, argb, 255, 0);
    expect(out[9] >= 197 && out[9] <= 203 && out[10] >= 117 && out[10] <= 123 && out[11] >= 37 && out[11] <= 43,
           "YpCbCr 601 vídeo: tom médio sem saturar (escala do croma)");
    vf rgba = {8, 32, 0, 3, 0, 0, 0}, bgra_pm = {8, 32, 0, 2 | 0x2000, 0, 0, 0}, rgb = {8, 24, 0, 0, 0, 0, 0}, gray = {8, 8, 0, 0, 0, 0, 0};
    long err = 1;
    void *cv = vImageConverter_CreateWithCGImageFormat(&rgba, &bgra_pm, 0, 0, &err);
    uint8_t s1[4] = {200, 100, 50, 128}, d1[4];
    vb sb = {s1, 1, 1, 4}, db = {d1, 1, 1, 4};
    vImageConvert_AnyToAny(cv, &sb, &db, 0, 0);
    expect(!err && cv && d1[0] == 25 && d1[1] == 50 && d1[2] == 100 && d1[3] == 128, "RGBA -> BGRA pré-multiplicado (32 little)");
    void *cg = vImageConverter_CreateWithCGImageFormat(&rgb, &gray, 0, 0, &err);
    uint8_t s2[3] = {255, 255, 255}, d2[1];
    vb sb2 = {s2, 1, 1, 3}, db2 = {d2, 1, 1, 1};
    vImageConvert_AnyToAny(cg, &sb2, &db2, 0, 0);
    expect(!err && d2[0] == 255, "RGB -> cinza");
    vf bad = {8, 16, 0, 0, 0, 0, 0};
    expect(!vImageConverter_CreateWithCGImageFormat(&bad, &rgba, 0, 0, &err) && err == -21783, "formato sem suporte -> kvImageUnsupportedConversion");
    printf("accelerate_vimage: %d/%d ok\n", total - fails, total);
    return fails != 0;
}
