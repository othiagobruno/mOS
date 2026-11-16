/* mOS — núcleo do AVVideoComposition (ver avvcomp_core.h). Código original do mOS (MIT). */
#include "avvcomp_core.h"

static double lerp(double a, double b, double k) { return a + (b - a) * k; }
static double frac_of(double t0, double t1, double t) { return t1 > t0 ? (t <= t0 ? 0 : t >= t1 ? 1 : (t - t0) / (t1 - t0)) : (t < t0 ? 0 : 1); }
/* a rampa que vale em t: a última que começa até t; antes de todas, a primeira (no seu início) */
#define PICK(arr, n) int k = -1; for (int i = 0; i < (n); i++) if ((arr)[i].t0 <= t && (k < 0 || (arr)[i].t0 >= (arr)[k].t0)) k = i; \
    if (k < 0) { for (int i = 0; i < (n); i++) if (k < 0 || (arr)[i].t0 < (arr)[k].t0) k = i; }
vc_aff vc_layer_transform(const vc_layer *l, double t) {
    vc_aff id = {1, 0, 0, 1, 0, 0};
    if (!l->ntr) return id;
    PICK(l->tr, l->ntr);
    const vc_aff_ramp *r = &l->tr[k];
    double f = t < r->t0 ? 0 : frac_of(r->t0, r->t1, t);
    vc_aff o = {lerp(r->a0.a, r->a1.a, f), lerp(r->a0.b, r->a1.b, f), lerp(r->a0.c, r->a1.c, f), lerp(r->a0.d, r->a1.d, f),
                lerp(r->a0.tx, r->a1.tx, f), lerp(r->a0.ty, r->a1.ty, f)};
    return o;
}
float vc_layer_opacity(const vc_layer *l, double t) {
    if (!l->nop) return 1;
    PICK(l->op, l->nop);
    const vc_f_ramp *r = &l->op[k];
    double f = t < r->t0 ? 0 : frac_of(r->t0, r->t1, t);
    return (float)lerp(r->v0, r->v1, f);
}
int vc_layer_crop(const vc_layer *l, double t, vc_rect *out) {
    if (!l->ncr) return 0;
    PICK(l->cr, l->ncr);
    const vc_rect_ramp *r = &l->cr[k];
    double f = t < r->t0 ? 0 : frac_of(r->t0, r->t1, t);
    out->x = lerp(r->r0.x, r->r1.x, f); out->y = lerp(r->r0.y, r->r1.y, f); out->w = lerp(r->r0.w, r->r1.w, f); out->h = lerp(r->r0.h, r->r1.h, f);
    return 1;
}

static uint32_t mix(uint32_t a, uint32_t b, uint32_t k) {   /* k em 0..256 */
    uint32_t rb = ((a & 0xFF00FF) * (256 - k) + (b & 0xFF00FF) * k) >> 8 & 0xFF00FF;
    uint32_t ag = (((a >> 8) & 0xFF00FF) * (256 - k) + ((b >> 8) & 0xFF00FF) * k) & 0xFF00FF00;
    return rb | ag;
}
void vc_composite(uint32_t *dst, int dw, int dh, const uint32_t *src, int sw, int sh, vc_aff t, float opacity, const vc_rect *crop) {
    double det = t.a * t.d - t.b * t.c;
    if (det == 0 || opacity <= 0) return;
    /* inversa: ponto da tela -> ponto da origem (x' = a x + c y + tx, y' = b x + d y + ty) */
    double ia = t.d / det, ib = -t.b / det, ic = -t.c / det, id = t.a / det;
    double itx = -(ia * t.tx + ic * t.ty), ity = -(ib * t.tx + id * t.ty);
    double cx0 = 0, cy0 = 0, cx1 = sw, cy1 = sh;
    if (crop) { cx0 = crop->x; cy0 = crop->y; cx1 = crop->x + crop->w; cy1 = crop->y + crop->h; }
    uint32_t op = (uint32_t)(opacity > 1 ? 256 : opacity * 256 + 0.5);
    for (int y = 0; y < dh; y++)
        for (int x = 0; x < dw; x++) {
            double px = x + 0.5, py = y + 0.5;
            double sx = ia * px + ic * py + itx, sy = ib * px + id * py + ity;
            if (sx < cx0 || sy < cy0 || sx >= cx1 || sy >= cy1 || sx < 0 || sy < 0 || sx >= sw || sy >= sh) continue;
            double fx = sx - 0.5, fy = sy - 0.5;
            int x0 = (int)(fx < 0 ? 0 : fx), y0 = (int)(fy < 0 ? 0 : fy);
            int x1 = x0 + 1 < sw ? x0 + 1 : x0, y1 = y0 + 1 < sh ? y0 + 1 : y0;
            uint32_t kx = (uint32_t)((fx - x0 < 0 ? 0 : fx - x0) * 256), ky = (uint32_t)((fy - y0 < 0 ? 0 : fy - y0) * 256);
            if (kx > 256) kx = 256;
            if (ky > 256) ky = 256;
            uint32_t top = mix(src[y0 * sw + x0], src[y0 * sw + x1], kx), bot = mix(src[y1 * sw + x0], src[y1 * sw + x1], kx);
            uint32_t c = mix(top, bot, ky);
            uint32_t ca = c >> 24;
            ca += ca >> 7;                          /* 0..255 -> 0..256 */
            uint32_t a = (ca * op) >> 8;            /* alfa da origem x opacidade */
            dst[y * dw + x] = mix(dst[y * dw + x], c | 0xFF000000u, a > 256 ? 256 : a);
        }
}
