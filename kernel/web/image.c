/* mOS — Horizonte: decodificadores de imagem (PNG com inflate próprio, JPEG baseline/progressivo, GIF, BMP, PPM) */
#include "web.h"

#define MAX_PIXELS (6000u * 6000u)

/* ============================================================ inflate (RFC 1951) */
typedef struct {
    const u8 *in;
    usize n, pos;
    u32 bb;
    int bc;
    u8 *out;
    usize olen, ocap;
    bool err, fixed_cap;
} inf_t;

typedef struct { u16 count[16]; u16 sym[320]; } huff_t;

static int getbit(inf_t *s) {
    if (!s->bc) {
        if (s->pos >= s->n) { s->err = true; return 0; }
        s->bb = s->in[s->pos++];
        s->bc = 8;
    }
    int b = s->bb & 1;
    s->bb >>= 1;
    s->bc--;
    return b;
}

static u32 getbits(inf_t *s, int n) {
    u32 v = s->bb;
    while (s->bc < n) {
        if (s->pos >= s->n) { s->err = true; return 0; }
        v |= (u32)s->in[s->pos++] << s->bc;
        s->bc += 8;
    }
    s->bb = v >> n;
    s->bc -= n;
    return v & ((1u << n) - 1);
}

static int hdecode(inf_t *s, huff_t *h) {
    int code = 0, first = 0, index = 0;
    for (int len = 1; len < 16; len++) {
        code |= getbit(s);
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

static int hbuild(huff_t *h, const u8 *len, int n) {
    for (int i = 0; i < 16; i++) h->count[i] = 0;
    for (int i = 0; i < n; i++) h->count[len[i]]++;
    if (h->count[0] == n) return 0;
    int left = 1;
    for (int i = 1; i < 16; i++) { left <<= 1; left -= h->count[i]; if (left < 0) return left; }
    u16 offs[16];
    offs[1] = 0;
    for (int i = 1; i < 15; i++) offs[i + 1] = offs[i] + h->count[i];
    for (int i = 0; i < n; i++) if (len[i]) h->sym[offs[len[i]]++] = (u16)i;
    return left;
}

static bool out_byte(inf_t *s, u8 b) {
    if (s->olen >= s->ocap) {
        if (s->fixed_cap) { s->err = true; return false; }
        usize nc = s->ocap ? s->ocap * 2 : 65536;
        u8 *no = kmalloc(nc);
        if (!no) { s->err = true; return false; }
        if (s->out) { memcpy(no, s->out, s->olen); kfree(s->out); }
        s->out = no;
        s->ocap = nc;
    }
    s->out[s->olen++] = b;
    return true;
}

static const u16 lbase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
static const u16 lext[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const u16 dbase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
static const u16 dext[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

static bool codes(inf_t *s, huff_t *lc, huff_t *dc) {
    for (;;) {
        int sym = hdecode(s, lc);
        if (sym < 0 || s->err) return false;
        if (sym < 256) { if (!out_byte(s, (u8)sym)) return false; continue; }
        if (sym == 256) return true;
        sym -= 257;
        if (sym >= 29) return false;
        int len = lbase[sym] + (int)getbits(s, lext[sym]);
        int ds = hdecode(s, dc);
        if (ds < 0 || ds >= 30) return false;
        usize dist = dbase[ds] + getbits(s, dext[ds]);
        if (dist > s->olen) return false;
        /* cópia (pode sobrepor) */
        if (s->olen + (usize)len > s->ocap) {
            for (int i = 0; i < len; i++) if (!out_byte(s, s->out[s->olen - dist])) return false;
        } else {
            u8 *o = s->out + s->olen;
            const u8 *src = o - dist;
            for (int i = 0; i < len; i++) o[i] = src[i];
            s->olen += (usize)len;
        }
    }
}

static bool inflate_raw(inf_t *s) {
    static huff_t flc, fdc;
    static bool fixed_ready;
    int last;
    do {
        last = getbit(s);
        int type = (int)getbits(s, 2);
        if (s->err) return false;
        if (type == 0) {
            s->bb = 0; s->bc = 0;
            if (s->pos + 4 > s->n) return false;
            u32 len = s->in[s->pos] | (u32)s->in[s->pos + 1] << 8;
            s->pos += 4;
            if (s->pos + len > s->n) return false;
            for (u32 i = 0; i < len; i++) if (!out_byte(s, s->in[s->pos + i])) return false;
            s->pos += len;
        } else if (type == 1) {
            if (!fixed_ready) {
                u8 l[288];
                int i = 0;
                for (; i < 144; i++) l[i] = 8;
                for (; i < 256; i++) l[i] = 9;
                for (; i < 280; i++) l[i] = 7;
                for (; i < 288; i++) l[i] = 8;
                hbuild(&flc, l, 288);
                for (i = 0; i < 30; i++) l[i] = 5;
                hbuild(&fdc, l, 30);
                fixed_ready = true;
            }
            if (!codes(s, &flc, &fdc)) return false;
        } else if (type == 2) {
            static const u8 ord[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
            int nlen = (int)getbits(s, 5) + 257, ndist = (int)getbits(s, 5) + 1, ncode = (int)getbits(s, 4) + 4;
            if (nlen > 286 || ndist > 30) return false;
            u8 lens[320];
            memset(lens, 0, sizeof lens);
            for (int i = 0; i < ncode; i++) lens[ord[i]] = (u8)getbits(s, 3);
            huff_t lc, dc;
            if (hbuild(&lc, lens, 19) != 0) return false;
            int idx = 0;
            while (idx < nlen + ndist) {
                int sym = hdecode(s, &lc);
                if (sym < 0) return false;
                if (sym < 16) lens[idx++] = (u8)sym;
                else {
                    int len = 0, rep;
                    if (sym == 16) { if (!idx) return false; len = lens[idx - 1]; rep = 3 + (int)getbits(s, 2); }
                    else if (sym == 17) rep = 3 + (int)getbits(s, 3);
                    else rep = 11 + (int)getbits(s, 7);
                    if (idx + rep > nlen + ndist) return false;
                    while (rep--) lens[idx++] = (u8)len;
                }
            }
            int e1 = hbuild(&lc, lens, nlen);
            if (e1 < 0 || (e1 > 0 && nlen - lc.count[0] != 1)) return false;
            int e2 = hbuild(&dc, lens + nlen, ndist);
            if (e2 < 0 || (e2 > 0 && ndist - dc.count[0] != 1)) { if (e2 < 0) return false; }
            if (!codes(s, &lc, &dc)) return false;
        } else return false;
    } while (!last && !s->err);
    return !s->err;
}

/* zlib: devolve bytes descomprimidos em *out (kmalloc) */
static bool zlib_inflate(const u8 *d, usize n, u8 *out, usize cap, usize *olen) {
    if (n < 2 || (d[0] & 15) != 8 || ((d[0] << 8) | d[1]) % 31) return false;
    inf_t s = {0};
    s.in = d + 2;
    s.n = n - 2;
    s.out = out;
    s.ocap = cap;
    s.fixed_cap = true;
    bool ok = inflate_raw(&s);
    *olen = s.olen;
    return ok || s.olen > 0;
}

/* ============================================================ PNG */
static u32 be32(const u8 *p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }

static u8 paeth(int a, int b, int c) {
    int p = a + b - c, pa = p > a ? p - a : a - p, pb = p > b ? p - b : b - p, pc = p > c ? p - c : c - p;
    if (pa <= pb && pa <= pc) return (u8)a;
    if (pb <= pc) return (u8)b;
    return (u8)c;
}

static bool unfilter(u8 *rows, int h, usize rowbytes, int bpp) {
    u8 *prev = NULL;
    for (int y = 0; y < h; y++) {
        u8 *r = rows + (usize)y * (rowbytes + 1);
        u8 f = r[0];
        u8 *p = r + 1;
        u8 *q = prev ? prev + 1 : NULL;
        switch (f) {
        case 0: break;
        case 1: for (usize i = (usize)bpp; i < rowbytes; i++) p[i] = (u8)(p[i] + p[i - bpp]); break;
        case 2: if (q) for (usize i = 0; i < rowbytes; i++) p[i] = (u8)(p[i] + q[i]); break;
        case 3:
            for (usize i = 0; i < rowbytes; i++) {
                int a = i >= (usize)bpp ? p[i - bpp] : 0, b = q ? q[i] : 0;
                p[i] = (u8)(p[i] + ((a + b) >> 1));
            }
            break;
        case 4:
            for (usize i = 0; i < rowbytes; i++) {
                int a = i >= (usize)bpp ? p[i - bpp] : 0, b = q ? q[i] : 0, c = (q && i >= (usize)bpp) ? q[i - bpp] : 0;
                p[i] = (u8)(p[i] + paeth(a, b, c));
            }
            break;
        default: return false;
        }
        prev = r;
    }
    return true;
}

typedef struct {
    int w, h, depth, ctype, ch;
    u32 pal[256];
    int npal;
    bool has_key;
    u16 key[3];
} png_t;

static int sample(const u8 *row, int x, int c, png_t *P) {
    int d = P->depth;
    if (d == 8) return row[x * P->ch + c];
    if (d == 16) return row[(x * P->ch + c) * 2];
    int idx = x * P->ch + c;
    int bit = idx * d;
    int v = (row[bit >> 3] >> (8 - d - (bit & 7))) & ((1 << d) - 1);
    if (P->ctype == 3) return v;
    return v * 255 / ((1 << d) - 1);
}
static int sample16(const u8 *row, int x, int c, png_t *P) {
    if (P->depth == 16) return row[(x * P->ch + c) * 2] << 8 | row[(x * P->ch + c) * 2 + 1];
    int v = sample(row, x, c, P);
    return P->ctype == 3 ? v : v * 257;
}

static u32 png_px(const u8 *row, int x, png_t *P) {
    int r, g, b, a = 255;
    switch (P->ctype) {
    case 0:
        r = g = b = sample(row, x, 0, P);
        if (P->has_key && sample16(row, x, 0, P) == (P->depth == 16 ? P->key[0] : P->key[0] * 257 / (P->depth < 8 ? 257 : 1) * (P->depth < 8 ? 1 : 1))) a = 0;
        break;
    case 2:
        r = sample(row, x, 0, P); g = sample(row, x, 1, P); b = sample(row, x, 2, P);
        if (P->has_key && P->depth == 8 && r == P->key[0] && g == P->key[1] && b == P->key[2]) a = 0;
        break;
    case 3: {
        int i = sample(row, x, 0, P);
        u32 c = i < P->npal ? P->pal[i] : 0xFF000000u;
        return color_premul(c);
    }
    case 4: r = g = b = sample(row, x, 0, P); a = sample(row, x, 1, P); break;
    default: r = sample(row, x, 0, P); g = sample(row, x, 1, P); b = sample(row, x, 2, P); a = sample(row, x, 3, P); break;
    }
    return color_premul(RGBA(r, g, b, a));
}

static surface_t *decode_png(const u8 *d, usize n) {
    if (n < 33 || memcmp(d, "\x89PNG\r\n\x1a\n", 8)) return NULL;
    png_t P = {0};
    usize pos = 8;
    u8 *idat = NULL;
    usize ilen = 0, icap = 0;
    int interlace = 0;
    for (int i = 0; i < 256; i++) P.pal[i] = 0xFF000000u;
    while (pos + 12 <= n) {
        u32 len = be32(d + pos);
        const u8 *type = d + pos + 4;
        const u8 *data = d + pos + 8;
        if (pos + 12 + (usize)len > n) break;
        if (!memcmp(type, "IHDR", 4) && len >= 13) {
            P.w = (int)be32(data); P.h = (int)be32(data + 4);
            P.depth = data[8]; P.ctype = data[9]; interlace = data[12];
        } else if (!memcmp(type, "PLTE", 4)) {
            P.npal = (int)MIN(len / 3, 256u);
            for (int i = 0; i < P.npal; i++) P.pal[i] = RGB(data[i * 3], data[i * 3 + 1], data[i * 3 + 2]);
        } else if (!memcmp(type, "tRNS", 4)) {
            if (P.ctype == 3) { for (u32 i = 0; i < len && i < 256; i++) P.pal[i] = WITH_ALPHA(P.pal[i], data[i]); }
            else if (P.ctype == 0 && len >= 2) { P.has_key = true; P.key[0] = (u16)(data[0] << 8 | data[1]); }
            else if (P.ctype == 2 && len >= 6) {
                P.has_key = true;
                for (int k = 0; k < 3; k++) P.key[k] = (u16)(P.depth == 16 ? (data[k * 2] << 8 | data[k * 2 + 1]) : data[k * 2 + 1]);
            }
        } else if (!memcmp(type, "IDAT", 4)) {
            if (ilen + len > icap) {
                usize nc = MAX(icap * 2, ilen + len + 65536);
                u8 *ni = kmalloc(nc);
                if (!ni) { kfree(idat); return NULL; }
                if (idat) { memcpy(ni, idat, ilen); kfree(idat); }
                idat = ni;
                icap = nc;
            }
            memcpy(idat + ilen, data, len);
            ilen += len;
        } else if (!memcmp(type, "IEND", 4)) break;
        pos += 12 + (usize)len;
    }
    if (!idat || P.w <= 0 || P.h <= 0 || (u64)P.w * P.h > MAX_PIXELS) { kfree(idat); return NULL; }
    switch (P.ctype) { case 0: P.ch = 1; break; case 2: P.ch = 3; break; case 3: P.ch = 1; break; case 4: P.ch = 2; break; case 6: P.ch = 4; break; default: kfree(idat); return NULL; }
    if (P.depth != 1 && P.depth != 2 && P.depth != 4 && P.depth != 8 && P.depth != 16) { kfree(idat); return NULL; }
    int bpp = MAX(1, P.ch * P.depth / 8);
    /* tamanho total esperado */
    usize total = 0;
    static const int ax[7] = {0, 4, 0, 2, 0, 1, 0}, ay[7] = {0, 0, 4, 0, 2, 0, 1}, dx7[7] = {8, 8, 4, 4, 2, 2, 1}, dy7[7] = {8, 8, 8, 4, 4, 2, 2};
    if (!interlace) total = (usize)P.h * (((usize)P.w * P.ch * P.depth + 7) / 8 + 1);
    else for (int p = 0; p < 7; p++) {
        int pw = (P.w - ax[p] + dx7[p] - 1) / dx7[p], ph = (P.h - ay[p] + dy7[p] - 1) / dy7[p];
        if (pw > 0 && ph > 0) total += (usize)ph * (((usize)pw * P.ch * P.depth + 7) / 8 + 1);
    }
    u8 *raw = kmalloc(total + 16);
    if (!raw) { kfree(idat); return NULL; }
    memset(raw, 0, total);
    usize got = 0;
    bool ok = zlib_inflate(idat, ilen, raw, total, &got);
    kfree(idat);
    if (!ok) { kfree(raw); return NULL; }
    surface_t *s = surface_new(P.w, P.h);
    if (!s) { kfree(raw); return NULL; }
    if (!interlace) {
        usize rb = ((usize)P.w * P.ch * P.depth + 7) / 8;
        unfilter(raw, P.h, rb, bpp);
        for (int y = 0; y < P.h; y++) {
            const u8 *row = raw + (usize)y * (rb + 1) + 1;
            u32 *o = s->px + (usize)y * s->stride;
            for (int x = 0; x < P.w; x++) o[x] = png_px(row, x, &P);
        }
    } else {
        usize off = 0;
        for (int p = 0; p < 7; p++) {
            int pw = (P.w - ax[p] + dx7[p] - 1) / dx7[p], ph = (P.h - ay[p] + dy7[p] - 1) / dy7[p];
            if (pw <= 0 || ph <= 0) continue;
            usize rb = ((usize)pw * P.ch * P.depth + 7) / 8;
            unfilter(raw + off, ph, rb, bpp);
            for (int y = 0; y < ph; y++) {
                const u8 *row = raw + off + (usize)y * (rb + 1) + 1;
                int yy = ay[p] + y * dy7[p];
                for (int x = 0; x < pw; x++) {
                    int xx = ax[p] + x * dx7[p];
                    s->px[(usize)yy * s->stride + xx] = png_px(row, x, &P);
                }
            }
            off += (usize)ph * (rb + 1);
        }
    }
    kfree(raw);
    return s;
}

/* ============================================================ JPEG */
static const u8 dezig[64 + 16] = {
    0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5, 12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63,
    63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63};

typedef struct {
    u8 fast[512];          /* 9 bits → índice+1 (0 = lento) */
    u8 vals[256];
    u16 codes[256];
    u8 sizes[257];
    int maxcode[18];
    int delta[17];
    bool ok;
} jhuff_t;

typedef struct {
    int id, h, v, tq;
    int bw, bh;            /* blocos por linha / coluna (com preenchimento de MCU) */
    short *coef;
    int dc_pred;
    int td, ta;
    u8 *plane;
    int pw, ph;
} jcomp_t;

typedef struct {
    const u8 *d;
    usize n, pos;
    u32 bits;
    int nbits;
    bool marker;
    jhuff_t hdc[4], hac[4];
    u16 qt[4][64];
    jcomp_t c[4];
    int nc, w, h, hmax, vmax, mcux, mcuy;
    bool progressive;
    int restart;
    int eobrun;
    int ss, se, ah, al;
    int scomp[4], ns;
    bool adobe_rgb;
} jpeg_t;

static void jbuild(jhuff_t *h, const u8 *counts) {
    int k = 0;
    for (int i = 0; i < 16; i++) for (int j = 0; j < counts[i]; j++) h->sizes[k++] = (u8)(i + 1);
    h->sizes[k] = 0;
    int code = 0, j = 0;
    for (int len = 1; len <= 16; len++) {
        h->delta[len] = k ? j - code : 0;
        if (h->sizes[j] == len) { while (h->sizes[j] == len) h->codes[j++] = (u16)code++; }
        h->maxcode[len] = code << (16 - len);
        code <<= 1;
    }
    h->maxcode[17] = 0x7fffffff;
    memset(h->fast, 0, sizeof h->fast);
    for (int i = 0; i < j; i++) {
        int s = h->sizes[i];
        if (s <= 9) {
            int c = h->codes[i] << (9 - s);
            int m = 1 << (9 - s);
            for (int q = 0; q < m; q++) h->fast[c + q] = (u8)(i + 1);
        }
    }
    h->ok = true;
}

static void jfill(jpeg_t *J) {
    while (J->nbits <= 24) {
        u32 b = 0;
        if (!J->marker && J->pos < J->n) {
            b = J->d[J->pos];
            if (b == 0xFF) {
                u8 nx = J->pos + 1 < J->n ? J->d[J->pos + 1] : 0;
                if (nx == 0) J->pos += 2;
                else { J->marker = true; b = 0; }
            } else J->pos++;
        }
        J->bits |= b << (24 - J->nbits);
        J->nbits += 8;
    }
}

static int jdecode(jpeg_t *J, jhuff_t *h) {
    if (!h->ok) return -1;
    if (J->nbits < 16) jfill(J);
    int c = (int)(J->bits >> 23) & 511;
    int k = h->fast[c];
    if (k) {
        k--;
        int s = h->sizes[k];
        J->bits <<= s;
        J->nbits -= s;
        return h->vals[k];
    }
    u32 temp = J->bits >> 16;
    int len;
    for (len = 10; len < 17; len++) if ((int)temp < h->maxcode[len]) break;
    if (len == 17) { J->nbits -= 16; J->bits <<= 16; return -1; }
    int code = (int)(J->bits >> (32 - len));
    int idx = code + h->delta[len];
    J->bits <<= len;
    J->nbits -= len;
    if (idx < 0 || idx > 255) return -1;
    return h->vals[idx];
}

static int jbits(jpeg_t *J, int n) {
    if (!n) return 0;
    if (J->nbits < n) jfill(J);
    u32 v = J->bits >> (32 - n);
    J->bits <<= n;
    J->nbits -= n;
    return (int)v;
}
static int jextend(int v, int n) { return v < (1 << (n - 1)) ? v - (1 << n) + 1 : v; }
static int jreceive(jpeg_t *J, int n) { return n ? jextend(jbits(J, n), n) : 0; }

static void jdecode_block(jpeg_t *J, jcomp_t *c, short *blk) {
    if (!J->progressive) {
        int t = jdecode(J, &J->hdc[c->td]);
        int diff = t > 0 ? jreceive(J, t) : 0;
        c->dc_pred += diff;
        blk[0] = (short)c->dc_pred;
        for (int k = 1; k < 64;) {
            int rs = jdecode(J, &J->hac[c->ta]);
            if (rs < 0) return;
            int r = rs >> 4, s = rs & 15;
            if (!s) { if (r == 15) { k += 16; continue; } break; }
            k += r;
            if (k > 63) return;
            blk[dezig[k]] = (short)jreceive(J, s);
            k++;
        }
        return;
    }
    if (J->ss == 0) {
        if (J->ah == 0) {
            int t = jdecode(J, &J->hdc[c->td]);
            int diff = t > 0 ? jreceive(J, t) : 0;
            c->dc_pred += diff;
            blk[0] = (short)(c->dc_pred * (1 << J->al));
        } else if (jbits(J, 1)) blk[0] = (short)(blk[0] + (1 << J->al));
        return;
    }
    if (J->ah == 0) {
        if (J->eobrun) { J->eobrun--; return; }
        int k = J->ss;
        while (k <= J->se) {
            int rs = jdecode(J, &J->hac[c->ta]);
            if (rs < 0) return;
            int r = rs >> 4, s = rs & 15;
            if (!s) {
                if (r < 15) { J->eobrun = (1 << r) - 1; if (r) J->eobrun += jbits(J, r); break; }
                k += 16;
            } else {
                k += r;
                if (k > 63) return;
                blk[dezig[k++]] = (short)(jreceive(J, s) * (1 << J->al));
            }
        }
        return;
    }
    short bit = (short)(1 << J->al);
    if (J->eobrun) {
        J->eobrun--;
        for (int k = J->ss; k <= J->se; k++) {
            short *p = &blk[dezig[k]];
            if (*p && jbits(J, 1) && !(*p & bit)) *p = (short)(*p > 0 ? *p + bit : *p - bit);
        }
        return;
    }
    int k = J->ss;
    do {
        int rs = jdecode(J, &J->hac[c->ta]);
        if (rs < 0) return;
        int s = rs & 15, r = rs >> 4;
        if (!s) {
            if (r < 15) { J->eobrun = (1 << r) - 1; if (r) J->eobrun += jbits(J, r); r = 64; }
        } else s = jbits(J, 1) ? bit : -bit;
        while (k <= J->se) {
            short *p = &blk[dezig[k++]];
            if (*p) { if (jbits(J, 1) && !(*p & bit)) *p = (short)(*p > 0 ? *p + bit : *p - bit); }
            else { if (r == 0) { *p = (short)s; break; } r--; }
        }
    } while (k <= J->se);
}

static void jreset(jpeg_t *J) {
    J->bits = 0;
    J->nbits = 0;
    J->eobrun = 0;
    for (int i = 0; i < J->nc; i++) J->c[i].dc_pred = 0;
    /* pula o marcador RSTn */
    if (J->marker && J->pos + 1 < J->n && J->d[J->pos] == 0xFF && J->d[J->pos + 1] >= 0xD0 && J->d[J->pos + 1] <= 0xD7) J->pos += 2;
    else {
        while (J->pos + 1 < J->n && !(J->d[J->pos] == 0xFF && J->d[J->pos + 1] >= 0xD0 && J->d[J->pos + 1] <= 0xD7)) J->pos++;
        if (J->pos + 1 < J->n) J->pos += 2;
    }
    J->marker = false;
}

static void jscan(jpeg_t *J) {
    J->bits = 0; J->nbits = 0; J->marker = false; J->eobrun = 0;
    for (int i = 0; i < J->nc; i++) J->c[i].dc_pred = 0;
    int todo = J->restart ? J->restart : 0x7fffffff;
    if (J->ns == 1) {
        jcomp_t *c = &J->c[J->scomp[0]];
        int bw = (c->pw + 7) / 8, bh = (c->ph + 7) / 8;
        for (int by = 0; by < bh; by++)
            for (int bx = 0; bx < bw; bx++) {
                jdecode_block(J, c, c->coef + ((usize)by * c->bw + bx) * 64);
                if (--todo <= 0) { todo = J->restart; jreset(J); }
            }
        return;
    }
    for (int my = 0; my < J->mcuy; my++)
        for (int mx = 0; mx < J->mcux; mx++) {
            for (int s = 0; s < J->ns; s++) {
                jcomp_t *c = &J->c[J->scomp[s]];
                for (int v = 0; v < c->v; v++)
                    for (int h = 0; h < c->h; h++) {
                        int bx = mx * c->h + h, by = my * c->v + v;
                        jdecode_block(J, c, c->coef + ((usize)by * c->bw + bx) * 64);
                    }
            }
            if (--todo <= 0) { todo = J->restart; jreset(J); }
        }
}

static float idct_tab[8][8];
static bool idct_ready;

static void idct_block(const short *in, const u16 *q, u8 *out, int stride) {
    if (!idct_ready) {
        for (int x = 0; x < 8; x++)
            for (int u = 0; u < 8; u++) idct_tab[x][u] = (u ? 1.0f : 0.70710678f) * fcos((2 * x + 1) * u * PI_F / 16) / 2;
        idct_ready = true;
    }
    float f[64], t[64];
    bool ac = false;
    for (int i = 0; i < 64; i++) { f[i] = (float)in[i] * q[i]; if (i && in[i]) ac = true; }
    if (!ac) {
        int v = (int)(f[0] / 8 + 128.5f);
        v = CLAMP(v, 0, 255);
        for (int y = 0; y < 8; y++) memset(out + y * stride, v, 8);
        return;
    }
    /* linhas: t[v][x] = soma_u f[v][u] * c[x][u] */
    for (int v = 0; v < 8; v++) {
        const float *r = f + v * 8;
        for (int x = 0; x < 8; x++) {
            const float *c = idct_tab[x];
            t[v * 8 + x] = r[0] * c[0] + r[1] * c[1] + r[2] * c[2] + r[3] * c[3] + r[4] * c[4] + r[5] * c[5] + r[6] * c[6] + r[7] * c[7];
        }
    }
    for (int x = 0; x < 8; x++)
        for (int y = 0; y < 8; y++) {
            const float *c = idct_tab[y];
            float s = t[x] * c[0] + t[8 + x] * c[1] + t[16 + x] * c[2] + t[24 + x] * c[3] + t[32 + x] * c[4] + t[40 + x] * c[5] + t[48 + x] * c[6] + t[56 + x] * c[7];
            int v = (int)(s + 128.5f);
            out[y * stride + x] = (u8)CLAMP(v, 0, 255);
        }
}

static u16 be16(const u8 *p) { return (u16)(p[0] << 8 | p[1]); }

static surface_t *decode_jpeg(const u8 *d, usize n) {
    if (n < 4 || d[0] != 0xFF || d[1] != 0xD8) return NULL;
    jpeg_t *J = kcalloc(1, sizeof(jpeg_t));
    if (!J) return NULL;
    J->d = d;
    J->n = n;
    usize pos = 2;
    bool frame = false, ok = true;
    while (pos + 4 <= n && ok) {
        if (d[pos] != 0xFF) { pos++; continue; }
        u8 m = d[pos + 1];
        if (m == 0xFF) { pos++; continue; }
        if (m == 0xD9) break;
        if (m == 0xD8 || (m >= 0xD0 && m <= 0xD7) || m == 0x01) { pos += 2; continue; }
        usize len = be16(d + pos + 2);
        const u8 *p = d + pos + 4;
        if (pos + 2 + len > n) break;
        if (m == 0xDB) {
            usize k = 0;
            while (k + 1 <= len - 2) {
                int pq = p[k] >> 4, tq = p[k] & 3;
                k++;
                for (int i = 0; i < 64; i++) {
                    u16 v = pq ? be16(p + k + i * 2) : p[k + i];
                    J->qt[tq][dezig[i]] = v;
                }
                k += pq ? 128 : 64;
            }
        } else if (m == 0xC4) {
            usize k = 0;
            while (k + 17 <= len - 2) {
                int tc = p[k] >> 4, th = p[k] & 3;
                const u8 *cnt = p + k + 1;
                int tot = 0;
                for (int i = 0; i < 16; i++) tot += cnt[i];
                if (tot > 256 || k + 17 + tot > len - 2) { ok = false; break; }
                jhuff_t *h = tc ? &J->hac[th] : &J->hdc[th];
                memcpy(h->vals, p + k + 17, (usize)tot);
                jbuild(h, cnt);
                k += 17 + (usize)tot;
            }
        } else if (m == 0xC0 || m == 0xC1 || m == 0xC2) {
            J->progressive = m == 0xC2;
            if (p[0] != 8) { ok = false; break; }
            J->h = be16(p + 1);
            J->w = be16(p + 3);
            J->nc = p[5];
            if (J->nc != 1 && J->nc != 3) { ok = false; break; }
            if (!J->w || !J->h || (u64)J->w * J->h > MAX_PIXELS) { ok = false; break; }
            J->hmax = J->vmax = 1;
            for (int i = 0; i < J->nc; i++) {
                J->c[i].id = p[6 + i * 3];
                J->c[i].h = MAX(1, p[7 + i * 3] >> 4);
                J->c[i].v = MAX(1, p[7 + i * 3] & 15);
                J->c[i].tq = p[8 + i * 3] & 3;
                J->hmax = MAX(J->hmax, J->c[i].h);
                J->vmax = MAX(J->vmax, J->c[i].v);
            }
            J->mcux = (J->w + J->hmax * 8 - 1) / (J->hmax * 8);
            J->mcuy = (J->h + J->vmax * 8 - 1) / (J->vmax * 8);
            for (int i = 0; i < J->nc; i++) {
                jcomp_t *c = &J->c[i];
                c->bw = J->mcux * c->h;
                c->bh = J->mcuy * c->v;
                c->pw = (J->w * c->h + J->hmax - 1) / J->hmax;
                c->ph = (J->h * c->v + J->vmax - 1) / J->vmax;
                c->coef = kcalloc((usize)c->bw * c->bh * 64, sizeof(short));
                if (!c->coef) { ok = false; break; }
            }
            frame = true;
        } else if (m >= 0xC3 && m <= 0xCF && m != 0xC4 && m != 0xC8 && m != 0xCC) {
            ok = false; /* sem perdas / aritmético: não suportado */
            break;
        } else if (m == 0xDD) {
            J->restart = be16(p);
        } else if (m == 0xEE && len >= 12 && !memcmp(p, "Adobe", 5)) {
            J->adobe_rgb = p[11] == 0;
        } else if (m == 0xDA) {
            if (!frame) { ok = false; break; }
            J->ns = p[0];
            if (J->ns < 1 || J->ns > 4) { ok = false; break; }
            for (int i = 0; i < J->ns; i++) {
                int id = p[1 + i * 2], tb = p[2 + i * 2];
                int ci = -1;
                for (int k = 0; k < J->nc; k++) if (J->c[k].id == id) ci = k;
                if (ci < 0) { ok = false; break; }
                J->scomp[i] = ci;
                J->c[ci].td = tb >> 4 & 3;
                J->c[ci].ta = tb & 3;
            }
            if (!ok) break;
            const u8 *q = p + 1 + J->ns * 2;
            J->ss = q[0];
            J->se = MIN(q[1], 63);
            J->ah = q[2] >> 4;
            J->al = q[2] & 15;
            J->pos = pos + 2 + len;
            jscan(J);
            /* procura o próximo marcador */
            pos = J->pos;
            while (pos + 1 < n && !(d[pos] == 0xFF && d[pos + 1] != 0 && !(d[pos + 1] >= 0xD0 && d[pos + 1] <= 0xD7))) pos++;
            continue;
        }
        pos += 2 + len;
    }
    surface_t *s = NULL;
    if (frame && ok) {
        /* IDCT de todos os blocos para planos */
        for (int i = 0; i < J->nc && ok; i++) {
            jcomp_t *c = &J->c[i];
            int stride = c->bw * 8;
            c->plane = kmalloc((usize)stride * c->bh * 8);
            if (!c->plane) { ok = false; break; }
            for (int by = 0; by < c->bh; by++)
                for (int bx = 0; bx < c->bw; bx++)
                    idct_block(c->coef + ((usize)by * c->bw + bx) * 64, J->qt[c->tq], c->plane + (usize)by * 8 * stride + bx * 8, stride);
        }
        if (ok) s = surface_new(J->w, J->h);
        if (s) {
            for (int y = 0; y < J->h; y++) {
                u32 *o = s->px + (usize)y * s->stride;
                if (J->nc == 1) {
                    jcomp_t *c = &J->c[0];
                    const u8 *r = c->plane + (usize)y * c->bw * 8;
                    for (int x = 0; x < J->w; x++) o[x] = RGB(r[x], r[x], r[x]);
                    continue;
                }
                const u8 *rows[3];
                int sx[3];
                for (int k = 0; k < 3; k++) {
                    jcomp_t *c = &J->c[k];
                    int yy = y * c->v / J->vmax;
                    rows[k] = c->plane + (usize)yy * c->bw * 8;
                    sx[k] = c->h;
                }
                for (int x = 0; x < J->w; x++) {
                    int Y = rows[0][x * sx[0] / J->hmax];
                    int cb = rows[1][x * sx[1] / J->hmax] - 128;
                    int cr = rows[2][x * sx[2] / J->hmax] - 128;
                    int r, g, b;
                    if (J->adobe_rgb) { r = Y; g = cb + 128; b = cr + 128; }
                    else {
                        r = Y + ((91881 * cr) >> 16);
                        g = Y - ((22554 * cb + 46802 * cr) >> 16);
                        b = Y + ((116130 * cb) >> 16);
                    }
                    o[x] = RGB(CLAMP(r, 0, 255), CLAMP(g, 0, 255), CLAMP(b, 0, 255));
                }
            }
        }
    }
    for (int i = 0; i < 4; i++) { kfree(J->c[i].coef); kfree(J->c[i].plane); }
    kfree(J);
    return s;
}

/* ============================================================ GIF (primeiro quadro) */
static surface_t *decode_gif(const u8 *d, usize n) {
    if (n < 13 || memcmp(d, "GIF8", 4)) return NULL;
    int W = d[6] | d[7] << 8, H = d[8] | d[9] << 8;
    if (!W || !H || (u64)W * H > MAX_PIXELS) return NULL;
    u8 flags = d[10];
    usize pos = 13;
    u32 gct[256], lct[256];
    int gsize = 0;
    if (flags & 0x80) {
        gsize = 1 << ((flags & 7) + 1);
        if (pos + (usize)gsize * 3 > n) return NULL;
        for (int i = 0; i < gsize; i++) gct[i] = RGB(d[pos + i * 3], d[pos + i * 3 + 1], d[pos + i * 3 + 2]);
        pos += (usize)gsize * 3;
    }
    int transp = -1;
    while (pos < n) {
        u8 b = d[pos++];
        if (b == 0x21) {
            if (pos >= n) break;
            u8 label = d[pos++];
            if (label == 0xF9 && pos + 5 < n && d[pos] >= 4) { if (d[pos + 1] & 1) transp = d[pos + 4]; }
            while (pos < n && d[pos]) pos += d[pos] + 1;
            pos++;
            continue;
        }
        if (b != 0x2C) break;
        if (pos + 9 > n) break;
        int ix = d[pos] | d[pos + 1] << 8, iy = d[pos + 2] | d[pos + 3] << 8, iw = d[pos + 4] | d[pos + 5] << 8, ih = d[pos + 6] | d[pos + 7] << 8;
        u8 f2 = d[pos + 8];
        pos += 9;
        u32 *pal = gct;
        int psize = gsize;
        if (f2 & 0x80) {
            psize = 1 << ((f2 & 7) + 1);
            if (pos + (usize)psize * 3 > n) return NULL;
            for (int i = 0; i < psize; i++) lct[i] = RGB(d[pos + i * 3], d[pos + i * 3 + 1], d[pos + i * 3 + 2]);
            pos += (usize)psize * 3;
            pal = lct;
        }
        bool inter = f2 & 0x40;
        if (pos >= n) break;
        int mcs = d[pos++];
        if (mcs < 2 || mcs > 8) return NULL;
        /* junta os sub-blocos */
        usize dl = 0;
        for (usize q = pos; q < n && d[q]; q += d[q] + 1) dl += d[q];
        u8 *data = kmalloc(dl + 4);
        if (!data) return NULL;
        usize k = 0;
        while (pos < n && d[pos]) { usize l = d[pos]; if (pos + 1 + l > n) l = n - pos - 1; memcpy(data + k, d + pos + 1, l); k += l; pos += 1 + l; }
        u8 *idx = kcalloc((usize)iw * ih + 1, 1);
        u16 *prefix = kmalloc(4096 * 2);
        u8 *suffix = kmalloc(4096), *stack = kmalloc(4097);
        if (!idx || !prefix || !suffix || !stack) { kfree(data); kfree(idx); kfree(prefix); kfree(suffix); kfree(stack); return NULL; }
        int clear = 1 << mcs, eoi = clear + 1, next = clear + 2, csize = mcs + 1;
        int old = -1, first = 0;
        usize bitpos = 0, outp = 0, total = (usize)iw * ih;
        for (int i = 0; i < clear; i++) { prefix[i] = 0xFFFF; suffix[i] = (u8)i; }
        while (outp < total) {
            if ((bitpos + (usize)csize) > k * 8) break;
            int code = 0;
            for (int i = 0; i < csize; i++, bitpos++) code |= ((data[bitpos >> 3] >> (bitpos & 7)) & 1) << i;
            if (code == clear) { csize = mcs + 1; next = clear + 2; old = -1; continue; }
            if (code == eoi) break;
            int sp = 0, c = code;
            if (old < 0) {
                if (code >= clear) break;
                idx[outp++] = (u8)code;
                old = code; first = code;
                continue;
            }
            if (code >= next) { stack[sp++] = (u8)first; c = old; }
            while (c >= clear && sp < 4096) { stack[sp++] = suffix[c]; c = prefix[c]; }
            stack[sp++] = (u8)c;
            first = c;
            while (sp && outp < total) idx[outp++] = stack[--sp];
            if (next < 4096) { prefix[next] = (u16)old; suffix[next] = (u8)first; next++; if (next == (1 << csize) && csize < 12) csize++; }
            old = code;
        }
        surface_t *s = surface_new(W, H);
        if (s) {
            memset(s->px, 0, (usize)W * H * 4);
            for (int r = 0; r < ih; r++) {
                int row = r;
                if (inter) {
                    /* passes 0,8 / 4,8 / 2,4 / 1,2 */
                    int p1 = (ih + 7) / 8, p2 = (ih + 3) / 8, p3 = (ih + 1) / 4;
                    if (r < p1) row = r * 8;
                    else if (r < p1 + p2) row = (r - p1) * 8 + 4;
                    else if (r < p1 + p2 + p3) row = (r - p1 - p2) * 4 + 2;
                    else row = (r - p1 - p2 - p3) * 2 + 1;
                }
                int yy = iy + row;
                if (yy < 0 || yy >= H) continue;
                for (int x = 0; x < iw; x++) {
                    int xx = ix + x;
                    if (xx >= W) break;
                    int v = idx[(usize)r * iw + x];
                    if (v == transp || v >= psize) continue;
                    s->px[(usize)yy * W + xx] = pal[v];
                }
            }
        }
        kfree(data); kfree(idx); kfree(prefix); kfree(suffix); kfree(stack);
        return s;
    }
    return NULL;
}

/* ============================================================ BMP / PPM (como no Visualizador) */
static surface_t *decode_bmp(const u8 *d, usize n) {
    if (n < 54 || d[0] != 'B' || d[1] != 'M') return NULL;
    u32 off = *(const u32 *)(d + 10);
    i32 w = *(const i32 *)(d + 18), h = *(const i32 *)(d + 22);
    u16 bpp = *(const u16 *)(d + 28);
    if (w <= 0 || w > 8192 || h == 0 || (bpp != 24 && bpp != 32)) return NULL;
    bool flip = h > 0;
    if (h < 0) h = -h;
    if (h > 8192) return NULL;
    surface_t *s = surface_new(w, h);
    if (!s) return NULL;
    int rs = ((w * bpp / 8) + 3) & ~3;
    for (int y = 0; y < h; y++) {
        const u8 *row = d + off + (usize)(flip ? h - 1 - y : y) * rs;
        if (row + w * bpp / 8 > d + n) break;
        for (int x = 0; x < w; x++) {
            const u8 *p = row + x * (bpp / 8);
            s->px[y * w + x] = RGB(p[2], p[1], p[0]);
        }
    }
    return s;
}

static surface_t *decode_ppm(const u8 *d, usize n) {
    if (n < 8 || d[0] != 'P' || d[1] != '6') return NULL;
    const char *p = (const char *)d + 2;
    int vals[3] = {0, 0, 0}, k = 0;
    while (k < 3 && p < (const char *)d + n) {
        while (p < (const char *)d + n && isspace(*p)) p++;
        if (*p == '#') { while (p < (const char *)d + n && *p != '\n') p++; continue; }
        vals[k++] = (int)strtol(p, (char **)&p, 10);
    }
    p++;
    int w = vals[0], h = vals[1];
    if (w <= 0 || h <= 0 || w > 8192 || h > 8192 || (usize)((const u8 *)p - d) + (usize)w * h * 3 > n) return NULL;
    surface_t *s = surface_new(w, h);
    if (!s) return NULL;
    const u8 *q = (const u8 *)p;
    for (int i = 0; i < w * h; i++, q += 3) s->px[i] = RGB(q[0], q[1], q[2]);
    return s;
}

surface_t *image_decode(const u8 *d, usize n) {
    if (!d || n < 4) return NULL;
    if (d[0] == 0x89 && d[1] == 'P') return decode_png(d, n);
    if (d[0] == 0xFF && d[1] == 0xD8) return decode_jpeg(d, n);
    if (d[0] == 'G' && d[1] == 'I' && d[2] == 'F') return decode_gif(d, n);
    if (d[0] == 'B' && d[1] == 'M') return decode_bmp(d, n);
    if (d[0] == 'P' && d[1] == '6') return decode_ppm(d, n);
    return NULL;
}

/* redução por média de área (pixels pré-multiplicados) */
surface_t *image_scale(const surface_t *src, int w, int h) {
    if (w <= 0 || h <= 0 || w > 8192 || h > 8192) return NULL;
    surface_t *d = surface_new(w, h);
    if (!d) return NULL;
    for (int y = 0; y < h; y++) {
        int y0 = (int)((i64)y * src->h / h), y1 = (int)((i64)(y + 1) * src->h / h);
        if (y1 <= y0) y1 = y0 + 1;
        for (int x = 0; x < w; x++) {
            int x0 = (int)((i64)x * src->w / w), x1 = (int)((i64)(x + 1) * src->w / w);
            if (x1 <= x0) x1 = x0 + 1;
            u32 sa = 0, sr = 0, sg = 0, sb = 0, cnt = 0;
            for (int yy = y0; yy < y1 && yy < src->h; yy++) {
                const u32 *row = src->px + (usize)yy * src->stride;
                for (int xx = x0; xx < x1 && xx < src->w; xx++) {
                    u32 p = row[xx];
                    sa += p >> 24; sr += (p >> 16) & 255; sg += (p >> 8) & 255; sb += p & 255;
                    cnt++;
                }
            }
            if (!cnt) cnt = 1;
            d->px[(usize)y * d->stride + x] = ((sa / cnt) << 24) | ((sr / cnt) << 16) | ((sg / cnt) << 8) | (sb / cnt);
        }
    }
    return d;
}
