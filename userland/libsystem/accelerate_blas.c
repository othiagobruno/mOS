/* Accelerate: BLAS nível 2/3 na interface Fortran (dgemm_, zgemm_, ...) e vDSP que faltavam.
 * Implementação própria, coluna-maior, inteiros de 32 bits (__CLPK_integer do Accelerate), incrementos negativos
 * como no BLAS de referência. Os comprimentos ocultos dos argumentos char (gfortran) são ignorados.
 * Teste de host: tools/accelerate_blas_test.c. */
#include <stddef.h>

typedef struct { double r, i; } zc;

static int up(const char *c) { int x = c ? *c : 'N'; return x >= 'a' && x <= 'z' ? x - 32 : x; }
static long start(int n, int inc) { return inc < 0 ? (long)(1 - n) * inc : 0; }

static zc zmul(zc a, zc b) { zc r = {a.r * b.r - a.i * b.i, a.r * b.i + a.i * b.r}; return r; }
static zc zadd(zc a, zc b) { zc r = {a.r + b.r, a.i + b.i}; return r; }
static zc zsub(zc a, zc b) { zc r = {a.r - b.r, a.i - b.i}; return r; }
static zc zconj(zc a) { zc r = {a.r, -a.i}; return r; }
static zc zdiv(zc a, zc b) {
    double d = b.r * b.r + b.i * b.i;
    zc r = {(a.r * b.r + a.i * b.i) / d, (a.i * b.r - a.r * b.i) / d};
    return r;
}
static int zzero(zc a) { return a.r == 0 && a.i == 0; }

#define A_(i, j) A[(long)(i) + (long)(j) * lda]
#define B_(i, j) B[(long)(i) + (long)(j) * ldb]
#define C_(i, j) C[(long)(i) + (long)(j) * ldc]

/* ---------------- reais ---------------- */
void dgemm_(const char *ta, const char *tb, const int *pm, const int *pn, const int *pk, const double *palpha,
            const double *A, const int *plda, const double *B, const int *pldb, const double *pbeta, double *C, const int *pldc) {
    int m = *pm, n = *pn, k = *pk, lda = *plda, ldb = *pldb, ldc = *pldc;
    double alpha = *palpha, beta = *pbeta;
    int na = up(ta) == 'N', nb = up(tb) == 'N';
    if (m <= 0 || n <= 0) return;
    for (int j = 0; j < n; j++) {
        for (int i = 0; i < m; i++) {
            double s = 0;
            if (alpha != 0)
                for (int l = 0; l < k; l++) s += (na ? A_(i, l) : A_(l, i)) * (nb ? B_(l, j) : B_(j, l));
            double c = beta == 0 ? 0 : beta * C_(i, j);
            C_(i, j) = c + alpha * s;
        }
    }
}

void dgemv_(const char *tr, const int *pm, const int *pn, const double *palpha, const double *A, const int *plda,
            const double *x, const int *pincx, const double *pbeta, double *y, const int *pincy) {
    int m = *pm, n = *pn, lda = *plda, incx = *pincx, incy = *pincy;
    double alpha = *palpha, beta = *pbeta;
    if (m <= 0 || n <= 0) return;
    int t = up(tr) != 'N';
    int lx = t ? m : n, ly = t ? n : m;
    long kx = start(lx, incx), ky = start(ly, incy);
    for (int i = 0; i < ly; i++) {
        double s = 0;
        if (alpha != 0)
            for (int l = 0; l < lx; l++) s += (t ? A_(l, i) : A_(i, l)) * x[kx + (long)l * incx];
        double *yi = &y[ky + (long)i * incy];
        *yi = (beta == 0 ? 0 : beta * *yi) + alpha * s;
    }
}

