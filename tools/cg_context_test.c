/* CGBitmapContext de ponta a ponta: coregraphics.c (com coregraphics_bm.inc) + cg_raster.c de verdade,
 * sobre um runtime mínimo (classes, retain/release que chama o dealloc, CFArray). Confere pixels.
 *   zig cc -g -O1 -fno-sanitize=all -ffreestanding -fno-builtin -w -Iuserland/libsystem/kshim -Iuserland/libsystem/fsinc \
 *     -Ikernel/include -Iuserland/libsystem -c userland/libsystem/coregraphics.c -o /tmp/cg-lib.o
 *   zig cc -std=gnu11 -O1 -w -D_DEFAULT_SOURCE -Iuserland/libsystem tools/cg_context_test.c /tmp/cg-lib.o \
 *     userland/libsystem/cg_raster.c userland/libsystem/cgimage_crop.c -lm -o /tmp/cg_context_test && /tmp/cg_context_test
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
static mcls color_cls = {"NSColor", 0, 40, {{0, 0}}, 0};
void *cx_color(double r, double g, double b, double a) { double *o = alloc_of(&color_cls); o[1] = r; o[2] = g; o[3] = b; o[4] = a; return o; }
const char *cx_cstr(void *s) { return s ? *(const char **)((char *)s + 16) : ""; }
char __CFConstantStringClassReference[64];
/* CFArray: {isa, n, itens[8]} */
static mcls arr_cls = {"CFArray", 0, 80, {{0, 0}}, 0};
static void *array(int n, void **v) { void **o = alloc_of(&arr_cls); o[1] = (void *)(intptr_t)n; for (int i = 0; i < n; i++) o[2 + i] = v[i]; return o; }
long CFArrayGetCount(void *a) { return a ? (long)(intptr_t)((void **)a)[1] : 0; }
void *CFArrayGetValueAtIndex(void *a, long i) { return ((void **)a)[2 + i]; }
void *CFDataCreate(void *al, const uint8_t *b, long n) { (void)al; (void)b; (void)n; return 0; }
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

    if (fails) { fprintf(stderr, "cg_context: %d falhas\n", fails); return 1; }
    printf("cg_context: ok (0 falhas)\n");
    return 0;
}
