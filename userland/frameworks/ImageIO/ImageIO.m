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
#include "imageio_exif.h"
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
/* dicionários e chaves de metadados, com os mesmos textos do ImageIO da Apple */
CFSTR_DEF(kCGImagePropertyExifDictionary, "{Exif}")
CFSTR_DEF(kCGImagePropertyTIFFDictionary, "{TIFF}")
CFSTR_DEF(kCGImagePropertyGIFDictionary, "{GIF}")
CFSTR_DEF(kCGImagePropertyPNGDictionary, "{PNG}")
CFSTR_DEF(kCGImagePropertyWebPDictionary, "{WebP}")
CFSTR_DEF(kCGImagePropertyHEICSDictionary, "{HEICS}")
CFSTR_DEF(kCGImagePropertyIPTCDictionary, "{IPTC}")
CFSTR_DEF(kCGImagePropertyMakerAppleDictionary, "{MakerApple}")
CFSTR_DEF(kCGImagePropertyTIFFMake, "Make")
CFSTR_DEF(kCGImagePropertyTIFFModel, "Model")
CFSTR_DEF(kCGImagePropertyTIFFSoftware, "Software")
CFSTR_DEF(kCGImagePropertyTIFFDateTime, "DateTime")
CFSTR_DEF(kCGImagePropertyTIFFOrientation, "Orientation")
CFSTR_DEF(kCGImagePropertyExifExposureTime, "ExposureTime")
CFSTR_DEF(kCGImagePropertyExifFNumber, "FNumber")
CFSTR_DEF(kCGImagePropertyExifISOSpeedRatings, "ISOSpeedRatings")
CFSTR_DEF(kCGImagePropertyExifDateTimeOriginal, "DateTimeOriginal")
CFSTR_DEF(kCGImagePropertyExifDateTimeDigitized, "DateTimeDigitized")
CFSTR_DEF(kCGImagePropertyExifOffsetTimeOriginal, "OffsetTimeOriginal")
CFSTR_DEF(kCGImagePropertyExifShutterSpeedValue, "ShutterSpeedValue")
CFSTR_DEF(kCGImagePropertyExifApertureValue, "ApertureValue")
CFSTR_DEF(kCGImagePropertyExifExposureBiasValue, "ExposureBiasValue")
CFSTR_DEF(kCGImagePropertyExifMeteringMode, "MeteringMode")
CFSTR_DEF(kCGImagePropertyExifFlash, "Flash")
CFSTR_DEF(kCGImagePropertyExifFocalLength, "FocalLength")
CFSTR_DEF(kCGImagePropertyExifMakerNote, "MakerNote")
CFSTR_DEF(kCGImagePropertyExifColorSpace, "ColorSpace")
CFSTR_DEF(kCGImagePropertyExifPixelXDimension, "PixelXDimension")
CFSTR_DEF(kCGImagePropertyExifPixelYDimension, "PixelYDimension")
CFSTR_DEF(kCGImagePropertyExifSensingMethod, "SensingMethod")
CFSTR_DEF(kCGImagePropertyExifWhiteBalance, "WhiteBalance")
CFSTR_DEF(kCGImagePropertyExifSceneCaptureType, "SceneCaptureType")
CFSTR_DEF(kCGImagePropertyExifLensSpecification, "LensSpecification")
CFSTR_DEF(kCGImagePropertyExifLensMake, "LensMake")
CFSTR_DEF(kCGImagePropertyExifLensModel, "LensModel")
CFSTR_DEF(kCGImagePropertyGIFDelayTime, "DelayTime")
CFSTR_DEF(kCGImagePropertyGIFUnclampedDelayTime, "UnclampedDelayTime")
CFSTR_DEF(kCGImagePropertyGIFLoopCount, "LoopCount")
CFSTR_DEF(kCGImagePropertyHEICSDelayTime, "DelayTime")
CFSTR_DEF(kCGImagePropertyHEICSUnclampedDelayTime, "UnclampedDelayTime")
CFSTR_DEF(kCGImagePropertyWebPDelayTime, "DelayTime")
CFSTR_DEF(kCGImagePropertyWebPUnclampedDelayTime, "UnclampedDelayTime")
CFSTR_DEF(kCGImagePropertyAPNGUnclampedDelayTime, "UnclampedDelayTime")
CFSTR_DEF(kCGImagePropertyIPTCExtDigitalSourceType, "DigitalSourceType")
CFSTR_DEF(kCGImagePropertyGroups, "Groups")
CFSTR_DEF(kCGImagePropertyGroupType, "GroupType")
CFSTR_DEF(kCGImagePropertyGroupTypeStereoPair, "StereoPair")
CFSTR_DEF(kCGImageAuxiliaryDataInfoMetadata, "kCGImageAuxiliaryDataInfoMetadata")
CFSTR_DEF(kCGImageAuxiliaryDataTypeHDRGainMap, "kCGImageAuxiliaryDataTypeHDRGainMap")
CFSTR_DEF(kCGImageSourceTypeIdentifierHint, "kCGImageSourceTypeIdentifierHint")
CFSTR_DEF(kCGImageSourceShouldAllowFloat, "kCGImageSourceShouldAllowFloat")
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
extern void *CFDictionaryCreateMutable(void *alloc, long cap, const void *kcb, const void *vcb);
extern void CFDictionarySetValue(void *d, const void *k, const void *v);
extern void *CFStringCreateWithCString(void *alloc, const char *s, uint32_t enc);
extern void *CFArrayCreate(void *alloc, const void **vals, long n, const void *cb);
extern void *CFDataCreate(void *alloc, const uint8_t *b, long n);
extern void *CFRetain(void *o);

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
    mos_exif exif;
    int exif_state;              /* 0 ainda não lido, 1 achou, -1 não há */
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

