/*
 * mOS — Accelerate pedido por 2 apps do corpus (docs/lacunas-rodada-2026-11-22.md):
 *   - vImage: rotação livre (Planar8/ARGB8888) e de 90° (Planar8/16U), espelhos Planar8/16U, cisalhamento Planar8,
 *     warp afim Planar8, composição pré-multiplicada (BGRA normal; RGBA screen/multiply/lighten/darken), min/max/erosão/
 *     dilatação/convolução ARGB8888, conversões RGB888/Planar8/Planar16F/ARGB16U, preenchimento de canal Planar8,
 *     filtros de reamostragem e a ponte CVPixelBuffer <-> vImage_Buffer (formatos de 32 bits);
 *   - vDSP: vthrD, vsubD, vsaddD, vflt32, vfillD, vdivD, vasm, svesqD, mmovD, minmgv, maxvD, dotprD, distancesq;
 *     FFT complexa (fft_zip, fft_zop, fft_zopD) e real fora do lugar em double (fft_zropD); vForce vvlog1pf.
 * Teste: tools/accelerate_corpus2_test.sh. Código original do mOS (MIT).
 */
#include <stddef.h>
#include <stdint.h>

void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
void *memmove(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
double cos(double);
double sin(double);
double floor(double);
float log1pf(float);

typedef struct { void *data; unsigned long height, width, rowBytes; } vimg_t;
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
#define ROW(b, y) ((uint8_t *)(b)->data + (unsigned long)(y) * (b)->rowBytes)
static long chk(const vimg_t *s, const vimg_t *d) {
    if (!s || !d || !s->data || !d->data) return KV_NULL;
    return 0;
}
static uint8_t s8(double v) { v += 0.5; return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v); }

/* ================================================================ amostragem genérica (n canais de 8 bits) */
/* bilinear com fundo fora da imagem */
static void sample8(const vimg_t *s, int nc, double fx, double fy, const uint8_t *bg, uint8_t *out) {
    long x0 = (long)floor(fx), y0 = (long)floor(fy);
    double ax = fx - x0, ay = fy - y0;
    double acc[4] = {0, 0, 0, 0};
    for (int j = 0; j < 2; j++)
        for (int i = 0; i < 2; i++) {
            double w = (i ? ax : 1 - ax) * (j ? ay : 1 - ay);
            if (w == 0) continue;
            long x = x0 + i, y = y0 + j;
            const uint8_t *p = x >= 0 && y >= 0 && x < (long)s->width && y < (long)s->height ? ROW(s, y) + x * nc : bg;
            for (int c = 0; c < nc; c++) acc[c] += w * p[c];
        }
    for (int c = 0; c < nc; c++) out[c] = s8(acc[c]);
}

/* ---- rotação livre em torno do centro (ângulo em radianos, anti-horário como no vImage) */
static long rotate_n(const vimg_t *s, const vimg_t *d, double ang, const uint8_t *bg, int nc) {
    long e = chk(s, d);
    if (e) return e;
    double c = cos(ang), sn = sin(ang);
    double scx = s->width / 2.0, scy = s->height / 2.0, dcx = d->width / 2.0, dcy = d->height / 2.0;
    for (unsigned long y = 0; y < d->height; y++)
        for (unsigned long x = 0; x < d->width; x++) {
            double dx = x + 0.5 - dcx, dy = y + 0.5 - dcy;
            /* inversa: a imagem tem y para baixo, então a rotação anti-horária na tela é horária nas coordenadas */
            double sx = c * dx - sn * dy + scx - 0.5, sy = sn * dx + c * dy + scy - 0.5;
            sample8(s, nc, sx, sy, bg, ROW(d, y) + x * nc);
        }
    return 0;
}
long vImageRotate_Planar8(const vimg_t *s, const vimg_t *d, void *tmp, float ang, uint8_t bg, unsigned flags) {
    (void)tmp;
    if (flags & KV_TEMP) return 0;
    return rotate_n(s, d, ang, &bg, 1);
}
long vImageRotate_ARGB8888(const vimg_t *s, const vimg_t *d, void *tmp, float ang, const uint8_t bg[4], unsigned flags) {
    (void)tmp;
    if (flags & KV_TEMP) return 0;
    static const uint8_t z[4];
    return rotate_n(s, d, ang, bg ? bg : z, 4);
}

