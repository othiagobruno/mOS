/*
 * mOS — CoreImage.framework (Mach-O, /System/Library/Frameworks).
 *
 * Núcleo em CPU (coreimage_cpu.c): CIImage a partir de CGImage, de bytes PNG/JPEG
 * (ImageIO, mos_img_decode), de CVPixelBuffer ou de buffer; extent (inclusive infinito: geradores e
 * imageByClampingToExtent), corte, afim, as 8 orientações e 45 filtros por tabela (blur gaussiano e box, ajustes de
 * cor, matriz, efeitos de cor, 20 composições/misturas W3C e Porter-Duff, máscaras, geradores e gradientes,
 * Lanczos, nitidez e pixellate), com attributes, inputKeys e as categorias.
 * CIContext createCGImage:fromRect: e render:toBitmap:... escrevem esses pixels.
 *
 * O que esta versão não faz: CIKernel, Metal, CIDetector de rosto/retângulo/texto, formatos half/float,
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
#include "qrgen.h"
#include "qrread.h"
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
+ (NSValue *)valueWithBytes:(const void *)value objCType:(const char *)type;
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
@interface CIImage (MOSMore)
- (CIImage *)imageByApplyingOrientation:(int)o;
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
- (CIImage *)imageByApplyingCGOrientation:(int)orientation {   /* as 8 orientações do EXIF (MOSMore) */
    return [self imageByApplyingOrientation:orientation];
}
- (CIImage *)imageByCompositingOverImage:(CIImage *)bg {
    if (!bg) return nil;
    return wrap(mos_ci_over(img, bg->img));
}
@end

/* ---------------------------------------------------------------- constantes de atributos e categorias (valores da Apple) */
#define CIK(n) NSString *const k##n = @#n;
CIK(CIAttributeClass) CIK(CIAttributeType) CIK(CIAttributeDefault) CIK(CIAttributeIdentity) CIK(CIAttributeMin) CIK(CIAttributeMax)
CIK(CIAttributeSliderMin) CIK(CIAttributeSliderMax) CIK(CIAttributeDisplayName) CIK(CIAttributeDescription) CIK(CIAttributeReferenceDocumentation)
CIK(CIAttributeFilterName) CIK(CIAttributeFilterDisplayName) CIK(CIAttributeFilterCategories) CIK(CIAttributeFilterAvailable_Mac)
CIK(CIAttributeFilterAvailable_iOS)
CIK(CIAttributeTypeTime) CIK(CIAttributeTypeScalar) CIK(CIAttributeTypeDistance) CIK(CIAttributeTypeAngle) CIK(CIAttributeTypeBoolean)
CIK(CIAttributeTypeInteger) CIK(CIAttributeTypeCount) CIK(CIAttributeTypePosition) CIK(CIAttributeTypeOffset) CIK(CIAttributeTypePosition3)
CIK(CIAttributeTypeRectangle) CIK(CIAttributeTypeOpaqueColor) CIK(CIAttributeTypeColor) CIK(CIAttributeTypeGradient) CIK(CIAttributeTypeImage)
CIK(CIAttributeTypeTransform)
CIK(CICategoryDistortionEffect) CIK(CICategoryGeometryAdjustment) CIK(CICategoryCompositeOperation) CIK(CICategoryHalftoneEffect)
CIK(CICategoryColorAdjustment) CIK(CICategoryColorEffect) CIK(CICategoryTransition) CIK(CICategoryTileEffect) CIK(CICategoryGenerator)
CIK(CICategoryReduction) CIK(CICategoryGradient) CIK(CICategoryStylize) CIK(CICategorySharpen) CIK(CICategoryBlur) CIK(CICategoryVideo)
CIK(CICategoryStillImage) CIK(CICategoryInterlaced) CIK(CICategoryNonSquarePixels) CIK(CICategoryHighDynamicRange) CIK(CICategoryBuiltIn)
CIK(CICategoryFilterGenerator)
#undef CIK
NSString *const kCIInputCenterKey = @"inputCenter";
NSString *const kCIInputWidthKey = @"inputWidth";
NSString *const kCIInputSharpnessKey = @"inputSharpness";
NSString *const kCIInputAngleKey = @"inputAngle";
NSString *const kCIInputAmountKey = @"inputAmount";
NSString *const kCIInputTimeKey = @"inputTime";
NSString *const kCIInputTargetImageKey = @"inputTargetImage";
NSString *const kCIInputVersionKey = @"inputVersion";

/* ---------------------------------------------------------------- CIFilter por tabela
 * Cada filtro: nome, categorias, entradas (chave, tipo, padrão, mínimo/máximo do controle) e o construtor no
 * coreimage_cpu.c. setValue:forKey: guarda no dicionário; outputImage monta o nó com os valores atuais. */
