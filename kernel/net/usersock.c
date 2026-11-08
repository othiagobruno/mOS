/* mOS — SYS_NET: a pilha TCP/DNS do kernel (tcp.c, dns.c) para os apps (sockets AF_INET do libSystem,
 * userland/libsystem/sockets.c). Os sete apps-alvo (docs/compat-apps.md) falam com a rede por getaddrinfo + sockets
 * TCP; o TLS é deles (BoringSSL no Chromium, rustls no Codex), então o kernel só entrega o fluxo TCP.
 *
 * Tudo é não-bloqueante, como a API tcp_*: o libSystem espera com SYS_SLEEP e pergunta de novo. Cada conexão
 * pertence ao processo que a abriu (a tabela guarda o pid); as de um processo que sai são fechadas por
 * net_user_exit (process.c, ao lado de machipc_exit). A trava g_net_lock (net.c) serializa com o net_poll do laço
 * da área de trabalho, que roda noutro núcleo. */
#include "kernel.h"
#include "net.h"
#include "proc.h"
#include "syscall.h"

bool uvm_copy_in(process_t *p, void *dst, u64 va, usize len);
bool uvm_copy_out(process_t *p, u64 va, const void *src, usize len);

#define USOCKS 64
static struct { int pid; int tcp; } us[USOCKS];   /* pid 0 = livre */

static int handle_of(process_t *p, u64 h) {
    if (h >= USOCKS || us[h].pid != p->pid) return -1;
    return us[h].tcp;
}

/* ---------------------------------------------------------------- TLS sobre canal de memória
 * O SecureTransport (SSL*) do libSystem entrega ao kernel registros TLS que o app leu pelas próprias callbacks
 * (FEED) e recolhe o que o motor quer mandar (DRAIN). O motor é o mesmo tls.c do navegador: TLS 1.3/1.2, cadeia
 * até as raízes da Mozilla, nome, datas e OCSP grampeado. Para ele a sessão é um "socket" de id
 * TCP_MEM_BASE + i: tcp_state/send/recv (tcp.c) desviam para memsock_*. g_tls_lock serializa o motor (o
 * crypto_random e as contas de chave pública não são reentrantes) e protege a tabela. */
#define UTLS 32
#define UTLS_IN  (40 * 1024)         /* entrada: um registro por vez (5 + 16 KiB + 256) com folga */
#define UTLS_OUT (128 * 1024)        /* saída: o obuf do tls.c tem 96 KiB */
typedef struct {
    int pid;
    tls_conn_t *tls;
    u8 *in, *out;
    u32 in_off, in_len, out_len;
    bool in_eof;
} utls_t;
static utls_t ut[UTLS];
static spinlock_t g_tls_lock;

static utls_t *ut_sock(int s) {
    int i = s - TCP_MEM_BASE;
    return i >= 0 && i < UTLS && ut[i].pid ? &ut[i] : NULL;
}
int memsock_state(int s) {
    utls_t *u = ut_sock(s);
    if (!u) return TCP_CLOSED;
    return u->in_eof ? TCP_PEER_CLOSED : TCP_OPEN;
}
int memsock_send(int s, const void *data, int len) {
    utls_t *u = ut_sock(s);
    if (!u || len < 0) return -1;
    u32 n = UTLS_OUT - u->out_len;
    if ((u32)len < n) n = (u32)len;
    memcpy(u->out + u->out_len, data, n);
    u->out_len += n;
    return (int)n;
}
int memsock_recv(int s, void *buf, int max) {
    utls_t *u = ut_sock(s);
    if (!u) return -1;
    u32 avail = u->in_len - u->in_off;
    if (!avail || max <= 0) return u->in_eof && !avail ? -1 : 0;
    u32 n = (u32)max < avail ? (u32)max : avail;
    memcpy(buf, u->in + u->in_off, n);
    u->in_off += n;
    if (u->in_off == u->in_len) u->in_off = u->in_len = 0;
    return (int)n;
}
static void ut_free(utls_t *u) {
    if (u->tls) tls_free(u->tls);
    kfree(u->in);
    kfree(u->out);
    memset(u, 0, sizeof *u);
}
static utls_t *ut_of(process_t *p, u64 h) {
    return h < UTLS && ut[h].pid == p->pid ? &ut[h] : NULL;
}

