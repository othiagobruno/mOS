/*
 * mOS ImageIO — TIFF (6.0). Incluído por imageio.c depois de iio_bmp.inc.
 * Leitura: todas as páginas da cadeia de IFDs; faixas ou tiles; chunky ou planar; compressão
 * nenhuma (1), LZW (5), Deflate (8/32946) e PackBits (32773); predictor horizontal (2) em 8 e 16 bits;
 * WhiteIsZero, BlackIsZero, RGB, paleta e CMYK; 1/2/4/8/16 bits inteiros e 32 bits float;
 * alfa associado (pré-multiplicado, desfeito aqui) e não associado; Orientation, X/YResolution, ICC (34675).
 * Gravação: RGB ou RGBA 8 bits chunky, uma faixa a cada ~64 KiB, nenhuma/LZW/Deflate/PackBits,
 * várias páginas, Orientation e resolução.
 */

typedef struct { const u8 *b; usize n; int le; } tif_t;
static u32 tf16(const tif_t *t, usize o) {
    if (o + 2 > t->n) return 0;
    return t->le ? (u32)(t->b[o] | t->b[o + 1] << 8) : (u32)(t->b[o] << 8 | t->b[o + 1]);
}
static u32 tf32(const tif_t *t, usize o) {
    if (o + 4 > t->n) return 0;
    return t->le ? (u32)t->b[o] | (u32)t->b[o + 1] << 8 | (u32)t->b[o + 2] << 16 | (u32)t->b[o + 3] << 24
                 : (u32)t->b[o] << 24 | (u32)t->b[o + 1] << 16 | (u32)t->b[o + 2] << 8 | (u32)t->b[o + 3];
}
static usize tf_tsize(u32 type) {
    switch (type) { case 1: case 2: case 6: case 7: return 1; case 3: case 8: return 2; case 4: case 9: case 11: return 4;
    case 5: case 10: case 12: return 8; default: return 0; }
}
typedef struct { u32 tag, type, count; usize voff; } tf_ent;
/* i-ésimo valor inteiro (ou racional como double) de uma entrada */
static double tf_val(const tif_t *t, const tf_ent *e, u32 i) {
    if (!e || i >= e->count) return 0;
    usize p = e->voff + (usize)i * tf_tsize(e->type);
    switch (e->type) {
    case 1: case 7: return p < t->n ? t->b[p] : 0;
    case 6: return p < t->n ? (i8)t->b[p] : 0;
    case 3: return tf16(t, p);
    case 8: return (i16)tf16(t, p);
    case 4: return tf32(t, p);
    case 9: return (i32)tf32(t, p);
    case 5: { u32 a = tf32(t, p), b = tf32(t, p + 4); return b ? (double)a / b : 0; }
    case 10: { i32 a = (i32)tf32(t, p), b = (i32)tf32(t, p + 4); return b ? (double)a / b : 0; }
    default: return 0;
    }
}

/* LZW do TIFF: MSB primeiro, 9..12 bits, troca de tamanho um código antes ("early change") */
static usize tf_lzw(const u8 *in, usize n, u8 *out, usize cap) {
    u16 prefix[4096];
    u8 suffix[4096], stack[4097];
    u32 acc = 0;
    int bits = 0, size = 9, next = 258, old = -1;
    usize ip = 0, op = 0;
    u8 first = 0;
    for (int i = 0; i < 256; i++) { prefix[i] = 0xFFFF; suffix[i] = (u8)i; }
    /* fluxo antigo (bit 0 primeiro, sem early change) começa com 0x00 0x01 */
    if (n >= 2 && in[0] == 0 && (in[1] & 1)) return 0;
    while (op < cap) {
        while (bits < size && ip < n) { acc = acc << 8 | in[ip++]; bits += 8; }
        if (bits < size) break;
        int code = (int)((acc >> (bits - size)) & ((1u << size) - 1));
        bits -= size;
        if (code == 256) { size = 9; next = 258; old = -1; continue; }
        if (code == 257) break;
        int sp = 0, c = code;
        if (old < 0) {
            if (code > 255) break;
            out[op++] = (u8)code;
            old = code;
            first = (u8)code;
            continue;
        }
        if (code > next) break;
        if (code == next) { stack[sp++] = first; c = old; }
        while (c > 255) { if (sp >= 4096) return op; stack[sp++] = suffix[c]; c = prefix[c]; }
        first = (u8)c;
        stack[sp++] = first;
        while (sp && op < cap) out[op++] = stack[--sp];
        if (next < 4096) { prefix[next] = (u16)old; suffix[next] = first; next++; }
        if (next + 1 >= (1 << size) && size < 12) size++;
        old = code;
    }
    return op;
}

