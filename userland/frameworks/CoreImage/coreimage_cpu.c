/*
 * mOS — Core Image em CPU: bitmap, corte, afim, desfoque gaussiano,
 * CIColorControls, source-over e Lanczos. Sem GPU.
 * MIT.
 */
#include "coreimage_cpu.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

enum {
    K_BITMAP, K_CROP, K_AFFINE, K_BLUR, K_COLOR, K_OVER, K_SCALE
};

struct mos_ci {
    int ref;
    int kind;
    mos_ci *a, *b;
    double ox, oy, ew, eh;
    int ext_ok;
    uint32_t *px;
    int pw, ph;
    double radius, sat, bri, con, scale, aspect;
    double ax, ay, bx, by, tx, ty;
    double cx, cy, cw, ch;
};

/* A definição única fica no grok_imageio.c do libSystem. Aqui só o extern. */
typedef struct {
    int w, h;
    int has_alpha;
    uint32_t *px;
} mos_img_t;
extern const char *mos_img_uti(const uint8_t *data, size_t n);
extern int mos_img_decode(const uint8_t *data, size_t n, mos_img_t *out);
extern void mos_img_free(mos_img_t *img);

static int too_big(int w, int h) {
    if (w <= 0 || h <= 0) return 1;
    if (w > 8192 || h > 8192) return 1;
    return (long)w * (long)h > 4000000L;
}

static mos_ci *node(int kind) {
    mos_ci *im = calloc(1, sizeof *im);
    if (!im) return 0;
    im->ref = 1;
    im->kind = kind;
    return im;
}

void mos_ci_retain(mos_ci *im) { if (im) im->ref++; }

void mos_ci_release(mos_ci *im) {
    if (!im || --im->ref) return;
    mos_ci_release(im->a);
    mos_ci_release(im->b);
    free(im->px);
    free(im);
}

static int extent_of(mos_ci *im);

mos_ci *mos_ci_bitmap(const uint32_t *px, int w, int h, double ox, double oy) {
    if (!px || too_big(w, h)) return 0;
    mos_ci *im = node(K_BITMAP);
    if (!im) return 0;
    im->px = malloc((size_t)w * (size_t)h * 4u);
    if (!im->px) { free(im); return 0; }
    memcpy(im->px, px, (size_t)w * (size_t)h * 4u);
    im->pw = w;
    im->ph = h;
    im->ox = ox;
    im->oy = oy;
    im->ew = w;
    im->eh = h;
    im->ext_ok = 1;
    return im;
}

mos_ci *mos_ci_from_encoded(const uint8_t *data, size_t n) {
    mos_img_t img;
    if (mos_img_decode(data, n, &img) != 0) return 0;
    mos_ci *im = mos_ci_bitmap(img.px, img.w, img.h, 0, 0);
    mos_img_free(&img);
    return im;
}

mos_ci *mos_ci_crop(mos_ci *im, double x, double y, double w, double h) {
    if (!im || w <= 0 || h <= 0) return 0;
    mos_ci *n = node(K_CROP);
    if (!n) return 0;
    n->a = im; mos_ci_retain(im);
    n->cx = x; n->cy = y; n->cw = w; n->ch = h;
    return n;
}

mos_ci *mos_ci_affine(mos_ci *im, double a, double b, double c, double d, double tx, double ty) {
    if (!im) return 0;
    if (a * d - b * c == 0) return 0;
    mos_ci *n = node(K_AFFINE);
    if (!n) return 0;
    n->a = im; mos_ci_retain(im);
    n->ax = a; n->ay = b; n->bx = c; n->by = d; n->tx = tx; n->ty = ty;
    return n;
}

mos_ci *mos_ci_blur(mos_ci *im, double radius) {
    if (!im) return 0;
    mos_ci *n = node(K_BLUR);
    if (!n) return 0;
    n->a = im; mos_ci_retain(im);
    n->radius = radius < 0 ? 0 : radius;
    return n;
}

mos_ci *mos_ci_color(mos_ci *im, double saturation, double brightness, double contrast) {
    if (!im) return 0;
    mos_ci *n = node(K_COLOR);
    if (!n) return 0;
    n->a = im; mos_ci_retain(im);
    n->sat = saturation; n->bri = brightness; n->con = contrast;
    return n;
}

