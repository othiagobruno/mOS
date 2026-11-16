/*
 * mOS ColorSync — motor de cor e a API ColorSyncProfile / ColorSyncTransform.
 *
 * Motor: perfis ICC v2/v4 (matriz/TRC com curv e para, cinza com kTRC, LUTs lut8/lut16 e lutAtoB/lutBtoA),
 * PCS XYZ ou Lab, conversão dispositivo ↔ XYZ D50. Os perfis padrão (sRGB, Display P3, Adobe RGB (1998),
 * ITU-R 709/2020, lineares, cinza 2.2, Lab, CMYK genérico) são montados aqui a partir das primárias e curvas
 * publicadas nas normas (IEC 61966-2-1, SMPTE EG 432-1, Adobe RGB 1998 spec, ITU-R BT.709/2020) — nada da Apple.
 * Adaptação de branco: Bradford.
 *
 * Teste de host: tools/colorsync_host.c (compila com -DCS_HOST, sem a parte CF).
 */
#ifdef CS_HOST
#include <math.h>
#include <stdlib.h>
#include <string.h>
#else
#include "sys.h"
#include "libc.h"
double pow(double, double);
double log(double);
double exp(double);
char *strstr(const char *h, const char *n);
#endif
#include "colorsync.h"

typedef uint8_t u8_;
static uint32_t be32c(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
static uint32_t be16c(const uint8_t *p) { return (uint32_t)p[0] << 8 | p[1]; }
static double s15(const uint8_t *p) { return (double)(int32_t)be32c(p) / 65536.0; }
#define SIG(a, b, c, d) ((uint32_t)(a) << 24 | (uint32_t)(b) << 16 | (uint32_t)(c) << 8 | (uint32_t)(d))

static const double D50[3] = {0.9642, 1.0, 0.8249};
static const double D65[3] = {0.95047, 1.0, 1.08883};

/* ---------------------------------------------------------------- álgebra */
static void m3_mul(const double *a, const double *b, double *o) {
    double t[9];
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) t[i * 3 + j] = a[i * 3] * b[j] + a[i * 3 + 1] * b[3 + j] + a[i * 3 + 2] * b[6 + j];
    memcpy(o, t, sizeof t);
}
static void m3_vec(const double *m, const double *v, double *o) {
    double t[3] = {m[0] * v[0] + m[1] * v[1] + m[2] * v[2], m[3] * v[0] + m[4] * v[1] + m[5] * v[2], m[6] * v[0] + m[7] * v[1] + m[8] * v[2]};
    o[0] = t[0]; o[1] = t[1]; o[2] = t[2];
}
static int m3_inv(const double *m, double *o) {
    double det = m[0] * (m[4] * m[8] - m[5] * m[7]) - m[1] * (m[3] * m[8] - m[5] * m[6]) + m[2] * (m[3] * m[7] - m[4] * m[6]);
    if (det > -1e-15 && det < 1e-15) return 0;
    double t[9] = {(m[4] * m[8] - m[5] * m[7]) / det, (m[2] * m[7] - m[1] * m[8]) / det, (m[1] * m[5] - m[2] * m[4]) / det,
                   (m[5] * m[6] - m[3] * m[8]) / det, (m[0] * m[8] - m[2] * m[6]) / det, (m[2] * m[3] - m[0] * m[5]) / det,
                   (m[3] * m[7] - m[4] * m[6]) / det, (m[1] * m[6] - m[0] * m[7]) / det, (m[0] * m[4] - m[1] * m[3]) / det};
    memcpy(o, t, sizeof t);
    return 1;
}
/* Bradford: XYZ sob o branco 'from' → XYZ sob 'to' */
static void bradford(const double *from, const double *to, double *out) {
    static const double B[9] = {0.8951, 0.2664, -0.1614, -0.7502, 1.7135, 0.0367, 0.0389, -0.0685, 1.0296};
    double Bi[9], s[3], d[3];
    m3_inv(B, Bi);
    m3_vec(B, from, s);
    m3_vec(B, to, d);
    double D[9] = {d[0] / s[0], 0, 0, 0, d[1] / s[1], 0, 0, 0, d[2] / s[2]};
    double t[9];
    m3_mul(D, B, t);
    m3_mul(Bi, t, out);
}
/* primárias e branco (xy) → matriz RGB linear → XYZ sob D50 */
static void prim_matrix(const double *xy /* rx ry gx gy bx by */, const double *white, double *m) {
    double X[3], Y[3], Z[3];
    for (int i = 0; i < 3; i++) {
        double x = xy[2 * i], y = xy[2 * i + 1];
        X[i] = x / y; Y[i] = 1; Z[i] = (1 - x - y) / y;
    }
    double P[9] = {X[0], X[1], X[2], Y[0], Y[1], Y[2], Z[0], Z[1], Z[2]}, Pi[9], S[3];
    m3_inv(P, Pi);
    m3_vec(Pi, white, S);
    double M[9] = {X[0] * S[0], X[1] * S[1], X[2] * S[2], Y[0] * S[0], Y[1] * S[1], Y[2] * S[2], Z[0] * S[0], Z[1] * S[1], Z[2] * S[2]};
    double A[9];
    bradford(white, D50, A);
    m3_mul(A, M, m);
}

/* ---------------------------------------------------------------- curvas */
double cs_curve_eval(const cs_curve *c, double x) {
    if (x < 0) x = 0;
    if (x > 1) x = 1;
    switch (c->type) {
    case 1: return pow(x, c->g);
    case 2: {
        if (c->n < 2) return x;
        double f = x * (c->n - 1);
        int i = (int)f;
        if (i >= c->n - 1) return c->tab[c->n - 1];
        return c->tab[i] + (c->tab[i + 1] - c->tab[i]) * (f - i);
    }
    case 3: {
        const double *p = c->p;
        double g = p[0], a = p[1], b = p[2], cc = p[3], d = p[4], e = p[5], f = p[6], v;
        switch (c->ptype) {
        case 0: v = pow(x, g); break;
        case 1: v = x >= -b / a ? pow(a * x + b, g) : 0; break;
        case 2: v = x >= -b / a ? pow(a * x + b, g) + cc : cc; break;
        case 3: v = x >= d ? pow(a * x + b, g) : cc * x; break;
        default: v = x >= d ? pow(a * x + b, g) + e : cc * x + f; break;
        }
        return v;
    }
    case 4: {                                       /* SMPTE ST 2084 (PQ): 1 = 10000 nits; 100 nits = SDR branco */
        double m1 = 0.1593017578125, m2 = 78.84375, c1 = 0.8359375, c2 = 18.8515625, c3 = 18.6875;
        double e = pow(x, 1 / m2), num = e - c1 > 0 ? e - c1 : 0, l = pow(num / (c2 - c3 * e), 1 / m1);
        return l * 100.0;
    }
    case 5: {                                       /* ARIB STD-B67 (HLG), OETF inversa */
        double a = 0.17883277, b = 0.28466892, cc = 0.55991073;
        return x <= 0.5 ? x * x / 3 : (exp((x - cc) / a) + b) / 12;
    }
    default: return x;
    }
}
/* inversa por bisseção (as curvas de ICC são monótonas) */
double cs_curve_inv(const cs_curve *c, double y) {
    if (c->type == 0) return y < 0 ? 0 : y > 1 ? 1 : y;
    if (c->type == 1 && c->g > 0) return y <= 0 ? 0 : pow(y, 1 / c->g);
    double lo = 0, hi = 1, ylo = cs_curve_eval(c, 0), yhi = cs_curve_eval(c, 1);
    int up = yhi >= ylo;
    if (up ? y <= ylo : y >= ylo) return 0;
    if (up ? y >= yhi : y <= yhi) return 1;
    for (int i = 0; i < 40; i++) {
        double m = (lo + hi) / 2, v = cs_curve_eval(c, m);
        if ((v < y) == up) lo = m; else hi = m;
    }
    return (lo + hi) / 2;
}
static void curve_srgb(cs_curve *c) {
    c->type = 3; c->ptype = 3;
    c->p[0] = 2.4; c->p[1] = 1 / 1.055; c->p[2] = 0.055 / 1.055; c->p[3] = 1 / 12.92; c->p[4] = 0.04045;
}
static void curve_709(cs_curve *c) {
    c->type = 3; c->ptype = 3;
    c->p[0] = 1 / 0.45; c->p[1] = 1 / 1.099; c->p[2] = 0.099 / 1.099; c->p[3] = 1 / 4.5; c->p[4] = 0.081;
}
static void curve_gamma(cs_curve *c, double g) { c->type = g == 1 ? 0 : 1; c->g = g; }

