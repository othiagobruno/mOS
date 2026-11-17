/* teste de host do icon_core (leitura de .icns)
 * gcc -std=c11 -Wall -Wextra -Wno-multichar -Iuserland/libsystem userland/libsystem/icon_core.c userland/tests-appkit/icon_host.c -o /tmp/icon-test && /tmp/icon-test */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "icon_core.h"
static int ok, total;
static void check(int c, const char *m) { total++; if (c) ok++; printf("%s %s\n", c ? "  ok  " : "FALHOU", m); }
static long put(uint8_t *f, long p, const char *t, const uint8_t *d, long n) {
    memcpy(f + p, t, 4);
    long el = n + 8;
    f[p + 4] = (uint8_t)(el >> 24); f[p + 5] = (uint8_t)(el >> 16); f[p + 6] = (uint8_t)(el >> 8); f[p + 7] = (uint8_t)el;
    memcpy(f + p + 8, d, (size_t)n);
    return p + el;
}
static void hdr(uint8_t *f, long total_) { memcpy(f, "icns", 4); f[4] = (uint8_t)(total_ >> 24); f[5] = (uint8_t)(total_ >> 16); f[6] = (uint8_t)(total_ >> 8); f[7] = (uint8_t)total_; }
int main(void) {
    static uint8_t f[8192], scratch[4 * 128 * 128];
    static uint32_t px[128 * 128];
    /* is32: 16×16, cada canal = uma corrida de 256 bytes (2 corridas de 128: 0xFD + valor = 128 cópias) */
    uint8_t rle[12] = {0xFD, 10, 0xFD, 10, 0xFD, 20, 0xFD, 20, 0xFD, 30, 0xFD, 30};
    uint8_t mask[256];
    memset(mask, 200, sizeof mask);
    uint8_t png[16] = {0x89, 'P', 'N', 'G', 13, 10, 26, 10, 0, 0, 0, 13, 'I', 'H', 'D', 'R'};
    long p = 8;
    p = put(f, p, "is32", rle, sizeof rle);
    p = put(f, p, "s8mk", mask, sizeof mask);
    p = put(f, p, "ic07", png, sizeof png);
    p = put(f, p, "ic09", (const uint8_t *)"\0\0\0\x0cjP  ", 8);   /* JPEG 2000: ignorado */
    hdr(f, p);
    check(ic_is_icns(f, p) && !ic_is_icns((const uint8_t *)"abcdefgh", 8), "reconhece icns");
    ic_pick k;
    check(ic_pick_best(f, p, 16, &k) && k.kind == IC_RLE && k.side == 16 && k.mask_len == 256, "16 px: o is32 com máscara");
    check(ic_pick_best(f, p, 64, &k) && k.kind == IC_PNG && k.side == 128, "64 px: o PNG de 128 (JPEG 2000 fora)");
    check(ic_pick_best(f, p, 1024, &k) && k.kind == IC_PNG && k.side == 128, "maior que tudo: o maior legível");
    ic_pick_best(f, p, 16, &k);
    check(ic_decode_rle(f + k.off, k.len, 16, 0, f + k.mask_off, k.mask_len, px, scratch) && px[0] == 0xC80A141Eu && px[255] == 0xC80A141Eu, "RLE + máscara");
    uint8_t argb[4 + 8] = {'A', 'R', 'G', 'B', 0x80, 0xFF, 0x80, 1, 0x80, 2, 0x80, 3};   /* 1×1: corridas de 3? não: literais de 1 */
    argb[4] = 0x00; argb[5] = 0xFF; argb[6] = 0x00; argb[7] = 0x11; argb[8] = 0x00; argb[9] = 0x22; argb[10] = 0x00; argb[11] = 0x33;
    check(ic_decode_argb(argb, 12, 1, px, scratch) && px[0] == 0xFF112233u, "ARGB compactado");
    check(!ic_decode_rle((const uint8_t *)"\xFD", 1, 16, 0, 0, 0, px, scratch), "RLE truncado falha");
    printf("%d/%d\n", ok, total);
    return ok == total ? 0 : 1;
}
