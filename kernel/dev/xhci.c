/*
 * mOS — driver do controlador xHCI (eXtensible Host Controller Interface, USB 1.1/2.0/3.x)
 *
 * Sem interrupções: o anel de eventos é consumido por polling (xhci_poll / xhci_process_events).
 * Cada controlador tem um anel de comandos, um anel de eventos (1 segmento), DCBAA e buffers
 * de scratchpad.  Cada endpoint tem seu próprio anel de transferência (256 TRBs + link).
 *
 * Referência: eXtensible Host Controller Interface for USB, rev. 1.2 (Intel).
 */
#include "usb.h"

#define MAX_HC     4
#define MAX_SLOTS  64
#define RING_TRBS  256
#define EVT_TRBS   256
#define TD_MAX     20
#define ASYNC_N    32         /* transferências interrupt IN na fila por endpoint (~1 s de relatórios se o laço principal travar) */

/* tipos de TRB */
enum {
    TRB_NORMAL = 1, TRB_SETUP = 2, TRB_DATA = 3, TRB_STATUS = 4, TRB_LINK = 6,
    TRB_ENABLE_SLOT = 9, TRB_DISABLE_SLOT = 10, TRB_ADDRESS_DEV = 11, TRB_CONFIG_EP = 12, TRB_EVAL_CTX = 13,
    TRB_RESET_EP = 14, TRB_STOP_EP = 15, TRB_SET_DEQ = 16,
    TRB_EV_TRANSFER = 32, TRB_EV_CMD = 33, TRB_EV_PORT = 34, TRB_EV_HOST = 37
};
#define TRB_TC      (1u << 1)
#define TRB_ISP     (1u << 2)
#define TRB_CH      (1u << 4)
#define TRB_IOC     (1u << 5)
#define TRB_IDT     (1u << 6)
#define TRB_DIR_IN  (1u << 16)
#define TRB_TYPE(t) ((u32)(t) << 10)

/* códigos de conclusão */
enum { CC_SUCCESS = 1, CC_DATA_BUF = 2, CC_BABBLE = 3, CC_TRANSACTION = 4, CC_TRB = 5, CC_STALL = 6,
       CC_SHORT = 13, CC_STOPPED = 26, CC_STOPPED_LEN = 27 };

/* registradores operacionais */
#define OP_USBCMD   0x00
#define OP_USBSTS   0x04
#define OP_PAGESIZE 0x08
#define OP_CRCR     0x18
#define OP_DCBAAP   0x30
#define OP_CONFIG   0x38
#define CMD_RS      (1u << 0)
#define CMD_HCRST   (1u << 1)
#define STS_HCH     (1u << 0)
#define STS_HSE     (1u << 2)
#define STS_CNR     (1u << 11)

/* PORTSC */
#define PS_CCS      (1u << 0)
#define PS_PED      (1u << 1)
#define PS_PR       (1u << 4)
#define PS_PLS(v)   (((v) >> 5) & 0xF)
#define PS_PP       (1u << 9)
#define PS_SPEED(v) (((v) >> 10) & 0xF)
#define PS_CSC      (1u << 17)
#define PS_PRC      (1u << 21)
#define PS_WRC      (1u << 19)
#define PS_WPR      (1u << 31)
#define PS_CHANGE   (0x7Fu << 17)        /* CSC PEC WRC OCC PRC PLC CEC (RW1C) */
#define PS_KEEP     0x0E00C3E0u          /* PP, PLS, PIC, WCE/WDE/WOE: preservados ao escrever */

typedef struct { volatile u32 d[4]; } trb_t;
typedef struct { trb_t *t; u64 phys; u32 enq, cycle; } ring_t;

typedef struct {
    ring_t ring;
    u8   dci, addr;
    u16  mps;
    /* transferência síncrona */
    volatile bool sync, done;
    int  cc;
    u64  td[TD_MAX];
    u32  tdlen[TD_MAX];
    int  ntd;
    u32  total, actual, short_res;
    bool got_short, is_ctrl;
    u64  data_trb, status_trb;
    /* interrupt IN assíncrono */
    bool async, halted;
    usb_xfer_cb cb;
    void *ctx;
    u8  *buf;                /* ASYNC_N fatias de `stride` bytes */
    u16  len, stride;
    int  nslot;
    u64  pend[ASYNC_N];      /* TRB pendente de cada fatia (0 = livre) */
    u64  retry_at;
    int  errors;
} xep_t;

typedef struct {
    usb_dev_t *ud;
    u8   slot;
    u32  csz;
    u8  *in, *out;
    u64  in_phys, out_phys;
    xep_t *ep[32];
    u32  add_flags;
    u8   max_dci;
} xdev_t;

typedef struct {
    pci_dev_t *pci;
    volatile u8 *cap, *op, *rt, *db;
    u32  max_slots, max_ports, csz;
    u16  version;
    bool ac64, dead;
    u64 *dcbaa;
    u64  dcbaa_phys;
    ring_t cmd;
    trb_t *evt;
    u64  evt_phys;
    u32  evt_deq, evt_cycle;
    volatile bool cmd_done;
    u64  cmd_trb;
    int  cmd_cc;
    u8   cmd_slot;
    bool in_events;
    xdev_t *slots[MAX_SLOTS + 1];
    u8   port_proto[256];
    volatile u8 port_changed[256];
    u8   port_fail[256];
    usb_dev_t *port_dev[256];
    u8  *ctrl_buf;
    u64  ctrl_phys;
    u64  last_scan;
    char name[64];
} xhci_t;

static xhci_t *hcs[MAX_HC];
static int nhc;

/* ------------------------------------------------------------------ utilitários */
static inline void mb(void) { __asm__ volatile("mfence" ::: "memory"); }
static inline void cbar(void) { __asm__ volatile("" ::: "memory"); }
static inline void cpu_pause(void) { __asm__ volatile("pause"); }
static inline u32 rd(volatile u8 *b, u32 o) { return *(volatile u32 *)(b + o); }
static inline void wr(volatile u8 *b, u32 o, u32 v) { *(volatile u32 *)(b + o) = v; }
static inline void wr64(volatile u8 *b, u32 o, u64 v) { wr(b, o, (u32)v); wr(b, o + 4, (u32)(v >> 32)); }

