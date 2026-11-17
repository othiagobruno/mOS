/*
 * mOS libSystem — NSGradient de verdade: paradas de cor com posições, linear em retângulo, caminho ou entre dois
 * pontos, radial entre dois círculos ou num retângulo/caminho com centro relativo, cor interpolada.
 * O desenho é feito em faixas (linear) ou discos (radial) preenchidos com o NSBezierPath; dentro de um caminho, cada
 * faixa recorta o polígono do caminho (gradient_core.c, teste userland/tests-appkit/gradient_host.c).
 * Registro: cx_appkit_gradient_init(), chamado do cx_appkit_input_init (depois do NSGradient básico do appkit_postico.c).
 */
#include "foundation.h"
#include "libc.h"
#include "gradient_core.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void objc_setAssociatedObject(id obj, const void *key, id value, u64 policy);
id objc_getAssociatedObject(id obj, const void *key);
typedef struct { double x, y; } P;
typedef struct { double x, y, w, h; } R;
static cx_class_t *kls(const char *n) { return cx_class_named(n); }
static SEL sl(const char *n) { return sel_registerName(n); }

static char k_stops, k_space;
static gc_stops *ST_(id g) { id b = objc_getAssociatedObject(g, &k_stops); return b ? MSG0(void *, b, "pointerValue") : 0; }
static gc_stops *stops_new(id g) {
    gc_stops *s = calloc(1, sizeof *s);
    if (!s) return 0;
    objc_setAssociatedObject(g, &k_stops, MSG(id, (id)kls("NSValue"), "valueWithPointer:", (void *)s), 1);
    return s;
}
static gc_color comps(id c) {
    gc_color r = {0, 0, 0, 1};
    if (!c) return r;
    double rr = 0, gg = 0, bb = 0, aa = 1;
    ((void (*)(id, SEL, double *, double *, double *, double *))objc_msgSend)(c, sl("getRed:green:blue:alpha:"), &rr, &gg, &bb, &aa);
    r.r = rr; r.g = gg; r.b = bb; r.a = aa;
    return r;
}
static id make_color(gc_color c) { return MSG(id, (id)kls("NSColor"), "colorWithSRGBRed:green:blue:alpha:", c.r, c.g, c.b, c.a); }

/* ---- inicializadores */
static id gr_init_locs(id self, SEL c, id colors, const double *locs, id space) {
    (void)c;
    gc_stops *s = stops_new(self);
    u64 n = colors ? MSG0(u64, colors, "count") : 0;
    if (!s || n < 2) { if (s) s->n = 0; return n >= 2 ? self : 0; }
    if (n > 64) n = 64;
    for (u64 i = 0; i < n; i++) {
        id col = MSG(id, colors, "objectAtIndex:", i);
        s->c[i] = comps(col);
        s->loc[i] = locs ? locs[i] : (double)i / (double)(n - 1);
    }
    s->n = (int)n;
    gc_normalize(s);
    if (space) objc_setAssociatedObject(self, &k_space, space, 1);
    return self;
}
static id gr_init_colors(id self, SEL c, id colors) { return gr_init_locs(self, c, colors, 0, 0); }
static id gr_init_two(id self, SEL c, id a, id b) {
    if (!a || !b) return 0;
    id arr = MSG(id, (id)kls("NSMutableArray"), "arrayWithCapacity:", (u64)2);
    MSG(void, arr, "addObject:", a);
    MSG(void, arr, "addObject:", b);
    return gr_init_locs(self, c, arr, 0, 0);
}
static id gr_init_colors_locs_space(id self, SEL c, id colors, const double *locs, id space) { return gr_init_locs(self, c, colors, locs, space); }
/* initWithColorsAndLocations: cor, posição (CGFloat), cor, posição..., nil */
static id gr_init_va(id self, SEL c, id first, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, first);
    id arr = MSG(id, (id)kls("NSMutableArray"), "arrayWithCapacity:", (u64)8);
    double locs[64];
    int n = 0;
    for (id col = first; col && n < 64; col = __builtin_va_arg(ap, id)) {
        locs[n++] = __builtin_va_arg(ap, double);
        MSG(void, arr, "addObject:", col);
    }
    __builtin_va_end(ap);
    return gr_init_locs(self, c, arr, locs, 0);
}