static usize tf_packbits(const u8 *in, usize n, u8 *out, usize cap) {
    usize ip = 0, op = 0;
    while (ip < n && op < cap) {
        i8 c = (i8)in[ip++];
        if (c >= 0) {
            usize k = (usize)c + 1;
            if (ip + k > n) k = n - ip;
            if (op + k > cap) k = cap - op;
            memcpy(out + op, in + ip, k);
            ip += k; op += k;
        } else if (c != -128) {
            usize k = (usize)(1 - c);
            if (ip >= n) break;
            if (op + k > cap) k = cap - op;
            memset(out + op, in[ip++], k);
            op += k;
        }
    }
    return op;
}

typedef struct {
    int w, h, bps, spp, comp, photo, planar, pred, sfmt, extra, orient, resunit;
    double xres, yres;
    int tiled, tw, th;
    const tf_ent *offs, *cnts, *cmap;
    int has_icc;
    tf_ent icc;
} tf_ifd;

/* amostra s (0..spp-1) do pixel x de uma linha decodificada, normalizada para 0..255 */
static int tf_sample(const tf_ifd *I, const u8 *row, int x, int s, int le) {
    int idx = I->planar == 2 ? x : x * I->spp + s;
    switch (I->bps) {
    case 8: return row[idx];
    case 16: { int v = le ? row[idx * 2] | row[idx * 2 + 1] << 8 : row[idx * 2] << 8 | row[idx * 2 + 1]; return v >> 8; }
    case 32: {
        u32 v = le ? (u32)row[idx * 4] | (u32)row[idx * 4 + 1] << 8 | (u32)row[idx * 4 + 2] << 16 | (u32)row[idx * 4 + 3] << 24
                   : (u32)row[idx * 4] << 24 | (u32)row[idx * 4 + 1] << 16 | (u32)row[idx * 4 + 2] << 8 | (u32)row[idx * 4 + 3];
        if (I->sfmt == 3) { float f; memcpy(&f, &v, 4); if (!(f > 0)) return 0; if (f >= 1) return 255; return (int)(f * 255 + 0.5f); }
        return (int)(v >> 24);
    }
    default: {
        int bit = idx * I->bps, v = (row[bit >> 3] >> (8 - I->bps - (bit & 7))) & ((1 << I->bps) - 1);
        return I->photo == 3 ? v : v * 255 / ((1 << I->bps) - 1);
    }
    }
}

static void tf_unpredict(u8 *buf, usize rowbytes, int rows, int w, int spp, int bps, int le) {
    for (int y = 0; y < rows; y++) {
        u8 *r = buf + (usize)y * rowbytes;
        if (bps == 8) for (usize i = (usize)spp; i < (usize)w * spp; i++) r[i] = (u8)(r[i] + r[i - spp]);
        else if (bps == 16)
            for (usize i = (usize)spp; i < (usize)w * spp; i++) {
                u8 *a = r + i * 2, *b = r + (i - spp) * 2;
                int va = le ? a[0] | a[1] << 8 : a[0] << 8 | a[1], vb = le ? b[0] | b[1] << 8 : b[0] << 8 | b[1];
                int v = (va + vb) & 0xFFFF;
                if (le) { a[0] = (u8)v; a[1] = (u8)(v >> 8); } else { a[0] = (u8)(v >> 8); a[1] = (u8)v; }
            }
    }
}

