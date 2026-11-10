/* mOS — teste de host do BLE central (kernel/bt/ble.c) contra um controlador e periféricos simulados:
 * um mouse (Secure Connections, Just Works, endereço aleatório) e um teclado que exige MITM (passkey:
 * o "usuário" digita no teclado os 6 dígitos que o mOS mostra pelo notify). Cobre pareamento automático
 * dos dois ao mesmo tempo, HOGP até a entrada chegar, passkey errada, pareamento legado com passkey,
 * reconexão pela lista de aceitação com um aparelho dormindo, queda e volta, conexão manual cancelando a
 * reconexão, e vínculo esquecido pelo aparelho. Usa as funções reais de cripto (smp_crypto.c, p256.c).
 *
 *   gcc -O1 -Wall -Wextra -Wno-unused-parameter -Wno-incompatible-library-redeclaration -I kernel/include \
 *       tools/ble_host.c kernel/bt/ble.c kernel/bt/smp_crypto.c kernel/bt/p256.c kernel/bt/hid_parse.c kernel/wifi/wpa.c \
 *       -o /tmp/ble_host && /tmp/ble_host
 *
 * Limite: o periférico simulado segue a especificação (Core 5.x, Vol 3, Parte H) escrita aqui; as funções
 * c1/s1/f4/f5/f6 são conferidas contra os exemplos da especificação em tools/bttest.c. O que prova o
 * comportamento dos aparelhos reais é o hardware.
 */
#include "bluetooth.h"
#include "vfs.h"
#include "proc.h"

int printf(const char *fmt, ...);
int vprintf(const char *fmt, va_list ap);
void *malloc(usize n);
void free(void *p);
int rand(void);
int atoi(const char *s);

void smp_c1(const u8 *k, const u8 *r, const u8 *preq, const u8 *pres, u8 iat, const u8 *ia, u8 rat, const u8 *ra, u8 *out);
void smp_s1(const u8 *k, const u8 *r1, const u8 *r2, u8 *out);
void smp_f4(const u8 *u, const u8 *v, const u8 *x, u8 z, u8 *out);
void smp_f5(const u8 *w, const u8 *n1, const u8 *n2, const u8 *a1, const u8 *a2, u8 *mackey, u8 *ltk);
void smp_f6(const u8 *w, const u8 *n1, const u8 *n2, const u8 *r, const u8 *io, const u8 *a1, const u8 *a2, u8 *out);
void p256_public(const u8 *priv, u8 *x, u8 *y);
bool p256_ecdh(const u8 *priv, const u8 *px, const u8 *py, u8 *dh);
void bt_stack_init(void);
void bt_stack_poll(void);
int  bt_le_device_count(void);
bool bt_le_device(int i, char *name, usize ncap, char *addr, int *rssi, bool *hid);
bool bt_le_connect(int i);
bool bt_le_scan(bool on);
const char *bt_le_status(void);

/* ---------------------------------------------------------------- ambiente do kernel */
static int verbose;
void kprintf(const char *fmt, ...) {
    if (!verbose) return;
    va_list ap;
    va_start(ap, fmt);
    printf("    | ");
    vprintf(fmt, ap);
    va_end(ap);
}
void *kmalloc(usize n) { return malloc(n); }
void kfree(void *p) { free(p); }
static u64 now_ms = 1000;
u64 timer_ticks(void) { return now_ms; }
void crypto_random(void *buf, usize len) { u8 *b = buf; for (usize i = 0; i < len; i++) b[i] = (u8)rand(); }
bool uvm_copy_out(process_t *p, u64 va, const void *src, usize len) { return false; }
bool uvm_copy_in(process_t *p, void *dst, u64 va, usize len) { return false; }
bool classic_event(const u8 *ev, int len) { return false; }
bool classic_acl(const u8 *a, int len) { return false; }
void classic_poll(void) {}
void classic_init(void) {}

static int mouse_events, last_dx, last_dy;
void input_push(input_event_t *e) { if (e->type == EV_MOUSE) { mouse_events++; last_dx = e->dx; last_dy = e->dy; } }
u16 input_mods(void) { return 0; }
static int kbd_events;
static u8 last_key, last_mods;
void input_hid_keyboard(u8 mods, const u8 keys[6]) { kbd_events++; last_key = keys[0]; last_mods = mods; }
static char last_notify[128];
static int notifies;
void notify(const char *title, const char *body) { notifies++; snprintf(last_notify, sizeof last_notify, "%s", body); }

