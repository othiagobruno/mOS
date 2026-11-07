/* mOS — primitivas gráficas (pixels ARGB pré-multiplicados) */
#include "gfx.h"
#include "gpu.h"

/* ============================================================ utilidades */
rect_t rect_make(int x, int y, int w, int h) { rect_t r = {x, y, w, h}; return r; }
bool rect_empty(rect_t r) { return r.w <= 0 || r.h <= 0; }
rect_t rect_intersect(rect_t a, rect_t b) {
    int x0 = MAX(a.x, b.x), y0 = MAX(a.y, b.y);
    int x1 = MIN(a.x + a.w, b.x + b.w), y1 = MIN(a.y + a.h, b.y + b.h);
    rect_t r = {x0, y0, x1 - x0, y1 - y0};
    if (r.w < 0) r.w = 0;
    if (r.h < 0) r.h = 0;
    return r;
}
rect_t rect_union(rect_t a, rect_t b) {
    if (rect_empty(a)) return b;
    if (rect_empty(b)) return a;
    int x0 = MIN(a.x, b.x), y0 = MIN(a.y, b.y);
    int x1 = MAX(a.x + a.w, b.x + b.w), y1 = MAX(a.y + a.h, b.y + b.h);
    rect_t r = {x0, y0, x1 - x0, y1 - y0};
    return r;
}
bool rect_contains(rect_t r, int x, int y) { return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h; }
rect_t rect_inset(rect_t r, int d) { rect_t o = {r.x + d, r.y + d, r.w - 2 * d, r.h - 2 * d}; return o; }

static inline u32 div255(u32 v) { return (v + 1 + (v >> 8)) >> 8; }

u32 color_premul(u32 c) {
    u32 a = C_A(c);
    if (a == 255) return c;
    return (a << 24) | (div255(C_R(c) * a) << 16) | (div255(C_G(c) * a) << 8) | div255(C_B(c) * a);
}
u32 color_mix(u32 a, u32 b, int t) {
    int it = 255 - t;
    return ((div255(C_A(a) * it + C_A(b) * t)) << 24) | ((div255(C_R(a) * it + C_R(b) * t)) << 16) |
           ((div255(C_G(a) * it + C_G(b) * t)) << 8) | div255(C_B(a) * it + C_B(b) * t);
}
u32 color_lighten(u32 c, int amt) {
    int r = CLAMP((int)C_R(c) + amt, 0, 255), g = CLAMP((int)C_G(c) + amt, 0, 255), b = CLAMP((int)C_B(c) + amt, 0, 255);
    return (c & 0xFF000000) | (r << 16) | (g << 8) | b;
}
u32 color_alpha(u32 c, int a) { return WITH_ALPHA(c, div255(C_A(c) * (u32)CLAMP(a, 0, 255))); }

/* dst = src + dst*(1-αs), ambos pré-multiplicados */
static inline u32 blend_px(u32 d, u32 s) {
    u32 a = s >> 24;
    if (a == 255) return s;
    if (a == 0) return d;
    u32 ia = 255 - a;
    u32 rb = (d & 0x00FF00FF) * ia + 0x00800080;
    rb = ((rb + ((rb >> 8) & 0x00FF00FF)) >> 8) & 0x00FF00FF;
    u32 ag = ((d >> 8) & 0x00FF00FF) * ia + 0x00800080;
    ag = (ag + ((ag >> 8) & 0x00FF00FF)) & 0xFF00FF00;
    return s + (rb | ag);
}
/* multiplica um pixel pré-multiplicado por a/255 */
static inline u32 scale_px(u32 p, u32 a) {
    if (a >= 255) return p;
    u32 rb = (p & 0x00FF00FF) * a + 0x00800080;
    rb = ((rb + ((rb >> 8) & 0x00FF00FF)) >> 8) & 0x00FF00FF;
    u32 ag = ((p >> 8) & 0x00FF00FF) * a + 0x00800080;
    ag = (ag + ((ag >> 8) & 0x00FF00FF)) & 0xFF00FF00;
    return rb | ag;
}

/* ============================================================ SIMD (SSE2) e paralelismo
 *
 * O mOS não tem driver de GPU: a "GPU" é o conjunto de núcleos da CPU. As primitivas pesadas
 * dividem o retângulo em faixas de linhas (par_for, arch/smp.c) e cada faixa usa SSE2 (4 pixels
 * por instrução). As contas SIMD são bit a bit iguais às escalares (div255 com o mesmo arredondamento). */
#include <emmintrin.h>

#define PAR_MIN_PX 20000                     /* abaixo disso, acordar os outros núcleos não compensa */

static inline bool par_worth(int w, int h) { return h >= 8 && (i64)w * h >= PAR_MIN_PX && par_cpus() > 1; }
static inline int par_grain(int rows) { int g = rows / (par_cpus() * 4); return g < 2 ? 2 : g; }
/* roda fn(ctx, linha0, linha1, cpu) sobre [0, rows): em paralelo se o trabalho for grande */
static void par_rows(int w, int rows, par_fn_t fn, void *ctx) {
    if (rows <= 0) return;
    if (par_worth(w, rows)) par_for(rows, par_grain(rows), fn, ctx);
    else fn(ctx, 0, rows, 0);
}

static inline __m128i div255_epi16(__m128i x) {
    x = _mm_add_epi16(x, _mm_set1_epi16(128));
    return _mm_srli_epi16(_mm_add_epi16(x, _mm_srli_epi16(x, 8)), 8);
}
/* replica o alfa de cada pixel nos 4 canais (pixels já expandidos para 16 bits) */
static inline __m128i alpha_epi16(__m128i p) { return _mm_shufflehi_epi16(_mm_shufflelo_epi16(p, 0xFF), 0xFF); }

/* 4 pixels: s sobre d (pré-multiplicados) */
static inline __m128i blend4(__m128i d, __m128i s) {
    const __m128i z = _mm_setzero_si128(), c255 = _mm_set1_epi16(255);
    __m128i slo = _mm_unpacklo_epi8(s, z), shi = _mm_unpackhi_epi8(s, z);
    __m128i dlo = _mm_unpacklo_epi8(d, z), dhi = _mm_unpackhi_epi8(d, z);
    dlo = div255_epi16(_mm_mullo_epi16(dlo, _mm_sub_epi16(c255, alpha_epi16(slo))));
    dhi = div255_epi16(_mm_mullo_epi16(dhi, _mm_sub_epi16(c255, alpha_epi16(shi))));
    return _mm_add_epi8(_mm_packus_epi16(dlo, dhi), s);
}
/* 4 pixels multiplicados por a/255 */
static inline __m128i scale4(__m128i p, __m128i a16) {
    const __m128i z = _mm_setzero_si128();
    __m128i lo = div255_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(p, z), a16));
    __m128i hi = div255_epi16(_mm_mullo_epi16(_mm_unpackhi_epi8(p, z), a16));
    return _mm_packus_epi16(lo, hi);
}

/* d[i] = s[i] sobre d[i], com alfa global opcional (alpha < 255) */
static void blend_row(u32 *d, const u32 *s, int n, u32 alpha) {
    const __m128i amask = _mm_set1_epi32((int)0xFF000000), z = _mm_setzero_si128();
    const __m128i a16 = _mm_set1_epi16((short)alpha);
    int i = 0;
    for (; i + 4 <= n; i += 4) {
        __m128i sv = _mm_loadu_si128((const __m128i *)(s + i));
        if (alpha < 255) sv = scale4(sv, a16);
        __m128i am = _mm_and_si128(sv, amask);
        if (_mm_movemask_epi8(_mm_cmpeq_epi32(am, amask)) == 0xFFFF) { _mm_storeu_si128((__m128i *)(d + i), sv); continue; }
        if (_mm_movemask_epi8(_mm_cmpeq_epi32(am, z)) == 0xFFFF) continue;
        __m128i dv = _mm_loadu_si128((const __m128i *)(d + i));
        _mm_storeu_si128((__m128i *)(d + i), blend4(dv, sv));
    }
    for (; i < n; i++) d[i] = blend_px(d[i], alpha < 255 ? scale_px(s[i], alpha) : s[i]);
}
/* d[i] = cor constante (pré-multiplicada) sobre d[i] */
static void blend_const_row(u32 *d, u32 pm, int n) {
    if ((pm >> 24) == 255) { for (int i = 0; i < n; i++) d[i] = pm; return; }
    if (!(pm >> 24)) return;
    const __m128i sv = _mm_set1_epi32((int)pm);
    int i = 0;
    for (; i + 4 <= n; i += 4) {
        __m128i dv = _mm_loadu_si128((const __m128i *)(d + i));
        _mm_storeu_si128((__m128i *)(d + i), blend4(dv, sv));
    }
    for (; i < n; i++) d[i] = blend_px(d[i], pm);
}

