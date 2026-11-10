/* NSDockTile (I19). setBadgeLabel: publica o texto no Dock.
 * contentView fica no tile; o kernel ainda não desenha essa view no ícone (K19).
 * Com -DCX_DOCK_HOST o teste liga só o modelo, sem ObjC e sem syscall. */
#ifdef CX_DOCK_HOST
#include <string.h>
#else
#include "foundation.h"
#include "libc.h"
#include "../../kernel/include/syscall.h"
#endif
#include "appkit_docktile.h"

static char g_label[64];
static char g_shown[64];
static void *g_content;
static void *g_owner;
static int g_shows = 1;
static int g_displayed;
static int g_unknown;
static int g_loads;
static cx_dock_sink_fn g_sink;
static void *g_sink_ctx;

static void scopy(char *d, int cap, const char *s) {
    int i = 0;
    if (!s) s = "";
    if (cap < 1) return;
    for (; i < cap - 1 && s[i]; i++) d[i] = s[i];
    d[i] = 0;
}

static void publish(void) {
    if (g_shows && g_label[0]) scopy(g_shown, (int)sizeof g_shown, g_label);
    else g_shown[0] = 0;
    g_displayed = 1;
    if (g_sink) g_sink(g_shown, g_sink_ctx);
}

void cx_dock_reset(void) {
    g_label[0] = 0;
    g_shown[0] = 0;
    g_content = 0;
    g_owner = 0;
    g_shows = 1;
    g_displayed = 0;
    g_unknown = 0;
    g_loads = 0;
    g_sink = 0;
    g_sink_ctx = 0;
}

void cx_dock_set_badge(const char *label) {
    scopy(g_label, (int)sizeof g_label, label);
    publish();
}

const char *cx_dock_badge(void) { return g_label; }

void cx_dock_set_content(void *view) { g_content = view; }
void *cx_dock_content(void) { return g_content; }

void cx_dock_display(void) { publish(); }
int cx_dock_displayed(void) { return g_displayed; }

void cx_dock_set_shows_app_badge(int on) {
    g_shows = on ? 1 : 0;
    publish();
}

int cx_dock_shows_app_badge(void) { return g_shows; }
const char *cx_dock_shown(void) { return g_shown; }

void cx_dock_size(double *w, double *h) {
    if (w) *w = 128;
    if (h) *h = 128;
}

void cx_dock_set_owner(void *owner) { g_owner = owner; }
void *cx_dock_owner(void) { return g_owner; }

void cx_dock_set_sink(cx_dock_sink_fn fn, void *ctx) {
    g_sink = fn;
    g_sink_ctx = ctx;
}

int cx_dock_selector(const char *name) {
    static const char *names[] = {
        "setBadgeLabel:", "badgeLabel", "setContentView:", "contentView",
        "display", "setShowsApplicationBadge:", "showsApplicationBadge",
        "size", "owner", "dockTile", 0
    };
    if (!name) return 0;
    for (int i = 0; names[i]; i++) if (!strcmp(name, names[i])) return 1;
    return 0;
}

int cx_dock_unknown(const char *name) {
    (void)name;
    g_unknown++;
    return 0;
}

int cx_dock_unknown_count(void) { return g_unknown; }

int cx_dock_load_plugin(cx_dock_plugin_fn set_dock_tile, void *plugin) {
    g_loads++;
    if (set_dock_tile) set_dock_tile(plugin, &g_label);
    return 1;
}

int cx_dock_load_count(void) { return g_loads; }

#ifndef CX_DOCK_HOST
void *dlopen(const char *path, int mode);
id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);

cx_class_t NSDockTile_c __asm__("OBJC_CLASS_$_NSDockTile");
cx_class_t NSDockTile_m __asm__("OBJC_METACLASS_$_NSDockTile");

static id g_tile;
static id g_content_obj;

typedef struct { double width, height; } dock_size_t;

static void kernel_badge(const char *label, void *ctx) {
    (void)ctx;
    SYS2(SYS_DESKTOP, MOS_DESK_BADGE, label ? label : "");
}

static const char *cstr_or_empty(id s) {
    const char *p;
    if (!s) return "";
    p = cx_cstr(s);
    if (!p || !strcmp(p, "(null)")) return "";
    return p;
}

static void tile_set_badge(id self, SEL c, id label) {
    (void)self; (void)c;
    cx_dock_set_badge(cstr_or_empty(label));
}
static id tile_badge(id self, SEL c) {
    (void)self; (void)c;
    return objc_autorelease(cx_str(cx_dock_badge()));
}
static void tile_set_content(id self, SEL c, id view) {
    (void)self; (void)c;
    if (view) objc_retain(view);
    if (g_content_obj) objc_release(g_content_obj);
    g_content_obj = view;
    cx_dock_set_content(view);
}
static id tile_content(id self, SEL c) { (void)self; (void)c; return g_content_obj; }
static void tile_display(id self, SEL c) { (void)self; (void)c; cx_dock_display(); }
static void tile_set_shows(id self, SEL c, BOOL on) { (void)self; (void)c; cx_dock_set_shows_app_badge(on ? 1 : 0); }
static BOOL tile_shows(id self, SEL c) { (void)self; (void)c; return cx_dock_shows_app_badge() ? YES : NO; }
static dock_size_t tile_size(id self, SEL c) {
    dock_size_t s;
    (void)self; (void)c;
    cx_dock_size(&s.width, &s.height);
    return s;
}
static id tile_owner(id self, SEL c) { (void)self; (void)c; return (id)cx_dock_owner(); }

