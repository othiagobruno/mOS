/*
 * mOS — câmeras USB (USB Video Class 1.0/1.1/1.5) para o AVFoundation e o CoreMediaIO.
 *
 * A interface VideoStreaming (classe 0x0E, subclasse 2) é reivindicada aqui; os descritores, o PROBE/COMMIT e a
 * remontagem dos quadros ficam em uvc_core.c (testado no host por tools/uvc_core_test.c).
 *  - isócrono (a maioria das webcams): a alternate setting é escolhida pela banda que a câmera pediu no PROBE
 *    (dwMaxPayloadTransferSize); os payloads chegam no callback isócrono, que remonta o quadro e o copia para o
 *    buffer "pronto";
 *  - bulk (algumas câmeras USB 3): o DEV_CAMERA_READ puxa os payloads na thread de quem lê, até fechar um quadro.
 * O app lê o quadro mais novo; quadros que ele não leu a tempo são substituídos (como o
 * alwaysDiscardsLateVideoFrames do AVCaptureVideoDataOutput).
 * Código original do mOS (MIT).
 */
#include "usb.h"
#include "proc.h"
#include "../include/compat_gui.h"
#include "syscall.h"
#include "uvc_core.h"

#define MAX_CAMS 4
enum { VS_PROBE_CONTROL = 1, VS_COMMIT_CONTROL = 2 };
enum { SET_CUR = 0x01, GET_CUR = 0x81 };

typedef struct {
    bool used, gone;
    usb_dev_t *d;
    usb_iface_t *it;
    uint16_t bcd;
    uvc_stream_info si;
    int open_pid;
    bool bulk;
    u8 ep;
    uvc_format *fmt;
    uvc_frame *frm;
    uvc_probe pr;
    u32 interval;
    uvc_asm as;
    u8 *abuf, *ready, *user;     /* remontagem, último quadro completo, cópia para o app */
    u32 cap, ready_len;
    u64 seq, read_seq;
    u8 *pbuf;                    /* bulk: um payload */
    spinlock_t lock;
} cam_t;
static cam_t cams[MAX_CAMS];

static uint16_t vc_version(usb_dev_t *d) {
    for (int i = 0; i < d->nifaces; i++) {
        usb_iface_t *v = &d->iface[i];
        if (v->cls == 0x0E && v->sub == 1) {
            const u8 *x = v->alts[0].extra ? v->alts[0].extra : v->extra;
            u16 n = v->alts[0].extra ? v->alts[0].extra_len : v->extra_len;
            uint16_t b = x ? uvc_parse_vc_version(x, n) : 0;
            if (b) return b;
        }
    }
    return 0x0100;
}

static bool uvc_probe_iface(usb_dev_t *d, usb_iface_t *it) {
    if (it->cls != 0x0E || it->sub != 2) return false;
    cam_t *c = NULL;
    for (int i = 0; i < MAX_CAMS && !c; i++) if (!cams[i].used) c = &cams[i];
    if (!c) { kprintf("UVC: [%s] câmeras demais\n", d->path); return false; }
    const u8 *x = it->alts[0].extra ? it->alts[0].extra : it->extra;
    u16 n = it->alts[0].extra ? it->alts[0].extra_len : it->extra_len;
    memset(c, 0, sizeof *c);
    if (!x || uvc_parse_vs(x, n, &c->si) != 0) { kprintf("UVC: [%s] interface %u sem formato conhecido\n", d->path, it->num); return false; }
    c->used = true; c->d = d; c->it = it; c->bcd = vc_version(d);
    it->priv = c;
    /* câmera bulk: o endpoint fica na alt 0 e precisa ser configurado já */
    int idx;
    if (usb_find_ep(it, USB_EP_BULK, true, &idx)) { c->bulk = true; c->ep = it->ep[idx].bEndpointAddress; usb_add_ep(d, it, idx); }
    kprintf("UVC: [%s] %s, UVC %x.%02x, %u formato(s)%s\n", d->path, d->product[0] ? d->product : "câmera", c->bcd >> 8, c->bcd & 0xFF,
            c->si.nformat, c->bulk ? ", bulk" : "");
    return true;
}
static void uvc_start(usb_dev_t *d, usb_iface_t *it) { (void)d; (void)it; }

static void free_bufs(cam_t *c) {
    u64 f = spin_lock_irqsave(&c->lock);
    u8 *a = c->abuf, *r = c->ready, *u = c->user, *p = c->pbuf;
    c->abuf = c->ready = c->user = c->pbuf = NULL;
    c->cap = c->ready_len = 0;
    spin_unlock_irqrestore(&c->lock, f);
    kfree(a); kfree(r); kfree(u); kfree(p);
}
static void uvc_detach(usb_dev_t *d, usb_iface_t *it) {
    cam_t *c = it->priv;
    if (!c) return;
    c->gone = true;
    it->priv = NULL;
    /* aberta: os buffers ficam até o app fechar ou sair (ele pode estar copiando um quadro agora) */
    if (!c->open_pid) { free_bufs(c); c->used = false; }
    kprintf("UVC: [%s] câmera removida\n", d->path);
}

