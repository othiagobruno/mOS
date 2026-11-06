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
 */
#include <stdbool.h>
#include "foundation.h"
#include "libc.h"

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
