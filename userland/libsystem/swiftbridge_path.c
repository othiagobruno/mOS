/*
 * mOS libSystem — ponte C de caminhos (Path), traço, recorte, máscara e sombra para o renderer do
 * SwiftUI do mOS (OpenSwiftUIMOS/Core/MOSDisplayListRenderer.swift). API em
 * userland/sdk/swift/MOSBridge/MOSBridge_path.h.
 *
 * Toda a geometria é feita aqui, em double: os elementos ficam em coordenadas locais com uma afim
 * local -> view; no desenho as curvas são achatadas, o traço vira anéis (quadriláteros por segmento,
 * junções e terminações, todos com a mesma orientação, unidos pela regra não-zero) e tudo é levado
 * para pixels da janela. A saída usa as primitivas do kernel por meio de appkit.c (cx_gfx_*):
 *   - preenchimento não-zero sem recorte por caminho: um único GC_POLY (o kernel acumula área com
 *     sinal e satura |a|, ou seja, regra não-zero com AA). Vários anéis viram um polígono só, ligados
 *     por pontes de ida e volta ao primeiro ponto, que se cancelam na acumulação;
 *   - par-ímpar, recorte por caminho, máscara ou polígono grande demais: rasterização própria
 *     (4 sub-linhas por pixel, cobertura horizontal exata) numa máscara, multiplicada pelas máscaras
 *     de recorte e enviada como imagem ARGB (GC_IMAGE);
 *   - recorte retangular: GC_CLIP com a interseção da pilha (vale também para texto e imagens).
 */
#include "foundation.h"
#include "libc.h"

double sqrt(double), floor(double), ceil(double), fabs(double), cos(double), sin(double), atan2(double, double);

extern void cx_gfx_view_xform(double *ox, double *oy, double *sy);
extern void cx_gfx_view_rect_px(double *x, double *y, double *w, double *h);
extern void cx_gfx_clip_px(double x, double y, double w, double h);
extern void cx_gfx_poly_px(const float *xy, u32 n, u32 argb);
extern void cx_gfx_rrect_px(double x, double y, double w, double h, double r, u32 argb);
extern void cx_gfx_image_px(int x, int y, int w, int h, const u32 *px);

#define PI 3.14159265358979323846
static int trace = -1;
static int tracing(void) { if (trace < 0) trace = getenv("MOS_PATH_TRACE") ? 1 : 0; return trace; }
static int raster_flag = -1;
void mos_path_trace(int on) { trace = on & 1; raster_flag = (on & 2) != 0; }
#define TR(...) do { if (tracing()) cx_fdprintf(2, __VA_ARGS__); } while (0)
/* %f do cx_fdprintf pode não existir: inteiros em centésimos */
#define C(v) ((long)((v) * 100))

static double dmin(double a, double b) { return a < b ? a : b; }
static double dmax(double a, double b) { return a > b ? a : b; }
static double hypot_(double x, double y) { return sqrt(x * x + y * y); }
static double acos_(double c) { return atan2(sqrt(dmax(1 - c * c, 0)), c); }

/* ================================================================ caminho */
enum { OP_MOVE, OP_LINE, OP_QUAD, OP_CURVE, OP_CLOSE };
typedef struct {
    u8 *op; int nop, capop;
    double *v; int nv, capv;
    double m[6];        /* a b c d tx ty */
    int rd_i, rd_off;   /* cursor de mos_path_element */
} mpath_t;

void *mos_path_create(void) {
    mpath_t *p = calloc(1, sizeof *p);
    p->m[0] = p->m[3] = 1;
    return p;
}
void mos_path_destroy(void *pp) { mpath_t *p = pp; if (!p) return; free(p->op); free(p->v); free(p); }
void mos_path_clear(void *pp) { mpath_t *p = pp; if (!p) return; p->nop = p->nv = p->rd_i = p->rd_off = 0;p->m[0] = p->m[3] = 1; p->m[1] = p->m[2] = p->m[4] = p->m[5] = 0; }
void mos_path_set_transform(void *pp, double a, double b, double c, double d, double tx, double ty) {
    mpath_t *p = pp;
    p->m[0] = a; p->m[1] = b; p->m[2] = c; p->m[3] = d; p->m[4] = tx; p->m[5] = ty;
}
int mos_path_is_empty(void *pp) { mpath_t *p = pp; return !p || p->nop == 0; }
static const int op_npts[5] = {2, 2, 4, 6, 0};
int mos_path_element_count(void *pp) { mpath_t *p = pp; return p ? p->nop : 0; }
int mos_path_element(void *pp, int index, double *pts) {
    mpath_t *p = pp;
    if (!p || index < 0 || index >= p->nop) return -1;
    /* leitura sequencial (Path.strokedPath): continua do último índice lido */
    int i = 0, off = 0;
    if (p->rd_i > 0 && p->rd_i <= index) { i = p->rd_i; off = p->rd_off; }
    for (; i < index; i++) off += op_npts[p->op[i]];
    p->rd_i = index; p->rd_off = off;
    for (int k = 0; k < op_npts[p->op[index]]; k++) pts[k] = p->v[off + k];
    return p->op[index];
}
static void p_op(mpath_t *p, int op, const double *v, int n) {
    if (p->nop == p->capop) { p->capop = p->capop ? p->capop * 2 : 16; p->op = realloc(p->op, p->capop); }
    p->op[p->nop++] = (u8)op;
    if (p->nv + n > p->capv) { p->capv = (p->nv + n) * 2 + 32; p->v = realloc(p->v, p->capv * sizeof(double)); }
    for (int i = 0; i < n; i++) p->v[p->nv++] = v[i];
}
void mos_path_move(void *p, double x, double y) { double v[2] = {x, y}; p_op(p, OP_MOVE, v, 2); }
void mos_path_line(void *p, double x, double y) { double v[2] = {x, y}; p_op(p, OP_LINE, v, 2); }
void mos_path_quad(void *p, double cx, double cy, double x, double y) { double v[4] = {cx, cy, x, y}; p_op(p, OP_QUAD, v, 4); }
void mos_path_curve(void *p, double c1x, double c1y, double c2x, double c2y, double x, double y) { double v[6] = {c1x, c1y, c2x, c2y, x, y}; p_op(p, OP_CURVE, v, 6); }
void mos_path_close(void *p) { p_op(p, OP_CLOSE, 0, 0); }
void mos_path_rect(void *p, double x, double y, double w, double h) {
    mos_path_move(p, x, y); mos_path_line(p, x + w, y); mos_path_line(p, x + w, y + h); mos_path_line(p, x, y + h); mos_path_close(p);
}
#define KAPPA 0.5522847498307936
void mos_path_ellipse(void *p, double x, double y, double w, double h) {
    double rx = w / 2, ry = h / 2, cx = x + rx, cy = y + ry, kx = rx * KAPPA, ky = ry * KAPPA;
    mos_path_move(p, cx + rx, cy);
    mos_path_curve(p, cx + rx, cy + ky, cx + kx, cy + ry, cx, cy + ry);
    mos_path_curve(p, cx - kx, cy + ry, cx - rx, cy + ky, cx - rx, cy);
    mos_path_curve(p, cx - rx, cy - ky, cx - kx, cy - ry, cx, cy - ry);
    mos_path_curve(p, cx + kx, cy - ry, cx + rx, cy - ky, cx + rx, cy);
    mos_path_close(p);
}
void mos_path_rrect(void *p, double x, double y, double w, double h, double rx, double ry) {
    if (w < 0) { x += w; w = -w; }
    if (h < 0) { y += h; h = -h; }
    rx = dmin(dmax(rx, 0), w / 2); ry = dmin(dmax(ry, 0), h / 2);
    if (rx <= 0 || ry <= 0) { mos_path_rect(p, x, y, w, h); return; }
    double kx = rx * (1 - KAPPA), ky = ry * (1 - KAPPA);
    mos_path_move(p, x + rx, y);
    mos_path_line(p, x + w - rx, y);
    mos_path_curve(p, x + w - kx, y, x + w, y + ky, x + w, y + ry);
    mos_path_line(p, x + w, y + h - ry);
    mos_path_curve(p, x + w, y + h - ky, x + w - kx, y + h, x + w - rx, y + h);
    mos_path_line(p, x + rx, y + h);
    mos_path_curve(p, x + kx, y + h, x, y + h - ky, x, y + h - ry);
    mos_path_line(p, x, y + ry);
    mos_path_curve(p, x, y + ky, x + kx, y, x + rx, y);
    mos_path_close(p);
}

