/*
 * Codificador H.264 intra I_PCM, 4:2:0, 8 bits. O decodificador Baseline
 * (CAVLC, intra 4x4/16x16, P-slice, deblocking) está em h264bsd_glue.c.
 * Entrada e saída em I420. O access unit sai em AVCC (sample) e Annex B.
 *
 * As tabelas g_kuiVlc* vêm do OpenH264 (Cisco), BSD-2-Clause. O código em volta
 * é original do mOS (MIT). Não há x264, x265, libde265 nem FFmpeg no fonte.
 * HEVC não entra: o encoder permissivo (Kvazaar) é C++ e só codifica; o
 * decodificador permissivo pequeno o bastante para o libSystem freestanding não existe.
 */
#include "h264sw.h"
#include <stdlib.h>
#include <string.h>

/* ---- tabelas CAVLC (OpenH264, BSD-2-Clause; copyright Cisco Systems, 2013) ---- */
static const uint8_t vlc_token[5][17][4][2] = {
  { {{1,1},{0,0},{0,0},{0,0}}, {{5,6},{1,2},{0,0},{0,0}}, {{7,8},{4,6},{1,3},{0,0}}, {{7,9},{6,8},{5,7},{3,5}},
    {{7,10},{6,9},{5,8},{3,6}}, {{7,11},{6,10},{5,9},{4,7}}, {{15,13},{6,11},{5,10},{4,8}}, {{11,13},{14,13},{5,11},{4,9}},
    {{8,13},{10,13},{13,13},{4,10}}, {{15,14},{14,14},{9,13},{4,11}}, {{11,14},{10,14},{13,14},{12,13}}, {{15,15},{14,15},{9,14},{12,14}},
    {{11,15},{10,15},{13,15},{8,14}}, {{15,16},{1,15},{9,15},{12,15}}, {{11,16},{14,16},{13,16},{8,15}}, {{7,16},{10,16},{9,16},{12,16}},
    {{4,16},{6,16},{5,16},{8,16}} },
  { {{3,2},{0,0},{0,0},{0,0}}, {{11,6},{2,2},{0,0},{0,0}}, {{7,6},{7,5},{3,3},{0,0}}, {{7,7},{10,6},{9,6},{5,4}},
    {{7,8},{6,6},{5,6},{4,4}}, {{4,8},{6,7},{5,7},{6,5}}, {{7,9},{6,8},{5,8},{8,6}}, {{15,11},{6,9},{5,9},{4,6}},
    {{11,11},{14,11},{13,11},{4,7}}, {{15,12},{10,11},{9,11},{4,9}}, {{11,12},{14,12},{13,12},{12,11}}, {{8,12},{10,12},{9,12},{8,11}},
    {{15,13},{14,13},{13,13},{12,12}}, {{11,13},{10,13},{9,13},{12,13}}, {{7,13},{11,14},{6,13},{8,13}}, {{9,14},{8,14},{10,14},{1,13}},
    {{7,14},{6,14},{5,14},{4,14}} },
  { {{15,4},{0,0},{0,0},{0,0}}, {{15,6},{14,4},{0,0},{0,0}}, {{11,6},{15,5},{13,4},{0,0}}, {{8,6},{12,5},{14,5},{12,4}},
    {{15,7},{10,5},{11,5},{11,4}}, {{11,7},{8,5},{9,5},{10,4}}, {{9,7},{14,6},{13,6},{9,4}}, {{8,7},{10,6},{9,6},{8,4}},
    {{15,8},{14,7},{13,7},{13,5}}, {{11,8},{14,8},{10,7},{12,6}}, {{15,9},{10,8},{13,8},{12,7}}, {{11,9},{14,9},{9,8},{12,8}},
    {{8,9},{10,9},{13,9},{8,8}}, {{13,10},{7,9},{9,9},{12,9}}, {{9,10},{12,10},{11,10},{10,10}}, {{5,10},{8,10},{7,10},{6,10}},
    {{1,10},{4,10},{3,10},{2,10}} },
  { {{3,6},{0,0},{0,0},{0,0}}, {{0,6},{1,6},{0,0},{0,0}}, {{4,6},{5,6},{6,6},{0,0}}, {{8,6},{9,6},{10,6},{11,6}},
    {{12,6},{13,6},{14,6},{15,6}}, {{16,6},{17,6},{18,6},{19,6}}, {{20,6},{21,6},{22,6},{23,6}}, {{24,6},{25,6},{26,6},{27,6}},
    {{28,6},{29,6},{30,6},{31,6}}, {{32,6},{33,6},{34,6},{35,6}}, {{36,6},{37,6},{38,6},{39,6}}, {{40,6},{41,6},{42,6},{43,6}},
    {{44,6},{45,6},{46,6},{47,6}}, {{48,6},{49,6},{50,6},{51,6}}, {{52,6},{53,6},{54,6},{55,6}}, {{56,6},{57,6},{58,6},{59,6}},
    {{60,6},{61,6},{62,6},{63,6}} },
  { {{1,2},{0,0},{0,0},{0,0}}, {{7,6},{1,1},{0,0},{0,0}}, {{4,6},{6,6},{1,3},{0,0}}, {{3,6},{3,7},{2,7},{5,6}},
    {{2,6},{3,8},{2,8},{0,7}}, {{0,0},{0,0},{0,0},{0,0}}, {{0,0},{0,0},{0,0},{0,0}}, {{0,0},{0,0},{0,0},{0,0}},
    {{0,0},{0,0},{0,0},{0,0}}, {{0,0},{0,0},{0,0},{0,0}}, {{0,0},{0,0},{0,0},{0,0}}, {{0,0},{0,0},{0,0},{0,0}},
    {{0,0},{0,0},{0,0},{0,0}}, {{0,0},{0,0},{0,0},{0,0}}, {{0,0},{0,0},{0,0},{0,0}}, {{0,0},{0,0},{0,0},{0,0}},
    {{0,0},{0,0},{0,0},{0,0}} }
};
static const uint8_t vlc_tz[16][16][2] = {
  {{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}},
  {{1,1},{3,3},{2,3},{3,4},{2,4},{3,5},{2,5},{3,6},{2,6},{3,7},{2,7},{3,8},{2,8},{3,9},{2,9},{1,9}},
  {{7,3},{6,3},{5,3},{4,3},{3,3},{5,4},{4,4},{3,4},{2,4},{3,5},{2,5},{3,6},{2,6},{1,6},{0,6},{0,0}},
  {{5,4},{7,3},{6,3},{5,3},{4,4},{3,4},{4,3},{3,3},{2,4},{3,5},{2,5},{1,6},{1,5},{0,6},{0,0},{0,0}},
  {{3,5},{7,3},{5,4},{4,4},{6,3},{5,3},{4,3},{3,4},{3,3},{2,4},{2,5},{1,5},{0,5},{0,0},{0,0},{0,0}},
  {{5,4},{4,4},{3,4},{7,3},{6,3},{5,3},{4,3},{3,3},{2,4},{1,5},{1,4},{0,5},{0,0},{0,0},{0,0},{0,0}},
  {{1,6},{1,5},{7,3},{6,3},{5,3},{4,3},{3,3},{2,3},{1,4},{1,3},{0,6},{0,0},{0,0},{0,0},{0,0},{0,0}},
  {{1,6},{1,5},{5,3},{4,3},{3,3},{3,2},{2,3},{1,4},{1,3},{0,6},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}},
  {{1,6},{1,4},{1,5},{3,3},{3,2},{2,2},{2,3},{1,3},{0,6},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}},
  {{1,6},{0,6},{1,4},{3,2},{2,2},{1,3},{1,2},{1,5},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}},
  {{1,5},{0,5},{1,3},{3,2},{2,2},{1,2},{1,4},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}},
  {{0,4},{1,4},{1,3},{2,3},{1,1},{3,3},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}},
  {{0,4},{1,4},{1,2},{1,1},{1,3},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}},
  {{0,3},{1,3},{1,1},{1,2},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}},
  {{0,2},{1,2},{1,1},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}},
  {{0,1},{1,1},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}}
};
static const uint8_t vlc_tzc[4][4][2] = {
  {{0,0},{0,0},{0,0},{0,0}}, {{1,1},{1,2},{1,3},{0,3}}, {{1,1},{1,2},{0,2},{0,0}}, {{1,1},{0,1},{0,0},{0,0}}
};
static const uint8_t vlc_run[8][15][2] = {
  {{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}},
  {{1,1},{0,1},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}},
  {{1,1},{1,2},{0,2},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}},
  {{3,2},{2,2},{1,2},{0,2},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}},
  {{3,2},{2,2},{1,2},{1,3},{0,3},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}},
  {{3,2},{2,2},{3,3},{2,3},{1,3},{0,3},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}},
  {{3,2},{0,3},{1,3},{3,3},{2,3},{5,3},{4,3},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}},
  {{7,3},{6,3},{5,3},{4,3},{3,3},{2,3},{1,3},{1,4},{1,5},{1,6},{1,7},{1,8},{1,9},{1,10},{1,11}}
};
static const int zz[16] = {0,1,4,8,5,2,3,6,9,12,13,10,7,11,14,15};
static const int MF[6][3] = {
    {13107,8066,5243},{11916,7490,4660},{10082,6554,4194},
    {9362,5825,3647},{8192,5243,3355},{7282,4559,2893}};
