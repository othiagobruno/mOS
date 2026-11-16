/* Host: userland/libsystem/accelerate_lapack.c pelas propriedades (reconstrução, ortonormalidade, A v = lambda v).
 * gcc -O1 tools/accelerate_lapack_test.c userland/libsystem/accelerate_lapack.c -lm → "accelerate_lapack: N/N ok" */
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef double complex zx;
void dgesv_(const int *, const int *, double *, const int *, int *, double *, const int *, int *);
void dgetrf_(const int *, const int *, double *, const int *, int *, int *);
void dgetri_(const int *, double *, const int *, const int *, double *, const int *, int *);
void dpotrf_(const char *, const int *, double *, const int *, int *);
void dtrtri_(const char *, const char *, const int *, double *, const int *, int *);
void dgeqrf_(const int *, const int *, double *, const int *, double *, double *, const int *, int *);
void dorgqr_(const int *, const int *, const int *, double *, const int *, const double *, double *, const int *, int *);
void zgeqrf_(const int *, const int *, zx *, const int *, zx *, zx *, const int *, int *);
void zungqr_(const int *, const int *, const int *, zx *, const int *, const zx *, zx *, const int *, int *);
void dsyevd_(const char *, const char *, const int *, double *, const int *, double *, double *, const int *, int *, const int *, int *);
void zheevd_(const char *, const char *, const int *, zx *, const int *, double *, zx *, const int *, double *, const int *, int *, const int *, int *);
void dgesdd_(const char *, const int *, const int *, double *, const int *, double *, double *, const int *, double *, const int *, double *, const int *, int *, int *);
void zgesdd_(const char *, const int *, const int *, zx *, const int *, double *, zx *, const int *, zx *, const int *, zx *, const int *, double *, int *, int *);
void dgelsd_(const int *, const int *, const int *, double *, const int *, double *, const int *, double *, const double *, int *, double *, const int *, int *, int *);
void dgeev_(const char *, const char *, const int *, double *, const int *, double *, double *, double *, const int *, double *, const int *, double *, const int *, int *);
void zgeev_(const char *, const char *, const int *, zx *, const int *, zx *, zx *, const int *, zx *, const int *, zx *, const int *, double *, int *);
void sgesv_(const int *, const int *, float *, const int *, int *, float *, const int *, int *);
void cpotrf_(const char *, const int *, float complex *, const int *, int *);
extern void dgesv_nl(void) __asm__("dgesv$NEWLAPACK");

static int fails, total;
static void expect(int c, const char *m) { total++; if (!c) { printf("FALHOU: %s\n", m); fails++; } }
static double rnd(void) { return (rand() % 20001 - 10000) / 10000.0; }
#define A_(M, ld, i, j) (M)[(i) + (j) * (ld)]

