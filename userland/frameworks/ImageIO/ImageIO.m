/*
 * mOS — ImageIO.framework (Mach-O, /System/Library/Frameworks).
 *
 * Funcional: CGImageSource lê PNG e JPEG (baseline e progressivo, o mesmo decodificador de
 * imageio_decode.c) e devolve CGImage 8 bits, alfa reto. Propriedades PixelWidth, PixelHeight,
 * HasAlpha e Orientation (sempre 1: a orientação EXIF não é aplicada). Miniatura quando o
 * chamador pede kCGImageSourceCreateThumbnailFromImageAlways ou ...IfAbsent, no máximo
 * kCGImageSourceThumbnailMaxPixelSize na aresta maior. CGImageDestination grava PNG
 * (deflate armazenado) em URL ou em CFMutableData.
 *
 * Limitação: destino JPEG devolve falso (não há codificador JPEG). Sem EXIF, sem perfil de cor,
 * sem GIF/HEIF/WebP/TIFF, sem miniatura embutida no arquivo, sem cache de CGImageSource.
 * O CGImage do CoreGraphics recusa bitmaps acima de 32 MiB; arquivos maiores que isso decodificam
 * no teste do codec e falham na hora de virar CGImage.
 *
 * Quem pede: o Sparkle (ícone do app) e o OpenBot 0.66.0, que importa CGImageDestinationAddImage,
 * CGImageDestinationCreateWithURL, CGImageDestinationFinalize e kCGImageDestinationLossyCompressionQuality
 * (docs/compat-open-usage.md). tools/build-frameworks-objc.ps1
 */
#import <Foundation/Foundation.h>
#include "imageio_codec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef const void *CFTypeRef;
typedef const struct __CFString *CFStringRef;
typedef const struct __CFURL *CFURLRef;
typedef const struct __CFDictionary *CFDictionaryRef;
typedef const struct __CFData *CFDataRef;
typedef const void *CGImageRef;
typedef struct CGImageSource *CGImageSourceRef;
typedef struct CGImageDestination *CGImageDestinationRef;
typedef long CFIndex;
typedef unsigned char Boolean;

#define CFSTR_DEF(name, text) const CFStringRef name = (CFStringRef)__builtin___CFStringMakeConstantString(text);

CFSTR_DEF(kCGImagePropertyPixelWidth, "PixelWidth")
CFSTR_DEF(kCGImagePropertyPixelHeight, "PixelHeight")
CFSTR_DEF(kCGImagePropertyDPIWidth, "DPIWidth")
CFSTR_DEF(kCGImagePropertyDPIHeight, "DPIHeight")
CFSTR_DEF(kCGImagePropertyOrientation, "Orientation")
CFSTR_DEF(kCGImagePropertyHasAlpha, "HasAlpha")
CFSTR_DEF(kCGImageSourceCreateThumbnailFromImageAlways, "kCGImageSourceCreateThumbnailFromImageAlways")
CFSTR_DEF(kCGImageSourceCreateThumbnailFromImageIfAbsent, "kCGImageSourceCreateThumbnailFromImageIfAbsent")
CFSTR_DEF(kCGImageSourceThumbnailMaxPixelSize, "kCGImageSourceThumbnailMaxPixelSize")
CFSTR_DEF(kCGImageSourceShouldCache, "kCGImageSourceShouldCache")
CFSTR_DEF(kCGImageSourceShouldCacheImmediately, "kCGImageSourceShouldCacheImmediately")
CFSTR_DEF(kCGImageSourceCreateThumbnailWithTransform, "kCGImageSourceCreateThumbnailWithTransform")
CFSTR_DEF(kCGImageDestinationLossyCompressionQuality, "kCGImageDestinationLossyCompressionQuality")
static CFSTR_DEF(kMosUTTypePNG, "public.png")
static CFSTR_DEF(kMosUTTypeJPEG, "public.jpeg")

extern void *CGDataProviderCreateWithData(void *info, const void *data, size_t size, void (*release)(void *, const void *, size_t));
extern void CGDataProviderRelease(void *p);
extern void *CGImageCreate(size_t w, size_t h, size_t bpc, size_t bpp, size_t bpr, const void *space, uint32_t info,
                           void *provider, const void *decode, bool shouldInterpolate, int intent);
