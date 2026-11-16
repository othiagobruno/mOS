/*
 * mOS libSystem — ScreenCaptureKit.
 *
 *  SCShareableContent: um SCDisplay com a tela composta (todos os monitores num espaço só), as janelas do compositor
 *    (DEV_WINDOW_LIST, da frente para o fundo; acima/abaixo de uma janela âncora de verdade) e os apps donos delas
 *    (nome, pid, bundleIdentifier do .app). Sem kTCCServiceScreenCapture a lista de janelas vem vazia (o display, não).
 *  SCContentFilter: display com janelas ou apps incluídos/excluídos (com exceções) ou uma janela independente do desktop.
 *    O kernel compõe a tela de novo só com o que o filtro pede (DEV_SCREEN_COMPOSE, kernel/compat/sccapture.c) ou copia a
 *    superfície da janela mesmo coberta (DEV_WINDOW_CAPTURE).
 *  SCStreamConfiguration: width/height da saída, scalesToFit, preservesAspectRatio, sourceRect, destinationRect,
 *    backgroundColor, showsCursor, pixelFormat ('BGRA', '420v', '420f', 'l10r'), minimumFrameInterval, queueDepth,
 *    capturesAudio (mistura do sistema, DEV_AUDIO_SYSTEM), captureMicrophone, sampleRate/channelCount e o resto guardado.
 *  SCStream: thread própria; o quadro só sai quando a imagem muda; cada saída recebe na fila dela, com no máximo queueDepth
 *    quadros pendentes; áudio em Float32 não intercalado; updateConfiguration/updateContentFilter com o stream rodando;
 *    stream:didStopWithError: quando a janela capturada fecha.
 *  SCScreenshotManager: CGImage ou CMSampleBuffer com o mesmo filtro e configuração; captureImageInRect:.
 * Código original do mOS (MIT).
 */
#include "foundation.h"
#include "sys.h"
#include "../../kernel/include/compat_gui.h"
#include "computeruse_win.h"
#include "sck_core.h"
#include "avcap_conv.h"

void *memset(void *d, int c, size_t n);
void *memcpy(void *d, const void *s, size_t n);
int strcmp(const char *a, const char *b);
void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void free(void *p);
id objc_retain(id);
void objc_release(id);
id objc_autorelease(id);
void *_Block_copy(const void *b);
void _Block_release(const void *b);
void *dispatch_get_global_queue(long prio, unsigned long flags);
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *));
int pthread_create(unsigned long *t, const void *attr, void *(*fn)(void *), void *arg);
int usleep(unsigned us);
void *objc_autoreleasePoolPush(void);
void objc_autoreleasePoolPop(void *p);
unsigned long long mach_absolute_time(void);
int cx_tcc_preflight(const char *service);
IMP cx_lookup(cx_class_t *cls, SEL sel);

typedef struct { long long value; int timescale; unsigned flags; long long epoch; } cmt_t;
typedef struct { cmt_t duration, pts, dts; } cmtiming_t;
cmt_t CMClockGetTime(void *clock);
void *CMClockGetHostTimeClock(void);
int CVPixelBufferCreateWithBytes(void *a, u64 w, u64 h, u32 fmt, void *base, u64 bpr, void (*rel)(void *, const void *), void *ctx, id attrs, void **out);
int CVPixelBufferCreateWithPlanarBytes(void *a, u64 w, u64 h, u32 fmt, void *d, u64 ds, u64 np, void **pb, u64 *pw, u64 *ph, u64 *pr,
                                       void *rel, void *ctx, id attrs, void **out);
int CMVideoFormatDescriptionCreateForImageBuffer(void *a, void *img, void **out);
int CMSampleBufferCreateReadyWithImageBuffer(void *a, void *img, void *fd, const void *timing, void **out);
void CMSetAttachment(void *target, id key, id value, u32 mode);
void *cx_cm_audio_float_planar(const float *planar, unsigned frames, unsigned ch, double rate, cmt_t pts);
void *CGDataProviderCreateWithData(void *info, const void *data, size_t size, void (*release)(void *, const void *, size_t));
void *CGImageCreate(size_t w, size_t h, size_t bpc, size_t bpp, size_t bpr, const void *space, u32 info, void *prov, const void *dec, _Bool interp, int intent);
void *CGColorSpaceCreateDeviceRGB(void);
void CGColorSpaceRelease(void *c);
void CGDataProviderRelease(void *p);
void CGImageRelease(void *img);
void *CGColorRetain(void *c);
void CGColorRelease(void *c);
size_t CGColorGetNumberOfComponents(const void *c);
const double *CGColorGetComponents(const void *c);

#define DEV_OP(op, a, b, c) ((long)SYS4(GUI_DEVICE, (op), (a), (b), (c)))
#define FCC(a, b, c, d) ((u32)(a) << 24 | (u32)(b) << 16 | (u32)(c) << 8 | (u32)(d))
#define FMT_BGRA FCC('B', 'G', 'R', 'A')
#define FMT_L10R FCC('l', '1', '0', 'r')
typedef struct { double x, y, w, h; } screct_t;

/* ---------------------------------------------------------------- constantes das informações do quadro */
extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; unsigned long long flags; const char *s; unsigned long long n; } sck_cf_t;
#define CFCONST(var, text) static sck_cf_t var##_s = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; const void *var = &var##_s;
/* as outras chaves SCStreamFrameInfo* estão em screencap_prod.c */
extern const void *SCStreamFrameInfoStatus, *SCStreamFrameInfoDisplayTime, *SCStreamFrameInfoScaleFactor, *SCStreamFrameInfoContentScale,
    *SCStreamFrameInfoContentRect, *SCStreamFrameInfoDirtyRects, *SCStreamFrameInfoBoundingRect;
CFCONST(SCStreamFrameInfoScreenRect, "SCStreamFrameInfoScreenRect")
CFCONST(SCStreamErrorDomain, "com.apple.ScreenCaptureKit.SCStreamErrorDomain")

/* ---------------------------------------------------------------- objetos */
typedef struct { cx_class_t *isa; screct_t frame; } scdisplay_t;
typedef struct { cx_class_t *isa; id displays, windows, apps; } sccontent_t;
typedef struct { cx_class_t *isa; u32 wid; i32 pid; screct_t frame; int on, layer, active; char title[96]; char owner[64]; char bundle[128]; } scwin_t;
typedef struct { cx_class_t *isa; i32 pid; char name[64]; char bundle[128]; } scapp_t;
typedef struct {
    cx_class_t *isa;
    sck_filter f;
    screct_t rect;
    i64 style;
    int menubar;
    id incl_windows, incl_apps, incl_displays;
} scfilter_t;
typedef struct { cx_class_t *isa; screct_t rect; i64 style; float scale; } scinfo_t;
typedef struct {
    cx_class_t *isa;
    i64 w, h, queue_depth, sample_rate, channels, capture_resolution;
    long long interval_us;
    u32 pixfmt;
    int shows_cursor, scales, preserve, audio, mic, exclude_self_audio, opaque, shadows_only, clicks, child_windows, dyn_range;
    int ignore_shadows_window, ignore_shadows_display, ignore_clip_window, ignore_clip_display;
    screct_t source, dest;
    void *bg;
    id colorspace, matrix, name, mic_device;
} sccfg_t;
extern cx_class_t SCDisplay_c __asm__("OBJC_CLASS_$_SCDisplay"), SCDisplay_m __asm__("OBJC_METACLASS_$_SCDisplay");
extern cx_class_t SCWindow_c __asm__("OBJC_CLASS_$_SCWindow"), SCWindow_m __asm__("OBJC_METACLASS_$_SCWindow");
extern cx_class_t SCContentFilter_c __asm__("OBJC_CLASS_$_SCContentFilter"), SCContentFilter_m __asm__("OBJC_METACLASS_$_SCContentFilter");
extern cx_class_t SCShareableContent_c __asm__("OBJC_CLASS_$_SCShareableContent"), SCShareableContent_m __asm__("OBJC_METACLASS_$_SCShareableContent");
extern cx_class_t SCStreamConfiguration_c __asm__("OBJC_CLASS_$_SCStreamConfiguration"), SCStreamConfiguration_m __asm__("OBJC_METACLASS_$_SCStreamConfiguration");
cx_class_t SCRunningApplication_c __asm__("OBJC_CLASS_$_SCRunningApplication"), SCRunningApplication_m __asm__("OBJC_METACLASS_$_SCRunningApplication");
cx_class_t SCStream_c __asm__("OBJC_CLASS_$_SCStream"), SCStream_m __asm__("OBJC_METACLASS_$_SCStream");
cx_class_t SCShareableContentInfo_c __asm__("OBJC_CLASS_$_SCShareableContentInfo"), SCShareableContentInfo_m __asm__("OBJC_METACLASS_$_SCShareableContentInfo");
#define FLT(o) ((scfilter_t *)(o))
#define CFG(o) ((sccfg_t *)(o))

