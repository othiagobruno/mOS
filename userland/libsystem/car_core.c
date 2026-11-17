/* mOS — leitor de Assets.car (ver car_core.h). Código original do mOS. */
#include "car_core.h"

static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
static uint16_t be16(const uint8_t *p) { return (uint16_t)(p[0] << 8 | p[1]); }
static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static double led(const uint8_t *p) { union { uint64_t u; double d; } x; x.u = (uint64_t)le32(p) | (uint64_t)le32(p + 4) << 32; return x.d; }
#define FCC(a, b, c, d) ((uint32_t)(a) << 24 | (uint32_t)(b) << 16 | (uint32_t)(c) << 8 | (uint32_t)(d))

/* ---- BOM */
static int block(const car_file *c, uint32_t i, const uint8_t **p, long *len) {
    if (i >= c->nblocks) return 0;
    uint32_t a = be32(c->idx + 4 + 8 * i), l = be32(c->idx + 8 + 8 * i);
    if ((long)a + (long)l > c->n) return 0;
    *p = c->d + a; *len = l;
    return 1;
}
static int var_named(const car_file *c, const uint8_t *vars, long vlen, const char *name, uint32_t *out) {
    if (vlen < 4) return 0;
    uint32_t n = be32(vars);
    long p = 4;
    for (uint32_t i = 0; i < n && p + 5 <= vlen; i++) {
        uint32_t idx = be32(vars + p);
        int l = vars[p + 4];
        if (p + 5 + l > vlen) return 0;
        int k = 0;
        while (name[k] && k < l && name[k] == (char)vars[p + 5 + k]) k++;
        if (!name[k] && k == l) { *out = idx; return 1; }
        p += 5 + l;
    }
    (void)c;
    return 0;
}
/* percorre as folhas da árvore BOM da esquerda para a direita; fn devolve 1 para parar */
typedef int (*leaf_fn)(void *ctx, const uint8_t *key, long klen, const uint8_t *val, long vlen);
static int walk(const car_file *c, uint32_t tree, leaf_fn fn, void *ctx) {
    const uint8_t *t; long tl;
    if (!block(c, tree, &t, &tl) || tl < 20 || be32(t) != FCC('t', 'r', 'e', 'e')) return 0;
    uint32_t node = be32(t + 8);
    for (int depth = 0; depth < 32; depth++) {   /* desce pelo primeiro filho até a folha */
        const uint8_t *b; long bl;
        if (!block(c, node, &b, &bl) || bl < 12) return 0;
        if (be16(b)) break;
        if (be16(b + 2) == 0 || bl < 20) return 0;
        node = be32(b + 12);
    }
    for (int guard = 0; node && guard < 1 << 20; guard++) {
        const uint8_t *b; long bl;
        if (!block(c, node, &b, &bl) || bl < 12) return 0;
        uint16_t cnt = be16(b + 2);
        for (uint16_t i = 0; i < cnt && 12 + 8 * (long)i + 8 <= bl; i++) {
            const uint8_t *k, *v; long kl, vl;
            if (!block(c, be32(b + 16 + 8 * i), &k, &kl) || !block(c, be32(b + 12 + 8 * i), &v, &vl)) continue;
            if (fn(ctx, k, kl, v, vl)) return 1;
        }
        node = be32(b + 4);
    }
    return 0;
}

