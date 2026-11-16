/* CGBitmapContext de ponta a ponta: coregraphics.c (com coregraphics_bm.inc) + cg_raster.c de verdade,
 * sobre um runtime mínimo (classes, retain/release que chama o dealloc, CFArray). Confere pixels.
 *   zig cc -g -O1 -fno-sanitize=all -ffreestanding -fno-builtin -w -Iuserland/libsystem/kshim -Iuserland/libsystem/fsinc \
 *     -Ikernel/include -Iuserland/libsystem -c userland/libsystem/coregraphics.c -o /tmp/cg-lib.o
 *   zig cc -std=gnu11 -O1 -w -D_DEFAULT_SOURCE -Iuserland/libsystem tools/cg_context_test.c /tmp/cg-lib.o \
 *     userland/libsystem/cg_raster.c userland/libsystem/cgimage_crop.c -DCS_HOST userland/libsystem/colorsync.c -lm \
 *     -o /tmp/cg_context_test && /tmp/cg_context_test
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void *id;
typedef const char *SEL;
typedef void *IMP;
typedef struct { double x, y; } CGPoint;
typedef struct { double width, height; } CGSize;
typedef struct { CGPoint origin; CGSize size; } CGRect;
typedef struct { double a, b, c, d, tx, ty; } CGAffineTransform;

/* ---------------------------------------------------------------- runtime mínimo */
typedef struct mcls { const char *name; struct mcls *super; size_t size; struct { const char *sel; IMP imp; } m[8]; int nm; } mcls;
static mcls *classes[64];
static int nclasses;
typedef struct { long rc; } hdr_t;
static mcls *cls_info(void *k) {
    for (int i = 0; i < nclasses; i++) if (classes[i] == k) return classes[i];
    return 0;
}
void *cx_class_named(const char *name) {
    for (int i = 0; i < nclasses; i++) if (!strcmp(classes[i]->name, name)) return classes[i];
    if (!strcmp(name, "NSColor")) { extern mcls color_cls; return &color_cls; }
    if (!strcmp(name, "NSObject")) {
        static mcls ns = {"NSObject", 0, 8, {{0, 0}}, 0};
        classes[nclasses++] = &ns;
        return &ns;
    }
    return 0;
}
/* as classes estáticas do coregraphics.c (cx_class_t) são grandes o bastante: guardo o mcls numa tabela à parte */
static struct { void *k; mcls c; } statics[16];
static int nstatics;
void cx_define_class(void *k, void *m, const char *name, void *super, uint32_t extra) {
    (void)m;
    mcls *c = &statics[nstatics].c;
    statics[nstatics++].k = k;
    c->name = name; c->super = super; c->size = 8 + extra;
    classes[nclasses++] = c;
    memcpy(k, &c, sizeof c);                  /* o primeiro word da classe estática aponta o mcls */
}
static mcls *of_static(void *k) { mcls *c; memcpy(&c, k, sizeof c); return c; }
void cx_add_method(void *k, const char *sel, void *imp) {
    mcls *c = cls_info(k) ? cls_info(k) : of_static(k);
    c->m[c->nm].sel = sel; c->m[c->nm].imp = imp; c->nm++;
}
static void *alloc_of(mcls *c) {
    hdr_t *h = calloc(1, sizeof *h + c->size + 64);
    h->rc = 1;
    void **o = (void **)(h + 1);
    o[0] = c;
    return o;
}
void *cx_alloc_instance(void *k) { return alloc_of(cls_info(k) ? cls_info(k) : of_static(k)); }
void cx_object_free(void *o) { free((hdr_t *)o - 1); }
void *objc_allocateClassPair(void *super, const char *name, uint64_t extra) {
    mcls *c = calloc(1, sizeof *c);
    c->name = name; c->super = super; c->size = 8 + extra;
    classes[nclasses++] = c;                  /* já conta como classe: os métodos entram antes do register */
    return c;
}
int class_addIvar(void *k, const char *n, size_t size, uint8_t a, const char *t) { (void)n; (void)a; (void)t; ((mcls *)k)->size += size; return 1; }
int class_addMethod(void *k, const char *sel, void *imp, const char *t) { (void)t; cx_add_method(k, sel, imp); return 1; }
void objc_registerClassPair(void *k) { (void)k; }
void *class_createInstance(void *k, uint64_t extra) { (void)extra; return alloc_of(k); }
void *class_getMethodImplementation(void *k, const char *sel) { (void)k; (void)sel; return 0; }
const char *sel_registerName(const char *s) { return s; }
void *objc_retain(void *o) { if (o) ((hdr_t *)o - 1)->rc++; return o; }
void objc_release(void *o) {
    if (!o) return;
    hdr_t *h = (hdr_t *)o - 1;
    if (--h->rc > 0) return;
    mcls *c = *(mcls **)o;
    for (; c; c = c->super)
        for (int i = 0; i < c->nm; i++)
            if (!strcmp(c->m[i].sel, "dealloc")) { ((void (*)(void *, const char *))c->m[i].imp)(o, "dealloc"); return; }
    free(h);
}
/* NSColor do mOS: {isa, r, g, b, a} */
mcls color_cls = {"NSColor", 0, 40, {{0, 0}}, 0};
void *cx_color(double r, double g, double b, double a) { double *o = alloc_of(&color_cls); o[1] = r; o[2] = g; o[3] = b; o[4] = a; return o; }
const char *cx_cstr(void *s) { return s ? *(const char **)((char *)s + 16) : ""; }
char __CFConstantStringClassReference[64];
/* CFArray: {isa, n, itens[8]} */
static mcls arr_cls = {"CFArray", 0, 80, {{0, 0}}, 0};
static void *array(int n, void **v) { void **o = alloc_of(&arr_cls); o[1] = (void *)(intptr_t)n; for (int i = 0; i < n; i++) o[2 + i] = v[i]; return o; }
long CFArrayGetCount(void *a) { return a ? (long)(intptr_t)((void **)a)[1] : 0; }
void *CFArrayGetValueAtIndex(void *a, long i) { return ((void **)a)[2 + i]; }
void *CFDataCreate_(const uint8_t *b, long n);
void *CFDataCreate(void *al, const uint8_t *b, long n) { (void)al; return b ? CFDataCreate_(b, n) : 0; }
static const char *url_path;
uint8_t CFURLGetFileSystemRepresentation(void *url, uint8_t r, uint8_t *buf, long max) { (void)url; (void)r; snprintf((char *)buf, (size_t)max, "%s", url_path); return 1; }
uint32_t cx_cg_fill_argb(void) { return 0xFF000000u; }
void cx_cg_note_blend(int m) { (void)m; }
int cx_win_current(double *x, double *y) { (void)x; (void)y; return 0; }
void cx_win_clip(void) {}

