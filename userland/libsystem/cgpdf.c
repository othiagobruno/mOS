/*
 * mOS CoreGraphics — API CGPDF* sobre o núcleo de leitura (pdf_core.c).
 * CGPDFDocument e CGPDFPage são objetos CF (retain/release, CFGetTypeID); CGPDFObject, Dictionary, Array,
 * Stream e String são ponteiros para os objetos do núcleo (vivem enquanto o documento vive, como no macOS).
 * Scanner, content stream e tabela de operadores para quem lê o conteúdo; o desenho está em cgpdf_draw.c.
 */
#include "foundation.h"
#include "libc.h"
#include "pdf_core.h"
#include <stdbool.h>

typedef double CGFloat;
typedef struct { CGFloat x, y; } CGPoint;
typedef struct { CGFloat width, height; } CGSize;
typedef struct { CGPoint origin; CGSize size; } CGRect;
typedef struct { CGFloat a, b, c, d, tx, ty; } CGAffineTransform;

id objc_retain(id o);
void objc_release(id o);
void cx_object_free(id o);
void *CGDataProviderCopyData(void *p);
void *CGDataProviderRetain(void *p);
const u8 *CFDataGetBytePtr(void *d);
long CFDataGetLength(void *d);
void *CFDataCreate(void *alloc, const u8 *b, long n);
void CFRelease(void *o);
u8 CFURLGetFileSystemRepresentation(void *url, u8 resolve, u8 *buf, long max);
void *CFStringCreateWithCharacters(void *alloc, const u16 *chars, long n);
void *CFDateCreate(void *alloc, double at);
void *CFArrayCreate(void *alloc, const void **vals, long n, const void *cb);
void *CFDictionaryCreateMutable(void *alloc, long cap, const void *k, const void *v);
void CFDictionarySetValue(void *d, const void *k, const void *v);
void *CFNumberCreate(void *alloc, long type, const void *p);
void *CFStringCreateWithCString(void *alloc, const char *s, u32 enc);

/* ---------------------------------------------------------------- classes */
typedef struct cgpdfdoc cgpdfdoc_t;
typedef struct { cx_class_t *isa; u32 magic; cgpdfdoc_t *doc; int index; pdf_obj *dict; } cgpdfpage_t;
struct cgpdfdoc { cx_class_t *isa; u32 magic; pdf_doc *pd; cgpdfpage_t **pages; int npages; void *outline; };
#define PDOC_MAGIC 0x50444F43u
#define PPAG_MAGIC 0x50504147u
static cx_class_t D_c, D_m, G_c, G_m;
static int pdf_ready;
static void doc_dealloc(id self, SEL s) {
    (void)s;
    cgpdfdoc_t *d = (cgpdfdoc_t *)self;
    for (int i = 0; i < d->npages; i++) if (d->pages[i]) { d->pages[i]->doc = 0; objc_release((id)d->pages[i]); }
    free(d->pages);
    if (d->outline) CFRelease(d->outline);
    pdf_close(d->pd);
    d->magic = 0;
    cx_object_free(self);
}
static void page_dealloc(id self, SEL s) {
    (void)s;
    cgpdfpage_t *p = (cgpdfpage_t *)self;
    p->magic = 0;
    cx_object_free(self);
}
static u64 doc_tid(id self, SEL s) { (void)self; (void)s; return 0x2B8; }
static u64 page_tid(id self, SEL s) { (void)self; (void)s; return 0x2B9; }
static void pdf_classes(void) {
    if (pdf_ready) return;
    cx_class_t *ns = cx_class_named("NSObject");
    if (!ns) return;
    cx_define_class(&D_c, &D_m, "CGPDFDocument", ns, (u32)(sizeof(cgpdfdoc_t) - sizeof(void *)));
    cx_define_class(&G_c, &G_m, "CGPDFPage", ns, (u32)(sizeof(cgpdfpage_t) - sizeof(void *)));
    cx_add_method(&D_c, "dealloc", (void *)doc_dealloc);
    cx_add_method(&G_c, "dealloc", (void *)page_dealloc);
    cx_add_method(&D_c, "_cfTypeID", (void *)doc_tid);
    cx_add_method(&G_c, "_cfTypeID", (void *)page_tid);
    pdf_ready = 1;
}
static cgpdfdoc_t *as_doc(const void *p) { const cgpdfdoc_t *d = p; return d && d->magic == PDOC_MAGIC ? (cgpdfdoc_t *)d : 0; }
static cgpdfpage_t *as_page(const void *p) { const cgpdfpage_t *g = p; return g && g->magic == PPAG_MAGIC ? (cgpdfpage_t *)g : 0; }
u64 CGPDFDocumentGetTypeID(void) { return 0x2B8; }
u64 CGPDFPageGetTypeID(void) { return 0x2B9; }