mos_ci *mos_ci_over(mos_ci *fg, mos_ci *bg) {
    if (!fg || !bg) return 0;
    mos_ci *n = node(K_OVER);
    if (!n) return 0;
    n->a = fg; mos_ci_retain(fg);
    n->b = bg; mos_ci_retain(bg);
    return n;
}

mos_ci *mos_ci_lanczos(mos_ci *im, double scale, double aspect) {
    if (!im || scale <= 0 || aspect <= 0) return 0;
    mos_ci *n = node(K_SCALE);
    if (!n) return 0;
    n->a = im; mos_ci_retain(im);
    n->scale = scale; n->aspect = aspect;
    return n;
}

static void xform(double a, double b, double c, double d, double tx, double ty, double x, double y, double *ox, double *oy) {
    *ox = a * x + c * y + tx;
    *oy = b * x + d * y + ty;
}

static int extent_of(mos_ci *im) {
    if (!im) return 0;
    if (im->ext_ok) return 1;
    if (!extent_of(im->a)) return 0;
    double x = im->a->ox, y = im->a->oy, w = im->a->ew, h = im->a->eh;
    if (im->kind == K_CROP) {
        double x1 = x > im->cx ? x : im->cx;
        double y1 = y > im->cy ? y : im->cy;
        double x2 = x + w < im->cx + im->cw ? x + w : im->cx + im->cw;
        double y2 = y + h < im->cy + im->ch ? y + h : im->cy + im->ch;
        if (x2 <= x1 || y2 <= y1) return 0;
        im->ox = x1; im->oy = y1; im->ew = x2 - x1; im->eh = y2 - y1;
    } else if (im->kind == K_AFFINE) {
        double xs[4], ys[4];
        xform(im->ax, im->ay, im->bx, im->by, im->tx, im->ty, x, y, &xs[0], &ys[0]);
        xform(im->ax, im->ay, im->bx, im->by, im->tx, im->ty, x + w, y, &xs[1], &ys[1]);
        xform(im->ax, im->ay, im->bx, im->by, im->tx, im->ty, x, y + h, &xs[2], &ys[2]);
        xform(im->ax, im->ay, im->bx, im->by, im->tx, im->ty, x + w, y + h, &xs[3], &ys[3]);
        double minx = xs[0], maxx = xs[0], miny = ys[0], maxy = ys[0];
        for (int i = 1; i < 4; i++) {
            if (xs[i] < minx) minx = xs[i];
            if (xs[i] > maxx) maxx = xs[i];
            if (ys[i] < miny) miny = ys[i];
            if (ys[i] > maxy) maxy = ys[i];
        }
        im->ox = minx; im->oy = miny; im->ew = maxx - minx; im->eh = maxy - miny;
    } else if (im->kind == K_BLUR) {
        double rad = im->radius <= 0 ? 0 : ceil(im->radius * 3.0);
        im->ox = x - rad; im->oy = y - rad; im->ew = w + 2 * rad; im->eh = h + 2 * rad;
    } else if (im->kind == K_SCALE) {
        im->ox = x; im->oy = y;
        im->ew = w * im->scale * im->aspect;
        im->eh = h * im->scale;
    } else if (im->kind == K_OVER) {
        if (!extent_of(im->b)) return 0;
        double x2 = im->b->ox, y2 = im->b->oy;
        double minx = x < x2 ? x : x2;
        double miny = y < y2 ? y : y2;
        double maxx = x + w > x2 + im->b->ew ? x + w : x2 + im->b->ew;
        double maxy = y + h > y2 + im->b->eh ? y + h : y2 + im->b->eh;
        im->ox = minx; im->oy = miny; im->ew = maxx - minx; im->eh = maxy - miny;
    } else {
        im->ox = x; im->oy = y; im->ew = w; im->eh = h;
    }
    if (im->ew <= 0 || im->eh <= 0) return 0;
    im->ext_ok = 1;
    return 1;
}

int mos_ci_extent(const mos_ci *im, double *x, double *y, double *w, double *h) {
    if (!extent_of((mos_ci *)im)) return 0;
    if (x) *x = im->ox;
    if (y) *y = im->oy;
    if (w) *w = im->ew;
    if (h) *h = im->eh;
    return 1;
}

static int iround(double v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (int)(v + 0.5);
}

