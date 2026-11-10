/* Modificadores: ModifiedContent, padding/frame, onAppear, onChange, toque,
 * atalho de teclado, sheet com Binding e DismissAction, acessibilidade.
 * clang -std=c11 -Wall -Wextra -Werror -DSWIFTUI_MOD_HOST -DSWIFTUI_STATE_HOST -DSWIFTUI_VALUES_HOST \
 *   userland/libsystem/swiftui_modifiers.c userland/libsystem/swiftui_state.c \
 *   userland/libsystem/swiftui_values.c userland/tests-swiftui/modifiers_test.c -o /tmp/sui_mod
 */
#define SUI_HOST 1
#include <stdio.h>
#include "../libsystem/swiftui_model.h"

extern sui_desc $s7SwiftUI15ModifiedContentVMn, $s7SwiftUI14_PaddingLayoutVMn, $s7SwiftUI12_FrameLayoutVMn,
    $s7SwiftUI16_FlexFrameLayoutVMn, $s7SwiftUI25_AppearanceActionModifierVMn, $s7SwiftUI14_OpacityEffectVMn;
extern sui_conf $s7SwiftUI15ModifiedContentVyxq_GAA4ViewA2aERzAA0E8ModifierR_rlMc, $s7SwiftUI14_PaddingLayoutVAA12ViewModifierAAMc;
extern sui_wtab $s7SwiftUI14_PaddingLayoutVAA12ViewModifierAAWP;
extern sui_meta $s7SwiftUI14_PaddingLayoutVN;
extern sui_opaque $s7SwiftUI4ViewPAAE5sheet11isPresented9onDismiss7contentQrAA7BindingVySbG_yycSgqd__yctAaBRd__lFQOMQ;

void *$s7SwiftUI4ViewPAAE7paddingyQr12CoreGraphics7CGFloatVF(double len);
void *$s7SwiftUI4ViewPAAE7paddingyQrAA4EdgeO3SetV_12CoreGraphics7CGFloatVSgtF(int edges, double len, int is_nil);
void *$s7SwiftUI4ViewPAAE5frame5width6height9alignmentQr12CoreGraphics7CGFloatVSg_AkA9AlignmentVtF(double w, int wn, double h, int hn, int64_t ah, int64_t av);
void *$s7SwiftUI4ViewPAAE5frame8minWidth05idealF003maxF00E6Height0gI00hI09alignmentQr12CoreGraphics7CGFloatVSg_A5oA9AlignmentVtF(
    double a, int an, double b, int bn, double c, int cn, double d, int dn, double e, int en, double f, int fn, int64_t ah, int64_t av);
void *$s7SwiftUI4ViewPAAE7opacityyQrSdF(double op);
void *$s7SwiftUI4ViewPAAE8onAppear7performQryycSg_tF(void *fn, void *ctx);
void *$s7SwiftUI4ViewPAAE8onChange2of7initial_Qrqd___Sbyqd___qd__tctSQRd__lF(const uint64_t *value, int initial, void *fn, void *ctx);
void *$s7SwiftUI4ViewPAAE12onTapGesture5count7performQrSi_yyctF(int64_t count, void *fn, void *ctx);
void *$s7SwiftUI4ViewPAAE8disabledyQrSbF(int on);
void *$s7SwiftUI4ViewPAAE16keyboardShortcut_9modifiersQrAA13KeyEquivalentV_AA14EventModifiersVtF(uint64_t w0, uint64_t w1, uint64_t mods);
void *$s7SwiftUI4ViewPAAE16keyboardShortcutyQrAA08KeyboardE0VSgF(const void *sc);
void *$s7SwiftUI16KeyboardShortcutV13defaultActionACvgZ(void);
uint64_t $s7SwiftUI14EventModifiersV7commandACvgZ(void);
void *$s7SwiftUI4ViewPAAE5sheet11isPresented9onDismiss7contentQrAA7BindingVySbG_yycSgqd__yctAaBRd__lF(void *b, void *dfn, void *dctx, void *cfn, void *cctx);
void *$s7SwiftUI17EnvironmentValuesV7dismissAA13DismissActionVvg(void);
void $s7SwiftUI13DismissActionV14callAsFunctionyyF(void);
void *$s7SwiftUI4ViewPAAE18accessibilityLabelyAA15ModifiedContentVyxAA31AccessibilityAttachmentModifierVGqd__SyRd__lF(const uint64_t *s);
void *$s7SwiftUI15ModifiedContentVA2A31AccessibilityAttachmentModifierVRs_rlE22accessibilityAddTraitsyACyxAEGAA0eJ0VF(uint64_t traits);
void *$s7SwiftUI5StateV12wrappedValueACyxGx_tcfC(uint64_t value, void *metadata);
void *$s7SwiftUI5StateV14projectedValueAA7BindingVyxGvg(void);
void $s7SwiftUI5StateV12wrappedValuexvs(uint64_t value);
void *mos_sui_find_modifier(void *view, int op);
int mos_sui_tap(void *view, int count);
void *mos_sui_shortcut(void *view, const char *key, uint64_t mods);

