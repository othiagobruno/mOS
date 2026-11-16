/*
 * mOS ImageIO — GIF 87a/89a. Incluído por imageio.c (depois de iio_pngjpeg.inc).
 * Leitura: LZW, entrelaçado, tabela global e local, GCE (atraso, transparência, disposição 0..3),
 * NETSCAPE2.0/ANIMEXTS1.0 (repetições). Cada quadro sai composto na tela, fundo transparente.
 * Gravação: paleta exata quando há até 256 cores, senão corte mediano num histograma de 15 bits;
 * transparência para alfa < 128; LZW com código de limpeza ao encher a tabela.
 */

/* ------------------------------------------------------------------ leitura */
typedef struct {
    const u8 *d;
    usize n, pos;
} gif_rd;

/* junta os sub-blocos de dados a partir de r->pos; devolve o tamanho (malloc em *out) */
static usize gif_subblocks(gif_rd *r, u8 **out) {
    usize cap = 4096, len = 0;
    u8 *b = kmalloc(cap);
    if (!b) { *out = NULL; return 0; }
    while (r->pos < r->n) {
        u8 k = r->d[r->pos++];
        if (!k) break;
        if (r->pos + k > r->n) { k = (u8)(r->n - r->pos); }
        if (len + k > cap) {
            while (len + k > cap) cap *= 2;
            u8 *nb = kmalloc(cap);
            if (!nb) { kfree(b); *out = NULL; return 0; }
            memcpy(nb, b, len);
            kfree(b);
            b = nb;
        }
        memcpy(b + len, r->d + r->pos, k);
        len += k;
        r->pos += k;
    }
    *out = b;
    return len;
}

static void gif_skip_subblocks(gif_rd *r) {
    while (r->pos < r->n) {
        u8 k = r->d[r->pos++];
        if (!k) break;
        r->pos += k;
    }
}

/* LZW do GIF: códigos LSB primeiro, tamanho variável até 12 bits. Devolve quantos índices escreveu. */
static usize gif_lzw(const u8 *in, usize n, int minsz, u8 *out, usize cap) {
    if (minsz < 2 || minsz > 11) return 0;
    u16 prefix[4096];
    u8 suffix[4096], stack[4097];
    int clear = 1 << minsz, eoi = clear + 1, next = clear + 2, size = minsz + 1, old = -1;
    u8 first = 0;
    u32 acc = 0;
    int bits = 0;
    usize ip = 0, op = 0;
    for (int i = 0; i < clear; i++) { prefix[i] = 0xFFFF; suffix[i] = (u8)i; }
    for (;;) {
        while (bits < size && ip < n) { acc |= (u32)in[ip++] << bits; bits += 8; }
        if (bits < size) break;
        int code = (int)(acc & ((1u << size) - 1));
        acc >>= size;
        bits -= size;
        if (code == clear) { size = minsz + 1; next = clear + 2; old = -1; continue; }
        if (code == eoi) break;
        int sp = 0, c = code;
        if (old < 0) {
            if (code >= clear) break;
            if (op < cap) out[op++] = (u8)code;
            old = code;
            first = (u8)code;
            continue;
        }
        if (code > next || (code == next && next >= 4096)) break;
        if (code == next) { stack[sp++] = first; c = old; }
        while (c >= clear) {
            if (sp >= 4096 || c >= 4096) return op;
            stack[sp++] = suffix[c];
            c = prefix[c];
        }
        first = (u8)c;
        stack[sp++] = first;
        while (sp && op < cap) out[op++] = stack[--sp];
        if (next < 4096) {
            prefix[next] = (u16)old;
            suffix[next] = first;
            next++;
            if (next == (1 << size) && size < 12) size++;
        }
        old = code;
        if (op >= cap) break;
    }
    return op;
}

