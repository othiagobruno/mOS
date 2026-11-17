/*
 * mOS libSystem — menu flutuante do AppKit: -[NSMenu popUpMenuPositioningItem:atLocation:inView:],
 * +[NSMenu popUpContextMenu:withEvent:forView:(withFont:)], o menu de contexto do clique direito (-[NSView
 * menuForEvent:] / -menu), submenus em cascata, validação (autoenablesItems + validateMenuItem: /
 * validateUserInterfaceItem: pela cadeia de responders) e o envio da ação. Antes nada disso existia: o clique
 * direito só subia pela cadeia e um menu de contexto nunca aparecia.
 *
 * A janela do menu é um NSPanel sem moldura; o laço é o modal do NSApp (o run loop continua rodando). Layout,
 * posição na tela, teclado e rótulo do atalho ficam no núcleo testado no host (appkit_menupop_core.c,
 * userland/tests-appkit/menupop_host.c).
 *
 * Teclas: ↑ ↓ Home End, → abre o submenu, ← volta, Enter/Espaço escolhe, Esc fecha, letra = vai ao item.
 * Delegate do NSMenu: menuNeedsUpdate:, menuWillOpen:, menuDidClose:, menu:willHighlightItem:,
 * numberOfItemsInMenu: + menu:updateItem:atIndex:shouldCancel:.
 * Limite: clicar na janela do menu-pai com um submenu aberto fecha a cascata inteira (o macOS troca de submenu).
 * Registro: cx_appkit_menupop_init(), chamado do cx_appkit_input_init.
 */
#include "foundation.h"
#include "sys.h"
#include "libc.h"
#include "appkit_menupop_core.h"

id objc_msgSend_stret(id self, SEL op, ...);
id objc_retain(id o);
void objc_release(id o);
void objc_setAssociatedObject(id obj, const void *key, id value, u64 policy);
id objc_getAssociatedObject(id obj, const void *key);
IMP class_getMethodImplementation(cx_class_t *c, SEL s);
extern id NSApp;
id cx_color(double r, double g, double b, double a);

typedef struct { double x, y; } P;
typedef struct { double w, h; } S;
typedef struct { double x, y, w, h; } R;
void NSRectFill(R r);
void NSFrameRect(R r);

#define K(N) cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N); cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N);
K(_MOSMenuView) K(_MOSMenuWindowDelegate)
static cx_class_t *kls(const char *n) { return cx_class_named(n); }
static SEL sl(const char *n) { return sel_registerName(n); }
static BOOL responds(id o, const char *s) { return o && cx_responds(object_getClass(o), sl(s)); }
static BOOL responds_sel(id o, SEL s) { return o && s && cx_responds(object_getClass(o), s); }

static R msg_rect(id o, const char *s) { return ((R (*)(id, SEL))objc_msgSend_stret)(o, sl(s)); }
static R msg_rect_r(id o, const char *s, R r) { return ((R (*)(id, SEL, R))objc_msgSend_stret)(o, sl(s), r); }
static S m_size(id s, id attrs) { return ((S (*)(id, SEL, id))objc_msgSend)(s, sl("sizeWithAttributes:"), attrs); }
static void m_draw_at(id s, P p, id attrs) { ((void (*)(id, SEL, P, id))objc_msgSend)(s, sl("drawAtPoint:withAttributes:"), p, attrs); }
static P m_loc(id view, id ev) {
    P w = ((P (*)(id, SEL))objc_msgSend)(ev, sl("locationInWindow"));
    return ((P (*)(id, SEL, P, id))objc_msgSend)(view, sl("convertPoint:fromView:"), w, (id)0);
}
static id m_init_frame(id k, R r) { return ((id (*)(id, SEL, R))objc_msgSend)(MSG0(id, k, "alloc"), sl("initWithFrame:"), r); }

/* ---------------------------------------------------------------- estado por NSMenu (associado) */
static char k_autoen, k_minw, k_font, k_hl, k_state_col;
static BOOL menu_autoenables(id m) { id v = objc_getAssociatedObject(m, &k_autoen); return v ? MSG0(BOOL, v, "boolValue") : YES; }
static void mn_set_autoenables(id self, SEL c, BOOL b) { (void)c; objc_setAssociatedObject(self, &k_autoen, cx_num_i(b ? 1 : 0), 1); }
static BOOL mn_autoenables(id self, SEL c) { (void)c; return menu_autoenables(self); }
static double mn_min_width(id self, SEL c) { (void)c; id v = objc_getAssociatedObject(self, &k_minw); return v ? MSG0(double, v, "doubleValue") : 0; }
static void mn_set_min_width(id self, SEL c, double w) { (void)c; objc_setAssociatedObject(self, &k_minw, cx_num_d(w), 1); }
static id mn_font(id self, SEL c) { (void)c; id f = objc_getAssociatedObject(self, &k_font); return f ? f : MSG(id, (id)kls("NSFont"), "menuFontOfSize:", 0.0); }
static void mn_set_font(id self, SEL c, id f) { (void)c; objc_setAssociatedObject(self, &k_font, f, 1); }
static id mn_highlighted(id self, SEL c) { (void)c; return objc_getAssociatedObject(self, &k_hl); }
static BOOL mn_shows_state(id self, SEL c) { (void)c; id v = objc_getAssociatedObject(self, &k_state_col); return v ? MSG0(BOOL, v, "boolValue") : YES; }
static void mn_set_shows_state(id self, SEL c, BOOL b) { (void)c; objc_setAssociatedObject(self, &k_state_col, cx_num_i(b), 1); }
static id ft_menu(id k, SEL c, double sz) { (void)k; (void)c; return MSG(id, (id)kls("NSFont"), "systemFontOfSize:", sz > 0 ? sz : 13.0); }

