/*
 * mOS libSystem — aparência (claro/escuro) e cores do sistema no AppKit.
 *   O tema e a cor de destaque vêm do window server (GUI_DEVICE/DEV_THEME) e são conferidos no laço de eventos
 *   (cx_appearance_tick, chamado do appkit_app_more.c). Ao mudar: as cores do sistema já entregues se atualizam (são
 *   únicas por nome e trocam de valor, como as dinâmicas do Cocoa), as de colorWithName:dynamicProvider: são
 *   reavaliadas, NSApp avisa effectiveAppearance por KVO, saem NSSystemColorsDidChangeNotification e
 *   AppleInterfaceThemeChangedNotification, e cada view recebe viewDidChangeEffectiveAppearance e é redesenhada.
 *   NSAppearance: appearanceNamed: (Aqua, DarkAqua, Vibrant*, AccessibilityHighContrast*), name, allowsVibrancy,
 *   bestMatchFromAppearancesWithNames:, +currentAppearance/+currentDrawingAppearance/+setCurrentAppearance:,
 *   performAsCurrentDrawingAppearance:.
 *   NSApplication/NSWindow/NSView: appearance, setAppearance:, effectiveAppearance (a view herda da superview, da janela e
 *   do app). Com alguma aparência explícita, o appkit.c põe a aparência da view como a de desenho enquanto a desenha
 *   (cx_appearance_enter_view/leave) e as cores do sistema pedidas ali saem nessa aparência.
 * Paleta própria do mOS (a mesma do tema do window server, kernel/ui/theme.c). Código original do mOS.
 * Registro: cx_appkit_appearance_init(), chamado do cx_appkit_input_init.
 */
#include "foundation.h"
#include "libc.h"
#include "sys.h"
#include "../../kernel/include/compat_gui.h"
#include "appearance_core.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void objc_setAssociatedObject(id o, const void *k, id v, u64 policy);
id objc_getAssociatedObject(id o, const void *k);
id cx_color(double r, double g, double b, double a);
void *_Block_copy(const void *b);
extern id NSApp;

static cx_class_t *kls(const char *n) { return cx_class_named(n); }
static SEL sl(const char *n) { return sel_registerName(n); }
static int responds(id o, const char *s) { return o && cx_responds(object_getClass(o), sl(s)); }
static double *comps(id color) { return (double *)((u8 *)color + 8); }   /* NSColor: r, g, b, a logo após o isa */

/* ---------------------------------------------------------------- estado do sistema */
static int g_dark = -1;         /* tema do window server: -1 ainda não lido */
static u32 g_accent = 0x007AFF;
static void read_theme(int *dark, u32 *accent) {
    long v = (long)SYS4(GUI_DEVICE, DEV_THEME, 0, 0, 0);
    if (v < 0) { *dark = 0; *accent = 0x007AFF; return; }
    *dark = (int)(v & 1);
    *accent = (u32)((v >> 8) & 0xFFFFFF);
}
static void ensure_theme(void) { if (g_dark < 0) read_theme(&g_dark, &g_accent); }

