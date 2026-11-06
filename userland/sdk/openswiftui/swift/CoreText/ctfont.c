/* mOS SDK — tipos CF do CoreText (CTFont, CTFontDescriptor, CTGlyphInfo, CTAdaptiveImageGlyph) e as
 * constantes kCT*. Vai no libCoreText.a, na imagem do app, junto com o libMOSCFRuntime.a (cf_mos.cpp), que
 * cria os objetos no formato Swift (isa + contador do Swift + _cfinfoa = 24 bytes de cabeçalho).
 *
 * As constantes CFString são CFSTR (o cf_mos aceita o formato das constantes do clang). O OpenSwiftUI lê as
 * privadas por `@_silgen_name("kCT...") let x: CFString`, um acesso direto à variável global com esse nome. */
#include "include/MOSCoreTextC.h"

/* CFRetain/CFRelease do libSystem mexem no contador ObjC, não no do Swift: com objetos do cf_mos (objetos
 * Swift) o retain se perde e o objeto é liberado ainda em uso. Use os do cf_mos, que escolhem pelo isa. */
extern CFTypeRef mos_oag_CFRetain(CFTypeRef cf);
extern void mos_oag_CFRelease(CFTypeRef cf);
#define CFRetain mos_oag_CFRetain
#define CFRelease mos_oag_CFRelease

/* Constantes CFString no formato do CFSTR do swift-corelibs (DEPLOYMENT_RUNTIME_SWIFT): objeto Swift imortal
 * {isa = SwiftFoundation._NSCFConstantString, contador imortal, cfinfo, ptr, len}, o mesmo layout do CFString
 * do cf_mos. NÃO use o CFSTR estilo Apple (isa = __CFConstantStringClassReference, uma classe ObjC do
 * libSystem): guardado num Any, o Swift consulta a metadata do isa e derruba o app. O contador vai como
 * literal (0x80000004ffffffff = bits imortais do runtime), sem símbolo absoluto, então não gera fixup. */
extern char mos_ct_cfstr_isa[] __asm__("_$s15SwiftFoundation19_NSCFConstantStringCN");
typedef struct { const void *isa; uint64_t rc; uint64_t cfinfo; const char *ptr; CFIndex len; } MOSConstString;
#define S(x) ((CFStringRef)&(const MOSConstString){ mos_ct_cfstr_isa, 0x80000004ffffffffull, 0x7c8, x, sizeof(x) - 1 })

/* ---- CFRuntime (mesmo layout de cf_mos.cpp / CFRuntime.h do swift-corelibs com DEPLOYMENT_RUNTIME_SWIFT,
 * que o CFAvailability.h de lá liga: isa + contador do Swift + _cfinfoa = 24 bytes) */
typedef struct { uintptr_t isa; uintptr_t rc; uint64_t cfinfo; } MOSCFBase;
typedef struct {
    CFIndex version;
    const char *className;
    void (*init)(CFTypeRef cf);
    CFTypeRef (*copy)(CFAllocatorRef allocator, CFTypeRef cf);
    void (*finalize)(CFTypeRef cf);
    Boolean (*equal)(CFTypeRef cf1, CFTypeRef cf2);
    CFHashCode (*hash)(CFTypeRef cf);
    CFStringRef (*copyFormattingDesc)(CFTypeRef cf, CFDictionaryRef formatOptions);
    CFStringRef (*copyDebugDesc)(CFTypeRef cf);
    void (*reclaim)(CFTypeRef cf);
    uint32_t (*refcount)(intptr_t op, CFTypeRef cf);
    uintptr_t requiredAlignment;
} MOSCFRuntimeClass;
extern CFTypeID _CFRuntimeRegisterClass(const MOSCFRuntimeClass *cls);
extern CFTypeRef _CFRuntimeCreateInstance(CFAllocatorRef allocator, CFTypeID typeID, CFIndex extraBytes, unsigned char *category);

