/*
 * mOS libSystem — controles do AppKit que eram classes vazias: NSPopUpButton (pop-up e pull-down, com NSMenu de
 * verdade e o menu flutuante de appkit_menupop.c) e NSSegmentedControl (larguras, distribuição, modos de seleção,
 * menus por segmento). O cálculo dos segmentos fica no núcleo testado no host (appkit_segment_core.c,
 * userland/tests-appkit/controls_host.c).
 *
 * As duas classes começam pelo -initWithFrame: do NSControl (não do NSButton): o desenho é todo daqui, o
 * target/action, a fonte e o estado habilitado continuam os do NSControl.
 * Registro: cx_appkit_controls_init(), chamado do cx_appkit_input_init depois do menupop.
 */
#include "foundation.h"
#include "sys.h"
#include "libc.h"
#include "appkit_menupop_core.h"
#include "appkit_segment_core.h"

id objc_msgSend_stret(id self, SEL op, ...);
id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
IMP class_getMethodImplementation(cx_class_t *c, SEL s);
extern id NSApp;
id cx_color(double r, double g, double b, double a);
void free(void *p);

typedef struct { double x, y; } P;
typedef struct { double w, h; } S;
typedef struct { double x, y, w, h; } R;
void NSRectFill(R r);
void NSFrameRect(R r);
id cx_menu_popup_choose(id menu, int pos, P pt, int mode, double min_w, double font_size, id from_view);
int cx_menu_perform(id item, id from_view);
int cx_menu_item_valid(id menu, id item, id from_view);

static cx_class_t *kls(const char *n) { return cx_class_named(n); }
static SEL sl(const char *n) { return sel_registerName(n); }
static BOOL responds(id o, const char *s) { return o && cx_responds(object_getClass(o), sl(s)); }
static R msg_rect(id o, const char *s) { return ((R (*)(id, SEL))objc_msgSend_stret)(o, sl(s)); }
static R msg_rect_r(id o, const char *s, R r) { return ((R (*)(id, SEL, R))objc_msgSend_stret)(o, sl(s), r); }
static S m_size(id s, id attrs) { return ((S (*)(id, SEL, id))objc_msgSend)(s, sl("sizeWithAttributes:"), attrs); }
static void m_draw_at(id s, P p, id attrs) { ((void (*)(id, SEL, P, id))objc_msgSend)(s, sl("drawAtPoint:withAttributes:"), p, attrs); }
static P m_loc(id view, id ev) {
    P w = ((P (*)(id, SEL))objc_msgSend)(ev, sl("locationInWindow"));
    return ((P (*)(id, SEL, P, id))objc_msgSend)(view, sl("convertPoint:fromView:"), w, (id)0);
}
static P view_to_screen(id view, P p) {
    P w = ((P (*)(id, SEL, P, id))objc_msgSend)(view, sl("convertPoint:toView:"), p, (id)0);
    id win = MSG0(id, view, "window");
    if (!win) return w;
    R r = msg_rect_r(win, "convertRectToScreen:", (R){w.x, w.y, 0, 0});
    return (P){r.x, r.y};
}
static void redraw(id v) { MSG(void, v, "setNeedsDisplay:", (BOOL)YES); }
static id control_init(id self, R f) {
    SEL s = sl("initWithFrame:");
    return ((id (*)(id, SEL, R))class_getMethodImplementation(kls("NSControl"), s))(self, s, f);
}
static BOOL enabled(id self) { return !responds(self, "isEnabled") || MSG0(BOOL, self, "isEnabled"); }
static id ctl_font(id self) {
    id f = responds(self, "font") ? MSG0(id, self, "font") : 0;
    return f ? f : MSG(id, (id)kls("NSFont"), "systemFontOfSize:", 13.0);
}
static id attrs(id font, double r, double g, double b, double a) {
    extern id NSFontAttributeName, NSForegroundColorAttributeName;
    id d = MSG0(id, (id)kls("NSMutableDictionary"), "dictionary");
    MSG(void, d, "setObject:forKey:", font, NSFontAttributeName);
    MSG(void, d, "setObject:forKey:", cx_color(r, g, b, a), NSForegroundColorAttributeName);
    return d;
}
static void send_own_action(id self) {
    SEL a = responds(self, "action") ? MSG0(SEL, self, "action") : 0;
    id t = responds(self, "target") ? MSG0(id, self, "target") : 0;
    if (a) MSG(BOOL, self, "sendAction:to:", a, t);
}
static void post(const char *name, id obj) {
    id nc = MSG0(id, (id)kls("NSNotificationCenter"), "defaultCenter");
    if (nc) MSG(void, nc, "postNotificationName:object:", cx_str(name), obj);
}
static void fill(R r, double R_, double G, double B, double A) { MSG0(void, cx_color(R_, G, B, A), "setFill"); NSRectFill(r); }

