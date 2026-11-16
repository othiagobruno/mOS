/*
 * mOS — LAPACK do Accelerate (interface nova "$NEWLAPACK" e a clássica "x_"), em C e sem código de terceiros:
 * getrf/getrs/getri/gesv (LU com pivô parcial), potrf (Cholesky), trtri, geqrf + orgqr/ungqr (Householder como o
 * xLARFG), syevd/heevd (Jacobi hermitiano, autovalores em ordem crescente), gesdd (SVD por Jacobi de um lado),
 * geev (Hessenberg + QR complexo com deslocamento de Wilkinson, vetores pela forma de Schur) e gelsd (mínimos
 * quadrados de norma mínima pela SVD). Tudo calcula em complexo double; s/d/c/z convertem na entrada e na saída.
 * Matrizes em ordem de coluna, argumentos por ponteiro, como na referência. As consultas de espaço (lwork = -1)
 * devolvem tamanhos válidos; o espaço de trabalho de verdade é alocado aqui. Teste: tools/accelerate_lapack_test.c.
 * Código original do mOS (MIT).
 */
#include <stddef.h>
#include <stdint.h>

void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
double sqrt(double), fabs(double), hypot(double, double);

typedef struct { double r, i; } zc;
typedef struct { float r, i; } cf;
static zc Z(double r, double i) { zc z = {r, i}; return z; }
static zc zmul(zc a, zc b) { return Z(a.r * b.r - a.i * b.i, a.r * b.i + a.i * b.r); }
static zc zadd(zc a, zc b) { return Z(a.r + b.r, a.i + b.i); }
static zc zsub(zc a, zc b) { return Z(a.r - b.r, a.i - b.i); }
static zc zcj(zc a) { return Z(a.r, -a.i); }
static zc zsc(zc a, double s) { return Z(a.r * s, a.i * s); }
static double zabs(zc a) { return hypot(a.r, a.i); }
static zc zdiv(zc a, zc b) {
    double d = b.r * b.r + b.i * b.i;
    return d == 0 ? Z(0, 0) : Z((a.r * b.r + a.i * b.i) / d, (a.i * b.r - a.r * b.i) / d);
}
static double abs1(zc a) { return fabs(a.r) + fabs(a.i); }
#define EPS 2.220446049250313e-16
#define E(A, ld, i, j) (A)[(size_t)(i) + (size_t)(j) * (size_t)(ld)]

/* ---------------------------------------------------------------- conversão s/d/c/z <-> zc */
enum { K_S, K_D, K_C, K_Z };
static zc *load(int k, const void *a, int ld, int m, int n) {
    zc *z = calloc((size_t)(m ? m : 1) * (size_t)(n ? n : 1), sizeof *z);
    if (!z) return 0;
    for (int j = 0; j < n; j++)
        for (int i = 0; i < m; i++) {
            size_t o = (size_t)i + (size_t)j * (size_t)ld;
            zc v = k == K_S ? Z(((const float *)a)[o], 0) : k == K_D ? Z(((const double *)a)[o], 0)
                 : k == K_C ? Z(((const cf *)a)[o].r, ((const cf *)a)[o].i) : ((const zc *)a)[o];
            z[i + (size_t)j * m] = v;
        }
    return z;
}
static void store(int k, void *a, int ld, const zc *z, int m, int n) {
    for (int j = 0; j < n; j++)
        for (int i = 0; i < m; i++) {
            size_t o = (size_t)i + (size_t)j * (size_t)ld;
            zc v = z[i + (size_t)j * m];
            if (k == K_S) ((float *)a)[o] = (float)v.r;
            else if (k == K_D) ((double *)a)[o] = v.r;
            else if (k == K_C) { ((cf *)a)[o].r = (float)v.r; ((cf *)a)[o].i = (float)v.i; }
            else ((zc *)a)[o] = v;
        }
}
static void store_real(int k, void *a, const double *v, int n) {   /* vetor real (w, s): float para s/c, double para d/z */
    for (int i = 0; i < n; i++) { if (k == K_S || k == K_C) ((float *)a)[i] = (float)v[i]; else ((double *)a)[i] = v[i]; }
}
static void work_size(int k, void *work, double n) {
    if (!work) return;
    if (k == K_S) ((float *)work)[0] = (float)n;
    else if (k == K_D) ((double *)work)[0] = n;
    else if (k == K_C) { ((cf *)work)[0].r = (float)n; ((cf *)work)[0].i = 0; }
    else ((zc *)work)[0] = Z(n, 0);
}
static int up(const char *c) { int x = c ? *c : 'N'; return x >= 'a' && x <= 'z' ? x - 32 : x; }

