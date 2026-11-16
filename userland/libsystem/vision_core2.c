/* mOS — algoritmos de Vision sem aprendizado de máquina (ver vision_core2.h). Código original do mOS (MIT). */
#include "vision_core2.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
static float hyp(float a, float b) { return (float)sqrt((double)a * a + (double)b * b); }

/* ---------------------------------------------------------------- contornos */
static int otsu(const uint8_t *y, long n) {
    long hist[256] = {0}, sum = 0, sb = 0, wb = 0;
    for (long i = 0; i < n; i++) hist[y[i]]++;
    for (int i = 0; i < 256; i++) sum += (long)i * hist[i];
    double best = -1; int t = 128;
    for (int k = 0; k < 256; k++) {
        wb += hist[k];
        if (!wb || wb == n) continue;
        sb += (long)k * hist[k];
        double mb = (double)sb / wb, mf = (double)(sum - sb) / (n - wb), v = (double)wb * (n - wb) * (mb - mf) * (mb - mf);
        if (v > best) { best = v; t = k; }
    }
    return t;
}
/* rótulos de componentes (fg em 8-vizinhança, fundo em 4) por inundação com pilha */
static int label(const uint8_t *fg, int w, int h, int val, int conn8, int *lab, int first) {
    int next = first;
    int *stack = malloc(sizeof(int) * (size_t)w * h);
    if (!stack) return first;
    for (int i = 0; i < w * h; i++) {
        if (fg[i] != val || lab[i]) continue;
        int sp = 0;
        stack[sp++] = i; lab[i] = next;
        while (sp) {
            int p = stack[--sp], x = p % w, y = p / w;
            for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
                if (!dx && !dy) continue;
                if (!conn8 && dx && dy) continue;
                int nx = x + dx, ny = y + dy;
                if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
                int q = ny * w + nx;
                if (fg[q] == val && !lab[q]) { lab[q] = next; stack[sp++] = q; }
            }
        }
        next++;
    }
    free(stack);
    return next;
}
/* borda de Moore da região 'id' a partir do seu primeiro pixel (varredura de cima para baixo, esquerda para direita) */
static int trace(const int *lab, int w, int h, int id, int start, float **pts) {
    static const int DX[8] = {1, 1, 0, -1, -1, -1, 0, 1}, DY[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    int cap = 64, n = 0;
    float *p = malloc(sizeof(float) * 2 * cap);
    if (!p) return 0;
    int x = start % w, y = start / w, dir = 7, sx = x, sy = y, steps = 0;
    do {
        if (n == cap) { cap *= 2; float *q = realloc(p, sizeof(float) * 2 * cap); if (!q) break; p = q; }
        p[2 * n] = x + 0.5f; p[2 * n + 1] = y + 0.5f; n++;
        int found = 0;
        for (int k = 0; k < 8; k++) {
            int d = (dir + 6 + k) % 8, nx = x + DX[d], ny = y + DY[d];   /* começa à esquerda de onde veio */
            if (nx >= 0 && ny >= 0 && nx < w && ny < h && lab[ny * w + nx] == id) { x = nx; y = ny; dir = d; found = 1; break; }
        }
        if (!found) break;   /* pixel isolado */
    } while ((x != sx || y != sy) && ++steps < 4 * w * h);
    *pts = p;
    return n;
}
int vc_find_contours(const uint8_t *y, int w, int h, int dark, float contrast, vc_contours *out) {
    out->c = 0; out->n = 0;
    if (!y || w < 2 || h < 2) return -1;
    long N = (long)w * h;
    /* contraste: estica em torno de 128 antes do limiar */
    uint8_t *adj = malloc((size_t)N), *fg = malloc((size_t)N);
    int *lab = calloc((size_t)N, sizeof(int));
    if (!adj || !fg || !lab) { free(adj); free(fg); free(lab); return -1; }
    float k = contrast > 0 ? contrast : 1;
    for (long i = 0; i < N; i++) { float v = (y[i] - 128) * k + 128; adj[i] = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v); }
    int t = otsu(adj, N);
    for (long i = 0; i < N; i++) fg[i] = dark ? adj[i] <= t : adj[i] > t;
    int nfg = label(fg, w, h, 1, 1, lab, 1);        /* 1..nfg-1: primeiro plano */
    int nall = label(fg, w, h, 0, 0, lab, nfg);     /* nfg..: fundo */
    int nreg = nall;
    /* pai de cada região: a região à esquerda do primeiro pixel (fora da imagem = fundo externo, 0) */
    int *first = malloc(sizeof(int) * (size_t)nreg), *parent = malloc(sizeof(int) * (size_t)nreg), *border = calloc((size_t)nreg, sizeof(int));
    if (!first || !parent || !border) { free(first); free(parent); free(border); free(adj); free(fg); free(lab); return -1; }
    for (int i = 0; i < nreg; i++) { first[i] = -1; parent[i] = 0; }
    for (long i = 0; i < N; i++) {
        int L = lab[i], x = (int)(i % w), yy = (int)(i / w);
        if (first[L] < 0) { first[L] = (int)i; parent[L] = x > 0 ? lab[i - 1] : 0; }
        if (x == 0 || yy == 0 || x == w - 1 || yy == h - 1) border[L] = 1;
    }
    /* fundo que toca a borda é o lado de fora: não é buraco; seus "filhos" são contornos de topo */
    int *cid = malloc(sizeof(int) * (size_t)nreg);
    vc_contour *cs = calloc((size_t)nreg, sizeof(vc_contour));
    int nc = 0;
    if (!cid || !cs) { free(cid); free(cs); free(first); free(parent); free(border); free(adj); free(fg); free(lab); return -1; }
    for (int L = 1; L < nreg; L++) {
        cid[L] = -1;
        if (first[L] < 0) continue;
        int hole = L >= nfg;
        if (hole && border[L]) continue;
        cs[nc].is_hole = hole;
        cs[nc].npts = trace(lab, w, h, L, first[L], &cs[nc].pts);
        cs[nc].parent = parent[L];   /* região; vira índice de contorno abaixo */
        cid[L] = nc++;
    }
    for (int i = 0; i < nc; i++) {
        int pr = cs[i].parent;
        cs[i].parent = pr > 0 && pr < nreg ? cid[pr] : -1;
    }
    /* ordem de pais antes dos filhos já vale: o primeiro pixel do pai vem antes na varredura */
    out->c = cs; out->n = nc;
    free(cid); free(first); free(parent); free(border); free(adj); free(fg); free(lab);
    return 0;
}
void vc_free_contours(vc_contours *c) { for (int i = 0; i < c->n; i++) free(c->c[i].pts); free(c->c); c->c = 0; c->n = 0; }

