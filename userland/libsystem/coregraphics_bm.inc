/* CGBitmapContext com path próprio: incluído no fim do coregraphics.c (usa cgobj_t, blend_at, ctm_of).
 * O path guarda os pontos já no espaço do dispositivo (como no macOS, o CTM vale na hora de acrescentar).
 * Traço, recorte por máscara de cobertura, gradientes, camadas de transparência, padrões e CGLayer.
 * O cálculo geométrico mora em cg_raster.c (teste: tools/cg_raster_test.c). */

static cg_bmx_t *bmx(cgobj_t *o) {
    if (!o) return 0;
    if (!o->gx) {
        cg_bmx_t *x = calloc(1, sizeof *x);
        if (!x) return 0;
        cgr_init(&x->path);
        x->stroke = 0xFF000000u;
        x->lw = 1;
        x->miter = 10;
        x->galpha = 1;
        x->aa = 1;
        o->gx = x;
    }
    return o->gx;
}
static void bmx_free(cgobj_t *o) {
    cg_bmx_t *x = o->gx;
    if (!x) return;
    cgr_free(&x->path);
    free(x->mask);
    for (int i = 0; i < x->gsp; i++) free(x->gs[i].mask);
    free(x->tile);
    for (int i = 0; i < x->nlayer; i++) free(o->data != x->layer_data[i] ? x->layer_data[i] : 0);
    free(x);
    o->gx = 0;
}
static void bmx_save(cgobj_t *o) {
    cg_bmx_t *x = o->gx;
    if (!x || x->gsp >= 8) return;
    cg_bmx_gs *g = &x->gs[x->gsp++];
    g->stroke = x->stroke; g->lw = x->lw; g->miter = x->miter; g->phase = x->phase;
    memcpy(g->dash, x->dash, sizeof g->dash);
    g->ndash = x->ndash; g->cap = x->cap; g->join = x->join; g->galpha = x->galpha; g->aa = x->aa;
    g->sdx = x->sdx; g->sdy = x->sdy; g->sblur = x->sblur; g->scolor = x->scolor;
    g->pattern = x->pattern_on;
    g->mask = 0;
    if (x->mask) {                               /* o recorte volta no restore: guarda uma cópia */
        size_t n = (size_t)o->w * o->h;
        g->mask = malloc(n);
        if (g->mask) memcpy(g->mask, x->mask, n);
    }
}
static void bmx_restore(cgobj_t *o) {
    cg_bmx_t *x = o->gx;
    if (!x || x->gsp <= 0) return;
    cg_bmx_gs *g = &x->gs[--x->gsp];
    x->stroke = g->stroke; x->lw = g->lw; x->miter = g->miter; x->phase = g->phase;
    memcpy(x->dash, g->dash, sizeof x->dash);
    x->ndash = g->ndash; x->cap = g->cap; x->join = g->join; x->galpha = g->galpha; x->aa = g->aa;
    x->sdx = g->sdx; x->sdy = g->sdy; x->sblur = g->sblur; x->scolor = g->scolor;
    x->pattern_on = g->pattern;
    free(x->mask);
    x->mask = g->mask;
    g->mask = 0;
}

/* cor ARGB não pré-multiplicada -> pré-multiplicada, com o alfa global do contexto */
static u32 premul(u32 c, double galpha) {
    u32 a = (u32)((double)(c >> 24) * (galpha < 0 ? 0 : galpha > 1 ? 1 : galpha) + 0.5);
    if (a > 255) a = 255;
    return a << 24 | (((c >> 16) & 255) * a / 255) << 16 | (((c >> 8) & 255) * a / 255) << 8 | ((c & 255) * a / 255);
}
static u32 scale_px(u32 p, u32 k) {
    if (k >= 255) return p;
    return (((p >> 24) * k / 255) << 24) | ((((p >> 16) & 255) * k / 255) << 16) | ((((p >> 8) & 255) * k / 255) << 8) | ((p & 255) * k / 255);
}
/* fonte do preenchimento no pixel do dispositivo: cor sólida ou o ladrilho do padrão */
static u32 fill_src(cgobj_t *o, int x, int y) {
    cg_bmx_t *g = o->gx;
    if (g && g->pattern_on && g->tile && g->tw && g->th) {
        long tx = (long)floor(x - g->pdx), ty = (long)floor(y - g->pdy);
        tx %= g->tw; ty %= g->th;
        if (tx < 0) tx += g->tw;
        if (ty < 0) ty += g->th;
        return scale_px(g->tile[(size_t)ty * g->tw + tx], (u32)(g->galpha * 255 + 0.5));
    }
    return premul(o->fill ? o->fill : 0xFF000000u, g ? g->galpha : 1);
}

/* ---------------------------------------------------------------- path no espaço do dispositivo */
static double dev_tol(cgobj_t *o) { (void)o; return 0.2; }
static cgobj_t *bm(const void *c) { return as_kind(c, CGK_BM); }
static void dpt(cgobj_t *o, double x, double y, double *dx, double *dy) { ctm_of(o, x, y, dx, dy); }

int cx_bm_move(const void *c, double x, double y) {
    cgobj_t *o = bm(c); cg_bmx_t *g = bmx(o);
    if (!g) return 0;
    double dx, dy; dpt(o, x, y, &dx, &dy);
    cgr_move(&g->path, dx, dy);
    return 1;
}
int cx_bm_line(const void *c, double x, double y) {
    cgobj_t *o = bm(c); cg_bmx_t *g = bmx(o);
    if (!g) return 0;
    double dx, dy; dpt(o, x, y, &dx, &dy);
    cgr_line(&g->path, dx, dy);
    return 1;
}
int cx_bm_curve(const void *c, double c1x, double c1y, double c2x, double c2y, double x, double y) {
    cgobj_t *o = bm(c); cg_bmx_t *g = bmx(o);
    if (!g) return 0;
    double a, b, d, e, f, h;
    dpt(o, c1x, c1y, &a, &b); dpt(o, c2x, c2y, &d, &e); dpt(o, x, y, &f, &h);
    if (!g->path.n) cgr_move(&g->path, f, h);
    cgr_cubic(&g->path, a, b, d, e, f, h, dev_tol(o));
    return 1;
}
int cx_bm_quad(const void *c, double cx, double cy, double x, double y) {
    cgobj_t *o = bm(c); cg_bmx_t *g = bmx(o);
    if (!g) return 0;
    double a, b, f, h;
    dpt(o, cx, cy, &a, &b); dpt(o, x, y, &f, &h);
    if (!g->path.n) cgr_move(&g->path, f, h);
    cgr_quad(&g->path, a, b, f, h, dev_tol(o));
    return 1;
}
int cx_bm_close(const void *c) {
    cgobj_t *o = bm(c); cg_bmx_t *g = bmx(o);
    if (!g) return 0;
    cgr_close(&g->path);
    return 1;
}
int cx_bm_begin(const void *c) {
    cgobj_t *o = bm(c); cg_bmx_t *g = bmx(o);
    if (!g) return 0;
    cgr_clear(&g->path);
    return 1;
}
int cx_bm_rect(const void *c, double x, double y, double w, double h) {
    if (!cx_bm_move(c, x, y)) return 0;
    cx_bm_line(c, x + w, y);
    cx_bm_line(c, x + w, y + h);
    cx_bm_line(c, x, y + h);
    return cx_bm_close(c);
}
int cx_bm_ellipse(const void *c, double x, double y, double w, double h) {
    cgobj_t *o = bm(c);
    if (!bmx(o)) return 0;
    const double k = 0.5522847498307936;
    double rx = w / 2, ry = h / 2, cx = x + rx, cy = y + ry;
    cx_bm_move(c, cx + rx, cy);
    cx_bm_curve(c, cx + rx, cy + ry * k, cx + rx * k, cy + ry, cx, cy + ry);
    cx_bm_curve(c, cx - rx * k, cy + ry, cx - rx, cy + ry * k, cx - rx, cy);
    cx_bm_curve(c, cx - rx, cy - ry * k, cx - rx * k, cy - ry, cx, cy - ry);
    cx_bm_curve(c, cx + rx * k, cy - ry, cx + rx, cy - ry * k, cx + rx, cy);
    return cx_bm_close(c);
}

