/*
 * mOS libSystem — ScreenCaptureKit mínimo para capturas de tela (os helpers de "computer use": Grok Bot, Codex, Claude):
 *   +[SCShareableContent getShareableContentWithCompletionHandler:] (e as variantes com filtros): um SCDisplay com a tela
 *     composta do mOS (todos os monitores num espaço só) e as janelas do compositor (DEV_WINDOW_LIST);
 *   SCContentFilter (initWithDisplay:excludingWindows: e afins) e SCStreamConfiguration (largura/altura/sourceRect);
 *   +[SCScreenshotManager captureImageWithFilter:configuration:completionHandler:] e +captureImageInRect:completionHandler:
 *     -> CGDisplayCreateImage / CGDisplayCreateImageForRect (cgevent.c), entregues na fila global.
 *   SCStream entrega quadros contínuos pelo mesmo DEV_SCREEN_CAPTURE, em CMSampleBuffer (screencap_prod.c).
 *   Sem kTCCServiceScreenCapture o start não lê pixels. CGDisplayStream está em cgdisplaystream.c.
 * As classes vêm de vscode_fwclasses.c; SCStream e SCRunningApplication são definidas aqui. Os métodos entram em
 * cx_screencapturekit_init (chamado no fim do cx_objc_boot, depois de todas as classes).
 */
#include "foundation.h"
#include "sys.h"
#include "../../kernel/include/compat_gui.h"
#include "computeruse_win.h"
#include "screencap_run.h"

void *memset(void *d, int c, size_t n);
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
void CGImageRelease(void *img);

typedef struct { double x, y, w, h; } screct_t;
void *CGDisplayCreateImage(u32 display);
void *CGDisplayCreateImageForRect(u32 display, screct_t r);

static screct_t screen_rect(void) {
    long sz = (long)SYS4(GUI_DEVICE, DEV_SCREEN_CAPTURE, 0, 0, 0);    /* buf 0: só o tamanho da tela inteira */
    screct_t r = {0, 0, 0, 0};
    if (sz > 0) { r.w = (double)(u32)sz; r.h = (double)(u32)((u64)sz >> 32); }
    return r;
}

/* ---- objetos: {isa; campos} em classes já definidas (vazias) com 32 bytes extras */
typedef struct { cx_class_t *isa; screct_t frame; } scdisplay_t;          /* SCDisplay */
typedef struct { cx_class_t *isa; id displays, windows, apps; } sccontent_t;  /* SCShareableContent: cabe em 32 bytes */
typedef struct { cx_class_t *isa; screct_t rect; } scfilter_t;            /* SCContentFilter: área da captura */
typedef struct { cx_class_t *isa; i64 w, h, interval_us; } sccfg_t;       /* SCStreamConfiguration */
typedef struct { cx_class_t *isa; u32 wid; i32 pid; float x, y, w, h; int on; } scwin_t; /* SCWindow: cabe nos 32 bytes extras */
typedef struct { cx_class_t *isa; i32 pid; } scapp_t;                    /* SCRunningApplication */

int cx_tcc_preflight(const char *service);
cx_class_t SCRunningApplication_c __asm__("OBJC_CLASS_$_SCRunningApplication");
cx_class_t SCRunningApplication_m __asm__("OBJC_METACLASS_$_SCRunningApplication");
cx_class_t SCStream_c __asm__("OBJC_CLASS_$_SCStream");
cx_class_t SCStream_m __asm__("OBJC_METACLASS_$_SCStream");
static void ensure_app_class(void) {
    if (cx_class_named("SCRunningApplication")) return;
    cx_define_class(&SCRunningApplication_c, &SCRunningApplication_m, "SCRunningApplication", cx_class_named("NSObject"), 16);
}

static id new_of(const char *cls) { cx_class_t *c = cx_class_named(cls); return c ? cx_alloc_instance(c) : 0; }