static void *doc_from_bytes(const u8 *b, size_t n) {
    pdf_doc *pd = pdf_open(b, n);
    if (!pd) return 0;
    pdf_classes();
    if (!pdf_ready) { pdf_close(pd); return 0; }
    cgpdfdoc_t *d = (cgpdfdoc_t *)cx_alloc_instance(&D_c);
    if (!d) { pdf_close(pd); return 0; }
    d->magic = PDOC_MAGIC;
    d->pd = pd;
    return d;
}
void *CGPDFDocumentCreateWithProvider(void *provider) {
    void *data = provider ? CGDataProviderCopyData(provider) : 0;
    if (!data) return 0;
    void *d = doc_from_bytes(CFDataGetBytePtr(data), (size_t)CFDataGetLength(data));
    CFRelease(data);
    return d;
}
void *CGPDFDocumentCreateWithURL(void *url) {
    char path[1024];
    if (!url || !CFURLGetFileSystemRepresentation(url, 1, (u8 *)path, (long)sizeof path)) return 0;
    FILE_ *f = fopen(path, "rb");
    if (!f) return 0;
    size_t cap = 1 << 16, n = 0;
    u8 *b = malloc(cap);
    while (b) {
        size_t got = fread(b + n, 1, cap - n, f);
        n += got;
        if (n < cap) break;
        if (cap >= (1u << 30)) break;
        u8 *nb = realloc(b, cap * 2);
        if (!nb) break;
        b = nb;
        cap *= 2;
    }
    fclose(f);
    void *d = b && n ? doc_from_bytes(b, n) : 0;
    free(b);
    return d;
}
void *CGPDFDocumentRetain(void *d) { if (as_doc(d)) objc_retain((id)d); return d; }
void CGPDFDocumentRelease(void *d) { if (as_doc(d)) objc_release((id)d); }
bool CGPDFDocumentIsEncrypted(void *d) { cgpdfdoc_t *x = as_doc(d); return x && pdf_is_encrypted(x->pd); }
bool CGPDFDocumentIsUnlocked(void *d) { cgpdfdoc_t *x = as_doc(d); return x && pdf_is_unlocked(x->pd); }
static void drop_pages(cgpdfdoc_t *x) {
    for (int i = 0; i < x->npages; i++) if (x->pages[i]) { x->pages[i]->doc = 0; objc_release((id)x->pages[i]); }
    free(x->pages);
    x->pages = 0;
    x->npages = 0;
}
bool CGPDFDocumentUnlockWithPassword(void *d, const char *pw) {
    cgpdfdoc_t *x = as_doc(d);
    if (!x) return 0;
    if (pdf_is_unlocked(x->pd)) return 1;
    if (!pdf_unlock(x->pd, pw)) return 0;
    drop_pages(x);                                   /* as páginas são relidas já decifradas */
    return 1;
}
size_t CGPDFDocumentGetNumberOfPages(void *d) { cgpdfdoc_t *x = as_doc(d); return x && pdf_is_unlocked(x->pd) ? (size_t)pdf_page_count(x->pd) : 0; }
/* páginas: 1-based, a mesma CGPDFPage em cada chamada ("Get": o documento é o dono) */
void *CGPDFDocumentGetPage(void *d, size_t n) {
    cgpdfdoc_t *x = as_doc(d);
    if (!x || !n || !pdf_is_unlocked(x->pd)) return 0;
    int count = pdf_page_count(x->pd);
    if ((int)n > count) return 0;
    if (!x->pages) {
        x->pages = calloc((size_t)count, sizeof *x->pages);
        if (!x->pages) return 0;
        x->npages = count;
    }
    if (!x->pages[n - 1]) {
        cgpdfpage_t *p = (cgpdfpage_t *)cx_alloc_instance(&G_c);
        if (!p) return 0;
        p->magic = PPAG_MAGIC;
        p->doc = x;
        p->index = (int)n;
        p->dict = pdf_page(x->pd, (int)n - 1);
        x->pages[n - 1] = p;
    }
    return x->pages[n - 1];
}
void CGPDFDocumentGetVersion(void *d, int *major, int *minor) {
    cgpdfdoc_t *x = as_doc(d);
    int ma = 0, mi = 0;
    if (x) pdf_version(x->pd, &ma, &mi);
    if (major) *major = ma;
    if (minor) *minor = mi;
}
pdf_obj *CGPDFDocumentGetCatalog(void *d) { cgpdfdoc_t *x = as_doc(d); return x ? pdf_catalog(x->pd) : 0; }
pdf_obj *CGPDFDocumentGetInfo(void *d) { cgpdfdoc_t *x = as_doc(d); return x ? pdf_info(x->pd) : 0; }
pdf_obj *CGPDFDocumentGetID(void *d) { cgpdfdoc_t *x = as_doc(d); return x ? pdf_dict_get(x->pd, pdf_trailer(x->pd), "ID") : 0; }
/* CGPDFAccessPermissions a partir do /P (ISO 32000 tabela 22) */
u32 CGPDFDocumentGetAccessPermissions(void *d) {
    cgpdfdoc_t *x = as_doc(d);
    if (!x) return 0;
    if (!pdf_is_encrypted(x->pd)) return 0xFF;
    u32 p = pdf_permissions(x->pd), o = 0;
    if (p & (1u << 2)) o |= 1u << 0;                 /* imprimir (baixa qualidade) */
    if ((p & (1u << 2)) && (p & (1u << 11))) o |= 1u << 1;   /* alta qualidade */
    if (p & (1u << 3)) o |= 1u << 2;                 /* alterar */
    if (p & (1u << 10)) o |= 1u << 3;                /* montar */
    if (p & (1u << 4)) o |= 1u << 4;                 /* copiar */
    if (p & (1u << 9)) o |= 1u << 5;                 /* acessibilidade */
    if (p & (1u << 5)) o |= 1u << 6;                 /* comentar */
    if (p & (1u << 8)) o |= 1u << 7;                 /* formulários */
    return o;
}
bool CGPDFDocumentAllowsPrinting(void *d) { return (CGPDFDocumentGetAccessPermissions(d) & 1) != 0; }
bool CGPDFDocumentAllowsCopying(void *d) { return (CGPDFDocumentGetAccessPermissions(d) & (1u << 4)) != 0; }

