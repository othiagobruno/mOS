/*
 * mOS libSystem — desenho do AppKit fora da janela, num CGContext (PDF ou bitmap). Antes o AppKit só desenhava no
 * lote da janela: -[NSImage lockFocus] não fazia nada, não havia -cacheDisplayInRect:toBitmapImageRep:,
 * -dataWithPDFInsideRect: nem impressão, e +[NSGraphicsContext graphicsContextWithCGContext:flipped:] não existia.
 *
 * Como: o appkit.c grava o lote (cx_gc_record_*) com a view em 0,0; aqui o lote é reproduzido nas funções do Core
 * Graphics (appkit_gcreplay.c, teste userland/tests-appkit/gcreplay_host.c). O mesmo caminho serve para o que o app
 * desenha com um contexto CG corrente (NSRectFill, NSBezierPath, texto, imagens), entre setCurrentContext: e a volta.
 *
 * APIs: NSView dataWithPDFInsideRect:, bitmapImageRepForCachingDisplayInRect:, cacheDisplayInRect:toBitmapImageRep:,
 * displayRectIgnoringOpacity:inContext:; NSGraphicsContext graphicsContextWithCGContext:flipped:,
 * graphicsContextWithBitmapImageRep:, currentContext/setCurrentContext:, save/restoreGraphicsState, CGContext,
 * isFlipped, flushGraphics; NSImage lockFocus, lockFocusFlipped:, unlockFocus, imageWithSize:flipped:drawingHandler:;
 * NSBitmapImageRep initWithCGImage:, CGImage, representationUsingType:properties: (PNG/JPEG/TIFF/GIF/BMP pelo ImageIO),
 * TIFFRepresentation; NSImage TIFFRepresentation.
 * Limite: chamadas CG diretas do app num contexto corrente ficam na ordem certa em relação ao que o AppKit desenhou
 * antes de cada -CGContext (o lote é descarregado ali), não intercaladas linha a linha.
 * Registro: cx_appkit_offscreen_init(), chamado do cx_appkit_input_init.
 */
#include "foundation.h"
#include "sys.h"
#include "libc.h"
#include "appkit_gcreplay.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void objc_setAssociatedObject(id obj, const void *key, id value, u64 policy);
id objc_getAssociatedObject(id obj, const void *key);
IMP class_getMethodImplementation(cx_class_t *c, SEL s);
void free(void *p);
id cx_color(double r, double g, double b, double a);

typedef struct { double x, y; } P;
typedef struct { double w, h; } S;
typedef struct { double x, y, w, h; } R;

/* Core Graphics / Core Text / ImageIO */
void CGContextSaveGState(id c);
void CGContextRestoreGState(id c);
void CGContextClipToRect(id c, R r);
void CGContextSetRGBFillColor(id c, double r, double g, double b, double a);
void CGContextSetRGBStrokeColor(id c, double r, double g, double b, double a);
void CGContextFillRect(id c, R r);
void CGContextBeginPath(id c);
void CGContextMoveToPoint(id c, double x, double y);
void CGContextAddLineToPoint(id c, double x, double y);
void CGContextAddArcToPoint(id c, double x1, double y1, double x2, double y2, double r);
void CGContextClosePath(id c);
void CGContextFillPath(id c);
void CGContextStrokePath(id c);
void CGContextSetLineWidth(id c, double w);
void CGContextSetBlendMode(id c, int mode);
void CGContextDrawImage(id c, R r, void *img);
void CGContextSetTextPosition(id c, double x, double y);
void CGContextRelease(id c);
void *CGImageCreate(size_t w, size_t h, size_t bpc, size_t bpp, size_t bpr, const void *space, u32 info, void *prov, const void *dec, _Bool interp, int intent);
void CGImageRelease(void *i);
void *CGDataProviderCreateWithData(void *info, const void *data, size_t size, void (*rel)(void *, const void *, size_t));
void CGDataProviderRelease(void *p);
void *CGColorSpaceCreateDeviceRGB(void);
void CGColorSpaceRelease(void *s);
id CGBitmapContextCreate(void *data, size_t w, size_t h, size_t bpc, size_t bpr, void *space, u32 info);
void *CGBitmapContextCreateImage(id c);
size_t CGBitmapContextGetWidth(id c);
size_t CGBitmapContextGetHeight(id c);
R CGContextGetClipBoundingBox(id c);
id CGDataConsumerCreateWithCFData(id data);
void CGDataConsumerRelease(id c);
id CGPDFContextCreate(id consumer, const R *media, id info);
void CGPDFContextBeginPage(id c, id info);
void CGPDFContextEndPage(id c);
void CGPDFContextClose(id c);
id CGImageDestinationCreateWithData(id data, id type, size_t count, id opts);
void CGImageDestinationAddImage(id dest, void *img, id props);
BOOL CGImageDestinationFinalize(id dest);
id CTFontCreateWithName(id name, double size, const void *matrix);
id CTLineCreateWithAttributedString(id astr);
void CTLineDraw(id line, id ctx);
id CFAttributedStringCreate(void *alloc, id str, id attrs);
extern const void *kCTFontAttributeName, *kCTForegroundColorAttributeName;
int cx_bm_is(const void *c);
void *cx_gc_record_begin(double w, double h, int flipped);
const u8 *cx_gc_record_buffer(u32 *n);
void cx_gc_record_end(void *sv);
void cx_gc_record_view(id v, double rx, double ry, double rw, double rh);
void cx_nsimage_set_pixels(id img, const u8 *src, long bpr, int w, int h, int premul);
int cx_cgimage_src(const void *img, const u8 **px, long *bpr, int *w, int *h, int *premul);

