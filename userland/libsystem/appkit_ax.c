/* mOS — árvore de acessibilidade do AppKit deste processo.
 *
 * AXUIElementCreateApplication / SystemWide, CopyAttributeValue (role, title, value,
 * position, size, children, focused), PerformAction (AXPress) e SetAttributeValue
 * leem as views de verdade. A permissão é o TCC do Sistemas (cx_tcc_preflight de
 * kTCCServiceAccessibility): sem ela a resposta é -25211. Com ela, outro pid devolve
 * -25204 para a árvore de views. _AXUIElementCreateWithRemoteToken aceita o token
 * mAX1 (ax_token.h): com número de janela, título e geometria saem de DEV_WINDOW_LIST.
 * A árvore de views de outro processo continua -25204. Não há um segundo TCC nem um
 * XPC paralelo aqui.
 */
#include "foundation.h"
#include "computeruse_win.h"
#include "ax_token.h"

int cx_tcc_preflight(const char *service);
int getpid(void);
int strcmp(const char *a, const char *b);
void *memset(void *d, int c, size_t n);
int cx_appkit_window_frame(id w, double *x, double *y, double *wd, double *ht);
int cx_appkit_window_number(id w);
int cx_appkit_view_frame(id v, double *x, double *y, double *wd, double *ht);
id AXValueCreate(unsigned type, const void *p);
id CFArrayCreate(void *alloc, const id *vals, long n, const void *cb);
id objc_retain(id o);
void objc_release(id o);
id objc_msgSend(id self, SEL op, ...);

enum {
    AXK_APP = 1,
    AXK_WIN = 2,
    AXK_VIEW = 3,
    AXK_SYS = 4
};

enum {
    AX_OK = 0,
    AX_FAIL = -25200,
    AX_BADARG = -25201,
    AX_BAD_ELEM = -25202,
    AX_CANT = -25204,
    AX_NOATTR = -25205,
    AX_NOACT = -25206,
    AX_NOIMPL = -25208,
    AX_OFF = -25211,
    AX_NOVAL = -25212
};

typedef struct {
    cx_class_t *isa;
    int kind;
    int pid;
    id obj;
    unsigned wid;
} axel_t;

static cx_class_t ax_c, ax_m;

static void ax_boot(void) {
    if (ax_c.vtable) return;
    cx_define_class(&ax_c, &ax_m, "__mOSAXElement", cx_class_named("NSObject"), (u32)(sizeof(axel_t) - sizeof(void *)));
}

static id ax_new(int kind, int pid, id obj) {
    ax_boot();
    axel_t *e = (axel_t *)cx_alloc_instance(&ax_c);
    if (!e) return 0;
    e->kind = kind;
    e->pid = pid;
    e->obj = obj;
    e->wid = 0;
    return (id)e;
}

static axel_t *ax_of(id e) {
    ax_boot();
    if (!e || e->isa != &ax_c) return 0;
    return (axel_t *)e;
}

static int ax_gate(void) {
    return cx_tcc_preflight("kTCCServiceAccessibility") ? 0 : AX_OFF;
}

/* Outro processo: o elemento existe, a árvore não atravessa o processo. */
static int ax_remote(axel_t *e) {
    return e && e->kind == AXK_APP && e->pid && e->pid != getpid();
}

static int is_a(id o, const char *cls) {
    cx_class_t *c = cx_class_named(cls);
    return o && c && MSG(BOOL, o, "isKindOfClass:", (id)c);
}

static id nsapp(void) {
    cx_class_t *c = cx_class_named("NSApplication");
    return c ? MSG0(id, (id)c, "sharedApplication") : 0;
}

static id wrap_obj(id obj) {
    if (!obj) return 0;
    if (is_a(obj, "NSWindow")) return ax_new(AXK_WIN, getpid(), obj);
    return ax_new(AXK_VIEW, getpid(), obj);
}

id cx_ax_application(int pid) {
    if (!pid) pid = getpid();
    return ax_new(AXK_APP, pid, 0);
}

id cx_ax_system(void) { return ax_new(AXK_SYS, 0, 0); }

static const char *role_of(axel_t *e) {
    if (!e) return 0;
    if (e->kind == AXK_SYS) return "AXSystemWide";
    if (e->kind == AXK_APP) return "AXApplication";
    if (e->kind == AXK_WIN) return "AXWindow";
    id o = e->obj;
    if (is_a(o, "NSButton")) return "AXButton";
    if (is_a(o, "NSTextView")) return "AXTextArea";
    if (is_a(o, "NSTextField")) {
        int ed = MSG(BOOL, o, "respondsToSelector:", sel_registerName("isEditable")) && MSG0(BOOL, o, "isEditable");
        return ed ? "AXTextField" : "AXStaticText";
    }
    return "AXGroup";
}

