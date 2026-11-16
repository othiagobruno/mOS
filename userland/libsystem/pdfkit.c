/*
 * mOS PDFKit no libSystem (PDFKit.framework sem dylib cai aqui, kernel/compat/macho.c). Modelo: PDFDocument,
 * PDFPage, PDFSelection, PDFAnnotation, PDFOutline, PDFDestination, PDFAction (URL, GoTo, Named), PDFBorder e as
 * constantes. O PDFView e o PDFThumbnailView estão em pdfkit_view.c.
 * Por baixo: o leitor e o desenho do Core Graphics (cgpdf.c, cgpdf_draw.c), o texto da página com a caixa de cada
 * caractere (cx_pdf_page_text), a regravação (pdf_write.c) e o CGPDFContext (página a partir de imagem).
 * O que é texto puro (busca, palavra, linha) está em pdfkit_text.inc, com teste de host (tools/pdfkit_host.c).
 */
#include <stdbool.h>
#include "foundation.h"
#include "libc.h"
#include "pdf_core.h"
#include "pdf_write.h"

typedef struct { double x, y; } P;
typedef struct { double w, h; } S;
typedef struct { P o; S s; } R;
typedef struct { double a, b, c, d, tx, ty; } T;
#include "pdfkit_text.inc"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
id objc_msgSend_stret(id self, SEL op, ...);
void *CFStringCreateWithCharacters(void *alloc, const u16 *chars, long n);
const u8 *CFDataGetBytePtr(void *d);
long CFDataGetLength(void *d);
/* Core Graphics */
void *CGPDFDocumentCreateWithURL(id url);
void *CGPDFDocumentCreateWithProvider(void *prov);
void CGPDFDocumentRelease(void *d);
void *CGPDFDocumentRetain(void *d);
size_t CGPDFDocumentGetNumberOfPages(void *d);
void *CGPDFDocumentGetPage(void *d, size_t n);
bool CGPDFDocumentIsEncrypted(void *d);
bool CGPDFDocumentIsUnlocked(void *d);
bool CGPDFDocumentUnlockWithPassword(void *d, const char *pw);
void CGPDFDocumentGetVersion(void *d, int *major, int *minor);
u32 CGPDFDocumentGetAccessPermissions(void *d);
void *CGPDFPageRetain(void *p);
void CGPDFPageRelease(void *p);
pdf_obj *CGPDFPageGetDictionary(void *p);
R CGPDFPageGetBoxRect(void *p, int box);
int CGPDFPageGetRotationAngle(void *p);
pdf_doc *cx_pdf_page_doc(void *p);
int cx_pdf_dest_page(pdf_doc *pd, pdf_obj *dest);          /* 1 = primeira página */
pdf_obj *cx_pdf_dest_array(pdf_doc *pd, pdf_obj *dest);
int cx_pdf_text_utf16(const u8 *b, size_t n, u16 **out);
int cx_pdf_page_text(void *page, u16 **chars, R **boxes);
void cx_pdf_draw_page_content(void *ctx, void *page);
void cx_pdf_draw_annotation(void *ctx, void *page, pdf_obj *annot);
void *CGDataProviderCreateWithData(void *info, const void *data, size_t n, void (*rel)(void *, const void *, size_t));
void CGDataProviderRelease(void *p);
void *CGDataConsumerCreateWithCFData(id data);
void CGDataConsumerRelease(void *c);
void *CGPDFContextCreate(void *consumer, const R *mb, id aux);
void CGPDFContextClose(void *c);
void CGContextBeginPage(void *c, const R *mb);
void CGContextEndPage(void *c);
void CGContextRelease(void *c);
void CGContextSaveGState(void *c);
void CGContextRestoreGState(void *c);
void CGContextConcatCTM(void *c, T t);
void CGContextTranslateCTM(void *c, double x, double y);
void CGContextScaleCTM(void *c, double x, double y);
void CGContextDrawImage(void *c, R r, void *img);
void CGContextClipToRect(void *c, R r);
void CGContextSetRGBFillColor(void *c, double r, double g, double b, double a);
void CGContextSetRGBStrokeColor(void *c, double r, double g, double b, double a);
void CGContextFillRect(void *c, R r);
void CGContextStrokeRect(void *c, R r);
void CGContextStrokeRectWithWidth(void *c, R r, double w);
void CGContextSetLineWidth(void *c, double w);
void CGContextBeginPath(void *c);
void CGContextMoveToPoint(void *c, double x, double y);
void CGContextAddLineToPoint(void *c, double x, double y);
void CGContextAddEllipseInRect(void *c, R r);
void CGContextStrokePath(void *c);
void CGContextFillPath(void *c);
void CGContextSetBlendMode(void *c, int m);
void *CGBitmapContextCreate(void *data, size_t w, size_t h, size_t bpc, size_t bpr, void *space, u32 info);
void *CGBitmapContextCreateImage(void *c);
void *CGColorSpaceCreateDeviceRGB(void);
void CGColorSpaceRelease(void *s);
void CGImageRelease(void *i);
int cx_deflate(const u8 *in, size_t n, u8 **out, size_t *on);

/* ---------------------------------------------------------------- classes */
#define PKC(N) cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N); static cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N);
PKC(PDFDocument)
PKC(PDFPage)
PKC(PDFSelection)
PKC(PDFAnnotation)
PKC(PDFOutline)
PKC(PDFDestination)
PKC(PDFAction)
PKC(PDFActionURL)
PKC(PDFActionGoTo)
PKC(PDFActionNamed)
PKC(PDFActionResetForm)
PKC(PDFActionRemoteGoTo)
PKC(PDFBorder)
PKC(PDFAppearanceCharacteristics)

/* constantes NSString (valores do macOS) */
extern char pk_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } pk_const_t;
#define PKK(var, text) static pk_const_t var##_s = {pk_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; const void *var = &var##_s;
PKK(PDFDocumentTitleAttribute, "Title")
PKK(PDFDocumentAuthorAttribute, "Author")
PKK(PDFDocumentSubjectAttribute, "Subject")
PKK(PDFDocumentCreatorAttribute, "Creator")
PKK(PDFDocumentProducerAttribute, "Producer")
PKK(PDFDocumentCreationDateAttribute, "CreationDate")
PKK(PDFDocumentModificationDateAttribute, "ModDate")
PKK(PDFDocumentKeywordsAttribute, "Keywords")
PKK(PDFDocumentOwnerPasswordOption, "OwnerPassword")
PKK(PDFDocumentUserPasswordOption, "UserPassword")
PKK(PDFDocumentAccessPermissionsOption, "AccessPermissions")
PKK(PDFDocumentBurnInAnnotationsOption, "BurnInAnnotations")
PKK(PDFDocumentSaveTextFromOCROption, "SaveTextFromOCR")
PKK(PDFDocumentSaveImagesAsJPEGOption, "SaveImagesAsJPEG")
PKK(PDFDocumentOptimizeImagesForScreenOption, "OptimizeImagesForScreen")
PKK(PDFDocumentDidUnlockNotification, "PDFDidUnlock")
PKK(PDFDocumentDidBeginFindNotification, "PDFDidBeginFind")
PKK(PDFDocumentDidEndFindNotification, "PDFDidEndFind")
PKK(PDFDocumentDidBeginPageFindNotification, "PDFDidBeginPageFind")
PKK(PDFDocumentDidEndPageFindNotification, "PDFDidEndPageFind")
PKK(PDFDocumentDidFindMatchNotification, "PDFDidFindMatch")
PKK(PDFDocumentDidBeginWriteNotification, "PDFDidBeginDocumentWrite")
PKK(PDFDocumentDidEndWriteNotification, "PDFDidEndDocumentWrite")
PKK(PDFDocumentDidBeginPageWriteNotification, "PDFDidBeginPageWrite")
PKK(PDFDocumentDidEndPageWriteNotification, "PDFDidEndPageWrite")
PKK(PDFDocumentFoundSelectionKey, "PDFDocumentFoundSelection")
PKK(PDFDocumentPageIndexKey, "PDFDocumentPageIndex")
PKK(PDFAnnotationSubtypeText, "/Text")
PKK(PDFAnnotationSubtypeLink, "/Link")
PKK(PDFAnnotationSubtypeFreeText, "/FreeText")
PKK(PDFAnnotationSubtypeLine, "/Line")
PKK(PDFAnnotationSubtypeSquare, "/Square")
PKK(PDFAnnotationSubtypeCircle, "/Circle")
PKK(PDFAnnotationSubtypeHighlight, "/Highlight")
PKK(PDFAnnotationSubtypeUnderline, "/Underline")
PKK(PDFAnnotationSubtypeStrikeOut, "/StrikeOut")
PKK(PDFAnnotationSubtypeInk, "/Ink")
PKK(PDFAnnotationSubtypeStamp, "/Stamp")
PKK(PDFAnnotationSubtypePopup, "/Popup")
PKK(PDFAnnotationSubtypeWidget, "/Widget")
PKK(PDFAnnotationKeyAppearanceDictionary, "/AP")
PKK(PDFAnnotationKeyAppearanceState, "/AS")
PKK(PDFAnnotationKeyBorder, "/Border")
PKK(PDFAnnotationKeyColor, "/C")
PKK(PDFAnnotationKeyContents, "/Contents")
PKK(PDFAnnotationKeyFlags, "/F")
PKK(PDFAnnotationKeyDate, "/M")
PKK(PDFAnnotationKeyName, "/NM")
PKK(PDFAnnotationKeyPage, "/P")
PKK(PDFAnnotationKeyPopup, "/Popup")
PKK(PDFAnnotationKeyRect, "/Rect")
PKK(PDFAnnotationKeySubtype, "/Subtype")
PKK(PDFAnnotationKeyTitle, "/T")
PKK(PDFAnnotationKeyAction, "/A")
PKK(PDFAnnotationKeyDestination, "/Dest")
PKK(PDFAnnotationKeyInteriorColor, "/IC")
PKK(PDFAnnotationKeyQuadPoints, "/QuadPoints")
PKK(PDFAnnotationKeyIconName, "/Name")
PKK(PDFAnnotationKeyOpen, "/Open")
PKK(PDFAnnotationKeyInklist, "/InkList")
PKK(PDFAnnotationKeyLineEndingStyles, "/LE")
PKK(PDFAnnotationKeyLinePoints, "/L")
PKK(PDFAnnotationKeyDefaultAppearance, "/DA")
PKK(PDFAnnotationKeyTextLabel, "/T")
PKK(PDFAnnotationKeyWidgetFieldType, "/FT")
PKK(PDFAnnotationKeyWidgetValue, "/V")
PKK(PDFAnnotationKeyWidgetFieldFlags, "/Ff")
PKK(PDFAnnotationKeyHighlightingMode, "/H")
PKK(PDFAnnotationKeyAdditionalActions, "/AA")
PKK(PDFAnnotationKeyBorderStyle, "/BS")

/* estado de cada objeto: um ponteiro logo depois do que a superclasse usa */
static u32 g_off;
#define PK(o) (*(void **)((u8 *)(o) + g_off))
static id cls(const char *n) { return (id)cx_class_named(n); }
static id marr(void) { return objc_retain(MSG0(id, cls("NSMutableArray"), "array")); }
static long acount(id a) { return a ? MSG0(long, a, "count") : 0; }
static id aget(id a, long i) { return MSG(id, a, "objectAtIndex:", (unsigned long)i); }
static void keep(id *slot, id v) { if (v) objc_retain(v); if (*slot) objc_release(*slot); *slot = v; }
static void super_dealloc(id self, cx_class_t *k) {
    IMP imp = cx_lookup(k->super, sel_registerName("dealloc"));
    if (imp) ((void (*)(id, SEL))imp)(self, sel_registerName("dealloc"));
}
static id nsstr_utf16(const u16 *c, long n) { return objc_autorelease((id)CFStringCreateWithCharacters(0, c, n)); }
/* NSString → UTF-16 (malloc) */
static long str_utf16(id s, u16 **out) {
    const char *u = s ? cx_cstr(s) : "";
    size_t n = strlen(u);
    u16 *o = malloc((n + 1) * sizeof *o);
    long k = 0;
    if (!o) { *out = 0; return 0; }
    for (size_t i = 0; i < n;) {
        u32 c = (u8)u[i], ex = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
        c = ex == 3 ? c & 7 : ex == 2 ? c & 15 : ex == 1 ? c & 31 : c;
        i++;
        for (u32 e = 0; e < ex && i < n; e++, i++) c = c << 6 | ((u8)u[i] & 63);
        if (c >= 0x10000) { c -= 0x10000; o[k++] = (u16)(0xD800 + (c >> 10)); o[k++] = (u16)(0xDC00 + (c & 1023)); }
        else o[k++] = (u16)c;
    }
    *out = o;
    return k;
}
static void post(id name, id obj, id info) {
    id nc = MSG0(id, cls("NSNotificationCenter"), "defaultCenter");
    if (nc) MSG(void, nc, "postNotificationName:object:userInfo:", name, obj, info);
}

/* ---------------------------------------------------------------- estados */
typedef struct {
    void *cg;                      /* CGPDFDocument (retido) */
    id url, data, pages, attrs, outline, delegate;
    int modified;
    int finding;
} pk_doc;
typedef struct {
    id doc;                        /* fraco: o documento retém as páginas */
    void *cg;                      /* CGPDFPage (retida) */
    void *own_doc;                 /* documento de uma página feita com initWithImage: */
    int rotation;                  /* −1 = o do PDF */
    int hasbox[5]; R box[5];
    id annots;                     /* NSMutableArray, carregado na primeira vez */
    int annots_loaded, annots_changed, display_annots;
    u16 *text; R *tbox; long ntext; int text_loaded;
} pk_page;
typedef struct { id page; long loc, len; } pk_range;
typedef struct { id doc; pk_range *r; int n; id color; } pk_sel;
typedef struct {
    id page;                       /* fraco */
    R bounds;
    id type, contents, color, interior, url, action, user, border, dest, name, props, font, fontColor, popup;
    pdf_obj *src; pdf_doc *srcdoc;  /* dicionário de origem (aparência e cópia sem mudança) */
    int changed, should_display, should_print, open, highlighted;
    double *quad; int nquad;
    id ink;                        /* NSArray de NSBezierPath (Ink) */
} pk_annot;
typedef struct { id doc, parent, kids, label, dest, action; int open; } pk_outline;
typedef struct { id page; P pt; double zoom; } pk_dest;
typedef struct { id url, dest, name, fields; int kind; } pk_action;   /* kind 1 URL, 2 GoTo, 3 Named, 4 Reset, 5 Remote */
typedef struct { double lw; long style; id dash; } pk_border;

static void *st_new(id self, size_t n) { void *p = calloc(1, n); PK(self) = p; return p; }
static pk_doc *D(id o) { return o ? PK(o) : 0; }
static pk_page *G(id o) { return o ? PK(o) : 0; }
static pk_sel *SL(id o) { return o ? PK(o) : 0; }
static pk_annot *A(id o) { return o ? PK(o) : 0; }
static pk_outline *O(id o) { return o ? PK(o) : 0; }
static pk_dest *DS(id o) { return o ? PK(o) : 0; }
static pk_action *AC(id o) { return o ? PK(o) : 0; }
static pk_border *BD(id o) { return o ? PK(o) : 0; }

