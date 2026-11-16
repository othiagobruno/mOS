/*
 * mOS libSystem — mais classes do AppKit que os apps do corpus importam:
 *   Gestos: NSGestureRecognizer e NSPan/NSPress/NSClick/NSMagnification/NSRotationGestureRecognizer. A janela entrega
 *     o mouse aos reconhecedores da view clicada e dos ancestrais antes da view (cx_gr_mouse, chamado de appkit.c);
 *     subclasses do app sobrescrevem mouseDown:/mouseDragged:/mouseUp: e mudam -state como no AppKit.
 *     Magnify e rotate seguem as fases do NSEvent quando chegam (magnifyWithEvent:/rotateWithEvent:); hoje o mOS
 *     não tem trackpad multitoque que gere esses eventos.
 *   NSTouch (dados do toque; -[NSEvent touchesMatchingPhase:inView:] devolve vazio sem trackpad multitoque).
 *   NSAnimation de verdade: quadros com curva, marcas de progresso, encadeamento (start/stopWhenAnimation:), modos
 *     bloqueante e não bloqueante; NSViewAnimation move, redimensiona e esmaece views e janelas.
 *   NSController e NSObjectController (conteúdo, seleção, add:/remove:, edição).
 *   NSFontManager completo e NSFontPanel (família, estilo, tamanho, prévia; changeFont: pela cadeia de responders).
 *   NSFontCollection/NSMutableFontCollection (coleções com nome, consultas e exclusões).
 *   NSTextBlock (dimensões, margens, bordas e preenchimento; geometria do layout e fundo).
 *   NSPDFImageRep (páginas de PDF pelo CGPDF da Mariana) e NSEPSImageRep (caixa e prévia TIFF do EPS DOS; o mOS
 *     não tem interpretador PostScript). NSImage initWithData: com PDF agora vira imagem.
 *   NSScrubberLayout/NSScrubberFlowLayout/NSScrubberProportionalLayout e NSSharingServicePickerTouchBarItem.
 * Regras puras em appkit_more_core.c (teste userland/tests-appkit/more_host.c).
 * Registro: cx_appkit_more_classes() cedo (antes das classes que herdam destas) e cx_appkit_more_init() no fim.
 */
#include "foundation.h"
#include "sys.h"
#include "libc.h"
#include "appkit_more_core.h"

id objc_msgSend_stret(id self, SEL op, ...);
id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void objc_setAssociatedObject(id obj, const void *key, id value, u64 policy);
id objc_getAssociatedObject(id obj, const void *key);
IMP class_getMethodImplementation(cx_class_t *c, SEL s);
const char *sel_getName(SEL s);
id cx_color(double r, double g, double b, double a);
void NSBeep(void);
extern id NSApp;
void *dispatch_get_main_queue(void);
void dispatch_after_f(u64 when, void *q, void *ctx, void (*fn)(void *));
u64 dispatch_time(u64 when, long delta);
u64 mach_absolute_time(void);
int usleep(unsigned us);

typedef struct { double x, y; } P;
typedef struct { double w, h; } S;
typedef struct { double x, y, w, h; } R;
void NSRectFill(R r);
void NSFrameRect(R r);

#define K(N) cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N); cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N);
K(NSGestureRecognizer) K(NSPanGestureRecognizer) K(NSPressGestureRecognizer) K(NSMagnificationGestureRecognizer)
K(NSRotationGestureRecognizer) K(NSTouch) K(NSViewAnimation) K(NSController) K(NSObjectController) K(NSFontPanel)
K(NSFontCollection) K(NSMutableFontCollection) K(NSTextBlock) K(NSEPSImageRep) K(NSPDFImageRep) K(NSScrubberLayout)
K(NSScrubberFlowLayout) K(NSScrubberProportionalLayout) K(NSScrubberLayoutAttributes) K(NSSharingServicePickerTouchBarItem)
K(_MOSFontList)

static cx_class_t *kls(const char *n) { return cx_class_named(n); }
static SEL sl(const char *n) { return sel_registerName(n); }
static BOOL responds(id o, const char *s) { return o && cx_responds(object_getClass(o), sl(s)); }
static R msg_rect(id o, const char *s) { return ((R (*)(id, SEL))objc_msgSend_stret)(o, sl(s)); }
static void m_set_frame(id v, R r) { ((void (*)(id, SEL, R))objc_msgSend)(v, sl("setFrame:"), r); }
static void redraw(id v) { if (v && responds(v, "setNeedsDisplay:")) MSG(void, v, "setNeedsDisplay:", (BOOL)YES); }
static double now_s(void) { return (double)mach_absolute_time() / 1e9; }
static id arr(void) { return MSG0(id, (id)kls("NSMutableArray"), "array"); }
static id rect_val(R r) { return ((id (*)(id, SEL, R))objc_msgSend)((id)kls("NSValue"), sl("valueWithRect:"), r); }
static R val_rect(id v) { return v ? ((R (*)(id, SEL))objc_msgSend_stret)(v, sl("rectValue")) : (R){0, 0, 0, 0}; }

/* propriedades por objeto: dicionário associado */
static char k_props;
static id props(id o, int make) {
    id d = objc_getAssociatedObject(o, &k_props);
    if (!d && make) { d = MSG0(id, (id)kls("NSMutableDictionary"), "dictionary"); objc_setAssociatedObject(o, &k_props, d, 1); }
    return d;
}
static void pset(id o, const char *k, id v) { id d = props(o, 1); if (v) MSG(void, d, "setObject:forKey:", v, cx_str(k)); else MSG(void, d, "removeObjectForKey:", cx_str(k)); }
static id pget(id o, const char *k) { id d = props(o, 0); return d ? MSG(id, d, "objectForKey:", cx_str(k)) : 0; }
static i64 pgi(id o, const char *k, i64 def) { id v = pget(o, k); return v ? MSG0(i64, v, "longLongValue") : def; }
static double pgd(id o, const char *k, double def) { id v = pget(o, k); return v ? MSG0(double, v, "doubleValue") : def; }
static void psi(id o, const char *k, i64 v) { pset(o, k, cx_num_i(v)); }
static void psd(id o, const char *k, double v) { pset(o, k, cx_num_d(v)); }
/* referência fraca (alvo, delegate, view): associação sem retenção */
static char k_target, k_delegate, k_view;
static id weak_get(id o, char *key) { return objc_getAssociatedObject(o, key); }
static void weak_set(id o, char *key, id v) { objc_setAssociatedObject(o, key, v, 0); }
#define OBJP(fn, key) static id g_##fn(id self, SEL c) { (void)c; return pget(self, key); } static void s_##fn(id self, SEL c, id v) { (void)c; pset(self, key, v); }
#define INTP(fn, key, def) static i64 g_##fn(id self, SEL c) { (void)c; return pgi(self, key, def); } static void s_##fn(id self, SEL c, i64 v) { (void)c; psi(self, key, v); }
#define DBLP(fn, key, def) static double g_##fn(id self, SEL c) { (void)c; return pgd(self, key, def); } static void s_##fn(id self, SEL c, double v) { (void)c; psd(self, key, v); }
#define BOOLP(fn, key, def) static BOOL g_##fn(id self, SEL c) { (void)c; return pgi(self, key, def) != 0; } static void s_##fn(id self, SEL c, BOOL v) { (void)c; psi(self, key, v ? 1 : 0); }
static id g_delegate(id self, SEL c) { (void)c; return weak_get(self, &k_delegate); }
static void s_delegate(id self, SEL c, id d) { (void)c; weak_set(self, &k_delegate, d); }
static id g_target(id self, SEL c) { (void)c; return weak_get(self, &k_target); }
static void s_target(id self, SEL c, id t) { (void)c; weak_set(self, &k_target, t); }
static SEL g_action(id self, SEL c) { (void)c; id v = pget(self, "action"); return v ? sl(cx_cstr(v)) : 0; }
static void s_action(id self, SEL c, SEL a) { (void)c; pset(self, "action", a ? cx_str(sel_getName(a)) : 0); }
static BOOL yes_imp(id self, SEL c) { (void)self; (void)c; return YES; }
static BOOL no_imp(id self, SEL c) { (void)self; (void)c; return NO; }
static void noop_imp(id self, SEL c) { (void)self; (void)c; }
static void noop1_imp(id self, SEL c, id a) { (void)self; (void)c; (void)a; }

/* ================================================================ gestos */
static char k_core, k_grs;
static gr_core *core_of(id gr) {
    id d = objc_getAssociatedObject(gr, &k_core);
    if (!d) {
        d = MSG(id, (id)kls("NSMutableData"), "dataWithLength:", (u64)sizeof(gr_core));
        objc_setAssociatedObject(gr, &k_core, d, 1);
        gr_core *g = MSG0(gr_core *, d, "mutableBytes");
        int kind = GR_K_CLICK;
        cx_class_t *k = object_getClass(gr);
        if (cx_is_subclass(k, &NSPanGestureRecognizer_c)) kind = GR_K_PAN;
        else if (cx_is_subclass(k, &NSPressGestureRecognizer_c)) kind = GR_K_PRESS;
        else if (cx_is_subclass(k, &NSMagnificationGestureRecognizer_c)) kind = GR_K_MAGNIFY;
        else if (cx_is_subclass(k, &NSRotationGestureRecognizer_c)) kind = GR_K_ROTATE;
        gr_init(g, kind);
    }
    return MSG0(gr_core *, d, "mutableBytes");
}
static id grv_init(id self, SEL c, id target, SEL action) {
    (void)c;
    core_of(self);
    psi(self, "enabled", 1);
    weak_set(self, &k_target, target);
    s_action(self, 0, action);
    return self;
}
static id grv_init_plain(id self, SEL c) { return grv_init(self, c, 0, 0); }
static id grv_init_coder(id self, SEL c, id coder) { (void)coder; return grv_init(self, c, 0, 0); }
static id grv_view(id self, SEL c) { (void)c; return weak_get(self, &k_view); }
static i64 grv_state(id self, SEL c) { (void)c; return core_of(self)->state; }
static void grv_fire(id self) {
    SEL a = g_action(self, 0);
    id t = weak_get(self, &k_target);
    if (!a) return;
    if (t) ((void (*)(id, SEL, id))objc_msgSend)(t, a, self);
    else MSG(BOOL, NSApp, "sendAction:to:from:", a, (id)0, self);
}
/* mudar o estado: Began/Changed/Ended disparam a ação; Possible→Began/Ended consulta o delegate */
static void grv_set_state(id self, SEL c, i64 st) {
    (void)c;
    gr_core *g = core_of(self);
    i64 old = g->state;
    if (old == GR_POSSIBLE && (st == GR_BEGAN || st == GR_ENDED)) {
        id d = weak_get(self, &k_delegate);
        if (d && responds(d, "gestureRecognizerShouldBegin:") && !MSG(BOOL, d, "gestureRecognizerShouldBegin:", self)) { g->state = GR_FAILED; return; }
    }
    g->state = (int)st;
    if (st == GR_BEGAN || st == GR_CHANGED || st == GR_ENDED || st == GR_CANCELLED) grv_fire(self);
}
static BOOL grv_enabled(id self, SEL c) { (void)c; return pgi(self, "enabled", 1) != 0; }
static void grv_set_enabled(id self, SEL c, BOOL b) {
    (void)c;
    psi(self, "enabled", b);
    gr_core *g = core_of(self);
    if (!b && (g->state == GR_BEGAN || g->state == GR_CHANGED)) grv_set_state(self, 0, GR_CANCELLED);
}
static P grv_location(id self, SEL c, id view) {
    (void)c;
    gr_core *g = core_of(self);
    P w = {g->x, g->y};
    if (!view) return w;
    return ((P (*)(id, SEL, P, id))objc_msgSend)(view, sl("convertPoint:fromView:"), w, (id)0);
}
static void grv_reset(id self, SEL c) { (void)self; (void)c; }
static BOOL grv_can_prevent(id self, SEL c, id other) { (void)self; (void)c; (void)other; return YES; }
static BOOL grv_can_be_prevented(id self, SEL c, id other) { (void)self; (void)c; (void)other; return YES; }
static BOOL grv_should_require(id self, SEL c, id other) {
    (void)c;
    id req = pget(self, "requires");
    return req && MSG(BOOL, req, "containsObject:", other);
}
static void grv_require_fail(id self, SEL c, id other) { (void)c; id a = pget(self, "requires"); if (!a) { a = arr(); pset(self, "requires", a); } if (other) MSG(void, a, "addObject:", other); }
BOOLP(delaysPrimary, "delaysPrimary", 0) BOOLP(delaysSecondary, "delaysSecondary", 0) BOOLP(delaysOther, "delaysOther", 0)
BOOLP(delaysKey, "delaysKey", 0) BOOLP(delaysMag, "delaysMag", 0) BOOLP(delaysRot, "delaysRot", 0)
OBJP(grName, "name") OBJP(pressureConfig, "pressureConfiguration")
INTP(allowedTouchTypes, "touchTypes", 1)
/* NSView */
static id view_grs(id self, SEL c) { (void)c; id a = objc_getAssociatedObject(self, &k_grs); return a ? a : objc_autorelease(MSG0(id, (id)kls("NSArray"), "new")); }
static void view_add_gr(id self, SEL c, id gr) {
    (void)c;
    if (!gr) return;
    id a = objc_getAssociatedObject(self, &k_grs);
    if (!a) { a = arr(); objc_setAssociatedObject(self, &k_grs, a, 1); }
    id old = weak_get(gr, &k_view);
    if (old && old != self) { id oa = objc_getAssociatedObject(old, &k_grs); if (oa) MSG(void, oa, "removeObjectIdenticalTo:", gr); }
    if (!MSG(BOOL, a, "containsObject:", gr)) MSG(void, a, "addObject:", gr);
    weak_set(gr, &k_view, self);
}
static void view_remove_gr(id self, SEL c, id gr) {
    (void)c;
    id a = objc_getAssociatedObject(self, &k_grs);
    if (a && gr) { MSG(void, a, "removeObjectIdenticalTo:", gr); weak_set(gr, &k_view, 0); }
}
static void view_set_grs(id self, SEL c, id list) {
    (void)c;
    id a = objc_getAssociatedObject(self, &k_grs);
    while (a && MSG0(u64, a, "count")) view_remove_gr(self, 0, MSG0(id, a, "lastObject"));
    for (u64 i = 0, n = list ? MSG0(u64, list, "count") : 0; i < n; i++) view_add_gr(self, 0, MSG(id, list, "objectAtIndex:", i));
}
static void view_mouse_down_fwd(id self, SEL c, id ev) { (void)c; id sv = MSG0(id, self, "superview"); if (sv) MSG(void, sv, "mouseDown:", ev); }

