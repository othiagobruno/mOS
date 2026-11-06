/*
 * mOS libSystem — CoreVideo mínimo (porte do VS Code): CVDisplayLink (o "vsync" do Chromium) e
 * CVPixelBuffer sobre IOSurface (iosurface.c). Mais os stubs de "sem GPU" (Metal/CGL) que fazem o
 * Chromium escolher a composição por software.
 *
 * CVDisplayLink: uma thread do libSystem chama, a cada ~16,7 ms (60 Hz), o callback de cada link
 * ligado, como a thread do CoreVideo no macOS.
 */
#include "foundation.h"
#include "libc.h"

id objc_retain(id o);
void objc_release(id o);
int pthread_create(unsigned long *t, const void *attr, void *(*fn)(void *), void *arg);
int pthread_detach(unsigned long t);
int usleep(unsigned us);
u64 mach_absolute_time(void);

/* ---- constantes */
extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } cv_cfconst_t;
#define CFCONST(var, text) \
    static cv_cfconst_t var##_s = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; \
    const void *var = &var##_s;
CFCONST(kCVPixelBufferWidthKey, "Width") CFCONST(kCVPixelBufferHeightKey, "Height")
CFCONST(kCVPixelBufferPixelFormatTypeKey, "PixelFormatType") CFCONST(kCVPixelBufferIOSurfacePropertiesKey, "IOSurfaceProperties")
CFCONST(kCVPixelBufferBytesPerRowAlignmentKey, "BytesPerRowAlignment") CFCONST(kCVPixelBufferMetalCompatibilityKey, "MetalCompatibility")
CFCONST(kCVPixelBufferOpenGLCompatibilityKey, "OpenGLCompatibility") CFCONST(kCVPixelBufferCGImageCompatibilityKey, "CGImageCompatibility")
CFCONST(kCVPixelBufferPoolMinimumBufferCountKey, "MinimumBufferCount") CFCONST(kCVPixelBufferPoolAllocationThresholdKey, "AllocationThreshold")
CFCONST(kCVImageBufferColorPrimariesKey, "CVImageBufferColorPrimaries")
CFCONST(kCVImageBufferColorPrimaries_ITU_R_709_2, "ITU_R_709_2") CFCONST(kCVImageBufferColorPrimaries_EBU_3213, "EBU_3213")
CFCONST(kCVImageBufferColorPrimaries_SMPTE_C, "SMPTE_C") CFCONST(kCVImageBufferColorPrimaries_DCI_P3, "DCI_P3")
CFCONST(kCVImageBufferColorPrimaries_P3_D65, "P3_D65") CFCONST(kCVImageBufferColorPrimaries_ITU_R_2020, "ITU_R_2020")
CFCONST(kCVImageBufferTransferFunctionKey, "CVImageBufferTransferFunction")
CFCONST(kCVImageBufferTransferFunction_ITU_R_709_2, "ITU_R_709_2") CFCONST(kCVImageBufferTransferFunction_SMPTE_240M_1995, "SMPTE_240M_1995")
CFCONST(kCVImageBufferTransferFunction_UseGamma, "UseGamma") CFCONST(kCVImageBufferTransferFunction_ITU_R_2020, "ITU_R_2020")
CFCONST(kCVImageBufferTransferFunction_SMPTE_ST_428_1, "SMPTE_ST_428_1") CFCONST(kCVImageBufferTransferFunction_SMPTE_ST_2084_PQ, "SMPTE_ST_2084_PQ")
CFCONST(kCVImageBufferTransferFunction_ITU_R_2100_HLG, "ITU_R_2100_HLG") CFCONST(kCVImageBufferTransferFunction_Linear, "Linear")
CFCONST(kCVImageBufferTransferFunction_sRGB, "IEC_sRGB")
CFCONST(kCVImageBufferGammaLevelKey, "CVImageBufferGammaLevel")
CFCONST(kCVImageBufferYCbCrMatrixKey, "CVImageBufferYCbCrMatrix")
CFCONST(kCVImageBufferYCbCrMatrix_ITU_R_709_2, "ITU_R_709_2") CFCONST(kCVImageBufferYCbCrMatrix_ITU_R_601_4, "ITU_R_601_4")
CFCONST(kCVImageBufferYCbCrMatrix_SMPTE_240M_1995, "SMPTE_240M_1995") CFCONST(kCVImageBufferYCbCrMatrix_ITU_R_2020, "ITU_R_2020")
CFCONST(kCVImageBufferMasteringDisplayColorVolumeKey, "MasteringDisplayColorVolume")
CFCONST(kCVImageBufferContentLightLevelInfoKey, "ContentLightLevelInfo")
CFCONST(kCVImageBufferAmbientViewingEnvironmentKey, "AmbientViewingEnvironment")
CFCONST(kCVImageBufferChromaLocationTopFieldKey, "CVImageBufferChromaLocationTopField")
CFCONST(kCVImageBufferCGColorSpaceKey, "CVImageBufferCGColorSpace")

