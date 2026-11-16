/* mOS — algoritmos de Vision sem aprendizado de máquina (luminância 8 bits, linha 0 em cima). Teste: tools/vision_core2_test.c.
 * Código original do mOS (MIT). */
#pragma once
#include <stdint.h>

/* contornos: regiões de primeiro plano (escuro sobre claro, ou o contrário) e seus buracos, com hierarquia.
 * Cada contorno é a borda seguida no sentido de Moore; parent -1 = contorno de topo. */
typedef struct { float *pts; int npts; int parent; int is_hole; } vc_contour;
typedef struct { vc_contour *c; int n; } vc_contours;
int vc_find_contours(const uint8_t *y, int w, int h, int dark_on_light, float contrast, vc_contours *out);
void vc_free_contours(vc_contours *c);
/* simplificação Douglas-Peucker (eps no espaço dos pontos); devolve quantos ficaram em out */
int vc_simplify(const float *pts, int n, float eps, float *out);

/* saliência por contraste de frequência (Achanta et al. 2009): |média global - imagem suavizada|, normalizada 0..1,
 * reduzida para ow x oh; *box = caixa da região acima de metade do máximo (x, y, w, h em 0..1, y para cima) */
void vc_saliency(const uint8_t *y, int w, int h, float *out, int ow, int oh, float *box);

/* translação que leva a imagem alvo à de referência (pixels, y para baixo): busca em pirâmide pela menor diferença */
int vc_translation(const uint8_t *ref, const uint8_t *tgt, int w, int h, int *dx, int *dy);

/* rastreamento: melhor posição do modelo (tw x th) na imagem perto de (px, py), por correlação normalizada; devolve a nota */
float vc_track(const uint8_t *img, int w, int h, const uint8_t *tmpl, int tw, int th, int px, int py, int radius, int *bx, int *by);
