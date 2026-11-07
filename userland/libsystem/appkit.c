/*
 * mOS libSystem — AppKit (subconjunto) sobre a ponte gráfica do kernel (compat_gui.h).
 *
 * Cada NSWindow vira uma janela do window server do mOS; cada quadro é um lote de comandos de
 * desenho (retângulos, polígonos, texto com a fonte do sistema) enviado com GUI_DRAW. As views
 * usam as coordenadas do Cocoa (origem embaixo à esquerda, salvo isFlipped) e o laço de eventos
 * entrega mouseDown:/mouseUp:/mouseDragged:/keyDown:/scrollWheel: como no macOS.
 */
#include "foundation.h"
#include "libc.h"
#include "../../kernel/include/compat_gui.h"

double floor(double), cos(double), sin(double), strtod(const char *, char **);
char *strcpy(char *, const char *);
int isalnum(int);
void qsort(void *, size_t, size_t, int (*)(const void *, const void *));

typedef struct { double x, y; } NSPoint;
typedef struct { double w, h; } NSSize;
typedef struct { NSPoint o; NSSize s; } NSRect;

#define CLASS(N) cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N); cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N);
CLASS(NSResponder) CLASS(NSApplication) CLASS(NSWindow) CLASS(NSView) CLASS(NSControl) CLASS(NSButton) CLASS(NSScroller)
CLASS(NSTextField) CLASS(NSSlider) CLASS(NSProgressIndicator) CLASS(NSBox) CLASS(NSStackView) CLASS(NSImageView)
CLASS(NSColor) CLASS(NSFont) CLASS(NSBezierPath) CLASS(NSEvent) CLASS(NSMenu) CLASS(NSMenuItem) CLASS(NSAlert)
CLASS(NSScreen) CLASS(NSGraphicsContext) CLASS(NSCursor) CLASS(NSWorkspace) CLASS(NSImage) CLASS(NSAttributedString)
CLASS(NSRunningApplication) CLASS(NSSound) CLASS(NSPanel) CLASS(NSScrollView) CLASS(NSTextView) CLASS(NSClipView)
CLASS(NSVisualEffectView) CLASS(NSLayoutConstraint) CLASS(NSColorWell) CLASS(NSTableView) CLASS(NSTableColumn) CLASS(NSTableCellView) CLASS(NSIndexSet)

extern cx_class_t NSObject_c __asm__("OBJC_CLASS_$_NSObject");
extern cx_class_t NSString_c __asm__("OBJC_CLASS_$_NSString");
extern cx_class_t NSMutableArray_c __asm__("OBJC_CLASS_$_NSMutableArray");
extern void NSLog(id fmt, ...);
void cx_ca_poll(void);                     /* quartzcore.c: CALayerHost publicado por outro processo */

id NSApp;
id NSFontAttributeName, NSForegroundColorAttributeName, NSBackgroundColorAttributeName, NSParagraphStyleAttributeName;
id NSApplicationDidFinishLaunchingNotification, NSApplicationWillFinishLaunchingNotification, NSApplicationWillTerminateNotification;
id NSWindowWillCloseNotification, NSWindowDidResizeNotification, NSWindowDidBecomeKeyNotification;
id NSViewFrameDidChangeNotification, NSControlTextDidChangeNotification;
double NSFontWeightRegular = 0, NSFontWeightMedium = 0.23, NSFontWeightSemibold = 0.3, NSFontWeightBold = 0.4,
       NSFontWeightLight = -0.4, NSFontWeightThin = -0.6, NSFontWeightHeavy = 0.56, NSFontWeightBlack = 0.62, NSFontWeightUltraLight = -0.8;
double NSAppKitVersionNumber = 2487.0;

extern id objc_retain(id), objc_autorelease(id);
extern void objc_release(id);
extern void *objc_autoreleasePoolPush(void);
extern void objc_autoreleasePoolPop(void *);
/* guarda um objeto num campo, retendo o novo e soltando o antigo */
static id keep(id *slot, id v) { objc_retain(v); id old = *slot; *slot = v; objc_release(old); return v; }
static BOOL menus_dirty;
static void table_resize(id t, double width);
static void layout_scroll(id v);
static void set_clip(double x, double y, double w, double h);
struct absr;
static void draw_image_fit(id img, double x, double y, double w, double h);
static void perform_menu(int k);

/* ================================================================ estado dos objetos */
#define ST(o) (*(void **)((u8 *)(o) + 8))   /* todo objeto do AppKit guarda o estado aqui */

enum { K_VIEW, K_BUTTON, K_TEXT, K_SLIDER, K_PROGRESS, K_BOX, K_STACK, K_IMAGE, K_SCROLL, K_TABLE, K_TV };
#define ARRAYN(a) (sizeof(a) / sizeof((a)[0]))
enum { BT_PUSH, BT_CHECK, BT_RADIO };

typedef struct {
    NSRect frame;
    id superview, window;
    id *sub;
    u32 nsub, capsub;
    int kind;
    BOOL hidden, needs, enabled, highlighted, editable, bordered, bezeled, draws_bg, selectable, wants_layer;
    u64 autoresize;
    id target;
    SEL action;
    id title, value, placeholder;
    id font, text_color, bg_color;
    int alignment, btype, state, tag, bezel_style;
    double dval, minv, maxv;
    int caret;
    id key_equiv;
    int orientation;
    double spacing;
    id delegate;
    id tooltip;
    id image, doc, cols, data_source, text_field;
    double scroll_y, row_h;
    i64 selected, clicked;
    BOOL header, alt_rows;
    SEL double_action;
    int anchor;                    /* NSTextView: início da seleção */
    BOOL has_anchor, last_typing;
    id undo, redo;
    BOOL no_tamic;                 /* translatesAutoresizingMaskIntoConstraints = NO */
} vstate_t;

typedef struct {
    int gid;
    NSRect frame;                   /* tela, origem embaixo à esquerda (contentRect) */
    id content, title, delegate, first, bg, mouse_view;
    u64 style;
    BOOL visible, released_when_closed, closed, needs, is_modal_panel;
    int level;
    id cvc;                         /* contentViewController */
    double last_render;             /* cx_now() do último render_window (teto de quadros do pump) */
} wstate_t;

typedef struct { double r, g, b, a; } rgba_t;     /* NSColor: campos logo após o isa */
typedef struct { double size; int weight; int mono; } font_t;  /* NSFont */

static vstate_t *vs(id v) {
    if (!ST(v)) {
        vstate_t *s = calloc(1, sizeof *s);
        s->enabled = YES;
        s->needs = YES;
        s->draws_bg = NO;
        s->maxv = 1;
        ST(v) = s;
    }
    return ST(v);
}
static wstate_t *ws(id w) {
    if (!ST(w)) { wstate_t *s = calloc(1, sizeof *s); ST(w) = s; }
    return ST(w);
}
static rgba_t *col(id c) { return (rgba_t *)((u8 *)c + 8); }
static font_t *fnt(id f) { return (font_t *)((u8 *)f + 8); }

/* ================================================================ cores e fontes */
id cx_color(double r, double g, double b, double a) {
    id c = cx_alloc_instance(&NSColor_c);
    rgba_t *p = col(c);
    p->r = r; p->g = g; p->b = b; p->a = a;
    return c;
}
static u32 argb(id c) {
    if (!c) return 0xFF000000;
    rgba_t *p = col(c);
    double v[4] = {p->r, p->g, p->b, p->a};
    u32 o[4];
    for (int i = 0; i < 4; i++) { double x = v[i] < 0 ? 0 : v[i] > 1 ? 1 : v[i]; o[i] = (u32)(x * 255 + 0.5); }
    return o[3] << 24 | o[0] << 16 | o[1] << 8 | o[2];
}
static id cl_rgb(id cls, SEL c, double r, double g, double b, double a) { (void)cls; (void)c; return cx_color(r, g, b, a); }
static id cl_white_alpha(id cls, SEL c, double w, double a) { (void)cls; (void)c; return cx_color(w, w, w, a); }
static id cl_hsb(id cls, SEL c, double h, double s, double v, double a) {
    (void)cls; (void)c;
    double r = v, g = v, b = v;
    if (s > 0) {
        double hh = (h - floor(h)) * 6;
        int i = (int)hh;
        double f = hh - i, p = v * (1 - s), q = v * (1 - s * f), t = v * (1 - s * (1 - f));
        switch (i) { case 0: r = v; g = t; b = p; break; case 1: r = q; g = v; b = p; break; case 2: r = p; g = v; b = t; break;
                     case 3: r = p; g = q; b = v; break; case 4: r = t; g = p; b = v; break; default: r = v; g = p; b = q; }
    }
    return cx_color(r, g, b, a);
}
#define NAMED(fn, r, g, b, a) static id fn(id cls, SEL c) { (void)cls; (void)c; return cx_color(r, g, b, a); }
NAMED(cl_black, 0, 0, 0, 1) NAMED(cl_white, 1, 1, 1, 1) NAMED(cl_red, 1, 0.23, 0.19, 1) NAMED(cl_green, 0.2, 0.78, 0.35, 1)
NAMED(cl_blue, 0, 0.48, 1, 1) NAMED(cl_yellow, 1, 0.8, 0, 1) NAMED(cl_orange, 1, 0.58, 0, 1) NAMED(cl_purple, 0.69, 0.32, 0.87, 1)
NAMED(cl_gray, 0.56, 0.56, 0.58, 1) NAMED(cl_lgray, 0.82, 0.82, 0.84, 1) NAMED(cl_dgray, 0.33, 0.33, 0.33, 1) NAMED(cl_clear, 0, 0, 0, 0)
NAMED(cl_brown, 0.64, 0.52, 0.37, 1) NAMED(cl_cyan, 0.35, 0.78, 0.98, 1) NAMED(cl_magenta, 1, 0, 1, 1) NAMED(cl_pink, 1, 0.18, 0.33, 1)
NAMED(cl_teal, 0.19, 0.69, 0.78, 1) NAMED(cl_indigo, 0.35, 0.34, 0.84, 1)
NAMED(cl_label, 0.1, 0.1, 0.1, 0.85) NAMED(cl_sec_label, 0.24, 0.24, 0.26, 0.6) NAMED(cl_ter_label, 0.24, 0.24, 0.26, 0.3)
NAMED(cl_window_bg, 0.93, 0.93, 0.93, 1) NAMED(cl_control_bg, 1, 1, 1, 1) NAMED(cl_separator, 0, 0, 0, 0.1)
static id cl_with_alpha(id self, SEL c, double a) { (void)c; rgba_t *p = col(self); return cx_color(p->r, p->g, p->b, a); }
static double cl_r(id self, SEL c) { (void)c; return col(self)->r; }
static double cl_g(id self, SEL c) { (void)c; return col(self)->g; }
static double cl_b(id self, SEL c) { (void)c; return col(self)->b; }
static double cl_a(id self, SEL c) { (void)c; return col(self)->a; }
static id cl_blend(id self, SEL c, double f, id o) {
    (void)c;
    rgba_t *a = col(self), *b = col(o);
    return cx_color(a->r + (b->r - a->r) * f, a->g + (b->g - a->g) * f, a->b + (b->b - a->b) * f, a->a + (b->a - a->a) * f);
}
static id self_m(id self, SEL c) { (void)c; return self; }
static id noop_ret_nil(id self, SEL c) { (void)self; (void)c; return 0; }
static void noop_m(id self, SEL c) { (void)self; (void)c; }
static void noop1_m(id self, SEL c, id a) { (void)self; (void)c; (void)a; }
static BOOL yes_m(id self, SEL c) { (void)self; (void)c; return YES; }
static BOOL no_m(id self, SEL c) { (void)self; (void)c; return NO; }

/* contexto de desenho corrente (definido durante drawRect:) */
typedef struct {
    double tlx, tly, w, h;          /* retângulo absoluto da view (pixels do conteúdo da janela) */
    BOOL flipped;
    u32 fill, stroke;
    double line_width;
} ctx_t;
static ctx_t ctx = {0, 0, 0, 0, NO, 0xFF000000, 0xFF000000, 1};
u32 cx_cg_fill_argb(void) { return ctx.fill; }
static ctx_t ctx_stack[16];
static int ctx_sp;

static void cl_set(id self, SEL c) { (void)c; ctx.fill = ctx.stroke = argb(self); }
static void cl_set_fill(id self, SEL c) { (void)c; ctx.fill = argb(self); }
static void cl_set_stroke(id self, SEL c) { (void)c; ctx.stroke = argb(self); }

static id ft_make(double size, int weight, int mono) {
    id f = cx_alloc_instance(&NSFont_c);
    fnt(f)->size = size > 0 ? size : 13;
    fnt(f)->weight = weight;
    fnt(f)->mono = mono;
    return f;
}
static int weight_of(double w) { return w >= 0.35 ? 3 : w >= 0.27 ? 2 : w >= 0.2 ? 1 : 0; }
static id ft_system(id cls, SEL c, double s) { (void)cls; (void)c; return ft_make(s, 0, 0); }
static id ft_bold(id cls, SEL c, double s) { (void)cls; (void)c; return ft_make(s, 3, 0); }
static id ft_system_w(id cls, SEL c, double s, double w) { (void)cls; (void)c; return ft_make(s, weight_of(w), 0); }
static id ft_mono(id cls, SEL c, double s, double w) { (void)cls; (void)c; return ft_make(s, weight_of(w), 1); }
static id ft_named(id cls, SEL c, id name, double s) { (void)cls; (void)c; const char *n = cx_cstr(name); return ft_make(s, strstr(n, "Bold") ? 3 : strstr(n, "Semibold") ? 2 : strstr(n, "Medium") ? 1 : 0, !!strstr(n, "Mono") || !!strstr(n, "Courier") || !!strstr(n, "Menlo")); }
static double ft_size(id self, SEL c) { (void)c; return fnt(self)->size; }
static double ft_sys_size(id cls, SEL c) { (void)cls; (void)c; return 13; }
static double ft_small_size(id cls, SEL c) { (void)cls; (void)c; return 11; }
static id ft_name(id self, SEL c) { (void)c; return cx_str(fnt(self)->weight >= 3 ? ".AppleSystemUIFontBold" : ".AppleSystemUIFont"); }

/* ================================================================ lote de comandos */
static u8 *cmd;
static u32 ncmd, capcmd;
static void put(u32 op, const void *payload, u32 n) {
    u32 len = 8 + n;
    if (ncmd + len > capcmd) { capcmd = (ncmd + len) * 2 + 4096; cmd = realloc(cmd, capcmd); }
    memcpy(cmd + ncmd, &op, 4);
    memcpy(cmd + ncmd + 4, &len, 4);
    if (n) memcpy(cmd + ncmd + 8, payload, n);
    ncmd += len;
}
static void flush_to(int gid) {
    if (ncmd) SYS3(GUI_DRAW, gid, cmd, ncmd);
    ncmd = 0;
}
static void g_fill(double x, double y, double w, double h, u32 c) { float p[5] = {(float)x, (float)y, (float)w, (float)h, 0}; u8 b[24]; memcpy(b, p, 20); memcpy(b + 20, &c, 4); put(GC_RRECT, b, 24); }
static void g_rrect(double x, double y, double w, double h, double r, u32 c) { float p[5] = {(float)x, (float)y, (float)w, (float)h, (float)r}; u8 b[24]; memcpy(b, p, 20); memcpy(b + 20, &c, 4); put(GC_RRECT, b, 24); }
static void g_rstroke(double x, double y, double w, double h, double r, double t, u32 c) { float p[6] = {(float)x, (float)y, (float)w, (float)h, (float)r, (float)t}; u8 b[28]; memcpy(b, p, 24); memcpy(b + 24, &c, 4); put(GC_RRECT_STROKE, b, 28); }
static void g_line(double x0, double y0, double x1, double y1, double t, u32 c) { float p[5] = {(float)x0, (float)y0, (float)x1, (float)y1, (float)t}; u8 b[24]; memcpy(b, p, 20); memcpy(b + 20, &c, 4); put(GC_LINE, b, 24); }
static i32 last_clip[4]; static BOOL last_clip_on;   /* gancho QuartzCore: recorte vigente do lote */
static void g_clip(double x, double y, double w, double h) { i32 r[4] = {(i32)x, (i32)y, (i32)(w + 0.999), (i32)(h + 0.999)}; put(GC_CLIP, r, 16); memcpy(last_clip, r, 16); last_clip_on = YES; }
static void g_unclip(void) { put(GC_UNCLIP, 0, 0); last_clip_on = NO; }
/* gancho QuartzCore (userland/libsystem/quartzcore.c): views com CALayer. O QuartzCore despacha o
 * lote pendente (cx_gui_flush), manda os pixels dos layers direto com GUI_DRAW e devolve o controle;
 * o kernel zera o recorte a cada GUI_DRAW, então o recorte vigente é reposto no lote novo.
 * fase 0: antes do desenho da view (fundo do layer); fase 1: depois do drawRect: (conteúdo e sublayers). */
void (*cx_layer_draw_hook)(id view, int phase, double tlx, double tly, double w, double h,
                           double clip_x0, double clip_y0, double clip_x1, double clip_y1);
int cx_gui_cur_gid;
static void flush_to(int gid);
void cx_gui_flush(void) { flush_to(cx_gui_cur_gid); if (last_clip_on) put(GC_CLIP, last_clip, 16); }
__attribute__((unused)) static void g_grad(double x, double y, double w, double h, u32 top, u32 bot) { u8 b[24]; i32 r[4] = {(i32)x, (i32)y, (i32)w, (i32)h}; memcpy(b, r, 16); memcpy(b + 16, &top, 4); memcpy(b + 20, &bot, 4); put(GC_GRAD, b, 24); }
static void g_poly(const float *xy, u32 n, u32 c, int fill, double t) {
    if (n < 2) return;
    u32 hdr = fill ? 8 : 12;
    u8 *b = malloc(hdr + n * 8);
    memcpy(b, &n, 4);
    memcpy(b + 4, &c, 4);
    if (!fill) { float tf = (float)t; memcpy(b + 8, &tf, 4); }
    memcpy(b + hdr, xy, n * 8);
    put(fill ? GC_POLY : GC_POLYLINE, b, hdr + n * 8);
    free(b);
}
static void g_text(double x, double base, double size, int weight, u32 c, const char *s, u32 n) {
    u8 *b = malloc(24 + n);
    float f[3] = {(float)x, (float)base, (float)size};
    u32 u[3] = {(u32)weight, c, n};
    memcpy(b, f, 12);
    memcpy(b + 12, u, 12);
    memcpy(b + 24, s, n);
    put(GC_TEXT, b, 24 + n);
    free(b);
}
static gui_metrics_t measure(const char *s, u32 n, double size, int weight) {
    gui_metrics_t m = {0, 10, 3};
    SYS4(GUI_MEASURE, s, n, &m, (long)((u32)(size * 100) | (u32)weight << 16));
    return m;
}

/* conversão de coordenadas da view corrente para pixels */
static double px_x(double x) { return ctx.tlx + x; }
static double px_y(double y) { return ctx.flipped ? ctx.tly + y : ctx.tly + ctx.h - y; }
static void px_rect(NSRect r, double *x, double *y, double *w, double *h) {
    *x = ctx.tlx + r.o.x;
    *y = ctx.flipped ? ctx.tly + r.o.y : ctx.tly + ctx.h - r.o.y - r.s.h;
    *w = r.s.w;
    *h = r.s.h;
}

/* ================================================================ NSBezierPath */
typedef struct { float *xy; u32 n, cap; u32 *starts; u32 nstarts, capstarts; double lw; BOOL closed_last; } path_t;
static path_t *pth(id p) { if (!ST(p)) { path_t *s = calloc(1, sizeof *s); s->lw = 1; ST(p) = s; } return ST(p); }
static void p_add(path_t *p, double x, double y) {
    if (p->n == p->cap) { p->cap = p->cap ? p->cap * 2 : 32; p->xy = realloc(p->xy, p->cap * 8); }
    p->xy[p->n * 2] = (float)x;
    p->xy[p->n * 2 + 1] = (float)y;
    p->n++;
}
static void p_start(path_t *p) {
    if (p->nstarts == p->capstarts) { p->capstarts = p->capstarts ? p->capstarts * 2 : 8; p->starts = realloc(p->starts, p->capstarts * 4); }
    p->starts[p->nstarts++] = p->n;
}
static void bp_move(id self, SEL c, NSPoint pt) { (void)c; path_t *p = pth(self); p_start(p); p_add(p, pt.x, pt.y); }
static void bp_line(id self, SEL c, NSPoint pt) { (void)c; path_t *p = pth(self); if (!p->nstarts) p_start(p); p_add(p, pt.x, pt.y); }
static void bp_curve(id self, SEL c, NSPoint e, NSPoint c1, NSPoint c2) {
    (void)c;
    path_t *p = pth(self);
    if (!p->n) { p_start(p); p_add(p, e.x, e.y); return; }
    double x0 = p->xy[(p->n - 1) * 2], y0 = p->xy[(p->n - 1) * 2 + 1];
    for (int i = 1; i <= 16; i++) {
        double t = i / 16.0, u = 1 - t;
        p_add(p, u * u * u * x0 + 3 * u * u * t * c1.x + 3 * u * t * t * c2.x + t * t * t * e.x,
              u * u * u * y0 + 3 * u * u * t * c1.y + 3 * u * t * t * c2.y + t * t * t * e.y);
    }
}
static void bp_close(id self, SEL c) {
    (void)c;
    path_t *p = pth(self);
    if (!p->nstarts) return;
    u32 s = p->starts[p->nstarts - 1];
    if (p->n > s) p_add(p, p->xy[s * 2], p->xy[s * 2 + 1]);
}
static void add_rect(path_t *p, NSRect r) {
    p_start(p);
    p_add(p, r.o.x, r.o.y); p_add(p, r.o.x + r.s.w, r.o.y); p_add(p, r.o.x + r.s.w, r.o.y + r.s.h); p_add(p, r.o.x, r.o.y + r.s.h); p_add(p, r.o.x, r.o.y);
}
static void add_oval(path_t *p, NSRect r) {
    p_start(p);
    double cx = r.o.x + r.s.w / 2, cy = r.o.y + r.s.h / 2;
    for (int i = 0; i <= 64; i++) { double a = i * 6.283185307179586 / 64; p_add(p, cx + cos(a) * r.s.w / 2, cy + sin(a) * r.s.h / 2); }
}
static void add_rrect(path_t *p, NSRect r, double rx, double ry) {
    if (rx <= 0 || ry <= 0) { add_rect(p, r); return; }
    if (rx > r.s.w / 2) rx = r.s.w / 2;
    if (ry > r.s.h / 2) ry = r.s.h / 2;
    p_start(p);
    double cxs[4] = {r.o.x + r.s.w - rx, r.o.x + r.s.w - rx, r.o.x + rx, r.o.x + rx};
    double cys[4] = {r.o.y + ry, r.o.y + r.s.h - ry, r.o.y + r.s.h - ry, r.o.y + ry};
    double a0[4] = {-1.5707963, 0, 1.5707963, 3.14159265};
    for (int k = 0; k < 4; k++)
        for (int i = 0; i <= 8; i++) { double a = a0[k] + i * 1.5707963 / 8; p_add(p, cxs[k] + cos(a) * rx, cys[k] + sin(a) * ry); }
    p_add(p, p->xy[p->starts[p->nstarts - 1] * 2], p->xy[p->starts[p->nstarts - 1] * 2 + 1]);
}
static id bp_new(void) { return MSG0(id, MSG0(id, &NSBezierPath_c, "alloc"), "init"); }
static id bp_rect(id cls, SEL c, NSRect r) { (void)cls; (void)c; id o = bp_new(); add_rect(pth(o), r); return o; }
static id bp_oval(id cls, SEL c, NSRect r) { (void)cls; (void)c; id o = bp_new(); add_oval(pth(o), r); return o; }
static id bp_rrect(id cls, SEL c, NSRect r, double rx, double ry) { (void)cls; (void)c; id o = bp_new(); add_rrect(pth(o), r, rx, ry); return o; }
static void bp_app_rect(id self, SEL c, NSRect r) { (void)c; add_rect(pth(self), r); }
static void bp_app_oval(id self, SEL c, NSRect r) { (void)c; add_oval(pth(self), r); }
static void bp_app_rrect(id self, SEL c, NSRect r, double rx, double ry) { (void)c; add_rrect(pth(self), r, rx, ry); }
static void bp_set_lw(id self, SEL c, double w) { (void)c; pth(self)->lw = w; }
static double bp_lw(id self, SEL c) { (void)c; return pth(self)->lw; }
static void bp_remove_all(id self, SEL c) { (void)c; path_t *p = pth(self); p->n = 0; p->nstarts = 0; }
static void draw_path(path_t *p, int fill) {
    for (u32 k = 0; k < p->nstarts; k++) {
        u32 s = p->starts[k], e = k + 1 < p->nstarts ? p->starts[k + 1] : p->n;
        if (e - s < 2) continue;
        float *xy = malloc((e - s) * 8);
        for (u32 i = s; i < e; i++) { xy[(i - s) * 2] = (float)px_x(p->xy[i * 2]); xy[(i - s) * 2 + 1] = (float)px_y(p->xy[i * 2 + 1]); }
        if (fill && e - s >= 3) g_poly(xy, e - s, ctx.fill, 1, 0);
        else if (!fill) g_poly(xy, e - s, ctx.stroke, 0, p->lw);
        free(xy);
    }
}
static void bp_fill(id self, SEL c) { (void)c; draw_path(pth(self), 1); }
static void bp_stroke(id self, SEL c) { (void)c; draw_path(pth(self), 0); }
static void bp_cfill_rect(id cls, SEL c, NSRect r) { (void)cls; (void)c; double x, y, w, h; px_rect(r, &x, &y, &w, &h); g_fill(x, y, w, h, ctx.fill); }
static void bp_cstroke_rect(id cls, SEL c, NSRect r) { (void)cls; (void)c; id p = bp_rect(0, 0, r); bp_stroke(p, 0); }
static void bp_cstroke_line(id cls, SEL c, NSPoint a, NSPoint b) { (void)cls; (void)c; g_line(px_x(a.x), px_y(a.y), px_x(b.x), px_y(b.y), 1, ctx.stroke); }
static NSRect bp_bounds(id self, SEL c) {
    (void)c;
    path_t *p = pth(self);
    NSRect r = {{0, 0}, {0, 0}};
    if (!p->n) return r;
    double x0 = 1e30, y0 = 1e30, x1 = -1e30, y1 = -1e30;
    for (u32 i = 0; i < p->n; i++) {
        double x = p->xy[i * 2], y = p->xy[i * 2 + 1];
        if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; if (y > y1) y1 = y;
    }
    r.o.x = x0; r.o.y = y0; r.s.w = x1 - x0; r.s.h = y1 - y0;
    return r;
}
static void bp_add_clip(id self, SEL c) { NSRect r = bp_bounds(self, c); double x, y, w, h; px_rect(r, &x, &y, &w, &h); g_clip(x, y, w, h); }

/* funções C de desenho do AppKit */
void NSRectFill(NSRect r) { double x, y, w, h; px_rect(r, &x, &y, &w, &h); g_fill(x, y, w, h, ctx.fill); }
void NSRectFillUsingOperation(NSRect r, u64 op) { (void)op; NSRectFill(r); }
void NSEraseRect(NSRect r) { double x, y, w, h; px_rect(r, &x, &y, &w, &h); g_fill(x, y, w, h, 0xFFFFFFFF); }
void NSFrameRectWithWidth(NSRect r, double t) { double x, y, w, h; px_rect(r, &x, &y, &w, &h); g_rstroke(x + t / 2, y + t / 2, w - t, h - t, 0, t, ctx.fill); }
void NSFrameRect(NSRect r) { NSFrameRectWithWidth(r, 1); }
void NSRectFillList(const NSRect *r, i64 n) { for (i64 i = 0; i < n; i++) NSRectFill(r[i]); }
void NSBeep(void) {}

/* ================================================================ texto */
static void font_attrs(id attrs, double *size, int *weight, u32 *color) {
    *size = 13; *weight = 0; *color = 0xE6000000;
    if (!attrs) return;
    id f = MSG(id, attrs, "objectForKey:", NSFontAttributeName);
    id c = MSG(id, attrs, "objectForKey:", NSForegroundColorAttributeName);
    if (f) { *size = fnt(f)->size; *weight = fnt(f)->weight; }
    if (c) *color = argb(c);
}
static void draw_str_at(const char *s, NSPoint pt, id attrs) {
    double size; int weight; u32 color;
    font_attrs(attrs, &size, &weight, &color);
    u32 n = (u32)strlen(s);
    gui_metrics_t m = measure(s, n, size, weight);
    double x = px_x(pt.x), base;
    if (ctx.flipped) base = ctx.tly + pt.y + m.ascent;
    else base = ctx.tly + ctx.h - pt.y - m.descent;
    /* várias linhas */
    const char *line = s;
    for (;;) {
        const char *nl = strchr(line, '\n');
        u32 ln = nl ? (u32)(nl - line) : (u32)strlen(line);
        g_text(x, base, size, weight, color, line, ln);
        if (!nl) break;
        line = nl + 1;
        base += m.ascent + m.descent + 2;
    }
}
static void str_draw_at(id self, SEL c, NSPoint pt, id attrs) { (void)c; draw_str_at(cx_cstr(self), pt, attrs); }
static NSSize text_size(const char *s, double size, int weight) {
    NSSize z = {0, 0};
    const char *line = s;
    gui_metrics_t m = {0, 10, 3};
    for (;;) {
        const char *nl = strchr(line, '\n');
        u32 ln = nl ? (u32)(nl - line) : (u32)strlen(line);
        m = measure(line, ln, size, weight);
        if (m.width > z.w) z.w = m.width;
        z.h += m.ascent + m.descent + (nl ? 2 : 0);
        if (!nl) break;
        line = nl + 1;
    }
    return z;
}
static NSSize str_size(id self, SEL c, id attrs) {
    (void)c;
    double size; int weight; u32 color;
    font_attrs(attrs, &size, &weight, &color);
    return text_size(cx_cstr(self), size, weight);
}
/* quebra de linha por palavras dentro de um retângulo (pixels) */
static void draw_wrapped(const char *s, double x, double y, double w, double h, double size, int weight, u32 color, int align) {
    gui_metrics_t m0 = measure("Ag", 2, size, weight);
    double lh = m0.ascent + m0.descent + 2, base = y + m0.ascent;
    const char *p = s;
    while (*p && base - m0.ascent < y + h) {
        const char *best = p, *q = p;
        while (*q && *q != '\n') {
            const char *e = q;
            while (*e && *e != ' ' && *e != '\n') e++;
            if (measure(p, (u32)(e - p), size, weight).width > w && best != p) break;
            best = e;
            q = *e == ' ' ? e + 1 : e;
            if (measure(p, (u32)(best - p), size, weight).width > w) break;
        }
        if (best == p) { best = q; if (best == p) best = p + 1; }
        u32 n = (u32)(best - p);
        double tw = measure(p, n, size, weight).width, tx = x;
        if (align == 2) tx = x + (w - tw) / 2;
        else if (align == 1) tx = x + w - tw;
        g_text(tx, base, size, weight, color, p, n);
        p = best;
        while (*p == ' ') p++;
        if (*p == '\n') p++;
        base += lh;
    }
}
static void str_draw_in(id self, SEL c, NSRect r, id attrs) {
    (void)c;
    double size; int weight; u32 color;
    font_attrs(attrs, &size, &weight, &color);
    double x, y, w, h;
    px_rect(r, &x, &y, &w, &h);
    draw_wrapped(cx_cstr(self), x, y, w, h, size, weight, color, 0);
}

/* NSAttributedString mínimo: string + atributos uniformes */
typedef struct { id str, attrs; } astr_t;
static id as_init(id self, SEL c, id s, id attrs) { (void)c; astr_t *a = calloc(1, sizeof *a); a->str = objc_retain(s); a->attrs = objc_retain(attrs); ST(self) = a; return self; }
static id as_init1(id self, SEL c, id s) { return as_init(self, c, s, 0); }
static id as_string(id self, SEL c) { (void)c; return ((astr_t *)ST(self))->str; }
static void as_draw_at(id self, SEL c, NSPoint pt) { (void)c; astr_t *a = ST(self); draw_str_at(cx_cstr(a->str), pt, a->attrs); }
static NSSize as_size(id self, SEL c) { astr_t *a = ST(self); return str_size(a->str, c, a->attrs); }