extern size_t CGImageGetWidth(void *img);
extern size_t CGImageGetHeight(void *img);
extern void *CGImageGetDataProvider(void *img);
extern void *CGDataProviderCopyData(void *p);
extern const uint8_t *CFDataGetBytePtr(void *d);
extern long CFDataGetLength(void *d);
extern void CFDataAppendBytes(void *d, const uint8_t *b, long n);
extern void CFRelease(void *o);
extern void *CFDictionaryGetValue(void *d, const void *key);
extern void *CFDictionaryCreate(void *alloc, const void **keys, const void **vals, long n, const void *kcb, const void *vcb);
extern void *CFNumberCreate(void *alloc, long type, const void *p);
extern unsigned char CFNumberGetValue(void *n, long type, void *out);
extern unsigned char CFBooleanGetValue(void *b);
extern unsigned char CFURLGetFileSystemRepresentation(void *url, unsigned char resolve, uint8_t *buf, long max);

/* kCGImageAlphaFirst: o CoreGraphics do mOS lê B,G,R,A, que é 0xAARRGGBB em little-endian */
enum { kAlphaFirst = 4, kCFNumberSInt64 = 4 };

static void limit_once(const char *what) {
    static int w;
    if (!w++) fprintf(stderr, "ImageIO[mOS]: %s\n", what);
}

@interface MOSCGImageSource : NSObject {
   @public
    uint8_t *bytes;
    size_t nbytes;
    int owns;
    mos_img_t img;
    int tried;
    const char *uti;
}
@end

@implementation MOSCGImageSource
- (void)dealloc {
    mos_img_free(&img);
    if (owns) free(bytes);
    [super dealloc];
}
@end

static MOSCGImageSource *as_src(CGImageSourceRef s) {
    return [ (id)s isKindOfClass:[MOSCGImageSource class] ] ? (MOSCGImageSource *)s : nil;
}

static int ensure(MOSCGImageSource *s) {
    if (!s || s->tried) return s && s->img.px ? 1 : 0;
    s->tried = 1;
    s->uti = mos_img_uti(s->bytes, s->nbytes);
    if (!s->uti) return 0;
    return mos_img_decode(s->bytes, s->nbytes, &s->img) == 0;
}

static MOSCGImageSource *source_new(uint8_t *bytes, size_t n, int owns) {
    if (!bytes || !mos_img_uti(bytes, n)) {
        if (owns) free(bytes);
        return nil;
    }
    MOSCGImageSource *s = [[MOSCGImageSource alloc] init];
    if (!s) { if (owns) free(bytes); return nil; }
    s->bytes = bytes;
    s->nbytes = n;
    s->owns = owns;
    s->uti = mos_img_uti(bytes, n);
    return s;
}

static uint8_t *copy_data(const uint8_t *p, size_t n) {
    if (!p || !n || n > 64u * 1024u * 1024u) return NULL;
    uint8_t *b = malloc(n);
    if (b) memcpy(b, p, n);
    return b;
}

static uint8_t *read_file(const char *path, size_t *n) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long sz = ftell(f);
    if (sz <= 0 || sz > 64L * 1024L * 1024L) { fclose(f); return NULL; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    uint8_t *b = malloc((size_t)sz);
    if (!b || fread(b, 1, (size_t)sz, f) != (size_t)sz) { free(b); fclose(f); return NULL; }
    fclose(f);
    *n = (size_t)sz;
    return b;
}

static CGImageRef image_from_px(const uint32_t *px, int w, int h) {
    if (!px || w <= 0 || h <= 0) return NULL;
    size_t nb = (size_t)w * (size_t)h * 4u;
    void *prov = CGDataProviderCreateWithData(0, px, nb, 0);
    if (!prov) return NULL;
    void *img = CGImageCreate((size_t)w, (size_t)h, 8, 32, (size_t)w * 4u, 0, kAlphaFirst, prov, 0, 0, 0);
    CGDataProviderRelease(prov);
    return (CGImageRef)img;
}

static int opt_on(CFDictionaryRef o, CFStringRef key) {
    if (!o || !key) return 0;
    void *v = CFDictionaryGetValue((void *)o, key);
    return v && CFBooleanGetValue(v);
}

static long long opt_i64(CFDictionaryRef o, CFStringRef key) {
    if (!o || !key) return 0;
    void *v = CFDictionaryGetValue((void *)o, key);
    long long n = 0;
    if (!v || !CFNumberGetValue(v, kCFNumberSInt64, &n)) return 0;
    return n;
}

