/*
 * mOS libSystem — NSURL (URLs de arquivo) e a ponte CFURL/CFBundle-URL, para o porte do VS Code.
 * Registrado no boot do runtime por cx_darwin_init (gancho fraco em objc.c: cx_objc_boot).
 * URLs que não são de arquivo guardam só o texto (absoluteString); rede fica para depois.
 */
#include "foundation.h"

size_t strlen(const char *s);
void *memset(void *d, int c, size_t n);
void *malloc(size_t n);
void free(void *p);
int strncmp(const char *a, const char *b, size_t n);
char *strrchr(const char *s, int c);
id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
int cx_fdprintf(int fd, const char *fmt, ...);

typedef struct { cx_class_t *isa; id path; id string; BOOL file; } nsurl_t;
#define URL(o) ((nsurl_t *)(o))

cx_class_t NSURL_c __asm__("OBJC_CLASS_$_NSURL");
cx_class_t NSURL_m __asm__("OBJC_METACLASS_$_NSURL");

static id ns_str(const char *s) { return MSG(id, (id)cx_class_named("NSString"), "stringWithUTF8String:", s); }
static const char *c_str(id s) { return s ? MSG0(const char *, s, "UTF8String") : ""; }

static id url_make(id path, BOOL file, id string) {
    nsurl_t *u = (nsurl_t *)cx_alloc_instance(&NSURL_c);
    u->path = path ? objc_retain(path) : 0;
    u->string = string ? objc_retain(string) : 0;
    u->file = file;
    return (id)u;
}

static id u_file_path(id cls, SEL c, id path) { (void)cls; (void)c; return path ? objc_autorelease(url_make(path, YES, 0)) : 0; }
static id u_file_path_dir(id cls, SEL c, id path, BOOL dir) { (void)dir; return u_file_path(cls, c, path); }
static id u_with_string(id cls, SEL c, id s) {
    (void)cls; (void)c;
    if (!s) return 0;
    const char *t = c_str(s);
    if (!strncmp(t, "file://", 7)) {
        const char *p = t + 7;
        if (!strncmp(p, "localhost", 9)) p += 9;
        return objc_autorelease(url_make(ns_str(p), YES, 0));
    }
    return objc_autorelease(url_make(0, NO, s));
}
static id u_path(id self, SEL c) { (void)c; return URL(self)->path; }
static BOOL u_is_file(id self, SEL c) { (void)c; return URL(self)->file; }
static id u_absolute(id self, SEL c) {
    (void)c;
    if (!URL(self)->file) return URL(self)->string;
    return MSG(id, (id)cx_class_named("NSString"), "stringWithFormat:", ns_str("file://%@"), URL(self)->path);
}
static const char *u_fsrep(id self, SEL c) { (void)c; return c_str(URL(self)->path); }

/* Partes de um URL (RFC 3986: esquema:[//autoridade]caminho[?consulta][#fragmento]), lidas do texto sob demanda:
 * -scheme, -host, -port, -user, -password, -query, -fragment, -resourceSpecifier. O Chromium lê -scheme em cada
 * NSURL que vira GURL (net::GURLWithNSURL); antes caíam no "ainda não implementado" e voltavam nil. O -path de
 * URLs que não são de arquivo continua nil, como antes. */