/* VFS mínimo: só a pasta dos vínculos */
#define BOND_DIR "/Sistema/Biblioteca/Bluetooth"
static vnode_t dirnode;
static bool dir_exists;
static vnode_t fnodes[8];
static bool fused[8];
vnode_t *vfs_mkdir(const char *path) { if (!strcmp(path, BOND_DIR)) { dir_exists = true; dirnode.dir = true; } return &dirnode; }
static void relink(void) {
    dirnode.child = NULL;
    for (int i = 7; i >= 0; i--) if (fused[i]) { fnodes[i].next = dirnode.child; dirnode.child = &fnodes[i]; }
}
vnode_t *vfs_lookup(const char *path) {
    if (!strcmp(path, BOND_DIR)) return dir_exists ? &dirnode : NULL;
    for (int i = 0; i < 8; i++) if (fused[i] && !strcmp(fnodes[i].name, path)) return &fnodes[i];
    return NULL;
}
vnode_t *vfs_write_file(const char *path, const void *data, usize n) {
    vnode_t *v = vfs_lookup(path);
    if (!v) {
        int i = 0;
        while (i < 8 && fused[i]) i++;
        if (i == 8) return NULL;
        fused[i] = true;
        v = &fnodes[i];
        memset(v, 0, sizeof *v);
        snprintf(v->name, sizeof v->name, "%s", path);
    } else free(v->data);
    v->data = malloc(n);
    memcpy(v->data, data, n);
    v->size = n;
    relink();
    return v;
}
bool vfs_delete(vnode_t *n) {
    for (int i = 0; i < 8; i++) if (fused[i] && &fnodes[i] == n) { fused[i] = false; free(n->data); relink(); return true; }
    return false;
}
static int bond_count(void) { int k = 0; for (int i = 0; i < 8; i++) k += fused[i]; return k; }

/* ---------------------------------------------------------------- controlador e ar simulados */
void (*bt_on_event)(const u8 *ev, u32 len);
void (*bt_on_acl)(const u8 *acl, u32 len);

static const u8 A[6] = {0x66, 0x55, 0x44, 0x33, 0x22, 0x11};     /* nosso BD_ADDR (público) */

typedef struct {
    const char *name;
    u8 addr[6], type;
    u16 appearance;
    u8 iocap, authreq;
    bool kbd;
    bool adv, pairable;
    bool nonconn;                   /* anuncia, mas não aceita conexão (ADV_SCAN_IND) */
    int typo;                       /* soma à passkey digitada (0 = digita certo) */
    bool connected;
    u16 handle;
    /* SMP */
    u8 preq[7], pres[7], tk[16], mconf[16], srand[16];
    bool sc, passkey;
    u32 pk;
    int round;
    u8 priv[32], pkx[32], pky[32], peerx[32], peery[32], dh[32], na[16], nb[16], ca[16];
    u8 cur_key[16];
    bool cur_key_set, legacy_dist, paired_now;
    /* vínculo */
    bool bonded;
    u8 ltk[16], rand8[8];
    u16 ediv;
    bool notify_on;
} periph_t;

static periph_t P[2];
#define MOUSE (&P[0])
#define KBD   (&P[1])

typedef struct { u8 kind; u8 d[300]; u16 len; } qev_t;
static qev_t evq[256];
static int evn;
static bool scanning, pending;
static u8 pend_filter, pend_addr[6];
static u8 accept[8][7];
static int naccept;
static u16 next_handle = 0x40;
static int n_create, n_cancel, n_smp_pair_req;

