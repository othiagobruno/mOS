/*
 * mOS PDFKit — PDFView e PDFThumbnailView (NSView em C). As páginas são renderizadas pelo Core Graphics num bitmap
 * na escala da view (cache das últimas páginas) e desenhadas como NSImage; seleção de texto com o mouse (arrastar,
 * duplo clique palavra, triplo linha), links (destino interno ou URL, com o delegate PDFViewWillClickOnLink:withURL:),
 * rolagem, teclado, zoom, os quatro modos de exibição, copiar e as notificações do PDFKit.
 */
#include "foundation.h"
#include "libc.h"

typedef struct { double x, y; } P;
typedef struct { double w, h; } S;
typedef struct { P o; S s; } R;

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
id objc_msgSend_stret(id self, SEL op, ...);
void CGImageRelease(void *i);
void *cx_pdfkit_render_page(id page, long box, double scale, int *pw, int *ph);
int cx_pdfkit_selection_rects(id sel, id page, R **out);
void NSRectFill(R r);
void NSFrameRect(R r);

cx_class_t PDFView_c __asm__("OBJC_CLASS_$_PDFView");
cx_class_t PDFView_m __asm__("OBJC_METACLASS_$_PDFView");        /* exportado: apps herdam de PDFView */
cx_class_t PDFThumbnailView_c __asm__("OBJC_CLASS_$_PDFThumbnailView");
cx_class_t PDFThumbnailView_m __asm__("OBJC_METACLASS_$_PDFThumbnailView");

extern char pv_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } pv_const_t;
#define PVK(var, text) static pv_const_t var##_s = {pv_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; const void *var = &var##_s;
PVK(PDFViewDocumentChangedNotification, "PDFViewChangedDocument")
PVK(PDFViewChangedHistoryNotification, "PDFViewChangedHistory")
PVK(PDFViewPageChangedNotification, "PDFViewChangedPage")
PVK(PDFViewScaleChangedNotification, "PDFViewScaleChanged")
PVK(PDFViewAnnotationHitNotification, "PDFViewAnnotationHit")
PVK(PDFViewCopyPermissionNotification, "PDFViewCopyPermission")
PVK(PDFViewPrintPermissionNotification, "PDFViewPrintPermission")
PVK(PDFViewAnnotationWillHitNotification, "PDFViewAnnotationWillHit")
PVK(PDFViewSelectionChangedNotification, "PDFViewSelectionChanged")
PVK(PDFViewDisplayModeChangedNotification, "PDFViewDisplayModeChanged")
PVK(PDFViewDisplayBoxChangedNotification, "PDFViewDisplayBoxChanged")
PVK(PDFViewVisiblePagesChangedNotification, "PDFViewVisiblePagesChanged")
PVK(PDFThumbnailViewDocumentEditedNotification, "PDFThumbnailViewDocumentEdited")

