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
/* view posta na linha/coluna (rowForView:, columnForView:, viewAtColumn:row:makeIfNecessary:) */
void cx_tv_engine_placed_add_at(cx_tv_engine_t *e, void *view, int row, int col);
/* índice em placed, ou -1; row/col recebem a posição */
int cx_tv_engine_placed_find(const cx_tv_engine_t *e, const void *view, int *row, int *col);
/* col < 0 = qualquer coluna da linha */
void *cx_tv_engine_placed_view(const cx_tv_engine_t *e, int row, int col);
int cx_tv_engine_placed_count(const cx_tv_engine_t *e);
void *cx_tv_engine_placed_at(const cx_tv_engine_t *e, int i);

int cx_tv_engine_same_span(const cx_tv_engine_t *e, int first, int last, double scroll_y);
void cx_tv_engine_note_span(cx_tv_engine_t *e, int first, int last, double scroll_y);

/* 1 quando o texto cell-based desta célula deve aparecer. got_view é 1 se viewFor
 * devolveu uma view. Delegate com viewFor que devolve nil continua mostrando a célula. */
int cx_tv_show_cell(int got_view);
void cx_tv_engine_cells_clear(cx_tv_engine_t *e);
void cx_tv_engine_note_cell(cx_tv_engine_t *e, int row, int col, int got_view);
int cx_tv_engine_cell_view(const cx_tv_engine_t *e, int row, int col);

/* Outlet do protótipo que aponta para uma view clonada. Devolve o clone, ou NULL se o
 * valor não está na lista (não reescreve outlet de objeto que não é subview). */
void *cx_tv_remap_outlet(void *value, void *const *src, void *const *dst, int n);

/* scrollRowToVisible. in_scroll_view == 0 grava o scroll na própria tabela (own_scroll),
 * para a tabela que não está dentro de um NSScrollView. Com scroll view, só devolve o
 * novo scroll e não mexe em own_scroll. */
double cx_tv_reveal_row(cx_tv_engine_t *e, int in_scroll_view, double scroll_y, double view_h, double row_h, double header_h, int row, int nrows);
double cx_tv_engine_own_scroll(const cx_tv_engine_t *e);
/* y da célula. No NSScrollView o documento é que se desloca, então y fica no documento.
 * Sem scroll view, desconta own_scroll. */
double cx_tv_cell_y_scrolled(double header_h, double row_h, int row, int in_scroll_view, double own_scroll);

/* Chaves que o nib da tabela usa para as views reutilizáveis. */
extern const char cx_tv_archived_views_key[];
extern const char cx_tv_reusable_views_key[];

typedef struct {
    int (*count)(void *ctx, const void *parent);
    const void *(*child)(void *ctx, int index, const void *parent);
    int (*expanded)(void *ctx, const void *item);
} cx_ol_ops;
typedef void (*cx_ol_emit_fn)(void *emit_ctx, const void *item, int level);
/* Achata a árvore. parent NULL é a raiz. cap <= 0 não limita. Devolve quantas linhas emite. */
int cx_ol_flatten(const cx_ol_ops *ops, void *ctx, cx_ol_emit_fn emit, void *emit_ctx, int cap);

typedef struct {
    int enabled;
    int flexible;
    double min_w;
    void *target;
    void *action;
} cx_tb_spec;
typedef struct { double x, y, w, h; } cx_tb_rect;
typedef void (*cx_tb_send_fn)(void *target, void *action, int index, void *ctx);
void cx_tb_layout(const cx_tb_spec *items, int n, double width, double height, cx_tb_rect *out);
int cx_tb_hit(const cx_tb_rect *rects, int n, double x, double y);
/* 1 se o clique caiu num item ligado e a ação foi encaminhada. */
int cx_tb_click(const cx_tb_spec *items, const cx_tb_rect *rects, int n, double x, double y, cx_tb_send_fn send, void *ctx);

typedef struct {
    double (*min_coord)(void *ctx, int index, double proposed);
    double (*max_coord)(void *ctx, int index, double proposed);
    double (*position)(void *ctx, int index, double proposed);
} cx_sv_lim;
/* 1 se o mouse está na divisória ou na faixa de 4 pt de cada lado. */
int cx_sv_divider_hit(double mouse, double divider_pos, double thickness);
double cx_sv_drag(double pos, double delta, double min_pos, double max_pos, const cx_sv_lim *lim, void *ctx, int index);
double cx_sv_thickness(int style);
/* holdingPriority: hold[i] (NULL = 250 para todas); collapsed[i] != 0 fica com 0 */
void cx_sv_resize(double *sz, const float *hold, const int *collapsed, int n, double total, double div);
void cx_sv_set_position(double *sz, int n, int idx, double pos, double div);
int cx_sv_collapse(double proposed, double lo, double hi, double min_pos, double max_pos, int can_before, int can_after);
