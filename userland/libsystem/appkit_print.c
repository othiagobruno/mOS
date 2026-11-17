/*
 * mOS libSystem — impressão do AppKit: NSPrintInfo completo, NSPrinter, NSPrintOperation, NSPrintPanel, NSPageLayout e
 * o Core Printing em C (PM*). Antes NSPrintInfo tinha papel e margens fixos e não existiam NSPrintOperation, NSPrinter
 * nem NSPageLayout.
 *
 * O mOS não tem driver de impressora: a impressora é a virtual "Salvar como PDF". Imprimir gera um PDF de verdade
 * (Core Graphics PDF da Mariana): cada página é a parte da view desenhada pelo AppKit fora da janela
 * (appkit_offscreen.c). A paginação (área útil, escala, automática/ajustar/recortar, centralizar) está em
 * appkit_print_core.c (teste userland/tests-appkit/print_host.c). A view pode paginar sozinha (knowsPageRange: +
 * rectForPage:) e recebe beginDocument/endDocument e beginPageInRect:atPlacement:/endPage.
 * Destinos: NSPrintSaveJob com NSPrintJobSavingURL (arquivo), NSPrintPreviewJob (abre o PDF), PDFOperationWithView:
 * (dados ou caminho) e o padrão, que pergunta onde salvar.
 * Registro: cx_appkit_print_init(), chamado do cx_appkit_input_init.
 */
#include "foundation.h"
#include "sys.h"
#include "libc.h"
#include "appkit_print_core.h"

typedef int OSStatus;
id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void objc_setAssociatedObject(id obj, const void *key, id value, u64 policy);
id objc_getAssociatedObject(id obj, const void *key);
id objc_msgSend_stret(id self, SEL op, ...);
extern id NSApp;
void free(void *p);

typedef struct { double x, y; } P;
typedef struct { double w, h; } S;
typedef struct { double x, y, w, h; } R;
typedef struct { u64 loc, len; } RG;

id CGDataConsumerCreateWithCFData(id data);
id CGDataConsumerCreateWithURL(id url);
void CGDataConsumerRelease(id c);
id CGPDFContextCreate(id consumer, const R *media, id info);
void CGPDFContextBeginPage(id c, id info);
void CGPDFContextEndPage(id c);
void CGPDFContextClose(id c);
void CGContextRelease(id c);
void CGContextSaveGState(id c);
void CGContextRestoreGState(id c);
void CGContextTranslateCTM(id c, double x, double y);
void CGContextScaleCTM(id c, double x, double y);
void cx_view_render_to_cg(id view, R r, id cg);

#define K(N) cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N); cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N);
K(NSPrintOperation) K(NSPrinter) K(NSPageLayout) K(_MOSPrintPanelTarget)

static cx_class_t *kls(const char *n) { return cx_class_named(n); }
static SEL sl(const char *n) { return sel_registerName(n); }
static BOOL responds(id o, const char *s) { return o && cx_responds(object_getClass(o), sl(s)); }
static R msg_rect(id o, const char *s) { return ((R (*)(id, SEL))objc_msgSend_stret)(o, sl(s)); }
static void m_set_frame(id v, R r) { ((void (*)(id, SEL, R))objc_msgSend)(v, sl("setFrame:"), r); }
static id num_d(double v) { return MSG(id, (id)kls("NSNumber"), "numberWithDouble:", v); }
static id num_i(i64 v) { return MSG(id, (id)kls("NSNumber"), "numberWithLongLong:", v); }
static const char *cs(id s) { return s && cx_is_str(s) ? cx_cstr(s) : ""; }
#define PDF_PRINTER "Salvar como PDF"

/* ================================================================ NSPrintInfo */
static char k_dict;
static id g_shared_pi;
static id pi_d(id self) {
    id d = objc_getAssociatedObject(self, &k_dict);
    if (d) return d;
    d = MSG0(id, MSG0(id, (id)kls("NSMutableDictionary"), "alloc"), "init");
    double w = 595, h = 842;   /* A4: o papel do Brasil */
    id psz = MSG(id, (id)kls("NSValue"), "valueWithSize:", ((S){w, h}));
    MSG(void, d, "setObject:forKey:", psz, cx_str("NSPaperSize"));
    MSG(void, d, "setObject:forKey:", cx_str("iso-a4"), cx_str("NSPaperName"));
    const char *mk[] = {"NSLeftMargin", "NSRightMargin", "NSTopMargin", "NSBottomMargin"};
    double mv[] = {72, 72, 90, 90};
    for (int i = 0; i < 4; i++) MSG(void, d, "setObject:forKey:", num_d(mv[i]), cx_str(mk[i]));
    MSG(void, d, "setObject:forKey:", num_i(0), cx_str("NSOrientation"));
    MSG(void, d, "setObject:forKey:", num_d(1), cx_str("NSScalingFactor"));
    MSG(void, d, "setObject:forKey:", num_i(PRN_AUTO), cx_str("NSHorizontalPagination"));
    MSG(void, d, "setObject:forKey:", num_i(PRN_AUTO), cx_str("NSVerticalPagination"));
    MSG(void, d, "setObject:forKey:", num_i(1), cx_str("NSHorizontallyCentered"));
    MSG(void, d, "setObject:forKey:", num_i(0), cx_str("NSVerticallyCentered"));
    MSG(void, d, "setObject:forKey:", num_i(1), cx_str("NSCopies"));
    MSG(void, d, "setObject:forKey:", num_i(1), cx_str("NSAllPages"));
    MSG(void, d, "setObject:forKey:", num_i(1), cx_str("NSFirstPage"));
    MSG(void, d, "setObject:forKey:", num_i(9999), cx_str("NSLastPage"));
    MSG(void, d, "setObject:forKey:", cx_str("NSPrintSpoolJob"), cx_str("NSJobDisposition"));
    MSG(void, d, "setObject:forKey:", cx_str(PDF_PRINTER), cx_str("NSPrinterName"));
    objc_setAssociatedObject(self, &k_dict, d, 1);
    objc_release(d);
    return d;
}
/* as chaves públicas (NSPrintPaperSize = "NSPaperSize"...) */
static id pget(id self, const char *k) { return MSG(id, pi_d(self), "objectForKey:", cx_str(k)); }
static void pset(id self, const char *k, id v) { if (v) MSG(void, pi_d(self), "setObject:forKey:", v, cx_str(k)); else MSG(void, pi_d(self), "removeObjectForKey:", cx_str(k)); }
static double pd(id self, const char *k, double def) { id v = pget(self, k); return v ? MSG0(double, v, "doubleValue") : def; }
static i64 pn(id self, const char *k, i64 def) { id v = pget(self, k); return v ? MSG0(i64, v, "longLongValue") : def; }
static id pi_shared(id k, SEL c) { (void)c; if (!g_shared_pi) g_shared_pi = MSG0(id, MSG0(id, k, "alloc"), "init"); return g_shared_pi; }
static void pi_set_shared(id k, SEL c, id pi) { (void)k; (void)c; objc_retain(pi); objc_release(g_shared_pi); g_shared_pi = pi; }
static id pi_init(id self, SEL c) { (void)c; pi_d(self); return self; }
static id pi_init_dict(id self, SEL c, id dict) { (void)c; id d = pi_d(self); if (dict) MSG(void, d, "addEntriesFromDictionary:", dict); return self; }
static id pi_dict(id self, SEL c) { (void)c; return pi_d(self); }
static id pi_copy(id self, SEL c, void *zone) { (void)c; (void)zone; return pi_init_dict(MSG0(id, (id)object_getClass(self), "alloc"), 0, pi_d(self)); }
static S pi_paper(id self, SEL c) {
    (void)c;
    id v = pget(self, "NSPaperSize");
    return v ? ((S (*)(id, SEL))objc_msgSend)(v, sl("sizeValue")) : (S){595, 842};
}
static void pi_set_paper(id self, SEL c, S z) { (void)c; pset(self, "NSPaperSize", MSG(id, (id)kls("NSValue"), "valueWithSize:", z)); }
static id pi_paper_name(id self, SEL c) { (void)c; return pget(self, "NSPaperName"); }
static void pi_set_paper_name(id self, SEL c, id n) {
    (void)c;
    pset(self, "NSPaperName", n);
    double w, h;
    if (n && prn_paper_size(cs(n), &w, &h)) {
        if (pn(self, "NSOrientation", 0) == 1) { double t = w; w = h; h = t; }
        pi_set_paper(self, 0, (S){w, h});
    }
}
static id pi_local_paper(id self, SEL c) {
    (void)c;
    const char *n = cs(pget(self, "NSPaperName"));
    if (!strcmp(n, "iso-a4") || !strcmp(n, "A4")) return cx_str("A4");
    if (!strcmp(n, "na-letter") || !strcmp(n, "Letter")) return cx_str("Carta (EUA)");
    if (!strcmp(n, "na-legal") || !strcmp(n, "Legal")) return cx_str("Of\xC3\xAD" "cio (EUA)");
    return n[0] ? cx_str(n) : cx_str("A4");
}
static i64 pi_orient(id self, SEL c) { (void)c; return pn(self, "NSOrientation", 0); }
static void pi_set_orient(id self, SEL c, i64 o) {
    (void)c;
    S z = pi_paper(self, 0);
    if ((o == 1 && z.h > z.w) || (o == 0 && z.w > z.h)) pi_set_paper(self, 0, (S){z.h, z.w});   /* deitado: troca os lados */
    pset(self, "NSOrientation", num_i(o));
}
#define MARGIN(get, set, key) \
    static double get(id s, SEL c) { (void)c; return pd(s, key, 72); } \
    static void set(id s, SEL c, double v) { (void)c; pset(s, key, num_d(v)); }
