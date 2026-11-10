/*
 * mOS — Bluetooth Low Energy (central): varredura, conexão, pareamento SMP (legado e LE Secure
 * Connections, "Just Works" ou passkey), cliente GATT e HID sobre GATT (HOGP) para mouses e teclados BLE
 * (ex.: Logitech MX). Até MAXC enlaces ao mesmo tempo (teclado + mouse).
 *
 * Fluxo: varre -> conecta -> (vínculo salvo? cifra com a LTK : pareia, cifra com a STK/LTK)
 *        -> MTU -> acha o serviço HID (0x1812) -> lê o Report Map -> liga as notificações dos
 *        relatórios de entrada -> cada notificação vira evento de mouse/teclado.
 * Vínculos ficam em /Sistema/Biblioteca/Bluetooth/<endereço>.vinculo: endereço(6) tipo(1) LTK(16) Rand(8)
 * EDIV(2) e, opcional, o nome (até 31 bytes, terminado em 0). Os vinculados que não estão
 * conectados ficam na lista de aceitação do controlador e uma única conexão pendente (filtro 1) espera
 * o primeiro que aparecer; assim um mouse dormindo não segura a reconexão do teclado.
 *
 * Pareamento: anunciamos IO "só tela" (DisplayOnly) sem exigir MITM. Mouse (sem E/S) cai em Just Works;
 * teclado que exige MITM cai em passkey: o mOS mostra 6 dígitos (notify) e a pessoa digita no teclado.
 *
 * Os callbacks do HCI só enfileiram; os envios acontecem em bt_stack_poll (laço principal).
 */
#include "bluetooth.h"
#include "vfs.h"
#include "../include/hid.h"
#include "../include/ui.h"

#define MAX_DEVS 24
#define MAXC     4
#define BOND_DIR "/Sistema/Biblioteca/Bluetooth"
#define CID_ATT  0x0004
#define CID_SMP  0x0006
#define SMP_IOCAP 0x00   /* DisplayOnly */

void smp_c1(const u8 *k, const u8 *r, const u8 *preq, const u8 *pres, u8 iat, const u8 *ia, u8 rat, const u8 *ra, u8 *out);
void smp_s1(const u8 *k, const u8 *r1, const u8 *r2, u8 *out);
void smp_f4(const u8 *u, const u8 *v, const u8 *x, u8 z, u8 *out);
void smp_f5(const u8 *w, const u8 *n1, const u8 *n2, const u8 *a1, const u8 *a2, u8 *mackey, u8 *ltk);
void smp_f6(const u8 *w, const u8 *n1, const u8 *n2, const u8 *r, const u8 *io, const u8 *a1, const u8 *a2, u8 *out);
void p256_public(const u8 *priv, u8 *x, u8 *y);
bool p256_ecdh(const u8 *priv, const u8 *px, const u8 *py, u8 *dh);
void crypto_random(void *buf, usize len);                  /* net/crypto.c: DRBG ChaCha20 */
static void rev(u8 *d, const u8 *s, int n) { for (int i = 0; i < n; i++) d[i] = s[n - 1 - i]; }
int bt_hci_cmd_resp(u16 opcode, const void *p, u8 len, u8 *resp, int max, int timeout_ms);   /* bluetooth.c (ou btintel.c, fraca) */
__attribute__((weak)) void input_hid_keyboard(u8 mods, const u8 keys[6]) { (void)mods; (void)keys; }   /* dev/usb_hid.c */
__attribute__((weak)) void notify(const char *title, const char *body) { (void)title; (void)body; }  /* ui/desktop.c */

typedef struct {
    u8 addr[6], type;           /* little-endian, como no HCI */
    char name[32];
    int rssi;
    bool hid;                   /* anuncia o serviço HID ou aparência de mouse/teclado */
    bool pairable;              /* anúncio "descobrível" (modo de pareamento) */
    u8 tries;                   /* tentativas do pareamento automático */
    u16 appearance;
} bt_le_dev_t;

static bt_le_dev_t devs[MAX_DEVS];
static int ndevs;
static bool scanning, ready;
static bool advertising;          /* CBPeripheralManager anunciando: a próxima conexão de entrada vira canal ATT cru */
static bool periph_role;          /* o enlace cru atual é como periférico (o central é o outro lado) */
static u64 auto_until;            /* janela de pareamento automático */
static u8 my_addr[6];
static char last_err[96];

/* ---- enlaces */
enum { C_IDLE, C_CONNECTING, C_PAIRING, C_ENCRYPTING, C_GATT, C_READY };
typedef struct {
    int st;
    bool bg;                    /* conexão pendente pela lista de aceitação (reconexão) */
    u16 handle;
    u8 addr[6], type;
    char name[32];
    u8 preq[7], pres[7], mrand[16], sconf[16], stk[16], tk[16];
    u8 ltk[16], rand[8];
    u16 ediv;
    /* LE Secure Connections (P-256) */
    bool sc;
    u8 priv[32], pkx[32], pky[32], peerx[32], peery[32], dhkey[32], na[16], cb[16], mackey[16], sc_ltk[16];
    /* passkey (o mOS mostra, o teclado digita) */
    bool passkey;
    u32 pk;
    int round;                  /* SC: rodada 0..19 (um bit da passkey por rodada) */
    bool bonded, have_ltk;
    int resume;                 /* estado a retomar depois de recifrar um enlace já configurado */
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
    bool raw;               /* conexão do CoreBluetooth: sem HID, o GATT fica no app (DEV_BT_*) */
    char err[96];
} conn_t;
static conn_t conns[MAXC];
static conn_t *c = &conns[0];     /* enlace do evento/pacote sendo tratado */
static conn_t *rawc;              /* enlace do canal ATT cru (CoreBluetooth), se houver */
static int want_dev = -1;         /* conexão pedida enquanto a reconexão em segundo plano é cancelada */
static bool want_raw;
static u64 bg_next;

