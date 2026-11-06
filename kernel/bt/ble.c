/*
 * mOS — Bluetooth Low Energy (central): varredura, conexão, pareamento SMP legado "Just Works",
 * cliente GATT e HID sobre GATT (HOGP) para mouses e teclados BLE (ex.: Logitech MX).
 *
 * Fluxo: varre -> conecta -> (vínculo salvo? cifra com a LTK : pareia, cifra com a STK e recebe a LTK)
 *        -> MTU -> acha o serviço HID (0x1812) -> lê o Report Map -> liga as notificações dos
 *        relatórios de entrada -> cada notificação vira evento de mouse/teclado.
 * Vínculos ficam em /Sistema/Biblioteca/Bluetooth/<endereço>.vinculo (reconecta sozinho).
 *
 * Os callbacks do HCI só enfileiram; os envios acontecem em bt_stack_poll (laço principal).
 */
#include "bluetooth.h"
#include "vfs.h"
#include "../include/hid.h"

#define MAX_DEVS 24
#define BOND_DIR "/Sistema/Biblioteca/Bluetooth"
#define CID_ATT  0x0004
#define CID_SMP  0x0006

void smp_c1(const u8 *k, const u8 *r, const u8 *preq, const u8 *pres, u8 iat, const u8 *ia, u8 rat, const u8 *ra, u8 *out);
void smp_s1(const u8 *k, const u8 *r1, const u8 *r2, u8 *out);
void smp_f4(const u8 *u, const u8 *v, const u8 *x, u8 z, u8 *out);
void smp_f5(const u8 *w, const u8 *n1, const u8 *n2, const u8 *a1, const u8 *a2, u8 *mackey, u8 *ltk);
void smp_f6(const u8 *w, const u8 *n1, const u8 *n2, const u8 *r, const u8 *io, const u8 *a1, const u8 *a2, u8 *out);
void p256_public(const u8 *priv, u8 *x, u8 *y);
bool p256_ecdh(const u8 *priv, const u8 *px, const u8 *py, u8 *dh);
static void rev(u8 *d, const u8 *s, int n) { for (int i = 0; i < n; i++) d[i] = s[n - 1 - i]; }
int bt_hci_cmd_resp(u16 opcode, const void *p, u8 len, u8 *resp, int max, int timeout_ms);   /* bluetooth.c (ou btintel.c, fraca) */
__attribute__((weak)) void input_hid_keyboard(u8 mods, const u8 keys[6]) { (void)mods; (void)keys; }   /* dev/input.c */

typedef struct {
    u8 addr[6], type;           /* little-endian, como no HCI */
    char name[32];
    int rssi;
    bool hid;                   /* anuncia o serviço HID ou aparência de mouse/teclado */
    bool pairable;              /* anúncio "descobrível" (modo de pareamento) */
    u16 appearance;
} bt_le_dev_t;

static bt_le_dev_t devs[MAX_DEVS];
static int ndevs;
static bool scanning, ready;
static u64 auto_until;          /* varredura automática do primeiro uso */
static u8 my_addr[6];

/* ---- conexão atual (uma por vez) */
enum { C_IDLE, C_CONNECTING, C_PAIRING, C_ENCRYPTING, C_GATT, C_READY };
static struct {
    int st;
    u16 handle;
    u8 addr[6], type;
    u8 preq[7], pres[7], mrand[16], sconf[16], stk[16];
    u8 ltk[16], rand[8];
    u16 ediv;
    /* LE Secure Connections (P-256) */
    bool sc;
    u8 priv[32], pkx[32], pky[32], peerx[32], peery[32], dhkey[32], na[16], cb[16], mackey[16], sc_ltk[16];
    bool bonded, have_ltk;
    u16 mtu;
    /* GATT */
    int gstep;
    u16 hid_start, hid_end, next;
    u16 map_handle, report_h[8], cccd_h[8], refid[8];
    int nreports, cur;
    u8 map[1024];
    int maplen;
    hid_mouse_t mouse;
    hid_kbd_t kbd;
    bool has_mouse, has_kbd;
    u16 mouse_handle, kbd_handle;
    u64 t;
    char err[96];
} c;