MARGIN(pi_left, pi_set_left, "NSLeftMargin") MARGIN(pi_right, pi_set_right, "NSRightMargin")
MARGIN(pi_top, pi_set_top, "NSTopMargin") MARGIN(pi_bottom, pi_set_bottom, "NSBottomMargin")
static i64 pi_hpag(id s, SEL c) { (void)c; return pn(s, "NSHorizontalPagination", PRN_AUTO); }
static void pi_set_hpag(id s, SEL c, i64 v) { (void)c; pset(s, "NSHorizontalPagination", num_i(v)); }
static i64 pi_vpag(id s, SEL c) { (void)c; return pn(s, "NSVerticalPagination", PRN_AUTO); }
static void pi_set_vpag(id s, SEL c, i64 v) { (void)c; pset(s, "NSVerticalPagination", num_i(v)); }
static BOOL pi_hcenter(id s, SEL c) { (void)c; return pn(s, "NSHorizontallyCentered", 1) != 0; }
static void pi_set_hcenter(id s, SEL c, BOOL v) { (void)c; pset(s, "NSHorizontallyCentered", num_i(v)); }
static BOOL pi_vcenter(id s, SEL c) { (void)c; return pn(s, "NSVerticallyCentered", 0) != 0; }
static void pi_set_vcenter(id s, SEL c, BOOL v) { (void)c; pset(s, "NSVerticallyCentered", num_i(v)); }
static double pi_scale(id s, SEL c) { (void)c; return pd(s, "NSScalingFactor", 1); }
static void pi_set_scale(id s, SEL c, double v) { (void)c; pset(s, "NSScalingFactor", num_d(v)); }
static id pi_job(id s, SEL c) { (void)c; return pget(s, "NSJobDisposition"); }
static void pi_set_job(id s, SEL c, id v) { (void)c; pset(s, "NSJobDisposition", v); }
static BOOL pi_sel_only(id s, SEL c) { (void)c; return pn(s, "NSPrintSelectionOnly", 0) != 0; }
static void pi_set_sel_only(id s, SEL c, BOOL v) { (void)c; pset(s, "NSPrintSelectionOnly", num_i(v)); }
static R pi_imageable(id s, SEL c) { (void)c; S z = pi_paper(s, 0); return (R){18, 18, z.w - 36, z.h - 36}; }
static id pi_settings(id s, SEL c) { (void)c; return pi_d(s); }
static void pi_noop(id s, SEL c) { (void)s; (void)c; }
static id pi_printer(id s, SEL c);
static void pi_set_printer(id s, SEL c, id p) { (void)c; if (p && responds(p, "name")) pset(s, "NSPrinterName", MSG0(id, p, "name")); }
static void *pi_pm(id s, SEL c) { (void)c; return pi_d(s); }   /* PMPrintSession/PMPageFormat/PMPrintSettings: o mesmo dicionário */

/* ================================================================ NSPrinter: a impressora virtual */
static id g_pdf_printer;
static id printer_pdf(void) {
    if (!g_pdf_printer) g_pdf_printer = cx_alloc_instance(&NSPrinter_c);
    return g_pdf_printer;
}
static id pr_names(id k, SEL c) { (void)k; (void)c; return MSG(id, (id)kls("NSArray"), "arrayWithObject:", cx_str(PDF_PRINTER)); }
static id pr_types(id k, SEL c) { (void)k; (void)c; return MSG(id, (id)kls("NSArray"), "arrayWithObject:", cx_str("PDF")); }
static id pr_with_name(id k, SEL c, id n) { (void)k; (void)c; return n && !strcmp(cs(n), PDF_PRINTER) ? printer_pdf() : 0; }
static id pr_with_type(id k, SEL c, id t) { (void)k; (void)c; (void)t; return printer_pdf(); }
static id pr_name(id s, SEL c) { (void)s; (void)c; return cx_str(PDF_PRINTER); }
static id pr_type(id s, SEL c) { (void)s; (void)c; return cx_str("PDF"); }
static i64 pr_level(id s, SEL c) { (void)s; (void)c; return 3; }
static id pr_desc(id s, SEL c) {
    (void)s; (void)c;
    id d = MSG0(id, (id)kls("NSMutableDictionary"), "dictionary");
    MSG(void, d, "setObject:forKey:", cx_str("NO"), cx_str("NSDeviceIsScreen"));
    MSG(void, d, "setObject:forKey:", cx_str("YES"), cx_str("NSDeviceIsPrinter"));
    id res = MSG(id, (id)kls("NSValue"), "valueWithSize:", ((S){300, 300}));
    MSG(void, d, "setObject:forKey:", res, cx_str("NSDeviceResolution"));
    return d;
}
static id pi_printer(id s, SEL c) { (void)s; (void)c; return printer_pdf(); }
static id pi_default_printer(id k, SEL c) { (void)k; (void)c; return printer_pdf(); }

