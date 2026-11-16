/* mOS — CoreText do SwiftUI.framework na variante com interop ObjC (docs/swift-objc-runtime.md, fase 6).
 * Só declarações, com nomes e assinaturas do CoreText da Apple. Quem implementa é o CoreText do libSystem
 * (userland/libsystem/coretext.c, ct_layout.c...): os CTFont e CTFontDescriptor são os objetos CF/ObjC de lá.
 * Substitui, nesta variante, o userland/sdk/openswiftui/swift/CoreText/include/MOSCoreTextC.h da variante nativa
 * (objetos CF no formato Swift, ctfont.c), que não pode conviver com o CoreText do libSystem no mesmo processo.
 * A SPI privada que o OpenSwiftUI chama (CTFontDescriptorGetTextStyleSize...) ele mesmo declara por @_silgen_name. */
#ifndef MOS_CORETEXT_OBJC_H
#define MOS_CORETEXT_OBJC_H
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>

CF_EXTERN_C_BEGIN
CF_ASSUME_NONNULL_BEGIN

typedef const struct CF_BRIDGED_TYPE(id) __CTFont *CTFontRef;
typedef const struct CF_BRIDGED_TYPE(id) __CTFontDescriptor *CTFontDescriptorRef;
typedef const struct CF_BRIDGED_TYPE(id) __CTGlyphInfo *CTGlyphInfoRef;
typedef const struct CF_BRIDGED_TYPE(id) __CTLine *CTLineRef;
typedef const struct CF_BRIDGED_TYPE(id) __CTAdaptiveImageGlyph *CTAdaptiveImageGlyphRef;

typedef CF_OPTIONS(uint32_t, CTFontSymbolicTraits) {
    kCTFontTraitItalic = (1 << 0),
    kCTFontTraitBold = (1 << 1),
    kCTFontTraitExpanded = (1 << 5),
    kCTFontTraitCondensed = (1 << 6),
    kCTFontTraitMonoSpace = (1 << 10),
    kCTFontTraitVertical = (1 << 11),
    kCTFontTraitUIOptimized = (1 << 12),
    kCTFontTraitColorGlyphs = (1 << 13),
    kCTFontTraitComposite = (1 << 14),
    kCTFontTraitClassMask = (15U << 28),
    kCTFontItalicTrait = kCTFontTraitItalic,
    kCTFontBoldTrait = kCTFontTraitBold,
    kCTFontExpandedTrait = kCTFontTraitExpanded,
    kCTFontCondensedTrait = kCTFontTraitCondensed,
    kCTFontMonoSpaceTrait = kCTFontTraitMonoSpace,
    kCTFontVerticalTrait = kCTFontTraitVertical,
    kCTFontUIOptimizedTrait = kCTFontTraitUIOptimized,
    kCTFontColorGlyphsTrait = kCTFontTraitColorGlyphs,
    kCTFontCompositeTrait = kCTFontTraitComposite,
    kCTFontClassMaskTrait = kCTFontTraitClassMask
};

typedef CF_OPTIONS(uint32_t, CTFontStylisticClass) {
    kCTFontClassUnknown = (0 << 28),
    kCTFontClassOldStyleSerifs = (1 << 28),
    kCTFontClassTransitionalSerifs = (2 << 28),
    kCTFontClassModernSerifs = (3 << 28),
    kCTFontClassClarendonSerifs = (4 << 28),
    kCTFontClassSlabSerifs = (5 << 28),
    kCTFontClassFreeformSerifs = (7 << 28),
    kCTFontClassSansSerif = (8U << 28),
    kCTFontClassOrnamentals = (9U << 28),
    kCTFontClassScripts = (10U << 28),
    kCTFontClassSymbolic = (12U << 28),
    kCTFontUnknownClass = kCTFontClassUnknown,
    kCTFontOldStyleSerifsClass = kCTFontClassOldStyleSerifs,
    kCTFontTransitionalSerifsClass = kCTFontClassTransitionalSerifs,
    kCTFontModernSerifsClass = kCTFontClassModernSerifs,
    kCTFontClarendonSerifsClass = kCTFontClassClarendonSerifs,
    kCTFontSlabSerifsClass = kCTFontClassSlabSerifs,
    kCTFontFreeformSerifsClass = kCTFontClassFreeformSerifs,
    kCTFontSansSerifClass = kCTFontClassSansSerif,
    kCTFontOrnamentalsClass = kCTFontClassOrnamentals,
    kCTFontScriptsClass = kCTFontClassScripts,
    kCTFontSymbolicClass = kCTFontClassSymbolic
};

