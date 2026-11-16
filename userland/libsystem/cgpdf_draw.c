/*
 * mOS CoreGraphics — CGContextDrawPDFPage: interpretador do conteúdo de página (ISO 32000-1 §8–9) sobre
 * o próprio CoreGraphics. Estado gráfico, paths e recortes, cores em todos os espaços, padrões (ladrilho
 * desenhado célula a célula na matriz exata; shading), imagens e imagem inline, shadings 1–7, XObject de
 * form (com grupo de transparência), ExtGState (alfas, blend mode, máscara suave em contexto de bitmap),
 * texto (fontes embutidas TrueType/CFF/Type1 lidas aqui, Type3, fonte do sistema; 8 modos; vertical),
 * conteúdo opcional e aparência das anotações.
 */
#include "foundation.h"
#include "libc.h"
#include "pdf_core.h"
#include <stdbool.h>

typedef double CGFloat;
typedef struct { CGFloat x, y; } CGPoint;
typedef struct { CGFloat width, height; } CGSize;
typedef struct { CGPoint origin; CGSize size; } CGRect;
typedef struct { CGFloat a, b, c, d, tx, ty; } CGAffineTransform;
double sqrt(double), sin(double), cos(double), atan2(double, double), fabs(double), floor(double), ceil(double), pow(double, double),
    log(double), exp(double);
void CFRelease(void *o);
static int p_hexv(int c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }

#include "pdf_func.inc"
#include "pdf_color.inc"
#include "pdf_ccitt.inc"
#include "pdf_image.inc"
#include "pdf_shade.inc"
#include "pdf_enc.inc"
#include "pdf_glyph.inc"
#include "pdf_font.inc"

/* ---------------------------------------------------------------- CoreGraphics usado */
void CGContextSetLineWidth(void *c, double w);
void CGContextSetLineCap(void *c, int v);
void CGContextSetLineJoin(void *c, int v);
void CGContextSetMiterLimit(void *c, double v);
void CGContextSetLineDash(void *c, double phase, const double *l, size_t n);
void CGContextSetFlatness(void *c, double f);
void CGContextSetBlendMode(void *c, int m);
void CGContextSetAlpha(void *c, double a);
void CGContextSetStrokeColorWithColor(void *c, void *color);
void CGContextAddCurveToPoint(void *c, double a, double b, double cc, double d, double x, double y);
void CGContextDrawPath(void *c, int mode);
void CGContextStrokePath(void *c);
void CGContextEOFillPath(void *c);
void CGContextEOClip(void *c);
void CGContextAddPath(void *c, const void *p);
void *CGContextCopyPath(void *c);
void CGContextReplacePathWithStrokedPath(void *c);
CGRect CGContextGetClipBoundingBox(void *c);
void CGContextDrawImage(void *c, CGRect r, void *img);
void CGContextBeginTransparencyLayer(void *c, void *aux);
void CGContextEndTransparencyLayer(void *c);
void CGContextClipToMask(void *c, CGRect r, void *mask);
size_t CGBitmapContextGetWidth(void *c);
size_t CGBitmapContextGetHeight(void *c);
void *CGBitmapContextCreate(void *data, size_t w, size_t h, size_t bpc, size_t bpr, void *space, u32 info);
void *CGBitmapContextCreateImage(void *c);
void CGContextRelease(void *c);
void *CGPathCreateCopyByTransformingPath(const void *p, const CGAffineTransform *m);
void CGPathAddPath(void *dst, const CGAffineTransform *m, const void *src);
bool CGPathIsEmpty(const void *p);
pdf_doc *cx_pdf_page_doc(void *page);
pdf_obj *CGPDFPageGetDictionary(void *page);
CGRect CGPDFPageGetBoxRect(void *page, int box);
int cx_pdf_dest_page(pdf_doc *pd, pdf_obj *dest);

static CGAffineTransform tmul(CGAffineTransform a, CGAffineTransform b) {     /* a, depois b */
    CGAffineTransform r = {a.a * b.a + a.b * b.c, a.a * b.b + a.b * b.d, a.c * b.a + a.d * b.c, a.c * b.b + a.d * b.d, a.tx * b.a + a.ty * b.c + b.tx, a.tx * b.b + a.ty * b.d + b.ty};
    return r;
}
static CGAffineTransform tinv(CGAffineTransform m) {
    double det = m.a * m.d - m.b * m.c;
    if (det > -1e-14 && det < 1e-14) { CGAffineTransform z = {1, 0, 0, 1, 0, 0}; return z; }
    CGAffineTransform r = {m.d / det, -m.b / det, -m.c / det, m.a / det, (m.c * m.ty - m.d * m.tx) / det, (m.b * m.tx - m.a * m.ty) / det};
    return r;
}

/* ---------------------------------------------------------------- estado */
typedef struct {
    pcs *fcs, *scs;
    double fc[PF_MAXOUT], sc[PF_MAXOUT];
    pdf_obj *fpat, *spat;
    double fa, sa;
    pfont *font;
    double fsize, Tc, Tw, Th, TL, Ts;
    int Tr;
    double lw;
} gstate;
typedef struct rnd {
    void *ctx;
    pdf_doc *pd;
    gstate gs[64];
    int gsp;
    CGAffineTransform base;
    CGAffineTransform tm, tlm;
    void *textclip;
    double cpx, cpy, spx, spy;
    int pending_clip;              /* 1 W, 2 W* */
    pcs *owned[1024];
    int nowned;
    pfont *fonts[256];
    int nfonts;
    pdf_obj *stack_forms[32];
    int depth;
    int hidden;                    /* níveis de conteúdo opcional desligado */
    int hide_stack[64], nhide;
    int uncolored;                 /* dentro de padrão sem cor (PaintType 2) ou glifo Type3 d1 */
    pdf_obj *ocoff;                /* /OCProperties /D /OFF */
    /* para o PDFKit: cada glifo mostrado (Unicode e caixa no espaço da página) */
    void (*on_glyph)(void *info, const u16 *uni, int nu, CGRect box, double fsize);
    void *glyph_info;
} rnd;
static gstate *G(rnd *r) { return &r->gs[r->gsp]; }
static pcs *own(rnd *r, pcs *c) {
    if (c && r->nowned < 1024) r->owned[r->nowned++] = c;
    else if (c) { pcs_free(c); c = 0; }
    return c;
}
static pcs *dev_space(rnd *r, const char *n) { return own(r, pcs_device(n)); }
static void gs_init(rnd *r) {
    gstate *g = G(r);
    memset(g, 0, sizeof *g);
    g->fcs = g->scs = dev_space(r, "DeviceGray");
    g->fa = g->sa = 1;
    g->Th = 1;
    g->lw = 1;
}
static void push(rnd *r) {
    if (r->gsp < 63) { r->gs[r->gsp + 1] = r->gs[r->gsp]; r->gsp++; }
    CGContextSaveGState(r->ctx);
}
static void pop(rnd *r) {
    if (r->gsp > 0) r->gsp--;
    CGContextRestoreGState(r->ctx);
}
static pfont *font_of(rnd *r, pdf_obj *fd) {
    fd = pdf_resolve(r->pd, fd);
    if (!fd) return 0;
    for (int i = 0; i < r->nfonts; i++) if (r->fonts[i]->key == fd) return r->fonts[i];
    pfont *f = font_load(r->pd, fd);
    if (f && r->nfonts < 256) r->fonts[r->nfonts++] = f;
    else if (f) { font_free(r->fonts[0]); r->fonts[0] = f; }
    return f;
}
/* blend mode do PDF → CGBlendMode */
static int blend_of(const char *n) {
    static const char *const names[] = {"Normal", "Multiply", "Screen", "Overlay", "Darken", "Lighten", "ColorDodge", "ColorBurn",
                                        "SoftLight", "HardLight", "Difference", "Exclusion", "Hue", "Saturation", "Color", "Luminosity", 0};
    for (int i = 0; n && names[i]; i++) if (!strcmp(n, names[i])) return i;
    return n && !strcmp(n, "Compatible") ? 0 : 0;
}

