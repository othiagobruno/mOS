/*
 * mOS libSystem — a parte do Core Text que o SwiftUI usa para estilos de texto, tipos de fonte da interface e
 * métricas privadas: as constantes kCTUIFontTextStyle*, kCTFontContentSizeCategory*, kCTFontUIFontDesign*,
 * kCTFontWeight* e kCTFontWidth*, e as funções CTFontDescriptorCreateWithTextStyleAndAttributes,
 * CTFontDescriptorGetTextStyleSize, CTFontDescriptorCreateForUIType, CTFontDescriptorGetWeight/SymbolicTraits,
 * CTFontGetWeight, CTFontIsSystemUIFont, CTFontGetAccessibilityBoldWeightOfWeight, CTFontGetClippingMetrics,
 * CTFontGetLanguageAwareOutsets, CTFontCopySystemUIFontExcessiveLineHeightCharacterSet,
 * CTFontCopyTallestTextStyleLanguageForString, CTParagraphStyleGetCompositionLanguageForLanguage e
 * CTClearFontFallbacksCache. Tudo sobre a API pública de coretext.c (os descritores são dicionários).
 * Tamanhos e pesos dos estilos são os do macOS (Body 13 pt, Headline 13 pt negrito, Large Title 26 pt...).
 */
#include "foundation.h"
#include "objc.h"

id objc_retain(id o);
void objc_release(id o);
id cx_num_d(double v);
id cx_num_i(i64 v);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
static id cls(const char *n) { return (id)cx_class_named(n); }

extern const void *kCTFontNameAttribute, *kCTFontSizeAttribute, *kCTFontTraitsAttribute, *kCTFontWeightTrait,
    *kCTFontSymbolicTrait;
id CTFontDescriptorCreateWithAttributes(id attrs);
id CTFontCopyFontDescriptor(id f);
BOOL CTFontDescriptorIsSystemUIFont(id d);
double CTFontGetAscent(id f);
double CTFontGetDescent(id f);

/* ---- constantes (CFString constante no mesmo formato de coretext.c) */
extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } ts_const_t;
#define TSCONST(var, text) \
    static ts_const_t var##_s = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; \
    const void *var = &var##_s;

TSCONST(kCTUIFontTextStyleTitle0, "UICTFontTextStyleTitle0")
TSCONST(kCTUIFontTextStyleTitle1, "UICTFontTextStyleTitle1")
TSCONST(kCTUIFontTextStyleTitle2, "UICTFontTextStyleTitle2")
TSCONST(kCTUIFontTextStyleTitle3, "UICTFontTextStyleTitle3")
TSCONST(kCTUIFontTextStyleHeadline, "UICTFontTextStyleHeadline")
TSCONST(kCTUIFontTextStyleSubhead, "UICTFontTextStyleSubhead")
TSCONST(kCTUIFontTextStyleBody, "UICTFontTextStyleBody")
TSCONST(kCTUIFontTextStyleCallout, "UICTFontTextStyleCallout")
TSCONST(kCTUIFontTextStyleFootnote, "UICTFontTextStyleFootnote")
TSCONST(kCTUIFontTextStyleFootnote2, "UICTFontTextStyleFootnote2")
TSCONST(kCTUIFontTextStyleCaption1, "UICTFontTextStyleCaption1")
TSCONST(kCTUIFontTextStyleCaption2, "UICTFontTextStyleCaption2")
TSCONST(kCTUIFontTextStyleCaption3, "UICTFontTextStyleCaption3")
TSCONST(kCTUIFontTextStyleExtraLargeTitle, "UICTFontTextStyleExtraLargeTitle")
TSCONST(kCTUIFontTextStyleExtraLargeTitle2, "UICTFontTextStyleExtraLargeTitle2")

