/* mOS — Canvas 2D do motor web: bitmap, estado, caminhos, pintura (cor, gradientes linear/radial/cônico, padrão),
 * composição (Porter-Duff e modos de mistura), recorte, sombra com desfoque, drawImage com suavização, texto com
 * transformação, ImageData e PNG (deflate próprio). Implementação própria a partir da especificação HTML. */
#include "canvas.h"

#define MAX_CANVAS_SIDE 16384
#define MAX_CANVAS_AREA (8192u * 8192u)

static float fmodp(float a, float b) { float r = a - ffloor(a / b) * b; return r < 0 ? r + b : r; }
static float clampf(float v, float a, float b) { return v < a ? a : v > b ? b : v; }

/* ================================================================ comandos de caminho */
void pc_free(pcmd_t *pc) { if (pc->base) kfree(pc->base); memset(pc, 0, sizeof *pc); }
void pc_reset(pcmd_t *pc) { pc->n = 0; pc->has = false; pc->last = 0; }

static bool pc_push(pcmd_t *pc, int k, const float *v) {
    if (pc->n + k > pc->cap) {
        int nc = pc->cap ? pc->cap * 2 : 64;
        while (nc < pc->n + k) nc *= 2;
        if (nc > (1 << 24)) return false;
        float *nb = krealloc(pc->base, sizeof(float) * (PC_HDR + nc));
        if (!nb) return false;
        if (!pc->base) memset(nb, 0, sizeof(float) * PC_HDR);
        pc->base = nb;
        pc->cap = nc;
    }
    memcpy(pc_v(pc) + pc->n, v, sizeof(float) * k);
    pc->last = pc->n;
    pc->n += k;
    return true;
}

bool pc_move(pcmd_t *pc, const mat_t *m, float x, float y) {
    float v[3] = { PC_MOVE, 0, 0 };
    mapp(m, x, y, &v[1], &v[2]);
    /* MOVE seguido de MOVE: o primeiro não deixa rastro */
    if (pc->n >= 3 && pc->last == pc->n - 3 && pc_v(pc)[pc->last] == PC_MOVE) pc->n -= 3;
    if (!pc_push(pc, 3, v)) return false;
    pc->cx = pc->sx = v[1];
    pc->cy = pc->sy = v[2];
    pc->has = true;
    return true;
}

bool pc_line(pcmd_t *pc, const mat_t *m, float x, float y) {
    if (!pc->has) return pc_move(pc, m, x, y);
    float v[3] = { PC_LINE, 0, 0 };
    mapp(m, x, y, &v[1], &v[2]);
    if (!pc_push(pc, 3, v)) return false;
    pc->cx = v[1]; pc->cy = v[2];
    return true;
}

static bool pc_cubic_dev(pcmd_t *pc, float x1, float y1, float x2, float y2, float x3, float y3) {
    float v[7] = { PC_CUBIC, x1, y1, x2, y2, x3, y3 };
    if (!pc_push(pc, 7, v)) return false;
    pc->cx = x3; pc->cy = y3;
    return true;
}

bool pc_cubic(pcmd_t *pc, const mat_t *m, float x1, float y1, float x2, float y2, float x, float y) {
    if (!pc->has) pc_move(pc, m, x1, y1);
    float a, b, c, d, e, f;
    mapp(m, x1, y1, &a, &b); mapp(m, x2, y2, &c, &d); mapp(m, x, y, &e, &f);
    return pc_cubic_dev(pc, a, b, c, d, e, f);
}

bool pc_quad(pcmd_t *pc, const mat_t *m, float cpx, float cpy, float x, float y) {
    if (!pc->has) pc_move(pc, m, cpx, cpy);
    float qx, qy, ex, ey;
    mapp(m, cpx, cpy, &qx, &qy); mapp(m, x, y, &ex, &ey);
    float x0 = pc->cx, y0 = pc->cy;
    return pc_cubic_dev(pc, x0 + 2.0f / 3 * (qx - x0), y0 + 2.0f / 3 * (qy - y0), ex + 2.0f / 3 * (qx - ex), ey + 2.0f / 3 * (qy - ey), ex, ey);
}

void pc_close(pcmd_t *pc) {
    if (!pc->has || !pc->n) return;
    if (pc->last == pc->n - 1 && pc_v(pc)[pc->last] == PC_CLOSE) return;
    float v = PC_CLOSE;
    if (!pc_push(pc, 1, &v)) return;
    pc->cx = pc->sx; pc->cy = pc->sy;
}

bool pc_rect(pcmd_t *pc, const mat_t *m, float x, float y, float w, float h) {
    pc_move(pc, m, x, y);
    pc_line(pc, m, x + w, y);
    pc_line(pc, m, x + w, y + h);
    pc_line(pc, m, x, y + h);
    pc_close(pc);
    return true;
}

/* arco elíptico de a0 com varredura sw (rad), centro (cx,cy), raios e rotação: em cúbicas de até 90° */
static void arc_seg(pcmd_t *pc, const mat_t *m, float cx, float cy, float rx, float ry, float rot, float a0, float sw, bool join) {
    float cr = fcos(rot), sr = fsin(rot);
#define EP(a, X, Y) do { float ca_ = fcos(a), sa_ = fsin(a); X = cx + rx * ca_ * cr - ry * sa_ * sr; Y = cy + rx * ca_ * sr + ry * sa_ * cr; } while (0)
#define ED(a, X, Y) do { float ca_ = fcos(a), sa_ = fsin(a); X = -rx * sa_ * cr - ry * ca_ * sr; Y = -rx * sa_ * sr + ry * ca_ * cr; } while (0)
    float px, py;
    EP(a0, px, py);
    if (join && pc->has) pc_line(pc, m, px, py); else pc_move(pc, m, px, py);
    if (sw == 0) return;
    int segs = (int)(fabs_(sw) / (PI_F / 2) + 0.9999f);
    if (segs < 1) segs = 1;
    float step = sw / segs, k = 4.0f / 3.0f * fsin(step / 4) / fcos(step / 4);
    for (int i = 0; i < segs; i++) {
        float t0 = a0 + step * i, t1 = t0 + step;
        float x0, y0, x3, y3, d0x, d0y, d1x, d1y;
        EP(t0, x0, y0); EP(t1, x3, y3); ED(t0, d0x, d0y); ED(t1, d1x, d1y);
        pc_cubic(pc, m, x0 + k * d0x, y0 + k * d0y, x3 - k * d1x, y3 - k * d1y, x3, y3);
    }
#undef EP
#undef ED
}

bool pc_ellipse(pcmd_t *pc, const mat_t *m, float x, float y, float rx, float ry, float rot, float a0, float a1, bool ccw) {
    float tau = 2 * PI_F, sw;
    if (!ccw) sw = a1 - a0 >= tau ? tau : fmodp(a1 - a0, tau);
    else sw = a0 - a1 >= tau ? -tau : -fmodp(a0 - a1, tau);
    arc_seg(pc, m, x, y, rx, ry, rot, a0, sw, true);
    return true;
}

bool pc_arc_to(pcmd_t *pc, const mat_t *m, float x1, float y1, float x2, float y2, float r) {
    if (!pc->has) return pc_move(pc, m, x1, y1);
    mat_t inv;
    if (!minv(*m, &inv)) return pc_line(pc, m, x1, y1);
    float x0, y0;
    mapp(&inv, pc->cx, pc->cy, &x0, &y0);
    float d1x = x1 - x0, d1y = y1 - y0, d2x = x2 - x1, d2y = y2 - y1;
    float l1 = sqrtf_(d1x * d1x + d1y * d1y), l2 = sqrtf_(d2x * d2x + d2y * d2y);
    float cross = d1x * d2y - d1y * d2x;
    if (r == 0 || l1 < 1e-6f || l2 < 1e-6f || fabs_(cross) < 1e-6f * l1 * l2) return pc_line(pc, m, x1, y1);
    float v1x = -d1x / l1, v1y = -d1y / l1, v2x = d2x / l2, v2y = d2y / l2;   /* de p1 para p0 e para p2 */
    float cosT = clampf(v1x * v2x + v1y * v2y, -1, 1);
    float half_tan = sqrtf_((1 - cosT) / (1 + cosT));                        /* tan(θ/2) */
    float dist = r / half_tan;
    float t1x = x1 + v1x * dist, t1y = y1 + v1y * dist, t2x = x1 + v2x * dist, t2y = y1 + v2y * dist;
    float bx = v1x + v2x, by = v1y + v2y, bl = sqrtf_(bx * bx + by * by);
    float hyp = sqrtf_(dist * dist + r * r);
    float ccx = x1 + bx / bl * hyp, ccy = y1 + by / bl * hyp;
    bool ccw = cross < 0;
    float a0 = atan2_(t1y - ccy, t1x - ccx), a1 = atan2_(t2y - ccy, t2x - ccx);
    pc_line(pc, m, t1x, t1y);
    float sw = ccw ? -fmodp(a0 - a1, 2 * PI_F) : fmodp(a1 - a0, 2 * PI_F);
    arc_seg(pc, m, ccx, ccy, r, r, 0, a0, sw, true);
    return true;
}