#define kCVReturnSuccess 0
#define kCVReturnError -6660
#define kCVReturnInvalidArgument -6661
#define kCVReturnUnsupported -6670

/* ================================================================ CVDisplayLink */
typedef struct {
    u32 version; i32 videoTimeScale; i64 videoTime; u64 hostTime; double rateScalar; i64 videoRefreshPeriod;
    i16 subframes, subframeDivisor; u32 counter, type, flags; i16 hours, minutes, seconds, frames;
    u64 flags2, reserved;
} cvts_t;                                    /* CVTimeStamp (80 bytes) */
typedef struct { i64 value; i32 scale, flags; } cvtime_t;
typedef int (*cvcb_t)(void *link, const cvts_t *now, const cvts_t *out, u64 fin, u64 *fout, void *ctx);
typedef struct { cx_class_t *isa; cvcb_t cb; void *ctx; volatile int running; u32 display; i64 frame; } cvlink_t;

cx_class_t CVDisplayLink_c __asm__("OBJC_CLASS_$_CVDisplayLink");
cx_class_t CVDisplayLink_m __asm__("OBJC_METACLASS_$_CVDisplayLink");

#define PERIOD_US 16667
#define MAX_LINKS 32
static cvlink_t *links[MAX_LINKS];
static volatile int llock, thread_up;
static void L_lock(void) { while (__sync_lock_test_and_set(&llock, 1)) SYS0(SYS_YIELD); }
static void L_unlock(void) { __sync_lock_release(&llock); }

