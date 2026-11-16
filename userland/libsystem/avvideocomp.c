/*
 * mOS — AVVideoComposition, AVMutableVideoComposition, AVVideoCompositionInstruction (e mutável) e
 * AVVideoCompositionLayerInstruction (e mutável): renderSize, frameDuration, instruções por intervalo com cor de fundo,
 * e por camada transform/opacidade/recorte em valores fixos ou rampas (avvcomp_core.c). O AVAssetExportSession usa
 * cx_vcomp_describe para renderizar a composição quadro a quadro (avexport_objc.c). Código original do mOS (MIT).
 */
#include "foundation.h"
#include "avvcomp_core.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
id objc_msgSend_stret(id self, SEL op, ...);
void *memset(void *d, int c, size_t n);
void *CGColorRetain(void *c);
void CGColorRelease(void *c);
size_t CGColorGetNumberOfComponents(const void *c);
const double *CGColorGetComponents(const void *c);

typedef struct { long long value; int timescale; unsigned flags; long long epoch; } cmt_t;
typedef struct { cmt_t start, duration; } cmr_t;
typedef struct { double w, h; } sz_t;
static double secs(cmt_t t) { return t.timescale > 0 && (t.flags & 1) ? (double)t.value / t.timescale : 0; }
static cmt_t mk(double s) { cmt_t t = {(long long)(s * 600 + 0.5), 600, 1, 0}; return t; }

#define CLS(n) cx_class_t n##_c __asm__("OBJC_CLASS_$_" #n), n##_m __asm__("OBJC_METACLASS_$_" #n);
CLS(AVVideoComposition) CLS(AVMutableVideoComposition) CLS(AVVideoCompositionInstruction) CLS(AVMutableVideoCompositionInstruction)
CLS(AVVideoCompositionLayerInstruction) CLS(AVMutableVideoCompositionLayerInstruction)

typedef struct { cx_class_t *isa; sz_t size; cmt_t fd; id instructions; float scale; i32 timing_track; id tool, compositor, prim, matrix, transfer; } vcomp_t;
typedef struct { cx_class_t *isa; cmr_t range; id layers; void *bg; BOOL post; } vinst_t;
typedef struct { cx_class_t *isa; vc_layer l; } vlayer_t;
#define VC(o) ((vcomp_t *)(o))
#define VI(o) ((vinst_t *)(o))
#define VL(o) ((vlayer_t *)(o))
static void keep(id *slot, id v) { if (v) objc_retain(v); if (*slot) objc_release(*slot); *slot = v; }
static id copy_arr(id a) { return a ? MSG0(id, a, "copy") : MSG0(id, (id)cx_class_named("NSArray"), "array"); }

