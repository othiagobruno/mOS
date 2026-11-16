/* mOS — rasterizador vetorial do motor web (kernel/web/raster.c): tipos e funções comuns ao SVG e ao Canvas 2D */
#pragma once
#include "kernel.h"

typedef struct {
    float *xy;          /* pontos em espaço do usuário */
    int n, cap;
    int *start;         /* início de cada contorno */
    u8 *closed;
    int nc, ccap;
    float tol;          /* tolerância de achatamento (espaço do usuário) */
    float cx, cy, sx, sy; /* ponto atual e início do subcaminho */
} path_t;
typedef struct { float x0, y0, x1, y1, dxdy; int dir; } edge_t;
typedef struct { edge_t *e; int n, cap; float minx, miny, maxx, maxy; } edges_t;
typedef struct { int x, y, w, h; u8 *a; } mask_t;   /* cobertura 0..255 numa caixa do canvas */
enum { CAP_BUTT, CAP_ROUND, CAP_SQUARE };
enum { JOIN_MITER, JOIN_ROUND, JOIN_BEVEL };
static inline float sqrtf_(float x) { return x > 0 ? fsqrt(x) : 0; }
static inline float fabs_(float x) { return x < 0 ? -x : x; }

static inline float atan2_(float y, float x) {
    if (x == 0 && y == 0) return 0;
    float ax = fabs_(x), ay = fabs_(y);
    float a = ax > ay ? ay / ax : ax / ay;
    float s = a * a;
    float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
    if (ay > ax) r = 1.57079637f - r;
    if (x < 0) r = 3.14159274f - r;
    if (y < 0) r = -r;
    return r;
}
typedef struct { float a, b, c, d, e, f; } mat_t;
static const mat_t MID = { 1, 0, 0, 1, 0, 0 };
static inline mat_t mmul(mat_t m, mat_t n) {   /* m · n (n aplicada primeiro) */
    mat_t r = { m.a * n.a + m.c * n.b, m.b * n.a + m.d * n.b, m.a * n.c + m.c * n.d, m.b * n.c + m.d * n.d,
                m.a * n.e + m.c * n.f + m.e, m.b * n.e + m.d * n.f + m.f };
    return r;
}
static inline void mapp(const mat_t *m, float x, float y, float *ox, float *oy) { *ox = m->a * x + m->c * y + m->e; *oy = m->b * x + m->d * y + m->f; }
static inline bool minv(mat_t m, mat_t *o) {
    float det = m.a * m.d - m.b * m.c;
    if (fabs_(det) < 1e-12f) return false;
    float id = 1.0f / det;
    o->a = m.d * id; o->b = -m.b * id; o->c = -m.c * id; o->d = m.a * id;
    o->e = -(o->a * m.e + o->c * m.f); o->f = -(o->b * m.e + o->d * m.f);
    return true;
}
static inline float mscale(const mat_t *m) { return sqrtf_(fabs_(m->a * m->d - m->b * m->c)); }
static inline u32 premul(u32 c, float op) {
    u32 a = (u32)((c >> 24) * op + 0.5f);
    if (a > 255) a = 255;
    u32 r = (((c >> 16) & 255) * a + 127) / 255, g = (((c >> 8) & 255) * a + 127) / 255, b = ((c & 255) * a + 127) / 255;
    return a << 24 | r << 16 | g << 8 | b;
}
static inline u32 mul_px(u32 c, u32 m) {   /* c pré-multiplicado × m/255 */
    if (m == 255) return c;
    u32 rb = ((c & 0x00FF00FFu) * m + 0x00800080u) >> 8 & 0x00FF00FFu;
    u32 ag = (((c >> 8) & 0x00FF00FFu) * m + 0x00800080u) & 0xFF00FF00u;
    return rb | ag;
}
static inline u32 over(u32 d, u32 s) {
    u32 ia = 255 - (s >> 24);
    if (ia == 0) return s;
    return s + mul_px(d, ia);
}

typedef struct { u8 cap, join; float miter, dashoff; float dash[16]; int ndash; } rs_stroke_t;

bool   rs_num_start(const char *p);
bool   rs_parse_f(const char **pp, float *out);                 /* número no estilo SVG */
void   rs_path_free(path_t *p);
void   rs_path_pt(path_t *p, float x, float y);
void   rs_path_move(path_t *p, float x, float y);
void   rs_path_line(path_t *p, float x, float y);
void   rs_path_close(path_t *p);
int    rs_cont_len(const path_t *p, int c);
void   rs_path_cubic(path_t *p, float x1, float y1, float x2, float y2, float x3, float y3);
void   rs_path_quad(path_t *p, float x1, float y1, float x2, float y2);
void   rs_path_arc(path_t *p, float rx, float ry, float phi_deg, int large, int sweep, float x, float y);   /* arco do SVG */
void   rs_parse_path_d(path_t *P, const char *d);              /* atributo d do SVG */
void   rs_edges_add(edges_t *E, float x0, float y0, float x1, float y1);
void   rs_edges_from_path(edges_t *E, const path_t *p, const mat_t *m);
mask_t rs_rasterize(edges_t *E, bool evenodd, int cx0, int cy0, int cx1, int cy1);
void   rs_poly_add(path_t *o, const float *xy, int n);
void   rs_circle_poly(path_t *o, float cx, float cy, float r, float scale);
void   rs_stroke_poly(path_t *o, const float *pt, int n, bool closed, const rs_stroke_t *st, float hw, float scale);
void   rs_stroke_contour(path_t *o, const float *pt, int n, bool closed, const rs_stroke_t *st, float hw, float scale, float dscale);
void   rs_stroke_path(path_t *o, const path_t *p, const rs_stroke_t *st, float hw, float scale, float dscale);
