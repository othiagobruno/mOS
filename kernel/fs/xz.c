/*
 * mOS — descompressor xz / LZMA2 (chunks ULMO de imagens DMG, como o instalador do Google Chrome).
 *
 * Implementação própria e compacta, seguindo as especificações públicas do formato .xz
 * (tukaani.org/xz/xz-file-format.txt) e do LZMA2. Decodifica direto no buffer de saída
 * (que inteiro faz o papel do dicionário), então não aloca janela; só as probabilidades (~30 KiB).
 * Aceita streams concatenados e o filtro LZMA2 sozinho (BCJ/delta não são usados pelo ULMO).
 * As somas de verificação (CRC32/CRC64/SHA-256) são puladas: o UDIF já tem a própria.
 */
#include "fs.h"

/* ---------------------------------------------------------------- decodificador de faixa */
typedef struct {
    const u8 *in, *end;
    u32 range, code;
    bool err;
} rc_t;

static bool rc_init(rc_t *rc, const u8 *in, const u8 *end) {
    if (end - in < 5 || in[0] != 0) return false;
    rc->in = in + 5;
    rc->end = end;
    rc->code = (u32)in[1] << 24 | (u32)in[2] << 16 | (u32)in[3] << 8 | in[4];
    rc->range = 0xFFFFFFFFu;
    rc->err = false;
    return true;
}

static inline void rc_norm(rc_t *rc) {
    if (rc->range < (1u << 24)) {
        rc->range <<= 8;
        if (rc->in < rc->end) rc->code = (rc->code << 8) | *rc->in++;
        else { rc->code <<= 8; rc->err = true; }
    }
}

static inline u32 rc_bit(rc_t *rc, u16 *p) {
    rc_norm(rc);
    u32 bound = (rc->range >> 11) * *p;
    if (rc->code < bound) {
        rc->range = bound;
        *p += (2048 - *p) >> 5;
        return 0;
    }
    rc->range -= bound;
    rc->code -= bound;
    *p -= *p >> 5;
    return 1;
}

static inline u32 rc_tree(rc_t *rc, u16 *probs, u32 bits) {
    u32 s = 1;
    for (u32 i = 0; i < bits; i++) s = (s << 1) | rc_bit(rc, &probs[s]);
    return s - (1u << bits);
}

/* árvore reversa: lê probs[s - 1], com o símbolo s começando em 1 (quem chama ajusta a base) */
static inline u32 rc_tree_rev(rc_t *rc, u16 *probs, u32 bits) {
    u32 s = 1, r = 0;
    for (u32 i = 0; i < bits; i++) {
        u32 b = rc_bit(rc, &probs[s - 1]);
        s = (s << 1) | b;
        r |= b << i;
    }
    return r;
}

static inline u32 rc_direct(rc_t *rc, u32 bits) {
    u32 r = 0;
    while (bits--) {
        rc_norm(rc);
        rc->range >>= 1;
        u32 b = rc->code >= rc->range;
        if (b) rc->code -= rc->range;
        r = (r << 1) | b;
    }
    return r;
}

/* ---------------------------------------------------------------- estado do LZMA */
#define NSTATES      12
#define POS_MAX      16
#define LEN_LOW      8
#define LEN_MID      8
#define LEN_HIGH     256
#define DIST_SLOTS   64
#define DIST_END     14
#define FULL_DIST    128
#define ALIGN_BITS   4

typedef struct {
    u16 choice, choice2;
    u16 low[POS_MAX][LEN_LOW], mid[POS_MAX][LEN_MID], high[LEN_HIGH];
} lenprob_t;

typedef struct {
    u16 is_match[NSTATES][POS_MAX], is_rep[NSTATES], is_rep0[NSTATES], is_rep1[NSTATES], is_rep2[NSTATES];
    u16 is_rep0_long[NSTATES][POS_MAX];
    u16 dist_slot[4][DIST_SLOTS];
    u16 dist_special[FULL_DIST - DIST_END];
    u16 align[1 << ALIGN_BITS];
    lenprob_t match_len, rep_len;
    u16 literal[0x300 << 4];          /* lc + lp <= 4 no LZMA2 */
} probs_t;

