/*
 * mOS — descompressores LZFSE e LZVN (formatos abertos da Apple, usados por ULFO, APFS e
 * decmpfs). Implementação própria e compacta seguindo a especificação do lzfse de referência.
 */
#include "fs.h"

/* ================================================================ LZVN */
i64 lzvn_decompress(const u8 *src, u64 slen, u8 *dst, u64 dlen) {
    u64 i = 0, o = 0, d = 0;
    while (i < slen) {
        u8 op = src[i];
        u64 L = 0, M = 0, oplen;
        bool match = true;
        switch (op >> 4) {
        case 0xE: /* literais */
            match = false;
            if (op == 0xE0) { if (i + 2 > slen) return -1; L = src[i + 1] + 16u; oplen = 2; }
            else { L = op & 0x0F; oplen = 1; }
            break;
        case 0xF: /* match com distância anterior */
            if (op == 0xF0) { if (i + 2 > slen) return -1; M = src[i + 1] + 16u; oplen = 2; }
            else { M = op & 0x0F; oplen = 1; }
            break;
        case 0x7: case 0xD:
            return -1;
        case 0xA: case 0xB: /* distância média: 101LLMMM DDDDDDMM DDDDDDDD */
            if (i + 3 > slen) return -1;
            {
                u16 w = le16(src + i + 1);
                L = (op >> 3) & 3;
                M = (((u64)(op & 7) << 2) | (w & 3)) + 3;
                d = w >> 2;
                oplen = 3;
            }
            break;
        default:
            L = op >> 6;
            M = ((op >> 3) & 7) + 3u;
            switch (op & 7) {
            case 6:
                if (L == 0) {
                    if (op == 0x06) return (i64)o;            /* fim do stream */
                    if (op == 0x0E || op == 0x16) { i++; continue; } /* nop */
                    return -1;
                }
                oplen = 1; /* distância anterior */
                break;
            case 7:
                if (i + 3 > slen) return -1;
                d = le16(src + i + 1);
                oplen = 3;
                break;
            default:
                if (i + 2 > slen) return -1;
                d = ((u64)(op & 7) << 8) | src[i + 1];
                oplen = 2;
                break;
            }
            break;
        }
        i += oplen;
        if (L) {
            if (i + L > slen || o + L > dlen) return -1;
            memcpy(dst + o, src + i, L);
            i += L; o += L;
        }
        if (match && M) {
            if (d == 0 || d > o) return -1;
            if (o + M > dlen) { M = dlen - o; }
            for (u64 k = 0; k < M; k++, o++) dst[o] = dst[o - d];
            if (o == dlen) return (i64)o;
        }
    }
    return (i64)o;
}

/* ================================================================ LZFSE */
#define L_SYMS 20
#define M_SYMS 20
#define D_SYMS 64
#define LIT_SYMS 256
#define L_STATES 64
#define M_STATES 64
#define D_STATES 256
#define LIT_STATES 1024
#define MAX_LITERALS 40000

static const u8  L_XBITS[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 3, 5, 8};
static const i32 L_BASE[20]  = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 20, 28, 60};
static const u8  M_XBITS[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 5, 8, 11};
static const i32 M_BASE[20]  = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 24, 56, 312};
static const u8  D_XBITS[64] = {
    0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7,
    8, 8, 8, 8, 9, 9, 9, 9, 10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 13, 13, 13, 13, 14, 14, 14, 14, 15, 15, 15, 15};
static const i32 D_BASE[64] = {
    0, 1, 2, 3, 4, 6, 8, 10, 12, 16, 20, 24, 28, 36, 44, 52, 60, 76, 92, 108, 124, 156, 188, 220,
    252, 316, 380, 444, 508, 636, 764, 892, 1020, 1276, 1532, 1788, 2044, 2556, 3068, 3580, 4092,
    5116, 6140, 7164, 8188, 10236, 12284, 14332, 16380, 20476, 24572, 28668, 32764, 40956, 49148,
    57340, 65532, 81916, 98300, 114684, 131068, 163836, 196604, 229372};