int car_open(car_file *c, const uint8_t *d, long n) {
    for (unsigned i = 0; i < sizeof *c; i++) ((uint8_t *)c)[i] = 0;
    if (!d || n < 32 || d[0] != 'B' || d[1] != 'O' || d[2] != 'M' || d[3] != 'S') return 0;
    c->d = d; c->n = n;
    uint32_t ioff = be32(d + 16), voff = be32(d + 24), vlen = be32(d + 28);
    if ((long)ioff + 4 > n || (long)voff + (long)vlen > n) return 0;
    c->idx = d + ioff;
    c->nblocks = be32(c->idx);
    if ((long)ioff + 4 + 8 * (long)c->nblocks > n) return 0;
    uint32_t kf;
    if (!var_named(c, d + voff, vlen, "FACETKEYS", &c->facets_tree) || !var_named(c, d + voff, vlen, "RENDITIONS", &c->renditions_tree)) return 0;
    if (var_named(c, d + voff, vlen, "KEYFORMAT", &kf)) {
        const uint8_t *k; long kl;
        if (block(c, kf, &k, &kl) && kl >= 12) {
            uint32_t cnt = le32(k + 8);
            for (uint32_t i = 0; i < cnt && i < 32 && 12 + 4 * (long)i + 4 <= kl; i++) c->keyformat[c->nkeys++] = (uint16_t)le32(k + 12 + 4 * i);
        }
    }
    return c->nkeys > 0;
}

/* ---- FACETKEYS: nome → pares (atributo, valor) */
typedef struct { const char *name; car_attr attrs[16]; int n; int found; int want_i, at; char *out; int cap; int len; } facet_ctx;
static int facet_cb(void *x, const uint8_t *k, long kl, const uint8_t *v, long vl) {
    facet_ctx *f = x;
    if (f->out) {   /* car_name_at */
        if (f->at++ != f->want_i) return 0;
        int n = kl < f->cap - 1 ? (int)kl : f->cap - 1;
        for (int i = 0; i < n; i++) f->out[i] = (char)k[i];
        f->out[n] = 0;
        f->len = n;
        return 1;
    }
    int i = 0;
    while (f->name[i] && i < kl && f->name[i] == (char)k[i]) i++;
    if (f->name[i] || i != kl || vl < 6) return 0;
    int cnt = le16(v + 4);
    for (int j = 0; j < cnt && j < 16 && 6 + 4 * (long)j + 4 <= vl; j++) { f->attrs[j].attr = le16(v + 6 + 4 * j); f->attrs[j].value = le16(v + 8 + 4 * j); f->n++; }
    f->found = 1;
    return 1;
}
int car_name_at(const car_file *c, int i, char *out, int cap) {
    facet_ctx f = {0};
    f.out = out; f.cap = cap; f.want_i = i; f.len = -1;
    walk(c, c->facets_tree, facet_cb, &f);
    return f.len;
}

/* ---- CSI */
typedef struct {
    uint32_t pf; int w, h, scale, layout, cs;
    const uint8_t *tlv; long tlv_len; const uint8_t *data; long data_len;
    const uint8_t *name;
} csi;
static int parse_csi(const uint8_t *v, long vl, csi *s) {
    if (vl < 184 || v[0] != 'I' || v[1] != 'S' || v[2] != 'T' || v[3] != 'C') return 0;
    s->w = (int)le32(v + 12); s->h = (int)le32(v + 16); s->scale = (int)le32(v + 20); s->pf = le32(v + 24); s->cs = (int)(le32(v + 28) & 15);
    s->layout = le16(v + 36);
    s->name = v + 40;
    long tl = le32(v + 168), rl = le32(v + 180);
    if (184 + tl > vl) return 0;
    s->tlv = v + 184; s->tlv_len = tl;
    s->data = v + 184 + tl; s->data_len = 184 + tl + rl <= vl ? rl : vl - 184 - tl;
    return 1;
}
static const uint8_t *tlv_find(const csi *s, uint32_t tag, long *len) {
    for (long p = 0; p + 8 <= s->tlv_len;) {
        uint32_t t = le32(s->tlv + p), l = le32(s->tlv + p + 4);
        if (p + 8 + l > s->tlv_len) return 0;
        if (t == tag) { *len = l; return s->tlv + p + 8; }
        p += 8 + l;
    }
    return 0;
}
static uint16_t key_attr(const car_file *c, const uint8_t *k, long kl, uint16_t attr) {
    for (int i = 0; i < c->nkeys && 2 * (long)i + 2 <= kl; i++) if (c->keyformat[i] == attr) return le16(k + 2 * i);
    return 0;
}
static int key_has(const car_file *c, uint16_t attr) { for (int i = 0; i < c->nkeys; i++) if (c->keyformat[i] == attr) return 1; return 0; }

