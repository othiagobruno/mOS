/* Path e as formas Rectangle, RoundedRectangle e Circle, com fill/stroke.
 * O Path guarda a lista de elementos (move, line, curve, close) e o retângulo
 * que os contém. As formas recebem o CGRect em xmm0..xmm3; RoundedRectangle
 * recebe o próprio struct (cornerSize e style) depois do retângulo.
 */
#ifdef SWIFTUI_SHAPES_HOST
#define SUI_HOST 1
#endif
#include "swiftui_model.h"

enum { SUI_PE_MOVE, SUI_PE_LINE, SUI_PE_CURVE, SUI_PE_CLOSE };
typedef struct { int op; double p[6]; } sui_pel;
typedef struct {
    const sui_desc *isa;
    int n, cap;
    sui_pel *el;
    double cx, cy;              /* ponto corrente */
    double sx, sy;              /* início do subcaminho */
    double minx, miny, maxx, maxy;
} sui_path;

static void *sui_shape_body(void);

sui_desc $s7SwiftUI4PathVMn = { SUI_K_STRUCT, "Path" };
sui_meta $s7SwiftUI4PathVN = { &$s7SwiftUI4PathVMn };
sui_conf $s7SwiftUI4PathVAA4ViewAAMc = { (void *)sui_shape_body, "View" };

static sui_path *sui_path_new(void) {
    sui_path *p = (sui_path *)sui_new(&$s7SwiftUI4PathVMn, sizeof *p);
    if (!p) return 0;
    p->minx = p->miny = 1e300;
    p->maxx = p->maxy = -1e300;
    return p;
}

static void sui_path_grow(sui_path *p, double x, double y) {
    if (x < p->minx) p->minx = x;
    if (y < p->miny) p->miny = y;
    if (x > p->maxx) p->maxx = x;
    if (y > p->maxy) p->maxy = y;
}

static sui_pel *sui_path_add(sui_path *p, int op) {
    sui_pel *e;
    if (!p) return 0;
    if (p->n == p->cap) {
        int cap = p->cap ? p->cap * 2 : 8;
        sui_pel *nel = (sui_pel *)realloc(p->el, (size_t)cap * sizeof *nel);
        if (!nel) return 0;
        p->el = nel;
        p->cap = cap;
    }
    e = &p->el[p->n++];
    memset(e, 0, sizeof *e);
    e->op = op;
    return e;
}

static void sui_move(sui_path *p, double x, double y) {
    sui_pel *e = sui_path_add(p, SUI_PE_MOVE);
    if (!e) return;
    e->p[0] = x; e->p[1] = y;
    p->cx = p->sx = x; p->cy = p->sy = y;
    sui_path_grow(p, x, y);
}

static void sui_line(sui_path *p, double x, double y) {
    sui_pel *e;
    if (p && p->n == 0) { sui_move(p, x, y); return; }
    e = sui_path_add(p, SUI_PE_LINE);
    if (!e) return;
    e->p[0] = x; e->p[1] = y;
    p->cx = x; p->cy = y;
    sui_path_grow(p, x, y);
}

static void sui_curve(sui_path *p, double c1x, double c1y, double c2x, double c2y, double x, double y) {
    sui_pel *e;
    if (p && p->n == 0) sui_move(p, c1x, c1y);
    e = sui_path_add(p, SUI_PE_CURVE);
    if (!e) return;
    e->p[0] = c1x; e->p[1] = c1y; e->p[2] = c2x; e->p[3] = c2y; e->p[4] = x; e->p[5] = y;
    p->cx = x; p->cy = y;
    sui_path_grow(p, c1x, c1y);
    sui_path_grow(p, c2x, c2y);
    sui_path_grow(p, x, y);
}

static void sui_close(sui_path *p) {
    if (!p || p->n == 0) return;
    if (!sui_path_add(p, SUI_PE_CLOSE)) return;
    p->cx = p->sx; p->cy = p->sy;
}