/* ---------------------------------------------------------------- pintura com cobertura */
typedef struct { cgobj_t *o; u32 solid; int use_solid; } paint_t;
static void paint_row(void *ctx, int y, int x0, int x1, const uint8_t *cov) {
    paint_t *p = ctx;
    cg_bmx_t *g = p->o->gx;
    for (int x = x0; x < x1; x++) {
        u32 k = cov[x - x0];
        if (g && !g->aa) k = k >= 128 ? 255 : 0;
        if (!k) continue;
        blend_at(p->o, x, y, scale_px(p->use_solid ? p->solid : fill_src(p->o, x, y), k));
    }
}
static void fill_poly(cgobj_t *o, const cgr_poly *poly, int eo, u32 solid, int use_solid) {
    paint_t pt = {o, solid, use_solid};
    cgr_fill(poly, eo, o->cl, o->ct, o->cr, o->cbt, paint_row, &pt);
}
static int ctm_inverse(cgobj_t *o, double *ia, double *ib, double *ic, double *id, double *itx, double *ity) {
    double det = o->ca * o->cd - o->cb * o->cc;
    if (det > -1e-12 && det < 1e-12) return 0;
    *ia = o->cd / det; *ib = -o->cb / det; *ic = -o->cc / det; *id = o->ca / det;
    *itx = -(*ia * o->cu + *ic * o->cv);
    *ity = -(*ib * o->cu + *id * o->cv);
    return 1;
}
/* contorno do traço do path atual, no dispositivo: o traço é calculado no espaço do usuário */
static int stroke_outline(cgobj_t *o, const cgr_poly *path, cgr_poly *out) {
    cg_bmx_t *g = o->gx;
    double ia, ib, ic, id, itx, ity;
    if (!g || !ctm_inverse(o, &ia, &ib, &ic, &id, &itx, &ity)) return 0;
    cgr_poly user;
    cgr_init(&user);
    for (int k = 0; k < path->nsub; k++) {
        int a = path->sub[k], b = k + 1 < path->nsub ? path->sub[k + 1] : path->n;
        cgr_move(&user, path->pts[a].x, path->pts[a].y);
        for (int i = a + 1; i < b; i++) cgr_line(&user, path->pts[i].x, path->pts[i].y);
        if (path->closed[k]) cgr_close(&user);
    }
    cgr_transform(&user, ia, ib, ic, id, itx, ity);
    cgr_stroke_t st;
    double scale = sqrt(fabs(o->ca * o->cd - o->cb * o->cc));
    st.width = g->lw; st.miter = g->miter; st.phase = g->phase;
    st.dash = g->ndash ? g->dash : 0; st.ndash = g->ndash;
    st.cap = g->cap; st.join = g->join;
    st.tol = 0.2 / (scale > 1e-9 ? scale : 1);
    if (st.width * scale < 1) st.width = 1 / (scale > 1e-9 ? scale : 1);   /* traço fininho: um pixel, como o macOS */
    cgr_stroke(&user, &st, out);
    cgr_free(&user);
    cgr_transform(out, o->ca, o->cb, o->cc, o->cd, o->cu, o->cv);
    return 1;
}

int cx_bm_fill(const void *c, int eo) {
    cgobj_t *o = bm(c); cg_bmx_t *g = bmx(o);
    if (!g) return 0;
    if (o->data) fill_poly(o, &g->path, eo, 0, 0);
    cgr_clear(&g->path);
    return 1;
}
int cx_bm_stroke(const void *c) {
    cgobj_t *o = bm(c); cg_bmx_t *g = bmx(o);
    if (!g) return 0;
    if (o->data) {
        cgr_poly out;
        cgr_init(&out);
        if (stroke_outline(o, &g->path, &out)) fill_poly(o, &out, 0, premul(g->stroke, g->galpha), 1);
        cgr_free(&out);
    }
    cgr_clear(&g->path);
    return 1;
}
/* CGPathDrawingMode: 0 fill, 1 eoFill, 2 stroke, 3 fillStroke, 4 eoFillStroke */
int cx_bm_draw(const void *c, int mode) {
    cgobj_t *o = bm(c); cg_bmx_t *g = bmx(o);
    if (!g) return 0;
    cgr_poly keep;
    cgr_init(&keep);
    for (int k = 0; k < g->path.nsub; k++) {
        int a = g->path.sub[k], b = k + 1 < g->path.nsub ? g->path.sub[k + 1] : g->path.n;
        cgr_move(&keep, g->path.pts[a].x, g->path.pts[a].y);
        for (int i = a + 1; i < b; i++) cgr_line(&keep, g->path.pts[i].x, g->path.pts[i].y);
        if (g->path.closed[k]) cgr_close(&keep);
    }
    if (mode == 0 || mode == 3) fill_poly(o, &keep, 0, 0, 0);
    if (mode == 1 || mode == 4) fill_poly(o, &keep, 1, 0, 0);
    cgr_clear(&g->path);
    if (mode >= 2) {
        cgr_poly out;
        cgr_init(&out);
        if (stroke_outline(o, &keep, &out)) fill_poly(o, &out, 0, premul(g->stroke, g->galpha), 1);
        cgr_free(&out);
    }
    cgr_free(&keep);
    return 1;
}

int cx_bm_set_stroke(const void *c, u32 argb) { cg_bmx_t *g = bmx(bm(c)); if (!g) return 0; g->stroke = argb; return 1; }
int cx_bm_set_line_width(const void *c, double w) { cg_bmx_t *g = bmx(bm(c)); if (!g) return 0; g->lw = w < 0 ? 0 : w; return 1; }
int cx_bm_set_cap(const void *c, int v) { cg_bmx_t *g = bmx(bm(c)); if (!g) return 0; g->cap = v >= 0 && v <= 2 ? v : 0; return 1; }
int cx_bm_set_join(const void *c, int v) { cg_bmx_t *g = bmx(bm(c)); if (!g) return 0; g->join = v >= 0 && v <= 2 ? v : 0; return 1; }
int cx_bm_set_alpha(const void *c, double a) { cg_bmx_t *g = bmx(bm(c)); if (!g) return 0; g->galpha = a < 0 ? 0 : a > 1 ? 1 : a; return 1; }
int cx_bm_set_aa(const void *c, int v) { cg_bmx_t *g = bmx(bm(c)); if (!g) return 0; g->aa = v ? 1 : 0; return 1; }
int cx_bm_is(const void *c) { return bm(c) != 0; }

