/*
 * mOS libSystem — libbz2 (escrita): BZ2_bzBuffToBuffCompress, a API de fluxo (BZ2_bzCompressInit/BZ2_bzCompress/
 * BZ2_bzCompressEnd) e a de arquivo (BZ2_bzWriteOpen/BZ2_bzWrite/BZ2_bzWriteClose64). Compressor escrito do zero a partir
 * do formato (o leitor é o bzip2.c): RLE inicial (4 iguais + contagem), BWT por dobra de prefixos sobre as rotações
 * (O(n log n) mesmo com dados periódicos), MTF, RUNA/RUNB, Huffman canônico (comprimento máximo 17) com 2 tabelas
 * iguais e um seletor por grupo de 50 símbolos, CRC por bloco e combinado. Saída aceita pelo bzip2 da Apple/GNU.
 * Teste: tools/bzip2_enc_hosttest.sh (ida e volta pelo bzip2 do host e pelo bzip2.c).
 */
#include "sys.h"
typedef void FILE_;
size_t fwrite(const void *p, size_t s, size_t n, FILE_ *f);
void *malloc(size_t n);
void *calloc(size_t n, size_t m);
void *realloc(void *p, size_t n);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
void *memmove(void *d, const void *s, size_t n);

enum { BZ_OK = 0, BZ_RUN_OK = 1, BZ_FLUSH_OK = 2, BZ_FINISH_OK = 3, BZ_STREAM_END = 4, BZ_SEQUENCE_ERROR = -1,
       BZ_PARAM_ERROR = -2, BZ_MEM_ERROR = -3, BZ_IO_ERROR = -6, BZ_OUTBUFF_FULL = -8 };
enum { BZ_RUN = 0, BZ_FLUSH = 1, BZ_FINISH = 2 };

static u32 ecrc_tab[256];
static void ecrc_init(void) {
    if (ecrc_tab[1]) return;
    for (u32 i = 0; i < 256; i++) { u32 c = i << 24; for (int k = 0; k < 8; k++) c = c & 0x80000000u ? (c << 1) ^ 0x04C11DB7u : c << 1; ecrc_tab[i] = c; }
}

/* ---------------------------------------------------------------- saída de bits (MSB primeiro) */
typedef struct { u8 *d; size_t n, cap; u64 acc; int nb; int fail; } bw_t;
static void bw_byte(bw_t *w, u8 b) {
    if (w->n == w->cap) {
        size_t nc = w->cap ? w->cap * 2 : 65536;
        u8 *nd = realloc(w->d, nc);
        if (!nd) { w->fail = 1; return; }
        w->d = nd; w->cap = nc;
    }
    w->d[w->n++] = b;
}
static void bw(bw_t *w, int k, u32 v) {
    w->acc = (w->acc << k) | (v & ((k == 32) ? 0xffffffffu : ((1u << k) - 1)));
    w->nb += k;
    while (w->nb >= 8) { w->nb -= 8; bw_byte(w, (u8)(w->acc >> w->nb)); }
}
static void bw_flush(bw_t *w) { if (w->nb) { bw_byte(w, (u8)(w->acc << (8 - w->nb))); w->nb = 0; } }

/* ---------------------------------------------------------------- BWT: ordena as rotações por dobra de prefixos */
static int bwt(const u8 *s, int n, u8 *last, int *sa, int *rk, int *tmp, int *cnt) {
    int csz = n > 256 ? n : 256;
    memset(cnt, 0, sizeof(int) * (size_t)csz);
    for (int i = 0; i < n; i++) cnt[s[i]]++;
    for (int i = 1; i < 256; i++) cnt[i] += cnt[i - 1];
    for (int i = n - 1; i >= 0; i--) sa[--cnt[s[i]]] = i;
    rk[sa[0]] = 0;
    int classes = 1;
    for (int i = 1; i < n; i++) { if (s[sa[i]] != s[sa[i - 1]]) classes++; rk[sa[i]] = classes - 1; }
    for (int k = 1; k < n && classes < n; k <<= 1) {
        /* ordem pela segunda chave (rk[i+k]): a ordem atual deslocada de k */
        for (int x = 0; x < n; x++) { int j = sa[x] - k; tmp[x] = j < 0 ? j + n : j; }
        memset(cnt, 0, sizeof(int) * (size_t)classes);
        for (int x = 0; x < n; x++) cnt[rk[tmp[x]]]++;
        for (int c = 1; c < classes; c++) cnt[c] += cnt[c - 1];
        for (int x = n - 1; x >= 0; x--) sa[--cnt[rk[tmp[x]]]] = tmp[x];
        /* novas classes em tmp */
        tmp[sa[0]] = 0;
        int nc = 1;
        for (int x = 1; x < n; x++) {
            int a = sa[x], b = sa[x - 1];
            int a2 = a + k >= n ? a + k - n : a + k, b2 = b + k >= n ? b + k - n : b + k;
            if (rk[a] != rk[b] || rk[a2] != rk[b2]) nc++;
            tmp[a] = nc - 1;
        }
        memcpy(rk, tmp, sizeof(int) * (size_t)n);
        classes = nc;
    }
    int orig = 0;
    for (int x = 0; x < n; x++) { int j = sa[x]; if (!j) orig = x; last[x] = s[j ? j - 1 : n - 1]; }
    return orig;
}

