/*
 * mOS libSystem — NSToolbar completa, por cima da base do appkit_postico.c (itens, delegate, view hospedeira).
 *   Layout: mínimo/máximo, espaços flexíveis, itens centrados (centeredItemIdentifiers) e transbordo por
 *     visibilityPriority com o botão » (menu dos itens que saíram); núcleo appkit_toolbar_core.c, teste
 *     userland/tests-appkit/toolbar_host.c.
 *   Clique: alvo do item ou, sem alvo, a cadeia de responders (como o Cocoa); item selecionável
 *     (toolbarSelectableItemIdentifiers:) fica marcado e vira selectedItemIdentifier.
 *   Validação: autovalidates + validateToolbarItem:/validateUserInterfaceItem: e a ação resolvível, recalculada
 *     junto da barra de menus; validateVisibleItems.
 *   Itens padrão (separador, espaço, barra lateral, imprimir, cores, fontes, personalizar) com rótulo e ação.
 *   Classes novas: NSToolbarItemGroup (subitens num controle segmentado), NSMenuToolbarItem (menu ao clicar),
 *     NSSearchToolbarItem (campo de busca), NSTrackingSeparatorToolbarItem, NSSharingServicePickerToolbarItem.
 *   Personalização: runCustomizationPalette: (lista dos itens permitidos, Restaurar padrão), configuração
 *     salva no NSUserDefaults (autosavesConfiguration), configurationDictionary.
 *   NSWindow: toggleToolbarShown:, runToolbarCustomizationPalette:.
 * Registro: cx_appkit_toolbar_init(), chamado do cx_appkit_input_init depois do appkit_postico.
 */
#include "foundation.h"
#include "sys.h"
#include "libc.h"
#include "appkit_toolbar_core.h"
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

typedef struct { double x, y; } P;
typedef struct { double w, h; } S;
typedef struct { double x, y, w, h; } R;
void NSRectFill(R r);
id cx_menu_popup_choose(id menu, int pos, P pt, int mode, double min_w, double font_size, id from_view);
id cx_menu_find_target(SEL a, id from_view);
int cx_menu_perform(id item, id from_view);

#define K(N) cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N); cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N);
K(NSToolbarItemGroup) K(NSMenuToolbarItem) K(NSSearchToolbarItem) K(NSTrackingSeparatorToolbarItem) K(NSSharingServicePickerToolbarItem)
K(_MOSToolbarCustomTarget)

static cx_class_t *kls(const char *n) { return cx_class_named(n); }
static SEL sl(const char *n) { return sel_registerName(n); }
static BOOL responds(id o, const char *s) { return o && cx_responds(object_getClass(o), sl(s)); }
static BOOL responds_sel(id o, SEL s) { return o && s && cx_responds(object_getClass(o), s); }
static R msg_rect(id o, const char *s) { return ((R (*)(id, SEL))objc_msgSend_stret)(o, sl(s)); }
static R msg_rect_r(id o, const char *s, R r) { return ((R (*)(id, SEL, R))objc_msgSend_stret)(o, sl(s), r); }
static S msg_size(id o, const char *s) { return ((S (*)(id, SEL))objc_msgSend)(o, sl(s)); }
static void m_set_frame(id v, R r) { ((void (*)(id, SEL, R))objc_msgSend)(v, sl("setFrame:"), r); }
static S m_tsize(id s, id attrs) { return ((S (*)(id, SEL, id))objc_msgSend)(s, sl("sizeWithAttributes:"), attrs); }
static void m_draw_at(id s, P p, id attrs) { ((void (*)(id, SEL, P, id))objc_msgSend)(s, sl("drawAtPoint:withAttributes:"), p, attrs); }
static const char *cs(id s) { return s && cx_is_str(s) ? cx_cstr(s) : ""; }
static BOOL eq(id s, const char *t) { return !strcmp(cs(s), t); }
static id attrs(double size, double r, double g, double b, double a) {
    extern id NSFontAttributeName, NSForegroundColorAttributeName;
    id d = MSG0(id, (id)kls("NSMutableDictionary"), "dictionary");
    id font = MSG(id, (id)kls("NSFont"), "systemFontOfSize:", size);
    MSG(void, d, "setObject:forKey:", font, NSFontAttributeName);
    MSG(void, d, "setObject:forKey:", cx_color(r, g, b, a), NSForegroundColorAttributeName);
    return d;
}
static void fill(R r, double R_, double G, double B, double A) { MSG0(void, cx_color(R_, G, B, A), "setFill"); NSRectFill(r); }
#define BAR_H 28.0
#define BAR_H_LABELS 52.0
/* altura da faixa: no modo ícone e texto (NSToolbarDisplayModeIconAndLabel 1) com algum item de imagem, 52 pt (ícone em
 * cima, rótulo embaixo); nos outros modos, 28 pt */
static int tb_build(id tb, double width, id *objs, tbk_item *spec, tbk_out *out, double *chev);
double cx_toolbar_bar_height(id tb) {
    if (!tb || !cx_responds(object_getClass(tb), sel_registerName("displayMode")) || MSG0(i64, tb, "displayMode") != 1) return BAR_H;
    id items = cx_responds(object_getClass(tb), sel_registerName("items")) ? MSG0(id, tb, "items") : 0;
    for (u64 i = 0, n = items ? MSG0(u64, items, "count") : 0; i < n; i++) {
        id it = MSG(id, items, "objectAtIndex:", i);
        if (cx_responds(object_getClass(it), sel_registerName("image")) && MSG0(id, it, "image")) return BAR_H_LABELS;
    }
    return BAR_H;
}
#define CHEV_W 24.0
#define TBMAX 128

/* ================================================================ estado (associado) */
static char k_host, k_host_tb, k_auto_off, k_restored, k_palette, k_group_seg, k_menu_ind;
static IMP g_items_orig, g_insert_orig, g_remove_orig, g_item_init_orig, g_set_visible_orig;

static id tb_items(id tb) { return MSG0(id, tb, "items"); }
static id item_window(id tb) {
    id host = objc_getAssociatedObject(tb, &k_host);
    return host ? MSG0(id, host, "window") : 0;
}

