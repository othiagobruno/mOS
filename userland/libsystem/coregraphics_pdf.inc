/* mOS CoreGraphics — CGPDFContext: grava PDF de verdade (ISO 32000-1). Incluído no fim do coregraphics.c.
 *
 * O contexto PDF é um CGBitmapContext do tamanho da página (1 px = 1 pt) com um gravador (o->pdf): todo o estado
 * (CTM, path, recorte, cores, traço, texto) é o do bitmap, e os pontos de pintura viram operadores do PDF:
 *   - path preenchido ou traçado (o traço sai como o contorno já calculado, com tracejado e junções exatos);
 *   - recortes por path/retângulos (W n) e por máscara (SMask de luminosidade no ExtGState);
 *   - imagens (XObject com SMask para o alfa), gradientes e CGShading (shading 2/3 com função amostrada);
 *   - texto do CTFont em fonte Type3 com os contornos da face e ToUnicode (selecionável e pesquisável);
 *   - camadas de transparência como form XObject com grupo de transparência;
 *   - o resto (sombra, padrões, gradiente cônico, glifo sem contorno, blend Porter-Duff) é pintado no bitmap e
 *     sai como imagem na ordem certa (o que foi pintado em cima continua em cima).
 * Também: várias páginas e caixas, metadados (Info, XMP), links para URL e destinos nomeados, outline, tags
 * (marked content + árvore de estrutura), PDF/A-2B (OutputIntent sRGB) e cifra padrão RC4 40/128 (R2/R3).
 * Teste: tools/cgpdf_write_host.py (o PDF gerado relido pelo poppler e pelo próprio leitor). */

#include "pdf_core.h"
int cx_deflate(const u8 *in, size_t n, u8 **out, size_t *on);
long cx_consumer_put(void *consumer, const void *bytes, size_t n);
void *CGDataConsumerCreateWithURL(id url);
void *CGDataConsumerRetain(void *c);
void CGDataConsumerRelease(void *c);
void *cx_font_glyph_path(int face, int gid, double size, const CGAffineTransform *m);
double cx_font_glyph_advance(int face, int gid);
u32 cx_font_glyph_unicode(int face, int gid);
int cx_font_outline_kind(int face);
id CFDictionaryGetValue(id d, const void *k);
long CFArrayGetCount(id a);
id CFArrayGetValueAtIndex(id a, long i);
u8 CFNumberGetValue(id n, long type, void *out);
const u8 *CFDataGetBytePtr(id d);
long CFDataGetLength(id d);
id CFURLGetString(id url);
id CFRetain(id o);
extern const void *kCFBooleanTrue;
extern const void *kCGColorSpaceSRGB;
void *CGColorSpaceCreateWithName(const void *name);
id CGColorSpaceCopyICCData(void *space);
void CGColorSpaceRelease(void *s);
CGRect CGPathGetPathBoundingBox(const void *p);
void CGPathRelease(void *p);
extern char pw_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } pw_const_t;
#define PWK(var, text) static pw_const_t var##_s = {pw_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; const void *var = &var##_s;
PWK(kCGPDFContextMediaBox, "MediaBox")
PWK(kCGPDFContextCropBox, "CropBox")
PWK(kCGPDFContextBleedBox, "BleedBox")
PWK(kCGPDFContextTrimBox, "TrimBox")
PWK(kCGPDFContextArtBox, "ArtBox")
PWK(kCGPDFContextTitle, "kCGPDFContextTitle")
PWK(kCGPDFContextAuthor, "kCGPDFContextAuthor")
PWK(kCGPDFContextSubject, "kCGPDFContextSubject")
PWK(kCGPDFContextKeywords, "kCGPDFContextKeywords")
PWK(kCGPDFContextCreator, "kCGPDFContextCreator")
PWK(kCGPDFContextOwnerPassword, "kCGPDFContextOwnerPassword")
PWK(kCGPDFContextUserPassword, "kCGPDFContextUserPassword")
PWK(kCGPDFContextEncryptionKeyLength, "kCGPDFContextEncryptionKeyLength")
PWK(kCGPDFContextAllowsPrinting, "kCGPDFContextAllowsPrinting")
PWK(kCGPDFContextAllowsCopying, "kCGPDFContextAllowsCopying")
PWK(kCGPDFContextAccessPermissions, "kCGPDFContextAccessPermissions")
PWK(kCGPDFContextOutputIntent, "kCGPDFContextOutputIntent")
PWK(kCGPDFContextOutputIntents, "kCGPDFContextOutputIntents")
PWK(kCGPDFContextCreateLinearizedPDF, "kCGPDFContextCreateLinearizedPDF")
PWK(kCGPDFContextCreatePDFA, "kCGPDFContextCreatePDFA")
PWK(kCGPDFXOutputIntentSubtype, "S")
PWK(kCGPDFXOutputConditionIdentifier, "OutputConditionIdentifier")
PWK(kCGPDFXOutputCondition, "OutputCondition")
PWK(kCGPDFXRegistryName, "RegistryName")
PWK(kCGPDFXInfo, "Info")
PWK(kCGPDFXDestinationOutputProfile, "DestOutputProfile")
PWK(kCGPDFOutlineTitle, "Title")
PWK(kCGPDFOutlineChildren, "Children")
PWK(kCGPDFOutlineDestination, "Destination")
PWK(kCGPDFOutlineDestinationRect, "DestinationRect")
PWK(kCGPDFTagPropertyActualText, "ActualText")
PWK(kCGPDFTagPropertyAlternativeText, "AlternativeText")
PWK(kCGPDFTagPropertyTitleText, "TitleText")
PWK(kCGPDFTagPropertyLanguageText, "LanguageText")

/* ---------------------------------------------------------------- buffer e números */
typedef struct { u8 *b; size_t n, cap; } pbuf;
static void pb_put(pbuf *p, const void *d, size_t n) {
    if (p->n + n + 1 > p->cap) {
        size_t c = p->cap ? p->cap * 2 : 4096;
        while (c < p->n + n + 1) c *= 2;
        u8 *nb = realloc(p->b, c);
        if (!nb) return;
        p->b = nb; p->cap = c;
    }
    memcpy(p->b + p->n, d, n);
    p->n += n;
}
static void pb_s(pbuf *p, const char *s) { pb_put(p, s, strlen(s)); }
static void pb_int(pbuf *p, long v) {
    char t[24];
    int k = 0, neg = v < 0;
    unsigned long u = neg ? (unsigned long)-v : (unsigned long)v;
    do { t[k++] = (char)('0' + u % 10); u /= 10; } while (u);
    if (neg) t[k++] = '-';
    char o[24];
    for (int i = 0; i < k; i++) o[i] = t[k - 1 - i];
    pb_put(p, o, (size_t)k);
}
/* número com até 4 casas, sem zeros à direita nem notação científica */
static void pb_num(pbuf *p, double v) {
    if (v != v) v = 0;
    if (v > 1e9) v = 1e9;
    if (v < -1e9) v = -1e9;
    long long q = (long long)(v * 10000 + (v < 0 ? -0.5 : 0.5));
    if (q < 0) { pb_s(p, "-"); q = -q; }
    pb_int(p, (long)(q / 10000));
    long f = (long)(q % 10000);
    if (f) {
        char d[6] = {'.', (char)('0' + f / 1000), (char)('0' + f / 100 % 10), (char)('0' + f / 10 % 10), (char)('0' + f % 10), 0};
        int n = 5;
        while (n > 1 && d[n - 1] == '0') n--;
        pb_put(p, d, (size_t)n);
    }
}
static void pb_sp(pbuf *p) { pb_put(p, " ", 1); }
static void pb_nums(pbuf *p, const double *v, int n) { for (int i = 0; i < n; i++) { if (i) pb_sp(p); pb_num(p, v[i]); } }
static void pb_ref(pbuf *p, int id) { pb_int(p, id); pb_s(p, " 0 R"); }

/* ---------------------------------------------------------------- gravador */
typedef struct { int id; char name[12]; } pw_res;
typedef struct { const void *img; u32 color; int id; } pw_imgc;
typedef struct { int ca; int bm; int id; char name[10]; } pw_gsc;
typedef struct { int obj, n; u16 gid[256]; int proc[256]; double w[256]; u32 uni[256]; double bb[4]; char name[10]; } pw_t3;
typedef struct { int face; pw_t3 *f; int nf; u32 *map; } pw_face;
typedef struct { char *name; int page; double x, y; } pw_dest;
typedef struct { int page; double r[4]; char *uri; char *dest; } pw_link;
typedef struct {
    int id, parent, page;
    const char *type;
    char *alt, *actual, *title, *lang;
    int *kid; int nkid;                  /* > 0: MCID + 1 (na página do elemento); < 0: −(elemento + 1) */
    int *kpage;
} pw_elem_t;

typedef struct pdfw {
    void *consumer;
    size_t off;
    u32 *xref; int nobj, capobj;
    int pages_id, res_id, catalog_id, info_id, struct_id;
    int *page_ids; int npages, cappages;
    pbuf rx, rg, rsh, rf;                /* entradas do dicionário de recursos (único, do documento) */
    int nres;
    pw_imgc *imgs; int nimgs;
    pw_gsc *gss; int ngss;
    pw_face *faces; int nfaces;
    pbuf page;                           /* conteúdo da página */
    pbuf *lstack[8]; double lalpha[8]; int lblend[8]; int nlayer;
    int in_page, qdepth;
    double mb[4], defmb[4], boxes[4][4]; int hasbox[4];
    int rdirty, rx0, ry0, rx1, ry1;      /* área do bitmap pintada pelo caminho de raster */
    pw_dest *dests; int ndests;
    pw_link *links; int nlinks;
    id outline;
    id xmp;
    char *title, *author, *subject, *keywords, *creator;
    int pdfa;
    /* cifra */
    int crypt, rev, klen; u8 key[16]; u8 O[32], U[32]; int32_t P; u8 fid[16];
    /* tags */
    pw_elem_t *el; int nel, capel;
    int tagstk[64], ntag;
    int mcid, cur_mc;                    /* próximo MCID da página; elemento do marked content aberto (−1 nenhum) */
    int *pmc; int npmc;                  /* MCID → elemento, da página atual */
    int *ptree; int nptree;              /* StructParents da página k → array de MCIDs (obj) */
    int closed;
} pdfw;

