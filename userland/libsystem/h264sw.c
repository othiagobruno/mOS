/*
 * Codificador H.264 intra I_PCM, 4:2:0, 8 bits. A decodificação (h264_decode_nals/avcc) usa o OpenH264
 * corrigido pela casca cx_h264_* (host-audio/h264_glue.cpp).
 * Entrada e saída em I420. O access unit sai em AVCC (sample) e Annex B.
 *
 * O código daqui é original do mOS (MIT). Não há x264, x265, libde265 nem FFmpeg no fonte.
 */
#include "h264sw.h"
#include <stdlib.h>
#include <string.h>

/* ---- escrita de bits ---- */
static int clip8(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }
typedef struct { uint8_t *b; int cap, bits; } bw;
static int bw_need(bw *w, int add) {
    int nbytes = (w->bits + add + 7) >> 3;
    if (nbytes <= w->cap) return 0;
    int ncap = w->cap ? w->cap * 2 : 256;
    while (ncap < nbytes) ncap *= 2;
    uint8_t *p = realloc(w->b, (size_t)ncap);
    if (!p) return -1;
    if (ncap > w->cap) memset(p + w->cap, 0, (size_t)(ncap - w->cap));
    w->b = p; w->cap = ncap;
    return 0;
}
static int bw_put(bw *w, unsigned v, int n) {
    if (n <= 0) return 0;
    if (bw_need(w, n)) return -1;
    for (int i = n - 1; i >= 0; i--) {
        int bi = w->bits++;
        if ((v >> i) & 1) w->b[bi >> 3] |= (uint8_t)(1u << (7 - (bi & 7)));
    }
    return 0;
}
static int bw_ue(bw *w, unsigned v) {
    unsigned x = v + 1; int z = 0;
    while ((2u << z) <= x && z < 31) z++;
    return bw_put(w, 0, z) || bw_put(w, x, z + 1);
}
static int bw_se(bw *w, int v) { return bw_ue(w, v > 0 ? (unsigned)(v * 2 - 1) : (unsigned)(-v * 2)); }
static int bw_trail(bw *w) {
    if (bw_put(w, 1, 1)) return -1;
    int r = w->bits & 7;
    return r ? bw_put(w, 0, 8 - r) : 0;
}

typedef struct { const uint8_t *b; int nbytes, bit, err; } br;
static int br_get(br *r, int n) {
    int v = 0;
    for (int i = 0; i < n; i++) {
        int bi = r->bit++;
        if ((bi >> 3) >= r->nbytes) { r->err = 1; return 0; }
        v = (v << 1) | ((r->b[bi >> 3] >> (7 - (bi & 7))) & 1);
    }
    return v;
}
static int br_ue(br *r) {
    int z = 0;
    while (z < 31 && !br_get(r, 1)) z++;
    if (r->err) return 0;
    int x = 1;
    for (int i = 0; i < z; i++) x = (x << 1) | br_get(r, 1);
    return x - 1;
}
static uint8_t *rbsp_escape(const uint8_t *rbsp, int n, int *out_n) {
    uint8_t *o = malloc((size_t)n * 2 + 8);
    if (!o) return 0;
    int w = 0, z = 0;
    for (int i = 0; i < n; i++) {
        if (z >= 2 && rbsp[i] <= 3) { o[w++] = 3; z = 0; }
        o[w++] = rbsp[i];
        z = rbsp[i] == 0 ? z + 1 : 0;
    }
    *out_n = w;
    return o;
}
static uint8_t *rbsp_unescape(const uint8_t *nal, int n, int *out_n) {
    uint8_t *o = malloc((size_t)n + 1);
    if (!o) return 0;
    int w = 0, z = 0;
    for (int i = 0; i < n; i++) {
        if (z >= 2 && nal[i] == 3) { z = 0; continue; }
        o[w++] = nal[i];
        z = nal[i] == 0 ? z + 1 : 0;
    }
    *out_n = w;
    return o;
}

typedef struct {
    int w, h, cw, ch, mbw, mbh, qp;
    uint8_t *y, *u, *v;
    uint8_t *nnz, *cnnz;
} pic;

static void pput(pic *p, int plane, int x, int yy, int v) {
    int W = plane ? p->cw : p->w;
    uint8_t *b = plane == 0 ? p->y : plane == 1 ? p->u : p->v;
    b[yy * W + x] = (uint8_t)clip8(v);
}