/* o appkit.c desvia os CGContext* para o bitmap: aqui faço o mesmo */
int cx_bm_move(const void *, double, double);
int cx_bm_line(const void *, double, double);
int cx_bm_curve(const void *, double, double, double, double, double, double);
int cx_bm_close(const void *);
int cx_bm_begin(const void *);
int cx_bm_rect(const void *, double, double, double, double);
int cx_bm_fill(const void *, int);
int cx_bm_stroke(const void *);
int cx_bm_set_stroke(const void *, uint32_t);
int cx_bm_set_line_width(const void *, double);
int cx_bm_set_alpha(const void *, double);
int cx_bm_save(const void *);
int cx_bm_restore(const void *);
int cx_bm_fill_rect(const void *, double, double, double, double);
void cx_bm_set_fill(const void *, uint32_t);
void CGContextBeginPath(void *c) { cx_bm_begin(c); }
void CGContextMoveToPoint(void *c, double x, double y) { cx_bm_move(c, x, y); }
void CGContextAddLineToPoint(void *c, double x, double y) { cx_bm_line(c, x, y); }
void CGContextAddCurveToPoint(void *c, double a, double b, double d, double e, double x, double y) { cx_bm_curve(c, a, b, d, e, x, y); }
void CGContextStrokePath(void *c) { cx_bm_stroke(c); }


/* ---- o que cg_more.inc chama de outros arquivos do libSystem */
int cx_bm_ellipse(const void *, double, double, double, double);
int cx_bm_draw(const void *, int);
void CFRelease(void *o) { objc_release(o); }
void CGContextAddRect(void *c, CGRect r) { cx_bm_rect(c, r.origin.x, r.origin.y, r.size.width, r.size.height); }
void CGContextDrawPath(void *c, int m) { cx_bm_draw(c, m); }
void CGContextSetRGBFillColor(void *c, double r, double g, double b, double a) {
    cx_bm_set_fill(c, (uint32_t)(a * 255 + .5) << 24 | (uint32_t)(r * 255 + .5) << 16 | (uint32_t)(g * 255 + .5) << 8 | (uint32_t)(b * 255 + .5));
}
void CGContextSetRGBStrokeColor(void *c, double r, double g, double b, double a) {
    cx_bm_set_stroke(c, (uint32_t)(a * 255 + .5) << 24 | (uint32_t)(r * 255 + .5) << 16 | (uint32_t)(g * 255 + .5) << 8 | (uint32_t)(b * 255 + .5));
}
void CGPathApply(const void *path, void *info, void (*fn)(void *, const void *));
typedef struct { int type; CGPoint *pts; } elem_t;
static void addpath_el(void *c, const void *e0) {
    const elem_t *e = e0;
    if (e->type == 0) cx_bm_move(c, e->pts[0].x, e->pts[0].y);
    else if (e->type == 1) cx_bm_line(c, e->pts[0].x, e->pts[0].y);
    else if (e->type == 3) cx_bm_curve(c, e->pts[0].x, e->pts[0].y, e->pts[1].x, e->pts[1].y, e->pts[2].x, e->pts[2].y);
    else if (e->type == 4) cx_bm_close(c);
}
void CGContextAddPath(void *c, const void *p) { CGPathApply(p, c, addpath_el); }
void *cx_str(const char *s) { (void)s; return (void *)1; }
int mos_img_decode(const uint8_t *d, size_t n, void *o) { (void)d; (void)n; (void)o; return -1; }
const uint8_t *cx_colorsync_profile_bytes(const void *p, size_t *n) { (void)p; *n = 0; return 0; }
/* CFData mínimo para o ICC: {isa, bytes, len} */
static mcls data_cls = {"CFData", 0, 24, {{0, 0}}, 0};
void *CFDataCreate_(const uint8_t *b, long n) { void **o = alloc_of(&data_cls); uint8_t *c = malloc((size_t)n); memcpy(c, b, (size_t)n); o[1] = c; o[2] = (void *)(intptr_t)n; return o; }
const uint8_t *CFDataGetBytePtr(void *d) { return ((void **)d)[1]; }
long CFDataGetLength(void *d) { return (long)(intptr_t)((void **)d)[2]; }
int cx_is_str(void *o) { return o && *(void **)o == (void *)__CFConstantStringClassReference; }
/* CTFont falsa: cada glifo é um bloco de 4x4 px na posição, avanço 5 */
static mcls font_cls = {"FakeCTFont", 0, 16, {{0, 0}}, 0};
static int drawn_glyphs;
void *CTFontCreateWithGraphicsFont(void *f, double s, const void *m, void *a) { (void)f; (void)s; (void)m; (void)a; return alloc_of(&font_cls); }
void *CTFontCreateWithName(void *n, double s, const void *m) { (void)n; (void)s; (void)m; return alloc_of(&font_cls); }
int cx_bitmap_draw_glyph(void *ctx, double x, double y, const uint8_t *alpha, int gw, int gh, int left, int top);
void CTFontDrawGlyphs(void *f, const uint16_t *g, const CGPoint *p, size_t n, void *ctx) {
    static const uint8_t box[16] = {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};
    (void)f; (void)g;
    for (size_t i = 0; i < n; i++) { cx_bitmap_draw_glyph(ctx, p[i].x, p[i].y, box, 4, 4, 0, -4); drawn_glyphs++; }
}
double CTFontGetAdvancesForGlyphs(void *f, int o, const uint16_t *g, CGSize *adv, long n) { (void)f; (void)o; (void)g; for (long i = 0; adv && i < n; i++) { adv[i].width = 5; adv[i].height = 0; } return 5.0 * n; }
uint8_t CTFontGetGlyphsForCharacters(void *f, const uint16_t *c, uint16_t *g, long n) { (void)f; for (long i = 0; i < n; i++) g[i] = c[i]; return 1; }
void *CTFontCreatePathForGlyph(void *f, uint16_t g, const CGAffineTransform *m) {
    (void)f; (void)g;
    void *CGPathCreateMutable(void);
    void CGPathAddRect(void *, const CGAffineTransform *, CGRect);
    void *p = CGPathCreateMutable();
    CGRect r = {{0, 0}, {4, 4}};
    CGPathAddRect(p, m, r);
    return p;
}

