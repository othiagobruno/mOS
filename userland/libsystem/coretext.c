/*
 * mOS libSystem — CoreText para o porte do VS Code (docs/porte-vscode.md): o que o Electron importa de
 * CoreText (73 símbolos, medidos com tools/machocheck.c). No macOS CTFont e NSFont são o mesmo objeto;
 * aqui também: as funções CTFont* recebem e devolvem as fontes do appkit.c (NSFont), e lêem tamanho e
 * nome por mensagem (-pointSize, -fontName). As métricas são proporcionais ao tamanho, com os valores
 * típicos da fonte do sistema (SF: ascent 0,952 em, descent 0,241 em...). Descritores de fonte são
 * dicionários de atributos (como o macOS os expõe por CTFontDescriptorCopyAttributes).
 * As tabelas sfnt (cmap, glyf, loca, hmtx...) e o raster dos glifos vêm da fonte Inter do kernel
 * (GUI_FONT). CTFontDrawGlyphs pinta num CGBitmapContext. Sem o kernel (harness no Windows) continua
 * o modelo antigo: glifo = código do caractere, sem tabela e sem desenho.
 *
 * Postico 2.3.5 importa oito símbolos de linha (CTLine* e CTTypesetter*). O layout usa as métricas
 * do CTFont. Sem a fonte do kernel o avanço é 0,55 em e CTLineDraw carimba um retângulo no
 * CGBitmapContext; CTFontDrawGlyphs em si não muda e segue sem tinta.
 */
#include "foundation.h"
#include "../../kernel/include/compat_gui.h"

id CFDataCreate(void *alloc, const u8 *bytes, long n);
void *malloc(size_t n);
void free(void *p);
void *memset(void *d, int c, size_t n);
void *memcpy(void *d, const void *s, size_t n);

id objc_retain(id o);
void objc_release(id o);
id cx_num_i(i64 v);
id cx_num_d(double v);
int strncmp(const char *a, const char *b, size_t n);
int strcmp(const char *a, const char *b);
size_t strlen(const char *s);
char *strstr(const char *h, const char *n);

/* ---- constantes CFString (mesmo formato de darwin.c: {isa, flags, ptr, len}) */
extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } ct_const_t;
#define CTCONST(var, text) \
    static ct_const_t var##_s = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; \
    const void *var = &var##_s;
CTCONST(kCTFontCascadeListAttribute, "NSCTFontCascadeListAttribute")
CTCONST(kCTFontCollectionRemoveDuplicatesOption, "NSFontCollectionRemoveDuplicates")
CTCONST(kCTFontDisplayNameAttribute, "NSDisplayName")
CTCONST(kCTFontFamilyNameAttribute, "NSFontFamilyAttribute")
CTCONST(kCTFontFamilyNameKey, "CTFontFamilyName")
CTCONST(kCTFontFormatAttribute, "NSCTFontFormatAttribute")
CTCONST(kCTFontManagerRegisteredFontsChangedNotification, "CTFontManagerFontChangedNotification")
CTCONST(kCTFontNameAttribute, "NSFontNameAttribute")
CTCONST(kCTFontSlantTrait, "NSCTFontSlantTrait")
CTCONST(kCTFontStyleNameAttribute, "NSFontFaceAttribute")
CTCONST(kCTFontSymbolicTrait, "NSCTFontSymbolicTrait")
CTCONST(kCTFontTraitsAttribute, "NSCTFontTraitsAttribute")
CTCONST(kCTFontVariationAttribute, "NSCTFontVariationAttribute")
CTCONST(kCTFontVariationAxesAttribute, "NSCTFontVariationAxesAttribute")
CTCONST(kCTFontVariationAxisDefaultValueKey, "NSCTVariationAxisDefaultValue")
CTCONST(kCTFontVariationAxisIdentifierKey, "NSCTVariationAxisIdentifier")
CTCONST(kCTFontVariationAxisMaximumValueKey, "NSCTVariationAxisMaximumValue")
CTCONST(kCTFontVariationAxisMinimumValueKey, "NSCTVariationAxisMinimumValue")
CTCONST(kCTFontWeightTrait, "NSCTFontWeightTrait")
CTCONST(kCTFontWidthTrait, "NSCTFontWidthTrait")
CTCONST(kCTFontSizeAttribute, "NSFontSizeAttribute")
CTCONST(kCTFontAttributeName, "NSFont")
CTCONST(kCTFontCharacterSetAttribute, "NSCTFontCharacterSetAttribute")
CTCONST(kCTFontFeatureSelectorDefaultKey, "CTFeatureSelectorDefault")
CTCONST(kCTFontFeatureSelectorIdentifierKey, "CTFeatureSelectorIdentifier")
CTCONST(kCTFontFeatureSelectorNameKey, "CTFeatureSelectorName")
CTCONST(kCTFontFeatureSettingsAttribute, "NSCTFontFeatureSettingsAttribute")
CTCONST(kCTFontFeatureTypeExclusiveKey, "CTFeatureTypeExclusive")
CTCONST(kCTFontFeatureTypeIdentifierKey, "CTFeatureTypeIdentifier")
CTCONST(kCTFontFeatureTypeNameKey, "CTFeatureTypeName")
CTCONST(kCTFontFeatureTypeSelectorsKey, "CTFeatureTypeSelectors")
CTCONST(kCTFontFeaturesAttribute, "NSCTFontFeaturesAttribute")
CTCONST(kCTFontFullNameKey, "NSFontFullName")
CTCONST(kCTFontOpenTypeFeatureTag, "CTFontOpenTypeFeatureTag")
CTCONST(kCTFontOpenTypeFeatureValue, "CTFontOpenTypeFeatureValue")
CTCONST(kCTFontOrientationAttribute, "NSCTFontOrientationAttribute")
CTCONST(kCTFontPostScriptNameKey, "NSFontPostScriptName")
CTCONST(kCTFontStyleNameKey, "NSFontStyleName")
CTCONST(kCTFontURLAttribute, "NSCTFontURLAttribute")
CTCONST(kCTForegroundColorAttributeName, "CTForegroundColor")
CTCONST(kCTForegroundColorFromContextAttributeName, "CTForegroundColorFromContext")
CTCONST(kCTKernAttributeName, "kCTKernAttributeName")
CTCONST(kCTLanguageAttributeName, "kCTLanguageAttributeName")
CTCONST(kCTTrackingAttributeName, "CTTracking")
CTCONST(kCTTypesetterOptionForcedEmbeddingLevel, "CTTypesetterOptionForcedEmbeddingLevel")
CTCONST(kCTVerticalFormsAttributeName, "CTVerticalForms")

/* traços simbólicos (CTFontSymbolicTraits) */
#define TRAIT_ITALIC 0x1u
#define TRAIT_BOLD   0x2u
#define TRAIT_MONO   0x400u
#define TRAIT_UI     0x2000u

typedef struct { double x, y; } ct_pt_t;
typedef struct { double w, h; } ct_sz_t;
typedef struct { double x, y, w, h; } ct_rect_t;

static id cls(const char *n) { return (id)cx_class_named(n); }
static double fsize(id f) { if (!f) return 13; double s = MSG0(double, f, "pointSize"); return s > 0 ? s : 13; }

/* NSFont do mOS: {isa, double size, int weight, int mono} em appkit.c. weight é a face do kernel
 * (GUI_FONT): 0..3 = Inter regular, medium, semibold, bold; 32..79 = fonte que o app registrou a partir
 * de dados (GF_REGISTER: as fontes web do Chromium, como os codicons do VS Code). */
typedef struct { double size; int weight; int mono; } ft_t;
static ft_t *ftf(id f) { return (ft_t *)((u8 *)f + 8); }
#define USER_BASE 32
#define USER_MAX 48
#define USER_ID_KEY "mOSFontId"      /* no descritor de uma fonte registrada: o id da face */
static int is_user(int w) { return w >= USER_BASE && w < USER_BASE + USER_MAX; }
/* nomes das fontes registradas, lidos da tabela 'name' (por processo; o id vem do kernel) */
static struct { char ps[96], family[96], full[96]; } unames[USER_MAX];
/* peso gravado no NSFont, ou -1 se o objeto não parece um NSFont do appkit.c */
static int fweight_raw(id f) {
    if (!f) return -1;
    ft_t *ft = ftf(f);
    if (!(ft->size > 0 && ft->size < 1e6)) return -1;
    return (ft->weight >= 0 && ft->weight <= 3) || is_user(ft->weight) ? ft->weight : -1;
}
static const char *fname(id f) {
    int w = fweight_raw(f);
    if (is_user(w)) return unames[w - USER_BASE].ps[0] ? unames[w - USER_BASE].ps : "mOS-WebFont";
    id n = f ? MSG0(id, f, "fontName") : 0;
    return n ? cx_cstr(n) : ".AppleSystemUIFont";
}
static u32 ftraits(id f) {
    const char *n = fname(f);
    int w = fweight_raw(f);
    u32 t = 0;
    if (w == 2 || w == 3) t |= TRAIT_BOLD;
    if (strstr(n, "Bold") || strstr(n, "bold") || strstr(n, "Semibold") || strstr(n, "Black") || strstr(n, "Heavy")) t |= TRAIT_BOLD;
    if (strstr(n, "Italic") || strstr(n, "Oblique")) t |= TRAIT_ITALIC;
    if (strstr(n, "Mono") || strstr(n, "Menlo") || strstr(n, "Courier")) t |= TRAIT_MONO;
    if (w >= 0 && w <= 3 && ftf(f)->mono) t |= TRAIT_MONO;
    if (n[0] == '.') t |= TRAIT_UI;
    return t;
}
static id family_of(id f) {
    int w = fweight_raw(f);
    if (is_user(w)) {
        const char *fam = unames[w - USER_BASE].family;
        return cx_str(fam[0] ? fam : fname(f));
    }
    const char *n = fname(f);
    if (n[0] == '.') return cx_str(n);
    if (strstr(n, "Menlo") || strstr(n, "Mono")) return cx_str("Menlo");
    return cx_str("Helvetica");
}
static id font_of_size(double size, int bold) {
    return MSG(id, cls("NSFont"), bold ? "boldSystemFontOfSize:" : "systemFontOfSize:", size > 0 ? size : 13.0);
}
static int fweight(id f) {
    if (!f) return 0;
    int w = fweight_raw(f);
    if (w >= 0) return w;
    id nm = MSG0(id, f, "fontName");
    const char *n = nm ? cx_cstr(nm) : "";
    if (strstr(n, "Bold") || strstr(n, "bold")) return 3;
    if (strstr(n, "Semibold")) return 2;
    if (strstr(n, "Medium")) return 1;
    return 0;
}
/* NSFont novo (systemFontOfSize: devolve sempre um objeto novo) com a face w */
static id font_with_face(double size, int w, int mono) {
    id n = font_of_size(size, 0);
    if (n) { ftf(n)->weight = w; ftf(n)->mono = mono; }
    return n;
}
/* cópia de f em outro tamanho, com a mesma face (inclusive a de uma fonte registrada) */
static id font_copy(id f, double size) {
    int w = fweight_raw(f);
    return font_with_face(size, w >= 0 ? w : fweight(f), w >= 0 && w <= 3 ? ftf(f)->mono : 0);
}
static int font_live(void) {
    static int v;
    if (v) return 1;
    gui_font_metrics_t m;
    memset(&m, 0, sizeof m);
    /* upem 0 enquanto a Inter não carregou: tenta de novo na próxima, não grava o fracasso */
    if (SYS5(GUI_FONT, GF_METRICS, 0, 0, &m, 0) >= 0 && m.upem > 0) v = 1;
    return v;
}
static int face_metrics(id f, gui_font_metrics_t *m) {
    if (!font_live()) return 0;
    memset(m, 0, sizeof *m);
    return SYS5(GUI_FONT, GF_METRICS, fweight(f), 0, m, 0) >= 0 && m->upem > 0;
}
/* ---- métricas (unidades reais da Inter quando o kernel responde) */
static double em_metric(id f, int which) {
    gui_font_metrics_t m;
    if (!face_metrics(f, &m) || !m.upem) {
        double s = fsize(f);
        if (which == 0) return s * 0.952;
        if (which == 1) return s * 0.241;
        return 2048;
    }
    double s = fsize(f) / m.upem;
    if (which == 0) return m.ascender * s;
    if (which == 1) return m.descender < 0 ? -m.descender * s : m.descender * s;
    return m.upem;
}
double CTFontGetSize(id f) { return fsize(f); }
double CTFontGetAscent(id f) { return em_metric(f, 0); }
double CTFontGetDescent(id f) { return em_metric(f, 1); }
double CTFontGetLeading(id f) { (void)f; return 0; }
double CTFontGetCapHeight(id f) { return fsize(f) * 0.705; }
double CTFontGetXHeight(id f) { return fsize(f) * 0.517; }
double CTFontGetSlantAngle(id f) { return (ftraits(f) & TRAIT_ITALIC) ? -12 : 0; }
double CTFontGetUnderlinePosition(id f) { return -fsize(f) * 0.1; }
double CTFontGetUnderlineThickness(id f) { return fsize(f) * 0.05; }
unsigned CTFontGetUnitsPerEm(id f) { return (unsigned)em_metric(f, 2); }
long CTFontGetGlyphCount(id f) {
    gui_font_metrics_t m;
    if (face_metrics(f, &m) && m.nglyphs > 0) return m.nglyphs;
    return 0xFFFF;
}
u32 CTFontGetSymbolicTraits(id f) { return ftraits(f); }
ct_rect_t CTFontGetBoundingBox(id f) { double s = fsize(f); ct_rect_t r = {-0.1 * s, -0.3 * s, 1.3 * s, 1.3 * s}; return r; }