/* ---------------------------------------------------------------- NSAppearance */
typedef struct { cx_class_t *isa; id name; } appearance_t;
static const char *const k_names[] = {
    "NSAppearanceNameAqua", "NSAppearanceNameDarkAqua", "NSAppearanceNameVibrantLight", "NSAppearanceNameVibrantDark",
    "NSAppearanceNameAccessibilityHighContrastAqua", "NSAppearanceNameAccessibilityHighContrastDarkAqua",
    "NSAppearanceNameAccessibilityHighContrastVibrantLight", "NSAppearanceNameAccessibilityHighContrastVibrantDark",
};
#define NNAMES (int)(sizeof k_names / sizeof k_names[0])
static id g_shared[NNAMES];
static int name_index(id name) {
    if (!name || !cx_is_str(name)) return -1;
    const char *s = cx_cstr(name);
    for (int i = 0; i < NNAMES; i++) if (!strcmp(s, k_names[i])) return i;
    return -1;
}
static id shared(int i) {
    if (i < 0 || i >= NNAMES) return 0;
    if (!g_shared[i]) {
        cx_class_t *k = kls("NSAppearance");
        appearance_t *a = k ? (appearance_t *)cx_alloc_instance(k) : 0;
        if (!a) return 0;
        a->name = objc_retain(cx_str(k_names[i]));
        g_shared[i] = (id)a;
    }
    return g_shared[i];
}
static BOOL app_is_dark(id a) {
    if (!a) return NO;
    id n = ((appearance_t *)a)->name;
    return n && cx_is_str(n) && strstr(cx_cstr(n), "Dark") != 0;
}
static id ap_named(id cls, SEL c, id name) { (void)cls; (void)c; return shared(name_index(name)); }
static id ap_name(id self, SEL c) { (void)c; return ((appearance_t *)self)->name; }
static BOOL ap_vibrancy(id self, SEL c) {
    (void)c;
    id n = ((appearance_t *)self)->name;
    return n && strstr(cx_cstr(n), "Vibrant") != 0;
}
/* o nome da lista que mais se parece com esta aparência: o mesmo nome; senão um do mesmo lado (claro/escuro), com a
 * mesma vibração e o mesmo contraste antes; nil se nenhum é do mesmo lado */
static id ap_best_match(id self, SEL c, id names) {
    (void)c;
    u64 n = names ? MSG0(u64, names, "count") : 0;
    if (n > 64) n = 64;
    const char *list[64];
    for (u64 i = 0; i < n; i++) { id x = MSG(id, names, "objectAtIndex:", i); list[i] = x && cx_is_str(x) ? cx_cstr(x) : 0; }
    id mine = ((appearance_t *)self)->name;
    int k = ac_best_match(mine ? cx_cstr(mine) : 0, list, (int)n);
    return k >= 0 ? MSG(id, names, "objectAtIndex:", (u64)k) : 0;
}

/* aparência efetiva do app, da janela e da view */
static id g_app_explicit;       /* NSApp.appearance (retida) */
static id g_cur;                /* aparência de desenho atual (fraca: sempre uma das compartilhadas ou de quem chamou) */
static int g_overrides;         /* quantas janelas/views têm aparência explícita */
static char k_appearance;
static id system_appearance(void) { ensure_theme(); return shared(g_dark ? 1 : 0); }
static id app_effective(void) { return g_app_explicit ? g_app_explicit : system_appearance(); }
static id effective_of(id o) {
    for (int guard = 0; o && guard < 256; guard++) {
        id a = objc_getAssociatedObject(o, &k_appearance);
        if (a) return a;
        cx_class_t *view = kls("NSView");
        if (view && cx_is_subclass(object_getClass(o), view)) {
            id sup = MSG0(id, o, "superview");
            o = sup ? sup : MSG0(id, o, "window");
            continue;
        }
        break;   /* janela sem aparência própria: a do app */
    }
    return app_effective();
}
static id cur_drawing(void) { return g_cur ? g_cur : app_effective(); }
static id ap_current(id cls, SEL c) { (void)cls; (void)c; return cur_drawing(); }
static void ap_set_current(id cls, SEL c, id a) { (void)cls; (void)c; g_cur = a; }
static void ap_perform(id self, SEL c, void *block) {
    (void)c;
    typedef struct { void *isa; int flags, reserved; void (*invoke)(void *); } blk_t;
    if (!block) return;
    id saved = g_cur;
    g_cur = self;
    ((blk_t *)block)->invoke(block);
    g_cur = saved;
}
/* appkit.c: enquanto desenha a view (só quando há alguma aparência explícita) */
id cx_appearance_enter_view(id v) {
    if (!g_overrides) return (id)-1;
    id saved = g_cur;
    g_cur = effective_of(v);
    return saved;
}
void cx_appearance_leave(id saved) { if (saved != (id)-1) g_cur = saved; }
BOOL cx_appearance_dark(void) { return app_is_dark(cur_drawing()); }

