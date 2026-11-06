/*
 * mOS — rádio Wi-Fi simulado, para testar a pilha sem hardware (o QEMU não emula Wi-Fi).
 *
 * Só é ativado se não houver placa real e existir /Sistema/Biblioteca/wifi-simulado (build.ps1 -Tests).
 * Simula o "ar" com três redes:
 *   canal 1  "Cafe Aberto"   aberta (só beacons)
 *   canal 6  "mOS-Teste"     WPA2-PSK, senha "senha-do-teste": ponto de acesso completo, com o lado
 *                            autenticador do handshake de 4 vias, CCMP e um mini-roteador
 *                            (ARP, DHCP 192.168.77.10/24, ping para 192.168.77.1)
 *   canal 11 "Vizinho WPA3"  só SAE (para a lista mostrar redes não suportadas)
 * O lado do ponto de acesso é escrito à parte da estação e usa só as primitivas de wpa.c (testadas
 * contra vetores padrão), então o teste confere de verdade o protocolo da estação.
 */
#include "wifi.h"
#include "vfs.h"

#define AIRQ 64
static const u8 AP_MAC[6] = {0x02, 0x00, 0x00, 0x77, 0x00, 0x01};
static const u8 CAFE_MAC[6] = {0x02, 0x00, 0x00, 0x11, 0x00, 0x01};
static const u8 WPA3_MAC[6] = {0x02, 0x00, 0x00, 0x33, 0x00, 0x01};
static const char AP_SSID[] = "mOS-Teste", AP_PASS[] = "senha-do-teste";

static wifi_radio_t sim;
static int channel = 1;
static u8 *air[AIRQ];
static u16 air_len[AIRQ];
static int air_h, air_t;
static u64 next_beacon;
static u16 seq;

/* estado do ponto de acesso para a (única) estação */
static struct {
    bool auth, assoc;
    u8 sta[6];
    u8 pmk[32], anonce[32], ptk[48], gtk[16];
    u64 replay, pn_tx, pn_rx;
    int hs;                 /* 0 nada, 1 msg1 enviada, 3 msg3 enviada, 4 pronto */
    u32 sta_ip;
} ap;

static void put16le(u8 *p, u16 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); }
static void put16be(u8 *p, u16 v) { p[0] = (u8)(v >> 8); p[1] = (u8)v; }
static void put32be(u8 *p, u32 v) { p[0] = (u8)(v >> 24); p[1] = (u8)(v >> 16); p[2] = (u8)(v >> 8); p[3] = (u8)v; }
static u16 get16le(const u8 *p) { return (u16)(p[0] | p[1] << 8); }
static u16 get16be(const u8 *p) { return (u16)(p[0] << 8 | p[1]); }

static void to_air(const u8 *f, u16 len) {
    int nx = (air_h + 1) % AIRQ;
    if (nx == air_t) return;
    u8 *c = kmalloc(len);
    if (!c) return;
    memcpy(c, f, len);
    air[air_h] = c;
    air_len[air_h] = len;
    air_h = nx;
}

static int mk_hdr(u8 *f, u8 fc0, u8 fc1, const u8 *a1, const u8 *a2, const u8 *a3) {
    f[0] = fc0; f[1] = fc1; f[2] = f[3] = 0;
    memcpy(f + 4, a1, 6); memcpy(f + 10, a2, 6); memcpy(f + 16, a3, 6);
    put16le(f + 22, (u16)(seq++ << 4));
    return 24;
}

