/* Modificadores de View: cada um devolve um ModifiedContent { self, modificador }
 * com o tipo do modificador do SwiftUI (_PaddingLayout, _OpacityEffect...).
 * O body do ModifiedContent é o momento em que a view aparece: roda o
 * onAppear uma vez, compara o valor do onChange com o último visto e
 * mostra/esconde o conteúdo de sheet/alert/popover conforme o Binding.
 * self em r13; closures como par (função, contexto).
 */
#ifdef SWIFTUI_MOD_HOST
#define SUI_HOST 1
#endif
#include "swiftui_model.h"

uint64_t $s7SwiftUI7BindingV12wrappedValuexvg(void);
void $s7SwiftUI7BindingV12wrappedValuexvs(uint64_t value);

static void *sui_modifier_body(void *content) { return content; }
static void *sui_content_body(void) { return 0; }
static void *sui_modified_body(void);

/* ---- Tipos de modificador (descritores, metadados, conformidades) ---- */

sui_desc $s7SwiftUI11_BlurEffectVMn = { SUI_K_MODIFIER, "_BlurEffect" };
sui_meta $s7SwiftUI11_BlurEffectVN = { &$s7SwiftUI11_BlurEffectVMn };
sui_conf $s7SwiftUI11_BlurEffectVAA12ViewModifierAAMc = { (void *)sui_modifier_body, "ViewModifier" };

sui_desc $s7SwiftUI11_ClipEffectVMn = { SUI_K_MODIFIER, "_ClipEffect" };
sui_conf $s7SwiftUI11_ClipEffectVyxGAA12ViewModifierAAMc = { (void *)sui_modifier_body, "ViewModifier" };

sui_desc $s7SwiftUI11_MaskEffectVMn = { SUI_K_MODIFIER, "_MaskEffect" };
sui_conf $s7SwiftUI11_MaskEffectVyxGAA12ViewModifierAAMc = { (void *)sui_modifier_body, "ViewModifier" };

sui_desc $s7SwiftUI12_FrameLayoutVMn = { SUI_K_MODIFIER, "_FrameLayout" };
sui_meta $s7SwiftUI12_FrameLayoutVN = { &$s7SwiftUI12_FrameLayoutVMn };
sui_conf $s7SwiftUI12_FrameLayoutVAA12ViewModifierAAMc = { (void *)sui_modifier_body, "ViewModifier" };
sui_wtab $s7SwiftUI12_FrameLayoutVAA12ViewModifierAAWP = { &$s7SwiftUI12_FrameLayoutVAA12ViewModifierAAMc, (void *)sui_modifier_body };

sui_desc $s7SwiftUI12_ScaleEffectVMn = { SUI_K_MODIFIER, "_ScaleEffect" };
static sui_conf sui_conf_scaleeffect = { (void *)sui_modifier_body, "ViewModifier" };
sui_wtab $s7SwiftUI12_ScaleEffectVAA12ViewModifierAAWP = { &sui_conf_scaleeffect, (void *)sui_modifier_body };

sui_desc $s7SwiftUI13_OffsetEffectVMn = { SUI_K_MODIFIER, "_OffsetEffect" };
sui_meta $s7SwiftUI13_OffsetEffectVN = { &$s7SwiftUI13_OffsetEffectVMn };
static sui_conf sui_conf_offseteffect = { (void *)sui_modifier_body, "ViewModifier" };
sui_wtab $s7SwiftUI13_OffsetEffectVAA12ViewModifierAAWP = { &sui_conf_offseteffect, (void *)sui_modifier_body };

sui_desc $s7SwiftUI13_ShadowEffectVMn = { SUI_K_MODIFIER, "_ShadowEffect" };
static sui_conf sui_conf_shadoweffect = { (void *)sui_modifier_body, "ViewModifier" };
sui_wtab $s7SwiftUI13_ShadowEffectVAA12ViewModifierAAWP = { &sui_conf_shadoweffect, (void *)sui_modifier_body };

sui_desc $s7SwiftUI13_TaskModifierVMn = { SUI_K_MODIFIER, "_TaskModifier" };
void *$s7SwiftUI13_TaskModifierVMa(void) { return &$s7SwiftUI13_TaskModifierVMn; }
sui_conf $s7SwiftUI13_TaskModifierVAA04ViewD0AAMc = { (void *)sui_modifier_body, "ViewModifier" };

sui_desc $s7SwiftUI14_OpacityEffectVMn = { SUI_K_MODIFIER, "_OpacityEffect" };
sui_meta $s7SwiftUI14_OpacityEffectVN = { &$s7SwiftUI14_OpacityEffectVMn };
sui_conf $s7SwiftUI14_OpacityEffectVAA12ViewModifierAAMc = { (void *)sui_modifier_body, "ViewModifier" };
sui_wtab $s7SwiftUI14_OpacityEffectVAA12ViewModifierAAWP = { &$s7SwiftUI14_OpacityEffectVAA12ViewModifierAAMc, (void *)sui_modifier_body };

sui_desc $s7SwiftUI14_PaddingLayoutVMn = { SUI_K_MODIFIER, "_PaddingLayout" };
sui_meta $s7SwiftUI14_PaddingLayoutVN = { &$s7SwiftUI14_PaddingLayoutVMn };
sui_conf $s7SwiftUI14_PaddingLayoutVAA12ViewModifierAAMc = { (void *)sui_modifier_body, "ViewModifier" };
sui_wtab $s7SwiftUI14_PaddingLayoutVAA12ViewModifierAAWP = { &$s7SwiftUI14_PaddingLayoutVAA12ViewModifierAAMc, (void *)sui_modifier_body };

sui_desc $s7SwiftUI15_HiddenModifierVMn = { SUI_K_MODIFIER, "_HiddenModifier" };
sui_meta $s7SwiftUI15_HiddenModifierVN = { &$s7SwiftUI15_HiddenModifierVMn };
sui_conf $s7SwiftUI15_HiddenModifierVAA04ViewD0AAMc = { (void *)sui_modifier_body, "ViewModifier" };

sui_desc $s7SwiftUI15_PositionLayoutVMn = { SUI_K_MODIFIER, "_PositionLayout" };

sui_desc $s7SwiftUI15_RotationEffectVMn = { SUI_K_MODIFIER, "_RotationEffect" };
static sui_conf sui_conf_rotationeffect = { (void *)sui_modifier_body, "ViewModifier" };
sui_wtab $s7SwiftUI15_RotationEffectVAA12ViewModifierAAWP = { &sui_conf_rotationeffect, (void *)sui_modifier_body };

sui_desc $s7SwiftUI16_BlendModeEffectVMn = { SUI_K_MODIFIER, "_BlendModeEffect" };
void *$s7SwiftUI16_BlendModeEffectVMa(void) { return &$s7SwiftUI16_BlendModeEffectVMn; }
static sui_conf sui_conf_blendmodeeffect = { (void *)sui_modifier_body, "ViewModifier" };
sui_wtab $s7SwiftUI16_BlendModeEffectVAA12ViewModifierAAWP = { &sui_conf_blendmodeeffect, (void *)sui_modifier_body };

sui_desc $s7SwiftUI16_FixedSizeLayoutVMn = { SUI_K_MODIFIER, "_FixedSizeLayout" };
sui_meta $s7SwiftUI16_FixedSizeLayoutVN = { &$s7SwiftUI16_FixedSizeLayoutVMn };
sui_conf $s7SwiftUI16_FixedSizeLayoutVAA12ViewModifierAAMc = { (void *)sui_modifier_body, "ViewModifier" };
sui_wtab $s7SwiftUI16_FixedSizeLayoutVAA12ViewModifierAAWP = { &$s7SwiftUI16_FixedSizeLayoutVAA12ViewModifierAAMc, (void *)sui_modifier_body };