TSCONST(kCTFontContentSizeCategory, "UICTContentSizeCategory")
TSCONST(kCTFontContentSizeCategoryXS, "UICTContentSizeCategoryXS")
TSCONST(kCTFontContentSizeCategoryS, "UICTContentSizeCategoryS")
TSCONST(kCTFontContentSizeCategoryM, "UICTContentSizeCategoryM")
TSCONST(kCTFontContentSizeCategoryL, "UICTContentSizeCategoryL")
TSCONST(kCTFontContentSizeCategoryXL, "UICTContentSizeCategoryXL")
TSCONST(kCTFontContentSizeCategoryXXL, "UICTContentSizeCategoryXXL")
TSCONST(kCTFontContentSizeCategoryXXXL, "UICTContentSizeCategoryXXXL")
TSCONST(kCTFontContentSizeCategoryAccessibilityM, "UICTContentSizeCategoryAccessibilityM")
TSCONST(kCTFontContentSizeCategoryAccessibilityL, "UICTContentSizeCategoryAccessibilityL")
TSCONST(kCTFontContentSizeCategoryAccessibilityXL, "UICTContentSizeCategoryAccessibilityXL")
TSCONST(kCTFontContentSizeCategoryAccessibilityXXL, "UICTContentSizeCategoryAccessibilityXXL")
TSCONST(kCTFontContentSizeCategoryAccessibilityXXXL, "UICTContentSizeCategoryAccessibilityXXXL")

TSCONST(kCTFontUIFontDesignTrait, "NSCTFontUIFontDesignTrait")
TSCONST(kCTFontUIFontDesignDefault, "NSCTFontUIFontDesignDefault")
TSCONST(kCTFontUIFontDesignMonospaced, "NSCTFontUIFontDesignMonospaced")
TSCONST(kCTFontUIFontDesignRounded, "NSCTFontUIFontDesignRounded")
TSCONST(kCTFontUIFontDesignSerif, "NSCTFontUIFontDesignSerif")
TSCONST(kCTFontUIFontDesignSoft, "NSCTFontUIFontDesignSoft")
TSCONST(kCTFontUIFontDesignCompact, "NSCTFontUIFontDesignCompact")
TSCONST(kCTFontUIFontDesignCompactRounded, "NSCTFontUIFontDesignCompactRounded")
TSCONST(kCTFontUIFontDesignCompactSoft, "NSCTFontUIFontDesignCompactSoft")

TSCONST(kCTFontGradeTrait, "NSCTFontGradeTrait")
TSCONST(kCTFontDescriptorLanguageAttribute, "NSCTFontDescriptorLanguageAttribute")
TSCONST(kCTFontLegibilityWeightAttribute, "NSCTFontLegibilityWeightAttribute")
TSCONST(kCTFontLanguageAwareLineHeightRatioAttribute, "NSCTFontLanguageAwareLineHeightRatioAttribute")
TSCONST(kCTTextScaleRatioAttributeName, "CTTextScaleRatio")
TSCONST(kCTFontTextStyleAttribute, "NSCTFontUIUsageAttribute")

/* pesos e larguras (os mesmos valores de NSFontWeight/NSFontWidth do AppKit) */
const double kCTFontWeightUltraLight = -0.8, kCTFontWeightThin = -0.6, kCTFontWeightLight = -0.4, kCTFontWeightRegular = 0.0,
             kCTFontWeightMedium = 0.23, kCTFontWeightSemibold = 0.3, kCTFontWeightBold = 0.4, kCTFontWeightHeavy = 0.56,
             kCTFontWeightBlack = 0.62;
const double kCTFontWidthCompressed = -0.3, kCTFontWidthCondensed = -0.2, kCTFontWidthStandard = 0.0, kCTFontWidthExpanded = 0.2;

/* ---- estilos de texto do macOS: tamanho em pontos e peso */
typedef struct { const char *suffix; double size, weight; } style_t;
static const style_t styles[] = {
    {"ExtraLargeTitle2", 28, 0.4}, {"ExtraLargeTitle", 36, 0.4},
    {"Title0", 26, 0}, {"Title1", 22, 0}, {"Title2", 17, 0}, {"Title3", 15, 0},
    {"Headline", 13, 0.4}, {"Subhead", 11, 0}, {"Body", 13, 0}, {"Callout", 12, 0},
    {"Footnote2", 10, 0}, {"Footnote", 10, 0}, {"Caption1", 10, 0}, {"Caption2", 10, 0}, {"Caption3", 10, 0},
};

static int ends_with(const char *s, const char *suf) {
    u64 a = 0, b = 0;
    while (s[a]) a++;
    while (suf[b]) b++;
    return a >= b && strcmp(s + a - b, suf) == 0;
}