/* ---- rotação de 90° (0, 1, 2, 3 = 0°, 90°, 180°, 270° horário, como o vImageRotate90_ARGB8888 do mOS) */
static long rot90_n(const vimg_t *s, const vimg_t *d, uint8_t rot, int bpp) {
    long e = chk(s, d);
    if (e) return e;
    if (rot > 3) return KV_PARAM;
    for (unsigned long y = 0; y < d->height; y++)
        for (unsigned long x = 0; x < d->width; x++) {
            unsigned long sx, sy;
            if (rot == 0) { sx = x; sy = y; }
            else if (rot == 1) { sx = y; sy = s->height - 1 - x; }
            else if (rot == 2) { sx = s->width - 1 - x; sy = s->height - 1 - y; }
            else { sx = s->width - 1 - y; sy = x; }
            if (sx >= s->width || sy >= s->height) continue;
            memcpy(ROW(d, y) + x * bpp, ROW(s, sy) + sx * bpp, (size_t)bpp);
        }
    return 0;
}
long vImageRotate90_Planar8(const vimg_t *s, const vimg_t *d, uint8_t rot, uint8_t bg, unsigned flags) { (void)bg; (void)flags; return rot90_n(s, d, rot, 1); }
long vImageRotate90_Planar16U(const vimg_t *s, const vimg_t *d, uint8_t rot, uint16_t bg, unsigned flags) { (void)bg; (void)flags; return rot90_n(s, d, rot, 2); }

/* ---- espelhos (em lugar funciona) */
static long hreflect(const vimg_t *s, const vimg_t *d, int bpp) {
    long e = chk(s, d);
    if (e) return e;
    if (s->width != d->width || s->height != d->height) return KV_SIZE;
    uint8_t t[16];
    for (unsigned long y = 0; y < d->height; y++) {
        uint8_t *in = ROW(s, y), *out = ROW(d, y);
        unsigned long w = d->width;
        for (unsigned long x = 0; x < (w + 1) / 2; x++) {
            unsigned long xr = w - 1 - x;
            memcpy(t, in + x * bpp, (size_t)bpp);
            memmove(out + x * bpp, in + xr * bpp, (size_t)bpp);
            memcpy(out + xr * bpp, t, (size_t)bpp);
        }
    }
    return 0;
}
static long vreflect(const vimg_t *s, const vimg_t *d, int bpp) {
    long e = chk(s, d);
    if (e) return e;
    if (s->width != d->width || s->height != d->height) return KV_SIZE;
    size_t n = d->width * (size_t)bpp;
    uint8_t *t = malloc(n);
    if (!t) return KV_NOMEM;
    for (unsigned long y = 0; y < (d->height + 1) / 2; y++) {
        unsigned long yr = d->height - 1 - y;
        memcpy(t, ROW(s, y), n);
        memmove(ROW(d, y), ROW(s, yr), n);
        memcpy(ROW(d, yr), t, n);
    }
    free(t);
    return 0;
}
long vImageHorizontalReflect_Planar8(const vimg_t *s, const vimg_t *d, unsigned flags) { (void)flags; return hreflect(s, d, 1); }
long vImageHorizontalReflect_Planar16U(const vimg_t *s, const vimg_t *d, unsigned flags) { (void)flags; return hreflect(s, d, 2); }
long vImageVerticalReflect_Planar16U(const vimg_t *s, const vimg_t *d, unsigned flags) { (void)flags; return vreflect(s, d, 2); }

/* ---- cisalhamento e warp afim (Planar8, bilinear, fundo fora da imagem) */
typedef struct { uint32_t magic; float scale; } rfilter_t;
void *vImageNewResamplingFilter(float scale, unsigned flags) {
    (void)flags;
    if (scale == 0) return 0;
    rfilter_t *f = malloc(sizeof *f);
    if (f) { f->magic = 0x52464c54; f->scale = scale; }
    return f;
}
void vImageDestroyResamplingFilter(void *f) { free(f); }
long vImageHorizontalShear_Planar8(const vimg_t *s, const vimg_t *d, unsigned long ox, unsigned long oy, float xt, float slope,
                                   const void *filter, uint8_t bg, unsigned flags) {
    (void)filter; (void)flags;
    long e = chk(s, d);
    if (e) return e;
    for (unsigned long y = 0; y < d->height; y++)
        for (unsigned long x = 0; x < d->width; x++) {
            double sy = (double)(y + oy), sx = (double)(x + ox) - xt - slope * sy;
            sample8(s, 1, sx, sy, &bg, ROW(d, y) + x);
        }
    return 0;
}
long vImageVerticalShear_Planar8(const vimg_t *s, const vimg_t *d, unsigned long ox, unsigned long oy, float yt, float slope,
                                 const void *filter, uint8_t bg, unsigned flags) {
    (void)filter; (void)flags;
    long e = chk(s, d);
    if (e) return e;
    for (unsigned long y = 0; y < d->height; y++)
        for (unsigned long x = 0; x < d->width; x++) {
            double sx = (double)(x + ox), sy = (double)(y + oy) - yt - slope * sx;
            sample8(s, 1, sx, sy, &bg, ROW(d, y) + x);
        }
    return 0;
}
typedef struct { double a, b, c, d, tx, ty; } cgaffine_t;
/* a transformação leva a fonte ao destino: inverte para amostrar */
long vImageAffineWarpCG_Planar8(const vimg_t *s, const vimg_t *d, void *tmp, const cgaffine_t *t, uint8_t bg, unsigned flags) {
    (void)tmp;
    if (flags & KV_TEMP) return 0;
    long e = chk(s, d);
    if (e) return e;
    if (!t) return KV_NULL;
    double det = t->a * t->d - t->b * t->c;
    if (det == 0) return KV_PARAM;
    double ia = t->d / det, ib = -t->b / det, ic = -t->c / det, id = t->a / det;
    for (unsigned long y = 0; y < d->height; y++)
        for (unsigned long x = 0; x < d->width; x++) {
            double u = x - t->tx, v = y - t->ty;
            sample8(s, 1, u * ia + v * ic, u * ib + v * id, &bg, ROW(d, y) + x);
        }
    return 0;
}