/* ============================================================ superfícies */
surface_t *surface_new(int w, int h) {
    surface_t *s = kcalloc(1, sizeof(surface_t));
    s->w = w; s->h = h; s->stride = w;
    s->px = kmalloc((usize)MAX(w, 1) * MAX(h, 1) * 4);
    surface_reset_clip(s);
    return s;
}
void surface_free(surface_t *s) { if (s) { kfree(s->px); kfree(s); } }
void surface_resize(surface_t *s, int w, int h) {
    if (w == s->w && h == s->h) return;
    if ((usize)w * h > (usize)s->stride * s->h || w > s->stride) {
        kfree(s->px);
        s->px = kmalloc((usize)MAX(w, 1) * MAX(h, 1) * 4);
        s->stride = w;
    } else {
        s->stride = w;
    }
    s->w = w; s->h = h;
    surface_reset_clip(s);
}
void surface_reset_clip(surface_t *s) { s->clip = rect_make(0, 0, s->w, s->h); s->ox = s->oy = 0; }
void surface_clip(surface_t *s, rect_t r) {
    r.x += s->ox; r.y += s->oy;
    s->clip = rect_intersect(r, rect_make(0, 0, s->w, s->h));
}
rect_t surface_push_clip(surface_t *s, rect_t r) {
    rect_t prev = s->clip;
    r.x += s->ox; r.y += s->oy;
    s->clip = rect_intersect(prev, r);
    return prev;
}
void surface_pop_clip(surface_t *s, rect_t prev) { s->clip = prev; }

/* ============================================================ preenchimentos */
typedef struct { surface_t *s; rect_t r; u32 pm; } fill_job_t;
static void fill_rows(void *c, int b, int e, int cpu) {
    fill_job_t *j = c;
    for (int y = b; y < e; y++) blend_const_row(j->s->px + (usize)(j->r.y + y) * j->s->stride + j->r.x, j->pm, j->r.w);
}

void gfx_clear(surface_t *s, u32 c) {
    u32 p = color_premul(c);
    if ((p >> 24) == 255) { fill_job_t j = { s, rect_make(0, 0, s->w, s->h), p }; par_rows(s->w, s->h, fill_rows, &j); return; }
    for (int y = 0; y < s->h; y++) {
        u32 *row = s->px + (usize)y * s->stride;
        for (int x = 0; x < s->w; x++) row[x] = p;
    }
}

void gfx_fill(surface_t *s, int x, int y, int w, int h, u32 c) {
    rect_t r = rect_intersect(rect_make(x + s->ox, y + s->oy, w, h), s->clip);
    if (rect_empty(r) || !C_A(c)) return;
    fill_job_t j = { s, r, color_premul(c) };
    par_rows(r.w, r.h, fill_rows, &j);
}

static inline void put(surface_t *s, int ax, int ay, u32 pm, int cover) {
    /* coordenadas absolutas, pm pré-multiplicado */
    if (ax < s->clip.x || ay < s->clip.y || ax >= s->clip.x + s->clip.w || ay >= s->clip.y + s->clip.h) return;
    u32 *d = s->px + (usize)ay * s->stride + ax;
    *d = blend_px(*d, cover >= 255 ? pm : scale_px(pm, cover));
}

void gfx_pixel(surface_t *s, int x, int y, u32 c, int cover) { put(s, x + s->ox, y + s->oy, color_premul(c), cover); }
void gfx_hline(surface_t *s, int x, int y, int w, u32 c) { gfx_fill(s, x, y, w, 1, c); }
void gfx_vline(surface_t *s, int x, int y, int h, u32 c) { gfx_fill(s, x, y, 1, h, c); }
void gfx_rect(surface_t *s, int x, int y, int w, int h, u32 c) {
    gfx_fill(s, x, y, w, 1, c); gfx_fill(s, x, y + h - 1, w, 1, c);
    gfx_fill(s, x, y + 1, 1, h - 2, c); gfx_fill(s, x + w - 1, y + 1, 1, h - 2, c);
}

typedef struct { surface_t *s; rect_t r; int y, h; u32 top, bottom; } gradv_job_t;
static void gradv_rows(void *c, int b, int e, int cpu) {
    gradv_job_t *j = c;
    for (int k = b; k < e; k++) {
        int row = j->r.y + k, t = row - j->y;
        u32 pm = color_premul(color_mix(j->top, j->bottom, j->h > 1 ? t * 255 / (j->h - 1) : 0));
        blend_const_row(j->s->px + (usize)row * j->s->stride + j->r.x, pm, j->r.w);
    }
}
void gfx_grad_v(surface_t *s, int x, int y, int w, int h, u32 top, u32 bottom) {
    if (h <= 0) return;
    rect_t r = rect_intersect(rect_make(x + s->ox, y + s->oy, w, h), s->clip);
    if (rect_empty(r)) return;
    gradv_job_t j = { s, r, y + s->oy, h, top, bottom };
    par_rows(r.w, r.h, gradv_rows, &j);
}
void gfx_grad_h(surface_t *s, int x, int y, int w, int h, u32 l, u32 r) {
    if (w <= 0) return;
    for (int i = 0; i < w; i++) gfx_fill(s, x + i, y, 1, h, color_mix(l, r, w > 1 ? i * 255 / (w - 1) : 0));
}

/* ============================================================ SDF: retângulos arredondados */
static inline float sd_rrect(float px, float py, float cx, float cy, float hw, float hh, float r) {
    float qx = fabsf_(px - cx) - (hw - r), qy = fabsf_(py - cy) - (hh - r);
    float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
    float out = fsqrt(ox * ox + oy * oy);
    float in = qx > qy ? qx : qy;
    if (in > 0) in = 0;
    return out + in - r;
}
static inline int cov_from_d(float d) {
    float c = 0.5f - d;
    if (c <= 0) return 0;
    if (c >= 1) return 255;
    return (int)(c * 255.0f + 0.5f);
}

typedef struct { surface_t *s; rect_t r; float x, y, w, h, rtl, rtr, rbr, rbl; u32 top, bottom; } rr_job_t;
static void rrect_rows(void *c, int b, int e, int cpu) {
    rr_job_t *J = c;
    surface_t *s = J->s;
    rect_t r = J->r;
    float x = J->x, y = J->y, w = J->w, h = J->h, rtl = J->rtl, rtr = J->rtr, rbr = J->rbr, rbl = J->rbl;
    float cx = x + w / 2, cy = y + h / 2, hw = w / 2, hh = h / 2;
    bool grad = J->top != J->bottom;
    u32 pm = color_premul(J->top);
    float rmax_l = MAX(rtl, rbl), rmax_r = MAX(rtr, rbr);
    for (int j = r.y + b; j < r.y + e; j++) {
        float py = j + 0.5f;
        if (grad) pm = color_premul(color_mix(J->top, J->bottom, CLAMP((int)((py - y) * 255 / h), 0, 255)));
        bool upper = py < cy;
        float rl = upper ? rtl : rbl, rr = upper ? rtr : rbr;
        /* faixa interna: totalmente coberta se não estamos nas bordas verticais */
        bool vedge = (py - y) < 1.0f || (y + h - py) < 1.0f;
        bool in_l_corner = upper ? (py < y + rl) : (py > y + h - rl);
        bool in_r_corner = upper ? (py < y + rr) : (py > y + h - rr);
        int fl = (int)fceil(x + (in_l_corner ? rmax_l : 0) + 1), fr = (int)ffloor(x + w - (in_r_corner ? rmax_r : 0) - 1);
        if (vedge) fl = fr + 1;
        u32 *row = s->px + (usize)j * s->stride;
        for (int i = r.x; i < r.x + r.w; i++) {
            if (i >= fl && i < fr) {
                int en = MIN(fr, r.x + r.w);
                blend_const_row(row + i, pm, en - i);
                i = en - 1;
                continue;
            }
            float px = i + 0.5f;
            float rad = px < cx ? rl : rr;
            int cv = cov_from_d(sd_rrect(px, py, cx, cy, hw, hh, rad));
            if (cv) row[i] = blend_px(row[i], scale_px(pm, cv));
        }
    }
}

void gfx_round_rect_ex(surface_t *s, float x, float y, float w, float h, float rtl, float rtr, float rbr, float rbl, u32 top, u32 bottom) {
    if (w <= 0 || h <= 0) return;
    float maxr = (w < h ? w : h) / 2;
    if (rtl > maxr) rtl = maxr;
    if (rtr > maxr) rtr = maxr;
    if (rbr > maxr) rbr = maxr;
    if (rbl > maxr) rbl = maxr;
    x += s->ox; y += s->oy;
    int x0 = (int)ffloor(x), y0 = (int)ffloor(y), x1 = (int)fceil(x + w), y1 = (int)fceil(y + h);
    rect_t r = rect_intersect(rect_make(x0, y0, x1 - x0, y1 - y0), s->clip);
    if (rect_empty(r)) return;
    rr_job_t jb = { s, r, x, y, w, h, rtl, rtr, rbr, rbl, top, bottom };
    par_rows(r.w, r.h, rrect_rows, &jb);
}

