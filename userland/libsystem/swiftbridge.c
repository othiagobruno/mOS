/*
 * mOS libSystem — ponte C para apps em Swift (Embedded Swift não tem interoperabilidade com Objective-C).
 * O módulo Swift "SwiftUI" do SDK do mOS (userland/sdk/swift) chama estas funções; elas montam e
 * manipulam objetos do AppKit do mOS por objc_msgSend. Ids são ponteiros opacos (void *).
 */
#include "foundation.h"
#include "libc.h"

typedef struct { double x, y; } pt_t;
typedef struct { double w, h; } sz_t;
typedef struct { pt_t o; sz_t s; } rc_t;
#define ST(o) (*(void **)((u8 *)(o) + 8))
extern id objc_retain(id), objc_autorelease(id);
extern void objc_release(id);

/* binários Swift Embedded não têm seções ObjC: o runtime (Foundation + AppKit) sobe aqui na 1ª chamada */
extern void cx_objc_boot(u64 sects, u64 n);
static id cls(const char *n) { cx_objc_boot(0, 0); return (id)cx_class_named(n); }

void *mos_class(const char *name) { return cls(name); }
void *mos_str(const char *s) { return cx_str(s ? s : ""); }                 /* +1 */
const char *mos_cstr(void *s) { return s ? cx_cstr((id)s) : ""; }
void mos_retain(void *o) { objc_retain((id)o); }
void mos_release(void *o) { objc_release((id)o); }
void *mos_send(void *o, const char *sel) { return o ? MSG0(id, o, sel) : 0; }
void *mos_send_p(void *o, const char *sel, void *a) { return o ? MSG(id, o, sel, a) : 0; }
void *mos_send_pp(void *o, const char *sel, void *a, void *b) { return o ? MSG(id, o, sel, a, b) : 0; }
void mos_send_d(void *o, const char *sel, double d) { if (o) MSG(void, o, sel, d); }
void mos_send_i(void *o, const char *sel, long v) { if (o) MSG(void, o, sel, v); }
long mos_send_ret_i(void *o, const char *sel) { return o ? MSG0(long, o, sel) : 0; }
double mos_send_ret_d(void *o, const char *sel) { return o ? MSG0(double, o, sel) : 0; }
void mos_set_string(void *o, const char *sel, const char *s) { if (o) { id str = cx_str(s ? s : ""); MSG(void, o, sel, str); objc_release(str); } }
const char *mos_get_string(void *o, const char *sel) { id s = o ? MSG0(id, o, sel) : 0; return s ? cx_cstr(s) : ""; }

void *mos_new_view(const char *klass, double x, double y, double w, double h) {
    rc_t r = {{x, y}, {w, h}};
    return MSG(id, MSG0(id, cls(klass), "alloc"), "initWithFrame:", r);
}

/* Vista do runtime embutido. O NSView comum só repassa scrollWheel:; esta subclasse entrega
 * a roda ao Swift (origem no topo) para o ScrollView andar sem os botões Subir/Descer. */
typedef void (*mos_scroll_fn)(void *ctx, double x, double y, double dx, double dy);
typedef struct { id view; mos_scroll_fn fn; void *ctx; } scroll_slot_t;
static scroll_slot_t scroll_slots[32];
static int nscroll;
static cx_class_t *host_cls;
static scroll_slot_t *scroll_of(id v) {
    int i;
    for (i = 0; i < nscroll; i++) if (scroll_slots[i].view == v) return &scroll_slots[i];
    return 0;
}
static void host_scroll(id self, SEL c, id e) {
    scroll_slot_t *s = scroll_of(self);
    pt_t p, local;
    rc_t f;
    SEL fs;
    (void)c;
    if (!s || !s->fn || !e) return;
    p = MSG0(pt_t, e, "locationInWindow");
    local = MSG(pt_t, self, "convertPoint:fromView:", p, (id)0);
    fs = sel_registerName("frame");
    f = ((rc_t (*)(id, SEL))cx_lookup(self->isa, fs))(self, fs);
    s->fn(s->ctx, local.x, f.s.h - local.y, 0, MSG0(double, e, "scrollingDeltaY"));
}
void *mos_host_view(double x, double y, double w, double h) {
    rc_t r = {{x, y}, {w, h}};
    if (!host_cls) {
        host_cls = cx_define_class(calloc(1, sizeof(cx_class_t)), calloc(1, sizeof(cx_class_t)), "_CXSwiftHost", (cx_class_t *)cls("NSView"), 0);
        cx_add_method(host_cls, "scrollWheel:", (void *)host_scroll);
    }
    return MSG(id, MSG0(id, (id)host_cls, "alloc"), "initWithFrame:", r);
}
void mos_view_on_scroll(void *view, mos_scroll_fn fn, void *ctx) {
    id v = (id)view;
    scroll_slot_t *s;
    if (!v) return;
    s = scroll_of(v);
    if (!s && nscroll < 32) { s = &scroll_slots[nscroll++]; s->view = v; }
    if (!s) return;
    s->fn = fn;
    s->ctx = ctx;
}
void mos_set_frame(void *v, double x, double y, double w, double h) { rc_t r = {{x, y}, {w, h}}; if (v) MSG(void, v, "setFrame:", r); }
void mos_add_subview(void *parent, void *child) { MSG(void, parent, "addSubview:", child); }
void mos_remove_from_superview(void *v) { MSG0(void, v, "removeFromSuperview"); }

