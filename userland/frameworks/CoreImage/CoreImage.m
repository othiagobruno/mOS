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
 * além de 8 bits reto. Opções de CIContext são aceitas e ignoradas.
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
    (void)options;
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

@interface CIContext : NSObject
+ (CIContext *)contextWithOptions:(NSDictionary *)options;
+ (CIContext *)context;
- (id)createCGImage:(CIImage *)im fromRect:(CGRect)r;
- (void)render:(CIImage *)im toBitmap:(void *)data rowBytes:(long)rb bounds:(CGRect)bounds format:(int)format colorSpace:(id)cs;
@end

static void free_bytes(void *info, const void *data, size_t size) {
    (void)info; (void)size;
    free((void *)data);
}

@implementation CIContext
+ (CIContext *)contextWithOptions:(NSDictionary *)options {
    (void)options;
    return [[CIContext alloc] init];
}
+ (CIContext *)context { return [self contextWithOptions:nil]; }
- (id)createCGImage:(CIImage *)im fromRect:(CGRect)r {
    if (!im || !im->img) return nil;
    int w = (int)llround(r.size.width);
    int h = (int)llround(r.size.height);
    if (w <= 0 || h <= 0) return nil;
    uint32_t *px = malloc((size_t)w * (size_t)h * 4u);
    if (!px) return nil;
    if (mos_ci_render(im->img, r.origin.x, r.origin.y, w, h, px) != 0) { free(px); return nil; }
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
    mos_ci_render_format(im->img, bounds.origin.x, bounds.origin.y, w, h, format, data, rb);
}
@end
