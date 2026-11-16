/*
 * mOS ImageIO — BMP (Windows 3/4/5 e OS/2 1.x) e ICO/CUR. Incluído por imageio.c depois de iio_gif.inc.
 * Leitura: 1/4/8/16/24/32 bits, BI_RGB, RLE8, RLE4, BITFIELDS/ALPHABITFIELDS, de cima para baixo
 * ou de baixo para cima, densidade em pixels por metro. ICO: o maior ícone (PNG ou DIB com máscara AND).
 * Gravação: 24 bits sem alfa, 32 bits BITMAPV4HEADER com máscaras quando a imagem tem alfa.
 */

static u32 le32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
static u16 le16(const u8 *p) { return (u16)(p[0] | p[1] << 8); }

static int mask_shift(u32 m) { int s = 0; if (!m) return 0; while (!(m & 1)) { m >>= 1; s++; } return s; }
static int mask_bits(u32 m) { int b = 0; while (m) { b += m & 1; m >>= 1; } return b; }
static int mask_get(u32 v, u32 m) {
    if (!m) return -1;
    int s = mask_shift(m), b = mask_bits(m);
    u32 x = (v & m) >> s;
    if (b >= 8) return (int)(x >> (b - 8));
    return b ? (int)(x * 255 / ((1u << b) - 1)) : 0;
}

/* DIB sem o cabeçalho de arquivo; pixoff = deslocamento dos pixels a partir de dib (0 = logo após a paleta).
 * ico = 1: altura dupla com máscara AND depois do XOR. */
static u32 *bmp_dib(const u8 *dib, usize n, usize pixoff, int ico, int *ow, int *oh, int *odepth, int *oalpha,
                    int *oidx, double *dx, double *dy) {
    if (n < 12) return NULL;
    u32 hs = le32(dib);
    int w, h, bpp, comp = 0, ncol = 0;
    u32 rm = 0, gm = 0, bm = 0, am = 0;
    int palsz = 4;
    if (hs == 12) {
        w = le16(dib + 4); h = (i16)le16(dib + 6); bpp = le16(dib + 10); palsz = 3;
    } else if (hs >= 40 && n >= 40) {
        w = (i32)le32(dib + 4); h = (i32)le32(dib + 8); bpp = le16(dib + 14); comp = (int)le32(dib + 16);
        ncol = (int)le32(dib + 32);
        if (dx) { i32 px = (i32)le32(dib + 24), py = (i32)le32(dib + 28); *dx = px > 0 ? px * 0.0254 : 0; *dy = py > 0 ? py * 0.0254 : 0; }
        if (comp == 3 || comp == 6) {
            if (hs >= 52 && n >= 52) { rm = le32(dib + 40); gm = le32(dib + 44); bm = le32(dib + 48); }
            else if (n >= 52) { rm = le32(dib + 40); gm = le32(dib + 44); bm = le32(dib + 48); }
            if ((hs >= 56 || comp == 6) && n >= 56) am = le32(dib + 52);
        }
    } else return NULL;
    if (ico) h /= 2;
    int topdown = h < 0;
    if (topdown) h = -h;
    if (w <= 0 || h <= 0 || (u64)w * h > MAX_PIXELS) return NULL;
    if (bpp != 1 && bpp != 4 && bpp != 8 && bpp != 16 && bpp != 24 && bpp != 32) return NULL;
    if (comp != 0 && comp != 1 && comp != 2 && comp != 3 && comp != 6) return NULL;
    usize palpos = hs + ((comp == 3 && hs == 40) ? 12 : (comp == 6 && hs == 40) ? 16 : 0);
    u32 pal[256];
    int npal = 0;
    if (bpp <= 8) {
        npal = ncol ? MIN(ncol, 256) : 1 << bpp;
        if (palpos + (usize)npal * palsz > n) npal = (int)((n - palpos) / palsz);
        for (int i = 0; i < npal; i++) {
            const u8 *c = dib + palpos + (usize)i * palsz;
            pal[i] = RGB(c[2], c[1], c[0]);
        }
        for (int i = npal; i < 256; i++) pal[i] = 0xFF000000u;
    }
    if (!pixoff) pixoff = palpos + (usize)npal * (bpp <= 8 ? palsz : 0);
    if (pixoff >= n) return NULL;
    if (bpp == 16 && comp == 0) { rm = 0x7C00; gm = 0x03E0; bm = 0x001F; }
    if (bpp == 32 && comp == 0) { rm = 0xFF0000; gm = 0xFF00; bm = 0xFF; }
    u32 *px = kcalloc((usize)w * h, 4);
    if (!px) return NULL;
    const u8 *p = dib + pixoff;
    usize avail = n - pixoff;
    int has_alpha = 0;
    if (comp == 1 || comp == 2) {
        /* RLE: escreve de baixo para cima; pixels não tocados ficam transparentes */
        int x = 0, y = 0;
        usize i = 0;
        has_alpha = 1;
        for (int k = 0; k < w * h; k++) px[k] = 0;
        while (i + 1 < avail && y < h) {
            u8 a = p[i], b = p[i + 1];
            i += 2;
            if (a) {
                for (int k = 0; k < a && x < w; k++, x++) {
                    int v = comp == 1 ? b : (k & 1 ? b & 15 : b >> 4);
                    px[(usize)(topdown ? y : h - 1 - y) * w + x] = pal[v];
                }
            } else if (b == 0) { x = 0; y++; }
            else if (b == 1) break;
            else if (b == 2) { if (i + 1 >= avail) break; x += p[i]; y += p[i + 1]; i += 2; }
            else {
                for (int k = 0; k < b; k++, x++) {
                    int v;
                    if (comp == 1) { if (i + (usize)k >= avail) break; v = p[i + k]; }
                    else { usize bi = i + (usize)k / 2; if (bi >= avail) break; v = k & 1 ? p[bi] & 15 : p[bi] >> 4; }
                    if (x < w && y < h) px[(usize)(topdown ? y : h - 1 - y) * w + x] = pal[v];
                }
                usize used = comp == 1 ? (usize)b : ((usize)b + 1) / 2;
                i += (used + 1) & ~(usize)1;
            }
        }
    } else {
        usize stride = (((usize)w * bpp + 31) / 32) * 4;
        for (int y = 0; y < h; y++) {
            usize ro = (usize)y * stride;
            if (ro + stride > avail) break;
            const u8 *row = p + ro;
            u32 *o = px + (usize)(topdown ? y : h - 1 - y) * w;
            for (int x = 0; x < w; x++) {
                if (bpp <= 8) {
                    int bit = x * bpp;
                    int v = (row[bit >> 3] >> (8 - bpp - (bit & 7))) & ((1 << bpp) - 1);
                    o[x] = pal[v];
                } else if (bpp == 24) {
                    o[x] = RGB(row[x * 3 + 2], row[x * 3 + 1], row[x * 3]);
                } else {
                    u32 v = bpp == 16 ? le16(row + x * 2) : le32(row + x * 4);
                    int r = mask_get(v, rm), g = mask_get(v, gm), b = mask_get(v, bm), a = mask_get(v, am);
                    o[x] = RGBA(r < 0 ? 0 : r, g < 0 ? 0 : g, b < 0 ? 0 : b, a < 0 ? 255 : a);
                }
            }
        }
        if (am) {
            int any = 0;
            for (usize k = 0; k < (usize)w * h; k++) if (px[k] >> 24) { any = 1; break; }
            if (!any) for (usize k = 0; k < (usize)w * h; k++) px[k] |= 0xFF000000u;   /* canal de alfa todo zero: ignorado */
            else has_alpha = 1;
        }
        if (ico) {
            usize mo = pixoff + stride * (usize)h, ms = (((usize)w + 31) / 32) * 4;
            int bpp32_alpha = bpp == 32 && has_alpha;
            if (!bpp32_alpha && mo + ms * (usize)h <= n) {
                for (int y = 0; y < h; y++) {
                    const u8 *mr = dib + mo + (usize)y * ms;
                    u32 *o = px + (usize)(h - 1 - y) * w;
                    for (int x = 0; x < w; x++) if (mr[x >> 3] & (0x80 >> (x & 7))) { o[x] &= 0x00FFFFFFu; has_alpha = 1; }
                }
            }
        }
    }
    *ow = w; *oh = h; *odepth = bpp; *oalpha = has_alpha; *oidx = bpp <= 8;
    return px;
}

