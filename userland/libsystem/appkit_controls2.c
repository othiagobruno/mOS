/*
 * mOS libSystem — mais controles do AppKit:
 *   NSSearchField: botão de limpar (×), ação a cada mudança (sendsWholeSearchString = NO) ou só no Enter,
 *     placeholder "Buscar", menu de buscas recentes (recentSearches / maximumRecents).
 *   NSColorWell + NSColorPanel: amostra clicável; o painel compartilhado tem paleta (matiz × claridade + cinzas),
 *     prévia e opacidade; manda a ação ao alvo, changeColor: pela cadeia de responders e a notificação.
 *   NSDatePicker (não existia): estilos campo+stepper, campo e calendário; campos dd/mm/aaaa hh:mm:ss com
 *     setas, digitação e Tab; mínimo/máximo; o cálculo de datas fica em appkit_date_core.c (teste de host
 *     userland/tests-appkit/controls_host.c).
 * Registro: cx_appkit_controls2_init(), chamado do cx_appkit_input_init.
 */
#include "foundation.h"
#include "sys.h"
#include "libc.h"
#include "appkit_date_core.h"
#include "appkit_menupop_core.h"

id objc_msgSend_stret(id self, SEL op, ...);
id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void objc_setAssociatedObject(id obj, const void *key, id value, u64 policy);
id objc_getAssociatedObject(id obj, const void *key);
IMP class_getMethodImplementation(cx_class_t *c, SEL s);
extern id NSApp;
id cx_color(double r, double g, double b, double a);
void free(void *p);
void NSBeep(void);

typedef struct { double x, y; } P;
typedef struct { double w, h; } S;
typedef struct { double x, y, w, h; } R;
void NSRectFill(R r);
void NSFrameRect(R r);
id cx_menu_popup_choose(id menu, int pos, P pt, int mode, double min_w, double font_size, id from_view);
id cx_menu_find_target(SEL a, id from_view);

#define K(N) cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N); cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N);
K(NSDatePicker) K(NSDatePickerCell) K(_MOSColorPalette) K(_MOSColorTarget)

static cx_class_t *kls(const char *n) { return cx_class_named(n); }
static SEL sl(const char *n) { return sel_registerName(n); }
static BOOL responds(id o, const char *s) { return o && cx_responds(object_getClass(o), sl(s)); }
static R msg_rect(id o, const char *s) { return ((R (*)(id, SEL))objc_msgSend_stret)(o, sl(s)); }
static R msg_rect_r(id o, const char *s, R r) { return ((R (*)(id, SEL, R))objc_msgSend_stret)(o, sl(s), r); }
static S m_size(id s, id attrs) { return ((S (*)(id, SEL, id))objc_msgSend)(s, sl("sizeWithAttributes:"), attrs); }
static void m_draw_at(id s, P p, id attrs) { ((void (*)(id, SEL, P, id))objc_msgSend)(s, sl("drawAtPoint:withAttributes:"), p, attrs); }
static void m_set_frame(id v, R r) { ((void (*)(id, SEL, R))objc_msgSend)(v, sl("setFrame:"), r); }
static id m_init_frame(id k, R r) { return ((id (*)(id, SEL, R))objc_msgSend)(MSG0(id, k, "alloc"), sl("initWithFrame:"), r); }
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
static BOOL enabled(id self) { return !responds(self, "isEnabled") || MSG0(BOOL, self, "isEnabled"); }
static id ctl_font(id self) { id f = responds(self, "font") ? MSG0(id, self, "font") : 0; return f ? f : MSG(id, (id)kls("NSFont"), "systemFontOfSize:", 13.0); }
static id attrs(id font, id color) {
    extern id NSFontAttributeName, NSForegroundColorAttributeName;
    id d = MSG0(id, (id)kls("NSMutableDictionary"), "dictionary");
    MSG(void, d, "setObject:forKey:", font, NSFontAttributeName);
    MSG(void, d, "setObject:forKey:", color, NSForegroundColorAttributeName);
    return d;
}
static void fill(R r, double R_, double G, double B, double A) { MSG0(void, cx_color(R_, G, B, A), "setFill"); NSRectFill(r); }
static void send_own_action(id self) {
    SEL a = responds(self, "action") ? MSG0(SEL, self, "action") : 0;
    id t = responds(self, "target") ? MSG0(id, self, "target") : 0;
    if (a) MSG(BOOL, self, "sendAction:to:", a, t);
}
static IMP super_imp(const char *cls, SEL s) { return class_getMethodImplementation(kls(cls), s); }