/* SCDisplay */
static u32 d_id(id s, SEL c) { (void)s; (void)c; return 1; }
static long d_w(id s, SEL c) { (void)c; return (long)((scdisplay_t *)s)->frame.w; }
static long d_h(id s, SEL c) { (void)c; return (long)((scdisplay_t *)s)->frame.h; }
static screct_t d_frame(id s, SEL c) { (void)c; return ((scdisplay_t *)s)->frame; }

/* SCWindow / SCRunningApplication: dados da lista do compositor */
static int sc_find(u32 wid, gui_cgwindow_t *out) {
    for (int i = 0; i < 64; i++) {
        if (!cx_cg_window_at(i, out)) return 0;
        if (out->number == wid) return 1;
    }
    return 0;
}
static u32 w_id(id s, SEL c) { (void)c; return ((scwin_t *)s)->wid; }
static screct_t w_frame(id s, SEL c) {
    (void)c;
    scwin_t *w = (scwin_t *)s;
    screct_t r = {w->x, w->y, w->w, w->h};
    return r;
}
static u8 w_on(id s, SEL c) { (void)c; return (u8)((scwin_t *)s)->on; }
static id w_title(id s, SEL c) {
    (void)c;
    gui_cgwindow_t info;
    if (!sc_find(((scwin_t *)s)->wid, &info)) return objc_autorelease(cx_str(""));
    return objc_autorelease(cx_str(info.name));
}
static id w_owner(id s, SEL c) {
    (void)c;
    ensure_app_class();
    scapp_t *a = (scapp_t *)cx_alloc_instance(cx_class_named("SCRunningApplication"));
    if (!a) return 0;
    a->pid = ((scwin_t *)s)->pid;
    return objc_autorelease((id)a);
}
static long app_pid(id s, SEL c) { (void)c; return ((scapp_t *)s)->pid; }
static id app_name(id s, SEL c) {
    (void)c;
    i32 pid = ((scapp_t *)s)->pid;
    gui_cgwindow_t info;
    for (int i = 0; i < 64; i++) {
        if (!cx_cg_window_at(i, &info)) break;
        if (info.pid == pid && info.owner[0]) return objc_autorelease(cx_str(info.owner));
    }
    return objc_autorelease(cx_str(""));
}
static id app_bundle(id s, SEL c) { (void)s; (void)c; return objc_autorelease(cx_str("")); }

/* SCShareableContent. Sem kTCCServiceScreenCapture a lista de janelas fica vazia (o display, não). */
static id content_make(int exclude_desktop, int onscreen_only) {
    sccontent_t *ct = (sccontent_t *)new_of("SCShareableContent");
    scdisplay_t *d = (scdisplay_t *)new_of("SCDisplay");
    if (!ct || !d) return 0;
    d->frame = screen_rect();
    cx_class_t *arr = cx_class_named("NSMutableArray");
    id windows = objc_retain(MSG0(id, (id)arr, "array"));
    id apps = objc_retain(MSG0(id, (id)arr, "array"));
    ct->displays = objc_retain(MSG(id, (id)cx_class_named("NSArray"), "arrayWithObject:", (id)d));
    objc_release((id)d);
    int granted = cx_tcc_preflight("kTCCServiceScreenCapture");
    int seen_pid[48], np = 0;
    if (granted) {
        for (int i = 0; i < 48; i++) {
            gui_cgwindow_t info;
            if (!cx_cg_window_at(i, &info)) break;
            if (exclude_desktop && info.pid == 0) continue;
            if (onscreen_only && !info.onscreen) continue;
            scwin_t *win = (scwin_t *)new_of("SCWindow");
            if (!win) continue;
            win->wid = info.number;
            win->pid = info.pid;
            win->x = (float)info.x;
            win->y = (float)info.y;
            win->w = (float)info.w;
            win->h = (float)info.h;
            win->on = info.onscreen;
            MSG(void, windows, "addObject:", (id)win);
            objc_release((id)win);
            if (info.pid == 0) continue;
            int dup = 0;
            for (int k = 0; k < np; k++) if (seen_pid[k] == info.pid) dup = 1;
            if (dup || np >= 48) continue;
            seen_pid[np++] = info.pid;
            ensure_app_class();
            scapp_t *ap = (scapp_t *)cx_alloc_instance(cx_class_named("SCRunningApplication"));
            if (!ap) continue;
            ap->pid = info.pid;
            MSG(void, apps, "addObject:", (id)ap);
            objc_release((id)ap);
        }
    }
    ct->windows = windows;
    ct->apps = apps;
    return objc_autorelease((id)ct);
}
static id ct_displays(id s, SEL c) { (void)c; return ((sccontent_t *)s)->displays; }
static id ct_windows(id s, SEL c) { (void)c; return ((sccontent_t *)s)->windows; }
static id ct_apps(id s, SEL c) { (void)c; return ((sccontent_t *)s)->apps; }

