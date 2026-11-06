/*
 * mOS — pilha 802.11 de estação (softMAC): scan ativo/passivo, autenticação aberta, associação,
 * handshake WPA2-PSK de 4 vias (e de grupo), CCMP e conversão 802.11 <-> Ethernet.
 * A interface aparece para a pilha IP como mais uma placa de rede (netdev_t "wlan0").
 */
#include "wifi.h"
#include "net.h"

#define MAX_NETS     32
#define DWELL_MS     110
#define STEP_TIMEOUT 600
#define RXQ          32

static const u8 BCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static const u8 OUI_RSN[3] = {0x00, 0x0F, 0xAC};
static const u8 LLC_EAPOL[8] = {0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00, 0x88, 0x8E};

static wifi_radio_t *radio;
static bool enabled = true;
static int state = WIFI_OFF;
static char err[96];

/* ---- varredura */
static wifi_network_t nets[MAX_NETS];
static int nnets;
static int scan_ch, scan_list[32], scan_nch;
static u64 scan_until;

/* ---- conexão */
static wifi_network_t target;
static u8 pmk[32], ptk[48], gtk[32], snonce[32], anonce[32];
static int gtk_id, gtk_len;
static bool keys_ok, gtk_ok;
static u64 tx_pn, rx_pn_pair, rx_pn_group, replay;
static u16 seqno, aid;
static u64 step_at;
static int tries;
static char want_ssid[33], want_pass[64];

/* ---- dados recebidos já convertidos para Ethernet */
static u8 *rxq[RXQ];
static u16 rxq_len[RXQ];
static int rxq_h, rxq_t;

#define KCK (ptk)
#define KEK (ptk + 16)
#define TK  (ptk + 32)

static void fail(const char *why) {
    snprintf(err, sizeof err, "%s", why);
    kprintf("wifi: %s\n", why);
    state = WIFI_FAILED;
    keys_ok = gtk_ok = false;
}

static void put16le(u8 *p, u16 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); }
static u16 get16le(const u8 *p) { return (u16)(p[0] | p[1] << 8); }
static u16 get16be(const u8 *p) { return (u16)(p[0] << 8 | p[1]); }
static void rnd(u8 *p, int n) { for (int i = 0; i < n; i++) p[i] = (u8)(rand32() ^ (u32)(rdtsc() >> (i & 7))); }

/* cabeçalho 802.11 de 24 bytes */
static int hdr(u8 *f, u8 fc0, u8 fc1, const u8 *a1, const u8 *a2, const u8 *a3) {
    f[0] = fc0; f[1] = fc1;
    f[2] = f[3] = 0;
    memcpy(f + 4, a1, 6);
    memcpy(f + 10, a2, 6);
    memcpy(f + 16, a3, 6);
    put16le(f + 22, (u16)(seqno++ << 4));
    return 24;
}

/* ============================================================ CCMP */
static int hdr_len(const u8 *f) {
    int l = 24;
    if ((f[1] & 3) == 3) l += 6;                 /* A4 */
    if ((f[0] & 0x0C) == 0x08 && (f[0] & 0x80)) l += 2;   /* QoS data */
    return l;
}

static void ccmp_aad_nonce(const u8 *f, int hl, u64 pn, u8 *aad, int *alen, u8 nonce[13]) {
    bool qos = (f[0] & 0x0C) == 0x08 && (f[0] & 0x80), a4 = (f[1] & 3) == 3;
    aad[0] = f[0] & ((f[0] & 0x0C) == 0x08 ? 0x8F : 0xFF);   /* dados: zera bits 4-6 do subtipo */
    aad[1] = (f[1] & ~0x38) | 0x40;              /* zera retry/pwrmgt/moredata, liga Protected */
    memcpy(aad + 2, f + 4, 18);                  /* A1 A2 A3 */
    aad[20] = f[22] & 0x0F;                      /* só o número do fragmento */
    aad[21] = 0;
    int n = 22;
    if (a4) { memcpy(aad + n, f + 24, 6); n += 6; }
    u8 tid = 0;
    if (qos) { tid = f[hl - 2] & 0x0F; aad[n++] = tid; aad[n++] = 0; }
    *alen = n;
    nonce[0] = tid;
    memcpy(nonce + 1, f + 10, 6);                /* A2 */
    for (int i = 0; i < 6; i++) nonce[7 + i] = (u8)(pn >> (40 - 8 * i));
}