/* ================================================================ NSSearchField */
static char k_sf_whole, k_sf_last, k_sf_recents, k_sf_max;
static double sf_btn_x(id self) { R b = msg_rect(self, "bounds"); return b.x + b.w - 20; }
static void sf_draw(id self, SEL c, R r) {
    IMP sup = super_imp("NSTextField", c);
    if (sup) ((void (*)(id, SEL, R))sup)(self, c, r);
    id v = MSG0(id, self, "stringValue");
    if (v && cx_cstr(v)[0]) {
        R b = msg_rect(self, "bounds");
        double cy = b.y + b.h / 2;
        fill((R){sf_btn_x(self) + 3, cy - 7, 14, 14}, 0.6, 0.6, 0.62, 1);
        m_draw_at(cx_str("\xC3\x97"), (P){sf_btn_x(self) + 6, cy - 9}, attrs(MSG(id, (id)kls("NSFont"), "systemFontOfSize:", 12.0), cx_color(1, 1, 1, 1)));
    }
}
static void sf_changed(id self) {
    id v = MSG0(id, self, "stringValue");
    id last = objc_getAssociatedObject(self, &k_sf_last);
    if (last && v && MSG(BOOL, last, "isEqualToString:", v)) return;
    objc_setAssociatedObject(self, &k_sf_last, v ? MSG0(id, v, "copy") : 0, 1);
    id w = objc_getAssociatedObject(self, &k_sf_whole);
    if (!(w && MSG0(BOOL, w, "boolValue"))) send_own_action(self);
}
static void sf_remember(id self) {
    id v = MSG0(id, self, "stringValue");
    if (!v || !cx_cstr(v)[0]) return;
    id r = objc_getAssociatedObject(self, &k_sf_recents);
    id a = MSG0(id, (id)kls("NSMutableArray"), "array");
    MSG(void, a, "addObject:", v);
    u64 n = r ? MSG0(u64, r, "count") : 0;
    id mx = objc_getAssociatedObject(self, &k_sf_max);
    u64 maxr = mx ? MSG0(u64, mx, "unsignedLongLongValue") : 10;
    for (u64 i = 0; i < n && MSG0(u64, a, "count") < maxr; i++) { id x = MSG(id, r, "objectAtIndex:", i); if (!MSG(BOOL, x, "isEqualToString:", v)) MSG(void, a, "addObject:", x); }
    objc_setAssociatedObject(self, &k_sf_recents, a, 1);
}
static void sf_key(id self, SEL c, id e) {
    IMP sup = super_imp("NSTextField", c);
    u16 code = MSG0(u16, e, "keyCode");
    if (code == 53) { MSG(void, self, "setStringValue:", cx_str("")); redraw(self); sf_changed(self); return; }   /* Esc limpa */
    if (sup) ((void (*)(id, SEL, id))sup)(self, c, e);
    if (code == 36 || code == 76) { sf_remember(self); return; }   /* o Enter já mandou a ação */
    sf_changed(self);
}
static void sf_mouse(id self, SEL c, id e) {
    P p = m_loc(self, e);
    id v = MSG0(id, self, "stringValue");
    if (p.x >= sf_btn_x(self) && v && cx_cstr(v)[0]) {
        MSG(void, self, "setStringValue:", cx_str(""));
        redraw(self);
        sf_changed(self);
        if (responds(self, "delegate")) { id d = MSG0(id, self, "delegate"); if (responds(d, "searchFieldDidEndSearching:")) MSG(void, d, "searchFieldDidEndSearching:", self); }
        return;
    }
    id rec = objc_getAssociatedObject(self, &k_sf_recents);
    R b = msg_rect(self, "bounds");
    if (p.x < b.x + 18 && rec && MSG0(u64, rec, "count")) {   /* lupa: menu das buscas recentes */
        id menu = MSG(id, MSG0(id, (id)kls("NSMenu"), "alloc"), "initWithTitle:", cx_str(""));
        MSG(void, menu, "setAutoenablesItems:", (BOOL)NO);
        u64 n = MSG0(u64, rec, "count");
        for (u64 i = 0; i < n; i++) {
            id title = MSG(id, rec, "objectAtIndex:", i);
            id it = MSG(id, MSG0(id, (id)kls("NSMenuItem"), "alloc"), "initWithTitle:action:keyEquivalent:", title, (SEL)0, cx_str(""));
            MSG(void, menu, "addItem:", it); objc_release(it);
        }
        BOOL fl = MSG0(BOOL, self, "isFlipped");
        id ch = cx_menu_popup_choose(menu, -1, view_to_screen(self, (P){b.x, fl ? b.y + b.h : b.y}), MP_PLACE_CONTEXT, b.w, 0, self);
        if (ch) { MSG(void, self, "setStringValue:", MSG0(id, ch, "title")); redraw(self); send_own_action(self); }
        objc_release(menu);
        return;
    }
    IMP sup = super_imp("NSTextField", c);
    if (sup) ((void (*)(id, SEL, id))sup)(self, c, e);
}
static id sf_init(id self, SEL c, R f) {
    IMP sup = super_imp("NSTextField", c);
    self = sup ? ((id (*)(id, SEL, R))sup)(self, c, f) : self;
    if (self) {
        MSG(void, self, "setPlaceholderString:", cx_str("Buscar"));
        MSG(void, self, "setBezeled:", (BOOL)YES);
        MSG(void, self, "setEditable:", (BOOL)YES);
    }
    return self;
}
static BOOL sf_whole(id s, SEL c) { (void)c; id v = objc_getAssociatedObject(s, &k_sf_whole); return v && MSG0(BOOL, v, "boolValue"); }
static void sf_set_whole(id s, SEL c, BOOL b) { (void)c; objc_setAssociatedObject(s, &k_sf_whole, cx_num_i(b), 1); }
static BOOL sf_immediate(id s, SEL c) { return !sf_whole(s, c); }
static void sf_set_immediate(id s, SEL c, BOOL b) { if (b) sf_set_whole(s, c, NO); }
static id sf_recents(id s, SEL c) { (void)c; id r = objc_getAssociatedObject(s, &k_sf_recents); return r ? r : MSG0(id, (id)kls("NSArray"), "array"); }
static void sf_set_recents(id s, SEL c, id a) { (void)c; objc_setAssociatedObject(s, &k_sf_recents, a ? MSG0(id, a, "mutableCopy") : 0, 1); }
static i64 sf_max(id s, SEL c) { (void)c; id v = objc_getAssociatedObject(s, &k_sf_max); return v ? MSG0(i64, v, "longLongValue") : 10; }
static void sf_set_max(id s, SEL c, i64 n) { (void)c; objc_setAssociatedObject(s, &k_sf_max, cx_num_i(n), 1); }