/* curv / para; devolve bytes consumidos (0 = erro) */
static size_t read_curve(const uint8_t *d, size_t n, cs_curve *c) {
    memset(c, 0, sizeof *c);
    if (n < 12) return 0;
    uint32_t t = be32c(d);
    if (t == SIG('c', 'u', 'r', 'v')) {
        uint32_t cnt = be32c(d + 8);
        if (12 + (size_t)cnt * 2 > n) return 0;
        if (cnt == 0) c->type = 0;
        else if (cnt == 1) { c->type = 1; c->g = be16c(d + 12) / 256.0; }
        else {
            c->type = 2; c->n = (int)cnt;
            c->tab = malloc(cnt * sizeof(float));
            if (!c->tab) return 0;
            for (uint32_t i = 0; i < cnt; i++) c->tab[i] = (float)(be16c(d + 12 + i * 2) / 65535.0);
        }
        return (12 + (size_t)cnt * 2 + 3) & ~(size_t)3;
    }
    if (t == SIG('p', 'a', 'r', 'a')) {
        static const int np[5] = {1, 3, 4, 5, 7};
        int ft = (int)be16c(d + 8);
        if (ft > 4 || 12 + (size_t)np[ft] * 4 > n) return 0;
        c->type = 3; c->ptype = ft;
        for (int i = 0; i < np[ft]; i++) c->p[i] = s15(d + 12 + i * 4);
        return 12 + (size_t)np[ft] * 4;
    }
    return 0;
}

