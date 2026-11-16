/*
 * Teste de host da casca CF do ImageIO (userland/libsystem/imageio_cf.inc) com uma CF mínima.
 *   gcc -O1 -DIIO_CF_HOST -Iuserland/libsystem -Itools tools/imageio_cf_host.c userland/libsystem/imageio.c -lz -lm -o /tmp/imageio_cf_host && /tmp/imageio_cf_host
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "iio_cf_host.h"

typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;

/* ---------------------------------------------------------------- CF mínima */
char cf_str_isa[16] __asm__("__CFConstantStringClassReference");
static cx_class_t num_c = {"num"}, bool_c = {"bool"}, dict_c = {"dict"}, data_c = {"data"}, arr_c = {"arr"}, img_c = {"img"},
                  prov_c = {"prov"}, col_c = {"col"}, cs_c = {"cs"}, ns_c = {"NSObject"};
typedef struct { void *isa; u64 flags; const char *s; u64 n; } str_t;
typedef struct { void *isa; double d; } num_t;
typedef struct { void *isa; int n; void *k[96], *v[96]; } dict_t;
typedef struct { void *isa; u8 *b; long n; } data_t;
typedef struct { void *isa; long n; void **v; } arr_t;
typedef struct { void *isa; int w, h; u32 *px; } img_t;
typedef struct { void *isa; const void *d; size_t n; } prov_t;
typedef struct { void *isa; double c[4]; } col_t;
static num_t t_true = {&bool_c, 1}, t_false = {&bool_c, 0};
void *kCFBooleanTrue = &t_true, *kCFBooleanFalse = &t_false;