/* ================================================================ validação */
static BOOL item_auto_valid(id item) {
    if (!responds(item, "autovalidates") || !MSG0(BOOL, item, "autovalidates")) return YES;
    SEL a = responds(item, "action") ? MSG0(SEL, item, "action") : 0;
    if (!a) return YES;
    id t = MSG0(id, item, "target");
    id tb = responds(item, "toolbar") ? MSG0(id, item, "toolbar") : 0;
    id win = tb ? item_window(tb) : 0;
    if (!t) t = cx_menu_find_target(a, win ? MSG0(id, win, "firstResponder") : 0);
    if (!t || !responds_sel(t, a)) return NO;
    if (responds(t, "validateToolbarItem:")) return MSG(BOOL, t, "validateToolbarItem:", item);
    if (responds(t, "validateUserInterfaceItem:")) return MSG(BOOL, t, "validateUserInterfaceItem:", item);
    return YES;
}
static void item_validate(id self, SEL c) {
    (void)c;
    BOOL off = !item_auto_valid(self);
    objc_setAssociatedObject(self, &k_auto_off, off ? cx_num_i(1) : 0, 1);
}
static BOOL item_on(id item) { return MSG0(BOOL, item, "isEnabled") && !objc_getAssociatedObject(item, &k_auto_off); }
static BOOL tb_validate_visible(id tb) {
    id items = tb_items(tb);
    u64 n = items ? MSG0(u64, items, "count") : 0;
    BOOL changed = NO;
    for (u64 i = 0; i < n; i++) {
        id it = MSG(id, items, "objectAtIndex:", i);
        BOOL was = objc_getAssociatedObject(it, &k_auto_off) != 0;
        MSG0(void, it, "validate");
        if (was != (objc_getAssociatedObject(it, &k_auto_off) != 0)) changed = YES;
    }
    return changed;
}
static void tb_validate_m(id self, SEL c) { (void)c; if (tb_validate_visible(self)) { id h = objc_getAssociatedObject(self, &k_host); if (h) MSG(void, h, "setNeedsDisplay:", (BOOL)YES); } }
/* chamado com a validação da barra de menus (appkit.c), na janela-chave */
void cx_toolbar_revalidate(id win) {
    id tb = win && responds(win, "toolbar") ? MSG0(id, win, "toolbar") : 0;
    if (tb) tb_validate_m(tb, 0);
}

/* ================================================================ layout */
static BOOL is_sep(id ident) { return eq(ident, "NSToolbarSeparatorItem") || eq(ident, "NSToolbarSidebarTrackingSeparatorItem") || eq(ident, "NSToolbarInspectorTrackingSeparatorItem"); }
static int tb_build(id tb, double width, id *objs, tbk_item *spec, tbk_out *out, double *chev_x) {
    id items = tb_items(tb);
    u64 n = items ? MSG0(u64, items, "count") : 0;
    if (n > TBMAX) n = TBMAX;
    id centered = responds(tb, "centeredItemIdentifiers") ? MSG0(id, tb, "centeredItemIdentifiers") : 0;
    for (u64 i = 0; i < n; i++) {
        id it = MSG(id, items, "objectAtIndex:", i);
        objs[i] = it;
        tbk_item *s = &spec[i];
        memset(s, 0, sizeof *s);
        id ident = MSG0(id, it, "itemIdentifier");
        if (eq(ident, "NSToolbarFlexibleSpaceItem")) s->flexible = 1;
        else if (eq(ident, "NSToolbarSpaceItem")) s->min_w = 12;
        else if (is_sep(ident)) s->min_w = 9;
        else {
            S mn = msg_size(it, "minSize"), mx = msg_size(it, "maxSize");
            s->min_w = mn.w > 1 ? mn.w + 4 : 36;
            s->max_w = mx.w > mn.w ? mx.w + 4 : 0;
        }
        s->priority = (int)MSG0(i64, it, "visibilityPriority");
        s->centered = centered && ident && MSG(BOOL, centered, "containsObject:", ident);
    }
    tbk_layout(spec, (int)n, width, CHEV_W, out, chev_x);
    return (int)n;
}
static BOOL item_selected(id tb, id it) {
    id sel = responds(tb, "selectedItemIdentifier") ? MSG0(id, tb, "selectedItemIdentifier") : 0;
    id ident = MSG0(id, it, "itemIdentifier");
    return sel && ident && MSG(BOOL, sel, "isEqualToString:", ident);
}
static void tb_paint(id tb, R b) {
    id objs[TBMAX]; tbk_item spec[TBMAX]; tbk_out out[TBMAX]; double chev;
    int n = tb_build(tb, b.w, objs, spec, out, &chev);
    fill(b, 0.93, 0.93, 0.94, 1);
    if (!responds(tb, "showsBaselineSeparator") || MSG0(BOOL, tb, "showsBaselineSeparator")) fill((R){b.x, b.y + b.h - 1, b.w, 1}, 0.80, 0.80, 0.82, 1);
    i64 mode = responds(tb, "displayMode") ? MSG0(i64, tb, "displayMode") : 0;
    for (int i = 0; i < n; i++) {
        if (!out[i].visible || out[i].w < 2) continue;
        id it = objs[i];
        id ident = MSG0(id, it, "itemIdentifier");
        R fr = {b.x + out[i].x + 2, b.y + 4, out[i].w - 4, b.h - 8};
        if (is_sep(ident)) { fill((R){b.x + out[i].x + out[i].w / 2, b.y + 5, 1, b.h - 10}, 0, 0, 0, 0.18); continue; }
        if (spec[i].flexible || eq(ident, "NSToolbarSpaceItem") || MSG0(id, it, "view")) continue;   /* a view se desenha */
        BOOL on = item_on(it), sel = item_selected(tb, it);
        if (sel) fill(fr, 0, 0, 0, 0.12);
        else if (MSG0(BOOL, it, "isBordered")) fill(fr, 0.86, 0.86, 0.88, 1);
        id img = mode == 3 ? 0 : MSG0(id, it, "image");
        id label = MSG0(id, it, "label");
        if (!label || !cs(label)[0]) label = responds(it, "title") ? MSG0(id, it, "title") : 0;
        if (img && mode == 1 && b.h >= 40) {     /* ícone e rótulo: ícone em cima, texto pequeno embaixo */
            double s = 22;
            R ir = {fr.x + (fr.w - s) / 2, fr.y + 2, s, s};
            ((void (*)(id, SEL, R, R, u64, double))objc_msgSend)(img, sl("drawInRect:fromRect:operation:fraction:"), ir, (R){0, 0, 0, 0}, (u64)2, on ? 1.0 : 0.35);
            if (label && cs(label)[0]) {
                id a = attrs(10, 0.12, 0.12, 0.14, on ? 1 : 0.35);
                S ts = m_tsize(label, a);
                m_draw_at(label, (P){fr.x + (fr.w - ts.w) / 2, fr.y + fr.h - ts.h - 1}, a);
            }
        } else if (img) {
            double s = fr.h < 18 ? fr.h : 18;
            R ir = {fr.x + (fr.w - s) / 2, fr.y + (fr.h - s) / 2, s, s};
            ((void (*)(id, SEL, R, R, u64, double))objc_msgSend)(img, sl("drawInRect:fromRect:operation:fraction:"), ir, (R){0, 0, 0, 0}, (u64)2, on ? 1.0 : 0.35);
        } else if (label && cs(label)[0]) {
            id a = attrs(12, 0.12, 0.12, 0.14, on ? 1 : 0.35);
            S ts = m_tsize(label, a);
            m_draw_at(label, (P){fr.x + (fr.w - ts.w) / 2, fr.y + (fr.h - ts.h) / 2}, a);
        }
        if (objc_getAssociatedObject(it, &k_menu_ind)) m_draw_at(cx_str("\xE2\x96\xBE"), (P){fr.x + fr.w - 10, fr.y + 4}, attrs(9, 0.2, 0.2, 0.2, on ? 1 : 0.35));
    }
    if (chev >= 0) m_draw_at(cx_str("\xC2\xBB"), (P){b.x + chev + 7, b.y + 5}, attrs(14, 0.2, 0.2, 0.22, 1));
}

