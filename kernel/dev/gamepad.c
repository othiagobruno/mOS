/*
 * mOS — controles de jogo: registro único para o GameController.framework dos apps do macOS.
 *
 * Fontes:
 *  - HID USB genérico (DualShock 4, DualSense, 8BitDo, Logitech, controles "DirectInput"): usb_hid.c lê o
 *    report descriptor e chama gamepad_report() com o estado já no layout padrão (A/B/X/Y, ombros, gatilhos,
 *    analógicos, direcional);
 *  - XInput: Xbox 360 com fio (classe ff/5d/01) e Xbox One/Series (GIP, classe ff/47/d0), driver aqui mesmo.
 * Vibração: o app pede pelo GUI_DEVICE (DEV_PAD_RUMBLE); o pedido é aplicado no laço principal (gamepad_poll),
 * porque as transferências USB síncronas não podem rodar dentro dos callbacks de interrupt IN.
 */
#include "usb.h"
#include "proc.h"
#include "../include/compat_gui.h"
#include "syscall.h"
#include "gfx.h"

#define MAX_PADS 4

typedef struct {
    bool used;
    gui_gamepad_t st;
    void (*rumble)(void *ctx, u8 strong, u8 weak);
    void *ctx;
    u8 want_s, want_w, cur_s, cur_w;
    u64 until;
} pad_t;
static pad_t pads[MAX_PADS];

int gamepad_attach(const char *name, u16 vid, u16 pid, u32 kind, u32 caps, void (*rumble)(void *, u8, u8), void *ctx) {
    for (int i = 0; i < MAX_PADS; i++) {
        pad_t *p = &pads[i];
        if (p->used) continue;
        u32 seq = p->st.seq;
        memset(p, 0, sizeof *p);
        p->used = true;
        p->st.connected = 1;
        p->st.seq = seq + 1;
        snprintf(p->st.name, sizeof p->st.name, "%s", name && *name ? name : "Controle de jogo");
        p->st.vid = vid; p->st.pid = pid; p->st.kind = kind; p->st.caps = rumble ? caps : 0;
        p->rumble = rumble; p->ctx = ctx;
        kprintf("controle de jogo %d: %s (%04x:%04x)%s\n", i, p->st.name, vid, pid, rumble ? ", vibração" : "");
        return i;
    }
    return -1;
}
void gamepad_detach(int id) {
    if (id < 0 || id >= MAX_PADS || !pads[id].used) return;
    pads[id].used = false;
    pads[id].st.connected = 0;
    pads[id].st.seq++;
    pads[id].rumble = NULL;
    kprintf("controle de jogo %d desconectado\n", id);
}
static float clamp1(float v) { return v < -1 ? -1 : v > 1 ? 1 : v; }
static float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
void gamepad_report(int id, u32 buttons, float lx, float ly, float rx, float ry, float lt, float rt) {
    if (id < 0 || id >= MAX_PADS || !pads[id].used) return;
    gui_gamepad_t *s = &pads[id].st;
    /* zona morta pequena: analógicos baratos não param exatamente no centro */
    const float dz = 0.06f;
    float v[4] = {lx, ly, rx, ry};
    for (int k = 0; k < 4; k++) { v[k] = clamp1(v[k]); if (v[k] > -dz && v[k] < dz) v[k] = 0; }
    s->buttons = buttons;
    s->lx = v[0]; s->ly = v[1]; s->rx = v[2]; s->ry = v[3];
    s->lt = clamp01(lt); s->rt = clamp01(rt);
    s->seq++;
}
/* direcional HID (hat switch 0..7 a partir do norte, no sentido horário; fora disso = solto) */
u32 gamepad_hat_buttons(int hat) {
    static const u32 m[8] = {PAD_UP, PAD_UP | PAD_RIGHT, PAD_RIGHT, PAD_DOWN | PAD_RIGHT, PAD_DOWN, PAD_DOWN | PAD_LEFT, PAD_LEFT, PAD_UP | PAD_LEFT};
    return hat >= 0 && hat < 8 ? m[hat] : 0;
}