static screct_t screen_rect(void) {
    long sz = DEV_OP(DEV_SCREEN_CAPTURE, 0, 0, 0);    /* buf 0: só o tamanho da tela inteira */
    screct_t r = {0, 0, 0, 0};
    if (sz > 0) { r.w = (double)(u32)sz; r.h = (double)(u32)((u64)sz >> 32); }
    return r;
}
static id arr_new(void) { return MSG0(id, (id)cx_class_named("NSMutableArray"), "array"); }
static id sc_error(long code, const char *msg) {
    id ui = MSG(id, (id)cx_class_named("NSDictionary"), "dictionaryWithObject:forKey:", cx_str(msg), cx_str("NSLocalizedDescription"));
    return MSG(id, (id)cx_class_named("NSError"), "errorWithDomain:code:userInfo:", (id)SCStreamErrorDomain, code, ui);
}
static void call_blk1(void *blk, id a) { if (blk) ((void (*)(void *, id))(*(void **)((u8 *)blk + 16)))(blk, a); }
static void call_blk2(void *blk, void *a, id b) { if (blk) ((void (*)(void *, void *, id))(*(void **)((u8 *)blk + 16)))(blk, a, b); }

/* ---------------------------------------------------------------- SCDisplay / SCWindow / SCRunningApplication */
static u32 d_id(id s, SEL c) { (void)s; (void)c; return 1; }
static long d_w(id s, SEL c) { (void)c; return (long)((scdisplay_t *)s)->frame.w; }
static long d_h(id s, SEL c) { (void)c; return (long)((scdisplay_t *)s)->frame.h; }
static screct_t d_frame(id s, SEL c) { (void)c; return ((scdisplay_t *)s)->frame; }
#define WIN(o) ((scwin_t *)(o))
static u32 w_id(id s, SEL c) { (void)c; return WIN(s)->wid; }
static screct_t w_frame(id s, SEL c) { (void)c; return WIN(s)->frame; }
static BOOL w_on(id s, SEL c) { (void)c; return WIN(s)->on ? YES : NO; }
static BOOL w_active(id s, SEL c) { (void)c; return WIN(s)->active ? YES : NO; }
static i64 w_layer(id s, SEL c) { (void)c; return WIN(s)->layer; }
static id w_title(id s, SEL c) { (void)c; return cx_str(WIN(s)->title); }
static id app_make(i32 pid, const char *name, const char *bundle) {
    scapp_t *a = (scapp_t *)cx_alloc_instance(&SCRunningApplication_c);
    if (!a) return 0;
    a->pid = pid;
    for (int i = 0; name && name[i] && i < 63; i++) a->name[i] = name[i];
    for (int i = 0; bundle && bundle[i] && i < 127; i++) a->bundle[i] = bundle[i];
    return (id)a;
}
static id w_owner(id s, SEL c) { (void)c; id a = app_make(WIN(s)->pid, WIN(s)->owner, WIN(s)->bundle); return a ? objc_autorelease(a) : 0; }
static i64 app_pid(id s, SEL c) { (void)c; return ((scapp_t *)s)->pid; }
static id app_name(id s, SEL c) { (void)c; return cx_str(((scapp_t *)s)->name); }
/* o CFBundleIdentifier do Info.plist do .app de onde o processo veio; "" se não veio de um bundle */
static id app_bundle(id s, SEL c) {
    (void)c;
    const char *path = ((scapp_t *)s)->bundle;
    id b = path[0] ? MSG(id, (id)cx_class_named("NSBundle"), "bundleWithPath:", cx_str(path)) : 0;
    id bid = b ? MSG0(id, b, "bundleIdentifier") : 0;
    return bid ? bid : cx_str("");
}

/* ---------------------------------------------------------------- SCShareableContent */
static id window_obj(const gui_cgwindow_t *info, int active) {
    scwin_t *w = (scwin_t *)cx_alloc_instance(&SCWindow_c);
    if (!w) return 0;
    w->wid = info->number; w->pid = info->pid; w->on = info->onscreen; w->layer = info->layer; w->active = active;
    w->frame.x = info->x; w->frame.y = info->y; w->frame.w = info->w; w->frame.h = info->h;
    memcpy(w->title, info->name, sizeof w->title); w->title[sizeof w->title - 1] = 0;
    memcpy(w->owner, info->owner, sizeof w->owner); w->owner[sizeof w->owner - 1] = 0;
    memcpy(w->bundle, info->bundle, sizeof w->bundle); w->bundle[sizeof w->bundle - 1] = 0;
    return (id)w;
}
/* side: 0 todas, 1 só acima de anchor (na frente), -1 só abaixo */
static id content_make(int exclude_desktop, int onscreen_only, u32 anchor, int side, int only_pid) {
    sccontent_t *ct = (sccontent_t *)cx_alloc_instance(&SCShareableContent_c);
    scdisplay_t *d = (scdisplay_t *)cx_alloc_instance(&SCDisplay_c);
    if (!ct || !d) return 0;
    d->frame = screen_rect();
    ct->displays = objc_retain(MSG(id, (id)cx_class_named("NSArray"), "arrayWithObject:", (id)d));
    objc_release((id)d);
    id windows = objc_retain(arr_new()), apps = objc_retain(arr_new());
    if (cx_tcc_preflight("kTCCServiceScreenCapture") == 1) {
        int anchor_at = -1, seen[64], np = 0;
        gui_cgwindow_t info;
        if (side) for (int i = 0; i < 64 && cx_cg_window_at(i, &info); i++) if (info.number == anchor) { anchor_at = i; break; }
        int first_active = 1;
        for (int i = 0; i < 64 && cx_cg_window_at(i, &info); i++) {
            int active = first_active && info.pid && info.onscreen && info.layer == 0;
            if (active) first_active = 0;
            if (side && (anchor_at < 0 || (side > 0 ? i >= anchor_at : i <= anchor_at))) continue;
            if (exclude_desktop && info.pid == 0) continue;
            if (onscreen_only && !info.onscreen) continue;
            if (only_pid && info.pid != only_pid) continue;
            id w = window_obj(&info, active);
            if (w) { MSG(void, windows, "addObject:", w); objc_release(w); }
            if (!info.pid) continue;
            int dup = 0;
            for (int k = 0; k < np; k++) if (seen[k] == info.pid) dup = 1;
            if (dup || np >= 64) continue;
            seen[np++] = info.pid;
            id a = app_make(info.pid, info.owner, info.bundle);
            if (a) { MSG(void, apps, "addObject:", a); objc_release(a); }
        }
    }
    ct->windows = windows;
    ct->apps = apps;
    return objc_autorelease((id)ct);
}
static id ct_displays(id s, SEL c) { (void)c; return ((sccontent_t *)s)->displays; }
static id ct_windows(id s, SEL c) { (void)c; return ((sccontent_t *)s)->windows; }
static id ct_apps(id s, SEL c) { (void)c; return ((sccontent_t *)s)->apps; }
typedef struct { void *blk; id content; } ctjob_t;
static void ct_run(void *a) {
    ctjob_t *j = a;
    call_blk2(j->blk, (void *)j->content, j->content ? 0 : sc_error(-3801, "Sem permissão de gravação da tela"));
    if (j->content) objc_release(j->content);
    _Block_release(j->blk);
    free(j);
}
static void ct_later(void *blk, id content) {
    if (!blk) return;
    ctjob_t *j = calloc(1, sizeof *j);
    if (!j) return;
    j->blk = _Block_copy(blk); j->content = content ? objc_retain(content) : 0;
    dispatch_async_f(dispatch_get_global_queue(0, 0), j, ct_run);
}
static void ct_get(id k, SEL c, void *blk) { (void)k; (void)c; ct_later(blk, content_make(0, 0, 0, 0, 0)); }
static void ct_get2(id k, SEL c, BOOL exd, BOOL on, void *blk) { (void)k; (void)c; ct_later(blk, content_make(exd, on, 0, 0, 0)); }
static void ct_below(id k, SEL c, BOOL exd, id w, void *blk) { (void)k; (void)c; ct_later(blk, content_make(exd, 1, w ? WIN(w)->wid : 0, -1, 0)); }
static void ct_above(id k, SEL c, BOOL exd, id w, void *blk) { (void)k; (void)c; ct_later(blk, content_make(exd, 1, w ? WIN(w)->wid : 0, 1, 0)); }
i32 getpid(void);
static void ct_current(id k, SEL c, void *blk) { (void)k; (void)c; ct_later(blk, content_make(0, 0, 0, 0, getpid())); }