static i64 tls_user_op(process_t *p, u64 op, u64 a, u64 b, u64 c) {
    u64 fl;
    i64 r = -MOS_EBADF;
    if (op == MOS_NET_TLS_NEW) {
        char sni[256];
        usize i = 0;
        for (; a && i < sizeof sni - 1; i++) {
            if (!uvm_copy_in(p, &sni[i], a + i, 1)) return -MOS_EFAULT;
            if (!sni[i]) break;
        }
        sni[i] = 0;
        u8 *in = kmalloc(UTLS_IN), *out = kmalloc(UTLS_OUT);
        if (!in || !out) { kfree(in); kfree(out); return -MOS_ENOMEM; }
        fl = spin_lock_irqsave(&g_tls_lock);
        int slot = -1;
        for (int k = 0; k < UTLS; k++) if (!ut[k].pid) { slot = k; break; }
        if (slot >= 0) {
            utls_t *u = &ut[slot];
            u->pid = p->pid; u->in = in; u->out = out;
            u->tls = tls_new(TCP_MEM_BASE + slot, sni[0] ? sni : NULL);
            if (!u->tls) { ut_free(u); slot = -2; }
        }
        spin_unlock_irqrestore(&g_tls_lock, fl);
        if (slot < 0) { if (slot == -1) { kfree(in); kfree(out); } return slot == -1 ? -MOS_EMFILE : -MOS_ENOMEM; }
        return slot;
    }

    usize n = c > UTLS_OUT ? UTLS_OUT : (usize)c;
    u8 *buf = NULL;
    if (op != MOS_NET_TLS_HANDSHAKE) {
        buf = kmalloc(n ? n : 1);
        if (!buf) return -MOS_ENOMEM;
        if ((op == MOS_NET_TLS_FEED || op == MOS_NET_TLS_SEND) && n && !uvm_copy_in(p, buf, b, n)) { kfree(buf); return -MOS_EFAULT; }
    }
    i64 out = 0;                        /* bytes a copiar de volta para b */
    fl = spin_lock_irqsave(&g_tls_lock);
    utls_t *u = ut_of(p, a);
    if (u) switch (op) {
    case MOS_NET_TLS_FEED:
        if (!n) { u->in_eof = true; r = 0; break; }
        if (u->in_off) { memmove(u->in, u->in + u->in_off, u->in_len - u->in_off); u->in_len -= u->in_off; u->in_off = 0; }
        r = UTLS_IN - u->in_len < n ? UTLS_IN - u->in_len : (i64)n;
        memcpy(u->in + u->in_len, buf, (usize)r);
        u->in_len += (u32)r;
        break;
    case MOS_NET_TLS_DRAIN:
        r = u->out_len < n ? u->out_len : (i64)n;
        memcpy(buf, u->out, (usize)r);
        memmove(u->out, u->out + r, u->out_len - (u32)r);
        u->out_len -= (u32)r;
        out = r;
        break;
    case MOS_NET_TLS_HANDSHAKE: r = tls_handshake(u->tls); break;
    case MOS_NET_TLS_SEND:      r = tls_send(u->tls, buf, (int)n); break;
    case MOS_NET_TLS_RECV:      r = tls_recv(u->tls, buf, (int)(n > 16384 ? 16384 : n)); out = r > 0 ? r : 0; break;
    case MOS_NET_TLS_ERROR: {
        const char *e = tls_error(u->tls);
        usize k = strlen(e);
        if (k > n) k = n;
        memcpy(buf, e, k);
        r = out = (i64)k;
        break;
    }
    case MOS_NET_TLS_FREE:
        tls_free(u->tls);               /* enfileira o close_notify no canal de saída */
        u->tls = NULL;
        r = u->out_len < n ? u->out_len : (i64)n;
        memcpy(buf, u->out, (usize)r);
        out = r;
        ut_free(u);
        break;
    default: r = -MOS_EINVAL;
    }
    spin_unlock_irqrestore(&g_tls_lock, fl);
    if (out > 0 && !uvm_copy_out(p, b, buf, (usize)out)) r = -MOS_EFAULT;
    kfree(buf);
    return r;
}