#define GAP 10.0
#define CACHE 8
typedef struct { id page; double scale; long box; long rot; id image; } pv_img;
typedef struct {
    id doc, delegate, sel, highlights, bg;
    long mode, box, cur;
    double scale, minScale, maxScale, scroll;
    int autoscale, breaks, display_as_book, rtl;
    pv_img cache[CACHE]; int cnext;
    id anchor_page; P anchor;
    id history; long hpos;
} pv_t;
typedef struct { id pdfview; S thumb; long cols; id bg; double scroll; } tv_t;
static u32 g_voff;
#define PV(o) ((pv_t *)*(void **)((u8 *)(o) + g_voff))
#define TV(o) ((tv_t *)*(void **)((u8 *)(o) + g_voff))
static id cls(const char *n) { return (id)cx_class_named(n); }
static long acount(id a) { return a ? MSG0(long, a, "count") : 0; }
static void keep(id *slot, id v) { if (v) objc_retain(v); if (*slot) objc_release(*slot); *slot = v; }
static R vbounds(id v) { return ((R (*)(id, SEL))objc_msgSend_stret)(v, sel_registerName("bounds")); }
static R page_box(id page, long box) { return ((R (*)(id, SEL, long))objc_msgSend_stret)(page, sel_registerName("boundsForBox:"), box); }
static long page_rot(id page) { return MSG0(long, page, "rotation"); }
static void post(id name, id obj) { id nc = MSG0(id, cls("NSNotificationCenter"), "defaultCenter"); if (nc) MSG(void, nc, "postNotificationName:object:", name, obj); }
static void redraw(id v) { MSG(void, v, "setNeedsDisplay:", (BOOL)YES); }
static long npages(pv_t *s) { return s->doc ? MSG0(long, s->doc, "pageCount") : 0; }
static id page_at(pv_t *s, long i) { return s->doc && i >= 0 && i < npages(s) ? MSG(id, s->doc, "pageAtIndex:", (unsigned long)i) : 0; }
static long index_of(pv_t *s, id page) { return s->doc && page ? (long)MSG(unsigned long, s->doc, "indexForPage:", page) : -1; }
/* tamanho exibido da página (com rotação), em pontos */
static S page_size(pv_t *s, id page) {
    R b = page_box(page, s->box);
    long r = page_rot(page);
    S z = {b.s.w, b.s.h};
    if (r == 90 || r == 270) { z.w = b.s.h; z.h = b.s.w; }
    return z;
}
static int two_up(pv_t *s) { return s->mode == 2 || s->mode == 3; }
static int continuous(pv_t *s) { return s->mode == 1 || s->mode == 3; }
/* páginas que aparecem: todas (contínuo) ou a atual (e a vizinha no 2 por vez) */
static void visible_range(pv_t *s, long *a, long *b) {
    long n = npages(s);
    if (continuous(s)) { *a = 0; *b = n; return; }
    long c = s->cur;
    if (two_up(s)) { long first = s->display_as_book ? (c == 0 ? 0 : ((c - 1) & ~1L) + 1) : (c & ~1L); *a = first; *b = first + ((s->display_as_book && first == 0) ? 1 : 2); }
    else { *a = c; *b = c + 1; }
    if (*b > n) *b = n;
}
/* retângulo de cada página na view (y para baixo; a view é invertida) */
static int layout(id v, pv_t *s, long i, R *out, double *total_h) {
    R vb = vbounds(v);
    long a, b;
    visible_range(s, &a, &b);
    double y = GAP, maxw = 0;
    int found = 0;
    for (long k = a; k < b;) {
        long k2 = two_up(s) ? k + ((s->display_as_book && k == 0) ? 1 : 2) : k + 1;
        if (k2 > b) k2 = b;
        double rowh = 0, roww = 0;
        for (long q = k; q < k2; q++) { S z = page_size(s, page_at(s, q)); roww += z.w * s->scale + (q > k ? GAP : 0); if (z.h * s->scale > rowh) rowh = z.h * s->scale; }
        double x = (vb.s.w - roww) / 2;
        if (x < GAP) x = GAP;
        if (two_up(s) && s->display_as_book && k == 0 && k2 - k == 1) x = vb.s.w / 2 + GAP / 2;
        for (long q = k; q < k2; q++) {
            S z = page_size(s, page_at(s, q));
            long qq = s->rtl && k2 - k == 2 ? (q == k ? k + 1 : k) : q;
            if (qq == i) { out->o.x = x; out->o.y = y + (rowh - z.h * s->scale) / 2; out->s.w = z.w * s->scale; out->s.h = z.h * s->scale; found = 1; }
            x += z.w * s->scale + GAP;
        }
        if (roww > maxw) maxw = roww;
        y += rowh + GAP;
        k = k2;
    }
    if (total_h) *total_h = y;
    return found;
}
/* ponto da página (espaço do PDF) ↔ view */
static P page_to_view(pv_t *s, id page, R pr, P p) {
    R b = page_box(page, s->box);
    double u = p.x - b.o.x, w = p.y - b.o.y, X, Y;
    long r = page_rot(page);
    if (r == 90) { X = w; Y = u; }
    else if (r == 180) { X = b.s.w - u; Y = w; }
    else if (r == 270) { X = b.s.h - w; Y = b.s.w - u; }
    else { X = u; Y = b.s.h - w; }
    P o = {pr.o.x + X * s->scale, pr.o.y + Y * s->scale - s->scroll};
    return o;
}
static P view_to_page(pv_t *s, id page, R pr, P v) {
    R b = page_box(page, s->box);
    double X = (v.x - pr.o.x) / s->scale, Y = (v.y + s->scroll - pr.o.y) / s->scale, u, w;
    long r = page_rot(page);
    if (r == 90) { w = X; u = Y; }
    else if (r == 180) { u = b.s.w - X; w = Y; }
    else if (r == 270) { w = b.s.h - X; u = b.s.w - Y; }
    else { u = X; w = b.s.h - Y; }
    P o = {b.o.x + u, b.o.y + w};
    return o;
}
static void clamp_scroll(id v, pv_t *s) {
    double total;
    R tmp;
    layout(v, s, -1, &tmp, &total);
    double maxs = total - vbounds(v).s.h;
    if (s->scroll > maxs) s->scroll = maxs;
    if (s->scroll < 0) s->scroll = 0;
}
static double fit_scale(id v, pv_t *s) {
    R vb = vbounds(v);
    double maxw = 1, maxh = 1;
    for (long i = 0; i < npages(s); i++) {
        S z = page_size(s, page_at(s, i));
        double w = two_up(s) ? z.w * 2 + GAP : z.w;
        if (w > maxw) maxw = w;
        if (z.h > maxh) maxh = z.h;
    }
    double sw = (vb.s.w - 2 * GAP) / maxw, sh = (vb.s.h - 2 * GAP) / maxh;
    double sc = continuous(s) ? sw : (sw < sh ? sw : sh);
    return sc > 0.05 ? sc : 0.05;
}
static void update_current(id v, pv_t *s) {
    if (!continuous(s)) return;
    R pr;
    long best = s->cur;
    for (long i = 0; i < npages(s); i++)
        if (layout(v, s, i, &pr, 0) && pr.o.y + pr.s.h > s->scroll + 1) { best = i; break; }
    if (best != s->cur) { s->cur = best; post((id)PDFViewPageChangedNotification, v); }
}
static id cached_image(pv_t *s, id page) {
    long rot = page_rot(page);
    for (int i = 0; i < CACHE; i++)
        if (s->cache[i].page == page && s->cache[i].scale == s->scale && s->cache[i].box == s->box && s->cache[i].rot == rot) return s->cache[i].image;
    int w, h;
    void *img = cx_pdfkit_render_page(page, s->box, s->scale, &w, &h);
    if (!img) return 0;
    id ni = MSG(id, MSG0(id, cls("NSImage"), "alloc"), "initWithCGImage:size:", (id)img, ((S){w, h}));
    CGImageRelease(img);
    pv_img *c = &s->cache[s->cnext];
    s->cnext = (s->cnext + 1) % CACHE;
    keep(&c->image, 0);
    if (c->page) objc_release(c->page);
    c->page = objc_retain(page); c->scale = s->scale; c->box = s->box; c->rot = rot; c->image = ni;
    return ni;
}
static void drop_cache(pv_t *s) {
    for (int i = 0; i < CACHE; i++) { keep(&s->cache[i].image, 0); if (s->cache[i].page) objc_release(s->cache[i].page); s->cache[i].page = 0; }
}