static const char *role_desc(const char *role) {
    if (!role) return "";
    if (!strcmp(role, "AXButton")) return "button";
    if (!strcmp(role, "AXTextField")) return "text field";
    if (!strcmp(role, "AXTextArea")) return "text entry area";
    if (!strcmp(role, "AXStaticText")) return "text";
    if (!strcmp(role, "AXWindow")) return "window";
    if (!strcmp(role, "AXApplication")) return "application";
    if (!strcmp(role, "AXSystemWide")) return "system-wide";
    return "group";
}

static id str_attr(id obj, const char *sel) {
    if (!obj || !MSG(BOOL, obj, "respondsToSelector:", sel_registerName(sel))) return cx_str("");
    id v = MSG0(id, obj, sel);
    if (!v) return cx_str("");
    if (cx_is_str(v)) return objc_retain(v);
    id d = MSG0(id, v, "description");
    return cx_is_str(d) ? objc_retain(d) : cx_str("");
}

static int listed_win(axel_t *e, gui_cgwindow_t *info) {
    if (!e || e->kind != AXK_WIN || e->obj || !e->wid) return 0;
    for (int i = 0; i < 64; i++) {
        if (!cx_cg_window_at(i, info)) return 0;
        if (info->number == e->wid) return 1;
    }
    return 0;
}
static int screen_h(void) {
    gui_wininfo_t i;
    memset(&i, 0, sizeof i);
    i.w = 1440;
    i.h = 876;
    SYS1(GUI_SCREEN, &i);
    return i.h > 0 ? i.h + 24 : 900;
}
static int frame_of(axel_t *e, double *x, double *y, double *w, double *h) {
    gui_cgwindow_t info;
    if (listed_win(e, &info)) {
        *x = info.x;
        *y = (double)screen_h() - info.y - info.h;
        *w = info.w;
        *h = info.h;
        return 1;
    }
    if (e->kind == AXK_WIN) return e->obj && cx_appkit_window_frame(e->obj, x, y, w, h);
    if (e->kind == AXK_VIEW) return cx_appkit_view_frame(e->obj, x, y, w, h);
    *x = *y = *w = *h = 0;
    return 0;
}

static id kids_of_view(id view) {
    id subs = view ? MSG0(id, view, "subviews") : 0;
    u64 n = subs ? MSG0(u64, subs, "count") : 0;
    id tmp[48];
    long got = 0;
    for (u64 i = 0; i < n && got < 48; i++) {
        id s = MSG(id, subs, "objectAtIndex:", i);
        if (!s || MSG0(BOOL, s, "isHidden")) continue;
        tmp[got++] = wrap_obj(s);
    }
    id arr = CFArrayCreate(0, tmp, got, 0);
    for (long i = 0; i < got; i++) objc_release(tmp[i]);
    return arr;
}

static id children_of(axel_t *e) {
    if (e->kind == AXK_SYS) {
        id one = cx_ax_application(getpid());
        id arr = CFArrayCreate(0, &one, one ? 1 : 0, 0);
        if (one) objc_release(one);
        return arr;
    }
    if (e->kind == AXK_APP) {
        id wins = 0;
        id app = nsapp();
        if (app) wins = MSG0(id, app, "orderedWindows");
        u64 n = wins ? MSG0(u64, wins, "count") : 0;
        id tmp[48];
        long got = 0;
        for (u64 i = 0; i < n && got < 48; i++) {
            id w = MSG(id, wins, "objectAtIndex:", i);
            if (w) tmp[got++] = wrap_obj(w);
        }
        id arr = CFArrayCreate(0, tmp, got, 0);
        for (long i = 0; i < got; i++) objc_release(tmp[i]);
        return arr;
    }
    if (e->kind == AXK_WIN) {
        id content = e->obj ? MSG0(id, e->obj, "contentView") : 0;
        if (!content) return CFArrayCreate(0, 0, 0, 0);
        id one = wrap_obj(content);
        id arr = CFArrayCreate(0, &one, 1, 0);
        objc_release(one);
        return arr;
    }
    return kids_of_view(e->obj);
}

