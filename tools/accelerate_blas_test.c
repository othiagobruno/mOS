/* Teste de host do BLAS do Accelerate (userland/libsystem/accelerate_blas.c).
 * gemm/gemv/ger contra produtos ingênuos escritos aqui; trsm/trsv por reconstrução (op(A)·X = α·B), em todas as
 * combinações de lado, triângulo, transposição (N/T/C), diagonal e incrementos negativos. Saída: "blas: N/N ok".
 * gcc -O1 tools/accelerate_blas_test.c userland/libsystem/accelerate_blas.c -lm -o /tmp/blas && /tmp/blas */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct { double r, i; } zc;
void dgemm_(const char *, const char *, const int *, const int *, const int *, const double *, const double *, const int *, const double *, const int *, const double *, double *, const int *);
void dgemv_(const char *, const int *, const int *, const double *, const double *, const int *, const double *, const int *, const double *, double *, const int *);
void dger_(const int *, const int *, const double *, const double *, const int *, const double *, const int *, double *, const int *);
void dtrsv_(const char *, const char *, const char *, const int *, const double *, const int *, double *, const int *);
void dtrsm_(const char *, const char *, const char *, const char *, const int *, const int *, const double *, const double *, const int *, double *, const int *);
void zgemm_(const char *, const char *, const int *, const int *, const int *, const zc *, const zc *, const int *, const zc *, const int *, const zc *, zc *, const int *);
void zgemv_(const char *, const int *, const int *, const zc *, const zc *, const int *, const zc *, const int *, const zc *, zc *, const int *);
void zgeru_(const int *, const int *, const zc *, const zc *, const int *, const zc *, const int *, zc *, const int *);
void ztrsv_(const char *, const char *, const char *, const int *, const zc *, const int *, zc *, const int *);
void ztrsm_(const char *, const char *, const char *, const char *, const int *, const int *, const zc *, const zc *, const int *, zc *, const int *);
void vDSP_maxv(const float *, long, float *, unsigned long);
void vDSP_measqv(const float *, long, float *, unsigned long);
void vDSP_vsmsa(const float *, long, const float *, const float *, float *, long, unsigned long);

static int ok, total;
static void check(const char *w, double err) {
    total++;
    if (err < 1e-9) ok++; else printf("FALHA %s erro %g\n", w, err);
}
static double rnd(void) { return (double)rand() / RAND_MAX * 2 - 1; }
static zc zr(void) { zc z = {rnd(), rnd()}; return z; }
static zc zm(zc a, zc b) { zc r = {a.r * b.r - a.i * b.i, a.r * b.i + a.i * b.r}; return r; }
static zc za(zc a, zc b) { zc r = {a.r + b.r, a.i + b.i}; return r; }
/* op(A)(i,j) ingênuo */
static double dop(const double *A, int lda, int t, int i, int j) { return t ? A[j + i * lda] : A[i + j * lda]; }
static zc zop(const zc *A, int lda, int op, int i, int j) {
    zc v = op ? A[j + i * lda] : A[i + j * lda];
    if (op == 2) v.i = -v.i;
    return v;
}
/* matriz triangular bem condicionada */
static void tri(double *A, int n, int upper, int unit) {
    for (int j = 0; j < n; j++) for (int i = 0; i < n; i++) {
        int in = upper ? i <= j : i >= j;
        A[i + j * n] = in ? (i == j ? (unit ? 77 : 2 + fabs(rnd())) : rnd() * 0.3) : 999; /* lixo fora do triângulo */
    }
}
static void ztri(zc *A, int n, int upper, int unit) {
    for (int j = 0; j < n; j++) for (int i = 0; i < n; i++) {
        int in = upper ? i <= j : i >= j;
        zc d = {2 + fabs(rnd()), rnd() * 0.5}, g = {999, 999}, u = {77, 0}, o = {rnd() * 0.3, rnd() * 0.3};
        A[i + j * n] = in ? (i == j ? (unit ? u : d) : o) : g;
    }
}
/* elemento de op(A) respeitando triângulo e diagonal unitária */
static double dtri_el(const double *A, int n, int upper, int t, int unit, int i, int j) {
    int r = t ? j : i, c = t ? i : j;
    int in = upper ? r <= c : r >= c;
    if (!in) return 0;
    if (r == c && unit) return 1;
    return A[r + c * n];
}
static zc ztri_el(const zc *A, int n, int upper, int op, int unit, int i, int j) {
    int r = op ? j : i, c = op ? i : j;
    int in = upper ? r <= c : r >= c;
    zc z = {0, 0}, one = {1, 0};
    if (!in) return z;
    if (r == c && unit) return one;
    zc v = A[r + c * n];
    if (op == 2) v.i = -v.i;
    return v;
}