/* subclasses do sistema: a máquina de estados do núcleo */
static void grk_feed(id self, int e, id ev) {
    gr_core *g = core_of(self);
    if (!grv_enabled(self, 0)) return;
    P w = ev ? ((P (*)(id, SEL))objc_msgSend)(ev, sl("locationInWindow")) : (P){g->x, g->y};
    int button = ev ? (int)MSG0(i64, ev, "buttonNumber") : 0;
    int clicks = ev ? (int)MSG0(i64, ev, "clickCount") : 1;
    double amount = 0;
    int end = 0;
    if (e == GE_MAG || e == GE_ROT) {
        amount = e == GE_MAG ? MSG0(double, ev, "magnification") : MSG0(float, ev, "rotation");
        u64 ph = MSG0(u64, ev, "phase");   /* NSEventPhaseEnded 8, Cancelled 16 */
        end = (ph & (8 | 16)) != 0;
    }
    int before = g->state;
    int fired = gr_feed(g, e, w.x, w.y, e == GE_TICK ? now_s() : (ev ? MSG0(double, ev, "timestamp") : now_s()), button, clicks, amount, end);
    if (!fired) return;
    int now = g->state;
    g->state = before;                      /* setState: decide (delegate) e dispara a ação */
    grv_set_state(self, 0, now);
}
static void grk_down(id self, SEL c, id ev) { (void)c; grk_feed(self, GE_DOWN, ev); }
static void grk_drag(id self, SEL c, id ev) { (void)c; grk_feed(self, GE_DRAG, ev); }
static void grk_up(id self, SEL c, id ev) { (void)c; grk_feed(self, GE_UP, ev); }
static void grk_mag(id self, SEL c, id ev) { (void)c; grk_feed(self, GE_MAG, ev); }
static void grk_rot(id self, SEL c, id ev) { (void)c; grk_feed(self, GE_ROT, ev); }
static i64 grk_mask(id self, SEL c) { (void)c; return core_of(self)->button_mask; }
static void grk_set_mask(id self, SEL c, i64 m) { (void)c; core_of(self)->button_mask = (int)m; }
static i64 grk_clicks(id self, SEL c) { (void)c; return core_of(self)->clicks_required; }
static void grk_set_clicks(id self, SEL c, i64 n) { (void)c; core_of(self)->clicks_required = (int)(n > 0 ? n : 1); }
static double grk_min_dur(id self, SEL c) { (void)c; return core_of(self)->min_duration; }
static void grk_set_min_dur(id self, SEL c, double d) { (void)c; core_of(self)->min_duration = d; }
static double grk_allow(id self, SEL c) { (void)c; return core_of(self)->allowable; }
static void grk_set_allow(id self, SEL c, double d) { (void)c; core_of(self)->allowable = d; }
static i64 grk_touches(id self, SEL c) { (void)self; (void)c; return 1; }
static P pan_translation(id self, SEL c, id view) {
    (void)c;
    gr_core *g = core_of(self);
    P a = {g->x0 + pgd(self, "tx0", 0), g->y0 + pgd(self, "ty0", 0)}, b = {g->x, g->y};
    if (view) {
        a = ((P (*)(id, SEL, P, id))objc_msgSend)(view, sl("convertPoint:fromView:"), a, (id)0);
        b = ((P (*)(id, SEL, P, id))objc_msgSend)(view, sl("convertPoint:fromView:"), b, (id)0);
    }
    return (P){b.x - a.x, b.y - a.y};
}
static void pan_set_translation(id self, SEL c, P t, id view) {
    (void)c; (void)view;
    gr_core *g = core_of(self);
    /* a translação passa a valer t: move a origem de referência */
    psd(self, "tx0", g->x - t.x - g->x0); psd(self, "ty0", g->y - t.y - g->y0);
}
static P pan_velocity(id self, SEL c, id view) {
    (void)c;
    gr_core *g = core_of(self);
    double dt = now_s() - g->t0;
    P t = pan_translation(self, 0, view);
    return dt > 0 ? (P){t.x / dt, t.y / dt} : (P){0, 0};
}
static double mag_value(id self, SEL c) { (void)c; return core_of(self)->magnification; }
static void mag_set(id self, SEL c, double v) { (void)c; core_of(self)->magnification = v; }
static double rot_rad(id self, SEL c) { (void)c; return core_of(self)->rotation * 3.14159265358979323846 / 180.0; }
static void rot_set_rad(id self, SEL c, double v) { (void)c; core_of(self)->rotation = v * 180.0 / 3.14159265358979323846; }
static double rot_deg(id self, SEL c) { (void)c; return core_of(self)->rotation; }
static void rot_set_deg(id self, SEL c, double v) { (void)c; core_of(self)->rotation = v; }

/* roteamento: no mouseDown a janela junta os reconhecedores da view clicada e dos ancestrais */
#define GR_MAX 16
static id g_active[GR_MAX];
static int g_nactive, g_seq, g_recognized, g_delayed;
static id g_pending_down, g_pending_view;
static void active_clear(void) { for (int i = 0; i < g_nactive; i++) objc_release(g_active[i]); g_nactive = 0; }
static void reset_done(void) {
    for (int i = 0; i < g_nactive; i++) {
        gr_core *g = core_of(g_active[i]);
        if (g->state == GR_ENDED || g->state == GR_CANCELLED || g->state == GR_FAILED) {
            g->down = 0;
            gr_reset_if_done(g);
            g->state = GR_POSSIBLE;
            psd(g_active[i], "tx0", 0); psd(g_active[i], "ty0", 0);
            MSG0(void, g_active[i], "reset");
        }
    }
}
static int any_state(int st) { for (int i = 0; i < g_nactive; i++) if (core_of(g_active[i])->state == st) return 1; return 0; }
static int any_recognizing(void) {
    for (int i = 0; i < g_nactive; i++) { int s = core_of(g_active[i])->state; if (s == GR_BEGAN || s == GR_CHANGED || s == GR_ENDED) return 1; }
    return 0;
}
static int delaying_alive(void) {
    for (int i = 0; i < g_nactive; i++)
        if (pgi(g_active[i], "delaysPrimary", 0) && core_of(g_active[i])->state == GR_POSSIBLE) return 1;
    return 0;
}
/* mouse retido por um reconhecedor que falhou: a view recebe o mouseDown: atrasado */
static void flush_delayed(void) {
    if (!g_delayed || delaying_alive()) return;
    g_delayed = 0;
    if (!g_recognized && g_pending_view && g_pending_down) MSG(void, g_pending_view, "mouseDown:", g_pending_down);
    if (g_pending_down) objc_release(g_pending_down);
    if (g_pending_view) objc_release(g_pending_view);
    g_pending_down = g_pending_view = 0;
}
static void press_tick(void *ctx) {
    if ((int)(long)ctx != g_seq) return;     /* outro clique já começou */
    for (int i = 0; i < g_nactive; i++)
        if (cx_is_subclass(object_getClass(g_active[i]), &NSPressGestureRecognizer_c)) grk_feed(g_active[i], GE_TICK, 0);
    if (any_recognizing()) g_recognized = 1;
    flush_delayed();
}
/* phase: 0 mouseDown, 1 mouseDragged, 2 mouseUp, 3 rightMouseDown, 4 rightMouseUp.
 * Devolve 1 se a view NÃO deve receber o evento (um reconhecedor já reconheceu ou retém o clique). */
int cx_gr_mouse(id view, id ev, int phase) {
    if (phase == 0 || phase == 3) {
        active_clear();
        g_recognized = 0; g_seq++;
        if (g_pending_down) { objc_release(g_pending_down); g_pending_down = 0; }
        if (g_pending_view) { objc_release(g_pending_view); g_pending_view = 0; }
        g_delayed = 0;
        for (id v = view; v && g_nactive < GR_MAX; v = MSG0(id, v, "superview")) {
            id a = objc_getAssociatedObject(v, &k_grs);
            for (u64 i = 0, n = a ? MSG0(u64, a, "count") : 0; i < n && g_nactive < GR_MAX; i++) {
                id gr = MSG(id, a, "objectAtIndex:", i);
                if (!grv_enabled(gr, 0)) continue;
                id d = weak_get(gr, &k_delegate);
                if (d && responds(d, "gestureRecognizer:shouldAttemptToRecognizeWithEvent:") &&
                    !MSG(BOOL, d, "gestureRecognizer:shouldAttemptToRecognizeWithEvent:", gr, ev)) continue;
                g_active[g_nactive++] = objc_retain(gr);
            }
            if (!responds(v, "superview")) break;
        }
        if (!g_nactive) return 0;
        for (int i = 0; i < g_nactive; i++) MSG(void, g_active[i], phase == 3 ? "rightMouseDown:" : "mouseDown:", ev);
        int has_press = 0;
        for (int i = 0; i < g_nactive; i++) if (cx_is_subclass(object_getClass(g_active[i]), &NSPressGestureRecognizer_c)) has_press = 1;
        if (has_press) {
            double d = 0.5;
            for (int i = 0; i < g_nactive; i++) if (cx_is_subclass(object_getClass(g_active[i]), &NSPressGestureRecognizer_c)) d = core_of(g_active[i])->min_duration;
            dispatch_after_f(dispatch_time(0, (long)(d * 1e9) + 1000000), dispatch_get_main_queue(), (void *)(long)g_seq, press_tick);
        }
        if (any_recognizing()) g_recognized = 1;
        if (phase == 0 && delaying_alive()) {
            g_delayed = 1;
            g_pending_down = objc_retain(ev); g_pending_view = objc_retain(view);
            return 1;
        }
        return g_recognized;
    }
    if (!g_nactive) return 0;
    const char *s = phase == 1 ? "mouseDragged:" : phase == 4 ? "rightMouseUp:" : "mouseUp:";
    for (int i = 0; i < g_nactive; i++) MSG(void, g_active[i], s, ev);
    if (any_recognizing()) g_recognized = 1;
    int was_delayed = g_delayed;
    flush_delayed();
    int swallow = g_recognized || (was_delayed && g_delayed);
    if (phase == 2 || phase == 4) {
        reset_done();
        if (!any_state(GR_BEGAN) && !any_state(GR_CHANGED)) active_clear();
        /* o mouseDown: foi entregue: o mouseUp: também vai, para a view não ficar presa no meio de um clique */
        if (!was_delayed) swallow = 0;
    }
    return swallow;
}
/* magnify/rotate: entregues aos reconhecedores da view sob o cursor e dos ancestrais */
int cx_gr_gesture(id view, id ev) {
    int used = 0;
    const char *s = MSG0(u64, ev, "type") == 30 ? "magnifyWithEvent:" : "rotateWithEvent:";
    for (id v = view; v; v = MSG0(id, v, "superview")) {
        id a = objc_getAssociatedObject(v, &k_grs);
        for (u64 i = 0, n = a ? MSG0(u64, a, "count") : 0; i < n; i++) {
            id gr = MSG(id, a, "objectAtIndex:", i);
            if (!responds(gr, s) || !grv_enabled(gr, 0)) continue;
            MSG(void, gr, s, ev);
            gr_core *g = core_of(gr);
            if (g->state != GR_POSSIBLE) used = 1;
            if (g->state == GR_ENDED || g->state == GR_FAILED || g->state == GR_CANCELLED) { gr_reset_if_done(g); MSG0(void, gr, "reset"); }
        }
        if (!responds(v, "superview")) break;
    }
    return used;
}

/* ================================================================ NSTouch */
OBJP(tIdentity, "identity") OBJP(tDevice, "device")
INTP(tPhase, "phase", 0) INTP(tType, "type", 1) BOOLP(tResting, "resting", 0)
static P t_norm(id self, SEL c) { (void)c; return (P){pgd(self, "nx", 0), pgd(self, "ny", 0)}; }
static S t_dev_size(id self, SEL c) { (void)c; return (S){pgd(self, "dw", 0), pgd(self, "dh", 0)}; }
static P t_loc(id self, SEL c, id view) {
    (void)c;
    P w = {pgd(self, "wx", 0), pgd(self, "wy", 0)};
    return view ? ((P (*)(id, SEL, P, id))objc_msgSend)(view, sl("convertPoint:fromView:"), w, (id)0) : w;
}
static P t_prev(id self, SEL c, id view) {
    (void)c;
    P w = {pgd(self, "px", pgd(self, "wx", 0)), pgd(self, "py", pgd(self, "wy", 0))};
    return view ? ((P (*)(id, SEL, P, id))objc_msgSend)(view, sl("convertPoint:fromView:"), w, (id)0) : w;
}
static id t_copy(id self, SEL c, void *z) { (void)c; (void)z; return objc_retain(self); }
static id t_desc(id self, SEL c) {
    (void)c;
    char b[160];
    snprintf(b, sizeof b, "<NSTouch: %p phase=%lld normalizedPosition={%.3f, %.3f}>", (void *)self, (long long)pgi(self, "phase", 0), pgd(self, "nx", 0), pgd(self, "ny", 0));
    return cx_str(b);
}
/* toque montado por quem tem um dispositivo multitoque (ninguém, hoje) */
id cx_touch_make(id identity, u64 phase, double nx, double ny, double dev_w, double dev_h, int resting) {
    id t = objc_autorelease(cx_alloc_instance(&NSTouch_c));
    pset(t, "identity", identity); psi(t, "phase", (i64)phase); psd(t, "nx", nx); psd(t, "ny", ny);
    psd(t, "dw", dev_w); psd(t, "dh", dev_h); psi(t, "resting", resting); psi(t, "type", 1);
    return t;
}
static id ev_touches(id self, SEL c, u64 phase, id view) { (void)self; (void)c; (void)phase; (void)view; return MSG0(id, (id)kls("NSSet"), "set"); }
static id ev_all_touches(id self, SEL c) { (void)self; (void)c; return MSG0(id, (id)kls("NSSet"), "set"); }
static id ev_touches_for_view(id self, SEL c, id view) { (void)self; (void)c; (void)view; return MSG0(id, (id)kls("NSSet"), "set"); }