sui_desc $s7SwiftUI16_FlexFrameLayoutVMn = { SUI_K_MODIFIER, "_FlexFrameLayout" };
sui_meta $s7SwiftUI16_FlexFrameLayoutVN = { &$s7SwiftUI16_FlexFrameLayoutVMn };
sui_conf $s7SwiftUI16_FlexFrameLayoutVAA12ViewModifierAAMc = { (void *)sui_modifier_body, "ViewModifier" };
sui_wtab $s7SwiftUI16_FlexFrameLayoutVAA12ViewModifierAAWP = { &$s7SwiftUI16_FlexFrameLayoutVAA12ViewModifierAAMc, (void *)sui_modifier_body };

sui_desc $s7SwiftUI16_OverlayModifierVMn = { SUI_K_MODIFIER, "_OverlayModifier" };
void *$s7SwiftUI16_OverlayModifierVMa(void) { return &$s7SwiftUI16_OverlayModifierVMn; }
sui_conf $s7SwiftUI16_OverlayModifierVyxGAA04ViewD0AAMc = { (void *)sui_modifier_body, "ViewModifier" };

sui_desc $s7SwiftUI17_BrightnessEffectVMn = { SUI_K_MODIFIER, "_BrightnessEffect" };

sui_desc $s7SwiftUI17_SaturationEffectVMn = { SUI_K_MODIFIER, "_SaturationEffect" };

sui_desc $s7SwiftUI18_AnimationModifierVMn = { SUI_K_MODIFIER, "_AnimationModifier" };
sui_conf $s7SwiftUI18_AnimationModifierVyxGAA04ViewD0AAMc = { (void *)sui_modifier_body, "ViewModifier" };

sui_desc $s7SwiftUI18_AspectRatioLayoutVMn = { SUI_K_MODIFIER, "_AspectRatioLayout" };
sui_meta $s7SwiftUI18_AspectRatioLayoutVN = { &$s7SwiftUI18_AspectRatioLayoutVMn };
sui_conf $s7SwiftUI18_AspectRatioLayoutVAA12ViewModifierAAMc = { (void *)sui_modifier_body, "ViewModifier" };
sui_wtab $s7SwiftUI18_AspectRatioLayoutVAA12ViewModifierAAWP = { &$s7SwiftUI18_AspectRatioLayoutVAA12ViewModifierAAMc, (void *)sui_modifier_body };

sui_desc $s7SwiftUI18_TaskValueModifierVMn = { SUI_K_MODIFIER, "_TaskValueModifier" };
sui_conf $s7SwiftUI18_TaskValueModifierVyxGAA04ViewE0AAMc = { (void *)sui_modifier_body, "ViewModifier" };

sui_desc $s7SwiftUI19_BackgroundModifierVMn = { SUI_K_MODIFIER, "_BackgroundModifier" };
sui_conf $s7SwiftUI19_BackgroundModifierVyxGAA04ViewD0AAMc = { (void *)sui_modifier_body, "ViewModifier" };

sui_desc $s7SwiftUI20_HoverRegionModifierVMn = { SUI_K_MODIFIER, "_HoverRegionModifier" };
sui_meta $s7SwiftUI20_HoverRegionModifierVN = { &$s7SwiftUI20_HoverRegionModifierVMn };
sui_conf $s7SwiftUI20_HoverRegionModifierVAA04ViewE0AAMc = { (void *)sui_modifier_body, "ViewModifier" };

sui_desc $s7SwiftUI20_MaskAlignmentEffectVMn = { SUI_K_MODIFIER, "_MaskAlignmentEffect" };
void *$s7SwiftUI20_MaskAlignmentEffectVMa(void) { return &$s7SwiftUI20_MaskAlignmentEffectVMn; }
sui_conf $s7SwiftUI20_MaskAlignmentEffectVyxGAA12ViewModifierAAMc = { (void *)sui_modifier_body, "ViewModifier" };

sui_desc $s7SwiftUI20_ValueActionModifierVMn = { SUI_K_MODIFIER, "_ValueActionModifier" };
void *$s7SwiftUI20_ValueActionModifierVMa(void) { return &$s7SwiftUI20_ValueActionModifierVMn; }
sui_conf $s7SwiftUI20_ValueActionModifierVyxGAA04ViewE0AAMc = { (void *)sui_modifier_body, "ViewModifier" };

sui_desc $s7SwiftUI21_ContentShapeModifierVMn = { SUI_K_MODIFIER, "_ContentShapeModifier" };
sui_conf $s7SwiftUI21_ContentShapeModifierVyxGAA04ViewE0AAMc = { (void *)sui_modifier_body, "ViewModifier" };

sui_desc $s7SwiftUI21_OverlayStyleModifierVMn = { SUI_K_MODIFIER, "_OverlayStyleModifier" };
sui_conf $s7SwiftUI21_OverlayStyleModifierVyxGAA04ViewE0AAMc = { (void *)sui_modifier_body, "ViewModifier" };

sui_desc $s7SwiftUI21_TraitWritingModifierVMn = { SUI_K_MODIFIER, "_TraitWritingModifier" };
void *$s7SwiftUI21_TraitWritingModifierVMa(void) { return &$s7SwiftUI21_TraitWritingModifierVMn; }
sui_conf $s7SwiftUI21_TraitWritingModifierVyxGAA04ViewE0AAMc = { (void *)sui_modifier_body, "ViewModifier" };

sui_desc $s7SwiftUI21_ViewModifier_ContentVMn = { SUI_K_MODIFIER, "_ViewModifier_Content" };
void *$s7SwiftUI21_ViewModifier_ContentVMa(void) { return &$s7SwiftUI21_ViewModifier_ContentVMn; }
sui_conf $s7SwiftUI21_ViewModifier_ContentVyxGAA0C0AAMc = { (void *)sui_content_body, "View" };

sui_desc $s7SwiftUI23_CompositingGroupEffectVMn = { SUI_K_MODIFIER, "_CompositingGroupEffect" };
static sui_conf sui_conf_compositinggroupeffect = { (void *)sui_modifier_body, "ViewModifier" };
sui_wtab $s7SwiftUI23_CompositingGroupEffectVAA12ViewModifierAAWP = { &sui_conf_compositinggroupeffect, (void *)sui_modifier_body };

sui_desc $s7SwiftUI24_BackgroundStyleModifierVMn = { SUI_K_MODIFIER, "_BackgroundStyleModifier" };
sui_conf $s7SwiftUI24_BackgroundStyleModifierVyxGAA04ViewE0AAMc = { (void *)sui_modifier_body, "ViewModifier" };

sui_desc $s7SwiftUI24_CoordinateSpaceModifierVMn = { SUI_K_MODIFIER, "_CoordinateSpaceModifier" };
sui_conf $s7SwiftUI24_CoordinateSpaceModifierVyxGAA04ViewE0AAMc = { (void *)sui_modifier_body, "ViewModifier" };

sui_desc $s7SwiftUI24_ForegroundStyleModifierVMn = { SUI_K_MODIFIER, "_ForegroundStyleModifier" };
sui_conf $s7SwiftUI24_ForegroundStyleModifierVyxGAA04ViewE0AAMc = { (void *)sui_modifier_body, "ViewModifier" };

sui_desc $s7SwiftUI25_AllowsHitTestingModifierVMn = { SUI_K_MODIFIER, "_AllowsHitTestingModifier" };
sui_meta $s7SwiftUI25_AllowsHitTestingModifierVN = { &$s7SwiftUI25_AllowsHitTestingModifierVMn };
static sui_conf sui_conf_allowshittestingmodifier = { (void *)sui_modifier_body, "ViewModifier" };
sui_wtab $s7SwiftUI25_AllowsHitTestingModifierVAA04ViewF0AAWP = { &sui_conf_allowshittestingmodifier, (void *)sui_modifier_body };

sui_desc $s7SwiftUI25_AppearanceActionModifierVMn = { SUI_K_MODIFIER, "_AppearanceActionModifier" };
sui_meta $s7SwiftUI25_AppearanceActionModifierVN = { &$s7SwiftUI25_AppearanceActionModifierVMn };
sui_conf $s7SwiftUI25_AppearanceActionModifierVAA04ViewE0AAMc = { (void *)sui_modifier_body, "ViewModifier" };
sui_wtab $s7SwiftUI25_AppearanceActionModifierVAA04ViewE0AAWP = { &$s7SwiftUI25_AppearanceActionModifierVAA04ViewE0AAMc, (void *)sui_modifier_body };

