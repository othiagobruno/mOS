/*
 * mOS — CAConstraint e CAConstraintLayoutManager (QuartzCore): layout de camadas por restrições.
 *
 * Cada CAConstraint diz "o atributo A desta camada = atributo B da camada `sourceName` × scale + offset"
 * (kCAConstraintMinX 0, MidX 1, MaxX 2, Width 3, MinY 4, MidY 5, MaxY 6, Height 7). A camada guarda as dela em
 * `constraints` (quartzcore.c, objeto associado). O CAConstraintLayoutManager, posto em `layoutManager` da camada
 * mãe, é chamado no layoutSublayers/layoutIfNeeded: para cada subcamada, resolve as restrições de cada eixo e muda o
 * frame. A fonte "superlayer" é a camada mãe (nos bounds dela); outro nome é a irmã com esse `name`, já posicionada.
 * Dois atributos no mesmo eixo definem posição e tamanho; um só move a camada mantendo o tamanho.
 */
#include "foundation.h"
#include "libc.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
IMP class_getMethodImplementation(cx_class_t *c, SEL s);

void objc_setAssociatedObject(id o, const void *key, id value, int policy);
id objc_getAssociatedObject(id o, const void *key);
void objc_msgSend_stret(void);
typedef struct { double x, y; } cc_p;
typedef struct { double w, h; } cc_s;
typedef struct { cc_p o; cc_s s; } cc_r;
#define MSGR(T, obj, sel) ((T (*)(id, SEL))objc_msgSend_stret)((id)(obj), sel_registerName(sel))

cx_class_t CAConstraint_c __asm__("OBJC_CLASS_$_CAConstraint"), CAConstraint_m __asm__("OBJC_METACLASS_$_CAConstraint");
cx_class_t CAConstraintLayoutManager_c __asm__("OBJC_CLASS_$_CAConstraintLayoutManager"),
    CAConstraintLayoutManager_m __asm__("OBJC_METACLASS_$_CAConstraintLayoutManager");

typedef struct { cx_class_t *isa; int attr, srcattr; id src; double scale, offset; } cst_t;

static id cst_init5(id s, SEL c, int attr, id src, int srcattr, double scale, double offset) {
    (void)c;
    cst_t *k = (cst_t *)s;
    k->attr = attr; k->srcattr = srcattr; k->scale = scale; k->offset = offset;
    k->src = src ? objc_retain(MSG0(id, src, "copy")) : 0;
    return s;
}
static id cst_new5(id k, SEL c, int attr, id src, int srcattr, double scale, double offset) {
    (void)k;
    return objc_autorelease(cst_init5(cx_alloc_instance(&CAConstraint_c), c, attr, src, srcattr, scale, offset));
}
static id cst_new4(id k, SEL c, int attr, id src, int srcattr, double offset) { return cst_new5(k, c, attr, src, srcattr, 1.0, offset); }
static id cst_new3(id k, SEL c, int attr, id src, int srcattr) { return cst_new5(k, c, attr, src, srcattr, 1.0, 0.0); }
static int cst_attr(id s, SEL c) { (void)c; return ((cst_t *)s)->attr; }
static int cst_srcattr(id s, SEL c) { (void)c; return ((cst_t *)s)->srcattr; }
static id cst_src(id s, SEL c) { (void)c; return ((cst_t *)s)->src; }
static double cst_scale(id s, SEL c) { (void)c; return ((cst_t *)s)->scale; }
static double cst_offset(id s, SEL c) { (void)c; return ((cst_t *)s)->offset; }
static void cst_dealloc(id s, SEL c) {
    (void)c;
    if (((cst_t *)s)->src) objc_release(((cst_t *)s)->src);
    cx_class_t *ob = cx_class_named("NSObject");
    IMP d = class_getMethodImplementation(ob, sel_registerName("dealloc"));
    if (d) ((void (*)(id, SEL))d)(s, sel_registerName("dealloc"));
}

