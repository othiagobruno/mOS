/*
 * mOS — mais Accelerate pedido pelo corpus (docs/lacunas/isabela.md): vDSP (dupla precisão, rampas, biquad e biquadm,
 * desamp, FFT real em double), vForce (vvatan2, vvlog, vvsincos, vvsqrt, vvexp) e BLAS (cblas nível 1 a 3 reais e
 * complexos, *copy). Os nomes "...$NEWLAPACK" (a interface nova da Apple, mesmas assinaturas) são apelidos dos mesmos
 * códigos. Matemática em C, sem código de terceiros. Teste: tools/accelerate_mais_test.c. Código original do mOS (MIT).
 */
#include <stddef.h>
#include <stdint.h>

void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
double sin(double), cos(double), sqrt(double), exp(double), log(double), atan2(double, double);
float sinf(float), cosf(float), logf(float), atan2f(float, float);

typedef long vstride;
typedef unsigned long vlen;
typedef struct { float *realp, *imagp; } dsp_split_t;
typedef struct { double *realp, *imagp; } dsp_splitd_t;
typedef struct { double real, imag; } dsp_complexd_t;
#define PI 3.14159265358979323846
/* x$NEWLAPACK: no ELF um alias da função; no Mach-O (libSystem arm64, que não aceita alias) um salto em assembly com o
 * mesmo nome */
#if defined(__MACH__)
# if defined(__aarch64__)
#  define MOS_TRAMP(n, t) __asm__(".text\n.globl \"_" n "\"\n.p2align 2\n\"_" n "\":\n\tb _" t "\n");
# else
#  define MOS_TRAMP(n, t) __asm__(".text\n.globl \"_" n "\"\n\"_" n "\":\n\tjmp _" t "\n");
# endif
# define NEWLAPACK(f) MOS_TRAMP(#f "$NEWLAPACK", #f)
#else
#define NEWLAPACK(f) extern __typeof(f) f##_nl __asm__(#f "$NEWLAPACK") __attribute__((alias(#f)));
#endif

/* ---------------------------------------------------------------- vDSP */
void vDSP_vclrD(double *c, vstride ic, vlen n) { for (vlen i = 0; i < n; i++) c[i * ic] = 0; }
void vDSP_vsdivD(const double *a, vstride ia, const double *b, double *c, vstride ic, vlen n) { double s = *b; for (vlen i = 0; i < n; i++) c[i * ic] = a[i * ia] / s; }
void vDSP_vspdp(const float *a, vstride ia, double *c, vstride ic, vlen n) { for (vlen i = 0; i < n; i++) c[i * ic] = a[i * ia]; }
void vDSP_vdpsp(const double *a, vstride ia, float *c, vstride ic, vlen n) { for (vlen i = 0; i < n; i++) c[i * ic] = (float)a[i * ia]; }
/* intercalado <-> separado; IC conta em doubles (como no vDSP_ctoz, que conta em floats) */
void vDSP_ctozD(const dsp_complexd_t *c, vstride ic, const dsp_splitd_t *z, vstride iz, vlen n) {
    const double *f = (const double *)c;
    for (vlen i = 0; i < n; i++) { z->realp[i * iz] = f[i * ic]; z->imagp[i * iz] = f[i * ic + 1]; }
}
void vDSP_ztocD(const dsp_splitd_t *z, vstride iz, dsp_complexd_t *c, vstride ic, vlen n) {
    double *f = (double *)c;
    for (vlen i = 0; i < n; i++) { f[i * ic] = z->realp[i * iz]; f[i * ic + 1] = z->imagp[i * iz]; }
}
void vDSP_zvmagsD(const dsp_splitd_t *a, vstride ia, double *c, vstride ic, vlen n) {
    for (vlen i = 0; i < n; i++) { double r = a->realp[i * ia], m = a->imagp[i * ia]; c[i * ic] = r * r + m * m; }
}
void vDSP_dotpr(const float *a, vstride ia, const float *b, vstride ib, float *c, vlen n) {
    double s = 0; for (vlen i = 0; i < n; i++) s += (double)a[i * ia] * b[i * ib]; *c = (float)s;
}
void vDSP_vflt16(const short *a, vstride ia, float *c, vstride ic, vlen n) { for (vlen i = 0; i < n; i++) c[i * ic] = (float)a[i * ia]; }
/* rampas: o ganho começa em *start e anda *step por amostra; *start sai atualizado */
void vDSP_vrampmul(const float *in, vstride ii, float *start, const float *step, float *out, vstride io, vlen n) {
    float g = *start;
    for (vlen i = 0; i < n; i++) { out[i * io] = in[i * ii] * g; g += *step; }
    *start = g;
}
void vDSP_vrampmul2(const float *i0, const float *i1, vstride ii, float *start, const float *step, float *o0, float *o1, vstride io, vlen n) {
    float g = *start;
    for (vlen i = 0; i < n; i++) { o0[i * io] = i0[i * ii] * g; o1[i * io] = i1[i * ii] * g; g += *step; }
    *start = g;
}
void vDSP_vrampmuladd(const float *in, vstride ii, float *start, const float *step, float *out, vstride io, vlen n) {
    float g = *start;
    for (vlen i = 0; i < n; i++) { out[i * io] += in[i * ii] * g; g += *step; }
    *start = g;
}
void vDSP_vsmsma(const float *a, vstride ia, const float *b, const float *c, vstride ic, const float *d, float *e, vstride ie, vlen n) {
    float B = *b, D = *d;
    for (vlen i = 0; i < n; i++) e[i * ie] = a[i * ia] * B + c[i * ic] * D;
}
void vDSP_zvma(const dsp_split_t *a, vstride ia, const dsp_split_t *b, vstride ib, const dsp_split_t *c, vstride ic, const dsp_split_t *d, vstride id, vlen n) {
    for (vlen i = 0; i < n; i++) {
        float ar = a->realp[i * ia], ai = a->imagp[i * ia], br = b->realp[i * ib], bi = b->imagp[i * ib];
        float cr = c->realp[i * ic], ci = c->imagp[i * ic];
        d->realp[i * id] = ar * br - ai * bi + cr;
        d->imagp[i * id] = ar * bi + ai * br + ci;
    }
}
/* decimação com filtro FIR: C[n] = soma_p A[n*DF + p] * F[p] */
void vDSP_desamp(const float *a, vstride df, const float *f, float *c, vlen n, vlen p) {
    for (vlen i = 0; i < n; i++) {
        double s = 0;
        for (vlen k = 0; k < p; k++) s += (double)a[i * df + k] * f[k];
        c[i] = (float)s;
    }
}