/* ---------------------------------------------------------------- desenho */
static void draw_selection(pv_t *s, id sel, id page, R pr, double r, double g, double b, double a) {
    R *rs;
    int n = cx_pdfkit_selection_rects(sel, page, &rs);
    if (!n) { free(rs); return; }
    MSG0(void, MSG(id, cls("NSColor"), "colorWithCalibratedRed:green:blue:alpha:", r, g, b, a), "set");
    for (int i = 0; i < n; i++) {
        P p0 = page_to_view(s, page, pr, rs[i].o), p1 = page_to_view(s, page, pr, (P){rs[i].o.x + rs[i].s.w, rs[i].o.y + rs[i].s.h});
        R vr = {{p0.x < p1.x ? p0.x : p1.x, p0.y < p1.y ? p0.y : p1.y}, {p1.x > p0.x ? p1.x - p0.x : p0.x - p1.x, p1.y > p0.y ? p1.y - p0.y : p0.y - p1.y}};
        NSRectFill(vr);
    }
    free(rs);
}
static void pv_draw(id self, SEL c, R dirty) {
    (void)c;
    pv_t *s = PV(self);
    R vb = vbounds(self);
    id bg = s->bg ? s->bg : MSG(id, cls("NSColor"), "colorWithCalibratedWhite:alpha:", 0.5, 1.0);
    MSG0(void, bg, "set");
    NSRectFill(vb);
    if (!s->doc) return;
    if (s->autoscale) { double f = fit_scale(self, s); if (f != s->scale) { s->scale = f; clamp_scroll(self, s); } }
    for (long i = 0; i < npages(s); i++) {
        R pr;
        if (!layout(self, s, i, &pr, 0)) continue;
        R onv = {{pr.o.x, pr.o.y - s->scroll}, pr.s};
        if (onv.o.y > vb.s.h || onv.o.y + onv.s.h < 0) continue;
        if (onv.o.y > dirty.o.y + dirty.s.h || onv.o.y + onv.s.h < dirty.o.y) continue;
        id page = page_at(s, i);
        if (s->breaks) {                                   /* sombra da folha */
            MSG0(void, MSG(id, cls("NSColor"), "colorWithCalibratedWhite:alpha:", 0.0, 0.25), "set");
            NSRectFill((R){{onv.o.x + 1, onv.o.y + 2}, onv.s});
        }
        id img = cached_image(s, page);
        if (img) MSG(void, img, "drawInRect:", onv);
        else { MSG0(void, MSG0(id, cls("NSColor"), "whiteColor"), "set"); NSRectFill(onv); }
        for (long k = 0; k < acount(s->highlights); k++) {
            id h = MSG(id, s->highlights, "objectAtIndex:", (unsigned long)k);
            id col = MSG0(id, h, "color");
            double rgba[4] = {1, 0.9, 0, 0.4};
            if (col) { id rgb = MSG(id, col, "colorUsingColorSpaceName:", cx_str("NSCalibratedRGBColorSpace")); if (rgb) { rgba[0] = MSG0(double, rgb, "redComponent"); rgba[1] = MSG0(double, rgb, "greenComponent"); rgba[2] = MSG0(double, rgb, "blueComponent"); } }
            draw_selection(s, h, page, pr, rgba[0], rgba[1], rgba[2], 0.4);
        }
        if (s->sel) draw_selection(s, s->sel, page, pr, 0.0, 0.4, 0.86, 0.3);
    }
}
static BOOL yes_imp(id self, SEL c) { (void)self; (void)c; return YES; }

/* ---------------------------------------------------------------- navegação */
static void push_history(pv_t *s) {
    if (!s->history) s->history = objc_retain(MSG0(id, cls("NSMutableArray"), "array"));
    while (acount(s->history) > s->hpos + 1) MSG0(void, s->history, "removeLastObject");
    id num = MSG(id, cls("NSNumber"), "numberWithLong:", s->cur);
    MSG(void, s->history, "addObject:", num);
    s->hpos = acount(s->history) - 1;
}
static void go_index(id self, long i, double y_in_page_from_top, int history) {
    pv_t *s = PV(self);
    if (i < 0 || i >= npages(s)) return;
    if (history) push_history(s);
    long old = s->cur;
    s->cur = i;
    R pr;
    if (continuous(s) && layout(self, s, i, &pr, 0)) s->scroll = pr.o.y - GAP + y_in_page_from_top * s->scale;
    else s->scroll = 0;
    clamp_scroll(self, s);
    if (old != i) post((id)PDFViewPageChangedNotification, self);
    if (history) post((id)PDFViewChangedHistoryNotification, self);
    redraw(self);
}
static id pv_document(id self, SEL c) { (void)c; return PV(self)->doc; }
static void pv_set_document(id self, SEL c, id doc) {
    (void)c;
    pv_t *s = PV(self);
    if (s->doc == doc) return;
    drop_cache(s);
    keep(&s->doc, doc);
    keep(&s->sel, 0);
    s->cur = 0; s->scroll = 0;
    if (s->history) MSG0(void, s->history, "removeAllObjects");
    post((id)PDFViewDocumentChangedNotification, self);
    post((id)PDFViewPageChangedNotification, self);
    redraw(self);
}
static id pv_current_page(id self, SEL c) { (void)c; update_current(self, PV(self)); return page_at(PV(self), PV(self)->cur); }
static void pv_go_page(id self, SEL c, id page) { (void)c; go_index(self, index_of(PV(self), page), 0, 1); }
static BOOL pv_can_next(id self, SEL c) { (void)c; pv_t *s = PV(self); return s->cur + (two_up(s) ? 2 : 1) < npages(s); }
static BOOL pv_can_prev(id self, SEL c) { (void)c; return PV(self)->cur > 0; }
static void pv_next(id self, SEL c, id sender) { (void)c; (void)sender; pv_t *s = PV(self); update_current(self, s); go_index(self, s->cur + (two_up(s) ? 2 : 1), 0, 0); }
static void pv_prev(id self, SEL c, id sender) { (void)c; (void)sender; pv_t *s = PV(self); update_current(self, s); go_index(self, s->cur - (two_up(s) ? 2 : 1) < 0 ? 0 : s->cur - (two_up(s) ? 2 : 1), 0, 0); }
static void pv_first(id self, SEL c, id sender) { (void)c; (void)sender; go_index(self, 0, 0, 1); }
static void pv_last(id self, SEL c, id sender) { (void)c; (void)sender; go_index(self, npages(PV(self)) - 1, 0, 1); }
static BOOL pv_can_first(id self, SEL c) { (void)c; return PV(self)->cur > 0; }
static BOOL pv_can_last(id self, SEL c) { (void)c; return PV(self)->cur < npages(PV(self)) - 1; }
static BOOL pv_can_back(id self, SEL c) { (void)c; return PV(self)->hpos > 0; }
static BOOL pv_can_fwd(id self, SEL c) { (void)c; return PV(self)->hpos + 1 < acount(PV(self)->history); }
static void pv_back(id self, SEL c, id sender) {
    (void)c; (void)sender;
    pv_t *s = PV(self);
    if (s->hpos <= 0) return;
    s->hpos--;
    go_index(self, MSG0(long, MSG(id, s->history, "objectAtIndex:", (unsigned long)s->hpos), "longValue"), 0, 0);
    post((id)PDFViewChangedHistoryNotification, self);
}
static void pv_fwd(id self, SEL c, id sender) {
    (void)c; (void)sender;
    pv_t *s = PV(self);
    if (s->hpos + 1 >= acount(s->history)) return;
    s->hpos++;
    go_index(self, MSG0(long, MSG(id, s->history, "objectAtIndex:", (unsigned long)s->hpos), "longValue"), 0, 0);
    post((id)PDFViewChangedHistoryNotification, self);
}
static void pv_go_dest(id self, SEL c, id dest) {
    (void)c;
    pv_t *s = PV(self);
    if (!dest) return;
    id page = MSG0(id, dest, "page");
    long i = index_of(s, page);
    if (i < 0) return;
    P pt = MSG0(P, dest, "point");
    R b = page_box(page, s->box);
    double from_top = b.o.y + b.s.h - pt.y;
    go_index(self, i, from_top > 0 && from_top < b.s.h ? from_top : 0, 1);
}
static id pv_current_dest(id self, SEL c) {
    (void)c;
    pv_t *s = PV(self);
    id page = page_at(s, s->cur);
    if (!page) return 0;
    R pr;
    P pt = {0, 0};
    if (layout(self, s, s->cur, &pr, 0)) pt = view_to_page(s, page, pr, (P){pr.o.x, 0});
    return objc_autorelease(MSG(id, MSG0(id, cls("PDFDestination"), "alloc"), "initWithPage:atPoint:", page, pt));
}
static void pv_go_sel(id self, SEL c, id sel) {
    (void)c;
    pv_t *s = PV(self);
    id pages = sel ? MSG0(id, sel, "pages") : 0;
    if (!acount(pages)) return;
    id page = MSG(id, pages, "objectAtIndex:", (unsigned long)0);
    R b = ((R (*)(id, SEL, id))objc_msgSend_stret)(sel, sel_registerName("boundsForPage:"), page);
    R pb = page_box(page, s->box);
    double from_top = pb.o.y + pb.s.h - (b.o.y + b.s.h) - 40;
    go_index(self, index_of(s, page), from_top > 0 ? from_top : 0, 1);
}
static void pv_go_rect(id self, SEL c, R r, id page) {
    (void)c;
    pv_t *s = PV(self);
    R pb = page_box(page, s->box);
    double from_top = pb.o.y + pb.s.h - (r.o.y + r.s.h);
    go_index(self, index_of(s, page), from_top > 0 ? from_top : 0, 1);
}

