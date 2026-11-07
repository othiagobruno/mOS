/*
 * mOS — driver USB HID: teclado (boot protocol), mouse e apontadores absolutos (tablet).
 *
 * Teclado: relatórios boot de 8 bytes → scancodes set 1 → input_kbd_scancode() (mesma lógica de
 *          layout ABNT2/US e teclas mortas do PS/2).  Repetição por software: 500 ms / 33 ms.
 * Apontador: report protocol com um parser mínimo de report descriptor (X, Y, roda, botões;
 *          relativo ou absoluto).  Se o parser falhar e a interface for boot mouse, usa boot protocol.
 */
#include "usb.h"
#include "compat_gui.h"      /* PAD_*: layout dos botões do GameController */

#define MAX_HID 12

typedef struct { bool have; u16 off, size; i32 lmin, lmax; bool abs; } hfield_t;

typedef struct {
    bool used, active;
    usb_dev_t *d;
    usb_iface_t *it;
    int  type;                 /* 1 = teclado, 2 = apontador */
    u8   ep;
    u16  mps;
    /* teclado */
    u8   prev[8];
    u8   rep_usage;
    u64  rep_next;
    u64  last_rx;       /* último relatório de teclado recebido (ms) */
    u8   leds;
    bool led_fail;
    /* apontador */
    bool boot, ids, abs;
    u8   rid;
    hfield_t x, y, wheel;
    u16  btn_off;
    u8   nbtn;
    u8   last_buttons;
    i32  last_x, last_y;
    /* controle de jogo (type 3): eixos X Y Z Rx Ry Rz, acelerador/freio, direcional e até 16 botões */
    hfield_t gax[8], ghat;
    u16  gbtn_off;
    u8   gnbtn, grid;
    bool gids;
    int  pad;
} hid_t;
enum { GA_X, GA_Y, GA_Z, GA_RX, GA_RY, GA_RZ, GA_ACCEL, GA_BRAKE };

static hid_t hids[MAX_HID];
static int key_log;

/* uso HID (página 7) → scancode set 1 (0x100 = prefixo E0); 0 = ignorado */
static const u16 hid2sc[256] = {
    [0x04] = 0x1E, [0x05] = 0x30, [0x06] = 0x2E, [0x07] = 0x20, [0x08] = 0x12, [0x09] = 0x21, [0x0A] = 0x22,
    [0x0B] = 0x23, [0x0C] = 0x17, [0x0D] = 0x24, [0x0E] = 0x25, [0x0F] = 0x26, [0x10] = 0x32, [0x11] = 0x31,
    [0x12] = 0x18, [0x13] = 0x19, [0x14] = 0x10, [0x15] = 0x13, [0x16] = 0x1F, [0x17] = 0x14, [0x18] = 0x16,
    [0x19] = 0x2F, [0x1A] = 0x11, [0x1B] = 0x2D, [0x1C] = 0x15, [0x1D] = 0x2C,
    [0x1E] = 0x02, [0x1F] = 0x03, [0x20] = 0x04, [0x21] = 0x05, [0x22] = 0x06, [0x23] = 0x07, [0x24] = 0x08,
    [0x25] = 0x09, [0x26] = 0x0A, [0x27] = 0x0B,
    [0x28] = 0x1C, [0x29] = 0x01, [0x2A] = 0x0E, [0x2B] = 0x0F, [0x2C] = 0x39, [0x2D] = 0x0C, [0x2E] = 0x0D,
    [0x2F] = 0x1A, [0x30] = 0x1B, [0x31] = 0x2B, [0x32] = 0x2B, [0x33] = 0x27, [0x34] = 0x28, [0x35] = 0x29,
    [0x36] = 0x33, [0x37] = 0x34, [0x38] = 0x35, [0x39] = 0x3A,
    [0x3A] = 0x3B, [0x3B] = 0x3C, [0x3C] = 0x3D, [0x3D] = 0x3E, [0x3E] = 0x3F, [0x3F] = 0x40, [0x40] = 0x41,
    [0x41] = 0x42, [0x42] = 0x43, [0x43] = 0x44, [0x44] = 0x57, [0x45] = 0x58,
    [0x46] = 0x137, [0x47] = 0x46, [0x49] = 0x152, [0x4A] = 0x147, [0x4B] = 0x149, [0x4C] = 0x153,
    [0x4D] = 0x14F, [0x4E] = 0x151, [0x4F] = 0x14D, [0x50] = 0x14B, [0x51] = 0x150, [0x52] = 0x148,
    [0x53] = 0x45, [0x54] = 0x135, [0x55] = 0x37, [0x56] = 0x4A, [0x57] = 0x4E, [0x58] = 0x11C,
    [0x59] = 0x4F, [0x5A] = 0x50, [0x5B] = 0x51, [0x5C] = 0x4B, [0x5D] = 0x4C, [0x5E] = 0x4D,
    [0x5F] = 0x47, [0x60] = 0x48, [0x61] = 0x49, [0x62] = 0x52, [0x63] = 0x53, [0x64] = 0x56, [0x65] = 0x15D,
    [0x85] = 0x7E,   /* vírgula do teclado numérico (ABNT2: '.') */
    [0x87] = 0x73,   /* International1: tecla /? do ABNT2 */
};
/* bits de modificador do relatório boot → scancode */
static const u16 modsc[8] = {0x1D, 0x2A, 0x38, 0x15B, 0x11D, 0x36, 0x138, 0x15C};