/* ---------------------------------------------------------------- páginas */
void *CGPDFPageRetain(void *p) { if (as_page(p)) objc_retain((id)p); return p; }
void CGPDFPageRelease(void *p) { if (as_page(p)) objc_release((id)p); }
void *CGPDFPageGetDocument(void *p) { cgpdfpage_t *g = as_page(p); return g ? g->doc : 0; }
size_t CGPDFPageGetPageNumber(void *p) { cgpdfpage_t *g = as_page(p); return g ? (size_t)g->index : 0; }
pdf_obj *CGPDFPageGetDictionary(void *p) { cgpdfpage_t *g = as_page(p); return g ? g->dict : 0; }
pdf_doc *cx_pdf_page_doc(void *p) { cgpdfpage_t *g = as_page(p); return g && g->doc ? g->doc->pd : 0; }
static int box_of(pdf_doc *pd, pdf_obj *page, const char *key, CGRect *r) {
    pdf_obj *b = pdf_page_attr(pd, page, key);
    double v[4];
    if (!b || b->type != PO_ARRAY || b->n < 4) return 0;
    for (int i = 0; i < 4; i++) if (!pdf_num(pdf_array_get(pd, b, (u32)i), &v[i])) return 0;
    double x0 = v[0] < v[2] ? v[0] : v[2], x1 = v[0] < v[2] ? v[2] : v[0], y0 = v[1] < v[3] ? v[1] : v[3], y1 = v[1] < v[3] ? v[3] : v[1];
    r->origin.x = x0; r->origin.y = y0; r->size.width = x1 - x0; r->size.height = y1 - y0;
    return 1;
}
/* kCGPDFMediaBox 0, CropBox 1, BleedBox 2, TrimBox 3, ArtBox 4; as que faltam caem na Crop, e esta na Media */
CGRect CGPDFPageGetBoxRect(void *p, int box) {
    cgpdfpage_t *g = as_page(p);
    CGRect media = {{0, 0}, {612, 792}}, r;
    if (!g || !g->doc) { CGRect z = {{0, 0}, {0, 0}}; return z; }
    pdf_doc *pd = g->doc->pd;
    box_of(pd, g->dict, "MediaBox", &media);
    if (box == 0) return media;
    static const char *const names[5] = {"MediaBox", "CropBox", "BleedBox", "TrimBox", "ArtBox"};
    CGRect crop = media;
    if (box_of(pd, g->dict, "CropBox", &crop)) {
        double x0 = crop.origin.x > media.origin.x ? crop.origin.x : media.origin.x, y0 = crop.origin.y > media.origin.y ? crop.origin.y : media.origin.y;
        double x1 = crop.origin.x + crop.size.width < media.origin.x + media.size.width ? crop.origin.x + crop.size.width : media.origin.x + media.size.width;
        double y1 = crop.origin.y + crop.size.height < media.origin.y + media.size.height ? crop.origin.y + crop.size.height : media.origin.y + media.size.height;
        crop.origin.x = x0; crop.origin.y = y0; crop.size.width = x1 > x0 ? x1 - x0 : 0; crop.size.height = y1 > y0 ? y1 - y0 : 0;
    }
    if (box == 1 || box < 0 || box > 4) return crop;
    if (!box_of(pd, g->dict, names[box], &r)) return crop;
    return r;
}
int CGPDFPageGetRotationAngle(void *p) {
    cgpdfpage_t *g = as_page(p);
    double r = 0;
    if (!g || !g->doc) return 0;
    pdf_num(pdf_page_attr(g->doc->pd, g->dict, "Rotate"), &r);
    int a = ((int)r % 360 + 360) % 360;
    return a - a % 90;
}
static CGAffineTransform t_mul(CGAffineTransform a, CGAffineTransform b) {   /* a depois b */
    CGAffineTransform r = {a.a * b.a + a.b * b.c, a.a * b.b + a.b * b.d, a.c * b.a + a.d * b.c, a.c * b.b + a.d * b.d,
                           a.tx * b.a + a.ty * b.c + b.tx, a.tx * b.b + a.ty * b.d + b.ty};
    return r;
}
/* caixa da página (girada por Rotate + rotate) encaixada em rect: só encolhe, centraliza */
CGAffineTransform CGPDFPageGetDrawingTransform(void *page, int box, CGRect rect, int rotate, bool preserveAspect) {
    CGAffineTransform id = {1, 0, 0, 1, 0, 0};
    CGRect b = CGPDFPageGetBoxRect(page, box);
    if (b.size.width <= 0 || b.size.height <= 0) return id;
    rotate = ((rotate + CGPDFPageGetRotationAngle(page)) % 360 + 360) % 360;
    rotate -= rotate % 90;
    double bw = b.size.width, bh = b.size.height;
    if (rotate == 90 || rotate == 270) { double k = bw; bw = bh; bh = k; }
    double sx = rect.size.width / bw, sy = rect.size.height / bh;
    if (preserveAspect) { double s = sx < sy ? sx : sy; sx = sy = s; }
    if (sx > 1) sx = 1;
    if (sy > 1) sy = 1;
    double rad = -rotate * 3.14159265358979323846 / 180, c = rotate == 90 || rotate == 270 ? 0 : rotate == 180 ? -1 : 1, s = rotate == 90 ? -1 : rotate == 270 ? 1 : 0;
    (void)rad;
    CGAffineTransform t1 = {1, 0, 0, 1, -(b.origin.x + b.size.width / 2), -(b.origin.y + b.size.height / 2)};
    CGAffineTransform r = {c, s, -s, c, 0, 0};
    CGAffineTransform sc = {sx, 0, 0, sy, 0, 0};
    CGAffineTransform t2 = {1, 0, 0, 1, rect.origin.x + rect.size.width / 2, rect.origin.y + rect.size.height / 2};
    return t_mul(t_mul(t_mul(t1, r), sc), t2);
}