u16 ccmp_encrypt_frame(const u8 tk[16], u64 pn, int keyid, const u8 *in, u16 len, u8 *out) {
    int hl = hdr_len(in);
    memcpy(out, in, hl);
    out[1] |= 0x40;
    u8 *c = out + hl;
    c[0] = (u8)pn; c[1] = (u8)(pn >> 8); c[2] = 0; c[3] = (u8)(0x20 | keyid << 6);
    c[4] = (u8)(pn >> 16); c[5] = (u8)(pn >> 24); c[6] = (u8)(pn >> 32); c[7] = (u8)(pn >> 40);
    u16 plen = (u16)(len - hl);
    memcpy(out + hl + 8, in + hl, plen);
    u8 aad[32], nonce[13];
    int alen;
    ccmp_aad_nonce(out, hl, pn, aad, &alen, nonce);
    wpa_ccm_encrypt(tk, nonce, aad, (usize)alen, out + hl + 8, plen, out + hl + 8 + plen);
    return (u16)(len + 16);
}

int ccmp_decrypt_frame(const u8 tk[16], const u8 *in, u16 len, u8 *out, u64 *pn_out) {
    int hl = hdr_len(in);
    if (len < hl + 16 || !(in[hl + 3] & 0x20)) return -1;
    const u8 *c = in + hl;
    u64 pn = (u64)c[0] | (u64)c[1] << 8 | (u64)c[4] << 16 | (u64)c[5] << 24 | (u64)c[6] << 32 | (u64)c[7] << 40;
    u16 plen = (u16)(len - hl - 16);
    memcpy(out, in, hl);
    out[1] &= ~0x40;
    memcpy(out + hl, c + 8, plen);
    u8 aad[32], nonce[13];
    int alen;
    ccmp_aad_nonce(in, hl, pn, aad, &alen, nonce);
    if (!wpa_ccm_decrypt(tk, nonce, aad, (usize)alen, out + hl, plen, c + 8 + plen)) return -1;
    if (pn_out) *pn_out = pn;
    return hl + plen;
}

/* ============================================================ envio */
static bool tx_raw(const u8 *f, u16 len) { return radio && radio->tx(radio, f, len); }

/* quadro de dados (para o AP), protegido se já houver chaves */
static bool tx_data(const u8 *dst, const u8 *src, u16 ethertype, const u8 *payload, u16 plen, bool allow_plain) {
    u8 *f = kmalloc(plen + 64), *e = NULL;
    if (!f) return false;
    int n = hdr(f, 0x08, 0x01, target.bssid, src, dst);   /* data, ToDS */
    static const u8 snap[6] = {0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00};
    memcpy(f + n, snap, 6);
    f[n + 6] = (u8)(ethertype >> 8); f[n + 7] = (u8)ethertype;
    memcpy(f + n + 8, payload, plen);
    u16 len = (u16)(n + 8 + plen);
    bool ok;
    if (keys_ok) {
        e = kmalloc(len + 32);
        ok = e && tx_raw(e, ccmp_encrypt_frame(TK, ++tx_pn, 0, f, len, e));
    } else ok = allow_plain && tx_raw(f, len);
    kfree(f);
    kfree(e);
    return ok;
}

/* ============================================================ elementos de informação */
static int rsn_ie(u8 *p) {
    static const u8 ie[] = {48, 20, 1, 0, 0x00, 0x0F, 0xAC, 4, 1, 0, 0x00, 0x0F, 0xAC, 4, 1, 0, 0x00, 0x0F, 0xAC, 2, 0, 0};
    memcpy(p, ie, sizeof ie);
    return sizeof ie;
}

static int parse_security(const u8 *ies, int len, bool privacy) {
    int sec = privacy ? WIFI_SEC_WEP : WIFI_SEC_OPEN;
    for (int i = 0; i + 2 <= len && i + 2 + ies[i + 1] <= len; i += 2 + ies[i + 1]) {
        if (ies[i] != 48 || ies[i + 1] < 18) continue;
        const u8 *r = ies + i + 2, *end = r + ies[i + 1];
        const u8 *q = r + 2 + 4;                  /* versão + suíte de grupo */
        if (q + 2 > end) return WIFI_SEC_ENTERPRISE;
        u16 npw = get16le(q); q += 2 + 4 * npw;
        if (q + 2 > end) return WIFI_SEC_ENTERPRISE;
        u16 nakm = get16le(q); q += 2;
        int best = WIFI_SEC_ENTERPRISE;
        for (int k = 0; k < nakm && q + 4 <= end; k++, q += 4)
            if (!memcmp(q, OUI_RSN, 3)) {
                if (q[3] == 2 || q[3] == 6) best = WIFI_SEC_WPA2_PSK;       /* PSK (inclusive SHA-256) */
                else if (q[3] == 8 && best != WIFI_SEC_WPA2_PSK) best = WIFI_SEC_WPA3_SAE;
            }
        return best;
    }
    return sec;
}

