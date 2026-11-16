/*
 * mOS ImageIO — CGImageSource e CGImageDestination. Incluído por imageio.c (fora do teste de host).
 * As duas são objetos CF de verdade (classe ObjC do mOS, CFRetain/CFRelease/CFGetTypeID funcionam).
 * As imagens que saem daqui são CGImage do coregraphics.c, B,G,R,A em memória
 * (kCGImageAlphaFirst | kCGBitmapByteOrder32Little), alfa reto.
 */
#ifdef IIO_CF_HOST
#include "iio_cf_host.h"
#else
#include "objc.h"
#endif

extern char iio_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } iio_cfconst_t;
#define IIOK(var, text) \
    static iio_cfconst_t var##_box = {(void *)iio_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; \
    const void *var = &var##_box;

/* chaves de propriedade (mesmos textos do ImageIO da Apple) */
IIOK(kCGImagePropertyFileSize, "FileSize")
IIOK(kCGImagePropertyPixelWidth, "PixelWidth")
IIOK(kCGImagePropertyPixelHeight, "PixelHeight")
IIOK(kCGImagePropertyDepth, "Depth")
IIOK(kCGImagePropertyColorModel, "ColorModel")
IIOK(kCGImagePropertyColorModelRGB, "RGB")
IIOK(kCGImagePropertyColorModelGray, "Gray")
IIOK(kCGImagePropertyColorModelCMYK, "CMYK")
IIOK(kCGImagePropertyColorModelLab, "Lab")
IIOK(kCGImagePropertyHasAlpha, "HasAlpha")
IIOK(kCGImagePropertyIsIndexed, "IsIndexed")
IIOK(kCGImagePropertyIsFloat, "IsFloat")
IIOK(kCGImagePropertyOrientation, "Orientation")
IIOK(kCGImagePropertyDPIWidth, "DPIWidth")
IIOK(kCGImagePropertyDPIHeight, "DPIHeight")
IIOK(kCGImagePropertyProfileName, "ProfileName")
IIOK(kCGImagePropertyPrimaryImage, "PrimaryImage")
IIOK(kCGImagePropertyImageCount, "ImageCount")
IIOK(kCGImagePropertyImages, "Images")
IIOK(kCGImagePropertyThumbnailImages, "ThumbnailImages")
IIOK(kCGImagePropertyTIFFDictionary, "{TIFF}")
IIOK(kCGImagePropertyExifDictionary, "{Exif}")
IIOK(kCGImagePropertyExifAuxDictionary, "{ExifAux}")
IIOK(kCGImagePropertyGPSDictionary, "{GPS}")
IIOK(kCGImagePropertyIPTCDictionary, "{IPTC}")
IIOK(kCGImagePropertyJFIFDictionary, "{JFIF}")
IIOK(kCGImagePropertyGIFDictionary, "{GIF}")
IIOK(kCGImagePropertyPNGDictionary, "{PNG}")
IIOK(kCGImagePropertyBMPDictionary, "{BMP}")
IIOK(kCGImagePropertyWebPDictionary, "{WebP}")
IIOK(kCGImagePropertyHEICSDictionary, "{HEICS}")
IIOK(kCGImagePropertyHEIFDictionary, "{HEIF}")
IIOK(kCGImagePropertyMakerAppleDictionary, "{MakerApple}")
IIOK(kCGImagePropertyRawDictionary, "{Raw}")
IIOK(kCGImagePropertyCIFFDictionary, "{CIFF}")
IIOK(kCGImageProperty8BIMDictionary, "{8BIM}")
IIOK(kCGImagePropertyDNGDictionary, "{DNG}")
IIOK(kCGImagePropertyOpenEXRDictionary, "{OpenEXR}")
IIOK(kCGImagePropertyTGADictionary, "{TGA}")
IIOK(kCGImagePropertyTIFFCompression, "Compression")
IIOK(kCGImagePropertyTIFFPhotometricInterpretation, "PhotometricInterpretation")
IIOK(kCGImagePropertyTIFFOrientation, "Orientation")
IIOK(kCGImagePropertyTIFFXResolution, "XResolution")
IIOK(kCGImagePropertyTIFFYResolution, "YResolution")
IIOK(kCGImagePropertyTIFFResolutionUnit, "ResolutionUnit")
IIOK(kCGImagePropertyTIFFMake, "Make")
IIOK(kCGImagePropertyTIFFModel, "Model")
IIOK(kCGImagePropertyTIFFSoftware, "Software")
IIOK(kCGImagePropertyTIFFDateTime, "DateTime")
IIOK(kCGImagePropertyTIFFArtist, "Artist")
IIOK(kCGImagePropertyTIFFCopyright, "Copyright")
IIOK(kCGImagePropertyTIFFImageDescription, "ImageDescription")
IIOK(kCGImagePropertyExifExposureTime, "ExposureTime")
IIOK(kCGImagePropertyExifFNumber, "FNumber")
IIOK(kCGImagePropertyExifExposureProgram, "ExposureProgram")
IIOK(kCGImagePropertyExifISOSpeedRatings, "ISOSpeedRatings")
IIOK(kCGImagePropertyExifDateTimeOriginal, "DateTimeOriginal")
IIOK(kCGImagePropertyExifDateTimeDigitized, "DateTimeDigitized")
IIOK(kCGImagePropertyExifOffsetTime, "OffsetTime")
IIOK(kCGImagePropertyExifOffsetTimeOriginal, "OffsetTimeOriginal")
IIOK(kCGImagePropertyExifOffsetTimeDigitized, "OffsetTimeDigitized")
IIOK(kCGImagePropertyExifShutterSpeedValue, "ShutterSpeedValue")
IIOK(kCGImagePropertyExifApertureValue, "ApertureValue")
IIOK(kCGImagePropertyExifBrightnessValue, "BrightnessValue")
IIOK(kCGImagePropertyExifExposureBiasValue, "ExposureBiasValue")
IIOK(kCGImagePropertyExifMeteringMode, "MeteringMode")
IIOK(kCGImagePropertyExifFlash, "Flash")
IIOK(kCGImagePropertyExifFocalLength, "FocalLength")
IIOK(kCGImagePropertyExifMakerNote, "MakerNote")
IIOK(kCGImagePropertyExifSubsecTime, "SubsecTime")
IIOK(kCGImagePropertyExifSubsecTimeOriginal, "SubsecTimeOriginal")
IIOK(kCGImagePropertyExifColorSpace, "ColorSpace")
IIOK(kCGImagePropertyExifPixelXDimension, "PixelXDimension")
IIOK(kCGImagePropertyExifPixelYDimension, "PixelYDimension")
IIOK(kCGImagePropertyExifSensingMethod, "SensingMethod")
IIOK(kCGImagePropertyExifExposureMode, "ExposureMode")
IIOK(kCGImagePropertyExifWhiteBalance, "WhiteBalance")
IIOK(kCGImagePropertyExifFocalLenIn35mmFilm, "FocalLenIn35mmFilm")
IIOK(kCGImagePropertyExifSceneCaptureType, "SceneCaptureType")
IIOK(kCGImagePropertyExifLensSpecification, "LensSpecification")
IIOK(kCGImagePropertyExifLensMake, "LensMake")
IIOK(kCGImagePropertyExifLensModel, "LensModel")
IIOK(kCGImagePropertyJFIFVersion, "JFIFVersion")
IIOK(kCGImagePropertyJFIFXDensity, "XDensity")
IIOK(kCGImagePropertyJFIFYDensity, "YDensity")
IIOK(kCGImagePropertyJFIFDensityUnit, "DensityUnit")
IIOK(kCGImagePropertyJFIFIsProgressive, "IsProgressive")
IIOK(kCGImagePropertyGIFLoopCount, "LoopCount")
IIOK(kCGImagePropertyGIFDelayTime, "DelayTime")
IIOK(kCGImagePropertyGIFUnclampedDelayTime, "UnclampedDelayTime")
IIOK(kCGImagePropertyGIFHasGlobalColorMap, "HasGlobalColorMap")
IIOK(kCGImagePropertyGIFImageColorMap, "ImageColorMap")
IIOK(kCGImagePropertyGIFCanvasPixelWidth, "CanvasPixelWidth")
IIOK(kCGImagePropertyGIFCanvasPixelHeight, "CanvasPixelHeight")
IIOK(kCGImagePropertyGIFFrameInfoArray, "FrameInfo")
IIOK(kCGImagePropertyPNGGamma, "Gamma")
IIOK(kCGImagePropertyPNGInterlaceType, "InterlaceType")
IIOK(kCGImagePropertyPNGsRGBIntent, "sRGBIntent")
IIOK(kCGImagePropertyPNGXPixelsPerMeter, "XPixelsPerMeter")
IIOK(kCGImagePropertyPNGYPixelsPerMeter, "YPixelsPerMeter")
IIOK(kCGImagePropertyPNGCompressionFilter, "kCGImagePropertyPNGCompressionFilter")
IIOK(kCGImagePropertyAPNGLoopCount, "LoopCount")
IIOK(kCGImagePropertyAPNGDelayTime, "DelayTime")
IIOK(kCGImagePropertyAPNGUnclampedDelayTime, "UnclampedDelayTime")
IIOK(kCGImagePropertyAPNGFrameInfoArray, "FrameInfo")
IIOK(kCGImagePropertyAPNGCanvasPixelWidth, "CanvasPixelWidth")
IIOK(kCGImagePropertyAPNGCanvasPixelHeight, "CanvasPixelHeight")
IIOK(kCGImagePropertyWebPLoopCount, "LoopCount")
IIOK(kCGImagePropertyWebPDelayTime, "DelayTime")
IIOK(kCGImagePropertyWebPUnclampedDelayTime, "UnclampedDelayTime")
IIOK(kCGImagePropertyWebPFrameInfoArray, "FrameInfo")
IIOK(kCGImagePropertyWebPCanvasPixelWidth, "CanvasPixelWidth")
IIOK(kCGImagePropertyWebPCanvasPixelHeight, "CanvasPixelHeight")
IIOK(kCGImagePropertyHEICSLoopCount, "LoopCount")
IIOK(kCGImagePropertyHEICSDelayTime, "DelayTime")
IIOK(kCGImagePropertyHEICSUnclampedDelayTime, "UnclampedDelayTime")
IIOK(kCGImagePropertyHEICSFrameInfoArray, "FrameInfo")
IIOK(kCGImagePropertyHEICSCanvasPixelWidth, "CanvasPixelWidth")
IIOK(kCGImagePropertyHEICSCanvasPixelHeight, "CanvasPixelHeight")
IIOK(kCGImagePropertyIPTCExtDigitalSourceType, "DigitalSourceType")
IIOK(kCGImagePropertyGroups, "Groups")
IIOK(kCGImagePropertyGroupType, "GroupType")
IIOK(kCGImagePropertyGroupTypeStereoPair, "StereoPair")
IIOK(kCGImagePropertyGroupTypeAlternate, "Alternate")
IIOK(kCGImagePropertyGroupIndex, "GroupIndex")
IIOK(kCGImagePropertyGroupImageIndexLeft, "GroupImageIndexLeft")
IIOK(kCGImagePropertyGroupImageIndexRight, "GroupImageIndexRight")
IIOK(kCGImageAuxiliaryDataInfoData, "kCGImageAuxiliaryDataInfoData")
IIOK(kCGImageAuxiliaryDataInfoDataDescription, "kCGImageAuxiliaryDataInfoDataDescription")
IIOK(kCGImageAuxiliaryDataInfoMetadata, "kCGImageAuxiliaryDataInfoMetadata")
IIOK(kCGImageAuxiliaryDataTypeDepth, "kCGImageAuxiliaryDataTypeDepth")
IIOK(kCGImageAuxiliaryDataTypeDisparity, "kCGImageAuxiliaryDataTypeDisparity")
IIOK(kCGImageAuxiliaryDataTypePortraitEffectsMatte, "kCGImageAuxiliaryDataTypePortraitEffectsMatte")
IIOK(kCGImageAuxiliaryDataTypeHDRGainMap, "kCGImageAuxiliaryDataTypeHDRGainMap")
IIOK(kCGImageAuxiliaryDataTypeISOGainMap, "kCGImageAuxiliaryDataTypeISOGainMap")
/* opções da fonte */
IIOK(kCGImageSourceTypeIdentifierHint, "kCGImageSourceTypeIdentifierHint")
IIOK(kCGImageSourceShouldCache, "kCGImageSourceShouldCache")
IIOK(kCGImageSourceShouldCacheImmediately, "kCGImageSourceShouldCacheImmediately")
IIOK(kCGImageSourceShouldAllowFloat, "kCGImageSourceShouldAllowFloat")
IIOK(kCGImageSourceCreateThumbnailFromImageIfAbsent, "kCGImageSourceCreateThumbnailFromImageIfAbsent")
IIOK(kCGImageSourceCreateThumbnailFromImageAlways, "kCGImageSourceCreateThumbnailFromImageAlways")
IIOK(kCGImageSourceThumbnailMaxPixelSize, "kCGImageSourceThumbnailMaxPixelSize")
IIOK(kCGImageSourceCreateThumbnailWithTransform, "kCGImageSourceCreateThumbnailWithTransform")
IIOK(kCGImageSourceSubsampleFactor, "kCGImageSourceSubsampleFactor")
IIOK(kCGImageSourceDecodeRequest, "kCGImageSourceDecodeRequest")
IIOK(kCGImageSourceDecodeToHDR, "kCGImageSourceDecodeToHDR")
IIOK(kCGImageSourceDecodeToSDR, "kCGImageSourceDecodeToSDR")
/* opções do destino */
IIOK(kCGImageDestinationLossyCompressionQuality, "kCGImageDestinationLossyCompressionQuality")
IIOK(kCGImageDestinationBackgroundColor, "kCGImageDestinationBackgroundColor")
IIOK(kCGImageDestinationImageMaxPixelSize, "kCGImageDestinationImageMaxPixelSize")
IIOK(kCGImageDestinationEmbedThumbnail, "kCGImageDestinationEmbedThumbnail")
IIOK(kCGImageDestinationOptimizeColorForSharing, "kCGImageDestinationOptimizeColorForSharing")
IIOK(kCGImageDestinationPreserveGainMap, "kCGImageDestinationPreserveGainMap")
IIOK(kCGImageDestinationMetadata, "kCGImageDestinationMetadata")
IIOK(kCGImageDestinationMergeMetadata, "kCGImageDestinationMergeMetadata")
IIOK(kCGImageDestinationDateTime, "kCGImageDestinationDateTime")
IIOK(kCGImageDestinationOrientation, "kCGImageDestinationOrientation")

