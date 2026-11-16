/*
 * mOS libSystem — CoreVideo mínimo (porte do VS Code): CVDisplayLink (o "vsync" do Chromium) e
 * CVPixelBuffer sobre IOSurface (iosurface.c). Mais os stubs de "sem OpenGL" (CGL) que fazem o
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
#define kCVReturnInvalidPixelFormat -6680
IMP class_getMethodImplementation(cx_class_t *c, SEL s);

/* ================================================================ CVDisplayLink */
typedef struct {
    u32 version; i32 videoTimeScale; i64 videoTime; u64 hostTime; double rateScalar; i64 videoRefreshPeriod;
    i16 subframes, subframeDivisor; u32 counter, type, flags; i16 hours, minutes, seconds, frames;
    u64 flags2, reserved;
} cvts_t;                                    /* CVTimeStamp (80 bytes) */
typedef struct { i64 value; i32 scale, flags; } cvtime_t;
typedef int (*cvcb_t)(void *link, const cvts_t *now, const cvts_t *out, u64 fin, u64 *fout, void *ctx);
typedef struct { cx_class_t *isa; cvcb_t cb; void *ctx; volatile int running; u32 display; i64 frame; void *handler;
                 u32 period_us;      /* período do modo de vídeo do monitor do link */
                 u64 next_host;      /* próximo disparo (mach_absolute_time, ns) */
} cvlink_t;
/* CVDisplayLinkOutputHandler: bloco (link, now, outputTime, flagsIn, flagsOut) -> CVReturn */
typedef struct { void *isa; int flags, reserved; int (*invoke)(void *blk, void *link, const cvts_t *now, const cvts_t *out, u64 fin, u64 *fout); } cvblock_t;
void *_Block_copy(const void *b);
void _Block_release(const void *b);

cx_class_t CVDisplayLink_c __asm__("OBJC_CLASS_$_CVDisplayLink");
cx_class_t CVDisplayLink_m __asm__("OBJC_METACLASS_$_CVDisplayLink");

#define PERIOD_US 16667           /* 60 Hz quando o monitor não informa a taxa */
#define MAX_LINKS 32
/* período do monitor pelo modo de vídeo atual (CoreGraphics) */
void *CGDisplayCopyDisplayMode(u32 d);
double CGDisplayModeGetRefreshRate(void *mode);
void CGDisplayModeRelease(void *mode);
int CGGetActiveDisplayList(u32 max, u32 *ids, u32 *count);
static u32 display_period_us(u32 d) {
    void *m = CGDisplayCopyDisplayMode(d);
    double hz = m ? CGDisplayModeGetRefreshRate(m) : 0;
    if (m) CGDisplayModeRelease(m);
    if (!(hz >= 1 && hz <= 1000)) return PERIOD_US;
    return (u32)(1e6 / hz + 0.5);
}
static cvlink_t *links[MAX_LINKS];
static volatile int llock, thread_up;
static void L_lock(void) { while (__sync_lock_test_and_set(&llock, 1)) SYS0(SYS_YIELD); }
static void L_unlock(void) { __sync_lock_release(&llock); }

static void stamp(cvts_t *t, i64 frame, u64 host, u32 period) {
    memset(t, 0, sizeof *t);
    t->videoTimeScale = 1000000;
    t->videoRefreshPeriod = period;
    t->videoTime = frame * (i64)period;
    t->hostTime = host;
    t->rateScalar = 1;
    /* VideoTimeValid | HostTimeValid | VideoRefreshPeriodValid | RateScalarValid (sem SMPTE: os campos dele são zero).
     * Antes ia 1|2|4: o período e a escala não eram "válidos" para quem confere as flags (vsync do Chromium) */
    t->flags2 = 1 | 2 | 8 | 16;
}
/* Como no macOS, CVDisplayLinkStop/Release esperam o callback em andamento terminar: o Chrome para o link no
 * destrutor do ui::DisplayLinkMac e libera o contexto logo depois; o callback que ainda rodava escrevia em memória
 * liberada e corrompia o heap (o mapa global de DisplayLinkMac caía ao fechar a janela). Dentro do próprio callback
 * (mesma thread) não espera. */