/* ---------------------------------------------------------------- cadeia de responders */
id cx_menu_find_target(SEL a, id from_view) {
    if (!a) return 0;
    id seen[80];
    int ns = 0;
    id w = from_view && responds(from_view, "window") ? MSG0(id, from_view, "window") : 0;
    if (!w && NSApp) w = MSG0(id, NSApp, "keyWindow");
    id r = w && responds(w, "firstResponder") ? MSG0(id, w, "firstResponder") : 0;
    if (!r) r = from_view;
    while (r && ns < 64) {
        for (int i = 0; i < ns; i++) if (seen[i] == r) { r = 0; break; }
        if (!r) break;
        seen[ns++] = r;
        if (responds_sel(r, a)) return r;
        r = responds(r, "nextResponder") ? MSG0(id, r, "nextResponder") : 0;
    }
    id wcl = w && responds(w, "windowController") ? MSG0(id, w, "windowController") : 0;
    id doc = wcl && responds(wcl, "document") ? MSG0(id, wcl, "document") : 0;
    cx_class_t *dcc = kls("NSDocumentController");
    id dc = dcc && cx_responds(dcc->isa, sl("sharedDocumentController")) ? MSG0(id, (id)dcc, "sharedDocumentController") : 0;
    id tail[7] = {w, w && responds(w, "delegate") ? MSG0(id, w, "delegate") : 0, wcl, doc,
                  NSApp, NSApp ? MSG0(id, NSApp, "delegate") : 0, dc};
    for (int i = 0; i < 7; i++) if (responds_sel(tail[i], a)) return tail[i];
    return 0;
}

/* item habilitado, como no Cocoa: com autoenablesItems o alvo decide (validateMenuItem: /
 * validateUserInterfaceItem:); sem ação só o item com submenu fica ativo */
int cx_menu_item_valid(id menu, id item, id from_view) {
    if (!item) return 0;
    if (responds(item, "isSeparatorItem") && MSG0(BOOL, item, "isSeparatorItem")) return 0;
    if (!menu_autoenables(menu)) return responds(item, "isEnabled") ? MSG0(BOOL, item, "isEnabled") : 1;
    /* com autoenablesItems o alvo decide; o setEnabled: do item não conta (Cocoa) */
    SEL a = responds(item, "action") ? MSG0(SEL, item, "action") : 0;
    id sub = responds(item, "submenu") ? MSG0(id, item, "submenu") : 0;
    if (!a) return sub != 0;
    id t = responds(item, "target") ? MSG0(id, item, "target") : 0;
    if (!t) t = cx_menu_find_target(a, from_view);
    if (!t || !responds_sel(t, a)) return 0;
    if (responds(t, "validateMenuItem:")) return MSG(BOOL, t, "validateMenuItem:", item);
    if (responds(t, "validateUserInterfaceItem:")) return MSG(BOOL, t, "validateUserInterfaceItem:", item);
    return 1;
}

int cx_menu_perform(id item, id from_view) {
    if (!item) return 0;
    SEL a = responds(item, "action") ? MSG0(SEL, item, "action") : 0;
    if (!a) return 0;
    id t = responds(item, "target") ? MSG0(id, item, "target") : 0;
    if (!t) t = cx_menu_find_target(a, from_view);
    if (!t || !responds_sel(t, a)) return 0;
    ((void (*)(id, SEL, id))objc_msgSend)(t, a, item);
    return 1;
}

/* ---------------------------------------------------------------- contexto de um menu aberto */
enum { RES_DISMISS = 0, RES_CHOSEN = 1, RES_BACK = 2 };
typedef struct mpctx {
    id menu, from_view, win, view, font, attrs, attrs_hl, attrs_off, attrs_key, attrs_key_hl, attrs_head;
    id item[512];
    char keylab[512][40];
    mp_item_t mi[512];
    mp_menu_t m;
    int hl, pending_open, child_item, popup_mode;
    struct mpctx *parent, *child;
    R frame;
    id chosen;
    BOOL opening_child, closing;
} mpctx_t;
static mpctx_t *g_top;       /* menu aberto mais interno */