/* biquad em cascata (forma direta I). Coeficientes por seção: b0, b1, b2, a1, a2 (a0 = 1).
 * Delay tem 2*M + 2 floats: x[n-1], x[n-2] da entrada e, para cada seção, y[n-1], y[n-2] da saída dela. */
typedef struct { unsigned long m; double c[1]; } biquad_t;
void *vDSP_biquad_CreateSetup(const double *coeffs, vlen m) {
    if (!coeffs || !m) return 0;
    biquad_t *s = malloc(sizeof *s + sizeof(double) * (5 * m));
    if (!s) return 0;
    s->m = m;
    memcpy(s->c, coeffs, sizeof(double) * 5 * m);
    return s;
}
void vDSP_biquad_DestroySetup(void *s) { free(s); }
void vDSP_biquad_SetCoefficientsDouble(void *setup, const double *coeffs, vlen start, vlen count) {
    biquad_t *s = setup;
    if (!s || !coeffs || start + count > s->m) return;
    memcpy(s->c + 5 * start, coeffs, sizeof(double) * 5 * count);
}
void vDSP_biquad(void *setup, float *delay, const float *x, vstride ix, float *y, vstride iy, vlen n) {
    biquad_t *s = setup;
    if (!s || !delay) return;
    for (vlen i = 0; i < n; i++) {
        double in = x[i * ix], x1 = delay[0], x2 = delay[1];
        delay[1] = delay[0]; delay[0] = (float)in;
        for (unsigned long k = 0; k < s->m; k++) {
            const double *c = s->c + 5 * k;
            float *d = delay + 2 + 2 * k;
            double out = c[0] * in + c[1] * x1 + c[2] * x2 - c[3] * d[0] - c[4] * d[1];
            x1 = d[0]; x2 = d[1];   /* a entrada da próxima seção é a saída desta */
            d[1] = d[0]; d[0] = (float)out;
            in = out;
        }
        y[i * iy] = (float)in;
    }
}
/* biquad de vários canais: coeficientes [seção][canal][5]; estado próprio por canal */
typedef struct { unsigned long m, ch; double *c, *st; } biquadm_t;
void *vDSP_biquadm_CreateSetup(const double *coeffs, vlen m, vlen ch) {
    if (!coeffs || !m || !ch) return 0;
    biquadm_t *s = calloc(1, sizeof *s);
    if (!s) return 0;
    s->m = m; s->ch = ch;
    s->c = malloc(sizeof(double) * 5 * m * ch);
    s->st = calloc((2 * m + 2) * ch, sizeof(double));
    if (!s->c || !s->st) { free(s->c); free(s->st); free(s); return 0; }
    memcpy(s->c, coeffs, sizeof(double) * 5 * m * ch);
    return s;
}
void vDSP_biquadm_DestroySetup(void *setup) { biquadm_t *s = setup; if (s) { free(s->c); free(s->st); free(s); } }
void vDSP_biquadm_ResetState(void *setup) { biquadm_t *s = setup; if (s) for (unsigned long i = 0; i < (2 * s->m + 2) * s->ch; i++) s->st[i] = 0; }
void vDSP_biquadm_SetCoefficientsDouble(void *setup, const double *coeffs, vlen sec0, vlen ch0, vlen nsec, vlen nch) {
    biquadm_t *s = setup;
    if (!s || !coeffs || sec0 + nsec > s->m || ch0 + nch > s->ch) return;
    for (vlen k = 0; k < nsec; k++)
        for (vlen c = 0; c < nch; c++) memcpy(s->c + 5 * ((sec0 + k) * s->ch + ch0 + c), coeffs + 5 * (k * nch + c), sizeof(double) * 5);
}
void vDSP_biquadm(void *setup, const float **x, vstride ix, float **y, vstride iy, vlen n) {
    biquadm_t *s = setup;
    if (!s || !x || !y) return;
    for (unsigned long ch = 0; ch < s->ch; ch++) {
        double *st = s->st + ch * (2 * s->m + 2);
        for (vlen i = 0; i < n; i++) {
            double in = x[ch][i * ix], x1 = st[0], x2 = st[1];
            st[1] = st[0]; st[0] = in;
            for (unsigned long k = 0; k < s->m; k++) {
                const double *c = s->c + 5 * (k * s->ch + ch);
                double *d = st + 2 + 2 * k, out = c[0] * in + c[1] * x1 + c[2] * x2 - c[3] * d[0] - c[4] * d[1];
                x1 = d[0]; x2 = d[1];
                d[1] = d[0]; d[0] = out;
                in = out;
            }
            y[ch][i * iy] = (float)in;
        }
    }
}

