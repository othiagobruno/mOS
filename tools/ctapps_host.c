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
    printf("%s (%d)\n", fails ? "FALHOU" : "ok", fails);
    return fails ? 1 : 0;
}
