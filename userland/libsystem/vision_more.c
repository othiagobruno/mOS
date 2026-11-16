/*
 * mOS libSystem — Vision: pedidos que não precisam de modelo de aprendizado de máquina (algoritmos em vision_core2.c).
 *  - VNDetectContoursRequest -> VNContoursObservation/VNContour: regiões e buracos com hierarquia, pontos normalizados
 *    (origem embaixo à esquerda), polygonApproximationWithEpsilon: (Douglas-Peucker), normalizedPath.
 *  - VNGenerateObjectnessBasedSaliencyImageRequest e VNGenerateAttentionBasedSaliencyImageRequest -> VNSaliencyImageObservation:
 *    mapa 68x68 em float (contraste de frequência, não a rede da Apple) e salientObjects.
 *  - VNTranslationalImageRegistrationRequest -> VNImageTranslationAlignmentObservation (alignmentTransform).
 *  - VNTrackObjectRequest (VNSequenceRequestHandler): correlação normalizada perto da última posição.
 *  - VNDetectHumanRectanglesRequest, VNRecognizedTextObservation/VNRecognizedText, VNHumanObservation,
 *    VNPixelBufferObservation: classes completas; humanos e texto dependem de modelo e saem sem resultados, como
 *    faces e texto no vision.c.
 * Código original do mOS (MIT).
 */
#include "foundation.h"
#include "vision_core2.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void *malloc(size_t n);
void *calloc(size_t n, size_t m);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
typedef struct { double x, y, w, h; } rc_t;
typedef struct { double a, b, c, d, tx, ty; } aff_t;
id cx_vision_obs_new(cx_class_t *k, rc_t box);
id objc_msgSend_stret(id self, SEL op, ...);
unsigned cx_vision_obs_size(void);
unsigned cx_vision_req_size(void);
id cx_vision_req_init(id s, void *completion);
int cx_vision_luma(void *img, int is_pb, int *w, int *h, u8 **y);
void *CGPathCreateMutable(void);
void CGPathMoveToPoint(void *p, const void *m, double x, double y);
void CGPathAddLineToPoint(void *p, const void *m, double x, double y);
void CGPathCloseSubpath(void *p);
int CVPixelBufferCreateWithBytes(void *a, u64 w, u64 h, u32 fmt, void *base, u64 bpr, void (*rel)(void *, const void *), void *ctx, id attrs, void **out);
extern cx_class_t VNObservation_c __asm__("OBJC_CLASS_$_VNObservation");
extern cx_class_t VNImageBasedRequest_c __asm__("OBJC_CLASS_$_VNImageBasedRequest");
extern cx_class_t VNDetectedObjectObservation_c __asm__("OBJC_CLASS_$_VNDetectedObjectObservation");
extern cx_class_t VNRectangleObservation_c __asm__("OBJC_CLASS_$_VNRectangleObservation");

#define CLS(n) cx_class_t n##_c __asm__("OBJC_CLASS_$_" #n), n##_m __asm__("OBJC_METACLASS_$_" #n);
CLS(VNDetectContoursRequest) CLS(VNContoursObservation) CLS(VNContour)
CLS(VNGenerateObjectnessBasedSaliencyImageRequest) CLS(VNGenerateAttentionBasedSaliencyImageRequest)
CLS(VNPixelBufferObservation) CLS(VNSaliencyImageObservation)
CLS(VNImageRegistrationRequest) CLS(VNTargetedImageRequest) CLS(VNTranslationalImageRegistrationRequest)
CLS(VNImageAlignmentObservation) CLS(VNImageTranslationAlignmentObservation)
CLS(VNTrackingRequest) CLS(VNTrackObjectRequest)
CLS(VNDetectHumanRectanglesRequest) CLS(VNHumanObservation) CLS(VNRecognizedTextObservation) CLS(VNRecognizedText)

