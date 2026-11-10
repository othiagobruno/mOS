/* mOS — árvore de acessibilidade do AppKit deste processo.
 *
 * AXUIElementCreateApplication / SystemWide, CopyAttributeValue (role, title, value,
 * position, size, children, focused), PerformAction (AXPress) e SetAttributeValue
 * leem as views de verdade. Atributos parametrizados de texto (AXStringForRange,
 * AXBoundsForRange, AXRangeForPosition) também. A permissão é o TCC do Sistemas
 * (cx_tcc_preflight de kTCCServiceAccessibility): sem ela a resposta é -25211.
 * _AXUIElementCreateWithRemoteToken aceita o token mAX1 (ax_token.h): com número de
 * janela, título e geometria saem de DEV_WINDOW_LIST. A árvore de views de outro pid
 * atravessa cx_xpc_peer_call quando cx_ax_transport_ready(). Se o ready falha, ou se
 * o call devolve 60 ou 61, a resposta continua -25204. O protocolo está em axproto.h.
 */
#include "foundation.h"
#include "computeruse_win.h"
#include "ax_token.h"
#include "axproto.h"
#include "axtransport.h"

int cx_tcc_preflight(const char *service);
int getpid(void);
int strcmp(const char *a, const char *b);
void *memset(void *d, int c, size_t n);
int cx_appkit_window_frame(id w, double *x, double *y, double *wd, double *ht);
int cx_appkit_window_number(id w);
int cx_appkit_view_frame(id v, double *x, double *y, double *wd, double *ht);
id AXValueCreate(unsigned type, const void *p);
unsigned char AXValueGetValue(id v, unsigned type, void *out);
size_t strlen(const char *s);
void *memcpy(void *d, const void *s, unsigned long n);
id CFArrayCreate(void *alloc, const id *vals, long n, const void *cb);
id objc_retain(id o);
void objc_release(id o);
id objc_msgSend(id self, SEL op, ...);
int snprintf(char *b, size_t n, const char *fmt, ...);
void *memset(void *d, int c, size_t n);
void cx_ax_bind_xpc(void);
int cx_ax_wire_status(int rc);

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
    int elem_id;
    id obj;
    unsigned wid;
} axel_t;

static cx_class_t ax_c, ax_m;

static void ax_boot(void) {
    if (ax_c.vtable) {
        cx_ax_bind_xpc();
        return;
    }
    cx_define_class(&ax_c, &ax_m, "__mOSAXElement", cx_class_named("NSObject"), (u32)(sizeof(axel_t) - sizeof(void *)));
    cx_ax_bind_xpc();
}

static struct { id obj; int kind, id; } g_tab[96];
static int g_ntab, g_seq = 1000;

static int id_for(id obj, int kind) {
    int idn = 0;
    if (!obj) return 0;
    for (int i = 0; i < g_ntab; i++) if (g_tab[i].obj == obj) return g_tab[i].id;
    if (kind == AXK_WIN) idn = cx_appkit_window_number(obj);
    if (idn <= 0) idn = g_seq++;
    if (g_ntab < 96) { g_tab[g_ntab].obj = obj; g_tab[g_ntab].kind = kind; g_tab[g_ntab].id = idn; g_ntab++; }
    return idn;
}

static id ax_new(int kind, int pid, id obj) {
    ax_boot();
    axel_t *e = (axel_t *)cx_alloc_instance(&ax_c);
    if (!e) return 0;
    e->kind = kind;
    e->pid = pid;
    e->obj = obj;
    e->wid = 0;
    e->elem_id = obj ? id_for(obj, kind) : (kind == AXK_APP ? pid : 0);
    return (id)e;
}

static id ax_handle(int kind, int pid, int elem_id) {
    id o = ax_new(kind, pid, 0);
    if (o) ((axel_t *)o)->elem_id = elem_id;
    return o;
}

static axel_t *ax_of(id e) {
    ax_boot();
    if (!e || e->isa != &ax_c) return 0;
    return (axel_t *)e;
}

static int ax_gate(void) {
    return cx_tcc_preflight("kTCCServiceAccessibility") ? 0 : AX_OFF;
}