enum { T_IMAGE = 1, T_SCALAR, T_DISTANCE, T_ANGLE, T_POSITION, T_VECTOR, T_COLOR, T_RECT, T_TRANSFORM, T_COUNT, T_DATA, T_STRING };
typedef struct { const char *key; int type; double def[4]; double smin, smax; } ci_in;
typedef struct { const char *name; const char *cats[4]; ci_in in[6]; } ci_def;
#define IMG {"inputImage", T_IMAGE, {0}, 0, 0}
#define BGI {"inputBackgroundImage", T_IMAGE, {0}, 0, 0}
#define WHITE {1, 1, 1, 1}
#define BLACK {0, 0, 0, 1}
#define BLEND(n) {n, {"CICategoryCompositeOperation", "CICategoryVideo", "CICategoryStillImage"}, {IMG, BGI}}
static const ci_def ci_defs[] = {
    {"CIGaussianBlur", {"CICategoryBlur", "CICategoryStillImage", "CICategoryVideo"}, {IMG, {"inputRadius", T_DISTANCE, {10}, 0, 100}}},
    {"CIBoxBlur", {"CICategoryBlur", "CICategoryStillImage", "CICategoryVideo"}, {IMG, {"inputRadius", T_DISTANCE, {10}, 1, 100}}},
    {"CIColorControls", {"CICategoryColorAdjustment", "CICategoryVideo", "CICategoryStillImage"},
     {IMG, {"inputSaturation", T_SCALAR, {1}, 0, 2}, {"inputBrightness", T_SCALAR, {0}, -1, 1}, {"inputContrast", T_SCALAR, {1}, 0.25, 4}}},
    {"CIExposureAdjust", {"CICategoryColorAdjustment", "CICategoryVideo", "CICategoryStillImage"}, {IMG, {"inputEV", T_SCALAR, {0}, -10, 10}}},
    {"CIGammaAdjust", {"CICategoryColorAdjustment", "CICategoryVideo", "CICategoryStillImage"}, {IMG, {"inputPower", T_SCALAR, {1}, 0.25, 4}}},
    {"CIHueAdjust", {"CICategoryColorAdjustment", "CICategoryVideo", "CICategoryStillImage"}, {IMG, {"inputAngle", T_ANGLE, {0}, -3.141592653589793, 3.141592653589793}}},
    {"CIVibrance", {"CICategoryColorAdjustment", "CICategoryVideo", "CICategoryStillImage"}, {IMG, {"inputAmount", T_SCALAR, {0}, -1, 1}}},
    {"CIColorMatrix", {"CICategoryColorAdjustment", "CICategoryVideo", "CICategoryStillImage"},
     {IMG, {"inputRVector", T_VECTOR, {1, 0, 0, 0}, 0, 0}, {"inputGVector", T_VECTOR, {0, 1, 0, 0}, 0, 0}, {"inputBVector", T_VECTOR, {0, 0, 1, 0}, 0, 0},
      {"inputAVector", T_VECTOR, {0, 0, 0, 1}, 0, 0}, {"inputBiasVector", T_VECTOR, {0, 0, 0, 0}, 0, 0}}},
    {"CIColorClamp", {"CICategoryColorAdjustment", "CICategoryVideo", "CICategoryStillImage"},
     {IMG, {"inputMinComponents", T_VECTOR, {0, 0, 0, 0}, 0, 0}, {"inputMaxComponents", T_VECTOR, {1, 1, 1, 1}, 0, 0}}},
    {"CIColorInvert", {"CICategoryColorEffect", "CICategoryVideo", "CICategoryStillImage"}, {IMG}},
    {"CIColorMonochrome", {"CICategoryColorEffect", "CICategoryVideo", "CICategoryStillImage"},
     {IMG, {"inputColor", T_COLOR, {0.6, 0.45, 0.3, 1}, 0, 0}, {"inputIntensity", T_SCALAR, {1}, 0, 1}}},
    {"CISepiaTone", {"CICategoryColorEffect", "CICategoryVideo", "CICategoryStillImage"}, {IMG, {"inputIntensity", T_SCALAR, {1}, 0, 1}}},
    {"CIColorPosterize", {"CICategoryColorEffect", "CICategoryVideo", "CICategoryStillImage"}, {IMG, {"inputLevels", T_SCALAR, {6}, 2, 30}}},
    {"CIMaskToAlpha", {"CICategoryColorEffect", "CICategoryVideo", "CICategoryStillImage"}, {IMG}},
    {"CIMinimumComponent", {"CICategoryColorEffect", "CICategoryVideo", "CICategoryStillImage"}, {IMG}},
    {"CIMaximumComponent", {"CICategoryColorEffect", "CICategoryVideo", "CICategoryStillImage"}, {IMG}},
    {"CIFalseColor", {"CICategoryColorEffect", "CICategoryVideo", "CICategoryStillImage"},
     {IMG, {"inputColor0", T_COLOR, {0.3, 0, 0, 1}, 0, 0}, {"inputColor1", T_COLOR, {1, 0.9, 0.8, 1}, 0, 0}}},
    BLEND("CISourceOverCompositing"), BLEND("CISourceInCompositing"), BLEND("CISourceOutCompositing"), BLEND("CISourceAtopCompositing"),
    BLEND("CIAdditionCompositing"), BLEND("CIMultiplyCompositing"), BLEND("CIMinimumCompositing"), BLEND("CIMaximumCompositing"),
    BLEND("CIMultiplyBlendMode"), BLEND("CIScreenBlendMode"), BLEND("CIOverlayBlendMode"), BLEND("CIHardLightBlendMode"),
    BLEND("CISoftLightBlendMode"), BLEND("CIDarkenBlendMode"), BLEND("CILightenBlendMode"), BLEND("CIDifferenceBlendMode"),
    BLEND("CIExclusionBlendMode"), BLEND("CIColorDodgeBlendMode"), BLEND("CIColorBurnBlendMode"),
    {"CIBlendWithMask", {"CICategoryCompositeOperation", "CICategoryVideo", "CICategoryStillImage"}, {IMG, BGI, {"inputMaskImage", T_IMAGE, {0}, 0, 0}}},
    {"CIBlendWithAlphaMask", {"CICategoryCompositeOperation", "CICategoryVideo", "CICategoryStillImage"}, {IMG, BGI, {"inputMaskImage", T_IMAGE, {0}, 0, 0}}},
    {"CIConstantColorGenerator", {"CICategoryGenerator", "CICategoryVideo", "CICategoryStillImage"}, {{"inputColor", T_COLOR, {1, 0, 0, 1}, 0, 0}}},
    {"CILinearGradient", {"CICategoryGradient", "CICategoryVideo", "CICategoryStillImage"},
     {{"inputPoint0", T_POSITION, {0, 0}, 0, 0}, {"inputPoint1", T_POSITION, {200, 200}, 0, 0}, {"inputColor0", T_COLOR, WHITE, 0, 0}, {"inputColor1", T_COLOR, BLACK, 0, 0}}},
    {"CISmoothLinearGradient", {"CICategoryGradient", "CICategoryVideo", "CICategoryStillImage"},
     {{"inputPoint0", T_POSITION, {0, 0}, 0, 0}, {"inputPoint1", T_POSITION, {200, 200}, 0, 0}, {"inputColor0", T_COLOR, WHITE, 0, 0}, {"inputColor1", T_COLOR, BLACK, 0, 0}}},
    {"CIRadialGradient", {"CICategoryGradient", "CICategoryVideo", "CICategoryStillImage"},
     {{"inputCenter", T_POSITION, {150, 150}, 0, 0}, {"inputRadius0", T_DISTANCE, {5}, 0, 800}, {"inputRadius1", T_DISTANCE, {100}, 0, 800},
      {"inputColor0", T_COLOR, WHITE, 0, 0}, {"inputColor1", T_COLOR, BLACK, 0, 0}}},
    {"CICheckerboardGenerator", {"CICategoryGenerator", "CICategoryVideo", "CICategoryStillImage"},
     {{"inputCenter", T_POSITION, {150, 150}, 0, 0}, {"inputColor0", T_COLOR, WHITE, 0, 0}, {"inputColor1", T_COLOR, BLACK, 0, 0},
      {"inputWidth", T_DISTANCE, {80}, 0, 800}, {"inputSharpness", T_SCALAR, {1}, 0, 1}}},
    {"CIAffineTransform", {"CICategoryGeometryAdjustment", "CICategoryVideo", "CICategoryStillImage"}, {IMG, {"inputTransform", T_TRANSFORM, {0}, 0, 0}}},
    {"CICrop", {"CICategoryGeometryAdjustment", "CICategoryVideo", "CICategoryStillImage"},
     {IMG, {"inputRectangle", T_RECT, {-8.9884656743115785e307, -8.9884656743115785e307, 1.7976931348623157e308, 1.7976931348623157e308}, 0, 0}}},
    {"CILanczosScaleTransform", {"CICategoryGeometryAdjustment", "CICategoryVideo", "CICategoryStillImage"},
     {IMG, {"inputScale", T_SCALAR, {1}, 0.05, 1.5}, {"inputAspectRatio", T_SCALAR, {1}, 0.05, 2}}},
    {"CIUnsharpMask", {"CICategorySharpen", "CICategoryVideo", "CICategoryStillImage"},
     {IMG, {"inputRadius", T_DISTANCE, {2.5}, 0, 100}, {"inputIntensity", T_SCALAR, {0.5}, 0, 1}}},
    {"CISharpenLuminance", {"CICategorySharpen", "CICategoryVideo", "CICategoryStillImage"},
     {IMG, {"inputSharpness", T_SCALAR, {0.4}, 0, 2}, {"inputRadius", T_DISTANCE, {1.69}, 0, 20}}},
    {"CIQRCodeGenerator", {"CICategoryGenerator", "CICategoryStillImage"},
     {{"inputMessage", T_DATA, {0}, 0, 0}, {"inputCorrectionLevel", T_STRING, {0}, 0, 0}}},
    {"CIPixellate", {"CICategoryStylize", "CICategoryVideo", "CICategoryStillImage"},
     {IMG, {"inputCenter", T_POSITION, {150, 150}, 0, 0}, {"inputScale", T_DISTANCE, {8}, 1, 100}}},
};
#undef IMG
#undef BGI
static const ci_def *def_named(const char *n) {
    if (!n) return 0;
    for (unsigned i = 0; i < sizeof ci_defs / sizeof ci_defs[0]; i++) if (!strcmp(ci_defs[i].name, n)) return &ci_defs[i];
    return 0;
}

