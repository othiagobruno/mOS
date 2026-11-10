/* Servidor da árvore AX no processo que tem as janelas.
 * cx_ax_server_handle traduz um ax_msg na árvore local e devolve a resposta.
 * O listen só é ligado quando o transporte de SYS-XPC-MACH / KER-UNIX-IPC existe.
 * Sem esse vínculo o servidor continua utilizável por quem chamar handle direto. */
#include "foundation.h"
#include "axproto.h"
#include "axtransport.h"
#include "xpctransport.h"

int cx_ax_copy(id elem, id attr, id *v);
int cx_ax_set(id elem, id attr, id value);
int cx_ax_perform(id elem, id action);
int cx_ax_param(id elem, id attr, id param, id *v);
int cx_ax_actions(id elem, id *names);
id cx_ax_find(int kind, int id);
int cx_ax_kind_id(id obj, int *kind, int *id);
id AXValueCreate(unsigned type, const void *p);
unsigned AXValueGetType(id v);
unsigned char AXValueGetValue(id v, unsigned type, void *out);
id objc_retain(id o);
void objc_release(id o);
int strcmp(const char *a, const char *b);
int snprintf(char *b, size_t n, const char *fmt, ...);
void *memset(void *d, int c, size_t n);

enum { AX_OK = 0, AX_CANT = -25204, AX_NOATTR = -25205 };

typedef struct {
    int used, pid, kind, id;
    char note[AX_NAME_MAX];
} ax_sub;

static ax_sub g_sub[32];
static int g_listening;

static int name_eq(const char *a, const char *b) {
    if (!a) a = "";
    if (!b) b = "";
    return strcmp(a, b) == 0;
}

static void reply_status(ax_msg *m, int st) {
    m->op = AX_OP_REPLY;
    m->status = st;
    m->pay = AX_PAY_NONE;
}

static int send_reply(int to, const ax_msg *m) {
    unsigned char buf[640];
    int n = ax_msg_encode(m, buf, (int)sizeof buf);
    const cx_ax_transport *t = cx_ax_transport_get();
    if (n < 0 || !t || !t->send) return -1;
    return t->send(to, buf, n);
}

static id elem_of(const ax_msg *req) {
    return cx_ax_find(req->kind, req->id);
}

static void fill_value(ax_msg *rep, id v) {
    if (!v) { reply_status(rep, AX_NOATTR); return; }
    if (cx_is_str(v)) {
        const char *s = cx_cstr(v);
        int i = 0;
        rep->pay = AX_PAY_STR;
        for (; s[i] && i < AX_STR_MAX - 1; i++) rep->str[i] = s[i];
        rep->str[i] = 0;
        rep->status = AX_OK;
        return;
    }
    unsigned ty = AXValueGetType(v);
    if (ty == 1 || ty == 2) {
        double p[2] = {0, 0};
        AXValueGetValue(v, ty, p);
        rep->pay = AX_PAY_BOX;
        if (ty == 1) { rep->x = (int)p[0]; rep->y = (int)p[1]; }
        else { rep->w = (int)p[0]; rep->h = (int)p[1]; }
        rep->status = AX_OK;
        return;
    }
    if (ty == 3) {
        double r[4] = {0, 0, 0, 0};
        AXValueGetValue(v, 3, r);
        rep->pay = AX_PAY_BOX;
        rep->x = (int)r[0]; rep->y = (int)r[1]; rep->w = (int)r[2]; rep->h = (int)r[3];
        rep->status = AX_OK;
        return;
    }
    if (ty == 4) {
        long rg[2] = {0, 0};
        AXValueGetValue(v, 4, rg);
        rep->pay = AX_PAY_RANGE;
        rep->loc = (int)rg[0]; rep->len = (int)rg[1];
        rep->status = AX_OK;
        return;
    }
    if (MSG(BOOL, v, "respondsToSelector:", sel_registerName("count")) &&
        MSG(BOOL, v, "respondsToSelector:", sel_registerName("objectAtIndex:"))) {
        unsigned long n = MSG0(unsigned long, v, "count");
        rep->pay = AX_PAY_KIDS;
        rep->nkids = 0;
        for (unsigned long i = 0; i < n && rep->nkids < AX_KIDS_MAX; i++) {
            id kid = MSG(id, v, "objectAtIndex:", i);
            int k = 0, idn = 0;
            if (cx_ax_kind_id(kid, &k, &idn)) {
                rep->kid_kind[rep->nkids] = k;
                rep->kid_id[rep->nkids] = idn;
                rep->nkids++;
            }
        }
        rep->status = AX_OK;
        return;
    }
    if (MSG(BOOL, v, "respondsToSelector:", sel_registerName("intValue"))) {
        rep->pay = AX_PAY_I32;
        rep->i = (int)MSG0(long, v, "intValue");
        rep->status = AX_OK;
        return;
    }
    reply_status(rep, AX_NOATTR);
}