typedef struct { const char *t; size_t sch, rs, auth, an, q, qn, f, fn; int has_auth, has_q, has_f; } uparts_t;
static void u_split(id self, uparts_t *p) {
    memset(p, 0, sizeof *p);
    const char *t = URL(self)->file ? 0 : c_str(URL(self)->string);
    if (!t) return;
    p->t = t;
    size_t i = 0;
    if ((t[0] >= 'a' && t[0] <= 'z') || (t[0] >= 'A' && t[0] <= 'Z')) {
        while ((t[i] >= 'a' && t[i] <= 'z') || (t[i] >= 'A' && t[i] <= 'Z') || (t[i] >= '0' && t[i] <= '9') ||
               t[i] == '+' || t[i] == '-' || t[i] == '.') i++;
        if (t[i] == ':') { p->sch = i; p->rs = i + 1; }
    }
    size_t r = p->rs, n = strlen(t), end = n;
    for (size_t k = r; k < n; k++) if (t[k] == '#') { p->has_f = 1; p->f = k + 1; p->fn = n - k - 1; end = k; break; }
    for (size_t k = r; k < end; k++) if (t[k] == '?') { p->has_q = 1; p->q = k + 1; p->qn = end - k - 1; break; }
    if (t[r] == '/' && t[r + 1] == '/') {
        p->has_auth = 1; p->auth = r + 2;
        size_t k = p->auth;
        while (k < n && t[k] != '/' && t[k] != '?' && t[k] != '#') k++;
        p->an = k - p->auth;
    }
}
/* autoridade = [usuário[:senha]@]host[:porta]; host IPv6 entre colchetes */
static void u_hostport(const uparts_t *p, size_t *h, size_t *hn, size_t *ui, size_t *uin, long *port) {
    size_t a = p->auth, e = p->auth + p->an;
    *ui = a; *uin = 0; *port = -1;
    for (size_t k = e; k > a; k--) if (p->t[k - 1] == '@') { *uin = k - 1 - a; a = k; break; }
    size_t he = e;
    if (a < e && p->t[a] == '[') {
        size_t k = a; while (k < e && p->t[k] != ']') k++;
        *h = a + 1; *hn = k > a ? k - a - 1 : 0;
        he = k < e ? k + 1 : e;
    } else {
        for (size_t k = e; k > a; k--) if (p->t[k - 1] == ':') { he = k - 1; break; }
        *h = a; *hn = he - a;
    }
    if (he < e && p->t[he] == ':' && he + 1 < e) {
        long v = 0; size_t k = he + 1;
        for (; k < e && p->t[k] >= '0' && p->t[k] <= '9'; k++) v = v * 10 + (p->t[k] - '0');
        if (k == e) *port = v;
    }
}
static id u_scheme(id self, SEL c) {
    (void)c;
    if (URL(self)->file) return ns_str("file");
    uparts_t p; u_split(self, &p);
    return p.sch ? cx_str_n(p.t, p.sch) : 0;
}
static id u_resource_spec(id self, SEL c) {
    (void)c;
    if (URL(self)->file) return MSG(id, (id)cx_class_named("NSString"), "stringWithFormat:", ns_str("//%@"), URL(self)->path);
    uparts_t p; u_split(self, &p);
    return p.t ? ns_str(p.t + p.rs) : 0;
}
static id u_host(id self, SEL c) {
    (void)c;
    uparts_t p; u_split(self, &p);
    if (!p.has_auth) return 0;
    size_t h, hn, ui, uin; long port;
    u_hostport(&p, &h, &hn, &ui, &uin, &port);
    return hn ? cx_str_n(p.t + h, hn) : 0;
}
static id u_port(id self, SEL c) {
    (void)c;
    uparts_t p; u_split(self, &p);
    if (!p.has_auth) return 0;
    size_t h, hn, ui, uin; long port;
    u_hostport(&p, &h, &hn, &ui, &uin, &port);
    return port >= 0 ? MSG(id, (id)cx_class_named("NSNumber"), "numberWithLong:", port) : 0;
}
static id u_userinfo_part(id self, int want_pass) {
    uparts_t p; u_split(self, &p);
    if (!p.has_auth) return 0;
    size_t h, hn, ui, uin; long port;
    u_hostport(&p, &h, &hn, &ui, &uin, &port);
    if (!uin) return 0;
    size_t colon = uin;
    for (size_t k = 0; k < uin; k++) if (p.t[ui + k] == ':') { colon = k; break; }
    if (!want_pass) return cx_str_n(p.t + ui, colon);
    return colon < uin ? cx_str_n(p.t + ui + colon + 1, uin - colon - 1) : 0;
}
static id u_user(id self, SEL c) { (void)c; return u_userinfo_part(self, 0); }
static id u_password(id self, SEL c) { (void)c; return u_userinfo_part(self, 1); }
static id u_query(id self, SEL c) { (void)c; uparts_t p; u_split(self, &p); return p.has_q ? cx_str_n(p.t + p.q, p.qn) : 0; }
static id u_fragment(id self, SEL c) { (void)c; uparts_t p; u_split(self, &p); return p.has_f ? cx_str_n(p.t + p.f, p.fn) : 0; }
static id u_append(id self, SEL c, id comp) {
    (void)c;
    id p = MSG(id, URL(self)->path, "stringByAppendingPathComponent:", comp);
    return objc_autorelease(url_make(p, YES, 0));
}
static id u_append_dir(id self, SEL c, id comp, BOOL dir) { (void)dir; return u_append(self, c, comp); }
static id u_delete_last(id self, SEL c) {
    (void)c;
    id p = MSG0(id, URL(self)->path, "stringByDeletingLastPathComponent");
    return objc_autorelease(url_make(p, YES, 0));
}
static id u_last(id self, SEL c) { (void)c; return MSG0(id, URL(self)->path, "lastPathComponent"); }
static id u_ext(id self, SEL c) { (void)c; return MSG0(id, URL(self)->path, "pathExtension"); }
static id u_std(id self, SEL c) { (void)c; return self; }
static id u_copy(id self, SEL c) { (void)c; return objc_retain(self); }
static BOOL u_equal(id self, SEL c, id o) {
    (void)c;
    if (self == o) return YES;
    if (!o || ((cx_class_t **)o)[0] != &NSURL_c) return NO;
    id a = MSG0(id, self, "absoluteString"), b = MSG0(id, o, "absoluteString");
    return MSG(BOOL, a, "isEqualToString:", b);
}
static u64 u_hash(id self, SEL c) { (void)c; return MSG0(u64, MSG0(id, self, "absoluteString"), "hash"); }
static BOOL u_reachable(id self, SEL c, id *err) {
    (void)c;
    if (err) *err = 0;
    extern int access(const char *p, int m);
    return URL(self)->file && access(c_str(URL(self)->path), 0) == 0;
}

/* NSBundle por URL (em cima dos métodos por caminho do foundation.c) */
static id b_with_url(id cls, SEL c, id url) { (void)c; return url ? MSG(id, cls, "bundleWithPath:", MSG0(id, url, "path")) : 0; }
static id b_url(id self, SEL c) { (void)c; return u_file_path(0, 0, MSG0(id, self, "bundlePath")); }
static id b_exec_url(id self, SEL c) { (void)c; id p = MSG0(id, self, "executablePath"); return p ? u_file_path(0, 0, p) : 0; }
static id b_res_url(id self, SEL c) { (void)c; return u_file_path(0, 0, MSG0(id, self, "resourcePath")); }
static id b_url_for(id self, SEL c, id name, id ext) {
    (void)c;
    id p = MSG(id, self, "pathForResource:ofType:", name, ext);
    return p ? u_file_path(0, 0, p) : 0;
}
static id b_url_for_sub(id self, SEL c, id name, id ext, id sub) {
    (void)c;
    id p = MSG(id, self, "pathForResource:ofType:inDirectory:", name, ext, sub);
    return p ? u_file_path(0, 0, p) : 0;
}

