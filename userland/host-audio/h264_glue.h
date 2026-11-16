/* H.264 no mOS: OpenH264 2.6.0 corrigido (userland/third_party/openh264) atrás de uma interface C.
 * Entrada: uma unidade de acesso em Annex B por chamada (SPS/PPS podem vir junto). Saída: I420 pela função emit,
 * com o carimbo de 64 bits da entrada que gerou o quadro (pts, refCon...). */
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct cx_h264 cx_h264;
/* cor do VUI (H.264 E.2.1); sem VUI: faixa de vídeo e 2 (não especificado) nos três códigos */
typedef struct { int full_range, matrix, primaries, transfer; } cx_h264_color;
typedef void (*cx_h264_emit)(void *ctx, const uint8_t *y, const uint8_t *u, const uint8_t *v, int stride_y, int stride_uv,
                             int w, int h, uint64_t tag, const cx_h264_color *color);
/* decode_order 1: cada quadro sai na chamada que o decodificou (o VideoToolbox sem EnableTemporalProcessing);
 * 0: ordem de exibição, com atraso de reordenação (o resto sai no cx_h264_flush). */
cx_h264 *cx_h264_open(int decode_order);
/* devolve 0, ou < 0 se o fluxo tiver erro (o quadro errado não sai) */
int cx_h264_decode(cx_h264 *d, const uint8_t *annexb, int n, uint64_t tag, cx_h264_emit emit, void *ctx);
/* fim do fluxo: entrega os quadros que esperavam reordenação e deixa o decodificador pronto para continuar */
int cx_h264_flush(cx_h264 *d, cx_h264_emit emit, void *ctx);
void cx_h264_close(cx_h264 *d);
#ifdef __cplusplus
}
#endif