/* decodifica um bloco (faixa ou tile) comprimido em out (cap bytes); devolve bytes produzidos */
static usize tf_block(const tif_t *t, const tf_ifd *I, usize off, usize len, u8 *out, usize cap) {
    if (off >= t->n) return 0;
    if (len > t->n - off) len = t->n - off;
    const u8 *src = t->b + off;
    switch (I->comp) {
    case 1: { usize k = len < cap ? len : cap; memcpy(out, src, k); return k; }
    case 5: return tf_lzw(src, len, out, cap);
    case 8: case 32946: { usize got = 0; if (!zlib_inflate(src, len, out, cap, &got)) return got; return got; }
    case 32773: return tf_packbits(src, len, out, cap);
    default: return 0;
    }
}

static u32 *tf_decode_ifd(const tif_t *t, tf_ifd *I) {
    int w = I->w, h = I->h, spp = I->spp, bps = I->bps;
    int bw = I->tiled ? I->tw : w, bh = I->tiled ? I->th : (I->th > 0 && I->th < h ? I->th : h);
    int across = (w + bw - 1) / bw, down = (h + bh - 1) / bh, planes = I->planar == 2 ? spp : 1;
    usize rowb = ((usize)bw * (I->planar == 2 ? 1 : spp) * bps + 7) / 8, blkb = rowb * bh;
    usize nblk = (usize)across * down * planes;
    if (!I->offs || I->offs->count < nblk) return NULL;
    /* imagem inteira descomprimida: planes × h × (linha de w) */
    usize frow = ((usize)w * (I->planar == 2 ? 1 : spp) * bps + 7) / 8;
    u8 *img = kcalloc(frow * h * planes + 16, 1);
    u8 *blk = kmalloc(blkb + 16);
    if (!img || !blk) { kfree(img); kfree(blk); return NULL; }
    for (usize b = 0; b < nblk; b++) {
        usize off = (usize)tf_val(t, I->offs, (u32)b), len = I->cnts && b < I->cnts->count ? (usize)tf_val(t, I->cnts, (u32)b) : t->n - off;
        memset(blk, 0, blkb);
        usize got = tf_block(t, I, off, len, blk, blkb);
        (void)got;
        int pl = (int)(b / ((usize)across * down)), bi = (int)(b % ((usize)across * down)), bx = bi % across, by = bi / across;
        if (I->pred == 2) tf_unpredict(blk, rowb, bh, bw, I->planar == 2 ? 1 : spp, bps, t->le);
        for (int y = 0; y < bh; y++) {
            int yy = by * bh + y;
            if (yy >= h) break;
            usize xb = ((usize)bx * bw * (I->planar == 2 ? 1 : spp) * bps) / 8;
            usize cp = rowb;
            if (xb + cp > frow) cp = frow - xb;
            memcpy(img + ((usize)pl * h + yy) * frow + xb, blk + (usize)y * rowb, cp);
        }
    }
    kfree(blk);
    u32 *px = kcalloc((usize)w * h, 4);
    if (!px) { kfree(img); return NULL; }
    int alpha_s = I->extra ? (I->photo == 5 ? 4 : (I->photo <= 1 ? 1 : 3)) : -1;
    if (alpha_s >= spp) alpha_s = -1;
    int assoc = I->extra == 1;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int v[5] = {0, 0, 0, 0, 255};
            for (int s = 0; s < spp && s < 5; s++) {
                const u8 *row = img + ((usize)(I->planar == 2 ? s : 0) * h + y) * frow;
                v[s] = tf_sample(I, row, x, I->planar == 2 ? 0 : s, t->le);
            }
            int r, g, b, a = alpha_s >= 0 ? v[alpha_s] : 255;
            switch (I->photo) {
            case 0: r = g = b = 255 - v[0]; break;
            case 1: r = g = b = v[0]; break;
            case 3: {
                int i = v[0], nc = I->cmap ? (int)I->cmap->count / 3 : 0;
                if (i < nc) { r = (int)tf_val(t, I->cmap, (u32)i) >> 8; g = (int)tf_val(t, I->cmap, (u32)(nc + i)) >> 8; b = (int)tf_val(t, I->cmap, (u32)(2 * nc + i)) >> 8; }
                else r = g = b = 0;
                break;
            }
            case 5: {
                int K = v[3];
                r = (255 - v[0]) * (255 - K) / 255; g = (255 - v[1]) * (255 - K) / 255; b = (255 - v[2]) * (255 - K) / 255;
                break;
            }
            default: r = v[0]; g = v[1]; b = v[2]; break;
            }
            if (assoc && a > 0 && a < 255) { r = MIN(255, r * 255 / a); g = MIN(255, g * 255 / a); b = MIN(255, b * 255 / a); }
            px[(usize)y * w + x] = RGBA(r, g, b, a);
        }
    }
    kfree(img);
    return px;
}