/* ---------------------------------------------------------------- objetos */
enum { kNull = 1, kBool, kInt, kReal, kName, kString, kArray, kDict, kStream };
static pdf_doc *odoc(const pdf_obj *o) { return o ? (pdf_doc *)o->ext : 0; }
static pdf_obj *res(pdf_obj *o) { return o ? pdf_resolve(odoc(o), o) : 0; }
int CGPDFObjectGetType(pdf_obj *o) {
    o = res(o);
    if (!o) return kNull;
    switch (o->type) {
    case PO_BOOL: return kBool;
    case PO_INT: return kInt;
    case PO_REAL: return kReal;
    case PO_NAME: return kName;
    case PO_STRING: return kString;
    case PO_ARRAY: return kArray;
    case PO_DICT: return kDict;
    case PO_STREAM: return kStream;
    default: return kNull;
    }
}
bool CGPDFObjectGetValue(pdf_obj *o, int type, void *value) {
    o = res(o);
    int t = CGPDFObjectGetType(o);
    if (type == kReal && t == kInt) { if (value) *(CGFloat *)value = (CGFloat)o->v.i; return 1; }
    if (t != type) return type == kNull && t == kNull;
    if (!value) return 1;
    switch (t) {
    case kBool: *(u8 *)value = (u8)o->v.b; break;
    case kInt: *(long *)value = (long)o->v.i; break;
    case kReal: *(CGFloat *)value = o->v.r; break;
    case kName: *(const char **)value = o->v.name; break;
    default: *(pdf_obj **)value = o; break;
    }
    return 1;
}
static pdf_obj *dget(pdf_obj *d, const char *k) { return d && k ? pdf_dict_get(odoc(d), d, k) : 0; }
size_t CGPDFDictionaryGetCount(pdf_obj *d) { d = res(d); if (d && d->type == PO_STREAM) d = d->v.stream.dict; return d && d->type == PO_DICT ? d->n : 0; }
bool CGPDFDictionaryGetObject(pdf_obj *d, const char *k, pdf_obj **v) { pdf_obj *o = dget(d, k); if (!o) return 0; if (v) *v = o; return 1; }
bool CGPDFDictionaryGetBoolean(pdf_obj *d, const char *k, u8 *v) { return CGPDFObjectGetValue(dget(d, k), kBool, v); }
bool CGPDFDictionaryGetInteger(pdf_obj *d, const char *k, long *v) { pdf_obj *o = dget(d, k); return o && o->type == PO_INT && CGPDFObjectGetValue(o, kInt, v); }
bool CGPDFDictionaryGetNumber(pdf_obj *d, const char *k, CGFloat *v) { return CGPDFObjectGetValue(dget(d, k), kReal, v); }
bool CGPDFDictionaryGetName(pdf_obj *d, const char *k, const char **v) { return CGPDFObjectGetValue(dget(d, k), kName, v); }
bool CGPDFDictionaryGetString(pdf_obj *d, const char *k, pdf_obj **v) { return CGPDFObjectGetValue(dget(d, k), kString, v); }
bool CGPDFDictionaryGetArray(pdf_obj *d, const char *k, pdf_obj **v) { return CGPDFObjectGetValue(dget(d, k), kArray, v); }
bool CGPDFDictionaryGetDictionary(pdf_obj *d, const char *k, pdf_obj **v) { return CGPDFObjectGetValue(dget(d, k), kDict, v); }
bool CGPDFDictionaryGetStream(pdf_obj *d, const char *k, pdf_obj **v) { return CGPDFObjectGetValue(dget(d, k), kStream, v); }
typedef void (*dict_fn)(const char *key, pdf_obj *value, void *info);
void CGPDFDictionaryApplyFunction(pdf_obj *d, dict_fn fn, void *info) {
    d = res(d);
    if (d && d->type == PO_STREAM) d = d->v.stream.dict;
    if (!d || d->type != PO_DICT || !fn) return;
    for (u32 i = 0; i < d->n; i++) fn(d->v.dict.keys[i], res(d->v.dict.vals[i]), info);
}
typedef struct { void *isa; int flags, reserved; void *invoke; } blk_t;
void CGPDFDictionaryApplyBlock(pdf_obj *d, void *block, void *info) {
    d = res(d);
    if (d && d->type == PO_STREAM) d = d->v.stream.dict;
    blk_t *b = block;
    if (!d || d->type != PO_DICT || !b) return;
    for (u32 i = 0; i < d->n; i++)
        if (!((bool (*)(void *, const char *, pdf_obj *, void *))b->invoke)(b, d->v.dict.keys[i], res(d->v.dict.vals[i]), info)) break;
}
size_t CGPDFArrayGetCount(pdf_obj *a) { a = res(a); return a && a->type == PO_ARRAY ? a->n : 0; }
static pdf_obj *aget(pdf_obj *a, size_t i) { a = res(a); return a && a->type == PO_ARRAY && i < a->n ? res(a->v.arr[i]) : 0; }
bool CGPDFArrayGetObject(pdf_obj *a, size_t i, pdf_obj **v) {
    pdf_obj *r = res(a);
    if (!r || r->type != PO_ARRAY || i >= r->n) return 0;
    if (v) *v = res(r->v.arr[i]);
    return 1;
}
bool CGPDFArrayGetNull(pdf_obj *a, size_t i) { pdf_obj *r = res(a); return r && r->type == PO_ARRAY && i < r->n && CGPDFObjectGetType(r->v.arr[i]) == kNull; }
bool CGPDFArrayGetBoolean(pdf_obj *a, size_t i, u8 *v) { return CGPDFObjectGetValue(aget(a, i), kBool, v); }
bool CGPDFArrayGetInteger(pdf_obj *a, size_t i, long *v) { pdf_obj *o = aget(a, i); return o && o->type == PO_INT && CGPDFObjectGetValue(o, kInt, v); }
bool CGPDFArrayGetNumber(pdf_obj *a, size_t i, CGFloat *v) { return CGPDFObjectGetValue(aget(a, i), kReal, v); }
bool CGPDFArrayGetName(pdf_obj *a, size_t i, const char **v) { return CGPDFObjectGetValue(aget(a, i), kName, v); }
bool CGPDFArrayGetString(pdf_obj *a, size_t i, pdf_obj **v) { return CGPDFObjectGetValue(aget(a, i), kString, v); }
bool CGPDFArrayGetArray(pdf_obj *a, size_t i, pdf_obj **v) { return CGPDFObjectGetValue(aget(a, i), kArray, v); }
bool CGPDFArrayGetDictionary(pdf_obj *a, size_t i, pdf_obj **v) { return CGPDFObjectGetValue(aget(a, i), kDict, v); }
bool CGPDFArrayGetStream(pdf_obj *a, size_t i, pdf_obj **v) { return CGPDFObjectGetValue(aget(a, i), kStream, v); }
void CGPDFArrayApplyBlock(pdf_obj *a, void *block, void *info) {
    a = res(a);
    blk_t *b = block;
    if (!a || a->type != PO_ARRAY || !b) return;
    for (u32 i = 0; i < a->n; i++)
        if (!((bool (*)(void *, size_t, pdf_obj *, void *))b->invoke)(b, i, res(a->v.arr[i]), info)) break;
}
pdf_obj *CGPDFStreamGetDictionary(pdf_obj *s) { s = res(s); return s && s->type == PO_STREAM ? s->v.stream.dict : 0; }
/* CGPDFDataFormat: 0 cru (filtros aplicados), 1 JPEG (os bytes antes do DCTDecode), 2 JPEG 2000 */
void *CGPDFStreamCopyData(pdf_obj *s, int *format) {
    s = res(s);
    if (format) *format = 0;
    if (!s || s->type != PO_STREAM) return 0;
    size_t n;
    const char *img = 0;
    const u8 *d = pdf_stream_data(odoc(s), s, &n, &img);
    if (!d) return 0;
    if (img && format) *format = !strcmp(img, "DCTDecode") ? 1 : !strcmp(img, "JPXDecode") ? 2 : 0;
    return CFDataCreate(0, d, (long)n);
}
size_t CGPDFStringGetLength(pdf_obj *s) { s = res(s); return s && s->type == PO_STRING ? s->n : 0; }
const u8 *CGPDFStringGetBytePtr(pdf_obj *s) { s = res(s); return s && s->type == PO_STRING ? s->v.str : 0; }
/* PDFDocEncoding 0x18..0x1F e 0x80..0xA0 (ISO 32000 anexo D); o resto coincide com Latin-1 */
static const u16 pdfdoc_lo[8] = {0x02D8, 0x02C7, 0x02C6, 0x02D9, 0x02DD, 0x02DB, 0x02DA, 0x02DC};
static const u16 pdfdoc_hi[33] = {0x2022, 0x2020, 0x2021, 0x2026, 0x2014, 0x2013, 0x0192, 0x2044, 0x2039, 0x203A, 0x2212, 0x2030,
                                  0x201E, 0x201C, 0x201D, 0x2018, 0x2019, 0x201A, 0x2122, 0xFB01, 0xFB02, 0x0141, 0x0152, 0x0160,
                                  0x0178, 0x017D, 0x0131, 0x0142, 0x0153, 0x0161, 0x017E, 0xFFFD, 0x20AC};