/* ================================================================ NSPopUpButton */
typedef struct pstate { struct pstate *next; id obj, menu; i64 sel; BOOL pulls, alters, uses_item; i64 edge; } pstate_t;
static pstate_t *g_pb;
static pstate_t *pb(id o) {
    for (pstate_t *p = g_pb; p; p = p->next) if (p->obj == o) return p;
    pstate_t *p = calloc(1, sizeof *p);
    if (!p) return 0;
    p->obj = o; p->sel = -1; p->alters = YES; p->uses_item = YES; p->edge = 1;
    p->menu = MSG(id, MSG0(id, (id)kls("NSMenu"), "alloc"), "initWithTitle:", cx_str(""));
    p->next = g_pb; g_pb = p;
    return p;
}
static void pb_forget(id o) {
    for (pstate_t **pp = &g_pb; *pp; pp = &(*pp)->next) if ((*pp)->obj == o) { pstate_t *p = *pp; *pp = p->next; objc_release(p->menu); free(p); return; }
}
static i64 pb_count(id self) { return MSG0(i64, pb(self)->menu, "numberOfItems"); }
static id pb_item(id self, i64 i) { return i >= 0 && i < pb_count(self) ? MSG(id, pb(self)->menu, "itemAtIndex:", i) : 0; }
static void pb_apply_state(id self) {
    pstate_t *p = pb(self);
    if (!p->alters || p->pulls) return;
    i64 n = pb_count(self);
    for (i64 i = 0; i < n; i++) { id it = pb_item(self, i); if (it) MSG(void, it, "setState:", (i64)(i == p->sel ? 1 : 0)); }
}
static void pb_select_index(id self, SEL c, i64 i) {
    (void)c;
    pstate_t *p = pb(self);
    i64 n = pb_count(self);
    p->sel = i >= 0 && i < n ? i : -1;
    pb_apply_state(self);
    redraw(self);
}
static void pb_fix_after_change(id self) {
    pstate_t *p = pb(self);
    i64 n = pb_count(self);
    if (p->sel >= n) p->sel = n - 1;
    if (p->sel < 0 && n > 0 && !p->pulls) p->sel = 0;
    pb_apply_state(self);
    redraw(self);
}

static id pb_init_frame(id self, SEL c, R f) {
    (void)c;
    self = control_init(self, f);
    if (self) pb(self);
    return self;
}
static id pb_init_pulls(id self, SEL c, R f, BOOL pulls) { self = pb_init_frame(self, c, f); if (self) pb(self)->pulls = pulls; return self; }
static void pb_dealloc(id self, SEL c) {
    pb_forget(self);
    IMP sup = class_getMethodImplementation(kls("NSControl"), c);
    if (sup) ((void (*)(id, SEL))sup)(self, c);
}
static id pb_menu(id self, SEL c) { (void)c; return pb(self)->menu; }
static void pb_set_menu(id self, SEL c, id m) {
    (void)c;
    pstate_t *p = pb(self);
    objc_retain(m); objc_release(p->menu);
    p->menu = m ? m : MSG(id, MSG0(id, (id)kls("NSMenu"), "alloc"), "initWithTitle:", cx_str(""));
    p->sel = -1;
    i64 n = pb_count(self);
    for (i64 i = 0; i < n; i++) { id it = pb_item(self, i); if (it && MSG0(i64, it, "state") == 1) { p->sel = i; break; } }
    pb_fix_after_change(self);
}
static BOOL pb_pulls(id self, SEL c) { (void)c; return pb(self)->pulls; }
static void pb_set_pulls(id self, SEL c, BOOL b) { (void)c; pb(self)->pulls = b; redraw(self); }
static BOOL pb_autoen(id self, SEL c) { (void)c; return MSG0(BOOL, pb(self)->menu, "autoenablesItems"); }
static void pb_set_autoen(id self, SEL c, BOOL b) { (void)c; MSG(void, pb(self)->menu, "setAutoenablesItems:", b); }
static i64 pb_edge(id self, SEL c) { (void)c; return pb(self)->edge; }
static void pb_set_edge(id self, SEL c, i64 e) { (void)c; pb(self)->edge = e; }
static BOOL pb_alters(id self, SEL c) { (void)c; return pb(self)->alters; }
static void pb_set_alters(id self, SEL c, BOOL b) { (void)c; pb(self)->alters = b; }
static BOOL pb_uses_item(id self, SEL c) { (void)c; return pb(self)->uses_item; }
static void pb_set_uses_item(id self, SEL c, BOOL b) { (void)c; pb(self)->uses_item = b; }