static int iio_bmp(const u8 *d, usize n, iio_image *out) {
    if (n < 26 || d[0] != 'B' || d[1] != 'M') return -1;
    u32 off = le32(d + 10);
    int w, h, depth, alpha, idx;
    double dx = 0, dy = 0;
    if (off < 14 + 12 || off >= n) return -1;
    u32 *px = bmp_dib(d + 14, n - 14, off - 14, 0, &w, &h, &depth, &alpha, &idx, &dx, &dy);
    if (!px) return -1;
    out->f = kcalloc(1, sizeof(iio_frame));
    if (!out->f) { kfree(px); return -1; }
    out->f[0] = (iio_frame){px, w, h, 0, depth <= 8 ? 8 : depth == 16 ? 5 : 8, IIO_RGB, alpha, idx, 1, dx, dy, 0, 0};
    out->nframes = 1;
    out->w = w;
    out->h = h;
    out->loop_count = -1;
    out->srgb_intent = -1;
    out->uti = "com.microsoft.bmp";
    return 0;
}

static int iio_png(const u8 *d, usize n, iio_image *out);
/* ICO/CUR: todas as imagens do diretório, a maior primeiro (como o ImageIO, que lista cada ícone) */
static int iio_ico(const u8 *d, usize n, iio_image *out) {
    if (n < 6 || le16(d) != 0 || (le16(d + 2) != 1 && le16(d + 2) != 2)) return -1;
    int cnt = le16(d + 4);
    if (!cnt || 6 + (usize)cnt * 16 > n) return -1;
    out->f = kcalloc((usize)cnt, sizeof(iio_frame));
    if (!out->f) return -1;
    for (int i = 0; i < cnt; i++) {
        const u8 *e = d + 6 + i * 16;
        u32 sz = le32(e + 8), off = le32(e + 12);
        if (off >= n || sz > n - off) continue;
        iio_frame *o = &out->f[out->nframes];
        if (sz >= 8 && !memcmp(d + off, "\x89PNG", 4)) {
            iio_image sub = {0};
            if (iio_png(d + off, sz, &sub) == 0 && sub.nframes) {
                *o = sub.f[0];
                sub.f[0].px = NULL;
                out->nframes++;
            }
            iio_image_free(&sub);
            continue;
        }
        int w, h, depth, alpha, idx;
        u32 *px = bmp_dib(d + off, sz, 0, 1, &w, &h, &depth, &alpha, &idx, NULL, NULL);
        if (!px) continue;
        *o = (iio_frame){px, w, h, 0, 8, IIO_RGB, alpha, idx, 1, 0, 0, 0, 0};
        out->nframes++;
    }
    if (!out->nframes) { kfree(out->f); out->f = NULL; return -1; }
    /* a maior primeiro; ordem estável para o resto */
    for (int i = 1; i < out->nframes; i++) {
        iio_frame t = out->f[i];
        int j = i;
        while (j > 0 && (u64)out->f[j - 1].w * out->f[j - 1].h < (u64)t.w * t.h) { out->f[j] = out->f[j - 1]; j--; }
        out->f[j] = t;
    }
    out->w = out->f[0].w;
    out->h = out->f[0].h;
    out->loop_count = -1;
    out->srgb_intent = -1;
    out->uti = le16(d + 2) == 2 ? "com.microsoft.cur" : "com.microsoft.ico";
    return 0;
}