/* ================================================================ composição com alfa pré-multiplicado */
enum { BL_NORMAL, BL_SCREEN, BL_MULTIPLY, BL_LIGHTEN, BL_DARKEN };
static long blend(const vimg_t *top, const vimg_t *bot, const vimg_t *d, int ai, int mode) {
    if (!top || !bot || !d || !top->data || !bot->data || !d->data) return KV_NULL;
    for (unsigned long y = 0; y < d->height; y++)
        for (unsigned long x = 0; x < d->width; x++) {
            const uint8_t *t = ROW(top, y) + x * 4, *b = ROW(bot, y) + x * 4;
            uint8_t *o = ROW(d, y) + x * 4;
            double sa = t[ai] / 255.0, da = b[ai] / 255.0, r[4];
            for (int c = 0; c < 4; c++) {
                double sc = t[c] / 255.0, dc = b[c] / 255.0, v;
                if (c == ai) v = sa + da - sa * da;
                else switch (mode) {
                    case BL_SCREEN: v = sc + dc - sc * dc; break;
                    case BL_MULTIPLY: v = sc * dc + sc * (1 - da) + dc * (1 - sa); break;
                    case BL_LIGHTEN: v = (sc * da > dc * sa ? sc * da : dc * sa) + sc * (1 - da) + dc * (1 - sa); break;
                    case BL_DARKEN: v = (sc * da < dc * sa ? sc * da : dc * sa) + sc * (1 - da) + dc * (1 - sa); break;
                    default: v = sc + dc * (1 - sa);
                }
                r[c] = v * 255;
            }
            for (int c = 0; c < 4; c++) o[c] = s8(r[c]);
        }
    return 0;
}
long vImagePremultipliedAlphaBlend_BGRA8888(const vimg_t *t, const vimg_t *b, const vimg_t *d, unsigned f) { (void)f; return blend(t, b, d, 3, BL_NORMAL); }
long vImagePremultipliedAlphaBlendScreen_RGBA8888(const vimg_t *t, const vimg_t *b, const vimg_t *d, unsigned f) { (void)f; return blend(t, b, d, 3, BL_SCREEN); }
long vImagePremultipliedAlphaBlendMultiply_RGBA8888(const vimg_t *t, const vimg_t *b, const vimg_t *d, unsigned f) { (void)f; return blend(t, b, d, 3, BL_MULTIPLY); }
long vImagePremultipliedAlphaBlendLighten_RGBA8888(const vimg_t *t, const vimg_t *b, const vimg_t *d, unsigned f) { (void)f; return blend(t, b, d, 3, BL_LIGHTEN); }
long vImagePremultipliedAlphaBlendDarken_RGBA8888(const vimg_t *t, const vimg_t *b, const vimg_t *d, unsigned f) { (void)f; return blend(t, b, d, 3, BL_DARKEN); }

long vImageOverwriteChannelsWithScalar_Planar8(uint8_t v, const vimg_t *d, unsigned flags) {
    (void)flags;
    if (!d || !d->data) return KV_NULL;
    for (unsigned long y = 0; y < d->height; y++) memset(ROW(d, y), v, d->width);
    return 0;
}

