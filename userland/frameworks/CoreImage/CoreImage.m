/*
 * mOS — CoreImage.framework (Mach-O, /System/Library/Frameworks).
 *
 * Núcleo em CPU (coreimage_cpu.c): CIImage a partir de CGImage, de bytes PNG/JPEG
 * (ImageIO, mos_img_decode) ou de buffer, extent, corte, afim, CIGaussianBlur,
 * CIColorControls, CISourceOverCompositing e CILanczosScaleTransform.
 * CIContext createCGImage:fromRect: e render:toBitmap:... escrevem esses pixels.
 *
 * O que esta versão não faz: CIKernel, Metal, CIDetector, formatos half/float,
 * GIF/HEIF/WebP (o ImageIO do mOS só decodifica PNG e JPEG) e o espaço de cor
 * além de 8 bits reto. kCIImageColorSpace com valor não nulo recusa a imagem:
 * não há conversão de espaço. kCIContextCacheIntermediates = NO recalcula o raster.
 * tools/build.sh grava o Mach-O em /System/Library/Frameworks/CoreImage.framework
 * (tools/build-coreimage.sh). O loader prefere esse arquivo; sem ele, a fachada
 * vazia do libSystem (vscode_fwclasses.c) continua.
 *
 * Símbolos medidos no repositório: não há binário do claude-ios-sim, do WhatsApp,
 * do ChatGPT nem do Grok Bot aqui. docs/compat-open-usage.md conta 7 símbolos de
 * CoreImage no OpenBot, sem listar os nomes. O conjunto abaixo é o que o pedido
 * descreveu e o que esses filtros exigem.
 * tools/build-frameworks-objc.ps1 -Only CoreImage
 */
#import <Foundation/Foundation.h>
#include "coreimage_cpu.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef struct { double a, b, c, d, tx, ty; } CGAffineTransform;
static CGAffineTransform CGAffineTransformMake(double a, double b, double c, double d, double tx, double ty) {
    CGAffineTransform t = {a, b, c, d, tx, ty};
    return t;
}

int kCIFormatARGB8 = MOS_CI_ARGB8;
int kCIFormatBGRA8 = MOS_CI_BGRA8;
int kCIFormatRGBA8 = MOS_CI_RGBA8;
int kCIFormatABGR8 = MOS_CI_ABGR8;
int kCIFormatRGBAh = MOS_CI_RGBAh;

NSString *const kCIInputImageKey = @"inputImage";
NSString *const kCIInputBackgroundImageKey = @"inputBackgroundImage";
NSString *const kCIInputRadiusKey = @"inputRadius";
NSString *const kCIInputSaturationKey = @"inputSaturation";
NSString *const kCIInputBrightnessKey = @"inputBrightness";
NSString *const kCIInputContrastKey = @"inputContrast";
NSString *const kCIInputScaleKey = @"inputScale";
NSString *const kCIInputAspectRatioKey = @"inputAspectRatio";
NSString *const kCIInputTransformKey = @"inputTransform";
NSString *const kCIOutputImageKey = @"outputImage";
NSString *const kCIContextCacheIntermediates = @"kCIContextCacheIntermediates";
NSString *const kCIImageColorSpace = @"kCIImageColorSpace";
NSString *const kCIInputExtentKey = @"inputExtent";
NSString *const kCIInputMaskImageKey = @"inputMaskImage";
NSString *const kCIInputIntensityKey = @"inputIntensity";
NSString *const kCIInputColorKey = @"inputColor";
NSString *const kCIInputEVKey = @"inputEV";
NSString *const kCIImageProperties = @"properties";
NSString *const kCIImageApplyOrientationProperty = @"kCIImageApplyOrientationProperty";
NSString *const kCIImageToneMapHDRtoSDR = @"kCIImageToneMapHDRtoSDR";
/* opções do CIContext: o renderizador do mOS é um só (CPU, 8 bits); estas chaves são aceitas e guardadas */
NSString *const kCIContextWorkingFormat = @"working_format";
NSString *const kCIContextWorkingColorSpace = @"working_color_space";
NSString *const kCIContextOutputColorSpace = @"output_color_space";
NSString *const kCIContextOutputPremultiplied = @"output_premultiplied";
NSString *const kCIContextUseSoftwareRenderer = @"software_renderer";
NSString *const kCIContextName = @"name";