void dger_(const int *pm, const int *pn, const double *palpha, const double *x, const int *pincx,
           const double *y, const int *pincy, double *A, const int *plda) {
    int m = *pm, n = *pn, lda = *plda, incx = *pincx, incy = *pincy;
    double alpha = *palpha;
    if (m <= 0 || n <= 0 || alpha == 0) return;
    long kx = start(m, incx), ky = start(n, incy);
    for (int j = 0; j < n; j++) {
        double t = alpha * y[ky + (long)j * incy];
        for (int i = 0; i < m; i++) A_(i, j) += x[kx + (long)i * incx] * t;
    }
}

/* op(A) x = b, x sobrescreve b. */
void dtrsv_(const char *pu, const char *ptr, const char *pd, const int *pn, const double *A, const int *plda,
            double *x, const int *pincx) {
    int n = *pn, lda = *plda, incx = *pincx;
    if (n <= 0) return;
    int upper = up(pu) == 'U', t = up(ptr) != 'N', unit = up(pd) == 'U';
    long kx = start(n, incx);
#define X(i) x[kx + (long)(i) * incx]
    /* (A ou A^T) triangular: "lower efetivo" decide a ordem */
    int lower_eff = upper == t;
    if (lower_eff) {
        for (int i = 0; i < n; i++) {
            double s = X(i);
            for (int l = 0; l < i; l++) s -= (t ? A_(l, i) : A_(i, l)) * X(l);
            X(i) = unit ? s : s / (t ? A_(i, i) : A_(i, i));
        }
    } else {
        for (int i = n - 1; i >= 0; i--) {
            double s = X(i);
            for (int l = i + 1; l < n; l++) s -= (t ? A_(l, i) : A_(i, l)) * X(l);
            X(i) = unit ? s : s / A_(i, i);
        }
    }
#undef X
}

/* side L: op(A) X = alpha B; side R: X op(A) = alpha B. X sobrescreve B. */
void dtrsm_(const char *ps, const char *pu, const char *pt, const char *pd, const int *pm, const int *pn,
            const double *palpha, const double *A, const int *plda, double *B, const int *pldb) {
    int m = *pm, n = *pn, lda = *plda, ldb = *pldb;
    double alpha = *palpha;
    if (m <= 0 || n <= 0) return;
    int left = up(ps) == 'L', upper = up(pu) == 'U', t = up(pt) != 'N', unit = up(pd) == 'U';
    for (int j = 0; j < n; j++) for (int i = 0; i < m; i++) B_(i, j) = alpha == 0 ? 0 : alpha * B_(i, j);
    if (alpha == 0) return;
    int lower_eff = upper == t;
#define OPA(i, j) (t ? A_(j, i) : A_(i, j))
    if (left) {
        for (int j = 0; j < n; j++) {
            if (lower_eff) {
                for (int i = 0; i < m; i++) {
                    double s = B_(i, j);
                    for (int l = 0; l < i; l++) s -= OPA(i, l) * B_(l, j);
                    B_(i, j) = unit ? s : s / A_(i, i);
                }
            } else {
                for (int i = m - 1; i >= 0; i--) {
                    double s = B_(i, j);
                    for (int l = i + 1; l < m; l++) s -= OPA(i, l) * B_(l, j);
                    B_(i, j) = unit ? s : s / A_(i, i);
                }
            }
        }
    } else {
        /* X op(A) = B: coluna j de X depende das colunas já resolvidas */
        for (int i = 0; i < m; i++) {
            if (lower_eff) {
                for (int j = n - 1; j >= 0; j--) {
                    double s = B_(i, j);
                    for (int l = j + 1; l < n; l++) s -= B_(i, l) * OPA(l, j);
                    B_(i, j) = unit ? s : s / A_(j, j);
                }
            } else {
                for (int j = 0; j < n; j++) {
                    double s = B_(i, j);
                    for (int l = 0; l < j; l++) s -= B_(i, l) * OPA(l, j);
                    B_(i, j) = unit ? s : s / A_(j, j);
                }
            }
        }
    }
#undef OPA
}