static bool repeatable(u8 u) { return hid2sc[u] && u != 0x39 && u != 0x53 && u != 0x47; }

static void key(u8 usage, bool up) {
    u16 sc = hid2sc[usage];
    if (!sc) return;
    if (key_log < 40) { key_log++; kprintf("USB HID: tecla uso 0x%02x %s → scancode %03x (via USB)\n", usage, up ? "solta" : "pressionada", sc); }
    input_kbd_scancode(sc, up);
}

static bool has(const u8 *k, int n, u8 u) {
    for (int i = 0; i < n; i++) if (k[i] == u) return true;
    return false;
}

static void kbd_report(hid_t *h, const u8 *r, u32 len) {
    if (len < 3) return;
    h->last_rx = timer_ticks();
    int n = MIN((int)len - 2, 6);
    const u8 *k = r + 2;
    for (int i = 0; i < n; i++) if (k[i] == 1) return;   /* ErrorRollOver: estado inválido */
    u8 ch = r[0] ^ h->prev[0];
    for (int b = 0; b < 8; b++) if (ch & (1 << b)) input_kbd_scancode(modsc[b], !(r[0] & (1 << b)));
    const u8 *ok = h->prev + 2;
    for (int i = 0; i < 6; i++) {
        u8 u = ok[i];
        if (u > 3 && !has(k, n, u)) { key(u, true); if (u == h->rep_usage) h->rep_usage = 0; }
    }
    for (int i = 0; i < n; i++) {
        u8 u = k[i];
        if (u > 3 && !has(ok, 6, u)) {
            key(u, false);
            if (repeatable(u)) { h->rep_usage = u; h->rep_next = timer_ticks() + 500; }
        }
    }
    memset(h->prev, 0, sizeof h->prev);
    memcpy(h->prev, r, MIN(len, 8u));
}

static void kbd_release_all(hid_t *h) {
    for (int b = 0; b < 8; b++) if (h->prev[0] & (1 << b)) input_kbd_scancode(modsc[b], true);
    for (int i = 2; i < 8; i++) if (h->prev[i] > 3) input_kbd_scancode(hid2sc[h->prev[i]], true);
    memset(h->prev, 0, sizeof h->prev);
    h->rep_usage = 0;
}

/* teclado Bluetooth LE (kernel/bt/ble.c): relatório boot já separado em modificadores + 6 teclas */
void input_hid_keyboard(u8 mods, const u8 keys[6]) {
    static hid_t ble_kbd;
    u8 r[8] = {mods, 0};
    memcpy(r + 2, keys, 6);
    kbd_report(&ble_kbd, r, 8);
}

