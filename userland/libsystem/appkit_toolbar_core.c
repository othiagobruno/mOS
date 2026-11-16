/* mOS — layout da NSToolbar (ver appkit_toolbar_core.h). Teste de host: -DCX_TBK_HOST. */
#ifdef CX_TBK_HOST
#include <string.h>
#else
#include "sys.h"
#include "libc.h"
#endif
#include "appkit_toolbar_core.h"

#define TBK_MAX 128
static double base_w(const tbk_item *t) { return t->flexible ? (t->min_w > 0 ? t->min_w : 0) : (t->min_w > 1 ? t->min_w : 32); }

int tbk_layout(const tbk_item *it, int n, double width, double chev_w, tbk_out *out, double *chev_x) {
    if (n > TBK_MAX) n = TBK_MAX;
    if (width < 0) width = 0;
    double need = 0;
    for (int i = 0; i < n; i++) { out[i].visible = 1; out[i].x = out[i].w = 0; need += base_w(&it[i]); }
    int overflow = 0;
    double avail = width;
    /* transbordo: tira o de menor prioridade (empate: o mais à direita) até caber, contando o » */
    while (need > avail) {
        if (!overflow) { overflow = 1; avail = width - chev_w; if (avail < 0) avail = 0; continue; }
        int victim = -1;
        for (int i = n - 1; i >= 0; i--) {
            if (!out[i].visible || it[i].flexible) continue;
            if (victim < 0 || it[i].priority < it[victim].priority) victim = i;
        }
        if (victim < 0) break;
        out[victim].visible = 0;
        need -= base_w(&it[victim]);
    }
    if (overflow) {   /* o » só fica se alguém saiu */
        int hidden = 0;
        for (int i = 0; i < n; i++) hidden |= !out[i].visible;
        if (!hidden) { overflow = 0; avail = width; }
    }
    for (int i = 0; i < n; i++) out[i].w = out[i].visible ? base_w(&it[i]) : 0;
    double extra = avail - need;
    int c0 = -1, c1 = -1;
    for (int i = 0; i < n; i++) if (it[i].centered && out[i].visible) { if (c0 < 0) c0 = i; c1 = i; }
    if (extra > 0) {
        /* cresce até o máximo os itens que podem (largura igual entre eles) */
        for (int round = 0; round < 8 && extra > 0.001; round++) {
            int ng = 0;
            for (int i = 0; i < n; i++) if (out[i].visible && !it[i].flexible && it[i].max_w > out[i].w) ng++;
            if (!ng) break;
            double share = extra / ng;
            for (int i = 0; i < n; i++) {
                if (!out[i].visible || it[i].flexible || it[i].max_w <= out[i].w) continue;
                double g = it[i].max_w - out[i].w;
                if (g > share) g = share;
                out[i].w += g; extra -= g;
            }
        }
        if (c0 < 0) {   /* o resto vai para os espaços flexíveis */
            int nf = 0;
            for (int i = 0; i < n; i++) if (out[i].visible && it[i].flexible) nf++;
            for (int i = 0; nf && i < n; i++) if (out[i].visible && it[i].flexible) out[i].w += extra / nf;
        }
    }
    double x = 0;
    for (int i = 0; i < n; i++) { out[i].x = x; x += out[i].w; }
    if (c0 >= 0) {   /* grupo centrado: no meio da barra, sem invadir os vizinhos */
        double left_end = out[c0].x, cw = out[c1].x + out[c1].w - left_end, right_w = 0;
        for (int i = c1 + 1; i < n; i++) right_w += out[i].w;
        double cx = (avail - cw) / 2;
        if (cx < left_end) cx = left_end;
        if (cx > avail - right_w - cw) cx = avail - right_w - cw;
        if (cx < left_end) cx = left_end;
        double shift = cx - left_end;
        for (int i = c0; i <= c1; i++) out[i].x += shift;
        double rx = avail - right_w;
        if (rx < out[c1].x + out[c1].w) rx = out[c1].x + out[c1].w;
        for (int i = c1 + 1; i < n; i++) { out[i].x = rx; rx += out[i].w; }
    }
    if (chev_x) *chev_x = overflow ? width - chev_w : -1;
    return overflow;
}

int tbk_hit(const tbk_out *o, int n, double x) {
    for (int i = 0; i < n; i++) if (o[i].visible && o[i].w > 0 && x >= o[i].x && x < o[i].x + o[i].w) return i;
    return -1;
}