/* ================================================================ clique */
static id find_win_first(id tb) { id w = item_window(tb); return w ? MSG0(id, w, "firstResponder") : 0; }
static void send_item(id tb, id it) {
    if (!item_on(it)) return;
    id sel_ids = 0;
    id d = responds(tb, "delegate") ? MSG0(id, tb, "delegate") : 0;
    if (responds(d, "toolbarSelectableItemIdentifiers:")) sel_ids = MSG(id, d, "toolbarSelectableItemIdentifiers:", tb);
    id ident = MSG0(id, it, "itemIdentifier");
    if (sel_ids && ident && MSG(BOOL, sel_ids, "containsObject:", ident)) MSG(void, tb, "setSelectedItemIdentifier:", ident);
    SEL a = MSG0(SEL, it, "action");
    if (!a) return;
    id t = MSG0(id, it, "target");
    if (!t) t = cx_menu_find_target(a, find_win_first(tb));
    if (t && responds_sel(t, a)) ((void (*)(id, SEL, id))objc_msgSend)(t, a, it);
}
static P host_to_screen(id host, P p) {
    P w = ((P (*)(id, SEL, P, id))objc_msgSend)(host, sl("convertPoint:toView:"), p, (id)0);
    id win = MSG0(id, host, "window");
    if (!win) return w;
    R r = msg_rect_r(win, "convertRectToScreen:", (R){w.x, w.y, 0, 0});
    return (P){r.x, r.y};
}
static id menu_form(id it) {
    id m = responds(it, "menuFormRepresentation") ? MSG0(id, it, "menuFormRepresentation") : 0;
    if (m) return m;
    id label = MSG0(id, it, "label");
    if (!label || !cs(label)[0]) label = responds(it, "paletteLabel") ? MSG0(id, it, "paletteLabel") : 0;
    id mi = MSG(id, MSG0(id, (id)kls("NSMenuItem"), "alloc"), "initWithTitle:action:keyEquivalent:", label ? label : cx_str(""), MSG0(SEL, it, "action"), cx_str(""));
    MSG(void, mi, "setTarget:", MSG0(id, it, "target"));
    if (MSG0(id, it, "image")) MSG(void, mi, "setImage:", MSG0(id, it, "image"));
    id sub = responds(it, "menu") ? MSG0(id, it, "menu") : 0;
    if (sub) MSG(void, mi, "setSubmenu:", sub);
    return objc_autorelease(mi);
}
static void tb_click(id tb, id host, double x, double y, double w) {
    (void)y;
    id objs[TBMAX]; tbk_item spec[TBMAX]; tbk_out out[TBMAX]; double chev;
    int n = tb_build(tb, w, objs, spec, out, &chev);
    BOOL flipped = MSG0(BOOL, host, "isFlipped");
    if (chev >= 0 && x >= chev) {   /* »: menu dos itens que não couberam */
        id menu = MSG(id, MSG0(id, (id)kls("NSMenu"), "alloc"), "initWithTitle:", cx_str(""));
        for (int i = 0; i < n; i++) if (!out[i].visible && !spec[i].flexible && !is_sep(MSG0(id, objs[i], "itemIdentifier"))) MSG(void, menu, "addItem:", menu_form(objs[i]));
        id ch = cx_menu_popup_choose(menu, -1, host_to_screen(host, (P){chev, flipped ? cx_toolbar_bar_height(tb) : 0}), MP_PLACE_CONTEXT, 0, 0, find_win_first(tb));
        if (ch) {
            for (int i = 0; i < n; i++) if (!out[i].visible && MSG0(id, ch, "title") && MSG(BOOL, MSG0(id, ch, "title"), "isEqual:", MSG0(id, objs[i], "label"))) { send_item(tb, objs[i]); ch = 0; break; }
            if (ch) cx_menu_perform(ch, find_win_first(tb));
        }
        objc_release(menu);
        return;
    }
    int i = tbk_hit(out, n, x);
    if (i < 0) return;
    id it = objs[i];
    if (MSG0(id, it, "view")) return;
    id menu = responds(it, "menu") ? MSG0(id, it, "menu") : 0;
    if (menu && item_on(it)) {   /* NSMenuToolbarItem: abre o menu embaixo do item */
        id ch = cx_menu_popup_choose(menu, -1, host_to_screen(host, (P){out[i].x, flipped ? cx_toolbar_bar_height(tb) : 0}), MP_PLACE_CONTEXT, out[i].w, 0, find_win_first(tb));
        if (ch) cx_menu_perform(ch, find_win_first(tb));
        return;
    }
    send_item(tb, it);
}

