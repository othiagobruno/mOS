/*
 * mOS — o Accelerate que o corpus ainda pedia (docs/lacunas/: Accelerate.framework, 3 a 8 apps cada):
 *   - vImage: (des)multiplicar alfa RGBA8888, espelho vertical ARGB8888, PlanarF <-> Planar16F, NV12
 *     (420Yp8_CbCr8) -> ARGB8888, ARGB8888 -> 420Yp8 (bi e triplanar) com a conversão ARGB->YpCbCr 601/709,
 *     ARGB8888 -> RGB888, histograma ARGB8888, dilatação ARGBFFFF e multiplicação de matriz ARGB8888 -> Planar8;
 *   - vDSP: vsmulD, vmulD, vaddD, vsq, mmul; cblas_scopy;
 *   - vBigNum: vU256Add, vU256Divide, vU128FullMultiply (inteiros little-endian, como o vU128/vU256 no x86 e no arm64);
 *   - LAPACK: posv (Cholesky pelo potrf_ do accelerate_lapack.c) e gels (mínimos quadrados pelo gelsd_).
 * Teste: tools/accelerate_corpus_test.sh. Código original do mOS (MIT).
 */
#include <stddef.h>
#include <stdint.h>

void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);

typedef struct { void *data; unsigned long height, width, rowBytes; } vimg_t;
#define KV_NULL (-21772)
#define KV_PARAM (-21773)
#define KV_SIZE (-21774)
#define KV_NOMEM (-21771)
#define ROW(b, y) ((uint8_t *)(b)->data + (unsigned long)(y) * (b)->rowBytes)
static long check2(const vimg_t *s, const vimg_t *d) {
    if (!s || !d || !s->data || !d->data) return KV_NULL;
    if (s->width < d->width || s->height < d->height) return KV_SIZE;
    return 0;
}
static uint8_t sat8(float v) { v += 0.5f; return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v); }

/* ---------------------------------------------------------------- alfa e espelho */
static uint8_t mul8(unsigned c, unsigned a) { return (uint8_t)((c * a + 127) / 255); }
long vImagePremultiplyData_RGBA8888(const vimg_t *s, const vimg_t *d, unsigned flags) {
    (void)flags;
    long e = check2(s, d);
    if (e) return e;
    for (unsigned long y = 0; y < d->height; y++)
        for (unsigned long x = 0; x < d->width; x++) {
            const uint8_t *in = ROW(s, y) + x * 4;
            uint8_t *o = ROW(d, y) + x * 4, a = in[3];
            o[0] = mul8(in[0], a); o[1] = mul8(in[1], a); o[2] = mul8(in[2], a); o[3] = a;
        }
    return 0;
}
/* vImageUnpremultiplyData_RGBA8888: accelerate_wa.c */
static long vreflect(const vimg_t *s, const vimg_t *d, size_t bpp) {
    long e = check2(s, d);
    if (e) return e;
    size_t row = d->width * bpp;
    uint8_t *tmp = malloc(row ? row : 1);
    if (!tmp) return KV_NOMEM;
    unsigned long h = d->height;
    for (unsigned long y = 0; y < (h + 1) / 2; y++) {     /* troca as linhas das pontas (funciona em lugar) */
        unsigned long z = h - 1 - y;
        memcpy(tmp, ROW(s, y), row);
        memcpy(ROW(d, y), ROW(s, z), row);
        memcpy(ROW(d, z), tmp, row);
    }
    free(tmp);
    return 0;
}
long vImageVerticalReflect_ARGB8888(const vimg_t *s, const vimg_t *d, unsigned flags) { (void)flags; return vreflect(s, d, 4); }
long vImageVerticalReflect_Planar8(const vimg_t *s, const vimg_t *d, unsigned flags) { (void)flags; return vreflect(s, d, 1); }
long vImageVerticalReflect_ARGBFFFF(const vimg_t *s, const vimg_t *d, unsigned flags) { (void)flags; return vreflect(s, d, 16); }