sui_desc $s7SwiftUI30_EnvironmentKeyWritingModifierVMn = { SUI_K_MODIFIER, "_EnvironmentKeyWritingModifier" };
sui_conf $s7SwiftUI30_EnvironmentKeyWritingModifierVyxGAA04ViewF0AAMc = { (void *)sui_modifier_body, "ViewModifier" };

sui_desc $s7SwiftUI30_SafeAreaRegionsIgnoringLayoutVMn = { SUI_K_MODIFIER, "_SafeAreaRegionsIgnoringLayout" };
static sui_conf sui_conf_safearearegionsignoringlayout = { (void *)sui_modifier_body, "ViewModifier" };
sui_wtab $s7SwiftUI30_SafeAreaRegionsIgnoringLayoutVAA12ViewModifierAAWP = { &sui_conf_safearearegionsignoringlayout, (void *)sui_modifier_body };

sui_desc $s7SwiftUI31AccessibilityAttachmentModifierVMn = { SUI_K_MODIFIER, "AccessibilityAttachmentModifier" };
void *$s7SwiftUI31AccessibilityAttachmentModifierVMa(void) { return &$s7SwiftUI31AccessibilityAttachmentModifierVMn; }
sui_conf $s7SwiftUI31AccessibilityAttachmentModifierVAA04ViewE0AAMc = { (void *)sui_modifier_body, "ViewModifier" };

sui_desc $s7SwiftUI32_EnvironmentKeyTransformModifierVMn = { SUI_K_MODIFIER, "_EnvironmentKeyTransformModifier" };
sui_conf $s7SwiftUI32_EnvironmentKeyTransformModifierVyxGAA04ViewF0AAMc = { (void *)sui_modifier_body, "ViewModifier" };
sui_desc $s7SwiftUI15ModifiedContentVMn = { SUI_K_MODIFIED, "ModifiedContent" };
void *$s7SwiftUI15ModifiedContentVMa(void) { return &$s7SwiftUI15ModifiedContentVMn; }
sui_conf $s7SwiftUI15ModifiedContentVyxq_GAA4ViewA2aERzAA0E8ModifierR_rlMc = { (void *)sui_modified_body, "View" };

/* Modificadores sem tipo público exportado. */
static sui_desc sui_d_tap = { SUI_K_MODIFIER, "TapGesture" };
static sui_desc sui_d_shortcut = { SUI_K_MODIFIER, "KeyboardShortcutModifier" };
static sui_desc sui_d_focus = { SUI_K_MODIFIER, "FocusedModifier" };
static sui_desc sui_d_present = { SUI_K_MODIFIER, "PresentationModifier" };
static sui_desc sui_d_toolbar = { SUI_K_MODIFIER, "ToolbarModifier" };
static sui_desc sui_d_nav = { SUI_K_MODIFIER, "NavigationTitleModifier" };
static sui_desc sui_d_style = { SUI_K_MODIFIER, "StyleModifier" };
static sui_desc sui_d_search = { SUI_K_MODIFIER, "SearchableModifier" };
static sui_desc sui_d_gesture = { SUI_K_MODIFIER, "GestureModifier" };
static sui_desc sui_d_submit = { SUI_K_MODIFIER, "SubmitModifier" };
static sui_desc sui_d_exit = { SUI_K_MODIFIER, "ExitCommandModifier" };

/* Descritores de tipo opaco (some View) de cada modificador. */
#define SUI_OPQ(sym, nm) sui_opaque sym = { SUI_K_PROTO, nm };
SUI_OPQ($s7SwiftUI4ViewPAAE010navigationC5StyleyQrqd__AA010NavigationcE0Rd__lFQOMQ, "navigationViewStyle")
SUI_OPQ($s7SwiftUI4ViewPAAE08progressC5StyleyQrqd__AA08ProgresscE0Rd__lFQOMQ, "progressViewStyle")
SUI_OPQ($s7SwiftUI4ViewPAAE10searchable4text9placement6promptQrAA7BindingVySSG_AA20SearchFieldPlacementVqd__tSyRd__lFQOMQ, "searchable")
SUI_OPQ($s7SwiftUI4ViewPAAE11buttonStyleyQrqd__AA015PrimitiveButtonE0Rd__lFQOMQ, "buttonStyle")
SUI_OPQ($s7SwiftUI4ViewPAAE11buttonStyleyQrqd__AA06ButtonE0Rd__lFQOMQ, "buttonStyle")
SUI_OPQ($s7SwiftUI4ViewPAAE11pickerStyleyQrqd__AA06PickerE0Rd__lFQOMQ, "pickerStyle")
SUI_OPQ($s7SwiftUI4ViewPAAE12keyboardTypeyQrSo010UIKeyboardE0VFQOMQ, "keyboardType")
SUI_OPQ($s7SwiftUI4ViewPAAE12labelsHiddenQryFQOMQ, "labelsHidden")
SUI_OPQ($s7SwiftUI4ViewPAAE12onTapGesture5count7performQrSi_yyctFQOMQ, "onTapGesture")
SUI_OPQ($s7SwiftUI4ViewPAAE13onExitCommand7performQryycSg_tFQOMQ, "onExitCommand")
SUI_OPQ($s7SwiftUI4ViewPAAE14textFieldStyleyQrqd__AA04TexteF0Rd__lFQOMQ, "textFieldStyle")
SUI_OPQ($s7SwiftUI4ViewPAAE15navigationTitleyQrAA18LocalizedStringKeyVFQOMQ, "navigationTitle")
SUI_OPQ($s7SwiftUI4ViewPAAE15navigationTitleyQrqd__SyRd__lFQOMQ, "navigationTitle")
SUI_OPQ($s7SwiftUI4ViewPAAE16keyboardShortcut_9modifiersQrAA13KeyEquivalentV_AA14EventModifiersVtFQOMQ, "keyboardShortcut")
SUI_OPQ($s7SwiftUI4ViewPAAE16keyboardShortcutyQrAA08KeyboardE0VSgFQOMQ, "keyboardShortcut")
SUI_OPQ($s7SwiftUI4ViewPAAE18navigationBarItems7leadingQrqd___tAaBRd__lFQOMQ, "navigationBarItems")
SUI_OPQ($s7SwiftUI4ViewPAAE20accessibilityElement8childrenQrAA26AccessibilityChildBehaviorV_tFQOMQ, "accessibilityElement")
SUI_OPQ($s7SwiftUI4ViewPAAE21disableAutocorrectionyQrSbSgFQOMQ, "disableAutocorrection")
SUI_OPQ($s7SwiftUI4ViewPAAE29navigationBarTitleDisplayModeyQrAA010NavigationE4ItemV0fgH0OFQOMQ, "navigationBarTitleDisplayMode")
SUI_OPQ($s7SwiftUI4ViewPAAE4helpyQrAA18LocalizedStringKeyVFQOMQ, "help")
SUI_OPQ($s7SwiftUI4ViewPAAE4helpyQrAA4TextVFQOMQ, "help")
SUI_OPQ($s7SwiftUI4ViewPAAE5alert11isPresented7contentQrAA7BindingVySbG_AA5AlertVyXEtFQOMQ, "alert")
SUI_OPQ($s7SwiftUI4ViewPAAE5alert_11isPresented7actions7messageQrAA18LocalizedStringKeyV_AA7BindingVySbGqd__yXEqd_0_yXEtAaBRd__AaBRd_0_r0_lFQOMQ, "alert")
SUI_OPQ($s7SwiftUI4ViewPAAE5sheet11isPresented9onDismiss7contentQrAA7BindingVySbG_yycSgqd__yctAaBRd__lFQOMQ, "sheet")
SUI_OPQ($s7SwiftUI4ViewPAAE7focusedyQrAA10FocusStateV7BindingVySb_GFQOMQ, "focused")
SUI_OPQ($s7SwiftUI4ViewPAAE7gesture_9includingQrqd___AA11GestureMaskVtAA0F0Rd__lFQOMQ, "gesture")
SUI_OPQ($s7SwiftUI4ViewPAAE7popover11isPresented16attachmentAnchor9arrowEdge7contentQrAA7BindingVySbG_AA017PopoverAttachmentH0OAA0J0Oqd__yctAaBRd__lFQOMQ, "popover")
SUI_OPQ($s7SwiftUI4ViewPAAE7toolbar7contentQrqd__yXE_tAA14ToolbarContentRd__lFQOMQ, "toolbar")
SUI_OPQ($s7SwiftUI4ViewPAAE8onChange2of7initial_Qrqd___Sbyqd___qd__tctSQRd__lFQOMQ, "onChange")
SUI_OPQ($s7SwiftUI4ViewPAAE8onSubmit2of_QrAA0E8TriggersV_yyctFQOMQ, "onSubmit")
SUI_OPQ($s7SwiftUI4ViewPAAE9listStyleyQrqd__AA04ListE0Rd__lFQOMQ, "listStyle")

