/* mOS — leitor de QR Code (ver qrread.h). ISO/IEC 18004: 6.3 (padrões), 7.4 (modos), 7.5 (Reed-Solomon), 7.9 (formato),
 * 7.10 (versão). Código original do mOS (MIT). */
#include "qrread.h"
#include "qrgen.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct { double x, y; } pt;
static double dist(pt a, pt b) { return sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y)); }

/* ---------------------------------------------------------------- binarização: média local + Otsu */
static uint8_t *binarize(const uint8_t *g, int w, int h) {
    long hist[256] = {0};
    for (long i = 0; i < (long)w * h; i++) hist[g[i]]++;
    long total = (long)w * h, sum = 0, sb = 0, wb = 0;
    for (int i = 0; i < 256; i++) sum += (long)i * hist[i];
    double best = -1; int otsu = 128;
    for (int t = 0; t < 256; t++) {
        wb += hist[t];
        if (!wb || wb == total) continue;
        sb += (long)t * hist[t];
        double mb = (double)sb / wb, mf = (double)(sum - sb) / (total - wb), v = (double)wb * (total - wb) * (mb - mf) * (mb - mf);
        if (v > best) { best = v; otsu = t; }
    }
    uint32_t *in = malloc(sizeof(uint32_t) * (size_t)(w + 1) * (h + 1));
    uint8_t *b = malloc((size_t)w * h);
    if (!in || !b) { free(in); free(b); return 0; }
    for (int x = 0; x <= w; x++) in[x] = 0;
    for (int y = 1; y <= h; y++) {
        uint32_t row = 0;
        in[(size_t)y * (w + 1)] = 0;
        for (int x = 1; x <= w; x++) { row += g[(size_t)(y - 1) * w + x - 1]; in[(size_t)y * (w + 1) + x] = in[(size_t)(y - 1) * (w + 1) + x] + row; }
    }
    int r = (w < h ? w : h) / 8;
    if (r < 8) r = 8;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int x0 = x - r < 0 ? 0 : x - r, x1 = x + r + 1 > w ? w : x + r + 1, y0 = y - r < 0 ? 0 : y - r, y1 = y + r + 1 > h ? h : y + r + 1;
            uint32_t s = in[(size_t)y1 * (w + 1) + x1] - in[(size_t)y0 * (w + 1) + x1] - in[(size_t)y1 * (w + 1) + x0] + in[(size_t)y0 * (w + 1) + x0];
            int mean = (int)(s / (uint32_t)((x1 - x0) * (y1 - y0)));
            b[(size_t)y * w + x] = g[(size_t)y * w + x] < (mean + otsu) / 2;
        }
    free(in);
    return b;
}