/* ---------------------------------------------------------------- LUTs */
static double clut_interp(const cs_lut *L, const double *in, int ch) {
    /* multilinear em 'in' dimensões (até 8) */
    int n = L->in;
    double acc = 0;
    int corners = 1 << n;
    size_t stride[16];
    stride[n - 1] = (size_t)L->out;
    for (int k = n - 2; k >= 0; k--) stride[k] = stride[k + 1] * (size_t)L->grid[k + 1];
    int base[16];
    double fr[16];
    for (int k = 0; k < n; k++) {
        double v = in[k] < 0 ? 0 : in[k] > 1 ? 1 : in[k];
        double f = v * (L->grid[k] - 1);
        int i = (int)f;
        if (i >= L->grid[k] - 1) i = L->grid[k] - 2;
        if (i < 0) i = 0;
        base[k] = i;
        fr[k] = L->grid[k] > 1 ? f - i : 0;
    }
    for (int cmask = 0; cmask < corners; cmask++) {
        double w = 1;
        size_t off = (size_t)ch;
        for (int k = 0; k < n; k++) {
            int bit = cmask >> k & 1;
            w *= bit ? fr[k] : 1 - fr[k];
            off += (size_t)(base[k] + (L->grid[k] > 1 ? bit : 0)) * stride[k];
        }
        if (w > 0) acc += w * L->clut[off];
    }
    return acc;
}
/* tetraédrica nos 3 últimos eixos a partir de 'base' (deslocamento já com os eixos anteriores) */
static double tetra3(const cs_lut *L, const double *in3, int g0, int g1, int g2, size_t s0, size_t s1, size_t s2, size_t base, int ch) {
    double f[3];
    int i[3], g[3] = {g0, g1, g2};
    for (int k = 0; k < 3; k++) {
        double v = in3[k] < 0 ? 0 : in3[k] > 1 ? 1 : in3[k], x = v * (g[k] - 1);
        i[k] = (int)x;
        if (i[k] >= g[k] - 1) i[k] = g[k] - 2;
        f[k] = x - i[k];
    }
    size_t o = base + (size_t)i[0] * s0 + (size_t)i[1] * s1 + (size_t)i[2] * s2 + (size_t)ch;
#define D(a, b, c) ((double)L->clut[o + (a) * s0 + (b) * s1 + (c) * s2])
    double rx = f[0], ry = f[1], rz = f[2], c1, c2, c3, c0 = D(0, 0, 0);
    if (rx >= ry && ry >= rz) { c1 = D(1, 0, 0) - c0; c2 = D(1, 1, 0) - D(1, 0, 0); c3 = D(1, 1, 1) - D(1, 1, 0); }
    else if (rx >= rz && rz >= ry) { c1 = D(1, 0, 0) - c0; c2 = D(1, 1, 1) - D(1, 0, 1); c3 = D(1, 0, 1) - D(1, 0, 0); }
    else if (rz >= rx && rx >= ry) { c1 = D(1, 0, 1) - D(0, 0, 1); c2 = D(1, 1, 1) - D(1, 0, 1); c3 = D(0, 0, 1) - c0; }
    else if (ry >= rx && rx >= rz) { c1 = D(1, 1, 0) - D(0, 1, 0); c2 = D(0, 1, 0) - c0; c3 = D(1, 1, 1) - D(1, 1, 0); }
    else if (ry >= rz && rz >= rx) { c1 = D(1, 1, 1) - D(0, 1, 1); c2 = D(0, 1, 0) - c0; c3 = D(0, 1, 1) - D(0, 1, 0); }
    else { c1 = D(1, 1, 1) - D(0, 1, 1); c2 = D(0, 1, 1) - D(0, 0, 1); c3 = D(0, 0, 1) - c0; }
#undef D
    return c0 + c1 * rx + c2 * ry + c3 * rz;
}
/* CLUT: 3 entradas tetraédrica; 4 entradas linear no primeiro eixo e tetraédrica nos outros; resto multilinear */
static double clut_eval(const cs_lut *L, const double *in, int ch) {
    int n = L->in;
    if ((n == 3 || n == 4) && L->grid[0] > 1 && L->grid[1] > 1 && L->grid[2] > 1 && (n == 3 || L->grid[3] > 1)) {
        size_t so = (size_t)L->out;
        if (n == 3) return tetra3(L, in, L->grid[0], L->grid[1], L->grid[2], so * L->grid[1] * L->grid[2], so * L->grid[2], so, 0, ch);
        size_t s3 = so, s2 = s3 * L->grid[3], s1 = s2 * L->grid[2], s0 = s1 * L->grid[1];
        double v = in[0] < 0 ? 0 : in[0] > 1 ? 1 : in[0], x = v * (L->grid[0] - 1);
        int k = (int)x;
        if (k >= L->grid[0] - 1) k = L->grid[0] - 2;
        double fk = x - k;
        double a = tetra3(L, in + 1, L->grid[1], L->grid[2], L->grid[3], s1, s2, s3, (size_t)k * s0, ch);
        double b = tetra3(L, in + 1, L->grid[1], L->grid[2], L->grid[3], s1, s2, s3, (size_t)(k + 1) * s0, ch);
        return a + (b - a) * fk;
    }
    return clut_interp(L, in, ch);
}
static void lut_eval(const cs_lut *L, const double *in, double *out) {
    double t[16], u[16];
    if (L->kind == 1) {
        for (int i = 0; i < L->in; i++) t[i] = cs_curve_eval(&L->incv[i], in[i]);
        for (int o = 0; o < L->out; o++) u[o] = clut_eval(L, t, o);
        for (int o = 0; o < L->out; o++) out[o] = cs_curve_eval(&L->outcv[o], u[o]);
        return;
    }
    /* lutAtoB: A → CLUT → M → matriz → B */
    for (int i = 0; i < L->in; i++) t[i] = cs_curve_eval(&L->incv[i], in[i]);
    if (L->clut) for (int o = 0; o < L->out; o++) u[o] = clut_eval(L, t, o);
    else for (int o = 0; o < L->out; o++) u[o] = t[o < L->in ? o : L->in - 1];
    if (L->has_m) {
        for (int o = 0; o < 3; o++) u[o] = cs_curve_eval(&L->mcv[o], u[o]);
        double v[3] = {L->mat[0] * u[0] + L->mat[1] * u[1] + L->mat[2] * u[2] + L->mat[9],
                       L->mat[3] * u[0] + L->mat[4] * u[1] + L->mat[5] * u[2] + L->mat[10],
                       L->mat[6] * u[0] + L->mat[7] * u[1] + L->mat[8] * u[2] + L->mat[11]};
        u[0] = v[0]; u[1] = v[1]; u[2] = v[2];
    }
    for (int o = 0; o < L->out; o++) out[o] = cs_curve_eval(&L->outcv[o], u[o]);
}
static int read_lut(const uint8_t *d, size_t n, cs_lut *L) {
    memset(L, 0, sizeof *L);
    if (n < 32) return -1;
    uint32_t t = be32c(d);
    if (t == SIG('m', 'f', 't', '1') || t == SIG('m', 'f', 't', '2')) {
        int is16 = t == SIG('m', 'f', 't', '2');
        L->kind = 1; L->in = d[8]; L->out = d[9];
        int g = d[10];
        if (L->in < 1 || L->in > 8 || L->out < 1 || L->out > 15 || g < 2) return -1;
        for (int k = 0; k < L->in; k++) L->grid[k] = g;
        for (int i = 0; i < 9; i++) L->mat[i] = s15(d + 12 + i * 4);
        size_t pos = 48;
        int nin = 256, nout = 256;
        if (is16) { if (n < 52) return -1; nin = (int)be16c(d + 48); nout = (int)be16c(d + 50); pos = 52; }
        int bytes = is16 ? 2 : 1;
        size_t gsz = 1;
        for (int k = 0; k < L->in; k++) gsz *= (size_t)g;
        size_t need = pos + (size_t)L->in * nin * bytes + gsz * L->out * bytes + (size_t)L->out * nout * bytes;
        if (need > n || nin < 2 || nout < 2) return -1;
        for (int k = 0; k < L->in; k++) {
            cs_curve *c = &L->incv[k];
            c->type = 2; c->n = nin; c->tab = malloc((size_t)nin * sizeof(float));
            if (!c->tab) return -1;
            for (int i = 0; i < nin; i++) c->tab[i] = (float)(is16 ? be16c(d + pos + i * 2) / 65535.0 : d[pos + i] / 255.0);
            pos += (size_t)nin * bytes;
        }
        L->clut = malloc(gsz * L->out * sizeof(float));
        if (!L->clut) return -1;
        for (size_t i = 0; i < gsz * L->out; i++) L->clut[i] = (float)(is16 ? be16c(d + pos + i * 2) / 65535.0 : d[pos + i] / 255.0);
        pos += gsz * L->out * bytes;
        for (int k = 0; k < L->out; k++) {
            cs_curve *c = &L->outcv[k];
            c->type = 2; c->n = nout; c->tab = malloc((size_t)nout * sizeof(float));
            if (!c->tab) return -1;
            for (int i = 0; i < nout; i++) c->tab[i] = (float)(is16 ? be16c(d + pos + i * 2) / 65535.0 : d[pos + i] / 255.0);
            pos += (size_t)nout * bytes;
        }
        return 0;
    }
    if (t == SIG('m', 'A', 'B', ' ') || t == SIG('m', 'B', 'A', ' ')) {
        int ab = t == SIG('m', 'A', 'B', ' ');
        L->kind = 2; L->in = d[8]; L->out = d[9];
        if (L->in < 1 || L->in > 15 || L->out < 1 || L->out > 15) return -1;
        uint32_t oB = be32c(d + 12), oM = be32c(d + 16), oC = be32c(d + 20), oA = be32c(d + 24), oMat = be32c(d + 20 - 4);
        oMat = be32c(d + 16 - 0);   /* reescrito abaixo: a ordem dos campos é B, matriz, M, CLUT, A */
        oB = be32c(d + 12); oMat = be32c(d + 16); oM = be32c(d + 20); oC = be32c(d + 24); oA = be32c(d + 28);
        /* para mAB a cadeia é A → CLUT → M → matriz → B: in usa A (in canais), out usa B (out canais).
         * para mBA é B → matriz → M → CLUT → A: avaliado na mesma estrutura com os papéis trocados. */
        cs_curve *first = L->incv, *last = L->outcv;
        uint32_t ofirst = ab ? oA : oB, olast = ab ? oB : oA;
        int nfirst = L->in, nlast = L->out;
        if (ofirst) {
            size_t p = ofirst;
            for (int k = 0; k < nfirst; k++) { size_t u = p < n ? read_curve(d + p, n - p, &first[k]) : 0; if (!u) return -1; p += u; }
        }
        if (olast) {
            size_t p = olast;
            for (int k = 0; k < nlast; k++) { size_t u = p < n ? read_curve(d + p, n - p, &last[k]) : 0; if (!u) return -1; p += u; }
        }
        if (oM) {
            size_t p = oM;
            for (int k = 0; k < 3; k++) { size_t u = p < n ? read_curve(d + p, n - p, &L->mcv[k]) : 0; if (!u) return -1; p += u; }
            L->has_m = 1;
        }
        if (oMat && oMat + 48 <= n) { for (int i = 0; i < 12; i++) L->mat[i] = s15(d + oMat + i * 4); L->has_mat = 1; }
        if (oM && !L->has_mat) { L->mat[0] = L->mat[4] = L->mat[8] = 1; }
        if (oC) {
            if (oC + 20 > n) return -1;
            int ci = ab ? L->in : L->out, co = ab ? L->out : L->in;
            /* a CLUT liga 'ci' canais a 'co' canais; na avaliação abaixo o lado da CLUT é sempre in → out */
            (void)co;
            size_t gsz = 1;
            for (int k = 0; k < ci; k++) { L->grid[k] = d[oC + k]; if (L->grid[k] < 2) return -1; gsz *= (size_t)L->grid[k]; }
            int prec = d[oC + 16];
            size_t need = oC + 20 + gsz * L->out * (prec == 1 ? 1 : 2);
            if (need > n) return -1;
            L->clut = malloc(gsz * L->out * sizeof(float));
            if (!L->clut) return -1;
            for (size_t i = 0; i < gsz * L->out; i++)
                L->clut[i] = (float)(prec == 1 ? d[oC + 20 + i] / 255.0 : be16c(d + oC + 20 + i * 2) / 65535.0);
        }
        return ab ? 0 : 0;
    }
    return -1;
}