static void dp(const float *p, int a, int b, float eps, uint8_t *keep) {
    if (b <= a + 1) return;
    float ax = p[2 * a], ay = p[2 * a + 1], bx = p[2 * b], by = p[2 * b + 1], dx = bx - ax, dy = by - ay, len = hyp(dx, dy);
    float best = -1; int bi = -1;
    for (int i = a + 1; i < b; i++) {
        float d = len > 0 ? (float)fabs(dy * p[2 * i] - dx * p[2 * i + 1] + bx * ay - by * ax) / len : hyp(p[2 * i] - ax, p[2 * i + 1] - ay);
        if (d > best) { best = d; bi = i; }
    }
    if (best > eps) { keep[bi] = 1; dp(p, a, bi, eps, keep); dp(p, bi, b, eps, keep); }
}
int vc_simplify(const float *p, int n, float eps, float *out) {
    if (n <= 2) { memcpy(out, p, sizeof(float) * 2 * n); return n; }
    uint8_t *keep = calloc((size_t)n, 1);
    if (!keep) return 0;
    /* contorno fechado: corta no ponto mais longe do primeiro */
    int far = 0; float fd = -1;
    for (int i = 1; i < n; i++) { float d = hyp(p[2 * i] - p[0], p[2 * i + 1] - p[1]); if (d > fd) { fd = d; far = i; } }
    keep[0] = keep[far] = 1;
    dp(p, 0, far, eps, keep);
    float *ext = malloc(sizeof(float) * 2 * (n - far + 1));
    if (ext) {
        memcpy(ext, p + 2 * far, sizeof(float) * 2 * (n - far));
        ext[2 * (n - far)] = p[0]; ext[2 * (n - far) + 1] = p[1];
        uint8_t *k2 = calloc((size_t)(n - far + 1), 1);
        if (k2) { dp(ext, 0, n - far, eps, k2); for (int i = 1; i < n - far; i++) if (k2[i]) keep[far + i] = 1; free(k2); }
        free(ext);
    }
    int m = 0;
    for (int i = 0; i < n; i++) if (keep[i]) { out[2 * m] = p[2 * i]; out[2 * m + 1] = p[2 * i + 1]; m++; }
    free(keep);
    return m;
}

/* ---------------------------------------------------------------- saliência */
void vc_saliency(const uint8_t *y, int w, int h, float *out, int ow, int oh, float *box) {
    double mean = 0;
    for (long i = 0; i < (long)w * h; i++) mean += y[i];
    mean /= (double)w * h;
    float mx = 0;
    for (int oy = 0; oy < oh; oy++)
        for (int ox = 0; ox < ow; ox++) {   /* média do bloco (suaviza) */
            int x0 = ox * w / ow, x1 = (ox + 1) * w / ow, y0 = oy * h / oh, y1 = (oy + 1) * h / oh;
            if (x1 <= x0) x1 = x0 + 1;
            if (y1 <= y0) y1 = y0 + 1;
            double s = 0; int c = 0;
            for (int yy = y0; yy < y1 && yy < h; yy++) for (int xx = x0; xx < x1 && xx < w; xx++) { s += y[yy * w + xx]; c++; }
            float v = (float)fabs((c ? s / c : mean) - mean);
            out[oy * ow + ox] = v;
            if (v > mx) mx = v;
        }
    int bx0 = ow, by0 = oh, bx1 = -1, by1 = -1;
    for (int i = 0; i < ow * oh; i++) {
        out[i] = mx > 0 ? out[i] / mx : 0;
        if (out[i] >= 0.5f) { int x = i % ow, yy = i / ow; if (x < bx0) bx0 = x; if (x > bx1) bx1 = x; if (yy < by0) by0 = yy; if (yy > by1) by1 = yy; }
    }
    if (box) {
        if (bx1 < 0) { box[0] = box[1] = box[2] = box[3] = 0; }
        else { box[0] = (float)bx0 / ow; box[2] = (float)(bx1 + 1 - bx0) / ow; box[3] = (float)(by1 + 1 - by0) / oh; box[1] = 1 - (float)(by1 + 1) / oh; }
    }
}

