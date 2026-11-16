/*
 * mOS libSystem — CoreGraphics: geometria (CGRect*, CGAffineTransform*), CGPath, CGColorSpace e CGColor.
 * Cabeçalho público: userland/sdk/include/CoreGraphics/CoreGraphics.h (mesma ABI do macOS, CGFloat = double).
 * Usado por apps do macOS e pelo OpenRenderBox/OpenSwiftUI. CGColorCreateGenericRGB/Gray e as funções de
 * CGContext ficam em appkit.c; CGDisplay* em cgdisplay.c.
 *
 *  - CGPath é um objeto ObjC (classe "CGPath", CFRetain/CFRelease funcionam) com a lista de elementos;
 *    arcos e elipses viram curvas de Bézier cúbicas (≤ 90° cada), como no macOS.
 *  - CGColor é o NSColor do mOS ({isa, r, g, b, a}): componentes RGBA em double.
 *  - CGColorSpace: um objeto por nome, criado na primeira vez e mantido vivo.
 *  - CGPoint/CGSize/CGRect *DictionaryRepresentation: userland/libsystem/cgdict.c.
 */
#include <stdbool.h>
#include "foundation.h"
#include "libc.h"
#include "cg_blend.h"
#include "cg_raster.h"

double sqrt(double), sin(double), cos(double), atan2(double, double), fabs(double), floor(double), ceil(double);
id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
cx_class_t *objc_allocateClassPair(cx_class_t *super, const char *name, u64 extra);
void objc_registerClassPair(cx_class_t *c);
BOOL class_addIvar(cx_class_t *c, const char *name, u64 size, u8 align, const char *types);
BOOL class_addMethod(cx_class_t *c, SEL s, IMP imp, const char *types);
IMP class_getMethodImplementation(cx_class_t *c, SEL s);
id class_createInstance(cx_class_t *c, u64 extra);
id cx_color(double r, double g, double b, double a);
u8 CFEqual(id a, id b);

typedef double CGFloat;
typedef struct { CGFloat x, y; } CGPoint;
typedef struct { CGFloat width, height; } CGSize;
typedef struct { CGPoint origin; CGSize size; } CGRect;
typedef struct { CGFloat dx, dy; } CGVector;
typedef struct { CGFloat a, b, c, d, tx, ty; } CGAffineTransform;
typedef struct { CGSize scale; CGFloat horizontalShear, rotation; CGVector translation; } CGAffineTransformComponents;
typedef u32 CGRectEdge;

#define INF __builtin_inf()
#define PI 3.14159265358979323846

/* ======================================================================== geometria */
const CGPoint CGPointZero = {0, 0};
const CGSize CGSizeZero = {0, 0};
const CGRect CGRectZero = {{0, 0}, {0, 0}};
const CGRect CGRectNull = {{INF, INF}, {0, 0}};
const CGRect CGRectInfinite = {{-__DBL_MAX__ / 2, -__DBL_MAX__ / 2}, {__DBL_MAX__, __DBL_MAX__}};
const CGAffineTransform CGAffineTransformIdentity = {1, 0, 0, 1, 0, 0};

static double dmin(double a, double b) { return a < b ? a : b; }
static double dmax(double a, double b) { return a > b ? a : b; }

CGRect CGRectStandardize(CGRect r) {
    if (r.size.width < 0) { r.origin.x += r.size.width; r.size.width = -r.size.width; }
    if (r.size.height < 0) { r.origin.y += r.size.height; r.size.height = -r.size.height; }
    return r;
}
bool CGRectIsNull(CGRect r) { return r.origin.x == INF || r.origin.y == INF; }
bool CGRectIsInfinite(CGRect r) { return r.size.width >= __DBL_MAX__ && r.size.height >= __DBL_MAX__; }
bool CGRectIsEmpty(CGRect r) { return CGRectIsNull(r) || r.size.width == 0 || r.size.height == 0; }
CGFloat CGRectGetMinX(CGRect r) { r = CGRectStandardize(r); return r.origin.x; }
CGFloat CGRectGetMinY(CGRect r) { r = CGRectStandardize(r); return r.origin.y; }
CGFloat CGRectGetMaxX(CGRect r) { r = CGRectStandardize(r); return r.origin.x + r.size.width; }
CGFloat CGRectGetMaxY(CGRect r) { r = CGRectStandardize(r); return r.origin.y + r.size.height; }
CGFloat CGRectGetMidX(CGRect r) { r = CGRectStandardize(r); return r.origin.x + r.size.width / 2; }
CGFloat CGRectGetMidY(CGRect r) { r = CGRectStandardize(r); return r.origin.y + r.size.height / 2; }
CGFloat CGRectGetWidth(CGRect r) { return fabs(r.size.width); }
CGFloat CGRectGetHeight(CGRect r) { return fabs(r.size.height); }
bool CGRectEqualToRect(CGRect a, CGRect b) {
    if (CGRectIsNull(a) || CGRectIsNull(b)) return CGRectIsNull(a) && CGRectIsNull(b);
    a = CGRectStandardize(a); b = CGRectStandardize(b);
    return a.origin.x == b.origin.x && a.origin.y == b.origin.y && a.size.width == b.size.width && a.size.height == b.size.height;
}
CGRect CGRectOffset(CGRect r, CGFloat dx, CGFloat dy) {
    if (CGRectIsNull(r)) return r;
    r = CGRectStandardize(r); r.origin.x += dx; r.origin.y += dy; return r;
}
CGRect CGRectInset(CGRect r, CGFloat dx, CGFloat dy) {
    if (CGRectIsNull(r)) return r;
    r = CGRectStandardize(r);
    r.origin.x += dx; r.origin.y += dy; r.size.width -= 2 * dx; r.size.height -= 2 * dy;
    return (r.size.width < 0 || r.size.height < 0) ? CGRectNull : r;
}
CGRect CGRectIntegral(CGRect r) {
    if (CGRectIsNull(r) || CGRectIsInfinite(r)) return r;
    r = CGRectStandardize(r);
    double x0 = floor(r.origin.x), y0 = floor(r.origin.y);
    double x1 = ceil(r.origin.x + r.size.width), y1 = ceil(r.origin.y + r.size.height);
    CGRect o = {{x0, y0}, {x1 - x0, y1 - y0}};
    return o;
}
CGRect CGRectUnion(CGRect a, CGRect b) {
    if (CGRectIsNull(a)) return CGRectStandardize(b);
    if (CGRectIsNull(b)) return CGRectStandardize(a);
    a = CGRectStandardize(a); b = CGRectStandardize(b);
    double x0 = dmin(a.origin.x, b.origin.x), y0 = dmin(a.origin.y, b.origin.y);
    double x1 = dmax(a.origin.x + a.size.width, b.origin.x + b.size.width), y1 = dmax(a.origin.y + a.size.height, b.origin.y + b.size.height);
    CGRect o = {{x0, y0}, {x1 - x0, y1 - y0}};
    return o;
}
CGRect CGRectIntersection(CGRect a, CGRect b) {
    if (CGRectIsNull(a) || CGRectIsNull(b)) return CGRectNull;
    a = CGRectStandardize(a); b = CGRectStandardize(b);
    double x0 = dmax(a.origin.x, b.origin.x), y0 = dmax(a.origin.y, b.origin.y);
    double x1 = dmin(a.origin.x + a.size.width, b.origin.x + b.size.width), y1 = dmin(a.origin.y + a.size.height, b.origin.y + b.size.height);
    if (x1 < x0 || y1 < y0) return CGRectNull;
    CGRect o = {{x0, y0}, {x1 - x0, y1 - y0}};
    return o;
}
bool CGRectIntersectsRect(CGRect a, CGRect b) { CGRect i = CGRectIntersection(a, b); return !CGRectIsNull(i) && !(i.size.width == 0 && i.size.height == 0 && (a.size.width != 0 || b.size.width != 0)); }
bool CGRectContainsPoint(CGRect r, CGPoint p) {
    if (CGRectIsNull(r)) return false;
    r = CGRectStandardize(r);
    return p.x >= r.origin.x && p.x < r.origin.x + r.size.width && p.y >= r.origin.y && p.y < r.origin.y + r.size.height;
}
bool CGRectContainsRect(CGRect a, CGRect b) { return CGRectEqualToRect(CGRectUnion(a, b), a); }
void CGRectDivide(CGRect r, CGRect *slice, CGRect *rem, CGFloat amount, CGRectEdge edge) {
    r = CGRectStandardize(r);
    CGRect s = r, m = r;
    if (amount < 0) amount = 0;
    switch (edge) {
    case 0: if (amount > r.size.width) amount = r.size.width; s.size.width = amount; m.origin.x += amount; m.size.width -= amount; break;
    case 1: if (amount > r.size.height) amount = r.size.height; s.size.height = amount; m.origin.y += amount; m.size.height -= amount; break;
    case 2: if (amount > r.size.width) amount = r.size.width; s.origin.x = r.origin.x + r.size.width - amount; s.size.width = amount; m.size.width -= amount; break;
    default: if (amount > r.size.height) amount = r.size.height; s.origin.y = r.origin.y + r.size.height - amount; s.size.height = amount; m.size.height -= amount; break;
    }
    if (slice) *slice = s;
    if (rem) *rem = m;
}

/* ======================================================================== CGAffineTransform */
CGAffineTransform CGAffineTransformMake(CGFloat a, CGFloat b, CGFloat c, CGFloat d, CGFloat tx, CGFloat ty) {
    CGAffineTransform t = {a, b, c, d, tx, ty}; return t;
}
CGAffineTransform CGAffineTransformMakeTranslation(CGFloat tx, CGFloat ty) { return CGAffineTransformMake(1, 0, 0, 1, tx, ty); }
CGAffineTransform CGAffineTransformMakeScale(CGFloat sx, CGFloat sy) { return CGAffineTransformMake(sx, 0, 0, sy, 0, 0); }
CGAffineTransform CGAffineTransformMakeRotation(CGFloat a) { double s = sin(a), c = cos(a); return CGAffineTransformMake(c, s, -s, c, 0, 0); }
bool CGAffineTransformIsIdentity(CGAffineTransform t) { return t.a == 1 && t.b == 0 && t.c == 0 && t.d == 1 && t.tx == 0 && t.ty == 0; }
bool CGAffineTransformEqualToTransform(CGAffineTransform p, CGAffineTransform q) {
    return p.a == q.a && p.b == q.b && p.c == q.c && p.d == q.d && p.tx == q.tx && p.ty == q.ty;
}
/* t1 seguido de t2 */
CGAffineTransform CGAffineTransformConcat(CGAffineTransform t1, CGAffineTransform t2) {
    return CGAffineTransformMake(t1.a * t2.a + t1.b * t2.c, t1.a * t2.b + t1.b * t2.d,
                                 t1.c * t2.a + t1.d * t2.c, t1.c * t2.b + t1.d * t2.d,
                                 t1.tx * t2.a + t1.ty * t2.c + t2.tx, t1.tx * t2.b + t1.ty * t2.d + t2.ty);
}
CGAffineTransform CGAffineTransformTranslate(CGAffineTransform t, CGFloat tx, CGFloat ty) { return CGAffineTransformConcat(CGAffineTransformMakeTranslation(tx, ty), t); }
CGAffineTransform CGAffineTransformScale(CGAffineTransform t, CGFloat sx, CGFloat sy) { return CGAffineTransformConcat(CGAffineTransformMakeScale(sx, sy), t); }
CGAffineTransform CGAffineTransformRotate(CGAffineTransform t, CGFloat a) { return CGAffineTransformConcat(CGAffineTransformMakeRotation(a), t); }
CGAffineTransform CGAffineTransformInvert(CGAffineTransform t) {
    double det = t.a * t.d - t.b * t.c;
    if (det == 0) return t;                               /* não invertível: o macOS devolve o próprio t */
    return CGAffineTransformMake(t.d / det, -t.b / det, -t.c / det, t.a / det,
                                 (t.c * t.ty - t.d * t.tx) / det, (t.b * t.tx - t.a * t.ty) / det);
}
CGPoint CGPointApplyAffineTransform(CGPoint p, CGAffineTransform t) {
    CGPoint o = {t.a * p.x + t.c * p.y + t.tx, t.b * p.x + t.d * p.y + t.ty}; return o;
}
CGSize CGSizeApplyAffineTransform(CGSize s, CGAffineTransform t) {
    CGSize o = {t.a * s.width + t.c * s.height, t.b * s.width + t.d * s.height}; return o;
}
CGRect CGRectApplyAffineTransform(CGRect r, CGAffineTransform t) {
    if (CGRectIsNull(r)) return r;
    r = CGRectStandardize(r);
    CGPoint p[4] = {{r.origin.x, r.origin.y}, {r.origin.x + r.size.width, r.origin.y},
                    {r.origin.x, r.origin.y + r.size.height}, {r.origin.x + r.size.width, r.origin.y + r.size.height}};
    double x0 = INF, y0 = INF, x1 = -INF, y1 = -INF;
    for (int i = 0; i < 4; i++) {
        CGPoint q = CGPointApplyAffineTransform(p[i], t);
        x0 = dmin(x0, q.x); y0 = dmin(y0, q.y); x1 = dmax(x1, q.x); y1 = dmax(y1, q.y);
    }
    CGRect o = {{x0, y0}, {x1 - x0, y1 - y0}};
    return o;
}
/* t = escala * cisalhamento * rotação * translação (a convenção do macOS 13+) */
CGAffineTransformComponents CGAffineTransformDecompose(CGAffineTransform t) {
    CGAffineTransformComponents c;
    double sx = sqrt(t.a * t.a + t.b * t.b);
    double rot = atan2(t.b, t.a);
    double det = t.a * t.d - t.b * t.c;
    double sy = sx != 0 ? det / sx : 0;
    double sh = (sx != 0 && sy != 0) ? (t.a * t.c + t.b * t.d) / (sx * sy) : 0;
    c.scale.width = sx; c.scale.height = sy; c.horizontalShear = sh; c.rotation = rot;
    c.translation.dx = t.tx; c.translation.dy = t.ty;
    return c;
}
CGAffineTransform CGAffineTransformMakeWithComponents(CGAffineTransformComponents c) {
    CGAffineTransform s = CGAffineTransformMake(c.scale.width, 0, 0, c.scale.height, 0, 0);
    CGAffineTransform h = CGAffineTransformMake(1, 0, c.horizontalShear, 1, 0, 0);
    /* escala, depois cisalhamento, depois rotação; a translação entra por último */
    CGAffineTransform t = CGAffineTransformConcat(CGAffineTransformConcat(s, h), CGAffineTransformMakeRotation(c.rotation));
    t.tx = c.translation.dx; t.ty = c.translation.dy;
    return t;
}

