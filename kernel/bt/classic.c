/*
 * mOS — Bluetooth clássico (BR/EDR): varredura (inquiry com EIR), conexão, pareamento SSP
 * (o controlador faz a criptografia; aqui respondemos "Just Works" e guardamos a chave de enlace),
 * L2CAP com canais dinâmicos, A2DP fonte (AVDTP + RTP + SBC) para fones e HID clássico (HIDP,
 * protocolo boot) para teclados e mouses.
 *
 * Vínculos em /Sistema/Biblioteca/Bluetooth/<endereço>.chave (16 bytes da chave + classe do dispositivo).
 * Eventos e ACL chegam pela fila de ble.c (classic_event / classic_acl) e os envios saem em classic_poll.
 */
#include "bluetooth.h"
#include "vfs.h"
#include "sbc.h"

#define MAX_DEVS  16
#define KEY_DIR   "/Sistema/Biblioteca/Bluetooth"
#define PSM_HID_CTRL 0x0011
#define PSM_HID_INTR 0x0013
#define PSM_AVDTP    0x0019
#define MAXCH 6

void input_hid_keyboard(u8 mods, const u8 keys[6]);

typedef struct { u8 addr[6]; u32 cod; int rssi; char name[48]; } cdev_t;
static cdev_t devs[MAX_DEVS];
static int ndevs;
static bool inquiring, ready;
static char err[96];

typedef struct {
    bool used, open, cfg_in, cfg_out;
    u16 psm, scid, dcid, rmtu;
} chan_t;

static struct {
    int st;                     /* 0 livre, 1 conectando, 2 autenticando, 3 cifrando, 4 pronto */
    u16 handle;
    u8 addr[6];
    u32 cod;
    chan_t ch[MAXCH];
    u8 sig_id;
    /* A2DP */
    int av_st;                  /* 0 nada, 1 discover, 2 caps, 3 setconf, 4 open, 5 midia, 6 start, 7 tocando */
    u8 av_label, acp_seid;
    int sig_ch, media_ch;
    sbc_enc_t sbc;
    u32 rtp_ts;
    u16 rtp_seq;
    u64 started;
    u64 sent_frames;
    /* HID */
    int hid_ctrl, hid_intr;
    u8 last_keys[6];
    u64 t;
} c;

/* ---- PCM para o fone: anel alimentado pelo mixer (audio_tap) ou pelo tom de teste */
#define PCM_RING (48000 * 2)
static i16 ring[PCM_RING * 2];
static u32 rh, rt;
static u32 tone_left;

static void tap(const i16 *stereo, u32 frames) {
    for (u32 i = 0; i < frames; i++) {
        u32 nx = (rh + 1) % PCM_RING;
        if (nx == rt) break;                                   /* cheio: descarta (o fone está atrasado) */
        ring[rh * 2] = stereo[i * 2];
        ring[rh * 2 + 1] = stereo[i * 2 + 1];
        rh = nx;
    }
}
static u32 ring_avail(void) { return (rh + PCM_RING - rt) % PCM_RING; }

/* ---- utilidades */
static u16 le16(const u8 *p) { return (u16)(p[0] | p[1] << 8); }
static void put16(u8 *p, u16 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); }
static void addr_str(const u8 *a, char *o) { snprintf(o, 18, "%02X:%02X:%02X:%02X:%02X:%02X", a[5], a[4], a[3], a[2], a[1], a[0]); }
static void fail(const char *m) { snprintf(err, sizeof err, "%s", m); kprintf("bt: %s\n", m); }

static void key_path(const u8 *a, char *p, usize n) {
    char s[18];
    addr_str(a, s);
    for (char *q = s; *q; q++) if (*q == ':') *q = '-';
    snprintf(p, n, KEY_DIR "/%s.chave", s);
}

/* ---- L2CAP */
static bool acl(const u8 *l2, u16 len) { return bt_acl_send(c.handle, l2, len); }
static bool l2_send(u16 cid, const u8 *d, u16 len) {
    u8 b[1100];
    if (len + 4 > (int)sizeof b) return false;
    put16(b, len); put16(b + 2, cid);
    memcpy(b + 4, d, len);
    return acl(b, (u16)(len + 4));
}
static void sig_send(u8 code, const u8 *d, u16 len) {
    u8 b[64];
    b[0] = code; b[1] = ++c.sig_id ? c.sig_id : ++c.sig_id; put16(b + 2, len);
    memcpy(b + 4, d, len);
    l2_send(0x0001, b, (u16)(4 + len));
}
static void sig_reply(u8 code, u8 id, const u8 *d, u16 len) {
    u8 b[64];
    b[0] = code; b[1] = id; put16(b + 2, len);
    memcpy(b + 4, d, len);
    l2_send(0x0001, b, (u16)(4 + len));
}

