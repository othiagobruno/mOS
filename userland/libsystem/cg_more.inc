/* CoreGraphics do mOS — o resto da API de CGContext (incluído no fim do coregraphics.c, depois de
 * coregraphics_bm.inc). Recorte por máscara e por retângulos, consultas ao path, cores por espaço de cor,
 * padrão de traço, texto (CGContextSetFont/ShowGlyphs e os 8 modos de desenho), CGFunction e CGShading,
 * gradiente cônico, conversões de espaço e os CGDataProvider sequencial/direto/por arquivo.
 * As funções que só montam path chamam as CGContext* públicas: valem para bitmap e para janela. */

void CGContextAddRect(void *c, CGRect r);
void CGContextClosePath(void *c);
void CGContextFillPath(void *c);
void CGContextDrawPath(void *c, int mode);
void CGContextSetRGBFillColor(void *c, double r, double g, double b, double a);
void CGContextSetRGBStrokeColor(void *c, double r, double g, double b, double a);
void CGContextAddPath(void *c, const void *path);
void CGContextSetLineWidth(void *c, double w);
int cx_bm_set_stroke(const void *c, u32 argb);
void cx_bm_set_fill(const void *c, u32 argb);
void *CTFontCreateWithGraphicsFont(void *font, double size, const CGAffineTransform *m, id attrs);
void *CTFontCreateWithName(id name, double size, const CGAffineTransform *m);
void CTFontDrawGlyphs(void *font, const u16 *glyphs, const CGPoint *pos, size_t n, void *ctx);
double CTFontGetAdvancesForGlyphs(void *font, int orient, const u16 *glyphs, CGSize *adv, long n);
u8 CTFontGetGlyphsForCharacters(void *font, const u16 *chars, u16 *glyphs, long n);
void *CTFontCreatePathForGlyph(void *font, u16 glyph, const CGAffineTransform *m);
id cx_str(const char *s);
void CFRelease(void *o);

/* ---------------------------------------------------------------- recorte */
/* máscara: image mask (0 pinta) ou imagem cinza (branco = visível); esticada para rect no espaço do usuário */
static void pw_clip_mask(cgobj_t *o, CGRect rect, const cgobj_t *m);
void CGContextClipToMask(void *c, CGRect rect, void *mask) {
    cgobj_t *o = bm(c), *m = as_kind(mask, CGK_IM);
    cg_bmx_t *g = bmx(o);
    if (!g || !m || rect.size.width == 0 || rect.size.height == 0) return;
    double det = o->ca * o->cd - o->cb * o->cc;
    if (det > -1e-12 && det < 1e-12) return;
    size_t n = (size_t)o->w * o->h;
    u8 *nm = calloc(n ? n : 1, 1);
    if (!nm) return;
    int x0, y0, x1, y1;
    dev_box(o, rect.origin.x, rect.origin.y, rect.size.width, rect.size.height, &x0, &y0, &x1, &y1);
    clamp_box(o, &x0, &y0, &x1, &y1);
    int interp = !(m->w == (u32)(x1 - x0) && m->h == (u32)(y1 - y0));
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            double dx = x + 0.5 - o->cu, dy = y + 0.5 - o->cv;
            double ux = (o->cd * dx - o->cc * dy) / det, uy = (-o->cb * dx + o->ca * dy) / det;
            double u = (ux - rect.origin.x) / rect.size.width, v = (uy - rect.origin.y) / rect.size.height;
            if (u < 0 || u >= 1 || v < 0 || v >= 1) continue;
            u32 p = img_sample(m, u, v, interp), k;
            if (m->ismask || (m->info & 31) == AI_ONLY) k = p >> 24;
            else {
                u32 r, gg, b, a = un_pm(p, &r, &gg, &b);
                k = ((r * 77 + gg * 150 + b * 29 + 128) >> 8) * a / 255;
            }
            nm[(size_t)y * o->w + x] = (u8)k;
        }
    if (g->mask) for (size_t i = 0; i < n; i++) nm[i] = (u8)(nm[i] * g->mask[i] / 255);
    free(g->mask);
    g->mask = nm;
    if (o->pdf) pw_clip_mask(o, rect, m);
    if (x0 > o->cl) o->cl = x0;
    if (y0 > o->ct) o->ct = y0;
    if (x1 < o->cr) o->cr = x1;
    if (y1 < o->cbt) o->cbt = y1;
}
/* união dos retângulos, depois interseção com o recorte atual */
void CGContextClipToRects(void *c, const CGRect *rects, size_t n) {
    cgobj_t *o = bm(c);
    cg_bmx_t *g = bmx(o);
    if (!g || !rects) return;
    cgr_poly p;
    cgr_init(&p);
    for (size_t i = 0; i < n; i++) {
        CGRect r = CGRectStandardize(rects[i]);
        double xs[4], ys[4];
        ctm_of(o, r.origin.x, r.origin.y, &xs[0], &ys[0]);
        ctm_of(o, r.origin.x + r.size.width, r.origin.y, &xs[1], &ys[1]);
        ctm_of(o, r.origin.x + r.size.width, r.origin.y + r.size.height, &xs[2], &ys[2]);
        ctm_of(o, r.origin.x, r.origin.y + r.size.height, &xs[3], &ys[3]);
        /* todos com a mesma orientação: na regra não nula a união sai certa */
        double area = (xs[1] - xs[0]) * (ys[2] - ys[0]) - (xs[2] - xs[0]) * (ys[1] - ys[0]);
        cgr_move(&p, xs[0], ys[0]);
        if (area >= 0) for (int k = 1; k < 4; k++) cgr_line(&p, xs[k], ys[k]);
        else for (int k = 3; k >= 1; k--) cgr_line(&p, xs[k], ys[k]);
        cgr_close(&p);
    }
    clip_with(o, &p, 0);
    cgr_free(&p);
}