static void note_bss(const u8 *f, int len, int rssi) {
    const u8 *bssid = f + 16, *fixed = f + 24, *ies = f + 36;
    int ielen = len - 36;
    if (ielen < 0) return;
    char ssid[33] = "";
    int ch = scan_ch ? scan_ch : 0;
    for (int i = 0; i + 2 <= ielen && i + 2 + ies[i + 1] <= ielen; i += 2 + ies[i + 1]) {
        if (ies[i] == 0 && ies[i + 1] <= 32) { memcpy(ssid, ies + i + 2, ies[i + 1]); ssid[ies[i + 1]] = 0; }
        if (ies[i] == 3 && ies[i + 1] == 1) ch = ies[i + 2];
    }
    if (!ssid[0]) return;                        /* rede oculta: ignora por enquanto */
    wifi_network_t *n = NULL;
    for (int i = 0; i < nnets; i++) if (!memcmp(nets[i].bssid, bssid, 6)) n = &nets[i];
    if (!n && nnets < MAX_NETS) n = &nets[nnets++];
    if (!n) return;
    strcpy(n->ssid, ssid);
    memcpy(n->bssid, bssid, 6);
    n->rssi = rssi;
    n->channel = ch;
    n->security = parse_security(ies, ielen, get16le(fixed + 10) & 0x10);
    n->supported = n->security == WIFI_SEC_OPEN || n->security == WIFI_SEC_WPA2_PSK;
}

/* ============================================================ EAPOL-Key */
#define KI_VER2    0x0002
#define KI_PAIR    0x0008
#define KI_INSTALL 0x0040
#define KI_ACK     0x0080
#define KI_MIC     0x0100
#define KI_SECURE  0x0200
#define KI_ENC     0x1000

static bool send_eapol_key(u16 info, const u8 *nonce, const u8 *kdata, u16 klen) {
    u8 b[256];
    u16 body = (u16)(95 + klen);
    memset(b, 0, sizeof b);
    b[0] = 2; b[1] = 3; b[2] = (u8)(body >> 8); b[3] = (u8)body;
    u8 *k = b + 4;
    k[0] = 2;                                    /* descritor RSN */
    k[1] = (u8)(info >> 8); k[2] = (u8)info;
    k[3] = 0; k[4] = 0;                          /* key length: 0 nas mensagens da estação */
    for (int i = 0; i < 8; i++) k[5 + i] = (u8)(replay >> (56 - 8 * i));
    if (nonce) memcpy(k + 13, nonce, 32);
    k[93] = (u8)(klen >> 8); k[94] = (u8)klen;
    if (klen) memcpy(k + 95, kdata, klen);
    if (info & KI_MIC) wpa_eapol_mic(KCK, b, 4u + body, k + 77);
    return tx_data(target.bssid, radio->mac, 0x888E, b, (u16)(4 + body), true);
}