static const style_t *style_of(id name) {
    const char *s = name ? cx_cstr(name) : 0;
    if (s)
        for (u32 i = 0; i < sizeof styles / sizeof styles[0]; i++)
            if (ends_with(s, styles[i].suffix)) return &styles[i];
    return &styles[8];   /* Body */
}

/* o macOS não aplica o Dynamic Type; só as categorias de acessibilidade aumentam o texto */
static double category_scale(id cat) {
    const char *c = cat ? cx_cstr(cat) : 0;
    if (!c) return 1;
    if (ends_with(c, "AccessibilityXXXL")) return 2.0;
    if (ends_with(c, "AccessibilityXXL")) return 1.8;
    if (ends_with(c, "AccessibilityXL")) return 1.6;
    if (ends_with(c, "AccessibilityL")) return 1.45;
    if (ends_with(c, "AccessibilityM")) return 1.3;
    return 1;
}

static id number(id d, const void *key) { return d ? MSG(id, d, "objectForKey:", (id)key) : 0; }
static double num_d(id n, double def) { return n ? MSG0(double, n, "doubleValue") : def; }

/* descritor da fonte do sistema com tamanho, peso e os atributos extras (que vencem) */
static id system_descriptor(double size, double weight, id extra, id style) {
    id d = MSG0(id, cls("NSMutableDictionary"), "dictionary");
    MSG(void, d, "setObject:forKey:", cx_str(".AppleSystemUIFont"), (id)kCTFontNameAttribute);
    MSG(void, d, "setObject:forKey:", cx_num_d(size), (id)kCTFontSizeAttribute);
    id traits = MSG0(id, cls("NSMutableDictionary"), "dictionary");
    MSG(void, traits, "setObject:forKey:", cx_num_d(weight), (id)kCTFontWeightTrait);
    MSG(void, traits, "setObject:forKey:", cx_num_i(weight >= 0.3 ? 2 : 0), (id)kCTFontSymbolicTrait);   /* kCTFontBoldTrait */
    MSG(void, d, "setObject:forKey:", traits, (id)kCTFontTraitsAttribute);
    if (style) MSG(void, d, "setObject:forKey:", style, (id)kCTFontTextStyleAttribute);
    if (extra) {
        id keys = MSG0(id, extra, "allKeys");
        long n = MSG0(long, keys, "count");
        for (long i = 0; i < n; i++) {
            id k = MSG(id, keys, "objectAtIndex:", i);
            id v = MSG(id, extra, "objectForKey:", k);
            /* traços do chamador entram por cima dos do estilo, chave a chave */
            if (k == (id)kCTFontTraitsAttribute && v) {
                id tk = MSG0(id, v, "allKeys");
                long tn = MSG0(long, tk, "count");
                for (long j = 0; j < tn; j++) {
                    id kk = MSG(id, tk, "objectAtIndex:", j);
                    id vv = MSG(id, v, "objectForKey:", kk);
                    MSG(void, traits, "setObject:forKey:", vv, kk);
                }
            } else if (v) {
                MSG(void, d, "setObject:forKey:", v, k);
            }
        }
    }
    return CTFontDescriptorCreateWithAttributes(d);
}

id CTFontDescriptorCreateWithTextStyleAndAttributes(id style, id category, id attributes) {
    const style_t *s = style_of(style);
    return system_descriptor(s->size * category_scale(category), s->weight, attributes, style);
}

double CTFontDescriptorGetTextStyleSize(id style, id category, int platform, const double *weight, const double *size) {
    (void)platform; (void)weight; (void)size;
    return style_of(style)->size * category_scale(category);
}

/* CTFontUIFontType: os tipos "emphasized" são negrito; os "small"/"mini" têm tamanho próprio quando size <= 0 */
id CTFontDescriptorCreateForUIType(u32 type, double size, id language) {
    (void)language;
    double def = 13;
    switch (type) {
    case 4: case 5: def = 11; break;            /* smallSystem, smallEmphasizedSystem */
    case 6: case 7: def = 9; break;             /* mini */
    case 19: case 20: def = 12; break;          /* systemDetail */
    case 10: case 12: case 26: def = 13; break; /* label, menuItem, controlContent */
    case 25: def = 11; break;                   /* toolTip */
    default: break;
    }
    int bold = type == 3 || type == 5 || type == 7 || type == 20 || type == 18;
    return system_descriptor(size > 0 ? size : def, bold ? 0.4 : 0, 0, 0);
}