/* ---------------------------------------------------------------- Huffman com comprimento máximo */
static void huff_lengths(const u32 *freq, int n, u8 *len, int maxlen) {
    u32 w[260];
    for (int i = 0; i < n; i++) w[i] = freq[i] ? freq[i] : 1;
    for (;;) {
        /* árvore por seleção dos dois menores (n <= 258: O(n^2) é desprezível) */
        u64 wt[520]; int par[520]; int alive[520]; int nn = n;
        for (int i = 0; i < n; i++) { wt[i] = w[i]; alive[i] = 1; par[i] = -1; }
        for (int left = n; left > 1; left--) {
            int a = -1, b = -1;
            for (int i = 0; i < nn; i++) {
                if (!alive[i]) continue;
                if (a < 0 || wt[i] < wt[a]) { b = a; a = i; }
                else if (b < 0 || wt[i] < wt[b]) b = i;
            }
            wt[nn] = wt[a] + wt[b]; alive[nn] = 1; par[nn] = -1;
            alive[a] = alive[b] = 0; par[a] = par[b] = nn;
            nn++;
        }
        int too = 0;
        for (int i = 0; i < n; i++) {
            int d = 0;
            for (int j = i; par[j] >= 0; j = par[j]) d++;
            if (!d) d = 1;
            len[i] = (u8)d;
            if (d > maxlen) too = 1;
        }
        if (!too) return;
        for (int i = 0; i < n; i++) w[i] = 1 + w[i] / 2;
    }
}

/* ---------------------------------------------------------------- estado do compressor */
typedef struct {
    int level, cap;                          /* cap: bytes de RLE por bloco */
    u8 *blk; int nblk;
    u32 bcrc, combined;
    int run_ch, run_len;                     /* corrida pendente do RLE inicial */
    int started;
    bw_t out; size_t out_pos;                /* bytes já entregues de out.d */
    int *sa, *rk, *tmp, *cnt; u8 *last; u16 *mtfv;
    int finished;
    u64 total_in;
} enc_t;

