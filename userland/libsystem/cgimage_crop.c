/*
 * Recorte de CGImageCreateWithImageInRect: CGRectIntegral, interseção com
 * [0,largura) × [0,altura) e cópia dos pixels. bytesPerRow do resultado é o da origem.
 * Sem ObjC: coregraphics.c monta o objeto CGImage em volta disto.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static double cg_floor(double v) {
    if (v != v) return v;
    long t = (long)v;
    if ((double)t == v) return v;
    return v > 0 ? (double)t : (double)(t - 1);
}
static double cg_ceil(double v) {
    if (v != v) return v;
    double f = cg_floor(v);
    return f == v ? v : f + 1.0;
}

int cgimage_crop(const uint8_t *src, uint32_t sw, uint32_t sh, uint32_t sbpr, uint32_t byte_pp,
                 double x, double y, double w, double h,
                 uint8_t **dst, uint32_t *dw, uint32_t *dh, uint32_t *dbpr) {
    if (dst) *dst = 0;
    if (!src || !dst || !dw || !dh || !dbpr || !sw || !sh || !sbpr || !byte_pp) return 0;
    if (w < 0) { x += w; w = -w; }
    if (h < 0) { y += h; h = -h; }
    if (!(w > 0.0) || !(h > 0.0)) return 0;
    double x1 = cg_floor(x), y1 = cg_floor(y), x2 = cg_ceil(x + w), y2 = cg_ceil(y + h);
    if (x1 != x1 || y1 != y1 || x2 != x2 || y2 != y2) return 0;
    long ix1 = (long)x1, iy1 = (long)y1, ix2 = (long)x2, iy2 = (long)y2;
    if (ix1 < 0) ix1 = 0;
    if (iy1 < 0) iy1 = 0;
    if (ix2 > (long)sw) ix2 = (long)sw;
    if (iy2 > (long)sh) iy2 = (long)sh;
    if (ix2 <= ix1 || iy2 <= iy1) return 0;
    uint32_t ow = (uint32_t)(ix2 - ix1), oh = (uint32_t)(iy2 - iy1);
    if ((uint64_t)ix1 * byte_pp + (uint64_t)ow * byte_pp > sbpr) return 0;
    if ((uint64_t)sbpr * oh > (32u << 20)) return 0;
    uint8_t *d = malloc((size_t)sbpr * oh);
    if (!d) return 0;
    memset(d, 0, (size_t)sbpr * oh);
    for (uint32_t row = 0; row < oh; row++) {
        memcpy(d + (size_t)row * sbpr,
               src + (size_t)(iy1 + row) * sbpr + (size_t)ix1 * byte_pp,
               (size_t)ow * byte_pp);
    }
    *dst = d;
    *dw = ow;
    *dh = oh;
    *dbpr = sbpr;
    return 1;
}