static bool wait_bits(volatile u8 *b, u32 off, u32 mask, u32 want, u32 ms) {
    u64 end = timer_ticks() + ms;
    for (;;) {
        u32 v = rd(b, off);
        if (v == 0xFFFFFFFF) return false;
        if ((v & mask) == want) return true;
        if (timer_ticks() > end) return false;
        cpu_pause();
    }
}

static void *dalloc(xhci_t *h, usize bytes, u64 *phys) {
    void *p = dma_alloc(bytes, phys);
    if (!p) { kprintf("xHCI: sem memória DMA (%llu bytes)\n", (u64)bytes); return NULL; }
    if (!h->ac64 && *phys + bytes > 0x100000000ULL) kprintf("xHCI: aviso — DMA acima de 4 GiB sem suporte AC64\n");
    return p;
}
static void dfree(void *p, usize bytes) { if (p) pmm_free((u64)p, ALIGN_UP(bytes, 4096) / 4096); }

static bool ring_init(xhci_t *h, ring_t *r) {
    r->t = dalloc(h, RING_TRBS * sizeof(trb_t), &r->phys);
    if (!r->t) return false;
    trb_t *l = &r->t[RING_TRBS - 1];
    l->d[0] = (u32)r->phys; l->d[1] = (u32)(r->phys >> 32); l->d[2] = 0;
    l->d[3] = TRB_TYPE(TRB_LINK) | TRB_TC;     /* ciclo 0: ainda inválido para o consumidor */
    r->enq = 0; r->cycle = 1;
    return true;
}

/* enfileira um TRB; se `hold`, o bit de ciclo fica invertido (o chamador entrega depois) */
static u64 ring_push(ring_t *r, u32 d0, u32 d1, u32 d2, u32 d3, bool hold) {
    trb_t *t = &r->t[r->enq];
    t->d[0] = d0; t->d[1] = d1; t->d[2] = d2;
    cbar();
    t->d[3] = (d3 & ~1u) | (hold ? r->cycle ^ 1 : r->cycle);
    u64 p = r->phys + (u64)r->enq * sizeof(trb_t);
    if (++r->enq == RING_TRBS - 1) {
        trb_t *l = &r->t[RING_TRBS - 1];
        l->d[0] = (u32)r->phys; l->d[1] = (u32)(r->phys >> 32); l->d[2] = 0;
        cbar();
        l->d[3] = TRB_TYPE(TRB_LINK) | TRB_TC | (d3 & TRB_CH) | r->cycle;
        r->cycle ^= 1;
        r->enq = 0;
    }
    return p;
}
static void ring_give(ring_t *r, u64 phys) {
    trb_t *t = &r->t[(phys - r->phys) / sizeof(trb_t)];
    cbar();
    t->d[3] ^= 1;
}
static u64 ring_deq_ptr(ring_t *r) { return (r->phys + (u64)r->enq * sizeof(trb_t)) | r->cycle; }

static volatile u32 *portsc_reg(xhci_t *h, int port) { return (volatile u32 *)(h->op + 0x400 + 0x10 * (port - 1)); }
static void doorbell(xhci_t *h, u32 slot, u32 target) { mb(); wr(h->db, slot * 4, target); }

static u32 *in_ctx(xdev_t *x, int i) { return (u32 *)(x->in + i * x->csz); }     /* 0 = controle, 1 = slot, dci+1 */
static u32 *out_ctx(xdev_t *x, int i) { return (u32 *)(x->out + i * x->csz); }   /* 0 = slot, dci */

/* ------------------------------------------------------------------ eventos */
static void intr_queue(xhci_t *h, xdev_t *x, xep_t *ep, int k) {
    u64 b = (u64)(ep->buf + k * ep->stride);
    ep->pend[k] = ring_push(&ep->ring, (u32)b, (u32)(b >> 32), ep->len, TRB_TYPE(TRB_NORMAL) | TRB_IOC | TRB_ISP, false);
    doorbell(h, x->slot, ep->dci);
}

static void xfer_event(xhci_t *h, u32 d0, u32 d1, u32 d2, u32 d3) {
    u32 slot = d3 >> 24, epid = (d3 >> 16) & 0x1F;
    int cc = d2 >> 24;
    u32 res = d2 & 0xFFFFFF;
    u64 p = d0 | (u64)d1 << 32;
    if (!slot || slot > MAX_SLOTS || !h->slots[slot]) return;
    xdev_t *x = h->slots[slot];
    xep_t *ep = x->ep[epid];
    if (!ep || (d3 & (1u << 2))) return;   /* sem endpoint ou Event Data TRB */

    if (ep->async) {
        int k;
        for (k = 0; k < ep->nslot; k++) if (ep->pend[k] && ep->pend[k] == p) break;
        if (k == ep->nslot) return;
        ep->pend[k] = 0;
        if (cc == CC_SUCCESS || cc == CC_SHORT) {
            u32 n = ep->len - MIN(res, (u32)ep->len);
            ep->errors = 0;
            if (ep->cb) ep->cb(x->ud, ep->addr, ep->buf + k * ep->stride, n, 0, ep->ctx);
            if (ep->async && ep->cb && !ep->halted) intr_queue(h, x, ep, k);
        } else if (cc != CC_STOPPED && cc != CC_STOPPED_LEN) {
            if (ep->errors < 3) kprintf("xHCI: interrupt IN slot %u ep %02x falhou (código %d)\n", slot, ep->addr, cc);
            ep->halted = true;
            ep->retry_at = timer_ticks() + 50 + 100 * MIN(ep->errors, 10);
        }
        return;
    }
    if (!ep->sync) return;
    int i;
    for (i = 0; i < ep->ntd; i++) if (ep->td[i] == p) break;
    if (i == ep->ntd) return;
    if (ep->is_ctrl) {
        if (cc == CC_SHORT && p == ep->data_trb) { ep->short_res = res; ep->got_short = true; return; }
        if (cc == CC_SUCCESS && p != ep->status_trb) return;
        if (cc == CC_SUCCESS || cc == CC_SHORT) { cc = CC_SUCCESS; ep->actual = ep->total - (ep->got_short ? MIN(ep->short_res, ep->total) : 0); }
        ep->cc = cc;
        ep->done = true;
        return;
    }
    if (cc == CC_SHORT) {
        u32 sum = 0;
        for (int k = 0; k < i; k++) sum += ep->tdlen[k];
        ep->actual = sum + ep->tdlen[i] - MIN(res, ep->tdlen[i]);
        ep->cc = CC_SUCCESS;
        ep->done = true;
    } else if (cc == CC_SUCCESS) {
        if (i != ep->ntd - 1) return;
        ep->actual = ep->total - MIN(res, ep->total);
        ep->cc = cc;
        ep->done = true;
    } else {
        ep->cc = cc;
        ep->done = true;
    }
}

