/* CoreGraphics do mOS — espaços de cor e CGColor em qualquer espaço (incluído pelo coregraphics.c depois
 * de cgspace_t e antes de cg_pixel.inc). A conversão é o motor do ColorSync (colorsync.c): o mOS desenha
 * em sRGB, então imagem e cor de qualquer espaço viram sRGB na entrada; o original fica guardado
 * (componentes da cor, bytes do provedor da imagem, ICC do espaço). */
#include "colorsync.h"

const u8 *cx_colorsync_profile_bytes(const void *prof, size_t *n);
const u8 *CFDataGetBytePtr(id d);
long CFDataGetLength(id d);
void *CFDataCreate(void *alloc, const u8 *bytes, long n);
void *CGDataProviderCopyData(void *p);
void cx_object_free(id o);
extern const void *kCGColorSpaceITUR_2020;

/* perfil do motor para o espaço; NULL = já é sRGB (ou equivalente) e não há o que converter */
static const cs_prof *space_prof(cgspace_t *s) {
    if (!s) return 0;
    if (s->prof) return s->prof_srgb ? 0 : (const cs_prof *)s->prof;
    if (s->model == 5 || s->model == 6) return 0;       /* indexado/padrão: o base resolve */
    cs_prof *p = calloc(1, sizeof *p);
    if (!p) return 0;
    int ok = -1;
    if (s->icc && s->nicc) ok = cs_parse(s->icc, (size_t)s->nicc, p);
    else if (s->calib == 1) { cs_calibrated_rgb(p, s->white, s->gamma, s->matrix); ok = 0; }
    else if (s->calib == 2) { cs_calibrated_gray(p, s->white, s->gamma[0]); ok = 0; }
    else if (s->calib == 3) { memset(p, 0, sizeof *p); cs_lab(p, s->white, s->range); ok = 0; }
    else if (s->name) ok = cs_builtin(cfstr_c(s->name), p);
    else if (s->model == 2) ok = cs_builtin("CMYK", p);
    if (ok) { free(p); s->prof = (void *)1; s->prof_srgb = 1; return 0; }
    s->prof = p;
    /* Device*, sRGB, Generic RGB e as versões estendidas do sRGB não precisam de conversão */
    s->prof_srgb = s->model == 1 && cs_is_srgb(p);
    return s->prof_srgb ? 0 : p;
}
/* componentes do espaço → sRGB 0..1 (Lab: L 0..100, a/b crus) */
static void space_to_srgb(cgspace_t *s, const double *in, double *rgb) {
    const cs_prof *p = space_prof(s);
    if (p) { cs_to_srgb(p, in, rgb); return; }
    int model = s ? s->model : 1;
    if (model == 0) rgb[0] = rgb[1] = rgb[2] = in[0];
    else if (model == 2) { rgb[0] = (1 - in[0]) * (1 - in[3]); rgb[1] = (1 - in[1]) * (1 - in[3]); rgb[2] = (1 - in[2]) * (1 - in[3]); }
    else { rgb[0] = in[0]; rgb[1] = in[1]; rgb[2] = in[2]; }
}
/* ponte para cg_pixel.inc: transformação por pixel quando o espaço precisa */
static void unpack_xf(const void *ctx, const double *in, double *rgb) { space_to_srgb((cgspace_t *)ctx, in, rgb); }