/* ---------------------------------------------------------------- pintura */
static void exec_content(rnd *r, const u8 *d, size_t n, pdf_obj *res);
static void exec_form(rnd *r, pdf_obj *form, pdf_obj *res);
static void set_fill(rnd *r) {
    gstate *g = G(r);
    if (r->uncolored) return;
    void *c = pcs_color(g->fcs, g->fc, g->fa);
    if (c) { CGContextSetFillColorWithColor(r->ctx, c); CFRelease(c); }
}
static void set_stroke(rnd *r) {
    gstate *g = G(r);
    if (r->uncolored) return;
    void *c = pcs_color(g->scs, g->sc, g->sa);
    if (c) { CGContextSetStrokeColorWithColor(r->ctx, c); CFRelease(c); }
}
/* preenche o path atual (já recortado) com o padrão: ladrilho célula a célula ou shading */
static void paint_pattern(rnd *r, pdf_obj *pat, pcs *under, const double *comps, double alpha, pdf_obj *res) {
    pdf_doc *pd = r->pd;
    pat = pdf_resolve(pd, pat);
    double type = 0;
    if (!pat || !pdf_num(pdf_dict_get(pd, pat, "PatternType"), &type)) return;
    double mt[6] = {1, 0, 0, 1, 0, 0};
    pf_nums(pd, pdf_dict_get(pd, pat, "Matrix"), mt, 6);
    CGAffineTransform M = {mt[0], mt[1], mt[2], mt[3], mt[4], mt[5]};
    /* espaço do padrão = espaço padrão da página × Matrix */
    CGAffineTransform cur = CGContextGetCTM(r->ctx);
    CGContextConcatCTM(r->ctx, tmul(tmul(M, r->base), tinv(cur)));
    CGContextSetAlpha(r->ctx, alpha);
    if (type == 2) {
        sh_paint(r->ctx, pd, pdf_dict_get(pd, pat, "Shading"), res);
        return;
    }
    double bb[4] = {0, 0, 0, 0}, xs = 0, ys = 0, pt = 1;
    pf_nums(pd, pdf_dict_get(pd, pat, "BBox"), bb, 4);
    pdf_num(pdf_dict_get(pd, pat, "XStep"), &xs);
    pdf_num(pdf_dict_get(pd, pat, "YStep"), &ys);
    pdf_num(pdf_dict_get(pd, pat, "PaintType"), &pt);
    if (xs == 0 || ys == 0) return;
    CGRect clip = CGContextGetClipBoundingBox(r->ctx);
    double bx0 = bb[0] < bb[2] ? bb[0] : bb[2], by0 = bb[1] < bb[3] ? bb[1] : bb[3], bw = fabs(bb[2] - bb[0]), bh = fabs(bb[3] - bb[1]);
    long i0 = (long)floor((clip.origin.x - bx0 - bw) / fabs(xs)), i1 = (long)ceil((clip.origin.x + clip.size.width - bx0) / fabs(xs));
    long j0 = (long)floor((clip.origin.y - by0 - bh) / fabs(ys)), j1 = (long)ceil((clip.origin.y + clip.size.height - by0) / fabs(ys));
    if ((i1 - i0) * (j1 - j0) > 20000 || i1 < i0 || j1 < j0) return;
    pdf_obj *pres = pdf_dict_get(pd, pat, "Resources");
    int was = r->uncolored;
    if (pt == 2) {
        /* sem cor própria: a cor vem do scn, no espaço base do Pattern */
        void *c = under ? pcs_color(under, comps, alpha) : 0;
        if (c) { CGContextSetFillColorWithColor(r->ctx, c); CGContextSetStrokeColorWithColor(r->ctx, c); CFRelease(c); }
        r->uncolored = 1;
    }
    size_t n;
    const u8 *d = pdf_stream_data(pd, pat, &n, 0);
    for (long j = j0; d && j <= j1; j++)
        for (long i = i0; i <= i1; i++) {
            push(r);
            CGAffineTransform t = {1, 0, 0, 1, i * fabs(xs), j * fabs(ys)};
            CGContextConcatCTM(r->ctx, t);
            CGRect cell = {{bx0, by0}, {bw, bh}};
            CGContextBeginPath(r->ctx);
            CGContextAddRect(r->ctx, cell);
            CGContextClip(r->ctx);
            exec_content(r, d, n, pres ? pres : res);
            pop(r);
        }
    r->uncolored = was;
}
/* pinta o path atual: mode 0 f, 1 f*, 2 S, 3 B, 4 B*; depois aplica o W/W* pendente */
static void paint_path(rnd *r, int mode, pdf_obj *res) {
    void *ctx = r->ctx;
    gstate *g = G(r);
    void *clip = r->pending_clip ? CGContextCopyPath(ctx) : 0;
    int fill = mode == 0 || mode == 1 || mode == 3 || mode == 4, stroke = mode >= 2, eo = mode == 1 || mode == 4;
    if (!r->hidden) {
        void *keep = (fill && g->fpat) || (stroke && g->spat) ? CGContextCopyPath(ctx) : 0;
        if (fill && g->fpat && !r->uncolored) {
            push(r);
            if (keep) CGContextAddPath(ctx, keep);
            if (eo) CGContextEOClip(ctx); else CGContextClip(ctx);
            paint_pattern(r, g->fpat, g->fcs ? g->fcs->base : 0, g->fc, g->fa, res);
            pop(r);
            fill = 0;
        }
        if (stroke && g->spat && !r->uncolored) {
            push(r);
            if (keep) CGContextAddPath(ctx, keep);
            CGContextReplacePathWithStrokedPath(ctx);
            CGContextClip(ctx);
            paint_pattern(r, g->spat, g->scs ? g->scs->base : 0, g->sc, g->sa, res);
            pop(r);
            stroke = 0;
        }
        if (keep && (fill || stroke)) { CGContextBeginPath(ctx); CGContextAddPath(ctx, keep); }
        if (keep) CGPathRelease(keep);
        if (fill) set_fill(r);
        if (stroke) set_stroke(r);
        if (fill && stroke) CGContextDrawPath(ctx, eo ? 4 : 3);
        else if (fill) { if (eo) CGContextEOFillPath(ctx); else CGContextFillPath(ctx); }
        else if (stroke) CGContextStrokePath(ctx);
        else CGContextBeginPath(ctx);
    } else CGContextBeginPath(ctx);
    if (clip) {
        CGContextBeginPath(ctx);
        CGContextAddPath(ctx, clip);
        if (r->pending_clip == 2) CGContextEOClip(ctx); else CGContextClip(ctx);
        CGPathRelease(clip);
    }
    r->pending_clip = 0;
    CGContextBeginPath(ctx);
}