typedef struct { i8 k; u8 sym; i16 delta; } fse_ent_t;
typedef struct { u8 total, vbits; i16 delta; i32 vbase; } fse_vent_t;

typedef struct {
    u64 accum;
    i32 nbits;
    const u8 *p, *start;
    bool err;
} fse_in_t;

static inline u64 mask64(u64 x, i32 n) { return n <= 0 ? 0 : n >= 64 ? x : x & ((1ull << n) - 1); }

static void fse_init(fse_in_t *s, i32 n, const u8 *end, const u8 *start) {
    s->p = end; s->start = start; s->err = false;
    if (n) {
        if (s->p < start + 8) { s->err = true; return; }
        s->p -= 8;
        memcpy(&s->accum, s->p, 8);
        s->nbits = n + 64;
    } else {
        if (s->p < start + 7) { s->err = true; return; }
        s->p -= 7;
        s->accum = 0;
        memcpy(&s->accum, s->p, 7);
        s->nbits = 56;
    }
    if (s->nbits < 56 || s->nbits >= 64 || (s->accum >> s->nbits)) s->err = true;
}

static inline void fse_flush(fse_in_t *s) {
    i32 nb = (63 - s->nbits) & -8;
    if (nb <= 0) return;
    const u8 *b = s->p - (nb >> 3);
    if (b < s->start) { s->err = true; return; }
    s->p = b;
    u64 in = 0;
    memcpy(&in, b, nb >> 3);
    s->accum = (s->accum << nb) | mask64(in, nb);
    s->nbits += nb;
}

static inline u64 fse_pull(fse_in_t *s, i32 n) {
    if (n > s->nbits) { s->err = true; return 0; }
    s->nbits -= n;
    u64 r = s->accum >> s->nbits;
    s->accum = mask64(s->accum, s->nbits);
    return r;
}

static int clz32(u32 v) { return v ? __builtin_clz(v) : 32; }

static bool fse_table(int nstates, int nsyms, const u16 *freq, fse_ent_t *t) {
    int nclz = clz32((u32)nstates), sum = 0;
    for (int i = 0; i < nsyms; i++) {
        int f = freq[i];
        if (!f) continue;
        sum += f;
        if (sum > nstates) return false;
        int k = clz32((u32)f) - nclz;
        int j0 = ((2 * nstates) >> k) - f;
        for (int j = 0; j < f; j++, t++) {
            t->sym = (u8)i;
            if (j < j0) { t->k = (i8)k; t->delta = (i16)(((f + j) << k) - nstates); }
            else { t->k = (i8)(k - 1); t->delta = (i16)((j - j0) << (k - 1)); }
        }
    }
    return true;
}

static bool fse_vtable(int nstates, int nsyms, const u16 *freq, const u8 *vbits, const i32 *vbase, fse_vent_t *t) {
    int nclz = clz32((u32)nstates), sum = 0;
    for (int i = 0; i < nsyms; i++) {
        int f = freq[i];
        if (!f) continue;
        sum += f;
        if (sum > nstates) return false;
        int k = clz32((u32)f) - nclz;
        int j0 = ((2 * nstates) >> k) - f;
        for (int j = 0; j < f; j++, t++) {
            t->vbits = vbits[i];
            t->vbase = vbase[i];
            if (j < j0) { t->total = (u8)(k + vbits[i]); t->delta = (i16)(((f + j) << k) - nstates); }
            else { t->total = (u8)(k - 1 + vbits[i]); t->delta = (i16)((j - j0) << (k - 1)); }
        }
    }
    return true;
}

static inline u8 fse_dec(u32 *st, const fse_ent_t *t, fse_in_t *in) {
    fse_ent_t e = t[*st];
    *st = (u32)(e.delta + (i32)fse_pull(in, e.k));
    return e.sym;
}

static inline i32 fse_vdec(u32 *st, const fse_vent_t *t, u32 nstates, fse_in_t *in) {
    fse_vent_t e = t[*st];
    u32 bits = (u32)fse_pull(in, e.total);
    *st = (u32)(e.delta + (i32)(bits >> e.vbits));
    if (*st >= nstates) { in->err = true; *st = 0; }
    return e.vbase + (i32)mask64(bits, e.vbits);
}