/* ---- Construção ---- */

static sui_fn sui_mkfn(void *fn, void *ctx) { sui_fn f; f.fn = fn; f.ctx = ctx; return f; }

static sui_mod *sui_mod_new(const sui_desc *d, int op) {
    sui_mod *m = (sui_mod *)sui_new(d, sizeof *m);
    if (m) m->op = op;
    return m;
}

static sui_modified *sui_apply(void *content, sui_mod *m) {
    sui_modified *v;
    if (!m) return 0;
    v = (sui_modified *)sui_new(&$s7SwiftUI15ModifiedContentVMn, sizeof *v);
    if (!v) return 0;
    v->content = content;
    v->mod = m;
    return v;
}

#define SUI_BEGIN(d, op) void *self = sui_self(); sui_mod *m = sui_mod_new(d, op); if (!m) return 0
#define SUI_END return sui_apply(self, m)

/* padding(_:) e padding(_:_:): 16 pontos quando o comprimento é nil. */
void *$s7SwiftUI4ViewPAAE7paddingyQr12CoreGraphics7CGFloatVF(double len) {
    SUI_BEGIN(&$s7SwiftUI14_PaddingLayoutVMn, SUI_OP_PADDING);
    m->u[0] = 15;
    m->d[0] = len;
    SUI_END;
}

void *$s7SwiftUI4ViewPAAE7paddingyQrAA4EdgeO3SetV_12CoreGraphics7CGFloatVSgtF(int edges, double len, int is_nil) {
    SUI_BEGIN(&$s7SwiftUI14_PaddingLayoutVMn, SUI_OP_PADDING);
    m->u[0] = (uint64_t)(edges & 0xF);
    m->d[0] = (is_nil & 1) ? 16 : len;
    SUI_END;
}

/* frame: d[] = valores, u[0] = bit i ligado quando o valor i é nil, u[1..2] = alinhamento. */
static void sui_frame_fill(sui_mod *m, double w, int wn, double h, int hn, int64_t ah, int64_t av) {
    m->d[0] = w; m->d[1] = h;
    m->u[0] = (uint64_t)((wn & 1) | ((hn & 1) << 1));
    m->u[1] = (uint64_t)ah; m->u[2] = (uint64_t)av;
}

void *$s7SwiftUI12_FrameLayoutV5width6height9alignmentAC12CoreGraphics7CGFloatVSg_AjA9AlignmentVtcfC(double w, int wn, double h, int hn, int64_t ah, int64_t av) {
    sui_mod *m = sui_mod_new(&$s7SwiftUI12_FrameLayoutVMn, SUI_OP_FRAME);
    if (m) sui_frame_fill(m, w, wn, h, hn, ah, av);
    return m;
}

void *$s7SwiftUI4ViewPAAE5frame5width6height9alignmentQr12CoreGraphics7CGFloatVSg_AkA9AlignmentVtF(double w, int wn, double h, int hn, int64_t ah, int64_t av) {
    SUI_BEGIN(&$s7SwiftUI12_FrameLayoutVMn, SUI_OP_FRAME);
    sui_frame_fill(m, w, wn, h, hn, ah, av);
    SUI_END;
}

static void sui_flex_fill(sui_mod *m, const double *v, const int *n, int64_t ah, int64_t av) {
    int i;
    m->u[0] = 0;
    for (i = 0; i < 6; i++) {
        m->d[i] = v[i];
        if (n[i] & 1) m->u[0] |= 1u << i;
    }
    m->u[1] = (uint64_t)ah; m->u[2] = (uint64_t)av;
}

void *$s7SwiftUI16_FlexFrameLayoutV8minWidth05idealG003maxG00F6Height0hJ00iJ09alignmentAC12CoreGraphics7CGFloatVSg_A5nA9AlignmentVtcfC(
    double a, int an, double b, int bn, double c, int cn, double d, int dn, double e, int en, double f, int fn, int64_t ah, int64_t av) {
    double v[6]; int n[6];
    sui_mod *m = sui_mod_new(&$s7SwiftUI16_FlexFrameLayoutVMn, SUI_OP_FLEXFRAME);
    v[0] = a; v[1] = b; v[2] = c; v[3] = d; v[4] = e; v[5] = f;
    n[0] = an; n[1] = bn; n[2] = cn; n[3] = dn; n[4] = en; n[5] = fn;
    if (m) sui_flex_fill(m, v, n, ah, av);
    return m;
}

void *$s7SwiftUI4ViewPAAE5frame8minWidth05idealF003maxF00E6Height0gI00hI09alignmentQr12CoreGraphics7CGFloatVSg_A5oA9AlignmentVtF(
    double a, int an, double b, int bn, double c, int cn, double d, int dn, double e, int en, double f, int fn, int64_t ah, int64_t av) {
    double v[6]; int n[6];
    SUI_BEGIN(&$s7SwiftUI16_FlexFrameLayoutVMn, SUI_OP_FLEXFRAME);
    v[0] = a; v[1] = b; v[2] = c; v[3] = d; v[4] = e; v[5] = f;
    n[0] = an; n[1] = bn; n[2] = cn; n[3] = dn; n[4] = en; n[5] = fn;
    sui_flex_fill(m, v, n, ah, av);
    SUI_END;
}

/* background e overlay: p[0] = view/estilo, fn = closure de conteúdo. */
void *$s7SwiftUI4ViewPAAE10background9alignment7contentQrAA9AlignmentV_qd__yXEtAaBRd__lF(int64_t ah, int64_t av, void *fn, void *ctx) {
    SUI_BEGIN(&$s7SwiftUI19_BackgroundModifierVMn, SUI_OP_BACKGROUND);
    m->u[1] = (uint64_t)ah; m->u[2] = (uint64_t)av;
    m->fn = sui_mkfn(fn, ctx);
    SUI_END;
}

void *$s7SwiftUI4ViewPAAE10background_20ignoresSafeAreaEdgesQrqd___AA4EdgeO3SetVtAA10ShapeStyleRd__lF(void *style, int edges) {
    SUI_BEGIN(&$s7SwiftUI24_BackgroundStyleModifierVMn, SUI_OP_BGSTYLE);
    m->p[0] = style;
    m->u[0] = (uint64_t)(edges & 0xF);
    SUI_END;
}

void *$s7SwiftUI4ViewPAAE10background_9alignmentQrqd___AA9AlignmentVtAaBRd__lF(void *view, int64_t ah, int64_t av) {
    SUI_BEGIN(&$s7SwiftUI19_BackgroundModifierVMn, SUI_OP_BACKGROUND);
    m->p[0] = view;
    m->u[1] = (uint64_t)ah; m->u[2] = (uint64_t)av;
    SUI_END;
}

