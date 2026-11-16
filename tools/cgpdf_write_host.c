/* CGPDFContext no host: a mesma cena vai para um PDF (CGPDFContext) e para um bitmap (CGBitmapContext);
 * tools/cgpdf_write_host.py rasteriza o PDF com o poppler e compara com o bitmap, e confere texto, metadados,
 * links, outline, tags e senha. Uso: cgpdf_write_host pdf saida.pdf [cifra] | png saida.rgba. Compilação: ver o .py. */
#define CG_TEST_OWN_CF 1
#define CG_TEST_OWN_SYS5 1
#define main cg_context_main
#include "cg_context_test.c"
#undef main

/* ---- o que o appkit.c faz para um contexto de bitmap */
int cx_bm_pattern_color(const void *c, const void *color, int stroke);
int cx_bm_set_cap(const void *, int);
int cx_bm_set_join(const void *, int);
static uint32_t argb_of(void *color) {
    double *o = color;
    uint32_t a = (uint32_t)(o[4] * 255 + .5), r = (uint32_t)(o[1] * 255 + .5), g = (uint32_t)(o[2] * 255 + .5), b = (uint32_t)(o[3] * 255 + .5);
    return a << 24 | r << 16 | g << 8 | b;
}
void CGContextSetFillColorWithColor(void *c, void *color) { if (!cx_bm_pattern_color(c, color, 0)) cx_bm_set_fill(c, argb_of(color)); }
void CGContextSetAlpha(void *c, double a) { cx_bm_set_alpha(c, a); }
void CGContextSetLineWidth(void *c, double w) { cx_bm_set_line_width(c, w); }
void CGContextClosePath(void *c) { cx_bm_close(c); }
void CGContextFillPath(void *c) { cx_bm_fill(c, 0); }
void CGContextSaveGState(void *c) { cx_bm_save(c); }
void CGContextRestoreGState(void *c) { cx_bm_restore(c); }
void CGContextSetLineCap(void *c, int v) { cx_bm_set_cap(c, v); }
void CGContextSetLineJoin(void *c, int v) { cx_bm_set_join(c, v); }
void CGContextConcatCTM(void *c, CGAffineTransform t);
void CFDataAppendBytes(void *d, const uint8_t *b, long n) { (void)d; (void)b; (void)n; }
void CGContextRotateCTM(void *c, double a) { CGContextConcatCTM(c, (CGAffineTransform){cos(a), sin(a), -sin(a), cos(a), 0, 0}); }
void CGContextFillRect(void *c, CGRect r) { cx_bm_fill_rect(c, r.origin.x, r.origin.y, r.size.width, r.size.height); }

/* ---- CF mínimo: dicionário por conteúdo da chave, números, booleanos, URL */
static mcls tdict_cls = {"TestDict", 0, 8 + 16 * 16 + 8, {{0, 0}}, 0}, num_cls = {"TestNum", 0, 16, {{0, 0}}, 0}, url_cls = {"TestURL", 0, 16, {{0, 0}}, 0};
static void *tdict(int n, const void **k, void **v) {
    void **o = alloc_of(&tdict_cls);
    o[1] = (void *)(intptr_t)n;
    for (int i = 0; i < n; i++) { o[2 + 2 * i] = (void *)k[i]; o[3 + 2 * i] = v[i]; }
    return o;
}
void *CFDictionaryGetValue(void *d, const void *k) {
    if (!d) return 0;
    void **o = d;
    for (int i = 0; i < (int)(intptr_t)o[1]; i++) if (!strcmp(cx_cstr(o[2 + 2 * i]), cx_cstr((void *)k))) return o[3 + 2 * i];
    return 0;
}
static void *tnum(double v) { double *o = alloc_of(&num_cls); o[1] = v; return o; }
uint8_t CFNumberGetValue(void *n, long t, void *out) {
    if (!n || *(void **)n != &num_cls) return 0;
    double v = ((double *)n)[1];
    if (t == 13) *(double *)out = v; else *(int *)out = (int)v;
    return 1;
}
static char true_obj[16], false_obj[16];
const void *kCFBooleanTrue = true_obj, *kCFBooleanFalse = false_obj;
static void *tstr(const char *s) { void **o = calloc(1, 32); o[0] = __CFConstantStringClassReference; o[1] = (void *)0x7c8; o[2] = strdup(s); o[3] = (void *)strlen(s); return o; }
static void *turl(const char *s) { void **o = alloc_of(&url_cls); o[1] = tstr(s); return o; }
void *CFURLGetString(void *u) { return u ? ((void **)u)[1] : 0; }
void *CFRetain(void *o) { return objc_retain(o); }
static void *tdata(const void *b, long n) { return CFDataCreate_(b, n); }

