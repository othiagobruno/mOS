/* mOS — portas Mach para os programas nativos (init/launchd, notifyd, logd, configd, launchctl nativo).
 * Usa as syscalls 80..82 direto (sem libSystem). Inclua depois de mos.h. */
#pragma once
#include "svcproto.h"

#define M_SEND        1u
#define M_RCV         2u
#define M_SEND_TO     0x10u
#define M_RCV_TO      0x100u
#define M_TRAILER     (3u << 24)        /* MACH_RCV_TRAILER_ELEMENTS(3): trailer com o audit token */
#define M_COMPLEX     0x80000000u
#define D_MOVE_RECV   16
#define D_MOVE_SEND   17
#define D_MOVE_SO     18
#define D_COPY_SEND   19
#define D_MAKE_SEND   20
#define D_MAKE_SO     21
#define RCV_TIMED_OUT 0x10004003
#define SEND_INVALID_DEST 0x10000003

typedef struct { unsigned bits, size, remote, local, voucher, id; } mhdr_t;

static inline i64 mach_msg_n(void *msg, unsigned opt, unsigned ssz, unsigned rsz, unsigned rname, unsigned to) {
    mos_mach_msg_args_t a = { (u64)msg, opt, ssz, rsz, rname, to, 0 };
    i64 r;
    r = sys3(SYS_MACH_MSG, (u64)&a, 0, 0);
    while (r == -4 && (opt & M_RCV)) { a.option = opt & ~M_SEND; a.send_size = 0; r = sys3(SYS_MACH_MSG, (u64)&a, 0, 0); }   /* sinal: retoma a recepção */
    return r;
}
static inline i64 mach_port_op(unsigned op, u64 *a) { return sys3(SYS_MACH_PORT, op, (u64)a, 0); }
static inline unsigned port_new_recv(void) { u64 a[8] = { 1, 0, 0 }; return mach_port_op(MACH_OP_ALLOCATE, a) == 0 ? (unsigned)a[2] : 0; }
static inline unsigned port_new_set(void) { u64 a[8] = { 3, 0, 0 }; return mach_port_op(MACH_OP_ALLOCATE, a) == 0 ? (unsigned)a[2] : 0; }
static inline void port_drop(unsigned name) { u64 a[8] = { name }; mach_port_op(MACH_OP_DEALLOCATE, a); }
static inline void port_destroy(unsigned name) { u64 a[8] = { name, 1 /* RECEIVE */, (u64)-1 }; mach_port_op(MACH_OP_MOD_REFS, a); }
static inline void port_join(unsigned port, unsigned set) { u64 a[8] = { port, set }; mach_port_op(MACH_OP_INSERT_MEMBER, a); }
static inline unsigned port_pending(unsigned name) { u64 a[8] = { name }; return mach_port_op(MACH_OP_PENDING, a) == 0 ? (unsigned)a[1] : 0; }
static inline void port_wait(unsigned name, u64 ms) { u64 a[8] = { name, ms }; mach_port_op(MACH_OP_WAIT, a); }
static inline i64 bs_op(unsigned op, const char *name, unsigned *port) { return sys3(SYS_MACH_BOOTSTRAP, op, (u64)name, (u64)port); }

/* audit token no trailer: val[5] = pid de quem enviou */
static inline int msg_sender_pid(const unsigned char *msg) {
    unsigned body = (((const mhdr_t *)msg)->size + 3u) & ~3u;
    return *(const int *)(msg + body + 40);
}

/* corpo de uma mensagem do protocolo (pula o descritor das complexas); NULL se não for do protocolo */
static inline svc_body_t *svc_body(unsigned char *msg, unsigned len, unsigned *port_out) {
    mhdr_t *h = (mhdr_t *)msg;
    unsigned off = sizeof(mhdr_t);
    if (port_out) *port_out = 0;
    if (h->bits & M_COMPLEX) {
        unsigned n = *(unsigned *)(msg + off);
        if (n != 1) return 0;
        if (port_out) *port_out = *(unsigned *)(msg + off + 4);
        off += 4 + 12;
    }
    if (off + sizeof(svc_body_t) > len || off + sizeof(svc_body_t) > h->size) return 0;
    svc_body_t *b = (svc_body_t *)(msg + off);
    if ((u64)off + sizeof *b + b->n1 + b->n2 > h->size) return 0;
    return b;
}
static inline char *svc_s1(svc_body_t *b) { return (char *)(b + 1); }
static inline char *svc_s2(svc_body_t *b) { return (char *)(b + 1) + b->n1; }

