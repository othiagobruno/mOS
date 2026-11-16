/*
 * mOS ImageIO — despacho de formatos, gravador PNG (deflate da zlib, filtro escolhido por linha),
 * redução por filtro de caixa, orientação EXIF e a API antiga mos_img_* (CoreImage, Photos).
 * Incluído por imageio.c depois de todos os codecs.
 */

/* zlib 1.3.2 (no libSystem vem de compression_z.c; no host, da -lz) */
static void *iio_zalloc(void *q, unsigned n, unsigned m) { (void)q; return kcalloc((usize)n, (usize)m); }
static void iio_zfree(void *q, void *p) { (void)q; kfree(p); }
static int zlib_deflate_buf(const u8 *in, usize n, int level, u8 **out, usize *on) {
    z_stream z;
    memset(&z, 0, sizeof z);
    z.zalloc = iio_zalloc;
    z.zfree = iio_zfree;
    if (deflateInit2_(&z, level, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY, ZLIB_VERSION, (int)sizeof(z_stream)) != Z_OK) return -1;
    usize cap = deflateBound(&z, (uLong)n) + 64;
    u8 *b = kmalloc(cap);
    if (!b) { deflateEnd(&z); return -1; }
    z.next_in = (Bytef *)in;
    z.avail_in = (uInt)n;
    z.next_out = b;
    z.avail_out = (uInt)cap;
    int r = deflate(&z, Z_FINISH);
    usize got = cap - z.avail_out;
    deflateEnd(&z);
    if (r != Z_STREAM_END) { kfree(b); return -1; }
    *out = b;
    *on = got;
    return 0;
}

static u32 iio_crc_tab[256];
static u32 iio_crc(const u8 *p, usize n) {
    if (!iio_crc_tab[1])
        for (u32 i = 0; i < 256; i++) {
            u32 c = i;
            for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            iio_crc_tab[i] = c;
        }
    u32 c = 0xFFFFFFFFu;
    for (usize i = 0; i < n; i++) c = iio_crc_tab[(c ^ p[i]) & 255] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}
static void ob_be32(obuf_t *o, u32 v) { ob_u8(o, (int)(v >> 24)); ob_u8(o, (int)(v >> 16) & 255); ob_u8(o, (int)(v >> 8) & 255); ob_u8(o, (int)v & 255); }
static void png_chunk(obuf_t *o, const char *type, const u8 *data, usize n) {
    ob_be32(o, (u32)n);
    usize at = o->n;
    ob_put(o, type, 4);
    if (n) ob_put(o, data, n);
    if (o->err) return;
    ob_be32(o, iio_crc(o->p + at, 4 + n));
}

/* linhas filtradas (o filtro com menor soma absoluta, heurística da libpng) e deflate */
static int png_idat(const u8 *raw, int w, int h, int bpp, u8 **z, usize *zn) {
    usize rb = (usize)w * bpp, fl = rb + 1;
    u8 *f = kmalloc(fl * h), *cand = kmalloc(fl * 5);
    if (!f || !cand) { kfree(f); kfree(cand); return -1; }
    for (int y = 0; y < h; y++) {
        const u8 *r = raw + (usize)y * rb, *p = y ? raw + (usize)(y - 1) * rb : NULL;
        u64 best = ~(u64)0;
        int bi = 0;
        for (int t = 0; t < 5; t++) {
            u8 *c = cand + (usize)t * fl;
            c[0] = (u8)t;
            u64 sum = 0;
            for (usize i = 0; i < rb; i++) {
                int a = i >= (usize)bpp ? r[i - bpp] : 0, b = p ? p[i] : 0, cc = p && i >= (usize)bpp ? p[i - bpp] : 0, v;
                switch (t) {
                case 0: v = r[i]; break;
                case 1: v = r[i] - a; break;
                case 2: v = r[i] - b; break;
                case 3: v = r[i] - ((a + b) >> 1); break;
                default: v = r[i] - paeth(a, b, cc); break;
                }
                c[1 + i] = (u8)v;
                sum += (u8)v < 128 ? (u8)v : 256 - (u8)v;
            }
            if (sum < best) { best = sum; bi = t; }
        }
        memcpy(f + (usize)y * fl, cand + (usize)bi * fl, fl);
    }
    kfree(cand);
    int rc = zlib_deflate_buf(f, fl * h, 6, z, zn);
    kfree(f);
    return rc;
}

