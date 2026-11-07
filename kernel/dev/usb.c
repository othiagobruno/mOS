/*
 * mOS — núcleo da pilha USB: enumeração de dispositivos, tabela de dispositivos,
 * despacho para drivers de classe e as funções de status usadas pela interface.
 */
#include "usb.h"

static usb_dev_t devs[USB_MAX_DEVS];
static const usb_driver_t *drivers[] = { &usb_hub_driver, &usb_hid_driver, &usb_msc_driver, &usb_bt_driver, &usb_xinput_driver };
#define NDRIVERS ((int)ARRAY_LEN(drivers))

/* ------------------------------------------------------------------ transferências */
int usb_control(usb_dev_t *d, u8 rt, u8 req, u16 val, u16 idx, u16 len, void *data) {
    if (!d || !d->used) return USB_NODEV;
    return xhci_control(d, rt, req, val, idx, len, data, 2000);
}
int usb_bulk(usb_dev_t *d, u8 ep, void *buf, u32 len, u32 timeout_ms) {
    if (!d || !d->used) return USB_NODEV;
    return xhci_bulk(d, ep, buf, len, timeout_ms);
}
int usb_intr_start(usb_dev_t *d, u8 ep, u16 len, usb_xfer_cb cb, void *ctx) {
    if (!d || !d->used) return USB_NODEV;
    return xhci_intr_start(d, ep, len, cb, ctx);
}
int usb_clear_halt(usb_dev_t *d, u8 ep) {
    if (!d || !d->used) return USB_NODEV;
    xhci_reset_endpoint(d, ep);
    return usb_control(d, 0x02, 1 /* CLEAR_FEATURE */, 0 /* ENDPOINT_HALT */, ep, 0, NULL);
}
int usb_get_descriptor(usb_dev_t *d, u8 type, u8 index, u16 lang, void *buf, u16 len) {
    return usb_control(d, 0x80, 6, ((u16)type << 8) | index, lang, len, buf);
}
int usb_hub_configure(usb_dev_t *d, int nports, bool mtt, int ttt) { return xhci_set_hub(d, nports, mtt, ttt); }
void usb_events(void) { xhci_process_events(); }

int usb_add_ep(usb_dev_t *d, usb_iface_t *it, int idx) {
    if (idx < 0 || idx >= it->nep) return USB_ERR;
    return xhci_add_endpoint(d, &it->ep[idx], it->ss_burst[idx]);
}