static void handle_eapol(const u8 *p, int len) {
    if (len < 99 || p[1] != 3 || p[4] != 2) return;
    const u8 *k = p + 4;
    u16 info = (u16)(k[1] << 8 | k[2]);
    u64 rc = 0;
    for (int i = 0; i < 8; i++) rc = rc << 8 | k[5 + i];
    u16 klen = get16be(k + 93);
    if (95 + klen > len - 4) return;
    if ((info & 7) != 2) { fail("A rede usa um tipo de chave não suportado."); return; }

    if ((info & KI_PAIR) && (info & KI_ACK) && !(info & KI_MIC)) {          /* mensagem 1 */
        if (state != WIFI_HANDSHAKE && state != WIFI_CONNECTED) return;
        replay = rc;
        memcpy(anonce, k + 13, 32);
        rnd(snonce, 32);
        wpa_ptk(pmk, target.bssid, radio->mac, anonce, snonce, ptk);
        u8 ie[32];
        int il = rsn_ie(ie);
        state = WIFI_HANDSHAKE;
        step_at = timer_ticks();
        send_eapol_key(KI_VER2 | KI_PAIR | KI_MIC, snonce, ie, (u16)il);
        return;
    }
    /* mensagens com MIC: confere antes de usar qualquer coisa */
    u8 copy[512], mic[16];
    if (len > (int)sizeof copy || !(info & KI_MIC)) return;
    if (rc < replay) return;
    memcpy(copy, p, len);
    memset(copy + 4 + 77, 0, 16);
    wpa_eapol_mic(KCK, copy, (usize)(4 + get16be(p + 2)), mic);
    if (memcmp(mic, k + 77, 16)) { fail("Senha incorreta (o roteador recusou a chave)."); return; }
    replay = rc;
    u8 kd[256];
    int kdl = 0;
    if (info & KI_ENC) {
        if (klen < 24 || klen > sizeof kd + 8 || !wpa_key_unwrap(KEK, k + 95, klen, kd)) { fail("Falha ao decifrar a chave de grupo."); return; }
        kdl = klen - 8;
    }
    /* GTK KDE: dd len 00-0F-AC 01 keyid rsv GTK */
    for (int i = 0; i + 2 <= kdl && i + 2 + kd[i + 1] <= kdl; i += 2 + kd[i + 1]) {
        if (kd[i] == 0xDD && kd[i + 1] >= 6 && !memcmp(kd + i + 2, OUI_RSN, 3) && kd[i + 5] == 1) {
            gtk_id = kd[i + 6] & 3;
            gtk_len = MIN(kd[i + 1] - 6, 32);
            memcpy(gtk, kd + i + 8, gtk_len);
            gtk_ok = true;
            rx_pn_group = 0;
        }
        if (kd[i] == 0) break;
    }
    if (info & KI_PAIR) {                                                   /* mensagem 3 */
        send_eapol_key(KI_VER2 | KI_PAIR | KI_MIC | KI_SECURE, NULL, NULL, 0);
        keys_ok = true;
        tx_pn = rx_pn_pair = 0;
        state = WIFI_CONNECTED;
        err[0] = 0;
        kprintf("wifi: conectado a \"%s\" (canal %d, WPA2, GTK %d)\n", target.ssid, target.channel, gtk_id);
    } else {                                                                /* troca de chave de grupo */
        send_eapol_key(KI_VER2 | KI_MIC | KI_SECURE, NULL, NULL, 0);
        kprintf("wifi: nova chave de grupo (%d)\n", gtk_id);
    }
}

/* ============================================================ recepção */
static void queue_eth(const u8 *dst, const u8 *src, const u8 *llc, int len) {
    if (len < 8 || memcmp(llc, LLC_EAPOL, 6)) return;                        /* só LLC/SNAP */
    int nx = (rxq_h + 1) % RXQ;
    if (nx == rxq_t) return;                                                 /* fila cheia: descarta */
    u8 *e = kmalloc(len + 6);
    if (!e) return;
    memcpy(e, dst, 6);
    memcpy(e + 6, src, 6);
    memcpy(e + 12, llc + 6, len - 6);                                        /* ethertype + dados */
    rxq[rxq_h] = e;
    rxq_len[rxq_h] = (u16)(len + 6);    rxq_h = nx;
}

static void rx_data(u8 *f, int len) {
    if ((f[1] & 3) != 2) return;                                             /* só FromDS */
    if (memcmp(f + 10, target.bssid, 6)) return;
    u8 *pt = f;
    int plen = len;
    u8 buf[2400];
    if (f[1] & 0x40) {
        if (!keys_ok || len > (int)sizeof buf) return;
        bool group = f[4] & 1;
        int kid = f[hdr_len(f) + 3] >> 6;
        u64 pn;
        if (group && (!gtk_ok || kid != gtk_id)) return;
        plen = ccmp_decrypt_frame(group ? gtk : TK, f, (u16)len, buf, &pn);
        if (plen < 0) return;
        u64 *last = group ? &rx_pn_group : &rx_pn_pair;
        if (pn <= *last) return;                                             /* repetição */
        *last = pn;
        pt = buf;
    } else if (keys_ok) {
        int hl = hdr_len(f);                                                 /* em claro só EAPOL */
        if (len < hl + 8 || memcmp(f + hl, LLC_EAPOL, 8)) return;
    }
    int hl = hdr_len(pt);
    const u8 *llc = pt + hl;
    int ll = plen - hl;
    if (ll >= 8 && !memcmp(llc, LLC_EAPOL, 8)) { handle_eapol(llc + 8, ll - 8); return; }
    queue_eth(pt + 4, pt + 16, llc, ll);
}