/* ================================================================ painéis */
static void tg_ok(id s, SEL c, id x) { (void)s; (void)c; (void)x; MSG(void, NSApp, "stopModalWithCode:", (i64)1); }
static void tg_cancel(id s, SEL c, id x) { (void)s; (void)c; (void)x; MSG(void, NSApp, "stopModalWithCode:", (i64)0); }
static id g_tgt;
static id target(void) { if (!g_tgt) g_tgt = cx_alloc_instance(&_MOSPrintPanelTarget_c); return g_tgt; }
static id label(const char *t, double x, double y, double w, id cv) {
    id l = MSG(id, (id)kls("NSTextField"), "labelWithString:", cx_str(t));
    m_set_frame(l, (R){x, y, w, 18});
    MSG(void, cv, "addSubview:", l);
    return l;
}
static id field(const char *t, double x, double y, double w, id cv) {
    id f = MSG(id, (id)kls("NSTextField"), "textFieldWithString:", cx_str(t));
    m_set_frame(f, (R){x, y, w, 22});
    MSG(void, cv, "addSubview:", f);
    return f;
}
static id popup(const char *const *items, int n, int sel, double x, double y, double w, id cv) {
    id p = ((id (*)(id, SEL, R, BOOL))objc_msgSend)(MSG0(id, (id)kls("NSPopUpButton"), "alloc"), sl("initWithFrame:pullsDown:"), (R){x, y, w, 24}, NO);
    for (int i = 0; i < n; i++) MSG(void, p, "addItemWithTitle:", cx_str(items[i]));
    MSG(void, p, "selectItemAtIndex:", (i64)sel);
    MSG(void, cv, "addSubview:", p);
    objc_release(p);
    return p;
}
static const char *paper_titles[] = {"A4", "Carta (EUA)", "Of\xC3\xAD" "cio (EUA)", "A3", "A5"};
static const char *paper_names[] = {"iso-a4", "na-letter", "na-legal", "A3", "A5"};
static int paper_index(id pi) {
    const char *n = cs(pget(pi, "NSPaperName"));
    for (int i = 0; i < 5; i++) if (!strcmp(n, paper_names[i])) return i;
    if (!strcmp(n, "A4")) return 0;
    if (!strcmp(n, "Letter")) return 1;
    return 0;
}
static id make_panel(const char *title, double h) {
    id w = ((id (*)(id, SEL, R, u64, u64, BOOL))objc_msgSend)(MSG0(id, (id)kls("NSPanel"), "alloc"), sl("initWithContentRect:styleMask:backing:defer:"), (R){0, 0, 420, h}, (u64)1, (u64)2, NO);
    MSG(void, w, "setTitle:", cx_str(title));
    MSG(void, w, "setReleasedWhenClosed:", (BOOL)NO);
    return w;
}
static void buttons(id cv, const char *ok) {
    id b = MSG(id, (id)kls("NSButton"), "buttonWithTitle:target:action:", cx_str(ok), target(), sl("cxPrintOK:"));
    m_set_frame(b, (R){300, 14, 104, 24});
    MSG(void, b, "setKeyEquivalent:", cx_str("\r"));
    MSG(void, cv, "addSubview:", b);
    id c = MSG(id, (id)kls("NSButton"), "buttonWithTitle:target:action:", cx_str("Cancelar"), target(), sl("cxPrintCancel:"));
    m_set_frame(c, (R){190, 14, 104, 24});
    MSG(void, cv, "addSubview:", c);
}
static void apply_paper_orient(id pi, id paper_pop, id orient_pop) {
    i64 o = MSG0(i64, orient_pop, "indexOfSelectedItem");
    pi_set_orient(pi, 0, 0);
    pi_set_paper_name(pi, 0, cx_str(paper_names[MSG0(i64, paper_pop, "indexOfSelectedItem") % 5]));
    pi_set_orient(pi, 0, o);
}
/* NSPageLayout: papel, orientação e escala */
static i64 run_page_layout(id pi) {
    id w = make_panel("Configurar p\xC3\xA1gina", 190);
    id cv = MSG0(id, w, "contentView");
    label("Papel:", 20, 146, 90, cv);
    id pp = popup(paper_titles, 5, paper_index(pi), 120, 142, 260, cv);
    label("Orienta\xC3\xA7\xC3\xA3o:", 20, 112, 90, cv);
    const char *ori[] = {"Retrato", "Paisagem"};
    id op = popup(ori, 2, (int)pn(pi, "NSOrientation", 0), 120, 108, 260, cv);
    label("Escala (%):", 20, 78, 90, cv);
    char sc[16]; snprintf(sc, sizeof sc, "%d", (int)(pd(pi, "NSScalingFactor", 1) * 100 + 0.5));
    id sf = field(sc, 120, 74, 80, cv);
    buttons(cv, "OK");
    MSG0(void, w, "center");
    i64 r = MSG(i64, NSApp, "runModalForWindow:", w);
    if (r == 1) {
        apply_paper_orient(pi, pp, op);
        double s = MSG0(double, sf, "doubleValue");
        if (s >= 1 && s <= 1000) pi_set_scale(pi, 0, s / 100);
    }
    MSG(void, w, "orderOut:", (id)0);
    MSG0(void, w, "close");
    objc_release(w);
    return r == 1 ? 1 : 0;
}
static id pl_new(id k, SEL c) { (void)c; return objc_autorelease(cx_alloc_instance((cx_class_t *)k)); }
static i64 pl_run(id s, SEL c) { (void)s; (void)c; return run_page_layout(pi_shared((id)kls("NSPrintInfo"), 0)); }
static i64 pl_run_with(id s, SEL c, id pi) { (void)s; (void)c; return run_page_layout(pi ? pi : pi_shared((id)kls("NSPrintInfo"), 0)); }
static void pl_sheet(id s, SEL c, id pi, id win, id dl, SEL sel, void *ctx) {
    (void)c; (void)win;
    i64 r = pl_run_with(s, 0, pi);
    if (dl && sel) ((void (*)(id, SEL, id, i64, void *))objc_msgSend)(dl, sel, s, r, ctx);
}
static id pl_pi(id s, SEL c) { (void)s; (void)c; return pi_shared((id)kls("NSPrintInfo"), 0); }