static void push(u8 kind, const u8 *d, u16 len) { if (evn < 256) { evq[evn].kind = kind; memcpy(evq[evn].d, d, len); evq[evn].len = len; evn++; } }
static void push_acl(periph_t *x, u16 cid, const u8 *pdu, u16 n) {
    u8 b[300];
    b[0] = (u8)x->handle; b[1] = (u8)(x->handle >> 8) | 0x20;
    b[2] = (u8)(n + 4); b[3] = (u8)((n + 4) >> 8);
    b[4] = (u8)n; b[5] = (u8)(n >> 8);
    b[6] = (u8)cid; b[7] = (u8)(cid >> 8);
    memcpy(b + 8, pdu, n);
    push(1, b, (u16)(n + 8));
}
static void conn_complete_ev(u8 status, periph_t *x) {
    u8 e[21] = {0x3E, 19, 0x01, status};
    if (x) { e[4] = (u8)x->handle; e[5] = (u8)(x->handle >> 8); e[6] = 0; e[7] = x->type; memcpy(e + 8, x->addr, 6); }
    push(0, e, 21);
}
static periph_t *by_handle(u16 h) { for (int i = 0; i < 2; i++) if (P[i].connected && P[i].handle == h) return &P[i]; return NULL; }
static void enc_change(periph_t *x, u8 status) { u8 e[6] = {0x08, 4, status, (u8)x->handle, (u8)(x->handle >> 8), status ? 0 : 1}; push(0, e, 6); }

static void rev(u8 *d, const u8 *s, int n) { for (int i = 0; i < n; i++) d[i] = s[n - 1 - i]; }
static void rnd(u8 *p, int n) { for (int i = 0; i < n; i++) p[i] = (u8)rand(); }

static void smp_send(periph_t *x, const u8 *p, u16 n) { push_acl(x, 6, p, n); }
static void smp_failp(periph_t *x, u8 r) { u8 f[2] = {0x05, r}; smp_send(x, f, 2); }
static void sc_addr(u8 out[7], u8 type, const u8 *a) { out[0] = type; rev(out + 1, a, 6); }
static u8 zbit(periph_t *x) { return x->passkey ? (u8)(0x80 | ((x->pk >> x->round) & 1)) : 0; }
static void typed_passkey(periph_t *x) {             /* o "usuário" lê o notify e digita no teclado */
    const char *s = last_notify;
    while (*s && (*s < '0' || *s > '9')) s++;
    x->pk = (u32)(atoi(s) + x->typo) % 1000000u;
    memset(x->tk, 0, 16);
    x->tk[0] = (u8)x->pk; x->tk[1] = (u8)(x->pk >> 8); x->tk[2] = (u8)(x->pk >> 16);
}
static void sc_cb(periph_t *x, u8 out_le[16]) {        /* Cb = f4(PKbx, PKax, Nb, z) */
    u8 u[32], v[32], nb[16], o[16];
    rev(u, x->pkx, 32); rev(v, x->peerx, 32); rev(nb, x->nb, 16);
    smp_f4(u, v, nb, zbit(x), o);
    rev(out_le, o, 16);
}

