/*
 * mOS libSystem — abas de janela do AppKit (window tabbing):
 *   NSWindowTabGroup (windows, selectedWindow, isTabBarVisible, add/insert/removeWindow:), NSWindowTab (título, dica,
 *   view acessória), e na NSWindow: tabbingMode, tabbingIdentifier, addTabbedWindow:ordered:, tabbedWindows, tabGroup,
 *   selectNextTab:, selectPreviousTab:, mergeAllWindows:, moveTabToNewWindow:, toggleTabBar:, newWindowForTab: pela
 *   cadeia de responders (o botão + da barra).
 *   As janelas de um grupo dividem a moldura; só a escolhida fica na tela. A barra de abas é uma janela filha sem moldura
 *   presa no alto do conteúdo da escolhida (GUI_FOCUS_OWNER e GUI_FOCUS_FRAME do window server): um clique escolhe a
 *   aba, o × fecha e o + pede uma janela nova.
 * Registro: cx_appkit_tabs_init(), chamado do cx_appkit_input_init.
 */
#include "foundation.h"
#include "sys.h"
#include "libc.h"

id objc_msgSend_stret(id self, SEL op, ...);
id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void objc_setAssociatedObject(id obj, const void *key, id value, u64 policy);
id objc_getAssociatedObject(id obj, const void *key);
id cx_color(double r, double g, double b, double a);
extern id NSApp;
extern id NSFontAttributeName, NSForegroundColorAttributeName;

typedef struct { double x, y; } P;
typedef struct { double w, h; } S;
typedef struct { double x, y, w, h; } R;
void NSRectFill(R r);

#define K(N) cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N); cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N);
K(NSWindowTabGroup) K(_MOSTabBarView)

static cx_class_t *kls(const char *n) { return cx_class_named(n); }
static SEL sl(const char *n) { return sel_registerName(n); }
static BOOL responds(id o, const char *s) { return o && cx_responds(object_getClass(o), sl(s)); }
static R msg_rect(id o, const char *s) { return ((R (*)(id, SEL))objc_msgSend_stret)(o, sl(s)); }
static R mkr(double x, double y, double w, double h) { R r = {x, y, w, h}; return r; }
static char k_group, k_bar, k_mode, k_ident, k_windows, k_selected, k_barwin_owner;
#define BAR_H 26.0

static id group_of(id w) { return objc_getAssociatedObject(w, &k_group); }
static id group_windows(id g) { id a = objc_getAssociatedObject(g, &k_windows); if (!a) { a = MSG0(id, (id)kls("NSMutableArray"), "array"); objc_setAssociatedObject(g, &k_windows, a, 1); } return a; }
static id group_selected(id g) { return objc_getAssociatedObject(g, &k_selected); }
static void layout_bar(id g);
static id ensure_group(id w) {
    id g = group_of(w);
    if (g) return g;
    g = objc_autorelease(cx_alloc_instance(&NSWindowTabGroup_c));
    MSG(void, group_windows(g), "addObject:", w);
    objc_setAssociatedObject(g, &k_selected, w, 0);
    objc_setAssociatedObject(w, &k_group, g, 1);
    return g;
}
static void show_only_selected(id g) {
    id ws = group_windows(g), sel = group_selected(g);
    for (u64 i = 0, n = MSG0(u64, ws, "count"); i < n; i++) {
        id w = MSG(id, ws, "objectAtIndex:", i);
        if (w != sel) MSG(void, w, "orderOut:", (id)0);
    }
    if (sel) MSG(void, sel, "makeKeyAndOrderFront:", (id)0);
    layout_bar(g);
}
static void select_window(id g, id w) {
    id old = group_selected(g);
    if (old && w && old != w) {   /* a nova aba herda a moldura da anterior */
        R f = msg_rect(old, "frame");
        MSG(void, w, "setFrame:display:", f, (BOOL)YES);
    }
    objc_setAssociatedObject(g, &k_selected, w, 0);
    show_only_selected(g);
}