/* blocos: {isa, flags, reserved, invoke, ...}; invoke recebe o próprio bloco primeiro */
typedef struct { void *blk; id a; id b; void *img; int kind; } job_t;
static void run_job(void *arg) {
    job_t *j = arg;
    void *inv = *(void **)((u8 *)j->blk + 16);
    if (j->kind == 0) ((void (*)(void *, id, id))inv)(j->blk, j->a, 0);                  /* (content, error) */
    else ((void (*)(void *, void *, id))inv)(j->blk, j->img, j->img ? 0 : j->b);           /* (CGImage, error) */
    if (j->a) objc_release(j->a);
    if (j->img) CGImageRelease(j->img);
    _Block_release(j->blk);
    free(j);
}
static void later(void *blk, int kind, id a, void *img) {
    if (!blk) { if (img) CGImageRelease(img); return; }
    job_t *j = malloc(sizeof *j);
    if (!j) return;
    memset(j, 0, sizeof *j);
    j->blk = _Block_copy(blk); j->kind = kind; j->a = a ? objc_retain(a) : 0; j->img = img;
    if (kind && !img) {
        id ui = MSG(id, (id)cx_class_named("NSDictionary"), "dictionaryWithObject:forKey:", cx_str("Falha ao capturar a tela"), cx_str("NSLocalizedDescription"));
        j->b = MSG(id, (id)cx_class_named("NSError"), "errorWithDomain:code:userInfo:", cx_str("com.apple.ScreenCaptureKit.SCStreamErrorDomain"), (long)-3801, ui);
    }
    dispatch_async_f(dispatch_get_global_queue(0, 0), j, run_job);
}
static void ct_get(id cls, SEL c, void *blk) { (void)cls; (void)c; later(blk, 0, content_make(0, 0), 0); }
static void ct_get2(id cls, SEL c, u8 exclude_desktop, u8 onscreen, void *blk) {
    (void)cls; (void)c; later(blk, 0, content_make(exclude_desktop, onscreen), 0);
}
/* Acima/abaixo de uma janela âncora: só o recorte on-screen. A âncora em si não filtra (stub dessa variante). */
static void ct_get3(id cls, SEL c, u8 exclude_desktop, id w, void *blk) {
    (void)cls; (void)c; (void)w; later(blk, 0, content_make(exclude_desktop, 1), 0);
}

/* SCContentFilter: a área é a da tela (com janelas excluídas ou não: o mOS captura a tela composta) */
static id f_init_display(id s, SEL c, id display) {
    (void)c;
    ((scfilter_t *)s)->rect = display ? ((scdisplay_t *)display)->frame : screen_rect();
    return s;
}
static id f_init_display2(id s, SEL c, id display, id list) { (void)list; return f_init_display(s, c, display); }
static id f_init_display3(id s, SEL c, id display, id apps, id except) { (void)apps; (void)except; return f_init_display(s, c, display); }
static id f_init_window(id s, SEL c, id win) {
    (void)c;
    scwin_t *w = (scwin_t *)win;
    screct_t r = screen_rect();
    if (w && w->w > 0 && w->h > 0) { r.x = w->x; r.y = w->y; r.w = w->w; r.h = w->h; }
    ((scfilter_t *)s)->rect = r;
    return s;
}
static screct_t f_rect(id s, SEL c) { (void)c; return ((scfilter_t *)s)->rect; }