static void sim_smp(periph_t *x, const u8 *p, int n) {
    switch (p[0]) {
    case 0x01: {                                                   /* Pairing Request */
        n_smp_pair_req++;
        memcpy(x->preq, p, 7);
        u8 r[7] = {0x02, x->iocap, 0, x->authreq, 16, 0x00, 0x01};
        memcpy(x->pres, r, 7);
        x->sc = (x->preq[3] & 8) && (x->authreq & 8);
        x->passkey = ((x->preq[3] | x->authreq) & 4) && x->preq[1] == 0x00 && (x->iocap == 2 || x->iocap == 4);
        x->round = 0;
        memset(x->tk, 0, 16);
        smp_send(x, r, 7);
        return;
    }
    case 0x0C: {                                                   /* chave pública do central */
        memcpy(x->peerx, p + 1, 32); memcpy(x->peery, p + 33, 32);
        rnd(x->priv, 32); x->priv[31] &= 0x7F;
        p256_public(x->priv, x->pkx, x->pky);
        if (!p256_ecdh(x->priv, x->peerx, x->peery, x->dh)) { smp_failp(x, 0x0B); return; }
        u8 k[65] = {0x0C};
        memcpy(k + 1, x->pkx, 32); memcpy(k + 33, x->pky, 32);
        smp_send(x, k, 65);
        if (!x->passkey) { rnd(x->nb, 16); u8 c[17] = {0x03}; sc_cb(x, c + 1); smp_send(x, c, 17); }
        else typed_passkey(x);
        return;
    }
    case 0x03:                                                     /* Confirm do central */
        if (x->sc) {                                               /* passkey: Cai chegou, responde Cbi */
            memcpy(x->ca, p + 1, 16);
            rnd(x->nb, 16);
            u8 c[17] = {0x03}; sc_cb(x, c + 1); smp_send(x, c, 17);
            return;
        }
        if (x->passkey) typed_passkey(x);
        memcpy(x->mconf, p + 1, 16);
        rnd(x->srand, 16);
        { u8 c[17] = {0x03}; smp_c1(x->tk, x->srand, x->preq, x->pres, 0, A, x->type, x->addr, c + 1); smp_send(x, c, 17); }
        return;
    case 0x04:                                                     /* Random do central */
        if (x->sc) {
            memcpy(x->na, p + 1, 16);
            if (x->passkey) {                                      /* confere Cai = f4(PKax, PKbx, Nai, z) */
                u8 u[32], v[32], na[16], o[16], ca[16];
                rev(u, x->peerx, 32); rev(v, x->pkx, 32); rev(na, x->na, 16);
                smp_f4(u, v, na, zbit(x), o);
                rev(ca, o, 16);
                if (memcmp(ca, x->ca, 16)) { smp_failp(x, 0x04); return; }
                x->round++;
            }
            u8 r[17] = {0x04}; memcpy(r + 1, x->nb, 16); smp_send(x, r, 17);
            return;
        }
        {
            u8 chk[16];
            smp_c1(x->tk, p + 1, x->preq, x->pres, 0, A, x->type, x->addr, chk);
            if (memcmp(chk, x->mconf, 16)) { smp_failp(x, 0x04); return; }
            u8 r[17] = {0x04}; memcpy(r + 1, x->srand, 16); smp_send(x, r, 17);
            smp_s1(x->tk, x->srand, p + 1, x->cur_key);
            x->cur_key_set = true;
            x->legacy_dist = true;
        }
        return;
    case 0x0D: {                                                   /* Ea */
        u8 w[32], na[16], nb[16], a1[7], a2[7], mk[16], ltk[16], io[3], r[16] = {0}, ea[16], got[16], eb[16];
        rev(w, x->dh, 32); rev(na, x->na, 16); rev(nb, x->nb, 16);
        sc_addr(a1, 0, A); sc_addr(a2, x->type, x->addr);
        smp_f5(w, na, nb, a1, a2, mk, ltk);
        if (x->passkey) { r[13] = (u8)(x->pk >> 16); r[14] = (u8)(x->pk >> 8); r[15] = (u8)x->pk; }
        io[0] = x->preq[3]; io[1] = x->preq[2]; io[2] = x->preq[1];
        smp_f6(mk, na, nb, r, io, a1, a2, ea);
        rev(got, p + 1, 16);
        if (memcmp(got, ea, 16)) { smp_failp(x, 0x0B); return; }
        io[0] = x->pres[3]; io[1] = x->pres[2]; io[2] = x->pres[1];
        smp_f6(mk, nb, na, r, io, a2, a1, eb);
        u8 pk[17] = {0x0D}; rev(pk + 1, eb, 16); smp_send(x, pk, 17);
        rev(x->cur_key, ltk, 16);
        x->cur_key_set = true;
        x->legacy_dist = false;
        return;
    }
    }
    (void)n;
}

/* GATT do periférico: 1800 (1-5) e HID 1812 (10-30): 11/12 Report Map, 13/14 Report (notify), 15 CCCD, 16 Report Ref */
static const u8 mouse_map[] = {0x05,0x01,0x09,0x02,0xA1,0x01,0x09,0x01,0xA1,0x00,0x05,0x09,0x19,0x01,0x29,0x03,0x15,0x00,0x25,0x01,
    0x95,0x03,0x75,0x01,0x81,0x02,0x95,0x01,0x75,0x05,0x81,0x03,0x05,0x01,0x09,0x30,0x09,0x31,0x09,0x38,0x15,0x81,0x25,0x7F,
    0x75,0x08,0x95,0x03,0x81,0x06,0xC0,0xC0};
static const u8 kbd_map[] = {0x05,0x01,0x09,0x06,0xA1,0x01,0x85,0x01,0x05,0x07,0x19,0xE0,0x29,0xE7,0x15,0x00,0x25,0x01,0x75,0x01,
    0x95,0x08,0x81,0x02,0x95,0x01,0x75,0x08,0x81,0x01,0x95,0x06,0x75,0x08,0x15,0x00,0x25,0x65,0x05,0x07,0x19,0x00,0x29,0x65,
    0x81,0x00,0xC0};
