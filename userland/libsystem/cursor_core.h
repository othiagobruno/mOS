/* mOS — imagens de cursor sem libc (32x32, ARGB não pré-multiplicado em u32 0xAARRGGBB, linha a linha de cima para
 * baixo) para as formas do NSCursor que o window server não desenha: proibido, atalho, menu de contexto, sumir e o I-beam
 * vertical (texto em colunas).
 * Polígonos com 4x4 amostras por pixel; seta preta com contorno branco, como as do window server (kernel/ui/wm.c).
 * Também reduz uma imagem ARGB qualquer para caber em 32x32 (NSCursor initWithImage:hotSpot:).
 * Desenho original do mOS. Teste: userland/tests-appkit/cursor_host.c. */
#ifndef CURSOR_CORE_H
#define CURSOR_CORE_H
#include <stdint.h>

#define CUR_PX 32
enum { CUR_NOT_ALLOWED = 1, CUR_LINK, CUR_CONTEXT, CUR_POOF, CUR_IBEAM_VERTICAL };

/* desenha a forma em out[32*32]; devolve 1 e o ponto quente (hx, hy), ou 0 para forma desconhecida */
int cur_draw(int form, uint32_t *out, int *hx, int *hy);
/* reduz src (sw x sh, ARGB não pré-multiplicado) por média de área para caber em 32x32 mantendo a proporção.
 * Escreve dw x dh em out (dw*dh pixels) e ajusta o ponto quente (hx, hy) na mesma escala. Devolve 1 se deu certo. */
int cur_fit(const uint32_t *src, int sw, int sh, uint32_t *out, int *dw, int *dh, double *hx, double *hy);
#endif
