/* mOS — núcleo do NSSegmentedControl (ver appkit_segment_core.h). Teste de host: -DCX_SG_HOST. */
#ifdef CX_SG_HOST
#include <string.h>
#else
#include "sys.h"
#include "libc.h"
#endif
#include "appkit_segment_core.h"

void sg_init(sg_t *s) { memset(s, 0, sizeof *s); s->selected_one = -1; s->pressed = -1; }

void sg_set_count(sg_t *s, int n) {
    if (n < 0) n = 0;
    if (n > SG_MAX) n = SG_MAX;
    for (int i = s->n; i < n; i++) { s->fixed[i] = 0; s->natural[i] = 0; s->selected[i] = 0; s->enabled[i] = 1; }
    s->n = n;
    if (s->selected_one >= n) s->selected_one = -1;
}

double sg_auto_width(double natural) { double w = natural + 16; return w < 20 ? 20 : w; }

static double seg_w(const sg_t *s, int i) { return s->fixed[i] > 0 ? s->fixed[i] : sg_auto_width(s->natural[i]); }

double sg_intrinsic_width(const sg_t *s) { double t = 0; for (int i = 0; i < s->n; i++) t += seg_w(s, i); return t; }

double sg_layout(sg_t *s, double total) {
    int n = s->n;
    if (!n) return 0;
    double base = sg_intrinsic_width(s), x = 0;
    int nauto = 0;
    for (int i = 0; i < n; i++) if (s->fixed[i] <= 0) nauto++;
    double extra = total > base ? total - base : 0;
    for (int i = 0; i < n; i++) {
        double w = seg_w(s, i);
        switch (s->distribution) {
        case SG_FILL: if (s->fixed[i] <= 0 && nauto) w += extra / nauto; break;
        case SG_FILL_EQUALLY: if (total > 0) w = total / n; break;
        case SG_FILL_PROPORTIONALLY: if (base > 0 && total > 0) w = w * total / base; break;
        default: break;
        }
        s->x[i] = x; s->w[i] = w;
        x += w;
    }
    return x;
}

int sg_hit(const sg_t *s, double x) {
    for (int i = 0; i < s->n; i++) if (x >= s->x[i] && x < s->x[i] + s->w[i]) return i;
    return -1;
}

void sg_set_selected(sg_t *s, int i, int on) {
    if (i < 0 || i >= s->n) return;
    if (s->tracking == SG_SELECT_ANY) { s->selected[i] = on != 0; return; }
    if (on) { for (int k = 0; k < s->n; k++) s->selected[k] = 0; s->selected[i] = 1; s->selected_one = i; }
    else { s->selected[i] = 0; if (s->selected_one == i) s->selected_one = -1; }
}

int sg_selected_segment(const sg_t *s) {
    if (s->tracking == SG_SELECT_ANY) {   /* Cocoa: o último segmento selecionado; aqui o de maior índice */
        for (int i = s->n - 1; i >= 0; i--) if (s->selected[i]) return i;
        return -1;
    }
    return s->selected_one;
}

int sg_click(sg_t *s, int i) {
    if (i < 0 || i >= s->n || !s->enabled[i]) return 0;
    switch (s->tracking) {
    case SG_SELECT_ANY: s->selected[i] = !s->selected[i]; s->selected_one = i; return 1;
    case SG_MOMENTARY: case SG_MOMENTARY_ACCEL: s->selected_one = i; return 1;   /* fica selecionado só durante o clique */
    default:
        if (s->selected_one == i) return 0;
        sg_set_selected(s, i, 1);
        return 1;
    }
}