static void hc_events(xhci_t *h) {
    if (h->dead || h->in_events) return;
    h->in_events = true;
    bool any = false;
    for (int guard = 0; guard < EVT_TRBS * 2; guard++) {
        trb_t *e = &h->evt[h->evt_deq];
        u32 d3 = e->d[3];
        if ((d3 & 1) != h->evt_cycle) break;
        cbar();
        u32 d0 = e->d[0], d1 = e->d[1], d2 = e->d[2];
        u32 type = (d3 >> 10) & 0x3F;
        if (type == TRB_EV_CMD) {
            u64 p = d0 | (u64)d1 << 32;
            if (p == h->cmd_trb) { h->cmd_cc = d2 >> 24; h->cmd_slot = d3 >> 24; h->cmd_done = true; }
        } else if (type == TRB_EV_PORT) {
            u8 port = d0 >> 24;
            if (port) h->port_changed[port] = 1;
        } else if (type == TRB_EV_TRANSFER) {
            xfer_event(h, d0, d1, d2, d3);
        } else if (type == TRB_EV_HOST) {
            kprintf("xHCI: evento do controlador, código %u\n", d2 >> 24);
        }
        any = true;
        if (++h->evt_deq == EVT_TRBS) { h->evt_deq = 0; h->evt_cycle ^= 1; }
    }
    if (any) wr64(h->rt, 0x20 + 0x18, (h->evt_phys + (u64)h->evt_deq * sizeof(trb_t)) | 8);
    h->in_events = false;
}

void xhci_process_events(void) {
    for (int i = 0; i < nhc; i++) hc_events(hcs[i]);
}

/* ------------------------------------------------------------------ comandos */
static int hc_cmd(xhci_t *h, u32 d0, u32 d1, u32 d2, u32 d3, u32 timeout) {
    if (h->dead) return -1;
    h->cmd_done = false;
    h->cmd_cc = 0;
    h->cmd_trb = ring_push(&h->cmd, d0, d1, d2, d3, false);
    doorbell(h, 0, 0);
    u64 end = timer_ticks() + timeout;
    while (!h->cmd_done) {
        hc_events(h);
        if (h->cmd_done) break;
        if (timer_ticks() > end) {
            kprintf("xHCI: comando tipo %u sem resposta (timeout)\n", (d3 >> 10) & 0x3F);
            h->cmd_trb = 0;
            if (rd(h->op, OP_USBSTS) & STS_HSE) { kprintf("xHCI: erro de sistema do controlador\n"); h->dead = true; }
            return -1;
        }
        cpu_pause();
    }
    h->cmd_trb = 0;
    return h->cmd_cc;
}

static u32 ep_state(xdev_t *x, u8 dci) { return out_ctx(x, dci)[0] & 7; }

/* devolve um endpoint parado/com erro ao estado utilizável, pulando TRBs pendentes */
static void ep_recover(xhci_t *h, xdev_t *x, xep_t *ep) {
    u32 st = ep_state(x, ep->dci);
    if (st == 2) hc_cmd(h, 0, 0, 0, TRB_TYPE(TRB_RESET_EP) | ((u32)ep->dci << 16) | ((u32)x->slot << 24), 1000);
    else if (st == 1) hc_cmd(h, 0, 0, 0, TRB_TYPE(TRB_STOP_EP) | ((u32)ep->dci << 16) | ((u32)x->slot << 24), 1000);
    u64 dq = ring_deq_ptr(&ep->ring);
    st = ep_state(x, ep->dci);
    if (st == 3 || st == 4)
        hc_cmd(h, (u32)dq, (u32)(dq >> 32), 0, TRB_TYPE(TRB_SET_DEQ) | ((u32)ep->dci << 16) | ((u32)x->slot << 24), 1000);
    ep->halted = false;
}

static int ep_wait(xhci_t *h, xdev_t *x, xep_t *ep, u32 timeout) {
    u64 end = timer_ticks() + timeout;
    while (!ep->done) {
        xhci_process_events();
        if (ep->done) break;
        if (h->dead || timer_ticks() > end) {
            ep->sync = false;
            if (!h->dead) ep_recover(h, x, ep);
            return USB_TIMEOUT;
        }
        cpu_pause();
    }
    ep->sync = false;
    if (ep->cc == CC_SUCCESS) return (int)ep->actual;
    int cc = ep->cc;
    ep_recover(h, x, ep);
    return cc == CC_STALL ? USB_STALL : USB_ERR;
}

static xep_t *ep_new(xhci_t *h, u8 dci, u8 addr, u16 mps) {
    xep_t *ep = kcalloc(1, sizeof(xep_t));
    if (!ep) return NULL;
    if (!ring_init(h, &ep->ring)) { kfree(ep); return NULL; }
    ep->dci = dci; ep->addr = addr; ep->mps = mps;
    return ep;
}
static void ep_free(xep_t *ep) {
    if (!ep) return;
    dfree(ep->ring.t, RING_TRBS * sizeof(trb_t));
    if (ep->buf) dfree(ep->buf, 4096);
    kfree(ep);
}

/* ------------------------------------------------------------------ dispositivos */
static u16 default_mps0(u8 speed) {
    return speed == USB_SPEED_LOW ? 8 : speed >= USB_SPEED_SUPER ? 512 : 64;
}

