/* mOS — núcleo do AVVideoComposition sem Objective-C: rampas de transform/opacidade/recorte de uma layer instruction
 * e a composição de um quadro BGRA na tela de render (transform afim do CGAffineTransform, amostragem bilinear,
 * opacidade, recorte no espaço da faixa). Teste: tools/avvcomp_test.sh. Código original do mOS (MIT). */
#pragma once
#include <stdint.h>

typedef struct { double a, b, c, d, tx, ty; } vc_aff;
typedef struct { double x, y, w, h; } vc_rect;
/* uma rampa: de t0 a t1 o valor vai de v0 a v1 (linear); fora dela vale o valor da borda. Um "set ... atTime:" é uma
 * rampa de duração zero. */
typedef struct { double t0, t1; vc_aff a0, a1; } vc_aff_ramp;
typedef struct { double t0, t1; float v0, v1; } vc_f_ramp;
typedef struct { double t0, t1; vc_rect r0, r1; } vc_rect_ramp;
#define VC_MAX_RAMPS 32
typedef struct {
    int32_t track;
    vc_aff_ramp tr[VC_MAX_RAMPS]; int ntr;
    vc_f_ramp op[VC_MAX_RAMPS]; int nop;
    vc_rect_ramp cr[VC_MAX_RAMPS]; int ncr;
} vc_layer;
/* valor no tempo t (sem rampa: identidade / 1.0 / sem recorte, has_crop = 0) */
vc_aff vc_layer_transform(const vc_layer *l, double t);
float vc_layer_opacity(const vc_layer *l, double t);
int vc_layer_crop(const vc_layer *l, double t, vc_rect *out);
/* compõe src (sw x sh, BGRA, origem em cima) em dst (dw x dh) com o transform (em pixels, eixo y para baixo, como o
 * AVFoundation usa nas coordenadas do quadro), opacidade e recorte (no espaço da origem; has_crop 0 = sem) */
void vc_composite(uint32_t *dst, int dw, int dh, const uint32_t *src, int sw, int sh, vc_aff t, float opacity, const vc_rect *crop);