/* ---------------------------------------------------------------- texto */
static void show_glyphs(rnd *r, pdf_obj *s, pdf_obj *res) {
    gstate *g = G(r);
    pfont *f = g->font;
    if (!f || !s || s->type != PO_STRING) return;
    double fs = g->fsize, th = g->Th;
    size_t i = 0;
    int mode = g->Tr;
    int fill = mode == 0 || mode == 2 || mode == 4 || mode == 6, stroke = mode == 1 || mode == 2 || mode == 5 || mode == 6, clip = mode >= 4;
    while (i < s->n) {
        u32 code;
        int nb = pf_next_code(f, s->v.str, s->n, &i, &code);
        if (!nb) break;
        double w0 = pf_width(f, code, nb);
        CGAffineTransform S = {fs * th, 0, 0, fs, 0, g->Ts};
        CGAffineTransform pos = r->tm;
        if (f->vertical) {
            /* escrita vertical: origem deslocada por (w0/2, DW2[0]) e avanço vertical DW2[1] */
            CGAffineTransform v = {1, 0, 0, 1, -w0 / 2 * fs, -f->dw2[0] / 1000.0 * fs};
            pos = tmul(v, r->tm);
        }
        CGAffineTransform trm = tmul(S, pos);
        if (r->on_glyph && !r->hidden) {
            u16 u[8];
            int nu = pf_unicode(f, code, nb, u, 8);
            double asc = 0.8, desc = -0.2;
            CGAffineTransform bt = tmul(trm, CGContextGetCTM(r->ctx));
            CGAffineTransform page = tmul(bt, tinv(r->base));
            double x0 = page.tx + page.c * desc, y0 = page.ty + page.d * desc;
            double adv = w0 + (fs ? g->Tc / fs : 0);               /* o espaçamento de caracteres faz parte do avanço */
            double ww = adv * page.a, hh = (asc - desc) * page.d;
            CGRect box = {{x0, y0}, {ww, hh}};
            r->on_glyph(r->glyph_info, u, nu, box, sqrt(fabs(page.a * page.d - page.b * page.c)));
        }
        if (f->kind == 3) {
            /* glifo Type3: o CharProc executado na matriz do glifo */
            const char *nm = code < 256 ? f->names[code] : 0;
            pdf_obj *cp = nm ? pdf_dict_get(r->pd, f->charprocs, nm) : 0;
            if (cp && cp->type == PO_STREAM && !r->hidden && r->depth < 24) {
                size_t n;
                const u8 *d = pdf_stream_data(r->pd, cp, &n, 0);
                CGAffineTransform fm = {f->fm[0], f->fm[1], f->fm[2], f->fm[3], f->fm[4], f->fm[5]};
                push(r);
                CGContextConcatCTM(r->ctx, tmul(fm, trm));
                set_fill(r);
                set_stroke(r);
                int was = r->uncolored;
                r->depth++;
                if (d) exec_content(r, d, n, f->t3res ? f->t3res : res);
                r->depth--;
                r->uncolored = was;
                pop(r);
            }
        } else if (fill || stroke || clip) {
            void *gp = pf_glyph_path(f, code, nb);
            if (gp && !CGPathIsEmpty(gp)) {
                CGAffineTransform fm = {f->fm[0], f->fm[1], f->fm[2], f->fm[3], f->fm[4], f->fm[5]};
                CGAffineTransform m = tmul(fm, trm);
                void *up = CGPathCreateCopyByTransformingPath(gp, &m);
                if (up && !r->hidden && (fill || stroke)) {
                    if (fill && g->fpat) {
                        push(r);
                        CGContextBeginPath(r->ctx);
                        CGContextAddPath(r->ctx, up);
                        CGContextClip(r->ctx);
                        paint_pattern(r, g->fpat, g->fcs ? g->fcs->base : 0, g->fc, g->fa, res);
                        pop(r);
                        if (stroke) { set_stroke(r); CGContextBeginPath(r->ctx); CGContextAddPath(r->ctx, up); CGContextStrokePath(r->ctx); }
                    } else {
                        if (fill) set_fill(r);
                        if (stroke) set_stroke(r);
                        CGContextBeginPath(r->ctx);
                        CGContextAddPath(r->ctx, up);
                        CGContextDrawPath(r->ctx, fill && stroke ? 3 : fill ? 0 : 2);
                    }
                }
                if (up && clip) {
                    if (!r->textclip) r->textclip = CGPathCreateMutable();
                    if (r->textclip) CGPathAddPath(r->textclip, 0, up);
                }
                if (up) CGPathRelease(up);
            }
        }
        /* avanço */
        double tw = nb == 1 && code == 32 ? g->Tw : 0;
        if (f->vertical) {
            double w1 = f->dw2[1] / 1000.0;
            double ty = w1 * fs + g->Tc + tw;
            CGAffineTransform t = {1, 0, 0, 1, 0, ty};
            r->tm = tmul(t, r->tm);
        } else {
            double tx = (w0 * fs + g->Tc + tw) * th;
            CGAffineTransform t = {1, 0, 0, 1, tx, 0};
            r->tm = tmul(t, r->tm);
        }
    }
}
static void text_adjust(rnd *r, double num) {
    gstate *g = G(r);
    if (g->font && g->font->vertical) { CGAffineTransform t = {1, 0, 0, 1, 0, -num / 1000.0 * g->fsize}; r->tm = tmul(t, r->tm); }
    else { CGAffineTransform t = {1, 0, 0, 1, -num / 1000.0 * g->fsize * g->Th, 0}; r->tm = tmul(t, r->tm); }
}
static void text_newline(rnd *r, double tx, double ty) {
    CGAffineTransform t = {1, 0, 0, 1, tx, ty};
    r->tlm = tmul(t, r->tlm);
    r->tm = r->tlm;
}