static uint32_t color_px(uint32_t p, double sat, double bri, double con) {
    double r = ((p >> 16) & 255) / 255.0;
    double g = ((p >> 8) & 255) / 255.0;
    double b = (p & 255) / 255.0;
    double a = (p >> 24) & 255;
    r += bri; g += bri; b += bri;
    r = (r - 0.5) * con + 0.5;
    g = (g - 0.5) * con + 0.5;
    b = (b - 0.5) * con + 0.5;
    double l = 0.2126 * r + 0.7152 * g + 0.0722 * b;
    r = l + sat * (r - l);
    g = l + sat * (g - l);
    b = l + sat * (b - l);
    return ((uint32_t)iround(a) << 24) | ((uint32_t)iround(r * 255.0) << 16) |
           ((uint32_t)iround(g * 255.0) << 8) | (uint32_t)iround(b * 255.0);
}

static uint32_t over_px(uint32_t s, uint32_t d) {
    double sa = ((s >> 24) & 255) / 255.0;
    double da = ((d >> 24) & 255) / 255.0;
    double oa = sa + da * (1.0 - sa);
    if (oa <= 0) return 0;
    double sc[3] = { ((s >> 16) & 255) / 255.0, ((s >> 8) & 255) / 255.0, (s & 255) / 255.0 };
    double dc[3] = { ((d >> 16) & 255) / 255.0, ((d >> 8) & 255) / 255.0, (d & 255) / 255.0 };
    int ch[3];
    for (int i = 0; i < 3; i++) {
        double c = (sc[i] * sa + dc[i] * da * (1.0 - sa)) / oa;
        ch[i] = iround(c * 255.0);
    }
    return ((uint32_t)iround(oa * 255.0) << 24) | ((uint32_t)ch[0] << 16) | ((uint32_t)ch[1] << 8) | (uint32_t)ch[2];
}

static uint32_t at_px(const mos_ci *im, int x, int y) {
    if (!im->px || x < 0 || y < 0 || x >= im->pw || y >= im->ph) return 0;
    return im->px[(size_t)y * (size_t)im->pw + (size_t)x];
}

static uint32_t sample_bilinear(const mos_ci *im, double sx, double sy) {
    double fx = sx - im->ox;
    double fy = sy - im->oy;
    int x0 = (int)floor(fx);
    int y0 = (int)floor(fy);
    double tx = fx - x0;
    double ty = fy - y0;
    if (tx < 0) { tx = 0; }
    if (ty < 0) { ty = 0; }
    uint32_t p00 = at_px(im, x0, y0);
    uint32_t p10 = at_px(im, x0 + 1, y0);
    uint32_t p01 = at_px(im, x0, y0 + 1);
    uint32_t p11 = at_px(im, x0 + 1, y0 + 1);
    double acc[4] = {0, 0, 0, 0};
    uint32_t ps[4] = {p00, p10, p01, p11};
    double ws[4] = {(1 - tx) * (1 - ty), tx * (1 - ty), (1 - tx) * ty, tx * ty};
    for (int i = 0; i < 4; i++) {
        double a = ((ps[i] >> 24) & 255) / 255.0 * ws[i];
        acc[3] += a;
        acc[0] += ((ps[i] >> 16) & 255) / 255.0 * a;
        acc[1] += ((ps[i] >> 8) & 255) / 255.0 * a;
        acc[2] += (ps[i] & 255) / 255.0 * a;
    }
    if (acc[3] <= 0) return 0;
    return ((uint32_t)iround(acc[3] * 255.0) << 24) |
           ((uint32_t)iround(acc[0] / acc[3] * 255.0) << 16) |
           ((uint32_t)iround(acc[1] / acc[3] * 255.0) << 8) |
           (uint32_t)iround(acc[2] / acc[3] * 255.0);
}

static mos_ci *flat_new(int w, int h, double ox, double oy) {
    if (too_big(w, h)) return 0;
    mos_ci *im = node(K_BITMAP);
    if (!im) return 0;
    im->px = calloc((size_t)w * (size_t)h, 4u);
    if (!im->px) { free(im); return 0; }
    im->pw = w; im->ph = h; im->ox = ox; im->oy = oy; im->ew = w; im->eh = h; im->ext_ok = 1;
    return im;
}

