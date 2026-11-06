/* mOS — ponte C de imagem para o SwiftUI (implementada em userland/libsystem/swiftbridge_image.c) */
#pragma once

/* Procura a imagem `name` em `dir` (NULL = recursos do bundle principal): arquivo direto
 * (nome, nome.png, nome@2x.png...) ou catálogo Assets.xcassets/<nome>.imageset.
 * Devolve um identificador opaco (NULL se não achou), o tamanho em pixels e a escala (1, 2, 3).
 * O resultado fica em cache; não libere o identificador. */
void *mos_image_lookup(const char *dir, const char *name, double *w, double *h, double *scale);
/* Desenha a imagem achada por mos_image_lookup escalada no retângulo (coordenadas da view, origem no topo). */
void mos_image_draw(void *img, double x, double y, double w, double h, double alpha);