static const int VS[6][3] = {
    {10,16,13},{11,18,14},{13,20,16},{14,23,18},{16,25,20},{18,29,23}};

static int clip8(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }
static int icat(int k) { int r = k >> 2, c = k & 3; if (((r | c) & 1) == 0) return 0; if ((r & 1) && (c & 1)) return 2; return 1; }
static int nc_tab(int nc) {
    static const uint8_t m[18] = {0,0,1,1,2,2,2,2,3,3,3,3,3,3,3,3,3,4};
    if (nc < 0) return 4;
    if (nc > 17) nc = 17;
    return m[nc];
}

typedef struct { uint8_t *b; int cap, bits; } bw;
static int bw_need(bw *w, int add) {
    int nbytes = (w->bits + add + 7) >> 3;
    if (nbytes <= w->cap) return 0;
    int ncap = w->cap ? w->cap * 2 : 256;
    while (ncap < nbytes) ncap *= 2;
    uint8_t *p = realloc(w->b, (size_t)ncap);
    if (!p) return -1;
    if (ncap > w->cap) memset(p + w->cap, 0, (size_t)(ncap - w->cap));
    w->b = p; w->cap = ncap;
    return 0;
}
static int bw_put(bw *w, unsigned v, int n) {
    if (n <= 0) return 0;
    if (bw_need(w, n)) return -1;
    for (int i = n - 1; i >= 0; i--) {
        int bi = w->bits++;
        if ((v >> i) & 1) w->b[bi >> 3] |= (uint8_t)(1u << (7 - (bi & 7)));
    }
    return 0;
}
static int bw_ue(bw *w, unsigned v) {
    unsigned x = v + 1; int z = 0;
    while ((2u << z) <= x && z < 31) z++;
    return bw_put(w, 0, z) || bw_put(w, x, z + 1);
}
static int bw_se(bw *w, int v) { return bw_ue(w, v > 0 ? (unsigned)(v * 2 - 1) : (unsigned)(-v * 2)); }
static int bw_trail(bw *w) {
    if (bw_put(w, 1, 1)) return -1;
    int r = w->bits & 7;
    return r ? bw_put(w, 0, 8 - r) : 0;
}

typedef struct { const uint8_t *b; int nbytes, bit, err; } br;
static int br_get(br *r, int n) {
    int v = 0;
    for (int i = 0; i < n; i++) {
        int bi = r->bit++;
        if ((bi >> 3) >= r->nbytes) { r->err = 1; return 0; }
        v = (v << 1) | ((r->b[bi >> 3] >> (7 - (bi & 7))) & 1);
    }
    return v;
}
static int br_ue(br *r) {
    int z = 0;
    while (z < 31 && !br_get(r, 1)) z++;
    if (r->err) return 0;
    int x = 1;
    for (int i = 0; i < z; i++) x = (x << 1) | br_get(r, 1);
    return x - 1;
}
static int br_se(br *r) { int k = br_ue(r); return (k & 1) ? (k + 1) / 2 : -(k / 2); }

static uint8_t *rbsp_escape(const uint8_t *rbsp, int n, int *out_n) {
    uint8_t *o = malloc((size_t)n * 2 + 8);
    if (!o) return 0;
    int w = 0, z = 0;
    for (int i = 0; i < n; i++) {
        if (z >= 2 && rbsp[i] <= 3) { o[w++] = 3; z = 0; }
        o[w++] = rbsp[i];
        z = rbsp[i] == 0 ? z + 1 : 0;
    }
    *out_n = w;
    return o;
}
static uint8_t *rbsp_unescape(const uint8_t *nal, int n, int *out_n) {
    uint8_t *o = malloc((size_t)n + 1);
    if (!o) return 0;
    int w = 0, z = 0;
    for (int i = 0; i < n; i++) {
        if (z >= 2 && nal[i] == 3) { z = 0; continue; }
        o[w++] = nal[i];
        z = nal[i] == 0 ? z + 1 : 0;
    }
    *out_n = w;
    return o;
}

static void fdct4(const int *x, int *y) {
    int t[16];
    for (int i = 0; i < 4; i++) {
        int s0 = x[i*4+0] + x[i*4+3], s1 = x[i*4+1] + x[i*4+2];
        int s2 = x[i*4+1] - x[i*4+2], s3 = x[i*4+0] - x[i*4+3];
        t[i*4+0] = s0 + s1; t[i*4+1] = (s3 << 1) + s2; t[i*4+2] = s0 - s1; t[i*4+3] = s3 - (s2 << 1);
    }
    for (int i = 0; i < 4; i++) {
        int s0 = t[i] + t[12+i], s1 = t[4+i] + t[8+i], s2 = t[4+i] - t[8+i], s3 = t[i] - t[12+i];
        y[i] = s0 + s1; y[4+i] = (s3 << 1) + s2; y[8+i] = s0 - s1; y[12+i] = s3 - (s2 << 1);
    }
}
static void idct4(int *y) {
    int t[16];
    for (int i = 0; i < 4; i++) {
        int s0 = y[i*4+0] + y[i*4+2], s1 = y[i*4+0] - y[i*4+2];
        int s2 = (y[i*4+1] >> 1) - y[i*4+3], s3 = y[i*4+1] + (y[i*4+3] >> 1);
        t[i*4+0] = s0 + s3; t[i*4+1] = s1 + s2; t[i*4+2] = s1 - s2; t[i*4+3] = s0 - s3;
    }
    for (int i = 0; i < 4; i++) {
        int s0 = t[i] + t[8+i], s1 = t[i] - t[8+i];
        int s2 = (t[4+i] >> 1) - t[12+i], s3 = t[4+i] + (t[12+i] >> 1);
        y[i] = (s0 + s3 + 32) >> 6; y[4+i] = (s1 + s2 + 32) >> 6;
        y[8+i] = (s1 - s2 + 32) >> 6; y[12+i] = (s0 - s3 + 32) >> 6;
    }
}
static void had4(int t[16]) {
    int m[16];
    for (int i = 0; i < 4; i++) {
        int d0 = t[i*4+0] + t[i*4+3], d1 = t[i*4+1] + t[i*4+2];
        int d2 = t[i*4+1] - t[i*4+2], d3 = t[i*4+0] - t[i*4+3];
        m[i*4+0] = d0 + d1; m[i*4+1] = d3 + d2; m[i*4+2] = d0 - d1; m[i*4+3] = d3 - d2;
    }
    for (int i = 0; i < 4; i++) {
        int d0 = m[0*4+i] + m[3*4+i], d1 = m[1*4+i] + m[2*4+i];
        int d2 = m[1*4+i] - m[2*4+i], d3 = m[0*4+i] - m[3*4+i];
        t[0*4+i] = d0 + d1; t[1*4+i] = d3 + d2; t[2*4+i] = d0 - d1; t[3*4+i] = d3 - d2;
    }
}