/* ---- fila de eventos/ACL recebidos nos callbacks */
#define Q 32
static u8 *qd[Q];
static u16 ql[Q];
static u8 qk[Q];               /* 0 evento, 1 ACL */
static int qh, qt;
static void enqueue(u8 kind, const u8 *d, u32 len) {
    int nx = (qh + 1) % Q;
    if (nx == qt || len > 1100) return;
    u8 *b = kmalloc(len);
    if (!b) return;
    memcpy(b, d, len);
    qd[qh] = b; ql[qh] = (u16)len; qk[qh] = kind;
    qh = nx;
}
/* tudo menos Command Complete/Status (o bluetooth.c já trata) e contagem de pacotes */
static void on_event(const u8 *ev, u32 len) { if (len >= 2 && ev[0] != 0x0E && ev[0] != 0x0F && ev[0] != 0x13) enqueue(0, ev, len); }
bool classic_event(const u8 *ev, int len);
bool classic_acl(const u8 *a, int len);
void classic_poll(void);
void classic_init(void);
static void on_acl(const u8 *acl, u32 len) { enqueue(1, acl, len); }

/* ---- utilidades */
static u16 le16(const u8 *p) { return (u16)(p[0] | p[1] << 8); }
static void put16(u8 *p, u16 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); }
static void addr_str(const u8 *a, char *o) { snprintf(o, 18, "%02X:%02X:%02X:%02X:%02X:%02X", a[5], a[4], a[3], a[2], a[1], a[0]); }
static void rnd(u8 *p, int n) { for (int i = 0; i < n; i++) p[i] = (u8)(rand32() ^ (u32)(rdtsc() >> (i & 7))); }
static void fail(const char *m) { snprintf(c.err, sizeof c.err, "%s", m); kprintf("ble: %s\n", m); }

static bool l2cap_send(u16 cid, const u8 *pdu, u16 len) {
    u8 b[64];
    if (len + 4 > (int)sizeof b) return false;
    put16(b, len);
    put16(b + 2, cid);
    memcpy(b + 4, pdu, len);
    return bt_acl_send(c.handle, b, (u16)(len + 4));
}

/* ---- vínculos */
static void bond_path(const u8 *a, char *p, usize n) {
    char s[18];
    addr_str(a, s);
    for (char *q = s; *q; q++) if (*q == ':') *q = '-';
    snprintf(p, n, BOND_DIR "/%s.vinculo", s);
}
static void bond_save(void) {
    u8 b[64];
    memcpy(b, c.addr, 6); b[6] = c.type;
    memcpy(b + 7, c.ltk, 16); memcpy(b + 23, c.rand, 8); put16(b + 31, c.ediv);
    char p[96];
    bond_path(c.addr, p, sizeof p);
    vfs_mkdir(BOND_DIR);
    vfs_write_file(p, b, 33);
    kprintf("ble: vínculo salvo\n");
}
static bool bond_load(const u8 *a) {
    char p[96];
    bond_path(a, p, sizeof p);
    vnode_t *v = vfs_lookup(p);
    if (!v || !v->data || v->size < 33) return false;
    memcpy(c.ltk, v->data + 7, 16); memcpy(c.rand, v->data + 23, 8); c.ediv = le16(v->data + 31);
    return true;
}

/* ---- API */
int  bt_le_device_count(void) { return ndevs; }
bool bt_le_device(int i, char *name, usize ncap, char *addr, int *rssi, bool *hid) {
    if (i < 0 || i >= ndevs) return false;
    if (name) snprintf(name, ncap, "%s", devs[i].name[0] ? devs[i].name : "(sem nome)");
    if (addr) addr_str(devs[i].addr, addr);
    if (rssi) *rssi = devs[i].rssi;
    if (hid) *hid = devs[i].hid;
    return true;
}
const char *bt_le_status(void) {
    static char s[128];
    static const char *n[] = {"livre", "conectando", "pareando", "cifrando", "configurando", "conectado"};
    snprintf(s, sizeof s, "%s%s%s", n[c.st], c.err[0] ? " — " : "", c.err);
    return s;
}

bool bt_le_scan(bool on) {
    if (!ready) return false;
    if (on) {
        ndevs = 0;
        u8 sp[7] = {0x01, 0x60, 0x00, 0x30, 0x00, 0x00, 0x00};     /* ativa, 60 ms / 30 ms, endereço público */
        bt_hci_cmd(0x200B, sp, 7, 1000);
    }
    u8 se[2] = {on ? 1 : 0, 0};
    scanning = bt_hci_cmd(0x200C, se, 2, 1000) == 0 && on;
    return true;
}