/* NSPrintPanel: impressora, cópias, páginas, papel, orientação, escala; acessórios do app */
static char k_acc, k_opts, k_help, k_job;
static i64 run_print_panel(id panel, id pi, int npages) {
    id accs = panel ? objc_getAssociatedObject(panel, &k_acc) : 0;
    u64 na = accs ? MSG0(u64, accs, "count") : 0;
    double acc_h = 0;
    for (u64 i = 0; i < na; i++) { id v = MSG0(id, MSG(id, accs, "objectAtIndex:", i), "view"); if (v) acc_h += msg_rect(v, "frame").h + 8; }
    double H = 270 + acc_h;
    id w = make_panel("Imprimir", H);
    id cv = MSG0(id, w, "contentView");
    double y = H - 40;
    label("Impressora:", 20, y + 4, 90, cv);
    const char *prs[] = {PDF_PRINTER};
    popup(prs, 1, 0, 120, y, 260, cv);
    y -= 34;
    label("C\xC3\xB3pias:", 20, y + 4, 90, cv);
    char cp[16]; snprintf(cp, sizeof cp, "%d", (int)pn(pi, "NSCopies", 1));
    id cf = field(cp, 120, y, 60, cv);
    y -= 34;
    label("P\xC3\xA1ginas:", 20, y + 4, 90, cv);
    id all = MSG(id, (id)kls("NSButton"), "radioButtonWithTitle:target:action:", cx_str("Todas"), (id)0, (SEL)0);
    m_set_frame(all, (R){120, y, 80, 22});
    MSG(void, all, "setState:", (i64)(pn(pi, "NSAllPages", 1) ? 1 : 0));
    MSG(void, cv, "addSubview:", all);
    label("de", 205, y + 4, 20, cv);
    char f1[16], f2[16];
    snprintf(f1, sizeof f1, "%d", (int)pn(pi, "NSFirstPage", 1));
    snprintf(f2, sizeof f2, "%d", npages > 0 ? npages : (int)pn(pi, "NSLastPage", 1));
    id ff = field(f1, 228, y, 50, cv);
    label("a", 284, y + 4, 14, cv);
    id lf = field(f2, 300, y, 50, cv);
    y -= 34;
    label("Papel:", 20, y + 4, 90, cv);
    id pp = popup(paper_titles, 5, paper_index(pi), 120, y, 260, cv);
    y -= 34;
    label("Orienta\xC3\xA7\xC3\xA3o:", 20, y + 4, 90, cv);
    const char *ori[] = {"Retrato", "Paisagem"};
    id op = popup(ori, 2, (int)pn(pi, "NSOrientation", 0), 120, y, 260, cv);
    y -= 34;
    label("Escala (%):", 20, y + 4, 90, cv);
    char sc[16]; snprintf(sc, sizeof sc, "%d", (int)(pd(pi, "NSScalingFactor", 1) * 100 + 0.5));
    id sf = field(sc, 120, y, 60, cv);
    y -= 8;
    for (u64 i = 0; i < na; i++) {
        id v = MSG0(id, MSG(id, accs, "objectAtIndex:", i), "view");
        if (!v) continue;
        R f = msg_rect(v, "frame");
        y -= f.h;
        m_set_frame(v, (R){20, y, f.w, f.h});
        MSG(void, cv, "addSubview:", v);
        y -= 8;
    }
    buttons(cv, "Salvar PDF\xE2\x80\xA6");
    MSG0(void, w, "center");
    i64 r = MSG(i64, NSApp, "runModalForWindow:", w);
    if (r == 1) {
        i64 copies = MSG0(i64, cf, "integerValue");
        pset(pi, "NSCopies", num_i(copies > 0 ? copies : 1));
        i64 a = MSG0(i64, ff, "integerValue"), b = MSG0(i64, lf, "integerValue");
        BOOL use_all = MSG0(i64, all, "state") == 1 && a <= 1 && (npages <= 0 || b >= npages);
        pset(pi, "NSAllPages", num_i(use_all));
        pset(pi, "NSFirstPage", num_i(a > 0 ? a : 1));
        pset(pi, "NSLastPage", num_i(b > 0 ? b : 9999));
        apply_paper_orient(pi, pp, op);
        double s = MSG0(double, sf, "doubleValue");
        if (s >= 1 && s <= 1000) pi_set_scale(pi, 0, s / 100);
    }
    for (u64 i = 0; i < na; i++) { id v = MSG0(id, MSG(id, accs, "objectAtIndex:", i), "view"); if (v) MSG0(void, v, "removeFromSuperview"); }
    MSG(void, w, "orderOut:", (id)0);
    MSG0(void, w, "close");
    objc_release(w);
    return r == 1 ? 1 : 0;
}
static id pp_new(id k, SEL c) { (void)c; return objc_autorelease(cx_alloc_instance((cx_class_t *)k)); }
static i64 pp_run(id s, SEL c) { (void)c; return run_print_panel(s, pi_shared((id)kls("NSPrintInfo"), 0), 0); }
static i64 pp_run_with(id s, SEL c, id pi) { (void)c; return run_print_panel(s, pi ? pi : pi_shared((id)kls("NSPrintInfo"), 0), 0); }
static void pp_sheet(id s, SEL c, id pi, id win, id dl, SEL sel, void *ctx) {
    (void)c; (void)win;
    i64 r = pp_run_with(s, 0, pi);
    if (dl && sel) ((void (*)(id, SEL, id, i64, void *))objc_msgSend)(dl, sel, s, r, ctx);
}
static void pp_add_acc(id s, SEL c, id ctl) {
    (void)c;
    id a = objc_getAssociatedObject(s, &k_acc);
    if (!a) { a = MSG0(id, (id)kls("NSMutableArray"), "array"); objc_setAssociatedObject(s, &k_acc, a, 1); }
    if (ctl) MSG(void, a, "addObject:", ctl);
}
static void pp_remove_acc(id s, SEL c, id ctl) { (void)c; id a = objc_getAssociatedObject(s, &k_acc); if (a && ctl) MSG(void, a, "removeObject:", ctl); }
static id pp_accs(id s, SEL c) { (void)c; id a = objc_getAssociatedObject(s, &k_acc); return a ? a : MSG0(id, (id)kls("NSArray"), "array"); }
static u64 pp_opts(id s, SEL c) { (void)c; id v = objc_getAssociatedObject(s, &k_opts); return v ? MSG0(u64, v, "unsignedLongLongValue") : 0; }
static void pp_set_opts(id s, SEL c, u64 o) { (void)c; objc_setAssociatedObject(s, &k_opts, num_i((i64)o), 1); }
static id pp_help(id s, SEL c) { (void)c; return objc_getAssociatedObject(s, &k_help); }
static void pp_set_help(id s, SEL c, id h) { (void)c; objc_setAssociatedObject(s, &k_help, h, 1); }
static id pp_job(id s, SEL c) { (void)c; return objc_getAssociatedObject(s, &k_job); }
static void pp_set_job(id s, SEL c, id j) { (void)c; objc_setAssociatedObject(s, &k_job, j, 1); }
static id pp_pi(id s, SEL c) { (void)s; (void)c; return pi_shared((id)kls("NSPrintInfo"), 0); }

/* ================================================================ NSPrintOperation */
typedef struct op {
    struct op *next;
    id obj, view, pi, panel, title, data, path;
    R rect;               /* PDFOperation…insideRect: (w = 0: a view inteira) */
    BOOL shows_panel, shows_progress, pdf_only;
    i64 cur_page;
    RG range;
} op_t;
static op_t *g_ops;
static id g_current_op;
static BOOL g_printing;
static op_t *opd(id o) { for (op_t *p = g_ops; p; p = p->next) if (p->obj == o) return p; return 0; }
static void keep(id *slot, id v) { objc_retain(v); objc_release(*slot); *slot = v; }
static id op_make(id k, id view, id pi) {
    id o = cx_alloc_instance((cx_class_t *)k);
    op_t *p = calloc(1, sizeof *p);
    if (!p) return 0;
    p->obj = o;
    keep(&p->view, view);
    keep(&p->pi, pi ? pi_copy(pi, 0, 0) : pi_copy(pi_shared((id)kls("NSPrintInfo"), 0), 0, 0));
    objc_release(p->pi);
    p->shows_panel = YES; p->shows_progress = YES;
    p->next = g_ops; g_ops = p;
    return objc_autorelease(o);
}
static id op_with_view(id k, SEL c, id view) { (void)c; return op_make(k, view, 0); }
static id op_with_view_pi(id k, SEL c, id view, id pi) { (void)c; return op_make(k, view, pi); }
static id op_pdf_data(id k, SEL c, id view, R r, id data, id pi) {
    (void)c;
    id o = op_make(k, view, pi);
    op_t *p = opd(o);
    p->rect = r; p->pdf_only = YES; p->shows_panel = NO;
    keep(&p->data, data);
    return o;
}
static id op_pdf_data3(id k, SEL c, id view, R r, id data) { return op_pdf_data(k, c, view, r, data, 0); }
static id op_pdf_path(id k, SEL c, id view, R r, id path, id pi) {
    (void)c;
    id o = op_make(k, view, pi);
    op_t *p = opd(o);
    p->rect = r; p->pdf_only = YES; p->shows_panel = NO;
    keep(&p->path, path);
    return o;
}
static id op_current(id k, SEL c) { (void)k; (void)c; return g_current_op; }
static void op_set_current(id k, SEL c, id o) { (void)k; (void)c; g_current_op = o; }
static id op_view(id s, SEL c) { (void)c; op_t *p = opd(s); return p ? p->view : 0; }
static id op_pi(id s, SEL c) { (void)c; op_t *p = opd(s); return p ? p->pi : 0; }
static void op_set_pi(id s, SEL c, id pi) { (void)c; op_t *p = opd(s); if (p) keep(&p->pi, pi); }
static BOOL op_shows_panel(id s, SEL c) { (void)c; op_t *p = opd(s); return p && p->shows_panel; }
static void op_set_shows_panel(id s, SEL c, BOOL b) { (void)c; op_t *p = opd(s); if (p) p->shows_panel = b; }
static BOOL op_shows_progress(id s, SEL c) { (void)c; op_t *p = opd(s); return p && p->shows_progress; }
static void op_set_shows_progress(id s, SEL c, BOOL b) { (void)c; op_t *p = opd(s); if (p) p->shows_progress = b; }
static id op_panel(id s, SEL c) { (void)c; op_t *p = opd(s); if (!p) return 0; if (!p->panel) p->panel = objc_retain(pp_new((id)kls("NSPrintPanel"), 0)); return p->panel; }
static void op_set_panel(id s, SEL c, id v) { (void)c; op_t *p = opd(s); if (p) keep(&p->panel, v); }
static id op_title(id s, SEL c) {
    (void)c;
    op_t *p = opd(s);
    if (p && p->title) return p->title;
    id w = p && p->view ? MSG0(id, p->view, "window") : 0;
    id t = w ? MSG0(id, w, "title") : 0;
    return t && cs(t)[0] ? t : cx_str("Sem t\xC3\xADtulo");
}
static void op_set_title(id s, SEL c, id t) { (void)c; op_t *p = opd(s); if (p) keep(&p->title, t); }
static i64 op_cur_page(id s, SEL c) { (void)c; op_t *p = opd(s); return p ? p->cur_page : 0; }
static RG op_range(id s, SEL c) { (void)c; op_t *p = opd(s); return p ? p->range : (RG){0, 0}; }
static BOOL op_copying(id s, SEL c) { (void)s; (void)c; return NO; }
static BOOL op_yes(id s, SEL c) { (void)s; (void)c; return YES; }
static void op_noop_b(id s, SEL c, BOOL b) { (void)s; (void)c; (void)b; }
static void op_noop(id s, SEL c) { (void)s; (void)c; }
static i64 op_order(id s, SEL c) { (void)s; (void)c; return 1; }   /* NSAscendingPageOrder */
static id op_context(id s, SEL c) { (void)s; (void)c; return g_printing ? MSG0(id, (id)kls("NSGraphicsContext"), "currentContext") : 0; }
static BOOL gc_drawing_to_screen(id k, SEL c) { (void)k; (void)c; return !g_printing; }

