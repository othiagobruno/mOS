/* Protocolos do SwiftUI como dados (Mp, Tq, Tl, Tn, Tb): cada requisito
 * aponta para o descritor do protocolo dono e leva o nome. Também ficam aqui
 * os tipos de apoio que os apps citam na partida: TimelineView, ToolbarItem,
 * _CALayerView, ImageRenderer, chaves de trait, os atributos de
 * AttributeScopes.SwiftUIAttributes e os padrões que devolvem valor nos
 * Representables (sizeThatFits = nil, _invalidatesSizeOnConstraintChanges = true).
 */
#ifdef SWIFTUI_PROTO_HOST
#define SUI_HOST 1
#endif
#include "swiftui_model.h"

#ifndef SUI_HOST
typedef struct cx_class cx_class_t;
const char *sel_registerName(const char *name);
void *cx_lookup(cx_class_t *cls, const char *sel);
cx_class_t *cx_class_named(const char *name);
void *cx_alloc_instance(cx_class_t *cls);
double CFAbsoluteTimeGetCurrent(void);
void *os_log_create(const char *subsystem, const char *category);
#endif

/* Descritores de protocolo (Mp). */
sui_desc $s7SwiftUI4ViewMp = { SUI_K_PROTO, "View" };
sui_desc $s7SwiftUI12ViewModifierMp = { SUI_K_PROTO, "ViewModifier" };
sui_desc $s7SwiftUI5ShapeMp = { SUI_K_PROTO, "Shape" };
sui_desc $s7SwiftUI10AnimatableMp = { SUI_K_PROTO, "Animatable" };
sui_desc $s7SwiftUI11ButtonStyleMp = { SUI_K_PROTO, "ButtonStyle" };
sui_desc $s7SwiftUI19NSViewRepresentableMp = { SUI_K_PROTO, "NSViewRepresentable" };
sui_desc $s7SwiftUI19UIViewRepresentableMp = { SUI_K_PROTO, "UIViewRepresentable" };
sui_desc $s7SwiftUI29NSViewControllerRepresentableMp = { SUI_K_PROTO, "NSViewControllerRepresentable" };