int cx_ax_server_handle(int from_pid, const void *req, int n, void *reply, int cap) {
    ax_msg in, out;
    id elem, val = 0, attr = 0;
    int st;
    (void)from_pid;
    memset(&out, 0, sizeof out);
    if (ax_msg_decode(req, n, &in) != 0) {
        reply_status(&out, -25201);
        return ax_msg_encode(&out, reply, cap);
    }
    out.op = AX_OP_REPLY;
    out.pid = in.pid;
    out.kind = in.kind;
    out.id = in.id;
    if (in.op == AX_OP_SUB) {
        int slot = -1;
        for (int i = 0; i < 32; i++) if (!g_sub[i].used || (g_sub[i].pid == from_pid && g_sub[i].kind == in.kind && g_sub[i].id == in.id && name_eq(g_sub[i].note, in.name))) { slot = i; break; }
        if (slot >= 0) {
            g_sub[slot].used = 1;
            g_sub[slot].pid = from_pid;
            g_sub[slot].kind = in.kind;
            g_sub[slot].id = in.id;
            snprintf(g_sub[slot].note, sizeof g_sub[slot].note, "%s", in.name);
        }
        reply_status(&out, slot >= 0 ? AX_OK : AX_CANT);
        return ax_msg_encode(&out, reply, cap);
    }
    elem = elem_of(&in);
    if (!elem && in.op != AX_OP_NOTE) {
        reply_status(&out, -25202);
        return ax_msg_encode(&out, reply, cap);
    }
    attr = cx_str(in.name);
    if (in.op == AX_OP_COPY && !strcmp(in.name, "AXActionNames")) {
        id names = 0;
        int at = 0;
        unsigned long kn, ki;
        st = cx_ax_actions(elem, &names);
        out.op = AX_OP_REPLY;
        out.str[0] = 0;
        if (st) reply_status(&out, st);
        else {
            kn = names ? MSG0(unsigned long, names, "count") : 0;
            for (ki = 0; ki < kn && at < AX_STR_MAX - 1; ki++) {
                id s = MSG(id, names, "objectAtIndex:", ki);
                const char *c = cx_cstr(s);
                int k;
                if (ki && at < AX_STR_MAX - 1) out.str[at++] = ',';
                for (k = 0; c && c[k] && at < AX_STR_MAX - 1; k++) out.str[at++] = c[k];
            }
            out.str[at] = 0;
            out.pay = AX_PAY_STR;
            out.status = AX_OK;
        }
        if (names) objc_release(names);
        if (attr) objc_release(attr);
        return ax_msg_encode(&out, reply, cap);
    }
    if (in.op == AX_OP_COPY) {
        st = cx_ax_copy(elem, attr, &val);
        if (st) reply_status(&out, st);
        else fill_value(&out, val);
    } else if (in.op == AX_OP_PARAM) {
        id param = 0;
        if (!strcmp(in.name, "AXRangeForPosition")) {
            double pt[2] = {in.px, in.py};
            param = AXValueCreate(1, pt);
        } else {
            long rg[2] = {in.loc, in.len};
            param = AXValueCreate(4, rg);
        }
        st = cx_ax_param(elem, attr, param, &val);
        if (param) objc_release(param);
        if (st) reply_status(&out, st);
        else fill_value(&out, val);
    } else if (in.op == AX_OP_SET) {
        id s = cx_str(in.str);
        st = cx_ax_set(elem, attr, s);
        if (s) objc_release(s);
        reply_status(&out, st);
    } else if (in.op == AX_OP_PRESS) {
        st = cx_ax_perform(elem, attr);
        reply_status(&out, st);
    } else if (in.op == AX_OP_NOTE) {
        reply_status(&out, AX_OK);
    } else {
        reply_status(&out, -25201);
    }
    if (val) objc_release(val);
    if (attr) objc_release(attr);
    return ax_msg_encode(&out, reply, cap);
}

void cx_ax_server_note(int kind, int id, const char *note) {
    const cx_ax_transport *t = cx_ax_transport_get();
    if (!t || !t->send || !cx_ax_transport_ready()) return;
    for (int i = 0; i < 32; i++) {
        ax_msg m;
        if (!g_sub[i].used || !name_eq(g_sub[i].note, note)) continue;
        if (!(g_sub[i].kind == 1 || (g_sub[i].kind == kind && g_sub[i].id == id))) continue;
        memset(&m, 0, sizeof m);
        m.op = AX_OP_NOTE;
        m.kind = kind;
        m.id = id;
        m.pid = g_sub[i].pid;
        m.pay = AX_PAY_NONE;
        snprintf(m.name, sizeof m.name, "%s", note ? note : "");
        send_reply(g_sub[i].pid, &m);
    }
}

static void on_transport(int from, const void *bytes, int n, void *ctx) {
    unsigned char reply[640];
    int rn;
    (void)ctx;
    rn = cx_ax_server_handle(from, bytes, n, reply, (int)sizeof reply);
    if (rn > 0) {
        const cx_ax_transport *t = cx_ax_transport_get();
        if (t && t->send) t->send(from, reply, rn);
    }
}

static void arm_listen(void) {
    const cx_ax_transport *t = cx_ax_transport_get();
    if (g_listening || !t || !t->listen) return;
    if (t->listen(on_transport, 0) == 0) g_listening = 1;
}

static cx_ax_transport g_xpc_ax = {
    "xpc-mach",
    cx_xpc_peer_ready,
    cx_xpc_peer_call,
    cx_xpc_peer_send,
    cx_xpc_peer_listen,
};
static int g_bound;

void cx_ax_bind_xpc(void) {
    if (!g_bound) {
        g_bound = 1;
        cx_ax_transport_bind(&g_xpc_ax);
    }
    arm_listen();
}

void cx_ax_server_start(void) {
    cx_ax_transport_on_bind(arm_listen);
    cx_ax_bind_xpc();
}