/* ---------------------------------------------------------------- half <-> float */
static uint16_t f2h(float f) {
    uint32_t x; memcpy(&x, &f, 4);
    uint32_t sign = (x >> 16) & 0x8000, e = (x >> 23) & 0xff, m = x & 0x7fffff;
    if (e == 0xff) return (uint16_t)(sign | 0x7c00 | (m ? 0x200 | (m >> 13) : 0));
    int ne = (int)e - 127 + 15;
    if (ne >= 0x1f) return (uint16_t)(sign | 0x7c00);
    if (ne <= 0) {
        if (ne < -10) return (uint16_t)sign;
        m |= 0x800000;
        int sh = 14 - ne;
        uint32_t h = m >> sh, rest = m & ((1u << sh) - 1), half = 1u << (sh - 1);
        if (rest > half || (rest == half && (h & 1))) h++;
        return (uint16_t)(sign | h);
    }
    uint32_t h = ((uint32_t)ne << 10) | (m >> 13), rest = m & 0x1fff;
    if (rest > 0x1000 || (rest == 0x1000 && (h & 1))) h++;
    return (uint16_t)(sign | h);
}
static float h2f(uint16_t h) {
    uint32_t sign = (uint32_t)(h & 0x8000) << 16, e = (h >> 10) & 0x1f, m = h & 0x3ff, x;
    if (e == 0) {
        if (!m) x = sign;
        else { e = 127 - 15 + 1; while (!(m & 0x400)) { m <<= 1; e--; } m &= 0x3ff; x = sign | (e << 23) | (m << 13); }
    } else if (e == 0x1f) x = sign | 0x7f800000 | (m << 13);
    else x = sign | ((e - 15 + 127) << 23) | (m << 13);
    float f; memcpy(&f, &x, 4);
    return f;
}
long vImageConvert_PlanarFtoPlanar16F(const vimg_t *s, const vimg_t *d, unsigned flags) {
    (void)flags;
    long e = check2(s, d);
    if (e) return e;
    for (unsigned long y = 0; y < d->height; y++) {
        const float *in = (const float *)ROW(s, y);
        uint16_t *o = (uint16_t *)ROW(d, y);
        for (unsigned long x = 0; x < d->width; x++) o[x] = f2h(in[x]);
    }
    return 0;
}
long vImageConvert_Planar16FtoPlanarF(const vimg_t *s, const vimg_t *d, unsigned flags) {
    (void)flags;
    long e = check2(s, d);
    if (e) return e;
    for (unsigned long y = d->height; y-- > 0;) {        /* de trás para frente: em lugar o destino é maior */
        const uint16_t *in = (const uint16_t *)ROW(s, y);
        float *o = (float *)ROW(d, y);
        for (unsigned long x = d->width; x-- > 0;) o[x] = h2f(in[x]);
    }
    return 0;
}