usb_ep_desc_t *usb_find_ep(usb_iface_t *it, u8 type, bool in, int *idx) {
    for (int i = 0; i < it->nep; i++) {
        usb_ep_desc_t *e = &it->ep[i];
        if ((e->bmAttributes & 3) == type && !!(e->bEndpointAddress & 0x80) == in) {
            if (idx) *idx = i;
            return e;
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------ strings */
static u16 dev_lang(usb_dev_t *d) {
    u8 b[8];
    int n = usb_get_descriptor(d, USB_DT_STRING, 0, 0, b, sizeof b);
    return n >= 4 ? (u16)(b[2] | b[3] << 8) : 0x0409;
}

static void get_string(usb_dev_t *d, u16 lang, u8 idx, char *out, int max) {
    out[0] = 0;
    if (!idx) return;
    u8 b[256];
    int n = usb_get_descriptor(d, USB_DT_STRING, idx, lang, b, 255);
    if (n < 2) return;
    int len = MIN(b[0], n), o = 0;
    for (int i = 2; i + 1 < len && o < max - 4; i += 2) {
        u16 c = b[i] | b[i + 1] << 8;
        if (c < 0x20) continue;
        if (c < 0x80) out[o++] = (char)c;
        else if (c < 0x800) { out[o++] = 0xC0 | (c >> 6); out[o++] = 0x80 | (c & 0x3F); }
        else { out[o++] = 0xE0 | (c >> 12); out[o++] = 0x80 | ((c >> 6) & 0x3F); out[o++] = 0x80 | (c & 0x3F); }
    }
    while (o > 0 && out[o - 1] == ' ') o--;
    out[o] = 0;
}

/* ------------------------------------------------------------------ parsing da configuração */
static void parse_config(usb_dev_t *d) {
    u8 *p = d->config, *end = d->config + d->config_len;
    usb_iface_t *cur = NULL;
    int last_ep = -1;
    d->nifaces = 0;
    while (p + 2 <= end && p[0] >= 2 && p + p[0] <= end) {
        u8 len = p[0], type = p[1];
        if (type == USB_DT_INTERFACE && len >= 9) {
            usb_iface_desc_t *id = (usb_iface_desc_t *)p;
            cur = NULL; last_ep = -1;
            if (id->bAlternateSetting == 0 && d->nifaces < USB_MAX_IFACES) {
                cur = &d->iface[d->nifaces++];
                memset(cur, 0, sizeof *cur);
                cur->num = id->bInterfaceNumber; cur->cls = id->bInterfaceClass;
                cur->sub = id->bInterfaceSubClass; cur->proto = id->bInterfaceProtocol;
                cur->driver = -1;
                cur->extra = p + len;
            }
        } else if (type == USB_DT_ENDPOINT && len >= 7 && cur) {
            if (cur->nep < USB_MAX_EPS) { memcpy(&cur->ep[cur->nep], p, 7); last_ep = cur->nep++; }
        } else if (type == USB_DT_SS_EP_COMP && len >= 6 && cur && last_ep >= 0) {
            cur->ss_burst[last_ep] = p[2];
        } else if (cur && cur->nep == 0) {
            cur->extra_len = (u16)(p + len - cur->extra);
        }
        p += len;
    }
}

static const char *kind_name(int k) {
    switch (k) {
    case USB_KIND_KEYBOARD: return "Teclado USB";
    case USB_KIND_MOUSE: return "Mouse USB";
    case USB_KIND_TABLET: return "Apontador absoluto USB";
    case USB_KIND_COMBO: return "Teclado e mouse USB";
    case USB_KIND_HID: return "Dispositivo HID USB";
    case USB_KIND_HUB: return "Hub USB";
    case USB_KIND_STORAGE: return "Armazenamento USB";
    case USB_KIND_BLUETOOTH: return "Adaptador Bluetooth USB";
    default: return "Dispositivo USB";
    }
}

static void build_desc(usb_dev_t *d) {
    const char *k = kind_name(d->kind);
    char name[100];
    if (d->product[0] && d->manufacturer[0] && !strstr(d->product, d->manufacturer)) snprintf(name, sizeof name, " — %s %s", d->manufacturer, d->product);
    else if (d->product[0]) snprintf(name, sizeof name, " — %s", d->product);
    else name[0] = 0;
    if (d->kind == USB_KIND_UNKNOWN) {
        u8 c = d->desc.bDeviceClass ? d->desc.bDeviceClass : d->nifaces ? d->iface[0].cls : 0;
        snprintf(d->desc_str, sizeof d->desc_str, "%s classe %02x%s (%04x:%04x) — porta %s", k, c, name,
                 d->desc.idVendor, d->desc.idProduct, d->path);
    } else
        snprintf(d->desc_str, sizeof d->desc_str, "%s%s (%04x:%04x) — porta %s", k, name, d->desc.idVendor, d->desc.idProduct, d->path);
}

void usb_set_kind(usb_dev_t *d, int kind) {
    if (d->kind == USB_KIND_UNKNOWN || d->kind == USB_KIND_HID) d->kind = kind;
    else if ((d->kind == USB_KIND_KEYBOARD && (kind == USB_KIND_MOUSE || kind == USB_KIND_TABLET)) ||
             ((d->kind == USB_KIND_MOUSE || d->kind == USB_KIND_TABLET) && kind == USB_KIND_KEYBOARD)) d->kind = USB_KIND_COMBO;
    if (d->configured) build_desc(d);
}

/* ------------------------------------------------------------------ enumeração */
static const char *spd(u8 s) {
    return s == USB_SPEED_LOW ? "LS" : s == USB_SPEED_FULL ? "FS" : s == USB_SPEED_HIGH ? "HS" : s == USB_SPEED_SUPER ? "SS" : s == USB_SPEED_SUPERPLUS ? "SS+" : "?";
}

static void free_dev(usb_dev_t *d) {
    if (d->hcpriv) xhci_free_device(d);
    kfree(d->config);
    memset(d, 0, sizeof *d);
}

usb_dev_t *usb_attach(void *hc, usb_dev_t *parent, u8 port, u8 speed) {
    usb_dev_t *d = NULL;
    for (int i = 0; i < USB_MAX_DEVS; i++) if (!devs[i].used) { d = &devs[i]; d->id = i; break; }
    if (!d) { kprintf("USB: tabela de dispositivos cheia\n"); return NULL; }
    int id = d->id;
    memset(d, 0, sizeof *d);
    d->id = id; d->used = true; d->hc = hc; d->parent = parent; d->speed = speed; d->port = port;
    if (parent) {
        if (parent->depth >= 5) { kprintf("USB: hubs encadeados demais\n"); d->used = false; return NULL; }
        d->root_port = parent->root_port;
        d->depth = parent->depth + 1;
        d->route = parent->route | ((u32)MIN(port, 15) << (4 * parent->depth));
        /* LS/FS atrás de um hub HS: o Transaction Translator é o hub HS mais próximo */
        if (speed == USB_SPEED_LOW || speed == USB_SPEED_FULL) {
            if (parent->speed == USB_SPEED_HIGH) { d->tt_slot = (u8)xhci_slot_of(parent); d->tt_port = port; }
            else { d->tt_slot = parent->tt_slot; d->tt_port = parent->tt_port; }
        }
        snprintf(d->path, sizeof d->path, "%s.%u", parent->path, port);
    } else {
        d->root_port = port;
        snprintf(d->path, sizeof d->path, "%u", port);
    }

    if (xhci_address_device(d) != USB_OK) { free_dev(d); return NULL; }

    /* descritor do dispositivo: 8 bytes primeiro para descobrir bMaxPacketSize0 */
    u8 b8[8];
    int r = USB_ERR;
    for (int t = 0; t < 3 && r < 8; t++) {
        r = usb_get_descriptor(d, USB_DT_DEVICE, 0, 0, b8, 8);
        if (r < 8) sleep_ms(20);
    }
    if (r < 8) { kprintf("USB: porta %s: GET_DESCRIPTOR(dispositivo) falhou (%d)\n", d->path, r); free_dev(d); return NULL; }
    u16 mps0 = speed >= USB_SPEED_SUPER ? (u16)(1u << MIN(b8[7], 9)) : b8[7];
    if (mps0 != 8 && mps0 != 16 && mps0 != 32 && mps0 != 64 && mps0 != 512) mps0 = d->mps0;
    if (mps0 != d->mps0) { d->mps0 = mps0; if (xhci_update_ep0(d) != USB_OK) kprintf("USB: Evaluate Context falhou\n"); }
    r = usb_get_descriptor(d, USB_DT_DEVICE, 0, 0, &d->desc, sizeof d->desc);
    if (r < (int)sizeof d->desc) { kprintf("USB: porta %s: descritor de dispositivo curto (%d)\n", d->path, r); free_dev(d); return NULL; }

    /* configuração 0 */
    usb_cfg_desc_t ch;
    r = usb_get_descriptor(d, USB_DT_CONFIG, 0, 0, &ch, sizeof ch);
    if (r < (int)sizeof ch) { kprintf("USB: porta %s: descritor de configuração falhou (%d)\n", d->path, r); free_dev(d); return NULL; }
    u16 total = MIN(ch.wTotalLength, 4096);
    d->config = kmalloc(total);
    if (!d->config) { free_dev(d); return NULL; }
    r = usb_get_descriptor(d, USB_DT_CONFIG, 0, 0, d->config, total);
    if (r < (int)sizeof ch) { kprintf("USB: porta %s: configuração completa falhou (%d)\n", d->path, r); free_dev(d); return NULL; }
    d->config_len = (u16)r;

    if (d->desc.iManufacturer || d->desc.iProduct) {
        u16 lang = dev_lang(d);
        get_string(d, lang, d->desc.iManufacturer, d->manufacturer, sizeof d->manufacturer);
        get_string(d, lang, d->desc.iProduct, d->product, sizeof d->product);
    }
    parse_config(d);
    kprintf("USB: [%s] %04x:%04x %s USB %x.%02x classe %02x/%02x/%02x \"%s %s\", %d interface(s)\n", d->path,
            d->desc.idVendor, d->desc.idProduct, spd(speed), d->desc.bcdUSB >> 8, d->desc.bcdUSB & 0xFF,
            d->desc.bDeviceClass, d->desc.bDeviceSubClass, d->desc.bDeviceProtocol, d->manufacturer, d->product, d->nifaces);

    /* drivers reivindicam interfaces e registram endpoints */
    for (int i = 0; i < d->nifaces; i++) {
        usb_iface_t *it = &d->iface[i];
        kprintf("USB:   interface %u classe %02x/%02x/%02x, %u endpoint(s)\n", it->num, it->cls, it->sub, it->proto, it->nep);
        for (int k = 0; k < NDRIVERS; k++)
            if (drivers[k]->probe(d, it)) { it->driver = k; break; }
    }
    if (xhci_commit_endpoints(d) != USB_OK) kprintf("USB: porta %s: endpoints não configurados\n", d->path);
    r = usb_control(d, 0x00, 9 /* SET_CONFIGURATION */, ((usb_cfg_desc_t *)d->config)->bConfigurationValue, 0, 0, NULL);
    if (r < 0) kprintf("USB: porta %s: SET_CONFIGURATION falhou (%d)\n", d->path, r);
    d->configured = true;
    if (d->desc.bDeviceClass == 9) usb_set_kind(d, USB_KIND_HUB);
    for (int i = 0; i < d->nifaces; i++) {
        usb_iface_t *it = &d->iface[i];
        if (it->driver >= 0 && drivers[it->driver]->start) drivers[it->driver]->start(d, it);
        if (!d->used) return NULL;   /* removido durante o start */
    }
    build_desc(d);
    kprintf("USB: %s\n", d->desc_str);
    return d;
}

void usb_detach(usb_dev_t *d) {
    if (!d || !d->used) return;
    for (int i = 0; i < USB_MAX_DEVS; i++)
        if (devs[i].used && devs[i].parent == d) usb_detach(&devs[i]);
    kprintf("USB: removido: %s\n", d->desc_str[0] ? d->desc_str : d->path);
    usb_dev_t *parent = d->parent;
    if (d->hcpriv) xhci_free_device(d);     /* para callbacks antes de liberar o estado dos drivers */
    for (int i = 0; i < d->nifaces; i++) {
        usb_iface_t *it = &d->iface[i];
        if (it->driver >= 0 && drivers[it->driver]->detach) drivers[it->driver]->detach(d, it);
    }
    if (parent) usb_hub_child_gone(parent, d);
    free_dev(d);
}

/* ------------------------------------------------------------------ consulta */
usb_dev_t *usb_dev_get(int i) {
    for (int k = 0; k < USB_MAX_DEVS; k++)
        if (devs[k].used && devs[k].configured && i-- == 0) return &devs[k];
    return NULL;
}

int usb_device_count(void) {
    int n = 0;
    for (int k = 0; k < USB_MAX_DEVS; k++) if (devs[k].used && devs[k].configured) n++;
    return n;
}

const char *usb_device_desc(int i) {
    usb_dev_t *d = usb_dev_get(i);
    return d ? d->desc_str : "";
}

const char *usb_status(void) {
    static char buf[128];
    int n = usb_device_count();
    if (!xhci_count()) {
        bool ehci = false, other = false;
        for (int i = 0; i < pci_count(); i++) {
            pci_dev_t *p = pci_get(i);
            if (p->class_ == 0x0C && p->subclass == 3) { if (p->progif == 0x20) ehci = true; else other = true; }
        }
        if (ehci) return "Só há controlador USB 2.0 (EHCI) — sem driver; apenas xHCI é suportado";
        if (other) return "Controlador USB 1.1 (UHCI/OHCI) sem driver; apenas xHCI é suportado";
        return "Nenhum controlador USB xHCI encontrado";
    }
    if (xhci_count() == 1) snprintf(buf, sizeof buf, "xHCI: %d portas, %d dispositivo%s", xhci_total_ports(), n, n == 1 ? "" : "s");
    else snprintf(buf, sizeof buf, "%d controladores xHCI: %d portas, %d dispositivo%s", xhci_count(), xhci_total_ports(), n, n == 1 ? "" : "s");
    return buf;
}

void usb_init(void) {
    u64 t0 = timer_ticks();
    xhci_init_all();
    kprintf("USB: %s (%llu ms)\n", usb_status(), timer_ticks() - t0);
}

void usb_poll(void) {
    static bool busy;
    if (busy) return;
    busy = true;
    xhci_poll();
    for (int k = 0; k < NDRIVERS; k++) if (drivers[k]->poll) drivers[k]->poll();
    busy = false;
}