static void stamp(cvts_t *t, i64 frame, u64 host) {
    memset(t, 0, sizeof *t);
    t->videoTimeScale = 1000000;
    t->videoRefreshPeriod = PERIOD_US;
    t->videoTime = frame * PERIOD_US;
    t->hostTime = host;
    t->rateScalar = 1;
    t->flags2 = 1 | 2 | 4;                   /* VideoTimeValid | HostTimeValid | ... */
}
static void *link_thread(void *arg) {
    (void)arg;
    for (;;) {
        usleep(PERIOD_US);
        cvlink_t *run[MAX_LINKS];
        int n = 0;
        L_lock();
        for (int i = 0; i < MAX_LINKS; i++) if (links[i] && links[i]->running && links[i]->cb) { run[n++] = links[i]; objc_retain((id)links[i]); }
        L_unlock();
        u64 now = mach_absolute_time();
        for (int i = 0; i < n; i++) {
            cvts_t a, b;
            cvlink_t *l = run[i];
            l->frame++;
            stamp(&a, l->frame, now);
            stamp(&b, l->frame + 1, now + PERIOD_US * 1000ull);
            u64 fo = 0;
            if (l->running) l->cb(l, &a, &b, 0, &fo, l->ctx);
            objc_release((id)l);
        }
    }
    return 0;
}
static void link_dealloc(id self, SEL c) {
    L_lock();
    for (int i = 0; i < MAX_LINKS; i++) if (links[i] == (cvlink_t *)self) links[i] = 0;
    L_unlock();
    IMP sup = cx_lookup(cx_class_named("NSObject"), c);
    if (sup) ((void (*)(id, SEL))sup)(self, c);
}
int CVDisplayLinkCreateWithCGDisplay(u32 display, cvlink_t **out) {
    if (!out) return kCVReturnInvalidArgument;
    cvlink_t *l = (cvlink_t *)cx_alloc_instance(&CVDisplayLink_c);
    l->display = display;
    L_lock();
    int ok = 0;
    for (int i = 0; i < MAX_LINKS; i++) if (!links[i]) { links[i] = l; ok = 1; break; }
    L_unlock();
    if (!ok) { objc_release((id)l); *out = 0; return kCVReturnError; }
    *out = l;
    return kCVReturnSuccess;
}
int CVDisplayLinkCreateWithCGDisplays(u32 *displays, long n, cvlink_t **out) { return CVDisplayLinkCreateWithCGDisplay(n > 0 && displays ? displays[0] : 1, out); }
int CVDisplayLinkCreateWithActiveCGDisplays(cvlink_t **out) { return CVDisplayLinkCreateWithCGDisplay(1, out); }
cvlink_t *CVDisplayLinkRetain(cvlink_t *l) { if (l) objc_retain((id)l); return l; }
void CVDisplayLinkRelease(cvlink_t *l) { if (l) { l->running = 0; objc_release((id)l); } }
int CVDisplayLinkSetOutputCallback(cvlink_t *l, cvcb_t cb, void *ctx) { if (!l) return kCVReturnInvalidArgument; l->cb = cb; l->ctx = ctx; return kCVReturnSuccess; }
int CVDisplayLinkStart(cvlink_t *l) {
    if (!l) return kCVReturnInvalidArgument;
    l->running = 1;
    if (!__sync_lock_test_and_set(&thread_up, 1)) {
        unsigned long t;
        if (pthread_create(&t, 0, link_thread, 0) == 0) pthread_detach(t);
        else thread_up = 0;
    }
    return kCVReturnSuccess;
}
int CVDisplayLinkStop(cvlink_t *l) { if (!l) return kCVReturnInvalidArgument; l->running = 0; return kCVReturnSuccess; }
u8 CVDisplayLinkIsRunning(cvlink_t *l) { return l && l->running; }
u32 CVDisplayLinkGetCurrentCGDisplay(cvlink_t *l) { return l ? l->display : 0; }
int CVDisplayLinkSetCurrentCGDisplay(cvlink_t *l, u32 d) { if (!l) return kCVReturnInvalidArgument; l->display = d; return kCVReturnSuccess; }
cvtime_t CVDisplayLinkGetNominalOutputVideoRefreshPeriod(cvlink_t *l) { (void)l; cvtime_t t = {PERIOD_US, 1000000, 0}; return t; }
cvtime_t CVDisplayLinkGetOutputVideoLatency(cvlink_t *l) { (void)l; cvtime_t t = {0, 1000000, 0}; return t; }
double CVDisplayLinkGetActualOutputVideoRefreshPeriod(cvlink_t *l) { (void)l; return PERIOD_US / 1e6; }
int CVDisplayLinkGetCurrentTime(cvlink_t *l, cvts_t *out) {
    if (!l || !out) return kCVReturnInvalidArgument;
    stamp(out, l->frame, mach_absolute_time());
    return kCVReturnSuccess;
}
u64 CVGetCurrentHostTime(void) { return mach_absolute_time(); }
double CVGetHostClockFrequency(void) { return 1e9; }

/* ================================================================ CVPixelBuffer */
typedef struct {
    cx_class_t *isa;
    id surf;                    /* IOSurface (ou nil para buffer em memória comum) */
    u8 *bytes;
    u32 w, h, fmt;
    u64 bpr;
    id attach;                  /* anexos (CVBufferSetAttachment) */
    void (*release_cb)(void *ctx, const void *base);
    void *release_ctx;
} cvpb_t;
cx_class_t CVPixelBuffer_c __asm__("OBJC_CLASS_$_CVPixelBuffer");
cx_class_t CVPixelBuffer_m __asm__("OBJC_METACLASS_$_CVPixelBuffer");
cx_class_t CVPixelBufferPool_c __asm__("OBJC_CLASS_$_CVPixelBufferPool");
cx_class_t CVPixelBufferPool_m __asm__("OBJC_METACLASS_$_CVPixelBufferPool");

extern id IOSurfaceCreate(id props);
extern void *IOSurfaceGetBaseAddress(id), *IOSurfaceGetBaseAddressOfPlane(id, u64);
extern u64 IOSurfaceGetWidth(id), IOSurfaceGetHeight(id), IOSurfaceGetBytesPerRow(id), IOSurfaceGetAllocSize(id);
extern u64 IOSurfaceGetPlaneCount(id), IOSurfaceGetWidthOfPlane(id, u64), IOSurfaceGetHeightOfPlane(id, u64), IOSurfaceGetBytesPerRowOfPlane(id, u64);
extern u32 IOSurfaceGetPixelFormat(id);
extern int IOSurfaceLock(id, u32, u32 *), IOSurfaceUnlock(id, u32, u32 *);
extern const void *kIOSurfaceWidth, *kIOSurfaceHeight, *kIOSurfacePixelFormat, *kIOSurfaceBytesPerElement;