/* ================================================================ NSColorPanel e NSColorWell */
static id g_cpanel, g_cpal, g_cprev, g_calpha, g_active_well;
static char k_well_color, k_well_border, k_cp_target, k_cp_action, k_cp_alpha, k_cp_cont;
static id cur_color(void) { id c = g_cpanel ? objc_getAssociatedObject(g_cpanel, &k_well_color) : 0; return c ? c : cx_color(0, 0, 0, 1); }
static void hsl(double h, double s, double l, double *r, double *g, double *b) {
    double c = (1 - (2 * l - 1 < 0 ? -(2 * l - 1) : 2 * l - 1)) * s, hp = h * 6, x = c * (1 - ((hp - 2 * (int)(hp / 2)) - 1 < 0 ? -((hp - 2 * (int)(hp / 2)) - 1) : (hp - 2 * (int)(hp / 2)) - 1));
    double r1 = 0, g1 = 0, b1 = 0;
    int k = (int)hp;
    switch (k) { case 0: r1 = c; g1 = x; break; case 1: r1 = x; g1 = c; break; case 2: g1 = c; b1 = x; break;
                 case 3: g1 = x; b1 = c; break; case 4: r1 = x; b1 = c; break; default: r1 = c; b1 = x; }
    double m = l - c / 2;
    *r = r1 + m; *g = g1 + m; *b = b1 + m;
}
#define PAL_COLS 12
#define PAL_ROWS 8
static void pal_color(int col, int row, double *r, double *g, double *b) {
    if (row == PAL_ROWS - 1) { double v = (double)col / (PAL_COLS - 1); *r = *g = *b = v; return; }   /* cinzas */
    hsl((double)col / PAL_COLS, 0.85, 0.15 + 0.7 * (double)row / (PAL_ROWS - 2), r, g, b);
}
static void cp_apply(id color, BOOL notify) {
    if (!g_cpanel) return;
    objc_setAssociatedObject(g_cpanel, &k_well_color, color, 1);
    if (g_cprev) redraw(g_cprev);
    if (!notify) return;
    if (g_active_well) { objc_setAssociatedObject(g_active_well, &k_well_color, color, 1); redraw(g_active_well); send_own_action(g_active_well); }
    id t = objc_getAssociatedObject(g_cpanel, &k_cp_target);
    id an = objc_getAssociatedObject(g_cpanel, &k_cp_action);
    SEL a = an ? sl(cx_cstr(an)) : 0;
    if (a && t && cx_responds(object_getClass(t), a)) ((void (*)(id, SEL, id))objc_msgSend)(t, a, g_cpanel);
    else if (!g_active_well) {   /* sem alvo: changeColor: pela cadeia, como o Cocoa */
        id r = cx_menu_find_target(sl("changeColor:"), 0);
        if (r) MSG(void, r, "changeColor:", g_cpanel);
    }
    id nc = MSG0(id, (id)kls("NSNotificationCenter"), "defaultCenter");
    if (nc) MSG(void, nc, "postNotificationName:object:", cx_str("NSColorPanelColorDidChangeNotification"), g_cpanel);
}
static void pal_draw(id self, SEL c, R d) {
    (void)c; (void)d;
    R b = msg_rect(self, "bounds");
    double cw = b.w / PAL_COLS, ch = b.h / PAL_ROWS;
    for (int row = 0; row < PAL_ROWS; row++) for (int col = 0; col < PAL_COLS; col++) {
        double r, g, bl;
        pal_color(col, row, &r, &g, &bl);
        fill((R){b.x + col * cw, b.y + row * ch, cw, ch}, r, g, bl, 1);
    }
    MSG0(void, cx_color(0, 0, 0, 0.25), "setFill");
    NSFrameRect(b);
}
static void pal_mouse(id self, SEL c, id e) {
    (void)c;
    R b = msg_rect(self, "bounds");
    P p = m_loc(self, e);
    int col = (int)((p.x - b.x) / (b.w / PAL_COLS)), row = (int)((p.y - b.y) / (b.h / PAL_ROWS));
    if (col < 0 || col >= PAL_COLS || row < 0 || row >= PAL_ROWS) return;
    double r, g, bl;
    pal_color(col, row, &r, &g, &bl);
    double a = MSG0(double, cur_color(), "alphaComponent");
    cp_apply(cx_color(r, g, bl, a > 0 ? a : 1), YES);
}
static void prev_draw(id self, SEL c, R d) {
    (void)c; (void)d;
    R b = msg_rect(self, "bounds");
    fill(b, 1, 1, 1, 1);
    for (int i = 0; i < 8; i++) fill((R){b.x + i * b.w / 8, b.y, b.w / 16, b.h}, 0.8, 0.8, 0.8, 1);   /* xadrez para ver a opacidade */
    MSG0(void, cur_color(), "setFill");
    NSRectFill(b);
    MSG0(void, cx_color(0, 0, 0, 0.3), "setFill");
    NSFrameRect(b);
}
static void alpha_changed(id self, SEL c, id sl_) {
    (void)self; (void)c;
    id k = cur_color();
    double a = MSG0(double, sl_, "doubleValue");
    cp_apply(cx_color(MSG0(double, k, "redComponent"), MSG0(double, k, "greenComponent"), MSG0(double, k, "blueComponent"), a), YES);
}
static void cp_build(id panel) {
    MSG(void, panel, "setTitle:", cx_str("Cores"));
    MSG(void, panel, "setReleasedWhenClosed:", (BOOL)NO);
    id cv = MSG0(id, panel, "contentView");
    g_cpal = m_init_frame((id)&_MOSColorPalette_c, (R){12, 60, 240, 160});
    MSG(void, cv, "addSubview:", g_cpal);
    g_cprev = m_init_frame((id)&_MOSColorPalette_c, (R){12, 228, 240, 24});
    objc_setAssociatedObject(g_cprev, &k_cp_cont, cx_num_i(1), 1);     /* marca: este desenha a prévia */
    MSG(void, cv, "addSubview:", g_cprev);
    id lb = MSG(id, (id)kls("NSTextField"), "labelWithString:", cx_str("Opacidade"));
    m_set_frame(lb, (R){12, 30, 80, 18});
    MSG(void, cv, "addSubview:", lb);
    static id tgt;
    if (!tgt) tgt = cx_alloc_instance(&_MOSColorTarget_c);
    g_calpha = MSG(id, (id)kls("NSSlider"), "sliderWithValue:minValue:maxValue:target:action:", 1.0, 0.0, 1.0, tgt, sl("cxAlpha:"));
    m_set_frame(g_calpha, (R){92, 28, 160, 22});
    MSG(void, cv, "addSubview:", g_calpha);
}
static void palette_draw(id self, SEL c, R d) { if (objc_getAssociatedObject(self, &k_cp_cont)) prev_draw(self, c, d); else pal_draw(self, c, d); }
static void palette_mouse(id self, SEL c, id e) { if (!objc_getAssociatedObject(self, &k_cp_cont)) pal_mouse(self, c, e); }
static id cp_shared(id k, SEL c) {
    (void)c;
    if (g_cpanel) return g_cpanel;
    g_cpanel = ((id (*)(id, SEL, R, u64, u64, BOOL))objc_msgSend)(MSG0(id, k, "alloc"), sl("initWithContentRect:styleMask:backing:defer:"), (R){0, 0, 264, 264}, (u64)(1 | 2), (u64)2, NO);
    cp_build(g_cpanel);
    return g_cpanel;
}
static BOOL cp_exists(id k, SEL c) { (void)k; (void)c; return g_cpanel != 0; }
static id cp_color(id s, SEL c) { (void)s; (void)c; return cur_color(); }
static void cp_set_color(id s, SEL c, id col) { (void)s; (void)c; cp_apply(col, NO); if (g_calpha && col) MSG(void, g_calpha, "setDoubleValue:", MSG0(double, col, "alphaComponent")); }
static void cp_set_target(id s, SEL c, id t) { (void)c; objc_setAssociatedObject(s, &k_cp_target, t, 0); }
static void cp_set_action(id s, SEL c, SEL a) { (void)c; objc_setAssociatedObject(s, &k_cp_action, a ? cx_str(a) : 0, 1); }
static BOOL cp_shows_alpha(id s, SEL c) { (void)c; id v = objc_getAssociatedObject(s, &k_cp_alpha); return v ? MSG0(BOOL, v, "boolValue") : YES; }
static void cp_set_shows_alpha(id s, SEL c, BOOL b) { (void)c; objc_setAssociatedObject(s, &k_cp_alpha, cx_num_i(b), 1); if (g_calpha) MSG(void, g_calpha, "setHidden:", (BOOL)!b); }
static double cp_alpha(id s, SEL c) { (void)s; (void)c; return MSG0(double, cur_color(), "alphaComponent"); }
static BOOL cp_continuous(id s, SEL c) { (void)s; (void)c; return YES; }
static void cp_noop_b(id s, SEL c, BOOL b) { (void)s; (void)c; (void)b; }
static void cp_noop_i(id s, SEL c, i64 i) { (void)s; (void)c; (void)i; }
static i64 cp_mode(id s, SEL c) { (void)s; (void)c; return 6; /* NSColorPanelModeWheel: o mais próximo da paleta */ }
static void cp_cnoop_i(id k, SEL c, i64 i) { (void)k; (void)c; (void)i; }