extern int cx_cgimage_src(const void *img, const uint8_t **px, long *bpr, int *w, int *h, int *premul);
extern void *CGDataProviderCreateWithData(void *info, const void *data, size_t size, void (*release)(void *, const void *, size_t));
extern void CGDataProviderRelease(void *p);
extern void *CGImageCreate(size_t w, size_t h, size_t bpc, size_t bpp, size_t bpr, const void *space, uint32_t info,
                           void *provider, const void *decode, bool shouldInterpolate, int intent);
extern unsigned char CFURLGetFileSystemRepresentation(void *url, unsigned char resolve, uint8_t *buf, long max);

@interface NSURL : NSObject
- (NSString *)path;
@end

@interface NSValue : NSObject
- (void)getValue:(void *)value;
@end

static uint32_t unpremul(uint32_t p) {
    unsigned a = p >> 24;
    if (a == 0 || a == 255) return p;
    unsigned r = ((p >> 16) & 255) * 255u / a;
    unsigned g = ((p >> 8) & 255) * 255u / a;
    unsigned b = (p & 255) * 255u / a;
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return (a << 24) | (r << 16) | (g << 8) | b;
}

@interface CIVector : NSObject {
    double v[4];
    int n;
}
+ (CIVector *)vectorWithX:(double)x Y:(double)y;
+ (CIVector *)vectorWithX:(double)x Y:(double)y Z:(double)z W:(double)w;
- (double)X;
- (double)Y;
- (double)Z;
- (double)W;
- (double)valueAtIndex:(int)i;
@end

@implementation CIVector
+ (CIVector *)vectorWithX:(double)x Y:(double)y {
    CIVector *o = [[CIVector alloc] init];
    o->v[0] = x; o->v[1] = y; o->n = 2;
    return o;
}
+ (CIVector *)vectorWithX:(double)x Y:(double)y Z:(double)z W:(double)w {
    CIVector *o = [[CIVector alloc] init];
    o->v[0] = x; o->v[1] = y; o->v[2] = z; o->v[3] = w; o->n = 4;
    return o;
}
- (double)X { return v[0]; }
- (double)Y { return v[1]; }
- (double)Z { return v[2]; }
- (double)W { return v[3]; }
- (double)valueAtIndex:(int)i { return (i >= 0 && i < 4) ? v[i] : 0; }
@end

@interface CIColor : NSObject {
    double r, g, b, a;
}
+ (CIColor *)colorWithRed:(double)r green:(double)g blue:(double)b alpha:(double)a;
- (double)red;
- (double)green;
- (double)blue;
- (double)alpha;
@end

@implementation CIColor
+ (CIColor *)colorWithRed:(double)rr green:(double)gg blue:(double)bb alpha:(double)aa {
    CIColor *o = [[CIColor alloc] init];
    o->r = rr; o->g = gg; o->b = bb; o->a = aa;
    return o;
}
- (double)red { return r; }
- (double)green { return g; }
- (double)blue { return b; }
- (double)alpha { return a; }
@end

@interface CIImage : NSObject {
   @public
    mos_ci *img;
}
+ (CIImage *)imageWithBitmapData:(NSData *)data bytesPerRow:(long)bpr size:(CGSize)size format:(int)format colorSpace:(id)cs;
- (CIImage *)imageByCroppingToRect:(CGRect)r;
- (CIImage *)imageByApplyingTransform:(CGAffineTransform)t;
- (CGRect)extent;
@end

static CIImage *wrap(mos_ci *m) {
    if (!m) return nil;
    CIImage *o = [[CIImage alloc] init];
    if (!o) { mos_ci_release(m); return nil; }
    o->img = m;
    return o;
}

static CIImage *from_px(const uint32_t *px, int w, int h) {
    return wrap(mos_ci_bitmap(px, w, h, 0, 0));
}