static double sui_sqrt(double x) {
    double r;
    if (x <= 0) return 0;
    __asm__("sqrtsd %1, %0" : "=x"(r) : "x"(x));
    return r;
}

/* Quarto de elipse por Bézier: k = 0,5523 do raio. */
#define SUI_KAPPA 0.5522847498307936

static void sui_rect(sui_path *p, double x, double y, double w, double h) {
    sui_move(p, x, y);
    sui_line(p, x + w, y);
    sui_line(p, x + w, y + h);
    sui_line(p, x, y + h);
    sui_close(p);
}

static void sui_round_rect(sui_path *p, double x, double y, double w, double h, double rw, double rh) {
    double kx, ky;
    if (rw > w / 2) rw = w / 2;
    if (rh > h / 2) rh = h / 2;
    if (rw <= 0 || rh <= 0) { sui_rect(p, x, y, w, h); return; }
    kx = rw * SUI_KAPPA; ky = rh * SUI_KAPPA;
    sui_move(p, x + rw, y);
    sui_line(p, x + w - rw, y);
    sui_curve(p, x + w - rw + kx, y, x + w, y + rh - ky, x + w, y + rh);
    sui_line(p, x + w, y + h - rh);
    sui_curve(p, x + w, y + h - rh + ky, x + w - rw + kx, y + h, x + w - rw, y + h);
    sui_line(p, x + rw, y + h);
    sui_curve(p, x + rw - kx, y + h, x, y + h - rh + ky, x, y + h - rh);
    sui_line(p, x, y + rh);
    sui_curve(p, x, y + rh - ky, x + rw - kx, y, x + rw, y);
    sui_close(p);
}

static void sui_ellipse(sui_path *p, double x, double y, double w, double h) {
    double rx = w / 2, ry = h / 2, cx = x + rx, cy = y + ry, kx = rx * SUI_KAPPA, ky = ry * SUI_KAPPA;
    sui_move(p, cx + rx, cy);
    sui_curve(p, cx + rx, cy + ky, cx + kx, cy + ry, cx, cy + ry);
    sui_curve(p, cx - kx, cy + ry, cx - rx, cy + ky, cx - rx, cy);
    sui_curve(p, cx - rx, cy - ky, cx - kx, cy - ry, cx, cy - ry);
    sui_curve(p, cx + kx, cy - ry, cx + rx, cy - ky, cx + rx, cy);
    sui_close(p);
}

/* ---- Path ---- */

void *$s7SwiftUI4PathVACycfC(void) { return sui_path_new(); }

/* Path { path in ... }: a closure recebe o Path (inout) em rdi. */
void *$s7SwiftUI4PathVyACyACzXEcfC(void *fn, void *ctx) {
    sui_path *p = sui_path_new();
    sui_fn f;
    f.fn = fn; f.ctx = ctx;
    if (p) sui_call1(f, (uint64_t)(uintptr_t)p);
    return p;
}

void $s7SwiftUI4PathV4move2toySo7CGPointV_tF(double x, double y) { sui_move((sui_path *)sui_self(), x, y); }
void $s7SwiftUI4PathV7addLine2toySo7CGPointV_tF(double x, double y) { sui_line((sui_path *)sui_self(), x, y); }
void $s7SwiftUI4PathV8addCurve2to8control18control2ySo7CGPointV_A2ItF(double x, double y, double c1x, double c1y, double c2x, double c2y) {
    sui_curve((sui_path *)sui_self(), c1x, c1y, c2x, c2y, x, y);
}
void $s7SwiftUI4PathV12closeSubpathyyF(void) { sui_close((sui_path *)sui_self()); }

/* addArc(tangent1End:tangent2End:radius:transform:): linha até o ponto de
 * tangência e o arco como uma Bézier, k = 4/3 tan(θ/4); a transformação afim
 * (a, b, c, d, tx, ty) vale para os pontos novos. */