static int ch_new(u16 psm) {
    for (int i = 0; i < MAXCH; i++)
        if (!c.ch[i].used) { memset(&c.ch[i], 0, sizeof c.ch[i]); c.ch[i].used = true; c.ch[i].psm = psm; c.ch[i].scid = (u16)(0x0040 + i); c.ch[i].rmtu = 672; return i; }
    return -1;
}
static int ch_by_scid(u16 scid) { for (int i = 0; i < MAXCH; i++) if (c.ch[i].used && c.ch[i].scid == scid) return i; return -1; }

static int l2_connect(u16 psm) {
    int i = ch_new(psm);
    if (i < 0) return -1;
    u8 d[4]; put16(d, psm); put16(d + 2, c.ch[i].scid);
    sig_send(0x02, d, 4);
    return i;
}
static void l2_config(int i) {
    u8 d[8]; put16(d, c.ch[i].dcid); put16(d + 2, 0); d[4] = 0x01; d[5] = 2; put16(d + 6, 1013);  /* nosso MTU */
    sig_send(0x04, d, 8);
}

static void chan_opened(int i);

static void l2_signal(const u8 *p, int len) {
    while (len >= 4) {
        u8 code = p[0], id = p[1];
        u16 l = le16(p + 2);
        const u8 *d = p + 4;
        if (4 + l > len) return;
        switch (code) {
        case 0x02: {                                           /* pedido de conexão do dispositivo */
            u16 psm = le16(d), scid = le16(d + 2);
            int i = (psm == PSM_HID_CTRL || psm == PSM_HID_INTR || psm == PSM_AVDTP) ? ch_new(psm) : -1;
            u8 r[8];
            put16(r, i >= 0 ? c.ch[i].scid : 0); put16(r + 2, scid);
            put16(r + 4, i >= 0 ? 0 : 0x0002); put16(r + 6, 0);   /* 0 = ok, 2 = PSM não suportado */
            sig_reply(0x03, id, r, 8);
            if (i >= 0) { c.ch[i].dcid = scid; l2_config(i); }
            break;
        }
        case 0x03: {                                           /* resposta de conexão */
            u16 dcid = le16(d), scid = le16(d + 2), res = le16(d + 4);
            int i = ch_by_scid(scid);
            if (i < 0 || res == 1) break;                      /* 1 = pendente */
            if (res) { char m[64]; snprintf(m, sizeof m, "canal L2CAP recusado (PSM 0x%04x, %u)", c.ch[i].psm, res); fail(m); c.ch[i].used = false; break; }
            c.ch[i].dcid = dcid;
            l2_config(i);
            break;
        }
        case 0x04: {                                           /* pedido de configuração do dispositivo */
            u16 dcid = le16(d);
            int i = ch_by_scid(dcid);
            for (int k = 4; k + 2 <= l; k += 2 + d[k + 1]) if ((d[k] & 0x7F) == 0x01 && d[k + 1] >= 2) { if (i >= 0) c.ch[i].rmtu = le16(d + k + 2); }
            u8 r[6]; put16(r, i >= 0 ? c.ch[i].dcid : 0); put16(r + 2, 0); put16(r + 4, 0);
            sig_reply(0x05, id, r, 6);
            if (i >= 0) { c.ch[i].cfg_in = true; if (c.ch[i].cfg_out && !c.ch[i].open) { c.ch[i].open = true; chan_opened(i); } }
            break;
        }
        case 0x05: {                                           /* resposta à nossa configuração */
            int i = ch_by_scid(le16(d));
            if (i >= 0 && !le16(d + 4)) { c.ch[i].cfg_out = true; if (c.ch[i].cfg_in && !c.ch[i].open) { c.ch[i].open = true; chan_opened(i); } }
            break;
        }
        case 0x06: {                                           /* desconexão pedida pelo dispositivo */
            u16 dcid = le16(d), scid = le16(d + 2);
            u8 r[4]; put16(r, dcid); put16(r + 2, scid);
            sig_reply(0x07, id, r, 4);
            int i = ch_by_scid(dcid);
            if (i >= 0) c.ch[i].used = false;
            break;
        }
        case 0x0A: { u8 r[4]; put16(r, le16(d)); put16(r + 2, 0x0001); sig_reply(0x0B, id, r, 4); break; }   /* info: não suportado */
        default: break;
        }
        p += 4 + l;
        len -= 4 + l;
    }
}