/* ---------------------------------------------------------------- composição */
static id vc_new(cx_class_t *k) {
    vcomp_t *v = (vcomp_t *)cx_alloc_instance(k);
    v->fd = mk(1.0 / 30); v->scale = 1; v->instructions = objc_retain(MSG0(id, (id)cx_class_named("NSArray"), "array"));
    return objc_autorelease((id)v);
}
static id vc_make(id k, SEL c) { (void)c; return vc_new((cx_class_t *)k); }
/* com as propriedades do asset: tamanho e taxa da faixa de vídeo, uma instrução do asset inteiro com o preferredTransform */
static id vc_with_asset(id k, SEL c, id asset) {
    id v = vc_make(k, c);
    id tracks = asset ? MSG(id, asset, "tracksWithMediaType:", cx_str("vide")) : 0;
    id tr = tracks && MSG0(u64, tracks, "count") ? MSG(id, tracks, "objectAtIndex:", (u64)0) : 0;
    if (!tr) return v;
    sz_t ns = ((sz_t (*)(id, SEL))objc_msgSend)(tr, sel_registerName("naturalSize"));
    vc_aff pt = ((vc_aff (*)(id, SEL))objc_msgSend_stret)(tr, sel_registerName("preferredTransform"));
    /* tamanho depois do transform (90°: troca largura e altura) */
    double w = ns.w * pt.a + ns.h * pt.c, h = ns.w * pt.b + ns.h * pt.d;
    VC(v)->size.w = w < 0 ? -w : w; VC(v)->size.h = h < 0 ? -h : h;
    float fps = MSG0(float, tr, "nominalFrameRate");
    if (fps > 0) VC(v)->fd = mk(1.0 / fps);
    cmt_t dur = ((cmt_t (*)(id, SEL))objc_msgSend_stret)(asset, sel_registerName("duration"));
    vinst_t *in = (vinst_t *)cx_alloc_instance(&AVMutableVideoCompositionInstruction_c);
    in->range.start = mk(0); in->range.duration = dur;
    vlayer_t *ly = (vlayer_t *)cx_alloc_instance(&AVMutableVideoCompositionLayerInstruction_c);
    ly->l.track = MSG0(i32, tr, "trackID");
    ly->l.tr[0] = (vc_aff_ramp){0, 0, pt, pt}; ly->l.ntr = 1;
    in->layers = objc_retain(MSG(id, (id)cx_class_named("NSArray"), "arrayWithObject:", (id)ly));
    objc_release((id)ly);
    keep(&VC(v)->instructions, MSG(id, (id)cx_class_named("NSArray"), "arrayWithObject:", (id)in));
    objc_release((id)in);
    return v;
}
static sz_t vc_size(id s, SEL c) { (void)c; return VC(s)->size; }
static void vc_set_size(id s, SEL c, sz_t z) { (void)c; VC(s)->size = z; }
static cmt_t vc_fd(id s, SEL c) { (void)c; return VC(s)->fd; }
static void vc_set_fd(id s, SEL c, cmt_t t) { (void)c; VC(s)->fd = t; }
static id vc_inst(id s, SEL c) { (void)c; return VC(s)->instructions; }
static void vc_set_inst(id s, SEL c, id a) { (void)c; id cp = copy_arr(a); keep(&VC(s)->instructions, cp); objc_release(cp); }
static float vc_scale(id s, SEL c) { (void)c; return VC(s)->scale; }
static void vc_set_scale(id s, SEL c, float v) { (void)c; VC(s)->scale = v; }
static i32 vc_ttrack(id s, SEL c) { (void)c; return VC(s)->timing_track; }
static void vc_set_ttrack(id s, SEL c, i32 v) { (void)c; VC(s)->timing_track = v; }
static id vc_tool(id s, SEL c) { (void)c; return VC(s)->tool; }
static void vc_set_tool(id s, SEL c, id v) { (void)c; keep(&VC(s)->tool, v); }
static id vc_comp(id s, SEL c) { (void)c; return VC(s)->compositor; }
static void vc_set_comp(id s, SEL c, id v) { (void)c; VC(s)->compositor = v; }
static id vc_prim(id s, SEL c) { (void)c; return VC(s)->prim; }
static void vc_set_prim(id s, SEL c, id v) { (void)c; keep(&VC(s)->prim, v); }
static id vc_matrix(id s, SEL c) { (void)c; return VC(s)->matrix; }
static void vc_set_matrix(id s, SEL c, id v) { (void)c; keep(&VC(s)->matrix, v); }
static id vc_transfer(id s, SEL c) { (void)c; return VC(s)->transfer; }
static void vc_set_transfer(id s, SEL c, id v) { (void)c; keep(&VC(s)->transfer, v); }
static BOOL vc_valid(id s, SEL c, id asset, cmr_t r, id del) {
    (void)c; (void)asset; (void)r; (void)del;
    return VC(s)->size.w > 0 && VC(s)->size.h > 0 && secs(VC(s)->fd) > 0 ? YES : NO;
}
static id vc_copy(id s, SEL c) {
    (void)c;
    vcomp_t *n = (vcomp_t *)cx_alloc_instance(object_getClass(s) == &AVMutableVideoComposition_c ? &AVVideoComposition_c : object_getClass(s));
    *n = *VC(s);
    n->isa = object_getClass(s) == &AVMutableVideoComposition_c ? &AVVideoComposition_c : object_getClass(s);
    id *own[] = {&n->instructions, &n->tool, &n->prim, &n->matrix, &n->transfer};   /* compositor é uma classe: sem retain */
    for (unsigned i = 0; i < sizeof own / sizeof own[0]; i++) if (*own[i]) objc_retain(*own[i]);
    return (id)n;
}
static id vc_mcopy(id s, SEL c) { id n = vc_copy(s, c); *(cx_class_t **)n = &AVMutableVideoComposition_c; return n; }