/* ================================================================ polilinhas */
typedef struct { double x, y; } pt;
typedef struct { pt *p; int n, cap; int closed; } poly_t;
typedef struct { poly_t *r; int n, cap; } polys_t;

static void pl_add(poly_t *l, double x, double y) {
    if (l->n && fabs(l->p[l->n - 1].x - x) < 1e-9 && fabs(l->p[l->n - 1].y - y) < 1e-9) return;
    if (l->n == l->cap) { l->cap = l->cap ? l->cap * 2 : 16; l->p = realloc(l->p, l->cap * sizeof(pt)); }
    l->p[l->n].x = x; l->p[l->n].y = y; l->n++;
}
static poly_t *ps_new(polys_t *s) {
    if (s->n == s->cap) { s->cap = s->cap ? s->cap * 2 : 8; s->r = realloc(s->r, s->cap * sizeof(poly_t)); }
    poly_t *l = &s->r[s->n++];
    memset(l, 0, sizeof *l);
    return l;
}
static void ps_free(polys_t *s) { for (int i = 0; i < s->n; i++) free(s->r[i].p); free(s->r); s->r = 0; s->n = s->cap = 0; }

static double mscale(const double *m) { double d = fabs(m[0] * m[3] - m[1] * m[2]); return d > 0 ? sqrt(d) : 1; }
static void xf(const double *m, double x, double y, double *ox, double *oy) { *ox = m[0] * x + m[2] * y + m[4]; *oy = m[1] * x + m[3] * y + m[5]; }

/* achata o caminho em polilinhas nas coordenadas locais; tol = tolerância em unidades locais */
static int nsub(double len, double tol) { int n = (int)ceil(sqrt(len / (tol > 1e-6 ? tol : 1e-6)) * 0.9); return n < 1 ? 1 : n > 128 ? 128 : n; }
static void flatten(const mpath_t *p, double tol, polys_t *out) {
    poly_t *cur = 0;
    double sx = 0, sy = 0, lx = 0, ly = 0;
    const double *v = p->v;
    for (int i = 0; i < p->nop; i++) {
        int op = p->op[i];
        if (op != OP_MOVE && op != OP_CLOSE && !cur) { cur = ps_new(out); pl_add(cur, lx, ly); sx = lx; sy = ly; }
        switch (op) {
        case OP_MOVE:
            cur = ps_new(out);
            lx = sx = v[0]; ly = sy = v[1];
            pl_add(cur, lx, ly);
            v += 2;
            break;
        case OP_LINE:
            pl_add(cur, v[0], v[1]); lx = v[0]; ly = v[1]; v += 2;
            break;
        case OP_QUAD: {
            double len = hypot_(v[0] - lx, v[1] - ly) + hypot_(v[2] - v[0], v[3] - v[1]);
            int n = nsub(len, tol);
            for (int k = 1; k <= n; k++) {
                double t = (double)k / n, u = 1 - t;
                pl_add(cur, u * u * lx + 2 * u * t * v[0] + t * t * v[2], u * u * ly + 2 * u * t * v[1] + t * t * v[3]);
            }
            lx = v[2]; ly = v[3]; v += 4;
            break;
        }
        case OP_CURVE: {
            double len = hypot_(v[0] - lx, v[1] - ly) + hypot_(v[2] - v[0], v[3] - v[1]) + hypot_(v[4] - v[2], v[5] - v[3]);
            int n = nsub(len, tol) * 3 / 2 + 1;
            for (int k = 1; k <= n; k++) {
                double t = (double)k / n, u = 1 - t;
                double a = u * u * u, b = 3 * u * u * t, c = 3 * u * t * t, d = t * t * t;
                pl_add(cur, a * lx + b * v[0] + c * v[2] + d * v[4], a * ly + b * v[1] + c * v[3] + d * v[5]);
            }
            lx = v[4]; ly = v[5]; v += 6;
            break;
        }
        case OP_CLOSE:
            if (cur) {
                /* o fechamento é implícito: tira a repetição do ponto inicial */
                if (cur->n > 1 && fabs(cur->p[cur->n - 1].x - cur->p[0].x) < 1e-9 && fabs(cur->p[cur->n - 1].y - cur->p[0].y) < 1e-9) cur->n--;
                cur->closed = 1;
            }
            lx = sx; ly = sy;
            cur = 0;
            break;
        }
    }
}