/* ---- NSWindowTab: título (o da janela quando vazio), título rico, dica e view acessória (fica na aba, à direita) */
static char k_tab_title, k_tab_attr, k_tab_tip, k_tab_acc;
static id tab_window_of(id tab) { return tab && responds(tab, "window") ? MSG0(id, tab, "window") : 0; }
static void tab_changed(id tab) {
    id w = tab_window_of(tab), g = w ? group_of(w) : 0;
    if (g) layout_bar(g);
}
static id tab_get_title(id self, SEL c) { (void)c; id t = objc_getAssociatedObject(self, &k_tab_title); return t ? t : cx_str(""); }
static void tab_set_title(id self, SEL c, id t) { (void)c; objc_setAssociatedObject(self, &k_tab_title, t ? MSG0(id, t, "copy") : 0, 1); tab_changed(self); }
static id tab_get_attr(id self, SEL c) { (void)c; return objc_getAssociatedObject(self, &k_tab_attr); }
static void tab_set_attr(id self, SEL c, id t) {
    (void)c;
    objc_setAssociatedObject(self, &k_tab_attr, t, 1);
    if (t && responds(t, "string")) objc_setAssociatedObject(self, &k_tab_title, MSG0(id, MSG0(id, t, "string"), "copy"), 1);
    tab_changed(self);
}
static id tab_get_tip(id self, SEL c) { (void)c; return objc_getAssociatedObject(self, &k_tab_tip); }
static void tab_set_tip(id self, SEL c, id t) { (void)c; objc_setAssociatedObject(self, &k_tab_tip, t ? MSG0(id, t, "copy") : 0, 1); }
static id tab_get_acc(id self, SEL c) { (void)c; return objc_getAssociatedObject(self, &k_tab_acc); }
static void tab_set_acc(id self, SEL c, id v) {
    (void)c;
    id old = objc_getAssociatedObject(self, &k_tab_acc);
    if (old == v) return;
    if (old) MSG0(void, old, "removeFromSuperview");
    objc_setAssociatedObject(self, &k_tab_acc, v, 1);
    tab_changed(self);
}

