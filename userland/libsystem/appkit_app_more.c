/*
 * mOS libSystem — NSApplication: ativação, ocultar, atenção no Dock, ícone e o menu Janela.
 *   isActive e as notificações NSApplicationWill/DidBecomeActive e Will/DidResignActive (com o delegate): o app está
 *   ativo quando o processo da janela em foco é ele (GUI_DEVICE/DEV_FRONT_PID), conferido no laço de eventos.
 *   hide:, unhide:, unhideWithoutActivation, isHidden com Will/DidHide e Will/DidUnhide; o "Ocultar" do window server
 *   (⌘H) chega aqui. Ao voltar a ficar ativo (clique no Dock), o app oculto reaparece; sem janelas visíveis, o delegate
 *   recebe applicationShouldHandleReopen:hasVisibleWindows:.
 *   requestUserAttention: / cancelUserAttentionRequest: fazem o ícone pular no Dock (DEV_DOCK/DK_ATTENTION);
 *   setApplicationIconImage: troca o ícone no Dock (DK_ICON) e applicationIconImage o devolve.
 *   windowsMenu / setWindowsMenu: com a lista das janelas no fim do menu (marca na janela-chave), addWindowsItem:,
 *   removeWindowsItem:, changeWindowsItem:title:filename:, updateWindowsItem:, setWindowsNeedUpdate: e
 *   -[NSWindow setExcludedFromWindowsMenu:].
 * O appkit.c chama cx_app_more_tick() a cada volta do laço (que também confere o tema, appkit_appearance.c) e
 * cx_app_menu_hide() no ⌘H.
 * Registro: cx_appkit_app_more_init(), chamado do cx_appkit_input_init. Código original do mOS.
 */
#include "foundation.h"
#include "libc.h"
#include "sys.h"
#include "../../kernel/include/compat_gui.h"

typedef struct { double w, h; } S;
typedef struct { double x, y; } P;
typedef struct { P o; S s; } R;
id objc_retain(id o);
void objc_release(id o);
void objc_setAssociatedObject(id o, const void *k, id v, u64 policy);
id objc_getAssociatedObject(id o, const void *k);
int getpid(void);
double cx_now(void);
void *CGColorSpaceCreateDeviceRGB(void);
void CGColorSpaceRelease(void *s);
void *CGBitmapContextCreate(void *data, size_t w, size_t h, size_t bpc, size_t bpr, void *space, u32 info);
void *CGBitmapContextGetData(void *c);
void CGContextDrawImage(void *c, R r, void *img);
void CGContextRelease(void *c);
size_t CGImageGetWidth(void *img);
size_t CGImageGetHeight(void *img);
extern id NSApp;

static cx_class_t *kls(const char *n) { return cx_class_named(n); }
static SEL sl(const char *n) { return sel_registerName(n); }
static int responds(id o, const char *s) { return o && cx_responds(object_getClass(o), sl(s)); }
static id delegate(void) { return NSApp ? MSG0(id, NSApp, "delegate") : 0; }
/* notificação do app com o método do delegate correspondente */
static void post(const char *name, const char *dsel) {
    if (!NSApp) return;
    id note = MSG(id, (id)kls("NSNotification"), "notificationWithName:object:userInfo:", cx_str(name), NSApp, (id)0);
    id d = delegate();
    if (d && dsel && responds(d, dsel)) MSG(void, d, dsel, note);
    MSG(void, MSG0(id, (id)kls("NSNotificationCenter"), "defaultCenter"), "postNotification:", note);
}
static id windows(void) { return NSApp ? MSG0(id, NSApp, "windows") : 0; }
static u64 count(id a) { return a ? MSG0(u64, a, "count") : 0; }
static id at(id a, u64 i) { return MSG(id, a, "objectAtIndex:", i); }
static BOOL visible(id w) { return responds(w, "isVisible") && MSG0(BOOL, w, "isVisible"); }

