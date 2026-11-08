/*
 * mOS libSystem — leitor de bitcode LLVM (o AIR dos .metallib), incluído por mtlair.c. Portátil (também compila no
 * host, em tools/airdump.c): só precisa de memcpy/memset/malloc/calloc/realloc/free/strlen/strcmp.
 *
 * Bitstream (abreviações, BLOCKINFO, VBR, char6, blob) -> módulo: tipos, globais, funções, constantes, metadados
 * (strings em lote, nós, nós nomeados como !air.vertex) e corpos de função (instruções com IDs relativos).
 * Formato de referência: a documentação "LLVM Bitcode File Format" e o BitcodeReader do LLVM (Apache-2.0 com exceção
 * LLVM; aqui só o formato, escrito do zero).
 */

/* ================================================================ bitstream */
typedef struct { const u8 *p; u64 nbits, pos; int err; } bs_t;
static u64 bs_read(bs_t *b, int n) {
    if (n == 0) return 0;
    if (b->pos + (u64)n > b->nbits) { b->err = 1; b->pos = b->nbits; return 0; }
    u64 v = 0;
    for (int i = 0; i < n;) {
        u64 byte = b->pos >> 3;
        int bit = (int)(b->pos & 7);
        int take = 8 - bit;
        if (take > n - i) take = n - i;
        u64 chunk = (b->p[byte] >> bit) & ((1u << take) - 1);
        v |= chunk << i;
        i += take;
        b->pos += (u64)take;
    }
    return v;
}
static u64 bs_vbr(bs_t *b, int n) {
    u64 v = 0;
    int shift = 0;
    u64 hi = 1ull << (n - 1);
    for (;;) {
        u64 piece = bs_read(b, n);
        v |= (piece & (hi - 1)) << shift;
        if (!(piece & hi) || b->err) break;
        shift += n - 1;
        if (shift > 63) { b->err = 1; break; }
    }
    return v;
}
static void bs_align32(bs_t *b) { b->pos = (b->pos + 31) & ~31ull; if (b->pos > b->nbits) b->pos = b->nbits; }

/* abreviações */
enum { AO_LIT = 1, AO_FIXED, AO_VBR, AO_ARRAY, AO_CHAR6, AO_BLOB };
typedef struct { int kind; u64 val; } aop_t;
typedef struct { aop_t *ops; int n; } abbrev_t;
typedef struct { abbrev_t *a; int n, cap; } abbrevs_t;
static void abv_add(abbrevs_t *s, abbrev_t a) {
    if (s->n == s->cap) { s->cap = s->cap ? s->cap * 2 : 8; s->a = realloc(s->a, sizeof(abbrev_t) * (size_t)s->cap); }
    s->a[s->n++] = a;
}
/* BLOCKINFO: abreviações por id de bloco */
typedef struct { u32 id; abbrevs_t ab; } binfo_t;
typedef struct { binfo_t *b; int n; } binfos_t;
static abbrevs_t *binfo_of(binfos_t *bi, u32 id, int create) {
    for (int i = 0; i < bi->n; i++) if (bi->b[i].id == id) return &bi->b[i].ab;
    if (!create) return 0;
    bi->b = realloc(bi->b, sizeof(binfo_t) * (size_t)(bi->n + 1));
    memset(&bi->b[bi->n], 0, sizeof(binfo_t));
    bi->b[bi->n].id = id;
    return &bi->b[bi->n++].ab;
}

static abbrev_t read_abbrev(bs_t *b) {
    abbrev_t a = {0, 0};
    int n = (int)bs_vbr(b, 5);
    a.ops = calloc((size_t)(n ? n : 1), sizeof(aop_t));
    for (int i = 0; i < n && !b->err; i++) {
        int lit = (int)bs_read(b, 1);
        if (lit) { a.ops[a.n].kind = AO_LIT; a.ops[a.n].val = bs_vbr(b, 8); a.n++; continue; }
        int enc = (int)bs_read(b, 3);
        if (enc == 1) { a.ops[a.n].kind = AO_FIXED; a.ops[a.n].val = bs_vbr(b, 5); }
        else if (enc == 2) { a.ops[a.n].kind = AO_VBR; a.ops[a.n].val = bs_vbr(b, 5); }
        else if (enc == 3) a.ops[a.n].kind = AO_ARRAY;
        else if (enc == 4) a.ops[a.n].kind = AO_CHAR6;
        else if (enc == 5) a.ops[a.n].kind = AO_BLOB;
        else b->err = 1;
        a.n++;
    }
    return a;
}

