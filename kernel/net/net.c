/* mOS — núcleo da pilha de rede: dispositivo, Ethernet, ARP, IPv4, ICMP e UDP */
#include "netpriv.h"
#include "ui.h"

net_info_t g_net;
netdev_t  *g_netdev;

static const u8 MAC_BCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static u8  rx_frame[ETH_FRAME_MAX + 64];
static u8  tx_frame[ETH_FRAME_MAX + 64];
static u16 ip_ident;
static u64 last_link_check;
static bool notified;

/* =================================================================== utilidades */
u32 inet_csum_add(u32 sum, const void *data, usize len) {
    const u8 *p = data;
    while (len > 1) { sum += (u32)((p[0] << 8) | p[1]); p += 2; len -= 2; }
    if (len) sum += (u32)(p[0] << 8);
    return sum;
}

u16 inet_csum_fold(u32 sum) {
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (u16)~sum;
}

u32 inet_pseudo_sum(u32 src, u32 dst, u8 proto, u16 len) {
    u32 s = 0;
    s += src >> 16; s += src & 0xFFFF;
    s += dst >> 16; s += dst & 0xFFFF;
    s += proto; s += len;
    return s;
}

void ip_to_str(u32 ip, char *b) {
    snprintf(b, 16, "%u.%u.%u.%u", ip >> 24, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255);
}

bool str_to_ip(const char *s, u32 *ip) {
    if (!s) return false;
    u32 v = 0;
    for (int part = 0; part < 4; part++) {
        if (!isdigit(*s)) return false;
        u32 n = 0;
        int digits = 0;
        while (isdigit(*s)) { n = n * 10 + (u32)(*s++ - '0'); if (++digits > 3 || n > 255) return false; }
        v = (v << 8) | n;
        if (part < 3) { if (*s != '.') return false; s++; }
    }
    if (*s) return false;
    if (ip) *ip = v;
    return true;
}

u16 net_ephemeral_port(void) {
    static u16 next;
    if (!next) next = (u16)(49152 + rand32() % 16000);
    u16 p = next++;
    if (next < 49152) next = 49152;
    return p;
}

/* =================================================================== API de estado */
net_info_t *net_info(void) { return &g_net; }
bool net_link_up(void) { return g_net.link; }
bool net_configured(void) { return g_net.configured; }
const char *net_device_name(void) { return g_netdev ? g_netdev->name : "Nenhuma placa de rede"; }

void net_status_string(char *b, usize n) {
    if (!g_netdev) { snprintf(b, n, "Sem placa de rede"); return; }
    if (!g_net.link) { snprintf(b, n, "Cabo desconectado"); return; }
    if (!g_net.configured) { snprintf(b, n, "Obtendo endereço (DHCP)…"); return; }
    char ip[16];
    ip_to_str(g_net.ip, ip);
    snprintf(b, n, "Conectado — %s", ip);
}

/* várias interfaces podem ser registradas (cabo, USB, Wi-Fi); uma é a ativa.
 * Preferência: a primeira registrada (com fio) que tiver link; senão qualquer outra com link. */
#define MAX_NETDEV 4
static netdev_t *devs[MAX_NETDEV];
static int ndevs;
static bool stack_ready;

static bool dev_link(netdev_t *d) { return d->link_up ? d->link_up() : true; }

static void activate(netdev_t *d) {
    if (d == g_netdev) return;
    bool had = g_netdev != NULL;
    g_netdev = d;
    memcpy(g_net.mac, d->mac, 6);
    memset(g_net.driver, 0, sizeof g_net.driver);
    strncpy(g_net.driver, d->name ? d->name : "?", sizeof g_net.driver - 1);
    g_net.link = dev_link(d);
    if (had) {
        kprintf("net: interface ativa agora é %s\n", g_net.driver);
        g_net.configured = false;
        g_net.ip = 0;
        notified = false;
    }
    if (!stack_ready) { stack_ready = true; dns_init(); }
    dhcp_init();     /* nova interface: nova concessão (udp_bind é idempotente) */
}