@implementation CIImage
- (void)dealloc {
    mos_ci_release(img);
    [super dealloc];
}
- (CGRect)extent {
    double x = 0, y = 0, w = 0, h = 0;
    if (!mos_ci_extent(img, &x, &y, &w, &h)) return CGRectMake(0, 0, 0, 0);
    return CGRectMake(x, y, w, h);
}
+ (CIImage *)imageWithCGImage:(id)cg {
    return [self imageWithCGImage:cg options:nil];
}
+ (CIImage *)imageWithCGImage:(id)cg options:(NSDictionary *)options {
    id space = options ? [options objectForKey:kCIImageColorSpace] : nil;
    if (!mos_ci_colorspace_ok(space != nil)) return nil;
    const uint8_t *bytes = 0;
    long bpr = 0;
    int w = 0, h = 0, prem = 0;
    if (!cx_cgimage_src(cg, &bytes, &bpr, &w, &h, &prem) || !bytes) return nil;
    uint32_t *px = malloc((size_t)w * (size_t)h * 4u);
    if (!px) return nil;
    for (int y = 0; y < h; y++) {
        const uint32_t *row = (const uint32_t *)(bytes + y * bpr);
        for (int x = 0; x < w; x++) px[(size_t)y * (size_t)w + (size_t)x] = prem ? unpremul(row[x]) : row[x];
    }
    CIImage *o = from_px(px, w, h);
    free(px);
    return o;
}
+ (CIImage *)imageWithData:(NSData *)data {
    return [self imageWithData:data options:nil];
}
+ (CIImage *)imageWithData:(NSData *)data options:(NSDictionary *)options {
    (void)options;
    if (!data || !data.bytes || !data.length) return nil;
    return wrap(mos_ci_from_encoded(data.bytes, data.length));
}
+ (CIImage *)imageWithContentsOfURL:(id)url {
    return [self imageWithContentsOfURL:url options:nil];
}
+ (CIImage *)imageWithContentsOfURL:(id)url options:(NSDictionary *)options {
    (void)options;
    char path[1024];
    path[0] = 0;
    if ([url isKindOfClass:[NSURL class]]) {
        NSString *p = [(NSURL *)url path];
        if (p.UTF8String) snprintf(path, sizeof path, "%s", p.UTF8String);
    } else if (!CFURLGetFileSystemRepresentation(url, 1, (uint8_t *)path, (long)sizeof path)) {
        return nil;
    }
    if (!path[0]) return nil;
    FILE *f = fopen(path, "rb");
    if (!f) return nil;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return nil; }
    long sz = ftell(f);
    if (sz <= 0 || sz > 64L * 1024L * 1024L) { fclose(f); return nil; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return nil; }
    uint8_t *b = malloc((size_t)sz);
    if (!b || fread(b, 1, (size_t)sz, f) != (size_t)sz) { free(b); fclose(f); return nil; }
    fclose(f);
    CIImage *o = wrap(mos_ci_from_encoded(b, (size_t)sz));
    free(b);
    return o;
}
+ (CIImage *)imageWithBitmapData:(NSData *)data bytesPerRow:(long)bpr size:(CGSize)size format:(int)format colorSpace:(id)cs {
    (void)cs;
    int w = (int)llround(size.width), h = (int)llround(size.height);
    if (!data || !data.bytes || w <= 0 || h <= 0 || bpr < w * 4) return nil;
    if ((long)data.length < bpr * h) return nil;
    uint32_t *px = malloc((size_t)w * (size_t)h * 4u);
    if (!px) return nil;
    const uint8_t *src = data.bytes;
    for (int y = 0; y < h; y++) {
        const uint8_t *row = src + y * bpr;
        for (int x = 0; x < w; x++) {
            const uint8_t *p = row + x * 4;
            uint8_t A, R, G, B;
            if (format == kCIFormatBGRA8) { B = p[0]; G = p[1]; R = p[2]; A = p[3]; }
            else if (format == kCIFormatRGBA8) { R = p[0]; G = p[1]; B = p[2]; A = p[3]; }
            else if (format == kCIFormatABGR8) { A = p[0]; B = p[1]; G = p[2]; R = p[3]; }
            else { A = p[0]; R = p[1]; G = p[2]; B = p[3]; }
            px[(size_t)y * (size_t)w + (size_t)x] = ((uint32_t)A << 24) | ((uint32_t)R << 16) | ((uint32_t)G << 8) | B;
        }
    }
    CIImage *o = from_px(px, w, h);
    free(px);
    return o;
}
- (CIImage *)imageByCroppingToRect:(CGRect)r {
    return wrap(mos_ci_crop(img, r.origin.x, r.origin.y, r.size.width, r.size.height));
}
- (CIImage *)imageByApplyingTransform:(CGAffineTransform)t {
    return wrap(mos_ci_affine(img, t.a, t.b, t.c, t.d, t.tx, t.ty));
}
- (CIImage *)imageByApplyingCGOrientation:(int)orientation {
    if (orientation == 1 || orientation == 0) return wrap(mos_ci_affine(img, 1, 0, 0, 1, 0, 0));
    return nil;
}
- (CIImage *)imageByCompositingOverImage:(CIImage *)bg {
    if (!bg) return nil;
    return wrap(mos_ci_over(img, bg->img));
}
@end