/* ---------------------------------------------------------------- recorte por máscara */
typedef struct { u8 *m; int w; } maskrow_t;
static void mask_row(void *ctx, int y, int x0, int x1, const uint8_t *cov) {
    maskrow_t *r = ctx;
    memcpy(r->m + (size_t)y * r->w + x0, cov, (size_t)(x1 - x0));
}
static void clip_with(cgobj_t *o, const cgr_poly *poly, int eo) {
    cg_bmx_t *g = o->gx;
    size_t n = (size_t)o->w * o->h;
    u8 *m = calloc(n ? n : 1, 1);
    if (!m) return;
    maskrow_t r = {m, (int)o->w};
    cgr_fill(poly, eo, o->cl, o->ct, o->cr, o->cbt, mask_row, &r);
    if (g->mask) for (size_t i = 0; i < n; i++) m[i] = (u8)(m[i] * g->mask[i] / 255);
    free(g->mask);
    g->mask = m;
    /* o retângulo de recorte encolhe para a caixa do que sobrou */
    double x0, y0, x1, y1;
    if (cgr_bbox(poly, &x0, &y0, &x1, &y1)) {
        int a = (int)floor(x0), b = (int)floor(y0), c = (int)ceil(x1), d = (int)ceil(y1);
        if (a > o->cl) o->cl = a;
        if (b > o->ct) o->ct = b;
        if (c < o->cr) o->cr = c;
        if (d < o->cbt) o->cbt = d;
    } else {
        o->cr = o->cl;
        o->cbt = o->ct;
    }
}
int cx_bm_clip(const void *c, int eo) {
    cgobj_t *o = bm(c); cg_bmx_t *g = bmx(o);
    if (!g) return 0;
    clip_with(o, &g->path, eo);
    cgr_clear(&g->path);
    return 1;
}

/* ======================================================================== API pública que usa o estado acima */
double tan(double);
void CGContextBeginPath(void *c);
void CGContextMoveToPoint(void *c, double x, double y);
void CGContextAddLineToPoint(void *c, double x, double y);
void CGContextAddCurveToPoint(void *c, double c1x, double c1y, double c2x, double c2y, double x, double y);
void CGContextStrokePath(void *c);
int cx_win_current(double *x, double *y);
void cx_win_clip(void);

void CGContextClip(void *c) { if (!cx_bm_clip(c, 0)) cx_win_clip(); }
void CGContextEOClip(void *c) { if (!cx_bm_clip(c, 1)) cx_win_clip(); }

static void user_of(cgobj_t *o, double dx, double dy, double *ux, double *uy) {
    double ia, ib, ic, id, itx, ity;
    if (!ctm_inverse(o, &ia, &ib, &ic, &id, &itx, &ity)) { *ux = dx; *uy = dy; return; }
    *ux = ia * dx + ic * dy + itx;
    *uy = ib * dx + id * dy + ity;
}
static CGRect user_rect_of_dev(cgobj_t *o, double x0, double y0, double x1, double y1) {
    double xs[4], ys[4];
    user_of(o, x0, y0, &xs[0], &ys[0]); user_of(o, x1, y0, &xs[1], &ys[1]);
    user_of(o, x0, y1, &xs[2], &ys[2]); user_of(o, x1, y1, &xs[3], &ys[3]);
    double a = xs[0], b = ys[0], cc = xs[0], d = ys[0];
    for (int i = 1; i < 4; i++) { if (xs[i] < a) a = xs[i]; if (xs[i] > cc) cc = xs[i]; if (ys[i] < b) b = ys[i]; if (ys[i] > d) d = ys[i]; }
    CGRect r = {{a, b}, {cc - a, d - b}};
    return r;
}
CGRect CGContextGetClipBoundingBox(void *c) {
    cgobj_t *o = bm(c);
    CGRect r = {{0, 0}, {0, 0}};
    if (!o) return r;
    if (o->cr <= o->cl || o->cbt <= o->ct) { r.origin.x = r.origin.y = INF; return r; }   /* CGRectNull */
    return user_rect_of_dev(o, o->cl, o->ct, o->cr, o->cbt);
}
CGRect CGContextConvertRectToUserSpace(void *c, CGRect r) {
    cgobj_t *o = bm(c);
    if (!o) return r;
    return user_rect_of_dev(o, r.origin.x, r.origin.y, r.origin.x + r.size.width, r.origin.y + r.size.height);
}
CGAffineTransform CGContextGetUserSpaceToDeviceSpaceTransform(void *c) { return CGContextGetCTM(c); }