static id attrs_for(id font, id color) {
    extern id NSFontAttributeName, NSForegroundColorAttributeName;
    id d = MSG0(id, (id)kls("NSMutableDictionary"), "dictionary");
    MSG(void, d, "setObject:forKey:", font, NSFontAttributeName);
    MSG(void, d, "setObject:forKey:", color, NSForegroundColorAttributeName);
    return objc_retain(d);
}

static id item_title(id it) {
    id t = responds(it, "title") ? MSG0(id, it, "title") : 0;
    return t ? t : cx_str("");
}

static void build(mpctx_t *c) {
    id menu = c->menu;
    id dl = responds(menu, "delegate") ? MSG0(id, menu, "delegate") : 0;
    if (responds(dl, "menuNeedsUpdate:")) MSG(void, dl, "menuNeedsUpdate:", menu);
    if (responds(dl, "numberOfItemsInMenu:") && responds(dl, "menu:updateItem:atIndex:shouldCancel:")) {
        i64 want = MSG(i64, dl, "numberOfItemsInMenu:", menu);
        i64 have = MSG0(i64, menu, "numberOfItems");
        for (; have < want; have++) {
            id it = MSG0(id, MSG0(id, (id)kls("NSMenuItem"), "alloc"), "init");
            MSG(void, menu, "addItem:", it);
            objc_release(it);
        }
        while (have > want && have > 0) MSG(void, menu, "removeItemAtIndex:", (i64)--have);
        for (i64 i = 0; i < want; i++) {
            BOOL cancel = NO;
            BOOL go = ((BOOL (*)(id, SEL, id, id, i64, BOOL))objc_msgSend)(dl, sl("menu:updateItem:atIndex:shouldCancel:"), menu, MSG(id, menu, "itemAtIndex:", i), i, cancel);
            if (!go) break;
        }
    }
    if (responds(dl, "menuWillOpen:")) MSG(void, dl, "menuWillOpen:", menu);
    c->font = mn_font(menu, 0);
    if (c->popup_mode > 0) c->font = MSG(id, (id)kls("NSFont"), "systemFontOfSize:", (double)c->popup_mode);
    c->attrs = attrs_for(c->font, cx_color(0.1, 0.1, 0.1, 1));
    c->attrs_hl = attrs_for(c->font, cx_color(1, 1, 1, 1));
    c->attrs_off = attrs_for(c->font, cx_color(0.1, 0.1, 0.1, 0.35));
    c->attrs_key = attrs_for(c->font, cx_color(0.1, 0.1, 0.1, 0.55));
    c->attrs_key_hl = attrs_for(c->font, cx_color(1, 1, 1, 0.85));
    c->attrs_head = attrs_for(MSG(id, (id)kls("NSFont"), "boldSystemFontOfSize:", 11.0), cx_color(0.45, 0.45, 0.48, 1));
    i64 n = MSG0(i64, menu, "numberOfItems");
    if (n > 512) n = 512;
    c->m.n = 0;
    for (i64 i = 0; i < n; i++) {
        id it = MSG(id, menu, "itemAtIndex:", i);
        mp_item_t *x = &c->mi[i];
        memset(x, 0, sizeof *x);
        c->item[i] = it;
        BOOL sep = responds(it, "isSeparatorItem") && MSG0(BOOL, it, "isSeparatorItem");
        BOOL head = responds(it, "isSectionHeader") && MSG0(BOOL, it, "isSectionHeader");
        x->kind = sep ? MP_SEP : head ? MP_HEADER : MP_ITEM;
        x->hidden = responds(it, "isHidden") && MSG0(BOOL, it, "isHidden");
        if (!x->hidden && responds(it, "isAlternate") && MSG0(BOOL, it, "isAlternate")) x->hidden = 1;  /* alternativo só com ⌥ */
        x->has_sub = responds(it, "submenu") && MSG0(id, it, "submenu") != 0;
        x->enabled = x->kind == MP_ITEM && cx_menu_item_valid(menu, it, c->from_view);
        x->state = responds(it, "state") ? (int)MSG0(i64, it, "state") : 0;
        x->indent = responds(it, "indentationLevel") ? (int)MSG0(i64, it, "indentationLevel") : 0;
        id t = item_title(it);
        x->title_w = m_size(t, x->kind == MP_HEADER ? c->attrs_head : c->attrs).w;
        const char *ts = cx_cstr(t);
        x->first = (char)(ts[0] >= 'A' && ts[0] <= 'Z' ? ts[0] + 32 : ts[0]);
        c->keylab[i][0] = 0;
        id key = responds(it, "keyEquivalent") ? MSG0(id, it, "keyEquivalent") : 0;
        if (key && x->kind == MP_ITEM) {
            u64 mask = responds(it, "keyEquivalentModifierMask") ? MSG0(u64, it, "keyEquivalentModifierMask") : (1u << 20);
            mp_key_label(cx_cstr(key), mask, c->keylab[i], sizeof c->keylab[i]);
            if (c->keylab[i][0]) x->key_w = m_size(cx_str(c->keylab[i]), c->attrs_key).w;
        }
        c->m.n++;
    }
    c->m.it = c->mi;
    mp_defaults(&c->m);
    mp_layout(&c->m, mn_min_width(menu, 0));
}

