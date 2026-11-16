/* Host: filtros novos do coreimage_cpu.c contra contas feitas aqui (fórmulas do W3C Compositing and Blending, sRGB,
 * matrizes) e o extent infinito (geradores, clamp). Rodado por tools/test-coreimage-filtros.sh.
 * → "coreimage filtros: N/N ok" */
#include "coreimage_cpu.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails, total;
static void expect(int cond, const char *msg) { total++; if (!cond) { printf("FALHOU: %s\n", msg); fails++; } }
static int ch(uint32_t p, int i) { return (int)(p >> (24 - 8 * i)) & 255; }   /* 0 A, 1 R, 2 G, 3 B */
static int near(uint32_t p, int a, int r, int g, int b, int tol) {
    int v[4] = {a, r, g, b};
    for (int i = 0; i < 4; i++) if (abs(ch(p, i) - v[i]) > tol) return 0;
    return 1;
}
static uint32_t px(int a, int r, int g, int b) { return (uint32_t)a << 24 | (uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b; }
static int q(double v) { v = v < 0 ? 0 : v > 1 ? 1 : v; return (int)(v * 255 + 0.5); }
static uint32_t one(mos_ci *im, double x, double y) { uint32_t o = 0; if (mos_ci_render(im, x, y, 1, 1, &o)) return 0xdeadbeef; return o; }
static double lin(double v) { return v <= 0.04045 ? v / 12.92 : pow((v + 0.055) / 1.055, 2.4); }
static double srgb(double v) { return v <= 0.0031308 ? v * 12.92 : 1.055 * pow(v, 1 / 2.4) - 0.055; }

int main(void) {
    uint32_t src1 = px(255, 200, 100, 50);
    mos_ci *im = mos_ci_bitmap(&src1, 1, 1, 0, 0);
    double r = 200 / 255.0, g = 100 / 255.0, b = 50 / 255.0;

    /* operações por pixel */
    double M[20] = {0, 0, 1, 0,  0, 1, 0, 0,  1, 0, 0, 0,  0, 0, 0, 1,  0.1, 0, 0, 0};   /* troca R e B, +0,1 no R */
    expect(near(one(mos_ci_pixop(im, MOS_CI_OP_MATRIX, M, 20), 0, 0), 255, q(b + 0.1), 100, 200, 0), "CIColorMatrix");
    expect(near(one(mos_ci_pixop(im, MOS_CI_OP_INVERT, 0, 0), 0, 0), 255, 55, 155, 205, 0), "CIColorInvert");
    double one_[1] = {1};
    expect(near(one(mos_ci_pixop(im, MOS_CI_OP_SEPIA, one_, 1), 0, 0), 255, q(0.393 * r + 0.769 * g + 0.189 * b),
                q(0.349 * r + 0.686 * g + 0.168 * b), q(0.272 * r + 0.534 * g + 0.131 * b), 0), "CISepiaTone 1,0");
    double gp[1] = {2};
    expect(near(one(mos_ci_pixop(im, MOS_CI_OP_GAMMA, gp, 1), 0, 0), 255, q(r * r), q(g * g), q(b * b), 0), "CIGammaAdjust 2");
    double lv[1] = {2};
    expect(near(one(mos_ci_pixop(im, MOS_CI_OP_POSTERIZE, lv, 1), 0, 0), 255, q(floor(r * 2 + 0.5) / 2), q(floor(g * 2 + 0.5) / 2), q(floor(b * 2 + 0.5) / 2), 0), "CIColorPosterize 2");
    double ev[1] = {1};
    expect(near(one(mos_ci_pixop(im, MOS_CI_OP_EXPOSURE, ev, 1), 0, 0), 255, q(srgb(lin(r) * 2)), q(srgb(lin(g) * 2)), q(srgb(lin(b) * 2)), 0), "CIExposureAdjust +1 EV (linear)");
    double hz[1] = {0};
    expect(near(one(mos_ci_pixop(im, MOS_CI_OP_HUE, hz, 1), 0, 0), 255, 200, 100, 50, 1), "CIHueAdjust 0 é identidade");
    double L = 0.2125 * r + 0.7154 * g + 0.0721 * b;
    expect(near(one(mos_ci_pixop(im, MOS_CI_OP_MASK_TO_ALPHA, 0, 0), 0, 0), q(L), 255, 255, 255, 0), "CIMaskToAlpha");
    expect(near(one(mos_ci_pixop(im, MOS_CI_OP_MIN_COMPONENT, 0, 0), 0, 0), 255, 50, 50, 50, 0) &&
           near(one(mos_ci_pixop(im, MOS_CI_OP_MAX_COMPONENT, 0, 0), 0, 0), 255, 200, 200, 200, 0), "CIMinimumComponent/CIMaximumComponent");
    double cl[8] = {0, 0.5, 0, 0, 0.5, 1, 1, 1};
    expect(near(one(mos_ci_pixop(im, MOS_CI_OP_CLAMP_COMPONENTS, cl, 8), 0, 0), 255, 128, 128, 50, 0), "CIColorClamp");

    /* misturas: fundo 50% de alfa */
    uint32_t fgp = px(255, 200, 100, 50), bgp = px(128, 100, 200, 250);
    mos_ci *fg = mos_ci_bitmap(&fgp, 1, 1, 0, 0), *bg = mos_ci_bitmap(&bgp, 1, 1, 0, 0);
    double as = 1, ab = 128 / 255.0, cs[3] = {r, g, b}, cb[3] = {100 / 255.0, 200 / 255.0, 250 / 255.0};
    {
        double ao = as + ab - as * ab, o[3];
        for (int i = 0; i < 3; i++) o[i] = (cs[i] * as * (1 - ab) + cb[i] * ab * (1 - as) + as * ab * cb[i] * cs[i]) / ao;
        expect(near(one(mos_ci_blend(fg, bg, 0, MOS_CI_BL_MULTIPLY, 0), 0, 0), q(ao), q(o[0]), q(o[1]), q(o[2]), 1), "CIMultiplyBlendMode (W3C, alfa)");
        for (int i = 0; i < 3; i++) o[i] = (cs[i] * as * (1 - ab) + cb[i] * ab * (1 - as) + as * ab * fabs(cb[i] - cs[i])) / ao;
        expect(near(one(mos_ci_blend(fg, bg, 0, MOS_CI_BL_DIFFERENCE, 0), 0, 0), q(ao), q(o[0]), q(o[1]), q(o[2]), 1), "CIDifferenceBlendMode");
        for (int i = 0; i < 3; i++) o[i] = (cs[i] * as * (1 - ab) + cb[i] * ab * (1 - as) + as * ab * (cb[i] + cs[i] - cb[i] * cs[i])) / ao;
        expect(near(one(mos_ci_blend(fg, bg, 0, MOS_CI_BL_SCREEN, 0), 0, 0), q(ao), q(o[0]), q(o[1]), q(o[2]), 1), "CIScreenBlendMode");
    }
    expect(near(one(mos_ci_blend(fg, bg, 0, MOS_CI_BL_SOURCE_IN, 0), 0, 0), q(as * ab), 200, 100, 50, 1), "CISourceInCompositing");
    expect(near(one(mos_ci_blend(fg, bg, 0, MOS_CI_BL_SOURCE_OUT, 0), 0, 0), q(as * (1 - ab)), 200, 100, 50, 1), "CISourceOutCompositing");
    expect(near(one(mos_ci_blend(fg, bg, 0, MOS_CI_BL_SOURCE_ATOP, 0), 0, 0), q(ab), 200, 100, 50, 1), "CISourceAtopCompositing");
    uint32_t mp = px(255, 128, 128, 128);
    mos_ci *mask = mos_ci_bitmap(&mp, 1, 1, 0, 0);
    {
        double k = (0.2125 + 0.7154 + 0.0721) * 128 / 255.0, ao = ab + (as - ab) * k, o[3];
        for (int i = 0; i < 3; i++) o[i] = (cb[i] * ab + (cs[i] * as - cb[i] * ab) * k) / ao;
        expect(near(one(mos_ci_blend(fg, bg, mask, MOS_CI_BL_MASK, 0), 0, 0), q(ao), q(o[0]), q(o[1]), q(o[2]), 1), "CIBlendWithMask");
    }

    /* geradores e extent infinito */
    double cc[4] = {0.2, 0.4, 0.6, 1};
    mos_ci *k = mos_ci_generator(MOS_CI_GEN_CONSTANT, cc, 4);
    expect(mos_ci_is_infinite(k), "CIConstantColorGenerator é infinito");
    uint32_t out[16];
    expect(mos_ci_render(k, -1000, 5000, 4, 4, out) == 0 && near(out[15], 255, q(0.2), q(0.4), q(0.6), 0), "cor constante em qualquer lugar");
    double ex, ey, ew, eh;
    mos_ci *kc = mos_ci_crop(k, 10, 20, 30, 40);
    expect(mos_ci_extent(kc, &ex, &ey, &ew, &eh) && !mos_ci_is_infinite(kc) && ex == 10 && ey == 20 && ew == 30 && eh == 40, "corte do infinito é finito");
    mos_ci *ov = mos_ci_over(fg, k);
    expect(mos_ci_is_infinite(ov) && near(one(ov, 0, 0), 255, 200, 100, 50, 0) && near(one(ov, 7, 7), 255, q(0.2), q(0.4), q(0.6), 0), "source-over sobre infinito");
    double lg[12] = {0, 0, 10, 0, 1, 0, 0, 1, 0, 0, 1, 1};
    mos_ci *lin_ = mos_ci_generator(MOS_CI_GEN_LINEAR, lg, 12);
    expect(near(one(lin_, 4, 0), 255, q(1 - 0.45), 0, q(0.45), 1) && near(one(lin_, -5, 3), 255, 255, 0, 0, 0) && near(one(lin_, 50, 3), 255, 0, 0, 255, 0), "CILinearGradient");
    double rg[12] = {0, 0, 0, 10, 1, 1, 1, 1, 0, 0, 0, 1};
    mos_ci *rad = mos_ci_generator(MOS_CI_GEN_RADIAL, rg, 12);
    double d = sqrt(3.5 * 3.5 + 0.5 * 0.5) / 10;
    expect(near(one(rad, 3, 0), 255, q(1 - d), q(1 - d), q(1 - d), 1), "CIRadialGradient");
    double ck[12] = {0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 8, 1};
    mos_ci *chk = mos_ci_generator(MOS_CI_GEN_CHECKER, ck, 12);
    expect(near(one(chk, 3, 3), 255, 255, 255, 255, 0) && near(one(chk, 11, 3), 255, 0, 0, 0, 0) && near(one(chk, -3, 3), 255, 0, 0, 0, 0), "CICheckerboardGenerator");

    /* clamp + blur + corte (o padrão dos apps): a borda não fica transparente */
    uint32_t sq[16];
    for (int i = 0; i < 16; i++) sq[i] = px(255, 20 * i, 255 - 10 * i, 128);
    mos_ci *s4 = mos_ci_bitmap(sq, 4, 4, 0, 0);
    mos_ci *cbl = mos_ci_crop(mos_ci_blur(mos_ci_clamp(s4), 2), 0, 0, 4, 4), *pbl = mos_ci_crop(mos_ci_blur(s4, 2), 0, 0, 4, 4);
    uint32_t o1[16], o2[16];
    int opaque = mos_ci_render(cbl, 0, 0, 4, 4, o1) == 0, lowedge = mos_ci_render(pbl, 0, 0, 4, 4, o2) == 0 && ch(o2[0], 0) < 250;
    for (int i = 0; i < 16; i++) if (ch(o1[i], 0) != 255) opaque = 0;
    expect(opaque && lowedge, "imageByClampingToExtent + CIGaussianBlur + corte: borda opaca");

    /* box blur de um ponto branco: 3x3 com 1/9 */
    uint32_t w1 = px(255, 255, 255, 255);
    mos_ci *dot = mos_ci_bitmap(&w1, 1, 1, 0, 0), *bx = mos_ci_box_blur(dot, 1);
    expect(mos_ci_extent(bx, &ex, &ey, &ew, &eh) && ex == -1 && ew == 3 && near(one(bx, -1, -1), q(1 / 9.0), 255, 255, 255, 0) && near(one(bx, 0, 0), q(1 / 9.0), 255, 255, 255, 0), "CIBoxBlur");

    /* pixellate: casa de 2 tomando a cor do centro */
    mos_ci *pxl = mos_ci_pixellate(s4, 0, 0, 2);
    expect(one(pxl, 0, 0) == sq[1 * 4 + 1] && one(pxl, 1, 1) == sq[1 * 4 + 1] && one(pxl, 2, 3) == sq[3 * 4 + 3], "CIPixellate");

    /* unsharp em área lisa não muda; num degrau aumenta o contraste */
    uint32_t flat[25], step[8];
    for (int i = 0; i < 25; i++) flat[i] = px(255, 90, 90, 90);
    for (int i = 0; i < 8; i++) step[i] = i < 4 ? px(255, 60, 60, 60) : px(255, 180, 180, 180);
    mos_ci *fl = mos_ci_bitmap(flat, 5, 5, 0, 0), *st = mos_ci_bitmap(step, 8, 1, 0, 0);
    mos_ci *us = mos_ci_blend(fl, mos_ci_blur(mos_ci_clamp(fl), 1), 0, MOS_CI_BL_UNSHARP, 0.5);
    mos_ci *us2 = mos_ci_blend(st, mos_ci_blur(mos_ci_clamp(st), 1), 0, MOS_CI_BL_UNSHARP, 1.0);
    expect(near(one(us, 2, 2), 255, 90, 90, 90, 0) && ch(one(us2, 3, 0), 1) < 60 && ch(one(us2, 4, 0), 1) > 180, "CIUnsharpMask");

    /* regressão: transladar não pode mexer na origem da imagem de origem */
    mos_ci *tr = mos_ci_affine(s4, 1, 0, 0, 1, 5, 0);
    uint32_t t1[16];
    mos_ci_render(tr, 5, 0, 4, 4, t1);
    expect(mos_ci_extent(s4, &ex, &ey, &ew, &eh) && ex == 0 && one(s4, 0, 0) == sq[0] && t1[0] == sq[0], "translação não altera a imagem de origem");
    /* afim de um infinito continua infinito e desenha */
    mos_ci *ka = mos_ci_affine(k, 2, 0, 0, 2, 3, 3);
    expect(mos_ci_is_infinite(ka) && near(one(ka, 100, -50), 255, q(0.2), q(0.4), q(0.6), 0), "afim de imagem infinita");

    printf("coreimage filtros: %d/%d ok\n", total - fails, total);
    return fails != 0;
}