static int q_ac(int c, int qp, int cat) {
    int qbits = 15 + qp / 6, f = (1 << qbits) / 3;
    int a = c < 0 ? -c : c;
    int q = (int)(((long long)a * MF[qp % 6][cat] + f) >> qbits);
    return c < 0 ? -q : q;
}
static int dq_ac(int lv, int qp, int cat) {
    return (int)((long long)lv * VS[qp % 6][cat] << (qp / 6));
}
static int q_dc(int c, int qp) {
    int qbits = 15 + qp / 6, f = (1 << qbits) / 3;
    int a = c < 0 ? -c : c;
    int q = (int)(((long long)a * MF[qp % 6][0] + 2 * f) >> (qbits + 1));
    return c < 0 ? -q : q;
}
static int dq_dc_luma(int lv, int qp) {
    int sh = qp / 6, v = VS[qp % 6][0];
    if (sh >= 2) return lv * v << (sh - 2);
    return (lv * v + (1 << (1 - sh))) >> (2 - sh);
}
static int dq_dc_chroma(int lv, int qp) {
    int sh = qp / 6, v = VS[qp % 6][0];
    if (sh >= 1) return lv * v << (sh - 1);
    return (lv * v) >> 1;
}

static int cavlc_write(bw *w, const int *c, int ncoeff, int nc, int chroma_dc) {
    int level[16], run[16], total = 0, t1 = 0, signs = 0, last = -1;
    for (int i = ncoeff - 1; i >= 0; i--) if (c[i]) { last = i; break; }
    if (last >= 0) {
        int skipped = 0;
        for (int i = last; i >= 0; i--) {
            if (c[i]) {
                if (total) run[total - 1] = skipped;
                level[total] = c[i];
                total++;
                skipped = 0;
            } else skipped++;
        }
        if (total) run[total - 1] = skipped;
        int lim = total < 3 ? total : 3;
        for (int i = 0; i < lim && (level[i] == 1 || level[i] == -1); i++) {
            signs = (signs << 1) | (level[i] < 0);
            t1++;
        }
    }
    const uint8_t *tok = vlc_token[nc_tab(nc)][total][t1];
    if (!tok[1]) return -2;
    unsigned bits = ((unsigned)tok[0] << t1) | (unsigned)signs;
    if (bw_put(w, bits, tok[1] + t1)) return -1;
    if (!total) return 0;
    int suffix = (total > 10 && t1 < 3) ? 1 : 0;
    for (int i = t1; i < total; i++) {
        int val = level[i];
        int code = (val - 1) * 2;
        unsigned sign = (unsigned)(code >> 31);
        code = (int)(((unsigned)code ^ sign) + (sign << 1));
        if (i == t1 && t1 < 3) code -= 2;
        int prefix = code >> suffix;
        int ssize = suffix;
        int sbits = code - (prefix << suffix);
        if (prefix >= 14 && prefix < 30 && suffix == 0) { prefix = 14; sbits = code - 14; ssize = 4; }
        else if (prefix >= 15) {
            sbits = code - (15 << suffix);
            if (suffix == 0) sbits -= 15;
            prefix = 15; ssize = 12;
            if (sbits < 0 || sbits > 4095) return -3;
        }
        if (bw_put(w, 1, prefix + 1)) return -1;
        if (ssize && bw_put(w, (unsigned)sbits, ssize)) return -1;
        suffix += !suffix;
        int thr = 3 << (suffix - 1);
        if ((val > thr || val < -thr) && suffix < 6) suffix++;
    }
    int tz = last + 1 - total;
    if (total < ncoeff) {
        const uint8_t *e = chroma_dc ? vlc_tzc[total][tz] : vlc_tz[total][tz];
        if (!e[1] || bw_put(w, e[0], e[1])) return -4;
    }
    int zleft = tz;
    for (int i = 0; i + 1 < total && zleft > 0; i++) {
        int idx = zleft > 7 ? 7 : zleft;
        const uint8_t *e = vlc_run[idx][run[i]];
        if (!e[1] || bw_put(w, e[0], e[1])) return -5;
        zleft -= run[i];
    }
    return 0;
}

