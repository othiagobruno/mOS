/*
 * mOS — AVCapture: microfone e câmeras USB (UVC) para os apps do macOS.
 *
 * authorizationStatusForMediaType: e requestAccessForMediaType: consultam o TCC (kTCCServiceMicrophone para áudio,
 * kTCCServiceCamera para vídeo). Sem concessão, a sessão não corre e posta AVCaptureSessionRuntimeErrorNotification.
 *
 * Dispositivos: o microfone (o anel de captura do kernel, o mesmo do AudioUnit de entrada) e cada câmera que o driver
 * kernel/dev/uvc.c achou (GUI_DEVICE DEV_CAMERA_INFO). Os formatos da câmera viram AVCaptureDeviceFormat com
 * AVFrameRateRange; activeFormat/activeVideoMinFrameDuration valem sob lockForConfiguration: (ou o sessionPreset escolhe).
 *
 * AVCaptureSession roda numa thread: lê o quadro mais novo da câmera (DEV_CAMERA_READ: YUY2, NV12 ou JPEG decodificado
 * pelo ImageIO), converte para o formato que cada AVCaptureVideoDataOutput pediu (avcap_conv.c: '2vuy' nativo, 'yuvs',
 * '420v', '420f', 'y420', 'BGRA'), espelha se a conexão pede e entrega um CMSampleBuffer no delegate, na fila dele.
 * alwaysDiscardsLateVideoFrames (padrão YES): enquanto o delegate não volta, os quadros novos vão para
 * captureOutput:didDropSampleBuffer:fromConnection:. O áudio do microfone vai para os AVCaptureAudioDataOutput.
 * AVCaptureVideoPreviewLayer mostra o quadro mais novo (timer de 1/30 s); AVCapturePhotoOutput e
 * AVCaptureStillImageOutput usam o próximo quadro (JPEG pelo codificador do ImageIO).
 * Código original do mOS (MIT).
 */
#include "foundation.h"
#include "sys.h"
#include "avf_core.h"
#include "avcap_conv.h"
#include "imageio_codec.h"
#include "../../kernel/include/compat_gui.h"

const char *cx_cstr(id o);
int cx_media_tcc(const char *service);
int cx_tcc_preflight(const char *service);
int strcmp(const char *a, const char *b);
int snprintf(char *s, size_t n, const char *f, ...);
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
void *malloc(size_t n);
void *calloc(size_t n, size_t m);
void free(void *p);
id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void objc_setAssociatedObject(id obj, const void *key, id value, u64 policy);
id objc_getAssociatedObject(id obj, const void *key);
IMP class_getMethodImplementation(cx_class_t *c, SEL s);
void *objc_autoreleasePoolPush(void);
void objc_autoreleasePoolPop(void *p);
int pthread_create(unsigned long *t, const void *attr, void *(*fn)(void *), void *arg);
int usleep(unsigned us);
void *dispatch_get_main_queue(void);
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *));
void *_Block_copy(const void *b);
void _Block_release(const void *b);
extern void *cx_cm_audio_sample(const short *pcm, unsigned frames, double rate);

typedef struct { long long value; int timescale; unsigned flags; long long epoch; } cmt_t;
typedef struct { cmt_t duration, pts, dts; } cmtiming_t;
typedef struct { int w, h; } cmdim_t;
cmt_t CMClockGetTime(void *clock);
void *CMClockGetHostTimeClock(void);
int CMVideoFormatDescriptionCreate(void *a, u32 codec, int w, int h, id ext, void **out);
int CMVideoFormatDescriptionCreateForImageBuffer(void *a, void *img, void **out);
int CMSampleBufferCreateReadyWithImageBuffer(void *a, void *img, void *fd, const void *timing, void **out);
void *CMSampleBufferGetImageBuffer(void *sb);
int CVPixelBufferCreateWithBytes(void *a, u64 w, u64 h, u32 fmt, void *base, u64 bpr, void (*rel)(void *, const void *), void *ctx, id attrs, void **out);
int CVPixelBufferCreateWithPlanarBytes(void *a, u64 w, u64 h, u32 fmt, void *d, u64 ds, u64 np, void **pb, u64 *pw, u64 *ph, u64 *pr,
                                       void *rel, void *ctx, id attrs, void **out);
int CVPixelBufferLockBaseAddress(void *p, u64 f);
int CVPixelBufferUnlockBaseAddress(void *p, u64 f);
void *CVPixelBufferGetBaseAddress(void *p);
u64 CVPixelBufferGetBytesPerRow(void *p);
u64 CVPixelBufferGetWidth(void *p);
u64 CVPixelBufferGetHeight(void *p);
u32 CVPixelBufferGetPixelFormatType(void *p);
void CVBufferSetAttachment(void *p, id key, id val, u32 mode);
const void *CVYCbCrMatrixGetStringForIntegerCodePoint(int c);
extern const void *kCVImageBufferYCbCrMatrixKey, *kCVPixelBufferPixelFormatTypeKey, *kCVPixelBufferWidthKey, *kCVPixelBufferHeightKey;
void *CGDataProviderCreateWithData(void *info, const void *data, size_t size, void (*release)(void *, const void *, size_t));
void *CGImageCreate(size_t w, size_t h, size_t bpc, size_t bpp, size_t bpr, const void *space, u32 info, void *prov, const void *dec, _Bool interp, int intent);
void *CGColorSpaceCreateDeviceRGB(void);
void CGColorSpaceRelease(void *c);
void CGDataProviderRelease(void *p);
void CGImageRelease(void *i);