/* quadro completo (roda dentro do callback isócrono ou do READ bulk) */
static void got_frame(void *ctx, const uint8_t *data, uint32_t len, int fid) {
    (void)fid;
    cam_t *c = ctx;
    u64 f = spin_lock_irqsave(&c->lock);
    if (c->ready && len <= c->cap) { memcpy(c->ready, data, len); c->ready_len = len; c->seq++; }
    spin_unlock_irqrestore(&c->lock, f);
}
static void iso_cb(usb_dev_t *d, u8 ep, u8 *buf, int npkts, u32 *lens, int *status, void *ctx) {
    (void)d;
    cam_t *c = ctx;
    if (!c->abuf || c->gone) return;
    u32 esit = usb_iso_esit_max(c->d, ep);
    for (int i = 0; i < npkts; i++) {
        if (status[i] == USB_TIMEOUT || !lens[i]) continue;   /* intervalo sem dado (normal entre quadros) */
        if (status[i] != 0) { c->as.bad = 1; continue; }
        uvc_asm_payload(&c->as, buf + (u64)i * esit, lens[i]);
    }
}

/* banda de um endpoint isócrono por intervalo de serviço */
static u32 ep_bw(usb_dev_t *d, usb_alt_t *a, int i) {
    u16 mps = a->ep[i].wMaxPacketSize;
    if (d->speed >= USB_SPEED_SUPER) return a->ss_bpi[i] ? a->ss_bpi[i] : (u32)(mps & 0x7FF) * (a->ss_burst[i] + 1u) * (a->ss_mult[i] + 1u);
    return (u32)(mps & 0x7FF) * (((mps >> 11) & 3) + 1u);
}

static void choose(cam_t *c, u32 w, u32 h, u32 fourcc, u32 interval) {
    uvc_format *bf = NULL; uvc_frame *bfr = NULL;
    u64 best = ~0ull;
    for (int i = 0; i < c->si.nformat; i++) {
        uvc_format *f = &c->si.format[i];
        if (f->kind == UVC_FMT_OTHER) continue;
        if (fourcc && f->fourcc != fourcc) continue;
        for (int j = 0; j < f->nframe; j++) {
            uvc_frame *fr = &f->frame[j];
            u64 score;
            if (!w || !h) score = (fr->index == f->def_frame ? 0 : 1000000) + (u64)i * 10;   /* padrão do primeiro formato */
            else {
                i64 dw = (i64)fr->width - w, dh = (i64)fr->height - h;
                score = (u64)(dw * dw + dh * dh) * 4 + (fr->width >= w && fr->height >= h ? 0 : 1) + (f->kind == UVC_FMT_MJPEG ? 2 : 0);
            }
            if (score < best) { best = score; bf = f; bfr = fr; }
        }
    }
    if (!bf && fourcc) { choose(c, w, h, 0, interval); return; }
    c->fmt = bf; c->frm = bfr;
    if (!bfr) return;
    u32 iv = bfr->def_interval;
    if (interval) {
        if (bfr->ninterval == 2 && bfr->interval[0] < bfr->interval[1] && bfr->interval[0] <= interval && interval <= bfr->interval[1]) iv = interval;
        else {   /* o discreto mais perto */
            u32 bd = ~0u;
            for (int k = 0; k < bfr->ninterval; k++) {
                u32 dd = bfr->interval[k] > interval ? bfr->interval[k] - interval : interval - bfr->interval[k];
                if (dd < bd) { bd = dd; iv = bfr->interval[k]; }
            }
        }
    }
    if (!iv) iv = 333333;
    c->interval = iv;
}

static int negotiate(cam_t *c) {
    int len = uvc_probe_len(c->bcd);
    u8 b[48];
    for (int tries = 0; tries < 2; tries++) {
        uvc_probe p = { .hint = 1, .format = c->fmt->index, .frame = c->frm->index, .interval = c->interval };
        uvc_probe_pack(&p, b, len);
        int r = usb_control(c->d, 0x21, SET_CUR, VS_PROBE_CONTROL << 8, c->it->num, len, b);
        if (r < 0 && len != 26) { len = 26; continue; }   /* câmeras que anunciam 1.1 e só aceitam o bloco de 1.0 */
        if (r < 0) return r;
        r = usb_control(c->d, 0xA1, GET_CUR, VS_PROBE_CONTROL << 8, c->it->num, len, b);
        if (r < 0) return r;
        uvc_probe_unpack(b, len, &c->pr);
        r = usb_control(c->d, 0x21, SET_CUR, VS_COMMIT_CONTROL << 8, c->it->num, len, b);
        return r < 0 ? r : 0;
    }
    return USB_ERR;
}

