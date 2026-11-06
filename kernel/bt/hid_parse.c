/* mOS — interpretador de descritores de relatório HID (só o necessário para mouse e teclado) */
#include "../include/hid.h"

typedef struct {
    int page, size, count, report_id;
    int usages[16], nusages, umin, umax;
    int bit[256];                       /* posição corrente por ID de relatório */
} st_t;

static u32 item_u(const u8 *p, int n) { u32 v = 0; for (int i = 0; i < n; i++) v |= (u32)p[i] << (8 * i); return v; }

typedef void (*field_cb)(void *ctx, st_t *s, int usage_page, int usage, int bit, int size);

static bool walk(const u8 *d, int len, field_cb cb, void *ctx) {
    st_t s;
    memset(&s, 0, sizeof s);
    s.umin = s.umax = -1;
    for (int i = 0; i < len;) {
        u8 pre = d[i];
        if (pre == 0xFE) { if (i + 2 >= len) break; i += 3 + d[i + 1]; continue; }   /* item longo */
        int n = pre & 3;
        if (n == 3) n = 4;
        if (i + 1 + n > len) return false;
        u32 v = item_u(d + i + 1, n);
        int tag = pre & 0xFC;
        switch (tag) {
        case 0x04: s.page = (int)v; break;                                   /* Usage Page */
        case 0x74: s.size = (int)v; break;                                   /* Report Size */
        case 0x94: s.count = (int)v; break;                                  /* Report Count */
        case 0x84: s.report_id = (int)(v & 0xFF); break;                     /* Report ID */
        case 0x08: if (s.nusages < 16) s.usages[s.nusages++] = (int)(n == 4 ? v : (v | (u32)s.page << 16)); break;
        case 0x18: s.umin = (int)v; break;
        case 0x28: s.umax = (int)v; break;
        case 0x80: {                                                          /* Input */
            int *bit = &s.bit[s.report_id & 255];
            bool constant = v & 1;
            for (int k = 0; k < s.count; k++) {
                int usage = -1, page = s.page;
                if (!constant) {
                    if (s.nusages) {
                        int u = s.usages[MIN(k, s.nusages - 1)];
                        page = u >> 16 ? u >> 16 : s.page;
                        usage = u & 0xFFFF;
                    } else if (s.umin >= 0) usage = s.umin + k;
                }
                if (usage >= 0) cb(ctx, &s, page, usage, *bit, s.size);
                *bit += s.size;
            }
            s.nusages = 0; s.umin = s.umax = -1;
            break;
        }
        case 0x90: case 0xB0: s.nusages = 0; s.umin = s.umax = -1; break;  /* Output / Feature */
        case 0xA0: case 0xC0: s.nusages = 0; s.umin = s.umax = -1; break;  /* Collection */
        default: break;
        }
        i += 1 + n;
    }
    return true;
}

static void mouse_cb(void *ctx, st_t *s, int page, int usage, int bit, int size) {
    hid_mouse_t *m = ctx;
    if (m->x_bit >= 0 && m->report_id != s->report_id && !(page == 9 && m->buttons_count == 0)) return;   /* fica no 1º relatório com X */
    if (page == 9 && usage >= 1) {
        if (!m->buttons_count) { m->buttons_bit = bit; m->report_id = s->report_id; }
        if (m->report_id == s->report_id) m->buttons_count++;
    } else if (page == 1 && usage == 0x30 && m->x_bit < 0) { m->x_bit = bit; m->x_size = size; m->report_id = s->report_id; }
    else if (page == 1 && usage == 0x31 && m->y_bit < 0) { m->y_bit = bit; m->y_size = size; }
    else if (page == 1 && usage == 0x38 && m->wheel_bit < 0) { m->wheel_bit = bit; m->wheel_size = size; }
    else if (page == 0x0C && usage == 0x238 && m->hwheel_bit < 0) { m->hwheel_bit = bit; m->hwheel_size = size; }
}

bool hid_parse_mouse(const u8 *desc, int len, hid_mouse_t *m) {
    memset(m, 0, sizeof *m);
    m->x_bit = m->y_bit = m->wheel_bit = m->hwheel_bit = -1;
    if (!walk(desc, len, mouse_cb, m)) return false;
    return m->x_bit >= 0 && m->y_bit >= 0;
}

static void kbd_cb(void *ctx, st_t *s, int page, int usage, int bit, int size) {
    hid_kbd_t *k = ctx;
    if (page != 7) return;
    if (usage >= 0xE0 && usage <= 0xE7) { if (k->mods_bit < 0) { k->mods_bit = bit - (usage - 0xE0) * size; k->report_id = s->report_id; } }
    else if (size == 8 && k->keys_bit < 0) { k->keys_bit = bit; k->keys_count = s->count; }
}

bool hid_parse_keyboard(const u8 *desc, int len, hid_kbd_t *k) {
    memset(k, 0, sizeof *k);
    k->mods_bit = k->keys_bit = -1;
    if (!walk(desc, len, kbd_cb, k)) return false;
    return k->keys_bit >= 0;
}

static int bits(const u8 *r, int len, int at, int n, bool sign) {
    if (at < 0 || n <= 0 || at + n > len * 8) return 0;
    u32 v = 0;
    for (int i = 0; i < n; i++) if (r[(at + i) / 8] >> ((at + i) % 8) & 1) v |= 1u << i;
    if (sign && n < 32 && (v >> (n - 1) & 1)) v |= ~0u << n;
    return (int)v;
}

void hid_mouse_decode(const hid_mouse_t *m, const u8 *rep, int len, int *buttons, int *dx, int *dy, int *dz) {
    *buttons = bits(rep, len, m->buttons_bit, MIN(m->buttons_count, 8), false);
    *dx = bits(rep, len, m->x_bit, m->x_size, true);
    *dy = bits(rep, len, m->y_bit, m->y_size, true);
    *dz = m->wheel_bit >= 0 ? bits(rep, len, m->wheel_bit, m->wheel_size, true) : 0;
}