int xhci_address_device(usb_dev_t *d) {
    xhci_t *h = d->hc;
    int cc = hc_cmd(h, 0, 0, 0, TRB_TYPE(TRB_ENABLE_SLOT), 2000);
    if (cc != CC_SUCCESS) { kprintf("xHCI: Enable Slot falhou (código %d)\n", cc); return USB_ERR; }
    u8 slot = h->cmd_slot;
    if (!slot || slot > MAX_SLOTS) { kprintf("xHCI: slot %u fora do limite\n", slot); return USB_ERR; }
    xdev_t *x = kcalloc(1, sizeof(xdev_t));
    if (!x) return USB_ERR;
    x->ud = d; x->slot = slot; x->csz = h->csz;
    x->in = dalloc(h, 4096, &x->in_phys);
    x->out = dalloc(h, 4096, &x->out_phys);
    d->mps0 = default_mps0(d->speed);
    x->ep[1] = ep_new(h, 1, 0, d->mps0);
    if (!x->in || !x->out || !x->ep[1]) goto fail;
    h->dcbaa[slot] = x->out_phys;
    h->slots[slot] = x;
    d->hcpriv = x;

    u32 *ic = in_ctx(x, 0), *sc = in_ctx(x, 1), *e0 = in_ctx(x, 2);
    ic[0] = 0; ic[1] = 3;                                   /* A0 (slot) + A1 (EP0) */
    sc[0] = (d->route & 0xFFFFF) | ((u32)d->speed << 20) | (1u << 27);
    sc[1] = (u32)d->root_port << 16;
    sc[2] = 0;
    if (d->tt_slot && (d->speed == USB_SPEED_LOW || d->speed == USB_SPEED_FULL))
        sc[2] = d->tt_slot | ((u32)d->tt_port << 8);
    sc[3] = 0;
    e0[0] = 0;
    e0[1] = (3u << 1) | (4u << 3) | ((u32)d->mps0 << 16);   /* CErr=3, tipo Control */
    u64 dq = ring_deq_ptr(&x->ep[1]->ring);
    e0[2] = (u32)dq; e0[3] = (u32)(dq >> 32);
    e0[4] = 8;
    x->max_dci = 1;

    cc = hc_cmd(h, (u32)x->in_phys, (u32)(x->in_phys >> 32), 0, TRB_TYPE(TRB_ADDRESS_DEV) | ((u32)slot << 24), 3000);
    if (cc != CC_SUCCESS) { kprintf("xHCI: Address Device falhou na porta %s (código %d)\n", d->path, cc); goto fail; }
    sleep_ms(2);
    return USB_OK;
fail:
    if (h->slots[slot] == x) {
        hc_cmd(h, 0, 0, 0, TRB_TYPE(TRB_DISABLE_SLOT) | ((u32)slot << 24), 1000);
        h->slots[slot] = NULL;
        h->dcbaa[slot] = 0;
    }
    ep_free(x->ep[1]);
    dfree(x->in, 4096); dfree(x->out, 4096);
    kfree(x);
    d->hcpriv = NULL;
    return USB_ERR;
}

int xhci_update_ep0(usb_dev_t *d) {
    xhci_t *h = d->hc;
    xdev_t *x = d->hcpriv;
    if (!x) return USB_NODEV;
    u32 *ic = in_ctx(x, 0), *e0 = in_ctx(x, 2);
    ic[0] = 0; ic[1] = 2;                                   /* só A1 */
    e0[1] = (e0[1] & 0xFFFF) | ((u32)d->mps0 << 16);
    x->ep[1]->mps = d->mps0;
    int cc = hc_cmd(h, (u32)x->in_phys, (u32)(x->in_phys >> 32), 0, TRB_TYPE(TRB_EVAL_CTX) | ((u32)x->slot << 24), 2000);
    return cc == CC_SUCCESS ? USB_OK : USB_ERR;
}

int xhci_control(usb_dev_t *d, u8 rt, u8 req, u16 val, u16 idx, u16 len, void *data, u32 timeout) {
    xhci_t *h = d->hc;
    xdev_t *x = d->hcpriv;
    if (!x || !x->ep[1] || h->dead) return USB_NODEV;
    if (len > 4096) return USB_ERR;
    xep_t *ep = x->ep[1];
    bool in = rt & 0x80;
    if (!in && len) memcpy(h->ctrl_buf, data, len);
    ep->ntd = 0; ep->is_ctrl = true; ep->got_short = false; ep->short_res = 0;
    ep->total = len; ep->actual = 0; ep->done = false; ep->cc = 0; ep->data_trb = 0;
    u32 trt = len ? (in ? 3 : 2) : 0;
    u64 first = ring_push(&ep->ring, rt | ((u32)req << 8) | ((u32)val << 16), idx | ((u32)len << 16), 8,
                          TRB_TYPE(TRB_SETUP) | TRB_IDT | (trt << 16), true);
    ep->td[ep->ntd++] = first;
    if (len) {
        ep->data_trb = ring_push(&ep->ring, (u32)h->ctrl_phys, (u32)(h->ctrl_phys >> 32), len,
                                 TRB_TYPE(TRB_DATA) | TRB_ISP | (in ? TRB_DIR_IN : 0), false);
        ep->td[ep->ntd++] = ep->data_trb;
    }
    ep->status_trb = ring_push(&ep->ring, 0, 0, 0, TRB_TYPE(TRB_STATUS) | TRB_IOC | ((len && in) ? 0 : TRB_DIR_IN), false);
    ep->td[ep->ntd++] = ep->status_trb;
    ep->sync = true;
    ring_give(&ep->ring, first);
    doorbell(h, x->slot, 1);
    int r = ep_wait(h, x, ep, timeout);
    if (r > 0 && in) memcpy(data, h->ctrl_buf, r);
    return r;
}

