/* mOS — UDP para os apps (SYS_NET, MOS_NET_UDP_*): os sockets SOCK_DGRAM do libSystem (sockets_udp.inc).
 *
 * Cada socket é uma porta UDP local com uma fila de datagramas recebidos. A entrada chega por udp_user_deliver, que
 * o net.c (IPv4) e o ipv6.c chamam quando nenhum serviço do kernel (DHCP, DNS) é dono da porta. A saída usa
 * udp_send/udp6_send; 127.x, ::1 e o próprio endereço voltam pela fila do laço local, então dois processos do mOS
 * conversam por UDP sem rede. Broadcast (255.255.255.255 e o da sub-rede) e multicast de saída (224.0.0.0/4, ff00::/8)
 * passam; multicast de entrada só chega ao grupo que a placa entrega.
 *
 * Os sockets pertencem ao processo (udp_user_exit, chamado por net_user_exit). A trava g_net_lock serializa com o
 * net_poll do laço da área de trabalho. */
#include "kernel.h"
#include "net.h"
#include "proc.h"
#include "syscall.h"

bool uvm_copy_in(process_t *p, void *dst, u64 va, usize len);
bool uvm_copy_out(process_t *p, u64 va, const void *src, usize len);
bool udp6_send(const u8 *dst, u16 sport, u16 dport, const void *data, u16 len);
bool udp_port_kernel(u16 port);
void net_loop_drain(void);

#define UUDP      128
#define UQ        128            /* datagramas guardados por socket */
#define UQ_BYTES  (256u << 10)   /* teto de bytes na fila (o resto é descartado, como num SO_RCVBUF cheio) */
#define UDP4_MAX  (1500 - 20 - 8)
#define UDP6_MAX  (1500 - 40 - 8)

typedef struct { u8 src[16]; u16 sport; u16 len; u8 *data; } udg_t;
typedef struct {
    int pid;                     /* 0 = livre */
    int fam;                     /* MOS_NET_L_V4 | MOS_NET_L_V6 */
    u16 port;                    /* 0 = sem bind ainda */
    u8 laddr[16];                /* endereço do bind (zeros = qualquer) */
    udg_t q[UQ];
    int qh, qn;
    u32 qbytes;
} uudp_t;
static uudp_t uu[UUDP];
static u16 next_eph = 49152;

static uudp_t *uu_of(process_t *p, u64 h) {
    if (h >= UUDP || !uu[h].pid || uu[h].pid != p->pid) return NULL;
    return &uu[h];
}

static bool port_used(u16 port) {
    if (udp_port_kernel(port)) return true;
    for (int i = 0; i < UUDP; i++) if (uu[i].pid && uu[i].port == port) return true;
    return false;
}

static u16 eph_port(void) {
    for (int n = 0; n < 16384; n++) {
        u16 p = next_eph++;
        if (next_eph >= 65535 || next_eph < 49152) next_eph = 49152;
        if (!port_used(p)) return p;
    }
    return 0;
}

static void q_clear(uudp_t *u) {
    for (int i = 0; i < u->qn; i++) kfree(u->q[(u->qh + i) % UQ].data);
    u->qh = u->qn = 0;
    u->qbytes = 0;
}

/* o endereço local do bind aceita esse destino? (zeros = qualquer; senão o mesmo endereço, ou broadcast/multicast) */
static bool laddr_ok(uudp_t *u, const u8 *dst16) {
    if (ip6_is_any(u->laddr)) return true;
    if (ip6_is_v4mapped(u->laddr) && ip6_to_v4(u->laddr) == 0) return true;   /* 0.0.0.0 */
    if (!dst16) return true;
    if (ip6_eq(u->laddr, dst16)) return true;
    if (ip6_is_v4mapped(dst16)) {
        u32 d = ip6_to_v4(dst16);
        if (d == 0xFFFFFFFFu || (d >> 28) == 0xE) return true;
    } else if (dst16[0] == 0xFF) return true;
    return false;
}

/* chamada pela entrada UDP do kernel (net.c e ipv6.c, com g_net_lock) para portas sem serviço do kernel */
bool udp_user_deliver(const u8 *src16, const u8 *dst16, u16 sport, u16 dport, const u8 *data, u16 len) {
    bool v4 = ip6_is_v4mapped(src16);
    for (int i = 0; i < UUDP; i++) {
        uudp_t *u = &uu[i];
        if (!u->pid || u->port != dport) continue;
        if (v4 && !(u->fam & MOS_NET_L_V4)) continue;
        if (!v4 && !(u->fam & MOS_NET_L_V6)) continue;
        if (!laddr_ok(u, dst16)) continue;
        if (u->qn >= UQ || u->qbytes + len > UQ_BYTES) return true;   /* fila cheia: descarta, como o BSD */
        udg_t *g = &u->q[(u->qh + u->qn) % UQ];
        g->data = len ? kmalloc(len) : NULL;
        if (len && !g->data) return true;
        if (len) memcpy(g->data, data, len);
        memcpy(g->src, src16, 16);
        g->sport = sport;
        g->len = len;
        u->qn++;
        u->qbytes += len;
        return true;
    }
    return false;
}

static u64 lock(void) {
    u64 fl = spin_lock_irqsave(&g_net_lock);
    net_loop_drain();            /* o laço local anda a cada syscall, sem esperar o net_poll */
    return fl;
}
static void unlock(u64 fl) { spin_unlock_irqrestore(&g_net_lock, fl); }