void *$s7SwiftUI4ViewPAAE7overlay9alignment7contentQrAA9AlignmentV_qd__yXEtAaBRd__lF(int64_t ah, int64_t av, void *fn, void *ctx) {
    SUI_BEGIN(&$s7SwiftUI16_OverlayModifierVMn, SUI_OP_OVERLAY);
    m->u[1] = (uint64_t)ah; m->u[2] = (uint64_t)av;
    m->fn = sui_mkfn(fn, ctx);
    SUI_END;
}

void *$s7SwiftUI4ViewPAAE7overlay_20ignoresSafeAreaEdgesQrqd___AA4EdgeO3SetVtAA10ShapeStyleRd__lF(void *style, int edges) {
    SUI_BEGIN(&$s7SwiftUI21_OverlayStyleModifierVMn, SUI_OP_OVSTYLE);
    m->p[0] = style;
    m->u[0] = (uint64_t)(edges & 0xF);
    SUI_END;
}

void *$s7SwiftUI4ViewPAAE7overlay_9alignmentQrqd___AA9AlignmentVtAaBRd__lF(void *view, int64_t ah, int64_t av) {
    SUI_BEGIN(&$s7SwiftUI16_OverlayModifierVMn, SUI_OP_OVERLAY);
    m->p[0] = view;
    m->u[1] = (uint64_t)ah; m->u[2] = (uint64_t)av;
    SUI_END;
}

/* Valores de ambiente: cor, estilo, fonte, alinhamento, truncamento. */
#define SUI_ENV_PTR(sym, desc, op) \
    void *sym(void *v) { SUI_BEGIN(desc, op); m->p[0] = v; SUI_END; }
#define SUI_ENV_INT(sym, desc, op) \
    void *sym(int64_t v) { SUI_BEGIN(desc, op); m->u[0] = (uint64_t)v; SUI_END; }

SUI_ENV_PTR($s7SwiftUI4ViewPAAE15foregroundColoryQrAA0E0VSgF, &$s7SwiftUI30_EnvironmentKeyWritingModifierVMn, SUI_OP_FGCOLOR)
SUI_ENV_PTR($s7SwiftUI4ViewPAAE15foregroundStyleyQrqd__AA05ShapeE0Rd__lF, &$s7SwiftUI24_ForegroundStyleModifierVMn, SUI_OP_FGSTYLE)
SUI_ENV_PTR($s7SwiftUI4ViewPAAE4fontyQrAA4FontVSgF, &$s7SwiftUI30_EnvironmentKeyWritingModifierVMn, SUI_OP_FONT)
SUI_ENV_PTR($s7SwiftUI4ViewPAAE11accentColoryQrAA0E0VSgF, &$s7SwiftUI30_EnvironmentKeyWritingModifierVMn, SUI_OP_ACCENT)
SUI_ENV_INT($s7SwiftUI4ViewPAAE22multilineTextAlignmentyQrAA0eF0OF, &$s7SwiftUI30_EnvironmentKeyWritingModifierVMn, SUI_OP_TEXTALIGN)
SUI_ENV_INT($s7SwiftUI4ViewPAAE14truncationModeyQrAA4TextV010TruncationE0OF, &$s7SwiftUI30_EnvironmentKeyWritingModifierVMn, SUI_OP_TRUNCATION)
SUI_ENV_INT($s7SwiftUI4ViewPAAE8textCaseyQrAA4TextV0E0OSgF, &$s7SwiftUI30_EnvironmentKeyWritingModifierVMn, SUI_OP_TEXTCASE)
SUI_ENV_INT($s7SwiftUI4ViewPAAE9blendModeyQrAA05BlendE0OF, &$s7SwiftUI16_BlendModeEffectVMn, SUI_OP_BLEND)
SUI_ENV_INT($s7SwiftUI4ViewPAAE21disableAutocorrectionyQrSbSgF, &$s7SwiftUI30_EnvironmentKeyWritingModifierVMn, SUI_OP_AUTOCORRECT)
SUI_ENV_INT($s7SwiftUI4ViewPAAE12keyboardTypeyQrSo010UIKeyboardE0VF, &$s7SwiftUI30_EnvironmentKeyWritingModifierVMn, SUI_OP_KEYBOARDTYPE)
SUI_ENV_INT($s7SwiftUI4ViewPAAE29navigationBarTitleDisplayModeyQrAA010NavigationE4ItemV0fgH0OF, &sui_d_nav, SUI_OP_TITLEMODE)

/* lineLimit(Int?): u[0] = limite, u[1] = 1 quando nil. */
void *$s7SwiftUI4ViewPAAE9lineLimityQrSiSgF(int64_t n, int is_nil) {
    SUI_BEGIN(&$s7SwiftUI30_EnvironmentKeyWritingModifierVMn, SUI_OP_LINELIMIT);
    m->u[0] = (uint64_t)n;
    m->u[1] = (uint64_t)(is_nil & 1);
    SUI_END;
}

/* disabled: transforma isEnabled no ambiente. */
void *$s7SwiftUI4ViewPAAE8disabledyQrSbF(int on) {
    SUI_BEGIN(&$s7SwiftUI32_EnvironmentKeyTransformModifierVMn, SUI_OP_DISABLED);
    m->u[0] = (uint64_t)(on & 1);
    SUI_END;
}

void *$s7SwiftUI4ViewPAAE7opacityyQrSdF(double op) {
    SUI_BEGIN(&$s7SwiftUI14_OpacityEffectVMn, SUI_OP_OPACITY);
    m->d[0] = op < 0 ? 0 : op > 1 ? 1 : op;
    SUI_END;
}

void *$s7SwiftUI4ViewPAAE12cornerRadius_11antialiasedQr12CoreGraphics7CGFloatV_SbtF(double r, int aa) {
    SUI_BEGIN(&$s7SwiftUI11_ClipEffectVMn, SUI_OP_CORNER);
    m->d[0] = r < 0 ? 0 : r;
    m->u[0] = (uint64_t)(aa & 1);
    SUI_END;
}

void *$s7SwiftUI4ViewPAAE9clipShape_5styleQrqd___AA9FillStyleVtAA0E0Rd__lF(void *shape, int style) {
    SUI_BEGIN(&$s7SwiftUI11_ClipEffectVMn, SUI_OP_CLIP);
    m->p[0] = shape;
    m->u[0] = (uint64_t)(style & 0xFF);
    SUI_END;
}

void *$s7SwiftUI4ViewPAAE12contentShape_6eoFillQrqd___SbtAA0E0Rd__lF(void *shape, int eo) {
    SUI_BEGIN(&$s7SwiftUI21_ContentShapeModifierVMn, SUI_OP_CONTENTSHAPE);
    m->p[0] = shape;
    m->u[0] = (uint64_t)(eo & 1);
    SUI_END;
}

void *$s7SwiftUI4ViewPAAE4blur6radius6opaqueQr12CoreGraphics7CGFloatV_SbtF(double r, int opaque) {
    SUI_BEGIN(&$s7SwiftUI11_BlurEffectVMn, SUI_OP_BLUR);
    m->d[0] = r < 0 ? 0 : r;
    m->u[0] = (uint64_t)(opaque & 1);
    SUI_END;
}

void *$s7SwiftUI4ViewPAAE4mask9alignment_QrAA9AlignmentV_qd__yXEtAaBRd__lF(int64_t ah, int64_t av, void *fn, void *ctx) {
    SUI_BEGIN(&$s7SwiftUI20_MaskAlignmentEffectVMn, SUI_OP_MASK);
    m->u[1] = (uint64_t)ah; m->u[2] = (uint64_t)av;
    m->fn = sui_mkfn(fn, ctx);
    SUI_END;
}

SUI_ENV_PTR($s7SwiftUI4ViewPAAE4maskyQrqd__AaBRd__lF, &$s7SwiftUI11_MaskEffectVMn, SUI_OP_MASK)

