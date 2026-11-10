/* Composição no sentido do NSCompositingOperation (AppKit).
 * Pixels 0xAARRGGBB pré-multiplicados, o formato das superfícies do kernel.
 * O source-over usa a mesma conta de blend_px em kernel/gfx/gfx.c (o blit opaco do window server).
 * Os outros modos são os de Porter-Duff mais PlusDarker/PlusLighter e os blends mais usados.
 * Um op desconhecido cai em source-over: um app que manda um número novo não fura o fundo. */
#pragma once

#ifdef MOS_SB_HOST
#ifndef MOS_SB_U32
#define MOS_SB_U32
typedef unsigned int u32;
#endif
#endif

enum {
    GUI_COMP_CLEAR = 0,
    GUI_COMP_COPY = 1,
    GUI_COMP_SOURCE_OVER = 2,
    GUI_COMP_SOURCE_IN = 3,
    GUI_COMP_SOURCE_OUT = 4,
    GUI_COMP_SOURCE_ATOP = 5,
    GUI_COMP_DEST_OVER = 6,
    GUI_COMP_DEST_IN = 7,
    GUI_COMP_DEST_OUT = 8,
    GUI_COMP_DEST_ATOP = 9,
    GUI_COMP_XOR = 10,
    GUI_COMP_PLUS_DARKER = 11,
    GUI_COMP_HIGHLIGHT = 12, /* obsoleto no AppKit: trata como source-over */
    GUI_COMP_PLUS_LIGHTER = 13,
    GUI_COMP_MULTIPLY = 14,
    GUI_COMP_SCREEN = 15,
    GUI_COMP_OVERLAY = 16,
    GUI_COMP_DARKEN = 17,
    GUI_COMP_LIGHTEN = 18,
    GUI_COMP_DIFFERENCE = 23
};

static inline u32 gui_div255(u32 v) { return (v + 1 + (v >> 8)) >> 8; }

static inline u32 gui_premul(u32 c) {
    u32 a = c >> 24;
    if (a >= 255) return c;
    if (!a) return 0;
    return (a << 24) | (gui_div255(((c >> 16) & 255) * a) << 16) | (gui_div255(((c >> 8) & 255) * a) << 8) | gui_div255((c & 255) * a);
}

/* multiplica os 4 canais por a/255, com o mesmo arredondamento de scale_px */
static inline u32 gui_mul_px(u32 p, u32 a) {
    u32 rb, ag;
    if (a >= 255) return p;
    if (!a) return 0;
    rb = (p & 0x00FF00FFu) * a + 0x00800080u;
    rb = ((rb + ((rb >> 8) & 0x00FF00FFu)) >> 8) & 0x00FF00FFu;
    ag = ((p >> 8) & 0x00FF00FFu) * a + 0x00800080u;
    ag = (ag + ((ag >> 8) & 0x00FF00FFu)) & 0xFF00FF00u;
    return rb | ag;
}

/* src sobre dst. Idêntico a blend_px. */
static inline u32 gui_over(u32 d, u32 s) {
    u32 a = s >> 24, ia, rb, ag;
    if (a >= 255) return s;
    if (!a) return d;
    ia = 255 - a;
    rb = (d & 0x00FF00FFu) * ia + 0x00800080u;
    rb = ((rb + ((rb >> 8) & 0x00FF00FFu)) >> 8) & 0x00FF00FFu;
    ag = ((d >> 8) & 0x00FF00FFu) * ia + 0x00800080u;
    ag = (ag + ((ag >> 8) & 0x00FF00FFu)) & 0xFF00FF00u;
    return s + (rb | ag);
}

static inline u32 gui_sat_add(u32 a, u32 b) {
    u32 o = 0;
    int sh;
    for (sh = 0; sh < 32; sh += 8) {
        u32 s = ((a >> sh) & 255) + ((b >> sh) & 255);
        if (s > 255) s = 255;
        o |= s << sh;
    }
    return o;
}

static inline u32 gui_ch(u32 p, int i) { return (p >> (i * 8)) & 255; }

static inline u32 gui_pack(u32 a, u32 r, u32 g, u32 b) { return (a << 24) | (r << 16) | (g << 8) | b; }

static inline u32 gui_clamp_ch(int v) { return v < 0 ? 0 : v > 255 ? 255u : (u32)v; }