static int vlc_match(const uint8_t tab[][2], int nent, int cols, br *r, int *a, int *b) {
    int acc = 0, n = 0;
    for (;;) {
        acc = (acc << 1) | br_get(r, 1);
        n++;
        if (r->err || n > 16) return -1;
        for (int i = 0; i < nent; i++) {
            const uint8_t *e = &tab[i * cols][0];
            for (int k = 0; k < cols; k++) {
                if (e[k * 2 + 1] == n && e[k * 2] == acc) { *a = i; *b = k; return 0; }
            }
        }
    }
}
static int cavlc_read(br *r, int *c, int ncoeff, int nc, int chroma_dc) {
    memset(c, 0, sizeof(int) * 16);
    int total = 0, t1 = 0;
    int acc = 0, n = 0, found = 0;
    int tab = nc_tab(nc);
    while (!found) {
        acc = (acc << 1) | br_get(r, 1);
        n++;
        if (r->err || n > 16) return -1;
        for (int tc = 0; tc <= (tab == 4 ? 4 : 16) && !found; tc++) {
            for (int ones = 0; ones < 4; ones++) {
                if (ones > tc) continue;
                const uint8_t *e = vlc_token[tab][tc][ones];
                if (e[1] == n && e[0] == acc) { total = tc; t1 = ones; found = 1; break; }
            }
        }
    }
    int level[16], run[16];
    memset(level, 0, sizeof level); memset(run, 0, sizeof run);
    for (int i = 0; i < t1; i++) level[i] = br_get(r, 1) ? -1 : 1;
    int suffix = (total > 10 && t1 < 3) ? 1 : 0;
    for (int i = t1; i < total; i++) {
        int prefix = 0;
        while (prefix < 20 && !br_get(r, 1)) prefix++;
        if (r->err) return -1;
        int ssize = suffix;
        if (prefix == 14 && suffix == 0) ssize = 4;
        else if (prefix >= 15) ssize = prefix - 3;
        int sbits = ssize ? br_get(r, ssize) : 0;
        int code = (prefix < 15 ? prefix : 15) << suffix;
        code += sbits;
        if (prefix >= 15 && suffix == 0) code += 15;
        if (prefix >= 16) code += (1 << (prefix - 3)) - 4096;
        if (i == t1 && t1 < 3) code += 2;
        int val = (code & 1) ? -((code + 1) / 2) : (code / 2) + 1;
        level[i] = val;
        suffix += !suffix;
        int thr = 3 << (suffix - 1);
        if ((val > thr || val < -thr) && suffix < 6) suffix++;
    }
    int tz = 0;
    if (total && total < ncoeff) {
        int dummy, zeros = -1;
        int accz = 0, nz = 0;
        int maxz = ncoeff - total;
        while (zeros < 0) {
            accz = (accz << 1) | br_get(r, 1);
            nz++;
            if (r->err || nz > 16) return -1;
            for (int z = 0; z <= maxz; z++) {
                const uint8_t *e = chroma_dc ? vlc_tzc[total][z] : vlc_tz[total][z];
                if (e[1] == nz && e[0] == accz) { zeros = z; break; }
            }
        }
        tz = zeros;
        (void)dummy;
    }
    int zleft = tz;
    for (int i = 0; i + 1 < total && zleft > 0; i++) {
        int idx = zleft > 7 ? 7 : zleft;
        int accr = 0, nr = 0, runv = -1;
        while (runv < 0) {
            accr = (accr << 1) | br_get(r, 1);
            nr++;
            if (r->err || nr > 16) return -1;
            for (int k = 0; k <= 14 && k < 15; k++) {
                const uint8_t *e = vlc_run[idx][k];
                if (e[1] == nr && e[0] == accr) { runv = k; break; }
            }
        }
        run[i] = runv;
        zleft -= runv;
    }
    if (total) run[total - 1] = zleft;
    int pos = tz + total - 1;
    for (int i = 0; i < total; i++) {
        if (pos < 0 || pos >= ncoeff) return -1;
        c[pos] = level[i];
        if (i + 1 < total) pos -= run[i] + 1;
    }
    return r->err ? -1 : 0;
}

static int pred_dc(const uint8_t *top, const uint8_t *left, int n, int has_t, int has_l) {
    int s = 0;
    if (has_t && has_l) { for (int i = 0; i < n; i++) s += top[i] + left[i]; return (s + n) >> (n == 16 ? 5 : 4); }
    if (has_t) { for (int i = 0; i < n; i++) s += top[i]; return (s + n / 2) >> (n == 16 ? 4 : 3); }
    if (has_l) { for (int i = 0; i < n; i++) s += left[i]; return (s + n / 2) >> (n == 16 ? 4 : 3); }
    return 128;
}

typedef struct {
    int w, h, cw, ch, mbw, mbh, qp;
    uint8_t *y, *u, *v;
    uint8_t *nnz, *cnnz;
} pic;

static int pget(pic *p, int plane, int x, int yy) {
    int W = plane ? p->cw : p->w, H = plane ? p->ch : p->h;
    uint8_t *b = plane == 0 ? p->y : plane == 1 ? p->u : p->v;
    if (x < 0 || yy < 0 || x >= W || yy >= H) return 0;
    return b[yy * W + x];
}
static void pput(pic *p, int plane, int x, int yy, int v) {
    int W = plane ? p->cw : p->w;
    uint8_t *b = plane == 0 ? p->y : plane == 1 ? p->u : p->v;
    b[yy * W + x] = (uint8_t)clip8(v);
}

