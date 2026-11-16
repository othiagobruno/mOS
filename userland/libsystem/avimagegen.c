/*
 * mOS — AVAssetImageGenerator (AVFoundation): quadro de um vídeo como CGImage (miniaturas, linha do tempo).
 *
 * O quadro vem de avvideo.c (MP4/MOV, H.264 pelo OpenH264 corrigido): o quadro exibido no tempo pedido (o último com
 * pts <= t, que satisfaz qualquer tolerância), em BGRA com a faixa e a matriz do fluxo, girado pelo preferredTransform
 * quando appliesPreferredTrackTransform é YES e reduzido para caber em maximumSize. actualTime é o pts do quadro.
 * Asset sem vídeo decodificável: copyCGImageAtTime devolve NULL e AVErrorOperationNotSupportedForAsset (-11838), e os
 * geradores assíncronos entregam AVAssetImageGeneratorFailed, como a AVFoundation.
 */
#include "foundation.h"
#include "libc.h"
#include "avvideo.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
IMP class_getMethodImplementation(cx_class_t *c, SEL s);

cx_class_t AVAssetImageGenerator_c __asm__("OBJC_CLASS_$_AVAssetImageGenerator"),
    AVAssetImageGenerator_m __asm__("OBJC_METACLASS_$_AVAssetImageGenerator");

typedef struct { long long value; int timescale; unsigned flags; long long epoch; } ig_cmtime_t;
typedef struct { double w, h; } ig_size_t;
typedef struct {
    cx_class_t *isa;
    id asset;
    int applies_transform, cancelled;
    ig_size_t max;
    ig_cmtime_t tol_before, tol_after;
    id aperture;
    avv_t *vid;                /* aberto no primeiro pedido */
    int vid_err;
} ig_t;
typedef struct { void *isa; int flags, reserved; void (*invoke)(void *blk, ...); } ig_block_t;

static const ig_cmtime_t kPosInf = {0, 0, 0x5, 0};               /* kCMTimePositiveInfinity: válido | +inf */

