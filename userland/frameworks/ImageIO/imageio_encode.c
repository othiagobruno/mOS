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