/* Requisitos (Tq), conformidades associadas (Tn) e protocolos base (Tb). */
sui_req $s7SwiftUI10AnimatableP05_makeC05value6inputsyAA11_GraphValueVyxGz_AA01_G6InputsVtFZTq = { &$s7SwiftUI10AnimatableMp, "_makeAnimatable" };
sui_req $s7SwiftUI10AnimatableP0C4DataAC_AA16VectorArithmeticTn = { &$s7SwiftUI10AnimatableMp, "AnimatableData: VectorArithmetic" };
sui_req $s7SwiftUI10AnimatableP14animatableData0cE0QzvMTq = { &$s7SwiftUI10AnimatableMp, "animatableData.modify" };
sui_req $s7SwiftUI10AnimatableP14animatableData0cE0QzvgTq = { &$s7SwiftUI10AnimatableMp, "animatableData.get" };
sui_req $s7SwiftUI10AnimatableP14animatableData0cE0QzvsTq = { &$s7SwiftUI10AnimatableMp, "animatableData.set" };
sui_req $s7SwiftUI11ButtonStyleP4BodyAC_AA4ViewTn = { &$s7SwiftUI11ButtonStyleMp, "Body: View" };
sui_req $s7SwiftUI11ButtonStyleP8makeBody13configuration0F0QzAA0cD13ConfigurationV_tFTq = { &$s7SwiftUI11ButtonStyleMp, "makeBody(configuration:)" };
sui_req $s7SwiftUI12ViewModifierP05_makeC08modifier6inputs4bodyAA01_C7OutputsVAA11_GraphValueVyxG_AA01_C6InputsVAiA01_J0V_ANtctFZTq = { &$s7SwiftUI12ViewModifierMp, "_makeView" };
sui_req $s7SwiftUI12ViewModifierP05_makeC4List8modifier6inputs4bodyAA01_cF7OutputsVAA11_GraphValueVyxG_AA01_cF6InputsVAiA01_K0V_ANtctFZTq = { &$s7SwiftUI12ViewModifierMp, "_makeViewList" };
sui_req $s7SwiftUI12ViewModifierP14_viewListCount6inputs4bodySiSgAA01_cfG6InputsV_AgIXEtFZTq = { &$s7SwiftUI12ViewModifierMp, "_viewListCount" };
sui_req $s7SwiftUI12ViewModifierP4BodyAC_AA0C0Tn = { &$s7SwiftUI12ViewModifierMp, "Body: View" };
sui_req $s7SwiftUI12ViewModifierP4body7content4BodyQzAA01_cD8_ContentVyxG_tFTq = { &$s7SwiftUI12ViewModifierMp, "body(content:)" };
sui_req $s7SwiftUI19NSViewRepresentableP04makeC07context0C4TypeQzAA0cD7ContextVyxG_tFTq = { &$s7SwiftUI19NSViewRepresentableMp, "makeNSView(context:)" };
sui_req $s7SwiftUI19NSViewRepresentableP06updateC0_7contexty0C4TypeQz_AA0cD7ContextVyxGtFTq = { &$s7SwiftUI19NSViewRepresentableMp, "updateNSView(_:context:)" };
sui_req $s7SwiftUI19NSViewRepresentableP09dismantleC0_11coordinatory0C4TypeQz_11CoordinatorQztFZTq = { &$s7SwiftUI19NSViewRepresentableMp, "dismantleNSView(_:coordinator:)" };
sui_req $s7SwiftUI19NSViewRepresentableP12sizeThatFits_6nsView7contextSo6CGSizeVSgAA08ProposedI4SizeV_0C4TypeQzAA0cD7ContextVyxGtFTq = { &$s7SwiftUI19NSViewRepresentableMp, "sizeThatFits(_:nSView:context:)" };
sui_req $s7SwiftUI19NSViewRepresentableP14_layoutOptionsyAA013_PlatformViewd6LayoutF0V0C4TypeQzFZTq = { &$s7SwiftUI19NSViewRepresentableMp, "_layoutOptions" };
sui_req $s7SwiftUI19NSViewRepresentableP15makeCoordinator0F0QzyFTq = { &$s7SwiftUI19NSViewRepresentableMp, "makeCoordinator()" };
sui_req $s7SwiftUI19NSViewRepresentableP19_identifiedViewTree2inAA011_IdentifiedfG0O0C4TypeQz_tFTq = { &$s7SwiftUI19NSViewRepresentableMp, "_identifiedViewTree" };
sui_req $s7SwiftUI19NSViewRepresentableP21_overrideLayoutTraits_3foryAA01_fG0Vz_0C4TypeQztFTq = { &$s7SwiftUI19NSViewRepresentableMp, "_overrideLayoutTraits" };
sui_req $s7SwiftUI19NSViewRepresentableP21_overrideSizeThatFits_2in6nsViewySo6CGSizeVz_AA09_ProposedF0V0C4TypeQztFTq = { &$s7SwiftUI19NSViewRepresentableMp, "_overrideSizeThatFits" };
sui_req $s7SwiftUI19NSViewRepresentableP24_modifyBridgedViewInputsyyAA01_gH0VzFZTq = { &$s7SwiftUI19NSViewRepresentableMp, "_modifyBridgedViewInputs" };
sui_req $s7SwiftUI19NSViewRepresentableP35_invalidatesSizeOnConstraintChangesSbvgZTq = { &$s7SwiftUI19NSViewRepresentableMp, "_invalidatesSizeOnConstraintChanges" };
sui_req $s7SwiftUI19NSViewRepresentablePAA4ViewTb = { &$s7SwiftUI19NSViewRepresentableMp, "base: View" };
sui_req $s7SwiftUI19UIViewRepresentableP04makeC07context0C4TypeQzAA0cD7ContextVyxG_tFTq = { &$s7SwiftUI19UIViewRepresentableMp, "makeUIView(context:)" };
sui_req $s7SwiftUI19UIViewRepresentableP06updateC0_7contexty0C4TypeQz_AA0cD7ContextVyxGtFTq = { &$s7SwiftUI19UIViewRepresentableMp, "updateUIView(_:context:)" };
sui_req $s7SwiftUI19UIViewRepresentableP09dismantleC0_11coordinatory0C4TypeQz_11CoordinatorQztFZTq = { &$s7SwiftUI19UIViewRepresentableMp, "dismantleUIView(_:coordinator:)" };
sui_req $s7SwiftUI19UIViewRepresentableP15makeCoordinator0F0QzyFTq = { &$s7SwiftUI19UIViewRepresentableMp, "makeCoordinator()" };
sui_req $s7SwiftUI19UIViewRepresentableP19_identifiedViewTree2inAA011_IdentifiedfG0O0C4TypeQz_tFTq = { &$s7SwiftUI19UIViewRepresentableMp, "_identifiedViewTree" };
sui_req $s7SwiftUI19UIViewRepresentableP21_overrideLayoutTraits_3foryAA01_fG0Vz_0C4TypeQztFTq = { &$s7SwiftUI19UIViewRepresentableMp, "_overrideLayoutTraits" };
sui_req $s7SwiftUI19UIViewRepresentableP21_overrideSizeThatFits_2in6uiViewySo6CGSizeVz_AA09_ProposedF0V0C4TypeQztFTq = { &$s7SwiftUI19UIViewRepresentableMp, "_overrideSizeThatFits" };
sui_req $s7SwiftUI19UIViewRepresentableP24_modifyBridgedViewInputsyyAA01_gH0VzFZTq = { &$s7SwiftUI19UIViewRepresentableMp, "_modifyBridgedViewInputs" };
sui_req $s7SwiftUI19UIViewRepresentablePAA4ViewTb = { &$s7SwiftUI19UIViewRepresentableMp, "base: View" };
sui_req $s7SwiftUI29NSViewControllerRepresentableP04makecD07context0cD4TypeQzAA0cdE7ContextVyxG_tFTq = { &$s7SwiftUI29NSViewControllerRepresentableMp, "makeNSViewController(context:)" };
sui_req $s7SwiftUI29NSViewControllerRepresentableP06updatecD0_7contexty0cD4TypeQz_AA0cdE7ContextVyxGtFTq = { &$s7SwiftUI29NSViewControllerRepresentableMp, "updateNSViewController(_:context:)" };
sui_req $s7SwiftUI29NSViewControllerRepresentableP09dismantlecD0_11coordinatory0cD4TypeQz_11CoordinatorQztFZTq = { &$s7SwiftUI29NSViewControllerRepresentableMp, "dismantleNSViewController(_:coordinator:)" };
sui_req $s7SwiftUI29NSViewControllerRepresentableP12sizeThatFits_06nsViewD07contextSo6CGSizeVSgAA08ProposedJ4SizeV_0cD4TypeQzAA0cdE7ContextVyxGtFTq = { &$s7SwiftUI29NSViewControllerRepresentableMp, "sizeThatFits(_:nSViewController:context:)" };
sui_req $s7SwiftUI29NSViewControllerRepresentableP14_layoutOptionsyAA013_PlatformViewe6LayoutG0V0cD4TypeQzFZTq = { &$s7SwiftUI29NSViewControllerRepresentableMp, "_layoutOptions" };
sui_req $s7SwiftUI29NSViewControllerRepresentableP15makeCoordinator0G0QzyFTq = { &$s7SwiftUI29NSViewControllerRepresentableMp, "makeCoordinator()" };
sui_req $s7SwiftUI29NSViewControllerRepresentableP19_identifiedViewTree2inAA011_IdentifiedgH0O0cD4TypeQz_tFTq = { &$s7SwiftUI29NSViewControllerRepresentableMp, "_identifiedViewTree" };
sui_req $s7SwiftUI29NSViewControllerRepresentableP35_invalidatesSizeOnConstraintChangesSbvgZTq = { &$s7SwiftUI29NSViewControllerRepresentableMp, "_invalidatesSizeOnConstraintChanges" };
sui_req $s7SwiftUI29NSViewControllerRepresentablePAA4ViewTb = { &$s7SwiftUI29NSViewControllerRepresentableMp, "base: View" };
sui_req $s7SwiftUI4ViewP05_makeC04view6inputsAA01_C7OutputsVAA11_GraphValueVyxG_AA01_C6InputsVtFZTq = { &$s7SwiftUI4ViewMp, "_makeView" };
sui_req $s7SwiftUI4ViewP05_makeC4List4view6inputsAA01_cE7OutputsVAA11_GraphValueVyxG_AA01_cE6InputsVtFZTq = { &$s7SwiftUI4ViewMp, "_makeViewList" };
sui_req $s7SwiftUI4ViewP14_viewListCount6inputsSiSgAA01_ceF6InputsV_tFZTq = { &$s7SwiftUI4ViewMp, "_viewListCount" };
sui_req $s7SwiftUI4ViewP4BodyAC_AaBTn = { &$s7SwiftUI4ViewMp, "Body: View" };
sui_req $s7SwiftUI5ShapeP23layoutDirectionBehaviorAA06LayouteF0OvgTq = { &$s7SwiftUI5ShapeMp, "layoutDirectionBehavior" };
sui_req $s7SwiftUI5ShapeP4path2inAA4PathVSo6CGRectV_tFTq = { &$s7SwiftUI5ShapeMp, "path(in:)" };
sui_req $s7SwiftUI5ShapeP4roleAA0C4RoleOvgZTq = { &$s7SwiftUI5ShapeMp, "role" };
sui_req $s7SwiftUI5ShapePAA10AnimatableTb = { &$s7SwiftUI5ShapeMp, "base: Animatable" };
sui_req $s7SwiftUI5ShapePAA4ViewTb = { &$s7SwiftUI5ShapeMp, "base: View" };

