/* Teste de host da NSTableView view-based (geometria + fila de reutilização).
 * Compila e roda no Linux, sem machotest e sem o binário do Postico:
 *   gcc -std=c11 -Wall -Wextra -DCX_TV_HOST -Iuserland/libsystem \
 *       userland/libsystem/appkit_table.c userland/tests-appkit/table_host.c \
 *       -o /tmp/appkit-table-test && /tmp/appkit-table-test
 * O mesmo appkit_table.c é o que o AppKit chama. */
#include "appkit_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int ok, total;
static void check(int cond, const char *what) {
    total++;
    if (cond) ok++;
    printf("%s %s\n", cond ? "  ok  " : "  FALHA", what);
}

typedef struct { int serial; char ident[32]; } cell_t;
static int g_allocs;

/* Árvore A -> B, C -> D. expanded[i] controla o nó i. */
static const char *ol_name[] = {"A", "B", "C", "D"};
static const int ol_parent[] = {-1, 0, 0, 2};
static int ol_open[4];
static const void *ol_seen[8];
static int ol_level[8], ol_seen_n;
static int ol_count(void *ctx, const void *parent) {
    (void)ctx;
    int n = 0;
    for (int i = 0; i < 4; i++) {
        if (!parent && ol_parent[i] < 0) n++;
        else if (parent && ol_parent[i] >= 0 && ol_name[ol_parent[i]] == parent) n++;
    }
    return n;
}
static const void *ol_child(void *ctx, int index, const void *parent) {
    (void)ctx;
    int seen = 0;
    for (int i = 0; i < 4; i++) {
        int match = (!parent && ol_parent[i] < 0) || (parent && ol_parent[i] >= 0 && ol_name[ol_parent[i]] == parent);
        if (!match) continue;
        if (seen == index) return ol_name[i];
        seen++;
    }
    return 0;
}
static int ol_expanded(void *ctx, const void *item) {
    (void)ctx;
    for (int i = 0; i < 4; i++) if (item == ol_name[i]) return ol_open[i];
    return 0;
}
static void ol_emit(void *ctx, const void *item, int level) {
    (void)ctx;
    if (ol_seen_n < 8) { ol_seen[ol_seen_n] = item; ol_level[ol_seen_n] = level; ol_seen_n++; }
}

static int tb_calls;
static void *tb_target, *tb_action;
static void tb_send(void *target, void *action, int index, void *ctx) {
    (void)index; (void)ctx;
    tb_calls++;
    tb_target = target;
    tb_action = action;
}
static double lim_min(void *ctx, int index, double proposed) { (void)ctx; (void)index; (void)proposed; return 120; }
static double lim_max(void *ctx, int index, double proposed) { (void)ctx; (void)index; return proposed > 180 ? 180 : proposed; }

static cell_t *new_cell(const char *ident) {
    cell_t *c = calloc(1, sizeof *c);
    if (!c) return 0;
    c->serial = ++g_allocs;
    snprintf(c->ident, sizeof c->ident, "%s", ident);
    return c;
}

/* O mesmo contrato de -[NSTableView makeViewWithIdentifier:owner:]: fila, senão protótipo. */
static void *make_view(cx_tv_engine_t *e, const char *ident) {
    void *v = cx_tv_engine_take(e, ident);
    if (v) return v;
    if (!cx_tv_engine_token(e, ident, 0)) return 0;
    return new_cell(ident);
}

static void recycle_placed(cx_tv_engine_t *e) {
    int n = cx_tv_engine_placed_count(e);
    for (int i = 0; i < n; i++) {
        cell_t *v = cx_tv_engine_placed_at(e, i);
        if (v) cx_tv_engine_put(e, v->ident, v);
    }
    cx_tv_engine_placed_clear(e);
}

/* Uma view por linha visível (uma coluna), como o layout do AppKit. */
static int place_visible(cx_tv_engine_t *e, cx_tv_span_t sp, const char *ident) {
    int n = 0;
    if (sp.count <= 0) return 0;
    for (int r = sp.first; r <= sp.last; r++) {
        void *v = make_view(e, ident);
        if (!v) continue;
        cx_tv_engine_placed_add(e, v);
        n++;
    }
    return n;
}