void gamepad_poll(void) {
    u64 now = timer_ticks();
    for (int i = 0; i < MAX_PADS; i++) {
        pad_t *p = &pads[i];
        if (!p->used || !p->rumble) continue;
        if (p->until && now >= p->until) { p->want_s = p->want_w = 0; p->until = 0; }
        if (p->want_s != p->cur_s || p->want_w != p->cur_w) {
            p->cur_s = p->want_s; p->cur_w = p->want_w;
            p->rumble(p->ctx, p->cur_s, p->cur_w);
        }
    }
}

i64 gamepad_syscall(process_t *pr, u64 op, u64 b, u64 c, u64 d) {
    switch (op) {
    case DEV_PAD_GET:
        if (b >= MAX_PADS) return 0;
        if (c && !uvm_copy_out(pr, c, &pads[b].st, sizeof(gui_gamepad_t))) return -MOS_EFAULT;
        return 1;
    case DEV_AUDIO_PLAY: {
        /* PCM do app (CoreHaptics: eventos de áudio) direto no mixer, em pedaços de até 1/4 s */
        u64 frames = c > 12000 ? 12000 : c;
        if (!frames) return 0;
        i16 *buf = kmalloc((usize)frames * 4);
        if (!buf) return -MOS_ENOMEM;
        if (!uvm_copy_in(pr, buf, b, (usize)frames * 4)) { kfree(buf); return -MOS_EFAULT; }
        if (audio_available()) audio_play(buf, (u32)frames);
        kfree(buf);
        return (i64)frames;
    }
    case DEV_INPUT_POST: {
        /* CGEventPost / CGEventPostToPid: o evento sintético entra na mesma fila do teclado e do mouse do hardware,
         * então vale para o sistema todo (a janela em foco recebe a tecla; o clique vai para o que está sob o cursor) */
        input_event_t e;
        memset(&e, 0, sizeof e);
        e.type = (u8)(b & 0xFF);
        if (e.type != EV_KEY_DOWN && e.type != EV_KEY_UP && e.type != EV_MOUSE_ABS) return -MOS_EINVAL;
        e.buttons = (u8)((b >> 8) & 7);
        e.mods = (u16)((b >> 16) & 0xFFFF);
        e.dz = (i32)(b >> 32);
        e.key = (i32)c;
        e.dx = (i32)(u32)d;
        e.dy = (i32)(d >> 32);
        input_push(&e);
        return 0;
    }
    case DEV_SCREEN_CAPTURE: {
        /* CGDisplayCreateImage / CGWindowListCreateImage / SCScreenshotManager: um retângulo da tela composta (o
         * backbuffer do compositor, com todos os monitores) em ARGB 32 bits, linha a linha; d = 0 só pergunta o tamanho */
        i32 x = (i32)(u32)b, y = (i32)(b >> 32), w = (i32)(u32)c, h = (i32)(c >> 32);
        if (!g_screen) return -MOS_EINVAL;
        if (w <= 0 || h <= 0) { x = 0; y = 0; w = g_screen->w; h = g_screen->h; }
        if (x < 0) { w += x; x = 0; }
        if (y < 0) { h += y; y = 0; }
        if (x + w > g_screen->w) w = g_screen->w - x;
        if (y + h > g_screen->h) h = g_screen->h - y;
        if (w <= 0 || h <= 0) return -MOS_EINVAL;
        if (!d) return (i64)((u64)w | (u64)h << 32);
        for (i32 r = 0; r < h; r++)
            if (!uvm_copy_out(pr, d + (u64)r * (u64)w * 4, g_screen->px + (u64)(y + r) * (u64)g_screen->stride + x, (usize)w * 4))
                return -MOS_EFAULT;
        return (i64)((u64)w | (u64)h << 32);
    }
    case DEV_PAD_RUMBLE: {
        if (b >= MAX_PADS || !pads[b].used) return -MOS_EINVAL;
        pad_t *p = &pads[b];
        if (!p->rumble) return -MOS_EINVAL;
        p->want_s = (u8)(c & 0xFF); p->want_w = (u8)((c >> 8) & 0xFF);
        p->until = (p->want_s || p->want_w) ? timer_ticks() + (d ? MIN(d, 10000u) : 10000u) : 0;
        return 0;
    }
    }
    return -MOS_EINVAL;
}