bool pc_round_rect(pcmd_t *pc, const mat_t *m, float x, float y, float w, float h, const float *rr) {
    float r[8];
    memcpy(r, rr, sizeof r);
    /* retângulos com largura/altura negativas: espelha os cantos */
    if (w < 0) { x += w; w = -w; float t[8] = { r[2], r[3], r[0], r[1], r[6], r[7], r[4], r[5] }; memcpy(r, t, sizeof r); }
    if (h < 0) { y += h; h = -h; float t[8] = { r[6], r[7], r[4], r[5], r[2], r[3], r[0], r[1] }; memcpy(r, t, sizeof r); }
    float sc = 1;
    float top = r[0] + r[2], right = r[3] + r[5], bot = r[4] + r[6], left = r[7] + r[1];
    if (top > w && top > 0) sc = MIN(sc, w / top);
    if (bot > w && bot > 0) sc = MIN(sc, w / bot);
    if (left > h && left > 0) sc = MIN(sc, h / left);
    if (right > h && right > 0) sc = MIN(sc, h / right);
    for (int i = 0; i < 8; i++) r[i] *= sc;
    float hp = PI_F / 2;
    pc_move(pc, m, x + r[0], y);
    pc_line(pc, m, x + w - r[2], y);
    if (r[2] > 0 && r[3] > 0) arc_seg(pc, m, x + w - r[2], y + r[3], r[2], r[3], 0, -hp, hp, true);
    pc_line(pc, m, x + w, y + h - r[5]);
    if (r[4] > 0 && r[5] > 0) arc_seg(pc, m, x + w - r[4], y + h - r[5], r[4], r[5], 0, 0, hp, true);
    pc_line(pc, m, x + r[6], y + h);
    if (r[6] > 0 && r[7] > 0) arc_seg(pc, m, x + r[6], y + h - r[7], r[6], r[7], 0, hp, hp, true);
    pc_line(pc, m, x, y + r[1]);
    if (r[0] > 0 && r[1] > 0) arc_seg(pc, m, x + r[0], y + r[1], r[0], r[1], 0, PI_F, hp, true);
    pc_close(pc);
    return true;
}

bool pc_append(pcmd_t *pc, const pcmd_t *src, const mat_t *m) {
    const float *v = src->base ? pc_v(src) : NULL;
    for (int i = 0; i < src->n;) {
        int op = (int)v[i];
        if (op == PC_MOVE) { pc_move(pc, m, v[i + 1], v[i + 2]); i += 3; }
        else if (op == PC_LINE) { pc_line(pc, m, v[i + 1], v[i + 2]); i += 3; }
        else if (op == PC_CUBIC) { pc_cubic(pc, m, v[i + 1], v[i + 2], v[i + 3], v[i + 4], v[i + 5], v[i + 6]); i += 7; }
        else { pc_close(pc); i += 1; }
    }
    return true;
}

void pc_build(const pcmd_t *pc, const mat_t *m, float tol, bool drop_empty, path_t *out) {
    memset(out, 0, sizeof *out);
    out->tol = tol > 1e-4f ? tol : 1e-4f;
    if (!pc->base) return;
    const float *v = pc_v(pc);
    bool after_close = false;
    float sx = 0, sy = 0;
    for (int i = 0; i < pc->n;) {
        int op = (int)v[i];
        float a, b;
        if (op == PC_MOVE) {
            mapp(m, v[i + 1], v[i + 2], &a, &b);
            rs_path_move(out, a, b);
            sx = a; sy = b;
            after_close = false;
            i += 3;
        } else if (op == PC_LINE || op == PC_CUBIC) {
            if (after_close) { rs_path_move(out, sx, sy); after_close = false; }
            if (op == PC_LINE) { mapp(m, v[i + 1], v[i + 2], &a, &b); rs_path_line(out, a, b); i += 3; }
            else {
                float c, d, e, f;
                mapp(m, v[i + 1], v[i + 2], &a, &b); mapp(m, v[i + 3], v[i + 4], &c, &d); mapp(m, v[i + 5], v[i + 6], &e, &f);
                rs_path_cubic(out, a, b, c, d, e, f);
                i += 7;
            }
        } else { rs_path_close(out); after_close = true; i += 1; }
    }
    if (!drop_empty || !out->nc) return;
    /* tira subcaminhos de um ponto só (e de comprimento zero): o traço os ignora */
    int w = 0, nw = 0;
    for (int c = 0; c < out->nc; c++) {
        int s = out->start[c], n = rs_cont_len(out, c);
        bool empty = true;
        for (int i = 1; i < n && empty; i++)
            if (out->xy[2 * (s + i)] != out->xy[2 * s] || out->xy[2 * (s + i) + 1] != out->xy[2 * s + 1]) empty = false;
        if (empty) continue;
        memmove(out->xy + 2 * w, out->xy + 2 * s, sizeof(float) * 2 * n);
        out->start[nw] = w;
        out->closed[nw] = out->closed[c];
        nw++;
        w += n;
    }
    out->n = w;
    out->nc = nw;
}

/* ================================================================ bitmap e estado */
void canvas_size_attrs(node_t *n, int *w, int *h) {
    const char *names[2] = { "width", "height" };
    int def[2] = { 300, 150 }, *out[2] = { w, h };
    for (int i = 0; i < 2; i++) {
        const char *v = node_attr(n, names[i]);
        *out[i] = def[i];
        if (!v) continue;
        while (*v == ' ' || *v == '\t' || *v == '\n' || *v == '\r' || *v == '\f') v++;
        if (*v == '+') v++;
        if (!isdigit((u8)*v)) continue;
        long x = 0;
        while (isdigit((u8)*v)) { x = x * 10 + (*v++ - '0'); if (x > 1000000) x = 1000000; }
        *out[i] = (int)x;
    }
}

void canvas_state_init(cstate_t *st) {
    memset(st, 0, sizeof *st);
    st->m = MID;
    st->alpha = 1;
    st->op = OP_SRC_OVER;
    st->fill.kind = st->stroke.kind = PS_COLOR;
    st->fill.color = st->stroke.color = 0xFF000000u;
    st->lw = 1;
    st->miter = 10;
    st->cap = CAP_BUTT;
    st->join = JOIN_MITER;
    strcpy(st->font, "10px sans-serif");
    st->fsize = 10;
    st->fweight = 400;
    st->align = CTA_START;
    st->baseline = TB_ALPHABETIC;
    st->smooth = true;
}

void canvas_state_free(cstate_t *st) {
    if (st->dash) kfree(st->dash);
    if (st->clip) kfree(st->clip);
    st->dash = NULL;
    st->clip = NULL;
}

void canvas_state_copy(cstate_t *dst, const cstate_t *src, int W, int H) {
    *dst = *src;
    dst->dash = NULL;
    dst->clip = NULL;
    if (src->dash && src->ndash) {
        dst->dash = kmalloc(sizeof(float) * src->ndash);
        if (dst->dash) memcpy(dst->dash, src->dash, sizeof(float) * src->ndash); else dst->ndash = 0;
    }
    if (src->clip && W > 0 && H > 0) {
        dst->clip = kmalloc((usize)W * H);
        if (dst->clip) memcpy(dst->clip, src->clip, (usize)W * H);
    }
}

bool canvas_save(canvas2d_t *c) {
    if (c->nstack >= 1024) return false;
    if (c->nstack == c->cstack) {
        int nc = c->cstack ? c->cstack * 2 : 8;
        cstate_t *ns = krealloc(c->stack, sizeof(cstate_t) * nc);
        if (!ns) return false;
        c->stack = ns;
        c->cstack = nc;
    }
    canvas_state_copy(&c->stack[c->nstack++], &c->st, c->w, c->h);
    return true;
}

bool canvas_restore(canvas2d_t *c) {
    if (!c->nstack) return false;
    canvas_state_free(&c->st);
    c->st = c->stack[--c->nstack];
    return true;
}

static void canvas_reset_state(canvas2d_t *c) {
    canvas_state_free(&c->st);
    while (c->nstack) canvas_state_free(&c->stack[--c->nstack]);
    canvas_state_init(&c->st);
    pc_reset(&c->path);
}

void canvas_resize(canvas2d_t *c, int w, int h) {
    surface_free(c->s);
    c->s = NULL;
    canvas_reset_state(c);
    if (w > MAX_CANVAS_SIDE || h > MAX_CANVAS_SIDE || (u64)w * h > MAX_CANVAS_AREA) w = h = 0;
    c->w = w;
    c->h = h;
    if (w > 0 && h > 0) {
        c->s = surface_new(w, h);
        if (c->s && !c->s->px) { kfree(c->s); c->s = NULL; }
        if (c->s) {
            u32 bg = c->opaque ? 0xFF000000u : 0;
            for (usize i = 0; i < (usize)w * h; i++) c->s->px[i] = bg;
        } else c->w = c->h = 0;
    }
}

canvas2d_t *canvas_of(doc_t *d, node_t *n, bool create) {
    if (!n || n->type != N_ELEM || n->tag != T_CANVAS) return NULL;
    if (n->canvas || !create) return n->canvas;
    canvas2d_t *c = kcalloc(1, sizeof *c);
    if (!c) return NULL;
    c->node = n;
    canvas_state_init(&c->st);
    int w, h;
    canvas_size_attrs(n, &w, &h);
    canvas_resize(c, w, h);
    c->next = d->canvases;
    d->canvases = c;
    n->canvas = c;
    return c;
}

void canvas_intrinsic(node_t *n, int *w, int *h) {
    if (n->canvas) { *w = n->canvas->w; *h = n->canvas->h; return; }
    canvas_size_attrs(n, w, h);
}

surface_t *canvas_surface(node_t *n) { return n->canvas ? n->canvas->s : NULL; }

void canvas_free_all(doc_t *d) {
    for (canvas2d_t *c = d->canvases, *nx; c; c = nx) {
        nx = c->next;
        canvas_reset_state(c);
        canvas_state_free(&c->st);
        if (c->stack) kfree(c->stack);
        pc_free(&c->path);
        surface_free(c->s);
        if (c->node) c->node->canvas = NULL;
        kfree(c);
    }
    d->canvases = NULL;
}

/* ================================================================ fontes de pintura */
typedef struct {
    int kind;
    u32 color;            /* PS_COLOR: pré-multiplicado */
    u32 lut[256];
    mat_t inv;            /* pixel do canvas → espaço do gradiente/padrão */
    const cgrad_t *g;
    const cpat_t *p;
    bool smooth;
} psrc_t;