/* fontes e cores */
void *mos_font(double size, int weight) {   /* weight: 0 normal, 1 semibold, 2 bold, 3 mono */
    if (weight == 3) return MSG(id, cls("NSFont"), "monospacedSystemFontOfSize:weight:", size, 0.0);
    if (weight >= 1) return MSG(id, cls("NSFont"), "systemFontOfSize:weight:", size, weight == 2 ? 0.4 : 0.3);
    return MSG(id, cls("NSFont"), "systemFontOfSize:", size);
}
void *mos_color(double r, double g, double b, double a) { return MSG(id, cls("NSColor"), "colorWithRed:green:blue:alpha:", r, g, b, a); }

/* medida de texto (largura/altura) com a fonte do sistema */
void mos_measure(const char *s, double size, int weight, double *w, double *h) {
    id str = cx_str(s ? s : "");
    id lbl = MSG(id, cls("NSTextField"), "labelWithString:", str);
    MSG(void, lbl, "setFont:", mos_font(size, weight));
    sz_t z = MSG0(sz_t, lbl, "intrinsicContentSize");
    objc_release(str);
    *w = z.w; *h = z.h;
}

/* ---- callbacks: alvo de ação e delegate de texto que chamam uma função C com contexto */
typedef struct { void (*fn)(void *); void (*tfn)(void *, const char *); void *ctx; } cb_t;
static cx_class_t *target_cls;
static void tg_fire(id self, SEL c, id sender) { (void)c; (void)sender; cb_t *b = ST(self); if (b && b->fn) b->fn(b->ctx); }
static void tg_text(id self, SEL c, id note) {
    (void)c;
    cb_t *b = ST(self);
    id field = note ? MSG0(id, note, "object") : 0;
    if (b && b->tfn && field) b->tfn(b->ctx, cx_cstr(MSG0(id, field, "stringValue")));
}
static void tg_text_view(id self, SEL c, id note) {
    (void)c;
    cb_t *b = ST(self);
    id tv = note ? MSG0(id, note, "object") : 0;
    if (b && b->tfn && tv) b->tfn(b->ctx, cx_cstr(MSG0(id, tv, "string")));
}
static void ensure_target(void) {
    if (target_cls) return;
    target_cls = cx_define_class(calloc(1, sizeof(cx_class_t)), calloc(1, sizeof(cx_class_t)), "_CXSwiftTarget", (cx_class_t *)cls("NSObject"), 8);
    cx_add_method(target_cls, "fire:", (void *)tg_fire);
    cx_add_method(target_cls, "controlTextDidChange:", (void *)tg_text);
    cx_add_method(target_cls, "textDidChange:", (void *)tg_text_view);
}
/* liga a ação de um controle a fn(ctx); devolve o alvo (+1, guarde-o enquanto o controle existir) */
void *mos_on_action(void *control, void (*fn)(void *), void *ctx) {
    ensure_target();
    id t = cx_alloc_instance(target_cls);
    cb_t *b = calloc(1, sizeof *b);
    b->fn = fn; b->ctx = ctx;
    ST(t) = b;
    MSG(void, control, "setTarget:", t);
    MSG(void, control, "setAction:", sel_registerName("fire:"));
    return t;
}
void *mos_on_text(void *field, void (*fn)(void *, const char *), void *ctx) {
    ensure_target();
    id t = cx_alloc_instance(target_cls);
    cb_t *b = calloc(1, sizeof *b);
    b->tfn = fn; b->ctx = ctx;
    ST(t) = b;
    MSG(void, field, "setDelegate:", t);
    return t;
}
void mos_set_callback_ctx(void *target, void *ctx) { cb_t *b = target ? ST((id)target) : 0; if (b) b->ctx = ctx; }