/* ================================================================ NSAnimation (quadros de verdade) */
static void an_post_mark(id self, double mark) {
    id d = weak_get(self, &k_delegate);
    if (d && responds(d, "animation:didReachProgressMark:")) ((void (*)(id, SEL, id, float))objc_msgSend)(d, sl("animation:didReachProgressMark:"), self, (float)mark);
    extern id NSAnimationProgressMarkNotification, NSAnimationProgressMark;
    id info = MSG(id, (id)kls("NSDictionary"), "dictionaryWithObject:forKey:", cx_num_d(mark), NSAnimationProgressMark);
    MSG(void, MSG0(id, (id)kls("NSNotificationCenter"), "defaultCenter"), "postNotificationName:object:userInfo:", NSAnimationProgressMarkNotification, self, info);
    /* encadeamento: animações que esperam esta marca */
    id links = pget(self, "links");
    for (u64 i = 0, n = links ? MSG0(u64, links, "count") : 0; i < n; i++) {
        id l = MSG(id, links, "objectAtIndex:", i);
        if (MSG0(double, MSG(id, l, "objectAtIndex:", (u64)1), "doubleValue") != mark) continue;
        id other = MSG(id, l, "objectAtIndex:", (u64)0);
        MSG0(void, other, MSG0(i64, MSG(id, l, "objectAtIndex:", (u64)2), "longLongValue") ? "startAnimation" : "stopAnimation");
    }
}
static double an_progress(id self, SEL c) { (void)c; return pgd(self, "progress", 0); }
static void an_set_progress(id self, SEL c, float pf) {
    (void)c;
    double p = pf < 0 ? 0 : pf > 1 ? 1 : pf, old = pgd(self, "progress", 0);
    psd(self, "progress", p);
    id marks = pget(self, "marks");
    for (u64 i = 0, n = marks ? MSG0(u64, marks, "count") : 0; i < n; i++) {
        double m = MSG0(double, MSG(id, marks, "objectAtIndex:", i), "doubleValue");
        if ((old < m && p >= m) || (p == 0 && m == 0 && old == 0 && pgi(self, "frame", 0) == 0)) an_post_mark(self, m);
    }
}
static float an_value(id self, SEL c) {
    (void)c;
    double p = pgd(self, "progress", 0);
    id d = weak_get(self, &k_delegate);
    if (d && responds(d, "animation:valueForProgress:")) return ((float (*)(id, SEL, id, float))objc_msgSend)(d, sl("animation:valueForProgress:"), self, (float)p);
    return (float)amc_curve((int)pgi(self, "curve", 0), p);
}
static id an_init(id self, SEL c, double dur, i64 curve) { (void)c; psd(self, "duration", dur); psi(self, "curve", curve); psd(self, "frameRate", 60); return self; }
static id an_init_plain(id self, SEL c) { return an_init(self, c, 1.0, 0); }
DBLP(anDuration, "duration", 1) INTP(anCurve, "curve", 0) INTP(anBlocking, "blocking", 0)
static float an_frame_rate(id self, SEL c) { (void)c; return (float)pgd(self, "frameRate", 60); }
static void an_set_frame_rate(id self, SEL c, float f) { (void)c; psd(self, "frameRate", f >= 0 ? f : 0); }
static BOOL an_animating(id self, SEL c) { (void)c; return pgi(self, "running", 0) != 0; }
static void an_finish(id self) {
    psi(self, "running", 0);
    id d = weak_get(self, &k_delegate);
    if (d && responds(d, "animationDidEnd:")) MSG(void, d, "animationDidEnd:", self);
    objc_release(self);   /* a referência da corrida (an_start) */
}
static void display_windows(void) {
    id ws = MSG0(id, NSApp, "windows");
    for (u64 i = 0, n = ws ? MSG0(u64, ws, "count") : 0; i < n; i++) {
        id w = MSG(id, ws, "objectAtIndex:", i);
        if (MSG0(BOOL, w, "isVisible")) MSG0(void, w, "displayIfNeeded");
    }
}
static void an_step(id self) {
    double dur = pgd(self, "duration", 1), t = now_s() - pgd(self, "t0", 0);
    double p = dur > 0 ? t / dur : 1;
    if (p > 1) p = 1;
    psi(self, "frame", pgi(self, "frame", 0) + 1);
    ((void (*)(id, SEL, float))objc_msgSend)(self, sl("setCurrentProgress:"), (float)p);
}
static void an_tick(void *ctx) {
    id self = ctx;
    if (!pgi(self, "running", 0) || pgi(self, "token", 0) != pgi(self, "tickTok", 0)) { objc_release(self); return; }
    an_step(self);
    if (pgd(self, "progress", 0) >= 1) { an_finish(self); objc_release(self); return; }
    double fr = pgd(self, "frameRate", 60);
    if (fr <= 0 || fr > 60) fr = 60;
    dispatch_after_f(dispatch_time(0, (long)(1e9 / fr)), dispatch_get_main_queue(), self, an_tick);
}
static void an_start(id self, SEL c) {
    (void)c;
    if (pgi(self, "running", 0)) return;
    id d = weak_get(self, &k_delegate);
    if (d && responds(d, "animationShouldStart:") && !MSG(BOOL, d, "animationShouldStart:", self)) return;
    psi(self, "running", 1);
    psi(self, "frame", 0);
    objc_retain(self);
    double p0 = pgd(self, "progress", 0);
    if (p0 >= 1) p0 = 0;
    psd(self, "t0", now_s() - p0 * pgd(self, "duration", 1));
    if (pgi(self, "blocking", 0) == 0) {
        /* NSAnimationBlocking: quadros aqui mesmo, com a tela redesenhada a cada passo; volta só no fim */
        double fr = pgd(self, "frameRate", 60);
        if (fr <= 0 || fr > 60) fr = 60;
        while (pgi(self, "running", 0)) {
            an_step(self);
            display_windows();
            if (pgd(self, "progress", 0) >= 1) break;
            usleep((unsigned)(1e6 / fr));
        }
        if (pgi(self, "running", 0)) an_finish(self); else objc_release(self);
        return;
    }
    /* não bloqueante: quadros na fila principal; o fim nunca acontece dentro de quem chamou startAnimation */
    i64 tok = pgi(self, "token", 0) + 1;
    psi(self, "token", tok); psi(self, "tickTok", tok);
    objc_retain(self);
    dispatch_after_f(dispatch_time(0, 1000000), dispatch_get_main_queue(), self, an_tick);
}
static void an_stop(id self, SEL c) {
    (void)c;
    if (!pgi(self, "running", 0)) return;
    psi(self, "running", 0);
    psi(self, "token", pgi(self, "token", 0) + 1);
    id d = weak_get(self, &k_delegate);
    if (d && responds(d, "animationDidStop:")) MSG(void, d, "animationDidStop:", self);
    if (pgi(self, "blocking", 0) != 0) objc_release(self);   /* no bloqueante o laço solta a referência */
}
static id an_marks(id self, SEL c) { (void)c; id m = pget(self, "marks"); return m ? m : MSG0(id, (id)kls("NSArray"), "array"); }
static void an_set_marks(id self, SEL c, id a) { (void)c; pset(self, "marks", a ? MSG0(id, a, "mutableCopy") : 0); if (a) objc_release(pget(self, "marks")); }
static void an_add_mark(id self, SEL c, float m) {
    (void)c;
    id a = pget(self, "marks");
    if (!a) { a = arr(); pset(self, "marks", a); }
    id n = cx_num_d(m < 0 ? 0 : m > 1 ? 1 : m);
    if (!MSG(BOOL, a, "containsObject:", n)) MSG(void, a, "addObject:", n);
    MSG(void, a, "sortUsingSelector:", sl("compare:"));
}
static void an_remove_mark(id self, SEL c, float m) { (void)c; id a = pget(self, "marks"); if (a) MSG(void, a, "removeObject:", cx_num_d(m)); }
static void an_link(id self, id other, float p, int start) {
    if (!other) return;
    an_add_mark(other, 0, p);
    id links = pget(other, "links");
    if (!links) { links = arr(); pset(other, "links", links); }
    id l = arr();
    MSG(void, l, "addObject:", self); MSG(void, l, "addObject:", cx_num_d(p)); MSG(void, l, "addObject:", cx_num_i(start));
    MSG(void, links, "addObject:", l);
}
static void an_unlink(id self, int start) {
    id other = pget(self, start ? "startLink" : "stopLink");
    if (!other) return;
    id links = pget(other, "links");
    for (u64 i = links ? MSG0(u64, links, "count") : 0; i-- > 0;) {
        id l = MSG(id, links, "objectAtIndex:", i);
        if (MSG(id, l, "objectAtIndex:", (u64)0) == self && MSG0(i64, MSG(id, l, "objectAtIndex:", (u64)2), "longLongValue") == start) MSG(void, links, "removeObjectAtIndex:", i);
    }
    pset(self, start ? "startLink" : "stopLink", 0);
}
static void an_start_when(id self, SEL c, id other, float p) { (void)c; an_unlink(self, 1); an_link(self, other, p, 1); pset(self, "startLink", other); }
static void an_stop_when(id self, SEL c, id other, float p) { (void)c; an_unlink(self, 0); an_link(self, other, p, 0); pset(self, "stopLink", other); }
static void an_clear_start(id self, SEL c) { (void)c; an_unlink(self, 1); }
static void an_clear_stop(id self, SEL c) { (void)c; an_unlink(self, 0); }
static id an_modes(id self, SEL c) { (void)self; (void)c; return 0; }

/* ---- NSViewAnimation */
extern id NSViewAnimationTargetKey, NSViewAnimationStartFrameKey, NSViewAnimationEndFrameKey, NSViewAnimationEffectKey,
    NSViewAnimationFadeInEffect, NSViewAnimationFadeOutEffect;
static id vanim_init(id self, SEL c, id anims) {
    (void)c;
    an_init(self, 0, 0.5, 0);
    pset(self, "viewAnims", anims ? MSG0(id, anims, "copy") : 0);
    if (anims) objc_release(pget(self, "viewAnims"));
    return self;
}
static id vanim_anims(id self, SEL c) { (void)c; return pget(self, "viewAnims"); }
static void vanim_set_anims(id self, SEL c, id a) { (void)c; pset(self, "viewAnims", a); }
static int is_window(id o) { return MSG(BOOL, o, "isKindOfClass:", (id)kls("NSWindow")); }
static R target_frame(id t) { return msg_rect(t, "frame"); }
static void vanim_prepare(id self) {
    id a = pget(self, "viewAnims");
    id starts = arr();
    for (u64 i = 0, n = a ? MSG0(u64, a, "count") : 0; i < n; i++) {
        id d = MSG(id, a, "objectAtIndex:", i);
        id t = MSG(id, d, "objectForKey:", NSViewAnimationTargetKey);
        id sf = MSG(id, d, "objectForKey:", NSViewAnimationStartFrameKey);
        R f = sf ? val_rect(sf) : (t ? target_frame(t) : (R){0, 0, 0, 0});
        MSG(void, starts, "addObject:", rect_val(f));
        id eff = MSG(id, d, "objectForKey:", NSViewAnimationEffectKey);
        if (t && eff && MSG(BOOL, eff, "isEqual:", NSViewAnimationFadeInEffect)) {
            if (is_window(t)) { MSG(void, t, "setAlphaValue:", 0.0); MSG(void, t, "orderFront:", (id)0); }
            else MSG(void, t, "setHidden:", (BOOL)NO);
        }
    }
    pset(self, "starts", starts);
}
static void vanim_set_progress(id self, SEL c, float pf) {
    an_set_progress(self, c, pf);
    if (!pget(self, "starts")) vanim_prepare(self);
    double v = an_value(self, 0), p = pgd(self, "progress", 0);
    id a = pget(self, "viewAnims"), starts = pget(self, "starts");
    for (u64 i = 0, n = a ? MSG0(u64, a, "count") : 0; i < n; i++) {
        id d = MSG(id, a, "objectAtIndex:", i);
        id t = MSG(id, d, "objectForKey:", NSViewAnimationTargetKey);
        if (!t) continue;
        R s = val_rect(MSG(id, starts, "objectAtIndex:", i));
        id ef = MSG(id, d, "objectForKey:", NSViewAnimationEndFrameKey);
        if (ef) {
            R e = val_rect(ef), f = {s.x + (e.x - s.x) * v, s.y + (e.y - s.y) * v, s.w + (e.w - s.w) * v, s.h + (e.h - s.h) * v};
            if (is_window(t)) ((void (*)(id, SEL, R, BOOL))objc_msgSend)(t, sl("setFrame:display:"), f, (BOOL)YES);
            else { m_set_frame(t, f); redraw(MSG0(id, t, "superview")); }
        }
        id eff = MSG(id, d, "objectForKey:", NSViewAnimationEffectKey);
        if (eff) {
            int in = MSG(BOOL, eff, "isEqual:", NSViewAnimationFadeInEffect);
            double alpha = in ? v : 1 - v;
            if (responds(t, "setAlphaValue:")) MSG(void, t, "setAlphaValue:", alpha);
            if (p >= 1 && !in) { if (is_window(t)) MSG(void, t, "orderOut:", (id)0); else MSG(void, t, "setHidden:", (BOOL)YES); }
        }
    }
    if (p >= 1) pset(self, "starts", 0);
}
static void vanim_start(id self, SEL c) { pset(self, "starts", 0); psd(self, "progress", 0); an_start(self, c); }
static void vanim_stop(id self, SEL c) { an_stop(self, c); pset(self, "starts", 0); }

/* ================================================================ NSController / NSObjectController */
static char k_content;
static BOOL ctr_commit(id self, SEL c) { (void)c; id e = pget(self, "editors"); while (e && MSG0(u64, e, "count")) { id ed = MSG0(id, e, "lastObject"); if (responds(ed, "commitEditing") && !MSG0(BOOL, ed, "commitEditing")) return NO; MSG(void, e, "removeObjectIdenticalTo:", ed); } return YES; }
static void ctr_discard(id self, SEL c) { (void)c; id e = pget(self, "editors"); for (u64 i = 0, n = e ? MSG0(u64, e, "count") : 0; i < n; i++) { id ed = MSG(id, e, "objectAtIndex:", i); if (responds(ed, "discardEditing")) MSG0(void, ed, "discardEditing"); } if (e) MSG0(void, e, "removeAllObjects"); }
static BOOL ctr_editing(id self, SEL c) { (void)c; id e = pget(self, "editors"); return e && MSG0(u64, e, "count") > 0; }
static void ctr_begin(id self, SEL c, id ed) { (void)c; id e = pget(self, "editors"); if (!e) { e = arr(); pset(self, "editors", e); } if (ed && !MSG(BOOL, e, "containsObject:", ed)) MSG(void, e, "addObject:", ed); }
static void ctr_end(id self, SEL c, id ed) { (void)c; id e = pget(self, "editors"); if (e) MSG(void, e, "removeObjectIdenticalTo:", ed); }
static void ctr_commit_delegate(id self, SEL c, id del, SEL did, void *ctx) {
    (void)c;
    BOOL ok = ctr_commit(self, 0);
    if (del && did) ((void (*)(id, SEL, id, BOOL, void *))objc_msgSend)(del, did, self, ok, ctx);
}
static BOOL ctr_commit_error(id self, SEL c, id *err) { (void)c; if (err) *err = 0; return ctr_commit(self, 0); }
static void kvo_will(id self, const char *k) { if (responds(self, "willChangeValueForKey:")) MSG(void, self, "willChangeValueForKey:", cx_str(k)); }
static void kvo_did(id self, const char *k) { if (responds(self, "didChangeValueForKey:")) MSG(void, self, "didChangeValueForKey:", cx_str(k)); }
static id oc_init_content(id self, SEL c, id content) {
    (void)c;
    psi(self, "editable", 1);
    pset(self, "objectClass", (id)kls("NSMutableDictionary"));
    if (content) objc_setAssociatedObject(self, &k_content, content, 1);
    return self;
}
static id oc_init(id self, SEL c) { return oc_init_content(self, c, 0); }
static id oc_init_coder(id self, SEL c, id coder) { (void)coder; return oc_init_content(self, c, 0); }
static id oc_content(id self, SEL c) { (void)c; return objc_getAssociatedObject(self, &k_content); }
static void oc_set_content(id self, SEL c, id v) {
    (void)c;
    kvo_will(self, "content"); kvo_will(self, "selection"); kvo_will(self, "selectedObjects"); kvo_will(self, "canRemove");
    objc_setAssociatedObject(self, &k_content, v, 1);
    kvo_did(self, "content"); kvo_did(self, "selection"); kvo_did(self, "selectedObjects"); kvo_did(self, "canRemove");
}
extern const void *NSNoSelectionMarker;
static id oc_selection(id self, SEL c) { (void)c; id v = oc_content(self, 0); return v ? v : (id)NSNoSelectionMarker; }
static id oc_selected(id self, SEL c) { (void)c; id v = oc_content(self, 0); return v ? MSG(id, (id)kls("NSArray"), "arrayWithObject:", v) : MSG0(id, (id)kls("NSArray"), "array"); }
static id oc_class(id self, SEL c) { (void)c; return pget(self, "objectClass"); }
static void oc_set_class(id self, SEL c, id k) { (void)c; pset(self, "objectClass", k); }
static id oc_new(id self, SEL c) { (void)c; id k = pget(self, "objectClass"); if (!k) k = (id)kls("NSMutableDictionary"); return MSG0(id, MSG0(id, k, "alloc"), "init"); }
static void oc_add_object(id self, SEL c, id o) { (void)c; oc_set_content(self, 0, o); }
static void oc_remove_object(id self, SEL c, id o) { (void)c; if (oc_content(self, 0) == o) oc_set_content(self, 0, 0); }
static void oc_add(id self, SEL c, id sender) { (void)c; (void)sender; if (!pgi(self, "editable", 1)) return; id o = MSG0(id, self, "newObject"); MSG(void, self, "addObject:", o); objc_release(o); }
static void oc_remove(id self, SEL c, id sender) { (void)c; (void)sender; if (!pgi(self, "editable", 1)) return; id v = oc_content(self, 0); if (v) MSG(void, self, "removeObject:", v); }
static BOOL oc_can_add(id self, SEL c) { (void)c; return pgi(self, "editable", 1) != 0; }
static BOOL oc_can_remove(id self, SEL c) { (void)c; return pgi(self, "editable", 1) && oc_content(self, 0) != 0; }
static void oc_prepare(id self, SEL c) { (void)c; if (!oc_content(self, 0)) { id o = MSG0(id, self, "newObject"); oc_set_content(self, 0, o); objc_release(o); } }
static void oc_fetch(id self, SEL c, id sender) { (void)sender; oc_prepare(self, c); }
static BOOL oc_validate(id self, SEL c, id item) {
    (void)c;
    SEL a = responds(item, "action") ? MSG0(SEL, item, "action") : 0;
    if (a == sl("add:")) return oc_can_add(self, 0);
    if (a == sl("remove:")) return oc_can_remove(self, 0);
    return YES;
}
BOOLP(ocEditable, "editable", 1) BOOLP(ocAutoPrepare, "autoPrepare", 0) BOOLP(ocLazy, "lazy", 0)
OBJP(ocEntity, "entityName") OBJP(ocPredicate, "fetchPredicate") OBJP(ocMOC, "moc")