static void set_hl(mpctx_t *c, int i) {
    if (c->hl == i) return;
    c->hl = i;
    id it = i >= 0 ? c->item[i] : 0;
    objc_setAssociatedObject(c->menu, &k_hl, it, 0);
    id dl = responds(c->menu, "delegate") ? MSG0(id, c->menu, "delegate") : 0;
    if (responds(dl, "menu:willHighlightItem:")) MSG(void, dl, "menu:willHighlightItem:", c->menu, it);
    if (c->view) MSG(void, c->view, "setNeedsDisplay:", (BOOL)YES);
}

static mpctx_t *ctx_of_view(id v) { for (mpctx_t *c = g_top; c; c = c->parent) if (c->view == v) return c; return 0; }
static mpctx_t *ctx_of_win(id w) { for (mpctx_t *c = g_top; c; c = c->parent) if (c->win == w) return c; return 0; }

/* ---------------------------------------------------------------- desenho */
static void mv_draw(id self, SEL sc, R dirty) {
    (void)sc; (void)dirty;
    mpctx_t *c = ctx_of_view(self);
    if (!c) return;
    double W = c->m.w, H = c->m.h;
    MSG0(void, cx_color(0.965, 0.965, 0.97, 1), "setFill");
    NSRectFill((R){0, 0, W, H});
    MSG0(void, cx_color(0, 0, 0, 0.18), "setFill");
    NSFrameRect((R){0, 0, W, H});
    BOOL state_col = mn_shows_state(c->menu, 0);
    for (int i = 0; i < c->m.n; i++) {
        mp_item_t *x = &c->mi[i];
        if (x->hidden) continue;
        double y = c->m.y[i], h = c->m.ih[i];
        if (x->kind == MP_SEP) { MSG0(void, cx_color(0, 0, 0, 0.12), "setFill"); NSRectFill((R){10, y + 5, W - 20, 1}); continue; }
        id t = item_title(c->item[i]);
        if (x->kind == MP_HEADER) { m_draw_at(t, (P){12, y + 4}, c->attrs_head); continue; }
        BOOL hl = i == c->hl && x->enabled;
        if (hl) { MSG0(void, cx_color(0.0, 0.40, 0.86, 1), "setFill"); NSRectFill((R){5, y, W - 10, h}); }
        id a = hl ? c->attrs_hl : x->enabled ? c->attrs : c->attrs_off;
        if (state_col && x->state) m_draw_at(cx_str(x->state > 0 ? "\xE2\x9C\x93" : "\xE2\x80\x93"), (P){8, y + 3}, a);
        id img = responds(c->item[i], "image") ? MSG0(id, c->item[i], "image") : 0;
        double tx = c->m.left + x->indent * 10.0;
        if (img) {
            R ir = {tx, y + 3, 16, 16};
            ((void (*)(id, SEL, R, R, u64, double))objc_msgSend)(img, sl("drawInRect:fromRect:operation:fraction:"), ir, (R){0, 0, 0, 0}, (u64)2, x->enabled ? 1.0 : 0.4);
            tx += 20;
        }
        m_draw_at(t, (P){tx, y + 3}, a);
        if (c->keylab[i][0]) {
            id k = cx_str(c->keylab[i]);
            double kw = m_size(k, c->attrs_key).w;
            m_draw_at(k, (P){W - c->m.right_min - kw, y + 3}, hl ? c->attrs_key_hl : x->enabled ? c->attrs_key : c->attrs_off);
        }
        if (x->has_sub) m_draw_at(cx_str("\xE2\x96\xB8"), (P){W - 16, y + 3}, a);
    }
}
static BOOL yes_imp(id s, SEL c) { (void)s; (void)c; return YES; }

/* ---------------------------------------------------------------- abrir / fechar */
static int run_ctx(mpctx_t *c, R frame);

static void finish(mpctx_t *c, int code) {
    if (c->closing) return;
    c->closing = YES;
    MSG(void, NSApp, "stopModalWithCode:", (i64)code);
}

static R screen_frame(void) {
    id scr = MSG0(id, (id)kls("NSScreen"), "mainScreen");
    return scr ? msg_rect(scr, "frame") : (R){0, 0, 1440, 900};
}

