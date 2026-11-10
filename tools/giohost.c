/* Teste de host do ImageIO: CGImageSource e CGImageDestination, PNG e JPEG. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct obj {
    int kind; /* 1 str, 2 num, 3 bool */
    char s[80];
    long long n;
    int b;
} obj;

typedef struct {
    obj *k[8];
    obj *v[8];
    int n;
} dict;

typedef struct {
    unsigned char *p;
    long n, cap;
} data;

void *kCFBooleanTrue;
void *kCFBooleanFalse;
void *kCGImageDestinationLossyCompressionQuality;

void *CFStringCreateWithCString(void *a, const char *s, unsigned enc) {
    obj *o = calloc(1, sizeof *o);
    (void)a; (void)enc;
    o->kind = 1;
    snprintf(o->s, sizeof o->s, "%s", s ? s : "");
    return o;
}
const char *CFStringGetCStringPtr(void *s, unsigned enc) {
    obj *o = s; (void)enc;
    return o && o->kind == 1 ? o->s : 0;
}
void *CFNumberCreate(void *a, long type, const void *p) {
    obj *o = calloc(1, sizeof *o);
    (void)a; (void)type;
    o->kind = 2;
    o->n = p ? *(const long long *)p : 0;
    return o;
}
int CFNumberGetValue(void *n, long type, void *out) {
    obj *o = n; (void)type;
    if (!o || o->kind != 2 || !out) return 0;
    *(long long *)out = o->n;
    return 1;
}
int CFBooleanGetValue(void *b) {
    obj *o = b;
    if (b == kCFBooleanTrue) return 1;
    if (b == kCFBooleanFalse) return 0;
    return o && o->kind == 3 && o->b;
}
void *CFDictionaryCreateMutable(void *a, long cap, const void *k, const void *v) {
    (void)a; (void)cap; (void)k; (void)v;
    return calloc(1, sizeof(dict));
}
void CFDictionarySetValue(void *d, void *key, void *val) {
    dict *t = d;
    int i;
    if (!t || !key || t->n == 8) return;
    for (i = 0; i < t->n; i++) if (t->k[i] == key) { t->v[i] = val; return; }
    t->k[t->n] = key; t->v[t->n] = val; t->n++;
}
void *CFDictionaryGetValue(void *d, void *key) {
    dict *t = d;
    int i;
    if (!t) return 0;
    for (i = 0; i < t->n; i++) if (t->k[i] == key) return t->v[i];
    return 0;
}
const unsigned char *CFDataGetBytePtr(void *d) { return ((data *)d)->p; }
long CFDataGetLength(void *d) { return ((data *)d)->n; }
void CFDataAppendBytes(void *d, const unsigned char *b, long n) {
    data *x = d;
    if (x->n + n > x->cap) {
        long c = x->cap ? x->cap * 2 : 256;
        while (c < x->n + n) c *= 2;
        x->p = realloc(x->p, (size_t)c);
        x->cap = c;
    }
    memcpy(x->p + x->n, b, (size_t)n);
    x->n += n;
}

void *CGImageSourceCreateWithData(void *data, void *opts);
unsigned long CGImageSourceGetCount(void *src);
const char *CGImageSourceGetType(void *src);
void *CGImageSourceCreateImageAtIndex(void *src, unsigned long index, void *opts);
void *CGImageSourceCopyPropertiesAtIndex(void *src, unsigned long index, void *opts);
void *CGImageSourceCreateThumbnailAtIndex(void *src, unsigned long index, void *opts);
void *CGImageDestinationCreateWithData(void *data, const void *type, unsigned long count, void *opts);
void gio_destination_add(void *dst, void *image, void *props);
int gio_destination_finish(void *dst);
void CGImageDestinationAddImage(void *dst, void *img, void *props) { gio_destination_add(dst, img, props); }
int CGImageDestinationFinalize(void *dst) { return gio_destination_finish(dst); }
int gio_image_wh(void *image, int *w, int *h);
uint32_t gio_image_px(void *image, int x, int y);

static data *data_new(const unsigned char *p, long n) {
    data *d = calloc(1, sizeof *d);
    d->p = malloc((size_t)n);
    memcpy(d->p, p, (size_t)n);
    d->n = d->cap = n;
    return d;
}