/* ---- barra de abas: janela filha sem moldura com a view que desenha as abas */
static id tab_title(id w) {
    id tab = responds(w, "tab") ? MSG0(id, w, "tab") : 0;
    id t = tab && responds(tab, "title") ? MSG0(id, tab, "title") : 0;
    if (!t || !MSG0(u64, t, "length")) t = MSG0(id, w, "title");
    return t ? t : cx_str("");
}
static void bar_draw(id self, SEL c, R dirty) {
    (void)c; (void)dirty;
    id g = objc_getAssociatedObject(self, &k_barwin_owner);
    R b = msg_rect(self, "bounds");
    MSG0(void, cx_color(0.86, 0.86, 0.88, 1), "setFill"); NSRectFill(b);
    if (!g) return;
    id ws = group_windows(g), sel = group_selected(g);
    u64 n = MSG0(u64, ws, "count");
    double w = n ? (b.w - BAR_H) / (double)n : b.w;
    id a = MSG0(id, (id)kls("NSMutableDictionary"), "dictionary");
    id f12 = MSG(id, (id)kls("NSFont"), "systemFontOfSize:", 12.0);
    MSG(void, a, "setObject:forKey:", f12, NSFontAttributeName);
    for (u64 i = 0; i < n; i++) {
        id win = MSG(id, ws, "objectAtIndex:", i);
        R t = mkr(i * w, 0, w, b.h);
        if (win == sel) { MSG0(void, cx_color(0.96, 0.96, 0.97, 1), "setFill"); NSRectFill(t); }
        MSG0(void, cx_color(0.75, 0.75, 0.77, 1), "setFill"); NSRectFill(mkr(t.x + t.w - 1, 4, 1, b.h - 8));
        id tc = cx_color(0.1, 0.1, 0.12, win == sel ? 1 : 0.6);
        MSG(void, a, "setObject:forKey:", tc, NSForegroundColorAttributeName);
        id title = tab_title(win);
        S z = ((S (*)(id, SEL, id))objc_msgSend)(title, sl("sizeWithAttributes:"), a);
        ((void (*)(id, SEL, P, id))objc_msgSend)(title, sl("drawAtPoint:withAttributes:"), (P){t.x + (t.w - z.w) / 2, (b.h - z.h) / 2}, a);
        ((void (*)(id, SEL, P, id))objc_msgSend)(cx_str("\xC3\x97"), sl("drawAtPoint:withAttributes:"), (P){t.x + 8, (b.h - z.h) / 2}, a);
        id tab = responds(win, "tab") ? MSG0(id, win, "tab") : 0;
        id acc = tab ? objc_getAssociatedObject(tab, &k_tab_acc) : 0;
        if (acc) {   /* view acessória: centrada na altura, encostada à direita da aba */
            R af = msg_rect(acc, "frame");
            double aw = af.w > 0 ? af.w : 16, ah = af.h > 0 && af.h < b.h ? af.h : b.h - 8;
            R nf = mkr(t.x + t.w - aw - 8, (b.h - ah) / 2, aw, ah);
            if (af.x != nf.x || af.y != nf.y || af.w != nf.w || af.h != nf.h) ((void (*)(id, SEL, R))objc_msgSend)(acc, sl("setFrame:"), nf);
            if (MSG0(id, acc, "superview") != self) MSG(void, self, "addSubview:", acc);
        }
    }
    id pc = cx_color(0.1, 0.1, 0.12, 0.8);
    MSG(void, a, "setObject:forKey:", pc, NSForegroundColorAttributeName);
    ((void (*)(id, SEL, P, id))objc_msgSend)(cx_str("+"), sl("drawAtPoint:withAttributes:"), (P){b.w - BAR_H + 9, 4}, a);
}
static BOOL bar_flipped(id self, SEL c) { (void)self; (void)c; return YES; }
static void bar_mouse(id self, SEL c, id ev) {
    (void)c;
    id g = objc_getAssociatedObject(self, &k_barwin_owner);
    if (!g) return;
    P wp = ((P (*)(id, SEL))objc_msgSend)(ev, sl("locationInWindow"));
    P p = ((P (*)(id, SEL, P, id))objc_msgSend)(self, sl("convertPoint:fromView:"), wp, (id)0);
    R b = msg_rect(self, "bounds");
    id ws = group_windows(g);
    u64 n = MSG0(u64, ws, "count");
    if (p.x >= b.w - BAR_H) {   /* + : newWindowForTab: pela cadeia a partir da janela escolhida */
        id sel = group_selected(g);
        MSG(BOOL, NSApp, "sendAction:to:from:", sl("newWindowForTab:"), (id)0, sel);
        return;
    }
    double w = n ? (b.w - BAR_H) / (double)n : b.w;
    u64 i = (u64)(p.x / w);
    if (i >= n) return;
    id win = MSG(id, ws, "objectAtIndex:", i);
    if (p.x - i * w < 22) { MSG(void, win, "performClose:", (id)0); return; }
    select_window(g, win);
}
static void layout_bar(id g) {
    id ws = group_windows(g), sel = group_selected(g);
    id bar = objc_getAssociatedObject(g, &k_bar);
    BOOL want = MSG0(u64, ws, "count") > 1 || (sel && objc_getAssociatedObject(g, &k_mode) != 0);
    if (!want || !sel) { if (bar) { MSG(void, MSG0(id, bar, "parentWindow"), "removeChildWindow:", bar); MSG(void, bar, "orderOut:", (id)0); } return; }
    R f = msg_rect(sel, "frame");
    R cr = ((R (*)(id, SEL, R))objc_msgSend_stret)(sel, sl("contentRectForFrameRect:"), f);
    R bf = mkr(cr.x, cr.y + cr.h - BAR_H, cr.w, BAR_H);
    if (!bar) {
        bar = objc_autorelease(((id (*)(id, SEL, R, u64, u64, BOOL))objc_msgSend)(MSG0(id, (id)kls("NSPanel"), "alloc"), sl("initWithContentRect:styleMask:backing:defer:"), bf, (u64)0, (u64)2, (BOOL)NO));
        id v = objc_autorelease(((id (*)(id, SEL, R))objc_msgSend)(MSG0(id, (id)&_MOSTabBarView_c, "alloc"), sl("initWithFrame:"), mkr(0, 0, bf.w, BAR_H)));
        objc_setAssociatedObject(v, &k_barwin_owner, g, 0);
        MSG(void, bar, "setContentView:", v);
        objc_setAssociatedObject(g, &k_bar, bar, 1);
    }
    MSG(void, bar, "setFrame:display:", bf, (BOOL)YES);
    id parent = MSG0(id, bar, "parentWindow");
    if (parent != sel) { if (parent) MSG(void, parent, "removeChildWindow:", bar); MSG(void, sel, "addChildWindow:ordered:", bar, (i64)1); }
    MSG(void, bar, "orderFront:", (id)0);
    MSG(void, MSG0(id, bar, "contentView"), "setNeedsDisplay:", (BOOL)YES);
}