void net_set_active(netdev_t *d) { if (d) activate(d); }
int net_device_count(void) { return ndevs; }
netdev_t *net_device_at(int i) { return i >= 0 && i < ndevs ? devs[i] : NULL; }

void net_register_device(netdev_t *d) {
    if (!d || ndevs >= MAX_NETDEV) return;
    for (int i = 0; i < ndevs; i++) if (devs[i] == d) return;
    devs[ndevs++] = d;
    kprintf("net: interface %s registrada\n", d->name ? d->name : "?");
    if (!g_netdev) { activate(d); return; }
    if (!g_net.link && dev_link(d)) activate(d);   /* a ativa está sem link: usa a nova */
}

static void choose_interface(void) {
    netdev_t *best = NULL;
    for (int i = 0; i < ndevs && !best; i++) if (dev_link(devs[i])) best = devs[i];
    if (best && best != g_netdev) activate(best);
}

void net_on_configured(void) {
    char ip[16], mk[16], gw[16], dn[16];
    ip_to_str(g_net.ip, ip); ip_to_str(g_net.mask, mk); ip_to_str(g_net.gw, gw); ip_to_str(g_net.dns, dn);
    kprintf("net: configurado por DHCP: ip %s máscara %s gateway %s dns %s (%llu ms)\n", ip, mk, gw, dn, timer_ticks());
    if (!notified) {
        notified = true;
        char body[64];
        snprintf(body, sizeof body, "Endereço IP %s", ip);
        notify("Rede conectada", body);
    }
}

/* =================================================================== Ethernet */
static bool eth_send(const u8 *dst_mac, u16 type, const void *payload, u16 len) {
    if (!g_netdev || len > ETH_MTU) return false;
    memcpy(tx_frame, dst_mac, 6);
    memcpy(tx_frame + 6, g_net.mac, 6);
    wr16(tx_frame + 12, type);
    memcpy(tx_frame + ETH_HLEN, payload, len);
    u16 flen = ETH_HLEN + len;
    if (flen < 60) { memset(tx_frame + flen, 0, 60 - flen); flen = 60; }
    if (!g_netdev->send(tx_frame, flen)) return false;
    g_net.tx_packets++;
    g_net.tx_bytes += flen;
    return true;
}

/* =================================================================== ARP */
#define ARP_ENTRIES   32
#define ARP_TTL_MS    (10 * 60 * 1000)
#define ARP_RETRY_MS  1000
#define ARP_RETRIES   3
#define ARP_QUEUE     8

typedef struct { u32 ip; u8 mac[6]; u64 time; bool valid, resolving; u8 tries; u64 last_req; } arp_entry_t;
typedef struct { bool used; u32 hop; u16 len; u8 data[ETH_MTU]; } arp_pending_t;

static arp_entry_t arp_tab[ARP_ENTRIES];
static arp_pending_t *arp_q;   /* ARP_QUEUE pacotes IP aguardando resolução */

static arp_entry_t *arp_find(u32 ip) {
    for (int i = 0; i < ARP_ENTRIES; i++) if ((arp_tab[i].valid || arp_tab[i].resolving) && arp_tab[i].ip == ip) return &arp_tab[i];
    return NULL;
}

static arp_entry_t *arp_alloc(u32 ip) {
    arp_entry_t *e = arp_find(ip);
    if (e) return e;
    arp_entry_t *old = &arp_tab[0];
    for (int i = 0; i < ARP_ENTRIES; i++) {
        if (!arp_tab[i].valid && !arp_tab[i].resolving) { old = &arp_tab[i]; break; }
        if (arp_tab[i].time < old->time) old = &arp_tab[i];
    }
    memset(old, 0, sizeof *old);
    old->ip = ip;
    return old;
}

static void arp_send(u16 op, const u8 *tmac, u32 tip, const u8 *eth_dst) {
    u8 p[28];
    wr16(p, 1); wr16(p + 2, ETH_P_IP); p[4] = 6; p[5] = 4; wr16(p + 6, op);
    memcpy(p + 8, g_net.mac, 6); wr32(p + 14, g_net.ip);
    memcpy(p + 18, tmac, 6); wr32(p + 24, tip);
    eth_send(eth_dst, ETH_P_ARP, p, sizeof p);
}