/* ---------------------------------------------------------------- LU */
static int lu(zc *A, int m, int n, int *ipiv) {
    int info = 0, mn = m < n ? m : n;
    for (int j = 0; j < mn; j++) {
        int p = j;
        for (int i = j + 1; i < m; i++) if (abs1(E(A, m, i, j)) > abs1(E(A, m, p, j))) p = i;
        ipiv[j] = p + 1;
        if (abs1(E(A, m, p, j)) == 0) { if (!info) info = j + 1; continue; }
        if (p != j) for (int c = 0; c < n; c++) { zc t = E(A, m, j, c); E(A, m, j, c) = E(A, m, p, c); E(A, m, p, c) = t; }
        zc piv = E(A, m, j, j);
        for (int i = j + 1; i < m; i++) E(A, m, i, j) = zdiv(E(A, m, i, j), piv);
        for (int c = j + 1; c < n; c++)
            for (int i = j + 1; i < m; i++) E(A, m, i, c) = zsub(E(A, m, i, c), zmul(E(A, m, i, j), E(A, m, j, c)));
    }
    return info;
}
static void lu_solve(const zc *A, int n, const int *ipiv, zc *B, int nrhs) {
    for (int c = 0; c < nrhs; c++) {
        zc *b = B + (size_t)c * n;
        for (int i = 0; i < n; i++) { int p = ipiv[i] - 1; if (p != i) { zc t = b[i]; b[i] = b[p]; b[p] = t; } }
        for (int i = 0; i < n; i++) for (int k = 0; k < i; k++) b[i] = zsub(b[i], zmul(E(A, n, i, k), b[k]));
        for (int i = n - 1; i >= 0; i--) {
            for (int k = i + 1; k < n; k++) b[i] = zsub(b[i], zmul(E(A, n, i, k), b[k]));
            b[i] = zdiv(b[i], E(A, n, i, i));
        }
    }
}
static void getrf(int k, const int *pm, const int *pn, void *a, const int *plda, int *ipiv, int *info) {
    int m = *pm, n = *pn;
    *info = m < 0 ? -1 : n < 0 ? -2 : *plda < (m > 1 ? m : 1) ? -4 : 0;
    if (*info || !m || !n) return;
    zc *A = load(k, a, *plda, m, n);
    if (!A) { *info = -1000; return; }
    *info = lu(A, m, n, ipiv);
    store(k, a, *plda, A, m, n);
    free(A);
}
static void getrs(int k, const char *tr, const int *pn, const int *pnrhs, const void *a, const int *plda, const int *ipiv, void *b, const int *pldb, int *info) {
    int n = *pn, nrhs = *pnrhs;
    *info = n < 0 ? -2 : nrhs < 0 ? -3 : 0;
    if (*info || !n || !nrhs) return;
    zc *A = load(k, a, *plda, n, n), *B = load(k, b, *pldb, n, nrhs);
    if (!A || !B) { free(A); free(B); *info = -1000; return; }
    if (up(tr) == 'N') lu_solve(A, n, ipiv, B, nrhs);
    else {   /* A^T x = b (ou A^H): U^T L^T P x = b */
        int conj = up(tr) == 'C';
        for (int c = 0; c < nrhs; c++) {
            zc *x = B + (size_t)c * n;
            for (int i = 0; i < n; i++) {
                for (int q = 0; q < i; q++) { zc u = E(A, n, q, i); if (conj) u = zcj(u); x[i] = zsub(x[i], zmul(u, x[q])); }
                zc d = E(A, n, i, i); if (conj) d = zcj(d);
                x[i] = zdiv(x[i], d);
            }
            for (int i = n - 1; i >= 0; i--)
                for (int q = i + 1; q < n; q++) { zc l = E(A, n, q, i); if (conj) l = zcj(l); x[i] = zsub(x[i], zmul(l, x[q])); }
            for (int i = n - 1; i >= 0; i--) { int p = ipiv[i] - 1; if (p != i) { zc t = x[i]; x[i] = x[p]; x[p] = t; } }
        }
    }
    store(k, b, *pldb, B, n, nrhs);
    free(A); free(B);
}
static void getri(int k, const int *pn, void *a, const int *plda, const int *ipiv, void *work, const int *plwork, int *info) {
    int n = *pn;
    *info = 0;
    if (*plwork == -1) { work_size(k, work, n > 0 ? n : 1); return; }
    if (n <= 0) return;
    zc *A = load(k, a, *plda, n, n), *I = calloc((size_t)n * n, sizeof *I);
    if (!A || !I) { free(A); free(I); *info = -1000; return; }
    for (int i = 0; i < n; i++) if (abs1(E(A, n, i, i)) == 0) { *info = i + 1; free(A); free(I); return; }
    for (int i = 0; i < n; i++) E(I, n, i, i) = Z(1, 0);
    lu_solve(A, n, ipiv, I, n);
    store(k, a, *plda, I, n, n);
    free(A); free(I);
}
static void gesv(int k, const int *pn, const int *pnrhs, void *a, const int *plda, int *ipiv, void *b, const int *pldb, int *info) {
    getrf(k, pn, pn, a, plda, ipiv, info);
    if (*info) return;
    static const char N = 'N';
    getrs(k, &N, pn, pnrhs, a, plda, ipiv, b, pldb, info);
}

/* ---------------------------------------------------------------- Cholesky e triangular */
static void potrf(int k, const char *uplo, const int *pn, void *a, const int *plda, int *info) {
    int n = *pn, U = up(uplo) == 'U';
    *info = 0;
    if (n <= 0) return;
    zc *A = load(k, a, *plda, n, n);
    if (!A) { *info = -1000; return; }
    for (int j = 0; j < n && !*info; j++) {   /* L L^H com o triângulo de baixo (U: A^H no lugar) */
        double d = U ? E(A, n, j, j).r : E(A, n, j, j).r;
        for (int q = 0; q < j; q++) { zc l = U ? zcj(E(A, n, q, j)) : E(A, n, j, q); d -= l.r * l.r + l.i * l.i; }
        if (!(d > 0)) { *info = j + 1; break; }
        d = sqrt(d);
        E(A, n, j, j) = Z(d, 0);
        for (int i = j + 1; i < n; i++) {
            zc s = U ? zcj(E(A, n, j, i)) : E(A, n, i, j);
            for (int q = 0; q < j; q++) {
                zc li = U ? zcj(E(A, n, q, i)) : E(A, n, i, q), lj = U ? zcj(E(A, n, q, j)) : E(A, n, j, q);
                s = zsub(s, zmul(li, zcj(lj)));
            }
            s = zsc(s, 1 / d);
            if (U) E(A, n, j, i) = zcj(s); else E(A, n, i, j) = s;
        }
    }
    store(k, a, *plda, A, n, n);   /* em erro, as colunas antes de j já estão fatoradas, como na referência */
    free(A);
}
static void trtri(int k, const char *uplo, const char *diag, const int *pn, void *a, const int *plda, int *info) {
    int n = *pn, U = up(uplo) == 'U', unit = up(diag) == 'U';
    *info = 0;
    if (n <= 0) return;
    zc *A = load(k, a, *plda, n, n), *X = calloc((size_t)n * n, sizeof *X);
    if (!A || !X) { free(A); free(X); *info = -1000; return; }
    if (!unit) for (int i = 0; i < n; i++) if (abs1(E(A, n, i, i)) == 0) { *info = i + 1; free(A); free(X); return; }
    for (int c = 0; c < n; c++) {   /* coluna c da inversa: A x = e_c */
        if (U) for (int i = c; i >= 0; i--) {
            zc s = Z(i == c, 0);
            for (int q = i + 1; q <= c; q++) s = zsub(s, zmul(E(A, n, i, q), E(X, n, q, c)));
            E(X, n, i, c) = unit ? s : zdiv(s, E(A, n, i, i));
        }
        else for (int i = c; i < n; i++) {
            zc s = Z(i == c, 0);
            for (int q = c; q < i; q++) s = zsub(s, zmul(E(A, n, i, q), E(X, n, q, c)));
            E(X, n, i, c) = unit ? s : zdiv(s, E(A, n, i, i));
        }
    }
    for (int j = 0; j < n; j++) for (int i = 0; i < n; i++) if (U ? i <= j : i >= j) { if (!(unit && i == j)) E(A, n, i, j) = E(X, n, i, j); }
    store(k, a, *plda, A, n, n);
    free(A); free(X);
}

