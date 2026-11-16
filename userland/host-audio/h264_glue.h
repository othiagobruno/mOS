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

/* ---- codificador (OpenH264 2.6.0, BSD-2): I420 -> Annex B, sem quadros B (como o VideoToolbox em tempo real) */
typedef struct cx_h264enc cx_h264enc;
typedef struct {
    int width, height;
    double fps;                 /* quadros por segundo esperados (controle de taxa) */
    int bitrate;                /* bits por segundo; 0 = qualidade constante (qp) */
    int qp;                     /* com bitrate 0: 0..51 */
    int keyint;                 /* quadros entre IDRs; 0 = só o primeiro */
    int profile;                /* 66 Baseline (CAVLC), 77 Main, 100 High (CABAC) */
    int realtime;               /* 1: baixa latência (câmera, chamada); 0: arquivo */
    int entropy;                /* 0 pelo perfil (Baseline CAVLC, Main/High CABAC), 1 CAVLC, 2 CABAC (Main/High) */
} cx_h264enc_cfg;
/* nal: uma NAL sem start code; type = nal_unit_type (5 IDR, 1 fatia, 7 SPS, 8 PPS, 6 SEI) */
typedef void (*cx_h264enc_nal)(void *ctx, const uint8_t *nal, int n, int type);
cx_h264enc *cx_h264enc_open(const cx_h264enc_cfg *cfg);
/* um quadro; *idr = 1 se saiu IDR. 0 ok (quadro pulado pelo controle de taxa: nenhuma NAL), < 0 erro */
int cx_h264enc_encode(cx_h264enc *e, const uint8_t *y, const uint8_t *u, const uint8_t *v, int sy, int suv, uint64_t ts_ms,
                      int force_idr, cx_h264enc_nal nal, void *ctx, int *idr);
int cx_h264enc_set_bitrate(cx_h264enc *e, int bitrate);
int cx_h264enc_set_fps(cx_h264enc *e, double fps);
void cx_h264enc_close(cx_h264enc *e);
#ifdef __cplusplus
}
#endif
