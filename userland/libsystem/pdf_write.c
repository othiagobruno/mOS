/*
 * mOS PDF — regravação: monta um PDF novo com páginas copiadas de documentos abertos (pdf_core.c), com rotação,
 * caixas e anotações trocadas, Info e outline novos. Base do -[PDFDocument dataRepresentation]/writeToURL: do
 * PDFKit. Os objetos alcançáveis de cada página são copiados uma vez (referências renumeradas); referência a página
 * que não está no arquivo novo vira null (não arrasta a árvore de páginas antiga). Streams saem decifrados: dados
 * com Flate, imagem DCT/JPX/JBIG2/CCITT com o filtro original. Sem CoreFoundation: teste no host
 * (tools/pdf_write_host.py).
 */
#ifdef PDF_HOST
#include <stdlib.h>
#include <string.h>
#else
#include "sys.h"
#include "libc.h"
#endif
#include <stdint.h>
#include "pdf_core.h"
#include "pdf_write.h"

int cx_deflate(const uint8_t *in, size_t n, uint8_t **out, size_t *on);

typedef struct { uint8_t *b; size_t n, cap; } wbuf;
static void wb_put(wbuf *p, const void *d, size_t n) {
    if (p->n + n + 1 > p->cap) {
        size_t c = p->cap ? p->cap * 2 : 1024;
        while (c < p->n + n + 1) c *= 2;
        uint8_t *nb = realloc(p->b, c);
        if (!nb) return;
        p->b = nb; p->cap = c;
    }
    memcpy(p->b + p->n, d, n);
    p->n += n;
}
static void wb_s(wbuf *p, const char *s) { wb_put(p, s, strlen(s)); }
static void wb_i(wbuf *p, long long v) {
    char t[24], o[24];
    int k = 0, neg = v < 0;
    unsigned long long u = neg ? (unsigned long long)-v : (unsigned long long)v;
    do { t[k++] = (char)('0' + u % 10); u /= 10; } while (u);
    if (neg) t[k++] = '-';
    for (int i = 0; i < k; i++) o[i] = t[k - 1 - i];
    wb_put(p, o, (size_t)k);
}
static void wb_r(wbuf *p, double v) {
    if (v != v) v = 0;
    long long q = (long long)(v * 1000000 + (v < 0 ? -0.5 : 0.5));
    if (q < 0) { wb_s(p, "-"); q = -q; }
    wb_i(p, q / 1000000);
    long f = (long)(q % 1000000);
    if (f) {
        char d[8];
        d[0] = '.';
        for (int i = 6; i >= 1; i--) { d[i] = (char)('0' + f % 10); f /= 10; }
        int n = 7;
        while (n > 1 && d[n - 1] == '0') n--;
        wb_put(p, d, (size_t)n);
    }
}
static void wb_name(wbuf *p, const char *s) {
    wb_s(p, "/");
    static const char hx[] = "0123456789ABCDEF";
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c < 0x21 || c > 0x7E || strchr("#()<>[]{}/%", c)) { char e[3] = {'#', hx[c >> 4], hx[c & 15]}; wb_put(p, e, 3); }
        else wb_put(p, s, 1);
    }
}
static void wb_hex(wbuf *p, const uint8_t *s, size_t n) {
    static const char hx[] = "0123456789ABCDEF";
    wb_s(p, "<");
    for (size_t i = 0; i < n; i++) { char c[2] = {hx[s[i] >> 4], hx[s[i] & 15]}; wb_put(p, c, 2); }
    wb_s(p, ">");
}

typedef struct { pdf_doc *doc; uint32_t num; int id; } wmap;
typedef struct { pdf_doc *doc; pdf_obj *page; int id; } wpage;
struct pdfwr {
    wbuf out;
    uint32_t *xref; int nobj, cap;
    wmap *map; int nmap, capmap;          /* (documento, objeto) → id novo; tabela aberta */
    struct { pdf_doc *doc; uint32_t num; int id; } *queue; int nq, capq;
    wpage *pages; int npages;
    int pages_id, info_id, catalog_id;
    wbuf info;
    int outline_id;
};