static id focused_element(void) {
    id key = 0;
    id app = nsapp();
    if (app) key = MSG0(id, app, "keyWindow");
    if (!key) return 0;
    id fr = MSG0(id, key, "firstResponder");
    if (fr && fr != key) return wrap_obj(fr);
    return wrap_obj(key);
}

static int focused_flag(axel_t *e) {
    if (e->kind != AXK_VIEW && e->kind != AXK_WIN) return 0;
    id key = 0;
    id app = nsapp();
    if (app) key = MSG0(id, app, "keyWindow");
    if (!key) return 0;
    if (e->kind == AXK_WIN) return e->obj == key;
    id fr = MSG0(id, key, "firstResponder");
    return fr == e->obj;
}

int cx_ax_copy(id elem, id attr, id *v) {
    if (v) *v = 0;
    int g = ax_gate();
    if (g) return g;
    axel_t *e = ax_of(elem);
    if (!e || !attr) return e ? AX_BADARG : AX_BAD_ELEM;
    if (ax_remote(e)) return AX_CANT;
    const char *name = cx_cstr(attr);
    if (!strcmp(name, "AXRole")) { if (v) *v = cx_str(role_of(e)); return AX_OK; }
    if (!strcmp(name, "AXRoleDescription")) { if (v) *v = cx_str(role_desc(role_of(e))); return AX_OK; }
    if (!strcmp(name, "AXTitle")) {
        if (e->kind == AXK_WIN && !e->obj) {
            gui_cgwindow_t info;
            if (v) *v = listed_win(e, &info) ? cx_str(info.name) : cx_str("");
            return AX_OK;
        }
        if (e->kind == AXK_WIN) { if (v) *v = str_attr(e->obj, "title"); return AX_OK; }
        if (e->kind == AXK_APP) {
            id pi = MSG0(id, (id)cx_class_named("NSProcessInfo"), "processInfo");
            if (v) *v = str_attr(pi, "processName");
            return AX_OK;
        }
        if (e->kind == AXK_VIEW) { if (v) *v = str_attr(e->obj, "title"); return AX_OK; }
        if (v) *v = cx_str("");
        return AX_OK;
    }
    if (!strcmp(name, "AXValue")) {
        if (e->kind == AXK_VIEW) { if (v) *v = str_attr(e->obj, "stringValue"); return AX_OK; }
        return AX_NOATTR;
    }
    if (!strcmp(name, "AXPosition") || !strcmp(name, "AXSize")) {
        double x, y, w, h;
        if (!frame_of(e, &x, &y, &w, &h)) return AX_NOATTR;
        if (!strcmp(name, "AXPosition")) {
            double p[2] = {x, y};
            if (v) *v = AXValueCreate(1, p);
        } else {
            double p[2] = {w, h};
            if (v) *v = AXValueCreate(2, p);
        }
        return AX_OK;
    }
    if (!strcmp(name, "AXChildren")) { if (v) *v = children_of(e); return AX_OK; }
    if (!strcmp(name, "AXFocused")) { if (v) *v = cx_num_i(focused_flag(e)); return AX_OK; }
    if (!strcmp(name, "AXFocusedUIElement")) {
        if (e->kind != AXK_APP && e->kind != AXK_SYS) return AX_NOATTR;
        id f = focused_element();
        if (!f) return AX_NOVAL;
        if (v) *v = f;
        else objc_release(f);
        return AX_OK;
    }
    if (!strcmp(name, "AXWindow")) {
        id w = 0;
        if (e->kind == AXK_WIN) w = e->obj;
        else if (e->kind == AXK_VIEW && e->obj) w = MSG0(id, e->obj, "window");
        else return AX_NOATTR;
        if (!w) return AX_NOVAL;
        if (v) *v = wrap_obj(w);
        return AX_OK;
    }
    if (!strcmp(name, "AXEnabled")) {
        int on = 1;
        if (e->kind == AXK_VIEW && e->obj && MSG(BOOL, e->obj, "respondsToSelector:", sel_registerName("isEnabled")))
            on = MSG0(BOOL, e->obj, "isEnabled") ? 1 : 0;
        if (v) *v = cx_num_i(on);
        return AX_OK;
    }
    return AX_NOATTR;
}