/* ======================================================================== CGPath */
enum { MOVE, LINE, QUAD, CURVE, CLOSE };
typedef struct { i32 type; CGPoint *points; } CGPathElement;
typedef void (*CGPathApplierFunction)(void *info, const CGPathElement *el);
/* prim: forma primitiva de onde o caminho nasceu (CGPathCreateWith{Rect,EllipseInRect,RoundedRect}), com o retângulo,
 * os raios e a transformação; qualquer elemento novo apaga a marca. Serve às SPI CGPathIs*WithTransform (OpenRenderBox). */
enum { PRIM_NONE, PRIM_RECT, PRIM_ELLIPSE, PRIM_RRECT };
typedef struct { cx_class_t *isa; u64 cfinfo; u8 *types; CGPoint *pts; u64 n, cap, npts, ptcap; CGPoint start, cur;
                 int prim; CGRect prect; CGFloat pcw, pch; CGAffineTransform pm; } cgpath_t;

static const int pts_for[5] = {1, 1, 2, 3, 0};
static cx_class_t *path_cls;
static IMP nsobject_dealloc;

static void path_dealloc(id self, SEL c) {
    cgpath_t *p = (cgpath_t *)self;
    free(p->types); free(p->pts);
    p->types = 0; p->pts = 0;
    if (nsobject_dealloc) ((void (*)(id, SEL))nsobject_dealloc)(self, c);
}
static u64 path_hash(id self, SEL c) { (void)c; cgpath_t *p = (cgpath_t *)self; return p->n * 31 + p->npts; }
bool CGPathEqualToPath(const void *a, const void *b);
static BOOL path_isequal(id self, SEL c, id o) { (void)c; return CGPathEqualToPath(self, o) ? YES : NO; }
static id path_copy(id self, SEL c);

static cx_class_t *cgpath_class(void) {
    if (path_cls) return path_cls;
    cx_class_t *ns = cx_class_named("NSObject");
    nsobject_dealloc = ns ? class_getMethodImplementation(ns, sel_registerName("dealloc")) : 0;
    cx_class_t *k = objc_allocateClassPair(ns, "CGPath", 0);
    if (!k) k = cx_class_named("CGPath");
    class_addIvar(k, "_cg", sizeof(cgpath_t) - 8, 3, "?");
    class_addMethod(k, sel_registerName("dealloc"), (IMP)path_dealloc, "v@:");
    class_addMethod(k, sel_registerName("hash"), (IMP)path_hash, "Q@:");
    class_addMethod(k, sel_registerName("isEqual:"), (IMP)path_isequal, "c@:@");
    class_addMethod(k, sel_registerName("copy"), (IMP)path_copy, "@@:");
    class_addMethod(k, sel_registerName("copyWithZone:"), (IMP)path_copy, "@@:^v");
    objc_registerClassPair(k);
    path_cls = k;
    return k;
}
u64 CGPathGetTypeID(void) { return 0x2A0; }
void *CGPathCreateMutable(void) {
    cgpath_t *p = (cgpath_t *)class_createInstance(cgpath_class(), 0);
    if (!p) return 0;
    memset((u8 *)p + 8, 0, sizeof *p - 8);
    return p;
}
void *CGPathRetain(void *p) { return p ? (void *)objc_retain((id)p) : 0; }
void CGPathRelease(void *p) { if (p) objc_release((id)p); }

static CGPoint xf(const CGAffineTransform *m, double x, double y) {
    CGPoint p = {x, y};
    return m ? CGPointApplyAffineTransform(p, *m) : p;
}
static void push(cgpath_t *p, int type, const CGPoint *pts) {
    if (!p) return;
    p->prim = PRIM_NONE;
    int k = pts_for[type];
    if (p->n == p->cap) { p->cap = p->cap ? p->cap * 2 : 16; p->types = realloc(p->types, p->cap); }
    if (p->npts + k > p->ptcap) { while (p->npts + k > p->ptcap) p->ptcap = p->ptcap ? p->ptcap * 2 : 32; p->pts = realloc(p->pts, p->ptcap * sizeof(CGPoint)); }
    p->types[p->n++] = (u8)type;
    for (int i = 0; i < k; i++) p->pts[p->npts++] = pts[i];
    if (type == MOVE) p->start = pts[0];
    if (type == CLOSE) p->cur = p->start; else if (k) p->cur = pts[k - 1];
}
/* um segmento sem moveTo antes começa no ponto atual (como o macOS faz para lineTo em caminho vazio) */
static void need_start(cgpath_t *p, CGPoint at) { if (p && p->n == 0) push(p, MOVE, &at); }

void CGPathMoveToPoint(void *path, const CGAffineTransform *m, CGFloat x, CGFloat y) { CGPoint q = xf(m, x, y); push(path, MOVE, &q); }
void CGPathAddLineToPoint(void *path, const CGAffineTransform *m, CGFloat x, CGFloat y) {
    CGPoint q = xf(m, x, y); need_start(path, q); push(path, LINE, &q);
}
void CGPathAddQuadCurveToPoint(void *path, const CGAffineTransform *m, CGFloat cx, CGFloat cy, CGFloat x, CGFloat y) {
    CGPoint q[2] = {xf(m, cx, cy), xf(m, x, y)}; need_start(path, q[0]); push(path, QUAD, q);
}
void CGPathAddCurveToPoint(void *path, const CGAffineTransform *m, CGFloat c1x, CGFloat c1y, CGFloat c2x, CGFloat c2y, CGFloat x, CGFloat y) {
    CGPoint q[3] = {xf(m, c1x, c1y), xf(m, c2x, c2y), xf(m, x, y)}; need_start(path, q[0]); push(path, CURVE, q);
}
void CGPathCloseSubpath(void *path) {
    cgpath_t *p = path;
    if (p && p->n && p->types[p->n - 1] != CLOSE) push(p, CLOSE, 0);
}
void CGPathAddRect(void *path, const CGAffineTransform *m, CGRect r) {
    r = CGRectStandardize(r);
    double x0 = r.origin.x, y0 = r.origin.y, x1 = x0 + r.size.width, y1 = y0 + r.size.height;
    CGPathMoveToPoint(path, m, x0, y0); CGPathAddLineToPoint(path, m, x1, y0);
    CGPathAddLineToPoint(path, m, x1, y1); CGPathAddLineToPoint(path, m, x0, y1); CGPathCloseSubpath(path);
}
void CGPathAddRects(void *path, const CGAffineTransform *m, const CGRect *rects, size_t n) { for (size_t i = 0; rects && i < n; i++) CGPathAddRect(path, m, rects[i]); }
void CGPathAddLines(void *path, const CGAffineTransform *m, const CGPoint *pts, size_t n) {
    for (size_t i = 0; pts && i < n; i++) { if (i == 0) CGPathMoveToPoint(path, m, pts[i].x, pts[i].y); else CGPathAddLineToPoint(path, m, pts[i].x, pts[i].y); }
}
/* arco de a0 até a1 (radianos, a1 - a0 com sinal) em cúbicas de no máximo 90° */
static void arc_segments(void *path, const CGAffineTransform *m, double cx, double cy, double rx, double ry, double a0, double sweep, int move_first) {
    int n = (int)ceil(fabs(sweep) / (PI / 2) - 1e-9);
    if (n < 1) n = 1;
    double step = sweep / n, k = 4.0 / 3.0 * sin(step / 4) / cos(step / 4);   /* 4/3·tan(θ/4) */
    double x = cx + rx * cos(a0), y = cy + ry * sin(a0);
    cgpath_t *p = path;
    if (move_first || !p || p->n == 0) CGPathMoveToPoint(path, m, x, y); else CGPathAddLineToPoint(path, m, x, y);
    double a = a0;
    for (int i = 0; i < n; i++) {
        double b = a + step;
        double c1x = cx + rx * (cos(a) - k * sin(a)), c1y = cy + ry * (sin(a) + k * cos(a));
        double c2x = cx + rx * (cos(b) + k * sin(b)), c2y = cy + ry * (sin(b) - k * cos(b));
        CGPathAddCurveToPoint(path, m, c1x, c1y, c2x, c2y, cx + rx * cos(b), cy + ry * sin(b));
        a = b;
    }
}
void CGPathAddArc(void *path, const CGAffineTransform *m, CGFloat x, CGFloat y, CGFloat r, CGFloat a0, CGFloat a1, bool clockwise) {
    /* CG usa coordenadas com y para cima: "clockwise" = ângulo decrescente */
    double sweep = a1 - a0;
    if (clockwise) { while (sweep > 0) sweep -= 2 * PI; if (sweep < -2 * PI) sweep = -2 * PI; }
    else { while (sweep < 0) sweep += 2 * PI; if (sweep > 2 * PI) sweep = 2 * PI; }
    if (r <= 0) { CGPoint c = xf(m, x, y); cgpath_t *p = path; if (p && p->n) push(p, LINE, &c); else push(p, MOVE, &c); return; }
    arc_segments(path, m, x, y, r, r, a0, sweep, 0);
}
void CGPathAddRelativeArc(void *path, const CGAffineTransform *m, CGFloat x, CGFloat y, CGFloat r, CGFloat a0, CGFloat delta) {
    arc_segments(path, m, x, y, r, r, a0, delta, 0);
}
void CGPathAddArcToPoint(void *path, const CGAffineTransform *m, CGFloat x1, CGFloat y1, CGFloat x2, CGFloat y2, CGFloat r) {
    cgpath_t *p = path;
    if (!p) return;
    /* o ponto atual está no espaço já transformado; trabalhamos no espaço do usuário invertendo m */
    CGPoint p0 = p->cur;
    if (m) { CGAffineTransform inv = CGAffineTransformInvert(*m); p0 = CGPointApplyAffineTransform(p0, inv); }
    if (p->n == 0) { CGPathMoveToPoint(path, m, x1, y1); return; }
    double v1x = p0.x - x1, v1y = p0.y - y1, v2x = x2 - x1, v2y = y2 - y1;
    double l1 = sqrt(v1x * v1x + v1y * v1y), l2 = sqrt(v2x * v2x + v2y * v2y);
    double cr = v1x * v2y - v1y * v2x;
    if (l1 == 0 || l2 == 0 || r == 0 || fabs(cr) < 1e-12) { CGPathAddLineToPoint(path, m, x1, y1); return; }
    v1x /= l1; v1y /= l1; v2x /= l2; v2y /= l2;
    double cosang = v1x * v2x + v1y * v2y, ang = atan2(sqrt(1 - cosang * cosang), cosang);
    double d = r / (sin(ang / 2) / cos(ang / 2));                 /* distância do canto aos pontos de tangência */
    double t1x = x1 + v1x * d, t1y = y1 + v1y * d, t2x = x1 + v2x * d, t2y = y1 + v2y * d;
    double bx = v1x + v2x, by = v1y + v2y, bl = sqrt(bx * bx + by * by), h = r / sin(ang / 2);
    double ccx = x1 + bx / bl * h, ccy = y1 + by / bl * h;
    double a0 = atan2(t1y - ccy, t1x - ccx), a1 = atan2(t2y - ccy, t2x - ccx);
    double sweep = a1 - a0;
    while (sweep > PI) sweep -= 2 * PI;
    while (sweep < -PI) sweep += 2 * PI;
    CGPathAddLineToPoint(path, m, t1x, t1y);
    arc_segments(path, m, ccx, ccy, r, r, a0, sweep, 0);
}
void CGPathAddEllipseInRect(void *path, const CGAffineTransform *m, CGRect r) {
    r = CGRectStandardize(r);
    double rx = r.size.width / 2, ry = r.size.height / 2;
    arc_segments(path, m, r.origin.x + rx, r.origin.y + ry, rx, ry, 0, 2 * PI, 1);
    CGPathCloseSubpath(path);
}
void CGPathAddRoundedRect(void *path, const CGAffineTransform *m, CGRect r, CGFloat cw, CGFloat ch) {
    r = CGRectStandardize(r);
    if (cw > r.size.width / 2) cw = r.size.width / 2;
    if (ch > r.size.height / 2) ch = r.size.height / 2;
    if (cw <= 0 || ch <= 0) { CGPathAddRect(path, m, r); return; }
    double x0 = r.origin.x, y0 = r.origin.y, x1 = x0 + r.size.width, y1 = y0 + r.size.height;
    CGPathMoveToPoint(path, m, x1 - cw, y0);
    arc_segments(path, m, x1 - cw, y0 + ch, cw, ch, -PI / 2, PI / 2, 0);
    arc_segments(path, m, x1 - cw, y1 - ch, cw, ch, 0, PI / 2, 0);
    arc_segments(path, m, x0 + cw, y1 - ch, cw, ch, PI / 2, PI / 2, 0);
    arc_segments(path, m, x0 + cw, y0 + ch, cw, ch, PI, PI / 2, 0);
    CGPathCloseSubpath(path);
}
static void *prim_mark(cgpath_t *p, int kind, CGRect r, CGFloat cw, CGFloat ch, const CGAffineTransform *m) {
    if (!p) return p;
    p->prim = kind; p->prect = CGRectStandardize(r); p->pcw = cw; p->pch = ch;
    p->pm = m ? *m : CGAffineTransformIdentity;
    return p;
}
void *CGPathCreateWithRect(CGRect r, const CGAffineTransform *m) { void *p = CGPathCreateMutable(); CGPathAddRect(p, m, r); return prim_mark(p, PRIM_RECT, r, 0, 0, m); }
void *CGPathCreateWithEllipseInRect(CGRect r, const CGAffineTransform *m) { void *p = CGPathCreateMutable(); CGPathAddEllipseInRect(p, m, r); return prim_mark(p, PRIM_ELLIPSE, r, 0, 0, m); }
void *CGPathCreateWithRoundedRect(CGRect r, CGFloat cw, CGFloat ch, const CGAffineTransform *m) {
    void *p = CGPathCreateMutable();
    CGPathAddRoundedRect(p, m, r, cw, ch);
    CGRect s = CGRectStandardize(r);                 /* os mesmos limites que CGPathAddRoundedRect aplica */
    if (cw > s.size.width / 2) cw = s.size.width / 2;
    if (ch > s.size.height / 2) ch = s.size.height / 2;
    return cw <= 0 || ch <= 0 ? prim_mark(p, PRIM_RECT, r, 0, 0, m) : prim_mark(p, PRIM_RRECT, r, cw, ch, m);
}
/* SPI do CoreGraphics (OpenRenderBox, ORBPath.cpp): a forma primitiva do caminho no espaço dela + a transformação */
bool CGPathIsPrimitive(const void *path) { const cgpath_t *p = path; return p && p->prim != PRIM_NONE; }
bool CGPathIsRect(const void *path, CGRect *rect);
bool CGPathIsRectWithTransform(const void *path, CGRect *rect, CGAffineTransform *t) {
    const cgpath_t *p = path;
    if (!p) return false;
    if (p->prim == PRIM_RECT) { if (rect) *rect = p->prect; if (t) *t = p->pm; return true; }
    if (p->prim == PRIM_NONE && CGPathIsRect(path, rect)) { if (t) *t = CGAffineTransformIdentity; return true; }
    return false;
}
bool CGPathIsEllipseWithTransform(const void *path, CGRect *rect, CGAffineTransform *t) {
    const cgpath_t *p = path;
    if (!p || p->prim != PRIM_ELLIPSE) return false;
    if (rect) *rect = p->prect;
    if (t) *t = p->pm;
    return true;
}
bool CGPathIsRoundedRectWithTransform(const void *path, CGRect *rect, CGFloat *cw, CGFloat *ch, CGAffineTransform *t) {
    const cgpath_t *p = path;
    if (!p || (p->prim != PRIM_RRECT && p->prim != PRIM_RECT)) return false;
    if (rect) *rect = p->prect;
    if (cw) *cw = p->pcw;
    if (ch) *ch = p->pch;
    if (t) *t = p->pm;
    return true;
}