int cx_pdf_text_utf16(const u8 *b, size_t n, u16 **out) {
    u16 *u = malloc((n + 1) * sizeof(u16));
    size_t k = 0;
    if (!u) return -1;
    if (n >= 2 && b[0] == 0xFE && b[1] == 0xFF) {
        for (size_t i = 2; i + 1 < n; i += 2) {
            u16 c = (u16)(b[i] << 8 | b[i + 1]);
            if (c == 0x1B) {                         /* marcador de idioma: ESC .. ESC */
                size_t j = i + 2;
                while (j + 1 < n && !(b[j] == 0 && b[j + 1] == 0x1B)) j += 2;
                i = j;
                continue;
            }
            u[k++] = c;
        }
    } else if (n >= 3 && b[0] == 0xEF && b[1] == 0xBB && b[2] == 0xBF) {
        for (size_t i = 3; i < n;) {
            u32 c = b[i];
            int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
            c &= extra == 3 ? 7 : extra == 2 ? 15 : extra == 1 ? 31 : 127;
            i++;
            for (int e = 0; e < extra && i < n; e++) c = c << 6 | (b[i++] & 63);
            if (c >= 0x10000) { c -= 0x10000; u = realloc(u, (n + 2) * sizeof(u16) * 2); u[k++] = (u16)(0xD800 + (c >> 10)); u[k++] = (u16)(0xDC00 + (c & 1023)); }
            else u[k++] = (u16)c;
        }
    } else {
        for (size_t i = 0; i < n; i++) {
            u8 c = b[i];
            u[k++] = c >= 0x18 && c <= 0x1F ? pdfdoc_lo[c - 0x18] : c >= 0x80 && c <= 0xA0 ? pdfdoc_hi[c - 0x80] : c;
        }
    }
    *out = u;
    return (int)k;
}
void *CGPDFStringCopyTextString(pdf_obj *s) {
    s = res(s);
    if (!s || s->type != PO_STRING) return 0;
    u16 *u;
    int k = cx_pdf_text_utf16(s->v.str, s->n, &u);
    if (k < 0) return 0;
    void *str = CFStringCreateWithCharacters(0, u, k);
    free(u);
    return str;
}
/* D:AAAAMMDDHHmmSSOHH'mm' → CFDate (segundos desde 1/1/2001 UTC) */
static long days_from_civil(long y, unsigned m, unsigned d) {
    y -= m <= 2;
    long era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400), doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1, doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (long)doe - 719468;
}
void *CGPDFStringCopyDate(pdf_obj *s) {
    s = res(s);
    if (!s || s->type != PO_STRING) return 0;
    const u8 *b = s->v.str;
    size_t n = s->n, i = 0;
    if (n >= 2 && b[0] == 'D' && b[1] == ':') i = 2;
    int f[6] = {0, 1, 1, 0, 0, 0}, w[6] = {4, 2, 2, 2, 2, 2};
    for (int k = 0; k < 6; k++) {
        if (i + (size_t)w[k] > n || b[i] < '0' || b[i] > '9') { if (k == 0) return 0; break; }
        int v = 0;
        for (int j = 0; j < w[k]; j++) v = v * 10 + (b[i + j] - '0');
        f[k] = v;
        i += (size_t)w[k];
    }
    long tz = 0;
    if (i < n && (b[i] == '+' || b[i] == '-')) {
        int sgn = b[i] == '-' ? -1 : 1, hh = 0, mm = 0;
        i++;
        if (i + 2 <= n) { hh = (b[i] - '0') * 10 + (b[i + 1] - '0'); i += 2; }
        if (i < n && b[i] == '\'') i++;
        if (i + 2 <= n && b[i] >= '0' && b[i] <= '9') { mm = (b[i] - '0') * 10 + (b[i + 1] - '0'); }
        tz = sgn * (hh * 3600L + mm * 60L);
    }
    double unix_t = (double)days_from_civil(f[0], (unsigned)f[1], (unsigned)f[2]) * 86400.0 + f[3] * 3600.0 + f[4] * 60.0 + f[5] - (double)tz;
    return CFDateCreate(0, unix_t - 978307200.0);
}

