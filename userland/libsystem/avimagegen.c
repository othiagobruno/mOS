/*
 * mOS — AVAssetImageGenerator (AVFoundation): quadro de um vídeo como CGImage (miniaturas).
 *
 * O AVAsset do mOS (avfaudio.c) lê áudio; não há demultiplexador de MP4/MOV que entregue as amostras de vídeo ao
 * decodificador H.264 (h264sw.c). Então o gerador guarda as opções da API e, ao pedir um quadro, falha como a
 * AVFoundation falha com um asset sem vídeo decodificável: copyCGImageAtTime:actualTime:error: devolve NULL e
 * AVErrorOperationNotSupportedForAsset (-11838); generateCGImagesAsynchronouslyForTimes: chama o bloco com
 * AVAssetImageGeneratorFailed para cada tempo; o generateCGImageAsynchronouslyForTime: recebe o mesmo erro.
 */
#include "foundation.h"
#include "libc.h"

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
} ig_t;
typedef struct { void *isa; int flags, reserved; void (*invoke)(void *blk, ...); } ig_block_t;

static const ig_cmtime_t kPosInf = {0, 0, 0x5, 0};               /* kCMTimePositiveInfinity: válido | +inf */

static id ig_error(void) {
    id info = MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary");
    if (info) {
        MSG(void, info, "setObject:forKey:", cx_str("A operação não é suportada para esta mídia."), cx_str("NSLocalizedDescription"));
        MSG(void, info, "setObject:forKey:", cx_str("O mOS ainda não decodifica quadros de vídeo de um AVAsset."), cx_str("NSLocalizedFailureReason"));
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
static void *ig_copy_image(id s, SEL c, ig_cmtime_t t, ig_cmtime_t *actual, id *err) {
    (void)s; (void)c; (void)t;
    if (actual) memset(actual, 0, sizeof *actual);
    if (err) *err = ig_error();
    return 0;
}
/* bloco: (CMTime requested, CGImageRef, CMTime actual, AVAssetImageGeneratorResult, NSError *) */
static void ig_generate(id s, SEL c, id times, ig_block_t *blk) {
    (void)c;
    if (!blk || !blk->invoke) return;
    ig_t *g = (ig_t *)s;
    long n = times ? MSG0(long, times, "count") : 0;
    ig_cmtime_t zero = {0, 0, 0, 0};
    for (long i = 0; i < n; i++) {
        id v = MSG(id, times, "objectAtIndex:", (unsigned long)i);
        ig_cmtime_t req = zero;
        if (v) MSG(void, v, "getValue:", (void *)&req);
        long result = g->cancelled ? 2 /* Cancelled */ : 1 /* Failed */;
        ((void (*)(void *, ig_cmtime_t, void *, ig_cmtime_t, long, id))blk->invoke)(blk, req, 0, zero, result, g->cancelled ? 0 : ig_error());
    }
}
/* macOS 13: (CGImageRef, CMTime actual, NSError *) */
static void ig_generate_one(id s, SEL c, ig_cmtime_t t, ig_block_t *blk) {
    (void)s; (void)c; (void)t;
    if (!blk || !blk->invoke) return;
    ig_cmtime_t zero = {0, 0, 0, 0};
    ((void (*)(void *, void *, ig_cmtime_t, id))blk->invoke)(blk, 0, zero, ig_error());
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