/* ---------------------------------------------------------------- ExtGState e máscara suave */
static void apply_smask(rnd *r, pdf_obj *sm, pdf_obj *res) {
    pdf_doc *pd = r->pd;
    size_t W = CGBitmapContextGetWidth(r->ctx), H = CGBitmapContextGetHeight(r->ctx);
    if (!W || !H || W * H > (1u << 26)) return;                       /* só contexto de bitmap */
    pdf_obj *gform = pdf_dict_get(pd, sm, "G");
    if (!gform || gform->type != PO_STREAM) return;
    const char *s = pdf_name(pdf_dict_get(pd, sm, "S"));
    int lum = !s || strcmp(s, "Alpha");
    void *dev = CGColorSpaceCreateDeviceRGB();
    void *mc = CGBitmapContextCreate(0, W, H, 8, W * 4, dev, 1 /* premultipliedLast */);
    CGColorSpaceRelease(dev);
    if (!mc) return;
    if (lum) {
        double bc[PF_MAXOUT] = {0};
        pf_nums(pd, pdf_dict_get(pd, sm, "BC"), bc, PF_MAXOUT);
        pdf_obj *grp = pdf_dict_get(pd, gform, "Group");
        pcs *gcs = pcs_resolve(pd, pdf_dict_get_raw(grp, "CS"), res, 0);
        void *bcol = gcs ? pcs_color(gcs, bc, 1) : 0;
        if (bcol) { CGContextSetFillColorWithColor(mc, bcol); CFRelease(bcol); }
        CGRect all = {{0, 0}, {(double)W, (double)H}};
        CGContextBeginPath(mc);
        CGContextAddRect(mc, all);
        CGContextFillPath(mc);
        pcs_free(gcs);
    }
    /* mesmo CTM do contexto de destino (a máscara é tomada no momento do gs) */
    CGAffineTransform D = CGContextGetCTM(mc), T = CGContextGetCTM(r->ctx);
    CGContextConcatCTM(mc, tmul(T, tinv(D)));
    rnd sub;
    memset(&sub, 0, sizeof sub);
    sub.ctx = mc; sub.pd = pd; sub.base = r->base; sub.ocoff = r->ocoff;
    gs_init(&sub);
    exec_form(&sub, gform, res);
    for (int i = 0; i < sub.nowned; i++) pcs_free(sub.owned[i]);
    for (int i = 0; i < sub.nfonts; i++) font_free(sub.fonts[i]);
    void *img = CGBitmapContextCreateImage(mc);
    CGContextRelease(mc);
    if (!img) return;
    /* máscara em cinza: luminância (ou alfa), com a função de transferência TR se houver */
    const u8 *px;
    long bpr;
    int w, h, pm;
    u8 *gray = 0;
    pf_set tr = {{0}, 0};
    pfs_load(pd, pdf_dict_get_raw(sm, "TR"), &tr);
    if (cx_cgimage_src(img, &px, &bpr, &w, &h, &pm) && (gray = malloc((size_t)w * h))) {
        for (int y = 0; y < h; y++) {
            const u32 *row = (const u32 *)(px + (size_t)y * bpr);
            for (int x = 0; x < w; x++) {
                u32 p = row[x], a = p >> 24, v;
                if (lum) v = (((p >> 16) & 255) * 77 + ((p >> 8) & 255) * 150 + (p & 255) * 29 + 128) >> 8;
                else v = a;
                if (tr.nf) { double in = v / 255.0, o[PF_MAXOUT]; pfs_eval(&tr, &in, o); v = (u32)(pf_clamp(o[0], 0, 1) * 255 + 0.5); }
                gray[(size_t)y * w + x] = (u8)v;
            }
        }
        void *gs = CGColorSpaceCreateDeviceGray();
        void *prov = CGDataProviderCreateWithData(0, gray, (size_t)w * h, free_cb);
        void *mimg = prov ? CGImageCreate((size_t)w, (size_t)h, 8, 8, (size_t)w, gs, 0, prov, 0, 0, 0) : 0;
        if (prov) CGDataProviderRelease(prov); else free(gray);
        CGColorSpaceRelease(gs);
        if (mimg) {
            /* recorte no espaço inicial do bitmap (o mesmo CTM D do contexto da máscara), depois volta ao CTM atual */
            CGAffineTransform cur = CGContextGetCTM(r->ctx);
            CGContextConcatCTM(r->ctx, tmul(D, tinv(cur)));
            CGRect all = {{0, 0}, {(double)W, (double)H}};
            CGContextClipToMask(r->ctx, all, mimg);
            CGContextConcatCTM(r->ctx, tmul(cur, tinv(D)));
            CGImageRelease(mimg);
        }
    }
    pfs_free(&tr);
    CGImageRelease(img);
}
static void apply_extgstate(rnd *r, pdf_obj *e, pdf_obj *res) {
    pdf_doc *pd = r->pd;
    gstate *g = G(r);
    double v;
    if (!e || e->type != PO_DICT) return;
    if (pdf_num(pdf_dict_get(pd, e, "LW"), &v)) { CGContextSetLineWidth(r->ctx, v); g->lw = v; }
    if (pdf_num(pdf_dict_get(pd, e, "LC"), &v)) CGContextSetLineCap(r->ctx, (int)v);
    if (pdf_num(pdf_dict_get(pd, e, "LJ"), &v)) CGContextSetLineJoin(r->ctx, (int)v);
    if (pdf_num(pdf_dict_get(pd, e, "ML"), &v)) CGContextSetMiterLimit(r->ctx, v);
    if (pdf_num(pdf_dict_get(pd, e, "FL"), &v)) CGContextSetFlatness(r->ctx, v);
    if (pdf_num(pdf_dict_get(pd, e, "CA"), &v)) g->sa = pf_clamp(v, 0, 1);
    if (pdf_num(pdf_dict_get(pd, e, "ca"), &v)) g->fa = pf_clamp(v, 0, 1);
    pdf_obj *dd = pdf_dict_get(pd, e, "D");
    if (dd && dd->type == PO_ARRAY) {
        double ln[32], ph = 0;
        int k = pf_nums(pd, pdf_array_get(pd, dd, 0), ln, 32);
        pdf_num(pdf_array_get(pd, dd, 1), &ph);
        CGContextSetLineDash(r->ctx, ph, k ? ln : 0, (size_t)k);
    }
    pdf_obj *bm = pdf_dict_get(pd, e, "BM");
    if (bm) CGContextSetBlendMode(r->ctx, blend_of(pdf_name(bm->type == PO_ARRAY ? pdf_array_get(pd, bm, 0) : bm)));
    pdf_obj *fo = pdf_dict_get(pd, e, "Font");
    if (fo && fo->type == PO_ARRAY) { g->font = font_of(r, fo->n ? fo->v.arr[0] : 0); pdf_num(pdf_array_get(pd, fo, 1), &g->fsize); }
    pdf_obj *sm = pdf_dict_get(pd, e, "SMask");
    if (sm && sm->type == PO_DICT) apply_smask(r, sm, res);
}