/* ---- busca */
typedef struct {
    const car_file *c; const facet_ctx *f; int want_scale, dark;
    long best_score; const uint8_t *best_k; long best_kl; const uint8_t *best_v; long best_vl;
} find_ctx;
static int find_cb(void *x, const uint8_t *k, long kl, const uint8_t *v, long vl) {
    find_ctx *q = x;
    for (int i = 0; i < q->f->n; i++)
        if (key_has(q->c, q->f->attrs[i].attr) && key_attr(q->c, k, kl, q->f->attrs[i].attr) != q->f->attrs[i].value) return 0;
    csi s;
    if (!parse_csi(v, vl, &s)) return 0;
    long score = 0;
    /* aparência: 0 = qualquer; com escuro pedido, a não-zero ganha */
    uint16_t app = key_attr(q->c, k, kl, 7);
    if (q->dark) score += app ? 400000 : 200000; else score += app ? 0 : 400000;
    /* escala: a pedida, senão a maior */
    int sc = s.scale ? s.scale : key_attr(q->c, k, kl, 12) * 100;
    if (q->want_scale && sc == q->want_scale) score += 100000;
    score += sc;
    /* gamut sRGB, imagem de verdade antes do índice multitamanho, e o maior tamanho */
    if (key_attr(q->c, k, kl, 24) == 0) score += 50000;
    if (s.layout == 1010) score -= 300000;
    score += s.w > 4096 ? 4096 : s.w;
    if (!q->best_v || score > q->best_score) { q->best_score = score; q->best_k = k; q->best_kl = kl; q->best_v = v; q->best_vl = vl; }
    return 0;
}
static int fill_rendition(const csi *s, car_rendition *out) {
    for (unsigned i = 0; i < sizeof *out; i++) ((uint8_t *)out)[i] = 0;
    out->width = s->w; out->height = s->h; out->scale = s->scale; out->layout = s->layout; out->pixel_format = s->pf;
    for (int i = 0; i < 128 && s->name[i]; i++) out->name[i] = (char)s->name[i];
    if (s->layout == 1009 && s->data_len >= 16 && le32(s->data) == FCC('C', 'O', 'L', 'R')) {
        int n = (int)le32(s->data + 12);
        if (n > 4) n = 4;
        for (int i = 0; i < n && 16 + 8 * (long)i + 8 <= s->data_len; i++) out->color[i] = led(s->data + 16 + 8 * i);
        out->ncolor = n;
        out->kind = CAR_COLOR;
        return 1;
    }
    if (s->data_len >= 12 && le32(s->data) == FCC('R', 'A', 'W', 'D')) {
        long l = le32(s->data + 8);
        if (12 + l > s->data_len) l = s->data_len - 12;
        out->data = s->data + 12; out->data_len = l; out->kind = CAR_DATA;
        return 1;
    }
    out->kind = s->w > 0 && s->h > 0 ? CAR_BITMAP : CAR_UNSUPPORTED;
    return 1;
}
int car_find(const car_file *c, const char *name, int want_scale, int dark, car_rendition *out, uint16_t *key_out, int *nkey_out) {
    facet_ctx f = {0};
    f.name = name;
    walk(c, c->facets_tree, facet_cb, &f);
    if (!f.found) return 0;
    find_ctx q = {c, &f, want_scale, dark, 0, 0, 0, 0, 0};
    walk(c, c->renditions_tree, find_cb, &q);
    if (!q.best_v) return 0;
    csi s;
    if (!parse_csi(q.best_v, q.best_vl, &s)) return 0;
    fill_rendition(&s, out);
    if (key_out && nkey_out) {
        int n = (int)(q.best_kl / 2);
        if (n > 32) n = 32;
        for (int i = 0; i < n; i++) key_out[i] = le16(q.best_k + 2 * i);
        *nkey_out = n;
    }
    return 1;
}