/* Tipos associados (Tl). */
sui_req $s10NSViewType7SwiftUI0A13RepresentablePTl = { &$s7SwiftUI19NSViewRepresentableMp, "NSViewType" };
sui_req $s10UIViewType7SwiftUI0A13RepresentablePTl = { &$s7SwiftUI19UIViewRepresentableMp, "UIViewType" };
sui_req $s11Coordinator7SwiftUI19NSViewRepresentablePTl = { &$s7SwiftUI19NSViewRepresentableMp, "Coordinator" };
sui_req $s11Coordinator7SwiftUI19UIViewRepresentablePTl = { &$s7SwiftUI19UIViewRepresentableMp, "Coordinator" };
sui_req $s11Coordinator7SwiftUI29NSViewControllerRepresentablePTl = { &$s7SwiftUI29NSViewControllerRepresentableMp, "Coordinator" };
sui_req $s14AnimatableData7SwiftUI0A0PTl = { &$s7SwiftUI10AnimatableMp, "AnimatableData" };
sui_req $s20NSViewControllerType7SwiftUI0aB13RepresentablePTl = { &$s7SwiftUI29NSViewControllerRepresentableMp, "NSViewControllerType" };
sui_req $s4Body7SwiftUI11ButtonStylePTl = { &$s7SwiftUI11ButtonStyleMp, "Body" };
sui_req $s4Body7SwiftUI12ViewModifierPTl = { &$s7SwiftUI12ViewModifierMp, "Body" };
sui_req $s4Body7SwiftUI4ViewPTl = { &$s7SwiftUI4ViewMp, "Body" };