/* plusDarker: cada canal pré-multiplicado vai para max(0, s + d - 255) */
static inline u32 gui_plus_darker(u32 d, u32 s) {
    return gui_pack(gui_clamp_ch((int)gui_ch(s, 3) + (int)gui_ch(d, 3) - 255),
                    gui_clamp_ch((int)gui_ch(s, 2) + (int)gui_ch(d, 2) - 255),
                    gui_clamp_ch((int)gui_ch(s, 1) + (int)gui_ch(d, 1) - 255),
                    gui_clamp_ch((int)gui_ch(s, 0) + (int)gui_ch(d, 0) - 255));
}

/* blends SVG em pré-multiplicado: cr = f(cs,cd)*as*ad + cs*as*(1-ad) + cd*ad*(1-as) */
static inline u32 gui_blend_mode(u32 d, u32 s, int mode) {
    u32 sa = gui_ch(s, 3), da = gui_ch(d, 3), out[4];
    int i;
    out[3] = sa + da - gui_div255(sa * da);
    for (i = 0; i < 3; i++) {
        u32 sc = gui_ch(s, i), dc = gui_ch(d, i);
        u32 cs = sa ? (sc * 255) / sa : 0;
        u32 cd = da ? (dc * 255) / da : 0;
        int f = 0;
        if (mode == GUI_COMP_MULTIPLY) f = (int)((cs * cd) / 255);
        else if (mode == GUI_COMP_SCREEN) f = (int)(cs + cd - (cs * cd) / 255);
        else if (mode == GUI_COMP_DARKEN) f = (int)(cs < cd ? cs : cd);
        else if (mode == GUI_COMP_LIGHTEN) f = (int)(cs > cd ? cs : cd);
        else if (mode == GUI_COMP_DIFFERENCE) f = (int)(cs > cd ? cs - cd : cd - cs);
        else if (mode == GUI_COMP_OVERLAY) {
            f = cd < 128 ? (int)((2 * cs * cd) / 255) : (int)(255 - (2 * (255 - cs) * (255 - cd)) / 255);
        } else f = (int)cs;
        {
            /* Cout = B(Cs,Cd)·As·Ad + Cs·As·(1-Ad) + Cd·Ad·(1-As), em 8 bits pré-multiplicados */
            int prem = (f * (int)sa * (int)da) / (255 * 255);
            int tail = ((int)sc * (255 - (int)da) + (int)dc * (255 - (int)sa)) / 255;
            out[i] = gui_clamp_ch(prem + tail);
        }
    }
    return gui_pack(out[3], out[2], out[1], out[0]);
}

static inline u32 gui_composite_px(u32 d, u32 s, unsigned op) {
    switch (op) {
    case GUI_COMP_CLEAR: return 0;
    case GUI_COMP_COPY: return s;
    case GUI_COMP_SOURCE_OVER: return gui_over(d, s);
    case GUI_COMP_SOURCE_IN: return gui_mul_px(s, gui_ch(d, 3));
    case GUI_COMP_SOURCE_OUT: return gui_mul_px(s, 255 - gui_ch(d, 3));
    case GUI_COMP_SOURCE_ATOP: return gui_sat_add(gui_mul_px(s, gui_ch(d, 3)), gui_mul_px(d, 255 - gui_ch(s, 3)));
    case GUI_COMP_DEST_OVER: return gui_over(s, d);
    case GUI_COMP_DEST_IN: return gui_mul_px(d, gui_ch(s, 3));
    case GUI_COMP_DEST_OUT: return gui_mul_px(d, 255 - gui_ch(s, 3));
    case GUI_COMP_DEST_ATOP: return gui_sat_add(gui_mul_px(d, gui_ch(s, 3)), gui_mul_px(s, 255 - gui_ch(d, 3)));
    case GUI_COMP_XOR: return gui_sat_add(gui_mul_px(s, 255 - gui_ch(d, 3)), gui_mul_px(d, 255 - gui_ch(s, 3)));
    case GUI_COMP_PLUS_DARKER: return gui_plus_darker(d, s);
    case GUI_COMP_HIGHLIGHT: return gui_over(d, s);
    case GUI_COMP_PLUS_LIGHTER: return gui_sat_add(d, s);
    case GUI_COMP_MULTIPLY:
    case GUI_COMP_SCREEN:
    case GUI_COMP_OVERLAY:
    case GUI_COMP_DARKEN:
    case GUI_COMP_LIGHTEN:
    case GUI_COMP_DIFFERENCE: return gui_blend_mode(d, s, (int)op);
    default: return gui_over(d, s);
    }
}

static inline void gui_composite_span(u32 *d, const u32 *s, int n, unsigned op) {
    int i;
    for (i = 0; i < n; i++) d[i] = gui_composite_px(d[i], s[i], op);
}
