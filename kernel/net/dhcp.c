/* mOS — cliente DHCP (RFC 2131): DISCOVER → OFFER → REQUEST → ACK, renovação em T1/T2 */
#include "netpriv.h"

enum { D_IDLE, D_SELECTING, D_REQUESTING, D_BOUND, D_RENEWING, D_REBINDING };
enum { DHCPDISCOVER = 1, DHCPOFFER, DHCPREQUEST, DHCPDECLINE, DHCPACK, DHCPNAK, DHCPRELEASE };

static struct {
    int state;
    u32 xid;
    u32 offered_ip, server_id;
    u32 lease_s, t1_s, t2_s;
    u64 bound_at;          /* ms */
    u64 next_tx;           /* próximo (re)envio */
    u32 backoff;           /* ms */
    int tries;
} D;

static const u8 MAGIC[4] = {99, 130, 83, 99};

static u8 *opt_put(u8 *o, u8 code, u8 len, const void *data) {
    *o++ = code; *o++ = len;
    memcpy(o, data, len);
    return o + len;
}

static void dhcp_send(u8 type) {
    static u8 m[548];
    memset(m, 0, sizeof m);
    m[0] = 1; m[1] = 1; m[2] = 6; m[3] = 0;
    wr32(m + 4, D.xid);
    wr16(m + 8, (u16)MIN(0xFFFF, D.tries));
    bool renewing = D.state == D_RENEWING || D.state == D_REBINDING;
    wr16(m + 10, renewing ? 0 : 0x8000);             /* pede resposta em broadcast */
    if (renewing) wr32(m + 12, g_net.ip);           /* ciaddr */
    memcpy(m + 28, g_net.mac, 6);
    memcpy(m + 236, MAGIC, 4);
    u8 *o = m + 240;
    o = opt_put(o, 53, 1, &type);
    u8 cid[7] = {1};
    memcpy(cid + 1, g_net.mac, 6);
    o = opt_put(o, 61, 7, cid);
    if (type == DHCPREQUEST && !renewing) {
        u8 b[4];
        wr32(b, D.offered_ip); o = opt_put(o, 50, 4, b);
        wr32(b, D.server_id);  o = opt_put(o, 54, 4, b);
    }
    static const u8 prl[] = {1, 3, 6, 15, 26, 51, 54, 58, 59};
    o = opt_put(o, 55, sizeof prl, prl);
    u8 mss[2]; wr16(mss, 1500);
    o = opt_put(o, 57, 2, mss);
    o = opt_put(o, 12, 3, "mOS");
    *o++ = 255;
    u16 len = (u16)MAX(300, o - m);
    u32 dst = (D.state == D_RENEWING && D.server_id) ? D.server_id : IP_BROADCAST;
    udp_send(dst, DHCP_CLIENT_PORT, DHCP_SERVER_PORT, m, len);
}

static void start_discover(void) {
    D.state = D_SELECTING;
    D.xid = rand32();
    D.tries = 0;
    D.backoff = 2000;
    D.next_tx = 0;    /* envia no próximo tick */
}

static void schedule_retry(u64 now) {
    D.tries++;
    D.next_tx = now + D.backoff + (rand32() % 500);
    if (D.backoff < 16000) D.backoff *= 2;
}