/* ---- texto: a face 0 é a fonte do arquivo dado (ct_outline.c com CT_HOST lê as tabelas daqui) */
static uint8_t *font; static size_t nfont;
static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
static int hmtx_adv(int gid, int *upem);
long ct_host_sys5(long n, long op, long face, long tag, void *buf, long cap) {
    (void)n; (void)face;
    if (op == 2) { int upem; int a = hmtx_adv((int)tag, &upem); return (long)a | (long)upem << 16; }
    if (op != 0) return -1;
    unsigned nt = (unsigned)font[4] << 8 | font[5];
    for (unsigned i = 0; i < nt; i++) {
        const uint8_t *e = font + 12 + i * 16;
        if (be32(e) != (uint32_t)tag) continue;
        uint32_t off = be32(e + 8), l = be32(e + 12);
        if (buf) memcpy(buf, font + off, (size_t)(cap < (long)l ? cap : (long)l));
        return l;
    }
    return 0;
}
static const uint8_t *table(uint32_t tag) {
    unsigned nt = (unsigned)font[4] << 8 | font[5];
    for (unsigned i = 0; i < nt; i++) if (be32(font + 12 + i * 16) == tag) return font + be32(font + 12 + i * 16 + 8);
    return 0;
}
static int hmtx_adv(int gid, int *upem) {
    const uint8_t *head = table(0x68656164u), *hhea = table(0x68686561u), *hmtx = table(0x686D7478u);
    *upem = head[18] << 8 | head[19];
    int nh = hhea[34] << 8 | hhea[35];
    if (gid >= nh) gid = nh - 1;
    return hmtx[gid * 4] << 8 | hmtx[gid * 4 + 1];
}
int ct_host_cmap(const uint8_t *file, size_t n, uint32_t cp);
int cx_pdfw_glyphs(void *ctx, int face, double size, const uint16_t *glyphs, const CGPoint *pos, size_t n);
void *cx_font_glyph_path(int face, int gid, double size, const CGAffineTransform *m);
void CGPathRelease(void *p);
void CGContextDrawLinearGradient(void *c, void *g, CGPoint s, CGPoint e, uint32_t o);
/* CTFontDrawGlyphs do teste: no PDF vira Type3; no bitmap, os contornos preenchidos (mesma geometria) */
static double text_size = 24;
static void draw_text(void *c, double x, double y, const char *s) {
    uint16_t g[64]; CGPoint p[64];
    size_t n = 0;
    double pen = 0;
    for (; s[n] && n < 64; n++) {
        g[n] = (uint16_t)ct_host_cmap(font, nfont, (uint8_t)s[n]);
        p[n].x = x + pen; p[n].y = y;
        int upem;
        pen += hmtx_adv(g[n], &upem) * text_size / upem;
    }
    if (cx_pdfw_glyphs(c, 0, text_size, g, p, n)) return;
    CGContextBeginPath(c);
    for (size_t i = 0; i < n; i++) {
        CGAffineTransform m = {1, 0, 0, 1, p[i].x, p[i].y};
        void *gp = cx_font_glyph_path(0, g[i], text_size, &m);
        if (gp) { CGContextAddPath(c, gp); CGPathRelease(gp); }
    }
    CGContextFillPath(c);
}

