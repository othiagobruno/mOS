/*
 * mOS libSystem — libbz2 (leitura): BZ2_bzReadOpen/BZ2_bzRead/BZ2_bzReadClose e BZ2_bzBuffToBuffDecompress.
 * Descompressor bzip2 escrito do zero a partir do formato: blocos "1AY&SY" com CRC, mapa de símbolos, seletores com
 * MTF, tabelas de Huffman canônicas por grupo de 50 símbolos, RUNA/RUNB, MTF dos bytes, BWT inversa e o RLE final
 * (4 iguais + contagem). Verifica o CRC de cada bloco e o combinado. Blocos "randomizados" (bzip2 0.9.0) não são aceitos.
 */
#include "sys.h"
typedef void FILE_;
size_t fread(void *p, size_t s, size_t n, FILE_ *f);

void *malloc(size_t n);
void *calloc(size_t n, size_t m);
void *realloc(void *p, size_t n);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
int memcmp(const void *a, const void *b, size_t n);
int memcmp(const void *a, const void *b, size_t n);

enum { BZ_OK = 0, BZ_STREAM_END = 4, BZ_PARAM_ERROR = -2, BZ_MEM_ERROR = -3, BZ_DATA_ERROR = -4, BZ_DATA_ERROR_MAGIC = -5, BZ_IO_ERROR = -6,
       BZ_UNEXPECTED_EOF = -7, BZ_OUTBUFF_FULL = -8 };

static u32 crc_tab[256];
static void crc_init(void) {
    if (crc_tab[1]) return;
    for (u32 i = 0; i < 256; i++) { u32 c = i << 24; for (int k = 0; k < 8; k++) c = c & 0x80000000u ? (c << 1) ^ 0x04C11DB7u : c << 1; crc_tab[i] = c; }
}

typedef struct { const u8 *d; size_t n, pos; int bit; } br_t;   /* pos em bits */
static int rb(br_t *b, int k) {
    u32 v = 0;
    for (int i = 0; i < k; i++) {
        if (b->pos >= b->n * 8) { b->bit = -1; return -1; }
        v = v << 1 | ((b->d[b->pos >> 3] >> (7 - (b->pos & 7))) & 1);
        b->pos++;
    }
    return (int)v;
}

