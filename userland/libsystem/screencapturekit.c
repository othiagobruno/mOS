/*
 * mOS libSystem — ScreenCaptureKit mínimo para capturas de tela (os helpers de "computer use": Grok Bot, Codex, Claude):
 *   +[SCShareableContent getShareableContentWithCompletionHandler:] (e as variantes com filtros): um SCDisplay com a tela
 *     composta do mOS (todos os monitores num espaço só) e a lista de janelas vazia;
 *   SCContentFilter (initWithDisplay:excludingWindows: e afins) e SCStreamConfiguration (largura/altura/sourceRect);
 *   +[SCScreenshotManager captureImageWithFilter:configuration:completionHandler:] e +captureImageInRect:completionHandler:
 *     -> CGDisplayCreateImage / CGDisplayCreateImageForRect (cgevent.c), entregues na fila global.
 * SCStream (vídeo contínuo) não existe. As classes vêm vazias de vscode_fwclasses.c; os métodos entram em
 * cx_screencapturekit_init (chamado no fim do cx_objc_boot, depois de todas as classes).
 */
#include "foundation.h"
#include "sys.h"
#include "../../kernel/include/compat_gui.h"

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

static id new_of(const char *cls) { cx_class_t *c = cx_class_named(cls); return c ? cx_alloc_instance(c) : 0; }

/* SCDisplay */
static u32 d_id(id s, SEL c) { (void)s; (void)c; return 1; }
static long d_w(id s, SEL c) { (void)c; return (long)((scdisplay_t *)s)->frame.w; }
static long d_h(id s, SEL c) { (void)c; return (long)((scdisplay_t *)s)->frame.h; }
static screct_t d_frame(id s, SEL c) { (void)c; return ((scdisplay_t *)s)->frame; }

/* SCShareableContent */
static id content_make(void) {
    sccontent_t *ct = (sccontent_t *)new_of("SCShareableContent");
    scdisplay_t *d = (scdisplay_t *)new_of("SCDisplay");
    if (!ct || !d) return 0;
    d->frame = screen_rect();
    cx_class_t *arr = cx_class_named("NSArray");
    ct->displays = objc_retain(MSG(id, (id)arr, "arrayWithObject:", (id)d));
    ct->windows = objc_retain(MSG0(id, (id)arr, "array"));
    ct->apps = objc_retain(MSG0(id, (id)arr, "array"));
    objc_release((id)d);
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
static void ct_get(id cls, SEL c, void *blk) { (void)cls; (void)c; later(blk, 0, content_make(), 0); }
static void ct_get2(id cls, SEL c, u8 a, u8 b, void *blk) { (void)a; (void)b; ct_get(cls, c, blk); }
static void ct_get3(id cls, SEL c, u8 a, id w, void *blk) { (void)a; (void)w; ct_get(cls, c, blk); }

/* SCContentFilter: a área é a da tela (com janelas excluídas ou não: o mOS captura a tela composta) */
static id f_init_display(id s, SEL c, id display) {
    (void)c;
    ((scfilter_t *)s)->rect = display ? ((scdisplay_t *)display)->frame : screen_rect();
    return s;
}
static id f_init_display2(id s, SEL c, id display, id list) { (void)list; return f_init_display(s, c, display); }
static id f_init_display3(id s, SEL c, id display, id apps, id except) { (void)apps; (void)except; return f_init_display(s, c, display); }
static id f_init_window(id s, SEL c, id win) { (void)win; return f_init_display(s, c, 0); }
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
