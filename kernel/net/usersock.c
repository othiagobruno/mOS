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

#define CHUNK 16384
i64 net_user_syscall(process_t *p, u64 op, u64 a, u64 b, u64 c) {
    i64 r;
    u64 fl;
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
        fl = spin_lock_irqsave(&g_net_lock);
        r = dns_resolve(host, &ip);
        spin_unlock_irqrestore(&g_net_lock, fl);
        if (r == 1 && b && !uvm_copy_out(p, b, &ip, sizeof ip)) return -MOS_EFAULT;
        return r;
    }
    case MOS_NET_CONNECT: {
        int slot = -1;
        fl = spin_lock_irqsave(&g_net_lock);
        for (int i = 0; i < USOCKS; i++) if (!us[i].pid) { slot = i; break; }
        if (slot < 0) { spin_unlock_irqrestore(&g_net_lock, fl); return -MOS_EMFILE; }
        net_info_t *ni = net_info();
        int t = ni && ni->configured ? tcp_connect((u32)a, (u16)b) : -1;
        if (t >= 0) { us[slot].pid = p->pid; us[slot].tcp = t; }
        spin_unlock_irqrestore(&g_net_lock, fl);
        return t >= 0 ? slot : -MOS_ENETUNREACH;
    }
    case MOS_NET_STATE: {
        fl = spin_lock_irqsave(&g_net_lock);
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
            fl = spin_lock_irqsave(&g_net_lock);
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
        fl = spin_lock_irqsave(&g_net_lock);
        int t = handle_of(p, a);
        int k = t < 0 ? -2 : tcp_recv(t, buf, (int)max);
        spin_unlock_irqrestore(&g_net_lock, fl);
        if (k == -2) { kfree(buf); return -MOS_EBADF; }
        if (k > 0 && !uvm_copy_out(p, b, buf, (usize)k)) { kfree(buf); return -MOS_EFAULT; }
        kfree(buf);
        return k;
    }
    case MOS_NET_CLOSE: {
        fl = spin_lock_irqsave(&g_net_lock);
        int t = handle_of(p, a);
        if (t >= 0) { tcp_close(t); us[a].pid = 0; }
        spin_unlock_irqrestore(&g_net_lock, fl);
        return t >= 0 ? 0 : -MOS_EBADF;
    }
    case MOS_NET_AVAIL: {
        fl = spin_lock_irqsave(&g_net_lock);
        int t = handle_of(p, a);
        r = t < 0 ? -MOS_EBADF : tcp_avail(t);
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
    spin_unlock_irqrestore(&g_net_lock, fl);
}
