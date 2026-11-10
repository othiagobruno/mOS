/* Host: linha, run, frame e justificação do CoreText (CT_HOST). */
#include <stdio.h>
#include <string.h>
#include "objc.h"

typedef struct { double x, y; } ct_pt_t;
typedef struct { long loc, len; } ct_range_t;

void host_runtime_init(void);
void cx_coretext_init(void);
id ct_test_font(double size);
id ct_test_astr(const char *s, id font, id color);
id CTLineCreateWithAttributedString(id astr);
double CTLineGetTypographicBounds(id line, double *ascent, double *descent, double *leading);
long CTLineGetGlyphCount(id line);
id CTLineGetGlyphRuns(id line);
id CTLineCreateJustifiedLine(id line, double factor, double width);
double CTLineGetTrailingWhitespaceWidth(id line);
double CTLineGetOffsetForStringIndex(id line, long idx, double *secondary);
id CTFramesetterCreateFrame(id fs, ct_range_t range, void *path, id attrs);
id CTFrameGetLines(id frame);
ct_range_t CTFrameGetStringRange(id frame);
void CTFrameGetLineOrigins(id frame, ct_range_t range, ct_pt_t *buf);
long CTRunGetGlyphCount(id run);
const unsigned short *CTRunGetGlyphsPtr(id run);
const ct_pt_t *CTRunGetPositionsPtr(id run);
const long *CTRunGetStringIndicesPtr(id run);
void *CGFontCreateWithDataProvider(void *prov);
int CTFontManagerRegisterGraphicsFont(void *font, id *error);
id CTFontCreateWithGraphicsFont(void *font, double size, const void *matrix, id attrs);
id CTFontCopyFeatures(id font);
extern const void *kCTFontFeatureTypeNameKey;
const char *cx_cstr(id o);
id objc_retain(id o);

id CFDataCreate(void *alloc, const unsigned char *bytes, long n);
static id g_font_data;
void *CGDataProviderCopyData(void *p) {
    (void)p;
    return g_font_data ? objc_retain(g_font_data) : 0;
}
static void be16(unsigned char *p, unsigned v) { p[0] = (unsigned char)(v >> 8); p[1] = (unsigned char)v; }
static void be32(unsigned char *p, unsigned v) { be16(p, v >> 16); be16(p + 2, v); }
static id make_sfnt(void) {
    unsigned char b[256];
    unsigned name_at = 64, feat_at = 160;
    memset(b, 0, sizeof b);
    be32(b, 0x00010000);
    be16(b + 4, 2);
    be16(b + 6, 32);
    be16(b + 8, 1);
    b[12] = 'n'; b[13] = 'a'; b[14] = 'm'; b[15] = 'e';
    be32(b + 20, name_at);
    be32(b + 24, 42);
    b[28] = 'f'; b[29] = 'e'; b[30] = 'a'; b[31] = 't';
    be32(b + 36, feat_at);
    be32(b + 40, 28);
    be16(b + name_at + 2, 2);
    be16(b + name_at + 4, 30);
    be16(b + name_at + 6, 1);
    be16(b + name_at + 12, 6);
    be16(b + name_at + 14, 8);
    be16(b + name_at + 18, 1);
    be16(b + name_at + 24, 256);
    be16(b + name_at + 26, 4);
    be16(b + name_at + 28, 8);
    memcpy(b + name_at + 30, "TestFontLiga", 12);
    be32(b + feat_at, 0x00010000);
    be16(b + feat_at + 4, 1);
    be16(b + feat_at + 12, 1);
    be16(b + feat_at + 14, 1);
    be32(b + feat_at + 16, 24);
    be16(b + feat_at + 22, 256);
    be16(b + feat_at + 26, 256);
    return CFDataCreate(0, b, 188);
}

static int fails;

static void expect(const char *name, int ok) {
    if (!ok) { printf("FALHOU %s\n", name); fails++; }
    else printf("ok %s\n", name);
}