static id ig_error(void) {
    id info = MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary");
    if (info) {
        MSG(void, info, "setObject:forKey:", cx_str("A operação não é suportada para esta mídia."), cx_str("NSLocalizedDescription"));
        MSG(void, info, "setObject:forKey:", cx_str("O asset não tem uma faixa de vídeo H.264 (MP4/MOV) que o mOS decodifique."), cx_str("NSLocalizedFailureReason"));
    }
    return MSG(id, (id)cx_class_named("NSError"), "errorWithDomain:code:userInfo:", cx_str("AVFoundationErrorDomain"), (long long)-11838, info);
}
static id ig_init(id s, SEL c, id asset) {
    (void)c;
    ig_t *g = (ig_t *)s;
    g->asset = asset ? objc_retain(asset) : 0;
    g->tol_before = kPosInf;
    g->tol_after = kPosInf;
    return s;
}
static id ig_new(id k, SEL c, id asset) { (void)k; return objc_autorelease(ig_init(cx_alloc_instance(&AVAssetImageGenerator_c), c, asset)); }
static void ig_dealloc(id s, SEL c) {
    (void)c;
    ig_t *g = (ig_t *)s;
    if (g->asset) objc_release(g->asset);
    if (g->aperture) objc_release(g->aperture);
    if (g->vid) avv_close(g->vid);
    IMP d = class_getMethodImplementation(cx_class_named("NSObject"), sel_registerName("dealloc"));
    if (d) ((void (*)(id, SEL))d)(s, sel_registerName("dealloc"));
}
static id ig_asset(id s, SEL c) { (void)c; return ((ig_t *)s)->asset; }
static BOOL ig_applies(id s, SEL c) { (void)c; return ((ig_t *)s)->applies_transform ? YES : NO; }
static void ig_set_applies(id s, SEL c, BOOL v) { (void)c; ((ig_t *)s)->applies_transform = v; }
static ig_size_t ig_max(id s, SEL c) { (void)c; return ((ig_t *)s)->max; }
static void ig_set_max(id s, SEL c, ig_size_t v) { (void)c; ((ig_t *)s)->max = v; }
static ig_cmtime_t ig_tol_b(id s, SEL c) { (void)c; return ((ig_t *)s)->tol_before; }
static ig_cmtime_t ig_tol_a(id s, SEL c) { (void)c; return ((ig_t *)s)->tol_after; }
static void ig_set_tol_b(id s, SEL c, ig_cmtime_t t) { (void)c; ((ig_t *)s)->tol_before = t; }
static void ig_set_tol_a(id s, SEL c, ig_cmtime_t t) { (void)c; ((ig_t *)s)->tol_after = t; }
static id ig_aperture(id s, SEL c) { (void)c; id a = ((ig_t *)s)->aperture; return a ? a : cx_str("AVAssetImageGeneratorApertureModeCleanAperture"); }
static void ig_set_aperture(id s, SEL c, id a) {
    (void)c;
    ig_t *g = (ig_t *)s;
    id n = a ? objc_retain(MSG0(id, a, "copy")) : 0;
    if (g->aperture) objc_release(g->aperture);
    g->aperture = n;
}
void *CGDataProviderCreateWithData(void *info, const void *data, size_t size, void (*release)(void *, const void *, size_t));
void *CGImageCreate(size_t w, size_t h, size_t bpc, size_t bpp, size_t bpr, const void *space, u32 info, void *prov, const void *dec, _Bool interp, int intent);
void *CGColorSpaceCreateDeviceRGB(void);
void CGColorSpaceRelease(void *c);
void CGDataProviderRelease(void *p);
void CGImageRelease(void *i);
static void ig_free_pixels(void *info, const void *data, size_t n) { (void)info; (void)n; free((void *)data); }
static const char *ig_path(id asset) {
    id url = asset && MSG(BOOL, asset, "respondsToSelector:", sel_registerName("URL")) ? MSG0(id, asset, "URL") : 0;
    id path = url ? MSG0(id, url, "path") : 0;
    return path ? cx_cstr(path) : 0;
}
/* +1, ou 0 com o erro */
static void *ig_make(ig_t *g, ig_cmtime_t t, ig_cmtime_t *actual, id *err) {
    if (actual) memset(actual, 0, sizeof *actual);
    if (!g->vid && !g->vid_err) {
        int e = 0;
        g->vid = avv_open_path(ig_path(g->asset), &e);
        if (!g->vid) g->vid_err = e ? e : -11838;
    }
    if (!g->vid) { if (err) *err = ig_error(); return 0; }
    double sec = t.timescale > 0 ? (double)t.value / t.timescale : 0;
    if ((t.flags & 0x5) == 0x5) { double d; avv_info(g->vid, 0, 0, &d, 0, 0); sec = d; }   /* +infinito: o último quadro */
    avv_frame f;
    if (avv_frame_at(g->vid, sec, &f)) { if (err) *err = ig_error(); return 0; }
    int rot = g->applies_transform ? avv_rotation(g->vid) : 0;
    double nw = rot == 90 || rot == 270 ? f.h : f.w, nh = rot == 90 || rot == 270 ? f.w : f.h, k = 1;
    if (g->max.w > 0 && nw > g->max.w) k = g->max.w / nw;
    if (g->max.h > 0 && nh * k > g->max.h) k = g->max.h / nh;
    int ow = 0, oh = 0;
    uint8_t *px = 0;
    if (avv_to_bgra(&f, rot, k < 1 ? (int)(nw * k + 0.5) : 0, k < 1 ? (int)(nh * k + 0.5) : 0, &px, &ow, &oh)) { if (err) *err = ig_error(); return 0; }
    void *prov = CGDataProviderCreateWithData(0, px, (size_t)ow * oh * 4, ig_free_pixels);
    void *cs = CGColorSpaceCreateDeviceRGB();
    /* BGRA na memória = kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Little */
    void *img = CGImageCreate((size_t)ow, (size_t)oh, 8, 32, (size_t)ow * 4, cs, 6 | 0x2000, prov, 0, 1, 0);
    CGColorSpaceRelease(cs);
    CGDataProviderRelease(prov);
    if (!img) { if (err) *err = ig_error(); return 0; }
    if (actual) { actual->value = (long long)(f.pts * 600 + (f.pts >= 0 ? 0.5 : -0.5)); actual->timescale = 600; actual->flags = 1; actual->epoch = 0; }
    if (err) *err = 0;
    return img;
}
static void *ig_copy_image(id s, SEL c, ig_cmtime_t t, ig_cmtime_t *actual, id *err) { (void)c; return ig_make((ig_t *)s, t, actual, err); }
/* bloco: (CMTime requested, CGImageRef, CMTime actual, AVAssetImageGeneratorResult, NSError *) */
static void ig_generate(id s, SEL c, id times, ig_block_t *blk) {
    (void)c;
    if (!blk || !blk->invoke) return;
    ig_t *g = (ig_t *)s;
    long n = times ? MSG0(long, times, "count") : 0;
    ig_cmtime_t zero = {0, 0, 0, 0};
    for (long i = 0; i < n; i++) {
        id v = MSG(id, times, "objectAtIndex:", (unsigned long)i);
        ig_cmtime_t req = zero, act = zero;
        if (v) MSG(void, v, "getValue:", (void *)&req);
        if (g->cancelled) { ((void (*)(void *, ig_cmtime_t, void *, ig_cmtime_t, long, id))blk->invoke)(blk, req, 0, zero, 2 /* Cancelled */, 0); continue; }
        id e = 0;
        void *img = ig_make(g, req, &act, &e);
        ((void (*)(void *, ig_cmtime_t, void *, ig_cmtime_t, long, id))blk->invoke)(blk, req, img, act, img ? 0 /* Succeeded */ : 1 /* Failed */, e);
        if (img) CGImageRelease(img);
    }
}
/* macOS 13: (CGImageRef, CMTime actual, NSError *) */
static void ig_generate_one(id s, SEL c, ig_cmtime_t t, ig_block_t *blk) {
    (void)c;
    if (!blk || !blk->invoke) return;
    ig_cmtime_t act = {0, 0, 0, 0};
    id e = 0;
    void *img = ig_make((ig_t *)s, t, &act, &e);
    ((void (*)(void *, void *, ig_cmtime_t, id))blk->invoke)(blk, img, act, e);
    if (img) CGImageRelease(img);
}
static void ig_cancel(id s, SEL c) { (void)c; ((ig_t *)s)->cancelled = 1; }

