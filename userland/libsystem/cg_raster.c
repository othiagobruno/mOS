#include "cg_raster.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#if !defined(__STDC_HOSTED__) || __STDC_HOSTED__ == 0
/* o math.h do kshim só traz parte; o libSystem exporta estas (libc.c) */
double sin(double), hypot(double, double), fmod(double, double);
void qsort(void *base, size_t n, size_t sz, int (*cmp)(const void *, const void *));
#endif

#define CGR_PI 3.14159265358979323846

void cgr_init(cgr_poly *p) { memset(p, 0, sizeof *p); }
void cgr_free(cgr_poly *p) { free(p->pts); free(p->sub); free(p->closed); memset(p, 0, sizeof *p); }
void cgr_clear(cgr_poly *p) { p->n = 0; p->nsub = 0; }

static int grow_pts(cgr_poly *p) {
    if (p->n < p->cap) return 1;
    int nc = p->cap ? p->cap * 2 : 64;
    cgr_pt *np = realloc(p->pts, (size_t)nc * sizeof *np);
    if (!np) return 0;
    p->pts = np;
    p->cap = nc;
    return 1;
}
void cgr_move(cgr_poly *p, double x, double y) {
    if (p->nsub && p->n - p->sub[p->nsub - 1] == 1 && !p->closed[p->nsub - 1]) {
        p->pts[p->n - 1].x = x;                  /* moveTo seguido de moveTo: vale o último */
        p->pts[p->n - 1].y = y;
        return;
    }
    if (p->nsub == p->subcap) {
        int nc = p->subcap ? p->subcap * 2 : 8;
        int *ns = realloc(p->sub, (size_t)nc * sizeof *ns);
        uint8_t *ncl = ns ? realloc(p->closed, (size_t)nc) : 0;
        if (!ns || !ncl) { if (ns) p->sub = ns; return; }
        p->sub = ns;
        p->closed = ncl;
        p->subcap = nc;
    }
    if (!grow_pts(p)) return;
    p->sub[p->nsub] = p->n;
    p->closed[p->nsub] = 0;
    p->nsub++;
    p->pts[p->n].x = x;
    p->pts[p->n].y = y;
    p->n++;
}
void cgr_line(cgr_poly *p, double x, double y) {
    if (!p->nsub || p->closed[p->nsub - 1]) {
        cgr_pt at = p->n ? p->pts[p->sub[p->nsub - 1]] : (cgr_pt){x, y};
        cgr_move(p, at.x, at.y);
    }
    if (!grow_pts(p)) return;
    p->pts[p->n].x = x;
    p->pts[p->n].y = y;
    p->n++;
}
void cgr_close(cgr_poly *p) { if (p->nsub) p->closed[p->nsub - 1] = 1; }

static cgr_pt cur(const cgr_poly *p) { return p->n ? p->pts[p->n - 1] : (cgr_pt){0, 0}; }