int cx_ax_copy_values(id elem, id attr, long index, long maxValues, id *values) {
    if (values) *values = 0;
    id all = 0;
    int st = cx_ax_copy(elem, attr, &all);
    if (st) return st;
    if (!all) return AX_NOVAL;
    int is_arr = is_a(all, "NSArray");
    if (!is_arr) {
        if (index == 0 && maxValues > 0 && values) *values = CFArrayCreate(0, &all, 1, 0);
        else if (values) *values = CFArrayCreate(0, 0, 0, 0);
        objc_release(all);
        return AX_OK;
    }
    u64 n = MSG0(u64, all, "count");
    id tmp[48];
    long got = 0;
    if (index < 0) index = 0;
    for (long i = 0; i < maxValues && (unsigned long)(index + i) < n && got < 48; i++) {
        id o = MSG(id, all, "objectAtIndex:", (u64)(index + i));
        if (o) tmp[got++] = o;
    }
    if (values) *values = CFArrayCreate(0, tmp, got, 0);
    objc_release(all);
    return AX_OK;
}

int cx_ax_names(id elem, id *names) {
    if (names) *names = 0;
    int g = ax_gate();
    if (g) return g;
    if (!ax_of(elem)) return AX_BAD_ELEM;
    const char *list[] = {"AXRole", "AXRoleDescription", "AXTitle", "AXValue", "AXPosition", "AXSize", "AXChildren", "AXFocused", "AXWindow"};
    id tmp[9];
    for (int i = 0; i < 9; i++) tmp[i] = cx_str(list[i]);
    if (names) *names = CFArrayCreate(0, tmp, 9, 0);
    for (int i = 0; i < 9; i++) objc_release(tmp[i]);
    return AX_OK;
}

int cx_ax_set(id elem, id attr, id value) {
    int g = ax_gate();
    if (g) return g;
    axel_t *e = ax_of(elem);
    if (!e || !attr) return e ? AX_BADARG : AX_BAD_ELEM;
    if (ax_remote(e)) return AX_CANT;
    if (e->kind != AXK_VIEW || !e->obj) return AX_NOATTR;
    const char *name = cx_cstr(attr);
    if (!strcmp(name, "AXValue")) {
        if (!MSG(BOOL, e->obj, "respondsToSelector:", sel_registerName("setStringValue:"))) return AX_NOATTR;
        id s = value;
        if (s && !cx_is_str(s)) s = MSG0(id, s, "description");
        if (!cx_is_str(s)) return AX_BADARG;
        MSG(void, e->obj, "setStringValue:", s);
        return AX_OK;
    }
    if (!strcmp(name, "AXFocused")) {
        int on = value && MSG0(BOOL, value, "boolValue");
        id win = MSG0(id, e->obj, "window");
        if (on && win) MSG(BOOL, win, "makeFirstResponder:", e->obj);
        return AX_OK;
    }
    return AX_NOATTR;
}

int cx_ax_actions(id elem, id *names) {
    if (names) *names = 0;
    int g = ax_gate();
    if (g) return g;
    axel_t *e = ax_of(elem);
    if (!e) return AX_BAD_ELEM;
    if (ax_remote(e)) return AX_CANT;
    if (e->kind == AXK_VIEW && is_a(e->obj, "NSButton")) {
        id press = cx_str("AXPress");
        if (names) *names = CFArrayCreate(0, &press, 1, 0);
        objc_release(press);
        return AX_OK;
    }
    if (names) *names = CFArrayCreate(0, 0, 0, 0);
    return AX_OK;
}

int cx_ax_perform(id elem, id action) {
    int g = ax_gate();
    if (g) return g;
    axel_t *e = ax_of(elem);
    if (!e || !action) return e ? AX_BADARG : AX_BAD_ELEM;
    if (ax_remote(e)) return AX_CANT;
    if (strcmp(cx_cstr(action), "AXPress")) return AX_NOACT;
    if (e->kind != AXK_VIEW || !is_a(e->obj, "NSButton")) return AX_NOACT;
    SEL act = 0;
    id target = 0;
    if (MSG(BOOL, e->obj, "respondsToSelector:", sel_registerName("action")))
        act = MSG0(SEL, e->obj, "action");
    if (MSG(BOOL, e->obj, "respondsToSelector:", sel_registerName("target")))
        target = MSG0(id, e->obj, "target");
    if (act && target) ((void (*)(id, SEL, id))objc_msgSend)(target, act, e->obj);
    else if (act) {
        id app = nsapp();
        if (app) MSG(id, app, "sendAction:to:from:", act, (id)0, e->obj);
    }
    if (MSG(BOOL, e->obj, "respondsToSelector:", sel_registerName("setState:")))
        MSG(void, e->obj, "setState:", (long)1);
    return AX_OK;
}