/* beacon / probe response */
static void send_bss(u8 subtype, const u8 *dst, const u8 *bssid, const char *ssid, int ch, int kind) {
    u8 f[160];
    int n = mk_hdr(f, (u8)(subtype << 4), 0, dst, bssid, bssid);
    memset(f + n, 0, 8); n += 8;                                  /* timestamp */
    put16le(f + n, 100); n += 2;                                  /* intervalo */
    put16le(f + n, kind ? 0x0411 : 0x0401); n += 2;               /* ESS (+ privacidade) */
    usize sl = strlen(ssid);
    f[n++] = 0; f[n++] = (u8)sl; memcpy(f + n, ssid, sl); n += (int)sl;
    static const u8 rates[] = {1, 4, 0x82, 0x84, 0x8B, 0x96};
    memcpy(f + n, rates, sizeof rates); n += sizeof rates;
    f[n++] = 3; f[n++] = 1; f[n++] = (u8)ch;
    if (kind) {                                                   /* RSN: CCMP + PSK (1) ou SAE (2) */
        u8 rsn[] = {48, 20, 1, 0, 0x00, 0x0F, 0xAC, 4, 1, 0, 0x00, 0x0F, 0xAC, 4, 1, 0, 0x00, 0x0F, 0xAC, (u8)(kind == 1 ? 2 : 8), 0x0C, 0};
        memcpy(f + n, rsn, sizeof rsn); n += sizeof rsn;
    }
    to_air(f, (u16)n);
}

/* ---- quadro de dados do AP para a estação (cifrado se o handshake terminou) */
static void ap_data(const u8 *dst, const u8 *src, u16 ethertype, const u8 *pl, u16 plen) {
    u8 f[1600], e[1640];
    if (plen > 1500) return;
    int n = mk_hdr(f, 0x88, 0x02, dst, AP_MAC, src);              /* QoS data, FromDS */
    f[n++] = 0; f[n++] = 0;                                       /* QoS control: TID 0 */
    static const u8 snap[6] = {0xAA, 0xAA, 0x03, 0, 0, 0};
    memcpy(f + n, snap, 6);
    put16be(f + n + 6, ethertype);
    memcpy(f + n + 8, pl, plen);
    u16 len = (u16)(n + 8 + plen);
    if (ap.hs == 4 && ethertype != 0x888E) {
        bool group = dst[0] & 1;
        to_air(e, ccmp_encrypt_frame(group ? ap.gtk : ap.ptk + 32, ++ap.pn_tx, group ? 1 : 0, f, len, e));
    } else to_air(f, len);
}

/* ---- autenticador WPA2 */
static void ap_eapol(u16 info, const u8 *nonce, const u8 *kdata, u16 klen) {
    u8 b[300];
    memset(b, 0, sizeof b);
    u16 body = (u16)(95 + klen);
    b[0] = 2; b[1] = 3; put16be(b + 2, body);
    u8 *k = b + 4;
    k[0] = 2;
    put16be(k + 1, info);
    put16be(k + 3, 16);                                           /* tamanho da chave par (CCMP) */
    ap.replay++;
    for (int i = 0; i < 8; i++) k[5 + i] = (u8)(ap.replay >> (56 - 8 * i));
    if (nonce) memcpy(k + 13, nonce, 32);
    put16be(k + 93, klen);
    if (klen) memcpy(k + 95, kdata, klen);
    if (info & 0x0100) wpa_eapol_mic(ap.ptk, b, 4u + body, k + 77);
    ap_data(ap.sta, AP_MAC, 0x888E, b, (u16)(4 + body));
}

static void ap_msg1(void) {
    for (int i = 0; i < 32; i++) ap.anonce[i] = (u8)(rand32() >> 3);
    ap.hs = 1;
    ap_eapol(0x008A, ap.anonce, NULL, 0);                         /* ver2 | par | ack */
}

