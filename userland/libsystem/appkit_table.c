/* mOS — fila de reutilização e geometria da NSTableView view-based.
 * Sem classes: o teste de host compila este arquivo com -DCX_TV_HOST. */
#ifdef CX_TV_HOST
#include <stdlib.h>
#include <string.h>
#else
#include "sys.h"
#include "libc.h"
#endif
#include "appkit_table.h"

cx_tv_span_t cx_tv_visible_rows(double scroll_y, double view_h, double row_h, double header_h, int nrows) {
    cx_tv_span_t v;
    v.first = -1;
    v.last = -1;
    v.count = 0;
    if (nrows <= 0 || row_h <= 0 || view_h < 1) return v;
    if (scroll_y < 0) scroll_y = 0;
    if (header_h < 0) header_h = 0;
    double c0 = scroll_y - header_h;
    double c1 = scroll_y + view_h - header_h;
    if (c1 <= 0) return v;
    if (c0 < 0) c0 = 0;
    int first = (int)(c0 / row_h);
    int last = (int)((c1 - 0.001) / row_h);
    if (first >= nrows) return v;
    if (first < 0) first = 0;
    if (last >= nrows) last = nrows - 1;
    if (last < first) return v;
    v.first = first;
    v.last = last;
    v.count = last - first + 1;
    return v;
}

double cx_tv_scroll_target(double scroll_y, double view_h, double row_h, double header_h, int row, int nrows) {
    if (row < 0 || row >= nrows || row_h <= 0 || view_h <= 0) return scroll_y;
    if (header_h < 0) header_h = 0;
    double top = header_h + (double)row * row_h;
    double bot = top + row_h;
    double sy = scroll_y;
    if (top < sy) sy = top;
    else if (bot > sy + view_h) sy = bot - view_h;
    double maxs = header_h + (double)nrows * row_h - view_h;
    if (maxs < 0) maxs = 0;
    if (sy < 0) sy = 0;
    if (sy > maxs) sy = maxs;
    return sy;
}

double cx_tv_disclosure_x(double col_x, int level, double per_level, int outline_column) {
    if (!outline_column || level < 0 || per_level <= 0) return col_x;
    return col_x + (double)level * per_level;
}

double cx_tv_cell_x(double col_x, int level, double per_level, int outline_column) {
    if (!outline_column || level < 0 || per_level <= 0) return col_x;
    return cx_tv_disclosure_x(col_x, level, per_level, 1) + per_level;
}

double cx_tv_cell_w(double col_w, double col_x, double cell_x) {
    double w = col_w - (cell_x - col_x);
    return w < 1 ? 1 : w;
}

double cx_tv_cell_y(double header_h, double row_h, int row) {
    if (header_h < 0) header_h = 0;
    if (row < 0) row = 0;
    return header_h + (double)row * row_h;
}

unsigned long long cx_menu_key_mask(int mask_set, unsigned long long mask, int has_key) {
    if (mask_set) return mask;
    return has_key ? (1ull << 20) : 0;   /* NSEventModifierFlagCommand */
}

int cx_mods_changed(unsigned long long prev, unsigned long long next) { return prev != next; }

struct cx_tv_engine {
    void *key;
    struct { char *ident; void *view; } *pool;
    int np, cp;
    struct { char *ident; void *token; int kind; } *reg;
    int nr, cr;
    void **placed;
    int nplaced, cplaced;
    int last_first, last_last;
    double last_scroll;
    int have_span;
    double own_scroll;
    struct { int row, col, got; } *cells;
    int ncells, ccells;
};

#define CX_TV_MAX 64
static cx_tv_engine_t *g_eng[CX_TV_MAX];
static int g_neng;

static char *cx_tv_dup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *d = (char *)malloc(n + 1);
    if (!d) return 0;
    memcpy(d, s, n + 1);
    return d;
}

cx_tv_engine_t *cx_tv_engine_get(void *table, int create) {
    if (!table) return 0;
    for (int i = 0; i < g_neng; i++) if (g_eng[i] && g_eng[i]->key == table) return g_eng[i];
    if (!create || g_neng >= CX_TV_MAX) return 0;
    cx_tv_engine_t *e = (cx_tv_engine_t *)calloc(1, sizeof *e);
    if (!e) return 0;
    e->key = table;
    e->last_first = -2;
    g_eng[g_neng++] = e;
    return e;
}

void *cx_tv_engine_register(cx_tv_engine_t *e, const char *ident, void *token, int kind) {
    if (!e || !ident || !ident[0] || !token) return 0;
    for (int i = 0; i < e->nr; i++) if (e->reg[i].ident && !strcmp(e->reg[i].ident, ident)) {
        void *old = e->reg[i].token;
        e->reg[i].token = token;
        e->reg[i].kind = kind;
        return old == token ? 0 : old;
    }
    if (e->nr == e->cr) {
        int nc = e->cr ? e->cr * 2 : 4;
        void *p = realloc(e->reg, (size_t)nc * sizeof *e->reg);
        if (!p) return 0;
        e->reg = p;
        e->cr = nc;
    }
    e->reg[e->nr].ident = cx_tv_dup(ident);
    e->reg[e->nr].token = token;
    e->reg[e->nr].kind = kind;
    e->nr++;
    return 0;
}

