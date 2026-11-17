/* mOS — leitura de .icns sem libc (cola do NSImage em appkit.c e do NSWorkspace em appkit_icons.c).
 * Elementos aceitos: PNG (icp4..ic14, ic04/ic05), ARGB compactado (ic04/ic05 "ARGB") e os antigos RGB compactados
 * (is32/il32/ih32/it32) com a máscara de 8 bits (s8mk/l8mk/h8mk/t8mk). JPEG 2000 não é lido.
 * Teste de host: userland/tests-appkit/icon_host.c. */
#ifndef ICON_CORE_H
#define ICON_CORE_H
#include <stdint.h>

enum { IC_PNG = 1, IC_ARGB = 2, IC_RLE = 3 };
typedef struct { int kind, side; long off, len, mask_off, mask_len; int it32; } ic_pick;

/* 1 se a família é um .icns */
int ic_is_icns(const uint8_t *f, long n);
/* o melhor elemento para um lado de want pixels: o menor que cobre want, senão o maior. 0 se nenhum é legível */
int ic_pick_best(const uint8_t *f, long n, int want, ic_pick *out);
/* RGB compactado (+ máscara opcional) em side×side pixels ARGB; scratch tem 4×side×side bytes; 1 se deu certo */
int ic_decode_rle(const uint8_t *d, long n, int side, int it32, const uint8_t *mask, long mlen, uint32_t *argb, uint8_t *scratch);
/* "ARGB" + quatro canais compactados; scratch tem 4×side×side bytes; 1 se deu certo */
int ic_decode_argb(const uint8_t *d, long n, int side, uint32_t *argb, uint8_t *scratch);
/* lado em pixels de um tipo de elemento (0 se não é imagem) */
int ic_side_of(uint32_t type);
#endif