/* ================================================================ NSFontManager e NSFontPanel */
id CTFontManagerCopyAvailableFontFamilyNames(void);
id CTFontManagerCopyAvailablePostScriptNames(void);
u32 CTFontGetSymbolicTraits(id f);
id CTFontCreateCopyWithSymbolicTraits(id f, double size, const void *matrix, u32 value, u32 mask);
id CTFontCreateCopyWithFamily(id f, double size, const void *matrix, id family);
static id g_fm, g_fp;
static id font_size(id f) { (void)f; return 0; }
static double fsize(id f) { double s = f ? MSG0(double, f, "pointSize") : 0; return s > 0 ? s : 13; }
static id sys_font(double s) { return MSG(id, (id)kls("NSFont"), "systemFontOfSize:", s); }
static id fm_families(id self, SEL c) {
    (void)self; (void)c;
    id a = arr(), ct = CTFontManagerCopyAvailableFontFamilyNames();
    if (ct) { MSG(void, a, "addObjectsFromArray:", ct); objc_release(ct); }
    if (!MSG(BOOL, a, "containsObject:", cx_str("Inter"))) MSG(void, a, "insertObject:atIndex:", cx_str("Inter"), (u64)0);
    MSG(void, a, "sortUsingSelector:", sl("caseInsensitiveCompare:"));
    return a;
}
static id fm_fonts(id self, SEL c) { (void)self; (void)c; id ct = CTFontManagerCopyAvailablePostScriptNames(); return ct ? objc_autorelease(ct) : arr(); }
/* membros de uma família: [nome PostScript, estilo, peso, traços] */
static id fm_members(id self, SEL c, id family) {
    (void)self; (void)c;
    if (!family) return 0;
    id out = arr();
    const char *f = cx_cstr(family);
    static const char *const faces[][2] = {{"Regular", ""}, {"Medium", "-Medium"}, {"SemiBold", "-SemiBold"}, {"Bold", "-Bold"}};
    static const int wt[] = {5, 6, 8, 9};
    int inter = !strcmp(f, "Inter") || !strcmp(f, ".AppleSystemUIFont") || !strcmp(f, "Helvetica");
    for (int i = 0; i < (inter ? 4 : 2); i++) {
        int k = inter ? i : (i ? 3 : 0);
        char ps[128];
        snprintf(ps, sizeof ps, "%s%s", inter ? "Inter" : f, k ? faces[k][1] : (inter ? "-Regular" : ""));
        id m = arr();
        MSG(void, m, "addObject:", cx_str(ps)); MSG(void, m, "addObject:", cx_str(faces[k][0]));
        MSG(void, m, "addObject:", cx_num_i(wt[k])); MSG(void, m, "addObject:", cx_num_i(wt[k] >= 8 ? 2 : 0));
        MSG(void, out, "addObject:", m);
    }
    return out;
}
/* NSFontTraitMask ↔ traços simbólicos do CoreText: itálico 1, negrito 2, expandido 0x20, condensado 0x40, mono 0x400 */
static u64 fm_traits(id self, SEL c, id font) {
    (void)self; (void)c;
    if (!font) return 0;
    u32 t = CTFontGetSymbolicTraits(font);
    u64 m = 0;
    if (t & 1) m |= 1; else m |= 0x1000000;
    if (t & 2) m |= 2; else m |= 4;
    if (t & 0x20) m |= 0x20;
    if (t & 0x40) m |= 0x40;
    if (t & 0x400) m |= 0x400;
    return m;
}
static i64 fm_weight(id self, SEL c, id font) { (void)self; (void)c; return font && (CTFontGetSymbolicTraits(font) & 2) ? 9 : 5; }
static id fm_with_traits(id font, u64 add, u64 remove) {
    if (!font) return 0;
    u32 val = 0, mask = 0;
    if (add & 2) { val |= 2; mask |= 2; }
    if (add & 1) { val |= 1; mask |= 1; }
    if (remove & 4 || remove & 2) mask |= 2;
    if (remove & 0x1000000 || remove & 1) mask |= 1;
    id f = CTFontCreateCopyWithSymbolicTraits(font, fsize(font), 0, val, mask);
    return f ? objc_autorelease(f) : font;
}
static id fm_convert_trait(id self, SEL c, id font, u64 trait) {
    (void)self; (void)c;
    if (trait & 4) return fm_with_traits(font, 0, 2);
    if (trait & 0x1000000) return fm_with_traits(font, 0, 1);
    return fm_with_traits(font, trait, 0);
}
static id fm_convert_not_trait(id self, SEL c, id font, u64 trait) { (void)self; (void)c; return fm_with_traits(font, 0, trait); }
static id fm_convert_size(id self, SEL c, id font, double size) {
    (void)self; (void)c;
    if (!font) return sys_font(size);
    id n = MSG0(id, font, "fontName");
    id f = n ? MSG(id, (id)kls("NSFont"), "fontWithName:size:", n, size) : 0;
    if (!f) f = sys_font(size);
    u32 t = CTFontGetSymbolicTraits(font) & 3;
    return t ? fm_with_traits(f, t, 0) : f;
}
static id fm_convert_family(id self, SEL c, id font, id family) {
    (void)self; (void)c;
    if (!family) return font;
    double s = fsize(font);
    id f = font ? CTFontCreateCopyWithFamily(font, s, 0, family) : 0;
    if (f) return objc_autorelease(f);
    f = MSG(id, (id)kls("NSFont"), "fontWithName:size:", family, s);
    return f ? f : font;
}
static id fm_convert_face(id self, SEL c, id font, id face) {
    (void)self; (void)c;
    id f = face ? MSG(id, (id)kls("NSFont"), "fontWithName:size:", face, fsize(font)) : 0;
    return f ? f : font;
}
static id fm_font_family(id self, SEL c, id fam, u64 traits, i64 weight, double size) {
    (void)c;
    id f = fam ? MSG(id, (id)kls("NSFont"), "fontWithName:size:", fam, size) : 0;
    if (!f) f = sys_font(size);
    if (weight >= 8) traits |= 2;
    return traits & 3 ? fm_convert_trait(self, 0, f, traits & 3) : f;
}
static id fm_selected(id self, SEL c) { (void)c; return pget(self, "selected"); }
static BOOL fm_multiple(id self, SEL c) { (void)c; return pgi(self, "multiple", 0) != 0; }
static void fm_set_selected(id self, SEL c, id font, BOOL multiple) {
    (void)c;
    pset(self, "selected", font); psi(self, "multiple", multiple);
    if (g_fp) MSG(void, g_fp, "setPanelFont:isMultiple:", font, multiple);
}
static SEL fm_action(id self, SEL c) { (void)c; id v = pget(self, "action"); return v ? sl(cx_cstr(v)) : sl("changeFont:"); }
static BOOL fm_send_action(id self, SEL c) {
    (void)c;
    if (!pgi(self, "enabled", 1)) return NO;
    id t = weak_get(self, &k_target);
    return MSG(BOOL, NSApp, "sendAction:to:from:", fm_action(self, 0), t, self);
}
/* o que o painel ou o menu pediram: convertFont: aplica à fonte de cada trecho selecionado */
static id fm_convert(id self, SEL c, id font) {
    (void)c;
    i64 act = pgi(self, "pending", 0), arg = pgi(self, "pendingArg", 0);
    switch (act) {
    case 1: { id pf = g_fp ? MSG(id, g_fp, "panelConvertFont:", font) : 0; return pf ? pf : font; }
    case 2: return fm_convert_trait(self, 0, font, (u64)arg);
    case 7: return fm_convert_not_trait(self, 0, font, (u64)arg);
    case 3: return fm_convert_size(self, 0, font, fsize(font) + 1);
    case 4: return fm_convert_size(self, 0, font, fsize(font) > 2 ? fsize(font) - 1 : fsize(font));
    case 5: return fm_convert_trait(self, 0, font, 2);
    case 6: return fm_convert_not_trait(self, 0, font, 2);
    }
    return font;
}
static void fm_modify(id self, SEL c, id sender) {
    (void)c;
    i64 tag = sender && responds(sender, "tag") ? MSG0(i64, sender, "tag") : 0;
    psi(self, "pending", tag); psi(self, "pendingArg", 0);
    fm_send_action(self, 0);
}
static void fm_add_trait(id self, SEL c, id sender) {
    (void)c;
    i64 tag = sender && responds(sender, "tag") ? MSG0(i64, sender, "tag") : 0;
    psi(self, "pending", 2); psi(self, "pendingArg", tag);
    fm_send_action(self, 0);
}
static void fm_remove_trait(id self, SEL c, id sender) {
    (void)c;
    i64 tag = sender && responds(sender, "tag") ? MSG0(i64, sender, "tag") : 0;
    psi(self, "pending", 7); psi(self, "pendingArg", tag);
    fm_send_action(self, 0);
}
static void fm_modify_via_panel(id self, SEL c, id sender) { (void)c; (void)sender; psi(self, "pending", 1); fm_send_action(self, 0); }
static id fm_panel(id self, SEL c, BOOL create) { (void)self; (void)c; return create ? MSG0(id, (id)&NSFontPanel_c, "sharedFontPanel") : g_fp; }
static void fm_order_front(id self, SEL c, id sender) { (void)c; (void)sender; id p = fm_panel(self, 0, YES); if (p) MSG(void, p, "orderFront:", self); }
static id fm_menu(id self, SEL c, BOOL create) {
    (void)c;
    id m = pget(self, "fontMenu");
    if (m || !create) return m;
    m = MSG(id, MSG0(id, (id)kls("NSMenu"), "alloc"), "initWithTitle:", cx_str("Fonte"));
    struct { const char *t, *k; const char *sel; i64 tag; } it[] = {
        {"Mostrar Fontes", "t", "orderFrontFontPanel:", 0}, {"Negrito", "b", "addFontTrait:", 2}, {"Itálico", "i", "addFontTrait:", 1},
        {"Maior", "+", "modifyFont:", 3}, {"Menor", "-", "modifyFont:", 4}};
    for (int i = 0; i < 5; i++) {
        id mi = MSG(id, m, "addItemWithTitle:action:keyEquivalent:", cx_str(it[i].t), sl(it[i].sel), cx_str(it[i].k));
        MSG(void, mi, "setTarget:", self); MSG(void, mi, "setTag:", it[i].tag);
    }
    pset(self, "fontMenu", m);
    objc_release(m);
    return m;
}
static void fm_set_menu(id self, SEL c, id m) { (void)c; pset(self, "fontMenu", m); }
static id fm_localized(id self, SEL c, id family, id face) { (void)self; (void)c; return face ? MSG(id, (id)kls("NSString"), "stringWithFormat:", cx_str("%@ %@"), family, face) : family; }
static BOOL fm_enabled(id self, SEL c) { (void)c; return pgi(self, "enabled", 1) != 0; }
static void fm_set_enabled(id self, SEL c, BOOL b) { (void)c; psi(self, "enabled", b); }
static i64 fm_current_action(id self, SEL c) { (void)c; return pgi(self, "pending", 0); }
static id fm_shared(id k, SEL c) { (void)c; if (!g_fm) { g_fm = cx_alloc_instance((cx_class_t *)k); psi(g_fm, "enabled", 1); } return g_fm; }
static void fm_set_action(id self, SEL c, SEL a) { (void)c; pset(self, "action", a ? cx_str(sel_getName(a)) : 0); }

/* lista simples do painel (família, estilo, tamanho) */
static char k_list_owner;
static double list_row_h(void) { return 18; }
static void fl_draw(id self, SEL c, R dirty) {
    (void)c; (void)dirty;
    R b = msg_rect(self, "bounds");
    MSG0(void, cx_color(1, 1, 1, 1), "setFill"); NSRectFill(b);
    MSG0(void, cx_color(0.75, 0.75, 0.77, 1), "set"); NSFrameRect(b);
    id items = pget(self, "items");
    i64 sel = pgi(self, "sel", -1), top = pgi(self, "top", 0);
    extern id NSFontAttributeName, NSForegroundColorAttributeName;
    for (u64 i = (u64)top, n = items ? MSG0(u64, items, "count") : 0; i < n; i++) {
        double y = (double)(i - (u64)top) * list_row_h();
        if (y + list_row_h() > b.h) break;
        R row = {1, y + 1, b.w - 2, list_row_h()};
        int on = (i64)i == sel;
        if (on) { MSG0(void, cx_color(0.13, 0.45, 0.95, 1), "setFill"); NSRectFill(row); }
        id d = MSG0(id, (id)kls("NSMutableDictionary"), "dictionary");
        MSG(void, d, "setObject:forKey:", sys_font(12), NSFontAttributeName);
        MSG(void, d, "setObject:forKey:", on ? cx_color(1, 1, 1, 1) : cx_color(0, 0, 0, 1), NSForegroundColorAttributeName);
        ((void (*)(id, SEL, R, id))objc_msgSend)(MSG(id, items, "objectAtIndex:", i), sl("drawInRect:withAttributes:"), (R){row.x + 4, row.y + 2, row.w - 8, row.h - 2}, d);
    }
}
static BOOL fl_flipped(id self, SEL c) { (void)self; (void)c; return YES; }
static void fl_mouse(id self, SEL c, id ev) {
    (void)c;
    P w = ((P (*)(id, SEL))objc_msgSend)(ev, sl("locationInWindow"));
    P p = ((P (*)(id, SEL, P, id))objc_msgSend)(self, sl("convertPoint:fromView:"), w, (id)0);
    id items = pget(self, "items");
    i64 i = pgi(self, "top", 0) + (i64)(p.y / list_row_h());
    if (!items || i < 0 || (u64)i >= MSG0(u64, items, "count")) return;
    psi(self, "sel", i);
    redraw(self);
    id owner = weak_get(self, &k_list_owner);
    if (owner) MSG(void, owner, "_cxListChanged:", self);
}
static void fl_scroll(id self, SEL c, id ev) {
    (void)c;
    double dy = MSG0(double, ev, "scrollingDeltaY");
    id items = pget(self, "items");
    i64 n = items ? (i64)MSG0(u64, items, "count") : 0, top = pgi(self, "top", 0);
    R b = msg_rect(self, "bounds");
    i64 vis = (i64)(b.h / list_row_h());
    top += dy > 0 ? -1 : dy < 0 ? 1 : 0;
    if (top > n - vis) top = n - vis;
    if (top < 0) top = 0;
    psi(self, "top", top);
    redraw(self);
}
static id fl_make(id owner, R f) {
    id v = ((id (*)(id, SEL, R))objc_msgSend)(MSG0(id, (id)&_MOSFontList_c, "alloc"), sl("initWithFrame:"), f);
    weak_set(v, &k_list_owner, owner);
    return objc_autorelease(v);
}
static void fl_set(id list, id items, id select) {
    pset(list, "items", items);
    u64 i = select && items ? MSG(u64, items, "indexOfObject:", select) : (u64)-1;
    psi(list, "sel", i < MSG0(u64, items, "count") ? (i64)i : (items && MSG0(u64, items, "count") ? 0 : -1));
    psi(list, "top", 0);
    redraw(list);
}
static id fl_selected(id list) { id items = pget(list, "items"); i64 s = pgi(list, "sel", -1); return items && s >= 0 && (u64)s < MSG0(u64, items, "count") ? MSG(id, items, "objectAtIndex:", (u64)s) : 0; }

