/* mOS PDF — regravação de PDF a partir de páginas abertas (pdf_write.c). */
#ifndef MOS_PDF_WRITE_H
#define MOS_PDF_WRITE_H
#include "pdf_core.h"

typedef struct pdfwr pdfwr;
/* anotação da página nova: um objeto de origem (copiado) ou um dicionário em texto, onde @P<n>@ vira a referência
 * da página n do arquivo novo (ou null) e /P é posto pela página */
typedef struct { pdf_doc *doc; pdf_obj *obj; const char *raw; } pdfwr_annot;
typedef struct {
    int rotate;                  /* −1 = o da origem */
    int has_media, has_crop;
    double media[4], crop[4];
    const pdfwr_annot *annots;
    int nannots;                 /* −1 = as da origem como estão */
} pdfwr_page_opts;
typedef struct pdfwr_outline {
    const char *title;           /* UTF-8 */
    int page;                    /* página do arquivo novo (−1 = sem destino) */
    double x, y;
    const char *uri;
    int open;
    const struct pdfwr_outline *kids;
    int nkids;
} pdfwr_outline;

pdfwr *pdfwr_new(void);
/* reserva todas as páginas na ordem final antes de gravar qualquer uma; devolve o índice */
int pdfwr_reserve(pdfwr *w, pdf_doc *src, pdf_obj *page);
int pdfwr_page(pdfwr *w, int index, const pdfwr_page_opts *opts);
void pdfwr_info(pdfwr *w, const char *key, const char *utf8);
void pdfwr_outline_set(pdfwr *w, const pdfwr_outline *items, int n);
int pdfwr_finish(pdfwr *w, uint8_t **out, size_t *len);   /* *out é do chamador (free) */
void pdfwr_free(pdfwr *w);

#endif