u64 CVPixelBufferGetTypeID(void) { return 0x2C0; }
static u64 pb_typeid(id self, SEL c) { (void)self; (void)c; return CVPixelBufferGetTypeID(); }
static void pb_dealloc(id self, SEL c) {
    cvpb_t *p = (cvpb_t *)self;
    if (p->surf) objc_release(p->surf);
    if (p->attach) objc_release(p->attach);
    if (p->release_cb) p->release_cb(p->release_ctx, p->bytes);
    IMP sup = cx_lookup(cx_class_named("NSObject"), c);
    if (sup) ((void (*)(id, SEL))sup)(self, c);
}
static cvpb_t *pb_wrap(id surf) {
    cvpb_t *p = (cvpb_t *)cx_alloc_instance(&CVPixelBuffer_c);
    p->surf = objc_retain(surf);
    p->w = (u32)IOSurfaceGetWidth(surf);
    p->h = (u32)IOSurfaceGetHeight(surf);
    p->bpr = IOSurfaceGetBytesPerRow(surf);
    p->fmt = IOSurfaceGetPixelFormat(surf);
    return p;
}
int CVPixelBufferCreateWithIOSurface(void *alloc, id surf, id attrs, cvpb_t **out) {
    (void)alloc; (void)attrs;
    if (!surf || !out) return kCVReturnInvalidArgument;
    *out = pb_wrap(surf);
    return kCVReturnSuccess;
}
static id surf_props(u32 w, u32 h, u32 fmt) {
    id d = MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary");
    MSG(void, d, "setObject:forKey:", cx_num_i(w), (id)kIOSurfaceWidth);
    MSG(void, d, "setObject:forKey:", cx_num_i(h), (id)kIOSurfaceHeight);
    MSG(void, d, "setObject:forKey:", cx_num_i(fmt), (id)kIOSurfacePixelFormat);
    return d;
}
int CVPixelBufferCreate(void *alloc, u64 w, u64 h, u32 fmt, id attrs, cvpb_t **out) {
    (void)alloc; (void)attrs;
    if (!out || !w || !h) return kCVReturnInvalidArgument;
    id s = IOSurfaceCreate(surf_props((u32)w, (u32)h, fmt));
    if (!s) return kCVReturnError;
    *out = pb_wrap(s);
    objc_release(s);
    return kCVReturnSuccess;
}
int CVPixelBufferCreateWithBytes(void *alloc, u64 w, u64 h, u32 fmt, void *base, u64 bpr,
                                 void (*rel)(void *, const void *), void *ctx, id attrs, cvpb_t **out) {
    (void)alloc; (void)attrs;
    if (!out || !base) return kCVReturnInvalidArgument;
    cvpb_t *p = (cvpb_t *)cx_alloc_instance(&CVPixelBuffer_c);
    p->bytes = base; p->w = (u32)w; p->h = (u32)h; p->fmt = fmt; p->bpr = bpr; p->release_cb = rel; p->release_ctx = ctx;
    *out = p;
    return kCVReturnSuccess;
}
/* planos em memória do chamador (vídeo por software): ainda não */
int CVPixelBufferCreateWithPlanarBytes(void *alloc, u64 w, u64 h, u32 fmt, void *d, u64 ds, u64 np, void **pb, u64 *pw, u64 *ph, u64 *pr,
                                       void *rel, void *ctx, id attrs, cvpb_t **out) {
    (void)alloc; (void)w; (void)h; (void)fmt; (void)d; (void)ds; (void)np; (void)pb; (void)pw; (void)ph; (void)pr; (void)rel; (void)ctx; (void)attrs;
    if (out) *out = 0;
    return kCVReturnUnsupported;
}
id CVPixelBufferGetIOSurface(cvpb_t *p) { return p ? p->surf : 0; }
int CVPixelBufferLockBaseAddress(cvpb_t *p, u64 f) { if (!p) return kCVReturnInvalidArgument; if (p->surf) IOSurfaceLock(p->surf, (u32)f, 0); return 0; }
int CVPixelBufferUnlockBaseAddress(cvpb_t *p, u64 f) { if (!p) return kCVReturnInvalidArgument; if (p->surf) IOSurfaceUnlock(p->surf, (u32)f, 0); return 0; }
void *CVPixelBufferGetBaseAddress(cvpb_t *p) { return !p ? 0 : p->surf ? IOSurfaceGetBaseAddress(p->surf) : p->bytes; }
u64 CVPixelBufferGetBytesPerRow(cvpb_t *p) { return p ? p->bpr : 0; }
u64 CVPixelBufferGetWidth(cvpb_t *p) { return p ? p->w : 0; }
u64 CVPixelBufferGetHeight(cvpb_t *p) { return p ? p->h : 0; }
u32 CVPixelBufferGetPixelFormatType(cvpb_t *p) { return p ? p->fmt : 0; }
u64 CVPixelBufferGetDataSize(cvpb_t *p) { return !p ? 0 : p->surf ? IOSurfaceGetAllocSize(p->surf) : p->bpr * p->h; }
u64 CVPixelBufferGetPlaneCount(cvpb_t *p) { return p && p->surf ? IOSurfaceGetPlaneCount(p->surf) : 0; }
u8 CVPixelBufferIsPlanar(cvpb_t *p) { return CVPixelBufferGetPlaneCount(p) > 0; }
void *CVPixelBufferGetBaseAddressOfPlane(cvpb_t *p, u64 i) { return p && p->surf ? IOSurfaceGetBaseAddressOfPlane(p->surf, i) : 0; }
u64 CVPixelBufferGetBytesPerRowOfPlane(cvpb_t *p, u64 i) { return p && p->surf ? IOSurfaceGetBytesPerRowOfPlane(p->surf, i) : 0; }
u64 CVPixelBufferGetWidthOfPlane(cvpb_t *p, u64 i) { return p && p->surf ? IOSurfaceGetWidthOfPlane(p->surf, i) : 0; }
u64 CVPixelBufferGetHeightOfPlane(cvpb_t *p, u64 i) { return p && p->surf ? IOSurfaceGetHeightOfPlane(p->surf, i) : 0; }
cvpb_t *CVPixelBufferRetain(cvpb_t *p) { if (p) objc_retain((id)p); return p; }
void CVPixelBufferRelease(cvpb_t *p) { if (p) objc_release((id)p); }
typedef struct { double x, y, w, h; } cvrect_t;
cvrect_t CVImageBufferGetCleanRect(cvpb_t *p) { cvrect_t r = {0, 0, p ? p->w : 0, p ? p->h : 0}; return r; }

