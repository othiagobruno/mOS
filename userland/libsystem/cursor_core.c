/* mOS — imagens de cursor (ver cursor_core.h). Código original do mOS. */
#include "cursor_core.h"

typedef struct { double x, y; } pt;

/* compõe a cor c (0xAARRGGBB, não pré-multiplicada) com cobertura cov (0..1) sobre o pixel */
static void over(uint32_t *d, uint32_t c, double cov) {
    double sa = ((c >> 24) & 255) / 255.0 * cov;
    if (sa <= 0) return;
    uint32_t o = *d;
    double da = ((o >> 24) & 255) / 255.0, ra = sa + da * (1 - sa);
    double rc[3];
    for (int k = 0; k < 3; k++) {
        int sh = 16 - 8 * k;
        double s = ((c >> sh) & 255) / 255.0, t = ((o >> sh) & 255) / 255.0;
        rc[k] = ra > 0 ? (s * sa + t * da * (1 - sa)) / ra : 0;
    }
    *d = ((uint32_t)(ra * 255 + 0.5) << 24) | ((uint32_t)(rc[0] * 255 + 0.5) << 16) | ((uint32_t)(rc[1] * 255 + 0.5) << 8) |
         (uint32_t)(rc[2] * 255 + 0.5);
}

/* ponto dentro do polígono (regra par-ímpar) */
static int inside(const pt *p, int n, double x, double y) {
    int in = 0;
    for (int i = 0, j = n - 1; i < n; j = i++)
        if ((p[i].y > y) != (p[j].y > y) && x < (p[j].x - p[i].x) * (y - p[i].y) / (p[j].y - p[i].y) + p[i].x) in = !in;
    return in;
}
static void fill_poly(uint32_t *o, const pt *p, int n, uint32_t c) {
    double x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9;
    for (int i = 0; i < n; i++) {
        if (p[i].x < x0) x0 = p[i].x;
        if (p[i].x > x1) x1 = p[i].x;
        if (p[i].y < y0) y0 = p[i].y;
        if (p[i].y > y1) y1 = p[i].y;
    }
    int ix0 = x0 < 0 ? 0 : (int)x0, iy0 = y0 < 0 ? 0 : (int)y0;
    int ix1 = x1 > CUR_PX - 1 ? CUR_PX - 1 : (int)x1, iy1 = y1 > CUR_PX - 1 ? CUR_PX - 1 : (int)y1;
    for (int y = iy0; y <= iy1; y++)
        for (int x = ix0; x <= ix1; x++) {
            int hit = 0;
            for (int sy = 0; sy < 4; sy++)
                for (int sx = 0; sx < 4; sx++) hit += inside(p, n, x + (sx + 0.5) / 4, y + (sy + 0.5) / 4);
            if (hit) over(&o[y * CUR_PX + x], c, hit / 16.0);
        }
}
static void fill_circle(uint32_t *o, double cx, double cy, double r, uint32_t c) {
    for (int y = (int)(cy - r - 1); y <= (int)(cy + r + 1); y++)
        for (int x = (int)(cx - r - 1); x <= (int)(cx + r + 1); x++) {
            if (x < 0 || y < 0 || x >= CUR_PX || y >= CUR_PX) continue;
            int hit = 0;
            for (int sy = 0; sy < 4; sy++)
                for (int sx = 0; sx < 4; sx++) {
                    double dx = x + (sx + 0.5) / 4 - cx, dy = y + (sy + 0.5) / 4 - cy;
                    hit += dx * dx + dy * dy <= r * r;
                }
            if (hit) over(&o[y * CUR_PX + x], c, hit / 16.0);
        }
}
/* traço reto de espessura w com pontas redondas */
static void line(uint32_t *o, double x0, double y0, double x1, double y1, double w, uint32_t c) {
    double dx = x1 - x0, dy = y1 - y0, l = dx * dx + dy * dy;
    double len = 1;
    if (l > 0) { len = l; double g = len; for (int i = 0; i < 20; i++) g = 0.5 * (g + len / g); len = g; }   /* raiz sem libm */
    double nx = -dy / len * w / 2, ny = dx / len * w / 2;
    pt q[4] = {{x0 + nx, y0 + ny}, {x1 + nx, y1 + ny}, {x1 - nx, y1 - ny}, {x0 - nx, y0 - ny}};
    fill_poly(o, q, 4, c);
    fill_circle(o, x0, y0, w / 2, c);
    fill_circle(o, x1, y1, w / 2, c);
}

#define BLACK 0xFF000000u
#define WHITE 0xFFFFFFFFu

/* seta com contorno branco, ponta em (2, 1) */
static const pt ARROW[] = {{2, 1}, {2, 19}, {6.5, 15}, {9.5, 22}, {12.5, 20.8}, {9.6, 14}, {15.5, 14}};
static void arrow(uint32_t *o) {
    int n = (int)(sizeof ARROW / sizeof ARROW[0]);
    for (int i = 0, j = n - 1; i < n; j = i++) line(o, ARROW[j].x, ARROW[j].y, ARROW[i].x, ARROW[i].y, 3, WHITE);
    fill_poly(o, ARROW, n, BLACK);
}