/* FFT real empacotada em double, o mesmo layout do vDSP_fft_zrip: realp[0] DC, imagp[0] Nyquist, direta x2 */
typedef struct { unsigned long log2n; } fftd_t;
void *vDSP_create_fftsetupD(unsigned long log2n, int radix) { (void)radix; fftd_t *s = malloc(sizeof *s); if (s) s->log2n = log2n; return s; }
void vDSP_destroy_fftsetupD(void *s) { free(s); }
static void fftd(double *re, double *im, unsigned long n, int sign) {
    for (unsigned long i = 1, j = 0; i < n; i++) {
        unsigned long bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { double t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
    }
    for (unsigned long len = 2; len <= n; len <<= 1)
        for (unsigned long i = 0; i < n; i += len)
            for (unsigned long k = 0; k < len / 2; k++) {
                double ang = sign * 2 * PI * (double)k / (double)len, cr = cos(ang), ci = sin(ang);
                unsigned long a = i + k, b = a + len / 2;
                double tr = re[b] * cr - im[b] * ci, ti = re[b] * ci + im[b] * cr;
                re[b] = re[a] - tr; im[b] = im[a] - ti;
                re[a] += tr; im[a] += ti;
            }
}
void vDSP_fft_zripD(void *setup, const dsp_splitd_t *c, vstride ic, unsigned long log2n, int dir) {
    (void)setup;
    if (!c || log2n < 1) return;
    unsigned long n = 1ul << log2n, h = n / 2;
    double *re = malloc(sizeof(double) * n), *im = malloc(sizeof(double) * n);
    if (!re || !im) { free(re); free(im); return; }
    if (dir >= 0) {
        for (unsigned long k = 0; k < h; k++) { re[2 * k] = c->realp[k * ic]; re[2 * k + 1] = c->imagp[k * ic]; im[2 * k] = im[2 * k + 1] = 0; }
        fftd(re, im, n, -1);
        c->realp[0] = 2 * re[0];
        c->imagp[0] = 2 * re[h];
        for (unsigned long k = 1; k < h; k++) { c->realp[k * ic] = 2 * re[k]; c->imagp[k * ic] = 2 * im[k]; }
    } else {
        re[0] = c->realp[0]; im[0] = 0;
        re[h] = c->imagp[0]; im[h] = 0;
        for (unsigned long k = 1; k < h; k++) { re[k] = c->realp[k * ic]; im[k] = c->imagp[k * ic]; re[n - k] = re[k]; im[n - k] = -im[k]; }
        fftd(re, im, n, +1);
        for (unsigned long k = 0; k < h; k++) { c->realp[k * ic] = re[2 * k]; c->imagp[k * ic] = re[2 * k + 1]; }
    }
    free(re); free(im);
}
void vDSP_fft_zriptD(void *setup, const dsp_splitd_t *c, vstride ic, const dsp_splitd_t *buf, vstride ib, unsigned long log2n, int dir) {
    (void)buf; (void)ib;   /* o buffer temporário é opcional: a área de trabalho é alocada aqui */
    vDSP_fft_zripD(setup, c, ic, log2n, dir);
}
/* fora do lugar: A -> C e a transformada em C */
void vDSP_fft_zrip(void *setup, const dsp_split_t *c, vstride ic, unsigned long log2n, int dir);
void vDSP_fft_zrop(void *setup, const dsp_split_t *a, vstride ia, const dsp_split_t *c, vstride ic, unsigned long log2n, int dir) {
    if (!a || !c || log2n < 1) return;
    unsigned long h = 1ul << (log2n - 1);
    for (unsigned long k = 0; k < h; k++) { c->realp[k * ic] = a->realp[k * ia]; c->imagp[k * ic] = a->imagp[k * ia]; }
    vDSP_fft_zrip(setup, c, ic, log2n, dir);
}

/* ---------------------------------------------------------------- vForce */
static int vn(const int *n) { return n && *n > 0 ? *n : 0; }
void vvatan2(double *z, const double *y, const double *x, const int *n) { int c = vn(n); for (int i = 0; i < c; i++) z[i] = atan2(y[i], x[i]); }
void vvatan2f(float *z, const float *y, const float *x, const int *n) { int c = vn(n); for (int i = 0; i < c; i++) z[i] = atan2f(y[i], x[i]); }
void vvlog(double *y, const double *x, const int *n) { int c = vn(n); for (int i = 0; i < c; i++) y[i] = log(x[i]); }
void vvlogf(float *y, const float *x, const int *n) { int c = vn(n); for (int i = 0; i < c; i++) y[i] = logf(x[i]); }
void vvsincos(double *s, double *co, const double *x, const int *n) { int c = vn(n); for (int i = 0; i < c; i++) { s[i] = sin(x[i]); co[i] = cos(x[i]); } }
void vvsincosf(float *s, float *co, const float *x, const int *n) { int c = vn(n); for (int i = 0; i < c; i++) { s[i] = sinf(x[i]); co[i] = cosf(x[i]); } }
void vvsqrt(double *y, const double *x, const int *n) { int c = vn(n); for (int i = 0; i < c; i++) y[i] = sqrt(x[i]); }
void vvexp(double *y, const double *x, const int *n) { int c = vn(n); for (int i = 0; i < c; i++) y[i] = exp(x[i]); }

/* ---------------------------------------------------------------- BLAS (CBLAS: 101 linha, 102 coluna; 111 N, 112 T, 113 C;
 * 121 Upper, 122 Lower). Incrementos negativos andam do fim, como na referência. */
typedef struct { float r, i; } cf;
typedef struct { double r, i; } cd;
static long st(int n, int inc) { return inc < 0 ? (long)(1 - n) * inc : 0; }
#define AXPY(name, T) void name(int n, T a, const T *x, int ix, T *y, int iy) { long px = st(n, ix), py = st(n, iy); for (int i = 0; i < n; i++, px += ix, py += iy) y[py] += a * x[px]; }
AXPY(cblas_saxpy, float)
AXPY(cblas_daxpy, double)
float cblas_sdot(int n, const float *x, int ix, const float *y, int iy) { long px = st(n, ix), py = st(n, iy); float s = 0; for (int i = 0; i < n; i++, px += ix, py += iy) s += x[px] * y[py]; return s; }
double cblas_ddot(int n, const double *x, int ix, const double *y, int iy) { long px = st(n, ix), py = st(n, iy); double s = 0; for (int i = 0; i < n; i++, px += ix, py += iy) s += x[px] * y[py]; return s; }
#define CAXPY(name, T) void name(int n, const void *pa, const void *px_, int ix, void *py_, int iy) { \
    const T *a = pa, *x = px_; T *y = py_; long px = st(n, ix), py = st(n, iy); \
    for (int i = 0; i < n; i++, px += ix, py += iy) { y[py].r += a->r * x[px].r - a->i * x[px].i; y[py].i += a->r * x[px].i + a->i * x[px].r; } }