/* ------------------------------------------------------------------ CF que vem de outros arquivos */
void *CFStringCreateWithCString(void *alloc, const char *s, u32 enc);
const char *cx_cstr(id o);
BOOL cx_is_str(id o);
void *CFDictionaryCreateMutable(void *alloc, long cap, const void *k, const void *v);
void CFDictionarySetValue(void *d, const void *key, const void *val);
void *CFDictionaryGetValue(void *d, const void *key);
void *CFNumberCreate(void *alloc, long type, const void *p);
u8 CFNumberGetValue(void *n, long type, void *out);
u8 CFBooleanGetValue(void *b);
u64 CFGetTypeID(void *o);
u64 CFNumberGetTypeID(void);
u64 CFBooleanGetTypeID(void);
u64 CFDictionaryGetTypeID(void);
u64 CFDataGetTypeID(void);
const u8 *CFDataGetBytePtr(void *d);
long CFDataGetLength(void *d);
void CFDataAppendBytes(void *d, const u8 *b, long n);
void *CFDataCreate(void *alloc, const u8 *b, long n);
void *CFArrayCreate(void *alloc, const void **vals, long n, const void *cb);
void CFRelease(void *o);
void *CFRetain(void *o);
u8 CFURLGetFileSystemRepresentation(void *url, u8 resolve, u8 *buf, long max);
extern void *kCFBooleanTrue, *kCFBooleanFalse;
void *CGDataProviderCreateWithData(void *info, const void *data, size_t size, void (*release)(void *, const void *, size_t));
void CGDataProviderRelease(void *p);
void *CGDataProviderCopyData(void *p);
void *CGImageCreate(size_t w, size_t h, size_t bpc, size_t bpp, size_t bpr, const void *space, u32 info,
                    void *provider, const void *decode, bool interp, int intent);