/* ---------------------------------------------------------------- path */
void CGContextAddRects(void *c, const CGRect *r, size_t n) { for (size_t i = 0; r && i < n; i++) CGContextAddRect(c, r[i]); }
void CGContextAddLines(void *c, const CGPoint *p, size_t n) {
    if (!p || !n) return;
    CGContextMoveToPoint(c, p[0].x, p[0].y);
    for (size_t i = 1; i < n; i++) CGContextAddLineToPoint(c, p[i].x, p[i].y);
}
static int bm_inverse_pt(cgobj_t *o, double dx, double dy, double *ux, double *uy) {
    double det = o->ca * o->cd - o->cb * o->cc;
    if (det > -1e-12 && det < 1e-12) return 0;
    dx -= o->cu; dy -= o->cv;
    *ux = (o->cd * dx - o->cc * dy) / det;
    *uy = (-o->cb * dx + o->ca * dy) / det;
    return 1;
}
bool CGContextIsPathEmpty(void *c) { cgobj_t *o = bm(c); return !o || !o->gx || o->gx->path.n == 0; }
CGPoint CGContextGetPathCurrentPoint(void *c) {
    cgobj_t *o = bm(c);
    CGPoint p = {0, 0};
    if (!o || !o->gx || !o->gx->path.n) return p;
    const cgr_poly *q = &o->gx->path;
    double ux, uy;
    /* path fechado: o ponto atual volta ao início do subpath */
    int k = q->nsub - 1, a = q->sub[k];
    double dx = q->closed[k] ? q->pts[a].x : q->pts[q->n - 1].x, dy = q->closed[k] ? q->pts[a].y : q->pts[q->n - 1].y;
    if (bm_inverse_pt(o, dx, dy, &ux, &uy)) { p.x = ux; p.y = uy; }
    return p;
}
CGRect CGContextGetPathBoundingBox(void *c) {
    cgobj_t *o = bm(c);
    if (!o || !o->gx || !o->gx->path.n) return CGRectNull;
    double b[4] = {INF, INF, -INF, -INF};
    const cgr_poly *q = &o->gx->path;
    for (int i = 0; i < q->n; i++) {
        double ux, uy;
        if (!bm_inverse_pt(o, q->pts[i].x, q->pts[i].y, &ux, &uy)) continue;
        if (ux < b[0]) b[0] = ux;
        if (uy < b[1]) b[1] = uy;
        if (ux > b[2]) b[2] = ux;
        if (uy > b[3]) b[3] = uy;
    }
    CGRect r = {{b[0], b[1]}, {b[2] - b[0], b[3] - b[1]}};
    return b[0] > b[2] ? CGRectNull : r;
}
/* teste de ponto no polígono do dispositivo: regra não nula ou par-ímpar */
static int poly_contains(const cgr_poly *q, double x, double y, int eo) {
    int wind = 0, cross = 0;
    for (int k = 0; k < q->nsub; k++) {
        int a = q->sub[k], b = k + 1 < q->nsub ? q->sub[k + 1] : q->n;
        for (int i = a; i < b; i++) {
            const cgr_pt *p0 = &q->pts[i], *p1 = &q->pts[i + 1 < b ? i + 1 : a];
            if ((p0->y <= y) != (p1->y <= y)) {
                double t = (y - p0->y) / (p1->y - p0->y), xi = p0->x + t * (p1->x - p0->x);
                if (xi > x) { cross++; wind += p1->y > p0->y ? 1 : -1; }
            }
        }
    }
    return eo ? (cross & 1) : wind != 0;
}
bool CGContextPathContainsPoint(void *c, CGPoint pt, int mode) {
    cgobj_t *o = bm(c);
    if (!o || !o->gx || !o->gx->path.n) return 0;
    double dx, dy;
    ctm_of(o, pt.x, pt.y, &dx, &dy);
    if (mode != 2 && poly_contains(&o->gx->path, dx, dy, mode == 1 || mode == 4)) return 1;
    if (mode >= 2) {
        cgr_poly out;
        cgr_init(&out);
        int in = stroke_outline(o, &o->gx->path, &out) && poly_contains(&out, dx, dy, 0);
        cgr_free(&out);
        return in;
    }
    return 0;
}
void CGContextEOFillPath(void *c) { if (!cx_bm_fill(c, 1)) CGContextDrawPath(c, 1); }
void CGContextDrawPathDirect(void *c, int mode, CGRect bounds) { (void)bounds; CGContextDrawPath(c, mode); }
/* arco tangente às retas (ponto atual → p1) e (p1 → p2), como CGPathAddArcToPoint */
void CGContextAddArcToPoint(void *c, double x1, double y1, double x2, double y2, double r) {
    CGPoint p0 = CGContextGetPathCurrentPoint(c);
    void *tmp = CGPathCreateMutable();
    if (!tmp) return;
    CGPathMoveToPoint(tmp, 0, p0.x, p0.y);
    CGPathAddArcToPoint(tmp, 0, x1, y1, x2, y2, r);
    cgpath_t *p = tmp;
    u64 pi = 1;                                     /* pula o move inicial: o path do contexto já está em p0 */
    for (u64 i = 1; i < p->n; i++) {
        if (p->types[i] == LINE) { CGContextAddLineToPoint(c, p->pts[pi].x, p->pts[pi].y); pi += 1; }
        else if (p->types[i] == CURVE) { CGContextAddCurveToPoint(c, p->pts[pi].x, p->pts[pi].y, p->pts[pi + 1].x, p->pts[pi + 1].y, p->pts[pi + 2].x, p->pts[pi + 2].y); pi += 3; }
        else if (p->types[i] == QUAD) { CGContextAddQuadCurveToPoint(c, p->pts[pi].x, p->pts[pi].y, p->pts[pi + 1].x, p->pts[pi + 1].y); pi += 2; }
        else if (p->types[i] == MOVE) { CGContextMoveToPoint(c, p->pts[pi].x, p->pts[pi].y); pi += 1; }
    }
    CGPathRelease(tmp);
}