/* ---------------------------------------------------------------- QR (Householder, convenção do xLARFG) */
/* gera H = I - tau v v^H com v(0) = 1 tal que H^H [alpha; x] = [beta; 0]; x vira v(1:) */
static zc larfg(zc *alpha, zc *x, int n1, int incx) {
    double xn = 0;
    for (int i = 0; i < n1; i++) { zc v = x[(size_t)i * incx]; xn += v.r * v.r + v.i * v.i; }
    xn = sqrt(xn);
    if (xn == 0 && alpha->i == 0) return Z(0, 0);
    double beta = sqrt(alpha->r * alpha->r + alpha->i * alpha->i + xn * xn);
    if (alpha->r >= 0) beta = -beta;
    zc tau = Z((beta - alpha->r) / beta, -alpha->i / beta);
    zc sc = zdiv(Z(1, 0), zsub(*alpha, Z(beta, 0)));
    for (int i = 0; i < n1; i++) x[(size_t)i * incx] = zmul(x[(size_t)i * incx], sc);
    *alpha = Z(beta, 0);
    return tau;
}
/* C (rows x cols, ld) <- (I - tau v v^H) C, v[0] = 1 implícito */
static void apply_h(zc tau, const zc *v, zc *C, int ld, int rows, int cols) {
    if (tau.r == 0 && tau.i == 0) return;
    for (int c = 0; c < cols; c++) {
        zc w = E(C, ld, 0, c);
        for (int i = 1; i < rows; i++) w = zadd(w, zmul(zcj(v[i]), E(C, ld, i, c)));
        w = zmul(tau, w);
        E(C, ld, 0, c) = zsub(E(C, ld, 0, c), w);
        for (int i = 1; i < rows; i++) E(C, ld, i, c) = zsub(E(C, ld, i, c), zmul(v[i], w));
    }
}
static void qr(zc *A, int m, int n, zc *tau) {
    int kk = m < n ? m : n;
    zc *v = malloc(sizeof(zc) * (size_t)(m ? m : 1));
    for (int i = 0; i < kk && v; i++) {
        tau[i] = larfg(&E(A, m, i, i), &E(A, m, i + 1 < m ? i + 1 : i, i), m - i - 1, 1);
        v[0] = Z(1, 0);
        for (int r = 1; r < m - i; r++) v[r] = E(A, m, i + r, i);
        apply_h(zcj(tau[i]), v, &E(A, m, i, i + 1 < n ? i + 1 : i), m, m - i, n - i - 1);   /* H^H A */
    }
    free(v);
}
static void geqrf(int k, const int *pm, const int *pn, void *a, const int *plda, void *tau, void *work, const int *plwork, int *info) {
    int m = *pm, n = *pn, kk = m < n ? m : n;
    *info = 0;
    if (*plwork == -1) { work_size(k, work, n > 0 ? n : 1); return; }
    if (!kk) return;
    zc *A = load(k, a, *plda, m, n), *T = calloc((size_t)kk, sizeof *T);
    if (!A || !T) { free(A); free(T); *info = -1000; return; }
    qr(A, m, n, T);
    store(k, a, *plda, A, m, n);
    store(k, tau, kk, T, kk, 1);
    free(A); free(T);
}
static void orgqr(int k, const int *pm, const int *pn, const int *pk, void *a, const int *plda, const void *tau, void *work, const int *plwork, int *info) {
    int m = *pm, n = *pn, kr = *pk;
    *info = m < 0 ? -1 : n < 0 || n > m ? -2 : kr < 0 || kr > n ? -3 : 0;
    if (*info) return;
    if (*plwork == -1) { work_size(k, work, n > 0 ? n : 1); return; }
    if (!n) return;
    zc *A = load(k, a, *plda, m, n), *T = load(k, tau, kr ? kr : 1, kr ? kr : 1, 1), *Q = calloc((size_t)m * n, sizeof *Q), *v = malloc(sizeof(zc) * (size_t)m);
    if (!A || !T || !Q || !v) { free(A); free(T); free(Q); free(v); *info = -1000; return; }
    for (int i = 0; i < n; i++) E(Q, m, i, i) = Z(1, 0);
    for (int i = kr - 1; i >= 0; i--) {   /* Q = H(0) H(1) ... H(k-1) aplicado à identidade */
        v[0] = Z(1, 0);
        for (int r = 1; r < m - i; r++) v[r] = E(A, m, i + r, i);
        apply_h(T[i], v, &E(Q, m, i, i), m, m - i, n - i);
    }
    store(k, a, *plda, Q, m, n);
    free(A); free(T); free(Q); free(v);
}

/* ---------------------------------------------------------------- autovalores hermitianos (Jacobi cíclico) */
static void herm_eig(zc *A, int n, double *w, zc *V) {
    for (int i = 0; i < n * n; i++) V[i] = Z(0, 0);
    for (int i = 0; i < n; i++) E(V, n, i, i) = Z(1, 0);
    for (int sweep = 0; sweep < 100; sweep++) {
        double off = 0, tot = 0;
        for (int j = 0; j < n; j++) for (int i = 0; i < n; i++) { double a2 = zabs(E(A, n, i, j)); a2 *= a2; tot += a2; if (i != j) off += a2; }
        if (off <= EPS * EPS * tot || off == 0) break;
        for (int p = 0; p < n - 1; p++)
            for (int q = p + 1; q < n; q++) {
                zc apq = E(A, n, p, q);
                double g = zabs(apq);
                if (g == 0) continue;
                zc e = zsc(apq, 1 / g);
                double a = E(A, n, p, p).r, b = E(A, n, q, q).r, zeta = (b - a) / (2 * g);
                double t = (zeta >= 0 ? 1 : -1) / (fabs(zeta) + sqrt(1 + zeta * zeta)), c = 1 / sqrt(1 + t * t), s = t * c;
                /* J = diag(1, conj e) R: col_p' = c col_p - s conj(e) col_q; col_q' = s col_p + c conj(e) col_q */
                for (int r = 0; r < n; r++) {
                    zc xp = E(A, n, r, p), xq = E(A, n, r, q);
                    E(A, n, r, p) = zsub(zsc(xp, c), zmul(zsc(zcj(e), s), xq));
                    E(A, n, r, q) = zadd(zsc(xp, s), zmul(zsc(zcj(e), c), xq));
                    zc vp = E(V, n, r, p), vq = E(V, n, r, q);
                    E(V, n, r, p) = zsub(zsc(vp, c), zmul(zsc(zcj(e), s), vq));
                    E(V, n, r, q) = zadd(zsc(vp, s), zmul(zsc(zcj(e), c), vq));
                }
                for (int r = 0; r < n; r++) {   /* J^H pela esquerda */
                    zc xp = E(A, n, p, r), xq = E(A, n, q, r);
                    E(A, n, p, r) = zsub(zsc(xp, c), zmul(zsc(e, s), xq));
                    E(A, n, q, r) = zadd(zsc(xp, s), zmul(zsc(e, c), xq));
                }
                E(A, n, p, q) = E(A, n, q, p) = Z(0, 0);
                E(A, n, p, p).i = E(A, n, q, q).i = 0;
            }
    }
    for (int i = 0; i < n; i++) w[i] = E(A, n, i, i).r;
    for (int i = 1; i < n; i++)   /* ordem crescente, vetores juntos */
        for (int j = i; j > 0 && w[j - 1] > w[j]; j--) {
            double t = w[j]; w[j] = w[j - 1]; w[j - 1] = t;
            for (int r = 0; r < n; r++) { zc x = E(V, n, r, j); E(V, n, r, j) = E(V, n, r, j - 1); E(V, n, r, j - 1) = x; }
        }
}
static void heev(int k, const char *jobz, const char *uplo, const int *pn, void *a, const int *plda, void *w, int *info) {
    int n = *pn, U = up(uplo) == 'U', vec = up(jobz) == 'V';
    *info = 0;
    if (n <= 0) return;
    zc *A = load(k, a, *plda, n, n), *V = calloc((size_t)n * n, sizeof *V);
    double *ev = calloc((size_t)n, sizeof *ev);
    if (!A || !V || !ev) { free(A); free(V); free(ev); *info = -1000; return; }
    for (int j = 0; j < n; j++) for (int i = 0; i < n; i++)   /* completa o hermitiano com o triângulo dado */
        if (U ? i > j : i < j) E(A, n, i, j) = zcj(E(A, n, j, i));
    for (int i = 0; i < n; i++) E(A, n, i, i).i = 0;
    herm_eig(A, n, ev, V);
    store_real(k, w, ev, n);
    if (vec) store(k, a, *plda, V, n, n);
    free(A); free(V); free(ev);
}
static void syevd(int k, const char *jobz, const char *uplo, const int *pn, void *a, const int *plda, void *w, void *work, const int *plwork,
                  int *iwork, const int *pliwork, int *info) {
    int n = *pn;
    *info = 0;
    if (*plwork == -1 || *pliwork == -1) {
        work_size(k, work, up(jobz) == 'V' ? 1 + 6.0 * n + 2.0 * n * n : 2.0 * n + 1);
        if (iwork) iwork[0] = up(jobz) == 'V' ? 3 + 5 * n : 1;
        return;
    }
    heev(k, jobz, uplo, pn, a, plda, w, info);
}
static void heevd(int k, const char *jobz, const char *uplo, const int *pn, void *a, const int *plda, void *w, void *work, const int *plwork,
                  void *rwork, const int *plrwork, int *iwork, const int *pliwork, int *info) {
    int n = *pn;
    *info = 0;
    if (*plwork == -1 || *plrwork == -1 || *pliwork == -1) {
        work_size(k, work, up(jobz) == 'V' ? 2.0 * n + n * n : n + 1.0);
        double rw = up(jobz) == 'V' ? 1 + 5.0 * n + 2.0 * n * n : n;
        if (rwork) { if (k == K_C) ((float *)rwork)[0] = (float)rw; else ((double *)rwork)[0] = rw; }
        if (iwork) iwork[0] = up(jobz) == 'V' ? 3 + 5 * n : 1;
        return;
    }
    heev(k, jobz, uplo, pn, a, plda, w, info);
}