/* ================================================================ view hospedeira */
static void host_draw(id self, SEL c, R d) {
    (void)c; (void)d;
    id tb = objc_getAssociatedObject(self, &k_host_tb);
    if (tb) tb_paint(tb, msg_rect(self, "bounds"));
}
static void host_mouse(id self, SEL c, id e) {
    (void)c;
    id tb = objc_getAssociatedObject(self, &k_host_tb);
    if (!tb || !e) return;
    P w = ((P (*)(id, SEL))objc_msgSend)(e, sl("locationInWindow"));
    P p = ((P (*)(id, SEL, P, id))objc_msgSend)(self, sl("convertPoint:fromView:"), w, (id)0);
    tb_click(tb, self, p.x, p.y, msg_rect(self, "bounds").w);
}
static id tb_host(id self, SEL c, double width) {
    (void)c;
    id host = objc_getAssociatedObject(self, &k_host);
    if (!host) {
        host = ((id (*)(id, SEL, R))objc_msgSend)(MSG0(id, (id)kls("_MOSToolbarView"), "alloc"), sl("initWithFrame:"), (R){0, 0, width, cx_toolbar_bar_height(self)});
        if (!host) return 0;
        objc_setAssociatedObject(host, &k_host_tb, self, 0);
        objc_setAssociatedObject(self, &k_host, host, 1);
        objc_release(host);
    }
    double bar_h = cx_toolbar_bar_height(self);
    m_set_frame(host, (R){0, 0, width, bar_h});
    id objs[TBMAX]; tbk_item spec[TBMAX]; tbk_out out[TBMAX]; double chev;
    int n = tb_build(self, width, objs, spec, out, &chev);
    id subs = MSG0(id, host, "subviews");
    for (u64 k = subs ? MSG0(u64, subs, "count") : 0; k > 0; k--) {
        id sv = MSG(id, subs, "objectAtIndex:", k - 1);
        int keep = 0;
        for (int i = 0; i < n && !keep; i++) keep = out[i].visible && MSG0(id, objs[i], "view") == sv;
        if (!keep) MSG0(void, sv, "removeFromSuperview");
    }
    for (int i = 0; i < n; i++) {
        id v = MSG0(id, objs[i], "view");
        if (!v || !out[i].visible) continue;
        R vf = msg_rect(v, "frame");
        double h = vf.h > 1 && vf.h < 24 ? vf.h : 22;
        if (MSG0(id, v, "superview") != host) MSG(void, host, "addSubview:", v);
        m_set_frame(v, (R){out[i].x + 2, (bar_h - h) / 2, out[i].w - 4 > 1 ? out[i].w - 4 : 1, h});
    }
    return host;
}
static void tb_draw_in_rect(id self, SEL c, R r) { (void)c; if (MSG0(BOOL, self, "isVisible")) tb_paint(self, r); }
static void tb_mouse(id self, SEL c, id e) {
    (void)c;
    id host = objc_getAssociatedObject(self, &k_host);
    if (host) host_mouse(host, 0, e);
}

/* ================================================================ configuração salva */
static id cfg_key(id tb) {
    id ident = responds(tb, "identifier") ? MSG0(id, tb, "identifier") : 0;
    char buf[300];
    snprintf(buf, sizeof buf, "NSToolbar Configuration %s", cs(ident));
    return cx_str(buf);
}
static id tb_config(id self, SEL c) {
    (void)c;
    id ids = MSG0(id, (id)kls("NSMutableArray"), "array");
    id items = tb_items(self);
    u64 n = items ? MSG0(u64, items, "count") : 0;
    for (u64 i = 0; i < n; i++) { id ident = MSG0(id, MSG(id, items, "objectAtIndex:", i), "itemIdentifier"); if (ident) MSG(void, ids, "addObject:", ident); }
    id d = MSG0(id, (id)kls("NSMutableDictionary"), "dictionary");
    MSG(void, d, "setObject:forKey:", ids, cx_str("TB Item Identifiers"));
    MSG(void, d, "setObject:forKey:", cx_num_i(MSG0(i64, self, "displayMode")), cx_str("TB Display Mode"));
    MSG(void, d, "setObject:forKey:", cx_num_i(MSG0(BOOL, self, "isVisible")), cx_str("TB Is Shown"));
    return d;
}
static void tb_save(id tb) {
    if (!responds(tb, "autosavesConfiguration") || !MSG0(BOOL, tb, "autosavesConfiguration")) return;
    id ud = MSG0(id, (id)kls("NSUserDefaults"), "standardUserDefaults");
    if (ud) MSG(void, ud, "setObject:forKey:", tb_config(tb, 0), cfg_key(tb));
}
static void tb_set_ids(id tb, id ids) {
    id items = tb_items(tb);
    for (u64 n = items ? MSG0(u64, items, "count") : 0; n > 0; n--) ((void (*)(id, SEL, i64))g_remove_orig)(tb, sl("removeItemAtIndex:"), (i64)(n - 1));
    u64 m = ids ? MSG0(u64, ids, "count") : 0;
    for (u64 i = 0; i < m; i++) ((void (*)(id, SEL, id, i64))g_insert_orig)(tb, sl("insertItemWithItemIdentifier:atIndex:"), MSG(id, ids, "objectAtIndex:", i), (i64)i);
    id host = objc_getAssociatedObject(tb, &k_host);
    if (host) MSG(void, host, "setNeedsDisplay:", (BOOL)YES);
}
static void tb_set_config(id self, SEL c, id d) {
    (void)c;
    if (!d) return;
    id ids = MSG(id, d, "objectForKey:", cx_str("TB Item Identifiers"));
    if (ids) tb_set_ids(self, ids);
    id mode = MSG(id, d, "objectForKey:", cx_str("TB Display Mode"));
    if (mode) MSG(void, self, "setDisplayMode:", MSG0(i64, mode, "longLongValue"));
    id shown = MSG(id, d, "objectForKey:", cx_str("TB Is Shown"));
    if (shown) MSG(void, self, "setVisible:", MSG0(BOOL, shown, "boolValue"));
    tb_save(self);
}
static id tb_items_hook(id self, SEL c) {
    id items = g_items_orig ? ((id (*)(id, SEL))g_items_orig)(self, c) : 0;
    if (!objc_getAssociatedObject(self, &k_restored) && responds(self, "autosavesConfiguration") && MSG0(BOOL, self, "autosavesConfiguration")) {
        objc_setAssociatedObject(self, &k_restored, cx_num_i(1), 1);
        id ud = MSG0(id, (id)kls("NSUserDefaults"), "standardUserDefaults");
        id cfg = ud ? MSG(id, ud, "objectForKey:", cfg_key(self)) : 0;
        if (cfg) { tb_set_config(self, 0, cfg); items = g_items_orig ? ((id (*)(id, SEL))g_items_orig)(self, c) : 0; }
    }
    return items;
}
static void tb_insert_hook(id self, SEL c, id ident, i64 idx) { if (g_insert_orig) ((void (*)(id, SEL, id, i64))g_insert_orig)(self, c, ident, idx); tb_save(self); }
static void tb_remove_hook(id self, SEL c, i64 idx) { if (g_remove_orig) ((void (*)(id, SEL, i64))g_remove_orig)(self, c, idx); tb_save(self); }
static void tb_set_visible_hook(id self, SEL c, BOOL v) { if (g_set_visible_orig) ((void (*)(id, SEL, BOOL))g_set_visible_orig)(self, c, v); tb_save(self); }