/* ---------------------------------------------------------------- padrões de localização (1:1:3:1:1) */
typedef struct { pt c; double m; int n; } cand;
static int ratio_ok(const int *c, double *module) {
    int t = c[0] + c[1] + c[2] + c[3] + c[4];
    if (t < 7) return 0;
    double m = t / 7.0, v = m * 0.7;
    if (fabs(c[0] - m) > v || fabs(c[1] - m) > v || fabs(c[2] - 3 * m) > 3 * v || fabs(c[3] - m) > v || fabs(c[4] - m) > v) return 0;
    *module = m;
    return 1;
}
#define B(x, y) bin[(size_t)(y) * w + (x)]
/* conta as cinco faixas ao longo de (dx, dy) a partir do centro; devolve o novo centro ao longo do eixo ou -1 */
static double cross(const uint8_t *bin, int w, int h, int cx, int cy, int dx, int dy, double *module) {
    int c[5] = {0};
    int x = cx, y = cy;
    if (!B(x, y)) return -1;
    while (x >= 0 && y >= 0 && x < w && y < h && B(x, y)) { c[2]++; x -= dx; y -= dy; }
    while (x >= 0 && y >= 0 && x < w && y < h && !B(x, y)) { c[1]++; x -= dx; y -= dy; }
    while (x >= 0 && y >= 0 && x < w && y < h && B(x, y)) { c[0]++; x -= dx; y -= dy; }
    x = cx + dx; y = cy + dy;
    while (x >= 0 && y >= 0 && x < w && y < h && B(x, y)) { c[2]++; x += dx; y += dy; }
    while (x >= 0 && y >= 0 && x < w && y < h && !B(x, y)) { c[3]++; x += dx; y += dy; }
    while (x >= 0 && y >= 0 && x < w && y < h && B(x, y)) { c[4]++; x += dx; y += dy; }
    if (!ratio_ok(c, module)) return -1;
    int end = dx ? x : y;   /* posição logo depois da última faixa */
    return end - c[4] - c[3] - c[2] / 2.0;
}
static void add_cand(cand *cs, int *n, int max, pt p, double mm) {
    for (int k = 0; k < *n; k++)
        if (dist(cs[k].c, p) < mm * 2 && fabs(cs[k].m - mm) < mm * 0.5) {
            cs[k].c.x = (cs[k].c.x * cs[k].n + p.x) / (cs[k].n + 1);
            cs[k].c.y = (cs[k].c.y * cs[k].n + p.y) / (cs[k].n + 1);
            cs[k].m = (cs[k].m * cs[k].n + mm) / (cs[k].n + 1);
            cs[k].n++;
            return;
        }
    if (*n < max) { cs[*n].c = p; cs[*n].m = mm; cs[*n].n = 1; (*n)++; }
}
static int find_candidates(const uint8_t *bin, int w, int h, cand *cs, int max) {
    int n = 0;
    int *len = malloc(sizeof(int) * (size_t)(w + 1)), *start = malloc(sizeof(int) * (size_t)(w + 1));
    uint8_t *col = malloc((size_t)w + 1);
    if (!len || !start || !col) { free(len); free(start); free(col); return 0; }
    for (int y = 0; y < h; y++) {
        int nr = 0;
        for (int x = 0; x < w; x++) {   /* faixas da linha */
            int dark = B(x, y);
            if (nr && col[nr - 1] == dark) len[nr - 1]++;
            else { col[nr] = (uint8_t)dark; len[nr] = 1; start[nr] = x; nr++; }
        }
        for (int i = 0; i + 4 < nr; i++) {
            if (!col[i]) continue;
            double m;
            if (!ratio_ok(len + i, &m)) continue;
            double hx = start[i + 2] + len[i + 2] / 2.0, mv, mh;
            double vy = cross(bin, w, h, (int)hx, y, 0, 1, &mv);
            if (vy < 0) continue;
            double hx2 = cross(bin, w, h, (int)hx, (int)vy, 1, 0, &mh);
            if (hx2 < 0) continue;
            pt p = {hx2, vy};
            add_cand(cs, &n, max, p, (mv + mh + m) / 3);
        }
    }
    free(len); free(start); free(col);
    return n;
}
/* três candidatos em ângulo reto: tl é o vértice; tr e bl pela orientação */
static int pick_three(cand *cs, int n, pt *tl, pt *tr, pt *bl, double *m) {
    double best = 1e18;
    int found = 0;
    for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) for (int k = 0; k < n; k++) {
        if (i == j || i == k || j >= k) continue;
        cand *a = &cs[i], *b = &cs[j], *c = &cs[k];
        double mm = (a->m + b->m + c->m) / 3;
        if (fabs(a->m - mm) > mm * 0.4 || fabs(b->m - mm) > mm * 0.4 || fabs(c->m - mm) > mm * 0.4) continue;
        double ab = dist(a->c, b->c), ac = dist(a->c, c->c), bc = dist(b->c, c->c);
        if (ab < mm * 10 || ac < mm * 10) continue;
        double dot = (b->c.x - a->c.x) * (c->c.x - a->c.x) + (b->c.y - a->c.y) * (c->c.y - a->c.y);
        double cosv = fabs(dot) / (ab * ac), lr = ab > ac ? ab / ac : ac / ab;
        if (cosv > 0.25 || lr > 1.3) continue;
        double score = cosv * 10 + (lr - 1) * 5 + fabs(bc - sqrt(ab * ab + ac * ac)) / bc - (a->n + b->n + c->n) * 0.01;
        if (score < best) {
            best = score; found = 1;
            *tl = a->c; *m = mm;
            double cr = (b->c.x - a->c.x) * (c->c.y - a->c.y) - (b->c.y - a->c.y) * (c->c.x - a->c.x);
            if (cr > 0) { *tr = b->c; *bl = c->c; } else { *tr = c->c; *bl = b->c; }
        }
    }
    return found;
}