/* ---------------------------------------------------------------- ocultar */
static BOOL g_hidden;
static id g_hidden_wins;        /* janelas que estavam visíveis (NSMutableArray) */
static id g_hidden_key;         /* fraco */
static void ap_hide(id self, SEL c, id sender) {
    (void)self; (void)c; (void)sender;
    if (g_hidden) return;
    post("NSApplicationWillHideNotification", "applicationWillHide:");
    if (!g_hidden_wins) g_hidden_wins = MSG0(id, MSG0(id, (id)kls("NSMutableArray"), "alloc"), "init");
    MSG0(void, g_hidden_wins, "removeAllObjects");
    g_hidden_key = MSG0(id, NSApp, "keyWindow");
    id ws = windows();
    for (u64 i = 0, n = count(ws); i < n; i++) {
        id w = at(ws, i);
        if (!visible(w)) continue;
        MSG(void, g_hidden_wins, "addObject:", w);
        MSG(void, w, "orderOut:", (id)0);
    }
    g_hidden = YES;
    post("NSApplicationDidHideNotification", "applicationDidHide:");
}
static void ap_unhide_without_activation(id self, SEL c) {
    (void)self; (void)c;
    if (!g_hidden) return;
    post("NSApplicationWillUnhideNotification", "applicationWillUnhide:");
    g_hidden = NO;
    for (u64 i = 0, n = count(g_hidden_wins); i < n; i++) MSG(void, at(g_hidden_wins, i), "orderFront:", (id)0);
    id key = g_hidden_key;
    for (u64 i = 0, n = count(g_hidden_wins); key && i < n; i++)
        if (at(g_hidden_wins, i) == key) { MSG(void, key, "makeKeyAndOrderFront:", (id)0); break; }
    MSG0(void, g_hidden_wins, "removeAllObjects");
    g_hidden_key = 0;
    post("NSApplicationDidUnhideNotification", "applicationDidUnhide:");
}
static void ap_unhide(id self, SEL c, id sender) {
    (void)sender;
    ap_unhide_without_activation(self, c);
    MSG(void, NSApp, "activateIgnoringOtherApps:", (BOOL)YES);
}
static BOOL ap_is_hidden(id self, SEL c) { (void)self; (void)c; return g_hidden; }
void cx_app_menu_hide(void) { if (NSApp) MSG(void, NSApp, "hide:", (id)0); }

/* ---------------------------------------------------------------- ativação */
static int g_active = -1;       /* -1 = ainda não conferido */
static int front_is_me(void) {
    long pid = (long)SYS4(GUI_DEVICE, DEV_FRONT_PID, 0, 0, 0);
    return pid > 0 && pid == (long)getpid();
}
static BOOL ap_is_active(id self, SEL c) {
    (void)self; (void)c;
    if (g_active < 0) g_active = front_is_me();
    return g_active ? YES : NO;
}
static BOOL any_visible(void) {
    id ws = windows();
    for (u64 i = 0, n = count(ws); i < n; i++) if (visible(at(ws, i))) return YES;
    return NO;
}
static void activity_check(void) {
    int now = front_is_me();
    if (g_active < 0) { g_active = now; return; }   /* o estado inicial não gera notificação */
    if (now == g_active) return;
    if (now) {
        post("NSApplicationWillBecomeActiveNotification", "applicationWillBecomeActive:");
        g_active = 1;
        if (g_hidden) ap_unhide_without_activation(NSApp, 0);   /* clique no Dock traz o app oculto */
        else if (!any_visible()) {                               /* reabrir: o app decide se cria uma janela */
            id d = delegate();
            if (d && responds(d, "applicationShouldHandleReopen:hasVisibleWindows:"))
                MSG(BOOL, d, "applicationShouldHandleReopen:hasVisibleWindows:", NSApp, (BOOL)NO);
        }
        post("NSApplicationDidBecomeActiveNotification", "applicationDidBecomeActive:");
    } else {
        post("NSApplicationWillResignActiveNotification", "applicationWillResignActive:");
        g_active = 0;
        post("NSApplicationDidResignActiveNotification", "applicationDidResignActive:");
    }
}

/* ---------------------------------------------------------------- atenção no Dock */
static i64 g_attention_seq;
static i64 ap_request_attention(id self, SEL c, u64 type) {
    (void)self; (void)c;
    if (ap_is_active(NSApp, 0)) return 0;   /* app ativo: não pede (como no Cocoa) */
    SYS3(GUI_DEVICE, DEV_DOCK, DK_ATTENTION, type == 0 ? 2 : 1);   /* NSCriticalRequest = 0, NSInformationalRequest = 10 */
    return ++g_attention_seq;
}
static void ap_cancel_attention(id self, SEL c, i64 req) {
    (void)self; (void)c; (void)req;
    SYS3(GUI_DEVICE, DEV_DOCK, DK_ATTENTION, 0);
}