/* ================================================================ personalização */
static id g_pal_checks, g_pal_ids;
static void pal_ok(id s, SEL c, id x) { (void)s; (void)c; (void)x; MSG(void, NSApp, "stopModalWithCode:", (i64)1); }
static void pal_default(id s, SEL c, id x) { (void)s; (void)c; (void)x; MSG(void, NSApp, "stopModalWithCode:", (i64)2); }
static void pal_cancel(id s, SEL c, id x) { (void)s; (void)c; (void)x; MSG(void, NSApp, "stopModalWithCode:", (i64)0); }
static id palette_label(id tb, id ident) {
    if (eq(ident, "NSToolbarFlexibleSpaceItem")) return cx_str("Espa\xC3\xA7o flex\xC3\xADvel");
    if (eq(ident, "NSToolbarSpaceItem")) return cx_str("Espa\xC3\xA7o");
    if (is_sep(ident)) return cx_str("Separador");
    id d = MSG0(id, tb, "delegate");
    id it = responds(d, "toolbar:itemForItemIdentifier:willBeInsertedIntoToolbar:") ? MSG(id, d, "toolbar:itemForItemIdentifier:willBeInsertedIntoToolbar:", tb, ident, (BOOL)NO) : 0;
    id l = it && responds(it, "paletteLabel") ? MSG0(id, it, "paletteLabel") : 0;
    if (!l || !cs(l)[0]) l = it ? MSG0(id, it, "label") : 0;
    return l && cs(l)[0] ? l : ident;
}
static void tb_run_palette(id self, SEL c, id sender) {
    (void)c; (void)sender;
    if (objc_getAssociatedObject(self, &k_palette)) return;
    id d = MSG0(id, self, "delegate");
    id allowed = responds(d, "toolbarAllowedItemIdentifiers:") ? MSG(id, d, "toolbarAllowedItemIdentifiers:", self) : 0;
    id defaults = responds(d, "toolbarDefaultItemIdentifiers:") ? MSG(id, d, "toolbarDefaultItemIdentifiers:", self) : 0;
    if (!allowed) allowed = defaults;
    u64 n = allowed ? MSG0(u64, allowed, "count") : 0;
    if (!n) return;
    objc_setAssociatedObject(self, &k_palette, cx_num_i(1), 1);
    double H = 70 + 24.0 * (double)n;
    id w = ((id (*)(id, SEL, R, u64, u64, BOOL))objc_msgSend)(MSG0(id, (id)kls("NSPanel"), "alloc"), sl("initWithContentRect:styleMask:backing:defer:"), (R){0, 0, 360, H}, (u64)1, (u64)2, NO);
    MSG(void, w, "setTitle:", cx_str("Personalizar barra de ferramentas"));
    id cv = MSG0(id, w, "contentView");
    static id tgt;
    if (!tgt) tgt = cx_alloc_instance(&_MOSToolbarCustomTarget_c);
    g_pal_checks = cx_arr_new(kls("NSMutableArray"));
    g_pal_ids = allowed;
    id cur = tb_config(self, 0);
    id cur_ids = MSG(id, cur, "objectForKey:", cx_str("TB Item Identifiers"));
    for (u64 i = 0; i < n; i++) {
        id ident = MSG(id, allowed, "objectAtIndex:", i);
        id cb = MSG(id, (id)kls("NSButton"), "checkboxWithTitle:target:action:", palette_label(self, ident), (id)0, (SEL)0);
        m_set_frame(cb, (R){20, H - 34 - 24.0 * (double)i, 320, 20});
        BOOL has = MSG(BOOL, cur_ids, "containsObject:", ident);
        MSG(void, cb, "setState:", (i64)(has ? 1 : 0));
        MSG(void, cv, "addSubview:", cb);
        cx_arr_add(g_pal_checks, cb);
    }
    struct { const char *t; const char *s; double x; } bt[3] = {{"Conclu\xC3\xAD" "do", "cxPalOK:", 252}, {"Restaurar padr\xC3\xA3o", "cxPalDefault:", 110}, {"Cancelar", "cxPalCancel:", 16}};
    for (int k = 0; k < 3; k++) {
        id b = MSG(id, (id)kls("NSButton"), "buttonWithTitle:target:action:", cx_str(bt[k].t), tgt, sl(bt[k].s));
        m_set_frame(b, (R){bt[k].x, 14, k == 1 ? 136 : 92, 24});
        if (!k) MSG(void, b, "setKeyEquivalent:", cx_str("\r"));
        MSG(void, cv, "addSubview:", b);
    }
    MSG0(void, w, "center");
    i64 r = MSG(i64, NSApp, "runModalForWindow:", w);
    if (r == 1) {   /* na ordem atual; os novos entram na ordem da lista permitida */
        id ids = MSG0(id, (id)kls("NSMutableArray"), "array");
        u64 nc = MSG0(u64, cur_ids, "count");
        for (u64 i = 0; i < nc; i++) {
            id ident = MSG(id, cur_ids, "objectAtIndex:", i);
            u64 at = (u64)MSG(i64, allowed, "indexOfObject:", ident);
            if (at < n && MSG0(i64, MSG(id, g_pal_checks, "objectAtIndex:", at), "state")) MSG(void, ids, "addObject:", ident);
            else if (at >= n) MSG(void, ids, "addObject:", ident);
        }
        for (u64 i = 0; i < n; i++) {
            id ident = MSG(id, allowed, "objectAtIndex:", i);
            if (MSG0(i64, MSG(id, g_pal_checks, "objectAtIndex:", i), "state") && !MSG(BOOL, ids, "containsObject:", ident)) MSG(void, ids, "addObject:", ident);
        }
        tb_set_ids(self, ids);
        tb_save(self);
    } else if (r == 2 && defaults) { tb_set_ids(self, defaults); tb_save(self); }
    MSG(void, w, "orderOut:", (id)0);
    MSG0(void, w, "close");
    objc_release(g_pal_checks);
    g_pal_checks = 0; g_pal_ids = 0;
    objc_setAssociatedObject(self, &k_palette, 0, 1);
}
static BOOL tb_palette_running(id self, SEL c) { (void)c; return objc_getAssociatedObject(self, &k_palette) != 0; }
static void win_toggle_toolbar(id self, SEL c, id s) {
    (void)c; (void)s;
    id tb = MSG0(id, self, "toolbar");
    if (tb) { MSG(void, tb, "setVisible:", (BOOL)!MSG0(BOOL, tb, "isVisible")); MSG(void, self, "setToolbar:", tb); }
}
static void win_run_palette(id self, SEL c, id s) { (void)c; id tb = MSG0(id, self, "toolbar"); if (tb) tb_run_palette(tb, 0, s); }

