/* Modificadores do teclado (scancode set 1) -> tecla + máscara.
 * Shift, Ctrl, Option e Command, sozinhos, geram evento. O window server transforma
 * essas teclas em GEV_FLAGS, e o AppKit entrega flagsChanged: ao first responder.
 * Os números são os de kernel.h (KEY_LSHIFT …). input.c confere com _Static_assert. */
#pragma once

enum {
    KBD_KEY_LSHIFT = 0x116,
    KBD_KEY_RSHIFT = 0x117,
    KBD_KEY_LCTRL = 0x118,
    KBD_KEY_LALT = 0x119,
    KBD_KEY_LCMD = 0x11A,
    KBD_KEY_CAPS = 0x11B,
    KBD_KEY_RCTRL = 0x11C,
    KBD_KEY_RALT = 0x11D,
    KBD_KEY_RCMD = 0x11E
};

enum {
    KBD_PHYS_LSHIFT = 1,
    KBD_PHYS_RSHIFT = 2,
    KBD_PHYS_LCTRL = 4,
    KBD_PHYS_RCTRL = 8,
    KBD_PHYS_LALT = 16,
    KBD_PHYS_RALT = 32,
    KBD_PHYS_LCMD = 64,
    KBD_PHYS_RCMD = 128
};

/* 1 se a tecla é só modificador (flagsChanged, sem keyDown). */
static inline int kbd_key_is_modifier(int key) {
    return key == KBD_KEY_LSHIFT || key == KBD_KEY_RSHIFT || key == KBD_KEY_LCTRL || key == KBD_KEY_RCTRL ||
           key == KBD_KEY_LALT || key == KBD_KEY_RALT || key == KBD_KEY_LCMD || key == KBD_KEY_RCMD || key == KBD_KEY_CAPS;
}

/* 1 = emitir o evento, 2 = repetição (ignorar), 0 = não é modificador.
 * *mods fica com o estado novo (MOD_SHIFT 1, CTRL 2, ALT 4, CMD 8). *phys guarda esquerda e direita
 * separadas, para soltar um Shift não apagar o outro. */
static inline int kbd_mod_apply(unsigned short *mods, unsigned short *phys, unsigned char sc, int ext, int up, int *key) {
    unsigned short bit = 0;
    int which = 0;
    unsigned short agg = 0;
    if (sc == 0x2A) { bit = KBD_PHYS_LSHIFT; which = KBD_KEY_LSHIFT; agg = 1; }
    else if (sc == 0x36) { bit = KBD_PHYS_RSHIFT; which = KBD_KEY_RSHIFT; agg = 1; }
    else if (sc == 0x1D) {
        bit = ext ? KBD_PHYS_RCTRL : KBD_PHYS_LCTRL;
        which = ext ? KBD_KEY_RCTRL : KBD_KEY_LCTRL;
        agg = 2;
    } else if (sc == 0x38) {
        bit = ext ? KBD_PHYS_RALT : KBD_PHYS_LALT;
        which = ext ? KBD_KEY_RALT : KBD_KEY_LALT;
        agg = 4;
    } else if (ext && sc == 0x5B) { bit = KBD_PHYS_LCMD; which = KBD_KEY_LCMD; agg = 8; }
    else if (ext && sc == 0x5C) { bit = KBD_PHYS_RCMD; which = KBD_KEY_RCMD; agg = 8; }
    else return 0;
    if (!up && (*phys & bit)) return 2;
    if (up) *phys = (unsigned short)(*phys & ~bit);
    else *phys = (unsigned short)(*phys | bit);
    if (agg == 1) {
        if (*phys & (KBD_PHYS_LSHIFT | KBD_PHYS_RSHIFT)) *mods = (unsigned short)(*mods | 1);
        else *mods = (unsigned short)(*mods & ~1u);
    } else if (agg == 2) {
        if (*phys & (KBD_PHYS_LCTRL | KBD_PHYS_RCTRL)) *mods = (unsigned short)(*mods | 2);
        else *mods = (unsigned short)(*mods & ~2u);
    } else if (agg == 4) {
        if (*phys & (KBD_PHYS_LALT | KBD_PHYS_RALT)) *mods = (unsigned short)(*mods | 4);
        else *mods = (unsigned short)(*mods & ~4u);
    } else {
        if (*phys & (KBD_PHYS_LCMD | KBD_PHYS_RCMD)) *mods = (unsigned short)(*mods | 8);
        else *mods = (unsigned short)(*mods & ~8u);
    }
    *key = which;
    return 1;
}