/* ---------------------------------------------------------------- conteúdo opcional */
static int ocg_off(rnd *r, pdf_obj *oc) {
    pdf_doc *pd = r->pd;
    oc = pdf_resolve(pd, oc);
    if (!oc) return 0;
    const char *t = pdf_name(pdf_dict_get(pd, oc, "Type"));
    if (t && !strcmp(t, "OCMD")) {
        pdf_obj *ocgs = pdf_dict_get(pd, oc, "OCGs");
        const char *p = pdf_name(pdf_dict_get(pd, oc, "P"));
        int n = ocgs && ocgs->type == PO_ARRAY ? (int)ocgs->n : 1, on = 0, off = 0;
        for (int i = 0; i < n; i++) {
            pdf_obj *g = ocgs && ocgs->type == PO_ARRAY ? pdf_resolve(pd, ocgs->v.arr[i]) : ocgs;
            if (ocg_off(r, g)) off++; else on++;
        }
        if (!p || !strcmp(p, "AnyOn")) return on == 0;
        if (!strcmp(p, "AllOn")) return off > 0;
        if (!strcmp(p, "AnyOff")) return off == 0;
        if (!strcmp(p, "AllOff")) return on > 0;
        return 0;
    }
    for (u32 i = 0; r->ocoff && r->ocoff->type == PO_ARRAY && i < r->ocoff->n; i++) if (pdf_resolve(pd, r->ocoff->v.arr[i]) == oc) return 1;
    return 0;
}

/* ---------------------------------------------------------------- XObjects */
static void draw_image(rnd *r, pdf_obj *img, pdf_obj *res) {
    if (r->hidden) return;
    int mask = 0;
    void *ci = pimg_create(r->pd, img, res, 0, &mask);
    if (!ci) return;
    gstate *g = G(r);
    push(r);
    CGContextSetAlpha(r->ctx, g->fa);
    CGRect unit = {{0, 0}, {1, 1}};
    if (mask && g->fpat && !r->uncolored) {
        /* máscara de estêncil pintada com padrão: recorta pela máscara e pinta o padrão */
        CGContextClipToMask(r->ctx, unit, ci);
        CGContextBeginPath(r->ctx);
        CGContextAddRect(r->ctx, unit);
        CGContextClip(r->ctx);
        paint_pattern(r, g->fpat, g->fcs ? g->fcs->base : 0, g->fc, g->fa, res);
    } else {
        if (mask) set_fill(r);
        CGContextDrawImage(r->ctx, unit, ci);
    }
    pop(r);
    CGImageRelease(ci);
}
static void exec_form(rnd *r, pdf_obj *form, pdf_obj *res) {
    pdf_doc *pd = r->pd;
    if (r->depth >= 32) return;
    for (int i = 0; i < r->depth; i++) if (r->stack_forms[i] == form) return;     /* ciclo */
    pdf_obj *oc = pdf_dict_get(pd, form, "OC");
    if (oc && ocg_off(r, oc)) return;
    size_t n;
    const u8 *d = pdf_stream_data(pd, form, &n, 0);
    if (!d) return;
    push(r);
    double mt[6] = {1, 0, 0, 1, 0, 0}, bb[4];
    pf_nums(pd, pdf_dict_get(pd, form, "Matrix"), mt, 6);
    CGAffineTransform M = {mt[0], mt[1], mt[2], mt[3], mt[4], mt[5]};
    CGContextConcatCTM(r->ctx, M);
    if (pf_nums(pd, pdf_dict_get(pd, form, "BBox"), bb, 4) == 4) {
        CGRect b = {{bb[0] < bb[2] ? bb[0] : bb[2], bb[1] < bb[3] ? bb[1] : bb[3]}, {fabs(bb[2] - bb[0]), fabs(bb[3] - bb[1])}};
        CGContextBeginPath(r->ctx);
        CGContextAddRect(r->ctx, b);
        CGContextClip(r->ctx);
    }
    pdf_obj *grp = pdf_dict_get(pd, form, "Group");
    const char *gs = pdf_name(pdf_dict_get(pd, grp, "S"));
    int layer = gs && !strcmp(gs, "Transparency");
    if (layer) {
        CGContextSetAlpha(r->ctx, G(r)->fa);
        CGContextBeginTransparencyLayer(r->ctx, 0);
        G(r)->fa = G(r)->sa = 1;                      /* dentro do grupo o alfa vale na composição */
    }
    pdf_obj *fres = pdf_dict_get(pd, form, "Resources");
    r->stack_forms[r->depth++] = form;
    exec_content(r, d, n, fres ? fres : res);
    r->depth--;
    if (layer) CGContextEndTransparencyLayer(r->ctx);
    pop(r);
}