typedef struct {
    probs_t p;
    u32 lc, lp, pb;
    u32 state, rep0, rep1, rep2, rep3;
    u32 pending;                      /* bytes de um match que passou do fim do chunk anterior */
    bool props_ok, need_state_reset, need_dict_reset;
    u8 *out;
    u64 pos, cap, dict_start;
} lzma_t;

static void lzma_reset_state(lzma_t *z) {
    u16 *p = (u16 *)&z->p;
    u64 n = sizeof(probs_t) / sizeof(u16);
    for (u64 i = 0; i < n; i++) p[i] = 1024;
    z->state = 0;
    z->rep0 = z->rep1 = z->rep2 = z->rep3 = 0;
    z->pending = 0;
}

static u32 lzma_len(rc_t *rc, lenprob_t *l, u32 ps) {
    if (!rc_bit(rc, &l->choice)) return 2 + rc_tree(rc, l->low[ps], 3);
    if (!rc_bit(rc, &l->choice2)) return 2 + LEN_LOW + rc_tree(rc, l->mid[ps], 3);
    return 2 + LEN_LOW + LEN_MID + rc_tree(rc, l->high, 8);
}

/* copia um match, parando no fim do chunk (o resto fica pendente) */
static bool lzma_copy(lzma_t *z, u32 len, u64 limit) {
    u64 dist = (u64)z->rep0 + 1;
    if (dist > z->pos - z->dict_start) return false;
    u8 *o = z->out;
    while (len && z->pos < limit) {
        o[z->pos] = o[z->pos - dist];
        z->pos++;
        len--;
    }
    z->pending = len;
    return true;
}

/* decodifica um chunk LZMA até z->pos == limit */
static bool lzma_chunk(lzma_t *z, rc_t *rc, u64 limit) {
    probs_t *P = &z->p;
    u32 pbm = (1u << z->pb) - 1, lpm = (1u << z->lp) - 1;
    if (z->pending && !lzma_copy(z, z->pending, limit)) return false;
    while (z->pos < limit) {
        if (rc->err) return false;
        u64 rel = z->pos - z->dict_start;
        u32 ps = (u32)rel & pbm;
        u32 st = z->state;
        if (!rc_bit(rc, &P->is_match[st][ps])) {
            /* literal */
            u32 prev = rel ? z->out[z->pos - 1] : 0;
            u16 *lp = &P->literal[0x300 * ((((u32)rel & lpm) << z->lc) + (prev >> (8 - z->lc)))];
            u32 s = 1;
            if (st < 7) {
                while (s < 0x100) s = (s << 1) | rc_bit(rc, &lp[s]);
            } else {
                if ((u64)z->rep0 + 1 > rel) return false;
                u32 mb = (u32)z->out[z->pos - z->rep0 - 1] << 1, off = 0x100;
                while (s < 0x100) {
                    u32 bit_m = mb & off;
                    mb <<= 1;
                    if (rc_bit(rc, &lp[off + bit_m + s])) { s = (s << 1) | 1; off &= bit_m; }
                    else { s <<= 1; off &= ~bit_m; }
                }
            }
            z->out[z->pos++] = (u8)s;
            z->state = st < 4 ? 0 : st < 10 ? st - 3 : st - 6;
            continue;
        }
        u32 len;
        if (!rc_bit(rc, &P->is_rep[st])) {
            /* match novo */
            z->rep3 = z->rep2; z->rep2 = z->rep1; z->rep1 = z->rep0;
            len = lzma_len(rc, &P->match_len, ps);
            z->state = st < 7 ? 7 : 10;
            u32 ls = len - 2 < 3 ? len - 2 : 3;
            u32 slot = rc_tree(rc, P->dist_slot[ls], 6);
            u32 dist;
            if (slot < 4) dist = slot;
            else {
                u32 nd = (slot >> 1) - 1;
                dist = (2 | (slot & 1)) << nd;
                if (slot < DIST_END) {
                    /* a árvore começa em dist_special[dist - slot - 1 + 1] (o símbolo começa em 1) */
                    dist += rc_tree_rev(rc, &P->dist_special[dist - slot], nd);
                } else {
                    dist += rc_direct(rc, nd - ALIGN_BITS) << ALIGN_BITS;
                    /* align[16] com símbolo a partir de 1: índices 1..15 */
                    dist += rc_tree_rev(rc, P->align + 1, ALIGN_BITS);
                }
            }
            if (dist == 0xFFFFFFFFu) return false;   /* marcador de fim: não existe no LZMA2 */
            z->rep0 = dist;
        } else if (!rc_bit(rc, &P->is_rep0[st])) {
            if (!rc_bit(rc, &P->is_rep0_long[st][ps])) {
                /* short rep: um byte de rep0 */
                z->state = st < 7 ? 9 : 11;
                if (!lzma_copy(z, 1, limit)) return false;
                continue;
            }
            len = lzma_len(rc, &P->rep_len, ps);
            z->state = st < 7 ? 8 : 11;
        } else {
            u32 t;
            if (!rc_bit(rc, &P->is_rep1[st])) t = z->rep1;
            else {
                if (!rc_bit(rc, &P->is_rep2[st])) t = z->rep2;
                else { t = z->rep3; z->rep3 = z->rep2; }
                z->rep2 = z->rep1;
            }
            z->rep1 = z->rep0;
            z->rep0 = t;
            len = lzma_len(rc, &P->rep_len, ps);
            z->state = st < 7 ? 8 : 11;
        }
        if (!lzma_copy(z, len, limit)) return false;
    }
    return !rc->err;
}