/* ---------------- complexos ---------------- */
/* op: 0 = N, 1 = T, 2 = C (conjugada transposta) */
static int zop(const char *c) { int x = up(c); return x == 'N' ? 0 : x == 'T' ? 1 : 2; }
static zc zat(const zc *A, int lda, int op, int i, int j) {
    if (op == 0) return A_(i, j);
    zc v = A_(j, i);
    return op == 2 ? zconj(v) : v;
}

void zgemm_(const char *ta, const char *tb, const int *pm, const int *pn, const int *pk, const zc *palpha,
            const zc *A, const int *plda, const zc *B, const int *pldb, const zc *pbeta, zc *C, const int *pldc) {
    int m = *pm, n = *pn, k = *pk, lda = *plda, ldb = *pldb, ldc = *pldc;
    zc alpha = *palpha, beta = *pbeta;
    int oa = zop(ta), ob = zop(tb);
    if (m <= 0 || n <= 0) return;
    for (int j = 0; j < n; j++)
        for (int i = 0; i < m; i++) {
            zc s = {0, 0};
            if (!zzero(alpha))
                for (int l = 0; l < k; l++) s = zadd(s, zmul(zat(A, lda, oa, i, l), zat(B, ldb, ob, l, j)));
            zc c = zzero(beta) ? (zc){0, 0} : zmul(beta, C_(i, j));
            C_(i, j) = zadd(c, zmul(alpha, s));
        }
}

void zgemv_(const char *tr, const int *pm, const int *pn, const zc *palpha, const zc *A, const int *plda,
            const zc *x, const int *pincx, const zc *pbeta, zc *y, const int *pincy) {
    int m = *pm, n = *pn, lda = *plda, incx = *pincx, incy = *pincy;
    zc alpha = *palpha, beta = *pbeta;
    if (m <= 0 || n <= 0) return;
    int op = zop(tr);
    int lx = op ? m : n, ly = op ? n : m;
    long kx = start(lx, incx), ky = start(ly, incy);
    for (int i = 0; i < ly; i++) {
        zc s = {0, 0};
        if (!zzero(alpha))
            for (int l = 0; l < lx; l++) s = zadd(s, zmul(zat(A, lda, op, i, l), x[kx + (long)l * incx]));
        zc *yi = &y[ky + (long)i * incy];
        *yi = zadd(zzero(beta) ? (zc){0, 0} : zmul(beta, *yi), zmul(alpha, s));
    }
}

void zgeru_(const int *pm, const int *pn, const zc *palpha, const zc *x, const int *pincx,
            const zc *y, const int *pincy, zc *A, const int *plda) {
    int m = *pm, n = *pn, lda = *plda, incx = *pincx, incy = *pincy;
    zc alpha = *palpha;
    if (m <= 0 || n <= 0 || zzero(alpha)) return;
    long kx = start(m, incx), ky = start(n, incy);
    for (int j = 0; j < n; j++) {
        zc t = zmul(alpha, y[ky + (long)j * incy]);
        for (int i = 0; i < m; i++) A_(i, j) = zadd(A_(i, j), zmul(x[kx + (long)i * incx], t));
    }
}

void ztrsv_(const char *pu, const char *ptr, const char *pd, const int *pn, const zc *A, const int *plda,
            zc *x, const int *pincx) {
    int n = *pn, lda = *plda, incx = *pincx;
    if (n <= 0) return;
    int upper = up(pu) == 'U', op = zop(ptr), unit = up(pd) == 'U';
    long kx = start(n, incx);
#define X(i) x[kx + (long)(i) * incx]
    int lower_eff = upper == (op != 0);
    if (lower_eff) {
        for (int i = 0; i < n; i++) {
            zc s = X(i);
            for (int l = 0; l < i; l++) s = zsub(s, zmul(zat(A, lda, op, i, l), X(l)));
            X(i) = unit ? s : zdiv(s, zat(A, lda, op, i, i));
        }
    } else {
        for (int i = n - 1; i >= 0; i--) {
            zc s = X(i);
            for (int l = i + 1; l < n; l++) s = zsub(s, zmul(zat(A, lda, op, i, l), X(l)));
            X(i) = unit ? s : zdiv(s, zat(A, lda, op, i, i));
        }
    }
#undef X
}

