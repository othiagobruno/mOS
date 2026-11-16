/* mOS libSystem — núcleo sem ObjC de appkit_more.c (ver appkit_more_core.h). Sem libc: roda no libSystem e no host. */
#include "appkit_more_core.h"

static double clamp01(double p) { return p < 0 ? 0 : p > 1 ? 1 : p; }

double amc_curve(int curve, double p) {
    p = clamp01(p);
    switch (curve) {
    case 1: return p * p;                                          /* EaseIn: começa devagar */
    case 2: return 1 - (1 - p) * (1 - p);                          /* EaseOut: termina devagar */
    case 3: return p;                                              /* Linear */
    default: return p < 0.5 ? 2 * p * p : 1 - 2 * (1 - p) * (1 - p);   /* EaseInOut */
    }
}

/* ================================================================ gestos */
void gr_init(gr_core *g, int kind) {
    g->kind = kind; g->state = GR_POSSIBLE; g->down = 0;
    g->button_mask = kind == GR_K_PRESS || kind == GR_K_CLICK || kind == GR_K_PAN ? 1 : 0;
    g->clicks_required = 1;
    g->min_duration = 0.5;
    g->allowable = kind == GR_K_CLICK ? 4 : 10;
    g->x0 = g->y0 = g->t0 = g->x = g->y = 0;
    g->magnification = 0; g->rotation = 0;
}
static double dist2(gr_core *g) { double dx = g->x - g->x0, dy = g->y - g->y0; return dx * dx + dy * dy; }
static int bit_ok(gr_core *g, int button) { return (g->button_mask & (1 << button)) != 0; }

int gr_feed(gr_core *g, int ev, double x, double y, double t, int button, int clicks, double amount, int phase_end) {
    if (g->state == GR_FAILED || g->state == GR_ENDED || g->state == GR_CANCELLED) return 0;
    if (ev == GE_DOWN || ev == GE_DRAG || ev == GE_UP) { g->x = x; g->y = y; }
    switch (g->kind) {
    case GR_K_PAN:
        if (ev == GE_DOWN) { if (!bit_ok(g, button)) return 0; g->down = 1; g->x0 = x; g->y0 = y; g->t0 = t; return 0; }
        if (!g->down) return 0;
        if (ev == GE_DRAG) {
            if (g->state == GR_POSSIBLE) { if (dist2(g) >= 2 * 2) { g->state = GR_BEGAN; return 1; } return 0; }
            g->state = GR_CHANGED; return 1;
        }
        if (ev == GE_UP) {
            g->down = 0;
            if (g->state == GR_BEGAN || g->state == GR_CHANGED) { g->state = GR_ENDED; return 1; }
            g->state = GR_FAILED; return 0;
        }
        return 0;
    case GR_K_PRESS:
        if (ev == GE_DOWN) { if (!bit_ok(g, button)) return 0; g->down = 1; g->x0 = x; g->y0 = y; g->t0 = t; return 0; }
        if (!g->down) return 0;
        if (g->state == GR_POSSIBLE) {
            if (ev == GE_DRAG && dist2(g) > g->allowable * g->allowable) { g->state = GR_FAILED; g->down = 0; return 0; }
            if (ev == GE_UP) { g->state = GR_FAILED; g->down = 0; return 0; }
            if ((ev == GE_TICK || ev == GE_DRAG) && t - g->t0 >= g->min_duration) { g->state = GR_BEGAN; return 1; }
            return 0;
        }
        if (ev == GE_DRAG) { g->state = GR_CHANGED; return 1; }
        if (ev == GE_UP) { g->down = 0; g->state = GR_ENDED; return 1; }
        return 0;
    case GR_K_CLICK:   /* discreto: reconhece (Ended) no soltar do clique de número certo, sem sair do lugar */
        if (ev == GE_DOWN) { if (!bit_ok(g, button)) return 0; g->down = 1; g->x0 = x; g->y0 = y; g->t0 = t; return 0; }
        if (!g->down) return 0;
        if (ev == GE_DRAG && dist2(g) > g->allowable * g->allowable) { g->state = GR_FAILED; g->down = 0; return 0; }
        if (ev == GE_UP) {
            g->down = 0;
            if (clicks >= g->clicks_required && dist2(g) <= g->allowable * g->allowable) { g->state = GR_ENDED; return 1; }
            if (clicks > g->clicks_required) g->state = GR_FAILED;
            return 0;
        }
        return 0;
    case GR_K_MAGNIFY:
    case GR_K_ROTATE: {
        int want = g->kind == GR_K_MAGNIFY ? GE_MAG : GE_ROT;
        if (ev != want) return 0;
        if (g->kind == GR_K_MAGNIFY) g->magnification += amount; else g->rotation += amount;
        g->x = x; g->y = y;
        if (phase_end) {
            if (g->state == GR_BEGAN || g->state == GR_CHANGED) { g->state = GR_ENDED; return 1; }
            g->state = GR_FAILED; return 0;
        }
        g->state = g->state == GR_POSSIBLE ? GR_BEGAN : GR_CHANGED;
        return 1;
    }
    }
    return 0;
}
void gr_reset_if_done(gr_core *g) {
    if (g->state == GR_ENDED || g->state == GR_CANCELLED || g->state == GR_FAILED) {
        if (g->down) return;   /* falhou no meio do clique: só volta quando o botão soltar */
        g->state = GR_POSSIBLE; g->magnification = 0; g->rotation = 0;
    }
}