void gfx_round_rect(surface_t *s, float x, float y, float w, float h, float r, u32 c) {
    gfx_round_rect_ex(s, x, y, w, h, r, r, r, r, c, c);
}
void gfx_round_rect_grad(surface_t *s, float x, float y, float w, float h, float r, u32 top, u32 bottom) {
    gfx_round_rect_ex(s, x, y, w, h, r, r, r, r, top, bottom);
}

typedef struct { surface_t *s; rect_t rc; float x, y, w, h, r, t; u32 pm; } rrs_job_t;
static void rrect_stroke_rows(void *cx_, int b, int e, int cpu) {
    rrs_job_t *J = cx_;
    surface_t *s = J->s;
    rect_t rc = J->rc;
    float x = J->x, y = J->y, w = J->w, h = J->h, r = J->r, t = J->t;
    float cx = x + w / 2, cy = y + h / 2, hw = w / 2, hh = h / 2;
    float ri = r - t > 0 ? r - t : 0;
    u32 pm = J->pm;
    for (int j = rc.y + b; j < rc.y + e; j++) {
        float py = j + 0.5f;
        bool mid = py > y + r + 1 && py < y + h - r - 1;
        u32 *row = s->px + (usize)j * s->stride;
        for (int i = rc.x; i < rc.x + rc.w; i++) {
            float px = i + 0.5f;
            if (mid && px > x + t + 1 && px < x + w - t - 1) { int ni = (int)(x + w - t - 1) - 1; if (ni > i) { i = ni; continue; } }
            int co = cov_from_d(sd_rrect(px, py, cx, cy, hw, hh, r));
            int ci = cov_from_d(sd_rrect(px, py, cx, cy, hw - t, hh - t, ri));
            int cv = co - ci;
            if (cv > 0) row[i] = blend_px(row[i], scale_px(pm, cv));
        }
    }
}

void gfx_round_rect_stroke(surface_t *s, float x, float y, float w, float h, float r, float t, u32 c) {
    if (w <= 0 || h <= 0) return;
    float maxr = (w < h ? w : h) / 2;
    if (r > maxr) r = maxr;
    x += s->ox; y += s->oy;
    int x0 = (int)ffloor(x), y0 = (int)ffloor(y), x1 = (int)fceil(x + w), y1 = (int)fceil(y + h);
    rect_t rc = rect_intersect(rect_make(x0, y0, x1 - x0, y1 - y0), s->clip);
    if (rect_empty(rc)) return;
    rrs_job_t jb = { s, rc, x, y, w, h, r, t, color_premul(c) };
    par_rows(rc.w, rc.h, rrect_stroke_rows, &jb);
}

void gfx_circle(surface_t *s, float cx, float cy, float r, u32 c) { gfx_round_rect(s, cx - r, cy - r, 2 * r, 2 * r, r, c); }
void gfx_circle_stroke(surface_t *s, float cx, float cy, float r, float t, u32 c) { gfx_round_rect_stroke(s, cx - r, cy - r, 2 * r, 2 * r, r, t, c); }

static float fatan2(float y, float x) {
    float ax = fabsf_(x), ay = fabsf_(y);
    float a = (ax > ay ? ay / (ax + 1e-9f) : ax / (ay + 1e-9f));
    float s = a * a;
    float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
    if (ay > ax) r = 1.57079637f - r;
    if (x < 0) r = 3.14159274f - r;
    if (y < 0) r = -r;
    return r;
}

/* arco de anel: ângulos em radianos, 0 = topo, sentido horário */
void gfx_ring_arc(surface_t *s, float cx, float cy, float r, float t, float a0, float a1, u32 c) {
    cx += s->ox; cy += s->oy;
    rect_t rc = rect_intersect(rect_make((int)(cx - r - 1), (int)(cy - r - 1), (int)(2 * r + 3), (int)(2 * r + 3)), s->clip);
    u32 pm = color_premul(c);
    for (int j = rc.y; j < rc.y + rc.h; j++)
        for (int i = rc.x; i < rc.x + rc.w; i++) {
            float dx = i + 0.5f - cx, dy = j + 0.5f - cy;
            float d = fsqrt(dx * dx + dy * dy);
            int co = cov_from_d(d - r) - cov_from_d(d - (r - t));
            if (co <= 0) continue;
            float ang = fatan2(dx, -dy);
            if (ang < 0) ang += 2 * PI_F;
            if (ang < a0 || ang > a1) continue;
            u32 *p = s->px + (usize)j * s->stride + i;
            *p = blend_px(*p, scale_px(pm, co));
        }
}

void gfx_line(surface_t *s, float x0, float y0, float x1, float y1, float t, u32 c) {
    x0 += s->ox; y0 += s->oy; x1 += s->ox; y1 += s->oy;
    float hr = t / 2;
    int bx0 = (int)ffloor(MIN(x0, x1) - hr - 1), by0 = (int)ffloor(MIN(y0, y1) - hr - 1);
    int bx1 = (int)fceil(MAX(x0, x1) + hr + 1), by1 = (int)fceil(MAX(y0, y1) + hr + 1);
    rect_t rc = rect_intersect(rect_make(bx0, by0, bx1 - bx0, by1 - by0), s->clip);
    float dx = x1 - x0, dy = y1 - y0, ll = dx * dx + dy * dy;
    u32 pm = color_premul(c);
    for (int j = rc.y; j < rc.y + rc.h; j++)
        for (int i = rc.x; i < rc.x + rc.w; i++) {
            float px = i + 0.5f - x0, py = j + 0.5f - y0;
            float h = ll > 0 ? (px * dx + py * dy) / ll : 0;
            h = CLAMP(h, 0.0f, 1.0f);
            float ex = px - dx * h, ey = py - dy * h;
            int cv = cov_from_d(fsqrt(ex * ex + ey * ey) - hr);
            if (cv) { u32 *p = s->px + (usize)j * s->stride + i; *p = blend_px(*p, scale_px(pm, cv)); }
        }
}

/* sombra suave aproximada (gaussiana via smoothstep sobre a SDF) */
typedef struct { surface_t *s; rect_t rc; float fx, fy; int w, h; float r, blur; int alpha, dy; } shadow_job_t;
static void shadow_rows(void *c, int b, int e, int cpu);
void gfx_shadow(surface_t *s, int x, int y, int w, int h, float r, float blur, int alpha, int dy) {
    float fx = x + s->ox, fy = y + s->oy + dy;
    int b = (int)fceil(blur);
    rect_t rc = rect_intersect(rect_make((int)fx - b, (int)fy - b, w + 2 * b, h + 2 * b), s->clip);
    if (rect_empty(rc)) return;
    shadow_job_t jb = { s, rc, fx, fy, w, h, r, blur, alpha, dy };
    par_rows(rc.w, rc.h, shadow_rows, &jb);
}

static void shadow_rows(void *c, int b, int e, int cpu) {
    shadow_job_t *J = c;
    surface_t *s = J->s;
    rect_t rc = J->rc;
    float fx = J->fx, fy = J->fy, r = J->r, blur = J->blur;
    int w = J->w, h = J->h, alpha = J->alpha, dy = J->dy;
    float cx = fx + w / 2.0f, cy = fy + h / 2.0f;
    float inner_skip = r + 2;
    for (int j = rc.y + b; j < rc.y + e; j++) {
        float py = j + 0.5f;
        float wy = fy - dy; /* a janela em si (sem o deslocamento da sombra) cobre este miolo */
        bool mid = py > MAX(fy, wy) + inner_skip && py < MIN(fy, wy) + h - inner_skip;
        u32 *row = s->px + (usize)j * s->stride;
        for (int i = rc.x; i < rc.x + rc.w; i++) {
            float px = i + 0.5f;
            if (mid && px > fx + inner_skip && px < fx + w - inner_skip) { int ni = (int)(fx + w - inner_skip) - 1; if (ni > i) { i = ni; continue; } }
            float d = sd_rrect(px, py, cx, cy, w / 2.0f, h / 2.0f, r);
            if (d < -blur * 0.5f) d = -blur * 0.5f;
            float t = (d + blur * 0.5f) / (blur * 1.5f);
            if (t >= 1) continue;
            float k = 1 - t;
            k = k * k * (3 - 2 * k);
            k *= k;
            u32 a = (u32)(alpha * k);
            if (a) row[i] = blend_px(row[i], a << 24);
        }
    }
}