/* ---- Padrões dos Representables que devolvem valor ---- */

/* sizeThatFits(_:nsView:context:) padrão: nil. CGSize? = xmm0, xmm1 e o byte de nil em al. */
__asm__(
    ".text\n"
    ".p2align 4\n"
    ".globl \"$s7SwiftUI19NSViewRepresentablePAAE12sizeThatFits_6nsView7contextSo6CGSizeVSgAA08ProposedI4SizeV_0C4TypeQzAA0cD7ContextVyxGtF\"\n"
    ".type \"$s7SwiftUI19NSViewRepresentablePAAE12sizeThatFits_6nsView7contextSo6CGSizeVSgAA08ProposedI4SizeV_0C4TypeQzAA0cD7ContextVyxGtF\", @function\n"
    "\"$s7SwiftUI19NSViewRepresentablePAAE12sizeThatFits_6nsView7contextSo6CGSizeVSgAA08ProposedI4SizeV_0C4TypeQzAA0cD7ContextVyxGtF\":\n"
    ".globl \"$s7SwiftUI29NSViewControllerRepresentablePAAE12sizeThatFits_06nsViewD07contextSo6CGSizeVSgAA08ProposedJ4SizeV_0cD4TypeQzAA0cdE7ContextVyxGtF\"\n"
    ".type \"$s7SwiftUI29NSViewControllerRepresentablePAAE12sizeThatFits_06nsViewD07contextSo6CGSizeVSgAA08ProposedJ4SizeV_0cD4TypeQzAA0cdE7ContextVyxGtF\", @function\n"
    "\"$s7SwiftUI29NSViewControllerRepresentablePAAE12sizeThatFits_06nsViewD07contextSo6CGSizeVSgAA08ProposedJ4SizeV_0cD4TypeQzAA0cdE7ContextVyxGtF\":\n"
    "    xorpd %xmm0, %xmm0\n"
    "    xorpd %xmm1, %xmm1\n"
    "    mov $1, %eax\n"
    "    ret\n");