/* ---------------------------------------------------------------- escala e modo */
static double pv_scale(id self, SEL c) { (void)c; return PV(self)->scale; }
static void pv_set_scale(id self, SEL c, double v) {
    (void)c;
    pv_t *s = PV(self);
    if (v < s->minScale) v = s->minScale;
    if (v > s->maxScale) v = s->maxScale;
    if (v == s->scale) return;
    double ratio = v / s->scale;
    s->scale = v;
    s->scroll *= ratio;
    s->autoscale = 0;
    clamp_scroll(self, s);
    post((id)PDFViewScaleChangedNotification, self);
    redraw(self);
}
static BOOL pv_autoscales(id self, SEL c) { (void)c; return PV(self)->autoscale; }
static void pv_set_autoscales(id self, SEL c, BOOL v) { (void)c; PV(self)->autoscale = v; if (v) { PV(self)->scale = fit_scale(self, PV(self)); post((id)PDFViewScaleChangedNotification, self); } redraw(self); }
static double pv_fit(id self, SEL c) { (void)c; return fit_scale(self, PV(self)); }
static double pv_min(id self, SEL c) { (void)c; return PV(self)->minScale; }
static void pv_set_min(id self, SEL c, double v) { (void)c; PV(self)->minScale = v > 0.01 ? v : 0.01; }
static double pv_max(id self, SEL c) { (void)c; return PV(self)->maxScale; }
static void pv_set_max(id self, SEL c, double v) { (void)c; PV(self)->maxScale = v; }
static void pv_zoom_in(id self, SEL c, id sender) { (void)c; (void)sender; pv_set_scale(self, 0, PV(self)->scale * 1.25); }
static void pv_zoom_out(id self, SEL c, id sender) { (void)c; (void)sender; pv_set_scale(self, 0, PV(self)->scale / 1.25); }
static BOOL pv_can_in(id self, SEL c) { (void)c; return PV(self)->scale < PV(self)->maxScale; }
static BOOL pv_can_out(id self, SEL c) { (void)c; return PV(self)->scale > PV(self)->minScale; }
static long pv_mode(id self, SEL c) { (void)c; return PV(self)->mode; }
static void pv_set_mode(id self, SEL c, long m) { (void)c; pv_t *s = PV(self); if (m < 0 || m > 3) return; s->mode = m; go_index(self, s->cur, 0, 0); post((id)PDFViewDisplayModeChangedNotification, self); }
static long pv_box(id self, SEL c) { (void)c; return PV(self)->box; }
static void pv_set_box(id self, SEL c, long b) { (void)c; PV(self)->box = b; drop_cache(PV(self)); post((id)PDFViewDisplayBoxChangedNotification, self); redraw(self); }
static BOOL pv_breaks(id self, SEL c) { (void)c; return PV(self)->breaks; }
static void pv_set_breaks(id self, SEL c, BOOL v) { (void)c; PV(self)->breaks = v; redraw(self); }
static BOOL pv_book(id self, SEL c) { (void)c; return PV(self)->display_as_book; }
static void pv_set_book(id self, SEL c, BOOL v) { (void)c; PV(self)->display_as_book = v; redraw(self); }
static BOOL pv_rtl(id self, SEL c) { (void)c; return PV(self)->rtl; }
static void pv_set_rtl(id self, SEL c, BOOL v) { (void)c; PV(self)->rtl = v; redraw(self); }
static id pv_bg(id self, SEL c) { (void)c; return PV(self)->bg; }
static void pv_set_bg(id self, SEL c, id v) { (void)c; keep(&PV(self)->bg, v); redraw(self); }
static id pv_delegate(id self, SEL c) { (void)c; return PV(self)->delegate; }
static void pv_set_delegate(id self, SEL c, id v) { (void)c; PV(self)->delegate = v; }
static id pv_visible(id self, SEL c) {
    (void)c;
    pv_t *s = PV(self);
    id arr = MSG0(id, cls("NSMutableArray"), "array");
    R vb = vbounds(self);
    for (long i = 0; i < npages(s); i++) {
        R pr;
        if (layout(self, s, i, &pr, 0) && pr.o.y - s->scroll < vb.s.h && pr.o.y + pr.s.h - s->scroll > 0) MSG(void, arr, "addObject:", page_at(s, i));
    }
    return arr;
}
static id pv_doc_view(id self, SEL c) { (void)c; return self; }
static void pv_layout_doc(id self, SEL c) { (void)c; clamp_scroll(self, PV(self)); redraw(self); }