static id new_item(id title) {
    id it = MSG(id, MSG0(id, (id)kls("NSMenuItem"), "alloc"), "initWithTitle:action:keyEquivalent:", title ? title : cx_str(""), (SEL)0, cx_str(""));
    return objc_autorelease(it);
}
static i64 pb_index_title(id self, SEL c, id t) {
    (void)c;
    i64 n = pb_count(self);
    const char *s = t ? cx_cstr(t) : "";
    for (i64 i = 0; i < n; i++) if (!strcmp(cx_cstr(MSG0(id, pb_item(self, i), "title")), s)) return i;
    return -1;
}
/* como no Cocoa, um título repetido substitui o item antigo */
static void pb_add_title(id self, SEL c, id t) {
    (void)c;
    i64 old = pb_index_title(self, 0, t);
    if (old >= 0) MSG(void, pb(self)->menu, "removeItemAtIndex:", old);
    MSG(void, pb(self)->menu, "addItem:", new_item(t));
    pb_fix_after_change(self);
}
static void pb_add_titles(id self, SEL c, id arr) {
    u64 n = arr ? MSG0(u64, arr, "count") : 0;
    for (u64 i = 0; i < n; i++) pb_add_title(self, c, MSG(id, arr, "objectAtIndex:", i));
}
static void pb_insert_title(id self, SEL c, id t, i64 at) {
    (void)c;
    i64 old = pb_index_title(self, 0, t);
    pstate_t *p = pb(self);
    if (old >= 0) { MSG(void, p->menu, "removeItemAtIndex:", old); if (old < at) at--; if (p->sel == old) p->sel = -1; }
    i64 n = pb_count(self);
    if (at < 0) at = 0;
    if (at > n) at = n;
    MSG(void, p->menu, "insertItem:atIndex:", new_item(t), at);
    if (p->sel >= at) p->sel++;
    pb_fix_after_change(self);
}
static void pb_remove_index(id self, SEL c, i64 i) {
    (void)c;
    pstate_t *p = pb(self);
    if (i < 0 || i >= pb_count(self)) return;
    MSG(void, p->menu, "removeItemAtIndex:", i);
    if (p->sel == i) p->sel = -1; else if (p->sel > i) p->sel--;
    pb_fix_after_change(self);
}
static void pb_remove_title(id self, SEL c, id t) { pb_remove_index(self, c, pb_index_title(self, 0, t)); }
static void pb_remove_all(id self, SEL c) { (void)c; MSG0(void, pb(self)->menu, "removeAllItems"); pb(self)->sel = -1; redraw(self); }
static i64 pb_num(id self, SEL c) { (void)c; return pb_count(self); }
static id pb_items(id self, SEL c) { (void)c; return MSG0(id, pb(self)->menu, "itemArray"); }
static id pb_item_at(id self, SEL c, i64 i) { (void)c; return pb_item(self, i); }
static id pb_title_at(id self, SEL c, i64 i) { (void)c; id it = pb_item(self, i); return it ? MSG0(id, it, "title") : cx_str(""); }
static id pb_titles(id self, SEL c) {
    (void)c;
    id a = MSG0(id, (id)kls("NSMutableArray"), "array");
    i64 n = pb_count(self);
    for (i64 i = 0; i < n; i++) MSG(void, a, "addObject:", MSG0(id, pb_item(self, i), "title"));
    return a;
}
static id pb_item_title(id self, SEL c, id t) { (void)c; return pb_item(self, pb_index_title(self, 0, t)); }
static id pb_last(id self, SEL c) { (void)c; return pb_item(self, pb_count(self) - 1); }
static i64 pb_index_of(id self, SEL c, id it) { (void)c; return it ? MSG(i64, pb(self)->menu, "indexOfItem:", it) : -1; }
static i64 pb_index_tag(id self, SEL c, i64 tag) {
    (void)c;
    i64 n = pb_count(self);
    for (i64 i = 0; i < n; i++) if (MSG0(i64, pb_item(self, i), "tag") == tag) return i;
    return -1;
}
static i64 pb_index_rep(id self, SEL c, id o) {
    (void)c;
    i64 n = pb_count(self);
    for (i64 i = 0; i < n; i++) {
        id r = MSG0(id, pb_item(self, i), "representedObject");
        if (r == o || (r && o && MSG(BOOL, r, "isEqual:", o))) return i;
    }
    return -1;
}
static i64 pb_index_target_action(id self, SEL c, id t, SEL a) {
    (void)c;
    i64 n = pb_count(self);
    for (i64 i = 0; i < n; i++) {
        id it = pb_item(self, i);
        if (MSG0(id, it, "target") == t && (!a || MSG0(SEL, it, "action") == a)) return i;
    }
    return -1;
}
static id pb_selected(id self, SEL c) { (void)c; return pb_item(self, pb(self)->sel); }
static i64 pb_sel_index(id self, SEL c) { (void)c; return pb(self)->sel; }
static void pb_select(id self, SEL c, id it) { pb_select_index(self, c, pb_index_of(self, 0, it)); }
static BOOL pb_select_tag(id self, SEL c, i64 tag) { i64 i = pb_index_tag(self, 0, tag); if (i < 0) return NO; pb_select_index(self, c, i); return YES; }
static void pb_select_title(id self, SEL c, id t) { pb_select_index(self, c, pb_index_title(self, 0, t)); }
static id pb_sel_title(id self, SEL c) { (void)c; id it = pb_item(self, pb(self)->sel); return it ? MSG0(id, it, "title") : cx_str(""); }
static i64 pb_sel_tag(id self, SEL c) { (void)c; id it = pb_item(self, pb(self)->sel); return it ? MSG0(i64, it, "tag") : -1; }
static id pb_title(id self, SEL c) {
    (void)c;
    pstate_t *p = pb(self);
    id it = pb_item(self, p->pulls ? 0 : p->sel);
    return it ? MSG0(id, it, "title") : cx_str("");
}
/* setTitle: no pop-up seleciona o item com o título (cria se não há); no pull-down troca o título do 1º item */
static void pb_set_title(id self, SEL c, id t) {
    pstate_t *p = pb(self);
    if (p->pulls) {
        if (!pb_count(self)) MSG(void, p->menu, "addItem:", new_item(t));
        else MSG(void, pb_item(self, 0), "setTitle:", t ? t : cx_str(""));
        redraw(self);
        return;
    }
    i64 i = pb_index_title(self, 0, t);
    if (i < 0) { pb_add_title(self, c, t); i = pb_index_title(self, 0, t); }
    pb_select_index(self, c, i);
}
static void pb_sync(id self, SEL c) { (void)c; pb_apply_state(self); redraw(self); }
static id pb_object_value(id self, SEL c) { (void)c; return cx_num_i(pb(self)->sel); }
static void pb_set_object_value(id self, SEL c, id v) { pb_select_index(self, c, v ? MSG0(i64, v, "longLongValue") : -1); }
static i64 pb_int_value(id self, SEL c) { (void)c; return pb(self)->sel; }
static void pb_set_int_value(id self, SEL c, i64 v) { pb_select_index(self, c, v); }
static int pb_int32_value(id self, SEL c) { (void)c; return (int)pb(self)->sel; }
static void pb_set_int32_value(id self, SEL c, int v) { pb_select_index(self, c, v); }
static id pb_string_value(id self, SEL c) { return pb_sel_title(self, c); }