/* glifos: com o kernel, o índice é o da cmap da Inter e o avanço é o do hmtx. Sem ele, o glifo é o
 * código do caractere e o avanço é 0,55 em (0,6 em nas monoespaçadas). */
static double advance_fake(id f) { return fsize(f) * ((ftraits(f) & TRAIT_MONO) ? 0.6 : 0.55); }
static int real_advance(id f, u16 g, double *out) {
    if (!font_live()) return 0;
    long r = SYS5(GUI_FONT, GF_ADVANCE, fweight(f), g, 0, 0);
    if (r < 0) return 0;
    int units = (int)(r & 0xFFFF), upem = (int)((r >> 16) & 0xFFFF);
    if (!upem) return 0;
    *out = units * fsize(f) / upem;
    return 1;
}
BOOL CTFontGetGlyphsForCharacters(id f, const u16 *chars, u16 *glyphs, long n) {
    BOOL all = YES;
    int live = font_live();
    int w = fweight(f);
    for (long i = 0; i < n; i++) {
        if (live) {
            long g = SYS5(GUI_FONT, GF_CMAP, w, chars[i], 0, 0);
            glyphs[i] = g < 0 ? chars[i] : (u16)g;
            if (g <= 0) all = NO;
        } else {
            glyphs[i] = chars[i];
            if (!chars[i]) all = NO;
        }
    }
    return all;
}
double CTFontGetAdvancesForGlyphs(id f, int orientation, const u16 *glyphs, ct_sz_t *adv, long n) {
    (void)orientation;
    double sum = 0;
    for (long i = 0; i < n; i++) {
        double a;
        if (!real_advance(f, glyphs ? glyphs[i] : 0, &a)) a = advance_fake(f);
        if (adv) { adv[i].w = a; adv[i].h = 0; }
        sum += a;
    }
    return sum;
}
/* pinta o alfa do glifo num CGBitmapContext (coregraphics.c). 0 = não é bitmap. */
int cx_bitmap_draw_glyph(void *ctx, double x, double y, const u8 *alpha, int w, int h, int left, int top);
static int glyph_box(id f, u16 g, float size, i16 *w, i16 *h, i16 *left, i16 *top) {
    gui_glyph_req_t req;
    memset(&req, 0, sizeof req);
    req.weight = (u32)fweight(f);
    req.glyph = g;
    req.size = size > 0 ? size : (float)fsize(f);
    long n = SYS5(GUI_FONT, GF_RASTER, 0, 0, &req, 0);
    if (n < 0) return 0;
    *w = (i16)req.w; *h = (i16)req.h; *left = (i16)req.left; *top = (i16)req.top;
    return 1;
}
static int raster_glyph(id f, u16 g, float size, i16 *w, i16 *h, i16 *left, i16 *top, u8 **buf) {
    if (!glyph_box(f, g, size, w, h, left, top)) return 0;
    if (*w <= 0 || *h <= 0) { *buf = 0; return 1; }
    u32 n = (u32)(*w) * (u32)(*h);
    u8 *b = malloc(n);
    if (!b) return 0;
    gui_glyph_req_t req;
    memset(&req, 0, sizeof req);
    req.weight = (u32)fweight(f);
    req.glyph = g;
    req.size = size > 0 ? size : (float)fsize(f);
    req.cap = n;
    req.buf = (u64)(size_t)b;
    long n2 = SYS5(GUI_FONT, GF_RASTER, 0, 0, &req, 0);
    if (n2 < 0) { free(b); return 0; }
    *buf = b;
    return 1;
}
ct_rect_t CTFontGetBoundingRectsForGlyphs(id f, int orientation, const u16 *glyphs, ct_rect_t *rects, long n) {
    (void)orientation;
    ct_rect_t uni = {0, 0, 0, 0};
    int any = 0;
    for (long i = 0; i < n; i++) {
        ct_rect_t r = {0, -0.241 * fsize(f), advance_fake(f), 1.193 * fsize(f)};
        if (font_live() && glyphs) {
            i16 w = 0, h = 0, left = 0, top = 0;
            if (glyph_box(f, glyphs[i], (float)fsize(f), &w, &h, &left, &top)) {
                /* top é o deslocamento y-down a partir da linha de base (negativo = acima) */
                r.x = left;
                r.y = -(double)(top + h);
                r.w = w;
                r.h = h;
            }
        }
        if (rects) rects[i] = r;
        if (!any) { uni = r; any = 1; }
        else {
            double x1 = uni.x < r.x ? uni.x : r.x;
            double y1 = uni.y < r.y ? uni.y : r.y;
            double x2 = uni.x + uni.w > r.x + r.w ? uni.x + uni.w : r.x + r.w;
            double y2 = uni.y + uni.h > r.y + r.h ? uni.y + uni.h : r.y + r.h;
            uni.x = x1; uni.y = y1; uni.w = x2 - x1; uni.h = y2 - y1;
        }
    }
    return uni;
}
double cx_bm_scale(const void *ctx);
void CTFontDrawGlyphs(id f, const u16 *glyphs, const ct_pt_t *pos, size_t n, void *ctx) {
    if (!f || !glyphs || !pos || !ctx || !font_live()) return;
    double sc = cx_bm_scale(ctx);
    float sz = (float)(fsize(f) * (sc > 0 ? sc : 1));
    for (size_t i = 0; i < n; i++) {
        i16 w = 0, h = 0, left = 0, top = 0;
        u8 *buf = 0;
        if (!raster_glyph(f, glyphs[i], sz, &w, &h, &left, &top, &buf)) continue;
        if (buf) cx_bitmap_draw_glyph(ctx, pos[i].x, pos[i].y, buf, w, h, left, top);
        free(buf);
    }
}
/* Ainda sem contornos vetoriais (o kernel só entrega o raster). Mas o Skia (SkScalerContext_Mac::generateMetrics)
 * pergunta CTFontCreatePathForGlyph(f, g, NULL) a todo glifo de avanço zero e, se o caminho vier vazio, o trata como
 * vazio: os pontos do árabe (ccmp os separa em marcas), os diacríticos combinantes e os niqqud sumiam. Sem matriz,
 * devolve a caixa do glifo quando ele tem tinta; com matriz (o caminho de verdade, texto muito grande) segue NULL. */
void *CGPathCreateWithRect(ct_rect_t r, const void *m);
void *CTFontCreatePathForGlyph(id f, u16 g, const void *matrix) {
    if (matrix || !f || !font_live()) return 0;
    i16 w = 0, h = 0, left = 0, top = 0;
    if (!glyph_box(f, g, (float)fsize(f), &w, &h, &left, &top) || w <= 0 || h <= 0) return 0;
    return CGPathCreateWithRect((ct_rect_t){left, -(double)(top + h), w, h}, 0);
}
void *CTFontCopyGraphicsFont(id f, void *attrs) { (void)f; (void)attrs; return 0; }
id CTFontCopyAvailableTables(id f, u32 options) {
    (void)options;
    /* fourcc da Apple, em ordem: cmap glyf loca head hhea hmtx maxp name post OS/2 GPOS GSUB 'CFF ' */
    static const u32 tags[] = {0x636D6170u, 0x676C7966u, 0x6C6F6361u, 0x68656164u, 0x68686561u, 0x686D7478u,
                               0x6D617870u, 0x6E616D65u, 0x706F7374u, 0x4F532F32u, 0x47504F53u, 0x47535542u, 0x43464620u};
    id a = MSG0(id, cls("NSMutableArray"), "array");
    int w = fweight(f);
    for (u32 i = 0; font_live() && i < sizeof tags / sizeof tags[0]; i++)
        if (SYS5(GUI_FONT, GF_TABLE, w, tags[i], 0, 0) > 0) MSG(void, a, "addObject:", cx_num_i((i64)tags[i]));
    return objc_retain(a);
}
void *CTFontCopyTable(id f, u32 tag, u32 options) {
    (void)options;
    if (!font_live()) return 0;
    long n = SYS5(GUI_FONT, GF_TABLE, fweight(f), tag, 0, 0);
    if (n <= 0 || n > (8 << 20)) return 0;
    u8 *buf = malloc((size_t)n);
    if (!buf) return 0;
    long n2 = SYS5(GUI_FONT, GF_TABLE, fweight(f), tag, buf, n);
    id data = n2 > 0 ? (id)CFDataCreate(0, buf, n2 < n ? n2 : n) : 0;
    free(buf);
    return data;
}
/* o Skia cai aqui quando CTFontCopyTable não acha a tabela, com o CGFont de CTFontCopyGraphicsFont,
 * que é nulo no mOS: sem fonte não há tabela (antes vinha a da Inter, e o HarfBuzz aplicava o GPOS da
 * Inter aos glifos de outra fonte) */
void *CGFontCopyTableForTag(void *font, u32 tag) { return font ? CTFontCopyTable((id)font, tag, 0) : 0; }
void *CTFontCopyCharacterSet(id f) { (void)f; return 0; }
id CTFontCopyDefaultCascadeListForLanguages(id f, id langs) { (void)f; (void)langs; return objc_retain(MSG0(id, cls("NSArray"), "array")); }
id CTFontCopyVariation(id f) { (void)f; return 0; }
id CTFontCopyVariationAxes(id f) { (void)f; return 0; }

/* ---- nomes */
id CTFontCopyPostScriptName(id f) { return objc_retain(cx_str(fname(f))); }
id CTFontCopyFullName(id f) { return objc_retain(cx_str(fname(f))); }
id CTFontCopyFamilyName(id f) { return objc_retain(family_of(f)); }
id CTFontCopyName(id f, id key) { (void)key; return objc_retain(cx_str(fname(f))); }
id CTFontCopyLocalizedName(id f, id key, id *lang) { if (lang) *lang = 0; return CTFontCopyName(f, key); }