/* ---------------------------------------------------------------- ícone */
static id g_icon;
static void ap_set_icon(id self, SEL c, id image) {
    (void)self; (void)c;
    if (g_icon != image) { id old = g_icon; g_icon = image ? objc_retain(image) : 0; if (old) objc_release(old); }
    if (!image) { SYS3(GUI_DEVICE, DEV_DOCK, DK_ICON, 0); return; }
    void *cg = MSG(void *, image, "CGImageForProposedRect:context:hints:", (void *)0, (id)0, (id)0);
    if (!cg) return;
    long w = (long)CGImageGetWidth(cg), h = (long)CGImageGetHeight(cg);
    if (w <= 0 || h <= 0) return;
    long side = w > h ? w : h;
    if (side > GUI_DOCK_ICON_MAX) side = GUI_DOCK_ICON_MAX;   /* quadrado; a imagem é centrada mantendo a proporção */
    void *space = CGColorSpaceCreateDeviceRGB();
    void *ctx = CGBitmapContextCreate(0, (size_t)side, (size_t)side, 8, (size_t)side * 4, space, 2u | (2u << 12));
    CGColorSpaceRelease(space);
    if (!ctx) return;
    double sc = (double)side / (double)(w > h ? w : h), dw = w * sc, dh = h * sc;
    CGContextDrawImage(ctx, (R){{(side - dw) / 2, (side - dh) / 2}, {dw, dh}}, cg);
    u32 *src = CGBitmapContextGetData(ctx);
    gui_dock_icon_t *di = src ? malloc(sizeof *di) : 0;
    if (di) {
        di->w = di->h = (unsigned)side;
        for (long i = 0; i < side * side; i++) {   /* não pré-multiplicado para o Dock */
            u32 p = src[i], a = p >> 24;
            if (!a) { di->px[i] = 0; continue; }
            u32 r = ((p >> 16) & 255) * 255 / a, g = ((p >> 8) & 255) * 255 / a, b = (p & 255) * 255 / a;
            di->px[i] = (a << 24) | ((r > 255 ? 255 : r) << 16) | ((g > 255 ? 255 : g) << 8) | (b > 255 ? 255 : b);
        }
        SYS3(GUI_DEVICE, DEV_DOCK, DK_ICON, di);
        free(di);
    }
    CGContextRelease(ctx);
}
static id ap_icon(id self, SEL c) {
    (void)self; (void)c;
    if (g_icon) return g_icon;
    return MSG(id, (id)kls("NSImage"), "imageNamed:", cx_str("NSApplicationIcon"));
}