/* ---------------------------------------------------------------- perspectiva: (u,v) em módulos -> imagem */
typedef struct { double a[9]; } homog;
static int solve8(double M[8][9], double *x) {
    for (int c = 0; c < 8; c++) {
        int p = c;
        for (int r = c + 1; r < 8; r++) if (fabs(M[r][c]) > fabs(M[p][c])) p = r;
        if (fabs(M[p][c]) < 1e-12) return -1;
        if (p != c) for (int k = 0; k < 9; k++) { double t = M[c][k]; M[c][k] = M[p][k]; M[p][k] = t; }
        for (int r = 0; r < 8; r++) {
            if (r == c) continue;
            double f = M[r][c] / M[c][c];
            for (int k = c; k < 9; k++) M[r][k] -= f * M[c][k];
        }
    }
    for (int i = 0; i < 8; i++) x[i] = M[i][8] / M[i][i];
    return 0;
}
static int homog_make(const pt *src, const pt *dst, homog *H) {
    double M[8][9], x[8];
    for (int i = 0; i < 4; i++) {
        double u = src[i].x, v = src[i].y, X = dst[i].x, Y = dst[i].y;
        double r1[9] = {u, v, 1, 0, 0, 0, -u * X, -v * X, X}, r2[9] = {0, 0, 0, u, v, 1, -u * Y, -v * Y, Y};
        memcpy(M[2 * i], r1, sizeof r1); memcpy(M[2 * i + 1], r2, sizeof r2);
    }
    if (solve8(M, x)) return -1;
    for (int i = 0; i < 8; i++) H->a[i] = x[i];
    H->a[8] = 1;
    return 0;
}
static pt homog_map(const homog *H, double u, double v) {
    double z = H->a[6] * u + H->a[7] * v + H->a[8];
    pt p = {(H->a[0] * u + H->a[1] * v + H->a[2]) / z, (H->a[3] * u + H->a[4] * v + H->a[5]) / z};
    return p;
}
static int sample(const uint8_t *bin, int w, int h, pt p) {
    int x = (int)floor(p.x), y = (int)floor(p.y);
    if (x < 0 || y < 0 || x >= w || y >= h) return 0;
    return B(x, y);
}
/* padrão de alinhamento perto de 'guess' (5x5 módulos: escuro, claro, escuro no centro) */
static int find_align(const uint8_t *bin, int w, int h, pt guess, double m, pt *out) {
    int r = (int)(m * 4), best = -1;
    pt bp = guess;
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++) {
            pt c = {guess.x + dx, guess.y + dy};
            int s = 0;
            for (int j = -2; j <= 2; j++)
                for (int i = -2; i <= 2; i++) {
                    int ring = abs(i) > abs(j) ? abs(i) : abs(j);
                    pt q = {c.x + i * m, c.y + j * m};
                    s += sample(bin, w, h, q) == (ring != 1);
                }
            if (s > best || (s == best && hypot(dx, dy) < hypot(bp.x - guess.x, bp.y - guess.y))) { best = s; bp = c; }
        }
    if (best < 22) return 0;
    *out = bp;
    return 1;
}

