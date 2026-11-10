/*
 * mOS — Bluetooth: transporte HCI sobre USB (adaptadores classe E0/01/01).
 *
 *   comandos HCI   → requisição de controle de classe (bmRequestType 0x20, bRequest 0, wIndex = interface)
 *   eventos HCI    ← interrupt IN (remontados pelo comprimento do cabeçalho)
 *   ACL            ↔ bulk IN/OUT (endpoints configurados, sem tráfego por enquanto)
 *
 * Inicialização: Reset, Read Local Version, Read BD_ADDR, Read Local Name, Set Event Mask,
 * Write Inquiry Mode (EIR), Write Local Name, Write Scan Enable.  bt_scan() faz Inquiry clássico
 * (+ varredura LE se suportado) de forma assíncrona; os nomes clássicos vêm por Remote Name Request.
 * Nada aqui bloqueia o sistema se o adaptador não responder: todos os comandos têm timeout.
 */
#include "usb.h"

#define EVQ       32
#define MAX_FOUND 16

typedef struct { u16 len; u8 data[258]; } hci_ev_t;

typedef struct {
    u8   addr[6];
    u8   psrm;
    u16  clk;
    u32  cod;
    i8   rssi;
    bool le, named, tried;
    char name[64];
} btdev_t;

static struct {
    bool present, up;
    usb_dev_t *d;
    u8   iface, ep_evt, ep_in, ep_out;
    u8   asmb[300];
    u16  asm_len;
    hci_ev_t q[EVQ];
    u32  qh, qt;
    u8   bdaddr[6];
    u8   hci_ver, lmp_ver;
    u16  manuf, lmp_sub;
    char local_name[64];
    bool le;
    /* espera de comando */
    u16  wait_op;
    bool wait_done;
    u8   ret[256];
    u16  ret_len;
    /* varredura */
    bool scanning, inq_done, le_scanning, name_pending;
    u64  scan_end, le_end, name_deadline;
    int  name_idx;
    char status[160];
} bt;

static int hci_cmd(u16 op, const void *p, u8 plen, u32 timeout);

/* ganchos para a pilha superior (L2CAP/SSP/HID/A2DP em kernel/bt/): chamados do usb_poll, nunca em IRQ */
void (*bt_on_event)(const u8 *ev, u32 len);
void (*bt_on_acl)(const u8 *acl, u32 len);

#define ACLQ 16
typedef struct { u16 len; u8 data[1024]; } acl_pkt_t;
static acl_pkt_t aclq[ACLQ];
static u32 acl_h, acl_t;
static u8 acl_asm[1100];
static u32 acl_asm_len;

static btdev_t found[MAX_FOUND];
static int nfound;
static char dev_names[MAX_FOUND][112];

/* ------------------------------------------------------------------ textos */
static const char *company(u16 id) {
    switch (id) {
    case 0: return "Ericsson"; case 2: return "Intel"; case 10: return "Qualcomm (CSR)"; case 13: return "Texas Instruments";
    case 15: return "Broadcom"; case 29: return "Qualcomm"; case 48: return "ST Microelectronics"; case 70: return "MediaTek";
    case 93: return "Realtek"; case 305: return "Cypress"; case 1521: return "Infineon"; case 72: return "Marvell";
    default: return NULL;
    }
}
static const char *vendor_from_usb(u16 vid) {
    switch (vid) {
    case 0x8087: return "Intel"; case 0x0BDA: return "Realtek"; case 0x0A12: return "CSR"; case 0x0A5C: return "Broadcom";
    case 0x0E8D: return "MediaTek"; case 0x13D3: return "IMC Networks"; case 0x0489: return "Foxconn"; case 0x04CA: return "Lite-On";
    case 0x05AC: return "Apple"; case 0x0CF3: return "Qualcomm Atheros"; case 0x2357: return "TP-Link"; case 0x0B05: return "ASUS";
    default: return NULL;
    }
}
static const char *bt_version(u8 v) {
    static const char *n[] = {"1.0b", "1.1", "1.2", "2.0", "2.1", "3.0", "4.0", "4.1", "4.2", "5.0", "5.1", "5.2", "5.3", "5.4", "6.0"};
    return v < ARRAY_LEN(n) ? n[v] : "?";
}
static void addr_str(const u8 *a, char *out) {
    snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", a[5], a[4], a[3], a[2], a[1], a[0]);
}
static const char *cod_name(u32 cod) {
    switch ((cod >> 8) & 0x1F) {
    case 1: return "Computador"; case 2: return "Telefone"; case 3: return "Rede"; case 4: return "Áudio/vídeo";
    case 5: return "Periférico"; case 6: return "Imagem"; case 7: return "Vestível"; case 8: return "Brinquedo"; case 9: return "Saúde";
    default: return "Dispositivo";
    }
}

