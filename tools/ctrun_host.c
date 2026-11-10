/*
 * Host: CTRunGetStatus, CTFontGetVerticalTranslationsForGlyphs e CTFontCopyFeatures (CT_HOST).
 * Liga com tools/ctline_host_stubs.c, como o ctapps_host.c:
 *   zig cc -O1 -DCT_HOST -Iuserland/libsystem tools/ctrun_host.c tools/ctline_host_stubs.c \
 *     userland/libsystem/coretext.c userland/libsystem/cfhost.c userland/libsystem/objc_msg.S -o /tmp/ctrun && /tmp/ctrun
 * As fontes são sfnt montadas aqui (head/hhea/hmtx/vhea/vmtx/loca/glyf/VORG/GSUB/GPOS), registradas
 * por CTFontManagerRegisterGraphicsFont (no host cai no registro local, sem kernel).
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "objc.h"

id cx_alloc_instance(cx_class_t *cls);
id cx_str(const char *s);
const char *cx_cstr(id o);
id objc_retain(id o);

typedef struct { double x, y; } ct_pt_t;
typedef struct { double w, h; } ct_sz_t;
typedef struct { double a, b, c, d, tx, ty; } ct_mx_t;

void host_runtime_init(void);
void cx_coretext_init(void);
id ct_test_font(double size);
id ct_test_astr(const char *s, id font, id color);
id CTLineCreateWithAttributedString(id astr);
id CTLineGetGlyphRuns(id line);
double CTLineGetOffsetForStringIndex(id line, long idx, double *secondary);
long CTRunGetGlyphCount(id run);
const long *CTRunGetStringIndicesPtr(id run);
unsigned CTRunGetStatus(id run);
ct_mx_t CTRunGetTextMatrix(id run);
ct_mx_t CTFontGetMatrix(id f);
id CTFontCreateWithName(id name, double size, const void *matrix);
void *CGFontCreateWithDataProvider(void *prov);
int CTFontManagerRegisterGraphicsFont(void *font, id *error);
id CTFontCreateWithGraphicsFont(void *font, double size, const void *matrix, id attrs);
void CTFontGetVerticalTranslationsForGlyphs(id f, int orientation, const u16 *glyphs, ct_sz_t *trans, long n);
id CTFontCopyFeatures(id f);
double CTFontGetAscent(id f);
extern const void *kCTFontFeatureTypeIdentifierKey, *kCTFontFeatureTypeSelectorsKey, *kCTFontFeatureTypeExclusiveKey;
extern const void *kCTFontFeatureSelectorIdentifierKey, *kCTFontFeatureSelectorDefaultKey, *kCTFontOpenTypeFeatureTag;
id CFDataCreate(void *alloc, const unsigned char *bytes, long n);

/* ---- o que o coretext.c pede e o ctline_host_stubs.c não tem */
cx_class_t cx_cfconst_isa __asm__("__CFConstantStringClassReference");
static cx_class_t cfconst_meta, Num_c, Num_m, U16Str_c, U16Str_m;
typedef struct { cx_class_t *isa; double v; } num_t;
static i64 num_ll(id self, SEL c) { (void)c; return (i64)((num_t *)self)->v; }
static double num_d(id self, SEL c) { (void)c; return ((num_t *)self)->v; }
id cx_num_i(long long v) { num_t *n = (num_t *)cx_alloc_instance(&Num_c); n->v = (double)v; return (id)n; }
id cx_num_d(double v) { num_t *n = (num_t *)cx_alloc_instance(&Num_c); n->v = v; return (id)n; }
unsigned char *cx_read_file(const char *path, unsigned long long *len) { (void)path; if (len) *len = 0; return 0; }
double cx_bm_scale(const void *ctx) { (void)ctx; return 1; }
void *CGPathCreateWithRect(ct_pt_t r, const void *m) { (void)r; (void)m; return 0; }
unsigned long CFGetTypeID(const void *cf) { (void)cf; return 1; }
static id g_font_data;
void *CGDataProviderCopyData(void *p) { (void)p; return g_font_data ? objc_retain(g_font_data) : 0; }

