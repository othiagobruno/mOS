/*
 * Accelerate que o WhatsApp 2.26.45.13 importa e o libSystem ainda não tinha
 * (medido com tools/machocheck.c --disco no zip oficial). Matemática em C,
 * sem código de terceiros. A FFT real empacotada reusa vDSP_fft_zrip
 * (vscode_media.c): realp[0] é DC, imagp[0] é Nyquist, direta multiplica por 2.
 */
#include <stddef.h>
#include <stdint.h>

void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
float log10f(float), expf(float), powf(float, float), sinf(float), cosf(float), sqrtf(float);
double sin(double), cos(double), sqrt(double);

typedef long vstride;
typedef unsigned long vlen;
typedef struct { float *realp, *imagp; } dsp_split_t;
typedef struct { void *data; unsigned long height, width, rowBytes; } vimg_t;

#define PI 3.14159265358979323846
#define KV_NOALLOC 1u
#define KV_BG 4u
#define KV_TEMP 128u
#define KV_BAD_KERNEL (-21767)
#define KV_NULL (-21772)
#define KV_PARAM (-21773)
#define KV_NOMEM (-21771)

/* ---------------------------------------------------------------- vDSP */
void vDSP_vclr(float *c, vstride ic, vlen n) { for (vlen i = 0; i < n; i++) c[i * ic] = 0; }
void vDSP_vfill(const float *a, float *c, vstride ic, vlen n) { float s = *a; for (vlen i = 0; i < n; i++) c[i * ic] = s; }
void vDSP_vneg(const float *a, vstride ia, float *c, vstride ic, vlen n) { for (vlen i = 0; i < n; i++) c[i * ic] = -a[i * ia]; }
void vDSP_vabs(const float *a, vstride ia, float *c, vstride ic, vlen n) { for (vlen i = 0; i < n; i++) { float v = a[i * ia]; c[i * ic] = v < 0 ? -v : v; } }
/* C = A / B. O divisor é o primeiro vetor, como em vDSP_vsub. */
void vDSP_vdiv(const float *b, vstride ib, const float *a, vstride ia, float *c, vstride ic, vlen n) {
    for (vlen i = 0; i < n; i++) c[i * ic] = a[i * ia] / b[i * ib];
}
void vDSP_vsdiv(const float *a, vstride ia, const float *b, float *c, vstride ic, vlen n) {
    float s = *b; for (vlen i = 0; i < n; i++) c[i * ic] = a[i * ia] / s;
}
void vDSP_vma(const float *a, vstride ia, const float *b, vstride ib, const float *c, vstride ic, float *d, vstride id, vlen n) {
    for (vlen i = 0; i < n; i++) d[i * id] = a[i * ia] * b[i * ib] + c[i * ic];
}
void vDSP_vmax(const float *a, vstride ia, const float *b, vstride ib, float *c, vstride ic, vlen n) {
    for (vlen i = 0; i < n; i++) { float x = a[i * ia], y = b[i * ib]; c[i * ic] = x > y ? x : y; }
}
void vDSP_vfltu8(const uint8_t *a, vstride ia, float *c, vstride ic, vlen n) {
    for (vlen i = 0; i < n; i++) c[i * ic] = (float)a[i * ia];
}
void vDSP_sve(const float *a, vstride ia, float *c, vlen n) { float s = 0; for (vlen i = 0; i < n; i++) s += a[i * ia]; *c = s; }
void vDSP_sveD(const double *a, vstride ia, double *c, vlen n) { double s = 0; for (vlen i = 0; i < n; i++) s += a[i * ia]; *c = s; }
void vDSP_meanv(const float *a, vstride ia, float *c, vlen n) { float s = 0; for (vlen i = 0; i < n; i++) s += a[i * ia]; *c = n ? s / (float)n : 0; }
void vDSP_svemg(const float *a, vstride ia, float *c, vlen n) {
    float s = 0; for (vlen i = 0; i < n; i++) { float v = a[i * ia]; s += v < 0 ? -v : v; } *c = s;
}
void vDSP_rmsqv(const float *a, vstride ia, float *c, vlen n) {
    float s = 0; for (vlen i = 0; i < n; i++) s += a[i * ia] * a[i * ia]; *c = n ? sqrtf(s / (float)n) : 0;
}
void vDSP_maxvi(const float *a, vstride ia, float *c, vlen *idx, vlen n) {
    float m = n ? a[0] : 0; vlen at = 0;
    for (vlen i = 1; i < n; i++) if (a[i * ia] > m) { m = a[i * ia]; at = i; }
    if (c) *c = m;
    if (idx) *idx = at;
}
void vDSP_vramp(const float *a, const float *b, float *c, vstride ic, vlen n) {
    float x = *a, s = *b; for (vlen i = 0; i < n; i++) c[i * ic] = x + (float)i * s;
}
void vDSP_vgen(const float *a, const float *b, float *c, vstride ic, vlen n) {
    if (!n) return;
    if (n == 1) { c[0] = *a; return; }
    float x0 = *a, x1 = *b, d = (x1 - x0) / (float)(n - 1);
    for (vlen i = 0; i < n; i++) c[i * ic] = x0 + (float)i * d;
}
/* Flag 0: 10*log10 (potência). Flag 1: 20*log10 (amplitude). C = alpha * log10(A / B). */
void vDSP_vdbcon(const float *a, vstride ia, const float *b, float *c, vstride ic, vlen n, unsigned f) {
    float alpha = f ? 20.f : 10.f, den = *b;
    for (vlen i = 0; i < n; i++) {
        float v = a[i * ia];
        c[i * ic] = (v > 0 && den != 0) ? alpha * log10f(v / den) : -__builtin_inff();
    }
}
/* Flag bit 0: 0 divide por N-1 (denorm), 1 divide por N (norm). */
void vDSP_hann_window(float *c, vlen n, int flag) {
    if (!n) return;
    if (n == 1) { c[0] = 1; return; }
    double den = (flag & 1) ? (double)n : (double)(n - 1);
    for (vlen i = 0; i < n; i++) c[i] = (float)(0.5 * (1.0 - cos(2.0 * PI * (double)i / den)));
}
void vDSP_mmov(const float *a, float *c, vlen m, vlen n, vlen ta, vlen tc) {
    for (vlen row = 0; row < n; row++) for (vlen col = 0; col < m; col++) c[row * tc + col] = a[row * ta + col];
}
/* M colunas, N linhas. C[n + m*IC] = A[m + n*IA]. */
void vDSP_mtrans(const float *a, vstride ia, float *c, vstride ic, vlen m, vlen n) {
    for (vlen row = 0; row < n; row++) for (vlen col = 0; col < m; col++) c[col * ic + (long)row] = a[row * ia + (long)col];
}