/* ---- pixels */
typedef struct { const uint16_t *key; int nkey; const uint8_t *v; long vl; } bykey_ctx;
static int bykey_cb(void *x, const uint8_t *k, long kl, const uint8_t *v, long vl) {
    bykey_ctx *q = x;
    if (kl != 2 * q->nkey) return 0;
    for (int i = 0; i < q->nkey; i++) if (le16(k + 2 * i) != q->key[i]) return 0;
    q->v = v; q->vl = vl;
    return 1;
}
static int csi_by_key(const car_file *c, const uint16_t *key, int nkey, csi *s) {
    bykey_ctx q = {key, nkey, 0, 0};
    walk(c, c->renditions_tree, bykey_cb, &q);
    return q.v && parse_csi(q.v, q.vl, s);
}
/* ---- deepmap2 (compressão 11): o formato seguiu a descrição pública do decodificador carfile-go (licença MIT,
 * github.com/devcxm/carfile-go); a implementação aqui é própria.
 * Contêiner: u32 versão (1), u32 codificação, u64 tamanho, "dmp2", método, versão (1), -, formato de pixel, u16 largura,
 * u16 altura (do 1º segmento), u32 descritor (tamanho comprimido do 1º segmento ou, na paleta, contagem | componentes<<16).
 * Método 1 cru; 2 padrão (por segmento: alfa, um preditor por linha, plano alto e plano baixo dos resíduos em YCoCg com o
 * sinal no bit 0); 3 sem perdas (os pixels comprimidos); 4 paleta (entradas, u32 tamanho e índices comprimidos).
 * Saída como as outras renditions: BGRA pré-multiplicado (4 componentes) ou cinza+alfa (2). */
