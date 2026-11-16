/* mOS — pixels do NSImage (sem Objective-C): o NSImage guarda u32 ARGB sem pré-multiplicar, linha a linha.
 * Teste de host: userland/tests-appkit/image_host.c (-DCX_IMG_HOST). */
#pragma once
#include <stdint.h>
/* copia w×h pixels u32 ARGB de src (bpr bytes por linha) para dst contíguo; premul = 1 desfaz a pré-multiplicação */
void cx_px_import(const uint8_t *src, long bpr, int w, int h, int premul, uint32_t *dst);
/* tamanho de desenho: o pedido, ou o de pixels quando o pedido é zero (initWithCGImage:size: com NSZeroSize) */
void cx_px_size(double want_w, double want_h, int px_w, int px_h, double *out_w, double *out_h);