/* ================================================================ estado de pixels / recorte */
typedef struct { int x, y, w, h; u8 *m; } mask_t;      /* máscara de cobertura em pixels da janela */
typedef struct { double x0, y0, x1, y1; mask_t mk; int inverse; } clip_t;
#define MAXCLIP 32
static clip_t clips[MAXCLIP];
static int nclip, nmaskclip;
#define MAXCAP 8
static mask_t caps[MAXCAP];
static int ncap;

static void view_xf(double *m6) {   /* view -> pixels */
    double ox, oy, sy;
    cx_gfx_view_xform(&ox, &oy, &sy);
    m6[0] = 1; m6[1] = 0; m6[2] = 0; m6[3] = sy; m6[4] = ox; m6[5] = oy;
}
static void mcat(const double *a, const double *b, double *o) {   /* o = b(a(x)) */
    double r[6];
    r[0] = b[0] * a[0] + b[2] * a[1]; r[1] = b[1] * a[0] + b[3] * a[1];
    r[2] = b[0] * a[2] + b[2] * a[3]; r[3] = b[1] * a[2] + b[3] * a[3];
    r[4] = b[0] * a[4] + b[2] * a[5] + b[4]; r[5] = b[1] * a[4] + b[3] * a[5] + b[5];
    memcpy(o, r, sizeof r);
}
static void cur_clip(double *x0, double *y0, double *x1, double *y1) {
    double x, y, w, h;
    cx_gfx_view_rect_px(&x, &y, &w, &h);
    *x0 = x; *y0 = y; *x1 = x + w; *y1 = y + h;
    for (int i = 0; i < nclip; i++) {
        *x0 = dmax(*x0, clips[i].x0); *y0 = dmax(*y0, clips[i].y0);
        *x1 = dmin(*x1, clips[i].x1); *y1 = dmin(*y1, clips[i].y1);
    }
    if (*x1 < *x0) *x1 = *x0;
    if (*y1 < *y0) *y1 = *y0;
}
static void apply_clip(void) { double x0, y0, x1, y1; cur_clip(&x0, &y0, &x1, &y1); cx_gfx_clip_px(x0, y0, x1 - x0, y1 - y0); }

/* ================================================================ rasterização própria */
typedef struct { double x0, y0, x1, y1; int dir; } edge_t;
static int cmp_x(const void *a, const void *b) { double d = ((const double *)a)[0] - ((const double *)b)[0]; return d < 0 ? -1 : d > 0; }
static void add_span(float *row, int w, double a, double b, float k) {
    if (a < 0) a = 0;
    if (b > w) b = w;
    if (b <= a) return;
    int ia = (int)a, ib = (int)b;
    if (ia == ib) { row[ia] += (float)((b - a) * k); return; }
    row[ia] += (float)((ia + 1 - a) * k);
    for (int i = ia + 1; i < ib; i++) row[i] += k;
    if (ib < w) row[ib] += (float)((b - ib) * k);
}
#define SS 4
/* cobertura 0..255 dos anéis (pixels) na caixa (bx, by, bw, bh) */
static u8 *raster(const polys_t *s, int eo, int bx, int by, int bw, int bh) {
    int ne = 0;
    for (int i = 0; i < s->n; i++) ne += s->r[i].n;
    edge_t *e = malloc((ne + 1) * sizeof *e);
    int k = 0;
    for (int i = 0; i < s->n; i++) {
        const poly_t *l = &s->r[i];
        if (l->n < 2) continue;
        for (int j = 0; j < l->n; j++) {
            pt a = l->p[j], b = l->p[(j + 1) % l->n];
            if (a.y == b.y) continue;
            if (a.y < b.y) { e[k].x0 = a.x; e[k].y0 = a.y; e[k].x1 = b.x; e[k].y1 = b.y; e[k].dir = 1; }
            else { e[k].x0 = b.x; e[k].y0 = b.y; e[k].x1 = a.x; e[k].y1 = a.y; e[k].dir = -1; }
            k++;
        }
    }
    float *row = malloc((bw + 2) * sizeof(float));
    u8 *out = calloc((usize)bw * bh, 1);
    double *xs = malloc((k + 1) * 2 * sizeof(double));
    for (int y = 0; y < bh; y++) {
        memset(row, 0, (bw + 2) * sizeof(float));
        int any = 0;
        for (int sub = 0; sub < SS; sub++) {
            double sy = by + y + (sub + 0.5) / SS;
            int nx = 0;
            for (int i = 0; i < k; i++) {
                if (sy < e[i].y0 || sy >= e[i].y1) continue;
                xs[nx * 2] = e[i].x0 + (sy - e[i].y0) * (e[i].x1 - e[i].x0) / (e[i].y1 - e[i].y0) - bx;
                xs[nx * 2 + 1] = e[i].dir;
                nx++;
            }
            if (nx < 2) continue;
            qsort(xs, nx, 2 * sizeof(double), cmp_x);
            int w = 0;
            for (int i = 0; i < nx - 1; i++) {
                w += (int)xs[i * 2 + 1];
                int in = eo ? (w & 1) : (w != 0);
                if (in) { add_span(row, bw, xs[i * 2], xs[(i + 1) * 2], 1.0f / SS); any = 1; }
            }
        }
        if (!any) continue;
        for (int x = 0; x < bw; x++) { float v = row[x]; out[(usize)y * bw + x] = v >= 1 ? 255 : v <= 0 ? 0 : (u8)(v * 255 + 0.5f); }
    }
    free(xs); free(row); free(e);
    return out;
}
static int mask_at(const mask_t *m, int x, int y) {
    if (x < m->x || y < m->y || x >= m->x + m->w || y >= m->y + m->h) return 0;
    return m->m[(usize)(y - m->y) * m->w + (x - m->x)];
}
/* aplica as máscaras de recorte à cobertura (caixa bx..) */
static void apply_clip_masks(u8 *cov, int bx, int by, int bw, int bh) {
    for (int c = 0; c < nclip; c++) {
        if (!clips[c].mk.m) continue;
        for (int y = 0; y < bh; y++)
            for (int x = 0; x < bw; x++) {
                u8 *v = &cov[(usize)y * bw + x];
                if (!*v) continue;
                int mv = mask_at(&clips[c].mk, bx + x, by + y);
                if (clips[c].inverse) mv = 255 - mv;
                *v = (u8)((*v * mv + 127) / 255);
            }
    }
}

