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
typedef struct { double a, b, c, d, tx, ty; } ct_mx_t;

static id cls(const char *n) { return (id)cx_class_named(n); }
static double fsize(id f) { if (!f) return 13; double s = MSG0(double, f, "pointSize"); return s > 0 ? s : 13; }

/* matriz de fonte (o parâmetro matrix de CTFontCreateWith*): o NSFont do appkit.c não tem campo para
 * ela, então fica numa tabela da fonte. A tabela retém a fonte (o endereço não é reaproveitado por outro
 * objeto enquanto a entrada vive); passou de 64, a mais antiga sai e essa fonte volta à identidade. */
#define FMX_MAX 64
static struct { id f; ct_mx_t m; } fmx[FMX_MAX];
static int fmx_next;
static int mx_identity(const ct_mx_t *m) {
    return m->a == 1 && m->b == 0 && m->c == 0 && m->d == 1 && m->tx == 0 && m->ty == 0;
}
static int ct_font_matrix(id f, ct_mx_t *out) {
    for (int i = 0; f && i < FMX_MAX; i++)
        if (fmx[i].f == f) { if (out) *out = fmx[i].m; return 1; }
    if (out) { out->a = 1; out->b = 0; out->c = 0; out->d = 1; out->tx = 0; out->ty = 0; }
    return 0;
}
static void ct_font_set_matrix(id f, const ct_mx_t *m) {
    int i, slot = -1;
    if (!f) return;
    for (i = 0; i < FMX_MAX; i++) if (fmx[i].f == f) { slot = i; break; }
    if (!m || mx_identity(m)) {
        if (slot >= 0) { objc_release(fmx[slot].f); fmx[slot].f = 0; }
        return;
    }
    if (slot < 0) {
        slot = fmx_next;
        fmx_next = (fmx_next + 1) % FMX_MAX;
        if (fmx[slot].f) objc_release(fmx[slot].f);
        fmx[slot].f = objc_retain(f);
    }
    fmx[slot].m = *m;
}
/* f é sempre um objeto novo aqui: grava a matriz pedida, ou herda a de src */
static id ct_with_matrix(id f, const void *matrix, id src) {
    ct_mx_t m;
    if (matrix) ct_font_set_matrix(f, (const ct_mx_t *)matrix);
    else if (src && ct_font_matrix(src, &m)) ct_font_set_matrix(f, &m);
    return f;
}

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
static u8 *user_bytes[USER_MAX];
static long user_nbytes[USER_MAX];
static int ct_store_user_bytes(int fid, const u8 *b, long n);
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
    if (size <= 0) size = 12;
    id f = name ? MSG(id, cls("NSFont"), "fontWithName:size:", name, size) : 0;
    return objc_retain(ct_with_matrix(f ? f : font_of_size(size, 0), matrix, 0));
}
id CTFontCreateWithFontDescriptor(id desc, double size, const void *matrix) { return objc_retain(ct_with_matrix(font_from_desc(desc, size), matrix, 0)); }
id CTFontCreateUIFontForLanguage(u32 type, double size, id lang) { (void)type; (void)lang; return objc_retain(font_of_size(size > 0 ? size : 13, 0)); }
/* O Skia copia toda fonte para o tamanho do texto com um descritor que só traz variações
 * (SkCTFontCreateExactCopy), e o Blink pede o peso da fonte do sistema do mesmo jeito: um descritor sem
 * nome mantém a face de f (antes virava a regular, e uma fonte web virava a Inter). */
id CTFontCreateCopyWithAttributes(id f, double size, const void *matrix, id desc) {
    double s = size > 0 ? size : fsize(f);
    if (desc && (desc_user_id(desc) >= 0 || MSG(id, desc, "objectForKey:", (id)kCTFontNameAttribute) ||
                 MSG(id, desc, "objectForKey:", (id)kCTFontFamilyNameAttribute)))
        return objc_retain(ct_with_matrix(font_from_desc(desc, s), matrix, f));
    int w = desc_weight(desc);
    /* sem mudança devolve a própria f; com matriz nova é preciso outro objeto */
    if (w < 0 && !matrix && (size <= 0 || size == fsize(f)) && f) return objc_retain(f);
    id n = font_copy(f, s);
    int fw = fweight_raw(n);
    if (n && w >= 0 && fw >= 0 && fw <= 3) ftf(n)->weight = w;
    return objc_retain(ct_with_matrix(n, matrix, f));
}
id CTFontCreateCopyWithSymbolicTraits(id f, double size, const void *matrix, u32 value, u32 mask) {
    double s = size > 0 ? size : fsize(f);
    id n = font_copy(f, s);
    int fw = fweight_raw(n);
    if (n && fw >= 0 && fw <= 3 && (mask & TRAIT_BOLD)) ftf(n)->weight = (value & TRAIT_BOLD) ? 3 : 0;
    return objc_retain(ct_with_matrix(n, matrix, f));
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
    ct_store_user_bytes(fid, b, n);
    user_names_load(fid);
    id d = MSG0(id, cls("NSMutableDictionary"), "dictionary");
    MSG(void, d, "setObject:forKey:", cx_str(unames[i].ps), (id)kCTFontNameAttribute);
    MSG(void, d, "setObject:forKey:", cx_str(unames[i].family), (id)kCTFontFamilyNameAttribute);
    MSG(void, d, "setObject:forKey:", cx_str(unames[i].full), (id)kCTFontDisplayNameAttribute);
    MSG(void, d, "setObject:forKey:", cx_num_i(fid), cx_str(USER_ID_KEY));
    return objc_retain(d);
}
static const u8 *sfnt_find(const u8 *b, long n, u32 tag, long *len) {
    u32 nt, i, off, ln;
    if (len) *len = 0;
    if (!b || n < 12) return 0;
    nt = rd16(b + 4);
    if (12 + (long)nt * 16 > n) return 0;
    for (i = 0; i < nt; i++) {
        const u8 *r = b + 12 + i * 16;
        u32 tg = ((u32)r[0] << 24) | ((u32)r[1] << 16) | ((u32)r[2] << 8) | r[3];
        off = ((u32)r[8] << 24) | ((u32)r[9] << 16) | ((u32)r[10] << 8) | r[11];
        ln = ((u32)r[12] << 24) | ((u32)r[13] << 16) | ((u32)r[14] << 8) | r[15];
        if (tg == tag && (long)off + (long)ln <= n) {
            if (len) *len = (long)ln;
            return b + off;
        }
    }
    return 0;
}
static int ct_store_user_bytes(int fid, const u8 *b, long n) {
    int i = fid - USER_BASE;
    u8 *c;
    if (!is_user(fid) || !b || n < 12) return 0;
    c = malloc((size_t)n);
    if (!c) return 0;
    memcpy(c, b, (size_t)n);
    free(user_bytes[i]);
    user_bytes[i] = c;
    user_nbytes[i] = n;
    {
        long nl = 0;
        const u8 *nm = sfnt_find(c, n, 0x6E616D65u, &nl);
        if (nm) {
            name_entry(nm, nl, 6, unames[i].ps, sizeof unames[i].ps);
            name_entry(nm, nl, 1, unames[i].family, sizeof unames[i].family);
            name_entry(nm, nl, 4, unames[i].full, sizeof unames[i].full);
        }
    }
    if (!unames[i].ps[0]) {
        unames[i].ps[0] = 'F';
        unames[i].ps[1] = (char)('0' + i % 10);
        unames[i].ps[2] = 0;
    }
    if (!unames[i].family[0]) memcpy(unames[i].family, unames[i].ps, sizeof unames[i].ps);
    if (!unames[i].full[0]) memcpy(unames[i].full, unames[i].family, sizeof unames[i].family);
    return 1;
}
static int ct_register_local(const u8 *b, long n) {
    int i;
    for (i = 0; i < USER_MAX; i++) if (!user_bytes[i]) break;
    if (i == USER_MAX) return -1;
    if (!ct_store_user_bytes(USER_BASE + i, b, n)) return -1;
    return USER_BASE + i;
}
static id ct_desc_for_fid(int fid) {
    int i = fid - USER_BASE;
    id d = MSG0(id, cls("NSMutableDictionary"), "dictionary");
    if (!d) d = MSG0(id, cls("NSDictionary"), "dictionary");
    if (!d) return 0;
    MSG(void, d, "setObject:forKey:", cx_str(unames[i].ps), (id)kCTFontNameAttribute);
    MSG(void, d, "setObject:forKey:", cx_str(unames[i].family), (id)kCTFontFamilyNameAttribute);
    MSG(void, d, "setObject:forKey:", cx_str(unames[i].full), (id)kCTFontDisplayNameAttribute);
    MSG(void, d, "setObject:forKey:", cx_num_i(fid), cx_str(USER_ID_KEY));
    return objc_retain(d);
}
static void *ct_state(id o);
void cx_object_free(id o);
void *calloc(size_t n, size_t s);
static cx_class_t CGFont_c, CGFont_m;
static int cgfont_ready;
typedef struct { u8 *b; long n; int fid; } cgf_t;
static void cgfont_dealloc(id self, SEL c) {
    cgf_t *g = ct_state(self);
    (void)c;
    if (g) { free(g->b); free(g); }
    *(void **)((u8 *)self + 8) = 0;
    cx_object_free(self);
}
static void cgfont_init_cls(void) {
    cx_class_t *ns;
    if (cgfont_ready) return;
    ns = cx_class_named("NSObject");
    if (!ns) return;
    cx_define_class(&CGFont_c, &CGFont_m, "CGFont", ns, 8);
    cx_add_method(&CGFont_c, "dealloc", (void *)cgfont_dealloc);
    cgfont_ready = 1;
}
void *CGDataProviderCopyData(void *p);
/* CGFont com uma cópia dos bytes sfnt; fid = face do kernel já conhecida (peso 0..3 ou fonte registrada) ou -1 */
static id cgfont_from_bytes(const u8 *b, long n, int fid) {
    cgf_t *g;
    id o;
    cgfont_init_cls();
    if (!cgfont_ready || !b || n < 12 || n > (8 << 20)) return 0;
    g = calloc(1, sizeof *g);
    if (!g) return 0;
    g->b = malloc((size_t)n);
    if (!g->b) { free(g); return 0; }
    memcpy(g->b, b, (size_t)n);
    g->n = n;
    g->fid = fid;
    o = cx_alloc_instance(&CGFont_c);
    if (!o) { free(g->b); free(g); return 0; }
    *(void **)((u8 *)o + 8) = g;
    return o;
}
void *CGFontCreateWithDataProvider(void *prov) {
    id data;
    const u8 *b;
    long n;
    id o;
    if (!prov) return 0;
    data = (id)CGDataProviderCopyData(prov);
    b = data ? CFDataGetBytePtr(data) : 0;
    n = data ? CFDataGetLength(data) : 0;
    o = cgfont_from_bytes(b, n, -1);
    objc_release(data);
    return o;
}

