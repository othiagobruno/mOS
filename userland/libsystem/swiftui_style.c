/* Color, Font, Gradient, Material e os estilos (ButtonStyle, PickerStyle,
 * TextFieldStyle, ListStyle, ProgressViewStyle, NavigationViewStyle).
 * Color guarda RGBA em double; Font guarda tamanho, peso (o CGFloat do
 * Font.Weight), design e estilo de texto. self em r13.
 */
#ifdef SWIFTUI_STYLE_HOST
#define SUI_HOST 1
#endif
#include "swiftui_model.h"

#ifndef SUI_HOST
typedef struct cx_class cx_class_t;
const char *sel_registerName(const char *name);
signed char cx_responds(cx_class_t *cls, const char *sel);
void *cx_lookup(cx_class_t *cls, const char *sel);
double CTFontGetSize(void *font);
#endif

static void *sui_style_body(void) { return sui_self(); }
static void *sui_color_body(void) { return 0; }

/* ---- Color ---- */

sui_desc $s7SwiftUI5ColorVMn = { SUI_K_COLOR, "Color" };
sui_meta $s7SwiftUI5ColorVN = { &$s7SwiftUI5ColorVMn };
sui_conf $s7SwiftUI5ColorVAA4ViewAAMc = { (void *)sui_color_body, "View" };
sui_conf $s7SwiftUI5ColorVAA10ShapeStyleAAMc = { (void *)sui_style_body, "ShapeStyle" };
sui_wtab $s7SwiftUI5ColorVAA4ViewAAWP = { &$s7SwiftUI5ColorVAA4ViewAAMc, (void *)sui_color_body };

#define SUI_RGBA(r, g, b, a) { &$s7SwiftUI5ColorVMn, r, g, b, a, 0, 0 }
static sui_color sui_c_red = SUI_RGBA(1.0, 0.231, 0.188, 1);
static sui_color sui_c_orange = SUI_RGBA(1.0, 0.584, 0.0, 1);
static sui_color sui_c_yellow = SUI_RGBA(1.0, 0.8, 0.0, 1);
static sui_color sui_c_green = SUI_RGBA(0.204, 0.78, 0.349, 1);
static sui_color sui_c_blue = SUI_RGBA(0.0, 0.478, 1.0, 1);
static sui_color sui_c_purple = SUI_RGBA(0.686, 0.322, 0.871, 1);
static sui_color sui_c_gray = SUI_RGBA(0.557, 0.557, 0.576, 1);
static sui_color sui_c_black = SUI_RGBA(0, 0, 0, 1);
static sui_color sui_c_white = SUI_RGBA(1, 1, 1, 1);
static sui_color sui_c_clear = SUI_RGBA(0, 0, 0, 0);
static sui_color sui_c_primary = SUI_RGBA(0, 0, 0, 0.85);
static sui_color sui_c_secondary = SUI_RGBA(0, 0, 0, 0.5);
static sui_color sui_c_accent = SUI_RGBA(0.0, 0.478, 1.0, 1);

void *$s7SwiftUI5ColorV3redACvgZ(void) { return &sui_c_red; }
void *$s7SwiftUI5ColorV6orangeACvgZ(void) { return &sui_c_orange; }
void *$s7SwiftUI5ColorV6yellowACvgZ(void) { return &sui_c_yellow; }
void *$s7SwiftUI5ColorV5greenACvgZ(void) { return &sui_c_green; }
void *$s7SwiftUI5ColorV4blueACvgZ(void) { return &sui_c_blue; }
void *$s7SwiftUI5ColorV6purpleACvgZ(void) { return &sui_c_purple; }
void *$s7SwiftUI5ColorV4grayACvgZ(void) { return &sui_c_gray; }
void *$s7SwiftUI5ColorV5blackACvgZ(void) { return &sui_c_black; }
void *$s7SwiftUI5ColorV5whiteACvgZ(void) { return &sui_c_white; }
void *$s7SwiftUI5ColorV5clearACvgZ(void) { return &sui_c_clear; }
void *$s7SwiftUI5ColorV7primaryACvgZ(void) { return &sui_c_primary; }
void *$s7SwiftUI5ColorV9secondaryACvgZ(void) { return &sui_c_secondary; }
void *$s7SwiftUI5ColorV06accentC0ACvgZ(void) { return &sui_c_accent; }