/* NSThread: +isMultiThreaded e +detachNewThreadSelector:toTarget:withObject: (thread real do mOS) */
int pthread_create(unsigned long *t, const void *attr, void *(*fn)(void *), void *arg);
int pthread_detach(unsigned long t);
static volatile int multithreaded;
typedef struct { id target, arg; SEL sel; } detach_t;
static void *detach_main(void *p) {
    detach_t d = *(detach_t *)p;
    free(p);
    ((void (*)(id, SEL, id))objc_msgSend)(d.target, d.sel, d.arg);
    return 0;
}
static BOOL th_multi(id cls, SEL c) { (void)cls; (void)c; return multithreaded; }
/* -threadDictionary: um NSMutableDictionary por thread, num slot fixo da TSD (abaixo das chaves) */
#define TSD_THREAD_DICT 100
static id th_dict(id self, SEL c) {
    (void)self; (void)c;
    extern void **cx_tsd(void);
    void **t = cx_tsd();
    if (!t[TSD_THREAD_DICT]) t[TSD_THREAD_DICT] = objc_retain(MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary"));
    return (id)t[TSD_THREAD_DICT];
}
static void th_detach(id cls, SEL c, SEL sel, id target, id arg) {
    (void)cls; (void)c;
    multithreaded = 1;
    detach_t *d = malloc(sizeof *d);
    if (!d) return;
    d->target = target; d->arg = arg; d->sel = sel;
    unsigned long t;
    if (pthread_create(&t, 0, detach_main, d) == 0) pthread_detach(t);
    else free(d);
}

/* NSThread de verdade por thread: +isMainThread/-isMainThread comparam com a thread principal
 * (cx_main_tid) e +currentThread devolve um objeto por thread (slot fixo da TSD). Antes, toda thread
 * dizia ser a principal e o Chromium montava um MessagePumpNSApplication ([NSApp run]) em threads
 * secundárias, levando trabalho do V8 para a thread errada. */
#define TSD_THREAD_OBJ 101
extern int cx_main_tid;
static int on_main_thread(void) { return !cx_main_tid || (int)SYS0(SYS_GETTID) == cx_main_tid; }
static id main_thread_obj;
static id th_main_obj(id cls, SEL c) {
    (void)c;
    if (!main_thread_obj) main_thread_obj = cx_alloc_instance((cx_class_t *)cls);
    return main_thread_obj;
}
static id th_current_obj(id cls, SEL c) {
    if (on_main_thread()) return th_main_obj(cls, c);
    extern void **cx_tsd(void);
    void **t = cx_tsd();
    if (!t[TSD_THREAD_OBJ]) t[TSD_THREAD_OBJ] = cx_alloc_instance((cx_class_t *)cls);
    return (id)t[TSD_THREAD_OBJ];
}
static BOOL th_cls_is_main(id cls, SEL c) { (void)cls; (void)c; return on_main_thread(); }
static BOOL th_obj_is_main(id self, SEL c) { (void)c; return self == main_thread_obj || (!main_thread_obj && on_main_thread()); }

/* -performSelectorOnMainThread:withObject:waitUntilDone: pela fila principal do GCD (dispatch.c),
 * que o CFRunLoop da thread principal drena; na própria thread principal com espera, executa na hora */
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *));
void dispatch_sync_f(void *q, void *ctx, void (*fn)(void *));
void *dispatch_get_main_queue(void);
typedef struct { id target, arg; SEL sel; } perform_t;
static void perform_run(void *p) {
    perform_t *d = p;
    ((void (*)(id, SEL, id))objc_msgSend)(d->target, d->sel, d->arg);
    objc_release(d->target);
    if (d->arg) objc_release(d->arg);
    free(d);
}
static void o_perform_on_main(id self, SEL c, SEL s, id a, BOOL wait) {
    (void)c;
    if (wait && on_main_thread()) { ((void (*)(id, SEL, id))objc_msgSend)(self, s, a); return; }
    perform_t *d = malloc(sizeof *d);
    if (!d) return;
    d->target = objc_retain(self); d->arg = a ? objc_retain(a) : 0; d->sel = s;
    if (wait) dispatch_sync_f(dispatch_get_main_queue(), d, perform_run);
    else dispatch_async_f(dispatch_get_main_queue(), d, perform_run);
}
static void o_perform_on_main_modes(id self, SEL c, SEL s, id a, BOOL wait, id modes) { (void)modes; o_perform_on_main(self, c, s, a, wait); }

/* NSThemeFrame: no AppKit real a view de conteúdo de uma janela fica dentro da "moldura" da janela, e o
 * Electron (-[ElectronNSWindow initWithShell:styleMask:]) troca o -swipeWithEvent: da classe de
 * [[window contentView] superview] — com superview nil o CHECK do swizzling encerra o processo. Aqui a
 * moldura é um objeto por janela, fora da árvore de desenho do appkit.c: -superview da view de conteúdo
 * devolve a moldura quando o appkit.c não tem superview para ela. */
cx_class_t NSThemeFrame_c __asm__("OBJC_CLASS_$_NSThemeFrame");
cx_class_t NSThemeFrame_m __asm__("OBJC_METACLASS_$_NSThemeFrame");
void *calloc(size_t n, size_t s);
/* retornos de struct > 16 bytes (NSRect) vêm por ponteiro oculto: use objc_msgSend_stret */
void objc_msgSend_stret(void);
typedef struct { cx_class_t *isa; id window; } theme_frame_t;
#define MAX_FRAMES 64
static struct { id window; id frame; } frames[MAX_FRAMES];
static id (*orig_superview)(id, SEL);
static id frame_for(id window) {
    for (int i = 0; i < MAX_FRAMES; i++) if (frames[i].window == window) return frames[i].frame;
    for (int i = 0; i < MAX_FRAMES; i++)
        if (!frames[i].window) {
            /* instância "crua" da classe (sem o estado de view do appkit.c): só responde aos métodos abaixo */
            theme_frame_t *f = calloc(1, sizeof *f + 64);
            if (!f) return 0;
            f->isa = &NSThemeFrame_c;
            f->window = window;
            frames[i].window = window; frames[i].frame = (id)f;
            return (id)f;
        }
    return 0;
}
static id v_superview_frame(id self, SEL c) {
    id s = orig_superview ? orig_superview(self, c) : 0;
    if (s) return s;
    id w = MSG0(id, self, "window");
    if (w && MSG0(id, w, "contentView") == self) return frame_for(w);
    return 0;
}
static id tf_window(id self, SEL c) { (void)c; return ((theme_frame_t *)self)->window; }
static id tf_nil(id self, SEL c) { (void)self; (void)c; return 0; }
static id tf_subviews(id self, SEL c) {
    (void)c;
    id a = MSG0(id, (id)cx_class_named("NSMutableArray"), "array");
    id cv = MSG0(id, ((theme_frame_t *)self)->window, "contentView");
    if (cv) MSG(void, a, "addObject:", cv);
    return a;
}
static void tf_event(id self, SEL c, id ev) { (void)self; (void)c; (void)ev; }
typedef struct { double x, y, w, h; } tf_rect_t;
static tf_rect_t tf_frame(id self, SEL c) {
    (void)c;
    id w = ((theme_frame_t *)self)->window;
    tf_rect_t r = w ? ((tf_rect_t (*)(id, SEL))objc_msgSend_stret)(w, sel_registerName("frame")) : (tf_rect_t){0, 0, 0, 0};
    r.x = r.y = 0;
    return r;
}
static void tf_set_size(id self, SEL c, double w, double h) { (void)self; (void)c; (void)w; (void)h; }
static void tf_add_subview(id self, SEL c, id v) { (void)self; (void)c; (void)v; }   /* desenho ainda não */
static BOOL tf_yes(id self, SEL c) { (void)self; (void)c; return YES; }
/* a moldura não tem o estado de view do appkit.c: o -isHidden herdado do NSView lia lixo (12) e toda a janela tinha um
 * "ancestral oculto". O Chromium (-[WebContentsViewCocoa updateWebContentsVisibility]) via então toda página como
 * oculta, e uma WebContentsView filha (o claude.ai no Claude desktop) nunca era mostrada. */