/* ---- AVDTP (fonte) */
static void av_cmd(u8 sig, const u8 *d, int len) {
    u8 b[32];
    c.av_label = (u8)((c.av_label + 1) & 0x0F);
    b[0] = (u8)(c.av_label << 4);                              /* pacote único, comando */
    b[1] = sig;
    if (len) memcpy(b + 2, d, len);
    l2_send(c.ch[c.sig_ch].dcid, b, (u16)(2 + len));
}

static void av_start_stream(void) {
    sbc_init(&c.sbc, SBC_FREQ_48K, 16, SBC_MODE_JOINT, SBC_ALLOC_LOUDNESS, 51);
    c.started = timer_ticks();
    c.sent_frames = 0;
    c.av_st = 7;
    rt = rh;
    audio_tap = tap;
    audio_route_bluetooth = true;
    kprintf("bt: tocando no fone (SBC 48 kHz, estéreo conjunto, bitpool 51)\n");
}

static void av_rx(const u8 *p, int len) {
    if (len < 2) return;
    u8 type = p[0] & 3, sig = p[1] & 0x3F;
    if (type == 0) {                                           /* comando do fone: aceita o básico */
        u8 r[2] = {(u8)((p[0] & 0xF0) | 0x02), sig};
        if (sig == 0x07 && c.av_st >= 5) { l2_send(c.ch[c.sig_ch].dcid, r, 2); av_start_stream(); return; }   /* START do fone */
        if (sig == 0x09 || sig == 0x08) { l2_send(c.ch[c.sig_ch].dcid, r, 2); c.av_st = 5; audio_tap = NULL; audio_route_bluetooth = false; return; }
        u8 rej[3] = {(u8)((p[0] & 0xF0) | 0x01), sig, 0x19};    /* rejeição geral */
        l2_send(c.ch[c.sig_ch].dcid, rej, 3);
        return;
    }
    if (type == 3) { char m[64]; snprintf(m, sizeof m, "o fone recusou o comando AVDTP %u", sig); fail(m); return; }
    switch (sig) {
    case 0x01:                                                 /* DISCOVER: procura um "sink" de áudio livre */
        for (int i = 2; i + 2 <= len; i += 2) {
            u8 seid = p[i] >> 2, inuse = (p[i] >> 1) & 1, media = p[i + 1] >> 4, tsep = (p[i + 1] >> 3) & 1;
            if (!inuse && media == 0 && tsep == 1) { c.acp_seid = seid; break; }
        }
        if (!c.acp_seid) { fail("o dispositivo não tem receptor de áudio livre"); return; }
        { u8 d = (u8)(c.acp_seid << 2); c.av_st = 2; av_cmd(0x02, &d, 1); }
        return;
    case 0x02: {                                               /* GET_CAPABILITIES: confere SBC */
        bool sbc_ok = false;
        u8 caps0 = 0, caps1 = 0, maxbp = 53;
        for (int i = 2; i + 2 <= len; i += 2 + p[i + 1])
            if (p[i] == 0x07 && p[i + 1] >= 6 && p[i + 3] == 0x00) { sbc_ok = true; caps0 = p[i + 4]; caps1 = p[i + 5]; maxbp = p[i + 7]; }
        if (!sbc_ok || !(caps0 & 0x10) || !(caps0 & 0x01) || !(caps1 & 0x10) || !(caps1 & 0x04) || !(caps1 & 0x01)) {
            kprintf("bt: capacidades SBC do fone: 0x%02x 0x%02x (usando o que for obrigatório)\n", caps0, caps1);
        }
        u8 d[12] = {(u8)(c.acp_seid << 2), (u8)(1 << 2), 0x01, 0x00, 0x07, 0x06, 0x00, 0x00,
                    0x10 | 0x01,                               /* 48 kHz, estéreo conjunto */
                    0x10 | 0x04 | 0x01,                        /* 16 blocos, 8 sub-bandas, loudness */
                    2, (u8)MIN(maxbp, 51)};
        c.av_st = 3;
        av_cmd(0x03, d, 12);
        return;
    }
    case 0x03: { u8 d = (u8)(c.acp_seid << 2); c.av_st = 4; av_cmd(0x06, &d, 1); return; }   /* SET_CONFIGURATION ok -> OPEN */
    case 0x06: c.av_st = 5; c.media_ch = l2_connect(PSM_AVDTP); return;                       /* OPEN ok -> canal de mídia */
    case 0x07: av_start_stream(); return;                                                    /* START ok */
    default: return;
    }
}