static void ap_handle_eapol(const u8 *p, int len) {
    if (len < 99 || p[4] != 2) return;
    const u8 *k = p + 4;
    u16 info = get16be(k + 1);
    if (!(info & 0x0100)) return;
    if (ap.hs == 1 && (info & 0x0008)) {                          /* mensagem 2: deriva a PTK e confere o MIC */
        wpa_ptk(ap.pmk, AP_MAC, ap.sta, ap.anonce, k + 13, ap.ptk);
    }
    u8 c[400], mic[16];
    if (len > (int)sizeof c) return;
    memcpy(c, p, len);
    memset(c + 4 + 77, 0, 16);
    wpa_eapol_mic(ap.ptk, c, (usize)(4 + get16be(p + 2)), mic);
    if (memcmp(mic, k + 77, 16)) {
        kprintf("wifi-sim: MIC da estação não confere (senha errada) — desautenticando\n");
        u8 d[32];
        int n = mk_hdr(d, 0xC0, 0, ap.sta, AP_MAC, AP_MAC);
        put16le(d + n, 15);                                       /* 4-way handshake timeout */
        to_air(d, (u16)(n + 2));
        ap.hs = 0; ap.assoc = ap.auth = false;
        return;
    }
    if (ap.hs == 1) {                                             /* mensagem 3: RSN IE + GTK cifrada */
        u8 kd[64], wrapped[72];
        int n = 0;
        static const u8 rsn[] = {48, 20, 1, 0, 0x00, 0x0F, 0xAC, 4, 1, 0, 0x00, 0x0F, 0xAC, 4, 1, 0, 0x00, 0x0F, 0xAC, 2, 0x0C, 0};
        memcpy(kd, rsn, sizeof rsn); n = sizeof rsn;
        kd[n++] = 0xDD; kd[n++] = 22; kd[n++] = 0x00; kd[n++] = 0x0F; kd[n++] = 0xAC; kd[n++] = 1;
        kd[n++] = 1; kd[n++] = 0;                                 /* keyid 1 */
        memcpy(kd + n, ap.gtk, 16); n += 16;
        if (n % 8) { kd[n++] = 0xDD; while (n % 8) kd[n++] = 0; }   /* preenchimento do key wrap (802.11i) */
        wpa_key_wrap(ap.ptk + 16, kd, (usize)n, wrapped);
        ap.hs = 3;
        ap_eapol(0x13CA, ap.anonce, wrapped, (u16)(n + 8));       /* ver2|par|install|ack|mic|secure|enc */
    } else if (ap.hs == 3 && (info & 0x0008)) {                   /* mensagem 4 */
        ap.hs = 4;
        ap.pn_rx = 0;
        kprintf("wifi-sim: handshake concluído, chaves instaladas\n");
    }
}

/* ---- mini-roteador: ARP, DHCP, ICMP */
static u16 csum(const u8 *p, int n) {
    u32 s = 0;
    for (int i = 0; i + 1 < n; i += 2) s += (u32)(p[i] << 8 | p[i + 1]);
    if (n & 1) s += (u32)p[n - 1] << 8;
    while (s >> 16) s = (s & 0xFFFF) + (s >> 16);
    return (u16)~s;
}

static void ip_out(const u8 *dst_mac, u32 dst_ip, u8 proto, const u8 *pl, int plen) {
    u8 b[1500];
    if (plen > 1400) return;
    memset(b, 0, 20);
    b[0] = 0x45; put16be(b + 2, (u16)(20 + plen)); b[8] = 64; b[9] = proto;
    put32be(b + 12, 0xC0A84D01); put32be(b + 16, dst_ip);       /* 192.168.77.1 -> */
    put16be(b + 10, csum(b, 20));
    memcpy(b + 20, pl, plen);
    ap_data(dst_mac, AP_MAC, 0x0800, b, (u16)(20 + plen));
}