@interface CIFilter : NSObject {
    NSString *name;
    CIImage *inputImage;
    CIImage *inputBackgroundImage;
    double radius, saturation, brightness, contrast, scale, aspect;
    CGAffineTransform transform;
    int hasTransform;
}
+ (CIFilter *)filterWithName:(NSString *)n;
- (void)setDefaults;
- (void)setValue:(id)v forKey:(NSString *)key;
- (id)valueForKey:(NSString *)key;
- (CIImage *)outputImage;
@end

static double num_of(id v, double fallback) {
    if (!v) return fallback;
    if ([v isKindOfClass:[CIVector class]]) return [(CIVector *)v X];
    if ([v isKindOfClass:[NSNumber class]]) return [(NSNumber *)v doubleValue];
    return fallback;
}

int mos_coreimage_real(void) { return 0xC1; }

@implementation CIFilter
+ (CIFilter *)filterWithName:(NSString *)n {
    if (![n isEqualToString:@"CIGaussianBlur"] && ![n isEqualToString:@"CIColorControls"] &&
        ![n isEqualToString:@"CISourceOverCompositing"] && ![n isEqualToString:@"CILanczosScaleTransform"] &&
        ![n isEqualToString:@"CIAffineTransform"] && ![n isEqualToString:@"CICrop"])
        return nil;
    CIFilter *f = [[CIFilter alloc] init];
    f->name = [n retain];
    [f setDefaults];
    return f;
}
+ (CIFilter *)filterWithName:(NSString *)n keysAndValues:(id)first, ... {
    CIFilter *f = [self filterWithName:n];
    if (!f) return nil;
    va_list ap;
    va_start(ap, first);
    for (id key = first; key; key = va_arg(ap, id)) {
        id val = va_arg(ap, id);
        [f setValue:val forKey:key];
    }
    va_end(ap);
    return f;
}
- (void)dealloc {
    [name release];
    [inputImage release];
    [inputBackgroundImage release];
    [super dealloc];
}
- (void)setDefaults {
    radius = 10;
    saturation = 1;
    brightness = 0;
    contrast = 1;
    scale = 1;
    aspect = 1;
    transform = CGAffineTransformMake(1, 0, 0, 1, 0, 0);
    hasTransform = 0;
}
- (void)setValue:(id)v forKey:(NSString *)key {
    if ([key isEqualToString:kCIInputImageKey]) {
        [inputImage release];
        inputImage = [v retain];
    } else if ([key isEqualToString:kCIInputBackgroundImageKey]) {
        [inputBackgroundImage release];
        inputBackgroundImage = [v retain];
    } else if ([key isEqualToString:kCIInputRadiusKey]) radius = num_of(v, radius);
    else if ([key isEqualToString:kCIInputSaturationKey]) saturation = num_of(v, saturation);
    else if ([key isEqualToString:kCIInputBrightnessKey]) brightness = num_of(v, brightness);
    else if ([key isEqualToString:kCIInputContrastKey]) contrast = num_of(v, contrast);
    else if ([key isEqualToString:kCIInputScaleKey]) scale = num_of(v, scale);
    else if ([key isEqualToString:kCIInputAspectRatioKey]) aspect = num_of(v, aspect);
    else if ([key isEqualToString:kCIInputTransformKey] && [v isKindOfClass:[NSValue class]]) {
        [(NSValue *)v getValue:&transform];
        hasTransform = 1;
    } else if ([key isEqualToString:@"inputRectangle"] && [v isKindOfClass:[CIVector class]]) {
        CIVector *vec = v;
        transform.tx = [vec valueAtIndex:0];
        transform.ty = [vec valueAtIndex:1];
        transform.a = [vec valueAtIndex:2];
        transform.b = [vec valueAtIndex:3];
        hasTransform = 2;
    }
}
- (id)valueForKey:(NSString *)key {
    if ([key isEqualToString:kCIOutputImageKey] || [key isEqualToString:@"outputImage"]) return [self outputImage];
    if ([key isEqualToString:kCIInputImageKey]) return inputImage;
    if ([key isEqualToString:kCIInputBackgroundImageKey]) return inputBackgroundImage;
    if ([key isEqualToString:kCIInputRadiusKey]) return [NSNumber numberWithDouble:radius];
    if ([key isEqualToString:kCIInputScaleKey]) return [NSNumber numberWithDouble:scale];
    return nil;
}
- (CIImage *)outputImage {
    if (!inputImage || !inputImage->img) return nil;
    return wrap(mos_ci_apply(name.UTF8String, inputImage->img,
                             inputBackgroundImage ? inputBackgroundImage->img : 0,
                             radius, saturation, brightness, contrast, scale, aspect,
                             transform.a, transform.b, transform.c, transform.d, transform.tx, transform.ty,
                             hasTransform == 2));
}
@end