/* ---------------------------------------------------------------- instrução */
static id vi_make(id k, SEL c) { (void)c; return objc_autorelease(cx_alloc_instance((cx_class_t *)k)); }
static cmr_t vi_range(id s, SEL c) { (void)c; return VI(s)->range; }
static void vi_set_range(id s, SEL c, cmr_t r) { (void)c; VI(s)->range = r; }
static id vi_layers(id s, SEL c) { (void)c; return VI(s)->layers ? VI(s)->layers : MSG0(id, (id)cx_class_named("NSArray"), "array"); }
static void vi_set_layers(id s, SEL c, id a) { (void)c; id cp = copy_arr(a); keep(&VI(s)->layers, cp); objc_release(cp); }
static void *vi_bg(id s, SEL c) { (void)c; return VI(s)->bg; }
static void vi_set_bg(id s, SEL c, void *col) { (void)c; if (col) CGColorRetain(col); if (VI(s)->bg) CGColorRelease(VI(s)->bg); VI(s)->bg = col; }
static BOOL vi_post(id s, SEL c) { (void)c; return VI(s)->post; }
static void vi_set_post(id s, SEL c, BOOL v) { (void)c; VI(s)->post = v; }
static id vi_init(id s, SEL c) { (void)c; VI(s)->post = YES; return s; }