/* ---------------------------------------------------------------- YpCbCr */
/* o mesmo layout do ycc_info_t de accelerate_vimage.c (vImage_YpCbCrToARGB é opaco) */
typedef struct { uint32_t magic; float yb, cb, ys, cs, cr_r, cr_g, cb_g, cb_b; float ymin, ymax, cmin, cmax; } ycc_info_t;
long vImageConvert_420Yp8_CbCr8ToARGB8888(const vimg_t *Y, const vimg_t *C, const vimg_t *d, const void *info,
                                          const uint8_t map[4], uint8_t alpha, unsigned flags) {
    (void)flags;
    const ycc_info_t *i = info;
    if (!Y || !C || !d || !i || !Y->data || !C->data || !d->data) return KV_NULL;
    if (i->magic != 0x59434331) return KV_PARAM;
    for (unsigned long y = 0; y < d->height; y++)
        for (unsigned long x = 0; x < d->width; x++) {
            const uint8_t *cc = ROW(C, y / 2) + (x / 2) * 2;
            float yy = ROW(Y, y)[x], u = cc[0], v = cc[1];
            yy = yy < i->ymin ? i->ymin : yy > i->ymax ? i->ymax : yy;
            u = u < i->cmin ? i->cmin : u > i->cmax ? i->cmax : u;
            v = v < i->cmin ? i->cmin : v > i->cmax ? i->cmax : v;
            float L = (yy - i->yb) * i->ys, U = (u - i->cb) * i->cs, V = (v - i->cb) * i->cs;   /* cs = 127,5 / (CbCrRangeMax - viés): o croma vai a ±127,5 */
            uint8_t argb[4] = {alpha, sat8(L + i->cr_r * V), sat8(L + i->cr_g * V + i->cb_g * U), sat8(L + i->cb_b * U)};
            uint8_t *o = ROW(d, y) + x * 4;
            for (int c = 0; c < 4; c++) o[c] = map ? argb[map[c] & 3] : argb[c];
        }
    return 0;
}
typedef struct { float R_Yp, G_Yp, B_Yp, R_Cb, G_Cb, B_Cb_R_Cr, G_Cr, B_Cr; } argb2ycc_matrix_t;
static const argb2ycc_matrix_t a601 = {0.299f, 0.587f, 0.114f, -0.168736f, -0.331264f, 0.5f, -0.418688f, -0.081312f};
static const argb2ycc_matrix_t a709 = {0.2126f, 0.7152f, 0.0722f, -0.114572f, -0.385428f, 0.5f, -0.454153f, -0.045847f};
const argb2ycc_matrix_t *const kvImage_ARGBToYpCbCrMatrix_ITU_R_601_4 = &a601;
const argb2ycc_matrix_t *const kvImage_ARGBToYpCbCrMatrix_ITU_R_709_2 = &a709;
typedef struct { int32_t Yp_bias, CbCr_bias, YpRangeMax, CbCrRangeMax, YpMax, YpMin, CbCrMax, CbCrMin; } ycc_range_t;
/* vImage_ARGBToYpCbCr é opaco (128 bytes) */
typedef struct { uint32_t magic; float m[8]; float yb, cb, ys, cs, ymin, ymax, cmin, cmax; } a2y_info_t;
#define A2Y_MAGIC 0x41325943u
long vImageConvert_ARGBToYpCbCr_GenerateConversion(const argb2ycc_matrix_t *m, const ycc_range_t *r, void *out, int in_type, int out_type, unsigned flags) {
    (void)in_type; (void)out_type; (void)flags;
    if (!m || !r || !out) return KV_NULL;
    if (r->YpRangeMax <= r->Yp_bias || r->CbCrRangeMax <= r->CbCr_bias) return KV_PARAM;
    a2y_info_t *i = out;
    i->magic = A2Y_MAGIC;
    i->m[0] = m->R_Yp; i->m[1] = m->G_Yp; i->m[2] = m->B_Yp; i->m[3] = m->R_Cb; i->m[4] = m->G_Cb;
    i->m[5] = m->B_Cb_R_Cr; i->m[6] = m->G_Cr; i->m[7] = m->B_Cr;
    i->yb = (float)r->Yp_bias; i->cb = (float)r->CbCr_bias;
    i->ys = (float)(r->YpRangeMax - r->Yp_bias) / 255.0f;
    i->cs = (float)(r->CbCrRangeMax - r->CbCr_bias) * 2.0f / 255.0f;
    i->ymin = (float)r->YpMin; i->ymax = (float)r->YpMax; i->cmin = (float)r->CbCrMin; i->cmax = (float)r->CbCrMax;
    return 0;
}
static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static void a2y_px(const a2y_info_t *i, const uint8_t *p, const uint8_t map[4], float *Yv, float *U, float *V) {
    uint8_t argb[4];
    for (int c = 0; c < 4; c++) argb[c] = map ? p[map[c] & 3] : p[c];
    float R = argb[1], G = argb[2], B = argb[3];
    *Yv = i->yb + i->ys * (i->m[0] * R + i->m[1] * G + i->m[2] * B);
    *U = i->cb + i->cs * (i->m[3] * R + i->m[4] * G + i->m[5] * B);
    *V = i->cb + i->cs * (i->m[5] * R + i->m[6] * G + i->m[7] * B);
}
static long argb_to_420(const vimg_t *s, const vimg_t *Y, const vimg_t *Cb, const vimg_t *Cr, const vimg_t *CbCr, const void *info, const uint8_t map[4]) {
    const a2y_info_t *i = info;
    if (!s || !Y || !s->data || !Y->data || !i) return KV_NULL;
    if (i->magic != A2Y_MAGIC) return KV_PARAM;
    for (unsigned long y = 0; y < Y->height; y += 2)
        for (unsigned long x = 0; x < Y->width; x += 2) {
            float su = 0, sv = 0;
            int n = 0;
            for (unsigned long dy = 0; dy < 2 && y + dy < Y->height; dy++)
                for (unsigned long dx = 0; dx < 2 && x + dx < Y->width; dx++) {
                    float yy, u, v;
                    a2y_px(i, ROW(s, y + dy) + (x + dx) * 4, map, &yy, &u, &v);
                    ROW(Y, y + dy)[x + dx] = sat8(clampf(yy, i->ymin, i->ymax) - 0.5f + 0.5f);
                    su += u; sv += v; n++;
                }
            uint8_t U = sat8(clampf(su / n, i->cmin, i->cmax)), V = sat8(clampf(sv / n, i->cmin, i->cmax));
            if (CbCr) { uint8_t *c = ROW(CbCr, y / 2) + (x / 2) * 2; c[0] = U; c[1] = V; }
            else { ROW(Cb, y / 2)[x / 2] = U; ROW(Cr, y / 2)[x / 2] = V; }
        }
    return 0;
}
/* 4:2:2 '2vuy': a cada 2 pixels, Cb Y0 Cr Y1 (croma pela média do par) */
long vImageConvert_ARGB8888To422CbYpCrYp8(const vimg_t *s, const vimg_t *d, const void *info, const uint8_t map[4], unsigned flags) {
    (void)flags;
    const a2y_info_t *i = info;
    if (!s || !d || !s->data || !d->data || !i) return KV_NULL;
    if (i->magic != A2Y_MAGIC) return KV_PARAM;
    if (d->width & 1) return KV_SIZE;
    for (unsigned long y = 0; y < d->height; y++)
        for (unsigned long x = 0; x < d->width; x += 2) {
            float y0, u0, v0, y1, u1, v1;
            a2y_px(i, ROW(s, y) + x * 4, map, &y0, &u0, &v0);
            a2y_px(i, ROW(s, y) + (x + 1) * 4, map, &y1, &u1, &v1);
            uint8_t *o = ROW(d, y) + x * 2;
            o[0] = sat8(clampf((u0 + u1) / 2, i->cmin, i->cmax));
            o[1] = sat8(clampf(y0, i->ymin, i->ymax));
            o[2] = sat8(clampf((v0 + v1) / 2, i->cmin, i->cmax));
            o[3] = sat8(clampf(y1, i->ymin, i->ymax));
        }
    return 0;
}
long vImageConvert_ARGB8888To420Yp8_CbCr8(const vimg_t *s, const vimg_t *Y, const vimg_t *CbCr, const void *info, const uint8_t map[4], unsigned flags) {
    (void)flags;
    if (!CbCr || !CbCr->data) return KV_NULL;
    return argb_to_420(s, Y, 0, 0, CbCr, info, map);
}
long vImageConvert_ARGB8888To420Yp8_Cb8_Cr8(const vimg_t *s, const vimg_t *Y, const vimg_t *Cb, const vimg_t *Cr, const void *info, const uint8_t map[4], unsigned flags) {
    (void)flags;
    if (!Cb || !Cr || !Cb->data || !Cr->data) return KV_NULL;
    return argb_to_420(s, Y, Cb, Cr, 0, info, map);
}
long vImageConvert_ARGB8888toRGB888(const vimg_t *s, const vimg_t *d, unsigned flags) {
    (void)flags;
    long e = check2(s, d);
    if (e) return e;
    for (unsigned long y = 0; y < d->height; y++)
        for (unsigned long x = 0; x < d->width; x++) {
            const uint8_t *in = ROW(s, y) + x * 4;
            uint8_t *o = ROW(d, y) + x * 3;
            o[0] = in[1]; o[1] = in[2]; o[2] = in[3];
        }
    return 0;
}
long vImageConvert_RGBA8888toRGB888(const vimg_t *s, const vimg_t *d, unsigned flags) {
    (void)flags;
    long e = check2(s, d);
    if (e) return e;
    for (unsigned long y = 0; y < d->height; y++)
        for (unsigned long x = 0; x < d->width; x++) memcpy(ROW(d, y) + x * 3, ROW(s, y) + x * 4, 3);
    return 0;
}