/* ---------------------------------------------------------------- interpretador */
#define NARGS 64
static double num_at(pdf_obj **a, int na, int i) { double v = 0; if (i >= 0 && i < na) pdf_num(a[i], &v); return v; }
static void set_comps(rnd *r, int stroke, pdf_obj **a, int na, pdf_obj *res) {
    gstate *g = G(r);
    double *c = stroke ? g->sc : g->fc;
    pdf_obj **pat = stroke ? &g->spat : &g->fpat;
    int k = 0;
    for (int i = 0; i < na && k < PF_MAXOUT; i++) {
        if (a[i]->type == PO_NAME) {                 /* scn com nome: padrão */
            pdf_obj *p = pdf_dict_get(r->pd, pdf_dict_get(r->pd, res, "Pattern"), a[i]->v.name);
            *pat = p;
            continue;
        }
        double v;
        if (pdf_num(a[i], &v)) c[k++] = v;
    }
    pcs *cs = stroke ? g->scs : g->fcs;
    if (!cs || cs->kind != PCS_PATTERN) *pat = 0;
}
static void exec_content(rnd *r, const u8 *d, size_t n, pdf_obj *res) {
    pdf_doc *pd = r->pd;
    pdf_lexer *L = pdf_lex_begin(pd, d, n);
    if (!L) return;
    pdf_obj *a[NARGS];
    int na = 0, base_gsp = r->gsp, in_text = 0;
    pdf_obj *o;
    void *ctx = r->ctx;
    while ((o = pdf_lex_next(L))) {
        if (o->type != PO_OP) { if (na < NARGS) a[na++] = o; continue; }
        const char *op = o->v.name;
        gstate *g = G(r);
        char c0 = op[0], c1 = op[0] ? op[1] : 0, c2 = c1 ? op[2] : 0;
#define OP1(x) (c0 == (x) && !c1)
#define OP2(x, y) (c0 == (x) && c1 == (y) && !c2)
        if (OP1('q')) push(r);
        else if (OP1('Q')) { if (r->gsp > base_gsp) pop(r); }
        else if (OP2('c', 'm') && na >= 6) { CGAffineTransform t = {num_at(a, na, 0), num_at(a, na, 1), num_at(a, na, 2), num_at(a, na, 3), num_at(a, na, 4), num_at(a, na, 5)}; CGContextConcatCTM(ctx, t); }
        else if (OP1('w') && na >= 1) { g->lw = num_at(a, na, 0); CGContextSetLineWidth(ctx, g->lw); }
        else if (OP1('J') && na >= 1) CGContextSetLineCap(ctx, (int)num_at(a, na, 0));
        else if (OP1('j') && na >= 1) CGContextSetLineJoin(ctx, (int)num_at(a, na, 0));
        else if (OP1('M') && na >= 1) CGContextSetMiterLimit(ctx, num_at(a, na, 0));
        else if (OP1('d') && na >= 2) { double ln[32]; int k = pf_nums(pd, a[0], ln, 32); CGContextSetLineDash(ctx, num_at(a, na, 1), k ? ln : 0, (size_t)k); }
        else if (OP1('i') && na >= 1) CGContextSetFlatness(ctx, num_at(a, na, 0));
        else if (OP2('r', 'i')) {}
        else if (OP2('g', 's') && na >= 1 && a[0]->type == PO_NAME) apply_extgstate(r, pdf_dict_get(pd, pdf_dict_get(pd, res, "ExtGState"), a[0]->v.name), res);
        /* path */
        else if (OP1('m') && na >= 2) { r->cpx = r->spx = num_at(a, na, 0); r->cpy = r->spy = num_at(a, na, 1); CGContextMoveToPoint(ctx, r->cpx, r->cpy); }
        else if (OP1('l') && na >= 2) { r->cpx = num_at(a, na, 0); r->cpy = num_at(a, na, 1); CGContextAddLineToPoint(ctx, r->cpx, r->cpy); }
        else if (OP1('c') && na >= 6) { CGContextAddCurveToPoint(ctx, num_at(a, na, 0), num_at(a, na, 1), num_at(a, na, 2), num_at(a, na, 3), num_at(a, na, 4), num_at(a, na, 5)); r->cpx = num_at(a, na, 4); r->cpy = num_at(a, na, 5); }
        else if (OP1('v') && na >= 4) { CGContextAddCurveToPoint(ctx, r->cpx, r->cpy, num_at(a, na, 0), num_at(a, na, 1), num_at(a, na, 2), num_at(a, na, 3)); r->cpx = num_at(a, na, 2); r->cpy = num_at(a, na, 3); }
        else if (OP1('y') && na >= 4) { double x = num_at(a, na, 2), y = num_at(a, na, 3); CGContextAddCurveToPoint(ctx, num_at(a, na, 0), num_at(a, na, 1), x, y, x, y); r->cpx = x; r->cpy = y; }
        else if (OP1('h')) { CGContextClosePath(ctx); r->cpx = r->spx; r->cpy = r->spy; }
        else if (OP2('r', 'e') && na >= 4) { CGRect rr = {{num_at(a, na, 0), num_at(a, na, 1)}, {num_at(a, na, 2), num_at(a, na, 3)}}; CGContextAddRect(ctx, rr); r->cpx = r->spx = rr.origin.x; r->cpy = r->spy = rr.origin.y; }
        else if (OP1('S')) paint_path(r, 2, res);
        else if (OP1('s')) { CGContextClosePath(ctx); paint_path(r, 2, res); }
        else if (OP1('f') || OP1('F')) paint_path(r, 0, res);
        else if (OP2('f', '*')) paint_path(r, 1, res);
        else if (OP1('B')) paint_path(r, 3, res);
        else if (OP2('B', '*')) paint_path(r, 4, res);
        else if (OP1('b')) { CGContextClosePath(ctx); paint_path(r, 3, res); }
        else if (OP2('b', '*')) { CGContextClosePath(ctx); paint_path(r, 4, res); }
        else if (OP1('n')) paint_path(r, -1, res);
        else if (OP1('W')) r->pending_clip = 1;
        else if (OP2('W', '*')) r->pending_clip = 2;
        /* cor */
        else if ((OP2('C', 'S') || OP2('c', 's')) && na >= 1) {
            int st = c0 == 'C';
            pcs *cs = own(r, pcs_resolve(pd, a[0], res, 0));
            if (cs) {
                if (st) { g->scs = cs; pcs_initial(cs, g->sc); g->spat = 0; }
                else { g->fcs = cs; pcs_initial(cs, g->fc); g->fpat = 0; }
            }
        } else if (OP2('S', 'C') || !strcmp(op, "SCN")) set_comps(r, 1, a, na, res);
        else if (OP2('s', 'c') || !strcmp(op, "scn")) set_comps(r, 0, a, na, res);
        else if ((OP1('G') || OP1('g')) && na >= 1) { int st = c0 == 'G'; pcs *cs = dev_space(r, "DeviceGray"); if (st) { g->scs = cs; g->sc[0] = num_at(a, na, 0); g->spat = 0; } else { g->fcs = cs; g->fc[0] = num_at(a, na, 0); g->fpat = 0; } }
        else if ((OP2('R', 'G') || OP2('r', 'g')) && na >= 3) {
            int st = c0 == 'R';
            pcs *cs = dev_space(r, "DeviceRGB");
            double *c = st ? g->sc : g->fc;
            for (int k = 0; k < 3; k++) c[k] = num_at(a, na, k);
            if (st) { g->scs = cs; g->spat = 0; } else { g->fcs = cs; g->fpat = 0; }
        } else if ((OP1('K') || OP1('k')) && na >= 4) {
            int st = c0 == 'K';
            pcs *cs = dev_space(r, "DeviceCMYK");
            double *c = st ? g->sc : g->fc;
            for (int k = 0; k < 4; k++) c[k] = num_at(a, na, k);
            if (st) { g->scs = cs; g->spat = 0; } else { g->fcs = cs; g->fpat = 0; }
        }
        /* texto */
        else if (OP2('B', 'T')) { CGAffineTransform id = {1, 0, 0, 1, 0, 0}; r->tm = r->tlm = id; in_text = 1; }
        else if (OP2('E', 'T')) {
            in_text = 0;
            if (r->textclip) {
                CGContextBeginPath(ctx);
                CGContextAddPath(ctx, r->textclip);
                CGContextClip(ctx);
                CGPathRelease(r->textclip);
                r->textclip = 0;
            }
        } else if (OP2('T', 'c') && na >= 1) g->Tc = num_at(a, na, 0);
        else if (OP2('T', 'w') && na >= 1) g->Tw = num_at(a, na, 0);
        else if (OP2('T', 'z') && na >= 1) g->Th = num_at(a, na, 0) / 100.0;
        else if (OP2('T', 'L') && na >= 1) g->TL = num_at(a, na, 0);
        else if (OP2('T', 'r') && na >= 1) g->Tr = (int)num_at(a, na, 0);
        else if (OP2('T', 's') && na >= 1) g->Ts = num_at(a, na, 0);
        else if (OP2('T', 'f') && na >= 2 && a[0]->type == PO_NAME) { g->font = font_of(r, pdf_dict_get(pd, pdf_dict_get(pd, res, "Font"), a[0]->v.name)); g->fsize = num_at(a, na, 1); }
        else if (OP2('T', 'd') && na >= 2) text_newline(r, num_at(a, na, 0), num_at(a, na, 1));
        else if (OP2('T', 'D') && na >= 2) { g->TL = -num_at(a, na, 1); text_newline(r, num_at(a, na, 0), num_at(a, na, 1)); }
        else if (OP2('T', 'm') && na >= 6) { CGAffineTransform t = {num_at(a, na, 0), num_at(a, na, 1), num_at(a, na, 2), num_at(a, na, 3), num_at(a, na, 4), num_at(a, na, 5)}; r->tm = r->tlm = t; }
        else if (OP2('T', '*')) text_newline(r, 0, -g->TL);
        else if (OP2('T', 'j') && na >= 1) show_glyphs(r, a[na - 1], res);
        else if (OP1('\'') && na >= 1) { text_newline(r, 0, -g->TL); show_glyphs(r, a[na - 1], res); }
        else if (OP1('"') && na >= 3) { g->Tw = num_at(a, na, 0); g->Tc = num_at(a, na, 1); text_newline(r, 0, -g->TL); show_glyphs(r, a[2], res); }
        else if (OP2('T', 'J') && na >= 1 && a[0]->type == PO_ARRAY) {
            for (u32 k = 0; k < a[0]->n; k++) {
                pdf_obj *x = pdf_resolve(pd, a[0]->v.arr[k]);
                double v;
                if (x && x->type == PO_STRING) show_glyphs(r, x, res);
                else if (pdf_num(x, &v)) text_adjust(r, v);
            }
        } else if (OP2('d', '0') || OP2('d', '1')) { if (c1 == '1') r->uncolored = 1; }
        /* XObject, imagem inline, shading */
        else if (OP2('D', 'o') && na >= 1 && a[0]->type == PO_NAME) {
            pdf_obj *x = pdf_dict_get(pd, pdf_dict_get(pd, res, "XObject"), a[0]->v.name);
            const char *st = pdf_name(pdf_dict_get(pd, x, "Subtype"));
            if (x && st && !strcmp(st, "Image")) {
                pdf_obj *oc = pdf_dict_get(pd, x, "OC");
                if (!oc || !ocg_off(r, oc)) draw_image(r, x, res);
            } else if (x && st && !strcmp(st, "Form") && !r->hidden) exec_form(r, x, res);
        } else if (OP2('B', 'I')) {
            pdf_obj *img = pdf_lex_inline_image(L);
            if (img) draw_image(r, img, res);
        } else if (OP2('s', 'h') && na >= 1 && a[0]->type == PO_NAME && !r->hidden) {
            pdf_obj *sh = pdf_dict_get(pd, pdf_dict_get(pd, res, "Shading"), a[0]->v.name);
            push(r);
            CGContextSetAlpha(ctx, g->fa);
            sh_paint(ctx, pd, sh, res);
            pop(r);
        }
        /* conteúdo marcado: só o opcional importa */
        else if (!strcmp(op, "BDC") || !strcmp(op, "BMC")) {
            int off = 0;
            if (op[1] == 'D' && na >= 2 && a[0]->type == PO_NAME && !strcmp(a[0]->v.name, "OC")) {
                pdf_obj *p = a[1]->type == PO_NAME ? pdf_dict_get(pd, pdf_dict_get(pd, res, "Properties"), a[1]->v.name) : a[1];
                off = ocg_off(r, p);
            }
            if (r->nhide < 64) r->hide_stack[r->nhide++] = off;
            r->hidden += off;
        } else if (!strcmp(op, "EMC")) { if (r->nhide > 0) r->hidden -= r->hide_stack[--r->nhide]; }
        na = 0;
#undef OP1
#undef OP2
    }
    (void)in_text;
    while (r->gsp > base_gsp) pop(r);              /* q sem Q no fim do stream */
    pdf_lex_end(L);
}