/* ---------------------------------------------------------------- PDFDestination e ações */
static id dest_new(id page, P pt) {
    id d = MSG0(id, (id)&PDFDestination_c, "alloc");
    d = MSG(id, d, "initWithPage:atPoint:", page, pt);
    return d;
}
static id ds_init(id self, SEL c, id page, P pt) { (void)c; pk_dest *s = st_new(self, sizeof(pk_dest)); s->page = page; s->pt = pt; s->zoom = 0; return self; }
static id ds_page(id self, SEL c) { (void)c; return DS(self) ? DS(self)->page : 0; }
static P ds_point(id self, SEL c) { (void)c; P z = {0, 0}; return DS(self) ? DS(self)->pt : z; }
static double ds_zoom(id self, SEL c) { (void)c; return DS(self) ? DS(self)->zoom : 0; }
static void ds_set_zoom(id self, SEL c, double z) { (void)c; if (DS(self)) DS(self)->zoom = z; }
static long ds_compare(id self, SEL c, id other) {
    (void)c;
    pk_dest *a = DS(self), *b = DS(other);
    if (!a || !b || !a->page || !b->page) return 0;
    id doc = G(a->page) ? G(a->page)->doc : 0;
    long ia = doc ? MSG(long, doc, "indexForPage:", a->page) : 0, ib = doc ? MSG(long, doc, "indexForPage:", b->page) : 0;
    if (ia != ib) return ia < ib ? -1 : 1;
    if (a->pt.y != b->pt.y) return a->pt.y > b->pt.y ? -1 : 1;          /* mais alto na página vem antes */
    if (a->pt.x != b->pt.x) return a->pt.x < b->pt.x ? -1 : 1;
    return 0;
}
static void ds_dealloc(id self, SEL c) { (void)c; free(DS(self)); PK(self) = 0; super_dealloc(self, &PDFDestination_c); }

static id ac_url_init(id self, SEL c, id url) { (void)c; pk_action *s = st_new(self, sizeof(pk_action)); s->kind = 1; keep(&s->url, url); return self; }
static id ac_url(id self, SEL c) { (void)c; return AC(self) ? AC(self)->url : 0; }
static void ac_set_url(id self, SEL c, id u) { (void)c; if (AC(self)) keep(&AC(self)->url, u); }
static id ac_goto_init(id self, SEL c, id d) { (void)c; pk_action *s = st_new(self, sizeof(pk_action)); s->kind = 2; keep(&s->dest, d); return self; }
static id ac_dest(id self, SEL c) { (void)c; return AC(self) ? AC(self)->dest : 0; }
static void ac_set_dest(id self, SEL c, id d) { (void)c; if (AC(self)) keep(&AC(self)->dest, d); }
static id ac_named_init(id self, SEL c, long name) { (void)c; pk_action *s = st_new(self, sizeof(pk_action)); s->kind = 3; s->fields = (id)(intptr_t)name; return self; }
static long ac_name(id self, SEL c) { (void)c; return AC(self) ? (long)(intptr_t)AC(self)->fields : 0; }
static void ac_set_name(id self, SEL c, long n) { (void)c; if (AC(self)) AC(self)->fields = (id)(intptr_t)n; }
static id ac_reset_init(id self, SEL c) { (void)c; pk_action *s = st_new(self, sizeof(pk_action)); s->kind = 4; return self; }
static id ac_remote_init(id self, SEL c, long page, P pt, id url) { (void)c; (void)pt; pk_action *s = st_new(self, sizeof(pk_action)); s->kind = 5; keep(&s->url, url); s->fields = (id)(intptr_t)page; return self; }
static id ac_type(id self, SEL c) {
    (void)c;
    int k = AC(self) ? AC(self)->kind : 0;
    return cx_str(k == 1 ? "URL" : k == 2 ? "GoTo" : k == 3 ? "Named" : k == 4 ? "ResetForm" : k == 5 ? "GoToR" : "");
}
static void ac_dealloc(id self, SEL c) {
    (void)c;
    pk_action *s = AC(self);
    if (s) { keep(&s->url, 0); keep(&s->dest, 0); if (s->kind != 3 && s->kind != 5) keep(&s->fields, 0); free(s); }
    PK(self) = 0;
    super_dealloc(self, &PDFAction_c);
}

/* ---------------------------------------------------------------- PDFBorder */
static id bd_init(id self, SEL c) { (void)c; pk_border *b = st_new(self, sizeof(pk_border)); b->lw = 1; return self; }
static double bd_lw(id self, SEL c) { (void)c; return BD(self) ? BD(self)->lw : 1; }
static void bd_set_lw(id self, SEL c, double v) { (void)c; if (BD(self)) BD(self)->lw = v; }
static long bd_style(id self, SEL c) { (void)c; return BD(self) ? BD(self)->style : 0; }
static void bd_set_style(id self, SEL c, long v) { (void)c; if (BD(self)) BD(self)->style = v; }
static id bd_dash(id self, SEL c) { (void)c; return BD(self) ? BD(self)->dash : 0; }
static void bd_set_dash(id self, SEL c, id v) { (void)c; if (BD(self)) keep(&BD(self)->dash, v); }
static void bd_dealloc(id self, SEL c) { (void)c; if (BD(self)) { keep(&BD(self)->dash, 0); free(BD(self)); } PK(self) = 0; super_dealloc(self, &PDFBorder_c); }

/* ---------------------------------------------------------------- PDFAnnotation */
static id an_new_empty(void) { return MSG(id, MSG0(id, (id)&PDFAnnotation_c, "alloc"), "initWithBounds:forType:withProperties:", ((R){{0, 0}, {0, 0}}), (id)0, (id)0); }
static id an_init(id self, SEL c, R bounds, id type, id props) {
    (void)c;
    pk_annot *a = st_new(self, sizeof(pk_annot));
    a->bounds = bounds;
    keep(&a->type, type);
    keep(&a->props, props);
    a->should_display = a->should_print = 1;
    a->changed = 1;
    if (props) {
        id v = MSG(id, props, "objectForKey:", (id)PDFAnnotationKeyContents);
        if (v) keep(&a->contents, v);
        v = MSG(id, props, "objectForKey:", (id)PDFAnnotationKeyColor);
        if (v) keep(&a->color, v);
    }
    return self;
}
static id an_init_old(id self, SEL c, R bounds) { return an_init(self, c, bounds, 0, 0); }
static void an_mark(id self) {
    pk_annot *a = A(self);
    if (!a) return;
    a->changed = 1;
    if (a->page && G(a->page)) {
        G(a->page)->annots_changed = 1;
        if (G(a->page)->doc && D(G(a->page)->doc)) D(G(a->page)->doc)->modified = 1;
    }
}
static id an_page(id self, SEL c) { (void)c; return A(self) ? A(self)->page : 0; }
static void an_set_page(id self, SEL c, id p) { (void)c; if (A(self)) A(self)->page = p; }
static id an_type(id self, SEL c) { (void)c; pk_annot *a = A(self); if (!a || !a->type) return 0; const char *t = cx_cstr(a->type); return cx_str(t[0] == '/' ? t + 1 : t); }
static void an_set_type(id self, SEL c, id t) { (void)c; if (A(self)) { keep(&A(self)->type, t); an_mark(self); } }
static R an_bounds(id self, SEL c) { (void)c; R z = {{0, 0}, {0, 0}}; return A(self) ? A(self)->bounds : z; }
static void an_set_bounds(id self, SEL c, R b) { (void)c; if (A(self)) { A(self)->bounds = b; an_mark(self); } }
#define AN_OBJ(get, set, field) \
    static id get(id self, SEL c) { (void)c; return A(self) ? A(self)->field : 0; } \
    static void set(id self, SEL c, id v) { (void)c; if (A(self)) { keep(&A(self)->field, v); an_mark(self); } }