/* ---- traços e atributos */
/* kCTFontWeightTrait de cada face da Inter (NSFontWeightRegular, Medium, Semibold, Bold): o Blink e o
 * Skia tiram daqui o peso CSS da fonte, e um peso menor que o pedido vira negrito sintético */
static const double face_weight_trait[4] = {0.0, 0.23, 0.3, 0.4};
static id traits_dict(id f) {
    u32 t = ftraits(f);
    int w = fweight_raw(f);
    double wt = (w >= 0 && w <= 3) ? face_weight_trait[w] : (t & TRAIT_BOLD) ? 0.4 : 0.0;
    id d = MSG0(id, cls("NSMutableDictionary"), "dictionary");
    MSG(void, d, "setObject:forKey:", cx_num_i((i64)t), (id)kCTFontSymbolicTrait);
    MSG(void, d, "setObject:forKey:", cx_num_d(wt), (id)kCTFontWeightTrait);
    MSG(void, d, "setObject:forKey:", cx_num_d(0.0), (id)kCTFontWidthTrait);
    MSG(void, d, "setObject:forKey:", cx_num_d((t & TRAIT_ITALIC) ? 0.07 : 0.0), (id)kCTFontSlantTrait);
    return d;
}
id CTFontCopyTraits(id f) { return objc_retain(traits_dict(f)); }
/* descritor = dicionário de atributos */
static id descriptor_for(id f) {
    id d = MSG0(id, cls("NSMutableDictionary"), "dictionary");
    MSG(void, d, "setObject:forKey:", cx_str(fname(f)), (id)kCTFontNameAttribute);
    MSG(void, d, "setObject:forKey:", family_of(f), (id)kCTFontFamilyNameAttribute);
    MSG(void, d, "setObject:forKey:", cx_str(fname(f)), (id)kCTFontDisplayNameAttribute);
    MSG(void, d, "setObject:forKey:", cx_str((ftraits(f) & TRAIT_BOLD) ? "Bold" : "Regular"), (id)kCTFontStyleNameAttribute);
    MSG(void, d, "setObject:forKey:", traits_dict(f), (id)kCTFontTraitsAttribute);
    MSG(void, d, "setObject:forKey:", cx_num_d(fsize(f)), (id)kCTFontSizeAttribute);
    int w = fweight_raw(f);
    if (is_user(w)) MSG(void, d, "setObject:forKey:", cx_num_i(w), cx_str(USER_ID_KEY));
    return d;
}
id CTFontCopyFontDescriptor(id f) { return objc_retain(descriptor_for(f)); }
id CTFontCopyAttribute(id f, id key) { id v = MSG(id, descriptor_for(f), "objectForKey:", key); return v ? objc_retain(v) : 0; }

/* ---- criação */
/* peso pedido por um descritor, como face da Inter (0..3); -1 = não pede. O Blink pede o peso da fonte
 * do sistema pelo eixo 'wght' de kCTFontVariationAttribute (100..900); o Skia, por kCTFontWeightTrait. */
static int desc_weight(id desc) {
    if (!desc) return -1;
    id var = MSG(id, desc, "objectForKey:", (id)kCTFontVariationAttribute);
    if (var) {
        /* chaves são CFNumber com o tag do eixo: compara pelo valor, não pela igualdade do dicionário */
        id keys = MSG0(id, var, "allKeys");
        u64 n = keys ? MSG0(u64, keys, "count") : 0;
        for (u64 i = 0; i < n; i++) {
            id k = MSG(id, keys, "objectAtIndex:", i);
            if (!k || MSG0(i64, k, "longLongValue") != 0x77676874) continue;     /* 'wght' */
            id v = MSG(id, var, "objectForKey:", k);
            double w = v ? MSG0(double, v, "doubleValue") : 400;
            return w >= 650 ? 3 : w >= 550 ? 2 : w >= 450 ? 1 : 0;
        }
    }
    id tr = MSG(id, desc, "objectForKey:", (id)kCTFontTraitsAttribute);
    if (tr) {
        id v = MSG(id, tr, "objectForKey:", (id)kCTFontWeightTrait);
        if (v) {
            double w = MSG0(double, v, "doubleValue");
            return w >= 0.35 ? 3 : w >= 0.27 ? 2 : w >= 0.2 ? 1 : 0;
        }
        id s = MSG(id, tr, "objectForKey:", (id)kCTFontSymbolicTrait);
        if (s && (MSG0(i64, s, "longLongValue") & TRAIT_BOLD)) return 3;
    }
    return -1;
}
static int desc_user_id(id desc) {
    id u = desc ? MSG(id, desc, "objectForKey:", cx_str(USER_ID_KEY)) : 0;
    int w = u ? (int)MSG0(i64, u, "longLongValue") : -1;
    return is_user(w) ? w : -1;
}
static id font_from_desc(id desc, double size) {
    if (size <= 0 && desc) { id s = MSG(id, desc, "objectForKey:", (id)kCTFontSizeAttribute); if (s) size = MSG0(double, s, "doubleValue"); }
    if (size <= 0) size = 13;
    /* fonte registrada (CTFontManagerCreateFontDescriptorFromData): pelo id no descritor. Pelo nome não:
     * no macOS uma fonte criada de dados não entra no catálogo, e uma fonte web chamada "Helvetica" não
     * pode tomar o lugar da do sistema. */
    int u = desc_user_id(desc);
    if (u >= 0) return font_with_face(size, u, 0);
    id name = desc ? MSG(id, desc, "objectForKey:", (id)kCTFontNameAttribute) : 0;
    if (!name && desc) name = MSG(id, desc, "objectForKey:", (id)kCTFontFamilyNameAttribute);
    id f = name ? MSG(id, cls("NSFont"), "fontWithName:size:", name, size) : 0;
    if (!f) f = font_of_size(size, 0);
    int w = desc_weight(desc), fw = fweight_raw(f);
    if (w >= 0 && fw >= 0 && fw <= 3) ftf(f)->weight = w;   /* fontWithName:size: devolve um objeto novo */
    return f;
}
id CTFontCreateWithName(id name, double size, const void *matrix) {
    (void)matrix;
    if (size <= 0) size = 12;
    id f = name ? MSG(id, cls("NSFont"), "fontWithName:size:", name, size) : 0;
    return objc_retain(f ? f : font_of_size(size, 0));
}
id CTFontCreateWithFontDescriptor(id desc, double size, const void *matrix) { (void)matrix; return objc_retain(font_from_desc(desc, size)); }
id CTFontCreateUIFontForLanguage(u32 type, double size, id lang) { (void)type; (void)lang; return objc_retain(font_of_size(size > 0 ? size : 13, 0)); }
/* O Skia copia toda fonte para o tamanho do texto com um descritor que só traz variações
 * (SkCTFontCreateExactCopy), e o Blink pede o peso da fonte do sistema do mesmo jeito: um descritor sem
 * nome mantém a face de f (antes virava a regular, e uma fonte web virava a Inter). */
id CTFontCreateCopyWithAttributes(id f, double size, const void *matrix, id desc) {
    (void)matrix;
    double s = size > 0 ? size : fsize(f);
    if (desc && (desc_user_id(desc) >= 0 || MSG(id, desc, "objectForKey:", (id)kCTFontNameAttribute) ||
                 MSG(id, desc, "objectForKey:", (id)kCTFontFamilyNameAttribute)))
        return objc_retain(font_from_desc(desc, s));
    int w = desc_weight(desc);
    if (w < 0 && (size <= 0 || size == fsize(f)) && f) return objc_retain(f);
    id n = font_copy(f, s);
    int fw = fweight_raw(n);
    if (n && w >= 0 && fw >= 0 && fw <= 3) ftf(n)->weight = w;
    return objc_retain(n);
}
id CTFontCreateCopyWithSymbolicTraits(id f, double size, const void *matrix, u32 value, u32 mask) {
    (void)matrix;
    double s = size > 0 ? size : fsize(f);
    id n = font_copy(f, s);
    int fw = fweight_raw(n);
    if (n && fw >= 0 && fw <= 3 && (mask & TRAIT_BOLD)) ftf(n)->weight = (value & TRAIT_BOLD) ? 3 : 0;
    return objc_retain(n);
}
typedef struct { long loc, len; } ct_range_t;
/* ---- fontes de reserva (fallback): árabe, hebraico e CJK
 * A Inter do kernel só tem latim, grego e cirílico. Quando o primeiro caractere do trecho não tem glifo na fonte
 * pedida, o Blink (FontCache::PlatformFallbackFontForCharacter) pede outra a CTFontCreateForStringWithLanguage; aqui
 * as Noto Sans de /Sistema/Biblioteca/Fontes (OFL) são registradas no kernel na primeira vez que fazem falta
 * (GF_REGISTER; o mesmo arquivo dá o mesmo id em todo processo) e entra a primeira que tem o glifo. Para Han, a ordem
 * segue o idioma: ja → JP, ko → KR, o resto → SC. */
extern u8 *cx_read_file(const char *path, u64 *len);
static void user_names_load(int fid);
#define FB_DIR "/Sistema/Biblioteca/Fontes/"
enum { FB_AR, FB_HE, FB_SC, FB_JP, FB_KR, FB_N };
static const char *const fb_file[FB_N] = {"NotoSansArabic-Regular.ttf", "NotoSansHebrew-Regular.ttf", "NotoSansSC-Regular.otf",
                                          "NotoSansJP-Regular.otf", "NotoSansKR-Regular.otf"};
static int fb_id[FB_N];                 /* 0 = ainda não tentou; -1 = falhou; senão o id da face */
static volatile int fb_lock;
static int fb_face(int i) {
    while (__sync_lock_test_and_set(&fb_lock, 1)) SYS0(SYS_YIELD);
    if (!fb_id[i]) {
        char p[128];
        snprintf(p, sizeof p, FB_DIR "%s", fb_file[i]);
        u64 n = 0;
        u8 *b = font_live() ? cx_read_file(p, &n) : 0;
        long r = b && n >= 12 && n <= (8 << 20) ? SYS5(GUI_FONT, GF_REGISTER, 0, (long)n, b, 0) : -1;
        free(b);
        fb_id[i] = is_user((int)r) ? (int)r : -1;
        if (fb_id[i] > 0) user_names_load(fb_id[i]);
    }
    int id = fb_id[i];
    __sync_lock_release(&fb_lock);
    return id;
}
static int has_glyph(int face, u32 cp) { return SYS5(GUI_FONT, GF_CMAP, face, cp, 0, 0) > 0; }
static u32 str_cp(id str, ct_range_t r) {
    if (!str || r.len <= 0) return 0;
    u32 c = (u16)MSG(u16, str, "characterAtIndex:", (u64)r.loc);
    if (c >= 0xD800 && c < 0xDC00 && r.len > 1) {
        u32 d = (u16)MSG(u16, str, "characterAtIndex:", (u64)r.loc + 1);
        if (d >= 0xDC00 && d < 0xE000) c = 0x10000 + ((c - 0xD800) << 10) + (d - 0xDC00);
    }
    return c;
}
static id font_for_cp(id f, u32 cp, const char *lang) {
    if (!f) f = font_of_size(13, 0);
    if (!cp || !font_live() || has_glyph(fweight(f), cp)) return f;
    int order[FB_N] = {FB_AR, FB_HE, FB_SC, FB_JP, FB_KR};
    if (lang && !strncmp(lang, "ja", 2)) { order[2] = FB_JP; order[3] = FB_SC; }
    else if (lang && !strncmp(lang, "ko", 2)) { order[2] = FB_KR; order[4] = FB_SC; }
    if ((cp >= 0x3040 && cp < 0x3100) || (cp >= 0x31F0 && cp < 0x3200)) { order[2] = FB_JP; order[3] = FB_SC; }   /* kana */
    if ((cp >= 0xAC00 && cp < 0xD7B0) || (cp >= 0x1100 && cp < 0x1200) || (cp >= 0x3130 && cp < 0x3190)) {   /* hangul */
        order[2] = FB_KR; order[4] = FB_SC;
    }
    for (int k = 0; k < FB_N; k++) {
        int face = fb_face(order[k]);
        if (face > 0 && has_glyph(face, cp)) return font_with_face(fsize(f), face, 0);
    }
    return f;
}
id CTFontCreateForString(id f, id str, ct_range_t r) { return objc_retain(font_for_cp(f, str_cp(str, r), 0)); }
id CTFontCreateForStringWithLanguage(id f, id str, ct_range_t r, id lang) {
    return objc_retain(font_for_cp(f, str_cp(str, r), lang && cx_is_str(lang) ? cx_cstr(lang) : 0));
}