/* ---------------------------------------------------------------- SCContentFilter */
static int ids_of(id list, u32 *out, int max, int apps) {
    if (!list) return 0;
    int n = (int)MSG0(u64, list, "count");
    int k = 0;
    for (int i = 0; i < n && k < max; i++) {
        id o = MSG(id, list, "objectAtIndex:", (u64)i);
        if (!o) continue;
        if (apps) ((i32 *)out)[k++] = ((scapp_t *)o)->pid;
        else out[k++] = WIN(o)->wid;
    }
    return k;
}
static void keep(id *slot, id v) { if (v) objc_retain(v); if (*slot) objc_release(*slot); *slot = v; }
static id f_display(id s, id display, int kind) {
    scfilter_t *f = FLT(s);
    memset(&f->f, 0, sizeof f->f);
    f->f.kind = kind;
    f->rect = display ? ((scdisplay_t *)display)->frame : screen_rect();
    f->style = 2;   /* SCShareableContentStyleDisplay */
    f->menubar = 1;
    id one = display ? MSG(id, (id)cx_class_named("NSArray"), "arrayWithObject:", display) : arr_new();
    keep(&f->incl_displays, one);
    return s;
}
static id f_excl_windows(id s, SEL c, id d, id wins) {
    (void)c; f_display(s, d, SCK_DISPLAY_EXCL_WINDOWS);
    FLT(s)->f.nwin = ids_of(wins, FLT(s)->f.win, SCK_MAX, 0);
    return s;
}
static id f_incl_windows(id s, SEL c, id d, id wins) {
    (void)c; f_display(s, d, SCK_DISPLAY_INCL_WINDOWS);
    FLT(s)->f.nwin = ids_of(wins, FLT(s)->f.win, SCK_MAX, 0);
    keep(&FLT(s)->incl_windows, wins);
    return s;
}
static id f_excl_apps(id s, SEL c, id d, id apps, id except) {
    (void)c; f_display(s, d, SCK_DISPLAY_EXCL_APPS);
    FLT(s)->f.npid = ids_of(apps, (u32 *)FLT(s)->f.pid, SCK_MAX, 1);
    FLT(s)->f.nwin = ids_of(except, FLT(s)->f.win, SCK_MAX, 0);
    return s;
}
static id f_incl_apps(id s, SEL c, id d, id apps, id except) {
    (void)c; f_display(s, d, SCK_DISPLAY_INCL_APPS);
    FLT(s)->f.npid = ids_of(apps, (u32 *)FLT(s)->f.pid, SCK_MAX, 1);
    FLT(s)->f.nwin = ids_of(except, FLT(s)->f.win, SCK_MAX, 0);
    keep(&FLT(s)->incl_apps, apps);
    return s;
}
static id f_window(id s, SEL c, id win) {
    (void)c;
    scfilter_t *f = FLT(s);
    memset(&f->f, 0, sizeof f->f);
    f->f.kind = SCK_WINDOW;
    f->style = 1;   /* SCShareableContentStyleWindow */
    if (win) {
        f->f.window = WIN(win)->wid;
        f->rect.x = 0; f->rect.y = 0; f->rect.w = WIN(win)->frame.w; f->rect.h = WIN(win)->frame.h;
        keep(&f->incl_windows, MSG(id, (id)cx_class_named("NSArray"), "arrayWithObject:", win));
    }
    return s;
}
static screct_t f_rect(id s, SEL c) { (void)c; return FLT(s)->rect; }
static float f_scale(id s, SEL c) { (void)s; (void)c; return 1.0f; }
static i64 f_style(id s, SEL c) { (void)c; return FLT(s)->style; }
static i64 f_stream_type(id s, SEL c) { (void)c; return FLT(s)->f.kind == SCK_WINDOW ? 0 : 1; }
static BOOL f_menubar(id s, SEL c) { (void)c; return FLT(s)->menubar ? YES : NO; }
static void f_set_menubar(id s, SEL c, BOOL v) { (void)c; FLT(s)->menubar = v; }
static id f_incl_w(id s, SEL c) { (void)c; return FLT(s)->incl_windows ? FLT(s)->incl_windows : arr_new(); }
static id f_incl_a(id s, SEL c) { (void)c; return FLT(s)->incl_apps ? FLT(s)->incl_apps : arr_new(); }
static id f_incl_d(id s, SEL c) { (void)c; return FLT(s)->incl_displays ? FLT(s)->incl_displays : arr_new(); }
static id info_for(id k, SEL c, id filter) {
    (void)k; (void)c;
    scinfo_t *i = (scinfo_t *)cx_alloc_instance(&SCShareableContentInfo_c);
    if (!i) return 0;
    if (filter) { i->rect = FLT(filter)->rect; i->style = FLT(filter)->style; }
    i->scale = 1.0f;
    return objc_autorelease((id)i);
}
static screct_t info_rect(id s, SEL c) { (void)c; return ((scinfo_t *)s)->rect; }
static float info_scale(id s, SEL c) { (void)c; return ((scinfo_t *)s)->scale; }
static i64 info_style(id s, SEL c) { (void)c; return ((scinfo_t *)s)->style; }

/* ---------------------------------------------------------------- SCStreamConfiguration */
static id cfg_init(id s, SEL c) {
    (void)c;
    sccfg_t *k = CFG(s);
    k->pixfmt = FMT_BGRA; k->shows_cursor = 1; k->preserve = 1; k->queue_depth = 8; k->sample_rate = 48000; k->channels = 2;
    k->opaque = 0; k->child_windows = 1;
    return s;
}
#define CFG_I64(get, set, field) \
    static i64 get(id s, SEL c) { (void)c; return CFG(s)->field; } \
    static void set(id s, SEL c, i64 v) { (void)c; CFG(s)->field = v; }
#define CFG_BOOL(get, set, field) \
    static BOOL get(id s, SEL c) { (void)c; return CFG(s)->field ? YES : NO; } \
    static void set(id s, SEL c, BOOL v) { (void)c; CFG(s)->field = v != 0; }
#define CFG_RECT(get, set, field) \
    static screct_t get(id s, SEL c) { (void)c; return CFG(s)->field; } \
    static void set(id s, SEL c, screct_t v) { (void)c; CFG(s)->field = v; }
#define CFG_OBJ(get, set, field) \
    static id get(id s, SEL c) { (void)c; return CFG(s)->field; } \
    static void set(id s, SEL c, id v) { (void)c; keep(&CFG(s)->field, v); }