AN_OBJ(an_contents, an_set_contents, contents)
AN_OBJ(an_color, an_set_color, color)
AN_OBJ(an_interior, an_set_interior, interior)
AN_OBJ(an_url, an_set_url, url)
AN_OBJ(an_action, an_set_action, action)
AN_OBJ(an_user, an_set_user, user)
AN_OBJ(an_border, an_set_border, border)
AN_OBJ(an_dest, an_set_dest, dest)
AN_OBJ(an_name, an_set_name, name)
AN_OBJ(an_font, an_set_font, font)
AN_OBJ(an_fontcolor, an_set_fontcolor, fontColor)
AN_OBJ(an_popup, an_set_popup, popup)
static BOOL an_display(id self, SEL c) { (void)c; return A(self) && A(self)->should_display; }
static void an_set_display(id self, SEL c, BOOL v) { (void)c; if (A(self)) { A(self)->should_display = v; an_mark(self); } }
static BOOL an_print(id self, SEL c) { (void)c; return A(self) && A(self)->should_print; }
static void an_set_print(id self, SEL c, BOOL v) { (void)c; if (A(self)) { A(self)->should_print = v; an_mark(self); } }
static BOOL an_isopen(id self, SEL c) { (void)c; return A(self) && A(self)->open; }
static void an_set_open(id self, SEL c, BOOL v) { (void)c; if (A(self)) A(self)->open = v; }
static BOOL an_highlighted(id self, SEL c) { (void)c; return A(self) && A(self)->highlighted; }
static void an_set_highlighted(id self, SEL c, BOOL v) { (void)c; if (A(self)) A(self)->highlighted = v; }
static BOOL an_has_ap(id self, SEL c) {
    (void)c;
    pk_annot *a = A(self);
    return a && a->src && a->srcdoc && pdf_dict_get(a->srcdoc, a->src, "AP") && !a->changed;
}
static id an_quad(id self, SEL c) {
    (void)c;
    pk_annot *a = A(self);
    id arr = MSG0(id, cls("NSMutableArray"), "array");
    for (int i = 0; a && i + 1 < a->nquad; i += 2) {
        P p = {a->quad[i] - a->bounds.o.x, a->quad[i + 1] - a->bounds.o.y};
        id pv = MSG(id, cls("NSValue"), "valueWithPoint:", p);
        MSG(void, arr, "addObject:", pv);
    }
    return arr;
}
static void an_set_quad(id self, SEL c, id pts) {
    (void)c;
    pk_annot *a = A(self);
    if (!a) return;
    long n = acount(pts);
    free(a->quad);
    a->quad = calloc((size_t)(n * 2 + 1), sizeof(double));
    a->nquad = 0;
    for (long i = 0; a->quad && i < n; i++) {
        P p = MSG0(P, aget(pts, i), "pointValue");
        a->quad[a->nquad++] = p.x + a->bounds.o.x;
        a->quad[a->nquad++] = p.y + a->bounds.o.y;
    }
    an_mark(self);
}
static id an_ink(id self, SEL c) { (void)c; return A(self) ? A(self)->ink : 0; }
static void an_add_ink(id self, SEL c, id path) {
    (void)c;
    pk_annot *a = A(self);
    if (!a || !path) return;
    if (!a->ink) a->ink = marr();
    MSG(void, a->ink, "addObject:", path);
    an_mark(self);
}
static void an_remove_ink(id self, SEL c, id path) { (void)c; pk_annot *a = A(self); if (a && a->ink) { MSG(void, a->ink, "removeObject:", path); an_mark(self); } }
/* valores pela chave (/Contents, /C, /Rect...) */
static id an_value(id self, SEL c, id key) {
    (void)c;
    pk_annot *a = A(self);
    if (!a || !key) return 0;
    const char *k = cx_cstr(key);
    if (!strcmp(k, "/Contents")) return a->contents;
    if (!strcmp(k, "/C")) return a->color;
    if (!strcmp(k, "/IC")) return a->interior;
    if (!strcmp(k, "/Subtype")) return a->type;
    if (!strcmp(k, "/T")) return a->user;
    if (!strcmp(k, "/NM")) return a->name;
    if (!strcmp(k, "/Rect")) return MSG(id, cls("NSValue"), "valueWithRect:", a->bounds);
    if (!strcmp(k, "/A")) return a->action;
    if (a->props) return MSG(id, a->props, "objectForKey:", key);
    return 0;
}
static BOOL an_set_value(id self, SEL c, id v, id key) {
    (void)c;
    pk_annot *a = A(self);
    if (!a || !key) return NO;
    const char *k = cx_cstr(key);
    if (!strcmp(k, "/Contents")) keep(&a->contents, v);
    else if (!strcmp(k, "/C")) keep(&a->color, v);
    else if (!strcmp(k, "/IC")) keep(&a->interior, v);
    else if (!strcmp(k, "/Subtype")) keep(&a->type, v);
    else if (!strcmp(k, "/T")) keep(&a->user, v);
    else if (!strcmp(k, "/NM")) keep(&a->name, v);
    else if (!strcmp(k, "/A")) keep(&a->action, v);
    else if (!strcmp(k, "/Rect") && v) a->bounds = MSG0(R, v, "rectValue");
    else {
        if (!a->props) a->props = objc_retain(MSG0(id, cls("NSMutableDictionary"), "dictionary"));
        else if (!MSG(BOOL, a->props, "isKindOfClass:", cls("NSMutableDictionary"))) keep(&a->props, MSG0(id, a->props, "mutableCopy"));
        if (v) MSG(void, a->props, "setObject:forKey:", v, key); else MSG(void, a->props, "removeObjectForKey:", key);
    }
    an_mark(self);
    return YES;
}
static BOOL an_set_bool(id self, SEL c, BOOL v, id key) { return an_set_value(self, c, MSG(id, cls("NSNumber"), "numberWithBool:", v), key); }
static BOOL an_set_rect(id self, SEL c, R v, id key) { return an_set_value(self, c, MSG(id, cls("NSValue"), "valueWithRect:", v), key); }
static void an_remove_value(id self, SEL c, id key) { an_set_value(self, c, 0, key); }
static id an_annot_keys(id self, SEL c) {
    (void)c;
    pk_annot *a = A(self);
    id d = MSG0(id, cls("NSMutableDictionary"), "dictionary");
    if (!a) return d;
    if (a->props) MSG(void, d, "addEntriesFromDictionary:", a->props);
    if (a->type) MSG(void, d, "setObject:forKey:", a->type, (id)PDFAnnotationKeySubtype);
    if (a->contents) MSG(void, d, "setObject:forKey:", a->contents, (id)PDFAnnotationKeyContents);
    if (a->color) MSG(void, d, "setObject:forKey:", a->color, (id)PDFAnnotationKeyColor);
    id rv = MSG(id, cls("NSValue"), "valueWithRect:", a->bounds);
    MSG(void, d, "setObject:forKey:", rv, (id)PDFAnnotationKeyRect);
    return d;
}
static void rgba_of(id color, double *rgba) {
    rgba[0] = rgba[1] = rgba[2] = 0; rgba[3] = 1;
    if (!color) return;
    id rgb = MSG(id, color, "colorUsingColorSpaceName:", cx_str("NSCalibratedRGBColorSpace"));
    if (!rgb) rgb = color;
    rgba[0] = MSG0(double, rgb, "redComponent"); rgba[1] = MSG0(double, rgb, "greenComponent");
    rgba[2] = MSG0(double, rgb, "blueComponent"); rgba[3] = MSG0(double, rgb, "alphaComponent");
}
static const char *subtype_c(pk_annot *a) {
    const char *t = a->type ? cx_cstr(a->type) : "Text";
    return t[0] == '/' ? t + 1 : t;
}
/* aparência das anotações feitas ou mudadas aqui (as de origem sem mudança usam o /AP do PDF) */
static void an_draw(id self, SEL c, long box, void *ctx) {
    (void)c; (void)box;
    pk_annot *a = A(self);
    if (!a || !ctx || !a->should_display) return;
    if (a->src && !a->changed && a->page && G(a->page)) { cx_pdf_draw_annotation(ctx, G(a->page)->cg, a->src); return; }
    const char *t = subtype_c(a);
    double col[4], ic[4];
    rgba_of(a->color ? a->color : 0, col);
    if (!a->color) { col[0] = 1; col[1] = 0.85; col[2] = 0; col[3] = 1; }
    double lw = a->border && BD(a->border) ? BD(a->border)->lw : 1;
    R b = a->bounds;
    CGContextSaveGState(ctx);
    if (!strcmp(t, "Highlight") || !strcmp(t, "Underline") || !strcmp(t, "StrikeOut")) {
        int nq = a->nquad / 8;
        for (int q = 0; q < (nq ? nq : 1); q++) {
            R r = b;
            if (nq) {
                double *v = a->quad + q * 8, x0 = v[0], x1 = v[0], y0 = v[1], y1 = v[1];
                for (int k = 1; k < 4; k++) { if (v[2 * k] < x0) x0 = v[2 * k]; if (v[2 * k] > x1) x1 = v[2 * k]; if (v[2 * k + 1] < y0) y0 = v[2 * k + 1]; if (v[2 * k + 1] > y1) y1 = v[2 * k + 1]; }
                r.o.x = x0; r.o.y = y0; r.s.w = x1 - x0; r.s.h = y1 - y0;
            }
            if (!strcmp(t, "Highlight")) {
                CGContextSetBlendMode(ctx, 1);              /* multiply, como o marca-texto */
                CGContextSetRGBFillColor(ctx, col[0], col[1], col[2], col[3]);
                CGContextFillRect(ctx, r);
            } else {
                double y = !strcmp(t, "Underline") ? r.o.y + r.s.h * 0.1 : r.o.y + r.s.h * 0.5;
                CGContextSetRGBStrokeColor(ctx, col[0], col[1], col[2], col[3]);
                CGContextSetLineWidth(ctx, r.s.h > 20 ? r.s.h / 14 : 1.2);
                CGContextBeginPath(ctx);
                CGContextMoveToPoint(ctx, r.o.x, y);
                CGContextAddLineToPoint(ctx, r.o.x + r.s.w, y);
                CGContextStrokePath(ctx);
            }
        }
    } else if (!strcmp(t, "Square") || !strcmp(t, "Circle")) {
        R in = {{b.o.x + lw / 2, b.o.y + lw / 2}, {b.s.w - lw, b.s.h - lw}};
        if (a->interior) {
            rgba_of(a->interior, ic);
            CGContextSetRGBFillColor(ctx, ic[0], ic[1], ic[2], ic[3]);
            CGContextBeginPath(ctx);
            if (t[0] == 'C') CGContextAddEllipseInRect(ctx, in); else { CGContextFillRect(ctx, in); }
            if (t[0] == 'C') CGContextFillPath(ctx);
        }
        CGContextSetRGBStrokeColor(ctx, col[0], col[1], col[2], col[3]);
        CGContextSetLineWidth(ctx, lw);
        CGContextBeginPath(ctx);
        if (t[0] == 'C') { CGContextAddEllipseInRect(ctx, in); CGContextStrokePath(ctx); }
        else CGContextStrokeRect(ctx, in);
    } else if (!strcmp(t, "Line")) {
        CGContextSetRGBStrokeColor(ctx, col[0], col[1], col[2], col[3]);
        CGContextSetLineWidth(ctx, lw);
        CGContextBeginPath(ctx);
        CGContextMoveToPoint(ctx, b.o.x, b.o.y);
        CGContextAddLineToPoint(ctx, b.o.x + b.s.w, b.o.y + b.s.h);
        CGContextStrokePath(ctx);
    } else if (!strcmp(t, "Ink")) {
        CGContextSetRGBStrokeColor(ctx, col[0], col[1], col[2], col[3]);
        CGContextSetLineWidth(ctx, lw);
        for (long i = 0; i < acount(a->ink); i++) {
            id bp = aget(a->ink, i);
            long n = MSG0(long, bp, "elementCount");
            CGContextBeginPath(ctx);
            for (long k = 0; k < n; k++) {
                P pts[3];
                long ty = MSG(long, bp, "elementAtIndex:associatedPoints:", k, pts);
                if (ty == 0) CGContextMoveToPoint(ctx, b.o.x + pts[0].x, b.o.y + pts[0].y);
                else CGContextAddLineToPoint(ctx, b.o.x + pts[ty == 2 ? 2 : 0].x, b.o.y + pts[ty == 2 ? 2 : 0].y);
            }
            CGContextStrokePath(ctx);
        }
    } else if (!strcmp(t, "Text")) {
        /* nota: o ícone de balão (quadrado com dobra) */
        R r = {{b.o.x, b.o.y + b.s.h - 20}, {20, 20}};
        if (b.s.w < 20 || b.s.h < 20) r = b;
        CGContextSetRGBFillColor(ctx, col[0], col[1], col[2], 1);
        CGContextFillRect(ctx, r);
        CGContextSetRGBStrokeColor(ctx, 0, 0, 0, 0.6);
        CGContextSetLineWidth(ctx, 0.75);
        CGContextStrokeRect(ctx, r);
        for (int k = 1; k <= 3; k++) {
            CGContextBeginPath(ctx);
            CGContextMoveToPoint(ctx, r.o.x + 4, r.o.y + r.s.h - 4 - k * 3.5);
            CGContextAddLineToPoint(ctx, r.o.x + r.s.w - 4, r.o.y + r.s.h - 4 - k * 3.5);
            CGContextStrokePath(ctx);
        }
    } else if (!strcmp(t, "FreeText") || !strcmp(t, "Widget") || !strcmp(t, "Stamp")) {
        if (a->color && strcmp(t, "FreeText")) { CGContextSetRGBFillColor(ctx, col[0], col[1], col[2], col[3] * 0.25); CGContextFillRect(ctx, b); }
        CGContextSetRGBStrokeColor(ctx, col[0], col[1], col[2], col[3]);
        CGContextSetLineWidth(ctx, lw);
        CGContextStrokeRect(ctx, b);
        if (a->contents) {
            id font = a->font ? a->font : MSG(id, cls("NSFont"), "systemFontOfSize:", 12.0);
            id attrs = MSG0(id, cls("NSMutableDictionary"), "dictionary");
            MSG(void, attrs, "setObject:forKey:", font, cx_str("NSFont"));
            if (a->fontColor) MSG(void, attrs, "setObject:forKey:", a->fontColor, cx_str("NSColor"));
            id gctx = MSG(id, cls("NSGraphicsContext"), "graphicsContextWithCGContext:flipped:", (id)ctx, (BOOL)NO);
            if (gctx) {
                MSG0(void, cls("NSGraphicsContext"), "saveGraphicsState");
                MSG(void, cls("NSGraphicsContext"), "setCurrentContext:", gctx);
                R tr = {{b.o.x + 2, b.o.y + 2}, {b.s.w - 4, b.s.h - 4}};
                MSG(void, a->contents, "drawInRect:withAttributes:", tr, attrs);
                MSG0(void, cls("NSGraphicsContext"), "restoreGraphicsState");
            }
        }
    }
    CGContextRestoreGState(ctx);
}
/* dicionário do PDF da anotação (para a regravação); @P<n>@ = página n do arquivo novo */
static void pb_add(char **b, size_t *n, size_t *cap, const char *s) {
    size_t k = strlen(s);
    if (*n + k + 1 > *cap) { size_t c = (*cap ? *cap * 2 : 512); while (c < *n + k + 1) c *= 2; char *nb = realloc(*b, c); if (!nb) return; *b = nb; *cap = c; }
    memcpy(*b + *n, s, k + 1);
    *n += k;
}
static void pb_num(char **b, size_t *n, size_t *cap, double v) {
    char t[40];
    long long q = (long long)(v * 1000 + (v < 0 ? -0.5 : 0.5));
    char *p = t;
    if (q < 0) { *p++ = '-'; q = -q; }
    long long ip = q / 1000, f = q % 1000;
    char d[24]; int k = 0;
    do { d[k++] = (char)('0' + ip % 10); ip /= 10; } while (ip);
    while (k) *p++ = d[--k];
    if (f) { *p++ = '.'; *p++ = (char)('0' + f / 100); *p++ = (char)('0' + f / 10 % 10); *p++ = (char)('0' + f % 10); while (p[-1] == '0') p--; }
    *p++ = ' ';
    *p = 0;
    pb_add(b, n, cap, t);
}
static void pb_text(char **b, size_t *n, size_t *cap, id s) {
    u16 *u;
    long k = str_utf16(s, &u);
    static const char hx[] = "0123456789ABCDEF";
    pb_add(b, n, cap, "<FEFF");
    for (long i = 0; i < k; i++) { char h[5] = {hx[u[i] >> 12], hx[u[i] >> 8 & 15], hx[u[i] >> 4 & 15], hx[u[i] & 15], 0}; pb_add(b, n, cap, h); }
    pb_add(b, n, cap, "> ");
    free(u);
}
static char *an_pdf_dict(id self, id doc) {
    pk_annot *a = A(self);
    char *b = 0;
    size_t n = 0, cap = 0;
    pb_add(&b, &n, &cap, "<< /Type /Annot /Subtype /");
    pb_add(&b, &n, &cap, subtype_c(a));
    pb_add(&b, &n, &cap, " /Rect [");
    pb_num(&b, &n, &cap, a->bounds.o.x); pb_num(&b, &n, &cap, a->bounds.o.y);
    pb_num(&b, &n, &cap, a->bounds.o.x + a->bounds.s.w); pb_num(&b, &n, &cap, a->bounds.o.y + a->bounds.s.h);
    pb_add(&b, &n, &cap, "]");
    if (a->contents) { pb_add(&b, &n, &cap, " /Contents "); pb_text(&b, &n, &cap, a->contents); }
    if (a->user) { pb_add(&b, &n, &cap, " /T "); pb_text(&b, &n, &cap, a->user); }
    if (a->name) { pb_add(&b, &n, &cap, " /NM "); pb_text(&b, &n, &cap, a->name); }
    double col[4];
    if (a->color) { rgba_of(a->color, col); pb_add(&b, &n, &cap, " /C ["); for (int i = 0; i < 3; i++) pb_num(&b, &n, &cap, col[i]); pb_add(&b, &n, &cap, "]"); if (col[3] < 1) { pb_add(&b, &n, &cap, " /CA "); pb_num(&b, &n, &cap, col[3]); } }
    if (a->interior) { rgba_of(a->interior, col); pb_add(&b, &n, &cap, " /IC ["); for (int i = 0; i < 3; i++) pb_num(&b, &n, &cap, col[i]); pb_add(&b, &n, &cap, "]"); }
    int flags = (a->should_print ? 4 : 0) | (a->should_display ? 0 : 2);
    pb_add(&b, &n, &cap, " /F "); pb_num(&b, &n, &cap, flags);
    if (a->border && BD(a->border)) { pb_add(&b, &n, &cap, " /Border [0 0 "); pb_num(&b, &n, &cap, BD(a->border)->lw); pb_add(&b, &n, &cap, "]"); }
    else if (!strcmp(subtype_c(a), "Link")) pb_add(&b, &n, &cap, " /Border [0 0 0]");
    if (a->nquad) { pb_add(&b, &n, &cap, " /QuadPoints ["); for (int i = 0; i < a->nquad; i++) pb_num(&b, &n, &cap, a->quad[i]); pb_add(&b, &n, &cap, "]"); }
    if (a->open) pb_add(&b, &n, &cap, " /Open true");
    if (acount(a->ink)) {
        pb_add(&b, &n, &cap, " /InkList [");
        for (long i = 0; i < acount(a->ink); i++) {
            id bp = aget(a->ink, i);
            long ne = MSG0(long, bp, "elementCount");
            pb_add(&b, &n, &cap, "[");
            for (long k = 0; k < ne; k++) { P pts[3]; long ty = MSG(long, bp, "elementAtIndex:associatedPoints:", k, pts); P q = pts[ty == 2 ? 2 : 0]; pb_num(&b, &n, &cap, a->bounds.o.x + q.x); pb_num(&b, &n, &cap, a->bounds.o.y + q.y); }
            pb_add(&b, &n, &cap, "]");
        }
        pb_add(&b, &n, &cap, "]");
    }
    /* destino ou ação */
    id url = a->url;
    id dest = a->dest;
    if (a->action && AC(a->action)) {
        if (AC(a->action)->kind == 1) url = AC(a->action)->url;
        else if (AC(a->action)->kind == 2) dest = AC(a->action)->dest;
    }
    if (url) {
        id s = MSG0(id, url, "absoluteString");
        const char *u = s ? cx_cstr(s) : "";
        pb_add(&b, &n, &cap, " /A << /S /URI /URI (");
        for (const char *p = u; *p; p++) { char e[3] = {0}; if (*p == '(' || *p == ')' || *p == '\\') { e[0] = '\\'; e[1] = *p; } else e[0] = *p; pb_add(&b, &n, &cap, e); }
        pb_add(&b, &n, &cap, ") >>");
    } else if (dest && DS(dest)) {
        long pi = doc && DS(dest)->page ? MSG(long, doc, "indexForPage:", DS(dest)->page) : -1;
        char t[24];
        snprintf(t, sizeof t, "@P%ld@", pi);
        pb_add(&b, &n, &cap, " /Dest ["); pb_add(&b, &n, &cap, t); pb_add(&b, &n, &cap, " /XYZ ");
        pb_num(&b, &n, &cap, DS(dest)->pt.x); pb_num(&b, &n, &cap, DS(dest)->pt.y); pb_num(&b, &n, &cap, DS(dest)->zoom);
        pb_add(&b, &n, &cap, "]");
    }
    pb_add(&b, &n, &cap, " >>");
    return b;
}
static void an_dealloc(id self, SEL c) {
    (void)c;
    pk_annot *a = A(self);
    if (a) {
        id *f[] = {&a->type, &a->contents, &a->color, &a->interior, &a->url, &a->action, &a->user, &a->border, &a->dest, &a->name, &a->props, &a->font, &a->fontColor, &a->popup, &a->ink};
        for (size_t i = 0; i < sizeof f / sizeof f[0]; i++) keep(f[i], 0);
        free(a->quad);
        free(a);
    }
    PK(self) = 0;
    super_dealloc(self, &PDFAnnotation_c);
}