static int iio_png_encode(const iio_enc_frame *fr, int nfr, const iio_enc_opts *op, u8 **out, usize *on) {
    int W = fr[0].w, H = fr[0].h;
    if (!fr[0].px || W < 1 || H < 1) return -1;
    int alpha = 0, gray = 1;
    for (int f = 0; f < nfr; f++)
        for (usize i = 0; i < (usize)W * H && fr[f].px && fr[f].w == W && fr[f].h == H; i++) {
            u32 c = fr[f].px[i];
            if ((c >> 24) != 255) alpha = 1;
            if (((c >> 16) & 255) != ((c >> 8) & 255) || ((c >> 8) & 255) != (c & 255)) gray = 0;
        }
    int ct = gray ? (alpha ? 4 : 0) : (alpha ? 6 : 2), bpp = gray ? (alpha ? 2 : 1) : (alpha ? 4 : 3);
    obuf_t o = {0};
    ob_put(&o, "\x89PNG\r\n\x1a\n", 8);
    u8 ih[13] = {(u8)(W >> 24), (u8)(W >> 16), (u8)(W >> 8), (u8)W, (u8)(H >> 24), (u8)(H >> 16), (u8)(H >> 8), (u8)H, 8, (u8)ct, 0, 0, 0};
    png_chunk(&o, "IHDR", ih, 13);
    u8 srgb = 0;
    png_chunk(&o, "sRGB", &srgb, 1);
    if (op && op->dpi_x > 0) {
        u32 px = (u32)(op->dpi_x / 0.0254 + 0.5), py = (u32)((op->dpi_y > 0 ? op->dpi_y : op->dpi_x) / 0.0254 + 0.5);
        u8 ph[9] = {(u8)(px >> 24), (u8)(px >> 16), (u8)(px >> 8), (u8)px, (u8)(py >> 24), (u8)(py >> 16), (u8)(py >> 8), (u8)py, 1};
        png_chunk(&o, "pHYs", ph, 9);
    }
    if (op && op->orientation >= 2 && op->orientation <= 8) {
        /* eXIf (PNG 1.5): TIFF big-endian com o tag Orientation */
        u8 ex[26] = {'M', 'M', 0, 42, 0, 0, 0, 8, 0, 1, 0x01, 0x12, 0, 3, 0, 0, 0, 1, 0, (u8)op->orientation, 0, 0, 0, 0, 0, 0};
        png_chunk(&o, "eXIf", ex, 26);
    }
    int anim = nfr > 1;
    if (anim) {
        u8 ac[8] = {(u8)(nfr >> 24), (u8)(nfr >> 16), (u8)(nfr >> 8), (u8)nfr, 0, 0, 0, 0};
        int lc = op && op->loop_count > 0 ? op->loop_count : 0;
        ac[4] = (u8)(lc >> 24); ac[5] = (u8)(lc >> 16); ac[6] = (u8)(lc >> 8); ac[7] = (u8)lc;
        png_chunk(&o, "acTL", ac, 8);
    }
    u8 *raw = kmalloc((usize)W * H * bpp);
    if (!raw) { kfree(o.p); return -1; }
    u32 seq = 0;
    for (int f = 0; f < nfr && !o.err; f++) {
        if (!fr[f].px || fr[f].w != W || fr[f].h != H) continue;
        usize k = 0;
        for (usize i = 0; i < (usize)W * H; i++) {
            u32 c = fr[f].px[i];
            if (gray) raw[k++] = (u8)c;
            else { raw[k++] = (u8)(c >> 16); raw[k++] = (u8)(c >> 8); raw[k++] = (u8)c; }
            if (alpha) raw[k++] = (u8)(c >> 24);
        }
        u8 *z = NULL;
        usize zn = 0;
        if (png_idat(raw, W, H, bpp, &z, &zn)) { o.err = 1; break; }
        if (anim) {
            u32 dn = (u32)(fr[f].delay_ms > 0 ? fr[f].delay_ms : 0);
            u8 fc[26] = {0};
            u32 v[5] = {seq++, (u32)W, (u32)H, 0, 0};
            for (int i = 0; i < 5; i++) { fc[i * 4] = (u8)(v[i] >> 24); fc[i * 4 + 1] = (u8)(v[i] >> 16); fc[i * 4 + 2] = (u8)(v[i] >> 8); fc[i * 4 + 3] = (u8)v[i]; }
            fc[20] = (u8)(dn >> 8); fc[21] = (u8)dn; fc[22] = 0x03; fc[23] = 0xE8;     /* atraso em ms: dn/1000 */
            fc[24] = 0; fc[25] = 0;
            png_chunk(&o, "fcTL", fc, 26);
        }
        if (!anim || f == 0) png_chunk(&o, "IDAT", z, zn);
        else {
            u8 *fd = kmalloc(zn + 4);
            if (!fd) { kfree(z); o.err = 1; break; }
            fd[0] = (u8)(seq >> 24); fd[1] = (u8)(seq >> 16); fd[2] = (u8)(seq >> 8); fd[3] = (u8)seq;
            seq++;
            memcpy(fd + 4, z, zn);
            png_chunk(&o, "fdAT", fd, zn + 4);
            kfree(fd);
        }
        kfree(z);
    }
    kfree(raw);
    png_chunk(&o, "IEND", NULL, 0);
    if (o.err) { kfree(o.p); return -1; }
    *out = o.p;
    *on = o.n;
    return 0;
}

