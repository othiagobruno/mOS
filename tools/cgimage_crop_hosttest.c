/*
 * Host: CGImageCreateWithImageInRect via cgimage_crop (o mesmo recorte do libSystem).
 *   gcc -std=c11 -Wall -Wextra -O2 -o /tmp/cgimage_crop_host \
 *       tools/cgimage_crop_hosttest.c userland/libsystem/cgimage_crop.c
 *   /tmp/cgimage_crop_host
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int cgimage_crop(const uint8_t *src, uint32_t sw, uint32_t sh, uint32_t sbpr, uint32_t byte_pp,
                 double x, double y, double w, double h,
                 uint8_t **dst, uint32_t *dw, uint32_t *dh, uint32_t *dbpr);

static int falhas;
static void check(int cond, const char *msg) {
    if (!cond) { fprintf(stderr, "FALHA: %s\n", msg); falhas++; }
    else printf("ok  %s\n", msg);
}

static uint32_t px(const uint8_t *p, uint32_t bpr, uint32_t x, uint32_t y) {
    const uint8_t *s = p + (size_t)y * bpr + (size_t)x * 4;
    return (uint32_t)s[0] | (uint32_t)s[1] << 8 | (uint32_t)s[2] << 16 | (uint32_t)s[3] << 24;
}

int main(void) {
    const uint32_t W = 10, H = 8, BPR = 48; /* 40 de pixels + 8 de folga */
    uint8_t src[8 * 48];
    memset(src, 0xAB, sizeof src);
    for (uint32_t y = 0; y < H; y++)
        for (uint32_t x = 0; x < W; x++) {
            uint8_t *s = src + y * BPR + x * 4;
            s[0] = (uint8_t)x; s[1] = (uint8_t)y; s[2] = (uint8_t)(x + y); s[3] = 255;
        }
    uint8_t *d = 0;
    uint32_t dw = 0, dh = 0, dbpr = 0;
    /* (1.2, 1.8, 2.1, 2.2) → integral (1,1)–(4,4) */
    check(cgimage_crop(src, W, H, BPR, 4, 1.2, 1.8, 2.1, 2.2, &d, &dw, &dh, &dbpr) == 1, "rect fracionário recorta");
    check(dw == 3 && dh == 3 && dbpr == BPR, "tamanho integral e bytesPerRow da origem");
    check(px(d, dbpr, 0, 0) == px(src, BPR, 1, 1) && px(d, dbpr, 2, 2) == px(src, BPR, 3, 3), "pixels do rect fracionário");
    free(d);

    check(cgimage_crop(src, W, H, BPR, 4, -2.2, 1.1, 5.4, 3.2, &d, &dw, &dh, &dbpr) == 1, "rect parcial");
    /* integral (-3,1)–(4,5) ∩ imagem = (0,1)–(4,5) */
    check(dw == 4 && dh == 4, "interseção parcial 4×4");
    check(px(d, dbpr, 0, 0) == px(src, BPR, 0, 1) && px(d, dbpr, 3, 3) == px(src, BPR, 3, 4), "pixels da interseção");
    free(d);

    check(cgimage_crop(src, W, H, BPR, 4, 0, 0, 0, 4, &d, &dw, &dh, &dbpr) == 0 && d == 0, "largura zero");
    check(cgimage_crop(src, W, H, BPR, 4, 10, 0, 2, 2, &d, &dw, &dh, &dbpr) == 0 && d == 0, "todo à direita");
    check(cgimage_crop(src, W, H, BPR, 4, 0, -4, 3, 2, &d, &dw, &dh, &dbpr) == 0 && d == 0, "todo acima");
    check(cgimage_crop(0, W, H, BPR, 4, 0, 0, 1, 1, &d, &dw, &dh, &dbpr) == 0, "imagem NULL");

    if (falhas) { fprintf(stderr, "cgimage_crop_host: %d falhas\n", falhas); return 1; }
    printf("cgimage_crop_host: ok\n");
    return 0;
}
