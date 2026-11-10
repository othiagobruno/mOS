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
void *kCGImagePropertyTIFFDictionary;
void *kCGImagePropertyTIFFCompression;
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
    kCGImagePropertyTIFFDictionary = CFStringCreateWithCString(0, "{TIFF}", 0x08000100);
    kCGImagePropertyTIFFCompression = CFStringCreateWithCString(0, "Compression", 0x08000100);
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

static unsigned gio_u16(const unsigned char *p, int be) {
    return be ? ((unsigned)p[0] << 8 | p[1]) : ((unsigned)p[0] | (unsigned)p[1] << 8);
}
static unsigned gio_u32(const unsigned char *p, int be) {
    return be ? ((unsigned)p[0] << 24 | (unsigned)p[1] << 16 | (unsigned)p[2] << 8 | p[3])
              : ((unsigned)p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16 | (unsigned)p[3] << 24);
}

/* IFD do TIFF dentro do APP1 Exif. Tag 259 (Compression) vira o número do dicionário {TIFF}. */
static int exif_compression(const unsigned char *tif, unsigned long n, long long *out) {
    int be;
    unsigned off, count, i;
    if (n < 8) return 0;
    if (tif[0] == 'M' && tif[1] == 'M') be = 1;
    else if (tif[0] == 'I' && tif[1] == 'I') be = 0;
    else return 0;
    if (gio_u16(tif + 2, be) != 42) return 0;
    off = gio_u32(tif + 4, be);
    if ((unsigned long)off + 2 > n) return 0;
    count = gio_u16(tif + off, be);
    if ((unsigned long)off + 2 + (unsigned long)count * 12 > n) return 0;
    for (i = 0; i < count; i++) {
        const unsigned char *e = tif + off + 2 + i * 12;
        unsigned tag = gio_u16(e, be), typ = gio_u16(e + 2, be), cnt = gio_u32(e + 4, be);
        if (tag == 259 && typ == 3 && cnt >= 1) {
            *out = gio_u16(e + 8, be);
            return 1;
        }
    }
    return 0;
}

static int jpeg_exif_compression(const unsigned char *b, unsigned long n, long long *out) {
    unsigned long i;
    if (n < 4 || b[0] != 0xff || b[1] != 0xd8) return 0;
    i = 2;
    while (i + 4 < n) {
        unsigned m, len;
        if (b[i] != 0xff) return 0;
        while (i < n && b[i] == 0xff) i++;
        if (i >= n) return 0;
        m = b[i++];
        if (m == 0xd9 || m == 0xda) return 0;
        if (m == 0x01 || (m >= 0xd0 && m <= 0xd8)) continue;
        if (i + 2 > n) return 0;
        len = (unsigned)b[i] << 8 | b[i + 1];
        if (len < 2 || i + len > n) return 0;
        if (m == 0xe1 && len >= 8 && memcmp(b + i + 2, "Exif\0\0", 6) == 0)
            return exif_compression(b + i + 8, len - 8, out);
        i += len;
    }
    return 0;
}

void *CGImageSourceCopyPropertiesAtIndex(void *src, unsigned long index, void *opts) {
    gio_src *s = src;
    void *d, *nw, *nh;
    long long comp = 0;
    (void)opts;
    gio_keys();
    if (!s || s->magic != GIO_MAGIC || !s->ok || index != 0) return 0;
    d = CFDictionaryCreateMutable(0, 8, 0, 0);
    if (!d) return 0;
    nw = gio_num_obj(s->img.w);
    nh = gio_num_obj(s->img.h);
    CFDictionarySetValue(d, kCGImagePropertyPixelWidth, nw);
    CFDictionarySetValue(d, kCGImagePropertyPixelHeight, nh);
    if (jpeg_exif_compression(s->bytes, s->n, &comp)) {
        void *tiff = CFDictionaryCreateMutable(0, 4, 0, 0);
        if (tiff) {
            CFDictionarySetValue(tiff, kCGImagePropertyTIFFCompression, gio_num_obj(comp));
            CFDictionarySetValue(d, kCGImagePropertyTIFFDictionary, tiff);
        }
    }
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