/* ---- descritores */
id CTFontDescriptorCreateWithAttributes(id attrs) {
    id d = MSG0(id, cls("NSMutableDictionary"), "dictionary");
    if (attrs) MSG(void, d, "addEntriesFromDictionary:", attrs);
    return objc_retain(d);
}
id CTFontDescriptorCopyAttribute(id d, id key) { id v = d ? MSG(id, d, "objectForKey:", key) : 0; return v ? objc_retain(v) : 0; }
id CTFontDescriptorCopyLocalizedAttribute(id d, id key, id *lang) { if (lang) *lang = 0; return CTFontDescriptorCopyAttribute(d, key); }
id CTFontDescriptorCopyAttributes(id d) { return CTFontDescriptorCreateWithAttributes(d); }
id CTFontDescriptorCreateMatchingFontDescriptor(id d, id mandatory) { (void)mandatory; return d ? objc_retain(d) : 0; }
id CTFontDescriptorCreateMatchingFontDescriptors(id d, id mandatory) {
    (void)mandatory;
    id a = MSG0(id, cls("NSMutableArray"), "array");
    if (d) MSG(void, a, "addObject:", d);
    return objc_retain(a);
}
unsigned long CFGetTypeID(const void *cf);
unsigned long CTFontDescriptorGetTypeID(void) { return CFGetTypeID(MSG0(id, cls("NSMutableDictionary"), "dictionary")); }
BOOL CTFontDescriptorIsSystemUIFont(id d) {
    id n = d ? MSG(id, d, "objectForKey:", (id)kCTFontNameAttribute) : 0;
    return n && cx_cstr(n)[0] == '.';
}

/* ---- coleções e o gerenciador de fontes: as famílias que o mOS desenha */
static const char *families[] = {".AppleSystemUIFont", "Helvetica", "Menlo"};
static id family_descriptor(const char *fam) {
    id d = MSG0(id, cls("NSMutableDictionary"), "dictionary");
    MSG(void, d, "setObject:forKey:", cx_str(fam), (id)kCTFontFamilyNameAttribute);
    MSG(void, d, "setObject:forKey:", cx_str(fam), (id)kCTFontNameAttribute);
    return d;
}
id CTFontManagerCopyAvailableFontFamilyNames(void) {
    id a = MSG0(id, cls("NSMutableArray"), "array");
    for (u32 i = 1; i < sizeof families / sizeof families[0]; i++) MSG(void, a, "addObject:", cx_str(families[i]));
    return objc_retain(a);
}
/* ---- fontes a partir de dados: as fontes web do Chromium (SkTypeface_Mac::MakeFromStream) chegam
 * aqui já passadas pelo OTS. O kernel valida de novo, guarda a fonte e devolve o id da face, que vai no
 * descritor (USER_ID_KEY) e daí para o NSFont; tabelas, cmap, avanços e glifos saem dela por GUI_FONT
 * como os da Inter. TrueType (glyf) e OpenType CFF ('OTTO', kernel/gfx/font_cff.inc: as fontes web do Slack);
 * o que o kernel recusar (CFF2, tabelas malformadas) devolve nil, e o Blink segue para a próxima fonte da lista. */
long CFDataGetLength(id d);
const u8 *CFDataGetBytePtr(id d);
static u32 rd16(const u8 *p) { return (u32)p[0] << 8 | p[1]; }
/* nameID da tabela 'name' em ASCII: prefere Unicode (plataforma 0 ou Windows 3,1/3,0, UTF-16BE) e
 * aceita Mac Roman (1,0); o que não é ASCII imprimível vira '_' */
static void name_entry(const u8 *t, long n, u32 want, char *out, int cap) {
    out[0] = 0;
    if (n < 6) return;
    u32 count = rd16(t + 2), so = rd16(t + 4);
    int best = 0;
    for (u32 i = 0; i < count && 6 + (long)(i + 1) * 12 <= n; i++) {
        const u8 *r = t + 6 + i * 12;
        u32 pid = rd16(r), eid = rd16(r + 2), nid = rd16(r + 6), len = rd16(r + 8), off = rd16(r + 10);
        if (nid != want || (long)so + off + len > n) continue;
        int utf16 = pid == 0 || (pid == 3 && eid <= 1);
        int score = utf16 ? 2 : (pid == 1 && eid == 0) ? 1 : 0;
        if (score <= best) continue;
        const u8 *s = t + so + off;
        int k = 0;
        for (u32 j = 0; j + (utf16 ? 1u : 0u) < len && k < cap - 1; j += utf16 ? 2 : 1) {
            u32 c = utf16 ? rd16(s + j) : s[j];
            out[k++] = (c >= 0x20 && c < 0x7F) ? (char)c : '_';
        }
        out[k] = 0;
        if (k) best = score;
    }
}
static void user_names_load(int fid) {
    int i = fid - USER_BASE;
    long n = SYS5(GUI_FONT, GF_TABLE, fid, 0x6E616D65u, 0, 0);          /* 'name' */
    u8 *t = (n > 0 && n <= (1 << 20)) ? malloc((size_t)n) : 0;
    if (t && SYS5(GUI_FONT, GF_TABLE, fid, 0x6E616D65u, t, n) > 0) {
        name_entry(t, n, 6, unames[i].ps, sizeof unames[i].ps);
        name_entry(t, n, 1, unames[i].family, sizeof unames[i].family);
        name_entry(t, n, 4, unames[i].full, sizeof unames[i].full);
    }
    free(t);
    if (!unames[i].ps[0]) {                                         /* sem nome: "mOS-WebFont-<id>" */
        const char *p = "mOS-WebFont-";
        int k = 0;
        while (*p) unames[i].ps[k++] = *p++;
        unames[i].ps[k++] = (char)('0' + fid / 10);
        unames[i].ps[k++] = (char)('0' + fid % 10);
        unames[i].ps[k] = 0;
    }
    if (!unames[i].family[0]) memcpy(unames[i].family, unames[i].ps, sizeof unames[i].ps);
    if (!unames[i].full[0]) memcpy(unames[i].full, unames[i].family, sizeof unames[i].family);
}
id CTFontManagerCreateFontDescriptorFromData(id data) {
    if (!data || !font_live()) return 0;
    const u8 *b = CFDataGetBytePtr(data);
    long n = CFDataGetLength(data);
    if (!b || n < 12 || n > (8 << 20)) return 0;
    long r = SYS5(GUI_FONT, GF_REGISTER, 0, n, b, 0);
    if (!is_user((int)r)) return 0;
    int fid = (int)r, i = fid - USER_BASE;
    user_names_load(fid);
    id d = MSG0(id, cls("NSMutableDictionary"), "dictionary");
    MSG(void, d, "setObject:forKey:", cx_str(unames[i].ps), (id)kCTFontNameAttribute);
    MSG(void, d, "setObject:forKey:", cx_str(unames[i].family), (id)kCTFontFamilyNameAttribute);
    MSG(void, d, "setObject:forKey:", cx_str(unames[i].full), (id)kCTFontDisplayNameAttribute);
    MSG(void, d, "setObject:forKey:", cx_num_i(fid), cx_str(USER_ID_KEY));
    return objc_retain(d);
}
/* CGFont do mOS não carrega os bytes da fonte (CGFontCreateWithDataProvider devolve nulo).
 * Sem os bytes o kernel não registra a face; o app recebe falso e segue a lista de fallback. */
int CTFontManagerRegisterGraphicsFont(void *font, id *error) {
    (void)font;
    if (error) *error = 0;
    return 0;
}
static const char *ct_url_path(id url) {
    if (!url) return 0;
    if (cx_responds(object_getClass(url), sel_registerName("path"))) {
        id p = MSG0(id, url, "path");
        if (p && cx_is_str(p)) return cx_cstr(p);
    }
    if (cx_is_str(url)) return cx_cstr(url);
    return 0;
}
int open(const char *path, int flags, ...);
long read(int fd, void *b, unsigned long n);
int close(int fd);
static id ct_data_from_path(const char *path) {
    if (!path || !path[0]) return 0;
    int fd = open(path, 0);
    if (fd < 0) return 0;
    u8 *buf = malloc(8 << 20);
    long n = 0;
    if (buf) {
        while (n < (8 << 20)) {
            long r = read(fd, buf + n, (unsigned long)((8 << 20) - n));
            if (r <= 0) break;
            n += r;
        }
    }
    close(fd);
    id data = (buf && n > 0) ? CFDataCreate(0, buf, n) : 0;
    free(buf);
    return data;
}
id CTFontManagerCreateFontDescriptorsFromData(id data) {
    id a = MSG0(id, cls("NSMutableArray"), "array");
    id d = CTFontManagerCreateFontDescriptorFromData(data);
    if (d) MSG(void, a, "addObject:", d);
    objc_release(d);
    return objc_retain(a);
}
id CTFontManagerCreateFontDescriptorsFromURL(id url) {
    id data = ct_data_from_path(ct_url_path(url));
    id a = CTFontManagerCreateFontDescriptorsFromData(data);
    objc_release(data);
    return a;
}
int CTFontManagerRegisterFontsForURL(id url, unsigned scope, id *error) {
    (void)scope;
    if (error) *error = 0;
    id data = ct_data_from_path(ct_url_path(url));
    id d = data ? CTFontManagerCreateFontDescriptorFromData(data) : 0;
    objc_release(data);
    if (!d) return 0;
    objc_release(d);
    return 1;
}
unsigned CTFontGetTypeID(void) { return 0x4354464Eu; }
unsigned CTGetCoreTextVersion(void) { return 0x00080000u; }
typedef struct { double a, b, c, d, tx, ty; } ct_mx_t;
ct_mx_t CTFontGetMatrix(id f) {
    ct_mx_t m;
    (void)f;
    m.a = 1; m.b = 0; m.c = 0; m.d = 1; m.tx = 0; m.ty = 0;
    return m;
}
static u16 ct_glyph(id font, u32 cp);
u16 CTFontGetGlyphWithName(id f, id name) {
    const char *s = name && cx_is_str(name) ? cx_cstr(name) : "";
    unsigned cp = 0;
    if (!strcmp(s, "space")) cp = ' ';
    else if (!strcmp(s, "period")) cp = '.';
    else if (s[0] == 'u' && s[1] == 'n' && s[2] == 'i' && strlen(s) == 7) {
        for (int i = 3; i < 7; i++) {
            char c = s[i];
            unsigned h = c >= '0' && c <= '9' ? (unsigned)(c - '0') : c >= 'A' && c <= 'F' ? (unsigned)(c - 'A' + 10) : c >= 'a' && c <= 'f' ? (unsigned)(c - 'a' + 10) : 99;
            if (h > 15) { cp = 0; break; }
            cp = (cp << 4) | h;
        }
    } else if (s[0] && !s[1]) cp = (unsigned char)s[0];
    if (!cp) return 0;
    return ct_glyph(f, cp);
}
void CTFontGetVerticalTranslationsForGlyphs(id f, int orientation, const u16 *glyphs, ct_sz_t *trans, long n) {
    (void)f; (void)orientation; (void)glyphs;
    if (!trans || n <= 0) return;
    memset(trans, 0, (size_t)n * sizeof *trans);
}
id CTFontCopyFeatures(id f) {
    (void)f;
    return objc_retain(MSG0(id, cls("NSMutableArray"), "array"));
}
id CTFontCreateWithGraphicsFont(void *font, double size, const void *matrix, id attrs) {
    (void)font; (void)matrix;
    if (attrs) return CTFontCreateWithFontDescriptor(attrs, size, 0);
    return objc_retain(font_of_size(size > 0 ? size : 13, 0));
}
id CTFontDescriptorCreateCopyWithAttributes(id d, id attrs) {
    id n = CTFontDescriptorCreateWithAttributes(d);
    if (n && attrs) MSG(void, n, "addEntriesFromDictionary:", attrs);
    return n;
}
id CTFontCollectionCreateWithFontDescriptors(id descs, id options) {
    (void)options;
    if (descs) return objc_retain(descs);
    return objc_retain(MSG0(id, cls("NSMutableArray"), "array"));
}
id CTFontCollectionCreateFromAvailableFonts(id options) { (void)options; return objc_retain(MSG0(id, cls("NSMutableArray"), "array")); }
id CTFontCollectionCreateMatchingFontDescriptors(id coll) {
    (void)coll;
    id a = MSG0(id, cls("NSMutableArray"), "array");
    for (u32 i = 1; i < sizeof families / sizeof families[0]; i++) MSG(void, a, "addObject:", family_descriptor(families[i]));
    return objc_retain(a);
}
id CTFontCollectionCreateMatchingFontDescriptorsForFamily(id coll, id family, id options) {
    (void)coll; (void)options;
    if (!family) return 0;
    const char *f = cx_cstr(family);
    for (u32 i = 0; i < sizeof families / sizeof families[0]; i++)
        if (!strncmp(f, families[i], 64)) {
            id a = MSG0(id, cls("NSMutableArray"), "array");
            MSG(void, a, "addObject:", family_descriptor(families[i]));
            return objc_retain(a);
        }
    return 0;
}

