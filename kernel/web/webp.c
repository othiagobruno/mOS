/* mOS — decodificador WebP do motor web (implementação própria a partir da especificação):
 *   - contêiner RIFF: "VP8 " (com perdas), "VP8L" (sem perdas), "VP8X" (estendido), "ALPH" (alfa do VP8),
 *     "ANIM"/"ANMF" (animação: o primeiro quadro, composto na tela do tamanho do VP8X);
 *   - VP8L: transformações (preditor, cor, subtrair verde, índice de cor com empacotamento), códigos de prefixo
 *     simples e normais, LZ77 com mapa de distâncias 2D, cache de cores e imagem de meta-códigos;
 *   - VP8 (RFC 6386), quadro-chave: decodificador booleano, segmentos, partições de coeficientes, previsão intra
 *     16x16/8x8/4x4, IDCT e WHT inteiras, filtro de laço normal e simples, YUV→RGB com reamostragem bilinear;
 *   - ALPH: sem compressão ou VP8L, com os filtros horizontal, vertical e gradiente.
 * Só depende de kmalloc/kfree e do surface_t (BGRA pré-multiplicado), então serve também ao ImageIO. */
#include "web.h"
#include "gfx.h"
#include "webp_tab.h"

static u32 le16(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8; }
static u32 le24(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16; }
static u32 le32(const u8 *p) { return le24(p) | (u32)p[3] << 24; }

#define WEBP_MAX_DIM 16383
#define WEBP_MAX_PIXELS (64u * 1024 * 1024)

/* ================================================================ VP8L */

typedef struct {
    const u8 *p, *end;
    u64 buf;
    int nb;
    int over;     /* bytes lidos além do fim (zeros) */
    bool eos;
} lbr_t;

static void lbr_init(lbr_t *b, const u8 *p, usize n) {
    b->p = p; b->end = p + n; b->buf = 0; b->nb = 0; b->over = 0; b->eos = false;
}
static inline void lbr_fill(lbr_t *b) {
    while (b->nb <= 56) {
        u64 v = 0;
        if (b->p < b->end) v = *b->p++;
        else if (++b->over > 8) b->eos = true;
        b->buf |= v << b->nb;
        b->nb += 8;
    }
}
static inline u32 lbr_get(lbr_t *b, int n) {
    if (!n) return 0;
    lbr_fill(b);
    u32 v = (u32)(b->buf & ((1ull << n) - 1));
    b->buf >>= n;
    b->nb -= n;
    return v;
}

/* código de prefixo canônico (como o deflate: o 1º bit lido é o mais significativo do código) */
typedef struct {
    u16 count[16];
    u16 *sym;
    int single;          /* >= 0: um símbolo só, código de 0 bits */
    u16 fast[256];       /* consulta direta dos códigos de até 8 bits */
    u8 fastlen[256];     /* 0 = mais de 8 bits: caminho lento */
} lhuff_t;

static void lhuff_free(lhuff_t *h) { if (h->sym) kfree(h->sym); h->sym = NULL; }

static bool lhuff_build(lhuff_t *h, const u8 *len, int n) {
    memset(h, 0, sizeof *h);
    h->single = -1;
    int nz = 0, last = -1;
    for (int i = 0; i < n; i++) {
        if (len[i] > 15) return false;
        if (len[i]) { h->count[len[i]]++; nz++; last = i; }
    }
    if (nz == 0) return false;
    if (nz == 1) { h->single = last; return true; }
    int left = 1;
    for (int l = 1; l <= 15; l++) { left <<= 1; left -= h->count[l]; if (left < 0) return false; }
    if (left) return false;       /* incompleto: só vale com um símbolo */
    u16 offs[16];
    offs[1] = 0;
    for (int l = 1; l < 15; l++) offs[l + 1] = offs[l] + h->count[l];
    h->sym = kmalloc(sizeof(u16) * nz);
    if (!h->sym) return false;
    for (int i = 0; i < n; i++) if (len[i]) h->sym[offs[len[i]]++] = (u16)i;
    /* tabela rápida: código canônico invertido (lido do bit menos significativo) */
    int code = 0, idx = 0;
    for (int l = 1; l <= 8; l++) {
        for (int k = 0; k < h->count[l]; k++, idx++, code++) {
            int rev = 0;
            for (int b = 0; b < l; b++) if (code & (1 << b)) rev |= 1 << (l - 1 - b);
            for (int e = rev; e < 256; e += 1 << l) { h->fast[e] = h->sym[idx]; h->fastlen[e] = (u8)l; }
        }
        code <<= 1;
    }
    return true;
}

static inline int lhuff_dec(lbr_t *b, const lhuff_t *h) {
    if (h->single >= 0) return h->single;
    lbr_fill(b);
    u32 e = (u32)(b->buf & 0xFF);
    if (h->fastlen[e]) { int l = h->fastlen[e]; b->buf >>= l; b->nb -= l; return h->fast[e]; }
    int code = 0, first = 0, index = 0;
    for (int l = 1; l <= 15; l++) {
        code |= (int)lbr_get(b, 1);
        int cnt = h->count[l];
        if (code - cnt < first) return h->sym[index + (code - first)];
        index += cnt;
        first += cnt;
        first <<= 1;
        code <<= 1;
    }
    return -1;
}