static u8 ep_interval(usb_dev_t *d, const usb_ep_desc_t *e, u8 type) {
    u8 bi = e->bInterval;
    if (type == USB_EP_BULK || type == USB_EP_CONTROL) return 0;
    if (d->speed >= USB_SPEED_HIGH || type == USB_EP_ISOC) {
        int v = (bi ? bi : 1) - 1;
        if (d->speed < USB_SPEED_HIGH) v += 3;         /* isócrono FS: 2^(bInterval-1) frames */
        return CLAMP(v, 0, 15);
    }
    /* interrupt FS/LS: bInterval em ms → 2^n microframes */
    int n = 0;
    u32 us = (bi ? bi : 1) * 8;
    while ((1u << (n + 1)) <= us) n++;
    return CLAMP(n, 3, 10);
}

int xhci_add_endpoint(usb_dev_t *d, const usb_ep_desc_t *e, u8 burst) {
    xhci_t *h = d->hc;
    xdev_t *x = d->hcpriv;
    if (!x) return USB_NODEV;
    u8 num = e->bEndpointAddress & 0xF;
    bool in = e->bEndpointAddress & 0x80;
    u8 dci = num * 2 + (in ? 1 : 0);
    if (!num || dci > 31) return USB_ERR;
    u8 type = e->bmAttributes & 3;
    u32 xt = type == USB_EP_ISOC ? (in ? 5 : 1) : type == USB_EP_BULK ? (in ? 6 : 2) : type == USB_EP_INTR ? (in ? 7 : 3) : 4;
    u16 mps = e->wMaxPacketSize & 0x7FF;
    if (d->speed == USB_SPEED_HIGH && (type == USB_EP_INTR || type == USB_EP_ISOC)) burst = (e->wMaxPacketSize >> 11) & 3;
    if (d->speed < USB_SPEED_SUPER && type == USB_EP_BULK) burst = 0;
    if (!x->ep[dci]) x->ep[dci] = ep_new(h, dci, e->bEndpointAddress, mps);
    xep_t *ep = x->ep[dci];
    if (!ep) return USB_ERR;
    ep->mps = mps;
    u32 *c = in_ctx(x, dci + 1);
    memset(c, 0, x->csz);
    u32 esit = (type == USB_EP_INTR || type == USB_EP_ISOC) ? (u32)mps * (burst + 1) : 0;
    c[0] = ((u32)ep_interval(d, e, type) << 16) | ((esit >> 16) << 24);
    c[1] = ((type == USB_EP_ISOC ? 0u : 3u) << 1) | (xt << 3) | ((u32)burst << 8) | ((u32)mps << 16);
    u64 dq = ring_deq_ptr(&ep->ring);
    c[2] = (u32)dq; c[3] = (u32)(dq >> 32);
    u32 avg = type == USB_EP_INTR ? mps : type == USB_EP_BULK ? 3072 : mps;
    c[4] = avg | ((esit & 0xFFFF) << 16);
    x->add_flags |= 1u << dci;
    if (dci > x->max_dci) x->max_dci = dci;
    return USB_OK;
}

static int configure(xhci_t *h, xdev_t *x, u32 add) {
    u32 *ic = in_ctx(x, 0), *sc = in_ctx(x, 1);
    ic[0] = 0;
    ic[1] = add | 1;
    sc[0] = (sc[0] & ~(0x1Fu << 27)) | ((u32)x->max_dci << 27);
    int cc = hc_cmd(h, (u32)x->in_phys, (u32)(x->in_phys >> 32), 0, TRB_TYPE(TRB_CONFIG_EP) | ((u32)x->slot << 24), 3000);
    if (cc != CC_SUCCESS) kprintf("xHCI: Configure Endpoint falhou no slot %u (código %d)\n", x->slot, cc);
    return cc == CC_SUCCESS ? USB_OK : USB_ERR;
}

int xhci_commit_endpoints(usb_dev_t *d) {
    xdev_t *x = d->hcpriv;
    if (!x) return USB_NODEV;
    if (!x->add_flags) return USB_OK;
    int r = configure(d->hc, x, x->add_flags);
    x->add_flags = 0;
    return r;
}

int xhci_set_hub(usb_dev_t *d, int nports, bool mtt, int ttt) {
    xdev_t *x = d->hcpriv;
    if (!x) return USB_NODEV;
    u32 *sc = in_ctx(x, 1);
    sc[0] |= 1u << 26;
    if (mtt) sc[0] |= 1u << 25;
    sc[1] = (sc[1] & 0x00FFFFFF) | ((u32)(nports & 0xFF) << 24);
    if (d->speed == USB_SPEED_HIGH) sc[2] = (sc[2] & ~(3u << 16)) | ((u32)(ttt & 3) << 16);
    return configure(d->hc, x, 0);
}

static xep_t *find_ep(xdev_t *x, u8 addr) {
    u8 num = addr & 0xF, dci = num * 2 + ((addr & 0x80) ? 1 : 0);
    return (num && dci < 32) ? x->ep[dci] : NULL;
}

int xhci_bulk(usb_dev_t *d, u8 addr, void *buf, u32 len, u32 timeout) {
    xhci_t *h = d->hc;
    xdev_t *x = d->hcpriv;
    if (!x || h->dead) return USB_NODEV;
    xep_t *ep = find_ep(x, addr);
    if (!ep || ep->async) return USB_ERR;
    if (ep->halted) ep_recover(h, x, ep);
    u64 p = (u64)buf;
    /* divide em TRBs que não cruzam fronteiras de 64 KiB */
    u32 pieces[TD_MAX];
    u64 addrs[TD_MAX];
    int n = 0;
    u32 left = len;
    do {
        u32 room = 0x10000 - (u32)(p & 0xFFFF);
        u32 c = MIN(left, room);
        if (n == TD_MAX) return USB_ERR;
        addrs[n] = p; pieces[n] = c; n++;
        p += c; left -= c;
    } while (left);
    ep->ntd = 0; ep->is_ctrl = false; ep->total = len; ep->actual = 0; ep->done = false; ep->cc = 0;
    u64 first = 0;
    u32 sent = 0;
    for (int i = 0; i < n; i++) {
        sent += pieces[i];
        u32 rem = len - sent;
        u32 tds = ep->mps ? MIN((rem + ep->mps - 1) / ep->mps, 31u) : 0;
        bool last = i == n - 1;
        u32 d3 = TRB_TYPE(TRB_NORMAL) | TRB_ISP | (last ? TRB_IOC : TRB_CH);
        u64 t = ring_push(&ep->ring, (u32)addrs[i], (u32)(addrs[i] >> 32), pieces[i] | ((last ? 0 : tds) << 17), d3, i == 0);
        if (!i) first = t;
        ep->td[ep->ntd] = t;
        ep->tdlen[ep->ntd++] = pieces[i];
    }
    ep->sync = true;
    ring_give(&ep->ring, first);
    doorbell(h, x->slot, ep->dci);
    return ep_wait(h, x, ep, timeout);
}