/* ---- app: janela principal + renderização inicial e a cada redimensionamento */
typedef struct { void (*render)(void *ctx, void *content, double w, double h); void *ctx; id window; } appcb_t;
static appcb_t appcb;
static cx_class_t *appdel_cls;
static void render_now(void) {
    if (!appcb.window || !appcb.render) return;
    id content = MSG0(id, appcb.window, "contentView");
    SEL fs = sel_registerName("frame");
    rc_t f = ((rc_t (*)(id, SEL))cx_lookup(content->isa, fs))(content, fs);   /* retorno grande: chama o IMP direto */
    appcb.render(appcb.ctx, content, f.s.w, f.s.h);
}
static void ad_finish(id self, SEL c, id n) { (void)self; (void)c; (void)n; render_now(); MSG(void, appcb.window, "makeKeyAndOrderFront:", (id)0); }
static void ad_resize(id self, SEL c, id n) { (void)self; (void)c; (void)n; render_now(); }
static BOOL ad_last(id self, SEL c, id a) { (void)self; (void)c; (void)a; return YES; }
void mos_request_render(void) { render_now(); }
void *mos_main_window(void) { return appcb.window; }
void mos_run_app(const char *title, double w, double h, void (*render)(void *, void *, double, double), void *ctx) {
    id app = MSG0(id, cls("NSApplication"), "sharedApplication");
    appdel_cls = cx_define_class(calloc(1, sizeof(cx_class_t)), calloc(1, sizeof(cx_class_t)), "_CXSwiftAppDelegate", (cx_class_t *)cls("NSObject"), 0);
    cx_add_method(appdel_cls, "applicationDidFinishLaunching:", (void *)ad_finish);
    cx_add_method(appdel_cls, "windowDidResize:", (void *)ad_resize);
    cx_add_method(appdel_cls, "applicationShouldTerminateAfterLastWindowClosed:", (void *)ad_last);
    id del = cx_alloc_instance(appdel_cls);
    rc_t r = {{0, 0}, {w, h}};
    id win = MSG(id, MSG0(id, cls("NSWindow"), "alloc"), "initWithContentRect:styleMask:backing:defer:", r, (u64)15, (u64)2, 0);
    id t = cx_str(title ? title : "");
    MSG(void, win, "setTitle:", t);
    MSG(void, win, "setDelegate:", del);
    appcb.render = render; appcb.ctx = ctx; appcb.window = win;
    MSG(void, app, "setDelegate:", del);
    MSG0(void, app, "run");
}
void mos_log(const char *s) { cx_fdprintf(2, "%s\n", s ? s : ""); }
int mos_alert(const char *title, const char *message, const char *b1, const char *b2) {
    id a = MSG0(id, MSG0(id, cls("NSAlert"), "alloc"), "init");
    id t = cx_str(title ? title : ""), m = cx_str(message ? message : "");
    MSG(void, a, "setMessageText:", t);
    if (message && *message) MSG(void, a, "setInformativeText:", m);
    if (b1) { id s = cx_str(b1); MSG(id, a, "addButtonWithTitle:", s); }
    if (b2) { id s = cx_str(b2); MSG(id, a, "addButtonWithTitle:", s); }
    long r = MSG0(long, a, "runModal");
    return (int)(r - 1000);
}
static struct { id v; void *k; } canvases[256];
static int ncanvas;
static void *canvas_of(id v) { for (int i = 0; i < ncanvas; i++) if (canvases[i].v == v) return canvases[i].k; return 0; }
static void *mos_canvas_bind(id v, void *k) { if (ncanvas < 256) { canvases[ncanvas].v = v; canvases[ncanvas].k = k; ncanvas++; } return v; }
/* ---- Canvas: NSView invertida (origem no topo) cujo drawRect: chama uma função C */
typedef struct { void (*draw)(void *ctx, double w, double h); void *ctx; void (*mouse)(void *ctx, double x, double y); void (*key)(void *ctx, int ch); void (*scroll)(void *ctx, double x, double y, double dx, double dy); } canvas_t;
static cx_class_t *canvas_cls;
static void cv_draw(id self, SEL c, rc_t r) {
    (void)c; (void)r;
    canvas_t *k = canvas_of(self);
    SEL fs = sel_registerName("frame");
    rc_t f = ((rc_t (*)(id, SEL))cx_lookup(self->isa, fs))(self, fs);
    if (k && k->draw) k->draw(k->ctx, f.s.w, f.s.h);
}
static BOOL cv_flipped(id self, SEL c) { (void)self; (void)c; return YES; }
static BOOL cv_accepts(id self, SEL c) { (void)self; (void)c; return YES; }
static void cv_mouse(id self, SEL c, id e) {
    (void)c;
    canvas_t *k = canvas_of(self);
    if (!k || !k->mouse) return;
    pt_t p = MSG0(pt_t, e, "locationInWindow");
    SEL fs = sel_registerName("frame");
    rc_t f = ((rc_t (*)(id, SEL))cx_lookup(self->isa, fs))(self, fs);
    k->mouse(k->ctx, p.x - f.o.x, f.s.h - (p.y - f.o.y));   /* canvas invertido: y a partir do topo */
}
static void cv_key(id self, SEL c, id e) {
    (void)c;
    canvas_t *k = canvas_of(self);
    id ch = MSG0(id, e, "characters");
    const char *s = ch ? cx_cstr(ch) : "";
    if (k && k->key) k->key(k->ctx, (unsigned char)s[0]);
}
static void cv_scroll(id self, SEL c, id e) {
    canvas_t *k = canvas_of(self);
    pt_t p;
    SEL fs;
    rc_t f;
    (void)c;
    if (!k || !k->scroll || !e) return;
    p = MSG0(pt_t, e, "locationInWindow");
    fs = sel_registerName("frame");
    f = ((rc_t (*)(id, SEL))cx_lookup(self->isa, fs))(self, fs);
    k->scroll(k->ctx, p.x - f.o.x, f.s.h - (p.y - f.o.y), 0, MSG0(double, e, "scrollingDeltaY"));
}
void *mos_canvas(void (*draw)(void *, double, double), void *ctx) {
    if (!canvas_cls) {
        canvas_cls = cx_define_class(calloc(1, sizeof(cx_class_t)), calloc(1, sizeof(cx_class_t)), "_CXSwiftCanvas", (cx_class_t *)cls("NSView"), 0);
        cx_add_method(canvas_cls, "drawRect:", (void *)cv_draw);
        cx_add_method(canvas_cls, "isFlipped", (void *)cv_flipped);
        cx_add_method(canvas_cls, "mouseDown:", (void *)cv_mouse);
        cx_add_method(canvas_cls, "keyDown:", (void *)cv_key);
        cx_add_method(canvas_cls, "scrollWheel:", (void *)cv_scroll);
        cx_add_method(canvas_cls, "acceptsFirstResponder", (void *)cv_accepts);
    }
    rc_t r = {{0, 0}, {10, 10}};
    id v = MSG(id, MSG0(id, (id)canvas_cls, "alloc"), "initWithFrame:", r);
    /* o estado do AppKit já ocupa ST(v): guardamos o callback numa tabela por view */
    canvas_t *k = calloc(1, sizeof *k);
    k->draw = draw; k->ctx = ctx;
    return mos_canvas_bind(v, k);
}void mos_canvas_set_ctx(void *v, void *ctx) { canvas_t *k = canvas_of((id)v); if (k) k->ctx = ctx; }
void mos_set_needs_display(void *v) { MSG(void, v, "setNeedsDisplay:", 1); }