static u32 lerp_argb(u32 a, u32 b, float f) {
    u32 c = 0;
    for (int sh = 0; sh < 32; sh += 8) {
        int va = (int)((a >> sh) & 255), vb = (int)((b >> sh) & 255);
        c |= (u32)(va + (vb - va) * f + 0.5f) << sh;
    }
    return c;
}

static bool psrc_make(canvas2d_t *c, const cpaint_t *cp, psrc_t *P) {
    memset(P, 0, sizeof *P);
    P->kind = cp->kind;
    if (cp->kind == PS_COLOR) {
        P->color = premul(cp->color, 1.0f);
        return true;
    }
    if (!cp->obj) return false;
    if (cp->kind == PS_PATTERN) {
        P->p = cp->obj;
        if (!P->p->w || !P->p->h) return false;
        if (!minv(mmul(c->st.m, P->p->m), &P->inv)) return false;
        P->smooth = c->st.smooth;
        return true;
    }
    const cgrad_t *g = cp->obj;
    P->g = g;
    if (!g->nstops) return false;
    if (g->kind == PS_LINEAR && g->x0 == g->x1 && g->y0 == g->y1) return false;
    if (g->kind == PS_RADIAL && g->x0 == g->x1 && g->y0 == g->y1 && g->r0 == g->r1) return false;
    if (!minv(c->st.m, &P->inv)) return false;
    for (int i = 0; i < 256; i++) {
        float t = i / 255.0f;
        int k = 0;
        while (k < g->nstops - 1 && g->stops[k + 1].off <= t) k++;
        u32 col;
        if (t <= g->stops[0].off) col = g->stops[0].c;
        else if (k >= g->nstops - 1) col = g->stops[g->nstops - 1].c;
        else {
            float span = g->stops[k + 1].off - g->stops[k].off;
            col = lerp_argb(g->stops[k].c, g->stops[k + 1].c, span > 0 ? (t - g->stops[k].off) / span : 1);
        }
        P->lut[i] = premul(col, 1.0f);
    }
    return true;
}

static inline u32 pat_px(const cpat_t *p, int x, int y) {
    int w = p->w, h = p->h;
    if (p->rep == REP_REPEAT || p->rep == REP_X) { x %= w; if (x < 0) x += w; }
    else if (x < 0 || x >= w) return 0;
    if (p->rep == REP_REPEAT || p->rep == REP_Y) { y %= h; if (y < 0) y += h; }
    else if (y < 0 || y >= h) return 0;
    return p->px[(usize)y * w + x];
}

static u32 bilerp(u32 a, u32 b, u32 c, u32 d, u32 fx, u32 fy) {   /* fx, fy em 0..256 */
    u32 out = 0;
    for (int sh = 0; sh < 32; sh += 8) {
        u32 ta = (a >> sh) & 255, tb = (b >> sh) & 255, tc = (c >> sh) & 255, td = (d >> sh) & 255;
        u32 top = ta * (256 - fx) + tb * fx, bot = tc * (256 - fx) + td * fx;
        u32 v = (top * (256 - fy) + bot * fy + 32768) >> 16;
        out |= (v > 255 ? 255 : v) << sh;
    }
    return out;
}

static u32 psrc_at(const psrc_t *P, float px, float py) {
    if (P->kind == PS_COLOR) return P->color;
    float x, y;
    mapp(&P->inv, px, py, &x, &y);
    if (P->kind == PS_PATTERN) {
        if (!P->smooth) return pat_px(P->p, (int)ffloor(x), (int)ffloor(y));
        x -= 0.5f; y -= 0.5f;
        int ix = (int)ffloor(x), iy = (int)ffloor(y);
        u32 fx = (u32)((x - ix) * 256), fy = (u32)((y - iy) * 256);
        return bilerp(pat_px(P->p, ix, iy), pat_px(P->p, ix + 1, iy), pat_px(P->p, ix, iy + 1), pat_px(P->p, ix + 1, iy + 1), fx, fy);
    }
    const cgrad_t *g = P->g;
    float t;
    if (P->kind == PS_LINEAR) {
        float dx = g->x1 - g->x0, dy = g->y1 - g->y0;
        t = ((x - g->x0) * dx + (y - g->y0) * dy) / (dx * dx + dy * dy);
    } else if (P->kind == PS_CONIC) {
        t = (atan2_(y - g->y0, x - g->x0) - g->ang) / (2 * PI_F);
        t -= ffloor(t);
    } else {
        /* dois círculos: maior ω com r(ω) ≥ 0 e o ponto no círculo ω */
        float cdx = g->x1 - g->x0, cdy = g->y1 - g->y0, dr = g->r1 - g->r0;
        float pdx = x - g->x0, pdy = y - g->y0;
        float a = cdx * cdx + cdy * cdy - dr * dr;
        float b = pdx * cdx + pdy * cdy + g->r0 * dr;
        float c = pdx * pdx + pdy * pdy - g->r0 * g->r0;
        if (fabs_(a) < 1e-6f) {
            if (fabs_(b) < 1e-9f) return 0;
            t = c / (2 * b);
            if (g->r0 + t * dr < 0) return 0;
        } else {
            float disc = b * b - a * c;
            if (disc < 0) return 0;
            float sq = sqrtf_(disc);
            float t1 = (b + sq) / a, t2 = (b - sq) / a;
            float hi = t1 > t2 ? t1 : t2, lo = t1 > t2 ? t2 : t1;
            if (g->r0 + hi * dr >= 0) t = hi;
            else if (g->r0 + lo * dr >= 0) t = lo;
            else return 0;
        }
    }
    t = clampf(t, 0, 1);
    return P->lut[(int)(t * 255 + 0.5f)];
}

/* ================================================================ camadas e composição */
typedef struct { int x, y, w, h; u32 *px; } layer_t;   /* pré-multiplicado, cobertura já aplicada */

const char *const canvas_op_names[OP_COUNT] = {
    "source-over", "source-in", "source-out", "source-atop", "destination-over", "destination-in", "destination-out",
    "destination-atop", "lighter", "copy", "xor", "multiply", "screen", "overlay", "darken", "lighten", "color-dodge",
    "color-burn", "hard-light", "soft-light", "difference", "exclusion", "hue", "saturation", "color", "luminosity" };

static bool op_unbounded(int op) { return op == OP_SRC_IN || op == OP_SRC_OUT || op == OP_DST_IN || op == OP_DST_ATOP || op == OP_COPY; }

static inline u32 pd(u32 s, u32 d, u32 fa, u32 fb) {   /* s·Fa + d·Fb, Fa/Fb em 0..255, com saturação */
    u32 out = 0;
    for (int sh = 0; sh < 32; sh += 8) {
        u32 v = (((s >> sh) & 255) * fa + ((d >> sh) & 255) * fb + 127) / 255;
        out |= (v > 255 ? 255 : v) << sh;
    }
    return out;
}

static float lum3(const float *c) { return 0.3f * c[0] + 0.59f * c[1] + 0.11f * c[2]; }
static void clip_color(float *c) {
    float l = lum3(c), n = MIN(c[0], MIN(c[1], c[2])), x = MAX(c[0], MAX(c[1], c[2]));
    for (int i = 0; i < 3; i++) {
        if (n < 0 && l - n > 1e-9f) c[i] = l + (c[i] - l) * l / (l - n);
        if (x > 1 && x - l > 1e-9f) c[i] = l + (c[i] - l) * (1 - l) / (x - l);
    }
}
static void set_lum(float *c, float l) { float d = l - lum3(c); for (int i = 0; i < 3; i++) c[i] += d; clip_color(c); }
static float sat3(const float *c) { return MAX(c[0], MAX(c[1], c[2])) - MIN(c[0], MIN(c[1], c[2])); }
static void set_sat(float *c, float s) {
    int mx = 0, mn = 0;
    for (int i = 1; i < 3; i++) { if (c[i] > c[mx]) mx = i; if (c[i] < c[mn]) mn = i; }
    if (mx == mn) { c[0] = c[1] = c[2] = 0; return; }
    int md = 3 - mx - mn;
    c[md] = (c[md] - c[mn]) * s / (c[mx] - c[mn]);
    c[mx] = s;
    c[mn] = 0;
}

static float blend1(int op, float b, float s) {   /* B(Cb, Cs) dos modos separáveis */
    switch (op) {
    case OP_MULTIPLY: return b * s;
    case OP_SCREEN: return b + s - b * s;
    case OP_OVERLAY: return b <= 0.5f ? 2 * b * s : 1 - 2 * (1 - b) * (1 - s);   /* hard-light com os papéis trocados */
    case OP_DARKEN: return MIN(b, s);
    case OP_LIGHTEN: return MAX(b, s);
    case OP_DODGE: return b == 0 ? 0 : s >= 1 ? 1 : MIN(1, b / (1 - s));
    case OP_BURN: return b >= 1 ? 1 : s <= 0 ? 0 : 1 - MIN(1, (1 - b) / s);
    case OP_HARD: return s <= 0.5f ? 2 * b * s : 1 - 2 * (1 - b) * (1 - s);
    case OP_SOFT: {
        if (s <= 0.5f) return b - (1 - 2 * s) * b * (1 - b);
        float dd = b <= 0.25f ? ((16 * b - 12) * b + 4) * b : sqrtf_(b);
        return b + (2 * s - 1) * (dd - b);
    }
    case OP_DIFF: return fabs_(b - s);
    case OP_EXCL: return b + s - 2 * b * s;
    }
    return s;
}

