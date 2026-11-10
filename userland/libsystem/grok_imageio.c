/*
 * ImageIO que o Grok Bot 0.79.3 importa: PNG e JPEG de verdade
 * (imageio_decode.c / imageio_encode.c). Miniatura reduz a aresta maior.
 * Destino grava PNG. JPEG de saída não existe neste codec.
 */
#include "sys.h"
#include "libc.h"

const char kCGImagePropertyPixelWidth[] = "PixelWidth";
const char kCGImagePropertyPixelHeight[] = "PixelHeight";
const char kCGImageSourceCreateThumbnailFromImageAlways[] = "kCGImageSourceCreateThumbnailFromImageAlways";
const char kCGImageSourceCreateThumbnailWithTransform[] = "kCGImageSourceCreateThumbnailWithTransform";
const char kCGImageSourceShouldCache[] = "kCGImageSourceShouldCache";
const char kCGImageSourceShouldCacheImmediately[] = "kCGImageSourceShouldCacheImmediately";
const char kCGImageSourceThumbnailMaxPixelSize[] = "kCGImageSourceThumbnailMaxPixelSize";
extern const char kCGImageDestinationLossyCompressionQuality[];

#include "../frameworks/ImageIO/imageio_decode.c"
#include "../frameworks/ImageIO/imageio_encode.c"

extern const unsigned char *CFDataGetBytePtr(void *d);
extern long CFDataGetLength(void *d);
extern void CFDataAppendBytes(void *d, const unsigned char *b, long n);

typedef struct {
    unsigned char *bytes;
    unsigned long n;
    mos_img_t img;
    int ok;
} gio_src;

typedef struct {
    void *data;
    unsigned char *png;
    unsigned long pn;
} gio_dst;

static int truthy(void *v) {
    if (!v) return 0;
    if (v == (void *)kCGImageSourceCreateThumbnailFromImageAlways) return 1;
    return 1;
}

void *CGImageSourceCreateWithData(void *data, void *opts) {
    gio_src *s;
    const unsigned char *p;
    long n;
    (void)opts;
    if (!data) return 0;
    p = CFDataGetBytePtr(data);
    n = CFDataGetLength(data);
    if (!p || n < 8) return 0;
    s = calloc(1, sizeof *s);
    if (!s) return 0;
    s->bytes = malloc((unsigned long)n);
    if (!s->bytes) { free(s); return 0; }
    memcpy(s->bytes, p, (unsigned long)n);
    s->n = (unsigned long)n;
    s->ok = mos_img_decode(s->bytes, s->n, &s->img) == 0;
    return s;
}

unsigned long CGImageSourceGetCount(void *src) {
    gio_src *s = src;
    return s && s->ok ? 1 : 0;
}

const char *CGImageSourceGetType(void *src) {
    gio_src *s = src;
    if (!s) return 0;
    return mos_img_uti(s->bytes, s->n);
}

static void *image_box(const uint32_t *px, int w, int h) {
    struct { int w, h; uint32_t *px; } *b;
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
    if (!s || !s->ok || index != 0) return 0;
    return image_box(s->img.px, s->img.w, s->img.h);
}

void *CGImageSourceCopyPropertiesAtIndex(void *src, unsigned long index, void *opts) {
    gio_src *s = src;
    int *p;
    (void)opts;
    if (!s || !s->ok || index != 0) return 0;
    p = malloc(2 * sizeof *p);
    if (!p) return 0;
    p[0] = s->img.w;
    p[1] = s->img.h;
    return p;
}

void *CGImageSourceCreateThumbnailAtIndex(void *src, unsigned long index, void *opts) {
    gio_src *s = src;
    int maxe, w, h, nw, nh, x, y;
    uint32_t *px;
    (void)opts;
    if (!s || !s->ok || index != 0) return 0;
    w = s->img.w;
    h = s->img.h;
    maxe = w > h ? w : h;
    if (maxe < 1) return 0;
    if (maxe > 64) {
        nw = w * 64 / maxe;
        nh = h * 64 / maxe;
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
    {
        void *box = image_box(px, nw, nh);
        free(px);
        (void)truthy(opts);
        return box;
    }
}

void *CGImageDestinationCreateWithData(void *data, const void *type, unsigned long count, void *opts) {
    gio_dst *d;
    (void)type;
    (void)count;
    (void)opts;
    d = calloc(1, sizeof *d);
    if (!d) return 0;
    d->data = data;
    return d;
}

void gio_destination_add(void *dst, void *image) {
    struct { int w, h; uint32_t *px; } *b = image;
    gio_dst *d = dst;
    unsigned char *png = 0;
    size_t n = 0;
    if (!d || !b || !b->px) return;
    if (mos_img_encode_png(b->px, b->w, b->h, &png, &n)) return;
    free(d->png);
    d->png = png;
    d->pn = (unsigned long)n;
}

int gio_destination_finish(void *dst) {
    gio_dst *d = dst;
    if (!d || !d->png) return 0;
    if (d->data) CFDataAppendBytes(d->data, d->png, (long)d->pn);
    return 1;
}