/* ================================================================ morfologia e convolução ARGB8888 (borda estendida) */
static const uint8_t *px_ext(const vimg_t *s, long x, long y) {
    x = x < 0 ? 0 : x >= (long)s->width ? (long)s->width - 1 : x;
    y = y < 0 ? 0 : y >= (long)s->height ? (long)s->height - 1 : y;
    return ROW(s, y) + x * 4;
}
/* op 0 min, 1 max (kernel retangular), 2 erosão (min de pixel - k... ver abaixo), 3 dilatação */
static long morph(const vimg_t *s, const vimg_t *d, unsigned long ox, unsigned long oy, const uint8_t *k, unsigned long kh, unsigned long kw, int op) {
    long e = chk(s, d);
    if (e) return e;
    if (!(kh & 1) || !(kw & 1)) return KV_BAD_KERNEL;
    for (unsigned long y = 0; y < d->height; y++)
        for (unsigned long x = 0; x < d->width; x++) {
            int best[4];
            for (int c = 0; c < 4; c++) best[c] = (op == 0 || op == 2) ? 1 << 20 : -(1 << 20);
            for (unsigned long j = 0; j < kh; j++)
                for (unsigned long i = 0; i < kw; i++) {
                    const uint8_t *p = px_ext(s, (long)(x + ox + i) - (long)(kw / 2), (long)(y + oy + j) - (long)(kh / 2));
                    int kv = k ? k[j * kw + i] : 0;
                    for (int c = 0; c < 4; c++) {
                        /* erosão: min(pixel - k); dilatação: max(pixel - k) (com k = 0 viram mín/máx planos) */
                        int v = (int)p[c] - kv;
                        if (op == 0 || op == 2) { if (v < best[c]) best[c] = v; }
                        else if (v > best[c]) best[c] = v;
                    }
                }
            uint8_t *o = ROW(d, y) + x * 4;
            for (int c = 0; c < 4; c++) o[c] = (uint8_t)(best[c] < 0 ? 0 : best[c] > 255 ? 255 : best[c]);
        }
    return 0;
}
long vImageMin_ARGB8888(const vimg_t *s, const vimg_t *d, void *tmp, unsigned long ox, unsigned long oy, unsigned long kh, unsigned long kw, unsigned flags) {
    (void)tmp;
    if (flags & KV_TEMP) return 0;
    return morph(s, d, ox, oy, 0, kh, kw, 0);
}
long vImageMax_ARGB8888(const vimg_t *s, const vimg_t *d, void *tmp, unsigned long ox, unsigned long oy, unsigned long kh, unsigned long kw, unsigned flags) {
    (void)tmp;
    if (flags & KV_TEMP) return 0;
    return morph(s, d, ox, oy, 0, kh, kw, 1);
}
long vImageErode_ARGB8888(const vimg_t *s, const vimg_t *d, unsigned long ox, unsigned long oy, const uint8_t *k, unsigned long kh, unsigned long kw, unsigned flags) {
    (void)flags;
    if (!k) return KV_NULL;
    return morph(s, d, ox, oy, k, kh, kw, 2);
}
long vImageDilate_ARGB8888(const vimg_t *s, const vimg_t *d, unsigned long ox, unsigned long oy, const uint8_t *k, unsigned long kh, unsigned long kw, unsigned flags) {
    (void)flags;
    if (!k) return KV_NULL;
    return morph(s, d, ox, oy, k, kh, kw, 3);
}
long vImageConvolve_ARGB8888(const vimg_t *s, const vimg_t *d, void *tmp, unsigned long rx, unsigned long ry, const int16_t *k,
                             uint32_t kh, uint32_t kw, int32_t divisor, const uint8_t bg[4], unsigned flags) {
    (void)tmp;
    if (flags & KV_TEMP) return 0;
    long e = chk(s, d);
    if (e) return e;
    if (!k) return KV_NULL;
    if (!(kh & 1) || !(kw & 1)) return KV_BAD_KERNEL;
    int modes = !!(flags & KV_COPY_IN_PLACE) + !!(flags & KV_BG) + !!(flags & KV_EXTEND) + !!(flags & KV_TRUNC);
    if (modes != 1) return KV_EDGE;
    if (!divisor) divisor = 1;
    static const uint8_t z[4];
    if (!bg) bg = z;
    long hh = kh / 2, hw = kw / 2;
    for (unsigned long y = 0; y < d->height; y++)
        for (unsigned long x = 0; x < d->width; x++) {
            long cx = (long)(x + rx), cy = (long)(y + ry);
            uint8_t *o = ROW(d, y) + x * 4;
            int edge = cx - hw < 0 || cy - hh < 0 || cx + hw >= (long)s->width || cy + hh >= (long)s->height;
            if (edge && (flags & KV_COPY_IN_PLACE)) { memcpy(o, px_ext(s, cx, cy), 4); continue; }
            long sum[4] = {0, 0, 0, 0}, wsum = 0;
            for (long j = -hh; j <= hh; j++)
                for (long i = -hw; i <= hw; i++) {
                    long sx = cx + i, sy = cy + j;
                    int16_t kv = k[(j + hh) * (long)kw + (i + hw)];
                    int in = sx >= 0 && sy >= 0 && sx < (long)s->width && sy < (long)s->height;
                    const uint8_t *p;
                    if (in) p = ROW(s, sy) + sx * 4;
                    else if (flags & KV_BG) p = bg;
                    else if (flags & KV_EXTEND) p = px_ext(s, sx, sy);
                    else continue;
                    for (int c = 0; c < 4; c++) sum[c] += (long)p[c] * kv;
                    wsum += kv;
                }
            long div = divisor;
            if ((flags & KV_TRUNC) && edge && wsum) div = wsum;
            for (int c = 0; c < 4; c++) {
                long v = sum[c] >= 0 ? (sum[c] + div / 2) / div : -((-sum[c] + div / 2) / div);
                o[c] = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
            }
        }
    return 0;
}

