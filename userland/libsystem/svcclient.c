/*
 * mOS — libSystem: cliente dos serviços do sistema (launchd, notifyd, logd, configd) e o notify(3).
 *
 * cx_svc_call: pedido no protocolo de userland/svcproto.h para um nome do bootstrap. O direito de envio fica em cache
 * (o launchd guarda a porta do serviço entre reinícios do daemon, então ele continua válido); com resposta, usa uma
 * porta de resposta própria e MACH_SEND_MSG|MACH_RCV_MSG com prazo.
 *
 * notify(3) de verdade, entre processos, sobre o notifyd (userland/notifyd.c):
 *  - notify_post envia NT_POST (com resposta: a ordem entre post e check fica garantida);
 *  - register_check guarda a geração e notify_check compara (a 1ª chamada diz "mudou", como no macOS);
 *  - register_mach_port registra um envio para a porta do app (o aviso chega com msgh_id = token);
 *  - register_dispatch e register_file_descriptor usam uma porta do processo e uma thread que recebe os avisos e
 *    chama o bloco na fila (dispatch_async_f) ou escreve o token (ordem de rede) no pipe;
 *  - register_signal: o notifyd manda o sinal;
 *  - get_state/set_state guardam o estado de 64 bits no notifyd.
 */
#include "foundation.h"
#include "libc.h"
#include "../svcproto.h"

extern unsigned bootstrap_port;
int bootstrap_look_up(unsigned bp, const char *name, unsigned *port);
int mach_port_allocate(unsigned task, int right, unsigned *name);
int mach_port_mod_refs(unsigned task, unsigned name, int right, int delta);
int mach_port_deallocate(unsigned task, unsigned name);
int mach_msg(void *msg, int option, unsigned send_size, unsigned rcv_size, unsigned rcv_name, unsigned timeout, unsigned notify);
int pthread_create(unsigned long *t, const void *attr, void *(*fn)(void *), void *arg);
int sched_yield(void);
int pipe(int *fds);
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *));
void *dispatch_get_global_queue(long priority, unsigned long flags);
void _Block_release(const void *b);

typedef struct { unsigned bits, size, remote, local, voucher, id; } cx_mhdr_t;
#define MS_SEND 1
#define MS_RCV 2
#define MS_SEND_TO 0x10
#define MS_RCV_TO 0x100
#define SEND_INVALID_DEST 0x10000003
#define SEND_TIMED_OUT 0x10000004

/* ---------------------------------------------------------------- chamada genérica */
static struct { const char *name; volatile unsigned port; } svc_cache[8];
static unsigned svc_port(const char *name, int refresh) {
    int i;
    for (i = 0; i < 8 && svc_cache[i].name; i++)
        if (!strcmp(svc_cache[i].name, name)) break;
    if (i < 8 && svc_cache[i].name && svc_cache[i].port && !refresh) return svc_cache[i].port;
    unsigned p = 0;
    if (bootstrap_look_up(bootstrap_port, name, &p) != 0) p = 0;
    if (i < 8) {
        if (svc_cache[i].port && svc_cache[i].port != p) mach_port_deallocate(0x103, svc_cache[i].port);
        svc_cache[i].name = name;
        svc_cache[i].port = p;
    }
    return p;
}

/* rbuf = 0: só envia (prazo 0 no envio: com a fila cheia, descarta). give_port/give_disp: um direito que vai junto
 * (descritor de porta). Resposta: header | svc_body_t | dados, em rbuf. Devolve o kern_return_t (0 = ok) ou -1. */
