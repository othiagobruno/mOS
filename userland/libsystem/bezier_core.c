/* mOS — geometria de caminhos (ver bezier_core.h). Código original do mOS. */
#include "bezier_core.h"

#define PI 3.14159265358979323846
static double fabs_(double v) { return v < 0 ? -v : v; }
static double sqrt_(double v) { return __builtin_sqrt(v); }
static double cos_(double v) { return __builtin_cos(v); }
static double sin_(double v) { return __builtin_sin(v); }
static double tan_(double v) { return __builtin_tan(v); }
static double atan2_(double y, double x) { return __builtin_atan2(y, x); }

/* ================================================================ elementos */
static int push(bz_path *p, int kind, const double *pts, int npts) {
    if (p->n == p->cap) {
        int nc = p->cap ? p->cap * 2 : 16;
        bz_el *e = bz_realloc(p->el, (unsigned long)nc * sizeof *e);
        if (!e) return 0;
        p->el = e; p->cap = nc;
    }
    bz_el *e = &p->el[p->n++];
    e->kind = kind;
    for (int i = 0; i < 6; i++) e->p[i] = i < npts ? pts[i] : 0;
    return 1;
}
void bz_clear(bz_path *p) { p->n = 0; }
void bz_release(bz_path *p) { bz_free(p->el); p->el = 0; p->n = p->cap = 0; }
int bz_copy(bz_path *d, const bz_path *s) { bz_clear(d); return bz_append(d, s); }
int bz_append(bz_path *p, const bz_path *q) {
    for (int i = 0; i < q->n; i++) if (!push(p, q->el[i].kind, q->el[i].p, 6)) return 0;
    return 1;
}
/* ponto atual: o fim do último elemento; depois de close, o começo do subcaminho */
int bz_current(const bz_path *p, double *x, double *y) {
    if (!p->n) return 0;
    const bz_el *e = &p->el[p->n - 1];
    if (e->kind == BZ_CLOSE) {
        for (int i = p->n - 1; i >= 0; i--) if (p->el[i].kind == BZ_MOVE) { *x = p->el[i].p[0]; *y = p->el[i].p[1]; return 1; }
        return 0;
    }
    if (e->kind == BZ_CURVE) { *x = e->p[4]; *y = e->p[5]; } else { *x = e->p[0]; *y = e->p[1]; }
    return 1;
}
int bz_move(bz_path *p, double x, double y) { double v[2] = {x, y}; return push(p, BZ_MOVE, v, 2); }
int bz_line(bz_path *p, double x, double y) {
    double v[2] = {x, y}, cx, cy;
    if (!bz_current(p, &cx, &cy)) return push(p, BZ_MOVE, v, 2);
    if (p->el[p->n - 1].kind == BZ_CLOSE && !bz_move(p, cx, cy)) return 0;
    return push(p, BZ_LINE, v, 2);
}
int bz_curve(bz_path *p, double c1x, double c1y, double c2x, double c2y, double x, double y) {
    double v[6] = {c1x, c1y, c2x, c2y, x, y}, cx, cy;
    if (!bz_current(p, &cx, &cy)) { if (!bz_move(p, c1x, c1y)) return 0; }
    else if (p->el[p->n - 1].kind == BZ_CLOSE && !bz_move(p, cx, cy)) return 0;
    return push(p, BZ_CURVE, v, 6);
}
int bz_close(bz_path *p) {
    if (!p->n || p->el[p->n - 1].kind == BZ_CLOSE) return 1;
    return push(p, BZ_CLOSE, 0, 0);
}
void bz_rect_path(bz_path *p, bz_rect r) {
    bz_move(p, r.x, r.y); bz_line(p, r.x + r.w, r.y); bz_line(p, r.x + r.w, r.y + r.h); bz_line(p, r.x, r.y + r.h); bz_close(p);
}
#define KAPPA 0.5522847498307936
void bz_oval(bz_path *p, bz_rect r) {
    double cx = r.x + r.w / 2, cy = r.y + r.h / 2, rx = r.w / 2, ry = r.h / 2, kx = rx * KAPPA, ky = ry * KAPPA;
    bz_move(p, cx + rx, cy);
    bz_curve(p, cx + rx, cy + ky, cx + kx, cy + ry, cx, cy + ry);
    bz_curve(p, cx - kx, cy + ry, cx - rx, cy + ky, cx - rx, cy);
    bz_curve(p, cx - rx, cy - ky, cx - kx, cy - ry, cx, cy - ry);
    bz_curve(p, cx + kx, cy - ry, cx + rx, cy - ky, cx + rx, cy);
    bz_close(p);
}
void bz_rrect(bz_path *p, bz_rect r, double rx, double ry) {
    if (rx > r.w / 2) rx = r.w / 2;
    if (ry > r.h / 2) ry = r.h / 2;
    if (rx <= 0 || ry <= 0) { bz_rect_path(p, r); return; }
    double x0 = r.x, y0 = r.y, x1 = r.x + r.w, y1 = r.y + r.h, kx = rx * KAPPA, ky = ry * KAPPA;
    bz_move(p, x0 + rx, y0);
    bz_line(p, x1 - rx, y0);
    bz_curve(p, x1 - rx + kx, y0, x1, y0 + ry - ky, x1, y0 + ry);
    bz_line(p, x1, y1 - ry);
    bz_curve(p, x1, y1 - ry + ky, x1 - rx + kx, y1, x1 - rx, y1);
    bz_line(p, x0 + rx, y1);
    bz_curve(p, x0 + rx - kx, y1, x0, y1 - ry + ky, x0, y1 - ry);
    bz_line(p, x0, y0 + ry);
    bz_curve(p, x0, y0 + ry - ky, x0 + rx - kx, y0, x0 + rx, y0);
    bz_close(p);
}
/* arco em radianos de a0 a a1 (já com o sentido certo), em pedaços de até 90° */
static void arc_rad(bz_path *p, double cx, double cy, double r, double a0, double a1, int connect) {
    double sx = cx + r * cos_(a0), sy = cy + r * sin_(a0), x, y;
    if (connect && bz_current(p, &x, &y)) bz_line(p, sx, sy); else bz_move(p, sx, sy);
    double sweep = a1 - a0;
    int segs = (int)(fabs_(sweep) / (PI / 2) + 0.999999);
    if (segs < 1) segs = 1;
    double step = sweep / segs, k = 4.0 / 3.0 * tan_(step / 4);
    for (int i = 0; i < segs; i++) {
        double t0 = a0 + i * step, t1 = t0 + step;
        double c0 = cos_(t0), s0 = sin_(t0), c1 = cos_(t1), s1 = sin_(t1);
        bz_curve(p, cx + r * (c0 - k * s0), cy + r * (s0 + k * c0), cx + r * (c1 + k * s1), cy + r * (s1 - k * c1), cx + r * c1, cy + r * s1);
    }
}
void bz_arc(bz_path *p, double cx, double cy, double r, double a0, double a1, int clockwise) {
    if (r < 0) r = -r;
    double s = a0 * PI / 180, e = a1 * PI / 180;
    if (!clockwise) { while (e < s) e += 2 * PI; if (e - s > 2 * PI) e = s + 2 * PI; }
    else { while (e > s) e -= 2 * PI; if (s - e > 2 * PI) e = s - 2 * PI; }
    arc_rad(p, cx, cy, r, s, e, 1);
}
void bz_arc_to(bz_path *p, double x1, double y1, double x2, double y2, double r) {
    double x0, y0;
    if (!bz_current(p, &x0, &y0)) { bz_move(p, x1, y1); return; }
    double ux = x0 - x1, uy = y0 - y1, vx = x2 - x1, vy = y2 - y1;
    double lu = sqrt_(ux * ux + uy * uy), lv = sqrt_(vx * vx + vy * vy);
    if (lu < 1e-9 || lv < 1e-9 || r <= 0) { bz_line(p, x1, y1); return; }
    ux /= lu; uy /= lu; vx /= lv; vy /= lv;
    double cosang = ux * vx + uy * vy;
    if (cosang > 1) cosang = 1;
    if (cosang < -1) cosang = -1;
    double ang = __builtin_acos(cosang);
    if (ang < 1e-9 || PI - ang < 1e-9) { bz_line(p, x1, y1); return; }   /* colineares */
    double d = r / tan_(ang / 2);
    double t1x = x1 + ux * d, t1y = y1 + uy * d, t2x = x1 + vx * d, t2y = y1 + vy * d;
    double bx = ux + vx, by = uy + vy, bl = sqrt_(bx * bx + by * by);
    double h = r / sin_(ang / 2), cx = x1 + bx / bl * h, cy = y1 + by / bl * h;
    double a0 = atan2_(t1y - cy, t1x - cx), a1 = atan2_(t2y - cy, t2x - cx);
    double cross = ux * vy - uy * vx;                      /* > 0: a curva gira no sentido horário */
    if (cross > 0) { while (a1 > a0) a1 -= 2 * PI; } else { while (a1 < a0) a1 += 2 * PI; }
    arc_rad(p, cx, cy, r, a0, a1, 1);
}
void bz_transform(bz_path *p, const double m[6]) {
    for (int i = 0; i < p->n; i++) {
        int np = p->el[i].kind == BZ_CURVE ? 3 : p->el[i].kind == BZ_CLOSE ? 0 : 1;
        for (int k = 0; k < np; k++) {
            double x = p->el[i].p[k * 2], y = p->el[i].p[k * 2 + 1];
            p->el[i].p[k * 2] = m[0] * x + m[2] * y + m[4];
            p->el[i].p[k * 2 + 1] = m[1] * x + m[3] * y + m[5];
        }
    }
}
/* inverte cada subcaminho: começa no fim, segmentos de trás para a frente, close no mesmo lugar */
int bz_reverse(bz_path *d, const bz_path *s) {
    bz_clear(d);
    int i = 0;
    while (i < s->n) {
        if (s->el[i].kind != BZ_MOVE) { i++; continue; }
        int j = i + 1;
        while (j < s->n && s->el[j].kind != BZ_MOVE && s->el[j].kind != BZ_CLOSE) j++;
        int closed = j < s->n && s->el[j].kind == BZ_CLOSE;
        /* pontos finais de cada segmento: end[k] é o ponto ao fim do elemento i+k */
        double ex, ey;
        const bz_el *last = &s->el[j - 1];
        if (last->kind == BZ_CURVE) { ex = last->p[4]; ey = last->p[5]; } else { ex = last->p[0]; ey = last->p[1]; }
        if (!bz_move(d, ex, ey)) return 0;
        for (int k = j - 1; k > i; k--) {
            const bz_el *e = &s->el[k], *prev = &s->el[k - 1];
            double px = prev->kind == BZ_CURVE ? prev->p[4] : prev->p[0], py = prev->kind == BZ_CURVE ? prev->p[5] : prev->p[1];
            if (e->kind == BZ_CURVE) { if (!bz_curve(d, e->p[2], e->p[3], e->p[0], e->p[1], px, py)) return 0; }
            else if (!bz_line(d, px, py)) return 0;
        }
        if (closed && !bz_close(d)) return 0;
        i = closed ? j + 1 : j;
    }
    return 1;
}
bz_rect bz_control_bounds(const bz_path *p) {
    double x0 = 1e300, y0 = 1e300, x1 = -1e300, y1 = -1e300;
    for (int i = 0; i < p->n; i++) {
        int np = p->el[i].kind == BZ_CURVE ? 3 : p->el[i].kind == BZ_CLOSE ? 0 : 1;
        for (int k = 0; k < np; k++) {
            double x = p->el[i].p[k * 2], y = p->el[i].p[k * 2 + 1];
            if (x < x0) x0 = x;
            if (x > x1) x1 = x;
            if (y < y0) y0 = y;
            if (y > y1) y1 = y;
        }
    }
    bz_rect r = {0, 0, 0, 0};
    if (x1 >= x0) { r.x = x0; r.y = y0; r.w = x1 - x0; r.h = y1 - y0; }
    return r;
}