void CGPathAddPath(void *dst, const CGAffineTransform *m, const void *srcp) {
    const cgpath_t *s = srcp;
    if (!dst || !s) return;
    u64 pi = 0;
    for (u64 i = 0; i < s->n; i++) {
        int t = s->types[i], k = pts_for[t];
        CGPoint q[3];
        for (int j = 0; j < k; j++) q[j] = m ? CGPointApplyAffineTransform(s->pts[pi + j], *m) : s->pts[pi + j];
        push(dst, t, q);
        pi += k;
    }
}
void *CGPathCreateMutableCopyByTransformingPath(const void *src, const CGAffineTransform *m) {
    if (!src) return 0;
    void *p = CGPathCreateMutable();
    CGPathAddPath(p, m, src);
    const cgpath_t *s = src;                          /* a cópia continua sendo a mesma forma primitiva */
    if (s->prim != PRIM_NONE) {
        CGAffineTransform t = m ? CGAffineTransformConcat(s->pm, *m) : s->pm;
        prim_mark(p, s->prim, s->prect, s->pcw, s->pch, &t);
    }
    return p;
}
void *CGPathCreateMutableCopy(const void *src) { return CGPathCreateMutableCopyByTransformingPath(src, 0); }
void *CGPathCreateCopy(const void *src) { return CGPathCreateMutableCopyByTransformingPath(src, 0); }
void *CGPathCreateCopyByTransformingPath(const void *src, const CGAffineTransform *m) { return CGPathCreateMutableCopyByTransformingPath(src, m); }
static id path_copy(id self, SEL c) { (void)c; return (id)CGPathCreateCopy(self); }

bool CGPathEqualToPath(const void *a, const void *b) {
    const cgpath_t *p = a, *q = b;
    if (p == q) return true;
    if (!p || !q || p->n != q->n || p->npts != q->npts) return false;
    if (p->n && memcmp(p->types, q->types, p->n)) return false;
    return !p->npts || !memcmp(p->pts, q->pts, p->npts * sizeof(CGPoint));
}
bool CGPathIsEmpty(const void *path) { const cgpath_t *p = path; return !p || p->n == 0; }
size_t CGPathGetNumberOfPoints(const void *path) { const cgpath_t *p = path; return p ? p->npts : 0; }
size_t CGPathGetNumberOfElements(const void *path) { const cgpath_t *p = path; return p ? p->n : 0; }
CGPoint CGPathGetCurrentPoint(const void *path) { const cgpath_t *p = path; return (p && p->n) ? p->cur : CGPointZero; }

void CGPathApply(const void *path, void *info, CGPathApplierFunction fn) {
    const cgpath_t *p = path;
    if (!p || !fn) return;
    u64 pi = 0;
    for (u64 i = 0; i < p->n; i++) {
        CGPathElement el = {p->types[i], p->pts + pi};
        fn(info, &el);
        pi += pts_for[p->types[i]];
    }
}
/* blocos: {isa, flags, reserved, invoke, ...}; invoke recebe o próprio bloco primeiro */
typedef struct { void *isa; int flags, reserved; void *invoke; } block_t;
void CGPathApplyWithBlock2(const void *path, void *blk) {
    const cgpath_t *p = path;
    if (!p || !blk) return;
    void (*inv)(void *, const CGPathElement *, bool *) = (void (*)(void *, const CGPathElement *, bool *))((block_t *)blk)->invoke;
    u64 pi = 0;
    for (u64 i = 0; i < p->n; i++) {
        CGPathElement el = {p->types[i], p->pts + pi};
        bool stop = false;
        inv(blk, &el, &stop);
        if (stop) break;
        pi += pts_for[p->types[i]];
    }
}
void CGPathApplyWithBlock(const void *path, void *blk) {
    const cgpath_t *p = path;
    if (!p || !blk) return;
    void (*inv)(void *, const CGPathElement *) = (void (*)(void *, const CGPathElement *))((block_t *)blk)->invoke;
    u64 pi = 0;
    for (u64 i = 0; i < p->n; i++) {
        CGPathElement el = {p->types[i], p->pts + pi};
        inv(blk, &el);
        pi += pts_for[p->types[i]];
    }
}

static void grow_box(double *b, CGPoint q) { b[0] = dmin(b[0], q.x); b[1] = dmin(b[1], q.y); b[2] = dmax(b[2], q.x); b[3] = dmax(b[3], q.y); }
static CGRect box_rect(const double *b) { if (b[0] > b[2]) return CGRectNull; CGRect r = {{b[0], b[1]}, {b[2] - b[0], b[3] - b[1]}}; return r; }
/* inclui pontos de controle */
CGRect CGPathGetBoundingBox(const void *path) {
    const cgpath_t *p = path;
    double b[4] = {INF, INF, -INF, -INF};
    for (u64 i = 0; p && i < p->npts; i++) grow_box(b, p->pts[i]);
    return box_rect(b);
}
/* curvas amostradas: caixa justa da geometria desenhada */
static CGPoint bez(const CGPoint *p0, const CGPoint *c, int k, double t) {
    double u = 1 - t;
    CGPoint o;
    if (k == 2) { o.x = u * u * p0->x + 2 * u * t * c[0].x + t * t * c[1].x; o.y = u * u * p0->y + 2 * u * t * c[0].y + t * t * c[1].y; }
    else { o.x = u * u * u * p0->x + 3 * u * u * t * c[0].x + 3 * u * t * t * c[1].x + t * t * t * c[2].x;
           o.y = u * u * u * p0->y + 3 * u * u * t * c[0].y + 3 * u * t * t * c[1].y + t * t * t * c[2].y; }
    return o;
}
/* percorre o caminho achatado em segmentos de reta: cb(a, b) para cada aresta (subcaminhos fechados) */
static void flatten(const cgpath_t *p, void (*cb)(void *, CGPoint, CGPoint), void *ctx, int close_open) {
    if (!p) return;
    CGPoint cur = {0, 0}, start = {0, 0};
    int open = 0;
    u64 pi = 0;
    for (u64 i = 0; i < p->n; i++) {
        int t = p->types[i], k = pts_for[t];
        const CGPoint *q = p->pts + pi;
        switch (t) {
        case MOVE: if (open && close_open) cb(ctx, cur, start); cur = start = q[0]; open = 1; break;
        case LINE: cb(ctx, cur, q[0]); cur = q[0]; break;
        case QUAD: case CURVE: {
            CGPoint prev = cur;
            for (int s = 1; s <= 16; s++) { CGPoint nx = bez(&cur, q, k, s / 16.0); cb(ctx, prev, nx); prev = nx; }
            cur = q[k - 1];
            break;
        }
        case CLOSE: cb(ctx, cur, start); cur = start; open = 0; break;
        }
        pi += k;
    }
    if (open && close_open) cb(ctx, cur, start);
}
static void box_cb(void *ctx, CGPoint a, CGPoint b) { grow_box(ctx, a); grow_box(ctx, b); }
CGRect CGPathGetPathBoundingBox(const void *path) {
    const cgpath_t *p = path;
    double b[4] = {INF, INF, -INF, -INF};
    if (!p) return CGRectNull;
    for (u64 i = 0, pi = 0; i < p->n; pi += pts_for[p->types[i]], i++) if (p->types[i] == MOVE && i + 1 == p->n) grow_box(b, p->pts[pi]);
    flatten(p, box_cb, b, 0);
    return box_rect(b);
}
bool CGPathIsRect(const void *path, CGRect *rect) {
    const cgpath_t *p = path;
    if (!p || p->npts < 4 || p->npts > 5) return false;
    if (p->types[0] != MOVE) return false;
    for (u64 i = 1; i < p->n; i++) if (p->types[i] != LINE && p->types[i] != CLOSE) return false;
    const CGPoint *q = p->pts;
    bool axis = (q[0].y == q[1].y && q[1].x == q[2].x && q[2].y == q[3].y && q[3].x == q[0].x) ||
                (q[0].x == q[1].x && q[1].y == q[2].y && q[2].x == q[3].x && q[3].y == q[0].y);
    if (!axis) return false;
    if (rect) *rect = CGPathGetBoundingBox(path);
    return true;
}
typedef struct { CGPoint pt; int wind, cross; } hit_t;
static void hit_cb(void *ctx, CGPoint a, CGPoint b) {
    hit_t *h = ctx;
    if ((a.y <= h->pt.y) != (b.y <= h->pt.y)) {
        double x = a.x + (h->pt.y - a.y) * (b.x - a.x) / (b.y - a.y);
        if (x > h->pt.x) { h->cross++; h->wind += b.y > a.y ? 1 : -1; }
    }
}
bool CGPathContainsPoint(const void *path, const CGAffineTransform *m, CGPoint pt, bool eo) {
    if (!path) return false;
    if (m) pt = CGPointApplyAffineTransform(pt, CGAffineTransformInvert(*m));
    hit_t h = {pt, 0, 0};
    flatten(path, hit_cb, &h, 1);
    return eo ? (h.cross & 1) : h.wind != 0;
}