int main(void) {
    srand(7);
    const char *TR = "NTC";
    /* dgemm */
    for (int a = 0; a < 2; a++) for (int b = 0; b < 2; b++) {
        int m = 5, n = 4, k = 3, lda = 9, ldb = 8, ldc = 7;
        double A[9 * 9], B[8 * 8], C[7 * 7], R[7 * 7], al = 1.5, be = -0.5;
        for (int i = 0; i < 81; i++) A[i] = rnd();
        for (int i = 0; i < 64; i++) B[i] = rnd();
        for (int i = 0; i < 49; i++) C[i] = R[i] = rnd();
        dgemm_(&TR[a], &TR[b], &m, &n, &k, &al, A, &lda, B, &ldb, &be, C, &ldc);
        double e = 0;
        for (int j = 0; j < n; j++) for (int i = 0; i < m; i++) {
            double s = 0;
            for (int l = 0; l < k; l++) s += dop(A, lda, a, i, l) * dop(B, ldb, b, l, j);
            e = fmax(e, fabs(C[i + j * ldc] - (al * s + be * R[i + j * ldc])));
        }
        char w[32]; snprintf(w, sizeof w, "dgemm %c%c", TR[a], TR[b]); check(w, e);
    }
    /* zgemm */
    for (int a = 0; a < 3; a++) for (int b = 0; b < 3; b++) {
        int m = 4, n = 3, k = 5, lda = 6, ldb = 6, ldc = 5;
        zc A[36], B[36], C[25], R[25], al = {1.2, -0.3}, be = {0.4, 0.7};
        for (int i = 0; i < 36; i++) { A[i] = zr(); B[i] = zr(); }
        for (int i = 0; i < 25; i++) C[i] = R[i] = zr();
        zgemm_(&TR[a], &TR[b], &m, &n, &k, &al, A, &lda, B, &ldb, &be, C, &ldc);
        double e = 0;
        for (int j = 0; j < n; j++) for (int i = 0; i < m; i++) {
            zc s = {0, 0};
            for (int l = 0; l < k; l++) s = za(s, zm(zop(A, lda, a, i, l), zop(B, ldb, b, l, j)));
            zc w = za(zm(al, s), zm(be, R[i + j * ldc]));
            e = fmax(e, hypot(C[i + j * ldc].r - w.r, C[i + j * ldc].i - w.i));
        }
        char w[32]; snprintf(w, sizeof w, "zgemm %c%c", TR[a], TR[b]); check(w, e);
    }
    /* dgemv / zgemv com incrementos 1, 2, -1, -3 */
    int incs[4] = {1, 2, -1, -3};
    for (int t = 0; t < 2; t++) for (int ix = 0; ix < 4; ix++) for (int iy = 0; iy < 4; iy++) {
        int m = 5, n = 3, lda = 6, incx = incs[ix], incy = incs[iy];
        int lx = t ? m : n, ly = t ? n : m;
        double A[36], x[32], y[32], y0[32], al = 0.7, be = 1.3;
        for (int i = 0; i < 36; i++) A[i] = rnd();
        for (int i = 0; i < 32; i++) { x[i] = rnd(); y[i] = y0[i] = rnd(); }
        dgemv_(&TR[t], &m, &n, &al, A, &lda, x, &incx, &be, y, &incy);
        double e = 0;
        for (int i = 0; i < ly; i++) {
            double s = 0;
            for (int l = 0; l < lx; l++) s += dop(A, lda, t, i, l) * x[incx > 0 ? l * incx : (lx - 1 - l) * -incx];
            int yi = incy > 0 ? i * incy : (ly - 1 - i) * -incy;
            e = fmax(e, fabs(y[yi] - (al * s + be * y0[yi])));
        }
        char w[48]; snprintf(w, sizeof w, "dgemv %c %d %d", TR[t], incx, incy); check(w, e);
    }
    for (int t = 0; t < 3; t++) for (int ix = 0; ix < 4; ix++) {
        int m = 4, n = 5, lda = 4, incx = incs[ix], incy = incs[3 - ix];
        int lx = t ? m : n, ly = t ? n : m;
        zc A[20], x[32], y[32], y0[32], al = {0.5, 0.5}, be = {-1, 0.25};
        for (int i = 0; i < 20; i++) A[i] = zr();
        for (int i = 0; i < 32; i++) { x[i] = zr(); y[i] = y0[i] = zr(); }
        zgemv_(&TR[t], &m, &n, &al, A, &lda, x, &incx, &be, y, &incy);
        double e = 0;
        for (int i = 0; i < ly; i++) {
            zc s = {0, 0};
            for (int l = 0; l < lx; l++) s = za(s, zm(zop(A, lda, t, i, l), x[incx > 0 ? l * incx : (lx - 1 - l) * -incx]));
            int yi = incy > 0 ? i * incy : (ly - 1 - i) * -incy;
            zc w = za(zm(al, s), zm(be, y0[yi]));
            e = fmax(e, hypot(y[yi].r - w.r, y[yi].i - w.i));
        }
        char w[48]; snprintf(w, sizeof w, "zgemv %c %d %d", TR[t], incx, incy); check(w, e);
    }
    /* dger / zgeru */
    for (int ix = 0; ix < 4; ix++) {
        int m = 4, n = 3, lda = 5, incx = incs[ix], incy = incs[(ix + 1) % 4];
        double A[15], A0[15], x[16], y[16], al = -0.8;
        zc ZA[15], ZA0[15], zx[16], zy[16], zal = {0.3, -1.1};
        for (int i = 0; i < 15; i++) { A[i] = A0[i] = rnd(); ZA[i] = ZA0[i] = zr(); }
        for (int i = 0; i < 16; i++) { x[i] = rnd(); y[i] = rnd(); zx[i] = zr(); zy[i] = zr(); }
        dger_(&m, &n, &al, x, &incx, y, &incy, A, &lda);
        zgeru_(&m, &n, &zal, zx, &incx, zy, &incy, ZA, &lda);
        double e = 0, ez = 0;
        for (int j = 0; j < n; j++) for (int i = 0; i < m; i++) {
            int xi = incx > 0 ? i * incx : (m - 1 - i) * -incx, yj = incy > 0 ? j * incy : (n - 1 - j) * -incy;
            e = fmax(e, fabs(A[i + j * lda] - (A0[i + j * lda] + al * x[xi] * y[yj])));
            zc w = za(ZA0[i + j * lda], zm(zal, zm(zx[xi], zy[yj])));
            ez = fmax(ez, hypot(ZA[i + j * lda].r - w.r, ZA[i + j * lda].i - w.i));
        }
        char w[32]; snprintf(w, sizeof w, "dger %d %d", incx, incy); check(w, e);
        snprintf(w, sizeof w, "zgeru %d %d", incx, incy); check(w, ez);
    }
    /* trsv: resolve e reconstrói */
    const char *UL = "UL", *DG = "NU";
    for (int u = 0; u < 2; u++) for (int t = 0; t < 3; t++) for (int d = 0; d < 2; d++) for (int ix = 0; ix < 4; ix++) {
        int n = 5, lda = 5, inc = incs[ix];
        long k0 = inc > 0 ? 0 : (long)(n - 1) * -inc;
        if (t < 2) {
            double A[25], b[32], x[32];
            tri(A, n, u == 0, d);
            for (int i = 0; i < 32; i++) b[i] = x[i] = rnd();
            dtrsv_(&UL[u], &TR[t], &DG[d], &n, A, &lda, x, &inc);
            double e = 0;
            for (int i = 0; i < n; i++) {
                double s = 0;
                for (int l = 0; l < n; l++) s += dtri_el(A, n, u == 0, t, d, i, l) * x[k0 + (long)l * inc];
                e = fmax(e, fabs(s - b[k0 + (long)i * inc]));
            }
            char w[48]; snprintf(w, sizeof w, "dtrsv %c%c%c %d", UL[u], TR[t], DG[d], inc); check(w, e);
        }
        zc A[25], b[32], x[32];
        ztri(A, n, u == 0, d);
        for (int i = 0; i < 32; i++) b[i] = x[i] = zr();
        ztrsv_(&UL[u], &TR[t], &DG[d], &n, A, &lda, x, &inc);
        double e = 0;
        for (int i = 0; i < n; i++) {
            zc s = {0, 0};
            for (int l = 0; l < n; l++) s = za(s, zm(ztri_el(A, n, u == 0, t, d, i, l), x[k0 + (long)l * inc]));
            e = fmax(e, hypot(s.r - b[k0 + (long)i * inc].r, s.i - b[k0 + (long)i * inc].i));
        }
        char w[48]; snprintf(w, sizeof w, "ztrsv %c%c%c %d", UL[u], TR[t], DG[d], inc); check(w, e);
    }
    /* trsm: L e R */
    const char *SD = "LR";
    for (int s = 0; s < 2; s++) for (int u = 0; u < 2; u++) for (int t = 0; t < 3; t++) for (int d = 0; d < 2; d++) {
        int m = 4, n = 3, na = s == 0 ? m : n, lda = na, ldb = 6;
        if (t < 2) {
            double A[16], B[18], X[18], al = 1.7;
            tri(A, na, u == 0, d);
            for (int i = 0; i < 18; i++) B[i] = X[i] = rnd();
            dtrsm_(&SD[s], &UL[u], &TR[t], &DG[d], &m, &n, &al, A, &lda, X, &ldb);
            double e = 0;
            for (int j = 0; j < n; j++) for (int i = 0; i < m; i++) {
                double v = 0;
                if (s == 0) for (int l = 0; l < m; l++) v += dtri_el(A, na, u == 0, t, d, i, l) * X[l + j * ldb];
                else for (int l = 0; l < n; l++) v += X[i + l * ldb] * dtri_el(A, na, u == 0, t, d, l, j);
                e = fmax(e, fabs(v - al * B[i + j * ldb]));
            }
            char w[48]; snprintf(w, sizeof w, "dtrsm %c%c%c%c", SD[s], UL[u], TR[t], DG[d]); check(w, e);
        }
        zc A[16], B[18], X[18], al = {0.9, -0.4};
        ztri(A, na, u == 0, d);
        for (int i = 0; i < 18; i++) B[i] = X[i] = zr();
        ztrsm_(&SD[s], &UL[u], &TR[t], &DG[d], &m, &n, &al, A, &lda, X, &ldb);
        double e = 0;
        for (int j = 0; j < n; j++) for (int i = 0; i < m; i++) {
            zc v = {0, 0};
            if (s == 0) for (int l = 0; l < m; l++) v = za(v, zm(ztri_el(A, na, u == 0, t, d, i, l), X[l + j * ldb]));
            else for (int l = 0; l < n; l++) v = za(v, zm(X[i + l * ldb], ztri_el(A, na, u == 0, t, d, l, j)));
            zc w = zm(al, B[i + j * ldb]);
            e = fmax(e, hypot(v.r - w.r, v.i - w.i));
        }
        char w[48]; snprintf(w, sizeof w, "ztrsm %c%c%c%c", SD[s], UL[u], TR[t], DG[d]); check(w, e);
    }
    /* vDSP */
    float v[8] = {1, -4, 9, 2, -7, 3, 5, 0}, c, d[8];
    vDSP_maxv(v, 2, &c, 4);                 /* 1, 9, -7, 5 */
    check("vDSP_maxv", fabs(c - 9));
    vDSP_maxv(v, 1, &c, 0);
    check("vDSP_maxv vazio", isinf(c) && c < 0 ? 0 : 1);
    vDSP_measqv(v, 1, &c, 8);               /* (1+16+81+4+49+9+25+0)/8 = 23.125 */
    check("vDSP_measqv", fabs(c - 23.125f));
    float sb = 2, sc = -1;
    vDSP_vsmsa(v, 1, &sb, &sc, d, 1, 8);
    double e = 0;
    for (int i = 0; i < 8; i++) e = fmax(e, fabs(d[i] - (v[i] * 2 - 1)));
    check("vDSP_vsmsa", e);
    printf("blas: %d/%d ok\n", ok, total);
    return ok == total ? 0 : 1;
}
