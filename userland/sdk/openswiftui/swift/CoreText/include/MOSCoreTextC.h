/* mOS SDK — lado C do CoreText do mOS (módulo clang _MOSCoreTextC, reexportado pelo módulo Swift CoreText).
 *
 * CTFont, CTFontDescriptor, CTGlyphInfo e CTAdaptiveImageGlyph são tipos CF de verdade, registrados no
 * CFRuntime do mOS (libMOSCFRuntime.a, userland/sdk/openswiftui/src/cf_mos.cpp). Por isso são objetos Swift
 * nativos, que funcionam com CFGetTypeID, CFRetain/CFRelease e o ARC do Swift sem ObjC. Os atributos de um
 * descritor ficam num CFDictionary imutável; o resto da API (traços, estilos de texto, métricas) está em
 * CoreText.swift. Implementação: ctfont.c. */
#ifndef MOS_CORETEXT_C_H
#define MOS_CORETEXT_C_H
#include <CoreFoundation/CoreFoundation.h>

CF_EXTERN_C_BEGIN
CF_ASSUME_NONNULL_BEGIN

typedef const struct CF_BRIDGED_TYPE(id) __CTFont *CTFontRef;
typedef const struct CF_BRIDGED_TYPE(id) __CTFontDescriptor *CTFontDescriptorRef;
typedef const struct CF_BRIDGED_TYPE(id) __CTGlyphInfo *CTGlyphInfoRef;
typedef const struct CF_BRIDGED_TYPE(id) __CTAdaptiveImageGlyph *CTAdaptiveImageGlyphRef;

CF_EXPORT CFTypeID CTFontGetTypeID(void);
CF_EXPORT CFTypeID CTFontDescriptorGetTypeID(void);
CF_EXPORT CFTypeID CTGlyphInfoGetTypeID(void);
CF_EXPORT CFTypeID CTAdaptiveImageGlyphGetTypeID(void);

/* descritor: guarda uma cópia imutável de `attributes` */
CF_EXPORT CTFontDescriptorRef CTFontDescriptorCreateWithAttributes(CFDictionaryRef attributes) CF_RETURNS_RETAINED;
CF_EXPORT CFDictionaryRef CTFontDescriptorCopyAttributes(CTFontDescriptorRef descriptor) CF_RETURNS_RETAINED;
CF_EXPORT CFTypeRef _Nullable CTFontDescriptorCopyAttribute(CTFontDescriptorRef descriptor, CFStringRef attribute) CF_RETURNS_RETAINED;

/* fonte = descritor + tamanho em pontos */
CF_EXPORT CTFontRef _MOSCTFontCreate(CTFontDescriptorRef descriptor, double size) CF_RETURNS_RETAINED;
CF_EXPORT double _MOSCTFontGetSize(CTFontRef font);
CF_EXPORT CTFontDescriptorRef CTFontCopyFontDescriptor(CTFontRef font) CF_RETURNS_RETAINED;
CF_EXPORT CFTypeRef _Nullable CTFontCopyAttribute(CTFontRef font, CFStringRef attribute) CF_RETURNS_RETAINED;

/* chaves públicas (CFSTR em ctfont.c; as privadas que o OpenSwiftUI lê por @_silgen_name também estão lá) */
CF_EXPORT const CFStringRef kCTFontNameAttribute;
CF_EXPORT const CFStringRef kCTFontFamilyNameAttribute;
CF_EXPORT const CFStringRef kCTFontSizeAttribute;
CF_EXPORT const CFStringRef kCTFontTraitsAttribute;
CF_EXPORT const CFStringRef kCTFontSymbolicTrait;
CF_EXPORT const CFStringRef kCTFontWeightTrait;
CF_EXPORT const CFStringRef kCTFontSlantTrait;
CF_EXPORT const CFStringRef kCTFontFeatureSettingsAttribute;
CF_EXPORT const CFStringRef kCTFontVariationAttribute;
CF_EXPORT const CFStringRef kCTFontOpticalSizeAttribute;
CF_EXPORT const CFStringRef kCTFontAttributeName;
CF_EXPORT const CFStringRef kCTForegroundColorAttributeName;

CF_ASSUME_NONNULL_END
CF_EXTERN_C_END
#endif