/* ------------------------------------------------------------------ apontador */
static i32 getbits(const u8 *b, u32 len, u32 off, u32 size, bool sgn) {
    if (!size || size > 32 || off + size > len * 8) return 0;
    u64 v = 0;
    for (u32 i = 0; i < size; i++) {
        u32 bit = off + i;
        if ((b[bit >> 3] >> (bit & 7)) & 1) v |= 1ULL << i;
    }
    if (sgn && size < 64 && ((v >> (size - 1)) & 1)) v |= ~0ULL << size;
    return (i32)(i64)v;
}

static i32 field(const hfield_t *f, const u8 *r, u32 len, u32 base) {
    return getbits(r, len, base + f->off, f->size, f->lmin < 0);
}

static void ptr_report(hid_t *h, const u8 *r, u32 len) {
    input_event_t ev = {0};
    ev.mods = input_mods();
    i32 wheel = 0;
    if (h->boot) {
        if (len < 3) return;
        ev.buttons = r[0] & 7;
        ev.dx = (i8)r[1];
        ev.dy = (i8)r[2];
        if (len >= 4) wheel = (i8)r[3];
        ev.type = EV_MOUSE;
    } else {
        u32 base = 0;
        if (h->ids) { if (len < 1 || r[0] != h->rid) return; base = 8; }
        u8 b = 0;
        for (int i = 0; i < h->nbtn && i < 3; i++) if (getbits(r, len, base + h->btn_off + i, 1, false)) b |= 1 << i;
        ev.buttons = b;
        i32 x = field(&h->x, r, len, base), y = field(&h->y, r, len, base);
        if (h->wheel.have) wheel = field(&h->wheel, r, len, base);
        if (h->abs) {
            i32 rx = h->x.lmax - h->x.lmin, ry = h->y.lmax - h->y.lmin;
            if (rx <= 0 || ry <= 0) return;
            i32 W = (i32)g_boot->fb.width, H = (i32)g_boot->fb.height;
            ev.dx = (i32)((i64)(CLAMP(x, h->x.lmin, h->x.lmax) - h->x.lmin) * (W - 1) / rx);
            ev.dy = (i32)((i64)(CLAMP(y, h->y.lmin, h->y.lmax) - h->y.lmin) * (H - 1) / ry);
            ev.type = EV_MOUSE_ABS;
            if (ev.dx == h->last_x && ev.dy == h->last_y && b == h->last_buttons && !wheel) return;
            h->last_x = ev.dx; h->last_y = ev.dy;
        } else {
            ev.dx = x; ev.dy = y;
            ev.type = EV_MOUSE;
        }
    }
    if (ev.type == EV_MOUSE && !ev.dx && !ev.dy && !wheel && ev.buttons == h->last_buttons) return;
    h->last_buttons = ev.buttons;
    ev.dz = -wheel;                      /* HID: positivo = para longe do usuário; mOS segue o PS/2 */    input_push(&ev);
}

/* parser mínimo de report descriptor: procura X/Y (Generic Desktop), roda e botões */
typedef struct { u8 rid; hfield_t x, y, wheel; u16 btn_off; u8 nbtn; bool used; } layout_t;