/* SCStreamConfiguration */
static long cfg_w(id s, SEL c) { (void)c; return ((sccfg_t *)s)->w; }
static long cfg_h(id s, SEL c) { (void)c; return ((sccfg_t *)s)->h; }
static void cfg_set_w(id s, SEL c, long v) { (void)c; ((sccfg_t *)s)->w = v; }
static void cfg_set_h(id s, SEL c, long v) { (void)c; ((sccfg_t *)s)->h = v; }
typedef struct { i64 value; i32 timescale; u32 flags; i64 epoch; } sc_time_t;
static sc_time_t cfg_interval(id s, SEL c) {
    (void)c;
    long long us = ((sccfg_t *)s)->interval_us;
    if (us <= 0) us = 16667;
    sc_time_t t = {us, 1000000, 1, 0};
    return t;
}
static void cfg_set_interval(id s, SEL c, sc_time_t t) {
    (void)c;
    long long us = 16667;
    if ((t.flags & 1) && t.timescale) us = (long long)((double)t.value * 1000000.0 / (double)t.timescale);
    if (us < 0) us = 0;
    ((sccfg_t *)s)->interval_us = us;
}
static void noop_bool(id s, SEL c, u8 v) { (void)s; (void)c; (void)v; }
static void noop_long(id s, SEL c, long v) { (void)s; (void)c; (void)v; }
static void noop_obj(id s, SEL c, id v) { (void)s; (void)c; (void)v; }
static void noop_rect(id s, SEL c, screct_t r) { (void)s; (void)c; (void)r; }
/* SCScreenshotManager */
static void shot_filter(id cls, SEL c, id filter, id cfg, void *blk) {
    (void)cls; (void)c; (void)cfg;
    screct_t r = filter ? ((scfilter_t *)filter)->rect : screen_rect();
    later(blk, 1, 0, r.w > 0 ? CGDisplayCreateImageForRect(1, r) : CGDisplayCreateImage(1));
}
static void shot_rect(id cls, SEL c, screct_t r, void *blk) { (void)cls; (void)c; later(blk, 1, 0, CGDisplayCreateImageForRect(1, r)); }

/* SCStream: o estado fica no heap (não cabe nos 32 bytes de uma classe vazia). */
typedef struct {
    id delegate, output, self;
} sc_out_t;
typedef struct { cx_class_t *isa; cx_cap_run *run; } scstream_t;

