/* Centro local de notificações. Sem classes: o teste de host e o ObjC usam estas funções. */
#ifdef CX_UN_HOST
#include <string.h>
#else
#include "foundation.h"
#include "libc.h"
#endif
#include "un_center.h"

enum { UN_CAP = 24, UN_ID = 64, UN_TITLE = 96, UN_BODY = 160 };

typedef struct {
    int used, pending, delivered, repeats, token;
    double interval, fire_at;
    char ident[UN_ID];
    char title[UN_TITLE];
    char body[UN_BODY];
} un_req_t;

static un_req_t g_req[UN_CAP];
static int g_auth;
static int g_tok = 1;
static int g_present_n, g_receive_n;
static double g_clock;
static char g_action[80];
static char g_last_ident[UN_ID];
static cx_un_present_fn g_present;
static cx_un_receive_fn g_receive;
static cx_un_banner_fn g_sink;
static void *g_ctx, *g_sink_ctx;
static int g_busy;

static struct {
    int live, token;
    char ident[UN_ID];
    char title[UN_TITLE];
    char body[UN_BODY];
} g_ban;

static void scopy(char *d, int cap, const char *s) {
    int i = 0;
    if (!s) s = "";
    if (cap < 1) return;
    for (; i < cap - 1 && s[i]; i++) d[i] = s[i];
    d[i] = 0;
}

static int same(const char *a, const char *b) {
    if (!a) a = "";
    if (!b) b = "";
    return strcmp(a, b) == 0;
}

static double un_now(void) {
#ifdef CX_UN_HOST
    return g_clock;
#else
    return cx_now();
#endif
}

static void deliver(un_req_t *r) {
    int opts = CX_UN_PRESENT_BANNER | CX_UN_PRESENT_LIST | CX_UN_PRESENT_SOUND;
    if (g_busy > 4) return;
    g_busy++;
    r->delivered = 1;
    r->token = g_tok++;
    if (g_tok < 1) g_tok = 1;
    if (r->repeats && r->interval > 0) {
        r->pending = 1;
        r->fire_at = un_now() + r->interval;
    } else {
        r->pending = 0;
    }
    if (g_present) {
        g_present_n++;
        opts = g_present(r->ident, r->title, r->body, g_ctx);
    }
    if (opts & (CX_UN_PRESENT_BANNER | CX_UN_PRESENT_ALERT)) {
        g_ban.live = 1;
        g_ban.token = r->token;
        scopy(g_ban.ident, UN_ID, r->ident);
        scopy(g_ban.title, UN_TITLE, r->title);
        scopy(g_ban.body, UN_BODY, r->body);
        if (g_sink) g_sink(r->title, r->body, r->token, g_sink_ctx);
    }
    g_busy--;
}

void cx_un_reset(void) {
    memset(g_req, 0, sizeof g_req);
    memset(&g_ban, 0, sizeof g_ban);
    g_auth = 0;
    g_tok = 1;
    g_present_n = 0;
    g_receive_n = 0;
    g_clock = 0;
    g_action[0] = 0;
    g_last_ident[0] = 0;
    g_present = 0;
    g_receive = 0;
    g_sink = 0;
    g_ctx = 0;
    g_sink_ctx = 0;
    g_busy = 0;
}

void cx_un_set_auth(int status) { g_auth = status; }
int cx_un_auth(void) { return g_auth; }

void cx_un_set_delegate(cx_un_present_fn present, cx_un_receive_fn receive, void *ctx) {
    g_present = present;
    g_receive = receive;
    g_ctx = ctx;
}

void cx_un_set_banner_sink(cx_un_banner_fn fn, void *ctx) {
    g_sink = fn;
    g_sink_ctx = ctx;
}

static un_req_t *find_ident(const char *ident, int pending_only) {
    for (int i = 0; i < UN_CAP; i++) {
        if (!g_req[i].used || !same(g_req[i].ident, ident)) continue;
        if (pending_only && !g_req[i].pending) continue;
        return &g_req[i];
    }
    return 0;
}

static void free_if_idle(un_req_t *r) {
    if (r && !r->pending && !r->delivered) {
        memset(r, 0, sizeof *r);
    }
}

