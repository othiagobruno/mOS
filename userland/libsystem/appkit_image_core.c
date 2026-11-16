/* mOS — pixels do NSImage (ver appkit_image_core.h). Teste de host: -DCX_IMG_HOST. */
#ifdef CX_IMG_HOST
#include <string.h>
#else
#include "sys.h"
#include "libc.h"
#endif
#include "appkit_image_core.h"

static uint32_t unpremul(uint32_t p) {
    uint32_t a = p >> 24;
    if (a == 255) return p;
    if (a == 0) return 0;
    uint32_t r = (p >> 16) & 255, g = (p >> 8) & 255, b = p & 255;
    r = (r * 255 + a / 2) / a; g = (g * 255 + a / 2) / a; b = (b * 255 + a / 2) / a;
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return a << 24 | r << 16 | g << 8 | b;
}
void cx_px_import(const uint8_t *src, long bpr, int w, int h, int premul, uint32_t *dst) {
    if (!src || !dst || w <= 0 || h <= 0) return;
    if (bpr < (long)w * 4) bpr = (long)w * 4;
    for (int y = 0; y < h; y++) {
        const uint8_t *row = src + (long)y * bpr;
        uint32_t *out = dst + (long)y * w;
        memcpy(out, row, (size_t)w * 4);
        if (premul) for (int x = 0; x < w; x++) out[x] = unpremul(out[x]);
    }
}
void cx_px_size(double want_w, double want_h, int px_w, int px_h, double *out_w, double *out_h) {
    if (want_w > 0 && want_h > 0) { *out_w = want_w; *out_h = want_h; return; }
    *out_w = px_w; *out_h = px_h;
}