static BOOL tf_no(id self, SEL c) { (void)self; (void)c; return NO; }
/* NSWindow: o que o NativeWindowMac do Electron chama ao criar a janela e o appkit.c não tem */
typedef struct { double x, y, w, h; } nw_rect_t;
static void w_set_frame_animate(id self, SEL c, nw_rect_t r, BOOL display, BOOL animate) {
    (void)c; (void)animate;
    ((void (*)(id, SEL, nw_rect_t, BOOL))objc_msgSend)(self, sel_registerName("setFrame:display:"), r, display);
}
static id w_screen(id self, SEL c) { (void)self; (void)c; return MSG0(id, (id)cx_class_named("NSScreen"), "mainScreen"); }
static BOOL w_yes(id self, SEL c) { (void)self; (void)c; return YES; }
static BOOL w_no(id self, SEL c) { (void)self; (void)c; return NO; }
static u64 w_zero(id self, SEL c) { (void)self; (void)c; return 0; }
static void w_noop1(id self, SEL c, id a) { (void)self; (void)c; (void)a; }
typedef struct { double x, y; } nw_pt_t;
static u64 w_style(id self, SEL c) { (void)self; (void)c; return 15; }   /* titled|closable|miniaturizable|resizable */
static nw_pt_t w_pt_identity(id self, SEL c, nw_pt_t p) {
    (void)c;
    nw_rect_t f = ((nw_rect_t (*)(id, SEL))objc_msgSend_stret)(self, sel_registerName("frame"));
    p.x -= f.x; p.y -= f.y;                                     /* tela -> base da janela */
    return p;
}
static nw_pt_t w_pt_to_screen(id self, SEL c, nw_pt_t p) {
    (void)c;
    nw_rect_t f = ((nw_rect_t (*)(id, SEL))objc_msgSend_stret)(self, sel_registerName("frame"));
    p.x += f.x; p.y += f.y;
    return p;
}
static id tf_nil_1(id self, SEL c, long a) { (void)self; (void)c; (void)a; return 0; }
static void w_noop_size(id self, SEL c, double w, double h) { (void)self; (void)c; (void)w; (void)h; }
static nw_pt_t ev_mouse_location(id cls, SEL c) { (void)cls; (void)c; nw_pt_t p = {0, 0}; return p; }
static void window_methods_init(void) {
    cx_class_t *w = cx_class_named("NSWindow");
    if (!w) return;
#define ADD_IF_MISSING(k, s, f) do { if (!cx_responds(k, sel_registerName(s))) cx_add_method(k, s, (void *)(f)); } while (0)
    ADD_IF_MISSING(w, "styleMask", w_style);
    ADD_IF_MISSING(w, "level", w_zero);
    ADD_IF_MISSING(w, "isMiniaturized", w_no);
    ADD_IF_MISSING(w, "isZoomed", w_no);
    ADD_IF_MISSING(w, "isOpaque", w_yes);
    ADD_IF_MISSING(w, "standardWindowButton:", tf_nil_1);
    ADD_IF_MISSING(w, "setContentMaxSize:", w_noop_size);
    ADD_IF_MISSING(w, "setContentMinSize:", w_noop_size);
    ADD_IF_MISSING(w, "setMaxSize:", w_noop_size);
    ADD_IF_MISSING(w, "setMinSize:", w_noop_size);
    ADD_IF_MISSING(w, "convertPointFromScreen:", w_pt_identity);
    ADD_IF_MISSING(w, "convertPointToScreen:", w_pt_to_screen);
    cx_class_t *ev = cx_class_named("NSEvent");
    if (ev && !cx_responds(*(cx_class_t **)ev, sel_registerName("mouseLocation"))) cx_add_cmethod(ev, "mouseLocation", (void *)ev_mouse_location);
    if (!cx_responds(w, sel_registerName("setFrame:display:animate:"))) cx_add_method(w, "setFrame:display:animate:", (void *)w_set_frame_animate);
    if (!cx_responds(w, sel_registerName("screen"))) cx_add_method(w, "screen", (void *)w_screen);
    if (!cx_responds(w, sel_registerName("isOnActiveSpace"))) cx_add_method(w, "isOnActiveSpace", (void *)w_yes);
    if (!cx_responds(w, sel_registerName("collectionBehavior"))) cx_add_method(w, "collectionBehavior", (void *)w_zero);
    if (!cx_responds(w, sel_registerName("setIgnoresMouseEvents:"))) cx_add_method(w, "setIgnoresMouseEvents:", (void *)w_noop1);
    if (!cx_responds(w, sel_registerName("ignoresMouseEvents"))) cx_add_method(w, "ignoresMouseEvents", (void *)w_no);
    cx_class_t *v = cx_class_named("NSView");
    if (v && !cx_responds(v, sel_registerName("registerForDraggedTypes:"))) cx_add_method(v, "registerForDraggedTypes:", (void *)w_noop1);
    cx_class_t *app = cx_class_named("NSApplication");
    if (app && !cx_responds(app, sel_registerName("isFullKeyboardAccessEnabled"))) cx_add_method(app, "isFullKeyboardAccessEnabled", (void *)w_no);
}

static void frame_class_init(void) {
    window_methods_init();
    cx_class_t *view = cx_class_named("NSView");
    if (!view) return;
    cx_define_class(&NSThemeFrame_c, &NSThemeFrame_m, "NSThemeFrame", view, sizeof(theme_frame_t) - 8 + 64);
    cx_add_method(&NSThemeFrame_c, "window", (void *)tf_window);
    cx_add_method(&NSThemeFrame_c, "superview", (void *)tf_nil);
    cx_add_method(&NSThemeFrame_c, "subviews", (void *)tf_subviews);
    cx_add_method(&NSThemeFrame_c, "frame", (void *)tf_frame);
    cx_add_method(&NSThemeFrame_c, "bounds", (void *)tf_frame);
    cx_add_method(&NSThemeFrame_c, "setFrameSize:", (void *)tf_set_size);
    cx_add_method(&NSThemeFrame_c, "addSubview:", (void *)tf_add_subview);
    cx_add_method(&NSThemeFrame_c, "isFlipped", (void *)tf_yes);
    cx_add_method(&NSThemeFrame_c, "isHidden", (void *)tf_no);
    cx_add_method(&NSThemeFrame_c, "isHiddenOrHasHiddenAncestor", (void *)tf_no);
    /* eventos que o Electron troca ou consulta na moldura */
    cx_add_method(&NSThemeFrame_c, "swipeWithEvent:", (void *)tf_event);
    cx_add_method(&NSThemeFrame_c, "mouseDown:", (void *)tf_event);
    cx_add_method(&NSThemeFrame_c, "mouseUp:", (void *)tf_event);
    cx_add_method(&NSThemeFrame_c, "rightMouseDown:", (void *)tf_event);
    extern void *class_replaceMethod(cx_class_t *c, SEL s, void *imp, const char *types);
    orig_superview = (id (*)(id, SEL))class_replaceMethod(view, sel_registerName("superview"), (void *)v_superview_frame, "@@:");
}

