/* mOS — paginação da impressão (NSPrintOperation), sem Objective-C.
 * Teste de host: userland/tests-appkit/print_host.c (-DCX_PRN_HOST). */
#pragma once
enum { PRN_AUTO = 0, PRN_FIT = 1, PRN_CLIP = 2 };   /* NSPrintingPaginationMode */
typedef struct {
    double paper_w, paper_h;                 /* papel em pontos (já com a orientação aplicada) */
    double left, right, top, bottom;         /* margens */
    double scale;                            /* NSPrintScalingFactor (1 = 100%) */
    int hpag, vpag;                          /* paginação horizontal e vertical */
    int hcenter, vcenter;                    /* centralizar quando sobra papel */
} prn_setup_t;
typedef struct {
    double x, y, w, h;      /* retângulo da view impresso nesta página (coordenadas da view) */
    double ox, oy;          /* onde ele cai no papel (pontos, origem embaixo à esquerda) */
    double scale;           /* escala efetiva (inclui o ajuste de FIT) */
} prn_page_t;
/* Pagina uma view de vw × vh (flipped: y cresce para baixo, a 1ª página é a de cima). Devolve o número de páginas
 * (até cap em out). Páginas na ordem do Cocoa: linhas de cima para baixo, colunas da esquerda para a direita. */
int prn_paginate(const prn_setup_t *s, double vw, double vh, int flipped, prn_page_t *out, int cap);
/* papéis conhecidos: nome do Cocoa → tamanho em pontos; 0 se não conhece */
int prn_paper_size(const char *name, double *w, double *h);
/* páginas pedidas [first, last] (1-based) dentro de 1..n; 0 se o intervalo é vazio */
int prn_clamp_range(int n, int all, int first, int last, int *a, int *b);