static void arp_request(u32 ip) {
    static const u8 zero[6];
    arp_send(1, zero, ip, MAC_BCAST);
}

static void arp_flush(u32 ip, const u8 *mac) {
    if (!arp_q) return;
    for (int i = 0; i < ARP_QUEUE; i++)
        if (arp_q[i].used && arp_q[i].hop == ip) {
            eth_send(mac, ETH_P_IP, arp_q[i].data, arp_q[i].len);
            arp_q[i].used = false;
        }
}

static void arp_learn(u32 ip, const u8 *mac) {
    if (!ip || (mac[0] & 1)) return;
    arp_entry_t *e = arp_alloc(ip);
    memcpy(e->mac, mac, 6);
    e->valid = true;
    e->resolving = false;
    e->time = timer_ticks();
    arp_flush(ip, mac);
}

static void arp_input(const u8 *p, u16 len) {
    if (len < 28 || rd16(p) != 1 || rd16(p + 2) != ETH_P_IP || p[4] != 6 || p[5] != 4) return;
    u16 op = rd16(p + 6);
    u32 sip = rd32(p + 14), tip = rd32(p + 24);
    const u8 *smac = p + 8;
    bool for_us = g_net.ip && tip == g_net.ip;
    /* atualiza entradas existentes; cria uma nova apenas se o pacote é para nós */
    if (arp_find(sip) || for_us) arp_learn(sip, smac);
    if (op == 1 && for_us) arp_send(2, smac, sip, smac);
}

static void arp_tick(u64 now) {
    for (int i = 0; i < ARP_ENTRIES; i++) {
        arp_entry_t *e = &arp_tab[i];
        if (e->valid && now - e->time > ARP_TTL_MS) e->valid = false;
        if (!e->resolving || now - e->last_req < ARP_RETRY_MS) continue;
        if (e->tries >= ARP_RETRIES) {
            char s[16];
            ip_to_str(e->ip, s);
            kprintf("arp: sem resposta de %s\n", s);
            e->resolving = false;
            if (arp_q) for (int k = 0; k < ARP_QUEUE; k++) if (arp_q[k].used && arp_q[k].hop == e->ip) arp_q[k].used = false;
            continue;
        }
        e->tries++;
        e->last_req = now;
        arp_request(e->ip);
    }
}

/* =================================================================== IPv4 */
static bool ip_is_local_bcast(u32 dst) {
    return dst == IP_BROADCAST || (g_net.mask && g_net.mask != IP_BROADCAST && (dst | g_net.mask) == IP_BROADCAST && (dst & g_net.mask) == (g_net.ip & g_net.mask));
}

bool ipv4_send(u32 dst, u8 proto, const void *payload, u16 len) {
    if (!g_netdev || len > ETH_MTU - IP_HLEN) return false;
    static u8 pkt[ETH_MTU];
    u8 *h = pkt;
    h[0] = 0x45; h[1] = 0;
    wr16(h + 2, IP_HLEN + len);
    wr16(h + 4, ip_ident++);
    wr16(h + 6, 0x4000);                    /* DF */
    h[8] = 64; h[9] = proto;
    wr16(h + 10, 0);
    wr32(h + 12, g_net.ip);
    wr32(h + 16, dst);
    wr16(h + 10, inet_csum_fold(inet_csum_add(0, h, IP_HLEN)));
    memcpy(pkt + IP_HLEN, payload, len);
    u16 total = IP_HLEN + len;

    if (ip_is_local_bcast(dst) || !g_net.configured) return eth_send(MAC_BCAST, ETH_P_IP, pkt, total);
    if ((dst >> 24) == 127 || dst == g_net.ip) return false;

    u32 hop = (g_net.mask && (dst & g_net.mask) == (g_net.ip & g_net.mask)) ? dst : g_net.gw;
    if (!hop) return false;
    arp_entry_t *e = arp_find(hop);
    if (e && e->valid) return eth_send(e->mac, ETH_P_IP, pkt, total);

    /* enfileira até o ARP responder */
    if (!arp_q) arp_q = kcalloc(ARP_QUEUE, sizeof(arp_pending_t));
    if (!arp_q) return false;
    int slot = -1;
    for (int i = 0; i < ARP_QUEUE; i++) if (!arp_q[i].used) { slot = i; break; }
    if (slot < 0) slot = (int)(rand32() % ARP_QUEUE);
    arp_q[slot].used = true;
    arp_q[slot].hop = hop;
    arp_q[slot].len = total;
    memcpy(arp_q[slot].data, pkt, total);
    if (!e) {
        e = arp_alloc(hop);
        e->resolving = true;
        e->tries = 1;
        e->time = e->last_req = timer_ticks();
        arp_request(hop);
    }
    return true;
}

