/* AXObserver: avisos reais do AppKit deste processo (valor, foco, janela criada
 * ou destruída). Outro pid só entrega quando o transporte AX estiver ligado.
 * Com a permissão de acessibilidade o retorno é 0. Sem ela, -25211. Não há -25208. */
#include "foundation.h"
#include "axproto.h"
#include "axtransport.h"

int cx_tcc_preflight(const char *service);
int getpid(void);
int strcmp(const char *a, const char *b);
void *memset(void *d, int c, size_t n);
int snprintf(char *b, size_t n, const char *fmt, ...);
void *calloc(size_t n, size_t s);
void *CFRunLoopSourceCreate(void *alloc, long order, void *ctx);
void CFRunLoopSourceSignal(void *s);
void *CFRunLoopGetCurrent(void);
void CFRunLoopWakeUp(void *rl);
int cx_ax_matches(id elem, id obj);
int cx_ax_kind_id(id obj, int *kind, int *id);
void cx_ax_server_note(int kind, int id, const char *note);
id objc_retain(id o);

enum { AX_OK = 0, AX_BADARG = -25201, AX_NOATTR = -25205, AX_OFF = -25211 };
enum { MAX_REG = 12, MAX_Q = 16 };

typedef void (*ax_cb)(id obs, id elem, id note, void *refcon);
typedef void (*ax_cb_info)(id obs, id elem, id note, id info, void *refcon);

typedef struct {
    long version;
    void *info;
    void *retain, *release, *copyDescription, *equal, *hash;
    void *schedule, *cancel;
    void (*perform)(void *info);
} ax_src_ctx;

typedef struct {
    id elem;
    char note[48];
    void *refcon;
    int used;
} ax_reg;

typedef struct ax_obs_st {
    id self;
    int pid;
    int with_info;
    void *cb;
    id source;
    ax_reg reg[MAX_REG];
    struct { id elem; char note[48]; void *refcon; } q[MAX_Q];
    int qh, qt;
} ax_obs_st;

typedef struct { cx_class_t *isa; ax_obs_st *st; } ax_obs;

static cx_class_t obs_c, obs_m;
static ax_obs *g_obs[8];
static int g_nobs;

static int gate(void) {
    return cx_tcc_preflight("kTCCServiceAccessibility") ? AX_OK : AX_OFF;
}

static void boot(void) {
    if (obs_c.vtable) return;
    cx_define_class(&obs_c, &obs_m, "AXObserver", cx_class_named("NSObject"), 8);
}

static id note_obj(const char *name) {
    static id cache[8];
    static const char *keys[8];
    static int n;
    for (int i = 0; i < n; i++) if (!strcmp(keys[i], name)) return cache[i];
    if (n >= 8) return cx_str(name);
    keys[n] = name;
    cache[n] = cx_str(name);
    return cache[n++];
}

static void obs_perform(void *info) {
    ax_obs_st *st = info;
    if (!st) return;
    while (st->qh != st->qt) {
        id elem = st->q[st->qh].elem;
        void *ref = st->q[st->qh].refcon;
        id note = note_obj(st->q[st->qh].note);
        st->qh = (st->qh + 1) % MAX_Q;
        if (!st->cb) continue;
        if (st->with_info) ((ax_cb_info)st->cb)(st->self, elem, note, 0, ref);
        else ((ax_cb)st->cb)(st->self, elem, note, ref);
    }
}

static id make_source(ax_obs_st *st) {
    ax_src_ctx ctx;
    memset(&ctx, 0, sizeof ctx);
    ctx.version = 0;
    ctx.info = st;
    ctx.perform = obs_perform;
    return (id)CFRunLoopSourceCreate(0, 0, &ctx);
}

static int make_obs(int pid, void *cb, int with_info, id *out) {
    int g = gate();
    ax_obs *o;
    if (g) return g;
    if (!cb || !out) return AX_BADARG;
    boot();
    o = (ax_obs *)cx_alloc_instance(&obs_c);
    if (!o) return -25200;
    o->st = calloc(1, sizeof *o->st);
    if (!o->st) return -25200;
    o->st->self = (id)o;
    o->st->pid = pid > 0 ? pid : getpid();
    o->st->cb = cb;
    o->st->with_info = with_info;
    o->st->source = make_source(o->st);
    if (g_nobs < 8) g_obs[g_nobs++] = o;
    *out = (id)o;
    return AX_OK;
}

int AXObserverCreate(int pid, void *cb, id *out) { return make_obs(pid, cb, 0, out); }
int AXObserverCreateWithInfoCallback(int pid, void *cb, id *out) { return make_obs(pid, cb, 1, out); }

