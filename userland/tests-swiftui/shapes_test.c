/* Path (move/line/curve/close/addArc/roundedRect), Rectangle, RoundedRectangle,
 * Circle e fill/stroke/strokeBorder/body.
 * clang -std=c11 -Wall -Wextra -Werror -DSWIFTUI_SHAPES_HOST \
 *   userland/libsystem/swiftui_shapes.c userland/tests-swiftui/shapes_test.c -o /tmp/sui_shapes
 */
#define SUI_HOST 1
#include <stdio.h>
#include "../libsystem/swiftui_model.h"

typedef struct { int op; double p[6]; } pel;
typedef struct { const sui_desc *isa; int n, cap; pel *el; double cx, cy, sx, sy, minx, miny, maxx, maxy; } path;
typedef struct { const sui_desc *isa; void *shape; void *style; double width; uint64_t flags; } shapeview;

void *$s7SwiftUI4PathVACycfC(void);
void *$s7SwiftUI4PathVyACyACzXEcfC(void *fn, void *ctx);
void $s7SwiftUI4PathV4move2toySo7CGPointV_tF(double x, double y);
void $s7SwiftUI4PathV7addLine2toySo7CGPointV_tF(double x, double y);
void $s7SwiftUI4PathV8addCurve2to8control18control2ySo7CGPointV_A2ItF(double x, double y, double c1x, double c1y, double c2x, double c2y);
void $s7SwiftUI4PathV12closeSubpathyyF(void);
void $s7SwiftUI4PathV6addArc11tangent1End08tangent2G06radius9transformySo7CGPointV_AJ12CoreGraphics7CGFloatVSo17CGAffineTransformVtF(
    double x1, double y1, double x2, double y2, double r, double a, double b, double c, double d, double tx, double ty);
void *$s7SwiftUI4PathV11roundedRect12cornerRadius5styleACSo6CGRectV_12CoreGraphics7CGFloatVAA18RoundedCornerStyleOtcfC(double x, double y, double w, double h, double r, int style);
void *$s7SwiftUI9RectangleV4path2inAA4PathVSo6CGRectV_tF(double x, double y, double w, double h);
void *$s7SwiftUI16RoundedRectangleV4path2inAA4PathVSo6CGRectV_tF(double x, double y, double w, double h, double cw, double ch, int style);
extern sui_conf $s7SwiftUI6CircleVAA5ShapeAAMc;
extern sui_desc $s7SwiftUI10_ShapeViewVMn, $s7SwiftUI15StrokeShapeViewVMn;
void *$s7SwiftUI5ShapePAAE4fill_5styleQrqd___AA9FillStyleVtAA0cG0Rd__lF(void *content, uint64_t style);
void *$s7SwiftUI5ShapePAAE6stroke_9lineWidthQrqd___12CoreGraphics7CGFloatVtAA0C5StyleRd__lF(void *content, double width);
void *$s7SwiftUI15InsettableShapePAAE12strokeBorder_9lineWidth11antialiasedQrqd___12CoreGraphics7CGFloatVSbtAA0D5StyleRd__lF(void *content, double width, int aa);
void *$s7SwiftUI5ShapePAAE4bodyAA01_C4ViewVyxAA15ForegroundStyleVGvg(void);

#define arm(p) __asm__ volatile("mov %0, %%r13" :: "r"(p) : "r13", "memory")

static int near(double a, double b) { double d = a - b; return d < 1e-6 && d > -1e-6; }

/* Path { p in p.move(to: (1,2)); p.addLine(to: (5,2)) } */
static void build(void *p) {
    arm(p);
    $s7SwiftUI4PathV4move2toySo7CGPointV_tF(1, 2);
    arm(p);
    $s7SwiftUI4PathV7addLine2toySo7CGPointV_tF(5, 2);
    /* Sem isto o -O2 vira tail call e restaura o r13 antes do salto. */
    __asm__ volatile("" ::: "memory");
}