static void pb_run(id self) {
    if (!enabled(self)) return;
    pstate_t *p = pb(self);
    i64 n = pb_count(self);
    if (!n) return;
    post("NSPopUpButtonWillPopUpNotification", self);
    R b = msg_rect(self, "bounds");
    BOOL flipped = MSG0(BOOL, self, "isFlipped");
    double fs = MSG0(double, ctl_font(self), "pointSize");
    id chosen = 0;
    if (p->pulls) {      /* pull-down: o 1º item é o título; o menu abre embaixo do botão */
        id first = pb_item(self, 0);
        BOOL was_hidden = MSG0(BOOL, first, "isHidden");
        MSG(void, first, "setHidden:", (BOOL)YES);
        P sp = view_to_screen(self, (P){b.x + 8, flipped ? b.y + b.h : b.y});
        chosen = cx_menu_popup_choose(p->menu, -1, sp, MP_PLACE_CONTEXT, b.w, fs, self);
        MSG(void, first, "setHidden:", was_hidden);
    } else {             /* pop-up: o item escolhido fica por cima do botão */
        P sp = view_to_screen(self, (P){b.x + 10, flipped ? b.y : b.y + b.h});
        chosen = cx_menu_popup_choose(p->menu, (int)(p->sel >= 0 ? p->sel : 0), sp, MP_PLACE_POSITION, b.w, fs, self);
    }
    if (!chosen) return;
    i64 i = pb_index_of(self, 0, chosen);
    if (!p->pulls) pb_select_index(self, 0, i);
    else redraw(self);
    /* o item com ação própria manda a dele; senão vale a do botão */
    if (MSG0(SEL, chosen, "action")) cx_menu_perform(chosen, self);
    else send_own_action(self);
}
static void pb_mouse_down(id self, SEL c, id e) { (void)c; (void)e; pb_run(self); }
static void pb_perform_click(id self, SEL c, id s) { (void)c; (void)s; pb_run(self); }
static void pb_key_down(id self, SEL c, id e) {
    u16 k = MSG0(u16, e, "keyCode");
    if (k == 49 || k == 125 || k == 126 || k == 36) { pb_run(self); return; }
    IMP sup = class_getMethodImplementation(kls("NSControl"), c);
    if (sup) ((void (*)(id, SEL, id))sup)(self, c, e);
}
static BOOL pb_accepts_first(id self, SEL c) { (void)c; return enabled(self); }

static void pb_draw(id self, SEL c, R dirty) {
    (void)c; (void)dirty;
    R b = msg_rect(self, "bounds");
    BOOL on = enabled(self);
    double h = b.h > 26 ? 22 : b.h - 2, y = b.y + (b.h - h) / 2;
    R box = {b.x + 1, y, b.w - 2, h};
    fill(box, 1, 1, 1, 1);
    MSG0(void, cx_color(0, 0, 0, 0.22), "setFill");
    NSFrameRect(box);
    pstate_t *p = pb(self);
    id font = ctl_font(self);
    id a = attrs(font, 0.1, 0.1, 0.1, on ? 1 : 0.4);
    id t = pb_title(self, 0);
    S ts = m_size(t, a);
    BOOL flipped = MSG0(BOOL, self, "isFlipped");
    double ty = flipped ? y + (h - ts.h) / 2 : y + (h - ts.h) / 2;
    /* corta o título antes das setas */
    double room = box.w - 30;
    if (ts.w > room) {
        const char *s = cx_cstr(t);
        size_t n = strlen(s);
        char buf[512];
        if (n > 500) n = 500;
        while (n > 0) {
            n--;
            while (n > 0 && ((unsigned char)s[n] & 0xC0) == 0x80) n--;
            memcpy(buf, s, n); memcpy(buf + n, "\xE2\x80\xA6", 4);
            t = cx_str(buf);
            if (m_size(t, a).w <= room) break;
        }
    }
    m_draw_at(t, (P){box.x + 8, ty}, a);
    /* setas: ⌃⌄ no pop-up, ⌄ no pull-down */
    fill((R){box.x + box.w - 20, box.y + 3, 17, box.h - 6}, 0.0, 0.40, 0.86, on ? 1 : 0.35);
    id aw = attrs(MSG(id, (id)kls("NSFont"), "systemFontOfSize:", 9.0), 1, 1, 1, 1);
    if (p->pulls) m_draw_at(cx_str("\xE2\x96\xBE"), (P){box.x + box.w - 16, box.y + (box.h - 12) / 2}, aw);
    else {
        m_draw_at(cx_str("\xE2\x96\xB4"), (P){box.x + box.w - 16, flipped ? box.y + 2 : box.y + box.h / 2 - 1}, aw);
        m_draw_at(cx_str("\xE2\x96\xBE"), (P){box.x + box.w - 16, flipped ? box.y + box.h / 2 - 1 : box.y + 2}, aw);
    }
}
static S pb_intrinsic(id self, SEL c) {
    (void)c;
    id a = attrs(ctl_font(self), 0, 0, 0, 1);
    double w = 0;
    i64 n = pb_count(self);
    for (i64 i = 0; i < n; i++) { double tw = m_size(MSG0(id, pb_item(self, i), "title"), a).w; if (tw > w) w = tw; }
    return (S){w + 8 + 30, 22};
}
static void pb_size_to_fit(id self, SEL c) {
    S z = pb_intrinsic(self, c);
    R f = msg_rect(self, "frame");
    ((void (*)(id, SEL, S))objc_msgSend)(self, sl("setFrameSize:"), (S){z.w, f.h > 0 ? f.h : z.h});
}