static const double k_sizes[] = {9, 10, 11, 12, 13, 14, 18, 24, 36, 48, 64, 72, 96, 144, 288};
static id fp_size_items(void) {
    id a = arr();
    for (u32 i = 0; i < sizeof k_sizes / sizeof k_sizes[0]; i++) { char b[16]; snprintf(b, sizeof b, "%g", k_sizes[i]); MSG(void, a, "addObject:", cx_str(b)); }
    return a;
}
static id fp_current_font(id self) {
    id fam = fl_selected(pget(self, "famList")), face = fl_selected(pget(self, "faceList")), sz = fl_selected(pget(self, "sizeList"));
    double s = sz ? MSG0(double, sz, "doubleValue") : 13;
    id fmm = MSG0(id, (id)kls("NSFontManager"), "sharedFontManager");
    id f = fm_font_family(fmm, 0, fam, 0, 5, s);
    if (face) {
        const char *fc = cx_cstr(face);
        if (strstr(fc, "Bold") || strstr(fc, "SemiBold")) f = fm_convert_trait(fmm, 0, f, 2);
        if (strstr(fc, "Italic")) f = fm_convert_trait(fmm, 0, f, 1);
    }
    return f;
}
static void fp_update_preview(id self) {
    id pv = pget(self, "preview");
    if (!pv) return;
    id f = fp_current_font(self);
    MSG(void, pv, "setFont:", f);
    id fam = fl_selected(pget(self, "famList"));
    MSG(void, pv, "setStringValue:", fam ? fam : cx_str(""));
}
static void fp_fill_faces(id self, id select) {
    id fam = fl_selected(pget(self, "famList"));
    id mem = fm_members(0, 0, fam), faces = arr();
    for (u64 i = 0, n = mem ? MSG0(u64, mem, "count") : 0; i < n; i++) { id m = MSG(id, mem, "objectAtIndex:", i); id nm = MSG(id, m, "objectAtIndex:", (u64)1); MSG(void, faces, "addObject:", nm); }
    fl_set(pget(self, "faceList"), faces, select);
}
static void fp_list_changed(id self, SEL c, id list) {
    (void)c;
    if (list == pget(self, "famList")) fp_fill_faces(self, fl_selected(pget(self, "faceList")));
    fp_update_preview(self);
    if (!pgi(self, "enabled", 1)) return;
    id fmm = MSG0(id, (id)kls("NSFontManager"), "sharedFontManager");
    psi(fmm, "pending", 1);
    fm_send_action(fmm, 0);
}
static id fp_shared(id k, SEL c) {
    (void)c;
    if (g_fp) return g_fp;
    R fr = {0, 0, 420, 300};
    id p = ((id (*)(id, SEL, R, u64, u64, BOOL))objc_msgSend)(cx_alloc_instance((cx_class_t *)k), sl("initWithContentRect:styleMask:backing:defer:"), fr, (u64)(1 | 2 | 8 | 16), (u64)2, (BOOL)NO);
    if (!p) return 0;
    g_fp = p;
    psi(p, "enabled", 1);
    MSG(void, p, "setTitle:", cx_str("Fontes"));
    MSG(void, p, "setReleasedWhenClosed:", (BOOL)NO);
    if (responds(p, "setFloatingPanel:")) MSG(void, p, "setFloatingPanel:", (BOOL)YES);
    id cv = MSG0(id, p, "contentView");
    id prev = ((id (*)(id, SEL, R))objc_msgSend)(MSG0(id, (id)kls("NSTextField"), "alloc"), sl("initWithFrame:"), (R){10, 236, 400, 54});
    MSG(void, prev, "setEditable:", (BOOL)NO); MSG(void, prev, "setAlignment:", (i64)2);
    MSG(void, cv, "addSubview:", prev); pset(p, "preview", prev); objc_release(prev);
    id fl = fl_make(p, (R){10, 10, 180, 218}), fc = fl_make(p, (R){198, 10, 130, 218}), fs = fl_make(p, (R){336, 10, 74, 218});
    MSG(void, cv, "addSubview:", fl); MSG(void, cv, "addSubview:", fc); MSG(void, cv, "addSubview:", fs);
    pset(p, "famList", fl); pset(p, "faceList", fc); pset(p, "sizeList", fs);
    fl_set(fl, fm_families(0, 0), cx_str("Inter"));
    fp_fill_faces(p, 0);
    fl_set(fs, fp_size_items(), cx_str("13"));
    fp_update_preview(p);
    return p;
}
static BOOL fp_exists(id k, SEL c) { (void)k; (void)c; return g_fp != 0; }
static void fp_set_panel_font(id self, SEL c, id font, BOOL multiple) {
    (void)c;
    psi(self, "multiple", multiple);
    if (!font) return;
    id fam = responds(font, "familyName") ? MSG0(id, font, "familyName") : 0;
    if (fam && !strcmp(cx_cstr(fam), ".AppleSystemUIFont")) fam = cx_str("Inter");
    fl_set(pget(self, "famList"), pget(pget(self, "famList"), "items"), fam);
    fp_fill_faces(self, CTFontGetSymbolicTraits(font) & 2 ? cx_str("Bold") : cx_str("Regular"));
    char b[16];
    snprintf(b, sizeof b, "%g", fsize(font));
    id sizes = fp_size_items();
    if (!MSG(BOOL, sizes, "containsObject:", cx_str(b))) MSG(void, sizes, "addObject:", cx_str(b));
    MSG(void, sizes, "sortUsingSelector:", sl("localizedStandardCompare:"));
    fl_set(pget(self, "sizeList"), sizes, cx_str(b));
    fp_update_preview(self);
}
/* o que o painel mostra aplicado à fonte do trecho: família, estilo e tamanho escolhidos */
static id fp_convert(id self, SEL c, id font) {
    (void)c;
    id want = fp_current_font(self);
    if (!font) return want;
    return want ? want : font;
}
static BOOL fp_enabled(id self, SEL c) { (void)c; return pgi(self, "enabled", 1) != 0; }
static void fp_set_enabled(id self, SEL c, BOOL b) { (void)c; psi(self, "enabled", b); }
static void fp_reload(id self, SEL c) { (void)c; fl_set(pget(self, "famList"), fm_families(0, 0), fl_selected(pget(self, "famList"))); fp_fill_faces(self, 0); }
static id fp_accessory(id self, SEL c) { (void)c; return pget(self, "accessory"); }
static void fp_set_accessory(id self, SEL c, id v) {
    (void)c;
    id old = pget(self, "accessory");
    if (old) MSG0(void, old, "removeFromSuperview");
    pset(self, "accessory", v);
    if (!v) return;
    R vf = msg_rect(v, "frame"), wf = msg_rect(self, "frame");
    ((void (*)(id, SEL, R, BOOL))objc_msgSend)(self, sl("setFrame:display:"), (R){wf.x, wf.y - vf.h, wf.w, wf.h + vf.h}, (BOOL)YES);
    id cv = MSG0(id, self, "contentView");
    id subs = MSG0(id, cv, "subviews");
    for (u64 i = 0, n = MSG0(u64, subs, "count"); i < n; i++) {
        id s = MSG(id, subs, "objectAtIndex:", i);
        R f = msg_rect(s, "frame");
        f.y += vf.h;
        m_set_frame(s, f);
    }
    m_set_frame(v, (R){10, 0, vf.w, vf.h});
    MSG(void, cv, "addSubview:", v);
}

/* ================================================================ NSFontCollection */
static id g_named;   /* nome → coleção */
extern id NSFontCollectionAllFonts, NSFontCollectionUser, NSFontCollectionFavorites, NSFontCollectionRecentlyUsed;
static id fd_for_family(id fam) {
    extern id NSFontFamilyAttribute;
    id d = MSG(id, (id)kls("NSDictionary"), "dictionaryWithObject:forKey:", fam, NSFontFamilyAttribute);
    cx_class_t *fdk = kls("NSFontDescriptor");
    return fdk ? MSG(id, (id)fdk, "fontDescriptorWithFontAttributes:", d) : d;
}
static id fd_family(id d) {
    extern id NSFontFamilyAttribute;
    if (responds(d, "objectForKey:") && !responds(d, "fontAttributes")) return MSG(id, d, "objectForKey:", NSFontFamilyAttribute);
    return responds(d, "objectForKey:") ? MSG(id, d, "objectForKey:", NSFontFamilyAttribute) : 0;
}
static id fc_new(cx_class_t *k, id query, id excl) {
    id c = objc_autorelease(cx_alloc_instance(k));
    pset(c, "query", query ? MSG0(id, query, "mutableCopy") : arr());
    if (query) objc_release(pget(c, "query"));
    pset(c, "excl", excl ? MSG0(id, excl, "mutableCopy") : arr());
    if (excl) objc_release(pget(c, "excl"));
    return c;
}
static id all_descriptors(void) {
    id a = arr(), fams = fm_families(0, 0);
    for (u64 i = 0, n = MSG0(u64, fams, "count"); i < n; i++) { id f = MSG(id, fams, "objectAtIndex:", i); MSG(void, a, "addObject:", fd_for_family(f)); }
    return a;
}
static void named_init(void) {
    if (g_named) return;
    g_named = objc_retain(MSG0(id, (id)kls("NSMutableDictionary"), "dictionary"));
    MSG(void, g_named, "setObject:forKey:", fc_new(&NSFontCollection_c, all_descriptors(), 0), NSFontCollectionAllFonts);
    MSG(void, g_named, "setObject:forKey:", fc_new(&NSFontCollection_c, 0, 0), NSFontCollectionUser);
    MSG(void, g_named, "setObject:forKey:", fc_new(&NSFontCollection_c, 0, 0), NSFontCollectionFavorites);
    MSG(void, g_named, "setObject:forKey:", fc_new(&NSFontCollection_c, 0, 0), NSFontCollectionRecentlyUsed);
}
static id fc_all(id k, SEL c) { (void)c; return fc_new((cx_class_t *)k, all_descriptors(), 0); }
static id fc_with_desc(id k, SEL c, id d) { (void)c; return fc_new((cx_class_t *)k, d, 0); }
static id fc_with_locale(id k, SEL c, id loc) { (void)loc; return fc_all(k, c); }
static id fc_named(id k, SEL c, id name) { (void)k; (void)c; named_init(); return name ? MSG(id, g_named, "objectForKey:", name) : 0; }
static id fc_named_vis(id k, SEL c, id name, u64 vis) { (void)vis; return fc_named(k, c, name); }
static id fc_names(id k, SEL c) { (void)k; (void)c; named_init(); return MSG0(id, g_named, "allKeys"); }
static void fc_note(id action, id name, id old) {
    extern id NSFontCollectionDidChangeNotification, NSFontCollectionActionKey, NSFontCollectionNameKey, NSFontCollectionOldNameKey;
    id info = MSG0(id, (id)kls("NSMutableDictionary"), "dictionary");
    MSG(void, info, "setObject:forKey:", action, NSFontCollectionActionKey);
    if (name) MSG(void, info, "setObject:forKey:", name, NSFontCollectionNameKey);
    if (old) MSG(void, info, "setObject:forKey:", old, NSFontCollectionOldNameKey);
    MSG(void, MSG0(id, (id)kls("NSNotificationCenter"), "defaultCenter"), "postNotificationName:object:userInfo:", NSFontCollectionDidChangeNotification, (id)0, info);
}
static BOOL fc_show(id k, SEL c, id coll, id name, u64 vis, id *err) {
    (void)k; (void)c; (void)vis;
    extern id NSFontCollectionWasShown;
    if (err) *err = 0;
    if (!coll || !name) return NO;
    named_init();
    MSG(void, g_named, "setObject:forKey:", MSG0(id, coll, "copy"), name);
    objc_release(MSG(id, g_named, "objectForKey:", name));
    fc_note(NSFontCollectionWasShown, name, 0);
    return YES;
}
static BOOL fc_hide(id k, SEL c, id name, u64 vis, id *err) {
    (void)k; (void)c; (void)vis;
    extern id NSFontCollectionWasHidden;
    if (err) *err = 0;
    named_init();
    if (!name || !MSG(id, g_named, "objectForKey:", name)) return NO;
    MSG(void, g_named, "removeObjectForKey:", name);
    fc_note(NSFontCollectionWasHidden, name, 0);
    return YES;
}
static BOOL fc_rename(id k, SEL c, id name, u64 vis, id to, id *err) {
    (void)k; (void)c; (void)vis;
    extern id NSFontCollectionWasRenamed;
    if (err) *err = 0;
    named_init();
    id coll = name ? MSG(id, g_named, "objectForKey:", name) : 0;
    if (!coll || !to) return NO;
    objc_retain(coll);
    MSG(void, g_named, "removeObjectForKey:", name);
    MSG(void, g_named, "setObject:forKey:", coll, to);
    objc_release(coll);
    fc_note(NSFontCollectionWasRenamed, to, name);
    return YES;
}
static id fc_query(id self, SEL c) { (void)c; return pget(self, "query"); }
static id fc_excl(id self, SEL c) { (void)c; return pget(self, "excl"); }
static int desc_matches(id d, id fam) { id f = fd_family(d); return f && fam && MSG(BOOL, f, "isEqual:", fam); }
static id fc_matching_family(id self, SEL c, id family) {
    (void)c;
    id out = arr(), q = pget(self, "query"), ex = pget(self, "excl"), fams = fm_families(0, 0);
    for (u64 i = 0, n = MSG0(u64, fams, "count"); i < n; i++) {
        id fam = MSG(id, fams, "objectAtIndex:", i);
        if (family && !MSG(BOOL, fam, "isEqual:", family)) continue;
        int in = 0, out_ = 0;
        for (u64 j = 0, m = MSG0(u64, q, "count"); j < m && !in; j++) {
            id d = MSG(id, q, "objectAtIndex:", j);
            in = desc_matches(d, fam) || !fd_family(d);   /* descritor sem família = todas */
        }
        for (u64 j = 0, m = MSG0(u64, ex, "count"); j < m && !out_; j++) out_ = desc_matches(MSG(id, ex, "objectAtIndex:", j), fam);
        if (in && !out_) MSG(void, out, "addObject:", fd_for_family(fam));
    }
    return out;
}
static id fc_matching(id self, SEL c) { return fc_matching_family(self, c, 0); }
static id fc_matching_opts(id self, SEL c, id opts) { (void)opts; return fc_matching_family(self, c, 0); }
static id fc_matching_family_opts(id self, SEL c, id fam, id opts) { (void)opts; return fc_matching_family(self, c, fam); }
static id fc_copy(id self, SEL c, void *z) { (void)c; (void)z; return objc_retain(fc_new(&NSFontCollection_c, pget(self, "query"), pget(self, "excl"))); }
static id fc_mcopy(id self, SEL c, void *z) { (void)c; (void)z; return objc_retain(fc_new(&NSMutableFontCollection_c, pget(self, "query"), pget(self, "excl"))); }
static void mfc_set_query(id self, SEL c, id a) { (void)c; pset(self, "query", a ? MSG0(id, a, "mutableCopy") : arr()); if (a) objc_release(pget(self, "query")); }
static void mfc_set_excl(id self, SEL c, id a) { (void)c; pset(self, "excl", a ? MSG0(id, a, "mutableCopy") : arr()); if (a) objc_release(pget(self, "excl")); }
static void mfc_add_query(id self, SEL c, id d) { (void)c; if (d) MSG(void, pget(self, "query"), "addObjectsFromArray:", d); }
static void mfc_remove_query(id self, SEL c, id d) { (void)c; if (d) MSG(void, pget(self, "query"), "removeObjectsInArray:", d); }

