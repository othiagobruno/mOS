/* mOS PDF — imagens (XObject Image e imagem inline, ISO 32000-1 §8.9) para CGImage. Incluído por
 * cgpdf_draw.c. Máscara de estêncil (ImageMask), máscara explícita (Mask stream), por cor (Mask array),
 * máscara suave (SMask), Decode, Indexed, Separation/DeviceN (convertidos para o alternativo), DCTDecode
 * pelo decodificador do ImageIO e CCITTFaxDecode (pdf_ccitt.inc). */

void *CGImageCreate(size_t w, size_t h, size_t bpc, size_t bpp, size_t bpr, const void *space, u32 info,
                    void *provider, const double *decode, bool interp, int intent);
void *CGImageMaskCreate(size_t w, size_t h, size_t bpc, size_t bpp, size_t bpr, void *provider, const double *decode, bool interp);
void *CGImageCreateWithMask(void *img, void *mask);
void *CGImageCreateWithMaskingColors(void *img, const double *comps);
void CGImageRelease(void *img);
int cx_cgimage_src(const void *img, const u8 **px, long *bpr, int *w, int *h, int *premul);
typedef struct { int w, h, has_alpha; u32 *px; } pimg_codec;
int mos_img_decode(const u8 *data, size_t n, pimg_codec *out);
int pdf_ccitt_decode(const u8 *in, size_t n, int k, int cols, int rows, int black1, int byte_align, int eob, u8 **out, size_t *on);

/* chave do dicionário de imagem, com as abreviações da imagem inline */
static pdf_obj *idict(pdf_doc *pd, pdf_obj *d, const char *full, const char *abbr) {
    pdf_obj *v = pdf_dict_get(pd, d, full);
    return v ? v : abbr ? pdf_dict_get(pd, d, abbr) : 0;
}
static void free_cb(void *info, const void *data, size_t n) { (void)info; (void)n; free((void *)data); }

/* bytes de amostra da imagem (filtros aplicados, inclusive CCITT); *jpeg = 1 se ficou JPEG por decodificar */
static u8 *pimg_samples(pdf_doc *pd, pdf_obj *s, size_t *n, int *jpeg, int w, int h) {
    const char *imgf = 0;
    size_t len;
    const u8 *d = pdf_stream_data(pd, s, &len, &imgf);
    *jpeg = 0;
    if (!d) return 0;
    if (imgf && !strcmp(imgf, "DCTDecode")) { *jpeg = 1; u8 *c = malloc(len); if (c) memcpy(c, d, len); *n = len; return c; }
    if (imgf && !strcmp(imgf, "CCITTFaxDecode")) {
        pdf_obj *p = idict(pd, s, "DecodeParms", "DP");
        if (p && p->type == PO_ARRAY) p = pdf_array_get(pd, p, p->n - 1);
        double k = 0, cols = 1728, rows = h, b1 = 0, ba = 0, eob = 1;
        pdf_num(pdf_dict_get(pd, p, "K"), &k);
        pdf_num(pdf_dict_get(pd, p, "Columns"), &cols);
        pdf_num(pdf_dict_get(pd, p, "Rows"), &rows);
        pdf_obj *bo = pdf_dict_get(pd, p, "BlackIs1"), *eo = pdf_dict_get(pd, p, "EncodedByteAlign"), *eb = pdf_dict_get(pd, p, "EndOfBlock");
        if (bo && bo->type == PO_BOOL) b1 = bo->v.b;
        if (eo && eo->type == PO_BOOL) ba = eo->v.b;
        if (eb && eb->type == PO_BOOL) eob = eb->v.b;
        (void)w;
        u8 *o = 0;
        if (pdf_ccitt_decode(d, len, (int)k, (int)cols, (int)rows, (int)b1, (int)ba, (int)eob, &o, n)) return 0;
        return o;
    }
    if (imgf) return 0;                              /* JPXDecode, JBIG2Decode: ver docs (não decodificados) */
    u8 *c = malloc(len ? len : 1);
    if (c) memcpy(c, d, len);
    *n = len;
    return c;
}