static void enc_block(enc_t *e) {
    bw_t *w = &e->out;
    if (!e->started) { bw(w, 8, 'B'); bw(w, 8, 'Z'); bw(w, 8, 'h'); bw(w, 8, (u32)('0' + e->level)); e->started = 1; }
    int n = e->nblk;
    if (!n) return;
    u32 crc = ~e->bcrc;
    e->combined = ((e->combined << 1) | (e->combined >> 31)) ^ crc;
    int orig = bwt(e->blk, n, e->last, e->sa, e->rk, e->tmp, e->cnt);

    u8 inuse[256] = {0};
    for (int i = 0; i < n; i++) inuse[e->blk[i]] = 1;
    u8 unseq[256]; int ninuse = 0;
    u8 seq_of[256];
    for (int i = 0; i < 256; i++) if (inuse[i]) { seq_of[i] = (u8)ninuse; unseq[ninuse++] = (u8)i; }
    (void)unseq;
    int alpha = ninuse + 2, eob = ninuse + 1;

    /* MTF + RUNA/RUNB */
    u8 list[256];
    for (int i = 0; i < ninuse; i++) list[i] = (u8)i;
    int nm = 0, zp = 0;
    u32 freq[258] = {0};
#define EMIT(v) do { e->mtfv[nm++] = (u16)(v); freq[(v)]++; } while (0)
    for (int i = 0; i < n; i++) {
        u8 c = seq_of[e->last[i]];
        int j = 0;
        while (list[j] != c) j++;
        if (j) memmove(list + 1, list, (size_t)j);
        list[0] = c;
        if (!j) { zp++; continue; }
        if (zp) { int z = zp - 1; for (;;) { EMIT(z & 1); if (z < 2) break; z = (z - 2) / 2; } zp = 0; }
        EMIT(j + 1);
    }
    if (zp) { int z = zp - 1; for (;;) { EMIT(z & 1); if (z < 2) break; z = (z - 2) / 2; } }
    EMIT(eob);
#undef EMIT

    u8 len[258];
    huff_lengths(freq, alpha, len, 17);
    u32 code[258];
    {
        int minl = 32, maxl = 0;
        for (int i = 0; i < alpha; i++) { if (len[i] < minl) minl = len[i]; if (len[i] > maxl) maxl = len[i]; }
        u32 vec = 0;
        for (int l = minl; l <= maxl; l++) { for (int i = 0; i < alpha; i++) if (len[i] == l) code[i] = vec++; vec <<= 1; }
    }

    /* cabeçalho do bloco */
    bw(w, 24, 0x314159); bw(w, 24, 0x265359);
    bw(w, 32, crc);
    bw(w, 1, 0);
    bw(w, 24, (u32)orig);
    u32 used16 = 0;
    for (int i = 0; i < 16; i++) for (int j = 0; j < 16; j++) if (inuse[i * 16 + j]) { used16 |= 1u << (15 - i); break; }
    bw(w, 16, used16);
    for (int i = 0; i < 16; i++) {
        if (!(used16 & (1u << (15 - i)))) continue;
        u32 m = 0;
        for (int j = 0; j < 16; j++) if (inuse[i * 16 + j]) m |= 1u << (15 - j);
        bw(w, 16, m);
    }
    int nsel = (nm + 49) / 50;
    bw(w, 3, 2);                                       /* 2 tabelas (o mínimo do formato), iguais */
    bw(w, 15, (u32)nsel);
    for (int i = 0; i < nsel; i++) bw(w, 1, 0);         /* seletor 0 sempre: MTF dá 0 */
    for (int t = 0; t < 2; t++) {
        int cur = len[0];
        bw(w, 5, (u32)cur);
        for (int i = 0; i < alpha; i++) {
            while (cur < len[i]) { bw(w, 2, 2); cur++; }
            while (cur > len[i]) { bw(w, 2, 3); cur--; }
            bw(w, 1, 0);
        }
    }
    for (int i = 0; i < nm; i++) bw(w, len[e->mtfv[i]], code[e->mtfv[i]]);

    e->nblk = 0;
    e->bcrc = 0xffffffffu;
}

static void run_flush(enc_t *e) {
    if (e->run_len <= 0) return;
    u8 c = (u8)e->run_ch;
    if (e->nblk + 5 > e->cap) enc_block(e);
    for (int k = 0; k < e->run_len; k++) e->bcrc = (e->bcrc << 8) ^ ecrc_tab[(e->bcrc >> 24) ^ c];
    if (e->run_len < 4) for (int k = 0; k < e->run_len; k++) e->blk[e->nblk++] = c;
    else { for (int k = 0; k < 4; k++) e->blk[e->nblk++] = c; e->blk[e->nblk++] = (u8)(e->run_len - 4); }
    e->run_len = 0;
}
static void enc_feed(enc_t *e, const u8 *p, size_t n) {
    e->total_in += n;
    for (size_t i = 0; i < n; i++) {
        if (e->run_len && (p[i] != e->run_ch || e->run_len == 255)) run_flush(e);
        if (!e->run_len) e->run_ch = p[i];
        e->run_len++;
    }
}
static void enc_finish(enc_t *e) {
    run_flush(e);
    enc_block(e);
    if (!e->started) enc_block(e);                    /* fluxo vazio ainda tem "BZh9" */
    bw(&e->out, 24, 0x177245); bw(&e->out, 24, 0x385090);
    bw(&e->out, 32, e->combined);
    bw_flush(&e->out);
    e->finished = 1;
}
static enc_t *enc_new(int level) {
    if (level < 1 || level > 9) return 0;
    ecrc_init();
    enc_t *e = calloc(1, sizeof *e);
    if (!e) return 0;
    e->level = level;
    e->cap = level * 100000 - 19;
    int n = e->cap + 8;
    e->blk = malloc((size_t)n);
    e->last = malloc((size_t)n);
    e->sa = malloc(sizeof(int) * (size_t)n);
    e->rk = malloc(sizeof(int) * (size_t)n);
    e->tmp = malloc(sizeof(int) * (size_t)n);
    e->cnt = malloc(sizeof(int) * (size_t)(n > 256 ? n : 256));
    e->mtfv = malloc(sizeof(u16) * (size_t)(n + 2) * 2);
    e->bcrc = 0xffffffffu;
    if (!e->blk || !e->last || !e->sa || !e->rk || !e->tmp || !e->cnt || !e->mtfv) {
        free(e->blk); free(e->last); free(e->sa); free(e->rk); free(e->tmp); free(e->cnt); free(e->mtfv); free(e);
        return 0;
    }
    return e;
}
static void enc_free(enc_t *e) {
    if (!e) return;
    free(e->blk); free(e->last); free(e->sa); free(e->rk); free(e->tmp); free(e->cnt); free(e->mtfv); free(e->out.d); free(e);
}
/* descarta o que já foi entregue, para o buffer não crescer sem fim num fluxo longo */
static void enc_compact(enc_t *e) {
    if (e->out_pos && e->out_pos == e->out.n) { e->out.n = 0; e->out_pos = 0; }
}

