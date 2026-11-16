/*
 * mOS — vídeo do AVPlayer: AVPlayerItemVideoOutput e AVPlayerLayer.
 *
 * Cada saída abre o próprio leitor de vídeo (avvideo.c: MP4/MOV, H.264 pelo OpenH264 corrigido) para o asset do item e
 * pede o quadro do tempo atual do item (o relógio do AVPlayer de avfaudio.c: o áudio quando há, senão o host).
 *  - AVPlayerItemVideoOutput: hasNewPixelBufferForItemTime:, copyPixelBufferForItemTime:itemTimeForDisplay: em CVPixelBuffer
 *    '420v' (padrão), '420f' ou 'BGRA' (kCVPixelBufferPixelFormatTypeKey dos atributos), com os anexos de cor do VUI;
 *    itemTimeForHostTime:/itemTimeForMachAbsoluteTime: devolvem o tempo atual do item; o delegate recebe
 *    outputMediaDataWillChange: depois de requestNotificationOfMediaDataChangeWithAdvanceInterval:.
 *  - AVPlayerLayer: subclasse de CALayer; um timer de 1/60 s põe em contents o CGImage do quadro atual (girado pelo
 *    preferredTransform), videoGravity vira contentsGravity, isReadyForDisplay depois do primeiro quadro, videoRect.
 *  - AVSampleBufferDisplayLayer: amostras H.264 comprimidas passam por um VTDecompressionSession (saída BGRA); amostras com
 *    imagem entram direto. Os quadros esperam numa fila por pts e um timer de 1/60 s mostra o quadro devido pelo
 *    controlTimebase (sem timebase: o mais antigo, assim que sai). flush, flushAndRemoveImage, readyForMoreMediaData,
 *    requestMediaDataWhenReadyOnQueue:usingBlock:, DoNotDisplay e DisplayImmediately.
 */
#include "foundation.h"
#include "libc.h"
#include "avvideo.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void objc_setAssociatedObject(id o, const void *key, id value, int policy);
id objc_getAssociatedObject(id o, const void *key);
IMP class_getMethodImplementation(cx_class_t *c, SEL s);
id objc_msgSend_stret(id self, SEL op, ...);
double cx_avitem_seconds(id item);
const char *cx_avitem_video_path(id item);
id cx_avplayer_item(id player);
float cx_avplayer_rate(id player);
int CVPixelBufferCreate(void *a, u64 w, u64 h, u32 fmt, id attrs, void **out);
int CVPixelBufferLockBaseAddress(void *p, u64 f);
int CVPixelBufferUnlockBaseAddress(void *p, u64 f);
void *CVPixelBufferGetBaseAddress(void *p);
u64 CVPixelBufferGetBytesPerRow(void *p);
void *CVPixelBufferGetBaseAddressOfPlane(void *p, u64 i);
u64 CVPixelBufferGetBytesPerRowOfPlane(void *p, u64 i);
void CVBufferSetAttachment(void *p, id key, id val, u32 mode);
const void *CVYCbCrMatrixGetStringForIntegerCodePoint(int c);
const void *CVColorPrimariesGetStringForIntegerCodePoint(int c);
const void *CVTransferFunctionGetStringForIntegerCodePoint(int c);
extern const void *kCVPixelBufferPixelFormatTypeKey, *kCVImageBufferYCbCrMatrixKey, *kCVImageBufferColorPrimariesKey,
    *kCVImageBufferTransferFunctionKey;
void *CGDataProviderCreateWithData(void *info, const void *data, size_t size, void (*release)(void *, const void *, size_t));
void *CGImageCreate(size_t w, size_t h, size_t bpc, size_t bpp, size_t bpr, const void *space, u32 info, void *prov, const void *dec, _Bool interp, int intent);
void *CGColorSpaceCreateDeviceRGB(void);
void CGColorSpaceRelease(void *c);
void CGDataProviderRelease(void *p);
void CGImageRelease(void *i);
typedef void *dispatch_queue_t;
void *dispatch_get_main_queue(void);
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *));

typedef struct { long long value; int timescale; unsigned flags; long long epoch; } pv_cmtime_t;
typedef struct { double x, y, w, h; } pv_rect_t;
static pv_cmtime_t pv_time(double s) { pv_cmtime_t t = {(long long)(s * 600 + (s >= 0 ? 0.5 : -0.5)), 600, 1, 0}; return t; }
static double pv_sec(pv_cmtime_t t) { return t.timescale > 0 && (t.flags & 1) ? (double)t.value / t.timescale : 0; }
static pv_cmtime_t pv_invalid(void) { pv_cmtime_t t = {0, 0, 0, 0}; return t; }