/* ================================================================ conversões de formato */
long vImageConvert_RGB888toPlanar8(const vimg_t *s, const vimg_t *r, const vimg_t *g, const vimg_t *b, unsigned flags) {
    (void)flags;
    if (!s || !r || !g || !b || !s->data || !r->data || !g->data || !b->data) return KV_NULL;
    for (unsigned long y = 0; y < r->height; y++)
        for (unsigned long x = 0; x < r->width; x++) {
            const uint8_t *p = ROW(s, y) + x * 3;
            ROW(r, y)[x] = p[0]; ROW(g, y)[x] = p[1]; ROW(b, y)[x] = p[2];
        }
    return 0;
}
/* RGB888 + alfa (plano ou constante) -> 4 canais na ordem pedida; premultiply multiplica a cor pelo alfa */
static long rgb_to_4(const vimg_t *s, const vimg_t *sa, uint8_t alpha, const vimg_t *d, int premul, const int ord[4]) {
    long e = chk(s, d);
    if (e) return e;
    for (unsigned long y = 0; y < d->height; y++)
        for (unsigned long x = 0; x < d->width; x++) {
            const uint8_t *p = ROW(s, y) + x * 3;
            uint8_t a = sa && sa->data ? ROW(sa, y)[x] : alpha;
            uint8_t v[4] = {p[0], p[1], p[2], a};      /* r g b a */
            if (premul) for (int c = 0; c < 3; c++) v[c] = (uint8_t)((v[c] * a + 127) / 255);
            uint8_t *o = ROW(d, y) + x * 4;
            for (int c = 0; c < 4; c++) o[c] = v[ord[c]];
        }
    return 0;
}
static const int ORD_RGBA[4] = {0, 1, 2, 3}, ORD_BGRA[4] = {2, 1, 0, 3}, ORD_ARGB[4] = {3, 0, 1, 2};
long vImageConvert_RGB888toRGBA8888(const vimg_t *s, const vimg_t *sa, uint8_t a, const vimg_t *d, int pm, unsigned f) { (void)f; return rgb_to_4(s, sa, a, d, pm, ORD_RGBA); }
long vImageConvert_RGB888toBGRA8888(const vimg_t *s, const vimg_t *sa, uint8_t a, const vimg_t *d, int pm, unsigned f) { (void)f; return rgb_to_4(s, sa, a, d, pm, ORD_BGRA); }
long vImageConvert_RGB888toARGB8888(const vimg_t *s, const vimg_t *sa, uint8_t a, const vimg_t *d, int pm, unsigned f) { (void)f; return rgb_to_4(s, sa, a, d, pm, ORD_ARGB); }
long vImageConvert_BGRA8888toRGB888(const vimg_t *s, const vimg_t *d, unsigned flags) {
    (void)flags;
    long e = chk(s, d);
    if (e) return e;
    for (unsigned long y = 0; y < d->height; y++)
        for (unsigned long x = 0; x < d->width; x++) {
            const uint8_t *p = ROW(s, y) + x * 4;
            uint8_t *o = ROW(d, y) + x * 3;
            uint8_t b = p[0], g = p[1], r = p[2];
            o[0] = r; o[1] = g; o[2] = b;
        }
    return 0;
}
long vImageConvert_Planar8toARGB8888(const vimg_t *a, const vimg_t *r, const vimg_t *g, const vimg_t *b, const vimg_t *d, unsigned flags) {
    (void)flags;
    if (!a || !r || !g || !b || !d || !a->data || !r->data || !g->data || !b->data || !d->data) return KV_NULL;
    for (unsigned long y = 0; y < d->height; y++)
        for (unsigned long x = 0; x < d->width; x++) {
            uint8_t *o = ROW(d, y) + x * 4;
            o[0] = ROW(a, y)[x]; o[1] = ROW(r, y)[x]; o[2] = ROW(g, y)[x]; o[3] = ROW(b, y)[x];
        }
    return 0;
}
static float h2f(uint16_t h) {
    uint32_t s = (uint32_t)(h & 0x8000) << 16, e = (h >> 10) & 0x1f, m = h & 0x3ff, b;
    if (e == 0) {
        if (!m) b = s;
        else { e = 127 - 15 + 1; while (!(m & 0x400)) { m <<= 1; e--; } m &= 0x3ff; b = s | (e << 23) | (m << 13); }
    } else if (e == 31) b = s | 0x7f800000u | (m << 13);
    else b = s | ((e + 127 - 15) << 23) | (m << 13);
    float f; memcpy(&f, &b, 4); return f;
}
long vImageConvert_Planar16FtoPlanar8(const vimg_t *s, const vimg_t *d, unsigned flags) {
    (void)flags;
    long e = chk(s, d);
    if (e) return e;
    for (unsigned long y = 0; y < d->height; y++)
        for (unsigned long x = 0; x < d->width; x++) {
            float v = h2f(((const uint16_t *)ROW(s, y))[x]);
            if (v != v) v = 0;
            ROW(d, y)[x] = s8((double)v * 255.0);
        }
    return 0;
}
/* 16 bits -> 8 bits com permutação; copyMask: bit 3 = canal 0 ... bit 0 = canal 3 recebe a cor de fundo */
long vImageConvert_ARGB16UToARGB8888(const vimg_t *s, const vimg_t *d, const uint8_t map[4], uint8_t mask, const uint8_t bg[4], unsigned flags) {
    (void)flags;
    long e = chk(s, d);
    if (e) return e;
    for (unsigned long y = 0; y < d->height; y++)
        for (unsigned long x = 0; x < d->width; x++) {
            const uint16_t *p = (const uint16_t *)ROW(s, y) + x * 4;
            uint8_t *o = ROW(d, y) + x * 4;
            for (int c = 0; c < 4; c++) {
                if ((mask >> (3 - c)) & 1) { o[c] = bg ? bg[c] : 0; continue; }
                uint32_t v = p[map ? (map[c] & 3) : c];
                o[c] = (uint8_t)((v * 255 + 32767) / 65535);
            }
        }
    return 0;
}