/* ================================================================ NSMatrix */
int mx_hit(const mx_geom *g, double x, double y, int *row, int *col) {
    if (g->rows <= 0 || g->cols <= 0 || x < 0 || y < 0) return 0;
    double pw = g->cw + g->sx, ph = g->ch + g->sy;
    if (pw <= 0 || ph <= 0) return 0;
    int c = (int)(x / pw), r = (int)(y / ph);
    if (c >= g->cols || r >= g->rows) return 0;
    if (x - c * pw > g->cw || y - r * ph > g->ch) return 0;
    *row = r; *col = c;
    return 1;
}
void mx_cell_frame(const mx_geom *g, int row, int col, double *x, double *y, double *w, double *h) {
    *x = col * (g->cw + g->sx); *y = row * (g->ch + g->sy); *w = g->cw; *h = g->ch;
}
void mx_click(int mode, int allows_empty, unsigned char *sel, int n, int idx, int mods, int *anchor) {
    if (idx < 0 || idx >= n) return;
    if (mode == 0) {                                   /* rádio: exatamente uma (ou nenhuma, se permitido) */
        int was = sel[idx];
        for (int i = 0; i < n; i++) sel[i] = 0;
        sel[idx] = (was && allows_empty) ? 0 : 1;
        *anchor = idx;
        return;
    }
    if (mode == 2) {                                   /* lista: shift estende da âncora, ⌘ alterna */
        if (mods & 1) {
            int a = *anchor < 0 ? idx : *anchor, lo = a < idx ? a : idx, hi = a < idx ? idx : a;
            for (int i = 0; i < n; i++) sel[i] = (i >= lo && i <= hi);
            return;
        }
        if (mods & 2) {
            sel[idx] = !sel[idx];
            if (!allows_empty) { int any = 0; for (int i = 0; i < n; i++) any |= sel[i]; if (!any) sel[idx] = 1; }
            *anchor = idx;
            return;
        }
        for (int i = 0; i < n; i++) sel[i] = 0;
        sel[idx] = 1; *anchor = idx;
        return;
    }
    /* realce (1) e trilha (3): a célula clicada fica selecionada sozinha */
    for (int i = 0; i < n; i++) sel[i] = 0;
    sel[idx] = 1; *anchor = idx;
}

