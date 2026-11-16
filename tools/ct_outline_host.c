/* Contornos do ct_outline.c no host: serve as tabelas de um .ttf/.otf como o kernel (GUI_FONT/GF_TABLE), preenche
 * o caminho do glifo num CGBitmapContext do harness e grava a cobertura (PGM). tools/ct_outline_host.py compara
 * com o FreeType (PIL). Compilação: ver o .py. */
#define CG_TEST_OWN_SYS5 1
#define main cg_context_main
#include "cg_context_test.c"
#undef main
void *cx_font_glyph_path(int face, int gid, double size, const CGAffineTransform *m);
int ct_host_cmap(const uint8_t *file, size_t n, uint32_t cp);
CGRect CGPathGetPathBoundingBox(const void *p);
int cx_bm_fill(const void *, int);
void CGContextFillPath(void *c) { cx_bm_fill(c, 0); }
static uint8_t *font; static size_t nfont;
static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
long ct_host_sys5(long n, long op, long face, long tag, void *buf, long cap) {
    (void)n; (void)face;
    if (op == 1) return ct_host_cmap(font, nfont, (uint32_t)tag);   /* GF_CMAP */
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
int cx_font_metrics(int face, double *m);
int main(int argc, char **argv) {
    if (argc == 3 && !strcmp(argv[2], "metricas")) {      /* métricas da face: tools/ct_outline_host.py */
        FILE *f = fopen(argv[1], "rb");
        fseek(f, 0, SEEK_END); nfont = (size_t)ftell(f); fseek(f, 0, SEEK_SET);
        font = malloc(nfont); fread(font, 1, nfont, f); fclose(f);
        double m[13];
        if (!cx_font_metrics(0, m)) return 1;
        for (int i = 0; i < 13; i++) printf("%.6f ", m[i]);
        printf("\n");
        return 0;
    }
    if (argc < 5) { fprintf(stderr, "uso: fonte.ttf codepoint tamanho saida.pgm\n"); return 2; }
    FILE *f = fopen(argv[1], "rb");
    fseek(f, 0, SEEK_END); nfont = (size_t)ftell(f); fseek(f, 0, SEEK_SET);
    font = malloc(nfont); fread(font, 1, nfont, f); fclose(f);
    int gid = ct_host_cmap(font, nfont, (uint32_t)strtoul(argv[2], 0, 0));
    double size = atof(argv[3]);
    void *p = cx_font_glyph_path(0, gid, size, 0);
    if (!p) { fprintf(stderr, "sem contorno (gid %d)\n", gid); return 1; }
    int W = (int)(size * 2), H = (int)(size * 2);
    void *c = CGBitmapContextCreate(0, (size_t)W, (size_t)H, 8, (size_t)W * 4, 0, 1);
    cx_bm_set_fill(c, 0xFF000000u);
    CGContextTranslateCTM(c, size / 2, size / 2);
    CGContextAddPath(c, p);
    CGContextFillPath(c);
    FILE *o = fopen(argv[4], "wb");
    fprintf(o, "P5\n%d %d\n255\n", W, H);
    uint8_t *d = CGBitmapContextGetData(c);
    for (int i = 0; i < W * H; i++) fputc(d[i * 4 + 3], o);
    fclose(o);
    printf("gid %d\n", gid);
    return 0;
}