/* ---- NSWindowTabGroup */
static id tg_windows(id self, SEL c) { (void)c; return MSG0(id, group_windows(self), "copy"); }
static id tg_selected(id self, SEL c) { (void)c; return group_selected(self); }
static void tg_set_selected(id self, SEL c, id w) { (void)c; if (w && MSG(BOOL, group_windows(self), "containsObject:", w)) select_window(self, w); }
static BOOL tg_bar_visible(id self, SEL c) { (void)c; id bar = objc_getAssociatedObject(self, &k_bar); return bar && MSG0(BOOL, bar, "isVisible"); }
static BOOL tg_overview(id self, SEL c) { (void)self; (void)c; return NO; }
static id tg_ident(id self, SEL c) { (void)c; id w = group_selected(self); return w ? objc_getAssociatedObject(w, &k_ident) : 0; }
static void tg_insert(id self, SEL c, id w, u64 at) {
    (void)c;
    if (!w) return;
    id old = group_of(w);
    if (old && old != self) MSG(void, group_windows(old), "removeObject:", w);
    id ws = group_windows(self);
    if (MSG(BOOL, ws, "containsObject:", w)) return;
    if (at > MSG0(u64, ws, "count")) at = MSG0(u64, ws, "count");
    MSG(void, ws, "insertObject:atIndex:", w, at);
    objc_setAssociatedObject(w, &k_group, self, 1);
    select_window(self, w);
}
static void tg_add(id self, SEL c, id w) { tg_insert(self, c, w, MSG0(u64, group_windows(self), "count")); }
static void tg_remove(id self, SEL c, id w) {
    (void)c;
    id ws = group_windows(self);
    u64 i = MSG(u64, ws, "indexOfObject:", w);
    if (i >= MSG0(u64, ws, "count")) return;
    MSG(void, ws, "removeObjectAtIndex:", i);
    objc_setAssociatedObject(w, &k_group, 0, 1);
    if (group_selected(self) == w) {
        u64 n = MSG0(u64, ws, "count");
        id next = n ? MSG(id, ws, "objectAtIndex:", i < n ? i : n - 1) : 0;
        objc_setAssociatedObject(self, &k_selected, next, 0);
        if (next) {
            R f = msg_rect(w, "frame");
            MSG(void, next, "setFrame:display:", f, (BOOL)YES);
        }
    }
    show_only_selected(self);
}