/* ---------------------------------------------------------------- histograma, morfologia, matriz */
long vImageHistogramCalculation_ARGB8888(const vimg_t *s, unsigned long *hist[4], unsigned flags) {
    (void)flags;
    if (!s || !s->data || !hist || !hist[0] || !hist[1] || !hist[2] || !hist[3]) return KV_NULL;
    for (int c = 0; c < 4; c++) memset(hist[c], 0, 256 * sizeof(unsigned long));
    for (unsigned long y = 0; y < s->height; y++)
        for (unsigned long x = 0; x < s->width; x++) {
            const uint8_t *p = ROW(s, y) + x * 4;
            for (int c = 0; c < 4; c++) hist[c][p[c]]++;
        }
    return 0;
}
long vImageHistogramCalculation_Planar8(const vimg_t *s, unsigned long *hist, unsigned flags) {
    (void)flags;
    if (!s || !s->data || !hist) return KV_NULL;
    memset(hist, 0, 256 * sizeof(unsigned long));
    for (unsigned long y = 0; y < s->height; y++) for (unsigned long x = 0; x < s->width; x++) hist[ROW(s, y)[x]]++;
    return 0;
}
/* dilatação em tons de cinza: máximo de (pixel - kernel) sob o kernel, borda estendida */
long vImageDilate_ARGBFFFF(const vimg_t *s, const vimg_t *d, unsigned long ox, unsigned long oy, const float *k, unsigned long kh,
                           unsigned long kw, unsigned flags) {
    (void)flags;
    if (!s || !d || !s->data || !d->data || !k) return KV_NULL;
    if (!(kh & 1) || !(kw & 1)) return KV_PARAM;
    for (unsigned long y = 0; y < d->height; y++)
        for (unsigned long x = 0; x < d->width; x++) {
            float best[4] = {-1e30f, -1e30f, -1e30f, -1e30f};
            for (unsigned long j = 0; j < kh; j++)
                for (unsigned long i = 0; i < kw; i++) {
                    long sy = (long)(y + oy + j) - (long)(kh / 2), sx = (long)(x + ox + i) - (long)(kw / 2);
                    sy = sy < 0 ? 0 : sy >= (long)s->height ? (long)s->height - 1 : sy;
                    sx = sx < 0 ? 0 : sx >= (long)s->width ? (long)s->width - 1 : sx;
                    const float *p = (const float *)ROW(s, sy) + sx * 4;
                    float kv = k[j * kw + i];
                    for (int c = 0; c < 4; c++) if (p[c] - kv > best[c]) best[c] = p[c] - kv;
                }
            memcpy((float *)ROW(d, y) + x * 4, best, 16);
        }
    return 0;
}
long vImageMatrixMultiply_ARGB8888ToPlanar8(const vimg_t *s, const vimg_t *d, const int16_t m[4], int32_t div, const int16_t pre[4],
                                            int32_t post, unsigned flags) {
    (void)flags;
    long e = check2(s, d);
    if (e) return e;
    if (!m || !div) return KV_PARAM;
    for (unsigned long y = 0; y < d->height; y++)
        for (unsigned long x = 0; x < d->width; x++) {
            const uint8_t *p = ROW(s, y) + x * 4;
            int64_t acc = post;
            for (int c = 0; c < 4; c++) acc += ((int64_t)p[c] + (pre ? pre[c] : 0)) * m[c];
            acc /= div;
            ROW(d, y)[x] = (uint8_t)(acc < 0 ? 0 : acc > 255 ? 255 : acc);
        }
    return 0;
}