void CGContextAddQuadCurveToPoint(void *c, double cx, double cy, double x, double y) {
    if (cx_bm_quad(c, cx, cy, x, y)) return;
    double x0, y0;
    if (!cx_win_current(&x0, &y0)) { CGContextMoveToPoint(c, x, y); return; }
    /* quadrática -> cúbica com os mesmos extremos */
    CGContextAddCurveToPoint(c, x0 + 2.0 / 3 * (cx - x0), y0 + 2.0 / 3 * (cy - y0), x + 2.0 / 3 * (cx - x), y + 2.0 / 3 * (cy - y), x, y);
}
/* arco por cúbicas de no máximo 90°; clockwise segue o sistema do usuário (y para cima no bitmap) */
void CGContextAddArc(void *c, double x, double y, double r, double a0, double a1, int clockwise) {
    const double ARC_PI = 3.14159265358979323846;
    double sweep = a1 - a0;
    if (clockwise) { while (sweep > 0) sweep -= 2 * ARC_PI; }
    else { while (sweep < 0) sweep += 2 * ARC_PI; }
    if (sweep > 2 * ARC_PI) sweep = 2 * ARC_PI;
    if (sweep < -2 * ARC_PI) sweep = -2 * ARC_PI;
    double sx = x + r * cos(a0), sy = y + r * sin(a0), cx0, cy0;
    int has = 0;
    cgobj_t *o = bm(c);
    if (o && o->gx && o->gx->path.n) has = 1;
    else if (!o && cx_win_current(&cx0, &cy0)) has = 1;
    if (has) CGContextAddLineToPoint(c, sx, sy); else CGContextMoveToPoint(c, sx, sy);
    int n = (int)ceil(fabs(sweep) / (ARC_PI / 2));
    if (n < 1) n = 1;
    double step = sweep / n, k = 4.0 / 3 * tan(step / 4);
    double a = a0;
    for (int i = 0; i < n; i++) {
        double b = a + step;
        double ca = cos(a), sa = sin(a), cb = cos(b), sb = sin(b);
        CGContextAddCurveToPoint(c, x + r * (ca - k * sa), y + r * (sa + k * ca), x + r * (cb + k * sb), y + r * (sb - k * cb), x + r * cb, y + r * sb);
        a = b;
    }
}
/* pares de pontos: cada par é um segmento solto */
void CGContextStrokeLineSegments(void *c, const CGPoint *pts, size_t n) {
    if (!pts) return;
    CGContextBeginPath(c);
    for (size_t i = 0; i + 1 < n; i += 2) {
        CGContextMoveToPoint(c, pts[i].x, pts[i].y);
        CGContextAddLineToPoint(c, pts[i + 1].x, pts[i + 1].y);
    }
    CGContextStrokePath(c);
}
void CGContextSetLineDash(void *c, double phase, const double *lengths, size_t n) {
    cg_bmx_t *g = bmx(bm(c));
    if (!g) return;
    g->phase = phase;
    g->ndash = 0;
    double total = 0;
    for (size_t i = 0; lengths && i < n && i < 16; i++) { g->dash[i] = lengths[i] < 0 ? 0 : lengths[i]; total += g->dash[i]; g->ndash++; }
    if (total <= 0) g->ndash = 0;               /* tudo zero: linha cheia */
    else if (g->ndash & 1 && g->ndash < 16) {   /* contagem ímpar repete o padrão, como o PostScript */
        int m = g->ndash;
        for (int i = 0; i < m && g->ndash < 16; i++) g->dash[g->ndash++] = g->dash[i];
    }
}
void CGContextSetMiterLimit(void *c, double limit) { cg_bmx_t *g = bmx(bm(c)); if (g) g->miter = limit; }
void CGContextSetAllowsAntialiasing(void *c, bool v) { cg_bmx_t *g = bmx(bm(c)); if (g && !v) g->aa = 0; else if (g) g->aa = 1; }
void CGContextSetShadowWithColor(void *c, CGSize off, double blur, void *color) {
    cg_bmx_t *g = bmx(bm(c));
    if (!g) return;
    g->sdx = off.width; g->sdy = off.height; g->sblur = blur;
    const cgcolor_t *k = color;
    g->scolor = k ? ((u32)(k->a * 255 + 0.5) << 24 | (u32)(k->r * 255 + 0.5) << 16 | (u32)(k->g * 255 + 0.5) << 8 | (u32)(k->b * 255 + 0.5)) : 0;
}
void CGContextSetShadow(void *c, CGSize off, double blur) {
    double comps[4] = {0, 0, 0, 1.0 / 3};      /* sombra padrão do macOS: preto a 1/3 */
    cgcolor_t k = {0, comps[0], comps[1], comps[2], comps[3]};
    CGContextSetShadowWithColor(c, off, blur, &k);
}

/* path atual devolvido no espaço do usuário */
static void *poly_to_cgpath(const cgr_poly *p) {
    void *path = CGPathCreateMutable();
    for (int k = 0; path && k < p->nsub; k++) {
        int a = p->sub[k], b = k + 1 < p->nsub ? p->sub[k + 1] : p->n;
        CGPathMoveToPoint(path, 0, p->pts[a].x, p->pts[a].y);
        for (int i = a + 1; i < b; i++) CGPathAddLineToPoint(path, 0, p->pts[i].x, p->pts[i].y);
        if (p->closed[k]) CGPathCloseSubpath(path);
    }
    return path;
}
void *CGContextCopyPath(void *c) {
    cgobj_t *o = bm(c);
    if (!o || !o->gx) return CGPathCreateMutable();
    cgr_poly u;
    double ia, ib, ic, id, itx, ity;
    cgr_init(&u);
    for (int k = 0; k < o->gx->path.nsub; k++) {
        int a = o->gx->path.sub[k], b = k + 1 < o->gx->path.nsub ? o->gx->path.sub[k + 1] : o->gx->path.n;
        cgr_move(&u, o->gx->path.pts[a].x, o->gx->path.pts[a].y);
        for (int i = a + 1; i < b; i++) cgr_line(&u, o->gx->path.pts[i].x, o->gx->path.pts[i].y);
        if (o->gx->path.closed[k]) cgr_close(&u);
    }
    if (ctm_inverse(o, &ia, &ib, &ic, &id, &itx, &ity)) cgr_transform(&u, ia, ib, ic, id, itx, ity);
    void *p = poly_to_cgpath(&u);
    cgr_free(&u);
    return p;
}
void CGContextReplacePathWithStrokedPath(void *c) {
    cgobj_t *o = bm(c);
    if (!o || !o->gx) return;
    cgr_poly out;
    cgr_init(&out);
    if (stroke_outline(o, &o->gx->path, &out)) {
        cgr_free(&o->gx->path);
        o->gx->path = out;                     /* o contorno vira o path (no dispositivo, como o resto) */
        return;
    }
    cgr_free(&out);
}

/* CGPath -> cgr_poly no espaço do próprio path */
static void cgpath_to_poly(const void *path, const CGAffineTransform *m, cgr_poly *out, double tol) {
    const cgpath_t *p = path;
    if (!p) return;
    u64 pi = 0;
    for (u64 i = 0; i < p->n; i++) {
        const CGPoint *q = p->pts + pi;
        CGPoint t[3];
        int k = pts_for[p->types[i]];
        for (int j = 0; j < k; j++) t[j] = m ? CGPointApplyAffineTransform(q[j], *m) : q[j];
        switch (p->types[i]) {
        case MOVE: cgr_move(out, t[0].x, t[0].y); break;
        case LINE: cgr_line(out, t[0].x, t[0].y); break;
        case QUAD: cgr_quad(out, t[0].x, t[0].y, t[1].x, t[1].y, tol); break;
        case CURVE: cgr_cubic(out, t[0].x, t[0].y, t[1].x, t[1].y, t[2].x, t[2].y, tol); break;
        case CLOSE: cgr_close(out); break;
        }
        pi += k;
    }
}
void *CGPathCreateCopyByStrokingPath(const void *path, const CGAffineTransform *m, double width, int cap, int join, double miter) {
    if (!path) return 0;
    cgr_poly in, out;
    cgr_stroke_t st;
    cgr_init(&in); cgr_init(&out);
    cgpath_to_poly(path, 0, &in, 0.05);
    memset(&st, 0, sizeof st);
    st.width = width; st.cap = cap; st.join = join; st.miter = miter; st.tol = 0.05;
    cgr_stroke(&in, &st, &out);
    if (m) cgr_transform(&out, m->a, m->b, m->c, m->d, m->tx, m->ty);
    void *r = poly_to_cgpath(&out);
    cgr_free(&in); cgr_free(&out);
    return r;
}
void *CGPathCreateCopyByDashingPath(const void *path, const CGAffineTransform *m, double phase, const double *lengths, size_t n) {
    if (!path) return 0;
    cgr_poly in, out;
    cgr_init(&in); cgr_init(&out);
    cgpath_to_poly(path, 0, &in, 0.05);
    if (lengths && n) cgr_dash(&in, lengths, (int)n, phase, &out);
    else { cgr_free(&out); out = in; cgr_init(&in); }
    if (m) cgr_transform(&out, m->a, m->b, m->c, m->d, m->tx, m->ty);
    void *r = poly_to_cgpath(&out);
    cgr_free(&in); cgr_free(&out);
    return r;
}