/* ------------------------------------------------------------------ eventos */
static void ev_push(const u8 *p, u16 len) {
    u32 n = (bt.qh + 1) % EVQ;
    if (n == bt.qt) return;                 /* fila cheia: descarta */
    bt.q[bt.qh].len = len;
    memcpy(bt.q[bt.qh].data, p, len);
    bt.qh = n;
}

/* callback do interrupt IN (contexto de evento: nada de transferências síncronas aqui) */
static void evt_cb(usb_dev_t *d, u8 ep, u8 *data, u32 len, int st, void *ctx) {
    for (u32 i = 0; i < len; i++) {
        if (bt.asm_len < sizeof bt.asmb) bt.asmb[bt.asm_len++] = data[i];
        while (bt.asm_len >= 2 && bt.asm_len >= 2 + bt.asmb[1]) {
            u16 n = 2 + bt.asmb[1];
            ev_push(bt.asmb, n);
            memmove(bt.asmb, bt.asmb + n, bt.asm_len - n);
            bt.asm_len -= n;
        }
    }
}

static btdev_t *find_or_add(const u8 *addr, bool le) {
    for (int i = 0; i < nfound; i++) if (!memcmp(found[i].addr, addr, 6)) return &found[i];
    if (nfound >= MAX_FOUND) return NULL;
    btdev_t *f = &found[nfound++];
    memset(f, 0, sizeof *f);
    memcpy(f->addr, addr, 6);
    f->le = le;
    char a[18];
    addr_str(addr, a);
    kprintf("Bluetooth: encontrado %s%s\n", a, le ? " (LE)" : "");
    return f;
}

/* procura nome (tipos 0x08/0x09) em dados EIR/AD */
static void ad_name(btdev_t *f, const u8 *p, int len) {
    for (int i = 0; i + 1 < len;) {
        u8 l = p[i];
        if (!l || i + 1 + l > len) break;
        u8 t = p[i + 1];
        if ((t == 0x09 || (t == 0x08 && !f->named)) && l > 1) {
            int n = MIN(l - 1, (int)sizeof f->name - 1);
            memcpy(f->name, p + i + 2, n);
            f->name[n] = 0;
            f->named = true;
        }
        i += 1 + l;
    }
}

