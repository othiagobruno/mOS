/* Símbolos SwiftUI que o Postico 2.3.5 importa e o libSystem não tinha (machocheck, 10 nov. 2026).
 * zig cc -std=c11 -Wall -Wextra -Werror -DSWIFTUI_VALUES_HOST -DSWIFTUI_STYLE_HOST -DSWIFTUI_MOD_HOST \
 *   -DSWIFTUI_STATE_HOST -DSWIFTUI_SHAPES_HOST -DSWIFTUI_ENV_HOST -DSWIFTUI_HOSTING_HOST -DSWIFTUI_VIEWS_HOST \
 *   userland/libsystem/swiftui_values.c userland/libsystem/swiftui_style.c userland/libsystem/swiftui_modifiers.c \
 *   userland/libsystem/swiftui_state.c userland/libsystem/swiftui_shapes.c userland/libsystem/swiftui_env.c \
 *   userland/libsystem/swiftui_hosting.c userland/libsystem/swiftui_views.c \
 *   userland/tests-swiftui/postico_test.c -o /tmp/sui_postico && /tmp/sui_postico
 */
#define SUI_HOST 1
#include <stdio.h>
#include "../libsystem/swiftui_model.h"

#define arm(p) __asm__ volatile("mov %0, %%r13" :: "r"(p) : "r13", "memory")

typedef struct { uint64_t w0, w1; } pair;
typedef struct { const void *isa; uint64_t *slot; void *get, *set; uint64_t constant; int readonly; } binding;
typedef struct { const void *isa; void *root; void *view; void *coder; } hc;
typedef struct { const sui_desc *isa; void *shape; void *style; double width; uint64_t flags; } shapev;

/* valores */
extern const int32_t $s7SwiftUI18ControlActiveStateO3keyyA2CmFWC, $s7SwiftUI18ControlActiveStateO8inactiveyA2CmFWC;
extern const int32_t $s7SwiftUI18RoundedCornerStyleO8circularyA2CmFWC, $s7SwiftUI4TextV14TruncationModeO6middleyA2EmFWC;
int $s7SwiftUI18ControlActiveStateO2eeoiySbAC_ACtFZ(int a, int b);
void *$s7SwiftUI18ControlActiveStateOMa(void);
pair $s7SwiftUI13KeyEquivalentV6returnACvgZ(void);
uint64_t $s7SwiftUI14EventModifiersV8rawValueACSi_tcfC(int64_t raw);
/* estilos e cor */
extern sui_desc $s7SwiftUI21BorderlessButtonStyleVMn, $s7SwiftUI25CircularProgressViewStyleVMn, $s7SwiftUI5ColorVMn;
extern sui_conf $s7SwiftUI21BorderlessButtonStyleVAA09PrimitivedE0AAMc, $s7SwiftUI25CircularProgressViewStyleVAA0deF0AAMc;
void *$s7SwiftUI21BorderlessButtonStyleVACycfC(void);
void *$s7SwiftUI21BorderlessButtonStyleVMa(void);
void *$s7SwiftUI25CircularProgressViewStyleVACycfC(void);
void *$s7SwiftUI25CircularProgressViewStyleVMa(void);
int $s7SwiftUI5ColorV2eeoiySbAC_ACtFZ(const void *a, const void *b);
void *$s7SwiftUI5ColorV3redACvgZ(void);
void *$s7SwiftUI5ColorV4blueACvgZ(void);
void *$s7SwiftUI5ColorV_3red5green4blue7opacityA2C13RGBColorSpaceO_S4dtcfC(int space, double r, double g, double b, double a);
/* modificadores */
extern sui_opaque $s7SwiftUI4ViewPAAE4helpyQrqd__SyRd__lFQOMQ, $s7SwiftUI4ViewPAAE7tabItemyQrqd__yXEAaBRd__lFQOMQ,
    $s7SwiftUI4ViewPAAE9lineLimit_13reservesSpaceQrSi_SbtFQOMQ;