/* ---- NSWindow */
static i64 w_mode(id self, SEL c) { (void)c; id v = objc_getAssociatedObject(self, &k_mode); return v ? MSG0(i64, v, "longLongValue") : 0; }
static void w_set_mode(id self, SEL c, i64 m) { (void)c; objc_setAssociatedObject(self, &k_mode, cx_num_i(m), 1); }
static id w_ident(id self, SEL c) { (void)c; id v = objc_getAssociatedObject(self, &k_ident); return v ? v : cx_str(cx_class_name(object_getClass(self))); }
static void w_set_ident(id self, SEL c, id s) { (void)c; objc_setAssociatedObject(self, &k_ident, s, 1); }
static void w_add_tabbed(id self, SEL c, id w, i64 ordered) {
    (void)c;
    id g = ensure_group(self);
    id ws = group_windows(g);
    u64 at = MSG(u64, ws, "indexOfObject:", self);
    if (at >= MSG0(u64, ws, "count")) at = 0;
    tg_insert(g, 0, w, ordered == -1 ? at : at + 1);   /* NSWindowBelow = -1: antes desta */
}
static id w_tabbed(id self, SEL c) { (void)c; id g = group_of(self); return g && MSG0(u64, group_windows(g), "count") > 1 ? MSG0(id, group_windows(g), "copy") : 0; }
static id w_tab_group(id self, SEL c) { (void)c; return ensure_group(self); }
static void step(id self, int d) {
    id g = group_of(self);
    if (!g) return;
    id ws = group_windows(g);
    u64 n = MSG0(u64, ws, "count"), i = MSG(u64, ws, "indexOfObject:", group_selected(g));
    if (n < 2 || i >= n) return;
    select_window(g, MSG(id, ws, "objectAtIndex:", (i + n + (u64)d) % n));
}
static void w_next_tab(id self, SEL c, id s) { (void)c; (void)s; step(self, 1); }
static void w_prev_tab(id self, SEL c, id s) { (void)c; (void)s; step(self, -1); }
static void w_merge_all(id self, SEL c, id s) {
    (void)c; (void)s;
    id g = ensure_group(self);
    id ws = MSG0(id, NSApp, "windows");
    id my = w_ident(self, 0);
    for (u64 i = 0, n = MSG0(u64, ws, "count"); i < n; i++) {
        id w = MSG(id, ws, "objectAtIndex:", i);
        if (w == self || !MSG0(BOOL, w, "isVisible") || w_mode(w, 0) == 2) continue;   /* NSWindowTabbingModeDisallowed */
        if (!MSG(BOOL, w_ident(w, 0), "isEqual:", my)) continue;
        tg_add(g, 0, w);
    }
    select_window(g, self);
}
static void w_move_to_new(id self, SEL c, id s) {
    (void)c; (void)s;
    id g = group_of(self);
    if (!g || MSG0(u64, group_windows(g), "count") < 2) return;
    R f = msg_rect(self, "frame");
    tg_remove(g, 0, self);
    f.x += 30; f.y -= 30;
    MSG(void, self, "setFrame:display:", f, (BOOL)YES);
    MSG(void, self, "makeKeyAndOrderFront:", (id)0);
}
static void w_toggle_bar(id self, SEL c, id s) {
    (void)c; (void)s;
    id g = ensure_group(self);
    id cur = objc_getAssociatedObject(g, &k_mode);
    objc_setAssociatedObject(g, &k_mode, cur ? 0 : cx_num_i(1), 1);
    layout_bar(g);
}
/* fechar uma aba: sai do grupo e a vizinha aparece */
static void tabs_window_closing(id note_obj) {
    id g = note_obj ? group_of(note_obj) : 0;
    if (g) tg_remove(g, 0, note_obj);
}
static void observer_fire(id self, SEL c, id note) { (void)self; (void)c; tabs_window_closing(MSG0(id, note, "object")); }
static void observer_moved(id self, SEL c, id note) { (void)self; (void)c; id w = MSG0(id, note, "object"); id g = w ? group_of(w) : 0; if (g && group_selected(g) == w) layout_bar(g); }