/* ---------------------------------------------------------------- conversões */
static id page_for_point(id self, pv_t *s, P p, int nearest, R *out_pr) {
    double bd = 1e30;
    id best = 0;
    for (long i = 0; i < npages(s); i++) {
        R pr;
        if (!layout(self, s, i, &pr, 0)) continue;
        double y0 = pr.o.y - s->scroll;
        if (p.x >= pr.o.x && p.x <= pr.o.x + pr.s.w && p.y >= y0 && p.y <= y0 + pr.s.h) { if (out_pr) *out_pr = pr; return page_at(s, i); }
        if (nearest) {
            double dx = p.x < pr.o.x ? pr.o.x - p.x : p.x > pr.o.x + pr.s.w ? p.x - pr.o.x - pr.s.w : 0;
            double dy = p.y < y0 ? y0 - p.y : p.y > y0 + pr.s.h ? p.y - y0 - pr.s.h : 0;
            if (dx * dx + dy * dy < bd) { bd = dx * dx + dy * dy; best = page_at(s, i); if (out_pr) *out_pr = pr; }
        }
    }
    return best;
}
static id pv_page_for_point(id self, SEL c, P p, BOOL nearest) { (void)c; return page_for_point(self, PV(self), p, nearest, 0); }
static P pv_to_page(id self, SEL c, P p, id page) {
    (void)c;
    pv_t *s = PV(self);
    R pr;
    if (!layout(self, s, index_of(s, page), &pr, 0)) return p;
    return view_to_page(s, page, pr, p);
}
static P pv_from_page(id self, SEL c, P p, id page) {
    (void)c;
    pv_t *s = PV(self);
    R pr;
    if (!layout(self, s, index_of(s, page), &pr, 0)) return p;
    return page_to_view(s, page, pr, p);
}
static R pv_rect_to_page(id self, SEL c, R r, id page) {
    P a = pv_to_page(self, c, r.o, page), b = pv_to_page(self, c, (P){r.o.x + r.s.w, r.o.y + r.s.h}, page);
    R o = {{a.x < b.x ? a.x : b.x, a.y < b.y ? a.y : b.y}, {a.x < b.x ? b.x - a.x : a.x - b.x, a.y < b.y ? b.y - a.y : a.y - b.y}};
    return o;
}
static R pv_rect_from_page(id self, SEL c, R r, id page) {
    P a = pv_from_page(self, c, r.o, page), b = pv_from_page(self, c, (P){r.o.x + r.s.w, r.o.y + r.s.h}, page);
    R o = {{a.x < b.x ? a.x : b.x, a.y < b.y ? a.y : b.y}, {a.x < b.x ? b.x - a.x : a.x - b.x, a.y < b.y ? b.y - a.y : a.y - b.y}};
    return o;
}

