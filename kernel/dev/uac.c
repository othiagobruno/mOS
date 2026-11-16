/*
 * mOS — microfones e headsets USB (USB Audio Class 1.0 e 2.0), entrada.
 *
 * A interface AudioStreaming (classe 1, subclasse 2) com endpoint isócrono IN é reivindicada aqui; a AudioControl dá a
 * versão e o relógio (UAC2). No start: faixas de taxa do relógio (UAC2, GET RANGE), a melhor alternate setting
 * (48 kHz, 16 bits, até 2 canais), a taxa (UAC1: no endpoint; UAC2: no relógio) e o isócrono. Cada pacote vira
 * 48 kHz estéreo 16 bits (uac_core.c) e entra no anel de captura do sound.c, que passa a ter esta como entrada.
 * Saída (alto-falante USB) ainda não. Código original do mOS (MIT).
 */
#include "usb.h"
#include "uac_core.h"

#define MAX_UAC 2
#define OUT_FRAMES 4096
typedef struct {
    bool used, streaming;
    usb_dev_t *d;
    usb_iface_t *it;
    u16 ver;
    u8 clock, ac_num, ep;
    uac_alt_info alts[USB_MAX_ALTS];
    int nalts, cur;
    u32 rate;
    uac_rs rs;
    i16 *out;
} uac_t;
static uac_t uacs[MAX_UAC];
void snd_capture_ext(const i16 *src, u32 frames);
void audio_input_attach(const char *name);
void audio_input_detach(void);

static const u8 *alt_extra(usb_iface_t *it, int k, u16 *len) {
    if (k == 0 && !it->alts[0].extra) { *len = it->extra_len; return it->extra; }
    *len = it->alts[k].extra_len;
    return it->alts[k].extra;
}
static int iso_in(usb_alt_t *a) {
    for (int e = 0; e < a->nep; e++) if ((a->ep[e].bmAttributes & 3) == USB_EP_ISOC && (a->ep[e].bEndpointAddress & 0x80)) return e;
    return -1;
}

static bool uac_probe(usb_dev_t *d, usb_iface_t *it) {
    if (it->cls != 1 || it->sub != 2) return false;
    uac_t *u = NULL;
    for (int i = 0; i < MAX_UAC && !u; i++) if (!uacs[i].used) u = &uacs[i];
    if (!u) return false;
    memset(u, 0, sizeof *u);
    for (int i = 0; i < d->nifaces; i++) {
        usb_iface_t *ac = &d->iface[i];
        if (ac->cls != 1 || ac->sub != 1) continue;
        u16 n = 0;
        const u8 *x = alt_extra(ac, 0, &n);
        if (x) u->ver = uac_parse_ac(x, n, &u->clock);
        u->ac_num = ac->num;
        break;
    }
    if (!u->ver) u->ver = 0x0100;
    /* só entrada: as alternates com isócrono IN e PCM */
    for (int k = 0; k < it->nalt && u->nalts < USB_MAX_ALTS; k++) {
        usb_alt_t *a = &it->alts[k];
        if (!a->alt || iso_in(a) < 0) continue;
        u16 n = 0;
        const u8 *x = alt_extra(it, k, &n);
        uac_alt_info info;
        memset(&info, 0, sizeof info);
        if (!x || uac_parse_as(x, n, u->ver, &info)) continue;
        info.alt = a->alt;
        u->alts[u->nalts++] = info;
    }
    if (!u->nalts) return false;
    u->used = true; u->d = d; u->it = it;
    it->priv = u;
    kprintf("UAC: [%s] %s, UAC %x.%02x, %d formato(s) de entrada\n", d->path, d->product[0] ? d->product : "áudio USB", u->ver >> 8, u->ver & 0xFF, u->nalts);
    return true;
}

