/*
 * mOS libSystem — API cliente do launchd (liblaunch): launch_data_* completo, launch_msg ("CheckIn", "GetJob" do
 * próprio job), launch_activate_socket e vproc_* (transações, standby e chaves de consulta). O launchd do mOS é o
 * userland/init.c (docs/servicos.md); ele passa ao job XPC_SERVICE_NAME = Label e MOS_LAUNCHD_MACHSERVICES =
 * os nomes de MachServices separados por ':'. docs/darwin-bsd-mach.md, fase F9.
 */
#include "sys.h"

void *calloc(size_t n, size_t s);
void *realloc(void *p, size_t n);
void free(void *p);
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
char *strdup(const char *s);
void *memcpy(void *d, const void *s, size_t n);
char *getenv(const char *n);
int close(int fd);
int dup(int fd);
int bootstrap_check_in(u32 bp, const char *name, u32 *port);
extern u32 bootstrap_port;

enum { LD_DICT = 1, LD_ARRAY, LD_FD, LD_INTEGER, LD_REAL, LD_BOOL, LD_STRING, LD_OPAQUE, LD_ERRNO, LD_MACHPORT };
typedef struct ld {
    int type;
    union {
        i64 i; double r; int b; int fd; u32 port; int err;
        struct { char *s; size_t n; } str;
        struct { void *p; size_t n; } op;
        struct { struct ld **v; char **k; size_t n, cap; } c;   /* dicionário (k usado) e array */
    } u;
} ld_t;

void *launch_data_alloc(int type) {
    if (type < LD_DICT || type > LD_MACHPORT) return 0;
    ld_t *d = calloc(1, sizeof *d);
    if (d) d->type = type;
    return d;
}
int launch_data_get_type(const void *d) { return d ? ((const ld_t *)d)->type : 0; }
void launch_data_free(void *v) {
    ld_t *d = v;
    if (!d) return;
    if (d->type == LD_DICT || d->type == LD_ARRAY) {
        for (size_t i = 0; i < d->u.c.n; i++) { launch_data_free(d->u.c.v[i]); if (d->u.c.k) free(d->u.c.k[i]); }
        free(d->u.c.v); free(d->u.c.k);
    } else if (d->type == LD_STRING) free(d->u.str.s);
    else if (d->type == LD_OPAQUE) free(d->u.op.p);
    free(d);
}
static int grow(ld_t *d, int keys) {
    if (d->u.c.n < d->u.c.cap) return 1;
    size_t nc = d->u.c.cap ? d->u.c.cap * 2 : 8;
    void *v = realloc(d->u.c.v, nc * sizeof *d->u.c.v);
    if (!v) return 0;
    d->u.c.v = v;
    if (keys) { void *k = realloc(d->u.c.k, nc * sizeof *d->u.c.k); if (!k) return 0; d->u.c.k = k; }
    d->u.c.cap = nc;
    return 1;
}
/* dicionário: inserir uma chave que já existe troca o valor (e libera o antigo), como no liblaunch */
int launch_data_dict_insert(void *dv, void *val, const char *key) {
    ld_t *d = dv;
    if (!d || d->type != LD_DICT || !val || !key) return 0;
    for (size_t i = 0; i < d->u.c.n; i++)
        if (!strcmp(d->u.c.k[i], key)) { launch_data_free(d->u.c.v[i]); d->u.c.v[i] = val; return 1; }
    if (!grow(d, 1)) return 0;
    d->u.c.k[d->u.c.n] = strdup(key);
    d->u.c.v[d->u.c.n++] = val;
    return 1;
}
void *launch_data_dict_lookup(const void *dv, const char *key) {
    const ld_t *d = dv;
    if (!d || d->type != LD_DICT || !key) return 0;
    for (size_t i = 0; i < d->u.c.n; i++) if (!strcmp(d->u.c.k[i], key)) return d->u.c.v[i];
    return 0;
}
int launch_data_dict_remove(void *dv, const char *key) {
    ld_t *d = dv;
    if (!d || d->type != LD_DICT || !key) return 0;
    for (size_t i = 0; i < d->u.c.n; i++)
        if (!strcmp(d->u.c.k[i], key)) {
            launch_data_free(d->u.c.v[i]); free(d->u.c.k[i]);
            for (size_t j = i + 1; j < d->u.c.n; j++) { d->u.c.v[j - 1] = d->u.c.v[j]; d->u.c.k[j - 1] = d->u.c.k[j]; }
            d->u.c.n--;
            return 1;
        }
    return 0;
}
void launch_data_dict_iterate(const void *dv, void (*fn)(const void *val, const char *key, void *ctx), void *ctx) {
    const ld_t *d = dv;
    if (!d || d->type != LD_DICT || !fn) return;
    for (size_t i = 0; i < d->u.c.n; i++) fn(d->u.c.v[i], d->u.c.k[i], ctx);
}
size_t launch_data_dict_get_count(const void *dv) { const ld_t *d = dv; return d && d->type == LD_DICT ? d->u.c.n : 0; }
int launch_data_array_set_index(void *av, void *val, size_t idx) {
    ld_t *a = av;
    if (!a || a->type != LD_ARRAY || !val) return 0;
    while (a->u.c.n <= idx) {                   /* buraco: completa com nulos, como o liblaunch ao crescer */
        if (!grow(a, 0)) return 0;
        if (a->u.c.n == idx) break;
        a->u.c.v[a->u.c.n++] = 0;
    }
    if (idx < a->u.c.n) { launch_data_free(a->u.c.v[idx]); a->u.c.v[idx] = val; return 1; }
    a->u.c.v[a->u.c.n++] = val;
    return 1;
}
void *launch_data_array_get_index(const void *av, size_t idx) {
    const ld_t *a = av;
    return a && a->type == LD_ARRAY && idx < a->u.c.n ? a->u.c.v[idx] : 0;
}
size_t launch_data_array_get_count(const void *av) { const ld_t *a = av; return a && a->type == LD_ARRAY ? a->u.c.n : 0; }