#define arm(p) __asm__ volatile("mov %0, %%r13" :: "r"(p) : "r13", "memory")

static int appear, taps, dismissed, built;
static uint64_t seen_old, seen_new;
static int changes;
static void on_appear(void) { appear++; }
static void on_tap(void) { taps++; }
static void on_dismiss(void) { dismissed++; }
static void *sheet_content(void) { void *c; __asm__ volatile("mov %%r13, %0" : "=r"(c)); built++; return c; }
static void on_change(const uint64_t *o, const uint64_t *n) { seen_old = *o; seen_new = *n; changes++; }

typedef void *(*body_fn)(void);
static void *body(void *v) {
    body_fn f = (body_fn)$s7SwiftUI15ModifiedContentVyxq_GAA4ViewA2aERzAA0E8ModifierR_rlMc.body;
    arm(v);
    return f();
}

static void swstr_small(const char *s, uint64_t *w) {
    unsigned char b[16] = { 0 };
    int n = (int)strlen(s);
    memcpy(b, s, (size_t)n);
    b[15] = (unsigned char)(0xE0 | n);
    memcpy(&w[0], b, 8);
    memcpy(&w[1], b + 8, 8);
}

int main(void) {
    int fails = 0;
    sui_node text;
    sui_modified *v, *v2, *v3;
    char sheet_mark = 's';
    uint64_t val, w[2];
    void *state, *bind;
    memset(&text, 0, sizeof text);

    arm(&text);
    v = $s7SwiftUI4ViewPAAE7paddingyQr12CoreGraphics7CGFloatVF(10);
    if (!v || v->isa != &$s7SwiftUI15ModifiedContentVMn || v->content != &text || v->mod->isa != &$s7SwiftUI14_PaddingLayoutVMn ||
        v->mod->u[0] != 15 || v->mod->d[0] != 10) { fprintf(stderr, "FAIL padding\n"); fails++; }
    arm(v);
    v2 = $s7SwiftUI4ViewPAAE7paddingyQrAA4EdgeO3SetV_12CoreGraphics7CGFloatVSgtF(1 | 4, 0, 1);
    if (!v2 || v2->content != v || v2->mod->u[0] != 5 || v2->mod->d[0] != 16) { fprintf(stderr, "FAIL padding(edges, nil)\n"); fails++; }
    if (body(v2) != v) { fprintf(stderr, "FAIL body devolve o conteúdo\n"); fails++; }
    arm(&text);
    v = $s7SwiftUI4ViewPAAE5frame5width6height9alignmentQr12CoreGraphics7CGFloatVSg_AkA9AlignmentVtF(100, 0, 0, 1, -1, 0);
    if (!v || v->mod->isa != &$s7SwiftUI12_FrameLayoutVMn || v->mod->d[0] != 100 || v->mod->u[0] != 2 || (int64_t)v->mod->u[1] != -1) {
        fprintf(stderr, "FAIL frame\n"); fails++;
    }
    arm(&text);
    v = $s7SwiftUI4ViewPAAE5frame8minWidth05idealF003maxF00E6Height0gI00hI09alignmentQr12CoreGraphics7CGFloatVSg_A5oA9AlignmentVtF(
        200, 0, 0, 1, 900, 0, 0, 1, 0, 1, 600, 0, 1, 1);
    if (!v || v->mod->isa != &$s7SwiftUI16_FlexFrameLayoutVMn || v->mod->d[0] != 200 || v->mod->d[2] != 900 || v->mod->d[5] != 600 ||
        v->mod->u[0] != (2 | 8 | 16) || v->mod->u[1] != 1 || v->mod->u[2] != 1) { fprintf(stderr, "FAIL frame flexível\n"); fails++; }
    arm(&text);
    v = $s7SwiftUI4ViewPAAE7opacityyQrSdF(1.5);
    if (!v || v->mod->isa != &$s7SwiftUI14_OpacityEffectVMn || v->mod->d[0] != 1) { fprintf(stderr, "FAIL opacity\n"); fails++; }

    arm(&text);
    v = $s7SwiftUI4ViewPAAE8onAppear7performQryycSg_tF((void *)on_appear, 0);
    body(v); body(v);
    if (appear != 1 || v->mod->isa != &$s7SwiftUI25_AppearanceActionModifierVMn) { fprintf(stderr, "FAIL onAppear %d\n", appear); fails++; }

    val = 5;
    arm(&text);
    v = $s7SwiftUI4ViewPAAE8onChange2of7initial_Qrqd___Sbyqd___qd__tctSQRd__lF(&val, 1, (void *)on_change, 0);
    body(v);
    if (changes != 1 || seen_old != 5 || seen_new != 5) { fprintf(stderr, "FAIL onChange initial\n"); fails++; }
    body(v);
    if (changes != 1) { fprintf(stderr, "FAIL onChange sem mudança\n"); fails++; }
    val = 7;
    arm(&text);
    v = $s7SwiftUI4ViewPAAE8onChange2of7initial_Qrqd___Sbyqd___qd__tctSQRd__lF(&val, 1, (void *)on_change, 0);
    body(v);
    if (changes != 2 || seen_old != 5 || seen_new != 7) { fprintf(stderr, "FAIL onChange 5->7\n"); fails++; }

    arm(&text);
    v = $s7SwiftUI4ViewPAAE12onTapGesture5count7performQrSi_yyctF(2, (void *)on_tap, 0);
    if (mos_sui_tap(v, 1) || !mos_sui_tap(v, 2) || taps != 1) { fprintf(stderr, "FAIL onTapGesture\n"); fails++; }
    arm(v);
    v2 = $s7SwiftUI4ViewPAAE8disabledyQrSbF(1);
    if (mos_sui_tap(v2, 2) || taps != 1) { fprintf(stderr, "FAIL disabled bloqueia o toque\n"); fails++; }
    if (!mos_sui_find_modifier(v2, SUI_OP_TAP) || mos_sui_find_modifier(v2, SUI_OP_SHEET)) { fprintf(stderr, "FAIL find_modifier\n"); fails++; }

    swstr_small("k", w);
    arm(&text);
    v = $s7SwiftUI4ViewPAAE16keyboardShortcut_9modifiersQrAA13KeyEquivalentV_AA14EventModifiersVtF(w[0], w[1], $s7SwiftUI14EventModifiersV7commandACvgZ());
    if (mos_sui_shortcut(v, "k", 16) != &text || mos_sui_shortcut(v, "k", 0)) { fprintf(stderr, "FAIL keyboardShortcut\n"); fails++; }
    arm(&text);
    v = $s7SwiftUI4ViewPAAE16keyboardShortcutyQrAA08KeyboardE0VSgF($s7SwiftUI16KeyboardShortcutV13defaultActionACvgZ());
    if (mos_sui_shortcut(v, "\r", 0) != &text) { fprintf(stderr, "FAIL defaultAction\n"); fails++; }

    state = $s7SwiftUI5StateV12wrappedValueACyxGx_tcfC(0, 0);
    arm(state);
    bind = $s7SwiftUI5StateV14projectedValueAA7BindingVyxGvg();
    arm(&text);
    v = $s7SwiftUI4ViewPAAE5sheet11isPresented9onDismiss7contentQrAA7BindingVySbG_yycSgqd__yctAaBRd__lF(bind, (void *)on_dismiss, 0, (void *)sheet_content, &sheet_mark);
    body(v);
    if (built || v->mod->fired) { fprintf(stderr, "FAIL sheet fechado\n"); fails++; }
    arm(state);
    $s7SwiftUI5StateV12wrappedValuexvs(1);
    body(v);
    body(v);
    if (built != 1 || !v->mod->fired || v->mod->p[1] != &sheet_mark) { fprintf(stderr, "FAIL sheet aberto\n"); fails++; }
    arm($s7SwiftUI17EnvironmentValuesV7dismissAA13DismissActionVvg());
    $s7SwiftUI13DismissActionV14callAsFunctionyyF();
    if (((uint64_t *)state)[1] != 0 || dismissed != 1 || v->mod->fired) { fprintf(stderr, "FAIL dismiss\n"); fails++; }
    if (strcmp($s7SwiftUI4ViewPAAE5sheet11isPresented9onDismiss7contentQrAA7BindingVySbG_yycSgqd__yctAaBRd__lFQOMQ.name, "sheet") != 0) {
        fprintf(stderr, "FAIL descritor opaco\n"); fails++;
    }

    swstr_small("Enviar", w);
    arm(&text);
    v = $s7SwiftUI4ViewPAAE18accessibilityLabelyAA15ModifiedContentVyxAA31AccessibilityAttachmentModifierVGqd__SyRd__lF(w);
    arm(v);
    v3 = $s7SwiftUI15ModifiedContentVA2A31AccessibilityAttachmentModifierVRs_rlE22accessibilityAddTraitsyACyxAEGAA0eJ0VF(1);
    if (!v3 || strcmp((char *)v3->mod->p[1], "Enviar") != 0 || v3->mod->u[3] != 1 || v->mod->u[3] != 0 || v3->content != &text) {
        fprintf(stderr, "FAIL acessibilidade\n"); fails++;
    }

    if ($s7SwiftUI14_PaddingLayoutVAA12ViewModifierAAWP.conf != &$s7SwiftUI14_PaddingLayoutVAA12ViewModifierAAMc ||
        $s7SwiftUI14_PaddingLayoutVN.desc != &$s7SwiftUI14_PaddingLayoutVMn ||
        strcmp($s7SwiftUI14_PaddingLayoutVAA12ViewModifierAAMc.proto, "ViewModifier") != 0) {
        fprintf(stderr, "FAIL dados do _PaddingLayout\n"); fails++;
    }
    {
        void *(*mb)(void *) = (void *(*)(void *))$s7SwiftUI14_PaddingLayoutVAA12ViewModifierAAMc.body;
        if (mb(&text) != &text) { fprintf(stderr, "FAIL ViewModifier.body\n"); fails++; }
    }
    if (fails) { fprintf(stderr, "RESULT FAIL %d\n", fails); return 1; }
    printf("RESULT PASS\n");
    return 0;
}
