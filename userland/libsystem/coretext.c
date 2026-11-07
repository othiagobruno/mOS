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
 */
#include "foundation.h"
#include "../../kernel/include/compat_gui.h"

id CFDataCreate(void *alloc, const u8 *bytes, long n);
void *malloc(size_t n);
void free(void *p);
void *memset(void *d, int c, size_t n);
void *memcpy(void *d, const void *s, size_t n);

id objc_retain(id o);
id cx_num_i(i64 v);
id cx_num_d(double v);
int strncmp(const char *a, const char *b, size_t n);
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
}