/* ---------------------------------------------------------------- LZMA2 (dados de um bloco) */
static i64 lzma2_decode(lzma_t *z, const u8 *in, const u8 *end) {
    const u8 *s = in;
    z->need_dict_reset = true;
    z->props_ok = false;
    for (;;) {
        if (s >= end) return -1;
        u8 c = *s++;
        if (c == 0) return s - in;                       /* fim dos dados LZMA2 */
        if (c == 1 || c == 2) {                          /* chunk sem compressão */
            if (end - s < 2) return -1;
            u64 n = ((u64)s[0] << 8 | s[1]) + 1;
            s += 2;
            if (c == 1) { z->dict_start = z->pos; z->need_dict_reset = false; }
            else if (z->need_dict_reset) return -1;
            if ((u64)(end - s) < n || z->cap - z->pos < n) return -1;
            memcpy(z->out + z->pos, s, n);
            z->pos += n;
            s += n;
            z->need_state_reset = true;
            z->pending = 0;
            continue;
        }
        if (c < 0x80) return -1;
        if (end - s < 4) return -1;
        u64 usize = ((u64)(c & 0x1F) << 16 | (u64)s[0] << 8 | s[1]) + 1;
        u64 psize = ((u64)s[2] << 8 | s[3]) + 1;
        s += 4;
        u32 reset = (c >> 5) & 3;
        if (reset == 3) { z->dict_start = z->pos; z->need_dict_reset = false; }
        else if (z->need_dict_reset) return -1;
        if (reset >= 2) {
            if (s >= end) return -1;
            u32 pr = *s++;
            if (pr >= 9 * 5 * 5) return -1;
            z->lc = pr % 9; pr /= 9;
            z->lp = pr % 5;
            z->pb = pr / 5;
            if (z->lc + z->lp > 4) return -1;
            z->props_ok = true;
        } else if (!z->props_ok) return -1;
        if (reset >= 1) { lzma_reset_state(z); z->need_state_reset = false; }
        else if (z->need_state_reset) return -1;
        if ((u64)(end - s) < psize || z->cap - z->pos < usize) return -1;
        rc_t rc;
        if (!rc_init(&rc, s, s + psize)) return -1;
        if (!lzma_chunk(z, &rc, z->pos + usize)) return -1;
        s += psize;
    }
}