static cgspace_t *space_new(int model, int ncomp) {
    void *any = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);   /* cria a classe se ainda não existe */
    if (any) CGColorSpaceRelease(any);
    cgspace_t *s = (cgspace_t *)class_createInstance(space_cls, 0);
    if (!s) return 0;
    s->model = model;
    s->ncomp = (u32)ncomp;
    return s;
}
CSNAME(kCGColorSpaceGenericCMYK, "kCGColorSpaceGenericCMYK")
CSNAME(kCGColorSpaceGenericLab, "kCGColorSpaceGenericLab")
CSNAME(kCGColorSpaceGenericXYZ, "kCGColorSpaceGenericXYZ")
CSNAME(kCGColorSpaceAdobeRGB1998, "kCGColorSpaceAdobeRGB1998")
CSNAME(kCGColorSpaceACESCGLinear, "kCGColorSpaceACESCGLinear")
CSNAME(kCGColorSpaceROMMRGB, "kCGColorSpaceROMMRGB")
CSNAME(kCGColorSpaceGenericRGBLinear, "kCGColorSpaceGenericRGBLinear")
CSNAME(kCGColorSpaceDisplayP3_HLG, "kCGColorSpaceDisplayP3_HLG")
CSNAME(kCGColorSpaceDisplayP3_PQ, "kCGColorSpaceDisplayP3_PQ")
CSNAME(kCGColorSpaceITUR_2100_PQ_EOTF, "kCGColorSpaceITUR_2100_PQ")
CSNAME(kCGColorSpaceExtendedITUR_2020, "kCGColorSpaceExtendedITUR_2020")
CSNAME(kCGColorSpaceExtendedLinearITUR_2020, "kCGColorSpaceExtendedLinearITUR_2020")
CSNAME(kCGColorSpaceLinearITUR_2020, "kCGColorSpaceLinearITUR_2020")
CSNAME(kCGColorSpaceITUR_709_PQ, "kCGColorSpaceITUR_709_PQ")
CSNAME(kCGColorSpaceITUR_709_HLG, "kCGColorSpaceITUR_709_HLG")
CSNAME(kCGColorSpaceITUR_2020_sRGBGamma, "kCGColorSpaceITUR_2020_sRGBGamma")
CSNAME(kCGColorSpaceCoreMedia709, "kCGColorSpaceCoreMedia709")

