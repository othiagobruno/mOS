/* Color, Font, Gradient, Material e estilos de controle.
 * clang -std=c11 -Wall -Wextra -Werror -DSWIFTUI_STYLE_HOST \
 *   userland/libsystem/swiftui_style.c userland/tests-swiftui/style_test.c -o /tmp/sui_style
 */
#define SUI_HOST 1
#include <stdio.h>
#include "../libsystem/swiftui_model.h"

typedef struct { void *color; double location; } stop;
typedef struct { const sui_desc *isa; int n; stop s[16]; } gradient;
typedef struct { void *isa; int64_t count; int64_t cap; void *el[3]; } arr3;
typedef struct { void *isa; int64_t count; int64_t cap; stop el[2]; } arrs;
typedef struct { const sui_desc *isa; int64_t code; } scode;
typedef struct { const sui_desc *isa; void *label; int pressed; } btncfg;

void *$s7SwiftUI5ColorV3redACvgZ(void);
void *$s7SwiftUI5ColorV4blueACvgZ(void);
void *$s7SwiftUI5ColorV5clearACvgZ(void);
void *$s7SwiftUI5ColorV_3red5green4blue7opacityA2C13RGBColorSpaceO_S4dtcfC(int space, double r, double g, double b, double a);
void *$s7SwiftUI5ColorV_5white7opacityA2C13RGBColorSpaceO_S2dtcfC(int space, double w, double a);
void *$s7SwiftUI5ColorV7opacityyACSdF(double op);
void *$s7SwiftUI5ColorV02nsC0ACSo7NSColorC_tcfC(void *o);
double $s7SwiftUI4FontV6WeightV4boldAEvgZ(void);
double $s7SwiftUI4FontV6WeightV8semiboldAEvgZ(void);
int $s7SwiftUI4FontV6WeightV2eeoiySbAE_AEtFZ(double a, double b);
void *$s7SwiftUI4FontV4bodyACvgZ(void);
void *$s7SwiftUI4FontV8headlineACvgZ(void);
void *$s7SwiftUI4FontV7captionACvgZ(void);
void *$s7SwiftUI4FontV6system4size6weight6designAC12CoreGraphics7CGFloatV_AC6WeightVAC6DesignOtFZ(double size, double weight, int design);
void *$s7SwiftUI4FontV6system4size6weight6designAC12CoreGraphics7CGFloatV_AC6WeightVSgAC6DesignOSgtFZ(double size, double weight, int wn, int design);
void *$s7SwiftUI4FontV6system_6designA2C9TextStyleO_AC6DesignOtFZ(int style, int design);
void *$s7SwiftUI4FontV6weightyA2C6WeightVF(double w);
void *$s7SwiftUI4FontV4boldACyF(void);
stop $s7SwiftUI8GradientV4StopV5color8locationAeA5ColorV_12CoreGraphics7CGFloatVtcfC(void *color, double loc);
void *$s7SwiftUI8GradientV6colorsACSayAA5ColorVG_tcfC(const void *arr);
void *$s7SwiftUI8GradientV5stopsACSayAC4StopVG_tcfC(const void *arr);
void *$s7SwiftUI14LinearGradientV8gradient10startPoint03endG0AcA0D0V_AA04UnitG0VAJtcfC(void *g, double sx, double sy, double ex, double ey);
void *$s7SwiftUI8MaterialV7regularACvgZ(void);
uint32_t $s7SwiftUI22HierarchicalShapeStyleV9secondaryACvgZ(void);
void *$s7SwiftUI16PlainButtonStyleVACycfC(void);
void *$s7SwiftUI16PlainButtonStyleVMa(void);
void *$s7SwiftUI15MenuPickerStyleVACycfC(void);
extern sui_desc $s7SwiftUI16PlainButtonStyleVMn, $s7SwiftUI15MenuPickerStyleVMn, $s7SwiftUI5ColorVMn;
extern sui_wtab $s7SwiftUI15MenuPickerStyleVAA0dE0AAWP;
void *$s7SwiftUI24ButtonStyleConfigurationV5labelAC5LabelVvg(void);
int $s7SwiftUI24ButtonStyleConfigurationV9isPressedSbvg(void);