/* imagem num espaço que o CG não representa (Separation/DeviceN, Indexed sobre eles): RGB 8 bits na mão */
static void *pimg_convert(const pcs *cs, const u8 *d, size_t n, int w, int h, int bpc, const double *decode) {
    int nc = cs->n;
    size_t bpr = ((size_t)w * nc * bpc + 7) / 8;
    if (bpr * (size_t)h > n) return 0;
    u8 *rgb = malloc((size_t)w * h * 4);
    if (!rgb) return 0;
    void *dev = CGColorSpaceCreateDeviceRGB();
    double maxv = (double)((1u << bpc) - 1);
    /* cache para 1 componente de 8 bits ou menos (o caso comum: Separation) */
    u32 lut[256];
    int use_lut = nc == 1 && bpc <= 8;
    if (use_lut)
        for (int v = 0; v <= (int)maxv; v++) {
            double x = decode ? decode[0] + v * (decode[1] - decode[0]) / maxv : (cs->kind == PCS_INDEXED ? v : v / maxv), o[PF_MAXOUT];
            pcs *t = pcs_cgcomps(cs, &x, o);
            void *col = t && t->cg ? (o[t->n] = 1, CGColorCreate(t->cg, o)) : 0;
            const double *rr = col ? (const double *)((u8 *)col + 8) : 0;     /* r,g,b em sRGB do CGColor do mOS */
            lut[v] = rr ? (u32)(rr[0] * 255 + .5) << 16 | (u32)(rr[1] * 255 + .5) << 8 | (u32)(rr[2] * 255 + .5) : 0;
            if (col) CFRelease(col);
        }
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            u32 c;
            if (use_lut) {
                size_t bit = (size_t)y * bpr * 8 + (size_t)x * bpc;
                u32 v = bpc == 8 ? d[bit / 8] : (d[bit / 8] >> (8 - bpc - bit % 8)) & ((1u << bpc) - 1);
                c = lut[v];
            } else {
                double in[PF_MAXOUT], o[PF_MAXOUT];
                for (int k = 0; k < nc && k < PF_MAXOUT; k++) {
                    size_t bit = (size_t)y * bpr * 8 + ((size_t)x * nc + k) * bpc;
                    u32 v = bpc == 8 ? d[bit / 8] : bpc == 16 ? (u32)(d[bit / 8] << 8 | d[bit / 8 + 1]) : (d[bit / 8] >> (8 - bpc - bit % 8)) & ((1u << bpc) - 1);
                    double mv = bpc == 16 ? 65535.0 : maxv;
                    in[k] = decode ? decode[2 * k] + v * (decode[2 * k + 1] - decode[2 * k]) / mv : v / mv;
                }
                pcs *t = pcs_cgcomps(cs, in, o);
                void *col = t && t->cg ? (o[t->n] = 1, CGColorCreate(t->cg, o)) : 0;
                const double *rr = col ? (const double *)((u8 *)col + 8) : 0;
                c = rr ? (u32)(rr[0] * 255 + .5) << 16 | (u32)(rr[1] * 255 + .5) << 8 | (u32)(rr[2] * 255 + .5) : 0;
                if (col) CFRelease(col);
            }
            u8 *p = rgb + ((size_t)y * w + x) * 4;
            p[0] = (u8)(c >> 16); p[1] = (u8)(c >> 8); p[2] = (u8)c; p[3] = 255;
        }
    void *prov = CGDataProviderCreateWithData(0, rgb, (size_t)w * h * 4, free_cb);
    void *img = prov ? CGImageCreate((size_t)w, (size_t)h, 8, 32, (size_t)w * 4, dev, 5 /* NoneSkipLast */, prov, 0, 1, 0) : 0;
    if (prov) CGDataProviderRelease(prov); else free(rgb);
    CGColorSpaceRelease(dev);
    return img;
}

/* máscara suave: alfa = luminância da imagem SMask (esticada), aplicada como alfa reto; com /Matte a cor veio
 * pré-misturada com o fundo matte: c = m + (c' - m) / a */