void *CGColorSpaceCreateDeviceCMYK(void) { return CGColorSpaceCreateWithName(kCGColorSpaceGenericCMYK); }
/* Lab: branco (XYZ), preto e o intervalo de a e b [amin amax bmin bmax] */
void *CGColorSpaceCreateLab(const CGFloat *white, const CGFloat *black, const CGFloat *range) {
    if (!white || white[1] <= 0) return 0;
    cgspace_t *s = space_new(3, 3);
    if (!s) return 0;
    s->calib = 3;
    for (int i = 0; i < 3; i++) { s->white[i] = white[i]; s->black[i] = black ? black[i] : 0; }
    for (int i = 0; i < 4; i++) s->range[i] = range ? range[i] : (i & 1 ? 100 : -100);
    return s;
}
void *CGColorSpaceCreateIndexed(void *base, size_t lastIndex, const u8 *table) {
    cgspace_t *b = base;
    if (!b || !table || lastIndex > 255 || b->model == 5 || b->model == 6) return 0;
    cgspace_t *s = space_new(5, 1);
    if (!s) return 0;
    size_t n = (lastIndex + 1) * b->ncomp;
    s->table = malloc(n);
    if (!s->table) { objc_release((id)s); return 0; }
    memcpy(s->table, table, n);
    s->ntable = (int)lastIndex + 1;
    s->base = objc_retain((id)base);
    return s;
}
size_t CGColorSpaceGetColorTableCount(const void *s) { const cgspace_t *c = s; return c && c->model == 5 ? (size_t)c->ntable : 0; }
void CGColorSpaceGetColorTable(const void *s, u8 *out) {
    const cgspace_t *c = s;
    if (!c || c->model != 5 || !out) return;
    memcpy(out, c->table, (size_t)c->ntable * ((const cgspace_t *)c->base)->ncomp);
}
static int icc_model(const u8 *b, long n, int *ncomp) {
    if (n < 20) return -1;
    u32 sp = (u32)b[16] << 24 | (u32)b[17] << 16 | (u32)b[18] << 8 | b[19];
    switch (sp) {
    case 0x47524159u: *ncomp = 1; return 0;            /* GRAY */
    case 0x52474220u: *ncomp = 3; return 1;            /* RGB  */
    case 0x434D594Bu: *ncomp = 4; return 2;            /* CMYK */
    case 0x4C616220u: *ncomp = 3; return 3;            /* Lab  */
    case 0x58595A20u: *ncomp = 3; return 7;            /* XYZ  */
    default: *ncomp = (int)(sp >> 24) - '0'; return *ncomp >= 2 && *ncomp <= 15 && (sp & 0xFFFFFF) == 0x434C52u ? 4 : -1;   /* nCLR */
    }
}
/* espaço de um perfil ICC: o motor tem de conseguir ler, senão NULL (como o macOS com perfil inválido) */
void *CGColorSpaceCreateWithICCData(id data) {
    const u8 *b = 0;
    long n = 0;
    id copy = 0;
    if (!data) return 0;
    if (as_kind(data, CGK_PR)) { copy = (id)CGDataProviderCopyData(data); data = copy; }
    if (data) { b = CFDataGetBytePtr(data); n = CFDataGetLength(data); }
    cs_prof probe;
    int nc = 0, model = b ? icc_model(b, n, &nc) : -1;
    if (model < 0 || cs_parse(b, (size_t)n, &probe)) { if (copy) objc_release(copy); return 0; }
    cs_free(&probe);
    cgspace_t *s = space_new(model, nc);
    if (s) {
        s->icc = malloc((size_t)n);
        if (s->icc) { memcpy(s->icc, b, (size_t)n); s->nicc = n; }
    }
    if (copy) objc_release(copy);
    return s;
}
/* ICCBased (PDF): n componentes, faixa, provedor com o perfil, espaço alternativo se o perfil não serve */
void *CGColorSpaceCreateICCBased(size_t n, const CGFloat *range, void *profile, void *alternate) {
    void *s = profile ? CGColorSpaceCreateWithICCData((id)profile) : 0;
    if (s && ((cgspace_t *)s)->ncomp == n) {
        for (size_t i = 0; range && i < 2 * n && i < 8; i++) ((cgspace_t *)s)->range[i] = range[i];
        return s;
    }
    if (s) CGColorSpaceRelease(s);
    if (alternate) return CGColorSpaceRetain(alternate);
    return n == 1 ? CGColorSpaceCreateDeviceGray() : n == 4 ? CGColorSpaceCreateDeviceCMYK() : CGColorSpaceCreateDeviceRGB();
}
/* bytes ICC do espaço: o perfil embutido, ou um gerado para os espaços de nome (matriz/TRC e cinza) */
id CGColorSpaceCopyICCData(void *space) {
    cgspace_t *s = space;
    if (!s) return 0;
    if (s->icc && s->nicc) return (id)CFDataCreate(0, s->icc, s->nicc);
    cs_prof p;
    int ok = -1;
    if (s->calib == 1) { cs_calibrated_rgb(&p, s->white, s->gamma, s->matrix); ok = 0; }
    else if (s->calib == 2) { cs_calibrated_gray(&p, s->white, s->gamma[0]); ok = 0; }
    else if (s->name) ok = cs_builtin(cfstr_c(s->name), &p);
    else if (s->model == 0 || s->model == 1) ok = cs_builtin(s->model ? "sRGB" : "Gray", &p);
    if (ok || !p.has_mtx) return 0;
    u8 *icc = 0;
    size_t n = 0;
    if (cs_write_icc(&p, &icc, &n)) return 0;
    id d = (id)CFDataCreate(0, icc, (long)n);
    free(icc);
    return d;
}
void *CGColorSpaceCreateWithPlatformColorSpace(const void *ref) {
    size_t n = 0;
    const u8 *b = cx_colorsync_profile_bytes(ref, &n);
    if (!b || !n) return 0;
    id d = (id)CFDataCreate(0, b, (long)n);
    void *s = CGColorSpaceCreateWithICCData(d);
    objc_release(d);
    return s;
}
/* lista de propriedades: o nome (CFString) para espaços de nome, os bytes ICC (CFData) para o resto */
id CGColorSpaceCopyPropertyList(void *space) {
    cgspace_t *s = space;
    if (!s) return 0;
    if (s->name) return objc_retain((id)s->name);
    return CGColorSpaceCopyICCData(space);
}
void *CGColorSpaceCreateWithPropertyList(id plist) {
    if (!plist) return 0;
    if (cx_is_str(plist)) return CGColorSpaceCreateWithName(plist);
    return CGColorSpaceCreateWithICCData(plist);
}
bool CGColorSpaceIsHDR(const void *s) { const char *n = s && ((const cgspace_t *)s)->name ? cfstr_c(((const cgspace_t *)s)->name) : ""; return strstr(n, "PQ") || strstr(n, "HLG"); }
/* variantes por nome: {padrão, estendido, linear, estendido linear} */
static void *space_variant(void *space, int ext, int lin) {
    cgspace_t *s = space;
    if (!s || !s->name) return space ? CGColorSpaceRetain(space) : 0;
    const void *space_family[4][4] = {
        {kCGColorSpaceSRGB, kCGColorSpaceExtendedSRGB, kCGColorSpaceLinearSRGB, kCGColorSpaceExtendedLinearSRGB},
        {kCGColorSpaceDisplayP3, kCGColorSpaceExtendedDisplayP3, kCGColorSpaceLinearDisplayP3, kCGColorSpaceExtendedLinearDisplayP3},
        {kCGColorSpaceGenericGrayGamma2_2, kCGColorSpaceExtendedGray, kCGColorSpaceLinearGray, kCGColorSpaceExtendedLinearGray},
        {kCGColorSpaceITUR_2020, kCGColorSpaceExtendedITUR_2020, kCGColorSpaceLinearITUR_2020, kCGColorSpaceExtendedLinearITUR_2020},
    };
    const char *n = cfstr_c(s->name);
    for (u32 f = 0; f < 4; f++)
        for (int k = 0; k < 4; k++)
            if (!strcmp(n, cfstr_c(space_family[f][k]))) {
                int e = ext < 0 ? (k & 1) : ext, l = lin < 0 ? (k >> 1) : lin;
                return CGColorSpaceCreateWithName(space_family[f][e | l << 1]);
            }
    return CGColorSpaceRetain(space);
}
void *CGColorSpaceCreateExtended(void *s) { return space_variant(s, 1, -1); }
void *CGColorSpaceCreateExtendedLinearized(void *s) { return space_variant(s, 1, 1); }
void *CGColorSpaceCreateLinearized(void *s) { return space_variant(s, -1, 1); }
void *CGColorSpaceCreateCopyWithStandardRange(void *s) { return space_variant(s, 0, -1); }