void *CGColorSpaceCreateDeviceRGB(void);
void CGColorSpaceRelease(void *s);
int cx_cgimage_src(const void *img, const u8 **px, long *bpr, int *w, int *h, int *premul);
size_t CGColorGetNumberOfComponents(const void *c);
const double *CGColorGetComponents(const void *c);
long cx_consumer_put(void *consumer, const void *bytes, size_t n);
int open(const char *path, int flags, ...);
long read(int fd, void *b, unsigned long n);
long write(int fd, const void *b, unsigned long n);
int close(int fd);
void cx_object_free(id o);
void cx_cgimage_set_uti(void *img, const void *uti);

enum { kNumSInt64 = 4, kNumF64 = 13, UTF8 = 0x08000100 };
enum { ST_COMPLETE = 0, ST_INCOMPLETE = -1, ST_HEADER = -2, ST_UNKNOWN = -3, ST_INVALID = -4, ST_EOF = -5 };

static void *num_i(long long v) { return CFNumberCreate(0, kNumSInt64, &v); }
static void *num_d(double v) { return CFNumberCreate(0, kNumF64, &v); }
static void *cfs(const char *s) { return CFStringCreateWithCString(0, s, UTF8); }
static void put(void *d, const void *k, void *v) { if (d && v) { CFDictionarySetValue(d, k, v); CFRelease(v); } }
static void put_bool(void *d, const void *k, int v) { if (d) CFDictionarySetValue(d, k, v ? kCFBooleanTrue : kCFBooleanFalse); }

static int opt_bool(void *dict, const void *key) {
    void *v = dict ? CFDictionaryGetValue(dict, key) : 0;
    if (!v) return 0;
    if (v == kCFBooleanTrue) return 1;
    if (v == kCFBooleanFalse) return 0;
    return CFBooleanGetValue(v) != 0;
}
static int opt_num(void *dict, const void *key, double *out) {
    void *v = dict ? CFDictionaryGetValue(dict, key) : 0;
    double d = 0;
    if (!v || !CFNumberGetValue(v, kNumF64, &d)) return 0;
    *out = d;
    return 1;
}
static void *opt_dict(void *dict, const void *key) { return dict ? CFDictionaryGetValue(dict, key) : 0; }
static const char *str_of(const void *s) { return s && cx_is_str((id)s) ? cx_cstr((id)s) : 0; }

/* lê um arquivo inteiro (até 1 GiB) */
static u8 *read_path(const char *path, usize *n) {
    int fd = open(path, 0);
    usize cap = 0, len = 0;
    u8 *b = 0;
    if (fd < 0) return 0;
    for (;;) {
        if (len == cap) {
            usize nc = cap ? cap * 2 : 65536;
            if (nc > (1ul << 30)) break;
            u8 *nb = realloc(b, nc);
            if (!nb) break;
            b = nb;
            cap = nc;
        }
        long r = read(fd, b + len, cap - len);
        if (r <= 0) break;
        len += (usize)r;
    }
    close(fd);
    if (!len) { free(b); return 0; }
    *n = len;
    return b;
}
static int url_path(void *url, char *buf, long max) {
    return url && CFURLGetFileSystemRepresentation(url, 1, (u8 *)buf, max);
}

/* ------------------------------------------------------------------ classes */
enum { TID_SOURCE = 0x2B0, TID_DEST = 0x2B1 };
typedef struct {
    cx_class_t *isa;
    u32 magic;
    u8 *bytes;
    usize n;
    int final;            /* 0 só em fonte incremental até UpdateData(final=true) */
    int tried;
    iio_image img;
    int ok;
    char hint[64];
} isrc_t;