/* ---------------------------------------------------------------- API sob teste */
void *CGBitmapContextCreate(void *data, size_t w, size_t h, size_t bpc, size_t bpr, void *space, uint32_t info);
void CGContextRelease(void *c);
void *CGBitmapContextGetData(void *c);
void CGContextClip(void *c);
void CGContextEOClip(void *c);
void CGContextResetClip(void *c);
CGRect CGContextGetClipBoundingBox(void *c);
void CGContextAddArc(void *c, double x, double y, double r, double a0, double a1, int cw);
void CGContextAddQuadCurveToPoint(void *c, double cx, double cy, double x, double y);
void CGContextSetLineDash(void *c, double phase, const double *l, size_t n);
void CGContextStrokeLineSegments(void *c, const CGPoint *p, size_t n);
void *CGGradientCreateWithColors(const void *space, void *colors, const double *loc);
void CGGradientRelease(void *g);
void CGContextDrawLinearGradient(void *c, void *g, CGPoint s, CGPoint e, uint32_t opts);
void CGContextDrawRadialGradient(void *c, void *g, CGPoint c0, double r0, CGPoint c1, double r1, uint32_t opts);
void CGContextBeginTransparencyLayer(void *c, void *aux);
void CGContextEndTransparencyLayer(void *c);
void *CGLayerCreateWithContext(void *c, CGSize s, void *aux);
void *CGLayerGetContext(void *l);
void CGLayerRelease(void *l);
void CGContextDrawLayerAtPoint(void *c, CGPoint p, void *l);
typedef struct { uint32_t version; void (*draw)(void *, void *); void (*release)(void *); } pat_cb;
void *CGPatternCreate(void *info, CGRect b, CGAffineTransform m, double xs, double ys, int tiling, int colored, const pat_cb *cb);
void CGPatternRelease(void *p);
void *CGColorSpaceCreatePattern(void *base);
void *CGColorSpaceGetBaseColorSpace(const void *s);
void CGContextSetFillColorSpace(void *c, void *s);
void CGContextSetFillPattern(void *c, void *p, const double *comps);
void *CGContextCopyPath(void *c);
size_t CGPathGetNumberOfElements(const void *p);
void CGContextReplacePathWithStrokedPath(void *c);
void CGContextTranslateCTM(void *c, double x, double y);
void CGContextScaleCTM(void *c, double x, double y);
void *CGPDFDocumentCreateWithURL(void *url);
size_t CGPDFDocumentGetNumberOfPages(void *d);
int CGPDFDocumentIsEncrypted(void *d);
int CGPDFDocumentIsUnlocked(void *d);
void *CGPDFDocumentGetPage(void *d, size_t n);
CGRect CGPDFPageGetBoxRect(void *p, int box);
CGAffineTransform CGPDFPageGetDrawingTransform(void *p, int box, CGRect r, int rot, int aspect);
void CGPDFDocumentRelease(void *d);
int CGSizeEqualToSize(CGSize a, CGSize b);
int CGPointEqualToPoint(CGPoint a, CGPoint b);
extern const void *kCGColorSpaceDCIP3;

static int fails;
void ramp(void *info, const double *in, double *out) { (void)info; out[0] = in[0]; out[1] = 0; out[2] = 0; out[3] = 1; }
#define CHECK(c, m) do { if (!(c)) { fprintf(stderr, "FAIL %s\n", m); fails++; } else printf("ok %s\n", m); } while (0)

/* bitmap 64x64 premultipliedLast (RGBA): pixel (x, y) do dispositivo, y para baixo */
static uint8_t *px(void *c, int x, int y) { return (uint8_t *)CGBitmapContextGetData(c) + (size_t)y * 64 * 4 + (size_t)x * 4; }
static double coverage(void *c) { double s = 0; for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++) s += px(c, x, y)[3] / 255.0; return s; }
static void *ctx(void) { return CGBitmapContextCreate(0, 64, 64, 8, 256, 0, 1); }
static void red(void *c) { cx_bm_set_fill(c, 0xFFFF0000u); }

static void draw_checker(void *info, void *c) {
    (void)info;
    cx_bm_set_fill(c, 0xFF0000FFu);
    cx_bm_fill_rect(c, 0, 0, 4, 4);             /* metade do ladrilho de 8x4 */
}