void *cx_tv_engine_token(const cx_tv_engine_t *e, const char *ident, int *kind) {
    if (kind) *kind = 0;
    if (!e || !ident) return 0;
    for (int i = 0; i < e->nr; i++) if (e->reg[i].ident && !strcmp(e->reg[i].ident, ident)) {
        if (kind) *kind = e->reg[i].kind;
        return e->reg[i].token;
    }
    return 0;
}

void *cx_tv_engine_take(cx_tv_engine_t *e, const char *ident) {
    if (!e || !ident || !ident[0]) return 0;
    for (int i = e->np; i-- > 0;) {
        if (e->pool[i].ident && !strcmp(e->pool[i].ident, ident)) {
            void *v = e->pool[i].view;
            free(e->pool[i].ident);
            e->pool[i] = e->pool[--e->np];
            return v;
        }
    }
    return 0;
}

void cx_tv_engine_put(cx_tv_engine_t *e, const char *ident, void *view) {
    if (!e || !view || !ident || !ident[0]) return;
    if (e->np == e->cp) {
        int nc = e->cp ? e->cp * 2 : 8;
        void *p = realloc(e->pool, (size_t)nc * sizeof *e->pool);
        if (!p) return;
        e->pool = p;
        e->cp = nc;
    }
    e->pool[e->np].ident = cx_tv_dup(ident);
    e->pool[e->np].view = view;
    e->np++;
}

int cx_tv_engine_pool_count(const cx_tv_engine_t *e, const char *ident) {
    if (!e) return 0;
    int n = 0;
    for (int i = 0; i < e->np; i++) if (!ident || (e->pool[i].ident && !strcmp(e->pool[i].ident, ident))) n++;
    return n;
}

void cx_tv_engine_placed_clear(cx_tv_engine_t *e) { if (e) e->nplaced = 0; }

void cx_tv_engine_placed_add(cx_tv_engine_t *e, void *view) {
    if (!e || !view) return;
    if (e->nplaced == e->cplaced) {
        int nc = e->cplaced ? e->cplaced * 2 : 8;
        void *p = realloc(e->placed, (size_t)nc * sizeof(void *));
        if (!p) return;
        e->placed = p;
        e->cplaced = nc;
    }
    e->placed[e->nplaced++] = view;
}

int cx_tv_engine_placed_count(const cx_tv_engine_t *e) { return e ? e->nplaced : 0; }
void *cx_tv_engine_placed_at(const cx_tv_engine_t *e, int i) {
    if (!e || i < 0 || i >= e->nplaced) return 0;
    return e->placed[i];
}

int cx_tv_engine_same_span(const cx_tv_engine_t *e, int first, int last, double scroll_y) {
    return e && e->have_span && e->last_first == first && e->last_last == last && e->last_scroll == scroll_y;
}

void cx_tv_engine_note_span(cx_tv_engine_t *e, int first, int last, double scroll_y) {
    if (!e) return;
    e->have_span = 1;
    e->last_first = first;
    e->last_last = last;
    e->last_scroll = scroll_y;
}

int cx_tv_show_cell(int got_view) { return !got_view; }

void cx_tv_engine_cells_clear(cx_tv_engine_t *e) { if (e) e->ncells = 0; }

void cx_tv_engine_note_cell(cx_tv_engine_t *e, int row, int col, int got_view) {
    if (!e) return;
    for (int i = 0; i < e->ncells; i++) if (e->cells[i].row == row && e->cells[i].col == col) {
        e->cells[i].got = got_view ? 1 : 0;
        return;
    }
    if (e->ncells == e->ccells) {
        int nc = e->ccells ? e->ccells * 2 : 16;
        void *p = realloc(e->cells, (size_t)nc * sizeof *e->cells);
        if (!p) return;
        e->cells = p;
        e->ccells = nc;
    }
    e->cells[e->ncells].row = row;
    e->cells[e->ncells].col = col;
    e->cells[e->ncells].got = got_view ? 1 : 0;
    e->ncells++;
}

int cx_tv_engine_cell_view(const cx_tv_engine_t *e, int row, int col) {
    if (!e) return 0;
    for (int i = 0; i < e->ncells; i++) if (e->cells[i].row == row && e->cells[i].col == col) return e->cells[i].got;
    return 0;
}

void *cx_tv_remap_outlet(void *value, void *const *src, void *const *dst, int n) {
    if (!value || !src || !dst || n <= 0) return 0;
    for (int i = 0; i < n; i++) if (src[i] == value && dst[i]) return dst[i];
    return 0;
}