int main(void) {
    cx_tv_engine_t *e = cx_tv_engine_get((void *)1, 1);
    check(cx_tv_engine_register(e, "Fav", (void *)1, CX_TV_PROTO) == 0, "registra protótipo Fav");
    void *a = make_view(e, "Fav");
    check(a != 0, "makeView devolve view não nula");
    check(g_allocs == 1, "primeira view vem do protótipo");
    cx_tv_engine_put(e, "Fav", a);
    check(cx_tv_engine_pool_count(e, "Fav") == 1, "view volta para a fila");
    void *b = make_view(e, "Fav");
    check(b == a, "reutilização devolve a mesma view do identifier");
    check(g_allocs == 1, "reutilização não instancia de novo");
    check(cx_tv_engine_register(e, "Other", (void *)1, CX_TV_PROTO) == 0, "registra Other");
    void *c = make_view(e, "Other");
    check(c != 0 && c != a, "identifier diferente não reaproveita a view");
    check(make_view(e, "Ausente") == 0, "identifier sem nib nem fila devolve nulo");

    cx_tv_engine_t *t = cx_tv_engine_get((void *)2, 1);
    cx_tv_engine_register(t, "cell", (void *)1, CX_TV_PROTO);
    cx_tv_span_t sp = cx_tv_visible_rows(0, 100, 24, 24, 50);
    check(sp.first == 0 && sp.last == 3 && sp.count == 4, "topo: 4 linhas visíveis (100 pt, linha 24, cabeçalho 24)");
    int n = place_visible(t, sp, "cell");
    check(n == sp.count && cx_tv_engine_placed_count(t) == sp.count, "número de views = linhas visíveis");
    int alloc_antes = g_allocs;
    recycle_placed(t);
    double sy = cx_tv_scroll_target(0, 100, 24, 24, 10, 50);
    check(sy == 188, "scrollRowToVisible da linha 10 vai para scroll 188");
    sp = cx_tv_visible_rows(sy, 100, 24, 24, 50);
    check(sp.first <= 10 && sp.last >= 10, "a linha pedida fica visível");
    check(sp.count == 5, "depois do scroll há 5 linhas visíveis");
    n = place_visible(t, sp, "cell");
    check(n == sp.count, "número de views = linhas visíveis depois do scroll");
    check(g_allocs == alloc_antes + 1, "o scroll reaproveita as 4 views e cria só a que faltou");

    check(cx_tv_cell_y(24, 24, 3) == 96, "y da linha 3 abaixo do cabeçalho");
    check(cx_tv_disclosure_x(8, 2, 16, 1) == 40, "triângulo no nível 2");
    check(cx_tv_cell_x(8, 2, 16, 1) == 56, "célula do outline depois do triângulo");
    check(cx_tv_cell_w(200, 8, 56) == 152, "largura desconta a indentação");
    check(cx_tv_cell_x(8, 2, 16, 0) == 8, "coluna que não é a do outline não indenta");
    check(cx_tv_visible_rows(0, 0, 24, 24, 10).count == 0, "área de altura 0 não cria views");

    check(cx_menu_key_mask(0, 0, 1) == (1ull << 20), "atalho sem máscara explícita usa Command");
    check(cx_menu_key_mask(0, 0, 0) == 0, "item sem tecla não inventa modificador");
    check(cx_menu_key_mask(1, 0, 1) == 0, "máscara explícita 0 é respeitada");
    check(cx_menu_key_mask(1, (1ull << 17) | (1ull << 20), 1) == ((1ull << 17) | (1ull << 20)), "Shift+Command gravado volta igual");
    check(cx_mods_changed(0, 1ull << 20) == 1, "Command pressionado pede flagsChanged");
    check(cx_mods_changed(1ull << 20, 1ull << 20) == 0, "modificadores iguais não repetem flagsChanged");

    /* I24: viewFor que devolve nil continua cell-based; a coluna que ganhou view não desenha texto por cima. */
    check(cx_tv_show_cell(0) == 1, "viewFor nil mostra a célula cell-based");
    check(cx_tv_show_cell(1) == 0, "viewFor com view não desenha o texto por cima");
    cx_tv_engine_t *mix = cx_tv_engine_get((void *)9, 1);
    cx_tv_engine_note_cell(mix, 2, 0, 0);
    cx_tv_engine_note_cell(mix, 2, 1, 1);
    check(cx_tv_engine_cell_view(mix, 2, 0) == 0, "coluna cujo viewFor devolveu nil");
    check(cx_tv_show_cell(cx_tv_engine_cell_view(mix, 2, 0)) == 1, "essa coluna mostra a célula");
    check(cx_tv_show_cell(cx_tv_engine_cell_view(mix, 2, 1)) == 0, "a outra coluna, com view, não duplica o texto");
    cx_tv_engine_note_cell(mix, 2, 1, 0);
    check(cx_tv_show_cell(cx_tv_engine_cell_view(mix, 2, 1)) == 1, "viewFor que passa a devolver nil volta a mostrar a célula");

    /* I24: outlet de subclasse aponta para o clone, sem reinstanciar o nib. */
    void *src_v[] = {(void *)1, (void *)2, (void *)3};
    void *dst_v[] = {(void *)10, (void *)20, (void *)30};
    check(cx_tv_remap_outlet((void *)2, src_v, dst_v, 3) == (void *)20, "outlet de subclasse aponta para o clone");
    check(cx_tv_remap_outlet((void *)3, src_v, dst_v, 3) == (void *)30, "outlet aninhado também é remapeado");
    check(cx_tv_remap_outlet((void *)99, src_v, dst_v, 3) == 0, "outlet que não é subview não é reescrito");
    check(cx_tv_remap_outlet(0, src_v, dst_v, 3) == 0, "outlet nil continua nil");

    /* I24: scrollRowToVisible fora de NSScrollView. */
    cx_tv_engine_t *solo = cx_tv_engine_get((void *)3, 1);
    double solo_y = cx_tv_reveal_row(solo, 0, 0, 100, 24, 24, 10, 50);
    check(solo_y == 188, "scrollRowToVisible fora de NSScrollView move a tabela");
    check(cx_tv_engine_own_scroll(solo) == 188, "o scroll fica na própria tabela");
    cx_tv_span_t solo_sp = cx_tv_visible_rows(cx_tv_engine_own_scroll(solo), 100, 24, 24, 50);
    check(solo_sp.first <= 10 && solo_sp.last >= 10, "a linha pedida fica visível sem NSScrollView");
    double placed_y = cx_tv_cell_y_scrolled(24, 24, 10, 0, solo_y);
    check(placed_y >= 24 && placed_y + 24 <= 100, "a linha rolada cabe na altura da tabela");
    double kept = cx_tv_reveal_row(solo, 1, 0, 100, 24, 24, 0, 50);
    check(kept == 0, "dentro do NSScrollView a linha 0 já visível não rola");
    check(cx_tv_engine_own_scroll(solo) == 188, "o scroll do NSScrollView não apaga o da tabela");
    check(cx_tv_cell_y_scrolled(24, 24, 10, 1, 188) == cx_tv_cell_y(24, 24, 10), "no NSScrollView o y continua o do documento");

    /* views arquivadas no nib e registradas */
    int arch_kind = 0;
    check(strcmp(cx_tv_archived_views_key, "NSTableViewArchivedReusableViewsKey") == 0, "chave do nib das views arquivadas");
    check(cx_tv_engine_register(e, "Arch", (void *)7, CX_TV_NIBDATA) == 0, "registra view arquivada");
    check(cx_tv_engine_token(e, "Arch", &arch_kind) == (void *)7 && arch_kind == CX_TV_NIBDATA, "makeView acha o token arquivado");
    check(cx_tv_engine_token(e, "Fav", &arch_kind) && arch_kind == CX_TV_PROTO, "a view registrada continua no protótipo");

    /* outline: expandir mostra os filhos e empurra a célula */
    memset(ol_open, 0, sizeof ol_open);
    ol_seen_n = 0;
    cx_ol_ops oops = {ol_count, ol_child, ol_expanded};
    int nrow = cx_ol_flatten(&oops, 0, ol_emit, 0, 0);
    check(nrow == 1 && ol_seen_n == 1 && ol_seen[0] == ol_name[0] && ol_level[0] == 0, "outline fechado mostra só a raiz");
    check(cx_tv_cell_x(0, ol_level[0], 16, 1) == 16, "célula da raiz indentada um nível");
    ol_open[0] = 1;
    ol_seen_n = 0;
    nrow = cx_ol_flatten(&oops, 0, ol_emit, 0, 0);
    check(nrow == 3 && ol_level[1] == 1 && ol_level[2] == 1, "expandir a raiz mostra os dois filhos");
    check(cx_tv_cell_x(0, ol_level[1], 16, 1) == 32, "filho fica uma indentação adiante da raiz");
    ol_open[2] = 1;
    ol_seen_n = 0;
    nrow = cx_ol_flatten(&oops, 0, ol_emit, 0, 0);
    check(nrow == 4 && ol_seen[3] == ol_name[3] && ol_level[3] == 2, "expandir o filho mostra o neto");
    check(cx_tv_cell_x(0, ol_level[3], 16, 1) == 48, "neto fica dois níveis adiante");
    ol_open[0] = 0;
    ol_seen_n = 0;
    nrow = cx_ol_flatten(&oops, 0, ol_emit, 0, 0);
    check(nrow == 1, "colapsar a raiz esconde os filhos");

    /* toolbar: o clique no retângulo do item chega no target/action */
    cx_tb_spec tbs[2] = {{1, 0, 80, (void *)11, (void *)22}, {1, 0, 60, (void *)33, (void *)44}};
    cx_tb_rect tbr[2];
    cx_tb_layout(tbs, 2, 200, 28, tbr);
    check(tbr[0].w == 80 && tbr[1].x == 80 && tbr[1].w == 60 && tbr[0].h == 28, "toolbar desenha os retângulos dos itens");
    check(tbr[0].x + tbr[0].w <= tbr[1].x, "os itens da toolbar não se sobrepõem");
    tb_calls = 0;
    check(cx_tb_click(tbs, tbr, 2, tbr[1].x + 4, 10, tb_send, 0) == 1, "toolbar recebendo clique");
    check(tb_calls == 1 && tb_target == (void *)33 && tb_action == (void *)44, "o clique chega no target/action do item");
    check(cx_tb_click(tbs, tbr, 2, 190, 10, tb_send, 0) == 0 && tb_calls == 1, "clique no vazio da toolbar não dispara");
    tbs[1].enabled = 0;
    check(cx_tb_click(tbs, tbr, 2, tbr[1].x + 4, 10, tb_send, 0) == 0, "item desligado não recebe o clique");

    /* split: arrasto, limites e delegate */
    check(cx_sv_divider_hit(100, 100, 1) == 1, "clique na divisória");
    check(cx_sv_divider_hit(104, 100, 1) == 1, "clique na faixa da divisória ainda pega");
    check(cx_sv_divider_hit(110, 100, 1) == 0, "clique longe da divisória não pega");
    check(cx_sv_drag(100, 40, 0, 300, 0, 0, 0) == 140, "arrastar a divisória anda 40");
    check(cx_sv_drag(100, 500, 0, 300, 0, 0, 0) == 300, "arrasto não passa do limite máximo");
    check(cx_sv_drag(100, -90, 40, 300, 0, 0, 0) == 40, "arrasto não passa do limite mínimo");
    cx_sv_lim lim = {lim_min, lim_max, 0};
    check(cx_sv_drag(100, -50, 0, 300, &lim, 0, 0) == 120, "delegate segura o mínimo da divisória");
    check(cx_sv_drag(100, 200, 0, 300, &lim, 0, 0) == 180, "delegate segura o máximo da divisória");

    printf("appkit-table: %d/%d ok\n", ok, total);
    return ok == total ? 0 : 1;
}