/* páginas: a view decide (knowsPageRange: + rectForPage:) ou a paginação automática */
static int collect_pages(op_t *p, prn_page_t *pages, int cap) {
    id v = p->view;
    R b = p->rect.w > 0 ? p->rect : msg_rect(v, "bounds");
    S paper = pi_paper(p->pi, 0);
    prn_setup_t s = {paper.w, paper.h, pd(p->pi, "NSLeftMargin", 72), pd(p->pi, "NSRightMargin", 72), pd(p->pi, "NSTopMargin", 90),
                     pd(p->pi, "NSBottomMargin", 90), pd(p->pi, "NSScalingFactor", 1), (int)pn(p->pi, "NSHorizontalPagination", PRN_AUTO),
                     (int)pn(p->pi, "NSVerticalPagination", PRN_AUTO), (int)pn(p->pi, "NSHorizontallyCentered", 1), (int)pn(p->pi, "NSVerticallyCentered", 0)};
    if (p->pdf_only) {   /* PDFOperation: uma página do tamanho do retângulo */
        pages[0] = (prn_page_t){b.x, b.y, b.w, b.h, 0, 0, 1};
        return 1;
    }
    RG rg = {1, 0};
    if (responds(v, "knowsPageRange:") && ((BOOL (*)(id, SEL, RG *))objc_msgSend)(v, sl("knowsPageRange:"), &rg) && responds(v, "rectForPage:")) {
        int n = 0;
        for (u64 i = 0; i < rg.len && n < cap; i++) {
            R r = ((R (*)(id, SEL, i64))objc_msgSend_stret)(v, sl("rectForPage:"), (i64)(rg.loc + i));
            prn_setup_t one = s;
            one.hpag = PRN_CLIP; one.vpag = PRN_CLIP;
            prn_page_t pg;
            if (prn_paginate(&one, r.w, r.h, 1, &pg, 1) != 1) continue;
            pg.x = r.x; pg.y = r.y;
            pages[n++] = pg;
        }
        return n;
    }
    BOOL flipped = MSG0(BOOL, v, "isFlipped");
    int n = prn_paginate(&s, b.w, b.h, flipped, pages, cap);
    for (int i = 0; i < n; i++) { pages[i].x += b.x; pages[i].y += b.y; }
    return n;
}
static id choose_url(op_t *p) {
    const char *job = cs(pget(p->pi, "NSJobDisposition"));
    if (!strcmp(job, "NSPrintSaveJob")) { id u = pget(p->pi, "NSJobSavingURL"); if (u) return u; }
    if (!strcmp(job, "NSPrintPreviewJob")) {
        char path[512];
        snprintf(path, sizeof path, "/tmp/mos-print-%d.pdf", (int)getpid());
        return MSG(id, (id)kls("NSURL"), "fileURLWithPath:", cx_str(path));
    }
    id sp = MSG0(id, (id)kls("NSSavePanel"), "savePanel");
    id ext = MSG(id, (id)kls("NSArray"), "arrayWithObject:", cx_str("pdf"));
    MSG(void, sp, "setAllowedFileTypes:", ext);
    MSG(void, sp, "setNameFieldStringValue:", op_title(p->obj, 0));
    MSG(void, sp, "setTitle:", cx_str("Salvar como PDF"));
    if (MSG0(i64, sp, "runModal") != 1) return 0;
    return MSG0(id, sp, "URL");
}
static BOOL run_op(id self) {
    op_t *p = opd(self);
    if (!p || !p->view) return NO;
    prn_page_t pages[512];
    int n = collect_pages(p, pages, 512);
    if (p->shows_panel && !p->pdf_only) {
        if (!run_print_panel(op_panel(self, 0), p->pi, n)) return NO;
        n = collect_pages(p, pages, 512);   /* papel, escala ou orientação podem ter mudado */
    }
    int a = 1, b = n;
    if (!prn_clamp_range(n, (int)pn(p->pi, "NSAllPages", 1), (int)pn(p->pi, "NSFirstPage", 1), (int)pn(p->pi, "NSLastPage", 9999), &a, &b)) return NO;
    id url = 0, cons = 0;
    if (p->data) cons = CGDataConsumerCreateWithCFData(p->data);
    else if (p->path) { url = MSG(id, (id)kls("NSURL"), "fileURLWithPath:", p->path); cons = CGDataConsumerCreateWithURL(url); }
    else { url = choose_url(p); if (!url) return NO; cons = CGDataConsumerCreateWithURL(url); }
    if (!cons) return NO;
    S paper = pi_paper(p->pi, 0);
    R media = p->pdf_only ? (R){0, 0, pages[0].w, pages[0].h} : (R){0, 0, paper.w, paper.h};
    id pdf = CGPDFContextCreate(cons, &media, 0);
    if (!pdf) { CGDataConsumerRelease(cons); return NO; }
    id prev = g_current_op;
    g_current_op = self;
    g_printing = YES;
    p->range = (RG){(u64)a, (u64)(b - a + 1)};
    id v = p->view;
    if (responds(v, "beginDocument")) MSG0(void, v, "beginDocument");
    for (int i = a - 1; i < b; i++) {
        prn_page_t *pg = &pages[i];
        p->cur_page = i + 1;
        R vr = {pg->x, pg->y, pg->w, pg->h};
        if (responds(v, "beginPageInRect:atPlacement:")) ((void (*)(id, SEL, R, P))objc_msgSend)(v, sl("beginPageInRect:atPlacement:"), vr, (P){pg->ox, pg->oy});
        CGPDFContextBeginPage(pdf, 0);
        CGContextSaveGState(pdf);
        CGContextTranslateCTM(pdf, pg->ox, pg->oy);
        CGContextScaleCTM(pdf, pg->scale, pg->scale);
        cx_view_render_to_cg(v, vr, pdf);
        CGContextRestoreGState(pdf);
        if (responds(v, "drawPageBorderWithSize:")) ((void (*)(id, SEL, S))objc_msgSend)(v, sl("drawPageBorderWithSize:"), paper);
        CGPDFContextEndPage(pdf);
        if (responds(v, "endPage")) MSG0(void, v, "endPage");
    }
    if (responds(v, "endDocument")) MSG0(void, v, "endDocument");
    g_printing = NO;
    g_current_op = prev;
    CGPDFContextClose(pdf);
    CGContextRelease(pdf);
    CGDataConsumerRelease(cons);
    if (url && !strcmp(cs(pget(p->pi, "NSJobDisposition")), "NSPrintPreviewJob")) {
        id ws = MSG0(id, (id)kls("NSWorkspace"), "sharedWorkspace");
        if (ws) MSG(BOOL, ws, "openURL:", url);
    }
    return YES;
}
static BOOL op_run(id s, SEL c) { (void)c; return run_op(s); }
static void op_run_modal(id s, SEL c, id win, id dl, SEL sel, void *ctx) {
    (void)c; (void)win;
    BOOL ok = run_op(s);
    if (dl && sel) ((void (*)(id, SEL, id, BOOL, void *))objc_msgSend)(dl, sel, s, ok, ctx);
}