/* ---------------------------------------------------------------- SVD (Jacobi de um lado) */
/* A (m x n, m >= n) = U S V^H; U m x n, V n x n, s decrescente */
static void svd_tall(zc *A, int m, int n, double *s, zc *V) {
    for (int i = 0; i < n * n; i++) V[i] = Z(0, 0);
    for (int i = 0; i < n; i++) E(V, n, i, i) = Z(1, 0);
    for (int sweep = 0; sweep < 100; sweep++) {
        int rot = 0;
        for (int p = 0; p < n - 1; p++)
            for (int q = p + 1; q < n; q++) {
                double al = 0, be = 0;
                zc ga = Z(0, 0);
                for (int r = 0; r < m; r++) {
                    zc up_ = E(A, m, r, p), uq = E(A, m, r, q);
                    al += up_.r * up_.r + up_.i * up_.i; be += uq.r * uq.r + uq.i * uq.i;
                    ga = zadd(ga, zmul(zcj(up_), uq));
                }
                double g = zabs(ga);
                if (g == 0 || g <= EPS * sqrt(al * be)) continue;
                rot = 1;
                zc e = zsc(ga, 1 / g);
                double zeta = (be - al) / (2 * g), t = (zeta >= 0 ? 1 : -1) / (fabs(zeta) + sqrt(1 + zeta * zeta)), c = 1 / sqrt(1 + t * t), sn = t * c;
                for (int r = 0; r < m; r++) {
                    zc xp = E(A, m, r, p), xq = E(A, m, r, q);
                    E(A, m, r, p) = zsub(zsc(xp, c), zmul(zsc(zcj(e), sn), xq));
                    E(A, m, r, q) = zadd(zsc(xp, sn), zmul(zsc(zcj(e), c), xq));
                }
                for (int r = 0; r < n; r++) {
                    zc xp = E(V, n, r, p), xq = E(V, n, r, q);
                    E(V, n, r, p) = zsub(zsc(xp, c), zmul(zsc(zcj(e), sn), xq));
                    E(V, n, r, q) = zadd(zsc(xp, sn), zmul(zsc(zcj(e), c), xq));
                }
            }
        if (!rot) break;
    }
    for (int j = 0; j < n; j++) {
        double nn = 0;
        for (int r = 0; r < m; r++) { zc x = E(A, m, r, j); nn += x.r * x.r + x.i * x.i; }
        s[j] = sqrt(nn);
        if (s[j] > 0) for (int r = 0; r < m; r++) E(A, m, r, j) = zsc(E(A, m, r, j), 1 / s[j]);
    }
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && s[j - 1] < s[j]; j--) {
            double t = s[j]; s[j] = s[j - 1]; s[j - 1] = t;
            for (int r = 0; r < m; r++) { zc x = E(A, m, r, j); E(A, m, r, j) = E(A, m, r, j - 1); E(A, m, r, j - 1) = x; }
            for (int r = 0; r < n; r++) { zc x = E(V, n, r, j); E(V, n, r, j) = E(V, n, r, j - 1); E(V, n, r, j - 1) = x; }
        }
}
/* completa as colunas [from, cols) de Q (rows x cols) até uma base ortonormal (Gram-Schmidt com os e_i) */
static void complete_basis(zc *Q, int rows, int from, int cols) {
    int cand = 0;
    for (int j = from; j < cols; j++) {
        for (;;) {
            zc *c = &E(Q, rows, 0, j);
            if (cand < rows) { for (int r = 0; r < rows; r++) c[r] = Z(r == cand, 0); cand++; }
            else return;
            for (int pass = 0; pass < 2; pass++)
                for (int q = 0; q < j; q++) {
                    zc d = Z(0, 0);
                    for (int r = 0; r < rows; r++) d = zadd(d, zmul(zcj(E(Q, rows, r, q)), c[r]));
                    for (int r = 0; r < rows; r++) c[r] = zsub(c[r], zmul(d, E(Q, rows, r, q)));
                }
            double nn = 0;
            for (int r = 0; r < rows; r++) nn += c[r].r * c[r].r + c[r].i * c[r].i;
            if (nn > 1e-20) { nn = 1 / sqrt(nn); for (int r = 0; r < rows; r++) c[r] = zsc(c[r], nn); break; }
        }
    }
}
/* SVD completa: U (m x m), s (min), V (n x n), A = U S V^H */
static int svd(const zc *A0, int m, int n, zc **pU, double *s, zc **pV) {
    int mn = m < n ? m : n, tall = m >= n;
    int R = tall ? m : n, C = tall ? n : m;   /* trabalha em B = A (alta) ou A^H (larga) */
    zc *B = malloc(sizeof(zc) * (size_t)R * C), *W = malloc(sizeof(zc) * (size_t)C * C);
    zc *U = calloc((size_t)m * m, sizeof(zc)), *V = calloc((size_t)n * n, sizeof(zc));
    if (!B || !W || !U || !V) { free(B); free(W); free(U); free(V); return -1; }
    for (int j = 0; j < C; j++) for (int i = 0; i < R; i++) E(B, R, i, j) = tall ? E(A0, m, i, j) : zcj(E(A0, m, j, i));
    svd_tall(B, R, C, s, W);
    /* B = P S W^H. Alta: U = P, V = W. Larga: A = W S P^H, então U = W, V = P */
    zc *Lft = tall ? U : V, *Rgt = tall ? V : U;
    int lr = tall ? m : n, rr = tall ? n : m;
    int good = 0;
    for (int j = 0; j < C; j++) if (s[j] > 0) good = j + 1;
    for (int j = 0; j < good; j++) for (int i = 0; i < R; i++) E(Lft, lr, i, j) = E(B, R, i, j);
    complete_basis(Lft, lr, good, lr);
    for (int j = 0; j < C; j++) for (int i = 0; i < C; i++) E(Rgt, rr, i, j) = E(W, C, i, j);
    (void)mn;
    free(B); free(W);
    *pU = U; *pV = V;
    return 0;
}
static void gesdd(int k, const char *jobz, const int *pm, const int *pn, void *a, const int *plda, void *s, void *u, const int *pldu,
                  void *vt, const int *pldvt, void *work, const int *plwork, int *info) {
    int m = *pm, n = *pn, mn = m < n ? m : n, mx = m > n ? m : n, J = up(jobz);
    *info = 0;
    if (*plwork == -1) { work_size(k, work, 4.0 * mn * mn + 7.0 * mn + mx + 1); return; }
    if (!mn) return;
    zc *A = load(k, a, *plda, m, n), *U = 0, *V = 0;
    double *sv = calloc((size_t)mn, sizeof *sv);
    if (!A || !sv || svd(A, m, n, &U, sv, &V)) { free(A); free(sv); *info = -1000; return; }
    store_real(k, s, sv, mn);
    if (J != 'N') {
        zc *VT = calloc((size_t)n * n, sizeof *VT);
        for (int i = 0; i < n && VT; i++) for (int j = 0; j < n; j++) E(VT, n, i, j) = zcj(E(V, n, j, i));
        if (J == 'A') { store(k, u, *pldu, U, m, m); store(k, vt, *pldvt, VT, n, n); }
        else if (J == 'S') {
            store(k, u, *pldu, U, m, mn);
            zc *T = calloc((size_t)mn * n, sizeof *T);
            for (int i = 0; i < mn && T; i++) for (int j = 0; j < n; j++) E(T, mn, i, j) = E(VT, n, i, j);
            if (T) store(k, vt, *pldvt, T, mn, n);
            free(T);
        } else if (J == 'O') {   /* m >= n: U em A e VT inteiro; m < n: VT em A e U inteiro */
            if (m >= n) { store(k, a, *plda, U, m, n); store(k, vt, *pldvt, VT, n, n); }
            else {
                zc *T = calloc((size_t)m * n, sizeof *T);
                for (int i = 0; i < m && T; i++) for (int j = 0; j < n; j++) E(T, m, i, j) = E(VT, n, i, j);
                if (T) store(k, a, *plda, T, m, n);
                free(T);
                store(k, u, *pldu, U, m, m);
            }
        }
        free(VT);
    }
    free(A); free(U); free(V); free(sv);
}
static void gelsd(int k, const int *pm, const int *pn, const int *pnrhs, void *a, const int *plda, void *b, const int *pldb, void *s,
                  double rcond, int *rank, void *work, const int *plwork, int *info) {
    int m = *pm, n = *pn, nrhs = *pnrhs, mn = m < n ? m : n, mx = m > n ? m : n;
    *info = 0;
    if (*plwork == -1) { work_size(k, work, 12.0 * mx + 2.0 * mx * nrhs + 4.0 * mx * mx + 1); return; }
    if (!mn || !nrhs) { if (rank) *rank = 0; return; }
    zc *A = load(k, a, *plda, m, n), *B = load(k, b, *pldb, mx, nrhs), *U = 0, *V = 0;
    double *sv = calloc((size_t)mn, sizeof *sv);
    if (!A || !B || !sv || svd(A, m, n, &U, sv, &V)) { free(A); free(B); free(sv); *info = -1000; return; }
    double thr = (rcond < 0 ? EPS : rcond) * sv[0];
    int r = 0;
    for (int i = 0; i < mn; i++) if (sv[i] > thr) r++;
    zc *X = calloc((size_t)mx * nrhs, sizeof *X), *t = calloc((size_t)(r ? r : 1), sizeof *t);
    for (int c = 0; c < nrhs && X && t; c++) {   /* x = V S^+ U^H b */
        for (int i = 0; i < r; i++) {
            zc d = Z(0, 0);
            for (int q = 0; q < m; q++) d = zadd(d, zmul(zcj(E(U, m, q, i)), E(B, mx, q, c)));
            t[i] = zsc(d, 1 / sv[i]);
        }
        for (int q = 0; q < n; q++) {
            zc x = Z(0, 0);
            for (int i = 0; i < r; i++) x = zadd(x, zmul(E(V, n, q, i), t[i]));
            E(X, mx, q, c) = x;
        }
    }
    if (X) store(k, b, *pldb, X, n, nrhs);
    store_real(k, s, sv, mn);
    if (rank) *rank = r;
    free(A); free(B); free(U); free(V); free(sv); free(X); free(t);
}