/* ---- NSFont: o que o gfx::PlatformFontMac do Chromium pergunta e o appkit.c ainda não tem */
static id nf_sized(id cls_, SEL c, double size) { (void)cls_; (void)c; return font_of_size(size > 0 ? size : 13, 0); }
static double nf_ascender(id self, SEL c) { (void)c; return CTFontGetAscent(self); }
static double nf_descender(id self, SEL c) { (void)c; return -CTFontGetDescent(self); }
static double nf_cap(id self, SEL c) { (void)c; return CTFontGetCapHeight(self); }
static double nf_xh(id self, SEL c) { (void)c; return CTFontGetXHeight(self); }
static double nf_leading(id self, SEL c) { (void)self; (void)c; return 0; }
static id nf_family(id self, SEL c) { (void)c; return family_of(self); }
static id nf_display(id self, SEL c) { (void)c; return cx_str(fname(self)); }
static id nf_descriptor(id self, SEL c) { (void)c; return descriptor_for(self); }
static BOOL nf_fixed(id self, SEL c) { (void)c; return (ftraits(self) & TRAIT_MONO) != 0; }
static ct_rect_t nf_bbox(id self, SEL c) { (void)c; return CTFontGetBoundingBox(self); }
static double nf_underline_pos(id self, SEL c) { (void)c; return CTFontGetUnderlinePosition(self); }
static double nf_underline_th(id self, SEL c) { (void)c; return CTFontGetUnderlineThickness(self); }
static id fd_with_attrs(id cls_, SEL c, id attrs) { (void)cls_; (void)c; id d = CTFontDescriptorCreateWithAttributes(attrs); return d; }
/* -[NSFontDescriptor symbolicTraits] chega aqui como -[NSDictionary symbolicTraits] (descritor = dicionário) */
static u32 dict_symbolic(id self, SEL c) {
    (void)c;
    id t = MSG(id, self, "objectForKey:", (id)kCTFontTraitsAttribute);
    id s = t ? MSG(id, t, "objectForKey:", (id)kCTFontSymbolicTrait) : 0;
    return s ? (u32)MSG0(i64, s, "longLongValue") : 0;
}
static id dict_postscript(id self, SEL c) { (void)c; return MSG(id, self, "objectForKey:", (id)kCTFontNameAttribute); }
static double dict_point(id self, SEL c) { (void)c; id s = MSG(id, self, "objectForKey:", (id)kCTFontSizeAttribute); return s ? MSG0(double, s, "doubleValue") : 0; }
static id dict_attrs(id self, SEL c) { (void)c; return self; }

/* ---- CTLine / CTTypesetter (Postico 2.3.5)
 * Funcional: uma linha, um cluster por ponto de código, avanços do CTFont, truncamento e desenho
 * no CGBitmapContext na posição da text matrix (tx, ty), que anda a largura da linha.
 * Limitação: sem quebra de grafema (isso é ICU, outro agente). Tabulação = 4 avanços, sem paradas
 * de tab. Acima de 100000 unidades UTF-16 o resto da string não vira glifo. Pontuação pendurada,
 * optical bounds e language extents não mudam a caixa. Sem a fonte do kernel, CTLineDraw carimba
 * um retângulo; CTFontDrawGlyphs continua sem tinta. O NSString do mOS devolve U+FFFD para escalares
 * acima de U+FFFF (uma unidade), então o par substituto não aparece.
 * CT_HOST só existe no harness Linux (tools/ctline_host_stubs.c): o binário do libSystem não define
 * a macro e compila o caminho da fonte do kernel. O harness não pode executar int 0x80. */
extern id NSFontAttributeName, NSForegroundColorAttributeName;
void cx_object_free(id o);
void *calloc(size_t n, size_t s);
void cx_bm_set_fill(const void *c, u32 argb);
typedef struct { double a, b, c, d, tx, ty; } ct_tm;
ct_tm CGContextGetTextMatrix(void *c);
void CGContextSetTextMatrix(void *c, ct_tm t);
extern void cx_cfhost_init(void);

typedef struct {
    u32 cp;
    u16 glyph;
    long idx;
    int units;
    double adv;
    id font;
} ct_clus;
typedef struct {
    id astr;
    ct_range_t range;
    double width, ascent, descent, leading;
    ct_clus *g;
    long n;
    id runs;
} ct_line_t;
typedef struct {
    id astr;
    ct_clus *g;
    long n, utf16, seen;
} ct_ts_t;

static cx_class_t CTLine_c, CTLine_m, CTTypesetter_c, CTTypesetter_m;
static cx_class_t CTRun_c, CTRun_m, CTFrame_c, CTFrame_m, CTFramesetter_c, CTFramesetter_m;
static int ct_lines_ready;

static void *ct_state(id o) { return o ? *(void **)((u8 *)o + 8) : 0; }
static id ct_key(const char *s, id *slot) { if (!*slot) *slot = cx_str(s); return *slot; }