/* ================================================================ itens padrão */
static id item_init_hook(id self, SEL c, id ident) {
    self = g_item_init_orig ? ((id (*)(id, SEL, id))g_item_init_orig)(self, c, ident) : self;
    if (!self || !ident) return self;
    static const struct { const char *id, *label, *action; } std[] = {
        {"NSToolbarToggleSidebarItem", "Barra lateral", "toggleSidebar:"},
        {"NSToolbarToggleInspectorItem", "Inspetor", "toggleInspector:"},
        {"NSToolbarPrintItem", "Imprimir", "printDocument:"},
        {"NSToolbarShowColorsItem", "Cores", "orderFrontColorPanel:"},
        {"NSToolbarShowFontsItem", "Fontes", "orderFrontFontPanel:"},
        {"NSToolbarCustomizeToolbarItem", "Personalizar", "runToolbarCustomizationPalette:"},
        {"NSToolbarCloudSharingItem", "Compartilhar", "shareDocument:"},
    };
    for (unsigned i = 0; i < sizeof std / sizeof std[0]; i++) {
        if (!eq(ident, std[i].id)) continue;
        id l = cx_str(std[i].label);
        MSG(void, self, "setLabel:", l);
        MSG(void, self, "setPaletteLabel:", l);
        MSG(void, self, "setAction:", sl(std[i].action));
        break;
    }
    if (eq(ident, "NSToolbarSeparatorItem")) MSG(void, self, "setPaletteLabel:", cx_str("Separador"));
    return self;
}
static BOOL yes_imp(id s, SEL c) { (void)s; (void)c; return YES; }

/* ================================================================ NSToolbarItemGroup */
static char k_subitems, k_gmode, k_grep;
static id grp_subitems(id s, SEL c) { (void)c; id a = objc_getAssociatedObject(s, &k_subitems); return a ? a : MSG0(id, (id)kls("NSArray"), "array"); }
static void grp_seg_click(id self, SEL c, id seg) {
    (void)c;
    id grp = objc_getAssociatedObject(seg, &k_group_seg);
    if (!grp) return;
    id subs = grp_subitems(grp, 0);
    i64 i = MSG0(i64, seg, "selectedSegment");
    if (i < 0 || (u64)i >= MSG0(u64, subs, "count")) return;
    id sub = MSG(id, subs, "objectAtIndex:", (u64)i);
    id tb = MSG0(id, grp, "toolbar");
    SEL a = MSG0(SEL, sub, "action") ? MSG0(SEL, sub, "action") : MSG0(SEL, grp, "action");
    id t = MSG0(SEL, sub, "action") ? MSG0(id, sub, "target") : MSG0(id, grp, "target");
    if (!a) return;
    if (!t) t = cx_menu_find_target(a, tb ? find_win_first(tb) : 0);
    if (t && responds_sel(t, a)) ((void (*)(id, SEL, id))objc_msgSend)(t, a, MSG0(SEL, sub, "action") ? sub : grp);
    (void)self;
}
static void grp_rebuild(id self) {
    id subs = grp_subitems(self, 0);
    u64 n = MSG0(u64, subs, "count");
    id seg = MSG0(id, self, "view");
    if (!seg || !cx_is_subclass(object_getClass(seg), kls("NSSegmentedControl"))) {
        seg = ((id (*)(id, SEL, R))objc_msgSend)(MSG0(id, (id)kls("NSSegmentedControl"), "alloc"), sl("initWithFrame:"), (R){0, 0, 60, 24});
        static id tgt;
        if (!tgt) tgt = cx_alloc_instance(&_MOSToolbarCustomTarget_c);
        MSG(void, seg, "setTarget:", tgt);
        MSG(void, seg, "setAction:", sl("cxGroupSegment:"));
        objc_setAssociatedObject(seg, &k_group_seg, self, 0);
        MSG(void, self, "setView:", seg);
        objc_release(seg);
    }
    id m = objc_getAssociatedObject(self, &k_gmode);
    i64 mode = m ? MSG0(i64, m, "longLongValue") : 0;   /* NSToolbarItemGroupSelectionMode: 0 um, 1 vários, 2 momentâneo */
    MSG(void, seg, "setTrackingMode:", (i64)(mode == 1 ? 1 : mode == 2 ? 2 : 0));
    MSG(void, seg, "setSegmentCount:", (i64)n);
    for (u64 i = 0; i < n; i++) {
        id sub = MSG(id, subs, "objectAtIndex:", i);
        id img = MSG0(id, sub, "image");
        id lab = MSG0(id, sub, "label");
        if (img) MSG(void, seg, "setImage:forSegment:", img, (i64)i);
        else MSG(void, seg, "setLabel:forSegment:", lab ? lab : cx_str(""), (i64)i);
    }
    MSG0(void, seg, "sizeToFit");
    R f = msg_rect(seg, "frame");
    ((void (*)(id, SEL, S))objc_msgSend)(self, sl("setMinSize:"), (S){f.w, 24});
    ((void (*)(id, SEL, S))objc_msgSend)(self, sl("setMaxSize:"), (S){f.w, 24});
}
static void grp_set_subitems(id s, SEL c, id a) { (void)c; objc_setAssociatedObject(s, &k_subitems, a ? MSG0(id, a, "copy") : 0, 1); grp_rebuild(s); }
static i64 grp_mode(id s, SEL c) { (void)c; id m = objc_getAssociatedObject(s, &k_gmode); return m ? MSG0(i64, m, "longLongValue") : 0; }
static void grp_set_mode(id s, SEL c, i64 m) { (void)c; objc_setAssociatedObject(s, &k_gmode, cx_num_i(m), 1); grp_rebuild(s); }
static i64 grp_rep(id s, SEL c) { (void)c; id m = objc_getAssociatedObject(s, &k_grep); return m ? MSG0(i64, m, "longLongValue") : 0; }
static void grp_set_rep(id s, SEL c, i64 m) { (void)c; objc_setAssociatedObject(s, &k_grep, cx_num_i(m), 1); }
static i64 grp_selected_index(id s, SEL c) { (void)c; id v = MSG0(id, s, "view"); return v ? MSG0(i64, v, "selectedSegment") : -1; }
static void grp_set_selected_index(id s, SEL c, i64 i) { (void)c; id v = MSG0(id, s, "view"); if (v) MSG(void, v, "setSelectedSegment:", i); }
static void grp_set_selected(id s, SEL c, BOOL on, i64 i) { (void)c; id v = MSG0(id, s, "view"); if (v) MSG(void, v, "setSelected:forSegment:", on, i); }
static BOOL grp_is_selected(id s, SEL c, i64 i) { (void)c; id v = MSG0(id, s, "view"); return v ? MSG(BOOL, v, "isSelectedForSegment:", i) : NO; }
static id grp_make(id k, id ident, id titles, id images, i64 mode, id labels, id target, SEL action) {
    id g = MSG(id, MSG0(id, k, "alloc"), "initWithItemIdentifier:", ident);
    u64 n = titles ? MSG0(u64, titles, "count") : images ? MSG0(u64, images, "count") : 0;
    id subs = MSG0(id, (id)kls("NSMutableArray"), "array");
    for (u64 i = 0; i < n; i++) {
        char sid[64];
        snprintf(sid, sizeof sid, "%s.%llu", cs(ident), (unsigned long long)i);
        id sub = MSG(id, MSG0(id, (id)kls("NSToolbarItem"), "alloc"), "initWithItemIdentifier:", cx_str(sid));
        id lab = labels && i < MSG0(u64, labels, "count") ? MSG(id, labels, "objectAtIndex:", i) : titles ? MSG(id, titles, "objectAtIndex:", i) : 0;
        if (lab) MSG(void, sub, "setLabel:", lab);
        if (images) { id im = MSG(id, images, "objectAtIndex:", i); MSG(void, sub, "setImage:", im); }
        MSG(void, subs, "addObject:", sub);
        objc_release(sub);
    }
    objc_setAssociatedObject(g, &k_gmode, cx_num_i(mode), 1);
    MSG(void, g, "setTarget:", target);
    MSG(void, g, "setAction:", action);
    grp_set_subitems(g, 0, subs);
    return objc_autorelease(g);
}
static id grp_with_titles(id k, SEL c, id ident, id titles, i64 mode, id labels, id target, SEL action) { (void)c; return grp_make(k, ident, titles, 0, mode, labels, target, action); }
static id grp_with_images(id k, SEL c, id ident, id images, i64 mode, id labels, id target, SEL action) { (void)c; return grp_make(k, ident, 0, images, mode, labels, target, action); }