/* A view nativa pode mudar de tamanho com as constraints: o padrão é true. */
int $s7SwiftUI19NSViewRepresentablePAAE35_invalidatesSizeOnConstraintChangesSbvgZ(void) { return 1; }
int $s7SwiftUI29NSViewControllerRepresentablePAAE35_invalidatesSizeOnConstraintChangesSbvgZ(void) { return 1; }

/* Context { coordinator, transaction, environment }: o coordinator é o primeiro campo. */
static sui_desc sui_uictx_desc = { SUI_K_STRUCT, "UIViewRepresentableContext" };
void *$s7SwiftUI26UIViewRepresentableContextVMa(void) { return &sui_uictx_desc; }
void *$s7SwiftUI26UIViewRepresentableContextV11coordinator11CoordinatorQzvg(void) {
    void **ctx = (void **)sui_self();
    return ctx ? ctx[0] : 0;
}

/* ---- TimelineView ---- */

typedef struct { const sui_desc *isa; double interval; int paused; } sui_sched;
typedef struct { const sui_desc *isa; double date; int64_t cadence; } sui_tlctx;
typedef struct { const sui_desc *isa; void *schedule; sui_fn content; sui_tlctx ctx; void *child; } sui_timeline;

static void *sui_timeline_body(void);
sui_desc $s7SwiftUI12TimelineViewVMn = { SUI_K_STRUCT, "TimelineView" };
void *$s7SwiftUI12TimelineViewVMa(void) { return &$s7SwiftUI12TimelineViewVMn; }
sui_conf $s7SwiftUI12TimelineViewVyxq_GAA0D0A2aER_rlMc = { (void *)sui_timeline_body, "View" };
sui_desc $s7SwiftUI12TimelineViewV7ContextVMn = { SUI_K_STRUCT, "TimelineView.Context" };
void *$s7SwiftUI12TimelineViewV7ContextVMa(void) { return &$s7SwiftUI12TimelineViewV7ContextVMn; }

sui_desc $s7SwiftUI25AnimationTimelineScheduleVMn = { SUI_K_STRUCT, "AnimationTimelineSchedule" };
void *$s7SwiftUI25AnimationTimelineScheduleVMa(void) { return &$s7SwiftUI25AnimationTimelineScheduleVMn; }
sui_conf $s7SwiftUI25AnimationTimelineScheduleVAA0dE0AAMc = { 0, "TimelineSchedule" };
sui_desc $s7SwiftUI27EveryMinuteTimelineScheduleVMn = { SUI_K_STRUCT, "EveryMinuteTimelineSchedule" };