/* ---- PDF API */
void *CGDataConsumerCreate(void *info, const void *cb);
void CGDataConsumerRelease(void *c);
void *CGPDFContextCreate(void *consumer, const CGRect *mb, void *aux);
void CGPDFContextClose(void *c);
void CGContextBeginPage(void *c, const CGRect *mb);
void CGContextEndPage(void *c);
void CGPDFContextSetURLForRect(void *c, void *url, CGRect r);
void CGPDFContextAddDestinationAtPoint(void *c, void *name, CGPoint p);
void CGPDFContextSetDestinationForRect(void *c, void *name, CGRect r);
void CGPDFContextSetOutline(void *c, void *outline);
void CGPDFContextBeginTag(void *c, int type, void *props);
void CGPDFContextEndTag(void *c);
extern const void *kCGPDFContextTitle, *kCGPDFContextAuthor, *kCGPDFContextKeywords, *kCGPDFContextCreator, *kCGPDFContextUserPassword,
    *kCGPDFContextOwnerPassword, *kCGPDFContextAllowsCopying, *kCGPDFOutlineTitle, *kCGPDFOutlineChildren, *kCGPDFOutlineDestination,
    *kCGPDFTagPropertyAlternativeText;
static size_t put_file(void *info, const void *b, size_t n) { return fwrite(b, 1, n, info); }
void *CGImageCreate(size_t, size_t, size_t, size_t, size_t, const void *, uint32_t, void *, const void *, int, int);
void *CGDataProviderCreateWithData(void *, const void *, size_t, void *);
void *CGColorSpaceCreateDeviceRGB(void);
void *CGColorSpaceCreateDeviceGray(void);
void CGImageRelease(void *);
void CGContextDrawImage(void *c, CGRect r, void *img);
void CGContextClipToMask(void *c, CGRect r, void *m);
void CGContextSetBlendMode(void *c, int m);
void CGContextSetShadowWithColor(void *c, CGSize off, double blur, void *color);
void CGContextStrokeRect(void *c, CGRect r);
void CGContextAddEllipseInRect(void *c, CGRect r);
void CGContextSetLineDash(void *c, double phase, const double *l, size_t n);
void CGContextClipToRect(void *c, CGRect r);