@interface CIContext : NSObject {
    mos_ci_ctx ctx;
    int ready;
}
+ (CIContext *)contextWithOptions:(NSDictionary *)options;
+ (CIContext *)context;
- (id)createCGImage:(CIImage *)im fromRect:(CGRect)r;
- (void)render:(CIImage *)im toBitmap:(void *)data rowBytes:(long)rb bounds:(CGRect)bounds format:(int)format colorSpace:(id)cs;
@end

/* binary16 de um valor em [0, 1] (o que o CIContext escreve): sem NaN nem infinito */
static uint16_t half_of(double v) {
    if (v <= 0) return 0;
    if (v >= 65504) return 0x7BFF;
    int e = 0;
    double m = frexp(v, &e);                    /* v = m * 2^e, m em [0,5, 1) */
    int he = e - 1 + 15;
    if (he <= 0) return (uint16_t)lround(v * 16777216.0);   /* subnormal: v / 2^-24 */
    unsigned frac = (unsigned)lround((m * 2 - 1) * 1024);
    if (frac == 1024) { frac = 0; he++; }
    return (uint16_t)(he << 10 | frac);
}

static void free_bytes(void *info, const void *data, size_t size) {
    (void)info; (void)size;
    free((void *)data);
}

@implementation CIContext
- (void)dealloc {
    if (ready) mos_ci_ctx_free(&ctx);
    [super dealloc];
}
+ (CIContext *)contextWithOptions:(NSDictionary *)options {
    CIContext *o = [[CIContext alloc] init];
    if (!o) return nil;
    id flag = options ? [options objectForKey:kCIContextCacheIntermediates] : nil;
    int cache = flag ? [flag boolValue] : 1;
    mos_ci_ctx_init(&o->ctx, cache);
    o->ready = 1;
    return o;
}
+ (CIContext *)context { return [self contextWithOptions:nil]; }
- (id)createCGImage:(CIImage *)im fromRect:(CGRect)r {
    if (!im || !im->img) return nil;
    int w = (int)llround(r.size.width);
    int h = (int)llround(r.size.height);
    if (w <= 0 || h <= 0) return nil;
    uint32_t *px = malloc((size_t)w * (size_t)h * 4u);
    if (!px) return nil;
    if (!ready) mos_ci_ctx_init(&ctx, 1), ready = 1;
    if (mos_ci_ctx_render(&ctx, im->img, r.origin.x, r.origin.y, w, h, px) != 0) { free(px); return nil; }
    void *prov = CGDataProviderCreateWithData(0, px, (size_t)w * (size_t)h * 4u, free_bytes);
    if (!prov) { free(px); return nil; }
    void *cg = CGImageCreate((size_t)w, (size_t)h, 8, 32, (size_t)w * 4u, 0, 4, prov, 0, 0, 0);
    CGDataProviderRelease(prov);
    return cg;
}
- (void)render:(CIImage *)im toBitmap:(void *)data rowBytes:(long)rb bounds:(CGRect)bounds format:(int)format colorSpace:(id)cs {
    (void)cs;
    if (!im || !im->img || !data) return;
    int w = (int)llround(bounds.size.width);
    int h = (int)llround(bounds.size.height);
    if (w <= 0 || h <= 0) return;
    if (!ready) mos_ci_ctx_init(&ctx, 1), ready = 1;
    uint32_t *tmp = malloc((size_t)w * (size_t)h * 4u);
    if (!tmp) return;
    if (mos_ci_ctx_render(&ctx, im->img, bounds.origin.x, bounds.origin.y, w, h, tmp) != 0) { free(tmp); return; }
    for (int row = 0; row < h; row++) {
        uint8_t *d = (uint8_t *)data + (long)row * rb;
        for (int col = 0; col < w; col++) {
            uint32_t p = tmp[(size_t)row * (size_t)w + (size_t)col];
            uint8_t A = (uint8_t)(p >> 24), R = (uint8_t)(p >> 16), G = (uint8_t)(p >> 8), B = (uint8_t)p;
            uint8_t *o = d + (size_t)col * 4u;
            if (format == MOS_CI_RGBAh) {
                uint16_t *hf = (uint16_t *)((uint8_t *)data + (long)row * rb) + (size_t)col * 4u;
                hf[0] = half_of(R * A / 255.0 / 255.0); hf[1] = half_of(G * A / 255.0 / 255.0);
                hf[2] = half_of(B * A / 255.0 / 255.0); hf[3] = half_of(A / 255.0);
                continue;
            }
            if (format == MOS_CI_BGRA8) { o[0] = B; o[1] = G; o[2] = R; o[3] = A; }
            else if (format == MOS_CI_RGBA8) { o[0] = R; o[1] = G; o[2] = B; o[3] = A; }
            else if (format == MOS_CI_ABGR8) { o[0] = A; o[1] = B; o[2] = G; o[3] = R; }
            else { o[0] = A; o[1] = R; o[2] = G; o[3] = B; }
        }
    }
    free(tmp);
}
@end