static void open_child(mpctx_t *c, int i) {
    for (;;) {
        id sub = MSG0(id, c->item[i], "submenu");
        if (!sub) return;
        mpctx_t *k = calloc(1, sizeof *k);
        if (!k) return;
        k->menu = sub; k->from_view = c->from_view; k->parent = c; k->hl = -1; k->pending_open = -1;
        c->child = k; c->child_item = i; c->pending_open = -1;
        build(k);
        mp_rect pr = {c->frame.x, c->frame.y, c->frame.w, c->frame.h}, s0;
        R s = screen_frame();
        s0 = (mp_rect){s.x, s.y, s.w, s.h};
        mp_rect r = mp_place_sub(&k->m, pr, c->m.y[i], s0);
        c->opening_child = YES;
        int res = run_ctx(k, (R){r.x, r.y, r.w, r.h});
        c->opening_child = NO;
        c->child = 0;
        id chosen = k->chosen;
        free(k);
        if (res == RES_CHOSEN) { c->chosen = chosen; finish(c, RES_CHOSEN); return; }
        if (res == RES_DISMISS) { finish(c, RES_DISMISS); return; }
        if (c->win) MSG(void, c->win, "makeKeyAndOrderFront:", (id)0);
        if (c->pending_open >= 0 && c->pending_open != i && c->mi[c->pending_open].has_sub) { i = c->pending_open; continue; }
        return;
    }
}

static void hover(mpctx_t *c, int i) {
    if (i == c->hl && !(c->child && c->child_item != i)) return;
    set_hl(c, i);
    if (c->child) {                      /* outro item: fecha o submenu aberto (e abre o novo depois) */
        if (c->child_item != i) { c->pending_open = i; finish(c->child, RES_BACK); }
        return;
    }
    if (i >= 0 && c->mi[i].has_sub && c->mi[i].enabled) open_child(c, i);
}

static void choose(mpctx_t *c, int i) {
    if (!mp_selectable(&c->m, i)) return;
    if (c->mi[i].has_sub) { if (!c->child) open_child(c, i); return; }
    c->chosen = c->item[i];
    finish(c, RES_CHOSEN);
}

/* ---------------------------------------------------------------- eventos */
static void mv_moved(id self, SEL sc, id e) {
    (void)sc;
    mpctx_t *c = ctx_of_view(self);
    if (!c) return;
    P p = m_loc(self, e);
    int i = mp_hit(&c->m, p.x, p.y);
    hover(c, mp_selectable(&c->m, i) ? i : (c->child ? c->hl : -1));
}
static void mv_down(id self, SEL sc, id e) {
    mpctx_t *c = ctx_of_view(self);
    if (!c) return;
    mv_moved(self, sc, e);
}
static void mv_up(id self, SEL sc, id e) {
    (void)sc;
    mpctx_t *c = ctx_of_view(self);
    if (!c) return;
    P p = m_loc(self, e);
    int i = mp_hit(&c->m, p.x, p.y);
    if (i >= 0) choose(c, i);
}
static void mv_key(id self, SEL sc, id e) {
    (void)sc;
    mpctx_t *c = ctx_of_view(self);
    if (!c) return;
    u16 code = MSG0(u16, e, "keyCode");
    switch (code) {
    case 126: set_hl(c, mp_step(&c->m, c->hl, -1)); return;
    case 125: set_hl(c, mp_step(&c->m, c->hl, 1)); return;
    case 115: set_hl(c, mp_step(&c->m, -1, 1)); return;
    case 119: set_hl(c, mp_step(&c->m, -1, -1)); return;
    case 124: if (c->hl >= 0 && c->mi[c->hl].has_sub) open_child(c, c->hl); return;
    case 123: if (c->parent) finish(c, RES_BACK); return;
    case 53: finish(c, c->parent ? RES_BACK : RES_DISMISS); return;
    case 36: case 76: case 49: if (c->hl >= 0) choose(c, c->hl); return;
    }
    id ch = MSG0(id, e, "characters");
    const char *s = ch ? cx_cstr(ch) : "";
    if ((unsigned char)s[0] > ' ' && (unsigned char)s[0] < 0x7F) { int i = mp_type_select(&c->m, c->hl, s[0]); if (i >= 0) set_hl(c, i); }
}
static void mv_scroll(id self, SEL sc, id e) { (void)self; (void)sc; (void)e; }

/* a janela do menu perdeu o foco: clique fora (ou o submenu abriu) */
static void wd_resign(id self, SEL sc, id note) {
    (void)self; (void)sc;
    id w = note && responds(note, "object") ? MSG0(id, note, "object") : 0;
    mpctx_t *c = ctx_of_win(w);
    if (!c || c->opening_child || c->closing) return;
    finish(c, RES_DISMISS);
}