/* leitor de vídeo de uma saída: reabre quando o item (o asset) muda */
typedef struct { avv_t *v; char path[1024]; } pv_reader_t;
static avv_t *reader_for(pv_reader_t *r, id item) {
    const char *p = cx_avitem_video_path(item);
    if (!p) { if (r->v) { avv_close(r->v); r->v = 0; } r->path[0] = 0; return 0; }
    if (r->v && !strcmp(r->path, p)) return r->v;
    if (r->v) avv_close(r->v);
    int e = 0;
    r->v = avv_open_path(p, &e);
    snprintf(r->path, sizeof r->path, "%s", p);
    return r->v;
}
static void attach_color(void *img, const avv_frame *f) {
    const void *m = CVYCbCrMatrixGetStringForIntegerCodePoint(avv_matrix(f));
    const void *p = CVColorPrimariesGetStringForIntegerCodePoint(f->primaries);
    const void *t = CVTransferFunctionGetStringForIntegerCodePoint(f->transfer);
    if (m) CVBufferSetAttachment(img, (id)kCVImageBufferYCbCrMatrixKey, (id)m, 1);
    if (p) CVBufferSetAttachment(img, (id)kCVImageBufferColorPrimariesKey, (id)p, 1);
    if (t) CVBufferSetAttachment(img, (id)kCVImageBufferTransferFunctionKey, (id)t, 1);
}

/* ================================================================ AVPlayerItemVideoOutput */
typedef struct {
    cx_class_t *isa;
    id item;                    /* fraco: o item guarda a saída em outputs */
    u32 fmt;
    pv_reader_t rd;
    double last_pts;
    int has_last, suppresses;
    id delegate;
    void *queue;
} pvo_t;
cx_class_t AVPlayerItemVideoOutput_c __asm__("OBJC_CLASS_$_AVPlayerItemVideoOutput"),
    AVPlayerItemVideoOutput_m __asm__("OBJC_METACLASS_$_AVPlayerItemVideoOutput");
