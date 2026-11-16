/* mOS PDF — espaços de cor (ISO 32000-1 §8.6) para o CoreGraphics. Incluído por cgpdf_draw.c depois de
 * pdf_func.inc. Os que o CG representa (Device*, Cal*, Lab, ICCBased, Indexed, Pattern) viram CGColorSpace;
 * Separation e DeviceN passam pela função de tinta até o espaço alternativo. */

enum { PCS_GRAY, PCS_RGB, PCS_CMYK, PCS_CG, PCS_INDEXED, PCS_SEP, PCS_PATTERN };
typedef struct pcs pcs;
struct pcs {
    int kind, n;
    void *cg;                     /* espaço do CG (NULL em Separation/DeviceN) */
    pcs *alt, *base;              /* Separation/DeviceN: alternativo; Indexed/Pattern: base */
    pf_set tint;
    u8 *lookup;
    int hival;
    double range[8];
    int lab, none;                /* Separation /None: não pinta */
    pdf_obj *key;
};
typedef struct { pcs *v[64]; int n; } pcs_cache;

void *CGColorSpaceCreateDeviceGray(void);
void *CGColorSpaceCreateDeviceRGB(void);
void *CGColorSpaceCreateDeviceCMYK(void);
void *CGColorSpaceCreateCalibratedGray(const double *w, const double *b, double g);
void *CGColorSpaceCreateCalibratedRGB(const double *w, const double *b, const double *g, const double *m);
void *CGColorSpaceCreateLab(const double *w, const double *b, const double *range);
void *CGColorSpaceCreateICCBased(size_t n, const double *range, void *profile, void *alt);
void *CGColorSpaceCreateIndexed(void *base, size_t last, const u8 *table);
void *CGColorSpaceCreatePattern(void *base);
void CGColorSpaceRelease(void *s);
void *CGColorSpaceRetain(void *s);
void *CGColorCreate(const void *space, const double *comps);
void *CGDataProviderCreateWithData(void *info, const void *data, size_t size, void *release);
void CGDataProviderRelease(void *p);