static bool create_conn(const u8 *addr, u8 type) {
    u8 p[25] = {0};
    put16(p, 0x0060); put16(p + 2, 0x0030);
    p[4] = 0; p[5] = type;
    memcpy(p + 6, addr, 6);
    p[12] = 0;                                                     /* nosso endereço: público */
    put16(p + 13, 0x0006); put16(p + 15, 0x000C);                  /* intervalo 7,5..15 ms */
    put16(p + 17, 0); put16(p + 19, 0x00C8);                       /* latência 0, supervisão 2 s */
    if (bt_hci_cmd(0x200D, p, 25, 1000)) { fail("o controlador recusou a conexão"); return false; }
    memcpy(c.addr, addr, 6);
    c.type = type;
    c.st = C_CONNECTING;
    c.t = timer_ticks();
    return true;
}

bool bt_le_connect(int i) {
    if (!ready || i < 0 || i >= ndevs || c.st != C_IDLE) return false;
    if (scanning) bt_le_scan(false);
    c.err[0] = 0;
    return create_conn(devs[i].addr, devs[i].type);
}

void bt_le_forget(const char *addr_text) {
    for (int i = 0; i < ndevs; i++) {
        char s[18];
        addr_str(devs[i].addr, s);
        if (strcmp(s, addr_text)) continue;
        char p[96];
        bond_path(devs[i].addr, p, sizeof p);
        vnode_t *v = vfs_lookup(p);
        if (v) vfs_delete(v);
    }
}

/* ---- SMP (iniciador, pareamento legado Just Works) */
static void smp_start(void) {
    /* sem E/S, bonding + Secure Connections; no legado, a LTK vem do periférico */
    u8 p[7] = {0x01, 0x03, 0x00, 0x09, 16, 0x00, 0x01};
    memcpy(c.preq, p, 7);
    c.st = C_PAIRING;
    c.sc = false;
    l2cap_send(CID_SMP, p, 7);
}

/* endereços no formato das funções f5/f6: [tipo][endereço MSB primeiro] */
static void sc_addr(u8 out[7], u8 type, const u8 *addr_le) { out[0] = type; rev(out + 1, addr_le, 6); }

/* Pairing Random do periférico (SC, Just Works): confere Cb, deriva MacKey/LTK e envia Ea.
 * Tudo que entra nas funções f4/f5/f6 vai em MSB primeiro (rev() converte do formato dos pacotes). */
static bool sc_on_random(const u8 nb_le[16]) {
    u8 pkax[32], pkbx[32], na[16], nb[16], cb[16], chk[16], w[32], a1[7], a2[7], io[3], r0[16] = {0}, ea[16], pkt[17], ltk[16];
    rev(pkax, c.pkx, 32);
    rev(pkbx, c.peerx, 32);
    rev(na, c.na, 16);
    rev(nb, nb_le, 16);
    rev(cb, c.cb, 16);
    smp_f4(pkbx, pkax, nb, 0, chk);
    if (memcmp(chk, cb, 16)) return false;
    rev(w, c.dhkey, 32);
    sc_addr(a1, 0, my_addr);
    sc_addr(a2, c.type, c.addr);
    smp_f5(w, na, nb, a1, a2, c.mackey, ltk);
    rev(c.sc_ltk, ltk, 16);                                         /* o HCI usa a LTK em little-endian */
    memcpy(c.sconf, nb, 16);                                        /* guarda Nb (MSB) para conferir Eb */
    io[0] = c.preq[3]; io[1] = c.preq[2]; io[2] = c.preq[1];       /* AuthReq || OOB || IOcap */
    smp_f6(c.mackey, na, nb, r0, io, a1, a2, ea);
    pkt[0] = 0x0D;
    rev(pkt + 1, ea, 16);
    l2cap_send(CID_SMP, pkt, 17);
    return true;
}

static bool sc_on_dhkey_check(const u8 eb_le[16]) {
    u8 na[16], a1[7], a2[7], io[3], r0[16] = {0}, eb[16], want[16];
    rev(na, c.na, 16);
    rev(eb, eb_le, 16);
    sc_addr(a1, 0, my_addr);
    sc_addr(a2, c.type, c.addr);
    io[0] = c.pres[3]; io[1] = c.pres[2]; io[2] = c.pres[1];
    smp_f6(c.mackey, c.sconf, na, r0, io, a2, a1, want);
    return !memcmp(want, eb, 16);
}