static void zv_bin(const dsp_split_t *a, vstride ia, const dsp_split_t *b, vstride ib, const dsp_split_t *c, vstride ic, vlen n, int op) {
    for (vlen i = 0; i < n; i++) {
        float ar = a->realp[i * ia], ai = a->imagp[i * ia];
        float br = b->realp[i * ib], bi = b->imagp[i * ib];
        float cr, ci;
        if (op == 0) { cr = ar + br; ci = ai + bi; }
        else if (op == 1) { cr = ar - br; ci = ai - bi; }
        else {
            float d = br * br + bi * bi;
            cr = (ar * br + ai * bi) / d;
            ci = (ai * br - ar * bi) / d;
        }
        c->realp[i * ic] = cr; c->imagp[i * ic] = ci;
    }
}
void vDSP_zvadd(const dsp_split_t *a, vstride ia, const dsp_split_t *b, vstride ib, const dsp_split_t *c, vstride ic, vlen n) { zv_bin(a, ia, b, ib, c, ic, n, 0); }
void vDSP_zvsub(const dsp_split_t *a, vstride ia, const dsp_split_t *b, vstride ib, const dsp_split_t *c, vstride ic, vlen n) { zv_bin(a, ia, b, ib, c, ic, n, 1); }
/* C = A / B. B é o primeiro argumento. */
void vDSP_zvdiv(const dsp_split_t *b, vstride ib, const dsp_split_t *a, vstride ia, const dsp_split_t *c, vstride ic, vlen n) { zv_bin(a, ia, b, ib, c, ic, n, 2); }
void vDSP_zvconj(const dsp_split_t *a, vstride ia, const dsp_split_t *c, vstride ic, vlen n) {
    for (vlen i = 0; i < n; i++) { c->realp[i * ic] = a->realp[i * ia]; c->imagp[i * ic] = -a->imagp[i * ia]; }
}
void vDSP_zvabs(const dsp_split_t *a, vstride ia, float *c, vstride ic, vlen n) {
    for (vlen i = 0; i < n; i++) { float r = a->realp[i * ia], im = a->imagp[i * ia]; c[i * ic] = sqrtf(r * r + im * im); }
}
void vDSP_zvmags(const dsp_split_t *a, vstride ia, float *c, vstride ic, vlen n) {
    for (vlen i = 0; i < n; i++) { float r = a->realp[i * ia], im = a->imagp[i * ia]; c[i * ic] = r * r + im * im; }
}

