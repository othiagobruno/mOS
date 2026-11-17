/*
 * mOS libSystem — HIShape (HIToolbox/ApplicationServices): regiões imutáveis e mutáveis como lista de retângulos
 * disjuntos (carbon_core.c, teste userland/tests-appkit/carbon_host.c). HIShapeRef é um objeto CF (CFRetain/CFRelease
 * funcionam): uma instância de _MOSHIShape que guarda a região.
 */
#include "foundation.h"
#include "sys.h"
#include "libc.h"
#include "carbon_core.h"

typedef int OSStatus;
typedef struct { double x, y, w, h; } R;
typedef struct { double x, y; } P;
id objc_retain(id o);
IMP class_getMethodImplementation(cx_class_t *c, SEL s);
id objc_autorelease(id o);
void objc_setAssociatedObject(id obj, const void *key, id value, u64 policy);
id objc_getAssociatedObject(id obj, const void *key);
void CGContextBeginPath(void *c);
void CGContextAddRect(void *c, R r);

#define K(N) cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N); cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N);
K(_MOSHIShape)

/* a região mora num bloco de memória guardado como NSMutableData associado ao objeto */
static char k_shape;
static cc_shape *sh(id s) {
    if (!s) return 0;
    id d = objc_getAssociatedObject(s, &k_shape);
    return d ? MSG0(cc_shape *, d, "mutableBytes") : 0;
}
static void sh_dealloc_rects(id self, SEL c) {
    cc_shape *s = sh(self);
    if (s) { cc_free(s->r); s->r = 0; }
    IMP sup = class_getMethodImplementation(cx_class_named("NSObject"), c);
    ((void (*)(id, SEL))sup)(self, c);
}
static id sh_new(int mutable_) {
    id s = cx_alloc_instance(&_MOSHIShape_c);
    id d = MSG(id, (id)cx_class_named("NSMutableData"), "dataWithLength:", (u64)sizeof(cc_shape) + 8);
    objc_setAssociatedObject(s, &k_shape, d, 1);
    ((u8 *)MSG0(void *, d, "mutableBytes"))[sizeof(cc_shape)] = (u8)mutable_;
    return s;
}
static cc_rect cr(R r) {
    cc_rect o = {r.x, r.y, r.w, r.h};
    if (o.w < 0) { o.x += o.w; o.w = -o.w; }   /* CGRect não padronizado */
    if (o.h < 0) { o.y += o.h; o.h = -o.h; }
    return o;
}
id HIShapeCreateEmpty(void) { return sh_new(0); }
id HIShapeCreateWithRect(const R *r) { id s = sh_new(0); if (r) cc_shape_set_rect(sh(s), cr(*r)); return s; }
id HIShapeCreateCopy(id src) { id s = sh_new(0); if (src) cc_shape_copy(sh(s), sh(src)); return s; }
id HIShapeCreateMutable(void) { return sh_new(1); }
id HIShapeCreateMutableCopy(id src) { id s = sh_new(1); if (src) cc_shape_copy(sh(s), sh(src)); return s; }
id HIShapeCreateMutableWithRect(const R *r) { id s = sh_new(1); if (r) cc_shape_set_rect(sh(s), cr(*r)); return s; }
id HIShapeCreateIntersection(id a, id b) { id s = sh_new(0); cc_shape_intersect(sh(s), sh(a), sh(b)); return s; }
id HIShapeCreateDifference(id a, id b) { id s = sh_new(0); cc_shape_diff(sh(s), sh(a), sh(b)); return s; }
id HIShapeCreateUnion(id a, id b) { id s = sh_new(0); cc_shape_union(sh(s), sh(a), sh(b)); return s; }
id HIShapeCreateXor(id a, id b) { id s = sh_new(0); cc_shape_xor(sh(s), sh(a), sh(b)); return s; }
OSStatus HIShapeUnion(id a, id b, id out) { return out && a && b ? (cc_shape_union(sh(out), sh(a), sh(b)) ? -108 : 0) : -50; }
OSStatus HIShapeIntersect(id a, id b, id out) { return out && a && b ? (cc_shape_intersect(sh(out), sh(a), sh(b)) ? -108 : 0) : -50; }
OSStatus HIShapeDifference(id a, id b, id out) { return out && a && b ? (cc_shape_diff(sh(out), sh(a), sh(b)) ? -108 : 0) : -50; }
OSStatus HIShapeXor(id a, id b, id out) { return out && a && b ? (cc_shape_xor(sh(out), sh(a), sh(b)) ? -108 : 0) : -50; }
OSStatus HIShapeSetEmpty(id s) { if (!s) return -50; cc_shape_clear(sh(s)); return 0; }
OSStatus HIShapeSetWithShape(id dst, id src) { if (!dst || !src) return -50; return cc_shape_copy(sh(dst), sh(src)) ? -108 : 0; }
OSStatus HIShapeUnionWithRect(id s, const R *r) {
    if (!s || !r) return -50;
    cc_shape one = {0, 0, 0};
    cc_shape_set_rect(&one, cr(*r));
    int rc = cc_shape_union(sh(s), sh(s), &one);
    cc_free(one.r);
    return rc ? -108 : 0;
}
OSStatus HIShapeIntersectWithRect(id s, const R *r) {
    if (!s || !r) return -50;
    cc_shape one = {0, 0, 0};
    cc_shape_set_rect(&one, cr(*r));
    int rc = cc_shape_intersect(sh(s), sh(s), &one);
    cc_free(one.r);
    return rc ? -108 : 0;
}
OSStatus HIShapeOffset(id s, double dx, double dy) { if (!s) return -50; cc_shape_offset(sh(s), dx, dy); return 0; }
OSStatus HIShapeInset(id s, double dx, double dy) { if (!s) return -50; return cc_shape_inset(sh(s), dx, dy) ? -108 : 0; }
u8 HIShapeIsEmpty(id s) { return !s || cc_shape_empty(sh(s)); }
u8 HIShapeIsRectangular(id s) { return !s || cc_shape_rectangular(sh(s)); }
u8 HIShapeContainsPoint(id s, const P *p) { return s && p && cc_shape_contains(sh(s), p->x, p->y); }
u8 HIShapeIntersectsRect(id s, const R *r) { return s && r && cc_shape_intersects_rect(sh(s), cr(*r)); }
R *HIShapeGetBounds(id s, R *out) {
    if (!out) return 0;
    cc_rect b = s ? cc_shape_bounds(sh(s)) : (cc_rect){0, 0, 0, 0};
    out->x = b.x; out->y = b.y; out->w = b.w; out->h = b.h;
    return out;
}
OSStatus HIShapeReplacePathInCGContext(id s, void *ctx) {
    if (!s || !ctx) return -50;
    CGContextBeginPath(ctx);
    cc_shape *p = sh(s);
    for (int i = 0; i < p->n; i++) { R r = {p->r[i].x, p->r[i].y, p->r[i].w, p->r[i].h}; CGContextAddRect(ctx, r); }
    return 0;
}
/* HIShapeEnumerate: kHIShapeEnumerateInit 1, Rect 2, Terminate 3; ordem de cima para baixo (ou de baixo, com
 * kHIShapeParseFromBottom 1) e da esquerda para a direita (ou da direita, kHIShapeParseFromRight 2) */