static int dm2_layout(int pf, int *comps, int *has_alpha) {
    switch (pf) {
    case 1: *comps = 1; *has_alpha = 0; return 1;
    case 2: *comps = 2; *has_alpha = 1; return 1;
    case 3: *comps = 3; *has_alpha = 0; return 1;
    case 4: *comps = 4; *has_alpha = 1; return 1;
    }
    return 0;   /* 20 (componentes de 16 bits) não é lido */
}
static int dm2_abs(int v) { return v < 0 ? -v : v; }
static void dm2_unpredict(int16_t *row, const int16_t *prev, int n3, int pred) {
    switch (pred) {
    case 1:   /* de cima ou da esquerda, o que variar menos na luminância */
        for (int c = 0; c < 3; c++) row[c] = (int16_t)(row[c] + prev[c]);
        for (int i = 3; i < n3; i += 3) {
            const int16_t *src = dm2_abs(prev[i] - prev[i - 3]) > dm2_abs(row[i - 3] - prev[i - 3]) ? prev + i : row + i - 3;
            int16_t p0 = src[0], p1 = src[1], p2 = src[2];
            row[i] = (int16_t)(row[i] + p0); row[i + 1] = (int16_t)(row[i + 1] + p1); row[i + 2] = (int16_t)(row[i + 2] + p2);
        }
        break;
    case 2: for (int i = 3; i < n3; i++) row[i] = (int16_t)(row[i] + row[i - 3]); break;   /* esquerda */
    case 3: for (int i = 0; i < n3; i++) row[i] = (int16_t)(row[i] + prev[i]); break;      /* cima */
    case 4:   /* média de esquerda e cima, arredondada para cima e simétrica nos negativos */
        for (int c = 0; c < 3; c++) row[c] = (int16_t)(row[c] + prev[c]);
        for (int i = 3; i < n3; i++) {
            int32_t sum = (int32_t)row[i - 3] + prev[i] + 1;
            if (sum < 0) sum++;
            row[i] = (int16_t)(row[i] + (sum >> 1));
        }
        break;
    }
}
static uint8_t dm2_clip(int v) { return (uint8_t)(v & 255); }
/* um segmento padrão já descomprimido (rows linhas) → pixels de saída a partir da linha y0 */
static int dm2_default_segment(const uint8_t *src, long len, int w, int rows, int comps, int has_alpha, int16_t *prev, int16_t *cur,
                               uint8_t *out, long rowbytes, int y0) {
    long npx = (long)w * rows;
    int rc = has_alpha ? comps - 1 : comps;
    long abytes = has_alpha ? npx : 0, need = abytes + rows + 2 * rc * npx;
    if (len < need) return 0;
    const uint8_t *alpha = src, *pred = src + abytes, *hi = pred + rows, *lo = hi + rc * npx;
    for (int y = 0; y < rows; y++) {
        for (int i = 0; i < 3 * w; i++) cur[i] = 0;
        for (int x = 0; x < w; x++)
            for (int k = 0; k < rc; k++) {
                long idx = ((long)y * w + x) * rc + k;
                uint16_t e = (uint16_t)(lo[idx] | hi[idx] << 8);
                int16_t v = (int16_t)(e >> 1);
                cur[x * 3 + k] = (e & 1) ? (int16_t)-v : v;
            }
        dm2_unpredict(cur, prev, 3 * w, pred[y]);
        uint8_t *o = out + (long)(y0 + y) * rowbytes;
        for (int x = 0; x < w; x++) {
            uint8_t a = has_alpha ? alpha[(long)y * w + x] : 255;
            if (rc == 1) { o[x * comps] = dm2_clip(cur[x * 3]); if (has_alpha) o[x * comps + 1] = a; continue; }
            int yy = cur[x * 3], co = (int16_t)((uint16_t)cur[x * 3 + 1] << 1), cg = (int16_t)((uint16_t)cur[x * 3 + 2] << 1);
            int t = yy - cg / 2, g = t + cg, b = t - co / 2, r = b + co;
            o[x * comps + 0] = dm2_clip(b); o[x * comps + 1] = dm2_clip(g); o[x * comps + 2] = dm2_clip(r);
            if (comps == 4) o[x * comps + 3] = a;
        }
        int16_t *t = prev; prev = cur; cur = t;
    }
    /* a última linha decodificada fica em prev para o segmento seguinte */
    if (rows & 1) for (int i = 0; i < 3 * w; i++) { int16_t t = prev[i]; prev[i] = cur[i]; cur[i] = t; }
    return 1;
}
static int dm2_magic(const uint8_t *p, long n) { return n >= 4 && p[0] == 'b' && p[1] == 'v' && p[2] == 'x'; }
static long dm2_inflate(car_decompress_fn dec, const uint8_t *p, long n, uint8_t *out, long cap) {
    if (!dec) return 0;
    return dec(dm2_magic(p, n) ? 4 : 3, p, n, out, cap);
}
/* um contêiner dmp2: pixels a partir da linha *y em out; devolve 1 e avança *y */
static int dm2_container(const uint8_t *p, long n, car_decompress_fn dec, uint8_t *out, long rowbytes, long outcap, int want_w,
                         int want_comps, int *y, uint8_t *work, long workcap) {
    if (n < 32 || le32(p) != 1) return 0;
    long clen = (long)(le32(p + 8));
    if (clen > n - 16) clen = n - 16;
    long end = 16 + clen;
    if (end < 28 || p[16] != 'd' || p[17] != 'm' || p[18] != 'p' || p[19] != '2' || p[21] != 1) return 0;
    int method = p[20], comps, has_alpha;
    if (!dm2_layout(p[23], &comps, &has_alpha) || comps != want_comps) return 0;
    int w = le16(p + 24), h = le16(p + 26);
    if (w != want_w || h <= 0) return 0;
    long bpp = comps;
    if (method == 1) {
        if (end - 28 < (long)w * h * bpp || (long)(*y + h) * rowbytes > outcap) return 0;
        for (int yy = 0; yy < h; yy++) for (long i = 0; i < w * bpp; i++) out[(long)(*y + yy) * rowbytes + i] = p[28 + (long)yy * w * bpp + i];
        *y += h;
        return 1;
    }
    if (end < 32) return 0;
    uint32_t desc = le32(p + 28);
    const uint8_t *q = p + 32, *qe = p + end;
    if (method == 2 || method == 3) {
        int rc = has_alpha ? comps - 1 : comps;
        long row_in = (has_alpha ? w : 0) + 1 + 2L * rc * w;
        long ints = 3L * w * 2;   /* duas linhas de int16 */
        if (workcap < ints * 2 + 64) return 0;
        int16_t *prev = (int16_t *)work, *cur = prev + 3 * w;
        uint8_t *seg = work + ints * 2;
        long segcap = workcap - ints * 2;
        for (int i = 0; i < 3 * w; i++) prev[i] = 0;
        long clen1 = (long)desc;
        /* LZVN cru não diz o tamanho: o esperado é o de um segmento da altura do cabeçalho (alinhado a 16) */
        long expect = method == 2 ? ((has_alpha ? (long)w * h : 0) + h + 2L * rc * w * h + 15) & ~15L : (long)w * h * bpp;
        for (;;) {
            if (clen1 <= 0 || q + clen1 > qe) return 0;
            long got = dm2_inflate(dec, q, clen1, seg, dm2_magic(q, clen1) || expect > segcap ? segcap : expect);
            if (got <= 0) return 0;
            if (method == 2) {
                int rows = (int)(got / row_in);
                if (rows <= 0 || got - rows * row_in >= 16 || (long)(*y + rows) * rowbytes > outcap) return 0;
                if (!dm2_default_segment(seg, got, w, rows, comps, has_alpha, prev, cur, out, rowbytes, *y)) return 0;
                *y += rows;
            } else {
                long rb = (long)w * bpp;
                int rows = (int)(got / rb);
                if (rows <= 0 || (long)(*y + rows) * rowbytes > outcap) return 0;
                for (int yy = 0; yy < rows; yy++) for (long i = 0; i < rb; i++) out[(long)(*y + yy) * rowbytes + i] = seg[(long)yy * rb + i];
                *y += rows;
            }
            q += clen1;
            if (q >= qe) break;
            if (q + 4 > qe) return 0;
            clen1 = (long)le32(q);
            q += 4;
        }
        return 1;
    }
    if (method == 4) {   /* paleta: as primeiras pc componentes vêm da entrada; as outras em planos à parte */
        int count = (int)(desc & 0xFFFF), pc = (int)(desc >> 16);
        if (count <= 0 || count > 256 || pc <= 0 || pc > comps) return 0;
        long pbytes = (long)count * bpp;
        if (q + pbytes + 4 > qe) return 0;
        const uint8_t *pal = q;
        long elen = (long)le32(q + pbytes);
        const uint8_t *enc = q + pbytes + 4;
        if (enc + elen > qe) return 0;
        long npx = (long)w * h, sep = comps - pc, planes = npx * sep;
        if (workcap < planes + npx || (long)(*y + h) * rowbytes > outcap) return 0;
        long got = dm2_inflate(dec, enc, elen, work, planes + npx);
        if (got != planes + npx) return 0;
        const uint8_t *idx = work + planes;
        for (long i = 0; i < npx; i++) {
            if (idx[i] >= count) return 0;
            uint8_t *o = out + (long)(*y + i / w) * rowbytes + (i % w) * bpp;
            for (int k = 0; k < comps; k++) o[k] = pal[idx[i] * bpp + k];
            for (int k = 0; k < sep; k++) o[pc + k] = work[(long)k * npx + i];
        }
        *y += h;
        return 1;
    }
    return 0;
}
/* pixels de uma rendition deepmap2 (data depois do cabeçalho CELM; ver 1 = blocos KCBC) */
int car_deepmap2(const uint8_t *data, long n, int kcbc_blocks, car_decompress_fn dec, int w, int h, int comps, uint8_t *out,
                 long rowbytes, long outcap, uint8_t *work, long workcap) {
    int y = 0;
    if (kcbc_blocks > 0) {
        const uint8_t *p = data, *e = data + n;
        for (int i = 0; i < kcbc_blocks && p + 20 <= e; i++) {
            if (le32(p) != FCC('C', 'B', 'C', 'K')) return 0;
            long len = le32(p + 16);
            if (p + 20 + len > e) return 0;
            if (!dm2_container(p + 20, len, dec, out, rowbytes, outcap, w, comps, &y, work, workcap)) return 0;
            p += 20 + len;
        }
    } else if (!dm2_container(data, n, dec, out, rowbytes, outcap, w, comps, &y, work, workcap)) return 0;
    return y == h;
}