/* ---------------------------------------------------------------- CGColor em qualquer espaço */
/* subclasse do NSColor do mOS: r, g, b, a (sRGB, o que o AppKit lê) no mesmo lugar, e depois o espaço,
 * os componentes originais (com o alfa no fim) e o padrão de uma cor de padrão */
typedef struct cgcolx { void *space; int n; double c[6]; void *pattern; } cgcolx_t;
static cx_class_t SC_c, SC_m;
static int sc_ready;
static void *sc_isa;                               /* isa das instâncias (o runtime pode não usar &SC_c) */
static void sc_dealloc(id self, SEL s) {
    (void)s;
    cgcolx_t *x = (cgcolx_t *)((u8 *)self + 40);
    if (x->space) objc_release((id)x->space);
    if (x->pattern) objc_release((id)x->pattern);
    cx_object_free(self);
}
static cgcolx_t *colx(const void *c) {
    if (!c || !sc_ready) return 0;
    void *isa = *(void **)c;
    return isa && (isa == (void *)&SC_c || isa == sc_isa) ? (cgcolx_t *)((u8 *)c + 40) : 0;
}
static id space_color(void *space, const CGFloat *comps, int n, void *pattern) {
    if (!sc_ready) {
        cx_class_t *nc = cx_class_named("NSColor");
        if (!nc) return 0;
        cx_define_class(&SC_c, &SC_m, "CGSpaceColor", nc, (u32)(32 + sizeof(cgcolx_t)));
        cx_add_method(&SC_c, "dealloc", (void *)sc_dealloc);
        sc_ready = 1;
    }
    id o = cx_alloc_instance(&SC_c);
    if (!o) return 0;
    sc_isa = *(void **)o;
    double *rgba = (double *)((u8 *)o + 8), rgb[3] = {0, 0, 0}, in[6] = {0};
    cgcolx_t *x = (cgcolx_t *)((u8 *)o + 40);
    x->n = n;
    for (int i = 0; i < n && i < 6; i++) x->c[i] = comps ? comps[i] : 0;
    x->space = space ? objc_retain((id)space) : 0;
    x->pattern = pattern ? objc_retain((id)pattern) : 0;
    cgspace_t *s = space;
    int nc = s ? (int)s->ncomp : 3;
    for (int i = 0; i < nc && i < 5; i++) in[i] = x->c[i];
    if (s && s->model == 5) {                      /* indexado: a tabela dá os componentes do base */
        cgspace_t *b = s->base;
        int i = (int)(in[0] + 0.5);
        if (i < 0) i = 0;
        if (i >= s->ntable) i = s->ntable - 1;
        double t[5];
        for (int k = 0; k < (int)b->ncomp && k < 5; k++) t[k] = s->table[i * b->ncomp + k] / 255.0;
        space_to_srgb(b, t, rgb);
    } else if (s && s->model == 6) {
        if (s->base && n > 1) space_to_srgb((cgspace_t *)s->base, in, rgb);
    } else space_to_srgb(s, in, rgb);
    rgba[0] = rgb[0]; rgba[1] = rgb[1]; rgba[2] = rgb[2];
    rgba[3] = n > nc ? x->c[nc] : 1;
    return o;
}
void *CGColorCreateGenericGray(CGFloat g, CGFloat a) {
    void *s = CGColorSpaceCreateWithName(kCGColorSpaceGenericGrayGamma2_2);
    CGFloat c[2] = {g, a};
    id o = space_color(s, c, 2, 0);
    CGColorSpaceRelease(s);
    return o;
}
void *CGColorCreateGenericGrayGamma2_2(CGFloat g, CGFloat a) { return CGColorCreateGenericGray(g, a); }
void *CGColorCreateGenericCMYK(CGFloat c, CGFloat m, CGFloat y, CGFloat k, CGFloat a) {
    void *s = CGColorSpaceCreateDeviceCMYK();
    CGFloat v[5] = {c, m, y, k, a};
    id o = space_color(s, v, 5, 0);
    CGColorSpaceRelease(s);
    return o;
}
void *CGColorCreateWithPattern(void *space, void *pattern, const CGFloat *comps) {
    cgspace_t *s = space;
    if (!s || s->model != 6 || !pattern) return 0;
    int n = s->base ? (int)((cgspace_t *)s->base)->ncomp + 1 : 1;
    return space_color(space, comps, comps ? n : 0, pattern);
}
void *CGColorGetPattern(const void *c) { cgcolx_t *x = colx(c); return x ? x->pattern : 0; }
static void *colx_any(const void *c) { return colx(c); }
static void *colx_space(const void *c) { cgcolx_t *x = colx(c); return x ? x->space : 0; }
static int colx_n(const void *c) { cgcolx_t *x = colx(c); return x ? x->n : 0; }
static const double *colx_comps(const void *c) { cgcolx_t *x = colx(c); return x ? x->c : 0; }