/* ---------------------------------------------------------------- CIFeature e QR
 * Não há detector de rosto nem leitor de QR no mOS: o CIDetector não acha nada. As classes de resultado
 * existem e guardam o que recebem (o app também cria CIQRCodeDescriptor para gerar código). */
@interface CIFeature : NSObject {
   @public
    CGRect bounds;
    NSString *type;
}
- (CGRect)bounds;
- (NSString *)type;
@end
@implementation CIFeature
- (void)dealloc { [type release]; [super dealloc]; }
- (CGRect)bounds { return bounds; }
- (NSString *)type { return type; }
@end

@interface CIFaceFeature : CIFeature
@end
@implementation CIFaceFeature
- (BOOL)hasLeftEyePosition { return NO; }
- (BOOL)hasRightEyePosition { return NO; }
- (BOOL)hasMouthPosition { return NO; }
- (BOOL)hasFaceAngle { return NO; }
- (BOOL)hasSmile { return NO; }
- (BOOL)leftEyeClosed { return NO; }
- (BOOL)rightEyeClosed { return NO; }
- (BOOL)hasTrackingID { return NO; }
- (BOOL)hasTrackingFrameCount { return NO; }
@end

/* CIQRCodeErrorCorrectionLevel: L 'L', M 'M', Q 'Q', H 'H' */
@interface CIQRCodeDescriptor : NSObject {
    NSData *payload;
    long version;
    unsigned char mask;
    long level;
}
@end
@implementation CIQRCodeDescriptor
- (instancetype)initWithPayload:(NSData *)p symbolVersion:(long)v maskPattern:(unsigned char)m errorCorrectionLevel:(long)l {
    if (!p || v < 1 || v > 40 || m > 7 || (l != 'L' && l != 'M' && l != 'Q' && l != 'H')) { [self release]; return nil; }
    self = [super init];
    if (self) { payload = [p retain]; version = v; mask = m; level = l; }
    return self;
}
+ (instancetype)descriptorWithPayload:(NSData *)p symbolVersion:(long)v maskPattern:(unsigned char)m errorCorrectionLevel:(long)l {
    return [[[self alloc] initWithPayload:p symbolVersion:v maskPattern:m errorCorrectionLevel:l] autorelease];
}
- (void)dealloc { [payload release]; [super dealloc]; }
- (NSData *)errorCorrectedPayload { return payload; }
- (long)symbolVersion { return version; }
- (unsigned char)maskPattern { return mask; }
- (long)errorCorrectionLevel { return level; }
@end