/* NSString em UTF-16 (o do harness só guarda bytes): {nsstr_t, u16 *u} */
typedef struct { cx_class_t *isa; u64 flags; char *s; u64 n; u16 *u; } u16str_t;
static u64 u16_len(id self, SEL c) { (void)c; return ((u16str_t *)self)->n; }
static u16 u16_at(id self, SEL c, u64 i) { (void)c; u16str_t *s = (u16str_t *)self; return i < s->n ? s->u[i] : 0; }
static id u16_make(const u16 *u, u64 n) {
    u16str_t *s = (u16str_t *)cx_alloc_instance(&U16Str_c);
    s->u = malloc(n * 2);
    memcpy(s->u, u, n * 2);
    s->n = n;
    s->s = calloc(1, n + 1);
    for (u64 i = 0; i < n; i++) s->s[i] = s->u[i] < 0x80 ? (char)s->u[i] : '?';
    return (id)s;
}
/* NSAttributedString do harness: {string, attrs} em +8 */
static id astr_u16(const u16 *u, u64 n, id font) {
    id a = ct_test_astr("", font, 0);
    id *st = *(id **)((u8 *)a + 8);
    st[0] = u16_make(u, n);
    return a;
}
static id astr_attrs(id a) { return (*(id **)((u8 *)a + 8))[1]; }

static int fails;
static void expect(const char *name, int ok) {
    if (!ok) { printf("FALHOU %s\n", name); fails++; }
    else printf("ok %s\n", name);
}
static int near(double a, double b) { return a - b < 1e-6 && b - a < 1e-6; }

/* ---- sfnt montada */
static void be16(u8 *p, unsigned v) { p[0] = (u8)(v >> 8); p[1] = (u8)v; }
static void be32(u8 *p, unsigned v) { be16(p, v >> 16); be16(p + 2, v); }
typedef struct { const char *tag; u8 b[256]; int n; } tab_t;
static id make_font(tab_t *t, int nt) {
    static u8 out[4096];
    int at = 12 + nt * 16;
    memset(out, 0, sizeof out);
    be32(out, 0x00010000);
    be16(out + 4, (unsigned)nt);
    for (int i = 0; i < nt; i++) {
        u8 *r = out + 12 + i * 16;
        memcpy(r, t[i].tag, 4);
        be32(r + 8, (unsigned)at);
        be32(r + 12, (unsigned)t[i].n);
        memcpy(out + at, t[i].b, (size_t)t[i].n);
        at += (t[i].n + 3) & ~3;
    }
    return CFDataCreate(0, out, at);
}
static id reg_font(tab_t *t, int nt, double size) {
    void *cgf;
    g_font_data = make_font(t, nt);
    cgf = CGFontCreateWithDataProvider((void *)1);
    if (!cgf || CTFontManagerRegisterGraphicsFont(cgf, 0) != 1) return 0;
    return CTFontCreateWithGraphicsFont(cgf, size, 0, 0);
}
static void t_head(tab_t *t) {        /* upem 1000, loca curto */
    memset(t, 0, sizeof *t);
    t->tag = "head"; t->n = 54;
    be16(t->b + 18, 1000);
    be16(t->b + 50, 0);
}
static void t_hhea(tab_t *t, int asc, int nhm) {
    memset(t, 0, sizeof *t);
    t->tag = "hhea"; t->n = 36;
    be16(t->b + 4, (unsigned)asc);
    be16(t->b + 34, (unsigned)nhm);
}
static void t_hmtx(tab_t *t) {        /* glifo 0: 500, glifo 1: 600 */
    memset(t, 0, sizeof *t);
    t->tag = "hmtx"; t->n = 8;
    be16(t->b, 500);
    be16(t->b + 4, 600);
}
static void t_vhea(tab_t *t) {
    memset(t, 0, sizeof *t);
    t->tag = "vhea"; t->n = 36;
    be16(t->b + 34, 2);
}
static void t_vmtx(tab_t *t) {        /* tsb 100 e 50 */
    memset(t, 0, sizeof *t);
    t->tag = "vmtx"; t->n = 8;
    be16(t->b, 1000); be16(t->b + 2, 100);
    be16(t->b + 4, 1000); be16(t->b + 6, 50);
}
static void t_loca(tab_t *t) {        /* curto (offset/2): 0, 12, 24 */
    memset(t, 0, sizeof *t);
    t->tag = "loca"; t->n = 6;
    be16(t->b, 0); be16(t->b + 2, 6); be16(t->b + 4, 12);
}
static void t_glyf(tab_t *t) {        /* yMax 700 e 650 */
    memset(t, 0, sizeof *t);
    t->tag = "glyf"; t->n = 24;
    be16(t->b + 8, 700);
    be16(t->b + 12 + 8, 650);
}
static void t_vorg(tab_t *t) {        /* padrão 880; glifo 1 = 900 */
    memset(t, 0, sizeof *t);
    t->tag = "VORG"; t->n = 12;
    be16(t->b, 1);
    be16(t->b + 4, 880);
    be16(t->b + 6, 1);
    be16(t->b + 8, 1); be16(t->b + 10, 900);
}
/* GSUB/GPOS só com a FeatureList (offset no byte 6); os registros apontam para Feature vazia */
static void t_ot(tab_t *t, const char *tag, const char *const *feats, int nf) {
    memset(t, 0, sizeof *t);
    t->tag = tag;
    be32(t->b, 0x00010000);
    be16(t->b + 6, 10);
    be16(t->b + 10, (unsigned)nf);
    for (int i = 0; i < nf; i++) {
        memcpy(t->b + 12 + i * 6, feats[i], 4);
        be16(t->b + 12 + i * 6 + 4, 0);
    }
    t->n = 12 + nf * 6 + 4;
}