static int iio_gif(const u8 *d, usize n, iio_image *out) {
    if (n < 13 || memcmp(d, "GIF", 3) || (memcmp(d + 3, "87a", 3) && memcmp(d + 3, "89a", 3))) return -1;
    int W = d[6] | d[7] << 8, H = d[8] | d[9] << 8;
    u8 pk = d[10];
    gif_rd r = {d, n, 13};
    u32 gpal[256];
    int ngpal = 0;
    if (!W || !H || (u64)W * H > MAX_PIXELS) return -1;
    if (pk & 0x80) {
        ngpal = 2 << (pk & 7);
        if (r.pos + (usize)ngpal * 3 > n) return -1;
        for (int i = 0; i < ngpal; i++) gpal[i] = RGB(d[r.pos + i * 3], d[r.pos + i * 3 + 1], d[r.pos + i * 3 + 2]);
        r.pos += (usize)ngpal * 3;
    }
    out->has_global_map = ngpal > 0;
    out->loop_count = -1;
    out->srgb_intent = -1;
    u32 *canvas = kcalloc((usize)W * H, 4);
    u8 *idx = kmalloc((usize)W * H);
    if (!canvas || !idx) { kfree(canvas); kfree(idx); return -1; }
    int cap = 0, delay = 0, trans = -1, dispose = 0, any_trans = 0, interlaced_any = 0;
    while (r.pos < n) {
        u8 b = d[r.pos++];
        if (b == 0x3B) break;
        if (b == 0x21) {
            if (r.pos >= n) break;
            u8 lab = d[r.pos++];
            if (lab == 0xF9 && r.pos + 6 <= n && d[r.pos] >= 4) {
                u8 f = d[r.pos + 1];
                dispose = (f >> 2) & 7;
                delay = (d[r.pos + 2] | d[r.pos + 3] << 8) * 10;
                trans = (f & 1) ? d[r.pos + 4] : -1;
                r.pos += 1 + d[r.pos];
                gif_skip_subblocks(&r);
            } else if (lab == 0xFF && r.pos + 12 <= n && d[r.pos] == 11 &&
                       (!memcmp(d + r.pos + 1, "NETSCAPE2.0", 11) || !memcmp(d + r.pos + 1, "ANIMEXTS1.0", 11))) {
                r.pos += 12;
                while (r.pos < n) {
                    u8 k = d[r.pos++];
                    if (!k) break;
                    if (k >= 3 && r.pos + 3 <= n && d[r.pos] == 1) out->loop_count = d[r.pos + 1] | d[r.pos + 2] << 8;
                    r.pos += k;
                }
            } else gif_skip_subblocks(&r);
            continue;
        }
        if (b != 0x2C || r.pos + 9 > n) break;
        int fx = d[r.pos] | d[r.pos + 1] << 8, fy = d[r.pos + 2] | d[r.pos + 3] << 8;
        int fw = d[r.pos + 4] | d[r.pos + 5] << 8, fh = d[r.pos + 6] | d[r.pos + 7] << 8;
        u8 ip = d[r.pos + 8];
        r.pos += 9;
        u32 lpal[256];
        const u32 *pal = gpal;
        int npal = ngpal;
        if (ip & 0x80) {
            npal = 2 << (ip & 7);
            if (r.pos + (usize)npal * 3 > n) break;
            for (int i = 0; i < npal; i++) lpal[i] = RGB(d[r.pos + i * 3], d[r.pos + i * 3 + 1], d[r.pos + i * 3 + 2]);
            r.pos += (usize)npal * 3;
            pal = lpal;
        }
        if (r.pos >= n) break;
        int minsz = d[r.pos++];
        u8 *lz = NULL;
        usize lzn = gif_subblocks(&r, &lz);
        if (!lz) break;
        usize want = (usize)fw * fh;
        u8 *raw = want ? kcalloc(want, 1) : NULL;
        usize got = raw ? gif_lzw(lz, lzn, minsz, raw, want) : 0;
        kfree(lz);
        if (!raw || !got) { kfree(raw); if (out->nframes) break; continue; }
        u32 *saved = NULL;
        if (dispose == 3) {
            saved = kmalloc((usize)W * H * 4);
            if (saved) memcpy(saved, canvas, (usize)W * H * 4);
        }
        int il = (ip & 0x40) != 0;
        interlaced_any |= il;
        int row = 0, pass = 0;
        static const int istart[4] = {0, 4, 2, 1}, istep[4] = {8, 8, 4, 2};
        for (int y = 0; y < fh; y++) {
            int yy;
            if (il) {
                if (y == 0) { pass = 0; row = 0; }
                while (row >= fh && pass < 3) { pass++; row = istart[pass]; }
                yy = row;
                row += istep[pass];
            } else yy = y;
            int cy = fy + yy;
            if (cy < 0 || cy >= H) continue;
            for (int x = 0; x < fw; x++) {
                int cx = fx + x;
                usize k = (usize)y * fw + x;
                if (cx < 0 || cx >= W || k >= got) continue;
                u8 v = raw[k];
                if ((int)v == trans) { any_trans = 1; continue; }
                canvas[(usize)cy * W + cx] = v < npal ? pal[v] : 0xFF000000u;
            }
        }
        kfree(raw);
        if (out->nframes == cap) {
            int nc = cap ? cap * 2 : 8;
            iio_frame *nf = kcalloc((usize)nc, sizeof *nf);
            if (!nf) { kfree(saved); break; }
            if (out->f) { memcpy(nf, out->f, (usize)out->nframes * sizeof *nf); kfree(out->f); }
            out->f = nf;
            cap = nc;
        }
        iio_frame *o = &out->f[out->nframes];
        o->px = kmalloc((usize)W * H * 4);
        if (!o->px) { kfree(saved); break; }
        memcpy(o->px, canvas, (usize)W * H * 4);
        o->w = W;
        o->h = H;
        o->delay_ms = delay;
        o->depth = 8;
        o->color = IIO_RGB;
        o->indexed = 1;
        o->orientation = 1;
        out->nframes++;
        if (dispose == 2) {
            for (int y = 0; y < fh; y++) {
                int cy = fy + y;
                if (cy < 0 || cy >= H) continue;
                for (int x = 0; x < fw; x++) {
                    int cx = fx + x;
                    if (cx >= 0 && cx < W) canvas[(usize)cy * W + cx] = 0;
                }
            }
        } else if (dispose == 3 && saved) memcpy(canvas, saved, (usize)W * H * 4);
        kfree(saved);
        delay = 0; trans = -1; dispose = 0;
    }
    kfree(canvas);
    kfree(idx);
    if (!out->nframes) { kfree(out->f); out->f = NULL; return -1; }
    for (int i = 0; i < out->nframes; i++) out->f[i].has_alpha = any_trans;
    out->w = W;
    out->h = H;
    out->interlaced = interlaced_any;
    out->uti = "com.compuserve.gif";
    return 0;
}