/* KVO do NSObject: kvo.c */
extern void cx_kvo_init(cx_class_t *ob);
static id ws_center(id self, SEL c) { (void)self; (void)c; return MSG0(id, (id)cx_class_named("NSNotificationCenter"), "defaultCenter"); }

/* NSNotificationQueue: publica na hora no centro padrão */
cx_class_t NSNotificationQueue_c __asm__("OBJC_CLASS_$_NSNotificationQueue");
cx_class_t NSNotificationQueue_m __asm__("OBJC_METACLASS_$_NSNotificationQueue");
static id nq_default(id cls, SEL c) {
    (void)c;
    static id q;
    if (!q) q = cx_alloc_instance((cx_class_t *)cls);
    return q;
}
static void nq_enqueue(id self, SEL c, id note, long style) {
    (void)self; (void)c; (void)style;
    if (note) MSG(void, ws_center(0, 0), "postNotification:", note);
}
static void nq_enqueue_full(id self, SEL c, id note, long style, u64 mask, id modes) { (void)mask; (void)modes; nq_enqueue(self, c, note, style); }
static void nq_dequeue(id self, SEL c, id note, u64 mask) { (void)self; (void)c; (void)note; (void)mask; }

/* NSLocale (o mOS fala pt-BR) e NSAppleEventManager (sem Apple Events: registros aceitos) */
cx_class_t NSLocale_c __asm__("OBJC_CLASS_$_NSLocale");
cx_class_t NSLocale_m __asm__("OBJC_METACLASS_$_NSLocale");
cx_class_t NSAppleEventManager_c __asm__("OBJC_CLASS_$_NSAppleEventManager");
cx_class_t NSAppleEventManager_m __asm__("OBJC_METACLASS_$_NSAppleEventManager");
static int loc_en(void) { return cx_locale_id()[0] == 'e'; }
static id loc_langs(id cls, SEL c) {
    (void)cls; (void)c;
    static id pt, en;
    id *slot = loc_en() ? &en : &pt;
    if (!*slot) {
        id a = MSG0(id, (id)cx_class_named("NSMutableArray"), "array");
        if (loc_en()) {
            MSG(void, a, "addObject:", cx_str("en-US"));
            MSG(void, a, "addObject:", cx_str("en"));
            MSG(void, a, "addObject:", cx_str("pt-BR"));
        } else {
            MSG(void, a, "addObject:", cx_str("pt-BR"));
            MSG(void, a, "addObject:", cx_str("pt"));
            MSG(void, a, "addObject:", cx_str("en-US"));
        }
        *slot = objc_retain(a);
    }
    return *slot;
}
static id loc_current(id cls, SEL c) { (void)c; static id l; if (!l) l = cx_alloc_instance((cx_class_t *)cls); return l; }
static id loc_ident(id self, SEL c) {
    (void)self; (void)c;
    static int logged;
    const char *id = cx_locale_id();
    if (!logged) { logged = 1; cx_fdprintf(2, "locale: NSLocale %s\n", id); }
    return cx_str(id);
}
static id loc_lang(id self, SEL c) { (void)self; (void)c; return cx_str(loc_en() ? "en" : "pt"); }
static id loc_country(id self, SEL c) { (void)self; (void)c; return cx_str(loc_en() ? "US" : "BR"); }
static id loc_object(id self, SEL c, id key) {
    (void)self; (void)c;
    const char *k = c_str(key);
    if (!strncmp(k, "kCFLocaleLanguageCode", 21) || !strncmp(k, "languageCode", 12)) return cx_str(loc_en() ? "en" : "pt");
    if (!strncmp(k, "kCFLocaleCountryCode", 20) || !strncmp(k, "countryCode", 11)) return cx_str(loc_en() ? "US" : "BR");
    return cx_str(cx_locale_id());
}
static id aem_shared(id cls, SEL c) { (void)c; static id m; if (!m) m = cx_alloc_instance((cx_class_t *)cls); return m; }
static void aem_set(id self, SEL c, id h, SEL s, u32 cls, u32 ev) { (void)self; (void)c; (void)h; (void)s; (void)cls; (void)ev; }
static void aem_remove(id self, SEL c, u32 cls, u32 ev) { (void)self; (void)c; (void)cls; (void)ev; }
static void any_set_delegate(id self, SEL c, id d) { (void)self; (void)c; (void)d; }
/* -run / -stop: / -isRunning. Numa subclasse de NSApplication (o AtomApplication do Electron, que o
 * MessagePump do Chromium inicia com [NSApp run]) o laço também atende o CFRunLoop, onde estão as
 * tarefas do Chromium; o -run do appkit.c só bombeia a GUI. Os apps comuns usam o -run original. */
void *objc_autoreleasePoolPush(void);
void objc_autoreleasePoolPop(void *pool);
static volatile int app_is_running;
static void (*orig_run)(id, SEL);
static void (*orig_stop)(id, SEL, id);
int CFRunLoopRunInMode(void *mode, double seconds, unsigned char ret_after_source);
static BOOL app_running(id self, SEL c) { (void)self; (void)c; return app_is_running; }
static void app_run(id self, SEL c) {
    if (*(cx_class_t **)self == cx_class_named("NSApplication") && orig_run) { orig_run(self, c); return; }
    app_is_running = 1;
    MSG0(void, self, "finishLaunching");
    while (app_is_running) {
        void *pool = objc_autoreleasePoolPush();
        CFRunLoopRunInMode(0, 0.016, 1);
        if (cx_runloop_hook) cx_runloop_hook(cx_now() + 0.0005);
        objc_autoreleasePoolPop(pool);
    }
}
static void app_stop(id self, SEL c, id sender) { app_is_running = 0; if (orig_stop) orig_stop(self, c, sender); }
/* -nextEventMatchingMask:untilDate:inMode:dequeue: (laço do Chromium). O AppKit do mOS despacha os
 * eventos da GUI direto (sem fila de NSEvent), então isto é uma volta do laço: CFRunLoop até o prazo
 * (no máximo 16 ms) e o bombeamento do AppKit; devolve nil. */