@interface CIFilter : NSObject {
    NSString *name;
    const ci_def *def;
    NSMutableDictionary *values;
    BOOL enabled;
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

static id default_value(const ci_in *in) {
    switch (in->type) {
    case T_IMAGE: case T_DATA: return nil;
    case T_STRING: return @"M";   /* inputCorrectionLevel */
    case T_POSITION: return [CIVector vectorWithX:in->def[0] Y:in->def[1]];
    case T_VECTOR: case T_RECT: return [CIVector vectorWithX:in->def[0] Y:in->def[1] Z:in->def[2] W:in->def[3]];
    case T_COLOR: return [CIColor colorWithRed:in->def[0] green:in->def[1] blue:in->def[2] alpha:in->def[3]];
    case T_TRANSFORM: { CGAffineTransform t = CGAffineTransformMake(1, 0, 0, 1, 0, 0); return [NSValue valueWithBytes:&t objCType:"{CGAffineTransform=dddddd}"]; }
    default: return [NSNumber numberWithDouble:in->def[0]];
    }
}
static const char *attr_type(int t) {
    switch (t) {
    case T_IMAGE: return "CIAttributeTypeImage"; case T_DISTANCE: return "CIAttributeTypeDistance"; case T_ANGLE: return "CIAttributeTypeAngle";
    case T_POSITION: return "CIAttributeTypePosition"; case T_COLOR: return "CIAttributeTypeColor"; case T_RECT: return "CIAttributeTypeRectangle";
    case T_TRANSFORM: return "CIAttributeTypeTransform"; case T_COUNT: return "CIAttributeTypeCount"; case T_VECTOR: case T_DATA: case T_STRING: return 0;
    default: return "CIAttributeTypeScalar";
    }
}
static const char *attr_class(int t) {
    if (t == T_DATA) return "NSData";
    if (t == T_STRING) return "NSString";
    return t == T_IMAGE ? "CIImage" : t == T_POSITION || t == T_VECTOR || t == T_RECT ? "CIVector" : t == T_COLOR ? "CIColor" : t == T_TRANSFORM ? "NSAffineTransform" : "NSNumber";
}

@implementation CIFilter
+ (CIFilter *)filterWithName:(NSString *)n {
    const ci_def *d = def_named(n.UTF8String);
    if (!d) return nil;
    CIFilter *f = [[[CIFilter alloc] init] autorelease];
    f->name = [n copy];
    f->def = d;
    f->values = [[NSMutableDictionary alloc] init];
    f->enabled = YES;
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
+ (CIFilter *)filterWithName:(NSString *)n withInputParameters:(NSDictionary *)params {
    CIFilter *f = [self filterWithName:n];
    for (id k in params) [f setValue:[params objectForKey:k] forKey:k];
    return f;
}
+ (NSArray *)filterNamesInCategories:(NSArray *)cats {
    NSMutableArray *out = [NSMutableArray array];
    for (unsigned i = 0; i < sizeof ci_defs / sizeof ci_defs[0]; i++) {
        BOOL all = YES;
        for (NSString *c in cats) {
            BOOL has = [c isEqualToString:@"CICategoryBuiltIn"];
            for (int k = 0; k < 4 && ci_defs[i].cats[k]; k++) if (!strcmp(ci_defs[i].cats[k], c.UTF8String)) has = YES;
            if (!has) all = NO;
        }
        if (all) [out addObject:[NSString stringWithUTF8String:ci_defs[i].name]];
    }
    return out;
}
+ (NSArray *)filterNamesInCategory:(NSString *)cat { return [self filterNamesInCategories:cat ? [NSArray arrayWithObject:cat] : nil]; }
+ (NSString *)localizedNameForFilterName:(NSString *)n { return def_named(n.UTF8String) ? [n substringFromIndex:2] : nil; }
+ (NSString *)localizedNameForCategory:(NSString *)c { return c; }
- (void)dealloc {
    [name release];
    [values release];
    [super dealloc];
}
- (NSString *)name { return name; }
- (void)setName:(NSString *)n { if (n != name) { [name release]; name = [n copy]; } }
- (BOOL)isEnabled { return enabled; }
- (void)setEnabled:(BOOL)v { enabled = v; }
- (void)setDefaults {
    for (int i = 0; i < 6 && def->in[i].key; i++) {
        id v = default_value(&def->in[i]);
        NSString *k = [NSString stringWithUTF8String:def->in[i].key];
        if (v) [values setObject:v forKey:k];
        else [values removeObjectForKey:k];
    }
}
- (NSArray *)inputKeys {
    NSMutableArray *a = [NSMutableArray array];
    for (int i = 0; i < 6 && def->in[i].key; i++) [a addObject:[NSString stringWithUTF8String:def->in[i].key]];
    return a;
}
- (NSArray *)outputKeys { return [NSArray arrayWithObject:@"outputImage"]; }
- (NSDictionary *)attributes {
    NSMutableDictionary *d = [NSMutableDictionary dictionary];
    [d setObject:name forKey:kCIAttributeFilterName];
    [d setObject:[name substringFromIndex:2] forKey:kCIAttributeFilterDisplayName];
    NSMutableArray *cats = [NSMutableArray array];
    for (int k = 0; k < 4 && def->cats[k]; k++) [cats addObject:[NSString stringWithUTF8String:def->cats[k]]];
    [cats addObject:kCICategoryBuiltIn];
    [d setObject:cats forKey:kCIAttributeFilterCategories];
    for (int i = 0; i < 6 && def->in[i].key; i++) {
        const ci_in *in = &def->in[i];
        NSMutableDictionary *a = [NSMutableDictionary dictionary];
        [a setObject:[NSString stringWithUTF8String:attr_class(in->type)] forKey:kCIAttributeClass];
        if (attr_type(in->type)) [a setObject:[NSString stringWithUTF8String:attr_type(in->type)] forKey:kCIAttributeType];
        id dv = default_value(in);
        if (dv) [a setObject:dv forKey:kCIAttributeDefault];
        if (in->smin != in->smax) {
            [a setObject:[NSNumber numberWithDouble:in->smin] forKey:kCIAttributeSliderMin];
            [a setObject:[NSNumber numberWithDouble:in->smax] forKey:kCIAttributeSliderMax];
            [a setObject:[NSNumber numberWithDouble:in->smin < 0 ? in->smin : 0] forKey:kCIAttributeMin];
        }
        [a setObject:[[NSString stringWithUTF8String:in->key] substringFromIndex:5] forKey:kCIAttributeDisplayName];
        [d setObject:a forKey:[NSString stringWithUTF8String:in->key]];
    }
    return d;
}
- (void)setValue:(id)v forKey:(NSString *)key {
    if (!key) return;
    if (v) [values setObject:v forKey:key];
    else [values removeObjectForKey:key];
}
- (id)valueForKey:(NSString *)key {
    if ([key isEqualToString:kCIOutputImageKey]) return [self outputImage];
    if ([key isEqualToString:@"name"]) return name;
    if ([key isEqualToString:@"inputKeys"]) return [self inputKeys];
    if ([key isEqualToString:@"outputKeys"]) return [self outputKeys];
    if ([key isEqualToString:@"attributes"]) return [self attributes];
    return [values objectForKey:key];
}
- (id)copyWithZone:(void *)z {
    (void)z;
    CIFilter *f = [[CIFilter alloc] init];
    f->name = [name copy];
    f->def = def;
    f->values = [values mutableCopy];
    f->enabled = enabled;
    return f;
}
/* valores atuais, já convertidos */
- (mos_ci *)img:(const char *)k { id v = [values objectForKey:[NSString stringWithUTF8String:k]]; return [v isKindOfClass:[CIImage class]] ? ((CIImage *)v)->img : 0; }
- (double)num:(const char *)k { return num_of([values objectForKey:[NSString stringWithUTF8String:k]], 0); }
- (void)vec:(const char *)k out:(double *)o n:(int)n {
    id v = [values objectForKey:[NSString stringWithUTF8String:k]];
    for (int i = 0; i < n; i++) o[i] = 0;
    if ([v isKindOfClass:[CIVector class]]) for (int i = 0; i < n; i++) o[i] = [(CIVector *)v valueAtIndex:i];
    else if ([v isKindOfClass:[CIColor class]]) { o[0] = [(CIColor *)v red]; o[1] = [(CIColor *)v green]; if (n > 2) { o[2] = [(CIColor *)v blue]; o[3] = [(CIColor *)v alpha]; } }
    else if ([v isKindOfClass:[NSNumber class]]) o[0] = [(NSNumber *)v doubleValue];
}
- (CIImage *)outputImage {
    const char *n = def->name;
    mos_ci *in = [self img:"inputImage"], *bg = [self img:"inputBackgroundImage"], *r = 0;
    double p[24];
    if (!strncmp(n, "CI", 2) && def->in[0].type == T_IMAGE && !in) return nil;   /* sem imagem de entrada: nil, como na Apple */
    if (!enabled && in) { mos_ci_retain(in); return wrap(in); }
    static const struct { const char *n; int op; } ops[] = {
        {"CIColorInvert", MOS_CI_OP_INVERT}, {"CIMaskToAlpha", MOS_CI_OP_MASK_TO_ALPHA}, {"CIMinimumComponent", MOS_CI_OP_MIN_COMPONENT},
        {"CIMaximumComponent", MOS_CI_OP_MAX_COMPONENT}};
    for (unsigned i = 0; i < sizeof ops / sizeof ops[0]; i++) if (!strcmp(n, ops[i].n)) return wrap(mos_ci_pixop(in, ops[i].op, 0, 0));
    static const struct { const char *n; int mode; } bl[] = {
        {"CISourceInCompositing", MOS_CI_BL_SOURCE_IN}, {"CISourceOutCompositing", MOS_CI_BL_SOURCE_OUT}, {"CISourceAtopCompositing", MOS_CI_BL_SOURCE_ATOP},
        {"CIAdditionCompositing", MOS_CI_BL_ADDITION}, {"CIMultiplyCompositing", MOS_CI_BL_MULTIPLY_COMPOSITING}, {"CIMinimumCompositing", MOS_CI_BL_MINIMUM},
        {"CIMaximumCompositing", MOS_CI_BL_MAXIMUM}, {"CIMultiplyBlendMode", MOS_CI_BL_MULTIPLY}, {"CIScreenBlendMode", MOS_CI_BL_SCREEN},
        {"CIOverlayBlendMode", MOS_CI_BL_OVERLAY}, {"CIHardLightBlendMode", MOS_CI_BL_HARD_LIGHT}, {"CISoftLightBlendMode", MOS_CI_BL_SOFT_LIGHT},
        {"CIDarkenBlendMode", MOS_CI_BL_DARKEN}, {"CILightenBlendMode", MOS_CI_BL_LIGHTEN}, {"CIDifferenceBlendMode", MOS_CI_BL_DIFFERENCE},
        {"CIExclusionBlendMode", MOS_CI_BL_EXCLUSION}, {"CIColorDodgeBlendMode", MOS_CI_BL_COLOR_DODGE}, {"CIColorBurnBlendMode", MOS_CI_BL_COLOR_BURN},
        {"CIBlendWithMask", MOS_CI_BL_MASK}, {"CIBlendWithAlphaMask", MOS_CI_BL_ALPHA_MASK}};
    for (unsigned i = 0; i < sizeof bl / sizeof bl[0]; i++)
        if (!strcmp(n, bl[i].n)) {
            if (!bg) { mos_ci_retain(in); return wrap(in); }   /* sem fundo: a própria imagem */
            return wrap(mos_ci_blend(in, bg, [self img:"inputMaskImage"], bl[i].mode, 0));
        }
    if (!strcmp(n, "CISourceOverCompositing")) { if (!bg) { mos_ci_retain(in); return wrap(in); } return wrap(mos_ci_over(in, bg)); }
    if (!strcmp(n, "CIGaussianBlur")) return wrap(mos_ci_blur(in, [self num:"inputRadius"]));
    if (!strcmp(n, "CIBoxBlur")) return wrap(mos_ci_box_blur(in, [self num:"inputRadius"]));
    if (!strcmp(n, "CIColorControls")) { p[0] = [self num:"inputSaturation"]; p[1] = [self num:"inputBrightness"]; p[2] = [self num:"inputContrast"]; return wrap(mos_ci_pixop(in, MOS_CI_OP_COLOR_CONTROLS, p, 3)); }
    if (!strcmp(n, "CIExposureAdjust")) { p[0] = [self num:"inputEV"]; return wrap(mos_ci_pixop(in, MOS_CI_OP_EXPOSURE, p, 1)); }
    if (!strcmp(n, "CIGammaAdjust")) { p[0] = [self num:"inputPower"]; return wrap(mos_ci_pixop(in, MOS_CI_OP_GAMMA, p, 1)); }
    if (!strcmp(n, "CIHueAdjust")) { p[0] = [self num:"inputAngle"]; return wrap(mos_ci_pixop(in, MOS_CI_OP_HUE, p, 1)); }
    if (!strcmp(n, "CIVibrance")) { p[0] = [self num:"inputAmount"]; return wrap(mos_ci_pixop(in, MOS_CI_OP_VIBRANCE, p, 1)); }
    if (!strcmp(n, "CISepiaTone")) { p[0] = [self num:"inputIntensity"]; return wrap(mos_ci_pixop(in, MOS_CI_OP_SEPIA, p, 1)); }
    if (!strcmp(n, "CIColorPosterize")) { p[0] = [self num:"inputLevels"]; return wrap(mos_ci_pixop(in, MOS_CI_OP_POSTERIZE, p, 1)); }
    if (!strcmp(n, "CIColorMatrix")) {
        [self vec:"inputRVector" out:p n:4]; [self vec:"inputGVector" out:p + 4 n:4]; [self vec:"inputBVector" out:p + 8 n:4];
        [self vec:"inputAVector" out:p + 12 n:4]; [self vec:"inputBiasVector" out:p + 16 n:4];
        return wrap(mos_ci_pixop(in, MOS_CI_OP_MATRIX, p, 20));
    }
    if (!strcmp(n, "CIColorClamp")) { [self vec:"inputMinComponents" out:p n:4]; [self vec:"inputMaxComponents" out:p + 4 n:4]; return wrap(mos_ci_pixop(in, MOS_CI_OP_CLAMP_COMPONENTS, p, 8)); }
    if (!strcmp(n, "CIColorMonochrome")) { [self vec:"inputColor" out:p n:4]; p[3] = [self num:"inputIntensity"]; return wrap(mos_ci_pixop(in, MOS_CI_OP_MONOCHROME, p, 4)); }
    if (!strcmp(n, "CIFalseColor")) { [self vec:"inputColor0" out:p n:4]; [self vec:"inputColor1" out:p + 4 n:4]; return wrap(mos_ci_pixop(in, MOS_CI_OP_FALSE_COLOR, p, 8)); }
    if (!strcmp(n, "CIConstantColorGenerator")) { [self vec:"inputColor" out:p n:4]; return wrap(mos_ci_generator(MOS_CI_GEN_CONSTANT, p, 4)); }
    if (!strcmp(n, "CILinearGradient") || !strcmp(n, "CISmoothLinearGradient")) {
        [self vec:"inputPoint0" out:p n:2]; [self vec:"inputPoint1" out:p + 2 n:2]; [self vec:"inputColor0" out:p + 4 n:4]; [self vec:"inputColor1" out:p + 8 n:4];
        return wrap(mos_ci_generator(n[2] == 'S' ? MOS_CI_GEN_SMOOTH_LINEAR : MOS_CI_GEN_LINEAR, p, 12));
    }
    if (!strcmp(n, "CIRadialGradient")) {
        [self vec:"inputCenter" out:p n:2]; p[2] = [self num:"inputRadius0"]; p[3] = [self num:"inputRadius1"];
        [self vec:"inputColor0" out:p + 4 n:4]; [self vec:"inputColor1" out:p + 8 n:4];
        return wrap(mos_ci_generator(MOS_CI_GEN_RADIAL, p, 12));
    }
    if (!strcmp(n, "CICheckerboardGenerator")) {
        [self vec:"inputCenter" out:p n:2]; [self vec:"inputColor0" out:p + 2 n:4]; [self vec:"inputColor1" out:p + 6 n:4];
        p[10] = [self num:"inputWidth"]; p[11] = [self num:"inputSharpness"];
        return wrap(mos_ci_generator(MOS_CI_GEN_CHECKER, p, 12));
    }
    if (!strcmp(n, "CIAffineTransform")) {
        CGAffineTransform t = CGAffineTransformMake(1, 0, 0, 1, 0, 0);
        id v = [values objectForKey:@"inputTransform"];
        if ([v isKindOfClass:[NSValue class]]) [(NSValue *)v getValue:&t];
        return wrap(mos_ci_affine(in, t.a, t.b, t.c, t.d, t.tx, t.ty));
    }
    if (!strcmp(n, "CICrop")) { [self vec:"inputRectangle" out:p n:4]; return wrap(mos_ci_crop(in, p[0], p[1], p[2], p[3])); }
    if (!strcmp(n, "CILanczosScaleTransform")) return wrap(mos_ci_lanczos(in, [self num:"inputScale"], [self num:"inputAspectRatio"]));
    if (!strcmp(n, "CIUnsharpMask") || !strcmp(n, "CISharpenLuminance")) {
        int sharpen = n[2] == 'S';
        mos_ci *cl = mos_ci_clamp(in), *bl2 = cl ? mos_ci_blur(cl, [self num:"inputRadius"]) : 0;
        r = bl2 ? mos_ci_blend(in, bl2, 0, sharpen ? MOS_CI_BL_SHARPEN_LUMA : MOS_CI_BL_UNSHARP, [self num:sharpen ? "inputSharpness" : "inputIntensity"]) : 0;
        mos_ci_release(cl); mos_ci_release(bl2);
        return wrap(r);
    }
    if (!strcmp(n, "CIQRCodeGenerator")) {
        /* um pixel por módulo, módulo escuro preto e claro branco, com QR_MARGIN módulos claros em volta.
         * A margem que a Apple usa não foi conferida num Mac (fica 1 módulo até medir). */
        enum { QR_MARGIN = 1 };
        id d = [values objectForKey:@"inputMessage"];
        NSString *lv = [values objectForKey:@"inputCorrectionLevel"];
        if (![d respondsToSelector:@selector(bytes)]) return nil;
        const char *l = [lv isKindOfClass:[NSString class]] ? lv.UTF8String : "M";
        int level = l[0] == 'L' ? 0 : l[0] == 'Q' ? 2 : l[0] == 'H' ? 3 : 1, size = 0;
        static uint8_t mods[177 * 177];
        if (mos_qr_encode([(NSData *)d bytes], (int)[(NSData *)d length], level, 0, -1, mods, &size) < 0) return nil;
        int W = size + 2 * QR_MARGIN;
        uint32_t *px = malloc((size_t)W * W * 4u);
        if (!px) return nil;
        for (int y = 0; y < W; y++)
            for (int x = 0; x < W; x++) {
                int mx = x - QR_MARGIN, my = y - QR_MARGIN;
                int dark = mx >= 0 && my >= 0 && mx < size && my < size && mods[my * size + mx];
                px[(size_t)y * W + x] = dark ? 0xFF000000u : 0xFFFFFFFFu;
            }
        CIImage *o = from_px(px, W, W);
        free(px);
        return o;
    }
    if (!strcmp(n, "CIPixellate")) { [self vec:"inputCenter" out:p n:2]; return wrap(mos_ci_pixellate(in, p[0], p[1], [self num:"inputScale"])); }
    return nil;
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
    void *cg = CGImageCreate((size_t)w, (size_t)h, 8, 32, (size_t)w * 4u, 0, 4 | 0x2000 /* AlphaFirst, 32 little: px é 0xAARRGGBB */, prov, 0, 0, 0);
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

/* ---------------------------------------------------------------- CIFeature, QR e CIDetector
 * CIDetectorTypeQRCode lê QR Code de verdade (qrread.c). Rosto, retângulo e texto não têm detector no mOS:
 * detectorOfType: devolve nil para eles, como a Apple faz com um tipo que não conhece. */
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

@interface NSString (MOSBytes)
- (id)initWithBytes:(const void *)bytes length:(NSUInteger)len encoding:(NSUInteger)enc;
@end
@interface CIDetector : NSObject {
    CIContext *context;
}
@end
@implementation CIDetector
+ (CIDetector *)detectorOfType:(NSString *)type context:(CIContext *)ctx options:(NSDictionary *)options {
    (void)options;
    if (![type isEqualToString:@"CIDetectorTypeQRCode"]) return nil;
    CIDetector *d = [[[CIDetector alloc] init] autorelease];
    d->context = ctx ? [ctx retain] : [[CIContext contextWithOptions:nil] retain];
    return d;
}
- (void)dealloc { [context release]; [super dealloc]; }
- (NSArray *)featuresInImage:(CIImage *)image { return [self featuresInImage:image options:nil]; }
- (NSArray *)featuresInImage:(CIImage *)image options:(NSDictionary *)options {
    (void)options;
    NSMutableArray *out = [NSMutableArray array];
    if (!image || !image->img) return out;
    CGRect e = [image extent];
    if (mos_ci_is_infinite(image->img) || e.size.width < 21 || e.size.height < 21) return out;
    int w = (int)llround(e.size.width), h = (int)llround(e.size.height);
    if ((long)w * h > 6000L * 6000L) return out;
    uint32_t *px = malloc((size_t)w * h * 4);
    uint8_t *g = malloc((size_t)w * h);
    mos_qr_result *r = calloc(1, sizeof *r);
    if (!px || !g || !r || mos_ci_render(image->img, e.origin.x, e.origin.y, w, h, px) != 0) { free(px); free(g); free(r); return out; }
    for (long i = 0; i < (long)w * h; i++) {   /* luminância; o transparente conta como branco */
        uint32_t c = px[i], a = c >> 24;
        uint32_t lum = (((c >> 16) & 255) * 77 + ((c >> 8) & 255) * 150 + (c & 255) * 29) >> 8;
        g[i] = (uint8_t)((lum * a + 255 * (255 - a)) / 255);
    }
    free(px);
    if (mos_qr_read(g, w, h, r)) {
        CIQRCodeFeature *f = [[CIQRCodeFeature alloc] init];
        double minx = 1e18, miny = 1e18, maxx = -1e18, maxy = -1e18;
        for (int i = 0; i < 4; i++) {   /* pixels (y para baixo) -> coordenadas da imagem (y para cima) */
            CGPoint p = CGPointMake(e.origin.x + r->corners[i][0], e.origin.y + (h - r->corners[i][1]));
            f->corners[i] = p;
            if (p.x < minx) minx = p.x;
            if (p.y < miny) miny = p.y;
            if (p.x > maxx) maxx = p.x;
            if (p.y > maxy) maxy = p.y;
        }
        f->bounds = CGRectMake(minx, miny, maxx - minx, maxy - miny);
        f->type = [@"QRCode" retain];
        NSString *msg = nil;
        if (r->kanji) msg = [[NSString alloc] initWithBytes:r->text length:(NSUInteger)r->len encoding:8 /* NSShiftJISStringEncoding */];
        if (!msg) msg = [[NSString alloc] initWithBytes:r->text length:(NSUInteger)r->len encoding:NSUTF8StringEncoding];
        if (!msg) msg = [[NSString alloc] initWithBytes:r->text length:(NSUInteger)r->len encoding:5 /* NSISOLatin1StringEncoding */];
        f->message = msg;
        static const long lv[4] = {'L', 'M', 'Q', 'H'};
        f->descriptor = [[CIQRCodeDescriptor alloc] initWithPayload:[NSData dataWithBytes:r->data length:(NSUInteger)r->ndata]
                                                      symbolVersion:r->version maskPattern:(unsigned char)r->mask errorCorrectionLevel:lv[r->level]];
        [out addObject:f];
        [f release];
    }
    free(g); free(r);
    return out;
}
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

/* ---------------------------------------------------------------- CIColor, CIVector e CIImage: o resto da API usada */
extern const double *CGColorGetComponents(void *c);
extern size_t CGColorGetNumberOfComponents(void *c);
extern uint32_t CVPixelBufferGetPixelFormatType(void *p);
extern size_t CVPixelBufferGetWidth(void *p), CVPixelBufferGetHeight(void *p), CVPixelBufferGetBytesPerRow(void *p);
extern size_t CVPixelBufferGetBytesPerRowOfPlane(void *p, size_t i);
extern void *CVPixelBufferGetBaseAddress(void *p), *CVPixelBufferGetBaseAddressOfPlane(void *p, size_t i);
extern int CVPixelBufferLockBaseAddress(void *p, uint64_t f), CVPixelBufferUnlockBaseAddress(void *p, uint64_t f);

@implementation CIColor (MOSMore)
+ (CIColor *)colorWithRed:(double)rr green:(double)gg blue:(double)bb { return [self colorWithRed:rr green:gg blue:bb alpha:1]; }
+ (CIColor *)colorWithCGColor:(void *)c {
    const double *v = c ? CGColorGetComponents(c) : 0;
    size_t n = c ? CGColorGetNumberOfComponents(c) : 0;
    if (!v || !n) return nil;
    if (n == 2) return [self colorWithRed:v[0] green:v[0] blue:v[0] alpha:v[1]];   /* cinza + alfa */
    return [self colorWithRed:v[0] green:n > 1 ? v[1] : 0 blue:n > 2 ? v[2] : 0 alpha:n > 3 ? v[3] : 1];
}
+ (CIColor *)colorWithString:(NSString *)s {
    double c[4] = {0, 0, 0, 1};
    const char *p = s.UTF8String;
    for (int i = 0; p && i < 4; i++) {
        while (*p == ' ' || *p == '[' || *p == ']') p++;
        if (!*p) break;
        char *e;
        double v = strtod(p, &e);
        if (e == p) break;
        c[i] = v;
        p = e;
    }
    return [self colorWithRed:c[0] green:c[1] blue:c[2] alpha:c[3]];
}
- (NSString *)stringRepresentation { return [NSString stringWithFormat:@"%g %g %g %g", r, g, b, a]; }
- (size_t)numberOfComponents { return 4; }
+ (CIColor *)blackColor { return [self colorWithRed:0 green:0 blue:0 alpha:1]; }
+ (CIColor *)whiteColor { return [self colorWithRed:1 green:1 blue:1 alpha:1]; }
+ (CIColor *)grayColor { return [self colorWithRed:0.5 green:0.5 blue:0.5 alpha:1]; }
+ (CIColor *)redColor { return [self colorWithRed:1 green:0 blue:0 alpha:1]; }
+ (CIColor *)greenColor { return [self colorWithRed:0 green:1 blue:0 alpha:1]; }
+ (CIColor *)blueColor { return [self colorWithRed:0 green:0 blue:1 alpha:1]; }
+ (CIColor *)cyanColor { return [self colorWithRed:0 green:1 blue:1 alpha:1]; }
+ (CIColor *)magentaColor { return [self colorWithRed:1 green:0 blue:1 alpha:1]; }
+ (CIColor *)yellowColor { return [self colorWithRed:1 green:1 blue:0 alpha:1]; }
+ (CIColor *)clearColor { return [self colorWithRed:0 green:0 blue:0 alpha:0]; }
@end

@implementation CIVector (MOSMore)
+ (CIVector *)vectorWithX:(double)x { CIVector *o = [self vectorWithX:x Y:0]; o->n = 1; return o; }
+ (CIVector *)vectorWithX:(double)x Y:(double)y Z:(double)z { CIVector *o = [self vectorWithX:x Y:y Z:z W:0]; o->n = 3; return o; }
+ (CIVector *)vectorWithValues:(const double *)vals count:(size_t)c {
    CIVector *o = [self vectorWithX:0 Y:0 Z:0 W:0];
    for (size_t i = 0; i < c && i < 4; i++) o->v[i] = vals[i];
    o->n = (int)(c < 4 ? c : 4);
    return o;
}
+ (CIVector *)vectorWithCGPoint:(CGPoint)p { return [self vectorWithX:p.x Y:p.y]; }
+ (CIVector *)vectorWithCGRect:(CGRect)r { return [self vectorWithX:r.origin.x Y:r.origin.y Z:r.size.width W:r.size.height]; }
- (size_t)count { return (size_t)n; }
- (CGPoint)CGPointValue { return CGPointMake(v[0], v[1]); }
- (CGRect)CGRectValue { return CGRectMake(v[0], v[1], v[2], v[3]); }
- (NSString *)stringRepresentation {
    if (n <= 2) return [NSString stringWithFormat:@"[%g %g]", v[0], v[1]];
    if (n == 3) return [NSString stringWithFormat:@"[%g %g %g]", v[0], v[1], v[2]];
    return [NSString stringWithFormat:@"[%g %g %g %g]", v[0], v[1], v[2], v[3]];
}
@end

/* YCbCr de vídeo (faixa curta, BT.601) -> ARGB; '420f' é faixa cheia */
static uint32_t ycc(int Y, int Cb, int Cr, int full) {
    double y = full ? Y : (Y - 16) * 255.0 / 219.0, cb = (Cb - 128) * (full ? 1 : 255.0 / 224.0), cr = (Cr - 128) * (full ? 1 : 255.0 / 224.0);
    double R = y + 1.402 * cr, G = y - 0.344136 * cb - 0.714136 * cr, B = y + 1.772 * cb;
    int r = R < 0 ? 0 : R > 255 ? 255 : (int)(R + 0.5), g = G < 0 ? 0 : G > 255 ? 255 : (int)(G + 0.5), b = B < 0 ? 0 : B > 255 ? 255 : (int)(B + 0.5);
    return 0xFF000000u | (uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b;
}

@implementation CIImage (MOSMore)
+ (CIImage *)imageWithColor:(CIColor *)c {
    double p[4] = {[c red], [c green], [c blue], [c alpha]};
    return wrap(mos_ci_generator(MOS_CI_GEN_CONSTANT, p, 4));
}
+ (CIImage *)emptyImage { return [[[CIImage alloc] init] autorelease]; }   /* extent vazio, não desenha nada */
- (CIImage *)imageByClampingToExtent { return img ? wrap(mos_ci_clamp(img)) : self; }
- (CIImage *)imageByClampingToRect:(CGRect)r {
    mos_ci *c = img ? mos_ci_crop(img, r.origin.x, r.origin.y, r.size.width, r.size.height) : 0;
    CIImage *o = c ? wrap(mos_ci_clamp(c)) : nil;
    mos_ci_release(c);
    return o;
}
- (CIImage *)imageByApplyingFilter:(NSString *)name withInputParameters:(NSDictionary *)params {
    CIFilter *f = [CIFilter filterWithName:name withInputParameters:params];
    [f setValue:self forKey:kCIInputImageKey];
    return [f outputImage];
}
- (CIImage *)imageByApplyingFilter:(NSString *)name { return [self imageByApplyingFilter:name withInputParameters:nil]; }
- (CIImage *)imageByApplyingGaussianBlurWithSigma:(double)sigma { return img ? wrap(mos_ci_blur(img, sigma)) : nil; }
- (CIImage *)imageByCompositingOverImage:(CIImage *)bg withMode:(int)mode { (void)mode; return [self imageByCompositingOverImage:bg]; }
- (CIImage *)imageBySettingAlphaOneInExtent:(CGRect)r {
    double m[20] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    mos_ci *c = img ? mos_ci_crop(img, r.origin.x, r.origin.y, r.size.width, r.size.height) : 0;
    CIImage *o = c ? wrap(mos_ci_pixop(c, MOS_CI_OP_MATRIX, m, 20)) : nil;
    mos_ci_release(c);
    return o;
}
- (CGAffineTransform)imageTransformForCGOrientation:(int)o {
    CGRect e = [self extent];
    double w = e.size.width, h = e.size.height;
    switch (o) {
    case 2: return CGAffineTransformMake(-1, 0, 0, 1, w, 0);
    case 3: return CGAffineTransformMake(-1, 0, 0, -1, w, h);
    case 4: return CGAffineTransformMake(1, 0, 0, -1, 0, h);
    case 5: return CGAffineTransformMake(0, -1, -1, 0, h, w);
    case 6: return CGAffineTransformMake(0, -1, 1, 0, 0, w);
    case 7: return CGAffineTransformMake(0, 1, 1, 0, 0, 0);
    case 8: return CGAffineTransformMake(0, 1, -1, 0, h, 0);
    default: return CGAffineTransformMake(1, 0, 0, 1, 0, 0);
    }
}
- (CIImage *)imageByApplyingOrientation:(int)o {
    if (!img || o < 1 || o > 8) return o == 0 ? self : nil;
    CGRect e = [self extent];
    CGAffineTransform t = [self imageTransformForCGOrientation:o];
    /* a orientação gira em torno da origem do extent */
    mos_ci *a = mos_ci_affine(img, t.a, t.b, t.c, t.d, t.tx + e.origin.x - (t.a * e.origin.x + t.c * e.origin.y), t.ty + e.origin.y - (t.b * e.origin.x + t.d * e.origin.y));
    return wrap(a);
}
+ (CIImage *)imageWithCVPixelBuffer:(void *)pb options:(NSDictionary *)opt {
    (void)opt;
    if (!pb) return nil;
    uint32_t fmt = CVPixelBufferGetPixelFormatType(pb);
    int w = (int)CVPixelBufferGetWidth(pb), h = (int)CVPixelBufferGetHeight(pb);
    if (w <= 0 || h <= 0) return nil;
    uint32_t *px = malloc((size_t)w * (size_t)h * 4u);
    if (!px) return nil;
    CVPixelBufferLockBaseAddress(pb, 1);
    int ok = 1;
    if (fmt == 0x42475241 /* BGRA */ || fmt == 0x20 /* 32ARGB */) {
        const uint8_t *b = CVPixelBufferGetBaseAddress(pb);
        size_t rb = CVPixelBufferGetBytesPerRow(pb);
        for (int y = 0; b && y < h; y++)
            for (int x = 0; x < w; x++) {
                const uint8_t *s = b + (size_t)y * rb + (size_t)x * 4;
                uint32_t p = fmt == 0x20 ? (uint32_t)s[0] << 24 | (uint32_t)s[1] << 16 | (uint32_t)s[2] << 8 | s[3]
                                         : (uint32_t)s[3] << 24 | (uint32_t)s[2] << 16 | (uint32_t)s[1] << 8 | s[0];
                px[(size_t)y * w + x] = unpremul(p);
            }
        ok = b != 0;
    } else if (fmt == 0x34323076 /* 420v */ || fmt == 0x34323066 /* 420f */) {
        const uint8_t *Y = CVPixelBufferGetBaseAddressOfPlane(pb, 0), *C = CVPixelBufferGetBaseAddressOfPlane(pb, 1);
        size_t ry = CVPixelBufferGetBytesPerRowOfPlane(pb, 0), rc = CVPixelBufferGetBytesPerRowOfPlane(pb, 1);
        for (int y = 0; Y && C && y < h; y++)
            for (int x = 0; x < w; x++) {
                const uint8_t *c = C + (size_t)(y / 2) * rc + (size_t)(x / 2) * 2;
                px[(size_t)y * w + x] = ycc(Y[(size_t)y * ry + x], c[0], c[1], fmt == 0x34323066);
            }
        ok = Y && C;
    } else ok = 0;
    CVPixelBufferUnlockBaseAddress(pb, 1);
    CIImage *o = ok ? from_px(px, w, h) : nil;
    free(px);
    return o;
}
+ (CIImage *)imageWithCVPixelBuffer:(void *)pb { return [self imageWithCVPixelBuffer:pb options:nil]; }
+ (CIImage *)imageWithCVImageBuffer:(void *)pb { return [self imageWithCVPixelBuffer:pb options:nil]; }
+ (CIImage *)imageWithCVImageBuffer:(void *)pb options:(NSDictionary *)opt { return [self imageWithCVPixelBuffer:pb options:opt]; }
- (NSDictionary *)properties { return [NSDictionary dictionary]; }
- (id)colorSpace { return nil; }
- (void *)pixelBuffer { return 0; }
- (void *)CGImage { return 0; }
- (NSURL *)url { return nil; }
@end

@implementation CIContext (MOSPixelBuffer)
+ (CIContext *)contextWithCGContext:(void *)cg options:(NSDictionary *)o { (void)cg; return [self contextWithOptions:o]; }
+ (CIContext *)contextWithMTLDevice:(id)d { (void)d; return [self contextWithOptions:nil]; }
+ (CIContext *)contextWithMTLDevice:(id)d options:(NSDictionary *)o { (void)d; return [self contextWithOptions:o]; }
- (id)createCGImage:(CIImage *)im fromRect:(CGRect)r format:(int)f colorSpace:(id)cs { (void)f; (void)cs; return [self createCGImage:im fromRect:r]; }
- (void)render:(CIImage *)im toCVPixelBuffer:(void *)pb bounds:(CGRect)b colorSpace:(id)cs {
    (void)cs;
    if (!im || !pb) return;
    uint32_t fmt = CVPixelBufferGetPixelFormatType(pb);
    int w = (int)CVPixelBufferGetWidth(pb), h = (int)CVPixelBufferGetHeight(pb);
    int bw = (int)llround(b.size.width), bh = (int)llround(b.size.height);
    if (bw > w) bw = w;
    if (bh > h) bh = h;
    if (bw <= 0 || bh <= 0) return;
    uint32_t *px = malloc((size_t)bw * (size_t)bh * 4u);
    if (!px) return;
    if (!ready) mos_ci_ctx_init(&ctx, 1), ready = 1;
    if (mos_ci_ctx_render(&ctx, im->img, b.origin.x, b.origin.y, bw, bh, px) != 0) { free(px); return; }
    CVPixelBufferLockBaseAddress(pb, 0);
    if (fmt == 0x42475241 || fmt == 0x20) {
        uint8_t *d = CVPixelBufferGetBaseAddress(pb);
        size_t rb = CVPixelBufferGetBytesPerRow(pb);
        for (int y = 0; d && y < bh; y++)
            for (int x = 0; x < bw; x++) {
                uint32_t p = px[(size_t)y * bw + x];
                unsigned A = p >> 24, R = (p >> 16 & 255) * A / 255, G = (p >> 8 & 255) * A / 255, B = (p & 255) * A / 255;   /* pré-multiplicado */
                uint8_t *o = d + (size_t)y * rb + (size_t)x * 4;
                if (fmt == 0x20) { o[0] = (uint8_t)A; o[1] = (uint8_t)R; o[2] = (uint8_t)G; o[3] = (uint8_t)B; }
                else { o[0] = (uint8_t)B; o[1] = (uint8_t)G; o[2] = (uint8_t)R; o[3] = (uint8_t)A; }
            }
    } else if (fmt == 0x34323076 || fmt == 0x34323066) {   /* BT.601, faixa curta ('420v') ou cheia ('420f') */
        int full = fmt == 0x34323066;
        uint8_t *Y = CVPixelBufferGetBaseAddressOfPlane(pb, 0), *C = CVPixelBufferGetBaseAddressOfPlane(pb, 1);
        size_t ry = CVPixelBufferGetBytesPerRowOfPlane(pb, 0), rc = CVPixelBufferGetBytesPerRowOfPlane(pb, 1);
        for (int y = 0; Y && C && y < bh; y++)
            for (int x = 0; x < bw; x++) {
                uint32_t p = px[(size_t)y * bw + x];
                double R = p >> 16 & 255, G = p >> 8 & 255, B = p & 255, l = 0.299 * R + 0.587 * G + 0.114 * B;
                double v = full ? l : 16 + l * 219.0 / 255.0;
                Y[(size_t)y * ry + x] = (uint8_t)(v + 0.5);
                if (!(x & 1) && !(y & 1)) {
                    double cb = (B - l) / 1.772, cr = (R - l) / 1.402, k = full ? 1 : 224.0 / 255.0;
                    uint8_t *c = C + (size_t)(y / 2) * rc + (size_t)(x / 2) * 2;
                    double u = 128 + cb * k, vv = 128 + cr * k;
                    c[0] = (uint8_t)(u < 0 ? 0 : u > 255 ? 255 : u + 0.5);
                    c[1] = (uint8_t)(vv < 0 ? 0 : vv > 255 ? 255 : vv + 0.5);
                }
            }
    }
    CVPixelBufferUnlockBaseAddress(pb, 0);
    free(px);
}
- (void)render:(CIImage *)im toCVPixelBuffer:(void *)pb {
    if (!pb) return;
    [self render:im toCVPixelBuffer:pb bounds:CGRectMake(0, 0, (double)CVPixelBufferGetWidth(pb), (double)CVPixelBufferGetHeight(pb)) colorSpace:nil];
}
- (void)clearCaches { if (ready) { mos_ci_ctx_free(&ctx); mos_ci_ctx_init(&ctx, 1); } }
- (id)workingColorSpace { return nil; }
- (int)workingFormat { return MOS_CI_RGBA8; }
@end