static cx_class_t *kls(const char *n) { return cx_class_named(n); }
static SEL sl(const char *n) { return sel_registerName(n); }
static BOOL responds(id o, const char *s) { return o && cx_responds(object_getClass(o), sl(s)); }
static R msg_rect(id o, const char *s) { id objc_msgSend_stret(id, SEL, ...); return ((R (*)(id, SEL))objc_msgSend_stret)(o, sl(s)); }
static S msg_size(id o, const char *s) { return ((S (*)(id, SEL))objc_msgSend)(o, sl(s)); }

/* ================================================================ destino CG do reprodutor */
typedef struct { id cg; double w, h; int clipped; } cgsink_t;
static void rgba(u32 c, double *r, double *g, double *b, double *a) { *a = (c >> 24) / 255.0; *r = ((c >> 16) & 255) / 255.0; *g = ((c >> 8) & 255) / 255.0; *b = (c & 255) / 255.0; }
static void set_fill(id cg, u32 c) { double r, g, b, a; rgba(c, &r, &g, &b, &a); CGContextSetRGBFillColor(cg, r, g, b, a); }
static void set_stroke(id cg, u32 c) { double r, g, b, a; rgba(c, &r, &g, &b, &a); CGContextSetRGBStrokeColor(cg, r, g, b, a); }
static void rrect_path(id cg, double x, double y, double w, double h, double r) {
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    CGContextBeginPath(cg);
    CGContextMoveToPoint(cg, x + r, y);
    CGContextAddArcToPoint(cg, x + w, y, x + w, y + h, r);
    CGContextAddArcToPoint(cg, x + w, y + h, x, y + h, r);
    CGContextAddArcToPoint(cg, x, y + h, x, y, r);
    CGContextAddArcToPoint(cg, x, y, x + w, y, r);
    CGContextClosePath(cg);
}
static void k_clear(void *c, u32 argb) { cgsink_t *s = c; set_fill(s->cg, argb); CGContextFillRect(s->cg, (R){0, 0, s->w, s->h}); }
static void k_fill(void *c, double x, double y, double w, double h, double r, u32 argb) {
    cgsink_t *s = c;
    set_fill(s->cg, argb);
    if (r <= 0.01) { CGContextFillRect(s->cg, (R){x, y, w, h}); return; }
    rrect_path(s->cg, x, y, w, h, r);
    CGContextFillPath(s->cg);
}
static void k_stroke(void *c, double x, double y, double w, double h, double r, double t, u32 argb) {
    cgsink_t *s = c;
    set_stroke(s->cg, argb);
    CGContextSetLineWidth(s->cg, t > 0 ? t : 1);
    rrect_path(s->cg, x, y, w, h, r > 0 ? r : 0);
    CGContextStrokePath(s->cg);
}
static void k_line(void *c, double x0, double y0, double x1, double y1, double t, u32 argb) {
    cgsink_t *s = c;
    set_stroke(s->cg, argb);
    CGContextSetLineWidth(s->cg, t > 0 ? t : 1);
    CGContextBeginPath(s->cg);
    CGContextMoveToPoint(s->cg, x0, y0);
    CGContextAddLineToPoint(s->cg, x1, y1);
    CGContextStrokePath(s->cg);
}
static void k_poly(void *c, const double *xy, u32 n, u32 argb, int fill, double t) {
    cgsink_t *s = c;
    CGContextBeginPath(s->cg);
    CGContextMoveToPoint(s->cg, xy[0], xy[1]);
    for (u32 i = 1; i < n; i++) CGContextAddLineToPoint(s->cg, xy[2 * i], xy[2 * i + 1]);
    if (fill) { CGContextClosePath(s->cg); set_fill(s->cg, argb); CGContextFillPath(s->cg); }
    else { set_stroke(s->cg, argb); CGContextSetLineWidth(s->cg, t > 0 ? t : 1); CGContextStrokePath(s->cg); }
}
static void k_text(void *c, double x, double base, double size, int weight, u32 argb, const char *str, u32 n) {
    cgsink_t *s = c;
    id text = cx_str_n(str, n);
    id font = CTFontCreateWithName(cx_str(weight >= 3 ? ".AppleSystemUIFontBold" : ".AppleSystemUIFont"), size > 0 ? size : 13, 0);
    double r, g, b, a;
    rgba(argb, &r, &g, &b, &a);
    id attrs = MSG0(id, (id)kls("NSMutableDictionary"), "dictionary");
    if (font) MSG(void, attrs, "setObject:forKey:", font, (id)kCTFontAttributeName);
    MSG(void, attrs, "setObject:forKey:", cx_color(r, g, b, a), (id)kCTForegroundColorAttributeName);
    id as = CFAttributedStringCreate(0, text, attrs);
    id line = as ? CTLineCreateWithAttributedString(as) : 0;
    if (line) { CGContextSetTextPosition(s->cg, x, base); CTLineDraw(line, s->cg); objc_release(line); }
    objc_release(as);
    objc_release(font);
}
/* cada GC_CLIP do lote substitui o anterior: restaura o estado do clip de antes e aplica o novo */
static void k_clip(void *c, double x, double y, double w, double h) {
    cgsink_t *s = c;
    if (s->clipped) CGContextRestoreGState(s->cg);
    CGContextSaveGState(s->cg);
    CGContextClipToRect(s->cg, (R){x, y, w, h});
    s->clipped = 1;
}
static void k_unclip(void *c) { cgsink_t *s = c; if (s->clipped) { CGContextRestoreGState(s->cg); s->clipped = 0; } }
/* degradê vertical: uma faixa de 1 ponto por linha, de cima (top) para baixo (bottom) */
static void k_grad(void *c, double x, double y, double w, double h, u32 top, u32 bot) {
    cgsink_t *s = c;
    int n = (int)(h + 0.5);
    for (int i = 0; i < n; i++) {
        double t = n > 1 ? (double)i / (n - 1) : 0;
        u32 col = 0;
        for (int sh = 0; sh < 32; sh += 8) {
            double a0 = (top >> sh) & 255, a1 = (bot >> sh) & 255;
            col |= (u32)(a0 + (a1 - a0) * t + 0.5) << sh;
        }
        set_fill(s->cg, col);
        CGContextFillRect(s->cg, (R){x, y + h - 1 - i, w, 1});   /* y já está virado: a linha 0 (top) fica em cima */
    }
}
static void img_release(void *info, const void *data, size_t n) { (void)info; (void)n; free((void *)data); }
static int blend_for(u32 op) {
    static const int map[] = {16, 17, 0, 18, 19, 20, 21, 22, 23, 24, 25, 26, 0, 27, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    return op < sizeof map / sizeof map[0] ? map[op] : 0;
}
static void k_image(void *c, double x, double y, double w, double h, const void *argb, int pw, int ph, u32 op) {
    cgsink_t *s = c;
    size_t n = (size_t)pw * (size_t)ph * 4;
    void *copy = malloc(n);
    if (!copy) return;
    memcpy(copy, argb, n);
    void *prov = CGDataProviderCreateWithData(0, copy, n, img_release);
    void *space = CGColorSpaceCreateDeviceRGB();
    void *img = CGImageCreate((size_t)pw, (size_t)ph, 8, 32, (size_t)pw * 4, space, 4u | (2u << 12), prov, 0, 1, 0);
    CGDataProviderRelease(prov);
    CGColorSpaceRelease(space);
    if (!img) return;
    if (op != 2) CGContextSetBlendMode(s->cg, blend_for(op));
    CGContextDrawImage(s->cg, (R){x, y, w, h}, img);
    if (op != 2) CGContextSetBlendMode(s->cg, 0);
    CGImageRelease(img);
}
static void k_comp(void *c, double x, double y, double w, double h, u32 argb, u32 op) {
    cgsink_t *s = c;
    CGContextSetBlendMode(s->cg, blend_for(op));
    set_fill(s->cg, argb);
    CGContextFillRect(s->cg, (R){x, y, w, h});
    CGContextSetBlendMode(s->cg, 0);
}
static void replay_into(id cg, double w, double h) {
    u32 n = 0;
    const u8 *buf = cx_gc_record_buffer(&n);
    if (!buf || !n || !cg) return;
    cgsink_t s = {cg, w, h, 0};
    gcr_sink_t k = {&s, k_clear, k_fill, k_stroke, k_line, k_poly, k_text, k_clip, k_unclip, k_grad, k_image, k_comp};
    gcr_replay(buf, n, h, 1, &k);
    if (s.clipped) CGContextRestoreGState(cg);
}
/* desenha a região r da view no cg (origem do cg embaixo à esquerda, r.w × r.h) */
void cx_view_render_to_cg(id view, R r, id cg) {
    void *sv = cx_gc_record_begin(r.w, r.h, 0);
    if (!sv) return;
    cx_gc_record_view(view, r.x, r.y, r.w, r.h);
    replay_into(cg, r.w, r.h);
    cx_gc_record_end(sv);
}

/* ================================================================ NSView */
static id v_pdf(id self, SEL c, R r) {
    (void)c;
    id data = MSG0(id, (id)kls("NSMutableData"), "data");
    id cons = CGDataConsumerCreateWithCFData(data);
    R media = {0, 0, r.w, r.h};
    id pdf = cons ? CGPDFContextCreate(cons, &media, 0) : 0;
    if (!pdf) { if (cons) CGDataConsumerRelease(cons); return 0; }
    CGPDFContextBeginPage(pdf, 0);
    cx_view_render_to_cg(self, r, pdf);
    CGPDFContextEndPage(pdf);
    CGPDFContextClose(pdf);
    CGContextRelease(pdf);
    CGDataConsumerRelease(cons);
    return data;
}
static id rep_new(double w, double h) {
    i64 pw = (i64)(w + 0.5), ph = (i64)(h + 0.5);
    if (pw < 1) pw = 1;
    if (ph < 1) ph = 1;
    id rep = MSG0(id, (id)kls("NSBitmapImageRep"), "alloc");
    rep = ((id (*)(id, SEL, u8 **, i64, i64, i64, i64, BOOL, BOOL, id, i64, i64))objc_msgSend)(rep,
        sl("initWithBitmapDataPlanes:pixelsWide:pixelsHigh:bitsPerSample:samplesPerPixel:hasAlpha:isPlanar:colorSpaceName:bytesPerRow:bitsPerPixel:"),
        (u8 **)0, pw, ph, (i64)8, (i64)4, (BOOL)YES, (BOOL)NO, cx_str("NSCalibratedRGBColorSpace"), pw * 4, (i64)32);
    return rep ? objc_autorelease(rep) : 0;
}
static id v_bitmap_rep(id self, SEL c, R r) { (void)self; (void)c; return rep_new(r.w, r.h); }
/* o NSBitmapImageRep do mOS é RGBA pré-multiplicado (appkit_extra.c): um CGBitmapContext por cima dos bytes dele */
static id rep_context(id rep) {
    u8 *data = MSG0(u8 *, rep, "bitmapData");
    i64 w = MSG0(i64, rep, "pixelsWide"), h = MSG0(i64, rep, "pixelsHigh"), bpr = MSG0(i64, rep, "bytesPerRow");
    if (!data || w <= 0 || h <= 0) return 0;
    void *space = CGColorSpaceCreateDeviceRGB();
    id cg = CGBitmapContextCreate(data, (size_t)w, (size_t)h, 8, (size_t)bpr, space, 1 /* PremultipliedLast */);
    CGColorSpaceRelease(space);
    return cg;
}
static void v_cache(id self, SEL c, R r, id rep) {
    (void)c;
    if (!rep) return;
    id cg = rep_context(rep);
    if (!cg) return;
    u8 *data = MSG0(u8 *, rep, "bitmapData");
    i64 h = MSG0(i64, rep, "pixelsHigh"), bpr = MSG0(i64, rep, "bytesPerRow");
    memset(data, 0, (size_t)(bpr * h));
    cx_view_render_to_cg(self, r, cg);
    CGContextRelease(cg);
}
static id gc_cg_of(id gc);
static void v_display_in_ctx(id self, SEL c, R r, id gc) {
    (void)c;
    id cg = gc ? gc_cg_of(gc) : 0;
    if (cg) cx_view_render_to_cg(self, r, cg);
}

/* ================================================================ NSGraphicsContext com CGContext */
static cx_class_t cgk_c, cgk_m;
static char k_cg, k_flip, k_w, k_h, k_rec;
static id g_current;          /* contexto CG corrente (nil = a janela) */
static id g_stack[32];
static int g_sp;
static IMP g_cur_orig, g_save_orig, g_restore_orig;
static double num(id o, const void *k) { id v = objc_getAssociatedObject(o, k); return v ? MSG0(double, v, "doubleValue") : 0; }
static id gc_cg_of(id gc) {
    if (gc && object_getClass(gc) == &cgk_c) return objc_getAssociatedObject(gc, &k_cg);
    return gc ? MSG0(id, gc, "CGContext") : 0;
}
static id gc_make(id cg, BOOL flipped) {
    if (!cg) return 0;
    id gc = cx_alloc_instance(&cgk_c);
    objc_setAssociatedObject(gc, &k_cg, cg, 1);
    objc_setAssociatedObject(gc, &k_flip, cx_num_i(flipped), 1);
    double w = 0, h = 0;
    if (cx_bm_is(cg)) { w = (double)CGBitmapContextGetWidth(cg); h = (double)CGBitmapContextGetHeight(cg); }
    if (w <= 0 || h <= 0) { R b = CGContextGetClipBoundingBox(cg); w = b.w; h = b.h; }
    if (w <= 0 || h <= 0 || w > 1e6 || h > 1e6) { w = 612; h = 792; }   /* sem tamanho conhecido: carta (PDF) */
    objc_setAssociatedObject(gc, &k_w, cx_num_d(w), 1);
    objc_setAssociatedObject(gc, &k_h, cx_num_d(h), 1);
    return objc_autorelease(gc);
}
static id gc_with_cg(id k, SEL c, id cg, BOOL flipped) { (void)k; (void)c; return gc_make(cg, flipped); }
static id gc_with_rep(id k, SEL c, id rep) {
    (void)k; (void)c;
    id cg = rep ? rep_context(rep) : 0;
    id gc = gc_make(cg, NO);
    if (cg) CGContextRelease(cg);   /* o NSGraphicsContext retém */
    return gc;
}
/* descarrega no CGContext o que o AppKit já desenhou e continua gravando */
static void gc_flush_rec(id gc) {
    void *sv = gc ? (void *)MSG0(void *, objc_getAssociatedObject(gc, &k_rec), "pointerValue") : 0;
    if (!sv) return;
    id cg = objc_getAssociatedObject(gc, &k_cg);
    double w = num(gc, &k_w), h = num(gc, &k_h);
    replay_into(cg, w, h);
    cx_gc_record_end(sv);
    void *nsv = cx_gc_record_begin(w, h, (int)num(gc, &k_flip));
    objc_setAssociatedObject(gc, &k_rec, MSG(id, (id)kls("NSValue"), "valueWithPointer:", nsv), 1);
}
static void gc_stop_rec(id gc) {
    void *sv = gc ? (void *)MSG0(void *, objc_getAssociatedObject(gc, &k_rec), "pointerValue") : 0;
    if (!sv) return;
    replay_into(objc_getAssociatedObject(gc, &k_cg), num(gc, &k_w), num(gc, &k_h));
    cx_gc_record_end(sv);
    objc_setAssociatedObject(gc, &k_rec, 0, 1);
}
static void gc_start_rec(id gc) {
    if (!gc || object_getClass(gc) != &cgk_c || objc_getAssociatedObject(gc, &k_rec)) return;
    void *sv = cx_gc_record_begin(num(gc, &k_w), num(gc, &k_h), (int)num(gc, &k_flip));
    objc_setAssociatedObject(gc, &k_rec, MSG(id, (id)kls("NSValue"), "valueWithPointer:", sv), 1);
}
static void set_current(id gc) {
    if (gc == g_current) return;
    if (g_current) gc_stop_rec(g_current);
    objc_retain(gc); objc_release(g_current);
    g_current = gc && object_getClass(gc) == &cgk_c ? gc : 0;
    if (!g_current) objc_release(gc);
    if (g_current) gc_start_rec(g_current);
}
static id gc_current_hook(id k, SEL c) { if (g_current) return g_current; return g_cur_orig ? ((id (*)(id, SEL))g_cur_orig)(k, c) : 0; }
static void gc_set_current(id k, SEL c, id gc) { (void)k; (void)c; set_current(gc); }
static void gc_save_hook(id k, SEL c) {
    if (g_sp < 32) g_stack[g_sp++] = objc_retain(g_current);
    if (g_save_orig) ((void (*)(id, SEL))g_save_orig)(k, c);
}
static void gc_restore_hook(id k, SEL c) {
    if (g_restore_orig) ((void (*)(id, SEL))g_restore_orig)(k, c);
    if (g_sp > 0) { id prev = g_stack[--g_sp]; set_current(prev); objc_release(prev); }
}
static id gck_cg(id self, SEL c) { (void)c; gc_flush_rec(self); return objc_getAssociatedObject(self, &k_cg); }
static BOOL gck_flipped(id self, SEL c) { (void)c; return num(self, &k_flip) != 0; }
static BOOL gck_screen(id self, SEL c) { (void)self; (void)c; return NO; }
static void gck_flush(id self, SEL c) { (void)c; gc_flush_rec(self); }
static void gck_noop(id s, SEL c) { (void)s; (void)c; }
static void gck_noop_b(id s, SEL c, BOOL b) { (void)s; (void)c; (void)b; }
static BOOL gck_yes(id s, SEL c) { (void)s; (void)c; return YES; }

/* ================================================================ NSImage: lockFocus e drawingHandler */
static char k_focus_gc, k_focus_cg;
static void im_lock(id self, BOOL flipped) {
    S z = msg_size(self, "size");
    size_t w = (size_t)(z.w + 0.5), h = (size_t)(z.h + 0.5);
    if (!w || !h) return;
    void *space = CGColorSpaceCreateDeviceRGB();
    id cg = CGBitmapContextCreate(0, w, h, 8, w * 4, space, 2u | (2u << 12) /* PremultipliedFirst, 32 little = ARGB */);
    CGColorSpaceRelease(space);
    if (!cg) return;
    void *old = MSG(void *, self, "CGImageForProposedRect:context:hints:", (void *)0, (id)0, (id)0);   /* desenha por cima do que já havia */
    if (old) CGContextDrawImage(cg, (R){0, 0, (double)w, (double)h}, old);
    id gc = gc_make(cg, flipped);
    objc_setAssociatedObject(self, &k_focus_cg, cg, 1);
    CGContextRelease(cg);
    objc_setAssociatedObject(self, &k_focus_gc, gc, 1);
    gc_save_hook((id)kls("NSGraphicsContext"), sl("saveGraphicsState"));
    set_current(gc);
}
static void im_lock_focus(id self, SEL c) { (void)c; im_lock(self, NO); }
static void im_lock_flipped(id self, SEL c, BOOL f) { (void)c; im_lock(self, f); }
static void im_unlock_focus(id self, SEL c) {
    (void)c;
    id gc = objc_getAssociatedObject(self, &k_focus_gc);
    id cg = objc_getAssociatedObject(self, &k_focus_cg);
    if (!gc || !cg) return;
    gc_restore_hook((id)kls("NSGraphicsContext"), sl("restoreGraphicsState"));   /* termina a gravação e reproduz no bitmap */
    if (g_current == gc) set_current(0);
    void *img = CGBitmapContextCreateImage(cg);
    const u8 *px; long bpr; int w, h, premul;
    if (img && cx_cgimage_src(img, &px, &bpr, &w, &h, &premul)) cx_nsimage_set_pixels(self, px, bpr, w, h, premul);
    if (img) CGImageRelease(img);
    objc_setAssociatedObject(self, &k_focus_gc, 0, 1);
    objc_setAssociatedObject(self, &k_focus_cg, 0, 1);
}
/* +imageWithSize:flipped:drawingHandler: desenha na hora (o Cocoa adia até o 1º desenho; o resultado é o mesmo) */
static id im_with_handler(id k, SEL c, S size, BOOL flipped, void *blk) {
    (void)c;
    id img = ((id (*)(id, SEL, S))objc_msgSend)(MSG0(id, k, "alloc"), sl("initWithSize:"), size);
    if (!img) return 0;
    if (blk && size.w >= 1 && size.h >= 1) {
        im_lock(img, flipped);
        ((BOOL (*)(void *, R))(*(void **)((u8 *)blk + 16)))(blk, (R){0, 0, size.w, size.h});
        im_unlock_focus(img, 0);
    }
    return objc_autorelease(img);
}

/* ================================================================ NSBitmapImageRep <-> CGImage e codificação */
static void *rep_cgimage(id rep) {
    id cg = rep_context(rep);
    if (!cg) return 0;
    void *img = CGBitmapContextCreateImage(cg);
    CGContextRelease(cg);
    return img;
}
static char k_rep_cg;
static void *rep_cg_m(id self, SEL c) {
    (void)c;
    void *img = rep_cgimage(self);
    if (!img) return 0;
    objc_setAssociatedObject(self, &k_rep_cg, (id)img, 1);   /* fica com o rep, como no Cocoa */
    CGImageRelease(img);
    return img;
}
static id rep_init_cg(id self, SEL c, void *img) {
    (void)c;
    const u8 *px; long bpr; int w, h, premul;
    if (!img || !cx_cgimage_src(img, &px, &bpr, &w, &h, &premul)) return 0;
    self = ((id (*)(id, SEL, u8 **, i64, i64, i64, i64, BOOL, BOOL, id, i64, i64))objc_msgSend)(self,
        sl("initWithBitmapDataPlanes:pixelsWide:pixelsHigh:bitsPerSample:samplesPerPixel:hasAlpha:isPlanar:colorSpaceName:bytesPerRow:bitsPerPixel:"),
        (u8 **)0, (i64)w, (i64)h, (i64)8, (i64)4, (BOOL)YES, (BOOL)NO, cx_str("NSCalibratedRGBColorSpace"), (i64)w * 4, (i64)32);
    if (!self) return 0;
    id cg = rep_context(self);
    if (cg) { CGContextDrawImage(cg, (R){0, 0, (double)w, (double)h}, img); CGContextRelease(cg); }
    return self;
}
static id encode(void *img, u64 type, id props) {
    static const char *uti[] = {"public.tiff", "com.microsoft.bmp", "com.compuserve.gif", "public.jpeg", "public.png", "public.jpeg-2000"};
    if (!img || type > 5) return 0;
    id data = MSG0(id, (id)kls("NSMutableData"), "data");
    id dst = CGImageDestinationCreateWithData(data, cx_str(uti[type]), 1, 0);
    if (!dst) return 0;
    id cprops = 0;
    id q = props ? MSG(id, props, "objectForKey:", cx_str("NSImageCompressionFactor")) : 0;
    if (q) cprops = MSG(id, (id)kls("NSDictionary"), "dictionaryWithObject:forKey:", q, cx_str("kCGImageDestinationLossyCompressionQuality"));
    CGImageDestinationAddImage(dst, img, cprops);
    BOOL ok = CGImageDestinationFinalize(dst);
    objc_release(dst);
    return ok ? data : 0;
}
static id rep_repr(id self, SEL c, u64 type, id props) {
    (void)c;
    void *img = rep_cgimage(self);
    id d = encode(img, type, props);
    if (img) CGImageRelease(img);
    return d;
}
static id rep_tiff(id self, SEL c) { return rep_repr(self, c, 0, 0); }
static id im_tiff(id self, SEL c) {
    (void)c;
    void *img = MSG(void *, self, "CGImageForProposedRect:context:hints:", (void *)0, (id)0, (id)0);
    return encode(img, 0, 0);
}

#define ADD(cls, sel, fn) cx_add_method(cls, sel, (void *)(fn))
void cx_appkit_offscreen_init(void) {
    cx_class_t *view = kls("NSView"), *gcc = kls("NSGraphicsContext"), *img = kls("NSImage"), *rep = kls("NSBitmapImageRep");
    if (view) {
        ADD(view, "dataWithPDFInsideRect:", v_pdf);
        ADD(view, "bitmapImageRepForCachingDisplayInRect:", v_bitmap_rep);
        ADD(view, "cacheDisplayInRect:toBitmapImageRep:", v_cache);
        ADD(view, "displayRectIgnoringOpacity:inContext:", v_display_in_ctx);
    }
    if (gcc) {
        cx_define_class(&cgk_c, &cgk_m, "_MOSCGGraphicsContext", gcc, 0);
        ADD(&cgk_c, "CGContext", gck_cg); ADD(&cgk_c, "graphicsPort", gck_cg); ADD(&cgk_c, "isFlipped", gck_flipped);
        ADD(&cgk_c, "isDrawingToScreen", gck_screen); ADD(&cgk_c, "flushGraphics", gck_flush);
        ADD(&cgk_c, "saveGraphicsState", gck_noop); ADD(&cgk_c, "restoreGraphicsState", gck_noop);
        ADD(&cgk_c, "setShouldAntialias:", gck_noop_b); ADD(&cgk_c, "shouldAntialias", gck_yes);
        g_cur_orig = cx_lookup(gcc->isa, sl("currentContext"));
        g_save_orig = cx_lookup(gcc->isa, sl("saveGraphicsState"));
        g_restore_orig = cx_lookup(gcc->isa, sl("restoreGraphicsState"));
        cx_add_cmethod(gcc, "currentContext", (void *)gc_current_hook);
        cx_add_cmethod(gcc, "setCurrentContext:", (void *)gc_set_current);
        cx_add_cmethod(gcc, "saveGraphicsState", (void *)gc_save_hook);
        cx_add_cmethod(gcc, "restoreGraphicsState", (void *)gc_restore_hook);
        cx_add_cmethod(gcc, "graphicsContextWithCGContext:flipped:", (void *)gc_with_cg);
        cx_add_cmethod(gcc, "graphicsContextWithBitmapImageRep:", (void *)gc_with_rep);
    }
    if (img) {
        ADD(img, "lockFocus", im_lock_focus); ADD(img, "lockFocusFlipped:", im_lock_flipped); ADD(img, "unlockFocus", im_unlock_focus);
        ADD(img, "TIFFRepresentation", im_tiff);
        cx_add_cmethod(img, "imageWithSize:flipped:drawingHandler:", (void *)im_with_handler);
    }
    if (rep) {
        ADD(rep, "initWithCGImage:", rep_init_cg); ADD(rep, "CGImage", rep_cg_m);
        ADD(rep, "representationUsingType:properties:", rep_repr); ADD(rep, "TIFFRepresentation", rep_tiff);
    }
}
