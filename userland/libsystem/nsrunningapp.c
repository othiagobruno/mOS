/* NSRunningApplication e NSWorkspace.runningApplications / frontmostApplication.
 * A lista sai de SYS_PROCLIST. O nome e o .app, quando a janela existe, saem do
 * window server. activateWithOptions: foca uma janela desse pid (DEV_FOCUS_PID).
 * isActive compara com o pid da janela em foco (DEV_FRONT_PID), sem captura de tela. */
#include "foundation.h"
#include "sys.h"
#include "../../kernel/include/compat_gui.h"
#include "computeruse_win.h"

id objc_retain(id o);
void objc_release(id o);
id cx_plist_file(const char *path);
id CFArrayCreate(void *alloc, const id *vals, long n, const void *cb);
int snprintf(char *b, size_t n, const char *fmt, ...);
void *calloc(size_t n, size_t s);
void free(void *p);
int strcmp(const char *a, const char *b);

typedef struct {
    int pid;
    id name;
    id bundle;
} ra_st;

static id g_cache[64];
static int g_cache_n;

static ra_st *rst(id o) { return o ? *(ra_st **)((char *)o + 8) : 0; }

static id plist_ident(const char *bundle_path) {
    char path[400];
    id dict, v;
    if (!bundle_path || !bundle_path[0]) return 0;
    snprintf(path, sizeof path, "%s/Contents/Info.plist", bundle_path);
    dict = cx_plist_file(path);
    if (!dict) return 0;
    v = MSG(id, dict, "objectForKey:", cx_str("CFBundleIdentifier"));
    return v && cx_is_str(v) ? objc_retain(v) : 0;
}

static id ident_for(int pid, const char *bundle_path, const char *fallback) {
    id ident = plist_ident(bundle_path);
    if (ident) return ident;
    if (pid == getpid()) {
        cx_class_t *bcls = cx_class_named("NSBundle");
        id b = bcls ? MSG0(id, (id)bcls, "mainBundle") : 0;
        id v = b ? MSG0(id, b, "bundleIdentifier") : 0;
        if (v && cx_is_str(v)) return objc_retain(v);
    }
    return cx_str(fallback && fallback[0] ? fallback : "");
}

static void window_meta(int pid, char *name, int ncap, char *bundle, int bcap) {
    gui_cgwindow_t w;
    name[0] = bundle[0] = 0;
    for (int i = 0; i < 48; i++) {
        if (!cx_cg_window_at(i, &w)) break;
        if (w.pid != pid) continue;
        if (!name[0] && w.owner[0]) snprintf(name, (size_t)ncap, "%s", w.owner);
        if (!bundle[0] && w.bundle[0]) snprintf(bundle, (size_t)bcap, "%s", w.bundle);
    }
}

static id ra_get(int pid, const char *name, const char *bundle_path) {
    cx_class_t *cls = cx_class_named("NSRunningApplication");
    ra_st *st;
    id o;
    for (int i = 0; i < g_cache_n; i++) {
        st = rst(g_cache[i]);
        if (st && st->pid == pid) {
            if (name && name[0]) { objc_release(st->name); st->name = cx_str(name); }
            return g_cache[i];
        }
    }
    if (!cls || g_cache_n >= 64) return 0;
    o = cx_alloc_instance(cls);
    st = calloc(1, sizeof *st);
    st->pid = pid;
    st->name = cx_str(name && name[0] ? name : "Processo");
    st->bundle = ident_for(pid, bundle_path, name);
    *(ra_st **)((char *)o + 8) = st;
    g_cache[g_cache_n++] = o;
    return o;
}

static int front_pid(void) {
    long pid = (long)SYS4(GUI_DEVICE, DEV_FRONT_PID, 0, 0, 0);
    return pid > 0 ? (int)pid : 0;
}