static mos_ci *flatten(mos_ci *im);

static void blit(mos_ci *dst, mos_ci *src) {
    for (int y = 0; y < src->ph; y++) {
        int dy = (int)lrint(src->oy + y - dst->oy);
        for (int x = 0; x < src->pw; x++) {
            int dx = (int)lrint(src->ox + x - dst->ox);
            if (dx < 0 || dy < 0 || dx >= dst->pw || dy >= dst->ph) continue;
            dst->px[(size_t)dy * (size_t)dst->pw + (size_t)dx] = src->px[(size_t)y * (size_t)src->pw + (size_t)x];
        }
    }
}

static mos_ci *flatten_blur(mos_ci *src, double radius) {
    if (radius <= 0) return flatten(src);
    int rad = (int)ceil(radius * 3.0);
    if (rad < 1) rad = 1;
    mos_ci *in = flatten(src);
    if (!in) return 0;
    int nw = in->pw + 2 * rad, nh = in->ph + 2 * rad;
    mos_ci *out = flat_new(nw, nh, in->ox - rad, in->oy - rad);
    if (!out) { mos_ci_release(in); return 0; }
    double sigma = radius < 0.01 ? 0.01 : radius;
    int ntap = rad * 2 + 1;
    double *wt = malloc((size_t)ntap * sizeof(double));
    double *tmp = malloc((size_t)nw * (size_t)nh * 4u * sizeof(double));
    if (!wt || !tmp) { free(wt); free(tmp); mos_ci_release(in); mos_ci_release(out); return 0; }
    double sum = 0;
    for (int i = -rad; i <= rad; i++) {
        double g = exp(-0.5 * (double)(i * i) / (sigma * sigma));
        wt[i + rad] = g;
        sum += g;
    }
    for (int i = 0; i < ntap; i++) wt[i] /= sum;
    for (int y = 0; y < nh; y++) {
        for (int x = 0; x < nw; x++) {
            double acc[4] = {0, 0, 0, 0};
            for (int k = -rad; k <= rad; k++) {
                int sx = x + k - rad;
                uint32_t p = at_px(in, sx, y - rad);
                double a = ((p >> 24) & 255) / 255.0;
                double w = wt[k + rad];
                acc[3] += a * w;
                acc[0] += ((p >> 16) & 255) / 255.0 * a * w;
                acc[1] += ((p >> 8) & 255) / 255.0 * a * w;
                acc[2] += (p & 255) / 255.0 * a * w;
            }
            double *d = tmp + ((size_t)y * (size_t)nw + (size_t)x) * 4u;
            d[0] = acc[0]; d[1] = acc[1]; d[2] = acc[2]; d[3] = acc[3];
        }
    }
    for (int y = 0; y < nh; y++) {
        for (int x = 0; x < nw; x++) {
            double acc[4] = {0, 0, 0, 0};
            for (int k = -rad; k <= rad; k++) {
                int sy = y + k;
                if (sy < 0 || sy >= nh) continue;
                double *s = tmp + ((size_t)sy * (size_t)nw + (size_t)x) * 4u;
                double w = wt[k + rad];
                acc[0] += s[0] * w; acc[1] += s[1] * w; acc[2] += s[2] * w; acc[3] += s[3] * w;
            }
            uint32_t px = 0;
            if (acc[3] > 0) {
                px = ((uint32_t)iround(acc[3] * 255.0) << 24) |
                     ((uint32_t)iround(acc[0] / acc[3] * 255.0) << 16) |
                     ((uint32_t)iround(acc[1] / acc[3] * 255.0) << 8) |
                     (uint32_t)iround(acc[2] / acc[3] * 255.0);
            }
            out->px[(size_t)y * (size_t)nw + (size_t)x] = px;
        }
    }
    free(wt);
    free(tmp);
    mos_ci_release(in);
    return out;
}

static double lanczos(double x) {
    if (x < 0) x = -x;
    if (x >= 3) return 0;
    if (x < 1e-8) return 1;
    double pix = 3.14159265358979323846 * x;
    return (sin(pix) / pix) * (sin(pix / 3.0) / (pix / 3.0));
}