/* descomprime os pixels brutos (linhas de rowbytes) de uma rendition de bitmap; devolve rowbytes ou 0 */
static long raw_pixels(const csi *s, car_decompress_fn dec, uint8_t *raw, long cap) {
    long rb_len;
    const uint8_t *rb = tlv_find(s, 1007, &rb_len);
    int bpp = s->pf == FCC('G', 'A', '8', ' ') ? 2 : 4;
    long rowbytes = rb && rb_len >= 4 ? (long)le32(rb) : (long)s->w * bpp;
    if (rowbytes < (long)s->w * bpp) rowbytes = (long)s->w * bpp;
    long need = rowbytes * s->h;
    if (need > cap || s->data_len < 16 || le32(s->data) != FCC('C', 'E', 'L', 'M')) return 0;
    uint32_t ver = le32(s->data + 4), comp = le32(s->data + 8), f4 = le32(s->data + 12);
    const uint8_t *p = s->data + 16, *end = s->data + s->data_len;
    if (comp == 11) {   /* deepmap2: o resto do espaço de trabalho, depois dos pixels, serve de área de descompressão */
        long work_off = (need + 15) & ~15L;
        if (work_off >= cap) return 0;
        return car_deepmap2(p, end - p, ver == 1 ? (int)f4 : 0, dec, s->w, s->h, bpp, raw, rowbytes, need, raw + work_off, cap - work_off)
               ? rowbytes : 0;
    }
    if (comp >= 5 && comp != 4) return 0;   /* jpeg-lzfse, astc, paleta, hevc, deepmap antigo: não lidos aqui */
    long off = 0;
    if (ver == 1) {   /* blocos KCBC */
        for (uint32_t i = 0; i < f4 && p + 20 <= end; i++) {
            if (le32(p) != FCC('C', 'B', 'C', 'K')) return 0;
            long rows = le32(p + 12), len = le32(p + 16);
            p += 20;
            if (p + len > end) return 0;
            long want = rows * rowbytes;
            if (off + want > cap) return 0;
            long got = comp == 0 ? len : dec ? dec((int)comp, p, len, raw + off, cap - off) : 0;
            if (comp == 0) for (long k = 0; k < len && off + k < cap; k++) raw[off + k] = p[k];
            if (got <= 0) return 0;
            off += want;
            p += len;
        }
    } else {
        long len = (long)f4;
        if (p + len > end) len = end - p;
        long got = comp == 0 ? len : dec ? dec((int)comp, p, len, raw, cap) : 0;
        if (comp == 0) for (long k = 0; k < len && k < cap; k++) raw[k] = p[k];
        if (got <= 0) return 0;
    }
    return rowbytes;
}
/* pré-multiplicado (BGRA ou cinza+alfa) → ARGB direto */
static uint32_t unpremul(uint32_t a, uint32_t r, uint32_t g, uint32_t b) {
    if (a && a < 255) { r = r * 255 / a; g = g * 255 / a; b = b * 255 / a; if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255; }
    return a << 24 | r << 16 | g << 8 | b;
}
static void convert(const csi *s, const uint8_t *raw, long rowbytes, int x0, int y0, int w, int h, uint32_t *out) {
    int ga = s->pf == FCC('G', 'A', '8', ' ');
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            const uint8_t *p = raw + (long)(y0 + y) * rowbytes + (long)(x0 + x) * (ga ? 2 : 4);
            out[(long)y * w + x] = ga ? unpremul(p[1], p[0], p[0], p[0]) : unpremul(p[3], p[2], p[1], p[0]);
        }
}
int car_bitmap_argb(const car_file *c, const uint16_t *key, int nkey, car_decompress_fn dec, uint32_t *out, int w, int h);
static int bitmap_into(const car_file *c, const csi *s, car_decompress_fn dec, uint8_t *scratch, long cap, uint32_t *out, int w, int h) {
    long ll;
    const uint8_t *ln = tlv_find(s, 1010, &ll);
    if (ln && ll >= 30 && le32(ln) == FCC('I', 'N', 'L', 'K')) {   /* ligação para um pedaço de atlas */
        int x = (int)le32(ln + 8), y = (int)le32(ln + 12), lw = (int)le32(ln + 16), lh = (int)le32(ln + 20);
        long klen = le32(ln + 26);
        uint16_t akey[32];
        int nk = 0;
        /* a chave da ligação vem em pares (atributo, valor); a do atlas segue a ordem do KEYFORMAT */
        for (int i = 0; i < c->nkeys; i++) akey[i] = 0;
        for (long p = 30; p + 4 <= 30 + klen && p + 4 <= ll; p += 4) {
            uint16_t a = le16(ln + p), v = le16(ln + p + 2);
            if (!a && !v) break;
            for (int i = 0; i < c->nkeys; i++) if (c->keyformat[i] == a) akey[i] = v;
        }
        nk = c->nkeys;
        csi atlas;
        if (!csi_by_key(c, akey, nk, &atlas)) return 0;
        long rowbytes = raw_pixels(&atlas, dec, scratch, cap);
        if (!rowbytes || lw != w || lh != h || x + lw > atlas.w || y + lh > atlas.h) return 0;
        convert(&atlas, scratch, rowbytes, x, atlas.h - y - lh, w, h, out);   /* o y da ligação conta de baixo */
        return 1;
    }
    long rowbytes = raw_pixels(s, dec, scratch, cap);
    if (!rowbytes || s->w != w || s->h != h) return 0;
    convert(s, scratch, rowbytes, 0, 0, w, h, out);
    return 1;
}
/* out recebe w*h pixels; o espaço de trabalho fica no fim de out? não: usa o próprio out como bruto quando cabe */
static uint8_t *g_scratch;
static long g_scratch_cap;
void car_set_scratch(uint8_t *p, long cap) { g_scratch = p; g_scratch_cap = cap; }
int car_bitmap_argb(const car_file *c, const uint16_t *key, int nkey, car_decompress_fn dec, uint32_t *out, int w, int h) {
    csi s;
    if (!g_scratch || !csi_by_key(c, key, nkey, &s)) return 0;
    return bitmap_into(c, &s, dec, g_scratch, g_scratch_cap, out, w, h);
}

