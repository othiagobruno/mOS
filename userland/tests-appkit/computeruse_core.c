#include "computeruse_core.h"

enum { CU_MAX_WIN = 4, CU_MAX_CTL = 8 };

typedef struct {
    int id, pid, onscreen, layer, x, y, w, h, scroll;
    char owner[32];
    char name[64];
    char bundle[96];
    unsigned color;
} cu_win;

typedef struct {
    int id, win, kind, x, y, w, h;
    int clicks, presses, seq;
    unsigned color;
    char role[24];
    char title[40];
    char value[80];
} cu_ctl;

struct cu_world {
    int ready, done, grant_screen, grant_ax;
    int app_clicks, app_presses, app_scroll, app_text_n;
    char app_text[80];
    int cursor_x, cursor_y, last_tap, focus;
    int nwin, nctl, next_id;
    cu_win wins[CU_MAX_WIN];
    cu_ctl ctls[CU_MAX_CTL];
    unsigned fb[CU_W * CU_H];
    int focus_pid;
    int post_seq, post_pid, post_type, post_x, post_y, post_extra;
    int note_seq, note_pid, note_kind, note_id;
    char note_name[48];
    int ax_seq, ax_pid, ax_n, ax_rn, ax_busy;
    unsigned char ax_req[640];
    unsigned char ax_rep[640];
};

unsigned cu_world_bytes_size(void) { return (unsigned)sizeof(cu_world); }
cu_world *cu_world_bytes(void *mem) { return (cu_world *)mem; }