int xhci_intr_start(usb_dev_t *d, u8 addr, u16 len, usb_xfer_cb cb, void *ctx) {
    xhci_t *h = d->hc;
    xdev_t *x = d->hcpriv;
    if (!x) return USB_NODEV;
    xep_t *ep = find_ep(x, addr);
    if (!ep || !(addr & 0x80)) return USB_ERR;
    if (!len) return USB_ERR;
    if (len > 4096) len = 4096;
    if (!ep->buf) { u64 ph; ep->buf = dalloc(h, 4096, &ph); if (!ep->buf) return USB_ERR; }
    ep->len = len; ep->cb = cb; ep->ctx = ctx; ep->async = true; ep->halted = false; ep->errors = 0;
    ep->stride = (u16)ALIGN_UP(len, 64);
    ep->nslot = MIN(ASYNC_N, 4096 / ep->stride);
    /* várias transferências na fila: o controlador continua amostrando o dispositivo mesmo se o
     * laço principal demorar (senão pressionar+soltar rápidos se fundem num relatório só) */
    for (int k = 0; k < ep->nslot; k++) intr_queue(h, x, ep, k);
    return USB_OK;
}

int xhci_reset_endpoint(usb_dev_t *d, u8 addr) {
    xdev_t *x = d->hcpriv;
    if (!x) return USB_NODEV;
    xep_t *ep = find_ep(x, addr);
    if (!ep) return USB_ERR;
    u32 st = ep_state(x, ep->dci);
    if (st == 2 || st == 4) ep_recover(d->hc, x, ep);
    return USB_OK;
}

void xhci_free_device(usb_dev_t *d) {
    xhci_t *h = d->hc;
    xdev_t *x = d->hcpriv;
    if (!x) return;
    for (int i = 0; i < 32; i++) if (x->ep[i]) { x->ep[i]->cb = NULL; x->ep[i]->async = false; x->ep[i]->sync = false; }
    if (!h->dead) hc_cmd(h, 0, 0, 0, TRB_TYPE(TRB_DISABLE_SLOT) | ((u32)x->slot << 24), 1000);
    h->slots[x->slot] = NULL;
    h->dcbaa[x->slot] = 0;
    for (int i = 0; i < 32; i++) ep_free(x->ep[i]);
    dfree(x->in, 4096); dfree(x->out, 4096);
    kfree(x);
    d->hcpriv = NULL;
}

/* ------------------------------------------------------------------ portas raiz */
static bool port_reset(xhci_t *h, int p, bool usb3) {
    volatile u32 *ps = portsc_reg(h, p);
    u32 v = *ps;
    u32 pls = PS_PLS(v);
    bool warm = usb3 && (pls == 6 || pls == 10);    /* SS.Inactive / Compliance → warm reset */
    *ps = (v & PS_KEEP) | (warm ? PS_WPR : PS_PR);
    u64 end = timer_ticks() + 500;
    u32 done = warm ? (PS_WRC | PS_PRC) : PS_PRC;
    while (!(*ps & done) && timer_ticks() < end) sleep_ms(1);
    v = *ps;
    *ps = (v & PS_KEEP) | (v & PS_CHANGE & ~PS_CSC);
    if (!(v & done)) { kprintf("xHCI: reset da porta %d expirou (PORTSC %08x)\n", p, v); return false; }
    sleep_ms(usb3 ? 2 : 10);                        /* recuperação após reset */
    return (*ps & (PS_PED | PS_CCS)) == (PS_PED | PS_CCS);
}

static const char *speed_name(u8 s) {
    switch (s) { case USB_SPEED_LOW: return "1.5 Mb/s"; case USB_SPEED_FULL: return "12 Mb/s"; case USB_SPEED_HIGH: return "480 Mb/s";
    case USB_SPEED_SUPER: return "5 Gb/s"; case USB_SPEED_SUPERPLUS: return "10 Gb/s"; default: return "?"; }
}

static void port_attach(xhci_t *h, int p) {
    volatile u32 *ps = portsc_reg(h, p);
    u32 v = *ps;
    if (!(v & PS_CCS)) return;
    bool usb3 = h->port_proto[p] == 3;
    if (!usb3 || !(v & PS_PED)) {
        if (usb3 && PS_PLS(v) == 7) {               /* Polling: treinamento do link ainda em curso */
            u64 end = timer_ticks() + 300;
            while (!(*ps & PS_PED) && timer_ticks() < end) sleep_ms(5);
        }
        /* USB 2: sempre reseta (habilita a porta); USB 3: só se o link não subiu sozinho */
        if ((!usb3 || !(*ps & PS_PED)) && !port_reset(h, p, usb3)) {
            kprintf("xHCI: porta %d: dispositivo não habilitou (PORTSC %08x)\n", p, *ps);
            h->port_fail[p]++;
            return;
        }
    }
    v = *ps;
    u8 speed = PS_SPEED(v);
    kprintf("xHCI: porta %d (USB %d): dispositivo conectado, %s\n", p, usb3 ? 3 : 2, speed_name(speed));
    usb_dev_t *d = usb_attach(h, NULL, p, speed);
    h->port_dev[p] = d;
    if (!d) h->port_fail[p]++;
}