static void icmp_input(u32 src, const u8 *p, u16 len);

static void ipv4_input(const u8 *p, u16 len) {
    if (len < IP_HLEN || (p[0] >> 4) != 4) return;
    u16 ihl = (u16)((p[0] & 15) * 4);
    u16 tot = rd16(p + 2);
    if (ihl < IP_HLEN || tot < ihl || tot > len) return;
    if (inet_csum_fold(inet_csum_add(0, p, ihl)) != 0) return;
    u16 frag = rd16(p + 6);
    if ((frag & 0x2000) || (frag & 0x1FFF)) return;   /* fragmentos não são suportados */
    u32 src = rd32(p + 12), dst = rd32(p + 16);
    bool ours = dst == IP_BROADCAST || ip_is_local_bcast(dst) || (g_net.ip && dst == g_net.ip) || !g_net.configured;
    if (!ours) return;
    const u8 *pl = p + ihl;
    u16 plen = tot - ihl;
    switch (p[9]) {
    case IPPROTO_ICMP: icmp_input(src, pl, plen); break;
    case IPPROTO_UDP:  udp_input(src, dst, pl, plen); break;
    case IPPROTO_TCP:  if (g_net.configured && dst == g_net.ip) tcp_input(src, dst, pl, plen); break;
    }
}

/* =================================================================== ICMP */
#define PING_ID     0x6D4F
#define PING_SLOTS  16
#define PING_DATA   56

typedef struct { bool used, replied; u16 seq; u64 sent, rtt; } ping_slot_t;
static ping_slot_t pings[PING_SLOTS];

bool icmp_ping_send(u32 ip, u16 seq) {
    if (!g_netdev || !g_net.configured) return false;
    u8 p[8 + PING_DATA];
    p[0] = 8; p[1] = 0; wr16(p + 2, 0);
    wr16(p + 4, PING_ID); wr16(p + 6, seq);
    for (int i = 0; i < PING_DATA; i++) p[8 + i] = (u8)(0x20 + i);
    wr16(p + 2, inet_csum_fold(inet_csum_add(0, p, sizeof p)));
    ping_slot_t *s = &pings[seq % PING_SLOTS];
    s->used = true; s->replied = false; s->seq = seq; s->sent = timer_ticks(); s->rtt = 0;
    return ipv4_send(ip, IPPROTO_ICMP, p, sizeof p);
}

bool icmp_ping_reply(u16 seq, u64 *rtt_ms) {
    ping_slot_t *s = &pings[seq % PING_SLOTS];
    if (!s->used || s->seq != seq || !s->replied) return false;
    if (rtt_ms) *rtt_ms = s->rtt;
    s->used = false;
    return true;
}

static void icmp_input(u32 src, const u8 *p, u16 len) {
    if (len < 8 || inet_csum_fold(inet_csum_add(0, p, len)) != 0) return;
    if (p[0] == 8 && g_net.configured) {           /* echo request: responde */
        static u8 r[ETH_MTU];
        if (len > ETH_MTU - IP_HLEN) return;
        memcpy(r, p, len);
        r[0] = 0; wr16(r + 2, 0);
        wr16(r + 2, inet_csum_fold(inet_csum_add(0, r, len)));
        ipv4_send(src, IPPROTO_ICMP, r, len);
    } else if (p[0] == 0 && rd16(p + 4) == PING_ID) {  /* echo reply */
        u16 seq = rd16(p + 6);
        ping_slot_t *s = &pings[seq % PING_SLOTS];
        if (s->used && s->seq == seq && !s->replied) { s->replied = true; s->rtt = timer_ticks() - s->sent; }
    }
}