/* ---------------------------------------------------------------- PDFPage */
static void page_text(pk_page *g) {
    if (g->text_loaded) return;
    g->text_loaded = 1;
    g->ntext = g->cg ? cx_pdf_page_text(g->cg, &g->text, &g->tbox) : 0;
}
static id pg_init(id self, SEL c) { (void)c; pk_page *g = st_new(self, sizeof(pk_page)); g->rotation = -1; g->display_annots = 1; return self; }
static id page_wrap(id doc, void *cgpage) {
    id p = MSG0(id, MSG0(id, (id)&PDFPage_c, "alloc"), "init");
    pk_page *g = G(p);
    g->doc = doc;
    g->cg = CGPDFPageRetain(cgpage);
    return p;
}
static void color_of_pdf(pdf_doc *pd, pdf_obj *arr, id *out) {
    double v[4] = {0, 0, 0, 1};
    if (!arr || arr->type != PO_ARRAY || !arr->n) return;
    for (u32 i = 0; i < arr->n && i < 4; i++) pdf_num(pdf_resolve(pd, arr->v.arr[i]), &v[i]);
    double r = v[0], gg = v[0], b = v[0];
    if (arr->n == 3) { gg = v[1]; b = v[2]; }
    else if (arr->n == 4) { r = (1 - v[0]) * (1 - v[3]); gg = (1 - v[1]) * (1 - v[3]); b = (1 - v[2]) * (1 - v[3]); }
    keep(out, MSG(id, cls("NSColor"), "colorWithCalibratedRed:green:blue:alpha:", r, gg, b, 1.0));
}
static id pdf_text_str(pdf_obj *s) {
    if (!s || s->type != PO_STRING) return 0;
    u16 *u;
    int k = cx_pdf_text_utf16(s->v.str, s->n, &u);
    if (k < 0) return 0;
    id r = nsstr_utf16(u, k);
    free(u);
    return r;
}
/* anotações do PDF de origem, na primeira vez que alguém pergunta */
static void page_load_annots(id self) {
    pk_page *g = G(self);
    if (!g || g->annots_loaded) return;
    g->annots_loaded = 1;
    g->annots = marr();
    if (!g->cg) return;
    pdf_doc *pd = cx_pdf_page_doc(g->cg);
    pdf_obj *an = pd ? pdf_dict_get(pd, CGPDFPageGetDictionary(g->cg), "Annots") : 0;
    for (u32 i = 0; an && an->type == PO_ARRAY && i < an->n; i++) {
        pdf_obj *d = pdf_resolve(pd, an->v.arr[i]);
        if (!d || d->type != PO_DICT) continue;
        const char *st = pdf_name(pdf_dict_get(pd, d, "Subtype"));
        double rc[4] = {0, 0, 0, 0};
        pdf_obj *ra = pdf_dict_get(pd, d, "Rect");
        for (u32 k = 0; ra && ra->type == PO_ARRAY && k < 4 && k < ra->n; k++) pdf_num(pdf_resolve(pd, ra->v.arr[k]), &rc[k]);
        R b = {{rc[0] < rc[2] ? rc[0] : rc[2], rc[1] < rc[3] ? rc[1] : rc[3]}, {rc[2] > rc[0] ? rc[2] - rc[0] : rc[0] - rc[2], rc[3] > rc[1] ? rc[3] - rc[1] : rc[1] - rc[3]}};
        char tn[64];
        snprintf(tn, sizeof tn, "/%s", st ? st : "Unknown");
        id a = MSG(id, MSG0(id, (id)&PDFAnnotation_c, "alloc"), "initWithBounds:forType:withProperties:", b, cx_str(tn), (id)0);
        pk_annot *s = A(a);
        s->page = self;
        s->src = d; s->srcdoc = pd;
        keep(&s->contents, pdf_text_str(pdf_dict_get(pd, d, "Contents")));
        keep(&s->user, pdf_text_str(pdf_dict_get(pd, d, "T")));
        keep(&s->name, pdf_text_str(pdf_dict_get(pd, d, "NM")));
        color_of_pdf(pd, pdf_dict_get(pd, d, "C"), &s->color);
        color_of_pdf(pd, pdf_dict_get(pd, d, "IC"), &s->interior);
        double fl = 0;
        pdf_num(pdf_dict_get(pd, d, "F"), &fl);
        s->should_display = !((int)fl & 2);
        s->should_print = ((int)fl & 4) != 0;
        pdf_obj *q = pdf_dict_get(pd, d, "QuadPoints");
        if (q && q->type == PO_ARRAY && q->n && (s->quad = calloc(q->n, sizeof(double)))) {
            for (u32 k = 0; k < q->n; k++) pdf_num(pdf_resolve(pd, q->v.arr[k]), &s->quad[k]);
            s->nquad = (int)q->n;
        }
        pdf_obj *act = pdf_dict_get(pd, d, "A");
        const char *as = act ? pdf_name(pdf_dict_get(pd, act, "S")) : 0;
        if (as && !strcmp(as, "URI")) {
            pdf_obj *u = pdf_dict_get(pd, act, "URI");
            if (u && u->type == PO_STRING) {
                char *t = malloc(u->n + 1);
                if (t) { memcpy(t, u->v.str, u->n); t[u->n] = 0; keep(&s->url, MSG(id, cls("NSURL"), "URLWithString:", cx_str(t))); free(t); }
                if (s->url) { id ac = MSG(id, MSG0(id, (id)&PDFActionURL_c, "alloc"), "initWithURL:", s->url); keep(&s->action, ac); objc_release(ac); }
            }
        }
        pdf_obj *dest = pdf_dict_get(pd, d, "Dest");
        if (!dest && as && !strcmp(as, "GoTo")) dest = pdf_dict_get(pd, act, "D");
        if (dest && g->doc) {
            int pi = cx_pdf_dest_page(pd, dest) - 1;
            if (pi >= 0 && pi < acount(D(g->doc)->pages)) {
                pdf_obj *arr = cx_pdf_dest_array(pd, dest);
                double x = 0, y = 0;
                if (arr && arr->type == PO_ARRAY && arr->n >= 4) { pdf_num(pdf_resolve(pd, arr->v.arr[2]), &x); pdf_num(pdf_resolve(pd, arr->v.arr[3]), &y); }
                id dd = dest_new(aget(D(g->doc)->pages, pi), (P){x, y});
                keep(&s->dest, dd);
                id ac = MSG(id, MSG0(id, (id)&PDFActionGoTo_c, "alloc"), "initWithDestination:", dd);
                if (!s->action) keep(&s->action, ac);
                objc_release(ac);
                objc_release(dd);
            }
        }
        s->changed = 0;
        MSG(void, g->annots, "addObject:", a);
        objc_release(a);
    }
}
static id pg_document(id self, SEL c) { (void)c; return G(self) ? G(self)->doc : 0; }
static void *pg_ref(id self, SEL c) { (void)c; return G(self) ? G(self)->cg : 0; }
static id pg_label(id self, SEL c) {
    (void)c;
    pk_page *g = G(self);
    long i = g && g->doc ? MSG(long, g->doc, "indexForPage:", self) : 0;
    /* /PageLabels: decimal, romano ou letra com prefixo e início */
    if (g && g->cg) {
        pdf_doc *pd = cx_pdf_page_doc(g->cg);
        pdf_obj *pl = pd ? pdf_dict_get(pd, pdf_dict_get(pd, pdf_catalog(pd), "PageLabels"), "Nums") : 0;
        long start = -1;
        pdf_obj *lab = 0;
        for (u32 k = 0; pl && pl->type == PO_ARRAY && k + 1 < pl->n; k += 2) {
            double v;
            if (pdf_num(pdf_resolve(pd, pl->v.arr[k]), &v) && v <= i) { start = (long)v; lab = pdf_resolve(pd, pl->v.arr[k + 1]); }
        }
        if (lab) {
            const char *s = pdf_name(pdf_dict_get(pd, lab, "S"));
            double st = 1;
            pdf_num(pdf_dict_get(pd, lab, "St"), &st);
            long num = (long)st + (i - start);
            char out[96] = {0};
            pdf_obj *pf = pdf_dict_get(pd, lab, "P");
            size_t k = 0;
            if (pf && pf->type == PO_STRING) for (u32 q = 0; q < pf->n && k < 40; q++) out[k++] = (char)pf->v.str[q];
            if (s && (s[0] == 'r' || s[0] == 'R')) {
                static const char *const rn[13] = {"m", "cm", "d", "cd", "c", "xc", "l", "xl", "x", "ix", "v", "iv", "i"};
                static const int rv[13] = {1000, 900, 500, 400, 100, 90, 50, 40, 10, 9, 5, 4, 1};
                long v = num;
                for (int q = 0; q < 13 && k < 90; q++) while (v >= rv[q] && k < 90) { for (const char *p = rn[q]; *p; p++) out[k++] = s[0] == 'R' ? (char)(*p - 32) : *p; v -= rv[q]; }
            } else if (s && (s[0] == 'a' || s[0] == 'A')) {
                long rep = (num - 1) / 26 + 1;
                for (long q = 0; q < rep && k < 90; q++) out[k++] = (char)((s[0] == 'A' ? 'A' : 'a') + (num - 1) % 26);
            } else if (s) { char t[24]; snprintf(t, sizeof t, "%ld", num); for (char *p = t; *p && k < 90; p++) out[k++] = *p; }
            return cx_str(out);
        }
    }
    char t[24];
    snprintf(t, sizeof t, "%ld", i + 1);
    return cx_str(t);
}
static int box_index(long box) { return box >= 0 && box <= 4 ? (int)box : 0; }
static R pg_bounds(id self, SEL c, long box) {
    (void)c;
    pk_page *g = G(self);
    R z = {{0, 0}, {612, 792}};
    if (!g) return z;
    int b = box_index(box);
    if (g->hasbox[b]) return g->box[b];
    if (b != 0 && g->hasbox[0] && !g->cg) return g->box[0];
    return g->cg ? CGPDFPageGetBoxRect(g->cg, b) : z;
}
static void pg_set_bounds(id self, SEL c, R r, long box) {
    (void)c;
    pk_page *g = G(self);
    if (!g) return;
    int b = box_index(box);
    g->box[b] = r; g->hasbox[b] = 1;
    if (g->doc && D(g->doc)) D(g->doc)->modified = 1;
}
static long pg_rotation(id self, SEL c) { (void)c; pk_page *g = G(self); return !g ? 0 : g->rotation >= 0 ? g->rotation : g->cg ? CGPDFPageGetRotationAngle(g->cg) : 0; }
static void pg_set_rotation(id self, SEL c, long r) {
    (void)c;
    pk_page *g = G(self);
    if (!g) return;
    g->rotation = (int)(((r % 360) + 360) % 360 / 90 * 90);
    if (g->doc && D(g->doc)) D(g->doc)->modified = 1;
}
static id pg_annotations(id self, SEL c) { (void)c; page_load_annots(self); return G(self) ? MSG0(id, G(self)->annots, "copy") : 0; }
static void pg_add_annot(id self, SEL c, id a) {
    (void)c;
    pk_page *g = G(self);
    if (!g || !a) return;
    page_load_annots(self);
    if (A(a)) A(a)->page = self;
    MSG(void, g->annots, "addObject:", a);
    g->annots_changed = 1;
    if (A(a)) A(a)->changed = 1;
    if (g->doc && D(g->doc)) D(g->doc)->modified = 1;
}
static void pg_remove_annot(id self, SEL c, id a) {
    (void)c;
    pk_page *g = G(self);
    if (!g || !a) return;
    page_load_annots(self);
    MSG(void, g->annots, "removeObjectIdenticalTo:", a);
    if (A(a)) A(a)->page = 0;
    g->annots_changed = 1;
    if (g->doc && D(g->doc)) D(g->doc)->modified = 1;
}
static id pg_annot_at(id self, SEL c, P p) {
    (void)c;
    page_load_annots(self);
    pk_page *g = G(self);
    for (long i = acount(g ? g->annots : 0) - 1; i >= 0; i--) {
        pk_annot *a = A(aget(g->annots, i));
        if (!a || !a->should_display) continue;
        const char *t = subtype_c(a);
        if (!strcmp(t, "Popup")) continue;
        R b = a->bounds;
        if (p.x >= b.o.x && p.x <= b.o.x + b.s.w && p.y >= b.o.y && p.y <= b.o.y + b.s.h) return aget(g->annots, i);
    }
    return 0;
}
static BOOL pg_disp_annots(id self, SEL c) { (void)c; return G(self) && G(self)->display_annots; }
static void pg_set_disp_annots(id self, SEL c, BOOL v) { (void)c; if (G(self)) G(self)->display_annots = v; }
/* espaço da caixa (com rotação) → espaço padrão da página */
static void pg_transform(id self, SEL c, void *ctx, long box) {
    (void)c;
    if (!ctx) return;
    R b = pg_bounds(self, 0, box);
    long rot = pg_rotation(self, 0);
    if (rot == 90) { T t = {0, -1, 1, 0, 0, b.s.w}; CGContextConcatCTM(ctx, t); }
    else if (rot == 180) { T t = {-1, 0, 0, -1, b.s.w, b.s.h}; CGContextConcatCTM(ctx, t); }
    else if (rot == 270) { T t = {0, 1, -1, 0, b.s.h, 0}; CGContextConcatCTM(ctx, t); }
    CGContextTranslateCTM(ctx, -b.o.x, -b.o.y);
}
static void pg_draw_ctx(id self, SEL c, long box, void *ctx) {
    (void)c;
    pk_page *g = G(self);
    if (!g || !ctx) return;
    CGContextSaveGState(ctx);
    pg_transform(self, 0, ctx, box);
    R b = pg_bounds(self, 0, box);
    CGContextClipToRect(ctx, b);
    CGContextSetRGBFillColor(ctx, 1, 1, 1, 1);
    CGContextFillRect(ctx, b);
    if (g->cg) cx_pdf_draw_page_content(ctx, g->cg);
    if (g->display_annots) {
        page_load_annots(self);
        for (long i = 0; i < acount(g->annots); i++) MSG(void, aget(g->annots, i), "drawWithBox:inContext:", box, ctx);
    }
    CGContextRestoreGState(ctx);
}
static void *current_cgctx(void) {
    id gc = MSG0(id, cls("NSGraphicsContext"), "currentContext");
    return gc ? MSG0(void *, gc, "CGContext") : 0;
}
static void pg_draw_box(id self, SEL c, long box) { pg_draw_ctx(self, c, box, current_cgctx()); }
/* página inteira num bitmap: o PDFView e as miniaturas usam isto (escala em pixels por ponto) */
void *cx_pdfkit_render_page(id page, long box, double scale, int *pw, int *ph) {
    R b = pg_bounds(page, 0, box);
    long rot = pg_rotation(page, 0);
    double W = (rot == 90 || rot == 270) ? b.s.h : b.s.w, H = (rot == 90 || rot == 270) ? b.s.w : b.s.h;
    int w = (int)(W * scale + 0.5), h = (int)(H * scale + 0.5);
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if ((double)w * h > 64e6) return 0;
    void *cs = CGColorSpaceCreateDeviceRGB();
    void *ctx = CGBitmapContextCreate(0, (size_t)w, (size_t)h, 8, (size_t)w * 4, cs, 2 | 0x2000);
    CGColorSpaceRelease(cs);
    if (!ctx) return 0;
    CGContextScaleCTM(ctx, scale, scale);
    pg_draw_ctx(page, 0, box, ctx);
    void *img = CGBitmapContextCreateImage(ctx);
    CGContextRelease(ctx);
    if (pw) *pw = w;
    if (ph) *ph = h;
    return img;
}
static id pg_thumbnail(id self, SEL c, S size, long box) {
    (void)c;
    R b = pg_bounds(self, 0, box);
    long rot = pg_rotation(self, 0);
    double W = (rot == 90 || rot == 270) ? b.s.h : b.s.w, H = (rot == 90 || rot == 270) ? b.s.w : b.s.h;
    double sc = W > 0 && H > 0 ? (size.w / W < size.h / H ? size.w / W : size.h / H) : 1;
    int w, h;
    void *img = cx_pdfkit_render_page(self, box, sc, &w, &h);
    if (!img) return 0;
    id ni = MSG(id, MSG0(id, cls("NSImage"), "alloc"), "initWithCGImage:size:", (id)img, ((S){w, h}));
    CGImageRelease(img);
    return objc_autorelease(ni);
}
static long pg_nchars(id self, SEL c) { (void)c; pk_page *g = G(self); if (!g) return 0; page_text(g); return g->ntext; }
static id pg_string(id self, SEL c) { (void)c; pk_page *g = G(self); if (!g) return 0; page_text(g); return nsstr_utf16(g->text, g->ntext); }
static id pg_attr_string(id self, SEL c) {
    id s = pg_string(self, c);
    return s ? objc_autorelease(MSG(id, MSG0(id, cls("NSAttributedString"), "alloc"), "initWithString:", s)) : 0;
}
static R pg_char_bounds(id self, SEL c, long i) {
    (void)c;
    pk_page *g = G(self);
    R z = {{0, 0}, {0, 0}};
    if (!g) return z;
    page_text(g);
    return i >= 0 && i < g->ntext ? g->tbox[i] : z;
}
static long char_at(pk_page *g, P p, int nearest) {
    page_text(g);
    long best = -1;
    double bd = 1e30;
    for (long i = 0; i < g->ntext; i++) {
        R b = g->tbox[i];
        if (g->text[i] == '\n') continue;
        if (p.x >= b.o.x && p.x <= b.o.x + b.s.w && p.y >= b.o.y && p.y <= b.o.y + b.s.h) return i;
        if (nearest) {
            double dx = p.x < b.o.x ? b.o.x - p.x : p.x > b.o.x + b.s.w ? p.x - b.o.x - b.s.w : 0;
            double dy = p.y < b.o.y ? b.o.y - p.y : p.y > b.o.y + b.s.h ? p.y - b.o.y - b.s.h : 0;
            double d = dx * dx + dy * dy * 4;
            if (d < bd) { bd = d; best = i; }
        }
    }
    return best;
}
static long pg_char_index(id self, SEL c, P p) { (void)c; pk_page *g = G(self); return g ? char_at(g, p, 0) : -1; }
static id sel_new(id doc) { return MSG(id, MSG0(id, (id)&PDFSelection_c, "alloc"), "initWithDocument:", doc); }
static void sel_add_range(id s, id page, long loc, long len) {
    pk_sel *q = SL(s);
    if (!q || len <= 0) return;
    pk_range *n = realloc(q->r, (size_t)(q->n + 1) * sizeof *n);
    if (!n) return;
    q->r = n;
    q->r[q->n].page = objc_retain(page);
    q->r[q->n].loc = loc; q->r[q->n].len = len;
    q->n++;
}
static id pg_sel_range(id self, SEL c, struct { unsigned long loc, len; } r) {
    (void)c;
    pk_page *g = G(self);
    if (!g) return 0;
    page_text(g);
    long a = (long)r.loc, b = (long)(r.loc + r.len);
    if (a < 0) a = 0;
    if (b > g->ntext) b = g->ntext;
    if (b <= a) return 0;
    id s = sel_new(g->doc);
    sel_add_range(s, self, a, b - a);
    return objc_autorelease(s);
}
static id pg_sel_rect(id self, SEL c, R r) {
    (void)c;
    pk_page *g = G(self);
    if (!g) return 0;
    page_text(g);
    id s = sel_new(g->doc);
    long run = -1;
    for (long i = 0; i <= g->ntext; i++) {
        int in = 0;
        if (i < g->ntext) {
            R b = g->tbox[i];
            double cx = b.o.x + b.s.w / 2, cy = b.o.y + b.s.h / 2;
            in = cx >= r.o.x && cx <= r.o.x + r.s.w && cy >= r.o.y && cy <= r.o.y + r.s.h;
        }
        if (in && run < 0) run = i;
        if (!in && run >= 0) { sel_add_range(s, self, run, i - run); run = -1; }
    }
    if (!SL(s)->n) { objc_release(s); return 0; }
    return objc_autorelease(s);
}
static id pg_sel_points(id self, SEL c, P a, P b) {
    (void)c;
    pk_page *g = G(self);
    if (!g) return 0;
    long i = char_at(g, a, 1), j = char_at(g, b, 1);
    if (i < 0 || j < 0) return 0;
    if (j < i) { long t = i; i = j; j = t; }
    id s = sel_new(g->doc);
    sel_add_range(s, self, i, j - i + 1);
    return objc_autorelease(s);
}
static id pg_sel_word(id self, SEL c, P p) {
    (void)c;
    pk_page *g = G(self);
    if (!g) return 0;
    long i = char_at(g, p, 1), a, b;
    if (i < 0) return 0;
    pk_word_at(g->text, g->ntext, i, &a, &b);
    id s = sel_new(g->doc);
    sel_add_range(s, self, a, b - a);
    return objc_autorelease(s);
}
static id pg_sel_line(id self, SEL c, P p) {
    (void)c;
    pk_page *g = G(self);
    if (!g) return 0;
    long i = char_at(g, p, 1), a, b;
    if (i < 0) return 0;
    pk_line_at(g->text, g->ntext, i, &a, &b);
    id s = sel_new(g->doc);
    sel_add_range(s, self, a, b - a);
    return objc_autorelease(s);
}
static id pg_data(id self, SEL c) {
    (void)c;
    id d = MSG(id, MSG0(id, (id)&PDFDocument_c, "alloc"), "init");
    MSG(void, d, "insertPage:atIndex:", MSG0(id, self, "copy"), (unsigned long)0);
    id data = MSG0(id, d, "dataRepresentation");
    objc_retain(data);
    objc_release(d);
    return objc_autorelease(data);
}
static id pg_copy(id self, SEL c, void *zone) {
    (void)c; (void)zone;
    pk_page *g = G(self);
    id p = MSG0(id, MSG0(id, (id)&PDFPage_c, "alloc"), "init");
    pk_page *n = G(p);
    if (g) {
        n->cg = g->cg ? CGPDFPageRetain(g->cg) : 0;
        if (g->own_doc) n->own_doc = CGPDFDocumentRetain(g->own_doc);
        n->rotation = g->rotation;
        memcpy(n->hasbox, g->hasbox, sizeof n->hasbox);
        memcpy(n->box, g->box, sizeof n->box);
        n->display_annots = g->display_annots;
        page_load_annots(self);
        n->annots_loaded = 1;
        n->annots = marr();
        for (long i = 0; i < acount(g->annots); i++) {
            id a = aget(g->annots, i);
            MSG(void, n->annots, "addObject:", a);
        }
        n->annots_changed = g->annots_changed;
    }
    return p;
}
/* página a partir de uma imagem: um PDF de uma página feito pelo CGPDFContext */
static id pg_init_image(id self, SEL c, id image) {
    (void)c;
    pk_page *g = st_new(self, sizeof(pk_page));
    g->rotation = -1;
    g->display_annots = 1;
    if (!image) return self;
    S sz = MSG0(S, image, "size");
    R rect = {{0, 0}, sz};
    void *cg = MSG(void *, image, "CGImageForProposedRect:context:hints:", (void *)0, (id)0, (id)0);
    if (!cg || sz.w <= 0 || sz.h <= 0) return self;
    id data = MSG0(id, cls("NSMutableData"), "data");
    void *cons = CGDataConsumerCreateWithCFData(data);
    void *ctx = cons ? CGPDFContextCreate(cons, &rect, 0) : 0;
    if (ctx) {
        CGContextBeginPage(ctx, &rect);
        CGContextDrawImage(ctx, rect, cg);
        CGContextEndPage(ctx);
        CGPDFContextClose(ctx);
        CGContextRelease(ctx);
    }
    if (cons) CGDataConsumerRelease(cons);
    long n = MSG0(long, data, "length");
    u8 *copy = malloc((size_t)n);
    if (!copy) return self;
    memcpy(copy, MSG0(const void *, data, "bytes"), (size_t)n);
    void *prov = CGDataProviderCreateWithData(0, copy, (size_t)n, 0);
    g->own_doc = prov ? CGPDFDocumentCreateWithProvider(prov) : 0;
    if (prov) CGDataProviderRelease(prov);
    if (g->own_doc) g->cg = CGPDFPageRetain(CGPDFDocumentGetPage(g->own_doc, 1));
    return self;
}
static void pg_dealloc(id self, SEL c) {
    (void)c;
    pk_page *g = G(self);
    if (g) {
        if (g->cg) CGPDFPageRelease(g->cg);
        if (g->own_doc) CGPDFDocumentRelease(g->own_doc);
        for (long i = 0; i < acount(g->annots); i++) { pk_annot *a = A(aget(g->annots, i)); if (a && a->page == self) a->page = 0; }
        keep(&g->annots, 0);
        free(g->text); free(g->tbox);
        free(g);
    }
    PK(self) = 0;
    super_dealloc(self, &PDFPage_c);
}