/* ---------------------------------------------------------------- cores do sistema */
static int col_index(const char *sel) { return ac_index(sel); }
static void resolve(int i, int dark, double out[4]) { ensure_theme(); ac_resolve(i, dark, g_accent, out); }
#define NCOLS 128
static id g_cols[NCOLS];        /* uma por nome; trocam de valor quando a aparência do app muda */
static int g_cols_dark = -1;    /* lado (claro/escuro) dos valores atuais das cores únicas */

static int base_dark(void) { return app_is_dark(app_effective()); }
static void refresh_singletons(void) {
    int base = base_dark();
    for (int i = 0; i < NCOLS; i++) if (g_cols[i]) resolve(i, base, comps(g_cols[i]));
    g_cols_dark = base;
}
/* +[NSColor labelColor] e as outras: a única do nome na aparência do app; noutra aparência de desenho, uma resolvida */
static id sys_color(id cls, SEL c) {
    (void)cls;
    int i = col_index((const char *)c);
    if (i < 0) return cx_color(0.5, 0.5, 0.5, 1);
    int want = app_is_dark(cur_drawing()), base = base_dark();
    if (want != base || i >= NCOLS) {   /* view ou bloco com outra aparência: cor resolvida só para ali */
        double v[4];
        resolve(i, want, v);
        return cx_color(v[0], v[1], v[2], v[3]);
    }
    if (g_cols_dark != base) refresh_singletons();
    if (!g_cols[i]) { g_cols[i] = objc_retain(cx_color(0, 0, 0, 1)); resolve(i, base, comps(g_cols[i])); }
    return g_cols[i];
}
static id cl_alternating(id cls, SEL c) {
    (void)cls; (void)c;
    BOOL dark = app_is_dark(cur_drawing());
    id a = MSG0(id, (id)kls("NSMutableArray"), "array");
    MSG(void, a, "addObject:", dark ? cx_color(0.118, 0.118, 0.118, 1) : cx_color(1, 1, 1, 1));
    MSG(void, a, "addObject:", dark ? cx_color(0.15, 0.15, 0.15, 1) : cx_color(0.957, 0.961, 0.961, 1));
    return a;
}
/* colorWithCatalogName:@"System" colorName:@"labelColor" e afins; outros catálogos pela NSColorList, se houver */
static id cl_catalog(id cls, SEL c, id catalog, id name) {
    (void)c;
    if (!name || !cx_is_str(name)) return 0;
    if (catalog && cx_is_str(catalog) && !strcmp(cx_cstr(catalog), "System") && col_index(cx_cstr(name)) >= 0)
        return sys_color(cls, (SEL)sl(cx_cstr(name)));
    cx_class_t *lists = kls("NSColorList");
    id list = lists && catalog ? MSG(id, (id)lists, "colorListNamed:", catalog) : 0;
    return list && responds(list, "colorWithKey:") ? MSG(id, list, "colorWithKey:", name) : 0;
}
static u64 cl_control_tint(id cls, SEL c) {
    (void)cls; (void)c;
    ensure_theme();
    u32 r = (g_accent >> 16) & 255, g = (g_accent >> 8) & 255, b = g_accent & 255;
    return r == g && g == b ? 6 : 1;   /* NSGraphiteControlTint quando o destaque é cinza; senão NSBlueControlTint */
}