/* ================================================================ NSSegmentedControl */
typedef struct sstate {
    struct sstate *next;
    id obj;
    sg_t g;
    id label[SG_MAX], image[SG_MAX], menu[SG_MAX], tip[SG_MAX];
    i64 tag[SG_MAX], align[SG_MAX];
    i64 style;
    BOOL show_menu_ind[SG_MAX];
    id bezel_color;
} sstate_t;
static sstate_t *g_sg;
static sstate_t *sgs(id o) {
    for (sstate_t *p = g_sg; p; p = p->next) if (p->obj == o) return p;
    sstate_t *p = calloc(1, sizeof *p);
    if (!p) return 0;
    p->obj = o;
    sg_init(&p->g);
    p->next = g_sg; g_sg = p;
    return p;
}
static void sg_forget(id o) {
    for (sstate_t **pp = &g_sg; *pp; pp = &(*pp)->next) if ((*pp)->obj == o) {
        sstate_t *p = *pp; *pp = p->next;
        for (int i = 0; i < SG_MAX; i++) { objc_release(p->label[i]); objc_release(p->image[i]); objc_release(p->menu[i]); objc_release(p->tip[i]); }
        objc_release(p->bezel_color);
        free(p);
        return;
    }
}
static int seg_ok(sstate_t *p, i64 i) { return p && i >= 0 && i < p->g.n; }
static void keepi(id *slot, id v) { objc_retain(v); objc_release(*slot); *slot = v; }
static void sg_measure(id self) {
    sstate_t *p = sgs(self);
    id a = attrs(ctl_font(self), 0, 0, 0, 1);
    for (int i = 0; i < p->g.n; i++) {
        double w = p->label[i] ? m_size(p->label[i], a).w : 0;
        if (p->image[i]) { S is = ((S (*)(id, SEL))objc_msgSend)(p->image[i], sl("size")); w += (w > 0 ? 4 : 0) + (is.w > 0 ? is.w : 16); }
        if (p->menu[i] && p->show_menu_ind[i]) w += 10;
        p->g.natural[i] = w;
    }
}
static void sg_relayout(id self) {
    sstate_t *p = sgs(self);
    sg_measure(self);
    R b = msg_rect(self, "bounds");
    sg_layout(&p->g, b.w);
    redraw(self);
}
static id sg_init_frame(id self, SEL c, R f) {
    (void)c;
    self = control_init(self, f);
    if (self) sgs(self);
    return self;
}
static void sg_dealloc(id self, SEL c) {
    sg_forget(self);
    IMP sup = class_getMethodImplementation(kls("NSControl"), c);
    if (sup) ((void (*)(id, SEL))sup)(self, c);
}
static i64 sg_count(id self, SEL c) { (void)c; return sgs(self)->g.n; }
static void sg_set_count_m(id self, SEL c, i64 n) {
    (void)c;
    sstate_t *p = sgs(self);
    for (i64 i = n; i < p->g.n; i++) { keepi(&p->label[i], 0); keepi(&p->image[i], 0); keepi(&p->menu[i], 0); keepi(&p->tip[i], 0); }
    sg_set_count(&p->g, (int)n);
    sg_relayout(self);
}
static void sg_set_label(id self, SEL c, id l, i64 i) { (void)c; sstate_t *p = sgs(self); if (!seg_ok(p, i)) return; keepi(&p->label[i], l ? MSG0(id, l, "copy") : 0); objc_release(p->label[i]); sg_relayout(self); }
static id sg_label(id self, SEL c, i64 i) { (void)c; sstate_t *p = sgs(self); return seg_ok(p, i) ? p->label[i] : 0; }
static void sg_set_image(id self, SEL c, id img, i64 i) { (void)c; sstate_t *p = sgs(self); if (!seg_ok(p, i)) return; keepi(&p->image[i], img); sg_relayout(self); }
static id sg_image(id self, SEL c, i64 i) { (void)c; sstate_t *p = sgs(self); return seg_ok(p, i) ? p->image[i] : 0; }
static void sg_set_menu(id self, SEL c, id m, i64 i) { (void)c; sstate_t *p = sgs(self); if (!seg_ok(p, i)) return; keepi(&p->menu[i], m); sg_relayout(self); }
static id sg_menu(id self, SEL c, i64 i) { (void)c; sstate_t *p = sgs(self); return seg_ok(p, i) ? p->menu[i] : 0; }
static void sg_set_tip(id self, SEL c, id t, i64 i) { (void)c; sstate_t *p = sgs(self); if (seg_ok(p, i)) keepi(&p->tip[i], t); }
static id sg_tip(id self, SEL c, i64 i) { (void)c; sstate_t *p = sgs(self); return seg_ok(p, i) ? p->tip[i] : 0; }
static void sg_set_tag(id self, SEL c, i64 t, i64 i) { (void)c; sstate_t *p = sgs(self); if (seg_ok(p, i)) p->tag[i] = t; }
static i64 sg_tag(id self, SEL c, i64 i) { (void)c; sstate_t *p = sgs(self); return seg_ok(p, i) ? p->tag[i] : 0; }
static void sg_set_width(id self, SEL c, double w, i64 i) { (void)c; sstate_t *p = sgs(self); if (!seg_ok(p, i)) return; p->g.fixed[i] = w; sg_relayout(self); }
static double sg_width(id self, SEL c, i64 i) { (void)c; sstate_t *p = sgs(self); return seg_ok(p, i) ? p->g.fixed[i] : 0; }
static void sg_set_enabled(id self, SEL c, BOOL on, i64 i) { (void)c; sstate_t *p = sgs(self); if (!seg_ok(p, i)) return; p->g.enabled[i] = on; redraw(self); }
static BOOL sg_is_enabled(id self, SEL c, i64 i) { (void)c; sstate_t *p = sgs(self); return seg_ok(p, i) && p->g.enabled[i]; }
static void sg_set_sel(id self, SEL c, BOOL on, i64 i) { (void)c; sstate_t *p = sgs(self); if (!seg_ok(p, i)) return; sg_set_selected(&p->g, (int)i, on); redraw(self); }
static BOOL sg_is_sel(id self, SEL c, i64 i) {
    (void)c; sstate_t *p = sgs(self);
    if (!seg_ok(p, i)) return NO;
    if (p->g.tracking == SG_SELECT_ANY) return p->g.selected[i];
    return p->g.selected_one == i && p->g.tracking == SG_SELECT_ONE ? YES : p->g.selected[i];
}
static i64 sg_selected(id self, SEL c) { (void)c; return sg_selected_segment(&sgs(self)->g); }
static void sg_set_selected_m(id self, SEL c, i64 i) {
    (void)c; sstate_t *p = sgs(self);
    if (i < 0) { for (int k = 0; k < p->g.n; k++) p->g.selected[k] = 0; p->g.selected_one = -1; }
    else sg_set_selected(&p->g, (int)i, 1);
    redraw(self);
}
static BOOL sg_select_tag(id self, SEL c, i64 tag) {
    sstate_t *p = sgs(self);
    for (int i = 0; i < p->g.n; i++) if (p->tag[i] == tag) { sg_set_selected_m(self, c, i); return YES; }
    return NO;
}
static i64 sg_tracking(id self, SEL c) { (void)c; return sgs(self)->g.tracking; }
static void sg_set_tracking(id self, SEL c, i64 t) { (void)c; sgs(self)->g.tracking = (int)t; redraw(self); }
static i64 sg_distribution(id self, SEL c) { (void)c; return sgs(self)->g.distribution; }
static void sg_set_distribution(id self, SEL c, i64 d) { (void)c; sgs(self)->g.distribution = (int)d; sg_relayout(self); }
static i64 sg_style(id self, SEL c) { (void)c; return sgs(self)->style; }
static void sg_set_style(id self, SEL c, i64 s) { (void)c; sgs(self)->style = s; redraw(self); }
static void sg_set_align(id self, SEL c, i64 a, i64 i) { (void)c; sstate_t *p = sgs(self); if (seg_ok(p, i)) p->align[i] = a; redraw(self); }
static i64 sg_align(id self, SEL c, i64 i) { (void)c; sstate_t *p = sgs(self); return seg_ok(p, i) ? p->align[i] : 2; }
static void sg_set_menu_ind(id self, SEL c, BOOL b, i64 i) { (void)c; sstate_t *p = sgs(self); if (seg_ok(p, i)) p->show_menu_ind[i] = b; sg_relayout(self); }
static BOOL sg_menu_ind(id self, SEL c, i64 i) { (void)c; sstate_t *p = sgs(self); return seg_ok(p, i) && p->show_menu_ind[i]; }
static id sg_bezel_color(id self, SEL c) { (void)c; return sgs(self)->bezel_color; }
static void sg_set_bezel_color(id self, SEL c, id col) { (void)c; keepi(&sgs(self)->bezel_color, col); redraw(self); }
static double sg_double_value(id self, SEL c) { (void)c; return (double)sg_selected_segment(&sgs(self)->g); }
static S sg_intrinsic(id self, SEL c) { (void)c; sg_measure(self); return (S){sg_intrinsic_width(&sgs(self)->g), 24}; }
static void sg_set_frame_size(id self, SEL c, S z) {
    IMP sup = class_getMethodImplementation(kls("NSControl"), c);
    if (sup) ((void (*)(id, SEL, S))sup)(self, c, z);
    sg_relayout(self);
}
static void sg_set_frame(id self, SEL c, R f) {
    IMP sup = class_getMethodImplementation(kls("NSControl"), c);
    if (sup) ((void (*)(id, SEL, R))sup)(self, c, f);
    sg_relayout(self);
}
static void sg_size_to_fit(id self, SEL c) {
    (void)c;
    S z = sg_intrinsic(self, 0);
    ((void (*)(id, SEL, S))objc_msgSend)(self, sl("setFrameSize:"), z);
}