static const mos_exif *exif_of(MOSCGImageSource *s) {
    if (!s) return NULL;
    if (!s->exif_state) s->exif_state = mos_exif_parse(s->bytes, s->nbytes, &s->exif) == 0 ? 1 : -1;
    return s->exif_state > 0 ? &s->exif : NULL;
}
enum { kCFNumberFloat64 = 13 };
static void *cf_str(const char *s) { return CFStringCreateWithCString(0, s, 0x08000100 /* UTF-8 */); }
static void *cf_num(double v) { return CFNumberCreate(0, kCFNumberFloat64, &v); }
/* valor de uma entrada EXIF como o ImageIO da Apple entrega: texto, número, array de números ou dados */
static void *exif_value(const mos_exif_entry *e) {
    if (e->kind == EXIF_STR) return cf_str(e->str);
    if (e->kind == EXIF_BYTES) return CFDataCreate(0, e->bytes, (long)e->nbytes);
    if (e->nnum == 1 && e->tag != 0x8827 && e->tag != 0xA432) return cf_num(e->num[0]);
    const void *v[4];
    for (int i = 0; i < e->nnum; i++) v[i] = cf_num(e->num[i]);
    void *a = CFArrayCreate(0, v, e->nnum, 0);
    for (int i = 0; i < e->nnum; i++) CFRelease((void *)v[i]);
    return a;
}
static void *exif_dict(const mos_exif *x, int ifd) {
    void *d = CFDictionaryCreateMutable(0, 0, 0, 0);
    for (int i = 0; x && i < x->n; i++) {
        if (x->e[i].ifd != ifd) continue;
        void *k = cf_str(mos_exif_name(ifd, x->e[i].tag)), *v = exif_value(&x->e[i]);
        if (k && v) CFDictionarySetValue(d, k, v);
        if (k) CFRelease(k);
        if (v) CFRelease(v);
    }
    return d;
}