/* ---------------------------------------------------------------- menu Janela */
static id g_windows_menu;
static char k_win_item, k_excluded, k_win_sep;
static BOOL g_wm_dirty = YES;
static u64 g_wm_sig;
static void ap_set_windows_menu(id self, SEL c, id m) {
    (void)self; (void)c;
    if (g_windows_menu == m) return;
    id old = g_windows_menu;
    g_windows_menu = m ? objc_retain(m) : 0;
    if (old) objc_release(old);
    g_wm_dirty = YES;
}
static id ap_windows_menu(id self, SEL c) { (void)self; (void)c; return g_windows_menu; }
static void w_set_excluded(id self, SEL c, BOOL v) { (void)c; objc_setAssociatedObject(self, &k_excluded, v ? cx_num_i(1) : 0, 1); g_wm_dirty = YES; }
static BOOL w_excluded(id self, SEL c) {
    (void)c;
    if (objc_getAssociatedObject(self, &k_excluded)) return YES;
    cx_class_t *panel = kls("NSPanel");
    return panel && cx_is_subclass(object_getClass(self), panel);   /* painéis ficam fora, como no Cocoa */
}
static BOOL listed(id w) {
    if (!visible(w) && !(responds(w, "isMiniaturized") && MSG0(BOOL, w, "isMiniaturized"))) return NO;
    if (MSG0(BOOL, w, "isExcludedFromWindowsMenu")) return NO;
    id t = MSG0(id, w, "title");
    return t && MSG0(u64, t, "length") > 0;
}
static u64 signature(void) {
    u64 h = 1469598103934665603ull;
    id ws = windows(), key = NSApp ? MSG0(id, NSApp, "keyWindow") : 0;
    for (u64 i = 0, n = count(ws); i < n; i++) {
        id w = at(ws, i);
        if (!listed(w)) continue;
        h = (h ^ (u64)(usize)w) * 1099511628211ull;
        for (const char *t = cx_cstr(MSG0(id, w, "title")); *t; t++) h = (h ^ (u8)*t) * 1099511628211ull;
        if (w == key) h = (h ^ 0x4b) * 1099511628211ull;
    }
    return h;
}
static void rebuild_windows_menu(void) {
    id m = g_windows_menu;
    if (!m) return;
    /* tira os itens que este arquivo pôs */
    for (i64 i = MSG0(i64, m, "numberOfItems"); i-- > 0;) {
        id it = MSG(id, m, "itemAtIndex:", (i64)i);
        if (objc_getAssociatedObject(it, &k_win_item) || objc_getAssociatedObject(it, &k_win_sep)) MSG(void, m, "removeItemAtIndex:", (i64)i);
    }
    id ws = windows(), key = NSApp ? MSG0(id, NSApp, "keyWindow") : 0;
    BOOL first = YES;
    for (u64 i = 0, n = count(ws); i < n; i++) {
        id w = at(ws, i);
        if (!listed(w)) continue;
        if (first && MSG0(i64, m, "numberOfItems") > 0) {
            id sep = MSG0(id, (id)kls("NSMenuItem"), "separatorItem");
            objc_setAssociatedObject(sep, &k_win_sep, cx_num_i(1), 1);
            MSG(void, m, "addItem:", sep);
        }
        first = NO;
        id it = MSG(id, MSG0(id, (id)kls("NSMenuItem"), "alloc"), "initWithTitle:action:keyEquivalent:", MSG0(id, w, "title"),
                    sl("makeKeyAndOrderFront:"), cx_str(""));
        MSG(void, it, "setTarget:", w);
        MSG(void, it, "setState:", (i64)(w == key ? 1 : 0));
        objc_setAssociatedObject(it, &k_win_item, w, 0);   /* fraco: só marca o item */
        MSG(void, m, "addItem:", it);
        objc_release(it);
    }
}
static void ap_windows_need_update(id self, SEL c, BOOL f) { (void)self; (void)c; if (f) g_wm_dirty = YES; }
static void ap_add_windows_item(id self, SEL c, id w, id title, BOOL fname) { (void)self; (void)c; (void)w; (void)title; (void)fname; g_wm_dirty = YES; }
static void ap_remove_windows_item(id self, SEL c, id w) { (void)self; (void)c; (void)w; g_wm_dirty = YES; }
static void ap_change_windows_item(id self, SEL c, id w, id title, BOOL fname) { (void)self; (void)c; (void)w; (void)title; (void)fname; g_wm_dirty = YES; }
static void ap_update_windows_item(id self, SEL c, id w) { (void)self; (void)c; (void)w; g_wm_dirty = YES; }

/* ---------------------------------------------------------------- miudezas de janela, view e app
 * NSWindow identifier (NSUserInterfaceItemIdentification) e restorationClass; NSResponder invalidateRestorableState
 * (marca o estado como sujo: isRestorableStateDirty); NSView invalidateIntrinsicContentSize (o Auto Layout pergunta de
 * novo na próxima passada); NSProgressIndicator controlSize (o giratório: 32, 16 ou 10 pontos no sizeToFit);
 * NSApplication automaticCustomizeTouchBarMenuItemEnabled (o mOS não tem Touch Bar: o item não entra no menu). */
static char k_ident, k_restore_cls, k_restore_dirty, k_ctl_size, k_touchbar_item;
static void w_set_ident(id self, SEL c, id v) { (void)c; objc_setAssociatedObject(self, &k_ident, v ? MSG0(id, v, "copy") : 0, 1); }
static id w_ident(id self, SEL c) { (void)c; return objc_getAssociatedObject(self, &k_ident); }
static void w_set_restore_cls(id self, SEL c, id cls) { (void)c; objc_setAssociatedObject(self, &k_restore_cls, cls, 0); }
static id w_restore_cls(id self, SEL c) { (void)c; return objc_getAssociatedObject(self, &k_restore_cls); }
static void r_invalidate_restorable(id self, SEL c) { (void)c; objc_setAssociatedObject(self, &k_restore_dirty, cx_num_i(1), 1); }
static BOOL r_restorable_dirty(id self, SEL c) { (void)c; return objc_getAssociatedObject(self, &k_restore_dirty) != 0; }
static void v_invalidate_intrinsic(id self, SEL c) {
    (void)c;
    MSG(void, self, "setNeedsLayout:", (BOOL)YES);
    id sup = MSG0(id, self, "superview");
    if (sup) MSG(void, sup, "setNeedsLayout:", (BOOL)YES);
    MSG(void, self, "setNeedsDisplay:", (BOOL)YES);
}
static void pi_set_ctl_size(id self, SEL c, u64 sz) {
    (void)c;
    objc_setAssociatedObject(self, &k_ctl_size, cx_num_i((i64)sz), 1);
    MSG(void, self, "setNeedsDisplay:", (BOOL)YES);
}
static u64 pi_ctl_size(id self, SEL c) { (void)c; id v = objc_getAssociatedObject(self, &k_ctl_size); return v ? MSG0(u64, v, "unsignedLongLongValue") : 0; }
static void ap_set_tb_item(id self, SEL c, BOOL v) { (void)c; objc_setAssociatedObject(self, &k_touchbar_item, v ? cx_num_i(1) : 0, 1); }
static BOOL ap_tb_item(id self, SEL c) { (void)c; return objc_getAssociatedObject(self, &k_touchbar_item) != 0; }