static u32 argb(double r, double g, double b, double a) {
    if (a < 0) a = 0; if (a > 1) a = 1;
    if (r < 0) r = 0; if (r > 1) r = 1;
    if (g < 0) g = 0; if (g > 1) g = 1;
    if (b < 0) b = 0; if (b > 1) b = 1;
    return (u32)(a * 255 + 0.5) << 24 | (u32)(r * 255 + 0.5) << 16 | (u32)(g * 255 + 0.5) << 8 | (u32)(b * 255 + 0.5);
}
static void bbox(const polys_t *s, double *x0, double *y0, double *x1, double *y1) {
    *x0 = *y0 = 1e30; *x1 = *y1 = -1e30;
    for (int i = 0; i < s->n; i++)
        for (int j = 0; j < s->r[i].n; j++) {
            pt q = s->r[i].p[j];
            *x0 = dmin(*x0, q.x); *y0 = dmin(*y0, q.y); *x1 = dmax(*x1, q.x); *y1 = dmax(*y1, q.y);
        }
}
/* caixa inteira = interseção da caixa dos anéis (com margem) e do recorte atual */
static int box_for(double x0, double y0, double x1, double y1, double margin, int *bx, int *by, int *bw, int *bh) {
    double cx0, cy0, cx1, cy1;
    if (ncap) { cx0 = caps[ncap - 1].x; cy0 = caps[ncap - 1].y; cx1 = cx0 + caps[ncap - 1].w; cy1 = cy0 + caps[ncap - 1].h; }
    else cur_clip(&cx0, &cy0, &cx1, &cy1);
    x0 = dmax(floor(x0 - margin), floor(cx0)); y0 = dmax(floor(y0 - margin), floor(cy0));
    x1 = dmin(ceil(x1 + margin), ceil(cx1)); y1 = dmin(ceil(y1 + margin), ceil(cy1));
    if (x1 <= x0 || y1 <= y0) return 0;
    *bx = (int)x0; *by = (int)y0; *bw = (int)(x1 - x0); *bh = (int)(y1 - y0);
    if (*bw > 4096) *bw = 4096;
    if (*bh > 4096) *bh = 4096;
    return 1;
}
/* compõe a cobertura (já com recortes) na captura de máscara do topo: m = m + c*(1-m) */
static void cap_accumulate(const u8 *cov, int bx, int by, int bw, int bh, double alpha) {
    mask_t *m = &caps[ncap - 1];
    int al = (int)(alpha * 255 + 0.5);
    for (int y = 0; y < bh; y++)
        for (int x = 0; x < bw; x++) {
            int c = cov[(usize)y * bw + x] * al / 255;
            if (!c) continue;
            int mx = bx + x - m->x, my = by + y - m->y;
            if (mx < 0 || my < 0 || mx >= m->w || my >= m->h) continue;
            u8 *d = &m->m[(usize)my * m->w + mx];
            *d = (u8)(*d + c * (255 - *d) / 255);
        }
}
static void emit_cov(const u8 *cov, int bx, int by, int bw, int bh, u32 color) {
    u32 *img = malloc((usize)bw * bh * 4);
    u32 a = color >> 24, rgb = color & 0xFFFFFF;
    int any = 0;
    u64 sum = 0;
    for (usize i = 0; i < (usize)bw * bh; i++) {
        u32 c = cov[i] * a / 255;
        img[i] = c ? (c << 24 | rgb) : 0;
        any |= c != 0;
        sum += cov[i];
    }
    if (any) cx_gfx_image_px(bx, by, bw, bh, img);
    TR("[path]   área coberta %ld px (de %dx%d)\n", (long)((sum + 127) / 255), bw, bh);
    free(img);
}
/* depuração: MOS_PATH_RASTER força a rasterização própria (para o trace medir a área) */
static int force_raster(void) { if (raster_flag < 0) raster_flag = getenv("MOS_PATH_RASTER") ? 1 : 0; return raster_flag; }
/* preenche anéis em pixels */
static void fill_px(polys_t *s, int eo, u32 color) {
    if (!s->n || !(color >> 24)) return;
    double x0, y0, x1, y1;
    bbox(s, &x0, &y0, &x1, &y1);
    int bx, by, bw, bh;
    if (!box_for(x0, y0, x1, y1, 1, &bx, &by, &bw, &bh)) { TR("[path] fill fora do recorte\n"); return; }
    int npts = 0;
    for (int i = 0; i < s->n; i++) if (s->r[i].n >= 2) npts += s->r[i].n + 2;
    if (!ncap && !nmaskclip && !eo && npts <= 4096 && (x1 - x0) * (y1 - y0) < 4e6 && !force_raster()) {
        /* GC_POLY único com pontes ao primeiro ponto */
        float *xy = malloc((npts + 4) * 2 * sizeof(float));
        int n = 0, first = -1;
        for (int i = 0; i < s->n; i++) {
            const poly_t *l = &s->r[i];
            if (l->n < 2) continue;
            if (first < 0) first = i;
            for (int j = 0; j <= l->n; j++) { xy[n * 2] = (float)l->p[j % l->n].x; xy[n * 2 + 1] = (float)l->p[j % l->n].y; n++; }
            if (i != first) { xy[n * 2] = (float)s->r[first].p[0].x; xy[n * 2 + 1] = (float)s->r[first].p[0].y; n++; }
        }
        if (n >= 3) cx_gfx_poly_px(xy, (u32)n, color);
        TR("[path] fill poly %d anéis %d pts caixa %d,%d %dx%d cor %x\n", s->n, n, bx, by, bw, bh, color);
        free(xy);
        return;
    }
    u8 *cov = raster(s, eo, bx, by, bw, bh);
    apply_clip_masks(cov, bx, by, bw, bh);
    if (ncap) { cap_accumulate(cov, bx, by, bw, bh, (color >> 24) / 255.0); TR("[path] fill -> máscara %d,%d %dx%d\n", bx, by, bw, bh); }
    else { emit_cov(cov, bx, by, bw, bh, color); TR("[path] fill imagem %s %d anéis caixa %d,%d %dx%d cor %x\n", eo ? "par-ímpar" : "não-zero", s->n, bx, by, bw, bh, color); }
    free(cov);
}
static void to_px(polys_t *s, const double *m) {
    for (int i = 0; i < s->n; i++)
        for (int j = 0; j < s->r[i].n; j++) xf(m, s->r[i].p[j].x, s->r[i].p[j].y, &s->r[i].p[j].x, &s->r[i].p[j].y);
}
static void path_px_xf(const mpath_t *p, double *m) { double v[6]; view_xf(v); mcat(p->m, v, m); }