static CGImageRef make_image(MOSCGImageSource *s, CFDictionaryRef opts, int thumb) {
    if (!ensure(s)) return NULL;
    int w = s->img.w, h = s->img.h;
    const uint32_t *px = s->img.px;
    uint32_t *scaled = NULL;
    if (thumb) {
        int ask = opt_on(opts, kCGImageSourceCreateThumbnailFromImageAlways) ||
                  opt_on(opts, kCGImageSourceCreateThumbnailFromImageIfAbsent);
        if (!ask) return NULL;
        long long maxpx = opt_i64(opts, kCGImageSourceThumbnailMaxPixelSize);
        int longedge = w > h ? w : h;
        if (maxpx > 0 && maxpx < longedge) {
            int nw = (int)((long long)w * maxpx / longedge);
            int nh = (int)((long long)h * maxpx / longedge);
            if (nw < 1) nw = 1;
            if (nh < 1) nh = 1;
            scaled = malloc((size_t)nw * (size_t)nh * 4u);
            if (!scaled) return NULL;
            for (int y = 0; y < nh; y++) {
                int sy = y * h / nh;
                for (int x = 0; x < nw; x++) scaled[(size_t)y * (size_t)nw + (size_t)x] = px[(size_t)sy * (size_t)w + (size_t)(x * w / nw)];
            }
            px = scaled;
            w = nw;
            h = nh;
        }
    }
    CGImageRef img = image_from_px(px, w, h);
    free(scaled);
    return img;
}

static CFDictionaryRef props(MOSCGImageSource *s) {
    if (!ensure(s)) return NULL;
    long long w = s->img.w, h = s->img.h, ori = 1, alpha = s->img.has_alpha ? 1 : 0;
    void *nw = CFNumberCreate(0, kCFNumberSInt64, &w);
    void *nh = CFNumberCreate(0, kCFNumberSInt64, &h);
    void *no = CFNumberCreate(0, kCFNumberSInt64, &ori);
    void *na = CFNumberCreate(0, kCFNumberSInt64, &alpha);
    const void *keys[] = { kCGImagePropertyPixelWidth, kCGImagePropertyPixelHeight, kCGImagePropertyOrientation, kCGImagePropertyHasAlpha };
    const void *vals[] = { nw, nh, no, na };
    void *d = CFDictionaryCreate(0, keys, vals, 4, 0, 0);
    if (nw) CFRelease(nw);
    if (nh) CFRelease(nh);
    if (no) CFRelease(no);
    if (na) CFRelease(na);
    return (CFDictionaryRef)d;
}

CGImageSourceRef CGImageSourceCreateWithURL(CFURLRef url, CFDictionaryRef opts) {
    (void)opts;
    uint8_t path[1024];
    if (!url || !CFURLGetFileSystemRepresentation((void *)url, 1, path, (long)sizeof path)) return NULL;
    size_t n = 0;
    uint8_t *b = read_file((const char *)path, &n);
    return (CGImageSourceRef)source_new(b, n, 1);
}

CGImageSourceRef CGImageSourceCreateWithData(CFTypeRef data, CFDictionaryRef opts) {
    (void)opts;
    const uint8_t *p = data ? CFDataGetBytePtr((void *)data) : NULL;
    long n = data ? CFDataGetLength((void *)data) : 0;
    if (!p || n <= 0) return NULL;
    return (CGImageSourceRef)source_new(copy_data(p, (size_t)n), (size_t)n, 1);
}

size_t CGImageSourceGetCount(CGImageSourceRef src) {
    MOSCGImageSource *s = as_src(src);
    return ensure(s) ? 1 : 0;
}

CFStringRef CGImageSourceGetType(CGImageSourceRef src) {
    MOSCGImageSource *s = as_src(src);
    if (!s || !s->uti) return NULL;
    if (s->uti[7] == 'p') return kMosUTTypePNG;
    if (s->uti[7] == 'j') return kMosUTTypeJPEG;
    return NULL;
}

CFDictionaryRef CGImageSourceCopyPropertiesAtIndex(CGImageSourceRef src, size_t i, CFDictionaryRef o) {
    (void)o;
    if (i != 0) return NULL;
    return props(as_src(src));
}

CFDictionaryRef CGImageSourceCopyProperties(CGImageSourceRef src, CFDictionaryRef o) {
    return CGImageSourceCopyPropertiesAtIndex(src, 0, o);
}

CGImageRef CGImageSourceCreateImageAtIndex(CGImageSourceRef src, size_t i, CFDictionaryRef o) {
    (void)o;
    if (i != 0) return NULL;
    return make_image(as_src(src), o, 0);
}

CGImageRef CGImageSourceCreateThumbnailAtIndex(CGImageSourceRef src, size_t i, CFDictionaryRef o) {
    if (i != 0) return NULL;
    return make_image(as_src(src), o, 1);
}

/* ---------------------------------------------------------------- destino: PNG funcional, JPEG ainda não */
@interface MOSCGImageDestination : NSObject {
   @public
    char *path;
    id data;
    int jpeg;
    uint32_t *px;
    int w, h;
    int count;
}
@end