/* ------------------------------------------------------------------ despacho */
const char *const iio_read_types[] = {"public.png", "public.jpeg", "com.compuserve.gif", "public.tiff", "com.microsoft.bmp",
                                      "com.microsoft.ico", "com.microsoft.cur", NULL};
const char *const iio_write_types[] = {"public.png", "public.jpeg", "com.compuserve.gif", "public.tiff", "com.microsoft.bmp", NULL};

const char *iio_sniff(const uint8_t *d, size_t n) {
    if (!d || n < 4) return NULL;
    if (n >= 8 && !memcmp(d, "\x89PNG\r\n\x1a\n", 8)) return "public.png";
    if (d[0] == 0xFF && d[1] == 0xD8 && d[2] == 0xFF) return "public.jpeg";
    if (n >= 6 && !memcmp(d, "GIF8", 4) && (d[4] == '7' || d[4] == '9') && d[5] == 'a') return "com.compuserve.gif";
    if ((d[0] == 'I' && d[1] == 'I' && d[2] == 42 && d[3] == 0) || (d[0] == 'M' && d[1] == 'M' && d[2] == 0 && d[3] == 42)) return "public.tiff";
    if (n >= 26 && d[0] == 'B' && d[1] == 'M') return "com.microsoft.bmp";
    if (n >= 6 && !d[0] && !d[1] && (d[2] == 1 || d[2] == 2) && !d[3] && (d[4] || d[5])) return d[2] == 1 ? "com.microsoft.ico" : "com.microsoft.cur";
    return NULL;
}

/* o arquivo chegou ao fim do formato (IEND, EOI, trailer do GIF)? Formatos sem marca de fim: sim */
int iio_has_end(const uint8_t *d, size_t n) {
    const char *t = iio_sniff(d, n);
    if (!t) return 0;
    if (!strcmp(t, "public.png")) return n >= 12 && !memcmp(d + n - 8, "IEND", 4);
    if (!strcmp(t, "public.jpeg")) { while (n > 2 && d[n - 1] != 0xD9) n--; return n > 2 && d[n - 2] == 0xFF; }
    if (!strcmp(t, "com.compuserve.gif")) { while (n > 6 && !d[n - 1]) n--; return d[n - 1] == 0x3B; }
    return 1;
}

void iio_image_free(iio_image *img) {
    if (!img) return;
    for (int i = 0; i < img->nframes; i++) kfree(img->f[i].px);
    kfree(img->f);
    kfree(img->icc);
    memset(img, 0, sizeof *img);
}