/* ---------------------------------------------------------------- perfil */
static const uint8_t *find_tag(const uint8_t *d, size_t n, uint32_t sig, size_t *len) {
    if (n < 132) return 0;
    uint32_t cnt = be32c(d + 128);
    for (uint32_t i = 0; i < cnt && 132 + (size_t)i * 12 + 12 <= n; i++) {
        const uint8_t *t = d + 132 + i * 12;
        uint32_t off = be32c(t + 4), l = be32c(t + 8);
        if (be32c(t) == sig && off + (size_t)l <= n && l >= 8) { *len = l; return d + off; }
    }
    return 0;
}
static int read_xyz(const uint8_t *d, size_t n, uint32_t sig, double *v) {
    size_t l;
    const uint8_t *t = find_tag(d, n, sig, &l);
    if (!t || l < 20 || be32c(t) != SIG('X', 'Y', 'Z', ' ')) return 0;
    v[0] = s15(t + 8); v[1] = s15(t + 12); v[2] = s15(t + 16);
    return 1;
}
static void read_desc(const uint8_t *d, size_t n, char *out, size_t cap) {
    size_t l, m = 0;
    const uint8_t *t = find_tag(d, n, SIG('d', 'e', 's', 'c'), &l);
    out[0] = 0;
    if (!t || l < 12) return;
    if (be32c(t) == SIG('d', 'e', 's', 'c')) {
        uint32_t L = be32c(t + 8);
        for (uint32_t j = 0; j < L && 12 + j < l && m + 1 < cap && t[12 + j]; j++) out[m++] = (char)t[12 + j];
    } else if (be32c(t) == SIG('m', 'l', 'u', 'c') && l >= 28) {
        uint32_t sl = be32c(t + 20), so = be32c(t + 24);
        for (uint32_t j = 0; j + 1 < sl && so + j + 1 < l && m + 1 < cap; j += 2) {
            uint32_t c = be16c(t + so + j);
            out[m++] = c < 128 ? (char)c : '?';
        }
    }
    out[m] = 0;
}
static int nchan_of(uint32_t sp) {
    switch (sp) {
    case SIG('G', 'R', 'A', 'Y'): return 1;
    case SIG('C', 'M', 'Y', 'K'): return 4;
    case SIG('2', 'C', 'L', 'R'): return 2;
    case SIG('5', 'C', 'L', 'R'): return 5;
    case SIG('6', 'C', 'L', 'R'): return 6;
    case SIG('7', 'C', 'L', 'R'): return 7;
    case SIG('8', 'C', 'L', 'R'): return 8;
    default: return 3;
    }
}
int cs_parse(const uint8_t *d, size_t n, cs_prof *p) { return cs_parse_intent(d, n, 0, p); }
int cs_parse_intent(const uint8_t *d, size_t n, int intent, cs_prof *p) {
    memset(p, 0, sizeof *p);
    if (!d || n < 132 || be32c(d) > n || be32c(d + 36) != SIG('a', 'c', 's', 'p')) return -1;
    p->version = be32c(d + 8);
    p->cls = be32c(d + 12);
    p->space = be32c(d + 16);
    p->pcs = be32c(d + 20);
    p->nchan = nchan_of(p->space);
    if (!read_xyz(d, n, SIG('w', 't', 'p', 't'), p->wp)) { p->wp[0] = D50[0]; p->wp[1] = D50[1]; p->wp[2] = D50[2]; }
    read_desc(d, n, p->desc, sizeof p->desc);
    size_t l;
    const uint8_t *t;
    static const char dig[4] = {'0', '1', '2', '1'};
    int ti = intent >= 0 && intent <= 3 ? intent : 0;
    if ((t = find_tag(d, n, SIG('A', '2', 'B', dig[ti]), &l)) && read_lut(t, l, &p->a2b) == 0) p->has_a2b = 1;
    else if ((t = find_tag(d, n, SIG('A', '2', 'B', '0'), &l)) && read_lut(t, l, &p->a2b) == 0) p->has_a2b = 1;
    if ((t = find_tag(d, n, SIG('B', '2', 'A', dig[ti]), &l)) && read_lut(t, l, &p->b2a) == 0) p->has_b2a = 1;
    else if ((t = find_tag(d, n, SIG('B', '2', 'A', '0'), &l)) && read_lut(t, l, &p->b2a) == 0) p->has_b2a = 1;
    if (p->space == SIG('R', 'G', 'B', ' ')) {
        double r[3], g[3], b[3];
        if (read_xyz(d, n, SIG('r', 'X', 'Y', 'Z'), r) && read_xyz(d, n, SIG('g', 'X', 'Y', 'Z'), g) && read_xyz(d, n, SIG('b', 'X', 'Y', 'Z'), b)) {
            static const uint32_t trc[3] = {SIG('r', 'T', 'R', 'C'), SIG('g', 'T', 'R', 'C'), SIG('b', 'T', 'R', 'C')};
            int ok = 1;
            for (int k = 0; k < 3; k++) {
                const uint8_t *c = find_tag(d, n, trc[k], &l);
                if (!c || !read_curve(c, l, &p->trc[k])) ok = 0;
            }
            if (ok) {
                double m[9] = {r[0], g[0], b[0], r[1], g[1], b[1], r[2], g[2], b[2]};
                memcpy(p->mtx, m, sizeof m);
                p->has_mtx = 1;
            }
        }
    } else if (p->space == SIG('G', 'R', 'A', 'Y')) {
        const uint8_t *c = find_tag(d, n, SIG('k', 'T', 'R', 'C'), &l);
        if (c && read_curve(c, l, &p->trc[0])) p->has_mtx = 1;
    } else if (p->space == SIG('L', 'a', 'b', ' ') && !p->has_a2b) {
        p->lab = 1; p->wp[0] = D50[0]; p->wp[1] = D50[1]; p->wp[2] = D50[2];
        p->range[0] = -128; p->range[1] = 127; p->range[2] = -128; p->range[3] = 127;
    }
    if (p->space == SIG('C', 'M', 'Y', 'K') && !p->has_a2b) p->naive_cmyk = 1;
    if (!p->has_mtx && !p->has_a2b && !p->lab && !p->naive_cmyk && p->space != SIG('X', 'Y', 'Z', ' ')) { cs_free(p); return -1; }
    return 0;
}
static void free_curve(cs_curve *c) { free(c->tab); c->tab = 0; }
static void free_lut(cs_lut *L) {
    for (int i = 0; i < 16; i++) { free_curve(&L->incv[i]); free_curve(&L->outcv[i]); }
    for (int i = 0; i < 3; i++) free_curve(&L->mcv[i]);
    free(L->clut);
    L->clut = 0;
}
void cs_free(cs_prof *p) {
    if (!p) return;
    for (int i = 0; i < 3; i++) free_curve(&p->trc[i]);
    free_lut(&p->a2b);
    free_lut(&p->b2a);
}

