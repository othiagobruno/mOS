/*
 * mOS — gerador de QR Code (ISO/IEC 18004) para o CIQRCodeGenerator: modo byte, versões 1 a 40, correção L/M/Q/H,
 * Reed-Solomon em GF(256) (0x11D), blocos intercalados, padrões de função, informação de formato e de versão (BCH) e a
 * máscara de menor penalidade (regras N1 a N4). Teste: tools/test-qrgen.sh (matriz igual à da biblioteca qrcode).
 * Código original do mOS (MIT).
 */
#include "qrgen.h"
#include <stdlib.h>
#include <string.h>

static const signed char ECC_PER_BLOCK[4][41] = {
    {-1, 7, 10, 15, 20, 26, 18, 20, 24, 30, 18, 20, 24, 26, 30, 22, 24, 28, 30, 28, 28, 28, 28, 30, 30, 26, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},
    {-1, 10, 16, 26, 18, 24, 16, 18, 22, 22, 26, 30, 22, 22, 24, 24, 28, 28, 26, 26, 26, 26, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28},
    {-1, 13, 22, 18, 26, 18, 24, 18, 22, 20, 24, 28, 26, 24, 20, 30, 24, 28, 28, 26, 30, 28, 30, 30, 30, 30, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},
    {-1, 17, 28, 22, 16, 22, 28, 26, 26, 24, 28, 24, 28, 22, 24, 24, 30, 28, 28, 26, 28, 30, 24, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},
};
static const signed char NUM_BLOCKS[4][41] = {
    {-1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 4, 4, 4, 4, 4, 6, 6, 6, 6, 7, 8, 8, 9, 9, 10, 12, 12, 12, 13, 14, 15, 16, 17, 18, 19, 19, 20, 21, 22, 24, 25},
    {-1, 1, 1, 1, 2, 2, 4, 4, 4, 5, 5, 5, 8, 9, 9, 10, 10, 11, 13, 14, 16, 17, 17, 18, 20, 21, 23, 25, 26, 28, 29, 31, 33, 35, 37, 38, 40, 43, 45, 47, 49},
    {-1, 1, 1, 2, 2, 4, 4, 6, 6, 8, 8, 8, 10, 12, 16, 12, 17, 16, 18, 21, 20, 23, 23, 25, 27, 29, 34, 34, 35, 38, 40, 43, 45, 48, 51, 53, 56, 59, 62, 65, 68},
    {-1, 1, 1, 2, 4, 4, 4, 5, 6, 8, 8, 11, 11, 16, 16, 18, 16, 19, 21, 25, 25, 25, 34, 30, 32, 35, 37, 40, 42, 45, 48, 51, 54, 57, 60, 63, 66, 70, 74, 77, 81},
};

typedef struct { int n; uint8_t m[177 * 177], fn[177 * 177]; } grid;
#define AT(g, x, y) ((g)->m[(y) * (g)->n + (x)])
#define FN(g, x, y) ((g)->fn[(y) * (g)->n + (x)])
static void setf(grid *g, int x, int y, int v) { AT(g, x, y) = (uint8_t)v; FN(g, x, y) = 1; }