static int pcm_align(bw *w) {
    int r = w->bits & 7;
    return r ? bw_put(w, 0, 8 - r) : 0;
}
static int code_mb(bw *w, pic *p, const uint8_t *sy, int ystride, const uint8_t *su, int sustride, const uint8_t *sv, int svstride, int mbx, int mby, int write) {
    /* I_PCM: mb_type 25, alinhado em byte, 16x16 + 8x8 + 8x8. Sem residual CAVLC. */
    if (write) {
        if (bw_ue(w, 25) || pcm_align(w)) return -1;
        int x0 = mbx * 16, y0 = mby * 16;
        for (int row = 0; row < 16; row++) for (int col = 0; col < 16; col++) {
            int yy = y0 + row, xx = x0 + col;
            int s = (yy < p->h && xx < p->w) ? sy[yy * ystride + xx] : 0;
            if (bw_put(w, (unsigned)s, 8)) return -1;
            if (yy < p->h && xx < p->w) pput(p, 0, xx, yy, s);
        }
        for (int pidx = 0; pidx < 2; pidx++) {
            const uint8_t *src = pidx ? sv : su;
            int stride = pidx ? svstride : sustride;
            int cx0 = mbx * 8, cy0 = mby * 8;
            for (int row = 0; row < 8; row++) for (int col = 0; col < 8; col++) {
                int yy = cy0 + row, xx = cx0 + col;
                int s = (yy < p->ch && xx < p->cw) ? src[yy * stride + xx] : 0;
                if (bw_put(w, (unsigned)s, 8)) return -1;
                if (yy < p->ch && xx < p->cw) pput(p, pidx + 1, xx, yy, s);
            }
        }
        return 0;
    }
    return 0;
}
static int write_sps(bw *w, int wpx, int hpx) {
    int mbw = (wpx + 15) / 16, mbh = (hpx + 15) / 16;
    int crop_r = mbw * 16 - wpx, crop_b = mbh * 16 - hpx;
    if (bw_put(w, 66, 8) || bw_put(w, 1 << 7 | 1 << 6, 8) || bw_put(w, 31, 8)) return -1;
    if (bw_ue(w, 0) || bw_ue(w, 0) || bw_ue(w, 0) || bw_ue(w, 0)) return -1;
    if (bw_ue(w, 1) || bw_put(w, 0, 1)) return -1;
    if (bw_ue(w, (unsigned)(mbw - 1)) || bw_ue(w, (unsigned)(mbh - 1))) return -1;
    if (bw_put(w, 1, 1) || bw_put(w, 1, 1)) return -1;
    if (crop_r || crop_b) {
        if (bw_put(w, 1, 1)) return -1;
        if (bw_ue(w, 0) || bw_ue(w, (unsigned)(crop_r / 2)) || bw_ue(w, 0) || bw_ue(w, (unsigned)(crop_b / 2))) return -1;
    } else if (bw_put(w, 0, 1)) return -1;
    if (bw_put(w, 0, 1)) return -1;
    return bw_trail(w);
}
static int write_pps(bw *w) {
    if (bw_ue(w, 0) || bw_ue(w, 0) || bw_put(w, 0, 1) || bw_put(w, 0, 1)) return -1;
    if (bw_ue(w, 0) || bw_ue(w, 0) || bw_ue(w, 0) || bw_put(w, 0, 1) || bw_put(w, 0, 2)) return -1;
    if (bw_se(w, 0) || bw_se(w, 0) || bw_se(w, 0)) return -1;
    if (bw_put(w, 1, 1) || bw_put(w, 0, 1) || bw_put(w, 0, 1)) return -1;
    return bw_trail(w);
}

static int parse_sps(br *r, int *w, int *h, int *mbw, int *mbh) {
    if (br_get(r, 8) != 66) return -1;
    br_get(r, 8); br_get(r, 8);
    br_ue(r); br_ue(r); br_ue(r); br_ue(r);
    if (br_ue(r) != 1) return -1;
    br_get(r, 1);
    *mbw = br_ue(r) + 1; *mbh = br_ue(r) + 1;
    if (!br_get(r, 1)) return -1;
    br_get(r, 1);
    int cr = 0, cb = 0;
    if (br_get(r, 1)) { br_ue(r); cr = br_ue(r); br_ue(r); cb = br_ue(r); }
    *w = *mbw * 16 - cr * 2; *h = *mbh * 16 - cb * 2;
    return r->err ? -1 : 0;
}