static void *pimg_apply_smask(void *img, void *smask, const double *matte) {
    const u8 *ip, *mp;
    long ib, mb;
    int iw, ih, mw, mh, ipm, mpm;
    if (!cx_cgimage_src(img, &ip, &ib, &iw, &ih, &ipm) || !cx_cgimage_src(smask, &mp, &mb, &mw, &mh, &mpm)) return 0;
    u32 *out = malloc((size_t)iw * ih * 4);
    if (!out) return 0;
    for (int y = 0; y < ih; y++) {
        const u32 *ir = (const u32 *)(ip + (size_t)y * ib);
        const u32 *mr = (const u32 *)(mp + (size_t)((long)y * mh / ih) * mb);
        for (int x = 0; x < iw; x++) {
            u32 p = ir[x], a0 = p >> 24, r = (p >> 16) & 255, g = (p >> 8) & 255, b = p & 255;
            if (ipm && a0 && a0 < 255) { r = r * 255 / a0; g = g * 255 / a0; b = b * 255 / a0; }
            u32 m = mr[(long)x * mw / iw];
            u32 a = (((m >> 16) & 255) * 77 + ((m >> 8) & 255) * 150 + (m & 255) * 29 + 128) >> 8;
            a = a * a0 / 255;
            if (matte && a) {
                u32 *ch[3] = {&r, &g, &b};
                for (int k = 0; k < 3; k++) {
                    double mv = matte[k] * 255, v = mv + ((double)*ch[k] - mv) * 255.0 / a;
                    *ch[k] = (u32)(v < 0 ? 0 : v > 255 ? 255 : v + 0.5);
                }
            }
            out[(size_t)y * iw + x] = a << 24 | r << 16 | g << 8 | b;
        }
    }
    void *prov = CGDataProviderCreateWithData(0, out, (size_t)iw * ih * 4, free_cb);
    bool CGImageGetShouldInterpolate(void *img);
    void *res = prov ? CGImageCreate((size_t)iw, (size_t)ih, 8, 32, (size_t)iw * 4, 0, 4 | 0x2000 /* First, 32 little */, prov, 0,
                                     CGImageGetShouldInterpolate(img), 0) : 0;
    if (prov) CGDataProviderRelease(prov); else free(out);
    return res;
}