int main(void) {
    int fails = 0;
    path *p;
    shapeview *v;
    char red = 'r';
    void *(*circle)(double, double, double, double) = (void *(*)(double, double, double, double))$s7SwiftUI6CircleVAA5ShapeAAMc.body;

    p = $s7SwiftUI4PathVyACyACzXEcfC((void *)build, 0);
    if (!p || p->n != 2 || p->el[0].op != 0 || p->el[1].op != 1 || p->el[1].p[0] != 5 || p->maxx != 5 || p->minx != 1) {
        fprintf(stderr, "FAIL Path { }\n"); fails++;
    }
    p = $s7SwiftUI4PathVACycfC();
    arm(p); $s7SwiftUI4PathV4move2toySo7CGPointV_tF(0, 0);
    arm(p); $s7SwiftUI4PathV8addCurve2to8control18control2ySo7CGPointV_A2ItF(10, 0, 3, 5, 7, 5);
    arm(p); $s7SwiftUI4PathV12closeSubpathyyF();
    if (p->n != 3 || p->el[1].op != 2 || p->el[1].p[4] != 10 || p->el[1].p[0] != 3 || p->el[2].op != 3 || p->cx != 0 || p->maxy != 5) {
        fprintf(stderr, "FAIL curve/close\n"); fails++;
    }
    /* Canto de 90° em (10,0) indo para (10,10), raio 2: tangências em (8,0) e (10,2). */
    p = $s7SwiftUI4PathVACycfC();
    arm(p); $s7SwiftUI4PathV4move2toySo7CGPointV_tF(0, 0);
    arm(p); $s7SwiftUI4PathV6addArc11tangent1End08tangent2G06radius9transformySo7CGPointV_AJ12CoreGraphics7CGFloatVSo17CGAffineTransformVtF(10, 0, 10, 10, 2, 1, 0, 0, 1, 0, 0);
    if (p->n != 3 || !near(p->el[1].p[0], 8) || !near(p->el[1].p[1], 0) || !near(p->el[2].p[4], 10) || !near(p->el[2].p[5], 2) ||
        !near(p->el[2].p[0], 8 + 2 * 0.5522847498307936)) {
        fprintf(stderr, "FAIL addArc %g %g %g\n", p->el[1].p[0], p->el[2].p[4], p->el[2].p[0]); fails++;
    }
    p = $s7SwiftUI9RectangleV4path2inAA4PathVSo6CGRectV_tF(1, 1, 4, 3);
    if (!p || p->n != 5 || p->maxx != 5 || p->maxy != 4 || p->el[4].op != 3) { fprintf(stderr, "FAIL Rectangle.path\n"); fails++; }
    p = $s7SwiftUI16RoundedRectangleV4path2inAA4PathVSo6CGRectV_tF(0, 0, 20, 10, 4, 4, 1);
    if (!p || p->n != 10 || p->el[0].p[0] != 4 || p->maxx != 20 || p->maxy != 10) { fprintf(stderr, "FAIL RoundedRectangle.path\n"); fails++; }
    p = $s7SwiftUI4PathV11roundedRect12cornerRadius5styleACSo6CGRectV_12CoreGraphics7CGFloatVAA18RoundedCornerStyleOtcfC(0, 0, 10, 10, 50, 0);
    if (!p || p->el[0].p[0] != 5) { fprintf(stderr, "FAIL roundedRect raio limitado\n"); fails++; }
    p = circle(0, 0, 20, 10);
    if (!p || p->n != 6 || !near(p->minx, 5) || !near(p->maxx, 15) || !near(p->maxy, 10)) { fprintf(stderr, "FAIL Circle\n"); fails++; }

    arm(p);
    v = $s7SwiftUI5ShapePAAE4fill_5styleQrqd___AA9FillStyleVtAA0cG0Rd__lF(&red, 1);
    if (!v || v->isa != &$s7SwiftUI10_ShapeViewVMn || v->shape != p || v->style != &red || v->flags != 1) { fprintf(stderr, "FAIL fill\n"); fails++; }
    arm(p);
    v = $s7SwiftUI5ShapePAAE6stroke_9lineWidthQrqd___12CoreGraphics7CGFloatVtAA0C5StyleRd__lF(&red, 2);
    if (!v || v->isa != &$s7SwiftUI15StrokeShapeViewVMn || v->width != 2) { fprintf(stderr, "FAIL stroke\n"); fails++; }
    arm(p);
    v = $s7SwiftUI15InsettableShapePAAE12strokeBorder_9lineWidth11antialiasedQrqd___12CoreGraphics7CGFloatVSbtAA0D5StyleRd__lF(&red, 1, 1);
    if (!v || !(v->flags & 4) || !(v->flags & 2)) { fprintf(stderr, "FAIL strokeBorder\n"); fails++; }
    arm(p);
    v = $s7SwiftUI5ShapePAAE4bodyAA01_C4ViewVyxAA15ForegroundStyleVGvg();
    if (!v || v->shape != p || v->style) { fprintf(stderr, "FAIL Shape.body\n"); fails++; }
    if (fails) { fprintf(stderr, "RESULT FAIL %d\n", fails); return 1; }
    printf("RESULT PASS\n");
    return 0;
}