int cx_svc_call(const char *svc, const svc_body_t *in, const void *s1, unsigned n1, const void *s2, unsigned n2,
                unsigned give_port, int give_disp, void *rbuf, unsigned rcap, unsigned timeout_ms) {
    unsigned hdr = sizeof(cx_mhdr_t) + (give_port ? 16 : 0);
    unsigned size = (hdr + (unsigned)sizeof(svc_body_t) + n1 + n2 + 3u) & ~3u;
    if (size > SVC_MAX_MSG) return -1;
    unsigned cap = (rbuf && rcap > size ? rcap : size) + 128;
    unsigned char *m = calloc(1, cap);
    if (!m) return -1;
    int kr = -1;
    for (int attempt = 0; attempt < 2; attempt++) {
        unsigned port = svc_port(svc, attempt);
        if (!port) break;
        memset(m, 0, size);
        cx_mhdr_t *h = (cx_mhdr_t *)m;
        h->bits = 19 /* COPY_SEND */;
        h->size = size;
        h->remote = port;
        h->id = SVC_MSG_ID;
        if (give_port) {
            h->bits |= 0x80000000u;
            *(unsigned *)(m + 24) = 1;
            *(unsigned *)(m + 28) = give_port;
            m[38] = (unsigned char)give_disp;
            m[39] = 0;
        }
        svc_body_t *b = (svc_body_t *)(m + hdr);
        *b = *in;
        b->n1 = n1;
        b->n2 = n2;
        if (n1) memcpy(b + 1, s1, n1);
        if (n2) memcpy((char *)(b + 1) + n1, s2, n2);
        unsigned reply = 0;
        if (rbuf) {
            if (mach_port_allocate(0x103, 1, &reply) != 0) break;
            h->bits |= 21u << 8;        /* MAKE_SEND_ONCE */
            h->local = reply;
            kr = mach_msg(m, MS_SEND | MS_RCV | MS_RCV_TO | MS_SEND_TO, size, cap, reply, timeout_ms, 0);   /* a resposta chega em m */
            if (kr == 0x10004004) kr = -1;
            if (kr == 0) memcpy(rbuf, m, ((cx_mhdr_t *)m)->size < rcap ? ((cx_mhdr_t *)m)->size : rcap);
            mach_port_mod_refs(0x103, reply, 1, -1);
        } else {
            kr = mach_msg(m, MS_SEND | MS_SEND_TO, size, 0, 0, 0, 0);
        }
        if (kr != SEND_INVALID_DEST) break;
    }
    free(m);
    return kr;
}

/* a resposta de cx_svc_call: corpo e dados */
static svc_body_t *reply_body(void *rbuf) { return (svc_body_t *)((char *)rbuf + sizeof(cx_mhdr_t)); }
const char *cx_svc_reply_data(void *rbuf, unsigned *n) { svc_body_t *b = reply_body(rbuf); if (n) *n = b->n1; return (const char *)(b + 1); }
unsigned cx_svc_reply_code(void *rbuf) { return reply_body(rbuf)->op; }

/* ---------------------------------------------------------------- launchd */
/* pede ao launchd um serviço sob demanda (XPC dentro de bundles, SMAppService). 0 = ok */
int cx_launchd_submit(const char *label, const char *program) {
    svc_body_t q = { LC_SUBMIT, 0, 0, 0, 0, 0, 0 };
    unsigned char r[512];
    int kr = cx_svc_call(LAUNCHD_SERVICE, &q, label, (unsigned)strlen(label) + 1, program, (unsigned)strlen(program) + 1, 0, 0, r, sizeof r, 5000);
    return kr ? -1 : (int)cx_svc_reply_code(r);
}
int cx_launchd_cmd(unsigned op, const char *arg, char *out, unsigned cap) {
    svc_body_t q = { op, 0, 0, 0, 0, 0, 0 };
    unsigned char *r = malloc(SVC_MAX_MSG);
    if (!r) return -1;
    int kr = cx_svc_call(LAUNCHD_SERVICE, &q, arg, arg ? (unsigned)strlen(arg) + 1 : 0, 0, 0, 0, 0, r, SVC_MAX_MSG, 10000);
    int code = kr ? -1 : (int)cx_svc_reply_code(r);
    if (out && cap) {
        out[0] = 0;
        if (!kr) {
            unsigned n;
            const char *d = cx_svc_reply_data(r, &n);
            if (n >= cap) n = cap - 1;
            memcpy(out, d, n);
            out[n] = 0;
        }
    }
    free(r);
    return code;
}