/* ---------------------------------------------------------------- camada */
static id vl_make(id k, SEL c) { (void)c; return objc_autorelease(cx_alloc_instance((cx_class_t *)k)); }
static id vl_with_track(id k, SEL c, id track) { id l = vl_make(k, c); if (track) VL(l)->l.track = MSG0(i32, track, "trackID"); return l; }
static i32 vl_track(id s, SEL c) { (void)c; return VL(s)->l.track; }
static void vl_set_track(id s, SEL c, i32 v) { (void)c; VL(s)->l.track = v; }
static void vl_set_transform(id s, SEL c, vc_aff a, cmt_t t) {
    (void)c; vc_layer *l = &VL(s)->l;
    if (l->ntr < VC_MAX_RAMPS) l->tr[l->ntr++] = (vc_aff_ramp){secs(t), secs(t), a, a};
}
static void vl_ramp_transform(id s, SEL c, vc_aff a0, vc_aff a1, cmr_t r) {
    (void)c; vc_layer *l = &VL(s)->l;
    if (l->ntr < VC_MAX_RAMPS) l->tr[l->ntr++] = (vc_aff_ramp){secs(r.start), secs(r.start) + secs(r.duration), a0, a1};
}
static void vl_set_opacity(id s, SEL c, float v, cmt_t t) {
    (void)c; vc_layer *l = &VL(s)->l;
    if (l->nop < VC_MAX_RAMPS) l->op[l->nop++] = (vc_f_ramp){secs(t), secs(t), v, v};
}
static void vl_ramp_opacity(id s, SEL c, float v0, float v1, cmr_t r) {
    (void)c; vc_layer *l = &VL(s)->l;
    if (l->nop < VC_MAX_RAMPS) l->op[l->nop++] = (vc_f_ramp){secs(r.start), secs(r.start) + secs(r.duration), v0, v1};
}
static void vl_set_crop(id s, SEL c, vc_rect rc, cmt_t t) {
    (void)c; vc_layer *l = &VL(s)->l;
    if (l->ncr < VC_MAX_RAMPS) l->cr[l->ncr++] = (vc_rect_ramp){secs(t), secs(t), rc, rc};
}
static void vl_ramp_crop(id s, SEL c, vc_rect r0, vc_rect r1, cmr_t r) {
    (void)c; vc_layer *l = &VL(s)->l;
    if (l->ncr < VC_MAX_RAMPS) l->cr[l->ncr++] = (vc_rect_ramp){secs(r.start), secs(r.start) + secs(r.duration), r0, r1};
}
/* get*RampForTime: a rampa que vale em t; NO se não há nenhuma */
static int ramp_at(const double *t0, const double *t1, int n, double t) {
    int k = -1;
    for (int i = 0; i < n; i++) if (t0[i] <= t && (k < 0 || t0[i] >= t0[k])) k = i;
    (void)t1;
    return k;
}
static BOOL vl_get_transform(id s, SEL c, cmt_t t, vc_aff *a0, vc_aff *a1, cmr_t *r) {
    (void)c; vc_layer *l = &VL(s)->l;
    double t0[VC_MAX_RAMPS], t1[VC_MAX_RAMPS];
    for (int i = 0; i < l->ntr; i++) { t0[i] = l->tr[i].t0; t1[i] = l->tr[i].t1; }
    int k = ramp_at(t0, t1, l->ntr, secs(t));
    if (k < 0) return NO;
    if (a0) *a0 = l->tr[k].a0;
    if (a1) *a1 = l->tr[k].a1;
    if (r) { r->start = mk(l->tr[k].t0); r->duration = mk(l->tr[k].t1 - l->tr[k].t0); }
    return YES;
}
static BOOL vl_get_opacity(id s, SEL c, cmt_t t, float *v0, float *v1, cmr_t *r) {
    (void)c; vc_layer *l = &VL(s)->l;
    double t0[VC_MAX_RAMPS], t1[VC_MAX_RAMPS];
    for (int i = 0; i < l->nop; i++) { t0[i] = l->op[i].t0; t1[i] = l->op[i].t1; }
    int k = ramp_at(t0, t1, l->nop, secs(t));
    if (k < 0) return NO;
    if (v0) *v0 = l->op[k].v0;
    if (v1) *v1 = l->op[k].v1;
    if (r) { r->start = mk(l->op[k].t0); r->duration = mk(l->op[k].t1 - l->op[k].t0); }
    return YES;
}
static BOOL vl_get_crop(id s, SEL c, cmt_t t, vc_rect *r0, vc_rect *r1, cmr_t *r) {
    (void)c; vc_layer *l = &VL(s)->l;
    double t0[VC_MAX_RAMPS], t1[VC_MAX_RAMPS];
    for (int i = 0; i < l->ncr; i++) { t0[i] = l->cr[i].t0; t1[i] = l->cr[i].t1; }
    int k = ramp_at(t0, t1, l->ncr, secs(t));
    if (k < 0) return NO;
    if (r0) *r0 = l->cr[k].r0;
    if (r1) *r1 = l->cr[k].r1;
    if (r) { r->start = mk(l->cr[k].t0); r->duration = mk(l->cr[k].t1 - l->cr[k].t0); }
    return YES;
}
static id vl_copy(id s, SEL c, void *zone) {
    (void)c; (void)zone;
    vlayer_t *n = (vlayer_t *)cx_alloc_instance(object_getClass(s));
    n->l = VL(s)->l;
    return (id)n;
}