/* ================================================================ NSGraphicsContext / CoreGraphics */
static id gc_current(id cls, SEL c) { (void)c; static id g; if (!g) g = cx_alloc_instance((cx_class_t *)cls); return g; }
static void gc_save(id cls, SEL c) { (void)cls; (void)c; if (ctx_sp < 16) ctx_stack[ctx_sp++] = ctx; }
static void gc_restore(id cls, SEL c) { (void)cls; (void)c; if (ctx_sp > 0) ctx = ctx_stack[--ctx_sp]; g_unclip(); }
static id gc_cg(id self, SEL c) { (void)c; return self; }
typedef NSRect CGRect;
typedef NSPoint CGPoint;
static id cg_path;
static id cgp(void) { if (!cg_path) cg_path = bp_new(); return cg_path; }
void cx_bm_set_fill(const void *c, u32 argb);
int cx_bm_save(const void *c);
int cx_bm_restore(const void *c);
int cx_bm_fill_rect(const void *c, double x, double y, double w, double h);
int cx_bm_clear_rect(const void *c, double x, double y, double w, double h);
void CGContextSetRGBFillColor(id c, double r, double g, double b, double a) {
    ctx.fill = argb(cx_color(r, g, b, a));
    cx_bm_set_fill(c, ctx.fill);
}
void CGContextSetRGBStrokeColor(id c, double r, double g, double b, double a) { (void)c; ctx.stroke = argb(cx_color(r, g, b, a)); }
void CGContextSetGrayFillColor(id c, double g, double a) { CGContextSetRGBFillColor(c, g, g, g, a); }
void CGContextSetGrayStrokeColor(id c, double g, double a) { CGContextSetRGBStrokeColor(c, g, g, g, a); }
void CGContextSetFillColorWithColor(id c, id color) { ctx.fill = argb(color); cx_bm_set_fill(c, ctx.fill); }
void CGContextSetStrokeColorWithColor(id c, id color) { (void)c; ctx.stroke = argb(color); }
void CGContextSetLineWidth(id c, double w) { (void)c; ctx.line_width = w; pth(cgp())->lw = w; }
void CGContextFillRect(id c, CGRect r) {
    if (cx_bm_fill_rect(c, r.o.x, r.o.y, r.s.w, r.s.h)) return;
    NSRectFill(r);
}
void CGContextFillRects(id c, const CGRect *r, u64 n) { for (u64 i = 0; i < n; i++) CGContextFillRect(c, r[i]); }
void CGContextClearRect(id c, CGRect r) {
    if (cx_bm_clear_rect(c, r.o.x, r.o.y, r.s.w, r.s.h)) return;
    double x, y, w, h; px_rect(r, &x, &y, &w, &h); g_fill(x, y, w, h, 0xFFFFFFFF);
}
void CGContextStrokeRect(id c, CGRect r) { (void)c; id p = bp_rect(0, 0, r); pth(p)->lw = ctx.line_width; bp_stroke(p, 0); }
void CGContextStrokeRectWithWidth(id c, CGRect r, double w) { (void)c; id p = bp_rect(0, 0, r); pth(p)->lw = w; bp_stroke(p, 0); }
void CGContextFillEllipseInRect(id c, CGRect r) { (void)c; bp_fill(bp_oval(0, 0, r), 0); }
void CGContextStrokeEllipseInRect(id c, CGRect r) { (void)c; id p = bp_oval(0, 0, r); pth(p)->lw = ctx.line_width; bp_stroke(p, 0); }
void CGContextBeginPath(id c) { (void)c; bp_remove_all(cgp(), 0); }
void CGContextMoveToPoint(id c, double x, double y) { (void)c; NSPoint p = {x, y}; bp_move(cgp(), 0, p); }
void CGContextAddLineToPoint(id c, double x, double y) { (void)c; NSPoint p = {x, y}; bp_line(cgp(), 0, p); }
void CGContextAddCurveToPoint(id c, double c1x, double c1y, double c2x, double c2y, double x, double y) { (void)c; NSPoint e = {x, y}, a = {c1x, c1y}, b = {c2x, c2y}; bp_curve(cgp(), 0, e, a, b); }
void CGContextAddRect(id c, CGRect r) { (void)c; add_rect(pth(cgp()), r); }
void CGContextAddEllipseInRect(id c, CGRect r) { (void)c; add_oval(pth(cgp()), r); }
void CGContextClosePath(id c) { (void)c; bp_close(cgp(), 0); }
void CGContextFillPath(id c) { (void)c; bp_fill(cgp(), 0); bp_remove_all(cgp(), 0); }
void CGContextStrokePath(id c) { (void)c; pth(cgp())->lw = ctx.line_width; bp_stroke(cgp(), 0); bp_remove_all(cgp(), 0); }
void CGContextDrawPath(id c, int mode) { if (mode == 0 || mode == 3) bp_fill(cgp(), 0); if (mode >= 2) bp_stroke(cgp(), 0); bp_remove_all(cgp(), 0); (void)c; }
void CGContextSaveGState(id c) { if (!cx_bm_save(c)) gc_save(c, 0); }
void CGContextRestoreGState(id c) { if (!cx_bm_restore(c)) gc_restore(c, 0); }
void CGContextSetLineCap(id c, int v) { (void)c; (void)v; }
void CGContextSetLineJoin(id c, int v) { (void)c; (void)v; }
void CGContextSetShouldAntialias(id c, int v) { (void)c; (void)v; }
id CGColorCreateGenericRGB(double r, double g, double b, double a) { return cx_color(r, g, b, a); }
id CGColorCreateGenericGray(double g, double a) { return cx_color(g, g, g, a); }
void CGColorRelease(id c) { (void)c; }
id CGColorRetain(id c) { return c; }

/* ================================================================ views */
static NSRect r_make(double x, double y, double w, double h) { NSRect r = {{x, y}, {w, h}}; return r; }
static void mark(id v) { if (v) { vs(v)->needs = YES; id w = vs(v)->window; if (w) ws(w)->needs = YES; } }

static id v_init_frame(id self, SEL c, NSRect f) { (void)c; vs(self)->frame = f; return self; }
static id v_init(id self, SEL c) { return v_init_frame(self, c, r_make(0, 0, 0, 0)); }
static NSRect v_frame(id self, SEL c) { (void)c; return vs(self)->frame; }
static NSRect v_bounds(id self, SEL c) { (void)c; NSRect r = vs(self)->frame; r.o.x = r.o.y = 0; return r; }
static void resize_children(id v, NSSize old);
static void v_set_frame(id self, SEL c, NSRect f) {
    (void)c;
    vstate_t *s = vs(self);
    NSSize old = s->frame.s;
    s->frame = f;
    if (old.w != f.s.w || old.h != f.s.h) resize_children(self, old);
    mark(self);
}
static void v_set_size(id self, SEL c, NSSize sz) { NSRect f = vs(self)->frame; f.s = sz; v_set_frame(self, c, f); }
static void v_set_origin(id self, SEL c, NSPoint p) { NSRect f = vs(self)->frame; f.o = p; v_set_frame(self, c, f); }
static void set_window_rec(id v, id w) {
    vstate_t *s = vs(v);
    s->window = w;
    for (u32 i = 0; i < s->nsub; i++) set_window_rec(s->sub[i], w);
}
/* at é o índice depois de tirar sub do pai antigo. A última subview é a de cima (o hit-test anda de trás pra frente). */
static void v_insert_sub(id self, id sub, u32 at) {
    if (!sub) return;
    vstate_t *s = vs(self), *ss = vs(sub);
    objc_retain(sub);
    if (ss->superview) {
        vstate_t *ps = vs(ss->superview);
        for (u32 i = 0; i < ps->nsub; i++) if (ps->sub[i] == sub) {
            memmove(ps->sub + i, ps->sub + i + 1, (ps->nsub - i - 1) * sizeof(id));
            ps->nsub--;
            objc_release(sub);
            if (ps == s && i < at) at--;
            break;
        }
    }
    if (at > s->nsub) at = s->nsub;
    if (s->nsub == s->capsub) { s->capsub = s->capsub ? s->capsub * 2 : 8; s->sub = realloc(s->sub, s->capsub * sizeof(id)); }
    memmove(s->sub + at + 1, s->sub + at, (s->nsub - at) * sizeof(id));
    s->sub[at] = sub;
    s->nsub++;
    ss->superview = self;
    set_window_rec(sub, s->window);
    if (cx_responds(sub->isa, sel_registerName("viewDidMoveToWindow")) && s->window) MSG0(void, sub, "viewDidMoveToWindow");
    mark(self);
}
static void v_add_sub(id self, SEL c, id sub) { (void)c; v_insert_sub(self, sub, vs(self)->nsub); }
/* NSWindowAbove = 1 (depois de other, por cima), NSWindowBelow = -1 (no lugar de other, por baixo). other nulo: above vai pro fim, below pro começo. */
static void v_add_positioned(id self, SEL c, id sub, i64 place, id other) {
    (void)c;
    vstate_t *s = vs(self);
    u32 at = place < 0 ? 0 : s->nsub;
    if (other && other != sub) {
        for (u32 i = 0; i < s->nsub; i++) if (s->sub[i] == other) { at = place < 0 ? i : i + 1; break; }
    }
    v_insert_sub(self, sub, at);
}
typedef i64 (*vsort_fn)(id, id, void *);
static void v_sort_sub(id self, SEL c, vsort_fn cmp, void *ctx) {
    (void)c;
    if (!cmp) return;
    vstate_t *s = vs(self);
    for (u32 i = 1; i < s->nsub; i++) {
        id v = s->sub[i];
        u32 j = i;
        while (j > 0 && cmp(s->sub[j - 1], v, ctx) > 0) { s->sub[j] = s->sub[j - 1]; j--; }
        s->sub[j] = v;
    }
    mark(self);
}
static void v_remove(id self, SEL c) {
    (void)c;
    vstate_t *s = vs(self);
    if (!s->superview) return;
    vstate_t *ps = vs(s->superview);
    for (u32 i = 0; i < ps->nsub; i++) if (ps->sub[i] == self) { memmove(ps->sub + i, ps->sub + i + 1, (ps->nsub - i - 1) * sizeof(id)); ps->nsub--; break; }
    mark(s->superview);
    s->superview = 0;
    set_window_rec(self, 0);
    objc_release(self);
}
static id v_subviews(id self, SEL c) { (void)c; vstate_t *s = vs(self); id a = cx_arr_new(&NSMutableArray_c); for (u32 i = 0; i < s->nsub; i++) cx_arr_add(a, s->sub[i]); return a; }
static id v_superview(id self, SEL c) { (void)c; return vs(self)->superview; }
static id v_window(id self, SEL c) { (void)c; return vs(self)->window; }
static void v_set_needs(id self, SEL c, BOOL b) { (void)c; if (b) mark(self); }
static void v_set_needs_rect(id self, SEL c, NSRect r) { (void)c; (void)r; mark(self); }
static void v_display(id self, SEL c) { (void)c; mark(self); }
static BOOL v_needs(id self, SEL c) { (void)c; return vs(self)->needs; }
static void v_set_hidden(id self, SEL c, BOOL h) { (void)c; vs(self)->hidden = h; mark(vs(self)->superview ? vs(self)->superview : self); }
static BOOL v_hidden(id self, SEL c) { (void)c; return vs(self)->hidden; }
static void v_set_autoresize(id self, SEL c, u64 m) { (void)c; vs(self)->autoresize = m; }
static u64 v_autoresize(id self, SEL c) { (void)c; return vs(self)->autoresize; }
static void v_set_tag(id self, SEL c, i64 t) { (void)c; vs(self)->tag = (int)t; }
static i64 v_tag(id self, SEL c) { (void)c; return vs(self)->tag; }
static void v_set_tooltip(id self, SEL c, id t) { (void)c; keep(&vs(self)->tooltip, t); }
static void v_set_wants_layer(id self, SEL c, BOOL b) { (void)c; vs(self)->wants_layer = b; }
static id v_layer(id self, SEL c) { (void)self; (void)c; return 0; }
static id v_view_with_tag(id self, SEL c, i64 t) {
    vstate_t *s = vs(self);
    if (s->tag == t) return self;
    for (u32 i = 0; i < s->nsub; i++) { id r = v_view_with_tag(s->sub[i], c, t); if (r) return r; }
    return 0;
}
static NSSize v_intrinsic(id self, SEL c);
static void v_size_to_fit(id self, SEL c) { NSSize z = v_intrinsic(self, c); if (z.w > 0) v_set_size(self, c, z); }
static void v_draw_rect(id self, SEL c, NSRect r) { (void)self; (void)c; (void)r; }

/* autoresizing: NSViewMinXMargin 1, WidthSizable 2, MaxXMargin 4, MinYMargin 8, HeightSizable 16, MaxYMargin 32 */
static void resize_children(id v, NSSize old) {
    vstate_t *s = vs(v);
    double dw = s->frame.s.w - old.w, dh = s->frame.s.h - old.h;
    for (u32 i = 0; i < s->nsub; i++) {
        vstate_t *cs = vs(s->sub[i]);
        u64 m = cs->autoresize;
        if (!m) continue;
        NSRect f = cs->frame;
        int hx = !!(m & 1) + !!(m & 2) + !!(m & 4), hy = !!(m & 8) + !!(m & 16) + !!(m & 32);
        if (hx) { double d = dw / hx; if (m & 1) f.o.x += d; if (m & 2) f.s.w += d; }
        if (hy) { double d = dh / hy; if (m & 8) f.o.y += d; if (m & 16) f.s.h += d; }
        v_set_frame(s->sub[i], 0, f);
    }
}

/* conversão de pontos (só fromView:nil / toView:nil, a partir da janela) */
typedef struct { double tlx, tly, w, h; BOOL flipped; } absr_t;
static BOOL is_flipped(id v) { return MSG0(BOOL, v, "isFlipped"); }
static BOOL abs_rect(id v, absr_t *out) {
    vstate_t *s = vs(v);
    if (!s->window) return NO;
    id content = ws(s->window)->content;
    if (v == content) { out->tlx = 0; out->tly = 0; out->w = s->frame.s.w; out->h = s->frame.s.h; out->flipped = is_flipped(v); return YES; }
    if (!s->superview) return NO;
    absr_t p;
    if (!abs_rect(s->superview, &p)) return NO;
    out->tlx = p.tlx + s->frame.o.x;
    out->tly = p.flipped ? p.tly + s->frame.o.y : p.tly + p.h - s->frame.o.y - s->frame.s.h;
    out->w = s->frame.s.w;
    out->h = s->frame.s.h;
    out->flipped = is_flipped(v);
    return YES;
}
static double win_h(id w) { wstate_t *s = ws(w); return s->frame.s.h; }
static NSPoint v_convert_from(id self, SEL c, NSPoint p, id from) {
    (void)c;
    absr_t a;
    if (from || !abs_rect(self, &a)) return p;
    double wy = win_h(vs(self)->window) - p.y;   /* janela (Cocoa) -> pixels */
    NSPoint r = {p.x - a.tlx, a.flipped ? wy - a.tly : a.tly + a.h - wy};
    return r;
}
static NSPoint v_convert_to(id self, SEL c, NSPoint p, id to) {
    (void)c;
    absr_t a;
    if (to || !abs_rect(self, &a)) return p;
    double py = a.flipped ? a.tly + p.y : a.tly + a.h - p.y;
    NSPoint r = {a.tlx + p.x, win_h(vs(self)->window) - py};
    return r;
}
/* os dois cantos: entre uma view invertida (isFlipped) e outra não, a origem do retângulo troca de canto */
static NSRect v_convert_rect_to(id self, SEL c, NSRect r, id to) {
    NSPoint a = v_convert_to(self, c, r.o, to), b = v_convert_to(self, c, (NSPoint){r.o.x + r.s.w, r.o.y + r.s.h}, to);
    r.o.x = a.x < b.x ? a.x : b.x;
    r.o.y = a.y < b.y ? a.y : b.y;
    return r;
}

/* ================================================================ controles */
static id dflt_font(void) { static id f; if (!f) f = ft_make(13, 0, 0); return f; }
static u32 accent = 0xFF007AFF;

static void draw_view_tree(id v, absr_t *a);
static void draw_builtin(id v, absr_t *a);
static void draw_textview(id v, absr_t *a);
static void draw_table(id t, absr_t *a);

static void ctl_set_target(id self, SEL c, id t) { (void)c; vs(self)->target = t; }
static id ctl_target(id self, SEL c) { (void)c; return vs(self)->target; }
static void ctl_set_action(id self, SEL c, SEL a) { (void)c; vs(self)->action = a; }
static SEL ctl_action(id self, SEL c) { (void)c; return vs(self)->action; }
static void ctl_set_enabled(id self, SEL c, BOOL b) { (void)c; vs(self)->enabled = b; mark(self); }
static BOOL ctl_enabled(id self, SEL c) { (void)c; return vs(self)->enabled; }
static void ctl_set_font(id self, SEL c, id f) { (void)c; keep(&vs(self)->font, f); mark(self); }
static id ctl_font(id self, SEL c) { (void)c; return vs(self)->font ? vs(self)->font : dflt_font(); }
static void ctl_set_align(id self, SEL c, i64 a) { (void)c; vs(self)->alignment = (int)a; mark(self); }
static void ctl_set_string(id self, SEL c, id s) { (void)c; vstate_t *v = vs(self); id nv = s ? MSG0(id, s, "copy") : cx_str(""); objc_release(v->value); v->value = nv; if (v->caret > (int)strlen(cx_cstr(v->value))) v->caret = (int)strlen(cx_cstr(v->value)); mark(self); }
static id ctl_string(id self, SEL c) { (void)c; vstate_t *v = vs(self); return v->value ? v->value : cx_str(""); }
static void ctl_set_int(id self, SEL c, int i) { char b[32]; snprintf(b, sizeof b, "%d", i); vs(self)->dval = i; ctl_set_string(self, c, cx_str(b)); }
static void ctl_set_double(id self, SEL c, double d) {
    vstate_t *v = vs(self);
    v->dval = d;
    if (v->kind == K_SLIDER || v->kind == K_PROGRESS) { mark(self); return; }
    char b[48];
    snprintf(b, sizeof b, "%g", d);
    ctl_set_string(self, c, cx_str(b));
}
static int ctl_int(id self, SEL c) { (void)c; vstate_t *v = vs(self); return v->kind == K_SLIDER ? (int)v->dval : atoi(cx_cstr(ctl_string(self, c))); }
static double ctl_double(id self, SEL c) { (void)c; vstate_t *v = vs(self); return (v->kind == K_SLIDER || v->kind == K_PROGRESS) ? v->dval : strtod(cx_cstr(ctl_string(self, c)), 0); }
static void ctl_set_tcolor(id self, SEL c, id col_) { (void)c; keep(&vs(self)->text_color, col_); mark(self); }
static id ctl_tcolor(id self, SEL c) { (void)c; return vs(self)->text_color; }
static void bind_push(id view);
extern id cx_num_bool(BOOL b);
static void send_action(id sender) {
    if (cx_is_subclass(sender->isa, &NSView_c)) bind_push(sender);
    vstate_t *s = vs(sender);
    if (!s->action) return;
    id t = s->target;
    if (!t) {
        /* cadeia: janela -> delegate da janela -> delegate do app */
        id w = s->window;
        id cands[3] = {w ? ws(w)->first : 0, w ? ws(w)->delegate : 0, NSApp ? MSG0(id, NSApp, "delegate") : 0};
        for (int i = 0; i < 3 && !t; i++) if (cands[i] && cx_responds(cands[i]->isa, s->action)) t = cands[i];
    }
    if (t) ((void (*)(id, SEL, id))objc_msgSend)(t, s->action, sender);
}
static BOOL ctl_send(id self, SEL c, SEL a, id t) { (void)c; vstate_t *s = vs(self); SEL oa = s->action; id ot = s->target; s->action = a; s->target = t; send_action(self); s->action = oa; s->target = ot; return YES; }

/* NSButton */
static id bt_with_title(id cls, SEL c, id title, id target, SEL action) {
    (void)c;
    id b = MSG(id, MSG0(id, cls, "alloc"), "initWithFrame:", r_make(0, 0, 90, 24));
    vstate_t *s = vs(b);
    keep(&s->title, title); s->target = target; s->action = action;
    v_size_to_fit(b, 0);
    return b;
}
static id bt_checkbox(id cls, SEL c, id title, id target, SEL action) { id b = bt_with_title(cls, c, title, target, action); vs(b)->btype = BT_CHECK; v_size_to_fit(b, 0); return b; }
static id bt_radio(id cls, SEL c, id title, id target, SEL action) { id b = bt_with_title(cls, c, title, target, action); vs(b)->btype = BT_RADIO; v_size_to_fit(b, 0); return b; }
static id bt_init_frame(id self, SEL c, NSRect f) { v_init_frame(self, c, f); vstate_t *s = vs(self); s->kind = K_BUTTON; s->title = cx_str("Button"); s->bezeled = YES; return self; }
static void bt_set_title(id self, SEL c, id t) { (void)c; keep(&vs(self)->title, t); mark(self); }
static id bt_title(id self, SEL c) { (void)c; return vs(self)->title; }
static void bt_set_type(id self, SEL c, u64 t) { (void)c; vs(self)->btype = t == 3 ? BT_CHECK : t == 4 ? BT_RADIO : BT_PUSH; mark(self); }
static void bt_set_state(id self, SEL c, i64 st) { (void)c; vs(self)->state = (int)st; mark(self); }
static i64 bt_state(id self, SEL c) { (void)c; return vs(self)->state; }
static void bt_set_key(id self, SEL c, id k) { (void)c; keep(&vs(self)->key_equiv, k); mark(self); }
static void bt_set_bezel(id self, SEL c, u64 b) { (void)c; vs(self)->bezel_style = (int)b; }
static void bx_set_type(id self, SEL c, i64 t) { (void)c; vs(self)->btype = (int)t; mark(self); }
static void bx_set_radius(id self, SEL c, double r) { (void)c; vs(self)->dval = r; mark(self); }
static void tf_set_bg(id self, SEL c, id col_);
static void bt_set_bordered(id self, SEL c, BOOL b) { (void)c; vs(self)->bezeled = b; mark(self); }

/* NSTextField */
static id tf_init_frame(id self, SEL c, NSRect f) {
    v_init_frame(self, c, f);
    vstate_t *s = vs(self);
    s->kind = K_TEXT; s->editable = YES; s->bezeled = YES; s->draws_bg = YES; s->selectable = YES;
    s->value = cx_str("");
    return self;
}
static id tf_label(id cls, SEL c, id str) {
    (void)c;
    id t = MSG(id, MSG0(id, cls, "alloc"), "initWithFrame:", r_make(0, 0, 100, 17));
    vstate_t *s = vs(t);
    s->editable = NO; s->bezeled = NO; s->draws_bg = NO; s->selectable = NO;
    keep(&s->value, str);
    v_size_to_fit(t, 0);
    return t;
}
static id tf_wrapping_label(id cls, SEL c, id str) { return tf_label(cls, c, str); }
static id tf_field(id cls, SEL c, id str) {
    (void)c;
    id t = MSG(id, MSG0(id, cls, "alloc"), "initWithFrame:", r_make(0, 0, 160, 22));
    keep(&vs(t)->value, str ? str : cx_str(""));
    return t;
}
static void tf_set_editable(id self, SEL c, BOOL b) { (void)c; vs(self)->editable = b; mark(self); }
static BOOL tf_editable(id self, SEL c) { (void)c; return vs(self)->editable; }
static void tf_set_bezeled(id self, SEL c, BOOL b) { (void)c; vs(self)->bezeled = b; mark(self); }
static void tf_set_draws_bg(id self, SEL c, BOOL b) { (void)c; vs(self)->draws_bg = b; mark(self); }
static void tf_set_bg(id self, SEL c, id col_) { (void)c; keep(&vs(self)->bg_color, col_); mark(self); }
static void tf_set_placeholder(id self, SEL c, id p) { (void)c; keep(&vs(self)->placeholder, p); mark(self); }
static void tf_set_selectable(id self, SEL c, BOOL b) { (void)c; vs(self)->selectable = b; }

/* NSSlider / NSProgressIndicator / NSBox / NSStackView */
static id sl_init_frame(id self, SEL c, NSRect f) { v_init_frame(self, c, f); vstate_t *s = vs(self); s->kind = K_SLIDER; s->minv = 0; s->maxv = 1; return self; }
static id sl_with(id cls, SEL c, double v, double mn, double mx, id target, SEL action) {
    (void)c;
    id s = MSG(id, MSG0(id, cls, "alloc"), "initWithFrame:", r_make(0, 0, 160, 22));
    vstate_t *st = vs(s);
    st->dval = v; st->minv = mn; st->maxv = mx; st->target = target; st->action = action;
    return s;
}
static void sl_set_min(id self, SEL c, double v) { (void)c; vs(self)->minv = v; mark(self); }
static void sl_set_max(id self, SEL c, double v) { (void)c; vs(self)->maxv = v; mark(self); }
static double sl_min(id self, SEL c) { (void)c; return vs(self)->minv; }
static double sl_max(id self, SEL c) { (void)c; return vs(self)->maxv; }
static id pg_init_frame(id self, SEL c, NSRect f) { v_init_frame(self, c, f); vstate_t *s = vs(self); s->kind = K_PROGRESS; s->minv = 0; s->maxv = 100; return self; }
static void pg_inc(id self, SEL c, double d) { (void)c; vs(self)->dval += d; mark(self); }
static id bx_init_frame(id self, SEL c, NSRect f) { v_init_frame(self, c, f); vstate_t *s = vs(self); s->kind = K_BOX; s->title = cx_str(""); return self; }
static id st_init_frame(id self, SEL c, NSRect f) { v_init_frame(self, c, f); vstate_t *s = vs(self); s->kind = K_STACK; s->spacing = 8; s->orientation = 0; return self; }
static id st_with_views(id cls, SEL c, id views) {
    (void)c;
    id s = MSG(id, MSG0(id, cls, "alloc"), "initWithFrame:", r_make(0, 0, 0, 0));
    nsarr_t *a = (nsarr_t *)views;
    for (u64 i = 0; views && i < a->n; i++) v_add_sub(s, 0, a->items[i]);
    return s;
}
static void st_add(id self, SEL c, id v) { v_add_sub(self, c, v); }
static void st_set_orient(id self, SEL c, i64 o) { (void)c; vs(self)->orientation = (int)o; mark(self); }
static void st_set_spacing(id self, SEL c, double sp) { (void)c; vs(self)->spacing = sp; mark(self); }
static id iv_init_frame(id self, SEL c, NSRect f) { v_init_frame(self, c, f); vs(self)->kind = K_IMAGE; return self; }

static NSSize v_intrinsic(id self, SEL c) {
    (void)c;
    vstate_t *s = vs(self);
    id f = s->font ? s->font : dflt_font();
    double size = fnt(f)->size;
    NSSize z = {-1, -1};
    if (s->kind == K_BUTTON) {
        NSSize t = text_size(cx_cstr(s->title), size, fnt(f)->weight);
        if (s->btype == BT_PUSH) { z.w = t.w + 28; if (z.w < 70) z.w = 70; z.h = 24; }
        else { z.w = t.w + 24; z.h = 18; }
    } else if (s->kind == K_TEXT) {
        NSSize t = text_size(cx_cstr(s->value ? s->value : cx_str("")), size, fnt(f)->weight);
        z.w = t.w + (s->bezeled ? 10 : 4);
        z.h = t.h + (s->bezeled ? 6 : 1);
    } else if (s->kind == K_SLIDER) { z.w = 160; z.h = 22; }
    return z;
}

/* empilhamento simples para NSStackView */
static void layout_stack(id v) {
    vstate_t *s = vs(v);
    double pos = 0, cross = 0;
    for (u32 i = 0; i < s->nsub; i++) {
        vstate_t *cs = vs(s->sub[i]);
        if (cs->hidden) continue;
        NSSize z = v_intrinsic(s->sub[i], 0);
        if (z.w > 0 && cs->frame.s.w <= 0) cs->frame.s = z;
        if (s->orientation == 0) { if (cs->frame.s.h > cross) cross = cs->frame.s.h; }
        else if (cs->frame.s.w > cross) cross = cs->frame.s.w;
    }
    /* horizontal: da esquerda para a direita; vertical: de cima para baixo */
    double top = s->frame.s.h;
    for (u32 i = 0; i < s->nsub; i++) {
        vstate_t *cs = vs(s->sub[i]);
        if (cs->hidden) continue;
        if (s->orientation == 0) { cs->frame.o.x = pos; cs->frame.o.y = (s->frame.s.h - cs->frame.s.h) / 2; pos += cs->frame.s.w + s->spacing; }
        else { cs->frame.o.x = 0; top -= cs->frame.s.h; cs->frame.o.y = top; top -= s->spacing; }
    }
    (void)cross;
}

/* desenho dos controles nativos (coordenadas em pixels do conteúdo) */
static void draw_builtin(id v, absr_t *a) {
    vstate_t *s = vs(v);
    id f = s->font ? s->font : dflt_font();
    double size = fnt(f)->size;
    int wt = fnt(f)->weight;
    double x = a->tlx, y = a->tly, w = a->w, h = a->h;
    switch (s->kind) {
    case K_BUTTON: {
        const char *t = cx_cstr(s->title ? s->title : cx_str(""));
        if (s->image && !t[0]) { draw_image_fit(s->image, x + 2, y + 2, w - 4, h - 4); break; }
        u32 n = (u32)strlen(t);
        gui_metrics_t m = measure(t, n, size, wt);
        u32 tc = s->enabled ? 0xFF1D1D1F : 0xFF9A9A9E;
        if (s->btype == BT_PUSH) {
            BOOL def = s->key_equiv && !strcmp(cx_cstr(s->key_equiv), "\r");
            double bh = h > 28 ? h : 22, by = y + (h - bh) / 2;
            if (s->bezeled) {
                if (def && s->enabled) g_rrect(x + 1, by + 1, w - 2, bh - 2, 6, s->highlighted ? 0xFF0060D0 : accent);
                else {
                    g_rrect(x + 1, by + 1, w - 2, bh - 2, 6, s->highlighted ? 0xFFDCDCE0 : 0xFFFFFFFF);
                    g_rstroke(x + 1, by + 1, w - 2, bh - 2, 6, 1, 0x30000000);
                }
                if (def && s->enabled) tc = 0xFFFFFFFF;
            } else if (s->highlighted) g_rrect(x, y, w, h, 5, 0x18000000);
            g_text(x + (w - m.width) / 2, by + (bh + m.ascent - m.descent) / 2, size, wt, tc, t, n);
        } else {
            double bs = 14, bx = x + 1, byy = y + (h - bs) / 2;
            BOOL on = s->state == 1;
            if (s->btype == BT_CHECK) {
                g_rrect(bx, byy, bs, bs, 3.5, on ? accent : 0xFFFFFFFF);
                if (!on) g_rstroke(bx, byy, bs, bs, 3.5, 1, 0x40000000);
                if (on) { g_line(bx + 3.2, byy + 7.2, bx + 6, byy + 10, 2, 0xFFFFFFFF); g_line(bx + 6, byy + 10, bx + 11, byy + 4, 2, 0xFFFFFFFF); }
            } else {
                g_rrect(bx, byy, bs, bs, bs / 2, on ? accent : 0xFFFFFFFF);
                if (!on) g_rstroke(bx, byy, bs, bs, bs / 2, 1, 0x40000000);
                if (on) g_rrect(bx + 4.5, byy + 4.5, 5, 5, 2.5, 0xFFFFFFFF);
            }
            g_text(bx + bs + 6, y + (h + m.ascent - m.descent) / 2, size, wt, tc, t, n);
        }
        break;
    }
    case K_TEXT: {
        const char *t = cx_cstr(s->value ? s->value : cx_str(""));
        u32 n = (u32)strlen(t);
        u32 tc = s->text_color ? argb(s->text_color) : (s->enabled ? 0xF0000000 : 0x80000000);
        BOOL focused = s->window && ws(s->window)->first == v && s->editable;
        if (s->draws_bg || s->bezeled) {
            u32 bg = s->bg_color ? argb(s->bg_color) : 0xFFFFFFFF;
            if (s->bezeled) {
                g_rrect(x, y, w, h, 5, bg);
                g_rstroke(x + 0.5, y + 0.5, w - 1, h - 1, 5, focused ? 2.5 : 1, focused ? 0x99007AFF : 0x33000000);
            } else g_fill(x, y, w, h, bg);
        }
        gui_metrics_t m = measure(t, n, size, wt);
        double pad = s->bezeled ? 5 : 2;
        set_clip(x + 1, y + 1, w - 2, h - 2);
        if (!n && s->placeholder) {
            const char *p = cx_cstr(s->placeholder);
            g_text(x + pad, y + (h + m.ascent - m.descent) / 2, size, wt, 0x55000000, p, (u32)strlen(p));
        } else if (!s->bezeled && !s->editable && h > (m.ascent + m.descent) * 1.8) {
            draw_wrapped(t, x + pad, y + 1, w - 2 * pad, h, size, wt, tc, s->alignment);
        } else {
            double tx = x + pad;
            if (s->alignment == 2) tx = x + (w - m.width) / 2;          /* NSTextAlignmentCenter */
            else if (s->alignment == 1) tx = x + w - pad - m.width;     /* direita */
            double base = y + (h + m.ascent - m.descent) / 2;
            g_text(tx, base, size, wt, tc, t, n);
            if (focused) {
                gui_metrics_t cm = measure(t, (u32)s->caret, size, wt);
                g_fill(tx + cm.width, base - m.ascent, 1.5, m.ascent + m.descent, accent);
            }
        }
        set_clip(x, y, w, h);
        break;
    }
    case K_TV: draw_textview(v, a); break;
    case K_SLIDER: {
        double range = s->maxv - s->minv, t = range > 0 ? (s->dval - s->minv) / range : 0;
        if (t < 0) t = 0; if (t > 1) t = 1;
        double cy = y + h / 2, x0 = x + 9, x1 = x + w - 9, kx = x0 + (x1 - x0) * t;
        g_rrect(x0, cy - 2, x1 - x0, 4, 2, 0x26000000);
        g_rrect(x0, cy - 2, kx - x0, 4, 2, accent);
        g_rrect(kx - 9, cy - 9, 18, 18, 9, 0xFFFFFFFF);
        g_rstroke(kx - 9, cy - 9, 18, 18, 9, 1, 0x33000000);
        break;
    }
    case K_PROGRESS: {
        double range = s->maxv - s->minv, t = range > 0 ? (s->dval - s->minv) / range : 0;
        if (t < 0) t = 0; if (t > 1) t = 1;
        double cy = y + h / 2;
        g_rrect(x, cy - 3, w, 6, 3, 0x1F000000);
        if (t > 0) g_rrect(x, cy - 3, w * t, 6, 3, accent);
        break;
    }
    case K_TABLE: draw_table(v, a); break;
    case K_BOX: {
        if (s->btype == 2) { g_fill(x, y + h / 2, w, 1, 0x1F000000); break; }            /* NSBoxSeparator */
        if (s->bg_color) { g_rrect(x, y, w, h, s->dval, argb(s->bg_color)); break; }   /* NSBoxCustom com fillColor */
        const char *t = cx_cstr(s->title ? s->title : cx_str(""));
        if (s->image && !t[0]) { draw_image_fit(s->image, x + 2, y + 2, w - 4, h - 4); break; }
        u32 n = (u32)strlen(t);
        gui_metrics_t m = measure(t, n, 11, 0);
        double top = n ? m.ascent + m.descent + 4 : 0;
        g_rrect(x, y + top, w, h - top, 6, 0x0D000000);
        g_rstroke(x + 0.5, y + top + 0.5, w - 1, h - top - 1, 6, 1, 0x1A000000);
        if (n) g_text(x + 4, y + m.ascent, 11, 0, 0xB0000000, t, n);
        break;
    }
    }
}