static long num(id o) { return o ? (long)MSG0(i64, o, "longLongValue") : -1; }
static id at(id arr, u64 i) { return arr && i < MSG0(u64, arr, "count") ? MSG(id, arr, "objectAtIndex:", i) : 0; }
static id key(id d, const void *k) { return d ? MSG(id, d, "objectForKey:", (id)k) : 0; }

int main(void) {
    host_runtime_init();
    cx_define_class(&cx_cfconst_isa, &cfconst_meta, "__NSCFConstantString", cx_class_named("NSString"), 0);
    cx_define_class(&Num_c, &Num_m, "NSNumber", cx_class_named("NSObject"), 8);
    cx_add_method(&Num_c, "longLongValue", (void *)num_ll);
    cx_add_method(&Num_c, "doubleValue", (void *)num_d);
    cx_define_class(&U16Str_c, &U16Str_m, "HostU16String", cx_class_named("NSString"), 8);
    cx_add_method(&U16Str_c, "length", (void *)u16_len);
    cx_add_method(&U16Str_c, "characterAtIndex:", (void *)u16_at);
    cx_coretext_init();

    /* ---- CTRunGetStatus */
    {
        id font = ct_test_font(10);
        id line = CTLineCreateWithAttributedString(ct_test_astr("abc", font, 0));
        id runs = CTLineGetGlyphRuns(line);
        expect("ltr: um run, status 0", MSG0(u64, runs, "count") == 1 && CTRunGetStatus(at(runs, 0)) == 0);
    }
    {
        static const u16 s[] = {'a', 'b', ' ', 0x05D0, 0x05D1, 0x05D2};   /* "ab אבג" */
        id font = ct_test_font(10);
        id line = CTLineCreateWithAttributedString(astr_u16(s, 6, font));
        id runs = CTLineGetGlyphRuns(line);
        id r0 = at(runs, 0), r1 = at(runs, 1);
        const long *ix = r1 ? CTRunGetStringIndicesPtr(r1) : 0;
        double sec;
        expect("misto: dois runs", MSG0(u64, runs, "count") == 2);
        expect("misto: run latino LTR", r0 && CTRunGetGlyphCount(r0) == 3 && CTRunGetStatus(r0) == 0);
        expect("misto: run hebraico RTL", r1 && CTRunGetStatus(r1) == 1);
        expect("misto: índices decrescentes (ordem visual)", ix && ix[0] == 5 && ix[1] == 4 && ix[2] == 3);
        /* o alef (índice 3) é o glifo mais à direita: a borda de entrada dele é o fim da linha */
        expect("misto: offset do alef no RTL", near(CTLineGetOffsetForStringIndex(line, 3, &sec), 6 * 5.5));
    }
    {
        static const u16 s[] = {0x05D0, 0x05D1, ' ', '1', '2'};             /* "אב 12" */
        id line = CTLineCreateWithAttributedString(astr_u16(s, 5, ct_test_font(10)));
        id runs = CTLineGetGlyphRuns(line);
        id rn = at(runs, 0), rh = at(runs, 1);
        const long *ixn = rn ? CTRunGetStringIndicesPtr(rn) : 0;
        expect("parágrafo RTL: número fica à esquerda, LTR", MSG0(u64, runs, "count") == 2 && CTRunGetStatus(rn) == 0 &&
               ixn && ixn[0] == 3 && ixn[1] == 4);
        expect("parágrafo RTL: hebraico RTL, sem bit não monotônico", rh && CTRunGetStatus(rh) == 1);
    }
    {
        ct_mx_t sk = {1, 0, 0.25, 1, 0, 0}, got;
        id f = CTFontCreateWithName(cx_str("Helvetica"), 12, &sk);
        id line = CTLineCreateWithAttributedString(ct_test_astr("ab", f, 0));
        id r = at(CTLineGetGlyphRuns(line), 0);
        got = CTFontGetMatrix(f);
        expect("matriz da fonte guardada", near(got.c, 0.25) && near(got.a, 1));
        expect("run com matriz: status 4", r && CTRunGetStatus(r) == 4 && near(CTRunGetTextMatrix(r).c, 0.25));
    }
    {
        id a = ct_test_astr("ab", ct_test_font(12), 0);
        id r;
        MSG(void, astr_attrs(a), "setObject:forKey:", cx_num_d(0.2), cx_str("NSObliqueness"));
        r = at(CTLineGetGlyphRuns(CTLineCreateWithAttributedString(a)), 0);
        expect("obliquidade: status 4", r && CTRunGetStatus(r) == 4 && near(CTRunGetTextMatrix(r).c, 0.2));
    }

    /* ---- CTFontGetVerticalTranslationsForGlyphs */
    {
        tab_t t[7];
        u16 gs[2] = {0, 1};
        ct_sz_t tr[2];
        id f;
        t_head(&t[0]); t_hhea(&t[1], 800, 2); t_hmtx(&t[2]); t_vhea(&t[3]); t_vmtx(&t[4]); t_loca(&t[5]); t_glyf(&t[6]);
        f = reg_font(t, 7, 10);
        memset(tr, 0, sizeof tr);
        CTFontGetVerticalTranslationsForGlyphs(f, 0, gs, tr, 2);
        expect("vmtx+glyf: x = -avanço/2", f && near(tr[0].w, -2.5) && near(tr[1].w, -3.0));
        expect("vmtx+glyf: y = -(yMax+tsb)", near(tr[0].h, -8.0) && near(tr[1].h, -7.0));
    }
    {
        tab_t t[4];
        u16 gs[2] = {0, 1};
        ct_sz_t tr[2];
        id f;
        t_head(&t[0]); t_hhea(&t[1], 800, 2); t_hmtx(&t[2]); t_vorg(&t[3]);
        f = reg_font(t, 4, 10);
        CTFontGetVerticalTranslationsForGlyphs(f, 0, gs, tr, 2);
        expect("VORG: origem padrão e por glifo", f && near(tr[0].h, -8.8) && near(tr[1].h, -9.0) && near(tr[1].w, -3.0));
    }
    {
        tab_t t[3];
        u16 g = 1;
        ct_sz_t tr;
        id f;
        t_head(&t[0]); t_hhea(&t[1], 750, 2); t_hmtx(&t[2]);
        f = reg_font(t, 3, 10);
        CTFontGetVerticalTranslationsForGlyphs(f, 0, &g, &tr, 1);
        expect("sem vmtx: y = -ascender do hhea", f && near(tr.h, -7.5) && near(tr.w, -3.0));
    }
    {
        id f = ct_test_font(10);
        u16 g = 'a';
        ct_sz_t tr = {0, 0};
        CTFontGetVerticalTranslationsForGlyphs(f, 0, &g, &tr, 1);
        expect("sem bytes: x = -avanço/2, y = -ascent", near(tr.w, -2.75) && near(tr.h, -CTFontGetAscent(f)));
    }

    /* ---- CTFontCopyFeatures */
    {
        static const char *const sub[] = {"liga", "liga", "smcp", "kern", "ss01"};
        static const char *const pos[] = {"kern", "mark"};
        tab_t t[3];
        id f, fs, liga, sels, smcp, kern, ss, mark;
        t_head(&t[0]); t_ot(&t[1], "GSUB", sub, 5); t_ot(&t[2], "GPOS", pos, 2);
        f = reg_font(t, 3, 12);
        fs = f ? CTFontCopyFeatures(f) : 0;
        liga = at(fs, 0); smcp = at(fs, 1); kern = at(fs, 2); ss = at(fs, 3); mark = at(fs, 4);
        sels = key(liga, kCTFontFeatureTypeSelectorsKey);
        expect("OT: cinco tipos, sem repetir tag", fs && MSG0(u64, fs, "count") == 5);
        expect("OT: liga = tipo 1 com on/off", num(key(liga, kCTFontFeatureTypeIdentifierKey)) == 1 && MSG0(u64, sels, "count") == 2 &&
               num(key(at(sels, 0), kCTFontFeatureSelectorIdentifierKey)) == 2 &&
               num(key(at(sels, 1), kCTFontFeatureSelectorIdentifierKey)) == 3);
        expect("OT: liga ligada por padrão", key(at(sels, 0), kCTFontFeatureSelectorDefaultKey) != 0 &&
               key(at(sels, 1), kCTFontFeatureSelectorDefaultKey) == 0);
        expect("OT: smcp = tipo 37 exclusivo", num(key(smcp, kCTFontFeatureTypeIdentifierKey)) == 37 &&
               num(key(smcp, kCTFontFeatureTypeExclusiveKey)) == 1);
        expect("OT: kern sem tipo AAT usa a tag", num(key(kern, kCTFontFeatureTypeIdentifierKey)) == 0x6B65726EL &&
               !strcmp(cx_cstr(key(kern, kCTFontOpenTypeFeatureTag)), "kern") &&
               MSG0(u64, key(kern, kCTFontFeatureTypeSelectorsKey), "count") == 2);
        expect("OT: ss01 = tipo 35, seletores 2 e 3", num(key(ss, kCTFontFeatureTypeIdentifierKey)) == 35 &&
               num(key(at(key(ss, kCTFontFeatureTypeSelectorsKey), 0), kCTFontFeatureSelectorIdentifierKey)) == 2);
        expect("OT: mark só do GPOS", num(key(mark, kCTFontFeatureTypeIdentifierKey)) == 0x6D61726BL);
    }
    {
        id fs = CTFontCopyFeatures(ct_test_font(12));
        id lig = at(fs, 1);
        expect("sem bytes: padrão mínimo, não vazio", fs && MSG0(u64, fs, "count") == 2 &&
               num(key(at(fs, 0), kCTFontFeatureTypeIdentifierKey)) == 0);
        expect("sem bytes: ligaduras obrigatórias e comuns", num(key(lig, kCTFontFeatureTypeIdentifierKey)) == 1 &&
               MSG0(u64, key(lig, kCTFontFeatureTypeSelectorsKey), "count") == 4);
    }
    printf("ctrun: %s (%d falhas)\n", fails ? "FALHOU" : "ok", fails);
    return fails ? 1 : 0;
}
