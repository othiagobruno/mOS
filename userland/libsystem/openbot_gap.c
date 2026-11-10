/* O que o machocheck do OpenBot ainda marca em sistema depois da main.
 * DNS (#73) e o unwinding C++ ficam nas outras frentes. gap_stub devolve 0. */
#include "foundation.h"
#include "libc.h"
extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; unsigned long long flags; const char *s; unsigned long long n; } gap_cf_t;
#define GAPCF(var, text) \
    static gap_cf_t var##_box = {(void *)cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; \
    const void *var = &var##_box;
GAPCF(kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly, "kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly")
GAPCF(kCGImageDestinationLossyCompressionQuality, "kCGImageDestinationLossyCompressionQuality")
GAPCF(AVNumberOfChannelsKey, "AVNumberOfChannelsKey")
size_t CGImageGetWidth(void *img);
size_t CGImageGetHeight(void *img);
size_t CGImageGetBytesPerRow(void *img);
void *CGImageGetDataProvider(void *img);
void *CGDataProviderCopyData(void *p);
const unsigned char *CFDataGetBytePtr(void *d);
void CFRelease(void *o);
int open(const char *path, int flags, ...);
long write(int fd, const void *b, unsigned long n);
int close(int fd);

static unsigned gap_crc(const unsigned char *s, unsigned long n) {
    unsigned c = 0xffffffffu; unsigned long i; int k;
    for (i = 0; i < n; i++) { c ^= s[i]; for (k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1))); }
    return c ^ 0xffffffffu;
}
static void gap_chunk(unsigned char *png, unsigned long *o, const char *typ, const unsigned char *data, unsigned long n) {
    unsigned c; unsigned char *p = png + *o;
    p[0] = (unsigned char)(n >> 24); p[1] = (unsigned char)(n >> 16); p[2] = (unsigned char)(n >> 8); p[3] = (unsigned char)n;
    memcpy(p + 4, typ, 4); if (n && data) memcpy(p + 8, data, n);
    c = gap_crc(p + 4, 4 + n);
    p[8 + n] = (unsigned char)(c >> 24); p[9 + n] = (unsigned char)(c >> 16);
    p[10 + n] = (unsigned char)(c >> 8); p[11 + n] = (unsigned char)c;
    *o += 12 + n;
}
static int gap_png(const unsigned char *px, int w, int h, int bpr, unsigned char **out, unsigned long *n) {
    unsigned long row, rawn, zcap, zo, off, y, x;
    unsigned char *raw, *z, *png, ih[13];
    unsigned ad_a = 1, ad_b = 0;
    if (!px || w < 1 || h < 1 || bpr < w * 4) return -1;
    row = (unsigned long)w * 4ul + 1ul; rawn = row * (unsigned long)h;
    raw = malloc(rawn); if (!raw) return -1;
    for (y = 0; y < (unsigned long)h; y++) {
        unsigned char *r = raw + y * row; const unsigned char *src = px + y * (unsigned long)bpr;
        r[0] = 0;
        for (x = 0; x < (unsigned long)w; x++) {
            unsigned char *d = r + 1 + x * 4ul; const unsigned char *q = src + x * 4ul;
            d[0] = q[2]; d[1] = q[1]; d[2] = q[0]; d[3] = q[3];
        }
    }
    zcap = rawn + (rawn / 65535ul + 1ul) * 5ul + 6ul;
    z = malloc(zcap); if (!z) { free(raw); return -1; }
    z[0] = 0x78; z[1] = 0x01; zo = 2; off = 0;
    while (off < rawn) {
        unsigned long chunk = rawn - off; int last;
        if (chunk > 65535ul) chunk = 65535ul;
        last = off + chunk == rawn;
        z[zo++] = (unsigned char)last; z[zo++] = (unsigned char)chunk; z[zo++] = (unsigned char)(chunk >> 8);
        z[zo++] = (unsigned char)(chunk ^ 0xffu); z[zo++] = (unsigned char)((chunk >> 8) ^ 0xffu);
        memcpy(z + zo, raw + off, chunk); zo += chunk; off += chunk;
    }
    for (off = 0; off < rawn; off++) { ad_a = (ad_a + raw[off]) % 65521u; ad_b = (ad_b + ad_a) % 65521u; }
    z[zo++] = (unsigned char)((ad_b << 16 | ad_a) >> 24); z[zo++] = (unsigned char)((ad_b << 16 | ad_a) >> 16);
    z[zo++] = (unsigned char)((ad_b << 16 | ad_a) >> 8); z[zo++] = (unsigned char)(ad_b << 16 | ad_a);
    free(raw);
    png = malloc(8 + 25 + 12 + zo + 12); if (!png) { free(z); return -1; }
    memcpy(png, "\x89PNG\r\n\x1a\n", 8); off = 8;
    ih[0] = (unsigned char)(w >> 24); ih[1] = (unsigned char)(w >> 16); ih[2] = (unsigned char)(w >> 8); ih[3] = (unsigned char)w;
    ih[4] = (unsigned char)(h >> 24); ih[5] = (unsigned char)(h >> 16); ih[6] = (unsigned char)(h >> 8); ih[7] = (unsigned char)h;
    ih[8] = 8; ih[9] = 6; ih[10] = ih[11] = ih[12] = 0;
    gap_chunk(png, &off, "IHDR", ih, 13);
    gap_chunk(png, &off, "IDAT", z, zo);
    gap_chunk(png, &off, "IEND", 0, 0);
    free(z); *out = png; *n = off; return 0;
}
typedef struct { id url; id img; } gap_dest;
void *CGImageDestinationCreateWithURL(id url, id type, unsigned long count, id opts) {
    gap_dest *d; (void)type; (void)count; (void)opts;
    d = calloc(1, sizeof *d); if (!d) return 0; d->url = url; return d;
}
void CGImageDestinationAddImage(void *dst, id img, id props) {
    gap_dest *d = dst; (void)props; if (d) d->img = img;
}
int CGImageDestinationFinalize(void *dst) {
    gap_dest *d = dst; int w, h, bpr, fd; void *prov, *data; const unsigned char *px;
    unsigned char *png; unsigned long pn; id path; const char *c;
    if (!d || !d->img || !d->url) return 0;
    w = (int)CGImageGetWidth(d->img); h = (int)CGImageGetHeight(d->img); bpr = (int)CGImageGetBytesPerRow(d->img);
    prov = CGImageGetDataProvider(d->img);
    data = prov ? CGDataProviderCopyData(prov) : 0;
    if (!data) return 0;
    px = CFDataGetBytePtr(data);
    if (gap_png(px, w, h, bpr, &png, &pn)) { CFRelease(data); return 0; }
    CFRelease(data);
    path = MSG0(id, d->url, "path");
    c = path ? cx_cstr(path) : 0;
    if (!c) { free(png); return 0; }
    fd = open(c, 577, 0644);
    if (fd < 0) { free(png); return 0; }
    if (write(fd, png, pn) != (long)pn) { close(fd); free(png); return 0; }
    close(fd); free(png); return 1;
}

