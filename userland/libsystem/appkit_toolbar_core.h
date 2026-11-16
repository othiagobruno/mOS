/* mOS — layout da NSToolbar (sem Objective-C): mínimo/máximo, espaços flexíveis, itens centrados e transbordo
 * por visibilityPriority. Teste de host: userland/tests-appkit/toolbar_host.c (-DCX_TBK_HOST). */
#pragma once
typedef struct {
    double min_w, max_w;   /* max_w <= min_w: largura fixa */
    int flexible;          /* NSToolbarFlexibleSpaceItem */
    int priority;          /* NSToolbarItemVisibilityPriority: -1000 baixa, 0 padrão, 1000 alta, 2000 usuário */
    int centered;          /* está em centeredItemIdentifiers */
} tbk_item;
typedef struct { double x, w; int visible; } tbk_out;
/* devolve 1 quando sobrou item fora (precisa do botão », em *chev_x com largura chev_w) */
int tbk_layout(const tbk_item *it, int n, double width, double chev_w, tbk_out *out, double *chev_x);
int tbk_hit(const tbk_out *o, int n, double x);