void mos_path_fill(void *pp, int eo, double r, double g, double b, double a) {
    mpath_t *p = pp;
    if (!p || !p->nop) return;
    double m[6];
    path_px_xf(p, m);
    polys_t s = {0};
    flatten(p, 0.25 / mscale(m), &s);
    to_px(&s, m);
    fill_px(&s, eo, argb(r, g, b, a));
    ps_free(&s);
}

/* ================================================================ traço */
static double area(const poly_t *l) { double a = 0; for (int i = 0; i < l->n; i++) { pt p = l->p[i], q = l->p[(i + 1) % l->n]; a += p.x * q.y - q.x * p.y; } return a / 2; }
static void orient(poly_t *l) {   /* todos os anéis do contorno com área positiva */
    if (area(l) >= 0) return;
    for (int i = 0, j = l->n - 1; i < j; i++, j--) { pt t = l->p[i]; l->p[i] = l->p[j]; l->p[j] = t; }
}
static int arc_steps(double r, double ang, double tol) {
    double c = 1 - tol / (r > tol ? r : tol);
    double step = c > -1 && c < 1 ? 2 * acos_(c) : 0.5;
    int n = (int)ceil(fabs(ang) / (step > 0.05 ? step : 0.05));
    return n < 2 ? 2 : n > 128 ? 128 : n;
}
static void add_circle(polys_t *o, pt c, double r, double tol) {
    poly_t *l = ps_new(o);
    int n = arc_steps(r, 2 * PI, tol);
    for (int i = 0; i < n; i++) { double a = 2 * PI * i / n; pl_add(l, c.x + cos(a) * r, c.y + sin(a) * r); }
    l->closed = 1;
    orient(l);
}
static void add_quad(polys_t *o, pt a, pt b, pt c, pt d) {
    poly_t *l = ps_new(o);
    pl_add(l, a.x, a.y); pl_add(l, b.x, b.y); pl_add(l, c.x, c.y); pl_add(l, d.x, d.y);
    l->closed = 1;
    if (l->n < 3) { o->n--; free(l->p); return; }
    orient(l);
}
/* junção no vértice p entre as direções unitárias d0 (chegada) e d1 (saída) */
static void add_join(polys_t *o, pt p, pt d0, pt d1, double hw, int join, double miter, double tol) {
    double cr = d0.x * d1.y - d0.y * d1.x, dt = d0.x * d1.x + d0.y * d1.y;
    if (fabs(cr) < 1e-9 && dt > 0) return;   /* colinear */
    double side = cr > 0 ? -1 : 1;            /* lado de fora da curva */
    pt n0 = {-d0.y * side * hw, d0.x * side * hw}, n1 = {-d1.y * side * hw, d1.x * side * hw};
    pt a = {p.x + n0.x, p.y + n0.y}, b = {p.x + n1.x, p.y + n1.y};
    if (join == 1 || (fabs(cr) < 1e-9 && dt < 0)) {
        /* redonda (e meia-volta): leque de p de n0 até n1 pelo lado de fora */
        double a0 = atan2(n0.y, n0.x), a1 = atan2(n1.y, n1.x), da = a1 - a0;
        while (da > PI) da -= 2 * PI;
        while (da < -PI) da += 2 * PI;
        if (fabs(cr) < 1e-9) da = PI;
        poly_t *l = ps_new(o);
        pl_add(l, p.x, p.y);
        int n = arc_steps(hw, da, tol);
        for (int i = 0; i <= n; i++) { double t = a0 + da * i / n; pl_add(l, p.x + cos(t) * hw, p.y + sin(t) * hw); }
        l->closed = 1;
        orient(l);
        return;
    }
    if (join == 0) {
        /* ângulo interno θ entre -d0 e d1: razão do miter = 1/sin(θ/2) */
        double cosT = -dt, s2 = sqrt(dmax((1 - cosT) / 2, 0));
        double ratio = s2 > 1e-9 ? 1 / s2 : 1e9;
        if (ratio <= miter) {
            double bx = n0.x + n1.x, by = n0.y + n1.y, bl = hypot_(bx, by);
            if (bl > 1e-9) {
                pt tip = {p.x + bx / bl * hw * ratio, p.y + by / bl * hw * ratio};
                add_quad(o, p, a, tip, b);
                return;
            }
        }
    }
    /* chanfro */
    poly_t *l = ps_new(o);
    pl_add(l, p.x, p.y); pl_add(l, a.x, a.y); pl_add(l, b.x, b.y);
    l->closed = 1;
    if (l->n < 3) { o->n--; free(l->p); return; }
    orient(l);
}
static void stroke_line(polys_t *o, const poly_t *l, double hw, int cap, int join, double miter, double tol) {
    int n = l->n, closed = l->closed && n > 2;
    if (n == 1) {
        if (cap == 1) add_circle(o, l->p[0], hw, tol);
        else if (cap == 2) { pt c = l->p[0]; pt a = {c.x - hw, c.y - hw}, b = {c.x + hw, c.y - hw}, d = {c.x + hw, c.y + hw}, e = {c.x - hw, c.y + hw}; add_quad(o, a, b, d, e); }
        return;
    }
    int segs = closed ? n : n - 1;
    for (int i = 0; i < segs; i++) {
        pt a = l->p[i], b = l->p[(i + 1) % n];
        double dx = b.x - a.x, dy = b.y - a.y, len = hypot_(dx, dy);
        if (len < 1e-12) continue;
        dx /= len; dy /= len;
        if (!closed && cap == 2) {
            if (i == 0) { a.x -= dx * hw; a.y -= dy * hw; }
            if (i == segs - 1) { b.x += dx * hw; b.y += dy * hw; }
        }
        pt nx = {-dy * hw, dx * hw};
        pt q0 = {a.x + nx.x, a.y + nx.y}, q1 = {b.x + nx.x, b.y + nx.y}, q2 = {b.x - nx.x, b.y - nx.y}, q3 = {a.x - nx.x, a.y - nx.y};
        add_quad(o, q0, q1, q2, q3);
    }
    int j0 = closed ? 0 : 1, j1 = closed ? n : n - 1;
    for (int j = j0; j < j1; j++) {
        pt p = l->p[j], pa = l->p[(j - 1 + n) % n], pb = l->p[(j + 1) % n];
        double l0 = hypot_(p.x - pa.x, p.y - pa.y), l1 = hypot_(pb.x - p.x, pb.y - p.y);
        if (l0 < 1e-12 || l1 < 1e-12) continue;
        pt d0 = {(p.x - pa.x) / l0, (p.y - pa.y) / l0}, d1 = {(pb.x - p.x) / l1, (pb.y - p.y) / l1};
        add_join(o, p, d0, d1, hw, join, miter, tol);
    }
    if (!closed && cap == 1) { add_circle(o, l->p[0], hw, tol); add_circle(o, l->p[n - 1], hw, tol); }
}
/* tracejado: corta cada polilinha nos trechos "ligados" */
static void dash_lines(const polys_t *in, polys_t *out, const double *dash, int nd, double phase) {
    double total = 0;
    for (int i = 0; i < nd; i++) total += dash[i] > 0 ? dash[i] : 0;
    for (int r = 0; r < in->n; r++) {
        const poly_t *l = &in->r[r];
        int n = l->n, segs = l->closed && n > 2 ? n : n - 1;
        /* estado do tracejado no início, pela fase */
        int di = 0;
        double left = dash[0] > 0 ? dash[0] : 0, ph = phase;
        if (total > 0) { ph = ph - floor(ph / total) * total; }
        while (ph > 0 && total > 0) {
            if (ph >= left) { ph -= left; di = (di + 1) % nd; left = dash[di] > 0 ? dash[di] : 0; }
            else { left -= ph; ph = 0; }
        }
        poly_t *cur = 0;
        if (!(di & 1) && segs > 0) { cur = ps_new(out); pl_add(cur, l->p[0].x, l->p[0].y); }
        for (int i = 0; i < segs; i++) {
            pt a = l->p[i], b = l->p[(i + 1) % n];
            double len = hypot_(b.x - a.x, b.y - a.y), t = 0;
            if (len < 1e-12) continue;
            while (len - t > 1e-12) {
                double step = dmin(left, len - t);
                t += step; left -= step;
                double x = a.x + (b.x - a.x) * t / len, y = a.y + (b.y - a.y) * t / len;
                if (cur) pl_add(cur, x, y);
                if (left <= 1e-12) {
                    int guard = 0;
                    do { di = (di + 1) % nd; left = dash[di] > 0 ? dash[di] : 0; } while (left <= 0 && ++guard < nd);
                    if (di & 1) cur = 0;
                    else { cur = ps_new(out); pl_add(cur, x, y); }
                }
            }
        }
    }
}
/* contorno do traço em coordenadas locais (tol local) */
static void stroke_outline(const mpath_t *p, double tol, double width, int cap, int join, double miter,
                           const double *dash, int ndash, double phase, polys_t *o) {
    polys_t lines = {0};
    flatten(p, tol, &lines);
    double hw = fabs(width) / 2;
    if (miter < 1) miter = 10;
    double total = 0;
    for (int i = 0; i < ndash; i++) total += dash[i] > 0 ? dash[i] : 0;
    if (dash && ndash > 0 && total > 1e-9) {
        polys_t d = {0};
        dash_lines(&lines, &d, dash, ndash, phase);
        ps_free(&lines);
        lines = d;
    }
    for (int i = 0; i < lines.n; i++) stroke_line(o, &lines.r[i], hw, cap, join, miter, tol);
    ps_free(&lines);
}
void mos_path_stroke(void *pp, double width, int cap, int join, double miter, const double *dash, int ndash, double phase,
                     double r, double g, double b, double a) {
    mpath_t *p = pp;
    if (!p || !p->nop || width <= 0) return;
    double m[6];
    path_px_xf(p, m);
    double sc = mscale(m);
    /* traços finos demais somem com AA: largura mínima de ~1 px, compensada no alfa */
    if (width * sc < 1) { a *= width * sc; width = 1 / sc; }
    polys_t o = {0};
    stroke_outline(p, 0.25 / sc, width, cap, join, miter, dash, ndash, phase, &o);
    to_px(&o, m);
    TR("[path] stroke largura %ld cap %d join %d dash %d -> %d anéis\n", C(width * sc), cap, join, ndash, o.n);
    fill_px(&o, 0, argb(r, g, b, a));
    ps_free(&o);
}
int mos_path_stroke_outline(void *src, void *dst, double width, int cap, int join, double miter, const double *dash, int ndash, double phase) {
    mpath_t *p = src;
    if (!p || !dst) return 0;
    polys_t o = {0};
    stroke_outline(p, 0.1, width, cap, join, miter, dash, ndash, phase, &o);
    int n = 0;
    for (int i = 0; i < o.n; i++) {
        if (o.r[i].n < 3) continue;
        mos_path_move(dst, o.r[i].p[0].x, o.r[i].p[0].y);
        for (int j = 1; j < o.r[i].n; j++) mos_path_line(dst, o.r[i].p[j].x, o.r[i].p[j].y);
        mos_path_close(dst);
        n++;
    }
    ps_free(&o);
    return n;
}
int mos_path_contains(void *pp, double x, double y, int eo) {
    mpath_t *p = pp;
    if (!p || !p->nop) return 0;
    polys_t s = {0};
    flatten(p, 0.05, &s);
    int w = 0;
    for (int i = 0; i < s.n; i++) {
        const poly_t *l = &s.r[i];
        for (int j = 0; j < l->n && l->n > 1; j++) {
            pt a = l->p[j], b = l->p[(j + 1) % l->n];
            if ((a.y <= y) != (b.y <= y)) {
                double xc = a.x + (y - a.y) * (b.x - a.x) / (b.y - a.y);
                if (xc > x) w += a.y < b.y ? 1 : -1;
            }
        }
    }
    ps_free(&s);
    return eo ? (w & 1) : w != 0;
}