static int raw_modules(int v) {
    int r = (16 * v + 128) * v + 64;
    if (v >= 2) { int na = v / 7 + 2; r -= (25 * na - 10) * na - 55; if (v >= 7) r -= 36; }
    return r;
}
static int data_codewords(int v, int lvl) { return raw_modules(v) / 8 - ECC_PER_BLOCK[lvl][v] * NUM_BLOCKS[lvl][v]; }
static int align_pos(int v, int *out) {
    if (v == 1) return 0;
    int na = v / 7 + 2, size = v * 4 + 17;
    int step = v == 32 ? 26 : (v * 4 + na * 2 + 1) / (na * 2 - 2) * 2;
    out[0] = 6;
    for (int i = na - 1, pos = size - 7; i >= 1; i--, pos -= step) out[i] = pos;
    return na;
}
static uint8_t gmul(uint8_t x, uint8_t y) {
    int z = 0;
    for (int i = 7; i >= 0; i--) { z = (z << 1) ^ ((z >> 7) * 0x11D); z ^= ((y >> i) & 1) * x; }
    return (uint8_t)z;
}
static void rs_divisor(int deg, uint8_t *res) {
    memset(res, 0, (size_t)deg);
    res[deg - 1] = 1;
    uint8_t root = 1;
    for (int i = 0; i < deg; i++) {
        for (int j = 0; j < deg; j++) { res[j] = gmul(res[j], root); if (j + 1 < deg) res[j] ^= res[j + 1]; }
        root = gmul(root, 0x02);
    }
}
static void rs_remainder(const uint8_t *data, int len, const uint8_t *div, int deg, uint8_t *res) {
    memset(res, 0, (size_t)deg);
    for (int i = 0; i < len; i++) {
        uint8_t f = data[i] ^ res[0];
        memmove(res, res + 1, (size_t)deg - 1);
        res[deg - 1] = 0;
        for (int j = 0; j < deg; j++) res[j] ^= gmul(div[j], f);
    }
}
static void draw_finder(grid *g, int x, int y) {
    for (int dy = -4; dy <= 4; dy++)
        for (int dx = -4; dx <= 4; dx++) {
            int d = abs(dx) > abs(dy) ? abs(dx) : abs(dy), xx = x + dx, yy = y + dy;
            if (xx >= 0 && xx < g->n && yy >= 0 && yy < g->n) setf(g, xx, yy, d != 2 && d != 4);
        }
}
static void draw_format(grid *g, int lvl, int mask) {
    static const int lb[4] = {1, 0, 3, 2};
    int data = lb[lvl] << 3 | mask, rem = data;
    for (int i = 0; i < 10; i++) rem = (rem << 1) ^ ((rem >> 9) * 0x537);
    int bits = (data << 10 | rem) ^ 0x5412, n = g->n;
    for (int i = 0; i <= 5; i++) setf(g, 8, i, bits >> i & 1);
    setf(g, 8, 7, bits >> 6 & 1); setf(g, 8, 8, bits >> 7 & 1); setf(g, 7, 8, bits >> 8 & 1);
    for (int i = 9; i < 15; i++) setf(g, 14 - i, 8, bits >> i & 1);
    for (int i = 0; i < 8; i++) setf(g, n - 1 - i, 8, bits >> i & 1);
    for (int i = 8; i < 15; i++) setf(g, 8, n - 15 + i, bits >> i & 1);
    setf(g, 8, n - 8, 1);   /* módulo escuro fixo */
}
static void draw_functions(grid *g, int v, int lvl) {
    int n = g->n, pos[7];
    for (int i = 0; i < n; i++) { setf(g, 6, i, i % 2 == 0); setf(g, i, 6, i % 2 == 0); }
    draw_finder(g, 3, 3); draw_finder(g, n - 4, 3); draw_finder(g, 3, n - 4);
    int na = align_pos(v, pos);
    for (int i = 0; i < na; i++)
        for (int j = 0; j < na; j++) {
            if ((i == 0 && j == 0) || (i == 0 && j == na - 1) || (i == na - 1 && j == 0)) continue;
            for (int dy = -2; dy <= 2; dy++)
                for (int dx = -2; dx <= 2; dx++) setf(g, pos[i] + dx, pos[j] + dy, (abs(dx) > abs(dy) ? abs(dx) : abs(dy)) != 1);
        }
    draw_format(g, lvl, 0);
    if (v >= 7) {
        int rem = v;
        for (int i = 0; i < 12; i++) rem = (rem << 1) ^ ((rem >> 11) * 0x1F25);
        long bits = (long)v << 12 | rem;
        for (int i = 0; i < 18; i++) {
            int bit = bits >> i & 1, a = n - 11 + i % 3, b = i / 3;
            setf(g, a, b, bit); setf(g, b, a, bit);
        }
    }
}
static void draw_codewords(grid *g, const uint8_t *data, int len) {
    int i = 0, n = g->n;
    for (int right = n - 1; right >= 1; right -= 2) {
        if (right == 6) right = 5;
        for (int vert = 0; vert < n; vert++)
            for (int j = 0; j < 2; j++) {
                int x = right - j, up = ((right + 1) & 2) == 0, y = up ? n - 1 - vert : vert;
                if (!FN(g, x, y) && i < len * 8) { AT(g, x, y) = data[i >> 3] >> (7 - (i & 7)) & 1; i++; }
            }
    }
}
static void apply_mask(grid *g, int mask) {
    for (int y = 0; y < g->n; y++)
        for (int x = 0; x < g->n; x++) {
            if (FN(g, x, y)) continue;
            int inv;
            switch (mask) {
            case 0: inv = (x + y) % 2 == 0; break;
            case 1: inv = y % 2 == 0; break;
            case 2: inv = x % 3 == 0; break;
            case 3: inv = (x + y) % 3 == 0; break;
            case 4: inv = (x / 3 + y / 2) % 2 == 0; break;
            case 5: inv = x * y % 2 + x * y % 3 == 0; break;
            case 6: inv = (x * y % 2 + x * y % 3) % 2 == 0; break;
            default: inv = ((x + y) % 2 + x * y % 3) % 2 == 0; break;
            }
            AT(g, x, y) ^= (uint8_t)inv;
        }
}
/* penalidade (ISO 18004 7.8.3): N1 corridas de 5+, N2 blocos 2x2, N3 padrão 1:1:3:1:1 com 4 claros, N4 proporção */
static long penalty(const grid *g) {
    long p = 0;
    int n = g->n, dark = 0;
    for (int pass = 0; pass < 2; pass++)
        for (int a = 0; a < n; a++) {
            int run = 0, prev = -1;
            for (int b = 0; b < n; b++) {
                int c = pass ? g->m[b * n + a] : g->m[a * n + b];
                if (c == prev) { run++; if (run == 5) p += 3; else if (run > 5) p++; }
                else { run = 1; prev = c; }
            }
            for (int b = 0; b + 6 < n; b++) {   /* 1011101 com 4 claros de um lado */
                static const int pat[7] = {1, 0, 1, 1, 1, 0, 1};
                int ok = 1;
                for (int k = 0; k < 7 && ok; k++) ok = (pass ? g->m[(b + k) * n + a] : g->m[a * n + b + k]) == pat[k];
                if (!ok) continue;
                int before = 1, after = 1;
                for (int k = 1; k <= 4; k++) {
                    int bb = b - k, aa = b + 6 + k;
                    if (bb >= 0 && (pass ? g->m[bb * n + a] : g->m[a * n + bb])) before = 0;
                    if (aa < n && (pass ? g->m[aa * n + a] : g->m[a * n + aa])) after = 0;
                }
                if (before || after) p += 40;
            }
        }
    for (int y = 0; y + 1 < n; y++)
        for (int x = 0; x + 1 < n; x++) {
            int c = g->m[y * n + x];
            if (c == g->m[y * n + x + 1] && c == g->m[(y + 1) * n + x] && c == g->m[(y + 1) * n + x + 1]) p += 3;
        }
    for (int i = 0; i < n * n; i++) dark += g->m[i];
    int total = n * n, k = (abs(dark * 20 - total * 10) + total - 1) / total - 1;
    p += (k > 0 ? k : 0) * 10;
    return p;
}