/* ---- CGFont a partir de uma face do kernel, e leitura das tabelas sfnt do CGFont */
static u32 cgf_rd32(const u8 *p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }
static u32 cgf_rd16(const u8 *p) { return (u32)p[0] << 8 | p[1]; }
static void cgf_wr32(u8 *p, u32 v) { p[0] = (u8)(v >> 24); p[1] = (u8)(v >> 16); p[2] = (u8)(v >> 8); p[3] = (u8)v; }
static void cgf_wr16(u8 *p, u32 v) { p[0] = (u8)(v >> 8); p[1] = (u8)v; }
/* tabela 'tag' de um arquivo sfnt (ou da primeira fonte de uma coleção 'ttcf'); 0 se não existe */
static const u8 *cgf_table(const u8 *b, long n, u32 tag, long *len) {
    long base = 0;
    if (!b || n < 12) return 0;
    if (cgf_rd32(b) == 0x74746366u) {                         /* 'ttcf' */
        if (n < 16) return 0;
        base = (long)cgf_rd32(b + 12);
        if (base < 0 || base + 12 > n) return 0;
    }
    u32 nt = cgf_rd16(b + base + 4);
    for (u32 i = 0; i < nt; i++) {
        long r = base + 12 + (long)i * 16;
        if (r + 16 > n) break;
        if (cgf_rd32(b + r) != tag) continue;
        long off = (long)cgf_rd32(b + r + 8), l = (long)cgf_rd32(b + r + 12);
        if (off < 0 || l < 0 || off + l > n) return 0;
        *len = l;
        return b + off;
    }
    return 0;
}
static u32 cgf_checksum(const u8 *p, long n) {
    u32 s = 0;
    for (long i = 0; i < n; i += 4) {
        u8 w[4] = {0, 0, 0, 0};
        for (int k = 0; k < 4 && i + k < n; k++) w[k] = p[i + k];
        s += cgf_rd32(w);
    }
    return s;
}
/* O kernel entrega as tabelas da face uma a uma (GUI_FONT/GF_TABLE). Remonta o arquivo sfnt: diretório
 * com as tabelas em ordem de tag, cada uma alinhada a 4 bytes e com o checksum. */