/* NSColorWell */
static id cw_color(id s, SEL c) { (void)c; id v = objc_getAssociatedObject(s, &k_well_color); return v ? v : cx_color(1, 1, 1, 1); }
static void cw_set_color(id s, SEL c, id col) { (void)c; objc_setAssociatedObject(s, &k_well_color, col, 1); redraw(s); if (g_active_well == s && g_cpanel) cp_set_color(g_cpanel, 0, col); }
static BOOL cw_active(id s, SEL c) { (void)c; return g_active_well == s; }
static void cw_deactivate(id s, SEL c) { (void)c; if (g_active_well == s) { g_active_well = 0; redraw(s); } }
static void cw_activate(id s, SEL c, BOOL exclusive) {
    (void)c; (void)exclusive;
    if (g_active_well && g_active_well != s) { id o = g_active_well; g_active_well = 0; redraw(o); }
    g_active_well = s;
    id p = cp_shared((id)kls("NSColorPanel"), 0);
    cp_set_color(p, 0, cw_color(s, 0));
    MSG(void, p, "makeKeyAndOrderFront:", (id)0);
    redraw(s);
}
static BOOL cw_bordered(id s, SEL c) { (void)c; id v = objc_getAssociatedObject(s, &k_well_border); return v ? MSG0(BOOL, v, "boolValue") : YES; }
static void cw_set_bordered(id s, SEL c, BOOL b) { (void)c; objc_setAssociatedObject(s, &k_well_border, cx_num_i(b), 1); redraw(s); }
static void cw_draw(id self, SEL c, R d) {
    (void)c; (void)d;
    R b = msg_rect(self, "bounds");
    BOOL act = g_active_well == self;
    if (cw_bordered(self, 0)) { fill(b, act ? 0.75 : 0.93, act ? 0.82 : 0.93, act ? 0.95 : 0.94, 1); MSG0(void, cx_color(0, 0, 0, 0.25), "setFill"); NSFrameRect(b); }
    R in = cw_bordered(self, 0) ? (R){b.x + 4, b.y + 4, b.w - 8, b.h - 8} : b;
    MSG0(void, cw_color(self, 0), "setFill");
    NSRectFill(in);
    MSG0(void, cx_color(0, 0, 0, 0.2), "setFill");
    NSFrameRect(in);
}
static void cw_mouse(id self, SEL c, id e) {
    (void)c; (void)e;
    if (!enabled(self)) return;
    if (g_active_well == self) cw_deactivate(self, 0); else cw_activate(self, 0, YES);
}
static void cw_take(id self, SEL c, id sender) { (void)c; if (sender && responds(sender, "color")) cw_set_color(self, 0, MSG0(id, sender, "color")); }
static void cw_draw_inside(id self, SEL c, R r) { (void)c; MSG0(void, cw_color(self, 0), "setFill"); NSRectFill(r); }
static BOOL cw_supports_alpha(id s, SEL c) { (void)s; (void)c; return YES; }
static void tg_alpha(id self, SEL c, id s) { alpha_changed(self, c, s); }