cx_class_t AVPlayerItemOutput_c __asm__("OBJC_CLASS_$_AVPlayerItemOutput"), AVPlayerItemOutput_m __asm__("OBJC_METACLASS_$_AVPlayerItemOutput");
#define PVO(o) ((pvo_t *)(o))
static u32 fmt_from(id attrs) {
    id v = attrs ? MSG(id, attrs, "objectForKey:", (id)kCVPixelBufferPixelFormatTypeKey) : 0;
    if (v && MSG(BOOL, v, "respondsToSelector:", sel_registerName("objectAtIndex:"))) v = MSG0(u64, v, "count") ? MSG(id, v, "objectAtIndex:", (u64)0) : 0;
    u32 f = v ? (u32)MSG0(i64, v, "longLongValue") : 0x34323076u;
    return f == 0x42475241u || f == 0x34323066u ? f : 0x34323076u;   /* outros formatos: o nativo do decodificador */
}
static id pvo_init_attrs(id s, SEL c, id attrs) { (void)c; PVO(s)->fmt = fmt_from(attrs); return s; }
static id pvo_init_settings(id s, SEL c, id settings) { return pvo_init_attrs(s, c, settings); }
static id pvo_init(id s, SEL c) { return pvo_init_attrs(s, c, 0); }
static void pvo_dealloc(id s, SEL c) {
    if (PVO(s)->rd.v) avv_close(PVO(s)->rd.v);
    if (PVO(s)->delegate) objc_release(PVO(s)->delegate);
    IMP d = class_getMethodImplementation(cx_class_named("NSObject"), c);
    if (d) ((void (*)(id, SEL))d)(s, c);
}
static void pvo_attach(id s, SEL c, id item) { (void)c; PVO(s)->item = item; PVO(s)->has_last = 0; }
static pv_cmtime_t pvo_time_host(id s, SEL c, double host) { (void)c; (void)host; return PVO(s)->item ? pv_time(cx_avitem_seconds(PVO(s)->item)) : pv_invalid(); }
static pv_cmtime_t pvo_time_mach(id s, SEL c, i64 mach) { (void)mach; return pvo_time_host(s, c, 0); }
static pv_cmtime_t pvo_time_cv(id s, SEL c, const void *ts) { (void)ts; return pvo_time_host(s, c, 0); }
static BOOL pvo_has_new(id s, SEL c, pv_cmtime_t t) {
    (void)c;
    pvo_t *o = PVO(s);
    avv_t *v = o->item ? reader_for(&o->rd, o->item) : 0;
    avv_frame f;
    if (!v || avv_frame_at(v, pv_sec(t), &f)) return NO;
    return !o->has_last || f.pts != o->last_pts;
}
static void *pvo_copy(id s, SEL c, pv_cmtime_t t, pv_cmtime_t *disp) {
    (void)c;
    pvo_t *o = PVO(s);
    if (disp) *disp = pv_invalid();
    avv_t *v = o->item ? reader_for(&o->rd, o->item) : 0;
    avv_frame f;
    if (!v || avv_frame_at(v, pv_sec(t), &f)) return 0;
    void *img = 0;
    if (CVPixelBufferCreate(0, (u64)f.w, (u64)f.h, o->fmt, 0, &img) || !img) return 0;
    CVPixelBufferLockBaseAddress(img, 0);
    if (o->fmt == 0x42475241u) {
        uint8_t *px = 0;
        int ow, oh;
        if (!avv_to_bgra(&f, 0, 0, 0, &px, &ow, &oh)) {
            uint8_t *d = CVPixelBufferGetBaseAddress(img);
            u64 bpr = CVPixelBufferGetBytesPerRow(img);
            for (int r = 0; r < oh; r++) memcpy(d + (u64)r * bpr, px + (u64)r * ow * 4, (size_t)ow * 4);
            free(px);
        }
    } else {
        avv_to_nv12(&f, o->fmt == 0x34323066u, CVPixelBufferGetBaseAddressOfPlane(img, 0), (long)CVPixelBufferGetBytesPerRowOfPlane(img, 0),
                    CVPixelBufferGetBaseAddressOfPlane(img, 1), (long)CVPixelBufferGetBytesPerRowOfPlane(img, 1));
    }
    CVPixelBufferUnlockBaseAddress(img, 0);
    attach_color(img, &f);
    o->last_pts = f.pts;
    o->has_last = 1;
    if (disp) *disp = pv_time(f.pts);
    return img;   /* +1 (regra copy) */
}
static BOOL pvo_suppresses(id s, SEL c) { (void)c; return PVO(s)->suppresses ? YES : NO; }
static void pvo_set_suppresses(id s, SEL c, BOOL v) { (void)c; PVO(s)->suppresses = v; }
static id pvo_delegate(id s, SEL c) { (void)c; return PVO(s)->delegate; }
static void pvo_set_delegate(id s, SEL c, id d, void *q) {
    (void)c;
    if (d) objc_retain(d);
    if (PVO(s)->delegate) objc_release(PVO(s)->delegate);
    PVO(s)->delegate = d;
    PVO(s)->queue = q;
}
static void pvo_notify(void *a) {
    id s = a;
    if (PVO(s)->delegate && MSG(BOOL, PVO(s)->delegate, "respondsToSelector:", sel_registerName("outputMediaDataWillChange:")))
        MSG(void, PVO(s)->delegate, "outputMediaDataWillChange:", s);
    objc_release(s);
}
/* o próximo quadro já está disponível (decodificação síncrona): avisa o delegate na fila dele */
static void pvo_request(id s, SEL c, double interval) {
    (void)c; (void)interval;
    if (!PVO(s)->delegate) return;
    dispatch_async_f(PVO(s)->queue ? PVO(s)->queue : dispatch_get_main_queue(), objc_retain(s), pvo_notify);
}
static id pvo_attrs(id s, SEL c) {
    (void)c;
    id d = MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary");
    id n = MSG(id, (id)cx_class_named("NSNumber"), "numberWithUnsignedInt:", PVO(s)->fmt);
    MSG(void, d, "setObject:forKey:", n, (id)kCVPixelBufferPixelFormatTypeKey);
    return d;
}