static int pcm_align(bw *w) {
    int r = w->bits & 7;
    return r ? bw_put(w, 0, 8 - r) : 0;
}
static int code_mb(bw *w, pic *p, const uint8_t *sy, int ystride, const uint8_t *su, int sustride, const uint8_t *sv, int svstride, int mbx, int mby, int write) {
    /* I_PCM: mb_type 25, alinhado em byte, 16x16 + 8x8 + 8x8. Sem residual CAVLC. */
    if (write) {
        if (bw_ue(w, 25) || pcm_align(w)) return -1;
        int x0 = mbx * 16, y0 = mby * 16;
        for (int row = 0; row < 16; row++) for (int col = 0; col < 16; col++) {
            int yy = y0 + row, xx = x0 + col;
            int s = (yy < p->h && xx < p->w) ? sy[yy * ystride + xx] : 0;
            if (bw_put(w, (unsigned)s, 8)) return -1;
            if (yy < p->h && xx < p->w) pput(p, 0, xx, yy, s);
        }
        for (int pidx = 0; pidx < 2; pidx++) {
            const uint8_t *src = pidx ? sv : su;
            int stride = pidx ? svstride : sustride;
            int cx0 = mbx * 8, cy0 = mby * 8;
            for (int row = 0; row < 8; row++) for (int col = 0; col < 8; col++) {
                int yy = cy0 + row, xx = cx0 + col;
                int s = (yy < p->ch && xx < p->cw) ? src[yy * stride + xx] : 0;
                if (bw_put(w, (unsigned)s, 8)) return -1;
                if (yy < p->ch && xx < p->cw) pput(p, pidx + 1, xx, yy, s);
            }
        }
        return 0;
    }
    return 0;
}
static int code_mb_unused(bw *w, pic *p, const uint8_t *sy, int ystride, const uint8_t *su, int sustride, const uint8_t *sv, int svstride, int mbx, int mby, int write) {
    (void)w; (void)p; (void)sy; (void)ystride; (void)su; (void)sustride; (void)sv; (void)svstride; (void)mbx; (void)mby; (void)write;
    int x0 = mbx * 16, y0 = mby * 16;
    int cx0 = mbx * 8, cy0 = mby * 8;
    uint8_t top[16], left[16];
    int has_t = mby > 0, has_l = mbx > 0;
    for (int i = 0; i < 16; i++) {
        top[i] = has_t ? pget(p, 0, x0 + i, y0 - 1) : 0;
        left[i] = has_l ? pget(p, 0, x0 - 1, y0 + i) : 0;
    }
    int pred = pred_dc(top, left, 16, has_t, has_l);
    int dct[16][16], dc[16], pix[16][16];
    for (int by = 0; by < 4; by++) for (int bx = 0; bx < 4; bx++) {
        int blk[16], out[16];
        for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) {
            int yy = y0 + by * 4 + r, xx = x0 + bx * 4 + c;
            int s = (yy < p->h && xx < p->w) ? sy[yy * ystride + xx] : pred;
            pix[by * 4 + r][bx * 4 + c] = s;
            blk[r * 4 + c] = s - pred;
        }
        fdct4(blk, out);
        int b = by * 4 + bx;
        for (int k = 0; k < 16; k++) dct[b][k] = out[k];
        dc[b] = out[0];
    }
    had4(dc);
    int qdc[16];
    for (int i = 0; i < 16; i++) qdc[i] = q_dc(dc[i], p->qp);
    int qac[16][15], any_ac = 0, nnz[16];
    for (int b = 0; b < 16; b++) {
        int nz = 0;
        for (int k = 1; k < 16; k++) {
            int lv = q_ac(dct[b][zz[k]], p->qp, icat(zz[k]));
            qac[b][k - 1] = lv;
            if (lv) { any_ac = 1; nz++; }
        }
        nnz[b] = nz;
    }
    int cdc[2][4], cac[2][4][15], cnz[2][4], cbp_c = 0;
    uint8_t ctop[2][8], cleft[2][8];
    const uint8_t *csrc[2] = {su, sv};
    int cstrides[2] = {sustride, svstride};
    for (int pidx = 0; pidx < 2; pidx++) {
        int cstride = cstrides[pidx];
        for (int i = 0; i < 8; i++) {
            ctop[pidx][i] = has_t ? pget(p, pidx + 1, cx0 + i, cy0 - 1) : 0;
            cleft[pidx][i] = has_l ? pget(p, pidx + 1, cx0 - 1, cy0 + i) : 0;
        }
        int cpred = pred_dc(ctop[pidx], cleft[pidx], 8, has_t, has_l);
        int cd[4];
        for (int by = 0; by < 2; by++) for (int bx = 0; bx < 2; bx++) {
            int blk[16], out[16];
            for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) {
                int yy = cy0 + by * 4 + r, xx = cx0 + bx * 4 + c;
                int s = (yy < p->ch && xx < p->cw) ? csrc[pidx][yy * cstride + xx] : cpred;
                blk[r * 4 + c] = s - cpred;
            }
            fdct4(blk, out);
            int b = by * 2 + bx;
            cd[b] = out[0];
            int nz = 0;
            for (int k = 1; k < 16; k++) {
                int lv = q_ac(out[zz[k]], p->qp, icat(zz[k]));
                cac[pidx][b][k - 1] = lv;
                if (lv) nz++;
            }
            cnz[pidx][b] = nz;
            if (nz) cbp_c = 2;
        }
        int a = cd[0] + cd[1], b = cd[2] + cd[3], c = cd[0] - cd[1], d = cd[2] - cd[3];
        int h0 = a + b, h1 = c + d, h2 = a - b, h3 = c - d;
        int hq[4] = {q_dc(h0, p->qp), q_dc(h1, p->qp), q_dc(h2, p->qp), q_dc(h3, p->qp)};
        for (int i = 0; i < 4; i++) cdc[pidx][i] = hq[i];
        if ((hq[0] | hq[1] | hq[2] | hq[3]) && cbp_c < 1) cbp_c = 1;
    }
    int scan_dc[16];
    for (int i = 0; i < 16; i++) scan_dc[i] = qdc[zz[i]];
    if (write) {
        /* mb_type: 0 DC, 1 horizontal, 2 vertical, 3 plane. DC é 2. */
        int mb_type = 1 + 2 + (cbp_c << 2) + (any_ac ? 12 : 0);
        /* I_16x16: só intra_chroma_pred_mode. O modo 16x16 já está no mb_type. */
        if (bw_ue(w, (unsigned)mb_type) || bw_ue(w, 0)) return -1;
        int crv;
        if ((crv = cavlc_write(w, scan_dc, 16, 0, 0))) return crv;
        if (any_ac) {
            for (int b = 0; b < 16; b++) {
                int blx = b & 3, bly = b >> 2;
                int na = 0, nb = 0, ha = 0, hb = 0;
                if (blx) { na = nnz[b - 1]; ha = 1; }
                else if (has_l) { na = p->nnz[(mby * p->mbw + (mbx - 1)) * 16 + b + 3]; ha = 1; }
                if (bly) { nb = nnz[b - 4]; hb = 1; }
                else if (has_t) { nb = p->nnz[((mby - 1) * p->mbw + mbx) * 16 + b + 12]; hb = 1; }
                int nc = (ha && hb) ? (na + nb + 1) >> 1 : ha ? na : hb ? nb : 0;
                if ((crv = cavlc_write(w, qac[b], 15, nc, 0))) return crv;
            }
        }
        for (int pidx = 0; pidx < 2; pidx++) if ((crv = cavlc_write(w, cdc[pidx], 4, -1, 1))) return crv;
        if (cbp_c == 2) {
            for (int pidx = 0; pidx < 2; pidx++) for (int b = 0; b < 4; b++) {
                int blx = b & 1, bly = b >> 1;
                int na = 0, nb = 0, ha = 0, hb = 0;
                if (blx) { na = cnz[pidx][b - 1]; ha = 1; }
                else if (has_l) { na = p->cnnz[((mby * p->mbw + (mbx - 1)) * 2 + pidx) * 4 + b + 1]; ha = 1; }
                if (bly) { nb = cnz[pidx][b - 2]; hb = 1; }
                else if (has_t) { nb = p->cnnz[(((mby - 1) * p->mbw + mbx) * 2 + pidx) * 4 + b + 2]; hb = 1; }
                int nc = (ha && hb) ? (na + nb + 1) >> 1 : ha ? na : hb ? nb : 0;
                if ((crv = cavlc_write(w, cac[pidx][b], 15, nc, 0))) return crv;
            }
        }
    }
    /* reconstrução (a mesma do decodificador) */
    int idc[16];
    for (int i = 0; i < 16; i++) idc[i] = dq_dc_luma(qdc[i], p->qp);
    had4(idc);
    for (int i = 0; i < 16; i++) idc[i] = (idc[i] + 2) >> 2;
    for (int b = 0; b < 16; b++) {
        int coef[16];
        memset(coef, 0, sizeof coef);
        coef[0] = idc[b];
        if (any_ac) for (int k = 1; k < 16; k++) coef[zz[k]] = dq_ac(qac[b][k - 1], p->qp, icat(zz[k]));
        idct4(coef);
        int bx = b & 3, by = b >> 2;
        for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) {
            int yy = y0 + by * 4 + r, xx = x0 + bx * 4 + c;
            if (yy < p->h && xx < p->w) pput(p, 0, xx, yy, pred + coef[r * 4 + c]);
        }
        p->nnz[(mby * p->mbw + mbx) * 16 + b] = (uint8_t)nnz[b];
    }
    for (int pidx = 0; pidx < 2; pidx++) {
        int h[4];
        for (int i = 0; i < 4; i++) h[i] = dq_dc_chroma(cdc[pidx][i], p->qp);
        int a = h[0] + h[1], b = h[2] + h[3], c = h[0] - h[1], d = h[2] - h[3];
        int rec[4] = {(a + b) >> 1, (c + d) >> 1, (a - b) >> 1, (c - d) >> 1};
        int cpred = pred_dc(ctop[pidx], cleft[pidx], 8, has_t, has_l);
        for (int blk = 0; blk < 4; blk++) {
            int coef[16];
            memset(coef, 0, sizeof coef);
            coef[0] = rec[blk];
            if (cbp_c == 2) for (int k = 1; k < 16; k++) coef[zz[k]] = dq_ac(cac[pidx][blk][k - 1], p->qp, icat(zz[k]));
            idct4(coef);
            int bx = blk & 1, by = blk >> 1;
            for (int r = 0; r < 4; r++) for (int col = 0; col < 4; col++) {
                int yy = cy0 + by * 4 + r, xx = cx0 + bx * 4 + col;
                if (yy < p->ch && xx < p->cw) pput(p, pidx + 1, xx, yy, cpred + coef[r * 4 + col]);
            }
            p->cnnz[((mby * p->mbw + mbx) * 2 + pidx) * 4 + blk] = (uint8_t)cnz[pidx][blk];
        }
    }
    (void)pix;
    return 0;
}