static void rx_mgmt(u8 *f, int len, int rssi) {
    u8 st = f[0] >> 4;
    bool for_us = !memcmp(f + 4, radio->mac, 6);
    if (st == 8 || st == 5) { note_bss(f, len, rssi); if (!memcmp(f + 16, target.bssid, 6)) target.rssi = rssi; return; }
    if (!for_us || memcmp(f + 16, target.bssid, 6)) return;
    if (st == 11 && state == WIFI_AUTHENTICATING && len >= 30) {             /* autenticação */
        if (get16le(f + 26) != 2) return;
        if (get16le(f + 28)) { fail("O roteador recusou a autenticação."); return; }
        u8 a[160];
        int n = hdr(a, 0x00, 0x00, target.bssid, radio->mac, target.bssid);  /* associação */
        /* ESS | short preamble | short slot (+ privacidade em rede protegida) */
        put16le(a + n, target.security == WIFI_SEC_OPEN ? 0x0421 : 0x0431); n += 2;
        put16le(a + n, 10); n += 2;                                          /* listen interval */
        usize sl = strlen(target.ssid);
        a[n++] = 0; a[n++] = (u8)sl; memcpy(a + n, target.ssid, sl); n += (int)sl;
        static const u8 rates[] = {1, 8, 0x82, 0x84, 0x8B, 0x96, 0x0C, 0x12, 0x18, 0x24, 50, 4, 0x30, 0x48, 0x60, 0x6C};
        memcpy(a + n, rates, sizeof rates); n += sizeof rates;
        if (target.security == WIFI_SEC_WPA2_PSK) n += rsn_ie(a + n);
        state = WIFI_ASSOCIATING;
        step_at = timer_ticks();
        tries = 0;
        tx_raw(a, (u16)n);
        return;
    }
    if ((st == 1 || st == 3) && state == WIFI_ASSOCIATING && len >= 30) {   /* resposta de associação */
        u16 status = get16le(f + 26);
        if (status) { char m[64]; snprintf(m, sizeof m, "O roteador recusou a associação (código %u).", status); fail(m); return; }
        aid = get16le(f + 28) & 0x3FFF;
        if (target.security == WIFI_SEC_OPEN) {
            state = WIFI_CONNECTED;
            err[0] = 0;
            kprintf("wifi: conectado a \"%s\" (rede aberta, canal %d)\n", target.ssid, target.channel);
        } else {
            state = WIFI_HANDSHAKE;
            step_at = timer_ticks();
        }
        return;
    }
    if (st == 12 || st == 10) {                                              /* deauth / disassoc */
        u16 reason = len >= 26 ? get16le(f + 24) : 0;
        char m[80];
        snprintf(m, sizeof m, reason == 15 || reason == 2 ? "Senha incorreta ou chave expirada (motivo %u)." : "Desconectado pelo roteador (motivo %u).", reason);
        fail(m);
    }
}

static void rx_frame(u8 *f, int len, int rssi) {
    if (len < 24) return;
    int type = (f[0] >> 2) & 3;
    if (type == 0) rx_mgmt(f, len, rssi);
    else if (type == 2 && (state >= WIFI_HANDSHAKE)) rx_data(f, len);
}

/* ============================================================ netdev */
static bool nd_send(const void *frame, u16 len) {
    const u8 *e = frame;
    if (state != WIFI_CONNECTED || len < 14) return false;
    return tx_data(e, radio->mac, (u16)(e[12] << 8 | e[13]), e + 14, (u16)(len - 14), target.security == WIFI_SEC_OPEN);
}

static int nd_recv(void *frame, u16 max) {
    wifi_poll();                    /* net_poll chama isto a cada volta do laço: mantém o rádio vivo */
    if (rxq_t == rxq_h) return 0;
    u8 *e = rxq[rxq_t];
    int n = MIN((int)rxq_len[rxq_t], (int)max);
    memcpy(frame, e, n);
    kfree(e);
    rxq_t = (rxq_t + 1) % RXQ;
    return n;
}

