/* Os 28 modos de CGBlendMode, com a numeração da Apple, sobre ARGB pré-multiplicado.
 *  0 Normal  1 Multiply  2 Screen  3 Overlay  4 Darken  5 Lighten  6 ColorDodge  7 ColorBurn
 *  8 SoftLight  9 HardLight  10 Difference  11 Exclusion  12 Hue  13 Saturation  14 Color  15 Luminosity
 *  16 Clear  17 Copy  18 SourceIn  19 SourceOut  20 SourceAtop  21 DestinationOver  22 DestinationIn
 *  23 DestinationOut  24 DestinationAtop  25 XOR  26 PlusDarker  27 PlusLighter
 * Os separáveis e os não separáveis seguem as fórmulas da especificação W3C Compositing and Blending:
 * co = cs·(1−αb) + cb·(1−αs) + αs·αb·B(Cb, Cs), αo = αs + αb − αs·αb. */
#pragma once
#include <stdint.h>

static inline uint32_t cx_cg_over(uint32_t s, uint32_t d) {
    uint32_t a = s >> 24;
    if (a == 255) return s;
    if (!a) return d;
    uint32_t k = 255 - a;
    uint32_t rb = (((d & 0xFF00FFu) * k + 0x800080u) >> 8) & 0xFF00FFu;
    uint32_t ag = ((((d >> 8) & 0xFF00FFu) * k + 0x800080u) & 0xFF00FF00u);
    return s + (rb | ag);
}

/* modos em que uma fonte transparente ainda muda o destino */
static inline int cx_cg_blend_zero_src_matters(int mode) {
    return mode == 16 || mode == 17 || mode == 18 || mode == 19 || mode == 22 || mode == 24;
}