/* NSMenuToolbarItem */
static char k_menu;
static id mti_menu(id s, SEL c) { (void)c; return objc_getAssociatedObject(s, &k_menu); }
static void mti_set_menu(id s, SEL c, id m) { (void)c; objc_setAssociatedObject(s, &k_menu, m, 1); objc_setAssociatedObject(s, &k_menu_ind, cx_num_i(1), 1); }
static BOOL mti_shows(id s, SEL c) { (void)c; return objc_getAssociatedObject(s, &k_menu_ind) != 0; }
static void mti_set_shows(id s, SEL c, BOOL b) { (void)c; objc_setAssociatedObject(s, &k_menu_ind, b ? cx_num_i(1) : 0, 1); }
/* NSSearchToolbarItem */
static id sti_field(id s, SEL c) {
    (void)c;
    id v = MSG0(id, s, "view");
    if (!v) {
        v = ((id (*)(id, SEL, R))objc_msgSend)(MSG0(id, (id)kls("NSSearchField"), "alloc"), sl("initWithFrame:"), (R){0, 0, 180, 22});
        MSG(void, s, "setView:", v);
        ((void (*)(id, SEL, S))objc_msgSend)(s, sl("setMinSize:"), (S){120, 22});
        ((void (*)(id, SEL, S))objc_msgSend)(s, sl("setMaxSize:"), (S){240, 22});
        objc_release(v);
    }
    return v;
}
static void sti_set_field(id s, SEL c, id f) { (void)c; MSG(void, s, "setView:", f); }
static id sti_init(id self, SEL c, id ident) {
    IMP sup = class_getMethodImplementation(kls("NSToolbarItem"), c);
    self = sup ? ((id (*)(id, SEL, id))sup)(self, c, ident) : self;
    if (self) sti_field(self, 0);
    return self;
}
static void sti_begin(id s, SEL c) { (void)c; id f = sti_field(s, 0); id w = MSG0(id, f, "window"); if (w) MSG(void, w, "makeFirstResponder:", f); }
static void sti_end(id s, SEL c) { (void)c; id f = sti_field(s, 0); id w = MSG0(id, f, "window"); if (w) MSG(void, w, "makeFirstResponder:", (id)0); }
static double sti_pref_w(id s, SEL c) { (void)c; return msg_size(s, "maxSize").w; }
static void sti_set_pref_w(id s, SEL c, double w) { (void)c; ((void (*)(id, SEL, S))objc_msgSend)(s, sl("setMaxSize:"), (S){w, 22}); }
/* NSTrackingSeparatorToolbarItem */
static char k_split, k_div;
static id tsi_make(id k, SEL c, id ident, id split, i64 div) {
    (void)c;
    id it = MSG(id, MSG0(id, k, "alloc"), "initWithItemIdentifier:", ident);
    objc_setAssociatedObject(it, &k_split, split, 0);
    objc_setAssociatedObject(it, &k_div, cx_num_i(div), 1);
    return objc_autorelease(it);
}
static id tsi_split(id s, SEL c) { (void)c; return objc_getAssociatedObject(s, &k_split); }
static void tsi_set_split(id s, SEL c, id v) { (void)c; objc_setAssociatedObject(s, &k_split, v, 0); }
static i64 tsi_div(id s, SEL c) { (void)c; id v = objc_getAssociatedObject(s, &k_div); return v ? MSG0(i64, v, "longLongValue") : 0; }
static void tsi_set_div(id s, SEL c, i64 d) { (void)c; objc_setAssociatedObject(s, &k_div, cx_num_i(d), 1); }
static S tsi_size(id s, SEL c) { (void)s; (void)c; return (S){5, 28}; }
/* NSSharingServicePickerToolbarItem: o mOS não tem serviços de compartilhamento; o item fica sem ação */
static char k_share_dl;
static id sspt_dl(id s, SEL c) { (void)c; return objc_getAssociatedObject(s, &k_share_dl); }
static void sspt_set_dl(id s, SEL c, id d) { (void)c; objc_setAssociatedObject(s, &k_share_dl, d, 0); }