/* ---------------------------------------------------------------- PDFSelection */
static id sl_init(id self, SEL c, id doc) { (void)c; pk_sel *s = st_new(self, sizeof(pk_sel)); s->doc = doc; return self; }
static id sl_pages(id self, SEL c) {
    (void)c;
    pk_sel *s = SL(self);
    id arr = MSG0(id, cls("NSMutableArray"), "array");
    for (int i = 0; s && i < s->n; i++) if (!MSG(BOOL, arr, "containsObject:", s->r[i].page)) MSG(void, arr, "addObject:", s->r[i].page);
    return arr;
}
static id sl_string(id self, SEL c) {
    (void)c;
    pk_sel *s = SL(self);
    if (!s || !s->n) return 0;
    long total = 0;
    for (int i = 0; i < s->n; i++) total += s->r[i].len + 1;
    u16 *buf = malloc((size_t)total * sizeof *buf);
    long k = 0;
    for (int i = 0; buf && i < s->n; i++) {
        pk_page *g = G(s->r[i].page);
        if (!g) continue;
        page_text(g);
        if (i && k && buf[k - 1] != '\n') buf[k++] = (s->r[i].page != s->r[i - 1].page) ? '\n' : ' ';
        for (long j = s->r[i].loc; j < s->r[i].loc + s->r[i].len && j < g->ntext; j++) buf[k++] = g->text[j];
    }
    id r = nsstr_utf16(buf, k);
    free(buf);
    return r;
}
static id sl_attr(id self, SEL c) { id s = sl_string(self, c); return s ? objc_autorelease(MSG(id, MSG0(id, cls("NSAttributedString"), "alloc"), "initWithString:", s)) : 0; }
static R sl_bounds(id self, SEL c, id page) {
    (void)c;
    pk_sel *s = SL(self);
    R u = {{0, 0}, {0, 0}};
    int any = 0;
    for (int i = 0; s && i < s->n; i++) {
        if (s->r[i].page != page) continue;
        pk_page *g = G(page);
        page_text(g);
        for (long j = s->r[i].loc; j < s->r[i].loc + s->r[i].len && j < g->ntext; j++) {
            R b = g->tbox[j];
            if (g->text[j] == '\n' || b.s.h <= 0) continue;
            if (!any) { u = b; any = 1; continue; }
            double x0 = u.o.x < b.o.x ? u.o.x : b.o.x, y0 = u.o.y < b.o.y ? u.o.y : b.o.y;
            double x1 = u.o.x + u.s.w > b.o.x + b.s.w ? u.o.x + u.s.w : b.o.x + b.s.w, y1 = u.o.y + u.s.h > b.o.y + b.s.h ? u.o.y + u.s.h : b.o.y + b.s.h;
            u.o.x = x0; u.o.y = y0; u.s.w = x1 - x0; u.s.h = y1 - y0;
        }
    }
    return u;
}
static unsigned long sl_nranges(id self, SEL c, id page) { (void)c; pk_sel *s = SL(self); unsigned long n = 0; for (int i = 0; s && i < s->n; i++) if (s->r[i].page == page) n++; return n; }
typedef struct { unsigned long loc, len; } nsrange_t;
static nsrange_t sl_range(id self, SEL c, unsigned long idx, id page) {
    (void)c;
    pk_sel *s = SL(self);
    nsrange_t z = {(unsigned long)-1, 0};
    unsigned long k = 0;
    for (int i = 0; s && i < s->n; i++) if (s->r[i].page == page && k++ == idx) { z.loc = (unsigned long)s->r[i].loc; z.len = (unsigned long)s->r[i].len; return z; }
    return z;
}
/* uma seleção por linha: corta cada trecho nas quebras de linha */
static id sl_by_line(id self, SEL c) {
    (void)c;
    pk_sel *s = SL(self);
    id arr = MSG0(id, cls("NSMutableArray"), "array");
    for (int i = 0; s && i < s->n; i++) {
        pk_page *g = G(s->r[i].page);
        page_text(g);
        long a = s->r[i].loc, e = s->r[i].loc + s->r[i].len;
        for (long j = a; j <= e; j++) {
            if (j == e || (j < g->ntext && g->text[j] == '\n')) {
                if (j > a) { id l = sel_new(s->doc); sel_add_range(l, s->r[i].page, a, j - a); MSG(void, arr, "addObject:", l); objc_release(l); }
                a = j + 1;
            }
        }
    }
    return arr;
}
static void sl_add(id self, SEL c, id other) {
    (void)c;
    pk_sel *o = SL(other);
    for (int i = 0; o && i < o->n; i++) sel_add_range(self, o->r[i].page, o->r[i].loc, o->r[i].len);
}
static void sl_add_many(id self, SEL c, id arr) { for (long i = 0; i < acount(arr); i++) sl_add(self, c, aget(arr, i)); }
static void sl_extend_end(id self, SEL c, long n) {
    (void)c;
    pk_sel *s = SL(self);
    if (!s || !s->n) return;
    pk_range *r = &s->r[s->n - 1];
    pk_page *g = G(r->page);
    page_text(g);
    r->len += n;
    if (r->loc + r->len > g->ntext) r->len = g->ntext - r->loc;
    if (r->len < 0) r->len = 0;
}
static void sl_extend_start(id self, SEL c, long n) {
    (void)c;
    pk_sel *s = SL(self);
    if (!s || !s->n) return;
    pk_range *r = &s->r[0];
    long nl = r->loc - n;
    if (nl < 0) nl = 0;
    r->len += r->loc - nl;
    r->loc = nl;
}
static void sl_extend_lines(id self, SEL c) {
    (void)c;
    pk_sel *s = SL(self);
    if (!s || !s->n) return;
    pk_range *f = &s->r[0], *l = &s->r[s->n - 1];
    pk_page *gf = G(f->page), *gl = G(l->page);
    page_text(gf); page_text(gl);
    long a, b, end = l->loc + l->len;
    pk_line_at(gf->text, gf->ntext, f->loc, &a, &b);
    f->len += f->loc - a; f->loc = a;
    pk_line_at(gl->text, gl->ntext, end > 0 ? end - 1 : 0, &a, &b);
    if (b > end) l->len += b - end;
}
static id sl_color(id self, SEL c) { (void)c; return SL(self) ? SL(self)->color : 0; }
static void sl_set_color(id self, SEL c, id v) { (void)c; if (SL(self)) keep(&SL(self)->color, v); }
static id sl_copy(id self, SEL c, void *z) { (void)z; (void)c; pk_sel *s = SL(self); id n = sel_new(s ? s->doc : 0); sl_add(n, 0, self); if (s) keep(&SL(n)->color, s->color); return n; }
/* PDFView: as caixas de cada caractere da seleção na página (desenho do realce) */
int cx_pdfkit_selection_rects(id sel, id page, R **out) {
    pk_sel *s = SL(sel);
    int n = 0, cap = 0;
    *out = 0;
    for (int i = 0; s && i < s->n; i++) {
        if (s->r[i].page != page) continue;
        pk_page *g = G(page);
        page_text(g);
        for (long j = s->r[i].loc; j < s->r[i].loc + s->r[i].len && j < g->ntext; j++) {
            if (g->text[j] == '\n' || g->tbox[j].s.h <= 0) continue;
            if (n + 1 > cap) { cap = cap ? cap * 2 : 64; R *nr = realloc(*out, (size_t)cap * sizeof *nr); if (!nr) return n; *out = nr; }
            (*out)[n++] = g->tbox[j];
        }
    }
    return n;
}
static void sl_draw(id self, SEL c, id page, long box, BOOL active) {
    (void)c; (void)box;
    void *ctx = current_cgctx();
    R *rs;
    int n = cx_pdfkit_selection_rects(self, page, &rs);
    if (!ctx || !n) { free(rs); return; }
    double col[4] = {0.0, 0.4, 0.86, active ? 0.35 : 0.2};
    if (SL(self)->color) { rgba_of(SL(self)->color, col); col[3] = 0.4; }
    CGContextSaveGState(ctx);
    CGContextSetRGBFillColor(ctx, col[0], col[1], col[2], col[3]);
    for (int i = 0; i < n; i++) CGContextFillRect(ctx, rs[i]);
    CGContextRestoreGState(ctx);
    free(rs);
}
static void sl_dealloc(id self, SEL c) {
    (void)c;
    pk_sel *s = SL(self);
    if (s) { for (int i = 0; i < s->n; i++) objc_release(s->r[i].page); free(s->r); keep(&s->color, 0); free(s); }
    PK(self) = 0;
    super_dealloc(self, &PDFSelection_c);
}