static void scene_page1(void *c) {
    /* paths, traço com junção e tracejado, recorte par-ímpar, CTM girada */
    red(c);
    CGContextFillRect(c, (CGRect){{20, 20}, {100, 60}});
    cx_bm_set_stroke(c, 0xFF0000FFu);
    CGContextSetLineWidth(c, 6);
    CGContextSetLineJoin(c, 1);
    CGContextSetLineCap(c, 1);
    CGContextBeginPath(c);
    CGContextMoveToPoint(c, 150, 20); CGContextAddLineToPoint(c, 280, 30); CGContextAddLineToPoint(c, 200, 90);
    CGContextStrokePath(c);
    double dash[2] = {10, 6};
    CGContextSetLineDash(c, 0, dash, 2);
    CGContextSetLineWidth(c, 3);
    cx_bm_set_stroke(c, 0xFF008000u);
    CGContextBeginPath(c);
    CGContextAddArc(c, 340, 60, 40, 0, 6.2831853, 0);
    CGContextStrokePath(c);
    CGContextSetLineDash(c, 0, 0, 0);
    CGContextSaveGState(c);
    CGContextBeginPath(c);
    CGContextAddRect(c, (CGRect){{20, 110}, {120, 80}});
    CGContextAddRect(c, (CGRect){{50, 130}, {60, 40}});
    CGContextEOClip(c);
    cx_bm_set_fill(c, 0xFF8000C0u);
    CGContextFillRect(c, (CGRect){{0, 100}, {200, 100}});
    CGContextRestoreGState(c);
    CGContextSaveGState(c);
    CGContextTranslateCTM(c, 230, 150);
    CGContextRotateCTM(c, 0.5);
    cx_bm_set_fill(c, 0xFF00A0A0u);
    CGContextFillRect(c, (CGRect){{-30, -20}, {60, 40}});
    CGContextRestoreGState(c);
    /* alfa e blend multiply */
    CGContextSaveGState(c);
    CGContextSetAlpha(c, 0.5);
    cx_bm_set_fill(c, 0xFF0000FFu);
    CGContextFillRect(c, (CGRect){{300, 120}, {60, 60}});
    CGContextSetAlpha(c, 1);
    CGContextSetBlendMode(c, 1);
    cx_bm_set_fill(c, 0xFFFFFF00u);
    CGContextFillRect(c, (CGRect){{330, 140}, {60, 60}});
    CGContextRestoreGState(c);
    /* imagem RGBA 4x4 com alfa, esticada */
    static uint32_t px[16];
    for (int i = 0; i < 16; i++) px[i] = (i & 1 ? 0xFF000000u | 0x00FF00u * (i % 3 != 0) : 0x80800000u) | (uint32_t)(i * 15);
    void *prov = CGDataProviderCreateWithData(0, px, sizeof px, 0);
    void *rgb = CGColorSpaceCreateDeviceRGB();
    void *img = CGImageCreate(4, 4, 8, 32, 16, rgb, 2 | 0x2000, prov, 0, 0, 0);
    CGContextDrawImage(c, (CGRect){{20, 210}, {80, 80}}, img);
    /* gradiente linear e radial */
    void *cols[2] = {cx_color(1, 0, 0, 1), cx_color(0, 0, 1, 1)};
    void *arr = array(2, cols);
    void *gr = CGGradientCreateWithColors(0, arr, 0);
    CGContextSaveGState(c);
    CGContextClipToRect(c, (CGRect){{120, 210}, {120, 80}});
    CGContextDrawLinearGradient(c, gr, (CGPoint){120, 0}, (CGPoint){240, 0}, 3);
    CGContextRestoreGState(c);
    CGContextSaveGState(c);
    CGContextClipToRect(c, (CGRect){{260, 210}, {120, 80}});
    CGContextDrawRadialGradient(c, gr, (CGPoint){320, 250}, 0, (CGPoint){320, 250}, 40, 2);
    CGContextRestoreGState(c);
    /* camada de transparência com alfa 0,5: os dois retângulos sobrepostos ficam uniformes */
    CGContextSaveGState(c);
    CGContextSetAlpha(c, 0.5);
    CGContextBeginTransparencyLayer(c, 0);
    cx_bm_set_fill(c, 0xFF00C000u);
    CGContextFillRect(c, (CGRect){{20, 310}, {60, 40}});
    CGContextFillRect(c, (CGRect){{50, 330}, {60, 40}});
    CGContextEndTransparencyLayer(c);
    CGContextRestoreGState(c);
    /* sombra (sai como imagem) */
    CGContextSaveGState(c);
    CGContextSetShadowWithColor(c, (CGSize){4, -4}, 3, cx_color(0, 0, 0, 0.6));
    cx_bm_set_fill(c, 0xFFFF8000u);
    CGContextFillRect(c, (CGRect){{140, 310}, {60, 40}});
    CGContextRestoreGState(c);
    /* recorte por máscara cinza (gradiente horizontal) */
    static uint8_t mk[8] = {0, 36, 73, 109, 146, 182, 219, 255};
    void *mp = CGDataProviderCreateWithData(0, mk, 8, 0);
    void *gs = CGColorSpaceCreateDeviceGray();
    void *mimg = CGImageCreate(8, 1, 8, 8, 8, gs, 0, mp, 0, 1, 0);
    CGContextSaveGState(c);
    CGContextClipToMask(c, (CGRect){{240, 310}, {140, 50}}, mimg);
    cx_bm_set_fill(c, 0xFF000080u);
    CGContextFillRect(c, (CGRect){{240, 310}, {140, 50}});
    CGContextRestoreGState(c);
    /* texto */
    cx_bm_set_fill(c, 0xFF000000u);
    draw_text(c, 20, 380, "Hamburg Wxyz 123");
    cx_bm_set_fill(c, 0xFF0000C0u);
    draw_text(c, 20, 420, "mOS PDFContext");
}