static bool nd_link(void) { return state == WIFI_CONNECTED; }
static netdev_t wlan0 = {.name = "Wi-Fi (wlan0)", .send = nd_send, .recv = nd_recv, .link_up = nd_link};

/* ============================================================ API */
void wifi_register_radio(wifi_radio_t *r) {
    if (radio) return;
    radio = r;
    state = WIFI_IDLE;
    memcpy(wlan0.mac, r->mac, 6);
    kprintf("wifi: rádio \"%s\", MAC %02x:%02x:%02x:%02x:%02x:%02x\n", r->name, r->mac[0], r->mac[1], r->mac[2], r->mac[3], r->mac[4], r->mac[5]);
    net_register_device(&wlan0);   /* só vira a interface ativa se não houver cabo */
}

bool wifi_present(void) { return radio != NULL; }

static void scan_next(void) {
    if (scan_ch >= scan_nch) { state = target.ssid[0] && want_ssid[0] ? WIFI_IDLE : WIFI_IDLE; scan_ch = 0; return; }
    int ch = scan_list[scan_ch++];
    radio->set_channel(radio, ch);
    u8 p[64];
    int n = hdr(p, 0x40, 0x00, BCAST, radio->mac, BCAST);                    /* probe request */
    p[n++] = 0; p[n++] = 0;                                                  /* SSID curinga */
    static const u8 rates[] = {1, 4, 0x82, 0x84, 0x8B, 0x96};
    memcpy(p + n, rates, sizeof rates); n += sizeof rates;
    tx_raw(p, (u16)n);
    scan_until = timer_ticks() + DWELL_MS;
}

bool wifi_scan(void) {
    if (!radio || !enabled || state == WIFI_SCANNING) return false;
    if (state >= WIFI_AUTHENTICATING && state <= WIFI_CONNECTED) return false; /* não interrompe a conexão */
    nnets = 0;
    scan_nch = 0;
    for (int c = 1; c <= 13; c++) scan_list[scan_nch++] = c;
    static const int five[] = {36, 40, 44, 48, 149, 153, 157, 161};
    for (u32 i = 0; i < ARRAY_LEN(five); i++) scan_list[scan_nch++] = five[i];
    scan_ch = 0;
    state = WIFI_SCANNING;
    scan_next();
    return true;
}

int wifi_networks(wifi_network_t *out, int max) {
    int idx[MAX_NETS];
    for (int i = 0; i < nnets; i++) idx[i] = i;
    for (int i = 1; i < nnets; i++)                                          /* mais fortes primeiro */
        for (int j = i; j > 0 && nets[idx[j]].rssi > nets[idx[j - 1]].rssi; j--) { int t = idx[j]; idx[j] = idx[j - 1]; idx[j - 1] = t; }
    int n = 0;
    for (int i = 0; i < nnets && n < max; i++) {
        bool dup = false;                                                    /* um item por SSID */
        for (int k = 0; k < n; k++) if (!strcmp(out[k].ssid, nets[idx[i]].ssid)) dup = true;
        if (!dup) out[n++] = nets[idx[i]];
    }
    return n;
}

static void start_auth(void) {
    radio->set_channel(radio, target.channel);
    if (radio->fw_connect) {                                                 /* firmware faz o MLME */
        state = radio->fw_connect(radio, target.ssid, target.bssid, target.channel, pmk) ? WIFI_HANDSHAKE : WIFI_FAILED;
        return;
    }
    u8 a[32];
    int n = hdr(a, 0xB0, 0x00, target.bssid, radio->mac, target.bssid);
    put16le(a + n, 0); put16le(a + n + 2, 1); put16le(a + n + 4, 0);          /* aberta, seq 1 */
    state = WIFI_AUTHENTICATING;
    step_at = timer_ticks();
    tx_raw(a, (u16)(n + 6));
}