static int run_ctx(mpctx_t *c, R frame) {
    static id dlg;
    if (!dlg) dlg = cx_alloc_instance(&_MOSMenuWindowDelegate_c);
    c->frame = frame;
    id w = ((id (*)(id, SEL, R, u64, u64, BOOL))objc_msgSend)(MSG0(id, (id)kls("NSPanel"), "alloc"),
        sl("initWithContentRect:styleMask:backing:defer:"), frame, (u64)0, (u64)2, NO);
    MSG(void, w, "setReleasedWhenClosed:", (BOOL)NO);
    MSG(void, w, "setHasShadow:", (BOOL)YES);
    MSG(void, w, "setDelegate:", dlg);
    id v = m_init_frame((id)&_MOSMenuView_c, (R){0, 0, frame.w, frame.h});
    MSG(void, w, "setContentView:", v);
    id ta = MSG0(id, (id)kls("NSTrackingArea"), "alloc");
    if (ta) {
        ta = ((id (*)(id, SEL, R, u64, id, id))objc_msgSend)(ta, sl("initWithRect:options:owner:userInfo:"), (R){0, 0, frame.w, frame.h}, (u64)(0x01 | 0x02 | 0x80 | 0x200), v, (id)0);
        if (ta) { MSG(void, v, "addTrackingArea:", ta); objc_release(ta); }
    }
    c->win = w; c->view = v;
    mpctx_t *prev = g_top;
    g_top = c;
    MSG(void, w, "makeFirstResponder:", v);
    id nc = MSG0(id, (id)kls("NSNotificationCenter"), "defaultCenter");
    MSG(void, nc, "postNotificationName:object:", cx_str("NSMenuDidBeginTrackingNotification"), c->menu);
    i64 r = MSG(i64, NSApp, "runModalForWindow:", w);
    g_top = prev;
    MSG(void, nc, "postNotificationName:object:", cx_str("NSMenuDidEndTrackingNotification"), c->menu);
    MSG(void, w, "orderOut:", (id)0);
    MSG0(void, w, "close");
    c->win = c->view = 0;
    objc_release(w);
    objc_release(v);
    objc_setAssociatedObject(c->menu, &k_hl, 0, 0);
    id dl = responds(c->menu, "delegate") ? MSG0(id, c->menu, "delegate") : 0;
    if (responds(dl, "menuDidClose:")) MSG(void, dl, "menuDidClose:", c->menu);
    id rel[6] = {c->attrs, c->attrs_hl, c->attrs_off, c->attrs_key, c->attrs_key_hl, c->attrs_head};
    for (int i = 0; i < 6; i++) objc_release(rel[i]);
    if (r == RES_CHOSEN || r == RES_BACK) return (int)r;
    return RES_DISMISS;
}

/* menu aberto agora (GetMenuTrackingData do Carbon): o menu mais interno, o item destacado (-1 nenhum), a moldura
 * (Cocoa, na tela) e o topo/altura do item destacado medidos do topo do menu. 0 se nenhum menu está aberto. */
int cx_menu_tracking_info(id *menu, int *hl, double frame[4], double *item_top, double *item_h) {
    mpctx_t *c = g_top;
    if (!c || !c->win) return 0;
    if (menu) *menu = c->menu;
    if (hl) *hl = c->hl;
    if (frame) { frame[0] = c->frame.x; frame[1] = c->frame.y; frame[2] = c->frame.w; frame[3] = c->frame.h; }
    int i = c->hl >= 0 && c->hl < c->m.n ? c->hl : -1;
    if (item_top) *item_top = i >= 0 ? c->m.y[i] : 0;
    if (item_h) *item_h = i >= 0 ? c->m.ih[i] : 0;
    return 1;
}

/* API interna: abre o menu e devolve o item escolhido (sem enviar a ação).
 * pos = índice do item alinhado ao ponto (-1: topo), mode = MP_PLACE_*, pt na tela (Cocoa),
 * min_w = largura mínima (pop-up), font_size > 0 força o tamanho da fonte. */
id cx_menu_popup_choose(id menu, int pos, P pt, int mode, double min_w, double font_size, id from_view) {
    if (!menu || !NSApp) return 0;
    mpctx_t *c = calloc(1, sizeof *c);
    if (!c) return 0;
    c->menu = menu; c->from_view = from_view; c->hl = -1; c->pending_open = -1;
    c->popup_mode = font_size > 0 ? (int)font_size : 0;
    if (min_w > 0) mn_set_min_width(menu, 0, min_w > mn_min_width(menu, 0) ? min_w : mn_min_width(menu, 0));
    build(c);
    if (min_w > 0 && c->m.w < min_w) { c->m.w = min_w; }
    if (c->m.n == 0) { free(c); return 0; }
    R s = screen_frame();
    mp_rect r = mp_place(&c->m, pos, pt.x, pt.y, (mp_rect){s.x, s.y, s.w, s.h}, mode);
    if (pos >= 0 && mp_selectable(&c->m, pos)) c->hl = pos;
    int res = run_ctx(c, (R){r.x, r.y, r.w, r.h});
    id chosen = res == RES_CHOSEN ? c->chosen : 0;
    free(c);
    return chosen;
}