static void handle_event(const u8 *e, u16 len) {
    u8 code = e[0], plen = e[1];
    const u8 *p = e + 2;
    if (plen + 2 > len) return;
    switch (code) {
    case 0x0E:   /* Command Complete */
        if (plen >= 3 && bt.wait_op && (p[1] | p[2] << 8) == bt.wait_op) {
            bt.ret_len = plen - 3;
            memcpy(bt.ret, p + 3, bt.ret_len);
            bt.wait_done = true;
        }
        break;
    case 0x0F:   /* Command Status */
        if (plen >= 4 && bt.wait_op && (p[2] | p[3] << 8) == bt.wait_op) {
            bt.ret_len = 1;
            bt.ret[0] = p[0];
            bt.wait_done = true;
        }
        break;
    case 0x01:   /* Inquiry Complete */
        bt.inq_done = true;
        break;
    case 0x02: { /* Inquiry Result: campos em arrays */
        int n = plen ? p[0] : 0;
        if (1 + n * 14 > plen) break;
        for (int i = 0; i < n; i++) {
            btdev_t *f = find_or_add(p + 1 + i * 6, false);
            if (!f) continue;
            f->psrm = p[1 + n * 6 + i];
            f->cod = p[1 + n * 9 + i * 3] | p[2 + n * 9 + i * 3] << 8 | p[3 + n * 9 + i * 3] << 16;
            f->clk = p[1 + n * 12 + i * 2] | p[2 + n * 12 + i * 2] << 8;
        }
        break;
    }
    case 0x22: { /* Inquiry Result with RSSI */
        int n = plen ? p[0] : 0;
        if (1 + n * 14 > plen) break;
        for (int i = 0; i < n; i++) {
            btdev_t *f = find_or_add(p + 1 + i * 6, false);
            if (!f) continue;
            f->psrm = p[1 + n * 6 + i];
            f->cod = p[1 + n * 8 + i * 3] | p[2 + n * 8 + i * 3] << 8 | p[3 + n * 8 + i * 3] << 16;
            f->clk = p[1 + n * 11 + i * 2] | p[2 + n * 11 + i * 2] << 8;
            f->rssi = (i8)p[1 + n * 13 + i];
        }
        break;
    }
    case 0x2F: { /* Extended Inquiry Result */
        if (plen < 15) break;
        btdev_t *f = find_or_add(p + 1, false);
        if (!f) break;
        f->psrm = p[7];
        f->cod = p[9] | p[10] << 8 | p[11] << 16;
        f->clk = p[12] | p[13] << 8;
        f->rssi = (i8)p[14];
        ad_name(f, p + 15, plen - 15);
        break;
    }
    case 0x07: { /* Remote Name Request Complete */
        if (plen < 7) break;
        for (int i = 0; i < nfound; i++) {
            if (memcmp(found[i].addr, p + 1, 6)) continue;
            if (p[0] == 0 && p[7]) {
                int n = 0;
                while (n < (int)sizeof found[i].name - 1 && 7 + n < plen && p[7 + n]) { found[i].name[n] = p[7 + n]; n++; }
                found[i].name[n] = 0;
                found[i].named = true;
            }
            found[i].tried = true;
        }
        bt.name_pending = false;
        break;
    }
    case 0x3E: { /* LE Meta */
        if (plen < 2 || p[0] != 0x02) break;   /* só LE Advertising Report */
        int n = p[1], o = 2;
        for (int i = 0; i < n && o + 9 <= plen; i++) {
            const u8 *r = p + o;
            u8 dl = r[8];
            if (o + 9 + dl + 1 > plen) break;
            btdev_t *f = find_or_add(r + 2, true);
            if (f) { ad_name(f, r + 9, dl); f->rssi = (i8)r[9 + dl]; f->tried = true; }
            o += 10 + dl;
        }
        break;
    }
    case 0x10:   /* Hardware Error */
        kprintf("Bluetooth: erro de hardware 0x%02x\n", plen ? p[0] : 0);
        break;
    default:
        break;
    }
}

/* captura de eventos durante a configuração do fabricante (bt_wait_event) */
#define CAPQ 8
static hci_ev_t capq[CAPQ];
static u32 cap_h, cap_t;
static bool capturing;

static void drain(void) {
    while (bt.qt != bt.qh) {
        hci_ev_t *e = &bt.q[bt.qt];
        if (capturing && e->len >= 2) {
            u8 c = e->data[0];
            bool is_reply = bt.wait_op && (c == 0x0E || c == 0x0F);
            u32 nx = (cap_h + 1) % CAPQ;
            if (!is_reply && nx != cap_t) { capq[cap_h] = *e; cap_h = nx; }
        }
        handle_event(e->data, e->len);
        if (bt_on_event) bt_on_event(e->data, e->len);
        bt.qt = (bt.qt + 1) % EVQ;
    }
    while (acl_t != acl_h) {
        if (bt_on_acl) bt_on_acl(aclq[acl_t].data, aclq[acl_t].len);
        acl_t = (acl_t + 1) % ACLQ;
    }
}

/* bulk IN de ACL: remonta pacotes (cabeçalho de 4 bytes: handle+flags, tamanho) */
static void acl_cb(usb_dev_t *d, u8 ep, u8 *data, u32 len, int st, void *ctx) {
    for (u32 i = 0; i < len; i++) {
        if (acl_asm_len < sizeof acl_asm) acl_asm[acl_asm_len++] = data[i];
        while (acl_asm_len >= 4) {
            u32 n = 4 + (acl_asm[2] | (u32)acl_asm[3] << 8);
            if (n > sizeof aclq[0].data) { acl_asm_len = 0; break; }   /* pacote inválido: descarta */
            if (acl_asm_len < n) break;
            u32 nx = (acl_h + 1) % ACLQ;
            if (nx != acl_t) { aclq[acl_h].len = (u16)n; memcpy(aclq[acl_h].data, acl_asm, n); acl_h = nx; }
            memmove(acl_asm, acl_asm + n, acl_asm_len - n);
            acl_asm_len -= n;
        }
    }
}