static int iio_tiff(const u8 *d, usize n, iio_image *out) {
    tif_t t = {d, n, 0};
    if (n < 8) return -1;
    if (d[0] == 'I' && d[1] == 'I') t.le = 1;
    else if (!(d[0] == 'M' && d[1] == 'M')) return -1;
    if (tf16(&t, 2) != 42) return -1;
    u32 ifd = tf32(&t, 4);
    int cap = 0, guard = 0;
    out->loop_count = -1;
    out->srgb_intent = -1;
    while (ifd && ifd + 2 <= n && guard++ < 1024) {
        u32 cnt = tf16(&t, ifd);
        if ((usize)ifd + 2 + (usize)cnt * 12 > n) break;
        tf_ent ents[64];
        int ne = 0;
        for (u32 i = 0; i < cnt && ne < 64; i++) {
            usize e = ifd + 2 + (usize)i * 12;
            tf_ent x = {tf16(&t, e), tf16(&t, e + 2), tf32(&t, e + 4), 0};
            usize sz = tf_tsize(x.type);
            if (!sz || x.count > 0x4000000u) continue;
            x.voff = sz * x.count <= 4 ? e + 8 : tf32(&t, e + 8);
            if (x.voff + sz * x.count > n) continue;
            ents[ne++] = x;
        }
        u32 nextifd = tf32(&t, ifd + 2 + (usize)cnt * 12);
        tf_ifd I = {0};
        I.bps = 1; I.spp = 1; I.comp = 1; I.photo = -1; I.planar = 1; I.pred = 1; I.sfmt = 1; I.orient = 1; I.resunit = 2;
        int subfile = 0;
        for (int i = 0; i < ne; i++) {
            const tf_ent *e = &ents[i];
            switch (e->tag) {
            case 254: subfile = (int)tf_val(&t, e, 0); break;
            case 256: I.w = (int)tf_val(&t, e, 0); break;
            case 257: I.h = (int)tf_val(&t, e, 0); break;
            case 258: I.bps = (int)tf_val(&t, e, 0); break;
            case 259: I.comp = (int)tf_val(&t, e, 0); break;
            case 262: I.photo = (int)tf_val(&t, e, 0); break;
            case 273: I.offs = e; break;
            case 274: I.orient = (int)tf_val(&t, e, 0); break;
            case 277: I.spp = (int)tf_val(&t, e, 0); break;
            case 278: I.th = (int)tf_val(&t, e, 0); break;
            case 279: I.cnts = e; break;
            case 282: I.xres = tf_val(&t, e, 0); break;
            case 283: I.yres = tf_val(&t, e, 0); break;
            case 284: I.planar = (int)tf_val(&t, e, 0); break;
            case 296: I.resunit = (int)tf_val(&t, e, 0); break;
            case 317: I.pred = (int)tf_val(&t, e, 0); break;
            case 320: I.cmap = e; break;
            case 322: I.tw = (int)tf_val(&t, e, 0); I.tiled = 1; break;
            case 323: I.th = (int)tf_val(&t, e, 0); break;
            case 324: I.offs = e; break;
            case 325: I.cnts = e; break;
            case 338: I.extra = (int)tf_val(&t, e, 0); if (!I.extra) I.extra = 3; break;  /* 3 = amostra extra sem alfa */
            case 339: I.sfmt = (int)tf_val(&t, e, 0); break;
            case 34675: I.icc = *e; I.has_icc = 1; break;
            }
        }
        if (I.extra == 3) I.extra = 0;
        if (I.photo < 0) I.photo = I.spp >= 3 ? 2 : 1;
        ifd = nextifd;
        if (subfile & 1) continue;                      /* miniatura reduzida: não é página */
        if (I.w <= 0 || I.h <= 0 || (u64)I.w * I.h > MAX_PIXELS || I.spp < 1 || I.spp > 5) continue;
        if (I.bps != 1 && I.bps != 2 && I.bps != 4 && I.bps != 8 && I.bps != 16 && I.bps != 32) continue;
        if (I.bps == 32 && I.sfmt != 3 && I.sfmt != 1) continue;
        if (I.photo != 0 && I.photo != 1 && I.photo != 2 && I.photo != 3 && I.photo != 5) continue;
        if (I.tiled && (I.tw <= 0 || I.th <= 0)) continue;
        u32 *px = tf_decode_ifd(&t, &I);
        if (!px) continue;
        if (out->nframes == cap) {
            int nc = cap ? cap * 2 : 4;
            iio_frame *nf = kcalloc((usize)nc, sizeof *nf);
            if (!nf) { kfree(px); break; }
            if (out->f) { memcpy(nf, out->f, (usize)out->nframes * sizeof *nf); kfree(out->f); }
            out->f = nf;
            cap = nc;
        }
        double k = I.resunit == 3 ? 2.54 : I.resunit == 2 ? 1 : 0;
        out->f[out->nframes++] = (iio_frame){px, I.w, I.h, 0, I.bps, I.photo == 5 ? IIO_CMYK : I.photo <= 1 ? IIO_GRAY : IIO_RGB,
                                             I.extra != 0, I.photo == 3, I.orient >= 1 && I.orient <= 8 ? I.orient : 1,
                                             I.xres * k, I.yres * k, I.comp, I.photo};
        if (I.has_icc && !out->icc && I.icc.count) {
            out->icc = kmalloc(I.icc.count);
            if (out->icc) { memcpy(out->icc, d + I.icc.voff, I.icc.count); out->icc_n = I.icc.count; }
        }
    }
    if (!out->nframes) { kfree(out->f); out->f = NULL; return -1; }
    out->w = out->f[0].w;
    out->h = out->f[0].h;
    out->uti = "public.tiff";
    return 0;
}