/* ---------------------------------------------------------------- autovalores gerais (Schur complexo) */
static int schur(zc *H, int n, zc *Q) {
    for (int i = 0; i < n * n; i++) Q[i] = Z(0, 0);
    for (int i = 0; i < n; i++) E(Q, n, i, i) = Z(1, 0);
    zc *v = malloc(sizeof(zc) * (size_t)n);
    if (!v) return -1;
    for (int c = 0; c + 2 < n; c++) {   /* Hessenberg por Householder: H <- P^H H P, Q <- Q P */
        zc tau = larfg(&E(H, n, c + 1, c), &E(H, n, c + 2, c), n - c - 2, 1);
        v[0] = Z(1, 0);
        for (int r = 1; r < n - c - 1; r++) v[r] = E(H, n, c + 1 + r, c);
        for (int r = c + 2; r < n; r++) E(H, n, r, c) = Z(0, 0);
        apply_h(zcj(tau), v, &E(H, n, c + 1, c + 1), n, n - c - 1, n - c - 1);
        for (int r = 0; r < n; r++) {   /* pela direita: linhas de H e de Q */
            for (int pass = 0; pass < 2; pass++) {
                zc *M = pass ? Q : H;
                int c0 = c + 1;
                zc w = E(M, n, r, c0);
                for (int i = 1; i < n - c0; i++) w = zadd(w, zmul(E(M, n, r, c0 + i), v[i]));
                w = zmul(w, tau);
                E(M, n, r, c0) = zsub(E(M, n, r, c0), w);
                for (int i = 1; i < n - c0; i++) E(M, n, r, c0 + i) = zsub(E(M, n, r, c0 + i), zmul(w, zcj(v[i])));
            }
        }
    }
    free(v);
    int hi = n - 1, iter = 0;
    while (hi > 0) {   /* QR com deslocamento de Wilkinson e rotações de Givens */
        int lo = hi;
        while (lo > 0) {
            double s = abs1(E(H, n, lo - 1, lo - 1)) + abs1(E(H, n, lo, lo));
            if (abs1(E(H, n, lo, lo - 1)) <= EPS * (s == 0 ? 1 : s)) { E(H, n, lo, lo - 1) = Z(0, 0); break; }
            lo--;
        }
        if (lo == hi) { hi--; iter = 0; continue; }
        if (++iter > 300) return -1;
        zc a = E(H, n, hi - 1, hi - 1), b = E(H, n, hi - 1, hi), c = E(H, n, hi, hi - 1), d = E(H, n, hi, hi);
        zc tr = zadd(a, d), det = zsub(zmul(a, d), zmul(b, c));
        zc disc = zsub(zmul(tr, tr), zsc(det, 4));
        double dr = zabs(disc), ang = disc.r == 0 && disc.i == 0 ? 0 : 0.5 * __builtin_atan2(disc.i, disc.r);
        zc sq = Z(sqrt(dr) * __builtin_cos(ang), sqrt(dr) * __builtin_sin(ang));
        zc l1 = zsc(zadd(tr, sq), 0.5), l2 = zsc(zsub(tr, sq), 0.5);
        zc mu = zabs(zsub(l1, d)) < zabs(zsub(l2, d)) ? l1 : l2;
        if (iter % 11 == 0) mu = zadd(mu, Z(abs1(E(H, n, hi, hi - 1)), 0));   /* deslocamento excepcional */
        zc x = zsub(E(H, n, lo, lo), mu), y = E(H, n, lo + 1, lo);
        for (int kk = lo; kk < hi; kk++) {
            if (kk > lo) { x = E(H, n, kk, kk - 1); y = E(H, n, kk + 1, kk - 1); }
            double r = sqrt(x.r * x.r + x.i * x.i + y.r * y.r + y.i * y.i);
            if (r == 0) continue;
            zc cc = zsc(x, 1 / r), ss = zsc(y, 1 / r);   /* G = [conj c, conj s; -s, c] zera y */
            for (int j = (kk > lo ? kk - 1 : lo); j < n; j++) {
                zc p = E(H, n, kk, j), q = E(H, n, kk + 1, j);
                E(H, n, kk, j) = zadd(zmul(zcj(cc), p), zmul(zcj(ss), q));
                E(H, n, kk + 1, j) = zsub(zmul(cc, q), zmul(ss, p));
            }
            int rmax = kk + 2 < hi ? kk + 2 : hi;
            for (int i = 0; i <= rmax; i++) {
                zc p = E(H, n, i, kk), q = E(H, n, i, kk + 1);
                E(H, n, i, kk) = zadd(zmul(p, cc), zmul(q, ss));
                E(H, n, i, kk + 1) = zsub(zmul(q, zcj(cc)), zmul(p, zcj(ss)));
            }
            for (int i = 0; i < n; i++) {
                zc p = E(Q, n, i, kk), q = E(Q, n, i, kk + 1);
                E(Q, n, i, kk) = zadd(zmul(p, cc), zmul(q, ss));
                E(Q, n, i, kk + 1) = zsub(zmul(q, zcj(cc)), zmul(p, zcj(ss)));
            }
        }
    }
    return 0;
}
/* vetores de T triangular: direita (T x = t_kk x) e esquerda (y^H T = t_kk y^H), em X e Y (colunas) */
static void tri_vectors(const zc *T, int n, zc *X, zc *Y, double nrm) {
    double small = EPS * (nrm > 0 ? nrm : 1);
    for (int kq = 0; kq < n; kq++) {
        zc lam = E(T, n, kq, kq);
        if (X) {
            for (int i = 0; i < n; i++) E(X, n, i, kq) = Z(i == kq, 0);
            for (int i = kq - 1; i >= 0; i--) {
                zc s = Z(0, 0);
                for (int j = i + 1; j <= kq; j++) s = zadd(s, zmul(E(T, n, i, j), E(X, n, j, kq)));
                zc d = zsub(E(T, n, i, i), lam);
                if (zabs(d) < small) d = Z(small, 0);
                E(X, n, i, kq) = zdiv(Z(-s.r, -s.i), d);
            }
        }
        if (Y) {
            for (int i = 0; i < n; i++) E(Y, n, i, kq) = Z(i == kq, 0);
            for (int i = kq + 1; i < n; i++) {
                zc s = Z(0, 0);
                for (int j = kq; j < i; j++) s = zadd(s, zmul(zcj(E(T, n, j, i)), E(Y, n, j, kq)));
                zc d = zsub(zcj(E(T, n, i, i)), zcj(lam));
                if (zabs(d) < small) d = Z(small, 0);
                E(Y, n, i, kq) = zdiv(Z(-s.r, -s.i), d);
            }
        }
    }
}
/* M <- Q M e normaliza cada coluna (norma 1, maior componente real), como o xGEEV */
static void back_normalize(const zc *Q, zc *M, int n) {
    zc *t = malloc(sizeof(zc) * (size_t)n);
    for (int c = 0; c < n && t; c++) {
        for (int i = 0; i < n; i++) { zc s = Z(0, 0); for (int j = 0; j < n; j++) s = zadd(s, zmul(E(Q, n, i, j), E(M, n, j, c))); t[i] = s; }
        double nn = 0; int big = 0;
        for (int i = 0; i < n; i++) { double a2 = t[i].r * t[i].r + t[i].i * t[i].i; nn += a2; if (a2 > t[big].r * t[big].r + t[big].i * t[big].i) big = i; }
        nn = sqrt(nn);
        zc ph = zabs(t[big]) > 0 ? zsc(zcj(t[big]), 1 / zabs(t[big])) : Z(1, 0);
        for (int i = 0; i < n; i++) E(M, n, i, c) = nn > 0 ? zsc(zmul(t[i], ph), 1 / nn) : t[i];
        E(M, n, big, c).i = 0;
    }
    free(t);
}
static int eig_general(const zc *A0, int n, zc *w, zc *VL, zc *VR) {
    zc *H = malloc(sizeof(zc) * (size_t)n * n), *Q = malloc(sizeof(zc) * (size_t)n * n);
    if (!H || !Q) { free(H); free(Q); return -1; }
    memcpy(H, A0, sizeof(zc) * (size_t)n * n);
    double nrm = 0;
    for (int i = 0; i < n * n; i++) nrm += abs1(A0[i]);
    int rc = schur(H, n, Q);
    if (!rc) {
        for (int i = 0; i < n; i++) w[i] = E(H, n, i, i);
        tri_vectors(H, n, VR, VL, nrm);
        if (VR) back_normalize(Q, VR, n);
        if (VL) back_normalize(Q, VL, n);
    }
    free(H); free(Q);
    return rc;
}
static void geev_c(int k, const char *jl, const char *jr, const int *pn, void *a, const int *plda, void *w, void *vl, const int *pldvl,
                   void *vr, const int *pldvr, void *work, const int *plwork, int *info) {
    int n = *pn, L = up(jl) == 'V', Rr = up(jr) == 'V';
    *info = 0;
    if (*plwork == -1) { work_size(k, work, 4.0 * n + 1); return; }
    if (n <= 0) return;
    zc *A = load(k, a, *plda, n, n), *W = calloc((size_t)n, sizeof *W), *VL = L ? calloc((size_t)n * n, sizeof(zc)) : 0, *VR = Rr ? calloc((size_t)n * n, sizeof(zc)) : 0;
    if (!A || !W || (L && !VL) || (Rr && !VR) || eig_general(A, n, W, VL, VR)) { *info = n; free(A); free(W); free(VL); free(VR); return; }
    store(k, w, n, W, n, 1);
    if (L) store(k, vl, *pldvl, VL, n, n);
    if (Rr) store(k, vr, *pldvr, VR, n, n);
    free(A); free(W); free(VL); free(VR);
}
/* real: pares conjugados juntos (parte imaginária positiva primeiro); vetor do par = (Re, Im) em duas colunas */
static void geev_r(int k, const char *jl, const char *jr, const int *pn, void *a, const int *plda, void *wr, void *wi, void *vl, const int *pldvl,
                   void *vr, const int *pldvr, void *work, const int *plwork, int *info) {
    int n = *pn, L = up(jl) == 'V', Rr = up(jr) == 'V';
    *info = 0;
    if (*plwork == -1) { work_size(k, work, (L || Rr ? 4.0 : 3.0) * n + 1); return; }
    if (n <= 0) return;
    zc *A = load(k, a, *plda, n, n), *W = calloc((size_t)n, sizeof *W), *VL = calloc((size_t)n * n, sizeof(zc)), *VR = calloc((size_t)n * n, sizeof(zc));
    int *ord = calloc((size_t)n, sizeof *ord), *used = calloc((size_t)n, sizeof *used);
    double *re = calloc((size_t)n, sizeof *re), *im = calloc((size_t)n, sizeof *im);
    double *OL = calloc((size_t)n * n, sizeof *OL), *OR = calloc((size_t)n * n, sizeof *OR);
    if (!A || !W || !VL || !VR || !ord || !used || !re || !im || !OL || !OR || eig_general(A, n, W, L ? VL : 0, Rr ? VR : 0)) {
        *info = n; goto out;
    }
    double nrm = 0;
    for (int i = 0; i < n * n; i++) nrm += abs1(A[i]);
    double tol = 1e3 * EPS * (nrm > 1 ? nrm : 1);
    int o = 0;
    for (int i = 0; i < n; i++) {
        if (used[i]) continue;
        used[i] = 1;
        if (fabs(W[i].i) <= tol) {   /* real: vetor real (a fase já deixou a maior componente real) */
            re[o] = W[i].r; im[o] = 0;
            for (int r = 0; r < n; r++) { OR[r + (size_t)o * n] = E(VR, n, r, i).r; OL[r + (size_t)o * n] = E(VL, n, r, i).r; }
            o++;
            continue;
        }
        int best = -1;   /* o conjugado mais perto */
        for (int j = 0; j < n; j++) if (!used[j] && (best < 0 || zabs(zsub(W[j], zcj(W[i]))) < zabs(zsub(W[best], zcj(W[i]))))) best = j;
        if (best >= 0) used[best] = 1;
        int pos = W[i].i > 0 || best < 0 ? i : best;   /* o de parte imaginária positiva */
        double mr = best >= 0 ? (W[i].r + W[best].r) / 2 : W[i].r, mi = best >= 0 ? (fabs(W[i].i) + fabs(W[best].i)) / 2 : fabs(W[i].i);
        re[o] = mr; im[o] = mi; re[o + 1] = mr; im[o + 1] = -mi;
        for (int r = 0; r < n; r++) {
            zc x = E(VR, n, r, pos), y = E(VL, n, r, pos);
            OR[r + (size_t)o * n] = x.r; OR[r + (size_t)(o + 1) * n] = x.i;
            OL[r + (size_t)o * n] = y.r; OL[r + (size_t)(o + 1) * n] = y.i;
        }
        o += 2;
    }
    store_real(k, wr, re, n);
    store_real(k, wi, im, n);
    for (int j = 0; j < n; j++)
        for (int i = 0; i < n; i++) {
            if (Rr) { size_t q = (size_t)i + (size_t)j * *pldvr; if (k == K_S) ((float *)vr)[q] = (float)OR[i + (size_t)j * n]; else ((double *)vr)[q] = OR[i + (size_t)j * n]; }
            if (L) { size_t q = (size_t)i + (size_t)j * *pldvl; if (k == K_S) ((float *)vl)[q] = (float)OL[i + (size_t)j * n]; else ((double *)vl)[q] = OL[i + (size_t)j * n]; }
        }
out:
    free(A); free(W); free(VL); free(VR); free(ord); free(used); free(re); free(im); free(OL); free(OR);
}