static int segs_for(double len, double tol) {
    if (tol <= 0) tol = 0.25;
    int n = (int)ceil(sqrt(len / tol) * 0.7);
    return n < 1 ? 1 : n > 256 ? 256 : n;
}
void cgr_quad(cgr_poly *p, double cx, double cy, double x, double y, double tol) {
    cgr_pt a = cur(p);
    double len = hypot(cx - a.x, cy - a.y) + hypot(x - cx, y - cy);
    int n = segs_for(len, tol);
    for (int i = 1; i <= n; i++) {
        double t = (double)i / n, u = 1 - t;
        cgr_line(p, u * u * a.x + 2 * u * t * cx + t * t * x, u * u * a.y + 2 * u * t * cy + t * t * y);
    }
}
void cgr_cubic(cgr_poly *p, double c1x, double c1y, double c2x, double c2y, double x, double y, double tol) {
    cgr_pt a = cur(p);
    double len = hypot(c1x - a.x, c1y - a.y) + hypot(c2x - c1x, c2y - c1y) + hypot(x - c2x, y - c2y);
    int n = segs_for(len, tol);
    for (int i = 1; i <= n; i++) {
        double t = (double)i / n, u = 1 - t;
        double k0 = u * u * u, k1 = 3 * u * u * t, k2 = 3 * u * t * t, k3 = t * t * t;
        cgr_line(p, k0 * a.x + k1 * c1x + k2 * c2x + k3 * x, k0 * a.y + k1 * c1y + k2 * c2y + k3 * y);
    }
}
void cgr_arc(cgr_poly *p, double cx, double cy, double r, double a0, double a1, int cw, double tol) {
    double sweep = a1 - a0;
    if (cw) { while (sweep > 0) sweep -= 2 * CGR_PI; }
    else { while (sweep < 0) sweep += 2 * CGR_PI; }
    if (fabs(sweep) > 2 * CGR_PI) sweep = sweep > 0 ? 2 * CGR_PI : -2 * CGR_PI;
    double sx = cx + r * cos(a0), sy = cy + r * sin(a0);
    if (p->nsub && !p->closed[p->nsub - 1]) cgr_line(p, sx, sy);
    else cgr_move(p, sx, sy);
    int n = segs_for(fabs(sweep) * (r < 0 ? -r : r), tol);
    for (int i = 1; i <= n; i++) {
        double a = a0 + sweep * i / n;
        cgr_line(p, cx + r * cos(a), cy + r * sin(a));
    }
}
void cgr_transform(cgr_poly *p, double a, double b, double c, double d, double tx, double ty) {
    for (int i = 0; i < p->n; i++) {
        double x = p->pts[i].x, y = p->pts[i].y;
        p->pts[i].x = a * x + c * y + tx;
        p->pts[i].y = b * x + d * y + ty;
    }
}
int cgr_bbox(const cgr_poly *p, double *x0, double *y0, double *x1, double *y1) {
    if (!p->n) return 0;
    double a = p->pts[0].x, b = p->pts[0].y, c = a, d = b;
    for (int i = 1; i < p->n; i++) {
        if (p->pts[i].x < a) a = p->pts[i].x;
        if (p->pts[i].x > c) c = p->pts[i].x;
        if (p->pts[i].y < b) b = p->pts[i].y;
        if (p->pts[i].y > d) d = p->pts[i].y;
    }
    *x0 = a; *y0 = b; *x1 = c; *y1 = d;
    return 1;
}

/* ---------------------------------------------------------------- preenchimento */
typedef struct { double x; int dir; } cross_t;
static int cross_cmp(const void *a, const void *b) {
    double d = ((const cross_t *)a)->x - ((const cross_t *)b)->x;
    return d < 0 ? -1 : d > 0;
}
#define SUB 4      /* subamostras verticais por pixel */

int cgr_fill(const cgr_poly *p, int evenodd, int cx0, int cy0, int cx1, int cy1, cgr_emit_fn emit, void *ctx) {
    double bx0, by0, bx1, by1;
    if (!p->n || !cgr_bbox(p, &bx0, &by0, &bx1, &by1)) return 0;
    int x0 = (int)floor(bx0), y0 = (int)floor(by0), x1 = (int)ceil(bx1) + 1, y1 = (int)ceil(by1) + 1;
    if (x0 < cx0) x0 = cx0;
    if (y0 < cy0) y0 = cy0;
    if (x1 > cx1) x1 = cx1;
    if (y1 > cy1) y1 = cy1;
    if (x0 >= x1 || y0 >= y1) return 0;
    int w = x1 - x0;
    float *acc = calloc((size_t)w + 2, sizeof *acc);
    uint8_t *cov = malloc((size_t)w + 2);
    int ecap = p->n + p->nsub + 4;
    cross_t *xs = malloc((size_t)ecap * sizeof *xs);
    if (!acc || !cov || !xs) { free(acc); free(cov); free(xs); return 0; }
    for (int y = y0; y < y1; y++) {
        int any = 0;
        for (int s = 0; s < SUB; s++) {
            double sy = y + (s + 0.5) / SUB;
            int nx = 0;
            for (int k = 0; k < p->nsub; k++) {
                int a = p->sub[k], b = k + 1 < p->nsub ? p->sub[k + 1] : p->n;
                if (b - a < 2) continue;
                for (int i = a; i < b; i++) {
                    cgr_pt u = p->pts[i], v = p->pts[i + 1 < b ? i + 1 : a];   /* todo contorno fecha para preencher */
                    if (u.y == v.y) continue;
                    int dir = v.y > u.y ? 1 : -1;
                    double ylo = dir > 0 ? u.y : v.y, yhi = dir > 0 ? v.y : u.y;
                    if (sy < ylo || sy >= yhi) continue;
                    if (nx < ecap) {
                        xs[nx].x = u.x + (sy - u.y) * (v.x - u.x) / (v.y - u.y);
                        xs[nx].dir = dir;
                        nx++;
                    }
                }
            }
            if (nx < 2) continue;
            qsort(xs, (size_t)nx, sizeof *xs, cross_cmp);
            int wind = 0;
            for (int i = 0; i + 1 < nx; i++) {
                wind += evenodd ? 1 : xs[i].dir;
                int in = evenodd ? (wind & 1) : wind != 0;
                if (!in) continue;
                double a = xs[i].x - x0, b = xs[i + 1].x - x0;
                if (b <= 0 || a >= w) continue;
                if (a < 0) a = 0;
                if (b > w) b = w;
                int ia = (int)a, ib = (int)b;
                const float k = 1.0f / SUB;
                if (ia == ib) acc[ia] += (float)(b - a) * k;
                else {
                    acc[ia] += (float)(ia + 1 - a) * k;
                    for (int j = ia + 1; j < ib; j++) acc[j] += k;
                    if (ib < w) acc[ib] += (float)(b - ib) * k;
                }
                any = 1;
            }
        }
        if (!any) continue;
        int lo = -1, hi = -1;
        for (int i = 0; i < w; i++) {
            float c = acc[i];
            acc[i] = 0;
            int v = (int)(c * 255.0f + 0.5f);
            cov[i] = (uint8_t)(v > 255 ? 255 : v < 0 ? 0 : v);
            if (cov[i]) { if (lo < 0) lo = i; hi = i; }
        }
        if (lo >= 0) emit(ctx, y, x0 + lo, x0 + hi + 1, cov + lo);
    }
    free(acc);
    free(cov);
    free(xs);
    return 1;
}