static int w_newid(pdfwr *w) {
    if (w->nobj + 2 > w->cap) {
        int c = w->cap ? w->cap * 2 : 256;
        uint32_t *x = realloc(w->xref, (size_t)c * 4);
        if (!x) return 0;
        memset(x + w->cap, 0, (size_t)(c - w->cap) * 4);
        w->xref = x; w->cap = c;
    }
    return ++w->nobj;
}
static void w_begin(pdfwr *w, int id) { w->xref[id] = (uint32_t)w->out.n; wb_i(&w->out, id); wb_s(&w->out, " 0 obj\n"); }
static void w_end(pdfwr *w) { wb_s(&w->out, "\nendobj\n"); }

static unsigned w_hash(pdf_doc *d, uint32_t n, int cap) { return (unsigned)(((uintptr_t)d >> 4) * 2654435761u ^ n * 40503u) & (unsigned)(cap - 1); }
static int *w_slot(pdfwr *w, pdf_doc *d, uint32_t num, int create) {
    if (create && (w->nmap + 1) * 2 > w->capmap) {
        int nc = w->capmap ? w->capmap * 2 : 1024;
        wmap *nm = calloc((size_t)nc, sizeof *nm);
        if (!nm) return 0;
        for (int i = 0; i < w->capmap; i++)
            if (w->map[i].id) {
                unsigned h = w_hash(w->map[i].doc, w->map[i].num, nc);
                while (nm[h].id) h = (h + 1) & (unsigned)(nc - 1);
                nm[h] = w->map[i];
            }
        free(w->map);
        w->map = nm; w->capmap = nc;
    }
    if (!w->capmap) return 0;
    unsigned h = w_hash(d, num, w->capmap);
    while (w->map[h].id) {
        if (w->map[h].doc == d && w->map[h].num == num) return &w->map[h].id;
        h = (h + 1) & (unsigned)(w->capmap - 1);
    }
    if (!create) return 0;
    w->map[h].doc = d; w->map[h].num = num;
    w->nmap++;
    return &w->map[h].id;
}
static int is_type(pdf_doc *d, pdf_obj *o, const char *t) {
    const char *n = pdf_name(pdf_dict_get(d, o, "Type"));
    return n && !strcmp(n, t);
}
/* id novo do objeto indireto num do documento d (enfileira a cópia); 0 = vira null */
static int w_ref(pdfwr *w, pdf_doc *d, uint32_t num) {
    int *s = w_slot(w, d, num, 0);
    if (s) return *s;
    pdf_obj *o = pdf_get_object(d, num);
    if (!o) return 0;
    pdf_obj *dict = o->type == PO_STREAM ? o->v.stream.dict : o;
    if (dict && dict->type == PO_DICT && (is_type(d, dict, "Page") || is_type(d, dict, "Pages") || is_type(d, dict, "Catalog") || is_type(d, dict, "StructTreeRoot")))
        return 0;                                     /* página fora do arquivo novo, árvore antiga, catálogo: null */
    s = w_slot(w, d, num, 1);
    if (!s) return 0;
    *s = w_newid(w);
    if (w->nq + 1 > w->capq) {
        int c = w->capq ? w->capq * 2 : 256;
        void *q = realloc(w->queue, (size_t)c * sizeof *w->queue);
        if (!q) return *s;
        w->queue = q; w->capq = c;
    }
    w->queue[w->nq].doc = d; w->queue[w->nq].num = num; w->queue[w->nq].id = *s;
    w->nq++;
    return *s;
}
static void w_obj(pdfwr *w, wbuf *b, pdf_doc *d, pdf_obj *o, int depth);
static void w_dict(pdfwr *w, wbuf *b, pdf_doc *d, pdf_obj *o, int depth, const char *const *skip) {
    wb_s(b, "<<");
    for (uint32_t i = 0; i < o->n; i++) {
        const char *k = o->v.dict.keys[i];
        int sk = 0;
        for (int j = 0; skip && skip[j]; j++) if (!strcmp(k, skip[j])) sk = 1;
        if (sk) continue;
        /* /Parent que aponta para a árvore de páginas antiga fica de fora */
        if (!strcmp(k, "Parent")) {
            pdf_obj *pv = pdf_resolve(d, o->v.dict.vals[i]);
            if (pv && pv->type == PO_DICT && (is_type(d, pv, "Pages") || is_type(d, pv, "Page"))) continue;
        }
        wb_name(b, k);
        wb_s(b, " ");
        w_obj(w, b, d, o->v.dict.vals[i], depth + 1);
        wb_s(b, "\n");
    }
    wb_s(b, ">>");
}
static void w_obj(pdfwr *w, wbuf *b, pdf_doc *d, pdf_obj *o, int depth) {
    if (!o || depth > 64) { wb_s(b, "null"); return; }
    switch (o->type) {
    case PO_NULL: wb_s(b, "null"); break;
    case PO_BOOL: wb_s(b, o->v.b ? "true" : "false"); break;
    case PO_INT: wb_i(b, o->v.i); break;
    case PO_REAL: wb_r(b, o->v.r); break;
    case PO_NAME: wb_name(b, o->v.name); break;
    case PO_STRING: wb_hex(b, o->v.str, o->n); break;
    case PO_ARRAY:
        wb_s(b, "[");
        for (uint32_t i = 0; i < o->n; i++) { if (i) wb_s(b, " "); w_obj(w, b, d, o->v.arr[i], depth + 1); }
        wb_s(b, "]");
        break;
    case PO_DICT: w_dict(w, b, d, o, depth, 0); break;
    case PO_REF: {
        /* referência a uma página do arquivo novo: o id dela */
        for (int i = 0; i < w->npages; i++)
            if (w->pages[i].doc == d && pdf_resolve(d, o) == w->pages[i].page) { wb_i(b, w->pages[i].id); wb_s(b, " 0 R"); return; }
        int id = w_ref(w, d, o->v.ref.num);
        if (id) { wb_i(b, id); wb_s(b, " 0 R"); } else wb_s(b, "null");
        break;
    }
    default: wb_s(b, "null"); break;      /* stream direto não existe; PO_OP não aparece em objetos */
    }
}
/* stream: dados decodificados com Flate, ou o filtro de imagem original */
static void w_stream(pdfwr *w, pdf_doc *d, pdf_obj *s, int id) {
    size_t n = 0;
    const char *imgf = 0;
    const uint8_t *data = pdf_stream_data(d, s, &n, &imgf);
    pdf_obj *dict = s->v.stream.dict;
    static const char *const skip[] = {"Length", "Filter", "DecodeParms", "F", "DP", "FFilter", "FDecodeParms", "DL", 0};
    wbuf h = {0};
    if (dict && dict->type == PO_DICT) {
        w_dict(w, &h, d, dict, 0, skip);
        h.n -= 2;                                     /* tira o ">>" para acrescentar */
    } else wb_s(&h, "<<");
    uint8_t *z = 0;
    size_t zn = 0;
    if (imgf) {
        wb_s(&h, " /Filter /"); wb_s(&h, imgf);
        pdf_obj *p = pdf_dict_get(d, s, "DecodeParms");
        if (p && p->type == PO_ARRAY) p = p->n ? pdf_resolve(d, p->v.arr[p->n - 1]) : 0;
        if (p && p->type == PO_DICT) { wb_s(&h, " /DecodeParms "); w_obj(w, &h, d, p, 1); }
    } else if (data && n > 64 && cx_deflate(data, n, &z, &zn) == 0 && zn < n) {
        wb_s(&h, " /Filter /FlateDecode");
        data = z; n = zn;
    }
    if (!data) n = 0;
    wb_s(&h, " /Length "); wb_i(&h, (long long)n); wb_s(&h, ">>\nstream\n");
    w_begin(w, id);
    wb_put(&w->out, h.b, h.n);
    wb_put(&w->out, data, n);
    wb_s(&w->out, "\nendstream");
    w_end(w);
    free(h.b);
    free(z);
}
static void w_drain(pdfwr *w) {
    for (int i = 0; i < w->nq; i++) {
        pdf_doc *d = w->queue[i].doc;
        int id = w->queue[i].id;
        pdf_obj *o = pdf_get_object(d, w->queue[i].num);
        if (o && o->type == PO_STREAM) { w_stream(w, d, o, id); continue; }
        wbuf b = {0};
        w_obj(w, &b, d, o && o->type == PO_REF ? pdf_resolve(d, o) : o, 0);
        w_begin(w, id);
        wb_put(&w->out, b.b, b.n);
        w_end(w);
        free(b.b);
    }
    w->nq = 0;
}