static void att_err(periph_t *x, u8 op, u16 h, u8 code) { u8 e[5] = {0x01, op, (u8)h, (u8)(h >> 8), code}; push_acl(x, 4, e, 5); }
static void sim_att(periph_t *x, const u8 *p, int n) {
    u16 s = n >= 3 ? (u16)(p[1] | p[2] << 8) : 0;
    const u8 *map = x->kbd ? kbd_map : mouse_map;
    int maplen = x->kbd ? (int)sizeof kbd_map : (int)sizeof mouse_map;
    switch (p[0]) {
    case 0x02: { u8 r[3] = {0x03, 23, 0}; push_acl(x, 4, r, 3); return; }
    case 0x10:
        if (s <= 1) { u8 r[14] = {0x11, 6, 1,0, 5,0, 0x00,0x18, 10,0, 30,0, 0x12,0x18}; push_acl(x, 4, r, 14); }
        else att_err(x, 0x10, s, 0x0A);
        return;
    case 0x08:
        if (s <= 11) { u8 r[16] = {0x09, 7, 11,0, 0x02, 12,0, 0x4B,0x2A, 13,0, 0x12, 14,0, 0x4D,0x2A}; push_acl(x, 4, r, 16); }
        else att_err(x, 0x08, s, 0x0A);
        return;
    case 0x04:
        if (s <= 16) { u8 r[10] = {0x05, 1, 15,0, 0x02,0x29, 16,0, 0x08,0x29}; push_acl(x, 4, r, 10); }
        else att_err(x, 0x04, s, 0x0A);
        return;
    case 0x0A:
        if (s == 16) { u8 r[3] = {0x0B, (u8)(x->kbd ? 1 : 0), 1}; push_acl(x, 4, r, 3); return; }
        if (s == 12) { u8 r[23] = {0x0B}; int k = maplen < 22 ? maplen : 22; memcpy(r + 1, map, k); push_acl(x, 4, r, (u16)(1 + k)); return; }
        att_err(x, 0x0A, s, 0x01);
        return;
    case 0x0C: {
        u16 off = (u16)(p[3] | p[4] << 8);
        u8 r[23] = {0x0D};
        int k = maplen - off; if (k > 22) k = 22; if (k < 0) k = 0;
        memcpy(r + 1, map + off, k);
        push_acl(x, 4, r, (u16)(1 + k));
        return;
    }
    case 0x12: if (s == 15) x->notify_on = true; { u8 r = 0x13; push_acl(x, 4, &r, 1); } return;
    }
}

int bt_hci_cmd(u16 op, const void *pv, u8 len, int timeout_ms) {
    const u8 *p = pv;
    switch (op) {
    case 0x200C: scanning = p[0]; return 0;
    case 0x200D:
        if (pending) return 0x0C;
        n_create++;
        pending = true; pend_filter = p[4]; memcpy(pend_addr, p + 6, 6);
        return 0;
    case 0x200E:
        if (!pending) return 0x0C;
        n_cancel++;
        pending = false;
        conn_complete_ev(0x02, NULL);
        return 0;
    case 0x2010: if (pending && pend_filter) return 0x0C; naccept = 0; return 0;
    case 0x2011: if (pending && pend_filter) return 0x0C; if (naccept < 8) memcpy(accept[naccept++], p, 7); return 0;
    case 0x2019: {                                                 /* LE Start Encryption */
        periph_t *x = by_handle((u16)(p[0] | p[1] << 8));
        if (!x) return 0x02;
        const u8 *rnd8 = p + 2, *ltk = p + 12;
        u16 ediv = (u16)(p[10] | p[11] << 8);
        if (x->cur_key_set && !memcmp(ltk, x->cur_key, 16)) {      /* chave do pareamento que acabou de acontecer */
            x->cur_key_set = false;
            enc_change(x, 0);
            if (x->legacy_dist) {                                  /* legado: distribui LTK/EDIV/Rand */
                rnd(x->ltk, 16); rnd(x->rand8, 8); x->ediv = (u16)rand();
                u8 k[17] = {0x06}; memcpy(k + 1, x->ltk, 16); smp_send(x, k, 17);
                u8 m[11] = {0x07, (u8)x->ediv, (u8)(x->ediv >> 8)}; memcpy(m + 3, x->rand8, 8); smp_send(x, m, 11);
            } else { memcpy(x->ltk, ltk, 16); memset(x->rand8, 0, 8); x->ediv = 0; }
            x->bonded = true;
            x->paired_now = true;
            return 0;
        }
        if (x->bonded && !memcmp(ltk, x->ltk, 16) && ediv == x->ediv && !memcmp(rnd8, x->rand8, 8)) { enc_change(x, 0); return 0; }
        enc_change(x, 0x06);                                       /* PIN or Key Missing */
        return 0;
    }
    case 0x0406: {
        periph_t *x = by_handle((u16)(p[0] | p[1] << 8));
        if (!x) return 0x02;
        x->connected = false;
        u8 e[6] = {0x05, 4, 0, (u8)x->handle, (u8)(x->handle >> 8), p[2]};
        push(0, e, 6);
        return 0;
    }
    }
    return 0;
}
int bt_hci_cmd_resp(u16 op, const void *p, u8 len, u8 *resp, int max, int timeout_ms) {
    if (op == 0x1009) { u8 r[7] = {0}; memcpy(r + 1, A, 6); memcpy(resp, r, 7); return 7; }
    if (resp && max) resp[0] = 0;
    return bt_hci_cmd(op, p, len, timeout_ms) ? -1 : 1;
}
bool bt_acl_send(u16 handle, const void *data, u16 len) {
    const u8 *d = data;
    periph_t *x = by_handle(handle);
    if (!x || len < 4) return false;
    u16 cid = (u16)(d[2] | d[3] << 8);
    if (cid == 6) sim_smp(x, d + 4, len - 4);
    else if (cid == 4) sim_att(x, d + 4, len - 4);
    return true;
}