void $s7SwiftUI4PathV6addArc11tangent1End08tangent2G06radius9transformySo7CGPointV_AJ12CoreGraphics7CGFloatVSo17CGAffineTransformVtF(
    double x1, double y1, double x2, double y2, double r, double ta, double tb, double tc, double td, double tx, double ty) {
    sui_path *p = (sui_path *)sui_self();
    double ux, uy, vx, vy, lu, lv, cosphi, tanhalf, d, t1x, t1y, t2x, t2y, costh, tan2, tan4, k;
    double pts[8];
    int i;
    if (!p) return;
    if (p->n == 0) sui_move(p, x1, y1);
    ux = p->cx - x1; uy = p->cy - y1;
    vx = x2 - x1; vy = y2 - y1;
    lu = sui_sqrt(ux * ux + uy * uy);
    lv = sui_sqrt(vx * vx + vy * vy);
    if (lu == 0 || lv == 0 || r <= 0) { sui_line(p, ta * x1 + tc * y1 + tx, tb * x1 + td * y1 + ty); return; }
    ux /= lu; uy /= lu; vx /= lv; vy /= lv;
    cosphi = ux * vx + uy * vy;
    if (cosphi > 0.999999 || cosphi < -0.999999) { sui_line(p, ta * x1 + tc * y1 + tx, tb * x1 + td * y1 + ty); return; }
    tanhalf = sui_sqrt((1 - cosphi) / (1 + cosphi));
    d = r / tanhalf;
    t1x = x1 + ux * d; t1y = y1 + uy * d;
    t2x = x1 + vx * d; t2y = y1 + vy * d;
    costh = -cosphi;
    tan2 = sui_sqrt((1 - costh) / (1 + costh));
    tan4 = tan2 / (1 + sui_sqrt(1 + tan2 * tan2));
    k = 4.0 / 3.0 * tan4 * r;
    pts[0] = t1x; pts[1] = t1y;
    pts[2] = t1x - ux * k; pts[3] = t1y - uy * k;
    pts[4] = t2x - vx * k; pts[5] = t2y - vy * k;
    pts[6] = t2x; pts[7] = t2y;
    for (i = 0; i < 8; i += 2) {
        double px = pts[i], py = pts[i + 1];
        pts[i] = ta * px + tc * py + tx;
        pts[i + 1] = tb * px + td * py + ty;
    }
    sui_line(p, pts[0], pts[1]);
    sui_curve(p, pts[2], pts[3], pts[4], pts[5], pts[6], pts[7]);
}

/* Path(roundedRect:cornerRadius:style:). */
void *$s7SwiftUI4PathV11roundedRect12cornerRadius5styleACSo6CGRectV_12CoreGraphics7CGFloatVAA18RoundedCornerStyleOtcfC(double x, double y, double w, double h, double r, int style) {
    sui_path *p = sui_path_new();
    (void)style;
    if (p) sui_round_rect(p, x, y, w, h, r, r);
    return p;
}

/* ---- Formas ---- */

sui_desc $s7SwiftUI9RectangleVMn = { SUI_K_STRUCT, "Rectangle" };
sui_meta $s7SwiftUI9RectangleVN = { &$s7SwiftUI9RectangleVMn };
void *$s7SwiftUI9RectangleV4path2inAA4PathVSo6CGRectV_tF(double x, double y, double w, double h) {
    sui_path *p = sui_path_new();
    if (p) sui_rect(p, x, y, w, h);
    return p;
}
sui_conf $s7SwiftUI9RectangleVAA4ViewAAMc = { (void *)sui_shape_body, "View" };
sui_conf $s7SwiftUI9RectangleVAA5ShapeAAMc = { (void *)$s7SwiftUI9RectangleV4path2inAA4PathVSo6CGRectV_tF, "Shape" };

