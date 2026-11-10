/*
 * mOS — grava PNG a partir dos pixels 0xAARRGGBB do ImageIO.
 * O bloco deflate é do tipo "armazenado" (RFC 1951, BTYPE 00): o arquivo fica maior, e qualquer
 * leitor de PNG o aceita. JPEG de saída não entra aqui (ver ImageIO.m).
 */
#include "imageio_codec.h"
#include <stdlib.h>
#include <string.h>

static uint32_t adler32(const uint8_t *p, size_t n) {
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < n; i++) {
        a = (a + p[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    return (b << 16) | a;
}

static uint32_t crc32(const uint8_t *p, size_t n) {
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        c ^= p[i];
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return c ^ 0xFFFFFFFFu;
}

static int put_chunk(uint8_t **buf, size_t *len, size_t *cap, const char type[4], const uint8_t *data, size_t n) {
    size_t need = *len + 12 + n;
    if (need > *cap) {
        size_t nc = *cap ? *cap * 2 : 4096;
        while (nc < need) nc *= 2;
        uint8_t *nb = realloc(*buf, nc);
        if (!nb) return -1;
        *buf = nb;
        *cap = nc;
    }
    uint8_t *d = *buf + *len;
    d[0] = (uint8_t)(n >> 24); d[1] = (uint8_t)(n >> 16); d[2] = (uint8_t)(n >> 8); d[3] = (uint8_t)n;
    memcpy(d + 4, type, 4);
    if (n) memcpy(d + 8, data, n);
    uint32_t c = crc32(d + 4, 4 + n);
    d[8 + n] = (uint8_t)(c >> 24); d[9 + n] = (uint8_t)(c >> 16);
    d[10 + n] = (uint8_t)(c >> 8); d[11 + n] = (uint8_t)c;
    *len += 12 + n;
    return 0;
}

int mos_img_encode_png(const uint32_t *px, int w, int h, uint8_t **out, size_t *n) {
    if (!px || w <= 0 || h <= 0 || !out || !n) return -1;
    if ((uint64_t)w * (uint64_t)h > 6000ull * 6000ull) return -1;
    size_t row = (size_t)w * 4u + 1u;
    size_t raw_n = row * (size_t)h;
    uint8_t *raw = malloc(raw_n);
    if (!raw) return -1;
    for (int y = 0; y < h; y++) {
        uint8_t *r = raw + (size_t)y * row;
        r[0] = 0;
        for (int x = 0; x < w; x++) {
            uint32_t p = px[(size_t)y * (size_t)w + (size_t)x];
            uint8_t *o = r + 1 + (size_t)x * 4u;
            o[0] = (uint8_t)(p >> 16);
            o[1] = (uint8_t)(p >> 8);
            o[2] = (uint8_t)p;
            o[3] = (uint8_t)(p >> 24);
        }
    }
    /* zlib: CMF/FLG + blocos armazenados de até 65535 bytes + Adler-32 */
    size_t zcap = raw_n + (raw_n / 65535u + 1u) * 5u + 6u;
    uint8_t *z = malloc(zcap);
    if (!z) { free(raw); return -1; }
    size_t zo = 0;
    z[zo++] = 0x78; z[zo++] = 0x01;
    size_t off = 0;
    while (off < raw_n) {
        size_t chunk = raw_n - off;
        if (chunk > 65535u) chunk = 65535u;
        int last = off + chunk == raw_n;
        z[zo++] = (uint8_t)last;
        z[zo++] = (uint8_t)chunk;
        z[zo++] = (uint8_t)(chunk >> 8);
        z[zo++] = (uint8_t)(chunk ^ 0xFF);
        z[zo++] = (uint8_t)((chunk >> 8) ^ 0xFF);
        memcpy(z + zo, raw + off, chunk);
        zo += chunk;
        off += chunk;
    }
    uint32_t ad = adler32(raw, raw_n);
    z[zo++] = (uint8_t)(ad >> 24); z[zo++] = (uint8_t)(ad >> 16);
    z[zo++] = (uint8_t)(ad >> 8); z[zo++] = (uint8_t)ad;
    free(raw);

    uint8_t *png = NULL;
    size_t plen = 0, pcap = 0;
    static const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    png = malloc(8);
    if (!png) { free(z); return -1; }
    memcpy(png, sig, 8);
    plen = 8; pcap = 8;
    uint8_t ihdr[13];
    ihdr[0] = (uint8_t)((unsigned)w >> 24); ihdr[1] = (uint8_t)((unsigned)w >> 16);
    ihdr[2] = (uint8_t)((unsigned)w >> 8); ihdr[3] = (uint8_t)w;
    ihdr[4] = (uint8_t)((unsigned)h >> 24); ihdr[5] = (uint8_t)((unsigned)h >> 16);
    ihdr[6] = (uint8_t)((unsigned)h >> 8); ihdr[7] = (uint8_t)h;
    ihdr[8] = 8; ihdr[9] = 6; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    int rc = put_chunk(&png, &plen, &pcap, "IHDR", ihdr, 13);
    if (rc == 0) rc = put_chunk(&png, &plen, &pcap, "IDAT", z, zo);
    if (rc == 0) rc = put_chunk(&png, &plen, &pcap, "IEND", NULL, 0);
    free(z);
    if (rc != 0) { free(png); return -1; }
    *out = png;
    *n = plen;
    return 0;
}

/* JPEG baseline, um componente de cor por bloco 8x8, amostragem 4:4:4. */
static const uint8_t jpg_qy[64] = {
    16, 11, 10, 16, 24, 40, 51, 61, 12, 12, 14, 19, 26, 58, 60, 55,
    14, 13, 16, 24, 40, 57, 69, 56, 14, 17, 22, 29, 51, 87, 80, 62,
    18, 22, 37, 56, 68, 109, 103, 77, 24, 35, 55, 64, 81, 104, 113, 92,
    49, 64, 78, 87, 103, 121, 120, 101, 72, 92, 95, 98, 112, 100, 103, 99
};
static const uint8_t jpg_qc[64] = {
    17, 18, 24, 47, 99, 99, 99, 99, 18, 21, 26, 66, 99, 99, 99, 99,
    24, 26, 56, 99, 99, 99, 99, 99, 47, 66, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99
};
static const uint8_t jpg_zig[64] = {
    0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63
};
static const uint8_t hdcl_c[16] = {0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0};
static const uint8_t hdcl_s[12] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
static const uint8_t hdcc_c[16] = {0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0};
static const uint8_t hdcc_s[12] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
static const uint8_t hacl_c[16] = {0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 125};
static const uint8_t hacl_s[162] = {
    0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
    0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08, 0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
    0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
    0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
    0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
    0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
    0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
    0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
    0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
    0xf9, 0xfa
};
static const uint8_t hacc_c[16] = {0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 119};
static const uint8_t hacc_s[162] = {
    0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
    0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91, 0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
    0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34, 0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
    0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
    0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
    0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
    0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
    0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
    0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
    0xf9, 0xfa
};

typedef struct { uint8_t *p; size_t n, cap; uint32_t acc; int bits; } jbit_t;
static int jgrow(jbit_t *b, size_t extra) {
    if (b->n + extra <= b->cap) return 0;
    size_t nc = b->cap ? b->cap * 2 : 4096;
    while (nc < b->n + extra) nc *= 2;
    uint8_t *np = realloc(b->p, nc);
    if (!np) return -1;
    b->p = np; b->cap = nc; return 0;
}
static int jput(jbit_t *b, const void *s, size_t n) {
    if (jgrow(b, n)) return -1;
    memcpy(b->p + b->n, s, n); b->n += n; return 0;
}
static int jbit(jbit_t *b, int v, int n) {
    b->acc = (b->acc << n) | (unsigned)v;
    b->bits += n;
    while (b->bits >= 8) {
        uint8_t byte = (uint8_t)(b->acc >> (b->bits - 8));
        b->bits -= 8;
        if (jput(b, &byte, 1)) return -1;
        if (byte == 0xff) { uint8_t z = 0; if (jput(b, &z, 1)) return -1; }
    }
    return 0;
}
static int jflush(jbit_t *b) { return b->bits ? jbit(b, (1 << (8 - b->bits)) - 1, 8 - b->bits) : 0; }

static int jcode(const uint8_t *counts, const uint8_t *syms, int nsym, int sym, int *len, unsigned *code) {
    unsigned c = 0; int k = 0;
    for (int l = 1; l <= 16; l++) {
        for (int i = 0; i < counts[l - 1]; i++) {
            if (k >= nsym) return -1;
            if (syms[k] == sym) { *len = l; *code = c; return 0; }
            c++; k++;
        }
        c <<= 1;
    }
    return -1;
}
static int jmagbits(int v, int *n, int *mag) {
    int a = v < 0 ? -v : v;
    int k = 0; while (a) { k++; a >>= 1; }
    *n = k;
    *mag = v < 0 ? v + (1 << k) - 1 : v;
    return 0;
}
static void jfdct(int *b) {
    const double pi = 3.141592653589793;
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < 8; i++) {
            double in[8], t[8];
            int step = pass ? 8 : 1;
            int *r = pass ? b + i : b + i * 8;
            for (int n = 0; n < 8; n++) in[n] = r[n * step];
            for (int k = 0; k < 8; k++) {
                double s = 0;
                for (int n = 0; n < 8; n++) s += in[n] * __builtin_cos((2 * n + 1) * k * pi / 16.0);
                /* Escala do idct do decoder: bloco constante C vira coeficiente C*8. */
                t[k] = s * (k ? 1.0 : 0.7071067811865476) * 1.41421356237;
            }
            for (int n = 0; n < 8; n++) r[n * step] = (int)(t[n] + (t[n] < 0 ? -0.5 : 0.5));
        }
    }
}
static int jblock(jbit_t *o, int *blk, const uint8_t *q, const uint8_t *dc_c, const uint8_t *dc_s, int dc_n,
                  const uint8_t *ac_c, const uint8_t *ac_s, int ac_n, int *pred) {
    int nat[64];
    for (int i = 0; i < 64; i++) {
        int v = blk[i] / q[i];
        if (v > 1023) v = 1023; if (v < -1023) v = -1023;
        nat[i] = v;
    }
    int diff = nat[0] - *pred; *pred = nat[0];
    int n, mag; jmagbits(diff, &n, &mag);
    int len; unsigned code;
    if (jcode(dc_c, dc_s, dc_n, n, &len, &code)) return -1;
    if (jbit(o, (int)code, len)) return -1;
    if (n && jbit(o, mag, n)) return -1;
    int run = 0;
    for (int i = 1; i < 64; i++) {
        int coef = nat[jpg_zig[i]];
        if (coef == 0) { run++; continue; }
        while (run > 15) {
            if (jcode(ac_c, ac_s, ac_n, 0xf0, &len, &code)) return -1;
            if (jbit(o, (int)code, len)) return -1;
            run -= 16;
        }
        jmagbits(coef, &n, &mag);
        if (jcode(ac_c, ac_s, ac_n, (run << 4) | n, &len, &code)) return -1;
        if (jbit(o, (int)code, len) || jbit(o, mag, n)) return -1;
        run = 0;
    }
    if (run) {
        if (jcode(ac_c, ac_s, ac_n, 0, &len, &code)) return -1;
        if (jbit(o, (int)code, len)) return -1;
    }
    return 0;
}