static void sc_on_frame(void *ctx, id surf, id pb, id sb) {
    (void)surf; (void)pb;
    sc_out_t *o = ctx;
    if (!o || !sb) return;
    if (o->delegate) MSG(void, o->delegate, "stream:didOutputSampleBuffer:ofType:", o->self, sb, (long)0);
    if (o->output) MSG(void, o->output, "stream:didOutputSampleBuffer:ofType:", o->self, sb, (long)0);
}
static void stream_free(scstream_t *st) {
    if (!st || !st->run) return;
    cx_cap_stop(st->run);
    sc_out_t *o = st->run->ctx;
    if (o) {
        if (o->delegate) objc_release(o->delegate);
        if (o->output) objc_release(o->output);
        free(o);
    }
    free(st->run->cap.px);
    free(st->run);
    st->run = 0;
}
static void stream_dealloc(id s, SEL c) {
    stream_free((scstream_t *)s);
    IMP sup = cx_lookup(cx_class_named("NSObject"), c);
    if (sup) ((void (*)(id, SEL))sup)(s, c);
}
static void ensure_stream_class(void) {
    if (cx_class_named("SCStream")) return;
    cx_define_class(&SCStream_c, &SCStream_m, "SCStream", cx_class_named("NSObject"), (u32)(sizeof(scstream_t) - sizeof(void *)));
    cx_add_method(&SCStream_c, "dealloc", (void *)stream_dealloc);
}
static id stream_init(id s, SEL c, id filter, id cfg, id delegate) {
    (void)c;
    ensure_stream_class();
    scstream_t *st = (scstream_t *)s;
    stream_free(st);
    sc_out_t *o = calloc(1, sizeof *o);
    cx_cap_run *r = calloc(1, sizeof *r);
    int cap = 4096 * 2160;
    unsigned *px = calloc((size_t)cap, sizeof(unsigned));
    if (!o || !r || !px) { free(o); free(r); free(px); return 0; }
    o->delegate = delegate ? objc_retain(delegate) : 0;
    o->self = s;
    r->cap.px = px;
    r->cap.cap = cap;
    r->on_frame = sc_on_frame;
    r->ctx = o;
    if (cfg) {
        r->w = (int)((sccfg_t *)cfg)->w;
        r->h = (int)((sccfg_t *)cfg)->h;
        r->cap.interval_us = ((sccfg_t *)cfg)->interval_us;
    }
    if (filter) {
        screct_t rc = ((scfilter_t *)filter)->rect;
        r->x = (int)rc.x;
        r->y = (int)rc.y;
        if (!r->w && rc.w > 0 && rc.h > 0) { r->w = (int)rc.w; r->h = (int)rc.h; }
    }
    st->run = r;
    return s;
}
static void call_errblk(void *blk, id err) {
    if (!blk) return;
    void *inv = *(void **)((u8 *)blk + 16);
    if (inv) ((void (*)(void *, id))inv)(blk, err);
}
static id stream_err(void) {
    id ui = MSG(id, (id)cx_class_named("NSDictionary"), "dictionaryWithObject:forKey:", cx_str("Falha ao iniciar SCStream"), cx_str("NSLocalizedDescription"));
    return MSG(id, (id)cx_class_named("NSError"), "errorWithDomain:code:userInfo:", cx_str("com.apple.ScreenCaptureKit.SCStreamErrorDomain"), (long)-3801, ui);
}
static void stream_start(id s, SEL c, void *blk) {
    (void)c;
    scstream_t *st = (scstream_t *)s;
    int rc = st->run ? cx_cap_start(st->run) : -1;
    call_errblk(blk, rc ? stream_err() : 0);
}
static void stream_stop(id s, SEL c, void *blk) {
    (void)c;
    cx_cap_stop(((scstream_t *)s)->run);
    call_errblk(blk, 0);
}
static u8 stream_add_out(id s, SEL c, id output, long type, void *queue, id *err) {
    (void)c; (void)type; (void)queue;
    if (err) *err = 0;
    scstream_t *st = (scstream_t *)s;
    sc_out_t *o = st->run ? st->run->ctx : 0;
    if (!o || !output) return 0;
    if (o->output) objc_release(o->output);
    o->output = objc_retain(output);
    return 1;
}
static u8 stream_remove_out(id s, SEL c, id output, long type) {
    (void)c; (void)output; (void)type;
    scstream_t *st = (scstream_t *)s;
    sc_out_t *o = st->run ? st->run->ctx : 0;
    if (!o || !o->output) return 0;
    objc_release(o->output);
    o->output = 0;
    return 1;
}