/* ================================================================ achatamento */
static int fpush(bz_flat *f, double x, double y) {
    if (f->n == f->cap) {
        int nc = f->cap ? f->cap * 2 : 64;
        double *xy = bz_realloc(f->xy, (unsigned long)nc * 2 * sizeof(double));
        if (!xy) return 0;
        f->xy = xy; f->cap = nc;
    }
    f->xy[f->n * 2] = x; f->xy[f->n * 2 + 1] = y;
    f->n++;
    return 1;
}
static int fring(bz_flat *f) {
    if (f->nr == f->rcap) {
        int nc = f->rcap ? f->rcap * 2 : 8;
        int *st = bz_realloc(f->start, (unsigned long)nc * sizeof(int));
        if (!st) return 0;
        f->start = st;
        unsigned char *cl = bz_realloc(f->closed, (unsigned long)nc);
        if (!cl) return 0;
        f->closed = cl; f->rcap = nc;
    }
    f->start[f->nr] = f->n;
    f->closed[f->nr] = 0;
    f->nr++;
    return 1;
}
void bz_flat_release(bz_flat *f) { bz_free(f->xy); bz_free(f->start); bz_free(f->closed); f->xy = 0; f->start = 0; f->closed = 0; f->n = f->nr = f->cap = f->rcap = 0; }
int bz_flatten(const bz_path *p, double tol, bz_flat *f) {
    f->n = f->nr = 0;
    if (tol <= 0) tol = 0.25;
    double cx = 0, cy = 0, sx = 0, sy = 0;
    int open = 0;
    for (int i = 0; i < p->n; i++) {
        const bz_el *e = &p->el[i];
        switch (e->kind) {
        case BZ_MOVE:
            if (!fring(f) || !fpush(f, e->p[0], e->p[1])) return -1;
            cx = sx = e->p[0]; cy = sy = e->p[1]; open = 1;
            break;
        case BZ_LINE:
            if (!open) { if (!fring(f) || !fpush(f, cx, cy)) return -1; open = 1; }
            if (!fpush(f, e->p[0], e->p[1])) return -1;
            cx = e->p[0]; cy = e->p[1];
            break;
        case BZ_CURVE: {
            if (!open) { if (!fring(f) || !fpush(f, cx, cy)) return -1; open = 1; }
            double ax = cx - 2 * e->p[0] + e->p[2], ay = cy - 2 * e->p[1] + e->p[3];
            double bx = e->p[0] - 2 * e->p[2] + e->p[4], by = e->p[1] - 2 * e->p[3] + e->p[5];
            double dd = sqrt_(ax * ax + ay * ay), d2 = sqrt_(bx * bx + by * by);
            if (d2 > dd) dd = d2;
            int segs = (int)(sqrt_(dd * 0.75 / tol)) + 1;
            if (segs > 256) segs = 256;
            for (int k = 1; k <= segs; k++) {
                double t = (double)k / segs, u = 1 - t;
                double x = u * u * u * cx + 3 * u * u * t * e->p[0] + 3 * u * t * t * e->p[2] + t * t * t * e->p[4];
                double y = u * u * u * cy + 3 * u * u * t * e->p[1] + 3 * u * t * t * e->p[3] + t * t * t * e->p[5];
                if (!fpush(f, x, y)) return -1;
            }
            cx = e->p[4]; cy = e->p[5];
            break;
        }
        case BZ_CLOSE:
            if (open && f->nr) f->closed[f->nr - 1] = 1;
            cx = sx; cy = sy; open = 0;
            break;
        }
    }
    return f->nr;
}
static int ring_len(const bz_flat *f, int r) { return (r + 1 < f->nr ? f->start[r + 1] : f->n) - f->start[r]; }
bz_rect bz_bounds(const bz_path *p) {
    bz_flat f = {0, 0, 0, 0, 0, 0, 0};
    bz_rect r = {0, 0, 0, 0};
    if (bz_flatten(p, 0.1, &f) > 0 && f.n) {
        double x0 = f.xy[0], y0 = f.xy[1], x1 = x0, y1 = y0;
        for (int i = 1; i < f.n; i++) {
            double x = f.xy[i * 2], y = f.xy[i * 2 + 1];
            if (x < x0) x0 = x;
            if (x > x1) x1 = x;
            if (y < y0) y0 = y;
            if (y > y1) y1 = y;
        }
        r.x = x0; r.y = y0; r.w = x1 - x0; r.h = y1 - y0;
    }
    bz_flat_release(&f);
    return r;
}
/* número de voltas de um anel (fechado implicitamente) em torno do ponto */
static int winding(const double *xy, int n, double x, double y) {
    int w = 0;
    for (int i = 0; i < n; i++) {
        double x0 = xy[i * 2], y0 = xy[i * 2 + 1], x1 = xy[((i + 1) % n) * 2], y1 = xy[((i + 1) % n) * 2 + 1];
        if (y0 <= y) {
            if (y1 > y && (x1 - x0) * (y - y0) - (x - x0) * (y1 - y0) > 0) w++;
        } else if (y1 <= y && (x1 - x0) * (y - y0) - (x - x0) * (y1 - y0) < 0) w--;
    }
    return w;
}
int bz_contains(const bz_path *p, double x, double y, int evenodd) {
    bz_flat f = {0, 0, 0, 0, 0, 0, 0};
    int w = 0;
    if (bz_flatten(p, 0.1, &f) > 0)
        for (int r = 0; r < f.nr; r++) {
            int n = ring_len(&f, r);
            if (n >= 3) w += winding(f.xy + f.start[r] * 2, n, x, y);
        }
    bz_flat_release(&f);
    return evenodd ? (w & 1) != 0 : w != 0;
}
static double area2(const double *xy, int n) {
    double a = 0;
    for (int i = 0; i < n; i++) { int j = (i + 1) % n; a += xy[i * 2] * xy[j * 2 + 1] - xy[j * 2] * xy[i * 2 + 1]; }
    return a;
}
int bz_fill_polygon(const bz_flat *f, int evenodd, float **out) {
    int total = 0, first = -1;
    for (int r = 0; r < f->nr; r++) if (ring_len(f, r) >= 3) { total += ring_len(f, r) + 2; if (first < 0) first = r; }
    *out = 0;
    if (first < 0) return 0;
    float *o = bz_alloc((unsigned long)total * 2 * sizeof(float));
    if (!o) return -1;
    double ax = f->xy[f->start[first] * 2], ay = f->xy[f->start[first] * 2 + 1];
    int k = 0;
    for (int r = 0; r < f->nr; r++) {
        int n = ring_len(f, r);
        if (n < 3) continue;
        const double *xy = f->xy + f->start[r] * 2;
        int reverse = 0;
        if (evenodd) {
            /* par-ímpar → não-zero: anel dentro de um número par de outros gira no sentido positivo, ímpar no negativo */
            int depth = 0;
            for (int q = 0; q < f->nr; q++)
                if (q != r && ring_len(f, q) >= 3 && (winding(f->xy + f->start[q] * 2, ring_len(f, q), xy[0], xy[1]) & 1)) depth++;
            double a = area2(xy, n);
            reverse = (depth & 1) ? a > 0 : a < 0;
        }
        for (int i = 0; i < n; i++) {
            int s = reverse ? (n - i) % n : i;
            o[k * 2] = (float)xy[s * 2]; o[k * 2 + 1] = (float)xy[s * 2 + 1]; k++;
        }
        o[k * 2] = (float)xy[0]; o[k * 2 + 1] = (float)xy[1]; k++;          /* fecha o anel */
        o[k * 2] = (float)ax; o[k * 2 + 1] = (float)ay; k++;                /* ponte de volta à âncora */
    }
    *out = o;
    return k;
}
void bz_dash(const bz_flat *f, const double *pat, int count, double phase,
             void (*emit)(void *ctx, const double *xy, int n), void *ctx) {
    double len = 0;
    for (int i = 0; i < count; i++) len += pat[i] > 0 ? pat[i] : 0;
    double tmp[2 * 512];
    for (int r = 0; r < f->nr; r++) {
        int n = ring_len(f, r), segs = f->closed[r] ? n : n - 1;
        const double *xy = f->xy + f->start[r] * 2;
        if (n < 2) continue;
        if (count <= 0 || len <= 0) {   /* sem tracejado: o anel inteiro, em pedaços que cabem no buffer */
            int m = 0;
            for (int i = 0; i <= segs; i++) {
                tmp[m * 2] = xy[(i % n) * 2]; tmp[m * 2 + 1] = xy[(i % n) * 2 + 1]; m++;
                if (m == 512) { emit(ctx, tmp, m); tmp[0] = tmp[(m - 1) * 2]; tmp[1] = tmp[(m - 1) * 2 + 1]; m = 1; }
            }
            if (m >= 2) emit(ctx, tmp, m);
            continue;
        }
        /* posição inicial no padrão */
        double ph = phase;
        while (ph < 0) ph += len;
        while (ph >= len) ph -= len;
        int idx = 0;
        while (ph >= pat[idx]) { ph -= pat[idx]; idx = (idx + 1) % count; }
        double left = pat[idx] - ph;
        int on = (idx & 1) == 0, m = 0;
        if (on) { tmp[0] = xy[0]; tmp[1] = xy[1]; m = 1; }
        for (int i = 0; i < segs; i++) {
            double x0 = xy[i * 2], y0 = xy[i * 2 + 1], x1 = xy[((i + 1) % n) * 2], y1 = xy[((i + 1) % n) * 2 + 1];
            double dx = x1 - x0, dy = y1 - y0, sl = sqrt_(dx * dx + dy * dy), pos = 0;
            while (sl - pos > left) {
                pos += left;
                double px = x0 + dx * pos / sl, py = y0 + dy * pos / sl;
                if (on) { tmp[m * 2] = px; tmp[m * 2 + 1] = py; m++; emit(ctx, tmp, m); m = 0; }
                else { tmp[0] = px; tmp[1] = py; m = 1; }
                on = !on;
                idx = (idx + 1) % count;
                left = pat[idx];
                if (left <= 0) left = 1e-6;
            }
            left -= sl - pos;
            if (on) {
                tmp[m * 2] = x1; tmp[m * 2 + 1] = y1; m++;
                if (m == 512) { emit(ctx, tmp, m); tmp[0] = x1; tmp[1] = y1; m = 1; }
            }
        }
        if (on && m >= 2) emit(ctx, tmp, m);
    }
}