int iio_decode(const uint8_t *d, size_t n, iio_image *out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    const char *t = iio_sniff(d, n);
    int rc = -1;
    if (!t) return -1;
    if (!strcmp(t, "public.png")) rc = iio_png(d, n, out);
    else if (!strcmp(t, "public.jpeg")) rc = iio_jpeg(d, n, out);
    else if (!strcmp(t, "com.compuserve.gif")) rc = iio_gif(d, n, out);
    else if (!strcmp(t, "public.tiff")) rc = iio_tiff(d, n, out);
    else if (!strcmp(t, "com.microsoft.bmp")) rc = iio_bmp(d, n, out);
    else rc = iio_ico(d, n, out);
    if (rc) { iio_image_free(out); return -1; }
    if (!strcmp(t, "public.jpeg")) {
        /* EXIF do JPEG: orientação e densidade quando o JFIF não deu */
        mos_exif x;
        if (mos_exif_parse(d, n, &x) == 0) {
            const mos_exif_entry *e = mos_exif_find(&x, EXIF_IFD_TIFF, 0x0112);
            if (e && e->kind == EXIF_NUM && e->num[0] >= 1 && e->num[0] <= 8) out->f[0].orientation = (int)e->num[0];
            const mos_exif_entry *xr = mos_exif_find(&x, EXIF_IFD_TIFF, 0x011A), *yr = mos_exif_find(&x, EXIF_IFD_TIFF, 0x011B);
            const mos_exif_entry *ru = mos_exif_find(&x, EXIF_IFD_TIFF, 0x0128);
            double k = ru && ru->kind == EXIF_NUM && ru->num[0] == 3 ? 2.54 : 1;
            if (!out->f[0].dpi_x && xr && xr->kind == EXIF_NUM) out->f[0].dpi_x = xr->num[0] * k;
            if (!out->f[0].dpi_y && yr && yr->kind == EXIF_NUM) out->f[0].dpi_y = yr->num[0] * k;
        }
    }
    for (int i = 0; i < out->nframes; i++) if (!out->f[i].orientation) out->f[i].orientation = 1;
    return 0;
}

int iio_can_encode(const char *uti, int *multi) {
    for (int i = 0; iio_write_types[i]; i++)
        if (!strcmp(uti, iio_write_types[i])) {
            if (multi) *multi = i == 0 || i == 2 || i == 3;
            return 1;
        }
    return 0;
}

/* achata o alfa sobre a cor de fundo (para formatos sem alfa) */
static u32 *iio_flatten(const u32 *px, int w, int h, u32 bg) {
    u32 *o = kmalloc((usize)w * h * 4);
    if (!o) return NULL;
    for (usize i = 0; i < (usize)w * h; i++) {
        u32 c = px[i], a = c >> 24;
        if (a == 255) { o[i] = c; continue; }
        u32 r = 0xFF000000u;
        for (int s = 0; s < 24; s += 8) r |= ((((c >> s) & 255) * a + ((bg >> s) & 255) * (255 - a) + 127) / 255) << s;
        o[i] = r;
    }
    return o;
}

int iio_encode(const char *uti, const iio_enc_frame *fr, int n, const iio_enc_opts *o, uint8_t **out, size_t *on) {
    if (!uti || !fr || n < 1 || !out || !on) return -1;
    if (!fr[0].px || fr[0].w < 1 || fr[0].h < 1) return -1;
    if (!strcmp(uti, "public.png")) return iio_png_encode(fr, n, o, out, on);
    if (!strcmp(uti, "com.compuserve.gif")) return iio_gif_encode(fr, n, o, out, on);
    if (!strcmp(uti, "public.tiff")) return iio_tiff_encode(fr, n, o, out, on);
    if (!strcmp(uti, "com.microsoft.bmp")) return iio_bmp_encode(&fr[0], o, out, on);
    if (!strcmp(uti, "public.jpeg")) {
        int q = o && o->quality >= 0 ? (int)(o->quality * 100 + 0.5) : 85;
        if (q < 1) q = 1;
        if (q > 100) q = 100;
        u32 *flat = iio_flatten(fr[0].px, fr[0].w, fr[0].h, o ? o->background : 0xFFFFFFFFu);
        if (!flat) return -1;
        int rc = iio_jpeg_encode(flat, fr[0].w, fr[0].h, q, o && o->dpi_x > 0 ? (int)(o->dpi_x + 0.5) : 72,
                                 o ? o->orientation : 1, out, on);
        kfree(flat);
        return rc;
    }
    return -1;
}