static P view_to_screen(id view, P p) {
    if (!view) return p;
    P w = ((P (*)(id, SEL, P, id))objc_msgSend)(view, sl("convertPoint:toView:"), p, (id)0);
    id win = MSG0(id, view, "window");
    if (!win) return w;
    R r = msg_rect_r(win, "convertRectToScreen:", (R){w.x, w.y, 0, 0});
    return (P){r.x, r.y};
}

/* ---------------------------------------------------------------- métodos do NSMenu / NSView */
static BOOL mn_popup_positioning(id self, SEL sc, id item, P loc, id view) {
    (void)sc;
    i64 pos = item ? MSG(i64, self, "indexOfItem:", item) : -1;
    P sp = view_to_screen(view, loc);
    if (view && MSG0(BOOL, view, "isFlipped")) {   /* em view invertida, loc.y é o topo de cima para baixo: o ponto já é o topo */ }
    id ch = cx_menu_popup_choose(self, (int)pos, sp, MP_PLACE_POSITION, 0, 0, view);
    if (!ch) return NO;
    cx_menu_perform(ch, view);
    return YES;
}
static void mn_context(id k, SEL sc, id menu, id ev, id view) {
    (void)k; (void)sc;
    if (!menu) return;
    P sp = {0, 0};
    if (ev && view) sp = view_to_screen(MSG0(id, view, "window") ? MSG0(id, MSG0(id, view, "window"), "contentView") : view,
                                        ((P (*)(id, SEL))objc_msgSend)(ev, sl("locationInWindow")));
    else sp = ((P (*)(id, SEL))objc_msgSend)((id)kls("NSEvent"), sl("mouseLocation"));
    id ch = cx_menu_popup_choose(menu, -1, sp, MP_PLACE_CONTEXT, 0, 0, view);
    if (ch) cx_menu_perform(ch, view);
}
static void mn_context_font(id k, SEL sc, id menu, id ev, id view, id font) { (void)font; mn_context(k, sc, menu, ev, view); }
static void mn_cancel(id self, SEL sc) { (void)sc; for (mpctx_t *c = g_top; c; c = c->parent) if (c->menu == self) { finish(c, RES_DISMISS); return; } }
static void mn_update(id self, SEL sc) {
    (void)sc;
    if (!menu_autoenables(self)) return;
    i64 n = MSG0(i64, self, "numberOfItems");
    for (i64 i = 0; i < n; i++) {
        id it = MSG(id, self, "itemAtIndex:", i);
        if (responds(it, "isSeparatorItem") && MSG0(BOOL, it, "isSeparatorItem")) continue;
        if (!(responds(it, "action") && MSG0(SEL, it, "action"))) continue;
        MSG(void, it, "setEnabled:", (BOOL)(cx_menu_item_valid(self, it, 0) ? YES : NO));
    }
}
static void mn_perform_index(id self, SEL sc, i64 i) { (void)sc; cx_menu_perform(MSG(id, self, "itemAtIndex:", i), 0); }
static S mn_size(id self, SEL sc) {
    (void)sc;
    mpctx_t *c = calloc(1, sizeof *c);
    if (!c) return (S){0, 0};
    c->menu = self; c->hl = -1;
    build(c);
    S z = {c->m.w, c->m.h};
    id rel[6] = {c->attrs, c->attrs_hl, c->attrs_off, c->attrs_key, c->attrs_key_hl, c->attrs_head};
    for (int i = 0; i < 6; i++) objc_release(rel[i]);
    free(c);
    return z;
}
static double mn_bar_height(id self, SEL sc) { (void)self; (void)sc; return 24; }

/* clique direito: -menuForEvent: (padrão: -menu) e o menu de contexto; sem menu, sobe pela cadeia como antes */
static IMP g_right_super;
static id v_menu_for_event(id self, SEL sc, id e) { (void)sc; (void)e; return responds(self, "menu") ? MSG0(id, self, "menu") : 0; }
static void v_right_down(id self, SEL sc, id e) {
    id m = responds(self, "menuForEvent:") ? MSG(id, self, "menuForEvent:", e) : 0;
    if (m) {
        if (responds(self, "willOpenMenu:withEvent:")) MSG(void, self, "willOpenMenu:withEvent:", m, e);
        mn_context(0, 0, m, e, self);
        if (responds(self, "didCloseMenu:withEvent:")) MSG(void, self, "didCloseMenu:withEvent:", m, e);
        return;
    }
    if (g_right_super) ((void (*)(id, SEL, id))g_right_super)(self, sc, e);
}

/* ⌃clique (botão esquerdo com Control): o AppKit pede -menuForEvent: como no clique direito, da view clicada para cima;
 * havendo menu, abre o menu de contexto e a view não recebe o mouseDown:. Devolve 1 se abriu (appkit.c, dispatch). */