int CFRunLoopRunInMode(void *mode, double seconds, unsigned char ret_after_source);
static id app_next_event(id self, SEL c, u64 mask, id date, id mode, BOOL dequeue) {
    (void)c; (void)mask; (void)mode; (void)dequeue;
    (void)self;
    double wait = date ? MSG0(double, date, "timeIntervalSinceNow") : 0;
    if (wait > 0.016) wait = 0.016;
    if (wait < 0) wait = 0;
    CFRunLoopRunInMode(0, wait, 1);
    if (cx_runloop_hook) cx_runloop_hook(cx_now() + 0.0005);
    return 0;
}
static id b_res_loc(id self, SEL c, id name, id type, id dir, id loc) {
    (void)c;
    return cx_bundle_localized(self, name, type, dir, loc);
}
/* NSDistributedNotificationCenter: sem notificações entre processos; usa o centro local */
cx_class_t NSDistributedNotificationCenter_c __asm__("OBJC_CLASS_$_NSDistributedNotificationCenter");
cx_class_t NSDistributedNotificationCenter_m __asm__("OBJC_METACLASS_$_NSDistributedNotificationCenter");
static id dnc_default(id cls, SEL c) { (void)cls; (void)c; return ws_center(0, 0); }
static id b_pref_locs(id self, SEL c) {
    (void)self; (void)c;
    static id a;
    if (!a) {
        a = MSG0(id, (id)cx_class_named("NSMutableArray"), "array");
        MSG(void, a, "addObject:", cx_str(loc_en() ? "en" : "pt_BR"));
        a = objc_retain(a);
    }
    return a;
}

/* NSAppearance: o mOS tem claro/escuro, mas por ora o app vê sempre "Aqua" (claro) */
cx_class_t NSAppearance_c __asm__("OBJC_CLASS_$_NSAppearance");
cx_class_t NSAppearance_m __asm__("OBJC_METACLASS_$_NSAppearance");
typedef struct { cx_class_t *isa; id name; } nsappearance_t;
static id appearance_named(id cls, SEL c, id name) {
    (void)cls; (void)c;
    nsappearance_t *a = (nsappearance_t *)cx_alloc_instance(&NSAppearance_c);
    a->name = objc_retain(name ? name : cx_str("NSAppearanceNameAqua"));
    return a ? MSG0(id, (id)a, "autorelease") : 0;
}
static id appearance_aqua(id cls, SEL c) {
    (void)c;
    static id a;
    if (!a) a = objc_retain(appearance_named(cls, 0, cx_str("NSAppearanceNameAqua")));
    return a;
}
static id appearance_name(id self, SEL c) { (void)c; return ((nsappearance_t *)self)->name; }
static id appearance_best(id self, SEL c, id names) {
    (void)self; (void)c;
    return names && MSG0(u64, names, "count") ? MSG(id, names, "objectAtIndex:", (u64)0) : 0;
}
/* -performAsCurrentDrawingAppearance: executa o bloco (sem trocar de aparência por enquanto) */
static void appearance_perform(id self, SEL c, void *block) {
    (void)self; (void)c;
    typedef struct { void *isa; int flags, reserved; void (*invoke)(void *); } blk_t;
    if (block) ((blk_t *)block)->invoke(block);
}
static id app_appearance(id self, SEL c) { (void)self; (void)c; return appearance_aqua((id)&NSAppearance_c, 0); }
static void nc_add_suspension(id self, SEL c, id obs, SEL sel, id name, id obj, long behavior) {
    (void)c; (void)behavior;
    MSG(void, self, "addObserver:selector:name:object:", obs, sel, name, obj);
}

/* NSProcessInfo: estado térmico/energia (sempre nominal) e versão do sistema (a mesma do sysctl) */
typedef struct { long major, minor, patch; } os_version_t;
static long pi_thermal(id self, SEL c) { (void)self; (void)c; return 0; }       /* NSProcessInfoThermalStateNominal */
static BOOL pi_low_power(id self, SEL c) { (void)self; (void)c; return NO; }
static os_version_t pi_os_version(id self, SEL c) { (void)self; (void)c; os_version_t v = {14, 0, 0}; return v; }
static BOOL pi_at_least(id self, SEL c, os_version_t v) {
    (void)self; (void)c;
    return v.major < 14 || (v.major == 14 && v.minor == 0 && v.patch == 0);
}
static id pi_os_string(id self, SEL c) { (void)self; (void)c; return ns_str("Versão 14.0 (mOS)"); }