static id cgfont_from_kernel_face(int w) {
    static const u32 tags[] = {0x43464620u /* CFF  */, 0x4744454Fu /* GDEF */, 0x47504F53u /* GPOS */, 0x47535542u /* GSUB */,
                               0x4F532F32u /* OS/2 */, 0x636D6170u /* cmap */, 0x63767420u /* cvt  */, 0x6670676Du /* fpgm */,
                               0x67617370u /* gasp */, 0x676C7966u /* glyf */, 0x68656164u /* head */, 0x68686561u /* hhea */,
                               0x686D7478u /* hmtx */, 0x6B65726Eu /* kern */, 0x6C6F6361u /* loca */, 0x6D617870u /* maxp */,
                               0x6E616D65u /* name */, 0x706F7374u /* post */, 0x70726570u /* prep */};
    enum { NT = sizeof tags / sizeof tags[0] };
    long len[NT], total, off;
    u32 nt = 0, cff = 0;
    if (!font_live()) return 0;
    total = 12;
    for (u32 i = 0; i < NT; i++) {
        long l = SYS5(GUI_FONT, GF_TABLE, w, tags[i], 0, 0);
        len[i] = (l > 0 && l <= (8 << 20)) ? l : 0;
        if (!len[i]) continue;
        nt++;
        if (tags[i] == 0x43464620u) cff = 1;
        total += 16 + ((len[i] + 3) & ~3L);
    }
    if (!nt || total > (8 << 20)) return 0;
    u8 *f = calloc(1, (size_t)total);
    if (!f) return 0;
    u32 es = 0;
    while ((2u << es) <= nt) es++;
    cgf_wr32(f, cff ? 0x4F54544Fu /* 'OTTO' */ : 0x00010000u);
    cgf_wr16(f + 4, nt);
    cgf_wr16(f + 6, (1u << es) * 16);
    cgf_wr16(f + 8, es);
    cgf_wr16(f + 10, nt * 16 - (1u << es) * 16);
    off = 12 + (long)nt * 16;
    u32 r = 0;
    for (u32 i = 0; i < NT; i++) {
        if (!len[i]) continue;
        long got = SYS5(GUI_FONT, GF_TABLE, w, tags[i], f + off, len[i]);
        if (got <= 0) { free(f); return 0; }
        if (got < len[i]) len[i] = got;
        u8 *rec = f + 12 + (long)r * 16;
        cgf_wr32(rec, tags[i]);
        cgf_wr32(rec + 4, cgf_checksum(f + off, len[i]));
        cgf_wr32(rec + 8, (u32)off);
        cgf_wr32(rec + 12, (u32)len[i]);
        off += (len[i] + 3) & ~3L;
        r++;
    }
    id o = cgfont_from_bytes(f, off, w);
    free(f);
    return o;
}
static id cgfont_from_face(int w) {
    if (is_user(w)) {
        int i = w - USER_BASE;
        if (user_bytes[i] && user_nbytes[i] > 0) return cgfont_from_bytes(user_bytes[i], user_nbytes[i], w);
    }
    if ((w >= 0 && w <= 3) || is_user(w)) return cgfont_from_kernel_face(w);
    return 0;
}
/* Nome PostScript ou nome completo de uma fonte registrada (CTFontManagerRegister*) e, para o resto, a
 * mesma resolução do NSFont/CTFontCreateWithName do mOS: todo nome do sistema é uma face da fonte do kernel. */
void *CGFontCreateWithFontName(id name) {
    const char *n = name && cx_is_str(name) ? cx_cstr(name) : 0;
    if (!n || !n[0]) return 0;
    for (int i = 0; i < USER_MAX; i++)
        if (user_bytes[i] && (!strcmp(unames[i].ps, n) || !strcmp(unames[i].full, n))) return cgfont_from_face(USER_BASE + i);
    id f = MSG(id, cls("NSFont"), "fontWithName:size:", name, 12.0);
    return f ? cgfont_from_face(fweight(f)) : 0;
}
/* O ATSFontRef do mOS é o número da face no kernel (o "peso" do GUI_FONT: 0..3 ou o id de uma fonte
 * registrada). Referência que não é uma face conhecida não vira fonte, como um ATSFontRef inválido. */
void *CGFontCreateWithPlatformFont(void *platformFontReference) {
    if (!platformFontReference) return 0;
    return cgfont_from_face((int)*(const u32 *)platformFontReference);
}
void *CGFontRetain(void *font) { return font ? (void *)objc_retain((id)font) : 0; }
static cgf_t *cgf_of(void *font) {
    if (!font || !cgfont_ready || object_getClass((id)font) != &CGFont_c) return 0;
    cgf_t *g = ct_state((id)font);
    return g && g->b ? g : 0;
}
/* 'maxp'.numGlyphs; um CTFont/NSFont no lugar do CGFont responde pela própria contagem */
size_t CGFontGetNumberOfGlyphs(void *font) {
    cgf_t *g = cgf_of(font);
    long l = 0;
    if (!g) return font ? (size_t)CTFontGetGlyphCount((id)font) : 0;
    const u8 *t = cgf_table(g->b, g->n, 0x6D617870u, &l);
    return t && l >= 6 ? cgf_rd16(t + 4) : 0;
}
/* nameID 6 da tabela 'name' */
id CGFontCopyPostScriptName(void *font) {
    cgf_t *g = cgf_of(font);
    long l = 0;
    char ps[128];
    if (!g) return font ? CTFontCopyPostScriptName((id)font) : 0;
    const u8 *t = cgf_table(g->b, g->n, 0x6E616D65u, &l);
    if (!t) return 0;
    name_entry(t, l, 6, ps, (int)sizeof ps);
    return ps[0] ? objc_retain(cx_str(ps)) : 0;
}
/* os 258 nomes do conjunto padrão do Macintosh ('post' formatos 1 e 2) */
static const char *const mac_glyph_names[258] = {
    ".notdef", ".null", "nonmarkingreturn", "space", "exclam", "quotedbl", "numbersign", "dollar", "percent",
    "ampersand", "quotesingle", "parenleft", "parenright", "asterisk", "plus", "comma", "hyphen", "period", "slash",
    "zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "colon", "semicolon", "less",
    "equal", "greater", "question", "at", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O",
    "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "bracketleft", "backslash", "bracketright", "asciicircum",
    "underscore", "grave", "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r",
    "s", "t", "u", "v", "w", "x", "y", "z", "braceleft", "bar", "braceright", "asciitilde", "Adieresis", "Aring",
    "Ccedilla", "Eacute", "Ntilde", "Odieresis", "Udieresis", "aacute", "agrave", "acircumflex", "adieresis", "atilde",
    "aring", "ccedilla", "eacute", "egrave", "ecircumflex", "edieresis", "iacute", "igrave", "icircumflex", "idieresis",
    "ntilde", "oacute", "ograve", "ocircumflex", "odieresis", "otilde", "uacute", "ugrave", "ucircumflex", "udieresis",
    "dagger", "degree", "cent", "sterling", "section", "bullet", "paragraph", "germandbls", "registered", "copyright",
    "trademark", "acute", "dieresis", "notequal", "AE", "Oslash", "infinity", "plusminus", "lessequal", "greaterequal",
    "yen", "mu", "partialdiff", "summation", "product", "pi", "integral", "ordfeminine", "ordmasculine", "Omega", "ae",
    "oslash", "questiondown", "exclamdown", "logicalnot", "radical", "florin", "approxequal", "Delta", "guillemotleft",
    "guillemotright", "ellipsis", "nonbreakingspace", "Agrave", "Atilde", "Otilde", "OE", "oe", "endash", "emdash",
    "quotedblleft", "quotedblright", "quoteleft", "quoteright", "divide", "lozenge", "ydieresis", "Ydieresis",
    "fraction", "currency", "guilsinglleft", "guilsinglright", "fi", "fl", "daggerdbl", "periodcentered",
    "quotesinglbase", "quotedblbase", "perthousand", "Acircumflex", "Ecircumflex", "Aacute", "Edieresis", "Egrave",
    "Iacute", "Icircumflex", "Idieresis", "Igrave", "Oacute", "Ocircumflex", "apple", "Ograve", "Uacute",
    "Ucircumflex", "Ugrave", "dotlessi", "circumflex", "tilde", "macron", "breve", "dotaccent", "ring", "cedilla",
    "hungarumlaut", "ogonek", "caron", "Lslash", "lslash", "Scaron", "scaron", "Zcaron", "zcaron", "brokenbar", "Eth",
    "eth", "Yacute", "yacute", "Thorn", "thorn", "minus", "multiply", "onesuperior", "twosuperior", "threesuperior",
    "onehalf", "onequarter", "threequarters", "franc", "Gbreve", "gbreve", "Idotaccent", "Scedilla", "scedilla",
    "Cacute", "cacute", "Ccaron", "ccaron", "dcroat",
};
_Static_assert(sizeof mac_glyph_names / sizeof mac_glyph_names[0] == 258, "conjunto padrão do Macintosh");
/* Nome do glifo pela tabela 'post': formato 1 (ordem padrão do Macintosh) e 2 (índice padrão ou string
 * Pascal própria). Formato 3 (sem nomes, o comum em CFF/OTF) e glifo fora da fonte: NULL, como a Apple. */
