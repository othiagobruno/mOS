/* mOS — núcleo do menu flutuante (ver appkit_menupop_core.h). Teste de host: -DCX_MP_HOST. */
#ifdef CX_MP_HOST
#include <string.h>
#else
#include "sys.h"
#include "libc.h"
#endif
#include "appkit_menupop_core.h"

void mp_defaults(mp_menu_t *m) {
    m->row_h = 22; m->sep_h = 11; m->head_h = 22; m->pad_v = 5;
    m->left = 22;        /* coluna da marca (✓) */
    m->right_min = 18;   /* espaço da seta do submenu */
    m->gap_key = 24;
}

void mp_layout(mp_menu_t *m, double min_w) {
    double y = m->pad_v, wmax = 0, kmax = 0;
    int n = m->n > 512 ? 512 : m->n;
    for (int i = 0; i < n; i++) {
        mp_item_t *it = &m->it[i];
        double h = it->hidden ? 0 : it->kind == MP_SEP ? m->sep_h : it->kind == MP_HEADER ? m->head_h : m->row_h;
        m->y[i] = y; m->ih[i] = h;
        y += h;
        if (it->hidden || it->kind == MP_SEP) continue;
        double tw = it->title_w + it->indent * 10.0;
        if (tw > wmax) wmax = tw;
        if (it->key_w > kmax) kmax = it->key_w;
    }
    double w = m->left + wmax + (kmax > 0 ? m->gap_key + kmax : 0) + m->right_min + 6;
    if (w < min_w) w = min_w;
    m->w = w;
    m->h = y + m->pad_v;
}

int mp_selectable(const mp_menu_t *m, int i) {
    if (i < 0 || i >= m->n) return 0;
    const mp_item_t *it = &m->it[i];
    return it->kind == MP_ITEM && !it->hidden && it->enabled;
}

int mp_hit(const mp_menu_t *m, double x, double y) {
    if (x < 0 || x >= m->w) return -1;
    for (int i = 0; i < m->n && i < 512; i++) {
        if (m->ih[i] <= 0) continue;
        if (y >= m->y[i] && y < m->y[i] + m->ih[i]) return m->it[i].kind == MP_ITEM ? i : -1;
    }
    return -1;
}

int mp_step(const mp_menu_t *m, int from, int dir) {
    int i = from;
    if (from < 0) i = dir > 0 ? -1 : m->n;
    for (i += dir; i >= 0 && i < m->n; i += dir) if (mp_selectable(m, i)) return i;
    return from >= 0 && mp_selectable(m, from) ? from : -1;
}

static char lower(char c) { return c >= 'A' && c <= 'Z' ? (char)(c + 32) : c; }

int mp_type_select(const mp_menu_t *m, int from, char c) {
    c = lower(c);
    if (!c || m->n <= 0) return -1;
    for (int k = 1; k <= m->n; k++) {
        int i = ((from < 0 ? -1 : from) + k) % m->n;
        if (i < 0) i += m->n;
        if (mp_selectable(m, i) && m->it[i].first == c) return i;
    }
    return -1;
}

static double clampd(double v, double lo, double hi) { if (hi < lo) return lo; return v < lo ? lo : v > hi ? hi : v; }

mp_rect mp_place(const mp_menu_t *m, int pos, double x, double y, mp_rect s, int mode) {
    mp_rect r = {0, 0, m->w, m->h};
    double maxh = s.h - 8;
    if (r.h > maxh) r.h = maxh;
    if (mode == MP_PLACE_CONTEXT) {
        r.x = x;
        if (r.x + r.w > s.x + s.w - 4) r.x = x - r.w;                /* não cabe à direita: abre à esquerda */
        r.x = clampd(r.x, s.x + 4, s.x + s.w - 4 - r.w);
        r.y = y - r.h;                                                 /* topo no ponto */
        if (r.y < s.y + 4) r.y = y;                                    /* não cabe embaixo: abre para cima */
        r.y = clampd(r.y, s.y + 4, s.y + s.h - 4 - r.h);
        return r;
    }
    double off = (pos >= 0 && pos < m->n && pos < 512) ? m->y[pos] : m->pad_v;
    r.x = clampd(x - m->left, s.x + 4, s.x + s.w - 4 - r.w);         /* o texto do item fica alinhado com x */
    r.y = y + off - r.h;                                               /* topo do item em y */
    r.y = clampd(r.y, s.y + 4, s.y + s.h - 4 - r.h);
    return r;
}