/* ---------------------------------------------------------------- laço */
void cx_app_more_tick(void) {
    static double last;
    double now = cx_now();
    if (now - last < 0.25) return;
    last = now;
    activity_check();
    { extern void cx_appearance_tick(void); cx_appearance_tick(); }   /* appkit_appearance.c: tema claro/escuro e destaque */
    if (g_windows_menu) {
        u64 sig = signature();
        if (g_wm_dirty || sig != g_wm_sig) { g_wm_dirty = NO; g_wm_sig = sig; rebuild_windows_menu(); }
    }
}

void cx_appkit_app_more_init(void) {
    cx_class_t *a = kls("NSApplication");
    if (a) {
        cx_add_method(a, "isActive", (void *)ap_is_active);
        cx_add_method(a, "hide:", (void *)ap_hide);
        cx_add_method(a, "unhide:", (void *)ap_unhide);
        cx_add_method(a, "unhideWithoutActivation", (void *)ap_unhide_without_activation);
        cx_add_method(a, "isHidden", (void *)ap_is_hidden);
        cx_add_method(a, "requestUserAttention:", (void *)ap_request_attention);
        cx_add_method(a, "cancelUserAttentionRequest:", (void *)ap_cancel_attention);
        cx_add_method(a, "setApplicationIconImage:", (void *)ap_set_icon);
        cx_add_method(a, "applicationIconImage", (void *)ap_icon);
        cx_add_method(a, "setWindowsMenu:", (void *)ap_set_windows_menu);
        cx_add_method(a, "windowsMenu", (void *)ap_windows_menu);
        cx_add_method(a, "setWindowsNeedUpdate:", (void *)ap_windows_need_update);
        cx_add_method(a, "addWindowsItem:title:filename:", (void *)ap_add_windows_item);
        cx_add_method(a, "removeWindowsItem:", (void *)ap_remove_windows_item);
        cx_add_method(a, "changeWindowsItem:title:filename:", (void *)ap_change_windows_item);
        cx_add_method(a, "updateWindowsItem:", (void *)ap_update_windows_item);
    }
    if (a) {
        cx_add_method(a, "setAutomaticCustomizeTouchBarMenuItemEnabled:", (void *)ap_set_tb_item);
        cx_add_method(a, "isAutomaticCustomizeTouchBarMenuItemEnabled", (void *)ap_tb_item);
    }
    cx_class_t *r = kls("NSResponder");
    if (r) {
        cx_add_method(r, "invalidateRestorableState", (void *)r_invalidate_restorable);
        cx_add_method(r, "_cxRestorableStateIsDirty", (void *)r_restorable_dirty);
    }
    cx_class_t *v = kls("NSView");
    if (v) cx_add_method(v, "invalidateIntrinsicContentSize", (void *)v_invalidate_intrinsic);
    cx_class_t *pi = kls("NSProgressIndicator");
    if (pi) { cx_add_method(pi, "setControlSize:", (void *)pi_set_ctl_size); cx_add_method(pi, "controlSize", (void *)pi_ctl_size); }
    cx_class_t *w = kls("NSWindow");
    if (w) {
        cx_add_method(w, "setIdentifier:", (void *)w_set_ident); cx_add_method(w, "identifier", (void *)w_ident);
        cx_add_method(w, "setRestorationClass:", (void *)w_set_restore_cls); cx_add_method(w, "restorationClass", (void *)w_restore_cls);
        cx_add_method(w, "setExcludedFromWindowsMenu:", (void *)w_set_excluded);
        cx_add_method(w, "isExcludedFromWindowsMenu", (void *)w_excluded);
    }
}