/* ---------------------------------------------------------------- cores por espaço de cor */
/* componentes no espaço (cinza, RGB, CMYK; indexado pela tabela) + alfa → ARGB reto */
static u32 comps_argb(const cgspace_t *s, const double *v) {
    int model = s ? s->model : 1;
    double r, g, b, a;
    if (model == 5 && s && s->table && s->base) {
        int i = (int)(v[0] + 0.5), nb = (int)((const cgspace_t *)s->base)->ncomp;
        if (i < 0) i = 0;
        if (i >= s->ntable) i = s->ntable - 1;
        double t[4];
        for (int k = 0; k < nb && k < 4; k++) t[k] = s->table[i * nb + k] / 255.0;
        t[nb < 4 ? nb : 3] = v[1];
        return comps_argb(s->base, t);
    }
    if (model == 0) { r = g = b = v[0]; a = v[1]; }
    else if (model == 2) { r = (1 - v[0]) * (1 - v[3]); g = (1 - v[1]) * (1 - v[3]); b = (1 - v[2]) * (1 - v[3]); a = v[4]; }
    else { r = v[0]; g = v[1]; b = v[2]; a = v[3]; }
    double q[4] = {a, r, g, b};
    u32 o = 0;
    for (int i = 0; i < 4; i++) o |= (u32)((q[i] < 0 ? 0 : q[i] > 1 ? 1 : q[i]) * 255 + 0.5) << (24 - 8 * i);
    return o;
}
void CGContextSetFillColor(void *c, const double *comps) {
    cgobj_t *o = bm(c);
    if (!comps) return;
    const cgspace_t *s = o && o->gx ? o->gx->fill_space : 0;
    if (s && s->model == 6) return;                 /* espaço de padrão: a cor vem de CGContextSetFillPattern */
    u32 argb = comps_argb(s, comps);
    if (o) { cx_bm_set_fill(c, argb); if (o->gx) o->gx->pattern_on = 0; return; }
    CGContextSetRGBFillColor(c, ((argb >> 16) & 255) / 255.0, ((argb >> 8) & 255) / 255.0, (argb & 255) / 255.0, (argb >> 24) / 255.0);
}
void CGContextSetStrokeColorSpace(void *c, void *space) {
    cg_bmx_t *g = bmx(bm(c));
    const cgspace_t *s = space;
    if (!g) return;
    g->stroke_space = space;
    g->spattern_on = 0;
    if (s && s->model != 6) cx_bm_set_stroke(c, s->model == 2 ? 0xFF000000u : 0xFF000000u);   /* a cor inicial é o preto do espaço */
}
void CGContextSetStrokeColor(void *c, const double *comps) {
    cgobj_t *o = bm(c);
    if (!comps) return;
    const cgspace_t *s = o && o->gx ? o->gx->stroke_space : 0;
    if (s && s->model == 6) return;
    u32 argb = comps_argb(s, comps);
    if (o) { cx_bm_set_stroke(c, argb); if (o->gx) o->gx->spattern_on = 0; return; }
    CGContextSetRGBStrokeColor(c, ((argb >> 16) & 255) / 255.0, ((argb >> 8) & 255) / 255.0, (argb & 255) / 255.0, (argb >> 24) / 255.0);
}
void CGContextSetCMYKFillColor(void *c, double C, double M, double Y, double K, double a) {
    double v[5] = {C, M, Y, K, a};
    cgspace_t cm = {0};
    cm.model = 2; cm.ncomp = 4;
    u32 argb = comps_argb(&cm, v);
    CGContextSetRGBFillColor(c, ((argb >> 16) & 255) / 255.0, ((argb >> 8) & 255) / 255.0, (argb & 255) / 255.0, a);
}
void CGContextSetCMYKStrokeColor(void *c, double C, double M, double Y, double K, double a) {
    double v[5] = {C, M, Y, K, a};
    cgspace_t cm = {0};
    cm.model = 2; cm.ncomp = 4;
    u32 argb = comps_argb(&cm, v);
    CGContextSetRGBStrokeColor(c, ((argb >> 16) & 255) / 255.0, ((argb >> 8) & 255) / 255.0, (argb & 255) / 255.0, a);
}
/* padrão no traço: o ladrilho é montado como o do preenchimento e guardado à parte */
void CGContextSetStrokePattern(void *c, void *pattern, const double *comps) {
    cgobj_t *o = bm(c);
    cg_bmx_t *g = bmx(o);
    if (!g) return;
    u32 *ft = g->tile; int fw = g->tw, fh = g->th, fon = g->pattern_on;
    g->tile = 0;
    CGContextSetFillPattern(c, pattern, comps);
    free(g->stile);
    g->stile = g->tile; g->stw = g->tw; g->sth = g->th; g->spattern_on = g->tile != 0;
    g->tile = ft; g->tw = fw; g->th = fh; g->pattern_on = fon;
}
void CGContextSetRenderingIntent(void *c, int intent) { (void)c; (void)intent; }   /* o mOS desenha em sRGB: a intenção não muda o pixel */
void CGContextSetFlatness(void *c, double f) { cg_bmx_t *g = bmx(bm(c)); if (g) g->flat = f; }
void CGContextSetEDRTargetHeadroom(void *c, float h) { (void)c; (void)h; }          /* sem tela HDR: headroom 1 */