typedef CF_ENUM(uint32_t, CTFontUIFontType) {
    kCTFontUIFontNone = (uint32_t)-1,
    kCTFontUIFontUser = 0,
    kCTFontUIFontUserFixedPitch = 1,
    kCTFontUIFontSystem = 2,
    kCTFontUIFontEmphasizedSystem = 3,
    kCTFontUIFontSmallSystem = 4,
    kCTFontUIFontSmallEmphasizedSystem = 5,
    kCTFontUIFontMiniSystem = 6,
    kCTFontUIFontMiniEmphasizedSystem = 7,
    kCTFontUIFontViews = 8,
    kCTFontUIFontApplication = 9,
    kCTFontUIFontLabel = 10,
    kCTFontUIFontMenuTitle = 11,
    kCTFontUIFontMenuItem = 12,
    kCTFontUIFontMenuItemMark = 13,
    kCTFontUIFontMenuItemCmdKey = 14,
    kCTFontUIFontWindowTitle = 15,
    kCTFontUIFontPushButton = 16,
    kCTFontUIFontUtilityWindowTitle = 17,
    kCTFontUIFontAlertHeader = 18,
    kCTFontUIFontSystemDetail = 19,
    kCTFontUIFontEmphasizedSystemDetail = 20,
    kCTFontUIFontToolbar = 21,
    kCTFontUIFontSmallToolbar = 22,
    kCTFontUIFontMessage = 23,
    kCTFontUIFontPalette = 24,
    kCTFontUIFontToolTip = 25,
    kCTFontUIFontControlContent = 26
};

typedef CF_OPTIONS(CFOptionFlags, CTFontDescriptorOptions) {
    kCTFontDescriptorOptionSystemUIFont = 1 << 1,
    kCTFontDescriptorOptionPreferAppleSystemFont = kCTFontDescriptorOptionSystemUIFont
};

typedef CF_OPTIONS(CFOptionFlags, CTFontOptions) {
    kCTFontOptionsDefault = 0,
    kCTFontOptionsPreventAutoActivation = 1 << 0,
    kCTFontOptionsPreventAutoDownload = 1 << 1,
    kCTFontOptionsPreferSystemFont = 1 << 2
};

/* SPI (CoreText privado): peso de legibilidade guardado em kCTFontLegibilityWeightAttribute */
typedef CF_ENUM(CFIndex, CTFontLegibilityWeight) {
    kCTFontLegibilityWeightRegular = 0,
    kCTFontLegibilityWeightBold = 1
};

/* SPI: idioma de composição do parágrafo */
typedef CF_ENUM(int32_t, CTCompositionLanguage) {
    kCTCompositionLanguageUnset = 0,
    kCTCompositionLanguageNone = 1,
    kCTCompositionLanguageJapanese = 2,
    kCTCompositionLanguageSimplifiedChinese = 3,
    kCTCompositionLanguageTraditionalChinese = 4
};

CF_EXPORT CFTypeID CTFontGetTypeID(void);
CF_EXPORT CFTypeID CTFontDescriptorGetTypeID(void);
CF_EXPORT CFTypeID CTGlyphInfoGetTypeID(void);
CF_EXPORT CFTypeID CTLineGetTypeID(void);

/* descritor */
CF_EXPORT CTFontDescriptorRef CTFontDescriptorCreateWithAttributes(CFDictionaryRef attributes) CF_RETURNS_RETAINED;
CF_EXPORT CTFontDescriptorRef CTFontDescriptorCreateWithNameAndSize(CFStringRef name, CGFloat size) CF_RETURNS_RETAINED;
CF_EXPORT CTFontDescriptorRef CTFontDescriptorCreateCopyWithAttributes(CTFontDescriptorRef original, CFDictionaryRef attributes) CF_RETURNS_RETAINED;
CF_EXPORT CTFontDescriptorRef _Nullable CTFontDescriptorCreateCopyWithSymbolicTraits(CTFontDescriptorRef original, CTFontSymbolicTraits symTraitValue,
                                                                                    CTFontSymbolicTraits symTraitMask) CF_RETURNS_RETAINED;
CF_EXPORT CTFontDescriptorRef CTFontDescriptorCreateCopyWithFeature(CTFontDescriptorRef original, CFNumberRef featureTypeIdentifier,
                                                                    CFNumberRef featureSelectorIdentifier) CF_RETURNS_RETAINED;
CF_EXPORT CTFontDescriptorRef CTFontDescriptorCreateCopyWithVariation(CTFontDescriptorRef original, CFNumberRef variationIdentifier,
                                                                      CGFloat variationValue) CF_RETURNS_RETAINED;
CF_EXPORT CTFontDescriptorRef CTFontDescriptorCreateWithAttributesAndOptions(CFDictionaryRef attributes, CTFontDescriptorOptions options) CF_RETURNS_RETAINED;
CF_EXPORT CFDictionaryRef CTFontDescriptorCopyAttributes(CTFontDescriptorRef descriptor) CF_RETURNS_RETAINED;
CF_EXPORT CFTypeRef _Nullable CTFontDescriptorCopyAttribute(CTFontDescriptorRef descriptor, CFStringRef attribute) CF_RETURNS_RETAINED;
CF_EXPORT bool CTFontDescriptorIsSystemUIFont(CTFontDescriptorRef descriptor);