static id ct_dict_get(id attrs, id key) {
    if (!attrs || !key) return 0;
    if (!cx_responds(object_getClass(attrs), sel_registerName("objectForKey:"))) return 0;
    return MSG(id, attrs, "objectForKey:", key);
}
static id ct_attrs(id astr, long idx) {
    if (!astr) return 0;
    if (cx_responds(object_getClass(astr), sel_registerName("attributesAtIndex:effectiveRange:")))
        return MSG(id, astr, "attributesAtIndex:effectiveRange:", (u64)idx, (void *)0);
    void *st = *(void **)((u8 *)astr + 8);
    return st ? ((id *)st)[1] : 0;
}
static id ct_string(id astr) {
    if (!astr) return 0;
    if (cx_responds(object_getClass(astr), sel_registerName("string"))) return MSG0(id, astr, "string");
    void *st = *(void **)((u8 *)astr + 8);
    return st ? ((id *)st)[0] : 0;
}
static id ct_font_at(id astr, long idx) {
    id attrs = ct_attrs(astr, idx);
    id f = NSFontAttributeName ? ct_dict_get(attrs, NSFontAttributeName) : 0;
    if (!f) f = ct_dict_get(attrs, (id)kCTFontAttributeName);
    if (!f) { static id k; f = ct_dict_get(attrs, ct_key("NSFont", &k)); }
    return f;
}
static id ct_color_at(id astr, long idx) {
    id attrs = ct_attrs(astr, idx);
    id c = NSForegroundColorAttributeName ? ct_dict_get(attrs, NSForegroundColorAttributeName) : 0;
    if (!c) { static id k; c = ct_dict_get(attrs, ct_key("NSColor", &k)); }
    if (!c) c = ct_dict_get(attrs, (id)kCTForegroundColorAttributeName);
    if (!c) { static id k; c = ct_dict_get(attrs, ct_key("CTForegroundColor", &k)); }
    return c;
}
static double ct_kern_at(id astr, long idx) {
    id attrs = ct_attrs(astr, idx);
    static id k1, k2;
    id k = ct_dict_get(attrs, ct_key("NSKern", &k1));
    if (!k) k = ct_dict_get(attrs, (id)kCTKernAttributeName);
    if (!k) k = ct_dict_get(attrs, ct_key("kCTKernAttributeName", &k2));
    if (!k) k = ct_dict_get(attrs, (id)kCTTrackingAttributeName);
    if (!k || !cx_responds(object_getClass(k), sel_registerName("doubleValue"))) return 0;
    return MSG0(double, k, "doubleValue");
}
static int ct_is_color(id c) {
    if (!c) return 0;
    const char *n = cx_class_name(object_getClass(c));
    return n && strstr(n, "Color");
}
static u32 ct_argb(id c) {
    double *p = (double *)((u8 *)c + 8);
    u32 o[4];
    for (int i = 0; i < 4; i++) {
        double x = p[i] < 0 ? 0 : p[i] > 1 ? 1 : p[i];
        o[i] = (u32)(x * 255.0 + 0.5);
    }
    return o[3] << 24 | o[0] << 16 | o[1] << 8 | o[2];
}
static u32 ct_cp_at(id str, long idx, int *units) {
    u32 c = (u16)MSG(u16, str, "characterAtIndex:", (u64)idx);
    long len = (long)MSG0(u64, str, "length");
    *units = 1;
    if (c >= 0xD800 && c < 0xDC00 && idx + 1 < len) {
        u32 d = (u16)MSG(u16, str, "characterAtIndex:", (u64)(idx + 1));
        if (d >= 0xDC00 && d < 0xE000) {
            *units = 2;
            return 0x10000u + ((c - 0xD800u) << 10) + (d - 0xDC00u);
        }
    }
    return c;
}
static u16 ct_glyph(id font, u32 cp) {
    u16 g = (u16)(cp > 0xFFFF ? 0xFFFD : cp);
#if !defined(CT_HOST)
    if (font && font_live()) {
        u16 out = g;
        CTFontGetGlyphsForCharacters(font, &g, &out, 1);
        return out;
    }
#else
    (void)font;
#endif
    return g;
}
static double ct_adv(id font, u32 cp, u16 glyph) {
    if (cp == '\n' || cp == '\r' || cp == 0) return 0;
    double a = advance_fake(font);
#if !defined(CT_HOST)
    if (font && font_live() && cp >= 0x20) {
        ct_sz_t sz;
        CTFontGetAdvancesForGlyphs(font, 0, &glyph, &sz, 1);
        a = sz.w;
    }
#else
    (void)glyph;
#endif
    if (cp == '\t') a *= 4;
    return a;
}
static void ct_font_metrics(id font, double *as, double *de, double *ld) {
    double s = fsize(font);
#if !defined(CT_HOST)
    if (font && font_live()) {
        *as = CTFontGetAscent(font);
        *de = CTFontGetDescent(font);
        *ld = CTFontGetLeading(font);
        return;
    }
#endif
    *as = s * 0.952;
    *de = s * 0.241;
    *ld = 0;
}
static void ct_recompute(ct_line_t *b) {
    b->width = b->ascent = b->descent = b->leading = 0;
    for (long i = 0; i < b->n; i++) {
        double as, de, ld;
        ct_font_metrics(b->g[i].font, &as, &de, &ld);
        if (as > b->ascent) b->ascent = as;
        if (de > b->descent) b->descent = de;
        if (ld > b->leading) b->leading = ld;
        b->width += b->g[i].adv;
    }
    if (!b->n) {
        double as, de, ld;
        ct_font_metrics(0, &as, &de, &ld);
        b->ascent = as;
        b->descent = de;
        b->leading = ld;
    }
}
static void ct_free_line(ct_line_t *b) {
    if (!b) return;
    for (long i = 0; i < b->n; i++) objc_release(b->g[i].font);
    objc_release(b->runs);
    free(b->g);
    objc_release(b->astr);
    free(b);
}
static void ct_free_ts(ct_ts_t *t) {
    if (!t) return;
    for (long i = 0; i < t->n; i++) objc_release(t->g[i].font);
    free(t->g);
    objc_release(t->astr);
    free(t);
}
static int ct_build(id astr, id str, ct_clus **out, long *nout, long *seen) {
    long len = str ? (long)MSG0(u64, str, "length") : 0;
    if (len < 0) len = 0;
    long cap = len > 100000 ? 100000 : len;
    ct_clus *g = cap ? calloc((size_t)cap, sizeof *g) : 0;
    if (cap && !g) return 0;
    id fallback = 0;
    long n = 0;
    for (long i = 0; i < cap;) {
        int units = 1;
        u32 cp = str ? ct_cp_at(str, i, &units) : 0;
        if (units < 1) units = 1;
        if (i + units > cap) break;
        id font = ct_font_at(astr, i);
        if (!font) {
            if (!fallback) fallback = font_of_size(13.0, 0);
            font = fallback;
        }
        u16 gl = ct_glyph(font, cp);
        double adv = ct_adv(font, cp, gl) + ct_kern_at(astr, i);
        if (adv < 0) adv = 0;
        g[n].cp = cp;
        g[n].glyph = gl;
        g[n].idx = i;
        g[n].units = units;
        g[n].adv = adv;
        g[n].font = objc_retain(font);
        n++;
        i += units;
    }
    if (fallback) objc_release(fallback);
    *out = g;
    *nout = n;
    *seen = cap;
    return 1;
}
static void ct_line_dealloc(id self, SEL c) {
    (void)c;
    ct_free_line(ct_state(self));
    *(void **)((u8 *)self + 8) = 0;
    cx_object_free(self);
}
static void ct_ts_dealloc(id self, SEL c) {
    (void)c;
    ct_free_ts(ct_state(self));
    *(void **)((u8 *)self + 8) = 0;
    cx_object_free(self);
}
static void ct_run_dealloc(id self, SEL c);
static void ct_frame_dealloc(id self, SEL c);
static void ct_fs_dealloc(id self, SEL c);
static void ct_lines_init(void) {
    if (ct_lines_ready) return;
    cx_class_t *ns = cx_class_named("NSObject");
    if (!ns) return;
    cx_define_class(&CTLine_c, &CTLine_m, "CTLine", ns, 8);
    cx_define_class(&CTTypesetter_c, &CTTypesetter_m, "CTTypesetter", ns, 8);
    cx_define_class(&CTRun_c, &CTRun_m, "CTRun", ns, 8);
    cx_define_class(&CTFrame_c, &CTFrame_m, "CTFrame", ns, 8);
    cx_define_class(&CTFramesetter_c, &CTFramesetter_m, "CTFramesetter", ns, 8);
    cx_add_method(&CTLine_c, "dealloc", (void *)ct_line_dealloc);
    cx_add_method(&CTTypesetter_c, "dealloc", (void *)ct_ts_dealloc);
    cx_add_method(&CTRun_c, "dealloc", (void *)ct_run_dealloc);
    cx_add_method(&CTFrame_c, "dealloc", (void *)ct_frame_dealloc);
    cx_add_method(&CTFramesetter_c, "dealloc", (void *)ct_fs_dealloc);
    ct_lines_ready = 1;
}
static id ct_line_wrap(ct_line_t *b) {
    ct_lines_init();
    if (!ct_lines_ready || !b) { ct_free_line(b); return 0; }
    id o = cx_alloc_instance(&CTLine_c);
    if (!o) { ct_free_line(b); return 0; }
    *(void **)((u8 *)o + 8) = b;
    return o;
}

id CTLineCreateWithAttributedString(id astr) {
    if (!astr) return 0;
    ct_lines_init();
    if (!ct_lines_ready) return 0;
    id str = ct_string(astr);
    long len = str ? (long)MSG0(u64, str, "length") : 0;
    ct_clus *g = 0;
    long n = 0, seen = 0;
    if (!ct_build(astr, str, &g, &n, &seen)) return 0;
    ct_line_t *b = calloc(1, sizeof *b);
    if (!b) { free(g); return 0; }
    b->astr = objc_retain(astr);
    b->range.loc = 0;
    b->range.len = len;
    b->g = g;
    b->n = n;
    ct_recompute(b);
    return ct_line_wrap(b);
}
id CTTypesetterCreateWithAttributedString(id astr) {
    if (!astr) return 0;
    ct_lines_init();
    if (!ct_lines_ready) return 0;
    id str = ct_string(astr);
    ct_ts_t *t = calloc(1, sizeof *t);
    if (!t) return 0;
    t->astr = objc_retain(astr);
    t->utf16 = str ? (long)MSG0(u64, str, "length") : 0;
    if (!ct_build(astr, str, &t->g, &t->n, &t->seen)) { ct_free_ts(t); return 0; }
    id o = cx_alloc_instance(&CTTypesetter_c);
    if (!o) { ct_free_ts(t); return 0; }
    *(void **)((u8 *)o + 8) = t;
    return o;
}
static long ct_suggest(ct_ts_t *t, long start, double width, int word) {
    if (!t || start < 0 || start >= t->utf16) return 0;
    if (start >= t->seen) return t->utf16 - start;
    long i = 0;
    while (i < t->n && t->g[i].idx + t->g[i].units <= start) i++;
    if (i >= t->n) return start < t->utf16 ? t->utf16 - start : 0;
    double x = 0;
    long last_brk = -1, count = 0;
    int any = 0;
    for (; i < t->n; i++) {
        ct_clus *g = &t->g[i];
        int nl = g->cp == '\n' || g->cp == '\r';
        if (!nl && any && x + g->adv > width + 1e-6) {
            if (word && last_brk > start) return last_brk - start;
            return count;
        }
        x += g->adv;
        count = (g->idx + g->units) - start;
        any = 1;
        if (g->cp == ' ' || g->cp == '\t') last_brk = g->idx + g->units;
        if (nl) return count;
    }
    return count ? count : (start < t->utf16 ? t->utf16 - start : 0);
}
long CTTypesetterSuggestLineBreak(id ts, long start, double width) { return ct_suggest(ct_state(ts), start, width, 1); }
long CTTypesetterSuggestClusterBreak(id ts, long start, double width) { return ct_suggest(ct_state(ts), start, width, 0); }
id CTTypesetterCreateLine(id ts, ct_range_t range) {
    ct_ts_t *t = ct_state(ts);
    if (!t) return 0;
    if (range.loc < 0) range.loc = 0;
    if (range.len < 0) range.len = 0;
    long end = range.loc + range.len;
    long n = 0;
    for (long i = 0; i < t->n; i++)
        if (t->g[i].idx >= range.loc && t->g[i].idx < end) n++;
    ct_line_t *b = calloc(1, sizeof *b);
    if (!b) return 0;
    b->astr = objc_retain(t->astr);
    b->range = range;
    b->n = n;
    b->g = n ? calloc((size_t)n, sizeof(ct_clus)) : 0;
    if (n && !b->g) { ct_free_line(b); return 0; }
    long k = 0;
    for (long i = 0; i < t->n && k < n; i++) {
        if (t->g[i].idx < range.loc || t->g[i].idx >= end) continue;
        b->g[k] = t->g[i];
        b->g[k].font = objc_retain(t->g[i].font);
        k++;
    }
    ct_recompute(b);
    return ct_line_wrap(b);
}

static void ct_paint(void *ctx, double x, double y, ct_clus *g) {
#if !defined(CT_HOST)
    if (g->font && font_live()) {
        ct_pt_t p;
        p.x = x;
        p.y = y;
        u16 gl = g->glyph;
        CTFontDrawGlyphs(g->font, &gl, &p, 1, ctx);
        return;
    }
#endif
    double s = fsize(g->font);
    int gh = (int)(s * 0.70);
    int gw = (int)(s * 0.45);
    if (gh < 2) gh = 2;
    if (gw < 2) gw = 2;
    if (gh > 16) gh = 16;
    if (gw > 16) gw = 16;
    u8 pix[16 * 16];
    memset(pix, 200, (size_t)gw * (size_t)gh);
    cx_bitmap_draw_glyph(ctx, x, y, pix, gw, gh, 0, 1 - gh);
}
void CTLineDraw(id line, void *ctx) {
    ct_line_t *b = ct_state(line);
    if (!b || !ctx) return;
    ct_tm m = CGContextGetTextMatrix(ctx);
    for (long i = 0; i < b->n; i++) {
        if (b->g[i].idx < 0) continue;
        id col = ct_color_at(b->astr, b->g[i].idx);
        if (ct_is_color(col)) cx_bm_set_fill(ctx, ct_argb(col));
        break;
    }
    double x = m.tx, y = m.ty;
    for (long i = 0; i < b->n; i++) {
        ct_clus *g = &b->g[i];
        if (g->idx >= 0) {
            id col = ct_color_at(b->astr, g->idx);
            if (ct_is_color(col)) cx_bm_set_fill(ctx, ct_argb(col));
        }
        if (g->cp > 0x20 && g->cp != 0x7F && g->adv > 0) ct_paint(ctx, x, y, g);
        x += g->adv;
    }
    m.tx += b->width;
    CGContextSetTextMatrix(ctx, m);
}
static ct_rect_t ct_path_bounds(ct_line_t *b) {
    ct_rect_t u = {0, 0, 0, 0};
    int any = 0;
    double x = 0;
    for (long i = 0; i < b->n; i++) {
        ct_clus *g = &b->g[i];
        ct_rect_t r;
#if !defined(CT_HOST)
        if (g->font && font_live()) {
            u16 gl = g->glyph;
            r = CTFontGetBoundingRectsForGlyphs(g->font, 0, &gl, 0, 1);
        } else
#endif
        {
            double s = fsize(g->font);
            r.x = 0;
            r.y = -0.241 * s;
            r.w = g->adv;
            r.h = 1.193 * s;
        }
        r.x += x;
        if (!any) u = r;
        else {
            double x1 = u.x < r.x ? u.x : r.x, y1 = u.y < r.y ? u.y : r.y;
            double x2 = u.x + u.w > r.x + r.w ? u.x + u.w : r.x + r.w;
            double y2 = u.y + u.h > r.y + r.h ? u.y + u.h : r.y + r.h;
            u.x = x1;
            u.y = y1;
            u.w = x2 - x1;
            u.h = y2 - y1;
        }
        any = 1;
        x += g->adv;
    }
    return u;
}
ct_rect_t CTLineGetBoundsWithOptions(id line, unsigned long options) {
    ct_rect_t z = {0, 0, 0, 0};
    ct_line_t *b = ct_state(line);
    if (!b) return z;
    if ((options & 8) && b->n) return ct_path_bounds(b);
    z.y = -b->descent;
    z.w = b->width;
    z.h = b->ascent + b->descent + ((options & 1) ? 0 : b->leading);
    return z;
}