static bool in_accept(periph_t *x) {
    for (int i = 0; i < naccept; i++) if (!memcmp(accept[i] + 1, x->addr, 6)) return true;
    return false;
}
static void adv_report(periph_t *x) {
    u8 ad[31];
    int k = 0;
    ad[k++] = 2; ad[k++] = 0x01; ad[k++] = x->pairable ? 0x06 : 0x04;
    ad[k++] = 3; ad[k++] = 0x19; ad[k++] = (u8)x->appearance; ad[k++] = (u8)(x->appearance >> 8);
    int nl = (int)strlen(x->name);
    ad[k++] = (u8)(nl + 1); ad[k++] = 0x09; memcpy(ad + k, x->name, nl); k += nl;
    u8 e[48] = {0x3E, 0, 0x02, 1, 0, x->type};
    memcpy(e + 6, x->addr, 6);
    e[12] = (u8)k;
    memcpy(e + 13, ad, k);
    e[13 + k] = (u8)(i8)-50;
    e[1] = (u8)(12 + k);
    push(0, e, (u16)(14 + k));
}

/* um passo do "ar": entrega o que está na fila, anuncia, conecta, e roda o laço principal do mOS */
static void step(void) {
    static int tick;
    if (scanning && tick % 4 == 0) for (int i = 0; i < 2; i++) if (P[i].adv && !P[i].connected) adv_report(&P[i]);
    if (pending)
        for (int i = 0; i < 2; i++) {
            periph_t *x = &P[i];
            if (!x->adv || x->nonconn || x->connected) continue;
            if (pend_filter ? in_accept(x) : !memcmp(pend_addr, x->addr, 6)) {
                pending = false;
                x->connected = true;
                x->handle = next_handle++;
                x->notify_on = false;
                conn_complete_ev(0, x);
                break;
            }
        }
    int k = 0;
    while (k < evn && k < 16) {
        if (evq[k].kind) bt_on_acl(evq[k].d, evq[k].len); else bt_on_event(evq[k].d, evq[k].len);
        k++;
    }
    memmove(evq, evq + k, (evn - k) * sizeof evq[0]);
    evn -= k;
    bt_stack_poll();
    now_ms += 50;
    tick++;
}
static void run(int ms) { for (u64 end = now_ms + (u64)ms; now_ms < end;) step(); }