#define CHUNK 16384
void net_loop_drain(void);
static i64 net6_user_op(process_t *p, u64 op, u64 a, u64 b, u64 c);
/* a trava da pilha; o laço local (127.x, ::1) anda a cada syscall, sem esperar o net_poll da área de trabalho */
void tcp_tick(u64 now);
static u64 net_lock(void) {
    u64 fl = spin_lock_irqsave(&g_net_lock);
    net_loop_drain();
    tcp_tick(timer_ticks());     /* ACKs pendentes (ack_now) também saem já */
    net_loop_drain();
    return fl;
}

static int us_alloc(process_t *p, int tcp) {
    for (int i = 0; i < USOCKS; i++) if (!us[i].pid) { us[i].pid = p->pid; us[i].tcp = tcp; return i; }
    return -1;
}

/* conexão para qualquer família; o loopback sem escuta é recusado na hora (como o connect do BSD) */
static i64 connect_any(process_t *p, const u8 *a16, u16 port) {
    u64 fl = net_lock();
    int slot = -1;
    for (int i = 0; i < USOCKS; i++) if (!us[i].pid) { slot = i; break; }
    if (slot < 0) { spin_unlock_irqrestore(&g_net_lock, fl); return -MOS_EMFILE; }
    int t = tcp_connect6(a16, port);
    if (t >= 0) us_alloc(p, t);
    spin_unlock_irqrestore(&g_net_lock, fl);
    return t >= 0 ? slot : t == -2 ? -MOS_ECONNREFUSED : -MOS_ENETUNREACH;
}

static i64 net6_user_op(process_t *p, u64 op, u64 a, u64 b, u64 c) {
    u64 fl;
    i64 r;
    u8 a16[16];
    switch (op) {
    case MOS_NET_RESOLVE6: {
        char host[256];
        usize i = 0;
        for (; i < sizeof host - 1; i++) {
            if (!uvm_copy_in(p, &host[i], a + i, 1)) return -MOS_EFAULT;
            if (!host[i]) break;
        }
        host[i] = 0;
        fl = net_lock();
        r = dns_resolve6(host, a16);
        spin_unlock_irqrestore(&g_net_lock, fl);
        if (r == 1 && b && !uvm_copy_out(p, b, a16, 16)) return -MOS_EFAULT;
        return r;
    }
    case MOS_NET_CONNECT6:
        if (!uvm_copy_in(p, a16, a, 16)) return -MOS_EFAULT;
        return connect_any(p, a16, (u16)b);
    case MOS_NET_LISTEN: {
        if (a) { if (!uvm_copy_in(p, a16, a, 16)) return -MOS_EFAULT; }
        else memset(a16, 0, 16);
        int fam = (int)((c >> 16) & 3);
        int backlog = (int)(c & 0xFFFF);
        fl = net_lock();
        int l = tcp_listen(a16, (u16)b, backlog, fam ? fam : (TCP_L_V4 | TCP_L_V6));
        int slot = l >= 0 ? us_alloc(p, l) : -1;
        if (l >= 0 && slot < 0) tcp_close(l);
        spin_unlock_irqrestore(&g_net_lock, fl);
        if (l == -2) return -MOS_EADDRINUSE;
        if (l < 0) return -MOS_EADDRNOTAVAIL;
        return slot >= 0 ? slot : -MOS_EMFILE;
    }
    case MOS_NET_ACCEPT: {
        fl = net_lock();
        int l = handle_of(p, a);
        if (l < TCP_LISTEN_BASE) { spin_unlock_irqrestore(&g_net_lock, fl); return -MOS_EINVAL; }
        int slot = -1;
        for (int i = 0; i < USOCKS; i++) if (!us[i].pid) { slot = i; break; }
        int t = slot >= 0 ? tcp_accept(l) : -1;
        if (t >= 0) us_alloc(p, t);
        spin_unlock_irqrestore(&g_net_lock, fl);
        if (slot < 0) return -MOS_EMFILE;
        return t >= 0 ? slot : -MOS_EAGAIN;
    }
    case MOS_NET_ADDRS: {
        mos_net_addrs_t o;
        memset(&o, 0, sizeof o);
        fl = net_lock();
        int t = handle_of(p, a);
        bool ok = t >= 0 && tcp_endpoints(t, o.lip, &o.lport, o.rip, &o.rport);
        spin_unlock_irqrestore(&g_net_lock, fl);
        if (!ok) return -MOS_EBADF;
        if (b && !uvm_copy_out(p, b, &o, sizeof o)) return -MOS_EFAULT;
        return 0;
    }
    case MOS_NET_INFO6: {
        mos_net6_info_t o;
        memset(&o, 0, sizeof o);
        net6_info_t *g = net6_info();
        if (g->ll_ok) { memcpy(o.ll, g->ll, 16); o.flags |= MOS_NET6_LL; }
        if (g->global_ok) { memcpy(o.global, g->global, 16); o.flags |= MOS_NET6_GLOBAL; o.prefix_len = g->prefix_len; }
        if (g->has_router) { memcpy(o.router, g->router, 16); o.flags |= MOS_NET6_ROUTER; }
        if (g->dns_ok) { memcpy(o.dns, g->dns, 16); o.flags |= MOS_NET6_DNS; }
        if (a && !uvm_copy_out(p, a, &o, sizeof o)) return -MOS_EFAULT;
        return ip6_has_global_route();
    }
    case MOS_NET_ICMP_SEND: {
        u8 msg[1200];
        if (c < 8 || c > sizeof msg) return -MOS_EINVAL;
        if (!uvm_copy_in(p, a16, a, 16) || !uvm_copy_in(p, msg, b, (usize)c)) return -MOS_EFAULT;
        fl = net_lock();
        bool ok = icmp_echo_send(a16, msg, (u16)c);
        spin_unlock_irqrestore(&g_net_lock, fl);
        return ok ? (i64)c : -MOS_ENETUNREACH;
    }
    case MOS_NET_ICMP_RECV: {
        u8 buf[16 + 256];
        if (c < 16) return -MOS_EINVAL;
        int max = (int)(c - 16 > 256 ? 256 : c - 16);
        fl = net_lock();
        int n = icmp_echo_recv((u16)a, buf, buf + 16, max);
        spin_unlock_irqrestore(&g_net_lock, fl);
        if (n <= 0) return 0;
        if (!uvm_copy_out(p, b, buf, (usize)(16 + n))) return -MOS_EFAULT;
        return 16 + n;
    }
    }
    return -MOS_EINVAL;
}