/* ============================================================ rasterizador de área (font-rs) */
/* finito (nem NaN nem infinito), pelos bits: não depende de como o compilador trata NaN */
static inline bool fin_(float f) { u32 b; memcpy(&b, &f, 4); return (b & 0x7F800000u) != 0x7F800000u; }
/* acc tem w*h células. Os pontos podem vir de fontes dos apps (GF_REGISTER), fora da caixa do glifo ou
 * absurdos: todo índice fica dentro da linha e da altura, e coordenada não finita é descartada. */
void raster_line(float *acc, int w, int h, float x0, float y0, float x1, float y1) {
    if (w < 2 || h < 1 || !fin_(x0) || !fin_(y0) || !fin_(x1) || !fin_(y1)) return;
    if (fabsf_(y0 - y1) < 1e-6f) return;
    float dir = 1;
    if (y0 > y1) { float t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; dir = -1; }
    float dxdy = (x1 - x0) / (y1 - y0);
    float x = x0;
    if (y0 < 0) { x -= y0 * dxdy; y0 = 0; }
    if (y0 >= h) return;                                  /* também evita (int) de valor enorme */
    int ys = (int)y0, ye = (int)fceil(MIN((float)h, y1));
    for (int yy = ys; yy < ye; yy++) {
        int ls = yy * w;
        float dy = MIN((float)(yy + 1), y1) - MAX((float)yy, y0);
        float xn = x + dxdy * dy;
        float d = dy * dir;
        float a0x = x < xn ? x : xn, a1x = x < xn ? xn : x;
        /* as duas pontas em [0, w - 1.001]; !(v >= 0) também pega NaN de x somado a dxdy enorme */
        if (!(a0x >= 0)) a0x = 0;
        if (!(a1x >= 0)) a1x = 0;
        if (a0x > w - 1.001f) a0x = w - 1.001f;
        if (a1x > w - 1.001f) a1x = w - 1.001f;
        float x0f = ffloor(a0x);
        int x0i = (int)x0f;
        float x1c = fceil(a1x);
        int x1i = (int)x1c;
        if (x1i <= x0i + 1) {
            float xmf = 0.5f * (a0x + a1x) - x0f;
            acc[ls + x0i] += d - d * xmf;
            acc[ls + x0i + 1] += d * xmf;
        } else {
            float sc = 1.0f / (a1x - a0x);
            float x0r = a0x - x0f;
            float aa0 = 0.5f * sc * (1 - x0r) * (1 - x0r);
            float x1r = a1x - x1c + 1;
            float am = 0.5f * sc * x1r * x1r;
            acc[ls + x0i] += d * aa0;
            if (x1i == x0i + 2) {
                acc[ls + x0i + 1] += d * (1 - aa0 - am);
            } else {
                float a1 = sc * (1.5f - x0r);
                acc[ls + x0i + 1] += d * (a1 - aa0);
                for (int xi = x0i + 2; xi < x1i - 1; xi++) acc[ls + xi] += d * sc;
                float a2 = a1 + (x1i - x0i - 3) * sc;
                acc[ls + x1i - 1] += d * (1 - a2 - am);
            }
            acc[ls + x1i] += d * am;
        }
        x = xn;
    }
}

void raster_finish(const float *acc, int n, u8 *out) {
    float a = 0;
    for (int i = 0; i < n; i++) {
        a += acc[i];
        float v = a < 0 ? -a : a;
        if (v > 1) v = 1;
        out[i] = (u8)(v * 255 + 0.5f);
    }
}

void gfx_polygon(surface_t *s, const float *xy, int n, u32 c) {
    if (n < 3) return;
    float mnx = xy[0], mxx = xy[0], mny = xy[1], mxy = xy[1];
    for (int i = 1; i < n; i++) {
        mnx = MIN(mnx, xy[2 * i]); mxx = MAX(mxx, xy[2 * i]);
        mny = MIN(mny, xy[2 * i + 1]); mxy = MAX(mxy, xy[2 * i + 1]);
    }
    int bx = (int)ffloor(mnx), by = (int)ffloor(mny);
    int bw = (int)fceil(mxx) - bx + 2, bh = (int)fceil(mxy) - by + 1;
    if (bw <= 0 || bh <= 0 || bw * bh > 4 * 1024 * 1024) return;
    float *acc = kcalloc((usize)bw * bh + 4, sizeof(float));
    u8 *m = kmalloc((usize)bw * bh + 4);
    for (int i = 0; i < n; i++) {
        int k = (i + 1) % n;
        raster_line(acc, bw, bh, xy[2 * i] - bx, xy[2 * i + 1] - by, xy[2 * k] - bx, xy[2 * k + 1] - by);
    }
    raster_finish(acc, bw * bh, m);
    gfx_mask(s, bx, by, m, bw, bh, bw, c);
    kfree(acc);
    kfree(m);
}

/* ============================================================ blits */
typedef struct { surface_t *d; const surface_t *s; rect_t dr; int sx, sy, alpha; } blit_job_t;
static void blit_rows(void *c, int b, int e, int cpu) {
    blit_job_t *J = c;
    for (int j = b; j < e; j++)
        memcpy(J->d->px + (usize)(J->dr.y + j) * J->d->stride + J->dr.x,
               J->s->px + (usize)(J->sy + j) * J->s->stride + J->sx, (usize)J->dr.w * 4);
}
static void blend_rows(void *c, int b, int e, int cpu) {
    blit_job_t *J = c;
    for (int j = b; j < e; j++)
        blend_row(J->d->px + (usize)(J->dr.y + j) * J->d->stride + J->dr.x,
                  J->s->px + (usize)(J->sy + j) * J->s->stride + J->sx, J->dr.w, (u32)MIN(J->alpha, 255));
}

void gfx_blit_rect(surface_t *d, const surface_t *src, rect_t sr, int dx, int dy) {
    dx += d->ox; dy += d->oy;
    rect_t dr = rect_intersect(rect_make(dx, dy, sr.w, sr.h), d->clip);
    if (rect_empty(dr)) return;
    blit_job_t J = { d, src, dr, sr.x + (dr.x - dx), sr.y + (dr.y - dy), 255 };
    if (d == src) blit_rows(&J, 0, dr.h, 0);         /* rolagem na mesma superfície: a ordem das linhas importa */
    else par_rows(dr.w, dr.h, blit_rows, &J);
}
void gfx_blit(surface_t *d, const surface_t *src, int dx, int dy) { gfx_blit_rect(d, src, rect_make(0, 0, src->w, src->h), dx, dy); }

void gfx_blend_rect(surface_t *d, const surface_t *src, rect_t sr, int dx, int dy, int alpha) {
    dx += d->ox; dy += d->oy;
    rect_t dr = rect_intersect(rect_make(dx, dy, sr.w, sr.h), d->clip);
    if (rect_empty(dr) || alpha <= 0) return;
    blit_job_t J = { d, src, dr, sr.x + (dr.x - dx), sr.y + (dr.y - dy), alpha };
    if (d == src) blend_rows(&J, 0, dr.h, 0);
    else par_rows(dr.w, dr.h, blend_rows, &J);
}
void gfx_blend(surface_t *d, const surface_t *src, int dx, int dy, int alpha) { gfx_blend_rect(d, src, rect_make(0, 0, src->w, src->h), dx, dy, alpha); }

/* escala bilinear com alfa */
typedef struct { surface_t *d; const surface_t *src; rect_t dr; int dx, dy, alpha; u32 fx, fy; } scaled_job_t;
static void scaled_rows(void *c, int b, int e, int cpu);
void gfx_blit_scaled(surface_t *d, const surface_t *src, int dx, int dy, int dw, int dh, int alpha) {
    if (dw <= 0 || dh <= 0) return;
    if (dw == src->w && dh == src->h) { gfx_blend(d, src, dx, dy, alpha); return; }
    dx += d->ox; dy += d->oy;
    rect_t dr = rect_intersect(rect_make(dx, dy, dw, dh), d->clip);
    if (rect_empty(dr)) return;
    u32 fx = (u32)(((u64)src->w << 16) / dw), fy = (u32)(((u64)src->h << 16) / dh);
    scaled_job_t J = { d, src, dr, dx, dy, alpha, fx, fy };
    par_rows(dr.w, dr.h, scaled_rows, &J);
}
static void scaled_rows(void *c, int b, int e, int cpu) {
    scaled_job_t *J = c;
    surface_t *d = J->d;
    const surface_t *src = J->src;
    rect_t dr = J->dr;
    int dx = J->dx, dy = J->dy, alpha = J->alpha;
    u32 fx = J->fx, fy = J->fy;
    for (int j = dr.y + b; j < dr.y + e; j++) {
        i32 syf = (i32)((j - dy) * fy + fy / 2) - 32768;
        if (syf < 0) syf = 0;
        int sy0 = syf >> 16, sy1 = MIN(sy0 + 1, src->h - 1);
        u32 wy = (syf >> 8) & 0xFF;
        u32 *dp = d->px + (usize)j * d->stride;
        const u32 *r0 = src->px + (usize)sy0 * src->stride, *r1 = src->px + (usize)sy1 * src->stride;
        for (int i = dr.x; i < dr.x + dr.w; i++) {
            i32 sxf = (i32)((i - dx) * fx + fx / 2) - 32768;
            if (sxf < 0) sxf = 0;
            int sx0 = sxf >> 16, sx1 = MIN(sx0 + 1, src->w - 1);
            u32 wx = (sxf >> 8) & 0xFF;
            u32 top = scale_px(r0[sx0], 255 - wx) + scale_px(r0[sx1], wx);
            u32 bot = scale_px(r1[sx0], 255 - wx) + scale_px(r1[sx1], wx);
            u32 p = scale_px(top, 255 - wy) + scale_px(bot, wy);
            if (alpha < 255) p = scale_px(p, alpha);
            dp[i] = blend_px(dp[i], p);
        }
    }
}

