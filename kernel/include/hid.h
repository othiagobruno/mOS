/* mOS — interpretador de descritores HID (relatórios de mouse e teclado), usado pelo HID sobre
 * Bluetooth LE (HOGP). Posições em bits contadas a partir do início do relatório SEM o byte de ID. */
#pragma once
#ifdef MOS_HOST_TEST
#include "host_shim.h"
#else
#include "kernel.h"
#endif

typedef struct {
    int report_id;                      /* 0 = sem ID */
    int buttons_bit, buttons_count;
    int x_bit, x_size, y_bit, y_size;   /* relativos, com sinal */
    int wheel_bit, wheel_size;          /* -1 se não houver */
    int hwheel_bit, hwheel_size;
    int report_bits;
} hid_mouse_t;

typedef struct {
    int report_id;
    int mods_bit;                       /* 8 bits de modificadores (0xE0..0xE7) */
    int keys_bit, keys_count;           /* vetor de códigos de tecla (8 bits cada) */
} hid_kbd_t;

bool hid_parse_mouse(const u8 *desc, int len, hid_mouse_t *m);
bool hid_parse_keyboard(const u8 *desc, int len, hid_kbd_t *k);
void hid_mouse_decode(const hid_mouse_t *m, const u8 *rep, int len, int *buttons, int *dx, int *dy, int *dz);