#define DEV_OP(op, a, b, c) ((long)SYS4(79 /* GUI_DEVICE */, (op), (a), (b), (c)))
#define FCC_YUY2 0x32595559u
#define FCC_NV12 0x3231564eu
#define FCC_MJPG 0x47504a4du
static cmt_t cmt(long long v, int ts) { cmt_t t = {v, ts, 1, 0}; return t; }
static cmt_t host_now(void) { return CMClockGetTime(CMClockGetHostTimeClock()); }
static id arr_new(void) { return MSG0(id, (id)cx_class_named("NSMutableArray"), "array"); }
static id dict_new(void) { return MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary"); }
static void dset(id d, const char *k, id v) { if (v) MSG(void, d, "setObject:forKey:", v, cx_str(k)); }
static void post(const char *name, id obj) {
    id nc = MSG0(id, (id)cx_class_named("NSNotificationCenter"), "defaultCenter");
    if (nc) MSG(void, nc, "postNotificationName:object:", cx_str(name), obj);
}
static id av_error(long code) {
    return MSG(id, (id)cx_class_named("NSError"), "errorWithDomain:code:userInfo:", cx_str("AVFoundationErrorDomain"), code, (id)0);
}
static void spin_lock(volatile int *l) { while (__atomic_exchange_n(l, 1, __ATOMIC_ACQUIRE)) usleep(50); }
static void spin_unlock(volatile int *l) { __atomic_store_n(l, 0, __ATOMIC_RELEASE); }

/* ================================================================ TCC */
static int is_audio_type(id type) {
    const char *s = type ? cx_cstr(type) : "";
    return !strcmp(s, "soun") || !strcmp(s, "AVMediaTypeAudio");
}
static int is_video_type(id type) {
    const char *s = type ? cx_cstr(type) : "";
    return !strcmp(s, "vide") || !strcmp(s, "AVMediaTypeVideo") || !strcmp(s, "muxx") || !strcmp(s, "AVMediaTypeMuxed");
}
static const char *svc_for(id type) { return is_audio_type(type) ? "kTCCServiceMicrophone" : "kTCCServiceCamera"; }
static long auth_status(id cls, SEL c, id type) {
    (void)cls; (void)c;
    int p = cx_media_tcc(svc_for(type));
    if (p < 0) return 0;
    return p == 1 ? 3 : 2;
}
void cx_tcc_request_async(const char *service, void (*cb)(int granted, void *ctx), void *ctx);
static void access_done(int granted, void *blk) {
    void *inv = *(void **)((u8 *)blk + 16);
    ((void (*)(void *, u8))inv)(blk, granted ? 1 : 0);
    _Block_release(blk);
}
/* requestAccessForMediaType: o bloco volta depois da escolha no diálogo do TCC, numa fila global (como no macOS) */
static void request_access(id cls, SEL c, id type, void *blk) {
    (void)cls; (void)c;
    if (!blk) { cx_media_tcc(svc_for(type)); return; }
    cx_tcc_request_async(svc_for(type), access_done, _Block_copy(blk));
}

/* ================================================================ dispositivos */
enum { KIND_MIC = 1, KIND_CAM = 2 };
typedef struct {
    cx_class_t *isa;
    int kind, cam, position, connected;
    char uid[48], name[64], model[32];
    int nmode, active, active_set, locked;
    gui_camera_mode_t mode[GUI_CAMERA_MODES];
    cmt_t min_dur, max_dur;
    id formats;
} avdev_t;
typedef struct { cx_class_t *isa; avdev_t *dev; int mode; void *fd; } avfmt_t;
typedef struct { cx_class_t *isa; double minr, maxr; } avfrr_t;
extern cx_class_t AVCaptureDevice_c __asm__("OBJC_CLASS_$_AVCaptureDevice"), AVCaptureDevice_m __asm__("OBJC_METACLASS_$_AVCaptureDevice");
cx_class_t AVCaptureDeviceFormat_c __asm__("OBJC_CLASS_$_AVCaptureDeviceFormat"), AVCaptureDeviceFormat_m __asm__("OBJC_METACLASS_$_AVCaptureDeviceFormat");
cx_class_t AVFrameRateRange_c __asm__("OBJC_CLASS_$_AVFrameRateRange"), AVFrameRateRange_m __asm__("OBJC_METACLASS_$_AVFrameRateRange");
#define MAX_DEVS 8
static avdev_t *devs[MAX_DEVS];
static int ndevs;

static double fps_of(u32 interval) { return interval ? 10000000.0 / interval : 0; }
static cmt_t dur_of(u32 interval) { return interval ? cmt(interval, 10000000) : cmt(1, 30); }
static u32 interval_of(cmt_t t) {
    if (!(t.flags & 1) || t.timescale <= 0 || t.value <= 0) return 0;
    return (u32)((double)t.value * 10000000.0 / t.timescale + 0.5);
}
static void dev_reset_active(avdev_t *d) {
    d->active = 0;
    for (int i = 0; i < d->nmode; i++) if (d->mode[i].width == 1280 && d->mode[i].height == 720) { d->active = i; break; }
    d->min_dur = dur_of(d->nmode ? d->mode[d->active].def_interval : 333333);
    d->max_dur = dur_of(d->nmode ? d->mode[d->active].max_interval : 333333);
}
static avdev_t *dev_find(const char *uid) {
    for (int i = 0; i < ndevs; i++) if (!strcmp(devs[i]->uid, uid)) return devs[i];
    return 0;
}
static avdev_t *dev_add(int kind) {
    if (ndevs >= MAX_DEVS) return 0;
    avdev_t *d = (avdev_t *)cx_alloc_instance(&AVCaptureDevice_c);
    if (!d) return 0;
    d->kind = kind;
    devs[ndevs++] = d;   /* a tabela segura a referência: o mesmo objeto volta em toda consulta */
    return d;
}
/* atualiza a lista com o que o kernel tem agora; câmeras que sumiram ficam com connected = 0 */
static void dev_refresh(void) {
    int kinds[2];
    if (avf_device_kinds(kinds, 2) > 0 && !dev_find("mos-mic")) {
        avdev_t *m = dev_add(KIND_MIC);
        if (m) { snprintf(m->uid, sizeof m->uid, "mos-mic"); snprintf(m->name, sizeof m->name, "Microfone do mOS"); snprintf(m->model, sizeof m->model, "mOS"); m->connected = 1; m->cam = -1; }
    }
    for (int i = 0; i < ndevs; i++) if (devs[i]->kind == KIND_CAM) devs[i]->connected = 0;
    gui_camera_info_t *info = calloc(1, sizeof *info);
    if (!info) return;
    for (int k = 0; k < 8; k++) {
        if (DEV_OP(DEV_CAMERA_INFO, k, info, 0) != 1) break;
        char uid[48];
        snprintf(uid, sizeof uid, "uvc-%s-%04x%04x", info->path, info->vid, info->pid);
        avdev_t *d = dev_find(uid);
        if (!d) {
            d = dev_add(KIND_CAM);
            if (!d) break;
            snprintf(d->uid, sizeof d->uid, "%s", uid);
            snprintf(d->name, sizeof d->name, "%s", info->name);
            snprintf(d->model, sizeof d->model, "UVC Camera VendorID_%u ProductID_%u", info->vid, info->pid);
            d->nmode = info->nmode;
            memcpy(d->mode, info->mode, sizeof d->mode);
            dev_reset_active(d);
        }
        d->cam = k;
        d->connected = 1;
    }
    free(info);
}
static id dev_list(id type) {
    dev_refresh();
    id a = arr_new();
    for (int i = 0; i < ndevs; i++) {
        avdev_t *d = devs[i];
        if (!d->connected) continue;
        if (type && ((d->kind == KIND_MIC) != is_audio_type(type) || (d->kind == KIND_CAM && !is_video_type(type)))) continue;
        MSG(void, a, "addObject:", (id)d);
    }
    return a;
}
static id dev_devices(id cls, SEL c) { (void)cls; (void)c; return dev_list(0); }
static id dev_typed(id cls, SEL c, id type) { (void)cls; (void)c; return dev_list(type); }
static id dev_default(id cls, SEL c, id type) {
    (void)cls; (void)c;
    id a = dev_list(type);
    return MSG0(u64, a, "count") ? MSG(id, a, "objectAtIndex:", (u64)0) : 0;
}
static id dev_default3(id cls, SEL c, id dtype, id media, i64 pos) { (void)dtype; (void)pos; return dev_default(cls, c, media); }
static id dev_with_uid(id cls, SEL c, id uid) {
    (void)cls; (void)c;
    dev_refresh();
    avdev_t *d = uid ? dev_find(cx_cstr(uid)) : 0;
    return d && d->connected ? (id)d : 0;
}
static id dev_discovery(id cls, SEL c, id types, id media, i64 pos) {
    (void)cls; (void)c; (void)types; (void)pos;
    id s = cx_alloc_instance(cx_class_named("AVCaptureDeviceDiscoverySession"));
    if (s && media) objc_setAssociatedObject(s, "media", media, 1);
    return s ? objc_autorelease(s) : 0;
}
static id disc_devices(id s, SEL c) { (void)c; return dev_list(objc_getAssociatedObject(s, "media")); }

#define DEV(o) ((avdev_t *)(o))
static id dev_name(id s, SEL c) { (void)c; return cx_str(DEV(s)->name); }
static id dev_uid(id s, SEL c) { (void)c; return cx_str(DEV(s)->uid); }
static id dev_model(id s, SEL c) { (void)c; return cx_str(DEV(s)->model); }
static id dev_manufacturer(id s, SEL c) { (void)c; return cx_str(DEV(s)->kind == KIND_MIC ? "mOS" : ""); }
static id dev_type(id s, SEL c) {
    (void)c;
    return cx_str(DEV(s)->kind == KIND_MIC ? "AVCaptureDeviceTypeMicrophone" : "AVCaptureDeviceTypeExternal");
}
static i64 dev_position(id s, SEL c) { (void)c; return DEV(s)->position; }
static i32 dev_transport(id s, SEL c) { (void)c; return DEV(s)->kind == KIND_CAM ? 0x75736220 /* 'usb ' */ : 0x626c746e /* 'bltn' */; }
static BOOL dev_connected(id s, SEL c) { (void)c; dev_refresh(); return DEV(s)->connected ? YES : NO; }
static BOOL dev_no(id s, SEL c) { (void)s; (void)c; return NO; }
static BOOL dev_has_media(id s, SEL c, id type) {
    (void)c;
    return DEV(s)->kind == KIND_MIC ? (BOOL)is_audio_type(type) : (BOOL)is_video_type(type);
}
static BOOL dev_supports_preset(id s, SEL c, id p) {
    (void)c;
    if (DEV(s)->kind == KIND_MIC) return p && (!strcmp(cx_cstr(p), "AVCaptureSessionPresetHigh") || !strcmp(cx_cstr(p), "AVCaptureSessionPresetMedium") || !strcmp(cx_cstr(p), "AVCaptureSessionPresetLow"));
    return p != 0;
}
static BOOL dev_lock(id s, SEL c, id *err) { (void)c; if (err) *err = 0; DEV(s)->locked = 1; return YES; }
static void dev_unlock(id s, SEL c) { (void)c; DEV(s)->locked = 0; }
static id fmt_make(avdev_t *d, int i) {
    avfmt_t *f = (avfmt_t *)cx_alloc_instance(&AVCaptureDeviceFormat_c);
    if (!f) return 0;
    f->dev = d; f->mode = i;
    u32 cc = d->mode[i].fourcc == FCC_MJPG ? 0x646d6231 /* 'dmb1' */ : d->mode[i].fourcc == FCC_NV12 ? 0x34323076 : 0x79757673;
    CMVideoFormatDescriptionCreate(0, cc, d->mode[i].width, d->mode[i].height, 0, &f->fd);
    return (id)f;
}
static id dev_formats(id s, SEL c) {
    (void)c;
    avdev_t *d = DEV(s);
    if (!d->formats) {
        d->formats = objc_retain(arr_new());
        for (int i = 0; i < d->nmode; i++) { id f = fmt_make(d, i); if (f) { MSG(void, d->formats, "addObject:", f); objc_release(f); } }
    }
    return d->formats;
}
static id dev_active_format(id s, SEL c) {
    avdev_t *d = DEV(s);
    id a = dev_formats(s, c);
    return d->nmode && (u64)d->active < MSG0(u64, a, "count") ? MSG(id, a, "objectAtIndex:", (u64)d->active) : 0;
}
static void dev_set_active_format(id s, SEL c, id f) {
    (void)c;
    avdev_t *d = DEV(s);
    if (!f || ((avfmt_t *)f)->dev != d) return;
    d->active = ((avfmt_t *)f)->mode;
    d->active_set = 1;
    d->min_dur = dur_of(d->mode[d->active].def_interval);
    d->max_dur = dur_of(d->mode[d->active].max_interval);
}
static cmt_t dev_min_dur(id s, SEL c) { (void)c; return DEV(s)->min_dur; }
static cmt_t dev_max_dur(id s, SEL c) { (void)c; return DEV(s)->max_dur; }
/* kCMTimeInvalid volta ao padrão do formato ativo */
static void dev_set_min_dur(id s, SEL c, cmt_t t) {
    (void)c;
    avdev_t *d = DEV(s);
    if (!d->nmode) return;
    gui_camera_mode_t *m = &d->mode[d->active];
    u32 iv = interval_of(t);
    if (!iv) iv = m->def_interval;
    if (iv < m->min_interval) iv = m->min_interval;
    if (m->max_interval && iv > m->max_interval) iv = m->max_interval;
    d->min_dur = dur_of(iv);
}
static void dev_set_max_dur(id s, SEL c, cmt_t t) {
    (void)c;
    avdev_t *d = DEV(s);
    if (!d->nmode) return;
    u32 iv = interval_of(t);
    d->max_dur = dur_of(iv ? iv : d->mode[d->active].max_interval);
}

#define FMT(o) ((avfmt_t *)(o))
static void *fmt_desc(id s, SEL c) { (void)c; return FMT(s)->fd; }
static id fmt_media(id s, SEL c) { (void)s; (void)c; return cx_str("vide"); }
static id fmt_ranges(id s, SEL c) {
    (void)c;
    gui_camera_mode_t *m = &FMT(s)->dev->mode[FMT(s)->mode];
    id a = arr_new();
    avfrr_t *r = (avfrr_t *)cx_alloc_instance(&AVFrameRateRange_c);
    if (!r) return a;
    r->maxr = fps_of(m->min_interval ? m->min_interval : m->def_interval);
    r->minr = fps_of(m->max_interval ? m->max_interval : m->def_interval);
    MSG(void, a, "addObject:", (id)r);
    objc_release((id)r);
    return a;
}
static cmdim_t fmt_still(id s, SEL c) {
    (void)c;
    gui_camera_mode_t *m = &FMT(s)->dev->mode[FMT(s)->mode];
    cmdim_t d = {m->width, m->height};
    return d;
}
static double fmt_zoom(id s, SEL c) { (void)s; (void)c; return 1.0; }
static float fmt_fov(id s, SEL c) { (void)s; (void)c; return 0; }
static u32 fmt_subtype(id s, SEL c) {   /* mOS: o fourcc do UVC, para depuração */
    (void)c;
    return FMT(s)->dev->mode[FMT(s)->mode].fourcc;
}
static double frr_min(id s, SEL c) { (void)c; return ((avfrr_t *)s)->minr; }
static double frr_max(id s, SEL c) { (void)c; return ((avfrr_t *)s)->maxr; }
static cmt_t frr_min_dur(id s, SEL c) { (void)c; double r = ((avfrr_t *)s)->maxr; return r > 0 ? cmt((long long)(10000000.0 / r + 0.5), 10000000) : cmt(1, 30); }
static cmt_t frr_max_dur(id s, SEL c) { (void)c; double r = ((avfrr_t *)s)->minr; return r > 0 ? cmt((long long)(10000000.0 / r + 0.5), 10000000) : cmt(1, 30); }

/* ================================================================ entradas e conexões */
typedef struct { cx_class_t *isa; id device; } avin_t;
typedef struct { cx_class_t *isa; id output; int enabled, mirrored, auto_mirror; i64 orientation; } avconn_t;
cx_class_t AVCaptureInput_c __asm__("OBJC_CLASS_$_AVCaptureInput"), AVCaptureInput_m __asm__("OBJC_METACLASS_$_AVCaptureInput");
extern cx_class_t AVCaptureDeviceInput_c __asm__("OBJC_CLASS_$_AVCaptureDeviceInput"), AVCaptureDeviceInput_m __asm__("OBJC_METACLASS_$_AVCaptureDeviceInput");
cx_class_t AVCaptureConnection_c __asm__("OBJC_CLASS_$_AVCaptureConnection"), AVCaptureConnection_m __asm__("OBJC_METACLASS_$_AVCaptureConnection");
cx_class_t AVCaptureScreenInput_c __asm__("OBJC_CLASS_$_AVCaptureScreenInput"), AVCaptureScreenInput_m __asm__("OBJC_METACLASS_$_AVCaptureScreenInput");
typedef struct { cx_class_t *isa; u32 display; } avscreen_t;

static id input_init_device(id s, SEL c, id dev, id *err) {
    (void)c;
    if (err) *err = 0;
    if (!s) return 0;
    if (!dev) { if (err) *err = av_error(-11814 /* AVErrorDeviceNotConnected */); objc_release(s); return 0; }
    int tcc = cx_media_tcc(DEV(dev)->kind == KIND_MIC ? "kTCCServiceMicrophone" : "kTCCServiceCamera");
    if (tcc == 0) { if (err) *err = av_error(-11852 /* AVErrorApplicationIsNotAuthorizedToUseDevice */); objc_release(s); return 0; }
    ((avin_t *)s)->device = objc_retain(dev);
    return s;
}
static id input_with_device(id cls, SEL c, id dev, id *err) {
    id s = cx_alloc_instance((cx_class_t *)cls);
    s = input_init_device(s, c, dev, err);
    return s ? objc_autorelease(s) : 0;
}
static id input_device(id s, SEL c) { (void)c; return ((avin_t *)s)->device; }
static id input_ports(id s, SEL c) { (void)s; (void)c; return arr_new(); }
static id screen_init(id s, SEL c, u32 display) { (void)c; ((avscreen_t *)s)->display = display ? display : 1; return s; }

static id conn_make(id output) {
    avconn_t *k = (avconn_t *)cx_alloc_instance(&AVCaptureConnection_c);
    if (!k) return 0;
    k->output = output; k->enabled = 1; k->auto_mirror = 1; k->orientation = 3 /* LandscapeRight */;
    return (id)k;
}
#define CONN(o) ((avconn_t *)(o))
static BOOL conn_enabled(id s, SEL c) { (void)c; return CONN(s)->enabled ? YES : NO; }
static void conn_set_enabled(id s, SEL c, BOOL v) { (void)c; CONN(s)->enabled = v; }
static BOOL conn_yes(id s, SEL c) { (void)s; (void)c; return YES; }
static BOOL conn_no(id s, SEL c) { (void)s; (void)c; return NO; }
static BOOL conn_mirrored(id s, SEL c) { (void)c; return CONN(s)->mirrored ? YES : NO; }
static void conn_set_mirrored(id s, SEL c, BOOL v) { (void)c; CONN(s)->mirrored = v; }
static BOOL conn_auto(id s, SEL c) { (void)c; return CONN(s)->auto_mirror ? YES : NO; }
static void conn_set_auto(id s, SEL c, BOOL v) { (void)c; CONN(s)->auto_mirror = v; }
static i64 conn_orient(id s, SEL c) { (void)c; return CONN(s)->orientation; }
static void conn_set_orient(id s, SEL c, i64 v) { (void)c; CONN(s)->orientation = v; }   /* câmera USB: sem rotação (isVideoOrientationSupported = NO) */
static BOOL conn_angle_ok(id s, SEL c, double a) { (void)s; (void)c; return a == 0; }
static double conn_angle(id s, SEL c) { (void)s; (void)c; return 0; }
static id conn_output(id s, SEL c) { (void)c; return CONN(s)->output; }
static id conn_ports(id s, SEL c) { (void)s; (void)c; return arr_new(); }

/* ================================================================ saídas */
typedef struct {
    cx_class_t *isa;
    id delegate; void *queue; id settings;
    u32 fmt; int discard_late;
    volatile int inflight;
    id conn;
} avvdo_t;
typedef struct { id settings, delegate; } photoreq_t;
typedef struct { cx_class_t *isa; id conn; photoreq_t req[8]; int nreq; volatile int lock; int hires; } avpho_t;
typedef struct { cx_class_t *isa; id conn; id settings; void *blk[8]; int nblk; volatile int lock; } avstill_t;
typedef struct { cx_class_t *isa; i64 uid; id format; int flash; } avpset_t;
typedef struct { cx_class_t *isa; i64 uid; int w, h; } avrset_t;
typedef struct { cx_class_t *isa; void *pb; id resolved; id settings; cmt_t ts; id jpeg; void *cg; int w, h; } avphoto_t;
cx_class_t AVCaptureOutput_c __asm__("OBJC_CLASS_$_AVCaptureOutput"), AVCaptureOutput_m __asm__("OBJC_METACLASS_$_AVCaptureOutput");
extern cx_class_t AVCaptureVideoDataOutput_c __asm__("OBJC_CLASS_$_AVCaptureVideoDataOutput"), AVCaptureVideoDataOutput_m __asm__("OBJC_METACLASS_$_AVCaptureVideoDataOutput");
extern cx_class_t AVCapturePhotoOutput_c __asm__("OBJC_CLASS_$_AVCapturePhotoOutput"), AVCapturePhotoOutput_m __asm__("OBJC_METACLASS_$_AVCapturePhotoOutput");
extern cx_class_t AVCapturePhotoSettings_c __asm__("OBJC_CLASS_$_AVCapturePhotoSettings"), AVCapturePhotoSettings_m __asm__("OBJC_METACLASS_$_AVCapturePhotoSettings");
cx_class_t AVCaptureResolvedPhotoSettings_c __asm__("OBJC_CLASS_$_AVCaptureResolvedPhotoSettings"), AVCaptureResolvedPhotoSettings_m __asm__("OBJC_METACLASS_$_AVCaptureResolvedPhotoSettings");
cx_class_t AVCapturePhoto_c __asm__("OBJC_CLASS_$_AVCapturePhoto"), AVCapturePhoto_m __asm__("OBJC_METACLASS_$_AVCapturePhoto");
cx_class_t AVCaptureStillImageOutput_c __asm__("OBJC_CLASS_$_AVCaptureStillImageOutput"), AVCaptureStillImageOutput_m __asm__("OBJC_METACLASS_$_AVCaptureStillImageOutput");
#define VDO(o) ((avvdo_t *)(o))

static u32 fmt_from_settings(id st) {
    id v = st ? MSG(id, st, "objectForKey:", (id)kCVPixelBufferPixelFormatTypeKey) : 0;
    if (v && MSG(BOOL, v, "respondsToSelector:", sel_registerName("objectAtIndex:"))) v = MSG0(u64, v, "count") ? MSG(id, v, "objectAtIndex:", (u64)0) : 0;
    u32 f = v ? (u32)MSG0(i64, v, "longLongValue") : 0;
    return avc_supported(f) ? f : 0;   /* 0: o nativo da câmera */
}
static id vdo_init(id s, SEL c) {
    (void)c;
    VDO(s)->discard_late = 1;
    VDO(s)->conn = conn_make(s);
    return s;
}
/* a conexão de cada saída: no campo das classes daqui; nas outras (AVCaptureAudioDataOutput do slack_frameworks.c, cujos
 * campos começam logo depois do isa) num objeto associado */
static const char k_conn = 0;
static id out_conn_lazy(id s) {
    cx_class_t *k = object_getClass(s);
    id *slot = k == &AVCaptureVideoDataOutput_c ? &VDO(s)->conn : k == &AVCapturePhotoOutput_c ? &((avpho_t *)s)->conn
             : k == &AVCaptureStillImageOutput_c ? &((avstill_t *)s)->conn : 0;
    if (slot) { if (!*slot) *slot = conn_make(s); return *slot; }
    id c = objc_getAssociatedObject(s, &k_conn);
    if (!c) { c = conn_make(s); objc_setAssociatedObject(s, &k_conn, c, 1); objc_release(c); }
    return c;
}
static void vdo_set_delegate(id s, SEL c, id d, void *q) { (void)c; VDO(s)->delegate = d; VDO(s)->queue = q; }
static id vdo_delegate(id s, SEL c) { (void)c; return VDO(s)->delegate; }
static void *vdo_queue(id s, SEL c) { (void)c; return VDO(s)->queue; }
static id vdo_settings(id s, SEL c) {
    (void)c;
    if (VDO(s)->settings) return VDO(s)->settings;
    id d = dict_new();
    MSG(void, d, "setObject:forKey:", cx_num_i(VDO(s)->fmt ? VDO(s)->fmt : AVC_FMT_2VUY), (id)kCVPixelBufferPixelFormatTypeKey);
    return d;
}
static void vdo_set_settings(id s, SEL c, id st) {
    (void)c;
    if (st) objc_retain(st);
    if (VDO(s)->settings) objc_release(VDO(s)->settings);
    VDO(s)->settings = st;
    VDO(s)->fmt = fmt_from_settings(st);
}
static BOOL vdo_discards(id s, SEL c) { (void)c; return VDO(s)->discard_late ? YES : NO; }
static void vdo_set_discards(id s, SEL c, BOOL v) { (void)c; VDO(s)->discard_late = v; }
static id vdo_formats(id s, SEL c) {
    (void)s; (void)c;
    static const u32 f[] = {AVC_FMT_420V, AVC_FMT_420F, AVC_FMT_2VUY, AVC_FMT_YUVS, AVC_FMT_BGRA, AVC_FMT_Y420};
    id a = arr_new();
    for (unsigned i = 0; i < sizeof f / sizeof f[0]; i++) MSG(void, a, "addObject:", cx_num_i(f[i]));
    return a;
}
static id vdo_codecs(id s, SEL c) { (void)s; (void)c; return arr_new(); }
static id out_connections(id s, SEL c) { (void)c; id a = arr_new(); MSG(void, a, "addObject:", out_conn_lazy(s)); return a; }
static id out_conn_for(id s, SEL c, id type) {
    (void)c;
    if (cx_responds(object_getClass(s), sel_registerName("sampleBufferDelegate")) && object_getClass(s) != &AVCaptureVideoDataOutput_c)
        return is_audio_type(type) ? out_conn_lazy(s) : 0;   /* saída de áudio */
    return is_video_type(type) ? out_conn_lazy(s) : 0;
}

/* ================================================================ sessão */
typedef struct { id obj; int is_input, kind; avdev_t *dev; } ioitem_t;
typedef struct {
    cx_class_t *isa;
    int running, err, screen;
    ioitem_t *items; int n, cap;
    id preset;
    volatile int run, done;
    unsigned long th;
    int cam, mic;
    avdev_t *camdev;
    gui_camera_info_t cur;
    u8 *rbuf; u32 rcap;
    /* quadro mais novo em BGRA para a prévia */
    volatile int pvlock;
    u8 *pv; int pvw, pvh, pvmirror; u64 pvseq;
    int previews;
    double audio_t0; u64 audio_frames;
} avsess_t;
extern cx_class_t AVCaptureSession_c __asm__("OBJC_CLASS_$_AVCaptureSession"), AVCaptureSession_m __asm__("OBJC_METACLASS_$_AVCaptureSession");
#define SESS(o) ((avsess_t *)(o))

static void sess_dealloc(id s, SEL c) {
    avsess_t *ss = SESS(s);
    ss->run = 0;
    for (int i = 0; i < 200 && ss->th && !ss->done; i++) usleep(1000);
    for (int i = 0; i < ss->n; i++) objc_release(ss->items[i].obj);
    free(ss->items);
    free(ss->rbuf);
    free(ss->pv);
    if (ss->preset) objc_release(ss->preset);
    IMP d = class_getMethodImplementation(cx_class_named("NSObject"), c);
    if (d) ((void (*)(id, SEL))d)(s, c);
}

/* ---------- um quadro da câmera em CVPixelBuffer */
static void pb_free(void *ctx, const void *base) { (void)base; free(ctx); }
static void pb_free_planar(void *ctx, const void *d, u64 sz, u64 np, const void **pl) { (void)d; (void)sz; (void)np; (void)pl; free(ctx); }
typedef struct { int src; const u8 *data; u32 len; int w, h; u8 *bgra; } frame_t;
static void *make_pb(const frame_t *f, u32 fmt, int mirror) {
    size_t bpr[3], rows[3];
    int np = avc_planes(fmt, f->w, f->h, bpr, rows);
    if (!np) return 0;
    size_t total = 0, off[3];
    for (int i = 0; i < np; i++) { bpr[i] = (bpr[i] + 15) & ~(size_t)15; off[i] = total; total += bpr[i] * rows[i]; }
    u8 *mem = malloc(total);
    if (!mem) return 0;
    u8 *pl[3] = {mem + off[0], np > 1 ? mem + off[1] : 0, np > 2 ? mem + off[2] : 0};
    int rc = f->bgra ? avc_convert(AVC_SRC_BGRA, f->bgra, (size_t)f->w * f->h * 4, f->w, f->h, fmt, pl, bpr)
                     : avc_convert(f->src, f->data, f->len, f->w, f->h, fmt, pl, bpr);
    if (rc) { free(mem); return 0; }
    if (mirror) avc_mirror(fmt, f->w, f->h, pl, bpr);
    void *pb = 0;
    if (np == 1) CVPixelBufferCreateWithBytes(0, (u64)f->w, (u64)f->h, fmt, mem, bpr[0], pb_free, mem, 0, &pb);
    else {
        void *pbase[3] = {pl[0], pl[1], pl[2]};
        u64 pw[3], ph[3], pr[3];
        for (int i = 0; i < np; i++) { pw[i] = i ? (u64)(f->w + 1) / 2 : (u64)f->w; ph[i] = rows[i]; pr[i] = bpr[i]; }
        CVPixelBufferCreateWithPlanarBytes(0, (u64)f->w, (u64)f->h, fmt, mem, total, (u64)np, pbase, pw, ph, pr, (void *)pb_free_planar, mem, 0, &pb);
    }
    if (!pb) { free(mem); return 0; }
    if (fmt != AVC_FMT_BGRA) {
        const void *m = CVYCbCrMatrixGetStringForIntegerCodePoint(6);   /* BT.601, o padrão do UVC */
        if (m) CVBufferSetAttachment(pb, (id)kCVImageBufferYCbCrMatrixKey, (id)m, 1);
    }
    return pb;
}
/* para o CoreMediaIO (cmio.c): um quadro cru da câmera (YUY2, NV12 ou JPEG) em CVPixelBuffer do formato pedido; +1 */
void *cx_avcap_frame_pb(u32 fourcc, const u8 *data, u32 len, int w, int h, u32 fmt) {
    frame_t f = {fourcc == FCC_NV12 ? AVC_SRC_NV12 : AVC_SRC_YUY2, data, len, w, h, 0};
    mos_img_t img = {0};
    if (fourcc == FCC_MJPG) {
        if (mos_img_decode(data, len, &img) || !img.px) return 0;
        f.bgra = (u8 *)img.px; f.w = img.w; f.h = img.h; f.src = AVC_SRC_BGRA;
    }
    void *pb = make_pb(&f, fmt, 0);
    if (fourcc == FCC_MJPG) mos_img_free(&img);
    return pb;
}
static void *make_sb(void *pb, cmt_t pts, cmt_t dur) {
    void *fd = 0, *sb = 0;
    if (CMVideoFormatDescriptionCreateForImageBuffer(0, pb, &fd) || !fd) return 0;
    cmtiming_t t = {dur, pts, {0, 0, 0, 0}};
    CMSampleBufferCreateReadyWithImageBuffer(0, pb, fd, &t, &sb);
    objc_release((id)fd);
    return sb;
}

/* ---------- entrega no delegate, na fila dele */
typedef struct { id out, sb, conn, del; int drop; } job_t;
static void job_run(void *a) {
    job_t *j = a;
    const char *sel = j->drop ? "captureOutput:didDropSampleBuffer:fromConnection:" : "captureOutput:didOutputSampleBuffer:fromConnection:";
    if (j->del && cx_responds(object_getClass(j->del), sel_registerName(sel))) MSG(void, j->del, sel, j->out, j->sb, j->conn);
    if (!j->drop && object_getClass(j->out) == &AVCaptureVideoDataOutput_c) __atomic_sub_fetch(&VDO(j->out)->inflight, 1, __ATOMIC_ACQ_REL);
    objc_release(j->sb);
    objc_release(j->out);
    free(j);
}
static void deliver(id out, id del, void *queue, id sb, id conn, int drop) {
    job_t *j = calloc(1, sizeof *j);
    if (!j) return;
    j->out = objc_retain(out); j->sb = objc_retain(sb); j->conn = conn; j->del = del; j->drop = drop;
    if (queue) dispatch_async_f(queue, j, job_run); else job_run(j);
}

static u8 *frame_bgra(frame_t *f) {
    if (f->bgra) return f->bgra;
    size_t bpr[1] = {(size_t)f->w * 4};
    u8 *b = malloc((size_t)f->w * f->h * 4);
    if (!b) return 0;
    u8 *pl[1] = {b};
    if (avc_convert(f->src, f->data, f->len, f->w, f->h, AVC_FMT_BGRA, pl, bpr)) { free(b); return 0; }
    f->bgra = b;
    return b;
}

static void photo_fulfil(avsess_t *ss, frame_t *f, cmt_t pts);
static int is_file_output(id o);
static void rec_video(avsess_t *ss, id o, frame_t *f, cmt_t pts);
static void rec_audio(avsess_t *ss, id o, void *sample, double at, unsigned frames);
static void rec_session_stopped(avsess_t *ss);
static int rec_tick(id o, double now);
static double secs(cmt_t t);
static void still_fulfil(avsess_t *ss, frame_t *f, cmt_t pts);

static void on_frame(avsess_t *ss, frame_t *f) {
    cmt_t pts = host_now();
    cmt_t dur = ss->cur.cur_interval ? cmt(ss->cur.cur_interval, 10000000) : cmt(1, 30);
    for (int i = 0; i < ss->n; i++) {
        id o = ss->items[i].obj;
        if (ss->items[i].is_input || object_getClass(o) != &AVCaptureVideoDataOutput_c) continue;
        avvdo_t *v = VDO(o);
        if (!v->delegate || !CONN(v->conn)->enabled) continue;
        int late = v->discard_late && __atomic_load_n(&v->inflight, __ATOMIC_ACQUIRE) > 0;
        if (late && !cx_responds(object_getClass(v->delegate), sel_registerName("captureOutput:didDropSampleBuffer:fromConnection:"))) continue;
        u32 fmt = v->fmt ? v->fmt : (ss->cur.cur.fourcc == FCC_NV12 ? AVC_FMT_420V : AVC_FMT_2VUY);
        void *pb = make_pb(f, fmt, CONN(v->conn)->mirrored);
        if (!pb) continue;
        void *sb = make_sb(pb, pts, dur);
        objc_release((id)pb);
        if (!sb) continue;
        if (!late) __atomic_add_fetch(&v->inflight, 1, __ATOMIC_ACQ_REL);
        deliver(o, v->delegate, v->queue, (id)sb, v->conn, late);
        objc_release((id)sb);
    }
    if (ss->previews > 0 && frame_bgra(f)) {
        u8 *copy = malloc((size_t)f->w * f->h * 4);
        if (copy) {
            memcpy(copy, f->bgra, (size_t)f->w * f->h * 4);
            spin_lock(&ss->pvlock);
            u8 *old = ss->pv;
            ss->pv = copy; ss->pvw = f->w; ss->pvh = f->h; ss->pvseq++;
            spin_unlock(&ss->pvlock);
            free(old);
        }
    }
    photo_fulfil(ss, f, pts);
    still_fulfil(ss, f, pts);
    for (int i = 0; i < ss->n; i++)
        if (!ss->items[i].is_input && is_file_output(ss->items[i].obj)) rec_video(ss, ss->items[i].obj, f, pts);
}

int CMSampleBufferCreateCopyWithNewTiming(void *a, void *src, long nt, const void *t, void **out);
static void deliver_audio(avsess_t *ss) {
    short pcm[2048];
    int err = 0;
    unsigned n = avf_cap_pull(1, pcm, 1024, &err);
    if (!n) return;
    void *raw = cx_cm_audio_sample(pcm, n, 48000);
    if (!raw) return;
    /* pts no relógio do host: o primeiro bloco marca o início, os seguintes contam quadros */
    if (!ss->audio_frames) ss->audio_t0 = secs(host_now());
    double at = ss->audio_t0 + (double)ss->audio_frames / 48000.0;
    ss->audio_frames += n;
    cmtiming_t t = {cmt(n, 48000), cmt((long long)(at * 48000.0 + 0.5), 48000), {0, 0, 0, 0}};
    void *sample = 0;
    if (CMSampleBufferCreateCopyWithNewTiming(0, raw, 1, &t, &sample) || !sample) sample = objc_retain((id)raw);
    objc_release((id)raw);
    for (int i = 0; i < ss->n; i++)
        if (!ss->items[i].is_input && is_file_output(ss->items[i].obj)) rec_audio(ss, ss->items[i].obj, sample, at, n);
    for (int i = 0; i < ss->n; i++) {
        id o = ss->items[i].obj;
        if (ss->items[i].is_input || object_getClass(o) == &AVCaptureVideoDataOutput_c || is_file_output(o)) continue;
        if (!cx_responds(object_getClass(o), sel_registerName("sampleBufferDelegate"))) continue;
        id del = MSG0(id, o, "sampleBufferDelegate");
        void *q = cx_responds(object_getClass(o), sel_registerName("sampleBufferCallbackQueue")) ? MSG0(void *, o, "sampleBufferCallbackQueue") : 0;
        if (del) deliver(o, del, q, (id)sample, out_conn_lazy(o), 0);
    }
    objc_release((id)sample);
}

static void *sess_thread(void *a) {
    avsess_t *ss = a;
    while (ss->run) {
        void *pool = objc_autoreleasePoolPush();
        int got = 0;
        if (ss->cam >= 0) {
            long n = DEV_OP(DEV_CAMERA_READ, ss->cam, ss->rbuf, ss->rcap);
            if (n > 0) {
                frame_t f = {0, ss->rbuf, (u32)n, ss->cur.cur.width, ss->cur.cur.height, 0};
                mos_img_t img = {0};
                int ok = 1;
                if (ss->cur.cur.fourcc == FCC_MJPG) {
                    ok = mos_img_decode(ss->rbuf, (size_t)n, &img) == 0 && img.px;
                    if (ok) { f.bgra = (u8 *)img.px; f.w = img.w; f.h = img.h; f.src = AVC_SRC_BGRA; }
                } else f.src = ss->cur.cur.fourcc == FCC_NV12 ? AVC_SRC_NV12 : AVC_SRC_YUY2;
                if (ok) on_frame(ss, &f);
                if (ss->cur.cur.fourcc == FCC_MJPG) { if (ok) mos_img_free(&img); }
                else free(f.bgra);
                got = 1;
            } else if (n < 0) {
                /* câmera desligada no meio */
                ss->cam = -1;
                ss->err = 2;
                if (ss->camdev) { ss->camdev->connected = 0; post("AVCaptureDeviceWasDisconnectedNotification", (id)ss->camdev); }
                post("AVCaptureSessionRuntimeErrorNotification", (id)ss);
            }
        }
        if (ss->mic) deliver_audio(ss);
        if (ss->cam < 0) {   /* sem quadros: pedidos de parada das saídas de arquivo andam aqui */
            double now = secs(host_now());
            for (int i = 0; i < ss->n; i++)
                if (!ss->items[i].is_input && is_file_output(ss->items[i].obj)) rec_tick(ss->items[i].obj, now);
        }
        objc_autoreleasePoolPop(pool);
        if (!got) usleep(ss->cam >= 0 ? 3000 : 10000);
    }
    if (ss->cam >= 0) DEV_OP(DEV_CAMERA_CLOSE, ss->cam, 0, 0);
    ss->cam = -1;
    ss->done = 1;
    return 0;
}

/* sessionPreset -> modo da câmera */
static int pick_for_preset(avdev_t *d, const char *p) {
    avc_mode m[GUI_CAMERA_MODES];
    for (int i = 0; i < d->nmode; i++) { m[i].fourcc = d->mode[i].fourcc; m[i].w = d->mode[i].width; m[i].h = d->mode[i].height; m[i].min_interval = d->mode[i].min_interval; }
    static const struct { const char *n; int w, h; } sz[] = {
        {"AVCaptureSessionPresetLow", 192, 144}, {"AVCaptureSessionPresetMedium", 480, 360}, {"AVCaptureSessionPreset320x240", 320, 240},
        {"AVCaptureSessionPreset352x288", 352, 288}, {"AVCaptureSessionPreset640x480", 640, 480}, {"AVCaptureSessionPreset960x540", 960, 540},
        {"AVCaptureSessionPresetiFrame960x540", 960, 540}, {"AVCaptureSessionPreset1280x720", 1280, 720},
        {"AVCaptureSessionPresetiFrame1280x720", 1280, 720}, {"AVCaptureSessionPreset1920x1080", 1920, 1080},
        {"AVCaptureSessionPreset3840x2160", 3840, 2160},
    };
    if (p && !strcmp(p, "AVCaptureSessionPresetPhoto")) return avc_pick_mode(m, d->nmode, AVC_PICK_PHOTO, 0, 0);
    for (unsigned i = 0; p && i < sizeof sz / sizeof sz[0]; i++) if (!strcmp(p, sz[i].n)) return avc_pick_mode(m, d->nmode, AVC_PICK_SIZE, sz[i].w, sz[i].h);
    return avc_pick_mode(m, d->nmode, AVC_PICK_HIGH, 0, 0);
}

static int open_camera(avsess_t *ss, avdev_t *d) {
    if (!d->connected || d->cam < 0 || !d->nmode) return -1;
    const char *p = ss->preset ? cx_cstr(ss->preset) : "AVCaptureSessionPresetHigh";
    int mi = d->active_set || !strcmp(p, "AVCaptureSessionPresetInputPriority") ? d->active : pick_for_preset(d, p);
    if (mi < 0) return -1;
    if (!d->active_set) { d->active = mi; d->min_dur = dur_of(d->mode[mi].def_interval); }
    gui_camera_mode_t *m = &d->mode[mi];
    u32 iv = interval_of(d->min_dur);
    long r = DEV_OP(DEV_CAMERA_OPEN, d->cam, (u64)m->width | (u64)m->height << 32, (u64)m->fourcc | (u64)iv << 32);
    if (r < 0) return -1;
    if (DEV_OP(DEV_CAMERA_INFO, d->cam, &ss->cur, 0) != 1) { DEV_OP(DEV_CAMERA_CLOSE, d->cam, 0, 0); return -1; }
    u32 need = ss->cur.cur.max_size;
    u32 raw = (u32)ss->cur.cur.width * ss->cur.cur.height * 2;
    if (need < raw) need = raw;
    need += 65536;
    if (need > ss->rcap) { free(ss->rbuf); ss->rbuf = malloc(need); ss->rcap = ss->rbuf ? need : 0; }
    if (!ss->rbuf) { DEV_OP(DEV_CAMERA_CLOSE, d->cam, 0, 0); return -1; }
    ss->cam = d->cam;
    ss->camdev = d;
    return 0;
}

static void sess_start(id s, SEL c) {
    (void)c;
    avsess_t *ss = SESS(s);
    if (ss->running) return;
    /* entrada de tela: exige gravação de tela; os quadros são do SCStream */
    if (ss->screen) {
        ss->running = cx_tcc_preflight("kTCCServiceScreenCapture") == 1;
        ss->err = ss->running ? 0 : 1;
        if (ss->running) post("AVCaptureSessionDidStartRunningNotification", s);
        return;
    }
    avdev_t *camdev = 0;
    int mic = 0;
    for (int i = 0; i < ss->n; i++) if (ss->items[i].is_input) {
        if (ss->items[i].kind == KIND_MIC) mic = 1;
        if (ss->items[i].kind == KIND_CAM && !camdev) camdev = ss->items[i].dev;
    }
    ss->err = 0;
    ss->cam = -1;
    ss->mic = 0;
    ss->audio_frames = 0;
    if (mic) {
        int e = 0;
        ss->mic = avf_cap_start(1, 0, &e);
        if (!ss->mic) ss->err = e;
    }
    if (camdev) {
        dev_refresh();
        if (cx_media_tcc("kTCCServiceCamera") != 1) ss->err = 1;
        else if (open_camera(ss, camdev)) ss->err = 2;
    }
    if (ss->err || (!ss->mic && ss->cam < 0)) {
        if (!ss->err) ss->err = 2;
        if (ss->cam >= 0) { DEV_OP(DEV_CAMERA_CLOSE, ss->cam, 0, 0); ss->cam = -1; }
        post("AVCaptureSessionRuntimeErrorNotification", s);
        return;
    }
    ss->running = 1;
    ss->run = 1;
    ss->done = 0;
    if (pthread_create(&ss->th, 0, sess_thread, ss) != 0) {
        ss->run = 0; ss->running = 0; ss->th = 0;
        if (ss->cam >= 0) { DEV_OP(DEV_CAMERA_CLOSE, ss->cam, 0, 0); ss->cam = -1; }
        return;
    }
    post("AVCaptureSessionDidStartRunningNotification", s);
}
static void sess_stop(id s, SEL c) {
    (void)c;
    avsess_t *ss = SESS(s);
    if (!ss->running) return;
    ss->run = 0;
    for (int i = 0; i < 500 && ss->th && !ss->done; i++) usleep(1000);
    ss->th = 0;
    ss->running = 0;
    rec_session_stopped(ss);
    post("AVCaptureSessionDidStopRunningNotification", s);
}
static BOOL sess_running(id s, SEL c) { (void)c; return SESS(s)->running ? YES : NO; }
static BOOL sess_no(id s, SEL c) { (void)s; (void)c; return NO; }
static BOOL sess_can_add(id s, SEL c, id io) {
    (void)c;
    if (!io) return NO;
    avsess_t *ss = SESS(s);
    for (int i = 0; i < ss->n; i++) if (ss->items[i].obj == io) return NO;
    return YES;
}
static void sess_add(id s, SEL c, id io) {
    (void)c;
    if (!io || !sess_can_add(s, 0, io)) return;
    avsess_t *ss = SESS(s);
    if (object_getClass(io) == &AVCaptureScreenInput_c) ss->screen = 1;
    if (ss->n >= ss->cap) {
        int cap = ss->cap ? ss->cap * 2 : 4;
        ioitem_t *ni = calloc((size_t)cap, sizeof *ni);
        if (!ni) return;
        if (ss->items && ss->n) memcpy(ni, ss->items, (size_t)ss->n * sizeof *ni);
        free(ss->items);
        ss->items = ni;
        ss->cap = cap;
    }
    int input = cx_responds(object_getClass(io), sel_registerName("device")) || object_getClass(io) == &AVCaptureScreenInput_c;
    ioitem_t *it = &ss->items[ss->n];
    it->obj = objc_retain(io);
    it->is_input = input;
    it->dev = input && object_getClass(io) != &AVCaptureScreenInput_c ? (avdev_t *)MSG0(id, io, "device") : 0;
    it->kind = it->dev ? it->dev->kind : 0;
    ss->n++;
}
static void sess_remove(id s, SEL c, id io) {
    (void)c;
    avsess_t *ss = SESS(s);
    for (int i = 0; i < ss->n; i++) if (ss->items[i].obj == io) {
        if (object_getClass(io) == &AVCaptureScreenInput_c) ss->screen = 0;
        objc_release(io);
        ss->items[i] = ss->items[ss->n - 1];
        ss->n--;
        return;
    }
}
static id sess_list(id s, int inputs) {
    avsess_t *ss = SESS(s);
    id a = arr_new();
    for (int i = 0; i < ss->n; i++) if (ss->items[i].is_input == inputs) MSG(void, a, "addObject:", ss->items[i].obj);
    return a;
}
static id sess_inputs(id s, SEL c) { (void)c; return sess_list(s, 1); }
static id sess_outputs(id s, SEL c) { (void)c; return sess_list(s, 0); }
static id sess_connections(id s, SEL c) {
    (void)c;
    avsess_t *ss = SESS(s);
    id a = arr_new();
    for (int i = 0; i < ss->n; i++) if (!ss->items[i].is_input && cx_responds(object_getClass(ss->items[i].obj), sel_registerName("connections"))) {
        id cs = MSG0(id, ss->items[i].obj, "connections");
        if (cs) MSG(void, a, "addObjectsFromArray:", cs);
    }
    return a;
}
static void sess_cfg(id s, SEL c) { (void)s; (void)c; }
static id sess_preset(id s, SEL c) { (void)c; return SESS(s)->preset ? SESS(s)->preset : cx_str("AVCaptureSessionPresetHigh"); }
static void sess_set_preset(id s, SEL c, id p) {
    (void)c;
    if (!p) return;
    objc_retain(p);
    if (SESS(s)->preset) objc_release(SESS(s)->preset);
    SESS(s)->preset = p;
    /* outro preset solta o activeFormat escolhido à mão */
    if (strcmp(cx_cstr(p), "AVCaptureSessionPresetInputPriority"))
        for (int i = 0; i < SESS(s)->n; i++) if (SESS(s)->items[i].dev) SESS(s)->items[i].dev->active_set = 0;
}
static BOOL sess_can_preset(id s, SEL c, id p) { (void)s; (void)c; return p != 0; }
static i64 sess_err(id s, SEL c) { (void)c; return SESS(s)->err; }
static id sess_init(id s, SEL c) { (void)c; SESS(s)->cam = -1; return s; }

/* ================================================================ foto */
static i64 next_uid = 1;
static id pset_new(id cls, SEL c) {
    (void)cls; (void)c;
    avpset_t *p = (avpset_t *)cx_alloc_instance(&AVCapturePhotoSettings_c);
    if (!p) return 0;
    p->uid = __atomic_fetch_add(&next_uid, 1, __ATOMIC_RELAXED);
    return objc_autorelease((id)p);
}
static id pset_with_format(id cls, SEL c, id fmt) {
    id p = pset_new(cls, c);
    if (p && fmt) ((avpset_t *)p)->format = objc_retain(fmt);
    return p;
}
static id pset_from(id cls, SEL c, id other) { return pset_with_format(cls, c, other ? ((avpset_t *)other)->format : 0); }
static i64 pset_uid(id s, SEL c) { (void)c; return ((avpset_t *)s)->uid; }
static id pset_format(id s, SEL c) { (void)c; return ((avpset_t *)s)->format; }
static i64 pset_flash(id s, SEL c) { (void)c; return ((avpset_t *)s)->flash; }
static void pset_set_flash(id s, SEL c, i64 v) { (void)c; ((avpset_t *)s)->flash = (int)v; }
static i64 rset_uid(id s, SEL c) { (void)c; return ((avrset_t *)s)->uid; }
static cmdim_t rset_dim(id s, SEL c) { (void)c; cmdim_t d = {((avrset_t *)s)->w, ((avrset_t *)s)->h}; return d; }
static u64 rset_count(id s, SEL c) { (void)s; (void)c; return 1; }

static id pho_init(id s, SEL c) { (void)c; return s; }
static void pho_capture(id s, SEL c, id settings, id delegate) {
    (void)c;
    avpho_t *p = (avpho_t *)s;
    if (!delegate) return;
    spin_lock(&p->lock);
    if (p->nreq < 8) { p->req[p->nreq].settings = objc_retain(settings ? settings : pset_new(0, 0)); p->req[p->nreq].delegate = objc_retain(delegate); p->nreq++; }
    spin_unlock(&p->lock);
}
static id pho_codecs(id s, SEL c) { (void)s; (void)c; id a = arr_new(); MSG(void, a, "addObject:", cx_str("jpeg")); return a; }
static id pho_pixfmts(id s, SEL c) {
    (void)s; (void)c;
    id a = arr_new();
    MSG(void, a, "addObject:", cx_num_i(AVC_FMT_BGRA)); MSG(void, a, "addObject:", cx_num_i(AVC_FMT_420V)); MSG(void, a, "addObject:", cx_num_i(AVC_FMT_420F));
    return a;
}
static id pho_filetypes(id s, SEL c) { (void)s; (void)c; id a = arr_new(); MSG(void, a, "addObject:", cx_str("public.jpeg")); return a; }
static BOOL pho_hires(id s, SEL c) { (void)c; return ((avpho_t *)s)->hires ? YES : NO; }
static void pho_set_hires(id s, SEL c, BOOL v) { (void)c; ((avpho_t *)s)->hires = v; }

#define PHOTO(o) ((avphoto_t *)(o))
static void *photo_pb(id s, SEL c) { (void)c; return PHOTO(s)->pb; }
static id photo_resolved(id s, SEL c) { (void)c; return PHOTO(s)->resolved; }
static cmt_t photo_ts(id s, SEL c) { (void)c; return PHOTO(s)->ts; }
static i64 photo_count(id s, SEL c) { (void)s; (void)c; return 1; }
static BOOL photo_raw(id s, SEL c) { (void)s; (void)c; return NO; }
static id photo_metadata(id s, SEL c) { (void)s; (void)c; return dict_new(); }
static id jpeg_of_pb(void *pb) {
    if (!pb || CVPixelBufferGetPixelFormatType(pb) != AVC_FMT_BGRA) return 0;
    int w = (int)CVPixelBufferGetWidth(pb), h = (int)CVPixelBufferGetHeight(pb);
    CVPixelBufferLockBaseAddress(pb, 1);
    const u8 *base = CVPixelBufferGetBaseAddress(pb);
    u64 bpr = CVPixelBufferGetBytesPerRow(pb);
    u32 *px = malloc((size_t)w * h * 4);
    if (px && base) for (int y = 0; y < h; y++) memcpy(px + (size_t)y * w, base + (size_t)y * bpr, (size_t)w * 4);
    CVPixelBufferUnlockBaseAddress(pb, 1);
    if (!px || !base) { free(px); return 0; }
    u8 *out = 0;
    size_t n = 0;
    int rc = mos_img_encode_jpeg(px, w, h, 90, &out, &n);
    free(px);
    if (rc || !out) return 0;
    id d = MSG(id, (id)cx_class_named("NSData"), "dataWithBytes:length:", (const void *)out, (u64)n);
    free(out);
    return d;
}
static id photo_data(id s, SEL c) {
    (void)c;
    if (!PHOTO(s)->jpeg) { id d = jpeg_of_pb(PHOTO(s)->pb); if (d) PHOTO(s)->jpeg = objc_retain(d); }
    return PHOTO(s)->jpeg;
}
static void cg_free(void *info, const void *data, size_t n) { (void)data; (void)n; objc_release((id)info); }
static void *photo_cgimage(id s, SEL c) {
    (void)c;
    void *pb = PHOTO(s)->pb;
    if (!pb) return 0;
    if (PHOTO(s)->cg) return PHOTO(s)->cg;   /* a foto guarda o CGImage (regra get: sem +1 para o app) */
    CVPixelBufferLockBaseAddress(pb, 1);
    const void *base = CVPixelBufferGetBaseAddress(pb);
    size_t bpr = CVPixelBufferGetBytesPerRow(pb), h = CVPixelBufferGetHeight(pb), w = CVPixelBufferGetWidth(pb);
    CVPixelBufferUnlockBaseAddress(pb, 1);
    void *prov = CGDataProviderCreateWithData(objc_retain((id)pb), base, bpr * h, cg_free);
    void *cs = CGColorSpaceCreateDeviceRGB();
    void *img = CGImageCreate(w, h, 8, 32, bpr, cs, 6 | 0x2000, prov, 0, 1, 0);   /* BGRA */
    CGColorSpaceRelease(cs);
    CGDataProviderRelease(prov);
    PHOTO(s)->cg = img;
    return img;
}
static void photo_dealloc(id s, SEL c) {
    avphoto_t *p = PHOTO(s);
    if (p->cg) CGImageRelease(p->cg);
    if (p->pb) objc_release((id)p->pb);
    if (p->resolved) objc_release(p->resolved);
    if (p->settings) objc_release(p->settings);
    if (p->jpeg) objc_release(p->jpeg);
    IMP d = class_getMethodImplementation(cx_class_named("NSObject"), c);
    if (d) ((void (*)(id, SEL))d)(s, c);
}

typedef struct { id out, del, photo, rs; } photojob_t;
static void photo_send(void *a) {
    photojob_t *j = a;
    struct { const char *sel; int kind; } steps[] = {
        {"captureOutput:willBeginCaptureForResolvedSettings:", 0}, {"captureOutput:willCapturePhotoForResolvedSettings:", 0},
        {"captureOutput:didCapturePhotoForResolvedSettings:", 0}, {"captureOutput:didFinishProcessingPhoto:error:", 1},
        {"captureOutput:didFinishCaptureForResolvedSettings:error:", 2},
    };
    for (unsigned i = 0; i < sizeof steps / sizeof steps[0]; i++) {
        SEL sel = sel_registerName(steps[i].sel);
        if (!cx_responds(object_getClass(j->del), sel)) continue;
        if (steps[i].kind == 1) { if (j->photo) MSG(void, j->del, steps[i].sel, j->out, j->photo, (id)0); }
        else if (steps[i].kind == 2) MSG(void, j->del, steps[i].sel, j->out, j->rs, j->photo ? (id)0 : av_error(-11800));
        else MSG(void, j->del, steps[i].sel, j->out, j->rs);
    }
    objc_release(j->del); objc_release(j->out);
    if (j->photo) objc_release(j->photo);
    objc_release(j->rs);
    free(j);
}
static void photo_fulfil(avsess_t *ss, frame_t *f, cmt_t pts) {
    for (int i = 0; i < ss->n; i++) {
        id o = ss->items[i].obj;
        if (ss->items[i].is_input || object_getClass(o) != &AVCapturePhotoOutput_c) continue;
        avpho_t *p = (avpho_t *)o;
        if (!p->nreq) continue;
        spin_lock(&p->lock);
        photoreq_t req[8];
        int n = p->nreq;
        memcpy(req, p->req, sizeof req);
        p->nreq = 0;
        spin_unlock(&p->lock);
        for (int k = 0; k < n; k++) {
            u32 fmt = fmt_from_settings(((avpset_t *)req[k].settings)->format);
            if (!fmt || fmt == AVC_FMT_2VUY || fmt == AVC_FMT_YUVS || fmt == AVC_FMT_Y420) fmt = AVC_FMT_BGRA;
            frame_bgra(f);
            avrset_t *rs = (avrset_t *)cx_alloc_instance(&AVCaptureResolvedPhotoSettings_c);
            avphoto_t *ph = (avphoto_t *)cx_alloc_instance(&AVCapturePhoto_c);
            if (rs) { rs->uid = ((avpset_t *)req[k].settings)->uid; rs->w = f->w; rs->h = f->h; }
            if (ph) {
                ph->pb = make_pb(f, fmt, CONN(out_conn_lazy(o))->mirrored);
                ph->resolved = rs ? objc_retain((id)rs) : 0;
                ph->settings = objc_retain(req[k].settings);
                ph->ts = pts; ph->w = f->w; ph->h = f->h;
                if (!ph->pb) { objc_release((id)ph); ph = 0; }
            }
            objc_release(req[k].settings);
            photojob_t *j = calloc(1, sizeof *j);
            if (!j || !rs) { free(j); if (ph) objc_release((id)ph); if (rs) objc_release((id)rs); objc_release(req[k].delegate); continue; }
            j->out = objc_retain(o); j->del = req[k].delegate; j->photo = (id)ph; j->rs = (id)rs;
            dispatch_async_f(dispatch_get_main_queue(), j, photo_send);
        }
    }
}

/* AVCaptureStillImageOutput (antigo, ainda usado): o bloco recebe um CMSampleBuffer com o quadro em BGRA */
static id still_settings(id s, SEL c) { (void)c; avstill_t *p = (avstill_t *)s; if (!p->settings) { p->settings = objc_retain(dict_new()); dset(p->settings, "AVVideoCodecKey", cx_str("jpeg")); } return p->settings; }
static void still_set_settings(id s, SEL c, id st) { (void)c; if (st) objc_retain(st); if (((avstill_t *)s)->settings) objc_release(((avstill_t *)s)->settings); ((avstill_t *)s)->settings = st; }
static void still_capture(id s, SEL c, id conn, void *blk) {
    (void)c; (void)conn;
    avstill_t *p = (avstill_t *)s;
    if (!blk) return;
    spin_lock(&p->lock);
    if (p->nblk < 8) p->blk[p->nblk++] = _Block_copy(blk);
    spin_unlock(&p->lock);
}
static BOOL still_busy(id s, SEL c) { (void)c; return ((avstill_t *)s)->nblk ? YES : NO; }
typedef struct { void *blk; id sb; } stilljob_t;
static void still_send(void *a) {
    stilljob_t *j = a;
    void *inv = *(void **)((u8 *)j->blk + 16);
    ((void (*)(void *, id, id))inv)(j->blk, j->sb, j->sb ? (id)0 : av_error(-11800));
    _Block_release(j->blk);
    if (j->sb) objc_release(j->sb);
    free(j);
}
static void still_fulfil(avsess_t *ss, frame_t *f, cmt_t pts) {
    for (int i = 0; i < ss->n; i++) {
        id o = ss->items[i].obj;
        if (ss->items[i].is_input || object_getClass(o) != &AVCaptureStillImageOutput_c) continue;
        avstill_t *p = (avstill_t *)o;
        if (!p->nblk) continue;
        spin_lock(&p->lock);
        void *blk[8];
        int n = p->nblk;
        memcpy(blk, p->blk, sizeof blk);
        p->nblk = 0;
        spin_unlock(&p->lock);
        frame_bgra(f);
        for (int k = 0; k < n; k++) {
            void *pb = make_pb(f, AVC_FMT_BGRA, CONN(out_conn_lazy(o))->mirrored);
            void *sb = pb ? make_sb(pb, pts, cmt(1, 30)) : 0;
            if (pb) objc_release((id)pb);
            stilljob_t *j = calloc(1, sizeof *j);
            if (!j) { _Block_release(blk[k]); if (sb) objc_release((id)sb); continue; }
            j->blk = blk[k]; j->sb = (id)sb;
            dispatch_async_f(dispatch_get_main_queue(), j, still_send);
        }
    }
}
static id still_jpeg(id cls, SEL c, void *sb) {
    (void)cls; (void)c;
    return sb ? jpeg_of_pb(CMSampleBufferGetImageBuffer(sb)) : 0;
}
static id still_codecs(id s, SEL c) { return pho_codecs(s, c); }
static id still_pixfmts(id s, SEL c) { return pho_pixfmts(s, c); }

/* ================================================================ gravação em arquivo */
/* AVCaptureMovieFileOutput: H.264 (VTCompressionSession do avwriter.c) + LPCM 16 bits em QuickTime; AVCaptureAudioFileOutput:
 * só o áudio, no tipo de arquivo pedido (MOV, WAV). O AVAssetWriter é montado no primeiro quadro (o tamanho vem da câmera);
 * a sessão do escritor começa no pts desse quadro e a pausa desconta o tempo parado. */
enum { REC_IDLE = 0, REC_PENDING = 1, REC_ON = 2, REC_STOP = 3 };
typedef struct {
    cx_class_t *isa;
    id conn;
    id url, rdelegate, fdelegate, ftype, vsettings;
    id writer, vin, ain, adaptor;
    volatile int state, pause_req, resume_req;
    int paused, audio_only;
    double pause_at, offset, first, last, max_dur, next_check, min_free;
    i64 max_size, size;
    cmt_t frag;
} avmfo_t;
cx_class_t AVCaptureFileOutput_c __asm__("OBJC_CLASS_$_AVCaptureFileOutput"), AVCaptureFileOutput_m __asm__("OBJC_METACLASS_$_AVCaptureFileOutput");
cx_class_t AVCaptureMovieFileOutput_c __asm__("OBJC_CLASS_$_AVCaptureMovieFileOutput"), AVCaptureMovieFileOutput_m __asm__("OBJC_METACLASS_$_AVCaptureMovieFileOutput");
cx_class_t AVCaptureAudioFileOutput_c __asm__("OBJC_CLASS_$_AVCaptureAudioFileOutput"), AVCaptureAudioFileOutput_m __asm__("OBJC_METACLASS_$_AVCaptureAudioFileOutput");
#define MFO(o) ((avmfo_t *)(o))
const char *cx_audio_url_path(id url);
int fseek(FILE_ *f, long o, int w);
long ftell(FILE_ *f);
static int is_file_output(id o) { cx_class_t *k = object_getClass(o); return k == &AVCaptureMovieFileOutput_c || k == &AVCaptureAudioFileOutput_c; }
static double secs(cmt_t t) { return t.timescale > 0 && (t.flags & 1) ? (double)t.value / t.timescale : 0; }
static cmt_t from_secs(double s) { return cmt((long long)(s * 1000000000.0 + 0.5), 1000000000); }
static i64 file_size(const char *path) {
    FILE_ *f = path ? fopen(path, "rb") : 0;
    if (!f) return -1;
    fseek(f, 0, 2 /* SEEK_END */);
    long n = ftell(f);
    fclose(f);
    return n;
}
static id rec_connections(id o) { id a = arr_new(); MSG(void, a, "addObject:", out_conn_lazy(o)); return a; }
static id rec_error(long code, int finished) {
    id info = dict_new();
    id yes = MSG(id, (id)cx_class_named("NSNumber"), "numberWithBool:", (BOOL)(finished ? YES : NO));
    MSG(void, info, "setObject:forKey:", yes, cx_str("AVErrorRecordingSuccessfullyFinishedKey"));
    return MSG(id, (id)cx_class_named("NSError"), "errorWithDomain:code:userInfo:", cx_str("AVFoundationErrorDomain"), code, info);
}
typedef struct { id out, del, url, conns, err; int kind; } recjob_t;   /* kind 0 começou, 1 terminou, 2 pausou, 3 voltou */
static void rec_send(void *a) {
    recjob_t *j = a;
    static const char *sel[4] = {
        "captureOutput:didStartRecordingToOutputFileAtURL:fromConnections:",
        "captureOutput:didFinishRecordingToOutputFileAtURL:fromConnections:error:",
        "captureOutput:didPauseRecordingToOutputFileAtURL:fromConnections:",
        "captureOutput:didResumeRecordingToOutputFileAtURL:fromConnections:",
    };
    if (j->del && cx_responds(object_getClass(j->del), sel_registerName(sel[j->kind]))) {
        if (j->kind == 1) MSG(void, j->del, sel[1], j->out, j->url, j->conns, j->err);
        else MSG(void, j->del, sel[j->kind], j->out, j->url, j->conns);
    }
    objc_release(j->out); objc_release(j->del); objc_release(j->url); objc_release(j->conns);
    if (j->err) objc_release(j->err);
    free(j);
}
static void rec_notify(id o, int kind, id err) {
    avmfo_t *m = MFO(o);
    recjob_t *j = calloc(1, sizeof *j);
    if (!j || !m->rdelegate) { free(j); return; }
    j->out = objc_retain(o); j->del = objc_retain(m->rdelegate); j->url = objc_retain(m->url);
    j->conns = objc_retain(rec_connections(o)); j->err = err ? objc_retain(err) : 0; j->kind = kind;
    dispatch_async_f(dispatch_get_main_queue(), j, rec_send);
}
static void rec_release(avmfo_t *m) {
    if (m->adaptor) objc_release(m->adaptor);
    if (m->vin) objc_release(m->vin);
    if (m->ain) objc_release(m->ain);
    if (m->writer) objc_release(m->writer);
    m->adaptor = m->vin = m->ain = m->writer = 0;
}
/* fecha o arquivo; code != 0: parou por limite (o arquivo fica bom) ou falhou antes de gravar */
static void rec_finish(id o, long code) {
    avmfo_t *m = MFO(o);
    id err = 0;
    if (m->writer) {
        if (m->vin) MSG0(void, m->vin, "markAsFinished");
        if (m->ain) MSG0(void, m->ain, "markAsFinished");
        BOOL ok = MSG0(BOOL, m->writer, "finishWriting");
        if (!ok) { id we = MSG0(id, m->writer, "error"); err = we ? we : rec_error(-11800, 0); }
        else if (code) err = rec_error(code, 1);
        m->size = file_size(cx_audio_url_path(m->url));
    } else err = rec_error(code ? code : -11805 /* AVErrorNoDataCaptured */, 0);
    rec_release(m);
    m->state = REC_IDLE;
    m->paused = 0;
    rec_notify(o, 1, err);
}
static id lpcm_settings(void) {
    id d = dict_new();
    dset(d, "AVFormatIDKey", cx_num_i(0x6c70636d /* 'lpcm' */));
    dset(d, "AVSampleRateKey", cx_num_i(48000));
    dset(d, "AVNumberOfChannelsKey", cx_num_i(2));
    dset(d, "AVLinearPCMBitDepthKey", cx_num_i(16));
    dset(d, "AVLinearPCMIsFloatKey", cx_num_i(0));
    dset(d, "AVLinearPCMIsBigEndianKey", cx_num_i(0));
    dset(d, "AVLinearPCMIsNonInterleaved", cx_num_i(0));
    return d;
}
/* monta o AVAssetWriter; w/h = 0 no AVCaptureAudioFileOutput */
static int rec_setup(id o, avsess_t *ss, int w, int h, double start) {
    avmfo_t *m = MFO(o);
    const char *ft = m->ftype ? cx_cstr(m->ftype) : "com.apple.quicktime-movie";
    id err = 0;
    id wr = MSG(id, (id)cx_class_named("AVAssetWriter"), "assetWriterWithURL:fileType:error:", m->url, cx_str(ft), &err);
    if (!wr) return -11800;
    m->writer = objc_retain(wr);
    if (!m->audio_only) {
        if (w < 16 || h < 16 || (w & 1) || (h & 1)) return -11800;
        id vs = dict_new();
        dset(vs, "AVVideoCodecKey", cx_str("avc1"));
        dset(vs, "Width", cx_num_i(w));
        dset(vs, "Height", cx_num_i(h));
        id props = m->vsettings ? MSG(id, m->vsettings, "objectForKey:", cx_str("AVVideoCompressionPropertiesKey")) : 0;
        if (props) dset(vs, "AVVideoCompressionPropertiesKey", props);
        id vin = MSG(id, (id)cx_class_named("AVAssetWriterInput"), "assetWriterInputWithMediaType:outputSettings:", cx_str("vide"), vs);
        if (!vin || !MSG(BOOL, m->writer, "canAddInput:", vin)) return -11800;
        MSG(void, vin, "setExpectsMediaDataInRealTime:", (BOOL)YES);
        MSG(void, m->writer, "addInput:", vin);
        m->vin = objc_retain(vin);
        id ad = MSG(id, (id)cx_class_named("AVAssetWriterInputPixelBufferAdaptor"),
                    "assetWriterInputPixelBufferAdaptorWithAssetWriterInput:sourcePixelBufferAttributes:", vin, (id)0);
        if (!ad) return -11800;
        m->adaptor = objc_retain(ad);
    }
    if (ss->mic) {
        id ain = MSG(id, (id)cx_class_named("AVAssetWriterInput"), "assetWriterInputWithMediaType:outputSettings:", cx_str("soun"), lpcm_settings());
        if (ain && MSG(BOOL, m->writer, "canAddInput:", ain)) {
            MSG(void, ain, "setExpectsMediaDataInRealTime:", (BOOL)YES);
            MSG(void, m->writer, "addInput:", ain);
            m->ain = objc_retain(ain);
        }
    }
    if (m->audio_only && !m->ain) return -11800;
    if (!MSG0(BOOL, m->writer, "startWriting")) return -11800;
    ((void (*)(id, SEL, cmt_t))objc_msgSend)(m->writer, sel_registerName("startSessionAtSourceTime:"), from_secs(start));
    m->first = m->last = start;
    m->offset = 0;
    m->next_check = start + 1;
    return 0;
}
/* pedidos de pausa/volta e limites; devolve 1 se a gravação está ativa e não pausada */
static int rec_tick(id o, double now) {
    avmfo_t *m = MFO(o);
    if (m->state == REC_STOP) { rec_finish(o, 0); return 0; }
    if (m->state != REC_ON) return 0;
    if (m->pause_req) { m->pause_req = 0; if (!m->paused) { m->paused = 1; m->pause_at = now; rec_notify(o, 2, 0); } }
    if (m->resume_req) { m->resume_req = 0; if (m->paused) { m->paused = 0; m->offset += now - m->pause_at; rec_notify(o, 3, 0); } }
    if (m->paused) return 0;
    if (m->max_dur > 0 && now - m->offset - m->first >= m->max_dur) { rec_finish(o, -11810 /* AVErrorMaximumDurationReached */); return 0; }
    if (now >= m->next_check) {
        m->next_check = now + 1;
        i64 n = file_size(cx_audio_url_path(m->url));
        if (n >= 0) m->size = n;
        if (m->max_size > 0 && m->size >= m->max_size) { rec_finish(o, -11811 /* AVErrorMaximumFileSizeReached */); return 0; }
    }
    return 1;
}
static void rec_video(avsess_t *ss, id o, frame_t *f, cmt_t pts) {
    avmfo_t *m = MFO(o);
    double now = secs(pts);
    if (m->audio_only) { rec_tick(o, now); return; }
    if (m->state == REC_PENDING) {
        int rc = rec_setup(o, ss, f->w, f->h, now);
        if (rc) { rec_release(m); m->state = REC_IDLE; rec_notify(o, 1, rec_error(rc, 0)); return; }
        m->state = REC_ON;
        rec_notify(o, 0, 0);
    }
    if (!rec_tick(o, now) || !CONN(out_conn_lazy(o))->enabled) return;
    void *pb = make_pb(f, AVC_FMT_420V, CONN(out_conn_lazy(o))->mirrored);
    if (!pb) return;
    double rel = now - m->offset;
    if (MSG0(BOOL, m->vin, "isReadyForMoreMediaData"))
        ((BOOL (*)(id, SEL, void *, cmt_t))objc_msgSend)(m->adaptor, sel_registerName("appendPixelBuffer:withPresentationTime:"), pb, from_secs(rel));
    objc_release((id)pb);
    m->last = rel;
}
int CMSampleBufferCreateCopyWithNewTiming(void *a, void *src, long nt, const void *t, void **out);
static void rec_audio(avsess_t *ss, id o, void *sample, double at, unsigned frames) {
    avmfo_t *m = MFO(o);
    if (m->audio_only && m->state == REC_PENDING) {
        int rc = rec_setup(o, ss, 0, 0, at);
        if (rc) { rec_release(m); m->state = REC_IDLE; rec_notify(o, 1, rec_error(rc, 0)); return; }
        m->state = REC_ON;
        rec_notify(o, 0, 0);
    }
    if (m->audio_only && !rec_tick(o, at)) return;
    if (m->state != REC_ON || m->paused || !m->ain) return;
    double rel = at - m->offset;
    if (rel < m->first) return;   /* antes do primeiro quadro: o arquivo começa no vídeo */
    cmtiming_t t = {cmt(frames, 48000), from_secs(rel), {0, 0, 0, 0}};
    void *sb = 0;
    if (CMSampleBufferCreateCopyWithNewTiming(0, sample, 1, &t, &sb) || !sb) return;
    if (MSG0(BOOL, m->ain, "isReadyForMoreMediaData")) MSG(BOOL, m->ain, "appendSampleBuffer:", (id)sb);
    objc_release((id)sb);
    if (m->audio_only) m->last = rel;
}
/* a sessão parou: fecha o que estava gravando */
static void rec_session_stopped(avsess_t *ss) {
    for (int i = 0; i < ss->n; i++) {
        id o = ss->items[i].obj;
        if (ss->items[i].is_input || !is_file_output(o)) continue;
        if (MFO(o)->state != REC_IDLE) rec_finish(o, MFO(o)->state == REC_PENDING ? -11805 : 0);
    }
}

static void mfo_start(id s, SEL c, id url, id delegate) {
    (void)c;
    avmfo_t *m = MFO(s);
    if (m->state != REC_IDLE || !url) return;
    if (m->url) objc_release(m->url);
    if (m->rdelegate) objc_release(m->rdelegate);
    m->url = objc_retain(url);
    m->rdelegate = delegate ? objc_retain(delegate) : 0;
    m->size = 0; m->paused = 0; m->pause_req = m->resume_req = 0;
    m->audio_only = object_getClass(s) == &AVCaptureAudioFileOutput_c;
    if (file_size(cx_audio_url_path(url)) >= 0) { rec_notify(s, 1, rec_error(-11823 /* AVErrorFileAlreadyExists */, 0)); return; }
    m->state = REC_PENDING;
}
static void afo_start(id s, SEL c, id url, id type, id delegate) {
    if (type) { if (MFO(s)->ftype) objc_release(MFO(s)->ftype); MFO(s)->ftype = objc_retain(type); }
    mfo_start(s, c, url, delegate);
}
static void mfo_stop(id s, SEL c) {
    (void)c;
    avmfo_t *m = MFO(s);
    if (m->state == REC_PENDING) { rec_finish(s, -11805); return; }
    if (m->state == REC_ON) m->state = REC_STOP;   /* a thread da sessão fecha no próximo passo */
}
static BOOL mfo_recording(id s, SEL c) { (void)c; return MFO(s)->state != REC_IDLE ? YES : NO; }
static BOOL mfo_paused(id s, SEL c) { (void)c; return MFO(s)->paused ? YES : NO; }
static void mfo_pause(id s, SEL c) { (void)c; if (MFO(s)->state == REC_ON) MFO(s)->pause_req = 1; }
static void mfo_resume(id s, SEL c) { (void)c; if (MFO(s)->state == REC_ON) MFO(s)->resume_req = 1; }
static id mfo_url(id s, SEL c) { (void)c; return MFO(s)->state != REC_IDLE ? MFO(s)->url : 0; }
static cmt_t mfo_duration(id s, SEL c) { (void)c; avmfo_t *m = MFO(s); return from_secs(m->state == REC_ON ? m->last - m->first : 0); }
static i64 mfo_size(id s, SEL c) { (void)c; return MFO(s)->size; }
static cmt_t mfo_max_dur(id s, SEL c) { (void)c; return MFO(s)->max_dur > 0 ? from_secs(MFO(s)->max_dur) : cmt(0, 0); }
static void mfo_set_max_dur(id s, SEL c, cmt_t t) { (void)c; MFO(s)->max_dur = secs(t); }
static i64 mfo_max_size(id s, SEL c) { (void)c; return MFO(s)->max_size; }
static void mfo_set_max_size(id s, SEL c, i64 v) { (void)c; MFO(s)->max_size = v; }
static i64 mfo_min_free(id s, SEL c) { (void)c; return (i64)MFO(s)->min_free; }
static void mfo_set_min_free(id s, SEL c, i64 v) { (void)c; MFO(s)->min_free = (double)v; }
static cmt_t mfo_frag(id s, SEL c) { (void)c; return MFO(s)->frag.flags ? MFO(s)->frag : cmt(10, 1); }
static void mfo_set_frag(id s, SEL c, cmt_t t) { (void)c; MFO(s)->frag = t; }   /* o QuickTime sai inteiro no fim: guardado só para leitura */
static id mfo_delegate(id s, SEL c) { (void)c; return MFO(s)->fdelegate; }
static void mfo_set_delegate(id s, SEL c, id d) { (void)c; MFO(s)->fdelegate = d; }
static id mfo_codecs(id s, SEL c) { (void)s; (void)c; id a = arr_new(); MSG(void, a, "addObject:", cx_str("avc1")); return a; }
static id mfo_settings_for(id s, SEL c, id conn) {
    (void)c; (void)conn;
    if (MFO(s)->vsettings) return MFO(s)->vsettings;
    id d = dict_new();
    dset(d, "AVVideoCodecKey", cx_str("avc1"));
    return d;
}
/* só H.264: outro codec é recusado (como um codec fora de availableVideoCodecTypes no macOS) */
static void mfo_set_settings(id s, SEL c, id st, id conn) {
    (void)c; (void)conn;
    id codec = st ? MSG(id, st, "objectForKey:", cx_str("AVVideoCodecKey")) : 0;
    if (codec && strcmp(cx_cstr(codec), "avc1")) return;
    if (st) objc_retain(st);
    if (MFO(s)->vsettings) objc_release(MFO(s)->vsettings);
    MFO(s)->vsettings = st;
}
static id afo_types(id s, SEL c) {
    (void)s; (void)c;
    id a = arr_new();
    MSG(void, a, "addObject:", cx_str("com.apple.quicktime-movie"));
    MSG(void, a, "addObject:", cx_str("com.microsoft.waveform-audio"));
    return a;
}
static id afo_settings(id s, SEL c) { (void)s; (void)c; return lpcm_settings(); }

/* ================================================================ prévia */
typedef struct { cx_class_t *isa; id session; id timer; id layer; u64 seq; int w, h; id conn; } pvst_t;
cx_class_t MOSCapturePreviewState_c __asm__("OBJC_CLASS_$_MOSCapturePreviewState"), MOSCapturePreviewState_m __asm__("OBJC_METACLASS_$_MOSCapturePreviewState");
cx_class_t AVCaptureVideoPreviewLayer_c __asm__("OBJC_CLASS_$_AVCaptureVideoPreviewLayer"), AVCaptureVideoPreviewLayer_m __asm__("OBJC_METACLASS_$_AVCaptureVideoPreviewLayer");
static const char k_pv = 0, k_pvgrav = 0;
static pvst_t *pv_state(id layer, int create) {
    pvst_t *st = (pvst_t *)objc_getAssociatedObject(layer, &k_pv);
    if (!st && create) {
        st = (pvst_t *)cx_alloc_instance(&MOSCapturePreviewState_c);
        st->layer = layer;
        st->conn = conn_make(layer);
        CONN(st->conn)->auto_mirror = 1;
        objc_setAssociatedObject(layer, &k_pv, (id)st, 1);
        objc_release((id)st);
    }
    return st;
}
static void pv_free(void *info, const void *data, size_t n) { (void)info; (void)n; free((void *)data); }
static void pv_tick(id s, SEL c, id timer) {
    (void)c; (void)timer;
    pvst_t *st = (pvst_t *)s;
    avsess_t *ss = st->session ? SESS(st->session) : 0;
    if (!ss || !st->layer || ss->pvseq == st->seq || !CONN(st->conn)->enabled) return;
    spin_lock(&ss->pvlock);
    int w = ss->pvw, h = ss->pvh;
    u8 *px = ss->pv ? malloc((size_t)w * h * 4) : 0;
    if (px) memcpy(px, ss->pv, (size_t)w * h * 4);
    st->seq = ss->pvseq;
    spin_unlock(&ss->pvlock);
    if (!px) return;
    if (CONN(st->conn)->mirrored) { u8 *pl[1] = {px}; size_t bpr[1] = {(size_t)w * 4}; avc_mirror(AVC_FMT_BGRA, w, h, pl, bpr); }
    void *prov = CGDataProviderCreateWithData(0, px, (size_t)w * h * 4, pv_free);
    void *cs = CGColorSpaceCreateDeviceRGB();
    void *img = CGImageCreate((size_t)w, (size_t)h, 8, 32, (size_t)w * 4, cs, 6 | 0x2000, prov, 0, 1, 0);
    CGColorSpaceRelease(cs);
    CGDataProviderRelease(prov);
    if (!img) return;
    MSG(void, st->layer, "setContents:", (id)img);
    CGImageRelease(img);
    st->w = w; st->h = h;
}
static void pv_set_session(pvst_t *st, id session) {
    if (session == st->session) return;
    if (st->session) { SESS(st->session)->previews--; objc_release(st->session); }
    st->session = session ? objc_retain(session) : 0;
    st->seq = 0;
    if (session) SESS(session)->previews++;
    if (session && !st->timer)
        st->timer = objc_retain(MSG(id, (id)cx_class_named("NSTimer"), "scheduledTimerWithTimeInterval:target:selector:userInfo:repeats:",
                                    1.0 / 30, (id)st, sel_registerName("tick:"), (id)0, (BOOL)YES));
    else if (!session && st->timer) { MSG0(void, st->timer, "invalidate"); objc_release(st->timer); st->timer = 0; }
}
static void pvst_dealloc(id s, SEL c) {
    pvst_t *st = (pvst_t *)s;
    if (st->timer) { MSG0(void, st->timer, "invalidate"); objc_release(st->timer); }
    if (st->session) { SESS(st->session)->previews--; objc_release(st->session); }
    if (st->conn) objc_release(st->conn);
    IMP d = class_getMethodImplementation(cx_class_named("NSObject"), c);
    if (d) ((void (*)(id, SEL))d)(s, c);
}
static id pl_init_session(id s, SEL c, id session) {
    (void)c;
    s = MSG0(id, s, "init");
    if (!s) return 0;
    MSG(void, s, "setContentsGravity:", cx_str("resizeAspect"));
    pv_set_session(pv_state(s, 1), session);
    return s;
}
static id pl_with_session(id k, SEL c, id session) {
    (void)k;
    if (!cx_class_named("AVCaptureVideoPreviewLayer")) return 0;
    id l = pl_init_session(MSG0(id, (id)&AVCaptureVideoPreviewLayer_c, "alloc"), c, session);
    return l ? objc_autorelease(l) : 0;
}
static id pl_session(id s, SEL c) { (void)c; pvst_t *st = pv_state(s, 0); return st ? st->session : 0; }
static void pl_set_session(id s, SEL c, id session) { (void)c; pv_set_session(pv_state(s, 1), session); }
static id pl_connection(id s, SEL c) { (void)c; return pv_state(s, 1)->conn; }
static BOOL pl_previewing(id s, SEL c) { (void)c; pvst_t *st = pv_state(s, 0); return st && st->session && SESS(st->session)->running ? YES : NO; }
static const char *grav_ca(const char *g) {
    if (g && !strcmp(g, "AVLayerVideoGravityResizeAspectFill")) return "resizeAspectFill";
    if (g && !strcmp(g, "AVLayerVideoGravityResize")) return "resize";
    return "resizeAspect";
}
static id pl_gravity(id s, SEL c) { (void)c; id g = objc_getAssociatedObject(s, &k_pvgrav); return g ? g : cx_str("AVLayerVideoGravityResizeAspect"); }
static void pl_set_gravity(id s, SEL c, id g) {
    (void)c;
    objc_setAssociatedObject(s, &k_pvgrav, g, 3);
    MSG(void, s, "setContentsGravity:", cx_str(grav_ca(g ? cx_cstr(g) : 0)));
}

/* ================================================================ registro */
static void add(const char *cls, const char *sel, void *fn, int meta) {
    cx_class_t *k = cx_class_named(cls);
    if (!k) return;
    if (meta) cx_add_cmethod(k, sel, fn); else cx_add_method(k, sel, fn);
}
#define DEF(k, n, sup, sz) if (!cx_class_named(n)) cx_define_class(&k##_c, &k##_m, n, sup, (u32)(sz))

/* antes da casca do vscode_fwclasses.c e do AVCaptureAudioDataOutput do slack_frameworks.c: tamanho certo das instâncias */
void cx_avcapture_classes_init(void) {
    cx_class_t *ob = cx_class_named("NSObject");
    DEF(AVCaptureDevice, "AVCaptureDevice", ob, sizeof(avdev_t) - 8);
    DEF(AVCaptureDeviceFormat, "AVCaptureDeviceFormat", ob, sizeof(avfmt_t) - 8);
    DEF(AVFrameRateRange, "AVFrameRateRange", ob, sizeof(avfrr_t) - 8);
    DEF(AVCaptureInput, "AVCaptureInput", ob, 0);
    DEF(AVCaptureDeviceInput, "AVCaptureDeviceInput", &AVCaptureInput_c, sizeof(avin_t) - 8);
    DEF(AVCaptureScreenInput, "AVCaptureScreenInput", &AVCaptureInput_c, sizeof(avscreen_t) - 8);
    DEF(AVCaptureConnection, "AVCaptureConnection", ob, sizeof(avconn_t) - 8);
    DEF(AVCaptureOutput, "AVCaptureOutput", ob, 0);
    DEF(AVCaptureVideoDataOutput, "AVCaptureVideoDataOutput", &AVCaptureOutput_c, sizeof(avvdo_t) - 8);
    DEF(AVCapturePhotoOutput, "AVCapturePhotoOutput", &AVCaptureOutput_c, sizeof(avpho_t) - 8);
    DEF(AVCaptureStillImageOutput, "AVCaptureStillImageOutput", &AVCaptureOutput_c, sizeof(avstill_t) - 8);
    DEF(AVCapturePhotoSettings, "AVCapturePhotoSettings", ob, sizeof(avpset_t) - 8);
    DEF(AVCaptureResolvedPhotoSettings, "AVCaptureResolvedPhotoSettings", ob, sizeof(avrset_t) - 8);
    DEF(AVCapturePhoto, "AVCapturePhoto", ob, sizeof(avphoto_t) - 8);
    DEF(AVCaptureSession, "AVCaptureSession", ob, sizeof(avsess_t) - 8);
    DEF(AVCaptureFileOutput, "AVCaptureFileOutput", &AVCaptureOutput_c, sizeof(avmfo_t) - 8);
    DEF(AVCaptureMovieFileOutput, "AVCaptureMovieFileOutput", &AVCaptureFileOutput_c, 0);
    DEF(AVCaptureAudioFileOutput, "AVCaptureAudioFileOutput", &AVCaptureFileOutput_c, 0);
    DEF(MOSCapturePreviewState, "MOSCapturePreviewState", ob, sizeof(pvst_t) - 8);
}

void cx_avcapture_init(void) {
    cx_avcapture_classes_init();
    cx_class_t *ca = cx_class_named("CALayer");
    if (ca) DEF(AVCaptureVideoPreviewLayer, "AVCaptureVideoPreviewLayer", ca, 0);
    cx_add_method(&AVCaptureScreenInput_c, "initWithDisplayID:", (void *)screen_init);

    add("AVCaptureDevice", "authorizationStatusForMediaType:", (void *)auth_status, 1);
    add("AVCaptureDevice", "requestAccessForMediaType:completionHandler:", (void *)request_access, 1);
    add("AVCaptureDevice", "devices", (void *)dev_devices, 1);
    add("AVCaptureDevice", "devicesWithMediaType:", (void *)dev_typed, 1);
    add("AVCaptureDevice", "defaultDeviceWithMediaType:", (void *)dev_default, 1);
    add("AVCaptureDevice", "defaultDeviceWithDeviceType:mediaType:position:", (void *)dev_default3, 1);
    add("AVCaptureDevice", "deviceWithUniqueID:", (void *)dev_with_uid, 1);
    add("AVCaptureDevice", "localizedName", (void *)dev_name, 0);
    add("AVCaptureDevice", "uniqueID", (void *)dev_uid, 0);
    add("AVCaptureDevice", "modelID", (void *)dev_model, 0);
    add("AVCaptureDevice", "manufacturer", (void *)dev_manufacturer, 0);
    add("AVCaptureDevice", "deviceType", (void *)dev_type, 0);
    add("AVCaptureDevice", "position", (void *)dev_position, 0);
    add("AVCaptureDevice", "transportType", (void *)dev_transport, 0);
    add("AVCaptureDevice", "isConnected", (void *)dev_connected, 0);
    add("AVCaptureDevice", "isSuspended", (void *)dev_no, 0);
    add("AVCaptureDevice", "isInUseByAnotherApplication", (void *)dev_no, 0);
    add("AVCaptureDevice", "hasFlash", (void *)dev_no, 0);
    add("AVCaptureDevice", "hasTorch", (void *)dev_no, 0);
    add("AVCaptureDevice", "isContinuityCamera", (void *)dev_no, 0);
    add("AVCaptureDevice", "hasMediaType:", (void *)dev_has_media, 0);
    add("AVCaptureDevice", "supportsAVCaptureSessionPreset:", (void *)dev_supports_preset, 0);
    add("AVCaptureDevice", "lockForConfiguration:", (void *)dev_lock, 0);
    add("AVCaptureDevice", "unlockForConfiguration", (void *)dev_unlock, 0);
    add("AVCaptureDevice", "formats", (void *)dev_formats, 0);
    add("AVCaptureDevice", "activeFormat", (void *)dev_active_format, 0);
    add("AVCaptureDevice", "setActiveFormat:", (void *)dev_set_active_format, 0);
    add("AVCaptureDevice", "activeVideoMinFrameDuration", (void *)dev_min_dur, 0);
    add("AVCaptureDevice", "activeVideoMaxFrameDuration", (void *)dev_max_dur, 0);
    add("AVCaptureDevice", "setActiveVideoMinFrameDuration:", (void *)dev_set_min_dur, 0);
    add("AVCaptureDevice", "setActiveVideoMaxFrameDuration:", (void *)dev_set_max_dur, 0);
    add("AVCaptureDeviceDiscoverySession", "discoverySessionWithDeviceTypes:mediaType:position:", (void *)dev_discovery, 1);
    add("AVCaptureDeviceDiscoverySession", "devices", (void *)disc_devices, 0);

    add("AVCaptureDeviceFormat", "formatDescription", (void *)fmt_desc, 0);
    add("AVCaptureDeviceFormat", "mediaType", (void *)fmt_media, 0);
    add("AVCaptureDeviceFormat", "videoSupportedFrameRateRanges", (void *)fmt_ranges, 0);
    add("AVCaptureDeviceFormat", "highResolutionStillImageDimensions", (void *)fmt_still, 0);
    add("AVCaptureDeviceFormat", "videoMaxZoomFactor", (void *)fmt_zoom, 0);
    add("AVCaptureDeviceFormat", "videoFieldOfView", (void *)fmt_fov, 0);
    add("AVCaptureDeviceFormat", "mosFourCC", (void *)fmt_subtype, 0);
    add("AVFrameRateRange", "minFrameRate", (void *)frr_min, 0);
    add("AVFrameRateRange", "maxFrameRate", (void *)frr_max, 0);
    add("AVFrameRateRange", "minFrameDuration", (void *)frr_min_dur, 0);
    add("AVFrameRateRange", "maxFrameDuration", (void *)frr_max_dur, 0);

    add("AVCaptureDeviceInput", "deviceInputWithDevice:error:", (void *)input_with_device, 1);
    add("AVCaptureDeviceInput", "initWithDevice:error:", (void *)input_init_device, 0);
    add("AVCaptureDeviceInput", "device", (void *)input_device, 0);
    add("AVCaptureInput", "ports", (void *)input_ports, 0);

    add("AVCaptureConnection", "isEnabled", (void *)conn_enabled, 0);
    add("AVCaptureConnection", "setEnabled:", (void *)conn_set_enabled, 0);
    add("AVCaptureConnection", "isActive", (void *)conn_yes, 0);
    add("AVCaptureConnection", "isVideoMirroringSupported", (void *)conn_yes, 0);
    add("AVCaptureConnection", "isVideoMirrored", (void *)conn_mirrored, 0);
    add("AVCaptureConnection", "setVideoMirrored:", (void *)conn_set_mirrored, 0);
    add("AVCaptureConnection", "automaticallyAdjustsVideoMirroring", (void *)conn_auto, 0);
    add("AVCaptureConnection", "setAutomaticallyAdjustsVideoMirroring:", (void *)conn_set_auto, 0);
    add("AVCaptureConnection", "isVideoOrientationSupported", (void *)conn_no, 0);
    add("AVCaptureConnection", "videoOrientation", (void *)conn_orient, 0);
    add("AVCaptureConnection", "setVideoOrientation:", (void *)conn_set_orient, 0);
    add("AVCaptureConnection", "isVideoRotationAngleSupported:", (void *)conn_angle_ok, 0);
    add("AVCaptureConnection", "videoRotationAngle", (void *)conn_angle, 0);
    add("AVCaptureConnection", "isVideoMinFrameDurationSupported", (void *)conn_no, 0);
    add("AVCaptureConnection", "isVideoStabilizationSupported", (void *)conn_no, 0);
    add("AVCaptureConnection", "output", (void *)conn_output, 0);
    add("AVCaptureConnection", "inputPorts", (void *)conn_ports, 0);
    add("AVCaptureConnection", "audioChannels", (void *)conn_ports, 0);

    add("AVCaptureOutput", "connections", (void *)out_connections, 0);
    add("AVCaptureOutput", "connectionWithMediaType:", (void *)out_conn_for, 0);
    add("AVCaptureVideoDataOutput", "init", (void *)vdo_init, 0);
    add("AVCaptureVideoDataOutput", "setSampleBufferDelegate:queue:", (void *)vdo_set_delegate, 0);
    add("AVCaptureVideoDataOutput", "sampleBufferDelegate", (void *)vdo_delegate, 0);
    add("AVCaptureVideoDataOutput", "sampleBufferCallbackQueue", (void *)vdo_queue, 0);
    add("AVCaptureVideoDataOutput", "videoSettings", (void *)vdo_settings, 0);
    add("AVCaptureVideoDataOutput", "setVideoSettings:", (void *)vdo_set_settings, 0);
    add("AVCaptureVideoDataOutput", "alwaysDiscardsLateVideoFrames", (void *)vdo_discards, 0);
    add("AVCaptureVideoDataOutput", "setAlwaysDiscardsLateVideoFrames:", (void *)vdo_set_discards, 0);
    add("AVCaptureVideoDataOutput", "availableVideoCVPixelFormatTypes", (void *)vdo_formats, 0);
    add("AVCaptureVideoDataOutput", "availableVideoCodecTypes", (void *)vdo_codecs, 0);

    add("AVCapturePhotoSettings", "photoSettings", (void *)pset_new, 1);
    add("AVCapturePhotoSettings", "photoSettingsWithFormat:", (void *)pset_with_format, 1);
    add("AVCapturePhotoSettings", "photoSettingsFromPhotoSettings:", (void *)pset_from, 1);
    add("AVCapturePhotoSettings", "uniqueID", (void *)pset_uid, 0);
    add("AVCapturePhotoSettings", "format", (void *)pset_format, 0);
    add("AVCapturePhotoSettings", "flashMode", (void *)pset_flash, 0);
    add("AVCapturePhotoSettings", "setFlashMode:", (void *)pset_set_flash, 0);
    add("AVCaptureResolvedPhotoSettings", "uniqueID", (void *)rset_uid, 0);
    add("AVCaptureResolvedPhotoSettings", "photoDimensions", (void *)rset_dim, 0);
    add("AVCaptureResolvedPhotoSettings", "expectedPhotoCount", (void *)rset_count, 0);
    add("AVCapturePhotoOutput", "init", (void *)pho_init, 0);
    add("AVCapturePhotoOutput", "capturePhotoWithSettings:delegate:", (void *)pho_capture, 0);
    add("AVCapturePhotoOutput", "availablePhotoCodecTypes", (void *)pho_codecs, 0);
    add("AVCapturePhotoOutput", "availablePhotoPixelFormatTypes", (void *)pho_pixfmts, 0);
    add("AVCapturePhotoOutput", "availablePhotoFileTypes", (void *)pho_filetypes, 0);
    add("AVCapturePhotoOutput", "isHighResolutionCaptureEnabled", (void *)pho_hires, 0);
    add("AVCapturePhotoOutput", "setHighResolutionCaptureEnabled:", (void *)pho_set_hires, 0);
    add("AVCapturePhoto", "dealloc", (void *)photo_dealloc, 0);
    add("AVCapturePhoto", "pixelBuffer", (void *)photo_pb, 0);
    add("AVCapturePhoto", "resolvedSettings", (void *)photo_resolved, 0);
    add("AVCapturePhoto", "timestamp", (void *)photo_ts, 0);
    add("AVCapturePhoto", "photoCount", (void *)photo_count, 0);
    add("AVCapturePhoto", "isRawPhoto", (void *)photo_raw, 0);
    add("AVCapturePhoto", "metadata", (void *)photo_metadata, 0);
    add("AVCapturePhoto", "fileDataRepresentation", (void *)photo_data, 0);
    add("AVCapturePhoto", "CGImageRepresentation", (void *)photo_cgimage, 0);
    add("AVCaptureStillImageOutput", "outputSettings", (void *)still_settings, 0);
    add("AVCaptureStillImageOutput", "setOutputSettings:", (void *)still_set_settings, 0);
    add("AVCaptureStillImageOutput", "captureStillImageAsynchronouslyFromConnection:completionHandler:", (void *)still_capture, 0);
    add("AVCaptureStillImageOutput", "isCapturingStillImage", (void *)still_busy, 0);
    add("AVCaptureStillImageOutput", "availableImageDataCodecTypes", (void *)still_codecs, 0);
    add("AVCaptureStillImageOutput", "availableImageDataCVPixelFormatTypes", (void *)still_pixfmts, 0);
    add("AVCaptureStillImageOutput", "jpegStillImageNSDataRepresentation:", (void *)still_jpeg, 1);

    add("AVCaptureFileOutput", "startRecordingToOutputFileURL:recordingDelegate:", (void *)mfo_start, 0);
    add("AVCaptureAudioFileOutput", "startRecordingToOutputFileURL:outputFileType:recordingDelegate:", (void *)afo_start, 0);
    add("AVCaptureFileOutput", "stopRecording", (void *)mfo_stop, 0);
    add("AVCaptureFileOutput", "isRecording", (void *)mfo_recording, 0);
    add("AVCaptureFileOutput", "isRecordingPaused", (void *)mfo_paused, 0);
    add("AVCaptureFileOutput", "pauseRecording", (void *)mfo_pause, 0);
    add("AVCaptureFileOutput", "resumeRecording", (void *)mfo_resume, 0);
    add("AVCaptureFileOutput", "outputFileURL", (void *)mfo_url, 0);
    add("AVCaptureFileOutput", "recordedDuration", (void *)mfo_duration, 0);
    add("AVCaptureFileOutput", "recordedFileSize", (void *)mfo_size, 0);
    add("AVCaptureFileOutput", "maxRecordedDuration", (void *)mfo_max_dur, 0);
    add("AVCaptureFileOutput", "setMaxRecordedDuration:", (void *)mfo_set_max_dur, 0);
    add("AVCaptureFileOutput", "maxRecordedFileSize", (void *)mfo_max_size, 0);
    add("AVCaptureFileOutput", "setMaxRecordedFileSize:", (void *)mfo_set_max_size, 0);
    add("AVCaptureFileOutput", "minFreeDiskSpaceLimit", (void *)mfo_min_free, 0);
    add("AVCaptureFileOutput", "setMinFreeDiskSpaceLimit:", (void *)mfo_set_min_free, 0);
    add("AVCaptureFileOutput", "delegate", (void *)mfo_delegate, 0);
    add("AVCaptureFileOutput", "setDelegate:", (void *)mfo_set_delegate, 0);
    add("AVCaptureMovieFileOutput", "movieFragmentInterval", (void *)mfo_frag, 0);
    add("AVCaptureMovieFileOutput", "setMovieFragmentInterval:", (void *)mfo_set_frag, 0);
    add("AVCaptureMovieFileOutput", "availableVideoCodecTypes", (void *)mfo_codecs, 0);
    add("AVCaptureMovieFileOutput", "outputSettingsForConnection:", (void *)mfo_settings_for, 0);
    add("AVCaptureMovieFileOutput", "setOutputSettings:forConnection:", (void *)mfo_set_settings, 0);
    add("AVCaptureAudioFileOutput", "availableOutputFileTypes", (void *)afo_types, 1);
    add("AVCaptureAudioFileOutput", "audioSettings", (void *)afo_settings, 0);

    add("AVCaptureSession", "init", (void *)sess_init, 0);
    add("AVCaptureSession", "dealloc", (void *)sess_dealloc, 0);
    add("AVCaptureSession", "startRunning", (void *)sess_start, 0);
    add("AVCaptureSession", "stopRunning", (void *)sess_stop, 0);
    add("AVCaptureSession", "isRunning", (void *)sess_running, 0);
    add("AVCaptureSession", "isInterrupted", (void *)sess_no, 0);
    add("AVCaptureSession", "canAddInput:", (void *)sess_can_add, 0);
    add("AVCaptureSession", "addInput:", (void *)sess_add, 0);
    add("AVCaptureSession", "addInputWithNoConnections:", (void *)sess_add, 0);
    add("AVCaptureSession", "removeInput:", (void *)sess_remove, 0);
    add("AVCaptureSession", "canAddOutput:", (void *)sess_can_add, 0);
    add("AVCaptureSession", "addOutput:", (void *)sess_add, 0);
    add("AVCaptureSession", "addOutputWithNoConnections:", (void *)sess_add, 0);
    add("AVCaptureSession", "removeOutput:", (void *)sess_remove, 0);
    add("AVCaptureSession", "inputs", (void *)sess_inputs, 0);
    add("AVCaptureSession", "outputs", (void *)sess_outputs, 0);
    add("AVCaptureSession", "connections", (void *)sess_connections, 0);
    add("AVCaptureSession", "beginConfiguration", (void *)sess_cfg, 0);
    add("AVCaptureSession", "commitConfiguration", (void *)sess_cfg, 0);
    add("AVCaptureSession", "sessionPreset", (void *)sess_preset, 0);
    add("AVCaptureSession", "setSessionPreset:", (void *)sess_set_preset, 0);
    add("AVCaptureSession", "canSetSessionPreset:", (void *)sess_can_preset, 0);
    add("AVCaptureSession", "mosCaptureError", (void *)sess_err, 0);

    add("MOSCapturePreviewState", "tick:", (void *)pv_tick, 0);
    add("MOSCapturePreviewState", "dealloc", (void *)pvst_dealloc, 0);
    add("AVCaptureVideoPreviewLayer", "layerWithSession:", (void *)pl_with_session, 1);
    add("AVCaptureVideoPreviewLayer", "layerWithSessionWithNoConnection:", (void *)pl_with_session, 1);
    add("AVCaptureVideoPreviewLayer", "initWithSession:", (void *)pl_init_session, 0);
    add("AVCaptureVideoPreviewLayer", "initWithSessionWithNoConnection:", (void *)pl_init_session, 0);
    add("AVCaptureVideoPreviewLayer", "session", (void *)pl_session, 0);
    add("AVCaptureVideoPreviewLayer", "setSession:", (void *)pl_set_session, 0);
    add("AVCaptureVideoPreviewLayer", "setSessionWithNoConnection:", (void *)pl_set_session, 0);
    add("AVCaptureVideoPreviewLayer", "connection", (void *)pl_connection, 0);
    add("AVCaptureVideoPreviewLayer", "isPreviewing", (void *)pl_previewing, 0);
    add("AVCaptureVideoPreviewLayer", "videoGravity", (void *)pl_gravity, 0);
    add("AVCaptureVideoPreviewLayer", "setVideoGravity:", (void *)pl_set_gravity, 0);
}