/* ================================================================ slider / stepper */
double sl_value_at(double pos, double track_start, double track_len, double minv, double maxv) {
    if (track_len <= 0) return minv;
    return minv + clamp01((pos - track_start) / track_len) * (maxv - minv);
}
double sl_closest_tick(double v, double minv, double maxv, int ticks) {
    if (ticks <= 0) return v;
    if (ticks == 1) return (minv + maxv) / 2;
    double step = (maxv - minv) / (ticks - 1);
    if (step == 0) return minv;
    double k = (v - minv) / step;
    long i = (long)(k + (k >= 0 ? 0.5 : -0.5));
    if (i < 0) i = 0;
    if (i > ticks - 1) i = ticks - 1;
    return minv + i * step;
}
double sl_knob_pos(double v, double track_start, double track_len, double minv, double maxv) {
    if (maxv == minv) return track_start;
    return track_start + clamp01((v - minv) / (maxv - minv)) * track_len;
}
double st_step(double v, double inc, double minv, double maxv, int up, int wraps) {
    double n = v + (up ? inc : -inc);
    if (n > maxv) n = wraps ? minv : maxv;
    if (n < minv) n = wraps ? maxv : minv;
    return n;
}

/* ================================================================ EPS */
static unsigned long le32(const unsigned char *p) { return p[0] | (p[1] << 8) | ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24); }
int eps_dos_header(const unsigned char *b, unsigned long n, unsigned long *ps_off, unsigned long *ps_len,
                   unsigned long *tiff_off, unsigned long *tiff_len) {
    if (n < 30 || b[0] != 0xC5 || b[1] != 0xD0 || b[2] != 0xD3 || b[3] != 0xC6) return 0;
    *ps_off = le32(b + 4); *ps_len = le32(b + 8);
    *tiff_off = le32(b + 20); *tiff_len = le32(b + 24);
    if (*ps_off > n || *ps_len > n - *ps_off) return 0;
    if (*tiff_off > n || *tiff_len > n - *tiff_off) *tiff_off = *tiff_len = 0;
    return 1;
}
static const unsigned char *num(const unsigned char *p, const unsigned char *e, double *out) {
    while (p < e && (*p == ' ' || *p == '\t')) p++;
    int neg = 0, any = 0;
    if (p < e && (*p == '-' || *p == '+')) neg = *p++ == '-';
    double v = 0, f = 0.1;
    while (p < e && *p >= '0' && *p <= '9') { v = v * 10 + (*p++ - '0'); any = 1; }
    if (p < e && *p == '.') { p++; while (p < e && *p >= '0' && *p <= '9') { v += (*p++ - '0') * f; f /= 10; any = 1; } }
    if (!any) return 0;
    *out = neg ? -v : v;
    return p;
}
static int bbox_line(const unsigned char *ps, unsigned long n, const char *tag, double *x, double *y, double *w, double *h) {
    unsigned long tl = 0; while (tag[tl]) tl++;
    for (unsigned long i = 0; i + tl <= n; i++) {
        if (i && ps[i - 1] != '\n' && ps[i - 1] != '\r') continue;
        unsigned long k = 0; while (k < tl && ps[i + k] == (unsigned char)tag[k]) k++;
        if (k < tl) continue;
        const unsigned char *p = ps + i + tl, *e = ps + n;
        double v[4];
        int j = 0;
        for (; j < 4; j++) { p = num(p, e, &v[j]); if (!p) break; }
        if (j < 4) continue;                         /* "(atend)": o valor vem no fim; procura a próxima ocorrência */
        *x = v[0]; *y = v[1]; *w = v[2] - v[0]; *h = v[3] - v[1];
        return 1;
    }
    return 0;
}
int eps_bbox(const unsigned char *ps, unsigned long n, double *x, double *y, double *w, double *h) {
    return bbox_line(ps, n, "%%HiResBoundingBox:", x, y, w, h) || bbox_line(ps, n, "%%BoundingBox:", x, y, w, h);
}