i64 udp_user_op(process_t *p, u64 op, u64 a, u64 b, u64 c) {
    if (op == MOS_NET_UDP_OPEN) {
        int fam = (int)(a & (MOS_NET_L_V4 | MOS_NET_L_V6));
        if (!fam) fam = MOS_NET_L_V4;
        u64 fl = lock();
        for (int i = 0; i < UUDP; i++)
            if (!uu[i].pid) {
                memset(&uu[i], 0, sizeof uu[i]);
                uu[i].pid = p->pid;
                uu[i].fam = fam;
                unlock(fl);
                return i;
            }
        unlock(fl);
        return -MOS_EMFILE;
    }
    u64 fl = lock();
    uudp_t *u = uu_of(p, a);
    if (!u) { unlock(fl); return -MOS_EBADF; }
    i64 r = 0;
    switch (op) {
    case MOS_NET_UDP_BIND: {
        u8 l[16];
        memset(l, 0, 16);
        if (b) { unlock(fl); if (!uvm_copy_in(p, l, b, 16)) return -MOS_EFAULT; fl = lock(); u = uu_of(p, a); if (!u) { unlock(fl); return -MOS_EBADF; } }
        if (u->port) { r = -MOS_EINVAL; break; }
        /* o endereço precisa ser desta máquina (ou qualquer, broadcast, multicast) */
        bool any = ip6_is_any(l) || (ip6_is_v4mapped(l) && ip6_to_v4(l) == 0);
        bool group = (ip6_is_v4mapped(l) && ((ip6_to_v4(l) >> 28) == 0xE || ip6_to_v4(l) == 0xFFFFFFFFu)) || (!ip6_is_v4mapped(l) && l[0] == 0xFF);
        if (!any && !group && !ip6_is_local(l)) { r = -MOS_EADDRNOTAVAIL; break; }
        u16 port = (u16)c;
        if (!port) port = eph_port();
        else if (port_used(port)) { r = -MOS_EADDRINUSE; break; }
        if (!port) { r = -MOS_EADDRINUSE; break; }
        u->port = port;
        memcpy(u->laddr, l, 16);
        r = port;
        break;
    }
    case MOS_NET_UDP_SENDTO: {
        mos_net_dgram_t d;
        unlock(fl);
        if (!uvm_copy_in(p, &d, b, sizeof d)) return -MOS_EFAULT;
        bool v4 = ip6_is_v4mapped(d.addr);
        u32 max = v4 ? UDP4_MAX : UDP6_MAX;
        if (d.len > max) return -MOS_EMSGSIZE;
        if (!d.port) return -MOS_EINVAL;
        u8 *buf = d.len ? kmalloc(d.len) : NULL;
        if (d.len && !buf) return -MOS_ENOMEM;
        if (d.len && !uvm_copy_in(p, buf, d.buf, d.len)) { kfree(buf); return -MOS_EFAULT; }
        fl = lock();
        u = uu_of(p, a);
        if (!u) { kfree(buf); unlock(fl); return -MOS_EBADF; }
        if ((v4 && !(u->fam & MOS_NET_L_V4)) || (!v4 && !(u->fam & MOS_NET_L_V6))) { kfree(buf); r = -MOS_ENETUNREACH; break; }
        if (!u->port) {
            u16 pt = eph_port();
            if (!pt) { kfree(buf); r = -MOS_EADDRINUSE; break; }
            u->port = pt;
        }
        bool ok;
        if (v4) ok = udp_send(ip6_to_v4(d.addr), u->port, d.port, buf, (u16)d.len);
        else ok = udp6_send(d.addr, u->port, d.port, buf, (u16)d.len);
        kfree(buf);
        net_loop_drain();        /* entrega já os do laço local */
        r = ok ? (i64)d.len : -MOS_ENETUNREACH;
        break;
    }
    case MOS_NET_UDP_RECVFROM: {
        mos_net_dgram_t d;
        unlock(fl);
        if (!uvm_copy_in(p, &d, b, sizeof d)) return -MOS_EFAULT;
        fl = lock();
        u = uu_of(p, a);
        if (!u) { unlock(fl); return -MOS_EBADF; }
        if (!u->qn) { unlock(fl); return -MOS_EAGAIN; }
        udg_t g = u->q[u->qh];
        bool peek = d.flags & MOS_NET_DG_PEEK;
        if (!peek) { u->qh = (u->qh + 1) % UQ; u->qn--; u->qbytes -= g.len; }
        unlock(fl);
        u32 n = MIN((u32)g.len, d.cap);
        bool okc = !n || uvm_copy_out(p, d.buf, g.data, n);
        memcpy(d.addr, g.src, 16);
        d.port = g.sport;
        d.flags = g.len > d.cap ? MOS_NET_DG_TRUNC : 0;
        d.len = g.len;
        if (!peek) kfree(g.data);
        if (!okc || !uvm_copy_out(p, b, &d, sizeof d)) return -MOS_EFAULT;
        return (i64)n;   /* 0 = datagrama vazio (válido no UDP) */
    }
    case MOS_NET_UDP_AVAIL:
        r = u->qn ? u->q[u->qh].len : -1;
        break;
    case MOS_NET_UDP_CLOSE:
        q_clear(u);
        u->pid = 0;
        u->port = 0;
        break;
    case MOS_NET_UDP_ADDRS: {
        mos_net_addrs_t ad;
        memset(&ad, 0, sizeof ad);
        memcpy(ad.lip, u->laddr, 16);
        ad.lport = u->port;
        unlock(fl);
        return uvm_copy_out(p, b, &ad, sizeof ad) ? 0 : -MOS_EFAULT;
    }
    default: r = -MOS_EINVAL;
    }
    unlock(fl);
    return r;
}

/* o processo saiu: fecha os sockets UDP dele (chamado por net_user_exit, já com g_net_lock) */
void udp_user_exit_locked(process_t *p) {
    for (int i = 0; i < UUDP; i++)
        if (uu[i].pid == p->pid) { q_clear(&uu[i]); uu[i].pid = 0; uu[i].port = 0; }
}
