/*
 * mOS libSystem — ScreenCaptureKit mínimo para capturas de tela (os helpers de "computer use": Grok Bot, Codex, Claude):
 *   +[SCShareableContent getShareableContentWithCompletionHandler:] (e as variantes com filtros): um SCDisplay com a tela
 *     composta do mOS (todos os monitores num espaço só) e as janelas do compositor (DEV_WINDOW_LIST);
 *   SCContentFilter (initWithDisplay:excludingWindows: e afins) e SCStreamConfiguration (largura/altura/sourceRect);
 *   +[SCScreenshotManager captureImageWithFilter:configuration:completionHandler:] e +captureImageInRect:completionHandler:
 *     -> CGDisplayCreateImage / CGDisplayCreateImageForRect (cgevent.c), entregues na fila global.
 * SCStream (vídeo contínuo) não existe: estes apps capturam por SCScreenshotManager. As classes vêm vazias de vscode_fwclasses.c; os métodos entram em
 * cx_screencapturekit_init (chamado no fim do cx_objc_boot, depois de todas as classes).
 */
#include "foundation.h"
#include "sys.h"
#include "../../kernel/include/compat_gui.h"
#include "computeruse_win.h"

void *memset(void *d, int c, size_t n);
void *malloc(size_t n);
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
typedef struct { cx_class_t *isa; i64 w, h; } sccfg_t;                   /* SCStreamConfiguration */
typedef struct { cx_class_t *isa; u32 wid; i32 pid; float x, y, w, h; int on; } scwin_t; /* SCWindow: cabe nos 32 bytes extras */
typedef struct { cx_class_t *isa; i32 pid; } scapp_t;                    /* SCRunningApplication */

int cx_tcc_preflight(const char *service);
static cx_class_t SCRunningApplication_c, SCRunningApplication_m;
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

static void add(const char *cls, const char *sel, void *fn, int meta) {
    cx_class_t *k = cx_class_named(cls);
    if (!k) return;
    if (meta) cx_add_cmethod(k, sel, fn); else cx_add_method(k, sel, fn);
}
void cx_screencapturekit_init(void) {
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