/* ---------------------------------------------------------------- seleção */
static id pv_sel(id self, SEL c) { (void)c; return PV(self)->sel; }
static void set_sel(id self, id sel) { keep(&PV(self)->sel, sel); post((id)PDFViewSelectionChangedNotification, self); redraw(self); }
static void pv_set_sel(id self, SEL c, id sel) { (void)c; set_sel(self, sel); }
static void pv_set_sel_anim(id self, SEL c, id sel, BOOL anim) { (void)c; (void)anim; set_sel(self, sel); }
static void pv_clear_sel(id self, SEL c) { (void)c; set_sel(self, 0); }
static void pv_select_all(id self, SEL c, id sender) { (void)c; (void)sender; pv_t *s = PV(self); if (s->doc) set_sel(self, MSG0(id, s->doc, "selectionForEntireDocument")); }
static id pv_highlights(id self, SEL c) { (void)c; return PV(self)->highlights; }
static void pv_set_highlights(id self, SEL c, id arr) { (void)c; keep(&PV(self)->highlights, arr); redraw(self); }
static void pv_copy(id self, SEL c, id sender) {
    (void)c; (void)sender;
    pv_t *s = PV(self);
    if (!s->sel) return;
    if (s->doc && !MSG0(BOOL, s->doc, "allowsCopying")) { post((id)PDFViewCopyPermissionNotification, self); return; }
    id str = MSG0(id, s->sel, "string");
    id pb = MSG0(id, cls("NSPasteboard"), "generalPasteboard");
    if (!str || !pb) return;
    MSG0(long, pb, "clearContents");
    MSG(BOOL, pb, "setString:forType:", str, cx_str("public.utf8-plain-text"));
}
static P event_point(id self, id ev) {
    P w = ((P (*)(id, SEL))objc_msgSend)(ev, sel_registerName("locationInWindow"));
    return ((P (*)(id, SEL, P, id))objc_msgSend)(self, sel_registerName("convertPoint:fromView:"), w, (id)0);
}
static void follow_annotation(id self, id annot) {
    pv_t *s = PV(self);
    post((id)PDFViewAnnotationHitNotification, self);
    id action = MSG0(id, annot, "action");
    id url = MSG0(id, annot, "URL");
    id dest = MSG0(id, annot, "destination");
    if (action && cx_responds(action->isa, sel_registerName("URL"))) url = MSG0(id, action, "URL");
    if (action && cx_responds(action->isa, sel_registerName("destination"))) dest = MSG0(id, action, "destination");
    if (action && cx_responds(action->isa, sel_registerName("name"))) {
        long n = MSG0(long, action, "name");               /* kPDFActionNamed*: 1 próxima, 2 anterior, 3 primeira, 4 última, 5 voltar, 6 avançar */
        if (n == 1) pv_next(self, 0, 0); else if (n == 2) pv_prev(self, 0, 0); else if (n == 3) pv_first(self, 0, 0);
        else if (n == 4) pv_last(self, 0, 0); else if (n == 5) pv_back(self, 0, 0); else if (n == 6) pv_fwd(self, 0, 0);
        else if (n == 7) pv_zoom_in(self, 0, 0); else if (n == 8) pv_zoom_out(self, 0, 0);
        return;
    }
    if (dest) { pv_go_dest(self, 0, dest); return; }
    if (url) {
        if (s->delegate && cx_responds(s->delegate->isa, sel_registerName("PDFViewWillClickOnLink:withURL:"))) MSG(void, s->delegate, "PDFViewWillClickOnLink:withURL:", self, url);
        else MSG(BOOL, MSG0(id, cls("NSWorkspace"), "sharedWorkspace"), "openURL:", url);
    }
}
static void pv_mouse_down(id self, SEL c, id ev) {
    (void)c;
    pv_t *s = PV(self);
    P v = event_point(self, ev);
    R pr;
    id page = page_for_point(self, s, v, 0, &pr);
    MSG(BOOL, MSG0(id, self, "window"), "makeFirstResponder:", self);
    if (!page) { set_sel(self, 0); return; }
    P pp = view_to_page(s, page, pr, v);
    id annot = MSG(id, page, "annotationAtPoint:", pp);
    if (annot) {
        id t = MSG0(id, annot, "type");
        if (t && !strcmp(cx_cstr(t), "Link")) { follow_annotation(self, annot); return; }
    }
    long clicks = MSG0(long, ev, "clickCount");
    keep(&s->anchor_page, page);
    s->anchor = pp;
    if (clicks == 2) set_sel(self, MSG(id, page, "selectionForWordAtPoint:", pp));
    else if (clicks >= 3) set_sel(self, MSG(id, page, "selectionForLineAtPoint:", pp));
    else set_sel(self, 0);
}
static void pv_mouse_dragged(id self, SEL c, id ev) {
    (void)c;
    pv_t *s = PV(self);
    if (!s->anchor_page || !s->doc) return;
    P v = event_point(self, ev);
    R vb = vbounds(self);
    if (v.y < 0 || v.y > vb.s.h) { s->scroll += v.y < 0 ? v.y : v.y - vb.s.h; clamp_scroll(self, s); }   /* rola arrastando */
    R pr;
    id page = page_for_point(self, s, v, 1, &pr);
    if (!page) return;
    P pp = view_to_page(s, page, pr, v);
    id sel = MSG(id, s->doc, "selectionFromPage:atPoint:toPage:atPoint:", s->anchor_page, s->anchor, page, pp);
    set_sel(self, sel);
}
static void pv_mouse_up(id self, SEL c, id ev) { (void)c; (void)ev; keep(&PV(self)->anchor_page, 0); }
static void pv_scroll_wheel(id self, SEL c, id ev) {
    (void)c;
    pv_t *s = PV(self);
    double dy = MSG0(double, ev, "scrollingDeltaY");
    if (!MSG0(BOOL, ev, "hasPreciseScrollingDeltas")) dy *= 12;
    if (MSG0(unsigned long, ev, "modifierFlags") & (1ul << 20)) {        /* ⌘ + roda: zoom */
        pv_set_scale(self, 0, s->scale * (dy > 0 ? 1.1 : 1 / 1.1));
        return;
    }
    if (!continuous(s)) {
        if (dy < -2 && pv_can_next(self, 0)) pv_next(self, 0, 0);
        else if (dy > 2 && pv_can_prev(self, 0)) pv_prev(self, 0, 0);
        return;
    }
    s->scroll -= dy;
    clamp_scroll(self, s);
    update_current(self, s);
    post((id)PDFViewVisiblePagesChangedNotification, self);
    redraw(self);
}
static void pv_key_down(id self, SEL c, id ev) {
    (void)c;
    pv_t *s = PV(self);
    unsigned short k = MSG0(unsigned short, ev, "keyCode");
    double page_h = vbounds(self).s.h - 40;
    if (k == 125) s->scroll += 40;
    else if (k == 126) s->scroll -= 40;
    else if (k == 121 || k == 49) s->scroll += page_h;
    else if (k == 116) s->scroll -= page_h;
    else if (k == 115) { pv_first(self, 0, 0); return; }
    else if (k == 119) { pv_last(self, 0, 0); return; }
    else if (k == 124 && !continuous(s)) { pv_next(self, 0, 0); return; }
    else if (k == 123 && !continuous(s)) { pv_prev(self, 0, 0); return; }
    else return;
    if (!continuous(s)) { if (k == 125 || k == 121 || k == 49) pv_next(self, 0, 0); else pv_prev(self, 0, 0); return; }
    clamp_scroll(self, s);
    update_current(self, s);
    redraw(self);
}
static void pv_resize(id self, SEL c, S old) {
    (void)c; (void)old;
    pv_t *s = PV(self);
    if (s->autoscale) { s->scale = fit_scale(self, s); post((id)PDFViewScaleChangedNotification, self); }
    clamp_scroll(self, s);
    redraw(self);
}
static id pv_init_frame(id self, SEL c, R f) {
    IMP sup = cx_lookup(PDFView_c.super, sel_registerName("initWithFrame:"));
    self = ((id (*)(id, SEL, R))sup)(self, c, f);
    if (!self) return 0;
    pv_t *s = calloc(1, sizeof *s);
    *(void **)((u8 *)self + g_voff) = s;
    s->scale = 1; s->minScale = 0.1; s->maxScale = 20; s->mode = 1; s->box = 1; s->breaks = 1; s->autoscale = 0;
    return self;
}
static id pv_init(id self, SEL c) { (void)c; return MSG(id, self, "initWithFrame:", ((R){{0, 0}, {100, 100}})); }
static void pv_dealloc(id self, SEL c) {
    pv_t *s = PV(self);
    if (s) { drop_cache(s); keep(&s->doc, 0); keep(&s->sel, 0); keep(&s->highlights, 0); keep(&s->bg, 0); keep(&s->anchor_page, 0); keep(&s->history, 0); free(s); }
    *(void **)((u8 *)self + g_voff) = 0;
    IMP sup = cx_lookup(PDFView_c.super, sel_registerName("dealloc"));
    if (sup) ((void (*)(id, SEL))sup)(self, c);
}