mp_rect mp_place_sub(const mp_menu_t *sub, mp_rect p, double item_top, mp_rect s) {
    mp_rect r = {p.x + p.w - 2, 0, sub->w, sub->h};
    if (r.h > s.h - 8) r.h = s.h - 8;
    if (r.x + r.w > s.x + s.w - 4) r.x = p.x - r.w + 2;
    r.x = clampd(r.x, s.x + 4, s.x + s.w - 4 - r.w);
    double top = p.y + p.h - item_top + sub->pad_v;                   /* 1º item do submenu na altura do item-pai */
    r.y = clampd(top - r.h, s.y + 4, s.y + s.h - 4 - r.h);
    return r;
}

static int put(char *o, int n, int cap, const char *s) {
    int l = (int)strlen(s);
    if (n + l >= cap) return n;
    memcpy(o + n, s, (size_t)l + 1);
    return n + l;
}

void mp_key_label(const char *key, unsigned long long mask, char *out, int cap) {
    int n = 0;
    out[0] = 0;
    if (!key || !*key || cap < 2) return;
    unsigned char k = (unsigned char)key[0];
    int upper_letter = k >= 'A' && k <= 'Z';
    if (mask & (1ull << 18)) n = put(out, n, cap, "\xE2\x8C\x83");             /* ⌃ */
    if (mask & (1ull << 19)) n = put(out, n, cap, "\xE2\x8C\xA5");             /* ⌥ */
    if ((mask & (1ull << 17)) || upper_letter) n = put(out, n, cap, "\xE2\x87\xA7"); /* ⇧ (letra maiúscula = Shift) */
    if (mask & (1ull << 20)) n = put(out, n, cap, "\xE2\x8C\x98");             /* ⌘ */
    const char *sym = 0;
    switch (k) {
    case '\r': case 3: sym = "\xE2\x86\xA9"; break;          /* ↩ */
    case '\t': sym = "\xE2\x87\xA5"; break;                   /* ⇥ */
    case 27: sym = "\xE2\x8E\x8B"; break;                     /* ⎋ */
    case 8: case 127: sym = "\xE2\x8C\xAB"; break;            /* ⌫ */
    case ' ': sym = "Espa\xC3\xA7o"; break;
    }
    /* teclas de função do NSEvent (U+F700..): setas, F1.., Delete para frente */
    if (!sym && k == 0xEF && (unsigned char)key[1] == 0x9C) {
        unsigned c = (unsigned char)key[2];
        switch (c) {
        case 0x80: sym = "\xE2\x86\x91"; break; case 0x81: sym = "\xE2\x86\x93"; break;
        case 0x82: sym = "\xE2\x86\x90"; break; case 0x83: sym = "\xE2\x86\x92"; break;
        case 0xA8: sym = "\xE2\x8C\xA6"; break;               /* ⌦ (U+F728) */
        case 0xA9: sym = "\xE2\x86\x96"; break; case 0xAB: sym = "\xE2\x86\x98"; break; /* Home, End */
        case 0xAC: sym = "\xE2\x87\x9E"; break; case 0xAD: sym = "\xE2\x87\x9F"; break; /* PgUp, PgDn */
        default:
            if (c >= 0x84 && c <= 0x9A) {                     /* F1..F23 */
                char f[4] = {'F', 0, 0, 0};
                int v = (int)c - 0x84 + 1;
                if (v >= 10) { f[1] = (char)('0' + v / 10); f[2] = (char)('0' + v % 10); } else f[1] = (char)('0' + v);
                n = put(out, n, cap, f);
                return;
            }
        }
    }
    if (sym) { put(out, n, cap, sym); return; }
    char one[8];
    int l = 1;
    if (k >= 0x80) { l = (k & 0xE0) == 0xC0 ? 2 : (k & 0xF0) == 0xE0 ? 3 : 4; }
    for (int i = 0; i < l && key[i]; i++) one[i] = key[i];
    one[l] = 0;
    if (one[0] >= 'a' && one[0] <= 'z') one[0] = (char)(one[0] - 32);
    put(out, n, cap, one);
}