/* ---------------------------------------------------------------- outline */
static void *outline_node(pdf_doc *pd, pdf_obj *item, cgpdfdoc_t *x, int depth);
static void *outline_children(pdf_doc *pd, pdf_obj *parent, cgpdfdoc_t *x, int depth) {
    const void *v[512];
    long n = 0;
    pdf_obj *it = pdf_dict_get(pd, parent, "First");
    for (int guard = 0; it && n < 512 && guard < 10000; guard++) {
        void *node = outline_node(pd, it, x, depth + 1);
        if (node) v[n++] = node;
        pdf_obj *nx = pdf_dict_get(pd, it, "Next");
        if (nx == it) break;
        it = nx;
    }
    if (!n) return 0;
    void *a = CFArrayCreate(0, v, n, 0);
    for (long i = 0; i < n; i++) CFRelease((void *)v[i]);
    return a;
}
static int page_index_of(pdf_doc *pd, pdf_obj *pg) {
    for (int i = 0; i < pdf_page_count(pd); i++) if (pdf_page(pd, i) == pg) return i + 1;
    return 0;
}
static pdf_obj *named_dest(pdf_doc *pd, pdf_obj *name) {
    pdf_obj *cat = pdf_catalog(pd);
    if (name->type == PO_NAME) return pdf_dict_get(pd, pdf_dict_get(pd, cat, "Dests"), name->v.name);
    /* árvore de nomes /Names /Dests */
    pdf_obj *node = pdf_dict_get(pd, pdf_dict_get(pd, cat, "Names"), "Dests");
    for (int depth = 0; node && depth < 32; depth++) {
        pdf_obj *names = pdf_dict_get(pd, node, "Names");
        if (names && names->type == PO_ARRAY) {
            for (u32 i = 0; i + 1 < names->n; i += 2) {
                pdf_obj *k = pdf_resolve(pd, names->v.arr[i]);
                if (k && k->type == PO_STRING && k->n == name->n && !memcmp(k->v.str, name->v.str, name->n)) return pdf_resolve(pd, names->v.arr[i + 1]);
            }
            return 0;
        }
        pdf_obj *kids = pdf_dict_get(pd, node, "Kids"), *next = 0;
        for (u32 i = 0; kids && i < kids->n && !next; i++) {
            pdf_obj *kd = pdf_resolve(pd, kids->v.arr[i]), *lim = pdf_dict_get(pd, kd, "Limits");
            pdf_obj *lo = pdf_array_get(pd, lim, 0), *hi = pdf_array_get(pd, lim, 1);
            if (!lo || !hi) { next = kd; break; }
            size_t ln = lo->n < name->n ? lo->n : name->n, hn = hi->n < name->n ? hi->n : name->n;
            int c1 = memcmp(name->v.str, lo->v.str, ln), c2 = memcmp(name->v.str, hi->v.str, hn);
            if ((c1 > 0 || (c1 == 0 && name->n >= lo->n)) && (c2 < 0 || (c2 == 0 && name->n <= hi->n))) next = kd;
        }
        node = next;
    }
    return 0;
}
/* o array final do destino (nome, string e dicionário /D resolvidos) */
pdf_obj *cx_pdf_dest_array(pdf_doc *pd, pdf_obj *dest) {
    dest = pdf_resolve(pd, dest);
    for (int g = 0; dest && g < 4; g++) {
        if (dest->type == PO_NAME || dest->type == PO_STRING) { dest = pdf_resolve(pd, named_dest(pd, dest)); continue; }
        if (dest->type == PO_DICT) { dest = pdf_dict_get(pd, dest, "D"); continue; }
        break;
    }
    return dest && dest->type == PO_ARRAY ? dest : 0;
}
int cx_pdf_dest_page(pdf_doc *pd, pdf_obj *dest) {
    for (int g = 0; dest && g < 4; g++) {
        if (dest->type == PO_NAME || dest->type == PO_STRING) { dest = named_dest(pd, dest); continue; }
        if (dest->type == PO_DICT) { dest = pdf_dict_get(pd, dest, "D"); continue; }
        break;
    }
    if (!dest || dest->type != PO_ARRAY || !dest->n) return 0;
    pdf_obj *p = pdf_resolve(pd, dest->v.arr[0]);
    if (p && p->type == PO_INT) return (int)p->v.i + 1;
    return page_index_of(pd, p);
}
static void *outline_node(pdf_doc *pd, pdf_obj *item, cgpdfdoc_t *x, int depth) {
    if (depth > 64) return 0;
    void *d = CFDictionaryCreateMutable(0, 0, 0, 0);
    pdf_obj *t = pdf_dict_get(pd, item, "Title");
    if (t) {
        void *s = CGPDFStringCopyTextString(t);
        void *k = CFStringCreateWithCString(0, "Title", 0x08000100);
        if (s) { CFDictionarySetValue(d, k, s); CFRelease(s); }
        CFRelease(k);
    }
    pdf_obj *dest = pdf_dict_get(pd, item, "Dest");
    if (!dest) {
        pdf_obj *a = pdf_dict_get(pd, item, "A");
        const char *s = pdf_name(pdf_dict_get(pd, a, "S"));
        if (s && !strcmp(s, "GoTo")) dest = pdf_dict_get(pd, a, "D");
    }
    int pg = cx_pdf_dest_page(pd, dest);
    if (pg) {
        long long v = pg;
        void *num = CFNumberCreate(0, 4, &v), *k = CFStringCreateWithCString(0, "Destination", 0x08000100);
        CFDictionarySetValue(d, k, num);
        CFRelease(num);
        CFRelease(k);
    }
    void *kids = outline_children(pd, item, x, depth);
    if (kids) {
        void *k = CFStringCreateWithCString(0, "Children", 0x08000100);
        CFDictionarySetValue(d, k, kids);
        CFRelease(kids);
        CFRelease(k);
    }
    return d;
}
/* dicionário raiz com "Children" (como o CGPDFDocumentGetOutline do macOS); NULL sem outline */
void *CGPDFDocumentGetOutline(void *doc) {
    cgpdfdoc_t *x = as_doc(doc);
    if (!x || !pdf_is_unlocked(x->pd)) return 0;
    if (x->outline) return x->outline;
    pdf_obj *o = pdf_dict_get(x->pd, pdf_catalog(x->pd), "Outlines");
    if (!o) return 0;
    void *kids = outline_children(x->pd, o, x, 0);
    if (!kids) return 0;
    void *d = CFDictionaryCreateMutable(0, 0, 0, 0), *k = CFStringCreateWithCString(0, "Children", 0x08000100);
    CFDictionarySetValue(d, k, kids);
    CFRelease(kids);
    CFRelease(k);
    x->outline = d;
    return d;
}