void *$s7SwiftUI4ViewPAAE6offset1x1yQr12CoreGraphics7CGFloatV_AItF(double x, double y) {
    SUI_BEGIN(&$s7SwiftUI13_OffsetEffectVMn, SUI_OP_OFFSET);
    m->d[0] = x; m->d[1] = y;
    SUI_END;
}

void *$s7SwiftUI4ViewPAAE8position1x1yQr12CoreGraphics7CGFloatV_AItF(double x, double y) {
    SUI_BEGIN(&$s7SwiftUI15_PositionLayoutVMn, SUI_OP_POSITION);
    m->d[0] = x; m->d[1] = y;
    SUI_END;
}

void *$s7SwiftUI4ViewPAAE9fixedSize10horizontal8verticalQrSb_SbtF(int h, int v) {
    SUI_BEGIN(&$s7SwiftUI16_FixedSizeLayoutVMn, SUI_OP_FIXEDSIZE);
    m->u[0] = (uint64_t)(h & 1);
    m->u[1] = (uint64_t)(v & 1);
    SUI_END;
}

/* scaledToFit = aspectRatio(nil, contentMode: .fit). */
void *$s7SwiftUI4ViewPAAE11scaledToFitQryF(void) {
    SUI_BEGIN(&$s7SwiftUI18_AspectRatioLayoutVMn, SUI_OP_SCALEFIT);
    m->u[0] = 0;
    SUI_END;
}

void *$s7SwiftUI4ViewPAAE16allowsHitTestingyQrSbF(int on) {
    SUI_BEGIN(&$s7SwiftUI25_AllowsHitTestingModifierVMn, SUI_OP_HITTEST);
    m->u[0] = (uint64_t)(on & 1);
    SUI_END;
}

void *$s7SwiftUI4ViewPAAE12labelsHiddenQryF(void) {
    SUI_BEGIN(&$s7SwiftUI30_EnvironmentKeyWritingModifierVMn, SUI_OP_LABELSHIDDEN);
    m->u[0] = 1;
    SUI_END;
}

SUI_ENV_PTR($s7SwiftUI4ViewPAAE15coordinateSpace4nameQrqd___tSHRd__lF, &$s7SwiftUI24_CoordinateSpaceModifierVMn, SUI_OP_COORDSPACE)
SUI_ENV_PTR($s7SwiftUI4ViewPAAE17listRowBackgroundyQrqd__SgAaBRd__lF, &$s7SwiftUI21_TraitWritingModifierVMn, SUI_OP_ROWBG)
SUI_ENV_PTR($s7SwiftUI4ViewPAAE18navigationBarItems7leadingQrqd___tAaBRd__lF, &sui_d_nav, SUI_OP_BARITEMS)

/* listRowInsets(EdgeInsets?): quatro CGFloat e a flag de nil. */
void *$s7SwiftUI4ViewPAAE13listRowInsetsyQrAA04EdgeF0VSgF(double t, double l, double b, double r, int is_nil) {
    SUI_BEGIN(&$s7SwiftUI21_TraitWritingModifierVMn, SUI_OP_ROWINSETS);
    m->d[0] = t; m->d[1] = l; m->d[2] = b; m->d[3] = r;
    m->u[0] = (uint64_t)(is_nil & 1);
    SUI_END;
}

/* _trait(Key.self, valor): p[0] = tipo da chave, u[0] = primeiro word do valor. */
void *$s7SwiftUI4ViewPAAE6_traityQrqd__m_5ValueQyd__tAA01_C8TraitKeyRd__lF(void *key, const uint64_t *value) {
    SUI_BEGIN(&$s7SwiftUI21_TraitWritingModifierVMn, SUI_OP_TRAIT);
    m->p[0] = key;
    m->u[0] = value ? value[0] : 0;
    SUI_END;
}

/* modifier(_:): ModifiedContent com o modificador do app. */
void *$s7SwiftUI4ViewPAAE8modifieryAA15ModifiedContentVyxqd__Gqd__lF(void *mod) {
    SUI_BEGIN(&$s7SwiftUI21_ViewModifier_ContentVMn, SUI_OP_MODIFIER);
    m->p[0] = mod;
    SUI_END;
}

/* ---- Ações ---- */

void *$s7SwiftUI4ViewPAAE8onAppear7performQryycSg_tF(void *fn, void *ctx) {
    SUI_BEGIN(&$s7SwiftUI25_AppearanceActionModifierVMn, SUI_OP_APPEAR);
    m->fn = sui_mkfn(fn, ctx);
    SUI_END;
}

/* onChange: u[0] = valor (primeiro word), u[1] = initial, u[2] = 1 para (old, new). */
void *$s7SwiftUI4ViewPAAE8onChange2of7initial_Qrqd___Sbyqd___qd__tctSQRd__lF(const uint64_t *value, int initial, void *fn, void *ctx) {
    SUI_BEGIN(&$s7SwiftUI20_ValueActionModifierVMn, SUI_OP_CHANGE);
    m->u[0] = value ? value[0] : 0;
    m->u[1] = (uint64_t)(initial & 1);
    m->u[2] = 1;
    m->fn = sui_mkfn(fn, ctx);
    SUI_END;
}

void *$s7SwiftUI4ViewPAAE8onChange2of7performQrqd___yqd__ctSQRd__lF(const uint64_t *value, void *fn, void *ctx) {
    SUI_BEGIN(&$s7SwiftUI20_ValueActionModifierVMn, SUI_OP_CHANGE);
    m->u[0] = value ? value[0] : 0;
    m->fn = sui_mkfn(fn, ctx);
    SUI_END;
}

void *$s7SwiftUI4ViewPAAE12onTapGesture5count7performQrSi_yyctF(int64_t count, void *fn, void *ctx) {
    SUI_BEGIN(&sui_d_tap, SUI_OP_TAP);
    m->u[0] = (uint64_t)(count > 0 ? count : 1);
    m->fn = sui_mkfn(fn, ctx);
    SUI_END;
}

void *$s7SwiftUI4ViewPAAE7onHover7performQrySbc_tF(void *fn, void *ctx) {
    SUI_BEGIN(&$s7SwiftUI20_HoverRegionModifierVMn, SUI_OP_HOVER);
    m->fn = sui_mkfn(fn, ctx);
    SUI_END;
}

void *$s7SwiftUI4ViewPAAE13onExitCommand7performQryycSg_tF(void *fn, void *ctx) {
    SUI_BEGIN(&sui_d_exit, SUI_OP_EXIT);
    m->fn = sui_mkfn(fn, ctx);
    SUI_END;
}

void *$s7SwiftUI4ViewPAAE8onSubmit2of_QrAA0E8TriggersV_yyctF(uint64_t triggers, void *fn, void *ctx) {
    SUI_BEGIN(&sui_d_submit, SUI_OP_SUBMIT);
    m->u[0] = triggers;
    m->fn = sui_mkfn(fn, ctx);
    SUI_END;
}

void *$s7SwiftUI4ViewPAAE7gesture_9includingQrqd___AA11GestureMaskVtAA0F0Rd__lF(void *gesture, uint64_t mask) {
    SUI_BEGIN(&sui_d_gesture, SUI_OP_GESTURE);
    m->p[0] = gesture;
    m->u[0] = mask;
    SUI_END;
}

/* ---- Acessibilidade e ajuda ---- */

SUI_ENV_PTR($s7SwiftUI4ViewPAAE4helpyQrAA18LocalizedStringKeyVF, &$s7SwiftUI31AccessibilityAttachmentModifierVMn, SUI_OP_HELP)
SUI_ENV_PTR($s7SwiftUI4ViewPAAE4helpyQrAA4TextVF, &$s7SwiftUI31AccessibilityAttachmentModifierVMn, SUI_OP_HELP)
SUI_ENV_PTR($s7SwiftUI4ViewPAAE18accessibilityLabelyAA15ModifiedContentVyxAA31AccessibilityAttachmentModifierVGAA18LocalizedStringKeyVF, &$s7SwiftUI31AccessibilityAttachmentModifierVMn, SUI_OP_A11Y_LABEL)
SUI_ENV_PTR($s7SwiftUI4ViewPAAE18accessibilityLabelyAA15ModifiedContentVyxAA31AccessibilityAttachmentModifierVGAA4TextVF, &$s7SwiftUI31AccessibilityAttachmentModifierVMn, SUI_OP_A11Y_LABEL)
SUI_ENV_PTR($s7SwiftUI4ViewPAAE20accessibilityElement8childrenQrAA26AccessibilityChildBehaviorV_tF, &$s7SwiftUI31AccessibilityAttachmentModifierVMn, SUI_OP_A11Y_ELEMENT)

