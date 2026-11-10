/*
 * ImageIO do Grok: PNG e JPEG de verdade.
 * As chaves kCGImage* são CFString. Propriedades saem num CFDictionary.
 * A miniatura lê o dicionário: sem "sempre" não inventa thumbnail;
 * kCGImageSourceThumbnailMaxPixelSize limita a aresta maior.
 */
#ifdef GIO_HOST
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#else
#include "sys.h"
#include "libc.h"
#endif

#include "../frameworks/ImageIO/imageio_decode.c"
#include "../frameworks/ImageIO/imageio_encode.c"

char *strstr(const char *s, const char *n);
void *CFStringCreateWithCString(void *alloc, const char *s, unsigned enc);
void *CFDictionaryCreateMutable(void *alloc, long cap, const void *k, const void *v);
void CFDictionarySetValue(void *d, void *key, void *val);
void *CFDictionaryGetValue(void *d, void *key);
void *CFNumberCreate(void *alloc, long type, const void *p);
int CFNumberGetValue(void *n, long type, void *out);
int CFBooleanGetValue(void *b);
const char *CFStringGetCStringPtr(void *s, unsigned enc);
const unsigned char *CFDataGetBytePtr(void *d);
long CFDataGetLength(void *d);
void CFDataAppendBytes(void *d, const unsigned char *b, long n);
extern void *kCFBooleanTrue;
extern void *kCFBooleanFalse;

void *kCGImagePropertyPixelWidth;
void *kCGImagePropertyPixelHeight;
void *kCGImageSourceCreateThumbnailFromImageAlways;
void *kCGImageSourceCreateThumbnailWithTransform;
void *kCGImageSourceShouldCache;
void *kCGImageSourceShouldCacheImmediately;
void *kCGImageSourceThumbnailMaxPixelSize;
extern void *kCGImageDestinationLossyCompressionQuality;

enum { kGioSInt64 = 4 };
enum { GIO_MAGIC = 0x47494f31 };

static void gio_keys(void) {
    if (kCGImagePropertyPixelWidth) return;
    kCGImagePropertyPixelWidth = CFStringCreateWithCString(0, "PixelWidth", 0x08000100);
    kCGImagePropertyPixelHeight = CFStringCreateWithCString(0, "PixelHeight", 0x08000100);
    kCGImageSourceCreateThumbnailFromImageAlways = CFStringCreateWithCString(0, "kCGImageSourceCreateThumbnailFromImageAlways", 0x08000100);
    kCGImageSourceCreateThumbnailWithTransform = CFStringCreateWithCString(0, "kCGImageSourceCreateThumbnailWithTransform", 0x08000100);
    kCGImageSourceShouldCache = CFStringCreateWithCString(0, "kCGImageSourceShouldCache", 0x08000100);
    kCGImageSourceShouldCacheImmediately = CFStringCreateWithCString(0, "kCGImageSourceShouldCacheImmediately", 0x08000100);
    kCGImageSourceThumbnailMaxPixelSize = CFStringCreateWithCString(0, "kCGImageSourceThumbnailMaxPixelSize", 0x08000100);
}

typedef struct {
    unsigned magic;
    unsigned char *bytes;
    unsigned long n;
    mos_img_t img;
    int ok;
} gio_src;

typedef struct {
    int w, h;
    uint32_t *px;
} gio_image;

typedef struct {
    unsigned magic;
    void *data;
    const void *type;
    unsigned char *out;
    unsigned long on;
    int quality;
} gio_dst;

static int gio_yes(void *v) {
    if (!v || v == kCFBooleanFalse) return 0;
    if (v == kCFBooleanTrue) return 1;
    return CFBooleanGetValue(v);
}

static long gio_num(void *dict, void *key, long fallback) {
    void *v;
    long long n = 0;
    if (!dict || !key) return fallback;
    v = CFDictionaryGetValue(dict, key);
    if (!v) return fallback;
    if (!CFNumberGetValue(v, kGioSInt64, &n)) return fallback;
    return (long)n;
}

static void *gio_num_obj(long long n) {
    return CFNumberCreate(0, kGioSInt64, &n);
}

void *CGImageSourceCreateWithData(void *data, void *opts) {
    gio_src *s;
    const unsigned char *p;
    long n;
    (void)opts;
    gio_keys();
    if (!data) return 0;
    p = CFDataGetBytePtr(data);
    n = CFDataGetLength(data);
    if (!p || n < 8) return 0;
    s = calloc(1, sizeof *s);
    if (!s) return 0;
    s->magic = GIO_MAGIC;
    s->bytes = malloc((unsigned long)n);
    if (!s->bytes) { free(s); return 0; }
    memcpy(s->bytes, p, (unsigned long)n);
    s->n = (unsigned long)n;
    s->ok = mos_img_decode(s->bytes, s->n, &s->img) == 0;
    return s;
}

unsigned long CGImageSourceGetCount(void *src) {
    gio_src *s = src;
    return s && s->magic == GIO_MAGIC && s->ok ? 1 : 0;
}

const char *CGImageSourceGetType(void *src) {
    gio_src *s = src;
    if (!s || s->magic != GIO_MAGIC) return 0;
    return mos_img_uti(s->bytes, s->n);
}