/* ------------------------------------------------------------------ gravação */
typedef struct { u8 *p; usize n, cap; int err; } obuf_t;
static void ob_put(obuf_t *o, const void *s, usize n) {
    if (o->err) return;
    if (o->n + n > o->cap) {
        usize nc = o->cap ? o->cap * 2 : 65536;
        while (nc < o->n + n) nc *= 2;
        u8 *nb = kmalloc(nc);
        if (!nb) { o->err = 1; return; }
        if (o->p) { memcpy(nb, o->p, o->n); kfree(o->p); }
        o->p = nb;
        o->cap = nc;
    }
    memcpy(o->p + o->n, s, n);
    o->n += n;
}
static void ob_u8(obuf_t *o, int v) { u8 b = (u8)v; ob_put(o, &b, 1); }
static void ob_le16(obuf_t *o, int v) { ob_u8(o, v & 255); ob_u8(o, (v >> 8) & 255); }

/* corte mediano: caixas no espaço RGB de 5 bits, partidas no eixo mais longo pela mediana dos pesos */
typedef struct { int lo[3], hi[3]; u32 count; } gbox_t;
static void gbox_shrink(gbox_t *b, const u32 *hist) {
    int lo[3] = {31, 31, 31}, hi[3] = {0, 0, 0};
    u32 cnt = 0;
    for (int r = b->lo[0]; r <= b->hi[0]; r++)
        for (int g = b->lo[1]; g <= b->hi[1]; g++)
            for (int bl = b->lo[2]; bl <= b->hi[2]; bl++) {
                u32 c = hist[r << 10 | g << 5 | bl];
                if (!c) continue;
                cnt += c;
                if (r < lo[0]) lo[0] = r;
                if (r > hi[0]) hi[0] = r;
                if (g < lo[1]) lo[1] = g;
                if (g > hi[1]) hi[1] = g;
                if (bl < lo[2]) lo[2] = bl;
                if (bl > hi[2]) hi[2] = bl;
            }
    b->count = cnt;
    if (cnt) for (int k = 0; k < 3; k++) { b->lo[k] = lo[k]; b->hi[k] = hi[k]; }
}