static void sg_draw(id self, SEL c, R dirty) {
    (void)c; (void)dirty;
    sstate_t *p = sgs(self);
    R b = msg_rect(self, "bounds");
    sg_measure(self);
    sg_layout(&p->g, b.w);
    BOOL on = enabled(self);
    double h = b.h > 26 ? 24 : b.h - 1, y = b.y + (b.h - h) / 2;
    fill((R){b.x, y, p->g.n ? p->g.x[p->g.n - 1] + p->g.w[p->g.n - 1] : b.w, h}, 0.92, 0.92, 0.93, 1);
    id font = ctl_font(self);
    for (int i = 0; i < p->g.n; i++) {
        R r = {b.x + p->g.x[i], y, p->g.w[i], h};
        BOOL sel = (p->g.tracking == SG_SELECT_ANY) ? p->g.selected[i] : (p->g.tracking == SG_SELECT_ONE ? p->g.selected_one == i : p->g.pressed == i);
        BOOL en = on && p->g.enabled[i];
        if (sel) {
            if (p->bezel_color) { MSG0(void, p->bezel_color, "setFill"); NSRectFill((R){r.x + 1, r.y + 1, r.w - 2, r.h - 2}); }
            else if (p->g.tracking == SG_SELECT_ONE) fill((R){r.x + 1, r.y + 1, r.w - 2, r.h - 2}, 1, 1, 1, 1);
            else fill((R){r.x + 1, r.y + 1, r.w - 2, r.h - 2}, 0.0, 0.40, 0.86, 1);
        }
        if (i > 0) fill((R){r.x, r.y + 4, 1, r.h - 8}, 0, 0, 0, 0.15);
        BOOL white = sel && p->g.tracking != SG_SELECT_ONE && !p->bezel_color;
        id a = attrs(font, white ? 1 : 0.1, white ? 1 : 0.1, white ? 1 : 0.1, en ? 1 : 0.35);
        double cw = 0, tw = 0, iw = 0;
        S ts = {0, 0};
        if (p->label[i]) { ts = m_size(p->label[i], a); tw = ts.w; }
        if (p->image[i]) { S is = ((S (*)(id, SEL))objc_msgSend)(p->image[i], sl("size")); iw = is.w > 0 ? (is.w > r.w - 8 ? r.w - 8 : is.w) : 16; }
        cw = tw + iw + (tw > 0 && iw > 0 ? 4 : 0);
        double x = r.x + (r.w - cw) / 2;
        if (p->image[i]) {
            double ih = iw > h - 6 ? h - 6 : iw;
            ((void (*)(id, SEL, R, R, u64, double))objc_msgSend)(p->image[i], sl("drawInRect:fromRect:operation:fraction:"),
                (R){x, r.y + (h - ih) / 2, iw, ih}, (R){0, 0, 0, 0}, (u64)2, en ? 1.0 : 0.4);
            x += iw + 4;
        }
        if (p->label[i]) m_draw_at(p->label[i], (P){x, r.y + (h - ts.h) / 2}, a);
        if (p->menu[i] && p->show_menu_ind[i]) m_draw_at(cx_str("\xE2\x96\xBE"), (P){r.x + r.w - 11, r.y + (h - ts.h) / 2}, a);
    }
    MSG0(void, cx_color(0, 0, 0, 0.2), "setFill");
    if (p->g.n) NSFrameRect((R){b.x, y, p->g.x[p->g.n - 1] + p->g.w[p->g.n - 1], h});
}