/* primitivas de desenho (válidas dentro do callback do Canvas; coordenadas da view) */
static void setfill(double r, double g, double b, double a) { MSG0(void, MSG(id, cls("NSColor"), "colorWithRed:green:blue:alpha:", r, g, b, a), "setFill"); }
static void setstroke(double r, double g, double b, double a) { MSG0(void, MSG(id, cls("NSColor"), "colorWithRed:green:blue:alpha:", r, g, b, a), "setStroke"); }
void mos_draw_rect(double x, double y, double w, double h, double radius, double r, double g, double b, double a, int fill, double lw) {
    rc_t rc = {{x, y}, {w, h}};
    id p = radius > 0 ? MSG(id, cls("NSBezierPath"), "bezierPathWithRoundedRect:xRadius:yRadius:", rc, radius, radius) : MSG(id, cls("NSBezierPath"), "bezierPathWithRect:", rc);
    if (fill) { setfill(r, g, b, a); MSG0(void, p, "fill"); }
    else { setstroke(r, g, b, a); MSG(void, p, "setLineWidth:", lw); MSG0(void, p, "stroke"); }
}
void mos_draw_ellipse(double x, double y, double w, double h, double r, double g, double b, double a, int fill, double lw) {
    rc_t rc = {{x, y}, {w, h}};
    id p = MSG(id, cls("NSBezierPath"), "bezierPathWithOvalInRect:", rc);
    if (fill) { setfill(r, g, b, a); MSG0(void, p, "fill"); }
    else { setstroke(r, g, b, a); MSG(void, p, "setLineWidth:", lw); MSG0(void, p, "stroke"); }
}
/* caminho de pontos: xy[2n]; closed = fecha; fill/stroke */
void mos_draw_path(const double *xy, int n, int closed, double r, double g, double b, double a, int fill, double lw) {
    if (n < 2) return;
    id p = MSG0(id, cls("NSBezierPath"), "bezierPath");
    pt_t q = {xy[0], xy[1]};
    MSG(void, p, "moveToPoint:", q);
    for (int i = 1; i < n; i++) { pt_t t = {xy[2 * i], xy[2 * i + 1]}; MSG(void, p, "lineToPoint:", t); }
    if (closed) MSG0(void, p, "closePath");
    if (fill) { setfill(r, g, b, a); MSG0(void, p, "fill"); }
    else { setstroke(r, g, b, a); MSG(void, p, "setLineWidth:", lw); MSG0(void, p, "stroke"); }
}
void mos_draw_text(const char *s, double x, double y, double size, int weight, double r, double g, double b, double a) {
    id str = cx_str(s ? s : "");
    id keys[2] = {(id)0, (id)0}, vals[2];
    extern id NSFontAttributeName, NSForegroundColorAttributeName;
    keys[0] = NSFontAttributeName; vals[0] = mos_font(size, weight);
    keys[1] = NSForegroundColorAttributeName; vals[1] = mos_color(r, g, b, a);
    id attrs = MSG(id, cls("NSDictionary"), "dictionaryWithObjects:forKeys:count:", vals, keys, (u64)2);
    pt_t p = {x, y};
    MSG(void, str, "drawAtPoint:withAttributes:", p, attrs);
    objc_release(str);
}

