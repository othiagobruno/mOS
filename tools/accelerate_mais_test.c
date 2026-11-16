/* Host: userland/libsystem/accelerate_mais.c (+ o DFT real do accelerate_wa.c) contra contas diretas feitas aqui.
 * gcc -O1 -I... tools/accelerate_mais_test.c userland/libsystem/accelerate_mais.c userland/libsystem/accelerate_wa.c -lm
 * → "accelerate_mais: N/N ok" */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { float *realp, *imagp; } sp;
typedef struct { double *realp, *imagp; } spd;
typedef struct { float r, i; } cf;
typedef struct { double r, i; } cd;
void vDSP_vclrD(double *, long, unsigned long);
void vDSP_vsdivD(const double *, long, const double *, double *, long, unsigned long);
void vDSP_vspdp(const float *, long, double *, long, unsigned long);
void vDSP_vdpsp(const double *, long, float *, long, unsigned long);
void vDSP_ctozD(const void *, long, const spd *, long, unsigned long);
void vDSP_ztocD(const spd *, long, void *, long, unsigned long);
void vDSP_zvmagsD(const spd *, long, double *, long, unsigned long);
void vDSP_dotpr(const float *, long, const float *, long, float *, unsigned long);
void vDSP_vflt16(const short *, long, float *, long, unsigned long);
void vDSP_vrampmul(const float *, long, float *, const float *, float *, long, unsigned long);
void vDSP_vrampmul2(const float *, const float *, long, float *, const float *, float *, float *, long, unsigned long);
void vDSP_vrampmuladd(const float *, long, float *, const float *, float *, long, unsigned long);
void vDSP_vsmsma(const float *, long, const float *, const float *, long, const float *, float *, long, unsigned long);
void vDSP_zvma(const sp *, long, const sp *, long, const sp *, long, const sp *, long, unsigned long);
void vDSP_desamp(const float *, long, const float *, float *, unsigned long, unsigned long);
void *vDSP_biquad_CreateSetup(const double *, unsigned long);
void vDSP_biquad(void *, float *, const float *, long, float *, long, unsigned long);
void vDSP_biquad_DestroySetup(void *);
void *vDSP_biquadm_CreateSetup(const double *, unsigned long, unsigned long);
void vDSP_biquadm(void *, const float **, long, float **, long, unsigned long);
void vDSP_biquadm_DestroySetup(void *);
void *vDSP_create_fftsetupD(unsigned long, int);
void vDSP_fft_zripD(void *, const spd *, long, unsigned long, int);
void vDSP_fft_zriptD(void *, const spd *, long, const spd *, long, unsigned long, int);
void vDSP_fft_zrop(void *, const sp *, long, const sp *, long, unsigned long, int);
void *vDSP_DFT_zrop_CreateSetup(void *, unsigned long, int);
void vDSP_DFT_Execute(void *, const float *, const float *, float *, float *);
void vvatan2(double *, const double *, const double *, const int *);
void vvsincosf(float *, float *, const float *, const int *);
void vvlog(double *, const double *, const int *);
void vvexp(double *, const double *, const int *);
void cblas_daxpy(int, double, const double *, int, double *, int);
double cblas_ddot(int, const double *, int, const double *, int);
void cblas_dgemv(int, int, int, int, double, const double *, int, const double *, int, double, double *, int);
void cblas_sgemv(int, int, int, int, float, const float *, int, const float *, int, float, float *, int);
void cblas_dsyrk(int, int, int, int, int, double, const double *, int, double, double *, int);
void cblas_zgemm(int, int, int, int, int, int, const void *, const void *, int, const void *, int, const void *, void *, int);
void cblas_cgemv(int, int, int, int, const void *, const void *, int, const void *, int, const void *, void *, int);
void cblas_zdotc_sub(int, const void *, int, const void *, int, void *);
void cblas_csyrk(int, int, int, int, int, const void *, const void *, int, const void *, void *, int);
void dcopy(const int *, const double *, const int *, double *, const int *);
extern void cblas_ddot_nl(void) __asm__("cblas_ddot$NEWLAPACK");