/* envia quadros SBC no ritmo do relógio (48 kHz) */
static void av_pump(void) {
    if (c.av_st != 7 || c.media_ch < 0 || !c.ch[c.media_ch].open) return;
    int flen = sbc_frame_len(&c.sbc);
    int per = (int)MIN((u32)((c.ch[c.media_ch].rmtu - 13) / flen), 15u);
    if (per < 1) per = 1;
    u64 due = (timer_ticks() - c.started) * 48 / 128 + 4;    /* quadros que já deveriam ter saído (+ folga) */
    while (c.sent_frames + per <= due) {
        u8 pkt[1100];
        pkt[0] = 0x80; pkt[1] = 0x60;
        put16(pkt + 2, 0); pkt[2] = (u8)(c.rtp_seq >> 8); pkt[3] = (u8)c.rtp_seq;
        pkt[4] = (u8)(c.rtp_ts >> 24); pkt[5] = (u8)(c.rtp_ts >> 16); pkt[6] = (u8)(c.rtp_ts >> 8); pkt[7] = (u8)c.rtp_ts;
        pkt[8] = pkt[9] = pkt[10] = 0; pkt[11] = 1;           /* SSRC */
        pkt[12] = (u8)per;
        int off = 13;
        for (int f = 0; f < per; f++) {
            i16 pcm[128 * 2];
            for (int s = 0; s < 128; s++) {
                if (tone_left) {                               /* tom de teste: 660 Hz, decaindo */
                    u32 n = 48000 - tone_left;
                    i32 v = (i32)((n * 660 * 4 / 48000) & 2 ? 6000 : -6000) * (i32)tone_left / 48000;
                    pcm[s * 2] = pcm[s * 2 + 1] = (i16)v;
                    tone_left--;
                } else if (ring_avail()) { pcm[s * 2] = ring[rt * 2]; pcm[s * 2 + 1] = ring[rt * 2 + 1]; rt = (rt + 1) % PCM_RING; }
                else pcm[s * 2] = pcm[s * 2 + 1] = 0;
            }
            off += sbc_encode(&c.sbc, pcm, pkt + off);
        }
        l2_send(c.ch[c.media_ch].dcid, pkt, (u16)off);
        c.rtp_seq++;
        c.rtp_ts += (u32)(128 * per);
        c.sent_frames += (u64)per;
    }
}

/* ---- HID clássico (boot protocol) */
static void hid_rx(const u8 *p, int len) {
    if (len < 2 || p[0] != 0xA1) return;                       /* DATA | entrada */
    if (p[1] == 0x01 && len >= 10) input_hid_keyboard(p[2], p + 4);
    else if (p[1] == 0x02 && len >= 5) {
        input_event_t e = {0};
        e.type = EV_MOUSE;
        e.buttons = p[2] & 7;
        e.dx = (i8)p[3]; e.dy = (i8)p[4];
        e.dz = len >= 6 ? -(i8)p[5] : 0;
        e.mods = input_mods();
        input_push(&e);
    }
}