/* desfoque por caixa (3 passes ≈ gaussiano), em coordenadas absolutas.
 * Cada pixel vira 4 inteiros de 32 bits num registrador SSE (um por canal); a média usa
 * floor((soma + 0.5) * (1/janela)) em float, que dá exatamente soma / janela (soma < 2^24 e a parte
 * fracionária fica longe de 0 e de 1), sem as 4 divisões inteiras por pixel.
 * A passada vertical anda por faixas de BLUR_STRIP colunas, linha a linha (acesso sequencial à
 * memória), guardando num anel as rad+1 linhas originais que ainda serão subtraídas. */
#define BLUR_STRIP 64

static inline __m128i px_epi32(u32 p) {
    const __m128i z = _mm_setzero_si128();
    return _mm_unpacklo_epi16(_mm_unpacklo_epi8(_mm_cvtsi32_si128((int)p), z), z);
}
static inline u32 avg_px(__m128i sum, __m128 rcp) {
    __m128i q = _mm_cvttps_epi32(_mm_mul_ps(_mm_add_ps(_mm_cvtepi32_ps(sum), _mm_set1_ps(0.5f)), rcp));
    q = _mm_packs_epi32(q, q);
    return (u32)_mm_cvtsi128_si32(_mm_packus_epi16(q, q));
}

static void box_h_row(u32 *row, int w, int rad, __m128 rcp, u32 *tmp) {
    __m128i sum = _mm_setzero_si128();
    for (int i = -rad; i <= rad; i++) sum = _mm_add_epi32(sum, px_epi32(row[CLAMP(i, 0, w - 1)]));
    for (int i = 0; i < w; i++) {
        tmp[i] = avg_px(sum, rcp);
        sum = _mm_add_epi32(sum, _mm_sub_epi32(px_epi32(row[MIN(i + rad + 1, w - 1)]), px_epi32(row[MAX(i - rad, 0)])));
    }
    memcpy(row, tmp, (usize)w * 4);
}

/* colunas [c0, c1) de r; scratch: BLUR_STRIP somas (16 B) + anel de (rad+1) linhas */
static void box_v_strip(u32 *px, int stride, rect_t r, int rad, int c0, int c1, u8 *scratch, __m128 rcp) {
    int sw = c1 - c0, H = r.h, R = rad + 1;
    __m128i *sum = (__m128i *)scratch;
    u32 *ring = (u32 *)(scratch + BLUR_STRIP * 16);
    u32 *base = px + (usize)r.y * stride + r.x + c0;
    for (int c = 0; c < sw; c++) sum[c] = _mm_setzero_si128();
    for (int j = -rad; j <= rad; j++) {
        const u32 *row = base + (usize)CLAMP(j, 0, H - 1) * stride;
        for (int c = 0; c < sw; c++) sum[c] = _mm_add_epi32(sum[c], px_epi32(row[c]));
    }
    for (int j = 0; j < H; j++) {
        u32 *row = base + (usize)j * stride;
        memcpy(ring + (usize)(j % R) * sw, row, (usize)sw * 4);       /* original da linha j */
        int ai = MIN(j + rad + 1, H - 1), bi = MAX(j - rad, 0);
        const u32 *arow = ai > j ? base + (usize)ai * stride : ring + (usize)(ai % R) * sw;
        const u32 *brow = ring + (usize)(bi % R) * sw;
        for (int c = 0; c < sw; c++) {
            row[c] = avg_px(sum[c], rcp);
            sum[c] = _mm_add_epi32(sum[c], _mm_sub_epi32(px_epi32(arow[c]), px_epi32(brow[c])));
        }
    }
}

typedef struct { u32 *px; int stride; rect_t r; int rad; u8 *scratch; usize per; float rcp; } blur_job_t;
static void boxh_job(void *c, int b, int e, int cpu) {
    blur_job_t *J = c;
    __m128 rcp = _mm_set1_ps(J->rcp);
    u32 *tmp = (u32 *)(J->scratch + (usize)cpu * J->per);
    for (int j = b; j < e; j++) box_h_row(J->px + (usize)(J->r.y + j) * J->stride + J->r.x, J->r.w, J->rad, rcp, tmp);
}
static void boxv_job(void *c, int b, int e, int cpu) {
    blur_job_t *J = c;
    __m128 rcp = _mm_set1_ps(J->rcp);
    for (int s = b; s < e; s++) {
        int c0 = s * BLUR_STRIP;
        box_v_strip(J->px, J->stride, J->r, J->rad, c0, MIN(c0 + BLUR_STRIP, J->r.w), J->scratch + (usize)cpu * J->per, rcp);
    }
}

static u8 *blur_scratch;
static usize blur_scratch_cap;

/* 3 x (horizontal + vertical), divididos entre os núcleos (só do BSP: usa um buffer estático) */
static void blur3(u32 *px, int stride, rect_t r, int rad) {
    if (rect_empty(r) || rad < 1) return;
    int ncpu = par_cpus();
    usize per = ALIGN_UP(MAX((usize)r.w * 4, (usize)BLUR_STRIP * 16 + (usize)(rad + 1) * BLUR_STRIP * 4), 64);
    usize need = per * ncpu + 64;
    if (need > blur_scratch_cap) {
        kfree(blur_scratch);
        blur_scratch = kmalloc(need);
        blur_scratch_cap = blur_scratch ? need : 0;
        if (!blur_scratch) return;
    }
    blur_job_t J = { px, stride, r, rad, (u8 *)ALIGN_UP((uptr)blur_scratch, 64), per, 1.0f / (float)(2 * rad + 1) };
    int nstrips = (r.w + BLUR_STRIP - 1) / BLUR_STRIP;
    bool par = par_worth(r.w, r.h);
    for (int k = 0; k < 3; k++) {
        if (par) { par_for(r.h, par_grain(r.h), boxh_job, &J); par_for(nstrips, 1, boxv_job, &J); }
        else { boxh_job(&J, 0, r.h, 0); boxv_job(&J, 0, nstrips, 0); }
    }
}

void gfx_blur(surface_t *s, rect_t r, int radius) {
    r.x += s->ox; r.y += s->oy;
    r = rect_intersect(r, rect_make(0, 0, s->w, s->h));
    blur3(s->px, s->stride, r, radius);
}

/* ============================================================ vidro líquido */
static u32 *glass_buf;
static usize glass_cap;

static u32 *grow_buf(u32 **b, usize *cap, usize n) {
    if (n > *cap) { kfree(*b); *b = kmalloc(n * 4); *cap = n; }
    return *b;
}

static inline u32 lerp_px(u32 a, u32 b, u32 t) { return scale_px(a, 255 - t) + scale_px(b, t); }

/* amostra bilinear num buffer (coordenadas em pixels do buffer, já limitadas) */
static inline u32 sample_bilinear(const u32 *b, int bw, int bh, float fx, float fy) {
    if (fx < 0) fx = 0;
    if (fy < 0) fy = 0;
    if (fx > bw - 1.001f) fx = bw - 1.001f;
    if (fy > bh - 1.001f) fy = bh - 1.001f;
    int x0 = (int)fx, y0 = (int)fy;
    int x1 = MIN(x0 + 1, bw - 1), y1 = MIN(y0 + 1, bh - 1);
    u32 tx = (u32)((fx - x0) * 255), ty = (u32)((fy - y0) * 255);
    const u32 *r0 = b + (usize)y0 * bw, *r1 = b + (usize)y1 * bw;
    return lerp_px(lerp_px(r0[x0], r0[x1], tx), lerp_px(r1[x0], r1[x1], tx), ty);
}

static inline u32 white_px(u32 a) { a = MIN(a, 255u); return (a << 24) | (a << 16) | (a << 8) | a; }

