/* H.264: decodificador Baseline (h264bsd, Apache-2.0) e codificador intra I_PCM.
 * HEVC não entra. Tabelas VLC do codificador: OpenH264 / Cisco, BSD-2. O resto é MIT. */
#pragma once
#include <stdint.h>

typedef struct {
    uint8_t *sps, *pps, *sample, *annexb;
    int sps_n, pps_n, sample_n, annexb_n;
    int w, h;
} h264_au;

int h264_encode_i420(const uint8_t *y, const uint8_t *u, const uint8_t *v,
                     int w, int h, int sy, int su, int sv, int qp, h264_au *o);
void h264_au_free(h264_au *o);
int h264_decode_nals(const uint8_t *sps, int sps_n, const uint8_t *pps, int pps_n,
                     const uint8_t *slice, int slice_n,
                     uint8_t *y, uint8_t *u, uint8_t *v, int sy, int su, int sv,
                     int *w, int *h);
/* sample = NALs AVCC (tamanho de 4 bytes, big-endian). */
int h264_decode_avcc(const uint8_t *sps, int sps_n, const uint8_t *pps, int pps_n,
                     const uint8_t *sample, int sample_n,
                     uint8_t *y, uint8_t *u, uint8_t *v, int sy, int su, int sv,
                     int *w, int *h);
int h264_sps_wh(const uint8_t *nal, int n, int *w, int *h);
/* Annex B inteiro. *yuv é I420 concatenado (o chamador dá free). */
int h264_bsd_decode_all(const uint8_t *bytes, int n, uint8_t **yuv, int *frames, int *w, int *h);

void h264_bgra_to_i420(const uint8_t *bgra, int w, int h, int sb,
                       uint8_t *y, uint8_t *u, uint8_t *v, int sy, int su, int sv);
void h264_i420_to_bgra(const uint8_t *y, const uint8_t *u, const uint8_t *v,
                       int w, int h, int sy, int su, int sv, uint8_t *bgra, int sb);
void h264_nv12_to_i420(const uint8_t *yp, int sy, const uint8_t *uv, int suv,
                       int w, int h, uint8_t *y, uint8_t *u, uint8_t *v, int dy, int du, int dv);
void h264_i420_to_nv12(const uint8_t *y, const uint8_t *u, const uint8_t *v,
                       int w, int h, int sy, int su, int sv, uint8_t *yp, int dy, uint8_t *uv, int duv);