/* ---------------------------------------------------------------- vDSP e BLAS */
typedef long vstride;
typedef unsigned long vlen;
void vDSP_vsmulD(const double *a, vstride ia, const double *b, double *c, vstride ic, vlen n) { double s = *b; for (vlen i = 0; i < n; i++) c[i * ic] = a[i * ia] * s; }
void vDSP_vmulD(const double *a, vstride ia, const double *b, vstride ib, double *c, vstride ic, vlen n) { for (vlen i = 0; i < n; i++) c[i * ic] = a[i * ia] * b[i * ib]; }
void vDSP_vaddD(const double *a, vstride ia, const double *b, vstride ib, double *c, vstride ic, vlen n) { for (vlen i = 0; i < n; i++) c[i * ic] = a[i * ia] + b[i * ib]; }
void vDSP_vsq(const float *a, vstride ia, float *c, vstride ic, vlen n) { for (vlen i = 0; i < n; i++) { float v = a[i * ia]; c[i * ic] = v * v; } }
void vDSP_vsqD(const double *a, vstride ia, double *c, vstride ic, vlen n) { for (vlen i = 0; i < n; i++) { double v = a[i * ia]; c[i * ic] = v * v; } }
/* C (M x N) = A (M x P) * B (P x N), por linhas, com passos */
void vDSP_mmul(const float *a, vstride ia, const float *b, vstride ib, float *c, vstride ic, vlen M, vlen N, vlen P) {
    for (vlen i = 0; i < M; i++)
        for (vlen j = 0; j < N; j++) {
            double s = 0;
            for (vlen k = 0; k < P; k++) s += (double)a[(i * P + k) * ia] * b[(k * N + j) * ib];
            c[(i * N + j) * ic] = (float)s;
        }
}
void vDSP_mmulD(const double *a, vstride ia, const double *b, vstride ib, double *c, vstride ic, vlen M, vlen N, vlen P) {
    for (vlen i = 0; i < M; i++)
        for (vlen j = 0; j < N; j++) {
            double s = 0;
            for (vlen k = 0; k < P; k++) s += a[(i * P + k) * ia] * b[(k * N + j) * ib];
            c[(i * N + j) * ic] = s;
        }
}
void cblas_scopy(int n, const float *x, int incx, float *y, int incy) {
    if (n <= 0) return;
    const float *px = incx < 0 ? x + (size_t)(n - 1) * (size_t)-incx : x;
    float *py = incy < 0 ? y + (size_t)(n - 1) * (size_t)-incy : y;
    for (int i = 0; i < n; i++) py[(long)i * incy] = px[(long)i * incx];
}