/* ================================================================ desenho da árvore */
static double clip_x0 = -1e9, clip_y0 = -1e9, clip_x1 = 1e9, clip_y1 = 1e9;
static void set_clip(double x, double y, double w, double h) {
    double x0 = x > clip_x0 ? x : clip_x0, y0 = y > clip_y0 ? y : clip_y0;
    double x1 = x + w < clip_x1 ? x + w : clip_x1, y1 = y + h < clip_y1 ? y + h : clip_y1;
    g_clip(x0, y0, x1 > x0 ? x1 - x0 : 0, y1 > y0 ? y1 - y0 : 0);
}
static void draw_view_tree(id v, absr_t *a) {
    vstate_t *s = vs(v);
    if (s->hidden || a->w <= 0 || a->h <= 0) return;
    if (s->kind == K_STACK) layout_stack(v);
    if (s->kind == K_SCROLL) layout_scroll(v);
    double sx0 = clip_x0, sy0 = clip_y0, sx1 = clip_x1, sy1 = clip_y1;
    ctx.tlx = a->tlx; ctx.tly = a->tly; ctx.w = a->w; ctx.h = a->h; ctx.flipped = a->flipped;
    set_clip(a->tlx, a->tly, a->w, a->h);
    if (cx_layer_draw_hook) cx_layer_draw_hook(v, 0, a->tlx, a->tly, a->w, a->h, clip_x0, clip_y0, clip_x1, clip_y1);   /* gancho QuartzCore */
    if (s->kind == K_IMAGE && s->image) draw_image_fit(s->image, a->tlx, a->tly, a->w, a->h);
    if (s->kind != K_VIEW && s->kind != K_STACK && s->kind != K_IMAGE && s->kind != K_SCROLL) draw_builtin(v, a);
    set_clip(a->tlx, a->tly, a->w, a->h);
    ctx.tlx = a->tlx; ctx.tly = a->tly; ctx.w = a->w; ctx.h = a->h; ctx.flipped = a->flipped;
    ctx.fill = ctx.stroke = 0xFF000000;
    ctx.line_width = 1;
    /* drawRect: do app (ou da subclasse) */
    IMP imp = cx_lookup(v->isa, sel_registerName("drawRect:"));
    if (imp && imp != (IMP)v_draw_rect) {
        NSRect b = r_make(0, 0, a->w, a->h);
        ((void (*)(id, SEL, NSRect))imp)(v, sel_registerName("drawRect:"), b);
    }
    if (cx_layer_draw_hook) cx_layer_draw_hook(v, 1, a->tlx, a->tly, a->w, a->h, clip_x0, clip_y0, clip_x1, clip_y1);   /* gancho QuartzCore */
    s->needs = NO;
    if (a->tlx > clip_x0) clip_x0 = a->tlx;
    if (a->tly > clip_y0) clip_y0 = a->tly;
    if (a->tlx + a->w < clip_x1) clip_x1 = a->tlx + a->w;
    if (a->tly + a->h < clip_y1) clip_y1 = a->tly + a->h;
    for (u32 i = 0; i < s->nsub; i++) {
        id cv = s->sub[i];
        vstate_t *cs = vs(cv);
        absr_t ca = {a->tlx + cs->frame.o.x, a->flipped ? a->tly + cs->frame.o.y : a->tly + a->h - cs->frame.o.y - cs->frame.s.h,
                     cs->frame.s.w, cs->frame.s.h, is_flipped(cv)};
        draw_view_tree(cv, &ca);
    }
    if (s->kind == K_SCROLL && s->doc) {
        vstate_t *d = vs(s->doc);
        if (d->frame.s.h > a->h) {
            double vis = a->h / d->frame.s.h, bh = a->h * vis, by = a->tly + s->scroll_y / d->frame.s.h * a->h;
            set_clip(a->tlx, a->tly, a->w, a->h);
            g_rrect(a->tlx + a->w - 8, by + 2, 5, bh - 4 < 12 ? 12 : bh - 4, 2.5, 0x55000000);
        }
    }
    clip_x0 = sx0; clip_y0 = sy0; clip_x1 = sx1; clip_y1 = sy1;
    g_unclip();
}

/* ================================================================ NSWindow */
static id w_init(id self, SEL c, NSRect r, u64 style, u64 backing, BOOL defer) {
    (void)c; (void)backing; (void)defer;
    wstate_t *s = ws(self);
    s->frame = r;
    s->style = style;
    s->title = cx_str("");
    s->released_when_closed = YES;
    s->bg = cx_color(0.93, 0.93, 0.93, 1);
    id cv = MSG(id, MSG0(id, &NSView_c, "alloc"), "initWithFrame:", r_make(0, 0, r.s.w, r.s.h));
    s->content = cv;
    set_window_rec(cv, self);
    vs(cv)->autoresize = 2 | 16;
    return self;
}
static id w_init_simple(id self, SEL c) { return w_init(self, c, r_make(0, 0, 480, 320), 15, 2, NO); }
static id w_with_vc(id cls, SEL c, id vc) { (void)vc; return w_init(MSG0(id, cls, "alloc"), c, r_make(0, 0, 480, 320), 15, 2, NO); }
static void w_set_title(id self, SEL c, id t) {
    (void)c;
    wstate_t *s = ws(self);
    { id nt = t ? MSG0(id, t, "copy") : cx_str(""); objc_release(s->title); s->title = nt; }
    if (s->gid > 0) SYS2(GUI_WIN_TITLE, s->gid, cx_cstr(s->title));
}
static id w_title(id self, SEL c) { (void)c; return ws(self)->title; }
static id w_content(id self, SEL c) { (void)c; return ws(self)->content; }
static void w_set_content(id self, SEL c, id v) {
    (void)c;
    wstate_t *s = ws(self);
    if (!v) return;
    keep(&s->content, v);
    vs(v)->superview = 0;
    vs(v)->frame = r_make(0, 0, s->frame.s.w, s->frame.s.h);
    set_window_rec(v, self);
    s->needs = YES;
}
static void w_set_delegate(id self, SEL c, id d) { (void)c; ws(self)->delegate = d; }
static id w_delegate(id self, SEL c) { (void)c; return ws(self)->delegate; }
static NSRect w_frame(id self, SEL c) { (void)c; NSRect r = ws(self)->frame; r.s.h += 22; return r; }
static NSRect w_content_rect_for(id self, SEL c, NSRect r) { (void)self; (void)c; r.s.h -= 22; return r; }
static NSRect w_frame_rect_for(id self, SEL c, NSRect r) { (void)self; (void)c; r.s.h += 22; return r; }
static void w_set_bg(id self, SEL c, id col_) { (void)c; keep(&ws(self)->bg, col_); ws(self)->needs = YES; }
static id w_bg(id self, SEL c) { (void)c; return ws(self)->bg; }
static BOOL w_visible(id self, SEL c) { (void)c; return ws(self)->visible; }
static void w_set_released(id self, SEL c, BOOL b) { (void)c; ws(self)->released_when_closed = b; }
static id w_first(id self, SEL c) { (void)c; return ws(self)->first; }
static BOOL w_make_first(id self, SEL c, id r) { (void)c; wstate_t *s = ws(self); if (s->first) mark(s->first); s->first = r; if (r) mark(r); return YES; }
static i64 w_number(id self, SEL c) { (void)c; return ws(self)->gid; }
/* NSWindowOcclusionStateVisible = 2. Sem isto o Chromium trata a janela como coberta e não compõe. */
static u64 w_occlusion(id self, SEL c) { (void)self; (void)c; return 2; }
/* r está na base da janela (origem embaixo à esquerda do content, que no mOS coincide com a base). */
static NSRect w_convert_to_screen(id self, SEL c, NSRect r) {
    (void)c;
    wstate_t *s = ws(self);
    r.o.x += s->frame.o.x;
    r.o.y += s->frame.o.y;
    return r;
}

static id app_windows;
static void w_show(id self, SEL c, id sender) {
    (void)c; (void)sender;
    wstate_t *s = ws(self);
    if (s->gid <= 0) {
        u32 fl = (s->style & 8) ? GUI_WF_RESIZABLE : 0;
        if (!(s->style & 1) && s->style != 0) fl |= 0;
        long id_ = SYS4(GUI_WIN_CREATE, cx_cstr(s->title), (long)s->frame.s.w, (long)s->frame.s.h, fl);
        if (id_ <= 0) { cx_fdprintf(2, "AppKit[mOS]: não foi possível criar a janela (%ld)\n", id_); return; }
        s->gid = (int)id_;
        gui_wininfo_t i;
        if (SYS2(GUI_WIN_INFO, s->gid, &i) == 0 && (i.w != (int)s->frame.s.w || i.h != (int)s->frame.s.h)) {
            NSSize old = s->frame.s;
            s->frame.s.w = i.w; s->frame.s.h = i.h;
            (void)old;
            v_set_frame(s->content, 0, r_make(0, 0, i.w, i.h));
        }
        if (!app_windows) app_windows = cx_arr_new(&NSMutableArray_c);
        cx_arr_add(app_windows, self);
    } else SYS1(GUI_WIN_FOCUS, s->gid);
    s->visible = YES;
    s->closed = NO;
    s->needs = YES;
}
static void w_order_out(id self, SEL c, id sender) { (void)c; (void)sender; (void)self; }
static void w_center(id self, SEL c) { (void)self; (void)c; }
static void w_set_min(id self, SEL c, NSSize z) { (void)self; (void)c; (void)z; }
static void w_set_frame(id self, SEL c, NSRect r, BOOL disp) {
    (void)c; (void)disp;
    wstate_t *s = ws(self);
    r.s.h -= 22;
    s->frame.o = r.o;
    if (s->gid <= 0) { s->frame = r; v_set_frame(s->content, 0, r_make(0, 0, r.s.w, r.s.h)); }
}
static void w_set_content_size(id self, SEL c, NSSize z) {
    (void)c;
    wstate_t *s = ws(self);
    if (s->gid <= 0) { s->frame.s = z; v_set_frame(s->content, 0, r_make(0, 0, z.w, z.h)); }
}
static void close_window(id self, BOOL from_wm) {
    wstate_t *s = ws(self);
    if (s->closed) return;
    s->closed = YES;
    s->visible = NO;
    id d = s->delegate;
    if (d && cx_responds(d->isa, sel_registerName("windowWillClose:"))) MSG(void, d, "windowWillClose:", cx_note(NSWindowWillCloseNotification, self, 0));
    cx_post(cx_note(NSWindowWillCloseNotification, self, 0));
    if (!from_wm && s->gid > 0) SYS1(GUI_WIN_DESTROY, s->gid);
    s->gid = 0;
}
static void w_close(id self, SEL c) { (void)c; close_window(self, NO); }
static void w_perform_close(id self, SEL c, id sender) {
    (void)c; (void)sender;
    wstate_t *s = ws(self);
    if (s->delegate && cx_responds(s->delegate->isa, sel_registerName("windowShouldClose:")) && !MSG(BOOL, s->delegate, "windowShouldClose:", self)) return;
    close_window(self, NO);
}
static void w_display(id self, SEL c) { (void)c; ws(self)->needs = YES; }

static void autolayout_window(id w);
static void render_window(id w) {
    wstate_t *s = ws(w);
    if (s->gid <= 0 || !s->visible) return;
    vs(s->content)->frame = r_make(0, 0, s->frame.s.w, s->frame.s.h);
    autolayout_window(w);
    ncmd = 0;
    u32 bg = argb(s->bg);
    put(GC_CLEAR, &bg, 4);
    absr_t a = {0, 0, s->frame.s.w, s->frame.s.h, is_flipped(s->content)};
    clip_x0 = clip_y0 = -1e9; clip_x1 = clip_y1 = 1e9;
    vs(s->content)->frame = r_make(0, 0, s->frame.s.w, s->frame.s.h);
    cx_gui_cur_gid = s->gid;                     /* gancho QuartzCore */
    draw_view_tree(s->content, &a);
    flush_to(s->gid);
    SYS1(GUI_PRESENT, s->gid);
    s->needs = NO;
}
static BOOL tree_needs(id v) {
    vstate_t *s = vs(v);
    if (s->needs) return YES;
    for (u32 i = 0; i < s->nsub; i++) if (tree_needs(s->sub[i])) return YES;
    return NO;
}

/* ================================================================ NSEvent */
typedef struct { u64 type; double x, y; u64 mods; u16 keycode; id chars; i64 clicks; id window; double dy; } evstate_t;
static evstate_t *evs(id e) { return ST(e); }
static u64 ev_type(id self, SEL c) { (void)c; return evs(self)->type; }
static NSPoint ev_loc(id self, SEL c) { (void)c; NSPoint p = {evs(self)->x, evs(self)->y}; return p; }
static u64 ev_mods(id self, SEL c) { (void)c; return evs(self)->mods; }
static u16 ev_keycode(id self, SEL c) { (void)c; return evs(self)->keycode; }
static id ev_chars(id self, SEL c) { (void)c; return evs(self)->chars ? evs(self)->chars : cx_str(""); }
static i64 ev_clicks(id self, SEL c) { (void)c; return evs(self)->clicks; }
static id ev_window(id self, SEL c) { (void)c; return evs(self)->window; }
static double ev_dy(id self, SEL c) { (void)c; return evs(self)->dy; }
static double ev_zero(id self, SEL c) { (void)self; (void)c; return 0; }
static u64 g_mods;
static u64 ev_cmods(id cls, SEL c) { (void)cls; (void)c; return g_mods; }

static u64 mac_mods(u32 m) { return ((m & 1) ? 1u << 17 : 0) | ((m & 2) ? 1u << 18 : 0) | ((m & 4) ? 1u << 19 : 0) | ((m & 8) ? 1u << 20 : 0); }
static u16 mac_keycode(int k) {
    static const char map[] = "asdfhgzxcv\0bqweryt123465=97-80]ou[ip\0lj'k;\\,/nm.\0\0`";
    if (k >= 'A' && k <= 'Z') k += 32;
    for (int i = 0; i < (int)sizeof map; i++) if (map[i] == k && k) return (u16)i;
    switch (k) {
    case 10: return 36; case 9: return 48; case ' ': return 49; case 8: return 51; case 27: return 53; case 0x108: return 117;
    case 0x100: return 126; case 0x101: return 125; case 0x102: return 123; case 0x103: return 124;
    case 0x104: return 115; case 0x105: return 119; case 0x106: return 116; case 0x107: return 121;
    }
    return 0x7F;
}
static id make_event(u64 type, id win, double px, double py, gui_event_t *g) {
    id e = objc_autorelease(cx_alloc_instance(&NSEvent_c));
    evstate_t *s = calloc(1, sizeof *s);
    ST(e) = s;
    s->type = type;
    s->window = win;
    s->x = px;
    s->y = win ? win_h(win) - py : py;
    s->mods = mac_mods(g->mods);
    s->clicks = g->clicks;
    s->dy = -g->w;
    if (type == 10 || type == 11) {          /* keyDown / keyUp */
        s->keycode = mac_keycode(g->key);
        u32 cp = 0;
        switch (g->key) {
        case 0x100: cp = 0xF700; break; case 0x101: cp = 0xF701; break; case 0x102: cp = 0xF702; break; case 0x103: cp = 0xF703; break;
        case 10: cp = 13; break; case 8: cp = 127; break; case 0x108: cp = 0xF728; break;
        }
        if (cp) {
            char b[4];
            int n = cp < 0x80 ? (b[0] = (char)cp, 1) : (b[0] = (char)(0xE0 | cp >> 12), b[1] = (char)(0x80 | ((cp >> 6) & 0x3F)), b[2] = (char)(0x80 | (cp & 0x3F)), 3);
            s->chars = cx_str_n(b, (u64)n);
        } else if (g->text[0]) s->chars = cx_str(g->text);
        else if (g->key > 0 && g->key < 128) { char b[2] = {(char)g->key, 0}; s->chars = cx_str(b); }
    }
    return e;
}

/* ================================================================ eventos: views nativas */
static id hit(id v, absr_t *a, double x, double y) {
    vstate_t *s = vs(v);
    if (s->hidden || x < a->tlx || y < a->tly || x >= a->tlx + a->w || y >= a->tly + a->h) return 0;
    for (u32 i = s->nsub; i-- > 0;) {
        id cv = s->sub[i];
        vstate_t *cs = vs(cv);
        absr_t ca = {a->tlx + cs->frame.o.x, a->flipped ? a->tly + cs->frame.o.y : a->tly + a->h - cs->frame.o.y - cs->frame.s.h,
                     cs->frame.s.w, cs->frame.s.h, is_flipped(cv)};
        id r = hit(cv, &ca, x, y);
        if (r) return r;
    }
    return v;
}
static double slider_value_at(id v, double px) {
    vstate_t *s = vs(v);
    absr_t a;
    if (!abs_rect(v, &a)) return s->dval;
    double t = (px - (a.tlx + 9)) / (a.w - 18);
    if (t < 0) t = 0; if (t > 1) t = 1;
    return s->minv + t * (s->maxv - s->minv);
}
static void ctl_mouse_down(id self, SEL c, id ev) {
    (void)c;
    vstate_t *s = vs(self);
    if (!s->enabled) return;
    if (s->kind == K_BUTTON) { s->highlighted = YES; mark(self); }
    else if (s->kind == K_SLIDER) {
        absr_t a;
        abs_rect(self, &a);
        s->dval = slider_value_at(self, evs(ev)->x);
        send_action(self);
        mark(self);
    } else if (s->kind == K_TEXT && s->editable) {
        id w = s->window;
        if (w) w_make_first(w, 0, self);
        s->caret = (int)strlen(cx_cstr(s->value ? s->value : cx_str("")));
    }
}
static void ctl_mouse_dragged(id self, SEL c, id ev) {
    (void)c;
    vstate_t *s = vs(self);
    if (s->kind == K_SLIDER && s->enabled) { s->dval = slider_value_at(self, evs(ev)->x); send_action(self); mark(self); }
}
static void ctl_mouse_up(id self, SEL c, id ev) {
    (void)c;
    vstate_t *s = vs(self);
    if (s->kind != K_BUTTON || !s->highlighted) return;
    s->highlighted = NO;
    mark(self);
    absr_t a;
    double x = evs(ev)->x, y = win_h(s->window) - evs(ev)->y;
    if (!abs_rect(self, &a) || x < a.tlx || x >= a.tlx + a.w || y < a.tly || y >= a.tly + a.h) return;
    if (s->btype == BT_CHECK) s->state = !s->state;
    if (s->btype == BT_RADIO) {
        s->state = 1;
        /* desmarca os irmãos com a mesma ação */
        vstate_t *ps = s->superview ? vs(s->superview) : 0;
        for (u32 i = 0; ps && i < ps->nsub; i++) { vstate_t *o = vs(ps->sub[i]); if (ps->sub[i] != self && o->kind == K_BUTTON && o->btype == BT_RADIO && o->action == s->action) { o->state = 0; mark(ps->sub[i]); } }
    }
    send_action(self);
}
static void tf_key(id self, id ev) {
    vstate_t *s = vs(self);
    evstate_t *e = evs(ev);
    const char *t = cx_cstr(s->value ? s->value : cx_str(""));
    int n = (int)strlen(t);
    if (s->caret > n) s->caret = n;
    const char *ch = e->chars ? cx_cstr(e->chars) : "";
    if (e->keycode == 51) {                       /* apagar */
        if (s->caret > 0) {
            int k = s->caret - 1;
            while (k > 0 && ((u8)t[k] & 0xC0) == 0x80) k--;
            char *b = malloc((size_t)n + 1);
            memcpy(b, t, (size_t)k);
            strcpy(b + k, t + s->caret);
            s->value = cx_str(b);
            free(b);
            s->caret = k;
        }
    } else if (e->keycode == 123) { if (s->caret > 0) { s->caret--; while (s->caret > 0 && ((u8)t[s->caret] & 0xC0) == 0x80) s->caret--; } }
    else if (e->keycode == 124) { if (s->caret < n) { s->caret++; while (s->caret < n && ((u8)t[s->caret] & 0xC0) == 0x80) s->caret++; } }
    else if (e->keycode == 36) { send_action(self); return; }
    else if (ch[0] && (u8)ch[0] >= 32 && !(e->mods & (1u << 20))) {
        size_t cl = strlen(ch);
        char *b = malloc((size_t)n + cl + 1);
        memcpy(b, t, (size_t)s->caret);
        memcpy(b + s->caret, ch, cl);
        strcpy(b + s->caret + cl, t + s->caret);
        s->value = cx_str(b);
        free(b);
        s->caret += (int)cl;
    } else return;
    mark(self);
    bind_push(self);
    id d = s->delegate;
    if (d && cx_responds(d->isa, sel_registerName("controlTextDidChange:"))) MSG(void, d, "controlTextDidChange:", cx_note(NSControlTextDidChangeNotification, self, 0));
}
static void ctl_set_delegate(id self, SEL c, id d) { (void)c; vs(self)->delegate = d; }
static id ctl_delegate(id self, SEL c) { (void)c; return vs(self)->delegate; }
static BOOL v_accepts_first(id self, SEL c) { (void)c; vstate_t *s = vs(self); return (s->kind == K_TEXT && s->editable) || (s->kind == K_TV && s->selectable); }

/* ================================================================ NSTextView
 * Texto multilinha editável com quebra de linha, cursor, clique para posicionar e altura que cresce
 * dentro de um NSScrollView (como o documentView do Cocoa). Texto simples (uma fonte e uma cor). */
#define TV_INSET 5
static void tx_copy(id self, SEL c, id sender);
static void tx_cut(id self, SEL c, id sender);
static void tx_paste(id self, SEL c, id sender);
static void tx_select_all(id self, SEL c, id sender);
static void tx_undo(id self, SEL c, id sender);
static void tx_redo(id self, SEL c, id sender);
static void tx_undo_push(id self, BOOL typing);
static void tx_replace(id self, int a, int b, const char *ins);
static int tx_sel(vstate_t *s, int *a, int *b);
typedef struct { u32 start, len; } tx_line_t;
static void tx_draw_selection(id v, absr_t *a, const char *t, tx_line_t *L, u32 n, double size, int wt, double lh);
/* quebra o texto em linhas para a largura w; devolve o nº de linhas (lines alocado com malloc) */
static u32 tx_lines(const char *s, double w, double size, int wt, tx_line_t **out) {
    u32 cap = 16, n = 0;
    tx_line_t *L = malloc(cap * sizeof *L);
    const char *p = s;
    for (;;) {
        const char *best = p, *q = p;
        while (*q && *q != '\n') {
            const char *e = q;
            while (*e && *e != ' ' && *e != '\n') e++;
            if (measure(p, (u32)(e - p), size, wt).width > w && best != p) break;
            best = e;
            q = *e == ' ' ? e + 1 : e;
            if (measure(p, (u32)(best - p), size, wt).width > w) break;
        }
        if (best == p && *q && *q != '\n') { best = q; if (best == p) best = p + 1; }
        if (n == cap) { cap *= 2; L = realloc(L, cap * sizeof *L); }
        L[n].start = (u32)(p - s);
        L[n].len = (u32)(best - p);
        n++;
        p = best;
        if (*p == ' ') { while (*p == ' ') p++; L[n - 1].len = (u32)(p - s) - L[n - 1].start; }
        if (*p == '\n') { p++; if (!*p) { if (n == cap) { cap *= 2; L = realloc(L, cap * sizeof *L); } L[n].start = (u32)(p - s); L[n].len = 0; n++; break; } continue; }
        if (!*p) break;
    }
    *out = L;
    return n;
}
static double tx_line_h(id v) { vstate_t *s = vs(v); id f = s->font ? s->font : dflt_font(); gui_metrics_t m = measure("Ag", 2, fnt(f)->size, fnt(f)->weight); return m.ascent + m.descent + 3; }
/* ajusta a altura ao texto (no mínimo a área visível do scroll view) */
static void tx_fit(id v, double width) {
    vstate_t *s = vs(v);
    id f = s->font ? s->font : dflt_font();
    tx_line_t *L;
    u32 n = tx_lines(cx_cstr(s->value ? s->value : cx_str("")), width - 2 * TV_INSET, fnt(f)->size, fnt(f)->weight, &L);
    free(L);
    double h = n * tx_line_h(v) + 2 * TV_INSET;
    double minh = s->superview && vs(s->superview)->kind == K_SCROLL ? vs(s->superview)->frame.s.h : s->frame.s.h;
    if (h < minh) h = minh;
    if (s->superview && vs(s->superview)->kind == K_SCROLL) s->frame.s.h = h;
}
static void draw_textview(id v, absr_t *a) {
    vstate_t *s = vs(v);
    id f = s->font ? s->font : dflt_font();
    double size = fnt(f)->size;
    int wt = fnt(f)->weight;
    if (s->draws_bg) g_fill(a->tlx, a->tly, a->w, a->h, s->bg_color ? argb(s->bg_color) : 0xFFFFFFFF);
    const char *t = cx_cstr(s->value ? s->value : cx_str(""));
    tx_line_t *L;
    u32 n = tx_lines(t, a->w - 2 * TV_INSET, size, wt, &L);
    gui_metrics_t m = measure("Ag", 2, size, wt);
    double lh = tx_line_h(v), y = a->tly + TV_INSET;
    u32 tc = s->text_color ? argb(s->text_color) : 0xE6000000;
    BOOL focused = s->window && ws(s->window)->first == v && s->editable;
    tx_draw_selection(v, a, t, L, n, size, wt, lh);
    for (u32 i = 0; i < n; i++, y += lh) {
        if (y + lh < clip_y0 || y > clip_y1) continue;
        u32 len = L[i].len;
        while (len && (t[L[i].start + len - 1] == ' ')) len--;
        if (len) g_text(a->tlx + TV_INSET, y + m.ascent, size, wt, tc, t + L[i].start, len);
        if (focused && (u32)s->caret >= L[i].start && ((u32)s->caret < L[i].start + L[i].len || i == n - 1 || ((u32)s->caret == L[i].start + L[i].len && t[s->caret] == '\n'))) {
            double cx = a->tlx + TV_INSET + measure(t + L[i].start, (u32)s->caret - L[i].start, size, wt).width;
            g_fill(cx, y, 1.5, m.ascent + m.descent, accent);
            focused = NO;
        }
    }
    free(L);
}
/* posição no texto mais próxima de um ponto (coordenadas da view, topo-esquerda) */
static int tx_index_at(id v, double px, double py) {
    vstate_t *s = vs(v);
    id f = s->font ? s->font : dflt_font();
    const char *t = cx_cstr(s->value ? s->value : cx_str(""));
    tx_line_t *L;
    u32 n = tx_lines(t, s->frame.s.w - 2 * TV_INSET, fnt(f)->size, fnt(f)->weight, &L);
    int li = (int)((py - TV_INSET) / tx_line_h(v));
    if (li < 0) li = 0;
    if ((u32)li >= n) li = (int)n - 1;
    u32 st = L[li].start, len = L[li].len;
    while (len && (t[st + len - 1] == '\n')) len--;
    int best = (int)(st + len);
    for (u32 k = 0; k <= len; k++) {
        if (k < len && ((u8)t[st + k] & 0xC0) == 0x80) continue;
        if (measure(t + st, k, fnt(f)->size, fnt(f)->weight).width >= px - TV_INSET - 2) { best = (int)(st + k); break; }
    }
    free(L);
    return best;
}
static void tx_changed(id self) {
    vstate_t *s = vs(self);
    if (s->superview && vs(s->superview)->kind == K_SCROLL) {
        id sv = s->superview;
        tx_fit(self, vs(sv)->frame.s.w);
        /* mantém o cursor visível */
        id f = s->font ? s->font : dflt_font();
        const char *t = cx_cstr(s->value);
        tx_line_t *L;
        u32 n = tx_lines(t, s->frame.s.w - 2 * TV_INSET, fnt(f)->size, fnt(f)->weight, &L);
        u32 li = 0;
        for (u32 i = 0; i < n; i++) if ((u32)s->caret >= L[i].start) li = i;
        free(L);
        double cy = TV_INSET + li * tx_line_h(self), vh = vs(sv)->frame.s.h;
        vstate_t *ss = vs(sv);
        if (cy + tx_line_h(self) > ss->scroll_y + vh) ss->scroll_y = cy + tx_line_h(self) + TV_INSET - vh;
        if (cy < ss->scroll_y) ss->scroll_y = cy - TV_INSET;
        layout_scroll(sv);
        mark(sv);
    }
    mark(self);
    bind_push(self);
    id d = s->delegate;
    if (d && cx_responds(d->isa, sel_registerName("textDidChange:"))) MSG(void, d, "textDidChange:", cx_note(cx_str("NSTextDidChangeNotification"), self, 0));
}
static void tx_insert(id self, const char *ins) {
    vstate_t *s = vs(self);
    const char *t = cx_cstr(s->value ? s->value : cx_str(""));
    int n = (int)strlen(t);
    if (s->caret > n) s->caret = n;
    size_t cl = strlen(ins);
    char *b = malloc((size_t)n + cl + 1);
    memcpy(b, t, (size_t)s->caret);
    memcpy(b + s->caret, ins, cl);
    strcpy(b + s->caret + cl, t + s->caret);
    keep(&s->value, cx_str(b));
    free(b);
    s->caret += (int)cl;
    tx_changed(self);
}
static void tx_key(id self, id ev) {
    vstate_t *s = vs(self);
    evstate_t *e = evs(ev);
    const char *t = cx_cstr(s->value ? s->value : cx_str(""));
    int n = (int)strlen(t);
    if (s->caret > n) s->caret = n;
    const char *ch = e->chars ? cx_cstr(e->chars) : "";
    BOOL cmd = (e->mods & (1u << 20)) != 0, shift = (e->mods & (1u << 17)) != 0;
    int a, b, sel = tx_sel(s, &a, &b);
    if (cmd) {
        char k = (char)(ch[0] | 0x20);
        if (k == 'a') tx_select_all(self, 0, 0);
        else if (k == 'c') tx_copy(self, 0, 0);
        else if (k == 'x') tx_cut(self, 0, 0);
        else if (k == 'v') tx_paste(self, 0, 0);
        else if (k == 'z') { if (shift) tx_redo(self, 0, 0); else tx_undo(self, 0, 0); }
        return;
    }
    if (e->keycode == 123 || e->keycode == 124 || e->keycode == 125 || e->keycode == 126) {
        if (shift && !s->has_anchor) { s->anchor = s->caret; s->has_anchor = YES; }
        if (!shift && sel) { s->caret = e->keycode == 123 || e->keycode == 126 ? a : b; s->has_anchor = NO; mark(self); return; }
        if (!shift) s->has_anchor = NO;
        int k = s->caret;
        if (e->keycode == 123) { if (k > 0) { k--; while (k > 0 && ((u8)t[k] & 0xC0) == 0x80) k--; } }
        else if (e->keycode == 124) { if (k < n) { k++; while (k < n && ((u8)t[k] & 0xC0) == 0x80) k++; } }
        else if (e->keycode == 126) { while (k > 0 && t[k - 1] != '\n') k--; if (k > 0) { k--; while (k > 0 && t[k - 1] != '\n') k--; } }
        else { while (k < n && t[k] != '\n') k++; if (k < n) k++; }
        s->caret = k;
        s->last_typing = NO;
        mark(self);
        return;
    }
    if (!s->editable) return;
    if (e->keycode == 51) {
        if (sel) { tx_undo_push(self, NO); tx_replace(self, a, b, ""); return; }
        if (s->caret > 0) {
            int k = s->caret - 1;
            while (k > 0 && ((u8)t[k] & 0xC0) == 0x80) k--;
            tx_undo_push(self, YES);
            tx_replace(self, k, s->caret, "");
        }
        return;
    }
    const char *ins = 0;
    if (e->keycode == 36 || ch[0] == '\r' || ch[0] == '\n') ins = "\n";
    else if (ch[0] == '\t') ins = "    ";
    else if (ch[0] && (u8)ch[0] >= 32) ins = ch;
    if (!ins) return;
    tx_undo_push(self, ins[0] != '\n' && ins[0] != ' ' && !sel);
    tx_replace(self, a, b, ins);
}static id tx_init_frame(id self, SEL c, NSRect f) {
    v_init_frame(self, c, f);
    vstate_t *s = vs(self);
    s->kind = K_TV; s->editable = YES; s->selectable = YES; s->draws_bg = YES;
    s->value = cx_str("");
    s->autoresize = 2;
    return self;
}
static id tx_string(id self, SEL c) { (void)c; return vs(self)->value ? vs(self)->value : cx_str(""); }
static void tx_set_string(id self, SEL c, id str) {
    (void)c;
    vstate_t *s = vs(self);
    keep(&s->value, str ? MSG0(id, str, "copy") : cx_str(""));
    objc_release(s->value);
    s->caret = (int)strlen(cx_cstr(s->value));
    s->has_anchor = NO;
    if (s->superview && vs(s->superview)->kind == K_SCROLL) tx_fit(self, vs(s->superview)->frame.s.w);
    mark(s->superview ? s->superview : self);
}
static void tx_insert_text(id self, SEL c, id str) { (void)c; if (str) tx_insert(self, cx_cstr(cx_is_str(str) ? str : MSG0(id, str, "string"))); }
static void tx_append_astr(id self, SEL c, id astr) {
    (void)c;
    vstate_t *s = vs(self);
    id str = cx_is_str(astr) ? astr : MSG0(id, astr, "string");
    s->caret = (int)strlen(cx_cstr(s->value ? s->value : cx_str("")));
    if (str) tx_insert(self, cx_cstr(str));
}
static void tx_set_astr(id self, SEL c, id astr) { tx_set_string(self, c, cx_is_str(astr) ? astr : MSG0(id, astr, "string")); }
static id tx_storage(id self, SEL c) { (void)c; return self; }   /* textStorage: a própria view responde ao essencial */
static u64 tx_length(id self, SEL c) { (void)c; return strlen(cx_cstr(vs(self)->value ? vs(self)->value : cx_str(""))); }
static void tx_set_editable(id self, SEL c, BOOL b) { (void)c; vs(self)->editable = b; }
static BOOL tx_editable(id self, SEL c) { (void)c; return vs(self)->editable; }
static void tx_set_bg(id self, SEL c, id col_) { (void)c; keep(&vs(self)->bg_color, col_); mark(self); }
static void tx_set_draws_bg(id self, SEL c, BOOL b) { (void)c; vs(self)->draws_bg = b; mark(self); }
static void tx_scroll_end(id self, SEL c, id sender) {
    (void)c; (void)sender;
    vstate_t *s = vs(self);
    s->caret = (int)strlen(cx_cstr(s->value ? s->value : cx_str("")));
    if (s->superview && vs(s->superview)->kind == K_SCROLL) { vs(s->superview)->scroll_y = 1e9; layout_scroll(s->superview); mark(s->superview); }
}
typedef struct { u64 loc, len; } tx_range_t;
static tx_range_t tx_sel_range(id self, SEL c) { (void)c; tx_range_t r = {(u64)vs(self)->caret, 0}; return r; }
static void tx_set_sel_range(id self, SEL c, tx_range_t r) { (void)c; vs(self)->caret = (int)r.loc; mark(self); }
static void tx_mouse_down_m(id self, SEL c, id e) {
    (void)c;
    vstate_t *s = vs(self);
    absr_t a;
    if (!abs_rect(self, &a)) return;
    double px = evs(e)->x - a.tlx, py = win_h(s->window) - evs(e)->y - a.tly;
    if (evs(e)->mods & (1u << 17)) { if (!s->has_anchor) { s->anchor = s->caret; s->has_anchor = YES; } }
    else s->has_anchor = NO;
    s->caret = tx_index_at(self, px, py);
    s->last_typing = NO;
    if (s->window) w_make_first(s->window, 0, self);
    mark(self);
}
static id tx_scrollable(id cls, SEL c) {
    (void)c;
    id sv = MSG(id, cx_alloc_instance(&NSScrollView_c), "initWithFrame:", r_make(0, 0, 300, 200));
    id tv = MSG(id, MSG0(id, cls, "alloc"), "initWithFrame:", r_make(0, 0, 300, 200));
    MSG(void, sv, "setDocumentView:", tv);
    objc_release(tv);
    return objc_autorelease(sv);
}