/* ---------------------------------------------------------------- content stream, tabela de operadores, scanner */
typedef struct cgcs { cx_class_t *isa; u32 magic; int rc; pdf_doc *pd; pdf_obj *streams[64]; int nstreams; pdf_obj *res; struct cgcs *parent; u8 *buf; size_t blen; } cgcs_t;
typedef void (*op_cb)(void *scanner, void *info);
typedef struct { int rc; struct { char name[8]; op_cb cb; } op[96]; int n; } cgopt_t;
typedef struct { int rc; cgcs_t *cs; cgopt_t *tbl; void *info; pdf_obj *stack[512]; int sp; int stop; pdf_lexer *lx; } cgscan_t;
#define PCS_MAGIC 0x50435354u
void *CGPDFContentStreamCreateWithStream(pdf_obj *stream, pdf_obj *resources, void *parent) {
    stream = res(stream);
    if (!stream || stream->type != PO_STREAM) return 0;
    cgcs_t *c = calloc(1, sizeof *c);
    if (!c) return 0;
    c->magic = PCS_MAGIC;
    c->rc = 1;
    c->pd = odoc(stream);
    c->streams[0] = stream;
    c->nstreams = 1;
    c->res = res(resources);
    c->parent = parent;
    if (parent) ((cgcs_t *)parent)->rc++;
    return c;
}
void *CGPDFContentStreamCreateWithPage(void *page) {
    cgpdfpage_t *g = as_page(page);
    if (!g || !g->doc) return 0;
    pdf_doc *pd = g->doc->pd;
    cgcs_t *c = calloc(1, sizeof *c);
    if (!c) return 0;
    c->magic = PCS_MAGIC;
    c->rc = 1;
    c->pd = pd;
    pdf_obj *cont = pdf_dict_get(pd, g->dict, "Contents");
    if (cont && cont->type == PO_ARRAY)
        for (u32 i = 0; i < cont->n && c->nstreams < 64; i++) { pdf_obj *s = pdf_resolve(pd, cont->v.arr[i]); if (s && s->type == PO_STREAM) c->streams[c->nstreams++] = s; }
    else if (cont && cont->type == PO_STREAM) c->streams[c->nstreams++] = cont;
    c->res = pdf_page_attr(pd, g->dict, "Resources");
    return c;
}
void *CGPDFContentStreamRetain(void *c) { if (c) ((cgcs_t *)c)->rc++; return c; }
void CGPDFContentStreamRelease(void *cv) {
    cgcs_t *c = cv;
    if (!c || --c->rc > 0) return;
    if (c->parent) CGPDFContentStreamRelease(c->parent);
    free(c->buf);
    free(c);
}
void *CGPDFContentStreamGetStreams(void *cv) {
    cgcs_t *c = cv;
    if (!c) return 0;
    return CFArrayCreate(0, (const void **)c->streams, c->nstreams, 0);
}
/* recurso por categoria (Font, XObject, ExtGState, ColorSpace, Pattern, Shading, Properties) e nome; sobe pelos pais */
pdf_obj *CGPDFContentStreamGetResource(void *cv, const char *category, const char *name) {
    for (cgcs_t *c = cv; c; c = c->parent) {
        pdf_obj *cat = pdf_dict_get(c->pd, c->res, category);
        pdf_obj *o = cat ? pdf_dict_get(c->pd, cat, name) : 0;
        if (o) return o;
    }
    return 0;
}
pdf_doc *cx_pdf_cs_doc(void *cv) { return cv ? ((cgcs_t *)cv)->pd : 0; }
pdf_obj *cx_pdf_cs_resources(void *cv) { return cv ? ((cgcs_t *)cv)->res : 0; }