static id app_dock_tile(id self, SEL c) {
    (void)c;
    if (!g_tile) {
        g_tile = cx_alloc_instance(&NSDockTile_c);
        cx_dock_set_owner(self);
    }
    return g_tile;
}

/* Info.plist XML: <key>Nome</key> seguido de <string>valor</string>. */
static int plist_string(const char *buf, const char *key, char *out, int cap) {
    int klen = 0, i;
    while (key[klen]) klen++;
    for (i = 0; buf[i]; i++) {
        int k;
        for (k = 0; k < klen && buf[i + k] == key[k]; k++) {}
        if (k != klen) continue;
        {
            const char *p = buf + i + k;
            const char *a, *b;
            while (*p && *p != '<') p++;
            if (strncmp(p, "<string>", 8)) continue;
            a = p + 8;
            b = a;
            while (*b && *b != '<') b++;
            if (b <= a || (int)(b - a) >= cap) return 0;
            for (k = 0; k < (int)(b - a); k++) out[k] = a[k];
            out[k] = 0;
            return 1;
        }
    }
    return 0;
}

static int read_plist(const char *path, char *buf, int cap) {
    int fd, n;
    fd = open(path, 0, 0);
    if (fd < 0) return 0;
    n = (int)read(fd, buf, cap - 1);
    close(fd);
    if (n <= 0) return 0;
    buf[n] = 0;
    return 1;
}

int cx_docktile_load_path(const char *path) {
    char plist[512], bin[512], principal[128], exe[128], text[4096];
    void *h;
    id cls, plugin;
    if (!path || !path[0]) return 0;
    if (access(path, 0) != 0) return 0;
    principal[0] = 0;
    exe[0] = 0;
    snprintf(plist, sizeof plist, "%s/Contents/Info.plist", path);
    if (read_plist(plist, text, (int)sizeof text)) {
        plist_string(text, "NSPrincipalClass</key>", principal, (int)sizeof principal);
        plist_string(text, "CFBundleExecutable</key>", exe, (int)sizeof exe);
    }
    snprintf(bin, sizeof bin, "%s/Contents/MacOS/%s", path, exe[0] ? exe : "CodexDockTilePlugin");
    if (access(bin, 0) != 0) {
        /* o próprio caminho pode ser o binário */
        snprintf(bin, sizeof bin, "%s", path);
        if (access(bin, 0) != 0) return 0;
    }
    h = dlopen(bin, 1);
    if (!h) return 0;
    if (!principal[0]) return 1;
    cls = (id)cx_class_named(principal);
    if (!cls) return 1;
    plugin = MSG0(id, cls, "alloc");
    if (!plugin) return 1;
    plugin = MSG0(id, plugin, "init");
    if (!plugin) return 1;
    if (cx_responds(object_getClass(plugin), sel_registerName("setDockTile:"))) {
        id app = MSG0(id, (id)cx_class_named("NSApplication"), "sharedApplication");
        id tile = app ? MSG0(id, app, "dockTile") : g_tile;
        if (tile) ((void (*)(id, SEL, id))objc_msgSend)(plugin, sel_registerName("setDockTile:"), tile);
    }
    return 1;
}

int cx_dock_load_path(const char *path) { return cx_docktile_load_path(path); }

static void add_if_missing(cx_class_t *k, const char *sel, void *imp) {
    if (k && !cx_responds(k, sel_registerName(sel))) cx_add_method(k, sel, imp);
}

void cx_appkit_docktile_init(void) {
    cx_class_t *obj = cx_class_named("NSObject");
    cx_class_t *app = cx_class_named("NSApplication");
    cx_define_class(&NSDockTile_c, &NSDockTile_m, "NSDockTile", obj, 0);
    cx_add_method(&NSDockTile_c, "setBadgeLabel:", (void *)tile_set_badge);
    cx_add_method(&NSDockTile_c, "badgeLabel", (void *)tile_badge);
    cx_add_method(&NSDockTile_c, "setContentView:", (void *)tile_set_content);
    cx_add_method(&NSDockTile_c, "contentView", (void *)tile_content);
    cx_add_method(&NSDockTile_c, "display", (void *)tile_display);
    cx_add_method(&NSDockTile_c, "setShowsApplicationBadge:", (void *)tile_set_shows);
    cx_add_method(&NSDockTile_c, "showsApplicationBadge", (void *)tile_shows);
    cx_add_method(&NSDockTile_c, "size", (void *)tile_size);
    cx_add_method(&NSDockTile_c, "owner", (void *)tile_owner);
    add_if_missing(app, "dockTile", (void *)app_dock_tile);
    cx_dock_set_sink(kernel_badge, 0);
}
#else
int cx_dock_load_path(const char *path) {
    (void)path;
    return 0;
}
#endif