static mos_ci *flatten_lanczos(mos_ci *src, double scale, double aspect) {
    mos_ci *in = flatten(src);
    if (!in) return 0;
    if (scale == 1 && aspect == 1) return in;
    int nw = (int)llround(in->pw * scale * aspect);
    int nh = (int)llround(in->ph * scale);
    if (nw < 1) nw = 1;
    if (nh < 1) nh = 1;
    mos_ci *out = flat_new(nw, nh, in->ox, in->oy);
    if (!out) { mos_ci_release(in); return 0; }
    double sx = (double)in->pw / (double)nw;
    double sy = (double)in->ph / (double)nh;
    for (int y = 0; y < nh; y++) {
        for (int x = 0; x < nw; x++) {
            double cx = (x + 0.5) * sx - 0.5;
            double cy = (y + 0.5) * sy - 0.5;
            int x0 = (int)floor(cx) - 2;
            int y0 = (int)floor(cy) - 2;
            double acc[4] = {0, 0, 0, 0};
            double wsum = 0;
            for (int j = 0; j < 6; j++) {
                double ly = lanczos((y0 + j) - cy);
                if (ly == 0) continue;
                for (int i = 0; i < 6; i++) {
                    double lx = lanczos((x0 + i) - cx);
                    double w = lx * ly;
                    if (w == 0) continue;
                    uint32_t p = at_px(in, x0 + i, y0 + j);
                    double a = ((p >> 24) & 255) / 255.0;
                    acc[3] += a * w;
                    acc[0] += ((p >> 16) & 255) / 255.0 * a * w;
                    acc[1] += ((p >> 8) & 255) / 255.0 * a * w;
                    acc[2] += (p & 255) / 255.0 * a * w;
                    wsum += w;
                }
            }
            uint32_t px = 0;
            if (wsum != 0 && acc[3] > 0) {
                double a = acc[3] / wsum;
                if (a < 0) a = 0;
                if (a > 1) a = 1;
                px = ((uint32_t)iround(a * 255.0) << 24) |
                     ((uint32_t)iround(acc[0] / acc[3] * 255.0) << 16) |
                     ((uint32_t)iround(acc[1] / acc[3] * 255.0) << 8) |
                     (uint32_t)iround(acc[2] / acc[3] * 255.0);
            }
            out->px[(size_t)y * (size_t)nw + (size_t)x] = px;
        }
    }
    mos_ci_release(in);
    return out;
}

static mos_ci *flatten_affine(mos_ci *src, double a, double b, double c, double d, double tx, double ty) {
    mos_ci *in = flatten(src);
    if (!in) return 0;
    if (a == 1 && b == 0 && c == 0 && d == 1 && tx == trunc(tx) && ty == trunc(ty)) {
        in->ox += tx;
        in->oy += ty;
        in->ew = in->pw;
        in->eh = in->ph;
        return in;
    }
    double det = a * d - b * c;
    double ia = d / det, ib = -b / det, ic = -c / det, id = a / det;
    double itx = (c * ty - d * tx) / det;
    double ity = (b * tx - a * ty) / det;
    double xs[4], ys[4];
    xform(a, b, c, d, tx, ty, in->ox, in->oy, &xs[0], &ys[0]);
    xform(a, b, c, d, tx, ty, in->ox + in->pw, in->oy, &xs[1], &ys[1]);
    xform(a, b, c, d, tx, ty, in->ox, in->oy + in->ph, &xs[2], &ys[2]);
    xform(a, b, c, d, tx, ty, in->ox + in->pw, in->oy + in->ph, &xs[3], &ys[3]);
    double minx = xs[0], maxx = xs[0], miny = ys[0], maxy = ys[0];
    for (int i = 1; i < 4; i++) {
        if (xs[i] < minx) minx = xs[i];
        if (xs[i] > maxx) maxx = xs[i];
        if (ys[i] < miny) miny = ys[i];
        if (ys[i] > maxy) maxy = ys[i];
    }
    int ox = (int)floor(minx);
    int oy = (int)floor(miny);
    int nw = (int)ceil(maxx) - ox;
    int nh = (int)ceil(maxy) - oy;
    mos_ci *out = flat_new(nw, nh, ox, oy);
    if (!out) { mos_ci_release(in); return 0; }
    for (int y = 0; y < nh; y++) {
        for (int x = 0; x < nw; x++) {
            double px = ox + x + 0.5;
            double py = oy + y + 0.5;
            double sx, sy;
            xform(ia, ib, ic, id, itx, ity, px, py, &sx, &sy);
            out->px[(size_t)y * (size_t)nw + (size_t)x] = sample_bilinear(in, sx - 0.5, sy - 0.5);
        }
    }
    mos_ci_release(in);
    return out;
}