extern u8 *cx_read_file(const char *path, u64 *len);
extern id NSHomeDirectory(void);
/* ================================================================ NSPasteboard
 * Vários pasteboards com nome (geral, find, drag, únicos), cada um com changeCount e um dicionário
 * tipo → valor (NSData, NSString ou property list), como o Chromium usa (ui/base/clipboard). O texto do
 * pasteboard geral também vai para ~/.mOS-pasteboard, compartilhado entre os apps do macOS no mOS: quando o
 * arquivo muda por outro processo, o geral passa a ter só esse texto e o changeCount sobe. */
CLASS(NSPasteboard)
CLASS(NSPasteboardItem)
void cx_appkit_input_init(void);
id NSPasteboardTypeString, NSStringPboardType;
id CFDataCreate(void *alloc, const u8 *bytes, long n);
typedef struct { id name; i64 count; id items; id owner; char *disk; int used; } pbstate_t;
#define PB_MAX 32
static pbstate_t pb_tab[PB_MAX];
static id pb_obj[PB_MAX];
static id pb_cls(const char *n) { return (id)cx_class_named(n); }
static BOOL pb_resp(id o, const char *sel) { return o && cx_responds(object_getClass(o), sel_registerName(sel)); }
static BOOL pb_is_data(id o) { return pb_resp(o, "bytes") && pb_resp(o, "length") && !cx_is_str(o); }
static pbstate_t *pb_st(id self) {
    for (int i = 0; i < PB_MAX; i++) if (pb_obj[i] == self) return &pb_tab[i];
    return &pb_tab[0];
}
static int pb_is_general(pbstate_t *p) { return p->name && !strcmp(cx_cstr(p->name), "Apple CFPasteboard general"); }
static void pb_path(char *out, usize cap) {
    id h = NSHomeDirectory();
    snprintf(out, cap, "%s/.mOS-pasteboard", h ? cx_cstr(h) : "/tmp");
}
static void pb_write(const char *s, usize n) {
    char p[600];
    pb_path(p, sizeof p);
    FILE_ *f = fopen(p, "wb");
    if (!f) return;
    fwrite(s, 1, n, f);
    fclose(f);
}
static id pb_dict(pbstate_t *p) {
    if (!p->items) p->items = objc_retain(MSG0(id, pb_cls("NSMutableDictionary"), "dictionary"));
    return p->items;
}
static void pb_remember(pbstate_t *p, const char *t, usize n) {
    free(p->disk);
    p->disk = malloc(n + 1);
    if (p->disk) { memcpy(p->disk, t, n); p->disk[n] = 0; }
}
/* o geral: se outro processo escreveu um texto novo no arquivo, adota-o */
static void pb_sync(pbstate_t *p) {
    if (!pb_is_general(p)) return;
    char path[600];
    pb_path(path, sizeof path);
    u64 n = 0;
    u8 *b = cx_read_file(path, &n);
    if (!b) return;
    int same = p->disk ? (strlen(p->disk) == n && !memcmp(p->disk, b, n)) : n == 0;
    if (!same) {
        pb_remember(p, (const char *)b, n);
        MSG0(void, pb_dict(p), "removeAllObjects");
        if (n) MSG(void, pb_dict(p), "setObject:forKey:", cx_str_n((const char *)b, n), NSPasteboardTypeString);
        p->owner = 0;
        p->count++;
    }
    free(b);
}
static void pb_store(pbstate_t *p, id v, id type) {
    if (!type) type = NSPasteboardTypeString;
    if (v) MSG(void, pb_dict(p), "setObject:forKey:", v, type);
    else MSG(void, pb_dict(p), "removeObjectForKey:", type);
    if (pb_is_general(p) && cx_is_str(type) && !strcmp(cx_cstr(type), "public.utf8-plain-text")) {
        const char *t = v && cx_is_str(v) ? cx_cstr(v) : "";
        pb_write(t, strlen(t));
        pb_remember(p, t, strlen(t));
    }
}
static id pb_make(const char *name) {
    for (int i = 0; i < PB_MAX; i++)
        if (pb_tab[i].used && !strcmp(cx_cstr(pb_tab[i].name), name)) return pb_obj[i];
    for (int i = 0; i < PB_MAX; i++) {
        if (pb_tab[i].used) continue;
        pb_tab[i].used = 1;
        pb_tab[i].name = objc_retain(cx_str(name));
        pb_tab[i].count = 1;
        pb_obj[i] = cx_alloc_instance(&NSPasteboard_c);
        return pb_obj[i];
    }
    return pb_obj[0];
}
static id pb_general(id cls, SEL c) { (void)cls; (void)c; return pb_make("Apple CFPasteboard general"); }
static id pb_named(id cls, SEL c, id name) { return name ? pb_make(cx_cstr(name)) : pb_general(cls, c); }
static id pb_unique(id cls, SEL c) {
    (void)cls; (void)c;
    static int seq;
    char n[48];
    snprintf(n, sizeof n, "mOS-pasteboard-%d", ++seq);
    return pb_make(n);
}
static id pb_name(id self, SEL c) { (void)c; return pb_st(self)->name; }
static i64 pb_count(id self, SEL c) { (void)c; pbstate_t *p = pb_st(self); pb_sync(p); return p->count; }
static i64 pb_clear(id self, SEL c) {
    (void)c;
    pbstate_t *p = pb_st(self);
    MSG0(void, pb_dict(p), "removeAllObjects");
    if (pb_is_general(p)) { pb_write("", 0); pb_remember(p, "", 0); }
    p->owner = 0;
    return ++p->count;
}
static i64 pb_declare(id self, SEL c, id types, id owner) {
    (void)types;
    i64 n = pb_clear(self, c);
    pb_st(self)->owner = owner;
    return n;
}
static i64 pb_add_types(id self, SEL c, id types, id owner) { (void)c; (void)types; pbstate_t *p = pb_st(self); if (owner) p->owner = owner; return p->count; }
static void pb_release(id self, SEL c) { (void)self; (void)c; }
static id pb_value(id self, id type) {
    pbstate_t *p = pb_st(self);
    pb_sync(p);
    id v = type ? MSG(id, pb_dict(p), "objectForKey:", type) : 0;
    /* tipo declarado e ainda sem dado: o dono fornece sob demanda (pasteboard:provideDataForType:) */
    if (!v && type && pb_resp(p->owner, "pasteboard:provideDataForType:")) {
        MSG(void, p->owner, "pasteboard:provideDataForType:", self, type);
        v = MSG(id, pb_dict(p), "objectForKey:", type);
    }
    return v;
}
static id pb_as_data(id v) {
    if (!v) return 0;
    if (cx_is_str(v)) { const char *t = cx_cstr(v); return objc_autorelease(CFDataCreate(0, (const u8 *)t, (long)strlen(t))); }
    return pb_is_data(v) ? v : 0;
}
static id pb_as_string(id v) {
    if (!v) return 0;
    if (cx_is_str(v)) return v;
    if (pb_is_data(v)) return cx_str_n((const char *)MSG0(const void *, v, "bytes"), (usize)MSG0(u64, v, "length"));
    return 0;
}
static BOOL pb_set_data(id self, SEL c, id d, id type) { (void)c; pb_store(pb_st(self), d, type); return YES; }
static BOOL pb_set_string(id self, SEL c, id s, id type) { (void)c; if (!s) return NO; pb_store(pb_st(self), s, type); return YES; }
static BOOL pb_set_plist(id self, SEL c, id pl, id type) { (void)c; pb_store(pb_st(self), pl, type); return YES; }
static id pb_data(id self, SEL c, id type) { (void)c; return pb_as_data(pb_value(self, type)); }
static id pb_string(id self, SEL c, id type) { (void)c; return pb_as_string(pb_value(self, type ? type : NSPasteboardTypeString)); }
static id pb_plist(id self, SEL c, id type) { (void)c; return pb_value(self, type); }
static id pb_types(id self, SEL c) {
    (void)c;
    pbstate_t *p = pb_st(self);
    pb_sync(p);
    id a = MSG0(id, pb_dict(p), "allKeys");
    return a ? a : objc_autorelease(cx_arr_new(0));
}
static id pb_available(id self, SEL c, id want) {
    (void)c;
    nsarr_t *w = (nsarr_t *)want;
    for (u64 i = 0; want && i < w->n; i++) if (pb_value(self, w->items[i])) return w->items[i];
    return 0;
}
static BOOL pb_can_conform(id self, SEL c, id want) { return pb_available(self, c, want) != 0; }
/* ---- NSPasteboardItem: um dicionário tipo → valor (ivar logo depois do isa, como DEF reserva 8 bytes) */
typedef struct { void *isa; id items; } pbitem_t;
static id pi_dict(id self) {
    pbitem_t *it = (pbitem_t *)self;
    if (!it->items) it->items = objc_retain(MSG0(id, pb_cls("NSMutableDictionary"), "dictionary"));
    return it->items;
}
static id pi_init(id self, SEL c) { (void)c; pi_dict(self); return self; }
static BOOL pi_set(id self, SEL c, id v, id type) { (void)c; if (!type || !v) return NO; MSG(void, pi_dict(self), "setObject:forKey:", v, type); return YES; }
static id pi_get(id self, id type) { return type ? MSG(id, pi_dict(self), "objectForKey:", type) : 0; }
static id pi_data(id self, SEL c, id type) { (void)c; return pb_as_data(pi_get(self, type)); }
static id pi_string(id self, SEL c, id type) { (void)c; return pb_as_string(pi_get(self, type)); }
static id pi_plist(id self, SEL c, id type) { (void)c; return pi_get(self, type); }
static id pi_types(id self, SEL c) { (void)c; return MSG0(id, pi_dict(self), "allKeys"); }
static id pi_available(id self, SEL c, id want) {
    (void)c;
    nsarr_t *w = (nsarr_t *)want;
    for (u64 i = 0; want && i < w->n; i++) if (pi_get(self, w->items[i])) return w->items[i];
    return 0;
}
static BOOL pb_write_objects(id self, SEL c, id arr) {
    nsarr_t *a = (nsarr_t *)arr;
    BOOL any = NO;
    for (u64 i = 0; arr && i < a->n; i++) {
        id o = a->items[i];
        if (cx_is_str(o)) { pb_set_string(self, c, o, NSPasteboardTypeString); any = YES; }
        else if (cx_is_subclass(object_getClass(o), &NSPasteboardItem_c)) {
            id d = pi_dict(o);
            id keys = MSG0(id, d, "allKeys");
            nsarr_t *k = (nsarr_t *)keys;
            for (u64 j = 0; keys && j < k->n; j++) pb_store(pb_st(self), MSG(id, d, "objectForKey:", k->items[j]), k->items[j]);
            any = YES;
        } else if (pb_resp(o, "absoluteString")) {
            id u = MSG0(id, o, "absoluteString");
            pb_store(pb_st(self), u, cx_str("public.url"));
            if (pb_resp(o, "isFileURL") && MSG0(BOOL, o, "isFileURL")) pb_store(pb_st(self), u, cx_str("public.file-url"));
            any = YES;
        }
    }
    return any;
}
/* um item só com tudo o que o pasteboard tem (o Chromium lê pasteboardItems[0]) */
static id pb_items(id self, SEL c) {
    (void)c;
    pbstate_t *p = pb_st(self);
    pb_sync(p);
    id a = cx_arr_new(0);
    id keys = MSG0(id, pb_dict(p), "allKeys");
    nsarr_t *k = (nsarr_t *)keys;
    if (keys && k->n) {
        id it = pi_init(cx_alloc_instance(&NSPasteboardItem_c), 0);
        for (u64 j = 0; j < k->n; j++) pi_set(it, 0, MSG(id, pb_dict(p), "objectForKey:", k->items[j]), k->items[j]);
        cx_arr_add(a, it);
        objc_release(it);
    }
    return objc_autorelease(a);
}
static id pb_read_objects(id self, SEL c, id classes, id opts) {
    (void)opts;
    nsarr_t *cl = (nsarr_t *)classes;
    id out = cx_arr_new(0);
    for (u64 i = 0; classes && i < cl->n; i++) {
        const char *n = cx_class_name((cx_class_t *)cl->items[i]);
        if (!n) continue;
        if (strstr(n, "String")) { id s = pb_string(self, c, NSPasteboardTypeString); if (s) cx_arr_add(out, s); }
        else if (strstr(n, "URL")) {
            id s = pb_as_string(pb_value(self, cx_str("public.file-url")));
            if (!s) s = pb_as_string(pb_value(self, cx_str("public.url")));
            if (s) { id u = MSG(id, pb_cls("NSURL"), "URLWithString:", s); if (u) cx_arr_add(out, u); }
        } else if (strstr(n, "PasteboardItem")) {
            id items = pb_items(self, c);
            nsarr_t *it = (nsarr_t *)items;
            for (u64 j = 0; j < it->n; j++) cx_arr_add(out, it->items[j]);
        }
    }
    return objc_autorelease(out);
}
static BOOL pb_can_read(id self, SEL c, id classes, id opts) { id r = pb_read_objects(self, c, classes, opts); return r && ((nsarr_t *)r)->n > 0; }

/* ================================================================ NSTextView: seleção, edição e desfazer */
static int tx_sel(vstate_t *s, int *a, int *b) {
    if (!s->has_anchor || s->anchor == s->caret) { *a = *b = s->caret; return 0; }
    *a = s->anchor < s->caret ? s->anchor : s->caret;
    *b = s->anchor < s->caret ? s->caret : s->anchor;
    return *b - *a;
}
static void tx_undo_push(id self, BOOL typing) {
    vstate_t *s = vs(self);
    if (!s->undo) { s->undo = objc_retain(cx_arr_new(0)); s->redo = objc_retain(cx_arr_new(0)); }
    nsarr_t *u = (nsarr_t *)s->undo;
    if (typing && s->last_typing && u->n) return;      /* digitação seguida = um passo só */
    s->last_typing = typing;
    cx_arr_add(s->undo, s->value ? s->value : cx_str(""));
    if (u->n > 100) MSG(void, s->undo, "removeObjectAtIndex:", (u64)0);
    MSG0(void, s->redo, "removeAllObjects");
}
/* substitui [a, b) por ins */
static void tx_replace(id self, int a, int b, const char *ins) {
    vstate_t *s = vs(self);
    const char *t = cx_cstr(s->value ? s->value : cx_str(""));
    int n = (int)strlen(t);
    if (a < 0) a = 0;
    if (b > n) b = n;
    size_t cl = strlen(ins);
    char *buf = malloc((size_t)n + cl + 1);
    memcpy(buf, t, (size_t)a);
    memcpy(buf + a, ins, cl);
    strcpy(buf + a + cl, t + b);
    keep(&s->value, cx_str(buf));
    free(buf);
    s->caret = a + (int)cl;
    s->has_anchor = NO;
    tx_changed(self);
}
static void tx_copy(id self, SEL c, id sender) {
    (void)c; (void)sender;
    vstate_t *s = vs(self);
    int a, b;
    if (!tx_sel(s, &a, &b)) return;
    id g = pb_general(0, 0);
    pb_clear(g, 0);
    pb_set_string(g, 0, cx_str_n(cx_cstr(s->value) + a, (usize)(b - a)), NSPasteboardTypeString);
}
static void tx_cut(id self, SEL c, id sender) {
    vstate_t *s = vs(self);
    int a, b;
    if (!s->editable || !tx_sel(s, &a, &b)) return;
    tx_copy(self, c, sender);
    tx_undo_push(self, NO);
    tx_replace(self, a, b, "");
}
static void tx_paste(id self, SEL c, id sender) {
    (void)c; (void)sender;
    vstate_t *s = vs(self);
    id p = pb_string(pb_general(0, 0), 0, NSPasteboardTypeString);
    if (!s->editable || !p) return;
    int a, b;
    tx_sel(s, &a, &b);
    tx_undo_push(self, NO);
    tx_replace(self, a, b, cx_cstr(p));
}
static void tx_select_all(id self, SEL c, id sender) {
    (void)c; (void)sender;
    vstate_t *s = vs(self);
    s->anchor = 0;
    s->has_anchor = YES;
    s->caret = (int)strlen(cx_cstr(s->value ? s->value : cx_str("")));
    mark(self);
}
static void tx_undo(id self, SEL c, id sender) {
    (void)c; (void)sender;
    vstate_t *s = vs(self);
    nsarr_t *u = (nsarr_t *)s->undo;
    if (!u || !u->n) { NSBeep(); return; }
    cx_arr_add(s->redo, s->value ? s->value : cx_str(""));
    id prev = objc_retain(u->items[u->n - 1]);
    MSG0(void, s->undo, "removeLastObject");
    keep(&s->value, prev);
    objc_release(prev);
    s->caret = (int)strlen(cx_cstr(s->value));
    s->has_anchor = NO;
    s->last_typing = NO;
    tx_changed(self);
}
static void tx_redo(id self, SEL c, id sender) {
    (void)c; (void)sender;
    vstate_t *s = vs(self);
    nsarr_t *r = (nsarr_t *)s->redo;
    if (!r || !r->n) { NSBeep(); return; }
    cx_arr_add(s->undo, s->value ? s->value : cx_str(""));
    id nx = objc_retain(r->items[r->n - 1]);
    MSG0(void, s->redo, "removeLastObject");
    keep(&s->value, nx);
    objc_release(nx);
    s->caret = (int)strlen(cx_cstr(s->value));
    s->has_anchor = NO;
    tx_changed(self);
}
static void tx_mouse_dragged(id self, SEL c, id e) {
    (void)c;
    vstate_t *s = vs(self);
    absr_t a;
    if (!abs_rect(self, &a)) return;
    if (!s->has_anchor) { s->anchor = s->caret; s->has_anchor = YES; }
    s->caret = tx_index_at(self, evs(e)->x - a.tlx, win_h(s->window) - evs(e)->y - a.tly);
    mark(self);
}
/* destaque da seleção (chamado pelo desenho, antes do texto) */
static void tx_draw_selection(id v, absr_t *a, const char *t, tx_line_t *L, u32 n, double size, int wt, double lh) {
    vstate_t *s = vs(v);
    int sa, sb;
    if (!tx_sel(s, &sa, &sb)) return;
    double y = a->tly + TV_INSET;
    for (u32 i = 0; i < n; i++, y += lh) {
        int ls = (int)L[i].start, le = (int)(L[i].start + L[i].len);
        int x0 = sa > ls ? sa : ls, x1 = sb < le ? sb : le;
        if (x0 >= x1 && !(sb > le && sa <= le && i + 1 < n)) continue;
        double px0 = measure(t + ls, (u32)(x0 - ls), size, wt).width, px1 = x1 > x0 ? measure(t + ls, (u32)(x1 - ls), size, wt).width : px0 + 4;
        g_fill(a->tlx + TV_INSET + px0, y, px1 - px0, lh, 0x5534A0FF);
    }
}

/* respostas padrão do NSResponder: sobem para a superview */
static void r_mouse_down(id self, SEL c, id e) { (void)c; id sv = cx_is_subclass(self->isa, &NSView_c) ? vs(self)->superview : 0; if (sv) MSG(void, sv, "mouseDown:", e); }
static void r_mouse_up(id self, SEL c, id e) { (void)c; id sv = cx_is_subclass(self->isa, &NSView_c) ? vs(self)->superview : 0; if (sv) MSG(void, sv, "mouseUp:", e); }
static void r_mouse_dragged(id self, SEL c, id e) { (void)c; id sv = cx_is_subclass(self->isa, &NSView_c) ? vs(self)->superview : 0; if (sv) MSG(void, sv, "mouseDragged:", e); }
static void r_right_down(id self, SEL c, id e) { (void)c; id sv = cx_is_subclass(self->isa, &NSView_c) ? vs(self)->superview : 0; if (sv) MSG(void, sv, "rightMouseDown:", e); }
static void r_scroll(id self, SEL c, id e) { (void)c; id sv = cx_is_subclass(self->isa, &NSView_c) ? vs(self)->superview : 0; if (sv) MSG(void, sv, "scrollWheel:", e); }
BOOL cx_ime_native_key(id view, id event);      /* appkit_input.c: método de entrada para os campos nativos */
static void r_key_down(id self, SEL c, id e) {
    (void)c;
    if (cx_is_subclass(self->isa, &NSView_c)) {
        vstate_t *s = vs(self);
        if (((s->kind == K_TEXT && s->editable) || s->kind == K_TV) && cx_ime_native_key(self, e)) return;
        if (s->kind == K_TEXT && s->editable) { tf_key(self, e); return; }
        if (s->kind == K_TV) { tx_key(self, e); return; }
        if (s->superview) { MSG(void, s->superview, "keyDown:", e); return; }
    }
}

/* ---- para appkit_input.c (IME e arrastar/soltar), que não enxerga vstate_t/wstate_t */
/* texto confirmado pelo método de entrada num campo/NSTextView nativo do mOS */
void cx_native_insert_text(id v, const char *utf8) {
    if (!v || !utf8 || !*utf8 || !cx_is_subclass(v->isa, &NSView_c)) return;
    vstate_t *s = vs(v);
    if (s->kind == K_TV) { tx_insert(v, utf8); return; }
    if (s->kind != K_TEXT || !s->editable) return;
    gui_event_t g;
    memset(&g, 0, sizeof g);
    id ev = make_event(10, s->window, 0, 0, &g);
    evs(ev)->chars = cx_str(utf8);
    evs(ev)->keycode = 0x7F;
    evs(ev)->mods = 0;
    tf_key(v, ev);
}
/* retângulo da view na janela, origem em cima à esquerda (pixels do conteúdo) */
BOOL cx_view_rect_in_window(id v, double *x, double *y, double *w, double *h) {
    if (!v || !cx_is_subclass(v->isa, &NSView_c)) return NO;
    absr_t a;
    if (!abs_rect(v, &a)) return NO;
    *x = a.tlx; *y = a.tly; *w = a.w; *h = a.h;
    return YES;
}
int cx_window_gid(id w) { return w && cx_is_subclass(w->isa, &NSWindow_c) ? ws(w)->gid : 0; }
double cx_window_content_h(id w) { return w && cx_is_subclass(w->isa, &NSWindow_c) ? ws(w)->frame.s.h : 0; }
NSRect cx_window_screen_frame(id w) { NSRect z = {{0, 0}, {0, 0}}; return w && cx_is_subclass(w->isa, &NSWindow_c) ? ws(w)->frame : z; }
/* view mais funda sob (x, y) do conteúdo (origem em cima à esquerda) */
id cx_window_hit(id w, double x, double y) {
    if (!w || !cx_is_subclass(w->isa, &NSWindow_c)) return 0;
    wstate_t *s = ws(w);
    if (!s->content) return 0;
    absr_t a = {0, 0, s->frame.s.w, s->frame.s.h, is_flipped(s->content)};
    id v = hit(s->content, &a, x, y);
    return v ? v : s->content;
}

/* ================================================================ NSApplication */
typedef struct { id delegate, menu; BOOL running, launched, terminating; int modal_result; BOOL in_modal; } appstate_t;
static appstate_t app;
static id app_singleton;
/* O renderer do Chromium (RendererMainPlatformDelegate::EnableSandbox) faz CHECK(NSApp == nil)
 * depois de o próprio Init já ter chamado sharedApplication. O singleton continua existindo para
 * quem manda a mensagem; o global exportado fica nulo nos helpers (--type=renderer/gpu/utility
 * ou executável "Code Helper ..."). O processo do app (sem --type=) publica NSApp normalmente. */