static bool parse_report(hid_t *h, const u8 *rd, int len) {
    layout_t lay[8];
    memset(lay, 0, sizeof lay);
    u16 off[256];
    memset(off, 0, sizeof off);
    u32 upage = 0, rsize = 0, rcount = 0, lmax_u = 0;
    i32 lmin = 0, lmax = 0;
    u8 rid = 0;
    bool ids = false;
    u32 usages[32];
    int nus = 0;
    u32 umin = 0, umax = 0;
    bool range = false;
    struct { u32 upage, rsize, rcount, lmax_u; i32 lmin, lmax; } stack[4];
    int sp = 0;

    for (int i = 0; i < len;) {
        u8 b = rd[i++];
        if (b == 0xFE) { if (i + 1 >= len) break; i += 2 + rd[i]; continue; }
        int sz = b & 3;
        if (sz == 3) sz = 4;
        if (i + sz > len) break;
        u32 uv = 0;
        for (int k = 0; k < sz; k++) uv |= (u32)rd[i + k] << (8 * k);
        i32 sv = sz == 1 ? (i8)uv : sz == 2 ? (i16)uv : (i32)uv;
        i += sz;
        int type = (b >> 2) & 3, tag = b >> 4;
        if (type == 0) {
            if (tag == 8) {          /* Input */
                bool cnst = uv & 1, var = uv & 2, rel = uv & 4;
                layout_t *L = NULL;
                for (int k = 0; k < 8 && !L; k++) if (lay[k].used && lay[k].rid == rid) L = &lay[k];
                for (int k = 0; k < 8 && !L; k++) if (!lay[k].used) { L = &lay[k]; L->used = true; L->rid = rid; }
                i32 mx = lmax < lmin ? (i32)lmax_u : lmax;
                for (u32 k = 0; k < rcount && k < 64; k++) {
                    u32 u = 0;
                    if (nus) u = usages[k < (u32)nus ? k : (u32)nus - 1];
                    else if (range) u = umin + k <= umax ? umin + k : umax;
                    if (!cnst && var && L) {
                        hfield_t f = {true, off[rid], (u16)rsize, lmin, mx, !rel};
                        if (u == 0x10030 && !L->x.have) L->x = f;
                        else if (u == 0x10031 && !L->y.have) L->y = f;
                        else if (u == 0x10038 && !L->wheel.have) L->wheel = f;
                        else if ((u >> 16) == 9 && rsize == 1 && (u & 0xFFFF) >= 1 && (u & 0xFFFF) <= 8) {
                            if (!L->nbtn) L->btn_off = off[rid] - ((u & 0xFFFF) - 1);
                            if (L->nbtn < (u & 0xFFFF)) L->nbtn = (u8)(u & 0xFFFF);
                        }
                    }
                    off[rid] += rsize;
                }
            }
            nus = 0; range = false;   /* itens locais valem só para o próximo item principal */
        } else if (type == 1) {
            switch (tag) {
            case 0: upage = uv; break;
            case 1: lmin = sv; break;
            case 2: lmax = sv; lmax_u = uv; break;
            case 7: rsize = uv; break;
            case 8: rid = (u8)uv; ids = true; break;
            case 9: rcount = uv; break;
            case 10: if (sp < 4) { stack[sp].upage = upage; stack[sp].rsize = rsize; stack[sp].rcount = rcount;
                                   stack[sp].lmax_u = lmax_u; stack[sp].lmin = lmin; stack[sp].lmax = lmax; sp++; } break;
            case 11: if (sp > 0) { sp--; upage = stack[sp].upage; rsize = stack[sp].rsize; rcount = stack[sp].rcount;
                                   lmax_u = stack[sp].lmax_u; lmin = stack[sp].lmin; lmax = stack[sp].lmax; } break;
            }
        } else if (type == 2) {
            u32 full = sz == 4 ? uv : (upage << 16) | uv;
            if (tag == 0 && nus < 32) usages[nus++] = full;
            else if (tag == 1) { umin = full; range = true; }
            else if (tag == 2) { umax = full; range = true; }
        }
    }
    for (int k = 0; k < 8; k++) {
        layout_t *L = &lay[k];
        if (!L->used || !L->x.have || !L->y.have) continue;
        h->ids = ids; h->rid = L->rid;
        h->x = L->x; h->y = L->y; h->wheel = L->wheel;
        h->btn_off = L->btn_off; h->nbtn = L->nbtn;
        h->abs = L->x.abs;
        return true;
    }
    return false;
}

/* ------------------------------------------------------------------ controles de jogo HID
 * Aceita a coleção de aplicação Joystick (0x04) ou Gamepad (0x05) da página Generic Desktop. */