static uint8_t *nal_from_rbsp(int nal_type, bw *rbsp, int *n) {
    int rn = rbsp->bits >> 3;
    uint8_t *raw = malloc((size_t)rn + 1);
    if (!raw) return 0;
    raw[0] = (uint8_t)((3 << 5) | nal_type);
    memcpy(raw + 1, rbsp->b, (size_t)rn);
    uint8_t *esc = rbsp_escape(raw, rn + 1, n);
    free(raw);
    return esc;
}

int h264_sps_wh(const uint8_t *nal, int n, int *w, int *h) {
    int rn = 0;
    uint8_t *rb = rbsp_unescape(nal, n, &rn);
    if (!rb || rn < 4) { free(rb); return -1; }
    br r = {rb + 1, rn - 1, 0, 0};
    int mbw = 0, mbh = 0;
    int rc = parse_sps(&r, w, h, &mbw, &mbh);
    free(rb);
    return rc;
}

static int pic_alloc(pic *p, int w, int h, int qp) {
    memset(p, 0, sizeof *p);
    p->w = (w + 15) & ~15; p->h = (h + 15) & ~15;
    p->cw = p->w / 2; p->ch = p->h / 2;
    p->mbw = p->w / 16; p->mbh = p->h / 16; p->qp = qp;
    p->y = calloc((size_t)p->w * (size_t)p->h, 1);
    p->u = calloc((size_t)p->cw * (size_t)p->ch, 1);
    p->v = calloc((size_t)p->cw * (size_t)p->ch, 1);
    p->nnz = calloc((size_t)p->mbw * (size_t)p->mbh * 16, 1);
    p->cnnz = calloc((size_t)p->mbw * (size_t)p->mbh * 8, 1);
    if (!p->y || !p->u || !p->v || !p->nnz || !p->cnnz) return -1;
    return 0;
}
static void pic_free(pic *p) { free(p->y); free(p->u); free(p->v); free(p->nnz); free(p->cnnz); }

void h264_au_free(h264_au *o) {
    if (!o) return;
    free(o->sps); free(o->pps); free(o->sample); free(o->annexb);
    memset(o, 0, sizeof *o);
}

int h264_encode_i420(const uint8_t *y, const uint8_t *u, const uint8_t *v,
                     int w, int h, int sy, int su, int sv, int qp, h264_au *o) {
    if (!y || !u || !v || !o || w < 2 || h < 2 || (w & 1) || (h & 1) || w > 4096 || h > 4096) return -1;
    if (qp < 0) qp = 0;
    if (qp > 51) qp = 51;
    memset(o, 0, sizeof *o);
    o->w = w; o->h = h;
    bw sps = {0}, pps = {0}, sl = {0};
    if (write_sps(&sps, w, h) || write_pps(&pps)) { free(sps.b); free(pps.b); return -2; }
    pic p;
    if (pic_alloc(&p, w, h, qp)) { pic_free(&p); free(sps.b); free(pps.b); return -3; }
    if (bw_ue(&sl, 0) || bw_ue(&sl, 7) || bw_ue(&sl, 0) || bw_put(&sl, 0, 4)) { free(sps.b); free(pps.b); free(sl.b); pic_free(&p); return -4; }
    if (bw_ue(&sl, 0) || bw_put(&sl, 0, 4) || bw_put(&sl, 0, 1) || bw_put(&sl, 0, 1)) { free(sps.b); free(pps.b); free(sl.b); pic_free(&p); return -5; }
    if (bw_se(&sl, qp - 26) || bw_ue(&sl, 1)) { free(sps.b); free(pps.b); free(sl.b); pic_free(&p); return -6; }
    for (int my = 0; my < p.mbh; my++) for (int mx = 0; mx < p.mbw; mx++) {
        int mbr = code_mb(&sl, &p, y, sy, u, su, v, sv, mx, my, 1);
        if (mbr) { free(sps.b); free(pps.b); free(sl.b); pic_free(&p); return mbr; }
    }
    if (bw_trail(&sl)) goto fail;
    int sn = 0, pn = 0, ln = 0;
    o->sps = nal_from_rbsp(7, &sps, &sn);
    o->pps = nal_from_rbsp(8, &pps, &pn);
    o->sample = 0;
    uint8_t *slice = nal_from_rbsp(5, &sl, &ln);
    o->sps_n = sn; o->pps_n = pn;
    if (!o->sps || !o->pps || !slice) { free(slice); goto fail; }
    o->sample = malloc((size_t)ln + 4);
    if (!o->sample) { free(slice); goto fail; }
    o->sample[0] = (uint8_t)(ln >> 24); o->sample[1] = (uint8_t)(ln >> 16);
    o->sample[2] = (uint8_t)(ln >> 8); o->sample[3] = (uint8_t)ln;
    memcpy(o->sample + 4, slice, (size_t)ln);
    o->sample_n = ln + 4;
    o->annexb_n = 4 + sn + 4 + pn + 4 + ln;
    o->annexb = malloc((size_t)o->annexb_n);
    if (!o->annexb) { free(slice); goto fail; }
    uint8_t *d = o->annexb;
    #define SC(dst) do { (dst)[0]=0;(dst)[1]=0;(dst)[2]=0;(dst)[3]=1; dst += 4; } while (0)
    SC(d); memcpy(d, o->sps, (size_t)sn); d += sn;
    SC(d); memcpy(d, o->pps, (size_t)pn); d += pn;
    SC(d); memcpy(d, slice, (size_t)ln);
    #undef SC
    free(slice); free(sps.b); free(pps.b); free(sl.b); pic_free(&p);
    return 0;
fail:
    free(sps.b); free(pps.b); free(sl.b); pic_free(&p); h264_au_free(o);
    return -1;
}