/* ---------------------------------------------------------------- translação */
static double sad(const uint8_t *a, const uint8_t *b, int w, int h, int dx, int dy) {
    long s = 0, n = 0;
    for (int y = 0; y < h; y++) {
        int y2 = y + dy;
        if (y2 < 0 || y2 >= h) continue;
        for (int x = 0; x < w; x++) {
            int x2 = x + dx;
            if (x2 < 0 || x2 >= w) continue;
            s += abs((int)a[y * w + x] - (int)b[y2 * w + x2]); n++;
        }
    }
    return n > (long)w * h / 4 ? (double)s / n : 1e18;   /* exige sobreposição de um quarto */
}
static uint8_t *half(const uint8_t *s, int w, int h) {
    int hw = w / 2, hh = h / 2;
    uint8_t *d = malloc((size_t)hw * hh);
    if (!d) return 0;
    for (int y = 0; y < hh; y++) for (int x = 0; x < hw; x++)
        d[y * hw + x] = (uint8_t)((s[2 * y * w + 2 * x] + s[2 * y * w + 2 * x + 1] + s[(2 * y + 1) * w + 2 * x] + s[(2 * y + 1) * w + 2 * x + 1] + 2) / 4);
    return d;
}
/* tgt(x) ≈ ref(x + d): o alvo deslocado de -d volta à referência */
int vc_translation(const uint8_t *ref, const uint8_t *tgt, int w, int h, int *dx, int *dy) {
    if (w < 8 || h < 8) return -1;
    if (w >= 64 && h >= 64) {
        uint8_t *r2 = half(ref, w, h), *t2 = half(tgt, w, h);
        int cx = 0, cy = 0, rc = r2 && t2 ? vc_translation(r2, t2, w / 2, h / 2, &cx, &cy) : -1;
        free(r2); free(t2);
        if (rc) return rc;
        double best = 1e18; int bx = 2 * cx, by = 2 * cy;
        for (int y = 2 * cy - 2; y <= 2 * cy + 2; y++) for (int x = 2 * cx - 2; x <= 2 * cx + 2; x++) { double s = sad(tgt, ref, w, h, x, y); if (s < best) { best = s; bx = x; by = y; } }
        *dx = bx; *dy = by;
        return 0;
    }
    double best = 1e18; int bx = 0, by = 0;
    for (int y = -h / 2; y <= h / 2; y++) for (int x = -w / 2; x <= w / 2; x++) { double s = sad(tgt, ref, w, h, x, y); if (s < best) { best = s; bx = x; by = y; } }
    *dx = bx; *dy = by;
    return 0;
}

/* ---------------------------------------------------------------- rastreamento */
float vc_track(const uint8_t *img, int w, int h, const uint8_t *t, int tw, int th, int px, int py, int r, int *bx, int *by) {
    double tm = 0, tv = 0;
    for (int i = 0; i < tw * th; i++) tm += t[i];
    tm /= tw * th;
    for (int i = 0; i < tw * th; i++) tv += (t[i] - tm) * (t[i] - tm);
    float best = -2;
    *bx = px; *by = py;
    for (int oy = py - r; oy <= py + r; oy++)
        for (int ox = px - r; ox <= px + r; ox++) {
            if (ox < 0 || oy < 0 || ox + tw > w || oy + th > h) continue;
            double im = 0, iv = 0, cv = 0;
            for (int y = 0; y < th; y++) for (int x = 0; x < tw; x++) im += img[(oy + y) * w + ox + x];
            im /= tw * th;
            for (int y = 0; y < th; y++) for (int x = 0; x < tw; x++) {
                double a = img[(oy + y) * w + ox + x] - im, b = t[y * tw + x] - tm;
                iv += a * a; cv += a * b;
            }
            float ncc = iv > 0 && tv > 0 ? (float)(cv / sqrt(iv * tv)) : (iv == 0 && tv == 0 ? 1.0f : 0.0f);
            if (ncc > best) { best = ncc; *bx = ox; *by = oy; }
        }
    return best;
}