void cx_darwin_init(void) {
    extern void cx_foundation_more_init(void);
    cx_foundation_more_init();                  /* o resto do Foundation (classes e símbolos que os apps importam) */
    extern void cx_coreml_init(void);
    cx_coreml_init();                           /* MLDictionaryFeatureProvider (voz do OpenBot) */
    extern void **cx_tsd(void);
    cx_tsd();                                   /* GS da thread principal antes do código do app */
    extern int cx_main_tid;
    cx_main_tid = (int)SYS0(SYS_GETTID);       /* a fila principal do GCD pertence a esta thread */
    extern void cx_cgdisplay_init(void);
    cx_cgdisplay_init();
    extern void cx_literals_init(void);
    cx_literals_init();                         /* @{} e @[] vazios (literals.c) */
    frame_class_init();                         /* NSThemeFrame: moldura da janela para o Electron */
    extern void cx_coretext_init(void);
    cx_coretext_init();                         /* NSFont/CTFont para o gfx::PlatformFontMac (coretext.c) */
    cx_class_t *ob = cx_class_named("NSObject");
    if (ob) {
        cx_kvo_init(ob);
    }
    if (ob) cx_add_method(ob, "setDelegate:", (void *)any_set_delegate);
    cx_define_class(&NSLocale_c, &NSLocale_m, "NSLocale", ob, 0);
    cx_add_cmethod(&NSLocale_c, "preferredLanguages", (void *)loc_langs);
    cx_add_cmethod(&NSLocale_c, "currentLocale", (void *)loc_current);
    cx_add_cmethod(&NSLocale_c, "systemLocale", (void *)loc_current);
    cx_add_cmethod(&NSLocale_c, "autoupdatingCurrentLocale", (void *)loc_current);
    cx_add_method(&NSLocale_c, "localeIdentifier", (void *)loc_ident);
    cx_add_method(&NSLocale_c, "languageCode", (void *)loc_lang);
    cx_add_method(&NSLocale_c, "countryCode", (void *)loc_country);
    cx_add_method(&NSLocale_c, "regionCode", (void *)loc_country);
    cx_add_method(&NSLocale_c, "objectForKey:", (void *)loc_object);
    cx_define_class(&NSAppleEventManager_c, &NSAppleEventManager_m, "NSAppleEventManager", ob, 0);
    cx_add_cmethod(&NSAppleEventManager_c, "sharedAppleEventManager", (void *)aem_shared);
    cx_add_method(&NSAppleEventManager_c, "setEventHandler:andSelector:forEventClass:andEventID:", (void *)aem_set);
    cx_add_method(&NSAppleEventManager_c, "removeEventHandlerForEventClass:andEventID:", (void *)aem_remove);
    cx_class_t *app = cx_class_named("NSApplication");
    cx_define_class(&NSAppearance_c, &NSAppearance_m, "NSAppearance", ob, sizeof(nsappearance_t) - 8);
    cx_add_cmethod(&NSAppearance_c, "appearanceNamed:", (void *)appearance_named);
    cx_add_cmethod(&NSAppearance_c, "currentAppearance", (void *)appearance_aqua);
    cx_add_cmethod(&NSAppearance_c, "currentDrawingAppearance", (void *)appearance_aqua);
    cx_add_method(&NSAppearance_c, "name", (void *)appearance_name);
    cx_add_method(&NSAppearance_c, "performAsCurrentDrawingAppearance:", (void *)appearance_perform);
    cx_add_method(&NSAppearance_c, "bestMatchFromAppearancesWithNames:", (void *)appearance_best);
    { cx_class_t *ncs = cx_class_named("NSNotificationCenter");
      if (ncs) cx_add_method(ncs, "addObserver:selector:name:object:suspensionBehavior:", (void *)nc_add_suspension); }
    if (app) {
        cx_add_method(app, "effectiveAppearance", (void *)app_appearance);
        cx_add_method(app, "appearance", (void *)app_appearance);
        cx_add_method(app, "isRunning", (void *)app_running);
        extern void *class_replaceMethod(cx_class_t *c, SEL s, void *imp, const char *types);
        orig_run = (void (*)(id, SEL))class_replaceMethod(app, sel_registerName("run"), (void *)app_run, "v@:");
        orig_stop = (void (*)(id, SEL, id))class_replaceMethod(app, sel_registerName("stop:"), (void *)app_stop, "v@:@");
        cx_add_method(app, "nextEventMatchingMask:untilDate:inMode:dequeue:", (void *)app_next_event);
    }
    cx_class_t *bu = cx_class_named("NSBundle");
    if (bu) {
        cx_add_method(bu, "preferredLocalizations", (void *)b_pref_locs);
        cx_add_method(bu, "pathForResource:ofType:inDirectory:forLocalization:", (void *)b_res_loc);
    }
    cx_class_t *nc = cx_class_named("NSNotificationCenter");
    cx_define_class(&NSDistributedNotificationCenter_c, &NSDistributedNotificationCenter_m, "NSDistributedNotificationCenter", nc ? nc : ob, 0);
    cx_add_cmethod(&NSDistributedNotificationCenter_c, "defaultCenter", (void *)dnc_default);
    cx_class_t *ws = cx_class_named("NSWorkspace");
    if (ws) cx_add_method(ws, "notificationCenter", (void *)ws_center);
    cx_define_class(&NSNotificationQueue_c, &NSNotificationQueue_m, "NSNotificationQueue", ob, 0);
    cx_add_cmethod(&NSNotificationQueue_c, "defaultQueue", (void *)nq_default);
    cx_add_method(&NSNotificationQueue_c, "enqueueNotification:postingStyle:", (void *)nq_enqueue);
    cx_add_method(&NSNotificationQueue_c, "enqueueNotification:postingStyle:coalesceMask:forModes:", (void *)nq_enqueue_full);
    cx_add_method(&NSNotificationQueue_c, "dequeueNotificationsMatching:coalesceMask:", (void *)nq_dequeue);
    cx_class_t *pi = cx_class_named("NSProcessInfo");
    if (pi) {
        cx_add_method(pi, "thermalState", (void *)pi_thermal);
        cx_add_method(pi, "isLowPowerModeEnabled", (void *)pi_low_power);
        cx_add_method(pi, "operatingSystemVersion", (void *)pi_os_version);
        cx_add_method(pi, "isOperatingSystemAtLeastVersion:", (void *)pi_at_least);
        cx_add_method(pi, "operatingSystemVersionString", (void *)pi_os_string);
    }
    cx_class_t *th = cx_class_named("NSThread");
    if (th) {
        cx_add_cmethod(th, "isMultiThreaded", (void *)th_multi);
        cx_add_cmethod(th, "detachNewThreadSelector:toTarget:withObject:", (void *)th_detach);
        cx_add_method(th, "threadDictionary", (void *)th_dict);
        cx_add_cmethod(th, "isMainThread", (void *)th_cls_is_main);
        cx_add_cmethod(th, "currentThread", (void *)th_current_obj);
        cx_add_cmethod(th, "mainThread", (void *)th_main_obj);
        cx_add_method(th, "isMainThread", (void *)th_obj_is_main);
    }
    if (ob) {
        cx_add_method(ob, "performSelectorOnMainThread:withObject:waitUntilDone:", (void *)o_perform_on_main);
        cx_add_method(ob, "performSelectorOnMainThread:withObject:waitUntilDone:modes:", (void *)o_perform_on_main_modes);
    }
    cx_define_class(&NSURL_c, &NSURL_m, "NSURL", cx_class_named("NSObject"), sizeof(nsurl_t) - 8);
    cx_add_cmethod(&NSURL_c, "fileURLWithPath:", (void *)u_file_path);
    cx_add_cmethod(&NSURL_c, "fileURLWithPath:isDirectory:", (void *)u_file_path_dir);
    cx_add_cmethod(&NSURL_c, "URLWithString:", (void *)u_with_string);
    cx_add_method(&NSURL_c, "path", (void *)u_path);
    cx_add_method(&NSURL_c, "isFileURL", (void *)u_is_file);
    cx_add_method(&NSURL_c, "absoluteString", (void *)u_absolute);
    cx_add_method(&NSURL_c, "description", (void *)u_absolute);
    cx_add_method(&NSURL_c, "relativeString", (void *)u_absolute);
    cx_add_method(&NSURL_c, "scheme", (void *)u_scheme);
    cx_add_method(&NSURL_c, "resourceSpecifier", (void *)u_resource_spec);
    cx_add_method(&NSURL_c, "host", (void *)u_host);
    cx_add_method(&NSURL_c, "port", (void *)u_port);
    cx_add_method(&NSURL_c, "user", (void *)u_user);
    cx_add_method(&NSURL_c, "password", (void *)u_password);
    cx_add_method(&NSURL_c, "query", (void *)u_query);
    cx_add_method(&NSURL_c, "fragment", (void *)u_fragment);
    cx_add_method(&NSURL_c, "fileSystemRepresentation", (void *)u_fsrep);
    cx_add_method(&NSURL_c, "URLByAppendingPathComponent:", (void *)u_append);
    cx_add_method(&NSURL_c, "URLByAppendingPathComponent:isDirectory:", (void *)u_append_dir);
    cx_add_method(&NSURL_c, "URLByDeletingLastPathComponent", (void *)u_delete_last);
    cx_add_method(&NSURL_c, "lastPathComponent", (void *)u_last);
    cx_add_method(&NSURL_c, "pathExtension", (void *)u_ext);
    cx_add_method(&NSURL_c, "URLByStandardizingPath", (void *)u_std);
    cx_add_method(&NSURL_c, "URLByResolvingSymlinksInPath", (void *)u_std);
    cx_add_method(&NSURL_c, "absoluteURL", (void *)u_std);
    cx_add_method(&NSURL_c, "copy", (void *)u_copy);
    cx_add_method(&NSURL_c, "copyWithZone:", (void *)u_copy);
    cx_add_method(&NSURL_c, "isEqual:", (void *)u_equal);
    cx_add_method(&NSURL_c, "hash", (void *)u_hash);
    cx_add_method(&NSURL_c, "checkResourceIsReachableAndReturnError:", (void *)u_reachable);
    cx_class_t *b = cx_class_named("NSBundle");
    if (b) {
        cx_add_cmethod(b, "bundleWithURL:", (void *)b_with_url);
        cx_add_method(b, "bundleURL", (void *)b_url);
        cx_add_method(b, "executableURL", (void *)b_exec_url);
        cx_add_method(b, "resourceURL", (void *)b_res_url);
        cx_add_method(b, "URLForResource:withExtension:", (void *)b_url_for);
        cx_add_method(b, "URLForResource:withExtension:subdirectory:", (void *)b_url_for_sub);
    }
    { extern void cx_quartzcore_init(void); cx_quartzcore_init(); }   /* IOSurface/CALayer (quartzcore.c) */
    { extern void cx_electron_compat_init(void); cx_electron_compat_init(); }   /* KVC, envios adiados... (electron_compat.c) */
    { extern void cx_openurl_init(void); cx_openurl_init(); }   /* openURL:, LaunchServices e kAEGetURL entre apps (openurl.c) */
}

