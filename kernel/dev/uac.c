/*
 * mOS — microfones e headsets USB (USB Audio Class 1.0 e 2.0), entrada.
 *
 * A interface AudioStreaming (classe 1, subclasse 2) com endpoint isócrono IN é reivindicada aqui; a AudioControl dá a
 * versão e o relógio (UAC2). No start: faixas de taxa do relógio (UAC2, GET RANGE), a melhor alternate setting
 * (48 kHz, 16 bits, até 2 canais), a taxa (UAC1: no endpoint; UAC2: no relógio) e o isócrono. Cada pacote vira
 * 48 kHz estéreo 16 bits (uac_core.c) e entra no anel de captura do sound.c, que passa a ter esta como entrada.
 * Saída (alto-falante/headset): alternates com isócrono OUT a 48 kHz; a mistura final do sound.c (audio_usb_sink) vai
 * para um anel de ~170 ms e cada intervalo de serviço leva taxa x duração quadros (sem o endpoint de feedback: o
 * relógio do mixer e o do dispositivo podem escorregar; o anel absorve e, se esvaziar, sai silêncio).
 * Código original do mOS (MIT).
 */
#include "usb.h"
#include "uac_core.h"

#define MAX_UAC 4   /* um headset ocupa dois: entrada e saída */
#define OUT_FRAMES 4096
typedef struct {
    bool used, streaming, out_dir;
    u64 acc;
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
extern void (*audio_usb_sink)(const i16 *stereo, u32 frames);
/* anel da saída: escrito pelo mixer, lido pelo isócrono */
#define RING_N 8192u
static i16 ring[RING_N * 2];
static u32 ring_w, ring_r;
static spinlock_t ring_lock;
static uac_t *sink_dev;
static void usb_sink(const i16 *s, u32 n) {
    u64 fl = spin_lock_irqsave(&ring_lock);
    for (u32 i = 0; i < n; i++) {
        ring[(ring_w & (RING_N - 1)) * 2] = s[i * 2];
        ring[(ring_w & (RING_N - 1)) * 2 + 1] = s[i * 2 + 1];
        ring_w++;
        if (ring_w - ring_r > RING_N) ring_r = ring_w - RING_N;   /* cheio: perde o mais antigo */
    }
    spin_unlock_irqrestore(&ring_lock, fl);
}
static u32 ring_take(i16 *dst, u32 n) {
    u64 fl = spin_lock_irqsave(&ring_lock);
    u32 avail = ring_w - ring_r, k = n < avail ? n : avail;
    for (u32 i = 0; i < k; i++) { dst[i * 2] = ring[((ring_r + i) & (RING_N - 1)) * 2]; dst[i * 2 + 1] = ring[((ring_r + i) & (RING_N - 1)) * 2 + 1]; }
    ring_r += k;
    spin_unlock_irqrestore(&ring_lock, fl);
    for (u32 i = k; i < n; i++) dst[i * 2] = dst[i * 2 + 1] = 0;
    return k;
}
void snd_capture_ext(const i16 *src, u32 frames);
void audio_input_attach(const char *name);
void audio_input_detach(void);

static const u8 *alt_extra(usb_iface_t *it, int k, u16 *len) {
    if (k == 0 && !it->alts[0].extra) { *len = it->extra_len; return it->extra; }
    *len = it->alts[k].extra_len;
    return it->alts[k].extra;
}
static int iso_dir(usb_alt_t *a, bool in) {
    for (int e = 0; e < a->nep; e++)
        if ((a->ep[e].bmAttributes & 3) == USB_EP_ISOC && !!(a->ep[e].bEndpointAddress & 0x80) == in && ((a->ep[e].bmAttributes >> 4) & 3) != 1 /* não é feedback */) return e;
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
    /* entrada (isócrono IN) ou saída (isócrono OUT); PCM */
    for (int k = 0; k < it->nalt && u->nalts < USB_MAX_ALTS; k++) {
        usb_alt_t *a = &it->alts[k];
        if (!a->alt) continue;
        bool in = iso_dir(a, true) >= 0, out = iso_dir(a, false) >= 0;
        if (!in && !out) continue;
        if (u->nalts && u->out_dir != (!in)) continue;
        u->out_dir = !in;
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
    kprintf("UAC: [%s] %s, UAC %x.%02x, %d formato(s) de %s\n", d->path, d->product[0] ? d->product : "áudio USB", u->ver >> 8, u->ver & 0xFF,
            u->nalts, u->out_dir ? "saída" : "entrada");
    return true;
}

static void iso_cb(usb_dev_t *d, u8 ep, u8 *buf, int npkts, u32 *lens, int *status, void *ctx) {
    (void)d;
    uac_t *u = ctx;
    if (!u->streaming || !u->out) return;
    u32 esit = usb_iso_esit_max(u->d, ep);
    uac_alt_info *a = &u->alts[u->cur];
    if (u->out_dir) {   /* preencher: taxa x duração do intervalo, no formato do dispositivo */
        u32 us = usb_iso_interval_us(u->d, ep), fb = (u32)(a->channels * a->subslot);
        for (int i = 0; i < npkts; i++) {
            u32 n = uac_frames_for(u->rate, us ? us : 1000, &u->acc);
            if (fb && n * fb > esit) n = esit / fb;
            if (n > OUT_FRAMES) n = OUT_FRAMES;
            ring_take(u->out, n);
            lens[i] = uac_pack(u->out, n, a->channels, a->subslot, buf + (u64)i * esit);
        }
        return;
    }
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
    int best = -1, bs = -1;
    for (int k = 0; k < u->nalts; k++) {
        u32 r = uac_pick_rate(&u->alts[k]);
        if (u->out_dir && r != 48000) continue;   /* a mistura sai a 48 kHz: saída só nessa taxa */
        int s = uac_score(&u->alts[k], r);
        if (s > bs) { bs = s; best = k; }
    }
    if (best < 0) { kprintf("UAC: [%s] saída sem 48 kHz: não usada\n", d->path); return; }
    u->cur = best;
    uac_alt_info *a = &u->alts[best];
    u->rate = uac_pick_rate(a);
    if (usb_set_interface(d, it, a->alt) != USB_OK) { kprintf("UAC: [%s] alternate %u recusada\n", d->path, a->alt); return; }
    int idx;
    if (!usb_find_ep(it, USB_EP_ISOC, !u->out_dir, &idx)) return;
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
    u->acc = 0;
    u->streaming = true;
    if (u->out_dir) {
        if (sink_dev) { u->streaming = false; kfree(u->out); u->out = NULL; return; }   /* uma saída USB por vez */
        ring_w = ring_r = 0;
        sink_dev = u;
        audio_usb_sink = usb_sink;
    }
    if (usb_iso_start(d, u->ep, 0, 8, 8, iso_cb, u) != USB_OK) {
        u->streaming = false;
        if (u->out_dir) { audio_usb_sink = NULL; sink_dev = NULL; }
        kfree(u->out); u->out = NULL;
        return;
    }
    if (!u->out_dir) audio_input_attach(d->product[0] ? d->product : "Microfone USB");
    kprintf("UAC: [%s] %u Hz, %u canal(is), %u bits\n", d->path, u->rate, a->channels, a->bits);
}

static void uac_detach(usb_dev_t *d, usb_iface_t *it) {
    (void)d;
    uac_t *u = it->priv;
    if (!u) return;
    if (u->streaming) {
        u->streaming = false;
        if (u->out_dir) { if (sink_dev == u) { audio_usb_sink = NULL; sink_dev = NULL; } }
        else audio_input_detach();
    }
    kfree(u->out);
    u->out = NULL;
    u->used = false;
    it->priv = NULL;
}

const usb_driver_t usb_uac_driver = { "uac", uac_probe, uac_start, uac_detach, NULL };