/* ================================================================ NSTextBlock */
/* layer: Padding -1, Border 0, Margin 1; edge: MinX 0, MinY 1, MaxX 2, MaxY 3; tipo: Absolute 0, Percentage 1 */
static char k_tb;
typedef struct { double v[3][4]; int t[3][4]; double dim[6]; int dimt[6]; } tb_t;   /* dim: largura, mín., máx., altura, mín., máx. */
static tb_t *tb_of(id self) {
    id d = objc_getAssociatedObject(self, &k_tb);
    if (!d) { d = MSG(id, (id)kls("NSMutableData"), "dataWithLength:", (u64)sizeof(tb_t)); objc_setAssociatedObject(self, &k_tb, d, 1); }
    return MSG0(tb_t *, d, "mutableBytes");
}
static int layer_ix(i64 l) { return l < -1 ? 0 : l > 1 ? 2 : (int)l + 1; }
static double tb_resolve(double v, int t, double basis) { return t == 1 ? v * basis / 100.0 : v; }
static void tb_set_width_edge(id self, SEL c, double v, u64 t, i64 layer, u64 edge) { (void)c; tb_t *b = tb_of(self); if (edge < 4) { b->v[layer_ix(layer)][edge] = v; b->t[layer_ix(layer)][edge] = (int)t; } }
static void tb_set_width(id self, SEL c, double v, u64 t, i64 layer) { for (u64 e = 0; e < 4; e++) tb_set_width_edge(self, c, v, t, layer, e); }
static double tb_width(id self, SEL c, i64 layer, u64 edge) { (void)c; return edge < 4 ? tb_of(self)->v[layer_ix(layer)][edge] : 0; }
static u64 tb_width_type(id self, SEL c, i64 layer, u64 edge) { (void)c; return edge < 4 ? (u64)tb_of(self)->t[layer_ix(layer)][edge] : 0; }
static void tb_set_value(id self, SEL c, double v, u64 t, u64 dim) { (void)c; if (dim < 6) { tb_of(self)->dim[dim] = v; tb_of(self)->dimt[dim] = (int)t; } }
static double tb_value(id self, SEL c, u64 dim) { (void)c; return dim < 6 ? tb_of(self)->dim[dim] : 0; }
static u64 tb_value_type(id self, SEL c, u64 dim) { (void)c; return dim < 6 ? (u64)tb_of(self)->dimt[dim] : 0; }
static void tb_set_content_width(id self, SEL c, double v, u64 t) { tb_set_value(self, c, v, t, 0); }
static double tb_content_width(id self, SEL c) { return tb_value(self, c, 0); }
static u64 tb_content_width_type(id self, SEL c) { return tb_value_type(self, c, 0); }
OBJP(tbBg, "bg") INTP(tbVAlign, "valign", 0)
static void tb_set_border_color_edge(id self, SEL c, id col, u64 edge) { (void)c; char k[16]; snprintf(k, sizeof k, "border%llu", (unsigned long long)edge); pset(self, k, col); }
static void tb_set_border_color(id self, SEL c, id col) { for (u64 e = 0; e < 4; e++) tb_set_border_color_edge(self, c, col, e); }
static id tb_border_color(id self, SEL c, u64 edge) { (void)c; char k[16]; snprintf(k, sizeof k, "border%llu", (unsigned long long)edge); return pget(self, k); }
/* soma das três camadas num lado (resolvendo porcentagens pela largura do contêiner) */
static double tb_edge_sum(tb_t *b, int edge, double basis) { double s = 0; for (int l = 0; l < 3; l++) s += tb_resolve(b->v[l][edge], b->t[l][edge], basis); return s; }
static R tb_rect_for_layout(id self, SEL c, P start, R rect, id tc, struct { u64 loc, len; } range) {
    (void)c; (void)tc; (void)range;
    tb_t *b = tb_of(self);
    double basis = rect.w;
    double l = tb_edge_sum(b, 0, basis), t = tb_edge_sum(b, 1, basis), r = tb_edge_sum(b, 2, basis);
    double w = rect.x + rect.w - start.x - l - r;
    if (b->dim[0] > 0) w = tb_resolve(b->dim[0], b->dimt[0], basis);
    if (b->dim[1] > 0 && w < tb_resolve(b->dim[1], b->dimt[1], basis)) w = tb_resolve(b->dim[1], b->dimt[1], basis);
    if (b->dim[2] > 0 && w > tb_resolve(b->dim[2], b->dimt[2], basis)) w = tb_resolve(b->dim[2], b->dimt[2], basis);
    if (w < 0) w = 0;
    return (R){start.x + l, start.y + t, w, rect.y + rect.h - start.y - t};
}
static R tb_bounds_for_content(id self, SEL c, R content, R rect, id tc, struct { u64 loc, len; } range) {
    (void)c; (void)tc; (void)range;
    tb_t *b = tb_of(self);
    double basis = rect.w;
    double l = tb_edge_sum(b, 0, basis), t = tb_edge_sum(b, 1, basis), r = tb_edge_sum(b, 2, basis), bt = tb_edge_sum(b, 3, basis);
    double h = content.h;
    if (b->dim[3] > 0) h = tb_resolve(b->dim[3], b->dimt[3], basis);
    if (b->dim[4] > 0 && h < tb_resolve(b->dim[4], b->dimt[4], basis)) h = tb_resolve(b->dim[4], b->dimt[4], basis);
    if (b->dim[5] > 0 && h > tb_resolve(b->dim[5], b->dimt[5], basis)) h = tb_resolve(b->dim[5], b->dimt[5], basis);
    return (R){content.x - l, content.y - t, content.w + l + r, h + t + bt};
}
static void tb_draw_bg(id self, SEL c, R frame, id view, struct { u64 loc, len; } range, id lm) {
    (void)c; (void)view; (void)range; (void)lm;
    tb_t *b = tb_of(self);
    double basis = frame.w;
    /* tira a margem; a borda e o preenchimento ficam dentro */
    R box = {frame.x + tb_resolve(b->v[2][0], b->t[2][0], basis), frame.y + tb_resolve(b->v[2][1], b->t[2][1], basis), 0, 0};
    box.w = frame.w - tb_resolve(b->v[2][0], b->t[2][0], basis) - tb_resolve(b->v[2][2], b->t[2][2], basis);
    box.h = frame.h - tb_resolve(b->v[2][1], b->t[2][1], basis) - tb_resolve(b->v[2][3], b->t[2][3], basis);
    id bg = pget(self, "bg");
    if (bg) { MSG0(void, bg, "setFill"); NSRectFill(box); }
    double bw[4];
    for (int e = 0; e < 4; e++) bw[e] = tb_resolve(b->v[1][e], b->t[1][e], basis);
    R sides[4] = {{box.x, box.y, bw[0], box.h}, {box.x, box.y, box.w, bw[1]}, {box.x + box.w - bw[2], box.y, bw[2], box.h}, {box.x, box.y + box.h - bw[3], box.w, bw[3]}};
    for (int e = 0; e < 4; e++) {
        if (bw[e] <= 0) continue;
        id col = tb_border_color(self, 0, (u64)e);
        MSG0(void, col ? col : cx_color(0, 0, 0, 1), "setFill");
        NSRectFill(sides[e]);
    }
}
static id tb_copy(id self, SEL c, void *z) {
    (void)c; (void)z;
    id n = cx_alloc_instance(object_getClass(self));
    *tb_of(n) = *tb_of(self);
    id d = props(self, 0);
    if (d) { id m = MSG0(id, d, "mutableCopy"); objc_setAssociatedObject(n, &k_props, m, 1); objc_release(m); }
    return n;
}

/* ================================================================ NSPDFImageRep / NSEPSImageRep */
void *CGDataProviderCreateWithCFData(id data);
void CGDataProviderRelease(void *p);
void *CGPDFDocumentCreateWithProvider(void *provider);
void CGPDFDocumentRelease(void *d);
size_t CGPDFDocumentGetNumberOfPages(void *d);
void *CGPDFDocumentGetPage(void *d, size_t n);
R CGPDFPageGetBoxRect(void *p, int box);
void CGContextDrawPDFPage(void *c, void *page);
void *CGBitmapContextCreate(void *data, size_t w, size_t h, size_t bpc, size_t bpr, void *space, u32 info);
void *CGBitmapContextCreateImage(void *c);
void *CGColorSpaceCreateDeviceRGB(void);
void CGColorSpaceRelease(void *s);
void CGContextRelease(void *c);
void CGImageRelease(void *i);
void CGContextScaleCTM(void *c, double x, double y);
void CGContextTranslateCTM(void *c, double x, double y);
static char k_pdfdoc;
static void pdf_free_doc(void *d) { if (d) CGPDFDocumentRelease(d); }
static void *pdf_doc(id self) { id v = objc_getAssociatedObject(self, &k_pdfdoc); return v ? MSG0(void *, v, "pointerValue") : 0; }
static void *pdf_open(id data) {
    if (!data || MSG0(u64, data, "length") < 5) return 0;
    void *pr = CGDataProviderCreateWithCFData(data);
    if (!pr) return 0;
    void *doc = CGPDFDocumentCreateWithProvider(pr);
    CGDataProviderRelease(pr);
    return doc;
}
/* página → NSImage rasterizada (escala 2 para telas de alta densidade) */
static id pdf_page_image(void *doc, size_t page, R *box_out) {
    void *pg = doc ? CGPDFDocumentGetPage(doc, page) : 0;
    if (!pg) return 0;
    R box = CGPDFPageGetBoxRect(pg, 1);   /* kCGPDFCropBox */
    if (box.w <= 0 || box.h <= 0) box = CGPDFPageGetBoxRect(pg, 0);
    if (box_out) *box_out = box;
    double sc = 2;
    size_t w = (size_t)(box.w * sc + 0.5), h = (size_t)(box.h * sc + 0.5);
    if (!w || !h || w > 8192 || h > 8192) return 0;
    void *cs = CGColorSpaceCreateDeviceRGB();
    void *ctx = CGBitmapContextCreate(0, w, h, 8, 0, cs, 2 | 0x2000);   /* premultiplicado primeiro, 32 bits little-endian */
    CGColorSpaceRelease(cs);
    if (!ctx) return 0;
    CGContextScaleCTM(ctx, sc, sc);
    CGContextTranslateCTM(ctx, -box.x, -box.y);
    CGContextDrawPDFPage(ctx, pg);
    void *img = CGBitmapContextCreateImage(ctx);
    CGContextRelease(ctx);
    if (!img) return 0;
    id ni = ((id (*)(id, SEL, void *, S))objc_msgSend)(MSG0(id, (id)kls("NSImage"), "alloc"), sl("initWithCGImage:size:"), img, (S){box.w, box.h});
    CGImageRelease(img);
    return ni ? objc_autorelease(ni) : 0;
}
static id pdfr_init_data(id self, SEL c, id data) {
    (void)c;
    void *doc = pdf_open(data);
    if (!doc || !CGPDFDocumentGetNumberOfPages(doc)) { pdf_free_doc(doc); objc_release(self); return 0; }
    pset(self, "data", data);
    id pv = MSG(id, (id)kls("NSValue"), "valueWithPointer:", doc);
    objc_setAssociatedObject(self, &k_pdfdoc, pv, 1);
    psi(self, "page", 0);
    return self;
}
static id pdfr_with_data(id k, SEL c, id data) { id r = pdfr_init_data(cx_alloc_instance((cx_class_t *)k), c, data); return r ? objc_autorelease(r) : 0; }
static id pdfr_data(id self, SEL c) { (void)c; return pget(self, "data"); }
static i64 pdfr_count(id self, SEL c) { (void)c; void *d = pdf_doc(self); return d ? (i64)CGPDFDocumentGetNumberOfPages(d) : 0; }
static i64 pdfr_page(id self, SEL c) { (void)c; return pgi(self, "page", 0); }
static void pdfr_set_page(id self, SEL c, i64 p) { (void)c; if (p >= 0 && p < pdfr_count(self, 0)) { psi(self, "page", p); pset(self, "img", 0); } }
static R pdfr_bounds(id self, SEL c) {
    (void)c;
    void *d = pdf_doc(self);
    void *pg = d ? CGPDFDocumentGetPage(d, (size_t)pgi(self, "page", 0) + 1) : 0;
    if (!pg) return (R){0, 0, 0, 0};
    R b = CGPDFPageGetBoxRect(pg, 1);
    return b.w > 0 ? b : CGPDFPageGetBoxRect(pg, 0);
}
static S pdfr_size(id self, SEL c) { R b = pdfr_bounds(self, c); return (S){b.w, b.h}; }
static i64 pdfr_px_w(id self, SEL c) { return (i64)pdfr_size(self, c).w; }
static i64 pdfr_px_h(id self, SEL c) { return (i64)pdfr_size(self, c).h; }
static id pdfr_image(id self) {
    id img = pget(self, "img");
    if (!img) { img = pdf_page_image(pdf_doc(self), (size_t)pgi(self, "page", 0) + 1, 0); pset(self, "img", img); }
    return img;
}
static BOOL rep_draw_in(id self, SEL c, R r, id img) {
    (void)self; (void)c;
    if (!img) return NO;
    ((void (*)(id, SEL, R, R, u64, double))objc_msgSend)(img, sl("drawInRect:fromRect:operation:fraction:"), r, (R){0, 0, 0, 0}, (u64)2, 1.0);
    return YES;
}
static BOOL pdfr_draw_in(id self, SEL c, R r) { return rep_draw_in(self, c, r, pdfr_image(self)); }
static BOOL pdfr_draw(id self, SEL c) { S z = pdfr_size(self, 0); return pdfr_draw_in(self, c, (R){0, 0, z.w, z.h}); }
static BOOL pdfr_draw_at(id self, SEL c, P p) { S z = pdfr_size(self, 0); return pdfr_draw_in(self, c, (R){p.x, p.y, z.w, z.h}); }
static BOOL rep_can_pdf(id k, SEL c, id data) { (void)k; (void)c; if (!data || MSG0(u64, data, "length") < 5) return NO; const char *b = MSG0(const char *, data, "bytes"); return !memcmp(b, "%PDF", 4); }
static id rep_types_pdf(id k, SEL c) { (void)k; (void)c; return MSG(id, (id)kls("NSArray"), "arrayWithObject:", cx_str("com.adobe.pdf")); }
static void pdfr_dealloc_note(id self) { pdf_free_doc(pdf_doc(self)); }

static id epsr_init_data(id self, SEL c, id data) {
    (void)c;
    if (!data) { objc_release(self); return 0; }
    const unsigned char *b = MSG0(const unsigned char *, data, "bytes");
    unsigned long n = MSG0(u64, data, "length"), po, pl, to, tl;
    const unsigned char *ps = b;
    unsigned long psn = n;
    id preview = 0;
    if (eps_dos_header(b, n, &po, &pl, &to, &tl)) {
        ps = b + po; psn = pl;
        if (tl) {
            id td = MSG(id, (id)kls("NSData"), "dataWithBytes:length:", b + to, (u64)tl);
            preview = MSG(id, MSG0(id, (id)kls("NSImage"), "alloc"), "initWithData:", td);
            if (preview) objc_autorelease(preview);
        }
    }
    if (psn < 4 || memcmp(ps, "%!PS", 4)) { objc_release(self); return 0; }
    double x, y, w, h;
    if (!eps_bbox(ps, psn, &x, &y, &w, &h)) { x = y = 0; w = 612; h = 792; }
    pset(self, "data", data);
    pset(self, "bbox", rect_val((R){x, y, w, h}));
    pset(self, "preview", preview);
    return self;
}
static id epsr_with_data(id k, SEL c, id data) { id r = epsr_init_data(cx_alloc_instance((cx_class_t *)k), c, data); return r ? objc_autorelease(r) : 0; }
static id epsr_data(id self, SEL c) { (void)c; return pget(self, "data"); }
static R epsr_bbox(id self, SEL c) { (void)c; return val_rect(pget(self, "bbox")); }
static S epsr_size(id self, SEL c) { R b = epsr_bbox(self, c); return (S){b.w, b.h}; }
static BOOL epsr_draw_in(id self, SEL c, R r) { return rep_draw_in(self, c, r, pget(self, "preview")); }
static BOOL epsr_draw(id self, SEL c) { S z = epsr_size(self, 0); return epsr_draw_in(self, c, (R){0, 0, z.w, z.h}); }
static BOOL epsr_draw_at(id self, SEL c, P p) { S z = epsr_size(self, 0); return epsr_draw_in(self, c, (R){p.x, p.y, z.w, z.h}); }
static void epsr_prepare(id self, SEL c) { (void)self; (void)c; }
static BOOL rep_can_eps(id k, SEL c, id data) {
    (void)k; (void)c;
    if (!data || MSG0(u64, data, "length") < 4) return NO;
    const unsigned char *b = MSG0(const unsigned char *, data, "bytes");
    return !memcmp(b, "%!PS", 4) || (b[0] == 0xC5 && b[1] == 0xD0 && b[2] == 0xD3 && b[3] == 0xC6);
}
static id rep_types_eps(id k, SEL c) { (void)k; (void)c; return MSG(id, (id)kls("NSArray"), "arrayWithObject:", cx_str("com.adobe.encapsulated-postscript")); }

/* NSImage initWithData: com PDF: a primeira página vira a imagem, no tamanho em pontos da página */
static IMP old_img_init_data;
static id img_init_data(id self, SEL c, id data) {
    if (rep_can_pdf(0, 0, data)) {
        void *doc = pdf_open(data);
        R box;
        id pi = doc ? pdf_page_image(doc, 1, &box) : 0;
        if (doc) CGPDFDocumentRelease(doc);
        if (pi) {
            objc_release(self);
            return objc_retain(pi);
        }
    }
    return old_img_init_data ? ((id (*)(id, SEL, id))old_img_init_data)(self, c, data) : self;
}