static double attr_of(cc_r r, int a) {
    switch (a) {
    case 0: return r.o.x;
    case 1: return r.o.x + r.s.w / 2;
    case 2: return r.o.x + r.s.w;
    case 3: return r.s.w;
    case 4: return r.o.y;
    case 5: return r.o.y + r.s.h / 2;
    case 6: return r.o.y + r.s.h;
    case 7: return r.s.h;
    }
    return 0;
}
/* resolve um eixo: has[i]/v[i] para min, mid, max, tamanho */
static void solve(double *pos, double *len, const int has[4], const double v[4]) {
    double p = *pos, l = *len;
    if (has[3]) l = v[3];
    if (has[0] && has[2] && !has[3]) { p = v[0]; l = v[2] - v[0]; }
    else if (has[0] && has[1] && !has[3]) { p = v[0]; l = 2 * (v[1] - v[0]); }
    else if (has[1] && has[2] && !has[3]) { l = 2 * (v[2] - v[1]); p = v[2] - l; }
    else if (has[0]) p = v[0];
    else if (has[1]) p = v[1] - l / 2;
    else if (has[2]) p = v[2] - l;
    if (l < 0) l = 0;
    *pos = p;
    *len = l;
}
static id find_sibling(id subs, id name) {
    long n = subs ? MSG0(long, subs, "count") : 0;
    for (long i = 0; i < n; i++) {
        id l = MSG(id, subs, "objectAtIndex:", (unsigned long)i);
        id ln = MSG0(id, l, "name");
        if (ln && MSG(BOOL, ln, "isEqualToString:", name)) return l;
    }
    return 0;
}
static void lm_layout(id self, SEL c, id layer) {
    (void)self; (void)c;
    if (!layer) return;
    id subs = MSG0(id, layer, "sublayers");
    long n = subs ? MSG0(long, subs, "count") : 0;
    cc_r sb = MSGR(cc_r, layer, "bounds");
    for (long i = 0; i < n; i++) {
        id l = MSG(id, subs, "objectAtIndex:", (unsigned long)i);
        id cs = MSG0(id, l, "constraints");
        long nc = cs ? MSG0(long, cs, "count") : 0;
        if (!nc) continue;
        int hx[4] = {0}, hy[4] = {0};
        double vx[4] = {0}, vy[4] = {0};
        for (long j = 0; j < nc; j++) {
            cst_t *k = (cst_t *)MSG(id, cs, "objectAtIndex:", (unsigned long)j);
            if (!k || k->attr < 0 || k->attr > 7) continue;
            cc_r src;
            const char *sn = k->src ? cx_cstr(k->src) : 0;
            if (!sn || !strcmp(sn, "superlayer")) src = sb;
            else {
                id sib = find_sibling(subs, k->src);
                if (!sib || sib == l) continue;
                src = MSGR(cc_r, sib, "frame");
            }
            double v = attr_of(src, k->srcattr) * k->scale + k->offset;
            if (k->attr < 4) { hx[k->attr] = 1; vx[k->attr] = v; }
            else { hy[k->attr - 4] = 1; vy[k->attr - 4] = v; }
        }
        cc_r f = MSGR(cc_r, l, "frame");
        solve(&f.o.x, &f.s.w, hx, vx);
        solve(&f.o.y, &f.s.h, hy, vy);
        ((void (*)(id, SEL, cc_r))objc_msgSend)(l, sel_registerName("setFrame:"), f);
    }
}
static id lm_new(id k, SEL c) { (void)k; (void)c; return objc_autorelease(MSG0(id, cx_alloc_instance(&CAConstraintLayoutManager_c), "init")); }
/* tamanho preferido: o maior extremo das subcamadas depois do layout */
static cc_s lm_pref(id self, SEL c, id layer) {
    (void)self; (void)c;
    cc_s r = {0, 0};
    id subs = layer ? MSG0(id, layer, "sublayers") : 0;
    long n = subs ? MSG0(long, subs, "count") : 0;
    for (long i = 0; i < n; i++) {
        cc_r f = MSGR(cc_r, MSG(id, subs, "objectAtIndex:", (unsigned long)i), "frame");
        if (f.o.x + f.s.w > r.w) r.w = f.o.x + f.s.w;
        if (f.o.y + f.s.h > r.h) r.h = f.o.y + f.s.h;
    }
    return r;
}
static void lm_invalidate(id self, SEL c, id layer) { (void)self; (void)c; (void)layer; }

void cx_caconstraint_init(void) {
    cx_class_t *ob = cx_class_named("NSObject");
    cx_define_class(&CAConstraint_c, &CAConstraint_m, "CAConstraint", ob, (u32)(sizeof(cst_t) - 8));
    cx_add_cmethod(&CAConstraint_c, "constraintWithAttribute:relativeTo:attribute:scale:offset:", (void *)cst_new5);
    cx_add_cmethod(&CAConstraint_c, "constraintWithAttribute:relativeTo:attribute:offset:", (void *)cst_new4);
    cx_add_cmethod(&CAConstraint_c, "constraintWithAttribute:relativeTo:attribute:", (void *)cst_new3);
    cx_add_method(&CAConstraint_c, "initWithAttribute:relativeTo:attribute:scale:offset:", (void *)cst_init5);
    cx_add_method(&CAConstraint_c, "attribute", (void *)cst_attr);
    cx_add_method(&CAConstraint_c, "sourceAttribute", (void *)cst_srcattr);
    cx_add_method(&CAConstraint_c, "sourceName", (void *)cst_src);
    cx_add_method(&CAConstraint_c, "scale", (void *)cst_scale);
    cx_add_method(&CAConstraint_c, "offset", (void *)cst_offset);
    cx_add_method(&CAConstraint_c, "dealloc", (void *)cst_dealloc);
    cx_define_class(&CAConstraintLayoutManager_c, &CAConstraintLayoutManager_m, "CAConstraintLayoutManager", ob, 0);
    cx_add_cmethod(&CAConstraintLayoutManager_c, "layoutManager", (void *)lm_new);
    cx_add_method(&CAConstraintLayoutManager_c, "layoutSublayersOfLayer:", (void *)lm_layout);
    cx_add_method(&CAConstraintLayoutManager_c, "preferredSizeOfLayer:", (void *)lm_pref);
    cx_add_method(&CAConstraintLayoutManager_c, "invalidateLayoutOfLayer:", (void *)lm_invalidate);
}