static int cam_open(cam_t *c, int pid, u32 w, u32 h, u32 fourcc, u32 interval) {
    if (c->open_pid) return -MOS_EBUSY;
    choose(c, w, h, fourcc, interval);
    if (!c->frm) return -MOS_EINVAL;
    usb_set_interface(c->d, c->it, 0);
    if (negotiate(c) < 0) { kprintf("UVC: [%s] PROBE/COMMIT recusado\n", c->d->path); return -MOS_EIO; }
    /* tamanho do quadro: o do COMMIT; descompactado tem que bater com largura·altura·bpp */
    u32 fsz = c->pr.max_frame_size ? c->pr.max_frame_size : c->frm->max_size;
    u32 exact = 0;
    if (c->fmt->kind != UVC_FMT_MJPEG) {
        exact = (u32)c->frm->width * c->frm->height * (c->fmt->bpp ? c->fmt->bpp : 16) / 8;
        if (fsz < exact) fsz = exact;
    }
    if (!fsz) fsz = (u32)c->frm->width * c->frm->height * 2;
    if (fsz > 32u << 20) return -MOS_EINVAL;
    u8 *a = kmalloc(fsz), *r = kmalloc(fsz), *u = kmalloc(fsz);
    if (!a || !r || !u) { kfree(a); kfree(r); kfree(u); return -MOS_ENOMEM; }
    u64 fl = spin_lock_irqsave(&c->lock);
    c->abuf = a; c->ready = r; c->user = u; c->cap = fsz; c->ready_len = 0; c->seq = c->read_seq = 0;
    uvc_asm_init(&c->as, a, fsz, exact, got_frame, c);
    spin_unlock_irqrestore(&c->lock, fl);
    if (c->bulk) {
        u32 pl = c->pr.max_payload ? c->pr.max_payload : 512 * 1024;
        if (pl > (4u << 20)) pl = 4u << 20;
        c->pbuf = kmalloc(pl);
        if (!c->pbuf) { free_bufs(c); return -MOS_ENOMEM; }
        c->pr.max_payload = pl;
        c->open_pid = pid;
        return 0;
    }
    /* isócrono: a menor alternate setting que dá conta da banda pedida */
    usb_iface_t *it = c->it;
    u32 bw[USB_MAX_ALTS]; int map[USB_MAX_ALTS], n = 0;
    for (int k = 0; k < it->nalt; k++) {
        usb_alt_t *al = &it->alts[k];
        if (!al->alt) continue;
        for (int e = 0; e < al->nep; e++)
            if ((al->ep[e].bmAttributes & 3) == USB_EP_ISOC && (al->ep[e].bEndpointAddress & 0x80)) { bw[n] = ep_bw(c->d, al, e); map[n++] = k; break; }
    }
    int pick = uvc_pick_alt(bw, n, c->pr.max_payload);
    if (pick < 0) { free_bufs(c); return -MOS_ENXIO; }
    if (usb_set_interface(c->d, it, it->alts[map[pick]].alt) != USB_OK) { free_bufs(c); return -MOS_EIO; }
    int idx;
    if (!usb_find_ep(it, USB_EP_ISOC, true, &idx)) { usb_set_interface(c->d, it, 0); free_bufs(c); return -MOS_ENXIO; }
    c->ep = it->ep[idx].bEndpointAddress;
    u32 esit = usb_iso_esit_max(c->d, c->ep);
    int nbufs = 4, npkts = esit ? (int)((2u << 20) / (esit * (u32)nbufs)) : 8;
    if (npkts > 48) npkts = 48;
    if (npkts < 8) npkts = 8;
    c->open_pid = pid;
    if (usb_iso_start(c->d, c->ep, 0, npkts, nbufs, iso_cb, c) != USB_OK) {
        c->open_pid = 0; usb_set_interface(c->d, it, 0); free_bufs(c); return -MOS_EIO;
    }
    kprintf("UVC: [%s] %ux%u %s, %u qps, alt %u (%u B/intervalo)\n", c->d->path, c->frm->width, c->frm->height,
            c->fmt->kind == UVC_FMT_MJPEG ? "MJPEG" : c->fmt->kind == UVC_FMT_NV12 ? "NV12" : "YUY2", 10000000u / c->interval,
            it->alts[map[pick]].alt, bw[pick]);
    return 0;
}