/* ---------------------------------------------------------------- BZ2_bzBuffToBuffCompress */
int BZ2_bzBuffToBuffCompress(char *dest, unsigned *destlen, char *src, unsigned srclen, int block, int verb, int work) {
    (void)verb; (void)work;
    if (!dest || !destlen || (!src && srclen)) return BZ_PARAM_ERROR;
    enc_t *e = enc_new(block);
    if (!e) return block < 1 || block > 9 ? BZ_PARAM_ERROR : BZ_MEM_ERROR;
    enc_feed(e, (const u8 *)src, srclen);
    enc_finish(e);
    int r = BZ_OK;
    if (e->out.fail) r = BZ_MEM_ERROR;
    else if (e->out.n > *destlen) r = BZ_OUTBUFF_FULL;
    else { memcpy(dest, e->out.d, e->out.n); *destlen = (unsigned)e->out.n; }
    enc_free(e);
    return r;
}

/* ---------------------------------------------------------------- API de fluxo */
typedef struct {
    char *next_in; unsigned avail_in, total_in_lo32, total_in_hi32;
    char *next_out; unsigned avail_out, total_out_lo32, total_out_hi32;
    void *state; void *bzalloc, *bzfree, *opaque;
} bz_stream;
typedef struct { u32 magic; enc_t *e; int mode; } cstate_t;    /* mode: 0 run, 1 flush pendente, 2 finish */
#define CMAGIC 0x425a4543u
static void st_drain(bz_stream *s, enc_t *e) {
    size_t avail = e->out.n - e->out_pos;
    size_t k = avail < s->avail_out ? avail : s->avail_out;
    if (k) {
        memcpy(s->next_out, e->out.d + e->out_pos, k);
        e->out_pos += k; s->next_out += k; s->avail_out -= (unsigned)k;
        u64 t = ((u64)s->total_out_hi32 << 32 | s->total_out_lo32) + k;
        s->total_out_lo32 = (u32)t; s->total_out_hi32 = (u32)(t >> 32);
    }
    enc_compact(e);
}
int BZ2_bzCompressInit(bz_stream *s, int block, int verbosity, int work) {
    (void)verbosity; (void)work;
    if (!s || block < 1 || block > 9) return BZ_PARAM_ERROR;
    cstate_t *c = calloc(1, sizeof *c);
    if (!c) return BZ_MEM_ERROR;
    c->e = enc_new(block);
    if (!c->e) { free(c); return BZ_MEM_ERROR; }
    c->magic = CMAGIC;
    s->state = c;
    s->total_in_lo32 = s->total_in_hi32 = s->total_out_lo32 = s->total_out_hi32 = 0;
    return BZ_OK;
}
int BZ2_bzCompress(bz_stream *s, int action) {
    cstate_t *c = s ? (cstate_t *)s->state : 0;
    if (!c || c->magic != CMAGIC) return BZ_PARAM_ERROR;
    enc_t *e = c->e;
    if (c->mode == 2 && action != BZ_FINISH) return BZ_SEQUENCE_ERROR;
    if (s->avail_in && !e->finished) {
        enc_feed(e, (const u8 *)s->next_in, s->avail_in);
        u64 t = ((u64)s->total_in_hi32 << 32 | s->total_in_lo32) + s->avail_in;
        s->total_in_lo32 = (u32)t; s->total_in_hi32 = (u32)(t >> 32);
        s->next_in += s->avail_in;
        s->avail_in = 0;
    }
    if (e->out.fail) return BZ_MEM_ERROR;
    if (action == BZ_FINISH) {
        if (!e->finished) { c->mode = 2; enc_finish(e); }
        st_drain(s, e);
        return e->out.n - e->out_pos ? BZ_FINISH_OK : BZ_STREAM_END;
    }
    if (action == BZ_FLUSH) {
        if (c->mode != 1) { run_flush(e); enc_block(e); c->mode = 1; }
        st_drain(s, e);
        if (e->out.n - e->out_pos) return BZ_FLUSH_OK;
        c->mode = 0;
        return BZ_RUN_OK;
    }
    st_drain(s, e);
    return BZ_RUN_OK;
}
int BZ2_bzCompressEnd(bz_stream *s) {
    cstate_t *c = s ? (cstate_t *)s->state : 0;
    if (!c || c->magic != CMAGIC) return BZ_PARAM_ERROR;
    enc_free(c->e);
    c->magic = 0;
    free(c);
    s->state = 0;
    return BZ_OK;
}