/* ================================================================ recorte */
void mos_clip_reset(void) {
    for (int i = 0; i < nclip; i++) free(clips[i].mk.m);
    for (int i = 0; i < ncap; i++) free(caps[i].m);
    nclip = nmaskclip = ncap = 0;
    apply_clip();
}
int mos_clip_depth(void) { return nclip; }
static clip_t *clip_push(void) {
    if (nclip >= MAXCLIP) return 0;
    clip_t *c = &clips[nclip];
    memset(c, 0, sizeof *c);
    cur_clip(&c->x0, &c->y0, &c->x1, &c->y1);
    nclip++;
    return c;
}
void mos_clip_push_rect(double x, double y, double w, double h) {
    double v[6];
    view_xf(v);
    double ax, ay, bx, by;
    xf(v, x, y, &ax, &ay);
    xf(v, x + w, y + h, &bx, &by);
    clip_t *c = clip_push();
    if (!c) { TR("[path] pilha de recorte cheia\n"); return; }
    c->x0 = dmax(c->x0, dmin(ax, bx)); c->y0 = dmax(c->y0, dmin(ay, by));
    c->x1 = dmin(c->x1, dmax(ax, bx)); c->y1 = dmin(c->y1, dmax(ay, by));
    apply_clip();
    TR("[path] clip rect %ld,%ld,%ld,%ld (prof. %d)\n", C(c->x0), C(c->y0), C(c->x1 - c->x0), C(c->y1 - c->y0), nclip);
}
static void push_mask(mask_t mk, int inverse) {
    clip_t *c = clip_push();
    if (!c) { free(mk.m); return; }
    c->mk = mk;
    c->inverse = inverse;
    if (!inverse) {
        c->x0 = dmax(c->x0, mk.x); c->y0 = dmax(c->y0, mk.y);
        c->x1 = dmin(c->x1, mk.x + mk.w); c->y1 = dmin(c->y1, mk.y + mk.h);
    }
    nmaskclip++;
    apply_clip();
}
void mos_clip_push_path(void *pp, int eo, int inverse) {
    mpath_t *p = pp;
    double m[6];
    path_px_xf(p, m);
    polys_t s = {0};
    if (p && p->nop) { flatten(p, 0.25 / mscale(m), &s); to_px(&s, m); }
    double x0, y0, x1, y1;
    mask_t mk = {0, 0, 0, 0, 0};
    if (s.n) {
        bbox(&s, &x0, &y0, &x1, &y1);
        int bx, by, bw, bh;
        int saved = ncap; ncap = 0;   /* a caixa do recorte não depende da captura */
        int ok = box_for(x0, y0, x1, y1, 1, &bx, &by, &bw, &bh);
        ncap = saved;
        if (ok) {
            /* retângulo alinhado aos pixels: basta o recorte retangular */
            mk.x = bx; mk.y = by; mk.w = bw; mk.h = bh;
            mk.m = raster(&s, eo, bx, by, bw, bh);
        }
    }
    if (!mk.m) { mk.m = calloc(1, 1); mk.w = mk.h = 0; }
    TR("[path] clip path %s%s caixa %d,%d %dx%d (prof. %d)\n", eo ? "par-ímpar" : "não-zero", inverse ? " invertido" : "", mk.x, mk.y, mk.w, mk.h, nclip + 1);
    ps_free(&s);
    push_mask(mk, inverse);
}
void mos_clip_pop(void) {
    if (!nclip) return;
    nclip--;
    if (clips[nclip].mk.m) { free(clips[nclip].mk.m); nmaskclip--; }
    clips[nclip].mk.m = 0;
    apply_clip();
    TR("[path] clip pop (prof. %d)\n", nclip);
}
void mos_clip_bounds(double *x, double *y, double *w, double *h) {
    double x0, y0, x1, y1, v[6];
    cur_clip(&x0, &y0, &x1, &y1);
    view_xf(v);   /* inversa de view -> pixels (translação e espelho em y) */
    double ax = x0 - v[4], ay = (y0 - v[5]) / v[3], bx = x1 - v[4], by = (y1 - v[5]) / v[3];
    *x = dmin(ax, bx); *y = dmin(ay, by); *w = fabs(bx - ax); *h = fabs(by - ay);
}