/* ---------------------------------------------------------------- GF(256) e Reed-Solomon */
static uint8_t EXP[512], LOG[256];
static void gf_init(void) {
    if (EXP[0]) return;
    int x = 1;
    for (int i = 0; i < 255; i++) { EXP[i] = (uint8_t)x; LOG[x] = (uint8_t)i; x <<= 1; if (x & 0x100) x ^= 0x11D; }
    for (int i = 255; i < 512; i++) EXP[i] = EXP[i - 255];
}
static uint8_t gm(uint8_t a, uint8_t b) { return a && b ? EXP[LOG[a] + LOG[b]] : 0; }
static uint8_t gdiv(uint8_t a, uint8_t b) { return a ? EXP[(LOG[a] + 255 - LOG[b]) % 255] : 0; }
/* corrige c[0..n) (c[0] = coeficiente de maior grau), ns símbolos de paridade com raízes α^0..α^(ns-1); -1 se não dá */
static int rs_fix(uint8_t *c, int n, int ns) {
    uint8_t S[64], L[64] = {1}, Bp[64] = {1}, T[64], Om[64];
    int any = 0;
    for (int i = 0; i < ns; i++) {
        uint8_t s = 0;
        for (int j = 0; j < n; j++) s = gm(s, EXP[i]) ^ c[j];
        S[i] = s; any |= s;
    }
    if (!any) return 0;
    int l = 0, mm = 1;
    uint8_t b = 1;
    memset(L + 1, 0, 63); memset(Bp + 1, 0, 63);
    for (int k = 0; k < ns; k++) {   /* Berlekamp-Massey */
        uint8_t d = S[k];
        for (int i = 1; i <= l; i++) d ^= gm(L[i], S[k - i]);
        if (!d) { mm++; continue; }
        uint8_t coef = gdiv(d, b);
        memcpy(T, L, sizeof T);
        for (int i = 0; i + mm < 64; i++) L[i + mm] ^= gm(coef, Bp[i]);
        if (2 * l <= k) { l = k + 1 - l; memcpy(Bp, T, sizeof T); b = d; mm = 1; }
        else mm++;
    }
    if (l * 2 > ns) return -1;
    /* Ω = S·Λ mod x^ns */
    for (int i = 0; i < ns; i++) { uint8_t v = 0; for (int j = 0; j <= i && j <= l; j++) v ^= gm(L[j], S[i - j]); Om[i] = v; }
    int found = 0;
    for (int pos = 0; pos < n; pos++) {   /* Chien: posição pos tem potência p = n-1-pos, X = α^p */
        int p = n - 1 - pos;
        uint8_t xi = EXP[(255 - p % 255) % 255];   /* X^-1 */
        uint8_t v = 0, xp = 1;
        for (int i = 0; i <= l; i++) { v ^= gm(L[i], xp); xp = gm(xp, xi); }
        if (v) continue;
        /* Forney (primeira raiz α^0): e = X · Ω(X^-1) / Λ'(X^-1) */
        uint8_t om = 0, dl = 0;
        xp = 1;
        for (int i = 0; i < ns; i++) { om ^= gm(Om[i], xp); xp = gm(xp, xi); }
        xp = 1;
        for (int i = 1; i <= l; i += 2) { dl ^= gm(L[i], xp); xp = gm(xp, gm(xi, xi)); }
        if (!dl) return -1;
        c[pos] ^= gm(EXP[p % 255], gdiv(om, dl));
        found++;
    }
    if (found != l) return -1;
    for (int i = 0; i < ns; i++) {
        uint8_t s = 0;
        for (int j = 0; j < n; j++) s = gm(s, EXP[i]) ^ c[j];
        if (s) return -1;
    }
    return found;
}

/* ---------------------------------------------------------------- formato e versão */
static int popc(unsigned v) { int n = 0; while (v) { n += v & 1; v >>= 1; } return n; }
static int format_code(int lvl, int mask) {
    static const int lb[4] = {1, 0, 3, 2};
    int data = lb[lvl] << 3 | mask, rem = data;
    for (int i = 0; i < 10; i++) rem = (rem << 1) ^ ((rem >> 9) * 0x537);
    return (data << 10 | rem) ^ 0x5412;
}
static int version_code(int v) { int rem = v; for (int i = 0; i < 12; i++) rem = (rem << 1) ^ ((rem >> 11) * 0x1F25); return v << 12 | rem; }
#define M(x, y) mod[(y) * n + (x)]
static int read_format(const uint8_t *mod, int n, int *lvl, int *mask) {
    int a = 0, b = 0;
    for (int i = 0; i <= 5; i++) a |= M(8, i) << i;
    a |= M(8, 7) << 6 | M(8, 8) << 7 | M(7, 8) << 8;
    for (int i = 9; i < 15; i++) a |= M(14 - i, 8) << i;
    for (int i = 0; i < 8; i++) b |= M(n - 1 - i, 8) << i;
    for (int i = 8; i < 15; i++) b |= M(8, n - 15 + i) << i;
    int best = 99;
    for (int l = 0; l < 4; l++) for (int m = 0; m < 8; m++) {
        int code = format_code(l, m), d = popc((unsigned)(code ^ a)), d2 = popc((unsigned)(code ^ b));
        if (d2 < d) d = d2;
        if (d < best) { best = d; *lvl = l; *mask = m; }
    }
    return best <= 3 ? 0 : -1;
}
static int read_version(const uint8_t *mod, int n) {
    long a = 0, b = 0;
    for (int i = 0; i < 18; i++) { int x = n - 11 + i % 3, y = i / 3; a |= (long)M(x, y) << i; b |= (long)M(y, x) << i; }
    int best = 99, bv = -1;
    for (int v = 7; v <= 40; v++) {
        long code = version_code(v);
        int d = popc((unsigned)(code ^ a)), d2 = popc((unsigned)(code ^ b));
        if (d2 < d) d = d2;
        if (d < best) { best = d; bv = v; }
    }
    return best <= 3 ? bv : -1;
}
static int masked(int mask, int x, int y) {
    switch (mask) {
    case 0: return (x + y) % 2 == 0;
    case 1: return y % 2 == 0;
    case 2: return x % 3 == 0;
    case 3: return (x + y) % 3 == 0;
    case 4: return (x / 3 + y / 2) % 2 == 0;
    case 5: return x * y % 2 + x * y % 3 == 0;
    case 6: return (x * y % 2 + x * y % 3) % 2 == 0;
    default: return ((x + y) % 2 + x * y % 3) % 2 == 0;
    }
}