static int gif_palette(const u32 *px, usize np, u32 *pal, int maxc, int *ntrans) {
    /* tenta a paleta exata */
    int n = 0, t = 0;
    for (usize i = 0; i < np && n <= maxc; i++) {
        u32 c = px[i];
        if ((c >> 24) < 128) { t = 1; continue; }
        c |= 0xFF000000u;
        int k;
        for (k = 0; k < n; k++) if (pal[k] == c) break;
        if (k == n) { if (n == maxc) { n = maxc + 1; break; } pal[n++] = c; }
    }
    *ntrans = t;
    if (n <= maxc) return n;
    u32 *hist = kcalloc(32768, 4);
    if (!hist) return 0;
    for (usize i = 0; i < np; i++) {
        u32 c = px[i];
        if ((c >> 24) < 128) continue;
        hist[((c >> 19) & 31) << 10 | ((c >> 11) & 31) << 5 | ((c >> 3) & 31)]++;
    }
    gbox_t box[256];
    int nb = 1;
    box[0] = (gbox_t){{0, 0, 0}, {31, 31, 31}, 0};
    gbox_shrink(&box[0], hist);
    while (nb < maxc) {
        int best = -1, bl = 0;
        for (int i = 0; i < nb; i++) {
            int l = 0;
            for (int k = 0; k < 3; k++) if (box[i].hi[k] - box[i].lo[k] > l) l = box[i].hi[k] - box[i].lo[k];
            if (l > 0 && (best < 0 || (u64)box[i].count * (u64)(l + 1) > (u64)box[best].count * (u64)(bl + 1))) { best = i; bl = l; }
        }
        if (best < 0) break;
        gbox_t *b = &box[best];
        int ax = 0;
        for (int k = 1; k < 3; k++) if (b->hi[k] - b->lo[k] > b->hi[ax] - b->lo[ax]) ax = k;
        u32 half = b->count / 2, acc = 0;
        int cut = b->lo[ax];
        for (int v = b->lo[ax]; v < b->hi[ax]; v++) {
            int lo[3] = {b->lo[0], b->lo[1], b->lo[2]}, hi[3] = {b->hi[0], b->hi[1], b->hi[2]};
            lo[ax] = hi[ax] = v;
            for (int r = lo[0]; r <= hi[0]; r++)
                for (int g = lo[1]; g <= hi[1]; g++)
                    for (int c2 = lo[2]; c2 <= hi[2]; c2++) acc += hist[r << 10 | g << 5 | c2];
            cut = v;
            if (acc >= half) break;
        }
        gbox_t nbx = *b;
        b->hi[ax] = cut;
        nbx.lo[ax] = cut + 1;
        gbox_shrink(b, hist);
        gbox_shrink(&nbx, hist);
        box[nb++] = nbx;
    }
    for (int i = 0; i < nb; i++) {
        u64 s[3] = {0, 0, 0}, c = 0;
        gbox_t *b = &box[i];
        for (int r = b->lo[0]; r <= b->hi[0]; r++)
            for (int g = b->lo[1]; g <= b->hi[1]; g++)
                for (int bl = b->lo[2]; bl <= b->hi[2]; bl++) {
                    u32 h = hist[r << 10 | g << 5 | bl];
                    s[0] += (u64)h * (u64)(r * 8 + 4); s[1] += (u64)h * (u64)(g * 8 + 4); s[2] += (u64)h * (u64)(bl * 8 + 4);
                    c += h;
                }
        if (!c) c = 1;
        pal[i] = RGB(s[0] / c, s[1] / c, s[2] / c);
    }
    kfree(hist);
    return nb;
}

static int gif_nearest(const u32 *pal, int n, u32 c) {
    int best = 0;
    long bd = 1L << 30;
    int r = (int)(c >> 16 & 255), g = (int)(c >> 8 & 255), b = (int)(c & 255);
    for (int i = 0; i < n; i++) {
        int dr = r - (int)(pal[i] >> 16 & 255), dg = g - (int)(pal[i] >> 8 & 255), db = b - (int)(pal[i] & 255);
        long dd = (long)dr * dr * 2 + (long)dg * dg * 4 + (long)db * db * 3;
        if (dd < bd) { bd = dd; best = i; if (!dd) break; }
    }
    return best;
}

typedef struct { obuf_t *o; u8 blk[255]; int nb; u32 acc; int bits; } gif_bw;
static void gbw_byte(gif_bw *w, u8 b) {
    w->blk[w->nb++] = b;
    if (w->nb == 255) { ob_u8(w->o, 255); ob_put(w->o, w->blk, 255); w->nb = 0; }
}
static void gbw_code(gif_bw *w, int code, int size) {
    w->acc |= (u32)code << w->bits;
    w->bits += size;
    while (w->bits >= 8) { gbw_byte(w, (u8)w->acc); w->acc >>= 8; w->bits -= 8; }
}