/* ======================================================================== CGColorSpace */
extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } cg_cfconst_t;
#define CSNAME(var, text) \
    static cg_cfconst_t var##_s = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; \
    const void *var = &var##_s;
CSNAME(kCGColorSpaceGenericGray, "kCGColorSpaceGenericGray")
CSNAME(kCGColorSpaceGenericRGB, "kCGColorSpaceGenericRGB")
CSNAME(kCGColorSpaceGenericGrayGamma2_2, "kCGColorSpaceGenericGrayGamma2_2")
CSNAME(kCGColorSpaceSRGB, "kCGColorSpaceSRGB")
CSNAME(kCGColorSpaceExtendedSRGB, "kCGColorSpaceExtendedSRGB")
CSNAME(kCGColorSpaceLinearSRGB, "kCGColorSpaceLinearSRGB")
CSNAME(kCGColorSpaceExtendedLinearSRGB, "kCGColorSpaceExtendedLinearSRGB")
CSNAME(kCGColorSpaceDisplayP3, "kCGColorSpaceDisplayP3")
CSNAME(kCGColorSpaceExtendedDisplayP3, "kCGColorSpaceExtendedDisplayP3")
CSNAME(kCGColorSpaceLinearDisplayP3, "kCGColorSpaceLinearDisplayP3")
CSNAME(kCGColorSpaceExtendedLinearDisplayP3, "kCGColorSpaceExtendedLinearDisplayP3")
CSNAME(kCGColorSpaceExtendedGray, "kCGColorSpaceExtendedGray")
CSNAME(kCGColorSpaceLinearGray, "kCGColorSpaceLinearGray")
CSNAME(kCGColorSpaceExtendedLinearGray, "kCGColorSpaceExtendedLinearGray")
CSNAME(kCGColorSpaceITUR_2100_PQ, "kCGColorSpaceITUR_2100_PQ")
CSNAME(kCGColorSpaceITUR_2100_HLG, "kCGColorSpaceITUR_2100_HLG")

typedef struct {
    cx_class_t *isa; u64 cfinfo; const void *name; i32 model; u32 ncomp; u32 extended;
    void *base; u8 *table; int ntable;        /* indexado: espaço base e tabela (ntable entradas × ncomp do base) */
    u8 *icc; long nicc;                      /* perfil ICC (CGColorSpaceCreateWithICCData / ICCBased) */
    double white[3], black[3], gamma[3], matrix[9], range[8];   /* calibrado, Lab */
    void *prof; int prof_srgb;               /* perfil do motor do ColorSync (cg_color.inc), lido na primeira vez */
    int calib;                               /* 1 CalibratedRGB, 2 CalibratedGray, 3 Lab */
} cgspace_t;
static cx_class_t *space_cls;
#define MAX_SPACES 24
static cgspace_t *spaces[MAX_SPACES];
static int nspaces;

static const char *cfstr_c(const void *s) { return s ? cx_cstr((id)s) : ""; }
u64 CGColorSpaceGetTypeID(void) { return 0x2A1; }
void *CGColorSpaceCreateWithName(const void *name) {
    if (!name) return 0;
    const char *n = cfstr_c(name);
    for (int i = 0; i < nspaces; i++) if (!strcmp(cfstr_c(spaces[i]->name), n)) return objc_retain((id)spaces[i]);
    if (nspaces == MAX_SPACES) return 0;
    if (!space_cls) {
        cx_class_t *k = objc_allocateClassPair(cx_class_named("NSObject"), "CGColorSpace", 0);
        if (!k) k = cx_class_named("CGColorSpace");
        class_addIvar(k, "_cg", sizeof(cgspace_t) - 8, 3, "?");
        objc_registerClassPair(k);
        space_cls = k;
    }
    cgspace_t *s = (cgspace_t *)class_createInstance(space_cls, 0);
    if (!s) return 0;
    int gray = strstr(n, "Gray") != 0, cmyk = strstr(n, "CMYK") != 0, lab = strstr(n, "Lab") != 0, xyz = strstr(n, "XYZ") != 0;
    s->name = name;
    s->model = gray ? 0 : cmyk ? 2 : lab ? 3 : xyz ? 7 : 1;     /* CGColorSpaceModel */
    s->ncomp = gray ? 1 : cmyk ? 4 : 3;
    if (lab) { s->white[0] = 0.9642; s->white[1] = 1; s->white[2] = 0.8249; s->range[0] = -128; s->range[1] = 127; s->range[2] = -128; s->range[3] = 127; s->calib = 3; }
    s->extended = strstr(n, "Extended") != 0 || strstr(n, "ITUR_2100") != 0;
    objc_retain((id)s);                        /* a tabela mantém uma referência própria */
    spaces[nspaces++] = s;
    return s;
}
void *CGColorSpaceCreateDeviceRGB(void) { return CGColorSpaceCreateWithName(kCGColorSpaceSRGB); }
void *CGColorSpaceCreateDeviceGray(void) { return CGColorSpaceCreateWithName(kCGColorSpaceGenericGrayGamma2_2); }
/* espaços calibrados: o mOS não faz gerenciamento de cor; aproximados por sRGB / cinza gamma 2.2 */
static cgspace_t *space_new(int model, int ncomp);
void *CGColorSpaceCreateCalibratedRGB(const CGFloat *white, const CGFloat *black, const CGFloat *gamma, const CGFloat *matrix) {
    if (!white || white[1] <= 0) return 0;
    cgspace_t *s = space_new(1, 3);
    if (!s) return 0;
    s->calib = 1;
    for (int i = 0; i < 3; i++) { s->white[i] = white[i]; s->black[i] = black ? black[i] : 0; s->gamma[i] = gamma ? gamma[i] : 1; }
    for (int i = 0; i < 9; i++) s->matrix[i] = matrix ? matrix[i] : (i % 4 == 0);
    return s;
}
void *CGColorSpaceCreateCalibratedGray(const CGFloat *white, const CGFloat *black, CGFloat gamma) {
    if (!white || white[1] <= 0) return 0;
    cgspace_t *s = space_new(0, 1);
    if (!s) return 0;
    s->calib = 2;
    for (int i = 0; i < 3; i++) { s->white[i] = white[i]; s->black[i] = black ? black[i] : 0; }
    s->gamma[0] = gamma > 0 ? gamma : 1;
    return s;
}
void *CGColorSpaceRetain(void *s) { return s ? (void *)objc_retain((id)s) : 0; }
void CGColorSpaceRelease(void *s) { if (s) objc_release((id)s); }
const void *CGColorSpaceGetName(const void *s) { return s ? ((const cgspace_t *)s)->name : 0; }
const void *CGColorSpaceCopyName(const void *s) { return s ? (const void *)objc_retain((id)((const cgspace_t *)s)->name) : 0; }
size_t CGColorSpaceGetNumberOfComponents(const void *s) { return s ? ((const cgspace_t *)s)->ncomp : 0; }
i32 CGColorSpaceGetModel(const void *s) { return s ? ((const cgspace_t *)s)->model : -1; }
bool CGColorSpaceUsesExtendedRange(const void *s) { return s && ((const cgspace_t *)s)->extended; }
bool CGColorSpaceIsWideGamutRGB(const void *s) { return s && strstr(cfstr_c(((const cgspace_t *)s)->name), "P3") != 0; }

/* ======================================================================== CGColor (= NSColor do mOS) */
typedef struct { cx_class_t *isa; double r, g, b, a; } cgcolor_t;      /* igual a rgba_t do appkit.c */
u64 CGColorGetTypeID(void) { return 0x2A2; }
static id space_color(void *space, const CGFloat *comps, int n, void *pattern);
void *CGColorGetPattern(const void *c);
static void *colx_any(const void *c);
static void *colx_space(const void *c);
static int colx_n(const void *c);
static const double *colx_comps(const void *c);
void *CGColorCreate(const void *space, const CGFloat *c) {
    const cgspace_t *s = space;
    if (!c || !s) return 0;
    if (s->model == 6) return 0;                 /* espaço de padrão: CGColorCreateWithPattern */
    return space_color((void *)space, c, (int)s->ncomp + 1, 0);
}
void *CGColorCreateSRGB(CGFloat r, CGFloat g, CGFloat b, CGFloat a) {
    CGFloat c[4] = {r, g, b, a};
    void *s = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    id o = space_color(s, c, 4, 0);
    CGColorSpaceRelease(s);
    return o;
}
void *CGColorCreateCopy(const void *c) {
    const cgcolor_t *k = c;
    if (!k) return 0;
    if (colx_any(c)) return space_color(colx_space(c), colx_comps(c), colx_n(c), CGColorGetPattern(c));
    return cx_color(k->r, k->g, k->b, k->a);
}
void *CGColorCreateCopyWithAlpha(const void *c, CGFloat a) {
    const cgcolor_t *k = c;
    if (!k) return 0;
    if (colx_any(c)) {
        double v[6];
        int n = colx_n(c);
        memcpy(v, colx_comps(c), sizeof v);
        if (n > 0) v[n - 1] = a;
        return space_color(colx_space(c), v, n, CGColorGetPattern(c));
    }
    return cx_color(k->r, k->g, k->b, a);
}
bool CGColorEqualToColor(const void *x, const void *y) {
    const cgcolor_t *a = x, *b = y;
    if (a == b) return true;
    if (!a || !b) return false;
    if (colx_any(x) || colx_any(y)) {
        if (!colx_any(x) || !colx_any(y) || colx_space(x) != colx_space(y) || colx_n(x) != colx_n(y)) return false;
        for (int i = 0; i < colx_n(x); i++) if (colx_comps(x)[i] != colx_comps(y)[i]) return false;
        return CGColorGetPattern(x) == CGColorGetPattern(y);
    }
    return a->r == b->r && a->g == b->g && a->b == b->b && a->a == b->a;
}
size_t CGColorGetNumberOfComponents(const void *c) { return !c ? 0 : colx_any(c) ? (size_t)colx_n(c) : 4; }
const CGFloat *CGColorGetComponents(const void *c) { return !c ? 0 : colx_any(c) ? colx_comps(c) : &((const cgcolor_t *)c)->r; }
CGFloat CGColorGetAlpha(const void *c) { return c ? ((const cgcolor_t *)c)->a : 0; }
void *CGColorGetColorSpace(const void *c) {
    static void *srgb;
    if (!c) return 0;
    if (colx_any(c) && colx_space(c)) return colx_space(c);
    if (!srgb) srgb = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);   /* referência mantida: "Get" não transfere posse */
    return srgb;
}

/* ======================================================================== bitmap, CGImage, data provider
 * O Skia do Chromium pede um CGBitmapContext, desenha com CTFontDrawGlyphs e lê os pixels.
 * CGImage vira contents de CALayer (quartzcore.c pergunta por cx_cgimage_src). */
