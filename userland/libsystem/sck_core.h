/* mOS — núcleo do ScreenCaptureKit sem Objective-C: o SCContentFilter vira uma lista de janelas (inclusão ou exclusão),
 * o quadro capturado é posto na saída do SCStreamConfiguration (width/height, scalesToFit, preservesAspectRatio,
 * destinationRect, backgroundColor) e convertido para 'l10r'. Teste de host: tools/sck_test.sh. Código original (MIT). */
#pragma once
#include <stddef.h>
#include <stdint.h>

typedef struct { double x, y, w, h; } sck_rect;
enum { SCK_DISPLAY_EXCL_WINDOWS = 0, SCK_DISPLAY_INCL_WINDOWS = 1, SCK_DISPLAY_EXCL_APPS = 2, SCK_DISPLAY_INCL_APPS = 3, SCK_WINDOW = 4 };
#define SCK_MAX 64
typedef struct {
    int kind;
    uint32_t win[SCK_MAX]; int nwin;          /* janelas da lista (ou exceções, nos filtros por app) */
    int32_t pid[SCK_MAX]; int npid;            /* apps */
    uint32_t window;                           /* SCK_WINDOW */
} sck_filter;
typedef struct { uint32_t id; int32_t pid; } sck_win;
/* janelas do filtro em ids[] (até SCK_MAX); *include = 1: só elas, 0: todas menos elas. Devolve quantas. */
int sck_resolve(const sck_filter *f, const sck_win *wins, int n, uint32_t *ids, int *include);

/* onde o quadro de origem (sw x sh) entra na saída (ow x oh). dest vazio = a saída toda */
sck_rect sck_layout(int sw, int sh, int ow, int oh, int scales, int preserve, sck_rect dest);
/* desenha src (BGRA, stride em pixels) em dst (ow x oh, BGRA) no retângulo r (bilinear se escala), fundo bg (0xAARRGGBB) */
void sck_place(const uint32_t *src, int sw, int sh, int sstride, uint32_t *dst, int ow, int oh, sck_rect r, uint32_t bg);
/* BGRA -> 'l10r' (ARGB 2:10:10:10 little-endian) */
void sck_to_l10r(const uint32_t *bgra, int w, int h, uint32_t *out);
/* mesmo conteúdo do quadro anterior (o SCStream só manda quadro quando a tela muda) */
uint64_t sck_hash(const uint32_t *px, size_t n);