int cx_un_add(const char *ident, const char *title, const char *subtitle, const char *body,
              double interval, int repeats) {
    un_req_t *r;
    char shown[UN_BODY];
    if (g_auth != CX_UN_AUTHORIZED) return CX_UN_ERR_DENIED;
    if (!ident) return CX_UN_ERR_BAD;
    cx_un_flush();
    scopy(shown, UN_BODY, (body && body[0]) ? body : subtitle);
    r = find_ident(ident, 1);
    if (!r) {
        for (int i = 0; i < UN_CAP; i++) if (!g_req[i].used) { r = &g_req[i]; break; }
    }
    if (!r) return CX_UN_ERR_FULL;
    memset(r, 0, sizeof *r);
    r->used = 1;
    scopy(r->ident, UN_ID, ident);
    scopy(r->title, UN_TITLE, title);
    scopy(r->body, UN_BODY, shown);
    r->repeats = repeats ? 1 : 0;
    r->interval = interval > 0 ? interval : 0;
    if (r->interval <= 0) {
        r->repeats = 0;
        deliver(r);
    } else {
        r->pending = 1;
        r->fire_at = un_now() + r->interval;
    }
    return CX_UN_OK;
}

int cx_un_remove_pending(const char *ident) {
    int n = 0;
    for (int i = 0; i < UN_CAP; i++) {
        un_req_t *r = &g_req[i];
        if (!r->used || !r->pending) continue;
        if (ident && !same(r->ident, ident)) continue;
        r->pending = 0;
        r->repeats = 0;
        n++;
        free_if_idle(r);
    }
    return n;
}

int cx_un_remove_delivered(const char *ident) {
    int n = 0;
    for (int i = 0; i < UN_CAP; i++) {
        un_req_t *r = &g_req[i];
        if (!r->used || !r->delivered) continue;
        if (ident && !same(r->ident, ident)) continue;
        r->delivered = 0;
        n++;
        if (g_ban.live && same(g_ban.ident, r->ident)) g_ban.live = 0;
        free_if_idle(r);
    }
    return n;
}

int cx_un_pending_count(void) {
    int n = 0;
    for (int i = 0; i < UN_CAP; i++) if (g_req[i].used && g_req[i].pending) n++;
    return n;
}

int cx_un_delivered_count(void) {
    int n = 0;
    for (int i = 0; i < UN_CAP; i++) if (g_req[i].used && g_req[i].delivered) n++;
    return n;
}

int cx_un_pending_has(const char *ident) { return find_ident(ident, 1) != 0; }

int cx_un_delivered_has(const char *ident) {
    for (int i = 0; i < UN_CAP; i++)
        if (g_req[i].used && g_req[i].delivered && same(g_req[i].ident, ident)) return 1;
    return 0;
}

void cx_un_flush(void) {
    for (int i = 0; i < UN_CAP; i++) {
        un_req_t *r = &g_req[i];
        if (!r->used || !r->pending || r->interval <= 0) continue;
        if (r->fire_at <= un_now()) deliver(r);
    }
}

void cx_un_set_clock(double t) {
    g_clock = t;
    cx_un_flush();
}

void cx_un_advance(double dt) {
    if (dt < 0) dt = 0;
    g_clock += dt;
    cx_un_flush();
}

int cx_un_banner_visible(void) { return g_ban.live; }
const char *cx_un_banner_title(void) { return g_ban.live ? g_ban.title : ""; }
const char *cx_un_banner_body(void) { return g_ban.live ? g_ban.body : ""; }
int cx_un_banner_token(void) { return g_ban.live ? g_ban.token : 0; }

int cx_un_banner_click(void) {
    char ident[UN_ID];
    if (!g_ban.live) return 0;
    scopy(ident, UN_ID, g_ban.ident);
    g_ban.live = 0;
    scopy(g_last_ident, UN_ID, ident);
    scopy(g_action, (int)sizeof g_action, CX_UN_ACTION_DEFAULT);
    g_receive_n++;
    if (g_receive) g_receive(ident, CX_UN_ACTION_DEFAULT, g_ctx);
    return 1;
}

int cx_un_click_token(int token) {
    if (!g_ban.live || token <= 0 || g_ban.token != token) return 0;
    return cx_un_banner_click();
}

int cx_un_present_count(void) { return g_present_n; }
int cx_un_receive_count(void) { return g_receive_n; }
const char *cx_un_last_action(void) { return g_action; }
const char *cx_un_last_ident(void) { return g_last_ident; }