/* estado extra depois dos campos de VNRequest / VNObservation do vision.c */
typedef struct { float contrast; int dark; i64 maxdim; int upper_only; u8 *tgt; int tw, th; rc_t box; u8 *tmpl; int tpw, tph; int level, last; id input; } rqx_t;
typedef struct { vc_contours cs; float aspect; id *objs; void *pb; id salient; aff_t aff; int upper_only; } obx_t;
#define RX(o) ((rqx_t *)((u8 *)(o) + cx_vision_req_size()))
#define OX(o) ((obx_t *)((u8 *)(o) + cx_vision_obs_size()))
static id marr(void) { return MSG0(id, (id)cx_class_named("NSMutableArray"), "array"); }
static id verr(long code, const char *msg) {
    id ui = MSG(id, (id)cx_class_named("NSDictionary"), "dictionaryWithObject:forKey:", cx_str(msg), cx_str("NSLocalizedDescription"));
    return MSG(id, (id)cx_class_named("NSError"), "errorWithDomain:code:userInfo:", cx_str("com.apple.vis"), code, ui);
}
static int is(id o, cx_class_t *k) { return o && cx_is_subclass(object_getClass(o), k); }

/* ---------------------------------------------------------------- contornos */
typedef struct { cx_class_t *isa; id obs; int index; float *pts; int n; int owned; } vncontour_t;
#define CT(o) ((vncontour_t *)(o))
static id contour_obj(id obs, int index, float *pts, int n, int owned) {
    vncontour_t *c = (vncontour_t *)cx_alloc_instance(&VNContour_c);
    c->obs = obs; c->index = index; c->pts = pts; c->n = n; c->owned = owned;
    return (id)c;
}
static id children_of(id obs, int parent) {
    obx_t *x = OX(obs);
    id a = marr();
    for (int i = 0; i < x->cs.n; i++) if (x->cs.c[i].parent == parent) MSG(void, a, "addObject:", x->objs[i]);
    return a;
}
static i64 ct_count(id s, SEL c) { (void)c; return CT(s)->n; }
static const float *ct_points(id s, SEL c) { (void)c; return CT(s)->pts; }
static id ct_children(id s, SEL c) { (void)c; return CT(s)->obs && CT(s)->index >= 0 ? children_of(CT(s)->obs, CT(s)->index) : marr(); }
static i64 ct_child_count(id s, SEL c) { return (i64)MSG0(u64, ct_children(s, c), "count"); }
static id ct_child_at(id s, SEL c, i64 i, id *err) {
    id a = ct_children(s, c);
    if (i < 0 || (u64)i >= MSG0(u64, a, "count")) { if (err) *err = verr(14, "Índice de contorno inválido."); return 0; }
    if (err) *err = 0;
    return MSG(id, a, "objectAtIndex:", (u64)i);
}
static float ct_aspect(id s, SEL c) { (void)c; return CT(s)->obs ? OX(CT(s)->obs)->aspect : 1; }
static id ct_index_path(id s, SEL c) {
    (void)c;
    u64 path[64]; int n = 0;
    vncontour_t *ct = CT(s);
    if (!ct->obs || ct->index < 0) return MSG0(id, (id)cx_class_named("NSIndexPath"), "new");
    obx_t *x = OX(ct->obs);
    for (int i = ct->index; i >= 0 && n < 64; i = x->cs.c[i].parent) {   /* posição entre os irmãos, do filho ao topo */
        int p = x->cs.c[i].parent, k = 0;
        for (int j = 0; j < i; j++) if (x->cs.c[j].parent == p) k++;
        path[n++] = (u64)k;
    }
    for (int i = 0; i < n / 2; i++) { u64 t = path[i]; path[i] = path[n - 1 - i]; path[n - 1 - i] = t; }
    return MSG(id, (id)cx_class_named("NSIndexPath"), "indexPathWithIndexes:length:", path, (u64)n);
}
static id ct_approx(id s, SEL c, float eps, id *err) {
    (void)c;
    if (err) *err = 0;
    float *o = malloc(sizeof(float) * 2 * (CT(s)->n + 1));
    if (!o) return 0;
    int m = vc_simplify(CT(s)->pts, CT(s)->n, eps, o);
    return objc_autorelease(contour_obj(CT(s)->obs, -1, o, m, 1));
}
static void *path_of(const float *p, int n) {
    void *path = CGPathCreateMutable();
    for (int i = 0; i < n; i++) { if (!i) CGPathMoveToPoint(path, 0, p[0], p[1]); else CGPathAddLineToPoint(path, 0, p[2 * i], p[2 * i + 1]); }
    if (n) CGPathCloseSubpath(path);
    return path;
}
static void *ct_path(id s, SEL c) { (void)c; void *p = path_of(CT(s)->pts, CT(s)->n); return p ? (void *)objc_autorelease((id)p) : 0; }
static void ct_dealloc(id s, SEL c) {
    if (CT(s)->owned) free(CT(s)->pts);
    IMP d = cx_lookup(cx_class_named("NSObject"), c);
    if (d) ((void (*)(id, SEL))d)(s, c);
}
static i64 co_count(id s, SEL c) { (void)c; return OX(s)->cs.n; }
static id co_top(id s, SEL c) { (void)c; return children_of(s, -1); }
static i64 co_top_count(id s, SEL c) { return (i64)MSG0(u64, co_top(s, c), "count"); }
static id co_at(id s, SEL c, i64 i, id *err) {
    (void)c;
    if (i < 0 || i >= OX(s)->cs.n) { if (err) *err = verr(14, "Índice de contorno inválido."); return 0; }
    if (err) *err = 0;
    return OX(s)->objs[i];
}
static id co_at_path(id s, SEL c, id path, id *err) {
    u64 n = path ? MSG0(u64, path, "length") : 0;
    id cur = 0;
    for (u64 k = 0; k < n; k++) {
        u64 i = MSG(u64, path, "indexAtPosition:", k);
        id list = k ? ct_children(cur, c) : co_top(s, c);
        if (i >= MSG0(u64, list, "count")) { if (err) *err = verr(14, "Caminho de contorno inválido."); return 0; }
        cur = MSG(id, list, "objectAtIndex:", i);
    }
    if (err) *err = cur ? 0 : verr(14, "Caminho de contorno inválido.");
    return cur;
}
static void *co_path(id s, SEL c) {
    (void)c;
    void *path = CGPathCreateMutable();
    for (int i = 0; i < OX(s)->cs.n; i++) {
        vc_contour *k = &OX(s)->cs.c[i];
        for (int j = 0; j < k->npts; j++) { if (!j) CGPathMoveToPoint(path, 0, k->pts[0], k->pts[1]); else CGPathAddLineToPoint(path, 0, k->pts[2 * j], k->pts[2 * j + 1]); }
        if (k->npts) CGPathCloseSubpath(path);
    }
    return path ? (void *)objc_autorelease((id)path) : 0;
}
static float rq_contrast(id s, SEL c) { (void)c; return RX(s)->contrast; }
static void rq_set_contrast(id s, SEL c, float v) { (void)c; RX(s)->contrast = v; }
static BOOL rq_dark(id s, SEL c) { (void)c; return RX(s)->dark ? YES : NO; }
static void rq_set_dark(id s, SEL c, BOOL v) { (void)c; RX(s)->dark = v; }
static i64 rq_maxdim(id s, SEL c) { (void)c; return RX(s)->maxdim; }
static void rq_set_maxdim(id s, SEL c, i64 v) { (void)c; RX(s)->maxdim = v; }
static id contours_init(id s, SEL c) { (void)c; cx_vision_req_init(s, 0); RX(s)->contrast = 2.0f; RX(s)->dark = 1; RX(s)->maxdim = 512; return s; }
static id contours_init_h(id s, SEL c, void *h) { (void)c; cx_vision_req_init(s, h); RX(s)->contrast = 2.0f; RX(s)->dark = 1; RX(s)->maxdim = 512; return s; }
static void run_contours(id q, const u8 *y, int w, int h, id results) {
    /* reduz até maximumImageDimension (média de blocos) */
    int md = RX(q)->maxdim > 0 ? (int)RX(q)->maxdim : 512, f = 1;
    while ((w / f > md || h / f > md) && f < 64) f++;
    int sw = w / f, sh = h / f;
    u8 *small = (u8 *)y;
    if (f > 1) {
        small = malloc((size_t)sw * sh);
        if (!small) return;
        for (int yy = 0; yy < sh; yy++) for (int xx = 0; xx < sw; xx++) {
            int s = 0;
            for (int j = 0; j < f; j++) for (int i = 0; i < f; i++) s += y[(yy * f + j) * w + xx * f + i];
            small[yy * sw + xx] = (u8)(s / (f * f));
        }
    }
    vc_contours cs;
    int rc = vc_find_contours(small, sw, sh, RX(q)->dark, RX(q)->contrast, &cs);
    if (small != y) free(small);
    if (rc) return;
    for (int i = 0; i < cs.n; i++)   /* normaliza: x/w, 1 - y/h */
        for (int j = 0; j < cs.c[i].npts; j++) { cs.c[i].pts[2 * j] /= sw; cs.c[i].pts[2 * j + 1] = 1 - cs.c[i].pts[2 * j + 1] / sh; }
    rc_t box = {0, 0, 1, 1};
    id o = cx_vision_obs_new(&VNContoursObservation_c, box);
    obx_t *x = OX(o);
    x->cs = cs; x->aspect = (float)w / h;
    x->objs = calloc((size_t)(cs.n ? cs.n : 1), sizeof(id));
    for (int i = 0; i < cs.n && x->objs; i++) x->objs[i] = contour_obj(o, i, cs.c[i].pts, cs.c[i].npts, 0);
    MSG(void, results, "addObject:", o);
}