/* ---------------------------------------------------------------- traço */
static double area2(const cgr_pt *q, int n) {
    double s = 0;
    for (int i = 0; i < n; i++) { const cgr_pt *a = &q[i], *b = &q[(i + 1) % n]; s += a->x * b->y - b->x * a->y; }
    return s;
}
/* polígono convexo fechado sempre no mesmo sentido (anti-horário em y para cima): nonzero une sem buracos */
static void emit_poly(cgr_poly *out, cgr_pt *q, int n) {
    if (n < 3) return;
    double a = area2(q, n);
    if (fabs(a) < 1e-12) return;
    cgr_move(out, q[0].x, q[0].y);
    if (a > 0) for (int i = 1; i < n; i++) cgr_line(out, q[i].x, q[i].y);
    else for (int i = n - 1; i >= 1; i--) cgr_line(out, q[i].x, q[i].y);
    cgr_close(out);
}
static void emit_circle(cgr_poly *out, cgr_pt c, double r, double tol) {
    cgr_pt q[64];
    int n = segs_for(2 * CGR_PI * r, tol) * 2;
    if (n < 8) n = 8;
    if (n > 64) n = 64;
    for (int i = 0; i < n; i++) { double a = 2 * CGR_PI * i / n; q[i].x = c.x + r * cos(a); q[i].y = c.y + r * sin(a); }
    emit_poly(out, q, n);
}
static void stroke_open(cgr_poly *out, const cgr_pt *v, int n, int closed, const cgr_stroke_t *st) {
    double hw = st->width / 2;
    /* tira pontos repetidos */
    cgr_pt *u = malloc((size_t)(n + 1) * sizeof *u);
    if (!u) return;
    int m = 0;
    for (int i = 0; i < n; i++) if (!m || hypot(v[i].x - u[m - 1].x, v[i].y - u[m - 1].y) > 1e-9) u[m++] = v[i];
    if (closed && m > 1 && hypot(u[0].x - u[m - 1].x, u[0].y - u[m - 1].y) <= 1e-9) m--;
    if (m == 1) {                               /* ponto: só aparece com cap redondo ou quadrado */
        if (st->cap == 1) emit_circle(out, u[0], hw, st->tol);
        else if (st->cap == 2) { cgr_pt q[4] = {{u[0].x - hw, u[0].y - hw}, {u[0].x + hw, u[0].y - hw}, {u[0].x + hw, u[0].y + hw}, {u[0].x - hw, u[0].y + hw}}; emit_poly(out, q, 4); }
        free(u);
        return;
    }
    int segs = closed ? m : m - 1;
    for (int i = 0; i < segs; i++) {
        cgr_pt a = u[i], b = u[(i + 1) % m];
        double dx = b.x - a.x, dy = b.y - a.y, L = hypot(dx, dy);
        double nx = -dy / L * hw, ny = dx / L * hw;
        double ex = dx / L * hw, ey = dy / L * hw;
        if (!closed && st->cap == 2) {          /* square: estende as pontas */
            if (i == 0) { a.x -= ex; a.y -= ey; }
            if (i == segs - 1) { b.x += ex; b.y += ey; }
        }
        cgr_pt q[4] = {{a.x + nx, a.y + ny}, {b.x + nx, b.y + ny}, {b.x - nx, b.y - ny}, {a.x - nx, a.y - ny}};
        emit_poly(out, q, 4);
    }
    /* junções nos vértices internos (e em todos, se fechado) */
    int j0 = closed ? 0 : 1, j1 = closed ? m : m - 1;
    for (int i = j0; i < j1; i++) {
        cgr_pt p0 = u[(i - 1 + m) % m], p1 = u[i], p2 = u[(i + 1) % m];
        double d1x = p1.x - p0.x, d1y = p1.y - p0.y, l1 = hypot(d1x, d1y);
        double d2x = p2.x - p1.x, d2y = p2.y - p1.y, l2 = hypot(d2x, d2y);
        d1x /= l1; d1y /= l1; d2x /= l2; d2y /= l2;
        double crs = d1x * d2y - d1y * d2x, dot = d1x * d2x + d1y * d2y;
        if (fabs(crs) < 1e-9 && dot > 0) continue;   /* reto */
        if (st->join == 1) { emit_circle(out, p1, hw, st->tol); continue; }
        /* lado de fora da curva */
        double s = crs > 0 ? -1 : 1;
        cgr_pt o1 = {p1.x - d1y * hw * s, p1.y + d1x * hw * s};
        cgr_pt o2 = {p1.x - d2y * hw * s, p1.y + d2x * hw * s};
        double cos_half = sqrt((1 + dot) / 2);       /* cos(θ/2), θ = ângulo entre as direções */
        double ratio = cos_half > 1e-9 ? 1 / cos_half : 1e9;
        if (st->join == 0 && ratio <= (st->miter > 0 ? st->miter : 10)) {
            double bx = -d1y * s + -d2y * s, by = d1x * s + d2x * s, bl = hypot(bx, by);
            cgr_pt tip = {p1.x + bx / bl * hw * ratio, p1.y + by / bl * hw * ratio};
            cgr_pt q[4] = {p1, o1, tip, o2};
            emit_poly(out, q, 4);
        } else {
            cgr_pt q[3] = {p1, o1, o2};
            emit_poly(out, q, 3);
        }
    }
    if (!closed && st->cap == 1) { emit_circle(out, u[0], hw, st->tol); emit_circle(out, u[m - 1], hw, st->tol); }
    free(u);
}