@interface CIQRCodeFeature : CIFeature {
   @public
    NSString *message;
    CIQRCodeDescriptor *descriptor;
    CGPoint corners[4];
}
@end
@implementation CIQRCodeFeature
- (void)dealloc { [message release]; [descriptor release]; [super dealloc]; }
- (NSString *)messageString { return message; }
- (CIQRCodeDescriptor *)symbolDescriptor { return descriptor; }
- (CGPoint)topLeft { return corners[0]; }
- (CGPoint)topRight { return corners[1]; }
- (CGPoint)bottomLeft { return corners[2]; }
- (CGPoint)bottomRight { return corners[3]; }
@end

/* ---------------------------------------------------------------- CIKernel
 * O mOS não compila a linguagem de kernel do CoreImage nem Metal: as fábricas devolvem nil, como a Apple
 * faz quando o código do kernel não compila. Quem pede kernel cai no caminho sem efeito do app. */
@interface CIKernel : NSObject
@end
@implementation CIKernel
+ (instancetype)kernelWithString:(NSString *)s { (void)s; return nil; }
+ (NSArray *)kernelsWithString:(NSString *)s { (void)s; return nil; }
+ (instancetype)kernelWithFunctionName:(NSString *)n fromMetalLibraryData:(NSData *)d error:(id *)e { (void)n; (void)d; if (e) *e = nil; return nil; }
+ (NSArray *)kernelNamesFromMetalLibraryData:(NSData *)d { (void)d; return [NSArray array]; }
- (NSString *)name { return nil; }
@end
@interface CIColorKernel : CIKernel
@end
@implementation CIColorKernel
@end

/* ---------------------------------------------------------------- CIRenderDestination para bitmap */
@interface CIRenderDestination : NSObject {
   @public
    void *data;
    long w, h, rb;
    int format;
    BOOL flipped;
    long alphaMode;
}
@end
@implementation CIRenderDestination
- (instancetype)initWithBitmapData:(void *)d width:(unsigned long)width height:(unsigned long)height bytesPerRow:(unsigned long)bpr format:(int)f {
    if (!d || !width || !height) { [self release]; return nil; }
    self = [super init];
    if (self) { data = d; w = (long)width; h = (long)height; rb = (long)bpr; format = f; alphaMode = 1; }
    return self;
}
- (BOOL)isFlipped { return flipped; }
- (void)setFlipped:(BOOL)v { flipped = v; }
- (long)alphaMode { return alphaMode; }
- (void)setAlphaMode:(long)v { alphaMode = v; }
- (unsigned long)width { return (unsigned long)w; }
- (unsigned long)height { return (unsigned long)h; }
@end

/* a tarefa já está pronta quando volta: o render é síncrono */
@interface CIRenderTask : NSObject
@end
@implementation CIRenderTask
- (id)waitUntilCompletedAndReturnError:(id *)e { if (e) *e = nil; return nil; }
@end

@implementation CIContext (MOSRenderDestination)
- (CIRenderTask *)startTaskToRender:(CIImage *)im toDestination:(CIRenderDestination *)dst error:(id *)e {
    if (e) *e = nil;
    if (!im || !dst) return nil;
    CGRect b = [im extent];
    CGRect r = CGRectMake(b.origin.x, b.origin.y, (double)dst->w, (double)dst->h);
    [self render:im toBitmap:dst->data rowBytes:dst->rb bounds:r format:dst->format colorSpace:nil];
    if (dst->flipped) {                          /* origem embaixo: troca as linhas */
        uint8_t *tmp = malloc((size_t)dst->rb);
        for (long y = 0; tmp && y < dst->h / 2; y++) {
            uint8_t *a = (uint8_t *)dst->data + y * dst->rb, *z = (uint8_t *)dst->data + (dst->h - 1 - y) * dst->rb;
            memcpy(tmp, a, (size_t)dst->rb); memcpy(a, z, (size_t)dst->rb); memcpy(z, tmp, (size_t)dst->rb);
        }
        free(tmp);
    }
    return [[[CIRenderTask alloc] init] autorelease];
}
@end