/* ---------------------------------------------------------------- saliência */
static void sal_free(void *ctx, const void *b) { (void)b; free(ctx); }
static void run_saliency(id q, const u8 *y, int w, int h, id results) {
    (void)q;
    float *map = malloc(68 * 68 * sizeof(float)), box[4];
    if (!map) return;
    vc_saliency(y, w, h, map, 68, 68, box);
    void *pb = 0;
    CVPixelBufferCreateWithBytes(0, 68, 68, 0x4C303066 /* 'L00f' */, map, 68 * 4, sal_free, map, 0, &pb);
    if (!pb) { free(map); return; }
    rc_t all = {0, 0, 1, 1};
    id o = cx_vision_obs_new(&VNSaliencyImageObservation_c, all);
    OX(o)->pb = pb;
    id objs = marr();
    if (box[2] > 0) { rc_t b = {box[0], box[1], box[2], box[3]}; MSG(void, objs, "addObject:", cx_vision_obs_new(&VNRectangleObservation_c, b)); }
    OX(o)->salient = objc_retain(objs);
    MSG(void, results, "addObject:", o);
}
static void *pbo_buffer(id s, SEL c) { (void)c; return OX(s)->pb; }
static id pbo_feature(id s, SEL c) { (void)s; (void)c; return 0; }
static id sal_objects(id s, SEL c) { (void)c; return OX(s)->salient ? OX(s)->salient : marr(); }