/* monta header|corpo|s1|s2 em buf (cap bytes); devolve o tamanho (0 se não cabe) */
static inline unsigned svc_pack(unsigned char *buf, unsigned cap, unsigned remote, unsigned rbits, unsigned id,
                                const svc_body_t *in, const void *s1, unsigned n1, const void *s2, unsigned n2) {
    unsigned size = (unsigned)(sizeof(mhdr_t) + sizeof(svc_body_t) + n1 + n2);
    size = (size + 3u) & ~3u;
    if (size > cap) return 0;
    memset(buf, 0, size);
    mhdr_t *h = (mhdr_t *)buf;
    h->bits = rbits;
    h->size = size;
    h->remote = remote;
    h->id = id;
    svc_body_t *b = (svc_body_t *)(h + 1);
    *b = *in;
    b->n1 = n1;
    b->n2 = n2;
    if (n1) memcpy(b + 1, s1, n1);
    if (n2) memcpy((char *)(b + 1) + n1, s2, n2);
    return size;
}

/* responde ao pedido (porta de resposta send-once que veio no local do header); sem esperar */
static inline void svc_reply(unsigned reply_port, unsigned code, unsigned a, u64 q, const void *data, unsigned n) {
    static unsigned char rb[SVC_MAX_MSG];
    if (!reply_port) return;
    if (n > SVC_MAX_MSG - 128) n = SVC_MAX_MSG - 128;
    svc_body_t b = { code, a, 0, 0, q, 0, 0 };
    unsigned sz = svc_pack(rb, sizeof rb, reply_port, D_MOVE_SO, SVC_REPLY_ID, &b, data, n, 0, 0);
    if (sz) mach_msg_n(rb, M_SEND | M_SEND_TO, sz, 0, 0, 1000);
}

/* aviso (sem resposta, msgh_id = id) para um envio guardado; devolve o kern_return do envio (SEND_INVALID_DEST: o cliente morreu) */
static inline i64 svc_note(unsigned port, unsigned id, unsigned a, const void *s1, unsigned n1) {
    unsigned char nb[1024];
    if (n1 > sizeof nb - 96) n1 = sizeof nb - 96;
    svc_body_t b = { 0, a, 0, 0, 0, 0, 0 };
    unsigned sz = svc_pack(nb, sizeof nb, port, D_COPY_SEND, id, &b, s1, n1, 0, 0);
    return sz ? mach_msg_n(nb, M_SEND | M_SEND_TO, sz, 0, 0, 0) : -1;
}

/* pedido com resposta a um serviço (nativo: launchctl/log do mOS). Resposta em rbuf. 0 = ok */
static inline i64 svc_call(const char *service, const svc_body_t *in, const void *s1, unsigned n1, const void *s2, unsigned n2,
                           unsigned char *rbuf, unsigned rcap, unsigned timeout_ms) {
    unsigned svc = 0;
    if (bs_op(MACH_BS_LOOK_UP, service, &svc) != 0 || !svc) return -1;
    unsigned reply = port_new_recv();
    if (!reply) return -1;
    unsigned sz = svc_pack(rbuf, rcap, svc, D_COPY_SEND | (D_MAKE_SO << 8), SVC_MSG_ID, in, s1, n1, s2, n2);
    if (!sz) { port_destroy(reply); return -1; }
    ((mhdr_t *)rbuf)->local = reply;
    i64 r = mach_msg_n(rbuf, M_SEND | M_RCV | M_RCV_TO | M_SEND_TO, sz, rcap, reply, timeout_ms);
    port_destroy(reply);
    port_drop(svc);
    return r;
}