/* minimumInterval: Double? = (valor, flag de nil); nil = 1/60 s. */
void *$s7SwiftUI25AnimationTimelineScheduleV15minimumInterval6pausedACSdSg_SbtcfC(double interval, int is_nil, int paused) {
    sui_sched *s = (sui_sched *)sui_new(&$s7SwiftUI25AnimationTimelineScheduleVMn, sizeof *s);
    if (!s) return 0;
    s->interval = (is_nil & 1) || interval <= 0 ? 1.0 / 60.0 : interval;
    s->paused = paused & 1;
    return s;
}

void *$s7SwiftUI12TimelineViewVA2A0D0R_rlE_7contentACyxq_Gx_q_AC7ContextVyxq__GctcfC(void *schedule, void *fn, void *ctx) {
    sui_timeline *t = (sui_timeline *)sui_new(&$s7SwiftUI12TimelineViewVMn, sizeof *t);
    if (!t) return 0;
    t->schedule = schedule;
    t->content.fn = fn;
    t->content.ctx = ctx;
    t->ctx.isa = &$s7SwiftUI12TimelineViewV7ContextVMn;
    return t;
}

#ifdef SUI_HOST
static double sui_host_clock;
static double sui_now(void) { return sui_host_clock += 1; }
#else
static double sui_now(void) { return CFAbsoluteTimeGetCurrent(); }
#endif

/* body: atualiza a data do Context e monta o conteúdo com ele. */
static void *sui_timeline_body(void) {
    sui_timeline *t = (sui_timeline *)sui_self();
    if (!t) return 0;
    t->ctx.date = sui_now();
    t->ctx.cadence = 0;
    t->child = (void *)(uintptr_t)sui_call1r(t->content, (uint64_t)(uintptr_t)&t->ctx);
    return t->child;
}

/* ---- ToolbarItem e _CALayerView ---- */

typedef struct { const sui_desc *isa; int64_t placement; sui_fn content; } sui_toolitem;
sui_desc $s7SwiftUI11ToolbarItemVMn = { SUI_K_STRUCT, "ToolbarItem" };
sui_conf $s7SwiftUI11ToolbarItemVyxq_GAA0C7ContentAAMc = { 0, "ToolbarContent" };
void *$s7SwiftUI11ToolbarItemVAAytRszrlE9placement7contentACyytq_GAA0cD9PlacementV_q_yXEtcfC(int64_t placement, void *fn, void *ctx) {
    sui_toolitem *t = (sui_toolitem *)sui_new(&$s7SwiftUI11ToolbarItemVMn, sizeof *t);
    if (!t) return 0;
    t->placement = placement;
    t->content.fn = fn;
    t->content.ctx = ctx;
    return t;
}

typedef struct { const sui_desc *isa; void *type; sui_fn update; } sui_layerview;
sui_desc $s7SwiftUI12_CALayerViewVMn = { SUI_K_STRUCT, "_CALayerView" };
sui_conf $s7SwiftUI12_CALayerViewVyxGAA0D0AAMc = { 0, "View" };
void *$s7SwiftUI12_CALayerViewV4type8onUpdateACyxGxm_yxctcfC(void *type, void *fn, void *ctx) {
    sui_layerview *l = (sui_layerview *)sui_new(&$s7SwiftUI12_CALayerViewVMn, sizeof *l);
    if (!l) return 0;
    l->type = type;
    l->update.fn = fn;
    l->update.ctx = ctx;
    return l;
}

/* ---- ImageRenderer ---- */

typedef struct { const sui_desc *isa; void *content; double scale; double w, h; int wnil, hnil; void *image; } sui_renderer;
sui_desc $s7SwiftUI13ImageRendererCMn = { SUI_K_STRUCT, "ImageRenderer" };
void *$s7SwiftUI13ImageRendererCMa(void) { return &$s7SwiftUI13ImageRendererCMn; }

void *$s7SwiftUI13ImageRendererC7contentACyxGx_tcfc(void *content, void *meta) {
    sui_renderer *r = (sui_renderer *)sui_new(&$s7SwiftUI13ImageRendererCMn, sizeof *r);
    (void)meta;
    if (!r) return 0;
    r->content = content;
    r->scale = 1;
    r->wnil = r->hnil = 1;
    return r;
}