/* ---------------------------------------------------------------- alinhamento por translação */
static id reg_init(id s, void *img, int pb, void *h) {
    cx_vision_req_init(s, h);
    int w, hh; u8 *y;
    if (!cx_vision_luma(img, pb, &w, &hh, &y)) { RX(s)->tgt = y; RX(s)->tw = w; RX(s)->th = hh; }
    return s;
}
static id reg_cg(id s, SEL c, void *img, id o) { (void)c; (void)o; return reg_init(s, img, 0, 0); }
static id reg_cg_h(id s, SEL c, void *img, id o, void *h) { (void)c; (void)o; return reg_init(s, img, 0, h); }
static id reg_pb(id s, SEL c, void *pb, id o) { (void)c; (void)o; return reg_init(s, pb, 1, 0); }
static id reg_pb_h(id s, SEL c, void *pb, id o, void *h) { (void)c; (void)o; return reg_init(s, pb, 1, h); }
static void run_translation(id q, const u8 *y, int w, int h, id results) {
    rqx_t *x = RX(q);
    if (!x->tgt || x->tw != w || x->th != h) return;   /* tamanhos diferentes: sem observação */
    int dx, dy;
    if (vc_translation(y, x->tgt, w, h, &dx, &dy)) return;
    rc_t all = {0, 0, 1, 1};
    id o = cx_vision_obs_new(&VNImageTranslationAlignmentObservation_c, all);
    aff_t t = {1, 0, 0, 1, dx, -dy};   /* em pixels, eixo y para cima */
    OX(o)->aff = t;
    MSG(void, results, "addObject:", o);
}
static aff_t al_transform(id s, SEL c) { (void)c; return OX(s)->aff; }