static void dhcp_rx(u32 src, u16 sport, const u8 *m, u16 len) {
    if (len < 240 || m[0] != 2 || rd32(m + 4) != D.xid || memcmp(m + 236, MAGIC, 4)) return;
    if (memcmp(m + 28, g_net.mac, 6)) return;
    u8 type = 0;
    u32 mask = 0, router = 0, dns = 0, sid = 0, lease = 0, t1 = 0, t2 = 0;
    for (u16 i = 240; i < len;) {
        u8 c = m[i++];
        if (c == 0) continue;
        if (c == 255 || i >= len) break;
        u8 l = m[i++];
        if (i + l > len) break;
        const u8 *v = m + i;
        switch (c) {
        case 53: if (l >= 1) type = v[0]; break;
        case 1:  if (l >= 4) mask = rd32(v); break;
        case 3:  if (l >= 4) router = rd32(v); break;
        case 6:  if (l >= 4) dns = rd32(v); break;
        case 51: if (l >= 4) lease = rd32(v); break;
        case 54: if (l >= 4) sid = rd32(v); break;
        case 58: if (l >= 4) t1 = rd32(v); break;
        case 59: if (l >= 4) t2 = rd32(v); break;
        }
        i += l;
    }
    u32 yiaddr = rd32(m + 16);
    u64 now = timer_ticks();

    if (type == DHCPOFFER && D.state == D_SELECTING) {
        if (!yiaddr) return;
        D.offered_ip = yiaddr;
        D.server_id = sid ? sid : src;
        D.state = D_REQUESTING;
        D.tries = 0;
        D.backoff = 2000;
        dhcp_send(DHCPREQUEST);
        schedule_retry(now);
    } else if (type == DHCPACK && (D.state == D_REQUESTING || D.state == D_RENEWING || D.state == D_REBINDING)) {
        if (!yiaddr) return;
        bool was = g_net.configured;
        u32 old_ip = g_net.ip;
        g_net.ip = yiaddr;
        g_net.mask = mask ? mask : 0xFFFFFF00;
        g_net.gw = router;
        g_net.dns = dns ? dns : router;
        if (sid) D.server_id = sid;
        D.lease_s = lease ? lease : 3600;
        if (D.lease_s == 0xFFFFFFFF) D.lease_s = 0x7FFFFFFF / 1000;   /* infinito */
        D.t1_s = t1 ? t1 : D.lease_s / 2;
        D.t2_s = t2 ? t2 : D.lease_s / 8 * 7;
        if (D.t1_s > D.lease_s) D.t1_s = D.lease_s / 2;
        if (D.t2_s > D.lease_s || D.t2_s < D.t1_s) D.t2_s = D.lease_s / 8 * 7;
        D.bound_at = now;
        D.state = D_BOUND;
        g_net.configured = true;
        if (!was || old_ip != yiaddr) net_on_configured();
        else kprintf("net: concessão DHCP renovada (%u s)\n", D.lease_s);
    } else if (type == DHCPNAK && D.state != D_SELECTING && D.state != D_BOUND) {
        kprintf("net: DHCP NAK, reiniciando\n");
        g_net.configured = false;
        g_net.ip = 0;
        start_discover();
    }
}

void dhcp_tick(u64 now) {
    switch (D.state) {
    case D_SELECTING:
    case D_REQUESTING:
        if (now < D.next_tx || !g_net.link) return;
        if (D.state == D_REQUESTING && D.tries >= 4) { start_discover(); return; }
        dhcp_send(D.state == D_SELECTING ? DHCPDISCOVER : DHCPREQUEST);
        schedule_retry(now);
        return;
    case D_BOUND: {
        u64 el = now - D.bound_at;
        if (el >= (u64)D.t1_s * 1000) {
            D.state = D_RENEWING;
            D.xid = rand32();
            D.tries = 0;
            D.backoff = 4000;
            D.next_tx = now;
        }
        return;
    }
    case D_RENEWING:
    case D_REBINDING: {
        u64 el = now - D.bound_at;
        if (el >= (u64)D.lease_s * 1000) {
            kprintf("net: concessão DHCP expirou\n");
            g_net.configured = false;
            g_net.ip = 0;
            start_discover();
            return;
        }
        if (D.state == D_RENEWING && el >= (u64)D.t2_s * 1000) D.state = D_REBINDING;
        if (now < D.next_tx) return;
        dhcp_send(DHCPREQUEST);
        schedule_retry(now);
        return;
    }
    default:
        return;
    }
}

void net_dhcp_renew(void) {
    if (!g_netdev) return;
    if (D.state == D_BOUND || D.state == D_RENEWING || D.state == D_REBINDING) {
        D.state = D_RENEWING;
        D.xid = rand32();
        D.tries = 0;
        D.backoff = 2000;
        D.next_tx = 0;
    } else if (D.state != D_SELECTING && D.state != D_REQUESTING) {
        start_discover();
    } else {
        D.next_tx = 0;   /* reenvia já */
    }
}

void dhcp_init(void) {
    memset(&D, 0, sizeof D);
    udp_bind(DHCP_CLIENT_PORT, dhcp_rx);
    start_discover();
    kprintf("net: DHCP iniciado\n");
}