/* ================================================================ AVPlayerLayer */
typedef struct {
    cx_class_t *isa;
    id player;
    id timer;
    id layer;                   /* fraco */
    pv_reader_t rd;
    double last_pts;
    int ready, w, h;
} pls_t;
cx_class_t MOSAVPlayerLayerState_c __asm__("OBJC_CLASS_$_MOSAVPlayerLayerState"), MOSAVPlayerLayerState_m __asm__("OBJC_METACLASS_$_MOSAVPlayerLayerState");
cx_class_t AVPlayerLayer_c __asm__("OBJC_CLASS_$_AVPlayerLayer"), AVPlayerLayer_m __asm__("OBJC_METACLASS_$_AVPlayerLayer");
static const char k_state = 0, k_gravity = 0;
static pls_t *state_of(id layer, int create) {
    pls_t *st = (pls_t *)objc_getAssociatedObject(layer, &k_state);
    if (!st && create) {
        st = (pls_t *)cx_alloc_instance(&MOSAVPlayerLayerState_c);
        st->layer = layer;
        st->last_pts = -1;
        objc_setAssociatedObject(layer, &k_state, (id)st, 1);
        objc_release((id)st);
    }
    return st;
}
static void pls_dealloc(id s, SEL c) {
    pls_t *st = (pls_t *)s;
    if (st->timer) { MSG0(void, st->timer, "invalidate"); objc_release(st->timer); }
    if (st->player) objc_release(st->player);
    if (st->rd.v) avv_close(st->rd.v);
    IMP d = class_getMethodImplementation(cx_class_named("NSObject"), c);
    if (d) ((void (*)(id, SEL))d)(s, c);
}
static void free_px(void *info, const void *data, size_t n) { (void)info; (void)n; free((void *)data); }
/* o quadro do tempo atual do item vira o contents do layer (só quando muda) */
static void pls_tick(id s, SEL c, id timer) {
    (void)c; (void)timer;
    pls_t *st = (pls_t *)s;
    id item = st->player ? cx_avplayer_item(st->player) : 0;
    avv_t *v = item ? reader_for(&st->rd, item) : 0;
    avv_frame f;
    if (!v || !st->layer || avv_frame_at(v, cx_avitem_seconds(item), &f) || f.pts == st->last_pts) return;
    uint8_t *px = 0;
    int ow, oh;
    if (avv_to_bgra(&f, avv_rotation(v), 0, 0, &px, &ow, &oh)) return;
    void *prov = CGDataProviderCreateWithData(0, px, (size_t)ow * oh * 4, free_px);
    void *cs = CGColorSpaceCreateDeviceRGB();
    void *img = CGImageCreate((size_t)ow, (size_t)oh, 8, 32, (size_t)ow * 4, cs, 6 | 0x2000, prov, 0, 1, 0);   /* BGRA */
    CGColorSpaceRelease(cs);
    CGDataProviderRelease(prov);
    if (!img) return;
    MSG(void, st->layer, "setContents:", (id)img);
    CGImageRelease(img);
    st->last_pts = f.pts;
    st->w = ow;
    st->h = oh;
    st->ready = 1;
}
static void pls_set_player(pls_t *st, id player) {
    if (player) objc_retain(player);
    if (st->player) objc_release(st->player);
    st->player = player;
    st->last_pts = -1;
    st->ready = 0;
    if (player && !st->timer)
        st->timer = objc_retain(MSG(id, (id)cx_class_named("NSTimer"), "scheduledTimerWithTimeInterval:target:selector:userInfo:repeats:",
                                    1.0 / 60, (id)st, sel_registerName("tick:"), (id)0, (BOOL)YES));
    else if (!player && st->timer) { MSG0(void, st->timer, "invalidate"); objc_release(st->timer); st->timer = 0; }
    if (player) pls_tick((id)st, 0, 0);
}
static id pl_with_player(id k, SEL c, id player) {
    (void)k; (void)c;
    id l = MSG0(id, MSG0(id, (id)&AVPlayerLayer_c, "alloc"), "init");
    if (!l) return 0;
    MSG(void, l, "setContentsGravity:", cx_str("resizeAspect"));
    pls_set_player(state_of(l, 1), player);
    return objc_autorelease(l);
}
static id pl_player(id s, SEL c) { (void)c; pls_t *st = state_of(s, 0); return st ? st->player : 0; }
static void pl_set_player(id s, SEL c, id p) { (void)c; pls_set_player(state_of(s, 1), p); }
static const char *grav_ca(const char *g) {
    if (g && !strcmp(g, "AVLayerVideoGravityResizeAspectFill")) return "resizeAspectFill";
    if (g && !strcmp(g, "AVLayerVideoGravityResize")) return "resize";
    return "resizeAspect";
}
static id pl_gravity(id s, SEL c) { (void)c; id g = objc_getAssociatedObject(s, &k_gravity); return g ? g : cx_str("AVLayerVideoGravityResizeAspect"); }
static void pl_set_gravity(id s, SEL c, id g) {
    (void)c;
    objc_setAssociatedObject(s, &k_gravity, g, 3);
    MSG(void, s, "setContentsGravity:", cx_str(grav_ca(g ? cx_cstr(g) : 0)));
}
static BOOL pl_ready(id s, SEL c) { (void)c; pls_t *st = state_of(s, 0); if (st && !st->ready) pls_tick((id)st, 0, 0); return st && st->ready ? YES : NO; }
/* videoRect: onde o quadro fica dentro de bounds segundo videoGravity */
typedef struct { double x, y, w, h; } cgrect_t;
static cgrect_t pl_video_rect(id s, SEL c) {
    (void)c;
    cgrect_t z = {0, 0, 0, 0};
    pls_t *st = state_of(s, 0);
    if (!st || !st->ready || st->w <= 0 || st->h <= 0) return z;
    cgrect_t b = ((cgrect_t (*)(id, SEL))objc_msgSend_stret)(s, sel_registerName("bounds"));   /* struct de 32 bytes */
    const char *g = grav_ca(cx_cstr(pl_gravity(s, 0)));
    if (!strcmp(g, "resize")) return b;
    double sx = b.w / st->w, sy = b.h / st->h, k = !strcmp(g, "resizeAspectFill") ? (sx > sy ? sx : sy) : (sx < sy ? sx : sy);
    z.w = st->w * k; z.h = st->h * k; z.x = b.x + (b.w - z.w) / 2; z.y = b.y + (b.h - z.h) / 2;
    return z;
}
static const char k_pbattrs = 0;
/* guardados como o app definiu; o layer compõe em BGRA (o formato do contents), qualquer que seja o pedido */
static id pl_pb_attrs(id s, SEL c) { (void)c; return objc_getAssociatedObject(s, &k_pbattrs); }
static void pl_set_pb_attrs(id s, SEL c, id a) { (void)c; objc_setAssociatedObject(s, &k_pbattrs, a, 3); }