/* API pública (bluetooth.h): comando HCI síncrono e envio de ACL */
int bt_hci_cmd(u16 opcode, const void *p, u8 len, int timeout_ms) {
    if (!bt.present || !bt.d) return -1;
    return hci_cmd(opcode, p, len, (u32)MAX(timeout_ms, 1));
}

static int hci_send(u16 op, const void *p, u8 plen);

/* como bt_hci_cmd, mas devolve os parâmetros do Command Complete (status incluído) em resp */
int bt_hci_cmd_resp(u16 opcode, const void *p, u8 len, u8 *resp, int max, int timeout_ms) {
    if (!bt.present || !bt.d) return -1;
    int r = hci_cmd(opcode, p, len, (u32)MAX(timeout_ms, 1));
    if (r < 0) return r;
    int n = MIN((int)bt.ret_len, max);
    if (resp && n > 0) memcpy(resp, bt.ret, n);
    return n;
}

/* envia sem esperar resposta (ex.: Intel Reset 0xFC01, que reinicia o chip) */
bool bt_hci_cmd_async(u16 opcode, const void *p, u8 len) {
    if (!bt.present || !bt.d) return false;
    return hci_send(opcode, p, len) >= 0;
}

/* próximo evento HCI recebido durante bt_vendor_setup ([código, tamanho, params...]); <0 em timeout */
int bt_wait_event(u8 *buf, int max, int timeout_ms) {
    if (!bt.present || !bt.d) return -1;
    u64 end = timer_ticks() + (u64)MAX(timeout_ms, 1);
    for (;;) {
        if (cap_t != cap_h) {
            hci_ev_t *e = &capq[cap_t];
            int n = MIN((int)e->len, max);
            memcpy(buf, e->data, n);
            cap_t = (cap_t + 1) % CAPQ;
            return n;
        }
        usb_events();
        drain();
        if (cap_t != cap_h) continue;
        if (!bt.d || timer_ticks() > end) return -2;
        sleep_ms(1);
    }
}

/* configuração do fabricante (firmware Intel/Realtek): bt_vendor_setup fica em kernel/bt/vendor.c */
bool bt_vendor_setup(u16 vid, u16 pid);

bool bt_acl_send(u16 handle, const void *data, u16 len) {
    if (!bt.present || !bt.d || !bt.ep_out || len > 1021) return false;
    static u8 buf[1028];
    u16 h = (handle & 0x0FFF) | 0x2000;   /* PB = primeiro pacote automaticamente descartável, BC = ponto a ponto */
    buf[0] = (u8)h; buf[1] = (u8)(h >> 8); buf[2] = (u8)len; buf[3] = (u8)(len >> 8);
    memcpy(buf + 4, data, len);
    return usb_bulk(bt.d, bt.ep_out, buf, 4u + len, 1000) == (int)(4u + len);
}

/* ------------------------------------------------------------------ comandos */
static int hci_send(u16 op, const void *p, u8 plen) {
    u8 b[260];
    b[0] = op; b[1] = op >> 8; b[2] = plen;
    if (plen) memcpy(b + 3, p, plen);
    return usb_control(bt.d, 0x20, 0, 0, bt.iface, 3 + plen, b);
}

/* envia e espera Command Complete/Status.  Retorna o status HCI (0 = ok), ou <0 em erro de transporte */
static int hci_cmd(u16 op, const void *p, u8 plen, u32 timeout) {
    if (!bt.d) return -1;
    bt.wait_op = op;
    bt.wait_done = false;
    bt.ret_len = 0;
    if (hci_send(op, p, plen) < 0) { bt.wait_op = 0; kprintf("Bluetooth: falha ao enviar comando %04x\n", op); return -1; }
    u64 end = timer_ticks() + timeout;
    while (!bt.wait_done) {
        usb_events();
        drain();
        if (bt.wait_done || !bt.d) break;
        if (timer_ticks() > end) { bt.wait_op = 0; kprintf("Bluetooth: comando %04x sem resposta\n", op); return -2; }
        sleep_ms(1);
    }
    bt.wait_op = 0;
    if (!bt.wait_done) return -1;
    return bt.ret_len ? bt.ret[0] : 0;
}