CAXPY(cblas_caxpy, cf)
CAXPY(cblas_zaxpy, cd)
#define CDOT(name, T, conj) void name(int n, const void *px_, int ix, const void *py_, int iy, void *out) { \
    const T *x = px_, *y = py_; T *o = out; long px = st(n, ix), py = st(n, iy); double sr = 0, si = 0; \
    for (int i = 0; i < n; i++, px += ix, py += iy) { double xr = x[px].r, xi = conj ? -x[px].i : x[px].i; sr += xr * y[py].r - xi * y[py].i; si += xr * y[py].i + xi * y[py].r; } \
    o->r = sr; o->i = si; }
CDOT(cblas_cdotu_sub, cf, 0)
CDOT(cblas_cdotc_sub, cf, 1)
CDOT(cblas_zdotu_sub, cd, 0)
CDOT(cblas_zdotc_sub, cd, 1)
/* elemento (i, j) de op(A) para ordem/transposição da CBLAS */
#define AIDX(order, tr, A, lda, i, j) ((order) == 101 ? ((tr) == 111 ? (A)[(long)(i) * (lda) + (j)] : (A)[(long)(j) * (lda) + (i)]) \
                                                       : ((tr) == 111 ? (A)[(long)(j) * (lda) + (i)] : (A)[(long)(i) * (lda) + (j)]))