/* ---------------------------------------------------------------- vBigNum (palavras de 32 bits, a menos significativa primeiro) */
typedef struct { uint32_t w[4]; } vU128;
typedef struct { uint32_t w[8]; } vU256;
void vU256Add(const vU256 *a, const vU256 *b, vU256 *r) {
    uint64_t c = 0;
    for (int i = 0; i < 8; i++) { c += (uint64_t)a->w[i] + b->w[i]; r->w[i] = (uint32_t)c; c >>= 32; }
}
void vU256Sub(const vU256 *a, const vU256 *b, vU256 *r) {
    int64_t br = 0;
    for (int i = 0; i < 8; i++) { int64_t v = (int64_t)a->w[i] - b->w[i] - br; br = v < 0; r->w[i] = (uint32_t)v; }
}
void vU128FullMultiply(const vU128 *a, const vU128 *b, vU256 *r) {
    uint32_t t[8] = {0};
    for (int i = 0; i < 4; i++) {
        uint64_t c = 0;
        for (int j = 0; j < 4; j++) { c += (uint64_t)a->w[i] * b->w[j] + t[i + j]; t[i + j] = (uint32_t)c; c >>= 32; }
        t[i + 4] = (uint32_t)c;
    }
    memcpy(r->w, t, sizeof t);
}
static int u256_cmp(const uint32_t *a, const uint32_t *b) { for (int i = 7; i >= 0; i--) if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1; return 0; }
void vU256Divide(const vU256 *num, const vU256 *den, vU256 *q, vU256 *rem) {
    uint32_t Q[8] = {0}, R[8] = {0}, D[8];
    memcpy(D, den->w, sizeof D);
    int zero = 1;
    for (int i = 0; i < 8; i++) if (D[i]) zero = 0;
    if (!zero)
        for (int bit = 255; bit >= 0; bit--) {
            for (int i = 7; i > 0; i--) R[i] = (R[i] << 1) | (R[i - 1] >> 31);
            R[0] = (R[0] << 1) | ((num->w[bit / 32] >> (bit % 32)) & 1);
            if (u256_cmp(R, D) >= 0) {
                int64_t br = 0;
                for (int i = 0; i < 8; i++) { int64_t v = (int64_t)R[i] - D[i] - br; br = v < 0; R[i] = (uint32_t)v; }
                Q[bit / 32] |= 1u << (bit % 32);
            }
        }
    if (q) memcpy(q->w, Q, sizeof Q);
    if (rem) memcpy(rem->w, R, sizeof R);
}