static u32 blend_px(int op, u32 s, u32 d) {
    float sa = (s >> 24) / 255.0f, da = (d >> 24) / 255.0f;
    if (sa == 0) return d;
    float sc[3], dc[3], r[3];
    for (int i = 0; i < 3; i++) {
        int sh = 16 - 8 * i;
        sc[i] = ((s >> sh) & 255) / 255.0f / sa;
        dc[i] = da > 0 ? ((d >> sh) & 255) / 255.0f / da : 0;
    }
    if (op >= OP_HUE) {
        float t[3];
        if (op == OP_HUE) { memcpy(t, sc, sizeof t); set_sat(t, sat3(dc)); set_lum(t, lum3(dc)); }
        else if (op == OP_SAT) { memcpy(t, dc, sizeof t); set_sat(t, sat3(sc)); set_lum(t, lum3(dc)); }
        else if (op == OP_COLOR) { memcpy(t, sc, sizeof t); set_lum(t, lum3(dc)); }
        else { memcpy(t, dc, sizeof t); set_lum(t, lum3(sc)); }
        memcpy(r, t, sizeof r);
    } else for (int i = 0; i < 3; i++) r[i] = blend1(op, dc[i], clampf(sc[i], 0, 1));
    float ra = sa + da - sa * da;
    u32 out = (u32)(clampf(ra, 0, 1) * 255 + 0.5f) << 24;
    for (int i = 0; i < 3; i++) {
        float v = sa * (1 - da) * sc[i] + da * (1 - sa) * dc[i] + sa * da * clampf(r[i], 0, 1);
        out |= (u32)(clampf(v, 0, ra) * 255 + 0.5f) << (16 - 8 * i);
    }
    return out;
}

static inline u32 apply_op(int op, u32 s, u32 d) {
    u32 sa = s >> 24, da = d >> 24;
    switch (op) {
    case OP_SRC_OVER: return over(d, s);
    case OP_SRC_IN: return mul_px(s, da);
    case OP_SRC_OUT: return mul_px(s, 255 - da);
    case OP_SRC_ATOP: return pd(s, d, da, 255 - sa);
    case OP_DST_OVER: return over(s, d);
    case OP_DST_IN: return mul_px(d, sa);
    case OP_DST_OUT: return mul_px(d, 255 - sa);
    case OP_DST_ATOP: return pd(s, d, 255 - da, sa);
    case OP_LIGHTER: return pd(s, d, 255, 255);
    case OP_COPY: return s;
    case OP_XOR: return pd(s, d, 255 - da, 255 - sa);
    default: return blend_px(op, s, d);
    }
}

static inline u32 lerp_px(u32 d, u32 r, u32 c) {   /* d + (r − d)·c/255, por canal */
    if (c == 255) return r;
    return mul_px(r, c) + mul_px(d, 255 - c);
}

static void composite(canvas2d_t *c, const layer_t *L, int op, float alpha) {
    surface_t *s = c->s;
    u32 ga = (u32)(clampf(alpha, 0, 1) * 255 + 0.5f);
    const u8 *clip = c->st.clip;
    bool unb = op_unbounded(op);
    int x0 = unb ? 0 : MAX(L->x, 0), y0 = unb ? 0 : MAX(L->y, 0);
    int x1 = unb ? c->w : MIN(L->x + L->w, c->w), y1 = unb ? c->h : MIN(L->y + L->h, c->h);
    for (int y = y0; y < y1; y++) {
        u32 *dr = s->px + (usize)y * s->stride;
        const u8 *cr = clip ? clip + (usize)y * c->w : NULL;
        bool in_row = y >= L->y && y < L->y + L->h;
        const u32 *lr = in_row ? L->px + (usize)(y - L->y) * L->w - L->x : NULL;
        for (int x = x0; x < x1; x++) {
            u32 sp = in_row && x >= L->x && x < L->x + L->w ? lr[x] : 0;
            if (ga != 255) sp = mul_px(sp, ga);
            u32 cv = cr ? cr[x] : 255;
            if (!cv) continue;
            if (!sp && !unb) continue;
            u32 d = dr[x];
            u32 r = op == OP_SRC_OVER ? over(d, sp) : apply_op(op, sp, d);
            dr[x] = cv == 255 ? r : lerp_px(d, r, cv);
        }
    }
    if (c->opaque) for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) s->px[(usize)y * s->stride + x] |= 0xFF000000u;
}

static bool shadow_on(const cstate_t *st) { return (st->shcolor >> 24) && (st->shblur > 0 || st->shx != 0 || st->shy != 0); }

/* margem além do canvas que a sombra pode trazer para dentro */
static void shadow_margin(const cstate_t *st, int *mx, int *my) {
    if (!shadow_on(st)) { *mx = *my = 0; return; }
    int r = (int)fceil(st->shblur * 1.5f) + 2;
    *mx = (int)fceil(fabs_(st->shx)) + r;
    *my = (int)fceil(fabs_(st->shy)) + r;
}

static void box_blur(u8 *a, u8 *tmp, int w, int h, int r, bool horiz) {
    int n = horiz ? w : h, m = horiz ? h : w;
    int div = 2 * r + 1;
    for (int j = 0; j < m; j++) {
        u8 *src = horiz ? a + (usize)j * w : a + j;
        u8 *dst = horiz ? tmp + (usize)j * w : tmp + j;
        int step = horiz ? 1 : w;
        int acc = 0;
        for (int i = -r; i <= r; i++) if (i >= 0 && i < n) acc += src[i * step];
        for (int i = 0; i < n; i++) {
            dst[i * step] = (u8)((acc + div / 2) / div);
            int add = i + r + 1, sub = i - r;
            if (add < n) acc += src[add * step];
            if (sub >= 0) acc -= src[sub * step];
        }
    }
    memcpy(a, tmp, (usize)w * h);
}

static void draw_layer(canvas2d_t *c, layer_t *L) {
    cstate_t *st = &c->st;
    if (shadow_on(st) && L->w > 0 && L->h > 0) {
        float sigma = st->shblur / 2;
        int boxes[3] = { 0, 0, 0 };
        int pad = 0;
        if (sigma > 0) {
            /* três caixas que aproximam a gaussiana */
            float wi = sqrtf_(12 * sigma * sigma / 3 + 1);
            int wl = (int)ffloor(wi);
            if (!(wl & 1)) wl--;
            int wu = wl + 2;
            float mi = (12 * sigma * sigma - 3 * wl * wl - 12 * wl - 9) / (-4.0f * wl - 4);
            int m = (int)(mi + (mi >= 0 ? 0.5f : -0.5f));
            for (int i = 0; i < 3; i++) { boxes[i] = ((i < m ? wl : wu) - 1) / 2; pad += boxes[i]; }
        }
        int sw = L->w + 2 * pad, sh = L->h + 2 * pad;
        u8 *a = kcalloc((usize)sw * sh, 1), *tmp = kmalloc((usize)sw * sh);
        u32 *spx = kmalloc((usize)sw * sh * 4);
        if (a && tmp && spx) {
            for (int y = 0; y < L->h; y++)
                for (int x = 0; x < L->w; x++) a[(usize)(y + pad) * sw + x + pad] = (u8)(L->px[(usize)y * L->w + x] >> 24);
            for (int i = 0; i < 3; i++) if (boxes[i] > 0) { box_blur(a, tmp, sw, sh, boxes[i], true); box_blur(a, tmp, sw, sh, boxes[i], false); }
            u32 col = premul(st->shcolor, 1.0f);
            for (usize i = 0; i < (usize)sw * sh; i++) spx[i] = mul_px(col, a[i]);
            layer_t S = { L->x - pad + (int)ffloor(st->shx + 0.5f), L->y - pad + (int)ffloor(st->shy + 0.5f), sw, sh, spx };
            composite(c, &S, st->op, st->alpha);
        }
        kfree(a); kfree(tmp); kfree(spx);
    }
    composite(c, L, st->op, st->alpha);
}

/* máscara de cobertura → camada pintada */
static void paint_mask(canvas2d_t *c, const mask_t *M, const psrc_t *P) {
    if (!M->a) return;
    layer_t L = { M->x, M->y, M->w, M->h, kmalloc((usize)M->w * M->h * 4) };
    if (!L.px) return;
    for (int y = 0; y < M->h; y++) {
        const u8 *mr = M->a + (usize)y * M->w;
        u32 *lr = L.px + (usize)y * M->w;
        for (int x = 0; x < M->w; x++) {
            u32 m = mr[x];
            lr[x] = m ? mul_px(psrc_at(P, M->x + x + 0.5f, M->y + y + 0.5f), m) : 0;
        }
    }
    draw_layer(c, &L);
    kfree(L.px);
}

static mask_t path_mask(canvas2d_t *c, const path_t *p, const mat_t *m, bool evenodd, bool for_paint) {
    edges_t E = { 0 };
    rs_edges_from_path(&E, p, m);
    int mx = 0, my = 0;
    if (for_paint) shadow_margin(&c->st, &mx, &my);
    mask_t M = rs_rasterize(&E, evenodd, -mx, -my, c->w + mx, c->h + my);
    if (E.e) kfree(E.e);
    return M;
}

static void fill_dev(canvas2d_t *c, const path_t *p, const mat_t *m, bool evenodd, const cpaint_t *paint) {
    psrc_t P;
    if (!psrc_make(c, paint, &P)) return;
    mask_t M = path_mask(c, p, m, evenodd, true);
    paint_mask(c, &M, &P);
    if (M.a) kfree(M.a);
}

/* ================================================================ fill, stroke, clip */
void canvas_fill(canvas2d_t *c, const pcmd_t *pc, const mat_t *pm, bool evenodd) {
    if (!c->s) return;
    path_t p;
    pc_build(pc, pm, 0.2f, false, &p);
    fill_dev(c, &p, &MID, evenodd, &c->st.fill);
    rs_path_free(&p);
}