static int nsapp_is_helper(void) {
    extern int NXArgc;
    extern char **NXArgv;
    extern const char *__progname;
    if (__progname && strstr(__progname, "Helper")) return 1;
    if (!NXArgv) return 0;
    for (int i = 1; i < NXArgc; i++) {
        const char *a = NXArgv[i];
        if (a && strncmp(a, "--type=", 7) == 0 && a[7]) return 1;
    }
    return 0;
}
static id ap_shared(id cls, SEL c) {
    (void)c;
    if (!app_singleton) {
        app_singleton = MSG0(id, MSG0(id, cls, "alloc"), "init");
        if (!nsapp_is_helper()) NSApp = app_singleton;
    }
    return app_singleton;
}
static void ap_set_delegate(id self, SEL c, id d) { (void)self; (void)c; app.delegate = d; }
static id ap_delegate(id self, SEL c) { (void)self; (void)c; return app.delegate; }
static BOOL ap_set_policy(id self, SEL c, i64 p) { (void)self; (void)c; (void)p; return YES; }
static void ap_set_menu(id self, SEL c, id m) { (void)self; (void)c; keep(&app.menu, m); menus_dirty = YES; }
static id ap_menu(id self, SEL c) { (void)self; (void)c; return app.menu; }
static id ap_windows(id self, SEL c) { (void)self; (void)c; return app_windows ? app_windows : cx_arr_new(&NSMutableArray_c); }
static id ap_key_window(id self, SEL c) {
    (void)self; (void)c;
    nsarr_t *a = (nsarr_t *)app_windows;
    for (u64 i = 0; a && i < a->n; i++) { gui_wininfo_t inf; wstate_t *s = ws(a->items[i]); if (s->gid > 0 && SYS2(GUI_WIN_INFO, s->gid, &inf) == 0 && inf.focused) return a->items[i]; }
    return a && a->n ? a->items[a->n - 1] : 0;
}
static void ap_terminate(id self, SEL c, id sender) {
    (void)c; (void)sender;
    id d = app.delegate;
    if (d && cx_responds(d->isa, sel_registerName("applicationShouldTerminate:")) && MSG(u64, d, "applicationShouldTerminate:", self) == 0) return;
    if (d && cx_responds(d->isa, sel_registerName("applicationWillTerminate:"))) MSG(void, d, "applicationWillTerminate:", cx_note(NSApplicationWillTerminateNotification, self, 0));
    cx_post(cx_note(NSApplicationWillTerminateNotification, self, 0));
    exit(0);
}
static void ap_stop(id self, SEL c, id sender) { (void)self; (void)c; (void)sender; app.running = NO; }
static void ap_finish_launching(id self, SEL c) {
    (void)c;
    if (app.launched) return;
    app.launched = YES;
    { extern void cx_gc_app_launched(void); cx_gc_app_launched(); }   /* GameController: controles já ligados */
    id d = app.delegate;
    id note = cx_note(NSApplicationWillFinishLaunchingNotification, self, 0);
    if (d && cx_responds(d->isa, sel_registerName("applicationWillFinishLaunching:"))) MSG(void, d, "applicationWillFinishLaunching:", note);
    cx_post(note);
    note = cx_note(NSApplicationDidFinishLaunchingNotification, self, 0);
    if (d && cx_responds(d->isa, sel_registerName("applicationDidFinishLaunching:"))) MSG(void, d, "applicationDidFinishLaunching:", note);
    cx_post(note);
}
static id find_window(int gid) {
    nsarr_t *a = (nsarr_t *)app_windows;
    for (u64 i = 0; a && i < a->n; i++) if (ws(a->items[i])->gid == gid) return a->items[i];
    return 0;
}
static BOOL menu_shortcut(gui_event_t *g);
id cx_hover_view(id w);              /* appkit_input.c */
void cx_set_hover_view(id w, id v);
static void dispatch(gui_event_t *g) {
    if (g->type == GEV_QUIT) { MSG(void, NSApp, "terminate:", (id)0); return; }
    if (g->type == GEV_MENU) { perform_menu(g->key); return; }
    if (g->type == GEV_INPUT_SOURCE) { extern void cx_ime_source_changed(int src); cx_ime_source_changed(g->key); return; }
    id w = find_window((int)g->win);
    if (g->type >= GEV_DRAG_ENTER && g->type <= GEV_DRAG_END) {
        /* arrastar e soltar (appkit_input.c); o fim de um arrasto chega mesmo se a janela da fonte já fechou */
        extern void cx_drag_event(id win, const gui_event_t *g);
        cx_drag_event(w, g);
        return;
    }
    if (!w) return;
    wstate_t *s = ws(w);
    g_mods = mac_mods(g->mods);
    { extern void cx_gc_mouse_event(int type, int x, int y, int buttons, int scroll);   /* GCMouse */
      if (g->type >= GEV_MOUSE_DOWN && g->type <= GEV_SCROLL) cx_gc_mouse_event((int)g->type, g->x, g->y, g->buttons, g->w); }
    switch (g->type) {
    case GEV_CLOSE:
        close_window(w, YES);
        break;
    case GEV_RESIZE: {
        s->frame.s.w = g->w;
        s->frame.s.h = g->h;
        v_set_frame(s->content, 0, r_make(0, 0, g->w, g->h));
        if (s->delegate && cx_responds(s->delegate->isa, sel_registerName("windowDidResize:"))) MSG(void, s->delegate, "windowDidResize:", cx_note(NSWindowDidResizeNotification, w, 0));
        s->needs = YES;
        break;
    }
    case GEV_MOUSE_DOWN: case GEV_RIGHT_DOWN: case GEV_SCROLL: {
        absr_t a = {0, 0, s->frame.s.w, s->frame.s.h, is_flipped(s->content)};
        id v = hit(s->content, &a, g->x, g->y);
        if (!v) v = s->content;
        if (g->type == GEV_MOUSE_DOWN) {
            if (MSG0(BOOL, v, "acceptsFirstResponder")) w_make_first(w, 0, v);
            else if (s->first && vs(s->first)->kind == K_TEXT) w_make_first(w, 0, 0);
            s->mouse_view = v;
            MSG(void, v, "mouseDown:", make_event(1, w, g->x, g->y, g));
        } else if (g->type == GEV_RIGHT_DOWN) MSG(void, v, "rightMouseDown:", make_event(3, w, g->x, g->y, g));
        else MSG(void, v, "scrollWheel:", make_event(22, w, g->x, g->y, g));
        break;
    }
    case GEV_MOUSE_UP:
        if (s->mouse_view) { MSG(void, s->mouse_view, "mouseUp:", make_event(2, w, g->x, g->y, g)); s->mouse_view = 0; }
        break;
    case GEV_MOUSE_MOVE:
        if (g->buttons && s->mouse_view) MSG(void, s->mouse_view, "mouseDragged:", make_event(6, w, g->x, g->y, g));
        else {
            /* sem botão: hover (o Chromium realça botões, mostra dicas e muda o cursor com estes eventos) */
            absr_t a = {0, 0, s->frame.s.w, s->frame.s.h, is_flipped(s->content)};
            id v = hit(s->content, &a, g->x, g->y);
            if (!v) v = s->content;
            id prev = cx_hover_view(w);
            if (prev != v) {
                if (prev) MSG(void, prev, "mouseExited:", make_event(9, w, g->x, g->y, g));
                MSG(void, v, "mouseEntered:", make_event(8, w, g->x, g->y, g));
                cx_set_hover_view(w, v);
            }
            MSG(void, v, "mouseMoved:", make_event(5, w, g->x, g->y, g));
        }
        break;
    case GEV_KEY_UP: {
        extern void cx_gc_keyboard_event(u16 mac_keycode, u64 mods, int down);
        id e = make_event(11, w, 0, 0, g);
        cx_gc_keyboard_event(mac_keycode(g->key), mac_mods(g->mods), 0);
        id target = s->first ? s->first : s->content;
        if (target) MSG(void, target, "keyUp:", e);
        break;
    }
    case GEV_KEY_DOWN: {
        id e = make_event(10, w, 0, 0, g);
        { extern void cx_gc_keyboard_event(u16 mac_keycode, u64 mods, int down); cx_gc_keyboard_event(mac_keycode(g->key), mac_mods(g->mods), 1); }
        if (menu_shortcut(g)) break;
        if ((g->mods & 8) && (g->key == 'q' || g->key == 'Q')) { MSG(void, NSApp, "terminate:", (id)0); break; }
        if ((g->mods & 8) && (g->key == 'w' || g->key == 'W')) { w_perform_close(w, 0, 0); break; }
        /* Enter aciona o botão padrão */
        /* (um campo de texto com ação própria trata o Return; sem ação, vale o botão padrão, como no Cocoa) */
        if (g->key == 10 && !(s->first && ((vs(s->first)->kind == K_TEXT && vs(s->first)->action) || vs(s->first)->kind == K_TV))) {
            id stackv[64]; int sp = 0; stackv[sp++] = s->content;
            while (sp) {
                id v = stackv[--sp];
                vstate_t *vv = vs(v);
                if (vv->kind == K_BUTTON && vv->key_equiv && !strcmp(cx_cstr(vv->key_equiv), "\r") && !vv->hidden) { send_action(v); goto keydone; }
                for (u32 i = 0; i < vv->nsub && sp < 64; i++) stackv[sp++] = vv->sub[i];
            }
        }
        {
            id target = s->first ? s->first : s->content;
            MSG(void, target, "keyDown:", e);
        }
    keydone:
        break;
    }
    }
}
static int visible_windows(void) {
    nsarr_t *a = (nsarr_t *)app_windows;
    int n = 0;
    for (u64 i = 0; a && i < a->n; i++) if (ws(a->items[i])->visible) n++;
    return n;
}
/* uma volta do laço: eventos, timers, pintura. Devolve o tempo de espera sugerido (ms). */
static void sync_menus(void);
static void bind_refresh(void);
static int pump(void) {
    void *pool = objc_autoreleasePoolPush();
    gui_event_t g;
    int got = 0;
    sync_menus();
    bind_refresh();
    while (SYS1(GUI_POLL, &g) == 1) { dispatch(&g); got++; }
    double next = cx_run_timers();
    cx_ca_poll();                              /* CALayerHost de outro processo: geração nova pede redesenho */
    /* teto de 25 quadros/s por janela: o compositor do Chromium (e cada IOSurfaceUnlock de escrita, quartzcore.c)
     * marca a janela a cada quadro, e o render_window (autolayout, composição inteira em software, flush, present)
     * roda na thread principal — a mesma do JS do Electron. Sem teto, a thread principal do VS Code vivia
     * redesenhando e a navegação da janela não andava. Cedo demais: s->needs fica ligado e o próximo pump desenha. */
    double now = cx_now();
    nsarr_t *a = (nsarr_t *)app_windows;
    for (u64 i = 0; a && i < a->n; i++) {
        id w = a->items[i];
        wstate_t *s = ws(w);
        if (!s->visible || !(s->needs || tree_needs(s->content))) continue;
        if (now - s->last_render < 0.04) continue;
        s->last_render = now;
        render_window(w);
    }
    objc_autoreleasePoolPop(pool);
    if (got) return 0;
    int ms = next < 0 ? 16 : (int)(next * 1000);
    return ms > 16 ? 16 : ms < 1 ? 1 : ms;
}
/* -[NSRunLoop run/runUntilDate:/runMode:beforeDate:] chegam aqui (foundation.c, cx_runloop_hook) de QUALQUER thread.
 * Só a thread principal bombeia o AppKit (eventos de janela, timers do Foundation, desenho). Uma thread de fundo roda o
 * CFRunLoop dela: é o que o MessagePumpNSRunLoop do Chromium espera (a fonte de trabalho dele está nesse run loop).
 * Antes, cinco threads do processo principal do VS Code (e uma do helper de rede) bombeavam o pump em paralelo com a
 * principal — ~58 s de CPU em 4 min, desenho da janela fora da thread principal e eventos de entrada roubados. */
int pthread_main_np(void);
int CFRunLoopRunInMode(void *mode, double seconds, unsigned char return_after_source);
static void run_loop_until(double until) {
    if (!pthread_main_np()) {
        do {
            double left = until - cx_now();
            if (left <= 0) return;
            int r = CFRunLoopRunInMode(0, left, 1);          /* uma passada: volta depois de uma fonte ou no prazo */
            if (r == 2) return;                              /* CFRunLoopStop */
            if (r == 1) {                                    /* sem fontes nem timers: espera sem girar */
                double l = until - cx_now();
                if (l > 0) SYS1(SYS_SLEEP, l > 0.05 ? 50 : (int)(l * 1000) + 1);
            }
        } while (until >= 1e299);                            /* -[NSRunLoop run]: para sempre, como antes */
        return;
    }
    while (cx_now() < until) { int ms = pump(); if (ms) SYS1(SYS_SLEEP, ms); }
}
static void ap_run(id self, SEL c) {
    ap_finish_launching(self, c);
    app.running = YES;
    BOOL had = NO;
    while (app.running) {
        int ms = pump();
        int vis = visible_windows();
        if (vis) had = YES;
        if (had && !vis && !app.in_modal) {
            id d = app.delegate;
            SEL s = sel_registerName("applicationShouldTerminateAfterLastWindowClosed:");
            /* no mOS um app sem janelas fica inacessível: encerra, salvo se o delegate disser NÃO */
            if (!d || !cx_responds(d->isa, s) || MSG(BOOL, d, "applicationShouldTerminateAfterLastWindowClosed:", self)) MSG(void, self, "terminate:", (id)0);
            had = NO;
        }
        if (!had && !vis && !app.delegate && !cx_timers && app.launched) {
            cx_puts_fd(2, "AppKit[mOS]: o app não abriu janelas (interfaces em .nib/.storyboard ainda não são suportadas)\n");
            exit(1);
        }
        if (ms) SYS1(SYS_SLEEP, ms);
    }
}
static void ap_activate(id self, SEL c, BOOL b) { (void)self; (void)c; (void)b; }
static BOOL ap_send_action(id self, SEL c, SEL a, id to, id from) {
    (void)self; (void)c;
    if (!to) to = app.delegate;
    if (to && cx_responds(to->isa, a)) { ((void (*)(id, SEL, id))objc_msgSend)(to, a, from); return YES; }
    return NO;
}
static i64 ap_run_modal(id self, SEL c, id w) {
    (void)self; (void)c;
    w_show(w, 0, 0);
    BOOL old = app.in_modal;
    app.in_modal = YES;
    app.modal_result = -1000;
    while (app.modal_result == -1000 && ws(w)->visible) { int ms = pump(); if (ms) SYS1(SYS_SLEEP, ms); }
    app.in_modal = old;
    if (ws(w)->visible) close_window(w, NO);
    return app.modal_result == -1000 ? -1001 : app.modal_result;
}
static void ap_stop_modal_code(id self, SEL c, i64 code) { (void)self; (void)c; app.modal_result = (int)code; }
static void ap_stop_modal(id self, SEL c) { (void)self; (void)c; app.modal_result = -1000 + 1; }

static void load_main_interface(void);
int NSApplicationMain(int argc, const char **argv) {
    (void)argc; (void)argv;
    MSG0(id, &NSApplication_c, "sharedApplication");
    load_main_interface();
    MSG0(void, NSApp, "run");
    return 0;
}

/* ================================================================ NSAlert */
typedef struct { id message, info, buttons; i64 style; } alertst_t;
static alertst_t *al(id a) { if (!ST(a)) { alertst_t *s = calloc(1, sizeof *s); s->buttons = cx_arr_new(&NSMutableArray_c); s->message = cx_str("Alerta"); ST(a) = s; } return ST(a); }
static void al_set_msg(id self, SEL c, id m) { (void)c; keep(&al(self)->message, m); }
static void al_set_info(id self, SEL c, id m) { (void)c; keep(&al(self)->info, m); }
static id al_add_button(id self, SEL c, id t) { (void)c; id b = cx_str(cx_cstr(t)); cx_arr_add(al(self)->buttons, b); return b; }
static void al_set_style(id self, SEL c, i64 s) { (void)c; al(self)->style = s; }
static void al_button_hit(id self, SEL c, id sender) { (void)self; (void)c; app.modal_result = 1000 + vs(sender)->tag; }
static cx_class_t AlertTarget_c, AlertTarget_m;
static i64 al_run(id self, SEL c) {
    (void)c;
    alertst_t *s = al(self);
    if (!((nsarr_t *)s->buttons)->n) cx_arr_add(s->buttons, cx_str("OK"));
    double W = 420;
    NSSize ms = text_size(cx_cstr(s->message), 13, 3);
    double infoh = 0;
    if (s->info) { NSSize is = text_size(cx_cstr(s->info), 12, 0); infoh = (is.w / (W - 40) + 1) * (is.h + 2); }
    double H = 70 + ms.h + infoh + 40;
    id w = w_init(MSG0(id, &NSWindow_c, "alloc"), 0, r_make(0, 0, W, H), 1, 2, NO);
    w_set_title(w, 0, cx_str(""));
    id cv = ws(w)->content;
    id msg = tf_label((id)&NSTextField_c, 0, s->message);
    vs(msg)->font = ft_make(13, 3, 0);
    v_set_frame(msg, 0, r_make(20, H - 20 - ms.h - 4, W - 40, ms.h + 4));
    v_add_sub(cv, 0, msg);
    if (s->info) {
        id inf = tf_label((id)&NSTextField_c, 0, s->info);
        vs(inf)->font = ft_make(12, 0, 0);
        v_set_frame(inf, 0, r_make(20, 56, W - 40, infoh));
        v_add_sub(cv, 0, inf);
    }
    id tgt = cx_alloc_instance(&AlertTarget_c);
    nsarr_t *bs = (nsarr_t *)s->buttons;
    double x = W - 20;
    for (u64 i = 0; i < bs->n; i++) {
        id b = bt_with_title((id)&NSButton_c, 0, bs->items[i], tgt, sel_registerName("cxAlertButton:"));
        vs(b)->tag = (int)i;
        if (i == 0) vs(b)->key_equiv = cx_str("\r");
        double bw = vs(b)->frame.s.w;
        x -= bw;
        v_set_frame(b, 0, r_make(x, 16, bw, 24));
        x -= 10;
        v_add_sub(cv, 0, b);
    }
    return ap_run_modal(NSApp, 0, w);
}

/* ================================================================ NSMenu (aceito, sem efeito visual) */
typedef struct { id title, items, key, submenu; SEL action; id target; i64 tag; int state; BOOL sep, disabled; u64 mask;
                 BOOL hidden, alternate, header; id image, represented; } menust_t;
static menust_t *mn(id m) { if (!ST(m)) { menust_t *s = calloc(1, sizeof *s); s->items = cx_arr_new(&NSMutableArray_c); ST(m) = s; } return ST(m); }
static id mn_init_title(id self, SEL c, id t) { (void)c; keep(&mn(self)->title, t); menus_dirty = YES; return self; }
static void mn_add(id self, SEL c, id item) { (void)c; cx_arr_add(mn(self)->items, item); menus_dirty = YES; }
static id mn_items(id self, SEL c) { (void)c; return mn(self)->items; }
static id mn_add_titled(id self, SEL c, id t, SEL a, id k) {
    id it = MSG0(id, MSG0(id, &NSMenuItem_c, "alloc"), "init");
    menust_t *s = mn(it);
    keep(&s->title, t); s->action = a; keep(&s->key, k);
    mn_add(self, c, it);
    return it;
}
static id mi_init(id self, SEL c, id t, SEL a, id k) { (void)c; menust_t *s = mn(self); keep(&s->title, t); s->action = a; keep(&s->key, k); menus_dirty = YES; return self; }
static id mi_sep(id cls, SEL c) { (void)c; id it = MSG0(id, MSG0(id, cls, "alloc"), "init"); mn(it)->sep = YES; return it; }
static void mi_set_enabled(id self, SEL c, BOOL b) { (void)c; mn(self)->disabled = !b; menus_dirty = YES; }
static void mi_set_state(id self, SEL c, i64 st) { (void)c; mn(self)->state = (int)st; menus_dirty = YES; }
static void mi_set_mask(id self, SEL c, u64 m) { (void)c; mn(self)->mask = m; menus_dirty = YES; }
static void mi_set_action(id self, SEL c, SEL a) { (void)c; mn(self)->action = a; }
static void mi_set_title(id self, SEL c, id t) { (void)c; keep(&mn(self)->title, t); menus_dirty = YES; }
static void mi_set_key(id self, SEL c, id k) { (void)c; keep(&mn(self)->key, k); menus_dirty = YES; }
static void mi_set_tag(id self, SEL c, i64 t) { (void)c; mn(self)->tag = t; }
static i64 mi_tag(id self, SEL c) { (void)c; return mn(self)->tag; }
static i64 mi_state(id self, SEL c) { (void)c; return mn(self)->state; }
static void mi_set_submenu(id self, SEL c, id m) { (void)c; keep(&mn(self)->submenu, m); menus_dirty = YES; }
static id mi_submenu(id self, SEL c) { (void)c; return mn(self)->submenu; }
static void mi_set_target(id self, SEL c, id t) { (void)c; mn(self)->target = t; }
static id mi_title(id self, SEL c) { (void)c; return mn(self)->title; }
/* edição do menu por índice/tag (o Chrome reconstrói o menu do app: remove itens do topo até o separador,
 * -[NSMenu removeItemAtIndex:]/-itemAtIndex: + -[NSMenuItem isSeparatorItem]; sem isso o laço nunca termina) */
static i64 mn_count(id self, SEL c) { (void)c; return (i64)MSG0(u64, mn(self)->items, "count"); }
static id mn_item_at(id self, SEL c, i64 i) { return i >= 0 && i < mn_count(self, c) ? MSG(id, mn(self)->items, "objectAtIndex:", (u64)i) : 0; }
static void mn_remove_at(id self, SEL c, i64 i) { if (i >= 0 && i < mn_count(self, c)) { MSG(void, mn(self)->items, "removeObjectAtIndex:", (u64)i); menus_dirty = YES; } }
static i64 mn_index_of(id self, SEL c, id item) {
    i64 n = mn_count(self, c);
    for (i64 i = 0; i < n; i++) if (mn_item_at(self, c, i) == item) return i;
    return -1;
}
static void mn_remove(id self, SEL c, id item) { mn_remove_at(self, c, mn_index_of(self, c, item)); }
static void mn_insert_at(id self, SEL c, id item, i64 i) {
    i64 n = mn_count(self, c);
    if (!item) return;
    if (i < 0 || i >= n) { mn_add(self, c, item); return; }
    MSG(void, mn(self)->items, "insertObject:atIndex:", item, (u64)i);
    menus_dirty = YES;
}
static void mn_remove_all(id self, SEL c) { (void)c; MSG0(void, mn(self)->items, "removeAllObjects"); menus_dirty = YES; }
static id mn_item_tag(id self, SEL c, i64 tag) {
    i64 n = mn_count(self, c);
    for (i64 i = 0; i < n; i++) { id it = mn_item_at(self, c, i); if (it && mn(it)->tag == tag) return it; }
    return 0;
}
static i64 mn_index_tag(id self, SEL c, i64 tag) { id it = mn_item_tag(self, c, tag); return it ? mn_index_of(self, c, it) : -1; }
static id mn_item_title(id self, SEL c, id t) {
    i64 n = mn_count(self, c);
    const char *s = t ? cx_cstr(t) : 0;
    for (i64 i = 0; s && i < n; i++) { id it = mn_item_at(self, c, i); if (it && mn(it)->title && !strcmp(cx_cstr(mn(it)->title), s)) return it; }
    return 0;
}
static id mn_title_get(id self, SEL c) { (void)c; return mn(self)->title; }
static BOOL mi_is_sep(id self, SEL c) { (void)c; return mn(self)->sep; }
static void mi_set_hidden(id self, SEL c, BOOL b) { (void)c; mn(self)->hidden = b; menus_dirty = YES; }
static BOOL mi_hidden(id self, SEL c) { (void)c; return mn(self)->hidden; }
static void mi_set_alternate(id self, SEL c, BOOL b) { (void)c; mn(self)->alternate = b; }
static BOOL mi_alternate(id self, SEL c) { (void)c; return mn(self)->alternate; }
static void mi_set_image(id self, SEL c, id img) { (void)c; keep(&mn(self)->image, img); }
static id mi_image(id self, SEL c) { (void)c; return mn(self)->image; }
static void mi_set_repr(id self, SEL c, id o) { (void)c; keep(&mn(self)->represented, o); }
static id mi_repr(id self, SEL c) { (void)c; return mn(self)->represented; }
static BOOL mi_is_header(id self, SEL c) { (void)c; return mn(self)->header; }
static BOOL mi_enabled(id self, SEL c) { (void)c; return !mn(self)->disabled; }
static id mi_key(id self, SEL c) { (void)c; return mn(self)->key; }
static SEL mi_action(id self, SEL c) { (void)c; return mn(self)->action; }
static id mi_target(id self, SEL c) { (void)c; return mn(self)->target; }
static BOOL mi_has_submenu(id self, SEL c) { (void)c; return mn(self)->submenu != 0; }
static id mi_section_header(id cls, SEL c, id title) {          /* +[NSMenuItem sectionHeaderWithTitle:] (macOS 14) */
    (void)c;
    id it = MSG0(id, MSG0(id, cls, "alloc"), "init");
    keep(&mn(it)->title, title);
    mn(it)->header = YES;
    mn(it)->disabled = YES;
    return it;
}

/* ================================================================ menus -> barra do mOS */
static id *menu_tab;
static u32 nmenu_tab, capmenu_tab;
static BOOL menus_sent;

static u32 menu_id(id item) {
    for (u32 i = 0; i < nmenu_tab; i++) if (menu_tab[i] == item) return i + 1;
    if (nmenu_tab == capmenu_tab) { capmenu_tab = capmenu_tab ? capmenu_tab * 2 : 64; menu_tab = realloc(menu_tab, capmenu_tab * sizeof(id)); }
    menu_tab[nmenu_tab++] = objc_retain(item);
    return nmenu_tab;
}
static void mb_put(u8 **b, u32 *n, u32 *cap, const void *d, u32 len) {
    if (*n + len > *cap) { *cap = (*n + len) * 2 + 256; *b = realloc(*b, *cap); }
    memcpy(*b + *n, d, len);
    *n += len;
}
static void mb_str(u8 **b, u32 *n, u32 *cap, const char *s) { u32 l = (u32)strlen(s); if (l > 200) l = 200; mb_put(b, n, cap, &l, 4); mb_put(b, n, cap, s, l); }
static void shortcut_text(id key, u64 mask, char *out, usize cap) {
    out[0] = 0;
    const char *k = key ? cx_cstr(key) : "";
    if (!k[0]) return;
    BOOL upper = k[0] >= 'A' && k[0] <= 'Z';
    snprintf(out, cap, "%s%s%s\xE2\x8C\x98%c", (mask & (1u << 19)) ? "\xE2\x8C\xA5" : "", (upper || (mask & (1u << 17))) ? "\xE2\x87\xA7" : "",
             (mask & (1u << 18)) ? "\xE2\x8C\x83" : "", (char)toupper((u8)k[0]));
}
static void sync_menus(void) {
    if (!app.menu || (menus_sent && !menus_dirty)) return;
    menus_dirty = NO;
    menus_sent = YES;
    nsarr_t *top = (nsarr_t *)mn(app.menu)->items;
    u8 *b = 0;
    u32 n = 0, cap = 0, ntop = 0;
    mb_put(&b, &n, &cap, &ntop, 4);
    for (u64 i = 1; i < top->n; i++) {             /* o 1º menu (o do app) já é desenhado pelo mOS */
        menust_t *ti = mn(top->items[i]);
        id sub = ti->submenu;
        if (!sub) continue;
        menust_t *sm = mn(sub);
        mb_str(&b, &n, &cap, cx_cstr(sm->title ? sm->title : ti->title ? ti->title : cx_str("Menu")));
        nsarr_t *its = (nsarr_t *)sm->items;
        u32 cnt = (u32)its->n;
        mb_put(&b, &n, &cap, &cnt, 4);
        for (u64 k = 0; k < its->n; k++) {
            menust_t *m = mn(its->items[k]);
            u32 act = m->sep ? 0 : menu_id(its->items[k]), fl = (m->disabled ? 1 : 0) | (m->state ? 2 : 0);
            char sc[32];
            shortcut_text(m->key, m->mask ? m->mask : (1u << 20), sc, sizeof sc);
            mb_put(&b, &n, &cap, &act, 4);
            mb_put(&b, &n, &cap, &fl, 4);
            mb_str(&b, &n, &cap, m->title ? cx_cstr(m->title) : "");
            mb_str(&b, &n, &cap, sc);
        }
        ntop++;
    }
    memcpy(b, &ntop, 4);
    SYS2(GUI_SET_MENU, b, n);
    free(b);
}
static id key_window_obj(void);
static BOOL perform_item(id item) {
    menust_t *m = mn(item);
    if (!m->action) return NO;
    id t = m->target;
    if (!t) {
        id w = key_window_obj();
        id cands[8];
        int nc = 0;
        if (w) {
            for (id r = ws(w)->first; r && nc < 4; r = cx_is_subclass(r->isa, &NSView_c) ? vs(r)->superview : 0) cands[nc++] = r;
            cands[nc++] = w;
            if (ws(w)->delegate) cands[nc++] = ws(w)->delegate;
        }
        cands[nc++] = NSApp;
        if (app.delegate) cands[nc++] = app.delegate;
        for (int i = 0; i < nc && !t; i++) if (cands[i] && cx_responds(cands[i]->isa, m->action)) t = cands[i];
    }
    if (!t) return NO;
    ((void (*)(id, SEL, id))objc_msgSend)(t, m->action, item);
    return YES;
}
static id find_item(id menu, BOOL (*pred)(menust_t *, void *), void *arg) {
    if (!menu) return 0;
    nsarr_t *its = (nsarr_t *)mn(menu)->items;
    for (u64 i = 0; i < its->n; i++) {
        menust_t *m = mn(its->items[i]);
        if (pred(m, arg)) return its->items[i];
        id r = find_item(m->submenu, pred, arg);
        if (r) return r;
    }
    return 0;
}
static BOOL pred_action(menust_t *m, void *a) { return m->action == (SEL)a; }
static BOOL pred_key(menust_t *m, void *a) {
    const char *k = m->key ? cx_cstr(m->key) : "";
    const gui_event_t *g = a;
    if (!k[0] || m->sep) return NO;
    int want = g->key;
    if (want >= 'A' && want <= 'Z') want += 32;
    BOOL need_shift = (k[0] >= 'A' && k[0] <= 'Z') || (m->mask & (1u << 17));
    return tolower((u8)k[0]) == want && need_shift == !!(g->mods & 1);
}
static BOOL pred_comma(menust_t *m, void *a) { (void)a; return m->key && !strcmp(cx_cstr(m->key), ","); }
static void standard_about(void) {
    id a = MSG0(id, MSG0(id, &NSAlert_c, "alloc"), "init");
    extern const char *__progname;
    MSG(void, a, "setMessageText:", cx_str(__progname));
    MSG(void, a, "setInformativeText:", cx_str("Versão 1.0\nApp do macOS rodando no mOS."));
    MSG0(i64, a, "runModal");
}
static void perform_menu(int k) {
    if (k == GUI_MENU_ABOUT) {
        id it = app.menu ? find_item(app.menu, pred_action, (void *)sel_registerName("orderFrontStandardAboutPanel:")) : 0;
        if (it && mn(it)->target) perform_item(it);
        else if (app.delegate && cx_responds(app.delegate->isa, sel_registerName("orderFrontStandardAboutPanel:"))) MSG(void, app.delegate, "orderFrontStandardAboutPanel:", (id)0);
        else standard_about();
        return;
    }
    if (k == GUI_MENU_PREFS) { id it = find_item(app.menu, pred_comma, 0); if (it) perform_item(it); return; }
    if (k == GUI_MENU_HIDE) return;
    if (k >= 1 && (u32)k <= nmenu_tab) perform_item(menu_tab[k - 1]);
}
static BOOL menu_shortcut(gui_event_t *g) {
    if (!app.menu || !(g->mods & 8)) return NO;
    id it = find_item(app.menu, pred_key, g);
    return it && !mn(it)->disabled && perform_item(it);
}
static void ap_about_panel(id self, SEL c, id s) { (void)self; (void)c; (void)s; standard_about(); }

/* ================================================================ NSData e NSImage */
typedef struct { cx_class_t *isa; u8 *bytes; u64 len; } nsdata_t;
static id dt_with_bytes(id cls, SEL c, const void *b, u64 n) { (void)c; nsdata_t *d = (nsdata_t *)cx_alloc_instance((cx_class_t *)cls); d->bytes = malloc(n ? n : 1); memcpy(d->bytes, b, n); d->len = n; return (id)d; }
static u8 *read_whole(const char *path, u64 *n) {
    FILE_ *f = fopen(path, "r");
    if (!f) return 0;
    u8 *buf = 0;
    u64 cap = 0, len = 0, k;
    u8 tmp[8192];
    while ((k = fread(tmp, 1, sizeof tmp, f)) > 0) {
        if (len + k > cap) { cap = (len + k) * 2; buf = realloc(buf, cap); }
        memcpy(buf + len, tmp, k);
        len += k;
    }
    fclose(f);
    *n = len;
    return buf ? buf : malloc(1);
}
static id dt_with_file(id cls, SEL c, id path) { (void)c; u64 n; u8 *b = read_whole(cx_cstr(path), &n); if (!b) return 0; nsdata_t *d = (nsdata_t *)cx_alloc_instance((cx_class_t *)cls); d->bytes = b; d->len = n; return (id)d; }
static id dt_init_file(id self, SEL c, id path) { (void)c; u64 n; u8 *b = read_whole(cx_cstr(path), &n); if (!b) return 0; ((nsdata_t *)self)->bytes = b; ((nsdata_t *)self)->len = n; return self; }
static id dt_init_bytes(id self, SEL c, const void *b, u64 n) { (void)c; nsdata_t *d = (nsdata_t *)self; d->bytes = malloc(n ? n : 1); memcpy(d->bytes, b, n); d->len = n; return self; }
static const void *dt_bytes(id self, SEL c) { (void)c; return ((nsdata_t *)self)->bytes; }
static u64 dt_length(id self, SEL c) { (void)c; return ((nsdata_t *)self)->len; }
static BOOL dt_write(id self, SEL c, id path, BOOL at) { (void)c; (void)at; FILE_ *f = fopen(cx_cstr(path), "w"); if (!f) return NO; fwrite(((nsdata_t *)self)->bytes, 1, ((nsdata_t *)self)->len, f); fclose(f); return YES; }

typedef struct { int w, h; u32 *px; double dw, dh; } imgst_t;
static imgst_t *im(id o) { if (!ST(o)) ST(o) = calloc(1, sizeof(imgst_t)); return ST(o); }
static BOOL decode_into(id img, const u8 *data, u64 n) {
    long need = SYS4(GUI_DECODE_IMAGE, data, n, 0, 0);
    if (need <= 8) return NO;
    u8 *out = malloc((size_t)need);
    if (!out || SYS4(GUI_DECODE_IMAGE, data, n, out, need) != need) { free(out); return NO; }
    imgst_t *s = im(img);
    s->w = (int)((u32 *)out)[0];
    s->h = (int)((u32 *)out)[1];
    s->px = malloc((size_t)s->w * s->h * 4);
    memcpy(s->px, out + 8, (size_t)s->w * s->h * 4);
    s->dw = s->w; s->dh = s->h;
    free(out);
    return YES;
}
static id im_init_file(id self, SEL c, id path) { (void)c; u64 n; u8 *b = read_whole(cx_cstr(path), &n); BOOL ok = b && decode_into(self, b, n); free(b); return ok ? self : 0; }
static id im_init_data(id self, SEL c, id data) { (void)c; nsdata_t *d = (nsdata_t *)data; return d && decode_into(self, d->bytes, d->len) ? self : 0; }
static id im_named(id cls, SEL c, id name) {
    (void)c;
    id bundle = MSG0(id, cx_class_named("NSBundle"), "mainBundle");
    id res = MSG0(id, bundle, "resourcePath");
    static const char *exts[] = {"", ".png", ".jpg", ".jpeg", ".gif", ".bmp"};
    for (u32 i = 0; i < ARRAYN(exts); i++) {
        char p[1200];
        snprintf(p, sizeof p, "%s/%s%s", cx_cstr(res), cx_cstr(name), exts[i]);
        if (access(p, 0) == 0) { id o = im_init_file(MSG0(id, cls, "alloc"), 0, cx_str(p)); if (o) return o; }
    }
    return 0;
}
static id im_init_size(id self, SEL c, NSSize z) { (void)c; imgst_t *s = im(self); s->dw = z.w; s->dh = z.h; return self; }
static NSSize im_size(id self, SEL c) { (void)c; imgst_t *s = im(self); NSSize z = {s->dw, s->dh}; return z; }
static void im_set_size(id self, SEL c, NSSize z) { (void)c; im(self)->dw = z.w; im(self)->dh = z.h; }
static BOOL im_valid(id self, SEL c) { (void)c; return im(self)->px != 0; }
/* desenha a imagem escalada para um retângulo em pixels */
static void blit_image(imgst_t *s, double x, double y, double w, double h, double alpha) {
    if (!s->px || w < 1 || h < 1) return;
    int dw = (int)(w + 0.5), dh = (int)(h + 0.5);
    if (dw > 2048) dw = 2048;
    if (dh > 2048) dh = 2048;
    u32 *buf = malloc((size_t)dw * dh * 4 + 16);
    if (!buf) return;
    u32 a8 = (u32)(alpha * 255 + 0.5);
    for (int j = 0; j < dh; j++) {
        int sy = (int)((j + 0.5) * s->h / dh);
        for (int i = 0; i < dw; i++) {
            int sx = (int)((i + 0.5) * s->w / dw);
            u32 p = s->px[sy * s->w + sx];
            if (a8 < 255) p = (((p >> 24) * a8 / 255) << 24) | (p & 0xFFFFFF);
            buf[j * dw + i] = p;
        }
    }
    u32 hdrsz = 16, bytes = (u32)dw * dh * 4;
    u8 *b = malloc(hdrsz + bytes);
    i32 r[4] = {(i32)x, (i32)y, dw, dh};
    memcpy(b, r, 16);
    memcpy(b + 16, buf, bytes);
    put(GC_IMAGE, b, hdrsz + bytes);
    free(b);
    free(buf);
}
static void im_draw_in(id self, SEL c, NSRect r) { (void)c; double x, y, w, h; px_rect(r, &x, &y, &w, &h); blit_image(im(self), x, y, w, h, 1); }
static void im_draw_in_frac(id self, SEL c, NSRect r, NSRect from, u64 op, double f) { (void)c; (void)from; (void)op; double x, y, w, h; px_rect(r, &x, &y, &w, &h); blit_image(im(self), x, y, w, h, f); }
static void im_draw_at(id self, SEL c, NSPoint p, NSRect from, u64 op, double f) { (void)from; imgst_t *s = im(self); NSRect r = {p, {s->dw, s->dh}}; im_draw_in_frac(self, c, r, r, op, f); }