static void sg_mouse_down(id self, SEL c, id e) {
    (void)c;
    if (!enabled(self)) return;
    sstate_t *p = sgs(self);
    P pt = m_loc(self, e);
    R b = msg_rect(self, "bounds");
    sg_measure(self);
    sg_layout(&p->g, b.w);
    int i = sg_hit(&p->g, pt.x - b.x);
    if (i < 0 || !p->g.enabled[i]) return;
    BOOL has_action = responds(self, "action") && MSG0(SEL, self, "action");
    if (p->menu[i] && (!has_action || p->show_menu_ind[i] || !p->label[i])) {   /* segmento-menu: abre embaixo dele */
        BOOL flipped = MSG0(BOOL, self, "isFlipped");
        P sp = view_to_screen(self, (P){b.x + p->g.x[i], flipped ? b.y + b.h : b.y});
        id ch = cx_menu_popup_choose(p->menu[i], -1, sp, MP_PLACE_CONTEXT, p->g.w[i], 0, self);
        if (ch) cx_menu_perform(ch, self);
        return;
    }
    p->g.pressed = i;
    int changed = sg_click(&p->g, i);
    redraw(self);
    if (changed) send_own_action(self);
    if (p->g.tracking == SG_MOMENTARY || p->g.tracking == SG_MOMENTARY_ACCEL) p->g.selected_one = i;   /* selectedSegment = o último clicado */
    p->g.pressed = -1;
    redraw(self);
}

static id sg_make(id k, id labels, id images, i64 tracking, id target, SEL action) {
    id s = ((id (*)(id, SEL, R))objc_msgSend)(MSG0(id, k, "alloc"), sl("initWithFrame:"), (R){0, 0, 100, 24});
    u64 n = labels ? MSG0(u64, labels, "count") : images ? MSG0(u64, images, "count") : 0;
    sg_set_count_m(s, 0, (i64)n);
    for (u64 i = 0; i < n; i++) {
        if (labels) sg_set_label(s, 0, MSG(id, labels, "objectAtIndex:", i), (i64)i);
        if (images) sg_set_image(s, 0, MSG(id, images, "objectAtIndex:", i), (i64)i);
    }
    sg_set_tracking(s, 0, tracking);
    MSG(void, s, "setTarget:", target);
    MSG(void, s, "setAction:", action);
    sg_size_to_fit(s, 0);
    return objc_autorelease(s);
}
static id sg_with_labels(id k, SEL c, id labels, i64 t, id target, SEL a) { (void)c; return sg_make(k, labels, 0, t, target, a); }
static id sg_with_images(id k, SEL c, id images, i64 t, id target, SEL a) { (void)c; return sg_make(k, 0, images, t, target, a); }