static pbuf *pw_out(pdfw *w) { return w->nlayer ? w->lstack[w->nlayer - 1] : &w->page; }
static void pw_write(pdfw *w, const void *d, size_t n) { if (n) { cx_consumer_put(w->consumer, d, n); w->off += n; } }
static void pw_writes(pdfw *w, const char *s) { pw_write(w, s, strlen(s)); }
static int pw_newid(pdfw *w) {
    if (w->nobj + 2 > w->capobj) {
        int c = w->capobj ? w->capobj * 2 : 256;
        u32 *x = realloc(w->xref, (size_t)c * 4);
        if (!x) return 0;
        memset(x + w->capobj, 0, (size_t)(c - w->capobj) * 4);
        w->xref = x; w->capobj = c;
    }
    return ++w->nobj;
}
static void pw_begin(pdfw *w, int id) {
    w->xref[id] = (u32)w->off;
    pbuf h = {0};
    pb_int(&h, id);
    pb_s(&h, " 0 obj\n");
    pw_write(w, h.b, h.n);
    free(h.b);
}
static void pw_obj(pdfw *w, int id, const pbuf *body) {
    pw_begin(w, id);
    pw_write(w, body->b, body->n);
    pw_writes(w, "\nendobj\n");
}
/* chave RC4 do objeto (algoritmo 1 da norma) */
static void pw_objkey(pdfw *w, int id, u8 *k, int *kl) {
    u8 t[21];
    memcpy(t, w->key, (size_t)w->klen);
    t[w->klen] = (u8)id; t[w->klen + 1] = (u8)(id >> 8); t[w->klen + 2] = (u8)(id >> 16); t[w->klen + 3] = 0; t[w->klen + 4] = 0;
    u8 h[16];
    pdf_md5(t, (size_t)w->klen + 5, h);
    *kl = w->klen + 5 > 16 ? 16 : w->klen + 5;
    memcpy(k, h, (size_t)*kl);
}
/* string do PDF no objeto id: hex, cifrada se o documento tem senha */
static void pw_str(pdfw *w, pbuf *p, int id, const u8 *s, size_t n) {
    u8 *t = malloc(n ? n : 1);
    if (!t) return;
    memcpy(t, s, n);
    if (w->crypt && id) { u8 k[16]; int kl; pw_objkey(w, id, k, &kl); pdf_rc4(k, (size_t)kl, t, n); }
    static const char hx[] = "0123456789ABCDEF";
    pb_s(p, "<");
    for (size_t i = 0; i < n; i++) { char c[2] = {hx[t[i] >> 4], hx[t[i] & 15]}; pb_put(p, c, 2); }
    pb_s(p, ">");
    free(t);
}
/* texto: ASCII puro como está; senão UTF-16BE com BOM */
static void pw_text(pdfw *w, pbuf *p, int id, const char *u8s) {
    size_t n = strlen(u8s);
    int ascii = 1;
    for (size_t i = 0; i < n; i++) if ((u8)u8s[i] >= 0x80) ascii = 0;
    if (ascii) { pw_str(w, p, id, (const u8 *)u8s, n); return; }
    u8 *b = malloc(n * 4 + 2);
    if (!b) return;
    size_t k = 0;
    b[k++] = 0xFE; b[k++] = 0xFF;
    for (size_t i = 0; i < n;) {
        u32 c = (u8)u8s[i], extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
        c = extra == 3 ? c & 7 : extra == 2 ? c & 15 : extra == 1 ? c & 31 : c;
        i++;
        for (u32 e = 0; e < extra && i < n; e++, i++) c = c << 6 | ((u8)u8s[i] & 63);
        if (c >= 0x10000) { c -= 0x10000; u32 hi = 0xD800 + (c >> 10), lo = 0xDC00 + (c & 1023); b[k++] = (u8)(hi >> 8); b[k++] = (u8)hi; b[k++] = (u8)(lo >> 8); b[k++] = (u8)lo; }
        else { b[k++] = (u8)(c >> 8); b[k++] = (u8)c; }
    }
    pw_str(w, p, id, b, k);
    free(b);
}
/* stream: dicionário extra + dados (Flate) cifrados com a chave do objeto */
static void pw_stream(pdfw *w, int id, const char *dict, const u8 *d, size_t n, int flate) {
    u8 *z = 0;
    size_t zn = 0;
    if (flate && n > 32 && cx_deflate(d, n, &z, &zn) == 0 && zn < n) { d = z; n = zn; }
    else if (z) { free(z); z = 0; flate = 0; }
    else flate = 0;
    u8 *c = 0;
    if (w->crypt && n) {
        c = malloc(n);
        if (c) { memcpy(c, d, n); u8 k[16]; int kl; pw_objkey(w, id, k, &kl); pdf_rc4(k, (size_t)kl, c, n); d = c; }
    }
    pbuf h = {0};
    pb_s(&h, "<<");
    pb_s(&h, dict);
    if (flate) pb_s(&h, " /Filter /FlateDecode");
    pb_s(&h, " /Length ");
    pb_int(&h, (long)n);
    pb_s(&h, ">>\nstream\n");
    pw_begin(w, id);
    pw_write(w, h.b, h.n);
    pw_write(w, d, n);
    pw_writes(w, "\nendstream\nendobj\n");
    free(h.b);
    free(z);
    free(c);
}
static void pw_addres(pdfw *w, pbuf *cat, const char *prefix, int id, char *name_out) {
    char nm[12];
    int k = 0;
    for (const char *s = prefix; *s; s++) nm[k++] = *s;
    long v = ++w->nres;
    char t[8];
    int tk = 0;
    do { t[tk++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (tk) nm[k++] = t[--tk];
    nm[k] = 0;
    pb_s(cat, "/"); pb_s(cat, nm); pb_sp(cat); pb_ref(cat, id); pb_s(cat, "\n");
    memcpy(name_out, nm, (size_t)k + 1);
}

/* ---------------------------------------------------------------- coordenadas */
/* dispositivo (pixel do bitmap, y para baixo) → página do PDF */
static void pw_dev2pdf(const pdfw *w, double dx, double dy, double *x, double *y) { *x = dx + w->mb[0]; *y = w->mb[1] + (w->mb[3] - w->mb[1]) - dy; }
/* usuário → página: a CTM do bitmap seguida da troca de eixo */
static CGAffineTransform pw_user2pdf(const cgobj_t *o, const pdfw *w) {
    double H = w->mb[3] - w->mb[1];
    CGAffineTransform t = {o->ca, -o->cb, o->cc, -o->cd, o->cu + w->mb[0], w->mb[1] + H - o->cv};
    return t;
}
static void pb_matrix(pbuf *p, CGAffineTransform t) { double v[6] = {t.a, t.b, t.c, t.d, t.tx, t.ty}; pb_nums(p, v, 6); }

/* ---------------------------------------------------------------- ExtGState (alfa, blend, máscara) */
static const char *pw_bm_name(int bm) {
    static const char *const n[16] = {"Normal", "Multiply", "Screen", "Overlay", "Darken", "Lighten", "ColorDodge", "ColorBurn",
                                      "SoftLight", "HardLight", "Difference", "Exclusion", "Hue", "Saturation", "Color", "Luminosity"};
    return bm >= 0 && bm < 16 ? n[bm] : "Normal";
}
static void pw_gs(pdfw *w, pbuf *out, double alpha, int bm) {
    int ca = (int)(alpha * 1000 + 0.5);
    if (ca > 1000) ca = 1000;
    if (bm >= 16) bm = 0;                        /* Porter-Duff (copy, source in...) não existe no PDF */
    pw_gsc *g = 0;
    for (int i = 0; i < w->ngss; i++) if (w->gss[i].ca == ca && w->gss[i].bm == bm) g = &w->gss[i];
    if (!g) {
        pw_gsc *n = realloc(w->gss, (size_t)(w->ngss + 1) * sizeof *n);
        if (!n) return;
        w->gss = n;
        g = &w->gss[w->ngss++];
        g->ca = ca; g->bm = bm; g->id = pw_newid(w);
        pbuf b = {0};
        pb_s(&b, "<< /Type /ExtGState /ca "); pb_num(&b, ca / 1000.0);
        pb_s(&b, " /CA "); pb_num(&b, ca / 1000.0);
        pb_s(&b, " /BM /"); pb_s(&b, pw_bm_name(bm));
        pb_s(&b, " >>");
        pw_obj(w, g->id, &b);
        free(b.b);
        pw_addres(w, &w->rg, "G", g->id, g->name);
    }
    pb_s(out, "/"); pb_s(out, g->name); pb_s(out, " gs\n");
}
static void pw_color(pbuf *out, u32 straight_rgb) {
    double v[3] = {((straight_rgb >> 16) & 255) / 255.0, ((straight_rgb >> 8) & 255) / 255.0, (straight_rgb & 255) / 255.0};
    pb_nums(out, v, 3);
    pb_s(out, " rg\n");
}
/* ARGB pré-multiplicado → RGB reto e alfa */
static u32 pw_unpm(u32 p, double *alpha) {
    u32 a = p >> 24;
    *alpha = a / 255.0;
    if (!a) return 0;
    u32 r = ((p >> 16) & 255) * 255 / a, g = ((p >> 8) & 255) * 255 / a, b = (p & 255) * 255 / a;
    return (r > 255 ? 255 : r) << 16 | (g > 255 ? 255 : g) << 8 | (b > 255 ? 255 : b);
}

/* ---------------------------------------------------------------- imagens */
/* XObject de imagem a partir de pixels ARGB (pm = pré-multiplicados); alfa vira SMask. color != 0: máscara
 * (o alfa da imagem pinta a cor dada) */
static int pw_image_obj(pdfw *w, const u32 *px, long stride_px, int iw, int ih, int pm, int interp, int mask, u32 color) {
    size_t n = (size_t)iw * ih;
    u8 *rgb = malloc(mask ? 3 : n * 3), *al = malloc(n);
    if (!rgb || !al) { free(rgb); free(al); return 0; }
    int anyalpha = 0;
    for (int y = 0; y < ih; y++)
        for (int x = 0; x < iw; x++) {
            u32 p = px[(size_t)y * stride_px + x], a = p >> 24;
            size_t i = (size_t)y * iw + x;
            al[i] = (u8)a;
            if (a < 255) anyalpha = 1;
            if (mask) continue;
            u32 r = (p >> 16) & 255, g = (p >> 8) & 255, b = p & 255;
            if (pm && a && a < 255) { r = r * 255 / a; g = g * 255 / a; b = b * 255 / a; if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255; }
            rgb[i * 3] = (u8)r; rgb[i * 3 + 1] = (u8)g; rgb[i * 3 + 2] = (u8)b;
        }
    if (mask) { rgb[0] = (u8)(color >> 16); rgb[1] = (u8)(color >> 8); rgb[2] = (u8)color; anyalpha = 1; }
    int sid = 0;
    if (anyalpha) {
        sid = pw_newid(w);
        pbuf d = {0};
        pb_s(&d, " /Type /XObject /Subtype /Image /Width "); pb_int(&d, iw); pb_s(&d, " /Height "); pb_int(&d, ih);
        pb_s(&d, " /ColorSpace /DeviceGray /BitsPerComponent 8");
        if (interp) pb_s(&d, " /Interpolate true");
        pb_put(&d, "", 1);
        pw_stream(w, sid, (const char *)d.b, al, n, 1);
        free(d.b);
    }
    int id = pw_newid(w);
    pbuf d = {0};
    pb_s(&d, " /Type /XObject /Subtype /Image /Width "); pb_int(&d, mask ? 1 : iw); pb_s(&d, " /Height "); pb_int(&d, mask ? 1 : ih);
    pb_s(&d, " /ColorSpace /DeviceRGB /BitsPerComponent 8");
    if (interp && !mask) pb_s(&d, " /Interpolate true");
    if (sid) { pb_s(&d, " /SMask "); pb_ref(&d, sid); }
    pb_put(&d, "", 1);
    pw_stream(w, id, (const char *)d.b, rgb, mask ? 3 : n * 3, 1);
    free(d.b);
    free(rgb);
    free(al);
    return id;
}
static const char *pw_xobj_name(pdfw *w, int id) {
    static char nm[12];
    pw_addres(w, &w->rx, "X", id, nm);
    return nm;
}
static void pw_draw_xobj(pdfw *w, pbuf *out, int id, CGAffineTransform m, double alpha, int bm, char *cached_name) {
    char nm[12];
    if (cached_name && cached_name[0]) memcpy(nm, cached_name, 12);
    else { memcpy(nm, pw_xobj_name(w, id), 12); if (cached_name) memcpy(cached_name, nm, 12); }
    pb_s(out, "q\n");
    pw_gs(w, out, alpha, bm);
    pb_matrix(out, m);
    pb_s(out, " cm /"); pb_s(out, nm); pb_s(out, " Do\nQ\n");
}

/* ---------------------------------------------------------------- caminho de raster */
static void pw_note(cgobj_t *o, int x, int y) {
    pdfw *w = o->pdf;
    if (!w->rdirty) { w->rx0 = w->rx1 = x; w->ry0 = w->ry1 = y; w->rdirty = 1; return; }
    if (x < w->rx0) w->rx0 = x;
    if (x > w->rx1) w->rx1 = x;
    if (y < w->ry0) w->ry0 = y;
    if (y > w->ry1) w->ry1 = y;
}
/* o que foi pintado no bitmap sai como imagem (com alfa) na posição dele, e o bitmap volta a transparente */
static void pw_flush(cgobj_t *o) {
    pdfw *w = o->pdf;
    if (!w || !w->rdirty) return;
    w->rdirty = 0;
    int x0 = w->rx0, y0 = w->ry0, iw = w->rx1 - w->rx0 + 1, ih = w->ry1 - w->ry0 + 1;
    const u32 *px = (const u32 *)(o->data + (size_t)y0 * o->bpr) + x0;
    int id = pw_image_obj(w, px, (long)(o->bpr / 4), iw, ih, 1, 0, 0, 0);
    for (int y = y0; y < y0 + ih; y++) memset(o->data + (size_t)y * o->bpr + (size_t)x0 * 4, 0, (size_t)iw * 4);
    if (!id) return;
    double X, Y;
    pw_dev2pdf(w, x0, y0 + ih, &X, &Y);
    CGAffineTransform m = {iw, 0, 0, ih, X, Y};
    pw_draw_xobj(w, pw_out(w), id, m, 1, o->blend, 0);
}

/* ---------------------------------------------------------------- paths */
static void pw_poly(pdfw *w, pbuf *out, const cgr_poly *p) {
    for (int k = 0; k < p->nsub; k++) {
        int a = p->sub[k], b = k + 1 < p->nsub ? p->sub[k + 1] : p->n;
        for (int i = a; i < b; i++) {
            double x, y;
            pw_dev2pdf(w, p->pts[i].x, p->pts[i].y, &x, &y);
            pb_num(out, x); pb_sp(out); pb_num(out, y);
            pb_s(out, i == a ? " m\n" : " l\n");
        }
        if (p->closed[k]) pb_s(out, "h\n");
    }
}
static int pw_shadow_on(cgobj_t *o) { return o->gx && (o->gx->scolor >> 24); }
/* preenchimento vetorial de um polígono do dispositivo; 0 = vai pelo raster (sombra, padrão) */
static int pw_fill(cgobj_t *o, const cgr_poly *poly, int eo, u32 solid, int use_solid) {
    pdfw *w = o->pdf;
    if (!w->in_page) return 1;
    cg_bmx_t *g = o->gx;
    if (pw_shadow_on(o)) return 0;
    if (!use_solid && g && g->pattern_on && g->tile) return 0;
    if (o->blend == 16) return 1;                /* clear: nada a pôr no papel */
    u32 p = use_solid ? solid : premul(o->fill, g ? g->galpha : 1);
    double a;
    u32 rgb = pw_unpm(p, &a);
    if (a <= 0) return 1;
    pw_flush(o);
    pbuf *out = pw_out(w);
    pw_gs(w, out, a, o->blend);
    pw_color(out, rgb);
    pw_poly(w, out, poly);
    pb_s(out, eo ? "f*\n" : "f\n");
    return 1;
}
static void pw_clip(cgobj_t *o, const cgr_poly *poly, int eo) {
    pdfw *w = o->pdf;
    if (!w->in_page) return;
    pw_flush(o);
    pbuf *out = pw_out(w);
    if (!poly->n) { pb_s(out, "0 0 0 0 re W n\n"); return; }
    pw_poly(w, out, poly);
    pb_s(out, eo ? "W* n\n" : "W n\n");
}
static void pw_clip_box(cgobj_t *o, int x0, int y0, int x1, int y1) {
    pdfw *w = o->pdf;
    if (!w->in_page) return;
    pw_flush(o);
    pbuf *out = pw_out(w);
    double X, Y;
    pw_dev2pdf(w, x0, y1, &X, &Y);
    double v[4] = {X, Y, (double)(x1 > x0 ? x1 - x0 : 0), (double)(y1 > y0 ? y1 - y0 : 0)};
    pb_nums(out, v, 4);
    pb_s(out, " re W n\n");
}
/* recorte por máscara: SMask de luminosidade com a máscara (cinza) desenhada no retângulo */
static void pw_clip_mask(cgobj_t *o, CGRect rect, const cgobj_t *m) {
    pdfw *w = o->pdf;
    if (!w->in_page) return;
    pw_flush(o);
    size_t n = (size_t)m->w * m->h;
    u8 *gray = malloc(n ? n : 1);
    if (!gray) return;
    for (u32 y = 0; y < m->h; y++)
        for (u32 x = 0; x < m->w; x++) {
            u32 p = img_pm(m, (int)x, (int)y), k;
            if (m->ismask || (m->info & 31) == AI_ONLY) k = p >> 24;
            else { u32 r, gg, b, a = un_pm(p, &r, &gg, &b); k = ((r * 77 + gg * 150 + b * 29 + 128) >> 8) * a / 255; }
            gray[(size_t)y * m->w + x] = (u8)k;
        }
    int mid = pw_newid(w);
    pbuf d = {0};
    pb_s(&d, " /Type /XObject /Subtype /Image /Width "); pb_int(&d, (long)m->w); pb_s(&d, " /Height "); pb_int(&d, (long)m->h);
    pb_s(&d, " /ColorSpace /DeviceGray /BitsPerComponent 8 /Interpolate true");
    pb_put(&d, "", 1);
    pw_stream(w, mid, (const char *)d.b, gray, n, 1);
    free(d.b);
    free(gray);
    /* form do grupo: a imagem no retângulo (espaço da página) */
    CGAffineTransform u = pw_user2pdf(o, w);
    CGAffineTransform r = {rect.size.width * u.a, rect.size.width * u.b, rect.size.height * u.c, rect.size.height * u.d,
                           rect.origin.x * u.a + rect.origin.y * u.c + u.tx, rect.origin.x * u.b + rect.origin.y * u.d + u.ty};
    pbuf c = {0};
    pb_s(&c, "q "); pb_matrix(&c, r); pb_s(&c, " cm /M Do Q\n");
    int fid = pw_newid(w);
    pbuf fd = {0};
    pb_s(&fd, " /Type /XObject /Subtype /Form /BBox ["); pb_nums(&fd, w->mb, 4);
    pb_s(&fd, "] /Group << /S /Transparency /CS /DeviceGray >> /Resources << /XObject << /M "); pb_ref(&fd, mid); pb_s(&fd, " >> >>");
    pb_put(&fd, "", 1);
    pw_stream(w, fid, (const char *)fd.b, c.b, c.n, 1);
    free(fd.b); free(c.b);
    int gid = pw_newid(w);
    pbuf gs = {0};
    pb_s(&gs, "<< /Type /ExtGState /SMask << /Type /Mask /S /Luminosity /G "); pb_ref(&gs, fid); pb_s(&gs, " >> >>");
    pw_obj(w, gid, &gs);
    free(gs.b);
    char nm[12];
    pw_addres(w, &w->rg, "G", gid, nm);
    pbuf *out = pw_out(w);
    pb_s(out, "/"); pb_s(out, nm); pb_s(out, " gs\n");
}
/* volta ao recorte da página: fecha todos os q abertos e reabre o mesmo número */
static void pw_reset_clip(cgobj_t *o) {
    pdfw *w = o->pdf;
    if (!w->in_page) return;
    pw_flush(o);
    pbuf *out = pw_out(w);
    for (int i = 0; i <= w->qdepth; i++) pb_s(out, "Q\n");
    for (int i = 0; i <= w->qdepth; i++) pb_s(out, "q\n");
}
static void pw_save(cgobj_t *o) {
    pdfw *w = o->pdf;
    if (!w->in_page) return;
    pw_flush(o);
    pb_s(pw_out(w), "q\n");
    w->qdepth++;
}
static void pw_restore(cgobj_t *o) {
    pdfw *w = o->pdf;
    if (!w->in_page || w->qdepth <= 0) return;
    pw_flush(o);
    pb_s(pw_out(w), "Q\n");
    w->qdepth--;
}

/* ---------------------------------------------------------------- imagem, gradiente e shading */
static int pw_draw_image(cgobj_t *o, CGRect rect, cgobj_t *im) {
    pdfw *w = o->pdf;
    if (!w->in_page) return 1;
    if (pw_shadow_on(o)) return 0;
    cg_bmx_t *g = o->gx;
    if (im->ismask && g && g->pattern_on && g->tile) return 0;
    if (o->blend == 16) return 1;
    pw_flush(o);
    double ga = g ? g->galpha : 1, fa = 1;
    u32 color = 0;
    if (im->ismask) { color = pw_unpm(premul(o->fill, 1), &fa); }
    int interp = im->shint && !(g && g->interp == 1);
    pw_imgc *c = 0;
    for (int i = 0; i < w->nimgs; i++) if (w->imgs[i].img == im && (!im->ismask || w->imgs[i].color == color)) c = &w->imgs[i];
    if (!c) {
        int iid = pw_image_obj(w, (const u32 *)im->data, (long)(im->bpr / 4), (int)im->w, (int)im->h, im->cb_size ? 1 : 0, interp, im->ismask, color);
        if (!iid) return 1;
        pw_imgc *n = realloc(w->imgs, (size_t)(w->nimgs + 1) * sizeof *n);
        if (!n) return 1;
        w->imgs = n;
        c = &w->imgs[w->nimgs++];
        c->img = objc_retain((id)im); c->color = color; c->id = iid;
    }
    CGAffineTransform u = pw_user2pdf(o, w);
    CGAffineTransform m = {rect.size.width * u.a, rect.size.width * u.b, rect.size.height * u.c, rect.size.height * u.d,
                           rect.origin.x * u.a + rect.origin.y * u.c + u.tx, rect.origin.x * u.b + rect.origin.y * u.d + u.ty};
    char nm[12] = {0};
    pw_draw_xobj(w, pw_out(w), c->id, m, ga * fa, o->blend, nm);
    return 1;
}
/* função amostrada (tipo 0, 256 amostras RGB) a partir de 256 cores ARGB pré-multiplicadas */
static int pw_sampled_fn(pdfw *w, const u32 *lut, double d0, double d1) {
    u8 s[256 * 3];
    for (int i = 0; i < 256; i++) { double a; u32 c = pw_unpm(lut[i], &a); s[i * 3] = (u8)(c >> 16); s[i * 3 + 1] = (u8)(c >> 8); s[i * 3 + 2] = (u8)c; }
    int id = pw_newid(w);
    pbuf d = {0};
    pb_s(&d, " /FunctionType 0 /Domain ["); pb_num(&d, d0); pb_sp(&d); pb_num(&d, d1);
    pb_s(&d, "] /Range [0 1 0 1 0 1] /Size [256] /BitsPerSample 8");
    pb_put(&d, "", 1);
    pw_stream(w, id, (const char *)d.b, s, sizeof s, 1);
    free(d.b);
    return id;
}
/* shading axial/radial no espaço do usuário; 0 = vai pelo raster (sombra ou cores com alfa) */
static int pw_shading(cgobj_t *o, const u32 *lut, double d0, double d1, int radial, CGPoint p0, double r0, CGPoint p1, double r1, int e0, int e1) {
    pdfw *w = o->pdf;
    if (!w->in_page) return 1;
    if (pw_shadow_on(o)) return 0;
    for (int i = 0; i < 256; i++) if ((lut[i] >> 24) < 255) return 0;
    if (o->blend == 16) return 1;
    pw_flush(o);
    int fn = pw_sampled_fn(w, lut, d0, d1);
    int id = pw_newid(w);
    pbuf b = {0};
    pb_s(&b, "<< /ShadingType "); pb_int(&b, radial ? 3 : 2);
    pb_s(&b, " /ColorSpace /DeviceRGB /Coords [");
    if (radial) { double v[6] = {p0.x, p0.y, r0, p1.x, p1.y, r1}; pb_nums(&b, v, 6); }
    else { double v[4] = {p0.x, p0.y, p1.x, p1.y}; pb_nums(&b, v, 4); }
    pb_s(&b, "] /Domain ["); pb_num(&b, d0); pb_sp(&b); pb_num(&b, d1);
    pb_s(&b, "] /Function "); pb_ref(&b, fn);
    pb_s(&b, " /Extend ["); pb_s(&b, e0 ? "true " : "false "); pb_s(&b, e1 ? "true" : "false"); pb_s(&b, "] >>");
    pw_obj(w, id, &b);
    free(b.b);
    char nm[12];
    pw_addres(w, &w->rsh, "S", id, nm);
    pbuf *out = pw_out(w);
    pb_s(out, "q\n");
    pw_gs(w, out, o->gx ? o->gx->galpha : 1, o->blend);
    pb_matrix(out, pw_user2pdf(o, w));
    pb_s(out, " cm /"); pb_s(out, nm); pb_s(out, " sh\nQ\n");
    return 1;
}

/* ---------------------------------------------------------------- texto (Type3 com os contornos da face) */
typedef struct { pbuf *b; double cx, cy; } pw_pathctx;
static void pw_path_el(void *info, const CGPathElement *e) {
    pw_pathctx *c = info;
    pbuf *b = c->b;
    switch (e->type) {
    case 0: pb_num(b, e->points[0].x); pb_sp(b); pb_num(b, e->points[0].y); pb_s(b, " m\n"); c->cx = e->points[0].x; c->cy = e->points[0].y; break;
    case 1: pb_num(b, e->points[0].x); pb_sp(b); pb_num(b, e->points[0].y); pb_s(b, " l\n"); c->cx = e->points[0].x; c->cy = e->points[0].y; break;
    case 2: {
        double q[6] = {c->cx + 2.0 / 3 * (e->points[0].x - c->cx), c->cy + 2.0 / 3 * (e->points[0].y - c->cy),
                       e->points[1].x + 2.0 / 3 * (e->points[0].x - e->points[1].x), e->points[1].y + 2.0 / 3 * (e->points[0].y - e->points[1].y), e->points[1].x, e->points[1].y};
        pb_nums(b, q, 6); pb_s(b, " c\n"); c->cx = e->points[1].x; c->cy = e->points[1].y;
        break;
    }
    case 3: { double q[6] = {e->points[0].x, e->points[0].y, e->points[1].x, e->points[1].y, e->points[2].x, e->points[2].y}; pb_nums(b, q, 6); pb_s(b, " c\n"); c->cx = e->points[2].x; c->cy = e->points[2].y; break; }
    case 4: pb_s(b, "h\n"); break;
    }
}
/* código do glifo na fonte Type3 da face (cria fonte/procedimento na primeira vez); −1 = sem contorno */
static int pw_glyph_code(pdfw *w, int face, int gid, pw_t3 **font) {
    pw_face *f = 0;
    for (int i = 0; i < w->nfaces; i++) if (w->faces[i].face == face) f = &w->faces[i];
    if (!f) {
        pw_face *n = realloc(w->faces, (size_t)(w->nfaces + 1) * sizeof *n);
        if (!n) return -1;
        w->faces = n;
        f = &w->faces[w->nfaces++];
        memset(f, 0, sizeof *f);
        f->face = face;
        f->map = calloc(65536, 4);
        if (!f->map) return -1;
    }
    if (gid < 0 || gid > 65535) return -1;
    u32 m = f->map[gid];
    if (m) { *font = &f->f[(m >> 8) - 1]; return (int)(m & 255); }
    void *path = cx_font_glyph_path(face, gid, 1000, 0);
    if (!path) return -1;
    if (!f->nf || f->f[f->nf - 1].n >= 256) {
        pw_t3 *n = realloc(f->f, (size_t)(f->nf + 1) * sizeof *n);
        if (!n) { CGPathRelease(path); return -1; }
        f->f = n;
        memset(&f->f[f->nf], 0, sizeof f->f[0]);
        f->f[f->nf].obj = pw_newid(w);
        pw_addres(w, &w->rf, "T", f->f[f->nf].obj, f->f[f->nf].name);
        f->nf++;
    }
    pw_t3 *t = &f->f[f->nf - 1];
    int code = t->n++;
    t->gid[code] = (u16)gid;
    t->w[code] = cx_font_glyph_advance(face, gid);
    t->uni[code] = cx_font_glyph_unicode(face, gid);
    CGRect bb = CGPathGetPathBoundingBox(path);
    double b4[4] = {bb.origin.x, bb.origin.y, bb.origin.x + bb.size.width, bb.origin.y + bb.size.height};
    if (bb.size.width <= 0 && bb.size.height <= 0) b4[0] = b4[1] = b4[2] = b4[3] = 0;
    if (!code) memcpy(t->bb, b4, sizeof b4);
    else if (b4[2] > b4[0]) {
        if (b4[0] < t->bb[0]) t->bb[0] = b4[0];
        if (b4[1] < t->bb[1]) t->bb[1] = b4[1];
        if (b4[2] > t->bb[2]) t->bb[2] = b4[2];
        if (b4[3] > t->bb[3]) t->bb[3] = b4[3];
    }
    pbuf proc = {0};
    double d1[6] = {t->w[code], 0, b4[0], b4[1], b4[2], b4[3]};
    pb_nums(&proc, d1, 6);
    pb_s(&proc, " d1\n");
    pw_pathctx pc = {&proc, 0, 0};
    CGPathApply(path, &pc, pw_path_el);
    pb_s(&proc, "f\n");
    CGPathRelease(path);
    t->proc[code] = pw_newid(w);
    pw_stream(w, t->proc[code], "", proc.b, proc.n, 1);
    free(proc.b);
    f->map[gid] = (u32)f->nf << 8 | (u32)code;
    *font = t;
    return code;
}
/* CTFontDrawGlyphs num contexto PDF: posições no espaço de texto (matriz de texto, depois a CTM) */
int cx_pdfw_glyphs(void *ctx, int face, double size, const u16 *glyphs, const CGPoint *pos, size_t n) {
    cgobj_t *o = as_kind(ctx, CGK_BM);
    if (!o || !o->pdf) return 0;
    pdfw *w = o->pdf;
    if (!w->in_page) return 1;
    cg_bmx_t *g = o->gx;
    if (pw_shadow_on(o) || (g && g->pattern_on && g->tile) || !cx_font_outline_kind(face)) return 0;
    if (o->blend == 16) return 1;
    double a;
    u32 fill = o->fill ? o->fill : 0xFF000000u;
    u32 rgb = pw_unpm(premul(fill, g ? g->galpha : 1), &a);
    if (a <= 0) return 1;
    pw_flush(o);
    pbuf *out = pw_out(w);
    pw_gs(w, out, a, o->blend);
    pw_color(out, rgb);
    pb_s(out, "BT\n");
    CGAffineTransform u = pw_user2pdf(o, w);
    pw_t3 *cur = 0;
    for (size_t i = 0; i < n; i++) {
        pw_t3 *t = 0;
        int code = pw_glyph_code(w, face, glyphs[i], &t);
        if (code < 0) continue;
        if (t != cur) { pb_s(out, "/"); pb_s(out, t->name); pb_s(out, " 1 Tf\n"); cur = t; }
        /* glifo (1 em = size) na posição, matriz de texto, CTM e página */
        double px = o->ta * pos[i].x + o->tc * pos[i].y + o->tx, py = o->tb * pos[i].x + o->td * pos[i].y + o->ty;
        CGAffineTransform m1 = {size * o->ta, size * o->tb, size * o->tc, size * o->td, px, py};
        CGAffineTransform m = {m1.a * u.a + m1.b * u.c, m1.a * u.b + m1.b * u.d, m1.c * u.a + m1.d * u.c, m1.c * u.b + m1.d * u.d,
                               m1.tx * u.a + m1.ty * u.c + u.tx, m1.tx * u.b + m1.ty * u.d + u.ty};
        pb_matrix(out, m);
        static const char hx[] = "0123456789ABCDEF";
        char cs[8] = {' ', 'T', 'm', ' ', '<', hx[code >> 4], hx[code & 15], '>'};
        pb_put(out, cs, 8);
        pb_s(out, " Tj\n");
    }
    pb_s(out, "ET\n");
    return 1;
}
int cx_pdfw_is(const void *ctx) { cgobj_t *o = as_kind(ctx, CGK_BM); return o && o->pdf; }

/* ---------------------------------------------------------------- camadas */
static int pw_layer_begin(cgobj_t *o) {
    pdfw *w = o->pdf;
    if (!w->in_page || w->nlayer >= 8) return 1;
    pw_flush(o);
    pbuf *b = calloc(1, sizeof *b);
    if (!b) return 1;
    w->lalpha[w->nlayer] = o->gx ? o->gx->galpha : 1;
    w->lblend[w->nlayer] = o->blend;
    w->lstack[w->nlayer++] = b;
    cx_bm_save(o);
    if (o->gx) { o->gx->galpha = 1; o->gx->scolor = 0; }
    o->blend = 0;
    return 1;
}
static void pw_layer_end(cgobj_t *o) {
    pdfw *w = o->pdf;
    if (!w->in_page || !w->nlayer) return;
    pw_flush(o);
    cx_bm_restore(o);
    pbuf *b = w->lstack[--w->nlayer];
    int id = pw_newid(w);
    pbuf d = {0};
    pb_s(&d, " /Type /XObject /Subtype /Form /BBox ["); pb_nums(&d, w->mb, 4);
    pb_s(&d, "] /Group << /S /Transparency /CS /DeviceRGB >> /Resources "); pb_ref(&d, w->res_id);
    pb_put(&d, "", 1);
    pw_stream(w, id, (const char *)d.b, b->b, b->n, 1);
    free(d.b);
    free(b->b);
    free(b);
    CGAffineTransform idm = {1, 0, 0, 1, 0, 0};
    pw_draw_xobj(w, pw_out(w), id, idm, w->lalpha[w->nlayer], w->lblend[w->nlayer], 0);
}

/* ---------------------------------------------------------------- tags (marked content e estrutura) */
static const char *pw_tag_name(int t) {
    static const struct { int v; const char *n; } tab[] = {
        {100, "Document"}, {101, "Part"}, {102, "Art"}, {103, "Sect"}, {104, "Div"}, {105, "BlockQuote"}, {106, "Caption"},
        {107, "TOC"}, {108, "TOCI"}, {109, "Index"}, {110, "NonStruct"}, {111, "Private"},
        {200, "P"}, {201, "H"}, {202, "H1"}, {203, "H2"}, {204, "H3"}, {205, "H4"}, {206, "H5"}, {207, "H6"},
        {300, "L"}, {301, "LI"}, {302, "Lbl"}, {303, "LBody"},
        {400, "Table"}, {401, "TR"}, {402, "TH"}, {403, "TD"}, {404, "THead"}, {405, "TBody"}, {406, "TFoot"},
        {500, "Span"}, {501, "Quote"}, {502, "Note"}, {503, "Reference"}, {504, "BibEntry"}, {505, "Code"}, {506, "Link"}, {507, "Annot"},
        {600, "Ruby"}, {601, "RB"}, {602, "RT"}, {603, "RP"}, {604, "Warichu"}, {605, "WT"}, {606, "WP"},
        {700, "Figure"}, {701, "Formula"}, {702, "Form"}};
    for (size_t i = 0; i < sizeof tab / sizeof tab[0]; i++) if (tab[i].v == t) return tab[i].n;
    return "Span";
}
static char *pw_cfstr_dup(id s) {
    if (!s) return 0;
    const char *c = cx_cstr(s);
    if (!c) return 0;
    size_t n = strlen(c);
    char *d = malloc(n + 1);
    if (d) memcpy(d, c, n + 1);
    return d;
}
static void pw_el_addkid(pw_elem_t *e, int kid, int page) {
    int *k = realloc(e->kid, (size_t)(e->nkid + 1) * sizeof *k), *p = realloc(e->kpage, (size_t)(e->nkid + 1) * sizeof *p);
    if (k) e->kid = k;
    if (p) e->kpage = p;
    if (!k || !p) return;
    e->kid[e->nkid] = kid;
    e->kpage[e->nkid++] = page;
}
/* abre um trecho de marked content para o elemento e (MCID novo na página) */
static void pw_mc_open(pdfw *w, int e) {
    pbuf *out = pw_out(w);
    int mc = w->mcid++;
    pb_s(out, "/"); pb_s(out, w->el[e].type); pb_s(out, " <</MCID "); pb_int(out, mc); pb_s(out, ">> BDC\n");
    pw_el_addkid(&w->el[e], mc + 1, w->npages);
    int *n = realloc(w->pmc, (size_t)(mc + 1) * sizeof *n);
    if (n) { w->pmc = n; w->pmc[mc] = e; w->npmc = mc + 1; }
    w->cur_mc = e;
}
static void pw_mc_close(pdfw *w) { if (w->cur_mc >= 0) { pb_s(pw_out(w), "EMC\n"); w->cur_mc = -1; } }

/* ---------------------------------------------------------------- páginas */
static double pw_cfnum(id n, double dflt) { double v; return n && CFNumberGetValue(n, 13, &v) ? v : dflt; }
static int pw_box_of(id dict, const void *key, double *b) {
    id d = dict ? CFDictionaryGetValue(dict, key) : 0;
    if (!d || CFDataGetLength(d) < (long)sizeof(CGRect)) return 0;
    CGRect r;
    memcpy(&r, CFDataGetBytePtr(d), sizeof r);
    b[0] = r.origin.x; b[1] = r.origin.y; b[2] = r.origin.x + r.size.width; b[3] = r.origin.y + r.size.height;
    return r.size.width > 0 && r.size.height > 0;
}
/* estado gráfico do começo de página (como no macOS, cada página começa do zero) */
static void pw_reset_state(cgobj_t *o) {
    pdfw *w = o->pdf;
    while (o->gsp > 0) cx_bm_restore(o);
    double W = w->mb[2] - w->mb[0], H = w->mb[3] - w->mb[1];
    u32 iw = (u32)(W + 0.5), ih = (u32)(H + 0.5);
    if (iw < 1) iw = 1;
    if (ih < 1) ih = 1;
    if (iw != o->w || ih != o->h) {
        u8 *d = calloc((size_t)iw * ih, 4);
        if (d) {
            if (o->owns) free(o->data);
            o->data = d; o->owns = 1; o->w = iw; o->h = ih; o->bpr = iw * 4;
        }
    } else memset(o->data, 0, (size_t)o->bpr * o->h);
    /* usuário = espaço da página do PDF: a origem da MediaBox fica na origem */
    o->ca = 1; o->cb = 0; o->cc = 0; o->cd = -1; o->cu = -w->mb[0]; o->cv = w->mb[1] + (double)o->h;
    o->ta = 1; o->tb = 0; o->tc = 0; o->td = 1; o->tx = 0; o->ty = 0;
    o->cl = 0; o->ct = 0; o->cr = (i32)o->w; o->cbt = (i32)o->h;
    o->fill = 0xFF000000u;
    o->blend = 0;
    cg_bmx_t *g = bmx(o);
    if (g) {
        cgr_clear(&g->path);
        free(g->mask); g->mask = 0;
        g->stroke = 0xFF000000u; g->lw = 1; g->miter = 10; g->phase = 0; g->ndash = 0; g->cap = 0; g->join = 0;
        g->galpha = 1; g->aa = 1; g->sdx = g->sdy = g->sblur = 0; g->scolor = 0;
        g->pattern_on = 0; g->spattern_on = 0; g->interp = 0; g->charsp = 0; g->tmode = 0;
    }
    w->rdirty = 0;
}
void CGPDFContextBeginPage(void *c, id pageInfo) {
    cgobj_t *o = as_kind(c, CGK_BM);
    pdfw *w = o ? o->pdf : 0;
    if (!w || w->closed) return;
    if (w->in_page) return;
    memcpy(w->mb, w->defmb, sizeof w->mb);
    double b[4];
    if (pw_box_of(pageInfo, kCGPDFContextMediaBox, b)) memcpy(w->mb, b, sizeof b);
    const void *keys[4] = {kCGPDFContextCropBox, kCGPDFContextBleedBox, kCGPDFContextTrimBox, kCGPDFContextArtBox};
    for (int i = 0; i < 4; i++) w->hasbox[i] = pw_box_of(pageInfo, keys[i], w->boxes[i]);
    pw_reset_state(o);
    if (w->npages + 1 > w->cappages) {
        int cp = w->cappages ? w->cappages * 2 : 16;
        int *n = realloc(w->page_ids, (size_t)cp * sizeof *n);
        if (!n) return;
        w->page_ids = n; w->cappages = cp;
    }
    w->page_ids[w->npages] = pw_newid(w);
    w->page.n = 0;
    w->qdepth = 0;
    w->in_page = 1;
    w->mcid = 0; w->npmc = 0; w->cur_mc = -1;
    pb_s(&w->page, "q\n");
    /* tags que atravessam a página continuam nela */
    if (w->ntag) pw_mc_open(w, w->tagstk[w->ntag - 1]);
}
void CGContextBeginPage(void *c, const CGRect *mediaBox) {
    cgobj_t *o = as_kind(c, CGK_BM);
    pdfw *w = o ? o->pdf : 0;
    if (!w) return;
    double save[4];
    memcpy(save, w->defmb, sizeof save);
    if (mediaBox && mediaBox->size.width > 0 && mediaBox->size.height > 0) {
        w->defmb[0] = mediaBox->origin.x; w->defmb[1] = mediaBox->origin.y;
        w->defmb[2] = mediaBox->origin.x + mediaBox->size.width; w->defmb[3] = mediaBox->origin.y + mediaBox->size.height;
    }
    CGPDFContextBeginPage(c, 0);
    memcpy(w->defmb, save, sizeof save);
}
static void pw_rect_of_user(cgobj_t *o, pdfw *w, CGRect r, double *out) {
    CGAffineTransform u = pw_user2pdf(o, w);
    double xs[2] = {r.origin.x, r.origin.x + r.size.width}, ys[2] = {r.origin.y, r.origin.y + r.size.height};
    out[0] = out[1] = 1e30; out[2] = out[3] = -1e30;
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 2; j++) {
            double X = u.a * xs[i] + u.c * ys[j] + u.tx, Y = u.b * xs[i] + u.d * ys[j] + u.ty;
            if (X < out[0]) out[0] = X;
            if (Y < out[1]) out[1] = Y;
            if (X > out[2]) out[2] = X;
            if (Y > out[3]) out[3] = Y;
        }
}
void CGPDFContextEndPage(void *c) {
    cgobj_t *o = as_kind(c, CGK_BM);
    pdfw *w = o ? o->pdf : 0;
    if (!w || !w->in_page) return;
    pw_flush(o);
    while (w->nlayer) pw_layer_end(o);
    pw_mc_close(w);
    while (w->qdepth > 0) { pb_s(&w->page, "Q\n"); w->qdepth--; }
    pb_s(&w->page, "Q\n");
    int cid = pw_newid(w);
    pw_stream(w, cid, "", w->page.b, w->page.n, 1);
    /* anotações de link desta página */
    pbuf annots = {0};
    for (int i = 0; i < w->nlinks; i++) {
        pw_link *l = &w->links[i];
        if (l->page != w->npages) continue;
        int aid = pw_newid(w);
        pbuf a = {0};
        pb_s(&a, "<< /Type /Annot /Subtype /Link /Rect ["); pb_nums(&a, l->r, 4); pb_s(&a, "] /Border [0 0 0] /P "); pb_ref(&a, w->page_ids[w->npages]);
        if (l->uri) { pb_s(&a, " /A << /S /URI /URI "); pw_str(w, &a, aid, (const u8 *)l->uri, strlen(l->uri)); pb_s(&a, " >>"); }
        else if (l->dest) { pb_s(&a, " /Dest "); pw_str(w, &a, aid, (const u8 *)l->dest, strlen(l->dest)); }
        pb_s(&a, " >>");
        pw_obj(w, aid, &a);
        free(a.b);
        pb_ref(&annots, aid); pb_sp(&annots);
    }
    /* estrutura: array MCID → elemento da página */
    int sp = -1;
    if (w->npmc) {
        int *n = realloc(w->ptree, (size_t)(w->nptree + 1) * sizeof *n);
        if (n) {
            w->ptree = n;
            int pid = pw_newid(w);
            sp = w->nptree;
            pbuf a = {0};
            pb_s(&a, "[");
            for (int i = 0; i < w->npmc; i++) { if (w->el[w->pmc[i]].id) pb_ref(&a, w->el[w->pmc[i]].id); else pb_s(&a, "null"); pb_sp(&a); }
            pb_s(&a, "]");
            pw_obj(w, pid, &a);
            free(a.b);
            w->ptree[w->nptree++] = pid;
        }
    }
    pbuf p = {0};
    pb_s(&p, "<< /Type /Page /Parent "); pb_ref(&p, w->pages_id);
    pb_s(&p, " /MediaBox ["); pb_nums(&p, w->mb, 4); pb_s(&p, "]");
    static const char *const bn[4] = {" /CropBox [", " /BleedBox [", " /TrimBox [", " /ArtBox ["};
    for (int i = 0; i < 4; i++) if (w->hasbox[i]) { pb_s(&p, bn[i]); pb_nums(&p, w->boxes[i], 4); pb_s(&p, "]"); }
    pb_s(&p, " /Resources "); pb_ref(&p, w->res_id);
    pb_s(&p, " /Contents "); pb_ref(&p, cid);
    pb_s(&p, " /Group << /S /Transparency /CS /DeviceRGB >>");
    if (annots.n) { pb_s(&p, " /Annots ["); pb_put(&p, annots.b, annots.n); pb_s(&p, "]"); }
    if (sp >= 0) { pb_s(&p, " /StructParents "); pb_int(&p, sp); pb_s(&p, " /Tabs /S"); }
    pb_s(&p, " >>");
    pw_obj(w, w->page_ids[w->npages], &p);
    free(p.b);
    free(annots.b);
    w->npages++;
    w->in_page = 0;
}
void CGContextEndPage(void *c) { CGPDFContextEndPage(c); }

/* ---------------------------------------------------------------- links, destinos, outline, metadados, tags */
void CGPDFContextSetURLForRect(void *c, id url, CGRect rect) {
    cgobj_t *o = as_kind(c, CGK_BM);
    pdfw *w = o ? o->pdf : 0;
    if (!w || !w->in_page || !url) return;
    pw_link *n = realloc(w->links, (size_t)(w->nlinks + 1) * sizeof *n);
    if (!n) return;
    w->links = n;
    pw_link *l = &w->links[w->nlinks++];
    memset(l, 0, sizeof *l);
    l->page = w->npages;
    pw_rect_of_user(o, w, rect, l->r);
    l->uri = pw_cfstr_dup(CFURLGetString(url));
}
void CGPDFContextSetDestinationForRect(void *c, id name, CGRect rect) {
    cgobj_t *o = as_kind(c, CGK_BM);
    pdfw *w = o ? o->pdf : 0;
    if (!w || !w->in_page || !name) return;
    pw_link *n = realloc(w->links, (size_t)(w->nlinks + 1) * sizeof *n);
    if (!n) return;
    w->links = n;
    pw_link *l = &w->links[w->nlinks++];
    memset(l, 0, sizeof *l);
    l->page = w->npages;
    pw_rect_of_user(o, w, rect, l->r);
    l->dest = pw_cfstr_dup(name);
}
void CGPDFContextAddDestinationAtPoint(void *c, id name, CGPoint p) {
    cgobj_t *o = as_kind(c, CGK_BM);
    pdfw *w = o ? o->pdf : 0;
    if (!w || !w->in_page || !name) return;
    pw_dest *n = realloc(w->dests, (size_t)(w->ndests + 1) * sizeof *n);
    if (!n) return;
    w->dests = n;
    pw_dest *d = &w->dests[w->ndests++];
    d->name = pw_cfstr_dup(name);
    d->page = w->npages;
    CGAffineTransform u = pw_user2pdf(o, w);
    d->x = u.a * p.x + u.c * p.y + u.tx;
    d->y = u.b * p.x + u.d * p.y + u.ty;
}
void CGPDFContextSetOutline(void *c, id outline) {
    cgobj_t *o = as_kind(c, CGK_BM);
    pdfw *w = o ? o->pdf : 0;
    if (!w) return;
    if (outline) CFRetain(outline);
    if (w->outline) CFRelease(w->outline);
    w->outline = outline;
}
void CGPDFContextAddDocumentMetadata(void *c, id xmp) {
    cgobj_t *o = as_kind(c, CGK_BM);
    pdfw *w = o ? o->pdf : 0;
    if (!w || !xmp) return;
    CFRetain(xmp);
    if (w->xmp) CFRelease(w->xmp);
    w->xmp = xmp;
}
void CGPDFContextBeginTag(void *c, int tagType, id props) {
    cgobj_t *o = as_kind(c, CGK_BM);
    pdfw *w = o ? o->pdf : 0;
    if (!w || w->ntag >= 64) return;
    if (w->nel + 1 > w->capel) {
        int cp = w->capel ? w->capel * 2 : 32;
        pw_elem_t *n = realloc(w->el, (size_t)cp * sizeof *n);
        if (!n) return;
        w->el = n; w->capel = cp;
    }
    int e = w->nel++;
    pw_elem_t *el = &w->el[e];
    memset(el, 0, sizeof *el);
    el->type = pw_tag_name(tagType);
    el->parent = w->ntag ? w->tagstk[w->ntag - 1] : -1;
    el->page = w->npages;
    el->id = pw_newid(w);
    if (props) {
        el->actual = pw_cfstr_dup(CFDictionaryGetValue(props, kCGPDFTagPropertyActualText));
        el->alt = pw_cfstr_dup(CFDictionaryGetValue(props, kCGPDFTagPropertyAlternativeText));
        el->title = pw_cfstr_dup(CFDictionaryGetValue(props, kCGPDFTagPropertyTitleText));
        el->lang = pw_cfstr_dup(CFDictionaryGetValue(props, kCGPDFTagPropertyLanguageText));
    }
    if (el->parent >= 0) pw_el_addkid(&w->el[el->parent], -(e + 1), 0);
    w->tagstk[w->ntag++] = e;
    if (w->in_page) {
        pw_flush(o);
        pw_mc_close(w);
        pw_mc_open(w, e);
    }
}
void CGPDFContextEndTag(void *c) {
    cgobj_t *o = as_kind(c, CGK_BM);
    pdfw *w = o ? o->pdf : 0;
    if (!w || !w->ntag) return;
    if (w->in_page) { pw_flush(o); pw_mc_close(w); }
    w->ntag--;
    if (w->in_page && w->ntag) pw_mc_open(w, w->tagstk[w->ntag - 1]);
}
void CGPDFContextSetIDTree(void *c, id tree) { (void)c; (void)tree; }          /* árvore de IDs: a do PDF de estrutura já sai pelos elementos */
void CGPDFContextSetPageTagStructureTree(void *c, id tree) { (void)c; (void)tree; }

/* ---------------------------------------------------------------- fechamento */
static int pw_outline_items(pdfw *w, id arr, int parent, int *first, int *last, int *count);
static int pw_outline_item(pdfw *w, id d, int id_, int parent, int prev, int next, int *count) {
    pbuf b = {0};
    id title = CFDictionaryGetValue(d, kCGPDFOutlineTitle);
    pb_s(&b, "<< /Title ");
    pw_text(w, &b, id_, title ? cx_cstr(title) : "");
    pb_s(&b, " /Parent "); pb_ref(&b, parent);
    if (prev) { pb_s(&b, " /Prev "); pb_ref(&b, prev); }
    if (next) { pb_s(&b, " /Next "); pb_ref(&b, next); }
    id dest = CFDictionaryGetValue(d, kCGPDFOutlineDestination);
    if (dest) {
        double pg = pw_cfnum(dest, -1);
        if (pg >= 1 && pg <= w->npages) {
            pb_s(&b, " /Dest ["); pb_ref(&b, w->page_ids[(int)pg - 1]);
            double r[4];
            if (pw_box_of(d, kCGPDFOutlineDestinationRect, r)) { pb_s(&b, " /FitR "); pb_nums(&b, r, 4); }
            else pb_s(&b, " /Fit");
            pb_s(&b, "]");
        } else if (pg < 0) {
            const char *s = cx_cstr(dest);
            if (s && *s) { pb_s(&b, " /Dest "); pw_str(w, &b, id_, (const u8 *)s, strlen(s)); }
        }
    }
    id kids = CFDictionaryGetValue(d, kCGPDFOutlineChildren);
    int f = 0, l = 0, n = 0;
    if (kids && CFArrayGetCount(kids) > 0) {
        pw_outline_items(w, kids, id_, &f, &l, &n);
        pb_s(&b, " /First "); pb_ref(&b, f); pb_s(&b, " /Last "); pb_ref(&b, l); pb_s(&b, " /Count "); pb_int(&b, n);
    }
    pb_s(&b, " >>");
    pw_obj(w, id_, &b);
    free(b.b);
    *count += 1 + n;
    return id_;
}
static int pw_outline_items(pdfw *w, id arr, int parent, int *first, int *last, int *count) {
    long n = CFArrayGetCount(arr);
    if (n <= 0) return 0;
    int *ids = malloc((size_t)n * sizeof *ids);
    if (!ids) return 0;
    for (long i = 0; i < n; i++) ids[i] = pw_newid(w);
    for (long i = 0; i < n; i++)
        pw_outline_item(w, CFArrayGetValueAtIndex(arr, i), ids[i], parent, i ? ids[i - 1] : 0, i + 1 < n ? ids[i + 1] : 0, count);
    *first = ids[0]; *last = ids[n - 1];
    free(ids);
    return 1;
}
static int pw_dest_cmp(const pw_dest *a, const pw_dest *b) { return strcmp(a->name ? a->name : "", b->name ? b->name : ""); }
static void pw_date(char *out) {
    long t = time(0);
    long days = t / 86400, rem = t % 86400;
    /* civil a partir de dias desde 1970 (algoritmo de Howard Hinnant) */
    long z = days + 719468, era = (z >= 0 ? z : z - 146096) / 146097, doe = z - era * 146097;
    long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365, y = yoe + era * 400, doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    long mp = (5 * doy + 2) / 153, d = doy - (153 * mp + 2) / 5 + 1, m = mp < 10 ? mp + 3 : mp - 9;
    if (m <= 2) y++;
    long v[6] = {y, m, d, rem / 3600, rem / 60 % 60, rem % 60};
    int k = 0;
    out[k++] = 'D'; out[k++] = ':';
    for (int i = 0; i < 6; i++) {
        long x = v[i];
        if (i == 0) { out[k++] = (char)('0' + x / 1000 % 10); out[k++] = (char)('0' + x / 100 % 10); }
        out[k++] = (char)('0' + x / 10 % 10); out[k++] = (char)('0' + x % 10);
    }
    out[k++] = 'Z'; out[k] = 0;
}
static void pw_close(cgobj_t *o) {
    pdfw *w = o->pdf;
    if (!w || w->closed) return;
    if (w->in_page) CGPDFContextEndPage(o);
    w->closed = 1;
    /* fontes Type3 */
    for (int i = 0; i < w->nfaces; i++)
        for (int k = 0; k < w->faces[i].nf; k++) {
            pw_t3 *t = &w->faces[i].f[k];
            pbuf cm = {0};
            pb_s(&cm, "/CIDInit /ProcSet findresource begin\n12 dict begin\nbegincmap\n/CIDSystemInfo << /Registry (Adobe) /Ordering (UCS) /Supplement 0 >> def\n"
                      "/CMapName /Adobe-Identity-UCS def\n/CMapType 2 def\n1 begincodespacerange\n<00> <FF>\nendcodespacerange\n");
            int nu = 0;
            for (int g = 0; g < t->n; g++) if (t->uni[g]) nu++;
            for (int g = 0, done = 0; g < t->n; g++) {
                if (!t->uni[g]) continue;
                if (done % 100 == 0) { pb_int(&cm, nu - done < 100 ? nu - done : 100); pb_s(&cm, " beginbfchar\n"); }
                static const char hx[] = "0123456789ABCDEF";
                char c[4] = {'<', hx[g >> 4], hx[g & 15], '>'};
                pb_put(&cm, c, 4);
                u32 u = t->uni[g];
                char s[16];
                int sn = 0;
                s[sn++] = ' '; s[sn++] = '<';
                u32 units[2];
                int nunit = 1;
                if (u >= 0x10000) { u32 v = u - 0x10000; units[0] = 0xD800 + (v >> 10); units[1] = 0xDC00 + (v & 1023); nunit = 2; }
                else units[0] = u;
                pb_put(&cm, s, (size_t)sn);
                for (int q = 0; q < nunit; q++) { char h4[4] = {hx[units[q] >> 12 & 15], hx[units[q] >> 8 & 15], hx[units[q] >> 4 & 15], hx[units[q] & 15]}; pb_put(&cm, h4, 4); }
                pb_s(&cm, ">\n");
                done++;
                if (done % 100 == 0 || done == nu) pb_s(&cm, "endbfchar\n");
            }
            pb_s(&cm, "endcmap\nCMapName currentdict /CMap defineresource pop\nend\nend\n");
            int tu = pw_newid(w);
            pw_stream(w, tu, "", cm.b, cm.n, 1);
            free(cm.b);
            pbuf f = {0};
            pb_s(&f, "<< /Type /Font /Subtype /Type3 /FontBBox ["); pb_nums(&f, t->bb, 4);
            pb_s(&f, "] /FontMatrix [0.001 0 0 0.001 0 0] /Resources << >> /FirstChar 0 /LastChar "); pb_int(&f, t->n - 1);
            pb_s(&f, " /Widths ["); for (int g = 0; g < t->n; g++) { if (g) pb_sp(&f); pb_num(&f, t->w[g]); }
            pb_s(&f, "] /Encoding << /Type /Encoding /Differences [0");
            for (int g = 0; g < t->n; g++) { pb_s(&f, " /g"); pb_int(&f, t->gid[g]); }
            pb_s(&f, "] >> /CharProcs <<");
            for (int g = 0; g < t->n; g++) { pb_s(&f, " /g"); pb_int(&f, t->gid[g]); pb_sp(&f); pb_ref(&f, t->proc[g]); }
            pb_s(&f, " >> /ToUnicode "); pb_ref(&f, tu); pb_s(&f, " >>");
            pw_obj(w, t->obj, &f);
            free(f.b);
        }
    /* recursos */
    pbuf r = {0};
    pb_s(&r, "<< /ProcSet [/PDF /Text /ImageB /ImageC /ImageI]");
    if (w->rx.n) { pb_s(&r, "\n/XObject <<\n"); pb_put(&r, w->rx.b, w->rx.n); pb_s(&r, ">>"); }
    if (w->rg.n) { pb_s(&r, "\n/ExtGState <<\n"); pb_put(&r, w->rg.b, w->rg.n); pb_s(&r, ">>"); }
    if (w->rsh.n) { pb_s(&r, "\n/Shading <<\n"); pb_put(&r, w->rsh.b, w->rsh.n); pb_s(&r, ">>"); }
    if (w->rf.n) { pb_s(&r, "\n/Font <<\n"); pb_put(&r, w->rf.b, w->rf.n); pb_s(&r, ">>"); }
    pb_s(&r, " >>");
    pw_obj(w, w->res_id, &r);
    free(r.b);
    /* árvore de páginas */
    pbuf p = {0};
    pb_s(&p, "<< /Type /Pages /Kids [");
    for (int i = 0; i < w->npages; i++) { pb_ref(&p, w->page_ids[i]); pb_sp(&p); }
    pb_s(&p, "] /Count "); pb_int(&p, w->npages); pb_s(&p, " >>");
    pw_obj(w, w->pages_id, &p);
    free(p.b);
    /* estrutura */
    int stroot = 0;
    if (w->nel) {
        stroot = pw_newid(w);
        int ptid = pw_newid(w);
        for (int e = 0; e < w->nel; e++) {
            pw_elem_t *el = &w->el[e];
            pbuf b = {0};
            pb_s(&b, "<< /Type /StructElem /S /"); pb_s(&b, el->type);
            pb_s(&b, " /P "); pb_ref(&b, el->parent >= 0 ? w->el[el->parent].id : stroot);
            int pg = el->page < w->npages ? el->page : w->npages - 1;
            if (pg >= 0) { pb_s(&b, " /Pg "); pb_ref(&b, w->page_ids[pg]); }
            if (el->alt) { pb_s(&b, " /Alt "); pw_text(w, &b, el->id, el->alt); }
            if (el->actual) { pb_s(&b, " /ActualText "); pw_text(w, &b, el->id, el->actual); }
            if (el->title) { pb_s(&b, " /T "); pw_text(w, &b, el->id, el->title); }
            if (el->lang) { pb_s(&b, " /Lang "); pw_text(w, &b, el->id, el->lang); }
            pb_s(&b, " /K [");
            for (int k = 0; k < el->nkid; k++) {
                int v = el->kid[k];
                if (v > 0) {
                    pb_s(&b, " << /Type /MCR /MCID "); pb_int(&b, v - 1);
                    if (el->kpage[k] < w->npages) { pb_s(&b, " /Pg "); pb_ref(&b, w->page_ids[el->kpage[k]]); }
                    pb_s(&b, " >>");
                } else { pb_sp(&b); pb_ref(&b, w->el[-v - 1].id); }
            }
            pb_s(&b, " ] >>");
            pw_obj(w, el->id, &b);
            free(b.b);
        }
        pbuf pt = {0};
        pb_s(&pt, "<< /Nums [");
        for (int i = 0; i < w->nptree; i++) { pb_int(&pt, i); pb_sp(&pt); pb_ref(&pt, w->ptree[i]); pb_sp(&pt); }
        pb_s(&pt, "] >>");
        pw_obj(w, ptid, &pt);
        free(pt.b);
        pbuf sr = {0};
        pb_s(&sr, "<< /Type /StructTreeRoot /K [");
        for (int e = 0; e < w->nel; e++) if (w->el[e].parent < 0) { pb_ref(&sr, w->el[e].id); pb_sp(&sr); }
        pb_s(&sr, "] /ParentTree "); pb_ref(&sr, ptid); pb_s(&sr, " /ParentTreeNextKey "); pb_int(&sr, w->nptree); pb_s(&sr, " >>");
        pw_obj(w, stroot, &sr);
        free(sr.b);
    }
    /* outline */
    int outl = 0;
    if (w->outline) {
        id kids = CFDictionaryGetValue(w->outline, kCGPDFOutlineChildren);
        if (kids && CFArrayGetCount(kids) > 0) {
            outl = pw_newid(w);
            int f = 0, l = 0, n = 0;
            pw_outline_items(w, kids, outl, &f, &l, &n);
            pbuf b = {0};
            pb_s(&b, "<< /Type /Outlines /First "); pb_ref(&b, f); pb_s(&b, " /Last "); pb_ref(&b, l); pb_s(&b, " /Count "); pb_int(&b, n); pb_s(&b, " >>");
            pw_obj(w, outl, &b);
            free(b.b);
        }
    }
    /* destinos nomeados (árvore de nomes com uma folha, em ordem) */
    int dests = 0;
    if (w->ndests) {
        for (int i = 1; i < w->ndests; i++)
            for (int j = i; j > 0 && pw_dest_cmp(&w->dests[j - 1], &w->dests[j]) > 0; j--) { pw_dest t = w->dests[j]; w->dests[j] = w->dests[j - 1]; w->dests[j - 1] = t; }
        dests = pw_newid(w);
        pbuf b = {0};
        pb_s(&b, "<< /Names [");
        for (int i = 0; i < w->ndests; i++) {
            pw_dest *d = &w->dests[i];
            if (d->page >= w->npages) continue;
            pw_str(w, &b, dests, (const u8 *)d->name, strlen(d->name));
            pb_s(&b, " ["); pb_ref(&b, w->page_ids[d->page]); pb_s(&b, " /XYZ "); pb_num(&b, d->x); pb_sp(&b); pb_num(&b, d->y); pb_s(&b, " 0] ");
        }
        pb_s(&b, "] >>");
        pw_obj(w, dests, &b);
        free(b.b);
    }
    /* metadados: XMP do app, ou o mínimo do PDF/A */
    int meta = 0;
    char date[24];
    pw_date(date);
    if (w->xmp || w->pdfa) {
        meta = pw_newid(w);
        pbuf x = {0};
        if (w->xmp) pb_put(&x, CFDataGetBytePtr(w->xmp), (size_t)CFDataGetLength(w->xmp));
        else {
            pb_s(&x, "<?xpacket begin=\"\xEF\xBB\xBF\" id=\"W5M0MpCehiHzreSzNTczkc9d\"?>\n<x:xmpmeta xmlns:x=\"adobe:ns:meta/\">\n"
                     "<rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">\n"
                     "<rdf:Description rdf:about=\"\" xmlns:pdfaid=\"http://www.aiim.org/pdfa/ns/id/\" xmlns:dc=\"http://purl.org/dc/elements/1.1/\""
                     " xmlns:pdf=\"http://ns.adobe.com/pdf/1.3/\" xmlns:xmp=\"http://ns.adobe.com/xap/1.0/\">\n"
                     "<pdfaid:part>2</pdfaid:part><pdfaid:conformance>B</pdfaid:conformance>\n<pdf:Producer>mOS Quartz PDFContext</pdf:Producer>\n");
            if (w->title) { pb_s(&x, "<dc:title><rdf:Alt><rdf:li xml:lang=\"x-default\">"); pb_s(&x, w->title); pb_s(&x, "</rdf:li></rdf:Alt></dc:title>\n"); }
            if (w->author) { pb_s(&x, "<dc:creator><rdf:Seq><rdf:li>"); pb_s(&x, w->author); pb_s(&x, "</rdf:li></rdf:Seq></dc:creator>\n"); }
            pb_s(&x, "</rdf:Description>\n</rdf:RDF>\n</x:xmpmeta>\n<?xpacket end=\"w\"?>");
        }
        pw_stream(w, meta, " /Type /Metadata /Subtype /XML", x.b, x.n, 0);
        free(x.b);
    }
    /* OutputIntent sRGB (PDF/A) */
    int intent = 0;
    if (w->pdfa) {
        void *srgb = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
        id icc = srgb ? CGColorSpaceCopyICCData(srgb) : 0;
        if (srgb) CGColorSpaceRelease(srgb);
        if (icc) {
            int prof = pw_newid(w);
            pw_stream(w, prof, " /N 3", CFDataGetBytePtr(icc), (size_t)CFDataGetLength(icc), 1);
            CFRelease(icc);
            intent = pw_newid(w);
            pbuf b = {0};
            pb_s(&b, "<< /Type /OutputIntent /S /GTS_PDFA1 /OutputConditionIdentifier (sRGB IEC61966-2.1) /Info (sRGB IEC61966-2.1) /DestOutputProfile ");
            pb_ref(&b, prof);
            pb_s(&b, " >>");
            pw_obj(w, intent, &b);
            free(b.b);
        }
    }
    /* Info */
    pbuf in = {0};
    pb_s(&in, "<< /Producer "); pw_text(w, &in, w->info_id, "mOS Quartz PDFContext");
    pb_s(&in, " /CreationDate "); pw_str(w, &in, w->info_id, (const u8 *)date, strlen(date));
    pb_s(&in, " /ModDate "); pw_str(w, &in, w->info_id, (const u8 *)date, strlen(date));
    if (w->title) { pb_s(&in, " /Title "); pw_text(w, &in, w->info_id, w->title); }
    if (w->author) { pb_s(&in, " /Author "); pw_text(w, &in, w->info_id, w->author); }
    if (w->subject) { pb_s(&in, " /Subject "); pw_text(w, &in, w->info_id, w->subject); }
    if (w->keywords) { pb_s(&in, " /Keywords "); pw_text(w, &in, w->info_id, w->keywords); }
    if (w->creator) { pb_s(&in, " /Creator "); pw_text(w, &in, w->info_id, w->creator); }
    pb_s(&in, " >>");
    pw_obj(w, w->info_id, &in);
    free(in.b);
    /* catálogo */
    pbuf c = {0};
    pb_s(&c, "<< /Type /Catalog /Pages "); pb_ref(&c, w->pages_id);
    if (outl) { pb_s(&c, " /Outlines "); pb_ref(&c, outl); pb_s(&c, " /PageMode /UseOutlines"); }
    if (dests) { pb_s(&c, " /Names << /Dests "); pb_ref(&c, dests); pb_s(&c, " >>"); }
    if (meta) { pb_s(&c, " /Metadata "); pb_ref(&c, meta); }
    if (intent) { pb_s(&c, " /OutputIntents ["); pb_ref(&c, intent); pb_s(&c, "]"); }
    if (stroot) { pb_s(&c, " /StructTreeRoot "); pb_ref(&c, stroot); pb_s(&c, " /MarkInfo << /Marked true >>"); }
    pb_s(&c, " >>");
    pw_obj(w, w->catalog_id, &c);
    free(c.b);
    /* cifra */
    int enc = 0;
    if (w->crypt) {
        int saved = w->crypt;
        w->crypt = 0;                                /* o dicionário de cifra não é cifrado */
        enc = pw_newid(w);
        pbuf e = {0};
        pb_s(&e, "<< /Filter /Standard /V "); pb_int(&e, w->rev == 2 ? 1 : 2); pb_s(&e, " /R "); pb_int(&e, w->rev);
        pb_s(&e, " /Length "); pb_int(&e, w->klen * 8); pb_s(&e, " /P "); pb_int(&e, w->P);
        pb_s(&e, " /O "); pw_str(w, &e, 0, w->O, 32); pb_s(&e, " /U "); pw_str(w, &e, 0, w->U, 32); pb_s(&e, " >>");
        pw_obj(w, enc, &e);
        free(e.b);
        w->crypt = saved;
    }
    /* xref e trailer */
    size_t xoff = w->off;
    pbuf x = {0};
    pb_s(&x, "xref\n0 "); pb_int(&x, w->nobj + 1); pb_s(&x, "\n0000000000 65535 f \n");
    for (int i = 1; i <= w->nobj; i++) {
        char line[21];
        u32 v = w->xref[i];
        for (int k = 9; k >= 0; k--) { line[k] = (char)('0' + v % 10); v /= 10; }
        memcpy(line + 10, w->xref[i] || i == 0 ? " 00000 n \n" : " 00000 f \n", 10);
        if (!w->xref[i]) memcpy(line, "0000000000 65535 f \n", 20);
        pb_put(&x, line, 20);
    }
    pb_s(&x, "trailer\n<< /Size "); pb_int(&x, w->nobj + 1);
    pb_s(&x, " /Root "); pb_ref(&x, w->catalog_id); pb_s(&x, " /Info "); pb_ref(&x, w->info_id);
    int cr = w->crypt;
    w->crypt = 0;
    pb_s(&x, " /ID ["); pw_str(w, &x, 0, w->fid, 16); pw_str(w, &x, 0, w->fid, 16); pb_s(&x, "]");
    w->crypt = cr;
    if (enc) { pb_s(&x, " /Encrypt "); pb_ref(&x, enc); }
    pb_s(&x, " >>\nstartxref\n"); pb_int(&x, (long)xoff); pb_s(&x, "\n%%EOF\n");
    pw_write(w, x.b, x.n);
    free(x.b);
}
static void pw_free(pdfw *w) {
    if (!w) return;
    if (w->consumer) CGDataConsumerRelease(w->consumer);
    free(w->xref); free(w->page_ids);
    free(w->rx.b); free(w->rg.b); free(w->rsh.b); free(w->rf.b); free(w->page.b);
    for (int i = 0; i < w->nimgs; i++) objc_release((id)w->imgs[i].img);
    free(w->imgs); free(w->gss);
    for (int i = 0; i < w->nfaces; i++) { free(w->faces[i].f); free(w->faces[i].map); }
    free(w->faces);
    for (int i = 0; i < w->ndests; i++) free(w->dests[i].name);
    free(w->dests);
    for (int i = 0; i < w->nlinks; i++) { free(w->links[i].uri); free(w->links[i].dest); }
    free(w->links);
    for (int i = 0; i < w->nel; i++) { free(w->el[i].kid); free(w->el[i].kpage); free(w->el[i].alt); free(w->el[i].actual); free(w->el[i].title); free(w->el[i].lang); }
    free(w->el); free(w->pmc); free(w->ptree);
    for (int i = 0; i < w->nlayer; i++) { free(w->lstack[i]->b); free(w->lstack[i]); }
    if (w->outline) CFRelease(w->outline);
    if (w->xmp) CFRelease(w->xmp);
    free(w->title); free(w->author); free(w->subject); free(w->keywords); free(w->creator);
    free(w);
}
void CGPDFContextClose(void *c) {
    cgobj_t *o = as_kind(c, CGK_BM);
    if (o && o->pdf) pw_close(o);
}

/* ---------------------------------------------------------------- cifra padrão (algoritmos 2–5 da norma) */
static const u8 pw_pad[32] = {0x28, 0xBF, 0x4E, 0x5E, 0x4E, 0x75, 0x8A, 0x41, 0x64, 0x00, 0x4E, 0x56, 0xFF, 0xFA, 0x01, 0x08,
                              0x2E, 0x2E, 0x00, 0xB6, 0xD0, 0x68, 0x3E, 0x80, 0x2F, 0x0C, 0xA9, 0xFE, 0x64, 0x53, 0x69, 0x7A};
static void pw_padpw(const char *s, u8 *out) {
    size_t n = s ? strlen(s) : 0;
    if (n > 32) n = 32;
    memcpy(out, s, n);
    memcpy(out + n, pw_pad, 32 - n);
}
static void pw_setup_crypt(pdfw *w, const char *user, const char *owner, int bits, int32_t P) {
    w->crypt = 1;
    w->rev = bits <= 40 ? 2 : 3;
    w->klen = w->rev == 2 ? 5 : 16;
    w->P = P;
    u8 up[32], op[32], h[16];
    pw_padpw(user, up);
    pw_padpw(owner && *owner ? owner : user, op);
    /* O */
    pdf_md5(op, 32, h);
    if (w->rev == 3) for (int i = 0; i < 50; i++) pdf_md5(h, (size_t)w->klen, h);
    memcpy(w->O, up, 32);
    pdf_rc4(h, (size_t)w->klen, w->O, 32);
    if (w->rev == 3)
        for (int i = 1; i <= 19; i++) { u8 k[16]; for (int j = 0; j < w->klen; j++) k[j] = (u8)(h[j] ^ i); pdf_rc4(k, (size_t)w->klen, w->O, 32); }
    /* chave do documento */
    u8 b[32 + 32 + 4 + 16];
    memcpy(b, up, 32); memcpy(b + 32, w->O, 32);
    b[64] = (u8)P; b[65] = (u8)(P >> 8); b[66] = (u8)(P >> 16); b[67] = (u8)(P >> 24);
    memcpy(b + 68, w->fid, 16);
    pdf_md5(b, sizeof b, h);
    if (w->rev == 3) for (int i = 0; i < 50; i++) pdf_md5(h, (size_t)w->klen, h);
    memcpy(w->key, h, (size_t)w->klen);
    /* U */
    if (w->rev == 2) { memcpy(w->U, pw_pad, 32); pdf_rc4(w->key, 5, w->U, 32); }
    else {
        u8 t[48];
        memcpy(t, pw_pad, 32); memcpy(t + 32, w->fid, 16);
        pdf_md5(t, 48, h);
        pdf_rc4(w->key, 16, h, 16);
        for (int i = 1; i <= 19; i++) { u8 k[16]; for (int j = 0; j < 16; j++) k[j] = (u8)(w->key[j] ^ i); pdf_rc4(k, 16, h, 16); }
        memcpy(w->U, h, 16);
        memcpy(w->U + 16, pw_pad, 16);
    }
}

/* ---------------------------------------------------------------- criação */
static char *pw_info_str(id aux, const void *key) {
    id v = aux ? CFDictionaryGetValue(aux, key) : 0;
    if (!v) return 0;
    if (cx_is_str(v)) return pw_cfstr_dup(v);
    /* Keywords pode vir como array de strings */
    long n = CFArrayGetCount(v);
    if (n <= 0) return 0;
    pbuf b = {0};
    for (long i = 0; i < n; i++) { const char *s = cx_cstr(CFArrayGetValueAtIndex(v, i)); if (i) pb_s(&b, ", "); if (s) pb_s(&b, s); }
    pb_put(&b, "", 1);
    return (char *)b.b;
}
static int pw_bool(id aux, const void *key, int dflt) {
    id v = aux ? CFDictionaryGetValue(aux, key) : 0;
    if (!v) return dflt;
    return (const void *)v == kCFBooleanTrue;
}
void *CGPDFContextCreate(void *consumer, const CGRect *mediaBox, id aux) {
    if (!consumer) return 0;
    double mb[4] = {0, 0, 612, 792};
    if (mediaBox && mediaBox->size.width > 0 && mediaBox->size.height > 0) {
        mb[0] = mediaBox->origin.x; mb[1] = mediaBox->origin.y; mb[2] = mediaBox->origin.x + mediaBox->size.width; mb[3] = mediaBox->origin.y + mediaBox->size.height;
    }
    double W = mb[2] - mb[0], H = mb[3] - mb[1];
    if (W > 14400 || H > 14400) return 0;            /* limite do PDF: 200 polegadas */
    void *dev = CGColorSpaceCreateDeviceRGB();
    cgobj_t *o = CGBitmapContextCreate(0, (size_t)(W + 0.5) ? (size_t)(W + 0.5) : 1, (size_t)(H + 0.5) ? (size_t)(H + 0.5) : 1, 8, 0, dev, AI_PFIRST | BO_32L);
    CGColorSpaceRelease(dev);
    if (!o) return 0;
    pdfw *w = calloc(1, sizeof *w);
    if (!w) { objc_release((id)o); return 0; }
    o->pdf = w;
    w->consumer = CGDataConsumerRetain(consumer);
    memcpy(w->defmb, mb, sizeof mb);
    memcpy(w->mb, mb, sizeof mb);
    w->cur_mc = -1;
    w->catalog_id = pw_newid(w);
    w->pages_id = pw_newid(w);
    w->res_id = pw_newid(w);
    w->info_id = pw_newid(w);
    w->title = pw_info_str(aux, kCGPDFContextTitle);
    w->author = pw_info_str(aux, kCGPDFContextAuthor);
    w->subject = pw_info_str(aux, kCGPDFContextSubject);
    w->keywords = pw_info_str(aux, kCGPDFContextKeywords);
    w->creator = pw_info_str(aux, kCGPDFContextCreator);
    w->pdfa = pw_bool(aux, kCGPDFContextCreatePDFA, 0);
    /* ID do arquivo: MD5 do instante, do endereço e dos metadados */
    pbuf seed = {0};
    pb_int(&seed, time(0)); pb_int(&seed, (long)(size_t)w);
    if (w->title) pb_s(&seed, w->title);
    pdf_md5(seed.b, seed.n, w->fid);
    free(seed.b);
    char *upw = pw_info_str(aux, kCGPDFContextUserPassword), *opw = pw_info_str(aux, kCGPDFContextOwnerPassword);
    if ((upw || opw) && !w->pdfa) {
        int bits = (int)pw_cfnum(aux ? CFDictionaryGetValue(aux, kCGPDFContextEncryptionKeyLength) : 0, 128);
        /* permissões: bits 3 imprimir, 4 alterar, 5 copiar, 6 anotar, 9 formulários, 10 acessibilidade, 11 montar, 12 alta qualidade */
        int32_t P = (int32_t)0xFFFFF0C0u;
        id ap = aux ? CFDictionaryGetValue(aux, kCGPDFContextAccessPermissions) : 0;
        if (ap) {
            int v = (int)pw_cfnum(ap, 0);
            if (v & 1) P |= 4;
            if (v & 2) P |= 4 | 2048;
            if (v & 4) P |= 8;
            if (v & 8) P |= 1024;
            if (v & 16) P |= 16;
            if (v & 32) P |= 512;
            if (v & 64) P |= 32;
            if (v & 128) P |= 256;
        } else {
            P |= 8 | 32 | 256 | 512 | 1024;
            if (pw_bool(aux, kCGPDFContextAllowsPrinting, 1)) P |= 4 | 2048;
            if (pw_bool(aux, kCGPDFContextAllowsCopying, 1)) P |= 16;
        }
        pw_setup_crypt(w, upw ? upw : "", opw, bits, P);
    }
    free(upw); free(opw);
    pw_writes(w, w->pdfa ? "%PDF-1.7\n%\xE2\xE3\xCF\xD3\n" : "%PDF-1.7\n%\xE2\xE3\xCF\xD3\n");
    return o;
}
void *CGPDFContextCreateWithURL(id url, const CGRect *mediaBox, id aux) {
    void *cons = url ? CGDataConsumerCreateWithURL(url) : 0;
    if (!cons) return 0;
    void *c = CGPDFContextCreate(cons, mediaBox, aux);
    CGDataConsumerRelease(cons);
    return c;
}