static bool parse_gamepad(hid_t *h, const u8 *rd, int len) {
    u16 off[256];
    memset(off, 0, sizeof off);
    u32 upage = 0, rsize = 0, rcount = 0, lmax_u = 0;
    i32 lmin = 0, lmax = 0;
    u8 rid = 0;
    bool ids = false, is_pad = false;
    u32 usages[32];
    int nus = 0;
    u32 umin = 0, umax = 0;
    bool range = false;
    int pad_rid = -1;
    for (int i = 0; i < len;) {
        u8 b = rd[i++];
        if (b == 0xFE) { if (i + 1 >= len) break; i += 2 + rd[i]; continue; }
        int sz = b & 3;
        if (sz == 3) sz = 4;
        if (i + sz > len) break;
        u32 uv = 0;
        for (int k = 0; k < sz; k++) uv |= (u32)rd[i + k] << (8 * k);
        i32 sv = sz == 1 ? (i8)uv : sz == 2 ? (i16)uv : (i32)uv;
        i += sz;
        int type = (b >> 2) & 3, tag = b >> 4;
        if (type == 0) {
            if (tag == 10 && uv == 1) {               /* Collection(Application) */
                u32 u = nus ? usages[0] : 0;
                if (u == 0x10004 || u == 0x10005) { is_pad = true; if (pad_rid < 0) pad_rid = rid; }
            } else if (tag == 8 && is_pad && (pad_rid < 0 || rid == pad_rid || !ids)) {   /* Input */
                bool cnst = uv & 1, var = uv & 2, rel = uv & 4;
                i32 mx = lmax < lmin ? (i32)lmax_u : lmax;
                for (u32 k = 0; k < rcount && k < 64; k++) {
                    u32 u = 0;
                    if (nus) u = usages[k < (u32)nus ? k : (u32)nus - 1];
                    else if (range) u = umin + k <= umax ? umin + k : umax;
                    if (!cnst && var) {
                        hfield_t f = {true, off[rid], (u16)rsize, lmin, mx, !rel};
                        if (u >= 0x10030 && u <= 0x10035 && !h->gax[u - 0x10030].have) h->gax[u - 0x10030] = f;
                        else if (u == 0x20C4 && !h->gax[GA_ACCEL].have) h->gax[GA_ACCEL] = f;
                        else if (u == 0x20C5 && !h->gax[GA_BRAKE].have) h->gax[GA_BRAKE] = f;
                        else if (u == 0x10039 && !h->ghat.have) h->ghat = f;
                        else if ((u >> 16) == 9 && rsize == 1 && (u & 0xFFFF) >= 1 && (u & 0xFFFF) <= 16) {
                            if (!h->gnbtn) h->gbtn_off = off[rid] - ((u & 0xFFFF) - 1);
                            if (h->gnbtn < (u & 0xFFFF)) h->gnbtn = (u8)(u & 0xFFFF);
                        }
                    }
                    off[rid] += rsize;
                }
                h->grid = rid;
            } else if (tag == 8) {
                for (u32 k = 0; k < rcount && k < 64; k++) off[rid] += rsize;
            }
            nus = 0; range = false;
        } else if (type == 1) {
            switch (tag) {
            case 0: upage = uv; break;
            case 1: lmin = sv; break;
            case 2: lmax = sv; lmax_u = uv; break;
            case 7: rsize = uv; break;
            case 8: rid = (u8)uv; ids = true; break;
            case 9: rcount = uv; break;
            }
        } else if (type == 2) {
            u32 full = sz == 4 ? uv : (upage << 16) | uv;
            if (tag == 0 && nus < 32) usages[nus++] = full;
            else if (tag == 1) { umin = full; range = true; }
            else if (tag == 2) { umax = full; range = true; }
        }
    }
    h->gids = ids;
    return is_pad && h->gax[GA_X].have && h->gax[GA_Y].have && h->gnbtn >= 2;
}
static float gaxis(const hfield_t *f, const u8 *r, u32 len, u32 base, bool sym) {
    if (!f->have || f->lmax == f->lmin) return 0;
    i32 v = getbits(r, len, base + f->off, f->size, f->lmin < 0);
    float t = (float)(v - f->lmin) / (float)(f->lmax - f->lmin);
    return sym ? t * 2 - 1 : t;
}
/* DualShock 4 / DualSense (Sony, 054c): quadrado, X, círculo, triângulo; Z/Rz = analógico direito; Rx/Ry = gatilhos */
static void gp_report(hid_t *h, const u8 *r, u32 len) {
    u32 base = 0;
    if (h->gids) { if (!len || r[0] != h->grid) return; base = 8; }
    bool sony = h->d->desc.idVendor == 0x054C;
    bool btn[17] = {0};
    for (int i = 1; i <= h->gnbtn && i <= 16; i++) btn[i] = getbits(r, len, base + h->gbtn_off + (i - 1), 1, false) != 0;
    u32 b = 0;
    if (sony) {
        static const u32 m[15] = {0, PAD_X, PAD_A, PAD_B, PAD_Y, PAD_LB, PAD_RB, 0, 0, PAD_OPTIONS, PAD_MENU, PAD_LS, PAD_RS, PAD_HOME, PAD_TOUCHPAD};
        for (int i = 1; i < 15; i++) if (btn[i]) b |= m[i];
    } else {
        /* ordem genérica (a mesma do SDL para controles sem mapeamento): A B X Y LB RB Back Start Guide LS RS */
        static const u32 m[12] = {0, PAD_A, PAD_B, PAD_X, PAD_Y, PAD_LB, PAD_RB, PAD_OPTIONS, PAD_MENU, PAD_HOME, PAD_LS, PAD_RS};
        for (int i = 1; i < 12; i++) if (btn[i]) b |= m[i];
    }
    if (h->ghat.have) {
        i32 hv = getbits(r, len, base + h->ghat.off, h->ghat.size, false) - h->ghat.lmin;
        i32 span = h->ghat.lmax - h->ghat.lmin + 1;
        b |= gamepad_hat_buttons(span == 8 ? hv : span == 4 ? hv * 2 : -1);
    }
    float lx = gaxis(&h->gax[GA_X], r, len, base, true), ly = -gaxis(&h->gax[GA_Y], r, len, base, true);
    float rx = 0, ry = 0, lt = 0, rt = 0;
    bool z = h->gax[GA_Z].have, rz = h->gax[GA_RZ].have, rxh = h->gax[GA_RX].have, ryh = h->gax[GA_RY].have;
    if (z && rz) {
        rx = gaxis(&h->gax[GA_Z], r, len, base, true); ry = -gaxis(&h->gax[GA_RZ], r, len, base, true);
        if (rxh) lt = gaxis(&h->gax[GA_RX], r, len, base, false);
        if (ryh) rt = gaxis(&h->gax[GA_RY], r, len, base, false);
    } else if (rxh && ryh) {
        rx = gaxis(&h->gax[GA_RX], r, len, base, true); ry = -gaxis(&h->gax[GA_RY], r, len, base, true);
        if (z) { float t = gaxis(&h->gax[GA_Z], r, len, base, true); if (t < 0) lt = -t; else rt = t; }
    }
    if (h->gax[GA_BRAKE].have) lt = gaxis(&h->gax[GA_BRAKE], r, len, base, false);
    if (h->gax[GA_ACCEL].have) rt = gaxis(&h->gax[GA_ACCEL], r, len, base, false);
    if (!sony && !h->gax[GA_BRAKE].have && !(z && rz && rxh)) { if (btn[7] && h->gnbtn >= 12) lt = 1; if (btn[8] && h->gnbtn >= 12) rt = 1; }
    gamepad_report(h->pad, b, lx, ly, rx, ry, lt, rt);
}
/* vibração do DualShock 4 por USB: relatório de saída 0x05 (SET_REPORT pelo endpoint de controle) */
static void ds4_rumble(void *ctx, u8 strong, u8 weak) {
    hid_t *h = ctx;
    if (!h->used || !h->active) return;
    u8 rep[32] = {0x05, 0xFF, 0x00, 0x00, weak, strong};
    usb_control(h->d, 0x21, 0x09, 0x0205, h->it->num, sizeof rep, rep);
}