typedef struct { u32 *px; int w, h, delay_ms, orientation; double dpi_x, dpi_y, quality; } dframe_t;
typedef struct {
    cx_class_t *isa;
    u32 magic;
    char uti[48];
    void *data;           /* CFMutableData (retido) */
    char *path;
    void *consumer;       /* CGDataConsumer (retido) */
    usize count;
    dframe_t *fr;
    int nfr, cap;
    iio_enc_opts opts;
    int finalized;
} idst_t;
#define SRC_MAGIC 0x49534F31u
#define DST_MAGIC 0x49445331u

static cx_class_t SRC_c, SRC_m, DST_c, DST_m;
static int iio_ready;
static void src_dealloc(id self, SEL c) {
    (void)c;
    isrc_t *s = (isrc_t *)self;
    iio_image_free(&s->img);
    free(s->bytes);
    s->magic = 0;
    cx_object_free(self);
}
static void dst_dealloc(id self, SEL c) {
    (void)c;
    idst_t *d = (idst_t *)self;
    for (int i = 0; i < d->nfr; i++) free(d->fr[i].px);
    free(d->fr);
    free(d->path);
    if (d->data) CFRelease(d->data);
    if (d->consumer) CFRelease(d->consumer);
    d->magic = 0;
    cx_object_free(self);
}
static u64 src_tid(id self, SEL c) { (void)self; (void)c; return TID_SOURCE; }
static u64 dst_tid(id self, SEL c) { (void)self; (void)c; return TID_DEST; }
static void iio_classes(void) {
    if (iio_ready) return;
    cx_class_t *ns = cx_class_named("NSObject");
    if (!ns) return;
    cx_define_class(&SRC_c, &SRC_m, "CGImageSource", ns, (u32)(sizeof(isrc_t) - sizeof(void *)));
    cx_define_class(&DST_c, &DST_m, "CGImageDestination", ns, (u32)(sizeof(idst_t) - sizeof(void *)));
    cx_add_method(&SRC_c, "dealloc", (void *)src_dealloc);
    cx_add_method(&DST_c, "dealloc", (void *)dst_dealloc);
    cx_add_method(&SRC_c, "_cfTypeID", (void *)src_tid);
    cx_add_method(&DST_c, "_cfTypeID", (void *)dst_tid);
    iio_ready = 1;
}
static isrc_t *as_src(const void *p) { const isrc_t *s = p; return s && s->magic == SRC_MAGIC ? (isrc_t *)s : 0; }
static idst_t *as_dst(const void *p) { const idst_t *d = p; return d && d->magic == DST_MAGIC ? (idst_t *)d : 0; }

u64 CGImageSourceGetTypeID(void) { return TID_SOURCE; }
u64 CGImageDestinationGetTypeID(void) { return TID_DEST; }

static void *type_array(const char *const *t) {
    const void *v[16];
    long n = 0;
    for (; t[n] && n < 16; n++) v[n] = cfs(t[n]);
    void *a = CFArrayCreate(0, v, n, 0);
    for (long i = 0; i < n; i++) CFRelease((void *)v[i]);
    return a;
}
void *CGImageSourceCopyTypeIdentifiers(void) { return type_array(iio_read_types); }
void *CGImageDestinationCopyTypeIdentifiers(void) { return type_array(iio_write_types); }

/* ------------------------------------------------------------------ fonte */
static isrc_t *src_new(u8 *bytes, usize n, int final, void *opts) {
    iio_classes();
    if (!iio_ready) { free(bytes); return 0; }
    isrc_t *s = (isrc_t *)cx_alloc_instance(&SRC_c);
    if (!s) { free(bytes); return 0; }
    s->magic = SRC_MAGIC;
    s->bytes = bytes;
    s->n = n;
    s->final = final;
    const char *h = str_of(opt_dict(opts, kCGImageSourceTypeIdentifierHint));
    if (h) { usize k = strlen(h); if (k > 63) k = 63; memcpy(s->hint, h, k); s->hint[k] = 0; }
    return s;
}
static int src_ensure(isrc_t *s) {
    if (!s) return 0;
    if (s->tried) return s->ok;
    s->tried = 1;
    s->ok = s->n && iio_decode(s->bytes, s->n, &s->img) == 0;
    return s->ok;
}
static u8 *dup_bytes(const u8 *p, usize n) {
    u8 *b = n ? malloc(n) : 0;
    if (b) memcpy(b, p, n);
    return b;
}

void *CGImageSourceCreateWithData(void *data, void *opts) {
    const u8 *p = data ? CFDataGetBytePtr(data) : 0;
    long n = data ? CFDataGetLength(data) : 0;
    if (!p || n <= 0) return 0;
    u8 *b = dup_bytes(p, (usize)n);
    return b ? src_new(b, (usize)n, 1, opts) : 0;
}
void *CGImageSourceCreateWithURL(void *url, void *opts) {
    char path[1024];
    usize n = 0;
    if (!url_path(url, path, sizeof path)) return 0;
    u8 *b = read_path(path, &n);
    return b ? src_new(b, n, 1, opts) : 0;
}
void *CGImageSourceCreateWithDataProvider(void *prov, void *opts) {
    void *d = prov ? CGDataProviderCopyData(prov) : 0;
    if (!d) return 0;
    void *s = CGImageSourceCreateWithData(d, opts);
    CFRelease(d);
    return s;
}
void *CGImageSourceCreateIncremental(void *opts) { return src_new(0, 0, 0, opts); }
void CGImageSourceUpdateData(void *src, void *data, bool final) {
    isrc_t *s = as_src(src);
    if (!s || !data) return;
    const u8 *p = CFDataGetBytePtr(data);
    long n = CFDataGetLength(data);
    u8 *b = n > 0 ? dup_bytes(p, (usize)n) : 0;
    free(s->bytes);
    s->bytes = b;
    s->n = b ? (usize)n : 0;
    s->final = final;
    iio_image_free(&s->img);
    s->tried = s->ok = 0;
}
void CGImageSourceUpdateDataProvider(void *src, void *prov, bool final) {
    void *d = prov ? CGDataProviderCopyData(prov) : 0;
    if (!d) return;
    CGImageSourceUpdateData(src, d, final);
    CFRelease(d);
}