/* ---------------------------------------------------------------- pontos de entrada (x_ e x$NEWLAPACK) */
typedef int I;
/* x$NEWLAPACK: no ELF um alias da função; no Mach-O (libSystem arm64, que não aceita alias) um salto em assembly com o
 * mesmo nome */
#if defined(__MACH__)
# if defined(__aarch64__)
/* prefixo dos nomes C: "_" no Mach-O, vazio no ELF do libSystem arm64 (triplo arm64-apple-macos11-elf, também __MACH__) */
#  define MOS_UP_S2(x) #x
#  define MOS_UP_S(x) MOS_UP_S2(x)
#  define MOS_UP MOS_UP_S(__USER_LABEL_PREFIX__)
#  define MOS_TRAMP(n, t) __asm__(".text\n.globl \"" MOS_UP n "\"\n.p2align 2\n\"" MOS_UP n "\":\n\tb " MOS_UP t "\n");
# else
#  define MOS_TRAMP(n, t) __asm__(".text\n.globl \"_" n "\"\n\"_" n "\":\n\tjmp _" t "\n");
# endif
# define ALIAS(f) MOS_TRAMP(#f "$NEWLAPACK", #f "_")
#else
#define ALIAS(f) extern __typeof(f##_) f##_nl __asm__(#f "$NEWLAPACK") __attribute__((alias(#f "_")));
#endif
#define FOR4(M) M(s, K_S, float, float) M(d, K_D, double, double) M(c, K_C, cf, float) M(z, K_Z, zc, double)
#define DEF_GETRF(p, K, T, R) void p##getrf_(const I *m, const I *n, T *a, const I *lda, I *ipiv, I *info) { getrf(K, m, n, a, lda, ipiv, info); } ALIAS(p##getrf)
#define DEF_GETRS(p, K, T, R) void p##getrs_(const char *t, const I *n, const I *nr, const T *a, const I *lda, const I *ipiv, T *b, const I *ldb, I *info) { getrs(K, t, n, nr, a, lda, ipiv, b, ldb, info); } ALIAS(p##getrs)
#define DEF_GETRI(p, K, T, R) void p##getri_(const I *n, T *a, const I *lda, const I *ipiv, T *w, const I *lw, I *info) { getri(K, n, a, lda, ipiv, w, lw, info); } ALIAS(p##getri)
#define DEF_GESV(p, K, T, R) void p##gesv_(const I *n, const I *nr, T *a, const I *lda, I *ipiv, T *b, const I *ldb, I *info) { gesv(K, n, nr, a, lda, ipiv, b, ldb, info); } ALIAS(p##gesv)
#define DEF_POTRF(p, K, T, R) void p##potrf_(const char *u, const I *n, T *a, const I *lda, I *info) { potrf(K, u, n, a, lda, info); } ALIAS(p##potrf)
#define DEF_TRTRI(p, K, T, R) void p##trtri_(const char *u, const char *d, const I *n, T *a, const I *lda, I *info) { trtri(K, u, d, n, a, lda, info); } ALIAS(p##trtri)
#define DEF_GEQRF(p, K, T, R) void p##geqrf_(const I *m, const I *n, T *a, const I *lda, T *tau, T *w, const I *lw, I *info) { geqrf(K, m, n, a, lda, tau, w, lw, info); } ALIAS(p##geqrf)
#define DEF_GESDD_R(p, K, T, R) void p##gesdd_(const char *j, const I *m, const I *n, T *a, const I *lda, R *s, T *u, const I *ldu, T *vt, const I *ldvt, T *w, const I *lw, I *iw, I *info) { (void)iw; gesdd(K, j, m, n, a, lda, s, u, ldu, vt, ldvt, w, lw, info); } ALIAS(p##gesdd)
#define DEF_GESDD_C(p, K, T, R) void p##gesdd_(const char *j, const I *m, const I *n, T *a, const I *lda, R *s, T *u, const I *ldu, T *vt, const I *ldvt, T *w, const I *lw, R *rw, I *iw, I *info) { (void)rw; (void)iw; gesdd(K, j, m, n, a, lda, s, u, ldu, vt, ldvt, w, lw, info); } ALIAS(p##gesdd)
#define DEF_GELSD_R(p, K, T, R) void p##gelsd_(const I *m, const I *n, const I *nr, T *a, const I *lda, T *b, const I *ldb, R *s, const R *rc, I *rank, T *w, const I *lw, I *iw, I *info) { (void)iw; gelsd(K, m, n, nr, a, lda, b, ldb, s, *rc, rank, w, lw, info); } ALIAS(p##gelsd)
#define DEF_GELSD_C(p, K, T, R) void p##gelsd_(const I *m, const I *n, const I *nr, T *a, const I *lda, T *b, const I *ldb, R *s, const R *rc, I *rank, T *w, const I *lw, R *rw, I *iw, I *info) { (void)rw; (void)iw; gelsd(K, m, n, nr, a, lda, b, ldb, s, *rc, rank, w, lw, info); } ALIAS(p##gelsd)
#define DEF_GEEV_R(p, K, T, R) void p##geev_(const char *jl, const char *jr, const I *n, T *a, const I *lda, T *wr, T *wi, T *vl, const I *ldvl, T *vr, const I *ldvr, T *w, const I *lw, I *info) { geev_r(K, jl, jr, n, a, lda, wr, wi, vl, ldvl, vr, ldvr, w, lw, info); } ALIAS(p##geev)
#define DEF_GEEV_C(p, K, T, R) void p##geev_(const char *jl, const char *jr, const I *n, T *a, const I *lda, T *wv, T *vl, const I *ldvl, T *vr, const I *ldvr, T *w, const I *lw, R *rw, I *info) { (void)rw; geev_c(K, jl, jr, n, a, lda, wv, vl, ldvl, vr, ldvr, w, lw, info); } ALIAS(p##geev)
FOR4(DEF_GETRF) FOR4(DEF_GETRS) FOR4(DEF_GETRI) FOR4(DEF_GESV) FOR4(DEF_POTRF) FOR4(DEF_TRTRI) FOR4(DEF_GEQRF)
DEF_GESDD_R(s, K_S, float, float) DEF_GESDD_R(d, K_D, double, double) DEF_GESDD_C(c, K_C, cf, float) DEF_GESDD_C(z, K_Z, zc, double)
DEF_GELSD_R(s, K_S, float, float) DEF_GELSD_R(d, K_D, double, double) DEF_GELSD_C(c, K_C, cf, float) DEF_GELSD_C(z, K_Z, zc, double)
DEF_GEEV_R(s, K_S, float, float) DEF_GEEV_R(d, K_D, double, double) DEF_GEEV_C(c, K_C, cf, float) DEF_GEEV_C(z, K_Z, zc, double)
void sorgqr_(const I *m, const I *n, const I *k, float *a, const I *lda, const float *tau, float *w, const I *lw, I *info) { orgqr(K_S, m, n, k, a, lda, tau, w, lw, info); } ALIAS(sorgqr)
void dorgqr_(const I *m, const I *n, const I *k, double *a, const I *lda, const double *tau, double *w, const I *lw, I *info) { orgqr(K_D, m, n, k, a, lda, tau, w, lw, info); } ALIAS(dorgqr)
void cungqr_(const I *m, const I *n, const I *k, cf *a, const I *lda, const cf *tau, cf *w, const I *lw, I *info) { orgqr(K_C, m, n, k, a, lda, tau, w, lw, info); } ALIAS(cungqr)
void zungqr_(const I *m, const I *n, const I *k, zc *a, const I *lda, const zc *tau, zc *w, const I *lw, I *info) { orgqr(K_Z, m, n, k, a, lda, tau, w, lw, info); } ALIAS(zungqr)
void ssyevd_(const char *j, const char *u, const I *n, float *a, const I *lda, float *w, float *wk, const I *lw, I *iw, const I *liw, I *info) { syevd(K_S, j, u, n, a, lda, w, wk, lw, iw, liw, info); } ALIAS(ssyevd)
void dsyevd_(const char *j, const char *u, const I *n, double *a, const I *lda, double *w, double *wk, const I *lw, I *iw, const I *liw, I *info) { syevd(K_D, j, u, n, a, lda, w, wk, lw, iw, liw, info); } ALIAS(dsyevd)
void cheevd_(const char *j, const char *u, const I *n, cf *a, const I *lda, float *w, cf *wk, const I *lw, float *rw, const I *lrw, I *iw, const I *liw, I *info) { heevd(K_C, j, u, n, a, lda, w, wk, lw, rw, lrw, iw, liw, info); } ALIAS(cheevd)
void zheevd_(const char *j, const char *u, const I *n, zc *a, const I *lda, double *w, zc *wk, const I *lw, double *rw, const I *lrw, I *iw, const I *liw, I *info) { heevd(K_Z, j, u, n, a, lda, w, wk, lw, rw, lrw, iw, liw, info); } ALIAS(zheevd)