/* ================================================================ AVSampleBufferDisplayLayer */
typedef void (*vt_cb_t)(void *ref, void *frame_ref, i32 st, u32 info, void *img, pv_cmtime_t pts, pv_cmtime_t dur);
i32 VTDecompressionSessionCreate(void *a, void *fd, id spec, id attrs, const void *cb, void **out);
i32 VTDecompressionSessionDecodeFrame(void *s, void *sb, u32 flags, void *ref, u32 *info);
i32 VTDecompressionSessionFinishDelayedFrames(void *s);
void VTDecompressionSessionInvalidate(void *s);
u8 VTDecompressionSessionCanAcceptFormatDescription(void *s, void *fd);
void *CMSampleBufferGetImageBuffer(void *sb);
void *CMSampleBufferGetFormatDescription(void *sb);
pv_cmtime_t CMSampleBufferGetPresentationTimeStamp(void *sb);
id CMSampleBufferGetSampleAttachmentsArray(void *sb, u8 create);
pv_cmtime_t CMTimebaseGetTime(void *tb);
u64 CVPixelBufferGetWidth(void *p);
u64 CVPixelBufferGetHeight(void *p);
u32 CVPixelBufferGetPixelFormatType(void *p);
extern const void *kCMSampleAttachmentKey_DoNotDisplay, *kCMSampleAttachmentKey_DisplayImmediately;
#define SBQ 32
typedef struct {
    cx_class_t *isa;
    id layer;                   /* fraco */
    void *vt, *vt_fd;
    struct { void *img; double pts; int now; } q[SBQ];
    int qn;
    void *timebase;
    id timer, error;
    long status;                /* AVQueuedSampleBufferRenderingStatus: 0 desconhecido, 1 renderizando, 2 falhou */
    void *ready_blk, *ready_q;
    int requesting;
} sbl_t;
cx_class_t MOSSBDLState_c __asm__("OBJC_CLASS_$_MOSSBDLState"), MOSSBDLState_m __asm__("OBJC_METACLASS_$_MOSSBDLState");
cx_class_t AVSampleBufferDisplayLayer_c __asm__("OBJC_CLASS_$_AVSampleBufferDisplayLayer"),
    AVSampleBufferDisplayLayer_m __asm__("OBJC_METACLASS_$_AVSampleBufferDisplayLayer");