static void iv_set_image(id self, SEL c, id img) { (void)c; keep(&vs(self)->image, img); mark(self); }
static id iv_image(id self, SEL c) { (void)c; return vs(self)->image; }
static id iv_with_image(id cls, SEL c, id img) { id v = MSG(id, MSG0(id, cls, "alloc"), "initWithFrame:", r_make(0, 0, img ? im(img)->dw : 32, img ? im(img)->dh : 32)); iv_set_image(v, c, img); return v; }
static void draw_image_fit(id img, double x, double y, double w, double h) {
    imgst_t *s = im(img);
    if (!s->px) return;
    double sc = 1;
    if (s->dw > w || s->dh > h) { double a = w / s->dw, b = h / s->dh; sc = a < b ? a : b; }
    double iw = s->dw * sc, ih = s->dh * sc;
    blit_image(s, x + (w - iw) / 2, y + (h - ih) / 2, iw, ih, 1);
}

/* ================================================================ NSScrollView */
static id sv_init_frame(id self, SEL c, NSRect f) { v_init_frame(self, c, f); vs(self)->kind = K_SCROLL; return self; }
static void sv_set_doc(id self, SEL c, id doc) {
    vstate_t *s = vs(self);
    if (s->doc) v_remove(s->doc, c);
    keep(&s->doc, doc);
    if (doc) v_add_sub(self, c, doc);
    s->scroll_y = 0;
}
static id sv_doc(id self, SEL c) { (void)c; return vs(self)->doc; }
static id sv_content(id self, SEL c) { (void)c; return self; }
static void layout_scroll(id v) {
    vstate_t *s = vs(v);
    if (!s->doc) return;
    vstate_t *d = vs(s->doc);
    if (d->kind == K_TABLE) table_resize(s->doc, s->frame.s.w);
    if (d->kind == K_TV) { d->frame.s.w = s->frame.s.w; tx_fit(s->doc, s->frame.s.w); }
    if (d->frame.s.w < s->frame.s.w || (d->autoresize & 2)) d->frame.s.w = s->frame.s.w;
    double maxs = d->frame.s.h - s->frame.s.h;
    if (maxs < 0) maxs = 0;
    if (s->scroll_y > maxs) s->scroll_y = maxs;
    if (s->scroll_y < 0) s->scroll_y = 0;
    d->frame.o.x = 0;
    d->frame.o.y = s->frame.s.h - d->frame.s.h + s->scroll_y;   /* topo do documento alinhado ao topo visível */
}
static void sv_scroll(id self, SEL c, id e) {
    (void)c;
    vstate_t *s = vs(self);
    s->scroll_y += -evs(e)->dy * 40;
    layout_scroll(self);
    mark(self);
}

/* ================================================================ NSTableView */
typedef struct { id ident, title; double width; } colst_t;
static colst_t *cs_(id col) { if (!ST(col)) { colst_t *s = calloc(1, sizeof *s); s->width = 120; ST(col) = s; } return ST(col); }
static id col_init(id self, SEL c, id ident) { (void)c; keep(&cs_(self)->ident, ident); keep(&cs_(self)->title, ident); return self; }
static id col_ident(id self, SEL c) { (void)c; return cs_(self)->ident; }
static void col_set_title(id self, SEL c, id t) { (void)c; keep(&cs_(self)->title, t); }
static id col_title(id self, SEL c) { (void)c; return cs_(self)->title; }
static void col_set_width(id self, SEL c, double w) { (void)c; cs_(self)->width = w; }
static double col_width(id self, SEL c) { (void)c; return cs_(self)->width; }

static id tv_init_frame(id self, SEL c, NSRect f) {
    v_init_frame(self, c, f);
    vstate_t *s = vs(self);
    s->kind = K_TABLE; s->row_h = 24; s->selected = -1; s->header = YES; s->alt_rows = NO;
    s->cols = objc_retain(cx_arr_new(&NSMutableArray_c));
    return self;
}
static void tv_add_col(id self, SEL c, id col) { (void)c; cx_arr_add(vs(self)->cols, col); mark(self); }
static id tv_cols(id self, SEL c) { (void)c; return vs(self)->cols; }
static void tv_set_ds(id self, SEL c, id d) { (void)c; vs(self)->data_source = d; mark(self); }
static id tv_ds(id self, SEL c) { (void)c; return vs(self)->data_source; }
static i64 tv_rows(id self) {
    vstate_t *s = vs(self);
    if (s->data_source && cx_responds(s->data_source->isa, sel_registerName("numberOfRowsInTableView:"))) return MSG(i64, s->data_source, "numberOfRowsInTableView:", self);
    return 0;
}
static i64 tv_num_rows(id self, SEL c) { (void)c; return tv_rows(self); }
static void table_resize(id t, double width) {
    vstate_t *s = vs(t);
    s->frame.s.w = width > s->frame.s.w ? width : s->frame.s.w;
    s->frame.s.h = (s->header ? 24 : 0) + s->row_h * (double)tv_rows(t);
}
static void tv_reload(id self, SEL c) {
    (void)c;
    vstate_t *s = vs(self);
    if (s->selected >= tv_rows(self)) s->selected = -1;
    if (s->superview && vs(s->superview)->kind == K_SCROLL) layout_scroll(s->superview);
    mark(s->superview ? s->superview : self);
}
static i64 tv_selected(id self, SEL c) { (void)c; return vs(self)->selected; }
static i64 tv_clicked(id self, SEL c) { (void)c; return vs(self)->clicked; }
static void tv_set_row_h(id self, SEL c, double h) { (void)c; vs(self)->row_h = h; mark(self); }
static void tv_set_header(id self, SEL c, id h) { (void)c; vs(self)->header = h != 0; mark(self); }
static id tv_header(id self, SEL c) { (void)c; return vs(self)->header ? self : 0; }
static void tv_set_alt(id self, SEL c, BOOL b) { (void)c; vs(self)->alt_rows = b; mark(self); }
static void tv_set_double(id self, SEL c, SEL a) { (void)c; vs(self)->double_action = a; }
static id tv_make_view(id self, SEL c, id ident, id owner) { (void)self; (void)c; (void)ident; (void)owner; return 0; }
static void tv_scroll_row(id self, SEL c, i64 r) { (void)self; (void)c; (void)r; }
typedef struct { cx_class_t *isa; i64 first; } nsindexset_t;
static id ixs_with_index(id cls, SEL c, u64 i) { (void)c; nsindexset_t *o = (nsindexset_t *)cx_alloc_instance((cx_class_t *)cls); o->first = (i64)i; return (id)o; }
static i64 ixs_first(id self, SEL c) { (void)c; return ((nsindexset_t *)self)->first; }
static void select_row(id t, i64 r, BOOL notify) {
    vstate_t *s = vs(t);
    if (s->selected == r) return;
    s->selected = r;
    mark(t);
    if (notify && s->delegate && cx_responds(s->delegate->isa, sel_registerName("tableViewSelectionDidChange:")))
        MSG(void, s->delegate, "tableViewSelectionDidChange:", objc_autorelease(cx_note(cx_str("NSTableViewSelectionDidChangeNotification"), t, 0)));
}
static void tv_select(id self, SEL c, id ixs, BOOL ext) { (void)c; (void)ext; select_row(self, ixs ? ((nsindexset_t *)ixs)->first : -1, NO); }
static void tv_deselect_all(id self, SEL c, id s) { (void)c; (void)s; select_row(self, -1, NO); }
/* texto da célula: cell-based (objectValue) ou view-based (textField do NSTableCellView) */
static id cell_text(id t, id col, i64 row) {
    vstate_t *s = vs(t);
    id ds = s->data_source, dl = s->delegate;
    if (ds && cx_responds(ds->isa, sel_registerName("tableView:objectValueForTableColumn:row:"))) {
        id v = MSG(id, ds, "tableView:objectValueForTableColumn:row:", t, col, row);
        return v ? MSG0(id, v, "description") : 0;
    }
    if (dl && cx_responds(dl->isa, sel_registerName("tableView:viewForTableColumn:row:"))) {
        id v = MSG(id, dl, "tableView:viewForTableColumn:row:", t, col, row);
        if (!v) return 0;
        if (cx_responds(v->isa, sel_registerName("textField"))) { id tf = MSG0(id, v, "textField"); if (tf) return MSG0(id, tf, "stringValue"); }
        if (cx_responds(v->isa, sel_registerName("stringValue"))) return MSG0(id, v, "stringValue");
    }
    return 0;
}
static void draw_table(id t, absr_t *a) {
    vstate_t *s = vs(t);
    nsarr_t *cols = (nsarr_t *)s->cols;
    i64 rows = tv_rows(t);
    double y = a->tly + (s->header ? 24 : 0), x0 = a->tlx;
    for (i64 r = 0; r < rows; r++) {
        double ry = y + r * s->row_h;
        if (ry + s->row_h < clip_y0 || ry > clip_y1) continue;   /* fora da área visível da rolagem */
        BOOL sel = r == s->selected;
        if (sel) g_rrect(x0 + 4, ry + 1, a->w - 8, s->row_h - 2, 5, accent);
        else if (s->alt_rows && (r & 1)) g_fill(x0, ry, a->w, s->row_h, 0x08000000);
        double x = x0;
        for (u64 k = 0; k < cols->n; k++) {
            colst_t *cs = cs_(cols->items[k]);
            id txt = cell_text(t, cols->items[k], r);
            if (txt) {
                const char *tt = cx_cstr(txt);
                gui_metrics_t m = measure(tt, (u32)strlen(tt), 13, 0);
                set_clip(x, ry, cs->width - 4, s->row_h);
                g_text(x + 8, ry + (s->row_h + m.ascent - m.descent) / 2, 13, 0, sel ? 0xFFFFFFFF : 0xE6000000, tt, (u32)strlen(tt));
                set_clip(a->tlx, a->tly, a->w, a->h);
            }
            x += cs->width;
        }
    }
    if (s->header) {                 /* cabeçalho fixo no topo da área visível (como no macOS) */
        double hy = a->tly;
        if (s->superview && vs(s->superview)->kind == K_SCROLL && clip_y0 > hy) hy = clip_y0;
        set_clip(a->tlx, a->tly, a->w, a->h);
        g_fill(x0, hy, a->w, 24, 0xFFF5F5F7);
        g_fill(x0, hy + 23, a->w, 1, 0x22000000);
        double x = x0;
        for (u64 k = 0; k < cols->n; k++) {
            colst_t *cs = cs_(cols->items[k]);
            const char *tt = cs->title ? cx_cstr(cs->title) : "";
            gui_metrics_t m = measure(tt, (u32)strlen(tt), 11, 2);
            g_text(x + 8, hy + (24 + m.ascent - m.descent) / 2, 11, 2, 0xB0000000, tt, (u32)strlen(tt));
            x += cs->width;
            if (k + 1 < cols->n) g_fill(x - 1, hy + 4, 1, 16, 0x1A000000);
        }
    }
}
static void tv_mouse_down(id self, SEL c, id e) {
    (void)c;
    vstate_t *s = vs(self);
    absr_t a;
    if (!abs_rect(self, &a)) return;
    absr_t sa;
    if (s->header && s->superview && vs(s->superview)->kind == K_SCROLL && abs_rect(s->superview, &sa) && win_h(s->window) - evs(e)->y - sa.tly < 24) return;   /* clique no cabeçalho fixo */
    double py = win_h(s->window) - evs(e)->y - a.tly - (s->header ? 24 : 0);
    i64 r = py < 0 ? -1 : (i64)(py / s->row_h);
    if (r >= tv_rows(self)) r = -1;
    s->clicked = r;
    if (r >= 0) select_row(self, r, YES);
    if (evs(e)->clicks >= 2 && s->double_action && r >= 0) {
        id t = s->target ? s->target : (s->delegate ? s->delegate : app.delegate);
        if (t) ((void (*)(id, SEL, id))objc_msgSend)(t, s->double_action, self);
    } else if (s->action && r >= 0) send_action(self);
}
/* NSTableCellView: guarda textField/imageView */
static void tcv_set_tf(id self, SEL c, id tf) { (void)c; vstate_t *s = vs(self); keep(&s->text_field, tf); if (tf) v_add_sub(self, c, tf); }
static id tcv_tf(id self, SEL c) { (void)c; return vs(self)->text_field; }

/* ================================================================ NSScreen, NSCursor, NSWorkspace, NSImage */
static id sc_main(id cls, SEL c) { (void)c; static id s; if (!s) s = cx_alloc_instance((cx_class_t *)cls); return s; }
static id sc_screens(id cls, SEL c) { id a = cx_arr_new(&NSMutableArray_c); cx_arr_add(a, sc_main(cls, c)); return a; }
static NSRect sc_frame(id self, SEL c) { (void)self; (void)c; gui_wininfo_t i = {1440, 876, 1, 1}; SYS1(GUI_SCREEN, &i); return r_make(0, 0, i.w, i.h + 24); }
static NSRect sc_visible(id self, SEL c) { (void)self; (void)c; gui_wininfo_t i = {1440, 876, 1, 1}; SYS1(GUI_SCREEN, &i); return r_make(0, 70, i.w, i.h - 70); }
static double sc_scale(id self, SEL c) { (void)self; (void)c; return 1.0; }
static id generic_singleton(id cls, SEL c) { (void)c; static struct { id c, o; } t[8]; for (int i = 0; i < 8; i++) { if (t[i].c == cls) return t[i].o; if (!t[i].c) { t[i].c = cls; t[i].o = cx_alloc_instance((cx_class_t *)cls); return t[i].o; } } return 0; }
static BOOL ws_open_url(id self, SEL c, id u) { (void)self; (void)c; (void)u; return NO; }
static id key_window_obj(void) { return ap_key_window(NSApp, 0); }

/* ================================================================ registro */
#define M(cls, s, f) cx_add_method(&cls##_c, s, (void *)(f))
#define CM(cls, s, f) cx_add_cmethod(&cls##_c, s, (void *)(f))
#define DEF(cls, sup) cx_define_class(&cls##_c, &cls##_m, #cls, &sup##_c, 8)

/* ================================================================ Auto Layout
 * NSLayoutConstraint/NSLayoutAnchor como no Cocoa, resolvidos por propagação: as restrições
 * obrigatórias (prioridade 1000) primeiro, depois as opcionais, depois o tamanho intrínseco e por
 * fim o frame atual. Desigualdades (>=, <=) são aplicadas como limites depois. Vale para views com
 * translatesAutoresizingMaskIntoConstraints = NO (as demais continuam no autoresizing). */
CLASS(NSLayoutAnchor) CLASS(NSLayoutXAxisAnchor) CLASS(NSLayoutYAxisAnchor) CLASS(NSLayoutDimension) CLASS(NSLayoutGuide)

enum { LA_NONE, LA_LEFT, LA_RIGHT, LA_TOP, LA_BOTTOM, LA_LEADING, LA_TRAILING, LA_WIDTH, LA_HEIGHT, LA_CX, LA_CY, LA_LASTBASE, LA_FIRSTBASE };
typedef struct { id a, b; int aa, ba, rel; double mult, k; float prio; BOOL active; id ident; } lc_t;
typedef struct { id view; int attr; } anc_t;
static id *lcs;                 /* restrições ativas (retidas) */
static u32 nlc, caplc;
static BOOL layout_dirty;

static lc_t *lc(id c) { if (!ST(c)) { lc_t *s = calloc(1, sizeof *s); s->mult = 1; s->prio = 1000; ST(c) = s; } return ST(c); }
static anc_t *an(id a) { if (!ST(a)) ST(a) = calloc(1, sizeof(anc_t)); return ST(a); }
static BOOL is_view(id o) { return o && cx_is_subclass(o->isa, &NSView_c); }
static void lc_touch(id c) {
    lc_t *s = lc(c);
    layout_dirty = YES;
    id v[2] = {s->a, s->b};
    for (int i = 0; i < 2; i++) if (is_view(v[i])) mark(vs(v[i])->superview ? vs(v[i])->superview : v[i]);
}
static void lc_set_active(id self, SEL c, BOOL on) {
    (void)c;
    lc_t *s = lc(self);
    if (on == s->active) return;
    if (on) {
        if (nlc == caplc) { caplc = caplc ? caplc * 2 : 32; lcs = realloc(lcs, caplc * sizeof(id)); }
        lcs[nlc++] = objc_retain(self);
        s->active = YES;
        lc_touch(self);
    } else {
        for (u32 i = 0; i < nlc; i++) if (lcs[i] == self) { lcs[i] = lcs[--nlc]; break; }
        s->active = NO;
        lc_touch(self);
        objc_release(self);
    }
}
static BOOL lc_is_active(id self, SEL c) { (void)c; return lc(self)->active; }
static double lc_const(id self, SEL c) { (void)c; return lc(self)->k; }
static void lc_set_const(id self, SEL c, double k) { (void)c; lc(self)->k = k; lc_touch(self); }
static float lc_prio(id self, SEL c) { (void)c; return lc(self)->prio; }
static void lc_set_prio(id self, SEL c, float p) { (void)c; lc(self)->prio = p; lc_touch(self); }
static double lc_mult(id self, SEL c) { (void)c; return lc(self)->mult; }
static id lc_first(id self, SEL c) { (void)c; return lc(self)->a; }
static id lc_second(id self, SEL c) { (void)c; return lc(self)->b; }
static i64 lc_first_attr(id self, SEL c) { (void)c; return lc(self)->aa; }
static i64 lc_second_attr(id self, SEL c) { (void)c; return lc(self)->ba; }
static i64 lc_relation(id self, SEL c) { (void)c; return lc(self)->rel; }
static void lc_set_ident(id self, SEL c, id i) { (void)c; keep(&lc(self)->ident, i); }
static id lc_ident(id self, SEL c) { (void)c; return lc(self)->ident; }
static void lc_dealloc(id self, SEL c) {
    lc_t *s = ST(self);
    if (s) { objc_release(s->a); objc_release(s->b); objc_release(s->ident); free(s); ST(self) = 0; }
    ((void (*)(id, SEL))cx_lookup(&NSObject_c, c))(self, c);
}
static id lc_make(id a, int aa, int rel, id b, int ba, double mult, double k) {
    id c = objc_autorelease(cx_alloc_instance(&NSLayoutConstraint_c));
    lc_t *s = lc(c);
    s->a = objc_retain(a); s->aa = aa; s->rel = rel; s->b = objc_retain(b); s->ba = b ? ba : LA_NONE; s->mult = mult; s->k = k;
    if (is_view(a) && !b && (aa == LA_WIDTH || aa == LA_HEIGHT)) {}
    return c;
}
static id lc_with_item(id cls, SEL c, id a, i64 aa, i64 rel, id b, i64 ba, double mult, double k) {
    (void)cls; (void)c;
    return lc_make(a, (int)aa, (int)rel, b, (int)ba, mult, k);
}
static void lc_activate_all(id cls, SEL c, id arr) {
    (void)cls; (void)c;
    nsarr_t *r = (nsarr_t *)arr;
    for (u64 i = 0; arr && i < r->n; i++) lc_set_active(r->items[i], 0, YES);
}
static void lc_deactivate_all(id cls, SEL c, id arr) {
    (void)cls; (void)c;
    nsarr_t *r = (nsarr_t *)arr;
    for (u64 i = r ? r->n : 0; i-- > 0;) lc_set_active(r->items[i], 0, NO);
}

/* ---- âncoras */
static id anchor_for(id v, int attr) {
    cx_class_t *k = attr == LA_WIDTH || attr == LA_HEIGHT ? &NSLayoutDimension_c
                  : attr == LA_TOP || attr == LA_BOTTOM || attr == LA_CY || attr == LA_LASTBASE || attr == LA_FIRSTBASE ? &NSLayoutYAxisAnchor_c
                  : &NSLayoutXAxisAnchor_c;
    id a = objc_autorelease(cx_alloc_instance(k));
    an(a)->view = v;
    an(a)->attr = attr;
    return a;
}
#define ANCHOR(name, attr) static id name(id self, SEL c) { (void)c; return anchor_for(self, attr); }
ANCHOR(v_leading_a, LA_LEADING) ANCHOR(v_trailing_a, LA_TRAILING) ANCHOR(v_left_a, LA_LEFT) ANCHOR(v_right_a, LA_RIGHT)
ANCHOR(v_top_a, LA_TOP) ANCHOR(v_bottom_a, LA_BOTTOM) ANCHOR(v_width_a, LA_WIDTH) ANCHOR(v_height_a, LA_HEIGHT)
ANCHOR(v_cx_a, LA_CX) ANCHOR(v_cy_a, LA_CY) ANCHOR(v_lastbase_a, LA_LASTBASE) ANCHOR(v_firstbase_a, LA_FIRSTBASE)
static id an_rel(id self, int rel, id other, double mult, double k) {
    anc_t *a = an(self), *b = other ? an(other) : 0;
    return lc_make(a->view, a->attr, rel, b ? b->view : 0, b ? b->attr : LA_NONE, mult, k);
}
static id an_eq(id s, SEL c, id o) { (void)c; return an_rel(s, 0, o, 1, 0); }
static id an_eq_k(id s, SEL c, id o, double k) { (void)c; return an_rel(s, 0, o, 1, k); }
static id an_ge(id s, SEL c, id o) { (void)c; return an_rel(s, 1, o, 1, 0); }
static id an_ge_k(id s, SEL c, id o, double k) { (void)c; return an_rel(s, 1, o, 1, k); }
static id an_le(id s, SEL c, id o) { (void)c; return an_rel(s, -1, o, 1, 0); }
static id an_le_k(id s, SEL c, id o, double k) { (void)c; return an_rel(s, -1, o, 1, k); }
static id an_eq_const(id s, SEL c, double k) { (void)c; return an_rel(s, 0, 0, 1, k); }
static id an_ge_const(id s, SEL c, double k) { (void)c; return an_rel(s, 1, 0, 1, k); }
static id an_le_const(id s, SEL c, double k) { (void)c; return an_rel(s, -1, 0, 1, k); }
static id an_eq_m(id s, SEL c, id o, double m) { (void)c; return an_rel(s, 0, o, m, 0); }
static id an_eq_mk(id s, SEL c, id o, double m, double k) { (void)c; return an_rel(s, 0, o, m, k); }
static id an_ge_mk(id s, SEL c, id o, double m, double k) { (void)c; return an_rel(s, 1, o, m, k); }
static id an_le_mk(id s, SEL c, id o, double m, double k) { (void)c; return an_rel(s, -1, o, m, k); }
static id an_ge_m(id s, SEL c, id o, double m) { (void)c; return an_rel(s, 1, o, m, 0); }
static id an_le_m(id s, SEL c, id o, double m) { (void)c; return an_rel(s, -1, o, m, 0); }
/* anchorWithOffsetToAnchor: e afins não são suportados; system spacing = 8 */
static id an_eq_sys(id s, SEL c, id o, double m) { (void)c; return an_rel(s, 0, o, 1, 8 * m); }
static id an_ge_sys(id s, SEL c, id o, double m) { (void)c; return an_rel(s, 1, o, 1, 8 * m); }

/* ---- NSView */
static void v_set_tamic(id self, SEL c, BOOL b) { (void)c; vs(self)->no_tamic = !b; layout_dirty = YES; mark(self); }
static BOOL v_tamic(id self, SEL c) { (void)c; return !vs(self)->no_tamic; }
static void v_add_constraint(id self, SEL c, id k) { (void)self; (void)c; lc_set_active(k, 0, YES); }
static void v_add_constraints(id self, SEL c, id arr) { (void)self; lc_activate_all(0, c, arr); }
static void v_remove_constraint(id self, SEL c, id k) { (void)self; (void)c; lc_set_active(k, 0, NO); }
static void v_remove_constraints(id self, SEL c, id arr) { (void)self; lc_deactivate_all(0, c, arr); }
static id v_constraints(id self, SEL c) {
    (void)c;
    id r = objc_autorelease(cx_arr_new(&NSMutableArray_c));
    for (u32 i = 0; i < nlc; i++) {
        lc_t *s = lc(lcs[i]);
        if (s->a == self || s->b == self || (is_view(s->a) && vs(s->a)->superview == self)) cx_arr_add(r, lcs[i]);
    }
    return r;
}
static void v_layout_now(id self, SEL c);
/* ---- NSLayoutGuide: retângulo de layout sem desenho, dono de uma view (o resolvedor o trata como view) */
static BOOL is_guide(id o) { return o && cx_is_subclass(o->isa, &NSLayoutGuide_c); }
static void v_add_guide(id self, SEL c, id g) {
    (void)c;
    if (!g) return;
    vstate_t *gs = vs(g);
    gs->superview = self;           /* não entra em subviews: não desenha nem recebe eventos */
    gs->hidden = YES;
    gs->no_tamic = YES;
    gs->window = vs(self)->window;
    objc_retain(g);
}
static void v_remove_guide(id self, SEL c, id g) { (void)self; (void)c; if (g) vs(g)->superview = 0; }
static id lg_owning(id self, SEL c) { (void)c; return vs(self)->superview; }
static NSRect lg_frame(id self, SEL c) { (void)c; return vs(self)->frame; }
static id guide_pinned(id owner, double inset) {
    id g = MSG0(id, cx_alloc_instance(&NSLayoutGuide_c), "init");
    v_add_guide(owner, 0, g);
    int at[4] = {LA_LEADING, LA_TRAILING, LA_TOP, LA_BOTTOM};
    double k[4] = {inset, -inset, inset, -inset};
    for (int i = 0; i < 4; i++) lc_set_active(lc_make(g, at[i], 0, owner, at[i], 1, k[i]), 0, YES);
    return g;
}
static id v_safe_area(id self, SEL c) {
    (void)c;
    static id owners[64], guides[64];
    for (int i = 0; i < 64; i++) if (owners[i] == self) return guides[i];
    for (int i = 0; i < 64; i++) if (!owners[i]) { owners[i] = self; return guides[i] = guide_pinned(self, 0); }
    return guide_pinned(self, 0);
}
static id v_margins(id self, SEL c) {
    (void)c;
    static id owners[64], guides[64];
    for (int i = 0; i < 64; i++) if (owners[i] == self) return guides[i];
    for (int i = 0; i < 64; i++) if (!owners[i]) { owners[i] = self; return guides[i] = guide_pinned(self, 20); }
    return guide_pinned(self, 20);
}
/* ---- resolvedor (eixos horizontal e vertical independentes) */
typedef struct { id v; double x, y, w, h; BOOL hdone, vdone; double hs[4], vsl[4]; u8 hset[4], vset[4]; int depth; } lv_t;
static lv_t *lv;
static u32 nlv, caplv;
static double lw_w, lw_h;        /* conteúdo da janela em resolução */
static id lw_content;