static void port_check(xhci_t *h, int p) {
    volatile u32 *ps = portsc_reg(h, p);
    u32 v = *ps;
    if (v == 0xFFFFFFFF) return;
    *ps = (v & PS_KEEP) | (v & PS_CHANGE);           /* confirma as mudanças */
    bool conn = v & PS_CCS;
    if (h->port_dev[p] && (!conn || (v & PS_CSC))) {
        kprintf("xHCI: porta %d: dispositivo removido\n", p);
        usb_detach(h->port_dev[p]);
        h->port_dev[p] = NULL;
    }
    if (!conn) { h->port_fail[p] = 0; return; }
    if (!h->port_dev[p] && h->port_fail[p] < 3) {
        if (v & PS_CSC) sleep_ms(100);              /* debounce de conexão (USB 2.0 §7.1.7.3) */
        port_attach(h, p);
    }
}

void xhci_poll(void) {
    u64 now = timer_ticks();
    for (int i = 0; i < nhc; i++) {
        xhci_t *h = hcs[i];
        if (h->dead) continue;
        hc_events(h);
        for (u32 p = 1; p <= h->max_ports; p++)
            if (h->port_changed[p]) { h->port_changed[p] = 0; port_check(h, p); }
        /* rede de segurança: varre mudanças de porta perdidas a cada segundo */
        if (now - h->last_scan >= 1000) {
            h->last_scan = now;
            if (rd(h->op, OP_USBSTS) == 0xFFFFFFFF) { kprintf("xHCI: controlador sumiu\n"); h->dead = true; continue; }
            for (u32 p = 1; p <= h->max_ports; p++) {
                u32 v = *portsc_reg(h, p);
                if ((v & PS_CSC) || ((v & PS_CCS) && !h->port_dev[p] && h->port_fail[p] < 3)) port_check(h, p);
            }
        }
        /* recupera interrupt INs com erro (ex.: erro de transação momentâneo) */
        for (int s = 1; s <= MAX_SLOTS; s++) {
            xdev_t *x = h->slots[s];
            if (!x) continue;
            for (int e = 2; e < 32; e++) {
                xep_t *ep = x->ep[e];
                if (!ep || !ep->async || !ep->halted || now < ep->retry_at) continue;
                if (++ep->errors > 20) { ep->async = false; kprintf("xHCI: desistindo do endpoint %02x (slot %d)\n", ep->addr, s); continue; }
                ep_recover(h, x, ep);          /* pula todos os TRBs pendentes */
                memset(ep->pend, 0, sizeof ep->pend);
                if (ep->async && ep->cb)
                    for (int k = 0; k < ep->nslot; k++) intr_queue(h, x, ep, k);
            }
        }
    }
}

/* ------------------------------------------------------------------ inicialização */
static void bios_handoff(xhci_t *h, u32 off) {
    u32 v = rd(h->cap, off);
    if (v & (1u << 16)) {
        wr(h->cap, off, v | (1u << 24));
        if (!wait_bits(h->cap, off, 1u << 16, 0, 1000)) {
            kprintf("xHCI: firmware não liberou o controlador; assumindo à força\n");
            wr(h->cap, off, (rd(h->cap, off) & ~(1u << 16)) | (1u << 24));
        } else kprintf("xHCI: controle obtido do firmware (USBLEGSUP)\n");
    } else wr(h->cap, off, v | (1u << 24));
    u32 c = rd(h->cap, off + 4);
    c &= ~((1u << 0) | (1u << 4) | (7u << 13));      /* desliga SMIs */
    c |= 7u << 29;                                   /* limpa status RW1C */
    wr(h->cap, off + 4, c);
}

static void intel_route_ports(xhci_t *h) {
    static const u16 ids[] = {0x1E31, 0x8C31, 0x9C31, 0x9CB1, 0x8CB1, 0x8D31};
    pci_dev_t *pd = h->pci;
    if (pd->vendor != 0x8086) return;
    for (u32 i = 0; i < ARRAY_LEN(ids); i++) if (pd->device == ids[i]) {
        pci_write(pd, 0xD8, pci_read(pd, 0xDC));     /* USB3_PSSEN ← USB3PRM */
        pci_write(pd, 0xD0, pci_read(pd, 0xD4));     /* XUSB2PR ← XUSB2PRM */
        kprintf("xHCI: portas Intel roteadas do EHCI para o xHCI\n");
        return;
    }
}