/* LZW com tabela de hash (prefixo << 8 | byte) */
static void gif_lzw_encode(obuf_t *o, const u8 *idx, usize n, int minsz) {
    enum { HS = 5003 };
    int *hcode = kmalloc(HS * sizeof(int));
    u32 *hkey = kmalloc(HS * sizeof(u32));
    gif_bw w = {o, {0}, 0, 0, 0};
    int clear = 1 << minsz, eoi = clear + 1, next = clear + 2, size = minsz + 1;
    ob_u8(o, minsz);
    if (!hcode || !hkey) { o->err = 1; kfree(hcode); kfree(hkey); return; }
    for (int i = 0; i < HS; i++) hcode[i] = -1;
    gbw_code(&w, clear, size);
    if (n) {
        int cur = idx[0];
        for (usize i = 1; i < n; i++) {
            u8 c = idx[i];
            u32 key = (u32)cur << 8 | c;
            u32 h = (key * 2654435761u) % HS;
            while (hcode[h] >= 0 && hkey[h] != key) h = (h + 1) % HS;
            if (hcode[h] >= 0) { cur = hcode[h]; continue; }
            gbw_code(&w, cur, size);
            if (next < 4096) {
                hcode[h] = next;
                hkey[h] = key;
                if (next == (1 << size) && size < 12) size++;
                next++;
                /* o leitor cresce o código um passo antes; acompanha */
            } else {
                gbw_code(&w, clear, size);
                for (int k = 0; k < HS; k++) hcode[k] = -1;
                next = clear + 2;
                size = minsz + 1;
            }
            cur = c;
        }
        gbw_code(&w, cur, size);
    }
    gbw_code(&w, eoi, size);
    if (w.bits) gbw_byte(&w, (u8)w.acc);
    if (w.nb) { ob_u8(o, w.nb); ob_put(o, w.blk, (usize)w.nb); }
    ob_u8(o, 0);
    kfree(hcode);
    kfree(hkey);
}

static int iio_gif_encode(const iio_enc_frame *fr, int nfr, const iio_enc_opts *op, u8 **out, usize *on) {
    if (nfr < 1 || !fr[0].px || fr[0].w < 1 || fr[0].h < 1 || fr[0].w > 65535 || fr[0].h > 65535) return -1;
    int W = fr[0].w, H = fr[0].h;
    obuf_t o = {0};
    ob_put(&o, "GIF89a", 6);
    ob_le16(&o, W);
    ob_le16(&o, H);
    ob_u8(&o, 0x70);          /* sem tabela global; 8 bits de resolução de cor */
    ob_u8(&o, 0);
    ob_u8(&o, 0);
    if (nfr > 1) {
        ob_u8(&o, 0x21); ob_u8(&o, 0xFF); ob_u8(&o, 11); ob_put(&o, "NETSCAPE2.0", 11);
        ob_u8(&o, 3); ob_u8(&o, 1); ob_le16(&o, op && op->loop_count > 0 ? op->loop_count : 0); ob_u8(&o, 0);
    }
    u8 *idx = kmalloc((usize)W * H);
    if (!idx) return -1;
    for (int f = 0; f < nfr && !o.err; f++) {
        if (!fr[f].px || fr[f].w != W || fr[f].h != H) continue;
        u32 pal[256];
        int ntr = 0;
        int np = gif_palette(fr[f].px, (usize)W * H, pal, 255, &ntr);
        if (np < 1) { pal[0] = 0xFF000000u; np = 1; }
        int ti = ntr ? np : -1;
        int tot = np + (ntr ? 1 : 0), bits = 1;
        while ((1 << bits) < tot) bits++;
        for (usize i = 0; i < (usize)W * H; i++) {
            u32 c = fr[f].px[i];
            idx[i] = (u8)((c >> 24) < 128 && ti >= 0 ? ti : gif_nearest(pal, np, c));
        }
        int delay = (fr[f].delay_ms + 5) / 10;
        ob_u8(&o, 0x21); ob_u8(&o, 0xF9); ob_u8(&o, 4);
        ob_u8(&o, (ntr ? 2 : 1) << 2 | (ntr ? 1 : 0));
        ob_le16(&o, delay);
        ob_u8(&o, ti >= 0 ? ti : 0);
        ob_u8(&o, 0);
        ob_u8(&o, 0x2C); ob_le16(&o, 0); ob_le16(&o, 0); ob_le16(&o, W); ob_le16(&o, H);
        ob_u8(&o, 0x80 | (bits - 1));
        for (int i = 0; i < (1 << bits); i++) {
            u32 c = i < np ? pal[i] : 0;
            ob_u8(&o, (int)(c >> 16 & 255)); ob_u8(&o, (int)(c >> 8 & 255)); ob_u8(&o, (int)(c & 255));
        }
        gif_lzw_encode(&o, idx, (usize)W * H, bits < 2 ? 2 : bits);
    }
    kfree(idx);
    ob_u8(&o, 0x3B);
    if (o.err) { kfree(o.p); return -1; }
    *out = o.p;
    *on = o.n;
    return 0;
}
