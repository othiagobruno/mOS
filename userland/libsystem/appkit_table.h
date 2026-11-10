/* mOS — cálculo da NSTableView/NSOutlineView view-based (sem Objective-C).
 * O AppKit (appkit.c) e o teste de host (userland/tests-appkit/table_host.c) usam as mesmas funções. */
#pragma once

typedef struct {
    int first;   /* -1 se nenhuma linha visível */
    int last;
    int count;
} cx_tv_span_t;

/* Linhas cujo retângulo cruza a faixa visível. scroll_y e view_h em pontos a partir do topo
 * do documento; header_h é a faixa do cabeçalho, que não é linha. view_h < 1 ou row_h <= 0
 * não mostra linha nenhuma. */
cx_tv_span_t cx_tv_visible_rows(double scroll_y, double view_h, double row_h, double header_h, int nrows);

/* Novo scroll_y para a linha `row` ficar inteira na área visível. Fora do intervalo, devolve scroll_y. */
double cx_tv_scroll_target(double scroll_y, double view_h, double row_h, double header_h, int row, int nrows);

/* Coluna de outline: o triângulo fica em level*per e a célula começa uma indentação depois,
 * para o desenho do triângulo não ficar por baixo da view. Nas outras colunas, x = col_x. */
double cx_tv_disclosure_x(double col_x, int level, double per_level, int outline_column);
double cx_tv_cell_x(double col_x, int level, double per_level, int outline_column);
double cx_tv_cell_w(double col_w, double col_x, double cell_x);
double cx_tv_cell_y(double header_h, double row_h, int row);

/* Máscara de atalho do NSMenuItem: se o app não definiu, Command quando há key equivalent (Cocoa). */
unsigned long long cx_menu_key_mask(int mask_set, unsigned long long mask, int has_key);
/* 1 quando os modificadores mudaram e o AppKit deve postar flagsChanged:. */
int cx_mods_changed(unsigned long long prev, unsigned long long next);

enum { CX_TV_PROTO = 1, CX_TV_NIB = 2, CX_TV_NIBDATA = 3 };

typedef struct cx_tv_engine cx_tv_engine_t;

cx_tv_engine_t *cx_tv_engine_get(void *table, int create);
/* Devolve o token anterior do mesmo identifier (para o chamador soltar). */
void *cx_tv_engine_register(cx_tv_engine_t *e, const char *ident, void *token, int kind);
void *cx_tv_engine_token(const cx_tv_engine_t *e, const char *ident, int *kind);
/* Tira uma view da fila deste identifier, ou NULL. O ponteiro volta a ser do chamador. */
void *cx_tv_engine_take(cx_tv_engine_t *e, const char *ident);
void cx_tv_engine_put(cx_tv_engine_t *e, const char *ident, void *view);
int cx_tv_engine_pool_count(const cx_tv_engine_t *e, const char *ident);

void cx_tv_engine_placed_clear(cx_tv_engine_t *e);
void cx_tv_engine_placed_add(cx_tv_engine_t *e, void *view);
int cx_tv_engine_placed_count(const cx_tv_engine_t *e);
void *cx_tv_engine_placed_at(const cx_tv_engine_t *e, int i);

int cx_tv_engine_same_span(const cx_tv_engine_t *e, int first, int last, double scroll_y);
void cx_tv_engine_note_span(cx_tv_engine_t *e, int first, int last, double scroll_y);