static cvlink_t *volatile g_in_cb;
static void *volatile g_link_tid;
void *pthread_self(void);
static void wait_callback(cvlink_t *l) {
    __sync_synchronize();
    if (!g_link_tid || pthread_self() == g_link_tid) return;
    while (g_in_cb == l) SYS0(SYS_YIELD);
}
/* uma thread para todos os links; cada um dispara no ritmo do seu monitor (next_host += period) */
static void *link_thread(void *arg) {
    (void)arg;
    g_link_tid = pthread_self();
    for (;;) {
        cvlink_t *run[MAX_LINKS];
        int n = 0;
        u64 now = mach_absolute_time(), wake = now + PERIOD_US * 1000ull;
        L_lock();
        for (int i = 0; i < MAX_LINKS; i++) {
            cvlink_t *l = links[i];
            if (!l || !l->running || !(l->cb || l->handler)) continue;
            if (!l->next_host) l->next_host = now + (u64)l->period_us * 1000ull;
            if (l->next_host <= now) { run[n++] = l; objc_retain((id)l); }
            else if (l->next_host < wake) wake = l->next_host;
        }
        L_unlock();
        for (int i = 0; i < n; i++) {
            cvts_t a, b;
            cvlink_t *l = run[i];
            u64 per = (u64)l->period_us * 1000ull;
            l->frame++;
            stamp(&a, l->frame, now, l->period_us);
            stamp(&b, l->frame + 1, now + per, l->period_us);
            /* próximo vsync; atrasado demais, pula os quadros perdidos em vez de disparar em rajada */
            l->next_host += per;
            if (l->next_host <= now) l->next_host = now + per;
            if (l->next_host < wake) wake = l->next_host;
            u64 fo = 0;
            { extern int cx_ca_diag(void); if (cx_ca_diag() && (l->frame <= 3 || l->frame % 600 == 0)) cx_fdprintf(2, "[ca] CVDisplayLink %p: disparo %llu\n", (void *)l, (unsigned long long)l->frame); }
            g_in_cb = l;
            __sync_synchronize();
            if (l->running && l->cb) l->cb(l, &a, &b, 0, &fo, l->ctx);
            else if (l->running && l->handler) { cvblock_t *h = (cvblock_t *)l->handler; if (h->invoke) h->invoke(h, l, &a, &b, 0, &fo); }
            g_in_cb = 0;
            objc_release((id)l);
        }
        now = mach_absolute_time();
        if (wake > now) usleep((unsigned)((wake - now) / 1000ull));
    }
    return 0;
}
static void link_dealloc(id self, SEL c) {
    if (((cvlink_t *)self)->handler) { _Block_release(((cvlink_t *)self)->handler); ((cvlink_t *)self)->handler = 0; }
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
    l->period_us = display_period_us(display);
    L_lock();
    int ok = 0;
    for (int i = 0; i < MAX_LINKS; i++) if (!links[i]) { links[i] = l; ok = 1; break; }
    L_unlock();
    if (!ok) { objc_release((id)l); *out = 0; return kCVReturnError; }
    *out = l;
    return kCVReturnSuccess;
}
int CVDisplayLinkCreateWithCGDisplays(u32 *displays, long n, cvlink_t **out) { return CVDisplayLinkCreateWithCGDisplay(n > 0 && displays ? displays[0] : 1, out); }
int CVDisplayLinkCreateWithActiveCGDisplays(cvlink_t **out) {
    /* o link começa no monitor principal (o primeiro da lista ativa) */
    u32 ids[8], n = 0;
    if (CGGetActiveDisplayList(8, ids, &n) != 0 || !n) ids[0] = 1;
    return CVDisplayLinkCreateWithCGDisplay(ids[0], out);
}
cvlink_t *CVDisplayLinkRetain(cvlink_t *l) { if (l) objc_retain((id)l); return l; }
void CVDisplayLinkRelease(cvlink_t *l) { if (l) { l->running = 0; wait_callback(l); objc_release((id)l); } }
int CVDisplayLinkSetOutputCallback(cvlink_t *l, cvcb_t cb, void *ctx) {
    if (!l) return kCVReturnInvalidArgument;
    void *old = l->handler;
    if (old) { wait_callback(l); l->handler = 0; _Block_release(old); }
    l->cb = cb;
    l->ctx = ctx;
    return kCVReturnSuccess;
}
/* Versão com bloco: substitui o callback de função (o macOS guarda um só dos dois). */
int CVDisplayLinkSetOutputHandler(cvlink_t *l, void *handler) {
    if (!l) return kCVReturnInvalidArgument;
    void *nb = handler ? _Block_copy(handler) : 0;
    wait_callback(l);
    void *old = l->handler;
    l->handler = nb;
    l->cb = 0;
    l->ctx = 0;
    if (old) _Block_release(old);
    return kCVReturnSuccess;
}
int CVDisplayLinkStart(cvlink_t *l) {
    if (!l) return kCVReturnInvalidArgument;
    { extern int cx_ca_diag(void); if (cx_ca_diag()) cx_fdprintf(2, "[ca] CVDisplayLinkStart %p (tela %u, callback %p)\n", (void *)l, l->display, (void *)l->cb); }
    l->next_host = mach_absolute_time() + (u64)l->period_us * 1000ull;
    l->running = 1;
    if (!__sync_lock_test_and_set(&thread_up, 1)) {
        unsigned long t;
        if (pthread_create(&t, 0, link_thread, 0) == 0) pthread_detach(t);
        else thread_up = 0;
    }
    return kCVReturnSuccess;
}
int CVDisplayLinkStop(cvlink_t *l) { if (!l) return kCVReturnInvalidArgument; l->running = 0; wait_callback(l); return kCVReturnSuccess; }
u8 CVDisplayLinkIsRunning(cvlink_t *l) { return l && l->running; }
u32 CVDisplayLinkGetCurrentCGDisplay(cvlink_t *l) { return l ? l->display : 0; }
int CVDisplayLinkSetCurrentCGDisplay(cvlink_t *l, u32 d) {
    if (!l) return kCVReturnInvalidArgument;
    l->display = d;
    l->period_us = display_period_us(d);   /* outro monitor, outra taxa */
    return kCVReturnSuccess;
}
cvtime_t CVDisplayLinkGetNominalOutputVideoRefreshPeriod(cvlink_t *l) { cvtime_t t = {l ? l->period_us : PERIOD_US, 1000000, 0}; return t; }
cvtime_t CVDisplayLinkGetOutputVideoLatency(cvlink_t *l) { (void)l; cvtime_t t = {0, 1000000, 0}; return t; }
double CVDisplayLinkGetActualOutputVideoRefreshPeriod(cvlink_t *l) { return (l ? l->period_us : PERIOD_US) / 1e6; }
int CVDisplayLinkGetCurrentTime(cvlink_t *l, cvts_t *out) {
    if (!l || !out) return kCVReturnInvalidArgument;
    stamp(out, l->frame, mach_absolute_time(), l->period_us);
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
    id attach;                  /* anexos que propagam (kCVAttachmentMode_ShouldPropagate) */
    id attach_np;               /* anexos que não propagam */
    void (*release_cb)(void *ctx, const void *base);
    void *release_ctx;
    /* planos em memória do chamador (CVPixelBufferCreateWithPlanarBytes) */
    u32 np;
    void *pbase[4];
    u64 pw[4], ph[4], pbpr[4];
    void (*prel)(void *ctx, const void *data, u64 size, u64 np, const void **planes);
    void *pdata;
    u64 psize;
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
extern const void *kIOSurfacePlaneInfo, *kIOSurfacePlaneWidth, *kIOSurfacePlaneHeight;
extern const void *kIOSurfacePlaneBytesPerElement, *kIOSurfacePlaneBytesPerRow, *kIOSurfacePlaneOffset;
extern const void *kIOSurfacePlaneSize, *kIOSurfacePlaneElementWidth, *kIOSurfacePlaneElementHeight;

u64 CVPixelBufferGetTypeID(void) { return 0x2C0; }
static u64 pb_typeid(id self, SEL c) { (void)self; (void)c; return CVPixelBufferGetTypeID(); }
static void pb_dealloc(id self, SEL c) {
    cvpb_t *p = (cvpb_t *)self;
    if (p->surf) objc_release(p->surf);
    if (p->attach) objc_release(p->attach);
    if (p->attach_np) objc_release(p->attach_np);
    if (p->prel) { const void *pl[4] = {p->pbase[0], p->pbase[1], p->pbase[2], p->pbase[3]}; p->prel(p->release_ctx, p->pdata, p->psize, p->np, pl); }
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
static id plane_dict(u32 w, u32 h, u32 bpe, u32 ew, u32 eh, u64 bpr, u64 off) {
    id d = MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary");
    MSG(void, d, "setObject:forKey:", cx_num_i(w), (id)kIOSurfacePlaneWidth);
    MSG(void, d, "setObject:forKey:", cx_num_i(h), (id)kIOSurfacePlaneHeight);
    MSG(void, d, "setObject:forKey:", cx_num_i(bpe), (id)kIOSurfacePlaneBytesPerElement);
    MSG(void, d, "setObject:forKey:", cx_num_i(ew), (id)kIOSurfacePlaneElementWidth);
    MSG(void, d, "setObject:forKey:", cx_num_i(eh), (id)kIOSurfacePlaneElementHeight);
    MSG(void, d, "setObject:forKey:", cx_num_i((i64)bpr), (id)kIOSurfacePlaneBytesPerRow);
    MSG(void, d, "setObject:forKey:", cx_num_i((i64)off), (id)kIOSurfacePlaneOffset);
    MSG(void, d, "setObject:forKey:", cx_num_i((i64)(bpr * h)), (id)kIOSurfacePlaneSize);
    return d;
}
static id surf_props(u32 w, u32 h, u32 fmt) {
    id d = MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary");
    MSG(void, d, "setObject:forKey:", cx_num_i(w), (id)kIOSurfaceWidth);
    MSG(void, d, "setObject:forKey:", cx_num_i(h), (id)kIOSurfaceHeight);
    MSG(void, d, "setObject:forKey:", cx_num_i(fmt), (id)kIOSurfacePixelFormat);
    if ((fmt == 0x34323076 || fmt == 0x34323066) && (w & 1) == 0 && (h & 1) == 0) {
        u64 ybpr = (w + 63u) & ~63u;
        u64 uvbpr = (w + 63u) & ~63u;
        id y = plane_dict(w, h, 1, 1, 1, ybpr, 0);
        id uv = plane_dict(w / 2, h / 2, 2, 2, 1, uvbpr, ybpr * h);
        id arr = MSG0(id, (id)cx_class_named("NSMutableArray"), "array");
        if (arr && y && uv) {
            MSG(void, arr, "addObject:", y);
            MSG(void, arr, "addObject:", uv);
            MSG(void, d, "setObject:forKey:", arr, (id)kIOSurfacePlaneInfo);
        }
    }
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
/* planos em memória do chamador: o buffer aponta para eles (sem cópia) e o callback solta no fim */
int CVPixelBufferCreateWithPlanarBytes(void *alloc, u64 w, u64 h, u32 fmt, void *d, u64 ds, u64 np, void **pb, u64 *pw, u64 *ph, u64 *pr,
                                       void *rel, void *ctx, id attrs, cvpb_t **out) {
    (void)alloc; (void)attrs;
    if (!out) return kCVReturnInvalidArgument;
    *out = 0;
    if (!w || !h || np < 1 || np > 4 || !pb || !pw || !ph || !pr) return kCVReturnInvalidArgument;
    for (u64 i = 0; i < np; i++) if (!pb[i]) return kCVReturnInvalidArgument;
    cvpb_t *p = (cvpb_t *)cx_alloc_instance(&CVPixelBuffer_c);
    p->w = (u32)w; p->h = (u32)h; p->fmt = fmt;
    p->np = (u32)np;
    for (u64 i = 0; i < np; i++) { p->pbase[i] = pb[i]; p->pw[i] = pw[i]; p->ph[i] = ph[i]; p->pbpr[i] = pr[i]; }
    p->bytes = pb[0];
    p->bpr = pr[0];
    p->prel = (void (*)(void *, const void *, u64, u64, const void **))rel;
    p->release_ctx = ctx;
    p->pdata = d;
    p->psize = ds;
    *out = p;
    return kCVReturnSuccess;
}
id CVPixelBufferGetIOSurface(cvpb_t *p) { return p ? p->surf : 0; }
int CVPixelBufferLockBaseAddress(cvpb_t *p, u64 f) { if (!p) return kCVReturnInvalidArgument; if (p->surf) IOSurfaceLock(p->surf, (u32)f, 0); return 0; }
int CVPixelBufferUnlockBaseAddress(cvpb_t *p, u64 f) { if (!p) return kCVReturnInvalidArgument; if (p->surf) IOSurfaceUnlock(p->surf, (u32)f, 0); return 0; }
void *CVPixelBufferGetBaseAddress(cvpb_t *p) { return !p ? 0 : p->surf ? IOSurfaceGetBaseAddress(p->surf) : p->bytes; }
u64 CVPixelBufferGetBytesPerRow(cvpb_t *p) { return p ? p->bpr : 0; }
u64 CVPixelBufferGetWidth(cvpb_t *p) { return p ? p->w : 0; }
u64 CVPixelBufferGetHeight(cvpb_t *p) { return p ? p->h : 0; }
u32 CVPixelBufferGetPixelFormatType(cvpb_t *p) { return p ? p->fmt : 0; }
u64 CVPixelBufferGetDataSize(cvpb_t *p) { return !p ? 0 : p->surf ? IOSurfaceGetAllocSize(p->surf) : p->np ? p->psize : p->bpr * p->h; }
u64 CVPixelBufferGetPlaneCount(cvpb_t *p) { return !p ? 0 : p->surf ? IOSurfaceGetPlaneCount(p->surf) : p->np; }
u8 CVPixelBufferIsPlanar(cvpb_t *p) { return CVPixelBufferGetPlaneCount(p) > 0; }
void *CVPixelBufferGetBaseAddressOfPlane(cvpb_t *p, u64 i) { return !p ? 0 : p->surf ? IOSurfaceGetBaseAddressOfPlane(p->surf, i) : i < p->np ? p->pbase[i] : 0; }
u64 CVPixelBufferGetBytesPerRowOfPlane(cvpb_t *p, u64 i) { return !p ? 0 : p->surf ? IOSurfaceGetBytesPerRowOfPlane(p->surf, i) : i < p->np ? p->pbpr[i] : 0; }
u64 CVPixelBufferGetWidthOfPlane(cvpb_t *p, u64 i) { return !p ? 0 : p->surf ? IOSurfaceGetWidthOfPlane(p->surf, i) : i < p->np ? p->pw[i] : 0; }
u64 CVPixelBufferGetHeightOfPlane(cvpb_t *p, u64 i) { return !p ? 0 : p->surf ? IOSurfaceGetHeightOfPlane(p->surf, i) : i < p->np ? p->ph[i] : 0; }
cvpb_t *CVPixelBufferRetain(cvpb_t *p) { if (p) objc_retain((id)p); return p; }
void CVPixelBufferRelease(cvpb_t *p) { if (p) objc_release((id)p); }
typedef struct { double x, y, w, h; } cvrect_t;
cvrect_t CVImageBufferGetCleanRect(cvpb_t *p) { cvrect_t r = {0, 0, p ? p->w : 0, p ? p->h : 0}; return r; }

/* ---- anexos (CVBuffer*) */
/* modo 1 (kCVAttachmentMode_ShouldPropagate) e 0 (ShouldNotPropagate) ficam em dicionários separados */
void CVBufferSetAttachment(cvpb_t *p, id key, id val, u32 mode) {
    if (!p || !key) return;
    id *mine = mode ? &p->attach : &p->attach_np, other = mode ? p->attach_np : p->attach;
    if (other) MSG(void, other, "removeObjectForKey:", key);
    if (!*mine) *mine = objc_retain(MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary"));
    if (val) MSG(void, *mine, "setObject:forKey:", val, key);
    else MSG(void, *mine, "removeObjectForKey:", key);
}
id CVBufferGetAttachment(cvpb_t *p, id key, u32 *mode) {
    if (!p || !key) return 0;
    id v = p->attach ? MSG(id, p->attach, "objectForKey:", key) : 0;
    if (v) { if (mode) *mode = 1; return v; }
    v = p->attach_np ? MSG(id, p->attach_np, "objectForKey:", key) : 0;
    if (v && mode) *mode = 0;
    return v;
}
/* o dicionário do modo pedido (sem retenção: regra do Get) */
id CVBufferGetAttachments(cvpb_t *p, u32 mode) {
    id d = !p ? 0 : mode ? p->attach : p->attach_np;
    return d && MSG0(u64, d, "count") ? d : 0;
}
id CVBufferCopyAttachment(cvpb_t *p, id key, u32 *mode) { id v = CVBufferGetAttachment(p, key, mode); return v ? objc_retain(v) : 0; }
u8 CVBufferHasAttachment(cvpb_t *p, id key) { return CVBufferGetAttachment(p, key, 0) != 0; }
void CVBufferRemoveAttachment(cvpb_t *p, id key) {
    if (!p || !key) return;
    if (p->attach) MSG(void, p->attach, "removeObjectForKey:", key);
    if (p->attach_np) MSG(void, p->attach_np, "removeObjectForKey:", key);
}
void CVBufferRemoveAllAttachments(cvpb_t *p) { if (p && p->attach) MSG0(void, p->attach, "removeAllObjects"); if (p && p->attach_np) MSG0(void, p->attach_np, "removeAllObjects"); }
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

/* ---- anexos em lote, atributos do pool e constantes usadas pelo WhatsApp */
CFCONST(kCVImageBufferAlphaChannelModeKey, "AlphaChannelMode") CFCONST(kCVImageBufferAlphaChannelMode_StraightAlpha, "StraightAlpha")
CFCONST(kCVImageBufferAlphaChannelMode_PremultipliedAlpha, "PremultipliedAlpha")
CFCONST(kCVBufferTimeScaleKey, "TimeScale") CFCONST(kCVBufferTimeValueKey, "TimeValue") CFCONST(kCVBufferMovieTimeKey, "QTMovieTime")
CFCONST(kCVBufferPropagatedAttachmentsKey, "PropagatedAttachments") CFCONST(kCVBufferNonPropagatedAttachmentsKey, "NonPropagatedAttachments")
CFCONST(kCVPixelBufferCGBitmapContextCompatibilityKey, "CGBitmapContextCompatibility")

CFCONST(kCVImageBufferChromaLocationBottomFieldKey, "ChromaLocationBottomField")
CFCONST(kCVPixelBufferIOSurfaceCoreAnimationCompatibilityKey, "IOSurfaceCoreAnimationCompatibility")
CFCONST(kCVPixelBufferPoolMaximumBufferAgeKey, "MaximumBufferAge") CFCONST(kCVImageBufferICCProfileKey, "ICCProfile")
id CVBufferCopyAttachments(cvpb_t *p, u32 mode) {
    id d = CVBufferGetAttachments(p, mode);
    return d ? MSG0(id, d, "copy") : 0;
}
void CVBufferSetAttachments(cvpb_t *p, id dict, u32 mode) {
    if (!p || !dict) return;
    id keys = MSG0(id, dict, "allKeys");
    u64 n = MSG0(u64, keys, "count");
    for (u64 i = 0; i < n; i++) {
        id k = MSG(id, keys, "objectAtIndex:", i);
        CVBufferSetAttachment(p, k, MSG(id, dict, "objectForKey:", k), mode);
    }
}
void *CGColorSpaceCreateDeviceRGB(void);
void *CVImageBufferCreateColorSpaceFromAttachments(id attrs) { (void)attrs; return CGColorSpaceCreateDeviceRGB(); }
id CVPixelBufferPoolGetPixelBufferAttributes(cvpool_t *pool) {
    if (!pool) return 0;
    id d = MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary");
    id w = cx_num_i(pool->w), h = cx_num_i(pool->h), f = cx_num_i(pool->fmt);
    MSG(void, d, "setObject:forKey:", w, (id)kCVPixelBufferWidthKey);
    MSG(void, d, "setObject:forKey:", h, (id)kCVPixelBufferHeightKey);
    MSG(void, d, "setObject:forKey:", f, (id)kCVPixelBufferPixelFormatTypeKey);
    objc_release(w); objc_release(h); objc_release(f);
    return d;
}
id CVPixelBufferPoolGetAttributes(cvpool_t *pool) { (void)pool; return MSG0(id, (id)cx_class_named("NSDictionary"), "dictionary"); }
cvpool_t *CVPixelBufferPoolRetain(cvpool_t *p) { if (p) objc_retain((id)p); return p; }
void CVPixelBufferPoolRelease(cvpool_t *p) { if (p) objc_release((id)p); }

u64 CVPixelBufferPoolGetTypeID(void) { return 0x2C1; }
int CVPixelBufferFillExtendedPixels(cvpb_t *p, void *ext) {
    (void)ext;
    return p ? kCVReturnSuccess : kCVReturnInvalidArgument;
}
int CVPixelBufferGetExtendedPixels(cvpb_t *p, u64 *left, u64 *right, u64 *top, u64 *bottom) {
    if (!p) return kCVReturnInvalidArgument;
    if (left) *left = 0;
    if (right) *right = 0;
    if (top) *top = 0;
    if (bottom) *bottom = 0;
    return kCVReturnSuccess;
}
CFCONST(kCVImageBufferChromaLocation_Left, "Left")
CFCONST(kCVImageBufferChromaLocation_Center, "Center")
CFCONST(kCVImageBufferChromaLocation_TopLeft, "TopLeft")
CFCONST(kCVImageBufferChromaLocation_Top, "Top")
CFCONST(kCVImageBufferChromaLocation_BottomLeft, "BottomLeft")
CFCONST(kCVImageBufferChromaLocation_Bottom, "Bottom")
CFCONST(kCVImageBufferPixelAspectRatioKey, "CVImageBufferPixelAspectRatio")
CFCONST(kCVImageBufferPixelAspectRatioHorizontalSpacingKey, "CVImageBufferPixelAspectRatioHorizontalSpacing")
CFCONST(kCVImageBufferPixelAspectRatioVerticalSpacingKey, "CVImageBufferPixelAspectRatioVerticalSpacing")

/* Códigos iguais aos de ISO/IEC 23001-8, que o CoreVideo usa. Código desconhecido devolve NULL. */
const void *CVColorPrimariesGetStringForIntegerCodePoint(int c) {
    switch (c) {
    case 1: return kCVImageBufferColorPrimaries_ITU_R_709_2;
    case 5: return kCVImageBufferColorPrimaries_EBU_3213;
    case 6: return kCVImageBufferColorPrimaries_SMPTE_C;
    case 9: return kCVImageBufferColorPrimaries_ITU_R_2020;
    case 11: return kCVImageBufferColorPrimaries_DCI_P3;
    case 12: return kCVImageBufferColorPrimaries_P3_D65;
    default: return 0;
    }
}
const void *CVTransferFunctionGetStringForIntegerCodePoint(int c) {
    switch (c) {
    case 1: return kCVImageBufferTransferFunction_ITU_R_709_2;
    case 7: return kCVImageBufferTransferFunction_SMPTE_240M_1995;
    case 8: return kCVImageBufferTransferFunction_Linear;
    case 13: return kCVImageBufferTransferFunction_sRGB;
    case 14: return kCVImageBufferTransferFunction_ITU_R_2020;
    case 16: return kCVImageBufferTransferFunction_SMPTE_ST_2084_PQ;
    case 17: return kCVImageBufferTransferFunction_SMPTE_ST_428_1;
    case 18: return kCVImageBufferTransferFunction_ITU_R_2100_HLG;
    default: return 0;
    }
}
const void *CVYCbCrMatrixGetStringForIntegerCodePoint(int c) {
    switch (c) {
    case 1: return kCVImageBufferYCbCrMatrix_ITU_R_709_2;
    case 6: return kCVImageBufferYCbCrMatrix_ITU_R_601_4;
    case 7: return kCVImageBufferYCbCrMatrix_SMPTE_240M_1995;
    case 9: return kCVImageBufferYCbCrMatrix_ITU_R_2020;
    default: return 0;
    }
}

u8 CVIsCompressedPixelFormatAvailable(u32 fmt) { (void)fmt; return 0; }
id CVPixelFormatDescriptionCreateWithPixelFormatType(void *alloc, u32 fmt) { (void)alloc; (void)fmt; return 0; }

/* ================================================================ sem OpenGL */
/* Metal: metal.c (por software). A CGL (CGLChoosePixelFormat continua recusando, para o Chromium cair no
 * SwiftShader) e o NSOpenGL estão em appkit_nsopengl.c */

void cx_metal_init(void);

/* ================================================================ OpenGL: tamanhos, constantes e CVOpenGLTextureCache */
CFCONST(kCVPixelBufferIOSurfaceOpenGLTextureCompatibilityKey, "IOSurfaceOpenGLTextureCompatibility")
CFCONST(kCVPixelBufferIOSurfaceOpenGLFBOCompatibilityKey, "IOSurfaceOpenGLFBOCompatibility")
CFCONST(kCVPixelBufferOpenGLTextureCacheCompatibilityKey, "OpenGLTextureCacheCompatibility")
CFCONST(kCVImageBufferColorPrimaries_P22, "P22")
CFCONST(kCVImageBufferChromaLocation_DV420, "DV 4:2:0")
CFCONST(kCVOpenGLTextureCacheChromaSamplingModeKey, "ChromaSamplingMode")
typedef struct { double w, h; } cvsize_t;
cvsize_t CVImageBufferGetEncodedSize(cvpb_t *p) { cvsize_t z = {p ? p->w : 0, p ? p->h : 0}; return z; }
cvsize_t CVImageBufferGetDisplaySize(cvpb_t *p) { return CVImageBufferGetEncodedSize(p); }
u8 CVImageBufferIsFlipped(cvpb_t *p) { (void)p; return 1; }   /* linha 0 da memória é a de cima */
/* o display link segue a tela do contexto GL: o mOS tem uma tela de saída por vez */
int CVDisplayLinkSetCurrentCGDisplayFromOpenGLContext(cvlink_t *l, void *cgl_ctx, void *cgl_pix) {
    (void)cgl_ctx; (void)cgl_pix;
    if (!l) return kCVReturnInvalidArgument;
    l->display = 1;
    return kCVReturnSuccess;
}

/* GL usado pelo cache (OpenGL por software, gl_*.c) */
void *CGLGetCurrentContext(void);
int CGLSetCurrentContext(void *ctx);
void glGenTextures(int n, u32 *t);
void glDeleteTextures(int n, const u32 *t);
void glBindTexture(u32 target, u32 t);
void glTexImage2D(u32 target, int lvl, int ifmt, int w, int h, int border, u32 fmt, u32 type, const void *px);
void glTexParameteri(u32 target, u32 p, int v);
void glPixelStorei(u32 p, int v);
void glGetIntegerv(u32 p, int *v);
#define CV_GL_TEXTURE_RECTANGLE 0x84F5
#define CV_GL_TEXTURE_BINDING_RECTANGLE 0x84F6
#define CV_GL_RGBA8 0x8058
#define CV_GL_BGRA 0x80E1
#define CV_GL_UNSIGNED_INT_8_8_8_8_REV 0x8367
#define CV_GL_UNPACK_ROW_LENGTH 0x0CF2
#define CV_GL_UNPACK_ALIGNMENT 0x0CF5
#define CV_GL_TEXTURE_MIN_FILTER 0x2801
#define CV_GL_TEXTURE_MAG_FILTER 0x2800
#define CV_GL_LINEAR 0x2601
typedef struct { cx_class_t *isa; void *cgl; void *pix; } cvglcache_t;
typedef struct { cx_class_t *isa; cvglcache_t *cache; u32 name, target; u32 w, h; cvpb_t *image; } cvgltex_t;
cx_class_t CVOpenGLTextureCache_c __asm__("OBJC_CLASS_$_CVOpenGLTextureCache");
cx_class_t CVOpenGLTextureCache_m __asm__("OBJC_METACLASS_$_CVOpenGLTextureCache");
cx_class_t CVOpenGLTexture_c __asm__("OBJC_CLASS_$_CVOpenGLTexture");
cx_class_t CVOpenGLTexture_m __asm__("OBJC_METACLASS_$_CVOpenGLTexture");
void *CGLRetainContext(void *c);
void CGLReleaseContext(void *c);
int CVOpenGLTextureCacheCreate(void *alloc, id cattrs, void *cgl_ctx, void *cgl_pix, id tattrs, cvglcache_t **out) {
    (void)alloc; (void)cattrs; (void)tattrs;
    if (!out || !cgl_ctx) return kCVReturnInvalidArgument;
    cvglcache_t *c = (cvglcache_t *)cx_alloc_instance(&CVOpenGLTextureCache_c);
    c->cgl = CGLRetainContext(cgl_ctx);
    c->pix = cgl_pix;
    *out = c;
    return kCVReturnSuccess;
}
static void cache_dealloc(id self, SEL c) {
    cvglcache_t *k = (cvglcache_t *)self;
    if (k->cgl) CGLReleaseContext(k->cgl);
    IMP sup = class_getMethodImplementation(cx_class_named("NSObject"), sel_registerName("dealloc"));
    if (sup) ((void (*)(id, SEL))sup)(self, c);
}
static u8 clamp8(int v) { return (u8)(v < 0 ? 0 : v > 255 ? 255 : v); }
/* YCbCr -> BGRA (BT.601 para SD, BT.709 para HD; faixa de vídeo ou cheia) */
static void yuv_px(int y, int cb, int cr, int full, int hd, u8 *o) {
    double Y = full ? y : (y - 16) * 255.0 / 219.0, U = (cb - 128) * (full ? 1.0 : 255.0 / 224.0), V = (cr - 128) * (full ? 1.0 : 255.0 / 224.0);
    double r, g, b;
    if (hd) { r = Y + 1.5748 * V; g = Y - 0.1873 * U - 0.4681 * V; b = Y + 1.8556 * U; }
    else { r = Y + 1.402 * V; g = Y - 0.344136 * U - 0.714136 * V; b = Y + 1.772 * U; }
    o[0] = clamp8((int)(b + 0.5)); o[1] = clamp8((int)(g + 0.5)); o[2] = clamp8((int)(r + 0.5)); o[3] = 255;
}
/* os pixels do buffer em BGRA8 (devolve memória nova ou 0) */
static u8 *to_bgra(cvpb_t *p) {
    u32 w = p->w, h = p->h, f = p->fmt;
    u8 *o = malloc((size_t)w * h * 4 + 1);
    if (!o) return 0;
    int hd = w > 1024 || h > 576;
    switch (f) {
    case 0x42475241: /* 'BGRA' */ { const u8 *b = CVPixelBufferGetBaseAddress(p); u64 bpr = CVPixelBufferGetBytesPerRow(p); for (u32 y = 0; y < h; y++) memcpy(o + (u64)y * w * 4, b + y * bpr, (size_t)w * 4); return o; }
    case 0x00000020: /* k32ARGBPixelFormat */ case 0x52474241: /* 'RGBA' */ {
        const u8 *b = CVPixelBufferGetBaseAddress(p);
        u64 bpr = CVPixelBufferGetBytesPerRow(p);
        for (u32 y = 0; y < h; y++)
            for (u32 x = 0; x < w; x++) {
                const u8 *q = b + y * bpr + x * 4;
                u8 *d = o + ((u64)y * w + x) * 4;
                if (f == 0x20) { d[0] = q[3]; d[1] = q[2]; d[2] = q[1]; d[3] = q[0]; }
                else { d[0] = q[2]; d[1] = q[1]; d[2] = q[0]; d[3] = q[3]; }
            }
        return o;
    }
    case 0x34323076: case 0x34323066: { /* '420v' / '420f': Y + CbCr intercalado */
        const u8 *Y = CVPixelBufferGetBaseAddressOfPlane(p, 0), *C = CVPixelBufferGetBaseAddressOfPlane(p, 1);
        u64 yb = CVPixelBufferGetBytesPerRowOfPlane(p, 0), cb = CVPixelBufferGetBytesPerRowOfPlane(p, 1);
        if (!Y || !C) break;
        for (u32 y = 0; y < h; y++)
            for (u32 x = 0; x < w; x++) {
                const u8 *c = C + (y / 2) * cb + (x / 2) * 2;
                yuv_px(Y[y * yb + x], c[0], c[1], f == 0x34323066, hd, o + ((u64)y * w + x) * 4);
            }
        return o;
    }
    case 0x32767579: case 0x79757673: { /* '2vuy' (Cb Y0 Cr Y1) / 'yuvs' (Y0 Cb Y1 Cr) */
        const u8 *b = CVPixelBufferGetBaseAddress(p);
        u64 bpr = CVPixelBufferGetBytesPerRow(p);
        for (u32 y = 0; y < h; y++)
            for (u32 x = 0; x < w; x++) {
                const u8 *q = b + y * bpr + (x / 2) * 4;
                int yy, u, v;
                if (f == 0x32767579) { u = q[0]; yy = (x & 1) ? q[3] : q[1]; v = q[2]; }
                else { yy = (x & 1) ? q[2] : q[0]; u = q[1]; v = q[3]; }
                yuv_px(yy, u, v, 0, hd, o + ((u64)y * w + x) * 4);
            }
        return o;
    }
    }
    free(o);
    return 0;
}
int CVOpenGLTextureCacheCreateTextureFromImage(void *alloc, cvglcache_t *cache, cvpb_t *img, id attrs, cvgltex_t **out) {
    (void)alloc; (void)attrs;
    if (!out || !cache || !img) return kCVReturnInvalidArgument;
    *out = 0;
    CVPixelBufferLockBaseAddress(img, 1);
    u8 *px = to_bgra(img);
    CVPixelBufferUnlockBaseAddress(img, 1);
    if (!px) return kCVReturnInvalidPixelFormat;
    void *prev = CGLGetCurrentContext();
    CGLSetCurrentContext(cache->cgl);
    int old_tex = 0, old_row = 0, old_al = 4;
    glGetIntegerv(CV_GL_TEXTURE_BINDING_RECTANGLE, &old_tex);
    glGetIntegerv(CV_GL_UNPACK_ROW_LENGTH, &old_row);
    glGetIntegerv(CV_GL_UNPACK_ALIGNMENT, &old_al);
    u32 name = 0;
    glGenTextures(1, &name);
    glBindTexture(CV_GL_TEXTURE_RECTANGLE, name);
    glPixelStorei(CV_GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(CV_GL_UNPACK_ALIGNMENT, 4);
    glTexImage2D(CV_GL_TEXTURE_RECTANGLE, 0, CV_GL_RGBA8, (int)img->w, (int)img->h, 0, CV_GL_BGRA, CV_GL_UNSIGNED_INT_8_8_8_8_REV, px);
    glTexParameteri(CV_GL_TEXTURE_RECTANGLE, CV_GL_TEXTURE_MIN_FILTER, CV_GL_LINEAR);
    glTexParameteri(CV_GL_TEXTURE_RECTANGLE, CV_GL_TEXTURE_MAG_FILTER, CV_GL_LINEAR);
    glBindTexture(CV_GL_TEXTURE_RECTANGLE, (u32)old_tex);
    glPixelStorei(CV_GL_UNPACK_ROW_LENGTH, old_row);
    glPixelStorei(CV_GL_UNPACK_ALIGNMENT, old_al);
    CGLSetCurrentContext(prev);
    free(px);
    cvgltex_t *t = (cvgltex_t *)cx_alloc_instance(&CVOpenGLTexture_c);
    t->cache = (cvglcache_t *)objc_retain((id)cache);
    t->name = name;
    t->target = CV_GL_TEXTURE_RECTANGLE;
    t->w = img->w;
    t->h = img->h;
    t->image = CVPixelBufferRetain(img);
    *out = t;
    return kCVReturnSuccess;
}
static void gltex_dealloc(id self, SEL c) {
    cvgltex_t *t = (cvgltex_t *)self;
    if (t->cache && t->name) {
        void *prev = CGLGetCurrentContext();
        CGLSetCurrentContext(t->cache->cgl);
        glDeleteTextures(1, &t->name);
        CGLSetCurrentContext(prev);
    }
    if (t->image) CVPixelBufferRelease(t->image);
    if (t->cache) objc_release((id)t->cache);
    IMP sup = class_getMethodImplementation(cx_class_named("NSObject"), sel_registerName("dealloc"));
    if (sup) ((void (*)(id, SEL))sup)(self, c);
}
void CVOpenGLTextureCacheFlush(cvglcache_t *c, u64 opts) { (void)c; (void)opts; }   /* cada textura solta o nome GL no dealloc */
cvglcache_t *CVOpenGLTextureCacheRetain(cvglcache_t *c) { if (c) objc_retain((id)c); return c; }
void CVOpenGLTextureCacheRelease(cvglcache_t *c) { if (c) objc_release((id)c); }
u32 CVOpenGLTextureGetName(cvgltex_t *t) { return t ? t->name : 0; }
u32 CVOpenGLTextureGetTarget(cvgltex_t *t) { return t ? t->target : 0; }
u8 CVOpenGLTextureIsFlipped(cvgltex_t *t) { (void)t; return 1; }
/* coordenadas (retângulo: em texels) dos quatro cantos; com a imagem de cabeça para baixo na textura */
void CVOpenGLTextureGetCleanTexCoords(cvgltex_t *t, float *ll, float *lr, float *ur, float *ul) {
    float w = t ? (float)t->w : 0, h = t ? (float)t->h : 0;
    if (ll) { ll[0] = 0; ll[1] = h; }
    if (lr) { lr[0] = w; lr[1] = h; }
    if (ur) { ur[0] = w; ur[1] = 0; }
    if (ul) { ul[0] = 0; ul[1] = 0; }
}
cvgltex_t *CVOpenGLTextureRetain(cvgltex_t *t) { if (t) objc_retain((id)t); return t; }
void CVOpenGLTextureRelease(cvgltex_t *t) { if (t) objc_release((id)t); }
void cx_corevideo_gl_init(void) {
    cx_class_t *ob = cx_class_named("NSObject");
    cx_define_class(&CVOpenGLTextureCache_c, &CVOpenGLTextureCache_m, "CVOpenGLTextureCache", ob, sizeof(cvglcache_t) - 8);
    cx_add_method(&CVOpenGLTextureCache_c, "dealloc", (void *)cache_dealloc);
    cx_define_class(&CVOpenGLTexture_c, &CVOpenGLTexture_m, "CVOpenGLTexture", ob, sizeof(cvgltex_t) - 8);
    cx_add_method(&CVOpenGLTexture_c, "dealloc", (void *)gltex_dealloc);
}
void cx_corevideo_init(void) {
    cx_metal_init();
    cx_class_t *ob = cx_class_named("NSObject");
    cx_define_class(&CVDisplayLink_c, &CVDisplayLink_m, "CVDisplayLink", ob, sizeof(cvlink_t) - 8);
    cx_add_method(&CVDisplayLink_c, "dealloc", (void *)link_dealloc);
    cx_define_class(&CVPixelBuffer_c, &CVPixelBuffer_m, "CVPixelBuffer", ob, sizeof(cvpb_t) - 8);
    cx_add_method(&CVPixelBuffer_c, "dealloc", (void *)pb_dealloc);
    cx_add_method(&CVPixelBuffer_c, "_cfTypeID", (void *)pb_typeid);
    cx_define_class(&CVPixelBufferPool_c, &CVPixelBufferPool_m, "CVPixelBufferPool", ob, sizeof(cvpool_t) - 8);
    cx_corevideo_gl_init();
}