/* ---------------------------------------------------------------- rastreamento */
static rc_t box_of(id obs) { return ((rc_t (*)(id, SEL))objc_msgSend_stret)(obs, sel_registerName("boundingBox")); }
static id track_init(id s, id obs, void *h) {
    cx_vision_req_init(s, h);
    if (obs) { RX(s)->box = box_of(obs); RX(s)->input = objc_retain(obs); }
    return s;
}
static id tr_init(id s, SEL c, id obs) { (void)c; return track_init(s, obs, 0); }
static id tr_init_h(id s, SEL c, id obs, void *h) { (void)c; return track_init(s, obs, h); }
static id tr_input(id s, SEL c) { (void)c; return RX(s)->input; }
static void tr_set_input(id s, SEL c, id obs) {
    (void)c;
    if (obs) objc_retain(obs);
    if (RX(s)->input) objc_release(RX(s)->input);
    RX(s)->input = obs;
    if (obs) { RX(s)->box = box_of(obs); free(RX(s)->tmpl); RX(s)->tmpl = 0; }
}
static i64 tr_level(id s, SEL c) { (void)c; return RX(s)->level; }
static void tr_set_level(id s, SEL c, i64 v) { (void)c; RX(s)->level = (int)v; }
static BOOL tr_last(id s, SEL c) { (void)c; return RX(s)->last ? YES : NO; }
static void tr_set_last(id s, SEL c, BOOL v) { (void)c; RX(s)->last = v; }
static void crop(rqx_t *x, const u8 *y, int w, int h, int bx, int by) {
    free(x->tmpl);
    x->tmpl = malloc((size_t)x->tpw * x->tph);
    if (!x->tmpl) return;
    for (int j = 0; j < x->tph; j++) memcpy(x->tmpl + j * x->tpw, y + (by + j) * w + bx, (size_t)x->tpw);
    (void)h;
}
static void run_track(id q, const u8 *y, int w, int h, id results) {
    rqx_t *x = RX(q);
    if (x->box.w <= 0 || x->box.h <= 0) return;
    /* caixa normalizada (y para cima) -> pixels (y para baixo) */
    int bw = (int)(x->box.w * w + 0.5), bh = (int)(x->box.h * h + 0.5), bx = (int)(x->box.x * w + 0.5), by = (int)((1 - x->box.y - x->box.h) * h + 0.5);
    if (bw < 4 || bh < 4) return;
    if (bx < 0) bx = 0;
    if (by < 0) by = 0;
    if (bx + bw > w) bx = w - bw;
    if (by + bh > h) by = h - bh;
    float score = 1;
    if (!x->tmpl || x->tpw != bw || x->tph != bh) { x->tpw = bw; x->tph = bh; crop(x, y, w, h, bx, by); }
    else {
        int r = (bw > bh ? bw : bh) / 2, nx, ny;
        score = vc_track(y, w, h, x->tmpl, bw, bh, bx, by, r, &nx, &ny);
        bx = nx; by = ny;
        crop(x, y, w, h, bx, by);   /* o modelo acompanha a aparência */
    }
    rc_t nb = {(double)bx / w, 1 - (double)(by + bh) / h, (double)bw / w, (double)bh / h};
    x->box = nb;
    id o = cx_vision_obs_new(&VNDetectedObjectObservation_c, nb);
    *(float *)((u8 *)o + 8 + sizeof(rc_t)) = score < 0 ? 0 : score;   /* confidence: logo depois de box no vnobs_t */
    tr_set_input(q, 0, o);
    MSG(void, results, "addObject:", o);
}

/* ---------------------------------------------------------------- humanos e texto (sem modelo) */
static BOOL hu_upper(id s, SEL c) { (void)c; return RX(s)->upper_only ? YES : NO; }
static void hu_set_upper(id s, SEL c, BOOL v) { (void)c; RX(s)->upper_only = v; }
static BOOL ho_upper(id s, SEL c) { (void)c; return OX(s)->upper_only ? YES : NO; }
static id rt_candidates(id s, SEL c, u64 n) { (void)s; (void)c; (void)n; return marr(); }
typedef struct { cx_class_t *isa; id string; float confidence; } vnrtext_t;
static id rtx_string(id s, SEL c) { (void)c; return ((vnrtext_t *)s)->string; }
static float rtx_conf(id s, SEL c) { (void)c; return ((vnrtext_t *)s)->confidence; }
static id rtx_box(id s, SEL c, u64 loc, u64 len, id *err) { (void)s; (void)c; (void)loc; (void)len; if (err) *err = verr(3, "Sem a caixa desse trecho."); return 0; }