static bool read_code(lbr_t *b, int asize, lhuff_t *h) {
    u8 *len = kcalloc(asize, 1);
    if (!len) return false;
    bool ok = false;
    if (lbr_get(b, 1)) {                          /* código simples: 1 ou 2 símbolos */
        int ns = (int)lbr_get(b, 1) + 1;
        int first8 = (int)lbr_get(b, 1);
        int s0 = (int)lbr_get(b, first8 ? 8 : 1);
        if (s0 >= asize) goto out;
        len[s0] = 1;
        if (ns == 2) {
            int s1 = (int)lbr_get(b, 8);
            if (s1 >= asize) goto out;
            len[s1] = 1;
        }
        ok = lhuff_build(h, len, asize);
        goto out;
    }
    static const u8 ord[19] = { 17, 18, 0, 1, 2, 3, 4, 5, 16, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
    u8 cl[19] = { 0 };
    int ncl = 4 + (int)lbr_get(b, 4);
    for (int i = 0; i < ncl; i++) cl[ord[i]] = (u8)lbr_get(b, 3);
    lhuff_t lc;
    if (!lhuff_build(&lc, cl, 19)) goto out;
    int maxsym = asize;
    if (lbr_get(b, 1)) {
        int nb = 2 + 2 * (int)lbr_get(b, 3);
        maxsym = 2 + (int)lbr_get(b, nb);
        if (maxsym > asize) { lhuff_free(&lc); goto out; }
    }
    int prev = 8, s = 0;
    while (s < asize) {
        if (maxsym-- == 0) break;
        int c = lhuff_dec(b, &lc);
        if (c < 0 || b->eos) { lhuff_free(&lc); goto out; }
        if (c < 16) {
            len[s++] = (u8)c;
            if (c) prev = c;
        } else {
            int rep, val = 0;
            if (c == 16) { rep = 3 + (int)lbr_get(b, 2); val = prev; }
            else if (c == 17) rep = 3 + (int)lbr_get(b, 3);
            else rep = 11 + (int)lbr_get(b, 7);
            if (s + rep > asize) { lhuff_free(&lc); goto out; }
            while (rep--) len[s++] = (u8)val;
        }
    }
    lhuff_free(&lc);
    ok = lhuff_build(h, len, asize);
out:
    kfree(len);
    return ok;
}

typedef struct { lhuff_t h[5]; } hgroup_t;

static int prefix_value(lbr_t *b, int sym) {
    if (sym < 4) return sym + 1;
    int eb = (sym - 2) >> 1;
    int off = (2 + (sym & 1)) << eb;
    return off + (int)lbr_get(b, eb) + 1;
}

static const u8 code_to_plane[120] = {
    0x18, 0x07, 0x17, 0x19, 0x28, 0x06, 0x27, 0x29, 0x16, 0x1a, 0x26, 0x2a, 0x38, 0x05, 0x37, 0x39, 0x15, 0x1b, 0x36, 0x3a,
    0x25, 0x2b, 0x48, 0x04, 0x47, 0x49, 0x14, 0x1c, 0x35, 0x3b, 0x46, 0x4a, 0x24, 0x2c, 0x58, 0x45, 0x4b, 0x34, 0x3c, 0x03,
    0x57, 0x59, 0x13, 0x1d, 0x56, 0x5a, 0x23, 0x2d, 0x44, 0x4c, 0x55, 0x5b, 0x33, 0x3d, 0x68, 0x02, 0x67, 0x69, 0x12, 0x1e,
    0x66, 0x6a, 0x22, 0x2e, 0x54, 0x5c, 0x43, 0x4d, 0x65, 0x6b, 0x32, 0x3e, 0x78, 0x01, 0x77, 0x79, 0x53, 0x5d, 0x11, 0x1f,
    0x64, 0x6c, 0x42, 0x4e, 0x76, 0x7a, 0x21, 0x2f, 0x75, 0x7b, 0x31, 0x3f, 0x63, 0x6d, 0x52, 0x5e, 0x00, 0x74, 0x7c, 0x41,
    0x4f, 0x10, 0x20, 0x62, 0x6e, 0x30, 0x73, 0x7d, 0x51, 0x5f, 0x40, 0x72, 0x7e, 0x61, 0x6f, 0x50, 0x71, 0x7f, 0x60, 0x70,
};

static int plane_dist(int xs, int code) {
    if (code > 120) return code - 120;
    int dc = code_to_plane[code - 1];
    int yo = dc >> 4, xo = 8 - (dc & 0xf);
    int d = yo * xs + xo;
    return d >= 1 ? d : 1;
}

#define DIVR(n, bits) (((n) + (1 << (bits)) - 1) >> (bits))

/* imagem codificada por entropia (principal com meta-códigos se level0) */
static bool decode_entropy(lbr_t *b, int xs, int ys, bool level0, u32 *out) {
    int cb = 0;
    if (lbr_get(b, 1)) { cb = (int)lbr_get(b, 4); if (cb < 1 || cb > 11) return false; }
    int hbits = 0, hw = 0, ngroups = 1;
    u32 *himg = NULL;
    bool ok = false;
    hgroup_t *g = NULL;
    u32 *cache = NULL;
    if (level0 && lbr_get(b, 1)) {
        hbits = (int)lbr_get(b, 3) + 2;
        hw = DIVR(xs, hbits);
        int hh = DIVR(ys, hbits);
        himg = kmalloc((usize)hw * hh * 4);
        if (!himg) return false;
        if (!decode_entropy(b, hw, hh, false, himg)) goto out;
        int mx = 0;
        for (int i = 0; i < hw * hh; i++) { himg[i] = (himg[i] >> 8) & 0xFFFF; if ((int)himg[i] > mx) mx = (int)himg[i]; }
        ngroups = mx + 1;
    }
    g = kcalloc(ngroups, sizeof(hgroup_t));
    if (!g) goto out;
    int sizes[5] = { 256 + 24 + (cb ? 1 << cb : 0), 256, 256, 256, 40 };
    for (int i = 0; i < ngroups; i++)
        for (int k = 0; k < 5; k++)
            if (!read_code(b, sizes[k], &g[i].h[k]) || b->eos) goto out;
    if (cb) { cache = kcalloc(1u << cb, 4); if (!cache) goto out; }
    int total = xs * ys, pos = 0, x = 0, y = 0;
    int csh = 32 - cb;
    hgroup_t *cur = &g[0];
    while (pos < total) {
        if (himg) cur = &g[himg[(y >> hbits) * hw + (x >> hbits)]];
        int s = lhuff_dec(b, &cur->h[0]);
        if (s < 0 || b->eos) goto out;
        if (s < 256) {
            int r = lhuff_dec(b, &cur->h[1]), bl = lhuff_dec(b, &cur->h[2]), a = lhuff_dec(b, &cur->h[3]);
            if (r < 0 || bl < 0 || a < 0) goto out;
            u32 c = (u32)a << 24 | (u32)r << 16 | (u32)s << 8 | (u32)bl;
            out[pos++] = c;
            if (cache) cache[(0x1e35a7bdu * c) >> csh] = c;
            if (++x == xs) { x = 0; y++; }
        } else if (s < 256 + 24) {
            int len = prefix_value(b, s - 256);
            int ds = lhuff_dec(b, &cur->h[4]);
            if (ds < 0) goto out;
            int dist = plane_dist(xs, prefix_value(b, ds));
            if (dist > pos || len > total - pos) goto out;
            for (int i = 0; i < len; i++) {
                u32 c = out[pos - dist];
                out[pos++] = c;
                if (cache) cache[(0x1e35a7bdu * c) >> csh] = c;
            }
            x += len;
            while (x >= xs) { x -= xs; y++; }
        } else {
            int ci = s - 280;
            if (!cache || ci >= (1 << cb)) goto out;
            u32 c = cache[ci];
            out[pos++] = c;
            cache[(0x1e35a7bdu * c) >> csh] = c;
            if (++x == xs) { x = 0; y++; }
        }
    }
    ok = !b->eos;
out:
    if (g) { for (int i = 0; i < ngroups; i++) for (int k = 0; k < 5; k++) lhuff_free(&g[i].h[k]); kfree(g); }
    if (cache) kfree(cache);
    if (himg) kfree(himg);
    return ok;
}

static inline u32 add_px(u32 a, u32 b) {
    return (((a & 0xFF00FF00u) + (b & 0xFF00FF00u)) & 0xFF00FF00u) | (((a & 0x00FF00FFu) + (b & 0x00FF00FFu)) & 0x00FF00FFu);
}
static inline u32 avg2(u32 a, u32 b) { return (((a ^ b) & 0xFEFEFEFEu) >> 1) + (a & b); }
static inline int ch(u32 c, int s) { return (int)((c >> s) & 0xFF); }
static inline int clamp255(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }

static u32 select_px(u32 L, u32 T, u32 TL) {
    int pl = 0, pt = 0;
    for (int s = 0; s < 32; s += 8) {
        int p = ch(L, s) + ch(T, s) - ch(TL, s);
        pl += p > ch(L, s) ? p - ch(L, s) : ch(L, s) - p;
        pt += p > ch(T, s) ? p - ch(T, s) : ch(T, s) - p;
    }
    return pl < pt ? L : T;
}
static u32 clamp_full(u32 a, u32 b, u32 c) {
    u32 r = 0;
    for (int s = 0; s < 32; s += 8) r |= (u32)clamp255(ch(a, s) + ch(b, s) - ch(c, s)) << s;
    return r;
}
static u32 clamp_half(u32 a, u32 b) {
    u32 r = 0;
    for (int s = 0; s < 32; s += 8) r |= (u32)clamp255(ch(a, s) + (ch(a, s) - ch(b, s)) / 2) << s;
    return r;
}

typedef struct { int type, bits, xs; u32 *data; } xform_t;

static void inv_predictor(const xform_t *t, u32 *img, int ys) {
    int W = t->xs, bw = DIVR(W, t->bits);
    for (int y = 0; y < ys; y++) {
        u32 *row = img + (usize)y * W;
        for (int x = 0; x < W; x++) {
            u32 *p = row + x, pred;
            if (y == 0) pred = x == 0 ? 0xFF000000u : p[-1];
            else if (x == 0) pred = p[-W];
            else {
                u32 L = p[-1], T = p[-W], TR = p[-W + 1], TL = p[-W - 1];
                switch ((t->data[(y >> t->bits) * bw + (x >> t->bits)] >> 8) & 0xF) {
                case 1: pred = L; break;
                case 2: pred = T; break;
                case 3: pred = TR; break;
                case 4: pred = TL; break;
                case 5: pred = avg2(avg2(L, TR), T); break;
                case 6: pred = avg2(L, TL); break;
                case 7: pred = avg2(L, T); break;
                case 8: pred = avg2(TL, T); break;
                case 9: pred = avg2(T, TR); break;
                case 10: pred = avg2(avg2(L, TL), avg2(T, TR)); break;
                case 11: pred = select_px(L, T, TL); break;
                case 12: pred = clamp_full(L, T, TL); break;
                case 13: pred = clamp_half(avg2(L, T), TL); break;
                default: pred = 0xFF000000u; break;
                }
            }
            *p = add_px(*p, pred);
        }
    }
}

static inline int ctdelta(int t, int c) { return ((int)(i8)t * (int)(i8)c) >> 5; }

static void inv_color(const xform_t *t, u32 *img, int ys) {
    int W = t->xs, bw = DIVR(W, t->bits);
    for (int y = 0; y < ys; y++)
        for (int x = 0; x < W; x++) {
            u32 m = t->data[(y >> t->bits) * bw + (x >> t->bits)];
            int g2r = m & 0xFF, g2b = (m >> 8) & 0xFF, r2b = (m >> 16) & 0xFF;
            u32 *p = img + (usize)y * W + x;
            int g = ch(*p, 8), r = ch(*p, 16), bl = ch(*p, 0);
            r = (r + ctdelta(g2r, g)) & 0xFF;
            bl = (bl + ctdelta(g2b, g)) & 0xFF;
            bl = (bl + ctdelta(r2b, r)) & 0xFF;
            *p = (*p & 0xFF00FF00u) | (u32)r << 16 | (u32)bl;
        }
}

/* desempacota índices de cor: entrada com largura DIVR(xs, bits), saída com largura xs */
static u32 *inv_index(const xform_t *t, u32 *img, int ys) {
    int W = t->xs, pw = DIVR(W, t->bits);
    u32 *o = kmalloc((usize)W * ys * 4);
    if (!o) return NULL;
    int bpp = 8 >> t->bits, mask = (1 << bpp) - 1;
    for (int y = 0; y < ys; y++)
        for (int x = 0; x < W; x++) {
            u32 pk = img[(usize)y * pw + (x >> t->bits)];
            int idx = (int)(((pk >> 8) & 0xFF) >> ((x & ((1 << t->bits) - 1)) * bpp)) & mask;
            o[(usize)y * W + x] = t->data[idx];
        }
    return o;
}

/* fluxo VP8L sem o cabeçalho de 5 bytes (o ALPH usa assim); devolve ARGB w×h */
static u32 *vp8l_stream(const u8 *d, usize n, int w, int h) {
    lbr_t b;
    lbr_init(&b, d, n);
    xform_t xf[4];
    int nx = 0, xs = w;
    u8 seen = 0;
    u32 *img = NULL;
    while (lbr_get(&b, 1)) {
        int t = (int)lbr_get(&b, 2);
        if (seen & (1 << t) || nx >= 4) goto fail;
        seen |= (u8)(1 << t);
        xform_t *T = &xf[nx++];
        T->type = t; T->xs = xs; T->data = NULL; T->bits = 0;
        if (t == 0 || t == 1) {
            T->bits = (int)lbr_get(&b, 3) + 2;
            int bw = DIVR(xs, T->bits), bh = DIVR(h, T->bits);
            T->data = kmalloc((usize)bw * bh * 4);
            if (!T->data || !decode_entropy(&b, bw, bh, false, T->data)) goto fail;
        } else if (t == 3) {
            int nc = (int)lbr_get(&b, 8) + 1;
            T->bits = nc > 16 ? 0 : nc > 4 ? 1 : nc > 2 ? 2 : 3;
            T->data = kcalloc(256, 4);
            if (!T->data || !decode_entropy(&b, nc, 1, false, T->data)) goto fail;
            for (int i = 1; i < nc; i++) T->data[i] = add_px(T->data[i], T->data[i - 1]);
            xs = DIVR(xs, T->bits);
        }
        if (b.eos) goto fail;
    }
    img = kmalloc((usize)xs * h * 4);
    if (!img || !decode_entropy(&b, xs, h, true, img)) goto fail;
    for (int i = nx - 1; i >= 0; i--) {
        xform_t *T = &xf[i];
        if (T->type == 0) inv_predictor(T, img, h);
        else if (T->type == 1) inv_color(T, img, h);
        else if (T->type == 2) {
            for (usize k = 0, e = (usize)T->xs * h; k < e; k++) {
                u32 c = img[k], g = (c >> 8) & 0xFF;
                img[k] = (c & 0xFF00FF00u) | ((((c >> 16) + g) & 0xFF) << 16) | (((c & 0xFF) + g) & 0xFF);
            }
        } else {
            u32 *o = inv_index(T, img, h);
            if (!o) goto fail;
            kfree(img);
            img = o;
        }
    }
    for (int i = 0; i < nx; i++) if (xf[i].data) kfree(xf[i].data);
    return img;
fail:
    for (int i = 0; i < nx; i++) if (xf[i].data) kfree(xf[i].data);
    if (img) kfree(img);
    return NULL;
}

static u32 *vp8l_decode(const u8 *d, usize n, int *ow, int *oh) {
    if (n < 5 || d[0] != 0x2F) return NULL;
    lbr_t b;
    lbr_init(&b, d + 1, 4);
    int w = (int)lbr_get(&b, 14) + 1, h = (int)lbr_get(&b, 14) + 1;
    lbr_get(&b, 1);                         /* alpha_is_used: só uma dica */
    if (lbr_get(&b, 3) != 0) return NULL;   /* versão */
    if ((u64)w * h > WEBP_MAX_PIXELS) return NULL;
    *ow = w; *oh = h;
    return vp8l_stream(d + 5, n - 5, w, h);
}

/* ================================================================ VP8 (com perdas) */

typedef struct { const u8 *p, *end; u32 value, range; int bits; } bd_t;

static void bd_init(bd_t *d, const u8 *p, usize n) {
    d->p = p; d->end = p + n;
    d->value = 0;
    for (int i = 0; i < 2; i++) d->value = (d->value << 8) | (d->p < d->end ? *d->p++ : 0);
    d->range = 255;
    d->bits = 0;
}
static inline int bd_bool(bd_t *d, int prob) {
    u32 split = 1 + (((d->range - 1) * (u32)prob) >> 8);
    u32 S = split << 8;
    int r;
    if (d->value >= S) { r = 1; d->range -= split; d->value -= S; }
    else { r = 0; d->range = split; }
    while (d->range < 128) {
        d->value <<= 1;
        d->range <<= 1;
        if (++d->bits == 8) { d->bits = 0; d->value |= d->p < d->end ? *d->p++ : 0; }
    }
    return r;
}
static u32 bd_lit(bd_t *d, int n) { u32 v = 0; while (n--) v = (v << 1) | (u32)bd_bool(d, 128); return v; }
static int bd_signed(bd_t *d, int n) { int v = (int)bd_lit(d, n); return bd_bool(d, 128) ? -v : v; }
static int bd_opt_signed(bd_t *d, int n) { return bd_bool(d, 128) ? bd_signed(d, n) : 0; }

enum { DC_PRED, V_PRED, H_PRED, TM_PRED, B_PRED };
enum { B_DC, B_TM, B_VE, B_HE, B_LD, B_RD, B_VR, B_VL, B_HD, B_HU };

static const u8 zigzag[16] = { 0, 1, 4, 8, 5, 2, 3, 6, 9, 12, 13, 10, 7, 11, 14, 15 };
static const u8 bands[17] = { 0, 1, 2, 3, 6, 4, 5, 6, 6, 6, 6, 6, 6, 6, 6, 7, 0 };
static const u8 pcat3[] = { 173, 148, 140, 0 };
static const u8 pcat4[] = { 176, 155, 140, 135, 0 };
static const u8 pcat5[] = { 180, 157, 141, 134, 130, 0 };
static const u8 pcat6[] = { 254, 254, 243, 230, 196, 177, 153, 140, 133, 130, 129, 0 };
static const u8 *const pcat3456[4] = { pcat3, pcat4, pcat5, pcat6 };

static const i8 bmode_tree[18] = {
    -B_DC, 2, -B_TM, 4, -B_VE, 6, 8, 12, -B_HE, 10, -B_RD, -B_VR, -B_LD, 14, -B_VL, 16, -B_HD, -B_HU,
};
static int bd_tree(bd_t *d, const i8 *tree, const u8 *probs) {
    int i = 0;
    while ((i = tree[i + bd_bool(d, probs[i >> 1])]) > 0) {}
    return -i;
}

typedef struct {
    int w, h, mbw, mbh;
    u8 *y, *u, *v;            /* planos sem filtro durante a decodificação */
    int ys, uvs;               /* passos */
    u8 probs[4 * 8 * 3 * 11];
    int seg_on, seg_map, seg_abs;
    int seg_q[4], seg_lf[4];
    u8 seg_p[3];
    int filter_type, lf_level, sharp;
    int lf_delta_on, ref_lf[4], mode_lf[4];
    int nparts;
    bd_t parts[8];
    int dq[4][6];              /* por segmento: y1dc y1ac y2dc y2ac uvdc uvac */
    int skip_on, skip_p;
    /* por macrobloco, para o filtro */
    u8 *mb_seg, *mb_i4, *mb_inner;
    /* contexto */
    u8 *a_nz, *a_nz_dc;        /* acima: 4 Y + 2 U + 2 V por mb, e Y2 */
    u8 *a_bm;                  /* modos 4x4 acima: 4 por mb */
} vp8_t;

static int get_large(bd_t *d, const u8 *p) {
    int v;
    if (!bd_bool(d, p[3])) {
        if (!bd_bool(d, p[4])) v = 2;
        else v = 3 + bd_bool(d, p[5]);
    } else if (!bd_bool(d, p[6])) {
        if (!bd_bool(d, p[7])) v = 5 + bd_bool(d, 159);
        else { v = 7 + 2 * bd_bool(d, 165); v += bd_bool(d, 145); }
    } else {
        int b1 = bd_bool(d, p[8]);
        int b0 = bd_bool(d, p[9 + b1]);
        int cat = 2 * b1 + b0;
        v = 0;
        for (const u8 *t = pcat3456[cat]; *t; t++) v += v + bd_bool(d, *t);
        v += 3 + (8 << cat);
    }
    return v;
}

/* coeficientes de um bloco; devolve a posição depois do último token (> first = bloco com dados) */
static int get_coeffs(bd_t *d, const u8 *probs, int type, int ctx, int dcq, int acq, int n, i16 *out) {
#define PR(pos, c) (probs + ((type * 8 + bands[pos]) * 3 + (c)) * 11)
    const u8 *p = PR(n, ctx);
    for (; n < 16; n++) {
        if (!bd_bool(d, p[0])) return n;
        while (!bd_bool(d, p[1])) {
            if (++n == 16) return 16;
            p = PR(n, 0);
        }
        int v;
        if (!bd_bool(d, p[2])) { v = 1; p = PR(n + 1, 1); }
        else { v = get_large(d, p); p = PR(n + 1, 2); }
        if (bd_bool(d, 128)) v = -v;
        out[zigzag[n]] = (i16)(v * (n > 0 ? acq : dcq));
    }
    return 16;
#undef PR
}

#define MUL1(a) ((((a) * 20091) >> 16) + (a))
#define MUL2(a) (((a) * 35468) >> 16)
static inline u8 clip8(int v) { return v < 0 ? 0 : v > 255 ? 255 : (u8)v; }

static void idct_add(const i16 *in, u8 *dst, int stride) {
    int C[16], *t = C;
    for (int i = 0; i < 4; i++, in++, t += 4) {
        int a = in[0] + in[8], b = in[0] - in[8];
        int c = MUL2(in[4]) - MUL1(in[12]), d = MUL1(in[4]) + MUL2(in[12]);
        t[0] = a + d; t[1] = b + c; t[2] = b - c; t[3] = a - d;
    }
    t = C;
    for (int i = 0; i < 4; i++, t++, dst += stride) {
        int dc = t[0] + 4;
        int a = dc + t[8], b = dc - t[8];
        int c = MUL2(t[4]) - MUL1(t[12]), d = MUL1(t[4]) + MUL2(t[12]);
        dst[0] = clip8(dst[0] + ((a + d) >> 3));
        dst[1] = clip8(dst[1] + ((b + c) >> 3));
        dst[2] = clip8(dst[2] + ((b - c) >> 3));
        dst[3] = clip8(dst[3] + ((a - d) >> 3));
    }
}

static void iwht(const i16 *in, i16 *out) {
    int t[16];
    for (int i = 0; i < 4; i++) {
        int a0 = in[0 + i] + in[12 + i], a1 = in[4 + i] + in[8 + i];
        int a2 = in[4 + i] - in[8 + i], a3 = in[0 + i] - in[12 + i];
        t[0 + i] = a0 + a1; t[8 + i] = a0 - a1; t[4 + i] = a3 + a2; t[12 + i] = a3 - a2;
    }
    for (int i = 0; i < 4; i++, out += 64) {
        int dc = t[0 + i * 4] + 3;
        int a0 = dc + t[3 + i * 4], a1 = t[1 + i * 4] + t[2 + i * 4];
        int a2 = t[1 + i * 4] - t[2 + i * 4], a3 = dc - t[3 + i * 4];
        out[0] = (i16)((a0 + a1) >> 3); out[16] = (i16)((a3 + a2) >> 3);
        out[32] = (i16)((a0 - a1) >> 3); out[48] = (i16)((a3 - a2) >> 3);
    }
}

/* área de trabalho com borda: linha -1 (canto, topo, 4 de cima à direita) e coluna -1 */
#define WB 32
#define AVG3(a, b, c) ((u8)(((a) + 2 * (b) + (c) + 2) >> 2))
#define AVG2(a, b) ((u8)(((a) + (b) + 1) >> 1))
#define DST(x, y) dst[(x) + (y) * WB]

static void pred_tm(u8 *dst, int n) {
    const u8 *top = dst - WB;
    int tl = top[-1];
    for (int y = 0; y < n; y++) {
        int l = dst[y * WB - 1];
        for (int x = 0; x < n; x++) dst[y * WB + x] = clip8(top[x] + l - tl);
    }
}
static void pred_fill(u8 *dst, int n, int v) { for (int y = 0; y < n; y++) memset(dst + y * WB, v, n); }
static void pred_big(u8 *dst, int n, int mode, bool has_top, bool has_left) {
    int sh = n == 16 ? 4 : 3;
    switch (mode) {
    case DC_PRED: {
        int s = 0;
        if (has_top && has_left) {
            for (int i = 0; i < n; i++) s += dst[i - WB] + dst[i * WB - 1];
            pred_fill(dst, n, (s + n) >> (sh + 1));
        } else if (has_top) {
            for (int i = 0; i < n; i++) s += dst[i - WB];
            pred_fill(dst, n, (s + (n >> 1)) >> sh);
        } else if (has_left) {
            for (int i = 0; i < n; i++) s += dst[i * WB - 1];
            pred_fill(dst, n, (s + (n >> 1)) >> sh);
        } else pred_fill(dst, n, 128);
        break;
    }
    case V_PRED: for (int y = 0; y < n; y++) memcpy(dst + y * WB, dst - WB, n); break;
    case H_PRED: for (int y = 0; y < n; y++) memset(dst + y * WB, dst[y * WB - 1], n); break;
    default: pred_tm(dst, n); break;
    }
}

static void pred4(u8 *dst, int mode) {
    const u8 *top = dst - WB;
    int X = top[-1], A = top[0], B = top[1], C = top[2], D = top[3], E = top[4], F = top[5], G = top[6], H = top[7];
    int I = dst[-1], J = dst[WB - 1], K = dst[2 * WB - 1], L = dst[3 * WB - 1];
    switch (mode) {
    case B_DC: {
        int s = 4;
        for (int i = 0; i < 4; i++) s += top[i] + dst[i * WB - 1];
        pred_fill(dst, 4, s >> 3);
        break;
    }
    case B_TM: pred_tm(dst, 4); break;
    case B_VE: {
        u8 v[4] = { AVG3(X, A, B), AVG3(A, B, C), AVG3(B, C, D), AVG3(C, D, E) };
        for (int y = 0; y < 4; y++) memcpy(dst + y * WB, v, 4);
        break;
    }
    case B_HE:
        memset(dst, AVG3(X, I, J), 4);
        memset(dst + WB, AVG3(I, J, K), 4);
        memset(dst + 2 * WB, AVG3(J, K, L), 4);
        memset(dst + 3 * WB, AVG3(K, L, L), 4);
        break;
    case B_RD:
        DST(0, 3) = AVG3(J, K, L);
        DST(1, 3) = DST(0, 2) = AVG3(I, J, K);
        DST(2, 3) = DST(1, 2) = DST(0, 1) = AVG3(X, I, J);
        DST(3, 3) = DST(2, 2) = DST(1, 1) = DST(0, 0) = AVG3(A, X, I);
        DST(3, 2) = DST(2, 1) = DST(1, 0) = AVG3(B, A, X);
        DST(3, 1) = DST(2, 0) = AVG3(C, B, A);
        DST(3, 0) = AVG3(D, C, B);
        break;
    case B_LD:
        DST(0, 0) = AVG3(A, B, C);
        DST(1, 0) = DST(0, 1) = AVG3(B, C, D);
        DST(2, 0) = DST(1, 1) = DST(0, 2) = AVG3(C, D, E);
        DST(3, 0) = DST(2, 1) = DST(1, 2) = DST(0, 3) = AVG3(D, E, F);
        DST(3, 1) = DST(2, 2) = DST(1, 3) = AVG3(E, F, G);
        DST(3, 2) = DST(2, 3) = AVG3(F, G, H);
        DST(3, 3) = AVG3(G, H, H);
        break;
    case B_VR:
        DST(0, 0) = DST(1, 2) = AVG2(X, A);
        DST(1, 0) = DST(2, 2) = AVG2(A, B);
        DST(2, 0) = DST(3, 2) = AVG2(B, C);
        DST(3, 0) = AVG2(C, D);
        DST(0, 3) = AVG3(K, J, I);
        DST(0, 2) = AVG3(J, I, X);
        DST(0, 1) = DST(1, 3) = AVG3(I, X, A);
        DST(1, 1) = DST(2, 3) = AVG3(X, A, B);
        DST(2, 1) = DST(3, 3) = AVG3(A, B, C);
        DST(3, 1) = AVG3(B, C, D);
        break;
    case B_VL:
        DST(0, 0) = AVG2(A, B);
        DST(1, 0) = DST(0, 2) = AVG2(B, C);
        DST(2, 0) = DST(1, 2) = AVG2(C, D);
        DST(3, 0) = DST(2, 2) = AVG2(D, E);
        DST(0, 1) = AVG3(A, B, C);
        DST(1, 1) = DST(0, 3) = AVG3(B, C, D);
        DST(2, 1) = DST(1, 3) = AVG3(C, D, E);
        DST(3, 1) = DST(2, 3) = AVG3(D, E, F);
        DST(3, 2) = AVG3(E, F, G);
        DST(3, 3) = AVG3(F, G, H);
        break;
    case B_HD:
        DST(0, 0) = DST(2, 1) = AVG2(I, X);
        DST(0, 1) = DST(2, 2) = AVG2(J, I);
        DST(0, 2) = DST(2, 3) = AVG2(K, J);
        DST(0, 3) = AVG2(L, K);
        DST(3, 0) = AVG3(A, B, C);
        DST(2, 0) = AVG3(X, A, B);
        DST(1, 0) = DST(3, 1) = AVG3(I, X, A);
        DST(1, 1) = DST(3, 2) = AVG3(J, I, X);
        DST(1, 2) = DST(3, 3) = AVG3(K, J, I);
        DST(1, 3) = AVG3(L, K, J);
        break;
    default: /* B_HU */
        DST(0, 0) = AVG2(I, J);
        DST(2, 0) = DST(0, 1) = AVG2(J, K);
        DST(2, 1) = DST(0, 2) = AVG2(K, L);
        DST(1, 0) = AVG3(I, J, K);
        DST(3, 0) = DST(1, 1) = AVG3(J, K, L);
        DST(3, 1) = DST(1, 2) = AVG3(K, L, L);
        DST(3, 2) = DST(2, 2) = DST(0, 3) = DST(1, 3) = DST(2, 3) = DST(3, 3) = (u8)L;
        break;
    }
}

/* ---------- filtro de laço */
static inline int sclip1(int v) { return v < -128 ? -128 : v > 127 ? 127 : v; }
static inline int sclip2(int v) { return v < -16 ? -16 : v > 15 ? 15 : v; }
static inline int iabs(int v) { return v < 0 ? -v : v; }

static inline void filt2(u8 *p, int s) {
    int p1 = p[-2 * s], p0 = p[-s], q0 = p[0], q1 = p[s];
    int a = 3 * (q0 - p0) + sclip1(p1 - q1);
    int a1 = sclip2((a + 4) >> 3), a2 = sclip2((a + 3) >> 3);
    p[-s] = clip8(p0 + a2);
    p[0] = clip8(q0 - a1);
}
static inline void filt4(u8 *p, int s) {
    int p1 = p[-2 * s], p0 = p[-s], q0 = p[0], q1 = p[s];
    int a = 3 * (q0 - p0);
    int a1 = sclip2((a + 4) >> 3), a2 = sclip2((a + 3) >> 3), a3 = (a1 + 1) >> 1;
    p[-2 * s] = clip8(p1 + a3);
    p[-s] = clip8(p0 + a2);
    p[0] = clip8(q0 - a1);
    p[s] = clip8(q1 - a3);
}
static inline void filt6(u8 *p, int s) {
    int p2 = p[-3 * s], p1 = p[-2 * s], p0 = p[-s], q0 = p[0], q1 = p[s], q2 = p[2 * s];
    int a = sclip1(3 * (q0 - p0) + sclip1(p1 - q1));
    int a1 = (27 * a + 63) >> 7, a2 = (18 * a + 63) >> 7, a3 = (9 * a + 63) >> 7;
    p[-3 * s] = clip8(p2 + a3);
    p[-2 * s] = clip8(p1 + a2);
    p[-s] = clip8(p0 + a1);
    p[0] = clip8(q0 - a1);
    p[s] = clip8(q1 - a2);
    p[2 * s] = clip8(q2 - a3);
}
static inline bool hev(const u8 *p, int s, int t) { return iabs(p[-2 * s] - p[-s]) > t || iabs(p[s] - p[0]) > t; }
static inline bool need1(const u8 *p, int s, int t) { return 4 * iabs(p[-s] - p[0]) + iabs(p[-2 * s] - p[s]) <= t; }
static inline bool need2(const u8 *p, int s, int t, int it) {
    int p3 = p[-4 * s], p2 = p[-3 * s], p1 = p[-2 * s], p0 = p[-s], q0 = p[0], q1 = p[s], q2 = p[2 * s], q3 = p[3 * s];
    if (4 * iabs(p0 - q0) + iabs(p1 - q1) > t) return false;
    return iabs(p3 - p2) <= it && iabs(p2 - p1) <= it && iabs(p1 - p0) <= it &&
           iabs(q3 - q2) <= it && iabs(q2 - q1) <= it && iabs(q1 - q0) <= it;
}
/* hs: passo através da borda; vs: passo ao longo dela */
static void simple_edge(u8 *p, int hs, int vs, int n, int thresh) {
    int t2 = 2 * thresh + 1;
    for (int i = 0; i < n; i++, p += vs) if (need1(p, hs, t2)) filt2(p, hs);
}
static void loop26(u8 *p, int hs, int vs, int n, int thresh, int it, int hevt) {
    int t2 = 2 * thresh + 1;
    for (int i = 0; i < n; i++, p += vs)
        if (need2(p, hs, t2, it)) { if (hev(p, hs, hevt)) filt2(p, hs); else filt6(p, hs); }
}
static void loop24(u8 *p, int hs, int vs, int n, int thresh, int it, int hevt) {
    int t2 = 2 * thresh + 1;
    for (int i = 0; i < n; i++, p += vs)
        if (need2(p, hs, t2, it)) { if (hev(p, hs, hevt)) filt2(p, hs); else filt4(p, hs); }
}

static void vp8_filter(vp8_t *V) {
    for (int my = 0; my < V->mbh; my++)
        for (int mx = 0; mx < V->mbw; mx++) {
            int i = my * V->mbw + mx, seg = V->mb_seg[i];
            int level = V->lf_level;
            if (V->seg_on) { level = V->seg_abs ? V->seg_lf[seg] : level + V->seg_lf[seg]; }
            if (V->lf_delta_on) {
                level += V->ref_lf[0];
                if (V->mb_i4[i]) level += V->mode_lf[0];
            }
            level = level < 0 ? 0 : level > 63 ? 63 : level;
            if (!level) continue;
            int il = level;
            if (V->sharp > 0) { il >>= V->sharp > 4 ? 2 : 1; if (il > 9 - V->sharp) il = 9 - V->sharp; }
            if (il < 1) il = 1;
            int limit = 2 * level + il;
            int hevt = level >= 40 ? 2 : level >= 15 ? 1 : 0;
            bool inner = V->mb_inner[i];
            u8 *y = V->y + (usize)my * 16 * V->ys + mx * 16;
            u8 *u = V->u + (usize)my * 8 * V->uvs + mx * 8, *v = V->v + (usize)my * 8 * V->uvs + mx * 8;
            int ys = V->ys, us = V->uvs;
            if (V->filter_type == 1) {
                if (mx > 0) simple_edge(y, 1, ys, 16, limit + 4);
                if (inner) for (int k = 4; k < 16; k += 4) simple_edge(y + k, 1, ys, 16, limit);
                if (my > 0) simple_edge(y, ys, 1, 16, limit + 4);
                if (inner) for (int k = 4; k < 16; k += 4) simple_edge(y + k * ys, ys, 1, 16, limit);
            } else {
                if (mx > 0) {
                    loop26(y, 1, ys, 16, limit + 4, il, hevt);
                    loop26(u, 1, us, 8, limit + 4, il, hevt);
                    loop26(v, 1, us, 8, limit + 4, il, hevt);
                }
                if (inner) {
                    for (int k = 4; k < 16; k += 4) loop24(y + k, 1, ys, 16, limit, il, hevt);
                    loop24(u + 4, 1, us, 8, limit, il, hevt);
                    loop24(v + 4, 1, us, 8, limit, il, hevt);
                }
                if (my > 0) {
                    loop26(y, ys, 1, 16, limit + 4, il, hevt);
                    loop26(u, us, 1, 8, limit + 4, il, hevt);
                    loop26(v, us, 1, 8, limit + 4, il, hevt);
                }
                if (inner) {
                    for (int k = 4; k < 16; k += 4) loop24(y + k * ys, ys, 1, 16, limit, il, hevt);
                    loop24(u + 4 * us, us, 1, 8, limit, il, hevt);
                    loop24(v + 4 * us, us, 1, 8, limit, il, hevt);
                }
            }
        }
}

static void vp8_free(vp8_t *V) {
    if (V->y) kfree(V->y);
    if (V->u) kfree(V->u);
    if (V->v) kfree(V->v);
    if (V->mb_seg) kfree(V->mb_seg);
    if (V->mb_i4) kfree(V->mb_i4);
    if (V->mb_inner) kfree(V->mb_inner);
    if (V->a_nz) kfree(V->a_nz);
    if (V->a_nz_dc) kfree(V->a_nz_dc);
    if (V->a_bm) kfree(V->a_bm);
}

/* carrega a borda da área de trabalho a partir dos planos (pixels sem filtro) */
static void load_border(u8 *ws, const u8 *plane, int stride, int mx, int my, int n, int mbw, bool topright) {
    u8 *dst = ws;
    const u8 *src = plane + (usize)my * n * stride + mx * n;
    if (my == 0) memset(dst - WB - 1, 127, n + 1 + (topright ? 4 : 0));
    else {
        dst[-WB - 1] = mx > 0 ? src[-stride - 1] : 129;
        memcpy(dst - WB, src - stride, n);
        if (topright) {
            if (mx < mbw - 1) memcpy(dst - WB + 16, src - stride + 16, 4);
            else memset(dst - WB + 16, src[-stride + 15], 4);
        }
    }
    for (int y = 0; y < n; y++) dst[y * WB - 1] = mx > 0 ? src[y * stride - 1] : 129;
    if (topright) for (int r = 3; r < 16; r += 4) memcpy(dst + r * WB + 16, dst - WB + 16, 4);
}
static void store_block(const u8 *ws, u8 *plane, int stride, int mx, int my, int n) {
    u8 *dst = plane + (usize)my * n * stride + mx * n;
    for (int y = 0; y < n; y++) memcpy(dst + (usize)y * stride, ws + y * WB, n);
}

/* decodifica um quadro-chave VP8; devolve os planos em V (o chamador converte) */
static bool vp8_decode(vp8_t *V, const u8 *d, usize n) {
    memset(V, 0, sizeof *V);
    if (n < 10) return false;
    u32 tag = le24(d);
    if (tag & 1) return false;                          /* só quadro-chave */
    u32 p0size = tag >> 5;
    if (d[3] != 0x9d || d[4] != 0x01 || d[5] != 0x2a) return false;
    V->w = (int)(le16(d + 6) & 0x3FFF);
    V->h = (int)(le16(d + 8) & 0x3FFF);
    if (!V->w || !V->h || (u64)V->w * V->h > WEBP_MAX_PIXELS) return false;
    d += 10; n -= 10;
    if (p0size > n) return false;
    V->mbw = (V->w + 15) >> 4; V->mbh = (V->h + 15) >> 4;
    V->ys = V->mbw * 16; V->uvs = V->mbw * 8;
    V->y = kcalloc((usize)V->ys * V->mbh * 16, 1);
    V->u = kcalloc((usize)V->uvs * V->mbh * 8, 1);
    V->v = kcalloc((usize)V->uvs * V->mbh * 8, 1);
    int nmb = V->mbw * V->mbh;
    V->mb_seg = kcalloc(nmb, 1); V->mb_i4 = kcalloc(nmb, 1); V->mb_inner = kcalloc(nmb, 1);
    V->a_nz = kcalloc(V->mbw, 8); V->a_nz_dc = kcalloc(V->mbw, 1); V->a_bm = kcalloc(V->mbw, 4);
    if (!V->y || !V->u || !V->v || !V->mb_seg || !V->mb_i4 || !V->mb_inner || !V->a_nz || !V->a_nz_dc || !V->a_bm)
        return false;
    bd_t hd;
    bd_init(&hd, d, p0size);
    bd_lit(&hd, 1);                                     /* espaço de cor */
    bd_lit(&hd, 1);                                     /* tipo de clamping */
    V->seg_on = bd_lit(&hd, 1);
    V->seg_p[0] = V->seg_p[1] = V->seg_p[2] = 255;
    if (V->seg_on) {
        V->seg_map = bd_lit(&hd, 1);
        if (bd_lit(&hd, 1)) {
            V->seg_abs = bd_lit(&hd, 1);
            for (int i = 0; i < 4; i++) V->seg_q[i] = bd_opt_signed(&hd, 7);
            for (int i = 0; i < 4; i++) V->seg_lf[i] = bd_opt_signed(&hd, 6);
        }
        if (V->seg_map) for (int i = 0; i < 3; i++) V->seg_p[i] = bd_lit(&hd, 1) ? (u8)bd_lit(&hd, 8) : 255;
    }
    V->filter_type = bd_lit(&hd, 1);
    V->lf_level = bd_lit(&hd, 6);
    V->sharp = bd_lit(&hd, 3);
    V->lf_delta_on = bd_lit(&hd, 1);
    if (V->lf_delta_on && bd_lit(&hd, 1)) {
        for (int i = 0; i < 4; i++) if (bd_lit(&hd, 1)) V->ref_lf[i] = bd_signed(&hd, 6);
        for (int i = 0; i < 4; i++) if (bd_lit(&hd, 1)) V->mode_lf[i] = bd_signed(&hd, 6);
    }
    V->nparts = 1 << bd_lit(&hd, 2);
    /* partições de coeficientes */
    const u8 *pp = d + p0size;
    usize rest = n - p0size;
    usize szs = 3 * (usize)(V->nparts - 1);
    if (rest < szs) return false;
    const u8 *pd = pp + szs;
    usize pleft = rest - szs;
    for (int i = 0; i < V->nparts; i++) {
        usize sz = i < V->nparts - 1 ? le24(pp + 3 * i) : pleft;
        if (sz > pleft) sz = pleft;
        bd_init(&V->parts[i], pd, sz);
        pd += sz; pleft -= sz;
    }
    /* quantizadores */
    int qi = bd_lit(&hd, 7);
    int dy1 = bd_opt_signed(&hd, 4), dy2 = bd_opt_signed(&hd, 4), ay2 = bd_opt_signed(&hd, 4);
    int duv = bd_opt_signed(&hd, 4), auv = bd_opt_signed(&hd, 4);
    for (int s = 0; s < 4; s++) {
        int q = qi;
        if (V->seg_on) q = V->seg_abs ? V->seg_q[s] : q + V->seg_q[s];
#define QC(x) ((x) < 0 ? 0 : (x) > 127 ? 127 : (x))
        V->dq[s][0] = vp8_dc_q[QC(q + dy1)];
        V->dq[s][1] = vp8_ac_q[QC(q)];
        V->dq[s][2] = vp8_dc_q[QC(q + dy2)] * 2;
        V->dq[s][3] = vp8_ac_q[QC(q + ay2)] * 155 / 100;
        if (V->dq[s][3] < 8) V->dq[s][3] = 8;
        V->dq[s][4] = vp8_dc_q[QC(q + duv)];
        if (V->dq[s][4] > 132) V->dq[s][4] = 132;
        V->dq[s][5] = vp8_ac_q[QC(q + auv)];
#undef QC
    }
    bd_lit(&hd, 1);                                     /* refresh_entropy_probs */
    memcpy(V->probs, vp8_default_coef_probs, sizeof V->probs);
    for (int i = 0; i < 4 * 8 * 3 * 11; i++)
        if (bd_bool(&hd, vp8_coef_update_probs[i])) V->probs[i] = (u8)bd_lit(&hd, 8);
    V->skip_on = bd_lit(&hd, 1);
    if (V->skip_on) V->skip_p = bd_lit(&hd, 8);

    u8 ws[(1 + 16) * WB + 8], wu[(1 + 8) * WB + 8], wv[(1 + 8) * WB + 8];
    u8 *wy = ws + WB + 4, *wuu = wu + WB + 4, *wvv = wv + WB + 4;
    i16 coef[25 * 16];
    for (int my = 0; my < V->mbh; my++) {
        bd_t *tp = &V->parts[my & (V->nparts - 1)];
        u8 l_nz[8] = { 0 }, l_nz_dc = 0, l_bm[4] = { B_DC, B_DC, B_DC, B_DC };
        for (int mx = 0; mx < V->mbw; mx++) {
            int mi = my * V->mbw + mx;
            int seg = 0;
            if (V->seg_map) seg = !bd_bool(&hd, V->seg_p[0]) ? bd_bool(&hd, V->seg_p[1]) : 2 + bd_bool(&hd, V->seg_p[2]);
            V->mb_seg[mi] = (u8)seg;
            int skip = V->skip_on ? bd_bool(&hd, V->skip_p) : 0;
            int ymode, bm[16];
            u8 *abm = V->a_bm + mx * 4;
            if (!bd_bool(&hd, 145)) {
                ymode = B_PRED;
                for (int by = 0; by < 4; by++)
                    for (int bx = 0; bx < 4; bx++) {
                        int A = by ? bm[(by - 1) * 4 + bx] : (my ? abm[bx] : B_DC);
                        int L = bx ? bm[by * 4 + bx - 1] : (mx ? l_bm[by] : B_DC);
                        bm[by * 4 + bx] = bd_tree(&hd, bmode_tree, vp8_kf_bmode_probs + (A * 10 + L) * 9);
                    }
                for (int k = 0; k < 4; k++) { abm[k] = (u8)bm[12 + k]; l_bm[k] = (u8)bm[k * 4 + 3]; }
            } else {
                ymode = !bd_bool(&hd, 156) ? (!bd_bool(&hd, 163) ? DC_PRED : V_PRED) : (!bd_bool(&hd, 128) ? H_PRED : TM_PRED);
                int im = ymode == DC_PRED ? B_DC : ymode == V_PRED ? B_VE : ymode == H_PRED ? B_HE : B_TM;
                for (int k = 0; k < 4; k++) abm[k] = l_bm[k] = (u8)im;
            }
            int uvmode = !bd_bool(&hd, 142) ? DC_PRED : !bd_bool(&hd, 114) ? V_PRED : !bd_bool(&hd, 183) ? H_PRED : TM_PRED;
            bool i4 = ymode == B_PRED;
            V->mb_i4[mi] = i4;
            /* resíduos */
            memset(coef, 0, sizeof coef);
            u8 *anz = V->a_nz + mx * 8;
            bool any = false;
            int *q = V->dq[seg];
            if (!skip) {
                int first = 0, ytype = 3;
                if (!i4) {
                    i16 dc[16] = { 0 };
                    int ctx = V->a_nz_dc[mx] + l_nz_dc;
                    int nz = get_coeffs(tp, V->probs, 1, ctx, q[2], q[3], 0, dc);
                    V->a_nz_dc[mx] = l_nz_dc = nz > 0;
                    if (nz > 1) iwht(dc, coef);
                    else { int dc0 = (dc[0] + 3) >> 3; for (int k = 0; k < 16; k++) coef[k * 16] = (i16)dc0; }
                    for (int k = 0; k < 16; k++) if (coef[k * 16]) any = true;
                    first = 1; ytype = 0;
                }
                for (int by = 0; by < 4; by++)
                    for (int bx = 0; bx < 4; bx++) {
                        int ctx = anz[bx] + l_nz[by];
                        int nz = get_coeffs(tp, V->probs, ytype, ctx, q[0], q[1], first, coef + (by * 4 + bx) * 16);
                        anz[bx] = l_nz[by] = nz > first;
                        if (nz > first) any = true;
                    }
                for (int c = 0; c < 2; c++)
                    for (int by = 0; by < 2; by++)
                        for (int bx = 0; bx < 2; bx++) {
                            int ctx = anz[4 + c * 2 + bx] + l_nz[4 + c * 2 + by];
                            int nz = get_coeffs(tp, V->probs, 2, ctx, q[4], q[5], 0, coef + (16 + c * 4 + by * 2 + bx) * 16);
                            anz[4 + c * 2 + bx] = l_nz[4 + c * 2 + by] = nz > 0;
                            if (nz > 0) any = true;
                        }
            } else {
                memset(anz, 0, 8);
                memset(l_nz, 0, 8);
                if (!i4) V->a_nz_dc[mx] = l_nz_dc = 0;
            }
            V->mb_inner[mi] = i4 || any;
            /* reconstrução: luma */
            load_border(wy, V->y, V->ys, mx, my, 16, V->mbw, true);
            if (i4) {
                for (int b = 0; b < 16; b++) {
                    u8 *dst = wy + (b >> 2) * 4 * WB + (b & 3) * 4;
                    pred4(dst, bm[b]);
                    idct_add(coef + b * 16, dst, WB);
                }
            } else {
                pred_big(wy, 16, ymode, my > 0, mx > 0);
                for (int b = 0; b < 16; b++) idct_add(coef + b * 16, wy + (b >> 2) * 4 * WB + (b & 3) * 4, WB);
            }
            store_block(wy, V->y, V->ys, mx, my, 16);
            /* croma */
            load_border(wuu, V->u, V->uvs, mx, my, 8, V->mbw, false);
            load_border(wvv, V->v, V->uvs, mx, my, 8, V->mbw, false);
            pred_big(wuu, 8, uvmode, my > 0, mx > 0);
            pred_big(wvv, 8, uvmode, my > 0, mx > 0);
            for (int b = 0; b < 4; b++) {
                idct_add(coef + (16 + b) * 16, wuu + (b >> 1) * 4 * WB + (b & 1) * 4, WB);
                idct_add(coef + (20 + b) * 16, wvv + (b >> 1) * 4 * WB + (b & 1) * 4, WB);
            }
            store_block(wuu, V->u, V->uvs, mx, my, 8);
            store_block(wvv, V->v, V->uvs, mx, my, 8);
        }
    }
    if (V->lf_level || V->seg_on) vp8_filter(V);
    return true;
}

static inline int mulhi(int v, int c) { return (v * c) >> 8; }
static inline u8 yclip(int v) { return (v & ~16383) == 0 ? (u8)(v >> 6) : v < 0 ? 0 : 255; }

/* YUV 4:2:0 → ARGB com reamostragem bilinear do croma (pesos 9/3/3/1) */
static u32 *vp8_to_argb(const vp8_t *V) {
    int w = V->w, h = V->h, cw = (w + 1) >> 1, chh = (h + 1) >> 1;
    u32 *o = kmalloc((usize)w * h * 4);
    if (!o) return NULL;
    for (int y = 0; y < h; y++) {
        int ny = y >> 1, my2 = (y & 1) ? ny + 1 : ny - 1;
        if (my2 < 0) my2 = 0;
        if (my2 > chh - 1) my2 = chh - 1;
        const u8 *Y = V->y + (usize)y * V->ys;
        const u8 *U0 = V->u + (usize)ny * V->uvs, *U1 = V->u + (usize)my2 * V->uvs;
        const u8 *V0 = V->v + (usize)ny * V->uvs, *V1 = V->v + (usize)my2 * V->uvs;
        for (int x = 0; x < w; x++) {
            int nx = x >> 1, mx2 = (x & 1) ? nx + 1 : nx - 1;
            if (mx2 < 0) mx2 = 0;
            if (mx2 > cw - 1) mx2 = cw - 1;
            int u = (9 * U0[nx] + 3 * U0[mx2] + 3 * U1[nx] + U1[mx2] + 8) >> 4;
            int v = (9 * V0[nx] + 3 * V0[mx2] + 3 * V1[nx] + V1[mx2] + 8) >> 4;
            int yy = mulhi(Y[x], 19077);
            u8 r = yclip(yy + mulhi(v, 26149) - 14234);
            u8 g = yclip(yy - mulhi(u, 6419) - mulhi(v, 13320) + 8708);
            u8 b = yclip(yy + mulhi(u, 33050) - 17685);
            o[(usize)y * w + x] = 0xFF000000u | (u32)r << 16 | (u32)g << 8 | b;
        }
    }
    return o;
}

/* ================================================================ ALPH */
static bool apply_alph(const u8 *d, usize n, u32 *argb, int w, int h) {
    if (n < 1) return false;
    int method = d[0] & 3, filter = (d[0] >> 2) & 3;
    usize np = (usize)w * h;
    u8 *a = kmalloc(np);
    if (!a) return false;
    if (method == 0) {
        if (n - 1 < np) { kfree(a); return false; }
        memcpy(a, d + 1, np);
    } else if (method == 1) {
        u32 *g = vp8l_stream(d + 1, n - 1, w, h);
        if (!g) { kfree(a); return false; }
        for (usize i = 0; i < np; i++) a[i] = (u8)(g[i] >> 8);
        kfree(g);
    } else { kfree(a); return false; }
    if (filter) {
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                u8 *p = a + (usize)y * w + x;
                int pred;
                if (x == 0 && y == 0) pred = 0;
                else if (y == 0) pred = p[-1];
                else if (x == 0) pred = p[-w];
                else if (filter == 1) pred = p[-1];
                else if (filter == 2) pred = p[-w];
                else pred = clamp255(p[-1] + p[-w] - p[-w - 1]);
                *p = (u8)(*p + pred);
            }
    }
    for (usize i = 0; i < np; i++) argb[i] = (argb[i] & 0xFFFFFF) | (u32)a[i] << 24;
    kfree(a);
    return true;
}