static void start_encryption(const u8 *ltk, const u8 *rand8, u16 ediv) {
    u8 p[28];
    put16(p, c.handle);
    memcpy(p + 2, rand8, 8);
    put16(p + 10, ediv);
    memcpy(p + 12, ltk, 16);
    c.st = C_ENCRYPTING;
    bt_hci_cmd(0x2019, p, 28, 1000);
}

static void smp_rx(const u8 *p, int len) {
    static const u8 zero16[16];
    switch (p[0]) {
    case 0x02:                                                     /* Pairing Response */
        if (len < 7) return;
        memcpy(c.pres, p, 7);
        if ((c.preq[3] & 0x08) && (c.pres[3] & 0x08)) {           /* os dois lados falam Secure Connections */
            c.sc = true;
            rnd(c.priv, 32);
            c.priv[31] &= 0x7F;                                     /* < n (ordem da curva) */
            p256_public(c.priv, c.pkx, c.pky);
            u8 pk[65];
            pk[0] = 0x0C;
            memcpy(pk + 1, c.pkx, 32);
            memcpy(pk + 33, c.pky, 32);
            /* 65 bytes: maior que o MTU de 23 do canal SMP? Não: o MTU mínimo do SMP com SC é 65 */
            u8 b[69];
            put16(b, 65); put16(b + 2, CID_SMP);
            memcpy(b + 4, pk, 65);
            bt_acl_send(c.handle, b, 69);
            kprintf("ble: pareamento LE Secure Connections (P-256)\n");
            return;
        }
        rnd(c.mrand, 16);
        { u8 conf[17]; conf[0] = 0x03;
          smp_c1(zero16, c.mrand, c.preq, c.pres, 0, my_addr, c.type, c.addr, conf + 1);
          l2cap_send(CID_SMP, conf, 17); }
        return;
    case 0x0C:                                                     /* Pairing Public Key do periférico (SC) */
        if (!c.sc || len < 65) return;
        memcpy(c.peerx, p + 1, 32);
        memcpy(c.peery, p + 33, 32);
        if (!p256_ecdh(c.priv, c.peerx, c.peery, c.dhkey)) { u8 f[2] = {0x05, 0x0B}; l2cap_send(CID_SMP, f, 2); fail("chave pública do dispositivo inválida"); return; }
        rnd(c.na, 16);
        return;                                                    /* agora o periférico manda o Confirm (Cb) */
    case 0x0D:                                                     /* DHKey Check do periférico (Eb) */
        if (!c.sc || len < 17) return;
        if (!sc_on_dhkey_check(p + 1)) { u8 f[2] = {0x05, 0x0B}; l2cap_send(CID_SMP, f, 2); fail("verificação DHKey falhou"); return; }
        { static const u8 z8[8]; memcpy(c.ltk, c.sc_ltk, 16); memset(c.rand, 0, 8); c.ediv = 0; c.have_ltk = true; start_encryption(c.sc_ltk, z8, 0); }
        return;
    case 0x03:                                                     /* Pairing Confirm do periférico */
        if (len < 17) return;
        if (c.sc) {                                                /* SC: Cb chegou, mandamos Na */
            memcpy(c.cb, p + 1, 16);
            u8 r[17]; r[0] = 0x04; memcpy(r + 1, c.na, 16); l2cap_send(CID_SMP, r, 17);
            return;
        }
        memcpy(c.sconf, p + 1, 16);
        { u8 r[17]; r[0] = 0x04; memcpy(r + 1, c.mrand, 16); l2cap_send(CID_SMP, r, 17); }
        return;
    case 0x04: {                                                   /* Pairing Random do periférico */
        if (len < 17) return;
        if (c.sc) {
            if (!sc_on_random(p + 1)) { u8 f[2] = {0x05, 0x04}; l2cap_send(CID_SMP, f, 2); fail("confirmação SC não confere"); }
            return;                                                /* espera o DHKey Check (Eb) */
        }
        u8 chk[16];
        smp_c1(zero16, p + 1, c.preq, c.pres, 0, my_addr, c.type, c.addr, chk);
        if (memcmp(chk, c.sconf, 16)) { u8 f[2] = {0x05, 0x04}; l2cap_send(CID_SMP, f, 2); fail("confirmação do pareamento não confere"); return; }
        smp_s1(zero16, p + 1, c.mrand, c.stk);
        int ks = c.pres[4] < 16 ? c.pres[4] : 16;                  /* tamanho de chave negociado */
        memset(c.stk + ks, 0, 16 - ks);
        static const u8 z8[8];
        start_encryption(c.stk, z8, 0);
        return;
    }
    case 0x05: {                                                   /* Pairing Failed */
        char m[96];
        u8 reason = len >= 2 ? p[1] : 0;
        snprintf(m, sizeof m, reason == 0x0C || reason == 0x03 ?
                 "o dispositivo exige LE Secure Connections (ainda não suportado) [%u]" : "pareamento recusado pelo dispositivo [%u]", reason);
        fail(m);
        return;
    }
    case 0x06: if (len >= 17) { memcpy(c.ltk, p + 1, 16); } return;               /* Encryption Information */
    case 0x07: if (len >= 11) { c.ediv = le16(p + 1); memcpy(c.rand, p + 3, 8); c.have_ltk = true; bond_save(); } return;
    case 0x0B:                                                     /* Security Request: o periférico quer parear */
        if (c.st == C_GATT || c.st == C_READY) { if (bond_load(c.addr)) start_encryption(c.ltk, c.rand, c.ediv); else smp_start(); }
        return;
    default: return;
    }
}