/* ---------------------------------------------------------------- conversões de espaço */
CGPoint CGContextConvertPointToDeviceSpace(void *c, CGPoint p) { return CGPointApplyAffineTransform(p, CGContextGetCTM(c)); }
CGPoint CGContextConvertPointToUserSpace(void *c, CGPoint p) { return CGPointApplyAffineTransform(p, CGAffineTransformInvert(CGContextGetCTM(c))); }
CGRect CGContextConvertRectToDeviceSpace(void *c, CGRect r) { return CGRectApplyAffineTransform(r, CGContextGetCTM(c)); }

/* ---------------------------------------------------------------- texto */
/* CTFont do CGFont + tamanho atuais (recriado quando um dos dois muda) */
static void *text_font(cgobj_t *o) {
    cg_bmx_t *g = o ? o->gx : 0;
    if (!g || !g->font) return 0;
    if (!g->ctfont || g->ctfont_size != g->fsize || g->ctfont_src != g->font) {
        if (g->ctfont) CFRelease(g->ctfont);
        g->ctfont = CTFontCreateWithGraphicsFont(g->font, g->fsize > 0 ? g->fsize : 12, 0, 0);
        g->ctfont_size = g->fsize;
        g->ctfont_src = g->font;
    }
    return g->ctfont;
}
void CGContextSetFont(void *c, void *font) {
    cg_bmx_t *g = bmx(bm(c));
    if (!g) return;
    if (font) objc_retain((id)font);
    if (g->font) objc_release((id)g->font);
    g->font = font;
}
void CGContextSetFontSize(void *c, double s) { cg_bmx_t *g = bmx(bm(c)); if (g) g->fsize = s; }
void CGContextSetCharacterSpacing(void *c, double s) { cg_bmx_t *g = bmx(bm(c)); if (g) g->charsp = s; }
void CGContextSetTextDrawingMode(void *c, int mode) { cg_bmx_t *g = bmx(bm(c)); if (g && mode >= 0 && mode <= 7) g->tmode = mode; }
void CGContextSetTextPosition(void *c, double x, double y) { cgobj_t *o = bm(c); if (o) { o->tx = x; o->ty = y; } }
CGPoint CGContextGetTextPosition(void *c) { cgobj_t *o = bm(c); CGPoint p = {o ? o->tx : 0, o ? o->ty : 0}; return p; }