void cgr_dash(const cgr_poly *in, const double *dash, int ndash, double phase, cgr_poly *out) {
    double total = 0;
    for (int i = 0; i < ndash; i++) total += dash[i] > 0 ? dash[i] : 0;
    for (int k = 0; k < in->nsub; k++) {
        int a = in->sub[k], b = k + 1 < in->nsub ? in->sub[k + 1] : in->n;
        if (b - a < 2) continue;
        if (total <= 0) {                        /* tracejado vazio: o contorno inteiro */
            cgr_move(out, in->pts[a].x, in->pts[a].y);
            for (int i = a + 1; i < b; i++) cgr_line(out, in->pts[i].x, in->pts[i].y);
            if (in->closed[k]) cgr_line(out, in->pts[a].x, in->pts[a].y);
            continue;
        }
        /* posição inicial no padrão */
        double ph = fmod(phase, total);
        if (ph < 0) ph += total;
        int di = 0;
        while (ph >= dash[di]) { ph -= dash[di]; di = (di + 1) % ndash; }
        double left = dash[di] - ph;
        int on = !(di & 1), pen = 0;
        int last = in->closed[k] ? b + 1 : b;
        for (int i = a; i + 1 < last; i++) {
            cgr_pt u = in->pts[i], v = in->pts[i + 1 < b ? i + 1 : a];
            double L = hypot(v.x - u.x, v.y - u.y), pos = 0;
            while (L - pos > 1e-12) {
                double step = left < L - pos ? left : L - pos;
                double t0 = pos / L, t1 = (pos + step) / L;
                if (on) {
                    if (!pen) { cgr_move(out, u.x + (v.x - u.x) * t0, u.y + (v.y - u.y) * t0); pen = 1; }
                    cgr_line(out, u.x + (v.x - u.x) * t1, u.y + (v.y - u.y) * t1);
                }
                pos += step;
                left -= step;
                if (left <= 1e-12) {
                    di = (di + 1) % ndash;
                    left = dash[di];
                    on = !on;
                    pen = 0;
                }
            }
        }
    }
}

