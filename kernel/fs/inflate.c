/* mOS — descompressor DEFLATE/zlib (RFC 1950/1951), usado por UDZO e decmpfs */
#include "fs.h"

typedef struct {
    const u8 *src, *end;
    u32 bits, nbits;
    u8 *dst;
    u64 pos, cap;
    bool err;
} inf_t;

typedef struct {
    u16 count[16];
    u16 sym[320];
} huff_t;

static u32 getbits(inf_t *s, u32 n) {
    while (s->nbits < n) {
        if (s->src >= s->end) { s->err = true; return 0; }
        s->bits |= (u32)*s->src++ << s->nbits;
        s->nbits += 8;
    }
    u32 v = s->bits & ((1u << n) - 1);
    s->bits >>= n;
    s->nbits -= n;
    return v;
}

static bool build(huff_t *h, const u8 *len, u32 n) {
    u16 offs[16];
    memset(h->count, 0, sizeof(h->count));
    for (u32 i = 0; i < n; i++) h->count[len[i]]++;
    h->count[0] = 0;
    offs[1] = 0;
    for (int i = 1; i < 15; i++) offs[i + 1] = offs[i] + h->count[i];
    for (u32 i = 0; i < n; i++)
        if (len[i]) h->sym[offs[len[i]]++] = (u16)i;
    return true;
}

static int decode(inf_t *s, const huff_t *h) {
    int code = 0, first = 0, index = 0;
    for (int len = 1; len < 16; len++) {
        code |= (int)getbits(s, 1);
        int count = h->count[len];
        if (code - count < first) return h->sym[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
        if (s->err) return -1;
    }
    s->err = true;
    return -1;
}

static const u16 LBASE[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
static const u8  LEXT[29]  = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const u16 DBASE[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
static const u8  DEXT[30]  = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

static bool codes(inf_t *s, const huff_t *lit, const huff_t *dist) {
    for (;;) {
        int sym = decode(s, lit);
        if (sym < 0 || s->err) return false;
        if (sym < 256) {
            if (s->pos >= s->cap) return false;
            s->dst[s->pos++] = (u8)sym;
        } else if (sym == 256) {
            return true;
        } else {
            sym -= 257;
            if (sym >= 29) return false;
            u32 len = LBASE[sym] + getbits(s, LEXT[sym]);
            int ds = decode(s, dist);
            if (ds < 0 || ds >= 30) return false;
            u32 d = DBASE[ds] + getbits(s, DEXT[ds]);
            if (s->err || d > s->pos || len > s->cap - s->pos) return false;
            u8 *o = s->dst + s->pos;
            const u8 *from = o - d;
            for (u32 i = 0; i < len; i++) o[i] = from[i];
            s->pos += len;
        }
    }
}

i64 zlib_decompress(const u8 *src, u64 slen, u8 *dst, u64 dlen) {
    inf_t s = {src, src + slen, 0, 0, dst, 0, dlen, false};
    if (slen >= 2 && (src[0] & 0x0F) == 8 && ((src[0] << 8) | src[1]) % 31 == 0) {
        if (src[1] & 0x20) return -1; /* dicionário pré-definido: não usado pelo macOS */
        s.src += 2;
    }
    huff_t lit, dist;
    u8 lens[320];
    for (;;) {
        u32 last = getbits(&s, 1);
        u32 type = getbits(&s, 2);
        if (s.err) return -1;
        if (type == 0) {
            s.bits = 0; s.nbits = 0;
            if (s.end - s.src < 4) return -1;
            u32 len = le16(s.src), nlen = le16(s.src + 2);
            s.src += 4;
            if ((len ^ 0xFFFF) != nlen || (u64)(s.end - s.src) < len || len > s.cap - s.pos) return -1;
            memcpy(s.dst + s.pos, s.src, len);
            s.src += len;
            s.pos += len;
        } else if (type == 1) {
            u32 i = 0;
            for (; i < 144; i++) lens[i] = 8;
            for (; i < 256; i++) lens[i] = 9;
            for (; i < 280; i++) lens[i] = 7;
            for (; i < 288; i++) lens[i] = 8;
            build(&lit, lens, 288);
            for (i = 0; i < 30; i++) lens[i] = 5;
            build(&dist, lens, 30);
            if (!codes(&s, &lit, &dist)) return -1;
        } else if (type == 2) {
            static const u8 ord[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
            u32 nlen = getbits(&s, 5) + 257, ndist = getbits(&s, 5) + 1, ncode = getbits(&s, 4) + 4;
            if (s.err || nlen > 286 || ndist > 30) return -1;
            memset(lens, 0, sizeof(lens));
            for (u32 i = 0; i < ncode; i++) lens[ord[i]] = (u8)getbits(&s, 3);
            huff_t lc;
            build(&lc, lens, 19);
            u32 idx = 0;
            while (idx < nlen + ndist) {
                int sym = decode(&s, &lc);
                if (sym < 0 || s.err) return -1;
                if (sym < 16) { lens[idx++] = (u8)sym; continue; }
                u8 v = 0;
                u32 rep;
                if (sym == 16) {
                    if (!idx) return -1;
                    v = lens[idx - 1];
                    rep = 3 + getbits(&s, 2);
                } else if (sym == 17) rep = 3 + getbits(&s, 3);
                else rep = 11 + getbits(&s, 7);
                if (idx + rep > nlen + ndist) return -1;
                while (rep--) lens[idx++] = v;
            }
            build(&lit, lens, nlen);
            build(&dist, lens + nlen, ndist);
            if (!codes(&s, &lit, &dist)) return -1;
        } else {
            return -1;
        }
        if (last) break;
    }
    return (i64)s.pos;
}

/* ---- ADC (Apple Data Compression), o formato do antigo UDCO ---- */
i64 adc_decompress(const u8 *src, u64 slen, u8 *dst, u64 dlen) {
    u64 i = 0, o = 0;
    while (i < slen) {
        u8 b = src[i];
        u32 len, off;
        if (b & 0x80) {
            len = (b & 0x7F) + 1u;
            if (i + 1 + len > slen || o + len > dlen) return -1;
            memcpy(dst + o, src + i + 1, len);
            i += 1 + len; o += len;
            continue;
        }
        if (b & 0x40) {
            if (i + 3 > slen) return -1;
            len = (b & 0x3F) + 4u;
            off = ((u32)src[i + 1] << 8 | src[i + 2]) + 1;
            i += 3;
        } else {
            if (i + 2 > slen) return -1;
            len = ((b >> 2) & 0x0F) + 3u;
            off = ((u32)(b & 3) << 8 | src[i + 1]) + 1;
            i += 2;
        }
        if (off > o || o + len > dlen) return -1;
        for (u32 k = 0; k < len; k++, o++) dst[o] = dst[o - off];
    }
    return (i64)o;
}