#define CGK_BM 0xC6B10001u
#define CGK_IM 0xC6B10002u
#define CGK_PR 0xC6B10003u
/* Estado de desenho do CGBitmapContext além do CTM/recorte retangular (coregraphics_bm.inc). */
typedef struct {
    u32 stroke; double lw, miter, phase, dash[16]; int ndash, cap, join; double galpha; int aa;
    double sdx, sdy, sblur; u32 scolor; int pattern;
    u8 *mask;
    u8 interp; double flat, charsp; void *font; double fsize; int tmode;
} cg_bmx_gs;
typedef struct {
    cgr_poly path;               /* no espaço do dispositivo */
    u32 stroke;                  /* ARGB não pré-multiplicado */
    double lw, miter, phase, dash[16];
    int ndash, cap, join, aa;
    double galpha;
    double sdx, sdy, sblur; u32 scolor;           /* sombra: guardada, ainda não desenhada */
    u8 *mask;                    /* cobertura do recorte por pixel; NULL = só o retângulo */
    u32 *tile; int tw, th, pattern_on; double pdx, pdy;   /* padrão de preenchimento, pré-multiplicado */
    void *fill_space;
    u8 *layer_data[4]; double layer_alpha[4]; int layer_owns[4]; int nlayer;
    cg_bmx_gs gs[8]; int gsp;
    u8 lfmt[4]; u32 linfo[4], lbpr[4], lbpp[4]; double lshadow[4][3]; u32 lscolor[4]; u8 lblend[4];
    u32 *sbuf; int sx0, sy0, sx1, sy1;     /* sombra: o desenho vai para cá e depois é composto com a sombra */
    u8 interp; double flat, charsp;
    void *font; double fsize; int tmode;    /* CGContextSetFont / SetFontSize / SetTextDrawingMode */
    void *stroke_space; u32 *stile; int stw, sth, spattern_on;   /* padrão de traço */
    void *ctfont, *ctfont_src; double ctfont_size;                 /* CTFont em cache para o texto do CGContext */
} cg_bmx_t;
typedef struct {
    cx_class_t *isa;
    u32 kind;
    u8 *data;
    u32 w, h, bpr, bpp, info;
    int owns;
    double ta, tb, tc, td, tx, ty;          /* matriz de texto */
    double ca, cb, cc, cd, cu, cv;          /* CTM: x' = ca*x + cc*y + cu */
    i32 cl, ct, cr, cbt;                     /* recorte em pixels do bitmap, [cl,cr) × [ct,cbt) */
    u32 fill;
    int gsp;
    double gstk[8][6];
    double tstk[8][6];
    i32 cstk[8][4];
    u32 fillstk[8];
    u8 blend;                                /* CGContextSetBlendMode: 0 normal, 1 copy, 2 multiply, 3 screen, 16 clear */
    u8 blendstk[8];
    void *cb_info;
    void (*rel_cb)(void *, const void *, size_t);
    u64 cb_size;
    cg_bmx_t *gx;                            /* só bitmap: path, traço, máscara, padrão, camadas */
    u8 fmt;                                  /* bitmap: PF_* de cg_pixel.inc */
    u8 ismask;                               /* imagem: máscara de imagem (CGImageMaskCreate), só alfa */
    u8 shint;                                /* imagem: shouldInterpolate */
    u16 sbpc, sbpp;                          /* imagem: bits por componente / pixel declarados na criação */
    void *space;                             /* espaço de cor (retido) */
    const void *uti;                         /* imagem: tipo de origem (ImageIO) */
    double decode[10]; int ndecode;
    void *pdf;                               /* CGPDFContext: o gravador (coregraphics_pdf.inc) */
    int fss;                                 /* CGContextSetFontSmoothingStyle */
} cgobj_t;
#include "cg_pixel.inc"
static void pw_flush(cgobj_t *o);
static void cg_derived_layout(cgobj_t *n);
static void bmx_free(cgobj_t *o);
static void bmx_save(cgobj_t *o);
static void bmx_restore(cgobj_t *o);
static u32 fill_src(cgobj_t *o, int x, int y);
static cx_class_t BM_c, BM_m, IM_c, IM_m, PR_c, PR_m;
static int cg_ready;
extern void cx_object_free(id o);
extern u32 cx_cg_fill_argb(void);
static void pw_dealloc(cgobj_t *o);
static void cg_dealloc(id self, SEL c) {
    (void)c;
    cgobj_t *o = (cgobj_t *)self;
    if (o->pdf) pw_dealloc(o);
    if (o->kind == CGK_PR && o->rel_cb) o->rel_cb(o->cb_info, o->data, (size_t)o->cb_size);
    if (o->kind == CGK_IM && o->cb_info) objc_release((id)o->cb_info);
    if (o->kind == CGK_BM) {
        bmx_free(o);
        if (o->rel_cb) ((void (*)(void *, void *))(void *)o->rel_cb)(o->cb_info, o->data);   /* CGBitmapContextCreateWithData */
    }
    if (o->owns) free(o->data);
    if (o->space) objc_release((id)o->space);
    o->data = 0;
    o->kind = 0;
    cx_object_free(self);
}
static void cg_ensure(void) {
    if (cg_ready) return;
    cx_class_t *ns = cx_class_named("NSObject");
    if (!ns) return;
    u32 extra = (u32)(sizeof(cgobj_t) - sizeof(void *));
    cx_define_class(&BM_c, &BM_m, "CGBitmapContext", ns, extra);
    cx_define_class(&IM_c, &IM_m, "CGImage", ns, extra);
    cx_define_class(&PR_c, &PR_m, "CGDataProvider", ns, extra);
    cx_add_method(&BM_c, "dealloc", (void *)cg_dealloc);
    cx_add_method(&IM_c, "dealloc", (void *)cg_dealloc);
    cx_add_method(&PR_c, "dealloc", (void *)cg_dealloc);
    cg_ready = 1;
}
static cgobj_t *as_kind(const void *p, u32 kind) {
    cgobj_t *o = (cgobj_t *)p;
    return o && o->kind == kind ? o : 0;
}
static cgobj_t *cg_new(cx_class_t *cls, u32 kind) {
    cg_ensure();
    if (!cg_ready) return 0;
    cgobj_t *o = (cgobj_t *)cx_alloc_instance(cls);
    if (!o) return 0;
    o->kind = kind;
    o->ta = 1;
    o->td = 1;
    return o;
}

#include "cg_color.inc"