static int is_str(const void *o) { return o && *(void **)o == (void *)cf_str_isa; }
void *CFStringCreateWithCString(void *a, const char *s, u32 e) {
    (void)a; (void)e;
    str_t *o = calloc(1, sizeof *o);
    o->isa = cf_str_isa; o->s = strdup(s); o->n = strlen(s);
    return o;
}
const char *cx_cstr(id o) { return is_str(o) ? ((str_t *)o)->s : 0; }
BOOL cx_is_str(id o) { return is_str(o); }
static int keq(const void *a, const void *b) { return a == b || (is_str(a) && is_str(b) && !strcmp(((str_t *)a)->s, ((str_t *)b)->s)); }
void *CFDictionaryCreateMutable(void *a, long c, const void *k, const void *v) { (void)a; (void)c; (void)k; (void)v; dict_t *d = calloc(1, sizeof *d); d->isa = &dict_c; return d; }
void CFDictionarySetValue(void *dd, const void *k, const void *v) {
    dict_t *d = dd;
    for (int i = 0; i < d->n; i++) if (keq(d->k[i], k)) { d->v[i] = (void *)v; return; }
    if (d->n < 96) { d->k[d->n] = (void *)k; d->v[d->n++] = (void *)v; }
}
void *CFDictionaryGetValue(void *dd, const void *k) {
    dict_t *d = dd;
    if (!d) return 0;
    for (int i = 0; i < d->n; i++) if (keq(d->k[i], k)) return d->v[i];
    return 0;
}
void *CFNumberCreate(void *a, long t, const void *p) {
    (void)a;
    num_t *n = calloc(1, sizeof *n);
    n->isa = &num_c;
    n->d = t == 13 ? *(const double *)p : (double)*(const long long *)p;
    return n;
}
u8 CFNumberGetValue(void *o, long t, void *out) {
    num_t *n = o;
    if (!n || (n->isa != &num_c && n->isa != &bool_c)) return 0;
    if (t == 13) *(double *)out = n->d; else *(long long *)out = (long long)n->d;
    return 1;
}
u8 CFBooleanGetValue(void *b) { return b && ((num_t *)b)->d != 0; }
u64 CFDictionaryGetTypeID(void) { return 18; }
u64 CFGetTypeID(void *o) {
    cx_class_t *c = *(cx_class_t **)o;
    if (c == &dict_c) return 18;
    if (c->tid) return ((u64 (*)(void *, void *))c->tid)(o, 0);
    return 1;
}
u64 CFNumberGetTypeID(void) { return 22; }
u64 CFBooleanGetTypeID(void) { return 21; }
u64 CFDataGetTypeID(void) { return 20; }
void *CFDataCreate(void *a, const u8 *b, long n) { (void)a; data_t *d = calloc(1, sizeof *d); d->isa = &data_c; d->b = malloc((size_t)n + 1); memcpy(d->b, b, (size_t)n); d->n = n; return d; }
const u8 *CFDataGetBytePtr(void *d) { return ((data_t *)d)->b; }
long CFDataGetLength(void *d) { return ((data_t *)d)->n; }
void CFDataAppendBytes(void *dd, const u8 *b, long n) { data_t *d = dd; d->b = realloc(d->b, (size_t)(d->n + n)); memcpy(d->b + d->n, b, (size_t)n); d->n += n; }
void *CFArrayCreate(void *a, const void **v, long n, const void *cb) { (void)a; (void)cb; arr_t *r = calloc(1, sizeof *r); r->isa = &arr_c; r->n = n; r->v = calloc((size_t)n + 1, sizeof(void *)); memcpy(r->v, v, (size_t)n * sizeof(void *)); return r; }
static int released_srcdst;
void CFRelease(void *o) {
    if (!o) return;
    cx_class_t *c = *(cx_class_t **)o;
    if (c && c->dealloc && c != (void *)cf_str_isa) { released_srcdst++; ((void (*)(void *, void *))c->dealloc)(o, 0); }
}
void *CFRetain(void *o) { return o; }
u8 CFURLGetFileSystemRepresentation(void *url, u8 r, u8 *buf, long max) { (void)r; snprintf((char *)buf, (size_t)max, "%s", cx_cstr(url)); return 1; }
void *CGDataProviderCreateWithData(void *info, const void *d, size_t n, void (*rel)(void *, const void *, size_t)) { (void)info; (void)rel; prov_t *p = calloc(1, sizeof *p); p->isa = &prov_c; p->d = d; p->n = n; return p; }
void CGDataProviderRelease(void *p) { free(p); }
void *CGDataProviderCopyData(void *p) { return CFDataCreate(0, ((prov_t *)p)->d, (long)((prov_t *)p)->n); }
static u32 last_info;
void *CGImageCreate(size_t w, size_t h, size_t bpc, size_t bpp, size_t bpr, const void *cs, u32 info, void *prov, const void *dec, bool ip, int intent) {
    (void)bpc; (void)bpp; (void)cs; (void)dec; (void)ip; (void)intent;
    img_t *i = calloc(1, sizeof *i);
    i->isa = &img_c; i->w = (int)w; i->h = (int)h; i->px = malloc(w * h * 4);
    for (size_t y = 0; y < h; y++) memcpy(i->px + y * w, (const u8 *)((prov_t *)prov)->d + y * bpr, w * 4);
    last_info = info;
    return i;
}
void *CGColorSpaceCreateDeviceRGB(void) { static void *cs = &cs_c; return &cs; }
void CGColorSpaceRelease(void *s) { (void)s; }
int cx_cgimage_src(const void *img, const u8 **px, long *bpr, int *w, int *h, int *pm) {
    const img_t *i = img;
    if (!i || i->isa != &img_c) return 0;
    *px = (const u8 *)i->px; *bpr = i->w * 4; *w = i->w; *h = i->h; *pm = 0;
    return 1;
}
size_t CGColorGetNumberOfComponents(const void *c) { (void)c; return 4; }
const double *CGColorGetComponents(const void *c) { return ((const col_t *)c)->c; }
long cx_consumer_put(void *c, const void *b, size_t n) { CFDataAppendBytes(c, b, (long)n); return (long)n; }
void cx_object_free(id o) { free(o); }
void cx_cgimage_set_uti(void *img, const void *uti) { (void)img; (void)uti; }
cx_class_t *cx_class_named(const char *n) { return !strcmp(n, "NSObject") ? &ns_c : 0; }
cx_class_t *cx_define_class(cx_class_t *c, cx_class_t *m, const char *n, cx_class_t *s, u32 extra) { (void)m; (void)s; (void)extra; c->name = n; return c; }
static size_t inst_size(cx_class_t *c) { return !strcmp(c->name, "CGImageSource") ? 1024 : 1024; }
void cx_add_method(cx_class_t *c, const char *sel, void *imp) { if (!strcmp(sel, "dealloc")) c->dealloc = imp; else c->tid = imp; }
id cx_alloc_instance(cx_class_t *c) { id o = calloc(1, inst_size(c)); o->isa = c; return o; }