int cur_draw(int form, uint32_t *out, int *hx, int *hy) {
    for (int i = 0; i < CUR_PX * CUR_PX; i++) out[i] = 0;
    switch (form) {
    case CUR_NOT_ALLOWED:   /* seta e o sinal de proibido: anel vermelho cortado */
        arrow(out);
        fill_circle(out, 23.5, 23.5, 8, WHITE);
        fill_circle(out, 23.5, 23.5, 6.6, 0xFFE0352Bu);
        fill_circle(out, 23.5, 23.5, 4.6, WHITE);
        line(out, 20.2, 20.2, 26.8, 26.8, 2, 0xFFE0352Bu);
        *hx = 2; *hy = 1;
        return 1;
    case CUR_LINK: {        /* seta e a flecha curva do atalho num quadrado branco */
        arrow(out);
        pt box[4] = {{16, 16}, {31, 16}, {31, 31}, {16, 31}};
        pt in[4] = {{17, 17}, {30, 17}, {30, 30}, {17, 30}};
        fill_poly(out, box, 4, BLACK);
        fill_poly(out, in, 4, WHITE);
        line(out, 20.5, 27, 20.5, 23.5, 1.6, BLACK);
        line(out, 20.5, 23.5, 26, 23.5, 1.6, BLACK);
        pt head[3] = {{28, 23.5}, {24.5, 20.5}, {24.5, 26.5}};
        fill_poly(out, head, 3, BLACK);
        *hx = 2; *hy = 1;
        return 1;
    }
    case CUR_CONTEXT: {     /* seta e um menu pequeno de três linhas */
        arrow(out);
        pt box[4] = {{16, 14}, {31, 14}, {31, 30}, {16, 30}};
        pt in[4] = {{17, 15}, {30, 15}, {30, 29}, {17, 29}};
        fill_poly(out, box, 4, BLACK);
        fill_poly(out, in, 4, WHITE);
        for (int k = 0; k < 3; k++) line(out, 19.5, 18.5 + 3.5 * k, 27.5, 18.5 + 3.5 * k, 1.4, BLACK);
        *hx = 2; *hy = 1;
        return 1;
    }
    case CUR_POOF: {        /* nuvem: o item some quando é solto */
        static const double C[][3] = {{9, 18, 6}, {16, 12, 7.5}, {23.5, 17, 6.5}, {15.5, 21, 6.5}};
        for (int k = 0; k < 4; k++) fill_circle(out, C[k][0], C[k][1], C[k][2] + 1.3, 0xFF6E6E73u);
        for (int k = 0; k < 4; k++) fill_circle(out, C[k][0], C[k][1], C[k][2], WHITE);
        *hx = 16; *hy = 16;
        return 1;
    }
    case CUR_IBEAM_VERTICAL:   /* o I-beam deitado: haste horizontal com as serifas nas pontas */
        line(out, 9.5, 16.5, 23.5, 16.5, 3.4, WHITE);   /* centros de pixel: a haste fica nítida */
        line(out, 9.5, 12.5, 9.5, 20.5, 3.4, WHITE);
        line(out, 23.5, 12.5, 23.5, 20.5, 3.4, WHITE);
        line(out, 9.5, 16.5, 23.5, 16.5, 1.4, BLACK);
        line(out, 9.5, 12.5, 9.5, 20.5, 1.4, BLACK);
        line(out, 23.5, 12.5, 23.5, 20.5, 1.4, BLACK);
        *hx = 16; *hy = 16;
        return 1;
    }
    return 0;
}

int cur_fit(const uint32_t *src, int sw, int sh, uint32_t *out, int *dw, int *dh, double *hx, double *hy) {
    if (!src || sw <= 0 || sh <= 0) return 0;
    double s = 1;
    if (sw > CUR_PX) s = (double)CUR_PX / sw;
    if (sh * s > CUR_PX) s = (double)CUR_PX / sh;
    int w = (int)(sw * s + 0.5), h = (int)(sh * s + 0.5);
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (w > CUR_PX) w = CUR_PX;
    if (h > CUR_PX) h = CUR_PX;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            /* caixa de origem do pixel de destino (área), acumulando em cor pré-multiplicada */
            int x0 = (int)((double)x * sw / w), x1 = (int)((double)(x + 1) * sw / w);
            int y0 = (int)((double)y * sh / h), y1 = (int)((double)(y + 1) * sh / h);
            if (x1 <= x0) x1 = x0 + 1;
            if (y1 <= y0) y1 = y0 + 1;
            double a = 0, r = 0, g = 0, b = 0;
            int n = 0;
            for (int yy = y0; yy < y1 && yy < sh; yy++)
                for (int xx = x0; xx < x1 && xx < sw; xx++) {
                    uint32_t p = src[yy * sw + xx];
                    double pa = (p >> 24) / 255.0;
                    a += pa; r += pa * ((p >> 16) & 255); g += pa * ((p >> 8) & 255); b += pa * (p & 255);
                    n++;
                }
            uint32_t v = 0;
            if (n && a > 0) {
                v = ((uint32_t)(a / n * 255 + 0.5) << 24) | ((uint32_t)(r / a + 0.5) << 16) | ((uint32_t)(g / a + 0.5) << 8) |
                    (uint32_t)(b / a + 0.5);
            }
            out[y * w + x] = v;
        }
    *dw = w; *dh = h;
    if (hx) *hx *= (double)w / sw;
    if (hy) *hy *= (double)h / sh;
    return 1;
}