#define CPTR(order, C, ldc, i, j) ((order) == 101 ? &(C)[(long)(i) * (ldc) + (j)] : &(C)[(long)(j) * (ldc) + (i)])
#define GEMV(name, T) void name(int order, int tr, int m, int n, T alpha, const T *A, int lda, const T *x, int ix, T beta, T *y, int iy) { \
    int rows = tr == 111 ? m : n, cols = tr == 111 ? n : m; long py = st(rows, iy); \
    for (int i = 0; i < rows; i++, py += iy) { double s = 0; long px = st(cols, ix); \
        for (int j = 0; j < cols; j++, px += ix) s += (double)AIDX(order, tr == 111 ? 111 : 112, A, lda, i, j) * x[px]; \
        y[py] = (T)(alpha * s + (beta == 0 ? 0 : beta * y[py])); } }
GEMV(cblas_sgemv, float)
GEMV(cblas_dgemv, double)
#define SYRK(name, T) void name(int order, int uplo, int tr, int n, int k, T alpha, const T *A, int lda, T beta, T *C, int ldc) { \
    for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) { \
        if ((uplo == 121 && j < i) || (uplo == 122 && j > i)) continue; \
        double s = 0; for (int l = 0; l < k; l++) s += (double)AIDX(order, tr == 111 ? 111 : 112, A, lda, i, l) * AIDX(order, tr == 111 ? 111 : 112, A, lda, j, l); \
        T *c = CPTR(order, C, ldc, i, j); *c = (T)(alpha * s + (beta == 0 ? 0 : beta * *c)); } }
SYRK(cblas_ssyrk, float)
SYRK(cblas_dsyrk, double)
/* complexos: op(A)(i, j) com conjugação quando ConjTrans */
#define CAT(T, order, tr, A, lda, r_, c_, out) do { T _v = AIDX(order, tr == 111 ? 111 : 112, A, lda, r_, c_); if (tr == 113) _v.i = -_v.i; out = _v; } while (0)
#define CGEMV(name, T) void name(int order, int tr, int m, int n, const void *pa, const void *pA, int lda, const void *px_, int ix, const void *pb, void *py_, int iy) { \
    const T *alpha = pa, *beta = pb, *A = pA, *x = px_; T *y = py_; int rows = tr == 111 ? m : n, cols = tr == 111 ? n : m; long py = st(rows, iy); \
    for (int i = 0; i < rows; i++, py += iy) { double sr = 0, si = 0; long px = st(cols, ix); \
        for (int j = 0; j < cols; j++, px += ix) { T a; CAT(T, order, tr, A, lda, i, j, a); sr += a.r * x[px].r - a.i * x[px].i; si += a.r * x[px].i + a.i * x[px].r; } \
        double yr = alpha->r * sr - alpha->i * si, yi = alpha->r * si + alpha->i * sr; \
        if (beta->r != 0 || beta->i != 0) { yr += beta->r * y[py].r - beta->i * y[py].i; yi += beta->r * y[py].i + beta->i * y[py].r; } \
        y[py].r = yr; y[py].i = yi; } }