/* ---------------------------------------------------------------- perfis embutidos */
static void rgb_prof(cs_prof *p, const double *xy, const double *white, int curve, double gamma, const char *desc) {
    memset(p, 0, sizeof *p);
    p->space = SIG('R', 'G', 'B', ' '); p->pcs = SIG('X', 'Y', 'Z', ' '); p->cls = SIG('m', 'n', 't', 'r');
    p->nchan = 3; p->has_mtx = 1; p->version = 0x02100000;
    prim_matrix(xy, white, p->mtx);
    for (int k = 0; k < 3; k++) {
        if (curve == 1) curve_srgb(&p->trc[k]);
        else if (curve == 2) curve_709(&p->trc[k]);
        else if (curve == 4) p->trc[k].type = 4;
        else if (curve == 5) p->trc[k].type = 5;
        else curve_gamma(&p->trc[k], gamma);
    }
    p->wp[0] = D50[0]; p->wp[1] = D50[1]; p->wp[2] = D50[2];
    size_t k = strlen(desc);
    if (k >= sizeof p->desc) k = sizeof p->desc - 1;
    memcpy(p->desc, desc, k);
    p->desc[k] = 0;
}
static void gray_prof(cs_prof *p, double gamma, int srgb, const char *desc) {
    memset(p, 0, sizeof *p);
    p->space = SIG('G', 'R', 'A', 'Y'); p->pcs = SIG('X', 'Y', 'Z', ' '); p->cls = SIG('m', 'n', 't', 'r');
    p->nchan = 1; p->has_mtx = 1; p->version = 0x02100000;
    if (srgb) curve_srgb(&p->trc[0]); else curve_gamma(&p->trc[0], gamma);
    p->wp[0] = D50[0]; p->wp[1] = D50[1]; p->wp[2] = D50[2];
    size_t k = strlen(desc);
    if (k >= sizeof p->desc) k = sizeof p->desc - 1;
    memcpy(p->desc, desc, k);
    p->desc[k] = 0;
}
int cs_builtin(const char *name, cs_prof *p) {
    static const double srgb[6] = {0.64, 0.33, 0.30, 0.60, 0.15, 0.06};
    static const double p3[6] = {0.680, 0.320, 0.265, 0.690, 0.150, 0.060};
    static const double adobe[6] = {0.64, 0.33, 0.21, 0.71, 0.15, 0.06};
    static const double bt2020[6] = {0.708, 0.292, 0.170, 0.797, 0.131, 0.046};
    static const double aces[6] = {0.713, 0.293, 0.165, 0.830, 0.128, 0.044};
    static const double romm[6] = {0.7347, 0.2653, 0.1596, 0.8404, 0.0366, 0.0001};
    static const double dci_white[3] = {0.89458689, 1.0, 0.95441595};     /* xy (0.314, 0.351) */
    static const double aces_white[3] = {0.95264607, 1.0, 1.00882518};    /* xy (0.32168, 0.33767) */
    if (!name) return -1;
    if (strstr(name, "Gray") || strstr(name, "gray") || strstr(name, "GRAY")) {
        if (strstr(name, "Linear")) gray_prof(p, 1.0, 0, "Linear Gray");
        else if (strstr(name, "Extended") && !strstr(name, "Gamma")) gray_prof(p, 0, 1, "Extended Gray");
        else gray_prof(p, 2.2, 0, "Generic Gray Gamma 2.2 Profile");
        return 0;
    }
    if (strstr(name, "CMYK")) {
        memset(p, 0, sizeof *p);
        p->space = SIG('C', 'M', 'Y', 'K'); p->pcs = SIG('L', 'a', 'b', ' '); p->cls = SIG('p', 'r', 't', 'r');
        p->nchan = 4; p->naive_cmyk = 1;
        memcpy(p->desc, "Generic CMYK", 13);
        return 0;
    }
    if (strstr(name, "Lab")) { memset(p, 0, sizeof *p); cs_lab(p, D50, 0); memcpy(p->desc, "Generic Lab Profile", 20); return 0; }
    if (strstr(name, "XYZ")) {
        memset(p, 0, sizeof *p);
        p->space = SIG('X', 'Y', 'Z', ' '); p->pcs = SIG('X', 'Y', 'Z', ' '); p->cls = SIG('s', 'p', 'a', 'c'); p->nchan = 3;
        memcpy(p->desc, "Generic XYZ Profile", 20);
        return 0;
    }
    int lin = strstr(name, "Linear") != 0;
    if (strstr(name, "DCIP3") || strstr(name, "DCI-P3") || strstr(name, "DCI P3")) { rgb_prof(p, p3, dci_white, 0, 2.6, "DCI(P3) RGB"); return 0; }
    if (strstr(name, "P3")) {
        if (strstr(name, "PQ")) rgb_prof(p, p3, D65, 4, 0, "Display P3 PQ");
        else if (strstr(name, "HLG")) rgb_prof(p, p3, D65, 5, 0, "Display P3 HLG");
        else rgb_prof(p, p3, D65, lin ? 0 : 1, 1.0, lin ? "Linear Display P3" : "Display P3");
        return 0;
    }
    if (strstr(name, "AdobeRGB") || strstr(name, "Adobe RGB")) { rgb_prof(p, adobe, D65, 0, 563.0 / 256.0, "Adobe RGB (1998)"); return 0; }
    if (strstr(name, "2020") || strstr(name, "2100")) {
        if (strstr(name, "PQ")) rgb_prof(p, bt2020, D65, 4, 0, "Rec. ITU-R BT.2100 PQ");
        else if (strstr(name, "HLG")) rgb_prof(p, bt2020, D65, 5, 0, "Rec. ITU-R BT.2100 HLG");
        else rgb_prof(p, bt2020, D65, lin ? 0 : 2, 1.0, lin ? "Linear ITU-R 2020" : "ITU-R BT.2020");
        return 0;
    }
    if (strstr(name, "709")) { rgb_prof(p, srgb, D65, 2, 0, "ITU-R BT.709"); return 0; }
    if (strstr(name, "ACES")) { rgb_prof(p, aces, aces_white, 0, 1.0, "ACEScg"); return 0; }
    if (strstr(name, "ROMM")) { rgb_prof(p, romm, D50, 0, 1.8, "ROMM RGB"); return 0; }
    if (strstr(name, "GenericRGB") && lin) { rgb_prof(p, srgb, D65, 0, 1.0, "Generic RGB Linear"); return 0; }
    rgb_prof(p, srgb, D65, lin ? 0 : 1, 1.0, lin ? "Linear sRGB" : "sRGB IEC61966-2.1");
    return 0;
}
void cs_calibrated_rgb(cs_prof *p, const double *white, const double *gamma, const double *matrix) {
    memset(p, 0, sizeof *p);
    p->space = SIG('R', 'G', 'B', ' '); p->pcs = SIG('X', 'Y', 'Z', ' '); p->cls = SIG('m', 'n', 't', 'r'); p->nchan = 3; p->has_mtx = 1;
    double w[3] = {white ? white[0] : D65[0], white ? white[1] : 1, white ? white[2] : D65[2]};
    /* matrix (PDF CalRGB): colunas Xr Yr Zr, Xg Yg Zg, Xb Yb Zb sob o branco w; adapta para D50 */
    double M[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    if (matrix) { M[0] = matrix[0]; M[3] = matrix[1]; M[6] = matrix[2]; M[1] = matrix[3]; M[4] = matrix[4]; M[7] = matrix[5]; M[2] = matrix[6]; M[5] = matrix[7]; M[8] = matrix[8]; }
    double A[9];
    bradford(w, D50, A);
    m3_mul(A, M, p->mtx);
    for (int k = 0; k < 3; k++) curve_gamma(&p->trc[k], gamma ? gamma[k] : 1);
    p->wp[0] = D50[0]; p->wp[1] = D50[1]; p->wp[2] = D50[2];
    memcpy(p->desc, "Calibrated RGB", 15);
}
void cs_calibrated_gray(cs_prof *p, const double *white, double gamma) {
    (void)white;
    gray_prof(p, gamma > 0 ? gamma : 1, 0, "Calibrated Gray");
}
void cs_lab(cs_prof *p, const double *white, const double *range) {
    p->space = SIG('L', 'a', 'b', ' '); p->pcs = SIG('L', 'a', 'b', ' '); p->cls = SIG('s', 'p', 'a', 'c'); p->nchan = 3; p->lab = 1;
    for (int i = 0; i < 3; i++) p->wp[i] = white ? white[i] : D50[i];
    for (int i = 0; i < 4; i++) p->range[i] = range ? range[i] : (i & 1 ? 127 : -128);
}

/* ---------------------------------------------------------------- conversão */
static double labf(double t) { return t > 216.0 / 24389 ? pow(t, 1.0 / 3) : (24389.0 / 27 * t + 16) / 116; }
static double labfi(double t) { return t > 6.0 / 29 ? t * t * t : (116 * t - 16) * 27 / 24389; }
static void lab_to_xyz(const double *lab, const double *wp, double *xyz) {
    double fy = (lab[0] + 16) / 116, fx = fy + lab[1] / 500, fz = fy - lab[2] / 200;
    xyz[0] = wp[0] * labfi(fx); xyz[1] = wp[1] * labfi(fy); xyz[2] = wp[2] * labfi(fz);
}
static void xyz_to_lab(const double *xyz, const double *wp, double *lab) {
    double fx = labf(xyz[0] / wp[0]), fy = labf(xyz[1] / wp[1]), fz = labf(xyz[2] / wp[2]);
    lab[0] = 116 * fy - 16; lab[1] = 500 * (fx - fy); lab[2] = 200 * (fy - fz);
}
/* PCS Lab codificado 0..1 (ICC v4: L 0..100, a/b −128..127) ↔ Lab */
static void pcs_lab_decode(const double *e, double *lab) { lab[0] = e[0] * 100; lab[1] = e[1] * 255 - 128; lab[2] = e[2] * 255 - 128; }
static void pcs_lab_encode(const double *lab, double *e) { e[0] = lab[0] / 100; e[1] = (lab[1] + 128) / 255; e[2] = (lab[2] + 128) / 255; }
static void pcs_xyz_decode(const double *e, double *xyz) { for (int i = 0; i < 3; i++) xyz[i] = e[i] * (1 + 32767.0 / 32768); }
static void pcs_xyz_encode(const double *xyz, double *e) { for (int i = 0; i < 3; i++) e[i] = xyz[i] / (1 + 32767.0 / 32768); }

/* sRGB linear ↔ XYZ D50 (para o CMYK ingênuo) */
static void srgb_lin_to_xyz(const double *rgb, double *xyz) {
    static double M[9];
    static int ready;
    static const double xy[6] = {0.64, 0.33, 0.30, 0.60, 0.15, 0.06};
    if (!ready) { prim_matrix(xy, D65, M); ready = 1; }
    m3_vec(M, rgb, xyz);
}
static void xyz_to_srgb_lin(const double *xyz, double *rgb) {
    static double Mi[9];
    static int ready;
    static const double xy[6] = {0.64, 0.33, 0.30, 0.60, 0.15, 0.06};
    if (!ready) { double M[9]; prim_matrix(xy, D65, M); m3_inv(M, Mi); ready = 1; }
    m3_vec(Mi, xyz, rgb);
}
static double srgb_enc(double v) { return v <= 0.0031308 ? 12.92 * v : 1.055 * pow(v, 1 / 2.4) - 0.055; }
static double srgb_dec(double v) { return v <= 0.04045 ? v / 12.92 : pow((v + 0.055) / 1.055, 2.4); }

void cs_to_xyz(const cs_prof *p, const double *in, double *xyz) {
    if (p->lab) {
        double lab[3] = {in[0], in[1], in[2]}, x[3], A[9];
        lab_to_xyz(lab, p->wp, x);
        bradford(p->wp, D50, A);
        m3_vec(A, x, xyz);
        return;
    }
    if (p->space == SIG('X', 'Y', 'Z', ' ')) { xyz[0] = in[0]; xyz[1] = in[1]; xyz[2] = in[2]; return; }
    if (p->has_mtx && p->space == SIG('R', 'G', 'B', ' ')) {
        double l[3] = {cs_curve_eval(&p->trc[0], in[0]), cs_curve_eval(&p->trc[1], in[1]), cs_curve_eval(&p->trc[2], in[2])};
        m3_vec(p->mtx, l, xyz);
        return;
    }
    if (p->has_mtx && p->space == SIG('G', 'R', 'A', 'Y')) {
        double y = cs_curve_eval(&p->trc[0], in[0]);
        xyz[0] = D50[0] * y; xyz[1] = y; xyz[2] = D50[2] * y;
        return;
    }
    if (p->has_a2b) {
        double e[16];
        lut_eval(&p->a2b, in, e);
        if (p->pcs == SIG('L', 'a', 'b', ' ')) {
            double lab[3];
            if (p->a2b.kind == 1 && p->version < 0x04000000) { e[0] *= 65535.0 / 65280.0; e[1] *= 65535.0 / 65280.0; e[2] *= 65535.0 / 65280.0; }
            pcs_lab_decode(e, lab);
            lab_to_xyz(lab, D50, xyz);
        } else pcs_xyz_decode(e, xyz);
        return;
    }
    if (p->naive_cmyk) {
        double k = in[3], rgb[3] = {srgb_dec((1 - in[0]) * (1 - k)), srgb_dec((1 - in[1]) * (1 - k)), srgb_dec((1 - in[2]) * (1 - k))};
        srgb_lin_to_xyz(rgb, xyz);
        return;
    }
    xyz[0] = xyz[1] = xyz[2] = 0;
}
void cs_from_xyz(const cs_prof *p, const double *xyz, double *out) {
    if (p->lab) {
        double A[9], x[3];
        bradford(D50, p->wp, A);
        m3_vec(A, xyz, x);
        xyz_to_lab(x, p->wp, out);
        return;
    }
    if (p->space == SIG('X', 'Y', 'Z', ' ')) { out[0] = xyz[0]; out[1] = xyz[1]; out[2] = xyz[2]; return; }
    if (p->has_mtx && p->space == SIG('R', 'G', 'B', ' ')) {
        double Mi[9], l[3];
        if (!m3_inv(p->mtx, Mi)) { out[0] = out[1] = out[2] = 0; return; }
        m3_vec(Mi, xyz, l);
        for (int k = 0; k < 3; k++) out[k] = cs_curve_inv(&p->trc[k], l[k] < 0 ? 0 : l[k]);
        return;
    }
    if (p->has_mtx && p->space == SIG('G', 'R', 'A', 'Y')) { out[0] = cs_curve_inv(&p->trc[0], xyz[1]); return; }
    if (p->has_b2a) {
        double e[3];
        if (p->pcs == SIG('L', 'a', 'b', ' ')) {
            double lab[3];
            xyz_to_lab(xyz, D50, lab);
            pcs_lab_encode(lab, e);
            if (p->b2a.kind == 1 && p->version < 0x04000000) { e[0] *= 65280.0 / 65535.0; e[1] *= 65280.0 / 65535.0; e[2] *= 65280.0 / 65535.0; }
        } else pcs_xyz_encode(xyz, e);
        lut_eval(&p->b2a, e, out);
        return;
    }
    if (p->naive_cmyk || p->space == SIG('C', 'M', 'Y', 'K')) {
        double rgb[3];
        xyz_to_srgb_lin(xyz, rgb);
        double r = srgb_enc(rgb[0] < 0 ? 0 : rgb[0]), g = srgb_enc(rgb[1] < 0 ? 0 : rgb[1]), b = srgb_enc(rgb[2] < 0 ? 0 : rgb[2]);
        double c = 1 - r, m = 1 - g, y = 1 - b, k = c < m ? (c < y ? c : y) : (m < y ? m : y);
        if (k > 0.9999) { out[0] = out[1] = out[2] = 0; out[3] = 1; return; }
        out[0] = (c - k) / (1 - k); out[1] = (m - k) / (1 - k); out[2] = (y - k) / (1 - k); out[3] = k;
        return;
    }
    out[0] = out[1] = out[2] = 0;
}
void cs_to_srgb(const cs_prof *p, const double *in, double *rgb) {
    double xyz[3], l[3];
    cs_to_xyz(p, in, xyz);
    xyz_to_srgb_lin(xyz, l);
    for (int k = 0; k < 3; k++) {
        double v = l[k] < 0 ? 0 : l[k] > 1 ? 1 : l[k];
        rgb[k] = srgb_enc(v);
    }
}
int cs_is_srgb(const cs_prof *p) {
    cs_prof s;
    if (!p->has_mtx || p->space != SIG('R', 'G', 'B', ' ')) return 0;
    cs_builtin("sRGB", &s);
    for (int i = 0; i < 9; i++) if (p->mtx[i] - s.mtx[i] > 0.002 || s.mtx[i] - p->mtx[i] > 0.002) return 0;
    for (int k = 0; k < 3; k++)
        for (int i = 1; i < 16; i++) {
            double x = i / 16.0, a = cs_curve_eval(&p->trc[k], x), b = cs_curve_eval(&s.trc[k], x);
            if (a - b > 0.003 || b - a > 0.003) return 0;
        }
    return 1;
}

/* ---------------------------------------------------------------- gravação ICC v2 (matriz/TRC e cinza) */
typedef struct { uint8_t *b; size_t n, cap; } wb_t;
static void wb_put(wb_t *w, const void *s, size_t n) {
    if (w->n + n > w->cap) {
        size_t nc = w->cap ? w->cap * 2 : 1024;
        while (nc < w->n + n) nc *= 2;
        uint8_t *nb = realloc(w->b, nc);
        if (!nb) return;
        w->b = nb;
        w->cap = nc;
    }
    memcpy(w->b + w->n, s, n);
    w->n += n;
}
static void wb_be32(wb_t *w, uint32_t v) { uint8_t b[4] = {(uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v}; wb_put(w, b, 4); }
static void wb_be16(wb_t *w, uint32_t v) { uint8_t b[2] = {(uint8_t)(v >> 8), (uint8_t)v}; wb_put(w, b, 2); }
static void wb_s15(wb_t *w, double v) { wb_be32(w, (uint32_t)(int32_t)(v * 65536.0 + (v < 0 ? -0.5 : 0.5))); }
static void wb_pad(wb_t *w) { while (w->n & 3) { uint8_t z = 0; wb_put(w, &z, 1); } }
static void tag_xyz(wb_t *w, const double *v) { wb_be32(w, SIG('X', 'Y', 'Z', ' ')); wb_be32(w, 0); for (int i = 0; i < 3; i++) wb_s15(w, v[i]); }
static void tag_curve(wb_t *w, const cs_curve *c) {
    if (c->type == 3) {
        static const int np[5] = {1, 3, 4, 5, 7};
        wb_be32(w, SIG('p', 'a', 'r', 'a')); wb_be32(w, 0); wb_be16(w, (uint32_t)c->ptype); wb_be16(w, 0);
        for (int i = 0; i < np[c->ptype]; i++) wb_s15(w, c->p[i]);
        return;
    }
    wb_be32(w, SIG('c', 'u', 'r', 'v')); wb_be32(w, 0);
    if (c->type == 0) { wb_be32(w, 0); return; }
    if (c->type == 1) { wb_be32(w, 1); wb_be16(w, (uint32_t)(c->g * 256 + 0.5)); wb_pad(w); return; }
    /* tabela (ou PQ/HLG): 1024 amostras */
    int n = c->type == 2 ? c->n : 1024;
    wb_be32(w, (uint32_t)n);
    for (int i = 0; i < n; i++) {
        double v = c->type == 2 ? c->tab[i] : cs_curve_eval(c, i / (double)(n - 1));
        if (v > 1) v = 1;
        wb_be16(w, (uint32_t)(v * 65535 + 0.5));
    }
    wb_pad(w);
}
static void tag_desc(wb_t *w, const char *s) {
    uint32_t L = (uint32_t)strlen(s) + 1;
    wb_be32(w, SIG('d', 'e', 's', 'c')); wb_be32(w, 0); wb_be32(w, L); wb_put(w, s, L);
    wb_be32(w, 0); wb_be32(w, 0);          /* Unicode: nada */
    wb_be16(w, 0); uint8_t z[67] = {0}; wb_put(w, z, 1); wb_put(w, z, 67);   /* ScriptCode */
    wb_pad(w);
}
static void tag_text(wb_t *w, const char *s) { wb_be32(w, SIG('t', 'e', 'x', 't')); wb_be32(w, 0); wb_put(w, s, strlen(s) + 1); wb_pad(w); }
int cs_write_icc(const cs_prof *p, uint8_t **out, size_t *on) {
    if (!p || !p->has_mtx) return -1;
    int gray = p->space == SIG('G', 'R', 'A', 'Y');
    uint32_t sigs[9];
    int nt = 0;
    sigs[nt++] = SIG('d', 'e', 's', 'c');
    sigs[nt++] = SIG('c', 'p', 'r', 't');
    sigs[nt++] = SIG('w', 't', 'p', 't');
    if (gray) sigs[nt++] = SIG('k', 'T', 'R', 'C');
    else {
        sigs[nt++] = SIG('r', 'X', 'Y', 'Z'); sigs[nt++] = SIG('g', 'X', 'Y', 'Z'); sigs[nt++] = SIG('b', 'X', 'Y', 'Z');
        sigs[nt++] = SIG('r', 'T', 'R', 'C'); sigs[nt++] = SIG('g', 'T', 'R', 'C'); sigs[nt++] = SIG('b', 'T', 'R', 'C');
    }
    wb_t body = {0};
    uint32_t off[9], len[9];
    size_t base = 128 + 4 + (size_t)nt * 12;
    for (int i = 0; i < nt; i++) {
        size_t start = body.n;
        uint32_t s = sigs[i];
        if (s == SIG('d', 'e', 's', 'c')) tag_desc(&body, p->desc[0] ? p->desc : "mOS profile");
        else if (s == SIG('c', 'p', 'r', 't')) tag_text(&body, "No copyright, use freely");
        else if (s == SIG('w', 't', 'p', 't')) tag_xyz(&body, p->wp);
        else if (s == SIG('k', 'T', 'R', 'C')) tag_curve(&body, &p->trc[0]);
        else if (s == SIG('r', 'X', 'Y', 'Z') || s == SIG('g', 'X', 'Y', 'Z') || s == SIG('b', 'X', 'Y', 'Z')) {
            int k = s == SIG('r', 'X', 'Y', 'Z') ? 0 : s == SIG('g', 'X', 'Y', 'Z') ? 1 : 2;
            double v[3] = {p->mtx[k], p->mtx[3 + k], p->mtx[6 + k]};
            tag_xyz(&body, v);
        } else {
            int k = s == SIG('r', 'T', 'R', 'C') ? 0 : s == SIG('g', 'T', 'R', 'C') ? 1 : 2;
            tag_curve(&body, &p->trc[k]);
        }
        wb_pad(&body);
        off[i] = (uint32_t)(base + start);
        len[i] = (uint32_t)(body.n - start);
    }
    wb_t w = {0};
    uint32_t total = (uint32_t)(base + body.n);
    wb_be32(&w, total);
    wb_be32(&w, 0);                                  /* CMM */
    wb_be32(&w, 0x02100000);
    wb_be32(&w, SIG('m', 'n', 't', 'r'));
    wb_be32(&w, gray ? SIG('G', 'R', 'A', 'Y') : SIG('R', 'G', 'B', ' '));
    wb_be32(&w, SIG('X', 'Y', 'Z', ' '));
    { uint8_t z[12] = {0x07, 0xEA, 0, 11, 0, 15, 0, 0, 0, 0, 0, 0}; wb_put(&w, z, 12); }   /* 2026-11-15 */
    wb_be32(&w, SIG('a', 'c', 's', 'p'));
    { uint8_t z[24] = {0}; wb_put(&w, z, 24); }      /* plataforma, flags, fabricante, modelo, atributos */
    wb_be32(&w, 0);                                  /* intenção perceptual */
    wb_s15(&w, D50[0]); wb_s15(&w, D50[1]); wb_s15(&w, D50[2]);
    { uint8_t z[48] = {0}; wb_put(&w, z, 48); }      /* criador, ID, reservado */
    wb_be32(&w, (uint32_t)nt);
    for (int i = 0; i < nt; i++) { wb_be32(&w, sigs[i]); wb_be32(&w, off[i]); wb_be32(&w, len[i]); }
    wb_put(&w, body.b, body.n);
    free(body.b);
    if (!w.b || w.n != total) { free(w.b); return -1; }
    uint8_t id[16];
    cs_profile_id(w.b, w.n, id);
    memcpy(w.b + 84, id, 16);
    *out = w.b;
    *on = w.n;
    return 0;
}

/* ---------------------------------------------------------------- MD5 (RFC 1321) */
static uint32_t rol(uint32_t x, int c) { return x << c | x >> (32 - c); }
void cs_md5(const uint8_t *msg, size_t len, uint8_t out[16]) {
    static const uint32_t K[64] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501, 0x698098d8, 0x8b44f7af, 0xffff5bb1,
        0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821, 0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453,
        0xd8a1e681, 0xe7d3fbc8, 0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a, 0xfffa3942,
        0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70, 0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
        0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665, 0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d,
        0x85845dd1, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};
    static const int R[64] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
                              4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
    uint32_t h0 = 0x67452301, h1 = 0xefcdab89, h2 = 0x98badcfe, h3 = 0x10325476;
    size_t nl = ((len + 8) / 64 + 1) * 64;
    uint8_t *m = calloc(nl, 1);
    if (!m) { memset(out, 0, 16); return; }
    memcpy(m, msg, len);
    m[len] = 0x80;
    uint64_t bits = (uint64_t)len * 8;
    for (int i = 0; i < 8; i++) m[nl - 8 + i] = (uint8_t)(bits >> (8 * i));
    for (size_t off = 0; off < nl; off += 64) {
        uint32_t w[16];
        for (int i = 0; i < 16; i++) w[i] = (uint32_t)m[off + i * 4] | (uint32_t)m[off + i * 4 + 1] << 8 | (uint32_t)m[off + i * 4 + 2] << 16 | (uint32_t)m[off + i * 4 + 3] << 24;
        uint32_t a = h0, b = h1, c = h2, d = h3;
        for (int i = 0; i < 64; i++) {
            uint32_t f;
            int g;
            if (i < 16) { f = (b & c) | (~b & d); g = i; }
            else if (i < 32) { f = (d & b) | (~d & c); g = (5 * i + 1) % 16; }
            else if (i < 48) { f = b ^ c ^ d; g = (3 * i + 5) % 16; }
            else { f = c ^ (b | ~d); g = (7 * i) % 16; }
            uint32_t t = d;
            d = c; c = b;
            b = b + rol(a + f + K[i] + w[g], R[i]);
            a = t;
        }
        h0 += a; h1 += b; h2 += c; h3 += d;
    }
    free(m);
    uint32_t h[4] = {h0, h1, h2, h3};
    for (int i = 0; i < 4; i++) for (int k = 0; k < 4; k++) out[i * 4 + k] = (uint8_t)(h[i] >> (8 * k));
}
void cs_profile_id(const uint8_t *d, size_t n, uint8_t out[16]) {
    uint8_t *c = malloc(n);
    if (!c) { memset(out, 0, 16); return; }
    memcpy(c, d, n);
    if (n >= 100) { memset(c + 44, 0, 4); memset(c + 64, 0, 4); memset(c + 84, 0, 16); }
    cs_md5(c, n, out);
    free(c);
}

#ifndef CS_HOST
#include "colorsync_cf.inc"
#endif
