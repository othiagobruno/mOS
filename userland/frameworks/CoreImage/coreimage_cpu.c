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
    K_BITMAP, K_CROP, K_AFFINE, K_BLUR, K_COLOR, K_OVER, K_SCALE,
    K_PIX, K_BLEND, K_GEN, K_CLAMP, K_BOX, K_PIXELLATE
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
    int inf;                    /* extent infinito (geradores, clamp e o que depende deles) */
    mos_ci *c;                  /* máscara (CIBlendWithMask) */
    int op;
    double p[24];
};

/* A definição única fica no imageio.c do libSystem (iio_core.inc). Aqui só o extern. */
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
    mos_ci_release(im->c);
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

static int extent_new(mos_ci *im);
static int extent_of(mos_ci *im) {
    if (!im) return 0;
    if (im->ext_ok) return 1;
    if (im->kind >= K_PIX) return extent_new(im);
    if (!extent_of(im->a)) return 0;
    if (im->a->inf || (im->kind == K_OVER && extent_of(im->b) && im->b->inf)) {   /* filho infinito */
        if (im->kind == K_CROP) {
            double x1 = im->cx, y1 = im->cy, x2 = im->cx + im->cw, y2 = im->cy + im->ch;
            if (!im->a->inf) {
                if (im->a->ox > x1) x1 = im->a->ox;
                if (im->a->oy > y1) y1 = im->a->oy;
                if (im->a->ox + im->a->ew < x2) x2 = im->a->ox + im->a->ew;
                if (im->a->oy + im->a->eh < y2) y2 = im->a->oy + im->a->eh;
            }
            if (x2 <= x1 || y2 <= y1) return 0;
            im->ox = x1; im->oy = y1; im->ew = x2 - x1; im->eh = y2 - y1;
        } else if (im->kind == K_SCALE) return 0;   /* Lanczos de uma imagem infinita: sem origem para escalar */
        else im->inf = 1;
        im->ext_ok = 1;
        return 1;
    }
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

int mos_ci_is_infinite(const mos_ci *im) { return extent_of((mos_ci *)im) && im->inf; }
int mos_ci_extent(const mos_ci *im, double *x, double *y, double *w, double *h) {
    if (!extent_of((mos_ci *)im)) return 0;
    if (im->inf) {   /* CGRectInfinite */
        if (x) *x = -8.9884656743115785e307;
        if (y) *y = -8.9884656743115785e307;
        if (w) *w = 1.7976931348623157e308;
        if (h) *h = 1.7976931348623157e308;
        return 1;
    }
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
        if (in->ref > 1) {   /* o bitmap é o da imagem de origem: translada uma cópia */
            mos_ci *cp = mos_ci_bitmap(in->px, in->pw, in->ph, in->ox, in->oy);
            mos_ci_release(in);
            if (!cp) return 0;
            in = cp;
        }
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

static int has_inf(mos_ci *im) { return im && ((extent_of(im) && im->inf) || has_inf(im->a) || has_inf(im->b) || has_inf(im->c)); }
static mos_ci *flatten_rect(mos_ci *im, int x, int y, int w, int h);
static mos_ci *flatten(mos_ci *im) {
    if (!im || !extent_of(im) || im->inf) return 0;
    if (im->kind == K_BITMAP) { mos_ci_retain(im); return im; }
    if (im->kind >= K_PIX || has_inf(im)) {   /* pelo retângulo do extent */
        int x0 = (int)floor(im->ox), y0 = (int)floor(im->oy);
        return flatten_rect(im, x0, y0, (int)ceil(im->ox + im->ew) - x0, (int)ceil(im->oy + im->eh) - y0);
    }
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

/* ================================================================ filtros novos (operações por pixel, misturas,
 * geradores, clamp, box blur, pixellate). Cores em 0..1, alfa reto como o resto do arquivo; a mistura compõe em
 * pré-multiplicado (W3C Compositing and Blending). Coordenadas de geradores no centro do pixel. */
typedef struct { double r, g, b, a; } rgba_t;
static rgba_t unpack(uint32_t p) { rgba_t c = {((p >> 16) & 255) / 255.0, ((p >> 8) & 255) / 255.0, (p & 255) / 255.0, ((p >> 24) & 255) / 255.0}; return c; }
static double clamp01(double v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
static uint32_t pack(rgba_t c) {
    double a = clamp01(c.a);
    if (a <= 0) return 0;
    return ((uint32_t)iround(a * 255.0) << 24) | ((uint32_t)iround(clamp01(c.r) * 255.0) << 16) |
           ((uint32_t)iround(clamp01(c.g) * 255.0) << 8) | (uint32_t)iround(clamp01(c.b) * 255.0);
}
/* pré-multiplicado -> pixel */
static uint32_t pack_pm(double r, double g, double b, double a) {
    if (a <= 0) return 0;
    rgba_t c = {r / a, g / a, b / a, a};
    return pack(c);
}
static double luma(rgba_t c) { return 0.2125 * c.r + 0.7154 * c.g + 0.0721 * c.b; }
static double to_lin(double v) { return v <= 0.04045 ? v / 12.92 : pow((v + 0.055) / 1.055, 2.4); }
static double to_srgb(double v) { return v <= 0.0031308 ? v * 12.92 : 1.055 * pow(v, 1 / 2.4) - 0.055; }

mos_ci *mos_ci_pixop(mos_ci *im, int op, const double *p, int np) {
    if (!im || np < 0 || np > 24) return 0;
    mos_ci *n = node(K_PIX);
    if (!n) return 0;
    n->a = im; mos_ci_retain(im);
    n->op = op;
    for (int i = 0; i < np; i++) n->p[i] = p[i];
    return n;
}
mos_ci *mos_ci_blend(mos_ci *fg, mos_ci *bg, mos_ci *mask, int mode, double param) {
    if (!fg || !bg) return 0;
    if ((mode == MOS_CI_BL_MASK || mode == MOS_CI_BL_ALPHA_MASK) && !mask) return 0;
    mos_ci *n = node(K_BLEND);
    if (!n) return 0;
    n->a = fg; mos_ci_retain(fg);
    n->b = bg; mos_ci_retain(bg);
    if (mask) { n->c = mask; mos_ci_retain(mask); }
    n->op = mode;
    n->p[0] = param;
    return n;
}
mos_ci *mos_ci_generator(int kind, const double *p, int np) {
    if (np < 0 || np > 24) return 0;
    mos_ci *n = node(K_GEN);
    if (!n) return 0;
    n->op = kind;
    for (int i = 0; i < np; i++) n->p[i] = p[i];
    n->inf = 1; n->ext_ok = 1;
    return n;
}
mos_ci *mos_ci_clamp(mos_ci *im) {
    if (!im || !extent_of(im)) return 0;
    if (im->inf) { mos_ci_retain(im); return im; }
    mos_ci *n = node(K_CLAMP);
    if (!n) return 0;
    n->a = im; mos_ci_retain(im);
    n->inf = 1; n->ext_ok = 1;
    return n;
}
mos_ci *mos_ci_box_blur(mos_ci *im, double radius) {
    if (!im) return 0;
    mos_ci *n = node(K_BOX);
    if (!n) return 0;
    n->a = im; mos_ci_retain(im);
    n->radius = radius < 0 ? 0 : radius;
    return n;
}
mos_ci *mos_ci_pixellate(mos_ci *im, double cx, double cy, double scale) {
    if (!im || scale < 1) return 0;
    mos_ci *n = node(K_PIXELLATE);
    if (!n) return 0;
    n->a = im; mos_ci_retain(im);
    n->p[0] = cx; n->p[1] = cy; n->p[2] = scale;
    return n;
}

static void rect_union(mos_ci *im, const mos_ci *x, const mos_ci *y) {
    double minx = x->ox < y->ox ? x->ox : y->ox, miny = x->oy < y->oy ? x->oy : y->oy;
    double maxx = x->ox + x->ew > y->ox + y->ew ? x->ox + x->ew : y->ox + y->ew;
    double maxy = x->oy + x->eh > y->oy + y->eh ? x->oy + x->eh : y->oy + y->eh;
    im->ox = minx; im->oy = miny; im->ew = maxx - minx; im->eh = maxy - miny;
}
static int rect_inter(mos_ci *im, const mos_ci *x, const mos_ci *y) {
    double x1 = x->ox > y->ox ? x->ox : y->ox, y1 = x->oy > y->oy ? x->oy : y->oy;
    double x2 = x->ox + x->ew < y->ox + y->ew ? x->ox + x->ew : y->ox + y->ew;
    double y2 = x->oy + x->eh < y->oy + y->eh ? x->oy + x->eh : y->oy + y->eh;
    if (x2 <= x1 || y2 <= y1) return 0;
    im->ox = x1; im->oy = y1; im->ew = x2 - x1; im->eh = y2 - y1;
    return 1;
}
static void copy_ext(mos_ci *im, const mos_ci *x) { im->ox = x->ox; im->oy = x->oy; im->ew = x->ew; im->eh = x->eh; im->inf = x->inf; }
static int extent_new(mos_ci *im) {
    if (im->kind == K_GEN || im->kind == K_CLAMP) { im->inf = 1; im->ext_ok = 1; return 1; }
    if (!extent_of(im->a)) return 0;
    mos_ci *a = im->a;
    if (im->kind == K_PIX) {
        copy_ext(im, a);
    } else if (im->kind == K_BOX || im->kind == K_PIXELLATE) {
        copy_ext(im, a);
        if (!a->inf) {
            double r = im->kind == K_BOX ? ceil(im->radius) : im->p[2];
            im->ox -= r; im->oy -= r; im->ew += 2 * r; im->eh += 2 * r;
        }
    } else if (im->kind == K_BLEND) {
        if (!extent_of(im->b)) {
            if (im->op == MOS_CI_BL_SOURCE_OUT || im->op == MOS_CI_BL_UNSHARP || im->op == MOS_CI_BL_SHARPEN_LUMA) { copy_ext(im, a); goto ok; }
            return 0;
        }
        mos_ci *b = im->b;
        switch (im->op) {
        case MOS_CI_BL_SOURCE_IN:   /* só onde as duas existem */
            if (a->inf && b->inf) im->inf = 1;
            else if (a->inf) copy_ext(im, b);
            else if (b->inf) copy_ext(im, a);
            else if (!rect_inter(im, a, b)) return 0;
            break;
        case MOS_CI_BL_SOURCE_ATOP: copy_ext(im, b); break;
        case MOS_CI_BL_SOURCE_OUT: case MOS_CI_BL_UNSHARP: case MOS_CI_BL_SHARPEN_LUMA: copy_ext(im, a); break;
        default:
            if (a->inf || b->inf) im->inf = 1;
            else rect_union(im, a, b);
        }
    } else return 0;
ok:
    if (!im->inf && (im->ew <= 0 || im->eh <= 0)) return 0;
    im->ext_ok = 1;
    return 1;
}

static uint32_t pix_op(const mos_ci *n, uint32_t px) {
    rgba_t c = unpack(px), o = c;
    const double *p = n->p;
    switch (n->op) {
    case MOS_CI_OP_MATRIX:   /* R, G, B, A (4 cada) e o bias (4) */
        o.r = p[0] * c.r + p[1] * c.g + p[2] * c.b + p[3] * c.a + p[16];
        o.g = p[4] * c.r + p[5] * c.g + p[6] * c.b + p[7] * c.a + p[17];
        o.b = p[8] * c.r + p[9] * c.g + p[10] * c.b + p[11] * c.a + p[18];
        o.a = p[12] * c.r + p[13] * c.g + p[14] * c.b + p[15] * c.a + p[19];
        break;
    case MOS_CI_OP_EXPOSURE: {   /* em luz linear, como o espaço de trabalho do Core Image */
        double k = pow(2.0, p[0]);
        o.r = to_srgb(to_lin(c.r) * k); o.g = to_srgb(to_lin(c.g) * k); o.b = to_srgb(to_lin(c.b) * k);
        break;
    }
    case MOS_CI_OP_INVERT: o.r = 1 - c.r; o.g = 1 - c.g; o.b = 1 - c.b; break;
    case MOS_CI_OP_MONOCHROME: {   /* luminância em overlay com a cor; mistura pela intensidade */
        double l = luma(c), f[3] = {p[0], p[1], p[2]}, m[3];
        for (int i = 0; i < 3; i++) m[i] = l < 0.5 ? 2 * l * f[i] : 1 - 2 * (1 - l) * (1 - f[i]);
        double k = p[3];
        o.r = c.r + (m[0] - c.r) * k; o.g = c.g + (m[1] - c.g) * k; o.b = c.b + (m[2] - c.b) * k;
        break;
    }
    case MOS_CI_OP_SEPIA: {
        double sr = 0.393 * c.r + 0.769 * c.g + 0.189 * c.b, sg = 0.349 * c.r + 0.686 * c.g + 0.168 * c.b,
               sb = 0.272 * c.r + 0.534 * c.g + 0.131 * c.b, k = p[0];
        o.r = c.r + (sr - c.r) * k; o.g = c.g + (sg - c.g) * k; o.b = c.b + (sb - c.b) * k;
        break;
    }
    case MOS_CI_OP_HUE: {   /* rotação do matiz no plano IQ do YIQ */
        double y = 0.299 * c.r + 0.587 * c.g + 0.114 * c.b, i = 0.596 * c.r - 0.274 * c.g - 0.322 * c.b,
               q = 0.211 * c.r - 0.523 * c.g + 0.312 * c.b, cs = cos(p[0]), sn = sin(p[0]);
        double i2 = i * cs - q * sn, q2 = i * sn + q * cs;
        o.r = y + 0.956 * i2 + 0.621 * q2; o.g = y - 0.272 * i2 - 0.647 * q2; o.b = y - 1.106 * i2 + 1.703 * q2;
        break;
    }
    case MOS_CI_OP_GAMMA: o.r = pow(c.r, p[0]); o.g = pow(c.g, p[0]); o.b = pow(c.b, p[0]); break;
    case MOS_CI_OP_CLAMP_COMPONENTS: {
        double v[4] = {c.r, c.g, c.b, c.a};
        for (int i = 0; i < 4; i++) { if (v[i] < p[i]) v[i] = p[i]; if (v[i] > p[4 + i]) v[i] = p[4 + i]; }
        o.r = v[0]; o.g = v[1]; o.b = v[2]; o.a = v[3];
        break;
    }
    case MOS_CI_OP_POSTERIZE: {
        double L = p[0] < 1 ? 1 : p[0];
        o.r = floor(c.r * L + 0.5) / L; o.g = floor(c.g * L + 0.5) / L; o.b = floor(c.b * L + 0.5) / L;
        break;
    }
    case MOS_CI_OP_MASK_TO_ALPHA: o.r = o.g = o.b = 1; o.a = luma(c) * c.a; break;
    case MOS_CI_OP_VIBRANCE: {
        double avg = (c.r + c.g + c.b) / 3, mx = c.r > c.g ? (c.r > c.b ? c.r : c.b) : (c.g > c.b ? c.g : c.b);
        double amt = (mx - avg) * (-p[0] * 3.0);
        o.r = c.r + (mx - c.r) * amt; o.g = c.g + (mx - c.g) * amt; o.b = c.b + (mx - c.b) * amt;
        break;
    }
    case MOS_CI_OP_MIN_COMPONENT: { double m = c.r < c.g ? (c.r < c.b ? c.r : c.b) : (c.g < c.b ? c.g : c.b); o.r = o.g = o.b = m; break; }
    case MOS_CI_OP_MAX_COMPONENT: { double m = c.r > c.g ? (c.r > c.b ? c.r : c.b) : (c.g > c.b ? c.g : c.b); o.r = o.g = o.b = m; break; }
    case MOS_CI_OP_FALSE_COLOR: {   /* luminância entre color0 e color1 */
        double l = luma(c);
        o.r = p[0] + (p[4] - p[0]) * l; o.g = p[1] + (p[5] - p[1]) * l; o.b = p[2] + (p[6] - p[2]) * l;
        o.a = c.a * (p[3] + (p[7] - p[3]) * l);
        break;
    }
    case MOS_CI_OP_COLOR_CONTROLS: return color_px(px, p[0], p[1], p[2]);
    case MOS_CI_OP_PHOTO_MONO: o.r = o.g = o.b = luma(c); break;
    default: break;
    }
    return pack(o);
}
static double blend_ch(int mode, double cb, double cs) {
    switch (mode) {
    case MOS_CI_BL_MULTIPLY: return cb * cs;
    case MOS_CI_BL_SCREEN: return cb + cs - cb * cs;
    case MOS_CI_BL_OVERLAY: return cb <= 0.5 ? 2 * cs * cb : 1 - 2 * (1 - cs) * (1 - cb);
    case MOS_CI_BL_HARD_LIGHT: return cs <= 0.5 ? 2 * cs * cb : 1 - 2 * (1 - cs) * (1 - cb);
    case MOS_CI_BL_DARKEN: return cb < cs ? cb : cs;
    case MOS_CI_BL_LIGHTEN: return cb > cs ? cb : cs;
    case MOS_CI_BL_DIFFERENCE: return fabs(cb - cs);
    case MOS_CI_BL_EXCLUSION: return cb + cs - 2 * cb * cs;
    case MOS_CI_BL_COLOR_DODGE: return cb <= 0 ? 0 : cs >= 1 ? 1 : (cb / (1 - cs) > 1 ? 1 : cb / (1 - cs));
    case MOS_CI_BL_COLOR_BURN: return cb >= 1 ? 1 : cs <= 0 ? 0 : 1 - ((1 - cb) / cs > 1 ? 1 : (1 - cb) / cs);
    case MOS_CI_BL_SOFT_LIGHT: {
        if (cs <= 0.5) return cb - (1 - 2 * cs) * cb * (1 - cb);
        double d = cb <= 0.25 ? ((16 * cb - 12) * cb + 4) * cb : sqrt(cb);
        return cb + (2 * cs - 1) * (d - cb);
    }
    default: return cs;
    }
}
static uint32_t blend_px(int mode, double param, uint32_t fs, uint32_t bs, uint32_t ms) {
    rgba_t s = unpack(fs), d = unpack(bs);
    double as = s.a, ab = d.a;
    double S[3] = {s.r * as, s.g * as, s.b * as}, D[3] = {d.r * ab, d.g * ab, d.b * ab}, O[3];
    double ao;
    switch (mode) {
    case MOS_CI_BL_SOURCE_IN: for (int i = 0; i < 3; i++) O[i] = S[i] * ab; ao = as * ab; break;
    case MOS_CI_BL_SOURCE_OUT: for (int i = 0; i < 3; i++) O[i] = S[i] * (1 - ab); ao = as * (1 - ab); break;
    case MOS_CI_BL_SOURCE_ATOP: for (int i = 0; i < 3; i++) O[i] = S[i] * ab + D[i] * (1 - as); ao = ab; break;
    case MOS_CI_BL_ADDITION: for (int i = 0; i < 3; i++) O[i] = S[i] + D[i]; ao = as + ab; break;
    case MOS_CI_BL_MULTIPLY_COMPOSITING: for (int i = 0; i < 3; i++) O[i] = S[i] * D[i]; ao = as * ab; break;
    case MOS_CI_BL_MINIMUM: for (int i = 0; i < 3; i++) O[i] = S[i] < D[i] ? S[i] : D[i]; ao = as < ab ? as : ab; break;
    case MOS_CI_BL_MAXIMUM: for (int i = 0; i < 3; i++) O[i] = S[i] > D[i] ? S[i] : D[i]; ao = as > ab ? as : ab; break;
    case MOS_CI_BL_MASK: case MOS_CI_BL_ALPHA_MASK: {   /* fundo -> imagem pela máscara */
        rgba_t m = unpack(ms);
        double k = mode == MOS_CI_BL_MASK ? luma(m) * m.a : m.a;
        for (int i = 0; i < 3; i++) O[i] = D[i] + (S[i] - D[i]) * k;
        ao = ab + (as - ab) * k;
        break;
    }
    case MOS_CI_BL_UNSHARP: {   /* fs = original, bs = desfocada (alfa reto) */
        rgba_t o = {s.r + (s.r - d.r) * param, s.g + (s.g - d.g) * param, s.b + (s.b - d.b) * param, s.a};
        return pack(o);
    }
    case MOS_CI_BL_SHARPEN_LUMA: {
        double k = (luma(s) - luma(d)) * param;
        rgba_t o = {s.r + k, s.g + k, s.b + k, s.a};
        return pack(o);
    }
    default: {   /* modos separáveis do W3C sobre source-over */
        double cs[3] = {s.r, s.g, s.b}, cb[3] = {d.r, d.g, d.b};
        for (int i = 0; i < 3; i++) O[i] = S[i] * (1 - ab) + D[i] * (1 - as) + as * ab * blend_ch(mode, cb[i], cs[i]);
        ao = as + ab - as * ab;
    }
    }
    if (ao > 1) ao = 1;
    for (int i = 0; i < 3; i++) if (O[i] > ao) O[i] = ao;
    return pack_pm(O[0], O[1], O[2], ao);
}
static uint32_t gen_px(const mos_ci *g, double x, double y) {
    const double *p = g->p;
    double t;
    const double *c0, *c1;
    switch (g->op) {
    case MOS_CI_GEN_CONSTANT: { rgba_t c = {p[0], p[1], p[2], p[3]}; return pack(c); }
    case MOS_CI_GEN_LINEAR: case MOS_CI_GEN_SMOOTH_LINEAR: {   /* p0 (0,1), p1 (2,3), c0 (4..7), c1 (8..11) */
        double dx = p[2] - p[0], dy = p[3] - p[1], L = dx * dx + dy * dy;
        t = L > 0 ? ((x - p[0]) * dx + (y - p[1]) * dy) / L : 0;
        t = clamp01(t);
        if (g->op == MOS_CI_GEN_SMOOTH_LINEAR) t = t * t * (3 - 2 * t);
        c0 = p + 4; c1 = p + 8;
        break;
    }
    case MOS_CI_GEN_RADIAL: {   /* centro (0,1), r0 (2), r1 (3), c0 (4..7), c1 (8..11) */
        double r = sqrt((x - p[0]) * (x - p[0]) + (y - p[1]) * (y - p[1]));
        t = p[3] != p[2] ? (r - p[2]) / (p[3] - p[2]) : r >= p[3];
        t = clamp01(t);
        c0 = p + 4; c1 = p + 8;
        break;
    }
    case MOS_CI_GEN_CHECKER: {   /* centro (0,1), c0 (2..5), c1 (6..9), largura (10), nitidez (11) */
        double w = p[10] > 0 ? p[10] : 1, fx = (x - p[0]) / w, fy = (y - p[1]) / w;
        int odd = ((long)floor(fx) + (long)floor(fy)) & 1;
        double sh = clamp01(p[11]);
        if (sh >= 1) t = odd;
        else {   /* borda suave: distância à aresta mais perto da casa, em unidades de casa */
            double ex = fx - floor(fx), ey = fy - floor(fy);
            double dx2 = ex < 0.5 ? ex : 1 - ex, dy2 = ey < 0.5 ? ey : 1 - ey, dmin = dx2 < dy2 ? dx2 : dy2;
            double soft = (1 - sh) * 0.5, k = soft > 0 ? clamp01(dmin / soft) * 0.5 + 0.5 : 1;
            t = odd ? k : 1 - k;
        }
        c0 = p + 2; c1 = p + 6;
        break;
    }
    default: return 0;
    }
    double a = c0[3] + (c1[3] - c0[3]) * t;   /* interpolação em pré-multiplicado */
    double r = c0[0] * c0[3] + (c1[0] * c1[3] - c0[0] * c0[3]) * t, gg = c0[1] * c0[3] + (c1[1] * c1[3] - c0[1] * c0[3]) * t,
           b = c0[2] * c0[3] + (c1[2] * c1[3] - c0[2] * c0[3]) * t;
    return pack_pm(r, gg, b, a);
}

/* bitmap [x,x+w) x [y,y+h) com o que está em src (fora: transparente) */
static mos_ci *crop_to(mos_ci *src, int x, int y, int w, int h) {
    mos_ci *out = flat_new(w, h, x, y);
    if (!out) return 0;
    if (src) blit(out, src);
    return out;
}
static int old_kind(int k) { return k < K_PIX; }
static mos_ci *box_flat(mos_ci *in, double radius) {
    int r = (int)ceil(radius);
    if (r < 1) { mos_ci_retain(in); return in; }
    mos_ci *out = flat_new(in->pw + 2 * r, in->ph + 2 * r, in->ox - r, in->oy - r);
    if (!out) return 0;
    int W = out->pw, H = out->ph;
    double *tmp = calloc((size_t)W * H * 4, sizeof(double));
    if (!tmp) { mos_ci_release(out); return 0; }
    double n = 2 * r + 1;
    for (int y = 0; y < in->ph; y++)   /* horizontal, em pré-multiplicado */
        for (int x = 0; x < W; x++) {
            double acc[4] = {0, 0, 0, 0};
            for (int k = -r; k <= r; k++) {
                int sx = x - r + k;
                if (sx < 0 || sx >= in->pw) continue;
                rgba_t c = unpack(in->px[(size_t)y * in->pw + sx]);
                acc[0] += c.r * c.a; acc[1] += c.g * c.a; acc[2] += c.b * c.a; acc[3] += c.a;
            }
            double *t = tmp + ((size_t)(y + r) * W + x) * 4;
            for (int i = 0; i < 4; i++) t[i] = acc[i] / n;
        }
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            double acc[4] = {0, 0, 0, 0};
            for (int k = -r; k <= r; k++) {
                int sy = y + k;
                if (sy < 0 || sy >= H) continue;
                double *t = tmp + ((size_t)sy * W + x) * 4;
                for (int i = 0; i < 4; i++) acc[i] += t[i];
            }
            out->px[(size_t)y * W + x] = pack_pm(acc[0] / n, acc[1] / n, acc[2] / n, acc[3] / n);
        }
    free(tmp);
    return out;
}
static mos_ci *flatten_rect(mos_ci *im, int x, int y, int w, int h) {
    if (!im || !extent_of(im) || too_big(w, h)) return 0;
    if (!im->inf && !has_inf(im) && old_kind(im->kind)) {   /* caminho antigo (extent finito) e recorte */
        mos_ci *f = flatten(im);
        mos_ci *out = crop_to(f, x, y, w, h);
        mos_ci_release(f);
        return out;
    }
    mos_ci *out = 0, *a = 0, *b = 0, *m = 0;
    switch (im->kind) {
    case K_BITMAP: return crop_to(im, x, y, w, h);
    case K_GEN:
        if (!(out = flat_new(w, h, x, y))) return 0;
        for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) out->px[(size_t)j * w + i] = gen_px(im, x + i + 0.5, y + j + 0.5);
        return out;
    case K_CLAMP: {
        mos_ci *f = flatten(im->a);
        if (!f || !(out = flat_new(w, h, x, y))) { mos_ci_release(f); return 0; }
        for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) {
            int sx = (int)floor(x + i - f->ox), sy = (int)floor(y + j - f->oy);
            sx = sx < 0 ? 0 : sx >= f->pw ? f->pw - 1 : sx;
            sy = sy < 0 ? 0 : sy >= f->ph ? f->ph - 1 : sy;
            out->px[(size_t)j * w + i] = f->px[(size_t)sy * f->pw + sx];
        }
        mos_ci_release(f);
        return out;
    }
    case K_CROP: {
        int x1 = x > (int)ceil(im->ox) ? x : (int)ceil(im->ox), y1 = y > (int)ceil(im->oy) ? y : (int)ceil(im->oy);
        int x2 = x + w < (int)floor(im->ox + im->ew) ? x + w : (int)floor(im->ox + im->ew);
        int y2 = y + h < (int)floor(im->oy + im->eh) ? y + h : (int)floor(im->oy + im->eh);
        if (x2 > x1 && y2 > y1) a = flatten_rect(im->a, x1, y1, x2 - x1, y2 - y1);
        out = crop_to(a, x, y, w, h);
        mos_ci_release(a);
        return out;
    }
    case K_PIX: case K_COLOR:
        if (!(a = flatten_rect(im->a, x, y, w, h))) return 0;
        if (!(out = flat_new(w, h, x, y))) { mos_ci_release(a); return 0; }
        for (size_t i = 0; i < (size_t)w * h; i++)
            out->px[i] = im->kind == K_COLOR ? color_px(a->px[i], im->sat, im->bri, im->con) : a->px[i] || im->op == MOS_CI_OP_MATRIX ? pix_op(im, a->px[i]) : 0;
        mos_ci_release(a);
        return out;
    case K_OVER: case K_BLEND: {
        int mode = im->kind == K_OVER ? -1 : im->op;
        a = flatten_rect(im->a, x, y, w, h);
        b = im->b && extent_of(im->b) ? flatten_rect(im->b, x, y, w, h) : crop_to(0, x, y, w, h);
        if (im->c) m = flatten_rect(im->c, x, y, w, h);
        if (!a || !b || (im->c && !m) || !(out = flat_new(w, h, x, y))) { mos_ci_release(a); mos_ci_release(b); mos_ci_release(m); mos_ci_release(out); return 0; }
        for (size_t i = 0; i < (size_t)w * h; i++)
            out->px[i] = mode < 0 ? over_px(a->px[i], b->px[i]) : blend_px(mode, im->p[0], a->px[i], b->px[i], m ? m->px[i] : 0);
        mos_ci_release(a); mos_ci_release(b); mos_ci_release(m);
        return out;
    }
    case K_BLUR: case K_BOX: {
        int r = im->kind == K_BLUR ? (int)ceil(im->radius * 3.0) : (int)ceil(im->radius);
        if (r < 0) r = 0;
        if (!(a = flatten_rect(im->a, x - r, y - r, w + 2 * r, h + 2 * r))) return 0;
        b = im->kind == K_BLUR ? flatten_blur(a, im->radius) : box_flat(a, im->radius);
        out = crop_to(b, x, y, w, h);
        mos_ci_release(a); mos_ci_release(b);
        return out;
    }
    case K_AFFINE: {   /* o retângulo de saída levado de volta para a origem */
        double det = im->ax * im->by - im->ay * im->bx;
        double ia = im->by / det, ib = -im->ay / det, ic = -im->bx / det, id = im->ax / det;
        double xs[4] = {x, x + w, x, x + w}, ys[4] = {y, y, y + h, y + h}, minx = 1e300, miny = 1e300, maxx = -1e300, maxy = -1e300;
        for (int k = 0; k < 4; k++) {
            double u = xs[k] - im->tx, v = ys[k] - im->ty, sx = ia * u + ic * v, sy = ib * u + id * v;
            if (sx < minx) minx = sx;
            if (sx > maxx) maxx = sx;
            if (sy < miny) miny = sy;
            if (sy > maxy) maxy = sy;
        }
        int sx0 = (int)floor(minx) - 2, sy0 = (int)floor(miny) - 2;
        if (!(a = flatten_rect(im->a, sx0, sy0, (int)ceil(maxx) + 2 - sx0, (int)ceil(maxy) + 2 - sy0))) return 0;
        b = flatten_affine(a, im->ax, im->ay, im->bx, im->by, im->tx, im->ty);
        out = crop_to(b, x, y, w, h);
        mos_ci_release(a); mos_ci_release(b);
        return out;
    }
    case K_PIXELLATE: {
        double cx = im->p[0], cy = im->p[1], sc = im->p[2];
        int gx0 = (int)floor((x - cx) / sc), gy0 = (int)floor((y - cy) / sc), gx1 = (int)floor((x + w - cx) / sc), gy1 = (int)floor((y + h - cy) / sc);
        int ax0 = (int)floor(cx + gx0 * sc) - 1, ay0 = (int)floor(cy + gy0 * sc) - 1;
        int ax1 = (int)ceil(cx + (gx1 + 1) * sc) + 1, ay1 = (int)ceil(cy + (gy1 + 1) * sc) + 1;
        if (!(a = flatten_rect(im->a, ax0, ay0, ax1 - ax0, ay1 - ay0)) || !(out = flat_new(w, h, x, y))) { mos_ci_release(a); return 0; }
        for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) {   /* cada casa toma a cor do centro dela */
            double px = x + i + 0.5, py = y + j + 0.5;
            double sx = cx + (floor((px - cx) / sc) + 0.5) * sc, sy = cy + (floor((py - cy) / sc) + 0.5) * sc;
            int ix = (int)floor(sx - a->ox), iy = (int)floor(sy - a->oy);
            out->px[(size_t)j * w + i] = ix >= 0 && iy >= 0 && ix < a->pw && iy < a->ph ? a->px[(size_t)iy * a->pw + ix] : 0;
        }
        mos_ci_release(a);
        return out;
    }
    case K_SCALE: default:
        return 0;
    }
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
    if (!extent_of(im)) { memset(px, 0, (size_t)w * (size_t)h * 4u); return 0; }   /* imagem vazia */
    mos_ci *flat = im->inf || has_inf(im) ? flatten_rect(im, (int)floor(x), (int)floor(y), w, h) : flatten(im);
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
