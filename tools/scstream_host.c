/* Teste de host da captura contínua: o quadro muda quando a cor da janela muda,
 * e sem permissão o buffer não é escrito. Não liga o kernel. */
#include <stdio.h>
#include <string.h>
#include "../userland/libsystem/screencap_core.inc"

static int allow = 1;
static int reads;
static unsigned color = 0xFF112233u;

int cx_cap_allowed(void) { return allow; }
int cx_cap_read(int x, int y, int w, int h, unsigned *dst, int dst_px, int *ow, int *oh) {
    (void)x; (void)y;
    reads++;
    int ww = w > 0 ? w : 8;
    int hh = h > 0 ? h : 8;
    if (ww * hh > dst_px) return 0;
    for (int i = 0; i < ww * hh; i++) dst[i] = color;
    if (ow) *ow = ww;
    if (oh) *oh = hh;
    return 1;
}

static int fail(int n) { fprintf(stderr, "scstream host falhou no passo %d\n", n); return n; }

int main(void) {
    cx_cap_t c;
    memset(&c, 0, sizeof c);
    unsigned buf[64];
    for (int i = 0; i < 64; i++) buf[i] = 0xA5A5A5A5u;
    c.px = buf;
    c.cap = 64;
    allow = 0;
    int before = reads;
    if (cx_cap_take(&c, 1000000, 0, 0, 0, 0) != -1) return fail(1);
    if (reads != before) return fail(2);
    if (buf[0] != 0xA5A5A5A5u) return fail(3);
    allow = 1;
    color = 0xFF112233u;
    if (cx_cap_take(&c, 1000000, 0, 0, 0, 0) != 1) return fail(4);
    if (buf[0] != color || c.w != 8 || c.h != 8) return fail(5);
    unsigned first = buf[0];
    if (cx_cap_take(&c, 1000000 + 1000, 0, 0, 0, 0) != 0) return fail(6);
    if (reads == before + 1) { /* a segunda leitura não pode ter acontecido */ }
    else return fail(7);
    color = 0xFF99AABBu;
    if (cx_cap_take(&c, 1000000 + 16667, 0, 0, 0, 0) != 1) return fail(8);
    if (buf[0] != color || buf[0] == first) return fail(9);
    c.interval_us = 50000;
    c.next_us = 0;
    color = 0xFF010101u;
    if (cx_cap_take(&c, 0, 0, 0, 8, 8) != 1) return fail(10);
    if (cx_cap_take(&c, 49999, 0, 0, 8, 8) != 0) return fail(11);
    color = 0xFF020202u;
    if (cx_cap_take(&c, 50000, 0, 0, 8, 8) != 1) return fail(12);
    if (buf[63] != color) return fail(13);
    printf("scstream host ok\n");
    return 0;
}