/* ---------------------------------------------------------------- API de arquivo (BZFILE de escrita) */
typedef struct { u32 magic; enc_t *e; FILE_ *f; u64 written; } wfile_t;
#define WMAGIC 0x425a5746u
static int wf_out(wfile_t *w) {
    enc_t *e = w->e;
    size_t k = e->out.n - e->out_pos;
    if (k && fwrite(e->out.d + e->out_pos, 1, k, w->f) != k) return -1;
    w->written += k;
    e->out_pos = e->out.n;
    enc_compact(e);
    return 0;
}
void *BZ2_bzWriteOpen(int *bzerror, FILE_ *f, int block, int verbosity, int work) {
    (void)verbosity; (void)work;
    if (!f || block < 1 || block > 9) { if (bzerror) *bzerror = BZ_PARAM_ERROR; return 0; }
    wfile_t *w = calloc(1, sizeof *w);
    if (w) w->e = enc_new(block);
    if (!w || !w->e) { free(w); if (bzerror) *bzerror = BZ_MEM_ERROR; return 0; }
    w->magic = WMAGIC;
    w->f = f;
    if (bzerror) *bzerror = BZ_OK;
    return w;
}
void BZ2_bzWrite(int *bzerror, void *b, void *buf, int len) {
    wfile_t *w = (wfile_t *)b;
    if (!w || w->magic != WMAGIC || len < 0 || (!buf && len)) { if (bzerror) *bzerror = BZ_PARAM_ERROR; return; }
    enc_feed(w->e, (const u8 *)buf, (size_t)len);
    int r = w->e->out.fail ? BZ_MEM_ERROR : wf_out(w) ? BZ_IO_ERROR : BZ_OK;
    if (bzerror) *bzerror = r;
}
void BZ2_bzWriteClose64(int *bzerror, void *b, int abandon, u32 *in_lo, u32 *in_hi, u32 *out_lo, u32 *out_hi) {
    wfile_t *w = (wfile_t *)b;
    if (!w || w->magic != WMAGIC) { if (bzerror) *bzerror = BZ_PARAM_ERROR; return; }
    int r = BZ_OK;
    u64 total_out = 0;
    if (!abandon) {
        enc_finish(w->e);
        if (w->e->out.fail) r = BZ_MEM_ERROR; else if (wf_out(w)) r = BZ_IO_ERROR;
        total_out = w->written;
    }
    u64 tin = w->e->total_in;
    if (in_lo) *in_lo = (u32)tin;
    if (in_hi) *in_hi = (u32)(tin >> 32);
    if (out_lo) *out_lo = (u32)total_out;
    if (out_hi) *out_hi = (u32)(total_out >> 32);
    enc_free(w->e);
    w->magic = 0;
    free(w);
    if (bzerror) *bzerror = r;
}