void ztrsm_(const char *ps, const char *pu, const char *pt, const char *pd, const int *pm, const int *pn,
            const zc *palpha, const zc *A, const int *plda, zc *B, const int *pldb) {
    int m = *pm, n = *pn, lda = *plda, ldb = *pldb;
    zc alpha = *palpha;
    if (m <= 0 || n <= 0) return;
    int left = up(ps) == 'L', upper = up(pu) == 'U', op = zop(pt), unit = up(pd) == 'U';
    for (int j = 0; j < n; j++) for (int i = 0; i < m; i++) B_(i, j) = zzero(alpha) ? (zc){0, 0} : zmul(alpha, B_(i, j));
    if (zzero(alpha)) return;
    int lower_eff = upper == (op != 0);
    if (left) {
        for (int j = 0; j < n; j++) {
            if (lower_eff) {
                for (int i = 0; i < m; i++) {
                    zc s = B_(i, j);
                    for (int l = 0; l < i; l++) s = zsub(s, zmul(zat(A, lda, op, i, l), B_(l, j)));
                    B_(i, j) = unit ? s : zdiv(s, zat(A, lda, op, i, i));
                }
            } else {
                for (int i = m - 1; i >= 0; i--) {
                    zc s = B_(i, j);
                    for (int l = i + 1; l < m; l++) s = zsub(s, zmul(zat(A, lda, op, i, l), B_(l, j)));
                    B_(i, j) = unit ? s : zdiv(s, zat(A, lda, op, i, i));
                }
            }
        }
    } else {
        for (int i = 0; i < m; i++) {
            if (lower_eff) {
                for (int j = n - 1; j >= 0; j--) {
                    zc s = B_(i, j);
                    for (int l = j + 1; l < n; l++) s = zsub(s, zmul(B_(i, l), zat(A, lda, op, l, j)));
                    B_(i, j) = unit ? s : zdiv(s, zat(A, lda, op, j, j));
                }
            } else {
                for (int j = 0; j < n; j++) {
                    zc s = B_(i, j);
                    for (int l = 0; l < j; l++) s = zsub(s, zmul(B_(i, l), zat(A, lda, op, l, j)));
                    B_(i, j) = unit ? s : zdiv(s, zat(A, lda, op, j, j));
                }
            }
        }
    }
}

/* ---------------- vDSP ---------------- */
typedef long vDSP_Stride;
typedef unsigned long vDSP_Length;

/* *C = max(A[n*IA]); n = 0 devolve -INFINITY como o Accelerate */
void vDSP_maxv(const float *A, vDSP_Stride IA, float *C, vDSP_Length N) {
    float m = -__builtin_inff();
    for (vDSP_Length n = 0; n < N; n++) if (A[(long)n * IA] > m) m = A[(long)n * IA];
    *C = m;
}
/* média dos quadrados */
void vDSP_measqv(const float *A, vDSP_Stride IA, float *C, vDSP_Length N) {
    double s = 0;
    for (vDSP_Length n = 0; n < N; n++) { double v = A[(long)n * IA]; s += v * v; }
    *C = N ? (float)(s / (double)N) : 0;
}
/* D = A * B + C (B e C escalares) */
void vDSP_vsmsa(const float *A, vDSP_Stride IA, const float *B, const float *C, float *D, vDSP_Stride ID, vDSP_Length N) {
    float b = *B, c = *C;
    for (vDSP_Length n = 0; n < N; n++) D[(long)n * ID] = A[(long)n * IA] * b + c;
}