static void make_status(void) {
    const char *v = company(bt.manuf);
    if (!v && bt.d) v = vendor_from_usb(bt.d->desc.idVendor);
    if (!v) v = "adaptador genérico";
    char a[18];
    addr_str(bt.bdaddr, a);
    snprintf(bt.status, sizeof bt.status, "Ligado — %s, Bluetooth %s%s (BD_ADDR %s)%s", v, bt_version(bt.hci_ver),
             bt.le ? " LE" : "", a, bt.scanning || bt.le_scanning ? " — procurando…" : "");
}

static bool bt_ready_for_setup;   /* bt_init já rodou (VFS pronto: o firmware pode ser lido) */
static bool bt_pending;           /* adaptador enumerado antes do VFS: configura no bt_init */

static void bt_bringup(void) {
    bt_pending = false;
    cap_h = cap_t = 0;
    capturing = true;
    bool ok = bt_vendor_setup(bt.d->desc.idVendor, bt.d->desc.idProduct);
    capturing = false;
    if (!bt.d) return;
    if (!ok) {
        snprintf(bt.status, sizeof bt.status, "Adaptador %04x:%04x encontrado, mas o firmware não foi carregado",
                 bt.d->desc.idVendor, bt.d->desc.idProduct);
        kprintf("Bluetooth: %s\n", bt.status);
        return;
    }
    int r = hci_cmd(0x0C03, NULL, 0, 3000);                   /* HCI_Reset */
    if (r != 0) {
        const char *v = vendor_from_usb(bt.d->desc.idVendor);
        snprintf(bt.status, sizeof bt.status, "Adaptador %s encontrado, mas não respondeu ao HCI Reset (%d)%s", v ? v : "Bluetooth", r,
                 bt.d->desc.idVendor == 0x8087 || bt.d->desc.idVendor == 0x0E8D ? " — provavelmente exige firmware" : "");
        kprintf("Bluetooth: %s\n", bt.status);
        return;
    }
    if (bt.d->desc.idVendor == 0x8087) {                     /* Intel: versão / modo bootloader */
        if (hci_cmd(0xFC05, NULL, 0, 1000) == 0 && bt.ret_len >= 10)
            kprintf("Bluetooth: Intel hw %02x/%02x/%02x, fw variante %02x%s\n", bt.ret[1], bt.ret[2], bt.ret[3], bt.ret[4],
                    bt.ret[4] == 0x06 ? " (bootloader: sem firmware operacional, funções limitadas)" : "");
    }
    if (hci_cmd(0x1001, NULL, 0, 1000) == 0 && bt.ret_len >= 9) {   /* Read Local Version Information */
        bt.hci_ver = bt.ret[1];
        bt.lmp_ver = bt.ret[4];
        bt.manuf = bt.ret[5] | bt.ret[6] << 8;
        bt.lmp_sub = bt.ret[7] | bt.ret[8] << 8;
    }
    if (hci_cmd(0x1009, NULL, 0, 1000) == 0 && bt.ret_len >= 7) memcpy(bt.bdaddr, bt.ret + 1, 6);   /* Read BD_ADDR */
    if (hci_cmd(0x0C14, NULL, 0, 1000) == 0 && bt.ret_len >= 2) {                                   /* Read Local Name */
        int n = 0;
        while (n < (int)sizeof bt.local_name - 1 && 1 + n < bt.ret_len && bt.ret[1 + n]) { bt.local_name[n] = bt.ret[1 + n]; n++; }
        bt.local_name[n] = 0;
    }
    if (hci_cmd(0x1003, NULL, 0, 1000) == 0 && bt.ret_len >= 9) bt.le = (bt.ret[1 + 4] >> 6) & 1;   /* LMP features: LE */
    static const u8 mask[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x1F, 0x00, 0x20};                       /* padrão + LE Meta */
    hci_cmd(0x0C01, mask, 8, 1000);                                                                 /* Set Event Mask */
    u8 mode = 2;
    if (hci_cmd(0x0C45, &mode, 1, 1000) != 0) { mode = 1; hci_cmd(0x0C45, &mode, 1, 1000); }          /* Write Inquiry Mode */
    u8 nm[248];
    memset(nm, 0, sizeof nm);
    strcpy((char *)nm, "mOS");
    hci_cmd(0x0C13, nm, 248, 1000);                                                                 /* Write Local Name */
    u8 scan = 0x03;
    hci_cmd(0x0C1A, &scan, 1, 1000);                                                                /* Write Scan Enable */
    bt.up = true;
    extern void bt_stack_init(void);
    bt_stack_init();                 /* pilha superior (kernel/bt/ble.c): GATT/HOGP, reconexão */
    make_status();
    char a[18];
    addr_str(bt.bdaddr, a);
    kprintf("Bluetooth: %s; HCI %s, LMP %s sub %04x, fabricante %u, nome local \"%s\"\n", bt.status, bt_version(bt.hci_ver),
            bt_version(bt.lmp_ver), bt.lmp_sub, bt.manuf, bt.local_name);
}