void *$s7SwiftUI4ViewPAAE4helpyQrqd__SyRd__lF(const uint64_t *s);
void *$s7SwiftUI4ViewPAAE7tabItemyQrqd__yXEAaBRd__lF(void *fn, void *ctx);
void *$s7SwiftUI4ViewPAAE9lineLimit_13reservesSpaceQrSi_SbtF(int64_t n, int reserves);
/* estado */
void *$s7SwiftUI7BindingV8constantyACyxGxFZ(const void *value, void *metadata);
uint64_t $s7SwiftUI7BindingV12wrappedValuexvg(void);
void $s7SwiftUI7BindingV12wrappedValuexvs(uint64_t value);
/* formas */
extern sui_desc $s7SwiftUI7CapsuleVMn, $s7SwiftUI21StrokeBorderShapeViewVMn, $s7SwiftUI34_InsettableBackgroundShapeModifierVMn;
extern sui_conf $s7SwiftUI7CapsuleVAA5ShapeAAMc, $s7SwiftUI34_InsettableBackgroundShapeModifierVyxq_GAA04ViewF0AAMc;
void *$s7SwiftUI7CapsuleVMa(void);
void *$s7SwiftUI15InsettableShapePAAE12strokeBorder_9lineWidth11antialiasedQrqd___12CoreGraphics7CGFloatVSbtAA0D5StyleRd__lF(void *content, double width, int aa);
/* ambiente */
int $s7SwiftUI17EnvironmentValuesV18controlActiveStateAA07ControlfG0Ovg(void);
void $s7SwiftUI17EnvironmentValuesV18controlActiveStateAA07ControlfG0Ovs(int v);
void *$s7SwiftUI11EnvironmentVyACyxGxmcRlzC11Observation10ObservableRzlufC(void *type, void *meta, void *wt);
void *$s7SwiftUI4ViewPAAE11environmentyQrqd__SgRld__C11Observation10ObservableRd__lF(void *object, void *meta, void *wt);
uint64_t $s7SwiftUI11EnvironmentV12wrappedValuexvg(void);
/* hosting e views */
extern struct { uint32_t kind; const char *name; } $s7SwiftUI19NSHostingControllerCMn;
void *$s7SwiftUI19NSHostingControllerC8rootViewACyxGx_tcfc(void *root, void *metadata);
extern sui_desc $s7SwiftUI7TabViewVMn;
extern sui_conf $s7SwiftUI7TabViewVyxq_GAA0D0AAMc;
void *$s7SwiftUI7TabViewVAASiRszrlE7contentACySiq_Gq_yXE_tcfC(void *fn, void *ctx);

static int fails;
#define CHECK(c, m) do { if (!(c)) { fprintf(stderr, "FAIL %s\n", m); fails++; } } while (0)

static const sui_desc root_d = { SUI_K_TEXT, "Text" };
static sui_node root = { &root_d, 0, 0, 0, 0, 0, 0, 0, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0, 0, 0}, 0, 0, {0, 0} };
static void *tab_label(void) { return 0; }

static sui_mod *mod_of(void *v) { return v ? ((sui_modified *)v)->mod : 0; }