id CGFontCopyGlyphNameForGlyph(void *font, u16 glyph) {
    cgf_t *g = cgf_of(font);
    long l = 0;
    if (!g) return 0;
    const u8 *t = cgf_table(g->b, g->n, 0x706F7374u, &l);
    if (!t || l < 32) return 0;
    u32 fmt = cgf_rd32(t);
    if (fmt == 0x00010000u) return glyph < 258 ? objc_retain(cx_str(mac_glyph_names[glyph])) : 0;
    if (fmt != 0x00020000u || l < 34) return 0;
    u32 ng = cgf_rd16(t + 32);
    if (glyph >= ng || 34 + (long)ng * 2 > l) return 0;
    u32 idx = cgf_rd16(t + 34 + glyph * 2);
    if (idx < 258) return objc_retain(cx_str(mac_glyph_names[idx]));
    idx -= 258;
    const u8 *p = t + 34 + (long)ng * 2, *end = t + l;
    for (u32 k = 0; k < idx; k++) {
        if (p >= end) return 0;
        p += 1 + *p;
    }
    if (p >= end || p + 1 + *p > end) return 0;
    return objc_retain(cx_str_n((const char *)p + 1, *p));
}
int CTFontManagerRegisterGraphicsFont(void *font, id *error) {
    cgf_t *g;
    int fid = -1;
    if (error) *error = 0;
    if (!font || object_getClass((id)font) != &CGFont_c) return 0;
    g = ct_state((id)font);
    if (!g || !g->b) return 0;
#if !defined(CT_HOST)
    if (font_live()) {
        id data = CFDataCreate(0, g->b, g->n);
        id d = data ? CTFontManagerCreateFontDescriptorFromData(data) : 0;
        objc_release(data);
        if (d) {
            id num = MSG(id, d, "objectForKey:", cx_str(USER_ID_KEY));
            if (num) fid = (int)MSG0(i64, num, "longLongValue");
            objc_release(d);
        }
    }
#endif
    if (!is_user(fid)) fid = ct_register_local(g->b, g->n);
    if (!is_user(fid)) return 0;
    g->fid = fid;
    return 1;
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
ct_mx_t CTFontGetMatrix(id f) {
    ct_mx_t m;
    ct_font_matrix(f, &m);
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
/* tabela sfnt da fonte: dos bytes guardados da fonte registrada, ou a cópia que o kernel dá da face
 * (GF_TABLE). *own recebe o buffer a liberar (0 quando aponta para user_bytes). */
static const u8 *ct_font_table(id f, u32 tag, long *len, u8 **own) {
    int w = fweight_raw(f);
    *own = 0;
    *len = 0;
    if (is_user(w) && user_bytes[w - USER_BASE])
        return sfnt_find(user_bytes[w - USER_BASE], user_nbytes[w - USER_BASE], tag, len);
    if (f && font_live()) {
        int face = w >= 0 ? w : fweight(f);
        long n = SYS5(GUI_FONT, GF_TABLE, face, tag, 0, 0);
        u8 *b = (n > 0 && n <= (8 << 20)) ? malloc((size_t)n) : 0;
        long n2 = b ? SYS5(GUI_FONT, GF_TABLE, face, tag, b, n) : 0;
        if (n2 > 0) {
            *own = b;
            *len = n2 < n ? n2 : n;
            return b;
        }
        free(b);
    }
    return 0;
}
static u32 rd32(const u8 *p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }
static int rds16(const u8 *p) { return (int)(i16)rd16(p); }
typedef struct { const u8 *p; long n; u8 *own; } ct_tab_t;
static void ct_tab(id f, u32 tag, ct_tab_t *t) { t->p = ct_font_table(f, tag, &t->n, &t->own); }
/* yMax do glifo g pelo loca/glyf; 0 = glifo vazio ou tabela ausente */
static int ct_glyph_ymax(const ct_tab_t *head, const ct_tab_t *loca, const ct_tab_t *glyf, u32 g, int *ymax) {
    long a, b;
    if (!head->p || head->n < 54 || !loca->p || !glyf->p) return 0;
    if (rds16(head->p + 50) == 0) {
        if ((long)(g + 2) * 2 > loca->n) return 0;
        a = (long)rd16(loca->p + g * 2) * 2;
        b = (long)rd16(loca->p + g * 2 + 2) * 2;
    } else {
        if ((long)(g + 2) * 4 > loca->n) return 0;
        a = (long)rd32(loca->p + g * 4);
        b = (long)rd32(loca->p + g * 4 + 4);
    }
    if (b <= a || a + 10 > glyf->n) return 0;
    *ymax = rds16(glyf->p + a + 8);
    return 1;
}
/* Translação do glifo para o desenho vertical, como o CoreText: x = -(avanço horizontal)/2 e
 * y = -(origem vertical). A origem vertical vem do VORG; senão de yMax + topSideBearing (vmtx + glyf);
 * senão do ascender do hhea (ou do ascent do CTFont, sem tabela). */
void CTFontGetVerticalTranslationsForGlyphs(id f, int orientation, const u16 *glyphs, ct_sz_t *trans, long n) {
    ct_tab_t head, hhea, hmtx, vhea, vmtx, vorg, loca, glyf;
    double scale, asc;
    u32 nhm = 0, nvm = 0;
    (void)orientation;
    if (!trans || n <= 0) return;
    ct_tab(f, 0x68656164u, &head);   /* head */
    ct_tab(f, 0x68686561u, &hhea);   /* hhea */
    ct_tab(f, 0x686D7478u, &hmtx);   /* hmtx */
    ct_tab(f, 0x76686561u, &vhea);   /* vhea */
    ct_tab(f, 0x766D7478u, &vmtx);   /* vmtx */
    ct_tab(f, 0x564F5247u, &vorg);   /* VORG */
    ct_tab(f, 0x6C6F6361u, &loca);   /* loca */
    ct_tab(f, 0x676C7966u, &glyf);   /* glyf */
    {
        u32 upem = head.p && head.n >= 20 ? rd16(head.p + 18) : 0;
        scale = upem ? fsize(f) / upem : 0;
    }
    if (hhea.p && hhea.n >= 36) nhm = rd16(hhea.p + 34);
    if (vhea.p && vhea.n >= 36) nvm = rd16(vhea.p + 34);
    asc = scale && hhea.p && hhea.n >= 6 ? rds16(hhea.p + 4) * scale : CTFontGetAscent(f);
    for (long i = 0; i < n; i++) {
        u32 g = glyphs ? glyphs[i] : 0;
        double adv, oy = asc;
        int ymax;
        if (scale && nhm && hmtx.p && (long)nhm * 4 <= hmtx.n)
            adv = rd16(hmtx.p + (g < nhm ? g : nhm - 1) * 4) * scale;
        else {
            ct_sz_t sz;
            u16 gl = (u16)g;
            CTFontGetAdvancesForGlyphs(f, 0, &gl, &sz, 1);
            adv = sz.w;
        }
        if (scale && vorg.p && vorg.n >= 8) {
            u32 cnt = rd16(vorg.p + 6), k;
            int y = rds16(vorg.p + 4);
            for (k = 0; k < cnt && 8 + (long)(k + 1) * 4 <= vorg.n; k++)
                if (rd16(vorg.p + 8 + k * 4) == g) { y = rds16(vorg.p + 8 + k * 4 + 2); break; }
            oy = y * scale;
        } else if (scale && nvm && vmtx.p && ct_glyph_ymax(&head, &loca, &glyf, g, &ymax)) {
            long at = g < nvm ? (long)g * 4 + 2 : (long)nvm * 4 + (long)(g - nvm) * 2;
            if (at + 2 <= vmtx.n) oy = (ymax + rds16(vmtx.p + at)) * scale;
        }
        trans[i].w = -adv / 2;
        trans[i].h = -oy;
    }
    free(head.own); free(hhea.own); free(hmtx.own); free(vhea.own);
    free(vmtx.own); free(vorg.own); free(loca.own); free(glyf.own);
}
static id ct_feat_dict(u32 ident, const char *name) {
    id d = MSG0(id, cls("NSMutableDictionary"), "dictionary");
    if (!d) d = MSG0(id, cls("NSDictionary"), "dictionary");
    if (!d) return 0;
    MSG(void, d, "setObject:forKey:", cx_num_i((i64)ident), (id)kCTFontFeatureTypeIdentifierKey);
    if (name && name[0]) MSG(void, d, "setObject:forKey:", cx_str(name), (id)kCTFontFeatureTypeNameKey);
    return d;
}
/* seletor dentro de typeDict; devolve o dicionário do seletor (sem retenção extra) */
static id ct_feat_add_sel(id typeDict, u32 ident, const char *name) {
    id arr, sel;
    if (!typeDict) return 0;
    arr = MSG(id, typeDict, "objectForKey:", (id)kCTFontFeatureTypeSelectorsKey);
    if (!arr) {
        arr = MSG0(id, cls("NSMutableArray"), "array");
        if (arr) MSG(void, typeDict, "setObject:forKey:", arr, (id)kCTFontFeatureTypeSelectorsKey);
    }
    sel = MSG0(id, cls("NSMutableDictionary"), "dictionary");
    if (!sel) return 0;
    /* o seletor usa as chaves de seletor, não as de tipo */
    MSG(void, sel, "setObject:forKey:", cx_num_i((i64)ident), (id)kCTFontFeatureSelectorIdentifierKey);
    if (name && name[0]) MSG(void, sel, "setObject:forKey:", cx_str(name), (id)kCTFontFeatureSelectorNameKey);
    if (!arr) return 0;
    MSG(void, arr, "addObject:", sel);  /* dictionary é autoreleased: quem segura é o array */
    return sel;
}
static void ct_name_by_id(const u8 *name, long nn, u32 idn, char *out, int cap) {
    name_entry(name, nn, idn, out, cap);
}
/* feature OpenType → tipo/seletores AAT (SFNTLayoutTypes.h), como o CoreText as expõe; on/off = -1 não
 * existe. excl: o tipo é exclusivo (um seletor por vez). */
typedef struct { char tag[5]; u16 type; i16 on, off; u8 excl, dflt; const char *tname, *sname; } ct_otmap_t;
static const ct_otmap_t ct_otmap[] = {
    {"liga", 1, 2, 3, 0, 1, "Ligatures", "Common Ligatures"},
    {"rlig", 1, 0, 1, 0, 1, "Ligatures", "Required Ligatures"},
    {"dlig", 1, 4, 5, 0, 0, "Ligatures", "Rare Ligatures"},
    {"clig", 1, 18, 19, 0, 1, "Ligatures", "Contextual Ligatures"},
    {"hlig", 1, 20, 21, 0, 0, "Ligatures", "Historical Ligatures"},
    {"tnum", 6, 0, -1, 1, 0, "Number Spacing", "Monospaced Numbers"},
    {"pnum", 6, 1, -1, 1, 0, "Number Spacing", "Proportional Numbers"},
    {"sups", 10, 1, -1, 1, 0, "Vertical Position", "Superiors"},
    {"subs", 10, 2, -1, 1, 0, "Vertical Position", "Inferiors"},
    {"ordn", 10, 3, -1, 1, 0, "Vertical Position", "Ordinals"},
    {"frac", 11, 2, -1, 1, 0, "Fractions", "Diagonal Fractions"},
    {"zero", 14, 4, 5, 0, 0, "Typographic Extras", "Slashed Zero"},
    {"onum", 21, 0, -1, 1, 0, "Number Case", "Lower Case Numbers"},
    {"lnum", 21, 1, -1, 1, 0, "Number Case", "Upper Case Numbers"},
    {"case", 33, 0, 1, 0, 0, "Case Sensitive Layout", "Case Sensitive Layout"},
    {"calt", 36, 0, 1, 0, 1, "Contextual Alternates", "Contextual Alternates"},
    {"smcp", 37, 1, -1, 1, 0, "Lower Case", "Small Capitals"},
    {"c2sc", 38, 1, -1, 1, 0, "Upper Case", "Small Capitals"},
};
#define CT_SS_TYPE 35u   /* kStylisticAlternativesType: ssNN = seletores 2NN (on) e 2NN+1 (off) */
typedef struct { u32 type; id dict; } ct_ftype_t;
static id ct_ftype_get(id arr, ct_ftype_t *ft, int *nft, int cap, u32 type, const char *tname, int excl, const char *tag) {
    id d;
    for (int i = 0; i < *nft; i++) if (ft[i].type == type) return ft[i].dict;
    if (*nft >= cap) return 0;
    d = ct_feat_dict(type, tname);
    if (!d) return 0;
    MSG(void, d, "setObject:forKey:", cx_num_i(excl ? 1 : 0), (id)kCTFontFeatureTypeExclusiveKey);
    if (tag) MSG(void, d, "setObject:forKey:", cx_str(tag), (id)kCTFontOpenTypeFeatureTag);
    MSG(void, arr, "addObject:", d);
    ft[*nft].type = type;
    ft[*nft].dict = d;
    (*nft)++;
    return d;
}
static void ct_sel_ot(id sel, const char *tag, int value, int dflt) {
    if (!sel) return;
    MSG(void, sel, "setObject:forKey:", cx_str(tag), (id)kCTFontOpenTypeFeatureTag);
    MSG(void, sel, "setObject:forKey:", cx_num_i(value), (id)kCTFontOpenTypeFeatureValue);
    if (dflt) MSG(void, sel, "setObject:forKey:", cx_num_i(1), (id)kCTFontFeatureSelectorDefaultKey);
}
/* FeatureList de um GSUB/GPOS (offset no byte 6 do cabeçalho) */
static void ct_features_ot(id arr, ct_ftype_t *ft, int *nft, int cap, const u8 *t, long tl) {
    u32 foff, fc, i;
    if (!t || tl < 10) return;
    foff = rd16(t + 6);
    if (!foff || foff + 2 > (u32)tl) return;
    fc = rd16(t + foff);
    if (fc > 512) fc = 512;
    for (i = 0; i < fc && foff + 2 + (i + 1) * 6 <= (u32)tl; i++) {
        const u8 *rec = t + foff + 2 + i * 6;
        char tag[5];
        const ct_otmap_t *m = 0;
        u32 k;
        id td, sel;
        tag[0] = (char)rec[0]; tag[1] = (char)rec[1]; tag[2] = (char)rec[2]; tag[3] = (char)rec[3]; tag[4] = 0;
        for (k = 0; k < sizeof ct_otmap / sizeof ct_otmap[0]; k++)
            if (!strncmp(ct_otmap[k].tag, tag, 4)) { m = &ct_otmap[k]; break; }
        if (m) {
            /* já visto (a FeatureList repete a tag por script/idioma)? */
            td = ct_ftype_get(arr, ft, nft, cap, m->type, m->tname, m->excl, 0);
            if (!td) continue;
            {
                id sels = MSG(id, td, "objectForKey:", (id)kCTFontFeatureTypeSelectorsKey);
                u64 ns = sels ? MSG0(u64, sels, "count") : 0, j;
                int dup = 0;
                for (j = 0; j < ns && !dup; j++) {
                    id sd = MSG(id, sels, "objectAtIndex:", j);
                    id st = sd ? MSG(id, sd, "objectForKey:", (id)kCTFontOpenTypeFeatureTag) : 0;
                    if (st && cx_is_str(st) && !strcmp(cx_cstr(st), tag)) dup = 1;
                }
                if (dup) continue;
            }
            sel = ct_feat_add_sel(td, (u32)m->on, m->sname);
            ct_sel_ot(sel, tag, 1, m->dflt);
            if (m->off >= 0) {
                char off[64];
                int L = 0;
                for (const char *p = m->sname; *p && L < 58; p++) off[L++] = *p;
                memcpy(off + L, " Off", 5);
                sel = ct_feat_add_sel(td, (u32)m->off, off);
                ct_sel_ot(sel, tag, 0, !m->dflt);
            }
        } else if (tag[0] == 's' && tag[1] == 's' && tag[2] >= '0' && tag[2] <= '9' && tag[3] >= '0' && tag[3] <= '9') {
            u32 nn = (u32)(tag[2] - '0') * 10 + (u32)(tag[3] - '0');
            char sn[24];
            td = ct_ftype_get(arr, ft, nft, cap, CT_SS_TYPE, "Alternative Stylistic Sets", 0, 0);
            if (!td || !nn || nn > 20) continue;
            memcpy(sn, "Stylistic Set ", 14);
            sn[14] = tag[2]; sn[15] = tag[3]; sn[16] = 0;
            {
                id sels = MSG(id, td, "objectForKey:", (id)kCTFontFeatureTypeSelectorsKey);
                u64 ns = sels ? MSG0(u64, sels, "count") : 0, j;
                int dup = 0;
                for (j = 0; j < ns && !dup; j++) {
                    id sd = MSG(id, sels, "objectAtIndex:", j);
                    id st = sd ? MSG(id, sd, "objectForKey:", (id)kCTFontOpenTypeFeatureTag) : 0;
                    if (st && cx_is_str(st) && !strcmp(cx_cstr(st), tag)) dup = 1;
                }
                if (dup) continue;
            }
            sel = ct_feat_add_sel(td, nn * 2, sn);
            ct_sel_ot(sel, tag, 1, 0);
            memcpy(sn + 16, " Off", 5);
            sel = ct_feat_add_sel(td, nn * 2 + 1, sn);
            ct_sel_ot(sel, tag, 0, 1);
        } else {
            /* sem tipo AAT: o tipo é a própria tag (como o CoreText faz com as features só OpenType) */
            u32 four = rd32(rec);
            td = ct_ftype_get(arr, ft, nft, cap, four, tag, 0, tag);
            if (!td || MSG(id, td, "objectForKey:", (id)kCTFontFeatureTypeSelectorsKey)) continue;
            {
                int on = !strncmp(tag, "kern", 4) || !strncmp(tag, "ccmp", 4) || !strncmp(tag, "locl", 4) ||
                         !strncmp(tag, "mark", 4) || !strncmp(tag, "mkmk", 4);
                sel = ct_feat_add_sel(td, 1, "On");
                ct_sel_ot(sel, tag, 1, on);
                sel = ct_feat_add_sel(td, 0, "Off");
                ct_sel_ot(sel, tag, 0, !on);
            }
        }
    }
}
static id ct_features_build(const u8 *name, long nl, const u8 *feat, long fl, const u8 *gsub, long gl,
                            const u8 *gpos, long pl) {
    id a = MSG0(id, cls("NSMutableArray"), "array");
    char nm[96];
    if (!a) return 0;
    if (feat && fl >= 12) {
        u32 count = rd16(feat + 4);
        u32 i;
        if (count > 256) count = 256;
        for (i = 0; i < count; i++) {
            const u8 *rec;
            u32 feature, nset, nameIndex, off, flags;
            id td;
            long recAt = 12 + (long)i * 12;
            if (recAt + 12 > fl) break;
            rec = feat + recAt;
            feature = rd16(rec);
            nset = rd16(rec + 2);
            off = rd32(rec + 4);
            flags = rd16(rec + 8);
            nameIndex = rd16(rec + 10);
            nm[0] = 0;
            if (name) ct_name_by_id(name, nl, nameIndex, nm, sizeof nm);
            td = ct_feat_dict(feature, nm[0] ? nm : 0);
            if (!td) continue;
            /* flags: bit 15 = exclusivo; bit 14 = o índice do seletor padrão está no byte baixo */
            MSG(void, td, "setObject:forKey:", cx_num_i((flags & 0x8000u) ? 1 : 0), (id)kCTFontFeatureTypeExclusiveKey);
            if (nset && off + 4 <= (u32)fl) {
                u32 sidx, dflt = (flags & 0x4000u) ? (flags & 0xFFu) : 0;
                if (nset > 64) nset = 64;
                for (sidx = 0; sidx < nset; sidx++) {
                    const u8 *st;
                    u32 sid, sname;
                    char sn[96];
                    id sel;
                    if (off + (sidx + 1) * 4 > (u32)fl) break;
                    st = feat + off + sidx * 4;
                    sid = rd16(st);
                    sname = rd16(st + 2);
                    sn[0] = 0;
                    if (name) ct_name_by_id(name, nl, sname, sn, sizeof sn);
                    sel = ct_feat_add_sel(td, sid, sn);
                    if (sel && sidx == dflt)
                        MSG(void, sel, "setObject:forKey:", cx_num_i(1), (id)kCTFontFeatureSelectorDefaultKey);
                }
            }
            MSG(void, a, "addObject:", td);
            }
    }
    if (MSG0(u64, a, "count") == 0) {
        ct_ftype_t ft[64];
        int nft = 0;
        ct_features_ot(a, ft, &nft, 64, gsub, gl);
        ct_features_ot(a, ft, &nft, 64, gpos, pl);
    }
    return a;
}
/* fonte sem tabela legível: o mínimo que o CoreText relata de qualquer fonte (o tipo "todas as
 * features tipográficas" e as ligaduras obrigatórias/comuns, ligadas por padrão) */
static id ct_features_default(void) {
    id a = MSG0(id, cls("NSMutableArray"), "array");
    id td, sel;
    if (!a) return 0;
    td = ct_feat_dict(0, "All Typographic Features");
    if (td) {
        MSG(void, td, "setObject:forKey:", cx_num_i(0), (id)kCTFontFeatureTypeExclusiveKey);
        sel = ct_feat_add_sel(td, 0, "All Type Features On");
        if (sel) MSG(void, sel, "setObject:forKey:", cx_num_i(1), (id)kCTFontFeatureSelectorDefaultKey);
        ct_feat_add_sel(td, 1, "All Type Features Off");
        MSG(void, a, "addObject:", td);
    }
    td = ct_feat_dict(1, "Ligatures");
    if (td) {
        MSG(void, td, "setObject:forKey:", cx_num_i(0), (id)kCTFontFeatureTypeExclusiveKey);
        sel = ct_feat_add_sel(td, 0, "Required Ligatures");
        if (sel) MSG(void, sel, "setObject:forKey:", cx_num_i(1), (id)kCTFontFeatureSelectorDefaultKey);
        ct_feat_add_sel(td, 1, "Required Ligatures Off");
        sel = ct_feat_add_sel(td, 2, "Common Ligatures");
        if (sel) MSG(void, sel, "setObject:forKey:", cx_num_i(1), (id)kCTFontFeatureSelectorDefaultKey);
        ct_feat_add_sel(td, 3, "Common Ligatures Off");
        MSG(void, a, "addObject:", td);
    }
    return a;
}
id CTFontCopyFeatures(id f) {
    ct_tab_t name, feat, gsub, gpos;
    id a;
    ct_tab(f, 0x6E616D65u, &name);   /* name */
    ct_tab(f, 0x66656174u, &feat);   /* feat */
    ct_tab(f, 0x47535542u, &gsub);   /* GSUB */
    ct_tab(f, 0x47504F53u, &gpos);   /* GPOS */
    a = ct_features_build(name.p, name.n, feat.p, feat.n, gsub.p, gsub.n, gpos.p, gpos.n);
    free(name.own); free(feat.own); free(gsub.own); free(gpos.own);
    if (a && MSG0(u64, a, "count") == 0) a = 0;
    if (!a) a = ct_features_default();
    return a ? objc_retain(a) : 0;
}
id CTFontCreateWithGraphicsFont(void *font, double size, const void *matrix, id attrs) {
    cgf_t *g;
    if (font && object_getClass((id)font) == &CGFont_c) {
        g = ct_state((id)font);
        if (g && is_user(g->fid)) return objc_retain(ct_with_matrix(font_with_face(size > 0 ? size : 13, g->fid, 0), matrix, 0));
    }
    if (attrs) return CTFontCreateWithFontDescriptor(attrs, size, matrix);
    return objc_retain(ct_with_matrix(font_of_size(size > 0 ? size : 13, 0), matrix, 0));
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
    u8 level;       /* nível bidi resolvido: ímpar = direita para esquerda */
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
/* Bidi simplificado (UBA sem embeddings nem isolates): letras hebraicas/árabes e afins são fortes R;
 * dígitos (ASCII e arábico-índicos) são números, que tomam a direção do forte anterior (W2/W7) e sobem
 * um ou dois níveis (I1/I2), mantendo a ordem LTR dentro do RTL; demais letras são L; espaço,
 * pontuação e controles são neutros e ficam com a direção dos vizinhos quando os dois lados concordam,
 * senão com a do parágrafo (N1/N2). O parágrafo é RTL se o primeiro forte for R (P2/P3). Depois
 * reordena (L2): inverte do nível mais alto até o 1. */
static int ct_bidi_class(u32 cp) {                     /* 0 = L, 1 = R, 2 = número, -1 = neutro */
    if ((cp >= '0' && cp <= '9') || (cp >= 0x0660 && cp <= 0x0669) || (cp >= 0x06F0 && cp <= 0x06F9)) return 2;
    if ((cp >= 0x0590 && cp <= 0x08FF) || (cp >= 0xFB1D && cp <= 0xFDFF) || (cp >= 0xFE70 && cp <= 0xFEFF) ||
        (cp >= 0x10800 && cp <= 0x10FFF) || (cp >= 0x1E800 && cp <= 0x1EFFF))
        return 1;
    if (cp < 0x80) return ((cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z')) ? 0 : -1;
    if ((cp >= 0x2000 && cp <= 0x206F) || (cp >= 0x3000 && cp <= 0x303F) || (cp >= 0xA0 && cp <= 0xBF) ||
        cp == 0xD7 || cp == 0xF7 || cp == 0xFFFD)
        return -1;
    return 0;
}
static void ct_bidi(ct_line_t *b) {
    long n = b->n, i, j;
    int base = 0, maxl = 0, strong;
    signed char *d;
    if (n <= 0) return;
    for (i = 0; i < n; i++) {
        int c = ct_bidi_class(b->g[i].cp);
        if (c == 0 || c == 1) { base = c; break; }
    }
    d = malloc((size_t)n);
    if (!d) { for (i = 0; i < n; i++) b->g[i].level = (u8)(base ? 1 : 0); return; }
    /* direção resolvida de fortes e números; neutros = -1 */
    strong = base;
    for (i = 0; i < n; i++) {
        int c = ct_bidi_class(b->g[i].cp);
        if (c == 0 || c == 1) strong = c;
        d[i] = (signed char)(c == 2 ? strong : c);   /* número no contexto R conta como R para os neutros */
    }
    for (i = 0; i < n; i++) {
        int c = ct_bidi_class(b->g[i].cp), lv;
        int dir = d[i];
        u32 cp = b->g[i].cp;
        if (dir < 0) {
            int before = -1, after = -1;
            for (j = i - 1; j >= 0 && before < 0; j--) before = d[j];
            for (j = i + 1; j < n && after < 0; j++) after = d[j];
            if (before < 0) before = base;
            if (after < 0) after = base;
            dir = before == after ? before : base;
            if (cp == '\n' || cp == '\r' || cp == '\t') dir = base;
        }
        if (c == 2) lv = dir ? 2 : (base ? 2 : 0);
        else lv = dir ? 1 : (base ? 2 : 0);
        b->g[i].level = (u8)lv;
        if (lv > maxl) maxl = lv;
    }
    free(d);
    for (int k = maxl; k >= 1; k--) {
        for (i = 0; i < n;) {
            if (b->g[i].level < k) { i++; continue; }
            j = i;
            while (j < n && b->g[j].level >= k) j++;
            for (long x = i, y = j - 1; x < y; x++, y--) {
                ct_clus t = b->g[x];
                b->g[x] = b->g[y];
                b->g[y] = t;
            }
            i = j;
        }
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
    ct_bidi(b);
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
    ct_bidi(b);
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
    u16 *glyphs;
    ct_pt_t *pos;
    long *idxs;
    u32 status;         /* CTRunStatus: 1 = RTL, 2 = índices não monotônicos, 4 = matriz não identidade */
    ct_mx_t matrix;     /* text matrix do run: matriz da fonte × formas verticais × obliquidade */
} ct_run_t;
#define CT_RUN_RTL 1u
#define CT_RUN_NONMONO 2u
#define CT_RUN_MATRIX 4u
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
    if (r) {
        objc_release(r->font);
        objc_release(r->attrs);
        free(r->glyphs);
        free(r->pos);
        free(r->idxs);
        free(r);
    }
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
static ct_mx_t ct_mx_mul(ct_mx_t p, ct_mx_t q) {      /* p seguido de q (CGAffineTransformConcat) */
    ct_mx_t r;
    r.a = p.a * q.a + p.b * q.c;
    r.b = p.a * q.b + p.b * q.d;
    r.c = p.c * q.a + p.d * q.c;
    r.d = p.c * q.b + p.d * q.d;
    r.tx = p.tx * q.a + p.ty * q.c + q.tx;
    r.ty = p.tx * q.b + p.ty * q.d + q.ty;
    return r;
}
static double ct_attr_num(id attrs, id key) {
    id v = ct_dict_get(attrs, key);
    if (!v || !cx_responds(object_getClass(v), sel_registerName("doubleValue"))) return 0;
    return MSG0(double, v, "doubleValue");
}
/* status e matriz a partir do que o run guarda: nível bidi dos clusters, ordem dos índices e atributos */
static void ct_run_status(ct_run_t *r) {
    ct_line_t *line = r->line;
    long prev = -1;
    int rtl;
    static id kobl, kvert;
    r->status = 0;
    ct_font_matrix(r->font, &r->matrix);
    if (r->count < 1) return;
    rtl = line->g[r->begin].level & 1;
    if (rtl) r->status |= CT_RUN_RTL;
    for (long i = 0; i < r->count; i++) {
        long idx = line->g[r->begin + i].idx;
        if (idx < 0) continue;                  /* glifo do token de truncamento */
        if (prev >= 0 && (rtl ? idx >= prev : idx <= prev)) r->status |= CT_RUN_NONMONO;
        prev = idx;
    }
    if (ct_attr_num(r->attrs, (id)kCTVerticalFormsAttributeName) != 0 ||
        ct_attr_num(r->attrs, ct_key("CTVerticalForms", &kvert)) != 0) {
        ct_mx_t rot = {0, -1, 1, 0, 0, 0};       /* glifo girado 90° para a coluna vertical */
        r->matrix = ct_mx_mul(r->matrix, rot);
    }
    {
        double ob = ct_attr_num(r->attrs, ct_key("NSObliqueness", &kobl));
        if (ob != 0) {
            ct_mx_t sk = {1, 0, ob, 1, 0, 0};
            r->matrix = ct_mx_mul(r->matrix, sk);
        }
    }
    if (!mx_identity(&r->matrix)) r->status |= CT_RUN_MATRIX;
}
static id ct_run_new(ct_line_t *line, long begin, long count) {
    ct_run_t *r = calloc(1, sizeof *r);
    id o;
    if (!r) return 0;
    r->line = line;
    r->begin = begin;
    r->count = count;
    if (count > 0) {
        long first = line->g[begin].idx;
        for (long i = 0; first < 0 && i < count; i++) first = line->g[begin + i].idx;
        r->font = objc_retain(line->g[begin].font);
        r->attrs = objc_retain(ct_attrs(line->astr, first < 0 ? 0 : first));
    }
    ct_run_status(r);
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
        int dir = b->g[i].level & 1;
        while (j < b->n && b->g[j].font == font && (b->g[j].level & 1) == dir) j++;
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
    /* ordem visual: procura o cluster que contém idx; no RTL a borda de entrada é a da direita */
    long lo = -1;
    for (long i = 0; i < b->n; i++) {
        ct_clus *g = &b->g[i];
        if (g->idx >= 0 && (lo < 0 || g->idx < lo)) lo = g->idx;
        if (g->idx >= 0 && idx >= g->idx && idx < g->idx + g->units) {
            int rtl = g->level & 1;
            int inside = g->units > 1 && idx > g->idx;
            double lead = rtl ? g->adv : 0;
            if (secondary) *secondary = x + g->adv * 0.5;
            return x + (inside ? g->adv - lead : lead);
        }
        x += g->adv;
    }
    if (lo >= 0 && idx < lo) return 0;
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
unsigned CTRunGetStatus(id run) { ct_run_t *r = ct_state(run); return r ? r->status : 0; }
ct_mx_t CTRunGetTextMatrix(id run) {
    ct_mx_t m = {1, 0, 0, 1, 0, 0};
    ct_run_t *r = ct_state(run);
    return r ? r->matrix : m;
}
void CTRunGetGlyphs(id run, ct_range_t range, u16 *buf) {
    ct_run_t *r = ct_state(run);
    long a, b;
    ct_run_bounds(r, range, &a, &b);
    if (!buf) return;
    for (long i = a; i < b; i++) buf[i - a] = r->line->g[r->begin + i].glyph;
}
static void ct_run_cache(ct_run_t *r) {
    double x;
    long i;
    if (!r || r->glyphs || r->count < 1 || !r->line) return;
    r->glyphs = calloc((size_t)r->count, sizeof(u16));
    r->pos = calloc((size_t)r->count, sizeof(ct_pt_t));
    r->idxs = calloc((size_t)r->count, sizeof(long));
    if (!r->glyphs || !r->pos || !r->idxs) {
        free(r->glyphs); free(r->pos); free(r->idxs);
        r->glyphs = 0; r->pos = 0; r->idxs = 0;
        return;
    }
    x = 0;
    for (i = 0; i < r->count; i++) {
        ct_clus *g = &r->line->g[r->begin + i];
        r->glyphs[i] = g->glyph;
        r->pos[i].x = x;
        r->pos[i].y = 0;
        r->idxs[i] = g->idx < 0 ? 0 : g->idx;
        x += g->adv;
    }
}
const u16 *CTRunGetGlyphsPtr(id run) {
    ct_run_t *r = ct_state(run);
    ct_run_cache(r);
    return r ? r->glyphs : 0;
}
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
const ct_pt_t *CTRunGetPositionsPtr(id run) {
    ct_run_t *r = ct_state(run);
    ct_run_cache(r);
    return r ? r->pos : 0;
}
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
const long *CTRunGetStringIndicesPtr(id run) {
    ct_run_t *r = ct_state(run);
    ct_run_cache(r);
    return r ? r->idxs : 0;
}
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
    ct_fs_t *owned = 0;
    id ts;
    ct_ts_t *t;
    ct_frame_t *fr;
    id o, arr;
    double maxw, maxh, y, usedh;
    long pos, fit_end;
    (void)attrs;
    ct_lines_init();
    if (fs && object_getClass(fs) == &CTFramesetter_c) {
        owned = ct_state(fs);
        ts = owned ? owned->ts : 0;
    } else ts = CTTypesetterCreateWithAttributedString(fs);
    if (!ts) return 0;
    t = ct_state(ts);
    if (!t) return 0;
    if (!t) { if (!owned) objc_release(ts); return 0; }
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
        line = CTTypesetterCreateLine(ts, rr);
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
        if (!owned) objc_release(ts);
        return 0;
    }
    if (!owned) objc_release(ts);
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