/* decodifica um bloco; devolve bytes em *out (malloc) ou erro < 0; 1 = fim do fluxo */
static int bz_block(br_t *b, int maxblock, u8 **out, size_t *outn, u32 *combined, int *got_crc_ok) {
    u32 m1 = (u32)rb(b, 24), m2 = (u32)rb(b, 24);
    if (b->bit < 0) return BZ_UNEXPECTED_EOF;
    if (m1 == 0x177245 && m2 == 0x385090) {
        u32 c = (u32)rb(b, 16) << 16; c |= (u32)rb(b, 16);
        *got_crc_ok = c == *combined;
        return 1;
    }
    if (m1 != 0x314159 || m2 != 0x265359) return BZ_DATA_ERROR;
    u32 bcrc = (u32)rb(b, 16) << 16; bcrc |= (u32)rb(b, 16);
    if (rb(b, 1)) return BZ_DATA_ERROR;                               /* randomizado */
    int orig = rb(b, 24);
    int used = rb(b, 16), nin = 0;
    u8 seq[256];
    for (int i = 0; i < 16; i++) if (used & (0x8000 >> i)) { int m = rb(b, 16); for (int j = 0; j < 16; j++) if (m & (0x8000 >> j)) seq[nin++] = (u8)(i * 16 + j); }
    if (b->bit < 0 || !nin) return BZ_DATA_ERROR;
    int alpha = nin + 2, ngroups = rb(b, 3), nsel = rb(b, 15);
    if (ngroups < 2 || ngroups > 6 || nsel < 1) return BZ_DATA_ERROR;
    u8 *sel = malloc((size_t)nsel), mtfg[6] = {0, 1, 2, 3, 4, 5};
    for (int i = 0; i < nsel; i++) {
        int j = 0;
        while (rb(b, 1) == 1) { j++; if (j >= ngroups) { free(sel); return BZ_DATA_ERROR; } }
        u8 v = mtfg[j];
        for (; j > 0; j--) mtfg[j] = mtfg[j - 1];
        mtfg[0] = v;
        sel[i] = v;
    }
    u8 len[6][258];
    int minl[6], maxl[6], perm[6][258], cnt[6][21], firstc[6][21], firsti[6][21];
    for (int g = 0; g < ngroups; g++) {
        int c = rb(b, 5);
        for (int s = 0; s < alpha; s++) {
            for (;;) {
                if (c < 1 || c > 20 || b->bit < 0) { free(sel); return BZ_DATA_ERROR; }
                if (!rb(b, 1)) break;
                c += rb(b, 1) ? -1 : 1;
            }
            len[g][s] = (u8)c;
        }
        minl[g] = 32; maxl[g] = 0;
        memset(cnt[g], 0, sizeof cnt[g]);
        for (int s = 0; s < alpha; s++) { if (len[g][s] < minl[g]) minl[g] = len[g][s]; if (len[g][s] > maxl[g]) maxl[g] = len[g][s]; cnt[g][len[g][s]]++; }
        int pp = 0, code = 0;
        for (int l = 1; l <= 20; l++) {
            firstc[g][l] = code; firsti[g][l] = pp;
            for (int s = 0; s < alpha; s++) if (len[g][s] == l) perm[g][pp++] = s;
            code = (code + cnt[g][l]) << 1;
        }
    }
    /* símbolos → contagens do MTF */
    u32 *tt = malloc(sizeof(u32) * (size_t)maxblock);
    u8 mtf[256];
    for (int i = 0; i < 256; i++) mtf[i] = (u8)i;
    int cnt_byte[256] = {0}, n = 0, gidx = 0, gleft = 0, g = 0, run = 0, runbit = 1;
    for (;;) {
        if (!gleft) { if (gidx >= nsel) { free(sel); free(tt); return BZ_DATA_ERROR; } g = sel[gidx++]; gleft = 50; }
        gleft--;
        int code = 0, sym = -1;
        for (int l = 1; l <= maxl[g]; l++) {
            int bit = rb(b, 1);
            if (bit < 0) { free(sel); free(tt); return BZ_UNEXPECTED_EOF; }
            code = code << 1 | bit;
            if (l >= minl[g] && cnt[g][l] && code - firstc[g][l] < cnt[g][l] && code >= firstc[g][l]) { sym = perm[g][firsti[g][l] + code - firstc[g][l]]; break; }
        }
        if (sym < 0) { free(sel); free(tt); return BZ_DATA_ERROR; }
        if (sym <= 1) { run += runbit << sym; runbit <<= 1; if (run > maxblock) { free(sel); free(tt); return BZ_DATA_ERROR; } continue; }
        if (run) {
            u8 v = seq[mtf[0]];
            if (n + run > maxblock) { free(sel); free(tt); return BZ_DATA_ERROR; }
            cnt_byte[v] += run;
            while (run--) tt[n++] = v;
            run = 0; runbit = 1;
        }
        if (sym == alpha - 1) break;                                  /* EOB */
        int k = sym - 1;
        u8 idx = mtf[k];
        for (; k > 0; k--) mtf[k] = mtf[k - 1];
        mtf[0] = idx;
        if (n >= maxblock) { free(sel); free(tt); return BZ_DATA_ERROR; }
        u8 v = seq[idx];
        cnt_byte[v]++;
        tt[n++] = v;
    }
    free(sel);
    if (orig < 0 || orig >= n) { free(tt); return BZ_DATA_ERROR; }
    /* BWT inversa: tt[i] guarda o byte no fim; os 24 bits de cima o próximo índice */
    int cf[256], sum = 0;
    for (int i = 0; i < 256; i++) { cf[i] = sum; sum += cnt_byte[i]; }
    for (int i = 0; i < n; i++) { u8 v = (u8)(tt[i] & 0xFF); tt[cf[v]++] |= (u32)i << 8; }
    size_t cap = (size_t)n + 1024, on = 0;
    u8 *o = malloc(cap);
    u32 pos = tt[orig] >> 8, crc = 0xFFFFFFFFu;
    int last = -1, same = 0;
    crc_init();
    for (int i = 0; i < n; i++) {
        u32 t = tt[pos];
        u8 v = (u8)(t & 0xFF);
        pos = t >> 8;
        if (same == 4) {                                              /* RLE: 4 iguais + contagem */
            for (int r = 0; r < v; r++) {
                if (on >= cap) { cap *= 2; o = realloc(o, cap); }
                o[on++] = (u8)last;
                crc = (crc << 8) ^ crc_tab[(crc >> 24) ^ (u8)last];
            }
            same = 0; last = -1;
            continue;
        }
        if (v == last) same++; else { same = 1; last = v; }
        if (on >= cap) { cap *= 2; o = realloc(o, cap); }
        o[on++] = v;
        crc = (crc << 8) ^ crc_tab[(crc >> 24) ^ v];
    }
    free(tt);
    crc = ~crc;
    if (crc != bcrc) { free(o); return BZ_DATA_ERROR; }
    *combined = ((*combined << 1) | (*combined >> 31)) ^ crc;
    *out = o; *outn = on;
    return 0;
}