void cx_avimagegen_init(void) {
    cx_class_t *ob = cx_class_named("NSObject");
    cx_define_class(&AVAssetImageGenerator_c, &AVAssetImageGenerator_m, "AVAssetImageGenerator", ob, (u32)(sizeof(ig_t) - 8));
    cx_add_cmethod(&AVAssetImageGenerator_c, "assetImageGeneratorWithAsset:", (void *)ig_new);
    cx_add_method(&AVAssetImageGenerator_c, "initWithAsset:", (void *)ig_init);
    cx_add_method(&AVAssetImageGenerator_c, "dealloc", (void *)ig_dealloc);
    cx_add_method(&AVAssetImageGenerator_c, "asset", (void *)ig_asset);
    cx_add_method(&AVAssetImageGenerator_c, "appliesPreferredTrackTransform", (void *)ig_applies);
    cx_add_method(&AVAssetImageGenerator_c, "setAppliesPreferredTrackTransform:", (void *)ig_set_applies);
    cx_add_method(&AVAssetImageGenerator_c, "maximumSize", (void *)ig_max);
    cx_add_method(&AVAssetImageGenerator_c, "setMaximumSize:", (void *)ig_set_max);
    cx_add_method(&AVAssetImageGenerator_c, "requestedTimeToleranceBefore", (void *)ig_tol_b);
    cx_add_method(&AVAssetImageGenerator_c, "requestedTimeToleranceAfter", (void *)ig_tol_a);
    cx_add_method(&AVAssetImageGenerator_c, "setRequestedTimeToleranceBefore:", (void *)ig_set_tol_b);
    cx_add_method(&AVAssetImageGenerator_c, "setRequestedTimeToleranceAfter:", (void *)ig_set_tol_a);
    cx_add_method(&AVAssetImageGenerator_c, "apertureMode", (void *)ig_aperture);
    cx_add_method(&AVAssetImageGenerator_c, "setApertureMode:", (void *)ig_set_aperture);
    cx_add_method(&AVAssetImageGenerator_c, "copyCGImageAtTime:actualTime:error:", (void *)ig_copy_image);
    cx_add_method(&AVAssetImageGenerator_c, "generateCGImagesAsynchronouslyForTimes:completionHandler:", (void *)ig_generate);
    cx_add_method(&AVAssetImageGenerator_c, "generateCGImageAsynchronouslyForTime:completionHandler:", (void *)ig_generate_one);
    cx_add_method(&AVAssetImageGenerator_c, "cancelAllCGImageGeneration", (void *)ig_cancel);
}