/* ---------------------------------------------------------------- contêiner .xz */
static bool vli(const u8 **p, const u8 *end, u64 *v) {
    u64 r = 0;
    for (u32 i = 0; i < 9; i++) {
        if (*p >= end) return false;
        u8 b = *(*p)++;
        r |= (u64)(b & 0x7F) << (7 * i);
        if (!(b & 0x80)) { *v = r; return true; }
    }
    return false;
}

static const u8 XZ_MAGIC[6] = {0xFD, '7', 'z', 'X', 'Z', 0};

static u32 check_size(u32 t) {
    if (t == 0) return 0;
    return 4u << ((t - 1) / 3);   /* 1-3: 4, 4-6: 8, 7-9: 16, 10-12: 32, 13-15: 64 */
}

i64 xz_decompress(const u8 *src, u64 slen, u8 *dst, u64 dlen) {
    lzma_t *z = kmalloc(sizeof(lzma_t));
    if (!z) return -1;
    memset(z, 0, sizeof(*z));
    z->out = dst;
    z->cap = dlen;
    const u8 *p = src, *end = src + slen;
    i64 result = -1;
    bool any = false;
    for (;;) {
        /* entre streams pode haver preenchimento de zeros */
        while (p + 4 <= end && !p[0] && !p[1] && !p[2] && !p[3]) p += 4;
        if (end - p < 12 || memcmp(p, XZ_MAGIC, 6)) { if (any) result = (i64)z->pos; break; }
        if (p[6] != 0 || p[7] > 15) break;
        u32 csz = check_size(p[7]);
        p += 12;
        bool ok = true;
        /* blocos */
        for (;;) {
            if (p >= end) { ok = false; break; }
            if (*p == 0) break;                          /* começo do índice */
            const u8 *bh = p;
            u64 hsz = ((u64)*p + 1) * 4;
            if ((u64)(end - p) < hsz) { ok = false; break; }
            const u8 *q = p + 1, *hend = p + hsz - 4;
            u8 flags = *q++;
            if (flags & 0x3C) { ok = false; break; }
            u64 tmp;
            if ((flags & 0x40) && !vli(&q, hend, &tmp)) { ok = false; break; }
            if ((flags & 0x80) && !vli(&q, hend, &tmp)) { ok = false; break; }
            u32 nf = (flags & 3) + 1;
            for (u32 f = 0; f < nf && ok; f++) {
                u64 id, psz;
                if (!vli(&q, hend, &id) || !vli(&q, hend, &psz) || (u64)(hend - q) < psz) { ok = false; break; }
                if (id != 0x21 || f != nf - 1) {
                    kprintf("xz: filtro %llx não suportado\n", (unsigned long long)id);
                    ok = false;
                    break;
                }
                q += psz;
            }
            if (!ok) break;
            p += hsz;
            i64 used = lzma2_decode(z, p, end);
            if (used < 0) { ok = false; break; }
            p += used;
            u64 blen = (u64)(p - bh);
            while (blen & 3) { if (p >= end) { ok = false; break; } p++; blen++; }
            if (!ok) break;
            if ((u64)(end - p) < csz) { ok = false; break; }
            p += csz;
            any = true;
        }
        if (!ok) break;
        /* índice: 0x00, número de registros, registros, preenchimento, CRC32 */
        const u8 *ix = p++;
        u64 nrec;
        if (!vli(&p, end, &nrec)) break;
        for (u64 i = 0; i < nrec && ok; i++) {
            u64 a, b;
            if (!vli(&p, end, &a) || !vli(&p, end, &b)) ok = false;
        }
        if (!ok) break;
        while ((p - ix) & 3) { if (p >= end) { ok = false; break; } p++; }
        if (!ok || end - p < 4 + 12) break;
        p += 4;
        if (p[10] != 'Y' || p[11] != 'Z') break;    /* rodapé do stream */
        p += 12;
        any = true;
        if (z->pos >= z->cap) { result = (i64)z->pos; break; }
    }
    kfree(z);
    return result;
}