CGEMV(cblas_cgemv, cf)
CGEMV(cblas_zgemv, cd)
#define CGEMM(name, T) void name(int order, int ta, int tb, int m, int n, int k, const void *pa, const void *pA, int lda, const void *pB, int ldb, const void *pb, void *pC, int ldc) { \
    const T *alpha = pa, *beta = pb, *A = pA, *B = pB; T *C = pC; \
    for (int i = 0; i < m; i++) for (int j = 0; j < n; j++) { double sr = 0, si = 0; \
        for (int l = 0; l < k; l++) { T a, b; CAT(T, order, ta, A, lda, i, l, a); CAT(T, order, tb, B, ldb, l, j, b); sr += a.r * b.r - a.i * b.i; si += a.r * b.i + a.i * b.r; } \
        T *c = CPTR(order, C, ldc, i, j); double cr = alpha->r * sr - alpha->i * si, ci = alpha->r * si + alpha->i * sr; \
        if (beta->r != 0 || beta->i != 0) { cr += beta->r * c->r - beta->i * c->i; ci += beta->r * c->i + beta->i * c->r; } \
        c->r = cr; c->i = ci; } }
CGEMM(cblas_cgemm, cf)
CGEMM(cblas_zgemm, cd)
#define CSYRK(name, T) void name(int order, int uplo, int tr, int n, int k, const void *pa, const void *pA, int lda, const void *pb, void *pC, int ldc) { \
    const T *alpha = pa, *beta = pb, *A = pA; T *C = pC; \
    for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) { \
        if ((uplo == 121 && j < i) || (uplo == 122 && j > i)) continue; \
        double sr = 0, si = 0; for (int l = 0; l < k; l++) { T a = AIDX(order, tr == 111 ? 111 : 112, A, lda, i, l), b = AIDX(order, tr == 111 ? 111 : 112, A, lda, j, l); \
            sr += a.r * b.r - a.i * b.i; si += a.r * b.i + a.i * b.r; } \
        T *c = CPTR(order, C, ldc, i, j); double cr = alpha->r * sr - alpha->i * si, ci = alpha->r * si + alpha->i * sr; \
        if (beta->r != 0 || beta->i != 0) { cr += beta->r * c->r - beta->i * c->i; ci += beta->r * c->i + beta->i * c->r; } \
        c->r = cr; c->i = ci; } }
CSYRK(cblas_csyrk, cf)
CSYRK(cblas_zsyrk, cd)
/* BLAS de referência (argumentos por ponteiro) */
#define COPY(name, T) void name(const int *pn, const T *x, const int *pix, T *y, const int *piy) { \
    int n = *pn, ix = *pix, iy = *piy; long px = st(n, ix), py = st(n, iy); for (int i = 0; i < n; i++, px += ix, py += iy) y[py] = x[px]; }
COPY(scopy, float)
COPY(dcopy, double)
COPY(ccopy, cf)
COPY(zcopy, cd)

NEWLAPACK(cblas_saxpy) NEWLAPACK(cblas_daxpy) NEWLAPACK(cblas_caxpy) NEWLAPACK(cblas_zaxpy)
NEWLAPACK(cblas_sdot) NEWLAPACK(cblas_ddot) NEWLAPACK(cblas_cdotu_sub) NEWLAPACK(cblas_cdotc_sub) NEWLAPACK(cblas_zdotu_sub) NEWLAPACK(cblas_zdotc_sub)
NEWLAPACK(cblas_sgemv) NEWLAPACK(cblas_dgemv) NEWLAPACK(cblas_cgemv) NEWLAPACK(cblas_zgemv)
NEWLAPACK(cblas_ssyrk) NEWLAPACK(cblas_dsyrk) NEWLAPACK(cblas_csyrk) NEWLAPACK(cblas_zsyrk)
NEWLAPACK(cblas_cgemm) NEWLAPACK(cblas_zgemm)
NEWLAPACK(scopy) NEWLAPACK(dcopy) NEWLAPACK(ccopy) NEWLAPACK(zcopy)