void *CGDataProviderCreateWithData(void *info, const void *data, size_t size, void (*release)(void *, const void *, size_t)) {
    if (!data || !size) return 0;
    cgobj_t *o = cg_new(&PR_c, CGK_PR);
    if (!o) return 0;
    o->data = (u8 *)data;
    o->cb_size = size;
    o->cb_info = info;
    o->rel_cb = release;
    return o;
}
void CGDataProviderRelease(void *p) { if (as_kind(p, CGK_PR)) objc_release((id)p); }
/* CGDataProviderCreateWithURL: lê o arquivo inteiro (só file://); o provedor é dono do buffer. */
u8 CFURLGetFileSystemRepresentation(id url, u8 resolve, u8 *buf, long max);
static void cg_free_bytes(void *info, const void *data, size_t size) { (void)info; (void)size; free((void *)data); }
void *CGDataProviderCreateWithURL(id url) {
    char path[1024];
    if (!url || !CFURLGetFileSystemRepresentation(url, 1, (u8 *)path, (long)sizeof path)) return 0;
    FILE_ *f = fopen(path, "rb");
    if (!f) return 0;
    size_t cap = 64 * 1024, n = 0;
    u8 *buf = malloc(cap);
    while (buf) {
        size_t got = fread(buf + n, 1, cap - n, f);
        n += got;
        if (n < cap) break;
        if (cap >= (256u << 20)) { free(buf); buf = 0; break; }   /* mesmo teto dos bitmaps: nada de arquivo gigante na memória */
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
void *CGDataProviderRetain(void *p) { if (as_kind(p, CGK_PR)) objc_retain((id)p); return p; }

/* qualquer layout aceito pelo macOS vira ARGB 8 bits (cg_pixel.inc); pré-multiplicado fica marcado (cb_size) */
static void *image_from(size_t w, size_t h, size_t bpc, size_t bpp, size_t bpr, const void *space, u32 info,
                        cgobj_t *p, const double *decode, int mask, int intent, int interp) {
    const cgspace_t *cs = space;
    if (!p || !w || !h || !bpp) return 0;
    if (bpc != 1 && bpc != 2 && bpc != 4 && bpc != 5 && bpc != 8 && bpc != 16 && bpc != 32) return 0;
    if ((u64)w * h * 4 > (1ull << 30)) return 0;
    if (!bpr) bpr = (w * bpp + 7) / 8;
    if ((u64)bpr * (h - 1) + (w * bpp + 7) / 8 > p->cb_size) return 0;
    unpack_t u = {bpc, bpp, 0, 0, info, 1, decode, 0, 0, 0, 1, 0, 0};
    double labdec[10];
    u32 ai = info & 31;
    if (mask) { u.model = -2; u.ncolor = 1; }
    else if (ai == AI_ONLY) u.ncolor = 0;
    else {
        int model = cs ? cs->model : (bpp / (bpc ? bpc : 8) >= 3 ? 1 : 0);
        u.model = model;
        u.ncolor = model == 0 ? 1 : model == 2 ? 4 : model == 5 ? 1 : 3;
        if (model == 5 && cs) {
            const cgspace_t *bs = cs->base;
            u.table = cs->table; u.ntable = cs->ntable;
            u.tcomp = bs ? (int)bs->ncomp : 3; u.table_model = bs ? bs->model : 1;
        }
        /* espaço que não é sRGB (ICC, calibrado, Lab, P3, CMYK com perfil...): o ColorSync converte cada pixel */
        cgspace_t *xs = model == 5 && cs ? (cgspace_t *)cs->base : (cgspace_t *)cs;
        if (xs && space_prof(xs)) { u.xf = unpack_xf; u.xfctx = xs; }
        if (model == 3) {
            u.model = 1;
            if (!decode && cs) {                     /* decode padrão do Lab: L 0..100, a e b pela faixa do espaço */
                labdec[0] = 0; labdec[1] = 100;
                labdec[2] = cs->range[0]; labdec[3] = cs->range[1]; labdec[4] = cs->range[2]; labdec[5] = cs->range[3];
                labdec[6] = 0; labdec[7] = 1;
                u.decode = labdec;
            }
        }
    }
    u.ncomp = u.ncolor + (ai_present(ai) && ai != AI_ONLY && !mask ? 1 : 0);
    if (!mask && ai != AI_ONLY && bpc != 5 && bpp < u.ncomp * bpc) return 0;
    cgobj_t *o = cg_new(&IM_c, CGK_IM);
    if (!o) return 0;
    o->w = (u32)w; o->h = (u32)h; o->bpr = (u32)w * 4; o->bpp = 4; o->info = info; o->owns = 1;
    o->sbpc = (u16)bpc; o->sbpp = (u16)bpp; o->ismask = (u8)mask; o->shint = (u8)interp;
    o->blend = (u8)(intent & 0xFF);          /* imagem não usa blend: guarda o rendering intent */
    if (space) o->space = objc_retain((id)space);
    if (decode) {
        int nd = (int)(2 * (u.ncolor + (ai_has_alpha(ai) ? 1 : 0)));
        if (mask) nd = 2;
        if (nd > 10) nd = 10;
        for (int i = 0; i < nd; i++) o->decode[i] = decode[i];
        o->ndecode = nd;
    }
    o->data = malloc((size_t)o->bpr * h);
    if (!o->data) { objc_release((id)o); return 0; }
    int prem = 0;
    for (size_t y = 0; y < h; y++) unpack_row(&u, p->data + y * bpr, w, (u32 *)(o->data + y * o->bpr), &prem);
    o->cb_info = objc_retain((id)p);         /* CGImageGetDataProvider devolve o provedor original */
    o->cb_size = prem || mask || ai == AI_ONLY ? 1 : 0;   /* só alfa / máscara: RGB 0, já "pré-multiplicado" */
    return o;
}
void *CGImageCreate(size_t w, size_t h, size_t bpc, size_t bpp, size_t bpr, const void *space, u32 info,
                    void *provider, const void *decode, bool shouldInterpolate, int intent) {
    return image_from(w, h, bpc, bpp, bpr, space, info, as_kind(provider, CGK_PR), decode, 0, intent, shouldInterpolate);
}
void *CGImageMaskCreate(size_t w, size_t h, size_t bpc, size_t bpp, size_t bpr, void *provider, const double *decode, bool interp) {
    if (bpc != 1 && bpc != 2 && bpc != 4 && bpc != 8) return 0;
    return image_from(w, h, bpc, bpp, bpr, 0, 0, as_kind(provider, CGK_PR), decode, 1, 0, interp);
}
void CGImageRelease(void *img) { if (as_kind(img, CGK_IM)) objc_release((id)img); }
void *CGImageRetain(void *img) { if (as_kind(img, CGK_IM)) objc_retain((id)img); return img; }
size_t CGImageGetWidth(void *img) { cgobj_t *o = as_kind(img, CGK_IM); return o ? o->w : 0; }
size_t CGImageGetHeight(void *img) { cgobj_t *o = as_kind(img, CGK_IM); return o ? o->h : 0; }
size_t CGImageGetBytesPerRow(void *img) { cgobj_t *o = as_kind(img, CGK_IM); return o ? o->bpr : 0; }

int cgimage_crop(const uint8_t *src, uint32_t sw, uint32_t sh, uint32_t sbpr, uint32_t byte_pp,
                 double x, double y, double w, double h,
                 uint8_t **dst, uint32_t *dw, uint32_t *dh, uint32_t *dbpr);
/* Interseção do retângulo integral com a imagem. bits, bytesPerRow, bitmapInfo e o espaço
 * (CGImageGetColorSpace) continuam os da origem. NULL se a interseção é vazia. */
void *CGImageCreateWithImageInRect(void *img, CGRect rect) {
    cgobj_t *o = as_kind(img, CGK_IM);
    uint8_t *data = 0;
    uint32_t w = 0, h = 0, bpr = 0;
    if (!o || !o->data) return 0;
    if (!cgimage_crop(o->data, o->w, o->h, o->bpr, o->bpp, rect.origin.x, rect.origin.y,
                      rect.size.width, rect.size.height, &data, &w, &h, &bpr))
        return 0;
    cgobj_t *n = cg_new(&IM_c, CGK_IM);
    if (!n) { free(data); return 0; }
    n->data = data;
    n->w = w;
    n->h = h;
    n->bpr = bpr;
    n->bpp = o->bpp;
    n->info = o->info;
    n->owns = 1;
    n->cb_size = o->cb_size;
    n->ismask = o->ismask; n->shint = o->shint; n->uti = o->uti;
    if (o->space) n->space = objc_retain((id)o->space);
    cg_derived_layout(n);
    return n;
}
int cx_cgimage_src(const void *img, const u8 **px, long *bpr, int *w, int *h, int *premul) {
    cgobj_t *o = as_kind(img, CGK_IM);
    if (!o || !o->data) return 0;
    if (px) *px = o->data;
    if (bpr) *bpr = (long)o->bpr;
    if (w) *w = (int)o->w;
    if (h) *h = (int)o->h;
    if (premul) *premul = o->cb_size ? 1 : 0;
    return 1;
}

void *CGBitmapContextCreate(void *data, size_t w, size_t h, size_t bpc, size_t bpr, void *space, u32 bitmapInfo) {
    const cgspace_t *cs = space;
    if (!w || !h) return 0;
    int model = cs ? cs->model : 1;
    if ((bitmapInfo & 31) == AI_ONLY) model = -1;
    size_t bpp = 0;
    int fmt = ctx_format(bpc, bitmapInfo, model, &bpp);
    if (fmt == PF_NONE) return 0;
    if (!bpr) bpr = w * bpp;
    if (bpr < w * bpp) return 0;
    if ((u64)bpr * h > (1ull << 30)) return 0;
    int owns = 0;
    if (!data) {
        data = malloc(bpr * h);
        if (!data) return 0;
        memset(data, 0, bpr * h);
        owns = 1;
    }
    cgobj_t *o = cg_new(&BM_c, CGK_BM);
    if (!o) { if (owns) free(data); return 0; }
    o->data = data; o->w = (u32)w; o->h = (u32)h; o->bpr = (u32)bpr; o->bpp = (u32)bpp; o->info = bitmapInfo; o->owns = owns;
    o->fmt = (u8)fmt; o->sbpc = (u16)bpc; o->sbpp = (u16)(bpp * 8);
    if (space) o->space = objc_retain((id)space);
    /* como no macOS: origem embaixo à esquerda, y para cima */
    o->ca = 1; o->cd = -1; o->cv = (double)h;
    o->cl = 0; o->ct = 0; o->cr = (i32)w; o->cbt = (i32)h;
    o->fill = 0xFF000000;
    return o;
}
void CGContextRelease(void *c) { if (as_kind(c, CGK_BM)) objc_release((id)c); }
void *CGContextRetain(void *c) { if (as_kind(c, CGK_BM)) objc_retain((id)c); return c; }
void *CGBitmapContextGetData(void *c) { cgobj_t *o = as_kind(c, CGK_BM); return o ? o->data : 0; }
size_t CGBitmapContextGetWidth(void *c) { cgobj_t *o = as_kind(c, CGK_BM); return o ? o->w : 0; }
size_t CGBitmapContextGetHeight(void *c) { cgobj_t *o = as_kind(c, CGK_BM); return o ? o->h : 0; }
size_t CGBitmapContextGetBytesPerRow(void *c) { cgobj_t *o = as_kind(c, CGK_BM); return o ? o->bpr : 0; }
size_t CGBitmapContextGetBitsPerPixel(void *c) { cgobj_t *o = as_kind(c, CGK_BM); return o ? o->bpp * 8 : 0; }
size_t CGBitmapContextGetBitsPerComponent(void *c) { cgobj_t *o = as_kind(c, CGK_BM); return o ? o->sbpc : 0; }
u32 CGBitmapContextGetBitmapInfo(void *c) { cgobj_t *o = as_kind(c, CGK_BM); return o ? o->info : 0; }
void CGContextSetShouldSmoothFonts(void *c, int v) { (void)c; (void)v; }
void CGContextSetAllowsFontSmoothing(void *c, int v) { (void)c; (void)v; }
void CGContextSetShouldSubpixelPositionFonts(void *c, int v) { (void)c; (void)v; }
void CGContextSetAllowsFontSubpixelPositioning(void *c, int v) { (void)c; (void)v; }
void CGContextSetShouldSubpixelQuantizeFonts(void *c, int v) { (void)c; (void)v; }
void CGContextSetAllowsFontSubpixelQuantization(void *c, int v) { (void)c; (void)v; }
void CGContextSetTextMatrix(void *c, CGAffineTransform t) {
    cgobj_t *o = as_kind(c, CGK_BM);
    if (!o) return;
    o->ta = t.a; o->tb = t.b; o->tc = t.c; o->td = t.d; o->tx = t.tx; o->ty = t.ty;
}
CGAffineTransform CGContextGetTextMatrix(void *c) {
    cgobj_t *o = as_kind(c, CGK_BM);
    CGAffineTransform t = CGAffineTransformIdentity;
    if (!o) return t;
    t.a = o->ta; t.b = o->tb; t.c = o->tc; t.d = o->td; t.tx = o->tx; t.ty = o->ty;
    return t;
}
void cx_cg_note_blend(int mode);
void CGContextSetBlendMode(void *c, int mode) {
    cgobj_t *o = as_kind(c, CGK_BM);
    if (o) { if (o->pdf && o->blend != (u8)mode) pw_flush(o); o->blend = (u8)mode; return; }
    cx_cg_note_blend(mode);
}
static void shadow_note(cg_bmx_t *g, int x, int y);
static int shadow_begin(cgobj_t *o);
static void shadow_end(cgobj_t *o, int on);
static u32 scale_px(u32 p, u32 k);
static u32 fill_src(cgobj_t *o, int x, int y);
static void pw_note(cgobj_t *o, int x, int y);
static void pw_save(cgobj_t *o);
static void pw_restore(cgobj_t *o);
static void pw_clip_box(cgobj_t *o, int x0, int y0, int x1, int y1);
static void pw_reset_clip(cgobj_t *o);
static int pw_draw_image(cgobj_t *o, CGRect rect, cgobj_t *im);
static void blend_at(cgobj_t *o, int x, int y, u32 src) {
    int mode = o->pdf ? 0 : o->blend;          /* contexto PDF: o blend vale na hora de compor a imagem no PDF */
    if (x < o->cl || y < o->ct || x >= o->cr || y >= o->cbt) return;
    if (o->gx && o->gx->mask) {                /* recorte por path/máscara: a fonte vale só na cobertura */
        u32 k = o->gx->mask[(size_t)y * o->w + x];
        if (!k) return;
        if (k < 255) src = (((src >> 24) * k / 255) << 24) | ((((src >> 16) & 255) * k / 255) << 16) | ((((src >> 8) & 255) * k / 255) << 8) | ((src & 255) * k / 255);
    }
    if (o->gx && o->gx->sbuf) {                /* desenho com sombra: acumula "source over" no buffer da operação */
        u32 *sp = &o->gx->sbuf[(size_t)y * o->w + x];
        *sp = cx_cg_over(src, *sp);
        shadow_note(o->gx, x, y);
        return;
    }
    if (!cx_cg_blend_zero_src_matters(mode) && !(src >> 24)) return;
    if (o->pdf) pw_note(o, x, y);
    u8 *dp = o->data + (size_t)y * o->bpr + (size_t)x * o->bpp;
    u32 dst = ctx_get(o, dp);
    ctx_put(o, dp, cx_cg_blend_px(mode, src, dst));
}
static void ctm_of(const cgobj_t *o, double x, double y, double *dx, double *dy) {
    *dx = o->ca * x + o->cc * y + o->cu;
    *dy = o->cb * x + o->cd * y + o->cv;
}
static void dev_of(const cgobj_t *o, double x, double y, double *dx, double *dy) {
    double ux = o->ta * x + o->tc * y + o->tx;
    double uy = o->tb * x + o->td * y + o->ty;
    ctm_of(o, ux, uy, dx, dy);
}
int cx_bitmap_draw_glyph(void *ctx, double x, double y, const u8 *alpha, int gw, int gh, int left, int top) {
    cgobj_t *o = as_kind(ctx, CGK_BM);
    if (!o || !o->data || !alpha || gw <= 0 || gh <= 0) return 0;
    double px, py;
    dev_of(o, x, y, &px, &py);
    int ox = (int)floor(px) + left;
    int oy = (int)floor(py) + top;
    u32 fill = o->fill ? o->fill : cx_cg_fill_argb();
    if (!fill) fill = 0xFF000000;
    if (o->gx && o->gx->galpha < 1) fill = ((u32)((fill >> 24) * o->gx->galpha + 0.5) << 24) | (fill & 0xFFFFFF);
    /* itálico sintético: o Skia (SkScalerContext_Mac) põe a inclinação na matriz de texto (c = 0,25 para o
     * font-style: italic de uma fonte sem face itálica). Cada linha do glifo anda c/d px por px de altura acima
     * da linha de base, com a cobertura dividida entre os dois pixels vizinhos. */
    double shear = (o->td > 1e-6 || o->td < -1e-6) ? o->tc / o->td : 0;
    if (shear > 1 || shear < -1) shear = 0;
    int sh = shadow_begin(o);
    for (int gy = 0; gy < gh; gy++) {
        double dx = shear ? shear * (double)(-(top + gy)) : 0;
        int ix = (int)floor(dx);
        u32 fr = shear ? (u32)((dx - ix) * 256) : 0;          /* fração para o pixel da direita, em 1/256 */
        for (int gx = 0; gx < gw; gx++) {
            u8 cov = alpha[(size_t)gy * gw + gx];
            if (!cov) continue;
            for (int part = 0; part < (fr ? 2 : 1); part++) {
                u32 c = fr ? (part ? cov * fr : cov * (256 - fr)) >> 8 : cov;
                if (!c) continue;
                u32 A = ((fill >> 24) * c) / 255;
                u32 src = A << 24 | (((fill >> 16) & 255) * A / 255) << 16 | (((fill >> 8) & 255) * A / 255) << 8 | ((fill & 255) * A / 255);
                blend_at(o, ox + ix + gx + part, oy + gy, src);
            }
        }
    }
    shadow_end(o, sh);
    return 1;
}
void cx_bm_set_fill(const void *c, u32 argb) { cgobj_t *o = as_kind(c, CGK_BM); if (o) o->fill = argb; }
double cx_bm_scale(const void *c) {
    cgobj_t *o = as_kind(c, CGK_BM);
    if (!o) return 1;
    double s1 = sqrt(o->cb * o->cb + o->cd * o->cd);
    double s2 = sqrt(o->tb * o->tb + o->td * o->td);
    double s = s1 * s2;
    return s > 0.02 ? s : 1;
}
static void ctm_concat(cgobj_t *o, double a, double b, double c, double d, double tx, double ty) {
    double na = o->ca * a + o->cc * b, nb = o->cb * a + o->cd * b;
    double nc = o->ca * c + o->cc * d, nd = o->cb * c + o->cd * d;
    double nu = o->ca * tx + o->cc * ty + o->cu, nv = o->cb * tx + o->cd * ty + o->cv;
    o->ca = na; o->cb = nb; o->cc = nc; o->cd = nd; o->cu = nu; o->cv = nv;
}
void CGContextTranslateCTM(void *c, double tx, double ty) { cgobj_t *o = as_kind(c, CGK_BM); if (o) ctm_concat(o, 1, 0, 0, 1, tx, ty); }
void CGContextScaleCTM(void *c, double sx, double sy) { cgobj_t *o = as_kind(c, CGK_BM); if (o) ctm_concat(o, sx, 0, 0, sy, 0, 0); }
void CGContextConcatCTM(void *c, CGAffineTransform t) { cgobj_t *o = as_kind(c, CGK_BM); if (o) ctm_concat(o, t.a, t.b, t.c, t.d, t.tx, t.ty); }
CGAffineTransform CGContextGetCTM(void *c) {
    cgobj_t *o = as_kind(c, CGK_BM);
    CGAffineTransform t = CGAffineTransformIdentity;
    if (!o) return t;
    t.a = o->ca; t.b = o->cb; t.c = o->cc; t.d = o->cd; t.tx = o->cu; t.ty = o->cv;
    return t;
}
/* CTM inteira (o par do GetCTM) e a base: no bitmap do mOS a base é a troca de eixo do começo, e a CTM do
 * usuário fica relativa a ela */
void CGContextSetCTM(void *c, CGAffineTransform t) {
    cgobj_t *o = as_kind(c, CGK_BM);
    if (!o) return;
    o->ca = t.a; o->cb = t.b; o->cc = t.c; o->cd = t.d; o->cu = t.tx; o->cv = t.ty;
}
void CGContextSetBaseCTM(void *c, CGAffineTransform t) {
    cgobj_t *o = as_kind(c, CGK_BM);
    if (!o) return;
    /* base nova × a troca de eixo do bitmap; a CTM do usuário volta à identidade sobre ela */
    o->ca = t.a; o->cb = -t.b; o->cc = t.c; o->cd = -t.d; o->cu = t.tx; o->cv = (double)o->h - t.ty;
}
void CGContextSetFontSmoothingStyle(void *c, int style) { cgobj_t *o = as_kind(c, CGK_BM); if (o) o->fss = style; }
int CGContextGetFontSmoothingStyle(void *c) { cgobj_t *o = as_kind(c, CGK_BM); return o ? o->fss : 0; }
/* espaço do usuário -> dispositivo: no contexto de bitmap do mOS a CTM já é a transformação até os pixels */
CGSize CGContextConvertSizeToDeviceSpace(void *c, CGSize s) { return CGSizeApplyAffineTransform(s, CGContextGetCTM(c)); }
CGSize CGContextConvertSizeToUserSpace(void *c, CGSize s) {
    CGAffineTransform t = CGContextGetCTM(c);
    double det = t.a * t.d - t.b * t.c;
    if (det == 0) return s;
    CGAffineTransform inv = {t.d / det, -t.b / det, -t.c / det, t.a / det, 0, 0};
    return CGSizeApplyAffineTransform(s, inv);
}
int cx_bm_save(const void *c) {
    cgobj_t *o = as_kind(c, CGK_BM);
    if (!o || o->gsp >= 8) return o ? 1 : 0;
    o->gstk[o->gsp][0] = o->ca; o->gstk[o->gsp][1] = o->cb; o->gstk[o->gsp][2] = o->cc;
    o->gstk[o->gsp][3] = o->cd; o->gstk[o->gsp][4] = o->cu; o->gstk[o->gsp][5] = o->cv;
    o->tstk[o->gsp][0] = o->ta; o->tstk[o->gsp][1] = o->tb; o->tstk[o->gsp][2] = o->tc;
    o->tstk[o->gsp][3] = o->td; o->tstk[o->gsp][4] = o->tx; o->tstk[o->gsp][5] = o->ty;
    o->cstk[o->gsp][0] = o->cl; o->cstk[o->gsp][1] = o->ct; o->cstk[o->gsp][2] = o->cr; o->cstk[o->gsp][3] = o->cbt;
    o->fillstk[o->gsp] = o->fill;
    o->blendstk[o->gsp] = o->blend;
    o->gsp++;
    bmx_save(o);
    if (o->pdf) pw_save(o);
    return 1;
}
int cx_bm_restore(const void *c) {
    cgobj_t *o = as_kind(c, CGK_BM);
    if (!o) return 0;
    if (o->gsp <= 0) return 1;
    if (o->pdf) pw_restore(o);
    o->gsp--;
    o->ca = o->gstk[o->gsp][0]; o->cb = o->gstk[o->gsp][1]; o->cc = o->gstk[o->gsp][2];
    o->cd = o->gstk[o->gsp][3]; o->cu = o->gstk[o->gsp][4]; o->cv = o->gstk[o->gsp][5];
    o->ta = o->tstk[o->gsp][0]; o->tb = o->tstk[o->gsp][1]; o->tc = o->tstk[o->gsp][2];
    o->td = o->tstk[o->gsp][3]; o->tx = o->tstk[o->gsp][4]; o->ty = o->tstk[o->gsp][5];
    o->cl = o->cstk[o->gsp][0]; o->ct = o->cstk[o->gsp][1]; o->cr = o->cstk[o->gsp][2]; o->cbt = o->cstk[o->gsp][3];
    o->fill = o->fillstk[o->gsp];
    o->blend = o->blendstk[o->gsp];
    bmx_restore(o);
    return 1;
}
static void dev_box(cgobj_t *o, double x, double y, double w, double h, int *x0, int *y0, int *x1, int *y1) {
    double xs[4], ys[4];
    int n = 0;
    for (int j = 0; j < 2; j++)
        for (int i = 0; i < 2; i++, n++)
            ctm_of(o, i ? x + w : x, j ? y + h : y, &xs[n], &ys[n]);
    double minx = xs[0], maxx = xs[0], miny = ys[0], maxy = ys[0];
    for (int i = 1; i < 4; i++) {
        if (xs[i] < minx) minx = xs[i];
        if (xs[i] > maxx) maxx = xs[i];
        if (ys[i] < miny) miny = ys[i];
        if (ys[i] > maxy) maxy = ys[i];
    }
    *x0 = (int)floor(minx); *y0 = (int)floor(miny);
    *x1 = (int)ceil(maxx); *y1 = (int)ceil(maxy);
}
static void clamp_box(cgobj_t *o, int *x0, int *y0, int *x1, int *y1) {
    if (*x0 < o->cl) *x0 = o->cl;
    if (*y0 < o->ct) *y0 = o->ct;
    if (*x1 > o->cr) *x1 = o->cr;
    if (*y1 > o->cbt) *y1 = o->cbt;
}
static void fill_poly(cgobj_t *o, const cgr_poly *poly, int eo, u32 solid, int use_solid);
int cx_bm_fill_rect(const void *c, double x, double y, double w, double h) {
    cgobj_t *o = as_kind(c, CGK_BM);
    if (!o || !o->data) return 0;
    int rotated = (o->cb > 1e-12 || o->cb < -1e-12 || o->cc > 1e-12 || o->cc < -1e-12);
    if (o->pdf || rotated) {                    /* PDF, ou CTM girada/inclinada: o retângulo é um polígono */
        cgr_poly p;
        cgr_init(&p);
        double xs[4] = {x, x + w, x + w, x}, ys[4] = {y, y, y + h, y + h};
        for (int i = 0; i < 4; i++) { double dx, dy; ctm_of(o, xs[i], ys[i], &dx, &dy); if (i) cgr_line(&p, dx, dy); else cgr_move(&p, dx, dy); }
        cgr_close(&p);
        fill_poly(o, &p, 0, 0, 0);
        cgr_free(&p);
        return 1;
    }
    int x0, y0, x1, y1;
    dev_box(o, x, y, w, h, &x0, &y0, &x1, &y1);
    clamp_box(o, &x0, &y0, &x1, &y1);
    int sh = shadow_begin(o);
    for (int py = y0; py < y1; py++)
        for (int px = x0; px < x1; px++) blend_at(o, px, py, fill_src(o, px, py));   /* pré-multiplicada, como o blend espera */
    shadow_end(o, sh);
    return 1;
}
int cx_bm_clear_rect(const void *c, double x, double y, double w, double h) {
    cgobj_t *o = as_kind(c, CGK_BM);
    if (!o || !o->data) return 0;
    if (!o->pdf && (o->cb > 1e-12 || o->cb < -1e-12 || o->cc > 1e-12 || o->cc < -1e-12)) {
        u8 b = o->blend;                        /* CTM girada: o polígono com blend clear */
        o->blend = 16;
        cx_bm_fill_rect(c, x, y, w, h);
        o->blend = b;
        return 1;
    }
    if (o->pdf) {                               /* PDF: limpar = papel branco */
        u32 f = o->fill; u8 b = o->blend;
        o->fill = 0xFFFFFFFFu; o->blend = 0;
        double ga = o->gx ? o->gx->galpha : 1;
        if (o->gx) o->gx->galpha = 1;
        cx_bm_fill_rect(c, x, y, w, h);
        if (o->gx) o->gx->galpha = ga;
        o->fill = f; o->blend = b;
        return 1;
    }
    int x0, y0, x1, y1;
    dev_box(o, x, y, w, h, &x0, &y0, &x1, &y1);
    clamp_box(o, &x0, &y0, &x1, &y1);
    for (int py = y0; py < y1; py++) {
        u8 *row = o->data + (size_t)py * o->bpr;
        for (int px = x0; px < x1; px++) ctx_put(o, row + (size_t)px * o->bpp, 0);
    }
    return 1;
}
void CGContextClipToRect(void *c, CGRect rect) {
    cgobj_t *o = as_kind(c, CGK_BM);
    if (!o) return;
    int x0, y0, x1, y1;
    dev_box(o, rect.origin.x, rect.origin.y, rect.size.width, rect.size.height, &x0, &y0, &x1, &y1);
    clamp_box(o, &x0, &y0, &x1, &y1);
    o->cl = x0; o->ct = y0; o->cr = x1; o->cbt = y1;
    if (o->pdf) pw_clip_box(o, x0, y0, x1, y1);
}
/* CGContextResetClip: volta ao bitmap inteiro (o recorte do CGContextClipToRect some). */
void CGContextResetClip(void *c) {
    cgobj_t *o = as_kind(c, CGK_BM);
    if (!o) return;
    o->cl = 0; o->ct = 0; o->cr = (i32)o->w; o->cbt = (i32)o->h;
    if (o->gx) { free(o->gx->mask); o->gx->mask = 0; }
    if (o->pdf) pw_reset_clip(o);
}
/* cópia do conteúdo como CGImage (ARGB pré-multiplicado por dentro); bitmapInfo, bpc e espaço do contexto */
void *CGBitmapContextCreateImage(void *c) {
    cgobj_t *o = as_kind(c, CGK_BM);
    if (!o || !o->data) return 0;
    cgobj_t *im = cg_new(&IM_c, CGK_IM);
    if (!im) return 0;
    im->w = o->w; im->h = o->h; im->bpr = o->w * 4; im->bpp = 4; im->owns = 1; im->info = o->info;
    im->sbpc = o->sbpc; im->sbpp = o->sbpp; im->cb_size = 1; im->shint = 1;
    if (o->space) im->space = objc_retain((id)o->space);
    im->data = malloc((size_t)im->bpr * o->h);
    if (!im->data) { objc_release((id)im); return 0; }
    for (u32 y = 0; y < o->h; y++) {
        const u8 *row = o->data + (size_t)y * o->bpr;
        u32 *out = (u32 *)(im->data + (size_t)y * im->bpr);
        for (u32 x = 0; x < o->w; x++) out[x] = ctx_get(o, row + (size_t)x * o->bpp);
    }
    /* o provedor da imagem tem os bytes do contexto, no layout do contexto (bitmapInfo e bpc acima) */
    u8 *raw = malloc((size_t)o->bpr * o->h);
    if (raw) {
        memcpy(raw, o->data, (size_t)o->bpr * o->h);
        im->cb_info = CGDataProviderCreateWithData(0, raw, (size_t)o->bpr * o->h, cg_free_bytes);
        if (!im->cb_info) free(raw);
    }
    if (o->fmt == PF_A8) im->ismask = 0;
    return im;
}
void *CGImageCreateCopy(void *img);
/* pixel pré-multiplicado da imagem (x, y inteiros, já dentro) */
static u32 img_pm(const cgobj_t *im, int x, int y) {
    u32 p = ((const u32 *)(im->data + (size_t)y * im->bpr))[x];
    if (!im->cb_size) {
        u32 a = p >> 24;
        if (a < 255) p = a << 24 | (((p >> 16) & 255) * a / 255) << 16 | (((p >> 8) & 255) * a / 255) << 8 | ((p & 255) * a / 255);
    }
    return p;
}
static u32 lerp_px(u32 a, u32 b, u32 t256) {
    u32 o = 0;
    for (int s = 0; s < 32; s += 8) {
        u32 x = (a >> s) & 255, y = (b >> s) & 255;
        o |= ((x * (256 - t256) + y * t256) >> 8) << s;
    }
    return o;
}
/* amostra em (u, v) ∈ [0,1]² da imagem (v = 0 embaixo): vizinho mais próximo ou bilinear */
static u32 img_sample(const cgobj_t *im, double u, double v, int bilinear) {
    double fx = u * im->w - 0.5, fy = (1 - v) * im->h - 0.5;
    if (!bilinear) {
        int sx = (int)floor(fx + 0.5), sy = (int)floor(fy + 0.5);
        if (sx < 0) sx = 0;
        if (sy < 0) sy = 0;
        if (sx >= (int)im->w) sx = (int)im->w - 1;
        if (sy >= (int)im->h) sy = (int)im->h - 1;
        return img_pm(im, sx, sy);
    }
    int x0 = (int)floor(fx), y0 = (int)floor(fy);
    u32 tx = (u32)((fx - x0) * 256), ty = (u32)((fy - y0) * 256);
    int x1 = x0 + 1, y1 = y0 + 1;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= (int)im->w) x1 = (int)im->w - 1;
    if (y1 >= (int)im->h) y1 = (int)im->h - 1;
    if (x0 >= (int)im->w) x0 = (int)im->w - 1;
    if (y0 >= (int)im->h) y0 = (int)im->h - 1;
    return lerp_px(lerp_px(img_pm(im, x0, y0), img_pm(im, x1, y0), tx), lerp_px(img_pm(im, x0, y1), img_pm(im, x1, y1), tx), ty);
}
void CGContextDrawImage(void *c, CGRect rect, void *image) {
    cgobj_t *o = as_kind(c, CGK_BM);
    cgobj_t *im = as_kind(image, CGK_IM);
    if (!o || !im || !o->data || !im->data || rect.size.width == 0 || rect.size.height == 0) return;
    double det = o->ca * o->cd - o->cb * o->cc;
    if (det > -1e-12 && det < 1e-12) return;
    if (o->pdf && pw_draw_image(o, rect, im)) return;
    double xs[4], ys[4];
    double rx[2] = {rect.origin.x, rect.origin.x + rect.size.width};
    double ry[2] = {rect.origin.y, rect.origin.y + rect.size.height};
    int n = 0;
    for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++, n++) ctm_of(o, rx[i], ry[j], &xs[n], &ys[n]);
    int x0 = (int)floor(xs[0]), y0 = (int)floor(ys[0]), x1 = x0, y1 = y0;
    for (int i = 1; i < 4; i++) {
        int ix = (int)floor(xs[i]), iy = (int)floor(ys[i]);
        if (ix < x0) x0 = ix;
        if (ix > x1) x1 = ix;
        if (iy < y0) y0 = iy;
        if (iy > y1) y1 = iy;
    }
    if (x0 < o->cl) x0 = o->cl;
    if (y0 < o->ct) y0 = o->ct;
    if (x1 >= o->cr) x1 = o->cr - 1;
    if (y1 >= o->cbt) y1 = o->cbt - 1;
    /* bilinear quando a imagem muda de tamanho ou gira, salvo kCGInterpolationNone ou shouldInterpolate = false */
    double sx = fabs(rect.size.width) * sqrt(o->ca * o->ca + o->cb * o->cb) / im->w;
    double sy = fabs(rect.size.height) * sqrt(o->cc * o->cc + o->cd * o->cd) / im->h;
    int axis = (o->cb > -1e-9 && o->cb < 1e-9 && o->cc > -1e-9 && o->cc < 1e-9);
    int same = axis && sx > 0.999 && sx < 1.001 && sy > 0.999 && sy < 1.001;
    int interp = o->gx ? o->gx->interp : 0;
    int bilinear = !same && interp != 1 && im->shint;
    double ga = o->gx ? o->gx->galpha : 1;
    u32 k = (u32)((ga < 0 ? 0 : ga > 1 ? 1 : ga) * 255 + 0.5);
    int sh = shadow_begin(o);
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            double dx = x + 0.5 - o->cu, dy = y + 0.5 - o->cv;
            double ux = (o->cd * dx - o->cc * dy) / det;
            double uy = (-o->cb * dx + o->ca * dy) / det;
            double u = (ux - rect.origin.x) / rect.size.width;
            double v = (uy - rect.origin.y) / rect.size.height;
            if (u < 0 || u >= 1 || v < 0 || v >= 1) continue;
            u32 p = img_sample(im, u, v, bilinear);
            if (im->ismask) {                        /* máscara de imagem: pinta a cor de preenchimento */
                u32 m = p >> 24;
                if (!m) continue;
                p = scale_px(fill_src(o, x, y), m);
            } else if (k < 255) p = scale_px(p, k);
            blend_at(o, x, y, p);
        }
    }
    shadow_end(o, sh);
}
/* mosaico: a imagem no tamanho de rect, repetida a partir da origem de rect, cobrindo o recorte */
void CGContextDrawTiledImage(void *c, CGRect r, void *image) {
    cgobj_t *o = as_kind(c, CGK_BM);
    if (!o || !as_kind(image, CGK_IM) || r.size.width <= 0 || r.size.height <= 0) return;
    double ia = 0, ib = 0, ic = 0, id_ = 0, itx = 0, ity = 0, det = o->ca * o->cd - o->cb * o->cc;
    if (det > -1e-12 && det < 1e-12) return;
    ia = o->cd / det; ib = -o->cb / det; ic = -o->cc / det; id_ = o->ca / det;
    itx = -(ia * o->cu + ic * o->cv); ity = -(ib * o->cu + id_ * o->cv);
    double ux[4], uy[4];
    double X[4] = {o->cl, o->cr, o->cl, o->cr}, Y[4] = {o->ct, o->ct, o->cbt, o->cbt};
    for (int i = 0; i < 4; i++) { ux[i] = ia * X[i] + ic * Y[i] + itx; uy[i] = ib * X[i] + id_ * Y[i] + ity; }
    double mnx = ux[0], mxx = ux[0], mny = uy[0], mxy = uy[0];
    for (int i = 1; i < 4; i++) {
        if (ux[i] < mnx) mnx = ux[i];
        if (ux[i] > mxx) mxx = ux[i];
        if (uy[i] < mny) mny = uy[i];
        if (uy[i] > mxy) mxy = uy[i];
    }
    long i0 = (long)floor((mnx - r.origin.x) / r.size.width), i1 = (long)ceil((mxx - r.origin.x) / r.size.width);
    long j0 = (long)floor((mny - r.origin.y) / r.size.height), j1 = (long)ceil((mxy - r.origin.y) / r.size.height);
    if ((i1 - i0) * (j1 - j0) > 1000000) return;
    for (long j = j0; j < j1; j++)
        for (long i = i0; i < i1; i++) {
            CGRect t = {{r.origin.x + i * r.size.width, r.origin.y + j * r.size.height}, r.size};
            CGContextDrawImage(c, t, image);
        }
}
u32 CGContextGetInterpolationQuality(void *c) { cgobj_t *o = as_kind(c, CGK_BM); return o && o->gx ? o->gx->interp : 0; }
static cg_bmx_t *bmx(cgobj_t *o);
void CGContextSetInterpolationQuality(void *c, int q) { cg_bmx_t *g = bmx(as_kind(c, CGK_BM)); if (g && q >= 0 && q <= 4) g->interp = (u8)q; }
size_t CGImageGetBitsPerPixel(void *img) { cgobj_t *o = as_kind(img, CGK_IM); return o ? (o->sbpp ? o->sbpp : 32) : 0; }
size_t CGImageGetBitsPerComponent(void *img) { cgobj_t *o = as_kind(img, CGK_IM); return o ? (o->sbpc ? o->sbpc : 8) : 0; }
u32 CGImageGetBitmapInfo(void *img) { cgobj_t *o = as_kind(img, CGK_IM); return o ? o->info : 0; }
u32 CGImageGetAlphaInfo(void *img) { cgobj_t *o = as_kind(img, CGK_IM); return o ? (o->info & 31) : 0; }
void *CGImageGetColorSpace(void *img) {
    cgobj_t *o = as_kind(img, CGK_IM);
    if (!o || o->ismask || (o->info & 31) == 7) return 0;      /* máscara e só-alfa não têm espaço de cor */
    if (o->space) return o->space;
    return CGColorGetColorSpace(img);
}
u64 CGImageGetTypeID(void) { return 0x2A3; }
u64 CGContextGetTypeID(void) { return 0x2A4; }
bool CGImageIsMask(void *img) { cgobj_t *o = as_kind(img, CGK_IM); return o && o->ismask; }
const double *CGImageGetDecode(void *img) { cgobj_t *o = as_kind(img, CGK_IM); return o && o->ndecode ? o->decode : 0; }
bool CGImageGetShouldInterpolate(void *img) { cgobj_t *o = as_kind(img, CGK_IM); return o && o->shint; }
u32 CGImageGetByteOrderInfo(void *img) { cgobj_t *o = as_kind(img, CGK_IM); return o ? (o->info & 0x7000) : 0; }
u32 CGImageGetPixelFormatInfo(void *img) { cgobj_t *o = as_kind(img, CGK_IM); return o ? (o->info & 0xF0000) : 0; }
bool CGImageContainsImageSpecificToneMappingMetadata(void *img) { (void)img; return 0; }   /* sem HDR com metadados */
void *CGBitmapContextGetColorSpace(void *c) {
    cgobj_t *o = as_kind(c, CGK_BM);
    if (!o || o->fmt == PF_A8) return 0;
    if (o->space) return o->space;
    o->space = o->fmt == PF_G8 || o->fmt == PF_GA8 || o->fmt == PF_G16 ? CGColorSpaceCreateDeviceGray() : CGColorSpaceCreateDeviceRGB();
    return o->space;
}
u32 CGBitmapContextGetAlphaInfo(void *c) { cgobj_t *o = as_kind(c, CGK_BM); return o ? (o->info & 31) : 0; }