/* ------------------------------------------------------------------ driver USB */
static bool bt_probe(usb_dev_t *d, usb_iface_t *it) {
    bool cls = it->cls == 0xE0 && it->sub == 0x01 && it->proto == 0x01;
    if (!cls) return false;
    if (bt.present) { kprintf("Bluetooth: segundo adaptador ignorado\n"); return false; }
    int ie, ii, io;
    if (!usb_find_ep(it, USB_EP_INTR, true, &ie)) { kprintf("Bluetooth: interface HCI sem endpoint de eventos\n"); return false; }
    memset(&bt, 0, sizeof bt);
    nfound = 0;
    bt.present = true;
    bt.d = d;
    bt.iface = it->num;
    bt.ep_evt = it->ep[ie].bEndpointAddress;
    usb_add_ep(d, it, ie);
    if (usb_find_ep(it, USB_EP_BULK, true, &ii)) { bt.ep_in = it->ep[ii].bEndpointAddress; usb_add_ep(d, it, ii); }
    if (usb_find_ep(it, USB_EP_BULK, false, &io)) { bt.ep_out = it->ep[io].bEndpointAddress; usb_add_ep(d, it, io); }
    it->priv = &bt;
    return true;
}

static void bt_start(usb_dev_t *d, usb_iface_t *it) {
    usb_set_kind(d, USB_KIND_BLUETOOTH);
    kprintf("Bluetooth: adaptador USB %04x:%04x (eventos %02x, ACL %02x/%02x)\n", d->desc.idVendor, d->desc.idProduct,
            bt.ep_evt, bt.ep_in, bt.ep_out);
    u16 mps = 16;
    for (int i = 0; i < it->nep; i++) if (it->ep[i].bEndpointAddress == bt.ep_evt) mps = it->ep[i].wMaxPacketSize & 0x7FF;
    if (usb_intr_start(d, bt.ep_evt, MAX(mps, 16), evt_cb, NULL) < 0) {
        snprintf(bt.status, sizeof bt.status, "Adaptador Bluetooth encontrado, mas o endpoint de eventos falhou");
        return;
    }
    if (bt.ep_in && usb_intr_start(d, bt.ep_in, 1024, acl_cb, NULL) < 0) kprintf("Bluetooth: bulk IN de ACL não iniciou\n");
    if (bt_ready_for_setup) bt_bringup();
    else { bt_pending = true; snprintf(bt.status, sizeof bt.status, "Adaptador Bluetooth inicializando…"); }
}

static void bt_detach(usb_dev_t *d, usb_iface_t *it) {
    if (bt.d != d) return;
    kprintf("Bluetooth: adaptador removido\n");
    memset(&bt, 0, sizeof bt);
    nfound = 0;
    acl_h = acl_t = 0; acl_asm_len = 0;
}