static u32 freq_value(u32 bits, int *nbits) {
    static const i8 NB[32] = {2, 3, 2, 5, 2, 3, 2, 8, 2, 3, 2, 5, 2, 3, 2, 14, 2, 3, 2, 5, 2, 3, 2, 8, 2, 3, 2, 5, 2, 3, 2, 14};
    static const i8 VAL[32] = {0, 2, 1, 4, 0, 3, 1, -1, 0, 2, 1, 5, 0, 3, 1, -1, 0, 2, 1, 6, 0, 3, 1, -1, 0, 2, 1, 7, 0, 3, 1, -1};
    u32 b = bits & 31;
    int n = NB[b];
    *nbits = n;
    if (n == 8) return 8 + ((bits >> 4) & 0x0F);
    if (n == 14) return 24 + ((bits >> 4) & 0x3FF);
    return (u32)VAL[b];
}

typedef struct {
    fse_ent_t lit[LIT_STATES];
    fse_vent_t l[L_STATES], m[M_STATES], d[D_STATES];
    u8 literals[MAX_LITERALS + 64];
    u16 freq[L_SYMS + M_SYMS + D_SYMS + LIT_SYMS];
} lzfse_ctx_t;

#ifdef LZFSE_DEBUG
#define FAIL(n) do { kprintf("lzfse: falha etapa %d\n", n); return 0; } while (0)
#else
#define FAIL(n) return 0
#endif
static inline u64 field(u64 v, int off, int n) { return (v >> off) & ((1ull << n) - 1); }

/* bloco bvx2; retorna bytes consumidos da entrada ou 0 em erro */
static u64 lzfse_block_v2(lzfse_ctx_t *c, const u8 *src, u64 slen, u8 *dst, u64 dlen, u64 *o) {
    if (slen < 32) FAIL(1);
    u32 nraw = le32(src + 4);
    u64 v0 = le64(src + 8), v1 = le64(src + 16), v2 = le64(src + 24);
    u32 nlit = (u32)field(v0, 0, 20), nlitbytes = (u32)field(v0, 20, 20), nmatch = (u32)field(v0, 40, 20);
    i32 litbits = (i32)field(v0, 60, 3) - 7;
    u32 lst[4] = {(u32)field(v1, 0, 10), (u32)field(v1, 10, 10), (u32)field(v1, 20, 10), (u32)field(v1, 30, 10)};
    u32 nlmdbytes = (u32)field(v1, 40, 20);
    i32 lmdbits = (i32)field(v1, 60, 3) - 7;
    u32 hsize = (u32)field(v2, 0, 32);
    u32 ls = (u32)field(v2, 32, 10), ms = (u32)field(v2, 42, 10), ds = (u32)field(v2, 52, 10);
    if (hsize < 32 || hsize > slen || nlit > MAX_LITERALS || (u64)hsize + nlitbytes + nlmdbytes > slen) FAIL(2);
    if (nraw > dlen - *o) FAIL(3);

    /* tabelas de frequência compactadas */
    const u8 *fp = src + 32, *fend = src + hsize;
    u32 acc = 0;
    int accn = 0;
    for (int i = 0; i < L_SYMS + M_SYMS + D_SYMS + LIT_SYMS; i++) {
        while (fp < fend && accn + 8 <= 32) { acc |= (u32)*fp++ << accn; accn += 8; }
        int nb;
        u32 f = freq_value(acc, &nb);
        if (nb > accn) FAIL(4);
        c->freq[i] = (u16)f;
        acc >>= nb;
        accn -= nb;
    }
    if (accn >= 8 || fp != fend) FAIL(5);
    const u16 *lf = c->freq, *mf = lf + L_SYMS, *df = mf + M_SYMS, *litf = df + D_SYMS;
    if (!fse_table(LIT_STATES, LIT_SYMS, litf, c->lit) ||
        !fse_vtable(L_STATES, L_SYMS, lf, L_XBITS, L_BASE, c->l) ||
        !fse_vtable(M_STATES, M_SYMS, mf, M_XBITS, M_BASE, c->m) ||
        !fse_vtable(D_STATES, D_SYMS, df, D_XBITS, D_BASE, c->d)) FAIL(6);

    /* literais: 4 estados FSE intercalados, lidos de trás para frente */
    const u8 *lbuf = src + hsize;
    fse_in_t in;
    /* o refill especulativo pode ler bytes antes do payload: o limite é o início do bloco */
    fse_init(&in, litbits, lbuf + nlitbytes, src);
    if (in.err) FAIL(7);
    for (u32 i = 0; i < 4; i++) if (lst[i] >= LIT_STATES) FAIL(8);
    for (u32 i = 0; i < nlit; i += 4) {
        fse_flush(&in);
        c->literals[i + 0] = fse_dec(&lst[0], c->lit, &in);
        c->literals[i + 1] = fse_dec(&lst[1], c->lit, &in);
        c->literals[i + 2] = fse_dec(&lst[2], c->lit, &in);
        c->literals[i + 3] = fse_dec(&lst[3], c->lit, &in);
        if (in.err) FAIL(9);
        for (int k = 0; k < 4; k++) if (lst[k] >= LIT_STATES) FAIL(10);
    }

    /* sequência L/M/D */
    const u8 *mbuf = lbuf + nlitbytes;
    fse_init(&in, lmdbits, mbuf + nlmdbytes, src);
    if (in.err || ls >= L_STATES || ms >= M_STATES || ds >= D_STATES) FAIL(11);
    const u8 *lit = c->literals, *litend = c->literals + nlit;
    u64 start = *o, end = *o + nraw;
    i64 D = -1;
    for (u32 n = 0; n < nmatch; n++) {
        fse_flush(&in);
        i32 L = fse_vdec(&ls, c->l, L_STATES, &in);
        i32 M = fse_vdec(&ms, c->m, M_STATES, &in);
        i32 nd = fse_vdec(&ds, c->d, D_STATES, &in);
        if (in.err) FAIL(12);
        if (nd) D = nd;
        if (lit + L > litend || *o + L + M > end) FAIL(13);
        memcpy(dst + *o, lit, L);
        lit += L;
        *o += L;
        if (M) {
            if (D <= 0 || (u64)D > *o) FAIL(14);
            u8 *p = dst + *o;
            for (i32 k = 0; k < M; k++) p[k] = p[k - D];
            *o += M;
        }
    }
    (void)start;
    if (*o != end) FAIL(15);
    return (u64)hsize + nlitbytes + nlmdbytes;
}