void *$s7SwiftUI13ImageRendererC7contentACyxGx_tcfC(void *content, void *meta) {
    return $s7SwiftUI13ImageRendererC7contentACyxGx_tcfc(content, meta);
}

void $s7SwiftUI13ImageRendererC5scale12CoreGraphics7CGFloatVvs(double scale) {
    sui_renderer *r = (sui_renderer *)sui_self();
    if (r) r->scale = scale > 0 ? scale : 1;
}

/* ProposedViewSize = (CGFloat?, CGFloat?). */
void $s7SwiftUI13ImageRendererC12proposedSizeAA012ProposedViewF0Vvs(double w, int wnil, double h, int hnil) {
    sui_renderer *r = (sui_renderer *)sui_self();
    if (!r) return;
    r->w = w; r->wnil = wnil & 1;
    r->h = h; r->hnil = hnil & 1;
}

/* nsImage: NSImage do tamanho proposto (240x200 quando não vem), em pixels × scale. */
void *$s7SwiftUI13ImageRendererC02nsC0So7NSImageCSgvg(void) {
    sui_renderer *r = (sui_renderer *)sui_self();
    double w, h;
    if (!r) return 0;
    w = (r->wnil || r->w <= 0) ? 240 : r->w;
    h = (r->hnil || r->h <= 0) ? 200 : r->h;
#ifdef SUI_HOST
    {
        double *img = (double *)malloc(2 * sizeof(double));
        if (img) { img[0] = w * r->scale; img[1] = h * r->scale; }
        r->image = img;
    }
#else
    {
        cx_class_t *cls = cx_class_named("NSImage");
        const char *sel = sel_registerName("initWithSize:");
        void *(*init)(void *, const char *, double, double);
        void *obj;
        if (!cls) return 0;
        obj = cx_alloc_instance(cls);
        init = (void *(*)(void *, const char *, double, double))cx_lookup(cls, sel);
        r->image = (obj && init) ? init(obj, sel, w * r->scale, h * r->scale) : obj;
    }
#endif
    return r->image;
}

/* ---- Chaves de trait, metadados de apoio e atributos ---- */

static sui_desc sui_tagkey_desc = { SUI_K_STRUCT, "TagValueTraitKey" };
static sui_desc sui_tagval_desc = { SUI_K_ENUM, "TagValueTraitKey.Value" };
void *$s7SwiftUI16TagValueTraitKeyVMa(void) { return &sui_tagkey_desc; }
void *$s7SwiftUI16TagValueTraitKeyV0D0OMa(void) { return &sui_tagval_desc; }
sui_conf $s7SwiftUI16TagValueTraitKeyVyxGAA05_VieweF0AAMc = { 0, "_ViewTraitKey" };
sui_desc $s7SwiftUI18TransitionTraitKeyVMn = { SUI_K_STRUCT, "TransitionTraitKey" };
sui_desc $s7SwiftUI21ListRowInsetsTraitKeyVMn = { SUI_K_STRUCT, "ListRowInsetsTraitKey" };
sui_desc $s7SwiftUI25ListRowBackgroundTraitKeyVMn = { SUI_K_STRUCT, "ListRowBackgroundTraitKey" };
sui_desc $s7SwiftUI13_VariadicViewO4TreeVMn = { SUI_K_STRUCT, "_VariadicView.Tree" };

sui_desc $s7SwiftUI16SubscriptionViewVMn = { SUI_K_STRUCT, "SubscriptionView" };
void *$s7SwiftUI16SubscriptionViewVMa(void) { return &$s7SwiftUI16SubscriptionViewVMn; }
sui_conf $s7SwiftUI16SubscriptionViewVyxq_GAA0D0AAMc = { 0, "View" };