static void br_align(br *r) {
    int n = (8 - (r->bit & 7)) & 7;
    if (n) br_get(r, n);
}
static int dec_mb(br *r, pic *p, int mbx, int mby) {
    int mb_type = br_ue(r);
    if (r->err || mb_type != 25) return -1;
    br_align(r);
    int x0 = mbx * 16, y0 = mby * 16;
    for (int row = 0; row < 16; row++) for (int col = 0; col < 16; col++) {
        int s = br_get(r, 8);
        int yy = y0 + row, xx = x0 + col;
        if (yy < p->h && xx < p->w) pput(p, 0, xx, yy, s);
    }
    for (int pidx = 0; pidx < 2; pidx++) {
        int cx0 = mbx * 8, cy0 = mby * 8;
        for (int row = 0; row < 8; row++) for (int col = 0; col < 8; col++) {
            int s = br_get(r, 8);
            int yy = cy0 + row, xx = cx0 + col;
            if (yy < p->ch && xx < p->cw) pput(p, pidx + 1, xx, yy, s);
        }
    }
    return r->err ? -1 : 0;
}
static int dec_mb_cavlc_unused(br *r, pic *p, int mbx, int mby) {
    int mb_type = br_ue(r);
    if (r->err || mb_type < 1 || mb_type > 24) return -1;
    int code = mb_type - 1;
    int any_ac = code >= 12;
    if (any_ac) code -= 12;
    int mode = code & 3;
    int cbp_c = code >> 2;
    if (mode != 2) return -1;
    if (br_ue(r) != 0) return -1;
    if (br_se(r) != 0) return -1;
    int x0 = mbx * 16, y0 = mby * 16, cx0 = mbx * 8, cy0 = mby * 8;
    int has_t = mby > 0, has_l = mbx > 0;
    uint8_t top[16], left[16];
    for (int i = 0; i < 16; i++) {
        top[i] = has_t ? pget(p, 0, x0 + i, y0 - 1) : 0;
        left[i] = has_l ? pget(p, 0, x0 - 1, y0 + i) : 0;
    }
    int pred = pred_dc(top, left, 16, has_t, has_l);
    int scan_dc[16];
    if (cavlc_read(r, scan_dc, 16, 0, 0)) return -1;
    int qdc[16], idc[16];
    for (int i = 0; i < 16; i++) qdc[zz[i]] = scan_dc[i];
    for (int i = 0; i < 16; i++) idc[i] = dq_dc_luma(qdc[i], p->qp);
    had4(idc);
    for (int i = 0; i < 16; i++) idc[i] = (idc[i] + 2) >> 2;
    for (int b = 0; b < 16; b++) {
        int ac[16]; memset(ac, 0, sizeof ac);
        int nc = 0;
        if (any_ac) {
            int blx = b & 3, bly = b >> 2, na = 0, nb = 0, ha = 0, hb = 0;
            if (blx) { na = p->nnz[(mby * p->mbw + mbx) * 16 + b - 1]; ha = 1; }
            else if (has_l) { na = p->nnz[(mby * p->mbw + mbx - 1) * 16 + b + 3]; ha = 1; }
            if (bly) { nb = p->nnz[(mby * p->mbw + mbx) * 16 + b - 4]; hb = 1; }
            else if (has_t) { nb = p->nnz[((mby - 1) * p->mbw + mbx) * 16 + b + 12]; hb = 1; }
            nc = (ha && hb) ? (na + nb + 1) >> 1 : ha ? na : hb ? nb : 0;
            if (cavlc_read(r, ac, 15, nc, 0)) return -1;
        }
        int nz = 0; for (int k = 0; k < 15; k++) if (ac[k]) nz++;
        p->nnz[(mby * p->mbw + mbx) * 16 + b] = (uint8_t)nz;
        int coef[16]; memset(coef, 0, sizeof coef);
        coef[0] = idc[b];
        for (int k = 1; k < 16; k++) coef[zz[k]] = dq_ac(ac[k - 1], p->qp, icat(zz[k]));
        idct4(coef);
        int bx = b & 3, by = b >> 2;
        for (int row = 0; row < 4; row++) for (int col = 0; col < 4; col++) {
            int yy = y0 + by * 4 + row, xx = x0 + bx * 4 + col;
            if (yy < p->h && xx < p->w) pput(p, 0, xx, yy, pred + coef[row * 4 + col]);
        }
    }
    int cdc[2][4];
    for (int pidx = 0; pidx < 2; pidx++) if (cavlc_read(r, cdc[pidx], 4, -1, 1)) return -1;
    for (int pidx = 0; pidx < 2; pidx++) {
        uint8_t ctop[8], cleft[8];
        for (int i = 0; i < 8; i++) {
            ctop[i] = has_t ? pget(p, pidx + 1, cx0 + i, cy0 - 1) : 0;
            cleft[i] = has_l ? pget(p, pidx + 1, cx0 - 1, cy0 + i) : 0;
        }
        int cpred = pred_dc(ctop, cleft, 8, has_t, has_l);
        int h[4];
        for (int i = 0; i < 4; i++) h[i] = dq_dc_chroma(cdc[pidx][i], p->qp);
        int a = h[0] + h[1], b = h[2] + h[3], c = h[0] - h[1], d = h[2] - h[3];
        int rec[4] = {(a + b) >> 1, (c + d) >> 1, (a - b) >> 1, (c - d) >> 1};
        for (int blk = 0; blk < 4; blk++) {
            int ac[16]; memset(ac, 0, sizeof ac);
            if (cbp_c == 2) {
                int blx = blk & 1, bly = blk >> 1, na = 0, nb = 0, ha = 0, hb = 0;
                if (blx) { na = p->cnnz[((mby * p->mbw + mbx) * 2 + pidx) * 4 + blk - 1]; ha = 1; }
                else if (has_l) { na = p->cnnz[((mby * p->mbw + mbx - 1) * 2 + pidx) * 4 + blk + 1]; ha = 1; }
                if (bly) { nb = p->cnnz[((mby * p->mbw + mbx) * 2 + pidx) * 4 + blk - 2]; hb = 1; }
                else if (has_t) { nb = p->cnnz[(((mby - 1) * p->mbw + mbx) * 2 + pidx) * 4 + blk + 2]; hb = 1; }
                int nc = (ha && hb) ? (na + nb + 1) >> 1 : ha ? na : hb ? nb : 0;
                if (cavlc_read(r, ac, 15, nc, 0)) return -1;
            }
            int nz = 0; for (int k = 0; k < 15; k++) if (ac[k]) nz++;
            p->cnnz[((mby * p->mbw + mbx) * 2 + pidx) * 4 + blk] = (uint8_t)nz;
            int coef[16]; memset(coef, 0, sizeof coef);
            coef[0] = rec[blk];
            for (int k = 1; k < 16; k++) coef[zz[k]] = dq_ac(ac[k - 1], p->qp, icat(zz[k]));
            idct4(coef);
            int bx = blk & 1, by = blk >> 1;
            for (int row = 0; row < 4; row++) for (int col = 0; col < 4; col++) {
                int yy = cy0 + by * 4 + row, xx = cx0 + bx * 4 + col;
                if (yy < p->ch && xx < p->cw) pput(p, pidx + 1, xx, yy, cpred + coef[row * 4 + col]);
            }
        }
    }
    return r->err ? -1 : 0;
}