CFG_I64(cfg_w, cfg_set_w, w)
CFG_I64(cfg_h, cfg_set_h, h)
CFG_I64(cfg_qd, cfg_set_qd, queue_depth)
CFG_I64(cfg_rate, cfg_set_rate, sample_rate)
CFG_I64(cfg_ch, cfg_set_ch, channels)
CFG_I64(cfg_res, cfg_set_res, capture_resolution)
CFG_BOOL(cfg_cursor, cfg_set_cursor, shows_cursor)
CFG_BOOL(cfg_scales, cfg_set_scales, scales)
CFG_BOOL(cfg_preserve, cfg_set_preserve, preserve)
CFG_BOOL(cfg_audio, cfg_set_audio, audio)
CFG_BOOL(cfg_mic, cfg_set_mic, mic)
CFG_BOOL(cfg_exself, cfg_set_exself, exclude_self_audio)
CFG_BOOL(cfg_opaque, cfg_set_opaque, opaque)
CFG_BOOL(cfg_shonly, cfg_set_shonly, shadows_only)
CFG_BOOL(cfg_clicks, cfg_set_clicks, clicks)
CFG_BOOL(cfg_child, cfg_set_child, child_windows)
CFG_BOOL(cfg_igsw, cfg_set_igsw, ignore_shadows_window)
CFG_BOOL(cfg_igsd, cfg_set_igsd, ignore_shadows_display)
CFG_BOOL(cfg_igcw, cfg_set_igcw, ignore_clip_window)
CFG_BOOL(cfg_igcd, cfg_set_igcd, ignore_clip_display)
CFG_RECT(cfg_src, cfg_set_src, source)
CFG_RECT(cfg_dst, cfg_set_dst, dest)
CFG_OBJ(cfg_cs, cfg_set_cs, colorspace)
CFG_OBJ(cfg_matrix, cfg_set_matrix, matrix)
CFG_OBJ(cfg_name, cfg_set_name, name)
CFG_OBJ(cfg_micdev, cfg_set_micdev, mic_device)
static u32 cfg_pix(id s, SEL c) { (void)c; return CFG(s)->pixfmt; }
static void cfg_set_pix(id s, SEL c, u32 v) { (void)c; CFG(s)->pixfmt = v; }
static cmt_t cfg_interval(id s, SEL c) { (void)c; cmt_t t = {CFG(s)->interval_us, 1000000, 1, 0}; return t; }
static void cfg_set_interval(id s, SEL c, cmt_t t) {
    (void)c;
    long long us = 0;
    if ((t.flags & 1) && t.timescale > 0 && t.value > 0) us = (long long)((double)t.value * 1000000.0 / t.timescale);
    CFG(s)->interval_us = us;
}
static void *cfg_bg(id s, SEL c) { (void)c; return CFG(s)->bg; }
static void cfg_set_bg(id s, SEL c, void *v) {
    (void)c;
    if (v) CGColorRetain(v);
    if (CFG(s)->bg) CGColorRelease(CFG(s)->bg);
    CFG(s)->bg = v;
}
static i64 cfg_dynrange(id s, SEL c) { (void)c; return CFG(s)->dyn_range; }
static void cfg_set_dynrange(id s, SEL c, i64 v) { (void)c; CFG(s)->dyn_range = (int)v; }
static u32 bg_argb(void *color) {
    if (!color) return 0xFF000000u;
    size_t n = CGColorGetNumberOfComponents(color);
    const double *k = CGColorGetComponents(color);
    if (!k || n < 2) return 0xFF000000u;
    double r = k[0], g = n >= 4 ? k[1] : k[0], b = n >= 4 ? k[2] : k[0], a = k[n - 1];
#define C8(v) ((u32)((v) < 0 ? 0 : (v) > 1 ? 255 : (v) * 255 + 0.5))
    return C8(a) << 24 | C8(r) << 16 | C8(g) << 8 | C8(b);
#undef C8
}