static const char *src_uti(isrc_t *s) { return s ? iio_sniff(s->bytes, s->n) : 0; }
static void *uti_cfstr(const char *u) {
    /* constantes estáveis para CGImageSourceGetType (não é "Copy": quem chama não solta) */
    static void *cache[16];
    for (int i = 0; iio_read_types[i]; i++)
        if (!strcmp(u, iio_read_types[i])) {
            if (!cache[i]) cache[i] = cfs(u);
            return cache[i];
        }
    return 0;
}
const void *CGImageSourceGetType(void *src) {
    const char *u = src_uti(as_src(src));
    return u ? uti_cfstr(u) : 0;
}
size_t CGImageSourceGetCount(void *src) {
    isrc_t *s = as_src(src);
    return src_ensure(s) ? (size_t)s->img.nframes : 0;
}
size_t CGImageSourceGetPrimaryImageIndex(void *src) { (void)src; return 0; }
int CGImageSourceGetStatus(void *src) {
    isrc_t *s = as_src(src);
    if (!s) return ST_INVALID;
    if (!s->n) return s->final ? ST_INVALID : ST_HEADER;
    if (!src_uti(s)) return s->n < 16 && !s->final ? ST_HEADER : ST_UNKNOWN;
    if (!src_ensure(s)) return s->final ? ST_INVALID : ST_INCOMPLETE;
    /* incremental: o decodificador aceita arquivo cortado (imagem parcial); completo só com o fim do formato */
    if (!s->final && !iio_has_end(s->bytes, s->n)) return ST_INCOMPLETE;
    return ST_COMPLETE;
}
int CGImageSourceGetStatusAtIndex(void *src, size_t i) {
    isrc_t *s = as_src(src);
    int st = CGImageSourceGetStatus(src);
    if (st != ST_COMPLETE) return st;
    return i < (size_t)s->img.nframes ? ST_COMPLETE : ST_INVALID;
}
void CGImageSourceRemoveCacheAtIndex(void *src, size_t i) {
    isrc_t *s = as_src(src);
    (void)i;
    if (s && s->tried) { iio_image_free(&s->img); s->tried = s->ok = 0; }
}
/* PNG/JPEG/GIF/TIFF/BMP/WebP não trazem mapa de ganho, profundidade ou matte: NULL, como o ImageIO sem o dado */
void *CGImageSourceCopyAuxiliaryDataInfoAtIndex(void *src, size_t i, const void *type) { (void)src; (void)i; (void)type; return 0; }

static void *make_cgimage(const u32 *px, int w, int h, int alpha) {
    void *prov = CGDataProviderCreateWithData(0, px, (size_t)w * (size_t)h * 4, 0);
    void *cs = CGColorSpaceCreateDeviceRGB();
    void *img = prov ? CGImageCreate((size_t)w, (size_t)h, 8, 32, (size_t)w * 4, cs, (alpha ? 4u : 6u) | 0x2000u, prov, 0, 1, 0) : 0;
    if (prov) CGDataProviderRelease(prov);
    if (cs) CGColorSpaceRelease(cs);
    return img;
}
void *CGImageSourceCreateImageAtIndex(void *src, size_t i, void *opts) {
    isrc_t *s = as_src(src);
    if (!src_ensure(s) || i >= (size_t)s->img.nframes) return 0;
    iio_frame *f = &s->img.f[i];
    double sub = 1;
    if (opt_num(opts, kCGImageSourceSubsampleFactor, &sub) && (sub == 2 || sub == 4 || sub == 8)) {
        int nw = (f->w + (int)sub - 1) / (int)sub, nh = (f->h + (int)sub - 1) / (int)sub;
        u32 *r = iio_resize(f->px, f->w, f->h, nw, nh);
        void *img = r ? make_cgimage(r, nw, nh, f->has_alpha) : 0;
        free(r);
        return img;
    }
    void *img = make_cgimage(f->px, f->w, f->h, f->has_alpha);
    if (img) cx_cgimage_set_uti(img, uti_cfstr(s->img.uti));
    return img;
}
void *CGImageSourceCreateThumbnailAtIndex(void *src, size_t i, void *opts) {
    isrc_t *s = as_src(src);
    if (!src_ensure(s) || i >= (size_t)s->img.nframes) return 0;
    /* não há miniatura embutida que este ImageIO leia: sem pedir para criar, não há miniatura */
    if (!opt_bool(opts, kCGImageSourceCreateThumbnailFromImageAlways) && !opt_bool(opts, kCGImageSourceCreateThumbnailFromImageIfAbsent))
        return 0;
    iio_frame *f = &s->img.f[i];
    const u32 *px = f->px;
    int w = f->w, h = f->h;
    u32 *rot = 0, *small = 0;
    if (opt_bool(opts, kCGImageSourceCreateThumbnailWithTransform) && f->orientation > 1) {
        rot = iio_orient(px, w, h, f->orientation, &w, &h);
        if (rot) px = rot;
    }
    double mx = 0;
    int edge = w > h ? w : h;
    if (opt_num(opts, kCGImageSourceThumbnailMaxPixelSize, &mx) && mx >= 1 && mx < edge) {
        int nw = (int)((double)w * mx / edge + 0.5), nh = (int)((double)h * mx / edge + 0.5);
        if (nw < 1) nw = 1;
        if (nh < 1) nh = 1;
        small = iio_resize(px, w, h, nw, nh);
        if (small) { px = small; w = nw; h = nh; }
    }
    void *img = make_cgimage(px, w, h, f->has_alpha);
    free(rot);
    free(small);
    return img;
}

/* {Exif} e as entradas EXIF de {TIFF} lidas pelo iio_exif.inc */
static void exif_into(void *d, const mos_exif *x, int ifd) {
    for (int i = 0; i < x->n; i++) {
        const mos_exif_entry *e = &x->e[i];
        if (e->ifd != ifd) continue;
        const char *name = mos_exif_name(ifd, e->tag);
        if (!name) continue;
        void *k = cfs(name), *v = 0;
        if (e->kind == EXIF_STR) v = cfs(e->str);
        else if (e->kind == EXIF_BYTES) v = CFDataCreate(0, e->bytes, (long)e->nbytes);
        else if (e->nnum == 1 && e->tag != 0x8827 && e->tag != 0xA432) v = num_d(e->num[0]);
        else {
            const void *vs[4];
            for (int j = 0; j < e->nnum; j++) vs[j] = num_d(e->num[j]);
            v = CFArrayCreate(0, vs, e->nnum, 0);
            for (int j = 0; j < e->nnum; j++) CFRelease((void *)vs[j]);
        }
        if (k && v) CFDictionarySetValue(d, k, v);
        if (k) CFRelease(k);
        if (v) CFRelease(v);
    }
}
static double clamp_delay(int ms) { double s = ms / 1000.0; return s < 0.011 ? 0.1 : s; }