/* ---------------------------------------------------------------- gradientes */
typedef struct { cx_class_t *isa; u64 cfinfo; int n; double *loc; double *rgba; } cggrad_t;
static cx_class_t *grad_cls;
static void grad_dealloc(id self, SEL c) {
    cggrad_t *g = (cggrad_t *)self;
    free(g->loc); free(g->rgba);
    g->loc = g->rgba = 0;
    if (nsobject_dealloc) ((void (*)(id, SEL))nsobject_dealloc)(self, c);
}
static cggrad_t *grad_new(int n) {
    if (!grad_cls) {
        cgpath_class();                          /* garante o nsobject_dealloc */
        cx_class_t *k = objc_allocateClassPair(cx_class_named("NSObject"), "CGGradient", 0);
        if (!k) k = cx_class_named("CGGradient");
        class_addIvar(k, "_cg", sizeof(cggrad_t) - 8, 3, "?");
        class_addMethod(k, sel_registerName("dealloc"), (IMP)grad_dealloc, "v@:");
        objc_registerClassPair(k);
        grad_cls = k;
    }
    cggrad_t *g = (cggrad_t *)class_createInstance(grad_cls, 0);
    if (!g) return 0;
    g->n = n;
    g->loc = calloc((size_t)n, sizeof(double));
    g->rgba = calloc((size_t)n * 4, sizeof(double));
    if (!g->loc || !g->rgba) { objc_release((id)g); return 0; }
    return g;
}
long CFArrayGetCount(id a);
id CFArrayGetValueAtIndex(id a, long i);
void *CGGradientCreateWithColors(const void *space, id colors, const double *locations) {
    (void)space;
    long n = colors ? CFArrayGetCount(colors) : 0;
    if (n < 1) return 0;
    cggrad_t *g = grad_new((int)n);
    if (!g) return 0;
    for (long i = 0; i < n; i++) {
        const cgcolor_t *k = (const cgcolor_t *)CFArrayGetValueAtIndex(colors, i);
        g->loc[i] = locations ? locations[i] : (n > 1 ? (double)i / (n - 1) : 0);
        if (k) { g->rgba[4 * i] = k->r; g->rgba[4 * i + 1] = k->g; g->rgba[4 * i + 2] = k->b; g->rgba[4 * i + 3] = k->a; }
    }
    return g;
}
void *CGGradientCreateWithColorComponents(const void *space, const double *comps, const double *locations, size_t n) {
    if (!comps || !n) return 0;
    int nc = space ? (int)CGColorSpaceGetNumberOfComponents(space) : 3;
    cggrad_t *g = grad_new((int)n);
    if (!g) return 0;
    for (size_t i = 0; i < n; i++) {
        const double *c = comps + i * (nc + 1);
        g->loc[i] = locations ? locations[i] : (n > 1 ? (double)i / (n - 1) : 0);
        if (nc == 1) { g->rgba[4 * i] = g->rgba[4 * i + 1] = g->rgba[4 * i + 2] = c[0]; g->rgba[4 * i + 3] = c[1]; }
        else { g->rgba[4 * i] = c[0]; g->rgba[4 * i + 1] = c[1]; g->rgba[4 * i + 2] = c[2]; g->rgba[4 * i + 3] = c[3]; }
    }
    return g;
}
void *CGGradientRetain(void *g) { return g ? (void *)objc_retain((id)g) : 0; }
void CGGradientRelease(void *g) { if (g) objc_release((id)g); }
u64 CGGradientGetTypeID(void) { return 0x2A5; }

static void draw_gradient(void *c, const cggrad_t *gr, int radial, CGPoint p0, double r0, CGPoint p1, double r1, u32 opts) {
    cgobj_t *o = bm(c);
    if (!o || !o->data || !gr) return;
    double ia, ib, ic, id, itx, ity;
    if (!ctm_inverse(o, &ia, &ib, &ic, &id, &itx, &ity)) return;
    cgr_grad g = {gr->n, gr->loc, gr->rgba};
    double ga = o->gx ? o->gx->galpha : 1;
    u32 k = (u32)(ga * 255 + 0.5);
    for (int y = o->ct; y < o->cbt; y++)
        for (int x = o->cl; x < o->cr; x++) {
            double ux = ia * (x + 0.5) + ic * (y + 0.5) + itx, uy = ib * (x + 0.5) + id * (y + 0.5) + ity, t;
            int ok = radial ? cgr_radial_t(ux, uy, p0.x, p0.y, r0, p1.x, p1.y, r1, (int)opts, &t)
                            : cgr_linear_t(ux, uy, p0.x, p0.y, p1.x, p1.y, (int)opts, &t);
            if (ok) blend_at(o, x, y, scale_px(cgr_grad_at(&g, t), k));
        }
}
void CGContextDrawLinearGradient(void *c, void *g, CGPoint start, CGPoint end, u32 opts) { draw_gradient(c, g, 0, start, 0, end, 0, opts); }
void CGContextDrawRadialGradient(void *c, void *g, CGPoint c0, double r0, CGPoint c1, double r1, u32 opts) { draw_gradient(c, g, 1, c0, r0, c1, r1, opts); }

/* ---------------------------------------------------------------- camadas de transparência */
static int bm_premul(const cgobj_t *o) { u32 a = o->info & 31; return a == 1 || a == 2; }
void CGContextBeginTransparencyLayerWithRect(void *c, CGRect r, id aux) {
    (void)r; (void)aux;
    cgobj_t *o = bm(c); cg_bmx_t *g = bmx(o);
    if (!g || !o->data || g->nlayer >= 4) return;
    size_t n = (size_t)o->bpr * o->h;
    u8 *fresh = calloc(n ? n : 1, 1);
    if (!fresh) return;
    g->layer_data[g->nlayer] = o->data;
    g->layer_alpha[g->nlayer] = g->galpha;
    g->layer_owns[g->nlayer] = 1;
    g->nlayer++;
    o->data = fresh;
    cx_bm_save(c);
    g->galpha = 1;                              /* dentro da camada desenha opaco; o alfa vale na composição */
}
void CGContextBeginTransparencyLayer(void *c, id aux) { CGRect r = {{0, 0}, {0, 0}}; CGContextBeginTransparencyLayerWithRect(c, r, aux); }
void CGContextEndTransparencyLayer(void *c) {
    cgobj_t *o = bm(c); cg_bmx_t *g = o ? o->gx : 0;
    if (!g || !g->nlayer) return;
    cx_bm_restore(c);
    g->nlayer--;
    u8 *layer = o->data;
    o->data = g->layer_data[g->nlayer];
    u32 k = (u32)(g->layer_alpha[g->nlayer] * 255 + 0.5);
    int pm = bm_premul(o);
    /* compõe a camada inteira de uma vez: o recorte e a máscara do momento valem (blend_at) */
    u8 *saved_mask = g->mask;
    g->mask = 0;                                 /* o conteúdo da camada já respeitou o recorte ao ser desenhado */
    for (u32 y = 0; y < o->h; y++)
        for (u32 x = 0; x < o->w; x++) {
            const u8 *sp = layer + (size_t)y * o->bpr + (size_t)x * o->bpp;
            u32 p;
            if (o->bpp == 1) p = (u32)sp[0] << 24;
            else {
                p = px_argb(sp, o->info);
                if (!pm) { u32 a = p >> 24; p = a << 24 | (((p >> 16) & 255) * a / 255) << 16 | (((p >> 8) & 255) * a / 255) << 8 | ((p & 255) * a / 255); }
            }
            if (!(p >> 24)) continue;
            blend_at(o, (int)x, (int)y, scale_px(p, k));
        }
    g->mask = saved_mask;
    free(layer);
}