#define NEW(T) ld_t *d = launch_data_alloc(T); if (!d) return 0
void *launch_data_new_fd(int fd) { NEW(LD_FD); d->u.fd = fd; return d; }
void *launch_data_new_machport(u32 p) { NEW(LD_MACHPORT); d->u.port = p; return d; }
void *launch_data_new_integer(i64 v) { NEW(LD_INTEGER); d->u.i = v; return d; }
void *launch_data_new_bool(int v) { NEW(LD_BOOL); d->u.b = !!v; return d; }
void *launch_data_new_real(double v) { NEW(LD_REAL); d->u.r = v; return d; }
void *launch_data_new_errno(int e) { NEW(LD_ERRNO); d->u.err = e; return d; }
void *launch_data_new_string(const char *s) {
    if (!s) return 0;
    NEW(LD_STRING);
    d->u.str.n = strlen(s);
    d->u.str.s = strdup(s);
    if (!d->u.str.s) { free(d); return 0; }
    return d;
}
void *launch_data_new_opaque(const void *p, size_t n) {
    NEW(LD_OPAQUE);
    d->u.op.p = calloc(1, n ? n : 1);
    if (!d->u.op.p) { free(d); return 0; }
    if (p && n) memcpy(d->u.op.p, p, n);
    d->u.op.n = n;
    return d;
}
#define SETTER(name, T, field, TY) int name(void *v, TY x) { ld_t *d = v; if (!d) return 0; d->type = T; d->u.field = x; return 1; }
SETTER(launch_data_set_fd, LD_FD, fd, int)
SETTER(launch_data_set_machport, LD_MACHPORT, port, u32)
SETTER(launch_data_set_integer, LD_INTEGER, i, i64)
SETTER(launch_data_set_bool, LD_BOOL, b, int)
SETTER(launch_data_set_real, LD_REAL, r, double)
SETTER(launch_data_set_errno, LD_ERRNO, err, int)
int launch_data_set_string(void *v, const char *s) {
    ld_t *d = v;
    if (!d || !s) return 0;
    char *c = strdup(s);
    if (!c) return 0;
    if (d->type == LD_STRING) free(d->u.str.s);
    d->type = LD_STRING; d->u.str.s = c; d->u.str.n = strlen(c);
    return 1;
}
int launch_data_set_opaque(void *v, const void *p, size_t n) {
    ld_t *d = v;
    if (!d) return 0;
    void *c = calloc(1, n ? n : 1);
    if (!c) return 0;
    if (p && n) memcpy(c, p, n);
    if (d->type == LD_OPAQUE) free(d->u.op.p);
    d->type = LD_OPAQUE; d->u.op.p = c; d->u.op.n = n;
    return 1;
}
int launch_data_get_fd(const void *v) { const ld_t *d = v; return d && d->type == LD_FD ? d->u.fd : -1; }
u32 launch_data_get_machport(const void *v) { const ld_t *d = v; return d && d->type == LD_MACHPORT ? d->u.port : 0; }
i64 launch_data_get_integer(const void *v) { const ld_t *d = v; return d && d->type == LD_INTEGER ? d->u.i : 0; }
int launch_data_get_bool(const void *v) { const ld_t *d = v; return d && d->type == LD_BOOL ? d->u.b : 0; }
double launch_data_get_real(const void *v) { const ld_t *d = v; return d && d->type == LD_REAL ? d->u.r : 0; }
const char *launch_data_get_string(const void *v) { const ld_t *d = v; return d && d->type == LD_STRING ? d->u.str.s : 0; }
void *launch_data_get_opaque(const void *v) { const ld_t *d = v; return d && d->type == LD_OPAQUE ? d->u.op.p : 0; }
size_t launch_data_get_opaque_size(const void *v) { const ld_t *d = v; return d && d->type == LD_OPAQUE ? d->u.op.n : 0; }
int launch_data_get_errno(const void *v) { const ld_t *d = v; return d && d->type == LD_ERRNO ? d->u.err : 0; }
void *launch_data_copy(const void *v) {
    const ld_t *s = v;
    if (!s) return 0;
    switch (s->type) {
    case LD_STRING: return launch_data_new_string(s->u.str.s);
    case LD_OPAQUE: return launch_data_new_opaque(s->u.op.p, s->u.op.n);
    case LD_DICT: case LD_ARRAY: {
        ld_t *d = launch_data_alloc(s->type);
        if (!d) return 0;
        for (size_t i = 0; i < s->u.c.n; i++) {
            void *c = s->u.c.v[i] ? launch_data_copy(s->u.c.v[i]) : 0;
            if (s->type == LD_DICT) launch_data_dict_insert(d, c, s->u.c.k[i]);
            else if (c) launch_data_array_set_index(d, c, i);
        }
        return d;
    }
    default: { ld_t *d = launch_data_alloc(s->type); if (d) d->u = s->u; return d; }
    }
}

