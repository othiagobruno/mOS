/* mOS — conversão dos quadros da câmera (ver avcap_conv.h). Código original do mOS (MIT). */
#include "avcap_conv.h"

void *malloc(size_t n);
void free(void *p);

static uint8_t clamp8(int v) { return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v); }

int avc_supported(uint32_t f) {
    return f == AVC_FMT_BGRA || f == AVC_FMT_2VUY || f == AVC_FMT_YUVS || f == AVC_FMT_420V || f == AVC_FMT_420F || f == AVC_FMT_Y420;
}
int avc_planes(uint32_t f, int w, int h, size_t *bpr, size_t *rows) {
    size_t cw = (size_t)(w + 1) / 2, ch = (size_t)(h + 1) / 2;
    switch (f) {
    case AVC_FMT_BGRA: bpr[0] = (size_t)w * 4; rows[0] = (size_t)h; return 1;
    case AVC_FMT_2VUY: case AVC_FMT_YUVS: bpr[0] = cw * 4; rows[0] = (size_t)h; return 1;
    case AVC_FMT_420V: case AVC_FMT_420F: bpr[0] = (size_t)w; rows[0] = (size_t)h; bpr[1] = cw * 2; rows[1] = ch; return 2;
    case AVC_FMT_Y420: bpr[0] = (size_t)w; rows[0] = (size_t)h; bpr[1] = bpr[2] = cw; rows[1] = rows[2] = ch; return 3;
    }
    return 0;
}

/* uma linha da origem em Y, U, V 4:4:4 (faixa de vídeo) ou BGRA */
static void row_yuv(int src, const uint8_t *s, int w, int h, int y, uint8_t *Y, uint8_t *U, uint8_t *V) {
    if (src == AVC_SRC_YUY2) {
        const uint8_t *r = s + (size_t)y * (size_t)((w + 1) / 2) * 4;
        for (int x = 0; x < w; x++) {
            const uint8_t *q = r + (x / 2) * 4;
            Y[x] = q[(x & 1) ? 2 : 0]; U[x] = q[1]; V[x] = q[3];
        }
    } else if (src == AVC_SRC_NV12) {
        const uint8_t *ry = s + (size_t)y * w, *ruv = s + (size_t)w * h + (size_t)(y / 2) * (size_t)((w + 1) / 2) * 2;
        for (int x = 0; x < w; x++) { Y[x] = ry[x]; U[x] = ruv[(x / 2) * 2]; V[x] = ruv[(x / 2) * 2 + 1]; }
    } else {
        const uint8_t *r = s + (size_t)y * w * 4;
        for (int x = 0; x < w; x++) {
            int b = r[x * 4], g = r[x * 4 + 1], rr = r[x * 4 + 2];
            Y[x] = clamp8(((66 * rr + 129 * g + 25 * b + 128) >> 8) + 16);
            U[x] = clamp8(((-38 * rr - 74 * g + 112 * b + 128) >> 8) + 128);
            V[x] = clamp8(((112 * rr - 94 * g - 18 * b + 128) >> 8) + 128);
        }
    }
}
static size_t src_need(int src, int w, int h) {
    if (src == AVC_SRC_YUY2) return (size_t)((w + 1) / 2) * 4 * h;
    if (src == AVC_SRC_NV12) return (size_t)w * h + (size_t)((w + 1) / 2) * 2 * ((h + 1) / 2);
    return (size_t)w * h * 4;
}
static uint8_t fullY(int y) { return clamp8(((y - 16) * 255 + 109) / 219); }
static uint8_t fullC(int c) { return clamp8(((c - 128) * 255) / 224 + 128); }