/* distância com sinal ao retângulo arredondado + normal para fora */
static inline float sd_rrect_n(float ax, float ay, float hw, float hh, float r, float *nx, float *ny) {
    float sx = ax < 0 ? -1.0f : 1.0f, sy = ay < 0 ? -1.0f : 1.0f;
    float qx = fabsf_(ax) - (hw - r), qy = fabsf_(ay) - (hh - r);
    if (qx > 0 && qy > 0) {
        float l = fsqrt(qx * qx + qy * qy);
        *nx = sx * qx / l; *ny = sy * qy / l;
        return l - r;
    }
    if (qx > qy) { *nx = sx; *ny = 0; return qx - r; }
    *nx = 0; *ny = sy;
    return qy - r;
}

/* intensidade especular para a normal (nx, ny): luz principal no topo-esquerda e reflexo no lado oposto */
static inline float specular(float nx, float ny) {
    float d = nx * -0.55f + ny * -0.835f;
    float a = d > 0 ? d : 0, b = d < 0 ? -d : 0;
    return a * a * 1.0f + b * b * 0.45f + 0.18f;
}

typedef struct {
    surface_t *s; const glass_t *g;
    u32 *b; int bw, bh;
    rect_t S, dr;
    float x, y, w, h, r;
} glass_job_t;
static void glass_down_rows(void *c, int jb, int je, int cpu);
static void glass_sat_rows(void *c, int jb, int je, int cpu);
static void glass_comp_rows(void *c, int jb, int je, int cpu);

void gfx_glass(surface_t *s, float x, float y, float w, float h, float r, const glass_t *g) {
    if (w <= 2 || h <= 2) return;
    float maxr = (w < h ? w : h) / 2;
    if (r > maxr) r = maxr;
    x += s->ox; y += s->oy;
    int x0 = (int)ffloor(x), y0 = (int)ffloor(y), x1 = (int)fceil(x + w), y1 = (int)fceil(y + h);
    rect_t dr = rect_intersect(rect_make(x0, y0, x1 - x0, y1 - y0), s->clip);
    if (rect_empty(dr)) return;

    /* 1) copia o fundo (com margem) subamostrado 2x */
    int m = GLASS_REACH(g);
    rect_t S = rect_intersect(rect_make(x0 - m, y0 - m, x1 - x0 + 2 * m, y1 - y0 + 2 * m), rect_make(0, 0, s->w, s->h));
    if (rect_empty(S)) return;
    int bw = (S.w + 1) / 2, bh = (S.h + 1) / 2;
    u32 *b = grow_buf(&glass_buf, &glass_cap, (usize)bw * bh);
    if (!b) { glass_cap = 0; return; }
    glass_job_t J = { s, g, b, bw, bh, S, dr, x, y, w, h, r };
    par_rows(bw * 2, bh, glass_down_rows, &J);
    /* 2) desfoque gaussiano aproximado (3 caixas) na resolução reduzida */
    blur3(b, bw, rect_make(0, 0, bw, bh), MAX(1, g->blur / 2));
    /* 3) vibração: realça a saturação do que está atrás */
    if (g->saturation && g->saturation != 256) par_rows(bw, bh, glass_sat_rows, &J);
    /* 4) composição: refração na borda, tinta, brilho especular e difuso */
    par_rows(dr.w * 3, dr.h, glass_comp_rows, &J);   /* ~3x o custo de um pixel comum */
}

static void glass_down_rows(void *c, int jb, int je, int cpu) {
    glass_job_t *J = c;
    surface_t *s = J->s;
    rect_t S = J->S;
    for (int j = jb; j < je; j++) {
        int sy0 = S.y + j * 2, sy1 = MIN(sy0 + 1, S.y + S.h - 1);
        const u32 *r0 = s->px + (usize)sy0 * s->stride, *r1 = s->px + (usize)sy1 * s->stride;
        u32 *o = J->b + (usize)j * J->bw;
        for (int i = 0; i < J->bw; i++) {
            int sx0 = S.x + i * 2, sx1 = MIN(sx0 + 1, S.x + S.w - 1);
            o[i] = ((r0[sx0] >> 2) & 0x3F3F3F3F) + ((r0[sx1] >> 2) & 0x3F3F3F3F) +
                   ((r1[sx0] >> 2) & 0x3F3F3F3F) + ((r1[sx1] >> 2) & 0x3F3F3F3F);
        }
    }
}

static void glass_sat_rows(void *c, int jb, int je, int cpu) {
    glass_job_t *J = c;
    int sat = J->g->saturation;
    for (int k = jb * J->bw; k < je * J->bw; k++) {
        u32 p = J->b[k];
        int R = (p >> 16) & 255, G = (p >> 8) & 255, B = p & 255;
        int L = (R * 77 + G * 150 + B * 29) >> 8;
        R = CLAMP(L + (((R - L) * sat) >> 8), 0, 255);
        G = CLAMP(L + (((G - L) * sat) >> 8), 0, 255);
        B = CLAMP(L + (((B - L) * sat) >> 8), 0, 255);
        J->b[k] = 0xFF000000u | ((u32)R << 16) | ((u32)G << 8) | (u32)B;
    }
}

static void glass_comp_rows(void *c, int jb, int je, int cpu) {
    glass_job_t *J = c;
    surface_t *s = J->s;
    const glass_t *g = J->g;
    const u32 *b = J->b;
    int bw = J->bw, bh = J->bh;
    rect_t S = J->S, dr = J->dr;
    float x = J->x, y = J->y, w = J->w, h = J->h, r = J->r;
    float cx = x + w / 2, cy = y + h / 2, hw = w / 2, hh = h / 2;
    u32 tint = color_premul(g->tint);
    int opacity = g->opacity ? g->opacity : 255;
    float bezel = g->bezel > 1 ? g->bezel : 1;
    float rimw = 1.6f;
    for (int j = dr.y + jb; j < dr.y + je; j++) {
        float py = j + 0.5f;
        float vy = (py - y) / h;
        float sheen_k = vy < 0.45f ? (1 - vy / 0.45f) : 0;
        sheen_k *= sheen_k;
        u32 *row = s->px + (usize)j * s->stride;
        for (int i = dr.x; i < dr.x + dr.w; i++) {
            float px = i + 0.5f, nx, ny;
            float d = sd_rrect_n(px - cx, py - cy, hw, hh, r, &nx, &ny);
            int cov = cov_from_d(d);
            if (!cov) continue;
            float t = -d; /* profundidade a partir da borda */
            float spx = px, spy = py, edge = 0, disp = 0;
            if (t < bezel) {
                edge = 1 - (t > 0 ? t : 0) / bezel;
                disp = g->refract * edge * edge;
                spx -= nx * disp;
                spy -= ny * disp;
            }
            float bx = (spx - S.x) * 0.5f - 0.5f, by = (spy - S.y) * 0.5f - 0.5f;
            u32 c = sample_bilinear(b, bw, bh, bx, by);
            if (disp > 0.05f && g->chroma > 0) {
                /* aberração cromática: cada canal refrata um pouco diferente (o azul fica na base);
                 * o deslocamento extra está em px de tela, o buffer é subamostrado 2x */
                float er = disp * GLASS_CHROMA_R * g->chroma * 0.5f, eg = disp * GLASS_CHROMA_G * g->chroma * 0.5f;
                u32 cr = sample_bilinear(b, bw, bh, bx - nx * er, by - ny * er);
                u32 cg = sample_bilinear(b, bw, bh, bx - nx * eg, by - ny * eg);
                c = (cr & 0x00FF0000u) | (cg & 0x0000FF00u) | (c & 0x000000FFu);
            }
            c = blend_px(c | 0xFF000000u, tint);
            float light = 0;
            if (g->rim) {
                float sp = specular(nx, ny);
                if (t < rimw) light += g->rim * sp * (1 - (t > 0 ? t : 0) / rimw);
                light += g->rim * 0.22f * edge * edge * sp;
            }
            light += g->sheen * sheen_k;
            if (light > 0.5f) c = blend_px(c, white_px((u32)light));
            int a = cov * opacity / 255;
            row[i] = a >= 255 ? (c | 0xFF000000u) : blend_px(row[i], scale_px(c | 0xFF000000u, (u32)a));
        }
    }
}

