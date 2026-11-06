/* mOS SDK — ponte C de caminhos (Path) e recorte para o renderer do SwiftUI do mOS.
 * Implementada em userland/libsystem/swiftbridge_path.c. Válida dentro do callback de desenho do canvas
 * (mos_canvas); coordenadas da view com origem no topo, como mos_draw_*.
 *
 * Um caminho guarda os elementos em coordenadas locais e uma transformação afim local -> view; as curvas
 * são achatadas no desenho, com tolerância de ~0,25 px já considerando a escala da transformação.
 * Preenchimento: regra não-zero (eo = 0) ou par-ímpar (eo = 1), com antisserrilhamento.
 * Traço: largura, terminações (cap: 0 butt, 1 round, 2 square), junções (join: 0 miter, 1 round, 2 bevel),
 * limite de miter e tracejado (dash[ndash] com fase). Recorte: pilha de retângulos e de caminhos
 * (com máscara de cobertura, opcionalmente invertida). Máscara/sombra: mos_mask_begin captura os
 * preenchimentos seguintes numa máscara em vez de desenhá-los; mos_mask_end_* a usa como recorte
 * (efeito mask) ou a desenha desfocada e deslocada (sombra). */
#pragma once

void *mos_path_create(void);
void mos_path_destroy(void *p);
void mos_path_clear(void *p);
void mos_path_move(void *p, double x, double y);
void mos_path_line(void *p, double x, double y);
void mos_path_quad(void *p, double cx, double cy, double x, double y);
void mos_path_curve(void *p, double c1x, double c1y, double c2x, double c2y, double x, double y);
void mos_path_close(void *p);
void mos_path_rect(void *p, double x, double y, double w, double h);
/* retângulo arredondado (rx, ry: raios dos cantos) e elipse inscrita, como subcaminhos fechados */
void mos_path_rrect(void *p, double x, double y, double w, double h, double rx, double ry);
void mos_path_ellipse(void *p, double x, double y, double w, double h);
/* transformação afim local -> view (padrão: identidade): x' = a*x + c*y + tx, y' = b*x + d*y + ty */
void mos_path_set_transform(void *p, double a, double b, double c, double d, double tx, double ty);
int mos_path_is_empty(void *p);

void mos_path_fill(void *p, int eo, double r, double g, double b, double a);
void mos_path_stroke(void *p, double width, int cap, int join, double miter, const double *dash, int ndash, double phase,
                     double r, double g, double b, double a);
/* sombra do preenchimento do caminho: desfoque gaussiano aproximado de raio `radius`, deslocado (dx, dy) */
void mos_path_fill_shadow(void *p, int eo, double radius, double dx, double dy, double r, double g, double b, double a);
/* contorno do traço como caminho (anéis fechados, todos na mesma orientação: preencha com não-zero) em dst,
 * nas coordenadas locais de src; devolve o número de anéis */
int mos_path_stroke_outline(void *src, void *dst, double width, int cap, int join, double miter, const double *dash, int ndash, double phase);
/* leitura dos elementos: op 0 move, 1 line, 2 quad, 3 curve, 4 close; pts recebe até 6 doubles
 * (pontos de controle antes do ponto final, como em mos_path_quad/curve); -1 fora do intervalo */
int mos_path_element_count(void *p);
int mos_path_element(void *p, int index, double *pts);
/* ponto dentro do caminho (coordenadas locais) */
int mos_path_contains(void *p, double x, double y, int eo);

/* ---- recorte (pilha; cada push intersecta com o recorte atual). Reset no começo de cada desenho. */
void mos_clip_reset(void);
void mos_clip_push_rect(double x, double y, double w, double h);   /* coordenadas da view */
void mos_clip_push_path(void *p, int eo, int inverse);
void mos_clip_pop(void);
int mos_clip_depth(void);
/* retângulo do recorte atual em coordenadas da view (para o chamador descartar o que está fora) */
void mos_clip_bounds(double *x, double *y, double *w, double *h);

/* ---- máscaras: captura dos preenchimentos (mos_path_fill/stroke/mos_fill_rect) até o end */
void mos_mask_begin(void);
void mos_mask_end_clip(int inverse);   /* empilha a máscara como recorte (desempilhe com mos_clip_pop) */
void mos_mask_end_shadow(double radius, double dx, double dy, double r, double g, double b, double a);
void mos_mask_end_discard(void);
int mos_mask_capturing(void);

/* retângulo (com cantos arredondados opcionais) que respeita recorte por caminho e captura de máscara */
void mos_fill_rect(double x, double y, double w, double h, double radius, double r, double g, double b, double a);

/* depuração: bit 1 = registra cada operação em stderr (também por MOS_PATH_TRACE no ambiente);
 * bit 2 = força a rasterização própria, que registra a área coberta (também por MOS_PATH_RASTER) */
void mos_path_trace(int on);