/* ================================================================ NSDatePicker */
typedef struct dp {
    struct dp *next;
    id obj, min, max, delegate, text_color, bg_color, tz;
    double t;                 /* segundos desde 1970 (UTC) */
    unsigned flags;
    i64 style, mode;
    double interval;
    int field, acc, nacc;
    int view_y, view_m;       /* mês mostrado no calendário */
    BOOL bezeled, draws_bg;
} dp_t;
static dp_t *g_dp;
static dp_t *dp(id o) {
    for (dp_t *p = g_dp; p; p = p->next) if (p->obj == o) return p;
    dp_t *p = calloc(1, sizeof *p);
    if (!p) return 0;
    p->obj = o;
    id now = MSG0(id, (id)kls("NSDate"), "date");
    p->t = now ? MSG0(double, now, "timeIntervalSince1970") : 0;
    p->flags = DT_EL_YEAR_MONTH_DAY | DT_EL_HOUR_MINUTE_SECOND;
    p->bezeled = YES; p->draws_bg = YES;
    p->next = g_dp; g_dp = p;
    return p;
}
static long long tz_off(dp_t *p) {
    id tz = p->tz ? p->tz : MSG0(id, (id)kls("NSTimeZone"), "localTimeZone");
    return tz && responds(tz, "secondsFromGMT") ? MSG0(i64, tz, "secondsFromGMT") : 0;
}
static void dp_civil(dp_t *p, dt_civil_t *c) { dt_from_seconds((long long)p->t + tz_off(p), c); }
static void dp_store(id self, dp_t *p, const dt_civil_t *c, BOOL act) {
    double t = (double)(dt_to_seconds(c) - tz_off(p));
    double mn = p->min ? MSG0(double, p->min, "timeIntervalSince1970") : 0, mx = p->max ? MSG0(double, p->max, "timeIntervalSince1970") : 0;
    t = (double)dt_clamp((long long)t, (long long)mn, (long long)mx, p->min != 0, p->max != 0);
    if (p->delegate && responds(p->delegate, "datePickerCell:validateProposedDateValue:timeInterval:")) {
        id d = MSG(id, (id)kls("NSDate"), "dateWithTimeIntervalSince1970:", t);
        double iv = p->interval;
        ((void (*)(id, SEL, id, id *, double *))objc_msgSend)(p->delegate, sl("datePickerCell:validateProposedDateValue:timeInterval:"), self, &d, &iv);
        if (d) t = MSG0(double, d, "timeIntervalSince1970");
    }
    if (t == p->t) { redraw(self); return; }
    p->t = t;
    redraw(self);
    if (act) send_own_action(self);
}
static id dp_init(id self, SEL c, R f) {
    IMP sup = super_imp("NSControl", c);
    self = sup ? ((id (*)(id, SEL, R))sup)(self, c, f) : self;
    if (self) dp(self);
    return self;
}
static id dp_date(id s, SEL c) { (void)c; return MSG(id, (id)kls("NSDate"), "dateWithTimeIntervalSince1970:", dp(s)->t); }
static void dp_set_date(id s, SEL c, id d) { (void)c; dp_t *p = dp(s); if (!d) return; p->t = MSG0(double, d, "timeIntervalSince1970"); redraw(s); }
static id dp_min(id s, SEL c) { (void)c; return dp(s)->min; }
static void dp_set_min(id s, SEL c, id d) { (void)c; dp_t *p = dp(s); objc_retain(d); objc_release(p->min); p->min = d; }
static id dp_max(id s, SEL c) { (void)c; return dp(s)->max; }
static void dp_set_max(id s, SEL c, id d) { (void)c; dp_t *p = dp(s); objc_retain(d); objc_release(p->max); p->max = d; }
static u64 dp_flags(id s, SEL c) { (void)c; return dp(s)->flags; }
static void dp_set_flags(id s, SEL c, u64 f) { (void)c; dp(s)->flags = (unsigned)f; dp(s)->field = 0; redraw(s); }
static i64 dp_style(id s, SEL c) { (void)c; return dp(s)->style; }
static void dp_set_style(id s, SEL c, i64 v) { (void)c; dp(s)->style = v; redraw(s); }
static i64 dp_mode(id s, SEL c) { (void)c; return dp(s)->mode; }
static void dp_set_mode(id s, SEL c, i64 v) { (void)c; dp(s)->mode = v; }
static double dp_interval(id s, SEL c) { (void)c; return dp(s)->interval; }
static void dp_set_interval(id s, SEL c, double v) { (void)c; dp(s)->interval = v; }
static id dp_delegate(id s, SEL c) { (void)c; return dp(s)->delegate; }
static void dp_set_delegate(id s, SEL c, id d) { (void)c; dp(s)->delegate = d; }
static id dp_tz(id s, SEL c) { (void)c; return dp(s)->tz; }
static void dp_set_tz(id s, SEL c, id tz) { (void)c; dp_t *p = dp(s); objc_retain(tz); objc_release(p->tz); p->tz = tz; redraw(s); }
static id dp_obj_cal(id s, SEL c) { (void)s; (void)c; return MSG0(id, (id)kls("NSCalendar"), "currentCalendar"); }
static id dp_obj_locale(id s, SEL c) { (void)s; (void)c; return MSG0(id, (id)kls("NSLocale"), "currentLocale"); }
static void dp_noop_obj(id s, SEL c, id o) { (void)s; (void)c; (void)o; }
static BOOL dp_bezeled(id s, SEL c) { (void)c; return dp(s)->bezeled; }
static void dp_set_bezeled(id s, SEL c, BOOL b) { (void)c; dp(s)->bezeled = b; redraw(s); }
static BOOL dp_draws_bg(id s, SEL c) { (void)c; return dp(s)->draws_bg; }
static void dp_set_draws_bg(id s, SEL c, BOOL b) { (void)c; dp(s)->draws_bg = b; redraw(s); }
static id dp_text_color(id s, SEL c) { (void)c; return dp(s)->text_color; }
static void dp_set_text_color(id s, SEL c, id col) { (void)c; dp_t *p = dp(s); objc_retain(col); objc_release(p->text_color); p->text_color = col; redraw(s); }
static id dp_bg_color(id s, SEL c) { (void)c; return dp(s)->bg_color; }
static void dp_set_bg_color(id s, SEL c, id col) { (void)c; dp_t *p = dp(s); objc_retain(col); objc_release(p->bg_color); p->bg_color = col; redraw(s); }
static BOOL yes_imp(id s, SEL c) { (void)s; (void)c; return YES; }

static const char *mes[] = {"janeiro", "fevereiro", "março", "abril", "maio", "junho", "julho", "agosto", "setembro", "outubro", "novembro", "dezembro"};
#define CAL_HEAD 26.0
#define CAL_CELL 24.0
static double text_h(void) { return 24; }

