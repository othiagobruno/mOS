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

    printf("appkit-table: %d/%d ok\n", ok, total);
    return ok == total ? 0 : 1;
}