static conn_t *conn_by_handle(u16 h) {
    for (int i = 0; i < MAXC; i++)
        if (conns[i].st > C_CONNECTING && conns[i].handle == h) return &conns[i];
    return NULL;
}
static conn_t *conn_pending(void) {
    for (int i = 0; i < MAXC; i++) if (conns[i].st == C_CONNECTING) return &conns[i];
    return NULL;
}
static conn_t *conn_free(void) {
    for (int i = 0; i < MAXC; i++) if (conns[i].st == C_IDLE) return &conns[i];
    return NULL;
}
static bool conn_has_addr(const u8 *a) {
    for (int i = 0; i < MAXC; i++) if (conns[i].st > C_CONNECTING && !memcmp(conns[i].addr, a, 6)) return true;
    return false;
}

/* ---- canal ATT cru do CoreBluetooth (DEV_BT_* do GUI_DEVICE): PDUs recebidas esperando o app */
#define RAWQ 48
static u8 raw_pdu[RAWQ][260];
static u16 raw_len[RAWQ];
static int raw_rh, raw_rt;
static bool raw_failed, raw_gone;
static void raw_push(const u8 *p, int n) {
    int nx = (raw_rh + 1) % RAWQ;
    if (nx == raw_rt || n <= 0 || n > 260) return;
    memcpy(raw_pdu[raw_rh], p, (usize)n);
    raw_len[raw_rh] = (u16)n;
    raw_rh = nx;
}

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
static void rnd(u8 *p, int n) { crypto_random(p, (usize)n); }
static void fail(const char *m) {
    snprintf(c->err, sizeof c->err, "%s", m);
    snprintf(last_err, sizeof last_err, "%s", m);
    kprintf("ble: %s\n", m);
}
static const char *dev_name(const u8 *a) {
    for (int i = 0; i < MAXC; i++) if (conns[i].st && !memcmp(conns[i].addr, a, 6) && conns[i].name[0]) return conns[i].name;
    for (int i = 0; i < ndevs; i++) if (!memcmp(devs[i].addr, a, 6) && devs[i].name[0]) return devs[i].name;
    return NULL;
}

static bool l2cap_send(u16 cid, const u8 *pdu, u16 len) {
    u8 b[72];
    if (len + 4 > (int)sizeof b) return false;
    put16(b, len);
    put16(b + 2, cid);
    memcpy(b + 4, pdu, len);
    return bt_acl_send(c->handle, b, (u16)(len + 4));
}

