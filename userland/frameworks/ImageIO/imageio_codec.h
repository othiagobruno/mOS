/*
 * mOS — pixels que o ImageIO decodifica (PNG e JPEG).
 * px é 0xAARRGGBB, alfa reto, uma linha depois da outra. O CGImage do mOS guarda o mesmo
 * layout (little-endian vira B,G,R,A, que o CoreGraphics lê com kCGImageAlphaFirst).
 */
#ifndef MOS_IMAGEIO_CODEC_H
#define MOS_IMAGEIO_CODEC_H
#include <stddef.h>
#include <stdint.h>

typedef struct {
    int w, h;
    int has_alpha;
    uint32_t *px;
} mos_img_t;

/* "public.png", "public.jpeg" ou NULL. Só olha o cabeçalho. */
const char *mos_img_uti(const uint8_t *data, size_t n);
/* 0 = ok. -1 = não é PNG/JPEG ou o arquivo não decodifica. */
int mos_img_decode(const uint8_t *data, size_t n, mos_img_t *out);
void mos_img_free(mos_img_t *img);

/* PNG RGBA sem filtro, deflate armazenado (válido, sem compressão). 0 = ok. */
int mos_img_encode_png(const uint32_t *px, int w, int h, uint8_t **out, size_t *n);
/* JPEG baseline 4:4:4. quality 1..100. 0 = ok. */
int mos_img_encode_jpeg(const uint32_t *px, int w, int h, int quality, uint8_t **out, size_t *n);

#endif