static void hc_init(pci_dev_t *pd) {
    if (nhc >= MAX_HC) return;
    u64 bar = pci_bar_addr(pd, 0);
    if (!bar || (pd->bar[0] & 1)) { kprintf("xHCI: BAR0 inválido\n"); return; }
    vmm_map_mmio(bar, 0x100000);
    u32 cmdreg = pci_read(pd, 0x04);
    pci_write(pd, 0x04, (cmdreg & 0xFFFF) | 0x06 | (1u << 10));   /* memória + bus master, sem INTx */

    xhci_t *h = kcalloc(1, sizeof(xhci_t));
    if (!h) return;
    h->pci = pd;
    h->cap = (volatile u8 *)bar;
    u32 cap0 = rd(h->cap, 0);
    if (cap0 == 0xFFFFFFFF) { kprintf("xHCI: MMIO não responde\n"); kfree(h); return; }
    h->version = cap0 >> 16;
    h->op = h->cap + (cap0 & 0xFF);
    h->rt = h->cap + (rd(h->cap, 0x18) & ~0x1Fu);
    h->db = h->cap + (rd(h->cap, 0x14) & ~0x3u);
    u32 hcs1 = rd(h->cap, 0x04), hcs2 = rd(h->cap, 0x08), hcc1 = rd(h->cap, 0x10);
    h->max_slots = hcs1 & 0xFF;
    h->max_ports = hcs1 >> 24;
    h->ac64 = hcc1 & 1;
    h->csz = (hcc1 & 4) ? 64 : 32;
    u32 nscratch = (((hcs2 >> 21) & 0x1F) << 5) | ((hcs2 >> 27) & 0x1F);
    snprintf(h->name, sizeof h->name, "xHCI %x.%02x (%04x:%04x)", h->version >> 8, h->version & 0xFF, pd->vendor, pd->device);
    kprintf("%s em %02x:%02x.%x: %u slots, %u portas, contexto %u B, %u scratchpads, MMIO 0x%llx\n", h->name,
            pd->bus, pd->dev, pd->fn, h->max_slots, h->max_ports, h->csz, nscratch, bar);

    /* capacidades estendidas: handoff do BIOS e protocolo de cada porta */
    u32 off = (hcc1 >> 16) << 2;
    for (int guard = 0; off && guard < 64; guard++) {
        u32 v = rd(h->cap, off);
        u8 id = v & 0xFF, next = (v >> 8) & 0xFF;
        if (id == 1) bios_handoff(h, off);
        else if (id == 2) {
            u8 major = v >> 24;
            u32 d2 = rd(h->cap, off + 8);
            u32 po = d2 & 0xFF, pc = (d2 >> 8) & 0xFF;
            for (u32 i = po; i < po + pc && i < 256; i++) h->port_proto[i] = major;
            kprintf("xHCI: portas %u-%u: USB %u.%u\n", po, po + pc - 1, major, (v >> 16) & 0xFF);
        }
        if (!next) break;
        off += (u32)next << 2;
    }
    intel_route_ports(h);

    /* para e reseta */
    if (!wait_bits(h->op, OP_USBSTS, STS_CNR, 0, 1000)) { kprintf("xHCI: controlador não ficou pronto\n"); goto fail; }
    wr(h->op, OP_USBCMD, rd(h->op, OP_USBCMD) & ~CMD_RS);
    if (!wait_bits(h->op, OP_USBSTS, STS_HCH, STS_HCH, 500)) kprintf("xHCI: controlador não parou\n");
    wr(h->op, OP_USBCMD, CMD_HCRST);
    sleep_ms(10);
    if (!wait_bits(h->op, OP_USBCMD, CMD_HCRST, 0, 1000) || !wait_bits(h->op, OP_USBSTS, STS_CNR, 0, 1000)) {
        kprintf("xHCI: reset do controlador expirou\n");
        goto fail;
    }
    if (!(rd(h->op, OP_PAGESIZE) & 1)) kprintf("xHCI: aviso — páginas de 4 KiB não suportadas\n");

    u32 nslots = MIN(h->max_slots, (u32)MAX_SLOTS);
    wr(h->op, OP_CONFIG, (rd(h->op, OP_CONFIG) & ~0xFFu) | nslots);
    h->dcbaa = dalloc(h, 4096, &h->dcbaa_phys);
    if (!h->dcbaa) goto fail;
    if (nscratch) {
        u64 ap;
        u64 *arr = dalloc(h, nscratch * 8, &ap);
        if (!arr) goto fail;
        for (u32 i = 0; i < nscratch; i++) {
            u64 pp;
            if (!dalloc(h, 4096, &pp)) goto fail;
            arr[i] = pp;
        }
        h->dcbaa[0] = ap;
    }
    wr64(h->op, OP_DCBAAP, h->dcbaa_phys);
    if (!ring_init(h, &h->cmd)) goto fail;
    wr64(h->op, OP_CRCR, h->cmd.phys | 1);

    u64 erst_phys;
    u64 *erst = dalloc(h, 64, &erst_phys);
    h->evt = dalloc(h, EVT_TRBS * sizeof(trb_t), &h->evt_phys);
    if (!erst || !h->evt) goto fail;
    erst[0] = h->evt_phys;
    erst[1] = EVT_TRBS;
    h->evt_deq = 0; h->evt_cycle = 1;
    volatile u8 *ir = h->rt + 0x20;
    wr(ir, 0x08, 1);                                 /* ERSTSZ */
    wr64(ir, 0x18, h->evt_phys);                     /* ERDP */
    wr64(ir, 0x10, erst_phys);                       /* ERSTBA */
    wr(ir, 0x04, 0);                                 /* IMOD */
    wr(ir, 0x00, 1);                                 /* IMAN: limpa IP, IE desligado (polling) */
    h->ctrl_buf = dalloc(h, 4096, &h->ctrl_phys);
    if (!h->ctrl_buf) goto fail;

    wr(h->op, OP_USBCMD, CMD_RS);
    if (!wait_bits(h->op, OP_USBSTS, STS_HCH, 0, 500)) { kprintf("xHCI: controlador não iniciou\n"); goto fail; }
    hcs[nhc++] = h;

    /* energiza as portas (se houver controle de energia) e espera o link USB 3 treinar */
    bool powered = false;
    for (u32 p = 1; p <= h->max_ports; p++) {
        volatile u32 *ps = portsc_reg(h, p);
        u32 v = *ps;
        if (!(v & PS_PP)) { *ps = (v & PS_KEEP) | PS_PP; powered = true; }
    }
    sleep_ms(powered ? 150 : 100);
    hc_events(h);
    int found = 0;
    for (u32 p = 1; p <= h->max_ports; p++) {
        h->port_changed[p] = 0;
        volatile u32 *ps = portsc_reg(h, p);
        u32 v = *ps;
        *ps = (v & PS_KEEP) | (v & PS_CHANGE);
        if (v & PS_CCS) { found++; port_attach(h, p); }
    }
    h->last_scan = timer_ticks();
    kprintf("xHCI: %d porta(s) com dispositivo na inicialização\n", found);
    return;
fail:
    kprintf("xHCI: inicialização abortada (%04x:%04x)\n", pd->vendor, pd->device);
    kfree(h);
}

int xhci_init_all(void) {
    for (int i = 0; i < pci_count(); i++) {
        pci_dev_t *d = pci_get(i);
        if (d->class_ == 0x0C && d->subclass == 3 && d->progif == 0x30) hc_init(d);
    }
    return nhc;
}

int xhci_slot_of(usb_dev_t *d) { return d && d->hcpriv ? ((xdev_t *)d->hcpriv)->slot : 0; }
int xhci_count(void) { return nhc; }
int xhci_total_ports(void) {
    int n = 0;
    for (int i = 0; i < nhc; i++) n += hcs[i]->max_ports;
    return n;
}
const char *xhci_name(void *hc) { return hc ? ((xhci_t *)hc)->name : "?"; }