static void chan_opened(int i) {
    kprintf("bt: canal L2CAP aberto (PSM 0x%04x, MTU remoto %u)\n", c.ch[i].psm, c.ch[i].rmtu);
    if (c.ch[i].psm == PSM_AVDTP) {
        if (i == c.media_ch) { u8 d = (u8)(c.acp_seid << 2); c.av_st = 6; av_cmd(0x07, &d, 1); }
        else if (c.sig_ch < 0) { c.sig_ch = i; c.av_st = 1; av_cmd(0x01, NULL, 0); }
    } else if (c.ch[i].psm == PSM_HID_CTRL) {
        c.hid_ctrl = i;
        u8 boot = 0x70;                                        /* SET_PROTOCOL: boot */
        l2_send(c.ch[i].dcid, &boot, 1);
        if (c.hid_intr < 0) c.hid_intr = l2_connect(PSM_HID_INTR);
    } else if (c.ch[i].psm == PSM_HID_INTR) {
        c.hid_intr = i;
        kprintf("bt: teclado/mouse Bluetooth pronto\n");
    }
}

static void services_start(void) {
    u8 major = (u8)((c.cod >> 8) & 0x1F);
    c.st = 4;
    err[0] = 0;
    if (major == 0x04) { c.sig_ch = -1; c.media_ch = -1; l2_connect(PSM_AVDTP); }     /* áudio/vídeo: A2DP */
    else if (major == 0x05) { c.hid_ctrl = c.hid_intr = -1; l2_connect(PSM_HID_CTRL); }/* periférico: HID */
    else kprintf("bt: conectado (classe 0x%06x sem perfil suportado)\n", c.cod);
}

/* ---- API */
int  bt_classic_count(void) { return ndevs; }
bool bt_classic_device(int i, char *name, usize ncap, char *addr, u32 *cod, int *rssi) {
    if (i < 0 || i >= ndevs) return false;
    if (name) snprintf(name, ncap, "%s", devs[i].name[0] ? devs[i].name : "(sem nome)");
    if (addr) addr_str(devs[i].addr, addr);
    if (cod) *cod = devs[i].cod;
    if (rssi) *rssi = devs[i].rssi;
    return true;
}
const char *bt_classic_status(void) {
    static char s[128];
    static const char *n[] = {"livre", "conectando", "pareando", "cifrando", "conectado"};
    snprintf(s, sizeof s, "%s%s%s%s", n[c.st], c.av_st == 7 ? " — tocando áudio" : "", err[0] ? " — " : "", err);
    return s;
}
bool bt_a2dp_active(void) { return c.av_st == 7; }
void bt_a2dp_test_tone(void) { tone_left = 48000; }

bool bt_classic_scan(bool on) {
    if (!ready) return false;
    if (on) {
        ndevs = 0;
        u8 lap[5] = {0x33, 0x8B, 0x9E, 8, 0};                  /* GIAC, 10 s */
        inquiring = bt_hci_cmd(0x0401, lap, 5, 1000) == 0;
    } else if (inquiring) { bt_hci_cmd(0x0402, NULL, 0, 1000); inquiring = false; }
    return true;
}

bool bt_classic_connect(int i) {
    if (!ready || i < 0 || i >= ndevs || c.st) return false;
    if (inquiring) bt_classic_scan(false);
    memset(&c, 0, sizeof c);
    memcpy(c.addr, devs[i].addr, 6);
    c.cod = devs[i].cod;
    u8 p[13];
    memcpy(p, c.addr, 6);
    put16(p + 6, 0xCC18); p[8] = 0x01; p[9] = 0; put16(p + 10, 0); p[12] = 1;
    c.st = 1;
    err[0] = 0;
    if (bt_hci_cmd(0x0405, p, 13, 1000)) { fail("o controlador recusou a conexão"); c.st = 0; return false; }
    return true;
}

void bt_classic_forget(const char *addr_text) {
    for (int i = 0; i < ndevs; i++) {
        char s[18];
        addr_str(devs[i].addr, s);
        if (strcmp(s, addr_text)) continue;
        char p[96];
        key_path(devs[i].addr, p, sizeof p);
        vnode_t *v = vfs_lookup(p);
        if (v) vfs_delete(v);
    }
}

/* ---- eventos HCI (do laço principal, via ble.c) */
static cdev_t *dev_note(const u8 *addr, u32 cod, int rssi) {
    cdev_t *d = NULL;
    for (int i = 0; i < ndevs; i++) if (!memcmp(devs[i].addr, addr, 6)) d = &devs[i];
    if (!d && ndevs < MAX_DEVS) { d = &devs[ndevs++]; memset(d, 0, sizeof *d); memcpy(d->addr, addr, 6); }
    if (d) { d->cod = cod; d->rssi = rssi; }
    return d;
}