/* ---------------------------------------------------------------- PDFOutline */
static id ol_init(id self, SEL c) { (void)c; pk_outline *o = st_new(self, sizeof(pk_outline)); o->kids = marr(); return self; }
static id ol_doc(id self, SEL c) { (void)c; return O(self) ? O(self)->doc : 0; }
static id ol_parent(id self, SEL c) { (void)c; return O(self) ? O(self)->parent : 0; }
static unsigned long ol_count(id self, SEL c) { (void)c; return O(self) ? (unsigned long)acount(O(self)->kids) : 0; }
static unsigned long ol_index(id self, SEL c) {
    (void)c;
    pk_outline *o = O(self);
    if (!o || !o->parent) return 0;
    return MSG(unsigned long, O(o->parent)->kids, "indexOfObjectIdenticalTo:", self);
}
static id ol_child(id self, SEL c, unsigned long i) { (void)c; pk_outline *o = O(self); return o && (long)i < acount(o->kids) ? aget(o->kids, (long)i) : 0; }
static void ol_set_doc_rec(id o, id doc) { pk_outline *s = O(o); if (!s) return; s->doc = doc; for (long i = 0; i < acount(s->kids); i++) ol_set_doc_rec(aget(s->kids, i), doc); }
static void ol_insert(id self, SEL c, id child, unsigned long i) {
    (void)c;
    pk_outline *o = O(self);
    if (!o || !child || !O(child)) return;
    if (O(child)->parent) MSG0(void, child, "removeFromParent");
    long n = acount(o->kids);
    MSG(void, o->kids, "insertObject:atIndex:", child, (unsigned long)((long)i > n ? n : (long)i));
    O(child)->parent = self;
    ol_set_doc_rec(child, o->doc);
    if (o->doc && D(o->doc)) D(o->doc)->modified = 1;
}
static void ol_remove(id self, SEL c) {
    (void)c;
    pk_outline *o = O(self);
    if (!o || !o->parent) return;
    id p = o->parent;
    o->parent = 0;
    objc_retain(self);
    MSG(void, O(p)->kids, "removeObjectIdenticalTo:", self);
    if (o->doc && D(o->doc)) D(o->doc)->modified = 1;
    objc_autorelease(self);
}
static id ol_label(id self, SEL c) { (void)c; return O(self) ? O(self)->label : 0; }
static void ol_set_label(id self, SEL c, id v) { (void)c; if (O(self)) keep(&O(self)->label, v); }
static BOOL ol_open(id self, SEL c) { (void)c; return O(self) && O(self)->open; }
static void ol_set_open(id self, SEL c, BOOL v) { (void)c; if (O(self)) O(self)->open = v; }
static id ol_dest(id self, SEL c) { (void)c; pk_outline *o = O(self); if (!o) return 0; if (o->dest) return o->dest; return o->action && AC(o->action) && AC(o->action)->kind == 2 ? AC(o->action)->dest : 0; }
static void ol_set_dest(id self, SEL c, id v) { (void)c; if (O(self)) keep(&O(self)->dest, v); }
static id ol_action(id self, SEL c) { (void)c; return O(self) ? O(self)->action : 0; }
static void ol_set_action(id self, SEL c, id v) { (void)c; if (O(self)) keep(&O(self)->action, v); }
static void ol_dealloc(id self, SEL c) {
    (void)c;
    pk_outline *o = O(self);
    if (o) {
        for (long i = 0; i < acount(o->kids); i++) if (O(aget(o->kids, i))) O(aget(o->kids, i))->parent = 0;
        keep(&o->kids, 0); keep(&o->label, 0); keep(&o->dest, 0); keep(&o->action, 0);
        free(o);
    }
    PK(self) = 0;
    super_dealloc(self, &PDFOutline_c);
}
/* outline do PDF (First/Next, Title, Dest ou A) */
static void outline_load(id doc, id parent, pdf_doc *pd, pdf_obj *item, int depth) {
    for (int guard = 0; item && item->type == PO_DICT && guard < 10000 && depth < 32; guard++) {
        id o = MSG0(id, MSG0(id, (id)&PDFOutline_c, "alloc"), "init");
        O(o)->label = objc_retain(pdf_text_str(pdf_dict_get(pd, item, "Title")));
        double cnt = 0;
        O(o)->open = pdf_num(pdf_dict_get(pd, item, "Count"), &cnt) && cnt > 0;
        pdf_obj *dest = pdf_dict_get(pd, item, "Dest"), *act = pdf_dict_get(pd, item, "A");
        const char *as = act ? pdf_name(pdf_dict_get(pd, act, "S")) : 0;
        if (!dest && as && !strcmp(as, "GoTo")) dest = pdf_dict_get(pd, act, "D");
        if (dest) {
            int pi = cx_pdf_dest_page(pd, dest) - 1;
            if (pi >= 0 && pi < acount(D(doc)->pages)) {
                pdf_obj *arr = cx_pdf_dest_array(pd, dest);
                double x = 0, y = 0;
                const char *fit = arr && arr->type == PO_ARRAY && arr->n >= 2 ? pdf_name(pdf_resolve(pd, arr->v.arr[1])) : 0;
                if (fit && !strcmp(fit, "XYZ") && arr->n >= 4) { pdf_num(pdf_resolve(pd, arr->v.arr[2]), &x); pdf_num(pdf_resolve(pd, arr->v.arr[3]), &y); }
                else { R b = pg_bounds(aget(D(doc)->pages, pi), 0, 1); x = b.o.x; y = b.o.y + b.s.h; }
                O(o)->dest = dest_new(aget(D(doc)->pages, pi), (P){x, y});
            }
        } else if (as && !strcmp(as, "URI")) {
            pdf_obj *u = pdf_dict_get(pd, act, "URI");
            if (u && u->type == PO_STRING) {
                char *t = malloc(u->n + 1);
                if (t) {
                    memcpy(t, u->v.str, u->n); t[u->n] = 0;
                    id url = MSG(id, cls("NSURL"), "URLWithString:", cx_str(t));
                    if (url) O(o)->action = MSG(id, MSG0(id, (id)&PDFActionURL_c, "alloc"), "initWithURL:", url);
                    free(t);
                }
            }
        }
        MSG(void, parent, "insertChild:atIndex:", o, (unsigned long)acount(O(parent)->kids));
        objc_release(o);
        pdf_obj *first = pdf_dict_get(pd, item, "First");
        if (first) outline_load(doc, o, pd, first, depth + 1);
        item = pdf_dict_get(pd, item, "Next");
    }
}

/* ---------------------------------------------------------------- PDFDocument */
static void doc_load_pages(id self) {
    pk_doc *d = D(self);
    keep(&d->pages, 0);
    d->pages = marr();
    if (!d->cg || !CGPDFDocumentIsUnlocked(d->cg)) return;
    size_t n = CGPDFDocumentGetNumberOfPages(d->cg);
    for (size_t i = 1; i <= n; i++) {
        id p = page_wrap(self, CGPDFDocumentGetPage(d->cg, i));
        MSG(void, d->pages, "addObject:", p);
        objc_release(p);
    }
}
static id dc_init(id self, SEL c) { (void)c; pk_doc *d = st_new(self, sizeof(pk_doc)); d->pages = marr(); return self; }
static id dc_init_data(id self, SEL c, id data) {
    (void)c;
    pk_doc *d = st_new(self, sizeof(pk_doc));
    long n = data ? MSG0(long, data, "length") : 0;
    if (n <= 0) { d->pages = marr(); return self; }
    u8 *copy = malloc((size_t)n);
    if (!copy) return self;
    memcpy(copy, MSG0(const void *, data, "bytes"), (size_t)n);
    void *prov = CGDataProviderCreateWithData(0, copy, (size_t)n, 0);
    d->cg = prov ? CGPDFDocumentCreateWithProvider(prov) : 0;
    if (prov) CGDataProviderRelease(prov);
    if (!d->cg) { free(PK(self)); PK(self) = 0; objc_release(self); return 0; }
    d->data = MSG0(id, data, "copy");
    doc_load_pages(self);
    return self;
}
static id dc_init_url(id self, SEL c, id url) {
    id data = url ? MSG(id, cls("NSData"), "dataWithContentsOfURL:", url) : 0;
    if (!data) { objc_release(self); return 0; }
    id r = dc_init_data(self, c, data);
    if (r) keep(&D(r)->url, url);
    return r;
}
static id dc_url(id self, SEL c) { (void)c; return D(self) ? D(self)->url : 0; }
static void *dc_ref(id self, SEL c) { (void)c; return D(self) ? D(self)->cg : 0; }
static unsigned long dc_count(id self, SEL c) { (void)c; return D(self) ? (unsigned long)acount(D(self)->pages) : 0; }
static id dc_page(id self, SEL c, unsigned long i) { (void)c; pk_doc *d = D(self); return d && (long)i < acount(d->pages) ? aget(d->pages, (long)i) : 0; }
static unsigned long dc_index(id self, SEL c, id page) { (void)c; pk_doc *d = D(self); return d ? MSG(unsigned long, d->pages, "indexOfObjectIdenticalTo:", page) : (unsigned long)-1; }
static void dc_insert(id self, SEL c, id page, unsigned long i) {
    (void)c;
    pk_doc *d = D(self);
    if (!d || !page || !G(page)) return;
    long n = acount(d->pages);
    MSG(void, d->pages, "insertObject:atIndex:", page, (unsigned long)((long)i > n ? n : (long)i));
    G(page)->doc = self;
    d->modified = 1;
}
static void dc_remove(id self, SEL c, unsigned long i) {
    (void)c;
    pk_doc *d = D(self);
    if (!d || (long)i >= acount(d->pages)) return;
    id p = aget(d->pages, (long)i);
    if (G(p)) G(p)->doc = 0;
    MSG(void, d->pages, "removeObjectAtIndex:", i);
    d->modified = 1;
}
static void dc_exchange(id self, SEL c, unsigned long a, unsigned long b) {
    (void)c;
    pk_doc *d = D(self);
    if (!d || (long)a >= acount(d->pages) || (long)b >= acount(d->pages)) return;
    MSG(void, d->pages, "exchangeObjectAtIndex:withObjectAtIndex:", a, b);
    d->modified = 1;
}
static id dc_page_class(id self, SEL c) { (void)self; (void)c; return (id)&PDFPage_c; }
static int dc_version(id self, int which) { int ma = 1, mi = 4; if (D(self) && D(self)->cg) CGPDFDocumentGetVersion(D(self)->cg, &ma, &mi); return which ? mi : ma; }
static long dc_major(id self, SEL c) { (void)c; return dc_version(self, 0); }
static long dc_minor(id self, SEL c) { (void)c; return dc_version(self, 1); }
static BOOL dc_encrypted(id self, SEL c) { (void)c; return D(self) && D(self)->cg && CGPDFDocumentIsEncrypted(D(self)->cg); }
static BOOL dc_locked(id self, SEL c) { (void)c; return D(self) && D(self)->cg && !CGPDFDocumentIsUnlocked(D(self)->cg); }
static BOOL dc_unlock(id self, SEL c, id pw) {
    (void)c;
    pk_doc *d = D(self);
    if (!d || !d->cg) return NO;
    int was = CGPDFDocumentIsUnlocked(d->cg);
    if (!CGPDFDocumentUnlockWithPassword(d->cg, pw ? cx_cstr(pw) : "")) return NO;
    if (!was) { doc_load_pages(self); post((id)PDFDocumentDidUnlockNotification, self, 0); }
    return YES;
}
static u32 perms(id self) { return D(self) && D(self)->cg ? CGPDFDocumentGetAccessPermissions(D(self)->cg) : 0xFF; }
static BOOL dc_allows_print(id self, SEL c) { (void)c; return (perms(self) & 1) != 0; }
static BOOL dc_allows_copy(id self, SEL c) { (void)c; return (perms(self) & (1u << 4)) != 0; }
static BOOL dc_allows_changes(id self, SEL c) { (void)c; return (perms(self) & (1u << 2)) != 0; }
static BOOL dc_allows_assembly(id self, SEL c) { (void)c; return (perms(self) & (1u << 3)) != 0; }
static BOOL dc_allows_access(id self, SEL c) { (void)c; return (perms(self) & (1u << 5)) != 0; }
static BOOL dc_allows_comment(id self, SEL c) { (void)c; return (perms(self) & (1u << 6)) != 0; }
static BOOL dc_allows_forms(id self, SEL c) { (void)c; return (perms(self) & (1u << 7)) != 0; }
static unsigned long dc_access(id self, SEL c) { (void)c; return perms(self); }
static long dc_perm_status(id self, SEL c) { (void)c; return dc_encrypted(self, 0) ? (dc_locked(self, 0) ? 0 : 1) : 2; }
static id dc_attrs(id self, SEL c) {
    (void)c;
    pk_doc *d = D(self);
    if (!d) return 0;
    if (d->attrs) return d->attrs;
    id m = MSG0(id, cls("NSMutableDictionary"), "dictionary");
    pdf_doc *pd = acount(d->pages) && G(aget(d->pages, 0))->cg ? cx_pdf_page_doc(G(aget(d->pages, 0))->cg) : 0;
    pdf_obj *info = pd ? pdf_info(pd) : 0;
    static const char *const keys[] = {"Title", "Author", "Subject", "Creator", "Producer", "Keywords"};
    for (int i = 0; info && i < 6; i++) {
        id v = pdf_text_str(pdf_dict_get(pd, info, keys[i]));
        if (!v) continue;
        if (i == 5) {
            /* Keywords: array separado por vírgula ou ponto e vírgula */
            id sep = MSG(id, cls("NSCharacterSet"), "characterSetWithCharactersInString:", cx_str(",;"));
            id parts = MSG(id, v, "componentsSeparatedByCharactersInSet:", sep);
            id clean = MSG0(id, cls("NSMutableArray"), "array");
            for (long k = 0; k < acount(parts); k++) {
                id t = MSG(id, aget(parts, k), "stringByTrimmingCharactersInSet:", MSG0(id, cls("NSCharacterSet"), "whitespaceCharacterSet"));
                if (MSG0(long, t, "length")) MSG(void, clean, "addObject:", t);
            }
            MSG(void, m, "setObject:forKey:", clean, cx_str(keys[i]));
        } else MSG(void, m, "setObject:forKey:", v, cx_str(keys[i]));
    }
    static const char *const dk[] = {"CreationDate", "ModDate"};
    for (int i = 0; info && i < 2; i++) {
        pdf_obj *s = pdf_dict_get(pd, info, dk[i]);
        if (!s || s->type != PO_STRING || s->n < 6) continue;
        /* D:AAAAMMDDHHmmSS±HH'mm */
        const u8 *t = s->v.str;
        size_t k = (t[0] == 'D' && t[1] == ':') ? 2 : 0;
        int v[6] = {0, 1, 1, 0, 0, 0}, len[6] = {4, 2, 2, 2, 2, 2};
        for (int q = 0; q < 6 && k < s->n; q++) { int x = 0, got = 0; for (int r = 0; r < len[q] && k < s->n && t[k] >= '0' && t[k] <= '9'; r++, k++) { x = x * 10 + (t[k] - '0'); got = 1; } if (got) v[q] = x; }
        long tz = 0;
        if (k < s->n && (t[k] == '+' || t[k] == '-')) {
            int sg = t[k] == '-' ? -1 : 1, hh = 0, mm = 0;
            k++;
            for (int r = 0; r < 2 && k < s->n && t[k] >= '0' && t[k] <= '9'; r++, k++) hh = hh * 10 + (t[k] - '0');
            if (k < s->n && t[k] == '\'') k++;
            for (int r = 0; r < 2 && k < s->n && t[k] >= '0' && t[k] <= '9'; r++, k++) mm = mm * 10 + (t[k] - '0');
            tz = sg * (hh * 3600 + mm * 60);
        }
        /* dias desde 2001-01-01 (referência do NSDate) */
        long y = v[0], mo = v[1], da = v[2];
        long yy = mo <= 2 ? y - 1 : y, era = (yy >= 0 ? yy : yy - 399) / 400, yoe = yy - era * 400;
        long doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + da - 1, doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        long days = era * 146097 + doe - 719468 - 11323;
        double at = (double)days * 86400 + v[3] * 3600 + v[4] * 60 + v[5] - tz;
        id date = MSG(id, cls("NSDate"), "dateWithTimeIntervalSinceReferenceDate:", at);
        if (date) MSG(void, m, "setObject:forKey:", date, cx_str(dk[i]));
    }
    d->attrs = objc_retain(m);
    return d->attrs;
}
static void dc_set_attrs(id self, SEL c, id a) { (void)c; if (D(self)) { keep(&D(self)->attrs, a); D(self)->modified = 1; } }
static id dc_outline(id self, SEL c) {
    (void)c;
    pk_doc *d = D(self);
    if (!d) return 0;
    if (d->outline) return d->outline;
    pdf_doc *pd = acount(d->pages) && G(aget(d->pages, 0))->cg ? cx_pdf_page_doc(G(aget(d->pages, 0))->cg) : 0;
    pdf_obj *ol = pd ? pdf_dict_get(pd, pdf_catalog(pd), "Outlines") : 0;
    pdf_obj *first = ol ? pdf_dict_get(pd, ol, "First") : 0;
    if (!first) return 0;
    id root = MSG0(id, MSG0(id, (id)&PDFOutline_c, "alloc"), "init");
    O(root)->doc = self;
    O(root)->open = 1;
    outline_load(self, root, pd, first, 0);
    d->outline = root;
    return root;
}
static void dc_set_outline(id self, SEL c, id o) { (void)c; pk_doc *d = D(self); if (!d) return; keep(&d->outline, o); if (o) ol_set_doc_rec(o, self); d->modified = 1; }
static id dc_string(id self, SEL c) {
    (void)c;
    pk_doc *d = D(self);
    id m = MSG0(id, cls("NSMutableString"), "string");
    for (long i = 0; d && i < acount(d->pages); i++) {
        id s = pg_string(aget(d->pages, i), 0);
        if (i) MSG(void, m, "appendString:", cx_str("\n"));
        if (s) MSG(void, m, "appendString:", s);
    }
    return m;
}
static id dc_sel_all(id self, SEL c) {
    (void)c;
    pk_doc *d = D(self);
    id s = sel_new(self);
    for (long i = 0; d && i < acount(d->pages); i++) { pk_page *g = G(aget(d->pages, i)); page_text(g); sel_add_range(s, aget(d->pages, i), 0, g->ntext); }
    return objc_autorelease(s);
}
static id dc_sel_chars(id self, SEL c, id pa, unsigned long ia, id pb, unsigned long ib) {
    (void)c;
    pk_doc *d = D(self);
    long a = (long)dc_index(self, 0, pa), b = (long)dc_index(self, 0, pb);
    if (!d || a < 0 || b < 0 || a >= acount(d->pages) || b >= acount(d->pages)) return 0;
    id s = sel_new(self);
    for (long i = a; i <= b; i++) {
        pk_page *g = G(aget(d->pages, i));
        page_text(g);
        long from = i == a ? (long)ia : 0, to = i == b ? (long)ib + 1 : g->ntext;
        if (to > g->ntext) to = g->ntext;
        if (to > from) sel_add_range(s, aget(d->pages, i), from, to - from);
    }
    return objc_autorelease(s);
}
static id dc_sel_points(id self, SEL c, id pa, P a, id pb, P b) {
    pk_page *ga = G(pa), *gb = G(pb);
    if (!ga || !gb) return 0;
    long ia = char_at(ga, a, 1), ib = char_at(gb, b, 1);
    if (ia < 0 || ib < 0) return 0;
    long xa = (long)dc_index(self, 0, pa), xb = (long)dc_index(self, 0, pb);
    if (xa > xb || (xa == xb && ia > ib)) { long t = ia; ia = ib; ib = t; id tp = pa; pa = pb; pb = tp; }
    return dc_sel_chars(self, c, pa, (unsigned long)ia, pb, (unsigned long)ib);
}
/* busca: NSCaseInsensitiveSearch 1, NSLiteralSearch 2, NSBackwardsSearch 4, NSDiacriticInsensitiveSearch 128 */
static id find_in(id self, id needle_s, unsigned long opts, long from_page, long from_char, int all) {
    pk_doc *d = D(self);
    id out = MSG0(id, cls("NSMutableArray"), "array");
    if (!d || !needle_s) return out;
    u16 *nd;
    long m = str_utf16(needle_s, &nd);
    int fold = (opts & 1) != 0, dia = (opts & 128) != 0, back = (opts & 4) != 0;
    long w = 0;
    for (long i = 0; i < m; i++) { u16 ch = fold || dia ? pk_fold(nd[i], dia) : nd[i]; if (!ch && dia) continue; if (ch == '\n' || ch == '\t' || ch == 0xA0) ch = ' '; nd[w++] = ch; }
    m = w;
    long np = acount(d->pages);
    for (long k = 0; m && k < np; k++) {
        long pi = back ? (from_page >= 0 ? from_page : np - 1) - k : (from_page >= 0 ? from_page : 0) + k;
        if (pi < 0 || pi >= np) break;
        id page = aget(d->pages, pi);
        pk_page *g = G(page);
        page_text(g);
        long start = (k == 0 && from_char >= 0) ? from_char : (back ? g->ntext - 1 : 0);
        for (;;) {
            long len, at = pk_find(g->text, g->ntext, nd, m, start, fold, dia, back, &len);
            if (at < 0) break;
            id s = sel_new(self);
            sel_add_range(s, page, at, len);
            MSG(void, out, "addObject:", s);
            objc_release(s);
            if (!all) { free(nd); return out; }
            start = back ? at - 1 : at + (len > 0 ? len : 1);
            if (start < 0 || start >= g->ntext) break;
        }
    }
    free(nd);
    return out;
}
static id dc_find(id self, SEL c, id s, unsigned long opts) {
    (void)c;
    post((id)PDFDocumentDidBeginFindNotification, self, 0);
    id r = find_in(self, s, opts, -1, -1, 1);
    id delegate = D(self) ? D(self)->delegate : 0;
    for (long i = 0; i < acount(r); i++) {
        id info = MSG(id, cls("NSDictionary"), "dictionaryWithObject:forKey:", aget(r, i), (id)PDFDocumentFoundSelectionKey);
        post((id)PDFDocumentDidFindMatchNotification, self, info);
        if (delegate && cx_responds(delegate->isa, sel_registerName("didMatchString:"))) MSG(void, delegate, "didMatchString:", aget(r, i));
    }
    post((id)PDFDocumentDidEndFindNotification, self, 0);
    return r;
}
static id dc_find_from(id self, SEL c, id s, id from, unsigned long opts) {
    (void)c;
    pk_sel *f = SL(from);
    long pi = -1, ci = -1;
    if (f && f->n) {
        pk_range *r = (opts & 4) ? &f->r[0] : &f->r[f->n - 1];
        pi = (long)dc_index(self, 0, r->page);
        ci = (opts & 4) ? r->loc - 1 : r->loc + r->len;
    }
    id res = find_in(self, s, opts, pi, ci, 0);
    return acount(res) ? aget(res, 0) : 0;
}
static void dc_begin_find(id self, SEL c, id s, unsigned long opts) { (void)dc_find(self, c, s, opts); }
static void dc_begin_find_many(id self, SEL c, id arr, unsigned long opts) { for (long i = 0; i < acount(arr); i++) (void)dc_find(self, c, aget(arr, i), opts); }
static BOOL dc_is_finding(id self, SEL c) { (void)self; (void)c; return NO; }
static void dc_cancel_find(id self, SEL c) { (void)self; (void)c; }
static id dc_delegate(id self, SEL c) { (void)c; return D(self) ? D(self)->delegate : 0; }
static void dc_set_delegate(id self, SEL c, id v) { (void)c; if (D(self)) D(self)->delegate = v; }
static id dc_outline_for_sel(id self, SEL c, id sel) {
    (void)c;
    id root = dc_outline(self, 0);
    pk_sel *s = SL(sel);
    if (!root || !s || !s->n) return 0;
    long target = (long)dc_index(self, 0, s->r[0].page);
    /* o item mais fundo cujo destino vem antes da seleção */
    id best = 0;
    id stack[256];
    int sp = 0;
    stack[sp++] = root;
    while (sp) {
        id o = stack[--sp];
        id dd = ol_dest(o, 0);
        if (dd && DS(dd) && DS(dd)->page) { long pi = (long)dc_index(self, 0, DS(dd)->page); if (pi <= target) best = o; }
        for (long i = acount(O(o)->kids) - 1; i >= 0 && sp < 256; i--) stack[sp++] = aget(O(o)->kids, i);
    }
    return best;
}