/* ---------------------------------------------------------------- declarações do ImageIO */
void *CGImageDestinationCreateWithData(void *, const void *, size_t, void *);
void *CGImageDestinationCreateWithURL(void *, const void *, size_t, void *);
void *CGImageDestinationCreateWithDataConsumer(void *, const void *, size_t, void *);
void CGImageDestinationAddImage(void *, void *, void *);
void CGImageDestinationAddImageFromSource(void *, void *, size_t, void *);
void CGImageDestinationSetProperties(void *, void *);
bool CGImageDestinationFinalize(void *);
void *CGImageSourceCreateWithData(void *, void *);
void *CGImageSourceCreateWithURL(void *, void *);
void *CGImageSourceCreateIncremental(void *);
void CGImageSourceUpdateData(void *, void *, bool);
int CGImageSourceGetStatus(void *);
size_t CGImageSourceGetCount(void *);
const void *CGImageSourceGetType(void *);
void *CGImageSourceCopyPropertiesAtIndex(void *, size_t, void *);
void *CGImageSourceCopyProperties(void *, void *);
void *CGImageSourceCreateImageAtIndex(void *, size_t, void *);
void *CGImageSourceCreateThumbnailAtIndex(void *, size_t, void *);
void *CGImageSourceCopyTypeIdentifiers(void);
u64 CGImageSourceGetTypeID(void);
extern const void *kCGImagePropertyPixelWidth, *kCGImagePropertyPixelHeight, *kCGImagePropertyHasAlpha, *kCGImagePropertyOrientation,
    *kCGImagePropertyGIFDictionary, *kCGImagePropertyGIFDelayTime, *kCGImagePropertyGIFLoopCount, *kCGImagePropertyDPIWidth,
    *kCGImageDestinationLossyCompressionQuality, *kCGImageSourceCreateThumbnailFromImageAlways, *kCGImageSourceThumbnailMaxPixelSize,
    *kCGImageSourceCreateThumbnailWithTransform, *kCGImagePropertyColorModel, *kCGImageDestinationBackgroundColor,
    *kCGImagePropertyTIFFDictionary, *kCGImagePropertyTIFFCompression, *kCGImageDestinationImageMaxPixelSize;

static int fails;
#define CHECK(c, m) do { if (!(c)) { printf("FALHA %s\n", m); fails++; } else printf("ok   %s\n", m); } while (0)
static double numv(void *d, const void *k) { double v = -1; void *n = CFDictionaryGetValue(d, k); if (n) CFNumberGetValue(n, 13, &v); return v; }
static void *mkdict(void) { return CFDictionaryCreateMutable(0, 0, 0, 0); }
static void *S(const char *s) { return CFStringCreateWithCString(0, s, 0); }
static void *N(double v) { return CFNumberCreate(0, 13, &v); }
static void *mkimg(int w, int h, u32 seed) {
    u32 *px = malloc((size_t)w * h * 4);
    for (int i = 0; i < w * h; i++) { seed = seed * 1103515245u + 12345u; px[i] = (i % 7 ? 0xFF000000u : 0x80000000u) | (seed >> 8 & 0xFFFFFF); }
    prov_t p = {&prov_c, px, (size_t)w * h * 4};
    void *img = CGImageCreate((size_t)w, (size_t)h, 8, 32, (size_t)w * 4, 0, 0, &p, 0, 0, 0);
    free(px);
    return img;
}