/* ================================================================ registro */
#define ADD(cls, sel, fn) cx_add_method(cls, sel, (void *)(fn))
void cx_appkit_controls_init(void) {
    cx_class_t *pbc = kls("NSPopUpButton"), *sgc = kls("NSSegmentedControl");
    if (pbc) {
        ADD(pbc, "initWithFrame:", pb_init_frame); ADD(pbc, "initWithFrame:pullsDown:", pb_init_pulls); ADD(pbc, "dealloc", pb_dealloc);
        ADD(pbc, "menu", pb_menu); ADD(pbc, "setMenu:", pb_set_menu); ADD(pbc, "pullsDown", pb_pulls); ADD(pbc, "setPullsDown:", pb_set_pulls);
        ADD(pbc, "autoenablesItems", pb_autoen); ADD(pbc, "setAutoenablesItems:", pb_set_autoen);
        ADD(pbc, "preferredEdge", pb_edge); ADD(pbc, "setPreferredEdge:", pb_set_edge);
        ADD(pbc, "altersStateOfSelectedItem", pb_alters); ADD(pbc, "setAltersStateOfSelectedItem:", pb_set_alters);
        ADD(pbc, "usesItemFromMenu", pb_uses_item); ADD(pbc, "setUsesItemFromMenu:", pb_set_uses_item);
        ADD(pbc, "addItemWithTitle:", pb_add_title); ADD(pbc, "addItemsWithTitles:", pb_add_titles);
        ADD(pbc, "insertItemWithTitle:atIndex:", pb_insert_title); ADD(pbc, "removeItemWithTitle:", pb_remove_title);
        ADD(pbc, "removeItemAtIndex:", pb_remove_index); ADD(pbc, "removeAllItems", pb_remove_all);
        ADD(pbc, "numberOfItems", pb_num); ADD(pbc, "itemArray", pb_items); ADD(pbc, "itemAtIndex:", pb_item_at);
        ADD(pbc, "itemTitleAtIndex:", pb_title_at); ADD(pbc, "itemTitles", pb_titles); ADD(pbc, "itemWithTitle:", pb_item_title);
        ADD(pbc, "lastItem", pb_last); ADD(pbc, "indexOfItem:", pb_index_of); ADD(pbc, "indexOfItemWithTag:", pb_index_tag);
        ADD(pbc, "indexOfItemWithTitle:", pb_index_title); ADD(pbc, "indexOfItemWithRepresentedObject:", pb_index_rep);
        ADD(pbc, "indexOfItemWithTarget:andAction:", pb_index_target_action);
        ADD(pbc, "selectedItem", pb_selected); ADD(pbc, "indexOfSelectedItem", pb_sel_index); ADD(pbc, "selectItem:", pb_select);
        ADD(pbc, "selectItemAtIndex:", pb_select_index); ADD(pbc, "selectItemWithTag:", pb_select_tag); ADD(pbc, "selectItemWithTitle:", pb_select_title);
        ADD(pbc, "titleOfSelectedItem", pb_sel_title); ADD(pbc, "selectedTag", pb_sel_tag); ADD(pbc, "title", pb_title); ADD(pbc, "setTitle:", pb_set_title);
        ADD(pbc, "synchronizeTitleAndSelectedItem", pb_sync);
        ADD(pbc, "objectValue", pb_object_value); ADD(pbc, "setObjectValue:", pb_set_object_value);
        ADD(pbc, "integerValue", pb_int_value); ADD(pbc, "setIntegerValue:", pb_set_int_value);
        ADD(pbc, "intValue", pb_int32_value); ADD(pbc, "setIntValue:", pb_set_int32_value); ADD(pbc, "stringValue", pb_string_value);
        ADD(pbc, "mouseDown:", pb_mouse_down); ADD(pbc, "performClick:", pb_perform_click); ADD(pbc, "keyDown:", pb_key_down);
        ADD(pbc, "acceptsFirstResponder", pb_accepts_first); ADD(pbc, "drawRect:", pb_draw);
        ADD(pbc, "intrinsicContentSize", pb_intrinsic); ADD(pbc, "sizeToFit", pb_size_to_fit);
    }
    if (sgc) {
        ADD(sgc, "initWithFrame:", sg_init_frame); ADD(sgc, "dealloc", sg_dealloc);
        cx_add_cmethod(sgc, "segmentedControlWithLabels:trackingMode:target:action:", (void *)sg_with_labels);
        cx_add_cmethod(sgc, "segmentedControlWithImages:trackingMode:target:action:", (void *)sg_with_images);
        ADD(sgc, "segmentCount", sg_count); ADD(sgc, "setSegmentCount:", sg_set_count_m);
        ADD(sgc, "setLabel:forSegment:", sg_set_label); ADD(sgc, "labelForSegment:", sg_label);
        ADD(sgc, "setImage:forSegment:", sg_set_image); ADD(sgc, "imageForSegment:", sg_image);
        ADD(sgc, "setMenu:forSegment:", sg_set_menu); ADD(sgc, "menuForSegment:", sg_menu);
        ADD(sgc, "setToolTip:forSegment:", sg_set_tip); ADD(sgc, "toolTipForSegment:", sg_tip);
        ADD(sgc, "setTag:forSegment:", sg_set_tag); ADD(sgc, "tagForSegment:", sg_tag);
        ADD(sgc, "setWidth:forSegment:", sg_set_width); ADD(sgc, "widthForSegment:", sg_width);
        ADD(sgc, "setEnabled:forSegment:", sg_set_enabled); ADD(sgc, "isEnabledForSegment:", sg_is_enabled);
        ADD(sgc, "setSelected:forSegment:", sg_set_sel); ADD(sgc, "isSelectedForSegment:", sg_is_sel);
        ADD(sgc, "selectedSegment", sg_selected); ADD(sgc, "setSelectedSegment:", sg_set_selected_m); ADD(sgc, "selectSegmentWithTag:", sg_select_tag);
        ADD(sgc, "trackingMode", sg_tracking); ADD(sgc, "setTrackingMode:", sg_set_tracking);
        ADD(sgc, "segmentDistribution", sg_distribution); ADD(sgc, "setSegmentDistribution:", sg_set_distribution);
        ADD(sgc, "segmentStyle", sg_style); ADD(sgc, "setSegmentStyle:", sg_set_style);
        ADD(sgc, "setAlignment:forSegment:", sg_set_align); ADD(sgc, "alignmentForSegment:", sg_align);
        ADD(sgc, "setShowsMenuIndicator:forSegment:", sg_set_menu_ind); ADD(sgc, "showsMenuIndicatorForSegment:", sg_menu_ind);
        ADD(sgc, "selectedSegmentBezelColor", sg_bezel_color); ADD(sgc, "setSelectedSegmentBezelColor:", sg_set_bezel_color);
        ADD(sgc, "doubleValueForSelectedSegment", sg_double_value);
        ADD(sgc, "intrinsicContentSize", sg_intrinsic); ADD(sgc, "sizeToFit", sg_size_to_fit);
        ADD(sgc, "setFrameSize:", sg_set_frame_size); ADD(sgc, "setFrame:", sg_set_frame);
        ADD(sgc, "drawRect:", sg_draw); ADD(sgc, "mouseDown:", sg_mouse_down);
    }
}