static ax_obs_st *st_of(id obs) {
    boot();
    if (!obs || obs->isa != &obs_c) return 0;
    return ((ax_obs *)obs)->st;
}

int AXObserverAddNotification(id obs, id elem, id note, void *refcon) {
    ax_obs_st *st = st_of(obs);
    const char *name;
    int g = gate();
    if (g) return g;
    if (!st || !elem || !note) return AX_BADARG;
    name = cx_cstr(note);
    for (int i = 0; i < MAX_REG; i++) {
        if (st->reg[i].used && st->reg[i].elem == elem && !strcmp(st->reg[i].note, name)) {
            st->reg[i].refcon = refcon;
            return AX_OK;
        }
    }
    for (int i = 0; i < MAX_REG; i++) {
        if (st->reg[i].used) continue;
        st->reg[i].used = 1;
        st->reg[i].elem = objc_retain(elem);
        st->reg[i].refcon = refcon;
        snprintf(st->reg[i].note, sizeof st->reg[i].note, "%s", name);
        if (st->pid && st->pid != getpid()) {
            const cx_ax_transport *t = cx_ax_transport_get();
            ax_msg m;
            unsigned char buf[256], rep[256];
            int kind = 0, idn = 0, n, rn = 0;
            if (t && t->call && cx_ax_kind_id(elem, &kind, &idn)) {
                memset(&m, 0, sizeof m);
                m.op = AX_OP_SUB;
                m.pid = st->pid;
                m.kind = kind ? kind : 1;
                m.id = idn;
                snprintf(m.name, sizeof m.name, "%s", name);
                n = ax_msg_encode(&m, buf, (int)sizeof buf);
                if (n > 0) t->call(st->pid, buf, n, rep, (int)sizeof rep, &rn);
            }
        }
        return AX_OK;
    }
    return AX_NOATTR;
}

int AXObserverRemoveNotification(id obs, id elem, id note) {
    ax_obs_st *st = st_of(obs);
    const char *name;
    int g = gate();
    if (g) return g;
    if (!st || !elem || !note) return AX_BADARG;
    name = cx_cstr(note);
    for (int i = 0; i < MAX_REG; i++) {
        if (!st->reg[i].used || st->reg[i].elem != elem || strcmp(st->reg[i].note, name)) continue;
        st->reg[i].used = 0;
        return AX_OK;
    }
    return AX_NOATTR;
}

id AXObserverGetRunLoopSource(id obs) {
    ax_obs_st *st = st_of(obs);
    return st ? st->source : 0;
}

static void enqueue(ax_obs_st *st, id elem, const char *note, void *refcon) {
    int next = (st->qt + 1) % MAX_Q;
    if (next == st->qh) st->qh = (st->qh + 1) % MAX_Q;
    snprintf(st->q[st->qt].note, sizeof st->q[st->qt].note, "%s", note);
    st->q[st->qt].elem = elem;
    st->q[st->qt].refcon = refcon;
    st->qt = next;
    if (st->source) {
        CFRunLoopSourceSignal(st->source);
        CFRunLoopWakeUp(CFRunLoopGetCurrent());
    }
}

void cx_ax_note_view(id obj, const char *note) {
    int kind = 0, idn = 0;
    if (!obj || !note) return;
    for (int i = 0; i < g_nobs; i++) {
        ax_obs_st *st = g_obs[i] ? g_obs[i]->st : 0;
        if (!st || (st->pid && st->pid != getpid())) continue;
        for (int r = 0; r < MAX_REG; r++) {
            if (!st->reg[r].used || strcmp(st->reg[r].note, note)) continue;
            if (!cx_ax_matches(st->reg[r].elem, obj)) continue;
            enqueue(st, st->reg[r].elem, note, st->reg[r].refcon);
        }
    }
    if (cx_ax_kind_id(obj, &kind, &idn)) cx_ax_server_note(kind, idn, note);
}

/* entrega uma notificação que chegou pelo transporte, no observador deste processo */
void cx_ax_observer_deliver(int pid, int kind, int id, const char *note) {
    (void)kind; (void)id;
    for (int i = 0; i < g_nobs; i++) {
        ax_obs_st *st = g_obs[i] ? g_obs[i]->st : 0;
        if (!st || st->pid != pid) continue;
        for (int r = 0; r < MAX_REG; r++) {
            if (!st->reg[r].used || strcmp(st->reg[r].note, note ? note : "")) continue;
            enqueue(st, st->reg[r].elem, note, st->reg[r].refcon);
        }
    }
}