/* acha uma rendition pelo nome interno dela (ex.: "icon_16x16.png" de um AppIcon) */
typedef struct { const char *name; const uint8_t *k; long kl; const uint8_t *v; long vl; } csiname_ctx;
static int csiname_cb(void *x, const uint8_t *k, long kl, const uint8_t *v, long vl) {
    csiname_ctx *q = x;
    if (vl < 184) return 0;
    int i = 0;
    while (q->name[i] && i < 128 && (char)v[40 + i] == q->name[i]) i++;
    if (q->name[i] || (i < 128 && v[40 + i])) return 0;
    q->k = k; q->kl = kl; q->v = v; q->vl = vl;
    return 1;
}
int car_find_rendition_named(const car_file *c, const char *rname, car_rendition *out, uint16_t *key_out, int *nkey_out) {
    csiname_ctx q = {rname, 0, 0, 0, 0};
    walk(c, c->renditions_tree, csiname_cb, &q);
    csi s;
    if (!q.v || !parse_csi(q.v, q.vl, &s)) return 0;
    fill_rendition(&s, out);
    int n = (int)(q.kl / 2);
    if (n > 32) n = 32;
    for (int i = 0; i < n; i++) key_out[i] = le16(q.k + 2 * i);
    *nkey_out = n;
    return 1;
}