static long ra_pid(id self, SEL c) { (void)c; ra_st *st = rst(self); return st ? st->pid : 0; }
static id ra_bundle(id self, SEL c) { (void)c; ra_st *st = rst(self); return st && st->bundle ? st->bundle : cx_str(""); }
static id ra_name(id self, SEL c) { (void)c; ra_st *st = rst(self); return st && st->name ? st->name : cx_str(""); }
static BOOL ra_active(id self, SEL c) {
    (void)c;
    ra_st *st = rst(self);
    int front = front_pid();
    return st && front && st->pid == front ? YES : NO;
}
static BOOL ra_activate(id self, SEL c, unsigned long opts) {
    ra_st *st = rst(self);
    long rc;
    (void)c; (void)opts;
    if (!st || st->pid <= 0) return NO;
    rc = (long)SYS4(GUI_DEVICE, DEV_FOCUS_PID, (unsigned long)st->pid, 0, 0);
    return rc == 0 ? YES : NO;
}

static id ra_current(id cls, SEL c) {
    mos_procinfo_t pi;
    char name[64], bundle[160];
    (void)cls; (void)c;
    name[0] = bundle[0] = 0;
    window_meta(getpid(), name, (int)sizeof name, bundle, (int)sizeof bundle);
    if (!name[0]) {
        for (long i = 0; SYS2(SYS_PROCLIST, i, &pi) == 1; i++)
            if ((int)pi.pid == getpid()) { snprintf(name, sizeof name, "%s", pi.name); break; }
    }
    return ra_get(getpid(), name, bundle);
}

static id collect_apps(void) {
    id tmp[64];
    long n = 0;
    mos_procinfo_t pi;
    for (long i = 0; SYS2(SYS_PROCLIST, i, &pi) == 1 && n < 64; i++) {
        char name[64], bundle[160];
        id app;
        if ((int)pi.pid <= 0) continue;
        window_meta((int)pi.pid, name, (int)sizeof name, bundle, (int)sizeof bundle);
        if (!name[0]) snprintf(name, sizeof name, "%s", pi.name[0] ? pi.name : "Processo");
        app = ra_get((int)pi.pid, name, bundle);
        if (app) tmp[n++] = app;
    }
    return CFArrayCreate(0, tmp, n, 0);
}

static id ws_running(id self, SEL c) { (void)self; (void)c; return collect_apps(); }
static id ws_front(id self, SEL c) {
    int pid = front_pid();
    id apps, found = 0;
    (void)self; (void)c;
    if (pid <= 0) return ra_current(0, 0);
    apps = collect_apps();
    if (apps) {
        unsigned long n = MSG0(unsigned long, apps, "count");
        for (unsigned long i = 0; i < n; i++) {
            id app = MSG(id, apps, "objectAtIndex:", i);
            ra_st *st = rst(app);
            if (st && st->pid == pid) { found = app; break; }
        }
        objc_release(apps);
    }
    if (found) return found;
    return ra_get(pid, "", "");
}
static id ra_running(id cls, SEL c) { (void)cls; (void)c; return collect_apps(); }

void cx_nsrunning_init(void) {
    cx_class_t *ra = cx_class_named("NSRunningApplication");
    cx_class_t *ws = cx_class_named("NSWorkspace");
    if (ra) {
        cx_add_cmethod(ra, "currentApplication", (void *)ra_current);
        cx_add_cmethod(ra, "runningApplications", (void *)ra_running);
        cx_add_method(ra, "processIdentifier", (void *)ra_pid);
        cx_add_method(ra, "bundleIdentifier", (void *)ra_bundle);
        cx_add_method(ra, "localizedName", (void *)ra_name);
        cx_add_method(ra, "isActive", (void *)ra_active);
        cx_add_method(ra, "activateWithOptions:", (void *)ra_activate);
    }
    if (ws) {
        cx_add_method(ws, "runningApplications", (void *)ws_running);
        cx_add_method(ws, "frontmostApplication", (void *)ws_front);
    }
}