/* ---- launch_msg: "CheckIn" devolve o dicionário do job (Label, MachServices com as portas recebidas pelo
 * bootstrap_check_in, ProgramArguments vazio não). Fora do launchd (sem XPC_SERVICE_NAME): erro ESRCH, como o macOS
 * responde a quem não é um job. Outros pedidos ("GetJobs", "SubmitJob", "RemoveJob"...) são do launchctl, que fala
 * o protocolo LC_* do mOS: EPERM, como o launchd da Apple responde a um processo comum sem privilégio. */
#define E_SRCH 3
#define E_PERM 1
#define E_INVAL 22
static ld_t *checkin_cache;
void *launch_msg(const void *req) {
    const ld_t *r = req;
    if (!r) { cx_errno = E_INVAL; return 0; }
    const char *what = r->type == LD_STRING ? r->u.str.s : 0;
    if (r->type == LD_DICT && r->u.c.n) what = r->u.c.k[0];   /* {"GetJob": "label"} e parecidos */
    if (!what) { cx_errno = E_INVAL; return 0; }
    const char *label = getenv("XPC_SERVICE_NAME");
    if (!strcmp(what, "CheckIn")) {
        if (!label || !*label) return launch_data_new_errno(E_SRCH);
        if (checkin_cache) return launch_data_copy(checkin_cache);
        ld_t *job = launch_data_alloc(LD_DICT);
        launch_data_dict_insert(job, launch_data_new_string(label), "Label");
        launch_data_dict_insert(job, launch_data_new_integer(30), "TimeOut");
        launch_data_dict_insert(job, launch_data_new_bool(1), "OnDemand");
        const char *ms = getenv("MOS_LAUNCHD_MACHSERVICES");
        if (ms && *ms) {
            ld_t *svcs = launch_data_alloc(LD_DICT);
            char name[256];
            while (*ms) {
                size_t n = 0;
                while (ms[n] && ms[n] != ':') n++;
                if (n && n < sizeof name) {
                    memcpy(name, ms, n); name[n] = 0;
                    u32 port = 0;
                    if (bootstrap_check_in(bootstrap_port, name, &port) == 0 && port)
                        launch_data_dict_insert(svcs, launch_data_new_machport(port), name);
                }
                ms += n;
                if (*ms == ':') ms++;
            }
            launch_data_dict_insert(job, svcs, "MachServices");
        }
        checkin_cache = job;                    /* o segundo CheckIn devolve as mesmas portas */
        return launch_data_copy(job);
    }
    if (!strcmp(what, "GetJob") && r->type == LD_DICT) {
        const ld_t *who = r->u.c.v[0];
        if (label && who && who->type == LD_STRING && !strcmp(who->u.str.s, label)) {
            ld_t *job = launch_data_alloc(LD_DICT);
            launch_data_dict_insert(job, launch_data_new_string(label), "Label");
            launch_data_dict_insert(job, launch_data_new_integer((i64)SYS0(SYS_GETPID)), "PID");
            return job;
        }
        return launch_data_new_errno(E_SRCH);
    }
    return launch_data_new_errno(E_PERM);
}
int launch_get_fd(void) { return -1; }          /* sem soquete de controle: o mOS fala com o launchd por Mach */