static lv_t *lv_find(id v) { for (u32 i = 0; i < nlv; i++) if (lv[i].v == v) return &lv[i]; return 0; }
static void lv_add(id v) {
    if (!is_view(v) || !vs(v)->no_tamic || v == lw_content || lv_find(v)) return;
    if (nlv == caplv) { caplv = caplv ? caplv * 2 : 32; lv = realloc(lv, caplv * sizeof *lv); }
    lv_t *e = &lv[nlv++];
    memset(e, 0, sizeof *e);
    e->v = v;
    for (id p = vs(v)->superview; p; p = vs(p)->superview) e->depth++;
}
/* posição absoluta (da janela, y para baixo) e tamanho de uma view num eixo, se já conhecidos */
static BOOL get_axis(id v, BOOL h, double *pos, double *size) {
    if (!is_view(v)) return NO;
    lv_t *e = lv_find(v);
    if (e) {
        if (!(h ? e->hdone : e->vdone)) return NO;
        *pos = h ? e->x : e->y;
        *size = h ? e->w : e->h;
        return YES;
    }
    if (v == lw_content) { *pos = 0; *size = h ? lw_w : lw_h; return YES; }
    vstate_t *s = vs(v);
    if (!s->superview) return NO;
    double pp, ps;
    if (!get_axis(s->superview, h, &pp, &ps)) return NO;
    if (h) { *pos = pp + s->frame.o.x; *size = s->frame.s.w; }
    else { *pos = is_flipped(s->superview) ? pp + s->frame.o.y : pp + ps - s->frame.o.y - s->frame.s.h; *size = s->frame.s.h; }
    return YES;
}
static BOOL axis_h(int a) { return a == LA_LEFT || a == LA_RIGHT || a == LA_LEADING || a == LA_TRAILING || a == LA_WIDTH || a == LA_CX; }
static int slot_of(int a) {
    switch (a) {
    case LA_LEFT: case LA_LEADING: case LA_TOP: case LA_FIRSTBASE: return 0;
    case LA_RIGHT: case LA_TRAILING: case LA_BOTTOM: case LA_LASTBASE: return 1;
    case LA_CX: case LA_CY: return 2;
    default: return 3;
    }
}
static double baseline_off(id v) {   /* distância do fundo da view até a linha de base do texto */
    vstate_t *s = vs(v);
    if (s->kind != K_TEXT && s->kind != K_BUTTON) return 0;
    return s->frame.s.h > 0 ? s->frame.s.h * 0.25 : 4;
}
static double attr_val(id v, int a, double pos, double size) {
    switch (slot_of(a)) {
    case 0: return a == LA_FIRSTBASE ? pos + size - baseline_off(v) : pos;
    case 1: return a == LA_LASTBASE ? pos + size - baseline_off(v) : pos + size;
    case 2: return pos + size / 2;
    default: return size;
    }
}
static BOOL get_attr(id v, int a, double *out) {
    double p, s;
    if (!get_axis(v, axis_h(a), &p, &s)) return NO;
    *out = attr_val(v, a, p, s);
    return YES;
}
static BOOL put_slot(lv_t *e, int a, double val) {
    BOOL h = axis_h(a);
    int sl = slot_of(a);
    u8 *set = h ? e->hset : e->vset;
    double *sv = h ? e->hs : e->vsl;
    if (h ? e->hdone : e->vdone) return NO;
    if (a == LA_LASTBASE || a == LA_FIRSTBASE) { sl = 1; val += baseline_off(e->v); }   /* guarda como fundo */
    if (set[sl]) return NO;
    set[sl] = 1;
    sv[sl] = val;
    return YES;
}
/* resolve um eixo a partir de 2 dos 4 valores (início, fim, centro, tamanho) */
static BOOL solve_axis(const u8 *set, const double *v, double *pos, double *size) {
    if (set[3]) {
        *size = v[3];
        if (set[0]) *pos = v[0];
        else if (set[1]) *pos = v[1] - v[3];
        else if (set[2]) *pos = v[2] - v[3] / 2;
        else return NO;
        return YES;
    }
    if (set[0] && set[1]) { *pos = v[0]; *size = v[1] - v[0]; return YES; }
    if (set[0] && set[2]) { *pos = v[0]; *size = 2 * (v[2] - v[0]); return YES; }
    if (set[1] && set[2]) { *size = 2 * (v[1] - v[2]); *pos = v[1] - *size; return YES; }
    return NO;
}
static BOOL try_done(lv_t *e) {
    BOOL any = NO;
    double p, s;
    if (!e->hdone && solve_axis(e->hset, e->hs, &p, &s)) { e->x = p; e->w = s < 0 ? 0 : s; e->hdone = YES; any = YES; }
    if (!e->vdone && solve_axis(e->vset, e->vsl, &p, &s)) { e->y = p; e->h = s < 0 ? 0 : s; e->vdone = YES; any = YES; }
    return any;
}
static BOOL propagate(BOOL optional) {
    BOOL any = NO;
    for (u32 i = 0; i < nlc; i++) {
        lc_t *s = lc(lcs[i]);
        if (s->rel != 0 || (optional ? s->prio >= 1000 : s->prio < 1000)) continue;
        lv_t *ea = lv_find(s->a), *eb = s->b ? lv_find(s->b) : 0;
        double v;
        if (ea) {
            if (!s->b) { if (put_slot(ea, s->aa, s->k)) any = YES; }
            else if (get_attr(s->b, s->ba, &v) && put_slot(ea, s->aa, s->mult * v + s->k)) any = YES;
        }
        if (eb && s->mult != 0 && get_attr(s->a, s->aa, &v))
            if (put_slot(eb, s->ba, (v - s->k) / s->mult)) any = YES;
    }
    for (u32 i = 0; i < nlv; i++) if (try_done(&lv[i])) any = YES;
    return any;
}
static int nset(const u8 *s) { return s[0] + s[1] + s[2] + s[3]; }
/* sem informação suficiente: completa UM eixo de UMA view (o menos restrito) e volta a propagar */
static BOOL fallback(BOOL positions) {
    lv_t *best = 0;
    BOOL best_h = NO;
    int best_n = 99;
    for (u32 i = 0; i < nlv; i++) {
        lv_t *e = &lv[i];
        for (int ax = 0; ax < 2; ax++) {
            BOOL h = ax == 0;
            if (h ? e->hdone : e->vdone) continue;
            const u8 *set = h ? e->hset : e->vset;
            if (!positions && set[3]) continue;                       /* já tem tamanho: falta posição */
            if (positions) { double pp, ps; if (!vs(e->v)->superview || !get_axis(vs(e->v)->superview, h, &pp, &ps)) continue; }
            int n = nset(set) * 4 + (positions ? 0 : e->depth == 0);
            if (!positions) {   /* views sem tamanho intrínseco (scroll views, containers) ficam por último */
                NSSize z = MSG0(NSSize, e->v, "intrinsicContentSize");
                if ((h ? z.w : z.h) <= 0) n += 100;
            }
            if (n < best_n) { best_n = n; best = e; best_h = h; }
        }
    }
    if (!best) return NO;
    vstate_t *s = vs(best->v);
    u8 *set = best_h ? best->hset : best->vset;
    double *sv = best_h ? best->hs : best->vsl;
    if (!positions) {
        NSSize z = MSG0(NSSize, best->v, "intrinsicContentSize");
        double iz = best_h ? z.w : z.h, fz = best_h ? s->frame.s.w : s->frame.s.h;
        set[3] = 1;
        sv[3] = iz > 0 ? iz : fz;
        /* se já havia dois valores (ex.: início e centro), o tamanho manda e o centro sai */
        if (set[0] && (set[1] || set[2])) set[1] = set[2] = 0;
        else if (set[1] && set[2]) set[2] = 0;
    } else {
        double pp, ps;
        get_axis(s->superview, best_h, &pp, &ps);
        double size = set[3] ? sv[3] : best_h ? s->frame.s.w : s->frame.s.h;
        set[0] = 1;
        sv[0] = best_h ? pp + s->frame.o.x : is_flipped(s->superview) ? pp + s->frame.o.y : pp + ps - s->frame.o.y - size;
        set[1] = set[2] = 0;
        if (!set[3]) { set[3] = 1; sv[3] = size; }
    }
    try_done(best);
    return YES;
}
static void apply_inequalities(void) {
    for (int pass = 0; pass < 3; pass++)
        for (u32 i = 0; i < nlc; i++) {
            lc_t *s = lc(lcs[i]);
            if (s->rel == 0) continue;
            lv_t *e = lv_find(s->a);
            BOOL h = axis_h(s->aa);
            if (!e || !(h ? e->hdone : e->vdone)) continue;
            double target = s->k, v;
            if (s->b) { if (!get_attr(s->b, s->ba, &v)) continue; target = s->mult * v + s->k; }
            double cur = attr_val(e->v, s->aa, h ? e->x : e->y, h ? e->w : e->h);
            if (!(s->rel > 0 ? cur < target - 0.01 : cur > target + 0.01)) continue;
            double d = target - cur;
            double *pos = h ? &e->x : &e->y, *size = h ? &e->w : &e->h;
            const u8 *set = h ? e->hset : e->vset;
            switch (slot_of(s->aa)) {
            case 3: if (set[1] && !set[0]) *pos -= d; else if (set[2] && !set[0]) *pos -= d / 2; *size = target; break;
            case 0: if (set[1]) *size -= d; *pos += d; break;        /* fim preso: encolhe */
            case 1: if (set[0]) *size += d; else *pos += d; break;
            case 2: *pos += d; break;
            }
            if (*size < 0) *size = 0;
        }
}
static int lv_depth_cmp(const void *a, const void *b) { return ((const lv_t *)a)->depth - ((const lv_t *)b)->depth; }
static void autolayout_window(id w) {
    if (!nlc) return;
    wstate_t *ww = ws(w);
    lw_content = ww->content;
    lw_w = ww->frame.s.w;
    lw_h = ww->frame.s.h;
    nlv = 0;
    for (u32 i = 0; i < nlc; i++) {
        lc_t *s = lc(lcs[i]);
        if (is_guide(s->a) && vs(s->a)->superview) vs(s->a)->window = vs(vs(s->a)->superview)->window;
        if (is_guide(s->b) && vs(s->b)->superview) vs(s->b)->window = vs(vs(s->b)->superview)->window;
        if (is_view(s->a) && vs(s->a)->window == w) lv_add(s->a);
        if (is_view(s->b) && vs(s->b)->window == w) lv_add(s->b);
    }
    if (!nlv) return;
    qsort(lv, nlv, sizeof *lv, lv_depth_cmp);
    for (int round = 0; round < 256; round++) {
        if (propagate(NO)) continue;
        if (propagate(YES)) continue;
        if (fallback(NO)) continue;
        if (fallback(YES)) continue;
        break;
    }
    apply_inequalities();
    /* grava os frames (pais antes dos filhos), em coordenadas do superview */
    for (u32 i = 0; i < nlv; i++) {
        lv_t *e = &lv[i];
        vstate_t *s = vs(e->v);
        double px, pw, py, ph;
        if (!e->hdone || !e->vdone || !s->superview) continue;
        if (!get_axis(s->superview, YES, &px, &pw) || !get_axis(s->superview, NO, &py, &ph)) continue;
        NSRect f = r_make(e->x - px, is_flipped(s->superview) ? e->y - py : py + ph - e->y - e->h, e->w, e->h);
        NSRect o = s->frame;
        if (o.o.x != f.o.x || o.o.y != f.o.y || o.s.w != f.s.w || o.s.h != f.s.h) {
            v_set_frame(e->v, 0, f);
            if (s->kind == K_SCROLL) layout_scroll(e->v);
        }
    }
    nlv = 0;
}
static void v_layout_now(id self, SEL c) { (void)c; id w = vs(self)->window; if (w) autolayout_window(w); }
static NSSize v_fitting(id self, SEL c) {
    /* fittingSize: tamanho intrínseco; para containers, a união dos filhos */
    NSSize z = v_intrinsic(self, c);
    if (z.w > 0) return z;
    vstate_t *s = vs(self);
    double mw = 0, mh = 0;
    for (u32 i = 0; i < s->nsub; i++) { NSRect f = vs(s->sub[i])->frame; if (f.o.x + f.s.w > mw) mw = f.o.x + f.s.w; if (f.o.y + f.s.h > mh) mh = f.o.y + f.s.h; }
    NSSize r = {mw, mh};
    return r;
}

/* ---- Visual Format Language: "H:|-[a]-8-[b(==80)]-|", "V:|-20-[a(>=30@750)]" */
static double vfl_num(const char **p, id metrics, BOOL *ok) {
    const char *s = *p;
    *ok = YES;
    if ((*s >= '0' && *s <= '9') || *s == '.' || *s == '-') { char *e; double d = strtod(s, &e); *p = e; return d; }
    char name[64]; int n = 0;
    while ((isalnum((unsigned char)*s) || *s == '_') && n < 63) name[n++] = *s++;
    name[n] = 0;
    *p = s;
    id v = metrics && n ? MSG(id, metrics, "objectForKey:", cx_str(name)) : 0;
    if (!v) { *ok = NO; return 0; }
    return MSG0(double, v, "doubleValue");
}
/* lê "==N", ">=N", "<=N", "N", "@prio" e opcionalmente "==view" */
static void vfl_pred(const char **p, int *rel, double *k, float *prio, id *other, id views, id metrics) {
    const char *s = *p;
    *rel = 0; *prio = 1000; *other = 0;
    if (s[0] == '=' && s[1] == '=') s += 2;
    else if (s[0] == '>' && s[1] == '=') { *rel = 1; s += 2; }
    else if (s[0] == '<' && s[1] == '=') { *rel = -1; s += 2; }
    BOOL ok;
    const char *save = s;
    *k = vfl_num(&s, metrics, &ok);
    if (!ok) {   /* nome de outra view */
        s = save;
        char name[64]; int n = 0;
        while ((isalnum((unsigned char)*s) || *s == '_') && n < 63) name[n++] = *s++;
        name[n] = 0;
        *other = views ? MSG(id, views, "objectForKey:", cx_str(name)) : 0;
        *k = 0;
    }
    if (*s == '@') { s++; *prio = (float)vfl_num(&s, metrics, &ok); }
    *p = s;
}
static id vfl_parse(id cls, SEL c, id fmt, u64 opts, id metrics, id views) {
    (void)cls; (void)c; (void)opts;
    id out = objc_autorelease(cx_arr_new(&NSMutableArray_c));
    const char *s = cx_cstr(fmt);
    BOOL vert = NO;
    if (s[0] == 'V' && s[1] == ':') { vert = YES; s += 2; }
    else if (s[0] == 'H' && s[1] == ':') s += 2;
    int A_START = vert ? LA_TOP : LA_LEADING, A_END = vert ? LA_BOTTOM : LA_TRAILING, A_SIZE = vert ? LA_HEIGHT : LA_WIDTH;
    id prev = 0;                  /* item anterior (0 = nenhum ainda) */
    BOOL prev_super = NO;
    id sup = 0;
    /* espaço pendente entre o anterior e o próximo */
    BOOL have_gap = NO;
    double gap = 0;
    int gap_rel = 0;
    float gap_prio = 1000;
    while (*s) {
        if (*s == ' ') { s++; continue; }
        if (*s == '|') {
            s++;
            if (prev && have_gap) {
                if (!sup) sup = vs(prev)->superview;
                id k = lc_make(sup, A_END, gap_rel == 0 ? 0 : -gap_rel, prev, A_END, 1, gap);
                lc(k)->prio = gap_prio;
                cx_arr_add(out, k);
            }
            prev = 0; prev_super = YES; have_gap = NO;
            continue;
        }
        if (*s == '-') {
            s++;
            have_gap = YES; gap = 8; gap_rel = 0; gap_prio = 1000;
            if (*s == '(') {
                s++;
                id o;
                vfl_pred(&s, &gap_rel, &gap, &gap_prio, &o, views, metrics);
                while (*s && *s != ')') s++;
                if (*s == ')') s++;
                if (*s == '-') s++;
            } else if (*s != '[' && *s != '|') {
                BOOL ok;
                gap = vfl_num(&s, metrics, &ok);
                if (*s == '@') { s++; gap_prio = (float)vfl_num(&s, metrics, &ok); }
                if (*s == '-') s++;
            } else if (prev_super || *s == '|') gap = 20;    /* espaço padrão até a borda */
            continue;
        }
        if (*s == '[') {
            s++;
            char name[64]; int n = 0;
            while (*s && *s != ']' && *s != '(' && n < 63) name[n++] = *s++;
            name[n] = 0;
            id v = views ? MSG(id, views, "objectForKey:", cx_str(name)) : 0;
            if (!v) { cx_puts_fd(2, "AppKit[mOS]: VFL com view desconhecida\n"); return out; }
            if (*s == '(') {
                s++;
                while (*s && *s != ')') {
                    int rel; double k; float pr; id other;
                    vfl_pred(&s, &rel, &k, &pr, &other, views, metrics);
                    id kk = lc_make(v, A_SIZE, rel, other, other ? A_SIZE : LA_NONE, 1, k);
                    lc(kk)->prio = pr;
                    cx_arr_add(out, kk);
                    if (*s == ',') s++;
                }
                if (*s == ')') s++;
            }
            if (*s == ']') s++;
            if (prev_super) {
                sup = vs(v)->superview;
                id k = lc_make(v, A_START, have_gap ? gap_rel : 0, sup, A_START, 1, have_gap ? gap : 0);
                lc(k)->prio = gap_prio;
                cx_arr_add(out, k);
            } else if (prev) {
                id k = lc_make(v, A_START, have_gap ? gap_rel : 0, prev, A_END, 1, have_gap ? gap : 0);
                lc(k)->prio = gap_prio;
                cx_arr_add(out, k);
            }
            prev = v; prev_super = NO; have_gap = NO;
            continue;
        }
        s++;
    }
    return out;
}

/* ================================================================ Cocoa bindings
 * bind:toObject:withKeyPath:options: e as conexões de binding dos nibs. Modelo → view: a cada volta do
 * laço os valores ligados são relidos por KVC (valueForKeyPath:) e aplicados se mudaram (sem precisar
 * de KVO). View → modelo: quando o usuário altera o controle, setValue:forKeyPath:. */
CLASS(NSUserDefaultsController) CLASS(_CXDefaultsValues)
typedef struct { id view, obj, path, last; char name[24]; BOOL neg; } bind_t;
static bind_t *binds;
static u32 nbinds, capbinds;
static BOOL obj_same(id a, id b) {
    if (a == b) return YES;
    if (!a || !b) return NO;
    return MSG(BOOL, a, "isEqual:", b);
}
static BOOL bval_bool(id v) { return v && (cx_is_str(v) ? strlen(cx_cstr(v)) > 0 : MSG0(BOOL, v, "boolValue")); }
static void bind_apply(bind_t *b, id v) {
    vstate_t *s = is_view(b->view) ? vs(b->view) : 0;
    const char *n = b->name;
    if (!strcmp(n, "value") || !strcmp(n, "stringValue") || !strcmp(n, "doubleValue")) {
        if (!s) return;
        if (s->kind == K_BUTTON && s->btype != BT_PUSH) MSG(void, b->view, "setState:", (i64)bval_bool(v));
        else if (s->kind == K_SLIDER || s->kind == K_PROGRESS) MSG(void, b->view, "setDoubleValue:", v ? MSG0(double, v, "doubleValue") : 0.0);
        else if (s->kind == K_TV) MSG(void, b->view, "setString:", v ? (cx_is_str(v) ? v : MSG0(id, v, "description")) : cx_str(""));
        else MSG(void, b->view, "setStringValue:", v ? (cx_is_str(v) ? v : MSG0(id, v, "description")) : cx_str(""));
    } else if (!strcmp(n, "enabled") || !strncmp(n, "enabled", 7)) MSG(void, b->view, "setEnabled:", (int)bval_bool(v));
    else if (!strcmp(n, "hidden")) MSG(void, b->view, "setHidden:", (int)(bval_bool(v) ^ b->neg));
    else if (!strcmp(n, "editable")) MSG(void, b->view, "setEditable:", (int)bval_bool(v));
    else if (!strcmp(n, "title")) MSG(void, b->view, "setTitle:", v ? (cx_is_str(v) ? v : MSG0(id, v, "description")) : cx_str(""));
    else if (!strcmp(n, "toolTip")) MSG(void, b->view, "setToolTip:", v);
}
static void bind_refresh(void) {
    for (u32 i = 0; i < nbinds; i++) {
        bind_t *b = &binds[i];
        if (!b->view) continue;
        id v = MSG(id, b->obj, "valueForKeyPath:", b->path);
        if (obj_same(v, b->last) && b->last) continue;
        keep(&b->last, v);
        bind_apply(b, v);
    }
}
/* o usuário mexeu no controle: grava no modelo */
static void bind_push(id view) {
    for (u32 i = 0; i < nbinds; i++) {
        bind_t *b = &binds[i];
        if (b->view != view || (strcmp(b->name, "value") && strcmp(b->name, "stringValue") && strcmp(b->name, "doubleValue"))) continue;
        vstate_t *s = vs(view);
        id v;
        if (s->kind == K_BUTTON && s->btype != BT_PUSH) v = cx_num_bool(s->state != 0);
        else if (s->kind == K_SLIDER || s->kind == K_PROGRESS) v = cx_num_d(s->dval);
        else v = s->value ? s->value : cx_str("");
        MSG(void, b->obj, "setValue:forKeyPath:", v, b->path);
        keep(&b->last, v);
    }
}
static void cx_bind(id view, const char *name, id obj, id path) {
    if (!view || !obj || !path) return;
    for (u32 i = 0; i < nbinds; i++) if (binds[i].view == view && !strcmp(binds[i].name, name)) { binds[i].view = 0; }
    if (nbinds == capbinds) { capbinds = capbinds ? capbinds * 2 : 16; binds = realloc(binds, capbinds * sizeof *binds); }
    bind_t *b = &binds[nbinds++];
    memset(b, 0, sizeof *b);
    b->view = view;
    b->obj = objc_retain(obj);
    b->path = objc_retain(path);
    snprintf(b->name, sizeof b->name, "%s", name);
    id v = MSG(id, obj, "valueForKeyPath:", path);
    keep(&b->last, v);
    bind_apply(b, v);
}
static void o_bind(id self, SEL c, id name, id obj, id path, id opts) { (void)c; (void)opts; cx_bind(self, cx_cstr(name), obj, path); }
static void o_unbind(id self, SEL c, id name) { (void)c; for (u32 i = 0; i < nbinds; i++) if (binds[i].view == self && !strcmp(binds[i].name, cx_cstr(name))) binds[i].view = 0; }
/* NSUserDefaultsController.values: proxy de KVC para o NSUserDefaults */
static id udc_shared(id cls, SEL c) { (void)c; static id s; if (!s) s = cx_alloc_instance((cx_class_t *)cls); return s; }
static id udc_values(id self, SEL c) { (void)self; (void)c; static id v; if (!v) v = cx_alloc_instance(&_CXDefaultsValues_c); return v; }
static id udc_defaults(id self, SEL c) { (void)self; (void)c; return MSG0(id, (id)cx_class_named("NSUserDefaults"), "standardUserDefaults"); }
static id dv_get(id self, SEL c, id key) { (void)self; (void)c; return MSG(id, MSG0(id, (id)cx_class_named("NSUserDefaults"), "standardUserDefaults"), "objectForKey:", key); }
static void dv_set(id self, SEL c, id v, id key) { (void)self; (void)c; MSG(void, MSG0(id, (id)cx_class_named("NSUserDefaults"), "standardUserDefaults"), "setObject:forKey:", v, key); }
static void bindings_register(void) {
    M(NSObject, "bind:toObject:withKeyPath:options:", o_bind); M(NSObject, "unbind:", o_unbind);
    DEF(NSUserDefaultsController, NSObject);
    CM(NSUserDefaultsController, "sharedUserDefaultsController", udc_shared); M(NSUserDefaultsController, "values", udc_values);
    M(NSUserDefaultsController, "defaults", udc_defaults);
    DEF(_CXDefaultsValues, NSObject);
    M(_CXDefaultsValues, "valueForKey:", dv_get); M(_CXDefaultsValues, "setValue:forKey:", dv_set);
}
#include "appkit_nib.inc"

/* NSDictionaryOfVariableBindings(a, b): nomes vêm do texto das variáveis */
id _NSDictionaryOfVariableBindings(id names, id first, ...) {
    id d = MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary");
    const char *s = cx_cstr(names);
    va_list ap;
    va_start(ap, first);
    id v = first;
    while (*s && v) {
        while (*s == ' ' || *s == ',') s++;
        char name[64]; int n = 0;
        while (*s && *s != ',' && *s != ' ' && n < 63) name[n++] = *s++;
        name[n] = 0;
        if (n) MSG(void, d, "setObject:forKey:", v, cx_str(name));
        v = va_arg(ap, id);
    }
    va_end(ap);
    return d;
}