/* accessibilityLabel<S: StringProtocol>: guarda a String (dois words) copiada. */
void *$s7SwiftUI4ViewPAAE18accessibilityLabelyAA15ModifiedContentVyxAA31AccessibilityAttachmentModifierVGqd__SyRd__lF(const uint64_t *s) {
    SUI_BEGIN(&$s7SwiftUI31AccessibilityAttachmentModifierVMn, SUI_OP_A11Y_LABEL);
    m->p[1] = s ? sui_strdup2(s[0], s[1]) : 0;
    SUI_END;
}

void *$s7SwiftUI4ViewPAAE19accessibilityHiddenyAA15ModifiedContentVyxAA31AccessibilityAttachmentModifierVGSbF(int hidden) {
    SUI_BEGIN(&$s7SwiftUI31AccessibilityAttachmentModifierVMn, SUI_OP_A11Y_HIDDEN);
    m->u[0] = (uint64_t)(hidden & 1);
    SUI_END;
}

/* accessibilityAddTraits soma os bits ao modificador de acessibilidade já aplicado. */
void *$s7SwiftUI15ModifiedContentVA2A31AccessibilityAttachmentModifierVRs_rlE22accessibilityAddTraitsyACyxAEGAA0eJ0VF(uint64_t traits) {
    sui_modified *src = (sui_modified *)sui_self();
    sui_modified *v;
    sui_mod *m;
    if (!src || !src->mod) return 0;
    m = (sui_mod *)malloc(sizeof *m);
    if (!m) return 0;
    *m = *src->mod;
    m->u[3] |= traits;
    v = sui_apply(src->content, m);
    return v;
}

/* ---- Teclado e foco ---- */

/* keyboardShortcut(_:modifiers:): KeyEquivalent é um Character (dois words). */
void *$s7SwiftUI4ViewPAAE16keyboardShortcut_9modifiersQrAA13KeyEquivalentV_AA14EventModifiersVtF(uint64_t w0, uint64_t w1, uint64_t mods) {
    char key[8];
    SUI_BEGIN(&sui_d_shortcut, SUI_OP_SHORTCUT);
    sui_unstr(w0, w1, key, (int)sizeof key);
    memcpy(&m->u[0], key, sizeof key);
    m->u[1] = mods;
    SUI_END;
}

/* KeyboardShortcut?: { isa, tecla[8], modificadores } de swiftui_values.c; nil desliga. */
void *$s7SwiftUI4ViewPAAE16keyboardShortcutyQrAA08KeyboardE0VSgF(const uint64_t *sc) {
    SUI_BEGIN(&sui_d_shortcut, SUI_OP_SHORTCUT);
    if (sc) { m->u[0] = sc[1]; m->u[1] = sc[2]; }
    else m->u[2] = 1;
    SUI_END;
}

SUI_ENV_PTR($s7SwiftUI4ViewPAAE7focusedyQrAA10FocusStateV7BindingVySb_GF, &sui_d_focus, SUI_OP_FOCUSED)

/* ---- Barras, títulos e estilos ---- */

void *$s7SwiftUI4ViewPAAE7toolbar7contentQrqd__yXE_tAA14ToolbarContentRd__lF(void *fn, void *ctx) {
    SUI_BEGIN(&sui_d_toolbar, SUI_OP_TOOLBAR);
    m->fn = sui_mkfn(fn, ctx);
    SUI_END;
}

static const char *sui_lsk_c(const void *k) {
    const sui_lsk *l = (const sui_lsk *)k;
    return (l && l->key) ? l->key : "";
}

void *$s7SwiftUI4ViewPAAE15navigationTitleyQrAA18LocalizedStringKeyVF(void *key) {
    SUI_BEGIN(&sui_d_nav, SUI_OP_NAVTITLE);
    m->p[0] = key;
    m->p[1] = sui_cdup(sui_lsk_c(key));
    SUI_END;
}

void *$s7SwiftUI4ViewPAAE15navigationTitleyQrqd__SyRd__lF(const uint64_t *s) {
    SUI_BEGIN(&sui_d_nav, SUI_OP_NAVTITLE);
    m->p[1] = s ? sui_strdup2(s[0], s[1]) : sui_cdup("");
    SUI_END;
}

/* u[0] diz qual família de estilo: 1 navegação, 2 progresso, 3 botão primitivo,
 * 4 botão, 5 picker, 6 campo de texto, 7 lista. */
#define SUI_STYLEMOD(sym, fam) \
    void *sym(void *style) { SUI_BEGIN(&sui_d_style, SUI_OP_STYLE); m->p[0] = style; m->u[0] = fam; SUI_END; }
SUI_STYLEMOD($s7SwiftUI4ViewPAAE010navigationC5StyleyQrqd__AA010NavigationcE0Rd__lF, 1)
SUI_STYLEMOD($s7SwiftUI4ViewPAAE08progressC5StyleyQrqd__AA08ProgresscE0Rd__lF, 2)
SUI_STYLEMOD($s7SwiftUI4ViewPAAE11buttonStyleyQrqd__AA015PrimitiveButtonE0Rd__lF, 3)
SUI_STYLEMOD($s7SwiftUI4ViewPAAE11buttonStyleyQrqd__AA06ButtonE0Rd__lF, 4)
SUI_STYLEMOD($s7SwiftUI4ViewPAAE11pickerStyleyQrqd__AA06PickerE0Rd__lF, 5)
SUI_STYLEMOD($s7SwiftUI4ViewPAAE14textFieldStyleyQrqd__AA04TexteF0Rd__lF, 6)
SUI_STYLEMOD($s7SwiftUI4ViewPAAE9listStyleyQrqd__AA04ListE0Rd__lF, 7)

void *$s7SwiftUI4ViewPAAE10searchable4text9placement6promptQrAA7BindingVySSG_AA20SearchFieldPlacementVqd__tSyRd__lF(void *binding, int64_t placement, const uint64_t *prompt) {
    SUI_BEGIN(&sui_d_search, SUI_OP_SEARCHABLE);
    m->p[0] = binding;
    m->p[1] = prompt ? sui_strdup2(prompt[0], prompt[1]) : sui_cdup("");
    m->u[0] = (uint64_t)placement;
    SUI_END;
}

/* ---- Apresentação: sheet, alert, popover e DismissAction ---- */

static sui_mod *sui_presented[8];
static int sui_npresented;

static int sui_bind_get(void *b) {
    sui_fn g;
    if (!b) return 0;
    g.fn = (void *)$s7SwiftUI7BindingV12wrappedValuexvg;
    g.ctx = b;
    return (int)(sui_call0r(g) & 1);
}

static void sui_bind_set(void *b, int v) {
    sui_fn s;
    if (!b) return;
    s.fn = (void *)$s7SwiftUI7BindingV12wrappedValuexvs;
    s.ctx = b;
    sui_call1(s, (uint64_t)(v & 1));
}

static void sui_present_push(sui_mod *m) {
    int i;
    for (i = 0; i < sui_npresented; i++) if (sui_presented[i] == m) return;
    if (sui_npresented < 8) sui_presented[sui_npresented++] = m;
}

static void sui_present_drop(sui_mod *m) {
    int i, j;
    for (i = 0; i < sui_npresented; i++) {
        if (sui_presented[i] != m) continue;
        for (j = i; j + 1 < sui_npresented; j++) sui_presented[j] = sui_presented[j + 1];
        sui_npresented--;
        return;
    }
}