/* ---- anexos (CVBuffer*) */
void CVBufferSetAttachment(cvpb_t *p, id key, id val, u32 mode) {
    (void)mode;
    if (!p || !key) return;
    if (!p->attach) p->attach = objc_retain(MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary"));
    if (val) MSG(void, p->attach, "setObject:forKey:", val, key);
    else MSG(void, p->attach, "removeObjectForKey:", key);
}
id CVBufferGetAttachment(cvpb_t *p, id key, u32 *mode) {
    if (mode) *mode = 1;
    return p && p->attach && key ? MSG(id, p->attach, "objectForKey:", key) : 0;
}
id CVBufferCopyAttachment(cvpb_t *p, id key, u32 *mode) { id v = CVBufferGetAttachment(p, key, mode); return v ? objc_retain(v) : 0; }
u8 CVBufferHasAttachment(cvpb_t *p, id key) { return CVBufferGetAttachment(p, key, 0) != 0; }
void CVBufferRemoveAttachment(cvpb_t *p, id key) { if (p && p->attach && key) MSG(void, p->attach, "removeObjectForKey:", key); }
void CVBufferRemoveAllAttachments(cvpb_t *p) { if (p && p->attach) MSG0(void, p->attach, "removeAllObjects"); }
void CVBufferPropagateAttachments(cvpb_t *a, cvpb_t *b) {
    if (!a || !b || !a->attach) return;
    nsdict_t *d = (nsdict_t *)a->attach;
    for (u64 i = 0; i < d->n; i++) CVBufferSetAttachment(b, d->keys[i], d->vals[i], 1);
}
cvpb_t *CVBufferRetain(cvpb_t *p) { return CVPixelBufferRetain(p); }
void CVBufferRelease(cvpb_t *p) { CVPixelBufferRelease(p); }

/* ---- CVPixelBufferPool: cria buffers do mesmo tamanho/formato sobre IOSurfaces novos */
typedef struct { cx_class_t *isa; u32 w, h, fmt; } cvpool_t;
static u32 dnum(id d, const void *k, u32 dflt) { id v = d ? MSG(id, d, "objectForKey:", (id)k) : 0; return v ? (u32)MSG0(i64, v, "longLongValue") : dflt; }
int CVPixelBufferPoolCreate(void *alloc, id pool_attrs, id pb_attrs, cvpool_t **out) {
    (void)alloc; (void)pool_attrs;
    if (!out) return kCVReturnInvalidArgument;
    cvpool_t *p = (cvpool_t *)cx_alloc_instance(&CVPixelBufferPool_c);
    p->w = dnum(pb_attrs, kCVPixelBufferWidthKey, 0);
    p->h = dnum(pb_attrs, kCVPixelBufferHeightKey, 0);
    p->fmt = dnum(pb_attrs, kCVPixelBufferPixelFormatTypeKey, 0x42475241);
    *out = p;
    return kCVReturnSuccess;
}
int CVPixelBufferPoolCreatePixelBuffer(void *alloc, cvpool_t *pool, cvpb_t **out) {
    if (!pool) return kCVReturnInvalidArgument;
    return CVPixelBufferCreate(alloc, pool->w, pool->h, pool->fmt, 0, out);
}
int CVPixelBufferPoolCreatePixelBufferWithAuxAttributes(void *alloc, cvpool_t *pool, id aux, cvpb_t **out) {
    (void)aux;
    return CVPixelBufferPoolCreatePixelBuffer(alloc, pool, out);
}
void CVPixelBufferPoolFlush(cvpool_t *pool, u64 opts) { (void)pool; (void)opts; }
cvpool_t *CVPixelBufferPoolRetain(cvpool_t *p) { if (p) objc_retain((id)p); return p; }
void CVPixelBufferPoolRelease(cvpool_t *p) { if (p) objc_release((id)p); }

u8 CVIsCompressedPixelFormatAvailable(u32 fmt) { (void)fmt; return 0; }
id CVPixelFormatDescriptionCreateWithPixelFormatType(void *alloc, u32 fmt) { (void)alloc; (void)fmt; return 0; }

/* ================================================================ sem GPU */
/* Sem Metal e sem OpenGL: o Chromium cai para o SwiftShader / composição por software */
id MTLCreateSystemDefaultDevice(void) { return 0; }
id MTLCopyAllDevices(void) { return objc_retain(MSG0(id, (id)cx_class_named("NSArray"), "array")); }
id MTLCopyAllDevicesWithObserver(id *observer, void *handler) { if (observer) *observer = 0; (void)handler; return MTLCopyAllDevices(); }
CFCONST(MTLDeviceWasAddedNotification, "MTLDeviceWasAddedNotification")
CFCONST(MTLDeviceRemovalRequestedNotification, "MTLDeviceRemovalRequestedNotification")
CFCONST(MTLDeviceWasRemovedNotification, "MTLDeviceWasRemovedNotification")
CFCONST(MTLCommonCounterSetTimestamp, "timestamp")
CFCONST(MTLCommonCounterTimestamp, "GPUTimestamp")
int CGLChoosePixelFormat(const int *attrs, void **pix, int *npix) { (void)attrs; if (pix) *pix = 0; if (npix) *npix = 0; return 10002; /* kCGLBadPixelFormat */ }
int CGLReleasePixelFormat(void *pix) { (void)pix; return 0; }

void cx_corevideo_init(void) {
    cx_class_t *ob = cx_class_named("NSObject");
    cx_define_class(&CVDisplayLink_c, &CVDisplayLink_m, "CVDisplayLink", ob, sizeof(cvlink_t) - 8);
    cx_add_method(&CVDisplayLink_c, "dealloc", (void *)link_dealloc);
    cx_define_class(&CVPixelBuffer_c, &CVPixelBuffer_m, "CVPixelBuffer", ob, sizeof(cvpb_t) - 8);
    cx_add_method(&CVPixelBuffer_c, "dealloc", (void *)pb_dealloc);
    cx_add_method(&CVPixelBuffer_c, "_cfTypeID", (void *)pb_typeid);
    cx_define_class(&CVPixelBufferPool_c, &CVPixelBufferPool_m, "CVPixelBufferPool", ob, sizeof(cvpool_t) - 8);
}