/* decodificação de uma unidade de acesso: OpenH264 corrigido pela casca cx_h264_* (host-audio/h264_glue.cpp) */
typedef struct { int full_range, matrix, primaries, transfer; } cx_h264_color_t;
typedef void (*cx_h264_emit_t)(void *ctx, const uint8_t *y, const uint8_t *u, const uint8_t *v, int sy, int suv, int w, int h,
                               uint64_t tag, const cx_h264_color_t *c);
void *cx_h264_open(int decode_order);
int cx_h264_decode(void *d, const uint8_t *annexb, int n, uint64_t tag, cx_h264_emit_t emit, void *ctx);
void cx_h264_close(void *d);
typedef struct { uint8_t *y, *u, *v; int sy, su, sv, w, h, got; } one_t;
static void one_emit(void *ctx, const uint8_t *y, const uint8_t *u, const uint8_t *v, int sy, int suv, int w, int h, uint64_t tag,
                     const cx_h264_color_t *c) {
    one_t *o = ctx;
    (void)tag; (void)c;
    if (o->got) return;
    for (int r = 0; r < h; r++) memcpy(o->y + (size_t)r * o->sy, y + (size_t)r * sy, (size_t)w);
    for (int r = 0; r < h / 2; r++) {
        memcpy(o->u + (size_t)r * o->su, u + (size_t)r * suv, (size_t)w / 2);
        memcpy(o->v + (size_t)r * o->sv, v + (size_t)r * suv, (size_t)w / 2);
    }
    o->w = w; o->h = h; o->got = 1;
}
static int decode_annexb(const uint8_t *b, int n, uint8_t *y, uint8_t *u, uint8_t *v, int sy, int su, int sv, int *ow, int *oh) {
    void *d = cx_h264_open(1);
    if (!d) return -1;
    one_t o = {y, u, v, sy, su, sv, 0, 0, 0};
    int rc = cx_h264_decode(d, b, n, 0, one_emit, &o);
    cx_h264_close(d);
    if (rc || !o.got) return -1;
    if (ow) *ow = o.w;
    if (oh) *oh = o.h;
    return 0;
}

static int annex_put(uint8_t *d, const uint8_t *nal, int n) {
    d[0] = 0; d[1] = 0; d[2] = 0; d[3] = 1;
    memcpy(d + 4, nal, (size_t)n);
    return n + 4;
}

int h264_decode_nals(const uint8_t *sps, int sps_n, const uint8_t *pps, int pps_n,
                     const uint8_t *slice, int slice_n,
                     uint8_t *y, uint8_t *u, uint8_t *v, int sy, int su, int sv,
                     int *ow, int *oh) {
    if (!sps || sps_n < 1 || !pps || pps_n < 1 || !slice || slice_n < 1 || !y) return -1;
    uint8_t *b = malloc((size_t)(12 + sps_n + pps_n + slice_n));
    if (!b) return -1;
    int n = 0;
    n += annex_put(b + n, sps, sps_n);
    n += annex_put(b + n, pps, pps_n);
    n += annex_put(b + n, slice, slice_n);
    int rc = decode_annexb(b, n, y, u, v, sy, su, sv, ow, oh);
    free(b);
    return rc;
}