static void *pimg_create(pdf_doc *pd, pdf_obj *s, pdf_obj *res, int depth, int *is_mask);
static void *pimg_base(pdf_doc *pd, pdf_obj *s, pdf_obj *res, int depth, int *is_mask) {
    double w = 0, h = 0, bpc = 8;
    pdf_num(idict(pd, s, "Width", "W"), &w);
    pdf_num(idict(pd, s, "Height", "H"), &h);
    pdf_num(idict(pd, s, "BitsPerComponent", "BPC"), &bpc);
    pdf_obj *im = idict(pd, s, "ImageMask", "IM"), *intp = idict(pd, s, "Interpolate", "I");
    int mask = im && im->type == PO_BOOL && im->v.b, interp = intp && intp->type == PO_BOOL && intp->v.b;
    *is_mask = mask;
    if (w < 1 || h < 1 || w > 30000 || h > 30000) return 0;
    if (mask) bpc = 1;
    double dec[2 * PF_MAXOUT];
    int nd = pf_nums(pd, idict(pd, s, "Decode", "D"), dec, 2 * PF_MAXOUT);
    size_t n;
    int jpeg;
    u8 *d = pimg_samples(pd, s, &n, &jpeg, (int)w, (int)h);
    if (!d) return 0;
    if (jpeg) {
        pimg_codec c;
        if (mos_img_decode(d, n, &c)) { free(d); return 0; }
        free(d);
        /* Decode [1 0] num JPEG cinza/RGB (raro): inverte */
        if (nd >= 2 && dec[0] == 1 && dec[1] == 0)
            for (size_t i = 0; i < (size_t)c.w * c.h; i++) c.px[i] = (c.px[i] & 0xFF000000u) | (~c.px[i] & 0xFFFFFFu);
        void *prov = CGDataProviderCreateWithData(0, c.px, (size_t)c.w * c.h * 4, free_cb);
        void *dev = CGColorSpaceCreateDeviceRGB();
        void *img = prov ? CGImageCreate((size_t)c.w, (size_t)c.h, 8, 32, (size_t)c.w * 4, dev, 6 | 0x2000, prov, 0, interp, 0) : 0;
        if (prov) CGDataProviderRelease(prov);
        CGColorSpaceRelease(dev);
        return img;
    }
    if (mask) {
        size_t bpr = ((size_t)w + 7) / 8;
        if (bpr * (size_t)h > n) { free(d); return 0; }
        void *prov = CGDataProviderCreateWithData(0, d, n, free_cb);
        double dm[2] = {nd >= 2 ? dec[0] : 0, nd >= 2 ? dec[1] : 1};
        void *img = prov ? CGImageMaskCreate((size_t)w, (size_t)h, 1, 1, bpr, prov, dm, interp) : 0;
        if (prov) CGDataProviderRelease(prov); else free(d);
        return img;
    }
    pcs *cs = pcs_resolve(pd, idict(pd, s, "ColorSpace", "CS"), res, depth);
    if (!cs) cs = pcs_device("DeviceGray");
    if (!cs) { free(d); return 0; }
    void *img = 0;
    int nc = cs->n ? cs->n : 1;
    size_t bpr = ((size_t)w * nc * (size_t)bpc + 7) / 8;
    int cg_ok = cs->cg && cs->kind != PCS_SEP && (cs->kind != PCS_INDEXED || cs->cg);
    if (cg_ok && bpr * (size_t)h <= n) {
        void *prov = CGDataProviderCreateWithData(0, d, n, free_cb);
        img = prov ? CGImageCreate((size_t)w, (size_t)h, (size_t)bpc, (size_t)(nc * bpc), bpr, cs->cg, 0, prov, nd >= 2 * nc ? dec : 0, interp, 0) : 0;
        if (prov) CGDataProviderRelease(prov); else free(d);
        d = 0;
    } else img = pimg_convert(cs, d, n, (int)w, (int)h, (int)bpc, nd >= 2 * nc ? dec : 0);
    free(d);
    /* Mask: array de faixas de cor (por componente, em valores de amostra) ou stream de estêncil */
    pdf_obj *m = pdf_dict_get(pd, s, "Mask");
    if (img && m && m->type == PO_ARRAY) {
        double r[2 * PF_MAXOUT];
        pf_nums(pd, m, r, 2 * PF_MAXOUT);
        void *mi = CGImageCreateWithMaskingColors(img, r);
        if (mi) { CGImageRelease(img); img = mi; }
    } else if (img && m && m->type == PO_STREAM) {
        int dummy;
        void *mk = pimg_create(pd, m, res, depth + 1, &dummy);
        if (mk) {
            void *mi = CGImageCreateWithMask(img, mk);
            CGImageRelease(mk);
            if (mi) { CGImageRelease(img); img = mi; }
        }
    }
    pcs_free(cs);
    return img;
}
static void *pimg_create(pdf_doc *pd, pdf_obj *s, pdf_obj *res, int depth, int *is_mask) {
    if (depth > 4) return 0;
    void *img = pimg_base(pd, s, res, depth, is_mask);
    pdf_obj *sm = pdf_dict_get(pd, s, "SMask");
    if (img && !*is_mask && sm && sm->type == PO_STREAM) {
        int dm;
        void *mimg = pimg_base(pd, sm, res, depth + 1, &dm);
        if (mimg) {
            /* /Matte nas componentes do espaço da imagem → RGB */
            double mt[PF_MAXOUT], mrgb[3], *mp = 0;
            pdf_obj *mo = pdf_dict_get(pd, sm, "Matte");
            if (mo && mo->type == PO_ARRAY && pf_nums(pd, mo, mt, PF_MAXOUT) > 0) {
                pcs *ics = pcs_resolve(pd, idict(pd, s, "ColorSpace", "CS"), res, 0);
                void *k = ics ? pcs_color(ics, mt, 1) : 0;
                if (k) { const double *o = k; mrgb[0] = o[1]; mrgb[1] = o[2]; mrgb[2] = o[3]; mp = mrgb; CFRelease(k); }
                pcs_free(ics);
            }
            void *r = pimg_apply_smask(img, mimg, mp);
            CGImageRelease(mimg);
            if (r) { CGImageRelease(img); img = r; }
        }
    }
    return img;
}