static mos_ci *flatten(mos_ci *im) {
    if (!im || !extent_of(im)) return 0;
    if (im->kind == K_BITMAP) { mos_ci_retain(im); return im; }
    if (im->kind == K_CROP) {
        mos_ci *in = flatten(im->a);
        if (!in) return 0;
        int x0 = (int)ceil(im->ox - in->ox);
        int y0 = (int)ceil(im->oy - in->oy);
        int x1 = (int)floor(im->ox + im->ew - in->ox);
        int y1 = (int)floor(im->oy + im->eh - in->oy);
        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;
        if (x1 > in->pw) x1 = in->pw;
        if (y1 > in->ph) y1 = in->ph;
        if (x1 <= x0 || y1 <= y0) { mos_ci_release(in); return 0; }
        mos_ci *out = flat_new(x1 - x0, y1 - y0, in->ox + x0, in->oy + y0);
        if (!out) { mos_ci_release(in); return 0; }
        for (int y = 0; y < out->ph; y++)
            memcpy(out->px + (size_t)y * (size_t)out->pw,
                   in->px + (size_t)(y0 + y) * (size_t)in->pw + (size_t)x0,
                   (size_t)out->pw * 4u);
        mos_ci_release(in);
        return out;
    }
    if (im->kind == K_COLOR) {
        mos_ci *in = flatten(im->a);
        if (!in) return 0;
        mos_ci *out = flat_new(in->pw, in->ph, in->ox, in->oy);
        if (!out) { mos_ci_release(in); return 0; }
        for (int i = 0; i < in->pw * in->ph; i++) out->px[i] = color_px(in->px[i], im->sat, im->bri, im->con);
        mos_ci_release(in);
        return out;
    }
    if (im->kind == K_BLUR) return flatten_blur(im->a, im->radius);
    if (im->kind == K_SCALE) return flatten_lanczos(im->a, im->scale, im->aspect);
    if (im->kind == K_AFFINE) return flatten_affine(im->a, im->ax, im->ay, im->bx, im->by, im->tx, im->ty);
    if (im->kind == K_OVER) {
        mos_ci *fg = flatten(im->a);
        mos_ci *bg = flatten(im->b);
        if (!fg || !bg) { mos_ci_release(fg); mos_ci_release(bg); return 0; }
        int ox = (int)floor(im->ox);
        int oy = (int)floor(im->oy);
        int nw = (int)ceil(im->ox + im->ew) - ox;
        int nh = (int)ceil(im->oy + im->eh) - oy;
        mos_ci *out = flat_new(nw, nh, ox, oy);
        if (!out) { mos_ci_release(fg); mos_ci_release(bg); return 0; }
        mos_ci *canvas = flat_new(nw, nh, ox, oy);
        if (!canvas) { mos_ci_release(fg); mos_ci_release(bg); mos_ci_release(out); return 0; }
        blit(canvas, bg);
        for (int y = 0; y < fg->ph; y++) {
            int dy = (int)lrint(fg->oy + y - oy);
            for (int x = 0; x < fg->pw; x++) {
                int dx = (int)lrint(fg->ox + x - ox);
                if (dx < 0 || dy < 0 || dx >= nw || dy >= nh) continue;
                uint32_t *dp = out->px + (size_t)dy * (size_t)nw + (size_t)dx;
                uint32_t dst = canvas->px[(size_t)dy * (size_t)nw + (size_t)dx];
                *dp = over_px(fg->px[(size_t)y * (size_t)fg->pw + (size_t)x], dst);
            }
        }
        for (int i = 0; i < nw * nh; i++) if (!out->px[i]) out->px[i] = canvas->px[i];
        mos_ci_release(canvas);
        mos_ci_release(fg);
        mos_ci_release(bg);
        return out;
    }
    return 0;
}