int main(void) {
    srand(11);
    int n = 5, one = 1, info, ipiv[8], lw = -1;
    double A[25], B[25], b[5], x[5], wq;
    for (int i = 0; i < 25; i++) A[i] = rnd();
    for (int i = 0; i < 5; i++) b[i] = rnd();
    memcpy(B, A, sizeof A); memcpy(x, b, sizeof b);
    dgesv_(&n, &one, B, &n, ipiv, x, &n, &info);
    double res = 0; for (int i = 0; i < 5; i++) { double s = 0; for (int j = 0; j < 5; j++) s += A_(A, 5, i, j) * x[j]; res = fmax(res, fabs(s - b[i])); }
    expect(!info && res < 1e-12, "dgesv: A x = b");
    memcpy(B, A, sizeof A); memcpy(x, b, sizeof b);
    ((void (*)(const int *, const int *, double *, const int *, int *, double *, const int *, int *))dgesv_nl)(&n, &one, B, &n, ipiv, x, &n, &info);
    expect(!info, "dgesv$NEWLAPACK");
    memcpy(B, A, sizeof A);
    dgetrf_(&n, &n, B, &n, ipiv, &info);
    dgetri_(&n, B, &n, ipiv, &wq, &lw, &info); expect(wq >= 5, "dgetri consulta de espaço");
    double wk[64]; int l64 = 64;
    dgetri_(&n, B, &n, ipiv, wk, &l64, &info);
    double e = 0; for (int i = 0; i < 5; i++) for (int j = 0; j < 5; j++) { double s = 0; for (int k = 0; k < 5; k++) s += A_(A, 5, i, k) * A_(B, 5, k, j); e = fmax(e, fabs(s - (i == j))); }
    expect(!info && e < 1e-11, "dgetrf + dgetri: A A^-1 = I");
    double S[25], L[25];
    for (int i = 0; i < 5; i++) for (int j = 0; j < 5; j++) { double s = 0; for (int k = 0; k < 5; k++) s += A_(A, 5, k, i) * A_(A, 5, k, j); A_(S, 5, i, j) = s + (i == j); }
    for (int up = 0; up < 2; up++) {
        memcpy(L, S, sizeof S);
        dpotrf_(up ? "U" : "L", &n, L, &n, &info);
        e = 0;
        for (int i = 0; i < 5; i++) for (int j = 0; j < 5; j++) {
            double s = 0;
            for (int k = 0; k < 5; k++) s += up ? (k <= i && k <= j ? A_(L, 5, k, i) * A_(L, 5, k, j) : 0) : (k <= i && k <= j ? A_(L, 5, i, k) * A_(L, 5, j, k) : 0);
            e = fmax(e, fabs(s - A_(S, 5, i, j)));
        }
        expect(!info && e < 1e-12, up ? "dpotrf U: U^T U = A" : "dpotrf L: L L^T = A");
    }
    double neg[4] = {1, 2, 2, 1}; int two = 2; dpotrf_("L", &two, neg, &two, &info); expect(info == 2, "dpotrf: não positiva definida dá info");
    float sf[4] = {2, 1, 1, 3}, bf[2] = {3, 5}; int ip2[2]; sgesv_(&two, &one, sf, &two, ip2, bf, &two, &info);
    expect(!info && fabsf(bf[0] - 0.8f) < 1e-6 && fabsf(bf[1] - 1.4f) < 1e-6, "sgesv");
    float complex hc[4] = {4, 1 - 2 * I, 1 + 2 * I, 6}, hl[4]; memcpy(hl, hc, sizeof hc);
    cpotrf_("L", &two, hl, &two, &info);
    expect(!info && cabsf(hl[0] * conjf(hl[0]) - 4) < 1e-5 && cabsf(hl[1] * conjf(hl[0]) - hc[1]) < 1e-5, "cpotrf hermitiano");
    /* trtri */
    double T[9] = {2, 0, 0, 1, 3, 0, -1, 4, 5}, Ti[9]; int n3 = 3;
    memcpy(Ti, T, sizeof T); dtrtri_("U", "N", &n3, Ti, &n3, &info);
    e = 0; for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) { double s = 0; for (int k = 0; k < 3; k++) s += A_(T, 3, i, k) * (k <= j ? A_(Ti, 3, k, j) : 0); e = fmax(e, fabs(s - (i == j))); }
    expect(!info && e < 1e-14, "dtrtri U N");
    double Lw[9] = {9, 2, 3, 0, 9, -1, 0, 0, 9}, Li[9]; memcpy(Li, Lw, sizeof Lw); dtrtri_("L", "U", &n3, Li, &n3, &info);
    expect(!info && Li[0] == 9 && fabs(A_(Li, 3, 1, 0) + 2) < 1e-15 && fabs(A_(Li, 3, 2, 0) - (-3 + 2 * -1)) < 1e-14, "dtrtri L U (diagonal unitária)");
    /* QR */
    int m = 6, nq = 4, lw64 = 64;
    double Q[24], Q0[24], tau[4];
    for (int i = 0; i < 24; i++) Q0[i] = rnd();
    memcpy(Q, Q0, sizeof Q);
    dgeqrf_(&m, &nq, Q, &m, tau, wk, &lw64, &info);
    double R[16] = {0}; for (int j = 0; j < 4; j++) for (int i = 0; i <= j; i++) A_(R, 4, i, j) = A_(Q, 6, i, j);
    dorgqr_(&m, &nq, &nq, Q, &m, tau, wk, &lw64, &info);
    double eo = 0, er = 0;
    for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) { double s = 0; for (int k = 0; k < 6; k++) s += A_(Q, 6, k, i) * A_(Q, 6, k, j); eo = fmax(eo, fabs(s - (i == j))); }
    for (int i = 0; i < 6; i++) for (int j = 0; j < 4; j++) { double s = 0; for (int k = 0; k < 4; k++) s += A_(Q, 6, i, k) * A_(R, 4, k, j); er = fmax(er, fabs(s - A_(Q0, 6, i, j))); }
    expect(!info && eo < 1e-13 && er < 1e-13, "dgeqrf + dorgqr: Q^T Q = I e Q R = A");
    zx Zq[24], Z0[24], zt[4], zw[64];
    for (int i = 0; i < 24; i++) Z0[i] = rnd() + rnd() * I;
    memcpy(Zq, Z0, sizeof Zq);
    zgeqrf_(&m, &nq, Zq, &m, zt, zw, &lw64, &info);
    zx ZR[16] = {0}; for (int j = 0; j < 4; j++) for (int i = 0; i <= j; i++) A_(ZR, 4, i, j) = A_(Zq, 6, i, j);
    zungqr_(&m, &nq, &nq, Zq, &m, zt, zw, &lw64, &info);
    eo = er = 0;
    for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) { zx s = 0; for (int k = 0; k < 6; k++) s += conj(A_(Zq, 6, k, i)) * A_(Zq, 6, k, j); eo = fmax(eo, cabs(s - (i == j))); }
    for (int i = 0; i < 6; i++) for (int j = 0; j < 4; j++) { zx s = 0; for (int k = 0; k < 4; k++) s += A_(Zq, 6, i, k) * A_(ZR, 4, k, j); er = fmax(er, cabs(s - A_(Z0, 6, i, j))); }
    expect(!info && eo < 1e-13 && er < 1e-13, "zgeqrf + zungqr: Q^H Q = I e Q R = A");
    /* simétrico / hermitiano */
    double V[25], w[5]; int iw[64], liw = 64, lwq = -1, liwq = -1; double wsz; int isz;
    dsyevd_("V", "L", &n, V, &n, w, &wsz, &lwq, &isz, &liwq, &info);
    expect(wsz >= 1 + 6 * 5 + 50 && isz >= 28, "dsyevd consulta de espaço");
    memcpy(V, S, sizeof S);
    dsyevd_("V", "U", &n, V, &n, w, wk, &lw64, iw, &liw, &info);
    e = 0; int asc = 1;
    for (int j = 0; j < 5; j++) { if (j && w[j] < w[j - 1]) asc = 0; for (int i = 0; i < 5; i++) { double s = 0; for (int k = 0; k < 5; k++) s += A_(S, 5, i, k) * A_(V, 5, k, j); e = fmax(e, fabs(s - w[j] * A_(V, 5, i, j))); } }
    expect(!info && asc && e < 1e-11, "dsyevd: A v = lambda v, crescente");
    zx H[16], HV[16]; double hw[4], rw[128]; int n4 = 4, lr = 128;
    for (int i = 0; i < 4; i++) for (int j = 0; j <= i; j++) { zx v = i == j ? rnd() : rnd() + rnd() * I; A_(H, 4, i, j) = v; A_(H, 4, j, i) = conj(v); }
    memcpy(HV, H, sizeof H);
    for (int i = 0; i < 4; i++) for (int j = i + 1; j < 4; j++) A_(HV, 4, i, j) = 99;   /* só o triângulo de baixo vale */
    zheevd_("V", "L", &n4, HV, &n4, hw, zw, &lw64, rw, &lr, iw, &liw, &info);
    e = 0; for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++) { zx s = 0; for (int k = 0; k < 4; k++) s += A_(H, 4, i, k) * A_(HV, 4, k, j); e = fmax(e, cabs(s - hw[j] * A_(HV, 4, i, j))); }
    expect(!info && hw[0] <= hw[3] && e < 1e-11, "zheevd: H v = lambda v (triângulo L)");
    /* SVD */
    for (int c = 0; c < 2; c++) {
        int mm = c ? 3 : 6, nn = c ? 5 : 4, mn = mm < nn ? mm : nn;
        double M[30], M0[30], s[5], U[36], VT[25];
        for (int i = 0; i < mm * nn; i++) M0[i] = rnd();
        memcpy(M, M0, sizeof M);
        dgesdd_("A", &mm, &nn, M, &mm, s, U, &mm, VT, &nn, wk, &lw64, iw, &info);
        double eu = 0, ev = 0, ea = 0;
        for (int i = 0; i < mm; i++) for (int j = 0; j < mm; j++) { double t = 0; for (int k = 0; k < mm; k++) t += A_(U, mm, k, i) * A_(U, mm, k, j); eu = fmax(eu, fabs(t - (i == j))); }
        for (int i = 0; i < nn; i++) for (int j = 0; j < nn; j++) { double t = 0; for (int k = 0; k < nn; k++) t += A_(VT, nn, i, k) * A_(VT, nn, j, k); ev = fmax(ev, fabs(t - (i == j))); }
        for (int i = 0; i < mm; i++) for (int j = 0; j < nn; j++) { double t = 0; for (int k = 0; k < mn; k++) t += A_(U, mm, i, k) * s[k] * A_(VT, nn, k, j); ea = fmax(ea, fabs(t - A_(M0, mm, i, j))); }
        int desc = 1; for (int k = 1; k < mn; k++) if (s[k] > s[k - 1]) desc = 0;
        expect(!info && desc && eu < 1e-12 && ev < 1e-12 && ea < 1e-12, c ? "dgesdd A (3x5): U S V^T = A" : "dgesdd A (6x4): U S V^T = A");
    }
    {
        int mm = 4, nn = 3; zx M[12], M0[12], U[16], VT[9]; double s[3], rwz[256];
        for (int i = 0; i < 12; i++) M0[i] = rnd() + rnd() * I;
        memcpy(M, M0, sizeof M);
        zgesdd_("S", &mm, &nn, M, &mm, s, U, &mm, VT, &nn, zw, &lw64, rwz, iw, &info);
        double ea = 0;
        for (int i = 0; i < mm; i++) for (int j = 0; j < nn; j++) { zx t = 0; for (int k = 0; k < 3; k++) t += A_(U, mm, i, k) * s[k] * A_(VT, nn, k, j); ea = fmax(ea, cabs(t - A_(M0, mm, i, j))); }
        expect(!info && ea < 1e-12, "zgesdd S: U S V^H = A");
    }
    /* mínimos quadrados: igual às equações normais */
    {
        int mm = 6, nn = 3, nr = 1, rank; double M[18], M0[18], bb[6], b0[6], s[3], rc = -1;
        for (int i = 0; i < 18; i++) M0[i] = rnd();
        for (int i = 0; i < 6; i++) b0[i] = rnd();
        memcpy(M, M0, sizeof M); memcpy(bb, b0, sizeof bb);
        dgelsd_(&mm, &nn, &nr, M, &mm, bb, &mm, s, &rc, &rank, wk, &lw64, iw, &info);
        double N_[9], r_[3]; int ip[3];
        for (int i = 0; i < 3; i++) { r_[i] = 0; for (int k = 0; k < 6; k++) r_[i] += A_(M0, 6, k, i) * b0[k]; for (int j = 0; j < 3; j++) { double t = 0; for (int k = 0; k < 6; k++) t += A_(M0, 6, k, i) * A_(M0, 6, k, j); A_(N_, 3, i, j) = t; } }
        dgesv_(&nn, &nr, N_, &nn, ip, r_, &nn, &info);
        expect(rank == 3 && fabs(bb[0] - r_[0]) < 1e-10 && fabs(bb[2] - r_[2]) < 1e-10, "dgelsd = equações normais");
    }
    /* geev real: pares conjugados, A v = lambda v e u^H A = lambda u^H */
    {
        double G[25], G0[25], wr[5], wi[5], VL[25], VR[25];
        for (int i = 0; i < 25; i++) G0[i] = rnd();
        memcpy(G, G0, sizeof G);
        dgeev_("V", "V", &n, G, &n, wr, wi, VL, &n, VR, &n, wk, &lw64, &info);
        double er2 = 0, el = 0; int pairs = 1, cplx = 0;
        for (int j = 0; j < 5; j++) {
            zx lam = wr[j] + wi[j] * I, vr[5], vl[5];
            if (wi[j] != 0) { cplx++; if (wi[j] > 0 && (j + 1 >= 5 || wi[j + 1] != -wi[j] || wr[j + 1] != wr[j])) pairs = 0; }
            for (int i = 0; i < 5; i++) {
                if (wi[j] == 0) { vr[i] = A_(VR, 5, i, j); vl[i] = A_(VL, 5, i, j); }
                else if (wi[j] > 0) { vr[i] = A_(VR, 5, i, j) + A_(VR, 5, i, j + 1) * I; vl[i] = A_(VL, 5, i, j) + A_(VL, 5, i, j + 1) * I; }
                else { vr[i] = A_(VR, 5, i, j - 1) - A_(VR, 5, i, j) * I; vl[i] = A_(VL, 5, i, j - 1) - A_(VL, 5, i, j) * I; }
            }
            for (int i = 0; i < 5; i++) {
                zx t = 0, u = 0;
                for (int k = 0; k < 5; k++) { t += A_(G0, 5, i, k) * vr[k]; u += conj(vl[k]) * A_(G0, 5, k, i); }
                er2 = fmax(er2, cabs(t - lam * vr[i]));
                el = fmax(el, cabs(u - lam * conj(vl[i])));
            }
        }
        printf("  dgeev: %d autovalores complexos\n", cplx);
        expect(!info && pairs && er2 < 1e-10 && el < 1e-10, "dgeev: pares conjugados, vetores à direita e à esquerda");
    }
    {
        zx G[16], G0[16], wv[4], VR[16];
        for (int i = 0; i < 16; i++) G0[i] = rnd() + rnd() * I;
        memcpy(G, G0, sizeof G);
        zgeev_("N", "V", &n4, G, &n4, wv, 0, &n4, VR, &n4, zw, &lw64, rw, &info);
        double er2 = 0;
        for (int j = 0; j < 4; j++) {
            double nn2 = 0; for (int i = 0; i < 4; i++) nn2 += cabs(A_(VR, 4, i, j)) * cabs(A_(VR, 4, i, j));
            for (int i = 0; i < 4; i++) { zx t = 0; for (int k = 0; k < 4; k++) t += A_(G0, 4, i, k) * A_(VR, 4, k, j); er2 = fmax(er2, cabs(t - wv[j] * A_(VR, 4, i, j))); }
            if (fabs(nn2 - 1) > 1e-12) er2 = 1;
        }
        expect(!info && er2 < 1e-10, "zgeev: A v = lambda v, vetores de norma 1");
    }
    printf("accelerate_lapack: %d/%d ok\n", total - fails, total);
    return fails != 0;
}