void *CGImageSourceCopyPropertiesAtIndex(void *src, size_t i, void *opts) {
    isrc_t *s = as_src(src);
    (void)opts;
    if (!src_ensure(s) || i >= (size_t)s->img.nframes) return 0;
    const iio_frame *f = &s->img.f[i];
    const char *u = s->img.uti;
    void *d = CFDictionaryCreateMutable(0, 0, 0, 0);
    if (!d) return 0;
    put(d, kCGImagePropertyFileSize, num_i((long long)s->n));
    put(d, kCGImagePropertyPixelWidth, num_i(f->w));
    put(d, kCGImagePropertyPixelHeight, num_i(f->h));
    put(d, kCGImagePropertyDepth, num_i(f->depth ? f->depth : 8));
    CFDictionarySetValue(d, kCGImagePropertyColorModel, f->color == IIO_GRAY ? kCGImagePropertyColorModelGray
                                                       : f->color == IIO_CMYK ? kCGImagePropertyColorModelCMYK : kCGImagePropertyColorModelRGB);
    put_bool(d, kCGImagePropertyHasAlpha, f->has_alpha);
    if (f->indexed) put_bool(d, kCGImagePropertyIsIndexed, 1);
    put(d, kCGImagePropertyOrientation, num_i(f->orientation));
    if (f->dpi_x > 0) put(d, kCGImagePropertyDPIWidth, num_d(f->dpi_x));
    if (f->dpi_y > 0) put(d, kCGImagePropertyDPIHeight, num_d(f->dpi_y));
    if (!s->img.icc) put(d, kCGImagePropertyProfileName, cfs(f->color == IIO_GRAY ? "Generic Gray Gamma 2.2 Profile" : "sRGB IEC61966-2.1"));
    else if (s->img.icc_n > 132) {
        /* nome do perfil: tag 'desc' (texto ASCII de 'desc' ou o primeiro registro de 'mluc') */
        const u8 *p = s->img.icc;
        u32 nt = be32(p + 128);
        for (u32 k = 0; k < nt && 132 + (usize)k * 12 + 12 <= s->img.icc_n; k++) {
            const u8 *t = p + 132 + k * 12;
            u32 off = be32(t + 4), len = be32(t + 8);
            if (memcmp(t, "desc", 4) || off + len > s->img.icc_n || len < 12) continue;
            char name[128];
            usize m = 0;
            if (!memcmp(p + off, "desc", 4)) {
                u32 L = be32(p + off + 8);
                for (u32 j = 0; j < L && j + 12 < len && m < sizeof name - 1 && p[off + 12 + j]; j++) name[m++] = (char)p[off + 12 + j];
            } else if (!memcmp(p + off, "mluc", 4) && len >= 28) {
                u32 sl = be32(p + off + 20), so = be32(p + off + 24);
                for (u32 j = 0; j + 1 < sl && off + so + j + 1 < s->img.icc_n && m < sizeof name - 1; j += 2) {
                    u16 c = (u16)(p[off + so + j] << 8 | p[off + so + j + 1]);
                    name[m++] = c < 128 ? (char)c : '?';
                }
            }
            name[m] = 0;
            if (m) put(d, kCGImagePropertyProfileName, cfs(name));
            break;
        }
    }
    void *tiff = CFDictionaryCreateMutable(0, 0, 0, 0);
    int ntiff = 0;
    if (!strcmp(u, "public.jpeg") || !strcmp(u, "public.tiff")) {
        mos_exif x;
        if (mos_exif_parse(s->bytes, s->n, &x) == 0) {
            void *ex = CFDictionaryCreateMutable(0, 0, 0, 0);
            exif_into(tiff, &x, EXIF_IFD_TIFF);
            exif_into(ex, &x, EXIF_IFD_EXIF);
            ntiff = 1;
            put(d, kCGImagePropertyExifDictionary, ex);
        }
    }
    if (!strcmp(u, "public.tiff")) {
        put(tiff, kCGImagePropertyTIFFCompression, num_i(f->compression));
        put(tiff, kCGImagePropertyTIFFPhotometricInterpretation, num_i(f->photometric));
        put(tiff, kCGImagePropertyTIFFOrientation, num_i(f->orientation));
        if (f->dpi_x > 0) put(tiff, kCGImagePropertyTIFFXResolution, num_d(f->dpi_x));
        if (f->dpi_y > 0) put(tiff, kCGImagePropertyTIFFYResolution, num_d(f->dpi_y));
        ntiff = 1;
    }
    if (ntiff) put(d, kCGImagePropertyTIFFDictionary, tiff);
    else CFRelease(tiff);
    if (!strcmp(u, "public.jpeg")) {
        void *j = CFDictionaryCreateMutable(0, 0, 0, 0);
        put_bool(j, kCGImagePropertyJFIFIsProgressive, s->img.interlaced);
        if (f->dpi_x > 0) {
            put(j, kCGImagePropertyJFIFXDensity, num_i((long long)(f->dpi_x + 0.5)));
            put(j, kCGImagePropertyJFIFYDensity, num_i((long long)(f->dpi_y + 0.5)));
            put(j, kCGImagePropertyJFIFDensityUnit, num_i(1));
        }
        put(d, kCGImagePropertyJFIFDictionary, j);
    } else if (!strcmp(u, "public.png")) {
        void *p = CFDictionaryCreateMutable(0, 0, 0, 0);
        put(p, kCGImagePropertyPNGInterlaceType, num_i(s->img.interlaced));
        if (s->img.gamma > 0) put(p, kCGImagePropertyPNGGamma, num_d(s->img.gamma));
        if (s->img.srgb_intent >= 0) put(p, kCGImagePropertyPNGsRGBIntent, num_i(s->img.srgb_intent));
        if (f->dpi_x > 0) {
            put(p, kCGImagePropertyPNGXPixelsPerMeter, num_i((long long)(f->dpi_x / 0.0254 + 0.5)));
            put(p, kCGImagePropertyPNGYPixelsPerMeter, num_i((long long)(f->dpi_y / 0.0254 + 0.5)));
        }
        if (s->img.nframes > 1) {
            put(p, kCGImagePropertyAPNGDelayTime, num_d(clamp_delay(f->delay_ms)));
            put(p, kCGImagePropertyAPNGUnclampedDelayTime, num_d(f->delay_ms / 1000.0));
        }
        put(d, kCGImagePropertyPNGDictionary, p);
    } else if (!strcmp(u, "org.webmproject.webp")) {
        void *w = CFDictionaryCreateMutable(0, 0, 0, 0);
        if (s->img.nframes > 1) {
            put(w, kCGImagePropertyWebPDelayTime, num_d(clamp_delay(f->delay_ms)));
            put(w, kCGImagePropertyWebPUnclampedDelayTime, num_d(f->delay_ms / 1000.0));
        }
        put(d, kCGImagePropertyWebPDictionary, w);
    } else if (!strcmp(u, "com.compuserve.gif")) {
        void *g = CFDictionaryCreateMutable(0, 0, 0, 0);
        put(g, kCGImagePropertyGIFDelayTime, num_d(clamp_delay(f->delay_ms)));
        put(g, kCGImagePropertyGIFUnclampedDelayTime, num_d(f->delay_ms / 1000.0));
        put_bool(g, kCGImagePropertyGIFImageColorMap, 1);
        put(d, kCGImagePropertyGIFDictionary, g);
    }
    return d;
}