bool wifi_connect(const char *ssid, const char *pass) {
    if (!radio || !enabled || !ssid || !*ssid) return false;
    wifi_network_t *found = NULL;
    for (int i = 0; i < nnets; i++)
        if (!strcmp(nets[i].ssid, ssid) && (!found || nets[i].rssi > found->rssi)) found = &nets[i];
    if (!found) {                                                            /* ainda não visto: varre antes */
        snprintf(want_ssid, sizeof want_ssid, "%s", ssid);
        snprintf(want_pass, sizeof want_pass, "%s", pass ? pass : "");
        target.ssid[0] = 0;
        return wifi_scan();
    }
    if (!found->supported) { fail(found->security == WIFI_SEC_WPA3_SAE ? "Redes só WPA3 ainda não são suportadas." : "Tipo de segurança não suportado."); return false; }
    if (found->security == WIFI_SEC_WPA2_PSK && (!pass || strlen(pass) < 8 || strlen(pass) > 63)) { fail("A senha do Wi-Fi precisa ter de 8 a 63 caracteres."); return false; }
    target = *found;
    want_ssid[0] = 0;
    keys_ok = gtk_ok = false;
    replay = 0;
    if (found->security == WIFI_SEC_WPA2_PSK) wpa_psk(pass, (const u8 *)target.ssid, strlen(target.ssid), pmk);
    tries = 0;
    kprintf("wifi: conectando a \"%s\" (canal %d)\n", target.ssid, target.channel);
    start_auth();
    return true;
}

void wifi_disconnect(void) {
    if (!radio || state < WIFI_AUTHENTICATING) return;
    u8 d[32];
    int n = hdr(d, 0xC0, 0x00, target.bssid, radio->mac, target.bssid);
    put16le(d + n, 3);                                                       /* motivo: saindo */
    tx_raw(d, (u16)(n + 2));
    state = WIFI_IDLE;
    keys_ok = gtk_ok = false;
    target.ssid[0] = 0;
}

void wifi_set_enabled(bool on) {
    if (!on) wifi_disconnect();
    enabled = on;
    if (radio) state = on ? WIFI_IDLE : WIFI_OFF;
}

void wifi_status(wifi_status_t *s) {
    memset(s, 0, sizeof *s);
    s->present = radio != NULL;
    s->state = radio ? state : WIFI_OFF;
    if (state >= WIFI_AUTHENTICATING && state <= WIFI_CONNECTED) {
        strcpy(s->ssid, target.ssid);
        memcpy(s->bssid, target.bssid, 6);
        s->rssi = target.rssi;
        s->channel = target.channel;
    }
    snprintf(s->error, sizeof s->error, "%s", err);
    extern const char *wifi_detected_chip(void);
    const char *chip = wifi_detected_chip();
    if (radio) snprintf(s->driver, sizeof s->driver, "%s", radio->name);
    else if (chip[0]) snprintf(s->driver, sizeof s->driver, "%s (sem driver)", chip);
    else snprintf(s->driver, sizeof s->driver, "Nenhuma placa Wi-Fi");
}

void wifi_poll(void) {
    static u64 last;
    if (!radio || timer_ticks() == last) return;   /* no máximo uma vez por milissegundo */
    last = timer_ticks();
    extern void wifi_selftest_poll(void);
    wifi_selftest_poll();
    if (radio->poll) radio->poll(radio);
    u8 *f = kmalloc(2400);
    if (!f) return;
    for (int k = 0; k < 64; k++) {
        int rssi = -60;
        int n = radio->rx(radio, f, 2400, &rssi);
        if (n <= 0) break;
        rx_frame(f, n, rssi);
    }
    kfree(f);
    u64 now = timer_ticks();
    if (state == WIFI_SCANNING && now >= scan_until) {
        scan_next();
        if (state != WIFI_SCANNING && want_ssid[0]) {                        /* conexão pedida antes da varredura */
            char s[33], p[64];
            strcpy(s, want_ssid); strcpy(p, want_pass);
            bool seen = false;
            for (int i = 0; i < nnets; i++) if (!strcmp(nets[i].ssid, s)) seen = true;
            want_ssid[0] = 0;
            if (seen) wifi_connect(s, p);
            else { char m[96]; snprintf(m, sizeof m, "A rede \"%.40s\" não foi encontrada.", s); fail(m); }
        }
    }
    if ((state == WIFI_AUTHENTICATING || state == WIFI_ASSOCIATING || state == WIFI_HANDSHAKE) && now - step_at > STEP_TIMEOUT) {
        if (++tries > 3) { fail(state == WIFI_HANDSHAKE ? "Tempo esgotado no handshake WPA2 (senha errada?)." : "O roteador não respondeu."); return; }
        if (state != WIFI_HANDSHAKE) start_auth();
        else step_at = now;
    }
}