/* NSColor(name:dynamicProvider:): o bloco decide a cor pela aparência; reavaliado quando o tema muda */
static id g_dynamic;            /* NSMutableArray das cores dinâmicas */
static char k_provider;
static void dyn_eval(id color, id appearance) {
    void *blk = (void *)objc_getAssociatedObject(color, &k_provider);
    if (!blk) return;
    typedef struct { void *isa; int flags, reserved; id (*invoke)(void *, id); } blk_t;
    id saved = g_cur;
    g_cur = appearance;
    id r = ((blk_t *)blk)->invoke(blk, appearance);
    g_cur = saved;
    if (r && r != color) { double *s = comps(r), *d = comps(color); for (int k = 0; k < 4; k++) d[k] = s[k]; }
}
static id cl_dynamic(id cls, SEL c, id name, void *provider) {
    (void)cls; (void)c; (void)name;
    id color = cx_color(0, 0, 0, 1);
    if (!provider) return color;
    objc_setAssociatedObject(color, &k_provider, (id)_Block_copy(provider), 1);
    if (!g_dynamic) g_dynamic = MSG0(id, MSG0(id, (id)kls("NSMutableArray"), "alloc"), "init");
    MSG(void, g_dynamic, "addObject:", color);
    dyn_eval(color, cur_drawing());
    return color;
}

/* ---------------------------------------------------------------- app, janela, view */
static void notify_tree(id v) {
    if (!v) return;
    if (responds(v, "viewDidChangeEffectiveAppearance")) MSG0(void, v, "viewDidChangeEffectiveAppearance");
    MSG(void, v, "setNeedsDisplay:", (BOOL)YES);
    id subs = MSG0(id, v, "subviews");
    u64 n = subs ? MSG0(u64, subs, "count") : 0;
    for (u64 i = 0; i < n; i++) notify_tree(MSG(id, subs, "objectAtIndex:", i));
}
static void notify_window(id w) {
    if (!w) return;
    id cv = MSG0(id, w, "contentView");
    if (cv) notify_tree(cv);
}
static void notify_all(void) {
    id ws = NSApp ? MSG0(id, NSApp, "windows") : 0;
    u64 n = ws ? MSG0(u64, ws, "count") : 0;
    for (u64 i = 0; i < n; i++) notify_window(MSG(id, ws, "objectAtIndex:", i));
}
static void app_kvo(BOOL will) {
    if (!NSApp || !responds(NSApp, will ? "willChangeValueForKey:" : "didChangeValueForKey:")) return;
    MSG(void, NSApp, will ? "willChangeValueForKey:" : "didChangeValueForKey:", cx_str("effectiveAppearance"));
}
static void reevaluate_dynamic(void) {
    u64 n = g_dynamic ? MSG0(u64, g_dynamic, "count") : 0;
    for (u64 i = 0; i < n; i++) dyn_eval(MSG(id, g_dynamic, "objectAtIndex:", i), app_effective());
}
static void app_appearance_changed(void) {
    refresh_singletons();
    reevaluate_dynamic();
    notify_all();
}
static id app_appearance(id self, SEL c) { (void)self; (void)c; return g_app_explicit; }
static void app_set_appearance(id self, SEL c, id a) {
    (void)self; (void)c;
    if (a == g_app_explicit) return;
    app_kvo(YES);
    id old = g_app_explicit;
    g_app_explicit = a ? objc_retain(a) : 0;
    if (old) objc_release(old);
    app_kvo(NO);
    app_appearance_changed();
}
static id app_effective_m(id self, SEL c) { (void)self; (void)c; return app_effective(); }
static id obj_appearance(id self, SEL c) { (void)c; return objc_getAssociatedObject(self, &k_appearance); }
static void obj_set_appearance(id self, SEL c, id a) {
    (void)c;
    id old = objc_getAssociatedObject(self, &k_appearance);
    if (old == a) return;
    if (!old && a) g_overrides++;
    else if (old && !a && g_overrides > 0) g_overrides--;
    if (responds(self, "willChangeValueForKey:")) MSG(void, self, "willChangeValueForKey:", cx_str("effectiveAppearance"));
    objc_setAssociatedObject(self, &k_appearance, a, 1);
    if (responds(self, "didChangeValueForKey:")) MSG(void, self, "didChangeValueForKey:", cx_str("effectiveAppearance"));
    cx_class_t *view = kls("NSView");
    if (view && cx_is_subclass(object_getClass(self), view)) notify_tree(self);
    else notify_window(self);
}
static id obj_effective(id self, SEL c) { (void)c; return effective_of(self); }

