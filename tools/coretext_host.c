/* Core Text no host (CT_HOST): framesetter, estilo de parágrafo, tabulação, truncagem, run delegate e as funções de
 * linha novas. O harness mede cada caractere ASCII com 0,55 em (ascent 0,952, descent 0,241), então as contas
 * abaixo são exatas. O bidi e a quebra UAX #14 com texto não ASCII estão em tools/ct_layout_host.c.
 *   zig cc -O1 -w -DCT_HOST -DCT_LAYOUT_HOST -Iuserland/libsystem tools/coretext_host.c tools/ctline_host_stubs.c \
 *     userland/libsystem/coretext.c userland/libsystem/cfhost.c userland/libsystem/objc_msg.S userland/libsystem/ct_layout.c \
 *     -ldl -o /tmp/coretext_host && /tmp/coretext_host */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "objc.h"

typedef struct { double x, y; } pt_t;
typedef struct { double w, h; } sz_t;
typedef struct { double x, y, w, h; } rc_t;
typedef struct { long loc, len; } rg_t;
typedef struct { unsigned spec; size_t size; const void *value; } ps_set_t;
typedef struct { long version; void (*dealloc)(void *); double (*ascent)(void *); double (*descent)(void *); double (*width)(void *); } rd_cb_t;
void host_runtime_init(void);
void cx_coretext_init(void);
id ct_test_font(double size);
id ct_test_astr(const char *s, id font, id color);
id cx_str(const char *s);
id CTFramesetterCreateWithAttributedString(id astr);
id CTFramesetterCreateFrame(id fs, rg_t range, void *path, id attrs);
sz_t CTFramesetterSuggestFrameSizeWithConstraints(id fs, rg_t range, id attrs, sz_t constraints, rg_t *fit);
id CTFramesetterGetTypesetter(id fs);
id CTFrameGetLines(id frame);
rg_t CTFrameGetStringRange(id frame);
rg_t CTFrameGetVisibleStringRange(id frame);
void CTFrameGetLineOrigins(id frame, rg_t range, pt_t *buf);
rg_t CTLineGetStringRange(id line);
double CTLineGetTypographicBounds(id line, double *a, double *d, double *l);
double CTLineGetOffsetForStringIndex(id line, long idx, double *secondary);
long CTLineGetStringIndexForPosition(id line, pt_t pos);
double CTLineGetPenOffsetForFlush(id line, double flush, double width);
id CTLineCreateWithAttributedString(id astr);
id CTParagraphStyleCreate(const ps_set_t *s, size_t n);
unsigned char CTParagraphStyleGetValueForSpecifier(id style, unsigned spec, size_t size, void *out);
id CTTextTabCreate(unsigned char align, double loc, id opts);
id CTRunDelegateCreate(const rd_cb_t *cb, void *ref);
void *CGPathCreateWithRect(rc_t r, const void *m);
extern const void *kCTParagraphStyleAttributeName, *kCTRunDelegateAttributeName;
/* ---- o que o coretext.c pede e o ctline_host_stubs.c não tem (como no ctrun_host.c) */
cx_class_t cx_cfconst_isa __asm__("__CFConstantStringClassReference");
static cx_class_t Num_c, Num_m;
typedef struct { cx_class_t *isa; double v; } num_t;
static double num_d(id self, SEL c) { (void)c; return ((num_t *)self)->v; }
static long long num_ll(id self, SEL c) { (void)c; return (long long)((num_t *)self)->v; }
id cx_num_i(long long v) { num_t *n = (num_t *)cx_alloc_instance(&Num_c); n->v = (double)v; return (id)n; }
id cx_num_d(double v) { num_t *n = (num_t *)cx_alloc_instance(&Num_c); n->v = v; return (id)n; }
unsigned char *cx_read_file(const char *path, unsigned long long *len) { (void)path; if (len) *len = 0; return 0; }
double cx_bm_scale(const void *ctx) { (void)ctx; return 1; }
unsigned long CFGetTypeID(const void *cf) { (void)cf; return 1; }
static id g_font_data;
id CFDataCreate(void *alloc, const unsigned char *bytes, long n);
id objc_retain(id o);
void *CGDataProviderCopyData(void *p) { (void)p; return g_font_data ? objc_retain(g_font_data) : 0; }
void *CGFontCreateWithDataProvider(void *prov);
int CTFontManagerRegisterGraphicsFont(void *font, id *error);
id CTFontCreateWithGraphicsFont(void *font, double size, const void *matrix, id attrs);
id CTFontCopyName(id f, id key);
id CTFontCopySupportedLanguages(id f);
unsigned char CTFontHasTable(id f, unsigned tag);
id CTFontManagerCopyAvailablePostScriptNames(void);
id CTFontDescriptorCreateCopyWithSymbolicTraits(id d, unsigned v, unsigned m);
id CTFontDescriptorCreateWithAttributes(id attrs);
const char *cx_cstr(id o);
extern const void *kCTFontCopyrightNameKey, *kCTFontFamilyNameKey, *kCTFontVersionNameKey, *kCTFontTraitsAttribute, *kCTFontSymbolicTrait;
static int arr_has(id arr, const char *s) {
    long n = ((long (*)(id, SEL))objc_msgSend)(arr, sel_registerName("count"));
    for (long i = 0; i < n; i++) if (!strcmp(cx_cstr(((id (*)(id, SEL, unsigned long))objc_msgSend)(arr, sel_registerName("objectAtIndex:"), (unsigned long)i)), s)) return 1;
    return 0;
}
static int fails;
#define CHECK(c, m) do { int ok_ = (c); printf("%s %s\n", ok_ ? "ok  " : "FAIL", m); if (!ok_) fails++; } while (0)
#define NEAR(a, b) (fabs((a) - (b)) < 0.01)
static id g_font;
static id astr_with(const char *s, id style) {
    id a = ct_test_astr(s, g_font, 0);
    if (style) { id attrs = ((id *)*(void **)((char *)a + 8))[1]; ((void (*)(id, SEL, id, id))objc_msgSend)(attrs, sel_registerName("setObject:forKey:"), style, (id)kCTParagraphStyleAttributeName); }
    return a;
}
static id style1(unsigned spec, const void *v, size_t size) { ps_set_t s = {spec, size, v}; return CTParagraphStyleCreate(&s, 1); }
static id frame(const char *s, id style, double w, double h, long *nlines, pt_t *org) {
    id fs = CTFramesetterCreateWithAttributedString(astr_with(s, style));
    rc_t r = {0, 0, w, h};
    id f = CTFramesetterCreateFrame(fs, (rg_t){0, 0}, CGPathCreateWithRect(r, 0), 0);
    id lines = CTFrameGetLines(f);
    *nlines = lines ? ((long (*)(id, SEL))objc_msgSend)(lines, sel_registerName("count")) : 0;
    if (org && *nlines) CTFrameGetLineOrigins(f, (rg_t){0, 0}, org);
    return f;
}
static id line_at(id f, long i) { return ((id (*)(id, SEL, unsigned long))objc_msgSend)(CTFrameGetLines(f), sel_registerName("objectAtIndex:"), (unsigned long)i); }
static double rd_w(void *r) { (void)r; return 40; }
static double rd_a(void *r) { (void)r; return 30; }
static double rd_d(void *r) { (void)r; return 5; }
id CTRubyAnnotationCreate(unsigned char al, unsigned char ov, double size, id text[4]);
id CTRubyAnnotationCreateWithAttributes(unsigned char al, unsigned char ov, unsigned char pos, id str, id attrs);
extern const void *kCTRubyAnnotationAttributeName, *kCTRubyAnnotationScaleToFitAttributeName;
void *ct_test_bitmap(int w, int h);
void CTLineDraw(id line, void *ctx);
extern int ct_test_nglyph; extern double ct_test_gx[], ct_test_gy[];
/* linha com ruby: desenha e devolve largura, ascent e descent */
static double ruby_line(const char *base, id ruby, double *as, double *de) {
    id a = ct_test_astr(base, g_font, 0);
    id at = ((id *)*(void **)((char *)a + 8))[1];
    ((void (*)(id, SEL, id, id))objc_msgSend)(at, sel_registerName("setObject:forKey:"), ruby, (id)kCTRubyAnnotationAttributeName);
    id l = CTLineCreateWithAttributedString(a);
    ct_test_nglyph = 0;
    CTLineDraw(l, ct_test_bitmap(200, 60));
    return CTLineGetTypographicBounds(l, as, de, 0);
}
static id rtext(int pos, const char *s) { static id t[4]; for (int i = 0; i < 4; i++) t[i] = 0; t[pos] = cx_str(s); return (id)t; }
int main(void) {
    host_runtime_init();
    cx_define_class(&Num_c, &Num_m, "NSNumber", cx_class_named("NSObject"), 8);
    cx_add_method(&Num_c, "doubleValue", (void *)num_d);
    cx_add_method(&Num_c, "longLongValue", (void *)num_ll);
    cx_coretext_init();
    g_font = ct_test_font(10);                    /* avanço 5,5; ascent 9,52; descent 2,41 */
    long n;
    pt_t o[16];
    id f = frame("aaaa bbbb cccc dddd", 0, 100, 200, &n, o);
    rg_t r0 = CTLineGetStringRange(line_at(f, 0)), r1 = n > 1 ? CTLineGetStringRange(line_at(f, 1)) : (rg_t){0, 0};
    CHECK(n == 2 && r0.loc == 0 && r0.len == 15 && r1.loc == 15 && r1.len == 4, "quebra por palavra: \"aaaa bbbb cccc \" | \"dddd\"");
    double a, d, l;
    CTLineGetTypographicBounds(line_at(f, 0), &a, &d, &l);
    CHECK(NEAR(o[0].x, 0) && NEAR(o[0].y, 200 - a), "origem da 1ª linha: x 0, y = altura − ascent (relativa à caixa)");
    CHECK(n > 1 && NEAR(o[1].y, o[0].y - d - l - a), "2ª linha: uma altura de linha abaixo");
    rg_t vis = CTFrameGetVisibleStringRange(f);
    CHECK(vis.loc == 0 && vis.len == 19, "faixa visível = o texto todo");
    unsigned char al = 1;
    f = frame("aaaa bbbb cccc dddd", style1(0, &al, 1), 100, 200, &n, o);
    CHECK(NEAR(o[0].x, 100 - 77) && NEAR(o[1].x, 100 - 22), "alinhado à direita (sem o espaço do fim)");
    al = 2;
    f = frame("aaaa bbbb cccc dddd", style1(0, &al, 1), 100, 200, &n, o);
    CHECK(NEAR(o[0].x, 11.5) && NEAR(o[1].x, 39), "centralizado");
    al = 3;
    f = frame("aaaa bbbb cccc dddd", style1(0, &al, 1), 100, 200, &n, o);
    CHECK(NEAR(CTLineGetTypographicBounds(line_at(f, 0), 0, 0, 0), 100) || CTLineGetTypographicBounds(line_at(f, 0), 0, 0, 0) > 95, "justificado: a linha do meio do parágrafo ocupa a largura");
    CHECK(NEAR(CTLineGetTypographicBounds(line_at(f, 1), 0, 0, 0), 22), "justificado: a última linha não estica");
    double first = 10, head = 20;
    ps_set_t ind[2] = {{1, sizeof(double), &first}, {2, sizeof(double), &head}};
    f = frame("aaaa bbbb cccc dddd eeee ffff", CTParagraphStyleCreate(ind, 2), 100, 200, &n, o);
    CHECK(NEAR(o[0].x, 10) && NEAR(o[1].x, 20), "recuo da 1ª linha e das outras");
    double sp = 4;
    f = frame("aaaa bbbb cccc dddd", style1(10, &sp, sizeof sp), 100, 200, &n, o);
    CHECK(n == 2 && NEAR(o[1].y, o[0].y - d - l - a - 4), "espaçamento entre linhas");
    double mult = 2;
    f = frame("aaaa bbbb cccc dddd", style1(7, &mult, sizeof mult), 100, 200, &n, o);
    CHECK(n == 2 && NEAR(o[0].y, 200 - a - (a + d + l)), "múltiplo de altura de linha 2");
    f = frame("ab\ncd\n\nef", 0, 100, 200, &n, o);
    CHECK(n == 4, "quebras obrigatórias (parágrafo vazio conta como linha)");
    double after = 6;
    f = frame("ab\ncd", style1(11, &after, sizeof after), 100, 200, &n, o);
    CHECK(n == 2 && NEAR(o[1].y, o[0].y - d - l - 6 - a), "espaço depois do parágrafo");
    f = frame("abcdefghijklmnopqrstuvwxyz", 0, 50, 200, &n, o);
    rg_t c0 = CTLineGetStringRange(line_at(f, 0));
    CHECK(c0.len == 9, "palavra maior que a largura: quebra por grafema (9 × 5,5 ≤ 50)");
    unsigned char tail = 4;
    f = frame("aaaa bbbb cccc dddd eeee ffff gggg", style1(6, &tail, 1), 100, 200, &n, o);
    CHECK(n == 1 && CTLineGetTypographicBounds(line_at(f, 0), 0, 0, 0) <= 100.01, "truncagem no fim: uma linha dentro da largura");
    f = frame("aaaa bbbb cccc dddd", 0, 100, 15, &n, o);
    vis = CTFrameGetVisibleStringRange(f);
    CHECK(n == 1 && vis.len == 15, "caixa baixa: só a 1ª linha cabe (faixa visível 15)");
    /* tamanho sugerido */
    id fs = CTFramesetterCreateWithAttributedString(astr_with("aaaa bbbb cccc dddd", 0));
    rg_t fit;
    sz_t z = CTFramesetterSuggestFrameSizeWithConstraints(fs, (rg_t){0, 0}, 0, (sz_t){100, 1e9}, &fit);
    CHECK(NEAR(z.w, 77) && NEAR(z.h, ceil(2 * (a + d + l))) && fit.len == 19, "tamanho sugerido (largura usada, duas linhas)");
    CHECK(CTFramesetterGetTypesetter(fs) != 0, "CTFramesetterGetTypesetter");
    /* tabulação: paradas de 28 pt por padrão; parada à direita */
    f = frame("a\tb", 0, 200, 50, &n, o);
    CHECK(NEAR(CTLineGetOffsetForStringIndex(line_at(f, 0), 2, 0), 28), "tabulação padrão: 28 pt");
    id tabs[1] = {CTTextTabCreate(1, 100, 0)};
    id arr = ((id (*)(id, SEL))objc_msgSend)((id)cx_class_named("NSMutableArray"), sel_registerName("array"));
    ((void (*)(id, SEL, id))objc_msgSend)(arr, sel_registerName("addObject:"), tabs[0]);
    f = frame("a\tbb", style1(4, &arr, sizeof arr), 200, 50, &n, o);
    CHECK(NEAR(CTLineGetOffsetForStringIndex(line_at(f, 0), 2, 0), 89), "tabulação à direita em 100: \"bb\" termina em 100");
    /* estilo: ida e volta */
    double v = 0;
    id st = style1(1, &first, sizeof first);
    CHECK(CTParagraphStyleGetValueForSpecifier(st, 1, sizeof v, &v) && v == 10, "GetValueForSpecifier devolve o valor");
    unsigned char a2 = 9;
    CHECK(CTParagraphStyleGetValueForSpecifier(0, 0, 1, &a2) && a2 == 4, "estilo nulo: alinhamento natural");
    /* linha: posição → índice, deslocamento para alinhar */
    id line = CTLineCreateWithAttributedString(astr_with("abcd", 0));
    CHECK(CTLineGetStringIndexForPosition(line, (pt_t){6, 0}) == 1 && CTLineGetStringIndexForPosition(line, (pt_t){9, 0}) == 2, "CTLineGetStringIndexForPosition (meio do glifo)");
    CHECK(NEAR(CTLineGetPenOffsetForFlush(line, 0.5, 100), 39), "CTLineGetPenOffsetForFlush 0,5");
    /* run delegate: largura e altura do anexo */
    rd_cb_t cb = {1, 0, rd_a, rd_d, rd_w};
    id ra = ct_test_astr("x", g_font, 0);
    id rattrs = ((id *)*(void **)((char *)ra + 8))[1];
    ((void (*)(id, SEL, id, id))objc_msgSend)(rattrs, sel_registerName("setObject:forKey:"), CTRunDelegateCreate(&cb, 0), (id)kCTRunDelegateAttributeName);
    line = CTLineCreateWithAttributedString(ra);
    double ra_a, ra_d;
    double rw = CTLineGetTypographicBounds(line, &ra_a, &ra_d, 0);
    CHECK(NEAR(rw, 40) && NEAR(ra_a, 30) && NEAR(ra_d, 5), "run delegate: largura 40, ascent 30, descent 5");

    /* ruby (fonte de teste 10: avanço 5,5; o ruby a 0,5 → 2,75, ascent 4,76, descent 1,205) */
    {
        double as, de, w;
        w = ruby_line("xx", CTRubyAnnotationCreate(0, 0, 0.5, (id *)rtext(0, "abc")), &as, &de);
        CHECK(NEAR(w, 11) && NEAR(as, 9.52 + 4.76 + 1.205) && NEAR(de, 2.41), "ruby acima: a linha sobe pela altura do ruby");
        CHECK(ct_test_nglyph == 5 && NEAR(ct_test_gx[2], 1.375) && NEAR(ct_test_gx[4], 6.875) && NEAR(ct_test_gy[2], 9.52 + 1.205),
              "ruby acima, centralizado: 3 glifos sobre a base");
        w = ruby_line("xx", CTRubyAnnotationCreate(1, 0, 0.5, (id *)rtext(1, "abc")), &as, &de);
        CHECK(NEAR(de, 2.41 + 5.965) && NEAR(ct_test_gx[2], 0) && NEAR(ct_test_gy[2], -2.41 - 4.76), "ruby abaixo, alinhado ao início");
        w = ruby_line("xx", CTRubyAnnotationCreate(3, 0, 0.5, (id *)rtext(0, "abc")), &as, &de);
        CHECK(NEAR(ct_test_gx[2], 11 - 8.25), "ruby alinhado ao fim");
        w = ruby_line("xx", CTRubyAnnotationCreate(4, 0, 0.5, (id *)rtext(0, "ab")), &as, &de);
        CHECK(NEAR(ct_test_gx[2], 0) && NEAR(ct_test_gx[3], 8.25), "DistributeLetter: pontas na base");
        w = ruby_line("xx", CTRubyAnnotationCreate(5, 0, 0.5, (id *)rtext(0, "ab")), &as, &de);
        CHECK(NEAR(ct_test_gx[2], 1.375) && NEAR(ct_test_gx[3], 6.875), "DistributeSpace: meia folga nas pontas");
        w = ruby_line("x", CTRubyAnnotationCreate(0, 3, 0.5, (id *)rtext(0, "abcdef")), &as, &de);
        CHECK(NEAR(w, 16.5) && NEAR(ct_test_gx[0], 5.5) && NEAR(ct_test_gx[1], 0), "overhang None: a base abre espaço e fica no meio");
        w = ruby_line("x", CTRubyAnnotationCreate(0, 0, 0.5, (id *)rtext(0, "abcdef")), &as, &de);
        CHECK(NEAR(w, 5.5) && NEAR(ct_test_gx[1], -5.5), "overhang Auto: o ruby passa por cima dos vizinhos");
        id ra = ct_test_astr("x", g_font, 0);
        id rat = ((id *)*(void **)((char *)ra + 8))[1];
        ((void (*)(id, SEL, id, id))objc_msgSend)(rat, sel_registerName("setObject:forKey:"), cx_num_i(1), (id)kCTRubyAnnotationScaleToFitAttributeName);
        w = ruby_line("x", CTRubyAnnotationCreateWithAttributes(0, 0, 0, cx_str("abcdef"), rat), &as, &de);
        CHECK(NEAR(w, 5.5) && NEAR(ct_test_gx[1], 0) && NEAR(ct_test_gx[2], 2.75 / 3) && NEAR(ct_test_gx[6], 5.5 - 2.75 / 3),
              "ScaleToFit: o ruby encolhe para a largura da base");
        w = ruby_line("xx", CTRubyAnnotationCreate(0, 0, 0.5, (id *)rtext(3, "ab")), &as, &de);
        CHECK(NEAR(w, 16.5) && NEAR(ct_test_gx[2], 11) && NEAR(ct_test_gy[2], 0) && NEAR(as, 9.52), "ruby inline: depois da base, na linha");
    }
    /* fonte de verdade: DejaVu registrada pelo gerenciador */
    FILE *ff = fopen("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", "rb");
    if (ff) {
        static unsigned char buf[1 << 20];
        long nb = (long)fread(buf, 1, sizeof buf, ff);
        fclose(ff);
        g_font_data = CFDataCreate(0, buf, nb);
        void *cg = CGFontCreateWithDataProvider((void *)1);
        CHECK(cg && CTFontManagerRegisterGraphicsFont(cg, 0), "DejaVu registrada (CTFontManagerRegisterGraphicsFont)");
        id dv = cg ? CTFontCreateWithGraphicsFont(cg, 12, 0, 0) : 0;
        id cpy = dv ? CTFontCopyName(dv, (id)kCTFontCopyrightNameKey) : 0;
        CHECK(cpy && strstr(cx_cstr(cpy), "Bitstream"), "CTFontCopyName(copyright) pela tabela name");
        id fam = dv ? CTFontCopyName(dv, (id)kCTFontFamilyNameKey) : 0;
        CHECK(fam && !strcmp(cx_cstr(fam), "DejaVu Sans"), "CTFontCopyName(família)");
        if (fam) printf("     família: %s\n", cx_cstr(fam));
        id langs = dv ? CTFontCopySupportedLanguages(dv) : 0;
        CHECK(langs && arr_has(langs, "ru") && arr_has(langs, "el") && arr_has(langs, "pt"), "idiomas pela OS/2 (ru, el, pt)");
        CHECK(dv && CTFontHasTable(dv, 0x47535542u) && !CTFontHasTable(dv, 0x43464620u), "CTFontHasTable: GSUB sim, CFF não");
        CHECK(arr_has(CTFontManagerCopyAvailablePostScriptNames(), "DejaVuSans"), "nomes PostScript disponíveis incluem a registrada");
    }
    id dsc = CTFontDescriptorCreateCopyWithSymbolicTraits(CTFontDescriptorCreateWithAttributes(0), 2, 2);
    id tr = ((id (*)(id, SEL, id))objc_msgSend)(dsc, sel_registerName("objectForKey:"), (id)kCTFontTraitsAttribute);
    CHECK(tr && ((id (*)(id, SEL, id))objc_msgSend)(tr, sel_registerName("objectForKey:"), (id)kCTFontSymbolicTrait), "descritor com traço simbólico");
    printf("%d falha(s)\n", fails);
    return fails ? 1 : 0;
}