static pcs *pcs_new(int kind, int n) {
    pcs *c = calloc(1, sizeof *c);
    if (c) { c->kind = kind; c->n = n; }
    return c;
}
static void pcs_free(pcs *c) {
    if (!c) return;
    if (c->cg) CGColorSpaceRelease(c->cg);
    pcs_free(c->alt);
    pcs_free(c->base);
    pfs_free(&c->tint);
    free(c->lookup);
    free(c);
}
static pcs *pcs_device(const char *n) {
    if (!n) return 0;
    if (!strcmp(n, "DeviceGray") || !strcmp(n, "G") || !strcmp(n, "CalGray")) { pcs *c = pcs_new(PCS_GRAY, 1); if (c) c->cg = CGColorSpaceCreateDeviceGray(); return c; }
    if (!strcmp(n, "DeviceRGB") || !strcmp(n, "RGB") || !strcmp(n, "CalRGB")) { pcs *c = pcs_new(PCS_RGB, 3); if (c) c->cg = CGColorSpaceCreateDeviceRGB(); return c; }
    if (!strcmp(n, "DeviceCMYK") || !strcmp(n, "CMYK")) { pcs *c = pcs_new(PCS_CMYK, 4); if (c) c->cg = CGColorSpaceCreateDeviceCMYK(); return c; }
    if (!strcmp(n, "Pattern")) return pcs_new(PCS_PATTERN, 0);
    return 0;
}
static pcs *pcs_load(pdf_doc *pd, pdf_obj *o, pdf_obj *res, int depth);
/* nome de recurso ou objeto direto */
static pcs *pcs_resolve(pdf_doc *pd, pdf_obj *o, pdf_obj *res, int depth) {
    o = pdf_resolve(pd, o);
    if (!o || depth > 8) return 0;
    if (o->type == PO_NAME) {
        pcs *d = pcs_device(o->v.name);
        if (d) return d;
        pdf_obj *named = pdf_dict_get(pd, pdf_dict_get(pd, res, "ColorSpace"), o->v.name);
        return named ? pcs_load(pd, named, res, depth + 1) : 0;
    }
    return pcs_load(pd, o, res, depth + 1);
}
static pcs *pcs_load(pdf_doc *pd, pdf_obj *o, pdf_obj *res, int depth) {
    o = pdf_resolve(pd, o);
    if (!o || depth > 8) return 0;
    if (o->type == PO_NAME) return pcs_resolve(pd, o, res, depth);
    if (o->type != PO_ARRAY || !o->n) return 0;
    const char *fam = pdf_name(pdf_array_get(pd, o, 0));
    if (!fam) return 0;
    pdf_obj *a1 = pdf_array_get(pd, o, 1);
    double wp[3] = {0.9505, 1, 1.089}, bp[3] = {0, 0, 0};
    pf_nums(pd, pdf_dict_get(pd, a1, "WhitePoint"), wp, 3);
    pf_nums(pd, pdf_dict_get(pd, a1, "BlackPoint"), bp, 3);
    if (!strcmp(fam, "CalGray")) {
        double g = 1;
        pdf_num(pdf_dict_get(pd, a1, "Gamma"), &g);
        pcs *c = pcs_new(PCS_CG, 1);
        if (c) c->cg = CGColorSpaceCreateCalibratedGray(wp, bp, g);
        return c;
    }
    if (!strcmp(fam, "CalRGB")) {
        double g[3] = {1, 1, 1}, m[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
        pf_nums(pd, pdf_dict_get(pd, a1, "Gamma"), g, 3);
        pf_nums(pd, pdf_dict_get(pd, a1, "Matrix"), m, 9);
        pcs *c = pcs_new(PCS_CG, 3);
        if (c) c->cg = CGColorSpaceCreateCalibratedRGB(wp, bp, g, m);
        return c;
    }
    if (!strcmp(fam, "Lab")) {
        double r[4] = {-100, 100, -100, 100};
        pf_nums(pd, pdf_dict_get(pd, a1, "Range"), r, 4);
        pcs *c = pcs_new(PCS_CG, 3);
        if (!c) return 0;
        c->cg = CGColorSpaceCreateLab(wp, bp, r);
        c->lab = 1;
        c->range[0] = 0; c->range[1] = 100;
        for (int i = 0; i < 4; i++) c->range[2 + i] = r[i];
        return c;
    }
    if (!strcmp(fam, "ICCBased")) {
        double nn = 3;
        pdf_num(pdf_dict_get(pd, a1, "N"), &nn);
        pcs *alt = pdf_dict_get_raw(a1, "Alternate") ? pcs_resolve(pd, pdf_dict_get_raw(a1, "Alternate"), res, depth + 1) : 0;
        size_t len;
        const u8 *icc = pdf_stream_data(pd, a1, &len, 0);
        void *prov = icc ? CGDataProviderCreateWithData(0, icc, len, 0) : 0;
        double r[8] = {0, 1, 0, 1, 0, 1, 0, 1};
        pf_nums(pd, pdf_dict_get(pd, a1, "Range"), r, 8);
        void *cg = CGColorSpaceCreateICCBased((size_t)nn, r, prov, alt ? alt->cg : 0);
        if (prov) CGDataProviderRelease(prov);
        pcs_free(alt);
        int n = (int)nn;
        pcs *c = pcs_new(n == 1 ? PCS_GRAY : n == 4 ? PCS_CMYK : PCS_CG, n);
        if (c) c->cg = cg;
        else if (cg) CGColorSpaceRelease(cg);
        return c;
    }
    if (!strcmp(fam, "Indexed") || !strcmp(fam, "I")) {
        pcs *base = pcs_resolve(pd, a1, res, depth + 1);
        double hv = 0;
        pdf_num(pdf_array_get(pd, o, 2), &hv);
        pdf_obj *lk = pdf_array_get(pd, o, 3);
        if (!base || hv < 0 || hv > 255) { pcs_free(base); return 0; }
        int nb = base->n, hi = (int)hv;
        size_t need = (size_t)(hi + 1) * (size_t)nb, have = 0;
        const u8 *tab = 0;
        if (lk && lk->type == PO_STRING) { tab = lk->v.str; have = lk->n; }
        else if (lk && lk->type == PO_STREAM) tab = pdf_stream_data(pd, lk, &have, 0);
        pcs *c = pcs_new(PCS_INDEXED, 1);
        if (!c) { pcs_free(base); return 0; }
        c->base = base;
        c->hival = hi;
        c->lookup = calloc(need ? need : 1, 1);
        if (c->lookup && tab) memcpy(c->lookup, tab, have < need ? have : need);
        if (base->cg && base->kind != PCS_SEP) c->cg = CGColorSpaceCreateIndexed(base->cg, (size_t)hi, c->lookup);
        return c;
    }
    if (!strcmp(fam, "Separation") || !strcmp(fam, "DeviceN")) {
        int dn = !strcmp(fam, "DeviceN");
        pdf_obj *names = a1;
        int n = dn ? (int)(names && names->type == PO_ARRAY ? names->n : 1) : 1;
        pcs *c = pcs_new(PCS_SEP, n);
        if (!c) return 0;
        const char *nm = dn ? 0 : pdf_name(names);
        if (nm && !strcmp(nm, "None")) c->none = 1;
        if (dn && names && names->type == PO_ARRAY) {
            int all_none = 1;
            for (u32 i = 0; i < names->n; i++) { const char *x = pdf_name(pdf_array_get(pd, names, i)); if (!x || strcmp(x, "None")) all_none = 0; }
            c->none = all_none;
        }
        c->alt = pcs_resolve(pd, pdf_array_get(pd, o, 2) ? o->v.arr[2] : 0, res, depth + 1);
        pfs_load(pd, o->n > 3 ? o->v.arr[3] : 0, &c->tint);
        if (!c->alt) { pcs_free(c); return 0; }
        return c;
    }
    if (!strcmp(fam, "Pattern")) {
        pcs *c = pcs_new(PCS_PATTERN, 0);
        if (c && o->n > 1) { c->base = pcs_resolve(pd, a1, res, depth + 1); if (c->base) c->n = c->base->n; }
        return c;
    }
    return pcs_device(fam);
}
/* componentes do espaço → componentes de um espaço que o CG representa (recursivo pela tinta) */
static pcs *pcs_cgcomps(const pcs *c, const double *in, double *out) {
    if (!c) return 0;
    if (c->kind == PCS_SEP) {
        double t[PF_MAXOUT] = {0};
        pfs_eval(&c->tint, in, t);
        return pcs_cgcomps(c->alt, t, out);
    }
    if (c->kind == PCS_INDEXED && !c->cg) {
        int i = (int)(in[0] + 0.5);
        if (i < 0) i = 0;
        if (i > c->hival) i = c->hival;
        double b[PF_MAXOUT];
        for (int k = 0; k < c->base->n; k++) b[k] = c->lookup[i * c->base->n + k] / 255.0;
        return pcs_cgcomps(c->base, b, out);
    }
    for (int k = 0; k < c->n; k++) out[k] = in[k];
    return (pcs *)c;
}
/* CGColor para os componentes (com alfa); NULL se o espaço não pinta */
static void *pcs_color(const pcs *c, const double *in, double alpha) {
    if (!c || c->none) return 0;
    double v[PF_MAXOUT + 1];
    pcs *t = pcs_cgcomps(c, in, v);
    if (!t || !t->cg) return 0;
    v[t->n] = alpha;
    return CGColorCreate(t->cg, v);
}
/* cor inicial do espaço (ISO 32000 §8.6.5.x): preto, tinta 1, índice 0, L/a/b recortado */
static void pcs_initial(const pcs *c, double *v) {
    for (int i = 0; i < 8; i++) v[i] = 0;
    if (!c) return;
    if (c->kind == PCS_CMYK) v[3] = 1;
    if (c->kind == PCS_SEP) for (int i = 0; i < c->n && i < 8; i++) v[i] = 1;
    if (c->lab) { v[1] = c->range[2] > 0 ? c->range[2] : c->range[3] < 0 ? c->range[3] : 0; v[2] = c->range[4] > 0 ? c->range[4] : c->range[5] < 0 ? c->range[5] : 0; }
}
