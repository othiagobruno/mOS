/*
 * mOS — driver de hub USB 2.0 (e 1.1).  Funciona em cascata (hub atrás de hub).
 * Hubs SuperSpeed (USB 3) não são tratados: os dispositivos USB 2 atrás deles aparecem pela
 * metade USB 2 do mesmo hub, que é enumerada numa porta USB 2 do controlador.
 *
 * O endpoint de status (interrupt IN) só marca quais portas mudaram; o trabalho (GET_STATUS,
 * reset, enumeração) é feito em hub_poll(), fora do processamento de eventos.
 */
#include "usb.h"

#define MAX_HUBS 8
#define HUB_PORTS 15

enum { PORT_CONNECTION = 0, PORT_ENABLE = 1, PORT_RESET = 4, PORT_POWER = 8,
       C_PORT_CONNECTION = 16, C_PORT_ENABLE = 17, C_PORT_SUSPEND = 18, C_PORT_OVER_CURRENT = 19, C_PORT_RESET = 20 };

typedef struct {
    bool used, ok, has_intr;
    usb_dev_t *d;
    u8  ep, nports;
    volatile u32 change;
    usb_dev_t *child[HUB_PORTS + 1];
    u8  fail[HUB_PORTS + 1];
    u64 next_scan;
} hub_t;

static hub_t hubs[MAX_HUBS];

static int port_status(hub_t *h, int port, u16 *st, u16 *ch) {
    u8 b[4];
    int r = usb_control(h->d, 0xA3, 0 /* GET_STATUS */, 0, port, 4, b);
    if (r < 4) return USB_ERR;
    *st = b[0] | b[1] << 8;
    *ch = b[2] | b[3] << 8;
    return USB_OK;
}
static int set_feat(hub_t *h, int port, int f) { return usb_control(h->d, 0x23, 3, f, port, 0, NULL); }
static int clr_feat(hub_t *h, int port, int f) { return usb_control(h->d, 0x23, 1, f, port, 0, NULL); }

static void hub_attach_port(hub_t *h, int port) {
    sleep_ms(100);                                         /* debounce */
    if (set_feat(h, port, PORT_RESET) < 0) return;
    u16 st = 0, ch = 0;
    u64 end = timer_ticks() + 500;
    do {
        sleep_ms(10);
        if (port_status(h, port, &st, &ch) < 0) return;
    } while (!(ch & (1 << 4)) && (st & (1 << 4)) && timer_ticks() < end);
    clr_feat(h, port, C_PORT_RESET);
    sleep_ms(10);                                          /* recuperação após reset */
    if (port_status(h, port, &st, &ch) < 0) return;
    if (!(st & 1)) return;
    if (!(st & 2)) { kprintf("USB hub [%s]: porta %d não habilitou após reset\n", h->d->path, port); h->fail[port]++; return; }
    u8 speed = (st & (1 << 9)) ? USB_SPEED_LOW : (st & (1 << 10)) ? USB_SPEED_HIGH : USB_SPEED_FULL;
    if (h->d->speed < USB_SPEED_HIGH && speed == USB_SPEED_HIGH) speed = USB_SPEED_FULL;
    kprintf("USB hub [%s]: dispositivo na porta %d (%s)\n", h->d->path, port,
            speed == USB_SPEED_LOW ? "low speed" : speed == USB_SPEED_HIGH ? "high speed" : "full speed");
    h->child[port] = usb_attach(h->d->hc, h->d, (u8)port, speed);
    if (!h->child[port]) { h->fail[port]++; clr_feat(h, port, PORT_ENABLE); }
}

static void hub_service(hub_t *h, u32 mask) {
    for (int p = 1; p <= h->nports; p++) {
        if (!(mask & (1u << p))) continue;
        u16 st, ch;
        if (port_status(h, p, &st, &ch) < 0) continue;
        if (ch & 1) clr_feat(h, p, C_PORT_CONNECTION);
        if (ch & 2) clr_feat(h, p, C_PORT_ENABLE);
        if (ch & 4) clr_feat(h, p, C_PORT_SUSPEND);
        if (ch & 8) { clr_feat(h, p, C_PORT_OVER_CURRENT); kprintf("USB hub [%s]: sobrecorrente na porta %d\n", h->d->path, p); }
        if (ch & 16) clr_feat(h, p, C_PORT_RESET);
        bool conn = st & 1;
        if (h->child[p] && (!conn || (ch & 1))) {
            usb_dev_t *c = h->child[p];
            h->child[p] = NULL;
            usb_detach(c);
            if (!h->used) return;
        }
        if (!conn) { h->fail[p] = 0; continue; }
        if (!h->child[p] && h->fail[p] < 3) hub_attach_port(h, p);
        if (!h->used) return;
    }
}