/* ================================================================ Core Printing (PM*) sobre o NSPrintInfo */
enum { kPMNoError = 0, kPMInvalidParameter = -50, kPMDuplexNone = 1, kPMPrinterIdle = 3 };
typedef struct { double top, left, bottom, right; } PMRect;
typedef struct { double h, v; } PMResolution;
static id new_pi(void) { return MSG0(id, MSG0(id, (id)kls("NSPrintInfo"), "alloc"), "init"); }
/* sessão, page format e print settings são NSPrintInfo (um objeto por criação) */
OSStatus PMCreateSession(id *out) { if (!out) return kPMInvalidParameter; *out = new_pi(); return 0; }
OSStatus PMCreatePrintSettings(id *out) { if (!out) return kPMInvalidParameter; *out = new_pi(); return 0; }
OSStatus PMCreatePageFormat(id *out) { if (!out) return kPMInvalidParameter; *out = new_pi(); return 0; }
OSStatus PMRetain(id o) { if (o) objc_retain(o); return 0; }
OSStatus PMRelease(id o) { if (o) objc_release(o); return 0; }
/* o erro da sessão fica no próprio objeto (PMSessionSetError/PMSessionError, como o app usa para cancelar) */
OSStatus PMSessionError(id s) { return s ? (OSStatus)pn(s, "_PMSessionError", 0) : kPMInvalidParameter; }
OSStatus PMSessionSetError(id s, OSStatus e) { if (!s) return kPMInvalidParameter; pset(s, "_PMSessionError", e ? num_i(e) : 0); return 0; }
OSStatus PMSessionDefaultPrintSettings(id s, id ps) { (void)s; if (!ps) return kPMInvalidParameter; MSG(void, pi_d(ps), "addEntriesFromDictionary:", pi_d(pi_shared((id)kls("NSPrintInfo"), 0))); return 0; }
OSStatus PMSessionDefaultPageFormat(id s, id pf) { return PMSessionDefaultPrintSettings(s, pf); }
OSStatus PMGetCopies(id ps, u32 *n) { if (!ps || !n) return kPMInvalidParameter; *n = (u32)pn(ps, "NSCopies", 1); return 0; }
OSStatus PMSetCopies(id ps, u32 n, u8 lock) { (void)lock; if (!ps) return kPMInvalidParameter; pset(ps, "NSCopies", num_i(n)); return 0; }
OSStatus PMGetCollate(id ps, u8 *c) { if (!ps || !c) return kPMInvalidParameter; *c = (u8)pn(ps, "NSMustCollate", 1); return 0; }
OSStatus PMGetDuplex(id ps, u32 *d) { if (!ps || !d) return kPMInvalidParameter; *d = kPMDuplexNone; return 0; }
OSStatus PMGetFirstPage(id ps, u32 *p) { if (!ps || !p) return kPMInvalidParameter; *p = (u32)pn(ps, "NSFirstPage", 1); return 0; }
OSStatus PMGetLastPage(id ps, u32 *p) { if (!ps || !p) return kPMInvalidParameter; *p = (u32)pn(ps, "NSLastPage", 9999); return 0; }
OSStatus PMSetPageRange(id ps, u32 a, u32 b) {
    if (!ps || a < 1 || b < a) return kPMInvalidParameter;
    pset(ps, "NSFirstPage", num_i(a)); pset(ps, "NSLastPage", num_i(b)); pset(ps, "NSAllPages", num_i(a == 1 && b >= 9999));
    pset(ps, "_PMMinPage", num_i(a)); pset(ps, "_PMMaxPage", num_i(b));
    return 0;
}
/* faixa permitida (a que o diálogo aceita): 1 até kPMPrintAllPages (0xFFFFFFFF) se o app não limitou */
OSStatus PMGetPageRange(id ps, u32 *mn, u32 *mx) {
    if (!ps || !mn || !mx) return kPMInvalidParameter;
    *mn = (u32)pn(ps, "_PMMinPage", 1);
    *mx = (u32)pn(ps, "_PMMaxPage", 0xFFFFFFFFll);
    return 0;
}
OSStatus PMPrintSettingsGetJobName(id ps, id *name) { if (!ps || !name) return kPMInvalidParameter; id n = pget(ps, "NSJobName"); *name = n ? n : cx_str("Sem t\xC3\xADtulo"); return 0; }
OSStatus PMPrintSettingsSetJobName(id ps, id name) { if (!ps) return kPMInvalidParameter; pset(ps, "NSJobName", name); return 0; }
OSStatus PMGetUnadjustedPaperRect(id pf, PMRect *r) {
    if (!pf || !r) return kPMInvalidParameter;
    S z = pi_paper(pf, 0);
    r->top = 0; r->left = 0; r->bottom = z.h; r->right = z.w;
    return 0;
}
OSStatus PMGetAdjustedPaperRect(id pf, PMRect *r) { return PMGetUnadjustedPaperRect(pf, r); }
/* área da página (o papel menos as margens do NSPrintInfo), em pontos, sem a escala */
OSStatus PMGetUnadjustedPageRect(id pf, PMRect *r) {
    if (!pf || !r) return kPMInvalidParameter;
    S z = pi_paper(pf, 0);
    r->top = pd(pf, "NSTopMargin", 90); r->left = pd(pf, "NSLeftMargin", 72);
    r->bottom = z.h - pd(pf, "NSBottomMargin", 90); r->right = z.w - pd(pf, "NSRightMargin", 72);
    return 0;
}
OSStatus PMGetAdjustedPageRect(id pf, PMRect *r) {
    int e = PMGetUnadjustedPageRect(pf, r);
    if (e) return e;
    double k = pi_scale(pf, 0);
    if (k > 0) { r->top /= k; r->left /= k; r->bottom /= k; r->right /= k; }
    return 0;
}
/* escala em porcentagem (100 = tamanho real); o NSPrintInfo guarda o fator */
OSStatus PMGetScale(id pf, double *pct) { if (!pf || !pct) return kPMInvalidParameter; *pct = pi_scale(pf, 0) * 100.0; return 0; }
OSStatus PMSetScale(id pf, double pct) { if (!pf || pct <= 0) return kPMInvalidParameter; pi_set_scale(pf, 0, pct / 100.0); return 0; }
/* um page format por papel que a impressora PDF aceita */
OSStatus PMSessionCreatePageFormatList(id s, id pr, id *list) {
    (void)s; (void)pr;
    if (!list) return kPMInvalidParameter;
    id a = MSG0(id, MSG0(id, (id)kls("NSMutableArray"), "alloc"), "init");
    for (int i = 0; i < 5; i++) {
        id pf = new_pi();
        pi_set_paper_name(pf, 0, cx_str(paper_names[i]));
        MSG(void, a, "addObject:", pf);
        objc_release(pf);
    }
    *list = a;
    return 0;
}
OSStatus PMSessionCreatePrinterList(id s, id *names, i64 *cur_index, id *cur) {
    (void)s;
    if (!names) return kPMInvalidParameter;
    *names = objc_retain(MSG(id, (id)kls("NSArray"), "arrayWithObject:", cx_str(PDF_PRINTER)));
    if (cur_index) *cur_index = 0;
    if (cur) *cur = printer_pdf();
    return 0;
}
OSStatus PMPaperCreateLocalizedName(id paper, id printer, id *name) { (void)printer; if (!name) return kPMInvalidParameter; *name = objc_retain(paper ? pi_local_paper(paper, 0) : cx_str("A4")); return 0; }
OSStatus PMServerCreatePrinterList(void *server, id *list) { (void)server; if (!list) return kPMInvalidParameter; *list = objc_retain(MSG(id, (id)kls("NSArray"), "arrayWithObject:", printer_pdf())); return 0; }
id PMPrinterGetName(id pr) { (void)pr; return cx_str(PDF_PRINTER); }
id PMPrinterGetLocation(id pr) { (void)pr; return cx_str("Este computador"); }
OSStatus PMPrinterGetMakeAndModelName(id pr, id *out) { (void)pr; if (!out) return kPMInvalidParameter; *out = cx_str("mOS PDF"); return 0; }
OSStatus PMPrinterGetState(id pr, u32 *st) { (void)pr; if (!st) return kPMInvalidParameter; *st = kPMPrinterIdle; return 0; }
u8 PMPrinterIsDefault(id pr) { (void)pr; return 1; }
u8 PMPrinterIsRemote(id pr, u8 *remote) { (void)pr; if (remote) *remote = 0; return 0; }
OSStatus PMPrinterGetMimeTypes(id pr, id ps, id *types) { (void)pr; (void)ps; if (!types) return kPMInvalidParameter; *types = MSG(id, (id)kls("NSArray"), "arrayWithObject:", cx_str("application/pdf")); return 0; }
OSStatus PMPrinterGetOutputResolution(id pr, id ps, PMResolution *r) { (void)pr; (void)ps; if (!r) return kPMInvalidParameter; r->h = r->v = 300; return 0; }
/* a impressora PDF grava em ~/Documents: esse é o destino dela */
OSStatus PMPrinterCopyDeviceURI(id pr, id *uri) {
    (void)pr;
    if (!uri) return kPMInvalidParameter;
    extern id NSHomeDirectory(void);
    id dir = MSG(id, NSHomeDirectory(), "stringByAppendingPathComponent:", cx_str("Documents"));
    *uri = objc_retain(MSG(id, (id)kls("NSURL"), "fileURLWithPath:isDirectory:", dir, (BOOL)YES));
    return 0;
}
OSStatus PMPrinterCopyHostName(id pr, id *host) {
    (void)pr;
    if (!host) return kPMInvalidParameter;
    id h = MSG0(id, MSG0(id, (id)kls("NSProcessInfo"), "processInfo"), "hostName");
    *host = objc_retain(h ? h : cx_str("localhost"));
    return 0;
}
OSStatus PMPrinterGetDriverCreator(id pr, u32 *creator) { (void)pr; if (!creator) return kPMInvalidParameter; *creator = ('m' << 24) | ('O' << 16) | ('S' << 8) | ' '; return 0; }
/* VersRec: NumVersion (maior BCD, menor/correção, estágio, revisão), país, versão curta e longa em Str255 */
typedef struct __attribute__((packed)) { u8 major, minor_bug, stage, rev; i16 country; u8 short_v[256], long_v[256]; } pm_vers;
static void pstr(u8 *d, const char *s, int cap) { int n = (int)strlen(s); if (n > cap - 1) n = cap - 1; d[0] = (u8)n; memcpy(d + 1, s, (size_t)n); }
OSStatus PMPrinterGetDriverReleaseInfo(id pr, pm_vers *v) {
    (void)pr;
    if (!v) return kPMInvalidParameter;
    memset(v, 0, sizeof *v);
    v->major = 0x01; v->minor_bug = 0x00; v->stage = 0x80; v->rev = 0;   /* 1.0 final */
    pstr(v->short_v, "1.0", 256);
    pstr(v->long_v, "1.0, mOS PDF", 256);
    return 0;
}
/* PMLanguageInfo: três Str32 (nível, versão, release) da linguagem da impressora; a do mOS é PDF 1.7 (pdf_write.c) */
typedef struct { u8 level[33], version[33], release[33]; } pm_lang;
OSStatus PMPrinterGetLanguageInfo(id pr, pm_lang *li) {
    (void)pr;
    if (!li) return kPMInvalidParameter;
    memset(li, 0, sizeof *li);
    pstr(li->level, "PDF", 33);
    pstr(li->version, "1.7", 33);
    pstr(li->release, "1", 33);
    return 0;
}
u8 PMPrinterIsFavorite(id pr) { (void)pr; return 1; }   /* a única impressora está sempre na lista */
OSStatus PMPrinterCopyDescriptionURL(id pr, id type, id *url) { (void)pr; (void)type; if (url) *url = 0; return kPMInvalidParameter; }   /* sem PPD */
/* imprimir um PDF pronto na impressora PDF: grava em ~/Documents com o nome do trabalho */
OSStatus PMPrinterPrintWithFile(id pr, id ps, id pf, id mime, id file) {
    (void)pr; (void)pf;
    if (!file) return kPMInvalidParameter;
    if (mime && strcmp(cs(mime), "application/pdf")) return kPMInvalidParameter;
    id name = 0;
    if (ps) PMPrintSettingsGetJobName(ps, &name);
    id home = MSG0(id, (id)kls("NSString"), "string");
    extern id NSHomeDirectory(void);
    home = NSHomeDirectory();
    char dst[1024];
    snprintf(dst, sizeof dst, "%s/Documents/%s.pdf", cs(home), name ? cs(name) : "Impresso");
    id fm = MSG0(id, (id)kls("NSFileManager"), "defaultManager");
    id src = MSG0(id, file, "path");
    id data = src ? MSG(id, (id)kls("NSData"), "dataWithContentsOfFile:", src) : 0;
    if (!data) return kPMInvalidParameter;
    (void)fm;
    return MSG(BOOL, data, "writeToFile:atomically:", cx_str(dst), (BOOL)YES) ? 0 : kPMInvalidParameter;
}
OSStatus PMWorkflowSubmitPDFWithSettings(id workflow, id ps, id pdf) {
    (void)workflow; (void)ps;
    id ws = MSG0(id, (id)kls("NSWorkspace"), "sharedWorkspace");
    return pdf && ws && MSG(BOOL, ws, "openURL:", pdf) ? 0 : kPMInvalidParameter;
}

