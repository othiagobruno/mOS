/* Teste de host da conversão de pixels do NSImage (initWithCGImage:size:). Roda no Linux, sem VM:
 *   gcc -std=c11 -Wall -Wextra -DCX_IMG_HOST -Iuserland/libsystem \
 *       userland/libsystem/appkit_image_core.c userland/tests-appkit/image_host.c \
 *       -o /tmp/appkit-image-test && /tmp/appkit-image-test */
#include "appkit_image_core.h"
#include <stdio.h>
#include <string.h>

static int ok, total;
static void check(int c, const char *w) { total++; if (c) ok++; printf("%s %s\n", c ? "  ok  " : "  FALHA", w); }

int main(void) {
    /* 2×2 com 4 bytes de sobra por linha */
    uint8_t src[2 * 12];
    memset(src, 0xEE, sizeof src);
    uint32_t px[4] = {0xFF102030, 0x80402010, 0x00000000, 0x40101010};
    memcpy(src, &px[0], 8);
    memcpy(src + 12, &px[2], 8);
    uint32_t out[4];
    cx_px_import(src, 12, 2, 2, 0, out);
    check(out[0] == 0xFF102030 && out[1] == 0x80402010 && out[2] == 0 && out[3] == 0x40101010, "copia pulando a sobra de bytesPerRow");
    cx_px_import(src, 12, 2, 2, 1, out);
    check(out[0] == 0xFF102030, "opaco não muda");
    check(out[1] == 0x80804020, "alfa 128: cor dividida pelo alfa (arredondada)");
    check(out[2] == 0, "transparente fica zero");
    check(out[3] == 0x40404040, "alfa 64");
    uint32_t sat = 0x10FFFFFF;
    cx_px_import((uint8_t *)&sat, 4, 1, 1, 1, out);
    check(out[0] == 0x10FFFFFF, "cor maior que o alfa satura em 255");
    cx_px_import(src, 0, 2, 1, 0, out);
    check(out[0] == 0xFF102030, "bytesPerRow 0 vale w*4");
    double w, h;
    cx_px_size(0, 0, 640, 480, &w, &h); check(w == 640 && h == 480, "tamanho zero = pixels");
    cx_px_size(320, 240, 640, 480, &w, &h); check(w == 320 && h == 240, "tamanho pedido (retina 2x)");
    printf("appkit-image: %d/%d ok\n", ok, total);
    return ok == total ? 0 : 1;
}
