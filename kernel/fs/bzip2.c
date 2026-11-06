/* mOS — descompressor bzip2 (blocos UDBZ de imagens DMG) */
#include "fs.h"

#define BZ_MAX_GROUPS 6
#define BZ_MAX_ALPHA  258
#define BZ_MAX_SEL    18002
#define BZ_MAX_LEN    20

typedef struct {
    const u8 *src, *end;
    u64 bits;
    u32 nbits;
    bool err;
} bzin_t;

static u32 bz_bits(bzin_t *s, u32 n) {
    while (s->nbits < n) {
        if (s->src >= s->end) { s->err = true; return 0; }
        s->bits = (s->bits << 8) | *s->src++;
        s->nbits += 8;
    }
    s->nbits -= n;
    return (u32)(s->bits >> s->nbits) & ((1u << n) - 1);
}

typedef struct {
    i32 limit[BZ_MAX_LEN + 2], base[BZ_MAX_LEN + 2];
    u16 perm[BZ_MAX_ALPHA];
    u8 minlen, maxlen;
} bzhuff_t;

static void bz_build(bzhuff_t *h, const u8 *len, int n) {
    int minl = 32, maxl = 0;
    for (int i = 0; i < n; i++) { if (len[i] > maxl) maxl = len[i]; if (len[i] < minl) minl = len[i]; }
    h->minlen = (u8)minl; h->maxlen = (u8)maxl;
    int pp = 0;
    for (int l = minl; l <= maxl; l++)
        for (int i = 0; i < n; i++)
            if (len[i] == l) h->perm[pp++] = (u16)i;
    int cnt[BZ_MAX_LEN + 2];
    memset(cnt, 0, sizeof(cnt));
    for (int i = 0; i < n; i++) cnt[len[i]]++;
    /* código canônico: limit[l] = maior código de comprimento l; base[l] = índice em perm - primeiro código */
    int code = 0, idx = 0;
    for (int l = 0; l <= BZ_MAX_LEN + 1; l++) { h->limit[l] = -1; h->base[l] = 0; }
    for (int l = minl; l <= maxl; l++) {
        h->base[l] = idx - code;
        code += cnt[l];
        idx += cnt[l];
        h->limit[l] = code - 1;
        code <<= 1;
    }
}

static int bz_sym(bzin_t *s, const bzhuff_t *h) {
    int l = h->minlen;
    i32 v = (i32)bz_bits(s, l);
    for (;;) {
        if (s->err || l > h->maxlen) return -1;
        if (v <= h->limit[l]) {
            int i = v + h->base[l];
            return (i >= 0 && i < BZ_MAX_ALPHA) ? h->perm[i] : -1;
        }
        v = (v << 1) | (i32)bz_bits(s, 1);
        l++;
    }
}

typedef struct {
    u8 *dst;
    u64 pos, cap;
    bool err;
} bzout_t;

static inline void bz_put(bzout_t *o, u8 c) {
    if (o->pos >= o->cap) { o->err = true; return; }
    o->dst[o->pos++] = c;
}