double CTFontDescriptorGetWeight(id d) { return num_d(number(number(d, kCTFontTraitsAttribute), kCTFontWeightTrait), 0); }
u32 CTFontDescriptorGetSymbolicTraits(id d) {
    id n = number(number(d, kCTFontTraitsAttribute), kCTFontSymbolicTrait);
    return n ? (u32)MSG0(long, n, "longValue") : 0;
}

double CTFontGetWeight(id f) {
    id d = f ? CTFontCopyFontDescriptor(f) : 0;
    double w = CTFontDescriptorGetWeight(d);
    if (d) objc_release(d);
    return w;
}

BOOL CTFontIsSystemUIFont(id f) {
    id d = f ? CTFontCopyFontDescriptor(f) : 0;
    BOOL r = d && CTFontDescriptorIsSystemUIFont(d);
    if (d) objc_release(d);
    return r;
}

/* Negrito de acessibilidade: o próximo peso mais pesado da escala do sistema */
double CTFontGetAccessibilityBoldWeightOfWeight(double w) {
    static const double scale[] = {-0.8, -0.6, -0.4, 0.0, 0.23, 0.3, 0.4, 0.56, 0.62};
    for (u32 i = 0; i < sizeof scale / sizeof scale[0]; i++)
        if (scale[i] > w + 0.001) return i + 1 < sizeof scale / sizeof scale[0] ? scale[i + 1] : scale[i];
    return w;
}

/* As fontes do mOS não têm métricas de recorte próprias: as de recorte são as de desenho (ascent/descent) */
BOOL CTFontGetClippingMetrics(id f, double *ascent, double *descent) {
    if (!f) return NO;
    if (ascent) *ascent = CTFontGetAscent(f);
    if (descent) *descent = CTFontGetDescent(f);
    return YES;
}

/* Sem fonte com acréscimos por idioma: os quatro lados são zero */
BOOL CTFontGetLanguageAwareOutsets(id f, double *l, double *t, double *r, double *b) {
    (void)f;
    if (l) *l = 0;
    if (t) *t = 0;
    if (r) *r = 0;
    if (b) *b = 0;
    return NO;
}

/* A fonte do sistema do mOS (Inter) não tem caracteres com altura de linha excessiva */
id CTFontCopySystemUIFontExcessiveLineHeightCharacterSet(void) { return 0; }

/* Idioma de escrita mais alta presente na string (scripts com acentos acima/abaixo que precisam de mais linha) */
id CTFontCopyTallestTextStyleLanguageForString(id str) {
    if (!str) return 0;
    long n = MSG0(long, str, "length");
    for (long i = 0; i < n; i++) {
        u32 c = MSG(u16, str, "characterAtIndex:", i);
        const char *lang = 0;
        if (c >= 0x0E00 && c <= 0x0E7F) lang = "th";
        else if (c >= 0x0E80 && c <= 0x0EFF) lang = "lo";
        else if (c >= 0x0F00 && c <= 0x0FFF) lang = "bo";
        else if (c >= 0x1000 && c <= 0x109F) lang = "my";
        else if (c >= 0x0900 && c <= 0x097F) lang = "hi";
        else if (c >= 0x0980 && c <= 0x09FF) lang = "bn";
        else if (c >= 0x0600 && c <= 0x06FF) lang = "ar";
        else if (c >= 0x0590 && c <= 0x05FF) lang = "he";
        if (lang) return objc_retain(cx_str(lang));
    }
    return 0;
}

/* CTCompositionLanguage: 0 sem idioma, 1 nenhum, 2 japonês, 3 chinês simplificado, 4 chinês tradicional */
int CTParagraphStyleGetCompositionLanguageForLanguage(id language) {
    const char *l = language ? cx_cstr(language) : 0;
    if (!l) return 0;
    if (!strncmp(l, "ja", 2)) return 2;
    if (!strncmp(l, "zh-Hant", 7) || !strncmp(l, "zh-TW", 5) || !strncmp(l, "zh-HK", 5) || !strncmp(l, "zh_TW", 5)) return 4;
    if (!strncmp(l, "zh", 2)) return 3;
    return 1;
}

/* O mOS não guarda cache de fontes de fallback (o fallback é decidido a cada glifo): nada a limpar */
void CTClearFontFallbacksCache(void) {}