static int slen(const char *s) { int n = 0; if (!s) return 0; while (s[n]) n++; return n; }
static void scopy(char *d, int cap, const char *s) {
    int i = 0;
    if (!s) s = "";
    if (cap < 1) return;
    for (; i < cap - 1 && s[i]; i++) d[i] = s[i];
    d[i] = 0;
}
static int scmp(const char *a, const char *b) {
    if (!a) a = "";
    if (!b) b = "";
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

static void fill(cu_world *w, int x, int y, int rw, int rh, unsigned color) {
    for (int row = 0; row < rh; row++) {
        int yy = y + row;
        if (yy < 0 || yy >= CU_H) continue;
        for (int col = 0; col < rw; col++) {
            int xx = x + col;
            if (xx < 0 || xx >= CU_W) continue;
            w->fb[(unsigned)yy * CU_W + (unsigned)xx] = color;
        }
    }
}

void cu_init(cu_world *w) {
    unsigned char *p = (unsigned char *)w;
    for (unsigned i = 0; i < sizeof *w; i++) p[i] = 0;
    w->focus = -1;
    w->next_id = 1;
    for (unsigned i = 0; i < CU_W * CU_H; i++) w->fb[i] = 0xFF101010u;
}

void cu_grant(cu_world *w, const char *service) {
    if (!scmp(service, "kTCCServiceScreenCapture"))
        __atomic_store_n(&w->grant_screen, 1, __ATOMIC_RELEASE);
    if (!scmp(service, "kTCCServiceAccessibility"))
        __atomic_store_n(&w->grant_ax, 1, __ATOMIC_RELEASE);
}

static int screen_ok(cu_world *w) { return __atomic_load_n(&w->grant_screen, __ATOMIC_ACQUIRE); }
static int ax_ok(cu_world *w) { return __atomic_load_n(&w->grant_ax, __ATOMIC_ACQUIRE); }

int cu_open_window(cu_world *w, int pid, const char *owner, const char *name, int x, int y, int wd, int ht, unsigned bg) {
    if (w->nwin >= CU_MAX_WIN) return 0;
    cu_win *win = &w->wins[w->nwin++];
    win->id = w->next_id++;
    win->pid = pid;
    win->onscreen = 1;
    win->layer = w->nwin;
    win->x = x; win->y = y; win->w = wd; win->h = ht;
    win->color = bg;
    scopy(win->owner, 32, owner);
    scopy(win->name, 64, name);
    fill(w, x, y, wd, ht, bg);
    return win->id;
}

static cu_win *win_by_id(cu_world *w, int id) {
    for (int i = 0; i < w->nwin; i++) if (w->wins[i].id == id) return &w->wins[i];
    return 0;
}
static cu_ctl *ctl_by_id(cu_world *w, int id) {
    for (int i = 0; i < w->nctl; i++) if (w->ctls[i].id == id) return &w->ctls[i];
    return 0;
}

static int add_ctl(cu_world *w, int win, int kind, const char *role, const char *title, int x, int y, int wd, int ht, unsigned color) {
    cu_win *owner = win_by_id(w, win);
    if (!owner || w->nctl >= CU_MAX_CTL) return 0;
    cu_ctl *c = &w->ctls[w->nctl++];
    c->id = w->next_id++;
    c->win = win;
    c->kind = kind;
    c->x = x; c->y = y; c->w = wd; c->h = ht;
    c->color = color;
    scopy(c->role, 24, role);
    scopy(c->title, 40, title);
    c->value[0] = 0;
    fill(w, owner->x + x, owner->y + y, wd, ht, color);
    return c->id;
}

int cu_add_button(cu_world *w, int win, const char *title, int x, int y, int wd, int ht, unsigned color) {
    return add_ctl(w, win, CU_BUTTON, "AXButton", title, x, y, wd, ht, color);
}
int cu_add_field(cu_world *w, int win, int x, int y, int wd, int ht) {
    return add_ctl(w, win, CU_FIELD, "AXTextField", "", x, y, wd, ht, 0xFFFFFFFFu);
}

int cu_capture(cu_world *w, int win, unsigned *px, int *out_w, int *out_h) {
    if (!screen_ok(w)) return -25211;
    int x = 0, y = 0, wd = CU_W, ht = CU_H;
    if (win) {
        cu_win *found = win_by_id(w, win);
        if (!found) return -25200;
        x = found->x; y = found->y; wd = found->w; ht = found->h;
    }
    if (out_w) *out_w = wd;
    if (out_h) *out_h = ht;
    if (!px) return 0;
    for (int row = 0; row < ht; row++)
        for (int col = 0; col < wd; col++) {
            int sx = x + col, sy = y + row;
            unsigned v = 0;
            if (sx >= 0 && sy >= 0 && sx < CU_W && sy < CU_H) v = w->fb[(unsigned)sy * CU_W + (unsigned)sx];
            px[(unsigned)row * (unsigned)wd + (unsigned)col] = v;
        }
    return 0;
}

int cu_list(cu_world *w, cu_info *out, int cap) {
    if (!screen_ok(w)) return -25211;
    int n = 0;
    for (int i = w->nwin - 1; i >= 0 && n < cap; i--) {
        cu_win *src = &w->wins[i];
        cu_info *d = &out[n++];
        d->id = src->id; d->pid = src->pid; d->layer = src->layer; d->onscreen = src->onscreen;
        d->x = src->x; d->y = src->y; d->w = src->w; d->h = src->h;
        scopy(d->owner, 32, src->owner);
        scopy(d->name, 64, src->name);
    }
    return n;
}

static cu_win *win_at(cu_world *w, int x, int y) {
    for (int i = w->nwin - 1; i >= 0; i--) {
        cu_win *win = &w->wins[i];
        if (!win->onscreen) continue;
        if (x >= win->x && y >= win->y && x < win->x + win->w && y < win->y + win->h) return win;
    }
    return 0;
}
static cu_ctl *ctl_at(cu_world *w, cu_win *win, int x, int y) {
    if (!win) return 0;
    for (int i = w->nctl - 1; i >= 0; i--) {
        cu_ctl *c = &w->ctls[i];
        if (c->win != win->id) continue;
        int sx = win->x + c->x, sy = win->y + c->y;
        if (x >= sx && y >= sy && x < sx + c->w && y < sy + c->h) return c;
    }
    return 0;
}

int cu_post_mouse(cu_world *w, int tap, int type, int x, int y) {
    if (!ax_ok(w)) return -25211;
    w->last_tap = tap;
    w->cursor_x = x;
    w->cursor_y = y;
    if (type != 1) return 0;
    cu_win *win = win_at(w, x, y);
    cu_ctl *c = ctl_at(w, win, x, y);
    if (!c) { w->focus = -1; return 0; }
    if (c->kind == CU_BUTTON) __atomic_fetch_add(&c->clicks, 1, __ATOMIC_RELEASE);
    if (c->kind == CU_FIELD) w->focus = c->id;
    return 0;
}

static void emit_note(cu_world *w, cu_ctl *c, const char *name) {
    cu_win *win = win_by_id(w, c->win);
    scopy(w->note_name, 48, name);
    w->note_pid = win ? win->pid : 0;
    w->note_kind = CU_KIND_CTL;
    w->note_id = c->id;
    __atomic_fetch_add(&w->note_seq, 1, __ATOMIC_RELEASE);
}

static void write_value(cu_world *w, cu_ctl *c, const char *s) {
    __atomic_fetch_add(&c->seq, 1, __ATOMIC_RELEASE);
    scopy(c->value, 80, s);
    __atomic_fetch_add(&c->seq, 1, __ATOMIC_RELEASE);
    emit_note(w, c, "AXValueChanged");
}

int cu_post_key(cu_world *w, int tap, unsigned unicode, int down) {
    if (!ax_ok(w)) return -25211;
    w->last_tap = tap;
    if (!down || unicode < 32 || unicode > 126) return 0;
    cu_ctl *c = ctl_by_id(w, w->focus);
    if (!c || c->kind != CU_FIELD) return 0;
    __atomic_fetch_add(&c->seq, 1, __ATOMIC_RELEASE);
    int n = slen(c->value);
    if (n < 79) { c->value[n] = (char)unicode; c->value[n + 1] = 0; }
    __atomic_fetch_add(&c->seq, 1, __ATOMIC_RELEASE);
    emit_note(w, c, "AXValueChanged");
    return 0;
}

int cu_post_scroll(cu_world *w, int tap, int dy) {
    if (!ax_ok(w)) return -25211;
    w->last_tap = tap;
    cu_win *win = win_at(w, w->cursor_x, w->cursor_y);
    if (win && dy) __atomic_fetch_add(&win->scroll, dy, __ATOMIC_RELEASE);
    return 0;
}

static void box_of_ctl(cu_world *w, cu_ctl *c, int *x, int *y, int *wd, int *ht) {
    cu_win *win = win_by_id(w, c->win);
    *x = (win ? win->x : 0) + c->x;
    *y = (win ? win->y : 0) + c->y;
    *wd = c->w;
    *ht = c->h;
}

int cu_ax_copy(cu_world *w, int kind, int id, const char *attr, cu_attr *out) {
    if (out) { unsigned char *p = (unsigned char *)out; for (unsigned i = 0; i < sizeof *out; i++) p[i] = 0; }
    if (!ax_ok(w)) return -25211;
    if (!out || !attr) return -25201;
    if (kind == CU_KIND_WIN) {
        cu_win *win = win_by_id(w, id);
        if (!win) return -25202;
        if (!scmp(attr, "AXRole")) { out->type = CU_ATTR_STR; scopy(out->str, 80, "AXWindow"); return 0; }
        if (!scmp(attr, "AXTitle")) { out->type = CU_ATTR_STR; scopy(out->str, 80, win->name); return 0; }
        if (!scmp(attr, "AXPosition")) { out->type = CU_ATTR_BOX; out->x = win->x; out->y = win->y; return 0; }
        if (!scmp(attr, "AXSize")) { out->type = CU_ATTR_BOX; out->w = win->w; out->h = win->h; return 0; }
        if (!scmp(attr, "AXChildren")) {
            out->type = CU_ATTR_KIDS;
            for (int i = 0; i < w->nctl && out->nkids < 8; i++)
                if (w->ctls[i].win == win->id) {
                    out->kid_kind[out->nkids] = CU_KIND_CTL;
                    out->kid_id[out->nkids] = w->ctls[i].id;
                    out->nkids++;
                }
            return 0;
        }
        return -25205;
    }
    if (kind == CU_KIND_CTL) {
        cu_ctl *c = ctl_by_id(w, id);
        if (!c) return -25202;
        if (!scmp(attr, "AXRole")) { out->type = CU_ATTR_STR; scopy(out->str, 80, c->role); return 0; }
        if (!scmp(attr, "AXTitle")) { out->type = CU_ATTR_STR; scopy(out->str, 80, c->title); return 0; }
        if (!scmp(attr, "AXValue")) {
            int s1 = __atomic_load_n(&c->seq, __ATOMIC_ACQUIRE);
            if (s1 & 1) return -25204;
            scopy(out->str, 80, c->value);
            int s2 = __atomic_load_n(&c->seq, __ATOMIC_ACQUIRE);
            if (s1 != s2) return -25204;
            out->type = CU_ATTR_STR;
            return 0;
        }
        if (!scmp(attr, "AXPosition") || !scmp(attr, "AXSize")) {
            int x, y, wd, ht;
            box_of_ctl(w, c, &x, &y, &wd, &ht);
            out->type = CU_ATTR_BOX;
            out->x = x; out->y = y; out->w = wd; out->h = ht;
            return 0;
        }
        if (!scmp(attr, "AXFocused")) { out->type = CU_ATTR_INT; out->i = w->focus == c->id; return 0; }
        return -25205;
    }
    return -25201;
}

int cu_ax_press(cu_world *w, int kind, int id) {
    if (!ax_ok(w)) return -25211;
    if (kind != CU_KIND_CTL) return -25206;
    cu_ctl *c = ctl_by_id(w, id);
    if (!c || c->kind != CU_BUTTON) return -25206;
    __atomic_fetch_add(&c->presses, 1, __ATOMIC_RELEASE);
    return 0;
}

int cu_ax_set(cu_world *w, int kind, int id, const char *attr, const char *value) {
    if (!ax_ok(w)) return -25211;
    if (kind != CU_KIND_CTL || scmp(attr, "AXValue")) return -25205;
    cu_ctl *c = ctl_by_id(w, id);
    if (!c || c->kind != CU_FIELD) return -25205;
    write_value(w, c, value ? value : "");
    return 0;
}

static cu_win *front_of_pid(cu_world *w, int pid) {
    for (int i = w->nwin - 1; i >= 0; i--) if (w->wins[i].pid == pid && w->wins[i].onscreen) return &w->wins[i];
    return 0;
}
static cu_win *win_of_pid_at(cu_world *w, int pid, int x, int y) {
    for (int i = w->nwin - 1; i >= 0; i--) {
        cu_win *win = &w->wins[i];
        if (win->pid != pid || !win->onscreen) continue;
        if (x >= win->x && y >= win->y && x < win->x + win->w && y < win->y + win->h) return win;
    }
    return front_of_pid(w, pid);
}
static cu_ctl *field_of_pid(cu_world *w, int pid) {
    for (int i = 0; i < w->nctl; i++) {
        cu_ctl *c = &w->ctls[i];
        cu_win *win = win_by_id(w, c->win);
        if (c->kind == CU_FIELD && win && win->pid == pid) return c;
    }
    return 0;
}

int cu_post_to_pid(cu_world *w, int pid, int type, int x, int y, int extra) {
    if (!ax_ok(w)) return -25211;
    if (type == 22) {
        cu_win *win = win_of_pid_at(w, pid, x, y);
        if (win && extra) __atomic_fetch_add(&win->scroll, extra, __ATOMIC_RELEASE);
        return win ? 0 : -3;
    }
    if (type == 1) {
        cu_win *win = win_of_pid_at(w, pid, x, y);
        cu_ctl *c = ctl_at(w, win, x, y);
        if (c && c->kind == CU_BUTTON) __atomic_fetch_add(&c->clicks, 1, __ATOMIC_RELEASE);
        return 0;
    }
    if (type == 10) {
        cu_ctl *c = field_of_pid(w, pid);
        if (!c || extra < 32 || extra > 126) return 0;
        __atomic_fetch_add(&c->seq, 1, __ATOMIC_RELEASE);
        int n = slen(c->value);
        if (n < 79) { c->value[n] = (char)extra; c->value[n + 1] = 0; }
        __atomic_fetch_add(&c->seq, 1, __ATOMIC_RELEASE);
        emit_note(w, c, "AXValueChanged");
        return 0;
    }
    return -25201;
}

void cu_activate(cu_world *w, int pid) { w->focus_pid = pid; }
int cu_front_pid(cu_world *w) { return w->focus_pid; }

int cu_running(cu_world *w, int index, int *pid, char *name, int cap) {
    int seen = 0;
    for (int i = 0; i < w->nwin; i++) {
        int dup = 0;
        for (int j = 0; j < i; j++) if (w->wins[j].pid == w->wins[i].pid) dup = 1;
        if (dup) continue;
        if (seen++ != index) continue;
        if (pid) *pid = w->wins[i].pid;
        if (name) scopy(name, cap, w->wins[i].owner);
        return 1;
    }
    return 0;
}

int cu_post_begin(cu_world *w, int pid, int type, int x, int y, int extra) {
    int seq = __atomic_load_n(&w->post_seq, __ATOMIC_ACQUIRE);
    if (seq & 1) return -1;
    w->post_pid = pid;
    w->post_type = type;
    w->post_x = x;
    w->post_y = y;
    w->post_extra = extra;
    __atomic_store_n(&w->post_seq, seq + 1, __ATOMIC_RELEASE);
    return seq;
}
int cu_post_seq(cu_world *w) { return __atomic_load_n(&w->post_seq, __ATOMIC_ACQUIRE); }
void cu_post_poll(cu_world *w, int pid) {
    int seq = __atomic_load_n(&w->post_seq, __ATOMIC_ACQUIRE);
    if (!(seq & 1) || w->post_pid != pid) return;
    cu_post_to_pid(w, w->post_pid, w->post_type, w->post_x, w->post_y, w->post_extra);
    __atomic_store_n(&w->post_seq, seq + 1, __ATOMIC_RELEASE);
}

int cu_note_seq(cu_world *w) { return __atomic_load_n(&w->note_seq, __ATOMIC_ACQUIRE); }
void cu_note_read(cu_world *w, int *pid, int *kind, int *id, char *name, int cap) {
    if (pid) *pid = w->note_pid;
    if (kind) *kind = w->note_kind;
    if (id) *id = w->note_id;
    if (name) scopy(name, cap, w->note_name);
}

int cu_ax_begin(cu_world *w, int pid, const void *req, int n) {
    int seq = __atomic_load_n(&w->ax_seq, __ATOMIC_ACQUIRE);
    if (seq & 1 || n < 0 || n > 640 || !req) return -1;
    for (int i = 0; i < n; i++) w->ax_req[i] = ((const unsigned char *)req)[i];
    w->ax_pid = pid;
    w->ax_n = n;
    w->ax_busy = 0;
    __atomic_store_n(&w->ax_seq, seq + 1, __ATOMIC_RELEASE);
    return seq;
}
int cu_ax_seq(cu_world *w) { return __atomic_load_n(&w->ax_seq, __ATOMIC_ACQUIRE); }
int cu_ax_poll(cu_world *w, int pid, void *req, int cap) {
    int seq = __atomic_load_n(&w->ax_seq, __ATOMIC_ACQUIRE);
    int n;
    if (!(seq & 1) || w->ax_pid != pid || w->ax_busy) return 0;
    w->ax_busy = 1;
    n = w->ax_n;
    if (n > cap) n = cap;
    if (req) for (int i = 0; i < n; i++) ((unsigned char *)req)[i] = w->ax_req[i];
    return w->ax_n;
}
void cu_ax_finish(cu_world *w, const void *rep, int n) {
    int seq = __atomic_load_n(&w->ax_seq, __ATOMIC_ACQUIRE);
    if (n < 0) n = 0;
    if (n > 640) n = 640;
    for (int i = 0; i < n; i++) w->ax_rep[i] = ((const unsigned char *)rep)[i];
    w->ax_rn = n;
    w->ax_busy = 0;
    __atomic_store_n(&w->ax_seq, seq + 1, __ATOMIC_RELEASE);
}
int cu_ax_reply(cu_world *w, void *rep, int cap) {
    int n = w->ax_rn;
    if (n > cap) n = cap;
    if (rep) for (int i = 0; i < n; i++) ((unsigned char *)rep)[i] = w->ax_rep[i];
    return w->ax_rn;
}

int cu_ctl_clicks(cu_world *w, int id) {
    cu_ctl *c = ctl_by_id(w, id);
    return c ? __atomic_load_n(&c->clicks, __ATOMIC_ACQUIRE) : 0;
}
int cu_ctl_presses(cu_world *w, int id) {
    cu_ctl *c = ctl_by_id(w, id);
    return c ? __atomic_load_n(&c->presses, __ATOMIC_ACQUIRE) : 0;
}
int cu_win_scroll(cu_world *w, int id) {
    cu_win *win = win_by_id(w, id);
    return win ? __atomic_load_n(&win->scroll, __ATOMIC_ACQUIRE) : 0;
}
static int *slot_ptr(cu_world *w, int slot) {
    switch (slot) {
    case CU_READY: return &w->ready;
    case CU_DONE: return &w->done;
    case CU_APP_CLICKS: return &w->app_clicks;
    case CU_APP_PRESSES: return &w->app_presses;
    case CU_APP_SCROLL: return &w->app_scroll;
    case CU_APP_TEXT_N: return &w->app_text_n;
    default: return 0;
    }
}
int cu_load(cu_world *w, int slot) {
    int *p = slot_ptr(w, slot);
    return p ? __atomic_load_n(p, __ATOMIC_ACQUIRE) : 0;
}
void cu_store(cu_world *w, int slot, int v) {
    int *p = slot_ptr(w, slot);
    if (p) __atomic_store_n(p, v, __ATOMIC_RELEASE);
}
void cu_publish_text(cu_world *w, const char *s, int n) {
    scopy(w->app_text, 80, s);
    __atomic_store_n(&w->app_text_n, n, __ATOMIC_RELEASE);
}
void cu_copy_text(cu_world *w, char *dst, int cap) { scopy(dst, cap, w->app_text); }

int cu_read_value(cu_world *w, int id, char *dst, int cap) {
    cu_ctl *c = ctl_by_id(w, id);
    if (!c) { if (dst && cap) dst[0] = 0; return 0; }
    for (;;) {
        int s1 = __atomic_load_n(&c->seq, __ATOMIC_ACQUIRE);
        if (s1 & 1) continue;
        scopy(dst, cap, c->value);
        int s2 = __atomic_load_n(&c->seq, __ATOMIC_ACQUIRE);
        if (s1 == s2) return slen(dst);
    }
}
