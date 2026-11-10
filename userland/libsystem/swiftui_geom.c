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

void mos_sui_popover_frame(double ax, double ay, double aw, double ah, double pw, double ph, double bx, double by,
                           double bw, double bh, double *x, double *y, double *w, double *h) {
    mos_sui_popover_frame_inline(ax, ay, aw, ah, pw, ph, bx, by, bw, bh, x, y, w, h);
}

void mos_sui_glass_fallback(double *r, double *g, double *b, double *a, double *radius, int *fallback,
                            int *samples_backdrop) {
    mos_sui_glass_fallback_inline(r, g, b, a, radius, fallback, samples_backdrop);
}

int mos_sui_selftest(char *err, int errlen) { return mos_sui_selftest_run(err, errlen); }