/* ================================================================ CVPixelBuffer <-> vImage_Buffer (32 bits) */
typedef struct { uint32_t bpc, bpp; void *space; uint32_t bitmap, version; const double *decode; int intent; } vimg_fmt_t;
void *vImageConverter_CreateWithCGImageFormat(const vimg_fmt_t *src, const vimg_fmt_t *dst, const double *bg, unsigned flags, long *err);
void vImageConverter_Release(void *c);
long vImageConvert_AnyToAny(void *c, const vimg_t *s, const vimg_t *d, void *tmp, unsigned flags);
long vImageBuffer_Init(vimg_t *b, unsigned long h, unsigned long w, uint32_t bits, unsigned flags);
int CVPixelBufferLockBaseAddress(void *p, uint64_t f);
int CVPixelBufferUnlockBaseAddress(void *p, uint64_t f);
void *CVPixelBufferGetBaseAddress(void *p);
uint64_t CVPixelBufferGetBytesPerRow(void *p);
uint64_t CVPixelBufferGetWidth(void *p);
uint64_t CVPixelBufferGetHeight(void *p);
uint32_t CVPixelBufferGetPixelFormatType(void *p);
void *CGColorSpaceRetain(void *s);
void CGColorSpaceRelease(void *s);
void *CGColorSpaceCreateDeviceRGB(void);

typedef struct { uint32_t magic, fourcc; void *space; int opaque; } cvfmt_t;
#define CVF_MAGIC 0x43564654u
static cvfmt_t *cvf(const void *p) { return p && ((const cvfmt_t *)p)->magic == CVF_MAGIC ? (cvfmt_t *)p : 0; }
void *vImageCVImageFormat_Create(uint32_t fourcc, const void *matrix, const void *siting, void *space, int opaque) {
    (void)matrix; (void)siting;
    cvfmt_t *f = calloc(1, sizeof *f);
    if (!f) return 0;
    f->magic = CVF_MAGIC; f->fourcc = fourcc; f->opaque = opaque;
    f->space = space ? CGColorSpaceRetain(space) : 0;
    return f;
}
void *vImageCVImageFormat_CreateWithCVPixelBuffer(void *pb) { return pb ? vImageCVImageFormat_Create(CVPixelBufferGetPixelFormatType(pb), 0, 0, 0, 0) : 0; }
void vImageCVImageFormat_Release(void *p) { cvfmt_t *f = cvf(p); if (f) { if (f->space) CGColorSpaceRelease(f->space); f->magic = 0; free(f); } }
long vImageCVImageFormat_SetColorSpace(void *p, void *space) {
    cvfmt_t *f = cvf(p);
    if (!f) return KV_NULL;
    if (f->space) CGColorSpaceRelease(f->space);
    f->space = space ? CGColorSpaceRetain(space) : 0;
    return 0;
}
/* o formato CG equivalente a um fourcc de 32 bits do CoreVideo */
static int cg_of_fourcc(uint32_t fc, vimg_fmt_t *g) {
    memset(g, 0, sizeof *g);
    g->bpc = 8; g->bpp = 32;
    switch (fc) {
    case 0x42475241: g->bitmap = 2 | 0x2000; return 0;      /* 'BGRA': pré-multiplicado primeiro, 32 little */
    case 0x00000020: g->bitmap = 2; return 0;               /* kCVPixelFormatType_32ARGB */
    case 0x52474241: g->bitmap = 1; return 0;               /* 'RGBA': pré-multiplicado por último */
    case 0x41424752: g->bitmap = 1 | 0x2000; return 0;      /* 'ABGR' */
    }
    return -1;
}
void *vImageConverter_CreateForCVToCGImageFormat(const void *src, const vimg_fmt_t *dst, const double *bg, unsigned flags, long *err) {
    cvfmt_t *f = cvf(src);
    vimg_fmt_t g;
    if (!f || !dst) { if (err) *err = KV_NULL; return 0; }
    if (cg_of_fourcc(f->fourcc, &g)) { if (err) *err = KV_FORMAT; return 0; }
    return vImageConverter_CreateWithCGImageFormat(&g, dst, bg, flags, err);
}
void *vImageConverter_CreateForCGToCVImageFormat(const vimg_fmt_t *src, const void *dst, const double *bg, unsigned flags, long *err) {
    cvfmt_t *f = cvf(dst);
    vimg_fmt_t g;
    if (!f || !src) { if (err) *err = KV_NULL; return 0; }
    if (cg_of_fourcc(f->fourcc, &g)) { if (err) *err = KV_FORMAT; return 0; }
    return vImageConverter_CreateWithCGImageFormat(src, &g, bg, flags, err);
}
#define KV_NOALLOC 512u
long vImageBuffer_InitWithCVPixelBuffer(vimg_t *buf, const vimg_fmt_t *dfmt, void *pb, void *cvformat, const double *bg, unsigned flags) {
    if (!buf || !dfmt || !pb) return KV_NULL;
    (void)cvformat;
    vimg_fmt_t g;
    if (cg_of_fourcc(CVPixelBufferGetPixelFormatType(pb), &g)) return KV_FORMAT;
    long e = 0;
    void *conv = vImageConverter_CreateWithCGImageFormat(&g, dfmt, bg, 0, &e);
    if (!conv) return e ? e : KV_FORMAT;
    CVPixelBufferLockBaseAddress(pb, 1);
    vimg_t src = {CVPixelBufferGetBaseAddress(pb), CVPixelBufferGetHeight(pb), CVPixelBufferGetWidth(pb), CVPixelBufferGetBytesPerRow(pb)};
    if (!(flags & KV_NOALLOC)) e = vImageBuffer_Init(buf, src.height, src.width, dfmt->bpp, 0);
    if (!e) e = src.data ? vImageConvert_AnyToAny(conv, &src, buf, 0, 0) : KV_NULL;
    CVPixelBufferUnlockBaseAddress(pb, 1);
    vImageConverter_Release(conv);
    return e;
}
long vImageBuffer_CopyToCVPixelBuffer(const vimg_t *buf, const vimg_fmt_t *sfmt, void *pb, void *cvformat, const double *bg, unsigned flags) {
    (void)cvformat; (void)flags;
    if (!buf || !sfmt || !pb) return KV_NULL;
    vimg_fmt_t g;
    if (cg_of_fourcc(CVPixelBufferGetPixelFormatType(pb), &g)) return KV_FORMAT;
    long e = 0;
    void *conv = vImageConverter_CreateWithCGImageFormat(sfmt, &g, bg, 0, &e);
    if (!conv) return e ? e : KV_FORMAT;
    CVPixelBufferLockBaseAddress(pb, 0);
    vimg_t dst = {CVPixelBufferGetBaseAddress(pb), CVPixelBufferGetHeight(pb), CVPixelBufferGetWidth(pb), CVPixelBufferGetBytesPerRow(pb)};
    if (dst.width > buf->width) dst.width = buf->width;
    if (dst.height > buf->height) dst.height = buf->height;
    e = dst.data ? vImageConvert_AnyToAny(conv, buf, &dst, 0, 0) : KV_NULL;
    CVPixelBufferUnlockBaseAddress(pb, 0);
    vImageConverter_Release(conv);
    return e;
}