/* RoundedRectangle: depois do retângulo vem o struct (cornerSize, style). */
sui_desc $s7SwiftUI16RoundedRectangleVMn = { SUI_K_STRUCT, "RoundedRectangle" };
void *$s7SwiftUI16RoundedRectangleVMa(void) { return &$s7SwiftUI16RoundedRectangleVMn; }
sui_desc $s7SwiftUI16RoundedRectangleV6_InsetVMn = { SUI_K_STRUCT, "RoundedRectangle._Inset" };
void *$s7SwiftUI16RoundedRectangleV6_InsetVMa(void) { return &$s7SwiftUI16RoundedRectangleV6_InsetVMn; }
void *$s7SwiftUI16RoundedRectangleV4path2inAA4PathVSo6CGRectV_tF(double x, double y, double w, double h, double cw, double ch, int style) {
    sui_path *p = sui_path_new();
    (void)style;
    if (p) sui_round_rect(p, x, y, w, h, cw, ch);
    return p;
}
sui_conf $s7SwiftUI16RoundedRectangleVAA4ViewAAMc = { (void *)sui_shape_body, "View" };
sui_conf $s7SwiftUI16RoundedRectangleVAA5ShapeAAMc = { (void *)$s7SwiftUI16RoundedRectangleV4path2inAA4PathVSo6CGRectV_tF, "Shape" };

/* Circle: o maior círculo centrado no retângulo. */
static void *sui_circle_path(double x, double y, double w, double h) {
    sui_path *p = sui_path_new();
    double s = w < h ? w : h;
    if (p) sui_ellipse(p, x + (w - s) / 2, y + (h - s) / 2, s, s);
    return p;
}
sui_desc $s7SwiftUI6CircleVMn = { SUI_K_STRUCT, "Circle" };
sui_meta $s7SwiftUI6CircleVN = { &$s7SwiftUI6CircleVMn };
sui_desc $s7SwiftUI6CircleV6_InsetVMn = { SUI_K_STRUCT, "Circle._Inset" };
sui_conf $s7SwiftUI6CircleVAA4ViewAAMc = { (void *)sui_shape_body, "View" };
sui_conf $s7SwiftUI6CircleVAA5ShapeAAMc = { (void *)sui_circle_path, "Shape" };
sui_conf $s7SwiftUI6CircleVAA15InsettableShapeAAMc = { (void *)sui_circle_path, "InsettableShape" };
sui_desc $s7SwiftUI7CapsuleVMn = { SUI_K_STRUCT, "Capsule" };
void *$s7SwiftUI7CapsuleVMa(void) { return &$s7SwiftUI7CapsuleVMn; }
/* Capsule: retângulo com raio de metade do lado menor (o estilo circular ou contínuo não muda o traçado). */
static void *sui_capsule_path(double x, double y, double w, double h) {
    sui_path *p = sui_path_new();
    double r = (w < h ? w : h) / 2;
    if (p) sui_round_rect(p, x, y, w, h, r, r);
    return p;
}
sui_conf $s7SwiftUI7CapsuleVAA4ViewAAMc = { (void *)sui_shape_body, "View" };
sui_conf $s7SwiftUI7CapsuleVAA5ShapeAAMc = { (void *)sui_capsule_path, "Shape" };
sui_desc $s7SwiftUI7EllipseVMn = { SUI_K_STRUCT, "Ellipse" };

/* ---- Visões de forma: fill, stroke, strokeBorder e body ---- */