int mos_img_encode_jpeg(const uint32_t *px, int w, int h, int quality, uint8_t **out, size_t *n) {
    if (!px || w <= 0 || h <= 0 || !out || !n) return -1;
    if (quality < 1) quality = 1; if (quality > 100) quality = 100;
    int scale = quality < 50 ? 5000 / quality : 200 - quality * 2;
    uint8_t qy[64], qc[64];
    for (int i = 0; i < 64; i++) {
        int y = (jpg_qy[i] * scale + 50) / 100; if (y < 1) y = 1; if (y > 255) y = 255; qy[i] = (uint8_t)y;
        int c = (jpg_qc[i] * scale + 50) / 100; if (c < 1) c = 1; if (c > 255) c = 255; qc[i] = (uint8_t)c;
    }
    jbit_t b = {0};
    uint8_t hdr[32];
    hdr[0] = 0xff; hdr[1] = 0xd8; hdr[2] = 0xff; hdr[3] = 0xe0;
    hdr[4] = 0; hdr[5] = 16; memcpy(hdr + 6, "JFIF", 5); hdr[10] = 0;
    hdr[11] = 1; hdr[12] = 1; hdr[13] = 0; hdr[14] = hdr[15] = 1; hdr[16] = hdr[17] = 0; hdr[18] = 0;
    if (jput(&b, hdr, 20)) { free(b.p); return -1; }
    for (int ti = 0; ti < 2; ti++) {
        uint8_t qh[5] = {0xff, 0xdb, 0, 67, (uint8_t)ti};
        if (jput(&b, qh, 5) || jput(&b, ti ? qc : qy, 64)) { free(b.p); return -1; }
    }
    uint8_t sof[19] = {0xff, 0xc0, 0, 17, 8};
    sof[5] = (uint8_t)(h >> 8); sof[6] = (uint8_t)h; sof[7] = (uint8_t)(w >> 8); sof[8] = (uint8_t)w; sof[9] = 3;
    sof[10] = 1; sof[11] = 0x11; sof[12] = 0;
    sof[13] = 2; sof[14] = 0x11; sof[15] = 1;
    sof[16] = 3; sof[17] = 0x11; sof[18] = 1;
    if (jput(&b, sof, 19)) { free(b.p); return -1; }
    const uint8_t *cs[4] = {hdcl_c, hdcc_c, hacl_c, hacc_c};
    const uint8_t *ss[4] = {hdcl_s, hdcc_s, hacl_s, hacc_s};
    int ns[4] = {12, 12, 162, 162};
    for (int ti = 0; ti < 4; ti++) {
        int nb = 0; for (int i = 0; i < 16; i++) nb += cs[ti][i];
        uint8_t dh[5] = {0xff, 0xc4, 0, (uint8_t)(19 + nb), (uint8_t)((ti >= 2 ? 0x10 : 0) | (ti & 1))};
        if (jput(&b, dh, 5) || jput(&b, cs[ti], 16) || jput(&b, ss[ti], (size_t)nb)) { free(b.p); return -1; }
    }
    uint8_t sos[14] = {0xff, 0xda, 0, 12, 3, 1, 0x00, 2, 0x11, 3, 0x11, 0, 63, 0};
    if (jput(&b, sos, 14)) { free(b.p); return -1; }
    int pred[3] = {0, 0, 0};
    int bw = (w + 7) / 8, bh = (h + 7) / 8;
    for (int by = 0; by < bh; by++) for (int bx = 0; bx < bw; bx++) {
        int yb[64], cb[64], cr[64];
        for (int j = 0; j < 8; j++) for (int i = 0; i < 8; i++) {
            int x = bx * 8 + i, y = by * 8 + j;
            if (x >= w) x = w - 1; if (y >= h) y = h - 1;
            uint32_t p = px[(size_t)y * (size_t)w + (size_t)x];
            int R = (int)((p >> 16) & 255), G = (int)((p >> 8) & 255), B = (int)(p & 255);
            int Y = (77 * R + 150 * G + 29 * B) >> 8;
            int k = j * 8 + i;
            yb[k] = Y - 128;
            cb[k] = ((-43 * R - 85 * G + 128 * B) >> 8);
            cr[k] = ((128 * R - 107 * G - 21 * B) >> 8);
        }
        int *planes[3] = {yb, cb, cr};
        for (int p = 0; p < 3; p++) {
            int *pl = planes[p];
            int flat = 1;
            for (int k = 1; k < 64; k++) if (pl[k] != pl[0]) { flat = 0; break; }
            if (flat) { pl[0] *= 8; for (int k = 1; k < 64; k++) pl[k] = 0; }
            else jfdct(pl);
        }
        if (jblock(&b, yb, qy, hdcl_c, hdcl_s, 12, hacl_c, hacl_s, 162, &pred[0])) { free(b.p); return -1; }
        if (jblock(&b, cb, qc, hdcc_c, hdcc_s, 12, hacc_c, hacc_s, 162, &pred[1])) { free(b.p); return -1; }
        if (jblock(&b, cr, qc, hdcc_c, hdcc_s, 12, hacc_c, hacc_s, 162, &pred[2])) { free(b.p); return -1; }
    }
    if (jflush(&b)) { free(b.p); return -1; }
    uint8_t eoi[2] = {0xff, 0xd9};
    if (jput(&b, eoi, 2)) { free(b.p); return -1; }
    *out = b.p; *n = b.n; return 0;
}