static void dp_draw(id self, SEL c, R d) {
    (void)c; (void)d;
    dp_t *p = dp(self);
    R b = msg_rect(self, "bounds");
    BOOL flipped = MSG0(BOOL, self, "isFlipped");
    BOOL on = enabled(self);
    id font = ctl_font(self);
    id tc = p->text_color ? p->text_color : cx_color(0.1, 0.1, 0.1, on ? 1 : 0.4);
    dt_civil_t cv;
    dp_civil(p, &cv);
    if (p->style == 1) {   /* calendário */
        if (!p->view_y) { p->view_y = cv.y; p->view_m = cv.mo; }
        fill(b, 1, 1, 1, 1);
        double top = flipped ? b.y : b.y + b.h;
        double dir = flipped ? 1 : -1;
        char head[48];
        const char *mn = mes[p->view_m - 1];
        size_t l = strlen(mn);
        memcpy(head, mn, l);
        head[l] = ' ';
        int y = p->view_y;
        head[l + 1] = (char)('0' + y / 1000 % 10); head[l + 2] = (char)('0' + y / 100 % 10); head[l + 3] = (char)('0' + y / 10 % 10); head[l + 4] = (char)('0' + y % 10); head[l + 5] = 0;
        id ah = attrs(MSG(id, (id)kls("NSFont"), "boldSystemFontOfSize:", 13.0), tc);
        double hy = flipped ? top + 4 : top - 20;
        m_draw_at(cx_str(head), (P){b.x + 8, hy}, ah);
        m_draw_at(cx_str("\xE2\x80\xB9"), (P){b.x + b.w - 40, hy}, ah);
        m_draw_at(cx_str("\xE2\x80\xBA"), (P){b.x + b.w - 18, hy}, ah);
        static const char *wd[] = {"D", "S", "T", "Q", "Q", "S", "S"};
        id aw = attrs(MSG(id, (id)kls("NSFont"), "systemFontOfSize:", 10.0), cx_color(0.45, 0.45, 0.48, 1));
        double cw = (b.w - 8) / 7;
        for (int i = 0; i < 7; i++) m_draw_at(cx_str(wd[i]), (P){b.x + 4 + i * cw + cw / 2 - 3, flipped ? top + CAL_HEAD - 4 : top - CAL_HEAD - 8}, aw);
        int g[42];
        dt_month_grid(p->view_y, p->view_m, g);
        id ad = attrs(font, tc), ao = attrs(font, cx_color(0.1, 0.1, 0.1, 0.3)), as = attrs(font, cx_color(1, 1, 1, 1));
        for (int i = 0; i < 42; i++) {
            int r = i / 7, col = i % 7;
            double cy = top + dir * (CAL_HEAD + 12 + r * CAL_CELL) - (flipped ? 0 : CAL_CELL);
            R cell = {b.x + 4 + col * cw, cy, cw, CAL_CELL};
            BOOL selday = g[i] > 0 && g[i] == cv.d && p->view_m == cv.mo && p->view_y == cv.y;
            if (selday) fill((R){cell.x + 2, cell.y + 2, cell.w - 4, cell.h - 4}, 0.0, 0.40, 0.86, 1);
            char num[4]; int v = g[i] < 0 ? -g[i] : g[i];
            num[0] = v >= 10 ? (char)('0' + v / 10) : ' '; num[1] = (char)('0' + v % 10); num[2] = 0;
            m_draw_at(cx_str(v >= 10 ? num : num + 1), (P){cell.x + cw / 2 - 7, cell.y + 4}, selday ? as : g[i] < 0 ? ao : ad);
        }
        MSG0(void, cx_color(0, 0, 0, 0.15), "setFill");
        NSFrameRect(b);
        return;
    }
    /* campo de texto (+ stepper) */
    double sw = p->style == 0 ? 16 : 0;
    double h = text_h() < b.h ? text_h() : b.h, y = b.y + (b.h - h) / 2;
    R box = {b.x, y, b.w - sw - (sw ? 3 : 0), h};
    if (p->draws_bg) { if (p->bg_color) { MSG0(void, p->bg_color, "setFill"); NSRectFill(box); } else fill(box, 1, 1, 1, 1); }
    if (p->bezeled) { MSG0(void, cx_color(0, 0, 0, 0.22), "setFill"); NSFrameRect(box); }
    char txt[64];
    int sp[DT_NFIELDS][2];
    int nf = dt_format(&cv, p->flags, txt, sizeof txt, sp);
    id a = attrs(font, tc);
    double x0 = box.x + 6;
    S ts = m_size(cx_str(txt), a);
    double ty = box.y + (h - ts.h) / 2;
    id w = MSG0(id, self, "window");
    BOOL focused = w && MSG0(id, w, "firstResponder") == self;
    if (focused && nf > 0) {
        int f = p->field < nf ? p->field : nf - 1;
        char pre[64]; memcpy(pre, txt, (size_t)sp[f][0]); pre[sp[f][0]] = 0;
        char seg[16]; int sl_ = sp[f][1] - sp[f][0]; memcpy(seg, txt + sp[f][0], (size_t)sl_); seg[sl_] = 0;
        double fx = m_size(cx_str(pre), a).w, fw = m_size(cx_str(seg), a).w;
        fill((R){x0 + fx - 1, ty, fw + 2, ts.h}, 0.0, 0.40, 0.86, 0.35);
    }
    m_draw_at(cx_str(txt), (P){x0, ty}, a);
    if (sw) {
        R st = {b.x + b.w - sw, y, sw, h};
        fill(st, 0.95, 0.95, 0.97, 1);
        MSG0(void, cx_color(0, 0, 0, 0.22), "setFill");
        NSFrameRect(st);
        NSRectFill((R){st.x, st.y + h / 2, sw, 1});
        id aa = attrs(MSG(id, (id)kls("NSFont"), "systemFontOfSize:", 8.0), cx_color(0.2, 0.2, 0.2, on ? 1 : 0.4));
        m_draw_at(cx_str("\xE2\x96\xB4"), (P){st.x + 4, flipped ? st.y + 1 : st.y + h / 2}, aa);
        m_draw_at(cx_str("\xE2\x96\xBE"), (P){st.x + 4, flipped ? st.y + h / 2 : st.y + 1}, aa);
    }
}
static int dp_field_at(id self, dp_t *p, double x) {
    dt_civil_t cv; dp_civil(p, &cv);
    char txt[64]; int sp[DT_NFIELDS][2];
    int nf = dt_format(&cv, p->flags, txt, sizeof txt, sp);
    id a = attrs(ctl_font(self), cx_color(0, 0, 0, 1));
    R b = msg_rect(self, "bounds");
    double x0 = b.x + 6;
    for (int i = 0; i < nf; i++) {
        char pre[64]; memcpy(pre, txt, (size_t)sp[i][1]); pre[sp[i][1]] = 0;
        if (x < x0 + m_size(cx_str(pre), a).w + 3) return i;
    }
    return nf - 1;
}
static void dp_step_field(id self, dp_t *p, int delta) {
    int fl[DT_NFIELDS], nf = dt_fields(p->flags, fl);
    if (!nf) return;
    if (p->field >= nf) p->field = nf - 1;
    dt_civil_t cv; dp_civil(p, &cv);
    dt_step(&cv, fl[p->field], delta);
    p->acc = p->nacc = 0;
    dp_store(self, p, &cv, YES);
}
static void dp_mouse(id self, SEL c, id e) {
    (void)c;
    if (!enabled(self)) return;
    dp_t *p = dp(self);
    id w = MSG0(id, self, "window");
    if (w) MSG(void, w, "makeFirstResponder:", self);
    R b = msg_rect(self, "bounds");
    P pt = m_loc(self, e);
    BOOL flipped = MSG0(BOOL, self, "isFlipped");
    if (p->style == 1) {
        double top = flipped ? b.y : b.y + b.h;
        double fromtop = flipped ? pt.y - top : top - pt.y;
        if (fromtop < CAL_HEAD) {
            if (pt.x > b.x + b.w - 46) {
                int dm = pt.x > b.x + b.w - 24 ? 1 : -1;
                p->view_m += dm;
                if (p->view_m > 12) { p->view_m = 1; p->view_y++; }
                if (p->view_m < 1) { p->view_m = 12; p->view_y--; }
                redraw(self);
            }
            return;
        }
        int r = (int)((fromtop - CAL_HEAD - 12) / CAL_CELL), col = (int)((pt.x - b.x - 4) / ((b.w - 8) / 7));
        if (r < 0 || r > 5 || col < 0 || col > 6) return;
        int g[42];
        dt_month_grid(p->view_y, p->view_m, g);
        int dday = g[r * 7 + col];
        dt_civil_t cv; dp_civil(p, &cv);
        if (dday > 0) { cv.y = p->view_y; cv.mo = p->view_m; cv.d = dday; }
        else {   /* dia do mês vizinho: vai para ele */
            int m = p->view_m + (r == 0 ? -1 : 1), y = p->view_y;
            if (m < 1) { m = 12; y--; } if (m > 12) { m = 1; y++; }
            cv.y = y; cv.mo = m; cv.d = -dday;
            p->view_y = y; p->view_m = m;
        }
        dp_store(self, p, &cv, YES);
        return;
    }
    if (p->style == 0 && pt.x >= b.x + b.w - 16) {
        double mid = b.y + b.h / 2;
        BOOL up = flipped ? pt.y < mid : pt.y >= mid;
        dp_step_field(self, p, up ? 1 : -1);
        return;
    }
    p->field = dp_field_at(self, p, pt.x);
    p->acc = p->nacc = 0;
    redraw(self);
}
static void dp_key(id self, SEL c, id e) {
    dp_t *p = dp(self);
    u16 code = MSG0(u16, e, "keyCode");
    u64 mods = MSG0(u64, e, "modifierFlags");
    int fl[DT_NFIELDS], nf = dt_fields(p->flags, fl);
    if (p->style == 1) {   /* calendário: setas andam um dia / uma semana */
        dt_civil_t cv; dp_civil(p, &cv);
        long long days = dt_days_from_civil(cv.y, cv.mo, cv.d);
        int dd = code == 123 ? -1 : code == 124 ? 1 : code == 126 ? -7 : code == 125 ? 7 : 0;
        if (!dd) { IMP sup = super_imp("NSControl", c); if (sup) ((void (*)(id, SEL, id))sup)(self, c, e); return; }
        dt_civil_t n2; dt_from_seconds((days + dd) * 86400, &n2);
        cv.y = n2.y; cv.mo = n2.mo; cv.d = n2.d;
        p->view_y = cv.y; p->view_m = cv.mo;
        dp_store(self, p, &cv, YES);
        return;
    }
    switch (code) {
    case 126: dp_step_field(self, p, 1); return;
    case 125: dp_step_field(self, p, -1); return;
    case 123: if (p->field > 0) p->field--; p->acc = p->nacc = 0; redraw(self); return;
    case 124: if (p->field < nf - 1) p->field++; p->acc = p->nacc = 0; redraw(self); return;
    case 48: {   /* Tab: próximo campo; no último, sai para a próxima view */
        BOOL back = (mods & (1u << 17)) != 0;
        if (!back && p->field < nf - 1) { p->field++; redraw(self); return; }
        if (back && p->field > 0) { p->field--; redraw(self); return; }
        break;
    }
    }
    id ch = MSG0(id, e, "characters");
    const char *s = ch ? cx_cstr(ch) : "";
    if (s[0] >= '0' && s[0] <= '9' && nf) {
        dt_civil_t cv; dp_civil(p, &cv);
        int done = dt_type_digit(&cv, fl[p->field], s[0] - '0', &p->acc, &p->nacc);
        dp_store(self, p, &cv, YES);
        if (done && p->field < nf - 1) p->field++;
        redraw(self);
        return;
    }
    IMP sup = super_imp("NSControl", c);
    if (sup) ((void (*)(id, SEL, id))sup)(self, c, e);
}
static S dp_intrinsic(id self, SEL c) {
    (void)c;
    dp_t *p = dp(self);
    if (p->style == 1) return (S){180, CAL_HEAD + 12 + 6 * CAL_CELL + 4};
    dt_civil_t cv = {2026, 12, 28, 23, 58, 58};
    char txt[64]; int sp[DT_NFIELDS][2];
    dt_format(&cv, p->flags, txt, sizeof txt, sp);
    double w = m_size(cx_str(txt), attrs(ctl_font(self), cx_color(0, 0, 0, 1))).w + 14 + (p->style == 0 ? 19 : 0);
    return (S){w, text_h()};
}
static void dp_size_to_fit(id self, SEL c) { S z = dp_intrinsic(self, c); ((void (*)(id, SEL, S))objc_msgSend)(self, sl("setFrameSize:"), z); }
static id dp_object_value(id s, SEL c) { return dp_date(s, c); }