/* ================================================================ vDSP */
typedef long vstride;
typedef unsigned long vlen;
void vDSP_vthrD(const double *a, vstride ia, const double *b, double *c, vstride ic, vlen n) { for (vlen i = 0; i < n; i++) { double v = a[i * ia]; c[i * ic] = v >= *b ? v : *b; } }
void vDSP_vsubD(const double *b, vstride ib, const double *a, vstride ia, double *c, vstride ic, vlen n) { for (vlen i = 0; i < n; i++) c[i * ic] = a[i * ia] - b[i * ib]; }
void vDSP_vsaddD(const double *a, vstride ia, const double *b, double *c, vstride ic, vlen n) { for (vlen i = 0; i < n; i++) c[i * ic] = a[i * ia] + *b; }
void vDSP_vflt32(const int32_t *a, vstride ia, float *c, vstride ic, vlen n) { for (vlen i = 0; i < n; i++) c[i * ic] = (float)a[i * ia]; }
void vDSP_vfillD(const double *a, double *c, vstride ic, vlen n) { for (vlen i = 0; i < n; i++) c[i * ic] = *a; }
void vDSP_vdivD(const double *b, vstride ib, const double *a, vstride ia, double *c, vstride ic, vlen n) { for (vlen i = 0; i < n; i++) c[i * ic] = a[i * ia] / b[i * ib]; }
void vDSP_vasm(const float *a, vstride ia, const float *b, vstride ib, const float *c, float *d, vstride id, vlen n) { for (vlen i = 0; i < n; i++) d[i * id] = (a[i * ia] + b[i * ib]) * *c; }
void vDSP_svesqD(const double *a, vstride ia, double *c, vlen n) { double s = 0; for (vlen i = 0; i < n; i++) s += a[i * ia] * a[i * ia]; *c = s; }
/* submatriz M colunas x N linhas; TA e TC são as larguras (colunas) de A e C */
void vDSP_mmovD(const double *a, double *c, vlen m, vlen n, vlen ta, vlen tc) { for (vlen r = 0; r < n; r++) memmove(c + r * tc, a + r * ta, m * sizeof(double)); }
void vDSP_minmgv(const float *a, vstride ia, float *c, vlen n) {
    float best = 1.0f / 0.0f;
    for (vlen i = 0; i < n; i++) { float v = a[i * ia] < 0 ? -a[i * ia] : a[i * ia]; if (v < best) best = v; }
    *c = best;
}
void vDSP_maxvD(const double *a, vstride ia, double *c, vlen n) { double best = -1.0 / 0.0; for (vlen i = 0; i < n; i++) if (a[i * ia] > best) best = a[i * ia]; *c = best; }
void vDSP_dotprD(const double *a, vstride ia, const double *b, vstride ib, double *c, vlen n) { double s = 0; for (vlen i = 0; i < n; i++) s += a[i * ia] * b[i * ib]; *c = s; }
void vDSP_distancesq(const float *a, vstride ia, const float *b, vstride ib, float *c, vlen n) {
    double s = 0;
    for (vlen i = 0; i < n; i++) { double d = (double)a[i * ia] - b[i * ib]; s += d * d; }
    *c = (float)s;
}
void vvlog1pf(float *y, const float *x, const int *n) { int c = n && *n > 0 ? *n : 0; for (int i = 0; i < c; i++) y[i] = log1pf(x[i]); }