/* aparência normal das anotações visíveis (ISO 32000-1 §12.5.5, algoritmo 8.1) */
static void draw_annotations(rnd *r, pdf_obj *page, pdf_obj *res) {
    pdf_doc *pd = r->pd;
    pdf_obj *an = pdf_dict_get(pd, page, "Annots");
    for (u32 i = 0; an && an->type == PO_ARRAY && i < an->n; i++) {
        pdf_obj *a = pdf_resolve(pd, an->v.arr[i]);
        const char *st = pdf_name(pdf_dict_get(pd, a, "Subtype"));
        double fl = 0;
        pdf_num(pdf_dict_get(pd, a, "F"), &fl);
        if (!a || (st && !strcmp(st, "Popup")) || ((int)fl & (2 | 32))) continue;
        pdf_obj *ap = pdf_dict_get(pd, pdf_dict_get(pd, a, "AP"), "N");
        if (ap && ap->type == PO_DICT) ap = pdf_dict_get(pd, ap, pdf_name(pdf_dict_get(pd, a, "AS")) ? pdf_name(pdf_dict_get(pd, a, "AS")) : "Off");
        if (!ap || ap->type != PO_STREAM) continue;
        double rc[4], bb[4] = {0, 0, 1, 1}, mt[6] = {1, 0, 0, 1, 0, 0};
        if (pf_nums(pd, pdf_dict_get(pd, a, "Rect"), rc, 4) < 4) continue;
        pf_nums(pd, pdf_dict_get(pd, ap, "BBox"), bb, 4);
        pf_nums(pd, pdf_dict_get(pd, ap, "Matrix"), mt, 6);
        CGAffineTransform M = {mt[0], mt[1], mt[2], mt[3], mt[4], mt[5]};
        double xs[4] = {bb[0], bb[2], bb[0], bb[2]}, ys[4] = {bb[1], bb[1], bb[3], bb[3]}, x0 = 1e30, y0 = 1e30, x1 = -1e30, y1 = -1e30;
        for (int k = 0; k < 4; k++) {
            double x = M.a * xs[k] + M.c * ys[k] + M.tx, y = M.b * xs[k] + M.d * ys[k] + M.ty;
            if (x < x0) x0 = x;
            if (x > x1) x1 = x;
            if (y < y0) y0 = y;
            if (y > y1) y1 = y;
        }
        double rx0 = rc[0] < rc[2] ? rc[0] : rc[2], ry0 = rc[1] < rc[3] ? rc[1] : rc[3], rw = fabs(rc[2] - rc[0]), rh = fabs(rc[3] - rc[1]);
        if (x1 - x0 < 1e-9 || y1 - y0 < 1e-9) continue;
        CGAffineTransform A = {rw / (x1 - x0), 0, 0, rh / (y1 - y0), rx0 - x0 * rw / (x1 - x0), ry0 - y0 * rh / (y1 - y0)};
        push(r);
        CGContextConcatCTM(r->ctx, A);
        exec_form(r, ap, res);
        pop(r);
    }
}
static void render_page(rnd *r, void *page) {
    pdf_doc *pd = cx_pdf_page_doc(page);
    pdf_obj *pg = CGPDFPageGetDictionary(page);
    if (!pd || !pg) return;
    r->pd = pd;
    r->base = CGContextGetCTM(r->ctx);
    r->ocoff = pdf_dict_get(pd, pdf_dict_get(pd, pdf_dict_get(pd, pdf_catalog(pd), "OCProperties"), "D"), "OFF");
    gs_init(r);
    CGContextSaveGState(r->ctx);
    CGRect crop = CGPDFPageGetBoxRect(page, 1);
    CGContextBeginPath(r->ctx);
    CGContextAddRect(r->ctx, crop);
    CGContextClip(r->ctx);
    pdf_obj *res = pdf_page_attr(pd, pg, "Resources");
    u8 *c;
    size_t n;
    pdf_page_contents(pd, pg, &c, &n);
    if (c) exec_content(r, c, n, res);
    free(c);
    draw_annotations(r, pg, res);
    CGContextRestoreGState(r->ctx);
    if (r->textclip) CGPathRelease(r->textclip);
    for (int i = 0; i < r->nowned; i++) pcs_free(r->owned[i]);
    for (int i = 0; i < r->nfonts; i++) font_free(r->fonts[i]);
}
void CGContextDrawPDFPage(void *ctx, void *page) {
    if (!ctx || !page) return;
    rnd *r = calloc(1, sizeof *r);
    if (!r) return;
    r->ctx = ctx;
    render_page(r, page);
    free(r);
}
/* para o PDFKit: percorre a página sem desenhar (num bitmap de 1×1 descartado) e entrega cada glifo */
void cx_pdf_page_glyphs(void *page, void (*cb)(void *info, const u16 *uni, int nu, CGRect box, double fsize), void *info) {
    void *dev = CGColorSpaceCreateDeviceRGB();
    void *ctx = CGBitmapContextCreate(0, 1, 1, 8, 4, dev, 1);
    CGColorSpaceRelease(dev);
    if (!ctx) return;
    rnd *r = calloc(1, sizeof *r);
    if (r) {
        r->ctx = ctx;
        r->on_glyph = cb;
        r->glyph_info = info;
        render_page(r, page);
        free(r);
    }
    CGContextRelease(ctx);
}