static void stroke_rs(const cstate_t *st, rs_stroke_t *rs) {
    memset(rs, 0, sizeof *rs);
    rs->cap = st->cap;
    rs->join = st->join;
    rs->miter = st->miter;
    rs->dashoff = st->dashoff;
    rs->ndash = st->ndash > 16 ? 16 : st->ndash;
    for (int i = 0; i < rs->ndash; i++) rs->dash[i] = st->dash[i];
}

/* contorno do traço em espaço do usuário; false se a transformação não é inversível */
static bool stroke_outline(canvas2d_t *c, const pcmd_t *pc, bool device, path_t *o) {
    mat_t inv;
    memset(o, 0, sizeof *o);
    if (!minv(c->st.m, &inv) || !(c->st.lw > 0)) return false;
    float sc = mscale(&c->st.m);
    path_t p;
    pc_build(pc, device ? &inv : &MID, 0.2f / sc, true, &p);
    rs_stroke_t rs;
    stroke_rs(&c->st, &rs);
    o->tol = p.tol;
    rs_stroke_path(o, &p, &rs, c->st.lw / 2, sc, 1.0f);
    rs_path_free(&p);
    return true;
}

void canvas_stroke(canvas2d_t *c, const pcmd_t *pc, bool device) {
    if (!c->s) return;
    path_t o;
    if (!stroke_outline(c, pc, device, &o)) return;
    fill_dev(c, &o, &c->st.m, false, &c->st.stroke);
    rs_path_free(&o);
}

void canvas_clip(canvas2d_t *c, const pcmd_t *pc, const mat_t *pm, bool evenodd) {
    if (!c->s) return;
    path_t p;
    pc_build(pc, pm, 0.2f, false, &p);
    mask_t M = path_mask(c, &p, &MID, evenodd, false);
    rs_path_free(&p);
    usize n = (usize)c->w * c->h;
    u8 *nc = kcalloc(n, 1);
    if (!nc) { if (M.a) kfree(M.a); return; }
    if (M.a)
        for (int y = 0; y < M.h; y++) {
            int cy = M.y + y;
            if (cy < 0 || cy >= c->h) continue;
            for (int x = 0; x < M.w; x++) {
                int cx = M.x + x;
                if (cx < 0 || cx >= c->w) continue;
                u32 v = M.a[(usize)y * M.w + x];
                if (c->st.clip) v = (v * c->st.clip[(usize)cy * c->w + cx] + 127) / 255;
                nc[(usize)cy * c->w + cx] = (u8)v;
            }
        }
    if (M.a) kfree(M.a);
    if (c->st.clip) kfree(c->st.clip);
    c->st.clip = nc;
}

void canvas_clear_rect(canvas2d_t *c, float x, float y, float w, float h) {
    if (!c->s || w == 0 || h == 0) return;
    pcmd_t pc = { 0 };
    pc_rect(&pc, &c->st.m, x, y, w, h);
    path_t p;
    pc_build(&pc, &MID, 0.2f, false, &p);
    pc_free(&pc);
    mask_t M = path_mask(c, &p, &MID, false, false);
    rs_path_free(&p);
    if (!M.a) return;
    u32 keep = c->opaque ? 0xFF000000u : 0;
    for (int yy = 0; yy < M.h; yy++) {
        int cy = M.y + yy;
        if (cy < 0 || cy >= c->h) continue;
        for (int xx = 0; xx < M.w; xx++) {
            int cx = M.x + xx;
            if (cx < 0 || cx >= c->w) continue;
            u32 v = M.a[(usize)yy * M.w + xx];
            if (c->st.clip) v = (v * c->st.clip[(usize)cy * c->w + cx] + 127) / 255;
            if (!v) continue;
            u32 *d = &c->s->px[(usize)cy * c->s->stride + cx];
            *d = mul_px(*d, 255 - v) | keep;
        }
    }
    kfree(M.a);
}

static bool point_in(const path_t *p, float x, float y, bool evenodd) {
    int wind = 0;
    for (int c = 0; c < p->nc; c++) {
        int s = p->start[c], n = rs_cont_len(p, c);
        if (n < 2) continue;
        for (int i = 0; i < n; i++) {
            float ax = p->xy[2 * (s + i)], ay = p->xy[2 * (s + i) + 1];
            int j = i + 1 < n ? s + i + 1 : s;
            float bx = p->xy[2 * j], by = p->xy[2 * j + 1];
            if ((ay <= y) != (by <= y)) {
                float ix = ax + (y - ay) * (bx - ax) / (by - ay);
                if (ix > x) wind += by > ay ? 1 : -1;
            }
        }
    }
    return evenodd ? (wind & 1) != 0 : wind != 0;
}

bool canvas_point_in_path(canvas2d_t *c, const pcmd_t *pc, const mat_t *pm, float x, float y, bool evenodd) {
    path_t p;
    pc_build(pc, pm, 0.1f, false, &p);
    bool r = point_in(&p, x, y, evenodd);
    rs_path_free(&p);
    (void)c;
    return r;
}

bool canvas_point_in_stroke(canvas2d_t *c, const pcmd_t *pc, bool device, float x, float y) {
    path_t o;
    mat_t inv;
    if (!minv(c->st.m, &inv) || !stroke_outline(c, pc, device, &o)) return false;
    float ux, uy;
    mapp(&inv, x, y, &ux, &uy);
    bool r = point_in(&o, ux, uy, false);
    rs_path_free(&o);
    return r;
}