/* ------------------------------------------------------------------ driver */
static void hid_cb(usb_dev_t *d, u8 ep, u8 *data, u32 len, int status, void *ctx) {
    hid_t *h = ctx;
    if (!h->used || !h->active) return;
    if (h->type == 1) kbd_report(h, data, len);
    else if (h->type == 3) gp_report(h, data, len);
    else ptr_report(h, data, len);
}

static bool hid_probe(usb_dev_t *d, usb_iface_t *it) {
    if (it->cls != 3) return false;
    int idx;
    if (!usb_find_ep(it, USB_EP_INTR, true, &idx)) return false;
    hid_t *h = NULL;
    for (int i = 0; i < MAX_HID; i++) if (!hids[i].used) { h = &hids[i]; break; }
    if (!h) return false;
    memset(h, 0, sizeof *h);
    h->used = true; h->d = d; h->it = it;
    h->ep = it->ep[idx].bEndpointAddress;
    h->mps = it->ep[idx].wMaxPacketSize & 0x7FF;
    it->priv = h;
    usb_add_ep(d, it, idx);
    return true;
}

static u16 report_desc_len(usb_iface_t *it) {
    const u8 *p = it->extra, *end = it->extra + it->extra_len;
    while (p + 2 <= end && p[0] >= 2) {
        if (p[1] == USB_DT_HID && p[0] >= 9) {
            int n = p[5];
            for (int i = 0; i < n && 6 + i * 3 + 2 < p[0]; i++)
                if (p[6 + i * 3] == USB_DT_REPORT) return p[7 + i * 3] | p[8 + i * 3] << 8;
        }
        p += p[0];
    }
    return 0;
}