i64 net_user_syscall(process_t *p, u64 op, u64 a, u64 b, u64 c) {
    i64 r;
    u64 fl;
    if (op >= MOS_NET_TLS_NEW && op <= MOS_NET_TLS_FREE) return tls_user_op(p, op, a, b, c);
    if (op >= MOS_NET_RESOLVE6 && op <= MOS_NET_ICMP_RECV) return net6_user_op(p, op, a, b, c);
    if (op >= MOS_NET_UDP_OPEN && op <= MOS_NET_UDP_ADDRS) { extern i64 udp_user_op(process_t *p, u64 op, u64 a, u64 b, u64 c); return udp_user_op(p, op, a, b, c); }   /* userudp.c */
    if (op == MOS_NET_TLS_ROOTS) {             /* a mesma lista que o tls.c usa (kernel/arch/certs.S) */
        extern const char mos_cacert_pem[], mos_cacert_pem_end[];
        u64 len = (u64)(mos_cacert_pem_end - mos_cacert_pem);
        if (a && b < len) return -MOS_EINVAL;
        if (a && !uvm_copy_out(p, a, mos_cacert_pem, (usize)len)) return -MOS_EFAULT;
        return (i64)len;
    }
    switch (op) {
    case MOS_NET_RESOLVE: {
        char host[256];
        usize i = 0;
        for (; i < sizeof host - 1; i++) {                 /* copia o nome (até o 0) */
            if (!uvm_copy_in(p, &host[i], a + i, 1)) return -MOS_EFAULT;
            if (!host[i]) break;
        }
        host[i] = 0;
        u32 ip = 0;
        fl = net_lock();
        r = dns_resolve(host, &ip);
        spin_unlock_irqrestore(&g_net_lock, fl);
        if (r == 1 && b && !uvm_copy_out(p, b, &ip, sizeof ip)) return -MOS_EFAULT;
        return r;
    }
    case MOS_NET_CONNECT: {                        /* IPv4 (a = ip em ordem de host); o mesmo caminho do CONNECT6 */
        u8 a16[16];
        ip6_from_v4((u32)a, a16);
        return connect_any(p, a16, (u16)b);
    }
    case MOS_NET_STATE: {
        fl = net_lock();
        int t = handle_of(p, a);
        r = t < 0 ? -MOS_EBADF : tcp_state(t);
        spin_unlock_irqrestore(&g_net_lock, fl);
        return r;
    }
    case MOS_NET_SEND: {
        u8 *buf = kmalloc(CHUNK);
        if (!buf) return -MOS_ENOMEM;
        i64 done = 0;
        while ((u64)done < c) {
            usize n = c - (u64)done > CHUNK ? CHUNK : (usize)(c - (u64)done);
            if (!uvm_copy_in(p, buf, b + (u64)done, n)) { kfree(buf); return done ? done : -MOS_EFAULT; }
            fl = net_lock();
            int t = handle_of(p, a);
            int k = t < 0 ? -2 : tcp_send(t, buf, (int)n);
            spin_unlock_irqrestore(&g_net_lock, fl);
            if (k == -2) { kfree(buf); return -MOS_EBADF; }
            if (k < 0) { kfree(buf); return done ? done : -MOS_EPIPE; }
            done += k;
            if ((usize)k < n) break;                         /* buffer de envio cheio */
        }
        kfree(buf);
        return done;
    }
    case MOS_NET_RECV: {
        usize max = c > CHUNK ? CHUNK : (usize)c;
        u8 *buf = kmalloc(max ? max : 1);
        if (!buf) return -MOS_ENOMEM;
        fl = net_lock();
        int t = handle_of(p, a);
        int k = t < 0 ? -2 : tcp_recv(t, buf, (int)max);
        spin_unlock_irqrestore(&g_net_lock, fl);
        if (k == -2) { kfree(buf); return -MOS_EBADF; }
        if (k > 0 && !uvm_copy_out(p, b, buf, (usize)k)) { kfree(buf); return -MOS_EFAULT; }
        kfree(buf);
        return k;
    }
    case MOS_NET_CLOSE: {
        fl = net_lock();
        int t = handle_of(p, a);
        if (t >= 0) { tcp_close(t); us[a].pid = 0; }
        spin_unlock_irqrestore(&g_net_lock, fl);
        return t >= 0 ? 0 : -MOS_EBADF;
    }
    case MOS_NET_AVAIL: {
        fl = net_lock();
        int t = handle_of(p, a);
        r = t < 0 ? -MOS_EBADF : t >= TCP_LISTEN_BASE ? tcp_pending(t) : tcp_avail(t);
        spin_unlock_irqrestore(&g_net_lock, fl);
        return r;
    }
    case MOS_NET_INFO: {
        net_info_t *ni = net_info();
        u32 out[4] = {0};
        if (ni && ni->configured) { out[0] = ni->ip; out[1] = ni->mask; out[2] = ni->gw; out[3] = ni->dns; }
        if (a && !uvm_copy_out(p, a, out, sizeof out)) return -MOS_EFAULT;
        return ni && ni->configured;
    }
    }
    return -MOS_EINVAL;
}

/* o processo saiu: fecha as conexões dele */
void net_user_exit(process_t *p) {
    u64 fl = spin_lock_irqsave(&g_net_lock);
    for (int i = 0; i < USOCKS; i++)
        if (us[i].pid == p->pid) { tcp_close(us[i].tcp); us[i].pid = 0; }
    { extern void udp_user_exit_locked(process_t *p); udp_user_exit_locked(p); }   /* sockets UDP (userudp.c) */
    spin_unlock_irqrestore(&g_net_lock, fl);
    fl = spin_lock_irqsave(&g_tls_lock);
    for (int i = 0; i < UTLS; i++)
        if (ut[i].pid == p->pid) ut_free(&ut[i]);
    spin_unlock_irqrestore(&g_tls_lock, fl);
}