/* decodifica um bloco; retorna 1 = bloco ok, 0 = fim do stream, -1 = erro */
static int bz_block(bzin_t *s, u32 *tt, u32 maxblock, bzout_t *out, u8 *sel, bzhuff_t *hufs) {
    u32 m1 = bz_bits(s, 24), m2 = bz_bits(s, 24);
    if (s->err) return -1;
    if (m1 == 0x177245 && m2 == 0x385090) { bz_bits(s, 32); return 0; }
    if (m1 != 0x314159 || m2 != 0x265359) return -1;
    bz_bits(s, 32); /* CRC do bloco */
    if (bz_bits(s, 1)) return -1; /* blocos "randomizados" (bzip2 0.9) não são suportados */
    u32 origptr = bz_bits(s, 24);

    u8 seq[256];
    int ninuse = 0;
    u32 used = bz_bits(s, 16);
    for (int i = 0; i < 16; i++)
        if (used & (0x8000 >> i)) {
            u32 m = bz_bits(s, 16);
            for (int j = 0; j < 16; j++)
                if (m & (0x8000 >> j)) seq[ninuse++] = (u8)(i * 16 + j);
        }
    if (!ninuse || s->err) return -1;
    int alpha = ninuse + 2;
    int ngroups = (int)bz_bits(s, 3);
    u32 nsel = bz_bits(s, 15);
    if (ngroups < 2 || ngroups > BZ_MAX_GROUPS || nsel < 1) return -1;

    u8 mtfg[BZ_MAX_GROUPS];
    for (int i = 0; i < ngroups; i++) mtfg[i] = (u8)i;
    u32 nstored = 0;
    for (u32 i = 0; i < nsel; i++) {
        int j = 0;
        while (bz_bits(s, 1)) { if (++j >= ngroups) return -1; }
        if (s->err) return -1;
        u8 v = mtfg[j];
        for (; j > 0; j--) mtfg[j] = mtfg[j - 1];
        mtfg[0] = v;
        if (nstored < BZ_MAX_SEL) sel[nstored++] = v;
    }
    nsel = nstored;

    u8 lens[BZ_MAX_ALPHA];
    for (int g = 0; g < ngroups; g++) {
        int cur = (int)bz_bits(s, 5);
        for (int i = 0; i < alpha; i++) {
            for (;;) {
                if (cur < 1 || cur > BZ_MAX_LEN || s->err) return -1;
                if (!bz_bits(s, 1)) break;
                cur += bz_bits(s, 1) ? -1 : 1;
            }
            lens[i] = (u8)cur;
        }
        bz_build(&hufs[g], lens, alpha);
    }

    u8 mtf[256];
    for (int i = 0; i < 256; i++) mtf[i] = (u8)i;
    u32 cnt[256];
    memset(cnt, 0, sizeof(cnt));
    u32 n = 0, gsel = 0;
    int gleft = 0;
    const bzhuff_t *h = NULL;
    u32 run = 0, runw = 1;
    for (;;) {
        if (!gleft) {
            if (gsel >= nsel) return -1;
            h = &hufs[sel[gsel++]];
            gleft = 50;
        }
        gleft--;
        int sym = bz_sym(s, h);
        if (sym < 0) return -1;
        if (sym <= 1) { /* RUNA / RUNB */
            run += (sym + 1) * runw;
            runw <<= 1;
            if (run > maxblock) return -1;
            continue;
        }
        if (run) {
            if (n + run > maxblock) return -1;
            u8 c = seq[mtf[0]];
            cnt[c] += run;
            while (run--) tt[n++] = c;
            run = 0; runw = 1;
        }
        if (sym == alpha - 1) break; /* EOB */
        if (n >= maxblock) return -1;
        int k = sym - 1;
        u8 v = mtf[k];
        memmove(mtf + 1, mtf, k);
        mtf[0] = v;
        u8 c = seq[v];
        cnt[c]++;
        tt[n++] = c;
    }
    if (origptr >= n) return -1;

    /* BWT inversa */
    u32 sum = 0;
    for (int i = 0; i < 256; i++) { u32 c = cnt[i]; cnt[i] = sum; sum += c; }
    for (u32 i = 0; i < n; i++) {
        u8 c = (u8)tt[i];
        tt[cnt[c]++] |= i << 8;
    }
    u32 pos = tt[origptr] >> 8;
    /* desfaz o RLE inicial: 4 bytes iguais + contador */
    int last = -1, rep = 0;
    for (u32 i = 0; i < n; i++) {
        u32 e = tt[pos];
        u8 c = (u8)e;
        pos = e >> 8;
        if (rep == 4) {
            for (u32 k = 0; k < c; k++) bz_put(out, (u8)last);
            rep = 0;
            last = -1;
            continue;
        }
        if (c == last) rep++;
        else { rep = 1; last = c; }
        bz_put(out, c);
    }
    return out->err ? -1 : 1;
}

i64 bzip2_decompress(const u8 *src, u64 slen, u8 *dst, u64 dlen) {
    bzin_t s = {src, src + slen, 0, 0, false};
    bzout_t out = {dst, 0, dlen, false};
    u32 *tt = NULL;
    u8 *sel = kmalloc(BZ_MAX_SEL);
    bzhuff_t *hufs = kmalloc(sizeof(bzhuff_t) * BZ_MAX_GROUPS);
    u32 ttsize = 0;
    i64 ret = -1;
    if (!sel || !hufs) goto done;
    /* um ou mais streams concatenados */
    while (s.end - s.src >= 4 || s.nbits >= 32) {
        s.nbits -= s.nbits % 8; /* streams começam alinhados a byte */
        if (bz_bits(&s, 8) != 'B' || bz_bits(&s, 8) != 'Z' || bz_bits(&s, 8) != 'h') break;
        u32 lvl = bz_bits(&s, 8);
        if (lvl < '1' || lvl > '9') goto done;
        u32 maxblock = (lvl - '0') * 100000;
        if (maxblock > ttsize) {
            kfree(tt);
            tt = kmalloc((usize)maxblock * 4);
            if (!tt) goto done;
            ttsize = maxblock;
        }
        int r;
        while ((r = bz_block(&s, tt, maxblock, &out, sel, hufs)) == 1) {}
        if (r < 0) goto done;
        if (out.pos == out.cap) break;
    }
    ret = (i64)out.pos;
done:
    kfree(tt);
    kfree(sel);
    kfree(hufs);
    return ret;
}
