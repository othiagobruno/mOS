/* Teste de host do userland/libsystem/sck_core.c. */
#include <stdio.h>
#include <string.h>
#include "../userland/libsystem/sck_core.h"
static int fails;
#define CHECK(c) do { if (!(c)) { printf("FALHOU %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
int main(void) {
    sck_win w[5] = {{10, 100}, {11, 100}, {12, 200}, {13, 300}, {14, 200}};
    uint32_t ids[SCK_MAX]; int inc = -1;
    sck_filter f = {0};
    f.kind = SCK_DISPLAY_EXCL_WINDOWS; f.nwin = 2; f.win[0] = 11; f.win[1] = 13;
    CHECK(sck_resolve(&f, w, 5, ids, &inc) == 2 && inc == 0 && ids[0] == 11 && ids[1] == 13);
    f.kind = SCK_DISPLAY_INCL_WINDOWS;
    CHECK(sck_resolve(&f, w, 5, ids, &inc) == 2 && inc == 1);
    memset(&f, 0, sizeof f);
    f.kind = SCK_DISPLAY_EXCL_APPS; f.npid = 1; f.pid[0] = 200; f.nwin = 1; f.win[0] = 14;
    CHECK(sck_resolve(&f, w, 5, ids, &inc) == 1 && inc == 0 && ids[0] == 12);   /* a 14 é exceção: continua */
    f.kind = SCK_DISPLAY_INCL_APPS; f.pid[1] = 100; f.npid = 2;
    CHECK(sck_resolve(&f, w, 5, ids, &inc) == 3 && inc == 1 && ids[0] == 10 && ids[1] == 11 && ids[2] == 12);
    f.kind = SCK_WINDOW; f.window = 77;
    CHECK(sck_resolve(&f, w, 5, ids, &inc) == 1 && inc == 1 && ids[0] == 77);

    sck_rect z = {0, 0, 0, 0};
    sck_rect r = sck_layout(1920, 1080, 1280, 1024, 1, 1, z);
    CHECK(r.w > 1279.9 && r.w < 1280.1 && r.h > 719.9 && r.h < 720.1 && r.y > 151.9 && r.y < 152.1);   /* letterbox */
    r = sck_layout(1920, 1080, 1280, 1024, 1, 0, z);
    CHECK(r.w == 1280 && r.h == 1024 && r.x == 0);
    r = sck_layout(100, 50, 1280, 720, 0, 1, (sck_rect){10, 20, 0, 0});
    CHECK(r.x == 0 && r.w == 100 && r.h == 50);   /* destino vazio: a saída toda; sem escala, tamanho original */
    r = sck_layout(100, 50, 1280, 720, 0, 1, (sck_rect){10, 20, 300, 300});
    CHECK(r.x == 10 && r.y == 20 && r.w == 100);

    /* cópia 1:1 e redução 2:1 com média */
    uint32_t src[4 * 2] = {0xFF000000, 0xFFFFFFFF, 0xFF000000, 0xFFFFFFFF, 0xFF000000, 0xFFFFFFFF, 0xFF000000, 0xFFFFFFFF};
    uint32_t dst[6 * 4];
    sck_place(src, 4, 2, 4, dst, 6, 4, (sck_rect){1, 1, 4, 2}, 0xFF0000FF);
    CHECK(dst[0] == 0xFF0000FF && dst[6 + 1] == 0xFF000000 && dst[6 + 2] == 0xFFFFFFFF && dst[2 * 6 + 4] == 0xFFFFFFFF && dst[3 * 6 + 5] == 0xFF0000FF);
    uint32_t half[2];
    sck_place(src, 4, 2, 4, half, 2, 1, (sck_rect){0, 0, 2, 1}, 0);
    int g = half[0] & 255;
    CHECK(g > 110 && g < 145 && (half[0] >> 24) == 255);   /* preto e branco viram cinza */
    /* ampliação de um pixel só: cor igual */
    uint32_t one = 0xFF336699, big[9];
    sck_place(&one, 1, 1, 1, big, 3, 3, (sck_rect){0, 0, 3, 3}, 0);
    CHECK(big[0] == one && big[4] == one && big[8] == one);

    uint32_t px[2] = {0xFFFF8000, 0x00000000}, o[2];
    sck_to_l10r(px, 2, 1, o);
    CHECK(o[0] == (3u << 30 | 1023u << 20 | 514u << 10 | 0) && o[1] == 0);
    CHECK(sck_hash(src, 8) == sck_hash(src, 8) && sck_hash(src, 8) != sck_hash(src, 7));
    printf(fails ? "sck_core: %d falhas\n" : "sck_core: ok\n", fails);
    return fails != 0;
}