/* ---------------------------------------------------------------- PDFThumbnailView */
static id tv_init_frame(id self, SEL c, R f) {
    IMP sup = cx_lookup(PDFThumbnailView_c.super, sel_registerName("initWithFrame:"));
    self = ((id (*)(id, SEL, R))sup)(self, c, f);
    if (!self) return 0;
    tv_t *t = calloc(1, sizeof *t);
    *(void **)((u8 *)self + g_voff) = t;
    t->thumb.w = 80; t->thumb.h = 100; t->cols = 1;
    return self;
}
static id tv_init(id self, SEL c) { (void)c; return MSG(id, self, "initWithFrame:", ((R){{0, 0}, {100, 100}})); }
static id tv_view(id self, SEL c) { (void)c; return TV(self)->pdfview; }
static void tv_set_view(id self, SEL c, id v) { (void)c; TV(self)->pdfview = v; redraw(self); }
static S tv_size(id self, SEL c) { (void)c; return TV(self)->thumb; }
static void tv_set_size(id self, SEL c, S z) { (void)c; TV(self)->thumb = z; redraw(self); }
static unsigned long tv_cols(id self, SEL c) { (void)c; return (unsigned long)TV(self)->cols; }
static void tv_set_cols(id self, SEL c, unsigned long n) { (void)c; TV(self)->cols = n ? (long)n : 1; redraw(self); }
static id tv_bg(id self, SEL c) { (void)c; return TV(self)->bg; }
static void tv_set_bg(id self, SEL c, id v) { (void)c; keep(&TV(self)->bg, v); redraw(self); }
static id tv_selected(id self, SEL c) {
    (void)c;
    id v = TV(self)->pdfview;
    id arr = MSG0(id, cls("NSMutableArray"), "array");
    id p = v ? MSG0(id, v, "currentPage") : 0;
    if (p) MSG(void, arr, "addObject:", p);
    return arr;
}
static R thumb_rect(tv_t *t, long i) {
    long col = i % t->cols, row = i / t->cols;
    R r = {{GAP + col * (t->thumb.w + GAP), GAP + row * (t->thumb.h + GAP + 14) - t->scroll}, t->thumb};
    return r;
}
static void tv_draw(id self, SEL c, R dirty) {
    (void)c; (void)dirty;
    tv_t *t = TV(self);
    R vb = vbounds(self);
    MSG0(void, t->bg ? t->bg : MSG(id, cls("NSColor"), "colorWithCalibratedWhite:alpha:", 0.92, 1.0), "set");
    NSRectFill(vb);
    id v = t->pdfview, doc = v ? MSG0(id, v, "document") : 0;
    if (!doc) return;
    id cur = MSG0(id, v, "currentPage");
    long n = MSG0(long, doc, "pageCount");
    for (long i = 0; i < n; i++) {
        R r = thumb_rect(t, i);
        if (r.o.y > vb.s.h || r.o.y + r.s.h < 0) continue;
        id page = MSG(id, doc, "pageAtIndex:", (unsigned long)i);
        id img = MSG(id, page, "thumbnailOfSize:forBox:", t->thumb, (long)1);
        if (page == cur) { MSG0(void, MSG(id, cls("NSColor"), "colorWithCalibratedRed:green:blue:alpha:", 0.0, 0.4, 0.86, 1.0), "set"); NSRectFill((R){{r.o.x - 3, r.o.y - 3}, {r.s.w + 6, r.s.h + 6}}); }
        if (img) {
            S z = MSG0(S, img, "size");
            R ir = {{r.o.x + (r.s.w - z.w) / 2, r.o.y + (r.s.h - z.h) / 2}, z};
            MSG(void, img, "drawInRect:", ir);
        }
        id label = MSG0(id, page, "label");
        if (label) MSG(void, label, "drawAtPoint:withAttributes:", ((P){r.o.x + r.s.w / 2 - 6, r.o.y + r.s.h + 1}), (id)0);
    }
}
static void tv_mouse_down(id self, SEL c, id ev) {
    (void)c;
    tv_t *t = TV(self);
    id v = t->pdfview, doc = v ? MSG0(id, v, "document") : 0;
    if (!doc) return;
    P p = event_point(self, ev);
    long n = MSG0(long, doc, "pageCount");
    for (long i = 0; i < n; i++) {
        R r = thumb_rect(t, i);
        if (p.x >= r.o.x && p.x <= r.o.x + r.s.w && p.y >= r.o.y && p.y <= r.o.y + r.s.h) {
            id pg = MSG(id, doc, "pageAtIndex:", (unsigned long)i);
            MSG(void, v, "goToPage:", pg);
            redraw(self);
            return;
        }
    }
}
static void tv_scroll(id self, SEL c, id ev) {
    (void)c;
    tv_t *t = TV(self);
    double dy = MSG0(double, ev, "scrollingDeltaY");
    if (!MSG0(BOOL, ev, "hasPreciseScrollingDeltas")) dy *= 12;
    t->scroll -= dy;
    if (t->scroll < 0) t->scroll = 0;
    redraw(self);
}
static void tv_dealloc(id self, SEL c) {
    tv_t *t = TV(self);
    if (t) { keep(&t->bg, 0); free(t); }
    *(void **)((u8 *)self + g_voff) = 0;
    IMP sup = cx_lookup(PDFThumbnailView_c.super, sel_registerName("dealloc"));
    if (sup) ((void (*)(id, SEL))sup)(self, c);
}