void bt_poll(void) {
    if (!bt.present || !bt.up) return;
    drain();
    u64 now = timer_ticks();
    if (bt.scanning && (bt.inq_done || now > bt.scan_end)) {
        if (!bt.inq_done) hci_cmd(0x0402, NULL, 0, 1000);     /* Inquiry Cancel */
        bt.scanning = false;
        kprintf("Bluetooth: inquiry concluído, %d dispositivo(s)\n", nfound);
        make_status();
    }
    if (bt.le_scanning && now > bt.le_end) {
        u8 dis[2] = {0, 0};
        hci_cmd(0x200C, dis, 2, 1000);
        bt.le_scanning = false;
        make_status();
    }
    /* resolve nomes clássicos, um de cada vez, depois do inquiry */
    if (!bt.scanning && bt.name_pending && now > bt.name_deadline) {
        hci_cmd(0x041A, found[bt.name_idx].addr, 6, 1000);   /* Remote Name Request Cancel */
        found[bt.name_idx].tried = true;
        bt.name_pending = false;
    }
    if (!bt.scanning && !bt.name_pending) {
        for (int i = 0; i < nfound; i++) {
            btdev_t *f = &found[i];
            if (f->le || f->named || f->tried) continue;
            u8 p[10];
            memcpy(p, f->addr, 6);
            p[6] = f->psrm; p[7] = 0; p[8] = f->clk; p[9] = (f->clk >> 8) | 0x80;
            f->tried = true;
            if (hci_cmd(0x0419, p, 10, 1000) == 0) {
                bt.name_idx = i;
                bt.name_pending = true;
                bt.name_deadline = now + 6000;
            }
            break;
        }
    }
    extern void bt_stack_poll(void);
    bt_stack_poll();
}

static void bt_drv_poll(void) { bt_poll(); }

const usb_driver_t usb_bt_driver = { "bluetooth", bt_probe, bt_start, bt_detach, bt_drv_poll };

/* ------------------------------------------------------------------ API pública */
void bt_init(void) {
    bt_ready_for_setup = true;
    if (bt.present && bt_pending && bt.d) bt_bringup();
    if (bt.present) kprintf("Bluetooth: %s\n", bt.status);
    else kprintf("Bluetooth: nenhum adaptador USB HCI encontrado\n");
}

const char *bt_status(void) {
    if (!bt.present) return "Nenhum adaptador Bluetooth encontrado";
    if (bt.up) make_status();
    return bt.status[0] ? bt.status : "Adaptador Bluetooth inicializando…";
}

int bt_device_count(void) { return nfound; }

const char *bt_device_name(int i) {
    if (i < 0 || i >= nfound) return "";
    btdev_t *f = &found[i];
    char a[18];
    addr_str(f->addr, a);
    if (f->named) snprintf(dev_names[i], sizeof dev_names[i], "%s (%s)%s", f->name, a, f->le ? " — LE" : "");
    else if (f->le) snprintf(dev_names[i], sizeof dev_names[i], "Dispositivo LE (%s)", a);
    else snprintf(dev_names[i], sizeof dev_names[i], "%s (%s)%s", cod_name(f->cod), a, f->tried ? "" : " — obtendo nome…");
    return dev_names[i];
}

void bt_scan(void) {
    if (!bt.present || !bt.up) { kprintf("Bluetooth: procurar ignorado (%s)\n", bt_status()); return; }
    if (bt.scanning || bt.le_scanning) return;
    nfound = 0;
    bt.name_pending = false;
    bt.inq_done = false;
    static const u8 inq[5] = {0x33, 0x8B, 0x9E, 6, 0};      /* GIAC, 6 × 1,28 s, sem limite */
    int r = hci_cmd(0x0401, inq, 5, 1500);
    if (r == 0) { bt.scanning = true; bt.scan_end = timer_ticks() + 10000; }
    else kprintf("Bluetooth: Inquiry recusado (%d)\n", r);
    if (bt.le) {
        static const u8 sp[7] = {0x01, 0x10, 0x00, 0x10, 0x00, 0x00, 0x00};   /* ativo, 10 ms/10 ms */
        static const u8 en[2] = {0x01, 0x01};
        if (hci_cmd(0x200B, sp, 7, 1000) == 0 && hci_cmd(0x200C, en, 2, 1000) == 0) {
            bt.le_scanning = true;
            bt.le_end = timer_ticks() + 6000;
        }
    }
    kprintf("Bluetooth: procurando dispositivos (clássico%s)…\n", bt.le_scanning ? " + LE" : "");
    make_status();
}