static void *image_box(const uint32_t *px, int w, int h) {
    gio_image *b;
    unsigned long n;
    if (!px || w < 1 || h < 1) return 0;
    b = calloc(1, sizeof *b);
    if (!b) return 0;
    n = (unsigned long)w * (unsigned long)h;
    b->px = malloc(n * 4);
    if (!b->px) { free(b); return 0; }
    memcpy(b->px, px, n * 4);
    b->w = w;
    b->h = h;
    return b;
}

void *CGImageSourceCreateImageAtIndex(void *src, unsigned long index, void *opts) {
    gio_src *s = src;
    (void)opts;
    if (!s || s->magic != GIO_MAGIC || !s->ok || index != 0) return 0;
    return image_box(s->img.px, s->img.w, s->img.h);
}

void *CGImageSourceCopyPropertiesAtIndex(void *src, unsigned long index, void *opts) {
    gio_src *s = src;
    void *d, *nw, *nh;
    (void)opts;
    gio_keys();
    if (!s || s->magic != GIO_MAGIC || !s->ok || index != 0) return 0;
    d = CFDictionaryCreateMutable(0, 4, 0, 0);
    if (!d) return 0;
    nw = gio_num_obj(s->img.w);
    nh = gio_num_obj(s->img.h);
    CFDictionarySetValue(d, kCGImagePropertyPixelWidth, nw);
    CFDictionarySetValue(d, kCGImagePropertyPixelHeight, nh);
    return d;
}

void *CGImageSourceCreateThumbnailAtIndex(void *src, unsigned long index, void *opts) {
    gio_src *s = src;
    long maxe;
    int w, h, nw, nh, x, y, edge;
    uint32_t *px;
    void *box;
    gio_keys();
    if (!s || s->magic != GIO_MAGIC || !s->ok || index != 0) return 0;
    if (!gio_yes(opts ? CFDictionaryGetValue(opts, kCGImageSourceCreateThumbnailFromImageAlways) : 0))
        return 0;
    (void)CFDictionaryGetValue(opts, kCGImageSourceCreateThumbnailWithTransform);
    (void)CFDictionaryGetValue(opts, kCGImageSourceShouldCache);
    w = s->img.w;
    h = s->img.h;
    edge = w > h ? w : h;
    maxe = gio_num(opts, kCGImageSourceThumbnailMaxPixelSize, edge);
    if (maxe < 1) maxe = 1;
    if (edge > maxe) {
        nw = (int)((long)w * maxe / edge);
        nh = (int)((long)h * maxe / edge);
        if (nw < 1) nw = 1;
        if (nh < 1) nh = 1;
    } else {
        nw = w;
        nh = h;
    }
    px = malloc((unsigned long)nw * (unsigned long)nh * 4);
    if (!px) return 0;
    for (y = 0; y < nh; y++) {
        int sy = y * h / nh;
        for (x = 0; x < nw; x++) {
            int sx = x * w / nw;
            px[y * nw + x] = s->img.px[sy * w + sx];
        }
    }
    box = image_box(px, nw, nh);
    free(px);
    return box;
}

void *CGImageDestinationCreateWithData(void *data, const void *type, unsigned long count, void *opts) {
    gio_dst *d;
    (void)count;
    (void)opts;
    gio_keys();
    d = calloc(1, sizeof *d);
    if (!d) return 0;
    d->magic = GIO_MAGIC;
    d->data = data;
    d->type = type;
    d->quality = 85;
    return d;
}

static int gio_is_jpeg(const void *type) {
    const char *s = CFStringGetCStringPtr((void *)type, 0x08000100);
    if (!s) s = type;
    return s && strstr(s, "jpeg") != 0;
}

void gio_destination_add(void *dst, void *image, void *props) {
    gio_image *b = image;
    gio_dst *d = dst;
    unsigned char *enc = 0;
    size_t n = 0;
    long q;
    if (!d || d->magic != GIO_MAGIC || !b || !b->px) return;
    gio_keys();
    q = gio_num(props, kCGImageDestinationLossyCompressionQuality, 0);
    if (q > 0 && q <= 1) q *= 100;
    if (q > 0 && q <= 100) d->quality = (int)q;
    if (gio_is_jpeg(d->type)) {
        if (mos_img_encode_jpeg(b->px, b->w, b->h, d->quality, &enc, &n)) return;
    } else if (mos_img_encode_png(b->px, b->w, b->h, &enc, &n)) {
        return;
    }
    free(d->out);
    d->out = enc;
    d->on = (unsigned long)n;
}

int gio_destination_finish(void *dst) {
    gio_dst *d = dst;
    if (!d || d->magic != GIO_MAGIC || !d->out) return 0;
    if (d->data) CFDataAppendBytes(d->data, d->out, (long)d->on);
    return 1;
}

int gio_image_wh(void *image, int *w, int *h) {
    gio_image *b = image;
    if (!b || !b->px) return 0;
    if (w) *w = b->w;
    if (h) *h = b->h;
    return 1;
}

uint32_t gio_image_px(void *image, int x, int y) {
    gio_image *b = image;
    if (!b || x < 0 || y < 0 || x >= b->w || y >= b->h) return 0;
    return b->px[y * b->w + x];
}