/* ---------------------------------------------------------------- captura de um quadro (filtro + configuração) */
typedef struct {
    sck_filter f;
    screct_t frect;
    int menubar;
    sccfg_t cfg;     /* cópia rasa: os objetos não são usados na thread */
    u32 bg;
} sckspec_t;
static void spec_from(sckspec_t *sp, id filter, id cfg) {
    memset(sp, 0, sizeof *sp);
    if (filter) { sp->f = FLT(filter)->f; sp->frect = FLT(filter)->rect; sp->menubar = FLT(filter)->menubar; }
    else { sp->frect = screen_rect(); sp->menubar = 1; }
    if (cfg) sp->cfg = *CFG(cfg);
    else { sccfg_t d; memset(&d, 0, sizeof d); cfg_init((id)&d, 0); sp->cfg = d; }
    sp->bg = bg_argb(sp->cfg.bg);
}
static int sck_windows(sck_win *out, int max) {
    gui_cgwindow_t info;
    int n = 0;
    for (int i = 0; i < 64 && n < max && cx_cg_window_at(i, &info); i++) { out[n].id = info.number; out[n].pid = info.pid; n++; }
    return n;
}
/* pixels BGRA da área pedida em *px (malloc); 0 ok, -1 a janela sumiu, -2 falha */
static int grab(const sckspec_t *sp, u32 **px, int *w, int *h) {
    *px = 0;
    if (sp->f.kind == SCK_WINDOW) {
        long sz = DEV_OP(DEV_WINDOW_CAPTURE, sp->f.window, 0, 0);
        if (sz < 0) return -1;
        int ww = (int)(u32)sz, wh = (int)(u32)((u64)sz >> 32);
        u32 *buf = malloc((size_t)ww * wh * 4);
        if (!buf) return -2;
        if (DEV_OP(DEV_WINDOW_CAPTURE, sp->f.window, buf, 0) < 0) { free(buf); return -1; }
        screct_t s = sp->cfg.source;   /* sourceRect: no espaço da janela */
        if (s.w > 0 && s.h > 0) {
            int x0 = (int)s.x, y0 = (int)s.y, cw = (int)s.w, ch = (int)s.h;
            if (x0 < 0) { cw += x0; x0 = 0; }
            if (y0 < 0) { ch += y0; y0 = 0; }
            if (x0 + cw > ww) cw = ww - x0;
            if (y0 + ch > wh) ch = wh - y0;
            if (cw > 0 && ch > 0) {
                u32 *crop = malloc((size_t)cw * ch * 4);
                if (crop) {
                    for (int y = 0; y < ch; y++) memcpy(crop + (size_t)y * cw, buf + (size_t)(y0 + y) * ww + x0, (size_t)cw * 4);
                    free(buf); buf = crop; ww = cw; wh = ch;
                }
            }
        }
        *px = buf; *w = ww; *h = wh;
        return 0;
    }
    gui_sc_compose_t rq;
    memset(&rq, 0, sizeof rq);
    screct_t a = sp->frect;
    if (sp->cfg.source.w > 0 && sp->cfg.source.h > 0) { a.x += sp->cfg.source.x; a.y += sp->cfg.source.y; a.w = sp->cfg.source.w; a.h = sp->cfg.source.h; }
    rq.x = (int)a.x; rq.y = (int)a.y; rq.w = (int)a.w; rq.h = (int)a.h;
    sck_win wins[64];
    int nw = sck_windows(wins, 64), include = 0;
    rq.nwin = (unsigned)sck_resolve(&sp->f, wins, nw, rq.win, &include);
    rq.flags = (include ? GUI_SC_INCLUDE : GUI_SC_DESKTOP) | (sp->cfg.shows_cursor ? GUI_SC_CURSOR : 0);
    long sz;
    int plain = !include && !rq.nwin && sp->cfg.shows_cursor;   /* a tela do compositor como está (com sombras) */
    u64 xy = (u64)(u32)rq.x | (u64)(u32)rq.y << 32, wh = (u64)(u32)rq.w | (u64)(u32)rq.h << 32;
    sz = plain ? DEV_OP(DEV_SCREEN_CAPTURE, xy, wh, 0) : DEV_OP(DEV_SCREEN_COMPOSE, &rq, 0, 0);
    if (sz <= 0) return -2;
    int cw = (int)(u32)sz, ch = (int)(u32)((u64)sz >> 32);
    u32 *buf = malloc((size_t)cw * ch * 4);
    if (!buf) return -2;
    long r;
    if (plain) r = DEV_OP(DEV_SCREEN_CAPTURE, xy, (u64)(u32)cw | (u64)(u32)ch << 32, buf);
    else r = DEV_OP(DEV_SCREEN_COMPOSE, &rq, buf, 0);
    if (r <= 0) { free(buf); return -2; }
    *px = buf; *w = cw; *h = ch;
    return 0;
}
/* saída do tamanho da configuração (0 = o da origem) com escala, destino e fundo */
static u32 *place(const sckspec_t *sp, const u32 *src, int sw, int sh, int *ow, int *oh, double *content_scale, screct_t *content) {
    int w = sp->cfg.w > 0 ? (int)sp->cfg.w : sw, h = sp->cfg.h > 0 ? (int)sp->cfg.h : sh;
    sck_rect d = {sp->cfg.dest.x, sp->cfg.dest.y, sp->cfg.dest.w, sp->cfg.dest.h};
    int scales = sp->cfg.scales || (sp->cfg.w > 0 && sp->cfg.h > 0 && sp->f.kind != SCK_WINDOW && (sw > w || sh > h));
    sck_rect r = sck_layout(sw, sh, w, h, scales, sp->cfg.preserve, d);
    u32 *out = malloc((size_t)w * h * 4);
    if (!out) return 0;
    sck_place(src, sw, sh, sw, out, w, h, r, sp->bg);
    *ow = w; *oh = h;
    *content_scale = sw > 0 ? r.w / sw : 1;
    content->x = r.x; content->y = r.y; content->w = r.w; content->h = r.h;
    return out;
}
static void pb_free(void *ctx, const void *base) { (void)base; free(ctx); }
static void pb_free_planar(void *ctx, const void *d, u64 sz, u64 np, const void **pl) { (void)d; (void)sz; (void)np; (void)pl; free(ctx); }
/* CVPixelBuffer no formato pedido; out passa a ser do buffer (BGRA) ou é liberado aqui */
static void *pixel_buffer(u32 *bgra, int w, int h, u32 fmt) {
    void *pb = 0;
    if (fmt == AVC_FMT_420V || fmt == AVC_FMT_420F) {
        size_t bpr[3], rows[3];
        avc_planes(fmt, w, h, bpr, rows);
        size_t total = bpr[0] * rows[0] + bpr[1] * rows[1];
        u8 *mem = malloc(total);
        if (!mem) { free(bgra); return 0; }
        u8 *pl[2] = {mem, mem + bpr[0] * rows[0]};
        avc_convert(AVC_SRC_BGRA, (const u8 *)bgra, (size_t)w * h * 4, w, h, fmt, pl, bpr);
        free(bgra);
        void *pbase[2] = {pl[0], pl[1]};
        u64 pw[2] = {(u64)w, (u64)(w + 1) / 2}, ph[2] = {rows[0], rows[1]}, pr[2] = {bpr[0], bpr[1]};
        CVPixelBufferCreateWithPlanarBytes(0, (u64)w, (u64)h, fmt, mem, total, 2, pbase, pw, ph, pr, (void *)pb_free_planar, mem, 0, &pb);
        if (!pb) free(mem);
        return pb;
    }
    if (fmt == FMT_L10R) sck_to_l10r(bgra, w, h, bgra);
    CVPixelBufferCreateWithBytes(0, (u64)w, (u64)h, fmt == FMT_L10R ? FMT_L10R : FMT_BGRA, bgra, (u64)w * 4, pb_free, bgra, 0, &pb);
    if (!pb) free(bgra);
    return pb;
}
static id rect_dict(screct_t r) {
    id d = MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary");
    MSG(void, d, "setObject:forKey:", cx_num_d(r.x), cx_str("X"));
    MSG(void, d, "setObject:forKey:", cx_num_d(r.y), cx_str("Y"));
    MSG(void, d, "setObject:forKey:", cx_num_d(r.w), cx_str("Width"));
    MSG(void, d, "setObject:forKey:", cx_num_d(r.h), cx_str("Height"));
    return d;
}
/* um quadro completo em CMSampleBuffer (+1), com as informações SCStreamFrameInfo* */
static void *frame_sample(const sckspec_t *sp, u32 *px, int sw, int sh, u64 *hash_io) {
    int ow, oh;
    double cs;
    screct_t content;
    u32 *out = place(sp, px, sw, sh, &ow, &oh, &cs, &content);
    if (!out) return 0;
    if (hash_io) {
        u64 hsh = sck_hash(out, (size_t)ow * oh);
        if (*hash_io && hsh == *hash_io) { free(out); return 0; }   /* nada mudou: o SCStream não manda quadro */
        *hash_io = hsh;
    }
    u32 fmt = sp->cfg.pixfmt ? sp->cfg.pixfmt : FMT_BGRA;
    if (fmt != FMT_BGRA && fmt != FMT_L10R && fmt != AVC_FMT_420V && fmt != AVC_FMT_420F) fmt = FMT_BGRA;
    void *pb = pixel_buffer(out, ow, oh, fmt);
    if (!pb) return 0;
    void *fd = 0, *sb = 0;
    cmt_t now = CMClockGetTime(CMClockGetHostTimeClock());
    cmtiming_t t = {{0, 0, 0, 0}, now, {0, 0, 0, 0}};
    if (!CMVideoFormatDescriptionCreateForImageBuffer(0, pb, &fd) && fd) CMSampleBufferCreateReadyWithImageBuffer(0, pb, fd, &t, &sb);
    if (fd) objc_release((id)fd);
    objc_release((id)pb);
    if (!sb) return 0;
    screct_t screen = sp->frect;
    if (sp->f.kind != SCK_WINDOW && sp->cfg.source.w > 0) { screen.x += sp->cfg.source.x; screen.y += sp->cfg.source.y; screen.w = sp->cfg.source.w; screen.h = sp->cfg.source.h; }
    id cr = rect_dict(content);
    CMSetAttachment(sb, (id)SCStreamFrameInfoStatus, cx_num_i(0 /* SCFrameStatusComplete */), 1);
    CMSetAttachment(sb, (id)SCStreamFrameInfoDisplayTime, cx_num_i((i64)mach_absolute_time()), 1);
    CMSetAttachment(sb, (id)SCStreamFrameInfoScaleFactor, cx_num_d(1.0), 1);
    CMSetAttachment(sb, (id)SCStreamFrameInfoContentScale, cx_num_d(cs), 1);
    CMSetAttachment(sb, (id)SCStreamFrameInfoContentRect, cr, 1);
    CMSetAttachment(sb, (id)SCStreamFrameInfoBoundingRect, cr, 1);
    CMSetAttachment(sb, (id)SCStreamFrameInfoScreenRect, rect_dict(screen), 1);
    CMSetAttachment(sb, (id)SCStreamFrameInfoDirtyRects, MSG(id, (id)cx_class_named("NSArray"), "arrayWithObject:", cr), 1);
    return sb;
}