/* XYZ D50 de uma cor (pelo espaço dela; NSColor comum é sRGB) */
static void color_xyz(const void *c, double *xyz, double *alpha) {
    cgcolx_t *x = colx(c);
    const cgcolor_t *k = c;
    cs_prof srgb;
    *alpha = k->a;
    if (x && x->space) {
        const cs_prof *p = space_prof(x->space);
        cgspace_t *s = x->space;
        if (p && s->model != 5 && s->model != 6) { cs_to_xyz(p, x->c, xyz); return; }
    }
    double rgb[3] = {k->r, k->g, k->b};
    cs_builtin("sRGB", &srgb);
    cs_to_xyz(&srgb, rgb, xyz);
}
id CGColorCreateCopyByMatchingToColorSpace(void *space, int intent, id color, id opts) {
    cgspace_t *s = space;
    (void)intent; (void)opts;
    if (!s || !color || s->model == 5 || s->model == 6) return 0;
    double xyz[3], a, out[6] = {0};
    color_xyz(color, xyz, &a);
    const cs_prof *p = space_prof(s);
    cs_prof tmp;
    if (!p) { cs_builtin(s->model == 0 ? "Gray" : s->model == 2 ? "CMYK" : "sRGB", &tmp); p = &tmp; }
    cs_from_xyz(p, xyz, out);
    int n = (int)s->ncomp;
    out[n] = a;
    return space_color(space, out, n + 1, 0);
}
/* cor de padrão posta pelo CGContextSetFill/StrokeColorWithColor (appkit.c chama isto antes do RGB) */
void CGContextSetFillPattern(void *c, void *pattern, const double *comps);
void CGContextSetStrokePattern(void *c, void *pattern, const double *comps);
int cx_bm_pattern_color(const void *c, const void *color, int stroke) {
    cgcolx_t *x = colx(color);
    if (!x || !x->pattern || !as_kind(c, CGK_BM)) return 0;
    if (stroke) CGContextSetStrokePattern((void *)c, x->pattern, x->n ? x->c : 0);
    else CGContextSetFillPattern((void *)c, x->pattern, x->n ? x->c : 0);
    return 1;
}