/* ---- GATT (cliente) */
static void att_send(const u8 *p, u16 len) { l2cap_send(CID_ATT, p, len); }
static void att_read_by_type(u16 s, u16 e, u16 uuid) { u8 p[7] = {0x08}; put16(p + 1, s); put16(p + 3, e); put16(p + 5, uuid); att_send(p, 7); }

static void gatt_next(void);

static void gatt_finish(void) {
    if (c.map_handle && c.maplen) {
        c.has_mouse = hid_parse_mouse(c.map, c.maplen, &c.mouse);
        c.has_kbd = hid_parse_keyboard(c.map, c.maplen, &c.kbd);
    }
    for (int i = 0; i < c.nreports; i++) {
        if (c.has_mouse && c.refid[i] == c.mouse.report_id && !c.mouse_handle) c.mouse_handle = c.report_h[i];
        if (c.has_kbd && c.refid[i] == c.kbd.report_id && !c.kbd_handle) c.kbd_handle = c.report_h[i];
    }
    if (c.has_mouse && !c.mouse_handle && c.nreports) c.mouse_handle = c.report_h[0];
    c.st = C_READY;
    kprintf("ble: pronto — %s%s%s (relatórios %d, mapa %d bytes)\n", c.has_mouse ? "mouse" : "", c.has_mouse && c.has_kbd ? " + " : "",
            c.has_kbd ? "teclado" : "", c.nreports, c.maplen);
}

/* passos: 1 MTU, 2 serviços, 3 características, 4 descritores de cada relatório, 5 Report Reference,
 * 6 Report Map (longo), 7 liga as notificações */
static void gatt_next(void) {
    switch (c.gstep) {
    case 1: { u8 p[3] = {0x02}; put16(p + 1, 247); att_send(p, 3); return; }
    case 2: { u8 p[7] = {0x10}; put16(p + 1, c.next ? c.next : 1); put16(p + 3, 0xFFFF); put16(p + 5, 0x2800); att_send(p, 7); return; }
    case 3: att_read_by_type(c.next, c.hid_end, 0x2803); return;
    case 4:
        if (c.cur >= c.nreports) { c.gstep = 5; c.cur = 0; gatt_next(); return; }
        { u8 p[5] = {0x04}; put16(p + 1, c.report_h[c.cur] + 1); put16(p + 3, c.cur + 1 < c.nreports ? c.report_h[c.cur + 1] - 2 : c.hid_end); att_send(p, 5); }
        return;
    case 5:
        if (c.cur >= c.nreports) { c.gstep = 6; c.maplen = 0; gatt_next(); return; }
        if (!c.refid[c.cur]) { c.cur++; gatt_next(); return; }    /* refid guarda o handle do descritor até ser lido */
        { u8 p[3] = {0x0A}; put16(p + 1, c.refid[c.cur]); att_send(p, 3); }
        return;
    case 6:
        if (!c.map_handle) { c.gstep = 7; c.cur = 0; gatt_next(); return; }
        if (!c.maplen) { u8 p[3] = {0x0A}; put16(p + 1, c.map_handle); att_send(p, 3); }
        else { u8 p[5] = {0x0C}; put16(p + 1, c.map_handle); put16(p + 3, (u16)c.maplen); att_send(p, 5); }
        return;
    case 7:
        while (c.cur < c.nreports && !c.cccd_h[c.cur]) c.cur++;
        if (c.cur >= c.nreports) { gatt_finish(); return; }
        { u8 p[5] = {0x12}; put16(p + 1, c.cccd_h[c.cur]); put16(p + 3, 0x0001); att_send(p, 5); }
        return;
    }
}