/* ---------------------------------------------------------------- padrões */
typedef struct { u32 version; void (*draw)(void *info, void *ctx); void (*release)(void *info); } cgpattern_cb;
typedef struct { cx_class_t *isa; u64 cfinfo; void *info; CGRect bounds; CGAffineTransform m; double xstep, ystep; int tiling, colored; cgpattern_cb cb; } cgpattern_t;
static cx_class_t *pat_cls;
static void pat_dealloc(id self, SEL c) {
    cgpattern_t *p = (cgpattern_t *)self;
    if (p->cb.release) p->cb.release(p->info);
    p->cb.release = 0;
    if (nsobject_dealloc) ((void (*)(id, SEL))nsobject_dealloc)(self, c);
}
void *CGPatternCreate(void *info, CGRect bounds, CGAffineTransform m, double xstep, double ystep, int tiling, bool colored, const cgpattern_cb *cb) {
    if (!pat_cls) {
        cgpath_class();
        cx_class_t *k = objc_allocateClassPair(cx_class_named("NSObject"), "CGPattern", 0);
        if (!k) k = cx_class_named("CGPattern");
        class_addIvar(k, "_cg", sizeof(cgpattern_t) - 8, 3, "?");
        class_addMethod(k, sel_registerName("dealloc"), (IMP)pat_dealloc, "v@:");
        objc_registerClassPair(k);
        pat_cls = k;
    }
    cgpattern_t *p = (cgpattern_t *)class_createInstance(pat_cls, 0);
    if (!p) return 0;
    p->info = info; p->bounds = bounds; p->m = m; p->xstep = xstep; p->ystep = ystep;
    p->tiling = tiling; p->colored = colored;
    if (cb) p->cb = *cb;
    return p;
}
void *CGPatternRetain(void *p) { return p ? (void *)objc_retain((id)p) : 0; }
void CGPatternRelease(void *p) { if (p) objc_release((id)p); }
u64 CGPatternGetTypeID(void) { return 0x2A6; }
void CGContextSetFillColorSpace(void *c, void *space) {
    cg_bmx_t *g = bmx(bm(c));
    if (!g) return;
    g->fill_space = space;
    if (!space || CGColorSpaceGetModel(space) != 6) g->pattern_on = 0;   /* kCGColorSpaceModelPattern */
}
void CGContextSetPatternPhase(void *c, CGSize phase) {
    cgobj_t *o = bm(c); cg_bmx_t *g = bmx(o);
    if (!g) return;
    g->pdx = o->ca * phase.width + o->cc * phase.height;
    g->pdy = o->cb * phase.width + o->cd * phase.height;
}
/* O ladrilho é desenhado uma vez num bitmap do tamanho do passo, na escala do contexto, e repetido. */
void CGContextSetFillPattern(void *c, void *pattern, const double *comps) {
    cgobj_t *o = bm(c); cg_bmx_t *g = bmx(o);
    cgpattern_t *p = pattern;
    if (!g || !p || !p->cb.draw) return;
    double sx = sqrt(o->ca * o->ca + o->cb * o->cb) * sqrt(p->m.a * p->m.a + p->m.b * p->m.b);
    double sy = sqrt(o->cc * o->cc + o->cd * o->cd) * sqrt(p->m.c * p->m.c + p->m.d * p->m.d);
    int tw = (int)ceil(fabs(p->xstep) * sx), th = (int)ceil(fabs(p->ystep) * sy);
    if (tw < 1 || th < 1 || tw > 2048 || th > 2048) return;
    void *tile = CGBitmapContextCreate(0, (size_t)tw, (size_t)th, 8, (size_t)tw * 4, 0, 1 /* premultipliedLast */);
    if (!tile) return;
    CGContextScaleCTM(tile, tw / fabs(p->xstep), th / fabs(p->ystep));
    CGContextTranslateCTM(tile, -p->bounds.origin.x, -p->bounds.origin.y);
    if (!p->colored && comps) {                  /* padrão sem cor própria: pinta com a cor dada */
        u32 a = (u32)(comps[3] * 255 + 0.5), r = (u32)(comps[0] * 255 + 0.5), gg = (u32)(comps[1] * 255 + 0.5), b = (u32)(comps[2] * 255 + 0.5);
        cx_bm_set_fill(tile, a << 24 | r << 16 | gg << 8 | b);
        cx_bm_set_stroke(tile, a << 24 | r << 16 | gg << 8 | b);
    }
    p->cb.draw(p->info, tile);
    cgobj_t *t = bm(tile);
    u32 *px = malloc((size_t)tw * th * 4);
    if (px) {
        for (int y = 0; y < th; y++)
            for (int x = 0; x < tw; x++) px[(size_t)y * tw + x] = px_argb(t->data + (size_t)y * t->bpr + (size_t)x * 4, t->info);
        free(g->tile);
        g->tile = px; g->tw = tw; g->th = th; g->pattern_on = 1;
    }
    CGContextRelease(tile);
}