/* o envio de respostas usa bt_hci_cmd, que espera a confirmação: fica para o classic_poll */
#define PQ 8
static u8 pend[PQ][24];
static u8 pend_len[PQ];
static u16 pend_op[PQ];
static int pq;
static void defer(u16 op, const u8 *d, u8 len) { if (pq < PQ) { pend_op[pq] = op; memcpy(pend[pq], d, len); pend_len[pq] = len; pq++; } }

bool classic_event(const u8 *ev, int len) {
    const u8 *p = ev + 2;
    switch (ev[0]) {
    case 0x01: inquiring = false; kprintf("bt: varredura clássica terminou: %d dispositivo(s)\n", ndevs); return true;
    case 0x02: case 0x22: {
        bool rssi = ev[0] == 0x22;
        for (int n = 0, k = 1; n < p[0] && k + 14 <= len; n++, k += rssi ? 14 : 14) {
            const u8 *r = p + k;
            dev_note(r, (u32)r[9] | (u32)r[10] << 8 | (u32)r[11] << 16, rssi ? (i8)r[13] : 0);
        }
        return true;
    }
    case 0x2F: {                                               /* resultado estendido, com o nome no EIR */
        if (len < 2 + 15) return true;
        cdev_t *d = dev_note(p + 1, (u32)p[9] | (u32)p[10] << 8 | (u32)p[11] << 16, (i8)p[14]);
        const u8 *eir = p + 15;
        for (int i = 0; d && i + 1 < 240 && eir[i]; i += 1 + eir[i])
            if ((eir[i + 1] == 0x08 || eir[i + 1] == 0x09) && eir[i] - 1 < (int)sizeof d->name) { memcpy(d->name, eir + i + 2, eir[i] - 1); d->name[eir[i] - 1] = 0; }
        return true;
    }
    case 0x07:                                                 /* nome remoto */
        for (int i = 0; i < ndevs; i++) if (!memcmp(devs[i].addr, p + 1, 6) && !p[0]) snprintf(devs[i].name, sizeof devs[i].name, "%.47s", (const char *)p + 7);
        return true;
    case 0x03:                                                 /* conexão concluída */
        if (p[9] != 0x01 || memcmp(p + 3, c.addr, 6)) return false;   /* não é ACL nosso */
        if (p[0]) { char m[64]; snprintf(m, sizeof m, "falha ao conectar (HCI 0x%02x)", p[0]); fail(m); c.st = 0; return true; }
        c.handle = le16(p + 1) & 0x0FFF;
        c.st = 2;
        { u8 h[2]; put16(h, c.handle); defer(0x0411, h, 2); }  /* autenticação (pareia se preciso) */
        return true;
    case 0x04: {                                               /* o dispositivo quer conectar (ex.: teclado acordando) */
        char kp[96];
        key_path(p, kp, sizeof kp);
        bool known = vfs_lookup(kp) != NULL;
        u8 r[7];
        memcpy(r, p, 6);
        if (known && !c.st) {
            memset(&c, 0, sizeof c);
            memcpy(c.addr, p, 6);
            c.cod = (u32)p[6] | (u32)p[7] << 8 | (u32)p[8] << 16;
            c.st = 1;
            r[6] = 0x01;
            defer(0x0409, r, 7);
        } else { r[6] = 0x0F; defer(0x040A, r, 7); }           /* recusa */
        return true;
    }
    case 0x17: {                                               /* pedido de chave de enlace */
        char kp[96];
        key_path(p, kp, sizeof kp);
        vnode_t *v = vfs_lookup(kp);
        if (v && v->data && v->size >= 16) { u8 r[22]; memcpy(r, p, 6); memcpy(r + 6, v->data, 16); defer(0x040B, r, 22); }
        else defer(0x040C, p, 6);
        return true;
    }
    case 0x18: {                                               /* nova chave de enlace: guarda */
        char kp[96];
        u8 b[20];
        memcpy(b, p + 6, 16);
        b[16] = (u8)c.cod; b[17] = (u8)(c.cod >> 8); b[18] = (u8)(c.cod >> 16); b[19] = p[22];
        key_path(p, kp, sizeof kp);
        vfs_mkdir(KEY_DIR);
        vfs_write_file(kp, b, 20);
        kprintf("bt: pareado, chave de enlace salva\n");
        return true;
    }
    case 0x31: { u8 r[9]; memcpy(r, p, 6); r[6] = 0x03; r[7] = 0x00; r[8] = 0x04; defer(0x042B, r, 9); return true; }   /* sem E/S, bonding geral */
    case 0x33: defer(0x042C, p, 6); return true;               /* confirma o "Just Works" */
    case 0x16: { u8 r[23] = {0}; memcpy(r, p, 6); r[6] = 4; memcpy(r + 7, "0000", 4); defer(0x040D, r, 23); return true; }  /* PIN legado */
    case 0x06:                                                 /* autenticação concluída */
        if (le16(p + 1) != c.handle) return false;
        if (p[0]) { char m[64]; snprintf(m, sizeof m, "pareamento falhou (HCI 0x%02x)", p[0]); fail(m); return true; }
        c.st = 3;
        { u8 r[3]; put16(r, c.handle); r[2] = 1; defer(0x0413, r, 3); }
        return true;
    case 0x08:                                                 /* cifragem */
        if (le16(p + 1) != c.handle) return false;
        if (p[0] || !p[3]) { fail("não foi possível cifrar o enlace"); return true; }
        services_start();
        return true;
    case 0x05:
        if (le16(p + 1) != c.handle || !c.st) return false;
        kprintf("bt: dispositivo clássico desconectado (0x%02x)\n", p[3]);
        if (audio_tap == tap) { audio_tap = NULL; audio_route_bluetooth = false; }
        memset(&c, 0, sizeof c);
        return true;
    }
    return false;
}