/* o vDSP_fft_zrip float mora no vscode_media.c (não ligado aqui): para o teste do zrop, a versão double daqui */
void vDSP_fft_zrip(void *s, const sp *c, long ic, unsigned long log2n, int dir) {
    unsigned long h = 1ul << (log2n - 1);
    double *r = malloc(h * 8), *im = malloc(h * 8);
    spd d = {r, im};
    for (unsigned long k = 0; k < h; k++) { r[k] = c->realp[k * ic]; im[k] = c->imagp[k * ic]; }
    vDSP_fft_zripD(s, &d, 1, log2n, dir);
    for (unsigned long k = 0; k < h; k++) { c->realp[k * ic] = (float)r[k]; c->imagp[k * ic] = (float)im[k]; }
    free(r); free(im);
}

static int fails, total;
static void expect(int c, const char *m) { total++; if (!c) { printf("FALHOU: %s\n", m); fails++; } }
static double rnd(void) { return (rand() % 20001 - 10000) / 10000.0; }
static int close_(double a, double b, double tol) { return fabs(a - b) <= tol * (1 + fabs(b)); }

int main(void) {
    srand(5);
    /* vDSP simples */
    double d[8], q = 4; float f[8], g[8];
    for (int i = 0; i < 8; i++) { d[i] = rnd(); f[i] = (float)rnd(); }
    double dd[8]; vDSP_vsdivD(d, 1, &q, dd, 1, 8);
    int ok = 1; for (int i = 0; i < 8; i++) ok &= dd[i] == d[i] / 4; expect(ok, "vDSP_vsdivD");
    vDSP_vspdp(f, 1, dd, 1, 8); vDSP_vdpsp(dd, 1, g, 1, 8); expect(!memcmp(f, g, sizeof f), "vDSP_vspdp/vdpsp");
    vDSP_vclrD(dd, 2, 4); expect(dd[0] == 0 && dd[2] == 0 && dd[6] == 0 && dd[1] == (double)f[1], "vDSP_vclrD com passo");
    double inter[8] = {1, 2, 3, 4, 5, 6, 7, 8}, re[4], im[4], back[8]; spd z = {re, im};
    vDSP_ctozD(inter, 2, &z, 1, 4); vDSP_ztocD(&z, 1, back, 2, 4);
    expect(re[1] == 3 && im[1] == 4 && !memcmp(inter, back, sizeof back), "vDSP_ctozD/ztocD");
    double mag[4]; vDSP_zvmagsD(&z, 1, mag, 1, 4); expect(mag[2] == 25 + 36, "vDSP_zvmagsD");
    float dot; vDSP_dotpr(f, 1, f, 1, &dot, 8); double ref = 0; for (int i = 0; i < 8; i++) ref += (double)f[i] * f[i];
    expect(close_(dot, ref, 1e-6), "vDSP_dotpr");
    short s16[3] = {-32768, 0, 1234}; float fs[3]; vDSP_vflt16(s16, 1, fs, 1, 3); expect(fs[0] == -32768 && fs[2] == 1234, "vDSP_vflt16");
    float ones[4] = {1, 1, 1, 1}, out[4], out2[4], st = 0.5f, step = 0.25f;
    vDSP_vrampmul(ones, 1, &st, &step, out, 1, 4); expect(out[0] == 0.5f && out[3] == 1.25f && st == 1.5f, "vDSP_vrampmul");
    st = 1; vDSP_vrampmul2(ones, f, 1, &st, &step, out, out2, 1, 4); expect(out[1] == 1.25f && out2[2] == f[2] * 1.5f, "vDSP_vrampmul2");
    st = 1; vDSP_vrampmuladd(ones, 1, &st, &step, out, 1, 4); expect(out[0] == 2.0f, "vDSP_vrampmuladd");
    float b = 2, dd2 = 3; vDSP_vsmsma(f, 1, &b, g, 1, &dd2, out, 1, 4); expect(out[1] == f[1] * 2 + g[1] * 3, "vDSP_vsmsma");
    float ar[2] = {1, 2}, ai[2] = {3, 4}, br[2] = {5, 6}, bi[2] = {7, 8}, cr[2] = {1, 1}, ci[2] = {2, 2}, xr[2], xi[2];
    sp A = {ar, ai}, B = {br, bi}, C = {cr, ci}, D = {xr, xi};
    vDSP_zvma(&A, 1, &B, 1, &C, 1, &D, 1, 2); expect(xr[0] == 1 * 5 - 3 * 7 + 1 && xi[1] == 2 * 8 + 4 * 6 + 2, "vDSP_zvma");
    float sig[20], fir[4] = {0.1f, 0.2f, 0.3f, 0.4f}, dec[8];
    for (int i = 0; i < 20; i++) sig[i] = (float)i;
    vDSP_desamp(sig, 2, fir, dec, 8, 4); expect(close_(dec[3], 6 * 0.1 + 7 * 0.2 + 8 * 0.3 + 9 * 0.4, 1e-6), "vDSP_desamp");

    /* biquad: 2 seções contra a forma direta I calculada aqui; em dois pedaços igual a um */
    double co[10] = {0.2, 0.4, 0.2, -0.3, 0.1, 0.5, -0.1, 0.2, 0.25, 0.05};
    float x[64], y1[64], y2[64], dl[6] = {0}, dl2[6] = {0};
    for (int i = 0; i < 64; i++) x[i] = (float)rnd();
    double s1x1 = 0, s1x2 = 0, s1y1 = 0, s1y2 = 0, s2y1 = 0, s2y2 = 0, refy[64];
    for (int i = 0; i < 64; i++) {
        double a = co[0] * x[i] + co[1] * s1x1 + co[2] * s1x2 - co[3] * s1y1 - co[4] * s1y2;
        double bb = co[5] * a + co[6] * s1y1 + co[7] * s1y2 - co[8] * s2y1 - co[9] * s2y2;   /* histórico antigo da seção 1 */
        s1x2 = s1x1; s1x1 = x[i]; s1y2 = s1y1; s1y1 = a; s2y2 = s2y1; s2y1 = bb; refy[i] = bb;
    }
    void *bq = vDSP_biquad_CreateSetup(co, 2);
    vDSP_biquad(bq, dl, x, 1, y1, 1, 64);
    vDSP_biquad(bq, dl2, x, 1, y2, 1, 30); vDSP_biquad(bq, dl2, x + 30, 1, y2 + 30, 1, 34);
    ok = 1; for (int i = 0; i < 64; i++) ok &= close_(y1[i], refy[i], 1e-4) && y1[i] == y2[i];
    expect(ok, "vDSP_biquad (2 seções, estado entre chamadas)");
    double com[20]; for (int k = 0; k < 2; k++) for (int c = 0; c < 2; c++) for (int j = 0; j < 5; j++) com[(k * 2 + c) * 5 + j] = c ? co[k * 5 + j] * (j < 3 ? 0.5 : 1) : co[k * 5 + j];
    void *bm = vDSP_biquadm_CreateSetup(com, 2, 2);
    float ym0[64], ym1[64]; const float *xin[2] = {x, x}; float *yout[2] = {ym0, ym1};
    vDSP_biquadm(bm, xin, 1, yout, 1, 64);
    ok = 1; for (int i = 0; i < 64; i++) ok &= close_(ym0[i], y1[i], 1e-5);
    expect(ok && fabs(ym1[10] - y1[10]) > 1e-6, "vDSP_biquadm (2 canais, coeficientes por canal)");
    vDSP_biquad_DestroySetup(bq); vDSP_biquadm_DestroySetup(bm);

    /* FFT real em double contra o DFT direto; ida e volta */
    enum { N = 32 };
    double sigd[N], pr[N / 2], pi_[N / 2];
    for (int i = 0; i < N; i++) sigd[i] = rnd();
    for (int k = 0; k < N / 2; k++) { pr[k] = sigd[2 * k]; pi_[k] = sigd[2 * k + 1]; }
    spd P = {pr, pi_};
    void *fs_ = vDSP_create_fftsetupD(5, 2);
    vDSP_fft_zripD(fs_, &P, 1, 5, 1);
    ok = 1;
    for (int k = 0; k <= N / 2; k++) {
        double sr = 0, si = 0;
        for (int t = 0; t < N; t++) { sr += sigd[t] * cos(-2 * M_PI * k * t / N); si += sigd[t] * sin(-2 * M_PI * k * t / N); }
        if (k == 0) ok &= close_(pr[0], 2 * sr, 1e-9);
        else if (k == N / 2) ok &= close_(pi_[0], 2 * sr, 1e-9);
        else ok &= close_(pr[k], 2 * sr, 1e-9) && close_(pi_[k], 2 * si, 1e-9);
    }
    expect(ok, "vDSP_fft_zripD direta = DFT x2 (empacotada)");
    vDSP_fft_zriptD(fs_, &P, 1, &P, 1, 5, -1);
    ok = 1; for (int k = 0; k < N / 2; k++) ok &= close_(pr[k] / (2 * N), sigd[2 * k], 1e-9) && close_(pi_[k] / (2 * N), sigd[2 * k + 1], 1e-9);
    expect(ok, "vDSP_fft_zriptD ida e volta (escala 2N)");
    float ir[N / 2], ii[N / 2], orr[N / 2], oi[N / 2], c1r[N / 2], c1i[N / 2];
    for (int k = 0; k < N / 2; k++) { ir[k] = (float)sigd[2 * k]; ii[k] = (float)sigd[2 * k + 1]; }
    sp in_ = {ir, ii}, o_ = {c1r, c1i};
    vDSP_fft_zrop(0, &in_, 1, &o_, 1, 5, 1);
    void *dft = vDSP_DFT_zrop_CreateSetup(0, N, 1);
    vDSP_DFT_Execute(dft, ir, ii, orr, oi);
    ok = ir[3] == (float)sigd[6];   /* entrada intacta */
    for (int k = 0; k < N / 2; k++) ok &= close_(orr[k], c1r[k], 1e-4) && close_(oi[k], c1i[k], 1e-4);
    expect(ok, "vDSP_fft_zrop e vDSP_DFT_zrop iguais à FFT empacotada");
    void *dft12 = vDSP_DFT_zrop_CreateSetup(0, 12, 1), *idft12 = vDSP_DFT_zrop_CreateSetup(0, 12, -1);
    float a12r[6], a12i[6], b12r[6], b12i[6], c12r[6], c12i[6];
    for (int k = 0; k < 6; k++) { a12r[k] = (float)rnd(); a12i[k] = (float)rnd(); }
    vDSP_DFT_Execute(dft12, a12r, a12i, b12r, b12i); vDSP_DFT_Execute(idft12, b12r, b12i, c12r, c12i);
    ok = 1; for (int k = 0; k < 6; k++) ok &= close_(c12r[k] / 24, a12r[k], 1e-4) && close_(c12i[k] / 24, a12i[k], 1e-4);
    expect(ok, "vDSP_DFT_zrop N=12 ida e volta");

    /* vForce */
    int n4 = 4; double yv[4] = {1, -1, 0.5, -2}, xv[4] = {1, 1, -3, -0.1}, rv[4];
    vvatan2(rv, yv, xv, &n4); ok = 1; for (int i = 0; i < 4; i++) ok &= rv[i] == atan2(yv[i], xv[i]); expect(ok, "vvatan2");
    float sv[4], cv[4], xf[4] = {0.1f, 1, -2, 3}; vvsincosf(sv, cv, xf, &n4);
    expect(sv[2] == sinf(-2) && cv[3] == cosf(3), "vvsincosf");
    double lx[4] = {1, 2, 10, 0.5}; vvlog(rv, lx, &n4); vvexp(rv, rv, &n4); ok = 1; for (int i = 0; i < 4; i++) ok &= close_(rv[i], lx[i], 1e-12); expect(ok, "vvlog/vvexp");

    /* BLAS contra laços diretos, nas duas ordens */
    double M[12], v3[3], v4[4], y4[4], yr4[4];
    for (int i = 0; i < 12; i++) M[i] = rnd();
    for (int i = 0; i < 3; i++) v3[i] = rnd();
    for (int i = 0; i < 4; i++) { y4[i] = rnd(); yr4[i] = y4[i]; }
    cblas_dgemv(101, 111, 4, 3, 2.0, M, 3, v3, 1, 0.5, y4, 1);   /* linha: A 4x3 */
    ok = 1; for (int i = 0; i < 4; i++) { double s = 0; for (int j = 0; j < 3; j++) s += M[i * 3 + j] * v3[j]; ok &= close_(y4[i], 2 * s + 0.5 * yr4[i], 1e-12); }
    expect(ok, "cblas_dgemv linha");
    for (int i = 0; i < 4; i++) v4[i] = rnd();
    double y3[3] = {0, 0, 0};
    cblas_dgemv(102, 112, 3, 4, 1.0, M, 3, v4, 1, 0, y3, 1);   /* coluna: A 3x4, y = A^T x? não: A^T é 4x3, x tem 3 */
    double y3b[3] = {0, 0, 0}; double v4b[3] = {v4[0], v4[1], v4[2]};
    cblas_dgemv(102, 112, 3, 4, 1.0, M, 3, v4b, 1, 0, y3b, 1);
    ok = 1;
    { double yy[4]; cblas_dgemv(102, 112, 3, 4, 1.0, M, 3, v3, 1, 0, yy, 1);
      for (int j = 0; j < 4; j++) { double s = 0; for (int i = 0; i < 3; i++) s += M[j * 3 + i] * v3[i]; ok &= close_(yy[j], s, 1e-12); } }
    expect(ok, "cblas_dgemv coluna transposta");
    float Mf[6] = {1, 2, 3, 4, 5, 6}, xf2[2] = {1, -1}, yf[3];
    cblas_sgemv(102, 111, 3, 2, 1.0f, Mf, 3, xf2, 1, 0, yf, 1); expect(yf[0] == 1 - 4 && yf[2] == 3 - 6, "cblas_sgemv coluna");
    double ax[3] = {1, 2, 3}, ay[3] = {10, 20, 30};
    cblas_daxpy(3, 2, ax, -1, ay, 1); expect(ay[0] == 16 && ay[2] == 32, "cblas_daxpy com incremento negativo");
    expect(cblas_ddot(3, ax, 1, ax, 1) == 14 && ((double (*)(int, const double *, int, const double *, int))cblas_ddot_nl)(3, ax, 1, ax, 1) == 14, "cblas_ddot e cblas_ddot$NEWLAPACK");
    double Cs[9] = {0}; cblas_dsyrk(101, 121, 111, 3, 4, 1.0, M, 4, 0, Cs, 3);
    ok = Cs[3] == 0;   /* só o triângulo de cima */
    for (int i = 0; i < 3; i++) for (int j = i; j < 3; j++) { double s = 0; for (int l = 0; l < 4; l++) s += M[i * 4 + l] * M[j * 4 + l]; ok &= close_(Cs[i * 3 + j], s, 1e-12); }
    expect(ok, "cblas_dsyrk (upper, linha)");
    cd Z1[4], Z2[4], Z3[4], al = {1, 0.5}, be = {0, 0};
    for (int i = 0; i < 4; i++) { Z1[i].r = rnd(); Z1[i].i = rnd(); Z2[i].r = rnd(); Z2[i].i = rnd(); }
    cblas_zgemm(102, 113, 111, 2, 2, 2, &al, Z1, 2, Z2, 2, &be, Z3, 2);   /* C = alfa A^H B */
    ok = 1;
    for (int i = 0; i < 2; i++) for (int j = 0; j < 2; j++) {
        double sr = 0, si = 0;
        for (int l = 0; l < 2; l++) { cd a = Z1[i * 2 + l], bb2 = Z2[j * 2 + l]; a.i = -a.i; sr += a.r * bb2.r - a.i * bb2.i; si += a.r * bb2.i + a.i * bb2.r; }
        double er = al.r * sr - al.i * si, ei = al.r * si + al.i * sr;
        ok &= close_(Z3[j * 2 + i].r, er, 1e-12) && close_(Z3[j * 2 + i].i, ei, 1e-12);
    }
    expect(ok, "cblas_zgemm A^H B (coluna)");
    cd dz; cblas_zdotc_sub(4, Z1, 1, Z2, 1, &dz);
    { double sr = 0, si = 0; for (int i = 0; i < 4; i++) { sr += Z1[i].r * Z2[i].r + Z1[i].i * Z2[i].i; si += Z1[i].r * Z2[i].i - Z1[i].i * Z2[i].r; }
      expect(close_(dz.r, sr, 1e-12) && close_(dz.i, si, 1e-12), "cblas_zdotc_sub"); }
    cf Cf[4] = {{1, 2}, {3, 4}, {5, 6}, {7, 8}}, xc[2] = {{1, 0}, {0, 1}}, yc[2], a1 = {1, 0}, b0 = {0, 0};
    cblas_cgemv(101, 111, 2, 2, &a1, Cf, 2, xc, 1, &b0, yc, 1); expect(yc[0].r == 1 - 4 && yc[0].i == 2 + 3, "cblas_cgemv");
    cf Cc[4] = {{0, 0}}; cblas_csyrk(101, 122, 111, 2, 2, &a1, Cf, 2, &b0, Cc, 2);
    expect(Cc[1].r == 0 && Cc[2].r == (1 * 5 - 2 * 6) + (3 * 7 - 4 * 8) && Cc[2].i == (1 * 6 + 2 * 5) + (3 * 8 + 4 * 7), "cblas_csyrk (lower, sem conjugar)");
    int n3 = 3, i1 = 1, im1 = -1; double cp[3]; dcopy(&n3, ax, &i1, cp, &im1); expect(cp[0] == 3 && cp[2] == 1, "dcopy com incremento negativo");
    printf("accelerate_mais: %d/%d ok\n", total - fails, total);
    return fails != 0;
}