static int roundtrip(const char *uti, int want_jpeg) {
    uint32_t px[64];
    int side = want_jpeg ? 8 : 2;
    for (int i = 0; i < 64; i++) px[i] = 0xffff0000u;
    if (!want_jpeg) {
        px[0] = 0xffff0000u; px[1] = 0xff00ff00u; px[2] = 0xff0000ffu; px[3] = 0xffffffffu;
    }
    void *img, *dst, *src, *props, *back, *dict, *nw, *seed;
    data *out = calloc(1, sizeof *out);
    data *in;
    unsigned char *blob = 0;
    size_t bn = 0;
    int w = 0, h = 0;
    long long n = 0;
    extern int mos_img_encode_png(const uint32_t *, int, int, uint8_t **, size_t *);
    extern void *kCGImagePropertyPixelWidth, *kCGImagePropertyPixelHeight;
    if (mos_img_encode_png(px, side, side, &blob, &bn)) return 1;
    in = data_new(blob, (long)bn);
    seed = CGImageSourceCreateWithData(in, 0);
    img = CGImageSourceCreateImageAtIndex(seed, 0, 0);
    dst = CGImageDestinationCreateWithData(out, CFStringCreateWithCString(0, uti, 0), 1, 0);
    props = CFDictionaryCreateMutable(0, 2, 0, 0);
    {
        long long q = 90;
        CFDictionarySetValue(props, kCGImageDestinationLossyCompressionQuality, CFNumberCreate(0, 4, &q));
    }
    CGImageDestinationAddImage(dst, img, props);
    if (!CGImageDestinationFinalize(dst)) return 1;
    src = CGImageSourceCreateWithData(out, 0);
    if (CGImageSourceGetCount(src) != 1) return 2;
    if (!CGImageSourceGetType(src) || !strstr(CGImageSourceGetType(src), want_jpeg ? "jpeg" : "png")) return 3;
    dict = CGImageSourceCopyPropertiesAtIndex(src, 0, 0);
    nw = CFDictionaryGetValue(dict, kCGImagePropertyPixelWidth);
    if (!CFNumberGetValue(nw, 4, &n) || n != side) return 4;
    if (!CFNumberGetValue(CFDictionaryGetValue(dict, kCGImagePropertyPixelHeight), 4, &n) || n != side) return 5;
    back = CGImageSourceCreateImageAtIndex(src, 0, 0);
    if (!gio_image_wh(back, &w, &h) || w != side || h != side) return 6;
    if (!want_jpeg && gio_image_px(back, 0, 0) != 0xffff0000u) return 7;
    if (want_jpeg && (gio_image_px(back, 0, 0) >> 16 & 255) < 200) return 8;
    return 0;
}

int main(void) {
    obj *yes = calloc(1, sizeof *yes), *no = calloc(1, sizeof *no);
    void *src, *thumb, *opts;
    unsigned char *png = 0;
    size_t pn = 0;
    uint32_t px[16];
    int i, w = 0, h = 0;
    extern int mos_img_encode_png(const uint32_t *, int, int, uint8_t **, size_t *);
    extern void *kCGImageSourceCreateThumbnailFromImageAlways;
    extern void *kCGImageSourceThumbnailMaxPixelSize;
    yes->kind = 3; yes->b = 1; no->kind = 3; no->b = 0;
    kCFBooleanTrue = yes; kCFBooleanFalse = no;
    kCGImageDestinationLossyCompressionQuality = CFStringCreateWithCString(0, "kCGImageDestinationLossyCompressionQuality", 0);
    for (i = 0; i < 16; i++) px[i] = 0xff112233u;
    if (mos_img_encode_png(px, 4, 4, &png, &pn)) return 10;
    src = CGImageSourceCreateWithData(data_new(png, (long)pn), 0);
    opts = CFDictionaryCreateMutable(0, 4, 0, 0);
    if (CGImageSourceCreateThumbnailAtIndex(src, 0, opts)) return 11;
    CFDictionarySetValue(opts, kCGImageSourceCreateThumbnailFromImageAlways, kCFBooleanTrue);
    {
        long long m = 2;
        CFDictionarySetValue(opts, kCGImageSourceThumbnailMaxPixelSize, CFNumberCreate(0, 4, &m));
    }
    thumb = CGImageSourceCreateThumbnailAtIndex(src, 0, opts);
    if (!gio_image_wh(thumb, &w, &h) || w != 2 || h != 2) return 12;
    if (roundtrip("public.png", 0)) return 20 + roundtrip("public.png", 0);
    if (roundtrip("public.jpeg", 1)) return 30 + roundtrip("public.jpeg", 1);
    printf("ok png jpeg thumb\n");
    return 0;
}