static void hub_cb(usb_dev_t *d, u8 ep, u8 *data, u32 len, int status, void *ctx) {
    hub_t *h = ctx;
    u32 m = 0;
    for (u32 i = 0; i < len && i < 4; i++) m |= (u32)data[i] << (8 * i);
    h->change |= m & ~1u;     /* bit 0 = status do próprio hub (ignorado) */
}

static bool hub_probe(usb_dev_t *d, usb_iface_t *it) {
    if (it->cls != 9) return false;
    hub_t *h = NULL;
    for (int i = 0; i < MAX_HUBS; i++) if (!hubs[i].used) { h = &hubs[i]; break; }
    if (!h) { kprintf("USB: hubs demais\n"); return false; }
    memset(h, 0, sizeof *h);
    h->used = true; h->d = d;
    it->priv = h;
    int idx;
    if (usb_find_ep(it, USB_EP_INTR, true, &idx)) { h->ep = it->ep[idx].bEndpointAddress; h->has_intr = true; usb_add_ep(d, it, idx); }
    return true;
}

static void hub_start(usb_dev_t *d, usb_iface_t *it) {
    hub_t *h = it->priv;
    if (!h) return;
    usb_set_kind(d, USB_KIND_HUB);
    if (d->speed >= USB_SPEED_SUPER) {
        kprintf("USB hub [%s]: hub SuperSpeed — não suportado (os dispositivos USB 2 aparecem pela metade USB 2 do hub)\n", d->path);
        return;
    }
    u8 b[16];
    int r = usb_control(d, 0xA0, 6, USB_DT_HUB << 8, 0, sizeof b, b);
    if (r < 7) { kprintf("USB hub [%s]: descritor do hub falhou (%d)\n", d->path, r); return; }
    h->nports = MIN(b[2], HUB_PORTS);
    u16 chars = b[3] | b[4] << 8;
    u32 pwr = b[5] * 2u;
    int ttt = (chars >> 5) & 3;
    if (usb_hub_configure(d, h->nports, false, ttt) < 0) kprintf("USB hub [%s]: contexto de hub não atualizado\n", d->path);
    kprintf("USB hub [%s]: %u portas, TT think time %d, energia em %u ms\n", d->path, h->nports, ttt, pwr);
    for (int p = 1; p <= h->nports; p++) set_feat(h, p, PORT_POWER);
    sleep_ms(MAX(pwr, 100u));
    h->ok = true;
    if (h->has_intr && usb_intr_start(d, h->ep, (h->nports + 8) / 8, hub_cb, h) < 0) h->has_intr = false;
    hub_service(h, ~0u);
    h->next_scan = timer_ticks() + 2000;
}

static void hub_detach(usb_dev_t *d, usb_iface_t *it) {
    hub_t *h = it->priv;
    if (!h) return;
    memset(h, 0, sizeof *h);
    it->priv = NULL;
}

void usb_hub_child_gone(usb_dev_t *hub, usb_dev_t *child) {
    for (int i = 0; i < MAX_HUBS; i++) {
        if (!hubs[i].used || hubs[i].d != hub) continue;
        for (int p = 0; p <= HUB_PORTS; p++) if (hubs[i].child[p] == child) hubs[i].child[p] = NULL;
    }
}

static void hub_poll(void) {
    u64 now = timer_ticks();
    for (int i = 0; i < MAX_HUBS; i++) {
        hub_t *h = &hubs[i];
        if (!h->used || !h->ok) continue;
        u32 m = h->change;
        /* sem endpoint de status (ou ele parou): varre periodicamente */
        if (!h->has_intr && now >= h->next_scan) { m = ~0u; h->next_scan = now + 2000; }
        if (!m) continue;
        h->change &= ~m;
        hub_service(h, m);
    }
}

const usb_driver_t usb_hub_driver = { "hub", hub_probe, hub_start, hub_detach, hub_poll };