/* ---------------------------------------------------------------- texto da página (PDFKit) */
/* Os glifos na ordem do conteúdo, com um espaço onde há um vão maior que 0,2 em entre glifos da mesma linha e uma
 * quebra de linha quando a linha de base muda (ou o texto volta para trás). Cada caractere tem a caixa no espaço
 * da página (o inserido tem caixa de largura zero no lugar do vão). */
typedef struct { u16 *ch; CGRect *box; int n, cap; double px, py, pw, ph, ps; int have; } pk_text;
static void pk_push(pk_text *t, u16 c, CGRect b) {
    if (t->n + 1 > t->cap) {
        int nc = t->cap ? t->cap * 2 : 256;
        u16 *a = realloc(t->ch, (size_t)nc * sizeof *a);
        CGRect *r = realloc(t->box, (size_t)nc * sizeof *r);
        if (a) t->ch = a;
        if (r) t->box = r;
        if (!a || !r) return;
        t->cap = nc;
    }
    t->ch[t->n] = c;
    t->box[t->n++] = b;
}
static void pk_glyph(void *info, const u16 *uni, int nu, CGRect box, double fsize) {
    pk_text *t = info;
    if (nu <= 0) return;
    double fs = fsize > 0.1 ? fsize : 1;
    if (box.size.width < 0) { box.origin.x += box.size.width; box.size.width = -box.size.width; }
    if (box.size.height < 0) { box.origin.y += box.size.height; box.size.height = -box.size.height; }
    if (t->have && t->n) {
        double dy = box.origin.y - t->py, gap = box.origin.x - (t->px + t->pw);
        u16 last = t->ch[t->n - 1];
        double lim = (fs < t->ps ? fs : t->ps);
        if (dy > 0.5 * lim || dy < -0.5 * lim || gap < -2 * lim) {
            if (last != '\n') { CGRect z = {{t->px + t->pw, t->py}, {0, t->ph}}; pk_push(t, '\n', z); }
        } else if (gap > 0.2 * lim && last != ' ' && uni[0] != ' ') {
            CGRect z = {{t->px + t->pw, t->py}, {gap, t->ph}};
            pk_push(t, ' ', z);
        }
    }
    double w1 = box.size.width / nu;
    for (int i = 0; i < nu; i++) {
        CGRect b = {{box.origin.x + w1 * i, box.origin.y}, {w1, box.size.height}};
        pk_push(t, uni[i], b);
    }
    t->px = box.origin.x; t->py = box.origin.y; t->pw = box.size.width; t->ph = box.size.height; t->ps = fs;
    t->have = 1;
}
/* texto da página: *chars (UTF-16) e *boxes em malloc, devolve o número de caracteres */
int cx_pdf_page_text(void *page, u16 **chars, CGRect **boxes) {
    pk_text t;
    memset(&t, 0, sizeof t);
    cx_pdf_page_glyphs(page, pk_glyph, &t);
    *chars = t.ch;
    *boxes = t.box;
    return t.n;
}