/* ---------------------------------------------------------------- dados */
typedef struct { const uint8_t *d; int n, bit; } bits;
static int getb(bits *b, int k) {
    int v = 0;
    for (int i = 0; i < k; i++) {
        if (b->bit >= b->n * 8) return -1;
        v = v << 1 | (b->d[b->bit >> 3] >> (7 - (b->bit & 7)) & 1);
        b->bit++;
    }
    return v;
}
static int put(mos_qr_result *r, int c) { if (r->len >= (int)sizeof r->text) return -1; r->text[r->len++] = (uint8_t)c; return 0; }
int mos_qr_parse_data(const uint8_t *d, int n, int v, mos_qr_result *r) {
    static const char AN[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:";
    bits b = {d, n, 0};
    int g = v <= 9 ? 0 : v <= 26 ? 1 : 2;
    r->len = 0; r->kanji = 0; r->eci = -1;
    for (;;) {
        int mode = b.bit + 4 <= n * 8 ? getb(&b, 4) : 0;
        if (mode <= 0) return 0;
        if (mode == 7) {   /* ECI */
            int e = getb(&b, 8);
            if (e < 0) return -1;
            if (e & 0x80) e = (e & 0x40) ? ((e & 0x1F) << 16 | getb(&b, 16)) : ((e & 0x3F) << 8 | getb(&b, 8));
            r->eci = e;
            continue;
        }
        static const int cnt[4][3] = {{10, 12, 14}, {9, 11, 13}, {8, 16, 16}, {8, 10, 12}};
        int mi = mode == 1 ? 0 : mode == 2 ? 1 : mode == 4 ? 2 : mode == 8 ? 3 : -1;
        if (mi < 0) return mode == 3 || mode == 5 || mode == 9 ? 0 : -1;   /* append estruturado/FNC1: o resto não é texto */
        int count = getb(&b, cnt[mi][g]);
        if (count < 0) return -1;
        if (mode == 1) {
            while (count >= 3) { int t = getb(&b, 10); if (t < 0 || t > 999) return -1; put(r, '0' + t / 100); put(r, '0' + t / 10 % 10); put(r, '0' + t % 10); count -= 3; }
            if (count == 2) { int t = getb(&b, 7); if (t < 0 || t > 99) return -1; put(r, '0' + t / 10); put(r, '0' + t % 10); }
            else if (count == 1) { int t = getb(&b, 4); if (t < 0 || t > 9) return -1; put(r, '0' + t); }
        } else if (mode == 2) {
            while (count >= 2) { int t = getb(&b, 11); if (t < 0 || t >= 45 * 45) return -1; put(r, AN[t / 45]); put(r, AN[t % 45]); count -= 2; }
            if (count) { int t = getb(&b, 6); if (t < 0 || t >= 45) return -1; put(r, AN[t]); }
        } else if (mode == 4) {
            while (count--) { int t = getb(&b, 8); if (t < 0) return -1; put(r, t); }
        } else {
            r->kanji = 1;
            while (count--) {
                int t = getb(&b, 13);
                if (t < 0) return -1;
                int c = (t / 0xC0) << 8 | (t % 0xC0);
                c += c < 0x1F00 ? 0x8140 : 0xC140;
                put(r, c >> 8); put(r, c & 0xFF);
            }
        }
    }
}

/* lê a grade de módulos de uma versão; 0 ok */
static int decode_grid(const uint8_t *mod, int n, int v, mos_qr_result *r) {
    int lvl, mask;
    if (read_format(mod, n, &lvl, &mask)) return -1;
    int nb, ecl, raw;
    uint8_t *fn = malloc((size_t)n * n), *all = calloc(3706 + 8, 1);
    if (!fn || !all) { free(fn); free(all); return -1; }
    if (mos_qr_layout(v, lvl, fn, &nb, &ecl, &raw) != n) { free(fn); free(all); return -1; }
    int i = 0;
    for (int right = n - 1; right >= 1; right -= 2) {
        if (right == 6) right = 5;
        for (int vert = 0; vert < n; vert++)
            for (int j = 0; j < 2; j++) {
                int x = right - j, up = ((right + 1) & 2) == 0, y = up ? n - 1 - vert : vert;
                if (fn[y * n + x] || i >= raw * 8) continue;
                int bit = M(x, y) ^ masked(mask, x, y);
                if (bit) all[i >> 3] |= (uint8_t)(0x80 >> (i & 7));
                i++;
            }
    }
    free(fn);
    /* desfaz a intercalação (blocos curtos têm um vão na posição do dado extra dos longos) */
    int nshort = nb - raw % nb, slen = raw / nb, k = 0, nd = 0, status = 0;
    uint8_t *blocks = calloc((size_t)nb * (slen + 1), 1);
    if (!blocks) { free(all); return -1; }
    for (int p = 0; p <= slen; p++)
        for (int q = 0; q < nb; q++)
            if (p != slen - ecl || q >= nshort) blocks[(size_t)q * (slen + 1) + p] = all[k++];
    r->corrected = 0;
    for (int q = 0; q < nb && !status; q++) {
        uint8_t *bl = blocks + (size_t)q * (slen + 1), cw[256];
        int dl = slen - ecl + (q < nshort ? 0 : 1);
        memcpy(cw, bl, (size_t)dl);
        memcpy(cw + dl, bl + (q < nshort ? dl + 1 : dl), (size_t)ecl);
        int fx = rs_fix(cw, dl + ecl, ecl);
        if (fx < 0) { status = -1; break; }
        r->corrected += fx;
        memcpy(r->data + nd, cw, (size_t)dl);
        nd += dl;
    }
    free(blocks); free(all);
    if (status) return -1;
    r->ndata = nd; r->version = v; r->level = lvl; r->mask = mask;
    return mos_qr_parse_data(r->data, nd, v, r);
}

int mos_qr_read(const uint8_t *gray, int w, int h, mos_qr_result *out) {
    if (!gray || w < 21 || h < 21 || !out) return 0;
    gf_init();
    uint8_t *bin = binarize(gray, w, h);
    if (!bin) return 0;
    cand cs[64];
    int nc = find_candidates(bin, w, h, cs, 64), ok = 0;
    pt tl, tr, bl;
    double m;
    if (nc >= 3 && pick_three(cs, nc, &tl, &tr, &bl, &m)) {
        double d = (dist(tl, tr) + dist(tl, bl)) / (2 * m) + 7;
        int v0 = (int)floor((d - 17) / 4 + 0.5);
        for (int attempt = 0; attempt < 5 && !ok; attempt++) {
            int v = v0 + (attempt == 1 ? 1 : attempt == 2 ? -1 : attempt == 3 ? 2 : attempt == 4 ? -2 : 0);
            if (v < 1 || v > 40) continue;
            int n = 4 * v + 17;
            pt br = {tr.x + bl.x - tl.x, tr.y + bl.y - tl.y};
            pt src[4] = {{3.5, 3.5}, {n - 3.5, 3.5}, {3.5, n - 3.5}, {n - 3.5, n - 3.5}}, dst[4] = {tl, tr, bl, br};
            homog H;
            if (homog_make(src, dst, &H)) continue;
            if (v >= 2) {   /* o alinhamento do canto inferior direito acerta a perspectiva */
                pt g = homog_map(&H, n - 6.5, n - 6.5), a;
                double mod_px = dist(tl, tr) / (n - 7);
                if (find_align(bin, w, h, g, mod_px, &a)) {
                    src[3].x = src[3].y = n - 6.5; dst[3] = a;
                    homog H2;
                    if (!homog_make(src, dst, &H2)) H = H2;
                }
            }
            uint8_t *mod = malloc((size_t)n * n);
            if (!mod) break;
            for (int y = 0; y < n; y++) for (int x = 0; x < n; x++) mod[y * n + x] = (uint8_t)sample(bin, w, h, homog_map(&H, x + 0.5, y + 0.5));
            int vv = v >= 7 ? read_version(mod, n) : v;
            if (vv == v && !decode_grid(mod, n, v, out)) {
                ok = 1;
                pt c[4] = {homog_map(&H, 0, 0), homog_map(&H, n, 0), homog_map(&H, 0, n), homog_map(&H, n, n)};
                for (int i = 0; i < 4; i++) { out->corners[i][0] = c[i].x; out->corners[i][1] = c[i].y; }
            }
            free(mod);
        }
    }
    free(bin);
    return ok;
}