/* glifos em posições do espaço de texto, com o modo de desenho do contexto */
static void show_glyphs(cgobj_t *o, void *font, const u16 *glyphs, const CGPoint *pos, size_t n) {
    cg_bmx_t *g = o->gx;
    int mode = g ? g->tmode : 0;
    if (!font || !n || mode == 3) return;
    int fill = mode == 0 || mode == 2 || mode == 4 || mode == 6, stroke = mode == 1 || mode == 2 || mode == 5 || mode == 6;
    int clip = mode >= 4;
    if (fill) {
        int sh = shadow_begin(o);
        CTFontDrawGlyphs(font, glyphs, pos, n, o);
        shadow_end(o, sh);
    }
    if (stroke || clip) {
        /* contornos dos glifos no espaço do usuário: matriz de texto aplicada à posição e ao glifo */
        CGContextBeginPath(o);
        for (size_t i = 0; i < n; i++) {
            CGAffineTransform m = {o->ta, o->tb, o->tc, o->td,
                                   o->ta * pos[i].x + o->tc * pos[i].y + o->tx, o->tb * pos[i].x + o->td * pos[i].y + o->ty};
            void *p = CTFontCreatePathForGlyph(font, glyphs[i], &m);
            if (p) { CGContextAddPath(o, p); CGPathRelease(p); }
        }
        if (stroke && clip) {
            void *keep = CGContextCopyPath(o);
            CGContextStrokePath(o);
            if (keep) { CGContextAddPath(o, keep); CGPathRelease(keep); }
            CGContextClip(o);
        } else if (stroke) CGContextStrokePath(o);
        else CGContextClip(o);
    }
}
/* posições relativas à posição de texto (como no macOS: o espaço de texto começa no text position) */
void CGContextShowGlyphsAtPositions(void *c, const u16 *glyphs, const CGPoint *pos, size_t n) {
    cgobj_t *o = bm(c);
    if (!o || !glyphs || !pos) return;
    show_glyphs(o, text_font(o), glyphs, pos, n);
}
/* avanços da fonte (mais o espaçamento de caracteres) a partir da posição de texto, que anda */
void CGContextShowGlyphsWithAdvances(void *c, const u16 *glyphs, const CGSize *adv, size_t n) {
    cgobj_t *o = bm(c);
    if (!o || !glyphs || !n) return;
    CGPoint *pos = malloc(n * sizeof *pos);
    if (!pos) return;
    double x = 0, y = 0;
    for (size_t i = 0; i < n; i++) { pos[i].x = x; pos[i].y = y; x += adv[i].width; y += adv[i].height; }
    show_glyphs(o, text_font(o), glyphs, pos, n);
    /* a posição de texto anda o total no espaço de texto */
    o->tx += o->ta * x + o->tc * y;
    o->ty += o->tb * x + o->td * y;
    free(pos);
}
void CGContextShowGlyphs(void *c, const u16 *glyphs, size_t n) {
    cgobj_t *o = bm(c);
    void *f = text_font(o);
    if (!o || !f || !glyphs || !n) return;
    CGSize *adv = calloc(n, sizeof *adv);
    if (!adv) return;
    CTFontGetAdvancesForGlyphs(f, 0, glyphs, adv, (long)n);
    for (size_t i = 0; i < n; i++) adv[i].width += o->gx ? o->gx->charsp : 0;
    CGContextShowGlyphsWithAdvances(c, glyphs, adv, n);
    free(adv);
}
void CGContextShowGlyphsAtPoint(void *c, double x, double y, const u16 *glyphs, size_t n) {
    CGContextSetTextPosition(c, x, y);
    CGContextShowGlyphs(c, glyphs, n);
}
/* CGContextSelectFont/ShowText (API antiga): bytes MacRoman/ASCII viram caracteres e depois glifos */
void CGContextSelectFont(void *c, const char *name, double size, int encoding) {
    cgobj_t *o = bm(c);
    cg_bmx_t *g = bmx(o);
    (void)encoding;
    if (!g || !name) return;
    void *f = CTFontCreateWithName(cx_str(name), size, 0);
    if (!f) return;
    if (g->ctfont) CFRelease(g->ctfont);
    g->ctfont = f;
    g->fsize = size;
    g->ctfont_size = size;
    g->ctfont_src = g->font;                         /* SetFont depois troca de novo */
}
static const u16 macroman_hi[128] = {
    0xC4, 0xC5, 0xC7, 0xC9, 0xD1, 0xD6, 0xDC, 0xE1, 0xE0, 0xE2, 0xE4, 0xE3, 0xE5, 0xE7, 0xE9, 0xE8,
    0xEA, 0xEB, 0xED, 0xEC, 0xEE, 0xEF, 0xF1, 0xF3, 0xF2, 0xF4, 0xF6, 0xF5, 0xFA, 0xF9, 0xFB, 0xFC,
    0x2020, 0xB0, 0xA2, 0xA3, 0xA7, 0x2022, 0xB6, 0xDF, 0xAE, 0xA9, 0x2122, 0xB4, 0xA8, 0x2260, 0xC6, 0xD8,
    0x221E, 0xB1, 0x2264, 0x2265, 0xA5, 0xB5, 0x2202, 0x2211, 0x220F, 0x3C0, 0x222B, 0xAA, 0xBA, 0x3A9, 0xE6, 0xF8,
    0xBF, 0xA1, 0xAC, 0x221A, 0x192, 0x2248, 0x2206, 0xAB, 0xBB, 0x2026, 0xA0, 0xC0, 0xC3, 0xD5, 0x152, 0x153,
    0x2013, 0x2014, 0x201C, 0x201D, 0x2018, 0x2019, 0xF7, 0x25CA, 0xFF, 0x178, 0x2044, 0x20AC, 0x2039, 0x203A, 0xFB01, 0xFB02,
    0x2021, 0xB7, 0x201A, 0x201E, 0x2030, 0xC2, 0xCA, 0xC1, 0xCB, 0xC8, 0xCD, 0xCE, 0xCF, 0xCC, 0xD3, 0xD4,
    0xF8FF, 0xD2, 0xDA, 0xDB, 0xD9, 0x131, 0x2C6, 0x2DC, 0xAF, 0x2D8, 0x2D9, 0x2DA, 0xB8, 0x2DD, 0x2DB, 0x2C7};