#define ADD(cls, sel, fn) cx_add_method(cls, sel, (void *)(fn))
#define CADD(cls, sel, fn) cx_add_cmethod(cls, sel, (void *)(fn))
void cx_appkit_print_init(void) {
    cx_class_t *obj = kls("NSObject"), *pi = kls("NSPrintInfo"), *pp = kls("NSPrintPanel"), *gc = kls("NSGraphicsContext");
    if (!obj || !pi) return;
    cx_define_class(&_MOSPrintPanelTarget_c, &_MOSPrintPanelTarget_m, "_MOSPrintPanelTarget", obj, 0);
    ADD(&_MOSPrintPanelTarget_c, "cxPrintOK:", tg_ok); ADD(&_MOSPrintPanelTarget_c, "cxPrintCancel:", tg_cancel);
    CADD(pi, "sharedPrintInfo", pi_shared); CADD(pi, "setSharedPrintInfo:", pi_set_shared); CADD(pi, "defaultPrinter", pi_default_printer);
    ADD(pi, "init", pi_init); ADD(pi, "initWithDictionary:", pi_init_dict); ADD(pi, "dictionary", pi_dict); ADD(pi, "printSettings", pi_dict);
    ADD(pi, "copyWithZone:", pi_copy); ADD(pi, "paperSize", pi_paper); ADD(pi, "setPaperSize:", pi_set_paper);
    ADD(pi, "paperName", pi_paper_name); ADD(pi, "setPaperName:", pi_set_paper_name); ADD(pi, "localizedPaperName", pi_local_paper);
    ADD(pi, "orientation", pi_orient); ADD(pi, "setOrientation:", pi_set_orient);
    ADD(pi, "leftMargin", pi_left); ADD(pi, "setLeftMargin:", pi_set_left); ADD(pi, "rightMargin", pi_right); ADD(pi, "setRightMargin:", pi_set_right);
    ADD(pi, "topMargin", pi_top); ADD(pi, "setTopMargin:", pi_set_top); ADD(pi, "bottomMargin", pi_bottom); ADD(pi, "setBottomMargin:", pi_set_bottom);
    ADD(pi, "horizontalPagination", pi_hpag); ADD(pi, "setHorizontalPagination:", pi_set_hpag);
    ADD(pi, "verticalPagination", pi_vpag); ADD(pi, "setVerticalPagination:", pi_set_vpag);
    ADD(pi, "isHorizontallyCentered", pi_hcenter); ADD(pi, "setHorizontallyCentered:", pi_set_hcenter);
    ADD(pi, "isVerticallyCentered", pi_vcenter); ADD(pi, "setVerticallyCentered:", pi_set_vcenter);
    ADD(pi, "scalingFactor", pi_scale); ADD(pi, "setScalingFactor:", pi_set_scale);
    ADD(pi, "jobDisposition", pi_job); ADD(pi, "setJobDisposition:", pi_set_job);
    ADD(pi, "isSelectionOnly", pi_sel_only); ADD(pi, "setSelectionOnly:", pi_set_sel_only);
    ADD(pi, "imageablePageBounds", pi_imageable); ADD(pi, "printer", pi_printer); ADD(pi, "setPrinter:", pi_set_printer);
    ADD(pi, "setUpPrintOperationDefaultValues", pi_noop); ADD(pi, "updateFromPMPrintSettings", pi_noop); ADD(pi, "updateFromPMPageFormat", pi_noop);
    ADD(pi, "PMPrintSession", pi_pm); ADD(pi, "PMPageFormat", pi_pm); ADD(pi, "PMPrintSettings", pi_pm);
    ADD(pi, "printSettings", pi_settings);
    cx_define_class(&NSPrinter_c, &NSPrinter_m, "NSPrinter", obj, 0);
    CADD(&NSPrinter_c, "printerNames", pr_names); CADD(&NSPrinter_c, "printerTypes", pr_types);
    CADD(&NSPrinter_c, "printerWithName:", pr_with_name); CADD(&NSPrinter_c, "printerWithType:", pr_with_type);
    ADD(&NSPrinter_c, "name", pr_name); ADD(&NSPrinter_c, "type", pr_type); ADD(&NSPrinter_c, "languageLevel", pr_level);
    ADD(&NSPrinter_c, "deviceDescription", pr_desc);
    cx_define_class(&NSPageLayout_c, &NSPageLayout_m, "NSPageLayout", obj, 0);
    CADD(&NSPageLayout_c, "pageLayout", pl_new);
    ADD(&NSPageLayout_c, "runModal", pl_run); ADD(&NSPageLayout_c, "runModalWithPrintInfo:", pl_run_with);
    ADD(&NSPageLayout_c, "beginSheetWithPrintInfo:modalForWindow:delegate:didEndSelector:contextInfo:", pl_sheet);
    ADD(&NSPageLayout_c, "printInfo", pl_pi);
    if (pp) {
        CADD(pp, "printPanel", pp_new);
        ADD(pp, "runModal", pp_run); ADD(pp, "runModalWithPrintInfo:", pp_run_with);
        ADD(pp, "beginSheetWithPrintInfo:modalForWindow:delegate:didEndSelector:contextInfo:", pp_sheet);
        ADD(pp, "addAccessoryController:", pp_add_acc); ADD(pp, "removeAccessoryController:", pp_remove_acc); ADD(pp, "accessoryControllers", pp_accs);
        ADD(pp, "options", pp_opts); ADD(pp, "setOptions:", pp_set_opts); ADD(pp, "helpAnchor", pp_help); ADD(pp, "setHelpAnchor:", pp_set_help);
        ADD(pp, "jobStyleHint", pp_job); ADD(pp, "setJobStyleHint:", pp_set_job); ADD(pp, "printInfo", pp_pi);
    }
    cx_define_class(&NSPrintOperation_c, &NSPrintOperation_m, "NSPrintOperation", obj, 0);
    cx_class_t *o = &NSPrintOperation_c;
    CADD(o, "printOperationWithView:", op_with_view); CADD(o, "printOperationWithView:printInfo:", op_with_view_pi);
    CADD(o, "PDFOperationWithView:insideRect:toData:", op_pdf_data3); CADD(o, "PDFOperationWithView:insideRect:toData:printInfo:", op_pdf_data);
    CADD(o, "PDFOperationWithView:insideRect:toPath:printInfo:", op_pdf_path);
    CADD(o, "currentOperation", op_current); CADD(o, "setCurrentOperation:", op_set_current);
    ADD(o, "runOperation", op_run); ADD(o, "runOperationModalForWindow:delegate:didRunSelector:contextInfo:", op_run_modal);
    ADD(o, "view", op_view); ADD(o, "printInfo", op_pi); ADD(o, "setPrintInfo:", op_set_pi);
    ADD(o, "showsPrintPanel", op_shows_panel); ADD(o, "setShowsPrintPanel:", op_set_shows_panel);
    ADD(o, "showsProgressPanel", op_shows_progress); ADD(o, "setShowsProgressPanel:", op_set_shows_progress);
    ADD(o, "printPanel", op_panel); ADD(o, "setPrintPanel:", op_set_panel);
    ADD(o, "jobTitle", op_title); ADD(o, "setJobTitle:", op_set_title);
    ADD(o, "currentPage", op_cur_page); ADD(o, "pageRange", op_range); ADD(o, "isCopyingOperation", op_copying);
    ADD(o, "canSpawnSeparateThread", op_yes); ADD(o, "setCanSpawnSeparateThread:", op_noop_b);
    ADD(o, "cleanUpOperation", op_noop); ADD(o, "deliverResult", op_yes); ADD(o, "createContext", op_context); ADD(o, "context", op_context);
    ADD(o, "pageOrder", op_order);
    if (gc) CADD(gc, "currentContextDrawingToScreen", gc_drawing_to_screen);
}