/* ================================================================ contêiner */
static u32 *decode_frame(const u8 *vp8, usize nvp8, bool lossless, const u8 *alph, usize nalph, int *w, int *h) {
    if (lossless) return vp8l_decode(vp8, nvp8, w, h);
    vp8_t V;
    u32 *o = NULL;
    if (vp8_decode(&V, vp8, nvp8)) {
        o = vp8_to_argb(&V);
        *w = V.w; *h = V.h;
        if (o && alph && !apply_alph(alph, nalph, o, V.w, V.h)) { /* alfa com defeito: imagem opaca, como o libwebp */ }
    }
    vp8_free(&V);
    return o;
}

/* procura VP8/VP8L (e ALPH antes dele) numa sequência de chunks */
static bool find_image(const u8 *p, const u8 *end, const u8 **img, usize *nimg, bool *lossless, const u8 **alph, usize *nalph) {
    *img = NULL; *alph = NULL;
    while (p + 8 <= end) {
        u32 sz = le32(p + 4);
        const u8 *data = p + 8;
        if (sz > (usize)(end - data)) sz = (u32)(end - data);
        if (!memcmp(p, "ALPH", 4)) { *alph = data; *nalph = sz; }
        else if (!memcmp(p, "VP8 ", 4) || !memcmp(p, "VP8L", 4)) {
            *img = data; *nimg = sz; *lossless = p[3] == 'L';
            return true;
        }
        p = data + sz + (sz & 1);
    }
    return false;
}