#define arm(p) __asm__ volatile("mov %0, %%r13" :: "r"(p) : "r13", "memory")

int main(void) {
    int fails = 0;
    sui_color *c, *red = $s7SwiftUI5ColorV3redACvgZ();
    sui_font *f;
    gradient *g;
    arr3 colors;
    arrs stops;
    stop s;
    scode *m;
    btncfg cfg;
    char lbl = 'l', nsc = 'n';
    void *lg;
    if (!red || red->isa != &$s7SwiftUI5ColorVMn || red->r != 1.0 || red->a != 1) { fprintf(stderr, "FAIL Color.red\n"); fails++; }
    if (((sui_color *)$s7SwiftUI5ColorV5clearACvgZ())->a != 0) { fprintf(stderr, "FAIL Color.clear\n"); fails++; }
    c = $s7SwiftUI5ColorV_3red5green4blue7opacityA2C13RGBColorSpaceO_S4dtcfC(2, 0.1, 0.2, 1.5, 0.5);
    if (!c || c->g != 0.2 || c->b != 1.0 || c->a != 0.5 || c->space != 2) { fprintf(stderr, "FAIL Color(rgb)\n"); fails++; }
    c = $s7SwiftUI5ColorV_5white7opacityA2C13RGBColorSpaceO_S2dtcfC(0, 0.3, 1);
    if (!c || c->r != 0.3 || c->b != 0.3) { fprintf(stderr, "FAIL Color(white)\n"); fails++; }
    arm(red);
    c = $s7SwiftUI5ColorV7opacityyACSdF(0.5);
    if (!c || c->a != 0.5 || red->a != 1 || c->r != 1.0) { fprintf(stderr, "FAIL Color.opacity\n"); fails++; }
    c = $s7SwiftUI5ColorV02nsC0ACSo7NSColorC_tcfC(&nsc);
    if (!c || c->native != &nsc) { fprintf(stderr, "FAIL Color(nsColor:)\n"); fails++; }

    if (!$s7SwiftUI4FontV6WeightV2eeoiySbAE_AEtFZ($s7SwiftUI4FontV6WeightV4boldAEvgZ(), 0.4) ||
        $s7SwiftUI4FontV6WeightV2eeoiySbAE_AEtFZ($s7SwiftUI4FontV6WeightV8semiboldAEvgZ(), 0.4)) {
        fprintf(stderr, "FAIL Font.Weight\n"); fails++;
    }
    f = $s7SwiftUI4FontV4bodyACvgZ();
    if (!f || f->size != 13 || f->style != 6) { fprintf(stderr, "FAIL Font.body\n"); fails++; }
    f = $s7SwiftUI4FontV8headlineACvgZ();
    if (!f || f->size != 13 || f->weight != 0.4) { fprintf(stderr, "FAIL Font.headline\n"); fails++; }
    if (((sui_font *)$s7SwiftUI4FontV7captionACvgZ())->size != 10) { fprintf(stderr, "FAIL Font.caption\n"); fails++; }
    f = $s7SwiftUI4FontV6system4size6weight6designAC12CoreGraphics7CGFloatV_AC6WeightVAC6DesignOtFZ(17, 0.3, 3);
    if (!f || f->size != 17 || f->weight != 0.3 || f->design != 3) { fprintf(stderr, "FAIL Font.system(size:)\n"); fails++; }
    f = $s7SwiftUI4FontV6system4size6weight6designAC12CoreGraphics7CGFloatV_AC6WeightVSgAC6DesignOSgtFZ(11, 0.4, 1, 4);
    if (!f || f->size != 11 || f->weight != 0 || f->design != 0) { fprintf(stderr, "FAIL Font.system opcionais\n"); fails++; }
    f = $s7SwiftUI4FontV6system_6designA2C9TextStyleO_AC6DesignOtFZ(0, 2);
    if (!f || f->size != 26 || f->design != 2) { fprintf(stderr, "FAIL Font.system(style)\n"); fails++; }
    arm(f);
    f = $s7SwiftUI4FontV4boldACyF();
    if (!f || !f->bold || f->weight != 0.4 || f->size != 26) { fprintf(stderr, "FAIL Font.bold\n"); fails++; }
    arm(f);
    f = $s7SwiftUI4FontV6weightyA2C6WeightVF(-0.4);
    if (!f || f->weight != -0.4 || f->bold) { fprintf(stderr, "FAIL Font.weight\n"); fails++; }

    s = $s7SwiftUI8GradientV4StopV5color8locationAeA5ColorV_12CoreGraphics7CGFloatVtcfC(red, 1.7);
    if (s.color != red || s.location != 1) { fprintf(stderr, "FAIL Gradient.Stop\n"); fails++; }
    colors.isa = 0; colors.count = 3; colors.cap = 3;
    colors.el[0] = red; colors.el[1] = $s7SwiftUI5ColorV4blueACvgZ(); colors.el[2] = red;
    g = $s7SwiftUI8GradientV6colorsACSayAA5ColorVG_tcfC(&colors);
    if (!g || g->n != 3 || g->s[1].location != 0.5 || g->s[2].location != 1 || g->s[1].color != colors.el[1]) {
        fprintf(stderr, "FAIL Gradient(colors:)\n"); fails++;
    }
    stops.isa = 0; stops.count = 2; stops.cap = 2;
    stops.el[0].color = red; stops.el[0].location = 0.2;
    stops.el[1].color = red; stops.el[1].location = 0.9;
    g = $s7SwiftUI8GradientV5stopsACSayAC4StopVG_tcfC(&stops);
    if (!g || g->n != 2 || g->s[1].location != 0.9) { fprintf(stderr, "FAIL Gradient(stops:)\n"); fails++; }
    lg = $s7SwiftUI14LinearGradientV8gradient10startPoint03endG0AcA0D0V_AA04UnitG0VAJtcfC(g, 0, 0, 1, 1);
    if (!lg || ((void **)lg)[1] != g) { fprintf(stderr, "FAIL LinearGradient\n"); fails++; }

    m = $s7SwiftUI8MaterialV7regularACvgZ();
    if (!m || m->code != 2 || $s7SwiftUI22HierarchicalShapeStyleV9secondaryACvgZ() != 1) { fprintf(stderr, "FAIL Material/Hierarchical\n"); fails++; }
    m = $s7SwiftUI16PlainButtonStyleVACycfC();
    if (!m || m->isa != &$s7SwiftUI16PlainButtonStyleVMn || $s7SwiftUI16PlainButtonStyleVMa() != &$s7SwiftUI16PlainButtonStyleVMn) {
        fprintf(stderr, "FAIL PlainButtonStyle\n"); fails++;
    }
    m = $s7SwiftUI15MenuPickerStyleVACycfC();
    if (!m || m->isa != &$s7SwiftUI15MenuPickerStyleVMn || !$s7SwiftUI15MenuPickerStyleVAA0dE0AAWP.conf ||
        strcmp($s7SwiftUI15MenuPickerStyleVAA0dE0AAWP.conf->proto, "PickerStyle") != 0) {
        fprintf(stderr, "FAIL MenuPickerStyle\n"); fails++;
    }
    cfg.isa = 0; cfg.label = &lbl; cfg.pressed = 1;
    arm(&cfg);
    if ($s7SwiftUI24ButtonStyleConfigurationV5labelAC5LabelVvg() != &lbl) { fprintf(stderr, "FAIL configuration.label\n"); fails++; }
    arm(&cfg);
    if (!$s7SwiftUI24ButtonStyleConfigurationV9isPressedSbvg()) { fprintf(stderr, "FAIL configuration.isPressed\n"); fails++; }
    if (fails) { fprintf(stderr, "RESULT FAIL %d\n", fails); return 1; }
    printf("RESULT PASS\n");
    return 0;
}
