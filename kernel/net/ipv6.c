/* mOS — IPv6 (RFC 8200), ICMPv6 (RFC 4443), descoberta de vizinhos e roteadores (NDP, RFC 4861), autoconfiguração
 * sem estado (SLAAC, RFC 4862, endereço EUI-64), DNS anunciado pelo roteador (RDNSS, RFC 8106), UDP sobre IPv6 e o
 * laço local (loopback) de IPv4 e IPv6.
 *
 * Endereços são sempre 16 bytes em ordem de rede. Fora daqui o IPv4 aparece como ::ffff:a.b.c.d (mapeado, RFC 4291
 * 2.5.5.2): o TCP (tcp.c) e os sockets dos apps (usersock.c) guardam um endereço de 16 bytes e escolhem a família por
 * ele (ip6_is_v4mapped).
 *
 * O que não tem: fragmentação (descartada), cabeçalho de roteamento, MLD (o e1000 recebe todo multicast com
 * RCTL_MPE, então os grupos de nó solicitado chegam sem registro), DAD com espera (o endereço vale na hora — o
 * "otimista" do RFC 4429), DHCPv6, endereços temporários (RFC 4941) e vários prefixos (fica o primeiro com A=1). */
#include "netpriv.h"

net6_info_t g6;

const u8 IP6_ANY[16];
const u8 IP6_LOOPBACK[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
static const u8 ALL_NODES[16] = {0xFF, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
static const u8 ALL_ROUTERS[16] = {0xFF, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2};

net6_info_t *net6_info(void) { return &g6; }

/* =================================================================== utilidades */
void ip6_from_v4(u32 ip, u8 *o) {
    memset(o, 0, 10);
    o[10] = o[11] = 0xFF;
    wr32(o + 12, ip);
}
bool ip6_is_v4mapped(const u8 *a) {
    static const u8 pre[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF};
    return !memcmp(a, pre, 12);
}
u32 ip6_to_v4(const u8 *a) { return rd32(a + 12); }
bool ip6_eq(const u8 *a, const u8 *b) { return !memcmp(a, b, 16); }
bool ip6_is_any(const u8 *a) { return ip6_eq(a, IP6_ANY); }
static bool is_linklocal(const u8 *a) { return a[0] == 0xFE && (a[1] & 0xC0) == 0x80; }
static bool is_mcast(const u8 *a) { return a[0] == 0xFF; }

/* endereço é desta máquina (inclui o laço local v4/v6) */
bool ip6_is_local(const u8 *a) {
    if (ip6_is_v4mapped(a)) {
        u32 v = ip6_to_v4(a);
        return (v >> 24) == 127 || (g_net.configured && v == g_net.ip);
    }
    if (ip6_eq(a, IP6_LOOPBACK)) return true;
    if (g6.ll_ok && ip6_eq(a, g6.ll)) return true;
    if (g6.global_ok && ip6_eq(a, g6.global)) return true;
    return false;
}

void ip6_to_str(const u8 *a, char *out) {   /* out: 46 bytes */
    if (ip6_is_v4mapped(a)) {
        char v4[16];
        ip_to_str(ip6_to_v4(a), v4);
        snprintf(out, 46, "::ffff:%s", v4);
        return;
    }
    u16 g[8];
    for (int i = 0; i < 8; i++) g[i] = rd16(a + 2 * i);
    int bs = -1, bl = 0;
    for (int i = 0; i < 8;) {
        if (g[i]) { i++; continue; }
        int j = i;
        while (j < 8 && !g[j]) j++;
        if (j - i > bl && j - i > 1) { bs = i; bl = j - i; }
        i = j;
    }
    int o = 0;
    for (int i = 0; i < 8; i++) {
        if (i == bs) { out[o++] = ':'; out[o++] = ':'; i += bl - 1; continue; }
        if (i && !(bs >= 0 && i == bs + bl)) out[o++] = ':';
        o += snprintf(out + o, 46 - o, "%x", g[i]);
    }
    out[o] = 0;
}

bool str_to_ip6(const char *s, u8 *out) {
    u16 g[8];
    int n = 0, gap = -1;
    if (!s) return false;
    if (s[0] == ':') { if (s[1] != ':') return false; s++; }
    while (*s) {
        if (*s == ':') {
            if (gap >= 0) return false;
            gap = n; s++;
            if (!*s) break;
            continue;
        }
        const char *dot = NULL;
        for (const char *q = s; *q && *q != ':'; q++) if (*q == '.') { dot = q; break; }
        if (dot) {
            u32 v4;
            if (n > 6 || !str_to_ip(s, &v4)) return false;
            g[n++] = (u16)(v4 >> 16); g[n++] = (u16)v4;
            s += strlen(s);
            break;
        }
        u32 v = 0;
        int k = 0;
        for (; k < 4; k++, s++) {
            char c = *s;
            int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
            if (d < 0) break;
            v = v << 4 | (u32)d;
        }
        if (!k || n >= 8) return false;
        g[n++] = (u16)v;
        if (*s == ':') { s++; if (!*s) return false; if (*s == ':') continue; }
        else if (*s) return false;
    }
    if (gap < 0 ? n != 8 : n > 7) return false;
    u16 full[8] = {0};
    if (gap < 0) memcpy(full, g, sizeof full);
    else {
        for (int i = 0; i < gap; i++) full[i] = g[i];
        for (int i = gap; i < n; i++) full[8 - (n - i)] = g[i];
    }
    if (out) for (int i = 0; i < 8; i++) wr16(out + 2 * i, full[i]);
    return true;
}

/* soma do pseudocabeçalho de TCP/UDP/ICMPv6 para os dois IPs (o IPv4 mapeado usa o de 12 bytes do RFC 793) */
u32 l4_pseudo_sum(const u8 *src, const u8 *dst, u8 proto, u16 len) {
    if (ip6_is_v4mapped(src) && ip6_is_v4mapped(dst)) return inet_pseudo_sum(ip6_to_v4(src), ip6_to_v4(dst), proto, len);
    u32 s = inet_csum_add(0, src, 16);
    s = inet_csum_add(s, dst, 16);
    s += len;          /* comprimento de 32 bits: a metade alta é 0 */
    s += proto;
    return s;
}

/* endereço de origem para falar com dst (RFC 6724 resumido): laço local, link-local ou o global */
bool ip6_source_for(const u8 *dst, u8 *src) {
    if (ip6_is_v4mapped(dst)) {
        u32 v = ip6_to_v4(dst);
        if ((v >> 24) == 127) { ip6_from_v4(0x7F000001, src); return true; }
        if (!g_net.configured) return false;
        ip6_from_v4(g_net.ip, src);
        return true;
    }
    if (ip6_eq(dst, IP6_LOOPBACK)) { memcpy(src, IP6_LOOPBACK, 16); return true; }
    if (ip6_is_local(dst)) { memcpy(src, dst, 16); return true; }
    if (is_linklocal(dst) || (is_mcast(dst) && (dst[1] & 0x0F) == 2)) {
        if (!g6.ll_ok) return false;
        memcpy(src, g6.ll, 16);
        return true;
    }
    if (g6.global_ok) { memcpy(src, g6.global, 16); return true; }
    return false;
}

/* há rota IPv6 para fora do enlace (endereço global + roteador padrão): o getaddrinfo só prefere AAAA assim */
bool ip6_has_global_route(void) { return g6.global_ok && g6.has_router; }

/* =================================================================== laço local
 * Pacotes para 127.x, ::1 ou para o próprio endereço não vão à placa: entram numa fila e são entregues de novo
 * à entrada (ipv4_input/ipv6_input) por net_loop_drain, chamado pelo net_poll e pelas syscalls de rede dos apps
 * (usersock.c), para que o TCP local não espere o laço da área de trabalho. */
#define LOOP_Q 96
typedef struct { u16 len; u8 v6; u8 data[ETH_MTU]; } loop_pkt_t;
static loop_pkt_t *loopq;
static int loop_head, loop_count;
static bool loop_draining;

bool net_loop_enqueue(bool v6, const void *pkt, u16 len) {
    if (len > ETH_MTU) return false;
    if (!loopq) loopq = kcalloc(LOOP_Q, sizeof(loop_pkt_t));
    if (!loopq || loop_count >= LOOP_Q) return false;     /* cheio: perdido, como numa placa (o TCP retransmite) */
    loop_pkt_t *q = &loopq[(loop_head + loop_count) % LOOP_Q];
    q->len = len; q->v6 = v6;
    memcpy(q->data, pkt, len);
    loop_count++;
    return true;
}

void net_loop_drain(void) {
    if (loop_draining || !loopq) return;
    loop_draining = true;
    static u8 buf[ETH_MTU];
    for (int budget = 256; loop_count && budget; budget--) {
        loop_pkt_t *q = &loopq[loop_head];
        u16 len = q->len;
        bool v6 = q->v6;
        memcpy(buf, q->data, len);
        loop_head = (loop_head + 1) % LOOP_Q;
        loop_count--;
        if (v6) ipv6_input(buf, len);
        else ipv4_input(buf, len);
    }
    loop_draining = false;
}

/* =================================================================== vizinhos (NDP) */
#define NB_ENTRIES 16
#define NB_TTL_MS  (10 * 60 * 1000)
#define NB_RETRY   1000
#define NB_TRIES   3
#define NB_QUEUE   8
typedef struct { u8 ip[16]; u8 mac[6]; u64 time, last_req; bool valid, resolving; u8 tries; } nb_entry_t;
typedef struct { bool used; u8 hop[16]; u16 len; u8 data[ETH_MTU]; } nb_pending_t;
static nb_entry_t nbs[NB_ENTRIES];
static nb_pending_t *nbq;

static nb_entry_t *nb_find(const u8 *ip) {
    for (int i = 0; i < NB_ENTRIES; i++) if ((nbs[i].valid || nbs[i].resolving) && ip6_eq(nbs[i].ip, ip)) return &nbs[i];
    return NULL;
}
static nb_entry_t *nb_alloc(const u8 *ip) {
    nb_entry_t *e = nb_find(ip);
    if (e) return e;
    nb_entry_t *old = &nbs[0];
    for (int i = 0; i < NB_ENTRIES; i++) {
        if (!nbs[i].valid && !nbs[i].resolving) { old = &nbs[i]; break; }
        if (nbs[i].time < old->time) old = &nbs[i];
    }
    memset(old, 0, sizeof *old);
    memcpy(old->ip, ip, 16);
    return old;
}

static void mcast_mac(const u8 *ip, u8 *mac) {
    mac[0] = 0x33; mac[1] = 0x33;
    memcpy(mac + 2, ip + 12, 4);
}
static void solicited_node(const u8 *ip, u8 *o) {
    static const u8 pre[13] = {0xFF, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0xFF};
    memcpy(o, pre, 13);
    memcpy(o + 13, ip + 13, 3);
}

static bool raw_send(const u8 *mac, const u8 *pkt, u16 len) { return net_eth_send(mac, ETH_P_IPV6, pkt, len); }

static void ns_send(const u8 *target);

static void nb_learn(const u8 *ip, const u8 *mac, bool create) {
    if (ip6_is_any(ip) || (mac[0] & 1)) return;
    nb_entry_t *e = nb_find(ip);
    if (!e) { if (!create) return; e = nb_alloc(ip); }
    memcpy(e->mac, mac, 6);
    e->valid = true;
    e->resolving = false;
    e->time = timer_ticks();
    if (!nbq) return;
    for (int i = 0; i < NB_QUEUE; i++)
        if (nbq[i].used && ip6_eq(nbq[i].hop, ip)) { raw_send(mac, nbq[i].data, nbq[i].len); nbq[i].used = false; }
}

static void nb_tick(u64 now) {
    for (int i = 0; i < NB_ENTRIES; i++) {
        nb_entry_t *e = &nbs[i];
        if (e->valid && now - e->time > NB_TTL_MS) e->valid = false;
        if (!e->resolving || now - e->last_req < NB_RETRY) continue;
        if (e->tries >= NB_TRIES) {
            char s[46];
            ip6_to_str(e->ip, s);
            kprintf("ndp: sem resposta de %s\n", s);
            e->resolving = false;
            if (nbq) for (int k = 0; k < NB_QUEUE; k++) if (nbq[k].used && ip6_eq(nbq[k].hop, e->ip)) nbq[k].used = false;
            continue;
        }
        e->tries++;
        e->last_req = now;
        ns_send(e->ip);
    }
}

/* =================================================================== saída */
static bool on_link(const u8 *dst) {
    if (is_linklocal(dst)) return true;
    if (!g6.global_ok || !g6.prefix_len) return false;
    int bits = g6.prefix_len, i = 0;
    for (; bits >= 8; bits -= 8, i++) if (dst[i] != g6.global[i]) return false;
    if (bits) { u8 m = (u8)(0xFF << (8 - bits)); if ((dst[i] & m) != (g6.global[i] & m)) return false; }
    return true;
}

bool ipv6_send_hl(const u8 *src, const u8 *dst, u8 nh, u8 hop_limit, const void *payload, u16 len) {
    if (len > ETH_MTU - IP6_HLEN) return false;
    static u8 pkt[ETH_MTU];
    u8 sbuf[16];
    if (!src) { if (!ip6_source_for(dst, sbuf)) return false; src = sbuf; }
    pkt[0] = 0x60; pkt[1] = 0; pkt[2] = 0; pkt[3] = 0;
    wr16(pkt + 4, len);
    pkt[6] = nh;
    pkt[7] = hop_limit;
    memcpy(pkt + 8, src, 16);
    memcpy(pkt + 24, dst, 16);
    memcpy(pkt + IP6_HLEN, payload, len);
    u16 total = (u16)(IP6_HLEN + len);
    if (ip6_is_local(dst)) return net_loop_enqueue(true, pkt, total);
    if (!g_netdev) return false;
    if (is_mcast(dst)) { u8 mac[6]; mcast_mac(dst, mac); return raw_send(mac, pkt, total); }
    const u8 *hop = on_link(dst) ? dst : (g6.has_router ? g6.router : NULL);
    if (!hop) return false;
    nb_entry_t *e = nb_find(hop);
    if (e && e->valid) return raw_send(e->mac, pkt, total);
    if (!nbq) nbq = kcalloc(NB_QUEUE, sizeof(nb_pending_t));
    if (!nbq) return false;
    int slot = -1;
    for (int i = 0; i < NB_QUEUE; i++) if (!nbq[i].used) { slot = i; break; }
    if (slot < 0) slot = (int)(rand32() % NB_QUEUE);
    nbq[slot].used = true;
    memcpy(nbq[slot].hop, hop, 16);
    nbq[slot].len = total;
    memcpy(nbq[slot].data, pkt, total);
    if (!e) {
        e = nb_alloc(hop);
        e->resolving = true;
        e->tries = 1;
        e->time = e->last_req = timer_ticks();
        ns_send(hop);
    }
    return true;
}

bool ipv6_send(const u8 *src, const u8 *dst, u8 nh, const void *payload, u16 len) {
    return ipv6_send_hl(src, dst, nh, 64, payload, len);
}

/* qualquer família: IPv4 mapeado vai pelo ipv4.c, o resto por aqui */
bool ip_send_any(const u8 *src, const u8 *dst, u8 proto, const void *payload, u16 len) {
    if (ip6_is_v4mapped(dst)) return ipv4_send_from(src ? ip6_to_v4(src) : 0, ip6_to_v4(dst), proto, payload, len);
    return ipv6_send(src, dst, proto, payload, len);
}

/* =================================================================== ICMPv6 */
static void icmp6_send(const u8 *src, const u8 *dst, u8 hl, u8 *msg, u16 len) {
    u8 sbuf[16];
    if (!src) { if (!ip6_source_for(dst, sbuf)) return; src = sbuf; }
    wr16(msg + 2, 0);
    wr16(msg + 2, inet_csum_fold(inet_csum_add(l4_pseudo_sum(src, dst, IPPROTO_ICMPV6, len), msg, len)));
    ipv6_send_hl(src, dst, IPPROTO_ICMPV6, hl, msg, len);
}

/* solicitação de vizinho: para o grupo de nó solicitado do alvo, com nosso MAC (opção 1) */
static void ns_send(const u8 *target) {
    u8 m[32];
    memset(m, 0, sizeof m);
    m[0] = 135;
    memcpy(m + 8, target, 16);
    m[24] = 1; m[25] = 1;
    memcpy(m + 26, g_net.mac, 6);
    u8 dst[16];
    solicited_node(target, dst);
    const u8 *src = g6.ll_ok ? g6.ll : NULL;
    if (!src) return;
    if (g6.global_ok && !is_linklocal(target) && on_link(target)) src = g6.global;
    icmp6_send(src, dst, 255, m, sizeof m);
}

static void na_send(const u8 *target, const u8 *dst, bool solicited) {
    u8 m[32];
    memset(m, 0, sizeof m);
    m[0] = 136;
    m[4] = (u8)(0x20 | (solicited ? 0x40 : 0));     /* O (substituir) e S (solicitado) */
    memcpy(m + 8, target, 16);
    m[24] = 2; m[25] = 1;                              /* endereço de enlace do alvo */
    memcpy(m + 26, g_net.mac, 6);
    icmp6_send(target, dst, 255, m, sizeof m);
}

static void rs_send(void) {
    if (!g6.ll_ok) return;
    u8 m[16];
    memset(m, 0, sizeof m);
    m[0] = 133;
    m[8] = 1; m[9] = 1;
    memcpy(m + 10, g_net.mac, 6);
    icmp6_send(g6.ll, ALL_ROUTERS, 255, m, sizeof m);
}

/* ---- respostas de eco para os apps (ping/ping6: socket SOCK_DGRAM com IPPROTO_ICMP/ICMPV6) e o terminal */
#define ECHO_RING 16
typedef struct { bool used; u16 id; u8 from[16]; u16 len; u8 data[256]; } echo_rx_t;
static echo_rx_t echos[ECHO_RING];
static int echo_next;

void icmp_echo_deliver(const u8 *from, const u8 *msg, u16 len) {
    if (len < 8) return;
    echo_rx_t *e = &echos[echo_next];
    echo_next = (echo_next + 1) % ECHO_RING;
    e->used = true;
    e->id = rd16(msg + 4);
    memcpy(e->from, from, 16);
    e->len = len > sizeof e->data ? (u16)sizeof e->data : len;
    memcpy(e->data, msg, e->len);
}

/* tira a resposta mais antiga com este identificador; devolve o tamanho, 0 se não há */
int icmp_echo_recv(u16 id, u8 *from, void *buf, int max) {
    int best = -1;
    for (int k = 0; k < ECHO_RING; k++) {
        int i = (echo_next + k) % ECHO_RING;       /* do mais antigo para o mais novo */
        if (echos[i].used && echos[i].id == id) { best = i; break; }
    }
    if (best < 0) return 0;
    echo_rx_t *e = &echos[best];
    int n = e->len < max ? e->len : max;
    memcpy(buf, e->data, (usize)n);
    if (from) memcpy(from, e->from, 16);
    e->used = false;
    return n;
}

/* envia uma mensagem de eco montada pelo app (tipo 8 ou 128): o kernel refaz a soma de verificação */
bool icmp_echo_send(const u8 *dst, const void *msg, u16 len) {
    if (len < 8 || len > 1200) return false;
    static u8 m[1200];
    memcpy(m, msg, len);
    if (ip6_is_v4mapped(dst)) {
        m[0] = 8; m[1] = 0;
        wr16(m + 2, 0);
        wr16(m + 2, inet_csum_fold(inet_csum_add(0, m, len)));
        return ip_send_any(NULL, dst, IPPROTO_ICMP, m, len);
    }
    m[0] = 128; m[1] = 0;
    u8 src[16];
    if (!ip6_source_for(dst, src)) return false;
    icmp6_send(src, dst, 64, m, len);
    return true;
}

static void eui64(const u8 *prefix, u8 *o) {
    memcpy(o, prefix, 8);
    o[8] = g_net.mac[0] ^ 0x02; o[9] = g_net.mac[1]; o[10] = g_net.mac[2];
    o[11] = 0xFF; o[12] = 0xFE;
    o[13] = g_net.mac[3]; o[14] = g_net.mac[4]; o[15] = g_net.mac[5];
}

static void ra_input(const u8 *src, const u8 *m, u16 len) {
    if (len < 16 || !is_linklocal(src)) return;
    u16 life = rd16(m + 6);
    bool changed = false;
    if (life) {
        if (!g6.has_router || !ip6_eq(g6.router, src)) changed = true;
        memcpy(g6.router, src, 16);
        g6.has_router = true;
        g6.router_until = timer_ticks() + (u64)life * 1000;
    } else if (g6.has_router && ip6_eq(g6.router, src)) {
        g6.has_router = false;
    }
    g6.ra_seen = true;
    for (u16 o = 16; o + 8 <= len;) {
        u8 type = m[o], l = m[o + 1];
        if (!l || o + l * 8 > len) break;
        const u8 *op = m + o;
        if (type == 1 && l == 1) nb_learn(src, op + 2, true);
        else if (type == 3 && l == 4) {
            u8 plen = op[2], flags = op[3];
            u32 valid = rd32(op + 4);
            const u8 *prefix = op + 16;
            if ((flags & 0x40) && plen == 64 && valid && !is_linklocal(prefix) && !g6.global_ok) {
                eui64(prefix, g6.global);
                g6.prefix_len = 64;
                g6.global_ok = true;
                changed = true;
            }
        } else if (type == 25 && l >= 3) {
            u32 lt = rd32(op + 4);
            if (lt && (!g6.dns_ok || !ip6_eq(g6.dns, op + 8))) { memcpy(g6.dns, op + 8, 16); g6.dns_ok = true; changed = true; }
        }
        o = (u16)(o + l * 8);
    }
    if (changed) {
        char a[46], r[46], d[46];
        ip6_to_str(g6.global_ok ? g6.global : g6.ll, a);
        ip6_to_str(g6.router, r);
        ip6_to_str(g6.dns, d);
        kprintf("net6: configurado por SLAAC: %s/%u roteador %s dns %s\n", a, g6.prefix_len ? g6.prefix_len : 64, g6.has_router ? r : "-", g6.dns_ok ? d : "-");
    }
}

static void icmp6_input(const u8 *src, const u8 *dst, const u8 *m, u16 len) {
    if (len < 4) return;
    if (inet_csum_fold(inet_csum_add(l4_pseudo_sum(src, dst, IPPROTO_ICMPV6, len), m, len)) != 0) return;
    switch (m[0]) {
    case 128: {                                       /* pedido de eco: responde */
        if (len < 8 || is_mcast(src)) return;
        static u8 r[ETH_MTU];
        if (len > ETH_MTU - IP6_HLEN) return;
        memcpy(r, m, len);
        r[0] = 129;
        icmp6_send(is_mcast(dst) ? NULL : dst, src, 64, r, len);
        break;
    }
    case 129: icmp_echo_deliver(src, m, len); break;  /* resposta de eco */
    case 134: ra_input(src, m, len); break;
    case 135: {                                       /* solicitação de vizinho */
        if (len < 24) return;
        const u8 *target = m + 8;
        bool ours = (g6.ll_ok && ip6_eq(target, g6.ll)) || (g6.global_ok && ip6_eq(target, g6.global));
        const u8 *slla = NULL;
        for (u16 o = 24; o + 8 <= len;) {
            u8 l = m[o + 1];
            if (!l || o + l * 8 > len) break;
            if (m[o] == 1) slla = m + o + 2;
            o = (u16)(o + l * 8);
        }
        if (slla && !ip6_is_any(src)) nb_learn(src, slla, ours);
        if (!ours) return;
        if (ip6_is_any(src)) na_send(target, ALL_NODES, false);   /* DAD de outro nó com o nosso endereço */
        else na_send(target, src, true);
        break;
    }
    case 136: {                                       /* anúncio de vizinho */
        if (len < 24) return;
        const u8 *target = m + 8;
        for (u16 o = 24; o + 8 <= len;) {
            u8 l = m[o + 1];
            if (!l || o + l * 8 > len) break;
            if (m[o] == 2) nb_learn(target, m + o + 2, false);
            o = (u16)(o + l * 8);
        }
        break;
    }
    }
}

/* =================================================================== UDP sobre IPv6 */
bool udp6_send(const u8 *dst, u16 sport, u16 dport, const void *data, u16 len) {
    if (len > ETH_MTU - IP6_HLEN - 8) return false;
    u8 src[16];
    if (!ip6_source_for(dst, src)) return false;
    static u8 p[ETH_MTU];
    u16 ulen = (u16)(8 + len);
    wr16(p, sport); wr16(p + 2, dport); wr16(p + 4, ulen); wr16(p + 6, 0);
    memcpy(p + 8, data, len);
    u16 c = inet_csum_fold(inet_csum_add(l4_pseudo_sum(src, dst, IPPROTO_UDP, ulen), p, ulen));
    wr16(p + 6, c ? c : 0xFFFF);
    return ipv6_send(src, dst, IPPROTO_UDP, p, ulen);
}

static void udp6_input(const u8 *src, const u8 *dst, const u8 *p, u16 len) {
    if (len < 8) return;
    u16 ulen = rd16(p + 4);
    if (ulen < 8 || ulen > len || !rd16(p + 6)) return;   /* no IPv6 a soma é obrigatória */
    if (inet_csum_fold(inet_csum_add(l4_pseudo_sum(src, dst, IPPROTO_UDP, ulen), p, ulen)) != 0) return;
    udp_deliver(0, rd16(p), rd16(p + 2), p + 8, (u16)(ulen - 8));
}

/* =================================================================== entrada */
static bool dst_ours(const u8 *dst, bool from_loop) {
    if (ip6_is_local(dst)) return from_loop || !ip6_eq(dst, IP6_LOOPBACK);
    if (!is_mcast(dst)) return false;
    if (ip6_eq(dst, ALL_NODES)) return true;
    u8 sn[16];
    if (g6.ll_ok) { solicited_node(g6.ll, sn); if (ip6_eq(dst, sn)) return true; }
    if (g6.global_ok) { solicited_node(g6.global, sn); if (ip6_eq(dst, sn)) return true; }
    return false;
}

static void ipv6_input_from(const u8 *p, u16 len, bool from_loop) {
    if (len < IP6_HLEN || (p[0] >> 4) != 6) return;
    u16 plen = rd16(p + 4);
    if (IP6_HLEN + plen > len) return;
    const u8 *src = p + 8, *dst = p + 24;
    if (!dst_ours(dst, from_loop)) return;
    if (is_mcast(src)) return;
    u8 nh = p[6];
    const u8 *pl = p + IP6_HLEN;
    u16 rem = plen;
    /* cabeçalhos de extensão simples: salto a salto, opções de destino e roteamento sem segmentos restantes */
    for (int guard = 0; guard < 8 && (nh == 0 || nh == 60 || nh == 43); guard++) {
        if (rem < 8) return;
        u16 hl = (u16)((pl[1] + 1) * 8);
        if (hl > rem) return;
        if (nh == 43 && pl[3]) return;
        nh = pl[0];
        pl += hl; rem = (u16)(rem - hl);
    }
    switch (nh) {
    case IPPROTO_ICMPV6: icmp6_input(src, dst, pl, rem); break;
    case IPPROTO_UDP:    udp6_input(src, dst, pl, rem); break;
    case IPPROTO_TCP:    if (!is_mcast(dst)) tcp_input6(src, dst, pl, rem); break;
    default: break;                                   /* 44 (fragmento) e o resto: descartados */
    }
}

void ipv6_input(const u8 *p, u16 len) { ipv6_input_from(p, len, true); }
void ipv6_eth_input(const u8 *p, u16 len) { ipv6_input_from(p, len, false); }

/* =================================================================== início e temporizadores */
static u64 rs_next;
static u8  rs_tries;

/* interface nova ou link de volta: endereço link-local EUI-64, esquece o roteador e pede um RA */
void ipv6_start(void) {
    memset(&g6, 0, sizeof g6);
    memset(nbs, 0, sizeof nbs);
    static const u8 ll_prefix[8] = {0xFE, 0x80, 0, 0, 0, 0, 0, 0};
    eui64(ll_prefix, g6.ll);
    g6.ll_ok = true;
    rs_tries = 0;
    rs_next = timer_ticks() + 300;
    char s[46];
    ip6_to_str(g6.ll, s);
    kprintf("net6: link-local %s\n", s);
}

void ipv6_tick(u64 now) {
    if (!g_netdev || !g6.ll_ok) return;
    if (!g6.ra_seen && rs_tries < 4 && now >= rs_next) {
        rs_tries++;
        rs_next = now + 4000;
        rs_send();
    }
    if (g6.has_router && g6.router_until && now >= g6.router_until) g6.has_router = false;
    nb_tick(now);
}