/* fonte */
CF_EXPORT CTFontRef CTFontCreateWithFontDescriptor(CTFontDescriptorRef descriptor, CGFloat size, const CGAffineTransform * _Nullable matrix) CF_RETURNS_RETAINED;
CF_EXPORT CTFontRef CTFontCreateWithName(CFStringRef name, CGFloat size, const CGAffineTransform * _Nullable matrix) CF_RETURNS_RETAINED;
CF_EXPORT CTFontRef _Nullable CTFontCreateUIFontForLanguage(CTFontUIFontType uiType, CGFloat size, CFStringRef _Nullable language) CF_RETURNS_RETAINED;
CF_EXPORT CTFontRef CTFontCreateCopyWithSymbolicTraits(CTFontRef font, CGFloat size, const CGAffineTransform * _Nullable matrix,
                                                       CTFontSymbolicTraits symTraitValue, CTFontSymbolicTraits symTraitMask) CF_RETURNS_RETAINED;
CF_EXPORT CTFontDescriptorRef CTFontCopyFontDescriptor(CTFontRef font) CF_RETURNS_RETAINED;
CF_EXPORT CFTypeRef _Nullable CTFontCopyAttribute(CTFontRef font, CFStringRef attribute) CF_RETURNS_RETAINED;
CF_EXPORT CFStringRef CTFontCopyPostScriptName(CTFontRef font) CF_RETURNS_RETAINED;
CF_EXPORT CFStringRef CTFontCopyFamilyName(CTFontRef font) CF_RETURNS_RETAINED;
CF_EXPORT CGFloat CTFontGetSize(CTFontRef font);
CF_EXPORT CTFontSymbolicTraits CTFontGetSymbolicTraits(CTFontRef font);
CF_EXPORT CGFloat CTFontGetAscent(CTFontRef font);
CF_EXPORT CGFloat CTFontGetDescent(CTFontRef font);
CF_EXPORT CGFloat CTFontGetLeading(CTFontRef font);
CF_EXPORT CGFloat CTFontGetCapHeight(CTFontRef font);
CF_EXPORT CGFloat CTFontGetXHeight(CTFontRef font);
CF_EXPORT CGFloat CTFontGetUnderlinePosition(CTFontRef font);
CF_EXPORT CGFloat CTFontGetUnderlineThickness(CTFontRef font);
CF_EXPORT CGFloat CTFontGetSlantAngle(CTFontRef font);
CF_EXPORT unsigned CTFontGetUnitsPerEm(CTFontRef font);
CF_EXPORT CGRect CTFontGetBoundingBox(CTFontRef font);

/* SPI de parágrafo (o OpenSwiftUI chama direto) */
CF_EXPORT CTCompositionLanguage CTParagraphStyleGetCompositionLanguageForLanguage(CFStringRef _Nullable language);

/* chaves de atributo e de traço */
CF_EXPORT const CFStringRef kCTFontNameAttribute;
CF_EXPORT const CFStringRef kCTFontDisplayNameAttribute;
CF_EXPORT const CFStringRef kCTFontFamilyNameAttribute;
CF_EXPORT const CFStringRef kCTFontStyleNameAttribute;
CF_EXPORT const CFStringRef kCTFontSizeAttribute;
CF_EXPORT const CFStringRef kCTFontMatrixAttribute;
CF_EXPORT const CFStringRef kCTFontTraitsAttribute;
CF_EXPORT const CFStringRef kCTFontSymbolicTrait;
CF_EXPORT const CFStringRef kCTFontWeightTrait;
CF_EXPORT const CFStringRef kCTFontWidthTrait;
CF_EXPORT const CFStringRef kCTFontSlantTrait;
CF_EXPORT const CFStringRef kCTFontFeatureSettingsAttribute;
CF_EXPORT const CFStringRef kCTFontVariationAttribute;
CF_EXPORT const CFStringRef kCTFontOpticalSizeAttribute;
CF_EXPORT const CFStringRef kCTFontAttributeName;
CF_EXPORT const CFStringRef kCTForegroundColorAttributeName;

/* pesos (CGFloat) */
CF_EXPORT const CGFloat kCTFontWeightUltraLight;
CF_EXPORT const CGFloat kCTFontWeightThin;
CF_EXPORT const CGFloat kCTFontWeightLight;
CF_EXPORT const CGFloat kCTFontWeightRegular;
CF_EXPORT const CGFloat kCTFontWeightMedium;
CF_EXPORT const CGFloat kCTFontWeightSemibold;
CF_EXPORT const CGFloat kCTFontWeightBold;
CF_EXPORT const CGFloat kCTFontWeightHeavy;
CF_EXPORT const CGFloat kCTFontWeightBlack;

CF_ASSUME_NONNULL_END
CF_EXTERN_C_END
#endif