/* ---------------------------------------------------------------- para o export */
#include "avvcomp_desc.h"
static u32 argb_of(void *col) {
    if (!col) return 0xFF000000u;
    size_t n = CGColorGetNumberOfComponents(col);
    const double *k = CGColorGetComponents(col);
    if (!k || n < 2) return 0xFF000000u;
    double r = k[0], g = n >= 4 ? k[1] : k[0], b = n >= 4 ? k[2] : k[0];
#define C8(v) ((u32)((v) < 0 ? 0 : (v) > 1 ? 255 : (v) * 255 + 0.5))
    return 0xFF000000u | C8(r) << 16 | C8(g) << 8 | C8(b);
#undef C8
}
int cx_vcomp_is(id o) { return o && (cx_is_subclass(object_getClass(o), &AVVideoComposition_c)); }
int cx_vcomp_describe(void *vcp, vcomp_desc *d) {
    id vc = (id)vcp;
    memset(d, 0, sizeof *d);
    if (!cx_vcomp_is(vc)) return -1;
    d->w = (int)(VC(vc)->size.w + 0.5); d->h = (int)(VC(vc)->size.h + 0.5);
    d->frame = secs(VC(vc)->fd);
    if (d->w < 16 || d->h < 16 || d->frame <= 0) return -1;
    id ins = VC(vc)->instructions;
    u64 n = ins ? MSG0(u64, ins, "count") : 0;
    for (u64 i = 0; i < n && d->ninst < VCD_MAX_INST; i++) {
        id in = MSG(id, ins, "objectAtIndex:", i);
        if (!in || !cx_is_subclass(object_getClass(in), &AVVideoCompositionInstruction_c)) return -2;   /* compositor próprio do app */
        vcd_inst *o = &d->inst[d->ninst++];
        o->t0 = secs(VI(in)->range.start);
        o->t1 = o->t0 + secs(VI(in)->range.duration);
        o->bg = argb_of(VI(in)->bg);
        id ls = VI(in)->layers;
        u64 nl = ls ? MSG0(u64, ls, "count") : 0;
        for (u64 j = 0; j < nl && o->nlayers < VCD_MAX_LAYERS; j++) {
            id l = MSG(id, ls, "objectAtIndex:", j);
            if (l && cx_is_subclass(object_getClass(l), &AVVideoCompositionLayerInstruction_c)) o->layers[o->nlayers++] = VL(l)->l;
        }
    }
    return 0;
}