/* ---- timer repetitivo (animações): chama fn(ctx) a cada intervalo até mos_timer_stop */
void *mos_timer(double interval, void (*fn)(void *), void *ctx) {
    ensure_target();
    id t = cx_alloc_instance(target_cls);
    cb_t *b = calloc(1, sizeof *b);
    b->fn = fn; b->ctx = ctx;
    ST(t) = b;
    return MSG(id, cls("NSTimer"), "scheduledTimerWithTimeInterval:target:selector:userInfo:repeats:", interval, t, sel_registerName("fire:"), (id)0, 1);
}
void mos_timer_stop(void *timer) { if (timer) MSG0(void, timer, "invalidate"); }
void mos_canvas_on_scroll(void *v, void (*scroll)(void *, double, double, double, double)) {
    canvas_t *k = canvas_of((id)v);
    if (k) k->scroll = scroll;
}
void mos_canvas_input(void *v, void (*mouse)(void *, double, double), void (*key)(void *, int)) {
    canvas_t *k = canvas_of((id)v);
    if (k) { k->mouse = mouse; k->key = key; }
    id w = MSG0(id, v, "window");
    if (w) MSG(BOOL, w, "makeFirstResponder:", v);
}
/* imagens do bundle (NSImage imageNamed:) */
void mos_image_size(const char *name, double *w, double *h) {
    id n = cx_str(name ? name : "");
    id img = MSG(id, cls("NSImage"), "imageNamed:", n);
    objc_release(n);
    *w = *h = 0;
    if (img) { sz_t z = MSG0(sz_t, img, "size"); *w = z.w; *h = z.h; }
}
void mos_draw_image(const char *name, double x, double y, double w, double h) {
    id n = cx_str(name ? name : "");
    id img = MSG(id, cls("NSImage"), "imageNamed:", n);
    objc_release(n);
    rc_t r = {{x, y}, {w, h}};
    if (img) MSG(void, img, "drawInRect:", r);
}