/* um registro: código + operandos (e o blob, se houver) */
typedef struct { u32 code; u64 *ops; int n, cap; const u8 *blob; u64 bloblen; } rec_t;
static void rec_push(rec_t *r, u64 v) {
    if (r->n == r->cap) { r->cap = r->cap ? r->cap * 2 : 32; r->ops = realloc(r->ops, sizeof(u64) * (size_t)r->cap); }
    r->ops[r->n++] = v;
}
static u64 read_scalar(bs_t *b, const aop_t *o) {
    switch (o->kind) {
    case AO_LIT: return o->val;
    case AO_FIXED: return bs_read(b, (int)o->val);
    case AO_VBR: return bs_vbr(b, (int)o->val);
    case AO_CHAR6: {
        u64 c = bs_read(b, 6);
        return c < 26 ? 'a' + c : c < 52 ? 'A' + (c - 26) : c < 62 ? '0' + (c - 52) : c == 62 ? '.' : '_';
    }
    }
    b->err = 1;
    return 0;
}
static void read_abbrev_record(bs_t *b, const abbrev_t *a, rec_t *r) {
    r->n = 0;
    r->blob = 0;
    r->bloblen = 0;
    int first = 1;
    for (int i = 0; i < a->n && !b->err; i++) {
        const aop_t *o = &a->ops[i];
        if (o->kind == AO_ARRAY) {
            u64 cnt = bs_vbr(b, 6);
            const aop_t *el = &a->ops[++i];
            for (u64 k = 0; k < cnt && !b->err; k++) {
                u64 v = read_scalar(b, el);
                if (first) { r->code = (u32)v; first = 0; } else rec_push(r, v);
            }
            continue;
        }
        if (o->kind == AO_BLOB) {
            u64 len = bs_vbr(b, 6);
            bs_align32(b);
            if (b->pos / 8 + len > b->nbits / 8) { b->err = 1; return; }
            r->blob = b->p + b->pos / 8;
            r->bloblen = len;
            b->pos += len * 8;
            bs_align32(b);
            continue;
        }
        u64 v = read_scalar(b, o);
        if (first) { r->code = (u32)v; first = 0; } else rec_push(r, v);
    }
}

/* um cursor dentro de um bloco */
typedef struct {
    bs_t *b;
    int width;
    abbrevs_t local;
    abbrevs_t *info;          /* as do BLOCKINFO para este id */
    u64 end;                  /* posição do fim do bloco (em bits) */
    u32 id;
} blk_t;
enum { EN_END = 0, EN_SUB, EN_REC, EN_ERR };
/* próxima entrada: registro em r, ou id do sub-bloco em *sub */
static int blk_next(blk_t *k, rec_t *r, u32 *sub, binfos_t *bi) {
    bs_t *b = k->b;
    for (;;) {
        if (b->err || b->pos >= b->nbits) return EN_ERR;
        u64 abbr = bs_read(b, k->width);
        if (abbr == 0) { bs_align32(b); return EN_END; }                          /* END_BLOCK */
        if (abbr == 1) { *sub = (u32)bs_vbr(b, 8); return EN_SUB; }               /* ENTER_SUBBLOCK */
        if (abbr == 2) { abbrev_t a = read_abbrev(b); abv_add(&k->local, a); continue; }   /* DEFINE_ABBREV */
        if (abbr == 3) {                                                           /* UNABBREV_RECORD */
            r->code = (u32)bs_vbr(b, 6);
            u64 n = bs_vbr(b, 6);
            r->n = 0;
            r->blob = 0;
            for (u64 i = 0; i < n && !b->err; i++) rec_push(r, bs_vbr(b, 6));
            return EN_REC;
        }
        u64 idx = abbr - 4;
        int ni = k->info ? k->info->n : 0;
        const abbrev_t *a = idx < (u64)ni ? &k->info->a[idx] : idx - (u64)ni < (u64)k->local.n ? &k->local.a[idx - (u64)ni] : 0;
        if (!a) { b->err = 1; return EN_ERR; }
        read_abbrev_record(b, a, r);
        (void)bi;
        return EN_REC;
    }
}
/* entra no sub-bloco (depois de EN_SUB) */
static void blk_enter(blk_t *k, bs_t *b, u32 id, binfos_t *bi) {
    memset(k, 0, sizeof *k);
    k->b = b;
    k->id = id;
    k->width = (int)bs_vbr(b, 4);
    bs_align32(b);
    u64 words = bs_read(b, 32);
    k->end = b->pos + words * 32;
    k->info = binfo_of(bi, id, 0);
}
static void blk_skip(bs_t *b) {
    bs_vbr(b, 4);
    bs_align32(b);
    u64 words = bs_read(b, 32);
    b->pos += words * 32;
    if (b->pos > b->nbits) { b->pos = b->nbits; b->err = 1; }
}
static void blk_free(blk_t *k) {
    for (int i = 0; i < k->local.n; i++) free(k->local.a[i].ops);
    free(k->local.a);
}
/* BLOCKINFO (bloco 0): SETBID e as abreviações que valem para outros blocos */
static void read_blockinfo(bs_t *b, binfos_t *bi) {
    blk_t k;
    blk_enter(&k, b, 0, bi);
    rec_t r = {0};
    abbrevs_t *cur = 0;
    for (;;) {
        u64 abbr = bs_read(b, k.width);
        if (b->err) break;
        if (abbr == 0) { bs_align32(b); break; }
        if (abbr == 1) { u32 sid = (u32)bs_vbr(b, 8); (void)sid; blk_skip(b); continue; }
        if (abbr == 2) { abbrev_t a = read_abbrev(b); if (cur) abv_add(cur, a); else free(a.ops); continue; }
        if (abbr == 3) {
            r.code = (u32)bs_vbr(b, 6);
            u64 n = bs_vbr(b, 6);
            r.n = 0;
            for (u64 i = 0; i < n; i++) rec_push(&r, bs_vbr(b, 6));
            if (r.code == 1 && r.n >= 1) cur = binfo_of(bi, (u32)r.ops[0], 1);   /* SETBID */
            continue;
        }
        b->err = 1;
        break;
    }
    free(r.ops);
    blk_free(&k);
}

static i64 sign_vbr(u64 v) { return (v & 1) ? -(i64)(v >> 1) : (i64)(v >> 1); }