static void add(const char *cls, const char *sel, void *fn, int meta) {
    cx_class_t *k = cx_class_named(cls);
    if (!k) return;
    if (meta) cx_add_cmethod(k, sel, fn); else cx_add_method(k, sel, fn);
}
void cx_screencapturekit_init(void) {
    ensure_stream_class();
    add("SCDisplay", "displayID", (void *)d_id, 0);
    add("SCDisplay", "width", (void *)d_w, 0);
    add("SCDisplay", "height", (void *)d_h, 0);
    add("SCDisplay", "frame", (void *)d_frame, 0);
    add("SCWindow", "windowID", (void *)w_id, 0);
    add("SCWindow", "frame", (void *)w_frame, 0);
    add("SCWindow", "title", (void *)w_title, 0);
    add("SCWindow", "owningApplication", (void *)w_owner, 0);
    add("SCWindow", "isOnScreen", (void *)w_on, 0);
    ensure_app_class();
    add("SCRunningApplication", "processID", (void *)app_pid, 0);
    add("SCRunningApplication", "applicationName", (void *)app_name, 0);
    add("SCRunningApplication", "bundleIdentifier", (void *)app_bundle, 0);
    add("SCShareableContent", "displays", (void *)ct_displays, 0);
    add("SCShareableContent", "windows", (void *)ct_windows, 0);
    add("SCShareableContent", "applications", (void *)ct_apps, 0);
    add("SCShareableContent", "getShareableContentWithCompletionHandler:", (void *)ct_get, 1);
    add("SCShareableContent", "getShareableContentExcludingDesktopWindows:onScreenWindowsOnly:completionHandler:", (void *)ct_get2, 1);
    add("SCShareableContent", "getShareableContentExcludingDesktopWindows:onScreenWindowsOnlyBelowWindow:completionHandler:", (void *)ct_get3, 1);
    add("SCShareableContent", "getShareableContentExcludingDesktopWindows:onScreenWindowsOnlyAboveWindow:completionHandler:", (void *)ct_get3, 1);
    add("SCShareableContent", "getCurrentProcessShareableContentWithCompletionHandler:", (void *)ct_get, 1);
    add("SCContentFilter", "initWithDisplay:excludingWindows:", (void *)f_init_display2, 0);
    add("SCContentFilter", "initWithDisplay:includingWindows:", (void *)f_init_display2, 0);
    add("SCContentFilter", "initWithDisplay:excludingApplications:exceptingWindows:", (void *)f_init_display3, 0);
    add("SCContentFilter", "initWithDisplay:includingApplications:exceptingWindows:", (void *)f_init_display3, 0);
    add("SCContentFilter", "initWithDesktopIndependentWindow:", (void *)f_init_window, 0);
    add("SCContentFilter", "contentRect", (void *)f_rect, 0);
    add("SCStreamConfiguration", "width", (void *)cfg_w, 0);
    add("SCStreamConfiguration", "height", (void *)cfg_h, 0);
    add("SCStreamConfiguration", "setWidth:", (void *)cfg_set_w, 0);
    add("SCStreamConfiguration", "setHeight:", (void *)cfg_set_h, 0);
    add("SCStreamConfiguration", "minimumFrameInterval", (void *)cfg_interval, 0);
    add("SCStreamConfiguration", "setMinimumFrameInterval:", (void *)cfg_set_interval, 0);
    add("SCStream", "initWithFilter:configuration:delegate:", (void *)stream_init, 0);
    add("SCStream", "startCaptureWithCompletionHandler:", (void *)stream_start, 0);
    add("SCStream", "stopCaptureWithCompletionHandler:", (void *)stream_stop, 0);
    add("SCStream", "addStreamOutput:type:sampleHandlerQueue:error:", (void *)stream_add_out, 0);
    add("SCStream", "removeStreamOutput:type:", (void *)stream_remove_out, 0);
    add("SCStreamConfiguration", "setShowsCursor:", (void *)noop_bool, 0);
    add("SCStreamConfiguration", "setScalesToFit:", (void *)noop_bool, 0);
    add("SCStreamConfiguration", "setCapturesAudio:", (void *)noop_bool, 0);
    add("SCStreamConfiguration", "setIgnoreShadowsSingleWindow:", (void *)noop_bool, 0);
    add("SCStreamConfiguration", "setPixelFormat:", (void *)noop_long, 0);
    add("SCStreamConfiguration", "setColorSpaceName:", (void *)noop_obj, 0);
    add("SCStreamConfiguration", "setSourceRect:", (void *)noop_rect, 0);
    add("SCStreamConfiguration", "setDestinationRect:", (void *)noop_rect, 0);
    add("SCScreenshotManager", "captureImageWithFilter:configuration:completionHandler:", (void *)shot_filter, 1);
    add("SCScreenshotManager", "captureImageInRect:completionHandler:", (void *)shot_rect, 1);
}