void CGContextShowText(void *c, const char *s, size_t n) {
    cgobj_t *o = bm(c);
    void *f = text_font(o);
    if (!o || !f || !s || !n) return;
    u16 *ch = malloc(n * 2), *gl = calloc(n, 2);
    if (!ch || !gl) { free(ch); free(gl); return; }
    for (size_t i = 0; i < n; i++) { u8 b = (u8)s[i]; ch[i] = b < 128 ? b : macroman_hi[b - 128]; }
    CTFontGetGlyphsForCharacters(f, ch, gl, (long)n);
    CGContextShowGlyphs(c, gl, n);
    free(ch);
    free(gl);
}
void CGContextShowTextAtPoint(void *c, double x, double y, const char *s, size_t n) {
    CGContextSetTextPosition(c, x, y);
    CGContextShowText(c, s, n);
}

/* ---------------------------------------------------------------- CGFunction e CGShading */
typedef struct { u32 version; void (*evaluate)(void *info, const double *in, double *out); void (*release)(void *info); } cgfunc_cb;
typedef struct { cx_class_t *isa; u64 cfinfo; void *info; int ndom, nrange; double dom[8], range[32]; cgfunc_cb cb; } cgfunc_t;
typedef struct { cx_class_t *isa; u64 cfinfo; int radial; void *space; CGPoint p0, p1; double r0, r1; void *fn; int e0, e1; } cgshade_t;
static cx_class_t *func_cls, *shade_cls;
static void func_dealloc(id self, SEL s) { (void)s; cgfunc_t *f = (cgfunc_t *)self; if (f->cb.release) f->cb.release(f->info); cx_object_free(self); }
static void shade_dealloc(id self, SEL s) {
    (void)s;
    cgshade_t *h = (cgshade_t *)self;
    if (h->fn) objc_release((id)h->fn);
    if (h->space) objc_release((id)h->space);
    cx_object_free(self);
}
static cx_class_t *mkcls(const char *name, size_t sz, void *dealloc) {
    cx_class_t *k = objc_allocateClassPair(cx_class_named("NSObject"), name, 0);
    if (!k) return cx_class_named(name);
    class_addIvar(k, "_cg", sz - 8, 3, "?");
    class_addMethod(k, sel_registerName("dealloc"), (IMP)dealloc, "v@:");
    objc_registerClassPair(k);
    return k;
}
void *CGFunctionCreate(void *info, size_t ndom, const double *dom, size_t nrange, const double *range, const cgfunc_cb *cb) {
    if (!cb || !cb->evaluate || ndom < 1 || ndom > 4 || nrange > 16) return 0;
    if (!func_cls) func_cls = mkcls("CGFunction", sizeof(cgfunc_t), func_dealloc);
    cgfunc_t *f = (cgfunc_t *)class_createInstance(func_cls, 0);
    if (!f) return 0;
    f->info = info; f->ndom = (int)ndom; f->nrange = (int)nrange; f->cb = *cb;
    for (size_t i = 0; i < 2 * ndom; i++) f->dom[i] = dom ? dom[i] : (i & 1);
    for (size_t i = 0; range && i < 2 * nrange; i++) f->range[i] = range[i];
    return f;
}
void *CGFunctionRetain(void *f) { return f ? (void *)objc_retain((id)f) : 0; }
void CGFunctionRelease(void *f) { if (f) objc_release((id)f); }
u64 CGFunctionGetTypeID(void) { return 0x2A9; }
static void *shade_new(int radial, void *space, CGPoint p0, double r0, CGPoint p1, double r1, void *fn, bool e0, bool e1) {
    if (!fn) return 0;
    if (!shade_cls) shade_cls = mkcls("CGShading", sizeof(cgshade_t), shade_dealloc);
    cgshade_t *h = (cgshade_t *)class_createInstance(shade_cls, 0);
    if (!h) return 0;
    h->radial = radial; h->p0 = p0; h->p1 = p1; h->r0 = r0; h->r1 = r1; h->e0 = e0; h->e1 = e1;
    h->fn = objc_retain((id)fn);
    h->space = space ? objc_retain((id)space) : 0;
    return h;
}
void *CGShadingCreateAxial(void *space, CGPoint start, CGPoint end, void *fn, bool e0, bool e1) { return shade_new(0, space, start, 0, end, 0, fn, e0, e1); }
void *CGShadingCreateRadial(void *space, CGPoint start, double r0, CGPoint end, double r1, void *fn, bool e0, bool e1) { return shade_new(1, space, start, r0, end, r1, fn, e0, e1); }
void *CGShadingRetain(void *s) { return s ? (void *)objc_retain((id)s) : 0; }
void CGShadingRelease(void *s) { if (s) objc_release((id)s); }
u64 CGShadingGetTypeID(void) { return 0x2AA; }
/* a função vira uma tabela de 256 cores (pré-multiplicadas) no domínio [d0, d1] */
static void shade_lut(const cgshade_t *h, u32 *lut) {
    const cgfunc_t *f = h->fn;
    const cgspace_t *s = h->space;
    int nc = s ? (int)s->ncomp : 3;
    for (int i = 0; i < 256; i++) {
        double t = f->dom[0] + (f->dom[1] - f->dom[0]) * i / 255.0, in[4] = {t, 0, 0, 0}, out[16] = {0};
        f->cb.evaluate(f->info, in, out);
        double v[5] = {0, 0, 0, 0, 1};
        for (int k = 0; k < nc && k < 4; k++) v[k] = out[k];
        v[nc < 4 ? nc : 4] = f->nrange > nc ? out[nc] : 1;
        u32 c = comps_argb(s, v);
        lut[i] = premul(c, 1);
    }
}
void CGContextDrawShading(void *c, void *shading) {
    cgobj_t *o = bm(c);
    cgshade_t *h = shading;
    if (!o || !o->data || !h) return;
    double ia, ib, ic, id, itx, ity;
    if (!ctm_inverse(o, &ia, &ib, &ic, &id, &itx, &ity)) return;
    u32 lut[256];
    shade_lut(h, lut);
    if (o->pdf && pw_shading(o, lut, ((const cgfunc_t *)h->fn)->dom[0], ((const cgfunc_t *)h->fn)->dom[1], h->radial, h->p0, h->r0, h->p1, h->r1, h->e0, h->e1)) return;
    int flags = (h->e0 ? 1 : 0) | (h->e1 ? 2 : 0);
    u32 k = (u32)((o->gx ? o->gx->galpha : 1) * 255 + 0.5);
    int sh = shadow_begin(o);
    for (int y = o->ct; y < o->cbt; y++)
        for (int x = o->cl; x < o->cr; x++) {
            double ux = ia * (x + 0.5) + ic * (y + 0.5) + itx, uy = ib * (x + 0.5) + id * (y + 0.5) + ity, t;
            int ok = h->radial ? cgr_radial_t(ux, uy, h->p0.x, h->p0.y, h->r0, h->p1.x, h->p1.y, h->r1, flags, &t)
                               : cgr_linear_t(ux, uy, h->p0.x, h->p0.y, h->p1.x, h->p1.y, flags, &t);
            if (!ok) continue;
            int i = (int)((t < 0 ? 0 : t > 1 ? 1 : t) * 255 + 0.5);
            blend_at(o, x, y, scale_px(lut[i], k));
        }
    shadow_end(o, sh);
}
/* gradiente cônico: o ângulo em volta do centro, a partir de 'angle', vai de 0 a 1 */
void CGContextDrawConicGradient(void *c, void *gradient, CGPoint center, double angle) {
    cgobj_t *o = bm(c);
    const cggrad_t *gr = gradient;
    if (!o || !o->data || !gr) return;
    double ia, ib, ic, id, itx, ity;
    if (!ctm_inverse(o, &ia, &ib, &ic, &id, &itx, &ity)) return;
    cgr_grad g = {gr->n, gr->loc, gr->rgba};
    u32 k = (u32)((o->gx ? o->gx->galpha : 1) * 255 + 0.5);
    int sh = shadow_begin(o);
    for (int y = o->ct; y < o->cbt; y++)
        for (int x = o->cl; x < o->cr; x++) {
            double ux = ia * (x + 0.5) + ic * (y + 0.5) + itx - center.x, uy = ib * (x + 0.5) + id * (y + 0.5) + ity - center.y;
            double a = atan2(uy, ux) - angle, tau = 6.283185307179586;
            while (a < 0) a += tau;
            while (a >= tau) a -= tau;
            blend_at(o, x, y, scale_px(cgr_grad_at(&g, a / tau), k));
        }
    shadow_end(o, sh);
}