typedef struct { unsigned long n; int dir; } dft_setup_t;
void *vDSP_DFT_zop_CreateSetup(void *prev, vlen length, int direction) {
    (void)prev;
    if (!length) return 0;
    dft_setup_t *s = malloc(sizeof *s);
    if (!s) return 0;
    s->n = length; s->dir = direction;
    return s;
}
void vDSP_DFT_DestroySetup(void *setup) { free(setup); }
/* Direção +1: expoente negativo (análise). Sem escala: ida e volta multiplicam por N. */
void vDSP_DFT_Execute(void *setup, const float *ir, const float *ii, float *ore, float *oim) {
    dft_setup_t *s = setup;
    if (!s || !ir || !ii || !ore || !oim) return;
    unsigned long n = s->n;
    double sign = s->dir >= 0 ? -1.0 : 1.0;
    for (unsigned long k = 0; k < n; k++) {
        double sr = 0, si = 0;
        for (unsigned long t = 0; t < n; t++) {
            double ang = sign * 2.0 * PI * (double)k * (double)t / (double)n;
            double cs = cos(ang), sn = sin(ang);
            sr += (double)ir[t] * cs - (double)ii[t] * sn;
            si += (double)ir[t] * sn + (double)ii[t] * cs;
        }
        ore[k] = (float)sr; oim[k] = (float)si;
    }
}
/* FFT complexa radix-2. Sinal -1 direta, +1 inversa, sem escala. */
static void fft_c(double *re, double *im, unsigned long n, int sign) {
    for (unsigned long i = 1, j = 0; i < n; i++) {
        unsigned long bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { double t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
    }
    for (unsigned long len = 2; len <= n; len <<= 1) {
        double ang = sign * 2 * PI / (double)len, wr = cos(ang), wi = sin(ang);
        for (unsigned long i = 0; i < n; i += len) {
            double cr = 1, ci = 0;
            for (unsigned long k = 0; k < len / 2; k++) {
                unsigned long a = i + k, b = a + len / 2;
                double tr = re[b] * cr - im[b] * ci, ti = re[b] * ci + im[b] * cr;
                re[b] = re[a] - tr; im[b] = im[a] - ti;
                re[a] += tr; im[a] += ti;
                double nr = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = nr;
            }
        }
    }
}
/* Layout empacotado da Apple: realp[0] = DC, imagp[0] = Nyquist. Direta (dir >= 0) multiplica por 2. */
static void fft_zrip_pack(dsp_split_t *c, vstride ic, unsigned long log2n, int dir) {
    unsigned long n = 1ul << log2n, h = n / 2;
    double *re = malloc(sizeof(double) * n), *im = malloc(sizeof(double) * n);
    if (!re || !im) { free(re); free(im); return; }
    if (dir >= 0) {
        for (unsigned long k = 0; k < h; k++) { re[2 * k] = c->realp[k * ic]; re[2 * k + 1] = c->imagp[k * ic]; im[2 * k] = im[2 * k + 1] = 0; }
        fft_c(re, im, n, -1);
        c->realp[0] = (float)(2 * re[0]);
        c->imagp[0] = (float)(2 * re[h]);
        for (unsigned long k = 1; k < h; k++) { c->realp[k * ic] = (float)(2 * re[k]); c->imagp[k * ic] = (float)(2 * im[k]); }
    } else {
        re[0] = c->realp[0]; im[0] = 0;
        re[h] = c->imagp[0]; im[h] = 0;
        for (unsigned long k = 1; k < h; k++) {
            re[k] = c->realp[k * ic]; im[k] = c->imagp[k * ic];
            re[n - k] = re[k]; im[n - k] = -im[k];
        }
        fft_c(re, im, n, +1);
        for (unsigned long k = 0; k < h; k++) { c->realp[k * ic] = (float)re[2 * k]; c->imagp[k * ic] = (float)re[2 * k + 1]; }
    }
    free(re); free(im);
}
/* O buffer temporário é a área de trabalho; o resultado volta para C. */
void vDSP_fft_zript(void *setup, const dsp_split_t *c, vstride ic, const dsp_split_t *buf, vstride ib, unsigned long log2n, int dir) {
    (void)setup;
    if (!c || !buf || !c->realp || !buf->realp || log2n < 1) return;
    unsigned long h = 1ul << (log2n - 1);
    dsp_split_t w = *buf;
    for (unsigned long k = 0; k < h; k++) {
        w.realp[k * ib] = c->realp[k * ic];
        w.imagp[k * ib] = c->imagp[k * ic];
    }
    fft_zrip_pack(&w, ib, log2n, dir);
    for (unsigned long k = 0; k < h; k++) {
        c->realp[k * ic] = w.realp[k * ib];
        c->imagp[k * ic] = w.imagp[k * ib];
    }
}

/* ---------------------------------------------------------------- vForce: vetores contíguos, *n é a contagem */
static int vn(const int *n) { return n && *n > 0 ? *n : 0; }
void vvrecf(float *y, const float *x, const int *n) { int c = vn(n); for (int i = 0; i < c; i++) y[i] = 1.f / x[i]; }
void vvsqrtf(float *y, const float *x, const int *n) { int c = vn(n); for (int i = 0; i < c; i++) y[i] = sqrtf(x[i]); }
void vvexpf(float *y, const float *x, const int *n) { int c = vn(n); for (int i = 0; i < c; i++) y[i] = expf(x[i]); }
void vvlog10f(float *y, const float *x, const int *n) { int c = vn(n); for (int i = 0; i < c; i++) y[i] = log10f(x[i]); }
void vvsinf(float *y, const float *x, const int *n) { int c = vn(n); for (int i = 0; i < c; i++) y[i] = sinf(x[i]); }
void vvcosf(float *y, const float *x, const int *n) { int c = vn(n); for (int i = 0; i < c; i++) y[i] = cosf(x[i]); }
/* z = x ^ y. A base é o terceiro ponteiro. */
void vvpowf(float *z, const float *y, const float *x, const int *n) {
    int c = vn(n); for (int i = 0; i < c; i++) z[i] = powf(x[i], y[i]);
}

/* ---------------------------------------------------------------- BLAS */
void cblas_sgemm(int order, int transa, int transb, int m, int n, int k, float alpha, const float *a, int lda, const float *b, int ldb, float beta, float *c, int ldc) {
    int rowmaj = order == 101;
    for (int i = 0; i < m; i++) for (int j = 0; j < n; j++) {
        float acc = 0;
        for (int p = 0; p < k; p++) {
            float av, bv;
            if (rowmaj) {
                av = transa == 111 ? a[i * lda + p] : a[p * lda + i];
                bv = transb == 111 ? b[p * ldb + j] : b[j * ldb + p];
            } else {
                av = transa == 111 ? a[p * lda + i] : a[i * lda + p];
                bv = transb == 111 ? b[j * ldb + p] : b[p * ldb + j];
            }
            acc += av * bv;
        }
        float *dst = rowmaj ? &c[i * ldc + j] : &c[j * ldc + i];
        *dst = alpha * acc + beta * *dst;
    }
}
void cblas_dgemm(int order, int transa, int transb, int m, int n, int k, double alpha, const double *a, int lda, const double *b, int ldb, double beta, double *c, int ldc) {
    int rowmaj = order == 101;
    for (int i = 0; i < m; i++) for (int j = 0; j < n; j++) {
        double acc = 0;
        for (int p = 0; p < k; p++) {
            double av, bv;
            if (rowmaj) {
                av = transa == 111 ? a[i * lda + p] : a[p * lda + i];
                bv = transb == 111 ? b[p * ldb + j] : b[j * ldb + p];
            } else {
                av = transa == 111 ? a[p * lda + i] : a[i * lda + p];
                bv = transb == 111 ? b[j * ldb + p] : b[p * ldb + j];
            }
            acc += av * bv;
        }
        double *dst = rowmaj ? &c[i * ldc + j] : &c[j * ldc + i];
        *dst = alpha * acc + beta * *dst;
    }
}

/* ---------------------------------------------------------------- vImage */
long vImageBoxConvolve_ARGB8888(const vimg_t *src, const vimg_t *dst, void *tmp, unsigned long rx, unsigned long ry, unsigned kh, unsigned kw, const uint8_t bg[4], unsigned flags) {
    (void)tmp;
    if (flags & KV_TEMP) return 0;
    if (!src || !dst || !src->data || !dst->data) return KV_NULL;
    if (!(kh & 1) || !(kw & 1) || !kh || !kw) return KV_BAD_KERNEL;
    int hh = (int)kh / 2, hw = (int)kw / 2;
    unsigned area = kh * kw;
    for (unsigned long y = 0; y < dst->height; y++) {
        uint8_t *out = (uint8_t *)dst->data + y * dst->rowBytes;
        for (unsigned long x = 0; x < dst->width; x++) {
            unsigned sum[4] = {0, 0, 0, 0};
            for (int dy = -hh; dy <= hh; dy++) for (int dx = -hw; dx <= hw; dx++) {
                long sy = (long)(y + ry) + dy, sx = (long)(x + rx) + dx;
                int outb = sy < 0 || sx < 0 || (unsigned long)sy >= src->height || (unsigned long)sx >= src->width;
                if (outb && (flags & KV_BG)) { for (int c = 0; c < 4; c++) sum[c] += bg ? bg[c] : 0; continue; }
                if (outb) {
                    if (sy < 0) sy = 0;
                    if (sx < 0) sx = 0;
                    if ((unsigned long)sy >= src->height) sy = (long)src->height - 1;
                    if ((unsigned long)sx >= src->width) sx = (long)src->width - 1;
                }
                const uint8_t *p = (const uint8_t *)src->data + (unsigned long)sy * src->rowBytes + (unsigned long)sx * 4;
                for (int c = 0; c < 4; c++) sum[c] += p[c];
            }
            for (int c = 0; c < 4; c++) out[x * 4 + c] = (uint8_t)((sum[c] + area / 2) / area);
        }
    }
    return 0;
}
long vImageBuffer_Init(vimg_t *buf, unsigned long height, unsigned long width, unsigned bits, unsigned flags) {
    if (!buf) return KV_NULL;
    if (!height || !width || !bits) return KV_PARAM;
    unsigned long row = (width * bits + 7) / 8;
    row = (row + 15ul) & ~15ul;
    buf->height = height; buf->width = width; buf->rowBytes = row;
    if (flags & KV_NOALLOC) return 0;
    buf->data = calloc(height, row);
    return buf->data ? 0 : KV_NOMEM;
}
long vImageCopyBuffer(const vimg_t *src, const vimg_t *dst, unsigned long pixel, unsigned flags) {
    (void)flags;
    if (!src || !dst || !src->data || !dst->data || !pixel) return KV_NULL;
    unsigned long h = src->height < dst->height ? src->height : dst->height;
    unsigned long w = src->width < dst->width ? src->width : dst->width;
    unsigned long bytes = w * pixel;
    for (unsigned long y = 0; y < h; y++)
        memcpy((uint8_t *)dst->data + y * dst->rowBytes, (const uint8_t *)src->data + y * src->rowBytes, bytes);
    return 0;
}

static float samp_f(const vimg_t *src, long x, long y, unsigned flags, float bg) {
    if (x < 0 || y < 0 || (unsigned long)x >= src->width || (unsigned long)y >= src->height) {
        if (flags & KV_BG) return bg;
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if ((unsigned long)x >= src->width) x = (long)src->width - 1;
        if ((unsigned long)y >= src->height) y = (long)src->height - 1;
    }
    return ((const float *)((const uint8_t *)src->data + (unsigned long)y * src->rowBytes))[x];
}
long vImageConvolve_PlanarF(const vimg_t *src, const vimg_t *dst, void *tmp, unsigned long rx, unsigned long ry,
                            const float *kernel, unsigned long kh, unsigned long kw, float bg, unsigned flags) {
    (void)tmp;
    if (flags & KV_TEMP) return 0;
    if (!src || !dst || !kernel) return KV_NULL;
    if (!(kh & 1) || !(kw & 1) || !kh || !kw) return KV_BAD_KERNEL;
    long hh = (long)kh / 2, hw = (long)kw / 2;
    for (unsigned long y = 0; y < dst->height; y++) {
        float *out = (float *)((uint8_t *)dst->data + y * dst->rowBytes);
        for (unsigned long x = 0; x < dst->width; x++) {
            float s = 0;
            for (unsigned long ky = 0; ky < kh; ky++) for (unsigned long kx = 0; kx < kw; kx++) {
                long sy = (long)(y + ry) + (long)ky - hh, sx = (long)(x + rx) + (long)kx - hw;
                s += kernel[ky * kw + kx] * samp_f(src, sx, sy, flags, bg);
            }
            out[x] = s;
        }
    }
    return 0;
}

/* Bilinear com o centro do pixel: src = (dst + 0.5) * srcSize / dstSize - 0.5, borda estendida. */
static void bilinear_at(unsigned long sw, unsigned long sh, unsigned long dw, unsigned long dh, unsigned long x, unsigned long y, float *fx, float *fy, long *x0, long *y0, long *x1, long *y1) {
    float gx = ((float)x + 0.5f) * (float)sw / (float)dw - 0.5f;
    float gy = ((float)y + 0.5f) * (float)sh / (float)dh - 0.5f;
    if (gx < 0) gx = 0;
    if (gy < 0) gy = 0;
    if (gx > (float)(sw - 1)) gx = (float)(sw - 1);
    if (gy > (float)(sh - 1)) gy = (float)(sh - 1);
    *x0 = (long)gx; *y0 = (long)gy;
    *x1 = *x0 + 1 < (long)sw ? *x0 + 1 : *x0;
    *y1 = *y0 + 1 < (long)sh ? *y0 + 1 : *y0;
    *fx = gx - (float)*x0; *fy = gy - (float)*y0;
}
static uint8_t lerp_u8(float a, float b, float c, float d, float fx, float fy) {
    float v = a * (1 - fx) * (1 - fy) + b * fx * (1 - fy) + c * (1 - fx) * fy + d * fx * fy;
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    return (uint8_t)(v + 0.5f);
}
static uint16_t lerp_u16(float a, float b, float c, float d, float fx, float fy) {
    float v = a * (1 - fx) * (1 - fy) + b * fx * (1 - fy) + c * (1 - fx) * fy + d * fx * fy;
    if (v < 0) v = 0;
    if (v > 65535) v = 65535;
    return (uint16_t)(v + 0.5f);
}
long vImageScale_Planar8(const vimg_t *src, const vimg_t *dst, void *tmp, unsigned flags) {
    (void)tmp; if (flags & KV_TEMP) return 0;
    if (!src || !dst || !src->width || !dst->width) return KV_NULL;
    for (unsigned long y = 0; y < dst->height; y++) {
        uint8_t *out = (uint8_t *)dst->data + y * dst->rowBytes;
        for (unsigned long x = 0; x < dst->width; x++) {
            float fx, fy; long x0, y0, x1, y1;
            bilinear_at(src->width, src->height, dst->width, dst->height, x, y, &fx, &fy, &x0, &y0, &x1, &y1);
            const uint8_t *r0 = (const uint8_t *)src->data + (unsigned long)y0 * src->rowBytes;
            const uint8_t *r1 = (const uint8_t *)src->data + (unsigned long)y1 * src->rowBytes;
            out[x] = lerp_u8(r0[x0], r0[x1], r1[x0], r1[x1], fx, fy);
        }
    }
    return 0;
}
long vImageScale_PlanarF(const vimg_t *src, const vimg_t *dst, void *tmp, unsigned flags) {
    (void)tmp; if (flags & KV_TEMP) return 0;
    if (!src || !dst || !src->width || !dst->width) return KV_NULL;
    for (unsigned long y = 0; y < dst->height; y++) {
        float *out = (float *)((uint8_t *)dst->data + y * dst->rowBytes);
        for (unsigned long x = 0; x < dst->width; x++) {
            float fx, fy; long x0, y0, x1, y1;
            bilinear_at(src->width, src->height, dst->width, dst->height, x, y, &fx, &fy, &x0, &y0, &x1, &y1);
            const float *r0 = (const float *)((const uint8_t *)src->data + (unsigned long)y0 * src->rowBytes);
            const float *r1 = (const float *)((const uint8_t *)src->data + (unsigned long)y1 * src->rowBytes);
            out[x] = r0[x0] * (1 - fx) * (1 - fy) + r0[x1] * fx * (1 - fy) + r1[x0] * (1 - fx) * fy + r1[x1] * fx * fy;
        }
    }
    return 0;
}
long vImageScale_Planar16U(const vimg_t *src, const vimg_t *dst, void *tmp, unsigned flags) {
    (void)tmp; if (flags & KV_TEMP) return 0;
    if (!src || !dst || !src->width || !dst->width) return KV_NULL;
    for (unsigned long y = 0; y < dst->height; y++) {
        uint16_t *out = (uint16_t *)((uint8_t *)dst->data + y * dst->rowBytes);
        for (unsigned long x = 0; x < dst->width; x++) {
            float fx, fy; long x0, y0, x1, y1;
            bilinear_at(src->width, src->height, dst->width, dst->height, x, y, &fx, &fy, &x0, &y0, &x1, &y1);
            const uint16_t *r0 = (const uint16_t *)((const uint8_t *)src->data + (unsigned long)y0 * src->rowBytes);
            const uint16_t *r1 = (const uint16_t *)((const uint8_t *)src->data + (unsigned long)y1 * src->rowBytes);
            out[x] = lerp_u16(r0[x0], r0[x1], r1[x0], r1[x1], fx, fy);
        }
    }
    return 0;
}
static long scale_ch(const vimg_t *src, const vimg_t *dst, unsigned ch, unsigned bytes, int wide, unsigned flags) {
    if (flags & KV_TEMP) return 0;
    if (!src || !dst || !src->width || !dst->width) return KV_NULL;
    for (unsigned long y = 0; y < dst->height; y++) {
        uint8_t *out = (uint8_t *)dst->data + y * dst->rowBytes;
        for (unsigned long x = 0; x < dst->width; x++) {
            float fx, fy; long x0, y0, x1, y1;
            bilinear_at(src->width, src->height, dst->width, dst->height, x, y, &fx, &fy, &x0, &y0, &x1, &y1);
            const uint8_t *r0 = (const uint8_t *)src->data + (unsigned long)y0 * src->rowBytes;
            const uint8_t *r1 = (const uint8_t *)src->data + (unsigned long)y1 * src->rowBytes;
            for (unsigned c = 0; c < ch; c++) {
                if (!wide) {
                    uint8_t v = lerp_u8(r0[x0 * ch + c], r0[x1 * ch + c], r1[x0 * ch + c], r1[x1 * ch + c], fx, fy);
                    out[x * ch + c] = v;
                } else {
                    const uint16_t *a = (const uint16_t *)r0, *b = (const uint16_t *)r1;
                    uint16_t v = lerp_u16(a[x0 * ch + c], a[x1 * ch + c], b[x0 * ch + c], b[x1 * ch + c], fx, fy);
                    ((uint16_t *)out)[x * ch + c] = v;
                }
            }
            (void)bytes;
        }
    }
    return 0;
}
long vImageScale_ARGB8888(const vimg_t *src, const vimg_t *dst, void *tmp, unsigned flags) { (void)tmp; return scale_ch(src, dst, 4, 1, 0, flags); }
long vImageScale_CbCr8(const vimg_t *src, const vimg_t *dst, void *tmp, unsigned flags) { (void)tmp; return scale_ch(src, dst, 2, 1, 0, flags); }
long vImageScale_CbCr16U(const vimg_t *src, const vimg_t *dst, void *tmp, unsigned flags) { (void)tmp; return scale_ch(src, dst, 2, 2, 1, flags); }

long vImageConvert_ARGB8888toPlanar8(const vimg_t *src, const vimg_t *dst4, unsigned flags) {
    (void)flags;
    if (!src || !dst4) return KV_NULL;
    for (unsigned long y = 0; y < src->height; y++) {
        const uint8_t *in = (const uint8_t *)src->data + y * src->rowBytes;
        for (int c = 0; c < 4; c++) {
            uint8_t *out = (uint8_t *)dst4[c].data + y * dst4[c].rowBytes;
            for (unsigned long x = 0; x < src->width; x++) out[x] = in[x * 4 + c];
        }
    }
    return 0;
}
long vImageConvert_Planar8toPlanarF(const vimg_t *src, const vimg_t *dst, unsigned flags) {
    (void)flags;
    if (!src || !dst) return KV_NULL;
    for (unsigned long y = 0; y < src->height; y++) {
        const uint8_t *in = (const uint8_t *)src->data + y * src->rowBytes;
        float *out = (float *)((uint8_t *)dst->data + y * dst->rowBytes);
        for (unsigned long x = 0; x < src->width; x++) out[x] = (float)in[x] / 255.f;
    }
    return 0;
}
long vImageConvert_PlanarFtoPlanar8(const vimg_t *src, const vimg_t *dst, unsigned flags) {
    (void)flags;
    if (!src || !dst) return KV_NULL;
    for (unsigned long y = 0; y < src->height; y++) {
        const float *in = (const float *)((const uint8_t *)src->data + y * src->rowBytes);
        uint8_t *out = (uint8_t *)dst->data + y * dst->rowBytes;
        for (unsigned long x = 0; x < src->width; x++) {
            float v = in[x]; if (v < 0) v = 0; if (v > 1) v = 1;
            out[x] = (uint8_t)(v * 255.f + 0.5f);
        }
    }
    return 0;
}
long vImageMatrixMultiply_ARGB8888(const vimg_t *src, const vimg_t *dst, const int16_t m[16], int32_t divisor, const int16_t *pre, const int32_t *post, unsigned flags) {
    (void)flags;
    if (!src || !dst || !m || !divisor) return KV_PARAM;
    for (unsigned long y = 0; y < dst->height && y < src->height; y++) {
        const uint8_t *in = (const uint8_t *)src->data + y * src->rowBytes;
        uint8_t *out = (uint8_t *)dst->data + y * dst->rowBytes;
        for (unsigned long x = 0; x < dst->width && x < src->width; x++) {
            int32_t p[4];
            for (int c = 0; c < 4; c++) p[c] = (int32_t)in[x * 4 + c] + (pre ? pre[c] : 0);
            for (int r = 0; r < 4; r++) {
                int32_t s = 0;
                for (int c = 0; c < 4; c++) s += (int32_t)m[r * 4 + c] * p[c];
                s = s / divisor + (post ? post[r] : 0);
                if (s < 0) s = 0;
                if (s > 255) s = 255;
                out[x * 4 + r] = (uint8_t)s;
            }
        }
    }
    return 0;
}
long vImageUnpremultiplyData_RGBA8888(const vimg_t *src, const vimg_t *dst, unsigned flags) {
    (void)flags;
    if (!src || !dst) return KV_NULL;
    for (unsigned long y = 0; y < src->height; y++) {
        const uint8_t *in = (const uint8_t *)src->data + y * src->rowBytes;
        uint8_t *out = (uint8_t *)dst->data + y * dst->rowBytes;
        for (unsigned long x = 0; x < src->width; x++) {
            unsigned a = in[x * 4 + 3];
            if (!a) { out[x * 4] = out[x * 4 + 1] = out[x * 4 + 2] = out[x * 4 + 3] = 0; continue; }
            for (int c = 0; c < 3; c++) {
                unsigned v = (in[x * 4 + c] * 255u + a / 2) / a;
                out[x * 4 + c] = (uint8_t)(v > 255 ? 255 : v);
            }
            out[x * 4 + 3] = (uint8_t)a;
        }
    }
    return 0;
}
/* 0, 1, 2, 3 = 0°, 90°, 180°, 270° no sentido horário. */
long vImageRotate90_ARGB8888(const vimg_t *src, const vimg_t *dst, uint8_t rot, const uint8_t bg[4], unsigned flags) {
    (void)bg; (void)flags;
    if (!src || !dst) return KV_NULL;
    if (rot > 3) return KV_PARAM;
    for (unsigned long y = 0; y < dst->height; y++) {
        uint8_t *out = (uint8_t *)dst->data + y * dst->rowBytes;
        for (unsigned long x = 0; x < dst->width; x++) {
            unsigned long sx, sy;
            if (rot == 0) { sx = x; sy = y; }
            else if (rot == 1) { sx = y; sy = src->height - 1 - x; }
            else if (rot == 2) { sx = src->width - 1 - x; sy = src->height - 1 - y; }
            else { sx = src->width - 1 - y; sy = x; }
            const uint8_t *in = (const uint8_t *)src->data + sy * src->rowBytes + sx * 4;
            for (int c = 0; c < 4; c++) out[x * 4 + c] = in[c];
        }
    }
    return 0;
}

size_t CGImageGetWidth(void *img);
size_t CGImageGetHeight(void *img);
int cx_cgimage_src(const void *img, const uint8_t **px, long *bpr, int *w, int *h, int *premul);
void *CGDataProviderCreateWithData(void *info, const void *data, size_t size, void (*release)(void *, const void *, size_t));
void CGDataProviderRelease(void *p);
void *CGImageCreate(size_t w, size_t h, size_t bpc, size_t bpp, size_t bpr, const void *space, uint32_t info, void *provider, const void *decode, int shouldInterpolate, int intent);

typedef struct { uint32_t bpc, bpp; void *space; uint32_t bitmap; uint32_t version; const double *decode; int intent; } vimg_fmt_t;

long vImageBuffer_InitWithCGImage(vimg_t *buf, const vimg_fmt_t *fmt, const void *bg, void *image, unsigned flags) {
    (void)bg;
    if (!buf || !image) return KV_NULL;
    const uint8_t *px = 0; long bpr = 0; int w = 0, h = 0, prem = 0;
    if (!cx_cgimage_src(image, &px, &bpr, &w, &h, &prem) || !px || w <= 0 || h <= 0) return KV_PARAM;
    unsigned bits = fmt && fmt->bpp ? fmt->bpp : 32;
    long st = vImageBuffer_Init(buf, (unsigned long)h, (unsigned long)w, bits, flags & ~KV_NOALLOC);
    if (st) return st;
    if (bits == 32) {
        for (int y = 0; y < h; y++) memcpy((uint8_t *)buf->data + (unsigned long)y * buf->rowBytes, px + y * bpr, (size_t)w * 4);
    } else if (bits == 8) {
        for (int y = 0; y < h; y++) {
            const uint8_t *in = px + y * bpr;
            uint8_t *out = (uint8_t *)buf->data + (unsigned long)y * buf->rowBytes;
            for (int x = 0; x < w; x++) {
                const uint8_t *p = in + x * 4;
                out[x] = (uint8_t)(((unsigned)p[0] + p[1] + p[2]) / 3);
            }
        }
    } else { free(buf->data); buf->data = 0; return KV_PARAM; }
    (void)flags;
    return 0;
}
static void cg_free_copy(void *info, const void *data, size_t size) { (void)info; (void)size; free((void *)data); }
void *vImageCreateCGImageFromBuffer(const vimg_t *buf, const vimg_fmt_t *fmt, void (*cb)(void *, const void *), void *info, unsigned flags, long *err) {
    (void)cb; (void)info; (void)flags;
    if (!buf || !buf->data || !buf->width || !buf->height) { if (err) *err = KV_NULL; return 0; }
    unsigned bpp = fmt && fmt->bpp ? fmt->bpp : 32;
    if (bpp != 32 && bpp != 8) { if (err) *err = KV_PARAM; return 0; }
    size_t row = buf->width * (bpp / 8);
    size_t nbytes = row * buf->height;
    uint8_t *copy = malloc(nbytes);
    if (!copy) { if (err) *err = KV_NOMEM; return 0; }
    for (unsigned long y = 0; y < buf->height; y++)
        memcpy(copy + y * row, (const uint8_t *)buf->data + y * buf->rowBytes, row);
    void *prov = CGDataProviderCreateWithData(0, copy, nbytes, cg_free_copy);
    if (!prov) { free(copy); if (err) *err = KV_NOMEM; return 0; }
    uint32_t info_bm = fmt ? fmt->bitmap : 0;
    void *img = CGImageCreate(buf->width, buf->height, 8, bpp, row, fmt ? fmt->space : 0, info_bm, prov, 0, 1, fmt ? fmt->intent : 0);
    CGDataProviderRelease(prov);
    if (!img && err) *err = KV_PARAM;
    else if (err) *err = 0;
    return img;
}