/* p[0] = Binding<Bool>, fn = conteúdo, fn2 = onDismiss/actions, p[1] = view montada. */
void *$s7SwiftUI4ViewPAAE5sheet11isPresented9onDismiss7contentQrAA7BindingVySbG_yycSgqd__yctAaBRd__lF(void *binding, void *dfn, void *dctx, void *cfn, void *cctx) {
    SUI_BEGIN(&sui_d_present, SUI_OP_SHEET);
    m->p[0] = binding;
    m->fn = sui_mkfn(cfn, cctx);
    m->fn2 = sui_mkfn(dfn, dctx);
    SUI_END;
}

void *$s7SwiftUI4ViewPAAE5alert11isPresented7contentQrAA7BindingVySbG_AA5AlertVyXEtF(void *binding, void *fn, void *ctx) {
    SUI_BEGIN(&sui_d_present, SUI_OP_ALERT);
    m->p[0] = binding;
    m->fn = sui_mkfn(fn, ctx);
    SUI_END;
}

void *$s7SwiftUI4ViewPAAE5alert_11isPresented7actions7messageQrAA18LocalizedStringKeyV_AA7BindingVySbGqd__yXEqd_0_yXEtAaBRd__AaBRd_0_r0_lF(void *title, void *binding, void *afn, void *actx, void *mfn, void *mctx) {
    SUI_BEGIN(&sui_d_present, SUI_OP_ALERT);
    m->p[0] = binding;
    m->p[2] = sui_cdup(sui_lsk_c(title));
    m->fn = sui_mkfn(afn, actx);
    m->fn2 = sui_mkfn(mfn, mctx);
    m->u[1] = 1;
    SUI_END;
}

void *$s7SwiftUI4ViewPAAE7popover11isPresented16attachmentAnchor9arrowEdge7contentQrAA7BindingVySbG_AA017PopoverAttachmentH0OAA0J0Oqd__yctAaBRd__lF(void *binding, uint64_t anchor, int edge, void *fn, void *ctx) {
    SUI_BEGIN(&sui_d_present, SUI_OP_POPOVER);
    m->p[0] = binding;
    m->u[0] = anchor;
    m->u[2] = (uint64_t)(edge & 0xFF);
    m->fn = sui_mkfn(fn, ctx);
    SUI_END;
}

/* Atualiza a apresentação: Binding true monta o conteúdo; false depois de
 * visível tira da pilha e roda o onDismiss (só no sheet). */
static void sui_present_update(sui_mod *m) {
    int on = sui_bind_get(m->p[0]);
    if (on && !m->fired) {
        m->p[1] = (void *)(uintptr_t)sui_call0r(m->fn);
        m->fired = 1;
        sui_present_push(m);
    } else if (!on && m->fired) {
        m->fired = 0;
        m->p[1] = 0;
        sui_present_drop(m);
        if (m->op == SUI_OP_SHEET) sui_call0(m->fn2);
    }
}

typedef struct { const sui_desc *isa; int64_t calls; } sui_dismiss;
sui_desc $s7SwiftUI13DismissActionVMn = { SUI_K_STRUCT, "DismissAction" };
void *$s7SwiftUI13DismissActionVMa(void) { return &$s7SwiftUI13DismissActionVMn; }
static sui_dismiss sui_dismiss_action = { &$s7SwiftUI13DismissActionVMn, 0 };

void *$s7SwiftUI17EnvironmentValuesV7dismissAA13DismissActionVvg(void) { return &sui_dismiss_action; }
void *$s7SwiftUI17EnvironmentValuesV7dismissAA13DismissActionVvpMV(void) { return &sui_dismiss_action; }

/* dismiss(): fecha a apresentação mais recente gravando false no Binding. */
void $s7SwiftUI13DismissActionV14callAsFunctionyyF(void) {
    sui_dismiss *d = (sui_dismiss *)sui_self();
    sui_mod *m;
    if (d && d->isa == &$s7SwiftUI13DismissActionVMn) d->calls++;
    else sui_dismiss_action.calls++;
    if (!sui_npresented) return;
    m = sui_presented[sui_npresented - 1];
    sui_bind_set(m->p[0], 0);
    sui_present_update(m);
}

/* ---- body do ModifiedContent ---- */

/* Último valor visto por cada onChange, pela função da closure. */
static struct { void *fn; uint64_t value; } sui_seen[64];

static void sui_change_update(sui_mod *m) {
    int i, free_i = -1;
    uint64_t now = m->u[0];
    for (i = 0; i < 64; i++) {
        if (sui_seen[i].fn == m->fn.fn) {
            uint64_t old = sui_seen[i].value;
            if (old != now) {
                sui_seen[i].value = now;
                if (m->u[2]) sui_call2(m->fn, (uint64_t)(uintptr_t)&old, (uint64_t)(uintptr_t)&now);
                else sui_call1(m->fn, (uint64_t)(uintptr_t)&now);
            }
            return;
        }
        if (!sui_seen[i].fn && free_i < 0) free_i = i;
    }
    if (free_i >= 0) { sui_seen[free_i].fn = m->fn.fn; sui_seen[free_i].value = now; }
    if (m->u[1]) sui_call2(m->fn, (uint64_t)(uintptr_t)&now, (uint64_t)(uintptr_t)&now);
}

static void *sui_modified_body(void) {
    sui_modified *v = (sui_modified *)sui_self();
    sui_mod *m;
    if (!v || !v->mod) return 0;
    m = v->mod;
    switch (m->op) {
    case SUI_OP_APPEAR:
        if (!m->fired) { m->fired = 1; sui_call0(m->fn); }
        break;
    case SUI_OP_CHANGE:
        if (m->fn.fn) sui_change_update(m);
        break;
    case SUI_OP_SHEET: case SUI_OP_ALERT: case SUI_OP_POPOVER:
        sui_present_update(m);
        break;
    case SUI_OP_BACKGROUND: case SUI_OP_OVERLAY: case SUI_OP_MASK:
        if (!m->p[0] && m->fn.fn) m->p[0] = (void *)(uintptr_t)sui_call0r(m->fn);
        break;
    default:
        break;
    }
    return v->content;
}

/* ---- Ponte para o AppKit ---- */

/* Procura, de fora para dentro, o primeiro modificador op na cadeia. */
void *mos_sui_find_modifier(void *view, int op) {
    sui_modified *v = (sui_modified *)view;
    int depth = 0;
    while (v && sui_kind(v) == SUI_K_MODIFIED && depth++ < 256) {
        if (v->mod && v->mod->op == op) return v->mod;
        v = (sui_modified *)v->content;
    }
    return 0;
}

/* Clique na view: roda o onTapGesture com a contagem certa. */
int mos_sui_tap(void *view, int count) {
    sui_modified *v = (sui_modified *)view;
    int depth = 0;
    while (v && sui_kind(v) == SUI_K_MODIFIED && depth++ < 256) {
        sui_mod *m = v->mod;
        if (m && m->op == SUI_OP_HITTEST && !(m->u[0] & 1)) return 0;
        if (m && m->op == SUI_OP_DISABLED && (m->u[0] & 1)) return 0;
        if (m && m->op == SUI_OP_TAP && (int64_t)m->u[0] == count) { sui_call0(m->fn); return 1; }
        v = (sui_modified *)v->content;
    }
    return 0;
}

/* Tecla com modificadores: devolve a view que declarou esse atalho (o Button). */
void *mos_sui_shortcut(void *view, const char *key, uint64_t mods) {
    sui_modified *v = (sui_modified *)view;
    int depth = 0;
    while (v && sui_kind(v) == SUI_K_MODIFIED && depth++ < 256) {
        sui_mod *m = v->mod;
        if (m && m->op == SUI_OP_SHORTCUT && !m->u[2]) {
            char k[9];
            memcpy(k, &m->u[0], 8);
            k[8] = 0;
            if (key && strcmp(k, key) == 0 && m->u[1] == mods) return v->content;
        }
        v = (sui_modified *)v->content;
    }
    return 0;
}