/* ---------------------------------------------------------------- notify(3) */
#define NOTIFY_STATUS_OK 0
#define NOTIFY_STATUS_INVALID_NAME 1
#define NOTIFY_STATUS_INVALID_TOKEN 2
#define NOTIFY_STATUS_FAILED 1000000
#define NOTIFY_REUSE 1
enum { NK_CHECK = 1, NK_PORT, NK_DISPATCH, NK_FD, NK_SIGNAL };
typedef struct { int used, kind, fd; char *name; unsigned seen, port; void *queue, *block; } ntok_t;
#define NTOK 256
static ntok_t ntok[NTOK];
static volatile int nlk;
static unsigned nport;                 /* porta do processo para dispatch/fd */
static void nlock(void) { while (__sync_lock_test_and_set(&nlk, 1)) sched_yield(); }
static void nunlock(void) { __sync_lock_release(&nlk); }
static ntok_t *ntok_get(int t) { return t > 0 && t < NTOK + 1 && ntok[t - 1].used ? &ntok[t - 1] : 0; }
static int ntok_new(const char *name, int kind) {
    nlock();
    int t = 0;
    for (int i = 0; i < NTOK; i++) if (!ntok[i].used) { memset(&ntok[i], 0, sizeof ntok[i]); ntok[i].used = 1; t = i + 1; break; }
    if (t) { ntok[t - 1].kind = kind; ntok[t - 1].name = strdup(name); ntok[t - 1].fd = -1; ntok[t - 1].seen = ~0u; }
    nunlock();
    return t;
}
static unsigned nt_call(unsigned op, const char *name, unsigned a, unsigned b, unsigned long long q, unsigned give, int disp,
                        unsigned *ra, unsigned long long *rq) {
    svc_body_t in = { op, a, b, 0, q, 0, 0 };
    unsigned char r[256];
    int kr = cx_svc_call(NOTIFYD_SERVICE, &in, name, name ? (unsigned)strlen(name) + 1 : 0, 0, 0, give, disp, r, sizeof r, 3000);
    if (kr) return NOTIFY_STATUS_FAILED;
    svc_body_t *rb = reply_body(r);
    if (ra) *ra = rb->a;
    if (rq) *rq = rb->q;
    return rb->op;
}

typedef struct { void *isa; int flags, reserved; void *invoke; } nblk_t;
typedef struct { void *block; int token; } ncall_t;
static void ncall_fn(void *p) {
    ncall_t *c = p;
    ((void (*)(void *, int))((nblk_t *)c->block)->invoke)(c->block, c->token);
    _Block_release(c->block);
    free(c);
}
static void *nlisten(void *arg) {
    (void)arg;
    unsigned char buf[1024];
    for (;;) {
        memset(buf, 0, sizeof(cx_mhdr_t));
        int kr = mach_msg(buf, MS_RCV, 0, sizeof buf, nport, 0, 0);
        if (kr) { if (kr == 0x10004004) continue; usleep(10000); continue; }
        int token = (int)((cx_mhdr_t *)buf)->id;
        nlock();
        ntok_t *t = ntok_get(token);
        void *blk = t && t->kind == NK_DISPATCH && t->block ? _Block_copy(t->block) : 0;
        void *q = t ? t->queue : 0;
        int fd = t && t->kind == NK_FD ? t->fd : -1;
        nunlock();
        if (blk) {
            ncall_t *c = malloc(sizeof *c);
            if (c) { c->block = blk; c->token = token; dispatch_async_f(q ? q : dispatch_get_global_queue(0, 0), c, ncall_fn); }
            else _Block_release(blk);
        }
        if (fd >= 0) {
            unsigned be = ((unsigned)token >> 24) | (((unsigned)token >> 8) & 0xff00) | (((unsigned)token << 8) & 0xff0000) | ((unsigned)token << 24);
            write(fd, &be, 4);
        }
    }
    return 0;
}
static unsigned nproc_port(void) {
    nlock();
    if (!nport) {
        unsigned p = 0;
        if (mach_port_allocate(0x103, 1, &p) == 0) {
            nport = p;
            unsigned long tid;
            pthread_create(&tid, 0, nlisten, 0);
        }
    }
    unsigned p = nport;
    nunlock();
    return p;
}