/* ---- consultas */
static i64 gr_count(id self, SEL c) { (void)c; gc_stops *s = ST_(self); return s ? s->n : 0; }
static void gr_get(id self, SEL c, id *color, double *loc, i64 i) {
    (void)c;
    gc_stops *s = ST_(self);
    if (!s || i < 0 || i >= s->n) return;
    if (color) *color = make_color(s->c[i]);
    if (loc) *loc = s->loc[i];
}
static id gr_color_at(id self, SEL c, double t) { (void)c; gc_stops *s = ST_(self); gc_color z = {0, 0, 0, 0}; return make_color(s ? gc_color_at(s, t) : z); }
static id gr_space(id self, SEL c) { (void)c; id sp = objc_getAssociatedObject(self, &k_space); return sp ? sp : MSG0(id, (id)kls("NSColorSpace"), "sRGBColorSpace"); }

/* ---- desenho */
static void fill_poly(const double *pts, int n, gc_color col) {
    if (n < 3) return;
    id path = MSG0(id, (id)kls("NSBezierPath"), "bezierPath");
    for (int i = 0; i < n; i++) {
        P p = {pts[i * 2], pts[i * 2 + 1]};
        ((void (*)(id, SEL, P))objc_msgSend)(path, sl(i ? "lineToPoint:" : "moveToPoint:"), p);
    }
    MSG0(void, path, "closePath");
    MSG0(void, make_color(col), "setFill");
    MSG0(void, path, "fill");
}
/* polígonos (subcaminhos achatados) de um NSBezierPath */
typedef struct { double *xy; int n; } poly_t;
static int path_polys(id path, poly_t *out, int cap) {
    id flat = MSG0(id, path, "bezierPathByFlatteningPath");
    i64 n = flat ? MSG0(i64, flat, "elementCount") : 0;
    int k = -1;
    for (i64 i = 0; i < n; i++) {
        P pt[3];
        i64 kind = MSG(i64, flat, "elementAtIndex:associatedPoints:", i, pt);
        if (kind == 0 || k < 0) {
            if (k + 1 >= cap) break;
            k++;
            out[k].xy = malloc(sizeof(double) * 2 * 1024);
            out[k].n = 0;
            if (!out[k].xy) return k;
        }
        if (kind == 3) continue;
        if (out[k].n < 1024) { out[k].xy[out[k].n * 2] = pt[0].x; out[k].xy[out[k].n * 2 + 1] = pt[0].y; out[k].n++; }
    }
    return k + 1;
}
static int steps_for(double len) { int s = (int)(len / 2); return s < 2 ? 2 : s > 256 ? 256 : s; }
/* linear entre p0 e p1, dentro de uma forma (polígonos) ou de uma caixa; opções: 1 antes do início, 2 depois do fim */
/* as faixas das pontas vão além de p0/p1 até cobrir a forma (drawInRect/drawInBezierPath pintam a forma inteira) */
static void draw_linear(gc_stops *s, P p0, P p1, const poly_t *polys, int npoly, const double bbox[4]) {
    if (!s || s->n < 1) return;
    double dx = p1.x - p0.x, dy = p1.y - p0.y, len = __builtin_sqrt(dx * dx + dy * dy);
    int steps = steps_for(len);
    double rect[8] = {bbox[0], bbox[1], bbox[0] + bbox[2], bbox[1], bbox[0] + bbox[2], bbox[1] + bbox[3], bbox[0], bbox[1] + bbox[3]};
    static double out[2 * 1024];
    for (int k = 0; k < steps; k++) {
        double q[8];
        gc_band(p0.x, p0.y, p1.x, p1.y, k, steps, bbox, q);
        gc_color col = gc_color_at(s, (k + 0.5) / steps);
        if (npoly) for (int i = 0; i < npoly; i++) { int n = gc_clip(polys[i].xy, polys[i].n, q, 4, out, 1024); fill_poly(out, n, col); }
        else { int n = gc_clip(rect, 4, q, 4, out, 1024); fill_poly(out, n, col); }
    }
}
static void axis_for(R r, double angle, P *a, P *b) {
    double rad = angle * 3.14159265358979323846 / 180.0, ux = __builtin_cos(rad), uy = __builtin_sin(rad);
    double cx = r.x + r.w / 2, cy = r.y + r.h / 2;
    double half = (__builtin_fabs(ux) * r.w + __builtin_fabs(uy) * r.h) / 2;   /* projeção do retângulo no eixo */
    a->x = cx - ux * half; a->y = cy - uy * half;
    b->x = cx + ux * half; b->y = cy + uy * half;
}
static void gr_draw_rect(id self, SEL c, R r, double angle) {
    (void)c;
    P a, b;
    axis_for(r, angle, &a, &b);
    double bb[4] = {r.x, r.y, r.w, r.h};
    draw_linear(ST_(self), a, b, 0, 0, bb);
}
static void gr_draw_path(id self, SEL c, id path, double angle) {
    (void)c;
    if (!path) return;
    R r = ((R (*)(id, SEL))objc_msgSend)(path, sl("bounds"));
    P a, b;
    axis_for(r, angle, &a, &b);
    poly_t polys[32];
    int np = path_polys(path, polys, 32);
    double bb[4] = {r.x, r.y, r.w, r.h};
    draw_linear(ST_(self), a, b, polys, np, bb);
    for (int i = 0; i < np; i++) free(polys[i].xy);
}
static void gr_draw_points(id self, SEL c, P a, P b, u64 opts) {
    (void)c;
    /* preenche a área do recorte atual: uma caixa grande o bastante (o window server recorta pela view) */
    double bb[4] = {-8192, -8192, 16384, 16384};
    gc_stops *s = ST_(self);
    if (!s) return;
    double dx = b.x - a.x, dy = b.y - a.y, len = __builtin_sqrt(dx * dx + dy * dy);
    if (len <= 0) return;
    int steps = steps_for(len);
    double rect[8] = {bb[0], bb[1], bb[0] + bb[2], bb[1], bb[0] + bb[2], bb[1] + bb[3], bb[0], bb[1] + bb[3]};
    static double out[2 * 1024];
    double ux = dx / len, uy = dy / len, nx = -uy, ny = ux, far = 16384;
    for (int k = 0; k < steps; k++) {
        double t0 = (double)k / steps * len, t1 = (double)(k + 1) / steps * len;
        if (k == 0 && (opts & 1)) t0 -= far;
        if (k == steps - 1 && (opts & 2)) t1 += far;
        double q[8] = {a.x + ux * t0 - nx * far, a.y + uy * t0 - ny * far, a.x + ux * t1 - nx * far, a.y + uy * t1 - ny * far,
                       a.x + ux * t1 + nx * far, a.y + uy * t1 + ny * far, a.x + ux * t0 + nx * far, a.y + uy * t0 + ny * far};
        int n = gc_clip(rect, 4, q, 4, out, 1024);
        fill_poly(out, n, gc_color_at(s, (k + 0.5) / steps));
    }
}
/* radial: discos do fim para o começo (o de dentro pinta por cima), centro e raio interpolados */
static void draw_radial(gc_stops *s, P c0, double r0, P c1, double r1, u64 opts, const poly_t *polys, int npoly) {
    if (!s || s->n < 1) return;
    double span = __builtin_fabs(r1 - r0) + __builtin_sqrt((c1.x - c0.x) * (c1.x - c0.x) + (c1.y - c0.y) * (c1.y - c0.y));
    int steps = steps_for(span);
    static double disk[2 * 96], out[2 * 1024];
    double tmax = 1, tmin = 0;
    if (opts & 2) tmax = 1 + 4096.0 / (span > 1 ? span : 1);   /* depois do fim: o disco cresce até cobrir tudo */
    for (int k = steps; k >= 0; k--) {
        double t = (double)k / steps * (tmax - tmin) + tmin;
        double cx = c0.x + (c1.x - c0.x) * t, cy = c0.y + (c1.y - c0.y) * t, r = r0 + (r1 - r0) * t;
        if (r <= 0) continue;
        for (int i = 0; i < 96; i++) { double a = i * 2 * 3.14159265358979323846 / 96; disk[i * 2] = cx + __builtin_cos(a) * r; disk[i * 2 + 1] = cy + __builtin_sin(a) * r; }
        gc_color col = gc_color_at(s, t > 1 ? 1 : t);
        if (npoly) for (int i = 0; i < npoly; i++) { int n = gc_clip(polys[i].xy, polys[i].n, disk, 96, out, 1024); fill_poly(out, n, col); }
        else fill_poly(disk, 96, col);
    }
    if ((opts & 1) && r0 > 0) {   /* antes do começo: o disco inicial preenchido com a primeira cor */
        for (int i = 0; i < 96; i++) { double a = i * 2 * 3.14159265358979323846 / 96; disk[i * 2] = c0.x + __builtin_cos(a) * r0; disk[i * 2 + 1] = c0.y + __builtin_sin(a) * r0; }
        if (npoly) for (int i = 0; i < npoly; i++) { int n = gc_clip(polys[i].xy, polys[i].n, disk, 96, out, 1024); fill_poly(out, n, s->c[0]); }
        else fill_poly(disk, 96, s->c[0]);
    }
}
static void gr_draw_radial(id self, SEL c, P c0, double r0, P c1, double r1, u64 opts) { (void)c; draw_radial(ST_(self), c0, r0, c1, r1, opts, 0, 0); }
/* centro relativo: (-1,-1) canto de baixo à esquerda, (0,0) meio; raio até o canto mais distante */
static void relative_center(R r, P rel, P *center, double *radius) {
    center->x = r.x + r.w / 2 * (1 + rel.x);
    center->y = r.y + r.h / 2 * (1 + rel.y);
    double best = 0;
    double xs[2] = {r.x, r.x + r.w}, ys[2] = {r.y, r.y + r.h};
    for (int i = 0; i < 2; i++) for (int j = 0; j < 2; j++) {
        double d = __builtin_sqrt((xs[i] - center->x) * (xs[i] - center->x) + (ys[j] - center->y) * (ys[j] - center->y));
        if (d > best) best = d;
    }
    *radius = best;
}
static void gr_draw_rect_rel(id self, SEL c, R r, P rel) {
    (void)c;
    P ct; double rad;
    relative_center(r, rel, &ct, &rad);
    double rect[8] = {r.x, r.y, r.x + r.w, r.y, r.x + r.w, r.y + r.h, r.x, r.y + r.h};
    poly_t one = {rect, 4};
    draw_radial(ST_(self), ct, 0, ct, rad, 2, &one, 1);
}
static void gr_draw_path_rel(id self, SEL c, id path, P rel) {
    (void)c;
    if (!path) return;
    R r = ((R (*)(id, SEL))objc_msgSend)(path, sl("bounds"));
    P ct; double rad;
    relative_center(r, rel, &ct, &rad);
    poly_t polys[32];
    int np = path_polys(path, polys, 32);
    draw_radial(ST_(self), ct, 0, ct, rad, 2, polys, np);
    for (int i = 0; i < np; i++) free(polys[i].xy);
}
static id gr_copy(id self, SEL c, void *zone) {
    (void)c; (void)zone;
    id o = MSG0(id, (id)object_getClass(self), "alloc");
    gc_stops *src = ST_(self), *dst = stops_new(o);
    if (src && dst) *dst = *src;
    return o;
}

#define ADD(s, f) cx_add_method(g, s, (void *)(f))
void cx_appkit_gradient_init(void) {
    cx_class_t *g = kls("NSGradient");
    if (!g) return;
    ADD("initWithColors:", gr_init_colors); ADD("initWithStartingColor:endingColor:", gr_init_two);
    ADD("initWithColors:atLocations:colorSpace:", gr_init_colors_locs_space); ADD("initWithColorsAndLocations:", gr_init_va);
    ADD("numberOfColorStops", gr_count); ADD("getColor:location:atIndex:", gr_get); ADD("interpolatedColorAtLocation:", gr_color_at);
    ADD("colorSpace", gr_space); ADD("copyWithZone:", gr_copy);
    ADD("drawInRect:angle:", gr_draw_rect); ADD("drawInBezierPath:angle:", gr_draw_path);
    ADD("drawFromPoint:toPoint:options:", gr_draw_points);
    ADD("drawFromCenter:radius:toCenter:radius:options:", gr_draw_radial);
    ADD("drawInRect:relativeCenterPosition:", gr_draw_rect_rel); ADD("drawInBezierPath:relativeCenterPosition:", gr_draw_path_rel);
}
