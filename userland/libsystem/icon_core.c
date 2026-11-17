/* mOS — leitura de .icns (ver icon_core.h). Código original do mOS. */
#include "icon_core.h"

#define FCC(a, b, c, d) ((uint32_t)(a) << 24 | (uint32_t)(b) << 16 | (uint32_t)(c) << 8 | (uint32_t)(d))
static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }

int ic_is_icns(const uint8_t *f, long n) { return f && n >= 8 && be32(f) == FCC('i', 'c', 'n', 's'); }

int ic_side_of(uint32_t t) {
    switch (t) {
    case FCC('i', 's', '3', '2'): case FCC('i', 'c', 'p', '4'): case FCC('i', 'c', '0', '4'): return 16;
    case FCC('i', 'l', '3', '2'): case FCC('i', 'c', 'p', '5'): case FCC('i', 'c', '0', '5'): case FCC('i', 'c', '1', '1'): return 32;
    case FCC('i', 'h', '3', '2'): return 48;
    case FCC('i', 'c', 'p', '6'): case FCC('i', 'c', '1', '2'): return 64;
    case FCC('i', 't', '3', '2'): case FCC('i', 'c', '0', '7'): return 128;
    case FCC('i', 'c', '0', '8'): case FCC('i', 'c', '1', '3'): return 256;
    case FCC('i', 'c', '0', '9'): case FCC('i', 'c', '1', '4'): return 512;
    case FCC('i', 'c', '1', '0'): return 1024;
    default: return 0;
    }
}
static uint32_t mask_for(uint32_t t) {
    switch (t) {
    case FCC('i', 's', '3', '2'): return FCC('s', '8', 'm', 'k');
    case FCC('i', 'l', '3', '2'): return FCC('l', '8', 'm', 'k');
    case FCC('i', 'h', '3', '2'): return FCC('h', '8', 'm', 'k');
    case FCC('i', 't', '3', '2'): return FCC('t', '8', 'm', 'k');
    default: return 0;
    }
}
static int is_png(const uint8_t *d, long n) { return n >= 8 && d[0] == 0x89 && d[1] == 'P' && d[2] == 'N' && d[3] == 'G'; }

/* acha um elemento pelo tipo */
static int find(const uint8_t *f, long total, uint32_t type, long *off, long *len) {
    for (long p = 8; p + 8 <= total;) {
        long el = (long)be32(f + p + 4);
        if (el < 8 || p + el > total) return 0;
        if (be32(f + p) == type) { *off = p + 8; *len = el - 8; return 1; }
        p += el;
    }
    return 0;
}

int ic_pick_best(const uint8_t *f, long n, int want, ic_pick *out) {
    if (!ic_is_icns(f, n) || !out) return 0;
    long total = (long)be32(f + 4);
    if (total > n || total < 8) total = n;
    ic_pick best = {0, 0, 0, 0, 0, 0, 0};
    for (long p = 8; p + 8 <= total;) {
        uint32_t t = be32(f + p);
        long el = (long)be32(f + p + 4);
        if (el < 8 || p + el > total) break;
        int side = ic_side_of(t);
        const uint8_t *d = f + p + 8;
        long dn = el - 8;
        ic_pick c = {0, side, p + 8, dn, 0, 0, 0};
        if (side) {
            if (is_png(d, dn)) c.kind = IC_PNG;
            else if (dn >= 4 && be32(d) == FCC('A', 'R', 'G', 'B')) c.kind = IC_ARGB;
            else if (mask_for(t)) {
                c.kind = IC_RLE;
                c.it32 = t == FCC('i', 't', '3', '2');
                find(f, total, mask_for(t), &c.mask_off, &c.mask_len);
            }
        }
        if (c.kind) {
            /* prefere o menor que cobre want; entre os que não cobrem, o maior; no empate, PNG */
            int cover = c.side >= want, bcover = best.side >= want;
            int better = !best.kind || (cover && !bcover) || (cover && bcover && c.side < best.side) ||
                         (!cover && !bcover && c.side > best.side) || (c.side == best.side && c.kind == IC_PNG && best.kind != IC_PNG);
            if (better) best = c;
        }
        p += el;
    }
    if (!best.kind) return 0;
    *out = best;
    return 1;
}

/* descompacta um canal: b < 128 copia b+1 bytes; senão repete o próximo byte b-125 vezes */
static long unpack(const uint8_t *d, long n, long pos, uint8_t *dst, long count) {
    long got = 0;
    while (got < count && pos < n) {
        uint8_t b = d[pos++];
        if (b < 0x80) {
            long k = b + 1;
            if (pos + k > n || got + k > count) return -1;
            for (long i = 0; i < k; i++) dst[got++] = d[pos++];
        } else {
            long k = b - 125;
            if (pos >= n || got + k > count) return -1;
            uint8_t v = d[pos++];
            for (long i = 0; i < k; i++) dst[got++] = v;
        }
    }
    return got == count ? pos : -1;
}

int ic_decode_rle(const uint8_t *d, long n, int side, int it32, const uint8_t *mask, long mlen, uint32_t *argb, uint8_t *scratch) {
    if (!d || side <= 0 || !argb || !scratch) return 0;
    long px = (long)side * side;
    uint8_t *ch[3] = {scratch, scratch + px, scratch + 2 * px};
    if (n == px * 3) {                          /* sem compressão (alguns il32 antigos) */
        for (long i = 0; i < px; i++) { ch[0][i] = d[i * 3]; ch[1][i] = d[i * 3 + 1]; ch[2][i] = d[i * 3 + 2]; }
    } else {
        long pos = it32 ? 4 : 0;
        for (int c = 0; c < 3; c++) {
            pos = unpack(d, n, pos, ch[c], px);
            if (pos < 0) return 0;
        }
    }
    for (long i = 0; i < px; i++) {
        uint32_t a = mask && mlen >= px ? mask[i] : 255;
        argb[i] = a << 24 | (uint32_t)ch[0][i] << 16 | (uint32_t)ch[1][i] << 8 | ch[2][i];
    }
    return 1;
}

int ic_decode_argb(const uint8_t *d, long n, int side, uint32_t *argb, uint8_t *scratch) {
    if (!d || n < 4 || be32(d) != FCC('A', 'R', 'G', 'B') || side <= 0 || !argb || !scratch) return 0;
    long px = (long)side * side, pos = 4;
    for (int c = 0; c < 4; c++) {
        pos = unpack(d, n, pos, scratch + c * px, px);
        if (pos < 0) return 0;
    }
    for (long i = 0; i < px; i++)
        argb[i] = (uint32_t)scratch[i] << 24 | (uint32_t)scratch[px + i] << 16 | (uint32_t)scratch[2 * px + i] << 8 | scratch[3 * px + i];
    return 1;
}