int cx_ctrl_click_menu(id view, id e) {
    for (id v = view; v; v = responds(v, "superview") ? MSG0(id, v, "superview") : 0) {
        id m = responds(v, "menuForEvent:") ? MSG(id, v, "menuForEvent:", e) : 0;
        if (!m) continue;
        if (responds(v, "willOpenMenu:withEvent:")) MSG(void, v, "willOpenMenu:withEvent:", m, e);
        mn_context(0, 0, m, e, v);
        if (responds(v, "didCloseMenu:withEvent:")) MSG(void, v, "didCloseMenu:withEvent:", m, e);
        return 1;
    }
    return 0;
}

void cx_appkit_menupop_init(void) {
    cx_class_t *view = kls("NSView"), *obj = kls("NSObject"), *menu = kls("NSMenu");
    if (!view || !menu) return;
    cx_define_class(&_MOSMenuView_c, &_MOSMenuView_m, "_MOSMenuView", view, 0);
    cx_add_method(&_MOSMenuView_c, "drawRect:", (void *)mv_draw);
    cx_add_method(&_MOSMenuView_c, "isFlipped", (void *)yes_imp);
    cx_add_method(&_MOSMenuView_c, "acceptsFirstResponder", (void *)yes_imp);
    cx_add_method(&_MOSMenuView_c, "acceptsFirstMouse:", (void *)yes_imp);
    cx_add_method(&_MOSMenuView_c, "mouseMoved:", (void *)mv_moved);
    cx_add_method(&_MOSMenuView_c, "mouseDragged:", (void *)mv_moved);
    cx_add_method(&_MOSMenuView_c, "mouseEntered:", (void *)mv_moved);
    cx_add_method(&_MOSMenuView_c, "mouseDown:", (void *)mv_down);
    cx_add_method(&_MOSMenuView_c, "mouseUp:", (void *)mv_up);
    cx_add_method(&_MOSMenuView_c, "keyDown:", (void *)mv_key);
    cx_add_method(&_MOSMenuView_c, "scrollWheel:", (void *)mv_scroll);
    cx_add_method(&_MOSMenuView_c, "rightMouseDown:", (void *)mv_down);
    cx_add_method(&_MOSMenuView_c, "rightMouseUp:", (void *)mv_up);
    cx_define_class(&_MOSMenuWindowDelegate_c, &_MOSMenuWindowDelegate_m, "_MOSMenuWindowDelegate", obj, 0);
    cx_add_method(&_MOSMenuWindowDelegate_c, "windowDidResignKey:", (void *)wd_resign);

    cx_add_method(menu, "popUpMenuPositioningItem:atLocation:inView:", (void *)mn_popup_positioning);
    cx_add_cmethod(menu, "popUpContextMenu:withEvent:forView:", (void *)mn_context);
    cx_add_cmethod(menu, "popUpContextMenu:withEvent:forView:withFont:", (void *)mn_context_font);
    cx_add_cmethod(menu, "menuBarHeight", (void *)mn_bar_height);
    cx_add_method(menu, "cancelTracking", (void *)mn_cancel);
    cx_add_method(menu, "cancelTrackingWithoutAnimation", (void *)mn_cancel);
    cx_add_method(menu, "update", (void *)mn_update);
    cx_add_method(menu, "performActionForItemAtIndex:", (void *)mn_perform_index);
    cx_add_method(menu, "size", (void *)mn_size);
    cx_add_method(menu, "autoenablesItems", (void *)mn_autoenables);
    cx_add_method(menu, "setAutoenablesItems:", (void *)mn_set_autoenables);
    cx_add_method(menu, "minimumWidth", (void *)mn_min_width);
    cx_add_method(menu, "setMinimumWidth:", (void *)mn_set_min_width);
    cx_add_method(menu, "font", (void *)mn_font);
    cx_add_method(menu, "setFont:", (void *)mn_set_font);
    cx_add_method(menu, "highlightedItem", (void *)mn_highlighted);
    cx_add_method(menu, "showsStateColumn", (void *)mn_shows_state);
    cx_add_method(menu, "setShowsStateColumn:", (void *)mn_set_shows_state);
    cx_class_t *font = kls("NSFont");
    if (font && !cx_responds(font->isa, sl("menuFontOfSize:"))) cx_add_cmethod(font, "menuFontOfSize:", (void *)ft_menu);
    if (font && !cx_responds(font->isa, sl("menuBarFontOfSize:"))) cx_add_cmethod(font, "menuBarFontOfSize:", (void *)ft_menu);
    g_right_super = cx_lookup(view, sl("rightMouseDown:"));
    if (!cx_responds(view, sl("menuForEvent:"))) cx_add_method(view, "menuForEvent:", (void *)v_menu_for_event);
    cx_add_method(view, "rightMouseDown:", (void *)v_right_down);
}