/* ---------------------------------------------------------------- SCStream */
enum { OUT_SCREEN = 0, OUT_AUDIO = 1, OUT_MIC = 2 };
#define MAX_OUT 4
typedef struct { id obj; void *queue; volatile int inflight; } scout_t;
typedef struct {
    cx_class_t *isa;
    id delegate, filter, cfg;
    scout_t out[3][MAX_OUT];
    volatile int run, done, lock, dirty;
    unsigned long th;
    sckspec_t spec;
    u64 hash, audio_pos;
    double mic_t0; u64 mic_frames;
} scstream_t;
#define ST(o) ((scstream_t *)(o))
static void spin(volatile int *l) { while (__atomic_exchange_n(l, 1, __ATOMIC_ACQUIRE)) usleep(50); }
static void unspin(volatile int *l) { __atomic_store_n(l, 0, __ATOMIC_RELEASE); }
typedef struct { id stream, out; void *sb; long type; volatile int *inflight; } outjob_t;
static void out_run(void *a) {
    outjob_t *j = a;
    if (cx_responds(object_getClass(j->out), sel_registerName("stream:didOutputSampleBuffer:ofType:")))
        MSG(void, j->out, "stream:didOutputSampleBuffer:ofType:", j->stream, (id)j->sb, j->type);
    if (j->inflight) __atomic_sub_fetch(j->inflight, 1, __ATOMIC_ACQ_REL);
    objc_release((id)j->sb);
    objc_release(j->out);
    objc_release(j->stream);
    free(j);
}
static void deliver(scstream_t *st, int type, void *sb) {
    spin(&st->lock);
    for (int i = 0; i < MAX_OUT; i++) {
        scout_t *o = &st->out[type][i];
        if (!o->obj) continue;
        if (type == OUT_SCREEN && __atomic_load_n(&o->inflight, __ATOMIC_ACQUIRE) >= (st->spec.cfg.queue_depth > 0 ? st->spec.cfg.queue_depth : 8)) continue;
        outjob_t *j = calloc(1, sizeof *j);
        if (!j) continue;
        j->stream = objc_retain((id)st); j->out = objc_retain(o->obj); j->sb = objc_retain((id)sb); j->type = type;
        j->inflight = type == OUT_SCREEN ? &o->inflight : 0;
        if (j->inflight) __atomic_add_fetch(j->inflight, 1, __ATOMIC_ACQ_REL);
        dispatch_async_f(o->queue ? o->queue : dispatch_get_global_queue(0, 0), j, out_run);
    }
    unspin(&st->lock);
}
typedef struct { id stream, delegate, err; } stopjob_t;
static void stop_run(void *a) {
    stopjob_t *j = a;
    if (j->delegate && cx_responds(object_getClass(j->delegate), sel_registerName("stream:didStopWithError:")))
        MSG(void, j->delegate, "stream:didStopWithError:", j->stream, j->err);
    objc_release(j->err); objc_release(j->delegate); objc_release(j->stream);
    free(j);
}
extern unsigned avf_copy_capture(short *dst, unsigned frames, int *err);
static void audio_step(scstream_t *st) {
    short pcm[4800 * 2];
    float planar[4800 * 2];
    if (st->spec.cfg.audio && (st->out[OUT_AUDIO][0].obj || st->out[OUT_AUDIO][1].obj)) {
        u64 pos = st->audio_pos;
        long n = DEV_OP(DEV_AUDIO_SYSTEM, pcm, 4800, &pos);
        if (n > 0) {
            cmt_t now = CMClockGetTime(CMClockGetHostTimeClock());
            /* o pts é o do primeiro quadro do bloco: agora menos a duração do que veio */
            cmt_t pts = {now.value - (long long)((double)n * 1e9 / 48000.0), 1000000000, 1, 0};
            unsigned ch = st->spec.cfg.channels == 1 ? 1 : 2;
            for (long i = 0; i < n; i++) {
                float l = pcm[i * 2] / 32768.0f, r = pcm[i * 2 + 1] / 32768.0f;
                if (ch == 1) planar[i] = (l + r) * 0.5f;
                else { planar[i] = l; planar[n + i] = r; }
            }
            void *sb = cx_cm_audio_float_planar(planar, (unsigned)n, ch, 48000, pts);
            if (sb) { deliver(st, OUT_AUDIO, sb); objc_release((id)sb); }
        }
        st->audio_pos = pos;
    }
    if (st->spec.cfg.mic && (st->out[OUT_MIC][0].obj || st->out[OUT_MIC][1].obj)) {
        int err = 0;
        unsigned n = avf_copy_capture(pcm, 2400, &err);
        if (n) {
            if (!st->mic_frames) st->mic_t0 = (double)CMClockGetTime(CMClockGetHostTimeClock()).value / 1e9;
            cmt_t pts = {(long long)((st->mic_t0 + (double)st->mic_frames / 48000.0) * 1e9), 1000000000, 1, 0};
            st->mic_frames += n;
            for (unsigned i = 0; i < n; i++) { planar[i] = pcm[i * 2] / 32768.0f; planar[n + i] = pcm[i * 2 + 1] / 32768.0f; }
            void *sb = cx_cm_audio_float_planar(planar, n, 2, 48000, pts);
            if (sb) { deliver(st, OUT_MIC, sb); objc_release((id)sb); }
        }
    }
}
static long long now_us(void) { return CMClockGetTime(CMClockGetHostTimeClock()).value / 1000; }
static void *stream_thread(void *a) {
    scstream_t *st = a;
    long long next = 0;
    while (st->run) {
        void *pool = objc_autoreleasePoolPush();
        long long t = now_us();
        sckspec_t sp;   /* cópia sob a trava: update* troca a especificação entre dois quadros */
        spin(&st->lock);
        sp = st->spec;
        if (st->dirty) { st->dirty = 0; st->hash = 0; }
        unspin(&st->lock);
        if (t >= next) {
            long long iv = sp.cfg.interval_us > 0 ? sp.cfg.interval_us : 16667;
            next = t + iv;
            u32 *px = 0;
            int w = 0, h = 0;
            int rc = grab(&sp, &px, &w, &h);
            if (rc == -1) {   /* a janela capturada fechou: o stream para e o delegate fica sabendo */
                st->run = 0;
                stopjob_t *j = calloc(1, sizeof *j);
                if (j) {
                    j->stream = objc_retain((id)st); j->delegate = st->delegate ? objc_retain(st->delegate) : 0;
                    j->err = objc_retain(sc_error(-3815, "A janela capturada não existe mais"));   /* código de "sem fonte": valor não confirmado */
                    dispatch_async_f(dispatch_get_global_queue(0, 0), j, stop_run);
                }
            } else if (!rc) {
                void *sb = frame_sample(&sp, px, w, h, &st->hash);
                if (sb) { deliver(st, OUT_SCREEN, sb); objc_release((id)sb); }
            }
            free(px);
        }
        audio_step(st);
        objc_autoreleasePoolPop(pool);
        usleep(4000);
    }
    st->done = 1;
    return 0;
}
static void stream_halt(scstream_t *st) {
    if (!st->th) return;
    st->run = 0;
    for (int i = 0; i < 1000 && !st->done; i++) usleep(1000);
    st->th = 0;
}
static id stream_init(id s, SEL c, id filter, id cfg, id delegate) {
    (void)c;
    scstream_t *st = ST(s);
    keep(&st->filter, filter);
    keep(&st->cfg, cfg);
    st->delegate = delegate;   /* fraco, como no macOS */
    spec_from(&st->spec, filter, cfg);
    return s;
}
static void stream_dealloc(id s, SEL c) {
    scstream_t *st = ST(s);
    stream_halt(st);
    for (int t = 0; t < 3; t++) for (int i = 0; i < MAX_OUT; i++) if (st->out[t][i].obj) objc_release(st->out[t][i].obj);
    keep(&st->filter, 0);
    keep(&st->cfg, 0);
    IMP sup = cx_lookup(cx_class_named("NSObject"), c);
    if (sup) ((void (*)(id, SEL))sup)(s, c);
}
static void stream_start(id s, SEL c, void *blk) {
    (void)c;
    scstream_t *st = ST(s);
    if (st->th) { call_blk1(blk, sc_error(-3808, "O stream já está rodando")); return; }
    if (cx_tcc_preflight("kTCCServiceScreenCapture") != 1) { call_blk1(blk, sc_error(-3801, "Sem permissão de gravação da tela")); return; }
    if (st->spec.cfg.mic && cx_tcc_preflight("kTCCServiceMicrophone") != 1) { call_blk1(blk, sc_error(-3801, "Sem permissão do microfone")); return; }
    st->hash = 0; st->audio_pos = 0; st->mic_frames = 0;
    st->run = 1; st->done = 0;
    if (pthread_create(&st->th, 0, stream_thread, st)) { st->run = 0; st->th = 0; call_blk1(blk, sc_error(-3802, "Falha ao iniciar a captura")); return; }
    call_blk1(blk, 0);
}
static void stream_stop(id s, SEL c, void *blk) {
    (void)c;
    scstream_t *st = ST(s);
    if (!st->th) { call_blk1(blk, sc_error(-3809, "O stream não está rodando")); return; }
    stream_halt(st);
    call_blk1(blk, 0);
}
static BOOL stream_add_out(id s, SEL c, id output, i64 type, void *queue, id *err) {
    (void)c;
    if (err) *err = 0;
    scstream_t *st = ST(s);
    if (!output || type < 0 || type > 2) { if (err) *err = sc_error(-3813, "Saída inválida"); return NO; }
    spin(&st->lock);
    int ok = 0;
    for (int i = 0; i < MAX_OUT && !ok; i++) if (!st->out[type][i].obj) { st->out[type][i].obj = objc_retain(output); st->out[type][i].queue = queue; st->out[type][i].inflight = 0; ok = 1; }
    unspin(&st->lock);
    if (!ok && err) *err = sc_error(-3813, "Saídas demais");
    return ok ? YES : NO;
}
static BOOL stream_remove_out(id s, SEL c, id output, i64 type, id *err) {
    (void)c;
    if (err) *err = 0;
    scstream_t *st = ST(s);
    if (type < 0 || type > 2) return NO;
    int ok = 0;
    spin(&st->lock);
    for (int i = 0; i < MAX_OUT; i++) if (st->out[type][i].obj == output && output) { objc_release(output); st->out[type][i].obj = 0; ok = 1; }
    unspin(&st->lock);
    if (!ok && err) *err = sc_error(-3813, "Saída não encontrada");
    return ok ? YES : NO;
}
static BOOL stream_remove_out2(id s, SEL c, id output, i64 type) { return stream_remove_out(s, c, output, type, 0); }
/* a thread lê spec a cada quadro: trocar a cópia inteira entre dois quadros basta */
static void stream_update_cfg(id s, SEL c, id cfg, void *blk) {
    (void)c;
    scstream_t *st = ST(s);
    keep(&st->cfg, cfg);
    sckspec_t sp;
    spec_from(&sp, st->filter, cfg);
    spin(&st->lock);
    st->spec = sp;
    st->dirty = 1;
    unspin(&st->lock);
    call_blk1(blk, 0);
}
static void stream_update_filter(id s, SEL c, id filter, void *blk) {
    (void)c;
    scstream_t *st = ST(s);
    keep(&st->filter, filter);
    sckspec_t sp;
    spec_from(&sp, filter, st->cfg);
    spin(&st->lock);
    st->spec = sp;
    st->dirty = 1;
    unspin(&st->lock);
    call_blk1(blk, 0);
}
static void *stream_clock(id s, SEL c) { (void)s; (void)c; return CMClockGetHostTimeClock(); }