static void notify_input(periph_t *x) {
    if (x->kbd) { u8 r[11] = {0x1B, 14, 0, 0x02, 0, 0x04, 0, 0, 0, 0, 0}; push_acl(x, 4, r, 11); }   /* Shift + 'a' */
    else { u8 r[7] = {0x1B, 14, 0, 0x01, 5, (u8)-3, 0}; push_acl(x, 4, r, 7); }                     /* botão 1, +5, -3 */
}
static void drop_link(periph_t *x, u8 reason) {
    if (!x->connected) return;
    x->connected = false;
    u8 e[6] = {0x05, 4, 0, (u8)x->handle, (u8)(x->handle >> 8), reason};
    push(0, e, 6);
}
static void reboot(void) {                       /* o mOS reinicia; os aparelhos guardam o que guardaram */
    for (int i = 0; i < 2; i++) { P[i].connected = false; P[i].paired_now = false; }
    evn = 0; pending = false; naccept = 0; scanning = false;
    run(200);                                    /* esvazia a fila interna do ble.c */
    bt_stack_init();
}

static int fails, oks;
static void check(int cond, const char *what) {
    if (cond) { oks++; printf("ok  %s\n", what); }
    else { fails++; printf("FALHOU %s\n       estado: %s\n", what, bt_le_status()); }
}
static bool ready_both(void) { const char *s = bt_le_status(); return strstr(s, "MX Master 3S: conectado") && strstr(s, "Slim X1 Keyboard: conectado"); }

static void setup_periphs(bool kbd_sc) {
    memset(P, 0, sizeof P);
    MOUSE->name = "MX Master 3S";
    u8 ma[6] = {0xC1, 0x17, 0xA3, 0x86, 0x73, 0xDA}; memcpy(MOUSE->addr, ma, 6);
    MOUSE->type = 1; MOUSE->appearance = 0x03C2; MOUSE->iocap = 0x03; MOUSE->authreq = 0x09;   /* sem E/S, bond + SC */
    KBD->name = "Slim X1 Keyboard";
    u8 ka[6] = {0x96, 0xB2, 0xEF, 0x0D, 0x6F, 0xC0}; memcpy(KBD->addr, ka, 6);
    KBD->type = 0; KBD->appearance = 0x03C1; KBD->kbd = true; KBD->iocap = 0x02;               /* só teclado */
    KBD->authreq = kbd_sc ? 0x0D : 0x05;                                                       /* bond + MITM (+ SC) */
}