/* ---------------------------------------------------------------- CGLayer: um bitmap com o tamanho pedido */
typedef struct { cx_class_t *isa; u64 cfinfo; void *ctx; CGSize size; } cglayer_t;
static cx_class_t *layer_cls;
static void layer_dealloc(id self, SEL c) {
    cglayer_t *l = (cglayer_t *)self;
    if (l->ctx) CGContextRelease(l->ctx);
    l->ctx = 0;
    if (nsobject_dealloc) ((void (*)(id, SEL))nsobject_dealloc)(self, c);
}
void *CGLayerCreateWithContext(void *ctx, CGSize size, id aux) {
    (void)aux;
    if (size.width <= 0 || size.height <= 0) return 0;
    cgobj_t *o = bm(ctx);
    double s = o ? cx_bm_scale(o) : 1;
    size_t w = (size_t)ceil(size.width * s), h = (size_t)ceil(size.height * s);
    void *lc = CGBitmapContextCreate(0, w, h, 8, w * 4, 0, 1 /* premultipliedLast */);
    if (!lc) return 0;
    if (s != 1) CGContextScaleCTM(lc, s, s);
    if (!layer_cls) {
        cgpath_class();
        cx_class_t *k = objc_allocateClassPair(cx_class_named("NSObject"), "CGLayer", 0);
        if (!k) k = cx_class_named("CGLayer");
        class_addIvar(k, "_cg", sizeof(cglayer_t) - 8, 3, "?");
        class_addMethod(k, sel_registerName("dealloc"), (IMP)layer_dealloc, "v@:");
        objc_registerClassPair(k);
        layer_cls = k;
    }
    cglayer_t *l = (cglayer_t *)class_createInstance(layer_cls, 0);
    if (!l) { CGContextRelease(lc); return 0; }
    l->ctx = lc;
    l->size = size;
    return l;
}
void *CGLayerGetContext(void *l) { return l ? ((cglayer_t *)l)->ctx : 0; }
CGSize CGLayerGetSize(void *l) { CGSize z = {0, 0}; return l ? ((cglayer_t *)l)->size : z; }
void *CGLayerRetain(void *l) { return l ? (void *)objc_retain((id)l) : 0; }
void CGLayerRelease(void *l) { if (l) objc_release((id)l); }
void CGContextDrawLayerInRect(void *c, CGRect r, void *l) {
    if (!l) return;
    void *img = CGBitmapContextCreateImage(((cglayer_t *)l)->ctx);
    if (!img) return;
    CGContextDrawImage(c, r, img);
    CGImageRelease(img);
}
void CGContextDrawLayerAtPoint(void *c, CGPoint p, void *l) {
    if (!l) return;
    CGRect r = {p, ((cglayer_t *)l)->size};
    CGContextDrawLayerInRect(c, r, l);
}

/* ---------------------------------------------------------------- bitmap, imagem, espaço de cor */
void *CGBitmapContextCreateWithData(void *data, size_t w, size_t h, size_t bpc, size_t bpr, void *space, u32 info,
                                    void (*release)(void *info, void *data), void *rinfo) {
    void *c = CGBitmapContextCreate(data, w, h, bpc, bpr, space, info);
    cgobj_t *o = bm(c);
    if (o && data && release) {                  /* só avisa quando o buffer é do chamador */
        o->rel_cb = (void (*)(void *, const void *, size_t))(void *)release;
        o->cb_info = rinfo;
    }
    return c;
}
void CGContextFlush(void *c) { (void)c; }      /* o bitmap já está atualizado: não há fila de desenho */
void CGContextSynchronize(void *c) { (void)c; }
void *CGImageCreateCopy(void *img) {
    cgobj_t *o = as_kind(img, CGK_IM);
    if (!o || !o->data) return 0;
    cgobj_t *n = cg_new(&IM_c, CGK_IM);
    if (!n) return 0;
    n->data = malloc((size_t)o->bpr * o->h);
    if (!n->data) { objc_release((id)n); return 0; }
    memcpy(n->data, o->data, (size_t)o->bpr * o->h);
    n->w = o->w; n->h = o->h; n->bpr = o->bpr; n->bpp = o->bpp; n->info = o->info; n->owns = 1; n->cb_size = o->cb_size;
    n->blend = o->blend;
    return n;
}
/* imagens que não saíram de um CGImageSource não têm tipo, como no macOS */
const void *CGImageGetUTType(void *img) { (void)img; return 0; }
int CGImageGetRenderingIntent(void *img) { cgobj_t *o = as_kind(img, CGK_IM); return o ? o->blend : 0; }

CSNAME(kCGColorSpaceITUR_709, "kCGColorSpaceITUR_709")
CSNAME(kCGColorSpaceITUR_2020, "kCGColorSpaceITUR_2020")
CSNAME(kCGColorSpaceDCIP3, "kCGColorSpaceDCIP3")
/* RGB e cinza podem ser destino de desenho; padrão não */
bool CGColorSpaceSupportsOutput(const void *s) { return s && (((const cgspace_t *)s)->model == 0 || ((const cgspace_t *)s)->model == 1); }
bool CGColorSpaceUsesITUR_2100TF(const void *s) { const char *n = s ? cfstr_c(((const cgspace_t *)s)->name) : ""; return strstr(n, "2100") != 0; }
bool CGColorSpaceIsPQBased(const void *s) { const char *n = s ? cfstr_c(((const cgspace_t *)s)->name) : ""; return strstr(n, "PQ") != 0; }
bool CGColorSpaceIsHLGBased(const void *s) { const char *n = s ? cfstr_c(((const cgspace_t *)s)->name) : ""; return strstr(n, "HLG") != 0; }
/* espaço de padrão: modelo 6, base guardada no nome (o mOS não tem outro uso para ele) */
static const void *pattern_base[MAX_SPACES];
static cgspace_t *pattern_spaces[MAX_SPACES];
static int npattern;
void *CGColorSpaceCreatePattern(void *base) {
    for (int i = 0; i < npattern; i++) if (pattern_base[i] == base) return objc_retain((id)pattern_spaces[i]);
    if (npattern == MAX_SPACES) return 0;
    void *any = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);   /* cria a classe CGColorSpace se preciso */
    if (any) CGColorSpaceRelease(any);
    cgspace_t *s = (cgspace_t *)class_createInstance(space_cls, 0);
    if (!s) return 0;
    s->name = 0;
    s->model = 6;                                /* kCGColorSpaceModelPattern */
    s->ncomp = 0;
    if (base) CGColorSpaceRetain(base);
    objc_retain((id)s);
    pattern_base[npattern] = base;
    pattern_spaces[npattern++] = s;
    return s;
}
void *CGColorSpaceGetBaseColorSpace(const void *s) {
    for (int i = 0; s && i < npattern; i++) if (pattern_spaces[i] == s) return (void *)pattern_base[i];
    return 0;
}
bool CGSizeEqualToSize(CGSize a, CGSize b) { return a.width == b.width && a.height == b.height; }
bool CGPointEqualToPoint(CGPoint a, CGPoint b) { return a.x == b.x && a.y == b.y; }

/* ---------------------------------------------------------------- PDF: documento, páginas e transformação
 * Não há renderizador de PDF no mOS (CGContextDrawPDFPage não desenha). O documento é lido de verdade:
 * cabeçalho %PDF-, número de páginas (objetos /Type /Page), MediaBox e /Encrypt no trailer. */