/* ---- FFT complexa radix-2 (direção +1 = direta com e^-i, -1 = inversa), sem escala, como o vDSP */
typedef struct { float *realp, *imagp; } split_t;
typedef struct { double *realp, *imagp; } splitd_t;
#define PI2 6.283185307179586
static void cfft(double *re, double *im, unsigned long n, int sign) {
    for (unsigned long i = 1, j = 0; i < n; i++) {
        unsigned long bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { double t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
    }
    for (unsigned long len = 2; len <= n; len <<= 1)
        for (unsigned long k = 0; k < len / 2; k++) {
            double ang = sign * PI2 * (double)k / (double)len, cr = cos(ang), ci = sin(ang);
            for (unsigned long i = 0; i < n; i += len) {
                unsigned long a = i + k, b = a + len / 2;
                double tr = re[b] * cr - im[b] * ci, ti = re[b] * ci + im[b] * cr;
                re[b] = re[a] - tr; im[b] = im[a] - ti;
                re[a] += tr; im[a] += ti;
            }
        }
}
static int fft_buf(unsigned long log2n, double **re, double **im) {
    unsigned long n = 1ul << log2n;
    *re = malloc(sizeof(double) * n); *im = malloc(sizeof(double) * n);
    if (!*re || !*im) { free(*re); free(*im); return -1; }
    return 0;
}
void vDSP_fft_zop(void *setup, const split_t *a, vstride ia, const split_t *c, vstride ic, unsigned long log2n, int dir) {
    (void)setup;
    if (!a || !c) return;
    unsigned long n = 1ul << log2n;
    double *re, *im;
    if (fft_buf(log2n, &re, &im)) return;
    for (unsigned long k = 0; k < n; k++) { re[k] = a->realp[k * ia]; im[k] = a->imagp[k * ia]; }
    cfft(re, im, n, dir >= 0 ? -1 : 1);
    for (unsigned long k = 0; k < n; k++) { c->realp[k * ic] = (float)re[k]; c->imagp[k * ic] = (float)im[k]; }
    free(re); free(im);
}
void vDSP_fft_zip(void *setup, const split_t *c, vstride ic, unsigned long log2n, int dir) { vDSP_fft_zop(setup, c, ic, c, ic, log2n, dir); }
void vDSP_fft_zopD(void *setup, const splitd_t *a, vstride ia, const splitd_t *c, vstride ic, unsigned long log2n, int dir) {
    (void)setup;
    if (!a || !c) return;
    unsigned long n = 1ul << log2n;
    double *re, *im;
    if (fft_buf(log2n, &re, &im)) return;
    for (unsigned long k = 0; k < n; k++) { re[k] = a->realp[k * ia]; im[k] = a->imagp[k * ia]; }
    cfft(re, im, n, dir >= 0 ? -1 : 1);
    for (unsigned long k = 0; k < n; k++) { c->realp[k * ic] = re[k]; c->imagp[k * ic] = im[k]; }
    free(re); free(im);
}
void vDSP_fft_zipD(void *setup, const splitd_t *c, vstride ic, unsigned long log2n, int dir) { vDSP_fft_zopD(setup, c, ic, c, ic, log2n, dir); }
void vDSP_fft_zripD(void *setup, const splitd_t *c, vstride ic, unsigned long log2n, int dir);
void vDSP_fft_zropD(void *setup, const splitd_t *a, vstride ia, const splitd_t *c, vstride ic, unsigned long log2n, int dir) {
    if (!a || !c || log2n < 1) return;
    unsigned long h = 1ul << (log2n - 1);
    for (unsigned long k = 0; k < h; k++) { c->realp[k * ic] = a->realp[k * ia]; c->imagp[k * ic] = a->imagp[k * ia]; }
    vDSP_fft_zripD(setup, c, ic, log2n, dir);
}