void gfx_specular_rim(surface_t *s, float x, float y, float w, float h, float r, int strength) {
    if (w <= 2 || h <= 2 || strength <= 0) return;
    float maxr = (w < h ? w : h) / 2;
    if (r > maxr) r = maxr;
    x += s->ox; y += s->oy;
    int x0 = (int)ffloor(x), y0 = (int)ffloor(y), x1 = (int)fceil(x + w), y1 = (int)fceil(y + h);
    rect_t dr = rect_intersect(rect_make(x0, y0, x1 - x0, y1 - y0), s->clip);
    float cx = x + w / 2, cy = y + h / 2, hw = w / 2, hh = h / 2;
    for (int j = dr.y; j < dr.y + dr.h; j++) {
        float py = j + 0.5f;
        bool mid = py > y + r + 2 && py < y + h - r - 2;
        u32 *row = s->px + (usize)j * s->stride;
        for (int i = dr.x; i < dr.x + dr.w; i++) {
            float px = i + 0.5f;
            if (mid && px > x + 2.5f && px < x + w - 2.5f) { int ni = (int)(x + w - 2.5f) - 1; if (ni > i) { i = ni; continue; } }
            float nx, ny;
            float d = sd_rrect_n(px - cx, py - cy, hw, hh, r, &nx, &ny);
            int cov = cov_from_d(d);
            if (!cov) continue;
            float t = -d;
            if (t >= 1.6f) continue;
            float l = strength * specular(nx, ny) * (1 - (t > 0 ? t : 0) / 1.6f);
            u32 a = (u32)(l * cov / 255);
            if (a) row[i] = blend_px(row[i], white_px(a));
        }
    }
}

void gfx_mask(surface_t *s, int x, int y, const u8 *mask, int mw, int mh, int mstride, u32 c) {
    x += s->ox; y += s->oy;
    rect_t dr = rect_intersect(rect_make(x, y, mw, mh), s->clip);
    if (rect_empty(dr)) return;
    u32 pm = color_premul(c);
    for (int j = 0; j < dr.h; j++) {
        const u8 *m = mask + (usize)(dr.y - y + j) * mstride + (dr.x - x);
        u32 *dp = s->px + (usize)(dr.y + j) * s->stride + dr.x;
        for (int i = 0; i < dr.w; i++) {
            u32 a = m[i];
            if (a) dp[i] = blend_px(dp[i], a == 255 ? pm : scale_px(pm, a));
        }
    }
}

/* ============================================================ framebuffer */
surface_t *g_screen;

/* ---- monitores
 *
 * "Saídas" são as conexões de vídeo: a 0 é a do framebuffer do UEFI (saída 0 da GPU) e as outras são
 * registradas pelos drivers (display_add). Cada saída tem uma posição livre no arranjo (lx, ly), como no
 * organizador do Mac: à esquerda, à direita, acima, abaixo ou desalinhada. O g_screen é o retângulo que
 * envolve todas, então o monitor principal (barra de menus e Dock) pode estar em qualquer lugar dele.
 * Na API display_*, o monitor 0 é sempre o principal. No modo espelho há um monitor só, mostrado em
 * todas as saídas. O arranjo é salvo em /Sistema/Biblioteca/monitores.conf e volta no próximo boot se
 * os monitores forem os mesmos (mesmos tamanhos). */
#include "vfs.h"

#define LAYOUT_FILE "/Sistema/Biblioteca/monitores.conf"

typedef struct { int w, h, lx, ly; char name[48]; } output_t;
static output_t outp[MAX_DISPLAYS];
static int nout = 1;                         /* a saída 0 sempre existe */
static int primary_out;
static bool mirrored;
static struct { rect_t r; int out; } disp[MAX_DISPLAYS];
static int ndisp;
static int identify_out = -1;

extern void wm_damage(rect_t r);
extern void wm_displays_changed(const rect_t *old_by_out, const rect_t *new_by_out, int n);
extern void desktop_displays_changed(void);

bool display_add(int w, int h, const char *name) {
    if (g_screen || nout >= MAX_DISPLAYS || w < 320 || h < 200 || w > 8192 || h > 8192) return false;
    output_t *o = &outp[nout++];
    memset(o, 0, sizeof *o);
    o->w = w; o->h = h;
    strncpy(o->name, name ? name : "Monitor", sizeof o->name - 1);
    return true;
}

int display_count(void) { return ndisp ? ndisp : 1; }
rect_t display_rect(int i) {
    if (!ndisp) return rect_make(0, 0, g_boot->fb.width, g_boot->fb.height);
    return disp[i >= 0 && i < ndisp ? i : 0].r;
}
const char *display_name(int i) { return ndisp && i >= 0 && i < ndisp ? outp[disp[i].out].name : "Monitor integrado"; }
int display_output_of(int i) { return ndisp && i >= 0 && i < ndisp ? disp[i].out : 0; }
int display_at(int x, int y) {
    for (int i = 0; i < display_count(); i++) if (rect_contains(display_rect(i), x, y)) return i;
    return -1;
}
int display_for(rect_t r) {
    int best = 0;
    i64 area = -1;
    for (int i = 0; i < display_count(); i++) {
        rect_t k = rect_intersect(r, display_rect(i));
        if ((i64)k.w * k.h > area) { area = (i64)k.w * k.h; best = i; }
    }
    return best;
}
void display_clamp(int *x, int *y) {
    if (display_at(*x, *y) >= 0) return;
    /* o monitor mais perto (distância de Manhattan até o retângulo) */
    int best = 0, bd = 0x7FFFFFFF;
    for (int i = 0; i < display_count(); i++) {
        rect_t r = display_rect(i);
        int cx = CLAMP(*x, r.x, r.x + r.w - 1), cy = CLAMP(*y, r.y, r.y + r.h - 1);
        int d = (cx > *x ? cx - *x : *x - cx) + (cy > *y ? cy - *y : *y - cy);
        if (d < bd) { bd = d; best = i; }
    }
    rect_t r = display_rect(best);
    *x = CLAMP(*x, r.x, r.x + r.w - 1);
    *y = CLAMP(*y, r.y, r.y + r.h - 1);
}

/* ---- arranjo */
int display_output_count(void) { return nout; }
bool display_output_info(int out, int *w, int *h, const char **name) {
    if (out < 0 || out >= nout) return false;
    if (w) *w = outp[out].w;
    if (h) *h = outp[out].h;
    if (name) *name = outp[out].name;
    return true;
}
void display_get_layout(display_layout_t *L) {
    memset(L, 0, sizeof *L);
    L->n = nout;
    for (int i = 0; i < nout; i++) { L->x[i] = outp[i].lx; L->y[i] = outp[i].ly; }
    L->primary = primary_out;
    L->mirror = mirrored;
}

/* que pedaço do g_screen a saída mostra (no espelho, todas mostram o principal) */
bool display_scanout(int out, rect_t *r) {
    if (out < 0 || out >= nout) return false;
    if (mirrored) { *r = display_rect(0); return true; }
    for (int i = 0; i < display_count(); i++) if (display_output_of(i) == out) { *r = display_rect(i); return true; }
    return false;
}

static rect_t lrect(const display_layout_t *L, int i) { return rect_make(L->x[i], L->y[i], outp[i].w, outp[i].h); }

/* sem sobreposição e todos ligados por uma borda em comum (como o Mac exige) */
bool display_layout_valid(const display_layout_t *L) {
    if (L->n != nout || L->primary < 0 || L->primary >= nout) return false;
    if (L->mirror || nout == 1) return true;
    for (int i = 0; i < nout; i++)
        for (int j = i + 1; j < nout; j++) if (!rect_empty(rect_intersect(lrect(L, i), lrect(L, j)))) return false;
    bool reach[MAX_DISPLAYS] = {true};
    for (int pass = 0; pass < nout; pass++)
        for (int i = 0; i < nout; i++) {
            if (!reach[i]) continue;
            rect_t a = lrect(L, i);
            for (int j = 0; j < nout; j++) {
                if (reach[j]) continue;
                rect_t b = lrect(L, j);
                bool vert = (a.x + a.w == b.x || b.x + b.w == a.x) && MIN(a.y + a.h, b.y + b.h) - MAX(a.y, b.y) > 0;
                bool horz = (a.y + a.h == b.y || b.y + b.h == a.y) && MIN(a.x + a.w, b.x + b.w) - MAX(a.x, b.x) > 0;
                if (vert || horz) reach[j] = true;
            }
        }
    for (int i = 0; i < nout; i++) if (!reach[i]) return false;
    return true;
}

/* monta disp[] (principal primeiro) e o tamanho do g_screen a partir do arranjo atual */
static void layout_build(int *cw, int *ch) {
    if (mirrored || nout == 1) {
        disp[0].r = rect_make(0, 0, outp[primary_out].w, outp[primary_out].h);
        disp[0].out = primary_out;
        ndisp = 1;
        *cw = disp[0].r.w; *ch = disp[0].r.h;
        return;
    }
    int minx = outp[0].lx, miny = outp[0].ly;
    for (int i = 1; i < nout; i++) { minx = MIN(minx, outp[i].lx); miny = MIN(miny, outp[i].ly); }
    ndisp = 0;
    *cw = *ch = 0;
    for (int k = 0; k < nout; k++) {
        int i = k == 0 ? primary_out : (k <= primary_out ? k - 1 : k);
        disp[ndisp].r = rect_make(outp[i].lx - minx, outp[i].ly - miny, outp[i].w, outp[i].h);
        disp[ndisp].out = i;
        *cw = MAX(*cw, disp[ndisp].r.x + disp[ndisp].r.w);
        *ch = MAX(*ch, disp[ndisp].r.y + disp[ndisp].r.h);
        ndisp++;
    }
}