/* ---- a API de arquivo (BZFILE de leitura) */
typedef struct { u8 *in; size_t inn; br_t br; int maxblock; u32 combined; u8 *blk; size_t bn, bpos; int done, err; } bzf_t;
void *BZ2_bzReadOpen(int *bzerror, FILE_ *f, int verbosity, int small, void *unused, int nunused) {
    (void)verbosity; (void)small;
    if (!f) { if (bzerror) *bzerror = BZ_PARAM_ERROR; return 0; }
    bzf_t *z = calloc(1, sizeof *z);
    size_t cap = 65536;
    z->in = malloc(cap);
    if (unused && nunused > 0) { memcpy(z->in, unused, (size_t)nunused); z->inn = (size_t)nunused; }
    for (;;) {
        if (z->inn + 65536 > cap) { cap *= 2; z->in = realloc(z->in, cap); }
        size_t k = fread(z->in + z->inn, 1, 65536, f);
        if (!k) break;
        z->inn += k;
    }
    z->br.d = z->in; z->br.n = z->inn;
    if (z->inn < 4 || z->in[0] != 'B' || z->in[1] != 'Z' || z->in[2] != 'h' || z->in[3] < '1' || z->in[3] > '9') {
        free(z->in); free(z);
        if (bzerror) *bzerror = BZ_DATA_ERROR_MAGIC;
        return 0;
    }
    z->maxblock = (z->in[3] - '0') * 100000;
    z->br.pos = 32;
    if (bzerror) *bzerror = BZ_OK;
    return z;
}
int BZ2_bzRead(int *bzerror, void *h, void *buf, int len) {
    bzf_t *z = h;
    if (!z || !buf || len < 0) { if (bzerror) *bzerror = BZ_PARAM_ERROR; return 0; }
    int got = 0;
    while (got < len) {
        if (z->bpos < z->bn) {
            size_t k = z->bn - z->bpos < (size_t)(len - got) ? z->bn - z->bpos : (size_t)(len - got);
            memcpy((u8 *)buf + got, z->blk + z->bpos, k);
            z->bpos += k; got += (int)k;
            continue;
        }
        if (z->done) break;
        free(z->blk); z->blk = 0; z->bn = z->bpos = 0;
        int ok = 0, r = bz_block(&z->br, z->maxblock, &z->blk, &z->bn, &z->combined, &ok);
        if (r == 1) {
            z->done = 1;
            if (!ok) { z->err = BZ_DATA_ERROR; break; }
            /* fluxos concatenados (pbzip2): outro "BZh" alinhado em byte */
            size_t p = (z->br.pos + 7) / 8;
            if (p + 4 <= z->inn && z->in[p] == 'B' && z->in[p + 1] == 'Z' && z->in[p + 2] == 'h') {
                z->br.pos = (p + 4) * 8; z->combined = 0; z->done = 0;
                z->maxblock = (z->in[p + 3] - '0') * 100000;
            }
            continue;
        }
        if (r < 0) { z->err = r; break; }
    }
    if (bzerror) *bzerror = z->err ? z->err : (z->done && z->bpos >= z->bn && got < len) ? BZ_STREAM_END : got == len && z->done && z->bpos >= z->bn ? BZ_STREAM_END : BZ_OK;
    return z->err && !got ? 0 : got;
}
void BZ2_bzReadClose(int *bzerror, void *h) {
    bzf_t *z = h;
    if (z) { free(z->blk); free(z->in); free(z); }
    if (bzerror) *bzerror = BZ_OK;
}
void BZ2_bzReadGetUnused(int *bzerror, void *h, void **unused, int *n) {
    (void)h;
    if (unused) *unused = 0;
    if (n) *n = 0;
    if (bzerror) *bzerror = BZ_OK;
}
int BZ2_bzBuffToBuffDecompress(char *dest, unsigned *destlen, char *src, unsigned srclen, int small, int verbosity) {
    (void)small; (void)verbosity;
    if (!dest || !destlen || !src || srclen < 4 || src[0] != 'B' || src[1] != 'Z' || src[2] != 'h') return BZ_DATA_ERROR_MAGIC;
    br_t b = {(const u8 *)src, srclen, 32, 0};
    int maxblock = (src[3] - '0') * 100000;
    u32 comb = 0, out = 0;
    for (;;) {
        u8 *blk = 0;
        size_t bn = 0;
        int ok = 0, r = bz_block(&b, maxblock, &blk, &bn, &comb, &ok);
        if (r == 1) { *destlen = out; return ok ? BZ_OK : BZ_DATA_ERROR; }
        if (r < 0) return r;
        if (out + bn > *destlen) { free(blk); return BZ_OUTBUFF_FULL; }
        memcpy(dest + out, blk, bn);
        out += (u32)bn;
        free(blk);
    }
}
const char *BZ2_bzlibVersion(void) { return "1.0.8, 13-Jul-2019 (mOS)"; }

/* autoteste: "hello world\n" comprimido pelo bzip2 1.0.8 com -9 */
int cx_bzip2_selftest(void) {
    static const u8 bz[] = {
        0x42, 0x5a, 0x68, 0x39, 0x31, 0x41, 0x59, 0x26, 0x53, 0x59, 0x4e, 0xec, 0xe8, 0x36, 0x00, 0x00, 0x02, 0x51, 0x80, 0x00,
        0x10, 0x40, 0x00, 0x06, 0x44, 0x90, 0x80, 0x20, 0x00, 0x31, 0x06, 0x4c, 0x41, 0x01, 0xa7, 0xa9, 0xa5, 0x80, 0xbb, 0x94,
        0x31, 0xf8, 0xbb, 0x92, 0x29, 0xc2, 0x84, 0x82, 0x77, 0x67, 0x41, 0xb0,
    };
    char out[64];
    unsigned n = sizeof out;
    int r = BZ2_bzBuffToBuffDecompress(out, &n, (char *)bz, sizeof bz, 0, 0);
    return r == BZ_OK && n == 12 && !memcmp(out, "hello world\n", 12) ? 0 : (r ? r : 99);
}
