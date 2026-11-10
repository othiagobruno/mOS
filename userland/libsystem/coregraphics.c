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
typedef struct { cx_class_t *isa; u64 cfinfo; u8 *types; CGPoint *pts; u64 n, cap, npts, ptcap; CGPoint start, cur; } cgpath_t;

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
void *CGPathCreateWithRect(CGRect r, const CGAffineTransform *m) { void *p = CGPathCreateMutable(); CGPathAddRect(p, m, r); return p; }
void *CGPathCreateWithEllipseInRect(CGRect r, const CGAffineTransform *m) { void *p = CGPathCreateMutable(); CGPathAddEllipseInRect(p, m, r); return p; }
void *CGPathCreateWithRoundedRect(CGRect r, CGFloat cw, CGFloat ch, const CGAffineTransform *m) { void *p = CGPathCreateMutable(); CGPathAddRoundedRect(p, m, r, cw, ch); return p; }

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

typedef struct { cx_class_t *isa; u64 cfinfo; const void *name; i32 model; u32 ncomp; u32 extended; } cgspace_t;
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
    int gray = strstr(n, "Gray") != 0;
    s->name = name;
    s->model = gray ? 0 : 1;
    s->ncomp = gray ? 1 : 3;
    s->extended = strstr(n, "Extended") != 0 || strstr(n, "ITUR_2100") != 0;
    objc_retain((id)s);                        /* a tabela mantém uma referência própria */
    spaces[nspaces++] = s;
    return s;
}
void *CGColorSpaceCreateDeviceRGB(void) { return CGColorSpaceCreateWithName(kCGColorSpaceSRGB); }
void *CGColorSpaceCreateDeviceGray(void) { return CGColorSpaceCreateWithName(kCGColorSpaceGenericGrayGamma2_2); }
/* espaços calibrados: o mOS não faz gerenciamento de cor; aproximados por sRGB / cinza gamma 2.2 */
void *CGColorSpaceCreateCalibratedRGB(const CGFloat *white, const CGFloat *black, const CGFloat *gamma, const CGFloat *matrix) {
    (void)white; (void)black; (void)gamma; (void)matrix;
    return CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
}
void *CGColorSpaceCreateCalibratedGray(const CGFloat *white, const CGFloat *black, CGFloat gamma) {
    (void)white; (void)black; (void)gamma;
    return CGColorSpaceCreateWithName(kCGColorSpaceGenericGrayGamma2_2);
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
void *CGColorCreate(const void *space, const CGFloat *c) {
    if (!c) return 0;
    if (space && ((const cgspace_t *)space)->ncomp == 1) return cx_color(c[0], c[0], c[0], c[1]);
    return cx_color(c[0], c[1], c[2], c[3]);
}
void *CGColorCreateSRGB(CGFloat r, CGFloat g, CGFloat b, CGFloat a) { return cx_color(r, g, b, a); }
void *CGColorCreateCopy(const void *c) { const cgcolor_t *k = c; return k ? cx_color(k->r, k->g, k->b, k->a) : 0; }
void *CGColorCreateCopyWithAlpha(const void *c, CGFloat a) { const cgcolor_t *k = c; return k ? cx_color(k->r, k->g, k->b, a) : 0; }
bool CGColorEqualToColor(const void *x, const void *y) {
    const cgcolor_t *a = x, *b = y;
    if (a == b) return true;
    return a && b && a->r == b->r && a->g == b->g && a->b == b->b && a->a == b->a;
}
size_t CGColorGetNumberOfComponents(const void *c) { return c ? 4 : 0; }
const CGFloat *CGColorGetComponents(const void *c) { return c ? &((const cgcolor_t *)c)->r : 0; }
CGFloat CGColorGetAlpha(const void *c) { return c ? ((const cgcolor_t *)c)->a : 0; }
void *CGColorGetColorSpace(const void *c) {
    static void *srgb;
    if (!c) return 0;
    if (!srgb) srgb = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);   /* referência mantida: "Get" não transfere posse */
    return srgb;
}

/* ======================================================================== bitmap, CGImage, data provider
 * O Skia do Chromium pede um CGBitmapContext, desenha com CTFontDrawGlyphs e lê os pixels.
 * CGImage vira contents de CALayer (quartzcore.c pergunta por cx_cgimage_src). */
