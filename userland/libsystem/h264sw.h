/* H.264 auxiliar: dimensões de um SPS e cópia NV12 -> I420. Código original do mOS (MIT). */
#pragma once
#include <stdint.h>
/* nal = SPS com o cabeçalho de 1 byte (como no avcC); 0 ok */
int h264_sps_wh(const uint8_t *nal, int n, int *w, int *h);
void h264_nv12_to_i420(const uint8_t *yp, int sy, const uint8_t *uv, int suv,
                       int w, int h, uint8_t *y, uint8_t *u, uint8_t *v, int dy, int du, int dv);