double cx_tv_reveal_row(cx_tv_engine_t *e, int in_scroll_view, double scroll_y, double view_h, double row_h, double header_h, int row, int nrows) {
    double next = cx_tv_scroll_target(scroll_y, view_h, row_h, header_h, row, nrows);
    if (e && !in_scroll_view) e->own_scroll = next;
    return next;
}

double cx_tv_engine_own_scroll(const cx_tv_engine_t *e) { return e ? e->own_scroll : 0; }

double cx_tv_cell_y_scrolled(double header_h, double row_h, int row, int in_scroll_view, double own_scroll) {
    double y = cx_tv_cell_y(header_h, row_h, row);
    if (!in_scroll_view && own_scroll > 0) y -= own_scroll;
    return y;
}

const char cx_tv_archived_views_key[] = "NSTableViewArchivedReusableViewsKey";
const char cx_tv_reusable_views_key[] = "NSTableViewReusableViewsKey";

static void cx_ol_rec(const cx_ol_ops *ops, void *ctx, const void *parent, int level, cx_ol_emit_fn emit, void *emit_ctx, int *n, int cap) {
    if (!ops || !ops->count || !ops->child || level > 32) return;
    int nch = ops->count(ctx, parent);
    if (nch < 0) nch = 0;
    if (nch > 4096) nch = 4096;
    for (int i = 0; i < nch; i++) {
        if (cap > 0 && *n >= cap) return;
        const void *ch = ops->child(ctx, i, parent);
        if (emit) emit(emit_ctx, ch, level);
        (*n)++;
        if (ops->expanded && ch && ops->expanded(ctx, ch))
            cx_ol_rec(ops, ctx, ch, level + 1, emit, emit_ctx, n, cap);
    }
}

int cx_ol_flatten(const cx_ol_ops *ops, void *ctx, cx_ol_emit_fn emit, void *emit_ctx, int cap) {
    int n = 0;
    cx_ol_rec(ops, ctx, 0, 0, emit, emit_ctx, &n, cap);
    return n;
}

void cx_tb_layout(const cx_tb_spec *items, int n, double width, double height, cx_tb_rect *out) {
    if (!out || n <= 0) return;
    if (height < 1) height = 1;
    if (width < 0) width = 0;
    double fixed = 0;
    int nflex = 0;
    for (int i = 0; i < n; i++) {
        out[i].x = out[i].y = out[i].w = out[i].h = 0;
        if (!items) continue;
        if (items[i].flexible) nflex++;
        else fixed += items[i].min_w > 1 ? items[i].min_w : 32;
    }
    double extra = width - fixed;
    if (extra < 0) extra = 0;
    double flex_w = nflex ? extra / (double)nflex : 0;
    double x = 0;
    for (int i = 0; i < n; i++) {
        double w = 32;
        if (items && items[i].flexible) w = flex_w;
        else if (items && items[i].min_w > 1) w = items[i].min_w;
        out[i].x = x;
        out[i].y = 0;
        out[i].w = w;
        out[i].h = height;
        x += w;
    }
}

int cx_tb_hit(const cx_tb_rect *rects, int n, double x, double y) {
    if (!rects || n <= 0) return -1;
    for (int i = 0; i < n; i++) {
        if (rects[i].w <= 0 || rects[i].h <= 0) continue;
        if (x >= rects[i].x && x < rects[i].x + rects[i].w && y >= rects[i].y && y < rects[i].y + rects[i].h) return i;
    }
    return -1;
}

int cx_tb_click(const cx_tb_spec *items, const cx_tb_rect *rects, int n, double x, double y, cx_tb_send_fn send, void *ctx) {
    int i = cx_tb_hit(rects, n, x, y);
    if (i < 0 || !items) return 0;
    if (!items[i].enabled || items[i].flexible) return 0;
    if (!items[i].target || !items[i].action) return 0;
    if (send) send(items[i].target, items[i].action, i, ctx);
    return 1;
}

int cx_sv_divider_hit(double mouse, double divider_pos, double thickness) {
    if (thickness < 1) thickness = 1;
    return mouse >= divider_pos - 4 && mouse < divider_pos + thickness + 4;
}

double cx_sv_drag(double pos, double delta, double min_pos, double max_pos, const cx_sv_lim *lim, void *ctx, int index) {
    if (min_pos > max_pos) { double t = min_pos; min_pos = max_pos; max_pos = t; }
    if (lim && lim->min_coord) min_pos = lim->min_coord(ctx, index, min_pos);
    if (lim && lim->max_coord) max_pos = lim->max_coord(ctx, index, max_pos);
    if (min_pos > max_pos) { double t = min_pos; min_pos = max_pos; max_pos = t; }
    double p = pos + delta;
    if (p < min_pos) p = min_pos;
    if (p > max_pos) p = max_pos;
    if (lim && lim->position) {
        p = lim->position(ctx, index, p);
        if (p < min_pos) p = min_pos;
        if (p > max_pos) p = max_pos;
    }
    return p;
}