static void layout_default(void) {
    int x = 0;
    for (int i = 0; i < nout; i++) { outp[i].lx = x; outp[i].ly = 0; x += outp[i].w; }
    primary_out = 0;
    mirrored = false;
}

static void layout_save(void) {
    char buf[512];
    int n = snprintf(buf, sizeof buf, "# arranjo dos monitores (Ajustes > Monitores)\nprincipal %d\nespelhar %d\n", primary_out, mirrored);
    for (int i = 0; i < nout && n < (int)sizeof buf - 64; i++)
        n += snprintf(buf + n, sizeof buf - n, "saida %d %dx%d %d %d\n", i, outp[i].w, outp[i].h, outp[i].lx, outp[i].ly);
    vfs_mkdir("/Sistema/Biblioteca");
    vfs_write_file(LAYOUT_FILE, buf, (usize)n);
}

/* lê o arranjo salvo; só vale se as saídas tiverem os mesmos tamanhos de quando foi salvo */
static void layout_load(void) {
    vnode_t *f = vfs_lookup(LAYOUT_FILE);
    if (!f || f->dir || !f->data || !f->size) return;
    display_layout_t L;
    display_get_layout(&L);
    int seen = 0;
    char line[96];
    for (usize p = 0; p < f->size;) {
        usize k = 0;
        while (p < f->size && f->data[p] != '\n' && k < sizeof line - 1) line[k++] = (char)f->data[p++];
        while (p < f->size && f->data[p] != '\n') p++;
        p++;
        line[k] = 0;
        char *e;
        if (!strncmp(line, "principal ", 10)) L.primary = (int)strtol(line + 10, NULL, 10);
        else if (!strncmp(line, "espelhar ", 9)) L.mirror = strtol(line + 9, NULL, 10) != 0;
        else if (!strncmp(line, "saida ", 6)) {
            int i = (int)strtol(line + 6, &e, 10);
            int w = (int)strtol(e, &e, 10);
            int h = (*e == 'x') ? (int)strtol(e + 1, &e, 10) : 0;
            int x = (int)strtol(e, &e, 10), y = (int)strtol(e, &e, 10);
            if (i < 0 || i >= nout || w != outp[i].w || h != outp[i].h) return;   /* outros monitores: ignora */
            L.x[i] = x; L.y[i] = y;
            seen |= 1 << i;
        }
    }
    if (seen != (1 << nout) - 1 || !display_layout_valid(&L)) return;
    for (int i = 0; i < nout; i++) { outp[i].lx = L.x[i]; outp[i].ly = L.y[i]; }
    primary_out = L.primary;
    mirrored = L.mirror;
}

/* aplica um arranjo novo sem reiniciar: realoca a área de desenho, religa a GPU, leva as janelas junto
 * com o monitor delas e atualiza barra de menus e Dock. false = arranjo inválido ou sem memória. */
bool display_set_layout(const display_layout_t *L) {
    if (!g_screen || !display_layout_valid(L)) return false;
    rect_t old_by_out[MAX_DISPLAYS], new_by_out[MAX_DISPLAYS];
    for (int i = 0; i < nout; i++) if (!display_scanout(i, &old_by_out[i])) old_by_out[i] = display_rect(0);
    output_t saved[MAX_DISPLAYS];
    memcpy(saved, outp, sizeof saved);
    int sp = primary_out;
    bool sm = mirrored;
    /* o principal fica na origem do arranjo, como no Mac (só estética: o g_screen é normalizado) */
    for (int i = 0; i < nout; i++) { outp[i].lx = L->x[i] - L->x[L->primary]; outp[i].ly = L->y[i] - L->y[L->primary]; }
    primary_out = L->primary;
    mirrored = L->mirror && nout > 1;
    int cw, ch;
    layout_build(&cw, &ch);
    if (cw != g_screen->w || ch != g_screen->h) {
        u32 *np = kmalloc((usize)cw * ch * 4);
        if (!np) {
            memcpy(outp, saved, sizeof saved); primary_out = sp; mirrored = sm;
            layout_build(&cw, &ch);
            return false;
        }
        kfree(g_screen->px);
        g_screen->px = np;
        g_screen->w = g_screen->stride = cw;
        g_screen->h = ch;
    }
    surface_reset_clip(g_screen);
    gfx_clear(g_screen, RGB(0, 0, 0));
    for (int i = 0; i < nout; i++) if (!display_scanout(i, &new_by_out[i])) new_by_out[i] = display_rect(0);
    kprintf("monitores: novo arranjo, principal = %s%s, área %dx%d\n", outp[primary_out].name, mirrored ? " (espelho)" : "", cw, ch);
    gpu_displays_changed();
    wm_displays_changed(old_by_out, new_by_out, nout);
    desktop_displays_changed();
    layout_save();
    return true;
}

/* contorno vermelho no monitor físico enquanto ele é arrastado no organizador (como no Mac) */
void display_identify(int out) {
    if (out == identify_out) return;
    rect_t r;
    if (identify_out >= 0 && display_scanout(identify_out, &r)) wm_damage(r);
    identify_out = out;
    if (identify_out >= 0 && display_scanout(identify_out, &r)) wm_damage(r);
}
int display_identifying(void) { return identify_out; }

void fb_init(void) {
    outp[0].w = g_boot->fb.width;
    outp[0].h = g_boot->fb.height;
    strncpy(outp[0].name, nout > 1 ? "Monitor integrado" : "Tela integrada", sizeof outp[0].name - 1);
    layout_default();
    if (nout > 1) layout_load();
    int cw, ch;
    layout_build(&cw, &ch);
    for (int i = 1; i < ndisp; i++)
        kprintf("monitores: %s %dx%d em (%d, %d)\n", display_name(i), disp[i].r.w, disp[i].r.h, disp[i].r.x, disp[i].r.y);
    g_screen = surface_new(cw, ch);
    if (!g_screen || !g_screen->px) {                 /* sem memória para a área estendida: só a saída 0 */
        surface_free(g_screen);
        nout = 1;
        layout_default();
        layout_build(&cw, &ch);
        g_screen = surface_new(cw, ch);
    }
    gfx_clear(g_screen, RGB(0, 0, 0));
}

/* cópia para o framebuffer (memória write-combining): várias faixas em paralelo */
static void flush_rows(void *c, int b, int e, int cpu);
void fb_flush(rect_t r) {
    r = rect_intersect(r, rect_make(0, 0, g_screen->w, g_screen->h));
    if (rect_empty(r)) return;
    /* GPU com fila de comandos (virtio-gpu): a superfície já é a memória do recurso, sem cópia */
    if (g_screen->stride == g_screen->w && gpu_present(g_screen->px, g_screen->w, g_screen->h, r.x, r.y, r.w, r.h)) return;
    /* framebuffer do UEFI: mostra só a saída 0, que pode estar em qualquer lugar do arranjo */
    rect_t o;
    if (!display_scanout(0, &o)) o = rect_make(0, 0, g_boot->fb.width, g_boot->fb.height);
    o.w = MIN(o.w, (int)g_boot->fb.width); o.h = MIN(o.h, (int)g_boot->fb.height);
    rect_t job[2] = { rect_intersect(r, o), o };      /* [0] = área em g_screen, [1] = origem da saída */
    if (rect_empty(job[0])) return;
    gpu_vsync_wait();                       /* Intel: começa no retorno vertical (sem tearing); senão, nada */
    par_rows(job[0].w, job[0].h, flush_rows, job);
}
static void flush_rows(void *c, int b, int e, int cpu) {
    rect_t r = ((rect_t *)c)[0], o = ((rect_t *)c)[1];
    framebuffer_t *fb = &g_boot->fb;
    for (int j = b; j < e; j++) {
        u32 *src = g_screen->px + (usize)(r.y + j) * g_screen->stride + r.x;
        volatile u32 *dst = fb->fb + (usize)(r.y - o.y + j) * fb->pitch + (r.x - o.x);
        if (fb->bgr) {
            memcpy((void *)dst, src, (usize)r.w * 4);
        } else {
            for (int i = 0; i < r.w; i++) {
                u32 p = src[i];
                dst[i] = ((p & 0xFF) << 16) | (p & 0xFF00) | ((p >> 16) & 0xFF);
            }
        }
    }
}