static void iso_cb(usb_dev_t *d, u8 ep, u8 *buf, int npkts, u32 *lens, int *status, void *ctx) {
    (void)d;
    uac_t *u = ctx;
    if (!u->streaming || !u->out) return;
    u32 esit = usb_iso_esit_max(u->d, ep);
    uac_alt_info *a = &u->alts[u->cur];
    for (int i = 0; i < npkts; i++) {
        if (status[i] != 0 || !lens[i]) continue;
        u32 n = uac_convert(&u->rs, buf + (u64)i * esit, lens[i], a->channels, a->subslot, u->out, OUT_FRAMES);
        if (n) snd_capture_ext(u->out, n);
    }
}

static void uac_start(usb_dev_t *d, usb_iface_t *it) {
    uac_t *u = it->priv;
    if (!u) return;
    /* UAC2: as taxas vêm do relógio, iguais para todas as alternates */
    if (u->ver >= 0x0200 && u->clock) {
        static u8 rg[2 + 12 * 16];
        int r = usb_control(d, 0xA1, 0x02 /* RANGE */, 0x0100 /* CS_SAM_FREQ_CONTROL */, (u16)(u->clock << 8 | u->ac_num), sizeof rg, rg);
        uac_alt_info tmp;
        memset(&tmp, 0, sizeof tmp);
        if (r >= 2) { int cont = 0; tmp.nrates = (u8)uac_parse_ranges(rg, (u32)r, tmp.rates, UAC_MAX_RATES, &cont); tmp.continuous = cont; }
        for (int k = 0; k < u->nalts; k++) { memcpy(u->alts[k].rates, tmp.rates, sizeof tmp.rates); u->alts[k].nrates = tmp.nrates; u->alts[k].continuous = tmp.continuous; }
    }
    int best = 0, bs = -1;
    for (int k = 0; k < u->nalts; k++) { int s = uac_score(&u->alts[k], uac_pick_rate(&u->alts[k])); if (s > bs) { bs = s; best = k; } }
    u->cur = best;
    uac_alt_info *a = &u->alts[best];
    u->rate = uac_pick_rate(a);
    if (usb_set_interface(d, it, a->alt) != USB_OK) { kprintf("UAC: [%s] alternate %u recusada\n", d->path, a->alt); return; }
    int idx;
    if (!usb_find_ep(it, USB_EP_ISOC, true, &idx)) return;
    u->ep = it->ep[idx].bEndpointAddress;
    if (u->ver >= 0x0200) {
        u8 f[4] = {(u8)u->rate, (u8)(u->rate >> 8), (u8)(u->rate >> 16), (u8)(u->rate >> 24)};
        if (u->clock) usb_control(d, 0x21, 0x01 /* CUR */, 0x0100, (u16)(u->clock << 8 | u->ac_num), 4, f);
    } else {
        u8 f[3] = {(u8)u->rate, (u8)(u->rate >> 8), (u8)(u->rate >> 16)};
        usb_control(d, 0x22, 0x01 /* SET_CUR */, 0x0100 /* SAMPLING_FREQ_CONTROL */, u->ep, 3, f);   /* taxa única: pode dar STALL */
    }
    u->out = kmalloc(OUT_FRAMES * 4);
    if (!u->out) return;
    uac_rs_init(&u->rs, u->rate);
    u->streaming = true;
    if (usb_iso_start(d, u->ep, 0, 8, 8, iso_cb, u) != USB_OK) { u->streaming = false; kfree(u->out); u->out = NULL; return; }
    audio_input_attach(d->product[0] ? d->product : "Microfone USB");
    kprintf("UAC: [%s] %u Hz, %u canal(is), %u bits\n", d->path, u->rate, a->channels, a->bits);
}

static void uac_detach(usb_dev_t *d, usb_iface_t *it) {
    (void)d;
    uac_t *u = it->priv;
    if (!u) return;
    if (u->streaming) { u->streaming = false; audio_input_detach(); }
    kfree(u->out);
    u->out = NULL;
    u->used = false;
    it->priv = NULL;
}

const usb_driver_t usb_uac_driver = { "uac", uac_probe, uac_start, uac_detach, NULL };