#define ADD(cls, sel, fn) cx_add_method(cls, sel, (void *)(fn))
static void def(cx_class_t *k, cx_class_t *m, const char *name, cx_class_t *sup) { if (!cx_class_named(name)) cx_define_class(k, m, name, sup, 0); }
void cx_appkit_toolbar_init(void) {
    cx_class_t *tb = kls("NSToolbar"), *item = kls("NSToolbarItem"), *host = kls("_MOSToolbarView"), *win = kls("NSWindow"), *obj = kls("NSObject");
    if (!tb || !item) return;
    g_items_orig = cx_lookup(tb, sl("items"));
    g_insert_orig = cx_lookup(tb, sl("insertItemWithItemIdentifier:atIndex:"));
    g_remove_orig = cx_lookup(tb, sl("removeItemAtIndex:"));
    g_set_visible_orig = cx_lookup(tb, sl("setVisible:"));
    g_item_init_orig = cx_lookup(item, sl("initWithItemIdentifier:"));
    ADD(tb, "items", tb_items_hook); ADD(tb, "insertItemWithItemIdentifier:atIndex:", tb_insert_hook);
    ADD(tb, "removeItemAtIndex:", tb_remove_hook); ADD(tb, "setVisible:", tb_set_visible_hook);
    ADD(tb, "_mosHostViewWithWidth:", tb_host); ADD(tb, "drawInRect:", tb_draw_in_rect); ADD(tb, "mouseDown:", tb_mouse);
    ADD(tb, "validateVisibleItems", tb_validate_m);
    ADD(tb, "configurationDictionary", tb_config); ADD(tb, "setConfigurationFromDictionary:", tb_set_config);
    ADD(tb, "runCustomizationPalette:", tb_run_palette); ADD(tb, "customizationPaletteIsRunning", tb_palette_running);
    ADD(item, "initWithItemIdentifier:", item_init_hook); ADD(item, "validate", item_validate);
    if (host) { ADD(host, "drawRect:", host_draw); ADD(host, "mouseDown:", host_mouse); ADD(host, "acceptsFirstMouse:", yes_imp); }
    if (win) { ADD(win, "toggleToolbarShown:", win_toggle_toolbar); ADD(win, "runToolbarCustomizationPalette:", win_run_palette); }
    if (obj) {
        def(&_MOSToolbarCustomTarget_c, &_MOSToolbarCustomTarget_m, "_MOSToolbarCustomTarget", obj);
        ADD(&_MOSToolbarCustomTarget_c, "cxPalOK:", pal_ok); ADD(&_MOSToolbarCustomTarget_c, "cxPalDefault:", pal_default);
        ADD(&_MOSToolbarCustomTarget_c, "cxPalCancel:", pal_cancel); ADD(&_MOSToolbarCustomTarget_c, "cxGroupSegment:", grp_seg_click);
    }
    def(&NSToolbarItemGroup_c, &NSToolbarItemGroup_m, "NSToolbarItemGroup", item);
    cx_class_t *g = kls("NSToolbarItemGroup");
    ADD(g, "subitems", grp_subitems); ADD(g, "setSubitems:", grp_set_subitems);
    ADD(g, "selectionMode", grp_mode); ADD(g, "setSelectionMode:", grp_set_mode);
    ADD(g, "controlRepresentation", grp_rep); ADD(g, "setControlRepresentation:", grp_set_rep);
    ADD(g, "selectedIndex", grp_selected_index); ADD(g, "setSelectedIndex:", grp_set_selected_index);
    ADD(g, "setSelected:atIndex:", grp_set_selected); ADD(g, "isSelectedAtIndex:", grp_is_selected);
    cx_add_cmethod(g, "groupWithItemIdentifier:titles:selectionMode:labels:target:action:", (void *)grp_with_titles);
    cx_add_cmethod(g, "groupWithItemIdentifier:images:selectionMode:labels:target:action:", (void *)grp_with_images);
    def(&NSMenuToolbarItem_c, &NSMenuToolbarItem_m, "NSMenuToolbarItem", item);
    cx_class_t *mt = kls("NSMenuToolbarItem");
    ADD(mt, "menu", mti_menu); ADD(mt, "setMenu:", mti_set_menu); ADD(mt, "showsIndicator", mti_shows); ADD(mt, "setShowsIndicator:", mti_set_shows);
    def(&NSSearchToolbarItem_c, &NSSearchToolbarItem_m, "NSSearchToolbarItem", item);
    cx_class_t *st = kls("NSSearchToolbarItem");
    ADD(st, "initWithItemIdentifier:", sti_init); ADD(st, "searchField", sti_field); ADD(st, "setSearchField:", sti_set_field);
    ADD(st, "beginSearchInteraction", sti_begin); ADD(st, "endSearchInteraction", sti_end);
    ADD(st, "preferredWidthForSearchField", sti_pref_w); ADD(st, "setPreferredWidthForSearchField:", sti_set_pref_w);
    ADD(st, "resignsFirstResponderWithCancel", yes_imp);
    def(&NSTrackingSeparatorToolbarItem_c, &NSTrackingSeparatorToolbarItem_m, "NSTrackingSeparatorToolbarItem", item);
    cx_class_t *ts = kls("NSTrackingSeparatorToolbarItem");
    cx_add_cmethod(ts, "trackingSeparatorToolbarItemWithIdentifier:splitView:dividerIndex:", (void *)tsi_make);
    ADD(ts, "splitView", tsi_split); ADD(ts, "setSplitView:", tsi_set_split); ADD(ts, "dividerIndex", tsi_div); ADD(ts, "setDividerIndex:", tsi_set_div);
    ADD(ts, "minSize", tsi_size); ADD(ts, "maxSize", tsi_size);
    def(&NSSharingServicePickerToolbarItem_c, &NSSharingServicePickerToolbarItem_m, "NSSharingServicePickerToolbarItem", item);
    cx_class_t *ss = kls("NSSharingServicePickerToolbarItem");
    ADD(ss, "delegate", sspt_dl); ADD(ss, "setDelegate:", sspt_set_dl);
}