static double sui_clamp01(double v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

/* Color == Color: a mesma referência, ou as duas com o mesmo espaço, os mesmos componentes e a mesma cor nativa. */
int $s7SwiftUI5ColorV2eeoiySbAC_ACtFZ(const sui_color *a, const sui_color *b) {
    if (a == b) return 1;
    if (!a || !b || a->isa != &$s7SwiftUI5ColorVMn || b->isa != &$s7SwiftUI5ColorVMn) return 0;
    return a->space == b->space && a->r == b->r && a->g == b->g && a->b == b->b && a->a == b->a && a->native == b->native;
}

static sui_color *sui_color_new(double r, double g, double b, double a, int space) {
    sui_color *c = (sui_color *)sui_new(&$s7SwiftUI5ColorVMn, sizeof *c);
    if (!c) return 0;
    c->r = sui_clamp01(r); c->g = sui_clamp01(g); c->b = sui_clamp01(b); c->a = sui_clamp01(a);
    c->space = space & 0xFF;
    return c;
}

void *$s7SwiftUI5ColorV_3red5green4blue7opacityA2C13RGBColorSpaceO_S4dtcfC(int space, double r, double g, double b, double a) {
    return sui_color_new(r, g, b, a, space);
}

void *$s7SwiftUI5ColorV_5white7opacityA2C13RGBColorSpaceO_S2dtcfC(int space, double w, double a) {
    return sui_color_new(w, w, w, a, space);
}

void *$s7SwiftUI5ColorV7opacityyACSdF(double op) {
    sui_color *src = (sui_color *)sui_self();
    sui_color *c;
    if (!src) return 0;
    c = sui_color_new(src->r, src->g, src->b, src->a * sui_clamp01(op), src->space);
    if (c) c->native = src->native;
    return c;
}

/* NSColor/UIColor: no libSystem lê o RGBA com getRed:green:blue:alpha:. */
static void *sui_color_native(void *obj) {
    sui_color *c = sui_color_new(0, 0, 0, 1, 0);
    if (!c) return 0;
    c->native = obj;
#ifndef SUI_HOST
    if (obj) {
        const char *sel = sel_registerName("getRed:green:blue:alpha:");
        cx_class_t *cls = *(cx_class_t **)obj;
        if (cls && cx_responds(cls, sel)) {
            void (*imp)(void *, const char *, double *, double *, double *, double *) =
                (void (*)(void *, const char *, double *, double *, double *, double *))cx_lookup(cls, sel);
            if (imp) imp(obj, sel, &c->r, &c->g, &c->b, &c->a);
        }
    }
#endif
    return c;
}

void *$s7SwiftUI5ColorV02nsC0ACSo7NSColorC_tcfC(void *o) { return sui_color_native(o); }
void *$s7SwiftUI5ColorV02uiC0ACSo7UIColorC_tcfC(void *o) { return sui_color_native(o); }
void *$s7SwiftUI5ColorVyACSo7NSColorCcfC(void *o) { return sui_color_native(o); }
void *$s7SwiftUI5ColorVyACSo7UIColorCcfC(void *o) { return sui_color_native(o); }

/* ---- Font ---- */

sui_desc $s7SwiftUI4FontVMn = { SUI_K_FONT, "Font" };

/* Font.Weight é um CGFloat: os valores do NSFont.Weight. */
double $s7SwiftUI4FontV6WeightV10ultraLightAEvgZ(void) { return -0.8; }
double $s7SwiftUI4FontV6WeightV4thinAEvgZ(void) { return -0.6; }
double $s7SwiftUI4FontV6WeightV5lightAEvgZ(void) { return -0.4; }
double $s7SwiftUI4FontV6WeightV7regularAEvgZ(void) { return 0.0; }
double $s7SwiftUI4FontV6WeightV6mediumAEvgZ(void) { return 0.23; }
double $s7SwiftUI4FontV6WeightV8semiboldAEvgZ(void) { return 0.3; }
double $s7SwiftUI4FontV6WeightV4boldAEvgZ(void) { return 0.4; }
double $s7SwiftUI4FontV6WeightV5heavyAEvgZ(void) { return 0.56; }
double $s7SwiftUI4FontV6WeightV5blackAEvgZ(void) { return 0.62; }
int $s7SwiftUI4FontV6WeightV2eeoiySbAE_AEtFZ(double a, double b) { return a == b; }

/* TextStyle: tamanho no macOS; -1 = fonte por tamanho. */
static const double sui_style_size[11] = { 26, 22, 17, 15, 13, 11, 13, 12, 10, 10, 10 };
#define SUI_TS(st, w) { &$s7SwiftUI4FontVMn, 0, w, 0, st, 0, 0 }
static sui_font sui_f_body = SUI_TS(6, 0);
static sui_font sui_f_callout = SUI_TS(7, 0);
static sui_font sui_f_caption = SUI_TS(9, 0);
static sui_font sui_f_caption2 = SUI_TS(10, 0);
static sui_font sui_f_footnote = SUI_TS(8, 0);
static sui_font sui_f_headline = SUI_TS(4, 0.4);
static sui_font sui_f_subheadline = SUI_TS(5, 0);

static sui_font *sui_font_fix(sui_font *f) {
    if (f && f->size == 0 && f->style >= 0 && f->style < 11) f->size = sui_style_size[f->style];
    return f;
}

void *$s7SwiftUI4FontV4bodyACvgZ(void) { return sui_font_fix(&sui_f_body); }
void *$s7SwiftUI4FontV7calloutACvgZ(void) { return sui_font_fix(&sui_f_callout); }
void *$s7SwiftUI4FontV7captionACvgZ(void) { return sui_font_fix(&sui_f_caption); }
void *$s7SwiftUI4FontV8caption2ACvgZ(void) { return sui_font_fix(&sui_f_caption2); }
void *$s7SwiftUI4FontV8footnoteACvgZ(void) { return sui_font_fix(&sui_f_footnote); }
void *$s7SwiftUI4FontV8headlineACvgZ(void) { return sui_font_fix(&sui_f_headline); }
void *$s7SwiftUI4FontV11subheadlineACvgZ(void) { return sui_font_fix(&sui_f_subheadline); }

static sui_font *sui_font_new(double size, double weight, int design, int style) {
    sui_font *f = (sui_font *)sui_new(&$s7SwiftUI4FontVMn, sizeof *f);
    if (!f) return 0;
    f->size = size > 0 ? size : 13;
    f->weight = weight;
    f->design = design;
    f->style = style;
    f->bold = weight >= 0.4;
    return f;
}

void *$s7SwiftUI4FontV6system4size6weight6designAC12CoreGraphics7CGFloatV_AC6WeightVAC6DesignOtFZ(double size, double weight, int design) {
    return sui_font_new(size, weight, design & 0xFF, -1);
}

/* Weight? = (CGFloat, flag de nil); Design? nil = 4. */
void *$s7SwiftUI4FontV6system4size6weight6designAC12CoreGraphics7CGFloatV_AC6WeightVSgAC6DesignOSgtFZ(double size, double weight, int weight_nil, int design) {
    design &= 0xFF;
    return sui_font_new(size, (weight_nil & 1) ? 0 : weight, design > 3 ? 0 : design, -1);
}

void *$s7SwiftUI4FontV6system_6designA2C9TextStyleO_AC6DesignOtFZ(int style, int design) {
    style &= 0xFF;
    if (style > 10) style = 6;
    return sui_font_new(sui_style_size[style], style == 4 ? 0.4 : 0, design & 0xFF, style);
}

void *$s7SwiftUI4FontV6weightyA2C6WeightVF(double weight) {
    sui_font *src = sui_font_fix((sui_font *)sui_self());
    sui_font *f;
    if (!src) return 0;
    f = sui_font_new(src->size, weight, src->design, src->style);
    if (f) f->ct = src->ct;
    return f;
}

void *$s7SwiftUI4FontV4boldACyF(void) {
    sui_font *src = sui_font_fix((sui_font *)sui_self());
    sui_font *f;
    if (!src) return 0;
    f = sui_font_new(src->size, src->weight > 0.4 ? src->weight : 0.4, src->design, src->style);
    if (f) { f->ct = src->ct; f->bold = 1; }
    return f;
}

void *$s7SwiftUI4FontVyACSo9CTFontRefacfC(void *ct) {
    double size = 13;
#ifndef SUI_HOST
    if (ct) size = CTFontGetSize(ct);
#endif
    {
        sui_font *f = sui_font_new(size, 0, 0, -1);
        if (f) f->ct = ct;
        return f;
    }
}

/* ---- Gradient ---- */

typedef struct { void *color; double location; } sui_stop;
typedef struct { const sui_desc *isa; int n; sui_stop stop[16]; } sui_gradient;
/* Array do libSystem (string_fdn.c): { isa, count, cap, elementos }. */
typedef struct { void *isa; int64_t count; int64_t cap; } sui_abuf;

sui_desc $s7SwiftUI8GradientV4StopVMn = { SUI_K_STRUCT, "Gradient.Stop" };
sui_meta $s7SwiftUI8GradientV4StopVN = { &$s7SwiftUI8GradientV4StopVMn };
static sui_desc sui_gradient_desc = { SUI_K_STRUCT, "Gradient" };

sui_stop $s7SwiftUI8GradientV4StopV5color8locationAeA5ColorV_12CoreGraphics7CGFloatVtcfC(void *color, double loc) {
    sui_stop s;
    s.color = color;
    s.location = sui_clamp01(loc);
    return s;
}

void *$s7SwiftUI8GradientV5stopsACSayAC4StopVG_tcfC(const sui_abuf *arr) {
    sui_gradient *g = (sui_gradient *)sui_new(&sui_gradient_desc, sizeof *g);
    const sui_stop *el;
    int i;
    if (!g) return 0;
    if (!arr) return g;
    el = (const sui_stop *)(arr + 1);
    g->n = arr->count > 16 ? 16 : (int)arr->count;
    for (i = 0; i < g->n; i++) g->stop[i] = el[i];
    return g;
}

void *$s7SwiftUI8GradientV6colorsACSayAA5ColorVG_tcfC(const sui_abuf *arr) {
    sui_gradient *g = (sui_gradient *)sui_new(&sui_gradient_desc, sizeof *g);
    void *const *el;
    int i;
    if (!g) return 0;
    if (!arr) return g;
    el = (void *const *)(arr + 1);
    g->n = arr->count > 16 ? 16 : (int)arr->count;
    for (i = 0; i < g->n; i++) {
        g->stop[i].color = el[i];
        g->stop[i].location = g->n > 1 ? (double)i / (double)(g->n - 1) : 0;
    }
    return g;
}

typedef struct { const sui_desc *isa; void *gradient; double p[4]; double r0, r1; } sui_grad_view;

sui_desc $s7SwiftUI14LinearGradientVMn = { SUI_K_STYLE, "LinearGradient" };
sui_meta $s7SwiftUI14LinearGradientVN = { &$s7SwiftUI14LinearGradientVMn };
sui_conf $s7SwiftUI14LinearGradientVAA4ViewAAMc = { (void *)sui_color_body, "View" };
sui_desc $s7SwiftUI14RadialGradientVMn = { SUI_K_STYLE, "RadialGradient" };

/* UnitPoint chega em dois CGFloat cada: start (xmm0, xmm1), end (xmm2, xmm3). */
void *$s7SwiftUI14LinearGradientV8gradient10startPoint03endG0AcA0D0V_AA04UnitG0VAJtcfC(void *g, double sx, double sy, double ex, double ey) {
    sui_grad_view *v = (sui_grad_view *)sui_new(&$s7SwiftUI14LinearGradientVMn, sizeof *v);
    if (!v) return 0;
    v->gradient = g;
    v->p[0] = sx; v->p[1] = sy; v->p[2] = ex; v->p[3] = ey;
    return v;
}

void *$s7SwiftUI14RadialGradientV8gradient6center11startRadius03endH0AcA0D0V_AA9UnitPointV12CoreGraphics7CGFloatVANtcfC(void *g, double cx, double cy, double r0, double r1) {
    sui_grad_view *v = (sui_grad_view *)sui_new(&$s7SwiftUI14RadialGradientVMn, sizeof *v);
    if (!v) return 0;
    v->gradient = g;
    v->p[0] = cx; v->p[1] = cy;
    v->r0 = r0 < 0 ? 0 : r0;
    v->r1 = r1 < v->r0 ? v->r0 : r1;
    return v;
}

/* ---- Material e estilos hierárquicos ---- */

sui_desc $s7SwiftUI8MaterialVMn = { SUI_K_STYLE, "Material" };
typedef struct { const sui_desc *isa; int64_t code; } sui_scode;
static sui_scode sui_m_ultrathin = { &$s7SwiftUI8MaterialVMn, 0 };
static sui_scode sui_m_thin = { &$s7SwiftUI8MaterialVMn, 1 };
static sui_scode sui_m_regular = { &$s7SwiftUI8MaterialVMn, 2 };
static sui_scode sui_m_thick = { &$s7SwiftUI8MaterialVMn, 3 };
void *$s7SwiftUI8MaterialV9ultraThinACvgZ(void) { return &sui_m_ultrathin; }
void *$s7SwiftUI8MaterialV4thinACvgZ(void) { return &sui_m_thin; }
void *$s7SwiftUI8MaterialV7regularACvgZ(void) { return &sui_m_regular; }
void *$s7SwiftUI8MaterialV5thickACvgZ(void) { return &sui_m_thick; }

/* HierarchicalShapeStyle é um UInt32: primary 0, secondary 1. */
sui_desc $s7SwiftUI22HierarchicalShapeStyleVMn = { SUI_K_STYLE, "HierarchicalShapeStyle" };
sui_meta $s7SwiftUI22HierarchicalShapeStyleVN = { &$s7SwiftUI22HierarchicalShapeStyleVMn };
sui_conf $s7SwiftUI22HierarchicalShapeStyleVAA0dE0AAMc = { (void *)sui_style_body, "ShapeStyle" };
uint32_t $s7SwiftUI22HierarchicalShapeStyleV7primaryACvgZ(void) { return 0; }
uint32_t $s7SwiftUI22HierarchicalShapeStyleV9secondaryACvgZ(void) { return 1; }

sui_desc $s7SwiftUI13AnyShapeStyleVMn = { SUI_K_STYLE, "AnyShapeStyle" };
sui_desc $s7SwiftUI15ForegroundStyleVMn = { SUI_K_STYLE, "ForegroundStyle" };

/* ---- Estilos de controle: init() devolve o valor único do estilo ---- */

#define SUI_STYLE(mn, ma, init, mc, k, proto) \
    sui_desc mn = { SUI_K_STYLE, k }; \
    static sui_scode sui_##init##_v = { &mn, 0 }; \
    void *ma(void) { return &mn; } \
    void *init(void) { return &sui_##init##_v; } \
    sui_conf mc = { (void *)sui_style_body, proto };

#define SUI_STYLE_WP(mn, ma, init, wp, k, proto) \
    sui_desc mn = { SUI_K_STYLE, k }; \
    static sui_scode sui_##init##_v = { &mn, 0 }; \
    static sui_conf sui_##init##_c = { (void *)sui_style_body, proto }; \
    void *ma(void) { return &mn; } \
    void *init(void) { return &sui_##init##_v; } \
    sui_wtab wp = { &sui_##init##_c, (void *)sui_style_body };

SUI_STYLE($s7SwiftUI16PlainButtonStyleVMn, $s7SwiftUI16PlainButtonStyleVMa, $s7SwiftUI16PlainButtonStyleVACycfC,
          $s7SwiftUI16PlainButtonStyleVAA09PrimitivedE0AAMc, "PlainButtonStyle", "PrimitiveButtonStyle")
SUI_STYLE($s7SwiftUI18DefaultButtonStyleVMn, $s7SwiftUI18DefaultButtonStyleVMa, $s7SwiftUI18DefaultButtonStyleVACycfC,
          $s7SwiftUI18DefaultButtonStyleVAA09PrimitivedE0AAMc, "DefaultButtonStyle", "PrimitiveButtonStyle")
SUI_STYLE($s7SwiftUI19PlainTextFieldStyleVMn, $s7SwiftUI19PlainTextFieldStyleVMa, $s7SwiftUI19PlainTextFieldStyleVACycfC,
          $s7SwiftUI19PlainTextFieldStyleVAA0deF0AAMc, "PlainTextFieldStyle", "TextFieldStyle")
SUI_STYLE($s7SwiftUI27RoundedBorderTextFieldStyleVMn, $s7SwiftUI27RoundedBorderTextFieldStyleVMa, $s7SwiftUI27RoundedBorderTextFieldStyleVACycfC,
          $s7SwiftUI27RoundedBorderTextFieldStyleVAA0efG0AAMc, "RoundedBorderTextFieldStyle", "TextFieldStyle")
SUI_STYLE($s7SwiftUI23LinearProgressViewStyleVMn, $s7SwiftUI23LinearProgressViewStyleVMa, $s7SwiftUI23LinearProgressViewStyleVACycfC,
          $s7SwiftUI23LinearProgressViewStyleVAA0deF0AAMc, "LinearProgressViewStyle", "ProgressViewStyle")
SUI_STYLE($s7SwiftUI21BorderlessButtonStyleVMn, $s7SwiftUI21BorderlessButtonStyleVMa, $s7SwiftUI21BorderlessButtonStyleVACycfC,
          $s7SwiftUI21BorderlessButtonStyleVAA09PrimitivedE0AAMc, "BorderlessButtonStyle", "PrimitiveButtonStyle")
SUI_STYLE($s7SwiftUI25CircularProgressViewStyleVMn, $s7SwiftUI25CircularProgressViewStyleVMa, $s7SwiftUI25CircularProgressViewStyleVACycfC,
          $s7SwiftUI25CircularProgressViewStyleVAA0deF0AAMc, "CircularProgressViewStyle", "ProgressViewStyle")
SUI_STYLE($s7SwiftUI24StackNavigationViewStyleVMn, $s7SwiftUI24StackNavigationViewStyleVMa, $s7SwiftUI24StackNavigationViewStyleVACycfC,
          $s7SwiftUI24StackNavigationViewStyleVAA0deF0AAMc, "StackNavigationViewStyle", "NavigationViewStyle")
SUI_STYLE_WP($s7SwiftUI15MenuPickerStyleVMn, $s7SwiftUI15MenuPickerStyleVMa, $s7SwiftUI15MenuPickerStyleVACycfC,
             $s7SwiftUI15MenuPickerStyleVAA0dE0AAWP, "MenuPickerStyle", "PickerStyle")
SUI_STYLE_WP($s7SwiftUI17InlinePickerStyleVMn, $s7SwiftUI17InlinePickerStyleVMa, $s7SwiftUI17InlinePickerStyleVACycfC,
             $s7SwiftUI17InlinePickerStyleVAA0dE0AAWP, "InlinePickerStyle", "PickerStyle")
SUI_STYLE_WP($s7SwiftUI20SegmentedPickerStyleVMn, $s7SwiftUI20SegmentedPickerStyleVMa, $s7SwiftUI20SegmentedPickerStyleVACycfC,
             $s7SwiftUI20SegmentedPickerStyleVAA0dE0AAWP, "SegmentedPickerStyle", "PickerStyle")
SUI_STYLE_WP($s7SwiftUI21InsetGroupedListStyleVMn, $s7SwiftUI21InsetGroupedListStyleVMa, $s7SwiftUI21InsetGroupedListStyleVACycfC,
             $s7SwiftUI21InsetGroupedListStyleVAA0eF0AAWP, "InsetGroupedListStyle", "ListStyle")

/* ---- ButtonStyle e a configuração que ele recebe ---- */

typedef struct { const sui_desc *isa; void *label; int pressed; } sui_btncfg;
sui_desc $s7SwiftUI24ButtonStyleConfigurationV5LabelVMn = { SUI_K_STRUCT, "ButtonStyleConfiguration.Label" };
sui_conf $s7SwiftUI24ButtonStyleConfigurationV5LabelVAA4ViewAAMc = { (void *)sui_color_body, "View" };
void *$s7SwiftUI24ButtonStyleConfigurationV5LabelVMa(void) { return &$s7SwiftUI24ButtonStyleConfigurationV5LabelVMn; }

void *$s7SwiftUI24ButtonStyleConfigurationV5labelAC5LabelVvg(void) {
    sui_btncfg *c = (sui_btncfg *)sui_self();
    return c ? c->label : 0;
}

int $s7SwiftUI24ButtonStyleConfigurationV9isPressedSbvg(void) {
    sui_btncfg *c = (sui_btncfg *)sui_self();
    return c ? c->pressed : 0;
}