static void hid_input(u16 handle, const u8 *v, int len) {
    if (c.has_mouse && handle == c.mouse_handle) {
        int b, dx, dy, dz;
        hid_mouse_decode(&c.mouse, v, len, &b, &dx, &dy, &dz);
        input_event_t e = {0};
        e.type = EV_MOUSE;
        e.buttons = (u8)(b & 7);
        e.dx = dx; e.dy = dy; e.dz = -dz;
        e.mods = input_mods();
        input_push(&e);
    } else if (c.has_kbd && handle == c.kbd_handle && len >= (c.kbd.keys_bit + 48) / 8) {
        u8 keys[6] = {0};
        for (int i = 0; i < 6 && i < c.kbd.keys_count; i++) keys[i] = v[c.kbd.keys_bit / 8 + i];
        input_hid_keyboard(c.kbd.mods_bit >= 0 ? v[c.kbd.mods_bit / 8] : 0, keys);
    }
}

static void att_rx(const u8 *p, int len) {
    u8 op = p[0];
    if (op == 0x1B && len >= 3) { hid_input(le16(p + 1), p + 3, len - 3); return; }              /* notificação */
    if (op == 0x1D && len >= 3) { u8 cfm = 0x1E; att_send(&cfm, 1); hid_input(le16(p + 1), p + 3, len - 3); return; }
    if (op == 0x02 || op == 0x08 || op == 0x0A || op == 0x10 || op == 0x12 || op == 0x04) {   /* pedido do periférico */
        u8 e[5] = {0x01, op, 0, 0, 0x06};                           /* "pedido não suportado" */
        if (op == 0x02 && len >= 3) { u8 r[3] = {0x03}; put16(r + 1, 23); att_send(r, 3); return; }
        att_send(e, 5);
        return;
    }
    bool err = op == 0x01;
    switch (c.gstep) {
    case 1: if (op == 0x03 || err) { c.mtu = op == 0x03 ? MIN(le16(p + 1), 247) : 23; c.gstep = 2; c.next = 1; gatt_next(); } return;
    case 2:
        if (op == 0x11 && len >= 2) {
            int el = p[1];
            u16 last = 0;
            for (int i = 2; i + el <= len; i += el) {
                u16 s = le16(p + i), e = le16(p + i + 2);
                if (el == 6 && le16(p + i + 4) == 0x1812 && !c.hid_start) { c.hid_start = s; c.hid_end = e; }
                last = e;
            }
            if (last && last != 0xFFFF) { c.next = (u16)(last + 1); gatt_next(); return; }
        }
        if (!c.hid_start) { fail("o dispositivo não tem serviço HID"); c.st = C_READY; return; }
        c.gstep = 3; c.next = c.hid_start; gatt_next();
        return;
    case 3:
        if (op == 0x09 && len >= 2) {
            int el = p[1];
            u16 last = 0;
            for (int i = 2; i + el <= len; i += el) {
                u16 decl = le16(p + i);
                u8 props = p[i + 2];
                u16 val = le16(p + i + 3);
                u16 uuid = el == 7 ? le16(p + i + 5) : 0;
                if (uuid == 0x2A4B) c.map_handle = val;
                if (uuid == 0x2A4D && (props & 0x10) && c.nreports < 8) c.report_h[c.nreports++] = val;
                last = decl;
            }
            if (last && last < c.hid_end) { c.next = (u16)(last + 1); gatt_next(); return; }
        }
        c.gstep = 4; c.cur = 0; gatt_next();
        return;
    case 4:
        if (op == 0x05 && len >= 2 && p[1] == 1)                    /* UUIDs de 16 bits */
            for (int i = 2; i + 4 <= len; i += 4) {
                u16 h = le16(p + i), u = le16(p + i + 2);
                if (u == 0x2902) c.cccd_h[c.cur] = h;
                if (u == 0x2908) c.refid[c.cur] = h;
            }
        c.cur++;
        gatt_next();
        return;
    case 5:
        c.refid[c.cur] = op == 0x0B && len >= 2 ? p[1] : 0;         /* Report Reference: [ID, tipo] */
        c.cur++;
        gatt_next();
        return;
    case 6:
        if ((op == 0x0B || op == 0x0D) && len > 1) {
            int k = MIN(len - 1, (int)sizeof c.map - c.maplen);
            memcpy(c.map + c.maplen, p + 1, k);
            c.maplen += k;
            if (len - 1 >= c.mtu - 1 && c.maplen < (int)sizeof c.map) { gatt_next(); return; }   /* pode haver mais */
        }
        c.gstep = 7; c.cur = 0; gatt_next();
        return;
    case 7: c.cur++; gatt_next(); return;
    }
}