static void hid_start(usb_dev_t *d, usb_iface_t *it) {
    hid_t *h = it->priv;
    if (!h) return;
    bool bootif = it->sub == 1;
    if (bootif && it->proto == 1) {
        h->type = 1;
        usb_control(d, 0x21, 0x0B, 0, it->num, 0, NULL);            /* SET_PROTOCOL(boot) */
        /* SET_IDLE(100 ms): o teclado reenvia o estado atual periodicamente enquanto há tecla pressionada,
         * o que corrige um relatório de "soltar" perdido (sem isso a tecla ficaria repetindo para sempre) */
        usb_control(d, 0x21, 0x0A, 25 << 8, it->num, 0, NULL);
        u8 leds = 0;
        if (usb_control(d, 0x21, 0x09, 0x0200, it->num, 1, &leds) < 0) h->led_fail = true;
        usb_set_kind(d, USB_KIND_KEYBOARD);
        kprintf("USB HID: teclado (boot protocol) na interface %u, ep %02x\n", it->num, h->ep);
    } else {
        u16 rl = report_desc_len(it);
        bool ok = false, gp = false;
        if (rl) {
            u8 *rd = kmalloc(MIN(rl, 4096));
            if (rd) {
                int n = usb_control(d, 0x81, 6, (USB_DT_REPORT << 8), it->num, MIN(rl, 4096), rd);
                if (n > 0) { gp = parse_gamepad(h, rd, n); if (!gp) ok = parse_report(h, rd, n); }   /* controle antes: ele também tem X/Y */
                kfree(rd);
            }
        }
        if (gp) {
            h->type = 3;
            usb_control(d, 0x21, 0x0A, 0, it->num, 0, NULL);          /* SET_IDLE(0): relatório só quando muda */
            usb_set_kind(d, USB_KIND_HID);
            bool sony = d->desc.idVendor == 0x054C;
            h->pad = gamepad_attach(d->product[0] ? d->product : "Controle de jogo USB", d->desc.idVendor, d->desc.idProduct,
                                    sony ? PAD_KIND_SONY : PAD_KIND_HID, sony ? PAD_CAP_RUMBLE : 0,
                                    sony && (d->desc.idProduct == 0x05C4 || d->desc.idProduct == 0x09CC) ? ds4_rumble : NULL, h);
            kprintf("USB HID: controle de jogo na interface %u (%u botões%s)\n", it->num, h->gnbtn, h->ghat.have ? ", direcional" : "");
        } else if (ok) {
            h->type = 2;
            if (bootif) usb_control(d, 0x21, 0x0B, 1, it->num, 0, NULL);   /* SET_PROTOCOL(report) */
            usb_control(d, 0x21, 0x0A, 0, it->num, 0, NULL);
            usb_set_kind(d, h->abs ? USB_KIND_TABLET : USB_KIND_MOUSE);
            kprintf("USB HID: apontador %s na interface %u (id %u, X %u bits @%u, Y %u bits @%u, %u botões%s)\n",
                    h->abs ? "absoluto" : "relativo", it->num, h->rid, h->x.size, h->x.off, h->y.size, h->y.off,
                    h->nbtn, h->wheel.have ? ", roda" : "");
        } else if (bootif && it->proto == 2) {
            h->type = 2; h->boot = true;
            usb_control(d, 0x21, 0x0B, 0, it->num, 0, NULL);
            usb_control(d, 0x21, 0x0A, 0, it->num, 0, NULL);
            usb_set_kind(d, USB_KIND_MOUSE);
            kprintf("USB HID: mouse (boot protocol) na interface %u\n", it->num);
        } else {
            if (d->kind == USB_KIND_UNKNOWN) usb_set_kind(d, USB_KIND_HID);
            kprintf("USB HID: interface %u sem teclado/apontador reconhecido — ignorada\n", it->num);
            return;
        }
    }
    h->active = true;
    u16 len = h->type == 1 ? MAX(h->mps, 8) : MAX(h->mps, 4);
    if (usb_intr_start(d, h->ep, len, hid_cb, h) < 0) { kprintf("USB HID: falha ao iniciar interrupt IN\n"); h->active = false; }
}