/* ================================================================ máscaras e sombra */
int mos_mask_capturing(void) { return ncap; }
void mos_mask_begin(void) {
    if (ncap >= MAXCAP) { ncap++; return; }   /* profundidade demais: só conta */
    double x0, y0, x1, y1;
    cur_clip(&x0, &y0, &x1, &y1);
    mask_t *m = &caps[ncap];
    m->x = (int)floor(x0); m->y = (int)floor(y0);
    m->w = (int)(ceil(x1) - m->x); m->h = (int)(ceil(y1) - m->y);
    if (m->w > 4096) m->w = 4096;
    if (m->h > 4096) m->h = 4096;
    if (m->w < 0) m->w = 0;
    if (m->h < 0) m->h = 0;
    m->m = calloc((usize)m->w * m->h + 1, 1);
    ncap++;
    TR("[path] máscara begin %d,%d %dx%d\n", m->x, m->y, m->w, m->h);
}
static int cap_pop(mask_t *out) {
    if (!ncap) return 0;
    ncap--;
    if (ncap >= MAXCAP) return 0;
    *out = caps[ncap];
    caps[ncap].m = 0;
    return 1;
}
/* reduz a máscara à caixa dos pixels não nulos (a captura começa do tamanho do recorte inteiro) */
static void mask_crop(mask_t *m) {
    int x0 = m->w, y0 = m->h, x1 = -1, y1 = -1;
    for (int y = 0; y < m->h; y++)
        for (int x = 0; x < m->w; x++)
            if (m->m[(usize)y * m->w + x]) { if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; if (y > y1) y1 = y; }
    if (x1 < 0) { m->w = m->h = 0; return; }
    int w = x1 - x0 + 1, h = y1 - y0 + 1;
    if (w == m->w && h == m->h) return;
    u8 *n = malloc((usize)w * h + 1);
    for (int y = 0; y < h; y++) memcpy(&n[(usize)y * w], &m->m[(usize)(y + y0) * m->w + x0], w);
    free(m->m);
    m->m = n; m->x += x0; m->y += y0; m->w = w; m->h = h;
}
void mos_mask_end_clip(int inverse) {
    mask_t m;
    if (!cap_pop(&m)) { clip_push(); return; }   /* mantém o pareamento com mos_clip_pop */
    mask_crop(&m);
    TR("[path] máscara -> clip%s\n", inverse ? " invertido" : "");
    push_mask(m, inverse);
}
void mos_mask_end_discard(void) { mask_t m; if (cap_pop(&m)) free(m.m); }
/* desfoque em caixa separável, 3 passadas (≈ gaussiano) */
static void box_blur(u8 *m, int w, int h, int r) {
    if (r < 1 || w <= 0 || h <= 0) return;
    int mx = w > h ? w : h;
    u32 *acc = malloc((mx + 1) * sizeof(u32));
    u8 *tmp = malloc(mx);
    for (int pass = 0; pass < 3; pass++) {
        for (int dir = 0; dir < 2; dir++) {
            int lines = dir ? w : h, len = dir ? h : w;
            for (int li = 0; li < lines; li++) {
                acc[0] = 0;
                for (int i = 0; i < len; i++) acc[i + 1] = acc[i] + m[dir ? (usize)i * w + li : (usize)li * w + i];
                for (int i = 0; i < len; i++) {
                    int a = i - r < 0 ? 0 : i - r, b = i + r + 1 > len ? len : i + r + 1;
                    tmp[i] = (u8)((acc[b] - acc[a]) / (u32)(2 * r + 1));
                }
                for (int i = 0; i < len; i++) m[dir ? (usize)i * w + li : (usize)li * w + i] = tmp[i];
            }
        }
    }
    free(acc); free(tmp);
}
/* desenha a máscara m (pixels) desfocada e deslocada, com recorte */
static void draw_shadow(const mask_t *src, double radius, double dx, double dy, u32 color) {
    double sigma = radius / 2;
    int r = (int)(sqrt(12 * sigma * sigma / 3 + 1) / 2 + 0.5), margin = r * 3 + 1;
    if (radius <= 0) { r = 0; margin = 0; }
    int odx = (int)floor(dx + 0.5), ody = (int)floor(dy + 0.5);
    mask_t m = {src->x - margin + odx, src->y - margin + ody, src->w + 2 * margin, src->h + 2 * margin, 0};
    if (m.w <= 0 || m.h <= 0 || m.w > 4096 || m.h > 4096) return;
    m.m = calloc((usize)m.w * m.h, 1);
    for (int y = 0; y < src->h; y++) memcpy(&m.m[(usize)(y + margin) * m.w + margin], &src->m[(usize)y * src->w], src->w);
    box_blur(m.m, m.w, m.h, r);
    /* recorta à caixa do recorte atual */
    int bx, by, bw, bh;
    if (box_for(m.x, m.y, m.x + m.w, m.y + m.h, 0, &bx, &by, &bw, &bh)) {
        u8 *cov = malloc((usize)bw * bh);
        for (int y = 0; y < bh; y++) for (int x = 0; x < bw; x++) cov[(usize)y * bw + x] = (u8)mask_at(&m, bx + x, by + y);
        apply_clip_masks(cov, bx, by, bw, bh);
        if (ncap) cap_accumulate(cov, bx, by, bw, bh, (color >> 24) / 255.0);
        else emit_cov(cov, bx, by, bw, bh, color);
        TR("[path] sombra raio %ld desloc %d,%d caixa %d,%d %dx%d\n", C(radius), odx, ody, bx, by, bw, bh);
        free(cov);
    }
    free(m.m);
}
void mos_mask_end_shadow(double radius, double dx, double dy, double r, double g, double b, double a) {
    mask_t m;
    if (!cap_pop(&m)) return;
    mask_crop(&m);
    draw_shadow(&m, radius, dx, dy, argb(r, g, b, a));
    free(m.m);
}
void mos_path_fill_shadow(void *pp, int eo, double radius, double dx, double dy, double r, double g, double b, double a) {
    mpath_t *p = pp;
    if (!p || !p->nop || a <= 0) return;
    double m[6];
    path_px_xf(p, m);
    polys_t s = {0};
    flatten(p, 0.25 / mscale(m), &s);
    to_px(&s, m);
    double x0, y0, x1, y1;
    bbox(&s, &x0, &y0, &x1, &y1);
    mask_t mk = {(int)floor(x0) - 1, (int)floor(y0) - 1, 0, 0, 0};
    mk.w = (int)ceil(x1) + 1 - mk.x; mk.h = (int)ceil(y1) + 1 - mk.y;
    if (mk.w > 0 && mk.h > 0 && mk.w <= 4096 && mk.h <= 4096) {
        mk.m = raster(&s, eo, mk.x, mk.y, mk.w, mk.h);
        draw_shadow(&mk, radius, dx, dy, argb(r, g, b, a));
        free(mk.m);
    }
    ps_free(&s);
}

/* ================================================================ retângulo */
void mos_fill_rect(double x, double y, double w, double h, double radius, double r, double g, double b, double a) {
    if (w <= 0 || h <= 0 || a <= 0) return;
    if (!ncap && !nmaskclip && !force_raster()) {
        /* caminho rápido: GC_RRECT do kernel (já respeita o recorte retangular) */
        double v[6], ax, ay, bx2, by2;
        view_xf(v);
        xf(v, x, y, &ax, &ay);
        xf(v, x + w, y + h, &bx2, &by2);
        cx_gfx_rrect_px(dmin(ax, bx2), dmin(ay, by2), fabs(bx2 - ax), fabs(by2 - ay), radius, argb(r, g, b, a));
        return;
    }
    mpath_t *p = mos_path_create();
    if (radius > 0) mos_path_rrect(p, x, y, w, h, radius, radius);
    else mos_path_rect(p, x, y, w, h);
    mos_path_fill(p, 0, r, g, b, a);
    mos_path_destroy(p);
}