typedef struct { cx_class_t *isa; u64 cfinfo; u8 *bytes; size_t n; int pages, encrypted, unlocked; CGRect media[64]; } cgpdf_t;
typedef struct { cgpdf_t *doc; int index; } cgpdfpage_t;
static cx_class_t *pdf_cls;
static void pdf_dealloc(id self, SEL c) {
    cgpdf_t *d = (cgpdf_t *)self;
    free(d->bytes);
    d->bytes = 0;
    if (nsobject_dealloc) ((void (*)(id, SEL))nsobject_dealloc)(self, c);
}
static const u8 *mem_find(const u8 *h, size_t hn, const char *needle, size_t from) {
    size_t nn = strlen(needle);
    for (size_t i = from; nn && i + nn <= hn; i++) if (h[i] == (u8)needle[0] && !memcmp(h + i, needle, nn)) return h + i;
    return 0;
}
static double pdf_num(const u8 **p, const u8 *end) {
    while (*p < end && (**p == ' ' || **p == '\n' || **p == '\r' || **p == '\t' || **p == '[')) (*p)++;
    double s = 1, v = 0, f = 0.1;
    if (*p < end && **p == '-') { s = -1; (*p)++; }
    while (*p < end && **p >= '0' && **p <= '9') v = v * 10 + (*(*p)++ - '0');
    if (*p < end && **p == '.') { (*p)++; while (*p < end && **p >= '0' && **p <= '9') { v += (*(*p)++ - '0') * f; f /= 10; } }
    return s * v;
}
void *CGPDFDocumentCreateWithProvider(void *provider) {
    cgobj_t *pr = as_kind(provider, CGK_PR);
    if (!pr || !pr->data || pr->cb_size < 8 || memcmp(pr->data, "%PDF-", 5)) return 0;
    if (!pdf_cls) {
        cgpath_class();
        cx_class_t *k = objc_allocateClassPair(cx_class_named("NSObject"), "CGPDFDocument", 0);
        if (!k) k = cx_class_named("CGPDFDocument");
        class_addIvar(k, "_cg", sizeof(cgpdf_t) - 8, 3, "?");
        class_addMethod(k, sel_registerName("dealloc"), (IMP)pdf_dealloc, "v@:");
        objc_registerClassPair(k);
        pdf_cls = k;
    }
    cgpdf_t *d = (cgpdf_t *)class_createInstance(pdf_cls, 0);
    if (!d) return 0;
    d->n = (size_t)pr->cb_size;
    d->bytes = malloc(d->n);
    if (!d->bytes) { objc_release((id)d); return 0; }
    memcpy(d->bytes, pr->data, d->n);
    CGRect deflt = {{0, 0}, {612, 792}};        /* Carta americana, o padrão do PDF sem MediaBox */
    const u8 *mb = mem_find(d->bytes, d->n, "/MediaBox", 0);
    if (mb) {
        const u8 *q = mb + 9, *end = d->bytes + d->n;
        double a = pdf_num(&q, end), b = pdf_num(&q, end), c = pdf_num(&q, end), e = pdf_num(&q, end);
        if (c > a && e > b) { deflt.origin.x = a; deflt.origin.y = b; deflt.size.width = c - a; deflt.size.height = e - b; }
    }
    size_t at = 0;
    const u8 *hit;
    while ((hit = mem_find(d->bytes, d->n, "/Type", at))) {
        const u8 *q = hit + 5, *end = d->bytes + d->n;
        while (q < end && (*q == ' ' || *q == '\n' || *q == '\r')) q++;
        if (q + 5 <= end && !memcmp(q, "/Page", 5) && (q + 5 == end || q[5] != 's')) {
            if (d->pages < 64) d->media[d->pages] = deflt;
            d->pages++;
        }
        at = (size_t)(hit - d->bytes) + 5;
    }
    d->encrypted = mem_find(d->bytes, d->n, "/Encrypt", 0) != 0;
    d->unlocked = !d->encrypted;
    return d;
}
void *CGPDFDocumentCreateWithURL(id url) {
    void *p = CGDataProviderCreateWithURL(url);
    if (!p) return 0;
    void *d = CGPDFDocumentCreateWithProvider(p);
    CGDataProviderRelease(p);
    return d;
}
void *CGPDFDocumentRetain(void *d) { return d ? (void *)objc_retain((id)d) : 0; }
void CGPDFDocumentRelease(void *d) { if (d) objc_release((id)d); }
bool CGPDFDocumentIsEncrypted(void *d) { return d && ((cgpdf_t *)d)->encrypted; }
bool CGPDFDocumentIsUnlocked(void *d) { return d && ((cgpdf_t *)d)->unlocked; }
bool CGPDFDocumentUnlockWithPassword(void *d, const char *pw) {
    (void)pw;
    return d && !((cgpdf_t *)d)->encrypted;      /* sem decifrador: só documentos sem senha abrem */
}
size_t CGPDFDocumentGetNumberOfPages(void *d) { return d ? (size_t)((cgpdf_t *)d)->pages : 0; }
static cgpdfpage_t pdf_pages[64];
void *CGPDFDocumentGetPage(void *d, size_t n) {
    cgpdf_t *doc = d;
    if (!doc || n < 1 || (int)n > doc->pages || n > 64) return 0;
    for (int i = 0; i < 64; i++)                 /* "Get": a página vive enquanto o documento vive */
        if (pdf_pages[i].doc == doc && pdf_pages[i].index == (int)n) return &pdf_pages[i];
    for (int i = 0; i < 64; i++)
        if (!pdf_pages[i].doc) { pdf_pages[i].doc = doc; pdf_pages[i].index = (int)n; return &pdf_pages[i]; }
    pdf_pages[n % 64].doc = doc;
    pdf_pages[n % 64].index = (int)n;
    return &pdf_pages[n % 64];
}
CGRect CGPDFPageGetBoxRect(void *page, int box) {
    (void)box;                                    /* crop/bleed/trim/art caem na MediaBox quando ausentes */
    cgpdfpage_t *p = page;
    CGRect z = {{0, 0}, {0, 0}};
    if (!p || !p->doc || p->index < 1) return z;
    return p->index <= 64 ? p->doc->media[p->index - 1] : p->doc->media[0];
}
size_t CGPDFPageGetPageNumber(void *page) { return page ? (size_t)((cgpdfpage_t *)page)->index : 0; }
/* leva a caixa da página para dentro de rect (escala que cabe, centralizada; nunca amplia) e gira em múltiplos de 90° */
CGAffineTransform CGPDFPageGetDrawingTransform(void *page, int box, CGRect rect, int rotate, bool preserveAspect) {
    CGAffineTransform t = CGAffineTransformIdentity;
    CGRect b = CGPDFPageGetBoxRect(page, box);
    if (b.size.width <= 0 || b.size.height <= 0) return t;
    rotate = ((rotate % 360) + 360) % 360;
    rotate -= rotate % 90;
    double bw = b.size.width, bh = b.size.height;
    if (rotate == 90 || rotate == 270) { double k = bw; bw = bh; bh = k; }
    double sx = rect.size.width / bw, sy = rect.size.height / bh;
    if (preserveAspect) { double s = sx < sy ? sx : sy; sx = sy = s; }
    if (sx > 1) sx = 1;
    if (sy > 1) sy = 1;
    /* centro da caixa -> origem, gira, escala, origem -> centro do destino */
    t = CGAffineTransformMakeTranslation(rect.origin.x + rect.size.width / 2, rect.origin.y + rect.size.height / 2);
    t = CGAffineTransformScale(t, sx, sy);
    t = CGAffineTransformRotate(t, -rotate * 3.14159265358979323846 / 180);
    t = CGAffineTransformTranslate(t, -(b.origin.x + b.size.width / 2), -(b.origin.y + b.size.height / 2));
    return t;
}