static CFDictionaryRef props(MOSCGImageSource *s) {
    if (!ensure(s)) return NULL;
    long long w = s->img.w, h = s->img.h, ori = 1, alpha = s->img.has_alpha ? 1 : 0;
    const mos_exif *x = exif_of(s);
    const mos_exif_entry *oe = x ? mos_exif_find(x, EXIF_IFD_TIFF, 0x0112) : NULL;
    if (oe && oe->kind == EXIF_NUM && oe->num[0] >= 1 && oe->num[0] <= 8) ori = (long long)oe->num[0];
    void *nw = CFNumberCreate(0, kCFNumberSInt64, &w);
    void *nh = CFNumberCreate(0, kCFNumberSInt64, &h);
    void *no = CFNumberCreate(0, kCFNumberSInt64, &ori);
    void *na = CFNumberCreate(0, kCFNumberSInt64, &alpha);
    const void *keys[] = { kCGImagePropertyPixelWidth, kCGImagePropertyPixelHeight, kCGImagePropertyOrientation, kCGImagePropertyHasAlpha };
    const void *vals[] = { nw, nh, no, na };
    void *d = CFDictionaryCreateMutable(0, 0, 0, 0);
    for (int i = 0; i < 4; i++) if (vals[i]) CFDictionarySetValue(d, keys[i], vals[i]);
    if (x) {
        void *tiff = exif_dict(x, EXIF_IFD_TIFF), *ex = exif_dict(x, EXIF_IFD_EXIF);
        CFDictionarySetValue(d, kCGImagePropertyTIFFDictionary, tiff);
        CFDictionarySetValue(d, kCGImagePropertyExifDictionary, ex);
        CFRelease(tiff);
        CFRelease(ex);
    }
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

/* provedor: os bytes são copiados (o provedor pode sumir antes da fonte) */
CGImageSourceRef CGImageSourceCreateWithDataProvider(void *provider, CFDictionaryRef opts) {
    void *data = provider ? CGDataProviderCopyData(provider) : NULL;
    if (!data) return NULL;
    CGImageSourceRef s = CGImageSourceCreateWithData(data, opts);
    CFRelease(data);
    return s;
}

/* CGImageSourceStatus: 0 Complete, -3 UnknownType, -4 InvalidData */
int CGImageSourceGetStatusAtIndex(CGImageSourceRef src, size_t i) {
    MOSCGImageSource *s = as_src(src);
    if (!s || !s->uti) return -3;
    if (i != 0) return -4;
    return ensure(s) ? 0 : -4;
}
int CGImageSourceGetStatus(CGImageSourceRef src) { return CGImageSourceGetStatusAtIndex(src, 0); }

/* PNG e JPEG baseline do mOS não carregam mapa de ganho HDR nem profundidade: NULL, como a Apple sem o dado */
CFDictionaryRef CGImageSourceCopyAuxiliaryDataInfoAtIndex(CGImageSourceRef src, size_t i, CFStringRef type) {
    (void)src; (void)i; (void)type;
    return NULL;
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

/* ---------------------------------------------------------------- CGImageMetadata sobre o EXIF lido
 * Caminhos no formato XMP do ImageIO: "exif:DateTimeOriginal", "tiff:Make". A fonte fica retida porque as
 * entradas apontam para os bytes dela. */
@interface MOSCGImageMetadata : NSObject {
   @public
    MOSCGImageSource *src;
}
@end
@implementation MOSCGImageMetadata
- (void)dealloc { [src release]; [super dealloc]; }
@end
@interface MOSCGImageMetadataTag : NSObject {
   @public
    void *value;
}
@end
@implementation MOSCGImageMetadataTag
- (void)dealloc { if (value) CFRelease(value); [super dealloc]; }
@end

unsigned long CGImageMetadataGetTypeID(void) { return 0x2B0; }
unsigned long CGImageMetadataTagGetTypeID(void) { return 0x2B1; }

void *CGImageSourceCopyMetadataAtIndex(CGImageSourceRef src, size_t i, CFDictionaryRef o) {
    (void)o;
    MOSCGImageSource *s = as_src(src);
    if (!s || i != 0 || !exif_of(s)) return NULL;
    MOSCGImageMetadata *m = [[MOSCGImageMetadata alloc] init];
    if (m) m->src = [s retain];
    return m;
}

static const mos_exif_entry *meta_find(void *meta, const char *path) {
    MOSCGImageMetadata *m = meta;
    if (!m || ![(id)m isKindOfClass:[MOSCGImageMetadata class]] || !path) return NULL;
    int ifd;
    if (!strncmp(path, "exif:", 5)) ifd = EXIF_IFD_EXIF;
    else if (!strncmp(path, "tiff:", 5)) ifd = EXIF_IFD_TIFF;
    else return NULL;
    const mos_exif *x = exif_of(m->src);
    for (int k = 0; x && k < x->n; k++)
        if (x->e[k].ifd == ifd && !strcmp(mos_exif_name(ifd, x->e[k].tag), path + 5)) return &x->e[k];
    return NULL;
}
static const char *path_c(CFStringRef path) { return path ? [(NSString *)path UTF8String] : NULL; }

CFStringRef CGImageMetadataCopyStringValueWithPath(void *meta, void *parent, CFStringRef path) {
    (void)parent;
    const mos_exif_entry *e = meta_find(meta, path_c(path));
    if (!e) return NULL;
    if (e->kind == EXIF_STR) return (CFStringRef)cf_str(e->str);
    if (e->kind == EXIF_BYTES) return NULL;
    char buf[160];
    size_t at = 0;
    for (int i = 0; i < e->nnum && at < sizeof buf - 24; i++)
        at += (size_t)snprintf(buf + at, sizeof buf - at, i ? ", %g" : "%g", e->num[i]);
    buf[at] = 0;
    return (CFStringRef)cf_str(buf);
}
void *CGImageMetadataCopyTagWithPath(void *meta, void *parent, CFStringRef path) {
    (void)parent;
    const mos_exif_entry *e = meta_find(meta, path_c(path));
    if (!e) return NULL;
    MOSCGImageMetadataTag *t = [[MOSCGImageMetadataTag alloc] init];
    if (t) t->value = exif_value(e);
    return t;
}
void *CGImageMetadataTagCopyValue(void *tag) {
    MOSCGImageMetadataTag *t = tag;
    if (!t || ![(id)t isKindOfClass:[MOSCGImageMetadataTag class]] || !t->value) return NULL;
    return CFRetain(t->value);
}