u32 notify_post(const char *name) {
    if (!name) return NOTIFY_STATUS_INVALID_NAME;
    return nt_call(NT_POST, name, 0, 0, 0, 0, 0, 0, 0);
}
u32 notify_register_check(const char *name, int *token) {
    if (!name || !token) return NOTIFY_STATUS_INVALID_NAME;
    int t = ntok_new(name, NK_CHECK);
    if (!t) return NOTIFY_STATUS_FAILED;
    *token = t;
    return NOTIFY_STATUS_OK;
}
u32 notify_check(int token, int *changed) {
    ntok_t *t = ntok_get(token);
    if (!t) return NOTIFY_STATUS_INVALID_TOKEN;
    unsigned gen = 0;
    u32 r = nt_call(NT_GENERATION, t->name, 0, 0, 0, 0, 0, &gen, 0);
    if (r) { if (changed) *changed = 0; return r; }
    if (changed) *changed = gen != t->seen;
    t->seen = gen;
    return NOTIFY_STATUS_OK;
}
static u32 nreg_port(const char *name, int kind, unsigned port, int *token) {
    int t = ntok_new(name, kind);
    if (!t) return NOTIFY_STATUS_FAILED;
    u32 r = nt_call(NT_REGISTER_PORT, name, (unsigned)t, 0, 0, port, 20 /* MAKE_SEND */, 0, 0);
    if (r) { nlock(); free(ntok[t - 1].name); ntok[t - 1].used = 0; nunlock(); return r; }
    ntok[t - 1].port = port;
    *token = t;
    return NOTIFY_STATUS_OK;
}
u32 notify_register_mach_port(const char *name, unsigned *port, int flags, int *token) {
    if (!name || !port || !token) return NOTIFY_STATUS_INVALID_NAME;
    if (!(flags & NOTIFY_REUSE) || !*port) {
        unsigned p = 0;
        if (mach_port_allocate(0x103, 1, &p) != 0) return NOTIFY_STATUS_FAILED;
        *port = p;
    }
    return nreg_port(name, NK_PORT, *port, token);
}
u32 notify_register_dispatch(const char *name, int *token, void *queue, void *handler) {
    if (!name || !token) return NOTIFY_STATUS_INVALID_NAME;
    unsigned p = nproc_port();
    if (!p) return NOTIFY_STATUS_FAILED;
    u32 r = nreg_port(name, NK_DISPATCH, p, token);
    if (!r) { ntok[*token - 1].queue = queue; ntok[*token - 1].block = handler ? _Block_copy(handler) : 0; }
    return r;
}
u32 notify_register_file_descriptor(const char *name, int *fd, int flags, int *token) {
    if (!name || !fd || !token) return NOTIFY_STATUS_INVALID_NAME;
    int fds[2];
    int wfd = -1;
    if ((flags & NOTIFY_REUSE) && *fd >= 0) {
        for (int i = 0; i < NTOK; i++) if (ntok[i].used && ntok[i].kind == NK_FD && ntok[i].fd >= 0) { wfd = ntok[i].fd; break; }   /* mesmo pipe */
        if (wfd < 0) return NOTIFY_STATUS_FAILED;
    } else {
        if (pipe(fds) != 0) return NOTIFY_STATUS_FAILED;
        *fd = fds[0];
        wfd = fds[1];
    }
    unsigned p = nproc_port();
    if (!p) return NOTIFY_STATUS_FAILED;
    u32 r = nreg_port(name, NK_FD, p, token);
    if (!r) ntok[*token - 1].fd = wfd;
    return r;
}
u32 notify_register_signal(const char *name, int sig, int *token) {
    if (!name || !token) return NOTIFY_STATUS_INVALID_NAME;
    int t = ntok_new(name, NK_SIGNAL);
    if (!t) return NOTIFY_STATUS_FAILED;
    u32 r = nt_call(NT_REGISTER_SIGNAL, name, (unsigned)t, (unsigned)sig, 0, 0, 0, 0, 0);
    if (r) { nlock(); free(ntok[t - 1].name); ntok[t - 1].used = 0; nunlock(); return r; }
    *token = t;
    return NOTIFY_STATUS_OK;
}
u32 notify_cancel(int token) {
    ntok_t *t = ntok_get(token);
    if (!t) return NOTIFY_STATUS_INVALID_TOKEN;
    if (t->kind != NK_CHECK) nt_call(NT_CANCEL, t->name, (unsigned)token, 0, 0, 0, 0, 0, 0);
    nlock();
    if (t->block) _Block_release(t->block);
    free(t->name);
    t->used = 0;
    nunlock();
    return NOTIFY_STATUS_OK;
}
u32 notify_get_state(int token, unsigned long long *state) {
    ntok_t *t = ntok_get(token);
    if (!t) return NOTIFY_STATUS_INVALID_TOKEN;
    unsigned long long q = 0;
    u32 r = nt_call(NT_GET_STATE, t->name, 0, 0, 0, 0, 0, 0, &q);
    if (state) *state = r ? 0 : q;
    return r;
}
u32 notify_set_state(int token, unsigned long long state) {
    ntok_t *t = ntok_get(token);
    if (!t) return NOTIFY_STATUS_INVALID_TOKEN;
    return nt_call(NT_SET_STATE, t->name, 0, 0, state, 0, 0, 0, 0);
}
int notify_is_valid_token(int token) { return ntok_get(token) != 0; }
u32 notify_suspend(int token) { return ntok_get(token) ? NOTIFY_STATUS_OK : NOTIFY_STATUS_INVALID_TOKEN; }
u32 notify_resume(int token) { return ntok_get(token) ? NOTIFY_STATUS_OK : NOTIFY_STATUS_INVALID_TOKEN; }