static void hid_detach(usb_dev_t *d, usb_iface_t *it) {
    hid_t *h = it->priv;
    if (!h) return;
    if (h->type == 1) kbd_release_all(h);
    if (h->type == 3) gamepad_detach(h->pad);
    if (h->type == 2 && h->last_buttons) {
        input_event_t ev = {0};
        ev.type = EV_MOUSE;
        input_push(&ev);
    }
    memset(h, 0, sizeof *h);
    it->priv = NULL;
}

static void hid_poll(void) {
    u64 now = timer_ticks();
    u8 want = (input_mods() & MOD_CAPS) ? 2 : 0;
    for (int i = 0; i < MAX_HID; i++) {
        hid_t *h = &hids[i];
        if (!h->used || !h->active || h->type != 1) continue;
        /* repetição: 500 ms de atraso, depois a cada 33 ms (recupera até 4 se o laço atrasou) */
        /* sem relatórios há 600 ms com repetição ativa: o "soltar" se perdeu (o idle reenvia a cada 100 ms) */
        if (h->rep_usage && h->last_rx && now - h->last_rx > 600) {
            input_kbd_scancode(hid2sc[h->rep_usage], true);
            h->rep_usage = 0;
        }
        for (int n = 0; h->rep_usage && now >= h->rep_next && n < 4; n++) {
            input_kbd_scancode(hid2sc[h->rep_usage], false);
            h->rep_next += 33;
        }
        if (h->rep_usage && now >= h->rep_next) h->rep_next = now + 33;
        if (!h->led_fail && h->leds != want) {
            h->leds = want;
            if (usb_control(h->d, 0x21, 0x09, 0x0200, h->it->num, 1, &want) < 0) h->led_fail = true;
        }
    }
}

const usb_driver_t usb_hid_driver = { "hid", hid_probe, hid_start, hid_detach, hid_poll };