/* regravação */
static void outline_tree(id self, id o, pdfwr_outline *out, char ***strs, int *ns) {
    pk_outline *s = O(o);
    memset(out, 0, sizeof *out);
    out->page = -1;
    const char *t = s->label ? cx_cstr(s->label) : "";
    char *dup = malloc(strlen(t) + 1);
    if (dup) { strcpy(dup, t); char **n = realloc(*strs, (size_t)(*ns + 1) * sizeof *n); if (n) { *strs = n; (*strs)[(*ns)++] = dup; } }
    out->title = dup ? dup : "";
    out->open = s->open;
    id dd = ol_dest(o, 0);
    if (dd && DS(dd) && DS(dd)->page) { out->page = (int)(long)dc_index(self, 0, DS(dd)->page); out->x = DS(dd)->pt.x; out->y = DS(dd)->pt.y; }
    else if (s->action && AC(s->action) && AC(s->action)->kind == 1 && AC(s->action)->url) {
        const char *u = cx_cstr(MSG0(id, AC(s->action)->url, "absoluteString"));
        char *du = malloc(strlen(u) + 1);
        if (du) { strcpy(du, u); char **n = realloc(*strs, (size_t)(*ns + 1) * sizeof *n); if (n) { *strs = n; (*strs)[(*ns)++] = du; } out->uri = du; }
    }
    long nk = acount(s->kids);
    if (nk) {
        pdfwr_outline *kids = calloc((size_t)nk, sizeof *kids);
        if (kids) {
            for (long i = 0; i < nk; i++) outline_tree(self, aget(s->kids, i), &kids[i], strs, ns);
            out->kids = kids;
            out->nkids = (int)nk;
        }
    }
}
static void outline_free(pdfwr_outline *o) { for (int i = 0; i < o->nkids; i++) outline_free((pdfwr_outline *)&o->kids[i]); free((void *)o->kids); }
static id dc_data(id self, SEL c) {
    (void)c;
    pk_doc *d = D(self);
    if (!d) return 0;
    if (!d->modified && d->data && !dc_encrypted(self, 0)) return d->data;
    post((id)PDFDocumentDidBeginWriteNotification, self, 0);
    pdfwr *w = pdfwr_new();
    if (!w) return 0;
    long np = acount(d->pages);
    for (long i = 0; i < np; i++) {
        pk_page *g = G(aget(d->pages, i));
        pdfwr_reserve(w, g && g->cg ? cx_pdf_page_doc(g->cg) : 0, g && g->cg ? CGPDFPageGetDictionary(g->cg) : 0);
    }
    for (long i = 0; i < np; i++) {
        id page = aget(d->pages, i);
        pk_page *g = G(page);
        pdfwr_page_opts o;
        memset(&o, 0, sizeof o);
        o.rotate = g->rotation;
        o.nannots = -1;
        if (g->hasbox[0]) { o.has_media = 1; o.media[0] = g->box[0].o.x; o.media[1] = g->box[0].o.y; o.media[2] = g->box[0].o.x + g->box[0].s.w; o.media[3] = g->box[0].o.y + g->box[0].s.h; }
        if (g->hasbox[1]) { o.has_crop = 1; o.crop[0] = g->box[1].o.x; o.crop[1] = g->box[1].o.y; o.crop[2] = g->box[1].o.x + g->box[1].s.w; o.crop[3] = g->box[1].o.y + g->box[1].s.h; }
        pdfwr_annot *an = 0;
        char **raws = 0;
        int nraw = 0;
        int changed = g->annots_changed;
        for (long k = 0; !changed && k < acount(g->annots); k++) if (A(aget(g->annots, k))->changed) changed = 1;
        if (g->annots_loaded && changed) {
            long na = acount(g->annots);
            an = calloc((size_t)(na ? na : 1), sizeof *an);
            raws = calloc((size_t)(na ? na : 1), sizeof *raws);
            for (long k = 0; an && raws && k < na; k++) {
                pk_annot *a = A(aget(g->annots, k));
                if (a->src && !a->changed) { an[k].doc = a->srcdoc; an[k].obj = a->src; }
                else { raws[nraw] = an_pdf_dict(aget(g->annots, k), self); an[k].raw = raws[nraw++]; }
            }
            o.annots = an;
            o.nannots = an ? (int)na : -1;
        }
        pdfwr_page(w, (int)i, &o);
        for (int k = 0; k < nraw; k++) free(raws[k]);
        free(raws);
        free(an);
    }
    id attrs = dc_attrs(self, 0);
    static const char *const keys[] = {"Title", "Author", "Subject", "Creator", "Keywords"};
    for (int i = 0; i < 5; i++) {
        id v = MSG(id, attrs, "objectForKey:", cx_str(keys[i]));
        if (!v) continue;
        if (MSG(BOOL, v, "isKindOfClass:", cls("NSArray"))) v = MSG(id, v, "componentsJoinedByString:", cx_str(", "));
        pdfwr_info(w, keys[i], cx_cstr(v));
    }
    id root = dc_outline(self, 0);
    if (root && acount(O(root)->kids)) {
        long nk = acount(O(root)->kids);
        pdfwr_outline *items = calloc((size_t)nk, sizeof *items);
        char **strs = 0;
        int ns = 0;
        for (long i = 0; items && i < nk; i++) outline_tree(self, aget(O(root)->kids, i), &items[i], &strs, &ns);
        if (items) pdfwr_outline_set(w, items, (int)nk);
        for (long i = 0; items && i < nk; i++) outline_free(&items[i]);
        free(items);
        for (int i = 0; i < ns; i++) free(strs[i]);
        free(strs);
    }
    u8 *out;
    size_t n;
    pdfwr_finish(w, &out, &n);
    pdfwr_free(w);
    id data = MSG(id, cls("NSData"), "dataWithBytes:length:", out, (unsigned long)n);
    free(out);
    post((id)PDFDocumentDidEndWriteNotification, self, 0);
    return data;
}
static id dc_data_opts(id self, SEL c, id opts) { (void)opts; return dc_data(self, c); }
static BOOL dc_write_file(id self, SEL c, id path) { id data = dc_data(self, c); return data && MSG(BOOL, data, "writeToFile:atomically:", path, (BOOL)YES); }
static BOOL dc_write_url(id self, SEL c, id url) { id data = dc_data(self, c); return data && url && MSG(BOOL, data, "writeToURL:atomically:", url, (BOOL)YES); }
static BOOL dc_write_file_opts(id self, SEL c, id path, id opts) { (void)opts; return dc_write_file(self, c, path); }
static BOOL dc_write_url_opts(id self, SEL c, id url, id opts) { (void)opts; return dc_write_url(self, c, url); }
static id dc_print_op(id self, SEL c, id info, long mode, BOOL autorot) { (void)self; (void)c; (void)info; (void)mode; (void)autorot; return 0; }
static void dc_dealloc(id self, SEL c) {
    (void)c;
    pk_doc *d = D(self);
    if (d) {
        for (long i = 0; i < acount(d->pages); i++) if (G(aget(d->pages, i)) && G(aget(d->pages, i))->doc == self) G(aget(d->pages, i))->doc = 0;
        keep(&d->pages, 0); keep(&d->url, 0); keep(&d->data, 0); keep(&d->attrs, 0); keep(&d->outline, 0);
        if (d->cg) CGPDFDocumentRelease(d->cg);
        free(d);
    }
    PK(self) = 0;
    super_dealloc(self, &PDFDocument_c);
}