/* ---------------------------------------------------------------- CGDataProvider */
void *CGDataProviderCreateWithFilename(const char *path) {
    if (!path) return 0;
    FILE_ *f = fopen(path, "rb");
    if (!f) return 0;
    size_t cap = 64 * 1024, n = 0;
    u8 *buf = malloc(cap);
    while (buf) {
        size_t got = fread(buf + n, 1, cap - n, f);
        n += got;
        if (n < cap) break;
        if (cap >= (1u << 30)) { free(buf); buf = 0; break; }
        u8 *nb = realloc(buf, cap * 2);
        if (!nb) { free(buf); buf = 0; break; }
        buf = nb;
        cap *= 2;
    }
    fclose(f);
    if (!buf || !n) { free(buf); return 0; }
    void *p = CGDataProviderCreateWithData(0, buf, n, cg_free_bytes);
    if (!p) free(buf);
    return p;
}
/* sequencial: lê tudo de uma vez pelos callbacks (o provedor do mOS é sempre um bloco de memória) */
typedef struct { u32 version; size_t (*getBytes)(void *, void *, size_t); long (*skip)(void *, long); void (*rewind)(void *); void (*release)(void *); } cgseq_cb;
void *CGDataProviderCreateSequential(void *info, const cgseq_cb *cb) {
    if (!cb || !cb->getBytes) return 0;
    if (cb->rewind) cb->rewind(info);
    size_t cap = 64 * 1024, n = 0;
    u8 *buf = malloc(cap);
    while (buf) {
        if (n == cap) {
            if (cap >= (1u << 30)) break;
            u8 *nb = realloc(buf, cap * 2);
            if (!nb) break;
            buf = nb;
            cap *= 2;
        }
        size_t got = cb->getBytes(info, buf + n, cap - n);
        if (!got) break;
        n += got;
    }
    if (cb->release) cb->release(info);
    if (!buf || !n) { free(buf); return 0; }
    void *p = CGDataProviderCreateWithData(0, buf, n, cg_free_bytes);
    if (!p) free(buf);
    if (p) ((cgobj_t *)p)->fill = 0;
    return p;
}
typedef struct { u32 version; const void *(*getBytePointer)(void *); void (*releaseBytePointer)(void *, const void *);
                 size_t (*getBytesAtPosition)(void *, void *, long, size_t); void (*releaseInfo)(void *); } cgdirect_cb;