pdfwr *pdfwr_new(void) {
    pdfwr *w = calloc(1, sizeof *w);
    if (!w) return 0;
    wb_s(&w->out, "%PDF-1.7\n%\xE2\xE3\xCF\xD3\n");
    w->catalog_id = w_newid(w);
    w->pages_id = w_newid(w);
    w->info_id = w_newid(w);
    return w;
}
/* reserva a página (a ordem do arquivo novo) antes de copiar: links entre páginas copiadas se resolvem */
int pdfwr_reserve(pdfwr *w, pdf_doc *src, pdf_obj *page) {
    wpage *n = realloc(w->pages, (size_t)(w->npages + 1) * sizeof *n);
    if (!n) return -1;
    w->pages = n;
    w->pages[w->npages].doc = src;
    w->pages[w->npages].page = page;
    w->pages[w->npages].id = w_newid(w);
    return w->npages++;
}
static void w_box(wbuf *b, const char *key, const double *v) {
    wb_s(b, " /"); wb_s(b, key); wb_s(b, " [");
    for (int i = 0; i < 4; i++) { if (i) wb_s(b, " "); wb_r(b, v[i]); }
    wb_s(b, "]");
}
/* troca @P<n>@ (página n do arquivo novo) pela referência, ou null */
static void w_raw(pdfwr *w, wbuf *b, const char *s) {
    while (*s) {
        if (s[0] == '@' && s[1] == 'P') {
            const char *e = s + 2;
            int n = 0, any = 0;
            while (*e >= '0' && *e <= '9') { n = n * 10 + (*e - '0'); e++; any = 1; }
            if (any && *e == '@') {
                if (n >= 0 && n < w->npages) { wb_i(b, w->pages[n].id); wb_s(b, " 0 R"); }
                else wb_s(b, "null");
                s = e + 1;
                continue;
            }
        }
        wb_put(b, s, 1);
        s++;
    }
}
int pdfwr_page(pdfwr *w, int index, const pdfwr_page_opts *o) {
    if (index < 0 || index >= w->npages) return -1;
    pdf_doc *d = w->pages[index].doc;
    pdf_obj *pg = w->pages[index].page;
    static const char *const skip[] = {"Parent", "Annots", "MediaBox", "CropBox", "Rotate", "Resources", "StructParents", "B", "Type", 0};
    wbuf b = {0};
    if (pg) {
        w_dict(w, &b, d, pg, 0, skip);
        b.n -= 2;
    } else wb_s(&b, "<<");
    wb_s(&b, " /Type /Page /Parent "); wb_i(&b, w->pages_id); wb_s(&b, " 0 R");
    double mb[4] = {0, 0, 612, 792}, cb[4];
    int hascb = 0, rot = 0;
    if (pg) {
        pdf_obj *m = pdf_page_attr(d, pg, "MediaBox"), *c = pdf_page_attr(d, pg, "CropBox"), *r = pdf_page_attr(d, pg, "Rotate");
        for (int i = 0; m && m->type == PO_ARRAY && i < 4 && i < (int)m->n; i++) pdf_num(pdf_resolve(d, m->v.arr[i]), &mb[i]);
        if (c && c->type == PO_ARRAY && c->n >= 4) { hascb = 1; for (int i = 0; i < 4; i++) pdf_num(pdf_resolve(d, c->v.arr[i]), &cb[i]); }
        double rv = 0;
        if (pdf_num(r, &rv)) rot = (int)rv;
        pdf_obj *res = pdf_page_attr(d, pg, "Resources");
        wb_s(&b, " /Resources ");
        if (res) w_obj(w, &b, d, res, 1); else wb_s(&b, "<<>>");
    } else wb_s(&b, " /Resources <<>>");
    if (o && o->has_media) memcpy(mb, o->media, sizeof mb);
    if (o && o->has_crop) { memcpy(cb, o->crop, sizeof cb); hascb = 1; }
    if (o && o->rotate >= 0) rot = o->rotate;
    w_box(&b, "MediaBox", mb);
    if (hascb) w_box(&b, "CropBox", cb);
    if (rot % 360) { wb_s(&b, " /Rotate "); wb_i(&b, ((rot % 360) + 360) % 360); }
    /* anotações: as de origem copiadas e as novas (texto de dicionário com @P<n>@) */
    if (o && o->nannots >= 0) {
        if (o->nannots) {
            wb_s(&b, " /Annots [");
            for (int i = 0; i < o->nannots; i++) {
                const pdfwr_annot *a = &o->annots[i];
                if (a->raw) {
                    int id = w_newid(w);
                    wbuf ab = {0};
                    w_raw(w, &ab, a->raw);
                    wbuf full = {0};
                    /* /P da página nova */
                    if (ab.n >= 2 && ab.b[0] == '<' && ab.b[1] == '<') {
                        wb_s(&full, "<< /P "); wb_i(&full, w->pages[index].id); wb_s(&full, " 0 R ");
                        wb_put(&full, ab.b + 2, ab.n - 2);
                    } else wb_put(&full, ab.b, ab.n);
                    w_begin(w, id);
                    wb_put(&w->out, full.b, full.n);
                    w_end(w);
                    free(ab.b); free(full.b);
                    wb_i(&b, id); wb_s(&b, " 0 R ");
                } else if (a->obj) {
                    w_obj(w, &b, a->doc ? a->doc : d, a->obj, 1);
                    wb_s(&b, " ");
                }
            }
            wb_s(&b, "]");
        }
    } else if (pg) {
        pdf_obj *an = pdf_dict_get_raw(pg, "Annots");
        if (an) { wb_s(&b, " /Annots "); w_obj(w, &b, d, an, 1); }
    }
    wb_s(&b, " >>");
    w_begin(w, w->pages[index].id);
    wb_put(&w->out, b.b, b.n);
    w_end(w);
    free(b.b);
    w_drain(w);
    return 0;
}
static void w_text(wbuf *b, const char *u8s) {
    size_t n = strlen(u8s);
    int ascii = 1;
    for (size_t i = 0; i < n; i++) if ((unsigned char)u8s[i] >= 0x80) ascii = 0;
    if (ascii) { wb_hex(b, (const uint8_t *)u8s, n); return; }
    uint8_t *t = malloc(n * 4 + 2);
    if (!t) return;
    size_t k = 0;
    t[k++] = 0xFE; t[k++] = 0xFF;
    for (size_t i = 0; i < n;) {
        uint32_t c = (unsigned char)u8s[i], ex = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
        c = ex == 3 ? c & 7 : ex == 2 ? c & 15 : ex == 1 ? c & 31 : c;
        i++;
        for (uint32_t e = 0; e < ex && i < n; e++, i++) c = c << 6 | ((unsigned char)u8s[i] & 63);
        if (c >= 0x10000) { c -= 0x10000; uint32_t hi = 0xD800 + (c >> 10), lo = 0xDC00 + (c & 1023); t[k++] = (uint8_t)(hi >> 8); t[k++] = (uint8_t)hi; t[k++] = (uint8_t)(lo >> 8); t[k++] = (uint8_t)lo; }
        else { t[k++] = (uint8_t)(c >> 8); t[k++] = (uint8_t)c; }
    }
    wb_hex(b, t, k);
    free(t);
}
void pdfwr_info(pdfwr *w, const char *key, const char *utf8) {
    if (!key || !utf8) return;
    wb_s(&w->info, " /"); wb_s(&w->info, key); wb_s(&w->info, " ");
    w_text(&w->info, utf8);
}
/* outline: irmãos ligados por Prev/Next, filhos por First/Last; destino /XYZ na página do arquivo novo */
static int w_outline(pdfwr *w, const pdfwr_outline *items, int n, int parent, int *first, int *last) {
    if (n <= 0) return 0;
    int *ids = malloc((size_t)n * sizeof *ids);
    if (!ids) return 0;
    for (int i = 0; i < n; i++) ids[i] = w_newid(w);
    int total = n;
    for (int i = 0; i < n; i++) {
        const pdfwr_outline *it = &items[i];
        int f = 0, l = 0, c = 0;
        if (it->nkids) c = w_outline(w, it->kids, it->nkids, ids[i], &f, &l);
        total += it->open ? c : 0;
        wbuf b = {0};
        wb_s(&b, "<< /Title "); w_text(&b, it->title ? it->title : "");
        wb_s(&b, " /Parent "); wb_i(&b, parent); wb_s(&b, " 0 R");
        if (i) { wb_s(&b, " /Prev "); wb_i(&b, ids[i - 1]); wb_s(&b, " 0 R"); }
        if (i + 1 < n) { wb_s(&b, " /Next "); wb_i(&b, ids[i + 1]); wb_s(&b, " 0 R"); }
        if (it->page >= 0 && it->page < w->npages) {
            wb_s(&b, " /Dest ["); wb_i(&b, w->pages[it->page].id); wb_s(&b, " 0 R /XYZ ");
            wb_r(&b, it->x); wb_s(&b, " "); wb_r(&b, it->y); wb_s(&b, " 0]");
        } else if (it->uri) { wb_s(&b, " /A << /S /URI /URI "); wb_hex(&b, (const uint8_t *)it->uri, strlen(it->uri)); wb_s(&b, " >>"); }
        if (it->nkids) {
            wb_s(&b, " /First "); wb_i(&b, f); wb_s(&b, " 0 R /Last "); wb_i(&b, l); wb_s(&b, " 0 R /Count ");
            wb_i(&b, it->open ? c : -c);
        }
        wb_s(&b, " >>");
        w_begin(w, ids[i]);
        wb_put(&w->out, b.b, b.n);
        w_end(w);
        free(b.b);
    }
    *first = ids[0]; *last = ids[n - 1];
    free(ids);
    return total;
}
void pdfwr_outline_set(pdfwr *w, const pdfwr_outline *items, int n) {
    if (n <= 0) return;
    w->outline_id = w_newid(w);
    int f = 0, l = 0;
    int c = w_outline(w, items, n, w->outline_id, &f, &l);
    w_begin(w, w->outline_id);
    wb_s(&w->out, "<< /Type /Outlines /First "); wb_i(&w->out, f); wb_s(&w->out, " 0 R /Last "); wb_i(&w->out, l);
    wb_s(&w->out, " 0 R /Count "); wb_i(&w->out, c); wb_s(&w->out, " >>");
    w_end(w);
}
int pdfwr_finish(pdfwr *w, uint8_t **out, size_t *len) {
    w_begin(w, w->pages_id);
    wb_s(&w->out, "<< /Type /Pages /Kids [");
    for (int i = 0; i < w->npages; i++) { wb_i(&w->out, w->pages[i].id); wb_s(&w->out, " 0 R "); }
    wb_s(&w->out, "] /Count "); wb_i(&w->out, w->npages); wb_s(&w->out, " >>");
    w_end(w);
    w_begin(w, w->info_id);
    wb_s(&w->out, "<< /Producer "); w_text(&w->out, "mOS PDFKit");
    if (w->info.n) wb_put(&w->out, w->info.b, w->info.n);
    wb_s(&w->out, " >>");
    w_end(w);
    w_begin(w, w->catalog_id);
    wb_s(&w->out, "<< /Type /Catalog /Pages "); wb_i(&w->out, w->pages_id); wb_s(&w->out, " 0 R");
    if (w->outline_id) { wb_s(&w->out, " /Outlines "); wb_i(&w->out, w->outline_id); wb_s(&w->out, " 0 R"); }
    wb_s(&w->out, " >>");
    w_end(w);
    size_t x = w->out.n;
    wb_s(&w->out, "xref\n0 "); wb_i(&w->out, w->nobj + 1); wb_s(&w->out, "\n0000000000 65535 f \n");
    for (int i = 1; i <= w->nobj; i++) {
        char line[21];
        uint32_t v = w->xref[i];
        if (!v) { memcpy(line, "0000000000 65535 f \n", 20); }
        else { for (int k = 9; k >= 0; k--) { line[k] = (char)('0' + v % 10); v /= 10; } memcpy(line + 10, " 00000 n \n", 10); }
        wb_put(&w->out, line, 20);
    }
    wb_s(&w->out, "trailer\n<< /Size "); wb_i(&w->out, w->nobj + 1);
    wb_s(&w->out, " /Root "); wb_i(&w->out, w->catalog_id); wb_s(&w->out, " 0 R /Info "); wb_i(&w->out, w->info_id);
    wb_s(&w->out, " 0 R >>\nstartxref\n"); wb_i(&w->out, (long long)x); wb_s(&w->out, "\n%%EOF\n");
    *out = w->out.b;
    *len = w->out.n;
    w->out.b = 0;
    return 0;
}
void pdfwr_free(pdfwr *w) {
    if (!w) return;
    free(w->out.b); free(w->xref); free(w->map); free(w->queue); free(w->pages); free(w->info.b);
    free(w);
}