/* ---- eventos e dados recebidos (no laço principal) */
static void parse_adv(const u8 *r, int len) {
    /* LE Advertising Report: num, [tipo de evento, tipo de endereço, endereço(6), len, dados, rssi] */
    if (len < 1) return;
    const u8 *p = r + 1;
    for (int n = r[0]; n > 0 && p + 9 <= r + len; n--) {
        u8 at = p[1];
        const u8 *addr = p + 2;
        u8 dl = p[8];
        const u8 *ad = p + 9;
        if (ad + dl + 1 > r + len) return;
        int rssi = (i8)ad[dl];
        bt_le_dev_t *d = NULL;
        for (int i = 0; i < ndevs; i++) if (!memcmp(devs[i].addr, addr, 6)) d = &devs[i];
        if (!d && ndevs < MAX_DEVS) { d = &devs[ndevs++]; memset(d, 0, sizeof *d); memcpy(d->addr, addr, 6); d->type = at; }
        if (d) {
            d->rssi = rssi;
            for (int i = 0; i + 1 < dl && i + 1 + ad[i] <= dl; i += 1 + ad[i]) {
                u8 l = ad[i], t = ad[i + 1];
                if (!l) break;
                if ((t == 0x08 || t == 0x09) && l - 1 < (int)sizeof d->name) { memcpy(d->name, ad + i + 2, l - 1); d->name[l - 1] = 0; }
                if (t == 0x01 && l >= 2 && (ad[i + 2] & 0x03)) d->pairable = true;   /* descobrível limitado/geral */
                if (t == 0x19 && l >= 3) { d->appearance = le16(ad + i + 2); if ((d->appearance >> 6) == 0x0F) d->hid = true; }
                if ((t == 0x02 || t == 0x03) && l >= 3)
                    for (int k = 2; k + 1 < l + 1; k += 2) if (le16(ad + i + k) == 0x1812) d->hid = true;
            }
        }
        p = ad + dl + 1;
    }
}

static void handle_event(const u8 *ev, int len) {
    if (ev[0] != 0x3E && classic_event(ev, len)) return;       /* eventos do Bluetooth clássico */
    if (ev[0] == 0x3E && len >= 3) {
        u8 sub = ev[2];
        const u8 *p = ev + 3;
        if (sub == 0x02) parse_adv(p, len - 3);
        else if ((sub == 0x01 || sub == 0x0A) && len >= 3 + 12) {     /* conexão (normal ou melhorada) */
            if (p[0]) { char m[64]; snprintf(m, sizeof m, "falha ao conectar (HCI 0x%02x)", p[0]); fail(m); c.st = C_IDLE; return; }
            c.handle = le16(p + 1) & 0x0FFF;
            c.mtu = 23;
            c.gstep = 0;
            char a[18];
            addr_str(c.addr, a);
            kprintf("ble: conectado a %s (handle %u)\n", a, c.handle);
            c.bonded = bond_load(c.addr);
            if (c.bonded) start_encryption(c.ltk, c.rand, c.ediv);
            else smp_start();
        }
    } else if (ev[0] == 0x08 && len >= 6 && le16(ev + 3) == c.handle) {   /* Encryption Change */
        if (ev[2] || !ev[5]) {
            if (c.bonded) { kprintf("ble: vínculo antigo recusado, pareando de novo\n"); c.bonded = false; smp_start(); }
            else fail("não foi possível cifrar o enlace");
            return;
        }
        kprintf("ble: enlace cifrado%s\n", c.sc ? " (LE Secure Connections)" : "");
        if (c.sc && c.have_ltk && !c.bonded) bond_save();           /* no SC a LTK é derivada, não enviada */
        c.st = C_GATT;
        c.gstep = 1;
        c.t = timer_ticks();
        gatt_next();
    } else if (ev[0] == 0x05 && len >= 6 && le16(ev + 3) == c.handle) {   /* Disconnection Complete */
        kprintf("ble: desconectado (motivo 0x%02x)\n", ev[5]);
        u8 a[6], t = c.type;
        memcpy(a, c.addr, 6);
        bool had = c.st == C_READY && (c.bonded || c.have_ltk);
        memset(&c, 0, sizeof c);
        if (had) create_conn(a, t);                                   /* volta sozinho quando o mouse acordar */
    }
}