void cx_appkit_init(void) {
    NSFontAttributeName = cx_str("NSFont");
    NSForegroundColorAttributeName = cx_str("NSColor");
    NSBackgroundColorAttributeName = cx_str("NSBackgroundColor");
    NSParagraphStyleAttributeName = cx_str("NSParagraphStyle");
    NSApplicationDidFinishLaunchingNotification = cx_str("NSApplicationDidFinishLaunchingNotification");
    NSApplicationWillFinishLaunchingNotification = cx_str("NSApplicationWillFinishLaunchingNotification");
    NSApplicationWillTerminateNotification = cx_str("NSApplicationWillTerminateNotification");
    NSWindowWillCloseNotification = cx_str("NSWindowWillCloseNotification");
    NSWindowDidResizeNotification = cx_str("NSWindowDidResizeNotification");
    NSWindowDidBecomeKeyNotification = cx_str("NSWindowDidBecomeKeyNotification");
    NSViewFrameDidChangeNotification = cx_str("NSViewFrameDidChangeNotification");
    NSControlTextDidChangeNotification = cx_str("NSControlTextDidChangeNotification");

    DEF(NSResponder, NSObject);
    M(NSResponder, "mouseDown:", r_mouse_down); M(NSResponder, "mouseUp:", r_mouse_up); M(NSResponder, "mouseDragged:", r_mouse_dragged);
    M(NSResponder, "rightMouseDown:", r_right_down); M(NSResponder, "scrollWheel:", r_scroll); M(NSResponder, "keyDown:", r_key_down);
    M(NSResponder, "mouseMoved:", noop1_m); M(NSResponder, "keyUp:", noop1_m); M(NSResponder, "acceptsFirstResponder", no_m);
    M(NSResponder, "becomeFirstResponder", yes_m); M(NSResponder, "resignFirstResponder", yes_m);

    DEF(NSApplication, NSResponder);
    CM(NSApplication, "sharedApplication", ap_shared);
    M(NSApplication, "setDelegate:", ap_set_delegate); M(NSApplication, "delegate", ap_delegate); M(NSApplication, "run", ap_run);
    M(NSApplication, "setActivationPolicy:", ap_set_policy); M(NSApplication, "activateIgnoringOtherApps:", ap_activate); M(NSApplication, "activate", noop_m);
    M(NSApplication, "setMainMenu:", ap_set_menu); M(NSApplication, "mainMenu", ap_menu); M(NSApplication, "terminate:", ap_terminate);
    M(NSApplication, "stop:", ap_stop); M(NSApplication, "finishLaunching", ap_finish_launching); M(NSApplication, "windows", ap_windows);
    M(NSApplication, "keyWindow", ap_key_window); M(NSApplication, "mainWindow", ap_key_window); M(NSApplication, "sendAction:to:from:", ap_send_action);
    M(NSApplication, "runModalForWindow:", ap_run_modal); M(NSApplication, "stopModalWithCode:", ap_stop_modal_code); M(NSApplication, "stopModal", ap_stop_modal);
    M(NSApplication, "setWindowsMenu:", noop1_m); M(NSApplication, "setServicesMenu:", noop1_m); M(NSApplication, "setHelpMenu:", noop1_m);
    M(NSApplication, "setApplicationIconImage:", noop1_m); M(NSApplication, "hide:", noop1_m); M(NSApplication, "orderFrontStandardAboutPanel:", ap_about_panel);
    M(NSApplication, "requestUserAttention:", noop1_m);

    DEF(NSWindow, NSResponder);
    DEF(NSPanel, NSWindow);
    M(NSWindow, "initWithContentRect:styleMask:backing:defer:", w_init); M(NSWindow, "init", w_init_simple);
    CM(NSWindow, "windowWithContentViewController:", w_with_vc);
    M(NSWindow, "setTitle:", w_set_title); M(NSWindow, "title", w_title); M(NSWindow, "contentView", w_content); M(NSWindow, "setContentView:", w_set_content);
    M(NSWindow, "isDocumentEdited", no_m); M(NSWindow, "representedFilename", noop_ret_nil);
    M(NSWindow, "setTouchBar:", noop1_m); M(NSWindow, "touchBar", noop_ret_nil);
    M(NSWindow, "setDelegate:", w_set_delegate); M(NSWindow, "delegate", w_delegate); M(NSWindow, "frame", w_frame);
    M(NSWindow, "contentRectForFrameRect:", w_content_rect_for); M(NSWindow, "frameRectForContentRect:", w_frame_rect_for);
    M(NSWindow, "makeKeyAndOrderFront:", w_show); M(NSWindow, "orderFront:", w_show); M(NSWindow, "orderFrontRegardless", w_show); M(NSWindow, "makeKeyWindow", w_show);
    M(NSWindow, "orderOut:", w_order_out); M(NSWindow, "center", w_center); M(NSWindow, "close", w_close); M(NSWindow, "performClose:", w_perform_close);
    M(NSWindow, "setBackgroundColor:", w_set_bg); M(NSWindow, "backgroundColor", w_bg); M(NSWindow, "isVisible", w_visible);
    M(NSWindow, "setReleasedWhenClosed:", w_set_released); M(NSWindow, "firstResponder", w_first); M(NSWindow, "makeFirstResponder:", w_make_first);
    M(NSWindow, "setMinSize:", w_set_min); M(NSWindow, "setMaxSize:", w_set_min); M(NSWindow, "setContentMinSize:", w_set_min);
    M(NSWindow, "setFrame:display:", w_set_frame); M(NSWindow, "setContentSize:", w_set_content_size); M(NSWindow, "display", w_display);
    M(NSWindow, "setFrameAutosaveName:", noop1_m); M(NSWindow, "setTitlebarAppearsTransparent:", noop1_m); M(NSWindow, "setTitleVisibility:", noop1_m);
    M(NSWindow, "setLevel:", noop1_m); M(NSWindow, "setOpaque:", noop1_m); M(NSWindow, "setHasShadow:", noop1_m); M(NSWindow, "setMovableByWindowBackground:", noop1_m);
    M(NSWindow, "setAcceptsMouseMovedEvents:", noop1_m); M(NSWindow, "setInitialFirstResponder:", noop1_m); M(NSWindow, "setStyleMask:", noop1_m);
    M(NSWindow, "setCollectionBehavior:", noop1_m); M(NSWindow, "setAppearance:", noop1_m); M(NSWindow, "setFrameOrigin:", noop1_m);
    M(NSWindow, "setFrameTopLeftPoint:", noop1_m); M(NSWindow, "windowNumber", w_number); M(NSWindow, "setTabbingMode:", noop1_m);
    M(NSWindow, "occlusionState", w_occlusion); M(NSWindow, "convertRectToScreen:", w_convert_to_screen);
    M(NSWindow, "isKeyWindow", yes_m); M(NSWindow, "isMainWindow", yes_m);

    DEF(NSView, NSResponder);
    M(NSView, "initWithFrame:", v_init_frame); M(NSView, "init", v_init); M(NSView, "frame", v_frame); M(NSView, "bounds", v_bounds);
    M(NSView, "setFrame:", v_set_frame); M(NSView, "setFrameSize:", v_set_size); M(NSView, "setFrameOrigin:", v_set_origin);
    M(NSView, "addSubview:", v_add_sub); M(NSView, "addSubview:positioned:relativeTo:", v_add_positioned);
    M(NSView, "sortSubviewsUsingFunction:context:", v_sort_sub); M(NSView, "setNextKeyView:", noop1_m);
    M(NSView, "removeFromSuperview", v_remove); M(NSView, "subviews", v_subviews);
    M(NSView, "superview", v_superview); M(NSView, "window", v_window); M(NSView, "setNeedsDisplay:", v_set_needs);
    M(NSView, "setNeedsDisplayInRect:", v_set_needs_rect); M(NSView, "display", v_display); M(NSView, "needsDisplay", v_needs);
    M(NSView, "setHidden:", v_set_hidden); M(NSView, "isHidden", v_hidden); M(NSView, "setAutoresizingMask:", v_set_autoresize);
    M(NSView, "autoresizingMask", v_autoresize); M(NSView, "drawRect:", v_draw_rect); M(NSView, "isFlipped", no_m);
    M(NSView, "convertPoint:fromView:", v_convert_from); M(NSView, "convertPoint:toView:", v_convert_to); M(NSView, "convertRect:toView:", v_convert_rect_to);
    M(NSView, "setTag:", v_set_tag); M(NSView, "tag", v_tag); M(NSView, "viewWithTag:", v_view_with_tag); M(NSView, "setToolTip:", v_set_tooltip);
    M(NSView, "setWantsLayer:", v_set_wants_layer); M(NSView, "layer", v_layer); M(NSView, "acceptsFirstResponder", v_accepts_first);
    M(NSView, "intrinsicContentSize", v_intrinsic); M(NSView, "sizeToFit", v_size_to_fit); M(NSView, "fittingSize", v_fitting);
    M(NSView, "setTranslatesAutoresizingMaskIntoConstraints:", v_set_tamic); M(NSView, "translatesAutoresizingMaskIntoConstraints", v_tamic); M(NSView, "viewDidMoveToWindow", noop_m);
    M(NSView, "setAlphaValue:", noop1_m); M(NSView, "updateTrackingAreas", noop_m); M(NSView, "addTrackingArea:", noop1_m);
    M(NSView, "acceptsFirstMouse:", yes_m);

    DEF(NSControl, NSView);
    M(NSControl, "setTarget:", ctl_set_target); M(NSControl, "target", ctl_target); M(NSControl, "setAction:", ctl_set_action); M(NSControl, "action", ctl_action);
    M(NSControl, "setEnabled:", ctl_set_enabled); M(NSControl, "isEnabled", ctl_enabled); M(NSControl, "setFont:", ctl_set_font); M(NSControl, "font", ctl_font);
    M(NSControl, "setAlignment:", ctl_set_align); M(NSControl, "setStringValue:", ctl_set_string); M(NSControl, "stringValue", ctl_string);
    M(NSControl, "setIntValue:", ctl_set_int); M(NSControl, "setIntegerValue:", ctl_set_int); M(NSControl, "setDoubleValue:", ctl_set_double);
    M(NSControl, "setFloatValue:", ctl_set_double); M(NSControl, "intValue", ctl_int); M(NSControl, "integerValue", ctl_int);
    M(NSControl, "doubleValue", ctl_double); M(NSControl, "floatValue", ctl_double); M(NSControl, "sendAction:to:", ctl_send);
    M(NSControl, "mouseDown:", ctl_mouse_down); M(NSControl, "mouseUp:", ctl_mouse_up); M(NSControl, "mouseDragged:", ctl_mouse_dragged);
    M(NSControl, "setDelegate:", ctl_set_delegate); M(NSControl, "delegate", ctl_delegate); M(NSControl, "setContinuous:", noop1_m);
    M(NSControl, "setControlSize:", noop1_m); M(NSControl, "setTextColor:", ctl_set_tcolor); M(NSControl, "textColor", ctl_tcolor);

    DEF(NSButton, NSControl);
    CM(NSButton, "buttonWithTitle:target:action:", bt_with_title); CM(NSButton, "checkboxWithTitle:target:action:", bt_checkbox);
    CM(NSButton, "radioButtonWithTitle:target:action:", bt_radio);
    M(NSButton, "initWithFrame:", bt_init_frame); M(NSButton, "setTitle:", bt_set_title); M(NSButton, "title", bt_title);
    M(NSButton, "setButtonType:", bt_set_type); M(NSButton, "setState:", bt_set_state); M(NSButton, "state", bt_state);
    M(NSButton, "setKeyEquivalent:", bt_set_key); M(NSButton, "setBezelStyle:", bt_set_bezel); M(NSButton, "setBordered:", bt_set_bordered);
    M(NSButton, "setImage:", noop1_m); M(NSButton, "setContentTintColor:", noop1_m);

    DEF(NSTextField, NSControl);
    CM(NSTextField, "labelWithString:", tf_label); CM(NSTextField, "wrappingLabelWithString:", tf_wrapping_label); CM(NSTextField, "textFieldWithString:", tf_field);
    M(NSTextField, "initWithFrame:", tf_init_frame); M(NSTextField, "setEditable:", tf_set_editable); M(NSTextField, "isEditable", tf_editable);
    M(NSTextField, "setBezeled:", tf_set_bezeled); M(NSTextField, "setBordered:", tf_set_bezeled); M(NSTextField, "setDrawsBackground:", tf_set_draws_bg);
    M(NSTextField, "setBackgroundColor:", tf_set_bg); M(NSTextField, "setPlaceholderString:", tf_set_placeholder); M(NSTextField, "setSelectable:", tf_set_selectable);
    M(NSTextField, "setLineBreakMode:", noop1_m); M(NSTextField, "setMaximumNumberOfLines:", noop1_m); M(NSTextField, "setUsesSingleLineMode:", noop1_m);
    M(NSTextField, "setPreferredMaxLayoutWidth:", noop1_m);

    DEF(NSSlider, NSControl);
    CM(NSSlider, "sliderWithValue:minValue:maxValue:target:action:", sl_with);
    M(NSSlider, "initWithFrame:", sl_init_frame); M(NSSlider, "setMinValue:", sl_set_min); M(NSSlider, "setMaxValue:", sl_set_max);
    M(NSSlider, "minValue", sl_min); M(NSSlider, "maxValue", sl_max);
    DEF(NSProgressIndicator, NSView);
    M(NSProgressIndicator, "initWithFrame:", pg_init_frame); M(NSProgressIndicator, "setDoubleValue:", ctl_set_double); M(NSProgressIndicator, "doubleValue", ctl_double);
    M(NSProgressIndicator, "setMinValue:", sl_set_min); M(NSProgressIndicator, "setMaxValue:", sl_set_max); M(NSProgressIndicator, "incrementBy:", pg_inc);
    M(NSProgressIndicator, "setIndeterminate:", noop1_m); M(NSProgressIndicator, "startAnimation:", noop1_m); M(NSProgressIndicator, "stopAnimation:", noop1_m);
    M(NSProgressIndicator, "setStyle:", noop1_m);
    DEF(NSBox, NSView);
    M(NSBox, "initWithFrame:", bx_init_frame); M(NSBox, "setTitle:", bt_set_title); M(NSBox, "setContentView:", v_add_sub); M(NSBox, "setBoxType:", bx_set_type); M(NSBox, "setFillColor:", tf_set_bg); M(NSBox, "setCornerRadius:", bx_set_radius);
    DEF(NSStackView, NSView);
    CM(NSStackView, "stackViewWithViews:", st_with_views);
    M(NSStackView, "initWithFrame:", st_init_frame); M(NSStackView, "addArrangedSubview:", st_add); M(NSStackView, "setOrientation:", st_set_orient);
    M(NSStackView, "setSpacing:", st_set_spacing); M(NSStackView, "setAlignment:", noop1_m); M(NSStackView, "setDistribution:", noop1_m); M(NSStackView, "setEdgeInsets:", noop1_m);
    DEF(NSImageView, NSView);
    M(NSImageView, "initWithFrame:", iv_init_frame); M(NSImageView, "setImage:", iv_set_image); M(NSImageView, "image", iv_image);
    CM(NSImageView, "imageViewWithImage:", iv_with_image); M(NSImageView, "setImageScaling:", noop1_m);
    M(NSButton, "setImage:", iv_set_image); M(NSButton, "image", iv_image);
    DEF(NSScrollView, NSView);
    DEF(NSClipView, NSView);
    M(NSScrollView, "initWithFrame:", sv_init_frame); M(NSScrollView, "setDocumentView:", sv_set_doc); M(NSScrollView, "documentView", sv_doc);
    M(NSScrollView, "contentView", sv_content); M(NSScrollView, "scrollWheel:", sv_scroll); M(NSScrollView, "setHasVerticalScroller:", noop1_m);
    M(NSScrollView, "setHasHorizontalScroller:", noop1_m); M(NSScrollView, "setAutohidesScrollers:", noop1_m); M(NSScrollView, "setBorderType:", noop1_m);
    M(NSScrollView, "setDrawsBackground:", noop1_m);
    DEF(NSTableView, NSControl);
    M(NSTableView, "initWithFrame:", tv_init_frame); M(NSTableView, "addTableColumn:", tv_add_col); M(NSTableView, "tableColumns", tv_cols);
    M(NSTableView, "setDataSource:", tv_set_ds); M(NSTableView, "dataSource", tv_ds); M(NSTableView, "reloadData", tv_reload);
    M(NSTableView, "numberOfRows", tv_num_rows); M(NSTableView, "selectedRow", tv_selected); M(NSTableView, "clickedRow", tv_clicked);
    M(NSTableView, "setRowHeight:", tv_set_row_h); M(NSTableView, "setHeaderView:", tv_set_header); M(NSTableView, "headerView", tv_header);
    M(NSTableView, "setUsesAlternatingRowBackgroundColors:", tv_set_alt); M(NSTableView, "setDoubleAction:", tv_set_double);
    M(NSTableView, "makeViewWithIdentifier:owner:", tv_make_view); M(NSTableView, "scrollRowToVisible:", tv_scroll_row);
    M(NSTableView, "selectRowIndexes:byExtendingSelection:", tv_select); M(NSTableView, "deselectAll:", tv_deselect_all);
    M(NSTableView, "mouseDown:", tv_mouse_down); M(NSTableView, "setStyle:", noop1_m); M(NSTableView, "setGridStyleMask:", noop1_m);
    M(NSTableView, "setAllowsMultipleSelection:", noop1_m); M(NSTableView, "sizeLastColumnToFit", noop_m);
    DEF(NSTableColumn, NSObject);
    M(NSTableColumn, "initWithIdentifier:", col_init); M(NSTableColumn, "identifier", col_ident); M(NSTableColumn, "setTitle:", col_set_title);
    M(NSTableColumn, "title", col_title); M(NSTableColumn, "setWidth:", col_set_width); M(NSTableColumn, "width", col_width);
    M(NSTableColumn, "setMinWidth:", noop1_m); M(NSTableColumn, "setEditable:", noop1_m);
    DEF(NSTableCellView, NSView);
    M(NSTableCellView, "setTextField:", tcv_set_tf); M(NSTableCellView, "textField", tcv_tf);
    DEF(NSIndexSet, NSObject);
    CM(NSIndexSet, "indexSetWithIndex:", ixs_with_index); M(NSIndexSet, "firstIndex", ixs_first);
    { extern cx_class_t NSData_x __asm__("OBJC_CLASS_$_NSData");
      cx_add_cmethod(&NSData_x, "dataWithContentsOfFile:", (void *)dt_with_file); cx_add_cmethod(&NSData_x, "dataWithBytes:length:", (void *)dt_with_bytes);
      cx_add_method(&NSData_x, "initWithContentsOfFile:", (void *)dt_init_file); cx_add_method(&NSData_x, "initWithBytes:length:", (void *)dt_init_bytes);
      cx_add_method(&NSData_x, "bytes", (void *)dt_bytes); cx_add_method(&NSData_x, "length", (void *)dt_length);
      cx_add_method(&NSData_x, "writeToFile:atomically:", (void *)dt_write); }
    DEF(NSTextView, NSView);
    M(NSTextView, "initWithFrame:", tx_init_frame); M(NSTextView, "string", tx_string); M(NSTextView, "setString:", tx_set_string);
    M(NSTextView, "insertText:", tx_insert_text); M(NSTextView, "insertText:replacementRange:", tx_insert_text);
    M(NSTextView, "textStorage", tx_storage); M(NSTextView, "appendAttributedString:", tx_append_astr); M(NSTextView, "setAttributedString:", tx_set_astr);
    M(NSTextView, "attributedString", tx_string); M(NSTextView, "length", tx_length); M(NSTextView, "setEditable:", tx_set_editable);
    M(NSTextView, "isEditable", tx_editable); M(NSTextView, "setSelectable:", noop1_m); M(NSTextView, "setFont:", ctl_set_font);
    M(NSTextView, "setTextColor:", ctl_set_tcolor); M(NSTextView, "setBackgroundColor:", tx_set_bg); M(NSTextView, "setDrawsBackground:", tx_set_draws_bg);
    M(NSTextView, "setDelegate:", ctl_set_delegate); M(NSTextView, "delegate", ctl_delegate); M(NSTextView, "mouseDown:", tx_mouse_down_m);
    M(NSTextView, "scrollToEndOfDocument:", tx_scroll_end); M(NSTextView, "selectedRange", tx_sel_range); M(NSTextView, "setSelectedRange:", tx_set_sel_range);
    M(NSTextView, "setRichText:", noop1_m); M(NSTextView, "setVerticallyResizable:", noop1_m); M(NSTextView, "setHorizontallyResizable:", noop1_m);
    M(NSTextView, "setMinSize:", noop1_m); M(NSTextView, "setMaxSize:", noop1_m); M(NSTextView, "setTextContainerInset:", noop1_m);
    M(NSTextView, "setAutomaticQuoteSubstitutionEnabled:", noop1_m); M(NSTextView, "setAutomaticSpellingCorrectionEnabled:", noop1_m);
    M(NSTextView, "setAllowsUndo:", noop1_m); M(NSTextView, "setUsesFindBar:", noop1_m); M(NSTextView, "setContinuousSpellCheckingEnabled:", noop1_m);
    CM(NSTextView, "scrollableTextView", tx_scrollable);
    M(NSTextView, "copy:", tx_copy); M(NSTextView, "cut:", tx_cut); M(NSTextView, "paste:", tx_paste); M(NSTextView, "selectAll:", tx_select_all);
    M(NSTextView, "undo:", tx_undo); M(NSTextView, "redo:", tx_redo); M(NSTextView, "mouseDragged:", tx_mouse_dragged);
    DEF(NSPasteboard, NSObject);
    CM(NSPasteboard, "generalPasteboard", pb_general); CM(NSPasteboard, "pasteboardWithName:", pb_named); CM(NSPasteboard, "pasteboardWithUniqueName", pb_unique);
    M(NSPasteboard, "name", pb_name); M(NSPasteboard, "changeCount", pb_count); M(NSPasteboard, "releaseGlobally", pb_release);
    M(NSPasteboard, "clearContents", pb_clear); M(NSPasteboard, "declareTypes:owner:", pb_declare); M(NSPasteboard, "addTypes:owner:", pb_add_types);
    M(NSPasteboard, "setString:forType:", pb_set_string); M(NSPasteboard, "stringForType:", pb_string);
    M(NSPasteboard, "setData:forType:", pb_set_data); M(NSPasteboard, "dataForType:", pb_data);
    M(NSPasteboard, "setPropertyList:forType:", pb_set_plist); M(NSPasteboard, "propertyListForType:", pb_plist);
    M(NSPasteboard, "writeObjects:", pb_write_objects); M(NSPasteboard, "readObjectsForClasses:options:", pb_read_objects);
    M(NSPasteboard, "canReadObjectForClasses:options:", pb_can_read); M(NSPasteboard, "canReadItemWithDataConformingToTypes:", pb_can_conform);
    M(NSPasteboard, "pasteboardItems", pb_items); M(NSPasteboard, "types", pb_types); M(NSPasteboard, "availableTypeFromArray:", pb_available);
    DEF(NSPasteboardItem, NSObject);
    M(NSPasteboardItem, "init", pi_init); M(NSPasteboardItem, "types", pi_types); M(NSPasteboardItem, "availableTypeFromArray:", pi_available);
    M(NSPasteboardItem, "setData:forType:", pi_set); M(NSPasteboardItem, "setString:forType:", pi_set); M(NSPasteboardItem, "setPropertyList:forType:", pi_set);
    M(NSPasteboardItem, "dataForType:", pi_data); M(NSPasteboardItem, "stringForType:", pi_string); M(NSPasteboardItem, "propertyListForType:", pi_plist);
    NSPasteboardTypeString = NSStringPboardType = cx_str("public.utf8-plain-text");
    DEF(NSVisualEffectView, NSView);
    DEF(NSColorWell, NSControl);
    DEF(NSScroller, NSView);
    CM(NSScroller, "preferredScrollerStyle", no_m); /* NSScrollerStyleLegacy: o Chromium aceita 0 */

    cx_define_class(&NSColor_c, &NSColor_m, "NSColor", &NSObject_c, 32);
    CM(NSColor, "colorWithRed:green:blue:alpha:", cl_rgb); CM(NSColor, "colorWithCalibratedRed:green:blue:alpha:", cl_rgb);
    CM(NSColor, "colorWithDeviceRed:green:blue:alpha:", cl_rgb); CM(NSColor, "colorWithSRGBRed:green:blue:alpha:", cl_rgb);
    CM(NSColor, "colorWithWhite:alpha:", cl_white_alpha); CM(NSColor, "colorWithCalibratedWhite:alpha:", cl_white_alpha);
    CM(NSColor, "colorWithHue:saturation:brightness:alpha:", cl_hsb); CM(NSColor, "colorWithCalibratedHue:saturation:brightness:alpha:", cl_hsb);
    CM(NSColor, "blackColor", cl_black); CM(NSColor, "whiteColor", cl_white); CM(NSColor, "redColor", cl_red); CM(NSColor, "greenColor", cl_green);
    CM(NSColor, "blueColor", cl_blue); CM(NSColor, "yellowColor", cl_yellow); CM(NSColor, "orangeColor", cl_orange); CM(NSColor, "purpleColor", cl_purple);
    CM(NSColor, "grayColor", cl_gray); CM(NSColor, "lightGrayColor", cl_lgray); CM(NSColor, "darkGrayColor", cl_dgray); CM(NSColor, "clearColor", cl_clear);
    CM(NSColor, "brownColor", cl_brown); CM(NSColor, "cyanColor", cl_cyan); CM(NSColor, "magentaColor", cl_magenta);
    CM(NSColor, "systemRedColor", cl_red); CM(NSColor, "systemGreenColor", cl_green); CM(NSColor, "systemBlueColor", cl_blue);
    CM(NSColor, "systemYellowColor", cl_yellow); CM(NSColor, "systemOrangeColor", cl_orange); CM(NSColor, "systemPurpleColor", cl_purple);
    CM(NSColor, "systemPinkColor", cl_pink); CM(NSColor, "systemTealColor", cl_teal); CM(NSColor, "systemIndigoColor", cl_indigo);
    CM(NSColor, "systemGrayColor", cl_gray); CM(NSColor, "controlAccentColor", cl_blue); CM(NSColor, "linkColor", cl_blue);
    CM(NSColor, "labelColor", cl_label); CM(NSColor, "textColor", cl_label); CM(NSColor, "controlTextColor", cl_label);
    CM(NSColor, "secondaryLabelColor", cl_sec_label); CM(NSColor, "tertiaryLabelColor", cl_ter_label); CM(NSColor, "placeholderTextColor", cl_ter_label);
    CM(NSColor, "windowBackgroundColor", cl_window_bg); CM(NSColor, "controlBackgroundColor", cl_control_bg); CM(NSColor, "textBackgroundColor", cl_control_bg);
    CM(NSColor, "controlColor", cl_control_bg); CM(NSColor, "separatorColor", cl_separator); CM(NSColor, "gridColor", cl_separator);
    CM(NSColor, "underPageBackgroundColor", cl_window_bg);
    M(NSColor, "set", cl_set); M(NSColor, "setFill", cl_set_fill); M(NSColor, "setStroke", cl_set_stroke);
    M(NSColor, "colorWithAlphaComponent:", cl_with_alpha); M(NSColor, "redComponent", cl_r); M(NSColor, "greenComponent", cl_g);
    M(NSColor, "blueComponent", cl_b); M(NSColor, "alphaComponent", cl_a); M(NSColor, "CGColor", self_m);
    M(NSColor, "colorUsingColorSpace:", self_m); M(NSColor, "blendedColorWithFraction:ofColor:", cl_blend);

    cx_define_class(&NSFont_c, &NSFont_m, "NSFont", &NSObject_c, 16);
    CM(NSFont, "systemFontOfSize:", ft_system); CM(NSFont, "boldSystemFontOfSize:", ft_bold); CM(NSFont, "systemFontOfSize:weight:", ft_system_w);
    CM(NSFont, "monospacedSystemFontOfSize:weight:", ft_mono); CM(NSFont, "monospacedDigitSystemFontOfSize:weight:", ft_system_w);
    CM(NSFont, "fontWithName:size:", ft_named); CM(NSFont, "labelFontOfSize:", ft_system); CM(NSFont, "userFontOfSize:", ft_system);
    CM(NSFont, "userFixedPitchFontOfSize:", ft_system); CM(NSFont, "messageFontOfSize:", ft_system); CM(NSFont, "titleBarFontOfSize:", ft_bold);
    CM(NSFont, "systemFontSize", ft_sys_size); CM(NSFont, "smallSystemFontSize", ft_small_size);
    M(NSFont, "pointSize", ft_size); M(NSFont, "fontName", ft_name);

    DEF(NSBezierPath, NSObject);
    CM(NSBezierPath, "bezierPath", bp_new); CM(NSBezierPath, "bezierPathWithRect:", bp_rect); CM(NSBezierPath, "bezierPathWithOvalInRect:", bp_oval);
    CM(NSBezierPath, "bezierPathWithRoundedRect:xRadius:yRadius:", bp_rrect); CM(NSBezierPath, "fillRect:", bp_cfill_rect);
    CM(NSBezierPath, "strokeRect:", bp_cstroke_rect); CM(NSBezierPath, "strokeLineFromPoint:toPoint:", bp_cstroke_line);
    M(NSBezierPath, "moveToPoint:", bp_move); M(NSBezierPath, "lineToPoint:", bp_line); M(NSBezierPath, "curveToPoint:controlPoint1:controlPoint2:", bp_curve);
    M(NSBezierPath, "closePath", bp_close); M(NSBezierPath, "appendBezierPathWithRect:", bp_app_rect); M(NSBezierPath, "appendBezierPathWithOvalInRect:", bp_app_oval);
    M(NSBezierPath, "appendBezierPathWithRoundedRect:xRadius:yRadius:", bp_app_rrect); M(NSBezierPath, "fill", bp_fill); M(NSBezierPath, "stroke", bp_stroke);
    M(NSBezierPath, "setLineWidth:", bp_set_lw); M(NSBezierPath, "lineWidth", bp_lw); M(NSBezierPath, "removeAllPoints", bp_remove_all);
    M(NSBezierPath, "bounds", bp_bounds); M(NSBezierPath, "addClip", bp_add_clip); M(NSBezierPath, "setLineCapStyle:", noop1_m);
    M(NSBezierPath, "setLineJoinStyle:", noop1_m); M(NSBezierPath, "setLineDash:count:phase:", noop1_m);

    cx_add_method(&NSString_c, "drawAtPoint:withAttributes:", (void *)str_draw_at);
    cx_add_method(&NSString_c, "drawInRect:withAttributes:", (void *)str_draw_in);
    cx_add_method(&NSString_c, "sizeWithAttributes:", (void *)str_size);
    DEF(NSAttributedString, NSObject);
    M(NSAttributedString, "initWithString:attributes:", as_init); M(NSAttributedString, "initWithString:", as_init1);
    M(NSAttributedString, "string", as_string); M(NSAttributedString, "drawAtPoint:", as_draw_at); M(NSAttributedString, "size", as_size);

    DEF(NSGraphicsContext, NSObject);
    CM(NSGraphicsContext, "currentContext", gc_current); CM(NSGraphicsContext, "saveGraphicsState", gc_save); CM(NSGraphicsContext, "restoreGraphicsState", gc_restore);
    M(NSGraphicsContext, "CGContext", gc_cg); M(NSGraphicsContext, "graphicsPort", gc_cg); M(NSGraphicsContext, "saveGraphicsState", gc_save);
    M(NSGraphicsContext, "restoreGraphicsState", gc_restore); M(NSGraphicsContext, "setShouldAntialias:", noop1_m);

    DEF(NSEvent, NSObject);
    CM(NSEvent, "modifierFlags", ev_cmods);
    M(NSEvent, "type", ev_type); M(NSEvent, "locationInWindow", ev_loc); M(NSEvent, "modifierFlags", ev_mods); M(NSEvent, "keyCode", ev_keycode);
    M(NSEvent, "characters", ev_chars); M(NSEvent, "charactersIgnoringModifiers", ev_chars); M(NSEvent, "clickCount", ev_clicks);
    M(NSEvent, "window", ev_window); M(NSEvent, "scrollingDeltaY", ev_dy); M(NSEvent, "deltaY", ev_dy); M(NSEvent, "deltaX", ev_zero);
    M(NSEvent, "scrollingDeltaX", ev_zero); M(NSEvent, "isARepeat", no_m);

    DEF(NSMenu, NSObject);
    M(NSMenu, "initWithTitle:", mn_init_title); M(NSMenu, "addItem:", mn_add); M(NSMenu, "itemArray", mn_items); M(NSMenu, "items", mn_items);
    M(NSMenu, "addItemWithTitle:action:keyEquivalent:", mn_add_titled); M(NSMenu, "setAutoenablesItems:", noop1_m); M(NSMenu, "removeAllItems", mn_remove_all);
    M(NSMenu, "numberOfItems", mn_count); M(NSMenu, "itemAtIndex:", mn_item_at); M(NSMenu, "removeItemAtIndex:", mn_remove_at);
    M(NSMenu, "removeItem:", mn_remove); M(NSMenu, "insertItem:atIndex:", mn_insert_at); M(NSMenu, "indexOfItem:", mn_index_of);
    M(NSMenu, "itemWithTag:", mn_item_tag); M(NSMenu, "indexOfItemWithTag:", mn_index_tag); M(NSMenu, "itemWithTitle:", mn_item_title);
    M(NSMenu, "title", mn_title_get); M(NSMenu, "setTitle:", mn_init_title);
    DEF(NSMenuItem, NSObject);
    CM(NSMenuItem, "separatorItem", mi_sep);
    M(NSMenuItem, "initWithTitle:action:keyEquivalent:", mi_init); M(NSMenuItem, "setSubmenu:", mi_set_submenu); M(NSMenuItem, "submenu", mi_submenu);
    M(NSMenuItem, "setTarget:", mi_set_target); M(NSMenuItem, "title", mi_title); M(NSMenuItem, "setKeyEquivalentModifierMask:", mi_set_mask);
    M(NSMenuItem, "setAction:", mi_set_action); M(NSMenuItem, "setTitle:", mi_set_title); M(NSMenuItem, "setKeyEquivalent:", mi_set_key); M(NSMenuItem, "tag", mi_tag); M(NSMenuItem, "state", mi_state);
    M(NSMenuItem, "setTag:", mi_set_tag); M(NSMenuItem, "setState:", mi_set_state); M(NSMenuItem, "setEnabled:", mi_set_enabled);
    M(NSMenuItem, "isSeparatorItem", mi_is_sep); M(NSMenuItem, "setHidden:", mi_set_hidden); M(NSMenuItem, "isHidden", mi_hidden);
    M(NSMenuItem, "setAlternate:", mi_set_alternate); M(NSMenuItem, "isAlternate", mi_alternate);
    M(NSMenuItem, "setImage:", mi_set_image); M(NSMenuItem, "image", mi_image);
    M(NSMenuItem, "setRepresentedObject:", mi_set_repr); M(NSMenuItem, "representedObject", mi_repr);
    M(NSMenuItem, "isSectionHeader", mi_is_header); M(NSMenuItem, "isEnabled", mi_enabled); M(NSMenuItem, "keyEquivalent", mi_key);
    M(NSMenuItem, "action", mi_action); M(NSMenuItem, "target", mi_target); M(NSMenuItem, "hasSubmenu", mi_has_submenu);
    CM(NSMenuItem, "sectionHeaderWithTitle:", mi_section_header);

    DEF(NSAlert, NSObject);
    M(NSAlert, "setMessageText:", al_set_msg); M(NSAlert, "setInformativeText:", al_set_info); M(NSAlert, "addButtonWithTitle:", al_add_button);
    M(NSAlert, "setAlertStyle:", al_set_style); M(NSAlert, "runModal", al_run);
    cx_define_class(&AlertTarget_c, &AlertTarget_m, "_CXAlertTarget", &NSObject_c, 0);
    cx_add_method(&AlertTarget_c, "cxAlertButton:", (void *)al_button_hit);

    DEF(NSScreen, NSObject);
    CM(NSScreen, "mainScreen", sc_main); CM(NSScreen, "screens", sc_screens);
    M(NSScreen, "frame", sc_frame); M(NSScreen, "visibleFrame", sc_visible); M(NSScreen, "backingScaleFactor", sc_scale);
    DEF(NSCursor, NSObject);
    CM(NSCursor, "arrowCursor", generic_singleton); CM(NSCursor, "pointingHandCursor", generic_singleton); CM(NSCursor, "IBeamCursor", generic_singleton);
    M(NSCursor, "set", noop_m); M(NSCursor, "push", noop_m); M(NSCursor, "pop", noop_m);
    DEF(NSWorkspace, NSObject);
    CM(NSWorkspace, "sharedWorkspace", generic_singleton); M(NSWorkspace, "openURL:", ws_open_url);
    DEF(NSImage, NSObject);
    CM(NSImage, "imageNamed:", im_named); CM(NSImage, "imageWithSystemSymbolName:accessibilityDescription:", noop_ret_nil);
    M(NSImage, "initWithContentsOfFile:", im_init_file); M(NSImage, "initWithData:", im_init_data); M(NSImage, "initWithSize:", im_init_size);
    M(NSImage, "size", im_size); M(NSImage, "setSize:", im_set_size); M(NSImage, "isValid", im_valid); M(NSImage, "drawInRect:", im_draw_in);
    M(NSImage, "drawInRect:fromRect:operation:fraction:", im_draw_in_frac); M(NSImage, "drawAtPoint:fromRect:operation:fraction:", im_draw_at);
    M(NSImage, "setTemplate:", noop1_m); M(NSImage, "lockFocus", noop_m); M(NSImage, "unlockFocus", noop_m);
    DEF(NSRunningApplication, NSObject);
    CM(NSRunningApplication, "currentApplication", generic_singleton); M(NSRunningApplication, "activateWithOptions:", yes_m);
    DEF(NSSound, NSObject);
    DEF(NSLayoutConstraint, NSObject);
    CM(NSLayoutConstraint, "activateConstraints:", lc_activate_all); CM(NSLayoutConstraint, "deactivateConstraints:", lc_deactivate_all);
    CM(NSLayoutConstraint, "constraintWithItem:attribute:relatedBy:toItem:attribute:multiplier:constant:", lc_with_item);
    CM(NSLayoutConstraint, "constraintsWithVisualFormat:options:metrics:views:", vfl_parse);
    M(NSLayoutConstraint, "setActive:", lc_set_active); M(NSLayoutConstraint, "isActive", lc_is_active); M(NSLayoutConstraint, "active", lc_is_active);
    M(NSLayoutConstraint, "constant", lc_const); M(NSLayoutConstraint, "setConstant:", lc_set_const); M(NSLayoutConstraint, "priority", lc_prio);
    M(NSLayoutConstraint, "setPriority:", lc_set_prio); M(NSLayoutConstraint, "multiplier", lc_mult); M(NSLayoutConstraint, "firstItem", lc_first);
    M(NSLayoutConstraint, "secondItem", lc_second); M(NSLayoutConstraint, "firstAttribute", lc_first_attr); M(NSLayoutConstraint, "secondAttribute", lc_second_attr);
    M(NSLayoutConstraint, "relation", lc_relation); M(NSLayoutConstraint, "setIdentifier:", lc_set_ident); M(NSLayoutConstraint, "identifier", lc_ident);
    M(NSLayoutConstraint, "dealloc", lc_dealloc);
    DEF(NSLayoutAnchor, NSObject); DEF(NSLayoutXAxisAnchor, NSLayoutAnchor); DEF(NSLayoutYAxisAnchor, NSLayoutAnchor); DEF(NSLayoutDimension, NSLayoutAnchor);
    M(NSLayoutAnchor, "constraintEqualToAnchor:", an_eq); M(NSLayoutAnchor, "constraintEqualToAnchor:constant:", an_eq_k);
    M(NSLayoutAnchor, "constraintGreaterThanOrEqualToAnchor:", an_ge); M(NSLayoutAnchor, "constraintGreaterThanOrEqualToAnchor:constant:", an_ge_k);
    M(NSLayoutAnchor, "constraintLessThanOrEqualToAnchor:", an_le); M(NSLayoutAnchor, "constraintLessThanOrEqualToAnchor:constant:", an_le_k);
    M(NSLayoutXAxisAnchor, "constraintEqualToSystemSpacingAfterAnchor:multiplier:", an_eq_sys);
    M(NSLayoutXAxisAnchor, "constraintGreaterThanOrEqualToSystemSpacingAfterAnchor:multiplier:", an_ge_sys);
    M(NSLayoutYAxisAnchor, "constraintEqualToSystemSpacingBelowAnchor:multiplier:", an_eq_sys);
    M(NSLayoutYAxisAnchor, "constraintGreaterThanOrEqualToSystemSpacingBelowAnchor:multiplier:", an_ge_sys);
    M(NSLayoutDimension, "constraintEqualToConstant:", an_eq_const); M(NSLayoutDimension, "constraintGreaterThanOrEqualToConstant:", an_ge_const);
    M(NSLayoutDimension, "constraintLessThanOrEqualToConstant:", an_le_const); M(NSLayoutDimension, "constraintEqualToAnchor:multiplier:", an_eq_m);
    M(NSLayoutDimension, "constraintEqualToAnchor:multiplier:constant:", an_eq_mk); M(NSLayoutDimension, "constraintGreaterThanOrEqualToAnchor:multiplier:", an_ge_m);
    M(NSLayoutDimension, "constraintGreaterThanOrEqualToAnchor:multiplier:constant:", an_ge_mk); M(NSLayoutDimension, "constraintLessThanOrEqualToAnchor:multiplier:", an_le_m);
    M(NSLayoutDimension, "constraintLessThanOrEqualToAnchor:multiplier:constant:", an_le_mk);
    M(NSView, "leadingAnchor", v_leading_a); M(NSView, "trailingAnchor", v_trailing_a); M(NSView, "leftAnchor", v_left_a); M(NSView, "rightAnchor", v_right_a);
    M(NSView, "topAnchor", v_top_a); M(NSView, "bottomAnchor", v_bottom_a); M(NSView, "widthAnchor", v_width_a); M(NSView, "heightAnchor", v_height_a);
    M(NSView, "centerXAnchor", v_cx_a); M(NSView, "centerYAnchor", v_cy_a); M(NSView, "lastBaselineAnchor", v_lastbase_a); M(NSView, "firstBaselineAnchor", v_firstbase_a);
    M(NSView, "addConstraint:", v_add_constraint); M(NSView, "addConstraints:", v_add_constraints); M(NSView, "removeConstraint:", v_remove_constraint);
    M(NSView, "removeConstraints:", v_remove_constraints); M(NSView, "constraints", v_constraints); M(NSView, "layoutSubtreeIfNeeded", v_layout_now);
    M(NSView, "layout", v_layout_now); M(NSView, "setNeedsLayout:", v_set_needs); M(NSView, "setNeedsUpdateConstraints:", v_set_needs);
    M(NSView, "updateConstraints", noop_m); M(NSView, "updateConstraintsForSubtreeIfNeeded", noop_m);
    DEF(NSLayoutGuide, NSView);
    M(NSLayoutGuide, "owningView", lg_owning); M(NSLayoutGuide, "layoutFrame", lg_frame); M(NSLayoutGuide, "frame", lg_frame);
    M(NSView, "addLayoutGuide:", v_add_guide); M(NSView, "removeLayoutGuide:", v_remove_guide);
    M(NSView, "safeAreaLayoutGuide", v_safe_area); M(NSView, "layoutMarginsGuide", v_margins);
    M(NSView, "setContentHuggingPriority:forOrientation:", noop1_m); M(NSView, "setContentCompressionResistancePriority:forOrientation:", noop1_m);
    M(NSWindow, "layoutIfNeeded", noop_m);
    nib_register();
    bindings_register();
    cx_appkit_input_init();     /* appkit_input.c: interpretKeyEvents:, NSTextInputContext, arrastar e soltar */
    (void)run_loop_until;
    cx_runloop_hook = run_loop_until;
}

/* ================================================================ primitivas da ponte de Path
 * (userland/libsystem/swiftbridge_path.c). Só acréscimos: nada acima muda. Coordenadas em pixels da
 * janela, exceto cx_gfx_view_xform, que dá a conversão da view corrente (a do drawRect: em andamento). */
void cx_gfx_view_xform(double *ox, double *oy, double *sy) {
    *ox = ctx.tlx;
    *oy = ctx.flipped ? ctx.tly : ctx.tly + ctx.h;
    *sy = ctx.flipped ? 1 : -1;
}
void cx_gfx_view_rect_px(double *x, double *y, double *w, double *h) {
    /* retângulo da view intersectado com o recorte dos ancestrais (o mesmo que set_clip aplicaria) */
    double x0 = ctx.tlx > clip_x0 ? ctx.tlx : clip_x0, y0 = ctx.tly > clip_y0 ? ctx.tly : clip_y0;
    double x1 = ctx.tlx + ctx.w < clip_x1 ? ctx.tlx + ctx.w : clip_x1, y1 = ctx.tly + ctx.h < clip_y1 ? ctx.tly + ctx.h : clip_y1;
    *x = x0; *y = y0; *w = x1 > x0 ? x1 - x0 : 0; *h = y1 > y0 ? y1 - y0 : 0;
}
void cx_gfx_clip_px(double x, double y, double w, double h) { set_clip(x, y, w, h); }
void cx_gfx_poly_px(const float *xy, u32 n, u32 argb) { g_poly(xy, n, argb, 1, 0); }
void cx_gfx_rrect_px(double x, double y, double w, double h, double r, u32 argb) { g_rrect(x, y, w, h, r, argb); }
/* imagem ARGB não pré-multiplicada, composta com alfa na posição (x, y) */
void cx_gfx_image_px(int x, int y, int w, int h, const u32 *px) {
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096) return;
    usize n = 16 + (usize)w * h * 4;
    u8 *b = malloc(n);
    if (!b) return;
    i32 r[4] = {x, y, w, h};
    memcpy(b, r, 16);
    memcpy(b + 16, px, (usize)w * h * 4);
    put(GC_IMAGE, b, (u32)n);
    free(b);
}

/* CGEventPost (slack_frameworks.c): uma tecla sintética entra na janela-chave deste app como se viesse do window server */
void cx_post_key_event(u16 mac, u64 flags, int down) {
    int key = -1;
    for (int k = 1; k < 0x110 && key < 0; k++) if (mac_keycode(k) == mac) key = k;
    id w = key_window_obj();
    if (key < 0 || !w) return;
    gui_event_t g;
    memset(&g, 0, sizeof g);
    g.type = down ? GEV_KEY_DOWN : GEV_KEY_UP;
    g.win = (u32)ws(w)->gid;
    g.mods = ((flags >> 17) & 1 ? 1 : 0) | ((flags >> 18) & 1 ? 2 : 0) | ((flags >> 19) & 1 ? 4 : 0) | ((flags >> 20) & 1 ? 8 : 0);
    if ((g.mods & 1) && key >= 'a' && key <= 'z') key -= 32;
    g.key = key;
    if (key >= 32 && key < 127 && !(g.mods & (2 | 8))) g.text[0] = (char)key;
    dispatch(&g);
}