/* launch_activate_socket: o launchd do mOS não abre Sockets do plist; sem a chave no job, o macOS também dá ESRCH */
int launch_activate_socket(const char *name, int **fds, size_t *cnt) {
    if (!name || !fds || !cnt) return E_INVAL;
    *fds = 0; *cnt = 0;
    return getenv("XPC_SERVICE_NAME") ? 2 /* ENOENT: o job não tem esse soquete */ : E_SRCH;
}

/* ---- vproc: transações (o job não é encerrado por ociosidade enquanto há uma aberta) e standby */
static volatile int vp_transactions, vp_standby;
typedef struct { int dummy; } vproc_obj_t;
static vproc_obj_t vp_self = {0};
void *vproc_transaction_begin(void *vp) { (void)vp; __sync_fetch_and_add(&vp_transactions, 1); return &vp_self; }
void vproc_transaction_end(void *vp, void *t) { (void)vp; if (t && vp_transactions > 0) __sync_fetch_and_sub(&vp_transactions, 1); }
size_t vproc_transaction_count(void) { return (size_t)vp_transactions; }
void *vproc_standby_begin(void *vp) { (void)vp; __sync_fetch_and_add(&vp_standby, 1); return &vp_self; }
void vproc_standby_end(void *vp, void *t) { (void)vp; if (t && vp_standby > 0) __sync_fetch_and_sub(&vp_standby, 1); }
void *vproc_retain(void *vp) { return vp; }
void vproc_release(void *vp) { (void)vp; }
/* vproc_swap_integer: VPROC_GSK_IS_MANAGED (14) = está sob o launchd; VPROC_GSK_TRANSACTIONS_ENABLED (17); o resto
 * (limites globais do launchd) só root troca: só leitura com o valor de agora */
void *vproc_swap_integer(void *vp, int key, const i64 *in, i64 *out) {
    (void)vp;
    i64 v = 0;
    switch (key) {
    case 14: v = getenv("XPC_SERVICE_NAME") ? 1 : 0; break;
    case 17: v = 1; break;
    case 21: v = vp_transactions; break;
    default: v = 0; break;
    }
    if (out) *out = v;
    if (in && key != 17) return &vp_self;       /* erro (vproc_err_t não nulo): sem privilégio para trocar */
    return 0;
}
void *vproc_swap_complex(void *vp, int key, void *in, void **out) { (void)vp; (void)key; (void)in; if (out) *out = 0; return &vp_self; }
void *vproc_swap_string(void *vp, int key, const char *in, char **out) { (void)vp; (void)key; (void)in; if (out) *out = 0; return &vp_self; }
void *vproc_get_last_exit_status(int *st) { if (st) *st = 0; return &vp_self; }   /* sem histórico de saída exposto */