#define CGK_BM 0xC6B10001u
#define CGK_IM 0xC6B10002u
#define CGK_PR 0xC6B10003u
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
} cgobj_t;
static cx_class_t BM_c, BM_m, IM_c, IM_m, PR_c, PR_m;
static int cg_ready;
extern void cx_object_free(id o);
extern u32 cx_cg_fill_argb(void);
static void cg_dealloc(id self, SEL c) {
    (void)c;
    cgobj_t *o = (cgobj_t *)self;
    if (o->kind == CGK_PR && o->rel_cb) o->rel_cb(o->cb_info, o->data, (size_t)o->cb_size);
    if (o->kind == CGK_IM && o->cb_info) objc_release((id)o->cb_info);
    if (o->owns) free(o->data);
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
void *CGDataProviderRetain(void *p) { if (as_kind(p, CGK_PR)) objc_retain((id)p); return p; }

static u32 px_argb(const u8 *s, u32 info) {
    u32 ainfo = info & 31;
    int first = ainfo == 2 || ainfo == 4 || ainfo == 6;
    u8 A, R, G, B;
    if (first) { B = s[0]; G = s[1]; R = s[2]; A = s[3]; }
    else { R = s[0]; G = s[1]; B = s[2]; A = s[3]; }
    if (ainfo == 0 || ainfo == 5 || ainfo == 6) A = 255;
    return (u32)A << 24 | (u32)R << 16 | (u32)G << 8 | B;
}
void *CGImageCreate(size_t w, size_t h, size_t bpc, size_t bpp, size_t bpr, const void *space, u32 info,
                    void *provider, const void *decode, bool shouldInterpolate, int intent) {
    (void)space; (void)decode; (void)shouldInterpolate; (void)intent;
    cgobj_t *p = as_kind(provider, CGK_PR);
    if (!p || !w || !h || bpc != 8 || (bpp != 32 && bpp != 8)) return 0;
    if ((u64)w * h * 4 > (32u << 20)) return 0;
    if (!bpr) bpr = w * (bpp / 8);
    if (!h || bpr > p->cb_size / h) return 0;
    cgobj_t *o = cg_new(&IM_c, CGK_IM);
    if (!o) return 0;
    o->w = (u32)w; o->h = (u32)h; o->bpr = (u32)w * 4; o->bpp = 4; o->info = info; o->owns = 1;
    o->data = malloc((size_t)o->bpr * h);
    if (!o->data) { objc_release((id)o); return 0; }
    u32 ainfo = info & 31;
    o->owns = 1;
    int prem = ainfo == 1 || ainfo == 2;
    /* guarda AARRGGBB. Já pré-multiplicado fica marcado para o compositor não multiplicar de novo. */
    o->cb_size = prem ? 1 : 0;
    for (size_t y = 0; y < h; y++) {
        u32 *row = (u32 *)(o->data + y * o->bpr);
        const u8 *src = p->data + y * bpr;
        if (bpp == 8) {
            for (size_t x = 0; x < w; x++) { u8 v = src[x]; row[x] = 0xFF000000u | (u32)v << 16 | (u32)v << 8 | v; }
        } else {
            for (size_t x = 0; x < w; x++) row[x] = px_argb(src + x * 4, info);
        }
    }
    return o;
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
    (void)space;
    if (!w || !h || bpc != 8) return 0;
    u32 ainfo = bitmapInfo & 31;
    u32 bpp = ainfo == 7 ? 1 : 4;
    if (!bpr) bpr = w * bpp;
    if (bpr < w * bpp) return 0;
    if ((u64)bpr * h > (32u << 20)) return 0;
    int owns = 0;
    if (!data) {
        data = malloc(bpr * h);
        if (!data) return 0;
        memset(data, 0, bpr * h);
        owns = 1;
    }
    cgobj_t *o = cg_new(&BM_c, CGK_BM);
    if (!o) { if (owns) free(data); return 0; }
    o->data = data; o->w = (u32)w; o->h = (u32)h; o->bpr = (u32)bpr; o->bpp = bpp; o->info = bitmapInfo; o->owns = owns;
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
size_t CGBitmapContextGetBitsPerComponent(void *c) { return as_kind(c, CGK_BM) ? 8 : 0; }
u32 CGBitmapContextGetBitmapInfo(void *c) { cgobj_t *o = as_kind(c, CGK_BM); return o ? o->info : 0; }
void CGContextSetShouldSmoothFonts(void *c, int v) { (void)c; (void)v; }
void CGContextSetAllowsFontSmoothing(void *c, int v) { (void)c; (void)v; }
void CGContextSetShouldSubpixelPositionFonts(void *c, int v) { (void)c; (void)v; }
void CGContextSetAllowsFontSubpixelPositioning(void *c, int v) { (void)c; (void)v; }
void CGContextSetShouldSubpixelQuantizeFonts(void *c, int v) { (void)c; (void)v; }
void CGContextSetAllowsFontSubpixelQuantization(void *c, int v) { (void)c; (void)v; }
void CGContextSetTextDrawingMode(void *c, int mode) { (void)c; (void)mode; }
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
    if (o) { o->blend = (u8)mode; return; }
    cx_cg_note_blend(mode);
}
static void store_px(u8 *dp, u32 info, u32 argb) {
    u32 ainfo = info & 31;
    int first = ainfo == 0 || ainfo == 2 || ainfo == 4 || ainfo == 6;
    u32 A = argb >> 24, R = (argb >> 16) & 255, G = (argb >> 8) & 255, B = argb & 255;
    if (first) { dp[0] = (u8)B; dp[1] = (u8)G; dp[2] = (u8)R; dp[3] = (u8)A; }
    else { dp[0] = (u8)R; dp[1] = (u8)G; dp[2] = (u8)B; dp[3] = (u8)A; }
}
static void blend_at(cgobj_t *o, int x, int y, u32 src) {
    int mode = o->blend;
    if (x < o->cl || y < o->ct || x >= o->cr || y >= o->cbt) return;
    if (mode != 1 && mode != 16 && !(src >> 24)) return;
    u8 *dp = o->data + (size_t)y * o->bpr + (size_t)x * o->bpp;
    if (o->bpp == 1) {
        if (mode == 16) dp[0] = 0;
        else if (mode == 1) dp[0] = (u8)(src >> 24);
        else if ((src >> 24) > dp[0]) dp[0] = (u8)(src >> 24);
        return;
    }
    u32 dst = px_argb(dp, o->info);
    u32 ainfo = o->info & 31;
    if (mode != 1 && mode != 16 && ainfo != 1 && ainfo != 2) {
        u32 a = dst >> 24;
        if (a && a < 255) dst = a << 24 | (((dst >> 16) & 255) * a / 255) << 16 | (((dst >> 8) & 255) * a / 255) << 8 | ((dst & 255) * a / 255);
    }
    store_px(dp, o->info, cx_cg_blend_px(mode, src, dst));
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
    /* itálico sintético: o Skia (SkScalerContext_Mac) põe a inclinação na matriz de texto (c = 0,25 para o
     * font-style: italic de uma fonte sem face itálica). Cada linha do glifo anda c/d px por px de altura acima
     * da linha de base, com a cobertura dividida entre os dois pixels vizinhos. */
    double shear = (o->td > 1e-6 || o->td < -1e-6) ? o->tc / o->td : 0;
    if (shear > 1 || shear < -1) shear = 0;
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
    return 1;
}
int cx_bm_restore(const void *c) {
    cgobj_t *o = as_kind(c, CGK_BM);
    if (!o) return 0;
    if (o->gsp <= 0) return 1;
    o->gsp--;
    o->ca = o->gstk[o->gsp][0]; o->cb = o->gstk[o->gsp][1]; o->cc = o->gstk[o->gsp][2];
    o->cd = o->gstk[o->gsp][3]; o->cu = o->gstk[o->gsp][4]; o->cv = o->gstk[o->gsp][5];
    o->ta = o->tstk[o->gsp][0]; o->tb = o->tstk[o->gsp][1]; o->tc = o->tstk[o->gsp][2];
    o->td = o->tstk[o->gsp][3]; o->tx = o->tstk[o->gsp][4]; o->ty = o->tstk[o->gsp][5];
    o->cl = o->cstk[o->gsp][0]; o->ct = o->cstk[o->gsp][1]; o->cr = o->cstk[o->gsp][2]; o->cbt = o->cstk[o->gsp][3];
    o->fill = o->fillstk[o->gsp];
    o->blend = o->blendstk[o->gsp];
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
int cx_bm_fill_rect(const void *c, double x, double y, double w, double h) {
    cgobj_t *o = as_kind(c, CGK_BM);
    if (!o || !o->data) return 0;
    int x0, y0, x1, y1;
    dev_box(o, x, y, w, h, &x0, &y0, &x1, &y1);
    clamp_box(o, &x0, &y0, &x1, &y1);
    u32 fill = o->fill ? o->fill : 0xFF000000u;
    for (int py = y0; py < y1; py++)
        for (int px = x0; px < x1; px++) blend_at(o, px, py, fill);
    return 1;
}
int cx_bm_clear_rect(const void *c, double x, double y, double w, double h) {
    cgobj_t *o = as_kind(c, CGK_BM);
    if (!o || !o->data) return 0;
    int x0, y0, x1, y1;
    dev_box(o, x, y, w, h, &x0, &y0, &x1, &y1);
    clamp_box(o, &x0, &y0, &x1, &y1);
    for (int py = y0; py < y1; py++) {
        u8 *row = o->data + (size_t)py * o->bpr;
        for (int px = x0; px < x1; px++) {
            u8 *dp = row + (size_t)px * o->bpp;
            for (u32 i = 0; i < o->bpp; i++) dp[i] = 0;
        }
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
}
void *CGBitmapContextCreateImage(void *c) {
    cgobj_t *o = as_kind(c, CGK_BM);
    if (!o || !o->data) return 0;
    void *p = CGDataProviderCreateWithData(0, o->data, (size_t)o->bpr * o->h, 0);
    void *img = CGImageCreate(o->w, o->h, 8, o->bpp * 8, o->bpr, 0, o->info, p, 0, false, 0);
    CGDataProviderRelease(p);
    return img;
}
void CGContextDrawImage(void *c, CGRect rect, void *image) {
    cgobj_t *o = as_kind(c, CGK_BM);
    cgobj_t *im = as_kind(image, CGK_IM);
    if (!o || !im || !o->data || !im->data || rect.size.width == 0 || rect.size.height == 0) return;
    double det = o->ca * o->cd - o->cb * o->cc;
    if (det > -1e-12 && det < 1e-12) return;
    double xs[4], ys[4];
    double rx[2] = {rect.origin.x, rect.origin.x + rect.size.width};
    double ry[2] = {rect.origin.y, rect.origin.y + rect.size.height};
    int n = 0;
    for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++, n++) ctm_of(o, rx[i], ry[j], &xs[n], &ys[n]);
    int x0 = (int)floor(xs[0]), y0 = (int)floor(ys[0]), x1 = x0, y1 = y0;
    for (int i = 1; i < 4; i++) {
        int ix = (int)floor(xs[i]), iy = (int)floor(ys[i]);
        if (ix < x0) x0 = ix; if (ix > x1) x1 = ix; if (iy < y0) y0 = iy; if (iy > y1) y1 = iy;
    }
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
    if (x1 >= (int)o->w) x1 = (int)o->w - 1; if (y1 >= (int)o->h) y1 = (int)o->h - 1;
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            double dx = x + 0.5 - o->cu, dy = y + 0.5 - o->cv;
            double ux = (o->cd * dx - o->cc * dy) / det;
            double uy = (-o->cb * dx + o->ca * dy) / det;
            double u = (ux - rect.origin.x) / rect.size.width;
            double v = (uy - rect.origin.y) / rect.size.height;
            if (u < 0 || u > 1 || v < 0 || v > 1) continue;
            int sx = (int)(u * (im->w - 1) + 0.5), sy = (int)((1 - v) * (im->h - 1) + 0.5);
            if (sx < 0) sx = 0; if (sy < 0) sy = 0;
            if (sx >= (int)im->w) sx = (int)im->w - 1; if (sy >= (int)im->h) sy = (int)im->h - 1;
            u32 p = ((u32 *)(im->data + (size_t)sy * im->bpr))[sx];
            if (!im->cb_size) {
                u32 a = p >> 24;
                if (a < 255) p = a << 24 | (((p >> 16) & 255) * a / 255) << 16 | (((p >> 8) & 255) * a / 255) << 8 | ((p & 255) * a / 255);
            }
            blend_at(o, x, y, p);
        }
    }
}
size_t CGImageGetBitsPerPixel(void *img) { return as_kind(img, CGK_IM) ? 32 : 0; }
size_t CGImageGetBitsPerComponent(void *img) { return as_kind(img, CGK_IM) ? 8 : 0; }
u32 CGImageGetBitmapInfo(void *img) { cgobj_t *o = as_kind(img, CGK_IM); return o ? o->info : 0; }
u32 CGImageGetAlphaInfo(void *img) { cgobj_t *o = as_kind(img, CGK_IM); return o ? (o->info & 31) : 0; }
void *CGImageGetColorSpace(void *img) {
    if (!as_kind(img, CGK_IM)) return 0;
    return CGColorGetColorSpace(img);
}
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
