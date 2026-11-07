/*
 * mOS — ImageIO.framework (Mach-O, em /System/Library/Frameworks), primeira versão: a API de fontes e destinos de
 * imagem (CGImageSource/CGImageDestination) que binários da Apple usam (o Sparkle, para o ícone do app). Ainda sem
 * decodificadores: criar uma fonte devolve NULL, como o macOS faz com um arquivo que não é imagem, e quem chama
 * segue o caminho de "sem imagem". As constantes de propriedades têm os nomes da Apple.
 * tools/build-frameworks-objc.ps1
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef const void *CFTypeRef;
typedef const struct __CFString *CFStringRef;
typedef const struct __CFURL *CFURLRef;
typedef const struct __CFDictionary *CFDictionaryRef;
typedef const void *CGImageRef;
typedef struct CGImageSource *CGImageSourceRef;
typedef struct CGImageDestination *CGImageDestinationRef;
typedef long CFIndex;
typedef unsigned long CFTypeID;
typedef unsigned char Boolean;

/* constantes CFString como as do macOS: literais do clang (o CFSTR) na seção __cfstring */
#define CFSTR_DEF(name, text) const CFStringRef name = (CFStringRef)__builtin___CFStringMakeConstantString(text);

CFSTR_DEF(kCGImagePropertyPixelWidth, "PixelWidth")
CFSTR_DEF(kCGImagePropertyPixelHeight, "PixelHeight")
CFSTR_DEF(kCGImagePropertyDPIWidth, "DPIWidth")
CFSTR_DEF(kCGImagePropertyDPIHeight, "DPIHeight")
CFSTR_DEF(kCGImagePropertyOrientation, "Orientation")
CFSTR_DEF(kCGImagePropertyHasAlpha, "HasAlpha")
CFSTR_DEF(kCGImageSourceCreateThumbnailFromImageAlways, "kCGImageSourceCreateThumbnailFromImageAlways")
CFSTR_DEF(kCGImageSourceThumbnailMaxPixelSize, "kCGImageSourceThumbnailMaxPixelSize")
CFSTR_DEF(kCGImageSourceShouldCache, "kCGImageSourceShouldCache")
CFSTR_DEF(kCGImageSourceShouldCacheImmediately, "kCGImageSourceShouldCacheImmediately")
CFSTR_DEF(kCGImageSourceCreateThumbnailWithTransform, "kCGImageSourceCreateThumbnailWithTransform")
CFSTR_DEF(kCGImageDestinationLossyCompressionQuality, "kCGImageDestinationLossyCompressionQuality")

static void warn(const char *what) {
    static int w;
    if (!w++) fprintf(stderr, "ImageIO[mOS]: sem decodificadores de imagem ainda (%s devolve NULL)\n", what);
}

CGImageSourceRef CGImageSourceCreateWithURL(CFURLRef url, CFDictionaryRef opts) { (void)url; (void)opts; warn("CGImageSourceCreateWithURL"); return NULL; }
CGImageSourceRef CGImageSourceCreateWithData(CFTypeRef data, CFDictionaryRef opts) { (void)data; (void)opts; warn("CGImageSourceCreateWithData"); return NULL; }
size_t CGImageSourceGetCount(CGImageSourceRef s) { (void)s; return 0; }
CFStringRef CGImageSourceGetType(CGImageSourceRef s) { (void)s; return NULL; }
CFDictionaryRef CGImageSourceCopyPropertiesAtIndex(CGImageSourceRef s, size_t i, CFDictionaryRef o) { (void)s; (void)i; (void)o; return NULL; }
CFDictionaryRef CGImageSourceCopyProperties(CGImageSourceRef s, CFDictionaryRef o) { (void)s; (void)o; return NULL; }
CGImageRef CGImageSourceCreateImageAtIndex(CGImageSourceRef s, size_t i, CFDictionaryRef o) { (void)s; (void)i; (void)o; return NULL; }
CGImageRef CGImageSourceCreateThumbnailAtIndex(CGImageSourceRef s, size_t i, CFDictionaryRef o) { (void)s; (void)i; (void)o; return NULL; }

CGImageDestinationRef CGImageDestinationCreateWithURL(CFURLRef url, CFStringRef type, size_t count, CFDictionaryRef o) {
    (void)url; (void)type; (void)count; (void)o; warn("CGImageDestinationCreateWithURL"); return NULL;
}
CGImageDestinationRef CGImageDestinationCreateWithData(CFTypeRef data, CFStringRef type, size_t count, CFDictionaryRef o) {
    (void)data; (void)type; (void)count; (void)o; return NULL;
}
void CGImageDestinationAddImage(CGImageDestinationRef d, CGImageRef img, CFDictionaryRef p) { (void)d; (void)img; (void)p; }
void CGImageDestinationAddImageFromSource(CGImageDestinationRef d, CGImageSourceRef s, size_t i, CFDictionaryRef p) { (void)d; (void)s; (void)i; (void)p; }
void CGImageDestinationSetProperties(CGImageDestinationRef d, CFDictionaryRef p) { (void)d; (void)p; }
Boolean CGImageDestinationFinalize(CGImageDestinationRef d) { (void)d; return 0; }