static void cam_close(cam_t *c) {
    if (!c->open_pid) return;
    if (!c->gone) {
        if (!c->bulk) usb_iso_stop(c->d, c->ep);
        usb_set_interface(c->d, c->it, 0);
    }
    c->open_pid = 0;
    free_bufs(c);
    if (c->gone) c->used = false;
}

/* bulk: puxa payloads até fechar um quadro (no máximo ~1 s) */
static void bulk_pull(cam_t *c) {
    u64 seq0 = c->seq, t0 = timer_ticks();
    while (c->seq == seq0 && !c->gone && timer_ticks() - t0 < 1000) {
        int r = usb_bulk(c->d, c->ep, c->pbuf, c->pr.max_payload, 200);
        if (r == USB_STALL) { usb_clear_halt(c->d, c->ep); continue; }
        if (r < 0) break;
        uvc_asm_payload(&c->as, c->pbuf, (u32)r);
    }
}

static void fill_mode(gui_camera_mode_t *m, uvc_format *f, uvc_frame *fr) {
    m->fourcc = f->fourcc; m->width = fr->width; m->height = fr->height;
    m->def_interval = fr->def_interval; m->max_size = fr->max_size;
    m->min_interval = m->max_interval = fr->def_interval;
    for (int k = 0; k < fr->ninterval; k++) {
        if (fr->interval[k] < m->min_interval) m->min_interval = fr->interval[k];
        if (fr->interval[k] > m->max_interval) m->max_interval = fr->interval[k];
    }
}

static cam_t *cam_at(u64 i) {
    int k = 0;
    for (int j = 0; j < MAX_CAMS; j++) if (cams[j].used && !cams[j].gone) { if ((u64)k == i) return &cams[j]; k++; }
    return NULL;
}

i64 uvc_syscall(process_t *pr, u64 op, u64 b, u64 cc, u64 d) {
    cam_t *c = cam_at(b);
    switch (op) {
    case DEV_CAMERA_INFO: {
        if (!c) return 0;
        gui_camera_info_t *info = kcalloc(1, sizeof *info);
        if (!info) return -MOS_ENOMEM;
        strncpy(info->name, c->d->product[0] ? c->d->product : "Câmera USB", sizeof info->name - 1);
        strncpy(info->path, c->d->path, sizeof info->path - 1);
        info->vid = c->d->desc.idVendor; info->pid = c->d->desc.idProduct;
        for (int i = 0; i < c->si.nformat; i++) {
            uvc_format *f = &c->si.format[i];
            if (f->kind == UVC_FMT_OTHER) continue;
            for (int j = 0; j < f->nframe && info->nmode < GUI_CAMERA_MODES; j++) fill_mode(&info->mode[info->nmode++], f, &f->frame[j]);
        }
        info->open_pid = c->open_pid;
        if (c->open_pid && c->frm) { fill_mode(&info->cur, c->fmt, c->frm); info->cur_interval = c->interval; }
        info->frames = c->as.frames; info->dropped = c->as.dropped;
        bool ok = cc && uvm_copy_out(pr, cc, info, sizeof *info);
        kfree(info);
        return ok ? 1 : -MOS_EFAULT;
    }
    case DEV_CAMERA_OPEN:
        if (!c) return -MOS_ENXIO;
        return cam_open(c, pr->pid, (u32)cc, (u32)(cc >> 32), (u32)d, (u32)(d >> 32));
    case DEV_CAMERA_READ: {
        if (!c) return -MOS_ENXIO;
        if (c->open_pid != pr->pid) return -MOS_EBADF;
        if (c->bulk) bulk_pull(c);
        u64 f = spin_lock_irqsave(&c->lock);
        if (!c->ready || c->seq == c->read_seq) { spin_unlock_irqrestore(&c->lock, f); return 0; }
        u32 n = c->ready_len;
        if (n > d) { spin_unlock_irqrestore(&c->lock, f); return -MOS_ENOSPC; }
        u8 *t = c->ready; c->ready = c->user; c->user = t;   /* o app copia sem segurar a trava */
        c->read_seq = c->seq;
        spin_unlock_irqrestore(&c->lock, f);
        return uvm_copy_out(pr, cc, c->user, n) ? (i64)n : -MOS_EFAULT;
    }
    case DEV_CAMERA_CLOSE:
        if (!c) return -MOS_ENXIO;
        if (c->open_pid != pr->pid) return -MOS_EBADF;
        cam_close(c);
        return 0;
    }
    return -MOS_EINVAL;
}

/* o processo saiu: solta as câmeras dele */
void uvc_proc_exit(int pid) {
    for (int i = 0; i < MAX_CAMS; i++) if (cams[i].used && cams[i].open_pid == pid) cam_close(&cams[i]);
}

const usb_driver_t usb_uvc_driver = { "uvc", uvc_probe_iface, uvc_start, uvc_detach, NULL };