/* cópia com outro espaço de cor: mesmos pixels, o número de componentes do espaço tem de bater */
/* imagem nova a partir de pixels internos: o provedor e o bitmapInfo passam a descrever esses pixels
 * (ARGB 8 bits em u32, ordem 32 little), com o alfa pré-multiplicado ou reto conforme ficaram */
static void cg_derived_layout(cgobj_t *n) {
    if (n->cb_info) { objc_release((id)n->cb_info); n->cb_info = 0; }
    n->info = (n->cb_size ? AI_PFIRST : AI_FIRST) | BO_32L;
    n->sbpc = 8; n->sbpp = 32; n->ndecode = 0;
}
void *CGImageCreateCopyWithColorSpace(void *img, void *space) {
    cgobj_t *o = as_kind(img, CGK_IM);
    const cgspace_t *s = space;
    if (!o || o->ismask || !s) return 0;
    const cgspace_t *cur = o->space;
    if (cur && cur->ncomp != s->ncomp) return 0;
    cgobj_t *n = CGImageCreateCopy(img);
    if (!n) return 0;
    if (n->space) objc_release((id)n->space);
    n->space = objc_retain((id)space);
    return n;
}
/* máscara: image mask (0 pinta) ou imagem cinza (amostra = alfa invertido: preto mostra a imagem) */
void *CGImageCreateWithMask(void *img, void *mask) {
    cgobj_t *o = as_kind(img, CGK_IM), *m = as_kind(mask, CGK_IM);
    if (!o || !m || o->ismask) return 0;
    cgobj_t *n = CGImageCreateCopy(img);
    if (!n) return 0;
    for (u32 y = 0; y < n->h; y++) {
        u32 *row = (u32 *)(n->data + (size_t)y * n->bpr);
        for (u32 x = 0; x < n->w; x++) {
            /* a máscara se estica para o tamanho da imagem */
            u32 mx = (u32)((u64)x * m->w / n->w), my = (u32)((u64)y * m->h / n->h);
            u32 mp = ((const u32 *)(m->data + (size_t)my * m->bpr))[mx], keep;
            if (m->ismask) keep = mp >> 24;
            else { u32 r = (mp >> 16) & 255, g = (mp >> 8) & 255, b = mp & 255; keep = 255 - ((r * 77 + g * 150 + b * 29 + 128) >> 8); }
            u32 p = row[x], a = p >> 24;
            if (n->cb_size) row[x] = scale_px(p, keep);
            else row[x] = (a * keep / 255) << 24 | (p & 0xFFFFFF);
        }
    }
    cg_derived_layout(n);
    return n;
}
/* cores mascaradas: pixel com todos os componentes dentro de [min, max] vira transparente (só imagens sem alfa) */
void *CGImageCreateWithMaskingColors(void *img, const double *comps) {
    cgobj_t *o = as_kind(img, CGK_IM);
    if (!o || !comps || o->ismask || ai_has_alpha(o->info & 31)) return 0;
    const cgspace_t *s = o->space;
    int nc = s ? (int)s->ncomp : 3, maxv = o->sbpc && o->sbpc < 16 ? (1 << o->sbpc) - 1 : 255;
    cgobj_t *n = CGImageCreateCopy(img);
    if (!n) return 0;
    for (u32 y = 0; y < n->h; y++) {
        u32 *row = (u32 *)(n->data + (size_t)y * n->bpr);
        for (u32 x = 0; x < n->w; x++) {
            u32 p = row[x];
            int v[3] = {(int)((p >> 16) & 255), (int)((p >> 8) & 255), (int)(p & 255)}, in = 1;
            for (int k = 0; k < nc && k < 3; k++) {
                double val = (double)v[nc == 1 ? 0 : k] * maxv / 255;
                if (val < comps[2 * k] - 0.5 || val > comps[2 * k + 1] + 0.5) { in = 0; break; }
            }
            if (in) row[x] = 0;
        }
    }
    n->cb_size = 1;
    cg_derived_layout(n);
    return n;
}
/* PNG e JPEG por provedor: o decodificador do ImageIO (imageio.c) */
typedef struct { int w, h, has_alpha; u32 *px; } cg_moscodec_t;
int mos_img_decode(const u8 *data, size_t n, cg_moscodec_t *out);
static void cg_free_px(void *info, const void *data, size_t size) { (void)info; (void)size; free((void *)data); }
static void *image_from_codec(void *provider, const double *decode, bool interp, int intent) {
    cgobj_t *p = as_kind(provider, CGK_PR);
    cg_moscodec_t m;
    if (!p || mos_img_decode(p->data, (size_t)p->cb_size, &m)) return 0;
    void *pr = CGDataProviderCreateWithData(0, m.px, (size_t)m.w * m.h * 4, cg_free_px);
    void *img = pr ? image_from((size_t)m.w, (size_t)m.h, 8, 32, (size_t)m.w * 4, 0, (m.has_alpha ? AI_FIRST : AI_SKIPFIRST) | BO_32L,
                                as_kind(pr, CGK_PR), decode, 0, intent, interp) : 0;
    if (pr) CGDataProviderRelease(pr);
    else free(m.px);
    return img;
}
void *CGImageCreateWithPNGDataProvider(void *provider, const double *decode, bool interp, int intent) { return image_from_codec(provider, decode, interp, intent); }
void *CGImageCreateWithJPEGDataProvider(void *provider, const double *decode, bool interp, int intent) { return image_from_codec(provider, decode, interp, intent); }
/* tipo de origem, posto pelo ImageIO */
void cx_cgimage_set_uti(void *img, const void *uti) { cgobj_t *o = as_kind(img, CGK_IM); if (o) o->uti = uti; }
void *CGImageGetDataProvider(void *img) {
    cgobj_t *o = as_kind(img, CGK_IM);
    if (!o || !o->data) return 0;
    if (!o->cb_info) o->cb_info = CGDataProviderCreateWithData(0, o->data, (size_t)o->bpr * o->h, 0);
    return o->cb_info;
}
void *CFDataCreate(void *alloc, const u8 *bytes, long n);
/* kCGErrorSuccess = 0. O renderer chama isto dentro de DisableSystemServices e faz CHECK_EQ com 0. */
int CGSSetDenyWindowServerConnections(int deny) { (void)deny; return 0; }
/* 0 = suavização ligada. RenderThreadImpl usa isto para decidir LCD text; 0 é o default do macOS. */
int CGFontRenderingGetFontSmoothingDisabled(void) { return 0; }

void *CGDataProviderCopyData(void *p) {
    cgobj_t *o = as_kind(p, CGK_PR);
    if (!o || !o->data) return 0;
    return CFDataCreate(0, o->data, (long)o->cb_size);
}

#include "coregraphics_bm.inc"
#include "cg_more.inc"
#include "coregraphics_pdf.inc"
static void pw_dealloc(cgobj_t *o) { pw_close(o); pw_free(o->pdf); o->pdf = 0; }