/* =================================================================== UDP */
#define UDP_BINDS 16
static struct { u16 port; udp_handler_t h; } udp_binds[UDP_BINDS];

bool udp_bind(u16 port, udp_handler_t h) {
    int free_slot = -1;
    for (int i = 0; i < UDP_BINDS; i++) {
        if (udp_binds[i].h && udp_binds[i].port == port) { udp_binds[i].h = h; if (!h) udp_binds[i].port = 0; return true; }
        if (!udp_binds[i].h && free_slot < 0) free_slot = i;
    }
    if (!h) return true;
    if (free_slot < 0) return false;
    udp_binds[free_slot].port = port;
    udp_binds[free_slot].h = h;
    return true;
}

bool udp_send(u32 dst_ip, u16 src_port, u16 dst_port, const void *data, u16 len) {
    if (!g_netdev || len > ETH_MTU - IP_HLEN - 8) return false;
    static u8 p[ETH_MTU];
    u16 ulen = 8 + len;
    wr16(p, src_port); wr16(p + 2, dst_port); wr16(p + 4, ulen); wr16(p + 6, 0);
    memcpy(p + 8, data, len);
    u32 s = inet_pseudo_sum(g_net.ip, dst_ip, IPPROTO_UDP, ulen);
    u16 c = inet_csum_fold(inet_csum_add(s, p, ulen));
    wr16(p + 6, c ? c : 0xFFFF);
    return ipv4_send(dst_ip, IPPROTO_UDP, p, ulen);
}

void udp_input(u32 src, u32 dst, const u8 *p, u16 len) {
    if (len < 8) return;
    u16 ulen = rd16(p + 4);
    if (ulen < 8 || ulen > len) return;
    if (rd16(p + 6)) {
        u32 s = inet_pseudo_sum(src, dst, IPPROTO_UDP, ulen);
        if (inet_csum_fold(inet_csum_add(s, p, ulen)) != 0) return;
    }
    u16 sport = rd16(p), dport = rd16(p + 2);
    for (int i = 0; i < UDP_BINDS; i++)
        if (udp_binds[i].h && udp_binds[i].port == dport) { udp_binds[i].h(src, sport, p + 8, ulen - 8); return; }
}

/* =================================================================== laço */
static void eth_input(const u8 *f, u16 len) {
    if (len < ETH_HLEN) return;
    const u8 *dst = f;
    if (memcmp(dst, g_net.mac, 6) && memcmp(dst, MAC_BCAST, 6)) return;
    u16 type = rd16(f + 12);
    if (type == ETH_P_IP) ipv4_input(f + ETH_HLEN, len - ETH_HLEN);
    else if (type == ETH_P_ARP) arp_input(f + ETH_HLEN, len - ETH_HLEN);
}

void net_poll(void) {
    if (!g_netdev) return;
    for (int i = 0; i < 64; i++) {
        int n = g_netdev->recv(rx_frame, sizeof rx_frame);
        if (n <= 0) break;
        g_net.rx_packets++;
        g_net.rx_bytes += (u64)n;
        eth_input(rx_frame, (u16)n);
    }
    u64 now = timer_ticks();
    if (now - last_link_check >= 500) {
        last_link_check = now;
        if (ndevs > 1) choose_interface();
        bool up = g_netdev->link_up ? g_netdev->link_up() : true;
        if (up != g_net.link) {
            g_net.link = up;
            kprintf("net: link %s\n", up ? "ativo" : "inativo");
            if (up) net_dhcp_renew();
        }
        arp_tick(now);
    }
    dhcp_tick(now);
    dns_tick(now);
    tcp_tick(now);}

void net_init(void) {
    if (!e1000_probe()) rtl8139_probe();
    if (!g_netdev) kprintf("net: nenhuma placa de rede com fio; aguardando outra interface (Wi-Fi/USB)\n");
}