i64 lzfse_decompress(const u8 *src, u64 slen, u8 *dst, u64 dlen) {
    u64 i = 0, o = 0;
    lzfse_ctx_t *c = NULL;
    i64 ret = -1;
    while (i + 4 <= slen) {
        u32 magic = le32(src + i);
        if (magic == 0x24787662) { ret = (i64)o; goto out; } /* bvx$ */
        if (magic == 0x2d787662) {                           /* bvx- (sem compressão) */
            if (i + 8 > slen) goto out;
            u32 n = le32(src + i + 4);
            if (i + 8 + n > slen || n > dlen - o) goto out;
            memcpy(dst + o, src + i + 8, n);
            o += n; i += 8 + n;
        } else if (magic == 0x6e787662) {                    /* bvxn (LZVN) */
            if (i + 12 > slen) goto out;
            u32 nraw = le32(src + i + 4), npay = le32(src + i + 8);
            if (i + 12 + npay > slen || nraw > dlen - o) goto out;
            i64 r = lzvn_decompress(src + i + 12, npay, dst + o, nraw);
            if (r != nraw) goto out;
            o += nraw; i += 12 + npay;
        } else if (magic == 0x32787662) {                    /* bvx2 */
            if (!c && !(c = kmalloc(sizeof(*c)))) goto out;
            u64 used = lzfse_block_v2(c, src + i, slen - i, dst, dlen, &o);
            if (!used) goto out;
            i += used;
        } else {
            goto out; /* bvx1 (antigo) ou lixo */
        }
    }
    ret = (i64)o; /* stream sem marcador final: aceita o que foi decodificado */
out:
    kfree(c);
    return ret;
}