int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "uso: pdf|png saida fonte.ttf [cifra]\n"); return 2; }
    FILE *ff = fopen(argv[3], "rb");
    fseek(ff, 0, SEEK_END); nfont = (size_t)ftell(ff); fseek(ff, 0, SEEK_SET);
    font = malloc(nfont); fread(font, 1, nfont, ff); fclose(ff);
    CGRect mb = {{0, 0}, {400, 450}};
    if (!strcmp(argv[1], "png")) {
        void *c = CGBitmapContextCreate(0, 400, 450, 8, 1600, 0, 1);
        cx_bm_set_fill(c, 0xFFFFFFFFu);
        cx_bm_fill_rect(c, 0, 0, 400, 450);
        scene_page1(c);
        FILE *o = fopen(argv[2], "wb");
        fwrite(CGBitmapContextGetData(c), 1, 400 * 450 * 4, o);
        fclose(o);
        return 0;
    }
    FILE *o = fopen(argv[2], "wb");
    struct { void *put; void *rel; } cb = {(void *)put_file, 0};
    void *cons = CGDataConsumerCreate(o, &cb);
    const void *k[8];
    void *v[8];
    int n = 0;
    k[n] = kCGPDFContextTitle; v[n++] = tstr("Relatório de teste — mOS");
    k[n] = kCGPDFContextAuthor; v[n++] = tstr("Mariana");
    k[n] = kCGPDFContextKeywords; v[n++] = tstr("pdf, quartz");
    k[n] = kCGPDFContextCreator; v[n++] = tstr("cgpdf_write_host");
    if (argc > 4) {
        k[n] = kCGPDFContextUserPassword; v[n++] = tstr("usuario");
        k[n] = kCGPDFContextOwnerPassword; v[n++] = tstr("dono");
        k[n] = kCGPDFContextAllowsCopying; v[n++] = (void *)kCFBooleanFalse;
    }
    void *c = CGPDFContextCreate(cons, &mb, tdict(n, k, v));
    CGContextBeginPage(c, 0);
    CGPDFContextBeginTag(c, 100, 0);
    const void *ak[1] = {kCGPDFTagPropertyAlternativeText};
    void *av[1] = {tstr("Figura de teste")};
    CGPDFContextBeginTag(c, 700, tdict(1, ak, av));
    scene_page1(c);
    CGPDFContextEndTag(c);
    CGPDFContextSetURLForRect(c, turl("https://example.org/mos"), (CGRect){{20, 20}, {100, 60}});
    CGPDFContextSetDestinationForRect(c, tstr("fim"), (CGRect){{150, 20}, {100, 60}});
    CGContextEndPage(c);
    /* página 2, paisagem, com destino nomeado */
    CGRect mb2 = {{0, 0}, {300, 200}};
    CGContextBeginPage(c, &mb2);
    CGPDFContextBeginTag(c, 200, 0);
    cx_bm_set_fill(c, 0xFF000000u);
    draw_text(c, 20, 100, "Segunda pagina");
    CGPDFContextEndTag(c);
    CGPDFContextEndTag(c);
    CGPDFContextAddDestinationAtPoint(c, tstr("fim"), (CGPoint){20, 150});
    CGContextEndPage(c);
    const void *ok[2] = {kCGPDFOutlineTitle, kCGPDFOutlineDestination};
    void *ov1[2] = {tstr("Capítulo 1"), tnum(1)}, *ov2[2] = {tstr("Capítulo 2"), tnum(2)};
    void *items[2] = {tdict(2, ok, ov1), tdict(2, ok, ov2)};
    const void *rk[1] = {kCGPDFOutlineChildren};
    void *rv[1] = {array(2, items)};
    CGPDFContextSetOutline(c, tdict(1, rk, rv));
    CGPDFContextClose(c);
    CGContextRelease(c);
    CGDataConsumerRelease(cons);
    fclose(o);
    (void)tdata;
    return 0;
}