static surface_t *to_surface(const u32 *argb, int w, int h) {
    surface_t *s = surface_new(w, h);
    if (!s) return NULL;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            u32 c = argb[(usize)y * w + x], a = c >> 24;
            if (a == 255) s->px[(usize)y * s->stride + x] = c;
            else {
                u32 r = (((c >> 16) & 255) * a + 127) / 255, g = (((c >> 8) & 255) * a + 127) / 255, b = ((c & 255) * a + 127) / 255;
                s->px[(usize)y * s->stride + x] = a << 24 | r << 16 | g << 8 | b;
            }
        }
    return s;
}

/* ARGB não pré-multiplicado (para quem não usa surface_t, como o ImageIO); kfree no resultado */
u32 *webp_decode_argb(const u8 *d, usize n, int *ow, int *oh) {
    if (!d || n < 20 || memcmp(d, "RIFF", 4) || memcmp(d + 8, "WEBP", 4)) return NULL;
    usize riff = le32(d + 4) + 8ull;
    const u8 *end = d + (riff < n ? riff : n);
    const u8 *p = d + 12;
    if (p + 8 > end) return NULL;
    const u8 *img, *alph;
    usize nimg = 0, nalph = 0;
    bool lossless = false;
    int w = 0, h = 0;
    if (!memcmp(p, "VP8X", 4)) {
        if (le32(p + 4) < 10) return NULL;
        int cw = (int)le24(p + 12) + 1, chh = (int)le24(p + 15) + 1;
        if ((u64)cw * chh > WEBP_MAX_PIXELS) return NULL;
        bool anim = (p[8] & 0x02) != 0;
        const u8 *q = p + 8 + le32(p + 4);
        if (anim) {
            /* primeiro ANMF, composto numa tela transparente do tamanho do VP8X */
            while (q + 8 <= end) {
                u32 sz = le32(q + 4);
                const u8 *data = q + 8;
                if (sz > (usize)(end - data)) sz = (u32)(end - data);
                if (!memcmp(q, "ANMF", 4) && sz >= 16) {
                    int fx = 2 * (int)le24(data), fy = 2 * (int)le24(data + 3);
                    if (!find_image(data + 16, data + sz, &img, &nimg, &lossless, &alph, &nalph)) return NULL;
                    u32 *f = decode_frame(img, nimg, lossless, alph, nalph, &w, &h);
                    if (!f) return NULL;
                    u32 *c = kcalloc((usize)cw * chh, 4);
                    if (!c) { kfree(f); return NULL; }
                    for (int y = 0; y < h && fy + y < chh; y++)
                        for (int x = 0; x < w && fx + x < cw; x++) c[(usize)(fy + y) * cw + fx + x] = f[(usize)y * w + x];
                    kfree(f);
                    *ow = cw; *oh = chh;
                    return c;
                }
                q = data + sz + (sz & 1);
            }
            return NULL;
        }
        if (!find_image(q, end, &img, &nimg, &lossless, &alph, &nalph)) return NULL;
    } else if (!find_image(p, end, &img, &nimg, &lossless, &alph, &nalph)) return NULL;
    u32 *o = decode_frame(img, nimg, lossless, lossless ? NULL : alph, nalph, &w, &h);
    if (o) { *ow = w; *oh = h; }
    return o;
}

surface_t *webp_decode(const u8 *d, usize n) {
    int w, h;
    u32 *argb = webp_decode_argb(d, n, &w, &h);
    if (!argb) return NULL;
    surface_t *s = to_surface(argb, w, h);
    kfree(argb);
    return s;
}
