/* pdf_write.c no host: monta PDFs novos a partir de PDFs abertos pelo pdf_core.c.
 *   gcc -O1 -DPDF_HOST -Iuserland/libsystem tools/pdf_write_host.c userland/libsystem/pdf_write.c userland/libsystem/pdf_core.c -lz -o /tmp/pdf_write_host
 * Uso: pdf_write_host saida.pdf modo entrada.pdf [entrada2.pdf] [senha]
 *   modos: reverse (páginas ao contrário), merge (entrada + entrada2), rotate (90° na 1ª, MediaBox nova na 2ª),
 *          annot (link para a última página + nota, anotações de origem mantidas), outline (outline e Info) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include "pdf_core.h"
#include "pdf_write.h"
int cx_deflate(const uint8_t *in, size_t n, uint8_t **out, size_t *on) {
    uLongf cap = compressBound((uLong)n);
    *out = malloc(cap);
    if (!*out || compress2(*out, &cap, in, (uLong)n, 6) != Z_OK) { free(*out); return -1; }
    *on = cap;
    return 0;
}
static pdf_doc *load(const char *path, const char *pw) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc((size_t)n); fread(b, 1, (size_t)n, f); fclose(f);
    pdf_doc *d = pdf_open(b, (size_t)n);
    free(b);
    if (d && pdf_is_encrypted(d) && !pdf_is_unlocked(d)) pdf_unlock(d, pw ? pw : "");
    return d;
}
int main(int argc, char **argv) {
    if (argc < 4) return 2;
    const char *mode = argv[2];
    pdf_doc *a = load(argv[3], argc > 5 ? argv[5] : 0), *b = argc > 4 && strcmp(argv[4], "-") ? load(argv[4], 0) : 0;
    if (!a) { fprintf(stderr, "não abriu\n"); return 1; }
    pdfwr *w = pdfwr_new();
    int na = pdf_page_count(a), nb = b ? pdf_page_count(b) : 0;
    if (!strcmp(mode, "reverse")) {
        for (int i = na - 1; i >= 0; i--) pdfwr_reserve(w, a, pdf_page(a, i));
        for (int i = 0; i < na; i++) pdfwr_page(w, i, 0);
    } else if (!strcmp(mode, "merge")) {
        for (int i = 0; i < na; i++) pdfwr_reserve(w, a, pdf_page(a, i));
        for (int i = 0; i < nb; i++) pdfwr_reserve(w, b, pdf_page(b, i));
        for (int i = 0; i < na + nb; i++) pdfwr_page(w, i, 0);
    } else if (!strcmp(mode, "rotate")) {
        for (int i = 0; i < na; i++) pdfwr_reserve(w, a, pdf_page(a, i));
        for (int i = 0; i < na; i++) {
            pdfwr_page_opts o = {-1, 0, 0, {0}, {0}, 0, -1};
            if (i == 0) o.rotate = 90;
            if (i == 1) { o.has_media = 1; o.media[0] = 0; o.media[1] = 0; o.media[2] = 200; o.media[3] = 150; }
            pdfwr_page(w, i, &o);
        }
    } else if (!strcmp(mode, "annot")) {
        for (int i = 0; i < na; i++) pdfwr_reserve(w, a, pdf_page(a, i));
        for (int i = 0; i < na; i++) {
            pdf_obj *orig = pdf_dict_get(a, pdf_page(a, i), "Annots");
            pdfwr_annot an[16];
            int n = 0;
            for (unsigned k = 0; orig && orig->type == PO_ARRAY && k < orig->n && n < 14; k++) { an[n].doc = a; an[n].obj = orig->v.arr[k]; an[n].raw = 0; n++; }
            if (i == 0) {
                an[n].doc = 0; an[n].obj = 0; an[n].raw = "<< /Type /Annot /Subtype /Link /Rect [10 10 60 40] /Border [0 0 0] /Dest [@P1@ /XYZ 0 100 0] >>"; n++;
                an[n].doc = 0; an[n].obj = 0; an[n].raw = "<< /Type /Annot /Subtype /Text /Rect [100 100 120 120] /Contents (nota do mOS) /C [1 1 0] >>"; n++;
            }
            pdfwr_page_opts o = {-1, 0, 0, {0}, {0}, an, n};
            pdfwr_page(w, i, &o);
        }
    } else if (!strcmp(mode, "outline")) {
        for (int i = 0; i < na; i++) pdfwr_reserve(w, a, pdf_page(a, i));
        for (int i = 0; i < na; i++) pdfwr_page(w, i, 0);
        pdfwr_outline kids[1] = {{"Seção 1.1", 0, 0, 50, 0, 1, 0, 0}};
        pdfwr_outline top[2] = {{"Capítulo 1", 0, 0, 100, 0, 1, kids, 1}, {"Capítulo 2", na > 1 ? 1 : 0, 0, 100, 0, 0, 0, 0}};
        pdfwr_outline_set(w, top, 2);
        pdfwr_info(w, "Title", "Documento regravado — mOS");
        pdfwr_info(w, "Author", "PDFKit");
    }
    uint8_t *out; size_t n;
    pdfwr_finish(w, &out, &n);
    pdfwr_free(w);
    FILE *f = fopen(argv[1], "wb"); fwrite(out, 1, n, f); fclose(f);
    free(out);
    return 0;
}