/* ================================================================ drawImage */
void canvas_draw_image(canvas2d_t *c, const u32 *px, int iw, int ih, int stride,
                       float sx, float sy, float sw, float sh, float dx, float dy, float dw, float dh) {
    if (!c->s || !px || iw <= 0 || ih <= 0 || sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return;
    /* recorta a origem à imagem, ajustando o destino na mesma proporção */
    float kx = dw / sw, ky = dh / sh;
    if (sx < 0) { dx -= sx * kx; dw += sx * kx; sw += sx; sx = 0; }
    if (sy < 0) { dy -= sy * ky; dh += sy * ky; sh += sy; sy = 0; }
    if (sx + sw > iw) { dw -= (sx + sw - iw) * kx; sw = iw - sx; }
    if (sy + sh > ih) { dh -= (sy + sh - ih) * ky; sh = ih - sy; }
    if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return;
    mat_t inv;
    if (!minv(c->st.m, &inv)) return;
    pcmd_t pc = { 0 };
    pc_rect(&pc, &c->st.m, dx, dy, dw, dh);
    path_t p;
    pc_build(&pc, &MID, 0.2f, false, &p);
    pc_free(&pc);
    mask_t M = path_mask(c, &p, &MID, false, true);
    rs_path_free(&p);
    if (!M.a) return;
    /* redução forte com suavização: filtro de área antes da amostragem bilinear */
    surface_t *tmp = NULL;
    float sc = mscale(&c->st.m);
    if (c->st.smooth && (dw * sc < sw * 0.5f || dh * sc < sh * 0.5f)) {
        int x0 = (int)ffloor(sx), y0 = (int)ffloor(sy), x1 = (int)fceil(sx + sw), y1 = (int)fceil(sy + sh);
        int tw = MAX(1, (int)(dw * sc + 0.5f)), th = MAX(1, (int)(dh * sc + 0.5f));
        surface_t sub = { x1 - x0, y1 - y0, stride, (u32 *)px + (usize)y0 * stride + x0, { 0, 0, x1 - x0, y1 - y0 }, 0, 0 };
        tmp = image_scale(&sub, MAX(1, (int)((x1 - x0) * (float)tw / sw + 0.5f)), MAX(1, (int)((y1 - y0) * (float)th / sh + 0.5f)));
        if (tmp) {
            float fx = (float)tmp->w / (x1 - x0), fy = (float)tmp->h / (y1 - y0);
            sx = (sx - x0) * fx; sy = (sy - y0) * fy; sw *= fx; sh *= fy;
            px = tmp->px; iw = tmp->w; ih = tmp->h; stride = tmp->stride;
        }
    }
    int lx = (int)ffloor(sx), ly = (int)ffloor(sy), hx = MIN(iw - 1, (int)fceil(sx + sw) - 1), hy = MIN(ih - 1, (int)fceil(sy + sh) - 1);
    if (lx < 0) lx = 0;
    if (ly < 0) ly = 0;
    layer_t L = { M.x, M.y, M.w, M.h, kmalloc((usize)M.w * M.h * 4) };
    if (L.px) {
        float rx = sw / dw, ry = sh / dh;
        for (int y = 0; y < M.h; y++)
            for (int x = 0; x < M.w; x++) {
                u32 m = M.a[(usize)y * M.w + x];
                u32 *o = &L.px[(usize)y * M.w + x];
                if (!m) { *o = 0; continue; }
                float ux, uy;
                mapp(&inv, M.x + x + 0.5f, M.y + y + 0.5f, &ux, &uy);
                float u = sx + (ux - dx) * rx, v = sy + (uy - dy) * ry;
                u32 col;
                if (!c->st.smooth) {
                    int ix = (int)ffloor(u), iy = (int)ffloor(v);
                    ix = ix < lx ? lx : ix > hx ? hx : ix;
                    iy = iy < ly ? ly : iy > hy ? hy : iy;
                    col = px[(usize)iy * stride + ix];
                } else {
                    u -= 0.5f; v -= 0.5f;
                    int ix = (int)ffloor(u), iy = (int)ffloor(v);
                    u32 fx = (u32)clampf((u - ix) * 256, 0, 256), fy = (u32)clampf((v - iy) * 256, 0, 256);
                    int x0 = ix < lx ? lx : ix > hx ? hx : ix, x1 = ix + 1 < lx ? lx : ix + 1 > hx ? hx : ix + 1;
                    int y0 = iy < ly ? ly : iy > hy ? hy : iy, y1 = iy + 1 < ly ? ly : iy + 1 > hy ? hy : iy + 1;
                    col = bilerp(px[(usize)y0 * stride + x0], px[(usize)y0 * stride + x1], px[(usize)y1 * stride + x0], px[(usize)y1 * stride + x1], fx, fy);
                }
                *o = mul_px(col, m);
            }
        draw_layer(c, &L);
        kfree(L.px);
    }
    kfree(M.a);
    surface_free(tmp);
}

/* ================================================================ texto */
static int face_of(int weight) { return weight >= 700 ? FONT_BOLD : weight >= 600 ? FONT_SEMIBOLD : weight >= 500 ? FONT_MEDIUM : FONT_REGULAR; }

typedef struct { u16 g; u8 face; float x; } tglyph_t;

/* glifos com a posição da caneta em px no tamanho da fonte; devolve a largura de avanço */
static int text_shape(const cstate_t *st, const char *s, int len, tglyph_t **out, float *width) {
    int cap = len + 1, n = 0;
    tglyph_t *g = kmalloc(sizeof(tglyph_t) * cap);
    *out = g;
    *width = 0;
    if (!g) return 0;
    int face = face_of(st->fweight);
    i32 upem = 0;
    font_face_metrics(face, &upem, NULL, NULL, NULL);
    float pen = 0;
    const char *p = s, *e = s + len;
    while (p < e && *p) {
        u32 cp = utf8_decode(&p);
        if (cp == '\t' || cp == '\n' || cp == '\r' || cp == '\f' || cp == 0x0B) cp = ' ';
        int f = face;
        u16 gi = font_glyph_id(f, cp);
        if (!gi && cp != ' ') {
            u16 g2 = font_glyph_id(FONT_FALLBACK_WEIGHT, cp);
            if (g2) { gi = g2; f = FONT_FALLBACK_WEIGHT; }
        }
        i32 fu = upem;
        if (f != face) font_face_metrics(f, &fu, NULL, NULL, NULL);
        if (!fu) fu = 2048;
        g[n].g = gi;
        g[n].face = (u8)f;
        g[n].x = pen;
        n++;
        pen += font_advance_units(f, gi) * st->fsize / fu + st->letter;
    }
    *width = pen;
    return n;
}

static void font_box(const cstate_t *st, float *asc, float *desc) {
    i32 upem = 0, a = 0, d = 0;
    font_face_metrics(face_of(st->fweight), &upem, &a, &d, NULL);
    if (!upem) { *asc = st->fsize * 0.8f; *desc = st->fsize * 0.2f; return; }
    *asc = (float)a * st->fsize / upem;
    *desc = (float)-d * st->fsize / upem;
}

static float align_off(const cstate_t *st, float w) {
    int a = st->align;
    if (a == CTA_START) a = st->rtl == 2 ? CTA_RIGHT : CTA_LEFT;
    if (a == CTA_END) a = st->rtl == 2 ? CTA_LEFT : CTA_RIGHT;
    return a == CTA_CENTER ? -w / 2 : a == CTA_RIGHT ? -w : 0;
}

/* deslocamento da linha de base alfabética em relação ao y pedido */
static float baseline_off(const cstate_t *st) {
    float asc, desc;
    font_box(st, &asc, &desc);
    switch (st->baseline) {
    case TB_TOP: return asc;
    case TB_HANGING: return asc * 0.8f;
    case TB_MIDDLE: return (asc - desc) / 2;
    case TB_IDEOGRAPHIC: case TB_BOTTOM: return -desc;
    }
    return 0;
}

/* glifos rasterizados num buffer A8; (bx0,by0) = canto em pixels do texto escalado (linha de base em y=0) */
typedef struct { u8 *a; int w, h, x0, y0; } tbuf_t;

static bool text_raster(const tglyph_t *g, int n, float gsize, float scale, int pad, tbuf_t *B) {
    memset(B, 0, sizeof *B);
    int x0 = 1 << 30, y0 = 1 << 30, x1 = -(1 << 30), y1 = -(1 << 30);
    for (int i = 0; i < n; i++) {
        i16 w, h, l, t;
        if (font_raster_glyph(g[i].face, g[i].g, gsize, NULL, 0, &w, &h, &l, &t) <= 0 || w <= 0 || h <= 0) continue;
        int gx = (int)ffloor(g[i].x * scale + 0.5f) + l, gy = t;
        x0 = MIN(x0, gx); y0 = MIN(y0, gy); x1 = MAX(x1, gx + w); y1 = MAX(y1, gy + h);
    }
    if (x1 <= x0 || y1 <= y0) return false;
    x0 -= pad; y0 -= pad; x1 += pad; y1 += pad;
    if ((u64)(x1 - x0) * (y1 - y0) > (64u << 20)) return false;
    B->w = x1 - x0; B->h = y1 - y0; B->x0 = x0; B->y0 = y0;
    B->a = kcalloc((usize)B->w * B->h, 1);
    u8 *gb = kmalloc(512 * 512);
    if (!B->a || !gb) { kfree(B->a); kfree(gb); B->a = NULL; return false; }
    for (int i = 0; i < n; i++) {
        i16 w, h, l, t;
        int k = font_raster_glyph(g[i].face, g[i].g, gsize, gb, 512 * 512, &w, &h, &l, &t);
        if (k <= 0 || w <= 0 || h <= 0 || (int)w * h > 512 * 512) continue;
        int gx = (int)ffloor(g[i].x * scale + 0.5f) + l - x0, gy = t - y0;
        for (int yy = 0; yy < h; yy++)
            for (int xx = 0; xx < w; xx++) {
                u8 *d = &B->a[(usize)(gy + yy) * B->w + gx + xx];
                u32 v = *d + gb[yy * w + xx];
                *d = v > 255 ? 255 : (u8)v;
            }
    }
    kfree(gb);
    return true;
}

/* contorno do texto: dilatação − erosão por um disco de raio r (em pixels do buffer) */
static void text_outline(tbuf_t *B, float r) {
    int R = (int)fceil(r);
    if (R < 1) R = 1;
    usize n = (usize)B->w * B->h;
    u8 *dil = kmalloc(n), *ero = kmalloc(n);
    if (!dil || !ero) { kfree(dil); kfree(ero); return; }
    for (int y = 0; y < B->h; y++)
        for (int x = 0; x < B->w; x++) {
            u8 mx = 0, mn = 255;
            for (int dy = -R; dy <= R; dy++) {
                int yy = y + dy;
                for (int dx = -R; dx <= R; dx++) {
                    if (dx * dx + dy * dy > (r + 0.5f) * (r + 0.5f)) continue;
                    int xx = x + dx;
                    u8 v = (yy < 0 || yy >= B->h || xx < 0 || xx >= B->w) ? 0 : B->a[(usize)yy * B->w + xx];
                    if (v > mx) mx = v;
                    if (v < mn) mn = v;
                }
            }
            dil[(usize)y * B->w + x] = mx;
            ero[(usize)y * B->w + x] = mn;
        }
    for (usize i = 0; i < n; i++) B->a[i] = (u8)(dil[i] > ero[i] ? dil[i] - ero[i] : 0);
    kfree(dil);
    kfree(ero);
}

void canvas_text(canvas2d_t *c, const char *s, int len, float x, float y, float maxw, bool stroke) {
    if (!c->s || len <= 0) return;
    cstate_t *st = &c->st;
    if (stroke && !(st->lw > 0)) return;
    tglyph_t *g;
    float width;
    int n = text_shape(st, s, len, &g, &width);
    if (!n) { kfree(g); return; }
    float hs = maxw > 0 && width > maxw ? maxw / width : 1;
    float sc = mscale(&st->m);
    if (sc < 0.01f) sc = 0.01f;
    float gsize = st->fsize * sc;
    if (gsize > 512) { sc *= 512 / gsize; gsize = 512; }
    int pad = stroke ? (int)fceil(st->lw / 2 * sc) + 2 : 1;
    tbuf_t B;
    bool ok = text_raster(g, n, gsize, sc, pad, &B);
    kfree(g);
    if (!ok) return;
    if (stroke) text_outline(&B, st->lw / 2 * sc);
    float ox = x + align_off(st, width * hs), oy = y + baseline_off(st);
    /* texto (buffer) → usuário → pixels */
    mat_t t2u = { hs / sc, 0, 0, 1 / sc, ox + B.x0 * hs / sc, oy + B.y0 / sc };
    mat_t t2d = mmul(st->m, t2u), d2t;
    if (!minv(t2d, &d2t)) { kfree(B.a); return; }
    float xs[4], ys[4];
    mapp(&t2d, 0, 0, &xs[0], &ys[0]); mapp(&t2d, (float)B.w, 0, &xs[1], &ys[1]);
    mapp(&t2d, 0, (float)B.h, &xs[2], &ys[2]); mapp(&t2d, (float)B.w, (float)B.h, &xs[3], &ys[3]);
    float minx = xs[0], maxx = xs[0], miny = ys[0], maxy = ys[0];
    for (int i = 1; i < 4; i++) { minx = MIN(minx, xs[i]); maxx = MAX(maxx, xs[i]); miny = MIN(miny, ys[i]); maxy = MAX(maxy, ys[i]); }
    int mx, my;
    shadow_margin(st, &mx, &my);
    int X0 = MAX((int)ffloor(minx), -mx), Y0 = MAX((int)ffloor(miny), -my);
    int X1 = MIN((int)fceil(maxx), c->w + mx), Y1 = MIN((int)fceil(maxy), c->h + my);
    psrc_t P;
    if (X1 > X0 && Y1 > Y0 && psrc_make(c, stroke ? &st->stroke : &st->fill, &P)) {
        layer_t L = { X0, Y0, X1 - X0, Y1 - Y0, kmalloc((usize)(X1 - X0) * (Y1 - Y0) * 4) };
        if (L.px) {
            for (int yy = 0; yy < L.h; yy++)
                for (int xx = 0; xx < L.w; xx++) {
                    float tx, ty;
                    mapp(&d2t, X0 + xx + 0.5f, Y0 + yy + 0.5f, &tx, &ty);
                    tx -= 0.5f; ty -= 0.5f;
                    int ix = (int)ffloor(tx), iy = (int)ffloor(ty);
                    u32 cov = 0;
                    if (ix >= -1 && iy >= -1 && ix < B.w && iy < B.h) {
                        float fx = tx - ix, fy = ty - iy;
                        float a00 = (ix >= 0 && iy >= 0) ? B.a[(usize)iy * B.w + ix] : 0;
                        float a10 = (ix + 1 < B.w && iy >= 0) ? B.a[(usize)iy * B.w + ix + 1] : 0;
                        float a01 = (ix >= 0 && iy + 1 < B.h) ? B.a[(usize)(iy + 1) * B.w + ix] : 0;
                        float a11 = (ix + 1 < B.w && iy + 1 < B.h) ? B.a[(usize)(iy + 1) * B.w + ix + 1] : 0;
                        float v = (a00 * (1 - fx) + a10 * fx) * (1 - fy) + (a01 * (1 - fx) + a11 * fx) * fy;
                        cov = (u32)(v + 0.5f);
                        if (cov > 255) cov = 255;
                    }
                    L.px[(usize)yy * L.w + xx] = cov ? mul_px(psrc_at(&P, X0 + xx + 0.5f, Y0 + yy + 0.5f), cov) : 0;
                }
            draw_layer(c, &L);
            kfree(L.px);
        }
    }
    kfree(B.a);
}

void canvas_measure(canvas2d_t *c, const char *s, int len, cmetrics_t *m) {
    cstate_t *st = &c->st;
    memset(m, 0, sizeof *m);
    tglyph_t *g;
    float width;
    int n = text_shape(st, s, len, &g, &width);
    float ao = align_off(st, width), bo = baseline_off(st), asc, desc;
    font_box(st, &asc, &desc);
    m->width = width;
    m->fasc = asc - bo;
    m->fdesc = desc + bo;
    m->alpha = -bo;
    m->hanging = asc * 0.8f - bo;
    m->ideo = -desc - bo;
    /* caixa real dos glifos, no tamanho da fonte */
    float l = 0, r = 0, top = 0, bot = 0;
    bool any = false;
    for (int i = 0; i < n; i++) {
        i16 w, h, gl, gt;
        if (font_raster_glyph(g[i].face, g[i].g, st->fsize, NULL, 0, &w, &h, &gl, &gt) <= 0 || w <= 0 || h <= 0) continue;
        float gx0 = g[i].x + gl, gx1 = gx0 + w, gy0 = gt, gy1 = gt + (float)h;
        if (!any) { l = gx0; r = gx1; top = gy0; bot = gy1; any = true; }
        else { l = MIN(l, gx0); r = MAX(r, gx1); top = MIN(top, gy0); bot = MAX(bot, gy1); }
    }
    kfree(g);
    if (any) {
        m->l = -(ao + l);
        m->r = ao + r;
        m->asc = -(top + bo);
        m->desc = bot + bo;
    } else {
        m->l = -ao;
        m->r = ao + width;
    }
}

/* fonte CSS: [estilo] [variante] [peso] [largura] tamanho[/altura-de-linha] família */
bool canvas_set_font(cstate_t *st, const char *v) {
    char buf[256];
    int k = 0;
    while (*v && isspace((u8)*v)) v++;
    for (; *v && k < (int)sizeof buf - 1; v++) buf[k++] = *v;
    while (k && isspace((u8)buf[k - 1])) k--;
    buf[k] = 0;
    if (!k) return false;
    bool italic = false, smallcaps = false;
    int weight = 400;
    char *p = buf;
    float size = -1;
    for (int tok = 0; tok < 6 && *p; tok++) {
        while (*p == ' ') p++;
        char *e = p;
        while (*e && *e != ' ') e++;
        int n = (int)(e - p);
        char w[32];
        if (n >= (int)sizeof w) break;
        for (int i = 0; i < n; i++) w[i] = (char)tolower((u8)p[i]);
        w[n] = 0;
        if (!strcmp(w, "normal")) { p = e; continue; }
        if (!strcmp(w, "italic") || !strcmp(w, "oblique")) { italic = true; p = e; continue; }
        if (!strcmp(w, "small-caps")) { smallcaps = true; p = e; continue; }
        if (!strcmp(w, "bold") || !strcmp(w, "bolder")) { weight = 700; p = e; continue; }
        if (!strcmp(w, "lighter")) { weight = 100; p = e; continue; }
        if (n == 3 && isdigit((u8)w[0]) && w[1] == '0' && w[2] == '0' && w[0] != '0') { weight = (w[0] - '0') * 100; p = e; continue; }
        if (strstr(w, "condensed") || strstr(w, "expanded")) { p = e; continue; }
        /* tamanho */
        const char *q = p;
        float num;
        if (rs_parse_f(&q, &num)) {
            char unit[8] = "";
            int u = 0;
            while (u < 7 && (isalpha((u8)*q) || *q == '%')) unit[u++] = (char)tolower((u8)*q++);
            unit[u] = 0;
            if (!strcmp(unit, "px")) size = num;
            else if (!strcmp(unit, "pt")) size = num * 4 / 3;
            else if (!strcmp(unit, "pc")) size = num * 16;
            else if (!strcmp(unit, "in")) size = num * 96;
            else if (!strcmp(unit, "cm")) size = num * 96 / 2.54f;
            else if (!strcmp(unit, "mm")) size = num * 96 / 25.4f;
            else if (!strcmp(unit, "q")) size = num * 96 / 101.6f;
            else if (!strcmp(unit, "em") || !strcmp(unit, "rem")) size = num * 10;
            else if (!strcmp(unit, "%")) size = num * 10 / 100;
            else if (!u && num == 0) size = 0;
            else return false;
            if (size < 0) return false;
            if (*q == '/') { q++; while (*q && *q != ' ') q++; }
            p = (char *)q;
            break;
        }
        static const struct { const char *k; float v; } kw[] = {
            { "xx-small", 9 }, { "x-small", 10 }, { "small", 13 }, { "medium", 16 }, { "large", 18 }, { "x-large", 24 },
            { "xx-large", 32 }, { "xxx-large", 48 }, { "smaller", 8.33f }, { "larger", 12 } };
        char kwb[32];
        int kn = 0;
        while (kn < 31 && w[kn] && w[kn] != '/') { kwb[kn] = w[kn]; kn++; }
        kwb[kn] = 0;
        for (u32 i = 0; i < ARRAY_LEN(kw); i++) if (!strcmp(kwb, kw[i].k)) size = kw[i].v;
        if (size < 0) return false;
        p += kn;
        if (*p == '/') { while (*p && *p != ' ') p++; }
        break;
    }
    if (size < 0) return false;
    while (*p == ' ') p++;
    if (!*p) return false;                      /* família obrigatória */
    char fam[128];
    int f = 0;
    bool sp = false;
    for (; *p && f < (int)sizeof fam - 2; p++) {
        if (isspace((u8)*p)) { sp = f > 0; continue; }
        if (sp) { fam[f++] = ' '; sp = false; }
        fam[f++] = *p;
    }
    fam[f] = 0;
    st->fsize = size;
    st->fweight = weight;
    st->italic = italic;
    char wb[16] = "";
    if (weight == 700) strcpy(wb, "bold ");
    else if (weight != 400) snprintf(wb, sizeof wb, "%d ", weight);
    char num[32];
    snprintf(num, sizeof num, "%.4f", (double)size);
    { int L = (int)strlen(num); while (L && num[L - 1] == '0') num[--L] = 0; if (L && num[L - 1] == '.') num[--L] = 0; }
    snprintf(st->font, sizeof st->font, "%s%s%s%spx %s", italic ? "italic " : "", smallcaps ? "small-caps " : "", wb, num, fam);
    return true;
}

/* ================================================================ ImageData */
void canvas_get_rgba(canvas2d_t *c, int sx, int sy, int sw, int sh, u8 *out) {
    for (int y = 0; y < sh; y++)
        for (int x = 0; x < sw; x++) {
            int cx = sx + x, cy = sy + y;
            u8 *o = out + ((usize)y * sw + x) * 4;
            if (!c->s || cx < 0 || cy < 0 || cx >= c->w || cy >= c->h) { o[0] = o[1] = o[2] = o[3] = 0; continue; }
            u32 p = c->s->px[(usize)cy * c->s->stride + cx], a = p >> 24;
            if (!a) { o[0] = o[1] = o[2] = o[3] = 0; continue; }
            u32 r = (p >> 16) & 255, g = (p >> 8) & 255, b = p & 255;
            if (a != 255) { r = (r * 255 + a / 2) / a; g = (g * 255 + a / 2) / a; b = (b * 255 + a / 2) / a; }
            o[0] = (u8)(r > 255 ? 255 : r); o[1] = (u8)(g > 255 ? 255 : g); o[2] = (u8)(b > 255 ? 255 : b); o[3] = (u8)a;
        }
}

void canvas_put_rgba(canvas2d_t *c, const u8 *rgba, int iw, int ih, int dx, int dy, int x0, int y0, int x1, int y1) {
    if (!c->s) return;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > iw) x1 = iw;
    if (y1 > ih) y1 = ih;
    for (int y = y0; y < y1; y++) {
        int cy = dy + y;
        if (cy < 0 || cy >= c->h) continue;
        for (int x = x0; x < x1; x++) {
            int cx = dx + x;
            if (cx < 0 || cx >= c->w) continue;
            const u8 *s = rgba + ((usize)y * iw + x) * 4;
            u32 a = c->opaque ? 255 : s[3];
            u32 r = (s[0] * a + 127) / 255, g = (s[1] * a + 127) / 255, b = (s[2] * a + 127) / 255;
            c->s->px[(usize)cy * c->s->stride + cx] = a << 24 | r << 16 | g << 8 | b;
        }
    }
}