typedef struct { const sui_desc *isa; void *shape; void *style; double width; uint64_t flags; } sui_shape_v;
sui_desc $s7SwiftUI10_ShapeViewVMn = { SUI_K_STYLE, "_ShapeView" };
sui_conf $s7SwiftUI10_ShapeViewVyxq_GAA0D0AAMc = { (void *)sui_shape_body, "View" };
sui_desc $s7SwiftUI15StrokeShapeViewVMn = { SUI_K_STYLE, "StrokeShapeView" };
sui_desc $s7SwiftUI21StrokeBorderShapeViewVMn = { SUI_K_STYLE, "StrokeBorderShapeView" };
/* background(_:in:) com forma insetável: o modificador que guarda a forma e o estilo. */
sui_desc $s7SwiftUI34_InsettableBackgroundShapeModifierVMn = { SUI_K_MODIFIER, "_InsettableBackgroundShapeModifier" };
sui_conf $s7SwiftUI34_InsettableBackgroundShapeModifierVyxq_GAA04ViewF0AAMc = { (void *)sui_shape_body, "ViewModifier" };
sui_desc $s7SwiftUI13_StrokedShapeVMn = { SUI_K_STYLE, "_StrokedShape" };

static void *sui_shape_body(void) { return 0; }

static void *sui_shapeview(const sui_desc *d, void *shape, void *style, double width, uint64_t flags) {
    sui_shape_v *v = (sui_shape_v *)sui_new(d, sizeof *v);
    if (!v) return 0;
    v->shape = shape;
    v->style = style;
    v->width = width;
    v->flags = flags;
    return v;
}

/* fill(_:style:): FillStyle tem eoFill e antialiased (bits 0 e 1). */
void *$s7SwiftUI5ShapePAAE4fill_5styleQrqd___AA9FillStyleVtAA0cG0Rd__lF(void *content, uint64_t style) {
    return sui_shapeview(&$s7SwiftUI10_ShapeViewVMn, sui_self(), content, 0, style & 3);
}

void *$s7SwiftUI5ShapePAAE6stroke_9lineWidthQrqd___12CoreGraphics7CGFloatVtAA0C5StyleRd__lF(void *content, double width) {
    return sui_shapeview(&$s7SwiftUI15StrokeShapeViewVMn, sui_self(), content, width < 0 ? 0 : width, 0);
}

/* strokeBorder: o traço fica dentro da forma (flag 4). */
void *$s7SwiftUI15InsettableShapePAAE12strokeBorder_9lineWidth11antialiasedQrqd___12CoreGraphics7CGFloatVSbtAA0D5StyleRd__lF(void *content, double width, int aa) {
    return sui_shapeview(&$s7SwiftUI21StrokeBorderShapeViewVMn, sui_self(), content, width < 0 ? 0 : width, 4 | ((uint64_t)(aa & 1) << 1));
}

/* body de toda Shape: _ShapeView<Self, ForegroundStyle> preenchendo com a cor do primeiro plano. */
void *$s7SwiftUI5ShapePAAE4bodyAA01_C4ViewVyxAA15ForegroundStyleVGvg(void) {
    return sui_shapeview(&$s7SwiftUI10_ShapeViewVMn, sui_self(), 0, 0, 2);
}

/* ShapeRole: fill 0, stroke 1, separator 2; o padrão de Shape é fill. */
int $s7SwiftUI5ShapePAAE4roleAA0C4RoleOvgZ(void) { return 0; }

/* StrokeStyle: largura, cap, join, miterLimit, dash (Array do libSystem) e fase. */
typedef struct { const sui_desc *isa; double width; int cap, join; double miter; void *dash; double phase; } sui_stroke;
static sui_desc sui_stroke_desc = { SUI_K_STRUCT, "StrokeStyle" };
void *$s7SwiftUI11StrokeStyleV9lineWidth0E3Cap0E4Join10miterLimit4dash0K5PhaseAC12CoreGraphics7CGFloatV_So06CGLineG0VSo0pH0VALSayALGALtcfC(
    double width, int cap, int join, double miter, void *dash, double phase) {
    sui_stroke *s = (sui_stroke *)sui_new(&sui_stroke_desc, sizeof *s);
    if (!s) return 0;
    s->width = width < 0 ? 0 : width;
    s->cap = cap & 0xFF;
    s->join = join & 0xFF;
    s->miter = miter > 0 ? miter : 10;
    s->dash = dash;
    s->phase = phase;
    return s;
}