static void handle_acl(const u8 *a, int len) {
    if (len < 8) return;
    u16 h = le16(a) & 0x0FFF, l2len = le16(a + 4), cid = le16(a + 6);
    if (h != c.handle || !c.st) { classic_acl(a, len); return; }   /* outro enlace: clássico */
    if (8 + l2len > len) return;
    if (cid == CID_SMP) smp_rx(a + 8, l2len);
    else if (cid == CID_ATT) att_rx(a + 8, l2len);
}

void bt_stack_poll(void) {
    if (!ready) return;
    while (qt != qh) {
        u8 *d = qd[qt];
        if (qk[qt]) handle_acl(d, ql[qt]); else handle_event(d, ql[qt]);
        kfree(d);
        qt = (qt + 1) % Q;
    }
    classic_poll();
    if (c.st == C_GATT && timer_ticks() - c.t > 15000) { fail("o dispositivo parou de responder durante a configuração"); c.st = C_READY; }
    /* primeiro uso (nenhum vínculo): varre 30 s e pareia com o 1º mouse/teclado em modo de pareamento */
    if (auto_until) {
        if (c.st == C_IDLE)
            for (int i = 0; i < ndevs; i++)
                if (devs[i].hid && devs[i].pairable) {
                    char a[18];
                    addr_str(devs[i].addr, a);
                    kprintf("ble: pareando automaticamente com \"%s\" (%s)\n", devs[i].name, a);
                    auto_until = 0;
                    bt_le_connect(i);
                    return;
                }
        if (timer_ticks() > auto_until) {
            auto_until = 0;
            bt_le_scan(false);
            kprintf("ble: varredura inicial terminou: %d dispositivo(s) LE\n", ndevs);
            for (int i = 0; i < ndevs; i++) {
                char a[18];
                addr_str(devs[i].addr, a);
                kprintf("ble:   %s \"%s\" %d dBm%s%s\n", a, devs[i].name, devs[i].rssi, devs[i].hid ? " HID" : "", devs[i].pairable ? " (pareável)" : "");
            }
        }
    }
}

/* chamado pelo bluetooth.c quando o controlador está pronto (depois do firmware e do HCI_Reset) */
void bt_stack_init(void) {
    u8 r[16];
    if (bt_hci_cmd_resp(0x1009, NULL, 0, r, sizeof r, 1000) >= 7 && !r[0]) memcpy(my_addr, r + 1, 6);
    u8 mask[8] = {0xFF, 0xFF, 0xFB, 0xFF, 0x07, 0xF8, 0xBF, 0x3D};   /* inclui o LE Meta Event */
    bt_hci_cmd(0x0C01, mask, 8, 1000);
    u8 lemask[8] = {0x1F, 0x02, 0, 0, 0, 0, 0, 0};                   /* conexão, anúncios, conexão melhorada */
    bt_hci_cmd(0x2001, lemask, 8, 1000);
    bt_on_event = on_event;
    bt_on_acl = on_acl;
    ready = true;
    classic_init();                                            /* SSP, EIR, reconexão de fones/teclados */
    char a[18];
    addr_str(my_addr, a);
    kprintf("ble: pronto (endereço %s)\n", a);
    /* reconecta ao primeiro dispositivo vinculado; sem nenhum, varre e pareia sozinho (primeiro uso) */
    vnode_t *d = vfs_lookup(BOND_DIR);
    for (vnode_t *f = d && d->dir ? d->child : NULL; f; f = f->next)
        if (f->data && f->size >= 33) { create_conn(f->data, f->data[6]); return; }
    kprintf("ble: nenhum dispositivo vinculado — procurando mouse/teclado em modo de pareamento por 30 s\n");
    bt_le_scan(true);
    auto_until = timer_ticks() + 30000;
}