/* ================================================================ PNG com deflate próprio (Huffman fixo + LZ77) */
typedef struct { u8 *d; usize n, cap; u32 bits; int nb; bool err; } bw_t;
static void bw_byte(bw_t *w, u8 b) {
    if (w->n == w->cap) {
        usize nc = w->cap ? w->cap * 2 : 4096;
        u8 *nd = krealloc(w->d, nc);
        if (!nd) { w->err = true; return; }
        w->d = nd;
        w->cap = nc;
    }
    w->d[w->n++] = b;
}
static void bw_bits(bw_t *w, u32 v, int n) {   /* LSB primeiro */
    w->bits |= v << w->nb;
    w->nb += n;
    while (w->nb >= 8) { bw_byte(w, (u8)w->bits); w->bits >>= 8; w->nb -= 8; }
}
static void bw_huff(bw_t *w, u32 code, int len) {   /* códigos de Huffman vão do bit mais alto */
    u32 r = 0;
    for (int i = 0; i < len; i++) r |= ((code >> i) & 1) << (len - 1 - i);
    bw_bits(w, r, len);
}
static void put_lit(bw_t *w, int sym) {
    if (sym < 144) bw_huff(w, 0x30 + sym, 8);
    else if (sym < 256) bw_huff(w, 0x190 + sym - 144, 9);
    else if (sym < 280) bw_huff(w, sym - 256, 7);
    else bw_huff(w, 0xC0 + sym - 280, 8);
}
static const u16 len_base[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
static const u8 len_extra[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
static const u16 dist_base[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
static const u8 dist_extra[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };

static void deflate_fixed(bw_t *w, const u8 *in, usize n) {
    bw_bits(w, 1, 1);   /* BFINAL */
    bw_bits(w, 1, 2);   /* BTYPE = 01 (Huffman fixo) */
    enum { HB = 15, WIN = 32768 };
    i32 *head = kmalloc(sizeof(i32) << HB), *prev = kmalloc(sizeof(i32) * WIN);
    if (!head || !prev) { kfree(head); kfree(prev); for (usize i = 0; i < n; i++) put_lit(w, in[i]); put_lit(w, 256); return; }
    for (int i = 0; i < (1 << HB); i++) head[i] = -1;
    usize i = 0;
#define HASH(p) ((((u32)(p)[0] << 10) ^ ((u32)(p)[1] << 5) ^ (p)[2]) & ((1u << HB) - 1))
    while (i < n) {
        int best = 0, bdist = 0;
        if (i + 3 <= n) {
            u32 h = HASH(in + i);
            i32 cand = head[h];
            int chain = 48;
            while (cand >= 0 && (usize)cand + WIN > i && chain--) {
                int maxl = (int)MIN((usize)258, n - i), l = 0;
                const u8 *a = in + cand, *b = in + i;
                while (l < maxl && a[l] == b[l]) l++;
                if (l > best) { best = l; bdist = (int)(i - cand); if (l == maxl) break; }
                cand = prev[cand & (WIN - 1)];
            }
            prev[i & (WIN - 1)] = head[h];
            head[h] = (i32)i;
        }
        if (best >= 3) {
            int li = 28;
            while (li > 0 && len_base[li] > best) li--;
            put_lit(w, 257 + li);
            if (len_extra[li]) bw_bits(w, best - len_base[li], len_extra[li]);
            int di = 29;
            while (di > 0 && dist_base[di] > bdist) di--;
            bw_huff(w, di, 5);
            if (dist_extra[di]) bw_bits(w, bdist - dist_base[di], dist_extra[di]);
            for (int k = 1; k < best; k++) {
                usize j = i + k;
                if (j + 3 <= n) { u32 h = HASH(in + j); prev[j & (WIN - 1)] = head[h]; head[h] = (i32)j; }
            }
            i += best;
        } else put_lit(w, in[i++]);
    }
#undef HASH
    put_lit(w, 256);
    if (w->nb) bw_bits(w, 0, 8 - w->nb);
    kfree(head);
    kfree(prev);
}

static u32 crc_tab[256];
static u32 crc32_png(u32 c, const u8 *d, usize n) {
    if (!crc_tab[1]) for (u32 i = 0; i < 256; i++) { u32 v = i; for (int k = 0; k < 8; k++) v = v & 1 ? 0xEDB88320u ^ (v >> 1) : v >> 1; crc_tab[i] = v; }
    c = ~c;
    for (usize i = 0; i < n; i++) c = crc_tab[(c ^ d[i]) & 255] ^ (c >> 8);
    return ~c;
}
static void be32(bw_t *w, u32 v) { bw_byte(w, (u8)(v >> 24)); bw_byte(w, (u8)(v >> 16)); bw_byte(w, (u8)(v >> 8)); bw_byte(w, (u8)v); }
static void chunk(bw_t *w, const char *type, const u8 *d, usize n) {
    be32(w, (u32)n);
    usize at = w->n;
    for (int i = 0; i < 4; i++) bw_byte(w, (u8)type[i]);
    for (usize i = 0; i < n; i++) bw_byte(w, d[i]);
    if (w->err) return;
    be32(w, crc32_png(0, w->d + at, n + 4));
}

static int paeth(int a, int b, int c) {
    int p = a + b - c, pa = p > a ? p - a : a - p, pb = p > b ? p - b : b - p, pc = p > c ? p - c : c - p;
    return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
}

u8 *png_encode_rgba(const u8 *rgba, int w, int h, usize *len) {
    usize row = (usize)w * 4, rawn = (row + 1) * h;
    u8 *raw = kmalloc(rawn), *cand = kmalloc(row * 5);
    if (!raw || !cand) { kfree(raw); kfree(cand); return NULL; }
    for (int y = 0; y < h; y++) {
        const u8 *cur = rgba + y * row, *up = y ? rgba + (y - 1) * row : NULL;
        u32 best = 0xFFFFFFFFu;
        int bf = 0;
        for (int f = 0; f < 5; f++) {
            u8 *o = cand + f * row;
            u32 sum = 0;
            for (usize i = 0; i < row; i++) {
                int a = i >= 4 ? cur[i - 4] : 0, b = up ? up[i] : 0, c = i >= 4 && up ? up[i - 4] : 0, pr;
                pr = f == 0 ? 0 : f == 1 ? a : f == 2 ? b : f == 3 ? (a + b) / 2 : paeth(a, b, c);
                o[i] = (u8)(cur[i] - pr);
                sum += (o[i] < 128 ? o[i] : 256 - o[i]);
            }
            if (sum < best) { best = sum; bf = f; }
        }
        raw[y * (row + 1)] = (u8)bf;
        memcpy(raw + y * (row + 1) + 1, cand + bf * row, row);
    }
    kfree(cand);
    bw_t z = { 0 };
    bw_byte(&z, 0x78); bw_byte(&z, 0x01);
    deflate_fixed(&z, raw, rawn);
    u32 s1 = 1, s2 = 0;
    for (usize i = 0; i < rawn; i++) { s1 = (s1 + raw[i]) % 65521; s2 = (s2 + s1) % 65521; }
    kfree(raw);
    be32(&z, s2 << 16 | s1);
    bw_t o = { 0 };
    static const u8 sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    for (int i = 0; i < 8; i++) bw_byte(&o, sig[i]);
    u8 ihdr[13] = { (u8)(w >> 24), (u8)(w >> 16), (u8)(w >> 8), (u8)w, (u8)(h >> 24), (u8)(h >> 16), (u8)(h >> 8), (u8)h, 8, 6, 0, 0, 0 };
    chunk(&o, "IHDR", ihdr, 13);
    chunk(&o, "IDAT", z.d, z.n);
    chunk(&o, "IEND", NULL, 0);
    bool err = z.err || o.err;
    kfree(z.d);
    if (err) { kfree(o.d); return NULL; }
    *len = o.n;
    return o.d;
}

u8 *canvas_png(canvas2d_t *c, usize *len) {
    if (!c->s || c->w <= 0 || c->h <= 0) return NULL;
    u8 *rgba = kmalloc((usize)c->w * c->h * 4);
    if (!rgba) return NULL;
    canvas_get_rgba(c, 0, 0, c->w, c->h, rgba);
    u8 *png = png_encode_rgba(rgba, c->w, c->h, len);
    kfree(rgba);
    return png;
}

/* ================================================================ cores */
bool canvas_parse_color(const char *s, u32 current, u32 *out) {
    while (isspace((u8)*s)) s++;
    if (!strncasecmp(s, "currentcolor", 12)) {
        const char *e = s + 12;
        while (isspace((u8)*e)) e++;
        if (!*e) { *out = current; return true; }
        return false;
    }
    return css_color(s, out);
}

void canvas_color_str(u32 c, char *out, int cap) {
    u32 a = c >> 24, r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
    if (a == 255) { snprintf(out, cap, "#%02x%02x%02x", r, g, b); return; }
    /* menor decimal que volta ao mesmo byte de alfa */
    char ab[16] = "0";
    if (a) {
        for (int digits = 1; digits <= 4; digits++) {
            int scale = digits == 1 ? 10 : digits == 2 ? 100 : digits == 3 ? 1000 : 10000;
            int v = (int)(a * scale / 255.0f + 0.5f);
            if ((int)(v * 255.0f / scale + 0.5f) == (int)a) {
                snprintf(ab, sizeof ab, "0.%0*d", digits, v);
                int L = (int)strlen(ab);
                while (L > 2 && ab[L - 1] == '0') ab[--L] = 0;
                break;
            }
        }
    }
    snprintf(out, cap, "rgba(%u, %u, %u, %s)", r, g, b, ab);
}
