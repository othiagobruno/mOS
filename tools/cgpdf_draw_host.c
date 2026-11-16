/* CGContextDrawPDFPage no host: reaproveita o runtime mínimo do cg_context_test.c e desenha a página 1 de
 * um PDF num bitmap RGBA de W×H (pontos = pixels, fundo branco), gravando o RGBA cru. O
 * tools/cgpdf_draw_host.py gera os PDFs, roda isto e compara com o pdftoppm. Compilação: ver o .py. */
#define main cg_context_main
#include "cg_context_test.c"
#undef main

/* o que o appkit.c faz para um contexto de bitmap */
int cx_bm_pattern_color(const void *c, const void *color, int stroke);
int cx_bm_set_cap(const void *, int);
int cx_bm_set_join(const void *, int);
int cx_bm_set_aa(const void *, int);
static uint32_t argb_of(void *color) {
    double *o = color;
    uint32_t a = (uint32_t)(o[4] * 255 + .5), r = (uint32_t)(o[1] * 255 + .5), g = (uint32_t)(o[2] * 255 + .5), b = (uint32_t)(o[3] * 255 + .5);
    return a << 24 | r << 16 | g << 8 | b;
}
void CGContextSetFillColorWithColor(void *c, void *color) { if (!cx_bm_pattern_color(c, color, 0)) cx_bm_set_fill(c, argb_of(color)); }
void CGContextSetStrokeColorWithColor(void *c, void *color) { if (!cx_bm_pattern_color(c, color, 1)) cx_bm_set_stroke(c, argb_of(color)); }
void CGContextSetAlpha(void *c, double a) { cx_bm_set_alpha(c, a); }
void CGContextSetLineWidth(void *c, double w) { cx_bm_set_line_width(c, w); }
void CGContextClosePath(void *c) { cx_bm_close(c); }
void CGContextFillPath(void *c) { cx_bm_fill(c, 0); }
void CGContextSaveGState(void *c) { cx_bm_save(c); }
void CGContextRestoreGState(void *c) { cx_bm_restore(c); }
void CGContextSetLineCap(void *c, int v) { cx_bm_set_cap(c, v); }
void CGContextSetLineJoin(void *c, int v) { cx_bm_set_join(c, v); }
void CGContextSetShouldAntialias(void *c, int v) { cx_bm_set_aa(c, v); }

void CGContextDrawPDFPage(void *ctx, void *page);
void CGContextSetRGBFillColor(void *c, double r, double g, double b, double a);
void CGContextFillRect(void *c, CGRect r);

int cx_pdf_page_text(void *page, uint16_t **chars, CGRect **boxes);
/* modo texto: o texto de cada página em UTF-8, páginas separadas por \f (como o pdftotext) */
static int dump_text(const char *path) {
    url_path = path;
    void *d = CGPDFDocumentCreateWithURL((void *)1);
    if (!d) return 1;
    for (size_t p = 1; p <= CGPDFDocumentGetNumberOfPages(d); p++) {
        uint16_t *ch; CGRect *bx;
        int n = cx_pdf_page_text(CGPDFDocumentGetPage(d, p), &ch, &bx);
        for (int i = 0; i < n; i++) {
            unsigned c = ch[i];
            if (c >= 0xD800 && c < 0xDC00 && i + 1 < n) { c = 0x10000 + ((c - 0xD800) << 10) + (ch[i + 1] - 0xDC00); i++; }
            if (c < 0x80) putchar((int)c);
            else if (c < 0x800) { putchar(0xC0 | c >> 6); putchar(0x80 | (c & 63)); }
            else if (c < 0x10000) { putchar(0xE0 | c >> 12); putchar(0x80 | (c >> 6 & 63)); putchar(0x80 | (c & 63)); }
            else { putchar(0xF0 | c >> 18); putchar(0x80 | (c >> 12 & 63)); putchar(0x80 | (c >> 6 & 63)); putchar(0x80 | (c & 63)); }
        }
        free(ch); free(bx);
        putchar('\f');
    }
    return 0;
}
int main(int argc, char **argv) {
    if (argc == 3 && !strcmp(argv[2], "text")) return dump_text(argv[1]);
    if (argc < 5) { fprintf(stderr, "uso: %s in.pdf out.rgba W H | in.pdf text\n", argv[0]); return 2; }
    int W = atoi(argv[3]), H = atoi(argv[4]);
    url_path = argv[1];
    void *d = CGPDFDocumentCreateWithURL((void *)1);
    if (!d) { fprintf(stderr, "não abriu %s\n", argv[1]); return 1; }
    void *pg = CGPDFDocumentGetPage(d, 1);
    void *c = CGBitmapContextCreate(0, (size_t)W, (size_t)H, 8, (size_t)W * 4, 0, 1);
    cx_bm_set_fill(c, 0xFFFFFFFFu);
    cx_bm_fill_rect(c, 0, 0, W, H);
    CGContextDrawPDFPage(c, pg);
    FILE *f = fopen(argv[2], "wb");
    fwrite(CGBitmapContextGetData(c), 1, (size_t)W * H * 4, f);
    fclose(f);
    CGContextRelease(c);
    CGPDFDocumentRelease(d);
    return 0;
}