static ct_line_t *ct_clone(ct_line_t *src) {
    ct_line_t *b = calloc(1, sizeof *b);
    if (!b) return 0;
    b->astr = objc_retain(src->astr);
    b->range = src->range;
    b->n = src->n;
    b->g = src->n ? calloc((size_t)src->n, sizeof(ct_clus)) : 0;
    if (src->n && !b->g) { ct_free_line(b); return 0; }
    for (long i = 0; i < src->n; i++) {
        b->g[i] = src->g[i];
        b->g[i].font = objc_retain(src->g[i].font);
    }
    ct_recompute(b);
    return b;
}
static ct_clus *ct_ellipsis(id font, long *n, double *w) {
    ct_clus *g = calloc(3, sizeof *g);
    if (!g) { *n = 0; *w = 0; return 0; }
    u16 gl = ct_glyph(font, '.');
    double a = ct_adv(font, '.', gl);
    for (int i = 0; i < 3; i++) {
        g[i].cp = '.';
        g[i].glyph = gl;
        g[i].idx = -1;
        g[i].units = 1;
        g[i].adv = a;
        g[i].font = objc_retain(font);
    }
    *n = 3;
    *w = a * 3;
    return g;
}
static long ct_fit_prefix(ct_clus *g, long n, double budget) {
    double acc = 0;
    long k = 0;
    for (long i = 0; i < n; i++) {
        if (acc + g[i].adv > budget + 1e-6) break;
        acc += g[i].adv;
        k++;
    }
    return k;
}
static long ct_fit_suffix(ct_clus *g, long n, double budget) {
    double acc = 0;
    long k = n;
    for (long i = n; i-- > 0;) {
        if (acc + g[i].adv > budget + 1e-6) break;
        acc += g[i].adv;
        k = i;
    }
    return k;
}
id CTLineCreateTruncatedLine(id line, double width, unsigned type, id token) {
    ct_line_t *src = ct_state(line);
    if (!src) return 0;
    if (width < 0) width = 0;
    if (src->width <= width + 1e-6) {
        ct_line_t *b = ct_clone(src);
        return b ? ct_line_wrap(b) : 0;
    }
    long tn = 0;
    double tw = 0;
    ct_clus *tg = 0;
    if (token && object_getClass(token) == &CTLine_c) {
        ct_line_t *tok = ct_state(token);
        if (tok && tok->n) {
            tg = calloc((size_t)tok->n, sizeof *tg);
            if (!tg) return 0;
            tn = tok->n;
            for (long i = 0; i < tn; i++) {
                tg[i] = tok->g[i];
                tg[i].idx = -1;
                tg[i].font = objc_retain(tok->g[i].font);
                tw += tg[i].adv;
            }
        }
    }
    if (!tg) tg = ct_ellipsis(src->n ? src->g[0].font : 0, &tn, &tw);
    double budget = width > tw ? width - tw : 0;
    long pre = 0, suf = src->n;
    if (type == 0) suf = ct_fit_suffix(src->g, src->n, budget);
    else if (type == 2) {
        pre = ct_fit_prefix(src->g, src->n, budget * 0.5);
        suf = ct_fit_suffix(src->g, src->n, budget * 0.5);
        if (suf < pre) suf = pre;
    } else pre = ct_fit_prefix(src->g, src->n, budget);
    long n = pre + tn + (src->n - suf);
    ct_line_t *b = calloc(1, sizeof *b);
    if (!b) {
        for (long i = 0; i < tn; i++) objc_release(tg[i].font);
        free(tg);
        return 0;
    }
    b->astr = objc_retain(src->astr);
    b->range = src->range;
    b->n = n;
    b->g = n ? calloc((size_t)n, sizeof(ct_clus)) : 0;
    if (n && !b->g) {
        for (long i = 0; i < tn; i++) objc_release(tg[i].font);
        free(tg);
        ct_free_line(b);
        return 0;
    }
    long k = 0;
    for (long i = 0; i < pre; i++) {
        b->g[k] = src->g[i];
        b->g[k].font = objc_retain(src->g[i].font);
        k++;
    }
    for (long i = 0; i < tn; i++) b->g[k++] = tg[i];
    free(tg);
    for (long i = suf; i < src->n; i++) {
        b->g[k] = src->g[i];
        b->g[k].font = objc_retain(src->g[i].font);
        k++;
    }
    ct_recompute(b);
    return ct_line_wrap(b);
}

typedef struct {
    ct_line_t *line;
    long begin, count;
    id font, attrs;
} ct_run_t;
typedef struct {
    id lines, path, astr;
    ct_pt_t *origins;
    long n;
    ct_range_t range, visible;
} ct_frame_t;
typedef struct { id ts; } ct_fs_t;