int mos_qr_encode(const uint8_t *msg, int len, int level, int version, int mask, uint8_t *out, int *size) {
    if (level < 0 || level > 3 || len < 0 || !out || !size) return -1;
    int v = version > 0 ? version : 1;
    for (;; v++) {
        if (v > 40) return -1;
        int bits = 4 + (v <= 9 ? 8 : 16) + 8 * len;
        if ((bits + 7) / 8 <= data_codewords(v, level)) break;
        if (version > 0) return -1;
    }
    int cap = data_codewords(v, level);
    uint8_t *dat = calloc((size_t)cap, 1), *all = calloc((size_t)raw_modules(v) / 8 + 1, 1);
    grid *g = calloc(1, sizeof *g);
    if (!dat || !all || !g) { free(dat); free(all); free(g); return -1; }
    long bit = 0;
#define PUT(val, nb) for (int _i = (nb) - 1; _i >= 0; _i--, bit++) if (((val) >> _i) & 1) dat[bit >> 3] |= (uint8_t)(0x80 >> (bit & 7))
    PUT(4, 4);
    PUT(len, v <= 9 ? 8 : 16);
    for (int i = 0; i < len; i++) PUT(msg[i], 8);
    long capbits = (long)cap * 8;
    for (int t = 0; t < 4 && bit < capbits; t++) bit++;   /* terminador */
    bit = (bit + 7) / 8 * 8;
    for (int pad = 0xEC; bit < capbits; pad ^= 0xEC ^ 0x11) PUT(pad, 8);
#undef PUT
    /* blocos, ECC e intercalação */
    int nb = NUM_BLOCKS[level][v], ecl = ECC_PER_BLOCK[level][v], raw = raw_modules(v) / 8;
    int nshort = nb - raw % nb, slen = raw / nb;
    uint8_t div[30], ecc[30];
    rs_divisor(ecl, div);
    int k = 0, outn = 0;
    uint8_t *blocks = calloc((size_t)nb * (size_t)(slen + 1), 1);
    if (!blocks) { free(dat); free(all); free(g); return -1; }
    for (int i = 0; i < nb; i++) {
        int dl = slen - ecl + (i < nshort ? 0 : 1);
        uint8_t *b = blocks + (size_t)i * (slen + 1);
        memcpy(b, dat + k, (size_t)dl);
        k += dl;
        rs_remainder(b, dl, div, ecl, ecc);
        if (i < nshort) { memcpy(b + dl + 1, ecc, (size_t)ecl); }   /* o bloco curto deixa um vão onde o longo tem dado */
        else memcpy(b + dl, ecc, (size_t)ecl);
    }
    for (int i = 0; i <= slen; i++)
        for (int j = 0; j < nb; j++)
            if (i != slen - ecl || j >= nshort) all[outn++] = blocks[(size_t)j * (slen + 1) + i];
    free(blocks);
    g->n = v * 4 + 17;
    draw_functions(g, v, level);
    draw_codewords(g, all, outn);
    int best = mask;
    if (mask < 0 || mask > 7) {
        long bp = -1;
        for (int m = 0; m < 8; m++) {
            apply_mask(g, m); draw_format(g, level, m);
            long pp = penalty(g);
            if (bp < 0 || pp < bp) { bp = pp; best = m; }
            apply_mask(g, m);
        }
    }
    apply_mask(g, best);
    draw_format(g, level, best);
    *size = g->n;
    memcpy(out, g->m, (size_t)g->n * g->n);
    free(dat); free(all); free(g);
    return v;
}

/* para o leitor (qrread.c): módulos de função da versão (padrões, formato, versão) e a divisão em blocos */
int mos_qr_layout(int v, int lvl, uint8_t *fn, int *nblocks, int *ecc_per_block, int *raw_codewords) {
    if (v < 1 || v > 40 || lvl < 0 || lvl > 3) return -1;
    grid *g = calloc(1, sizeof *g);
    if (!g) return -1;
    g->n = v * 4 + 17;
    draw_functions(g, v, lvl);
    if (fn) memcpy(fn, g->fn, (size_t)g->n * g->n);
    if (nblocks) *nblocks = NUM_BLOCKS[lvl][v];
    if (ecc_per_block) *ecc_per_block = ECC_PER_BLOCK[lvl][v];
    if (raw_codewords) *raw_codewords = raw_modules(v) / 8;
    int n = g->n;
    free(g);
    return n;
}