int h264_decode_avcc(const uint8_t *sps, int sps_n, const uint8_t *pps, int pps_n,
                     const uint8_t *sample, int sample_n,
                     uint8_t *y, uint8_t *u, uint8_t *v, int sy, int su, int sv,
                     int *ow, int *oh) {
    int off = 0, n = 0;
    if (!sps || !pps || !sample || sample_n < 5) return -1;
    uint8_t *b = malloc((size_t)sample_n + (size_t)sps_n + (size_t)pps_n + 64);
    if (!b) return -1;
    n += annex_put(b + n, sps, sps_n);
    n += annex_put(b + n, pps, pps_n);
    while (off + 4 <= sample_n) {
        int len = (sample[off] << 24) | (sample[off+1] << 16) | (sample[off+2] << 8) | sample[off+3];
        off += 4;
        if (len <= 0 || off + len > sample_n) { free(b); return -1; }
        n += annex_put(b + n, sample + off, len);
        off += len;
    }
    int rc = decode_annexb(b, n, y, u, v, sy, su, sv, ow, oh);
    free(b);
    return rc;
}

static int clamp_add(int a, int b) { return clip8(a + b); }
void h264_bgra_to_i420(const uint8_t *bgra, int w, int h, int sb,
                       uint8_t *y, uint8_t *u, uint8_t *v, int sy, int su, int sv) {
    for (int row = 0; row < h; row++) for (int col = 0; col < w; col++) {
        const uint8_t *p = bgra + row * sb + col * 4;
        int B = p[0], G = p[1], R = p[2];
        y[row * sy + col] = (uint8_t)((77 * R + 150 * G + 29 * B + 128) >> 8);
    }
    for (int row = 0; row < h / 2; row++) for (int col = 0; col < w / 2; col++) {
        int R = 0, G = 0, B = 0;
        for (int dy = 0; dy < 2; dy++) for (int dx = 0; dx < 2; dx++) {
            const uint8_t *p = bgra + (row * 2 + dy) * sb + (col * 2 + dx) * 4;
            B += p[0]; G += p[1]; R += p[2];
        }
        R >>= 2; G >>= 2; B >>= 2;
        u[row * su + col] = (uint8_t)clip8((((-43 * R - 85 * G + 128 * B + 128) >> 8) + 128));
        v[row * sv + col] = (uint8_t)clip8((((128 * R - 107 * G - 21 * B + 128) >> 8) + 128));
    }
}
void h264_i420_to_bgra(const uint8_t *y, const uint8_t *u, const uint8_t *v,
                       int w, int h, int sy, int su, int sv, uint8_t *bgra, int sb) {
    for (int row = 0; row < h; row++) for (int col = 0; col < w; col++) {
        int Y = y[row * sy + col];
        int U = u[(row / 2) * su + col / 2] - 128;
        int V = v[(row / 2) * sv + col / 2] - 128;
        uint8_t *p = bgra + row * sb + col * 4;
        p[2] = (uint8_t)clamp_add(Y, (359 * V) >> 8);
        p[1] = (uint8_t)clamp_add(Y, -(88 * U + 183 * V) >> 8);
        p[0] = (uint8_t)clamp_add(Y, (454 * U) >> 8);
        p[3] = 255;
    }
}
void h264_nv12_to_i420(const uint8_t *yp, int sy, const uint8_t *uv, int suv,
                       int w, int h, uint8_t *y, uint8_t *u, uint8_t *v, int dy, int du, int dv) {
    for (int row = 0; row < h; row++) memcpy(y + row * dy, yp + row * sy, (size_t)w);
    for (int row = 0; row < h / 2; row++) for (int col = 0; col < w / 2; col++) {
        u[row * du + col] = uv[row * suv + col * 2];
        v[row * dv + col] = uv[row * suv + col * 2 + 1];
    }
}
void h264_i420_to_nv12(const uint8_t *y, const uint8_t *u, const uint8_t *v,
                       int w, int h, int sy, int su, int sv, uint8_t *yp, int dy, uint8_t *uv, int duv) {
    for (int row = 0; row < h; row++) memcpy(yp + row * dy, y + row * sy, (size_t)w);
    for (int row = 0; row < h / 2; row++) for (int col = 0; col < w / 2; col++) {
        uv[row * duv + col * 2] = u[row * su + col];
        uv[row * duv + col * 2 + 1] = v[row * sv + col];
    }
}