static const char k_sbl = 0;
void *_Block_copy(const void *b);
void _Block_release(const void *b);
static void sbl_tick(id s, SEL c, id timer);
static sbl_t *sbl_of(id layer) {
    sbl_t *st = (sbl_t *)objc_getAssociatedObject(layer, &k_sbl);
    if (!st) {
        st = (sbl_t *)cx_alloc_instance(&MOSSBDLState_c);
        st->layer = layer;
        objc_setAssociatedObject(layer, &k_sbl, (id)st, 1);
        objc_release((id)st);
        st->timer = objc_retain(MSG(id, (id)cx_class_named("NSTimer"), "scheduledTimerWithTimeInterval:target:selector:userInfo:repeats:",
                                    1.0 / 60, (id)st, sel_registerName("tick:"), (id)0, (BOOL)YES));
    }
    return st;
}
static void sbl_clear(sbl_t *st) {
    for (int i = 0; i < st->qn; i++) objc_release((id)st->q[i].img);
    st->qn = 0;
}
static void sbl_dealloc(id s, SEL c) {
    sbl_t *st = (sbl_t *)s;
    sbl_clear(st);
    if (st->vt) VTDecompressionSessionInvalidate(st->vt);
    if (st->timer) { MSG0(void, st->timer, "invalidate"); objc_release(st->timer); }
    if (st->ready_blk) _Block_release(st->ready_blk);
    IMP d = class_getMethodImplementation(cx_class_named("NSObject"), c);
    if (d) ((void (*)(id, SEL))d)(s, c);
}
/* entra na fila em ordem de pts (a mais antiga primeiro); cheia: o mais antigo sai */
static void sbl_push(sbl_t *st, void *img, double pts, int now) {
    if (st->qn == SBQ) { objc_release((id)st->q[0].img); for (int i = 1; i < SBQ; i++) st->q[i - 1] = st->q[i]; st->qn--; }
    int i = st->qn;
    while (i > 0 && st->q[i - 1].pts > pts) { st->q[i] = st->q[i - 1]; i--; }
    st->q[i].img = objc_retain((id)img);
    st->q[i].pts = pts;
    st->q[i].now = now;
    st->qn++;
}
static void sbl_vt_out(void *ref, void *frame_ref, i32 status, u32 info, void *img, pv_cmtime_t pts, pv_cmtime_t dur) {
    (void)dur;
    sbl_t *st = ref;
    int flags = (int)(long)frame_ref;   /* 1 DoNotDisplay, 2 DisplayImmediately */
    if (status) { st->status = 2; return; }
    if (!img || (info & 2) || (flags & 1)) return;
    sbl_push(st, img, pv_sec(pts), flags & 2);
}
static int att_true(void *sb, const void *key) {
    id arr = CMSampleBufferGetSampleAttachmentsArray(sb, 0);
    id d = arr && MSG0(u64, arr, "count") ? MSG(id, arr, "objectAtIndex:", (u64)0) : 0;
    id v = d ? MSG(id, d, "objectForKey:", (id)key) : 0;
    return v && MSG0(BOOL, v, "boolValue");
}
static void sbl_enqueue(id s, SEL c, void *sb) {
    (void)c;
    sbl_t *st = sbl_of(s);
    if (!sb || st->status == 2) return;
    int flags = (att_true(sb, kCMSampleAttachmentKey_DoNotDisplay) ? 1 : 0) | (att_true(sb, kCMSampleAttachmentKey_DisplayImmediately) ? 2 : 0);
    double pts = pv_sec(CMSampleBufferGetPresentationTimeStamp(sb));
    void *img = CMSampleBufferGetImageBuffer(sb);
    st->status = 1;
    if (img) { if (!(flags & 1)) sbl_push(st, img, pts, flags & 2); return; }
    void *fd = CMSampleBufferGetFormatDescription(sb);
    if (st->vt && fd != st->vt_fd && !VTDecompressionSessionCanAcceptFormatDescription(st->vt, fd)) {
        VTDecompressionSessionFinishDelayedFrames(st->vt);
        VTDecompressionSessionInvalidate(st->vt);
        st->vt = 0;
    }
    if (!st->vt) {
        id attrs = MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary");
        id n = MSG(id, (id)cx_class_named("NSNumber"), "numberWithUnsignedInt:", (u32)0x42475241u);
        MSG(void, attrs, "setObject:forKey:", n, (id)kCVPixelBufferPixelFormatTypeKey);
        struct { vt_cb_t fn; void *ref; } cb = {sbl_vt_out, st};
        if (VTDecompressionSessionCreate(0, fd, 0, attrs, &cb, &st->vt)) {
            st->status = 2;
            id info = MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary");
            MSG(void, info, "setObject:forKey:", cx_str("O formato do vídeo não tem decodificador no mOS."), cx_str("NSLocalizedDescription"));
            id e = MSG(id, (id)cx_class_named("NSError"), "errorWithDomain:code:userInfo:", cx_str("AVFoundationErrorDomain"), (long long)-11821, info);
            if (st->error) objc_release(st->error);
            st->error = objc_retain(e);
            return;
        }
    }
    st->vt_fd = fd;
    VTDecompressionSessionDecodeFrame(st->vt, sb, 0, (void *)(long)flags, 0);
}
static void sbl_show(sbl_t *st, void *img) {
    u64 w = CVPixelBufferGetWidth(img), h = CVPixelBufferGetHeight(img);
    if (CVPixelBufferGetPixelFormatType(img) != 0x42475241u || !w || !h) return;   /* o VT do layer entrega BGRA */
    uint8_t *px = malloc((size_t)(w * h * 4));
    if (!px) return;
    CVPixelBufferLockBaseAddress(img, 1);
    const uint8_t *b = CVPixelBufferGetBaseAddress(img);
    u64 bpr = CVPixelBufferGetBytesPerRow(img);
    for (u64 y = 0; y < h; y++) memcpy(px + y * w * 4, b + y * bpr, (size_t)(w * 4));
    CVPixelBufferUnlockBaseAddress(img, 1);
    void *prov = CGDataProviderCreateWithData(0, px, (size_t)(w * h * 4), free_px);
    void *cs = CGColorSpaceCreateDeviceRGB();
    void *cg = CGImageCreate((size_t)w, (size_t)h, 8, 32, (size_t)w * 4, cs, 6 | 0x2000, prov, 0, 1, 0);
    CGColorSpaceRelease(cs);
    CGDataProviderRelease(prov);
    if (cg) { MSG(void, st->layer, "setContents:", (id)cg); CGImageRelease(cg); }
}
static void sbl_ready_call(void *a) {
    sbl_t *st = a;
    if (st->requesting && st->ready_blk) ((void (*)(void *))*(void **)((u8 *)st->ready_blk + 16))(st->ready_blk);
}
static void sbl_tick(id s, SEL c, id timer) {
    (void)c; (void)timer;
    sbl_t *st = (sbl_t *)s;
    if (st->qn && st->layer) {
        int k = -1;
        if (!st->timebase) k = 0;   /* sem timebase: o mais antigo, um por quadro do timer */
        else {
            double now = pv_sec(CMTimebaseGetTime(st->timebase));
            for (int i = 0; i < st->qn; i++) if (st->q[i].pts <= now || st->q[i].now) k = i;
        }
        if (k >= 0) {
            sbl_show(st, st->q[k].img);
            for (int i = 0; i <= k; i++) objc_release((id)st->q[i].img);
            for (int i = k + 1; i < st->qn; i++) st->q[i - k - 1] = st->q[i];
            st->qn -= k + 1;
        }
    }
    if (st->requesting && st->qn < SBQ / 2) dispatch_async_f(st->ready_q ? st->ready_q : dispatch_get_main_queue(), st, sbl_ready_call);
}
static void sbdl_flush(id s, SEL c) {
    (void)c;
    sbl_t *st = sbl_of(s);
    sbl_clear(st);
    if (st->vt) { VTDecompressionSessionInvalidate(st->vt); st->vt = 0; }
    if (st->status == 2) { st->status = 0; if (st->error) { objc_release(st->error); st->error = 0; } }
}
static void sbdl_flush_remove(id s, SEL c) { sbdl_flush(s, c); MSG(void, s, "setContents:", (id)0); }
static BOOL sbdl_ready_more(id s, SEL c) { (void)c; sbl_t *st = sbl_of(s); return st->status != 2 && st->qn < SBQ / 2; }
static void sbdl_request(id s, SEL c, void *queue, void *blk) {
    (void)c;
    sbl_t *st = sbl_of(s);
    if (st->ready_blk) _Block_release(st->ready_blk);
    st->ready_blk = blk ? _Block_copy(blk) : 0;
    st->ready_q = queue;
    st->requesting = blk != 0;
    if (blk) dispatch_async_f(queue ? queue : dispatch_get_main_queue(), st, sbl_ready_call);
}
static void sbdl_stop_request(id s, SEL c) { (void)c; sbl_t *st = sbl_of(s); st->requesting = 0; }
static long sbdl_status(id s, SEL c) { (void)c; return sbl_of(s)->status; }
static id sbdl_error(id s, SEL c) { (void)c; return sbl_of(s)->error; }
static void *sbdl_timebase(id s, SEL c) { (void)c; return sbl_of(s)->timebase; }
static void sbdl_set_timebase(id s, SEL c, void *tb) {
    (void)c;
    sbl_t *st = sbl_of(s);
    if (tb) objc_retain((id)tb);
    if (st->timebase) objc_release((id)st->timebase);
    st->timebase = tb;
}
static BOOL sbdl_requires_flush(id s, SEL c) { (void)c; return sbl_of(s)->status == 2; }