int main(int argc, char **argv) {
    verbose = argc > 1;
    memset(&dirnode, 0, sizeof dirnode);

    printf("-- primeiro boot: mouse e teclado em modo de pareamento, nenhum vínculo\n");
    setup_periphs(true);
    MOUSE->adv = MOUSE->pairable = true;
    KBD->adv = KBD->pairable = true;
    bt_stack_init();
    run(20000);
    check(ready_both(), "mouse e teclado conectados ao mesmo tempo");
    check(bond_count() == 2 && MOUSE->bonded && KBD->bonded, "dois vínculos salvos, um por aparelho");
    check(notifies == 1 && strstr(last_notify, "Digite") != NULL, "passkey mostrada uma vez (só o teclado exige MITM)");
    printf("    notify: \"%s\"\n", last_notify);
    check(MOUSE->notify_on && KBD->notify_on, "notificações HID ligadas nos dois (CCCD)");
    notify_input(MOUSE);
    notify_input(KBD);
    run(500);
    check(mouse_events == 1 && last_dx == 5 && last_dy == -3, "relatório do mouse vira movimento (+5, -3)");
    check(kbd_events == 1 && last_key == 0x04 && last_mods == 0x02, "relatório do teclado vira Shift + A");

    printf("-- reboot com o mouse dormindo: o teclado não pode esperar o mouse\n");
    MOUSE->adv = false; MOUSE->pairable = false;
    KBD->adv = true; KBD->pairable = false;
    int pairs = n_smp_pair_req;
    reboot();
    run(8000);
    check(strstr(bt_le_status(), "Slim X1 Keyboard: conectado") != NULL, "teclado volta pela lista de aceitação");
    check(n_smp_pair_req == pairs, "reconexão cifra com a LTK salva, sem parear de novo");
    check(naccept == 1 && in_accept(MOUSE), "o mouse continua na lista de aceitação");
    MOUSE->adv = true;
    run(8000);
    check(ready_both(), "o mouse acorda e volta sozinho");
    check(n_smp_pair_req == pairs, "nenhum pareamento novo na volta");

    printf("-- queda do enlace do mouse\n");
    drop_link(MOUSE, 0x08);
    run(3000);
    check(ready_both(), "o mouse reconecta depois da queda (supervision timeout)");

    printf("-- conexão manual com uma reconexão em segundo plano pendente\n");
    drop_link(MOUSE, 0x13);
    MOUSE->adv = false;
    run(3000);
    check(pending && pend_filter == 1, "reconexão em segundo plano pendente (filtro = lista de aceitação)");
    int cancels = n_cancel;
    bt_le_scan(true);                                        /* a pessoa abre Ajustes > Bluetooth */
    MOUSE->adv = true; MOUSE->nonconn = true;
    run(1000);
    MOUSE->adv = false; MOUSE->nonconn = false;
    check(pending && pend_filter == 1, "a reconexão em segundo plano segue pendente durante a varredura");
    int mi = -1;
    for (int i = 0; i < bt_le_device_count(); i++) { char nm[32]; bt_le_device(i, nm, sizeof nm, NULL, NULL, NULL); if (!strcmp(nm, "MX Master 3S")) mi = i; }
    check(mi >= 0, "o mouse está na lista de dispositivos vistos");
    MOUSE->adv = true;
    check(bt_le_connect(mi), "bt_le_connect aceita o pedido");
    run(5000);
    check(n_cancel == cancels + 1, "cancela a conexão pendente da lista de aceitação (0x200E)");
    check(ready_both(), "conexão manual concluída e os dois seguem conectados");

    printf("-- o mouse esqueceu o vínculo (pareado em outro computador)\n");
    drop_link(MOUSE, 0x13);
    MOUSE->bonded = false;
    memset(MOUSE->ltk, 0, 16);
    pairs = n_smp_pair_req;
    run(10000);
    check(n_smp_pair_req == pairs + 1 && ready_both(), "vínculo recusado (0x06): pareia de novo e conecta");

    printf("-- teclado com passkey digitada errada\n");
    memset(&dirnode, 0, sizeof dirnode); dir_exists = false;
    for (int i = 0; i < 8; i++) if (fused[i]) { free(fnodes[i].data); fused[i] = false; }
    setup_periphs(true);
    KBD->adv = KBD->pairable = true;
    KBD->typo = 1;
    pairs = n_smp_pair_req;
    reboot();
    run(65000);                                              /* janela inteira do pareamento automático */
    check(!KBD->bonded && bond_count() == 0, "passkey errada: nenhum vínculo");
    check(n_smp_pair_req == pairs + 3, "o pareamento automático tenta 3 vezes, cada uma com passkey nova");
    check(!KBD->connected, "cada falha derruba o enlace (a vaga fica livre)");
    check(strstr(bt_le_status(), "passkey digitada errada") != NULL, "o estado diz que a passkey estava errada");
    printf("    estado: %s\n", bt_le_status());
    KBD->typo = 0;
    int ki = -1;
    for (int i = 0; i < bt_le_device_count(); i++) { char nm[32]; bt_le_device(i, nm, sizeof nm, NULL, NULL, NULL); if (!strcmp(nm, "Slim X1 Keyboard")) ki = i; }
    check(ki >= 0 && bt_le_connect(ki), "a pessoa pede de novo em Ajustes");
    run(8000);
    check(KBD->bonded && strstr(bt_le_status(), "Slim X1 Keyboard: conectado"), "digitando certo, pareia e conecta");

    printf("-- teclado em pareamento legado (sem Secure Connections) com passkey\n");
    memset(&dirnode, 0, sizeof dirnode); dir_exists = false;
    for (int i = 0; i < 8; i++) if (fused[i]) { free(fnodes[i].data); fused[i] = false; }
    setup_periphs(false);
    KBD->adv = KBD->pairable = true;
    reboot();
    run(10000);
    check(KBD->bonded && bond_count() == 1 && strstr(bt_le_status(), "Slim X1 Keyboard: conectado"), "legado com passkey: vínculo com a LTK distribuída e HID pronto");
    KBD->pairable = false;
    reboot();
    pairs = n_smp_pair_req;
    run(8000);
    check(strstr(bt_le_status(), "Slim X1 Keyboard: conectado") && n_smp_pair_req == pairs, "legado: reconecta com EDIV/Rand/LTK salvos");

    printf("%s (%d ok, %d falhas)\n", fails ? "ble_host: FALHOU" : "ble_host: ok", oks, fails);
    return fails ? 1 : 0;
}