sui_desc $s7SwiftUI19EmptyAnimatableDataVMn = { SUI_K_STRUCT, "EmptyAnimatableData" };
sui_meta $s7SwiftUI19EmptyAnimatableDataVN = { &$s7SwiftUI19EmptyAnimatableDataVMn };
sui_conf $s7SwiftUI19EmptyAnimatableDataVAA16VectorArithmeticAAMc = { 0, "VectorArithmetic" };

sui_conf $sSi7SwiftUI18_FormatSpecifiableAAMc = { 0, "_FormatSpecifiable" };

/* AttributeScopes.SwiftUIAttributes: font, foregroundColor e underlineStyle. */
sui_desc $s10Foundation15AttributeScopesO7SwiftUIE0D12UIAttributesVMn = { SUI_K_STRUCT, "AttributeScopes.SwiftUIAttributes" };
sui_desc $s10Foundation15AttributeScopesO7SwiftUIE0D12UIAttributesV04FontB0OMn = { SUI_K_ENUM, "FontAttribute" };
sui_meta $s10Foundation15AttributeScopesO7SwiftUIE0D12UIAttributesV04FontB0ON = { &$s10Foundation15AttributeScopesO7SwiftUIE0D12UIAttributesV04FontB0OMn };
sui_conf $s10Foundation15AttributeScopesO7SwiftUIE0D12UIAttributesV04FontB0OAA19AttributedStringKeyADMc = { 0, "AttributedStringKey" };
sui_desc $s10Foundation15AttributeScopesO7SwiftUIE0D12UIAttributesV015ForegroundColorB0OMn = { SUI_K_ENUM, "ForegroundColorAttribute" };
sui_meta $s10Foundation15AttributeScopesO7SwiftUIE0D12UIAttributesV015ForegroundColorB0ON = { &$s10Foundation15AttributeScopesO7SwiftUIE0D12UIAttributesV015ForegroundColorB0OMn };
sui_conf $s10Foundation15AttributeScopesO7SwiftUIE0D12UIAttributesV015ForegroundColorB0OAA19AttributedStringKeyADMc = { 0, "AttributedStringKey" };
sui_desc $s10Foundation15AttributeScopesO7SwiftUIE0D12UIAttributesV014UnderlineStyleB0OMn = { SUI_K_ENUM, "UnderlineStyleAttribute" };
sui_meta $s10Foundation15AttributeScopesO7SwiftUIE0D12UIAttributesV014UnderlineStyleB0ON = { &$s10Foundation15AttributeScopesO7SwiftUIE0D12UIAttributesV014UnderlineStyleB0OMn };
sui_conf $s10Foundation15AttributeScopesO7SwiftUIE0D12UIAttributesV014UnderlineStyleB0OAA19AttributedStringKeyADMc = { 0, "AttributedStringKey" };
/* Descritores de propriedade (MV) das três chaves. */
sui_req $s10Foundation15AttributeScopesO7SwiftUIE0D12UIAttributesV4fontAF04FontB0OvpMV = { &$s10Foundation15AttributeScopesO7SwiftUIE0D12UIAttributesVMn, "font" };
sui_req $s10Foundation15AttributeScopesO7SwiftUIE0D12UIAttributesV15foregroundColorAF010ForegroundhB0OvpMV = { &$s10Foundation15AttributeScopesO7SwiftUIE0D12UIAttributesVMn, "foregroundColor" };
sui_req $s10Foundation15AttributeScopesO7SwiftUIE0D12UIAttributesV14underlineStyleAF09UnderlinehB0OvpMV = { &$s10Foundation15AttributeScopesO7SwiftUIE0D12UIAttributesVMn, "underlineStyle" };

/* Log.runtimeIssues: o os_log de avisos de runtime do SwiftUI, criado uma vez. */
static void *sui_runtime_log;
void *$s7SwiftUI3LogO013runtimeIssuesC0So9OS_os_logCvgZ(void) {
#ifdef SUI_HOST
    static char host_log[16];
    if (!sui_runtime_log) sui_runtime_log = host_log;
#else
    if (!sui_runtime_log) sui_runtime_log = os_log_create("com.apple.runtime-issues", "SwiftUI");
#endif
    return sui_runtime_log;
}