void *CGDataProviderCreateDirect(void *info, long size, const cgdirect_cb *cb) {
    if (!cb || size <= 0 || size > (1l << 30)) return 0;
    u8 *buf = malloc((size_t)size);
    if (!buf) return 0;
    size_t got = 0;
    if (cb->getBytePointer) {
        const void *p = cb->getBytePointer(info);
        if (p) { memcpy(buf, p, (size_t)size); got = (size_t)size; }
        if (cb->releaseBytePointer) cb->releaseBytePointer(info, p);
    }
    if (!got && cb->getBytesAtPosition)
        while (got < (size_t)size) {
            size_t k = cb->getBytesAtPosition(info, buf + got, (long)got, (size_t)size - got);
            if (!k) break;
            got += k;
        }
    if (cb->releaseInfo) cb->releaseInfo(info);
    if (!got) { free(buf); return 0; }
    void *p = CGDataProviderCreateWithData(0, buf, got, cg_free_bytes);
    if (!p) free(buf);
    return p;
}
void *CGDataProviderGetInfo(void *p) { cgobj_t *o = as_kind(p, CGK_PR); return o && o->rel_cb != cg_free_bytes ? o->cb_info : 0; }
u64 CGDataProviderGetTypeID(void) { return 0x2A7; }
u64 CGLayerGetTypeID(void) { return 0x2AB; }
