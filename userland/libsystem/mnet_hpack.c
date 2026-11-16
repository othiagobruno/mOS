/* mOS — mnet: HPACK (RFC 7541), compressão de cabeçalhos do HTTP/2. Tabelas em mnet_hpack_tab.inc. */
#include "mnet.h"
#include "mnet_hpack_tab.inc"

/* ---- Huffman: o código é canônico, então por comprimento os códigos são consecutivos */
static int hf_ready;
static uint32_t hf_first[31];      /* primeiro código de cada comprimento */
static uint16_t hf_count[31], hf_base[31];
static uint16_t hf_sym[257];       /* símbolos ordenados por (comprimento, código) */

static void hf_init(void) {
    if (hf_ready) return;
    int k = 0;
    for (int len = 1; len <= 30; len++) {
        hf_base[len] = (uint16_t)k;
        hf_count[len] = 0;
        /* inserção ordenada por código dentro do comprimento */
        for (int s = 0; s < 257; s++) {
            if (hp_huff_len[s] != len) continue;
            int j = k;
            while (j > hf_base[len] && hp_huff_code[hf_sym[j - 1]] > hp_huff_code[s]) { hf_sym[j] = hf_sym[j - 1]; j--; }
            hf_sym[j] = (uint16_t)s;
            k++;
            hf_count[len]++;
        }
        hf_first[len] = hf_count[len] ? hp_huff_code[hf_sym[hf_base[len]]] : 0;
    }
    hf_ready = 1;
}
int mnet_huff_decode(const uint8_t *d, size_t n, mnet_buf_t *out) {
    hf_init();
    uint32_t code = 0;
    int len = 0;
    for (size_t i = 0; i < n; i++)
        for (int b = 7; b >= 0; b--) {
            code = (code << 1) | ((d[i] >> b) & 1u);
            len++;
            if (len > 30) return -1;
            if (hf_count[len] && code >= hf_first[len] && code - hf_first[len] < hf_count[len]) {
                int s = hf_sym[hf_base[len] + (code - hf_first[len])];
                if (s == 256) return -1;                     /* EOS dentro da string: erro (§5.2) */
                if (mnet_buf_putc(out, s)) return -1;
                code = 0; len = 0;
            }
        }
    /* sobra: no máximo 7 bits, todos 1 (prefixo do EOS) */
    if (len > 7) return -1;
    if (len && code != (1u << len) - 1) return -1;
    return 0;
}
size_t mnet_huff_len(const uint8_t *d, size_t n) {
    size_t bits = 0;
    for (size_t i = 0; i < n; i++) bits += hp_huff_len[d[i]];
    return (bits + 7) / 8;
}
int mnet_huff_encode(const uint8_t *d, size_t n, mnet_buf_t *out) {
    uint64_t acc = 0;
    int bits = 0;
    for (size_t i = 0; i < n; i++) {
        acc = (acc << hp_huff_len[d[i]]) | hp_huff_code[d[i]];
        bits += hp_huff_len[d[i]];
        while (bits >= 8) { bits -= 8; if (mnet_buf_putc(out, (int)(acc >> bits) & 0xff)) return -1; }
        acc &= (1ull << bits) - 1;
    }
    if (bits) if (mnet_buf_putc(out, (int)((acc << (8 - bits)) | ((1u << (8 - bits)) - 1)) & 0xff)) return -1;
    return 0;
}