typedef OSStatus (*enum_fn)(int msg, id shape, const R *rect, void *ref);
OSStatus HIShapeEnumerate(id s, u32 opts, enum_fn fn, void *ref) {
    if (!s || !fn) return -50;
    cc_shape *p = sh(s);
    int n = p->n;
    int *ord = calloc(n ? (u64)n : 1, sizeof *ord);
    if (!ord) return -108;
    for (int i = 0; i < n; i++) ord[i] = i;
    int from_bottom = opts & 1, from_right = (opts & 2) != 0;
    for (int i = 1; i < n; i++)   /* inserção: poucas regiões */
        for (int j = i; j > 0; j--) {
            cc_rect a = p->r[ord[j - 1]], b = p->r[ord[j]];
            int before = from_bottom ? (b.y > a.y) : (b.y < a.y);
            if (b.y == a.y) before = from_right ? (b.x > a.x) : (b.x < a.x);
            if (!before) break;
            int t = ord[j]; ord[j] = ord[j - 1]; ord[j - 1] = t;
        }
    OSStatus rc = fn(1, s, 0, ref);
    for (int i = 0; !rc && i < n; i++) { R r = {p->r[ord[i]].x, p->r[ord[i]].y, p->r[ord[i]].w, p->r[ord[i]].h}; rc = fn(2, s, &r, ref); }
    fn(3, s, 0, ref);
    free(ord);
    return rc;
}
u64 HIShapeGetTypeID(void) { return 0x4853; }

void cx_carbon_shape_init(void) {
    if (cx_class_named("_MOSHIShape")) return;
    cx_define_class(&_MOSHIShape_c, &_MOSHIShape_m, "_MOSHIShape", cx_class_named("NSObject"), 0);
    cx_add_method(&_MOSHIShape_c, "dealloc", (void *)sh_dealloc_rects);
}