/* ---------------------------------------------------------------- registro */
void cx_pdfkit_view_init(void);
#define M(k, s, f) cx_add_method(&k##_c, s, (void *)(f))
void cx_pdfkit_init(void) {
    cx_class_t *ns = cx_class_named("NSObject");
    if (!ns || PDFDocument_c.vtable) return;
    g_off = cx_instance_size(ns);
    cx_define_class(&PDFDocument_c, &PDFDocument_m, "PDFDocument", ns, sizeof(void *));
    cx_define_class(&PDFPage_c, &PDFPage_m, "PDFPage", ns, sizeof(void *));
    cx_define_class(&PDFSelection_c, &PDFSelection_m, "PDFSelection", ns, sizeof(void *));
    cx_define_class(&PDFAnnotation_c, &PDFAnnotation_m, "PDFAnnotation", ns, sizeof(void *));
    cx_define_class(&PDFOutline_c, &PDFOutline_m, "PDFOutline", ns, sizeof(void *));
    cx_define_class(&PDFDestination_c, &PDFDestination_m, "PDFDestination", ns, sizeof(void *));
    cx_define_class(&PDFAction_c, &PDFAction_m, "PDFAction", ns, sizeof(void *));
    cx_define_class(&PDFActionURL_c, &PDFActionURL_m, "PDFActionURL", &PDFAction_c, 0);
    cx_define_class(&PDFActionGoTo_c, &PDFActionGoTo_m, "PDFActionGoTo", &PDFAction_c, 0);
    cx_define_class(&PDFActionNamed_c, &PDFActionNamed_m, "PDFActionNamed", &PDFAction_c, 0);
    cx_define_class(&PDFActionResetForm_c, &PDFActionResetForm_m, "PDFActionResetForm", &PDFAction_c, 0);
    cx_define_class(&PDFActionRemoteGoTo_c, &PDFActionRemoteGoTo_m, "PDFActionRemoteGoTo", &PDFAction_c, 0);
    cx_define_class(&PDFBorder_c, &PDFBorder_m, "PDFBorder", ns, sizeof(void *));
    cx_define_class(&PDFAppearanceCharacteristics_c, &PDFAppearanceCharacteristics_m, "PDFAppearanceCharacteristics", ns, sizeof(void *));

    M(PDFDocument, "init", dc_init); M(PDFDocument, "initWithURL:", dc_init_url); M(PDFDocument, "initWithData:", dc_init_data);
    M(PDFDocument, "documentURL", dc_url); M(PDFDocument, "documentRef", dc_ref); M(PDFDocument, "pageCount", dc_count);
    M(PDFDocument, "pageAtIndex:", dc_page); M(PDFDocument, "indexForPage:", dc_index); M(PDFDocument, "insertPage:atIndex:", dc_insert);
    M(PDFDocument, "removePageAtIndex:", dc_remove); M(PDFDocument, "exchangePageAtIndex:withPageAtIndex:", dc_exchange);
    M(PDFDocument, "pageClass", dc_page_class); M(PDFDocument, "majorVersion", dc_major); M(PDFDocument, "minorVersion", dc_minor);
    M(PDFDocument, "isEncrypted", dc_encrypted); M(PDFDocument, "isLocked", dc_locked); M(PDFDocument, "unlockWithPassword:", dc_unlock);
    M(PDFDocument, "allowsPrinting", dc_allows_print); M(PDFDocument, "allowsCopying", dc_allows_copy);
    M(PDFDocument, "allowsDocumentChanges", dc_allows_changes); M(PDFDocument, "allowsDocumentAssembly", dc_allows_assembly);
    M(PDFDocument, "allowsContentAccessibility", dc_allows_access); M(PDFDocument, "allowsCommenting", dc_allows_comment);
    M(PDFDocument, "allowsFormFieldEntry", dc_allows_forms); M(PDFDocument, "accessPermissions", dc_access);
    M(PDFDocument, "permissionsStatus", dc_perm_status);
    M(PDFDocument, "documentAttributes", dc_attrs); M(PDFDocument, "setDocumentAttributes:", dc_set_attrs);
    M(PDFDocument, "outlineRoot", dc_outline); M(PDFDocument, "setOutlineRoot:", dc_set_outline);
    M(PDFDocument, "outlineItemForSelection:", dc_outline_for_sel);
    M(PDFDocument, "string", dc_string); M(PDFDocument, "selectionForEntireDocument", dc_sel_all);
    M(PDFDocument, "selectionFromPage:atCharacterIndex:toPage:atCharacterIndex:", dc_sel_chars);
    M(PDFDocument, "selectionFromPage:atPoint:toPage:atPoint:", dc_sel_points);
    M(PDFDocument, "findString:withOptions:", dc_find); M(PDFDocument, "findString:fromSelection:withOptions:", dc_find_from);
    M(PDFDocument, "beginFindString:withOptions:", dc_begin_find); M(PDFDocument, "beginFindStrings:withOptions:", dc_begin_find_many);
    M(PDFDocument, "isFinding", dc_is_finding); M(PDFDocument, "cancelFindString", dc_cancel_find);
    M(PDFDocument, "delegate", dc_delegate); M(PDFDocument, "setDelegate:", dc_set_delegate);
    M(PDFDocument, "dataRepresentation", dc_data); M(PDFDocument, "dataRepresentationWithOptions:", dc_data_opts);
    M(PDFDocument, "writeToFile:", dc_write_file); M(PDFDocument, "writeToURL:", dc_write_url);
    M(PDFDocument, "writeToFile:withOptions:", dc_write_file_opts); M(PDFDocument, "writeToURL:withOptions:", dc_write_url_opts);
    M(PDFDocument, "printOperationForPrintInfo:scalingMode:autoRotate:", dc_print_op);
    M(PDFDocument, "dealloc", dc_dealloc);

    M(PDFPage, "init", pg_init); M(PDFPage, "initWithImage:", pg_init_image); M(PDFPage, "document", pg_document);
    M(PDFPage, "pageRef", pg_ref); M(PDFPage, "label", pg_label); M(PDFPage, "boundsForBox:", pg_bounds);
    M(PDFPage, "setBounds:forBox:", pg_set_bounds); M(PDFPage, "rotation", pg_rotation); M(PDFPage, "setRotation:", pg_set_rotation);
    M(PDFPage, "annotations", pg_annotations); M(PDFPage, "addAnnotation:", pg_add_annot); M(PDFPage, "removeAnnotation:", pg_remove_annot);
    M(PDFPage, "annotationAtPoint:", pg_annot_at); M(PDFPage, "displaysAnnotations", pg_disp_annots);
    M(PDFPage, "setDisplaysAnnotations:", pg_set_disp_annots); M(PDFPage, "drawWithBox:toContext:", pg_draw_ctx);
    M(PDFPage, "drawWithBox:", pg_draw_box); M(PDFPage, "transformContext:forBox:", pg_transform);
    M(PDFPage, "transformContextForBox:", pg_draw_box);
    M(PDFPage, "thumbnailOfSize:forBox:", pg_thumbnail); M(PDFPage, "numberOfCharacters", pg_nchars);
    M(PDFPage, "string", pg_string); M(PDFPage, "attributedString", pg_attr_string);
    M(PDFPage, "characterBoundsAtIndex:", pg_char_bounds); M(PDFPage, "characterIndexAtPoint:", pg_char_index);
    M(PDFPage, "selectionForRange:", pg_sel_range); M(PDFPage, "selectionForRect:", pg_sel_rect);
    M(PDFPage, "selectionFromPoint:toPoint:", pg_sel_points); M(PDFPage, "selectionForWordAtPoint:", pg_sel_word);
    M(PDFPage, "selectionForLineAtPoint:", pg_sel_line); M(PDFPage, "dataRepresentation", pg_data);
    M(PDFPage, "copyWithZone:", pg_copy); M(PDFPage, "dealloc", pg_dealloc);

    M(PDFSelection, "initWithDocument:", sl_init); M(PDFSelection, "pages", sl_pages); M(PDFSelection, "string", sl_string);
    M(PDFSelection, "attributedString", sl_attr); M(PDFSelection, "boundsForPage:", sl_bounds);
    M(PDFSelection, "numberOfTextRangesOnPage:", sl_nranges); M(PDFSelection, "rangeAtIndex:onPage:", sl_range);
    M(PDFSelection, "selectionsByLine", sl_by_line); M(PDFSelection, "addSelection:", sl_add); M(PDFSelection, "addSelections:", sl_add_many);
    M(PDFSelection, "extendSelectionAtEnd:", sl_extend_end); M(PDFSelection, "extendSelectionAtStart:", sl_extend_start);
    M(PDFSelection, "extendSelectionForLineBoundaries", sl_extend_lines); M(PDFSelection, "color", sl_color);
    M(PDFSelection, "setColor:", sl_set_color); M(PDFSelection, "copyWithZone:", sl_copy);
    M(PDFSelection, "drawForPage:active:", sl_draw); M(PDFSelection, "drawForPage:withBox:active:", sl_draw);
    M(PDFSelection, "dealloc", sl_dealloc);

    M(PDFAnnotation, "initWithBounds:forType:withProperties:", an_init); M(PDFAnnotation, "initWithBounds:", an_init_old);
    M(PDFAnnotation, "page", an_page); M(PDFAnnotation, "setPage:", an_set_page);
    M(PDFAnnotation, "type", an_type); M(PDFAnnotation, "setType:", an_set_type);
    M(PDFAnnotation, "bounds", an_bounds); M(PDFAnnotation, "setBounds:", an_set_bounds);
    M(PDFAnnotation, "contents", an_contents); M(PDFAnnotation, "setContents:", an_set_contents);
    M(PDFAnnotation, "color", an_color); M(PDFAnnotation, "setColor:", an_set_color);
    M(PDFAnnotation, "interiorColor", an_interior); M(PDFAnnotation, "setInteriorColor:", an_set_interior);
    M(PDFAnnotation, "URL", an_url); M(PDFAnnotation, "setURL:", an_set_url);
    M(PDFAnnotation, "action", an_action); M(PDFAnnotation, "setAction:", an_set_action);
    M(PDFAnnotation, "userName", an_user); M(PDFAnnotation, "setUserName:", an_set_user);
    M(PDFAnnotation, "border", an_border); M(PDFAnnotation, "setBorder:", an_set_border);
    M(PDFAnnotation, "destination", an_dest); M(PDFAnnotation, "setDestination:", an_set_dest);
    M(PDFAnnotation, "fieldName", an_name); M(PDFAnnotation, "setFieldName:", an_set_name);
    M(PDFAnnotation, "font", an_font); M(PDFAnnotation, "setFont:", an_set_font);
    M(PDFAnnotation, "fontColor", an_fontcolor); M(PDFAnnotation, "setFontColor:", an_set_fontcolor);
    M(PDFAnnotation, "popup", an_popup); M(PDFAnnotation, "setPopup:", an_set_popup);
    M(PDFAnnotation, "shouldDisplay", an_display); M(PDFAnnotation, "setShouldDisplay:", an_set_display);
    M(PDFAnnotation, "shouldPrint", an_print); M(PDFAnnotation, "setShouldPrint:", an_set_print);
    M(PDFAnnotation, "isOpen", an_isopen); M(PDFAnnotation, "setOpen:", an_set_open);
    M(PDFAnnotation, "isHighlighted", an_highlighted); M(PDFAnnotation, "setHighlighted:", an_set_highlighted);
    M(PDFAnnotation, "hasAppearanceStream", an_has_ap);
    M(PDFAnnotation, "quadrilateralPoints", an_quad); M(PDFAnnotation, "setQuadrilateralPoints:", an_set_quad);
    M(PDFAnnotation, "paths", an_ink); M(PDFAnnotation, "addBezierPath:", an_add_ink); M(PDFAnnotation, "removeBezierPath:", an_remove_ink);
    M(PDFAnnotation, "valueForAnnotationKey:", an_value); M(PDFAnnotation, "setValue:forAnnotationKey:", an_set_value);
    M(PDFAnnotation, "setBoolean:forAnnotationKey:", an_set_bool); M(PDFAnnotation, "setRect:forAnnotationKey:", an_set_rect);
    M(PDFAnnotation, "removeValueForAnnotationKey:", an_remove_value); M(PDFAnnotation, "annotationKeyValues", an_annot_keys);
    M(PDFAnnotation, "drawWithBox:inContext:", an_draw); M(PDFAnnotation, "dealloc", an_dealloc);

    M(PDFOutline, "init", ol_init); M(PDFOutline, "document", ol_doc); M(PDFOutline, "parent", ol_parent);
    M(PDFOutline, "numberOfChildren", ol_count); M(PDFOutline, "index", ol_index); M(PDFOutline, "childAtIndex:", ol_child);
    M(PDFOutline, "insertChild:atIndex:", ol_insert); M(PDFOutline, "removeFromParent", ol_remove);
    M(PDFOutline, "label", ol_label); M(PDFOutline, "setLabel:", ol_set_label); M(PDFOutline, "isOpen", ol_open);
    M(PDFOutline, "setIsOpen:", ol_set_open); M(PDFOutline, "destination", ol_dest); M(PDFOutline, "setDestination:", ol_set_dest);
    M(PDFOutline, "action", ol_action); M(PDFOutline, "setAction:", ol_set_action); M(PDFOutline, "dealloc", ol_dealloc);

    M(PDFDestination, "initWithPage:atPoint:", ds_init); M(PDFDestination, "page", ds_page); M(PDFDestination, "point", ds_point);
    M(PDFDestination, "zoom", ds_zoom); M(PDFDestination, "setZoom:", ds_set_zoom); M(PDFDestination, "compare:", ds_compare);
    M(PDFDestination, "dealloc", ds_dealloc);

    M(PDFAction, "type", ac_type); M(PDFAction, "dealloc", ac_dealloc);
    M(PDFActionURL, "initWithURL:", ac_url_init); M(PDFActionURL, "URL", ac_url); M(PDFActionURL, "setURL:", ac_set_url);
    M(PDFActionGoTo, "initWithDestination:", ac_goto_init); M(PDFActionGoTo, "destination", ac_dest); M(PDFActionGoTo, "setDestination:", ac_set_dest);
    M(PDFActionNamed, "initWithName:", ac_named_init); M(PDFActionNamed, "name", ac_name); M(PDFActionNamed, "setName:", ac_set_name);
    M(PDFActionResetForm, "init", ac_reset_init);
    M(PDFActionRemoteGoTo, "initWithPageIndex:atPoint:fileURL:", ac_remote_init); M(PDFActionRemoteGoTo, "URL", ac_url);

    M(PDFBorder, "init", bd_init); M(PDFBorder, "lineWidth", bd_lw); M(PDFBorder, "setLineWidth:", bd_set_lw);
    M(PDFBorder, "style", bd_style); M(PDFBorder, "setStyle:", bd_set_style); M(PDFBorder, "dashPattern", bd_dash);
    M(PDFBorder, "setDashPattern:", bd_set_dash); M(PDFBorder, "dealloc", bd_dealloc);
    (void)an_new_empty;
    cx_pdfkit_view_init();
}