static void ct_run_dealloc(id self, SEL c) {
    ct_run_t *r = ct_state(self);
    (void)c;
    if (r) { objc_release(r->font); objc_release(r->attrs); free(r); }
    *(void **)((u8 *)self + 8) = 0;
    cx_object_free(self);
}
static void ct_frame_dealloc(id self, SEL c) {
    ct_frame_t *f = ct_state(self);
    (void)c;
    if (f) { objc_release(f->lines); objc_release(f->path); objc_release(f->astr); free(f->origins); free(f); }
    *(void **)((u8 *)self + 8) = 0;
    cx_object_free(self);
}
static void ct_fs_dealloc(id self, SEL c) {
    ct_fs_t *f = ct_state(self);
    (void)c;
    if (f) { objc_release(f->ts); free(f); }
    *(void **)((u8 *)self + 8) = 0;
    cx_object_free(self);
}
static void ct_run_bounds(ct_run_t *r, ct_range_t range, long *a, long *b) {
    long n = r ? r->count : 0;
    if (!r || range.len < 0) { *a = 0; *b = 0; return; }
    if (range.loc == 0 && range.len == 0) { *a = 0; *b = n; return; }
    if (range.loc < 0) range.loc = 0;
    if (range.loc > n) range.loc = n;
    if (range.loc + range.len > n) range.len = n - range.loc;
    *a = range.loc;
    *b = range.loc + range.len;
}
static id ct_run_new(ct_line_t *line, long begin, long count) {
    ct_run_t *r = calloc(1, sizeof *r);
    id o;
    if (!r) return 0;
    r->line = line;
    r->begin = begin;
    r->count = count;
    if (count > 0) {
        r->font = objc_retain(line->g[begin].font);
        r->attrs = objc_retain(ct_attrs(line->astr, line->g[begin].idx));
    }
    o = cx_alloc_instance(&CTRun_c);
    if (!o) { objc_release(r->font); objc_release(r->attrs); free(r); return 0; }
    *(void **)((u8 *)o + 8) = r;
    return o;
}
id CTLineGetGlyphRuns(id line) {
    ct_line_t *b = ct_state(line);
    id arr;
    long i;
    if (!b) return 0;
    if (b->runs) return b->runs;
    ct_lines_init();
    arr = MSG0(id, cls("NSMutableArray"), "array");
    i = 0;
    while (i < b->n) {
        long j = i + 1;
        id font = b->g[i].font;
        while (j < b->n && b->g[j].font == font) j++;
        id run = ct_run_new(b, i, j - i);
        if (run) MSG(void, arr, "addObject:", run);
        objc_release(run);
        i = j;
    }
    b->runs = objc_retain(arr);
    return b->runs;
}
double CTLineGetTypographicBounds(id line, double *ascent, double *descent, double *leading) {
    ct_line_t *b = ct_state(line);
    if (!b) return 0;
    if (ascent) *ascent = b->ascent;
    if (descent) *descent = b->descent;
    if (leading) *leading = b->leading;
    return b->width;
}
long CTLineGetGlyphCount(id line) { ct_line_t *b = ct_state(line); return b ? b->n : 0; }
ct_range_t CTLineGetStringRange(id line) {
    ct_range_t z = {0, 0};
    ct_line_t *b = ct_state(line);
    return b ? b->range : z;
}
double CTLineGetOffsetForStringIndex(id line, long idx, double *secondary) {
    ct_line_t *b = ct_state(line);
    double x = 0;
    if (secondary) *secondary = 0;
    if (!b) return 0;
    for (long i = 0; i < b->n; i++) {
        ct_clus *g = &b->g[i];
        if (g->idx < 0) { x += g->adv; continue; }
        if (idx < g->idx + g->units) {
            double frac = g->units > 1 && idx > g->idx ? g->adv : 0;
            if (secondary) *secondary = x + g->adv * 0.5;
            return x + frac;
        }
        x += g->adv;
    }
    return x;
}
double CTLineGetTrailingWhitespaceWidth(id line) {
    ct_line_t *b = ct_state(line);
    double w = 0;
    if (!b) return 0;
    for (long i = b->n; i-- > 0;) {
        u32 cp = b->g[i].cp;
        if (cp != ' ' && cp != '\t' && cp != '\n' && cp != '\r') break;
        w += b->g[i].adv;
    }
    return w;
}
ct_rect_t CTLineGetImageBounds(id line, void *ctx) {
    ct_rect_t z = {0, 0, 0, 0};
    ct_line_t *b = ct_state(line);
    (void)ctx;
    if (!b) return z;
    return ct_path_bounds(b);
}
id CTLineCreateJustifiedLine(id line, double factor, double width) {
    ct_line_t *src = ct_state(line);
    ct_line_t *b;
    double extra, spaces, each;
    long ns = 0;
    if (!src) return 0;
    if (factor < 0) factor = 0;
    if (factor > 1) factor = 1;
    b = ct_clone(src);
    if (!b) return 0;
    extra = (width - b->width) * factor;
    if (extra <= 1e-6 || b->n < 1) return ct_line_wrap(b);
    for (long i = 0; i < b->n; i++) if (b->g[i].cp == ' ') ns++;
    spaces = ns ? (double)ns : (double)b->n;
    each = extra / spaces;
    for (long i = 0; i < b->n; i++) if (!ns || b->g[i].cp == ' ') b->g[i].adv += each;
    ct_recompute(b);
    return ct_line_wrap(b);
}
long CTRunGetGlyphCount(id run) { ct_run_t *r = ct_state(run); return r ? r->count : 0; }
ct_range_t CTRunGetStringRange(id run) {
    ct_range_t z = {0, 0};
    ct_run_t *r = ct_state(run);
    if (!r || r->count < 1 || !r->line) return z;
    z.loc = r->line->g[r->begin].idx;
    if (z.loc < 0) z.loc = 0;
    z.len = 0;
    for (long i = 0; i < r->count; i++) z.len += r->line->g[r->begin + i].units;
    return z;
}
id CTRunGetAttributes(id run) { ct_run_t *r = ct_state(run); return r ? r->attrs : 0; }
unsigned CTRunGetStatus(id run) { (void)run; return 0; }
void CTRunGetGlyphs(id run, ct_range_t range, u16 *buf) {
    ct_run_t *r = ct_state(run);
    long a, b;
    ct_run_bounds(r, range, &a, &b);
    if (!buf) return;
    for (long i = a; i < b; i++) buf[i - a] = r->line->g[r->begin + i].glyph;
}
const u16 *CTRunGetGlyphsPtr(id run) { (void)run; return 0; }
void CTRunGetAdvances(id run, ct_range_t range, ct_sz_t *buf) {
    ct_run_t *r = ct_state(run);
    long a, b;
    ct_run_bounds(r, range, &a, &b);
    if (!buf) return;
    for (long i = a; i < b; i++) { buf[i - a].w = r->line->g[r->begin + i].adv; buf[i - a].h = 0; }
}
void CTRunGetPositions(id run, ct_range_t range, ct_pt_t *buf) {
    ct_run_t *r = ct_state(run);
    long a, b;
    double x = 0;
    ct_run_bounds(r, range, &a, &b);
    if (!r || !buf) return;
    for (long i = 0; i < r->count; i++) {
        if (i >= a && i < b) { buf[i - a].x = x; buf[i - a].y = 0; }
        x += r->line->g[r->begin + i].adv;
    }
}
const ct_pt_t *CTRunGetPositionsPtr(id run) { (void)run; return 0; }
void CTRunGetStringIndices(id run, ct_range_t range, long *buf) {
    ct_run_t *r = ct_state(run);
    long a, b;
    ct_run_bounds(r, range, &a, &b);
    if (!buf) return;
    for (long i = a; i < b; i++) {
        long idx = r->line->g[r->begin + i].idx;
        buf[i - a] = idx < 0 ? 0 : idx;
    }
}
const long *CTRunGetStringIndicesPtr(id run) { (void)run; return 0; }
double CTRunGetTypographicBounds(id run, ct_range_t range, double *ascent, double *descent, double *leading) {
    ct_run_t *r = ct_state(run);
    long a, b;
    double w = 0, as = 0, de = 0, ld = 0;
    ct_run_bounds(r, range, &a, &b);
    for (long i = a; i < b; i++) {
        double a1, d1, l1;
        ct_font_metrics(r->line->g[r->begin + i].font, &a1, &d1, &l1);
        if (a1 > as) as = a1;
        if (d1 > de) de = d1;
        if (l1 > ld) ld = l1;
        w += r->line->g[r->begin + i].adv;
    }
    if (ascent) *ascent = as;
    if (descent) *descent = de;
    if (leading) *leading = ld;
    return w;
}
void CTRunDraw(id run, void *ctx, ct_range_t range) {
    ct_run_t *r = ct_state(run);
    long a, b;
    double x;
    ct_tm m;
    ct_run_bounds(r, range, &a, &b);
    if (!r || !ctx) return;
    m = CGContextGetTextMatrix(ctx);
    x = m.tx;
    for (long i = 0; i < a; i++) x += r->line->g[r->begin + i].adv;
    for (long i = a; i < b; i++) {
        ct_clus *g = &r->line->g[r->begin + i];
        if (g->cp > 0x20 && g->cp != 0x7F && g->adv > 0) ct_paint(ctx, x, m.ty, g);
        x += g->adv;
    }
}
id CTTypesetterCreateWithAttributedStringAndOptions(id astr, id opts) {
    (void)opts;
    return CTTypesetterCreateWithAttributedString(astr);
}
id CTFramesetterCreateWithAttributedString(id astr) {
    id ts, o;
    ct_fs_t *f;
    ct_lines_init();
    ts = CTTypesetterCreateWithAttributedString(astr);
    if (!ts || !ct_lines_ready) { objc_release(ts); return 0; }
    f = calloc(1, sizeof *f);
    if (!f) { objc_release(ts); return 0; }
    f->ts = ts;
    o = cx_alloc_instance(&CTFramesetter_c);
    if (!o) { objc_release(ts); free(f); return 0; }
    *(void **)((u8 *)o + 8) = f;
    return o;
}
static void ct_path_box(void *path, double *w, double *h) {
    typedef struct { double x, y; } pt_t;
    typedef struct { void *isa; u64 cfinfo; u8 *types; pt_t *pts; u64 n, cap, npts, ptcap; pt_t start, cur; } view_t;
    view_t *p = path;
    double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    *w = 1e9;
    *h = 1e9;
    if (!p || !p->pts || !p->npts) return;
    x0 = x1 = p->pts[0].x;
    y0 = y1 = p->pts[0].y;
    for (u64 i = 1; i < p->npts && i < 4096; i++) {
        if (p->pts[i].x < x0) x0 = p->pts[i].x;
        if (p->pts[i].y < y0) y0 = p->pts[i].y;
        if (p->pts[i].x > x1) x1 = p->pts[i].x;
        if (p->pts[i].y > y1) y1 = p->pts[i].y;
    }
    *w = x1 - x0;
    *h = y1 - y0;
    if (*w < 1) *w = 1;
    if (*h < 1) *h = 1;
}
id CTFramesetterCreateFrame(id fs, ct_range_t range, void *path, id attrs) {
    ct_fs_t *f = ct_state(fs);
    ct_ts_t *t;
    ct_frame_t *fr;
    id o, arr;
    double maxw, maxh, y, usedh;
    long pos, fit_end;
    (void)attrs;
    if (!f || !f->ts) return 0;
    t = ct_state(f->ts);
    if (!t) return 0;
    if (range.loc == 0 && range.len == 0) { range.loc = 0; range.len = t->utf16; }
    if (range.loc < 0) range.loc = 0;
    if (range.len < 0) range.len = 0;
    if (range.loc > t->utf16) range.loc = t->utf16;
    if (range.loc + range.len > t->utf16) range.len = t->utf16 - range.loc;
    ct_path_box(path, &maxw, &maxh);
    fr = calloc(1, sizeof *fr);
    if (!fr) return 0;
    fr->path = path ? objc_retain((id)path) : 0;
    fr->astr = objc_retain(t->astr);
    arr = MSG0(id, cls("NSMutableArray"), "array");
    y = 0;
    usedh = 0;
    pos = range.loc;
    fit_end = range.loc;
    while (pos < range.loc + range.len) {
        long br = ct_suggest(t, pos, maxw, 1);
        id line;
        ct_line_t *lb;
        double lh;
        ct_range_t rr;
        if (br < 1) br = 1;
        if (pos + br > range.loc + range.len) br = range.loc + range.len - pos;
        rr.loc = pos;
        rr.len = br;
        line = CTTypesetterCreateLine(f->ts, rr);
        lb = ct_state(line);
        lh = lb ? lb->ascent + lb->descent + lb->leading : 16;
        if (usedh > 0 && usedh + lh > maxh + 1e-6 && path) { objc_release(line); break; }
        if (line) MSG(void, arr, "addObject:", line);
        objc_release(line);
        y -= lh;
        usedh += lh;
        pos += br;
        fit_end = pos;
        if (!path && usedh > 0 && pos >= range.loc + range.len) break;
    }
    fr->lines = objc_retain(arr);
    fr->n = (long)MSG0(u64, arr, "count");
    fr->origins = fr->n ? calloc((size_t)fr->n, sizeof(ct_pt_t)) : 0;
    y = 0;
    for (long i = 0; i < fr->n; i++) {
        id line = MSG(id, arr, "objectAtIndex:", (u64)i);
        ct_line_t *lb = ct_state(line);
        double lh = lb ? lb->ascent + lb->descent + lb->leading : 0;
        if (fr->origins) { fr->origins[i].x = 0; fr->origins[i].y = y - (lb ? lb->ascent : 0); }
        y -= lh;
    }
    fr->range.loc = range.loc;
    fr->range.len = fit_end - range.loc;
    fr->visible = fr->range;
    ct_lines_init();
    o = cx_alloc_instance(&CTFrame_c);
    if (!o) {
        objc_release(fr->lines);
        objc_release(fr->path);
        objc_release(fr->astr);
        free(fr->origins);
        free(fr);
        return 0;
    }
    *(void **)((u8 *)o + 8) = fr;
    return o;
}
id CTFrameGetLines(id frame) { ct_frame_t *f = ct_state(frame); return f ? f->lines : 0; }
id CTFrameGetPath(id frame) { ct_frame_t *f = ct_state(frame); return f ? f->path : 0; }
ct_range_t CTFrameGetStringRange(id frame) {
    ct_range_t z = {0, 0};
    ct_frame_t *f = ct_state(frame);
    return f ? f->range : z;
}
ct_range_t CTFrameGetVisibleStringRange(id frame) {
    ct_range_t z = {0, 0};
    ct_frame_t *f = ct_state(frame);
    return f ? f->visible : z;
}
void CTFrameGetLineOrigins(id frame, ct_range_t range, ct_pt_t *buf) {
    ct_frame_t *f = ct_state(frame);
    long a, b;
    if (!f || !buf) return;
    if (range.loc == 0 && range.len == 0) { a = 0; b = f->n; }
    else {
        a = range.loc < 0 ? 0 : range.loc;
        if (a > f->n) a = f->n;
        b = a + range.len;
        if (b > f->n) b = f->n;
    }
    for (long i = a; i < b; i++) buf[i - a] = f->origins ? f->origins[i] : (ct_pt_t){0, 0};
}
void CTFrameDraw(id frame, void *ctx) {
    ct_frame_t *f = ct_state(frame);
    if (!f || !ctx) return;
    for (long i = 0; i < f->n; i++) {
        id line = MSG(id, f->lines, "objectAtIndex:", (u64)i);
        ct_tm m = CGContextGetTextMatrix(ctx);
        if (f->origins) { m.tx = f->origins[i].x; m.ty = f->origins[i].y; }
        CGContextSetTextMatrix(ctx, m);
        CTLineDraw(line, ctx);
    }
}

void cx_coretext_init(void) {
    cx_class_t *f = cx_class_named("NSFont");
    if (f) {
        cx_class_t *meta = *(cx_class_t **)f;
        static const char *sized[] = {"menuFontOfSize:", "menuBarFontOfSize:", "toolTipsFontOfSize:", "controlContentFontOfSize:",
                                      "paletteFontOfSize:", "labelFontOfSize:", "messageFontOfSize:"};
        for (u32 i = 0; i < sizeof sized / sizeof sized[0]; i++)
            if (!cx_responds(meta, sel_registerName(sized[i]))) cx_add_cmethod(f, sized[i], (void *)nf_sized);
#define FONT_M(s, fn) do { if (!cx_responds(f, sel_registerName(s))) cx_add_method(f, s, (void *)(fn)); } while (0)
        FONT_M("ascender", nf_ascender); FONT_M("descender", nf_descender); FONT_M("capHeight", nf_cap);
        FONT_M("xHeight", nf_xh); FONT_M("leading", nf_leading); FONT_M("familyName", nf_family);
        FONT_M("displayName", nf_display); FONT_M("fontDescriptor", nf_descriptor); FONT_M("isFixedPitch", nf_fixed);
        FONT_M("boundingRectForFont", nf_bbox); FONT_M("underlinePosition", nf_underline_pos);
        FONT_M("underlineThickness", nf_underline_th);
    }
    cx_class_t *d = cx_class_named("NSDictionary");
    if (d) {
        if (!cx_responds(d, sel_registerName("symbolicTraits"))) cx_add_method(d, "symbolicTraits", (void *)dict_symbolic);
        if (!cx_responds(d, sel_registerName("postscriptName"))) cx_add_method(d, "postscriptName", (void *)dict_postscript);
        if (!cx_responds(d, sel_registerName("pointSize"))) cx_add_method(d, "pointSize", (void *)dict_point);
        if (!cx_responds(d, sel_registerName("fontAttributes"))) cx_add_method(d, "fontAttributes", (void *)dict_attrs);
    }
    cx_class_t *fd = cx_class_named("NSFontDescriptor");
    if (fd && !cx_responds(*(cx_class_t **)fd, sel_registerName("fontDescriptorWithFontAttributes:")))
        cx_add_cmethod(fd, "fontDescriptorWithFontAttributes:", (void *)fd_with_attrs);
    ct_lines_init();
    cx_cfhost_init();
}