void *CGPDFOperatorTableCreate(void) { cgopt_t *t = calloc(1, sizeof *t); if (t) t->rc = 1; return t; }
void *CGPDFOperatorTableRetain(void *t) { if (t) ((cgopt_t *)t)->rc++; return t; }
void CGPDFOperatorTableRelease(void *t) { if (t && --((cgopt_t *)t)->rc <= 0) free(t); }
void CGPDFOperatorTableSetCallback(void *tv, const char *name, op_cb cb) {
    cgopt_t *t = tv;
    if (!t || !name || strlen(name) >= 8) return;
    for (int i = 0; i < t->n; i++) if (!strcmp(t->op[i].name, name)) { t->op[i].cb = cb; return; }
    if (t->n == 96) return;
    memcpy(t->op[t->n].name, name, strlen(name) + 1);
    t->op[t->n++].cb = cb;
}
void *CGPDFScannerCreate(void *cs, void *table, void *info) {
    cgscan_t *s = calloc(1, sizeof *s);
    if (!s) return 0;
    s->rc = 1;
    s->cs = CGPDFContentStreamRetain(cs);
    s->tbl = CGPDFOperatorTableRetain(table);
    s->info = info;
    return s;
}
void *CGPDFScannerRetain(void *s) { if (s) ((cgscan_t *)s)->rc++; return s; }
void CGPDFScannerRelease(void *sv) {
    cgscan_t *s = sv;
    if (!s || --s->rc > 0) return;
    CGPDFContentStreamRelease(s->cs);
    CGPDFOperatorTableRelease(s->tbl);
    free(s);
}
void *CGPDFScannerGetContentStream(void *s) { return s ? ((cgscan_t *)s)->cs : 0; }
void CGPDFScannerStop(void *s) { if (s) ((cgscan_t *)s)->stop = 1; }
/* lê os streams do content stream em ordem; cada operador chama o callback com os operandos na pilha */
bool CGPDFScannerScan(void *sv) {
    cgscan_t *s = sv;
    if (!s || !s->cs) return 0;
    cgcs_t *c = s->cs;
    for (int k = 0; k < c->nstreams && !s->stop; k++) {
        size_t n;
        const u8 *d = pdf_stream_data(c->pd, c->streams[k], &n, 0);
        if (!d) continue;
        pdf_lexer *L = pdf_lex_begin(c->pd, d, n);
        if (!L) return 0;
        s->lx = L;
        pdf_obj *o;
        while (!s->stop && (o = pdf_lex_next(L))) {
            if (o->type != PO_OP) {
                if (s->sp < 512) s->stack[s->sp++] = o;
                continue;
            }
            const char *op = o->v.name;
            if (!strcmp(op, "BI")) {                /* imagem inline: um stream na pilha e o callback de "EI" */
                pdf_obj *img = pdf_lex_inline_image(L);
                if (img && s->sp < 512) s->stack[s->sp++] = img;
                op = "EI";
            }
            op_cb cb = 0;
            for (int i = 0; s->tbl && i < s->tbl->n; i++) if (!strcmp(s->tbl->op[i].name, op)) { cb = s->tbl->op[i].cb; break; }
            if (cb) cb(s, s->info);
            s->sp = 0;                               /* operandos não consumidos são descartados */
        }
        pdf_lex_end(L);
        s->lx = 0;
    }
    return 1;
}
static pdf_obj *pop(cgscan_t *s) { return s && s->sp > 0 ? s->stack[--s->sp] : 0; }
bool CGPDFScannerPopObject(void *s, pdf_obj **v) { pdf_obj *o = pop(s); if (!o) return 0; if (v) *v = o; return 1; }
bool CGPDFScannerPopBoolean(void *s, u8 *v) { pdf_obj *o = pop(s); return o && CGPDFObjectGetValue(o, kBool, v); }
bool CGPDFScannerPopInteger(void *s, long *v) { pdf_obj *o = pop(s); return o && o->type == PO_INT && CGPDFObjectGetValue(o, kInt, v); }
bool CGPDFScannerPopNumber(void *s, CGFloat *v) { pdf_obj *o = pop(s); return o && CGPDFObjectGetValue(o, kReal, v); }
bool CGPDFScannerPopName(void *s, const char **v) { pdf_obj *o = pop(s); return o && CGPDFObjectGetValue(o, kName, v); }
bool CGPDFScannerPopString(void *s, pdf_obj **v) { pdf_obj *o = pop(s); return o && CGPDFObjectGetValue(o, kString, v); }
bool CGPDFScannerPopArray(void *s, pdf_obj **v) { pdf_obj *o = pop(s); return o && CGPDFObjectGetValue(o, kArray, v); }
bool CGPDFScannerPopDictionary(void *s, pdf_obj **v) { pdf_obj *o = pop(s); return o && CGPDFObjectGetValue(o, kDict, v); }
bool CGPDFScannerPopStream(void *s, pdf_obj **v) { pdf_obj *o = pop(s); return o && CGPDFObjectGetValue(o, kStream, v); }