id cx_num_i(long long v) { (void)v; return 0; }
id cx_num_d(double v) { (void)v; return 0; }
unsigned char *cx_read_file(const char *path, unsigned long long *len) { (void)path; if (len) *len = 0; return 0; }
double cx_bm_scale(const void *ctx) { (void)ctx; return 1; }
void *CGPathCreateWithRect(ct_pt_t r, const void *m) { (void)r; (void)m; return 0; }
unsigned long CFGetTypeID(const void *cf) { (void)cf; return 1; }
char cx_cfconst_isa[8] __asm__("__CFConstantStringClassReference");

int main(void) {
    id font, astr, line, runs, run, just, fs, frame;
    double as = 0, de = 0, ld = 0, w, sec = -1, off;
    ct_range_t zr = {0, 0}, fr;
    ct_pt_t origins[8];
    host_runtime_init();
    cx_coretext_init();
    font = ct_test_font(10);
    astr = ct_test_astr("ab c", font, 0);
    line = CTLineCreateWithAttributedString(astr);
    w = CTLineGetTypographicBounds(line, &as, &de, &ld);
    expect("linha tem largura", line && w > 0);
    expect("ascent", as > 0 && de > 0);
    expect("glifos", CTLineGetGlyphCount(line) == 4);
    runs = CTLineGetGlyphRuns(line);
    expect("um run", runs && MSG0(u64, runs, "count") == 1);
    run = MSG(id, runs, "objectAtIndex:", (u64)0);
    expect("run conta", CTRunGetGlyphCount(run) == 4);
    expect("espaço no fim", CTLineGetTrailingWhitespaceWidth(line) == 0);
    off = CTLineGetOffsetForStringIndex(line, 0, &sec);
    expect("offset 0", off == 0 && sec > 0);
    just = CTLineCreateJustifiedLine(line, 1, w * 2);
    expect("justificada mais larga", just && CTLineGetTypographicBounds(just, 0, 0, 0) > w + 1);
    fs = astr;
    frame = CTFramesetterCreateFrame(fs, zr, 0, 0);
    fr = CTFrameGetStringRange(frame);
    expect("frame cobre a string", frame && fr.len >= 4);
    expect("frame tem linha", CTFrameGetLines(frame) && MSG0(u64, CTFrameGetLines(frame), "count") >= 1);
    memset(origins, 0, sizeof origins);
    CTFrameGetLineOrigins(frame, zr, origins);
    expect("origem", origins[0].y != 0 || CTLineGetGlyphCount(line) == 4);
    {
        const unsigned short *gs = CTRunGetGlyphsPtr(run);
        const ct_pt_t *ps = CTRunGetPositionsPtr(run);
        const long *is = CTRunGetStringIndicesPtr(run);
        void *cgf;
        id ctf, feats, one, nm;
        expect("glyphs ptr", gs && gs[0] == (unsigned short)'a' && gs[3] == (unsigned short)'c');
        expect("pos ptr", ps && ps[0].x == 0 && ps[1].x > 0);
        expect("idx ptr", is && is[0] == 0 && is[1] == 1);
        g_font_data = make_sfnt();
        cgf = CGFontCreateWithDataProvider((void *)1);
        expect("cgfont", cgf != 0);
        expect("registro", CTFontManagerRegisterGraphicsFont(cgf, 0) == 1);
        ctf = CTFontCreateWithGraphicsFont(cgf, 12, 0, 0);
        feats = ctf ? CTFontCopyFeatures(ctf) : 0;
        one = feats ? MSG(id, feats, "objectAtIndex:", (u64)0) : 0;
        nm = one ? MSG(id, one, "objectForKey:", (id)kCTFontFeatureTypeNameKey) : 0;
        expect("feat", nm && cx_cstr(nm) && !strcmp(cx_cstr(nm), "Liga"));
    }
    printf("%s (%d)\n", fails ? "FALHOU" : "ok", fails);
    return fails ? 1 : 0;
}