int avc_convert(int src, const uint8_t *s, size_t slen, int w, int h, uint32_t f, uint8_t *const *pl, const size_t *bpr) {
    if (!s || w <= 0 || h <= 0 || !avc_supported(f) || slen < src_need(src, w, h)) return -1;
    if (f == AVC_FMT_BGRA && src == AVC_SRC_BGRA) {
        for (int y = 0; y < h; y++) { const uint8_t *a = s + (size_t)y * w * 4; uint8_t *b = pl[0] + (size_t)y * bpr[0]; for (int i = 0; i < w * 4; i++) b[i] = a[i]; }
        return 0;
    }
    uint8_t *buf = malloc((size_t)w * 6);
    if (!buf) return -1;
    uint8_t *Y0 = buf, *U0 = buf + w, *V0 = buf + 2 * w, *Y1 = buf + 3 * w, *U1 = buf + 4 * w, *V1 = buf + 5 * w;
    int full = f == AVC_FMT_420F;
    for (int y = 0; y < h; y += 2) {
        int two = y + 1 < h;
        row_yuv(src, s, w, h, y, Y0, U0, V0);
        if (two) row_yuv(src, s, w, h, y + 1, Y1, U1, V1);
        for (int k = 0; k < (two ? 2 : 1); k++) {
            const uint8_t *Y = k ? Y1 : Y0, *U = k ? U1 : U0, *V = k ? V1 : V0;
            int yy = y + k;
            if (f == AVC_FMT_BGRA) {
                uint8_t *o = pl[0] + (size_t)yy * bpr[0];
                for (int x = 0; x < w; x++) {
                    int c = (Y[x] - 16) * 1192, d = U[x] - 128, e = V[x] - 128;
                    o[x * 4] = clamp8((c + 2066 * d + 512) >> 10);
                    o[x * 4 + 1] = clamp8((c - 400 * d - 832 * e + 512) >> 10);
                    o[x * 4 + 2] = clamp8((c + 1634 * e + 512) >> 10);
                    o[x * 4 + 3] = 255;
                }
            } else if (f == AVC_FMT_2VUY || f == AVC_FMT_YUVS) {
                uint8_t *o = pl[0] + (size_t)yy * bpr[0];
                for (int x = 0; x < w; x += 2) {
                    int x1 = x + 1 < w ? x + 1 : x;
                    uint8_t u = (uint8_t)((U[x] + U[x1] + 1) / 2), v = (uint8_t)((V[x] + V[x1] + 1) / 2);
                    uint8_t *q = o + (x / 2) * 4;
                    if (f == AVC_FMT_2VUY) { q[0] = u; q[1] = Y[x]; q[2] = v; q[3] = Y[x1]; }
                    else { q[0] = Y[x]; q[1] = u; q[2] = Y[x1]; q[3] = v; }
                }
            } else {
                uint8_t *o = pl[0] + (size_t)yy * bpr[0];
                for (int x = 0; x < w; x++) o[x] = full ? fullY(Y[x]) : Y[x];
            }
        }
        if (f == AVC_FMT_420V || f == AVC_FMT_420F || f == AVC_FMT_Y420) {
            const uint8_t *Ub = two ? U1 : U0, *Vb = two ? V1 : V0;
            for (int x = 0; x < w; x += 2) {
                int x1 = x + 1 < w ? x + 1 : x;
                int u = (U0[x] + U0[x1] + Ub[x] + Ub[x1] + 2) / 4, v = (V0[x] + V0[x1] + Vb[x] + Vb[x1] + 2) / 4;
                if (full) { u = fullC(u); v = fullC(v); }
                if (f == AVC_FMT_Y420) { pl[1][(size_t)(y / 2) * bpr[1] + x / 2] = (uint8_t)u; pl[2][(size_t)(y / 2) * bpr[2] + x / 2] = (uint8_t)v; }
                else { uint8_t *q = pl[1] + (size_t)(y / 2) * bpr[1] + x; q[0] = (uint8_t)u; q[1] = (uint8_t)v; }
            }
        }
    }
    free(buf);
    return 0;
}

static void swapn(uint8_t *a, uint8_t *b, int n) { for (int i = 0; i < n; i++) { uint8_t t = a[i]; a[i] = b[i]; b[i] = t; } }
static void mirror_units(uint8_t *row, int units, int n) { for (int i = 0, j = units - 1; i < j; i++, j--) swapn(row + i * n, row + j * n, n); }
void avc_mirror(uint32_t f, int w, int h, uint8_t *const *pl, const size_t *bpr) {
    int cw = (w + 1) / 2, ch = (h + 1) / 2;
    switch (f) {
    case AVC_FMT_BGRA: for (int y = 0; y < h; y++) mirror_units(pl[0] + (size_t)y * bpr[0], w, 4); break;
    case AVC_FMT_2VUY: case AVC_FMT_YUVS: {
        int yo = f == AVC_FMT_2VUY ? 1 : 0;   /* posição de Y0 no macro-pixel; Y1 está 2 adiante */
        for (int y = 0; y < h; y++) {
            uint8_t *r = pl[0] + (size_t)y * bpr[0];
            mirror_units(r, cw, 4);
            for (int x = 0; x < cw; x++) { uint8_t t = r[x * 4 + yo]; r[x * 4 + yo] = r[x * 4 + yo + 2]; r[x * 4 + yo + 2] = t; }
        }
        break;
    }
    case AVC_FMT_420V: case AVC_FMT_420F:
        for (int y = 0; y < h; y++) mirror_units(pl[0] + (size_t)y * bpr[0], w, 1);
        for (int y = 0; y < ch; y++) mirror_units(pl[1] + (size_t)y * bpr[1], cw, 2);
        break;
    case AVC_FMT_Y420:
        for (int y = 0; y < h; y++) mirror_units(pl[0] + (size_t)y * bpr[0], w, 1);
        for (int k = 1; k < 3; k++) for (int y = 0; y < ch; y++) mirror_units(pl[k] + (size_t)y * bpr[k], cw, 1);
        break;
    }
}

static int better(const avc_mode *a, const avc_mode *b) {   /* desempate entre tamanhos iguais */
    uint32_t ia = a->min_interval ? a->min_interval : 0xFFFFFFFFu, ib = b->min_interval ? b->min_interval : 0xFFFFFFFFu;
    if (ia != ib) return ia < ib;
    return a->fourcc != 0x47504a4du && b->fourcc == 0x47504a4du;
}
int avc_pick_mode(const avc_mode *m, int n, int how, int w, int h) {
    int best = -1;
    long long bs = 0;
    for (int pass = 0; pass < 2 && best < 0; pass++)
        for (int i = 0; i < n; i++) {
            long long area = (long long)m[i].w * m[i].h, s;
            if (how == AVC_PICK_SIZE) { long long dw = m[i].w - w, dh = m[i].h - h; s = -(dw * dw + dh * dh); }
            else if (how == AVC_PICK_HIGH) {
                if (!pass && (m[i].w > 1920 || m[i].h > 1080 || !m[i].min_interval || m[i].min_interval > 416667)) continue;
                s = area;
            } else s = area;
            if (best < 0 || s > bs || (s == bs && better(&m[i], &m[best]))) { best = i; bs = s; }
        }
    return best;
}