/* propriedades do arquivo (não de uma imagem): tamanho, quadros e repetições da animação */
void *CGImageSourceCopyProperties(void *src, void *opts) {
    isrc_t *s = as_src(src);
    (void)opts;
    if (!s) return 0;
    void *d = CFDictionaryCreateMutable(0, 0, 0, 0);
    put(d, kCGImagePropertyFileSize, num_i((long long)s->n));
    if (!src_ensure(s)) return d;
    put(d, kCGImagePropertyImageCount, num_i(s->img.nframes));
    put(d, kCGImagePropertyPrimaryImage, num_i(0));
    const char *u = s->img.uti;
    if (!strcmp(u, "com.compuserve.gif")) {
        void *g = CFDictionaryCreateMutable(0, 0, 0, 0);
        put_bool(g, kCGImagePropertyGIFHasGlobalColorMap, s->img.has_global_map);
        if (s->img.loop_count >= 0) put(g, kCGImagePropertyGIFLoopCount, num_i(s->img.loop_count));
        put(g, kCGImagePropertyGIFCanvasPixelWidth, num_i(s->img.w));
        put(g, kCGImagePropertyGIFCanvasPixelHeight, num_i(s->img.h));
        put(d, kCGImagePropertyGIFDictionary, g);
    } else if (!strcmp(u, "org.webmproject.webp")) {
        void *w = CFDictionaryCreateMutable(0, 0, 0, 0);
        put(w, kCGImagePropertyWebPLoopCount, num_i(s->img.loop_count < 0 ? 0 : s->img.loop_count));
        put(w, kCGImagePropertyWebPCanvasPixelWidth, num_i(s->img.w));
        put(w, kCGImagePropertyWebPCanvasPixelHeight, num_i(s->img.h));
        put(d, kCGImagePropertyWebPDictionary, w);
    } else if (!strcmp(u, "public.png") && s->img.nframes > 1) {
        void *p = CFDictionaryCreateMutable(0, 0, 0, 0);
        put(p, kCGImagePropertyAPNGLoopCount, num_i(s->img.loop_count < 0 ? 0 : s->img.loop_count));
        put(p, kCGImagePropertyAPNGCanvasPixelWidth, num_i(s->img.w));
        put(p, kCGImagePropertyAPNGCanvasPixelHeight, num_i(s->img.h));
        put(d, kCGImagePropertyPNGDictionary, p);
    }
    return d;
}

/* ------------------------------------------------------------------ destino */
static int uti_norm(const char *t, char *out) {
    if (!t) return 0;
    static const struct { const char *alias, *uti; } al[] = {
        {"image/png", "public.png"}, {"image/jpeg", "public.jpeg"}, {"public.jpg", "public.jpeg"}, {"image/gif", "com.compuserve.gif"},
        {"image/tiff", "public.tiff"}, {"image/bmp", "com.microsoft.bmp"}, {"com.microsoft.windows-bitmap", "com.microsoft.bmp"}};
    for (usize i = 0; i < sizeof al / sizeof al[0]; i++) if (!strcmp(t, al[i].alias)) t = al[i].uti;
    if (!iio_can_encode(t, 0)) return 0;
    usize k = strlen(t);
    if (k > 47) return 0;
    memcpy(out, t, k + 1);
    return 1;
}
static void dst_props(idst_t *d, void *props, dframe_t *f);
static idst_t *dst_new(const void *type, size_t count, void *opts) {
    char u[48];
    iio_classes();
    if (!iio_ready || !count || !uti_norm(str_of(type), u)) return 0;
    idst_t *d = (idst_t *)cx_alloc_instance(&DST_c);
    if (!d) return 0;
    d->magic = DST_MAGIC;
    memcpy(d->uti, u, sizeof u);
    d->count = count;
    d->opts = (iio_enc_opts){-1, 1, 0, 0, 0, 0xFFFFFFFFu, 1};
    dst_props(d, opts, 0);
    return d;
}
void *CGImageDestinationCreateWithData(void *data, const void *type, size_t count, void *opts) {
    if (!data) return 0;
    idst_t *d = dst_new(type, count, opts);
    if (d) d->data = CFRetain(data);
    return d;
}
void *CGImageDestinationCreateWithURL(void *url, const void *type, size_t count, void *opts) {
    char path[1024];
    if (!url_path(url, path, sizeof path)) return 0;
    idst_t *d = dst_new(type, count, opts);
    if (!d) return 0;
    usize k = strlen(path);
    d->path = malloc(k + 1);
    if (!d->path) { CFRelease(d); return 0; }
    memcpy(d->path, path, k + 1);
    return d;
}
void *CGImageDestinationCreateWithDataConsumer(void *consumer, const void *type, size_t count, void *opts) {
    if (!consumer) return 0;
    idst_t *d = dst_new(type, count, opts);
    if (d) d->consumer = CFRetain(consumer);
    return d;
}