/* ================================================================ NSScrubber layouts */
OBJP(slaScrubber, "scrubber")
static R sla_frame(id self, SEL c) { (void)c; return val_rect(pget(self, "frame")); }
static void sla_set_frame(id self, SEL c, R r) { (void)c; pset(self, "frame", rect_val(r)); }
INTP(slaIndex, "itemIndex", 0) DBLP(slaAlpha, "alpha", 1)
static id sla_copy(id self, SEL c, void *z) {
    (void)c; (void)z;
    id n = cx_alloc_instance(object_getClass(self));
    id d = props(self, 0);
    if (d) { id m = MSG0(id, d, "mutableCopy"); objc_setAssociatedObject(n, &k_props, m, 1); objc_release(m); }
    return n;
}
static id sla_for_index(id k, SEL c, i64 i) { (void)c; id a = objc_autorelease(cx_alloc_instance((cx_class_t *)k)); psi(a, "itemIndex", i); psd(a, "alpha", 1); return a; }
static id sl_scrubber(id self, SEL c) { (void)c; return weak_get(self, &k_view); }
static void sl_set_scrubber(id self, SEL c, id s) { (void)c; weak_set(self, &k_view, s); }
static id sl_attr_class(id k, SEL c) { (void)k; (void)c; return (id)&NSScrubberLayoutAttributes_c; }
static R sl_visible(id self, SEL c) { (void)c; id s = sl_scrubber(self, 0); return s ? msg_rect(s, "bounds") : (R){0, 0, 0, 0}; }
static i64 sl_count(id self) {
    id s = sl_scrubber(self, 0);
    id ds = s && responds(s, "dataSource") ? MSG0(id, s, "dataSource") : 0;
    return ds && responds(ds, "numberOfItemsForScrubber:") ? MSG(i64, ds, "numberOfItemsForScrubber:", s) : 0;
}
/* fluxo: itens lado a lado com o tamanho do delegate (ou itemSize), espaçados por itemSpacing */
static S sl_item_size(id self, i64 i) {
    id s = sl_scrubber(self, 0);
    id d = s && responds(s, "delegate") ? MSG0(id, s, "delegate") : 0;
    if (d && responds(d, "scrubber:layout:sizeForItemAtIndex:")) return ((S (*)(id, SEL, id, id, i64))objc_msgSend)(d, sl("scrubber:layout:sizeForItemAtIndex:"), s, self, i);
    if (cx_is_subclass(object_getClass(self), &NSScrubberProportionalLayout_c)) {
        R v = sl_visible(self, 0);
        i64 n = pgi(self, "visibleCount", 1);
        return (S){n > 0 ? v.w / n : v.w, v.h};
    }
    return (S){pgd(self, "itemW", 50), pgd(self, "itemH", 30)};
}
static R sl_frame_at(id self, i64 idx) {
    double x = 0, gap = cx_is_subclass(object_getClass(self), &NSScrubberProportionalLayout_c) ? 0 : pgd(self, "spacing", 0);
    R v = sl_visible(self, 0);
    for (i64 i = 0; i < idx; i++) x += sl_item_size(self, i).w + gap;
    S z = sl_item_size(self, idx);
    return (R){x, (v.h - z.h) / 2, z.w, z.h};
}
static id sl_attrs_at(id self, SEL c, i64 i) { (void)c; id a = sla_for_index((id)&NSScrubberLayoutAttributes_c, 0, i); sla_set_frame(a, 0, sl_frame_at(self, i)); return a; }
static id sl_attrs_in(id self, SEL c, R r) {
    (void)c;
    id set = MSG0(id, (id)kls("NSMutableSet"), "set");
    for (i64 i = 0, n = sl_count(self); i < n; i++) {
        R f = sl_frame_at(self, i);
        if (f.x + f.w >= r.x && f.x <= r.x + r.w) MSG(void, set, "addObject:", sl_attrs_at(self, 0, i));
    }
    return set;
}
static S sl_content_size(id self, SEL c) {
    (void)c;
    i64 n = sl_count(self);
    if (!n) return (S){0, sl_visible(self, 0).h};
    R last = sl_frame_at(self, n - 1);
    return (S){last.x + last.w, sl_visible(self, 0).h};
}
static void sl_invalidate(id self, SEL c) { (void)c; redraw(sl_scrubber(self, 0)); }
static BOOL sl_should_invalidate_sel(id self, SEL c) { (void)self; (void)c; return NO; }
static BOOL sl_should_invalidate_bounds(id self, SEL c, R r) { (void)c; (void)r; return cx_is_subclass(object_getClass(self), &NSScrubberProportionalLayout_c); }
DBLP(sflSpacing, "spacing", 0)
static S sfl_item_size(id self, SEL c) { (void)c; return (S){pgd(self, "itemW", 50), pgd(self, "itemH", 30)}; }
static void sfl_set_item_size(id self, SEL c, S z) { (void)c; psd(self, "itemW", z.w); psd(self, "itemH", z.h); }
static id spl_init_count(id self, SEL c, i64 n) { (void)c; psi(self, "visibleCount", n > 0 ? n : 1); return self; }
INTP(splCount, "visibleCount", 1)

/* ================================================================ NSSharingServicePickerTouchBarItem */
OBJP(spButtonTitle, "buttonTitle") OBJP(spButtonImage, "buttonImage")
BOOLP(spEnabled, "enabled", 1)
static id sp_init_ident(id self, SEL c, id ident) {
    (void)c;
    pset(self, "identifier", ident);
    psi(self, "enabled", 1);
    pset(self, "buttonTitle", cx_str("Compartilhar"));
    return self;
}
static id sp_ident(id self, SEL c) { (void)c; return pget(self, "identifier"); }
static id sp_custom_label(id self, SEL c) { (void)c; return pget(self, "buttonTitle"); }
/* o clique do item (barra de toque ou quem a imita) abre o seletor de compartilhamento com os itens do delegate */
static void sp_activate(id self, SEL c, id sender) {
    (void)c; (void)sender;
    id d = weak_get(self, &k_delegate);
    id items = d && responds(d, "itemsForSharingServicePickerTouchBarItem:") ? MSG(id, d, "itemsForSharingServicePickerTouchBarItem:", self) : 0;
    if (!items || !MSG0(u64, items, "count")) { NSBeep(); return; }
    id picker = MSG(id, MSG0(id, (id)kls("NSSharingServicePicker"), "alloc"), "initWithItems:", items);
    if (!picker) return;
    id win = MSG0(id, NSApp, "keyWindow");
    id view = win ? MSG0(id, win, "contentView") : 0;
    if (view && responds(picker, "showRelativeToRect:ofView:preferredEdge:"))
        ((void (*)(id, SEL, R, id, u64))objc_msgSend)(picker, sl("showRelativeToRect:ofView:preferredEdge:"), msg_rect(view, "bounds"), view, (u64)1);
    objc_autorelease(picker);
}