/* ---------------------------------------------------------------- SCScreenshotManager */
static void img_free(void *info, const void *data, size_t n) { (void)info; (void)n; free((void *)data); }
static void *image_of(u32 *bgra, int w, int h) {
    void *prov = CGDataProviderCreateWithData(0, bgra, (size_t)w * h * 4, img_free);
    void *cs = CGColorSpaceCreateDeviceRGB();
    void *img = CGImageCreate((size_t)w, (size_t)h, 8, 32, (size_t)w * 4, cs, 2 /* premultipliedFirst */ | 0x2000 /* little */, prov, 0, 1, 0);
    CGColorSpaceRelease(cs);
    CGDataProviderRelease(prov);
    return img;
}
typedef struct { void *blk; void *obj; int kind; long err; } shotjob_t;   /* kind 0 CGImage, 1 CMSampleBuffer */
static void shot_run(void *a) {
    shotjob_t *j = a;
    call_blk2(j->blk, j->obj, j->obj ? 0 : sc_error(j->err ? j->err : -3802, "Falha ao capturar a tela"));
    if (j->obj) { if (j->kind == 0) CGImageRelease(j->obj); else objc_release((id)j->obj); }
    _Block_release(j->blk);
    free(j);
}
static void shot(id filter, id cfg, void *blk, int kind) {
    if (!blk) return;
    shotjob_t *j = calloc(1, sizeof *j);
    if (!j) return;
    j->blk = _Block_copy(blk); j->kind = kind;
    if (cx_tcc_preflight("kTCCServiceScreenCapture") != 1) j->err = -3801;
    else {
        sckspec_t sp;
        spec_from(&sp, filter, cfg);
        u32 *px = 0;
        int w, h;
        if (!grab(&sp, &px, &w, &h)) {
            if (kind == 1) j->obj = frame_sample(&sp, px, w, h, 0);
            else {
                int ow, oh;
                double cs;
                screct_t content;
                u32 *out = place(&sp, px, w, h, &ow, &oh, &cs, &content);
                if (out) j->obj = image_of(out, ow, oh);
            }
        }
        free(px);
    }
    dispatch_async_f(dispatch_get_global_queue(0, 0), j, shot_run);
}
static void shot_image(id k, SEL c, id filter, id cfg, void *blk) { (void)k; (void)c; shot(filter, cfg, blk, 0); }
static void shot_sample(id k, SEL c, id filter, id cfg, void *blk) { (void)k; (void)c; shot(filter, cfg, blk, 1); }
static void shot_rect(id k, SEL c, screct_t r, void *blk) {
    (void)k; (void)c;
    sccfg_t cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.isa = &SCStreamConfiguration_c;
    cfg_init((id)&cfg, 0);
    cfg.source = r;
    shot(0, (id)&cfg, blk, 0);
}