/* ---------------------------------------------------------------- despacho (chamado pelo run_requests do vision.c) */
int cx_vision_more_run(id q, const u8 *y, int w, int h, id results) {
    if (!y) return 0;
    if (is(q, &VNDetectContoursRequest_c)) { run_contours(q, y, w, h, results); return 1; }
    if (is(q, &VNGenerateObjectnessBasedSaliencyImageRequest_c) || is(q, &VNGenerateAttentionBasedSaliencyImageRequest_c)) { run_saliency(q, y, w, h, results); return 1; }
    if (is(q, &VNTranslationalImageRegistrationRequest_c)) { run_translation(q, y, w, h, results); return 1; }
    if (is(q, &VNTrackObjectRequest_c)) { run_track(q, y, w, h, results); return 1; }
    return 0;
}

static void K(cx_class_t *k, cx_class_t *m, const char *name, cx_class_t *super, u32 size) { if (!cx_class_named(name)) cx_define_class(k, m, name, super, size); }
#define A(k, s, f) cx_add_method(&k##_c, s, (void *)(f))
void cx_vision_more_init(void) {
    cx_class_t *ob = cx_class_named("NSObject");
    u32 rx = (u32)sizeof(rqx_t), ox = (u32)sizeof(obx_t);
    K(&VNDetectContoursRequest_c, &VNDetectContoursRequest_m, "VNDetectContoursRequest", &VNImageBasedRequest_c, rx);
    A(VNDetectContoursRequest, "init", contours_init); A(VNDetectContoursRequest, "initWithCompletionHandler:", contours_init_h);
    A(VNDetectContoursRequest, "contrastAdjustment", rq_contrast); A(VNDetectContoursRequest, "setContrastAdjustment:", rq_set_contrast);
    A(VNDetectContoursRequest, "detectsDarkOnLight", rq_dark); A(VNDetectContoursRequest, "setDetectsDarkOnLight:", rq_set_dark);
    A(VNDetectContoursRequest, "maximumImageDimension", rq_maxdim); A(VNDetectContoursRequest, "setMaximumImageDimension:", rq_set_maxdim);
    K(&VNContoursObservation_c, &VNContoursObservation_m, "VNContoursObservation", &VNObservation_c, ox);
    A(VNContoursObservation, "contourCount", co_count); A(VNContoursObservation, "topLevelContourCount", co_top_count);
    A(VNContoursObservation, "topLevelContours", co_top); A(VNContoursObservation, "contourAtIndex:error:", co_at);
    A(VNContoursObservation, "contourAtIndexPath:error:", co_at_path); A(VNContoursObservation, "normalizedPath", co_path);
    K(&VNContour_c, &VNContour_m, "VNContour", ob, (u32)(sizeof(vncontour_t) - 8));
    A(VNContour, "pointCount", ct_count); A(VNContour, "normalizedPoints", ct_points); A(VNContour, "childContours", ct_children);
    A(VNContour, "childContourCount", ct_child_count); A(VNContour, "childContourAtIndex:error:", ct_child_at);
    A(VNContour, "aspectRatio", ct_aspect); A(VNContour, "indexPath", ct_index_path);
    A(VNContour, "polygonApproximationWithEpsilon:error:", ct_approx); A(VNContour, "normalizedPath", ct_path); A(VNContour, "dealloc", ct_dealloc);

    K(&VNGenerateObjectnessBasedSaliencyImageRequest_c, &VNGenerateObjectnessBasedSaliencyImageRequest_m, "VNGenerateObjectnessBasedSaliencyImageRequest", &VNImageBasedRequest_c, rx);
    K(&VNGenerateAttentionBasedSaliencyImageRequest_c, &VNGenerateAttentionBasedSaliencyImageRequest_m, "VNGenerateAttentionBasedSaliencyImageRequest", &VNImageBasedRequest_c, rx);
    K(&VNPixelBufferObservation_c, &VNPixelBufferObservation_m, "VNPixelBufferObservation", &VNObservation_c, ox);
    A(VNPixelBufferObservation, "pixelBuffer", pbo_buffer); A(VNPixelBufferObservation, "featureName", pbo_feature);
    K(&VNSaliencyImageObservation_c, &VNSaliencyImageObservation_m, "VNSaliencyImageObservation", &VNPixelBufferObservation_c, 0);
    A(VNSaliencyImageObservation, "salientObjects", sal_objects);

    K(&VNTargetedImageRequest_c, &VNTargetedImageRequest_m, "VNTargetedImageRequest", &VNImageBasedRequest_c, rx);
    K(&VNImageRegistrationRequest_c, &VNImageRegistrationRequest_m, "VNImageRegistrationRequest", &VNTargetedImageRequest_c, 0);
    K(&VNTranslationalImageRegistrationRequest_c, &VNTranslationalImageRegistrationRequest_m, "VNTranslationalImageRegistrationRequest", &VNImageRegistrationRequest_c, 0);
    A(VNTargetedImageRequest, "initWithTargetedCGImage:options:", reg_cg); A(VNTargetedImageRequest, "initWithTargetedCGImage:options:completionHandler:", reg_cg_h);
    A(VNTargetedImageRequest, "initWithTargetedCVPixelBuffer:options:", reg_pb); A(VNTargetedImageRequest, "initWithTargetedCVPixelBuffer:options:completionHandler:", reg_pb_h);
    K(&VNImageAlignmentObservation_c, &VNImageAlignmentObservation_m, "VNImageAlignmentObservation", &VNObservation_c, ox);
    K(&VNImageTranslationAlignmentObservation_c, &VNImageTranslationAlignmentObservation_m, "VNImageTranslationAlignmentObservation", &VNImageAlignmentObservation_c, 0);
    A(VNImageTranslationAlignmentObservation, "alignmentTransform", al_transform);

    K(&VNTrackingRequest_c, &VNTrackingRequest_m, "VNTrackingRequest", &VNImageBasedRequest_c, rx);
    A(VNTrackingRequest, "inputObservation", tr_input); A(VNTrackingRequest, "setInputObservation:", tr_set_input);
    A(VNTrackingRequest, "trackingLevel", tr_level); A(VNTrackingRequest, "setTrackingLevel:", tr_set_level);
    A(VNTrackingRequest, "isLastFrame", tr_last); A(VNTrackingRequest, "setLastFrame:", tr_set_last);
    K(&VNTrackObjectRequest_c, &VNTrackObjectRequest_m, "VNTrackObjectRequest", &VNTrackingRequest_c, 0);
    A(VNTrackObjectRequest, "initWithDetectedObjectObservation:", tr_init); A(VNTrackObjectRequest, "initWithDetectedObjectObservation:completionHandler:", tr_init_h);

    K(&VNDetectHumanRectanglesRequest_c, &VNDetectHumanRectanglesRequest_m, "VNDetectHumanRectanglesRequest", &VNImageBasedRequest_c, rx);
    A(VNDetectHumanRectanglesRequest, "upperBodyOnly", hu_upper); A(VNDetectHumanRectanglesRequest, "setUpperBodyOnly:", hu_set_upper);
    K(&VNHumanObservation_c, &VNHumanObservation_m, "VNHumanObservation", &VNDetectedObjectObservation_c, ox);
    A(VNHumanObservation, "upperBodyOnly", ho_upper);
    K(&VNRecognizedTextObservation_c, &VNRecognizedTextObservation_m, "VNRecognizedTextObservation", &VNRectangleObservation_c, ox);
    A(VNRecognizedTextObservation, "topCandidates:", rt_candidates);
    K(&VNRecognizedText_c, &VNRecognizedText_m, "VNRecognizedText", ob, (u32)(sizeof(vnrtext_t) - 8));
    A(VNRecognizedText, "string", rtx_string); A(VNRecognizedText, "confidence", rtx_conf); A(VNRecognizedText, "boundingBoxForRange:error:", rtx_box);
}