/* ---------------------------------------------------------------- LAPACK: posv e gels */
typedef int I;
void dpotrf_(const char *u, const I *n, double *a, const I *lda, I *info);
void spotrf_(const char *u, const I *n, float *a, const I *lda, I *info);
void dgelsd_(const I *m, const I *n, const I *nr, double *a, const I *lda, double *b, const I *ldb, double *s, const double *rc, I *rank,
             double *w, const I *lw, I *iw, I *info);
#define E(A, ld, i, j) (A)[(size_t)(i) + (size_t)(j) * (size_t)(ld)]
/* resolve com o fator de Cholesky: U^T U x = b (uplo 'U') ou L L^T x = b (uplo 'L') */
static void chol_solve(char uplo, int n, const double *a, int lda, double *b, int ldb, int nrhs) {
    int up = uplo == 'U' || uplo == 'u';
    for (int r = 0; r < nrhs; r++) {
        double *x = b + (size_t)r * ldb;
        for (int i = 0; i < n; i++) {                      /* primeiro o triangular inferior */
            double s = x[i];
            for (int k = 0; k < i; k++) s -= (up ? E(a, lda, k, i) : E(a, lda, i, k)) * x[k];
            x[i] = s / E(a, lda, i, i);
        }
        for (int i = n - 1; i >= 0; i--) {                 /* depois o superior */
            double s = x[i];
            for (int k = i + 1; k < n; k++) s -= (up ? E(a, lda, i, k) : E(a, lda, k, i)) * x[k];
            x[i] = s / E(a, lda, i, i);
        }
    }
}
void dposv_(const char *uplo, const I *n, const I *nrhs, double *a, const I *lda, double *b, const I *ldb, I *info) {
    if (*n < 0) { *info = -2; return; }
    if (*nrhs < 0) { *info = -3; return; }
    if (*lda < (*n > 1 ? *n : 1)) { *info = -5; return; }
    if (*ldb < (*n > 1 ? *n : 1)) { *info = -7; return; }
    dpotrf_(uplo, n, a, lda, info);
    if (*info) return;
    chol_solve(*uplo, *n, a, *lda, b, *ldb, *nrhs);
}
void sposv_(const char *uplo, const I *n, const I *nrhs, float *a, const I *lda, float *b, const I *ldb, I *info) {
    int N = *n, R = *nrhs, LA = *lda, LB = *ldb;
    if (N < 0) { *info = -2; return; }
    if (R < 0) { *info = -3; return; }
    if (LA < (N > 1 ? N : 1)) { *info = -5; return; }
    if (LB < (N > 1 ? N : 1)) { *info = -7; return; }
    double *da = malloc(sizeof(double) * (size_t)(N ? N : 1) * (size_t)(N ? N : 1)), *db = malloc(sizeof(double) * (size_t)(N ? N : 1) * (size_t)(R ? R : 1));
    if (!da || !db) { free(da); free(db); *info = -1000; return; }
    for (int j = 0; j < N; j++) for (int i = 0; i < N; i++) da[i + (size_t)j * N] = E(a, LA, i, j);
    for (int j = 0; j < R; j++) for (int i = 0; i < N; i++) db[i + (size_t)j * N] = E(b, LB, i, j);
    I nn = N, nr = R;
    dposv_(uplo, &nn, &nr, da, &nn, db, &nn, info);
    for (int j = 0; j < N; j++) for (int i = 0; i < N; i++) E(a, LA, i, j) = (float)da[i + (size_t)j * N];
    if (!*info) for (int j = 0; j < R; j++) for (int i = 0; i < N; i++) E(b, LB, i, j) = (float)db[i + (size_t)j * N];
    free(da); free(db);
}
/* gels: mínimos quadrados (m >= n) ou norma mínima (m < n) pelo gelsd; trans 'T' resolve com A^T */
static void gels_d(char tr, int m, int n, int nrhs, const double *a, int lda, double *b, int ldb, I *info) {
    int t = tr == 'T' || tr == 't' || tr == 'C' || tr == 'c';
    int M = t ? n : m, N = t ? m : n, LB = ldb;
    double *A = malloc(sizeof(double) * (size_t)(M ? M : 1) * (size_t)(N ? N : 1));
    int rows = M > N ? M : N;
    double *B = calloc((size_t)(rows ? rows : 1) * (size_t)(nrhs ? nrhs : 1), sizeof(double));
    double *S = malloc(sizeof(double) * (size_t)((M < N ? M : N) + 1));
    if (!A || !B || !S) { free(A); free(B); free(S); *info = -1000; return; }
    for (int j = 0; j < N; j++) for (int i = 0; i < M; i++) A[i + (size_t)j * M] = t ? E(a, lda, j, i) : E(a, lda, i, j);
    for (int j = 0; j < nrhs; j++) for (int i = 0; i < M; i++) B[i + (size_t)j * rows] = E(b, LB, i, j);
    I im = M, in = N, inr = nrhs, ilda = M ? M : 1, ildb = rows ? rows : 1, rank = 0, lw = -1, iw0 = 0;
    double rc = -1, wq = 0;
    dgelsd_(&im, &in, &inr, A, &ilda, B, &ildb, S, &rc, &rank, &wq, &lw, &iw0, info);
    lw = (I)wq > 1 ? (I)wq : 1;
    double *W = malloc(sizeof(double) * (size_t)lw);
    I *IW = malloc(sizeof(I) * (size_t)(8 * (M + N) + 64));
    if (!W || !IW) { free(A); free(B); free(S); free(W); free(IW); *info = -1000; return; }
    dgelsd_(&im, &in, &inr, A, &ilda, B, &ildb, S, &rc, &rank, W, &lw, IW, info);
    if (!*info && rank < (M < N ? M : N)) *info = rank + 1;    /* gels exige posto cheio */
    if (!*info) for (int j = 0; j < nrhs; j++) for (int i = 0; i < N; i++) E(b, LB, i, j) = B[i + (size_t)j * rows];
    free(A); free(B); free(S); free(W); free(IW);
}
void dgels_(const char *tr, const I *m, const I *n, const I *nrhs, double *a, const I *lda, double *b, const I *ldb, double *w, const I *lw, I *info) {
    *info = 0;
    if (*m < 0) { *info = -2; return; }
    if (*n < 0) { *info = -3; return; }
    if (*lw == -1) { w[0] = (double)((*m > *n ? *m : *n) * (*nrhs > 1 ? *nrhs : 1) + 64); return; }
    gels_d(*tr, *m, *n, *nrhs, a, *lda, b, *ldb, info);
}
void sgels_(const char *tr, const I *m, const I *n, const I *nrhs, float *a, const I *lda, float *b, const I *ldb, float *w, const I *lw, I *info) {
    *info = 0;
    int M = *m, N = *n, R = *nrhs, LA = *lda, LB = *ldb;
    if (M < 0) { *info = -2; return; }
    if (N < 0) { *info = -3; return; }
    if (*lw == -1) { w[0] = (float)((M > N ? M : N) * (R > 1 ? R : 1) + 64); return; }
    int rows = M > N ? M : N;
    double *da = malloc(sizeof(double) * (size_t)(M ? M : 1) * (size_t)(N ? N : 1)), *db = calloc((size_t)(rows ? rows : 1) * (size_t)(R ? R : 1), sizeof(double));
    if (!da || !db) { free(da); free(db); *info = -1000; return; }
    for (int j = 0; j < N; j++) for (int i = 0; i < M; i++) da[i + (size_t)j * M] = E(a, LA, i, j);
    int brows = (*tr == 'N' || *tr == 'n') ? M : N;
    for (int j = 0; j < R; j++) for (int i = 0; i < brows; i++) db[i + (size_t)j * rows] = E(b, LB, i, j);
    gels_d(*tr, M, N, R, da, M ? M : 1, db, rows ? rows : 1, info);
    int xrows = (*tr == 'N' || *tr == 'n') ? N : M;
    if (!*info) for (int j = 0; j < R; j++) for (int i = 0; i < xrows; i++) E(b, LB, i, j) = (float)db[i + (size_t)j * rows];
    free(da); free(db);
}