/* ---- tabela dinâmica */
void mnet_hpack_init(mnet_hpack_t *t, size_t max) {
    memset(t, 0, sizeof *t);
    t->max = max;
    t->pending_max = (size_t)-1;
}
static void ent_free(mnet_hp_ent_t *e) { free(e->name); free(e->value); e->name = e->value = 0; }
static void evict(mnet_hpack_t *t, size_t need) {
    while (t->count && t->size + need > t->max) {
        mnet_hp_ent_t *e = &t->e[(t->first + t->count - 1) % t->cap];   /* a mais velha */
        t->size -= e->size;
        ent_free(e);
        t->count--;
    }
}
void mnet_hpack_free(mnet_hpack_t *t) {
    for (int i = 0; i < t->count; i++) ent_free(&t->e[(t->first + i) % t->cap]);
    free(t->e);
    memset(t, 0, sizeof *t);
}
static void resize(mnet_hpack_t *t, size_t max) { t->max = max; evict(t, 0); }
void mnet_hpack_set_max(mnet_hpack_t *t, size_t max) {
    /* §4.2: entre dois blocos, anuncia o menor tamanho que houve e depois o final */
    if (t->pending_max == (size_t)-1 || max < t->pending_min) t->pending_min = max;
    t->pending_max = max;
    resize(t, max);
}
static int add(mnet_hpack_t *t, const char *n, size_t nl, const char *v, size_t vl) {
    size_t sz = nl + vl + 32;
    if (sz > t->max) { evict(t, t->max + 1); return 0; }   /* maior que a tabela: esvazia, não entra (§4.4) */
    evict(t, sz);
    if (t->count == t->cap) {
        int nc = t->cap ? t->cap * 2 : 32;
        mnet_hp_ent_t *ne = malloc((size_t)nc * sizeof *ne);
        if (!ne) return -1;
        for (int i = 0; i < t->count; i++) ne[i] = t->e[(t->first + i) % t->cap];
        free(t->e);
        t->e = ne; t->cap = nc; t->first = 0;
    }
    char *a = mnet_strndup(n, nl), *b = mnet_strndup(v, vl);
    if (!a || !b) { free(a); free(b); return -1; }
    t->first = (t->first + t->cap - 1) % t->cap;
    t->e[t->first].name = a;
    t->e[t->first].value = b;
    t->e[t->first].size = sz;
    t->count++;
    t->size += sz;
    return 0;
}
/* índice 1..61 estático, 62.. dinâmico */
static int lookup(const mnet_hpack_t *t, uint64_t idx, const char **n, const char **v) {
    if (idx == 0) return -1;
    if (idx <= 61) { *n = hp_static[idx - 1][0]; *v = hp_static[idx - 1][1]; return 0; }
    idx -= 62;
    if (idx >= (uint64_t)t->count) return -1;
    const mnet_hp_ent_t *e = &t->e[(t->first + (int)idx) % t->cap];
    *n = e->name; *v = e->value;
    return 0;
}

/* ---- primitivas */
static int dec_int(const uint8_t *d, size_t n, size_t *i, int prefix, uint64_t *out) {
    if (*i >= n) return -1;
    uint64_t mask = (1u << prefix) - 1, v = d[(*i)++] & mask;
    if (v < mask) { *out = v; return 0; }
    int shift = 0;
    for (;;) {
        if (*i >= n || shift > 56) return -1;
        uint8_t b = d[(*i)++];
        v += (uint64_t)(b & 0x7f) << shift;
        shift += 7;
        if (!(b & 0x80)) break;
    }
    *out = v;
    return 0;
}
static int dec_str(const uint8_t *d, size_t n, size_t *i, mnet_buf_t *out) {
    if (*i >= n) return -1;
    int huff = d[*i] & 0x80;
    uint64_t len;
    if (dec_int(d, n, i, 7, &len) || len > n - *i) return -1;
    out->n = 0;
    int r = huff ? mnet_huff_decode(d + *i, (size_t)len, out) : mnet_buf_put(out, d + *i, (size_t)len);
    *i += (size_t)len;
    return r;
}
static int enc_int(mnet_buf_t *o, int first, int prefix, uint64_t v) {
    uint64_t mask = (1u << prefix) - 1;
    if (v < mask) return mnet_buf_putc(o, first | (int)v);
    int r = mnet_buf_putc(o, first | (int)mask);
    v -= mask;
    while (v >= 128) { r |= mnet_buf_putc(o, (int)(v & 0x7f) | 0x80); v >>= 7; }
    r |= mnet_buf_putc(o, (int)v);
    return r;
}
static int enc_str(mnet_buf_t *o, const char *s, size_t n, int huffman) {
    size_t hl = huffman ? mnet_huff_len((const uint8_t *)s, n) : n + 1;
    if (huffman && hl <= n) { int r = enc_int(o, 0x80, 7, hl); return r | mnet_huff_encode((const uint8_t *)s, n, o); }
    return enc_int(o, 0, 7, n) | mnet_buf_put(o, s, n);
}