mos_ci *mos_ci_apply(const char *name, mos_ci *input, mos_ci *background,
                     double radius, double saturation, double brightness, double contrast,
                     double scale, double aspect,
                     double a, double b, double c, double d, double tx, double ty, int crop) {
    if (!name || !input) return 0;
    if (!strcmp(name, "CIGaussianBlur")) return mos_ci_blur(input, radius);
    if (!strcmp(name, "CIColorControls")) return mos_ci_color(input, saturation, brightness, contrast);
    if (!strcmp(name, "CISourceOverCompositing")) return background ? mos_ci_over(input, background) : 0;
    if (!strcmp(name, "CILanczosScaleTransform")) return mos_ci_lanczos(input, scale, aspect);
    if (!strcmp(name, "CIAffineTransform")) return mos_ci_affine(input, a, b, c, d, tx, ty);
    if (!strcmp(name, "CICrop") && crop) return mos_ci_crop(input, tx, ty, a, b);
    return 0;
}

int mos_ci_render(mos_ci *im, double x, double y, int w, int h, uint32_t *px) {
    if (!im || !px || too_big(w, h)) return -1;
    mos_ci *flat = flatten(im);
    if (!flat) return -1;
    memset(px, 0, (size_t)w * (size_t)h * 4u);
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            int sx = (int)llround(x + col - flat->ox);
            int sy = (int)llround(y + row - flat->oy);
            if (sx < 0 || sy < 0 || sx >= flat->pw || sy >= flat->ph) continue;
            px[(size_t)row * (size_t)w + (size_t)col] = flat->px[(size_t)sy * (size_t)flat->pw + (size_t)sx];
        }
    }
    mos_ci_release(flat);
    return 0;
}

int mos_ci_render_format(mos_ci *im, double x, double y, int w, int h, int format, void *bytes, long row_bytes) {
    if (!bytes || row_bytes < (long)w * 4) return -1;
    uint32_t *tmp = malloc((size_t)w * (size_t)h * 4u);
    if (!tmp) return -1;
    if (mos_ci_render(im, x, y, w, h, tmp) != 0) { free(tmp); return -1; }
    for (int row = 0; row < h; row++) {
        uint8_t *d = (uint8_t *)bytes + (long)row * row_bytes;
        for (int col = 0; col < w; col++) {
            uint32_t p = tmp[(size_t)row * (size_t)w + (size_t)col];
            uint8_t A = (uint8_t)(p >> 24), R = (uint8_t)(p >> 16), G = (uint8_t)(p >> 8), B = (uint8_t)p;
            uint8_t *o = d + col * 4;
            if (format == MOS_CI_BGRA8) { o[0] = B; o[1] = G; o[2] = R; o[3] = A; }
            else if (format == MOS_CI_RGBA8) { o[0] = R; o[1] = G; o[2] = B; o[3] = A; }
            else if (format == MOS_CI_ABGR8) { o[0] = A; o[1] = B; o[2] = G; o[3] = R; }
            else { o[0] = A; o[1] = R; o[2] = G; o[3] = B; }
        }
    }
    free(tmp);
    return 0;
}

void mos_ci_ctx_init(mos_ci_ctx *c, int cache) {
    if (!c) return;
    memset(c, 0, sizeof *c);
    c->cache = cache ? 1 : 0;
}
int mos_ci_ctx_renders(const mos_ci_ctx *c) { return c ? c->renders : 0; }
void mos_ci_ctx_free(mos_ci_ctx *c) {
    if (!c) return;
    free(c->px);
    c->px = 0;
    c->im = 0;
}
int mos_ci_colorspace_ok(int foreign_space) { return foreign_space ? 0 : 1; }
int mos_ci_ctx_render(mos_ci_ctx *c, mos_ci *im, double x, double y, int w, int h, uint32_t *px) {
    if (!c || !px) return -1;
    size_t n = (size_t)w * (size_t)h;
    if (c->cache && c->px && c->im == im && c->x == x && c->y == y && c->w == w && c->h == h) {
        memcpy(px, c->px, n * 4u);
        return 0;
    }
    if (mos_ci_render(im, x, y, w, h, px) != 0) return -1;
    c->renders++;
    if (!c->cache) return 0;
    uint32_t *copy = malloc(n * 4u);
    if (!copy) return 0;
    memcpy(copy, px, n * 4u);
    free(c->px);
    c->px = copy;
    c->im = im;
    c->x = x;
    c->y = y;
    c->w = w;
    c->h = h;
    return 0;
}
