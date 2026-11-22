/*
 * mOS — mais vImage pedido pelo corpus: preencher, permutar canais, espelhar, (des)multiplicar alfa (ARGB e Planar8),
 * tabela Planar8 -> PlanarF, 16U -> 16F, convolução Planar8 (fundo, borda estendida, kernel truncado), warp afim ARGB8888,
 * YpCbCr 4:2:0 planar -> ARGB8888 (vImageConvert_YpCbCrToARGB_GenerateConversion com as matrizes 601/709 e a faixa
 * de vídeo/cheia) e o conversor genérico entre formatos de CGImage (vImageConverter + vImageConvert_AnyToAny) para
 * 8 bits (1, 3 ou 4 canais, alfa antes/depois, pré-multiplicado ou não, ordem 32 little), 16 bits e float 32.
 * Teste: tools/accelerate_vimage_test.c. Código original do mOS (MIT).
 */
#include <stddef.h>
#include <stdint.h>

void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
void *memmove(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
double floor(double);

typedef struct { void *data; unsigned long height, width, rowBytes; } vimg_t;
typedef struct { uint32_t bpc, bpp; void *space; uint32_t bitmap, version; const double *decode; int intent; } vimg_fmt_t;
#define KV_NOERR 0
#define KV_COPY_IN_PLACE 2u
#define KV_BG 4u
#define KV_EXTEND 8u
#define KV_TRUNC 16u
#define KV_TEMP 128u
#define KV_BAD_KERNEL (-21767)
#define KV_EDGE (-21768)
#define KV_NOMEM (-21771)
#define KV_NULL (-21772)
#define KV_PARAM (-21773)
#define KV_SIZE (-21774)
#define KV_FORMAT (-21778)
#define KV_OUT_OF_PLACE (-21780)
#define KV_UNSUPPORTED (-21783)
#define ROW(b, y) ((uint8_t *)(b)->data + (unsigned long)(y) * (b)->rowBytes)

static long check2(const vimg_t *s, const vimg_t *d) {
    if (!s || !d || !s->data || !d->data) return KV_NULL;
    if (s->width < d->width || s->height < d->height) return KV_SIZE;
    return 0;
}
long vImageBufferFill_ARGB8888(const vimg_t *d, const uint8_t color[4], unsigned flags) {
    (void)flags;
    if (!d || !d->data || !color) return KV_NULL;
    for (unsigned long y = 0; y < d->height; y++) for (unsigned long x = 0; x < d->width; x++) memcpy(ROW(d, y) + x * 4, color, 4);
    return 0;
}
long vImagePermuteChannels_ARGB8888(const vimg_t *s, const vimg_t *d, const uint8_t map[4], unsigned flags) {
    (void)flags;
    long e = check2(s, d);
    if (e) return e;
    if (!map || map[0] > 3 || map[1] > 3 || map[2] > 3 || map[3] > 3) return KV_PARAM;
    for (unsigned long y = 0; y < d->height; y++)
        for (unsigned long x = 0; x < d->width; x++) {
            uint8_t p[4];
            memcpy(p, ROW(s, y) + x * 4, 4);   /* em lugar funciona */
            for (int c = 0; c < 4; c++) ROW(d, y)[x * 4 + c] = p[map[c]];
        }
    return 0;
}
long vImageHorizontalReflect_ARGB8888(const vimg_t *s, const vimg_t *d, unsigned flags) {
    (void)flags;
    long e = check2(s, d);
    if (e) return e;
    unsigned long w = d->width;
    uint8_t *tmp = malloc(w * 4);
    if (!tmp) return KV_NOMEM;
    for (unsigned long y = 0; y < d->height; y++) {
        memcpy(tmp, ROW(s, y), w * 4);
        for (unsigned long x = 0; x < w; x++) memcpy(ROW(d, y) + x * 4, tmp + (w - 1 - x) * 4, 4);
    }
    free(tmp);
    return 0;
}
static uint8_t mul8(unsigned c, unsigned a) { return (uint8_t)((c * a + 127) / 255); }
long vImagePremultiplyData_ARGB8888(const vimg_t *s, const vimg_t *d, unsigned flags) {
    (void)flags;
    long e = check2(s, d);
    if (e) return e;
    for (unsigned long y = 0; y < d->height; y++)
        for (unsigned long x = 0; x < d->width; x++) {
            const uint8_t *in = ROW(s, y) + x * 4;
            uint8_t *out = ROW(d, y) + x * 4, a = in[0];
            out[1] = mul8(in[1], a); out[2] = mul8(in[2], a); out[3] = mul8(in[3], a); out[0] = a;
        }
    return 0;
}
long vImageUnpremultiplyData_ARGB8888(const vimg_t *s, const vimg_t *d, unsigned flags) {
    (void)flags;
    long e = check2(s, d);
    if (e) return e;
    for (unsigned long y = 0; y < d->height; y++)
        for (unsigned long x = 0; x < d->width; x++) {
            const uint8_t *in = ROW(s, y) + x * 4;
            uint8_t *out = ROW(d, y) + x * 4;
            unsigned a = in[0];
            for (int c = 1; c < 4; c++) { unsigned v = a ? (in[c] * 255u + a / 2) / a : 0; out[c] = (uint8_t)(v > 255 ? 255 : v); }
            out[0] = (uint8_t)a;
        }
    return 0;
}
long vImagePremultiplyData_Planar8(const vimg_t *s, const vimg_t *alpha, const vimg_t *d, unsigned flags) {
    (void)flags;
    long e = check2(s, d);
    if (e) return e;
    if (!alpha || !alpha->data) return KV_NULL;
    for (unsigned long y = 0; y < d->height; y++)
        for (unsigned long x = 0; x < d->width; x++) ROW(d, y)[x] = mul8(ROW(s, y)[x], ROW(alpha, y)[x]);
    return 0;
}
long vImageLookupTable_Planar8toPlanarF(const vimg_t *s, const vimg_t *d, const float table[256], unsigned flags) {
    (void)flags;
    long e = check2(s, d);
    if (e) return e;
    if (!table) return KV_NULL;
    for (unsigned long y = 0; y < d->height; y++)
        for (unsigned long x = 0; x < d->width; x++) ((float *)ROW(d, y))[x] = table[ROW(s, y)[x]];
    return 0;
}
/* inteiro sem sinal -> binary16 (arredondamento para o par mais próximo; acima de 65504 vira infinito) */
static uint16_t to_half(uint32_t v) {
    if (!v) return 0;
    int e = 31 - __builtin_clz(v);
    if (e >= 16) return 0x7c00;
    uint32_t m;
    if (e > 10) {
        uint32_t sh = (uint32_t)e - 10, q = v >> sh, rem = v & ((1u << sh) - 1), half = 1u << (sh - 1);
        if (rem > half || (rem == half && (q & 1))) q++;
        if (q >> 11) { q >>= 1; e++; }
        if (e >= 16) return 0x7c00;
        m = q & 0x3ff;
    } else m = (v << (10 - e)) & 0x3ff;
    return (uint16_t)((uint32_t)(e + 15) << 10 | m);
}
long vImageConvert_16Uto16F(const vimg_t *s, const vimg_t *d, unsigned flags) {
    (void)flags;
    long e = check2(s, d);
    if (e) return e;
    for (unsigned long y = 0; y < d->height; y++)
        for (unsigned long x = 0; x < d->width; x++) ((uint16_t *)ROW(d, y))[x] = to_half(((const uint16_t *)ROW(s, y))[x]);
    return 0;
}
/* convolução 8 bits: soma inteira / divisor (com arredondamento), saturada */
long vImageConvolve_Planar8(const vimg_t *s, const vimg_t *d, void *tmp, unsigned long rx, unsigned long ry, const int16_t *k,
                            uint32_t kh, uint32_t kw, int32_t divisor, uint8_t bg, unsigned flags) {
    (void)tmp;
    if (flags & KV_TEMP) return 0;
    if (!s || !d || !k || !s->data || !d->data) return KV_NULL;
    if (!(kh & 1) || !(kw & 1)) return KV_BAD_KERNEL;
    int modes = !!(flags & KV_COPY_IN_PLACE) + !!(flags & KV_BG) + !!(flags & KV_EXTEND) + !!(flags & KV_TRUNC);
    if (modes != 1) return KV_EDGE;
    if (!divisor) divisor = 1;
    long hh = kh / 2, hw = kw / 2;
    for (unsigned long y = 0; y < d->height; y++)
        for (unsigned long x = 0; x < d->width; x++) {
            long cx = (long)(x + rx), cy = (long)(y + ry);
            int edge = cx - hw < 0 || cy - hh < 0 || cx + hw >= (long)s->width || cy + hh >= (long)s->height;
            if (edge && (flags & KV_COPY_IN_PLACE)) { ROW(d, y)[x] = ROW(s, cy)[cx]; continue; }
            long sum = 0, wsum = 0;
            for (long j = -hh; j <= hh; j++)
                for (long i = -hw; i <= hw; i++) {
                    long sx = cx + i, sy = cy + j;
                    int16_t kv = k[(j + hh) * (long)kw + (i + hw)];
                    int in = sx >= 0 && sy >= 0 && sx < (long)s->width && sy < (long)s->height;
                    long px;
                    if (in) px = ROW(s, sy)[sx];
                    else if (flags & KV_BG) px = bg;
                    else if (flags & KV_EXTEND) {
                        long ex = sx < 0 ? 0 : sx >= (long)s->width ? (long)s->width - 1 : sx, ey = sy < 0 ? 0 : sy >= (long)s->height ? (long)s->height - 1 : sy;
                        px = ROW(s, ey)[ex];
                    } else continue;   /* truncado: só a parte do kernel dentro da imagem */
                    sum += px * kv;
                    wsum += kv;
                }
            long dv = divisor;
            if ((flags & KV_TRUNC) && edge) { long full = 0; for (unsigned long q = 0; q < (unsigned long)kh * kw; q++) full += k[q]; if (wsum && full) dv = divisor * wsum / full; if (!dv) dv = 1; }
            long r = sum >= 0 ? (sum + dv / 2) / dv : -((-sum + dv / 2) / dv);
            ROW(d, y)[x] = (uint8_t)(r < 0 ? 0 : r > 255 ? 255 : r);
        }
    return 0;
}
/* warp afim: a transformação leva a origem para o destino; cada destino busca a origem pela inversa (bilinear) */
typedef struct { double a, b, c, d, tx, ty; } vaff_t;
long vImageAffineWarpCG_ARGB8888(const vimg_t *s, const vimg_t *d, void *tmp, const vaff_t *t, const uint8_t bg[4], unsigned flags) {
    (void)tmp;
    if (flags & KV_TEMP) return 0;
    if (!s || !d || !t || !s->data || !d->data) return KV_NULL;
    double det = t->a * t->d - t->b * t->c;
    if (det == 0) return KV_PARAM;
    double ia = t->d / det, ib = -t->b / det, ic = -t->c / det, id = t->a / det;
    for (unsigned long y = 0; y < d->height; y++)
        for (unsigned long x = 0; x < d->width; x++) {
            double u = x + 0.5 - t->tx, v = y + 0.5 - t->ty;
            double sx = ia * u + ic * v - 0.5, sy = ib * u + id * v - 0.5;
            long x0 = (long)floor(sx), y0 = (long)floor(sy);
            double fx = sx - x0, fy = sy - y0, acc[4] = {0, 0, 0, 0};
            for (int q = 0; q < 4; q++) {
                long px = x0 + (q & 1), py = y0 + (q >> 1);
                double w = ((q & 1) ? fx : 1 - fx) * ((q >> 1) ? fy : 1 - fy);
                const uint8_t *p;
                uint8_t bgc[4] = {0, 0, 0, 0};
                if (px < 0 || py < 0 || px >= (long)s->width || py >= (long)s->height) {
                    if (flags & KV_EXTEND) { px = px < 0 ? 0 : px >= (long)s->width ? (long)s->width - 1 : px; py = py < 0 ? 0 : py >= (long)s->height ? (long)s->height - 1 : py; p = ROW(s, py) + px * 4; }
                    else p = (flags & KV_BG) && bg ? bg : bgc;
                } else p = ROW(s, py) + px * 4;
                for (int c = 0; c < 4; c++) acc[c] += w * p[c];
            }
            for (int c = 0; c < 4; c++) { double v2 = acc[c] + 0.5; ROW(d, y)[x * 4 + c] = (uint8_t)(v2 < 0 ? 0 : v2 > 255 ? 255 : v2); }
        }
    return 0;
}

/* ---------------------------------------------------------------- YpCbCr -> ARGB */
typedef struct { float Yp, Cr_R, Cr_G, Cb_G, Cb_B; } ycc_matrix_t;
typedef struct { int32_t Yp_bias, CbCr_bias, YpRangeMax, CbCrRangeMax, YpMax, YpMin, CbCrMax, CbCrMin; } ycc_range_t;
static const ycc_matrix_t m601 = {1, 1.402f, -0.714136f, -0.344136f, 1.772f};
static const ycc_matrix_t m709 = {1, 1.5748f, -0.468124f, -0.187324f, 1.8556f};
const ycc_matrix_t *const kvImage_YpCbCrToARGBMatrix_ITU_R_601_4 = &m601;
const ycc_matrix_t *const kvImage_YpCbCrToARGBMatrix_ITU_R_709_2 = &m709;
/* vImage_YpCbCrToARGB é opaco (128 bytes): aqui guarda a matriz, os desvios e as escalas */
typedef struct { uint32_t magic; float yb, cb, ys, cs, cr_r, cr_g, cb_g, cb_b; float ymin, ymax, cmin, cmax; } ycc_info_t;
long vImageConvert_YpCbCrToARGB_GenerateConversion(const ycc_matrix_t *m, const ycc_range_t *r, void *out, int in_type, int out_type, unsigned flags) {
    (void)in_type; (void)out_type; (void)flags;
    if (!m || !r || !out) return KV_NULL;
    if (!r->YpRangeMax || !r->CbCrRangeMax) return KV_PARAM;
    ycc_info_t *i = out;
    i->magic = 0x59434331;
    i->yb = (float)r->Yp_bias; i->cb = (float)r->CbCr_bias;
    i->ys = 255.0f / (float)(r->YpRangeMax - r->Yp_bias) * m->Yp;
    i->cs = 127.5f / (float)(r->CbCrRangeMax - r->CbCr_bias);
    i->cr_r = m->Cr_R; i->cr_g = m->Cr_G; i->cb_g = m->Cb_G; i->cb_b = m->Cb_B;
    i->ymin = (float)r->YpMin; i->ymax = (float)r->YpMax; i->cmin = (float)r->CbCrMin; i->cmax = (float)r->CbCrMax;
    return 0;
}
static uint8_t sat8(float v) { v += 0.5f; return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v); }
long vImageConvert_420Yp8_Cb8_Cr8ToARGB8888(const vimg_t *Y, const vimg_t *Cb, const vimg_t *Cr, const vimg_t *d, const void *info,
                                            const uint8_t map[4], uint8_t alpha, unsigned flags) {
    (void)flags;
    const ycc_info_t *i = info;
    if (!Y || !Cb || !Cr || !d || !i || !Y->data || !Cb->data || !Cr->data || !d->data) return KV_NULL;
    if (i->magic != 0x59434331) return KV_PARAM;
    for (unsigned long y = 0; y < d->height; y++)
        for (unsigned long x = 0; x < d->width; x++) {
            float yy = ROW(Y, y)[x], u = ROW(Cb, y / 2)[x / 2], v = ROW(Cr, y / 2)[x / 2];
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

/* ---------------------------------------------------------------- conversor entre formatos de CGImage */
typedef struct { int ch, bpc, alpha_pos, premul, skip, little, isfloat, bpp; } pfmt;   /* alpha_pos: -1 sem, 0 primeiro, 1 último */
typedef struct { uint32_t magic; pfmt s, d; double bg[4]; } vconv_t;
static int parse_fmt(const vimg_fmt_t *f, pfmt *p) {
    if (!f) return -1;
    uint32_t ai = f->bitmap & 0x1f, bo = f->bitmap & 0x7000;
    p->bpc = (int)f->bpc; p->bpp = (int)f->bpp; p->isfloat = (f->bitmap & 0x100) != 0;
    p->little = bo == 0x1000 || bo == 0x2000;
    p->alpha_pos = ai == 0 || ai == 5 || ai == 6 ? -1 : (ai == 1 || ai == 3) ? 1 : 0;
    p->premul = ai == 1 || ai == 2;
    p->skip = ai == 5 ? 1 : ai == 6 ? 0 : -1;   /* posição do byte ignorado */
    if (p->bpc != 8 && p->bpc != 16 && !(p->bpc == 32 && p->isfloat)) return -1;
    p->ch = p->bpp / p->bpc;
    if (p->ch != 1 && p->ch != 3 && p->ch != 4) return -1;
    if (p->ch == 4 && p->alpha_pos < 0 && p->skip < 0) return -1;
    if (p->ch != 4 && (p->alpha_pos >= 0 || p->skip >= 0)) { if (!(p->ch == 1 && ai == 7)) return -1; }
    if (ai == 7) { p->ch = 1; p->alpha_pos = 0; }   /* só alfa */
    return 0;
}
void *vImageConverter_CreateWithCGImageFormat(const vimg_fmt_t *src, const vimg_fmt_t *dst, const double *bg, unsigned flags, long *err) {
    (void)flags;
    vconv_t *c = calloc(1, sizeof *c);
    long e = !c ? KV_NOMEM : !src || !dst ? KV_NULL : parse_fmt(src, &c->s) || parse_fmt(dst, &c->d) ? KV_UNSUPPORTED : 0;
    if (err) *err = e;
    if (e) { free(c); return 0; }
    c->magic = 0x56434e56;
    if (bg) memcpy(c->bg, bg, sizeof c->bg); else c->bg[3] = 1;
    return c;
}
void vImageConverter_Release(void *c) { vconv_t *v = c; if (v && v->magic == 0x56434e56) { v->magic = 0; free(v); } }
void vImageConverter_Retain(void *c) { (void)c; }
long vImageConverter_MustOperateOutOfPlace(void *c, const vimg_t *s, const vimg_t *d, unsigned flags) {
    (void)s; (void)d; (void)flags;
    vconv_t *v = c;
    if (!v || v->magic != 0x56434e56) return KV_NULL;
    return v->s.bpp >= v->d.bpp ? 0 : KV_OUT_OF_PLACE;   /* em lugar só quando o pixel não cresce */
}
static double rd_comp(const uint8_t *p, const pfmt *f, int i) {
    if (f->bpc == 8) return p[i] / 255.0;
    if (f->bpc == 16) { uint16_t v = f->little ? (uint16_t)(p[2 * i] | p[2 * i + 1] << 8) : (uint16_t)(p[2 * i] << 8 | p[2 * i + 1]); return v / 65535.0; }
    float v; memcpy(&v, p + 4 * i, 4); return v;
}
static void wr_comp(uint8_t *p, const pfmt *f, int i, double v) {
    if (f->bpc == 32) { float x = (float)v; memcpy(p + 4 * i, &x, 4); return; }
    v = v < 0 ? 0 : v > 1 ? 1 : v;
    if (f->bpc == 8) { p[i] = (uint8_t)(v * 255 + 0.5); return; }
    uint16_t x = (uint16_t)(v * 65535 + 0.5);
    if (f->little) { p[2 * i] = (uint8_t)x; p[2 * i + 1] = (uint8_t)(x >> 8); } else { p[2 * i] = (uint8_t)(x >> 8); p[2 * i + 1] = (uint8_t)x; }
}
/* posição de cada canal (r, g, b, a) no pixel, já com a ordem 32 little (bytes invertidos) */
static void layout(const pfmt *f, int idx[4]) {
    idx[0] = idx[1] = idx[2] = idx[3] = -1;
    if (f->ch == 1) { if (f->alpha_pos == 0) idx[3] = 0; else idx[0] = idx[1] = idx[2] = 0; return; }
    if (f->ch == 3) { idx[0] = 0; idx[1] = 1; idx[2] = 2; }
    else {
        int first = f->alpha_pos == 0 || f->skip == 0;
        idx[0] = first ? 1 : 0; idx[1] = first ? 2 : 1; idx[2] = first ? 3 : 2;
        if (f->alpha_pos >= 0) idx[3] = first ? 0 : 3;
    }
    if (f->little && f->bpc == 8 && f->ch == 4) for (int c = 0; c < 4; c++) if (idx[c] >= 0) idx[c] = 3 - idx[c];
}
long vImageConvert_AnyToAny(void *c, const vimg_t *srcs, const vimg_t *dsts, void *tmp, unsigned flags) {
    (void)tmp;
    if (flags & KV_TEMP) return 0;
    vconv_t *v = c;
    if (!v || v->magic != 0x56434e56 || !srcs || !dsts || !srcs->data || !dsts->data) return KV_NULL;
    if (srcs->width < dsts->width || srcs->height < dsts->height) return KV_SIZE;
    int si[4], di[4];
    layout(&v->s, si); layout(&v->d, di);
    int sbp = v->s.bpp / 8, dbp = v->d.bpp / 8;
    uint8_t row_tmp[64];
    for (unsigned long y = 0; y < dsts->height; y++)
        for (unsigned long x = 0; x < dsts->width; x++) {
            const uint8_t *sp = ROW(srcs, y) + x * (unsigned)sbp;
            memcpy(row_tmp, sp, (size_t)sbp);   /* em lugar */
            double r = si[0] >= 0 ? rd_comp(row_tmp, &v->s, si[0]) : 0, g = si[1] >= 0 ? rd_comp(row_tmp, &v->s, si[1]) : r,
                   b = si[2] >= 0 ? rd_comp(row_tmp, &v->s, si[2]) : r, a = si[3] >= 0 ? rd_comp(row_tmp, &v->s, si[3]) : 1;
            if (v->s.premul && a > 0) { r /= a; g /= a; b /= a; }
            if (di[3] < 0 && a < 1) {   /* destino sem alfa: compõe sobre a cor de fundo */
                r = r * a + v->bg[0] * (1 - a); g = g * a + v->bg[1] * (1 - a); b = b * a + v->bg[2] * (1 - a);
            }
            uint8_t *dp = ROW(dsts, y) + x * (unsigned)dbp;
            if (v->d.ch == 1 && di[3] < 0) { wr_comp(dp, &v->d, 0, 0.2126 * r + 0.7152 * g + 0.0722 * b); continue; }   /* cinza */
            if (v->d.premul) { r *= a; g *= a; b *= a; }
            memset(dp, 0, (size_t)dbp);
            if (di[0] >= 0) wr_comp(dp, &v->d, di[0], r);
            if (di[1] >= 0) wr_comp(dp, &v->d, di[1], g);
            if (di[2] >= 0) wr_comp(dp, &v->d, di[2], b);
            if (di[3] >= 0) wr_comp(dp, &v->d, di[3], a);
            else if (v->d.skip >= 0) wr_comp(dp, &v->d, v->d.little && v->d.bpc == 8 ? 3 - (v->d.skip ? 3 : 0) : (v->d.skip ? 3 : 0), 1);
        }
    return 0;
}