/* ---------------------------------------------------------------- registro */
static void add(const char *cls, const char *sel, void *fn, int meta) {
    cx_class_t *k = cx_class_named(cls);
    if (!k) return;
    if (meta) cx_add_cmethod(k, sel, fn); else cx_add_method(k, sel, fn);
}
#define DEF(k, n, sz) if (!cx_class_named(n)) cx_define_class(&k##_c, &k##_m, n, cx_class_named("NSObject"), (u32)(sz))
/* antes das cascas do vscode_fwclasses.c: o tamanho certo das instâncias */
void cx_sck_classes_init(void) {
    DEF(SCDisplay, "SCDisplay", sizeof(scdisplay_t) - 8);
    DEF(SCWindow, "SCWindow", sizeof(scwin_t) - 8);
    DEF(SCContentFilter, "SCContentFilter", sizeof(scfilter_t) - 8);
    DEF(SCShareableContent, "SCShareableContent", sizeof(sccontent_t) - 8);
    DEF(SCStreamConfiguration, "SCStreamConfiguration", sizeof(sccfg_t) - 8);
    DEF(SCRunningApplication, "SCRunningApplication", sizeof(scapp_t) - 8);
    DEF(SCStream, "SCStream", sizeof(scstream_t) - 8);
    DEF(SCShareableContentInfo, "SCShareableContentInfo", sizeof(scinfo_t) - 8);
}
void cx_screencapturekit_init(void) {
    cx_sck_classes_init();
    add("SCDisplay", "displayID", (void *)d_id, 0);
    add("SCDisplay", "width", (void *)d_w, 0);
    add("SCDisplay", "height", (void *)d_h, 0);
    add("SCDisplay", "frame", (void *)d_frame, 0);
    add("SCWindow", "windowID", (void *)w_id, 0);
    add("SCWindow", "frame", (void *)w_frame, 0);
    add("SCWindow", "title", (void *)w_title, 0);
    add("SCWindow", "owningApplication", (void *)w_owner, 0);
    add("SCWindow", "isOnScreen", (void *)w_on, 0);
    add("SCWindow", "isActive", (void *)w_active, 0);
    add("SCWindow", "windowLayer", (void *)w_layer, 0);
    add("SCRunningApplication", "processID", (void *)app_pid, 0);
    add("SCRunningApplication", "applicationName", (void *)app_name, 0);
    add("SCRunningApplication", "bundleIdentifier", (void *)app_bundle, 0);
    add("SCShareableContent", "displays", (void *)ct_displays, 0);
    add("SCShareableContent", "windows", (void *)ct_windows, 0);
    add("SCShareableContent", "applications", (void *)ct_apps, 0);
    add("SCShareableContent", "getShareableContentWithCompletionHandler:", (void *)ct_get, 1);
    add("SCShareableContent", "getShareableContentExcludingDesktopWindows:onScreenWindowsOnly:completionHandler:", (void *)ct_get2, 1);
    add("SCShareableContent", "getShareableContentExcludingDesktopWindows:onScreenWindowsOnlyBelowWindow:completionHandler:", (void *)ct_below, 1);
    add("SCShareableContent", "getShareableContentExcludingDesktopWindows:onScreenWindowsOnlyAboveWindow:completionHandler:", (void *)ct_above, 1);
    add("SCShareableContent", "getCurrentProcessShareableContentWithCompletionHandler:", (void *)ct_current, 1);
    add("SCShareableContent", "infoForFilter:", (void *)info_for, 1);
    add("SCShareableContentInfo", "contentRect", (void *)info_rect, 0);
    add("SCShareableContentInfo", "pointPixelScale", (void *)info_scale, 0);
    add("SCShareableContentInfo", "style", (void *)info_style, 0);
    add("SCContentFilter", "initWithDisplay:excludingWindows:", (void *)f_excl_windows, 0);
    add("SCContentFilter", "initWithDisplay:includingWindows:", (void *)f_incl_windows, 0);
    add("SCContentFilter", "initWithDisplay:excludingApplications:exceptingWindows:", (void *)f_excl_apps, 0);
    add("SCContentFilter", "initWithDisplay:includingApplications:exceptingWindows:", (void *)f_incl_apps, 0);
    add("SCContentFilter", "initWithDesktopIndependentWindow:", (void *)f_window, 0);
    add("SCContentFilter", "contentRect", (void *)f_rect, 0);
    add("SCContentFilter", "pointPixelScale", (void *)f_scale, 0);
    add("SCContentFilter", "style", (void *)f_style, 0);
    add("SCContentFilter", "streamType", (void *)f_stream_type, 0);
    add("SCContentFilter", "includeMenuBar", (void *)f_menubar, 0);
    add("SCContentFilter", "setIncludeMenuBar:", (void *)f_set_menubar, 0);
    add("SCContentFilter", "includedWindows", (void *)f_incl_w, 0);
    add("SCContentFilter", "includedApplications", (void *)f_incl_a, 0);
    add("SCContentFilter", "includedDisplays", (void *)f_incl_d, 0);
    struct { const char *g, *s; void *gf, *sf; } props[] = {
        {"width", "setWidth:", (void *)cfg_w, (void *)cfg_set_w}, {"height", "setHeight:", (void *)cfg_h, (void *)cfg_set_h},
        {"queueDepth", "setQueueDepth:", (void *)cfg_qd, (void *)cfg_set_qd}, {"sampleRate", "setSampleRate:", (void *)cfg_rate, (void *)cfg_set_rate},
        {"channelCount", "setChannelCount:", (void *)cfg_ch, (void *)cfg_set_ch}, {"captureResolution", "setCaptureResolution:", (void *)cfg_res, (void *)cfg_set_res},
        {"showsCursor", "setShowsCursor:", (void *)cfg_cursor, (void *)cfg_set_cursor}, {"scalesToFit", "setScalesToFit:", (void *)cfg_scales, (void *)cfg_set_scales},
        {"preservesAspectRatio", "setPreservesAspectRatio:", (void *)cfg_preserve, (void *)cfg_set_preserve},
        {"capturesAudio", "setCapturesAudio:", (void *)cfg_audio, (void *)cfg_set_audio},
        {"captureMicrophone", "setCaptureMicrophone:", (void *)cfg_mic, (void *)cfg_set_mic},
        {"excludesCurrentProcessAudio", "setExcludesCurrentProcessAudio:", (void *)cfg_exself, (void *)cfg_set_exself},
        {"shouldBeOpaque", "setShouldBeOpaque:", (void *)cfg_opaque, (void *)cfg_set_opaque},
        {"capturesShadowsOnly", "setCapturesShadowsOnly:", (void *)cfg_shonly, (void *)cfg_set_shonly},
        {"showMouseClicks", "setShowMouseClicks:", (void *)cfg_clicks, (void *)cfg_set_clicks},
        {"includeChildWindows", "setIncludeChildWindows:", (void *)cfg_child, (void *)cfg_set_child},
        {"ignoreShadowsSingleWindow", "setIgnoreShadowsSingleWindow:", (void *)cfg_igsw, (void *)cfg_set_igsw},
        {"ignoreShadowsDisplay", "setIgnoreShadowsDisplay:", (void *)cfg_igsd, (void *)cfg_set_igsd},
        {"ignoreGlobalClipSingleWindow", "setIgnoreGlobalClipSingleWindow:", (void *)cfg_igcw, (void *)cfg_set_igcw},
        {"ignoreGlobalClipDisplay", "setIgnoreGlobalClipDisplay:", (void *)cfg_igcd, (void *)cfg_set_igcd},
        {"sourceRect", "setSourceRect:", (void *)cfg_src, (void *)cfg_set_src}, {"destinationRect", "setDestinationRect:", (void *)cfg_dst, (void *)cfg_set_dst},
        {"colorSpaceName", "setColorSpaceName:", (void *)cfg_cs, (void *)cfg_set_cs}, {"colorMatrix", "setColorMatrix:", (void *)cfg_matrix, (void *)cfg_set_matrix},
        {"streamName", "setStreamName:", (void *)cfg_name, (void *)cfg_set_name},
        {"microphoneCaptureDeviceID", "setMicrophoneCaptureDeviceID:", (void *)cfg_micdev, (void *)cfg_set_micdev},
        {"pixelFormat", "setPixelFormat:", (void *)cfg_pix, (void *)cfg_set_pix},
        {"minimumFrameInterval", "setMinimumFrameInterval:", (void *)cfg_interval, (void *)cfg_set_interval},
        {"backgroundColor", "setBackgroundColor:", (void *)cfg_bg, (void *)cfg_set_bg},
        {"captureDynamicRange", "setCaptureDynamicRange:", (void *)cfg_dynrange, (void *)cfg_set_dynrange},
    };
    add("SCStreamConfiguration", "init", (void *)cfg_init, 0);
    for (unsigned i = 0; i < sizeof props / sizeof props[0]; i++) {
        add("SCStreamConfiguration", props[i].g, props[i].gf, 0);
        add("SCStreamConfiguration", props[i].s, props[i].sf, 0);
    }
    add("SCStream", "initWithFilter:configuration:delegate:", (void *)stream_init, 0);
    add("SCStream", "dealloc", (void *)stream_dealloc, 0);
    add("SCStream", "startCaptureWithCompletionHandler:", (void *)stream_start, 0);
    add("SCStream", "stopCaptureWithCompletionHandler:", (void *)stream_stop, 0);
    add("SCStream", "addStreamOutput:type:sampleHandlerQueue:error:", (void *)stream_add_out, 0);
    add("SCStream", "removeStreamOutput:type:error:", (void *)stream_remove_out, 0);
    add("SCStream", "removeStreamOutput:type:", (void *)stream_remove_out2, 0);
    add("SCStream", "updateConfiguration:completionHandler:", (void *)stream_update_cfg, 0);
    add("SCStream", "updateContentFilter:completionHandler:", (void *)stream_update_filter, 0);
    add("SCStream", "synchronizationClock", (void *)stream_clock, 0);
    add("SCScreenshotManager", "captureImageWithFilter:configuration:completionHandler:", (void *)shot_image, 1);
    add("SCScreenshotManager", "captureSampleBufferWithFilter:configuration:completionHandler:", (void *)shot_sample, 1);
    add("SCScreenshotManager", "captureImageInRect:completionHandler:", (void *)shot_rect, 1);
}