/* opções do destino inteiro (f = 0) ou de uma imagem */
static void dst_props(idst_t *d, void *props, dframe_t *f) {
    double v;
    if (!props) return;
    if (opt_num(props, kCGImageDestinationLossyCompressionQuality, &v)) { if (f) f->quality = v; else d->opts.quality = v; }
    if (opt_num(props, kCGImagePropertyOrientation, &v) && v >= 1 && v <= 8) { if (f) f->orientation = (int)v; else d->opts.orientation = (int)v; }
    if (opt_num(props, kCGImageDestinationOrientation, &v) && v >= 1 && v <= 8) { if (f) f->orientation = (int)v; else d->opts.orientation = (int)v; }
    if (opt_num(props, kCGImagePropertyDPIWidth, &v) && v > 0) { if (f) f->dpi_x = v; else d->opts.dpi_x = v; }
    if (opt_num(props, kCGImagePropertyDPIHeight, &v) && v > 0) { if (f) f->dpi_y = v; else d->opts.dpi_y = v; }
    void *bg = opt_dict(props, kCGImageDestinationBackgroundColor);
    if (bg && CGColorGetNumberOfComponents(bg) >= 3) {
        const double *c = CGColorGetComponents(bg);
        u32 r = (u32)(c[0] * 255 + 0.5), g = (u32)(c[1] * 255 + 0.5), b = (u32)(c[2] * 255 + 0.5);
        d->opts.background = 0xFF000000u | (r > 255 ? 255 : r) << 16 | (g > 255 ? 255 : g) << 8 | (b > 255 ? 255 : b);
    }
    const void *anim[3] = {kCGImagePropertyGIFDictionary, kCGImagePropertyPNGDictionary, kCGImagePropertyTIFFDictionary};
    for (int i = 0; i < 3; i++) {
        void *sub = opt_dict(props, anim[i]);
        if (!sub || CFGetTypeID(sub) != CFDictionaryGetTypeID()) continue;
        if (i < 2) {
            if (opt_num(sub, kCGImagePropertyGIFLoopCount, &v) && v >= 0) d->opts.loop_count = (int)v;
            if (f && (opt_num(sub, kCGImagePropertyGIFUnclampedDelayTime, &v) || opt_num(sub, kCGImagePropertyGIFDelayTime, &v)) && v >= 0)
                f->delay_ms = (int)(v * 1000 + 0.5);
        } else {
            if (opt_num(sub, kCGImagePropertyTIFFCompression, &v)) d->opts.tiff_compression = (int)v;
            if (opt_num(sub, kCGImagePropertyTIFFOrientation, &v) && v >= 1 && v <= 8) { if (f) f->orientation = (int)v; else d->opts.orientation = (int)v; }
        }
    }
    if (f && opt_num(props, kCGImageDestinationImageMaxPixelSize, &v) && v >= 1) {
        int edge = f->w > f->h ? f->w : f->h;
        if (v < edge) {
            int nw = (int)((double)f->w * v / edge + 0.5), nh = (int)((double)f->h * v / edge + 0.5);
            if (nw < 1) nw = 1;
            if (nh < 1) nh = 1;
            u32 *r = iio_resize(f->px, f->w, f->h, nw, nh);
            if (r) { free(f->px); f->px = r; f->w = nw; f->h = nh; }
        }
    }
}
void CGImageDestinationSetProperties(void *dst, void *props) {
    idst_t *d = as_dst(dst);
    if (d) dst_props(d, props, 0);
}

static int dst_push(idst_t *d, u32 *px, int w, int h, void *props) {
    if (d->nfr == d->cap) {
        int nc = d->cap ? d->cap * 2 : 4;
        dframe_t *nf = realloc(d->fr, (usize)nc * sizeof *nf);
        if (!nf) { free(px); return 0; }
        d->fr = nf;
        d->cap = nc;
    }
    dframe_t *f = &d->fr[d->nfr];
    *f = (dframe_t){px, w, h, 100, 0, 0, 0, -2};
    dst_props(d, props, f);
    d->nfr++;
    return 1;
}
/* pixels ARGB retos de um CGImage */
static u32 *cg_pixels(void *img, int *w, int *h) {
    const u8 *src;
    long bpr;
    int pm;
    if (!cx_cgimage_src(img, &src, &bpr, w, h, &pm) || *w < 1 || *h < 1) return 0;
    u32 *px = malloc((usize)*w * *h * 4);
    if (!px) return 0;
    for (int y = 0; y < *h; y++) {
        const u32 *r = (const u32 *)(src + (usize)y * bpr);
        for (int x = 0; x < *w; x++) {
            u32 c = r[x], a = c >> 24;
            if (pm && a && a < 255)
                c = a << 24 | (((c >> 16 & 255) * 255 + a / 2) / a > 255 ? 255 : ((c >> 16 & 255) * 255 + a / 2) / a) << 16 |
                    (((c >> 8 & 255) * 255 + a / 2) / a > 255 ? 255 : ((c >> 8 & 255) * 255 + a / 2) / a) << 8 |
                    (((c & 255) * 255 + a / 2) / a > 255 ? 255 : ((c & 255) * 255 + a / 2) / a);
            px[(usize)y * *w + x] = c;
        }
    }
    return px;
}
void CGImageDestinationAddImage(void *dst, void *img, void *props) {
    idst_t *d = as_dst(dst);
    int w, h;
    if (!d || d->finalized || !img) return;
    u32 *px = cg_pixels(img, &w, &h);
    if (px) dst_push(d, px, w, h, props);
}
void CGImageDestinationAddImageFromSource(void *dst, void *src, size_t i, void *props) {
    idst_t *d = as_dst(dst);
    isrc_t *s = as_src(src);
    if (!d || d->finalized || !src_ensure(s) || i >= (size_t)s->img.nframes) return;
    iio_frame *f = &s->img.f[i];
    u32 *px = malloc((usize)f->w * f->h * 4);
    if (!px) return;
    memcpy(px, f->px, (usize)f->w * f->h * 4);
    if (dst_push(d, px, f->w, f->h, 0)) {
        dframe_t *df = &d->fr[d->nfr - 1];
        /* a imagem copiada leva o atraso, a orientação e a densidade da fonte; props do chamador ganham */
        df->delay_ms = f->delay_ms;
        df->orientation = f->orientation;
        df->dpi_x = f->dpi_x;
        df->dpi_y = f->dpi_y;
        dst_props(d, props, df);
    }
}

bool CGImageDestinationFinalize(void *dst) {
    idst_t *d = as_dst(dst);
    if (!d || d->finalized || !d->nfr) return 0;
    d->finalized = 1;
    int multi = 0;
    iio_can_encode(d->uti, &multi);
    int n = multi ? d->nfr : 1;
    iio_enc_frame *fr = malloc((usize)n * sizeof *fr);
    if (!fr) return 0;
    for (int i = 0; i < n; i++) fr[i] = (iio_enc_frame){d->fr[i].px, d->fr[i].w, d->fr[i].h, d->fr[i].delay_ms, d->fr[i].orientation};
    iio_enc_opts o = d->opts;
    const dframe_t *f0 = &d->fr[0];
    if (f0->quality >= 0) o.quality = f0->quality;
    if (f0->orientation) o.orientation = f0->orientation;
    if (f0->dpi_x > 0) o.dpi_x = f0->dpi_x;
    if (f0->dpi_y > 0) o.dpi_y = f0->dpi_y;
    u8 *out = 0;
    usize on = 0;
    int rc = iio_encode(d->uti, fr, n, &o, &out, &on);
    free(fr);
    if (rc) return 0;
    int ok = 1;
    if (d->data) CFDataAppendBytes(d->data, out, (long)on);
    else if (d->consumer) ok = cx_consumer_put(d->consumer, out, on) == (long)on;
    else if (d->path) {
        int fd = open(d->path, 577, 0644);          /* O_WRONLY | O_CREAT | O_TRUNC, como o resto do libSystem */
        ok = fd >= 0;
        for (usize k = 0; ok && k < on;) {
            long w = write(fd, out + k, on - k);
            if (w <= 0) ok = 0;
            else k += (usize)w;
        }
        if (fd >= 0) close(fd);
    }
    free(out);
    return ok;
}