static int iio_bmp_encode(const iio_enc_frame *f, const iio_enc_opts *op, u8 **out, usize *on) {
    int w = f->w, h = f->h, alpha = 0;
    if (!f->px || w < 1 || h < 1) return -1;
    for (usize i = 0; i < (usize)w * h; i++) if ((f->px[i] >> 24) != 255) { alpha = 1; break; }
    int bpp = alpha ? 32 : 24;
    usize stride = (((usize)w * bpp + 31) / 32) * 4, hs = alpha ? 108 : 40, off = 14 + hs, size = off + stride * h;
    u8 *b = kcalloc(size, 1);
    if (!b) return -1;
    b[0] = 'B'; b[1] = 'M';
    b[2] = (u8)size; b[3] = (u8)(size >> 8); b[4] = (u8)(size >> 16); b[5] = (u8)(size >> 24);
    b[10] = (u8)off; b[11] = (u8)(off >> 8);
    u8 *ih = b + 14;
    u32 vals[10] = {(u32)hs, (u32)w, (u32)h, 1u | (u32)bpp << 16, alpha ? 3u : 0u, (u32)(stride * h), 0, 0, 0, 0};
    u32 ppm = op && op->dpi_x > 0 ? (u32)(op->dpi_x / 0.0254 + 0.5) : 2835;
    vals[6] = ppm;
    vals[7] = op && op->dpi_y > 0 ? (u32)(op->dpi_y / 0.0254 + 0.5) : ppm;
    for (int i = 0; i < 10; i++) { u32 v = vals[i]; ih[i * 4] = (u8)v; ih[i * 4 + 1] = (u8)(v >> 8); ih[i * 4 + 2] = (u8)(v >> 16); ih[i * 4 + 3] = (u8)(v >> 24); }
    ih[12] = 1; ih[13] = 0; ih[14] = (u8)bpp; ih[15] = 0;
    if (alpha) {
        u32 m[4] = {0x00FF0000u, 0x0000FF00u, 0x000000FFu, 0xFF000000u};
        for (int i = 0; i < 4; i++) { ih[40 + i * 4] = (u8)m[i]; ih[41 + i * 4] = (u8)(m[i] >> 8); ih[42 + i * 4] = (u8)(m[i] >> 16); ih[43 + i * 4] = (u8)(m[i] >> 24); }
        memcpy(ih + 56, "BGRs", 4);         /* LCS_sRGB, gravado como 'sRGB' em little-endian */
    }
    for (int y = 0; y < h; y++) {
        u8 *row = b + off + (usize)(h - 1 - y) * stride;
        const u32 *s = f->px + (usize)y * w;
        for (int x = 0; x < w; x++) {
            u32 c = s[x];
            if (alpha) { row[x * 4] = (u8)c; row[x * 4 + 1] = (u8)(c >> 8); row[x * 4 + 2] = (u8)(c >> 16); row[x * 4 + 3] = (u8)(c >> 24); }
            else {
                row[x * 3] = (u8)c; row[x * 3 + 1] = (u8)(c >> 8); row[x * 3 + 2] = (u8)(c >> 16);
            }
        }
    }
    *out = b;
    *on = size;
    return 0;
}