/* ================================================================ registro */
#define ADD(k, s, f) cx_add_method(k, s, (void *)(f))
void cx_appkit_tabs_init(void) {
    cx_class_t *obj = kls("NSObject"), *view = kls("NSView"), *w = kls("NSWindow");
    if (!obj || !view || !w) return;
    cx_class_t *k = kls("NSWindowTabGroup");
    if (!k) k = cx_define_class(&NSWindowTabGroup_c, &NSWindowTabGroup_m, "NSWindowTabGroup", obj, 0);
    ADD(k, "windows", tg_windows); ADD(k, "selectedWindow", tg_selected); ADD(k, "setSelectedWindow:", tg_set_selected);
    ADD(k, "isTabBarVisible", tg_bar_visible); ADD(k, "isOverviewVisible", tg_overview); ADD(k, "identifier", tg_ident);
    ADD(k, "addWindow:", tg_add); ADD(k, "insertWindow:atIndex:", tg_insert); ADD(k, "removeWindow:", tg_remove);
    if (!kls("_MOSTabBarView")) cx_define_class(&_MOSTabBarView_c, &_MOSTabBarView_m, "_MOSTabBarView", view, 0);
    ADD(&_MOSTabBarView_c, "drawRect:", bar_draw); ADD(&_MOSTabBarView_c, "isFlipped", bar_flipped); ADD(&_MOSTabBarView_c, "mouseDown:", bar_mouse);
    ADD(w, "tabbingMode", w_mode); ADD(w, "setTabbingMode:", w_set_mode); ADD(w, "tabbingIdentifier", w_ident); ADD(w, "setTabbingIdentifier:", w_set_ident);
    ADD(w, "addTabbedWindow:ordered:", w_add_tabbed); ADD(w, "tabbedWindows", w_tabbed); ADD(w, "tabGroup", w_tab_group);
    ADD(w, "selectNextTab:", w_next_tab); ADD(w, "selectPreviousTab:", w_prev_tab); ADD(w, "mergeAllWindows:", w_merge_all);
    ADD(w, "moveTabToNewWindow:", w_move_to_new); ADD(w, "toggleTabBar:", w_toggle_bar);
    cx_class_t *tk = kls("NSWindowTab");
    if (tk) {
        ADD(tk, "title", tab_get_title); ADD(tk, "setTitle:", tab_set_title);
        ADD(tk, "attributedTitle", tab_get_attr); ADD(tk, "setAttributedTitle:", tab_set_attr);
        ADD(tk, "toolTip", tab_get_tip); ADD(tk, "setToolTip:", tab_set_tip);
        ADD(tk, "accessoryView", tab_get_acc); ADD(tk, "setAccessoryView:", tab_set_acc);
    }
    /* acompanha fechamento e movimento das janelas com abas */
    static cx_class_t obs_c, obs_m;
    if (!kls("_MOSTabObserver")) cx_define_class(&obs_c, &obs_m, "_MOSTabObserver", obj, 0);
    cx_class_t *ok = kls("_MOSTabObserver");
    ADD(ok, "windowWillClose:", observer_fire); ADD(ok, "windowMoved:", observer_moved);
    id obs = cx_alloc_instance(ok);
    id nc = MSG0(id, (id)kls("NSNotificationCenter"), "defaultCenter");
    MSG(void, nc, "addObserver:selector:name:object:", obs, sl("windowWillClose:"), cx_str("NSWindowWillCloseNotification"), (id)0);
    MSG(void, nc, "addObserver:selector:name:object:", obs, sl("windowMoved:"), cx_str("NSWindowDidResizeNotification"), (id)0);
    MSG(void, nc, "addObserver:selector:name:object:", obs, sl("windowMoved:"), cx_str("NSWindowDidMoveNotification"), (id)0);
}