static inline double cx_cg_clamp01(double v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

static inline double cx_cg_sep(int mode, double b, double s) {
    switch (mode) {
    case 1: return b * s;
    case 2: return b + s - b * s;
    case 3: return b <= 0.5 ? 2 * s * b : 1 - 2 * (1 - s) * (1 - b);   /* overlay = hardlight com os papéis trocados */
    case 4: return b < s ? b : s;
    case 5: return b > s ? b : s;
    case 6: if (b == 0) return 0; if (s >= 1) return 1; { double v = b / (1 - s); return v > 1 ? 1 : v; }
    case 7: if (b >= 1) return 1; if (s <= 0) return 0; { double v = 1 - (1 - b) / s; return v < 0 ? 0 : v; }
    case 8: {
        double d = b <= 0.25 ? ((16 * b - 12) * b + 4) * b : __builtin_sqrt(b);
        return s <= 0.5 ? b - (1 - 2 * s) * b * (1 - b) : b + (2 * s - 1) * (d - b);
    }
    case 9: return s <= 0.5 ? 2 * s * b : 1 - 2 * (1 - s) * (1 - b);
    case 10: return b > s ? b - s : s - b;
    case 11: return b + s - 2 * b * s;
    default: return s;
    }
}

/* não separáveis (Hue, Saturation, Color, Luminosity) */
static inline double cx_cg_lum(const double *c) { return 0.3 * c[0] + 0.59 * c[1] + 0.11 * c[2]; }
static inline void cx_cg_clipcolor(double *c) {
    double l = cx_cg_lum(c), n = c[0], x = c[0];
    for (int i = 1; i < 3; i++) { if (c[i] < n) n = c[i]; if (c[i] > x) x = c[i]; }
    for (int i = 0; i < 3; i++) {
        if (n < 0) c[i] = l + (c[i] - l) * l / (l - n);
        if (x > 1) c[i] = l + (c[i] - l) * (1 - l) / (x - l);
    }
}
static inline void cx_cg_setlum(double *c, double l) {
    double d = l - cx_cg_lum(c);
    for (int i = 0; i < 3; i++) c[i] += d;
    cx_cg_clipcolor(c);
}
static inline double cx_cg_sat(const double *c) {
    double n = c[0], x = c[0];
    for (int i = 1; i < 3; i++) { if (c[i] < n) n = c[i]; if (c[i] > x) x = c[i]; }
    return x - n;
}
static inline void cx_cg_setsat(double *c, double s) {
    int mx = 0, mn = 0;
    for (int i = 1; i < 3; i++) { if (c[i] > c[mx]) mx = i; if (c[i] < c[mn]) mn = i; }
    if (mx == mn) { c[0] = c[1] = c[2] = 0; return; }
    int md = 3 - mx - mn;
    c[md] = (c[md] - c[mn]) * s / (c[mx] - c[mn]);
    c[mx] = s;
    c[mn] = 0;
}

static inline uint32_t cx_cg_pack(double a, const double *c) {
    uint32_t A = (uint32_t)(cx_cg_clamp01(a) * 255 + 0.5);
    uint32_t o = A << 24;
    for (int i = 0; i < 3; i++) {
        uint32_t v = (uint32_t)(cx_cg_clamp01(c[i]) * 255 + 0.5);
        if (v > A) v = A;
        o |= v << (16 - 8 * i);
    }
    return o;
}

static inline uint32_t cx_cg_blend_px(int mode, uint32_t src, uint32_t dst) {
    if (mode == 0) return cx_cg_over(src, dst);
    if (mode == 16) return 0;
    if (mode == 17) return src;
    double as = (src >> 24) / 255.0, ab = (dst >> 24) / 255.0;
    double s[3], d[3];                                   /* pré-multiplicados, 0..1 */
    for (int i = 0; i < 3; i++) { s[i] = ((src >> (16 - 8 * i)) & 255) / 255.0; d[i] = ((dst >> (16 - 8 * i)) & 255) / 255.0; }
    double o[3], ao;
    if (mode >= 18 && mode <= 25) {                      /* Porter-Duff: Fa·S + Fb·D */
        double fa, fb;
        switch (mode) {
        case 18: fa = ab; fb = 0; break;                 /* SourceIn */
        case 19: fa = 1 - ab; fb = 0; break;             /* SourceOut */
        case 20: fa = ab; fb = 1 - as; break;            /* SourceAtop */
        case 21: fa = 1 - ab; fb = 1; break;             /* DestinationOver */
        case 22: fa = 0; fb = as; break;                 /* DestinationIn */
        case 23: fa = 0; fb = 1 - as; break;             /* DestinationOut */
        case 24: fa = 1 - ab; fb = as; break;            /* DestinationAtop */
        default: fa = 1 - ab; fb = 1 - as; break;        /* XOR */
        }
        for (int i = 0; i < 3; i++) o[i] = fa * s[i] + fb * d[i];
        ao = fa * as + fb * ab;
        return cx_cg_pack(ao, o);
    }
    if (mode == 26) {                                    /* PlusDarker: 1 − min(1, (1−S)+(1−D)) por canal, com alfa somado */
        ao = as + ab > 1 ? 1 : as + ab;
        for (int i = 0; i < 3; i++) {
            double v = ao - ((as - s[i]) + (ab - d[i]));
            o[i] = v < 0 ? 0 : v;
        }
        return cx_cg_pack(ao, o);
    }
    if (mode == 27) {                                    /* PlusLighter: soma saturada */
        ao = as + ab > 1 ? 1 : as + ab;
        for (int i = 0; i < 3; i++) o[i] = s[i] + d[i] > 1 ? 1 : s[i] + d[i];
        return cx_cg_pack(ao, o);
    }
    /* separáveis e não separáveis: trabalham na cor reta */
    double cs[3], cb[3], bl[3];
    for (int i = 0; i < 3; i++) { cs[i] = as > 0 ? s[i] / as : 0; cb[i] = ab > 0 ? d[i] / ab : 0; }
    if (mode <= 11) {
        for (int i = 0; i < 3; i++) bl[i] = cx_cg_sep(mode, cb[i], cs[i]);
    } else {
        double t[3] = {cs[0], cs[1], cs[2]};
        switch (mode) {
        case 12: cx_cg_setsat(t, cx_cg_sat(cb)); cx_cg_setlum(t, cx_cg_lum(cb)); break;                       /* Hue */
        case 13: t[0] = cb[0]; t[1] = cb[1]; t[2] = cb[2]; cx_cg_setsat(t, cx_cg_sat(cs)); cx_cg_setlum(t, cx_cg_lum(cb)); break;
        case 14: cx_cg_setlum(t, cx_cg_lum(cb)); break;                                                      /* Color */
        default: t[0] = cb[0]; t[1] = cb[1]; t[2] = cb[2]; cx_cg_setlum(t, cx_cg_lum(cs)); break;            /* Luminosity */
        }
        bl[0] = t[0]; bl[1] = t[1]; bl[2] = t[2];
    }
    ao = as + ab - as * ab;
    for (int i = 0; i < 3; i++) o[i] = s[i] * (1 - ab) + d[i] * (1 - as) + as * ab * cx_cg_clamp01(bl[i]);
    return cx_cg_pack(ao, o);
}