#define M(k, s, f) cx_add_method(&k##_c, s, (void *)(f))
void cx_pdfkit_view_init(void) {
    cx_class_t *view = cx_class_named("NSView");
    if (!view || PDFView_c.vtable) return;
    g_voff = cx_instance_size(view);
    cx_define_class(&PDFView_c, &PDFView_m, "PDFView", view, sizeof(void *));
    cx_define_class(&PDFThumbnailView_c, &PDFThumbnailView_m, "PDFThumbnailView", view, sizeof(void *));
    M(PDFView, "initWithFrame:", pv_init_frame); M(PDFView, "init", pv_init); M(PDFView, "dealloc", pv_dealloc);
    M(PDFView, "drawRect:", pv_draw); M(PDFView, "isFlipped", yes_imp); M(PDFView, "acceptsFirstResponder", yes_imp);
    M(PDFView, "document", pv_document); M(PDFView, "setDocument:", pv_set_document);
    M(PDFView, "currentPage", pv_current_page); M(PDFView, "goToPage:", pv_go_page);
    M(PDFView, "canGoToNextPage", pv_can_next); M(PDFView, "canGoToPreviousPage", pv_can_prev);
    M(PDFView, "goToNextPage:", pv_next); M(PDFView, "goToPreviousPage:", pv_prev);
    M(PDFView, "goToFirstPage:", pv_first); M(PDFView, "goToLastPage:", pv_last);
    M(PDFView, "canGoToFirstPage", pv_can_first); M(PDFView, "canGoToLastPage", pv_can_last);
    M(PDFView, "canGoBack", pv_can_back); M(PDFView, "canGoForward", pv_can_fwd);
    M(PDFView, "goBack:", pv_back); M(PDFView, "goForward:", pv_fwd);
    M(PDFView, "goToDestination:", pv_go_dest); M(PDFView, "currentDestination", pv_current_dest);
    M(PDFView, "goToSelection:", pv_go_sel); M(PDFView, "goToRect:onPage:", pv_go_rect);
    M(PDFView, "scaleFactor", pv_scale); M(PDFView, "setScaleFactor:", pv_set_scale);
    M(PDFView, "autoScales", pv_autoscales); M(PDFView, "setAutoScales:", pv_set_autoscales);
    M(PDFView, "scaleFactorForSizeToFit", pv_fit); M(PDFView, "minScaleFactor", pv_min); M(PDFView, "setMinScaleFactor:", pv_set_min);
    M(PDFView, "maxScaleFactor", pv_max); M(PDFView, "setMaxScaleFactor:", pv_set_max);
    M(PDFView, "zoomIn:", pv_zoom_in); M(PDFView, "zoomOut:", pv_zoom_out); M(PDFView, "canZoomIn", pv_can_in); M(PDFView, "canZoomOut", pv_can_out);
    M(PDFView, "displayMode", pv_mode); M(PDFView, "setDisplayMode:", pv_set_mode);
    M(PDFView, "displayBox", pv_box); M(PDFView, "setDisplayBox:", pv_set_box);
    M(PDFView, "displaysPageBreaks", pv_breaks); M(PDFView, "setDisplaysPageBreaks:", pv_set_breaks);
    M(PDFView, "displaysAsBook", pv_book); M(PDFView, "setDisplaysAsBook:", pv_set_book);
    M(PDFView, "displaysRTL", pv_rtl); M(PDFView, "setDisplaysRTL:", pv_set_rtl);
    M(PDFView, "backgroundColor", pv_bg); M(PDFView, "setBackgroundColor:", pv_set_bg);
    M(PDFView, "delegate", pv_delegate); M(PDFView, "setDelegate:", pv_set_delegate);
    M(PDFView, "visiblePages", pv_visible); M(PDFView, "documentView", pv_doc_view); M(PDFView, "layoutDocumentView", pv_layout_doc);
    M(PDFView, "pageForPoint:nearest:", pv_page_for_point);
    M(PDFView, "convertPoint:toPage:", pv_to_page); M(PDFView, "convertPoint:fromPage:", pv_from_page);
    M(PDFView, "convertRect:toPage:", pv_rect_to_page); M(PDFView, "convertRect:fromPage:", pv_rect_from_page);
    M(PDFView, "currentSelection", pv_sel); M(PDFView, "setCurrentSelection:", pv_set_sel);
    M(PDFView, "setCurrentSelection:animate:", pv_set_sel_anim); M(PDFView, "clearSelection", pv_clear_sel);
    M(PDFView, "selectAll:", pv_select_all); M(PDFView, "copy:", pv_copy);
    M(PDFView, "highlightedSelections", pv_highlights); M(PDFView, "setHighlightedSelections:", pv_set_highlights);
    M(PDFView, "mouseDown:", pv_mouse_down); M(PDFView, "mouseDragged:", pv_mouse_dragged); M(PDFView, "mouseUp:", pv_mouse_up);
    M(PDFView, "scrollWheel:", pv_scroll_wheel); M(PDFView, "keyDown:", pv_key_down);
    M(PDFView, "resizeSubviewsWithOldSize:", pv_resize);
    M(PDFThumbnailView, "initWithFrame:", tv_init_frame); M(PDFThumbnailView, "init", tv_init); M(PDFThumbnailView, "dealloc", tv_dealloc);
    M(PDFThumbnailView, "drawRect:", tv_draw); M(PDFThumbnailView, "isFlipped", yes_imp);
    M(PDFThumbnailView, "PDFView", tv_view); M(PDFThumbnailView, "setPDFView:", tv_set_view);
    M(PDFThumbnailView, "thumbnailSize", tv_size); M(PDFThumbnailView, "setThumbnailSize:", tv_set_size);
    M(PDFThumbnailView, "maximumNumberOfColumns", tv_cols); M(PDFThumbnailView, "setMaximumNumberOfColumns:", tv_set_cols);
    M(PDFThumbnailView, "backgroundColor", tv_bg); M(PDFThumbnailView, "setBackgroundColor:", tv_set_bg);
    M(PDFThumbnailView, "selectedPages", tv_selected); M(PDFThumbnailView, "mouseDown:", tv_mouse_down);
    M(PDFThumbnailView, "scrollWheel:", tv_scroll);
}