int main(void) {
    /* path preenchido: quadrado no espaço do usuário (y para cima) */
    void *c = ctx();
    red(c);
    cx_bm_begin(c);
    cx_bm_rect(c, 10, 10, 20, 20);
    cx_bm_fill(c, 0);
    CHECK(fabs(coverage(c) - 400) < 0.5, "FillPath: 20x20 = 400 px");
    CHECK(px(c, 15, 64 - 15)[0] == 255 && px(c, 15, 64 - 15)[3] == 255, "FillPath: vermelho opaco dentro");
    CHECK(px(c, 5, 64 - 5)[3] == 0, "FillPath: nada fora");
    /* alfa global: pré-multiplicado */
    cx_bm_set_alpha(c, 0.5);
    cx_bm_fill_rect(c, 40, 40, 4, 4);
    CHECK(px(c, 41, 64 - 41)[3] == 128 && px(c, 41, 64 - 41)[0] == 128, "fill_rect com alfa 0,5 sai pré-multiplicado (128,128)");
    CGContextRelease(c);

    /* traço com tracejado e segmentos soltos */
    c = ctx();
    cx_bm_set_stroke(c, 0xFF00FF00u);
    cx_bm_set_line_width(c, 2);
    double dash[2] = {4, 4};
    CGContextSetLineDash(c, 0, dash, 2);
    CGPoint seg[2] = {{0, 32}, {64, 32}};
    CGContextStrokeLineSegments(c, seg, 2);
    CHECK(fabs(coverage(c) - 64) < 1, "StrokeLineSegments tracejado 4/4: metade de 64x2");
    CHECK(px(c, 1, 31)[1] == 255 && px(c, 5, 31)[3] == 0, "tracejado: traço e espaço no lugar");
    CGContextRelease(c);

    /* recorte por path circular: só o círculo recebe a tinta */
    c = ctx();
    red(c);
    cx_bm_begin(c);
    CGContextAddArc(c, 32, 32, 16, 0, 2 * M_PI, 0);
    CGContextClip(c);
    CGRect bb = CGContextGetClipBoundingBox(c);
    CHECK(bb.origin.x >= 15.5 && bb.origin.x <= 16.5 && bb.size.width >= 31 && bb.size.width <= 33, "GetClipBoundingBox: caixa do círculo");
    cx_bm_fill_rect(c, 0, 0, 64, 64);
    double a = coverage(c);
    printf("  círculo r=16 recortado: %.1f (π r² = %.1f)\n", a, M_PI * 256);
    CHECK(fabs(a - M_PI * 256) / (M_PI * 256) < 0.01, "Clip por path: área do círculo");
    CGContextResetClip(c);
    cx_bm_fill_rect(c, 0, 0, 64, 64);
    CHECK(fabs(coverage(c) - 4096) < 0.5, "ResetClip: o bitmap inteiro volta");
    CGContextRelease(c);

    /* gradiente linear vermelho -> azul na horizontal */
    c = ctx();
    void *cols[2] = {cx_color(1, 0, 0, 1), cx_color(0, 0, 1, 1)};
    void *arr = array(2, cols);
    void *g = CGGradientCreateWithColors(0, arr, 0);
    CGPoint s = {0, 0}, e = {64, 0};
    CGContextDrawLinearGradient(c, g, s, e, 0);
    CHECK(px(c, 0, 10)[0] > 250 && px(c, 63, 10)[2] > 250 && abs(px(c, 32, 10)[0] - 125) < 6, "gradiente linear: vermelho, meio, azul");
    CGContextRelease(c);
    /* radial concêntrico: centro vermelho, borda azul, fora sem tinta */
    c = ctx();
    CGPoint ce = {32, 32};
    CGContextDrawRadialGradient(c, g, ce, 0, ce, 20, 0);
    CHECK(px(c, 32, 32)[0] > 245 && px(c, 2, 2)[3] == 0, "gradiente radial: centro vermelho, canto vazio");
    CGGradientRelease(g);
    CGContextRelease(c);

    /* camada de transparência: dois quadrados sobrepostos com alfa 0,5 compostos uma vez só */
    c = ctx();
    red(c);
    cx_bm_set_alpha(c, 0.5);
    CGContextBeginTransparencyLayer(c, 0);
    cx_bm_fill_rect(c, 10, 10, 20, 20);
    cx_bm_fill_rect(c, 20, 20, 20, 20);
    CGContextEndTransparencyLayer(c);
    CHECK(px(c, 25, 64 - 25)[3] == 128, "camada: a sobreposição não escurece (alfa 128)");
    CHECK(px(c, 12, 64 - 12)[3] == 128 && px(c, 5, 5)[3] == 0, "camada: resto com alfa 128, fora vazio");
    CGContextRelease(c);

    /* CGLayer desenhado num ponto */
    c = ctx();
    CGSize ls = {8, 8};
    void *l = CGLayerCreateWithContext(c, ls, 0);
    void *lc = CGLayerGetContext(l);
    red(lc);
    cx_bm_fill_rect(lc, 0, 0, 8, 8);
    CGPoint at = {40, 40};
    CGContextDrawLayerAtPoint(c, at, l);
    CHECK(px(c, 44, 64 - 44)[0] == 255 && px(c, 30, 64 - 30)[3] == 0, "CGLayer: bloco 8x8 em (40,40)");
    CGLayerRelease(l);
    CGContextRelease(c);

    /* padrão: ladrilho 8x4 com metade azul */
    c = ctx();
    void *ps = CGColorSpaceCreatePattern(0);
    CHECK(ps && CGColorSpaceGetBaseColorSpace(ps) == 0, "CGColorSpaceCreatePattern sem base");
    CGContextSetFillColorSpace(c, ps);
    pat_cb cb = {0, draw_checker, 0};
    CGRect pb = {{0, 0}, {8, 4}};
    CGAffineTransform ident = {1, 0, 0, 1, 0, 0};
    void *pat = CGPatternCreate(0, pb, ident, 8, 4, 0, 1, &cb);
    double one = 1;
    CGContextSetFillPattern(c, pat, &one);
    cx_bm_fill_rect(c, 0, 0, 64, 64);
    CHECK(fabs(coverage(c) - 2048) < 1, "padrão: metade do bitmap pintada");
    CGPatternRelease(pat);
    CGContextRelease(c);

    /* CopyPath e ReplacePathWithStrokedPath */
    c = ctx();
    cx_bm_begin(c);
    cx_bm_move(c, 10, 10);
    CGContextAddQuadCurveToPoint(c, 30, 50, 50, 10);
    void *path = CGContextCopyPath(c);
    CHECK(path && CGPathGetNumberOfElements(path) > 4, "CopyPath: curva vira segmentos no espaço do usuário");
    cx_bm_set_line_width(c, 4);
    CGContextReplacePathWithStrokedPath(c);
    red(c);
    cx_bm_fill(c, 0);
    CHECK(coverage(c) > 200, "ReplacePathWithStrokedPath + Fill pinta o contorno");
    CGContextRelease(c);

    /* PDF real de duas páginas, uma MediaBox A4 */
    {
        const char *pdf = "%PDF-1.4\n1 0 obj << /Type /Catalog /Pages 2 0 R >> endobj\n"
                          "2 0 obj << /Type /Pages /Kids [3 0 R 4 0 R] /Count 2 /MediaBox [0 0 595 842] >> endobj\n"
                          "3 0 obj << /Type /Page /Parent 2 0 R >> endobj\n4 0 obj << /Type/Page /Parent 2 0 R >> endobj\n"
                          "trailer << /Root 1 0 R >>\n%%EOF\n";
        url_path = "/tmp/mos-cg-test.pdf";
        FILE *f = fopen(url_path, "wb");
        fputs(pdf, f);
        fclose(f);
        void *d = CGPDFDocumentCreateWithURL((void *)1);
        CHECK(d && CGPDFDocumentGetNumberOfPages(d) == 2, "PDF: 2 páginas");
        CHECK(d && !CGPDFDocumentIsEncrypted(d) && CGPDFDocumentIsUnlocked(d), "PDF: sem /Encrypt, aberto");
        void *pg = d ? CGPDFDocumentGetPage(d, 1) : 0;
        CGRect mb = CGPDFPageGetBoxRect(pg, 0);
        CHECK(mb.size.width == 595 && mb.size.height == 842, "PDF: MediaBox A4");
        CGRect dst = {{0, 0}, {297.5, 421}};
        CGAffineTransform t = CGPDFPageGetDrawingTransform(pg, 0, dst, 0, 1);
        CHECK(fabs(t.a - 0.5) < 1e-9 && fabs(t.d - 0.5) < 1e-9 && fabs(t.tx) < 1e-6 && fabs(t.ty) < 1e-6, "PDF: transform encolhe à metade no lugar");
        CGPDFDocumentRelease(d);
        remove(url_path);
        url_path = "/tmp/nao-existe.pdf";
        CHECK(CGPDFDocumentCreateWithURL((void *)1) == 0, "PDF: arquivo ausente = NULL");
    }
    CGSize s1 = {1, 2}, s2 = {1, 2};
    CGPoint q1 = {3, 4}, q2 = {3, 5};
    CHECK(CGSizeEqualToSize(s1, s2) && !CGPointEqualToPoint(q1, q2), "CGSizeEqualToSize / CGPointEqualToPoint");
    CHECK(kCGColorSpaceDCIP3 && !strcmp(cx_cstr((void *)kCGColorSpaceDCIP3), "kCGColorSpaceDCIP3"), "kCGColorSpaceDCIP3");


    /* ===== CGContext completo (15 nov. 2026) ===== */
    {
        void CGContextSetBlendMode(void *, int);
        void CGContextFillRect(void *, CGRect);
        /* Multiply = 1 (antes o mOS tratava 1 como Copy) */
        void *b = ctx();
        cx_bm_set_fill(b, 0xFF808080u); cx_bm_fill_rect(b, 0, 0, 64, 64);
        CGContextSetBlendMode(b, 1);
        cx_bm_set_fill(b, 0xFFFF0000u); cx_bm_fill_rect(b, 0, 0, 8, 8);
        CHECK(px(b, 1, 62)[0] == 128 && px(b, 1, 62)[1] == 0, "blend Multiply (1): cinza × vermelho = (128,0,0)");
        CGContextSetBlendMode(b, 2);
        cx_bm_set_fill(b, 0xFF0000FFu); cx_bm_fill_rect(b, 10, 0, 8, 8);
        CHECK(px(b, 11, 62)[2] == 255 && px(b, 11, 62)[0] == 128, "blend Screen (2): (128,128,255)");
        CGContextSetBlendMode(b, 17);
        cx_bm_set_fill(b, 0x00000000u); cx_bm_fill_rect(b, 20, 0, 8, 8);
        CHECK(px(b, 21, 62)[3] == 0, "blend Copy (17) com cor transparente apaga");
        CGContextSetBlendMode(b, 23);
        cx_bm_set_fill(b, 0xFF000000u); cx_bm_fill_rect(b, 30, 0, 8, 8);
        CHECK(px(b, 31, 62)[3] == 0 && px(b, 41, 62)[3] == 255, "blend DestinationOut (23)");
        CGContextSetBlendMode(b, 10);
        cx_bm_set_fill(b, 0xFFFFFFFFu); cx_bm_fill_rect(b, 44, 0, 8, 8);
        CHECK(px(b, 45, 62)[0] == 127 || px(b, 45, 62)[0] == 128, "blend Difference (10): branco − cinza");
        CGContextRelease(b);

        /* ordem de bytes: PremultipliedFirst sem Little = A,R,G,B na memória; com 32Little = B,G,R,A */
        void *a1 = CGBitmapContextCreate(0, 4, 4, 8, 16, 0, 2);
        cx_bm_set_fill(a1, 0xFFFF0000u); cx_bm_fill_rect(a1, 0, 0, 4, 4);
        uint8_t *d1 = CGBitmapContextGetData(a1);
        CHECK(d1[0] == 255 && d1[1] == 255 && d1[2] == 0, "PremultipliedFirst (big): A R G B");
        void *a2 = CGBitmapContextCreate(0, 4, 4, 8, 16, 0, 2 | 0x2000);
        cx_bm_set_fill(a2, 0xFFFF0000u); cx_bm_fill_rect(a2, 0, 0, 4, 4);
        uint8_t *d2 = CGBitmapContextGetData(a2);
        CHECK(d2[0] == 0 && d2[2] == 255 && d2[3] == 255, "PremultipliedFirst|32Little: B G R A");
        CGContextRelease(a1); CGContextRelease(a2);

        /* cinza 8 bits sem alfa (antes: NULL) e 16 bits por componente */
        void *CGColorSpaceCreateDeviceGray(void);
        void *gs = CGColorSpaceCreateDeviceGray();
        void *g8 = CGBitmapContextCreate(0, 8, 8, 8, 8, gs, 0);
        CHECK(g8 != 0, "contexto cinza 8 bits sem alfa existe");
        cx_bm_set_fill(g8, 0xFFFFFFFFu); cx_bm_fill_rect(g8, 0, 0, 4, 8);
        CHECK(((uint8_t *)CGBitmapContextGetData(g8))[0] == 255 && ((uint8_t *)CGBitmapContextGetData(g8))[7] == 0, "cinza: branco à esquerda, preto à direita");
        void *CGBitmapContextGetColorSpace(void *);
        CHECK(CGBitmapContextGetColorSpace(g8) == gs, "CGBitmapContextGetColorSpace devolve o espaço do contexto");
        void *c16 = CGBitmapContextCreate(0, 4, 4, 16, 32, 0, 1 | 0x1000);
        CHECK(c16 != 0, "contexto RGBA 16 bpc");
        cx_bm_set_fill(c16, 0xFF00FF00u); cx_bm_fill_rect(c16, 0, 0, 4, 4);
        uint16_t *w16 = CGBitmapContextGetData(c16);
        CHECK(w16[1] == 0xFFFF && w16[0] == 0 && w16[3] == 0xFFFF, "RGBA 16 bpc little: G e A = 0xFFFF");
        void *cf = CGBitmapContextCreate(0, 4, 4, 32, 64, 0, 1 | 0x100 | 0x2000);
        CHECK(cf != 0, "contexto RGBA float");
        cx_bm_set_fill(cf, 0x80FF0000u); cx_bm_fill_rect(cf, 0, 0, 4, 4);
        float *fp = CGBitmapContextGetData(cf);
        CHECK(fabsf(fp[0] - 128 / 255.f) < 0.01f && fabsf(fp[3] - 128 / 255.f) < 0.01f, "float: vermelho a 50% pré-multiplicado");
        CGContextRelease(c16); CGContextRelease(cf);

        /* sombra: um quadrado com sombra deslocada (4, −4) aparece abaixo e à direita */
        void CGContextSetShadowWithColor(void *, CGSize, double, void *);
        void *s = ctx();
        void *black = cx_color(0, 0, 0, 1);
        CGSize off = {4, -4};
        CGContextSetShadowWithColor(s, off, 0, black);
        red(s);
        cx_bm_fill_rect(s, 10, 40, 10, 10);
        CHECK(px(s, 22, 64 - 38)[3] == 255 && px(s, 22, 64 - 38)[0] == 0, "sombra dura deslocada (4,−4) em preto");
        CHECK(px(s, 12, 64 - 48)[0] == 255, "o quadrado vermelho fica por cima da sombra");
        CGContextRelease(s);
        s = ctx();
        CGContextSetShadowWithColor(s, off, 6, black);
        red(s);
        cx_bm_fill_rect(s, 20, 20, 10, 10);
        int a_near = px(s, 31, 64 - 18)[3], a_far = px(s, 40, 64 - 10)[3];
        CHECK(a_near > a_far && a_near > 0 && a_near < 255, "sombra borrada decai com a distância");
        CGContextRelease(s);

        /* ClipToMask com imagem cinza: branco deixa pintar */
        void *CGDataProviderCreateWithData(void *, const void *, size_t, void *);
        void *CGImageCreate(size_t, size_t, size_t, size_t, size_t, const void *, uint32_t, void *, const void *, int, int);
        void CGContextClipToMask(void *, CGRect, void *);
        uint8_t mpx[4] = {255, 0, 0, 255};                  /* 2x2: branco preto / preto branco */
        void *mp = CGDataProviderCreateWithData(0, mpx, 4, 0);
        void *mimg = CGImageCreate(2, 2, 8, 8, 2, gs, 0, mp, 0, 0, 0);
        CHECK(mimg != 0, "CGImage cinza 8 bits");
        void *m = ctx();
        CGRect all = {{0, 0}, {64, 64}};
        CGContextClipToMask(m, all, mimg);
        red(m);
        cx_bm_fill_rect(m, 0, 0, 64, 64);
        CHECK(px(m, 10, 10)[3] == 255 && px(m, 50, 10)[3] == 0 && px(m, 50, 50)[3] == 255, "ClipToMask: quadrantes brancos pintados");
        CGContextRelease(m);

        /* CGImageMaskCreate: 1 bit, 0 pinta; desenha com a cor de preenchimento */
        void *CGImageMaskCreate(size_t, size_t, size_t, size_t, size_t, void *, const double *, int);
        void CGContextDrawImage(void *, CGRect, void *);
        uint8_t bits[2] = {0x0F, 0x0F};                     /* 8x2: 4 pinta, 4 mascara */
        void *bp = CGDataProviderCreateWithData(0, bits, 2, 0);
        void *msk = CGImageMaskCreate(8, 2, 1, 1, 1, bp, 0, 0);
        CHECK(msk != 0, "CGImageMaskCreate 1 bit");
        void *dm = ctx();
        cx_bm_set_fill(dm, 0xFF0000FFu);
        CGRect r8 = {{0, 0}, {64, 64}};
        void CGContextSetInterpolationQuality(void *, int);
        CGContextSetInterpolationQuality(dm, 1);
        CGContextDrawImage(dm, r8, msk);
        CHECK(px(dm, 4, 10)[2] == 255 && px(dm, 60, 10)[3] == 0, "image mask pinta a cor de preenchimento só onde é 0");
        CGContextRelease(dm);

        /* CGImageCreate com 16 bpc e com 1 bit cinza */
        uint16_t p16[4] = {0, 0xFFFF, 0, 0xFFFF};          /* um pixel RGBA16 big-endian? não: ordem 16 little */
        void *pp16 = CGDataProviderCreateWithData(0, p16, 8, 0);
        void *i16 = CGImageCreate(1, 1, 16, 64, 8, 0, 3 | 0x1000, pp16, 0, 0, 0);
        int CGImageGetBitsPerComponent_(void *);
        CHECK(i16 != 0, "CGImageCreate RGBA 16 bpc");
        uint8_t one[1] = {0xA0};
        void *p1 = CGDataProviderCreateWithData(0, one, 1, 0);
        void *i1 = CGImageCreate(4, 1, 1, 1, 1, gs, 0, p1, 0, 0, 0);
        CHECK(i1 != 0, "CGImageCreate cinza 1 bit");
        void *t1 = ctx();
        CGRect r4 = {{0, 0}, {4, 1}};
        CGContextDrawImage(t1, r4, i1);
        CHECK(px(t1, 0, 63)[0] == 255 && px(t1, 1, 63)[0] == 0 && px(t1, 2, 63)[0] == 255, "1 bit: 1010 → branco preto branco");
        CGContextRelease(t1);

        /* ClipToRects: união */
        void CGContextClipToRects(void *, const CGRect *, size_t);
        void *cr = ctx();
        CGRect rs[2] = {{{0, 0}, {10, 10}}, {{30, 30}, {10, 10}}};
        CGContextClipToRects(cr, rs, 2);
        red(cr);
        cx_bm_fill_rect(cr, 0, 0, 64, 64);
        CHECK(fabs(coverage(cr) - 200) < 1, "ClipToRects: 2 × 100 px");
        CGContextRelease(cr);

        /* consultas ao path */
        _Bool CGContextIsPathEmpty(void *);
        CGPoint CGContextGetPathCurrentPoint(void *);
        CGRect CGContextGetPathBoundingBox(void *);
        _Bool CGContextPathContainsPoint(void *, CGPoint, int);
        void CGContextAddLines(void *, const CGPoint *, size_t);
        void *q = ctx();
        CGContextScaleCTM(q, 2, 2);
        CHECK(CGContextIsPathEmpty(q), "IsPathEmpty no começo");
        CGPoint tri[3] = {{1, 1}, {11, 1}, {6, 9}};
        CGContextAddLines(q, tri, 3);
        CGPoint cp = CGContextGetPathCurrentPoint(q);
        if (fabs(cp.x - 6) > 1e-6 || fabs(cp.y - 9) > 1e-6) printf("  ponto atual %g %g\n", cp.x, cp.y);
        CHECK(!CGContextIsPathEmpty(q) && fabs(cp.x - 6) < 1e-6 && fabs(cp.y - 9) < 1e-6, "GetPathCurrentPoint no espaço do usuário");
        CGRect bb = CGContextGetPathBoundingBox(q);
        CHECK(fabs(bb.origin.x - 1) < 1e-6 && fabs(bb.size.width - 10) < 1e-6 && fabs(bb.size.height - 8) < 1e-6, "GetPathBoundingBox");
        CGPoint in = {6, 3}, outp = {1, 8};
        CHECK(CGContextPathContainsPoint(q, in, 0) && !CGContextPathContainsPoint(q, outp, 0), "PathContainsPoint");
        CGContextRelease(q);

        /* shading axial com CGFunction */
        typedef struct { uint32_t v; void (*eval)(void *, const double *, double *); void (*rel)(void *); } fcb;
        void ramp(void *info, const double *in, double *out);
        void *CGFunctionCreate(void *, size_t, const double *, size_t, const double *, const fcb *);
        void *CGShadingCreateAxial(void *, CGPoint, CGPoint, void *, int, int);
        void CGContextDrawShading(void *, void *);
        void *CGColorSpaceCreateDeviceRGB(void);
        double dom[2] = {0, 1}, rng[8] = {0, 1, 0, 1, 0, 1, 0, 1};
        fcb cb = {0, ramp, 0};
        void *fn = CGFunctionCreate(0, 1, dom, 4, rng, &cb);
        CGPoint s0 = {0, 0}, s1 = {64, 0};
        void *sh = CGShadingCreateAxial(CGColorSpaceCreateDeviceRGB(), s0, s1, fn, 1, 1);
        void *sc = ctx();
        CGContextDrawShading(sc, sh);
        CHECK(px(sc, 1, 32)[0] < 10 && px(sc, 62, 32)[0] > 245 && px(sc, 32, 32)[0] > 110 && px(sc, 32, 32)[0] < 145, "DrawShading axial: rampa de vermelho");
        CGContextRelease(sc);

        /* texto: ShowGlyphsAtPoint usa a fonte do contexto e anda a posição */
        void CGContextSetFont(void *, void *);
        void CGContextSetFontSize(void *, double);
        void CGContextShowGlyphsAtPoint(void *, double, double, const uint16_t *, size_t);
        CGPoint CGContextGetTextPosition(void *);
        void CGContextSetTextDrawingMode(void *, int);
        void *tc = ctx();
        CGContextSetFont(tc, alloc_of(&font_cls));
        CGContextSetFontSize(tc, 12);
        red(tc);
        uint16_t gl[3] = {1, 2, 3};
        CGContextShowGlyphsAtPoint(tc, 10, 10, gl, 3);
        CGPoint tp = CGContextGetTextPosition(tc);
        CHECK(drawn_glyphs == 3 && fabs(tp.x - 25) < 1e-6, "ShowGlyphsAtPoint: 3 glifos, posição andou 15");
        CHECK(fabs(coverage(tc) - 48) < 1, "glifos pintados (3 × 16 px)");
        CGContextSetTextDrawingMode(tc, 3);
        CGContextShowGlyphsAtPoint(tc, 40, 40, gl, 3);
        CHECK(fabs(coverage(tc) - 48) < 1, "modo invisível não pinta");
        CGContextRelease(tc);
    }

    /* ===== espaços de cor e ColorSync (16 nov. 2026) ===== */
    {
        void *CGColorCreateGenericGray(double, double);
        void *CGColorCreateGenericCMYK(double, double, double, double, double);
        size_t CGColorGetNumberOfComponents(const void *);
        const double *CGColorGetComponents(const void *);
        void *CGColorGetColorSpace(const void *);
        int CGColorSpaceGetModel(const void *);
        void *gg = CGColorCreateGenericGray(0.25, 0.5);
        CHECK(CGColorGetNumberOfComponents(gg) == 2 && CGColorGetComponents(gg)[0] == 0.25 && CGColorGetComponents(gg)[1] == 0.5, "cor cinza: 2 componentes (cinza, alfa)");
        CHECK(CGColorSpaceGetModel(CGColorGetColorSpace(gg)) == 0, "cor cinza no espaço cinza");
        void *ck = CGColorCreateGenericCMYK(1, 0, 0, 0, 1);
        double *rgba = (double *)((char *)ck + 8);
        CHECK(CGColorGetNumberOfComponents(ck) == 5 && rgba[0] < 0.05 && rgba[1] > 0.95 && rgba[2] > 0.95, "CMYK ciano: 5 componentes, sRGB (0,1,1)");
        /* P3: verde puro sai mais saturado que o sRGB consegue (recortado), branco fica branco */
        void *CGColorSpaceCreateWithName(const void *);
        extern const void *kCGColorSpaceDisplayP3, *kCGColorSpaceGenericCMYK;
        void *CGColorCreate(const void *, const double *);
        void *p3 = CGColorSpaceCreateWithName(kCGColorSpaceDisplayP3);
        double pg[4] = {0, 1, 0, 1}, pw[4] = {1, 1, 1, 1};
        double *g3 = (double *)((char *)CGColorCreate(p3, pg) + 8), *w3 = (double *)((char *)CGColorCreate(p3, pw) + 8);
        CHECK(g3[0] < 0.01 && g3[1] > 0.99 && g3[2] < 0.01 && w3[0] > 0.998 && w3[2] > 0.998, "cor no P3 convertida para sRGB");
        /* ICC: espaço com nome → bytes → espaço de novo, mesmo modelo */
        void *CGColorSpaceCopyICCData(void *);
        void *CGColorSpaceCreateWithICCData(void *);
        void *icc = CGColorSpaceCopyICCData(p3);
        void *p3b = icc ? CGColorSpaceCreateWithICCData(icc) : 0;
        CHECK(icc && CFDataGetLength(icc) > 300 && p3b && CGColorSpaceGetModel(p3b) == 1, "CopyICCData do P3 e CreateWithICCData de volta");
        double *g4 = p3b ? (double *)((char *)CGColorCreate(p3b, pg) + 8) : 0;
        CHECK(g4 && fabs(g4[0] - g3[0]) < 0.003 && fabs(g4[1] - g3[1]) < 0.003, "o P3 lido do ICC converte igual ao P3 com nome");
        /* imagem num espaço não sRGB: pixel P3 (0,255,0) desenha como verde do sRGB recortado */
        uint8_t p3px[4] = {0, 255, 0, 255};
        void *pp = CGDataProviderCreateWithData(0, p3px, 4, 0);
        void *pimg = CGImageCreate(1, 1, 8, 32, 4, p3, 5 /* SkipLast */, pp, 0, 0, 0);
        void *pc = ctx();
        CGRect one = {{0, 0}, {64, 64}};
        CGContextDrawImage(pc, one, pimg);
        CHECK(px(pc, 3, 3)[0] < 3 && px(pc, 3, 3)[1] == 255 && px(pc, 3, 3)[3] == 255, "imagem P3 entra no contexto convertida");
        CGContextRelease(pc);
        /* imagem CMYK 8 bits (DeviceCMYK sem perfil): ciano puro */
        void *cmyk = CGColorSpaceCreateWithName(kCGColorSpaceGenericCMYK);
        uint8_t cpx[4] = {255, 0, 0, 0};
        void *cp2 = CGDataProviderCreateWithData(0, cpx, 4, 0);
        void *cimg = CGImageCreate(1, 1, 8, 32, 4, cmyk, 0, cp2, 0, 0, 0);
        void *cc = ctx();
        CGContextDrawImage(cc, one, cimg);
        CHECK(cimg && px(cc, 3, 3)[0] < 8 && px(cc, 3, 3)[1] > 247 && px(cc, 3, 3)[2] > 247, "imagem CMYK: ciano vira (0,255,255)");
        CGContextRelease(cc);
        /* indexada: tabela de 2 cores, 1 bit por pixel */
        void *CGColorSpaceCreateIndexed(void *, size_t, const uint8_t *);
        void *CGColorSpaceCreateDeviceRGB(void);
        uint8_t tab[6] = {255, 0, 0, 0, 0, 255}, ib[1] = {0x40};       /* 0 vermelho, 1 azul: 0100 */
        void *ix = CGColorSpaceCreateIndexed(CGColorSpaceCreateDeviceRGB(), 1, tab);
        void *ip = CGDataProviderCreateWithData(0, ib, 1, 0);
        void *iimg = CGImageCreate(4, 1, 1, 1, 1, ix, 0, ip, 0, 0, 0);
        void *ic = ctx();
        CGRect r41 = {{0, 0}, {4, 1}};
        CGContextSetInterpolationQuality(ic, 1);
        CGContextDrawImage(ic, r41, iimg);
        CHECK(iimg && px(ic, 0, 63)[0] == 255 && px(ic, 1, 63)[2] == 255 && px(ic, 2, 63)[0] == 255, "imagem indexada de 1 bit pela tabela");
        CGContextRelease(ic);
        /* Lab: L=100 a=0 b=0 é branco */
        void *CGColorSpaceCreateLab(const double *, const double *, const double *);
        double wp[3] = {0.9642, 1, 0.8249}, rng[4] = {-128, 127, -128, 127}, lc[4] = {100, 0, 0, 1};
        void *lab = CGColorSpaceCreateLab(wp, 0, rng);
        double *lw = (double *)((char *)CGColorCreate(lab, lc) + 8);
        CHECK(lab && CGColorSpaceGetModel(lab) == 3 && lw[0] > 0.998 && lw[1] > 0.998 && lw[2] > 0.998, "cor Lab L=100 → branco");
        /* CopyByMatching: vermelho sRGB no cinza dá a luminância */
        void *CGColorCreateCopyByMatchingToColorSpace(void *, int, void *, void *);
        void *CGColorSpaceCreateDeviceGray(void);
        void *redc = cx_color(1, 0, 0, 1);
        void *mg = CGColorCreateCopyByMatchingToColorSpace(CGColorSpaceCreateDeviceGray(), 0, redc, 0);
        void *CGImageGetDataProvider(void *);
        CHECK(CGImageGetDataProvider(pimg) == pp, "CGImageGetDataProvider devolve o provedor original");
        CHECK(mg && CGColorGetNumberOfComponents(mg) == 2 && CGColorGetComponents(mg)[0] > 0.3 && CGColorGetComponents(mg)[0] < 0.6, "CopyByMatching sRGB → cinza");
    }
    if (fails) { fprintf(stderr, "cg_context: %d falhas\n", fails); return 1; }
    printf("cg_context: ok (0 falhas)\n");
    return 0;
}