int cx_ax_pid(id elem, int *pid) {
    int g = ax_gate();
    if (g) return g;
    axel_t *e = ax_of(elem);
    if (!e) return AX_BAD_ELEM;
    if (pid) *pid = e->kind == AXK_SYS ? 0 : (e->pid ? e->pid : getpid());
    return AX_OK;
}

int cx_ax_settable(id elem, id attr, unsigned char *settable) {
    if (settable) *settable = 0;
    int g = ax_gate();
    if (g) return g;
    axel_t *e = ax_of(elem);
    if (!e || !attr) return e ? AX_BADARG : AX_BAD_ELEM;
    if (ax_remote(e)) return AX_CANT;
    const char *name = cx_cstr(attr);
    int on = 0;
    if (e->kind == AXK_VIEW && e->obj && !strcmp(name, "AXValue")) {
        if (is_a(e->obj, "NSTextView")) on = 1;
        else if (is_a(e->obj, "NSTextField") && MSG0(BOOL, e->obj, "isEditable")) on = 1;
    }
    if (e->kind == AXK_VIEW && !strcmp(name, "AXFocused")) on = 1;
    if (settable) *settable = (unsigned char)on;
    return AX_OK;
}

int cx_ax_window(id elem, unsigned *wid) {
    int g = ax_gate();
    if (g) return g;
    axel_t *e = ax_of(elem);
    if (!e) return AX_BAD_ELEM;
    if (ax_remote(e)) return AX_CANT;
    id w = 0;
    if (e->kind == AXK_WIN) w = e->obj;
    else if (e->kind == AXK_VIEW && e->obj) w = MSG0(id, e->obj, "window");
    else return AX_NOATTR;
    if (wid) *wid = (unsigned)cx_appkit_window_number(w);
    return AX_OK;
}

static id deepest(id view, double x, double y) {
    id subs = view ? MSG0(id, view, "subviews") : 0;
    u64 n = subs ? MSG0(u64, subs, "count") : 0;
    for (u64 i = n; i-- > 0;) {
        id s = MSG(id, subs, "objectAtIndex:", i);
        if (!s || MSG0(BOOL, s, "isHidden")) continue;
        double fx, fy, fw, fh;
        if (!cx_appkit_view_frame(s, &fx, &fy, &fw, &fh)) continue;
        if (x < fx || y < fy || x >= fx + fw || y >= fy + fh) continue;
        id d = deepest(s, x, y);
        return d ? d : wrap_obj(s);
    }
    return 0;
}

long CFDataGetLength(id d);
const unsigned char *CFDataGetBytePtr(id d);
id _AXUIElementCreateWithRemoteToken(id data) {
    int pid = 0;
    unsigned wid = 0;
    const unsigned char *b = data ? CFDataGetBytePtr(data) : 0;
    long n = data ? CFDataGetLength(data) : 0;
    if (!cx_ax_token_read(b, n, &pid, &wid)) return 0;
    if (!wid) return cx_ax_application(pid ? pid : getpid());
    id e = ax_new(AXK_WIN, pid, 0);
    if (e) ((axel_t *)e)->wid = wid;
    return e;
}

int cx_ax_at(id app, float x, float y, id *out) {
    (void)app;
    if (out) *out = 0;
    int g = ax_gate();
    if (g) return g;
    id wins = 0;
    id ns = nsapp();
    if (ns) wins = MSG0(id, ns, "orderedWindows");
    u64 n = wins ? MSG0(u64, wins, "count") : 0;
    for (u64 i = 0; i < n; i++) {
        id w = MSG(id, wins, "objectAtIndex:", i);
        double fx, fy, fw, fh;
        if (!cx_appkit_window_frame(w, &fx, &fy, &fw, &fh)) continue;
        if (x < fx || y < fy || x >= fx + fw || y >= fy + fh) continue;
        id content = MSG0(id, w, "contentView");
        id hit = content ? deepest(content, x, y) : 0;
        if (!hit && content) {
            double cx, cy, cw, ch;
            if (cx_appkit_view_frame(content, &cx, &cy, &cw, &ch) && x >= cx && y >= cy && x < cx + cw && y < cy + ch)
                hit = wrap_obj(content);
        }
        if (!hit) hit = wrap_obj(w);
        if (out) *out = hit;
        return AX_OK;
    }
    return AX_NOVAL;
}