@implementation MOSCGImageDestination
- (void)dealloc {
    free(path);
    [data release];
    free(px);
    [super dealloc];
}
@end

static int type_is_jpeg(CFStringRef type) {
    if (!type) return 0;
    NSString *s = (NSString *)type;
    return [s isEqualToString:@"public.jpeg"] || [s isEqualToString:@"public.jpg"] || [s isEqualToString:@"image/jpeg"];
}

static int type_is_png(CFStringRef type) {
    if (!type) return 0;
    NSString *s = (NSString *)type;
    return [s isEqualToString:@"public.png"] || [s isEqualToString:@"image/png"];
}

static MOSCGImageDestination *dest_new(CFStringRef type, size_t count) {
    if (count < 1 || (!type_is_png(type) && !type_is_jpeg(type))) return nil;
    MOSCGImageDestination *d = [[MOSCGImageDestination alloc] init];
    if (!d) return nil;
    d->jpeg = type_is_jpeg(type);
    d->count = (int)count;
    return d;
}

CGImageDestinationRef CGImageDestinationCreateWithURL(CFURLRef url, CFStringRef type, size_t count, CFDictionaryRef o) {
    (void)o;
    uint8_t path[1024];
    MOSCGImageDestination *d = dest_new(type, count);
    if (!d || !url || !CFURLGetFileSystemRepresentation((void *)url, 1, path, (long)sizeof path)) {
        [d release];
        return NULL;
    }
    d->path = strdup((const char *)path);
    if (!d->path) { [d release]; return NULL; }
    return (CGImageDestinationRef)d;
}

CGImageDestinationRef CGImageDestinationCreateWithData(CFTypeRef data, CFStringRef type, size_t count, CFDictionaryRef o) {
    (void)o;
    MOSCGImageDestination *d = dest_new(type, count);
    if (!d || !data) { [d release]; return NULL; }
    d->data = [(id)data retain];
    return (CGImageDestinationRef)d;
}

static int take_image(MOSCGImageDestination *d, CGImageRef img) {
    if (!d || !img || d->px) return 0;
    size_t w = CGImageGetWidth((void *)img), h = CGImageGetHeight((void *)img);
    void *prov = CGImageGetDataProvider((void *)img);
    void *cf = prov ? CGDataProviderCopyData(prov) : NULL;
    const uint8_t *b = cf ? CFDataGetBytePtr(cf) : NULL;
    long n = cf ? CFDataGetLength(cf) : 0;
    if (!b || w == 0 || h == 0 || (size_t)n < w * h * 4u) { if (cf) CFRelease(cf); return 0; }
    uint32_t *px = malloc(w * h * 4u);
    if (!px) { CFRelease(cf); return 0; }
    memcpy(px, b, w * h * 4u);
    CFRelease(cf);
    d->px = px;
    d->w = (int)w;
    d->h = (int)h;
    return 1;
}

void CGImageDestinationAddImage(CGImageDestinationRef dest, CGImageRef img, CFDictionaryRef p) {
    (void)p;
    MOSCGImageDestination *d = [(id)dest isKindOfClass:[MOSCGImageDestination class]] ? (MOSCGImageDestination *)dest : nil;
    take_image(d, img);
}

void CGImageDestinationAddImageFromSource(CGImageDestinationRef dest, CGImageSourceRef src, size_t i, CFDictionaryRef p) {
    CGImageRef img = CGImageSourceCreateImageAtIndex(src, i, p);
    CGImageDestinationAddImage(dest, img, p);
    if (img) CFRelease((void *)img);
}

void CGImageDestinationSetProperties(CGImageDestinationRef dest, CFDictionaryRef p) { (void)dest; (void)p; }

Boolean CGImageDestinationFinalize(CGImageDestinationRef dest) {
    MOSCGImageDestination *d = [(id)dest isKindOfClass:[MOSCGImageDestination class]] ? (MOSCGImageDestination *)dest : nil;
    if (!d || !d->px) return 0;
    if (d->jpeg) {
        limit_once("destino JPEG ainda não grava (CGImageDestinationFinalize devolve falso); PNG grava");
        return 0;
    }
    uint8_t *png = NULL;
    size_t n = 0;
    if (mos_img_encode_png(d->px, d->w, d->h, &png, &n) != 0) return 0;
    int ok = 0;
    if (d->path) {
        FILE *f = fopen(d->path, "wb");
        ok = f && fwrite(png, 1, n, f) == n;
        if (f) fclose(f);
    } else if (d->data) {
        CFDataAppendBytes(d->data, png, (long)n);
        ok = 1;
    }
    free(png);
    return ok ? 1 : 0;
}