/* ---- CFURL (CFURLRef = NSURL*); regra Create/Copy: +1 */
id CFURLCreateWithFileSystemPath(void *alloc, id path, long style, u8 dir) {
    (void)alloc; (void)style; (void)dir;
    return path ? url_make(path, YES, 0) : 0;
}
id CFURLCreateFromFileSystemRepresentation(void *alloc, const u8 *buf, long n, u8 dir) {
    (void)alloc; (void)dir;
    if (!buf || n < 0) return 0;
    id s = MSG(id, (id)cx_class_named("NSString"), "stringWithUTF8String:", (const char *)buf);
    return s ? url_make(s, YES, 0) : 0;
}
id CFURLCopyFileSystemPath(id url, long style) { (void)style; id p = url ? URL(url)->path : 0; return p ? objc_retain(p) : 0; }
u8 CFURLGetFileSystemRepresentation(id url, u8 resolve, u8 *buf, long max) {
    (void)resolve;
    const char *p = url ? c_str(URL(url)->path) : 0;
    if (!p || !buf) return 0;
    size_t n = strlen(p);
    if ((long)n + 1 > max) return 0;
    for (size_t i = 0; i <= n; i++) buf[i] = (u8)p[i];
    return 1;
}
id CFURLCopyScheme(id url) {
    if (!url) return 0;
    if (URL(url)->file) return objc_retain(ns_str("file"));
    const char *t = c_str(URL(url)->string), *colon = t;
    while (*colon && *colon != ':' && *colon != '/') colon++;
    if (*colon != ':') return 0;
    char sch[32];
    size_t n = (size_t)(colon - t) < sizeof sch - 1 ? (size_t)(colon - t) : sizeof sch - 1;
    for (size_t i = 0; i < n; i++) sch[i] = t[i];
    sch[n] = 0;
    return objc_retain(ns_str(sch));
}
u8 CFURLHasDirectoryPath(id url) { const char *p = url ? c_str(URL(url)->path) : ""; size_t n = strlen(p); return n && p[n - 1] == '/'; }
id CFURLCopyLastPathComponent(id url) { return url ? objc_retain(MSG0(id, url, "lastPathComponent")) : 0; }
id CFURLGetString(id url) { return url ? MSG0(id, url, "absoluteString") : 0; }
id CFBundleCopyBundleURL(id bundle) { return bundle ? objc_retain(MSG0(id, bundle, "bundleURL")) : 0; }
id CFBundleCopyExecutableURL(id bundle) { id u = bundle ? MSG0(id, bundle, "executableURL") : 0; return u ? objc_retain(u) : 0; }
id CFBundleCopyResourcesDirectoryURL(id bundle) { return bundle ? objc_retain(MSG0(id, bundle, "resourceURL")) : 0; }
id CFBundleCreate(void *alloc, id url) { (void)alloc; id b = url ? MSG(id, (id)cx_class_named("NSBundle"), "bundleWithURL:", url) : 0; return b ? objc_retain(b) : 0; }