/* ================================================================ registro */
#define ADD(k, s, f) cx_add_method(k, s, (void *)(f))
#define PROP(k, get, set, fn) ADD(k, get, g_##fn); ADD(k, set, s_##fn)
static cx_class_t *ensure(cx_class_t *k, cx_class_t *m, const char *name, const char *super) {
    cx_class_t *e = kls(name);
    if (e) return e;
    cx_class_t *s = kls(super);
    if (!s) s = kls("NSObject");
    return cx_define_class(k, m, name, s, 0);
}
/* cedo: classes de que outras herdam (NSClickGestureRecognizer, NSArrayController, NSTextTableBlock) */
void cx_appkit_more_classes(void) {
    ensure(&NSGestureRecognizer_c, &NSGestureRecognizer_m, "NSGestureRecognizer", "NSObject");
    ensure(&NSController_c, &NSController_m, "NSController", "NSObject");
    ensure(&NSObjectController_c, &NSObjectController_m, "NSObjectController", "NSController");
    ensure(&NSTextBlock_c, &NSTextBlock_m, "NSTextBlock", "NSObject");
    ensure(&NSScrubberLayout_c, &NSScrubberLayout_m, "NSScrubberLayout", "NSObject");
}
void cx_appkit_more_init(void) {
    cx_class_t *k, *view = kls("NSView");
    cx_appkit_more_classes();
    /* gestos */
    k = &NSGestureRecognizer_c;
    ADD(k, "init", grv_init_plain); ADD(k, "initWithTarget:action:", grv_init); ADD(k, "initWithCoder:", grv_init_coder);
    ADD(k, "target", g_target); ADD(k, "setTarget:", s_target); ADD(k, "action", g_action); ADD(k, "setAction:", s_action);
    ADD(k, "delegate", g_delegate); ADD(k, "setDelegate:", s_delegate); ADD(k, "view", grv_view);
    ADD(k, "state", grv_state); ADD(k, "setState:", grv_set_state); ADD(k, "isEnabled", grv_enabled); ADD(k, "setEnabled:", grv_set_enabled);
    ADD(k, "enabled", grv_enabled); ADD(k, "locationInView:", grv_location); ADD(k, "reset", grv_reset);
    ADD(k, "canPreventGestureRecognizer:", grv_can_prevent); ADD(k, "canBePreventedByGestureRecognizer:", grv_can_be_prevented);
    ADD(k, "shouldRequireFailureOfGestureRecognizer:", grv_should_require); ADD(k, "shouldBeRequiredToFailByGestureRecognizer:", no_imp);
    ADD(k, "requireGestureRecognizerToFail:", grv_require_fail);
    PROP(k, "delaysPrimaryMouseButtonEvents", "setDelaysPrimaryMouseButtonEvents:", delaysPrimary);
    PROP(k, "delaysSecondaryMouseButtonEvents", "setDelaysSecondaryMouseButtonEvents:", delaysSecondary);
    PROP(k, "delaysOtherMouseButtonEvents", "setDelaysOtherMouseButtonEvents:", delaysOther);
    PROP(k, "delaysKeyEvents", "setDelaysKeyEvents:", delaysKey); PROP(k, "delaysMagnificationEvents", "setDelaysMagnificationEvents:", delaysMag);
    PROP(k, "delaysRotationEvents", "setDelaysRotationEvents:", delaysRot); PROP(k, "name", "setName:", grName);
    PROP(k, "pressureConfiguration", "setPressureConfiguration:", pressureConfig); PROP(k, "allowedTouchTypes", "setAllowedTouchTypes:", allowedTouchTypes);
    ADD(k, "mouseDown:", noop1_imp); ADD(k, "mouseDragged:", noop1_imp); ADD(k, "mouseUp:", noop1_imp);
    ADD(k, "rightMouseDown:", noop1_imp); ADD(k, "rightMouseDragged:", noop1_imp); ADD(k, "rightMouseUp:", noop1_imp);
    ADD(k, "otherMouseDown:", noop1_imp); ADD(k, "otherMouseUp:", noop1_imp); ADD(k, "keyDown:", noop1_imp); ADD(k, "keyUp:", noop1_imp);
    ADD(k, "flagsChanged:", noop1_imp); ADD(k, "tabletPoint:", noop1_imp); ADD(k, "magnifyWithEvent:", noop1_imp); ADD(k, "rotateWithEvent:", noop1_imp);
    ADD(k, "pressureChangeWithEvent:", noop1_imp); ADD(k, "touchesBeganWithEvent:", noop1_imp); ADD(k, "touchesMovedWithEvent:", noop1_imp);
    ADD(k, "touchesEndedWithEvent:", noop1_imp); ADD(k, "touchesCancelledWithEvent:", noop1_imp);
    cx_class_t *pan = ensure(&NSPanGestureRecognizer_c, &NSPanGestureRecognizer_m, "NSPanGestureRecognizer", "NSGestureRecognizer");
    cx_class_t *press = ensure(&NSPressGestureRecognizer_c, &NSPressGestureRecognizer_m, "NSPressGestureRecognizer", "NSGestureRecognizer");
    cx_class_t *mag = ensure(&NSMagnificationGestureRecognizer_c, &NSMagnificationGestureRecognizer_m, "NSMagnificationGestureRecognizer", "NSGestureRecognizer");
    cx_class_t *rot = ensure(&NSRotationGestureRecognizer_c, &NSRotationGestureRecognizer_m, "NSRotationGestureRecognizer", "NSGestureRecognizer");
    cx_class_t *click = kls("NSClickGestureRecognizer");
    cx_class_t *mouse_kinds[3] = {pan, press, click};
    for (int i = 0; i < 3; i++) {
        if (!mouse_kinds[i]) continue;
        ADD(mouse_kinds[i], "mouseDown:", grk_down); ADD(mouse_kinds[i], "mouseDragged:", grk_drag); ADD(mouse_kinds[i], "mouseUp:", grk_up);
        ADD(mouse_kinds[i], "rightMouseDown:", grk_down); ADD(mouse_kinds[i], "rightMouseDragged:", grk_drag); ADD(mouse_kinds[i], "rightMouseUp:", grk_up);
        ADD(mouse_kinds[i], "buttonMask", grk_mask); ADD(mouse_kinds[i], "setButtonMask:", grk_set_mask);
        ADD(mouse_kinds[i], "numberOfTouchesRequired", grk_touches); ADD(mouse_kinds[i], "setNumberOfTouchesRequired:", noop1_imp);
        /* o initWithTarget:action: do appkit_apps_ui.c guardava alvo e ação em outro lugar: aqui vale o da base */
        ADD(mouse_kinds[i], "initWithTarget:action:", grv_init); ADD(mouse_kinds[i], "target", g_target); ADD(mouse_kinds[i], "setTarget:", s_target);
        ADD(mouse_kinds[i], "action", g_action); ADD(mouse_kinds[i], "setAction:", s_action);
    }
    ADD(pan, "translationInView:", pan_translation); ADD(pan, "setTranslation:inView:", pan_set_translation); ADD(pan, "velocityInView:", pan_velocity);
    ADD(press, "minimumPressDuration", grk_min_dur); ADD(press, "setMinimumPressDuration:", grk_set_min_dur);
    ADD(press, "allowableMovement", grk_allow); ADD(press, "setAllowableMovement:", grk_set_allow);
    if (click) { ADD(click, "numberOfClicksRequired", grk_clicks); ADD(click, "setNumberOfClicksRequired:", grk_set_clicks); }
    ADD(mag, "magnifyWithEvent:", grk_mag); ADD(mag, "magnification", mag_value); ADD(mag, "setMagnification:", mag_set);
    ADD(rot, "rotateWithEvent:", grk_rot); ADD(rot, "rotation", rot_rad); ADD(rot, "setRotation:", rot_set_rad);
    ADD(rot, "rotationInDegrees", rot_deg); ADD(rot, "setRotationInDegrees:", rot_set_deg);
    if (view) {
        ADD(view, "gestureRecognizers", view_grs); ADD(view, "setGestureRecognizers:", view_set_grs);
        ADD(view, "addGestureRecognizer:", view_add_gr); ADD(view, "removeGestureRecognizer:", view_remove_gr);
        ADD(view, "mouseDown:", view_mouse_down_fwd);   /* os reconhecedores agora recebem o mouse na janela (cx_gr_mouse) */
    }
    /* NSTouch */
    k = ensure(&NSTouch_c, &NSTouch_m, "NSTouch", "NSObject");
    ADD(k, "identity", g_tIdentity); ADD(k, "device", g_tDevice); ADD(k, "phase", g_tPhase); ADD(k, "type", g_tType);
    ADD(k, "isResting", g_tResting); ADD(k, "normalizedPosition", t_norm); ADD(k, "deviceSize", t_dev_size);
    ADD(k, "locationInView:", t_loc); ADD(k, "previousLocationInView:", t_prev); ADD(k, "copyWithZone:", t_copy); ADD(k, "description", t_desc);
    { cx_class_t *ev = kls("NSEvent"); if (ev) { ADD(ev, "touchesMatchingPhase:inView:", ev_touches); ADD(ev, "allTouches", ev_all_touches); ADD(ev, "touchesForView:", ev_touches_for_view); } }
    /* NSAnimation e NSViewAnimation */
    cx_class_t *an = kls("NSAnimation");
    if (an) {
        ADD(an, "init", an_init_plain); ADD(an, "initWithDuration:animationCurve:", an_init);
        ADD(an, "startAnimation", an_start); ADD(an, "stopAnimation", an_stop); ADD(an, "isAnimating", an_animating);
        ADD(an, "currentProgress", an_progress); ADD(an, "setCurrentProgress:", an_set_progress); ADD(an, "currentValue", an_value);
        PROP(an, "duration", "setDuration:", anDuration); PROP(an, "animationCurve", "setAnimationCurve:", anCurve);
        PROP(an, "animationBlockingMode", "setAnimationBlockingMode:", anBlocking);
        ADD(an, "frameRate", an_frame_rate); ADD(an, "setFrameRate:", an_set_frame_rate);
        ADD(an, "delegate", g_delegate); ADD(an, "setDelegate:", s_delegate);
        ADD(an, "progressMarks", an_marks); ADD(an, "setProgressMarks:", an_set_marks);
        ADD(an, "addProgressMark:", an_add_mark); ADD(an, "removeProgressMark:", an_remove_mark);
        ADD(an, "startWhenAnimation:reachesProgress:", an_start_when); ADD(an, "stopWhenAnimation:reachesProgress:", an_stop_when);
        ADD(an, "clearStartAnimation", an_clear_start); ADD(an, "clearStopAnimation", an_clear_stop); ADD(an, "runLoopModesForAnimating", an_modes);
        k = ensure(&NSViewAnimation_c, &NSViewAnimation_m, "NSViewAnimation", "NSAnimation");
        ADD(k, "initWithViewAnimations:", vanim_init); ADD(k, "viewAnimations", vanim_anims); ADD(k, "setViewAnimations:", vanim_set_anims);
        ADD(k, "setCurrentProgress:", vanim_set_progress); ADD(k, "startAnimation", vanim_start); ADD(k, "stopAnimation", vanim_stop);
    }
    /* controladores */
    k = &NSController_c;
    ADD(k, "commitEditing", ctr_commit); ADD(k, "discardEditing", ctr_discard); ADD(k, "isEditing", ctr_editing);
    ADD(k, "objectDidBeginEditing:", ctr_begin); ADD(k, "objectDidEndEditing:", ctr_end);
    ADD(k, "commitEditingWithDelegate:didCommitSelector:contextInfo:", ctr_commit_delegate); ADD(k, "commitEditingAndReturnError:", ctr_commit_error);
    k = &NSObjectController_c;
    ADD(k, "init", oc_init); ADD(k, "initWithContent:", oc_init_content); ADD(k, "initWithCoder:", oc_init_coder);
    ADD(k, "content", oc_content); ADD(k, "setContent:", oc_set_content); ADD(k, "selection", oc_selection); ADD(k, "selectedObjects", oc_selected);
    ADD(k, "objectClass", oc_class); ADD(k, "setObjectClass:", oc_set_class); ADD(k, "newObject", oc_new);
    ADD(k, "addObject:", oc_add_object); ADD(k, "removeObject:", oc_remove_object); ADD(k, "add:", oc_add); ADD(k, "remove:", oc_remove);
    ADD(k, "canAdd", oc_can_add); ADD(k, "canRemove", oc_can_remove); ADD(k, "prepareContent", oc_prepare); ADD(k, "fetch:", oc_fetch);
    ADD(k, "validateUserInterfaceItem:", oc_validate); ADD(k, "validateMenuItem:", oc_validate);
    PROP(k, "isEditable", "setEditable:", ocEditable); ADD(k, "editable", g_ocEditable);
    PROP(k, "automaticallyPreparesContent", "setAutomaticallyPreparesContent:", ocAutoPrepare);
    PROP(k, "usesLazyFetching", "setUsesLazyFetching:", ocLazy); PROP(k, "entityName", "setEntityName:", ocEntity);
    PROP(k, "fetchPredicate", "setFetchPredicate:", ocPredicate); PROP(k, "managedObjectContext", "setManagedObjectContext:", ocMOC);
    /* NSFontManager e NSFontPanel */
    cx_class_t *fm = kls("NSFontManager");
    if (fm) {
        cx_add_cmethod(fm, "sharedFontManager", (void *)fm_shared);
        ADD(fm, "availableFontFamilies", fm_families); ADD(fm, "availableFonts", fm_fonts); ADD(fm, "availableMembersOfFontFamily:", fm_members);
        ADD(fm, "traitsOfFont:", fm_traits); ADD(fm, "weightOfFont:", fm_weight); ADD(fm, "fontWithFamily:traits:weight:size:", fm_font_family);
        ADD(fm, "convertFont:toHaveTrait:", fm_convert_trait); ADD(fm, "convertFont:toNotHaveTrait:", fm_convert_not_trait);
        ADD(fm, "convertFont:toSize:", fm_convert_size); ADD(fm, "convertFont:toFamily:", fm_convert_family); ADD(fm, "convertFont:toFace:", fm_convert_face);
        ADD(fm, "convertFont:", fm_convert); ADD(fm, "selectedFont", fm_selected); ADD(fm, "isMultiple", fm_multiple);
        ADD(fm, "setSelectedFont:isMultiple:", fm_set_selected); ADD(fm, "sendAction", fm_send_action);
        ADD(fm, "action", fm_action); ADD(fm, "setAction:", fm_set_action); ADD(fm, "target", g_target); ADD(fm, "setTarget:", s_target);
        ADD(fm, "delegate", g_delegate); ADD(fm, "setDelegate:", s_delegate);
        ADD(fm, "modifyFont:", fm_modify); ADD(fm, "addFontTrait:", fm_add_trait); ADD(fm, "removeFontTrait:", fm_remove_trait);
        ADD(fm, "modifyFontViaPanel:", fm_modify_via_panel); ADD(fm, "fontPanel:", fm_panel); ADD(fm, "orderFrontFontPanel:", fm_order_front);
        ADD(fm, "fontMenu:", fm_menu); ADD(fm, "setFontMenu:", fm_set_menu); ADD(fm, "localizedNameForFamily:face:", fm_localized);
        ADD(fm, "isEnabled", fm_enabled); ADD(fm, "setEnabled:", fm_set_enabled); ADD(fm, "currentFontAction", fm_current_action);
    }
    k = ensure(&_MOSFontList_c, &_MOSFontList_m, "_MOSFontList", "NSView");
    ADD(k, "drawRect:", fl_draw); ADD(k, "isFlipped", fl_flipped); ADD(k, "mouseDown:", fl_mouse); ADD(k, "scrollWheel:", fl_scroll);
    k = ensure(&NSFontPanel_c, &NSFontPanel_m, "NSFontPanel", "NSPanel");
    cx_add_cmethod(k, "sharedFontPanel", (void *)fp_shared); cx_add_cmethod(k, "sharedFontPanelExists", (void *)fp_exists);
    ADD(k, "setPanelFont:isMultiple:", fp_set_panel_font); ADD(k, "panelConvertFont:", fp_convert);
    ADD(k, "isEnabled", fp_enabled); ADD(k, "setEnabled:", fp_set_enabled); ADD(k, "enabled", fp_enabled);
    ADD(k, "worksWhenModal", yes_imp); ADD(k, "reloadDefaultFontFamilies", fp_reload);
    ADD(k, "accessoryView", fp_accessory); ADD(k, "setAccessoryView:", fp_set_accessory); ADD(k, "_cxListChanged:", fp_list_changed);
    /* NSFontCollection */
    k = ensure(&NSFontCollection_c, &NSFontCollection_m, "NSFontCollection", "NSObject");
    cx_class_t *mk = ensure(&NSMutableFontCollection_c, &NSMutableFontCollection_m, "NSMutableFontCollection", "NSFontCollection");
    cx_add_cmethod(k, "fontCollectionWithAllAvailableDescriptors", (void *)fc_all);
    cx_add_cmethod(k, "fontCollectionWithDescriptors:", (void *)fc_with_desc); cx_add_cmethod(k, "fontCollectionWithLocale:", (void *)fc_with_locale);
    cx_add_cmethod(k, "fontCollectionWithName:", (void *)fc_named); cx_add_cmethod(k, "fontCollectionWithName:visibility:", (void *)fc_named_vis);
    cx_add_cmethod(k, "allFontCollectionNames", (void *)fc_names);
    cx_add_cmethod(k, "showFontCollection:withName:visibility:error:", (void *)fc_show);
    cx_add_cmethod(k, "hideFontCollectionWithName:visibility:error:", (void *)fc_hide);
    cx_add_cmethod(k, "renameFontCollectionWithName:visibility:toName:error:", (void *)fc_rename);
    ADD(k, "queryDescriptors", fc_query); ADD(k, "exclusionDescriptors", fc_excl);
    ADD(k, "matchingDescriptors", fc_matching); ADD(k, "matchingDescriptorsWithOptions:", fc_matching_opts);
    ADD(k, "matchingDescriptorsForFamily:", fc_matching_family); ADD(k, "matchingDescriptorsForFamily:options:", fc_matching_family_opts);
    ADD(k, "copyWithZone:", fc_copy); ADD(k, "mutableCopyWithZone:", fc_mcopy);
    ADD(mk, "setQueryDescriptors:", mfc_set_query); ADD(mk, "setExclusionDescriptors:", mfc_set_excl);
    ADD(mk, "addQueryForDescriptors:", mfc_add_query); ADD(mk, "removeQueryForDescriptors:", mfc_remove_query);
    /* NSTextBlock */
    k = &NSTextBlock_c;
    ADD(k, "setWidth:type:forLayer:edge:", tb_set_width_edge); ADD(k, "setWidth:type:forLayer:", tb_set_width);
    ADD(k, "widthForLayer:edge:", tb_width); ADD(k, "widthValueTypeForLayer:edge:", tb_width_type);
    ADD(k, "setValue:type:forDimension:", tb_set_value); ADD(k, "valueForDimension:", tb_value); ADD(k, "valueTypeForDimension:", tb_value_type);
    ADD(k, "setContentWidth:type:", tb_set_content_width); ADD(k, "contentWidth", tb_content_width); ADD(k, "contentWidthValueType", tb_content_width_type);
    PROP(k, "backgroundColor", "setBackgroundColor:", tbBg); PROP(k, "verticalAlignment", "setVerticalAlignment:", tbVAlign);
    ADD(k, "setBorderColor:forEdge:", tb_set_border_color_edge); ADD(k, "setBorderColor:", tb_set_border_color); ADD(k, "borderColorForEdge:", tb_border_color);
    ADD(k, "rectForLayoutAtPoint:inRect:textContainer:characterRange:", tb_rect_for_layout);
    ADD(k, "boundsRectForContentRect:inRect:textContainer:characterRange:", tb_bounds_for_content);
    ADD(k, "drawBackgroundWithFrame:inView:characterRange:layoutManager:", tb_draw_bg); ADD(k, "copyWithZone:", tb_copy);
    /* NSPDFImageRep / NSEPSImageRep */
    k = ensure(&NSPDFImageRep_c, &NSPDFImageRep_m, "NSPDFImageRep", "NSImageRep");
    cx_add_cmethod(k, "imageRepWithData:", (void *)pdfr_with_data); cx_add_cmethod(k, "canInitWithData:", (void *)rep_can_pdf);
    cx_add_cmethod(k, "imageUnfilteredTypes", (void *)rep_types_pdf);
    ADD(k, "initWithData:", pdfr_init_data); ADD(k, "PDFRepresentation", pdfr_data); ADD(k, "pageCount", pdfr_count);
    ADD(k, "currentPage", pdfr_page); ADD(k, "setCurrentPage:", pdfr_set_page); ADD(k, "bounds", pdfr_bounds); ADD(k, "size", pdfr_size);
    ADD(k, "pixelsWide", pdfr_px_w); ADD(k, "pixelsHigh", pdfr_px_h);
    ADD(k, "draw", pdfr_draw); ADD(k, "drawAtPoint:", pdfr_draw_at); ADD(k, "drawInRect:", pdfr_draw_in);
    (void)pdfr_dealloc_note;
    k = ensure(&NSEPSImageRep_c, &NSEPSImageRep_m, "NSEPSImageRep", "NSImageRep");
    cx_add_cmethod(k, "imageRepWithData:", (void *)epsr_with_data); cx_add_cmethod(k, "canInitWithData:", (void *)rep_can_eps);
    cx_add_cmethod(k, "imageUnfilteredTypes", (void *)rep_types_eps);
    ADD(k, "initWithData:", epsr_init_data); ADD(k, "EPSRepresentation", epsr_data); ADD(k, "boundingBox", epsr_bbox); ADD(k, "size", epsr_size);
    ADD(k, "draw", epsr_draw); ADD(k, "drawAtPoint:", epsr_draw_at); ADD(k, "drawInRect:", epsr_draw_in); ADD(k, "prepareGState", epsr_prepare);
    { cx_class_t *ni = kls("NSImage"); if (ni) { old_img_init_data = class_getMethodImplementation(ni, sl("initWithData:")); ADD(ni, "initWithData:", img_init_data); } }
    /* NSScrubber layouts */
    k = ensure(&NSScrubberLayoutAttributes_c, &NSScrubberLayoutAttributes_m, "NSScrubberLayoutAttributes", "NSObject");
    cx_add_cmethod(k, "layoutAttributesForItemAtIndex:", (void *)sla_for_index);
    ADD(k, "frame", sla_frame); ADD(k, "setFrame:", sla_set_frame); PROP(k, "itemIndex", "setItemIndex:", slaIndex);
    PROP(k, "alpha", "setAlpha:", slaAlpha); ADD(k, "copyWithZone:", sla_copy);
    k = &NSScrubberLayout_c;
    cx_add_cmethod(k, "layoutAttributesClass", (void *)sl_attr_class);
    ADD(k, "scrubber", sl_scrubber); ADD(k, "setScrubber:", sl_set_scrubber); ADD(k, "visibleRect", sl_visible);
    ADD(k, "invalidateLayout", sl_invalidate); ADD(k, "prepareLayout", noop_imp); ADD(k, "scrubberContentSize", sl_content_size);
    ADD(k, "layoutAttributesForItemAtIndex:", sl_attrs_at); ADD(k, "layoutAttributesForItemsInRect:", sl_attrs_in);
    ADD(k, "shouldInvalidateLayoutForSelectionChange", sl_should_invalidate_sel); ADD(k, "shouldInvalidateLayoutForHighlightChange", sl_should_invalidate_sel);
    ADD(k, "shouldInvalidateLayoutForChangeFromVisibleRect:toVisibleRect:", sl_should_invalidate_bounds);
    ADD(k, "automaticallyMirrorsInRightToLeftLayout", yes_imp);
    k = ensure(&NSScrubberFlowLayout_c, &NSScrubberFlowLayout_m, "NSScrubberFlowLayout", "NSScrubberLayout");
    PROP(k, "itemSpacing", "setItemSpacing:", sflSpacing); ADD(k, "itemSize", sfl_item_size); ADD(k, "setItemSize:", sfl_set_item_size);
    ADD(k, "invalidateLayoutForItemsAtIndexes:", noop1_imp);
    k = ensure(&NSScrubberProportionalLayout_c, &NSScrubberProportionalLayout_m, "NSScrubberProportionalLayout", "NSScrubberLayout");
    ADD(k, "initWithNumberOfVisibleItems:", spl_init_count); PROP(k, "numberOfVisibleItems", "setNumberOfVisibleItems:", splCount);
    /* NSSharingServicePickerTouchBarItem */
    k = ensure(&NSSharingServicePickerTouchBarItem_c, &NSSharingServicePickerTouchBarItem_m, "NSSharingServicePickerTouchBarItem", "NSTouchBarItem");
    ADD(k, "initWithIdentifier:", sp_init_ident); ADD(k, "identifier", sp_ident); ADD(k, "customizationLabel", sp_custom_label);
    ADD(k, "delegate", g_delegate); ADD(k, "setDelegate:", s_delegate);
    PROP(k, "buttonTitle", "setButtonTitle:", spButtonTitle); PROP(k, "buttonImage", "setButtonImage:", spButtonImage);
    PROP(k, "isEnabled", "setEnabled:", spEnabled); ADD(k, "enabled", g_spEnabled); ADD(k, "_cxActivate:", sp_activate);
    (void)font_size;
}