/* Outro processo: o handle existe. A árvore só sai se o transporte estiver ligado. */
static int ax_remote(axel_t *e) {
    return e && e->pid && e->pid != getpid();
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

static int remote_roundtrip(axel_t *e, int op, const char *name, const char *str, int loc, int len, int px, int py, ax_msg *out) {
    const cx_ax_transport *t = cx_ax_transport_get();
    ax_msg req;
    unsigned char buf[640], rbuf[640];
    int n, rn = 0, rc;
    if (!out) return AX_CANT;
    memset(out, 0, sizeof *out);
    /* ready falho, ou call 60/61, continuam -25204. */
    if (!e || !cx_ax_transport_ready() || !t || !t->call) return AX_CANT;
    memset(&req, 0, sizeof req);
    req.op = op;
    req.pid = e->pid;
    req.kind = e->kind;
    req.id = e->elem_id;
    req.loc = loc;
    req.len = len;
    req.px = px;
    req.py = py;
    if (name) snprintf(req.name, sizeof req.name, "%s", name);
    if (str) {
        snprintf(req.str, sizeof req.str, "%s", str);
        req.pay = AX_PAY_STR;
    } else if (name && !strcmp(name, "AXRangeForPosition")) req.pay = AX_PAY_RANGE;
    n = ax_msg_encode(&req, buf, (int)sizeof buf);
    if (n < 0) return AX_CANT;
    rc = t->call(e->pid, buf, n, rbuf, (int)sizeof rbuf, &rn);
    if (cx_ax_wire_status(rc)) return AX_CANT;
    if (ax_msg_decode(rbuf, rn, out) != 0) return AX_CANT;
    return out->status;
}

static id msg_value(axel_t *e, const ax_msg *m, const char *attr) {
    if (m->pay == AX_PAY_STR) return cx_str(m->str);
    if (m->pay == AX_PAY_I32) return cx_num_i(m->i);
    if (m->pay == AX_PAY_BOX) {
        if (attr && !strcmp(attr, "AXPosition")) {
            double p[2] = {m->x, m->y};
            return AXValueCreate(1, p);
        }
        if (attr && !strcmp(attr, "AXSize")) {
            double p[2] = {m->w, m->h};
            return AXValueCreate(2, p);
        }
        double r[4] = {m->x, m->y, m->w, m->h};
        return AXValueCreate(3, r);
    }
    if (m->pay == AX_PAY_RANGE) {
        long rg[2] = {m->loc, m->len};
        return AXValueCreate(4, rg);
    }
    if (m->pay == AX_PAY_KIDS) {
        id tmp[AX_KIDS_MAX];
        int n = m->nkids;
        if (n < 0) n = 0;
        if (n > AX_KIDS_MAX) n = AX_KIDS_MAX;
        for (int i = 0; i < n; i++) tmp[i] = ax_handle(m->kid_kind[i], e->pid, m->kid_id[i]);
        id arr = CFArrayCreate(0, tmp, n, 0);
        for (int i = 0; i < n; i++) if (tmp[i]) objc_release(tmp[i]);
        return arr;
    }
    return 0;
}

static int remote_copy(axel_t *e, const char *name, id *v) {
    ax_msg out;
    int st = remote_roundtrip(e, AX_OP_COPY, name, 0, 0, 0, 0, 0, &out);
    if (st) return st;
    if (v) *v = msg_value(e, &out, name);
    return AX_OK;
}

int cx_ax_copy(id elem, id attr, id *v) {
    if (v) *v = 0;
    int g = ax_gate();
    if (g) return g;
    axel_t *e = ax_of(elem);
    if (!e || !attr) return e ? AX_BADARG : AX_BAD_ELEM;
    const char *name = cx_cstr(attr);
    /* Janela mAX1: role, título e geometria saem da lista do compositor, sem o transporte. */
    if (ax_remote(e) && !(e->kind == AXK_WIN && e->wid && !e->obj &&
            (!strcmp(name, "AXRole") || !strcmp(name, "AXRoleDescription") || !strcmp(name, "AXTitle") ||
             !strcmp(name, "AXPosition") || !strcmp(name, "AXSize"))))
        return remote_copy(e, name, v);
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
    if (ax_remote(e)) {
        const char *s = 0;
        char buf[AX_STR_MAX];
        if (value && cx_is_str(value)) s = cx_cstr(value);
        else if (value) {
            id d = MSG0(id, value, "description");
            if (cx_is_str(d)) { snprintf(buf, sizeof buf, "%s", cx_cstr(d)); s = buf; }
        }
        ax_msg out;
        return remote_roundtrip(e, AX_OP_SET, cx_cstr(attr), s, 0, 0, 0, 0, &out);
    }
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

static id actions_from_str(const char *s) {
    id tmp[8];
    int n = 0;
    if (s && s[0]) {
        const char *p = s;
        while (*p && n < 8) {
            char buf[AX_NAME_MAX];
            int i = 0;
            while (*p && *p != ',' && i < AX_NAME_MAX - 1) buf[i++] = *p++;
            buf[i] = 0;
            if (*p == ',') p++;
            if (i) tmp[n++] = cx_str(buf);
        }
    }
    id arr = CFArrayCreate(0, tmp, n, 0);
    for (int i = 0; i < n; i++) objc_release(tmp[i]);
    return arr;
}

int cx_ax_actions(id elem, id *names) {
    if (names) *names = 0;
    int g = ax_gate();
    if (g) return g;
    axel_t *e = ax_of(elem);
    if (!e) return AX_BAD_ELEM;
    if (ax_remote(e)) {
        ax_msg out;
        int st = remote_roundtrip(e, AX_OP_COPY, "AXActionNames", 0, 0, 0, 0, 0, &out);
        if (st) return st;
        if (names) *names = actions_from_str(out.pay == AX_PAY_STR ? out.str : "");
        return AX_OK;
    }
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
    if (ax_remote(e)) {
        ax_msg out;
        return remote_roundtrip(e, AX_OP_PRESS, cx_cstr(action), 0, 0, 0, 0, 0, &out);
    }
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
    if (e->kind == AXK_WIN && e->wid && !e->obj) {
        if (wid) *wid = e->wid;
        return AX_OK;
    }
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

id cx_ax_find(int kind, int idn) {
    if (kind == AXK_SYS) return cx_ax_system();
    if (kind == AXK_APP) {
        if (idn && idn != getpid()) return 0;
        return ax_new(AXK_APP, getpid(), 0);
    }
    for (int i = 0; i < g_ntab; i++) {
        if (g_tab[i].id == idn && (kind == 0 || g_tab[i].kind == kind))
            return wrap_obj(g_tab[i].obj);
    }
    if (kind == AXK_WIN || kind == 0) {
        id app = nsapp();
        id wins = app ? MSG0(id, app, "orderedWindows") : 0;
        u64 n = wins ? MSG0(u64, wins, "count") : 0;
        for (u64 i = 0; i < n; i++) {
            id w = MSG(id, wins, "objectAtIndex:", i);
            if (w && cx_appkit_window_number(w) == idn) return wrap_obj(w);
        }
    }
    return 0;
}

int cx_ax_kind_id(id obj, int *kind, int *idn) {
    axel_t *e = ax_of(obj);
    if (e) {
        if (kind) *kind = e->kind;
        if (idn) *idn = e->elem_id;
        return 1;
    }
    if (!obj) return 0;
    int k = is_a(obj, "NSWindow") ? AXK_WIN : AXK_VIEW;
    int idv = id_for(obj, k);
    if (kind) *kind = k;
    if (idn) *idn = idv;
    return 1;
}

int cx_ax_matches(id elem, id obj) {
    axel_t *e = ax_of(elem);
    if (!e) return 0;
    if (e->kind == AXK_APP && (!e->pid || e->pid == getpid())) return 1;
    if (e->kind == AXK_SYS) return 1;
    if (!obj) return 0;
    if (e->obj == obj) return 1;
    if (e->kind == AXK_WIN && !is_a(obj, "NSWindow")) {
        id w = MSG(BOOL, obj, "respondsToSelector:", sel_registerName("window")) ? MSG0(id, obj, "window") : 0;
        return w && w == e->obj;
    }
    return 0;
}

static const char *text_cstr(id obj, id *held) {
    id v = 0;
    *held = 0;
    if (!obj) return 0;
    if (is_a(obj, "NSTextView") && MSG(BOOL, obj, "respondsToSelector:", sel_registerName("string")))
        v = MSG0(id, obj, "string");
    else if (is_a(obj, "NSTextField") && MSG(BOOL, obj, "respondsToSelector:", sel_registerName("stringValue")))
        v = MSG0(id, obj, "stringValue");
    else return 0;
    if (!v) return "";
    if (!cx_is_str(v)) return "";
    *held = objc_retain(v);
    return cx_cstr(*held);
}

static id range_value(id param, long *loc, long *len) {
    long rg[2] = {0, 0};
    if (!param || !AXValueGetValue(param, 4, rg)) return (id)0;
    *loc = rg[0];
    *len = rg[1];
    return (id)1;
}

int cx_ax_param(id elem, id attr, id param, id *v) {
    long loc = 0, len = 0;
    id held = 0;
    const char *text, *name;
    if (v) *v = 0;
    int g = ax_gate();
    if (g) return g;
    axel_t *e = ax_of(elem);
    if (!e || !attr) return e ? AX_BADARG : AX_BAD_ELEM;
    name = cx_cstr(attr);
    if (ax_remote(e)) {
        ax_msg out;
        int px = 0, py = 0, st;
        if (!strcmp(name, "AXRangeForPosition")) {
            double pt[2] = {0, 0};
            if (param && AXValueGetValue(param, 1, pt)) { px = (int)pt[0]; py = (int)pt[1]; }
        } else if (!range_value(param, &loc, &len)) return AX_BADARG;
        st = remote_roundtrip(e, AX_OP_PARAM, name, 0, (int)loc, (int)len, px, py, &out);
        if (st) return st;
        if (v) *v = msg_value(e, &out, name);
        return AX_OK;
    }
    if (e->kind != AXK_VIEW || !e->obj) return AX_NOATTR;
    text = text_cstr(e->obj, &held);
    if (!text) return AX_NOATTR;
    if (!strcmp(name, "AXStringForRange")) {
        long n = (long)strlen(text);
        char buf[AX_STR_MAX];
        if (!range_value(param, &loc, &len)) { objc_release(held); return AX_BADARG; }
        if (loc < 0) loc = 0;
        if (loc > n) loc = n;
        if (len < 0) len = 0;
        if (loc + len > n) len = n - loc;
        if (len >= (long)sizeof buf) len = (long)sizeof buf - 1;
        memcpy(buf, text + loc, (unsigned long)len);
        buf[len] = 0;
        if (v) *v = cx_str(buf);
        objc_release(held);
        return AX_OK;
    }
    if (!strcmp(name, "AXBoundsForRange")) {
        double x, y, w, h;
        double r[4];
        if (!range_value(param, &loc, &len)) { objc_release(held); return AX_BADARG; }
        if (!frame_of(e, &x, &y, &w, &h)) { x = y = 0; }
        r[0] = x + (double)loc * 7.0;
        r[1] = y;
        r[2] = (double)(len < 0 ? 0 : len) * 7.0;
        r[3] = 16;
        if (v) *v = AXValueCreate(3, r);
        objc_release(held);
        return AX_OK;
    }
    if (!strcmp(name, "AXRangeForPosition")) {
        double pt[2] = {0, 0};
        double x, y, w, h;
        long n = (long)strlen(text);
        long idx;
        long rg[2];
        if (!param || !AXValueGetValue(param, 1, pt)) { objc_release(held); return AX_BADARG; }
        if (!frame_of(e, &x, &y, &w, &h)) x = 0;
        idx = (long)((pt[0] - x) / 7.0);
        if (idx < 0) idx = 0;
        if (idx > n) idx = n;
        rg[0] = idx;
        rg[1] = idx < n ? 1 : 0;
        if (v) *v = AXValueCreate(4, rg);
        objc_release(held);
        return AX_OK;
    }
    objc_release(held);
    return AX_NOATTR;
}

int cx_ax_param_names(id elem, id *names) {
    id held = 0;
    const char *text;
    if (names) *names = 0;
    int g = ax_gate();
    if (g) return g;
    axel_t *e = ax_of(elem);
    if (!e) return AX_BAD_ELEM;
    if (ax_remote(e)) {
        const char *list[] = {"AXBoundsForRange", "AXStringForRange", "AXRangeForPosition"};
        id tmp[3];
        for (int i = 0; i < 3; i++) tmp[i] = cx_str(list[i]);
        if (names) *names = CFArrayCreate(0, tmp, 3, 0);
        for (int i = 0; i < 3; i++) objc_release(tmp[i]);
        return AX_OK;
    }
    text = (e->kind == AXK_VIEW) ? text_cstr(e->obj, &held) : 0;
    if (!text) {
        if (names) *names = CFArrayCreate(0, 0, 0, 0);
        return AX_OK;
    }
    {
        const char *list[] = {"AXBoundsForRange", "AXStringForRange", "AXRangeForPosition"};
        id tmp[3];
        for (int i = 0; i < 3; i++) tmp[i] = cx_str(list[i]);
        if (names) *names = CFArrayCreate(0, tmp, 3, 0);
        for (int i = 0; i < 3; i++) objc_release(tmp[i]);
    }
    objc_release(held);
    return AX_OK;
}