/* ------------------------------------------------------------------ gravação */
/* LZW do TIFF (gravação): MSB primeiro, early change, limpeza ao chegar a 4094 */
typedef struct { obuf_t *o; u32 acc; int bits; } tf_bw;
static void tfw_code(tf_bw *w, int code, int size) {
    w->acc = w->acc << size | (u32)code;
    w->bits += size;
    while (w->bits >= 8) { ob_u8(w->o, (int)(w->acc >> (w->bits - 8)) & 255); w->bits -= 8; }
}
static void tf_lzw_encode(obuf_t *o, const u8 *s, usize n) {
    enum { HS = 9973 };
    int *hc = kmalloc(HS * sizeof(int));
    u32 *hk = kmalloc(HS * sizeof(u32));
    tf_bw w = {o, 0, 0};
    int next = 258, size = 9;
    if (!hc || !hk) { o->err = 1; kfree(hc); kfree(hk); return; }
    for (int i = 0; i < HS; i++) hc[i] = -1;
    tfw_code(&w, 256, size);
    if (n) {
        int cur = s[0];
        for (usize i = 1; i < n; i++) {
            u32 key = (u32)cur << 8 | s[i], h = (key * 2654435761u) % HS;
            while (hc[h] >= 0 && hk[h] != key) h = (h + 1) % HS;
            if (hc[h] >= 0) { cur = hc[h]; continue; }
            tfw_code(&w, cur, size);
            hc[h] = next; hk[h] = key; next++;
            if (next >= 4094) {
                tfw_code(&w, 256, size);
                for (int k = 0; k < HS; k++) hc[k] = -1;
                next = 258; size = 9;
            } else if (next >= (1 << size) && size < 12) size++;   /* como a libtiff: troca quando next passa de 2^n - 1 */
            cur = s[i];
        }
        tfw_code(&w, cur, size);
        next++;
        if (next >= (1 << size) && size < 12) size++;
    }
    tfw_code(&w, 257, size);
    if (w.bits) ob_u8(o, (int)(w.acc << (8 - w.bits)) & 255);
    kfree(hc);
    kfree(hk);
}
static void tf_packbits_encode(obuf_t *o, const u8 *s, usize n) {
    usize i = 0;
    while (i < n) {
        usize r = 1;
        while (i + r < n && r < 128 && s[i + r] == s[i]) r++;
        if (r >= 2) { ob_u8(o, (int)(257 - r) & 255); ob_u8(o, s[i]); i += r; continue; }
        usize l = 1;
        while (i + l < n && l < 128 && !(i + l + 1 < n && s[i + l] == s[i + l + 1])) l++;
        ob_u8(o, (int)l - 1);
        ob_put(o, s + i, l);
        i += l;
    }
}
static int zlib_deflate_buf(const u8 *in, usize n, int level, u8 **out, usize *on);