void cgr_stroke(const cgr_poly *in, const cgr_stroke_t *st, cgr_poly *out) {
    cgr_poly dashed;
    const cgr_poly *src = in;
    if (st->width <= 0) return;
    cgr_init(&dashed);
    if (st->ndash > 0 && st->dash) { cgr_dash(in, st->dash, st->ndash, st->phase, &dashed); src = &dashed; }
    for (int k = 0; k < src->nsub; k++) {
        int a = src->sub[k], b = k + 1 < src->nsub ? src->sub[k + 1] : src->n;
        stroke_open(out, src->pts + a, b - a, src->closed[k], st);
    }
    cgr_free(&dashed);
}

/* ---------------------------------------------------------------- gradientes */
uint32_t cgr_grad_at(const cgr_grad *g, double t) {
    double c[4];
    if (g->n <= 0) return 0;
    if (t <= g->loc[0] || g->n == 1) memcpy(c, g->rgba, sizeof c);
    else if (t >= g->loc[g->n - 1]) memcpy(c, g->rgba + 4 * (g->n - 1), sizeof c);
    else {
        int i = 0;
        while (i + 1 < g->n && t > g->loc[i + 1]) i++;
        double span = g->loc[i + 1] - g->loc[i], f = span > 0 ? (t - g->loc[i]) / span : 0;
        for (int k = 0; k < 4; k++) c[k] = g->rgba[4 * i + k] + (g->rgba[4 * (i + 1) + k] - g->rgba[4 * i + k]) * f;
    }
    for (int k = 0; k < 4; k++) c[k] = c[k] < 0 ? 0 : c[k] > 1 ? 1 : c[k];
    uint32_t A = (uint32_t)(c[3] * 255 + 0.5);
    uint32_t R = (uint32_t)(c[0] * c[3] * 255 + 0.5), G = (uint32_t)(c[1] * c[3] * 255 + 0.5), B = (uint32_t)(c[2] * c[3] * 255 + 0.5);
    return A << 24 | R << 16 | G << 8 | B;
}
int cgr_linear_t(double px, double py, double sx, double sy, double ex, double ey, int flags, double *t) {
    double dx = ex - sx, dy = ey - sy, l2 = dx * dx + dy * dy;
    double v = l2 > 0 ? ((px - sx) * dx + (py - sy) * dy) / l2 : 0;
    if (v < 0) { if (!(flags & 1)) return 0; v = 0; }
    if (v > 1) { if (!(flags & 2)) return 0; v = 1; }
    *t = v;
    return 1;
}
int cgr_radial_t(double px, double py, double c0x, double c0y, double r0, double c1x, double c1y, double r1, int flags, double *t) {
    double cdx = c1x - c0x, cdy = c1y - c0y, dr = r1 - r0;
    double pdx = px - c0x, pdy = py - c0y;
    double a = cdx * cdx + cdy * cdy - dr * dr;
    double b = pdx * cdx + pdy * cdy + r0 * dr;
    double c = pdx * pdx + pdy * pdy - r0 * r0;
    double cand[2];
    int nc = 0;
    if (fabs(a) < 1e-12) {
        if (fabs(b) < 1e-12) return 0;
        cand[nc++] = c / (2 * b);
    } else {
        double disc = b * b - a * c;
        if (disc < 0) return 0;
        double s = sqrt(disc);
        double t1 = (b + s) / a, t2 = (b - s) / a;
        cand[nc++] = t1 > t2 ? t1 : t2;
        cand[nc++] = t1 > t2 ? t2 : t1;
    }
    for (int i = 0; i < nc; i++) {
        double v = cand[i];
        if (r0 + v * dr < 0) continue;
        if (v < 0) { if (!(flags & 1)) continue; v = 0; }
        if (v > 1) { if (!(flags & 2)) continue; v = 1; }
        *t = v;
        return 1;
    }
    return 0;
}
