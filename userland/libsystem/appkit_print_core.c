/* mOS — paginação da impressão (ver appkit_print_core.h). Teste de host: -DCX_PRN_HOST. */
#ifdef CX_PRN_HOST
#include <string.h>
#include <math.h>
#else
#include "sys.h"
#include "libc.h"
double ceil(double);
#endif
#include "appkit_print_core.h"

int prn_paginate(const prn_setup_t *s, double vw, double vh, int flipped, prn_page_t *out, int cap) {
    double sc = s->scale > 0 ? s->scale : 1;
    double aw = s->paper_w - s->left - s->right, ah = s->paper_h - s->top - s->bottom;
    if (aw < 1) aw = 1;
    if (ah < 1) ah = 1;
    if (vw <= 0 || vh <= 0) return 0;
    /* FIT: escala para caber na direção pedida (nunca aumenta além do fator do usuário) */
    if (s->hpag == PRN_FIT && vw * sc > aw) sc = aw / vw;
    if (s->vpag == PRN_FIT && vh * sc > ah) sc = ah / vh;
    double pw = aw / sc, ph = ah / sc;          /* quanto da view cabe numa página */
    int cols = s->hpag == PRN_AUTO ? (int)ceil(vw / pw - 1e-9) : 1;
    int rows = s->vpag == PRN_AUTO ? (int)ceil(vh / ph - 1e-9) : 1;
    if (cols < 1) cols = 1;
    if (rows < 1) rows = 1;
    int n = 0;
    for (int r = 0; r < rows; r++) for (int c = 0; c < cols; c++) {
        if (n >= cap) return n;
        prn_page_t *p = &out[n++];
        p->scale = sc;
        p->x = c * pw;
        p->w = (c == cols - 1 || s->hpag != PRN_AUTO) ? vw - p->x : pw;
        if (p->w > pw) p->w = pw;
        double top = r * ph;                    /* distância do topo da view */
        double h = vh - top < ph ? vh - top : ph;
        if (s->vpag != PRN_AUTO) h = vh < ph ? vh : ph;
        p->h = h;
        p->y = flipped ? top : vh - top - h;    /* em coordenadas da view */
        double used_w = p->w * sc, used_h = p->h * sc;
        p->ox = s->left + (s->hcenter ? (aw - used_w) / 2 : 0);
        p->oy = s->bottom + ah - used_h - (s->vcenter ? (ah - used_h) / 2 : 0);   /* encosta no topo da área */
    }
    return n;
}

int prn_paper_size(const char *name, double *w, double *h) {
    static const struct { const char *n; double w, h; } p[] = {
        {"A4", 595, 842}, {"iso-a4", 595, 842}, {"A3", 842, 1191}, {"A5", 420, 595},
        {"na-letter", 612, 792}, {"Letter", 612, 792}, {"US Letter", 612, 792},
        {"na-legal", 612, 1008}, {"Legal", 612, 1008}, {"Tabloid", 792, 1224},
    };
    for (unsigned i = 0; i < sizeof p / sizeof p[0]; i++) if (!strcmp(name, p[i].n)) { *w = p[i].w; *h = p[i].h; return 1; }
    return 0;
}

int prn_clamp_range(int n, int all, int first, int last, int *a, int *b) {
    if (n <= 0) return 0;
    if (all) { *a = 1; *b = n; return 1; }
    if (first < 1) first = 1;
    if (last > n) last = n;
    if (first > last) return 0;
    *a = first; *b = last;
    return 1;
}