static void dhcp_reply(const u8 *req, int len) {
    if (len < 240 || req[0] != 1) return;
    int type = 0;
    for (int i = 240; i + 2 <= len && req[i] != 255; i += req[i] ? 2 + req[i + 1] : 1)
        if (req[i] == 53) type = req[i + 2];
    if (type != 1 && type != 3) return;                           /* DISCOVER / REQUEST */
    u8 u[8 + 300];
    u8 *d = u + 8;
    memset(u, 0, sizeof u);
    d[0] = 2; d[1] = 1; d[2] = 6;
    memcpy(d + 4, req + 4, 4);                                    /* xid */
    put32be(d + 16, 0xC0A84D0A);                                  /* yiaddr 192.168.77.10 */
    put32be(d + 20, 0xC0A84D01);
    memcpy(d + 28, req + 28, 16);                                 /* chaddr */
    d[236] = 99; d[237] = 130; d[238] = 83; d[239] = 99;
    int o = 240;
    d[o++] = 53; d[o++] = 1; d[o++] = type == 1 ? 2 : 5;          /* OFFER / ACK */
    d[o++] = 54; d[o++] = 4; put32be(d + o, 0xC0A84D01); o += 4;
    d[o++] = 51; d[o++] = 4; put32be(d + o, 3600); o += 4;
    d[o++] = 1; d[o++] = 4; put32be(d + o, 0xFFFFFF00); o += 4;
    d[o++] = 3; d[o++] = 4; put32be(d + o, 0xC0A84D01); o += 4;
    d[o++] = 6; d[o++] = 4; put32be(d + o, 0xC0A84D01); o += 4;
    d[o++] = 255;
    put16be(u, 67); put16be(u + 2, 68); put16be(u + 4, (u16)(8 + o)); put16be(u + 6, 0);
    if (type == 3) ap.sta_ip = 0xC0A84D0A;
    ip_out((const u8 *)"\xFF\xFF\xFF\xFF\xFF\xFF", 0xFFFFFFFF, 17, u, 8 + o);
    kprintf("wifi-sim: DHCP %s 192.168.77.10\n", type == 1 ? "OFFER" : "ACK");
}

static void ap_ip(const u8 *src_mac, const u8 *p, int len) {
    if (len < 20 || (p[0] >> 4) != 4) return;
    int ihl = (p[0] & 15) * 4;
    u32 dst = (u32)p[16] << 24 | (u32)p[17] << 16 | (u32)p[18] << 8 | p[19];
    u32 src = (u32)p[12] << 24 | (u32)p[13] << 16 | (u32)p[14] << 8 | p[15];
    const u8 *pl = p + ihl;
    int pll = MIN(len, (int)get16be(p + 2)) - ihl;
    if (p[9] == 17 && pll >= 8 && get16be(pl + 2) == 67) { dhcp_reply(pl + 8, pll - 8); return; }
    if (p[9] == 1 && dst == 0xC0A84D01 && pll >= 8 && pl[0] == 8) {   /* ping para o roteador */
        u8 r[1400];
        if (pll > (int)sizeof r) return;
        memcpy(r, pl, pll);
        r[0] = 0; r[2] = r[3] = 0;
        put16be(r + 2, csum(r, pll));
        ip_out(src_mac, src, 1, r, pll);
    }
}

static void ap_arp(const u8 *p, int len) {
    if (len < 28 || get16be(p + 6) != 1) return;
    if (p[24] != 192 || p[25] != 168 || p[26] != 77 || p[27] != 1) return;
    u8 r[28];
    put16be(r, 1); put16be(r + 2, 0x0800); r[4] = 6; r[5] = 4; put16be(r + 6, 2);
    memcpy(r + 8, AP_MAC, 6); memcpy(r + 14, p + 24, 4);
    memcpy(r + 18, p + 8, 6); memcpy(r + 24, p + 14, 4);
    ap_data(p + 8, AP_MAC, 0x0806, r, 28);
}