static void K(cx_class_t *k, cx_class_t *m, const char *n, cx_class_t *sup, u32 sz) { if (!cx_class_named(n)) cx_define_class(k, m, n, sup, sz); }
#define A(k, s, f) cx_add_method(&k##_c, s, (void *)(f))
#define AC(k, s, f) cx_add_cmethod(&k##_c, s, (void *)(f))
void cx_videocomposition_init(void) {
    cx_class_t *ob = cx_class_named("NSObject");
    K(&AVVideoComposition_c, &AVVideoComposition_m, "AVVideoComposition", ob, sizeof(vcomp_t) - 8);
    K(&AVMutableVideoComposition_c, &AVMutableVideoComposition_m, "AVMutableVideoComposition", &AVVideoComposition_c, 0);
    AC(AVVideoComposition, "videoCompositionWithPropertiesOfAsset:", vc_with_asset);
    AC(AVMutableVideoComposition, "videoComposition", vc_make);
    A(AVVideoComposition, "renderSize", vc_size); A(AVVideoComposition, "frameDuration", vc_fd); A(AVVideoComposition, "instructions", vc_inst);
    A(AVVideoComposition, "renderScale", vc_scale); A(AVVideoComposition, "sourceTrackIDForFrameTiming", vc_ttrack);
    A(AVVideoComposition, "animationTool", vc_tool); A(AVVideoComposition, "customVideoCompositorClass", vc_comp);
    A(AVVideoComposition, "colorPrimaries", vc_prim); A(AVVideoComposition, "colorYCbCrMatrix", vc_matrix); A(AVVideoComposition, "colorTransferFunction", vc_transfer);
    A(AVVideoComposition, "isValidForAsset:timeRange:validationDelegate:", vc_valid);
    A(AVVideoComposition, "copyWithZone:", vc_copy); A(AVVideoComposition, "copy", vc_copy);
    A(AVVideoComposition, "mutableCopyWithZone:", vc_mcopy); A(AVVideoComposition, "mutableCopy", vc_mcopy);
    A(AVMutableVideoComposition, "setRenderSize:", vc_set_size); A(AVMutableVideoComposition, "setFrameDuration:", vc_set_fd);
    A(AVMutableVideoComposition, "setInstructions:", vc_set_inst); A(AVMutableVideoComposition, "setRenderScale:", vc_set_scale);
    A(AVMutableVideoComposition, "setSourceTrackIDForFrameTiming:", vc_set_ttrack); A(AVMutableVideoComposition, "setAnimationTool:", vc_set_tool);
    A(AVMutableVideoComposition, "setCustomVideoCompositorClass:", vc_set_comp); A(AVMutableVideoComposition, "setColorPrimaries:", vc_set_prim);
    A(AVMutableVideoComposition, "setColorYCbCrMatrix:", vc_set_matrix); A(AVMutableVideoComposition, "setColorTransferFunction:", vc_set_transfer);

    K(&AVVideoCompositionInstruction_c, &AVVideoCompositionInstruction_m, "AVVideoCompositionInstruction", ob, sizeof(vinst_t) - 8);
    K(&AVMutableVideoCompositionInstruction_c, &AVMutableVideoCompositionInstruction_m, "AVMutableVideoCompositionInstruction", &AVVideoCompositionInstruction_c, 0);
    AC(AVMutableVideoCompositionInstruction, "videoCompositionInstruction", vi_make);
    A(AVVideoCompositionInstruction, "init", vi_init);
    A(AVVideoCompositionInstruction, "timeRange", vi_range); A(AVVideoCompositionInstruction, "layerInstructions", vi_layers);
    A(AVVideoCompositionInstruction, "backgroundColor", vi_bg); A(AVVideoCompositionInstruction, "enablePostProcessing", vi_post);
    A(AVMutableVideoCompositionInstruction, "setTimeRange:", vi_set_range); A(AVMutableVideoCompositionInstruction, "setLayerInstructions:", vi_set_layers);
    A(AVMutableVideoCompositionInstruction, "setBackgroundColor:", vi_set_bg); A(AVMutableVideoCompositionInstruction, "setEnablePostProcessing:", vi_set_post);

    K(&AVVideoCompositionLayerInstruction_c, &AVVideoCompositionLayerInstruction_m, "AVVideoCompositionLayerInstruction", ob, sizeof(vlayer_t) - 8);
    K(&AVMutableVideoCompositionLayerInstruction_c, &AVMutableVideoCompositionLayerInstruction_m, "AVMutableVideoCompositionLayerInstruction", &AVVideoCompositionLayerInstruction_c, 0);
    AC(AVMutableVideoCompositionLayerInstruction, "videoCompositionLayerInstruction", vl_make);
    AC(AVMutableVideoCompositionLayerInstruction, "videoCompositionLayerInstructionWithAssetTrack:", vl_with_track);
    A(AVVideoCompositionLayerInstruction, "trackID", vl_track);
    A(AVVideoCompositionLayerInstruction, "getTransformRampForTime:startTransform:endTransform:timeRange:", vl_get_transform);
    A(AVVideoCompositionLayerInstruction, "getOpacityRampForTime:startOpacity:endOpacity:timeRange:", vl_get_opacity);
    A(AVVideoCompositionLayerInstruction, "getCropRectangleRampForTime:startCropRectangle:endCropRectangle:timeRange:", vl_get_crop);
    A(AVVideoCompositionLayerInstruction, "copyWithZone:", vl_copy);
    A(AVMutableVideoCompositionLayerInstruction, "setTrackID:", vl_set_track);
    A(AVMutableVideoCompositionLayerInstruction, "setTransform:atTime:", vl_set_transform);
    A(AVMutableVideoCompositionLayerInstruction, "setTransformRampFromStartTransform:toEndTransform:timeRange:", vl_ramp_transform);
    A(AVMutableVideoCompositionLayerInstruction, "setOpacity:atTime:", vl_set_opacity);
    A(AVMutableVideoCompositionLayerInstruction, "setOpacityRampFromStartOpacity:toEndOpacity:timeRange:", vl_ramp_opacity);
    A(AVMutableVideoCompositionLayerInstruction, "setCropRectangle:atTime:", vl_set_crop);
    A(AVMutableVideoCompositionLayerInstruction, "setCropRectangleRampFromStartCropRectangle:toEndCropRectangle:timeRange:", vl_ramp_crop);
}
