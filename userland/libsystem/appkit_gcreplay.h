/* mOS — reprodução do lote de desenho do AppKit (comandos GC_* de kernel/include/compat_gui.h) num destino
 * qualquer: o libSystem passa as funções do Core Graphics (PDF, bitmap, lockFocus, impressão); o teste de host
 * (userland/tests-appkit/gcreplay_host.c) passa um destino que só registra. Sem Objective-C.
 * Coordenadas do lote: origem no canto de cima à esquerda, y para baixo. Com flip, o destino recebe origem embaixo
 * (y' = altura - y), como um CGContext. */
#pragma once
#include <stdint.h>
typedef struct {
    void *ctx;
    void (*clear)(void *c, uint32_t argb);
    void (*fill)(void *c, double x, double y, double w, double h, double r, uint32_t argb);          /* r: raio */
    void (*stroke_rect)(void *c, double x, double y, double w, double h, double r, double t, uint32_t argb);
    void (*line)(void *c, double x0, double y0, double x1, double y1, double t, uint32_t argb);
    void (*poly)(void *c, const double *xy, uint32_t n, uint32_t argb, int fill, double t);
    void (*text)(void *c, double x, double base, double size, int weight, uint32_t argb, const char *s, uint32_t n);
    void (*clip)(void *c, double x, double y, double w, double h);
    void (*unclip)(void *c);
    void (*grad)(void *c, double x, double y, double w, double h, uint32_t top, uint32_t bottom);
    void (*image)(void *c, double x, double y, double w, double h, const void *argb, int pw, int ph, uint32_t op);   /* argb pode estar desalinhado: copie com memcpy */
    void (*comp_fill)(void *c, double x, double y, double w, double h, uint32_t argb, uint32_t op);
} gcr_sink_t;
/* devolve quantos comandos reproduziu; -1 se o lote está malformado (para no primeiro erro) */
int gcr_replay(const uint8_t *cmd, uint32_t n, double height, int flip, const gcr_sink_t *s);