bool classic_acl(const u8 *a, int len) {
    if (!c.st || len < 8 || (le16(a) & 0x0FFF) != c.handle) return false;
    u16 l2len = le16(a + 4), cid = le16(a + 6);
    const u8 *d = a + 8;
    if (8 + l2len > len) return true;
    if (cid == 0x0001) { l2_signal(d, l2len); return true; }
    int i = ch_by_scid(cid);
    if (i < 0) return true;
    if (c.ch[i].psm == PSM_AVDTP && i == c.sig_ch) av_rx(d, l2len);
    else if (c.ch[i].psm == PSM_HID_INTR) hid_rx(d, l2len);
    return true;
}

void classic_poll(void) {
    for (int i = 0; i < pq; i++) bt_hci_cmd(pend_op[i], pend[i], pend_len[i], 1000);
    pq = 0;
    av_pump();
}

void classic_init(void) {
    u8 one = 1, mode = 2;
    bt_hci_cmd(0x0C56, &one, 1, 1000);                         /* Simple Secure Pairing */
    bt_hci_cmd(0x0C45, &mode, 1, 1000);                        /* inquiry com EIR (nomes) */
    u8 mask[8] = {0xFF, 0xFF, 0xFB, 0xFF, 0x07, 0xF8, 0xBF, 0x7D};   /* + EIR e LE Meta */
    bt_hci_cmd(0x0C01, mask, 8, 1000);
    ready = true;
    /* reconecta ao primeiro dispositivo clássico vinculado (fone/teclado) */
    vnode_t *d = vfs_lookup(KEY_DIR);
    for (vnode_t *f = d && d->dir ? d->child : NULL; f; f = f->next) {
        usize l = strlen(f->name);
        if (l < 23 || strcmp(f->name + l - 6, ".chave") || !f->data || f->size < 19) continue;
        u8 a[6];
        for (int k = 0; k < 6; k++) {                          /* nome "AA-BB-CC-DD-EE-FF.chave" */
            unsigned v = 0;
            for (int h = 0; h < 2; h++) { char ch = f->name[k * 3 + h]; v = v * 16 + (u32)(ch <= '9' ? ch - '0' : (ch | 32) - 'a' + 10); }
            a[5 - k] = (u8)v;
        }
        if (ndevs < MAX_DEVS) {
            cdev_t *x = &devs[ndevs++];
            memset(x, 0, sizeof *x);
            memcpy(x->addr, a, 6);
            x->cod = (u32)f->data[16] | (u32)f->data[17] << 8 | (u32)f->data[18] << 16;
            snprintf(x->name, sizeof x->name, "Dispositivo vinculado");
            bt_classic_connect(ndevs - 1);
        }
        break;
    }
}