static int write_sps(bw *w, int wpx, int hpx) {
    int mbw = (wpx + 15) / 16, mbh = (hpx + 15) / 16;
    int crop_r = mbw * 16 - wpx, crop_b = mbh * 16 - hpx;
    if (bw_put(w, 66, 8) || bw_put(w, 1 << 7 | 1 << 6, 8) || bw_put(w, 31, 8)) return -1;
    if (bw_ue(w, 0) || bw_ue(w, 0) || bw_ue(w, 0) || bw_ue(w, 0)) return -1;
    if (bw_ue(w, 1) || bw_put(w, 0, 1)) return -1;
    if (bw_ue(w, (unsigned)(mbw - 1)) || bw_ue(w, (unsigned)(mbh - 1))) return -1;
    if (bw_put(w, 1, 1) || bw_put(w, 1, 1)) return -1;
    if (crop_r || crop_b) {
        if (bw_put(w, 1, 1)) return -1;
        if (bw_ue(w, 0) || bw_ue(w, (unsigned)(crop_r / 2)) || bw_ue(w, 0) || bw_ue(w, (unsigned)(crop_b / 2))) return -1;
    } else if (bw_put(w, 0, 1)) return -1;
    if (bw_put(w, 0, 1)) return -1;
    return bw_trail(w);
}
static int write_pps(bw *w) {
    if (bw_ue(w, 0) || bw_ue(w, 0) || bw_put(w, 0, 1) || bw_put(w, 0, 1)) return -1;
    if (bw_ue(w, 0) || bw_ue(w, 0) || bw_ue(w, 0) || bw_put(w, 0, 1) || bw_put(w, 0, 2)) return -1;
    if (bw_se(w, 0) || bw_se(w, 0) || bw_se(w, 0)) return -1;
    if (bw_put(w, 1, 1) || bw_put(w, 0, 1) || bw_put(w, 0, 1)) return -1;
    return bw_trail(w);
}

static int parse_sps(br *r, int *w, int *h, int *mbw, int *mbh) {
    if (br_get(r, 8) != 66) return -1;
    br_get(r, 8); br_get(r, 8);
    br_ue(r); br_ue(r); br_ue(r); br_ue(r);
    if (br_ue(r) != 1) return -1;
    br_get(r, 1);
    *mbw = br_ue(r) + 1; *mbh = br_ue(r) + 1;
    if (!br_get(r, 1)) return -1;
    br_get(r, 1);
    int cr = 0, cb = 0;
    if (br_get(r, 1)) { br_ue(r); cr = br_ue(r); br_ue(r); cb = br_ue(r); }
    *w = *mbw * 16 - cr * 2; *h = *mbh * 16 - cb * 2;
    return r->err ? -1 : 0;
}

static uint8_t *nal_from_rbsp(int nal_type, bw *rbsp, int *n) {
    int rn = rbsp->bits >> 3;
    uint8_t *raw = malloc((size_t)rn + 1);
    if (!raw) return 0;
    raw[0] = (uint8_t)((3 << 5) | nal_type);
    memcpy(raw + 1, rbsp->b, (size_t)rn);
    uint8_t *esc = rbsp_escape(raw, rn + 1, n);
    free(raw);
    return esc;
}

int h264_sps_wh(const uint8_t *nal, int n, int *w, int *h) {
    int rn = 0;
    uint8_t *rb = rbsp_unescape(nal, n, &rn);
    if (!rb || rn < 4) { free(rb); return -1; }
    br r = {rb + 1, rn - 1, 0, 0};
    int mbw = 0, mbh = 0;
    int rc = parse_sps(&r, w, h, &mbw, &mbh);
    free(rb);
    return rc;
}

static int pic_alloc(pic *p, int w, int h, int qp) {
    memset(p, 0, sizeof *p);
    p->w = (w + 15) & ~15; p->h = (h + 15) & ~15;
    p->cw = p->w / 2; p->ch = p->h / 2;
    p->mbw = p->w / 16; p->mbh = p->h / 16; p->qp = qp;
    p->y = calloc((size_t)p->w * (size_t)p->h, 1);
    p->u = calloc((size_t)p->cw * (size_t)p->ch, 1);
    p->v = calloc((size_t)p->cw * (size_t)p->ch, 1);
    p->nnz = calloc((size_t)p->mbw * (size_t)p->mbh * 16, 1);
    p->cnnz = calloc((size_t)p->mbw * (size_t)p->mbh * 8, 1);
    if (!p->y || !p->u || !p->v || !p->nnz || !p->cnnz) return -1;
    return 0;
}
static void pic_free(pic *p) { free(p->y); free(p->u); free(p->v); free(p->nnz); free(p->cnnz); }

void h264_au_free(h264_au *o) {
    if (!o) return;
    free(o->sps); free(o->pps); free(o->sample); free(o->annexb);
    memset(o, 0, sizeof *o);
}

int h264_encode_i420(const uint8_t *y, const uint8_t *u, const uint8_t *v,
                     int w, int h, int sy, int su, int sv, int qp, h264_au *o) {
    if (!y || !u || !v || !o || w < 2 || h < 2 || (w & 1) || (h & 1) || w > 4096 || h > 4096) return -1;
    if (qp < 0) qp = 0;
    if (qp > 51) qp = 51;
    memset(o, 0, sizeof *o);
    o->w = w; o->h = h;
    bw sps = {0}, pps = {0}, sl = {0};
    if (write_sps(&sps, w, h) || write_pps(&pps)) { free(sps.b); free(pps.b); return -2; }
    pic p;
    if (pic_alloc(&p, w, h, qp)) { pic_free(&p); free(sps.b); free(pps.b); return -3; }
    if (bw_ue(&sl, 0) || bw_ue(&sl, 7) || bw_ue(&sl, 0) || bw_put(&sl, 0, 4)) { free(sps.b); free(pps.b); free(sl.b); pic_free(&p); return -4; }
    if (bw_ue(&sl, 0) || bw_put(&sl, 0, 4) || bw_put(&sl, 0, 1) || bw_put(&sl, 0, 1)) { free(sps.b); free(pps.b); free(sl.b); pic_free(&p); return -5; }
    if (bw_se(&sl, qp - 26) || bw_ue(&sl, 1)) { free(sps.b); free(pps.b); free(sl.b); pic_free(&p); return -6; }
    for (int my = 0; my < p.mbh; my++) for (int mx = 0; mx < p.mbw; mx++) {
        int mbr = code_mb(&sl, &p, y, sy, u, su, v, sv, mx, my, 1);
        if (mbr) { free(sps.b); free(pps.b); free(sl.b); pic_free(&p); return mbr; }
    }
    if (bw_trail(&sl)) goto fail;
    int sn = 0, pn = 0, ln = 0;
    o->sps = nal_from_rbsp(7, &sps, &sn);
    o->pps = nal_from_rbsp(8, &pps, &pn);
    o->sample = 0;
    uint8_t *slice = nal_from_rbsp(5, &sl, &ln);
    o->sps_n = sn; o->pps_n = pn;
    if (!o->sps || !o->pps || !slice) { free(slice); goto fail; }
    o->sample = malloc((size_t)ln + 4);
    if (!o->sample) { free(slice); goto fail; }
    o->sample[0] = (uint8_t)(ln >> 24); o->sample[1] = (uint8_t)(ln >> 16);
    o->sample[2] = (uint8_t)(ln >> 8); o->sample[3] = (uint8_t)ln;
    memcpy(o->sample + 4, slice, (size_t)ln);
    o->sample_n = ln + 4;
    o->annexb_n = 4 + sn + 4 + pn + 4 + ln;
    o->annexb = malloc((size_t)o->annexb_n);
    if (!o->annexb) { free(slice); goto fail; }
    uint8_t *d = o->annexb;
    #define SC(dst) do { (dst)[0]=0;(dst)[1]=0;(dst)[2]=0;(dst)[3]=1; dst += 4; } while (0)
    SC(d); memcpy(d, o->sps, (size_t)sn); d += sn;
    SC(d); memcpy(d, o->pps, (size_t)pn); d += pn;
    SC(d); memcpy(d, slice, (size_t)ln);
    #undef SC
    free(slice); free(sps.b); free(pps.b); free(sl.b); pic_free(&p);
    return 0;
fail:
    free(sps.b); free(pps.b); free(sl.b); pic_free(&p); h264_au_free(o);
    return -1;
}