typedef struct { MOSCFBase base; CFDictionaryRef attrs; } CTDescObj;
typedef struct { MOSCFBase base; CTFontDescriptorRef desc; double size; } CTFontObj;
typedef struct { MOSCFBase base; } CTPlainObj;

static void desc_finalize(CFTypeRef cf) { if (((CTDescObj *)cf)->attrs) CFRelease(((CTDescObj *)cf)->attrs); }
static Boolean desc_equal(CFTypeRef a, CFTypeRef b) { return CFEqual(((const CTDescObj *)a)->attrs, ((const CTDescObj *)b)->attrs); }
static CFHashCode desc_hash(CFTypeRef cf) { return CFDictionaryGetCount(((const CTDescObj *)cf)->attrs); }
static void font_finalize(CFTypeRef cf) { if (((CTFontObj *)cf)->desc) CFRelease(((CTFontObj *)cf)->desc); }
static Boolean font_equal(CFTypeRef a, CFTypeRef b) {
    const CTFontObj *x = a, *y = b;
    return x->size == y->size && CFEqual(x->desc, y->desc);
}
static CFHashCode font_hash(CFTypeRef cf) { return (CFHashCode)((const CTFontObj *)cf)->size; }

static const MOSCFRuntimeClass desc_class = { 0, "CTFontDescriptor", 0, 0, desc_finalize, desc_equal, desc_hash, 0, 0, 0, 0, 0 };
static const MOSCFRuntimeClass font_class = { 0, "CTFont", 0, 0, font_finalize, font_equal, font_hash, 0, 0, 0, 0, 0 };
static const MOSCFRuntimeClass glyph_class = { 0, "CTGlyphInfo", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static const MOSCFRuntimeClass adaptive_class = { 0, "CTAdaptiveImageGlyph", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static CFTypeID desc_tid, font_tid, glyph_tid, adaptive_tid;
static CFTypeID reg(CFTypeID *id, const MOSCFRuntimeClass *c) {
    if (!*id) *id = _CFRuntimeRegisterClass(c);
    return *id;
}

CFTypeID CTFontDescriptorGetTypeID(void) { return reg(&desc_tid, &desc_class); }
CFTypeID CTFontGetTypeID(void) { return reg(&font_tid, &font_class); }
CFTypeID CTGlyphInfoGetTypeID(void) { return reg(&glyph_tid, &glyph_class); }
CFTypeID CTAdaptiveImageGlyphGetTypeID(void) { return reg(&adaptive_tid, &adaptive_class); }

/* ---- descritor */
CTFontDescriptorRef CTFontDescriptorCreateWithAttributes(CFDictionaryRef attributes) {
    CTDescObj *d = (CTDescObj *)_CFRuntimeCreateInstance(0, CTFontDescriptorGetTypeID(),
                                                         sizeof(CTDescObj) - sizeof(MOSCFBase), 0);
    if (!d) return 0;
    /* cópia própria: o chamador pode continuar mexendo no dicionário dele */
    d->attrs = attributes ? (CFDictionaryRef)CFDictionaryCreateMutableCopy(0, 0, attributes)
                          : CFDictionaryCreate(0, 0, 0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    return (CTFontDescriptorRef)d;
}
CFDictionaryRef CTFontDescriptorCopyAttributes(CTFontDescriptorRef descriptor) {
    return (CFDictionaryRef)CFRetain(((const CTDescObj *)descriptor)->attrs);
}
CFTypeRef CTFontDescriptorCopyAttribute(CTFontDescriptorRef descriptor, CFStringRef attribute) {
    const void *v = CFDictionaryGetValue(((const CTDescObj *)descriptor)->attrs, attribute);
    return v ? CFRetain(v) : 0;
}

/* ---- fonte */
CTFontRef _MOSCTFontCreate(CTFontDescriptorRef descriptor, double size) {
    CTFontObj *f = (CTFontObj *)_CFRuntimeCreateInstance(0, CTFontGetTypeID(), sizeof(CTFontObj) - sizeof(MOSCFBase), 0);
    if (!f) return 0;
    f->desc = (CTFontDescriptorRef)CFRetain(descriptor);
    f->size = size;
    return (CTFontRef)f;
}
double _MOSCTFontGetSize(CTFontRef font) { return ((const CTFontObj *)font)->size; }
CTFontDescriptorRef CTFontCopyFontDescriptor(CTFontRef font) { return (CTFontDescriptorRef)CFRetain(((const CTFontObj *)font)->desc); }
CFTypeRef CTFontCopyAttribute(CTFontRef font, CFStringRef attribute) {
    return CTFontDescriptorCopyAttribute(((const CTFontObj *)font)->desc, attribute);
}

/* ---- constantes públicas */
const CFStringRef kCTFontNameAttribute = S("NSFontNameAttribute");
const CFStringRef kCTFontFamilyNameAttribute = S("NSFontFamilyAttribute");
const CFStringRef kCTFontSizeAttribute = S("NSFontSizeAttribute");
const CFStringRef kCTFontTraitsAttribute = S("NSCTFontTraitsAttribute");
const CFStringRef kCTFontSymbolicTrait = S("NSCTFontSymbolicTrait");
const CFStringRef kCTFontWeightTrait = S("NSCTFontWeightTrait");
const CFStringRef kCTFontSlantTrait = S("NSCTFontSlantTrait");
const CFStringRef kCTFontFeatureSettingsAttribute = S("NSCTFontFeatureSettingsAttribute");
const CFStringRef kCTFontVariationAttribute = S("NSCTFontVariationAttribute");
const CFStringRef kCTFontOpticalSizeAttribute = S("NSCTFontOpticalSizeAttribute");
const CFStringRef kCTFontAttributeName = S("NSFont");
const CFStringRef kCTForegroundColorAttributeName = S("CTForegroundColor");

/* ---- constantes privadas (lidas pelo OpenSwiftUI por @_silgen_name; ver CoreText+Private.swift) */
const CFStringRef kCTFontUIFontDesignTrait = S("NSCTFontUIFontDesignTrait");
const CFStringRef kCTFontGradeTrait = S("NSCTFontGradeTrait");
const CFStringRef kCTFontLegibilityWeightAttribute = S("NSCTFontLegibilityWeightAttribute");
const CFStringRef kCTFontDescriptorLanguageAttribute = S("NSCTFontDescriptorLanguageAttribute");
const CFStringRef kCTFontLanguageAwareLineHeightRatioAttribute = S("NSCTFontLanguageAwareLineHeightRatioAttribute");
const CFStringRef kCTFontWidthTrait = S("NSCTFontWidthTrait");
const CFStringRef kCTTextScaleRatioAttributeName = S("CTTextScaleRatio");

const CFStringRef kCTUIFontTextStyleTitle0 = S("UICTFontTextStyleTitle0");
const CFStringRef kCTUIFontTextStyleTitle1 = S("UICTFontTextStyleTitle1");
const CFStringRef kCTUIFontTextStyleTitle2 = S("UICTFontTextStyleTitle2");
const CFStringRef kCTUIFontTextStyleTitle3 = S("UICTFontTextStyleTitle3");
const CFStringRef kCTUIFontTextStyleHeadline = S("UICTFontTextStyleHeadline");
const CFStringRef kCTUIFontTextStyleSubhead = S("UICTFontTextStyleSubhead");
const CFStringRef kCTUIFontTextStyleBody = S("UICTFontTextStyleBody");
const CFStringRef kCTUIFontTextStyleCallout = S("UICTFontTextStyleCallout");
const CFStringRef kCTUIFontTextStyleFootnote = S("UICTFontTextStyleFootnote");
const CFStringRef kCTUIFontTextStyleCaption1 = S("UICTFontTextStyleCaption1");
const CFStringRef kCTUIFontTextStyleCaption2 = S("UICTFontTextStyleCaption2");
const CFStringRef kCTUIFontTextStyleExtraLargeTitle = S("UICTFontTextStyleExtraLargeTitle");
const CFStringRef kCTUIFontTextStyleExtraLargeTitle2 = S("UICTFontTextStyleExtraLargeTitle2");
const CFStringRef kCTUIFontTextStyleCaption3 = S("UICTFontTextStyleCaption3");
const CFStringRef kCTUIFontTextStyleFootnote2 = S("UICTFontTextStyleFootnote2");

const CFStringRef kCTFontUIFontDesignDefault = S("CTFontUIFontDesignDefault");
const CFStringRef kCTFontUIFontDesignSerif = S("CTFontUIFontDesignSerif");
const CFStringRef kCTFontUIFontDesignRounded = S("CTFontUIFontDesignRounded");
const CFStringRef kCTFontUIFontDesignMonospaced = S("CTFontUIFontDesignMonospaced");
const CFStringRef kCTFontUIFontDesignCompact = S("CTFontUIFontDesignCompact");
const CFStringRef kCTFontUIFontDesignCompactRounded = S("CTFontUIFontDesignCompactRounded");
const CFStringRef kCTFontUIFontDesignSoft = S("CTFontUIFontDesignSoft");
const CFStringRef kCTFontUIFontDesignCompactSoft = S("CTFontUIFontDesignCompactSoft");

const CFStringRef kCTFontContentSizeCategoryXS = S("UICTContentSizeCategoryXS");
const CFStringRef kCTFontContentSizeCategoryS = S("UICTContentSizeCategoryS");
const CFStringRef kCTFontContentSizeCategoryM = S("UICTContentSizeCategoryM");
const CFStringRef kCTFontContentSizeCategoryL = S("UICTContentSizeCategoryL");
const CFStringRef kCTFontContentSizeCategoryXL = S("UICTContentSizeCategoryXL");
const CFStringRef kCTFontContentSizeCategoryXXL = S("UICTContentSizeCategoryXXL");
const CFStringRef kCTFontContentSizeCategoryXXXL = S("UICTContentSizeCategoryXXXL");
const CFStringRef kCTFontContentSizeCategoryAccessibilityM = S("UICTContentSizeCategoryAccessibilityM");
const CFStringRef kCTFontContentSizeCategoryAccessibilityL = S("UICTContentSizeCategoryAccessibilityL");
const CFStringRef kCTFontContentSizeCategoryAccessibilityXL = S("UICTContentSizeCategoryAccessibilityXL");
const CFStringRef kCTFontContentSizeCategoryAccessibilityXXL = S("UICTContentSizeCategoryAccessibilityXXL");
const CFStringRef kCTFontContentSizeCategoryAccessibilityXXXL = S("UICTContentSizeCategoryAccessibilityXXXL");

/* pesos e larguras (valores do macOS; o Swift os lê como CGFloat) */
const double kCTFontWeightUltraLight = -0.8, kCTFontWeightThin = -0.6, kCTFontWeightLight = -0.4,
             kCTFontWeightRegular = 0.0, kCTFontWeightMedium = 0.23, kCTFontWeightSemibold = 0.3,
             kCTFontWeightBold = 0.4, kCTFontWeightHeavy = 0.56, kCTFontWeightBlack = 0.62;
const double kCTFontWidthCompressed = -0.3, kCTFontWidthCondensed = -0.2, kCTFontWidthStandard = 0.0,
             kCTFontWidthExpanded = 0.2;

/* NSTextEncapsulationAttributeName é um NSAttributedString.Key (struct Swift do Foundation do mOS): o espaço
 * fica reservado aqui (até 32 bytes) e é preenchido quando o tipo existir no Foundation. */
_Alignas(16) unsigned char NSTextEncapsulationAttributeName[32];
