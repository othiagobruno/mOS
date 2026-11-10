/* mOS — exporta a geometria do hosting SwiftUI para o módulo Swift (MOSBridge).
 * A cena de prova mora no cabeçalho, para o teste do host e o libSystem usarem o mesmo código. */
#include "../sdk/swift/MOSBridge/mos_sui_geom.h"

double mos_sui_clamp_offset(double offset, double delta, double viewport, double content) {
    return mos_sui_clamp_offset_inline(offset, delta, viewport, content);
}

int mos_sui_intersect(double ax, double ay, double aw, double ah, double bx, double by, double bw, double bh,
                      double *x, double *y, double *w, double *h) {
    return mos_sui_intersect_inline(ax, ay, aw, ah, bx, by, bw, bh, x, y, w, h);
}

void mos_sui_grid_cell(int index, int columns, double cellW, double cellH, double spacing, double *x, double *y) {
    mos_sui_grid_cell_inline(index, columns, cellW, cellH, spacing, x, y);
}

void mos_sui_popover_frame(double ax, double ay, double aw, double ah, double pw, double ph, double bx, double by,
                           double bw, double bh, double *x, double *y, double *w, double *h) {
    mos_sui_popover_frame_inline(ax, ay, aw, ah, pw, ph, bx, by, bw, bh, x, y, w, h);
}

void mos_sui_glass_fallback(double *r, double *g, double *b, double *a, double *radius, int *fallback,
                            int *samples_backdrop) {
    mos_sui_glass_fallback_inline(r, g, b, a, radius, fallback, samples_backdrop);
}

int mos_sui_indicator(double offset, double viewport, double content, int shows,
                     double vx, double vy, double vw, double vh, int vertical,
                     double *x, double *y, double *w, double *h) {
    return mos_sui_indicator_inline(offset, viewport, content, shows, vx, vy, vw, vh, vertical, x, y, w, h);
}

int mos_sui_ed_index(double x, double char_w, int len) {
    return mos_sui_ed_index_inline(x, char_w, len);
}

int mos_sui_ed_selection(int caret, int anchor, int *a, int *b) {
    return mos_sui_ed_selection_inline(caret, anchor, a, b);
}

void mos_sui_ed_place(int len, int *caret, int *anchor, int index, int extend) {
    mos_sui_ed_place_inline(len, caret, anchor, index, extend);
}

void mos_sui_ed_replace(char *text, int cap, int *len, int *caret, int *anchor, const char *ins) {
    mos_sui_ed_replace_inline(text, cap, len, caret, anchor, ins);
}

void mos_sui_ed_backspace(char *text, int cap, int *len, int *caret, int *anchor) {
    mos_sui_ed_backspace_inline(text, cap, len, caret, anchor);
}

void mos_sui_ed_arrow(const char *text, int len, int *caret, int *anchor, int dir, int shift) {
    mos_sui_ed_arrow_inline(text, len, caret, anchor, dir, shift);
}

void mos_sui_ed_vertical(const char *text, int len, int *caret, int *anchor, int dir, int shift) {
    mos_sui_ed_vertical_inline(text, len, caret, anchor, dir, shift);
}

int mos_sui_selftest(char *err, int errlen) { return mos_sui_selftest_run(err, errlen); }