/* ---------------------------------------------------------------- CGColorConversionInfo */
typedef struct { cx_class_t *isa; u64 cfinfo; int n; void *space[8]; int type[8], intent[8]; } cgconv_t;
static cx_class_t *conv_cls;
u64 CGColorConversionInfoGetTypeID(void) { return 0x2AD; }
static cgconv_t *conv_new(void) {
    if (!conv_cls) {
        cx_class_t *k = objc_allocateClassPair(cx_class_named("NSObject"), "CGColorConversionInfo", 0);
        if (!k) k = cx_class_named("CGColorConversionInfo");
        class_addIvar(k, "_cg", sizeof(cgconv_t) - 8, 3, "?");
        objc_registerClassPair(k);
        conv_cls = k;
    }
    return (cgconv_t *)class_createInstance(conv_cls, 0);
}
void *CGColorConversionInfoCreate(void *src, void *dst) {
    if (!src || !dst) return 0;
    cgconv_t *c = conv_new();
    if (!c) return 0;
    c->n = 2;
    c->space[0] = objc_retain((id)src); c->space[1] = objc_retain((id)dst);
    c->type[0] = 1; c->type[1] = 3;                     /* DeviceToPCS, PCSToDevice */
    return c;
}
/* (options, espaço, tipo, intenção, espaço, tipo, intenção, ..., NULL) */
void *CGColorConversionInfoCreateFromList(id options, void *space, int type, int intent, ...) {
    (void)options;
    if (!space) return 0;
    cgconv_t *c = conv_new();
    if (!c) return 0;
    c->space[0] = objc_retain((id)space); c->type[0] = type; c->intent[0] = intent; c->n = 1;
    va_list ap;
    va_start(ap, intent);
    for (;;) {
        void *s = va_arg(ap, void *);
        if (!s || c->n >= 8) break;
        c->space[c->n] = objc_retain((id)s);
        c->type[c->n] = va_arg(ap, int);
        c->intent[c->n] = va_arg(ap, int);
        c->n++;
    }
    va_end(ap);
    return c;
}
void *CGColorConversionInfoCreateWithOptions(void *src, void *dst, id options) { (void)options; return CGColorConversionInfoCreate(src, dst); }