/* As CF* que estavam aqui (bag, calendário, set, plist, stream, porta, soquete, XML, …)
 * estão implementadas em cf_public.c, vscode_cf.c, cfrunloop.c e slack_frameworks.c.
 * Os símbolos Swift de Foundation continuam nesta frente como ponte vazia: a T5 é quem os completa. */
long gap_sw_0(void) __asm__("$s10Foundation22_convertNSErrorToErrorys0E0_pSo0C0CSgF");
long gap_sw_0(void) { return 0; }
long gap_sw_1(void) __asm__("$s10Foundation3URLV15fileURLWithPathACSSh_tcfC");
long gap_sw_1(void) { return 0; }
long gap_sw_2(void) __asm__("$s10Foundation3URLV19_bridgeToObjectiveCSo5NSURLCyF");
long gap_sw_2(void) { return 0; }
long gap_sw_3(void) __asm__("$s10Foundation3URLV36_unconditionallyBridgeFromObjectiveCyACSo5NSURLCSgFZ");
long gap_sw_3(void) { return 0; }
long gap_sw_4(void) __asm__("$s10Foundation3URLV4pathSSvg");
long gap_sw_4(void) { return 0; }
long gap_sw_5(void) __asm__("$s10Foundation3URLVMa");
long gap_sw_5(void) { return 0; }
long gap_sw_6(void) __asm__("$sSD10FoundationE19_bridgeToObjectiveCSo12NSDictionaryCyF");
long gap_sw_6(void) { return 0; }
long gap_sw_7(void) __asm__("$sSS10FoundationE19_bridgeToObjectiveCSo8NSStringCyF");
long gap_sw_7(void) { return 0; }
long gap_sw_8(void) __asm__("$sSS10FoundationE26_forceBridgeFromObjectiveC_6resultySo8NSStringC_SSSgztFZ");
long gap_sw_8(void) { return 0; }
long gap_sw_9(void) __asm__("$sSS10FoundationE34_conditionallyBridgeFromObjectiveC_6resultSbSo8NSStringC_SSSgztFZ");
long gap_sw_9(void) { return 0; }
long gap_sw_10(void) __asm__("$sSS10FoundationE36_unconditionallyBridgeFromObjectiveCySSSo8NSStringCSgFZ");
long gap_sw_10(void) { return 0; }
long gap_sw_11(void) __asm__("$sSS10FoundationE8EncodingV4utf8ACvgZ");
long gap_sw_11(void) { return 0; }
long gap_sw_12(void) __asm__("$sSS10FoundationE8EncodingVMa");
long gap_sw_12(void) { return 0; }
long gap_sw_13(void) __asm__("$sSa10FoundationE19_bridgeToObjectiveCSo7NSArrayCyF");
long gap_sw_13(void) { return 0; }
long gap_sw_14(void) __asm__("$sSa10FoundationE34_conditionallyBridgeFromObjectiveC_6resultSbSo7NSArrayC_SayxGSgztFZ");
long gap_sw_14(void) { return 0; }
long gap_sw_15(void) __asm__("$sSa10FoundationE36_unconditionallyBridgeFromObjectiveCySayxGSo7NSArrayCSgFZ");
long gap_sw_15(void) { return 0; }
long gap_sw_16(void) __asm__("$sSo12NSDictionaryC10FoundationE17dictionaryLiteralAByp_yptd_tcfC");
long gap_sw_16(void) { return 0; }
long gap_sw_17(void) __asm__("$sSy10FoundationE7cString5usingSays4Int8VGSgSSAAE8EncodingV_tF");
long gap_sw_17(void) { return 0; }
long gap_sw_18(void) __asm__("$ss5ErrorP10FoundationE20localizedDescriptionSSvg");
long gap_sw_18(void) { return 0; }