/* ------------------------------------------------------------------ XInput (Xbox 360 / Xbox One) */
#define MAX_X 4
typedef struct {
    bool used, active, one;
    usb_dev_t *d;
    usb_iface_t *it;
    u8 in_ep, out_ep;
    u16 mps;
    int pad;
    u8 seq;
    u32 guide;              /* Xbox One: o botão Xbox chega num relatório próprio */
    bool init_pending;
} xin_t;
static xin_t xins[MAX_X];

static i16 le16s(const u8 *p) { return (i16)(p[0] | p[1] << 8); }
static u16 le16u(const u8 *p) { return (u16)(p[0] | p[1] << 8); }
static float stick(i16 v) { return v < 0 ? v / 32768.0f : v / 32767.0f; }

static void x_send(xin_t *x, const u8 *b, u32 n) {
    if (!x->out_ep) return;
    u8 buf[64];
    memcpy(buf, b, n);
    usb_bulk(x->d, x->out_ep, buf, n, 100);
}
static void x_rumble(void *ctx, u8 strong, u8 weak) {
    xin_t *x = ctx;
    if (!x->used || !x->active) return;
    if (x->one) {
        /* GIP: comando 0x09, todos os motores, intensidade 0..100 */
        u8 b[13] = {0x09, 0x00, x->seq++, 0x09, 0x00, 0x0F, 0x00, 0x00, (u8)(strong * 100 / 255), (u8)(weak * 100 / 255), 0xFF, 0x00, 0xFF};
        x_send(x, b, sizeof b);
    } else {
        u8 b[8] = {0x00, 0x08, 0x00, strong, weak, 0x00, 0x00, 0x00};
        x_send(x, b, sizeof b);
    }
}
static void x_cb(usb_dev_t *d, u8 ep, u8 *r, u32 len, int status, void *ctx) {
    (void)d; (void)ep;
    xin_t *x = ctx;
    if (!x->used || !x->active || status < 0) return;
    if (!x->one) {
        if (len < 14 || r[0] != 0x00 || r[1] < 0x14) return;
        u16 w = le16u(r + 2);
        u32 b = 0;
        if (w & 0x0001) b |= PAD_UP;
        if (w & 0x0002) b |= PAD_DOWN;
        if (w & 0x0004) b |= PAD_LEFT;
        if (w & 0x0008) b |= PAD_RIGHT;
        if (w & 0x0010) b |= PAD_MENU;
        if (w & 0x0020) b |= PAD_OPTIONS;
        if (w & 0x0040) b |= PAD_LS;
        if (w & 0x0080) b |= PAD_RS;
        if (w & 0x0100) b |= PAD_LB;
        if (w & 0x0200) b |= PAD_RB;
        if (w & 0x0400) b |= PAD_HOME;
        if (w & 0x1000) b |= PAD_A;
        if (w & 0x2000) b |= PAD_B;
        if (w & 0x4000) b |= PAD_X;
        if (w & 0x8000) b |= PAD_Y;
        gamepad_report(x->pad, b, stick(le16s(r + 6)), stick(le16s(r + 8)), stick(le16s(r + 10)), stick(le16s(r + 12)), r[4] / 255.0f, r[5] / 255.0f);
        return;
    }
    if (len >= 5 && r[0] == 0x07) { x->guide = (r[4] & 1) ? PAD_HOME : 0; return; }   /* botão Xbox */
    if (len < 18 || r[0] != 0x20) return;
    u32 b = x->guide;
    u8 b4 = r[4], b5 = r[5];
    if (b4 & 0x04) b |= PAD_MENU;
    if (b4 & 0x08) b |= PAD_OPTIONS;
    if (b4 & 0x10) b |= PAD_A;
    if (b4 & 0x20) b |= PAD_B;
    if (b4 & 0x40) b |= PAD_X;
    if (b4 & 0x80) b |= PAD_Y;
    if (b5 & 0x01) b |= PAD_UP;
    if (b5 & 0x02) b |= PAD_DOWN;
    if (b5 & 0x04) b |= PAD_LEFT;
    if (b5 & 0x08) b |= PAD_RIGHT;
    if (b5 & 0x10) b |= PAD_LB;
    if (b5 & 0x20) b |= PAD_RB;
    if (b5 & 0x40) b |= PAD_LS;
    if (b5 & 0x80) b |= PAD_RS;
    gamepad_report(x->pad, b, stick(le16s(r + 10)), stick(le16s(r + 12)), stick(le16s(r + 14)), stick(le16s(r + 16)),
                   le16u(r + 6) / 1023.0f, le16u(r + 8) / 1023.0f);
}
static bool x_probe(usb_dev_t *d, usb_iface_t *it) {
    bool x360 = it->cls == 0xFF && it->sub == 0x5D && it->proto == 0x01;
    bool one = it->cls == 0xFF && it->sub == 0x47 && it->proto == 0xD0;
    if (!x360 && !one) return false;
    int iin, iout;
    if (!usb_find_ep(it, USB_EP_INTR, true, &iin)) return false;
    xin_t *x = NULL;
    for (int i = 0; i < MAX_X; i++) if (!xins[i].used) { x = &xins[i]; break; }
    if (!x) return false;
    memset(x, 0, sizeof *x);
    x->used = true; x->d = d; x->it = it; x->one = one; x->pad = -1;
    x->in_ep = it->ep[iin].bEndpointAddress;
    x->mps = it->ep[iin].wMaxPacketSize & 0x7FF;
    usb_add_ep(d, it, iin);
    if (usb_find_ep(it, USB_EP_INTR, false, &iout)) { x->out_ep = it->ep[iout].bEndpointAddress; usb_add_ep(d, it, iout); }
    it->priv = x;
    return true;
}
static void x_start(usb_dev_t *d, usb_iface_t *it) {
    xin_t *x = it->priv;
    if (!x) return;
    const char *nm = d->product[0] ? d->product : x->one ? "Controle Xbox" : "Controle Xbox 360";
    x->pad = gamepad_attach(nm, d->desc.idVendor, d->desc.idProduct, x->one ? PAD_KIND_XBOXONE : PAD_KIND_XBOX360,
                            PAD_CAP_RUMBLE, x->out_ep ? x_rumble : NULL, x);
    x->active = true;
    if (x->one) {
        u8 init[5] = {0x05, 0x20, x->seq++, 0x01, 0x00};          /* GIP: liga o controle */
        x_send(x, init, sizeof init);
    } else {
        u8 led[3] = {0x01, 0x03, 0x06 + (x->pad > 0 ? x->pad : 0)}; /* anel de luz: jogador 1..4 */
        x_send(x, led, sizeof led);
    }
    usb_set_kind(d, USB_KIND_HID);
    if (usb_intr_start(d, x->in_ep, MAX(x->mps, 32), x_cb, x) < 0) { kprintf("xinput: falha ao iniciar interrupt IN\n"); x->active = false; }
}
static void x_detach(usb_dev_t *d, usb_iface_t *it) {
    (void)d;
    xin_t *x = it->priv;
    if (!x) return;
    gamepad_detach(x->pad);
    memset(x, 0, sizeof *x);
    it->priv = NULL;
}
static void x_poll(void) { gamepad_poll(); }
const usb_driver_t usb_xinput_driver = { "xinput", x_probe, x_start, x_detach, x_poll };