void cx_avplayervideo_init(void) {
    cx_class_t *ob = cx_class_named("NSObject");
    cx_define_class(&AVPlayerItemOutput_c, &AVPlayerItemOutput_m, "AVPlayerItemOutput", ob, 0);
    cx_define_class(&AVPlayerItemVideoOutput_c, &AVPlayerItemVideoOutput_m, "AVPlayerItemVideoOutput", &AVPlayerItemOutput_c, (u32)(sizeof(pvo_t) - 8));
#define M(sel, f) cx_add_method(&AVPlayerItemVideoOutput_c, sel, (void *)(f))
    M("init", pvo_init); M("initWithPixelBufferAttributes:", pvo_init_attrs); M("initWithOutputSettings:", pvo_init_settings);
    M("dealloc", pvo_dealloc); M("mosAttachItem:", pvo_attach);
    M("itemTimeForHostTime:", pvo_time_host); M("itemTimeForMachAbsoluteTime:", pvo_time_mach); M("itemTimeForCVTimeStamp:", pvo_time_cv);
    M("hasNewPixelBufferForItemTime:", pvo_has_new); M("copyPixelBufferForItemTime:itemTimeForDisplay:", pvo_copy);
    M("suppressesPlayerRendering", pvo_suppresses); M("setSuppressesPlayerRendering:", pvo_set_suppresses);
    M("delegate", pvo_delegate); M("setDelegate:queue:", pvo_set_delegate);
    M("requestNotificationOfMediaDataChangeWithAdvanceInterval:", pvo_request);
    M("requestNotificationOfMediaDataChangeAsSoonAsPossible", pvo_request);
    M("pixelBufferAttributes", pvo_attrs);
#undef M
    cx_define_class(&MOSAVPlayerLayerState_c, &MOSAVPlayerLayerState_m, "MOSAVPlayerLayerState", ob, (u32)(sizeof(pls_t) - 8));
    cx_add_method(&MOSAVPlayerLayerState_c, "dealloc", (void *)pls_dealloc);
    cx_add_method(&MOSAVPlayerLayerState_c, "tick:", (void *)pls_tick);
    cx_define_class(&MOSSBDLState_c, &MOSSBDLState_m, "MOSSBDLState", ob, (u32)(sizeof(sbl_t) - 8));
    cx_add_method(&MOSSBDLState_c, "dealloc", (void *)sbl_dealloc);
    cx_add_method(&MOSSBDLState_c, "tick:", (void *)sbl_tick);
    cx_class_t *ca = cx_class_named("CALayer");
    if (!ca) return;
    cx_define_class(&AVPlayerLayer_c, &AVPlayerLayer_m, "AVPlayerLayer", ca, 0);
    cx_add_cmethod(&AVPlayerLayer_c, "playerLayerWithPlayer:", (void *)pl_with_player);
    cx_add_method(&AVPlayerLayer_c, "player", (void *)pl_player);
    cx_add_method(&AVPlayerLayer_c, "setPlayer:", (void *)pl_set_player);
    cx_add_method(&AVPlayerLayer_c, "videoGravity", (void *)pl_gravity);
    cx_add_method(&AVPlayerLayer_c, "setVideoGravity:", (void *)pl_set_gravity);
    cx_add_method(&AVPlayerLayer_c, "isReadyForDisplay", (void *)pl_ready);
    cx_add_method(&AVPlayerLayer_c, "videoRect", (void *)pl_video_rect);
    cx_add_method(&AVPlayerLayer_c, "pixelBufferAttributes", (void *)pl_pb_attrs);
    cx_add_method(&AVPlayerLayer_c, "setPixelBufferAttributes:", (void *)pl_set_pb_attrs);
    cx_define_class(&AVSampleBufferDisplayLayer_c, &AVSampleBufferDisplayLayer_m, "AVSampleBufferDisplayLayer", ca, 0);
#define D(sel, f) cx_add_method(&AVSampleBufferDisplayLayer_c, sel, (void *)(f))
    D("enqueueSampleBuffer:", sbl_enqueue); D("flush", sbdl_flush); D("flushAndRemoveImage", sbdl_flush_remove);
    D("isReadyForMoreMediaData", sbdl_ready_more); D("requestMediaDataWhenReadyOnQueue:usingBlock:", sbdl_request);
    D("stopRequestingMediaData", sbdl_stop_request); D("status", sbdl_status); D("error", sbdl_error);
    D("controlTimebase", sbdl_timebase); D("setControlTimebase:", sbdl_set_timebase);
    D("requiresFlushToResumeDecoding", sbdl_requires_flush);
    D("videoGravity", pl_gravity); D("setVideoGravity:", pl_set_gravity);
#undef D
}