uint32_t *iio_resize(const uint32_t *px, int w, int h, int nw, int nh) {
    if (!px || w < 1 || h < 1 || nw < 1 || nh < 1) return NULL;
    u32 *o = kmalloc((usize)nw * nh * 4);
    if (!o) return NULL;
    for (int y = 0; y < nh; y++) {
        int y0 = (int)((i64)y * h / nh), y1 = (int)((i64)(y + 1) * h / nh);
        if (y1 <= y0) y1 = y0 + 1;
        for (int x = 0; x < nw; x++) {
            int x0 = (int)((i64)x * w / nw), x1 = (int)((i64)(x + 1) * w / nw);
            if (x1 <= x0) x1 = x0 + 1;
            /* média pré-multiplicada para a cor não vazar dos pixels transparentes */
            u64 sa = 0, sr = 0, sg = 0, sb = 0, cnt = 0;
            for (int yy = y0; yy < y1; yy++)
                for (int xx = x0; xx < x1; xx++) {
                    u32 c = px[(usize)yy * w + xx], a = c >> 24;
                    sa += a; sr += ((c >> 16) & 255) * a; sg += ((c >> 8) & 255) * a; sb += (c & 255) * a; cnt++;
                }
            u32 a = (u32)((sa + cnt / 2) / cnt);
            u32 r = sa ? (u32)((sr + sa / 2) / sa) : 0, g = sa ? (u32)((sg + sa / 2) / sa) : 0, b = sa ? (u32)((sb + sa / 2) / sa) : 0;
            o[(usize)y * nw + x] = a << 24 | r << 16 | g << 8 | b;
        }
    }
    return o;
}

uint32_t *iio_orient(const uint32_t *px, int w, int h, int ori, int *ow, int *oh) {
    if (!px || w < 1 || h < 1) return NULL;
    int swap = ori >= 5 && ori <= 8;
    int W = swap ? h : w, H = swap ? w : h;
    u32 *o = kmalloc((usize)W * H * 4);
    if (!o) return NULL;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int dx, dy;
            switch (ori) {
            case 2: dx = w - 1 - x; dy = y; break;
            case 3: dx = w - 1 - x; dy = h - 1 - y; break;
            case 4: dx = x; dy = h - 1 - y; break;
            case 5: dx = y; dy = x; break;
            case 6: dx = h - 1 - y; dy = x; break;
            case 7: dx = h - 1 - y; dy = w - 1 - x; break;
            case 8: dx = y; dy = w - 1 - x; break;
            default: dx = x; dy = y; break;
            }
            o[(usize)dy * W + dx] = px[(usize)y * w + x];
        }
    *ow = W;
    *oh = H;
    return o;
}

/* ------------------------------------------------------------------ API antiga (CoreImage, Photos) */
const char *mos_img_uti(const uint8_t *data, size_t n) { return iio_sniff(data, n); }
void mos_img_free(mos_img_t *img) {
    if (!img) return;
    kfree(img->px);
    img->px = NULL;
    img->w = img->h = img->has_alpha = 0;
}
int mos_img_decode(const uint8_t *data, size_t n, mos_img_t *out) {
    iio_image im;
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    if (iio_decode(data, n, &im)) return -1;
    out->w = im.f[0].w;
    out->h = im.f[0].h;
    out->px = im.f[0].px;
    im.f[0].px = NULL;
    for (usize i = 0; i < (usize)out->w * out->h; i++) if ((out->px[i] >> 24) != 255) { out->has_alpha = 1; break; }
    iio_image_free(&im);
    return 0;
}
int mos_img_encode_png(const uint32_t *px, int w, int h, uint8_t **out, size_t *n) {
    iio_enc_frame f = {px, w, h, 0, 0};
    return iio_encode("public.png", &f, 1, NULL, out, n);
}
int mos_img_encode_jpeg(const uint32_t *px, int w, int h, int quality, uint8_t **out, size_t *n) {
    iio_enc_frame f = {px, w, h, 0, 0};
    iio_enc_opts o = {quality / 100.0, 1, 0, 0, 0, 0xFFFFFFFFu, 0};
    return iio_encode("public.jpeg", &f, 1, &o, out, n);
}
