/* Modos de CGContextSetBlendMode que mudam o pixel. O resto cai no Normal (source-over).
 * 0 normal, 1 copy, 2 multiply, 3 screen, 16 clear. */
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

static inline uint32_t cx_cg_mul_ch(uint32_t s, uint32_t d) { return (s * d) / 255u; }

static inline uint32_t cx_cg_screen_ch(uint32_t s, uint32_t d) {
    return 255u - ((255u - s) * (255u - d)) / 255u;
}

static inline uint32_t cx_cg_blend_px(int mode, uint32_t src, uint32_t dst) {
    if (mode == 16) return 0;
    if (mode == 1) return src;
    if (mode == 2) {
        return cx_cg_mul_ch(src >> 24, dst >> 24) << 24 |
               cx_cg_mul_ch((src >> 16) & 255u, (dst >> 16) & 255u) << 16 |
               cx_cg_mul_ch((src >> 8) & 255u, (dst >> 8) & 255u) << 8 |
               cx_cg_mul_ch(src & 255u, dst & 255u);
    }
    if (mode == 3) {
        return cx_cg_screen_ch(src >> 24, dst >> 24) << 24 |
               cx_cg_screen_ch((src >> 16) & 255u, (dst >> 16) & 255u) << 16 |
               cx_cg_screen_ch((src >> 8) & 255u, (dst >> 8) & 255u) << 8 |
               cx_cg_screen_ch(src & 255u, dst & 255u);
    }
    return cx_cg_over(src, dst);
}