int mnet_hpack_decode(mnet_hpack_t *t, const uint8_t *d, size_t n, mnet_hdrs_t *out, size_t limit_max) {
    mnet_buf_t nb = {0}, vb = {0};
    size_t i = 0;
    int fields = 0, r = -1;
    while (i < n) {
        uint8_t b = d[i];
        if (b & 0x80) {                                      /* campo indexado */
            uint64_t idx;
            const char *nm, *v;
            if (dec_int(d, n, &i, 7, &idx) || lookup(t, idx, &nm, &v)) goto out;
            if (mnet_hdrs_add(out, nm, strlen(nm), v, strlen(v))) goto out;
            fields++;
            continue;
        }
        if ((b & 0xe0) == 0x20) {                            /* atualização do tamanho da tabela */
            uint64_t sz;
            if (fields || dec_int(d, n, &i, 5, &sz) || sz > limit_max) goto out;
            resize(t, (size_t)sz);
            continue;
        }
        int inc = (b & 0xc0) == 0x40;
        int prefix = inc ? 6 : 4;
        uint64_t idx;
        if (dec_int(d, n, &i, prefix, &idx)) goto out;
        const char *nm = 0, *v;
        size_t nml;
        if (idx) {
            if (lookup(t, idx, &nm, &v)) goto out;
            nb.n = 0;
            if (mnet_buf_puts(&nb, nm)) goto out;
        } else if (dec_str(d, n, &i, &nb)) goto out;
        if (dec_str(d, n, &i, &vb)) goto out;
        nml = nb.n;
        const char *ns = nb.p ? (const char *)nb.p : "";
        const char *vs = vb.p ? (const char *)vb.p : "";
        if (mnet_hdrs_add(out, ns, nml, vs, vb.n)) goto out;
        if (inc && add(t, ns, nml, vs, vb.n)) goto out;
        fields++;
    }
    r = 0;
out:
    mnet_buf_free(&nb);
    mnet_buf_free(&vb);
    return r;
}

static int sensitive(const char *n, const char *v) {
    if (mnet_ieq(n, "authorization") || mnet_ieq(n, "proxy-authorization")) return 1;
    if (mnet_ieq(n, "cookie") && strlen(v) < 20) return 1;      /* cookie curto: fácil de adivinhar (§7.1.3) */
    return 0;
}
int mnet_hpack_encode(mnet_hpack_t *t, const mnet_hdrs_t *h, mnet_buf_t *o, int huffman) {
    int r = 0;
    if (t->pending_max != (size_t)-1) {
        if (t->pending_min < t->pending_max) r |= enc_int(o, 0x20, 5, t->pending_min);
        r |= enc_int(o, 0x20, 5, t->pending_max);
        t->pending_max = (size_t)-1;
    }
    char lname[256];
    for (int k = 0; k < h->n && !r; k++) {
        const char *rn = h->v[k].name, *v = h->v[k].value;
        size_t nl = strlen(rn), vl = strlen(v);
        const char *nm = rn;
        if (nl < sizeof lname) {
            for (size_t q = 0; q <= nl; q++) lname[q] = (char)mnet_lc((unsigned char)rn[q]);
            nm = lname;
        }
        int full = 0, name_idx = 0;
        for (int s = 0; s < 61 && !full; s++)
            if (!strcmp(hp_static[s][0], nm)) {
                if (!name_idx) name_idx = s + 1;
                if (!strcmp(hp_static[s][1], v)) full = s + 1;
            }
        for (int s = 0; s < t->count && !full; s++) {
            const mnet_hp_ent_t *e = &t->e[(t->first + s) % t->cap];
            if (!strcmp(e->name, nm)) {
                if (!name_idx) name_idx = 62 + s;
                if (!strcmp(e->value, v)) full = 62 + s;
            }
        }
        int sens = sensitive(nm, v);
        if (full && !sens) { r |= enc_int(o, 0x80, 7, (uint64_t)full); continue; }
        int index = !sens && nl + vl + 32 <= t->max / 2 + 32;   /* valores enormes não entram na tabela */
        if (sens) r |= enc_int(o, 0x10, 4, (uint64_t)name_idx);
        else if (index) r |= enc_int(o, 0x40, 6, (uint64_t)name_idx);
        else r |= enc_int(o, 0x00, 4, (uint64_t)name_idx);
        if (!name_idx) r |= enc_str(o, nm, nl, huffman);
        r |= enc_str(o, v, vl, huffman);
        if (index) r |= add(t, nm, nl, v, vl) < 0;
    }
    return r ? -1 : 0;
}