/* ---------------------------------------------------------------- laço: o tema do sistema mudou? */
void cx_appearance_tick(void) {
    int dark;
    u32 accent;
    read_theme(&dark, &accent);
    if (g_dark < 0) { g_dark = dark; g_accent = accent; return; }
    if (dark == g_dark && accent == g_accent) return;
    BOOL theme = dark != g_dark;
    if (theme && !g_app_explicit) app_kvo(YES);
    g_dark = dark;
    g_accent = accent;
    if (theme && !g_app_explicit) app_kvo(NO);
    app_appearance_changed();
    id nc = MSG0(id, (id)kls("NSNotificationCenter"), "defaultCenter");
    MSG(void, nc, "postNotificationName:object:", cx_str("NSSystemColorsDidChangeNotification"), (id)0);
    if (theme) {
        cx_class_t *dnc = kls("NSDistributedNotificationCenter");
        id d = dnc ? MSG0(id, (id)dnc, "defaultCenter") : 0;
        if (d) MSG(void, d, "postNotificationName:object:", cx_str("AppleInterfaceThemeChangedNotification"), (id)0);
    }
}

void cx_appkit_appearance_init(void) {
    cx_class_t *ap = kls("NSAppearance");
    if (ap) {
        cx_add_cmethod(ap, "appearanceNamed:", (void *)ap_named);
        cx_add_cmethod(ap, "currentAppearance", (void *)ap_current);
        cx_add_cmethod(ap, "currentDrawingAppearance", (void *)ap_current);
        cx_add_cmethod(ap, "setCurrentAppearance:", (void *)ap_set_current);
        cx_add_method(ap, "name", (void *)ap_name);
        cx_add_method(ap, "allowsVibrancy", (void *)ap_vibrancy);
        cx_add_method(ap, "bestMatchFromAppearancesWithNames:", (void *)ap_best_match);
        cx_add_method(ap, "performAsCurrentDrawingAppearance:", (void *)ap_perform);
    }
    cx_class_t *col = kls("NSColor");
    if (col) {
        for (int i = 0; i < ac_count(); i++) cx_add_cmethod(col, ac_name(i), (void *)sys_color);
        cx_add_cmethod(col, "alternatingContentBackgroundColors", (void *)cl_alternating);
        cx_add_cmethod(col, "colorWithCatalogName:colorName:", (void *)cl_catalog);
        cx_add_cmethod(col, "currentControlTint", (void *)cl_control_tint);
        cx_add_cmethod(col, "colorWithName:dynamicProvider:", (void *)cl_dynamic);
    }
    cx_class_t *app = kls("NSApplication");
    if (app) {
        cx_add_method(app, "appearance", (void *)app_appearance);
        cx_add_method(app, "setAppearance:", (void *)app_set_appearance);
        cx_add_method(app, "effectiveAppearance", (void *)app_effective_m);
    }
    const char *owners[] = {"NSWindow", "NSView"};
    for (int i = 0; i < 2; i++) {
        cx_class_t *k = kls(owners[i]);
        if (!k) continue;
        cx_add_method(k, "appearance", (void *)obj_appearance);
        cx_add_method(k, "setAppearance:", (void *)obj_set_appearance);
        cx_add_method(k, "effectiveAppearance", (void *)obj_effective);
    }
}

/* cor de destaque do sistema em ARGB (appkit.c desenha os controles com ela) */
u32 cx_appearance_accent(void) { ensure_theme(); return 0xFF000000u | g_accent; }