int main(void) {
    /* ControlActiveState e outros enums */
    CHECK($s7SwiftUI18ControlActiveStateO3keyyA2CmFWC == 0 && $s7SwiftUI18ControlActiveStateO8inactiveyA2CmFWC == 2, "ControlActiveState casos");
    CHECK($s7SwiftUI18ControlActiveStateO2eeoiySbAC_ACtFZ(2, 0x102) && !$s7SwiftUI18ControlActiveStateO2eeoiySbAC_ACtFZ(0, 2), "ControlActiveState ==");
    CHECK($s7SwiftUI18ControlActiveStateOMa() != 0, "ControlActiveState Ma");
    CHECK($s7SwiftUI18RoundedCornerStyleO8circularyA2CmFWC == 0, "RoundedCornerStyle.circular");
    CHECK($s7SwiftUI4TextV14TruncationModeO6middleyA2EmFWC == 2, "TruncationMode.middle");
    {
        pair k = $s7SwiftUI13KeyEquivalentV6returnACvgZ();
        char buf[8];
        sui_unstr(k.w0, k.w1, buf, (int)sizeof buf);
        CHECK(buf[0] == '\r' && buf[1] == 0, "KeyEquivalent.return");
    }
    CHECK($s7SwiftUI14EventModifiersV8rawValueACSi_tcfC(16 | 2) == 18, "EventModifiers(rawValue:)");

    /* estilos */
    CHECK(*(const sui_desc **)$s7SwiftUI21BorderlessButtonStyleVACycfC() == &$s7SwiftUI21BorderlessButtonStyleVMn, "BorderlessButtonStyle()");
    CHECK($s7SwiftUI21BorderlessButtonStyleVMa() == &$s7SwiftUI21BorderlessButtonStyleVMn, "BorderlessButtonStyle Ma");
    CHECK(!strcmp($s7SwiftUI21BorderlessButtonStyleVAA09PrimitivedE0AAMc.proto, "PrimitiveButtonStyle"), "BorderlessButtonStyle Mc");
    CHECK(*(const sui_desc **)$s7SwiftUI25CircularProgressViewStyleVACycfC() == &$s7SwiftUI25CircularProgressViewStyleVMn, "CircularProgressViewStyle()");
    CHECK($s7SwiftUI25CircularProgressViewStyleVMa() == &$s7SwiftUI25CircularProgressViewStyleVMn, "CircularProgressViewStyle Ma");
    CHECK(!strcmp($s7SwiftUI25CircularProgressViewStyleVAA0deF0AAMc.proto, "ProgressViewStyle"), "CircularProgressViewStyle Mc");

    /* Color == */
    {
        void *a = $s7SwiftUI5ColorV_3red5green4blue7opacityA2C13RGBColorSpaceO_S4dtcfC(0, 0.2, 0.4, 0.6, 1);
        void *b = $s7SwiftUI5ColorV_3red5green4blue7opacityA2C13RGBColorSpaceO_S4dtcfC(0, 0.2, 0.4, 0.6, 1);
        void *c = $s7SwiftUI5ColorV_3red5green4blue7opacityA2C13RGBColorSpaceO_S4dtcfC(0, 0.2, 0.4, 0.6, 0.5);
        CHECK($s7SwiftUI5ColorV2eeoiySbAC_ACtFZ(a, b), "Color == mesmos componentes");
        CHECK(!$s7SwiftUI5ColorV2eeoiySbAC_ACtFZ(a, c), "Color != opacidade");
        CHECK($s7SwiftUI5ColorV2eeoiySbAC_ACtFZ($s7SwiftUI5ColorV3redACvgZ(), $s7SwiftUI5ColorV3redACvgZ()), "Color.red == Color.red");
        CHECK(!$s7SwiftUI5ColorV2eeoiySbAC_ACtFZ($s7SwiftUI5ColorV3redACvgZ(), $s7SwiftUI5ColorV4blueACvgZ()), "Color.red != Color.blue");
    }

    /* modificadores */
    {
        uint64_t s[2];
        sui_mod *m;
        memcpy(s, "Conectar\0\0\0\0\0\0\0", 15);
        s[1] = (s[1] & 0x00FFFFFFFFFFFFFFull) | (0xE8ull << 56);
        arm(&root);
        m = mod_of($s7SwiftUI4ViewPAAE4helpyQrqd__SyRd__lF(s));
        CHECK(m && m->op == SUI_OP_HELP && m->p[1] && !strcmp((const char *)m->p[1], "Conectar"), "help<S>");
        arm(&root);
        m = mod_of($s7SwiftUI4ViewPAAE7tabItemyQrqd__yXEAaBRd__lF((void *)tab_label, (void *)0x55));
        CHECK(m && m->op == SUI_OP_TABITEM && m->fn.fn == (void *)tab_label && m->fn.ctx == (void *)0x55, "tabItem");
        arm(&root);
        m = mod_of($s7SwiftUI4ViewPAAE9lineLimit_13reservesSpaceQrSi_SbtF(3, 1));
        CHECK(m && m->op == SUI_OP_LINELIMIT && m->u[0] == 3 && m->u[1] == 0 && m->u[2] == 1, "lineLimit(_:reservesSpace:)");
        arm(&root);
        m = mod_of($s7SwiftUI4ViewPAAE9lineLimit_13reservesSpaceQrSi_SbtF(0, 0));
        CHECK(m && m->u[0] == 1 && m->u[2] == 0, "lineLimit mínimo 1");
        CHECK(!strcmp($s7SwiftUI4ViewPAAE4helpyQrqd__SyRd__lFQOMQ.name, "help") &&
              !strcmp($s7SwiftUI4ViewPAAE7tabItemyQrqd__yXEAaBRd__lFQOMQ.name, "tabItem") &&
              !strcmp($s7SwiftUI4ViewPAAE9lineLimit_13reservesSpaceQrSi_SbtFQOMQ.name, "lineLimit"), "QOMQ");
    }

    /* Binding.constant ignora a escrita */
    {
        uint64_t v = 42;
        binding *b = (binding *)$s7SwiftUI7BindingV8constantyACyxGxFZ(&v, 0);
        CHECK(b && b->readonly, "Binding.constant");
        arm(b);
        CHECK($s7SwiftUI7BindingV12wrappedValuexvg() == 42, "Binding.constant lê");
        arm(b);
        $s7SwiftUI7BindingV12wrappedValuexvs(7);
        arm(b);
        CHECK($s7SwiftUI7BindingV12wrappedValuexvg() == 42 && v == 42, "Binding.constant não grava");
    }

    /* Capsule e formas */
    {
        void *(*capsule)(double, double, double, double) = (void *(*)(double, double, double, double))$s7SwiftUI7CapsuleVAA5ShapeAAMc.body;
        CHECK($s7SwiftUI7CapsuleVMa() == &$s7SwiftUI7CapsuleVMn, "Capsule Ma");
        CHECK(!strcmp($s7SwiftUI7CapsuleVAA5ShapeAAMc.proto, "Shape") && capsule && capsule(0, 0, 80, 20) != 0, "Capsule path");
        shapev *sv;
        arm(&root);
        sv = (shapev *)$s7SwiftUI15InsettableShapePAAE12strokeBorder_9lineWidth11antialiasedQrqd___12CoreGraphics7CGFloatVSbtAA0D5StyleRd__lF((void *)1, 2, 1);
        CHECK(sv && sv->isa == &$s7SwiftUI21StrokeBorderShapeViewVMn && (sv->flags & 4), "strokeBorder usa StrokeBorderShapeView");
        CHECK(!strcmp($s7SwiftUI34_InsettableBackgroundShapeModifierVMn.name, "_InsettableBackgroundShapeModifier") &&
              !strcmp($s7SwiftUI34_InsettableBackgroundShapeModifierVyxq_GAA04ViewF0AAMc.proto, "ViewModifier"), "_InsettableBackgroundShapeModifier");
    }

    /* ambiente: controlActiveState e objeto @Observable */
    {
        static int obj, meta, other;
        void *env;
        arm((void *)0);
        CHECK($s7SwiftUI17EnvironmentValuesV18controlActiveStateAA07ControlfG0Ovg() == 0, "controlActiveState padrão key");
        arm((void *)0);
        $s7SwiftUI17EnvironmentValuesV18controlActiveStateAA07ControlfG0Ovs(2);
        arm((void *)0);
        CHECK($s7SwiftUI17EnvironmentValuesV18controlActiveStateAA07ControlfG0Ovg() == 2, "controlActiveState grava");
        arm(&root);
        CHECK($s7SwiftUI4ViewPAAE11environmentyQrqd__SgRld__C11Observation10ObservableRd__lF(&obj, &meta, 0) == &root, "environment(obj) devolve a view");
        env = $s7SwiftUI11EnvironmentVyACyxGxmcRlzC11Observation10ObservableRzlufC(&meta, &meta, 0);
        arm(env);
        CHECK($s7SwiftUI11EnvironmentV12wrappedValuexvg() == (uint64_t)(uintptr_t)&obj, "@Environment(T.self) acha o objeto");
        env = $s7SwiftUI11EnvironmentVyACyxGxmcRlzC11Observation10ObservableRzlufC(&other, &other, 0);
        arm(env);
        CHECK($s7SwiftUI11EnvironmentV12wrappedValuexvg() == 0, "@Environment de outro tipo vazio");
    }

    /* NSHostingController e TabView */
    {
        hc *c = (hc *)$s7SwiftUI19NSHostingControllerC8rootViewACyxGx_tcfc(&root, 0);
        CHECK(c && c->isa == (const void *)&$s7SwiftUI19NSHostingControllerCMn && c->root == &root && c->view, "NSHostingController(rootView:)");
        sui_node *t = (sui_node *)$s7SwiftUI7TabViewVAASiRszrlE7contentACySiq_Gq_yXE_tcfC((void *)tab_label, (void *)0x77);
        CHECK(t && t->isa == &$s7SwiftUI7TabViewVMn && t->content.fn == (void *)tab_label && t->content.ctx == (void *)0x77, "TabView(content:)");
        CHECK($s7SwiftUI7TabViewVMn.kind == SUI_K_TABVIEW && !strcmp($s7SwiftUI7TabViewVyxq_GAA0D0AAMc.proto, "View"), "TabView Mn/Mc");
    }

    if (fails) { fprintf(stderr, "RESULT FAIL %d\n", fails); return 1; }
    printf("RESULT PASS\n");
    return 0;
}