static void ob_le32(obuf_t *o, u32 v) { ob_le16(o, (int)(v & 0xFFFF)); ob_le16(o, (int)(v >> 16)); }

static int iio_tiff_encode(const iio_enc_frame *fr, int nfr, const iio_enc_opts *op, u8 **out, usize *on) {
    obuf_t o = {0};
    int comp = op && op->tiff_compression ? op->tiff_compression : 1;
    if (comp != 1 && comp != 5 && comp != 8 && comp != 32773) comp = 1;
    ob_put(&o, "II", 2); ob_le16(&o, 42); ob_le32(&o, 0);
    usize link = 4;                                    /* onde gravar o ponteiro para o próximo IFD */
    for (int f = 0; f < nfr && !o.err; f++) {
        const iio_enc_frame *F = &fr[f];
        if (!F->px || F->w < 1 || F->h < 1) continue;
        int w = F->w, h = F->h, alpha = 0;
        for (usize i = 0; i < (usize)w * h; i++) if ((F->px[i] >> 24) != 255) { alpha = 1; break; }
        int spp = alpha ? 4 : 3;
        usize rowb = (usize)w * spp;
        int rps = (int)(65536 / rowb);
        if (rps < 1) rps = 1;
        if (rps > h) rps = h;
        int ns = (h + rps - 1) / rps;
        u32 *soff = kcalloc((usize)ns, 4), *scnt = kcalloc((usize)ns, 4);
        u8 *raw = kmalloc(rowb * rps);
        if (!soff || !scnt || !raw) { kfree(soff); kfree(scnt); kfree(raw); o.err = 1; break; }
        for (int s = 0; s < ns; s++) {
            int y0 = s * rps, y1 = MIN(h, y0 + rps);
            usize k = 0;
            for (int y = y0; y < y1; y++)
                for (int x = 0; x < w; x++) {
                    u32 c = F->px[(usize)y * w + x];
                    raw[k++] = (u8)(c >> 16); raw[k++] = (u8)(c >> 8); raw[k++] = (u8)c;
                    if (alpha) raw[k++] = (u8)(c >> 24);
                }
            if (o.n & 1) ob_u8(&o, 0);
            soff[s] = (u32)o.n;
            if (comp == 5) tf_lzw_encode(&o, raw, k);
            else if (comp == 32773) {
                for (int y = 0; y < y1 - y0; y++) tf_packbits_encode(&o, raw + (usize)y * rowb, rowb);  /* PackBits por linha */
            } else if (comp == 8) {
                u8 *z = NULL; usize zn = 0;
                if (zlib_deflate_buf(raw, k, 6, &z, &zn)) { o.err = 1; break; }
                ob_put(&o, z, zn);
                kfree(z);
            } else ob_put(&o, raw, k);
            scnt[s] = (u32)(o.n - soff[s]);
        }
        kfree(raw);
        /* dados longos fora do IFD */
        if (o.n & 1) ob_u8(&o, 0);
        u32 bps_off = (u32)o.n;
        for (int i = 0; i < spp; i++) ob_le16(&o, 8);
        u32 so_off = (u32)o.n;
        for (int s = 0; s < ns; s++) ob_le32(&o, soff[s]);
        u32 sc_off = (u32)o.n;
        for (int s = 0; s < ns; s++) ob_le32(&o, scnt[s]);
        u32 xr_off = (u32)o.n;
        u32 dx = op && op->dpi_x > 0 ? (u32)(op->dpi_x * 10000 + 0.5) : 720000, dy = op && op->dpi_y > 0 ? (u32)(op->dpi_y * 10000 + 0.5) : dx;
        ob_le32(&o, dx); ob_le32(&o, 10000); ob_le32(&o, dy); ob_le32(&o, 10000);
        u32 so0 = soff[0], sc0 = scnt[0];
        kfree(soff); kfree(scnt);
        if (o.n & 1) ob_u8(&o, 0);
        u32 ifd_off = (u32)o.n;
        if (o.err) break;
        o.p[link] = (u8)ifd_off; o.p[link + 1] = (u8)(ifd_off >> 8); o.p[link + 2] = (u8)(ifd_off >> 16); o.p[link + 3] = (u8)(ifd_off >> 24);
        struct { u16 tag, type; u32 cnt, val; } e[16];
        int ne = 0;
#define TE(t, ty, c, v) e[ne++] = (typeof(e[0])){t, ty, c, v}
        TE(256, 4, 1, (u32)w);
        TE(257, 4, 1, (u32)h);
        TE(258, 3, (u32)spp, bps_off);
        TE(259, 3, 1, (u32)comp);
        TE(262, 3, 1, 2);
        TE(273, 4, (u32)ns, ns == 1 ? so0 : so_off);
        int ori = F->orientation >= 1 && F->orientation <= 8 ? F->orientation : op && op->orientation >= 1 && op->orientation <= 8 ? op->orientation : 1;
        TE(274, 3, 1, (u32)ori);
        TE(277, 3, 1, (u32)spp);
        TE(278, 4, 1, (u32)rps);
        TE(279, 4, (u32)ns, ns == 1 ? sc0 : sc_off);
        TE(282, 5, 1, xr_off);
        TE(283, 5, 1, xr_off + 8);
        TE(284, 3, 1, 1);
        TE(296, 3, 1, 2);
        if (alpha) TE(338, 3, 1, 2);
#undef TE
        ob_le16(&o, ne);
        for (int i = 0; i < ne; i++) {
            ob_le16(&o, e[i].tag); ob_le16(&o, e[i].type); ob_le32(&o, e[i].cnt);
            /* SHORT único fica nos 2 primeiros bytes; 3 SHORTs de BitsPerSample (6 bytes) ficam fora */
            if (e[i].type == 3 && e[i].cnt <= 2 && e[i].tag != 258) { ob_le16(&o, (int)e[i].val); ob_le16(&o, 0); }
            else if (e[i].tag == 258 && spp <= 2) { ob_le16(&o, 8); ob_le16(&o, spp == 2 ? 8 : 0); }
            else ob_le32(&o, e[i].val);
        }
        link = o.n;
        ob_le32(&o, 0);
    }
    if (o.err || link == 4) { kfree(o.p); return -1; }
    *out = o.p;
    *on = o.n;
    return 0;
}