int main(void) {
    /* 1. PNG por CFData, ida e volta exata, propriedades */
    void *img = mkimg(20, 10, 7);
    void *data = CFDataCreate(0, (const u8 *)"", 0);
    void *dst = CGImageDestinationCreateWithData(data, S("public.png"), 1, 0);
    CHECK(dst, "destino PNG criado");
    CGImageDestinationAddImage(dst, img, 0);
    CHECK(CGImageDestinationFinalize(dst), "Finalize PNG");
    CHECK(!CGImageDestinationFinalize(dst), "segundo Finalize recusa");
    void *src = CGImageSourceCreateWithData(data, 0);
    CHECK(src && CGImageSourceGetCount(src) == 1, "fonte PNG com 1 imagem");
    CHECK(src && CFGetTypeID(src) == CGImageSourceGetTypeID(), "CFGetTypeID da fonte");
    CHECK(!strcmp(cx_cstr((id)CGImageSourceGetType(src)), "public.png"), "GetType = public.png");
    void *pr = CGImageSourceCopyPropertiesAtIndex(src, 0, 0);
    CHECK(numv(pr, kCGImagePropertyPixelWidth) == 20 && numv(pr, kCGImagePropertyPixelHeight) == 10, "PixelWidth/Height");
    CHECK(CFDictionaryGetValue(pr, kCGImagePropertyHasAlpha) == kCFBooleanTrue, "HasAlpha");
    CHECK(!strcmp(cx_cstr(CFDictionaryGetValue(pr, S("ColorModel"))), "RGB"), "ColorModel por chave de outro ponteiro");
    img_t *back = CGImageSourceCreateImageAtIndex(src, 0, 0);
    CHECK(back && back->w == 20 && !memcmp(back->px, ((img_t *)img)->px, 20 * 10 * 4), "pixels iguais depois da ida e volta");
    CHECK(last_info == (4u | 0x2000u), "bitmapInfo AlphaFirst|32Little com alfa");
    CFRelease(src);

    /* 2. GIF animado: atraso por quadro e repetições */
    data = CFDataCreate(0, (const u8 *)"", 0);
    dst = CGImageDestinationCreateWithData(data, S("com.compuserve.gif"), 3, 0);
    void *gp = mkdict(), *gd = mkdict();
    CFDictionarySetValue(gd, kCGImagePropertyGIFLoopCount, N(2));
    CFDictionarySetValue(gp, kCGImagePropertyGIFDictionary, gd);
    CGImageDestinationSetProperties(dst, gp);
    for (int i = 0; i < 3; i++) {
        void *fp = mkdict(), *fg = mkdict();
        CFDictionarySetValue(fg, kCGImagePropertyGIFDelayTime, N(0.2 + i * 0.1));
        CFDictionarySetValue(fp, kCGImagePropertyGIFDictionary, fg);
        CGImageDestinationAddImage(dst, mkimg(16, 12, (u32)i + 1), fp);
    }
    CHECK(CGImageDestinationFinalize(dst), "Finalize GIF animado");
    src = CGImageSourceCreateWithData(data, 0);
    CHECK(CGImageSourceGetCount(src) == 3, "GIF com 3 quadros");
    void *cp = CGImageSourceCopyProperties(src, 0);
    CHECK(numv(CFDictionaryGetValue(cp, kCGImagePropertyGIFDictionary), kCGImagePropertyGIFLoopCount) == 2, "LoopCount 2");
    pr = CGImageSourceCopyPropertiesAtIndex(src, 2, 0);
    CHECK(numv(CFDictionaryGetValue(pr, kCGImagePropertyGIFDictionary), kCGImagePropertyGIFDelayTime) == 0.4, "DelayTime do quadro 3 = 0.4");
    CHECK(!CGImageSourceCreateImageAtIndex(src, 3, 0), "índice fora devolve NULL");

    /* 3. JPEG com orientação, miniatura com transformação e limite */
    data = CFDataCreate(0, (const u8 *)"", 0);
    dst = CGImageDestinationCreateWithData(data, S("public.jpeg"), 1, 0);
    void *jp = mkdict();
    CFDictionarySetValue(jp, kCGImageDestinationLossyCompressionQuality, N(0.9));
    CFDictionarySetValue(jp, kCGImagePropertyOrientation, N(6));
    CFDictionarySetValue(jp, kCGImagePropertyDPIWidth, N(300));
    CGImageDestinationAddImage(dst, mkimg(20, 10, 3), jp);
    CHECK(CGImageDestinationFinalize(dst), "Finalize JPEG");
    src = CGImageSourceCreateWithData(data, 0);
    pr = CGImageSourceCopyPropertiesAtIndex(src, 0, 0);
    CHECK(numv(pr, kCGImagePropertyOrientation) == 6, "Orientation 6 gravada e lida");
    CHECK(numv(pr, kCGImagePropertyDPIWidth) == 300, "DPIWidth 300");
    CHECK(CFDictionaryGetValue(pr, kCGImagePropertyHasAlpha) == kCFBooleanFalse, "JPEG sem alfa");
    CHECK(!CGImageSourceCreateThumbnailAtIndex(src, 0, 0), "sem opção, sem miniatura");
    void *to = mkdict();
    CFDictionarySetValue(to, kCGImageSourceCreateThumbnailFromImageAlways, kCFBooleanTrue);
    CFDictionarySetValue(to, kCGImageSourceCreateThumbnailWithTransform, kCFBooleanTrue);
    CFDictionarySetValue(to, kCGImageSourceThumbnailMaxPixelSize, N(8));
    img_t *th = CGImageSourceCreateThumbnailAtIndex(src, 0, to);
    CHECK(th && th->w == 4 && th->h == 8, "miniatura girada 4x8");

    /* 4. TIFF por URL, duas páginas, LZW pedido em {TIFF} */
    void *url = S("/tmp/mariana-cf.tif");
    dst = CGImageDestinationCreateWithURL(url, S("public.tiff"), 2, 0);
    void *tp = mkdict(), *td = mkdict();
    CFDictionarySetValue(td, kCGImagePropertyTIFFCompression, N(5));
    CFDictionarySetValue(tp, kCGImagePropertyTIFFDictionary, td);
    CGImageDestinationSetProperties(dst, tp);
    CGImageDestinationAddImage(dst, mkimg(9, 9, 1), 0);
    CGImageDestinationAddImageFromSource(dst, src, 0, 0);
    CHECK(CGImageDestinationFinalize(dst), "Finalize TIFF em arquivo");
    void *ts = CGImageSourceCreateWithURL(url, 0);
    CHECK(ts && CGImageSourceGetCount(ts) == 2, "TIFF com 2 páginas pela URL");
    pr = CGImageSourceCopyPropertiesAtIndex(ts, 1, 0);
    CHECK(numv(CFDictionaryGetValue(pr, kCGImagePropertyTIFFDictionary), kCGImagePropertyTIFFCompression) == 5, "{TIFF} Compression 5");
    CHECK(numv(pr, kCGImagePropertyOrientation) == 6, "página copiada da fonte leva a orientação");
    remove("/tmp/mariana-cf.tif");

    /* 5. incremental */
    void *inc = CGImageSourceCreateIncremental(0);
    CHECK(CGImageSourceGetStatus(inc) == -2, "incremental vazio: lendo cabeçalho");
    data_t *full = data;
    void *half = CFDataCreate(0, full->b, full->n / 2);
    CGImageSourceUpdateData(inc, half, 0);
    CHECK(CGImageSourceGetStatus(inc) == -1, "metade: incompleto");
    CGImageSourceUpdateData(inc, data, 1);
    CHECK(CGImageSourceGetStatus(inc) == 0 && CGImageSourceGetCount(inc) == 1, "completo depois do final");

    /* 6. recusas, consumidor, limite de tamanho, fundo do JPEG */
    CHECK(!CGImageDestinationCreateWithData(data, S("public.heic"), 1, 0), "tipo sem gravador: NULL");
    CHECK(!CGImageDestinationCreateWithData(data, S("public.png"), 0, 0), "count 0: NULL");
    void *cons = CFDataCreate(0, (const u8 *)"", 0);
    dst = CGImageDestinationCreateWithDataConsumer(cons, S("image/png"), 1, 0);
    void *mp = mkdict();
    CFDictionarySetValue(mp, kCGImageDestinationImageMaxPixelSize, N(5));
    CGImageDestinationAddImage(dst, mkimg(20, 10, 9), mp);
    CHECK(CGImageDestinationFinalize(dst), "Finalize por consumidor (MIME image/png)");
    src = CGImageSourceCreateWithData(cons, 0);
    pr = CGImageSourceCopyPropertiesAtIndex(src, 0, 0);
    CHECK(numv(pr, kCGImagePropertyPixelWidth) == 5 && numv(pr, kCGImagePropertyPixelHeight) == 3, "ImageMaxPixelSize 5 → 5x3");
    data = CFDataCreate(0, (const u8 *)"", 0);
    dst = CGImageDestinationCreateWithData(data, S("public.jpeg"), 1, 0);
    u32 clear = 0;
    prov_t cpv = {&prov_c, &clear, 4};
    col_t red = {&col_c, {1, 0, 0, 1}};
    void *bp = mkdict();
    CFDictionarySetValue(bp, kCGImageDestinationBackgroundColor, &red);
    CGImageDestinationAddImage(dst, CGImageCreate(1, 1, 8, 32, 4, 0, 0, &cpv, 0, 0, 0), bp);
    CGImageDestinationFinalize(dst);
    img_t *r = CGImageSourceCreateImageAtIndex(CGImageSourceCreateWithData(data, 0), 0, 0);
    CHECK(r && (r->px[0] >> 16 & 255) > 200 && (r->px[0] >> 8 & 255) < 60, "alfa achatado sobre o fundo vermelho");
    arr_t *types = CGImageSourceCopyTypeIdentifiers();
    CHECK(types && types->n == 8, "8 tipos de leitura");
    printf("imageio_cf_host: %s (%d falhas)\n", fails ? "FALHOU" : "ok", fails);
    return fails != 0;
}