/* ---- vínculos */
static void bond_path(const u8 *a, char *p, usize n) {
    char s[18];
    addr_str(a, s);
    for (char *q = s; *q; q++) if (*q == ':') *q = '-';
    snprintf(p, n, BOND_DIR "/%s.vinculo", s);
}
static void bond_save(void) {
    u8 b[66] = {0};
    memcpy(b, c->addr, 6); b[6] = c->type;
    memcpy(b + 7, c->ltk, 16); memcpy(b + 23, c->rand, 8); put16(b + 31, c->ediv);
    const char *nm = c->name[0] ? c->name : dev_name(c->addr);
    usize nl = nm ? MIN(strlen(nm), (usize)31) : 0;
    if (nl) memcpy(b + 33, nm, nl);
    char p[96];
    bond_path(c->addr, p, sizeof p);
    vfs_mkdir(BOND_DIR);
    vfs_write_file(p, b, 33 + (nl ? nl + 1 : 0));
    kprintf("ble: vínculo salvo\n");
}
static bool bond_load(const u8 *a) {
    char p[96];
    bond_path(a, p, sizeof p);
    vnode_t *v = vfs_lookup(p);
    if (!v || !v->data || v->size < 33) return false;
    memcpy(c->ltk, v->data + 7, 16); memcpy(c->rand, v->data + 23, 8); c->ediv = le16(v->data + 31);
    if (v->size > 34 && !c->name[0]) {                             /* nome salvo (vínculos novos) */
        usize nl = MIN(v->size - 33, sizeof c->name - 1);
        memcpy(c->name, v->data + 33, nl);
        c->name[nl] = 0;
    }
    return true;
}
static bool is_bonded(const u8 *a) {
    char p[96];
    bond_path(a, p, sizeof p);
    vnode_t *v = vfs_lookup(p);
    return v && v->data && v->size >= 33;
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
    static char s[256];
    static const char *n[] = {"livre", "conectando", "pareando", "cifrando", "configurando", "conectado"};
    int k = 0;
    s[0] = 0;
    for (int i = 0; i < MAXC; i++) {
        conn_t *x = &conns[i];
        if (x->st <= C_CONNECTING || k >= (int)sizeof s - 2) continue;
        char a[18];
        addr_str(x->addr, a);
        const char *nm = dev_name(x->addr);
        k += snprintf(s + k, sizeof s - k, "%s%s: %s%s%s", k ? "; " : "", nm ? nm : a, n[x->st], x->err[0] ? " — " : "", x->err);
    }
    if (!k) snprintf(s, sizeof s, "%s%s%s", conn_pending() ? "conectando" : "livre", last_err[0] ? " — " : "", last_err);
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

/* uma conexão pendente por vez (regra do HCI). accept = usa a lista de aceitação (addr ignorado). */
static bool create_conn(const u8 *addr, u8 type, bool accept) {
    if (conn_pending()) return false;
    conn_t *n = conn_free();
    if (!n) { snprintf(last_err, sizeof last_err, "limite de %d dispositivos conectados", MAXC); return false; }
    u8 p[25] = {0};
    put16(p, 0x0060); put16(p + 2, 0x0030);
    p[4] = accept ? 1 : 0;
    p[5] = type;
    if (!accept) memcpy(p + 6, addr, 6);
    p[12] = 0;                                                     /* nosso endereço: público */
    put16(p + 13, 0x0006); put16(p + 15, 0x000C);                  /* intervalo 7,5..15 ms */
    put16(p + 17, 0); put16(p + 19, 0x00C8);                       /* latência 0, supervisão 2 s */
    int st = bt_hci_cmd(0x200D, p, 25, 1000);
    if (st) {
        if (!accept) snprintf(last_err, sizeof last_err, "o controlador recusou a conexão (0x%02x)", st);
        return false;
    }
    memset(n, 0, sizeof *n);
    if (!accept) { memcpy(n->addr, addr, 6); n->type = type; }
    n->bg = accept;
    n->st = C_CONNECTING;
    n->t = timer_ticks();
    c = n;
    return true;
}

/* conexão pedida (UI, pareamento automático, CoreBluetooth): cancela a reconexão em segundo plano se preciso */
static bool request_conn(int i, bool raw) {
    conn_t *pend = conn_pending();
    if (pend && !pend->bg) return false;
    if (!conn_free() && !pend) return false;
    if (pend) {                                                    /* o Connection Complete 0x02 libera o slot */
        bt_hci_cmd(0x200E, NULL, 0, 1000);
        want_dev = i;
        want_raw = raw;
        return true;
    }
    if (!create_conn(devs[i].addr, devs[i].type, false)) return false;
    c->raw = raw;
    if (raw) rawc = c;
    return true;
}

bool bt_le_connect(int i) {
    if (!ready || i < 0 || i >= ndevs || conn_has_addr(devs[i].addr)) return false;
    if (scanning) bt_le_scan(false);
    last_err[0] = 0;
    return request_conn(i, false);
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

/* reconexão: põe na lista de aceitação os vinculados que não estão conectados e espera o primeiro */
static void bg_reconnect(void) {
    if (conn_pending() || !conn_free() || want_dev >= 0 || timer_ticks() < bg_next) return;
    bg_next = timer_ticks() + 2000;
    vnode_t *d = vfs_lookup(BOND_DIR);
    if (!d || !d->dir) return;
    if (bt_hci_cmd(0x2010, NULL, 0, 1000)) return;                 /* limpa a lista de aceitação */
    int n = 0;
    for (vnode_t *f = d->child; f; f = f->next) {
        if (!f->data || f->size < 33 || conn_has_addr(f->data)) continue;
        u8 p[7];
        p[0] = f->data[6] ? 1 : 0;
        memcpy(p + 1, f->data, 6);
        if (!bt_hci_cmd(0x2011, p, 7, 1000)) n++;
    }
    if (n) create_conn(NULL, 0, true);
}

/* ---- SMP (iniciador) */
static void smp_start(void) {
    /* só tela (DisplayOnly), sem OOB, bonding + Secure Connections, sem exigir MITM; o periférico distribui a LTK */
    u8 p[7] = {0x01, SMP_IOCAP, 0x00, 0x09, 16, 0x00, 0x01};
    memcpy(c->preq, p, 7);
    c->st = C_PAIRING;
    c->sc = false;
    c->passkey = false;
    c->round = 0;
    l2cap_send(CID_SMP, p, 7);
}
/* pareamento falhou: derruba o enlace (motivo 0x05, falha de autenticação) para liberar a vaga e o aparelho */
static void smp_drop(void) {
    u8 q[3];
    put16(q, c->handle);
    q[2] = 0x05;
    bt_hci_cmd(0x0406, q, 3, 1000);
}
static void smp_fail(u8 reason, const char *m) {
    u8 f[2] = {0x05, reason};
    l2cap_send(CID_SMP, f, 2);
    fail(m);
    smp_drop();
}

/* método de pareamento (Core 5.x, Vol 3, Parte H, 2.3.5.1): com MITM pedido por algum lado, nós só com
 * tela e o periférico com teclado (KeyboardOnly/KeyboardDisplay) = passkey; o resto = Just Works */
static bool use_passkey(const u8 *preq, const u8 *pres) {
    bool mitm = ((preq[3] | pres[3]) & 0x04) != 0;
    return mitm && preq[1] == 0x00 && (pres[1] == 0x02 || pres[1] == 0x04);
}

static void show_passkey(void) {
    char t[96];
    const char *nm = dev_name(c->addr);
    snprintf(t, sizeof t, "Digite %06u no teclado%s%s e tecle Enter", c->pk, nm ? " " : "", nm ? nm : "");
    snprintf(c->err, sizeof c->err, "%s", t);
    kprintf("ble: passkey: %s\n", t);
    notify("Parear teclado Bluetooth", t);
}

/* endereços no formato das funções f5/f6: [tipo][endereço MSB primeiro] */
static void sc_addr(u8 out[7], u8 type, const u8 *addr_le) { out[0] = type; rev(out + 1, addr_le, 6); }
/* r de f6: passkey como inteiro de 128 bits (MSB primeiro), ou zero no Just Works */
static void sc_r(u8 r[16]) {
    memset(r, 0, 16);
    if (c->passkey) { r[13] = (u8)(c->pk >> 16); r[14] = (u8)(c->pk >> 8); r[15] = (u8)c->pk; }
}
static u8 sc_z(void) { return c->passkey ? (u8)(0x80 | ((c->pk >> c->round) & 1)) : 0; }

/* passkey SC: Nai novo e Cai = f4(PKax, PKbx, Nai, 0x80|ri) */
static void sc_send_confirm(void) {
    u8 pkax[32], pkbx[32], na[16], ca[16], pkt[17];
    rnd(c->na, 16);
    rev(pkax, c->pkx, 32);
    rev(pkbx, c->peerx, 32);
    rev(na, c->na, 16);
    smp_f4(pkax, pkbx, na, sc_z(), ca);
    pkt[0] = 0x03;
    rev(pkt + 1, ca, 16);
    l2cap_send(CID_SMP, pkt, 17);
}

/* Cb confere com Nb? (Just Works: z = 0; passkey: z = 0x80|ri) */
static bool sc_check(const u8 nb_le[16]) {
    u8 pkax[32], pkbx[32], nb[16], cb[16], chk[16];
    rev(pkax, c->pkx, 32);
    rev(pkbx, c->peerx, 32);
    rev(nb, nb_le, 16);
    rev(cb, c->cb, 16);
    smp_f4(pkbx, pkax, nb, sc_z(), chk);
    return !memcmp(chk, cb, 16);
}

/* fim das rodadas: deriva MacKey/LTK e envia Ea */
static void sc_finish(const u8 nb_le[16]) {
    u8 na[16], nb[16], w[32], a1[7], a2[7], io[3], r[16], ea[16], pkt[17], ltk[16];
    rev(na, c->na, 16);
    rev(nb, nb_le, 16);
    rev(w, c->dhkey, 32);
    sc_addr(a1, 0, my_addr);
    sc_addr(a2, c->type, c->addr);
    smp_f5(w, na, nb, a1, a2, c->mackey, ltk);
    rev(c->sc_ltk, ltk, 16);                                        /* o HCI usa a LTK em little-endian */
    memcpy(c->sconf, nb, 16);                                       /* guarda Nb (MSB) para conferir Eb */
    io[0] = c->preq[3]; io[1] = c->preq[2]; io[2] = c->preq[1];    /* AuthReq || OOB || IOcap */
    sc_r(r);
    smp_f6(c->mackey, na, nb, r, io, a1, a2, ea);
    pkt[0] = 0x0D;
    rev(pkt + 1, ea, 16);
    l2cap_send(CID_SMP, pkt, 17);
}

static bool sc_on_dhkey_check(const u8 eb_le[16]) {
    u8 na[16], a1[7], a2[7], io[3], r[16], eb[16], want[16];
    rev(na, c->na, 16);
    rev(eb, eb_le, 16);
    sc_addr(a1, 0, my_addr);
    sc_addr(a2, c->type, c->addr);
    io[0] = c->pres[3]; io[1] = c->pres[2]; io[2] = c->pres[1];
    sc_r(r);
    smp_f6(c->mackey, c->sconf, na, r, io, a2, a1, want);
    return !memcmp(want, eb, 16);
}

static void start_encryption(const u8 *ltk, const u8 *rand8, u16 ediv) {
    u8 p[28];
    put16(p, c->handle);
    memcpy(p + 2, rand8, 8);
    put16(p + 10, ediv);
    memcpy(p + 12, ltk, 16);
    if (c->st >= C_GATT) c->resume = c->st;
    c->st = C_ENCRYPTING;
    bt_hci_cmd(0x2019, p, 28, 1000);
}

static void smp_rx(const u8 *p, int len) {
    if (len < 1) return;
    switch (p[0]) {
    case 0x02:                                                     /* Pairing Response */
        if (len < 7) return;
        memcpy(c->pres, p, 7);
        c->passkey = use_passkey(c->preq, c->pres);
        if (c->passkey) {
            u8 b[4];
            rnd(b, 4);
            c->pk = (u32)(b[0] | b[1] << 8 | b[2] << 16 | (u32)b[3] << 24) % 1000000u;
            memset(c->tk, 0, 16);
            c->tk[0] = (u8)c->pk; c->tk[1] = (u8)(c->pk >> 8); c->tk[2] = (u8)(c->pk >> 16);
            show_passkey();
        } else memset(c->tk, 0, 16);                               /* Just Works: TK = 0 */
        if ((c->preq[3] & 0x08) && (c->pres[3] & 0x08)) {         /* os dois lados falam Secure Connections */
            c->sc = true;
            rnd(c->priv, 32);
            c->priv[31] &= 0x7F;                                    /* < n (ordem da curva) */
            p256_public(c->priv, c->pkx, c->pky);
            u8 pk[65];
            pk[0] = 0x0C;
            memcpy(pk + 1, c->pkx, 32);
            memcpy(pk + 33, c->pky, 32);
            l2cap_send(CID_SMP, pk, 65);                            /* o MTU mínimo do SMP com SC é 65 */
            kprintf("ble: pareamento LE Secure Connections (P-256, %s)\n", c->passkey ? "passkey" : "Just Works");
            return;
        }
        rnd(c->mrand, 16);
        { u8 conf[17]; conf[0] = 0x03;
          smp_c1(c->tk, c->mrand, c->preq, c->pres, 0, my_addr, c->type, c->addr, conf + 1);
          l2cap_send(CID_SMP, conf, 17); }
        return;
    case 0x0C:                                                     /* Pairing Public Key do periférico (SC) */
        if (!c->sc || len < 65) return;
        memcpy(c->peerx, p + 1, 32);
        memcpy(c->peery, p + 33, 32);
        if (!p256_ecdh(c->priv, c->peerx, c->peery, c->dhkey)) { smp_fail(0x0B, "chave pública do dispositivo inválida"); return; }
        if (c->passkey) { c->round = 0; sc_send_confirm(); }       /* passkey: o iniciador manda o Confirm primeiro */
        else rnd(c->na, 16);                                       /* Just Works: o periférico manda o Confirm (Cb) */
        return;
    case 0x0D:                                                     /* DHKey Check do periférico (Eb) */
        if (!c->sc || len < 17) return;
        if (!sc_on_dhkey_check(p + 1)) { smp_fail(0x0B, "verificação DHKey falhou"); return; }
        { static const u8 z8[8]; memcpy(c->ltk, c->sc_ltk, 16); memset(c->rand, 0, 8); c->ediv = 0; c->have_ltk = true; start_encryption(c->sc_ltk, z8, 0); }
        return;
    case 0x03:                                                     /* Pairing Confirm do periférico */
        if (len < 17) return;
        if (c->sc) {                                               /* SC: Cb chegou, mandamos Na */
            memcpy(c->cb, p + 1, 16);
            u8 r[17]; r[0] = 0x04; memcpy(r + 1, c->na, 16); l2cap_send(CID_SMP, r, 17);
            return;
        }
        memcpy(c->sconf, p + 1, 16);
        { u8 r[17]; r[0] = 0x04; memcpy(r + 1, c->mrand, 16); l2cap_send(CID_SMP, r, 17); }
        return;
    case 0x04: {                                                   /* Pairing Random do periférico */
        if (len < 17) return;
        if (c->sc) {
            if (!sc_check(p + 1)) { smp_fail(0x04, c->passkey ? "passkey errada" : "confirmação SC não confere"); return; }
            if (c->passkey && ++c->round < 20) { sc_send_confirm(); return; }   /* próxima rodada */
            sc_finish(p + 1);
            return;                                                /* espera o DHKey Check (Eb) */
        }
        u8 chk[16];
        smp_c1(c->tk, p + 1, c->preq, c->pres, 0, my_addr, c->type, c->addr, chk);
        if (memcmp(chk, c->sconf, 16)) { smp_fail(0x04, c->passkey ? "passkey errada" : "confirmação do pareamento não confere"); return; }
        smp_s1(c->tk, p + 1, c->mrand, c->stk);
        int ks = c->pres[4] < 16 ? c->pres[4] : 16;                /* tamanho de chave negociado */
        memset(c->stk + ks, 0, 16 - ks);
        static const u8 z8[8];
        start_encryption(c->stk, z8, 0);
        return;
    }
    case 0x05: {                                                   /* Pairing Failed */
        char m[96];
        u8 reason = len >= 2 ? p[1] : 0;
        snprintf(m, sizeof m, reason == 0x04 && c->passkey ? "passkey digitada errada; tente de novo [%u]" :
                 reason == 0x01 ? "passkey não digitada ou cancelada [%u]" :
                 reason == 0x03 || reason == 0x0C ? "o dispositivo exige um modo de pareamento que o mOS não oferece [%u]" :
                 "pareamento recusado pelo dispositivo [%u]", reason);
        fail(m);
        smp_drop();
        return;
    }
    case 0x06: if (len >= 17) { memcpy(c->ltk, p + 1, 16); } return;              /* Encryption Information */
    case 0x07: if (len >= 11) { c->ediv = le16(p + 1); memcpy(c->rand, p + 3, 8); c->have_ltk = true; bond_save(); } return;
    case 0x0B:                                                     /* Security Request: o periférico quer parear */
        if (c->st == C_GATT || c->st == C_READY) { if (bond_load(c->addr)) start_encryption(c->ltk, c->rand, c->ediv); else smp_start(); }
        return;
    default: return;
    }
}

/* ---- GATT (cliente) */
static void att_send(const u8 *p, u16 len) { l2cap_send(CID_ATT, p, len); }
static void att_read_by_type(u16 s, u16 e, u16 uuid) { u8 p[7] = {0x08}; put16(p + 1, s); put16(p + 3, e); put16(p + 5, uuid); att_send(p, 7); }

static void gatt_next(void);

static void gatt_finish(void) {
    if (c->map_handle && c->maplen) {
        c->has_mouse = hid_parse_mouse(c->map, c->maplen, &c->mouse);
        c->has_kbd = hid_parse_keyboard(c->map, c->maplen, &c->kbd);
    }
    for (int i = 0; i < c->nreports; i++) {
        if (c->has_mouse && c->refid[i] == c->mouse.report_id && !c->mouse_handle) c->mouse_handle = c->report_h[i];
        if (c->has_kbd && c->refid[i] == c->kbd.report_id && !c->kbd_handle) c->kbd_handle = c->report_h[i];
    }
    if (c->has_mouse && !c->mouse_handle && c->nreports) c->mouse_handle = c->report_h[0];
    c->st = C_READY;
    c->err[0] = 0;
    kprintf("ble: pronto — %s%s%s (relatórios %d, mapa %d bytes)\n", c->has_mouse ? "mouse" : "", c->has_mouse && c->has_kbd ? " + " : "",
            c->has_kbd ? "teclado" : "", c->nreports, c->maplen);
}

/* passos: 1 MTU, 2 serviços, 3 características, 4 descritores de cada relatório, 5 Report Reference,
 * 6 Report Map (longo), 7 liga as notificações */
static void gatt_next(void) {
    switch (c->gstep) {
    case 1: { u8 p[3] = {0x02}; put16(p + 1, 247); att_send(p, 3); return; }
    case 2: { u8 p[7] = {0x10}; put16(p + 1, c->next ? c->next : 1); put16(p + 3, 0xFFFF); put16(p + 5, 0x2800); att_send(p, 7); return; }
    case 3: att_read_by_type(c->next, c->hid_end, 0x2803); return;
    case 4:
        if (c->cur >= c->nreports) { c->gstep = 5; c->cur = 0; gatt_next(); return; }
        { u8 p[5] = {0x04}; put16(p + 1, c->report_h[c->cur] + 1); put16(p + 3, c->cur + 1 < c->nreports ? c->report_h[c->cur + 1] - 2 : c->hid_end); att_send(p, 5); }
        return;
    case 5:
        if (c->cur >= c->nreports) { c->gstep = 6; c->maplen = 0; gatt_next(); return; }
        if (!c->refid[c->cur]) { c->cur++; gatt_next(); return; }    /* refid guarda o handle do descritor até ser lido */
        { u8 p[3] = {0x0A}; put16(p + 1, c->refid[c->cur]); att_send(p, 3); }
        return;
    case 6:
        if (!c->map_handle) { c->gstep = 7; c->cur = 0; gatt_next(); return; }
        if (!c->maplen) { u8 p[3] = {0x0A}; put16(p + 1, c->map_handle); att_send(p, 3); }
        else { u8 p[5] = {0x0C}; put16(p + 1, c->map_handle); put16(p + 3, (u16)c->maplen); att_send(p, 5); }
        return;
    case 7:
        while (c->cur < c->nreports && !c->cccd_h[c->cur]) c->cur++;
        if (c->cur >= c->nreports) { gatt_finish(); return; }
        { u8 p[5] = {0x12}; put16(p + 1, c->cccd_h[c->cur]); put16(p + 3, 0x0001); att_send(p, 5); }
        return;
    }
}

static void hid_input(u16 handle, const u8 *v, int len) {
    if (c->has_mouse && handle == c->mouse_handle) {
        int b, dx, dy, dz;
        hid_mouse_decode(&c->mouse, v, len, &b, &dx, &dy, &dz);
        input_event_t e = {0};
        e.type = EV_MOUSE;
        e.buttons = (u8)(b & 7);
        e.dx = dx; e.dy = dy; e.dz = -dz;
        e.mods = input_mods();
        input_push(&e);
    } else if (c->has_kbd && handle == c->kbd_handle && len >= (c->kbd.keys_bit + 48) / 8) {
        u8 keys[6] = {0};
        for (int i = 0; i < 6 && i < c->kbd.keys_count; i++) keys[i] = v[c->kbd.keys_bit / 8 + i];
        input_hid_keyboard(c->kbd.mods_bit >= 0 ? v[c->kbd.mods_bit / 8] : 0, keys);
    }
}

static void att_rx(const u8 *p, int len) {
    if (len < 1) return;
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
    switch (c->gstep) {
    case 1: if (op == 0x03 || err) { c->mtu = op == 0x03 && len >= 3 ? MIN(le16(p + 1), 247) : 23; c->gstep = 2; c->next = 1; gatt_next(); } return;
    case 2:
        if (op == 0x11 && len >= 2) {
            int el = p[1];
            u16 last = 0;
            for (int i = 2; el >= 4 && i + el <= len; i += el) {
                u16 s = le16(p + i), e = le16(p + i + 2);
                if (el == 6 && le16(p + i + 4) == 0x1812 && !c->hid_start) { c->hid_start = s; c->hid_end = e; }
                last = e;
            }
            if (last && last != 0xFFFF) { c->next = (u16)(last + 1); gatt_next(); return; }
        }
        if (!c->hid_start) { fail("o dispositivo não tem serviço HID"); c->st = C_READY; return; }
        c->gstep = 3; c->next = c->hid_start; gatt_next();
        return;
    case 3:
        if (op == 0x09 && len >= 2) {
            int el = p[1];
            u16 last = 0;
            for (int i = 2; el >= 5 && i + el <= len; i += el) {
                u16 decl = le16(p + i);
                u8 props = p[i + 2];
                u16 val = le16(p + i + 3);
                u16 uuid = el == 7 ? le16(p + i + 5) : 0;
                if (uuid == 0x2A4B) c->map_handle = val;
                if (uuid == 0x2A4D && (props & 0x10) && c->nreports < 8) c->report_h[c->nreports++] = val;
                last = decl;
            }
            if (last && last < c->hid_end) { c->next = (u16)(last + 1); gatt_next(); return; }
        }
        c->gstep = 4; c->cur = 0; gatt_next();
        return;
    case 4:
        if (op == 0x05 && len >= 2 && p[1] == 1)                    /* UUIDs de 16 bits */
            for (int i = 2; i + 4 <= len; i += 4) {
                u16 h = le16(p + i), u = le16(p + i + 2);
                if (u == 0x2902) c->cccd_h[c->cur] = h;
                if (u == 0x2908) c->refid[c->cur] = h;
            }
        c->cur++;
        gatt_next();
        return;
    case 5:
        c->refid[c->cur] = op == 0x0B && len >= 2 ? p[1] : 0;       /* Report Reference: [ID, tipo] */
        c->cur++;
        gatt_next();
        return;
    case 6:
        if ((op == 0x0B || op == 0x0D) && len > 1) {
            int k = MIN(len - 1, (int)sizeof c->map - c->maplen);
            memcpy(c->map + c->maplen, p + 1, k);
            c->maplen += k;
            if (len - 1 >= c->mtu - 1 && c->maplen < (int)sizeof c->map) { gatt_next(); return; }   /* pode haver mais */
        }
        c->gstep = 7; c->cur = 0; gatt_next();
        return;
    case 7: c->cur++; gatt_next(); return;
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

static void conn_complete(const u8 *p) {
    /* p: status, handle(2), papel, tipo do endereço, endereço(6), ... */
    bool periph = p[3] == 1 && advertising;
    conn_t *n = periph ? conn_free() : conn_pending();
    if (!n) return;
    c = n;
    if (p[0]) {
        bool cancelled = p[0] == 0x02 && n->bg;
        if (!cancelled) { char m[64]; snprintf(m, sizeof m, "falha ao conectar (HCI 0x%02x)", p[0]); fail(m); }
        if (n->raw) { raw_failed = true; rawc = NULL; }
        memset(n, 0, sizeof *n);
        return;
    }
    if (periph) {                                    /* papel periférico: um central conectou no nosso anúncio */
        advertising = false;                         /* o controlador para de anunciar ao conectar */
        memset(n, 0, sizeof *n);
        n->raw = true; periph_role = true; rawc = n;
        raw_failed = raw_gone = false; raw_rh = raw_rt = 0;
    }
    n->type = p[4];
    memcpy(n->addr, p + 5, 6);                       /* pela lista de aceitação, o endereço só vem aqui */
    const char *nm = dev_name(n->addr);
    if (nm && nm != n->name) snprintf(n->name, sizeof n->name, "%s", nm);
    n->handle = le16(p + 1) & 0x0FFF;
    n->mtu = 23;
    n->gstep = 0;
    n->bg = false;
    bg_next = 0;                                     /* os outros vinculados voltam para a lista de aceitação */
    char a[18];
    addr_str(n->addr, a);
    kprintf("ble: conectado a %s (handle %u)\n", a, n->handle);
    if (n->raw) { n->st = C_READY; raw_rh = raw_rt = 0; kprintf("ble: canal ATT do app pronto\n"); return; }
    n->st = C_PAIRING;
    n->bonded = bond_load(n->addr);
    if (n->bonded) start_encryption(n->ltk, n->rand, n->ediv);
    else smp_start();
}

static void handle_event(const u8 *ev, int len) {
    if (ev[0] != 0x3E && classic_event(ev, len)) return;       /* eventos do Bluetooth clássico */
    if (ev[0] == 0x3E && len >= 3) {
        u8 sub = ev[2];
        const u8 *p = ev + 3;
        if (sub == 0x02) parse_adv(p, len - 3);
        else if ((sub == 0x01 || sub == 0x0A) && len >= 3 + 12) conn_complete(p);   /* conexão (normal ou melhorada) */
    } else if (ev[0] == 0x08 && len >= 6) {                         /* Encryption Change */
        conn_t *x = conn_by_handle(le16(ev + 3));
        if (!x) return;
        c = x;
        if (ev[2] || !ev[5]) {
            if (c->bonded) { kprintf("ble: vínculo antigo recusado, pareando de novo\n"); c->bonded = false; smp_start(); }
            else fail("não foi possível cifrar o enlace");
            return;
        }
        kprintf("ble: enlace cifrado%s\n", c->sc ? " (LE Secure Connections)" : "");
        if (c->sc && c->have_ltk && !c->bonded) bond_save();        /* no SC a LTK é derivada, não enviada */
        if (c->raw) { c->st = C_READY; return; }       /* o app continua o GATT dele, agora cifrado */
        if (c->resume) { c->st = c->resume; c->resume = 0; return; }   /* recifragem de um enlace já configurado */
        c->st = C_GATT;
        c->gstep = 1;
        c->t = timer_ticks();
        gatt_next();
    } else if (ev[0] == 0x05 && len >= 6) {                         /* Disconnection Complete */
        conn_t *x = conn_by_handle(le16(ev + 3));
        if (!x) return;
        kprintf("ble: desconectado (motivo 0x%02x)\n", ev[5]);
        if (x->raw) { raw_gone = true; periph_role = false; rawc = NULL; }
        memset(x, 0, sizeof *x);
        bg_next = 0;                                                /* vinculado volta pela lista de aceitação */
    }
}

static void handle_acl(const u8 *a, int len) {
    if (len < 8) return;
    u16 h = le16(a) & 0x0FFF, l2len = le16(a + 4), cid = le16(a + 6);
    conn_t *x = conn_by_handle(h);
    if (!x) { classic_acl(a, len); return; }                    /* outro enlace: clássico */
    if (8 + l2len > len) return;
    c = x;
    if (cid == CID_SMP) smp_rx(a + 8, l2len);
    else if (cid == CID_ATT && c->raw) raw_push(a + 8, l2len);
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
    for (int i = 0; i < MAXC; i++)
        if (conns[i].st == C_GATT && timer_ticks() - conns[i].t > 15000) {
            c = &conns[i];
            fail("o dispositivo parou de responder durante a configuração");
            c->st = C_READY;
        }
    /* conexão pedida enquanto a reconexão em segundo plano era cancelada */
    if (want_dev >= 0 && !conn_pending()) {
        int i = want_dev;
        bool raw = want_raw;
        want_dev = -1;
        want_raw = false;
        if (i < ndevs && create_conn(devs[i].addr, devs[i].type, false)) { c->raw = raw; if (raw) rawc = c; }
        else if (raw) raw_failed = true;
    }
    /* pareamento automático: cada mouse/teclado em modo de pareamento ainda sem vínculo */
    if (auto_until) {
        if (!conn_pending() && want_dev < 0)
            for (int i = 0; i < ndevs; i++)
                if (devs[i].hid && devs[i].pairable && devs[i].tries < 3 && !conn_has_addr(devs[i].addr) && !is_bonded(devs[i].addr)) {
                    char a[18];
                    addr_str(devs[i].addr, a);
                    kprintf("ble: pareando automaticamente com \"%s\" (%s)\n", devs[i].name, a);
                    devs[i].tries++;
                    request_conn(i, false);
                    break;
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
    bg_reconnect();
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
    memset(conns, 0, sizeof conns);
    c = &conns[0];
    rawc = NULL;
    want_dev = -1;
    ready = true;
    classic_init();                                            /* SSP, EIR, reconexão de fones/teclados */
    char a[18];
    addr_str(my_addr, a);
    kprintf("ble: pronto (endereço %s)\n", a);
    /* os vinculados voltam pela lista de aceitação; em paralelo, 60 s (30 s se já há vínculo) pareando
     * sozinho cada mouse/teclado em modo de pareamento */
    vnode_t *d = vfs_lookup(BOND_DIR);
    bool any = false;
    for (vnode_t *f = d && d->dir ? d->child : NULL; f; f = f->next) if (f->data && f->size >= 33) any = true;
    bg_next = 0;
    bg_reconnect();
    kprintf("ble: %s — procurando mouse/teclado em modo de pareamento por %d s\n", any ? "reconectando os vinculados" : "nenhum dispositivo vinculado", any ? 30 : 60);
    bt_le_scan(true);
    auto_until = timer_ticks() + (any ? 30000 : 60000);
}

/* ---- CoreBluetooth (GUI_DEVICE, DEV_BT_*): varredura, conexão com canal ATT cru e as PDUs em si */
#include "proc.h"
#include "syscall.h"
#include "../include/compat_gui.h"
static bool raw_send(const u8 *pdu, u16 len) {
    u8 b[264];
    if (!rawc || len + 4 > (int)sizeof b) return false;
    put16(b, len);
    put16(b + 2, CID_ATT);
    memcpy(b + 4, pdu, len);
    return bt_acl_send(rawc->handle, b, (u16)(len + 4));
}
i64 bt_user_syscall(process_t *p, u64 op, u64 b, u64 cc, u64 d) {
    switch (op) {
    case DEV_BT_STATE: return ready ? 2 : 0;
    case DEV_BT_SCAN: return bt_le_scan(b != 0) ? 0 : -MOS_EINVAL;
    case DEV_BT_DEVICE: {
        if ((int)b >= ndevs) return 0;
        gui_bt_device_t o;
        memset(&o, 0, sizeof o);
        bt_le_dev_t *v = &devs[b];
        snprintf(o.name, sizeof o.name, "%s", v->name);
        addr_str(v->addr, o.addr);
        o.rssi = v->rssi; o.hid = v->hid; o.connectable = 1;
        return uvm_copy_out(p, cc, &o, sizeof o) ? 1 : -MOS_EFAULT;
    }
    case DEV_BT_CONNECT:
        if (!ready || (int)b >= ndevs || rawc || want_raw || conn_has_addr(devs[b].addr)) return -MOS_EINVAL;
        if (scanning) bt_le_scan(false);
        last_err[0] = 0;
        raw_failed = raw_gone = false;
        raw_rh = raw_rt = 0;
        if (!request_conn((int)b, true)) { raw_failed = true; return -MOS_EINVAL; }
        return 0;
    case DEV_BT_ADVERTISE: {
        /* b = dados do anúncio (AD structures, até 31 bytes), cc = tamanho, d = ligar */
        if (!ready) return -MOS_EINVAL;
        if (!d) { u8 off = 0; bt_hci_cmd(0x200A, &off, 1, 1000); advertising = false; return 0; }
        if (cc > 31 || rawc || !conn_free()) return -MOS_EINVAL;
        u8 ad[32] = {0};
        ad[0] = (u8)cc;
        if (cc && !uvm_copy_in(p, ad + 1, b, cc)) return -MOS_EFAULT;
        u8 prm[15] = {0};
        put16(prm, 0x00A0); put16(prm + 2, 0x00A0);          /* 100 ms */
        prm[4] = 0x00;                                        /* ADV_IND: conectável, descobrível */
        prm[13] = 0x07;                                       /* os três canais */
        if (bt_hci_cmd(0x2006, prm, 15, 1000)) return -MOS_EINVAL;
        if (bt_hci_cmd(0x2008, ad, 32, 1000)) return -MOS_EINVAL;
        u8 on = 1;
        if (bt_hci_cmd(0x200A, &on, 1, 1000)) return -MOS_EINVAL;
        advertising = true;
        return 0;
    }
    case DEV_BT_DISCONNECT:
        if (rawc && rawc->st > C_CONNECTING) { u8 q[3]; put16(q, rawc->handle); q[2] = 0x13; bt_hci_cmd(0x0406, q, 3, 1000); }
        else if (rawc && rawc->st == C_CONNECTING) { bt_hci_cmd(0x200E, NULL, 0, 1000); }
        return 0;
    case DEV_BT_ATT_SEND: {
        if (!rawc || rawc->st != C_READY || cc > 256 || !cc) return -MOS_EINVAL;
        u8 pdu[256];
        if (!uvm_copy_in(p, pdu, b, cc)) return -MOS_EFAULT;
        return raw_send(pdu, (u16)cc) ? 0 : -MOS_EINVAL;
    }
    case DEV_BT_ATT_RECV: {
        if (raw_rt == raw_rh) return (raw_gone || raw_failed) && !rawc && !want_raw ? -1 : 0;
        u16 n = raw_len[raw_rt];
        if (cc < n) return -MOS_EINVAL;
        if (!uvm_copy_out(p, b, raw_pdu[raw_rt], n)) return -MOS_EFAULT;
        raw_rt = (raw_rt + 1) % RAWQ;
        /* resposta de erro "autenticação/cifra insuficiente": pareia e o app repete o pedido */
        if (n >= 5 && raw_pdu[(raw_rt + RAWQ - 1) % RAWQ][0] == 0x01 && rawc && rawc->st == C_READY) {
            u8 e = raw_pdu[(raw_rt + RAWQ - 1) % RAWQ][4];
            if (e == 0x05 || e == 0x0F) { c = rawc; if (bond_load(c->addr)) start_encryption(c->ltk, c->rand, c->ediv); else smp_start(); }
        }
        return n;
    }
    case DEV_BT_LINK: {
        gui_bt_link_t o;
        memset(&o, 0, sizeof o);
        o.state = rawc ? (rawc->st == C_READY ? 2 : 1) : want_raw ? 1 : raw_failed ? 3 : 0;
        o.role = periph_role ? 1 : 0;
        o.advertising = advertising;
        o.mtu = rawc && rawc->mtu ? rawc->mtu : 23;
        if (rawc) addr_str(rawc->addr, o.addr);
        snprintf(o.err, sizeof o.err, "%s", rawc && rawc->err[0] ? rawc->err : last_err);
        return uvm_copy_out(p, b, &o, sizeof o) ? 0 : -MOS_EFAULT;
    }
    }
    return -MOS_EINVAL;
}