/* ================================================================ registro */
#define ADD(cls, sel, fn) cx_add_method(cls, sel, (void *)(fn))
void cx_appkit_controls2_init(void) {
    cx_class_t *sf = kls("NSSearchField"), *cw = kls("NSColorWell"), *cp = kls("NSColorPanel"), *ctl = kls("NSControl"), *view = kls("NSView");
    cx_class_t *acell = kls("NSActionCell"), *obj = kls("NSObject");
    if (sf) {
        ADD(sf, "initWithFrame:", sf_init); ADD(sf, "drawRect:", sf_draw); ADD(sf, "keyDown:", sf_key); ADD(sf, "mouseDown:", sf_mouse);
        ADD(sf, "sendsWholeSearchString", sf_whole); ADD(sf, "setSendsWholeSearchString:", sf_set_whole);
        ADD(sf, "sendsSearchStringImmediately", sf_immediate); ADD(sf, "setSendsSearchStringImmediately:", sf_set_immediate);
        ADD(sf, "recentSearches", sf_recents); ADD(sf, "setRecentSearches:", sf_set_recents);
        ADD(sf, "maximumRecents", sf_max); ADD(sf, "setMaximumRecents:", sf_set_max);
    }
    if (view && obj) {
        cx_define_class(&_MOSColorPalette_c, &_MOSColorPalette_m, "_MOSColorPalette", view, 0);
        ADD(&_MOSColorPalette_c, "drawRect:", palette_draw); ADD(&_MOSColorPalette_c, "mouseDown:", palette_mouse);
        ADD(&_MOSColorPalette_c, "mouseDragged:", palette_mouse); ADD(&_MOSColorPalette_c, "isFlipped", yes_imp);
        cx_define_class(&_MOSColorTarget_c, &_MOSColorTarget_m, "_MOSColorTarget", obj, 0);
        ADD(&_MOSColorTarget_c, "cxAlpha:", tg_alpha);
    }
    if (cp) {
        cx_add_cmethod(cp, "sharedColorPanel", (void *)cp_shared);
        cx_add_cmethod(cp, "sharedColorPanelExists", (void *)cp_exists);
        cx_add_cmethod(cp, "setPickerMask:", (void *)cp_cnoop_i);
        cx_add_cmethod(cp, "setPickerMode:", (void *)cp_cnoop_i);
        ADD(cp, "color", cp_color); ADD(cp, "setColor:", cp_set_color); ADD(cp, "setTarget:", cp_set_target); ADD(cp, "setAction:", cp_set_action);
        ADD(cp, "showsAlpha", cp_shows_alpha); ADD(cp, "setShowsAlpha:", cp_set_shows_alpha); ADD(cp, "alpha", cp_alpha);
        ADD(cp, "isContinuous", cp_continuous); ADD(cp, "setContinuous:", cp_noop_b); ADD(cp, "mode", cp_mode); ADD(cp, "setMode:", cp_noop_i);
    }
    if (cw) {
        ADD(cw, "color", cw_color); ADD(cw, "setColor:", cw_set_color); ADD(cw, "isActive", cw_active);
        ADD(cw, "activate:", cw_activate); ADD(cw, "deactivate", cw_deactivate); ADD(cw, "isBordered", cw_bordered); ADD(cw, "setBordered:", cw_set_bordered);
        ADD(cw, "drawRect:", cw_draw); ADD(cw, "mouseDown:", cw_mouse); ADD(cw, "takeColorFrom:", cw_take); ADD(cw, "drawWellInside:", cw_draw_inside);
        ADD(cw, "supportsAlpha", cw_supports_alpha);
    }
    if (ctl) {
        cx_define_class(&NSDatePicker_c, &NSDatePicker_m, "NSDatePicker", ctl, 0);
        if (acell) cx_define_class(&NSDatePickerCell_c, &NSDatePickerCell_m, "NSDatePickerCell", acell, 0);
        cx_class_t *d = &NSDatePicker_c;
        ADD(d, "initWithFrame:", dp_init); ADD(d, "dateValue", dp_date); ADD(d, "setDateValue:", dp_set_date);
        ADD(d, "objectValue", dp_object_value); ADD(d, "setObjectValue:", dp_set_date);
        ADD(d, "minDate", dp_min); ADD(d, "setMinDate:", dp_set_min); ADD(d, "maxDate", dp_max); ADD(d, "setMaxDate:", dp_set_max);
        ADD(d, "datePickerElements", dp_flags); ADD(d, "setDatePickerElements:", dp_set_flags);
        ADD(d, "datePickerStyle", dp_style); ADD(d, "setDatePickerStyle:", dp_set_style);
        ADD(d, "datePickerMode", dp_mode); ADD(d, "setDatePickerMode:", dp_set_mode);
        ADD(d, "timeInterval", dp_interval); ADD(d, "setTimeInterval:", dp_set_interval);
        ADD(d, "delegate", dp_delegate); ADD(d, "setDelegate:", dp_set_delegate);
        ADD(d, "timeZone", dp_tz); ADD(d, "setTimeZone:", dp_set_tz);
        ADD(d, "calendar", dp_obj_cal); ADD(d, "setCalendar:", dp_noop_obj); ADD(d, "locale", dp_obj_locale); ADD(d, "setLocale:", dp_noop_obj);
        ADD(d, "isBezeled", dp_bezeled); ADD(d, "setBezeled:", dp_set_bezeled); ADD(d, "isBordered", dp_bezeled); ADD(d, "setBordered:", dp_set_bezeled);
        ADD(d, "drawsBackground", dp_draws_bg); ADD(d, "setDrawsBackground:", dp_set_draws_bg);
        ADD(d, "textColor", dp_text_color); ADD(d, "setTextColor:", dp_set_text_color);
        ADD(d, "backgroundColor", dp_bg_color); ADD(d, "setBackgroundColor:", dp_set_bg_color);
        ADD(d, "drawRect:", dp_draw); ADD(d, "mouseDown:", dp_mouse); ADD(d, "keyDown:", dp_key);
        ADD(d, "acceptsFirstResponder", yes_imp); ADD(d, "intrinsicContentSize", dp_intrinsic); ADD(d, "sizeToFit", dp_size_to_fit);
    }
}