/* ---- o AP recebe um quadro da estação */
static void ap_rx(const u8 *f, int len) {
    if (len < 24 || channel != 6) return;
    int type = (f[0] >> 2) & 3, st = f[0] >> 4;
    bool to_ap = !memcmp(f + 4, AP_MAC, 6), bcast = f[4] == 0xFF;
    if (type == 0) {
        if (st == 4 && (bcast || to_ap)) { send_bss(5, f + 10, AP_MAC, AP_SSID, 6, 1); return; }
        if (!to_ap) return;
        memcpy(ap.sta, f + 10, 6);
        if (st == 11 && len >= 30) {                              /* autenticação aberta */
            u8 r[32];
            int n = mk_hdr(r, 0xB0, 0, ap.sta, AP_MAC, AP_MAC);
            put16le(r + n, 0); put16le(r + n + 2, 2); put16le(r + n + 4, 0);
            ap.auth = true;
            to_air(r, (u16)(n + 6));
        } else if (st == 0 && ap.auth) {                          /* associação */
            u8 r[64];
            int n = mk_hdr(r, 0x10, 0, ap.sta, AP_MAC, AP_MAC);
            put16le(r + n, 0x0411); put16le(r + n + 2, 0); put16le(r + n + 4, 0xC001);
            n += 6;
            static const u8 rates[] = {1, 4, 0x82, 0x84, 0x8B, 0x96};
            memcpy(r + n, rates, sizeof rates); n += sizeof rates;
            ap.assoc = true;
            to_air(r, (u16)n);
            ap_msg1();
        } else if (st == 12) {
            ap.auth = ap.assoc = false;
            ap.hs = 0;
        }
        return;
    }
    if (type != 2 || !to_ap || !ap.assoc) return;
    u8 buf[1700];
    const u8 *pt = f;
    int plen = len;
    if (f[1] & 0x40) {
        if (ap.hs != 4 || len > (int)sizeof buf) return;
        u64 pn;
        plen = ccmp_decrypt_frame(ap.ptk + 32, f, (u16)len, buf, &pn);
        if (plen < 0) { kprintf("wifi-sim: quadro com MIC CCMP inválido descartado\n"); return; }
        if (pn <= ap.pn_rx) return;
        ap.pn_rx = pn;
        pt = buf;
    }
    int hl = 24 + (((pt[0] & 0x80)) ? 2 : 0);
    const u8 *llc = pt + hl;
    int ll = plen - hl;
    if (ll < 8 || llc[0] != 0xAA) return;
    u16 et = get16be(llc + 6);
    if (et == 0x888E) ap_handle_eapol(llc + 8, ll - 8);
    else if (ap.hs == 4 && et == 0x0806) ap_arp(llc + 8, ll - 8);
    else if (ap.hs == 4 && et == 0x0800) ap_ip(pt + 10, llc + 8, ll - 8);
}

/* ---- interface de rádio */
static bool s_set_channel(wifi_radio_t *r, int ch) { channel = ch; return true; }
static bool s_tx(wifi_radio_t *r, const u8 *f, u16 len) { ap_rx(f, len); return true; }
static int s_rx(wifi_radio_t *r, u8 *f, u16 max, int *rssi) {
    if (air_t == air_h) return 0;
    u8 *c = air[air_t];
    int n = MIN((int)air_len[air_t], (int)max);
    const u8 *bssid = c + 16;
    *rssi = !memcmp(bssid, AP_MAC, 6) ? -48 : !memcmp(bssid, CAFE_MAC, 6) ? -67 : -81;
    memcpy(f, c, n);
    kfree(c);
    air_t = (air_t + 1) % AIRQ;
    return n;
}
static void s_poll(wifi_radio_t *r) {
    u64 now = timer_ticks();
    if (now < next_beacon) return;
    next_beacon = now + 100;
    if (channel == 1) send_bss(8, (const u8 *)"\xFF\xFF\xFF\xFF\xFF\xFF", CAFE_MAC, "Cafe Aberto", 1, 0);
    if (channel == 6) send_bss(8, (const u8 *)"\xFF\xFF\xFF\xFF\xFF\xFF", AP_MAC, AP_SSID, 6, 1);
    if (channel == 11) send_bss(8, (const u8 *)"\xFF\xFF\xFF\xFF\xFF\xFF", WPA3_MAC, "Vizinho WPA3", 11, 2);
}

bool wifi_sim_init(void) {
    if (!vfs_lookup("/Sistema/Biblioteca/wifi-simulado")) return false;
    sim.name = "Rádio simulado (teste)";
    static const u8 mac[6] = {0x02, 0x6D, 0x4F, 0x53, 0x00, 0x01};
    memcpy(sim.mac, mac, 6);
    sim.set_channel = s_set_channel;
    sim.tx = s_tx;
    sim.rx = s_rx;
    sim.poll = s_poll;
    wpa_psk(AP_PASS, (const u8 *)AP_SSID, strlen(AP_SSID), ap.pmk);
    for (int i = 0; i < 16; i++) ap.gtk[i] = (u8)(rand32() >> 5);
    wifi_register_radio(&sim);
    return true;
}