static int decode_slice_rbsp(const uint8_t *rb, int n, pic *p) {
    br r = {rb, n, 0, 0};
    if (br_ue(&r) != 0) return -1;
    int st = br_ue(&r);
    if (st != 2 && st != 7) return -1;
    if (br_ue(&r) != 0) return -1;
    br_get(&r, 4);
    if (br_ue(&r) != 0) return -1;
    br_get(&r, 4);
    br_get(&r, 1); br_get(&r, 1);
    int qp = 26 + br_se(&r);
    if (qp < 0 || qp > 51) return -1;
    p->qp = qp;
    if (br_ue(&r) != 1) return -1;
    for (int my = 0; my < p->mbh; my++) for (int mx = 0; mx < p->mbw; mx++) {
        int dr = dec_mb(&r, p, mx, my);
        if (dr) return -(100 + my * p->mbw + mx); }
    return r.err ? -1 : 0;
}

int h264_bsd_open(void **out);
void h264_bsd_close(void *p);
int h264_bsd_feed(void *p, const uint8_t *bytes, int n,
                  uint8_t *y, uint8_t *u, uint8_t *v, int sy, int su, int sv,
                  int *ow, int *oh);

static int annex_put(uint8_t *d, const uint8_t *nal, int n) {
    d[0] = 0; d[1] = 0; d[2] = 0; d[3] = 1;
    memcpy(d + 4, nal, (size_t)n);
    return n + 4;
}

int h264_decode_nals(const uint8_t *sps, int sps_n, const uint8_t *pps, int pps_n,
                     const uint8_t *slice, int slice_n,
                     uint8_t *y, uint8_t *u, uint8_t *v, int sy, int su, int sv,
                     int *ow, int *oh) {
    void *st = 0;
    int rc;
    if (!sps || sps_n < 1 || !pps || pps_n < 1 || !slice || slice_n < 1 || !y) return -1;
    uint8_t *b = malloc((size_t)(12 + sps_n + pps_n + slice_n));
    if (!b) return -1;
    int n = 0;
    n += annex_put(b + n, sps, sps_n);
    n += annex_put(b + n, pps, pps_n);
    n += annex_put(b + n, slice, slice_n);
    if (h264_bsd_open(&st)) { free(b); return -1; }
    rc = h264_bsd_feed(st, b, n, y, u, v, sy, su, sv, ow, oh);
    h264_bsd_close(st);
    free(b);
    return rc;
}

int h264_decode_avcc(const uint8_t *sps, int sps_n, const uint8_t *pps, int pps_n,
                     const uint8_t *sample, int sample_n,
                     uint8_t *y, uint8_t *u, uint8_t *v, int sy, int su, int sv,
                     int *ow, int *oh) {
    void *st = 0;
    int rc, off = 0, n = 0;
    if (!sps || !pps || !sample || sample_n < 5) return -1;
    uint8_t *b = malloc((size_t)sample_n + (size_t)sps_n + (size_t)pps_n + 64);
    if (!b) return -1;
    n += annex_put(b + n, sps, sps_n);
    n += annex_put(b + n, pps, pps_n);
    while (off + 4 <= sample_n) {
        int len = (sample[off] << 24) | (sample[off+1] << 16) | (sample[off+2] << 8) | sample[off+3];
        off += 4;
        if (len <= 0 || off + len > sample_n) { free(b); return -1; }
        n += annex_put(b + n, sample + off, len);
        off += len;
    }
    if (h264_bsd_open(&st)) { free(b); return -1; }
    rc = h264_bsd_feed(st, b, n, y, u, v, sy, su, sv, ow, oh);
    h264_bsd_close(st);
    free(b);
    return rc;
}

static int clamp_add(int a, int b) { return clip8(a + b); }
void h264_bgra_to_i420(const uint8_t *bgra, int w, int h, int sb,
                       uint8_t *y, uint8_t *u, uint8_t *v, int sy, int su, int sv) {
    for (int row = 0; row < h; row++) for (int col = 0; col < w; col++) {
        const uint8_t *p = bgra + row * sb + col * 4;
        int B = p[0], G = p[1], R = p[2];
        y[row * sy + col] = (uint8_t)((77 * R + 150 * G + 29 * B + 128) >> 8);
    }
    for (int row = 0; row < h / 2; row++) for (int col = 0; col < w / 2; col++) {
        int R = 0, G = 0, B = 0;
        for (int dy = 0; dy < 2; dy++) for (int dx = 0; dx < 2; dx++) {
            const uint8_t *p = bgra + (row * 2 + dy) * sb + (col * 2 + dx) * 4;
            B += p[0]; G += p[1]; R += p[2];
        }
        R >>= 2; G >>= 2; B >>= 2;
        u[row * su + col] = (uint8_t)clip8((((-43 * R - 85 * G + 128 * B + 128) >> 8) + 128));
        v[row * sv + col] = (uint8_t)clip8((((128 * R - 107 * G - 21 * B + 128) >> 8) + 128));
    }
}
void h264_i420_to_bgra(const uint8_t *y, const uint8_t *u, const uint8_t *v,
                       int w, int h, int sy, int su, int sv, uint8_t *bgra, int sb) {
    for (int row = 0; row < h; row++) for (int col = 0; col < w; col++) {
        int Y = y[row * sy + col];
        int U = u[(row / 2) * su + col / 2] - 128;
        int V = v[(row / 2) * sv + col / 2] - 128;
        uint8_t *p = bgra + row * sb + col * 4;
        p[2] = (uint8_t)clamp_add(Y, (359 * V) >> 8);
        p[1] = (uint8_t)clamp_add(Y, -(88 * U + 183 * V) >> 8);
        p[0] = (uint8_t)clamp_add(Y, (454 * U) >> 8);
        p[3] = 255;
    }
}
void h264_nv12_to_i420(const uint8_t *yp, int sy, const uint8_t *uv, int suv,
                       int w, int h, uint8_t *y, uint8_t *u, uint8_t *v, int dy, int du, int dv) {
    for (int row = 0; row < h; row++) memcpy(y + row * dy, yp + row * sy, (size_t)w);
    for (int row = 0; row < h / 2; row++) for (int col = 0; col < w / 2; col++) {
        u[row * du + col] = uv[row * suv + col * 2];
        v[row * dv + col] = uv[row * suv + col * 2 + 1];
    }
}
void h264_i420_to_nv12(const uint8_t *y, const uint8_t *u, const uint8_t *v,
                       int w, int h, int sy, int su, int sv, uint8_t *yp, int dy, uint8_t *uv, int duv) {
    for (int row = 0; row < h; row++) memcpy(yp + row * dy, y + row * sy, (size_t)w);
    for (int row = 0; row < h / 2; row++) for (int col = 0; col < w / 2; col++) {
        uv[row * duv + col * 2] = u[row * su + col];
        uv[row * duv + col * 2 + 1] = v[row * sv + col];
    }
}
