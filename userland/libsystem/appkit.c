/*
 * mOS libSystem — AppKit (subconjunto) sobre a ponte gráfica do kernel (compat_gui.h).
 *
 * Cada NSWindow vira uma janela do window server do mOS; cada quadro é um lote de comandos de
 * desenho (retângulos, polígonos, texto com a fonte do sistema) enviado com GUI_DRAW. As views
 * usam as coordenadas do Cocoa (origem embaixo à esquerda, salvo isFlipped) e o laço de eventos
 * entrega mouseDown:/mouseUp:/mouseDragged:/keyDown:/scrollWheel: como no macOS.
 */
#include "foundation.h"
#include "progress_draw.h"
#include "libc.h"
#include "appkit_table.h"
#include "appkit_status.h"
#include "../../kernel/include/compat_gui.h"

void cx_ax_note_view(id obj, const char *note);

double floor(double), cos(double), sin(double), strtod(const char *, char **);
char *strcpy(char *, const char *);
int isalnum(int);
void qsort(void *, size_t, size_t, int (*)(const void *, const void *));

typedef struct { double x, y; } NSPoint;
typedef struct { double w, h; } NSSize;
typedef struct { NSPoint o; NSSize s; } NSRect;

#define CLASS(N) cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N); cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N);
extern cx_class_t NSAttributedString_c __asm__("OBJC_CLASS_$_NSAttributedString");   /* fdn_astr.c */
CLASS(NSResponder) CLASS(NSApplication) CLASS(NSWindow) CLASS(NSView) CLASS(NSControl) CLASS(NSButton) CLASS(NSScroller)
CLASS(NSTextField) CLASS(NSSlider) CLASS(NSProgressIndicator) CLASS(NSBox) CLASS(NSStackView) CLASS(NSImageView)
CLASS(NSColor) CLASS(NSFont) CLASS(NSBezierPath) CLASS(NSEvent) CLASS(NSMenu) CLASS(NSMenuItem) CLASS(NSAlert)
CLASS(NSScreen) CLASS(NSGraphicsContext) CLASS(NSCursor) CLASS(NSWorkspace) CLASS(NSImage) 
CLASS(NSRunningApplication) CLASS(NSSound) CLASS(NSPanel) CLASS(NSScrollView) CLASS(NSTextView) CLASS(NSClipView)
CLASS(NSWindowTab)
CLASS(NSVisualEffectView) CLASS(NSLayoutConstraint) CLASS(NSColorWell) CLASS(NSTableView) CLASS(NSTableColumn) CLASS(NSTableCellView)

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
void objc_setAssociatedObject(id obj, const void *key, id value, u64 policy);
id objc_getAssociatedObject(id obj, const void *key);
extern const char cx_tv_header_key;   /* NSTableHeaderView → a tabela (associação ASSIGN) */
extern void *objc_autoreleasePoolPush(void);
extern void objc_autoreleasePoolPop(void *);
/* guarda um objeto num campo, retendo o novo e soltando o antigo */
static id keep(id *slot, id v) { objc_retain(v); id old = *slot; *slot = v; objc_release(old); return v; }
static BOOL menus_dirty;
static void table_resize(id t, double width);
static void layout_scroll(id v);
static void tv_layout_views(id self);
static void tv_install_archived(id table, id arch);
static void set_clip(double x, double y, double w, double h);
struct absr;
static void draw_image_fit(id img, double x, double y, double w, double h);
static void draw_image_ctl(id img, double x, double y, double w, double h, int scaling, u32 tint);
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
    double scroll_x;               /* NSScrollView: deslocamento horizontal do documento */
    id identifier;                 /* NSUserInterfaceItemIdentifier (retido) */
    id next_responder;             /* nextResponder explícito; senão a superview (fraco) */
    id header_view;                /* NSTableHeaderView real, quando o nib traz um (retido) */
    int tv_dirty;                  /* NSTableView: o delegate precisa ser perguntado de novo */
    BOOL show_idle;                /* NSProgressIndicator: displayedWhenStopped */
    float hug_h, hug_v, cr_h, cr_v; /* Auto Layout: 0 = padrão (hugging 250, resistência 750) */
    BOOL pg_indet, pg_anim;         /* NSProgressIndicator: indeterminada, animando */
    int pg_style;                   /* 0 barra, 1 giratório */
    id pg_timer;                    /* quadros da animação (NSTimer, retido) */
    int st_align, st_dist;          /* NSStackView: alignment (NSLayoutAttribute) e distribution; st_align_set = 1 se o app pôs */
    BOOL st_align_set;
    double st_ins[4];               /* NSStackView edgeInsets: top, left, bottom, right */
    int img_scaling;                /* NSImageView imageScaling (0 = proporcional para baixo) */
    /* NSScrollView: barras pedidas (o *_set diz se o app escolheu; sem escolha, a barra aparece quando o documento passa
     * da área), autohidesScrollers, borderType (0 nenhuma, 1 linha, 2 bezel, 3 sulco) e fundo */
    BOOL sv_vset, sv_v, sv_hset, sv_h, sv_autohide;
    int sv_border;
    id tint;                        /* contentTintColor (NSButton, NSImageView), retido */
    float valpha;                   /* alphaValue da view (vale com valpha_set) */
    BOOL valpha_set;
    /* NSTextField: lineBreakMode + 1 (0 = o padrão do campo), maximumNumberOfLines (0 = sem limite), usesSingleLineMode,
     * preferredMaxLayoutWidth (0 = nenhuma) */
    int tf_lbm1;
    i64 tf_max_lines;
    BOOL tf_single;
    double tf_pref_w;
    int cont;                       /* NSControl continuous: 0 padrão (o slider é contínuo), 1 sim, 2 não */
} vstate_t;

typedef struct {
    int gid;
    NSRect frame;                   /* tela, origem embaixo à esquerda (contentRect) */
    id content, title, delegate, first, bg, mouse_view;
    u64 style;
    int frameless;                  /* 0 = não sei, 1 = moldura normal, 2 = conteúdo = moldura (w_frameless) */
    BOOL visible, released_when_closed, closed, needs, is_modal_panel;
    int level;
    id cvc;                         /* contentViewController */
    double last_render;             /* cx_now() do último render_window (teto de quadros do pump) */
    double alpha;                   /* alphaValue (0 = nunca definido = 1,0) */
    id parent;                      /* parentWindow (fraco) */
    i64 number;                     /* windowNumber: único e estável pela vida da janela (0 = ainda não pedido) */
    id subtitle, represented_url, tab;
    BOOL restorable;
    id next_responder;              /* nextResponder explícito; senão o NSApp */
    id tb_host;                     /* view que hospeda os itens da NSToolbar (fraco: a toolbar retém) */
    double tb_top;                  /* faixa da toolbar acima do conteúdo (0 sem toolbar ou com fullSizeContentView) */
    BOOL title_hidden;              /* titleVisibility = NSWindowTitleHidden: o window server mostra a barra sem texto */
    BOOL titlebar_transparent, movable_bg, accepts_moved, no_shadow, not_opaque, initial_done;
    u64 collection;                 /* collectionBehavior */
    id initial_fr;                  /* initialFirstResponder (fraco, como no Cocoa) */
    id autosave;                    /* frameAutosaveName (retido) */
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
    double galpha;                  /* CGContextSetAlpha: multiplica o alfa das cores de preenchimento e traço */
    int blend;                      /* CGContextSetBlendMode no contexto da janela; 16 = clear */
} ctx_t;
static ctx_t ctx = {0, 0, 0, 0, NO, 0xFF000000, 0xFF000000, 1, 1, 0};
void cx_cg_note_blend(int mode) { ctx.blend = mode; }
/* aplica o alfa global (CGContextSetAlpha) a uma cor ARGB */
static u32 ga(u32 argb) { if (ctx.galpha >= 1) return argb; u32 a = (u32)((argb >> 24) * (ctx.galpha > 0 ? ctx.galpha : 0) + 0.5); return (a << 24) | (argb & 0xFFFFFF); }
u32 cx_cg_fill_argb(void) { return ctx.fill; }
static ctx_t ctx_stack[16];
static int ctx_sp;

static void cl_set(id self, SEL c) { (void)c; ctx.fill = ctx.stroke = argb(self); }
static void cl_set_fill(id self, SEL c) { (void)c; ctx.fill = argb(self); }
static void cl_set_stroke(id self, SEL c) { (void)c; ctx.stroke = argb(self); }

int cx_mono_face(void);   /* fontmono.c: a JetBrains Mono registrada no kernel (o "Menlo" do mOS), ou -1 */
static id ft_make(double size, int weight, int mono) {
    id f = cx_alloc_instance(&NSFont_c);
    fnt(f)->size = size > 0 ? size : 13;
    /* monoespaçada: a face da JetBrains Mono quando ela existe (antes era a Inter só marcada "mono", e o terminal do
     * VS Code espalhava as letras pela grade medida no "W") */
    int m = mono ? cx_mono_face() : -1;
    fnt(f)->weight = m > 0 ? m : weight;
    fnt(f)->mono = mono;
    return f;
}
static int weight_of(double w) { return w >= 0.35 ? 3 : w >= 0.27 ? 2 : w >= 0.2 ? 1 : 0; }
static id ft_system(id cls, SEL c, double s) { (void)cls; (void)c; return ft_make(s, 0, 0); }
static id ft_bold(id cls, SEL c, double s) { (void)cls; (void)c; return ft_make(s, 3, 0); }
static id ft_system_w(id cls, SEL c, double s, double w) { (void)cls; (void)c; return ft_make(s, weight_of(w), 0); }
static id ft_mono(id cls, SEL c, double s, double w) { (void)cls; (void)c; return ft_make(s, weight_of(w), 1); }
static id ft_named(id cls, SEL c, id name, double s) { (void)cls; (void)c; const char *n = cx_cstr(name); return ft_make(s, strstr(n, "Bold") ? 3 : strstr(n, "Semibold") ? 2 : strstr(n, "Medium") ? 1 : 0, !!strstr(n, "Mono") || !!strstr(n, "Courier") || !!strstr(n, "Menlo") || !!strstr(n, "Monaco") || !!strstr(n, "Consolas")); }
static double ft_size(id self, SEL c) { (void)c; return fnt(self)->size; }
static double ft_sys_size(id cls, SEL c) { (void)cls; (void)c; return 13; }
static double ft_small_size(id cls, SEL c) { (void)cls; (void)c; return 11; }
static id ft_name(id self, SEL c) {
    (void)c;
    int w = fnt(self)->weight;
    if (w >= 32) return cx_str(fnt(self)->mono ? "JetBrainsMono-Regular" : ".AppleSystemUIFont");   /* face registrada (mono ou fonte web) */
    return cx_str(w >= 3 ? ".AppleSystemUIFontBold" : ".AppleSystemUIFont");
}

/* ================================================================ lote de comandos */
static u8 *cmd;
static u32 ncmd, capcmd;
/* alphaValue das views em desenho (produto dos ancestrais): o lote não tem opacidade de grupo, então cada cor e cada
 * pixel dos comandos sai com o alfa multiplicado */
static double g_view_alpha = 1;
static void alpha_u32(u8 *p, double a) { u32 c; memcpy(&c, p, 4); c = ((u32)((c >> 24) * a + 0.5) << 24) | (c & 0xFFFFFF); memcpy(p, &c, 4); }
static void put(u32 op, const void *payload, u32 n) {
    u32 len = 8 + n;
    if (ncmd + len > capcmd) { capcmd = (ncmd + len) * 2 + 4096; cmd = realloc(cmd, capcmd); }
    memcpy(cmd + ncmd, &op, 4);
    memcpy(cmd + ncmd + 4, &len, 4);
    if (n) memcpy(cmd + ncmd + 8, payload, n);
    if (g_view_alpha < 0.999 && n) {
        u8 *b = cmd + ncmd + 8;
        double a = g_view_alpha;
        switch (op) {
        case GC_FILL: case GC_RRECT: case GC_LINE: if (n >= 24) alpha_u32(b + 20, a); break;
        case GC_RRECT_STROKE: if (n >= 28) alpha_u32(b + 24, a); break;
        case GC_POLY: case GC_POLYLINE: if (n >= 8) alpha_u32(b + 4, a); break;
        case GC_TEXT: case GC_COMP_FILL: if (n >= 20) alpha_u32(b + 16, a); break;
        case GC_GRAD: if (n >= 24) { alpha_u32(b + 16, a); alpha_u32(b + 20, a); } break;
        case GC_IMAGE: for (u32 o = 16; o + 4 <= n; o += 4) alpha_u32(b + o, a); break;
        case GC_COMP_IMAGE: for (u32 o = 20; o + 4 <= n; o += 4) alpha_u32(b + o, a); break;
        }
    }
    ncmd += len;
}
static int g_offscreen;   /* > 0: o lote está sendo gravado para um CGContext (cx_gc_record_*); nada vai à janela */
static void flush_to(int gid) {
    if (g_offscreen) return;
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

/* ================================================================ NSBezierPath
 * Elementos de verdade (bezier_core.c, teste userland/tests-appkit/bezier_host.c): o preenchimento vira um polígono
 * só (o window server tem a regra não-zero; o par-ímpar é convertido), o traço passa pelo tracejado e ganha pontas e
 * junções redondas/quadradas. addClip/setClip recortam pela caixa do caminho (o recorte do window server é retangular). */
#include "bezier_core.h"
#include "textfit_core.h"
void *bz_alloc(unsigned long n) { return malloc(n); }
void *bz_realloc(void *p, unsigned long n) { return realloc(p, n); }
void bz_free(void *p) { free(p); }
typedef struct { bz_path e; double lw, miter, flat, phase; int cap, join, rule, ndash; double *dash; } path_t;
static double g_bp_lw = 1, g_bp_miter = 10, g_bp_flat = 0.6;
static int g_bp_cap, g_bp_join, g_bp_rule;
static path_t *pth(id p) {
    if (!ST(p)) {
        path_t *s = calloc(1, sizeof *s);
        s->lw = g_bp_lw; s->miter = g_bp_miter; s->flat = g_bp_flat; s->cap = g_bp_cap; s->join = g_bp_join; s->rule = g_bp_rule;
        ST(p) = s;
    }
    return ST(p);
}
static bz_rect bzr(NSRect r) { bz_rect b = {r.o.x, r.o.y, r.s.w, r.s.h}; return b; }
static void bp_move(id self, SEL c, NSPoint pt) { (void)c; bz_move(&pth(self)->e, pt.x, pt.y); }
static void bp_line(id self, SEL c, NSPoint pt) { (void)c; bz_line(&pth(self)->e, pt.x, pt.y); }
static void bp_curve(id self, SEL c, NSPoint e, NSPoint c1, NSPoint c2) { (void)c; bz_curve(&pth(self)->e, c1.x, c1.y, c2.x, c2.y, e.x, e.y); }
static void bp_close(id self, SEL c) { (void)c; bz_close(&pth(self)->e); }
static void add_rect(path_t *p, NSRect r) { bz_rect_path(&p->e, bzr(r)); }
static void add_oval(path_t *p, NSRect r) { bz_oval(&p->e, bzr(r)); }
static void add_rrect(path_t *p, NSRect r, double rx, double ry) { bz_rrect(&p->e, bzr(r), rx, ry); }
static id bp_new(void) { return MSG0(id, MSG0(id, &NSBezierPath_c, "alloc"), "init"); }
static id bp_rect(id cls, SEL c, NSRect r) { (void)cls; (void)c; id o = bp_new(); add_rect(pth(o), r); return o; }
static id bp_oval(id cls, SEL c, NSRect r) { (void)cls; (void)c; id o = bp_new(); add_oval(pth(o), r); return o; }
static id bp_rrect(id cls, SEL c, NSRect r, double rx, double ry) { (void)cls; (void)c; id o = bp_new(); add_rrect(pth(o), r, rx, ry); return o; }
static void bp_app_rect(id self, SEL c, NSRect r) { (void)c; add_rect(pth(self), r); }
static void bp_app_oval(id self, SEL c, NSRect r) { (void)c; add_oval(pth(self), r); }
static void bp_app_rrect(id self, SEL c, NSRect r, double rx, double ry) { (void)c; add_rrect(pth(self), r, rx, ry); }
static void bp_set_lw(id self, SEL c, double w) { (void)c; pth(self)->lw = w; }
static double bp_lw(id self, SEL c) { (void)c; return pth(self)->lw; }
static void bp_remove_all(id self, SEL c) { (void)c; bz_clear(&pth(self)->e); }
/* traço: cada pedaço (depois do tracejado) vira polilinha; pontas e junções redondas são círculos, quadradas estendem */
typedef struct { path_t *p; u32 color; } stroke_ctx;
static void stroke_piece(void *cv, const double *xy, int n) {
    stroke_ctx *sc = cv;
    double lw = sc->p->lw > 0 ? sc->p->lw : 1;
    if (n < 2) return;
    float *f = malloc((size_t)n * 8);
    if (!f) return;
    for (int i = 0; i < n; i++) { f[i * 2] = (float)px_x(xy[i * 2]); f[i * 2 + 1] = (float)px_y(xy[i * 2 + 1]); }
    if (sc->p->cap == 2) {   /* NSLineCapStyleSquare: estende meia largura nas pontas */
        for (int end = 0; end < 2; end++) {
            int a = end ? n - 1 : 0, b = end ? n - 2 : 1;
            double dx = f[a * 2] - f[b * 2], dy = f[a * 2 + 1] - f[b * 2 + 1], l = __builtin_sqrt(dx * dx + dy * dy);
            if (l > 0) { f[a * 2] += (float)(dx / l * lw / 2); f[a * 2 + 1] += (float)(dy / l * lw / 2); }
        }
    }
    g_poly(f, (u32)n, sc->color, 0, lw);
    if (lw > 1.5) {
        for (int i = 0; i < n; i++) {
            int endpt = i == 0 || i == n - 1;
            if ((endpt && sc->p->cap == 1) || (!endpt && sc->p->join == 1) || (!endpt && sc->p->join == 0 && lw > 2))
                g_rrect(f[i * 2] - lw / 2, f[i * 2 + 1] - lw / 2, lw, lw, lw / 2, sc->color);   /* cobre a fresta da junção */
        }
    }
    free(f);
}
static void draw_path(path_t *p, int fill) {
    bz_flat fl = {0, 0, 0, 0, 0, 0, 0};
    double tol = p->flat > 0 ? p->flat / 2 : 0.3;
    if (bz_flatten(&p->e, tol, &fl) <= 0) { bz_flat_release(&fl); return; }
    if (fill) {
        float *o = 0;
        int n = bz_fill_polygon(&fl, p->rule == 1, &o);
        while (n > 4096 && tol < 64) {   /* o window server aceita até 4096 pontos por polígono */
            free(o); o = 0; tol *= 2;
            if (bz_flatten(&p->e, tol, &fl) <= 0) break;
            n = bz_fill_polygon(&fl, p->rule == 1, &o);
        }
        if (n >= 3 && n <= 4096) {
            for (int i = 0; i < n; i++) { o[i * 2] = (float)px_x(o[i * 2]); o[i * 2 + 1] = (float)px_y(o[i * 2 + 1]); }
            g_poly(o, (u32)n, ctx.fill, 1, 0);
        }
        free(o);
    } else {
        stroke_ctx sc = {p, ctx.stroke};
        bz_dash(&fl, p->dash, p->ndash, p->phase, stroke_piece, &sc);
    }
    bz_flat_release(&fl);
}
static void bp_fill(id self, SEL c) { (void)c; draw_path(pth(self), 1); }
static void bp_stroke(id self, SEL c) { (void)c; draw_path(pth(self), 0); }
static void bp_cfill_rect(id cls, SEL c, NSRect r) { (void)cls; (void)c; double x, y, w, h; px_rect(r, &x, &y, &w, &h); g_fill(x, y, w, h, ctx.fill); }
static void bp_cstroke_rect(id cls, SEL c, NSRect r) { (void)cls; (void)c; id p = bp_rect(0, 0, r); bp_stroke(p, 0); }
static void bp_cstroke_line(id cls, SEL c, NSPoint a, NSPoint b) { (void)cls; (void)c; g_line(px_x(a.x), px_y(a.y), px_x(b.x), px_y(b.y), g_bp_lw, ctx.stroke); }
static NSRect bp_bounds(id self, SEL c) { (void)c; bz_rect b = bz_bounds(&pth(self)->e); NSRect r = {{b.x, b.y}, {b.w, b.h}}; return r; }
static void bp_add_clip(id self, SEL c) { NSRect r = bp_bounds(self, c); double x, y, w, h; px_rect(r, &x, &y, &w, &h); g_clip(x, y, w, h); }

/* resto do NSBezierPath: elementos, relativos, arcos, pontos, estilo de traço, regra, contenção, transformação */
static NSPoint bp_cur(id self) { NSPoint p = {0, 0}; bz_current(&pth(self)->e, &p.x, &p.y); return p; }
static NSPoint bp_current_point(id self, SEL c) { (void)c; return bp_cur(self); }
static BOOL bp_is_empty(id self, SEL c) { (void)c; return pth(self)->e.n == 0; }
static void bp_rmove(id self, SEL c, NSPoint d) { (void)c; NSPoint p = bp_cur(self); bz_move(&pth(self)->e, p.x + d.x, p.y + d.y); }
static void bp_rline(id self, SEL c, NSPoint d) { (void)c; NSPoint p = bp_cur(self); bz_line(&pth(self)->e, p.x + d.x, p.y + d.y); }
static void bp_rcurve(id self, SEL c, NSPoint e, NSPoint c1, NSPoint c2) {
    (void)c; NSPoint p = bp_cur(self);
    bz_curve(&pth(self)->e, p.x + c1.x, p.y + c1.y, p.x + c2.x, p.y + c2.y, p.x + e.x, p.y + e.y);
}
static void bp_append(id self, SEL c, id other) { (void)c; if (other) bz_append(&pth(self)->e, &pth(other)->e); }
static void bp_append_points(id self, SEL c, const NSPoint *pts, i64 n) {
    (void)c;
    if (!pts || n <= 0) return;
    path_t *p = pth(self);
    if (!p->e.n) bz_move(&p->e, pts[0].x, pts[0].y); else bz_line(&p->e, pts[0].x, pts[0].y);
    for (i64 i = 1; i < n; i++) bz_line(&p->e, pts[i].x, pts[i].y);
}
static void bp_arc_cw(id self, SEL c, NSPoint ct, double r, double a0, double a1, BOOL cw) { (void)c; bz_arc(&pth(self)->e, ct.x, ct.y, r, a0, a1, cw); }
static void bp_arc(id self, SEL c, NSPoint ct, double r, double a0, double a1) { bp_arc_cw(self, c, ct, r, a0, a1, NO); }
static void bp_arc_from(id self, SEL c, NSPoint p1, NSPoint p2, double r) { (void)c; bz_arc_to(&pth(self)->e, p1.x, p1.y, p2.x, p2.y, r); }
static i64 bp_count(id self, SEL c) { (void)c; return pth(self)->e.n; }
/* NSBezierPathElement: move 0, line 1, curve 2 (cubicCurveTo), close 3 */
static i64 bp_element_pts(id self, SEL c, i64 i, NSPoint *pts) {
    (void)c;
    path_t *p = pth(self);
    if (i < 0 || i >= p->e.n) return 0;
    const bz_el *e = &p->e.el[i];
    int np = e->kind == BZ_CURVE ? 3 : e->kind == BZ_CLOSE ? 0 : 1;
    for (int k = 0; pts && k < np; k++) { pts[k].x = e->p[k * 2]; pts[k].y = e->p[k * 2 + 1]; }
    return e->kind;
}
static i64 bp_element(id self, SEL c, i64 i) { return bp_element_pts(self, c, i, 0); }
static void bp_set_assoc(id self, SEL c, const NSPoint *pts, i64 i) {
    (void)c;
    path_t *p = pth(self);
    if (!pts || i < 0 || i >= p->e.n) return;
    bz_el *e = &p->e.el[i];
    int np = e->kind == BZ_CURVE ? 3 : e->kind == BZ_CLOSE ? 0 : 1;
    for (int k = 0; k < np; k++) { e->p[k * 2] = pts[k].x; e->p[k * 2 + 1] = pts[k].y; }
}
static NSRect bp_cbounds(id self, SEL c) { (void)c; bz_rect b = bz_control_bounds(&pth(self)->e); NSRect r = {{b.x, b.y}, {b.w, b.h}}; return r; }
static BOOL bp_contains(id self, SEL c, NSPoint pt) { (void)c; return bz_contains(&pth(self)->e, pt.x, pt.y, pth(self)->rule == 1); }
static i64 bp_cap(id self, SEL c) { (void)c; return pth(self)->cap; }
static void bp_set_cap(id self, SEL c, i64 v) { (void)c; pth(self)->cap = (int)v; }
static i64 bp_join(id self, SEL c) { (void)c; return pth(self)->join; }
static void bp_set_join(id self, SEL c, i64 v) { (void)c; pth(self)->join = (int)v; }
static i64 bp_rule(id self, SEL c) { (void)c; return pth(self)->rule; }
static void bp_set_rule(id self, SEL c, i64 v) { (void)c; pth(self)->rule = (int)v; }
static double bp_miter(id self, SEL c) { (void)c; return pth(self)->miter; }
static void bp_set_miter(id self, SEL c, double v) { (void)c; pth(self)->miter = v; }
static double bp_flat(id self, SEL c) { (void)c; return pth(self)->flat; }
static void bp_set_flat(id self, SEL c, double v) { (void)c; pth(self)->flat = v > 0 ? v : 0.6; }
static void bp_set_dash(id self, SEL c, const double *pat, i64 n, double phase) {
    (void)c;
    path_t *p = pth(self);
    free(p->dash); p->dash = 0; p->ndash = 0; p->phase = phase;
    if (pat && n > 0) { p->dash = malloc((size_t)n * sizeof(double)); if (p->dash) { memcpy(p->dash, pat, (size_t)n * sizeof(double)); p->ndash = (int)n; } }
}
static void bp_get_dash(id self, SEL c, double *pat, i64 *n, double *phase) {
    (void)c;
    path_t *p = pth(self);
    if (pat) memcpy(pat, p->dash, (size_t)p->ndash * sizeof(double));
    if (n) *n = p->ndash;
    if (phase) *phase = p->phase;
}
static void bp_copy_style(path_t *d, const path_t *s) {
    d->lw = s->lw; d->miter = s->miter; d->flat = s->flat; d->cap = s->cap; d->join = s->join; d->rule = s->rule; d->phase = s->phase;
    d->ndash = 0; d->dash = 0;
    if (s->ndash) { d->dash = malloc((size_t)s->ndash * sizeof(double)); if (d->dash) { memcpy(d->dash, s->dash, (size_t)s->ndash * sizeof(double)); d->ndash = s->ndash; } }
}
static id bp_copy(id self, SEL c, void *zone) {
    (void)c; (void)zone;
    id o = bp_new();
    path_t *d = pth(o), *sp = pth(self);
    bz_copy(&d->e, &sp->e);
    bp_copy_style(d, sp);
    return o;
}
static id bp_reversed(id self, SEL c) {
    (void)c;
    id o = bp_new();
    bz_reverse(&pth(o)->e, &pth(self)->e);
    bp_copy_style(pth(o), pth(self));
    return objc_autorelease(o);
}
static id bp_flattened(id self, SEL c) {
    (void)c;
    id o = bp_new();
    path_t *sp = pth(self), *d = pth(o);
    bz_flat fl = {0, 0, 0, 0, 0, 0, 0};
    if (bz_flatten(&sp->e, sp->flat > 0 ? sp->flat : 0.6, &fl) > 0)
        for (int r = 0; r < fl.nr; r++) {
            int a = fl.start[r], b = r + 1 < fl.nr ? fl.start[r + 1] : fl.n;
            for (int i = a; i < b; i++) { if (i == a) bz_move(&d->e, fl.xy[i * 2], fl.xy[i * 2 + 1]); else bz_line(&d->e, fl.xy[i * 2], fl.xy[i * 2 + 1]); }
            if (fl.closed[r]) bz_close(&d->e);
        }
    bz_flat_release(&fl);
    bp_copy_style(d, sp);
    return objc_autorelease(o);
}
/* matriz de um NSAffineTransform pelos pontos transformados (vale para qualquer implementação dele) */
static void at_matrix(id t, double m[6]) {
    NSPoint o = {0, 0}, x = {1, 0}, y = {0, 1};
    NSPoint to = ((NSPoint (*)(id, SEL, NSPoint))objc_msgSend)(t, sel_registerName("transformPoint:"), o);
    NSPoint tx = ((NSPoint (*)(id, SEL, NSPoint))objc_msgSend)(t, sel_registerName("transformPoint:"), x);
    NSPoint ty = ((NSPoint (*)(id, SEL, NSPoint))objc_msgSend)(t, sel_registerName("transformPoint:"), y);
    m[0] = tx.x - to.x; m[1] = tx.y - to.y; m[2] = ty.x - to.x; m[3] = ty.y - to.y; m[4] = to.x; m[5] = to.y;
}
static void bp_transform(id self, SEL c, id t) { (void)c; if (!t) return; double m[6]; at_matrix(t, m); bz_transform(&pth(self)->e, m); }
static id at_transform_path(id self, SEL c, id path) {
    (void)c;
    if (!path) return 0;
    id o = bp_copy(path, 0, 0);
    bp_transform(o, 0, self);
    return objc_autorelease(o);
}
/* CGPath (macOS 14): ida e volta com o CGPath do Core Graphics */
typedef struct { i32 type; NSPoint *points; } bp_cgel;
void *CGPathCreateMutable(void);
void CGPathMoveToPoint(void *p, const void *m, double x, double y);
void CGPathAddLineToPoint(void *p, const void *m, double x, double y);
void CGPathAddCurveToPoint(void *p, const void *m, double c1x, double c1y, double c2x, double c2y, double x, double y);
void CGPathAddQuadCurveToPoint(void *p, const void *m, double cx, double cy, double x, double y);
void CGPathCloseSubpath(void *p);
void CGPathApply(const void *path, void *info, void (*fn)(void *, const bp_cgel *));
static char k_bp_cg;
static void *bp_cgpath(id self, SEL c) {
    (void)c;
    void *cg = CGPathCreateMutable();
    path_t *p = pth(self);
    for (int i = 0; cg && i < p->e.n; i++) {
        const bz_el *e = &p->e.el[i];
        switch (e->kind) {
        case BZ_MOVE: CGPathMoveToPoint(cg, 0, e->p[0], e->p[1]); break;
        case BZ_LINE: CGPathAddLineToPoint(cg, 0, e->p[0], e->p[1]); break;
        case BZ_CURVE: CGPathAddCurveToPoint(cg, 0, e->p[0], e->p[1], e->p[2], e->p[3], e->p[4], e->p[5]); break;
        default: CGPathCloseSubpath(cg); break;
        }
    }
    if (cg) objc_setAssociatedObject(self, &k_bp_cg, (id)cg, 1);   /* o caminho fica com o objeto, como a propriedade */
    return cg;
}
static void cg_el_add(void *info, const bp_cgel *e) {
    bz_path *p = info;
    double x, y;
    switch (e->type) {
    case 0: bz_move(p, e->points[0].x, e->points[0].y); break;
    case 1: bz_line(p, e->points[0].x, e->points[0].y); break;
    case 2:   /* quadrática → cúbica */
        if (!bz_current(p, &x, &y)) { x = e->points[0].x; y = e->points[0].y; }
        bz_curve(p, x + 2.0 / 3 * (e->points[0].x - x), y + 2.0 / 3 * (e->points[0].y - y),
                 e->points[1].x + 2.0 / 3 * (e->points[0].x - e->points[1].x), e->points[1].y + 2.0 / 3 * (e->points[0].y - e->points[1].y),
                 e->points[1].x, e->points[1].y);
        break;
    case 3: bz_curve(p, e->points[0].x, e->points[0].y, e->points[1].x, e->points[1].y, e->points[2].x, e->points[2].y); break;
    default: bz_close(p); break;
    }
}
static void bp_set_cgpath(id self, SEL c, void *cg) { (void)c; bz_clear(&pth(self)->e); if (cg) CGPathApply(cg, &pth(self)->e, cg_el_add); }
static id bp_with_cgpath(id cls, SEL c, void *cg) { (void)cls; (void)c; id o = bp_new(); bp_set_cgpath(o, 0, cg); return objc_autorelease(o); }
static void bp_set_clip(id self, SEL c) { (void)c; g_unclip(); bp_add_clip(self, 0); }
static void bp_clip_rect(id cls, SEL c, NSRect r) { (void)cls; (void)c; double x, y, w, h; px_rect(r, &x, &y, &w, &h); g_clip(x, y, w, h); }
static void bp_set_def_lw(id k, SEL c, double v) { (void)k; (void)c; g_bp_lw = v; }
static double bp_def_lw(id k, SEL c) { (void)k; (void)c; return g_bp_lw; }
static void bp_set_def_cap(id k, SEL c, i64 v) { (void)k; (void)c; g_bp_cap = (int)v; }
static i64 bp_def_cap(id k, SEL c) { (void)k; (void)c; return g_bp_cap; }
static void bp_set_def_join(id k, SEL c, i64 v) { (void)k; (void)c; g_bp_join = (int)v; }
static i64 bp_def_join(id k, SEL c) { (void)k; (void)c; return g_bp_join; }
static void bp_set_def_rule(id k, SEL c, i64 v) { (void)k; (void)c; g_bp_rule = (int)v; }
static i64 bp_def_rule(id k, SEL c) { (void)k; (void)c; return g_bp_rule; }
static void bp_set_def_miter(id k, SEL c, double v) { (void)k; (void)c; g_bp_miter = v; }
static double bp_def_miter(id k, SEL c) { (void)k; (void)c; return g_bp_miter; }
static void bp_set_def_flat(id k, SEL c, double v) { (void)k; (void)c; g_bp_flat = v > 0 ? v : 0.6; }
static double bp_def_flat(id k, SEL c) { (void)k; (void)c; return g_bp_flat; }

/* funções C de desenho do AppKit */
void NSRectFill(NSRect r) { double x, y, w, h; px_rect(r, &x, &y, &w, &h); g_fill(x, y, w, h, ctx.fill); }
static void g_comp_fill(double x, double y, double w, double h, u32 c, u32 op) {
    float p[4] = {(float)x, (float)y, (float)w, (float)h};
    u8 b[24];
    if (op > 28) op = 2; /* NSCompositingOperationSourceOver */
    memcpy(b, p, 16);
    memcpy(b + 16, &c, 4);
    memcpy(b + 20, &op, 4);
    put(GC_COMP_FILL, b, 24);
}
void NSRectFillUsingOperation(NSRect r, u64 op) {
    double x, y, w, h;
    px_rect(r, &x, &y, &w, &h);
    g_comp_fill(x, y, w, h, ctx.fill, (u32)op);
}
void NSEraseRect(NSRect r) { double x, y, w, h; px_rect(r, &x, &y, &w, &h); g_fill(x, y, w, h, 0xFFFFFFFF); }
void NSFrameRectWithWidth(NSRect r, double t) { double x, y, w, h; px_rect(r, &x, &y, &w, &h); g_rstroke(x + t / 2, y + t / 2, w - t, h - t, 0, t, ctx.fill); }
void NSFrameRect(NSRect r) { NSFrameRectWithWidth(r, 1); }
void NSRectFillList(const NSRect *r, i64 n) { for (i64 i = 0; i < n; i++) NSRectFill(r[i]); }
void NSRectFillListUsingOperation(const NSRect *r, i64 n, u64 op) { for (i64 i = 0; i < n; i++) NSRectFillUsingOperation(r[i], op); }
/* uma cor por retângulo (o NSColor vira a cor de preenchimento corrente, como -set) */
void NSRectFillListWithColorsUsingOperation(const NSRect *r, const id *colors, i64 n, u64 op) {
    u32 keep_fill = ctx.fill;
    for (i64 i = 0; i < n; i++) { if (colors && colors[i]) ctx.fill = argb(colors[i]); NSRectFillUsingOperation(r[i], op); }
    ctx.fill = keep_fill;
}
void NSRectFillListWithColors(const NSRect *r, const id *colors, i64 n) { NSRectFillListWithColorsUsingOperation(r, colors, n, 2 /* sourceOver */); }
/* moldura com operação: as quatro faixas, cada uma com a operação pedida */
void NSFrameRectWithWidthUsingOperation(NSRect r, double t, u64 op) {
    if (t <= 0) return;
    NSRect b[4] = {{{r.o.x, r.o.y}, {r.s.w, t}}, {{r.o.x, r.o.y + r.s.h - t}, {r.s.w, t}},
                   {{r.o.x, r.o.y + t}, {t, r.s.h - 2 * t}}, {{r.o.x + r.s.w - t, r.o.y + t}, {t, r.s.h - 2 * t}}};
    for (int i = 0; i < 4; i++) if (b[i].s.w > 0 && b[i].s.h > 0) NSRectFillUsingOperation(b[i], op);
}
/* recorte: interseção com o recorte vigente (NSRectClip); a lista usa o retângulo que cobre todos
 * (o window server recorta por um retângulo só) */
void NSRectClip(NSRect r) {
    double x, y, w, h;
    px_rect(r, &x, &y, &w, &h);
    if (last_clip_on) {
        double x0 = x > last_clip[0] ? x : last_clip[0], y0 = y > last_clip[1] ? y : last_clip[1];
        double x1 = x + w < last_clip[0] + last_clip[2] ? x + w : last_clip[0] + last_clip[2];
        double y1 = y + h < last_clip[1] + last_clip[3] ? y + h : last_clip[1] + last_clip[3];
        x = x0; y = y0; w = x1 > x0 ? x1 - x0 : 0; h = y1 > y0 ? y1 - y0 : 0;
    }
    g_clip(x, y, w, h);
}
void NSRectClipList(const NSRect *r, i64 n) {
    if (n <= 0) return;
    double x0 = r[0].o.x, y0 = r[0].o.y, x1 = x0 + r[0].s.w, y1 = y0 + r[0].s.h;
    for (i64 i = 1; i < n; i++) {
        if (r[i].o.x < x0) x0 = r[i].o.x;
        if (r[i].o.y < y0) y0 = r[i].o.y;
        if (r[i].o.x + r[i].s.w > x1) x1 = r[i].o.x + r[i].s.w;
        if (r[i].o.y + r[i].s.h > y1) y1 = r[i].o.y + r[i].s.h;
    }
    NSRectClip((NSRect){{x0, y0}, {x1 - x0, y1 - y0}});
}
void NSBeep(void) { extern void cx_beep(void); cx_beep(); }   /* coreaudio.c: o alerta do sistema no mixer */

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
typedef struct { double size; int weight; } tfm_ctx;
static double tfm_measure(const char *s, int n, void *ctx) { tfm_ctx *t = ctx; return measure(s, (u32)n, t->size, t->weight).width; }
/* texto quebrado em linhas (TFIT_WORD ou TFIT_CHAR), no máximo maxl linhas (0 = até a altura acabar) */
static void draw_wrapped_mode(const char *s, double x, double y, double w, double h, double size, int weight, u32 color, int align, int mode, i64 maxl) {
    gui_metrics_t m0 = measure("Ag", 2, size, weight);
    double lh = m0.ascent + m0.descent + 2, base = y + m0.ascent;
    tfm_ctx tc = {size, weight};
    const char *p = s;
    i64 line = 0;
    while (*p && base - m0.ascent < y + h && (!maxl || line < maxl)) {
        int skip, n = tfit_break(p, w, mode, tfm_measure, &tc, &skip);
        double tw = measure(p, (u32)n, size, weight).width, tx = x;
        if (align == 2) tx = x + (w - tw) / 2;
        else if (align == 1) tx = x + w - tw;
        if (n) g_text(tx, base, size, weight, color, p, (u32)n);
        p += n + skip;
        if (!n && !skip) p++;
        base += lh;
        line++;
    }
}
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

/* NSAttributedString é do Foundation (fdn_astr.c); aqui só o desenho, pelos atributos do primeiro caractere */
static id as_attrs0(id self) { id s = MSG0(id, self, "string"); return s && MSG0(u64, s, "length") ? MSG(id, self, "attributesAtIndex:effectiveRange:", (u64)0, (void *)0) : 0; }
static void as_draw_at(id self, SEL c, NSPoint pt) { (void)c; draw_str_at(cx_cstr(MSG0(id, self, "string")), pt, as_attrs0(self)); }
static NSSize as_size(id self, SEL c) { return str_size(MSG0(id, self, "string"), c, as_attrs0(self)); }

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
/* coregraphics_bm.inc: o CGBitmapContext tem path, traço e estado próprios; 1 = era bitmap e já tratou */
int cx_bm_is(const void *c);
int cx_bm_move(const void *c, double x, double y);
int cx_bm_line(const void *c, double x, double y);
int cx_bm_curve(const void *c, double c1x, double c1y, double c2x, double c2y, double x, double y);
int cx_bm_close(const void *c);
int cx_bm_begin(const void *c);
int cx_bm_rect(const void *c, double x, double y, double w, double h);
int cx_bm_ellipse(const void *c, double x, double y, double w, double h);
int cx_bm_fill(const void *c, int eo);
int cx_bm_stroke(const void *c);
int cx_bm_draw(const void *c, int mode);
int cx_bm_set_stroke(const void *c, u32 argb);
int cx_bm_set_line_width(const void *c, double w);
int cx_bm_set_cap(const void *c, int v);
int cx_bm_set_join(const void *c, int v);
int cx_bm_set_alpha(const void *c, double a);
int cx_bm_set_aa(const void *c, int v);
/* path da janela para o coregraphics_bm.inc: ponto atual e recorte pela caixa do path */
int cx_win_current(double *x, double *y) { return bz_current(&pth(cgp())->e, x, y); }
void cx_win_clip(void) { if (pth(cgp())->e.n) bp_add_clip(cgp(), 0); bp_remove_all(cgp(), 0); }
void CGContextSetRGBFillColor(id c, double r, double g, double b, double a) {
    if (cx_bm_is(c)) { cx_bm_set_fill(c, argb(cx_color(r, g, b, a))); return; }
    ctx.fill = ga(argb(cx_color(r, g, b, a)));
    cx_bm_set_fill(c, ctx.fill);
}
void CGContextSetRGBStrokeColor(id c, double r, double g, double b, double a) { if (cx_bm_set_stroke(c, argb(cx_color(r, g, b, a)))) return; ctx.stroke = ga(argb(cx_color(r, g, b, a))); }
void CGContextSetGrayFillColor(id c, double g, double a) { CGContextSetRGBFillColor(c, g, g, g, a); }
void CGContextSetGrayStrokeColor(id c, double g, double a) { CGContextSetRGBStrokeColor(c, g, g, g, a); }
int cx_bm_pattern_color(const void *c, const void *color, int stroke);   /* coregraphics: cor de padrão */
void CGContextSetFillColorWithColor(id c, id color) { if (cx_bm_pattern_color(c, color, 0)) return; if (cx_bm_is(c)) { cx_bm_set_fill(c, argb(color)); return; } ctx.fill = ga(argb(color)); }
/* alfa global do contexto: vale para as cores definidas depois e para a cor de preenchimento atual */
void CGContextSetAlpha(id c, double a) {
    if (cx_bm_set_alpha(c, a)) return;
    double old = ctx.galpha > 0 ? ctx.galpha : 1;
    if (a < 0) a = 0;
    if (a > 1) a = 1;
    ctx.galpha = a;
    u32 fa = (u32)((ctx.fill >> 24) / old * a + 0.5); if (fa > 255) fa = 255;
    ctx.fill = (fa << 24) | (ctx.fill & 0xFFFFFF);
    cx_bm_set_fill(c, ctx.fill);
}
void CGContextSetStrokeColorWithColor(id c, id color) { if (cx_bm_pattern_color(c, color, 1)) return; if (cx_bm_set_stroke(c, argb(color))) return; ctx.stroke = ga(argb(color)); }
void CGContextSetLineWidth(id c, double w) { if (cx_bm_set_line_width(c, w)) return; ctx.line_width = w; pth(cgp())->lw = w; }
void CGContextFillRect(id c, CGRect r) {
    if (cx_bm_fill_rect(c, r.o.x, r.o.y, r.s.w, r.s.h)) return;
    if (ctx.blend == 16) { double x, y, w, h; px_rect(r, &x, &y, &w, &h); g_fill(x, y, w, h, 0); return; }
    NSRectFill(r);
}
void CGContextFillRects(id c, const CGRect *r, u64 n) { for (u64 i = 0; i < n; i++) CGContextFillRect(c, r[i]); }
void CGContextClearRect(id c, CGRect r) {
    if (cx_bm_clear_rect(c, r.o.x, r.o.y, r.s.w, r.s.h)) return;
    double x, y, w, h; px_rect(r, &x, &y, &w, &h); g_fill(x, y, w, h, 0xFFFFFFFF);
}
void CGContextStrokeRect(id c, CGRect r) {
    if (cx_bm_is(c)) { cx_bm_begin(c); cx_bm_rect(c, r.o.x, r.o.y, r.s.w, r.s.h); cx_bm_stroke(c); return; }
    id p = bp_rect(0, 0, r); pth(p)->lw = ctx.line_width; bp_stroke(p, 0); }
void CGContextStrokeRectWithWidth(id c, CGRect r, double w) {
    if (cx_bm_is(c)) { cx_bm_save(c); cx_bm_set_line_width(c, w); CGContextStrokeRect(c, r); cx_bm_restore(c); return; }
    id p = bp_rect(0, 0, r); pth(p)->lw = w; bp_stroke(p, 0); }
void CGContextFillEllipseInRect(id c, CGRect r) {
    if (cx_bm_is(c)) { cx_bm_begin(c); cx_bm_ellipse(c, r.o.x, r.o.y, r.s.w, r.s.h); cx_bm_fill(c, 0); return; }
    bp_fill(bp_oval(0, 0, r), 0);
}
void CGContextStrokeEllipseInRect(id c, CGRect r) {
    if (cx_bm_is(c)) { cx_bm_begin(c); cx_bm_ellipse(c, r.o.x, r.o.y, r.s.w, r.s.h); cx_bm_stroke(c); return; }
    id p = bp_oval(0, 0, r); pth(p)->lw = ctx.line_width; bp_stroke(p, 0); }
void CGContextBeginPath(id c) { if (cx_bm_begin(c)) return; bp_remove_all(cgp(), 0); }
void CGContextMoveToPoint(id c, double x, double y) { if (cx_bm_move(c, x, y)) return; NSPoint p = {x, y}; bp_move(cgp(), 0, p); }
void CGContextAddLineToPoint(id c, double x, double y) { if (cx_bm_line(c, x, y)) return; NSPoint p = {x, y}; bp_line(cgp(), 0, p); }
void CGContextAddCurveToPoint(id c, double c1x, double c1y, double c2x, double c2y, double x, double y) {
    if (cx_bm_curve(c, c1x, c1y, c2x, c2y, x, y)) return;
    NSPoint e = {x, y}, a = {c1x, c1y}, b = {c2x, c2y};
    bp_curve(cgp(), 0, e, a, b);
}
void CGContextAddRect(id c, CGRect r) { if (cx_bm_rect(c, r.o.x, r.o.y, r.s.w, r.s.h)) return; add_rect(pth(cgp()), r); }
void CGContextAddEllipseInRect(id c, CGRect r) { if (cx_bm_ellipse(c, r.o.x, r.o.y, r.s.w, r.s.h)) return; add_oval(pth(cgp()), r); }
void CGContextClosePath(id c) { if (cx_bm_close(c)) return; bp_close(cgp(), 0); }
void CGContextFillPath(id c) { if (cx_bm_fill(c, 0)) return; bp_fill(cgp(), 0); bp_remove_all(cgp(), 0); }
void CGContextStrokePath(id c) { if (cx_bm_stroke(c)) return; pth(cgp())->lw = ctx.line_width; bp_stroke(cgp(), 0); bp_remove_all(cgp(), 0); }
void CGContextDrawPath(id c, int mode) {
    if (cx_bm_draw(c, mode)) return;
    if (mode == 0 || mode == 1 || mode == 3 || mode == 4) bp_fill(cgp(), 0);
    if (mode >= 2) bp_stroke(cgp(), 0);
    bp_remove_all(cgp(), 0);
}
void CGContextSaveGState(id c) { if (!cx_bm_save(c)) gc_save(c, 0); }
void CGContextRestoreGState(id c) { if (!cx_bm_restore(c)) gc_restore(c, 0); }
void CGContextSetLineCap(id c, int v) { cx_bm_set_cap(c, v); }
void CGContextSetLineJoin(id c, int v) { cx_bm_set_join(c, v); }
void CGContextSetShouldAntialias(id c, int v) { cx_bm_set_aa(c, v); }
id CGColorCreateGenericRGB(double r, double g, double b, double a) { return cx_color(r, g, b, a); }
/* CGColorCreateGenericGray: coregraphics (cg_color.inc), com o espaço cinza e 2 componentes */
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
/* Como no Cocoa: setFrame: manda setFrameOrigin: e setFrameSize: por mensagem, e setFrameSize: reposiciona as subviews
 * (autoresizingMask) mandando setFrame: a cada uma. As subclasses observam por aí: a RenderWidgetHostViewCocoa do
 * Chromium sobrescreve setFrameSize: para avisar o renderer do tamanho; antes ele ficava com viewport vazio. */
static void v_set_size(id self, SEL c, NSSize sz) {
    (void)c;
    vstate_t *s = vs(self);
    NSSize old = s->frame.s;
    s->frame.s = sz;
    if (old.w != sz.w || old.h != sz.h) resize_children(self, old);
    mark(self);
}
static void v_set_origin(id self, SEL c, NSPoint p) { (void)c; vs(self)->frame.o = p; mark(self); }
static BOOL cr_view(id v);
static void v_set_frame(id self, SEL c, NSRect f) {
    (void)c;
    vstate_t *s = vs(self);
    if (cr_view(self) && (s->frame.o.x != f.o.x || s->frame.o.y != f.o.y || s->frame.s.w != f.s.w || s->frame.s.h != f.s.h))
        cx_fdprintf(2, "[ca] setFrame: %s %p (%.0f,%.0f %.0fx%.0f)\n", cx_class_name(object_getClass(self)), (void *)self, f.o.x, f.o.y, f.s.w, f.s.h);
    if (s->frame.o.x != f.o.x || s->frame.o.y != f.o.y)
        ((void (*)(id, SEL, NSPoint))objc_msgSend)(self, sel_registerName("setFrameOrigin:"), f.o);
    if (s->frame.s.w != f.s.w || s->frame.s.h != f.s.h)
        ((void (*)(id, SEL, NSSize))objc_msgSend)(self, sel_registerName("setFrameSize:"), f.s);
    mark(self);
}
/* Como no Cocoa, quem troca de janela (entra, sai com nil, muda) recebe viewWillMoveToWindow: antes e viewDidMoveToWindow
 * depois, em TODA a subárvore. A RenderWidgetHostViewCocoa do Chromium fica dentro da WebContentsViewCocoa: ao voltar
 * para uma aba só a de fora era avisada, e a página continuava "fora da janela" (oculta, conteúdo em branco). */
static void set_window_quiet(id v, id w) {
    vstate_t *s = vs(v);
    s->window = w;
    for (u32 i = 0; i < s->nsub; i++) set_window_quiet(s->sub[i], w);
}
static void set_window_rec(id v, id w) {
    vstate_t *s = vs(v);
    BOOL changed = s->window != w;
    if (changed && cx_responds(v->isa, sel_registerName("viewWillMoveToWindow:"))) MSG(void, v, "viewWillMoveToWindow:", w);
    s->window = w;
    for (u32 i = 0; i < s->nsub; i++) set_window_rec(s->sub[i], w);
    if (changed && cx_responds(v->isa, sel_registerName("viewDidMoveToWindow"))) MSG0(void, v, "viewDidMoveToWindow");
}
/* at é o índice depois de tirar sub do pai antigo. A última subview é a de cima (o hit-test anda de trás pra frente). */
/* diagnóstico (ca-log): movimentos das views do Chromium (WebContentsViewCocoa, RenderWidgetHostViewCocoa...) */
static BOOL cr_view(id v) {
    extern int cx_ca_diag(void);
    if (!v || !cx_ca_diag()) return NO;
    const char *n = cx_class_name(object_getClass(v));
    return n && (strstr(n, "WebContents") || strstr(n, "RenderWidget"));
}
static void v_insert_sub(id self, id sub, u32 at) {
    if (!sub) return;
    vstate_t *s = vs(self), *ss = vs(sub);
    if (cr_view(sub))
        cx_fdprintf(2, "[ca] addSubview: %s %p <- %s %p na posição %u de %u (antes em %s %p)\n", cx_class_name(object_getClass(self)), (void *)self,
                    cx_class_name(object_getClass(sub)), (void *)sub, at, s->nsub,
                    ss->superview ? cx_class_name(object_getClass(ss->superview)) : "-", (void *)ss->superview);
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
    BOOL same = ss->window == s->window;
    set_window_rec(sub, s->window);
    /* mesma janela: o set_window_rec não avisa; a view inserida ainda recebe viewDidMoveToWindow, como antes */
    if (same && s->window && cx_responds(sub->isa, sel_registerName("viewDidMoveToWindow"))) MSG0(void, sub, "viewDidMoveToWindow");
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
    if (cr_view(self)) cx_fdprintf(2, "[ca] removeFromSuperview: %s %p de %s %p\n", cx_class_name(object_getClass(self)), (void *)self,
                                   cx_class_name(object_getClass(s->superview)), (void *)s->superview);
    vstate_t *ps = vs(s->superview);
    for (u32 i = 0; i < ps->nsub; i++) if (ps->sub[i] == self) { memmove(ps->sub + i, ps->sub + i + 1, (ps->nsub - i - 1) * sizeof(id)); ps->nsub--; break; }
    mark(s->superview);
    s->superview = 0;
    set_window_rec(self, 0);
    objc_release(self);
}
/* Como no Cocoa: a view que morre solta as subviews (superview = nil). Sem isto os filhos guardavam o ponteiro do
 * pai já liberado, e o removeFromSuperview seguinte (Chrome, views::View) lia o estado de outro objeto no lugar. */
static void v_dealloc(id self, SEL c) {
    vstate_t *s = ST(self);
    if (s) {
        for (u32 i = 0; i < s->nsub; i++) {
            id sub = s->sub[i];
            vstate_t *ss = ST(sub);
            if (ss && ss->superview == self) { ss->superview = 0; set_window_quiet(sub, 0); }
            objc_release(sub);
        }
        s->nsub = 0;
        free(s->sub);
        s->sub = 0;
        s->capsub = 0;
        s->superview = 0;
        if (s->header_view) objc_setAssociatedObject(s->header_view, &cx_tv_header_key, 0, 0);
        objc_release(s->identifier);
        objc_release(s->header_view);
    }
    ((void (*)(id, SEL))cx_lookup(&NSResponder_c, c))(self, c);
}
static id v_subviews(id self, SEL c) { (void)c; vstate_t *s = vs(self); id a = cx_arr_new(&NSMutableArray_c); for (u32 i = 0; i < s->nsub; i++) cx_arr_add(a, s->sub[i]); return a; }
static id v_superview(id self, SEL c) { (void)c; return vs(self)->superview; }
static id v_window(id self, SEL c) { (void)c; return vs(self)->window; }
static void v_set_needs(id self, SEL c, BOOL b) { (void)c; if (b) mark(self); }
static void v_set_needs_rect(id self, SEL c, NSRect r) { (void)c; (void)r; mark(self); }
static void v_display(id self, SEL c) { (void)c; mark(self); }
static BOOL v_needs(id self, SEL c) { (void)c; return vs(self)->needs; }
/* Como no Cocoa: setHidden: avisa viewDidHide / viewDidUnhide à view e às descendentes que não estão ocultas por conta
 * própria. A WebContentsViewCocoa do Chromium recalcula a visibilidade da página aí: ao voltar para uma aba, a página
 * ficava "oculta" e em branco. */
static void notify_hidden(id v, BOOL h, BOOL top) {
    vstate_t *s = vs(v);
    if (!top && s->hidden) return;
    SEL sel = sel_registerName(h ? "viewDidHide" : "viewDidUnhide");
    if (cx_responds(v->isa, sel)) ((void (*)(id, SEL))objc_msgSend)(v, sel);
    for (u32 i = 0; i < s->nsub; i++) notify_hidden(s->sub[i], h, NO);
}
static BOOL cr_view(id v);
static void v_set_hidden(id self, SEL c, BOOL h) {
    (void)c;
    vstate_t *s = vs(self);
    BOOL was = s->hidden;
    if (!was != !h && cr_view(self)) cx_fdprintf(2, "[ca] setHidden:%d %s %p\n", (int)h, cx_class_name(object_getClass(self)), (void *)self);
    s->hidden = h;
    mark(s->superview ? s->superview : self);
    if (!was != !h) notify_hidden(self, h, YES);
}
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
        ((void (*)(id, SEL, NSRect))objc_msgSend)(s->sub[i], sel_registerName("setFrame:"), f);
    }
}

/* conversão de pontos (só fromView:nil / toView:nil, a partir da janela) */
typedef struct { double tlx, tly, w, h; BOOL flipped; } absr_t;
static BOOL is_flipped(id v) { return MSG0(BOOL, v, "isFlipped"); }
/* -[NSView hitTest:]: p nas coordenadas da superview (para a contentView, as da janela); devolve a descendente mais
 * funda que contém o ponto, ou nil. O Chromium só aceita um evento de mouse na RenderWidgetHostViewCocoa se o hitTest:
 * da contentView devolve ela ou uma descendente (shouldIgnoreMouseEvent:); sem este método todo clique e todo movimento
 * eram ignorados (ia um kMouseLeave e mais nada). As subviews respondem por mensagem: o Chromium sobrescreve hitTest:. */
static id v_hit_test(id self, SEL c, NSPoint p) {
    vstate_t *s = vs(self);
    if (s->hidden) return 0;
    id sup = s->superview;
    NSPoint q = {p.x - s->frame.o.x, p.y - s->frame.o.y};
    if ((sup ? is_flipped(sup) : NO) != is_flipped(self)) q.y = s->frame.s.h - q.y;
    if (q.x < 0 || q.y < 0 || q.x >= s->frame.s.w || q.y >= s->frame.s.h) return 0;
    for (u32 i = s->nsub; i-- > 0;) {
        id r = ((id (*)(id, SEL, NSPoint))objc_msgSend)(s->sub[i], c, q);
        if (r) return r;
    }
    return self;
}
static BOOL abs_rect(id v, absr_t *out) {
    vstate_t *s = vs(v);
    if (!s->window) return NO;
    id content = ws(s->window)->content;
    if (v == content) { out->tlx = 0; out->tly = ws(s->window)->tb_top; out->w = s->frame.s.w; out->h = s->frame.s.h; out->flipped = is_flipped(v); return YES; }
    if (v == ws(s->window)->tb_host) { out->tlx = 0; out->tly = 0; out->w = s->frame.s.w; out->h = s->frame.s.h; out->flipped = is_flipped(v); return YES; }
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
/* Geometria que a árvore de acessibilidade entrega. O frame da janela é o do AppKit
 * (origem no canto inferior esquerdo da tela). A view sai no mesmo espaço. */
int cx_appkit_window_frame(id w, double *x, double *y, double *wd, double *ht) {
    if (!w || !x || !y || !wd || !ht) return 0;
    wstate_t *s = ws(w);
    *x = s->frame.o.x;
    *y = s->frame.o.y;
    *wd = s->frame.s.w;
    *ht = s->frame.s.h;
    return 1;
}
int cx_appkit_window_number(id w) { return w ? ws(w)->gid : 0; }
int cx_appkit_view_frame(id v, double *x, double *y, double *wd, double *ht) {
    absr_t a;
    if (!v || !x || !y || !wd || !ht || !abs_rect(v, &a)) return 0;
    wstate_t *s = ws(vs(v)->window);
    *x = s->frame.o.x + a.tlx;
    *y = s->frame.o.y + (s->frame.s.h - a.tly - a.h);
    *wd = a.w;
    *ht = a.h;
    return 1;
}
static double win_h(id w) { wstate_t *s = ws(w); return s->frame.s.h; }
/* conversões entre views passam pela base da janela (from/to nil = a janela, como no AppKit) */
static NSPoint v_convert_to(id self, SEL c, NSPoint p, id to);
static NSPoint v_convert_from(id self, SEL c, NSPoint p, id from) {
    absr_t a;
    if (from == self) return p;
    if (from) p = v_convert_to(from, c, p, 0);    /* da outra view para a janela */
    if (!abs_rect(self, &a)) return p;
    double wy = win_h(vs(self)->window) - p.y;   /* janela (Cocoa) -> pixels */
    NSPoint r = {p.x - a.tlx, a.flipped ? wy - a.tly : a.tly + a.h - wy};
    return r;
}
static NSPoint v_convert_to(id self, SEL c, NSPoint p, id to) {
    absr_t a;
    if (to == self) return p;
    if (!abs_rect(self, &a)) return p;
    double py = a.flipped ? a.tly + p.y : a.tly + a.h - p.y;
    NSPoint r = {a.tlx + p.x, win_h(vs(self)->window) - py};
    return to ? v_convert_from(to, c, r, 0) : r;
}
/* os dois cantos: entre uma view invertida (isFlipped) e outra não, a origem do retângulo troca de canto */
static NSRect v_convert_rect_to(id self, SEL c, NSRect r, id to) {
    NSPoint a = v_convert_to(self, c, r.o, to), b = v_convert_to(self, c, (NSPoint){r.o.x + r.s.w, r.o.y + r.s.h}, to);
    r.o.x = a.x < b.x ? a.x : b.x;
    r.o.y = a.y < b.y ? a.y : b.y;
    return r;
}
/* -convertRect:fromView: (nil = a janela). O Chromium posiciona a NSView de uma WebContentsView filha
 * (NativeViewHostMac::ShowWidget / WebContentsNSViewBridge::SetBounds) com [[superview] convertRect:fromView:nil]. */
static NSRect v_convert_rect_from(id self, SEL c, NSRect r, id from) {
    NSPoint a = v_convert_from(self, c, r.o, from), b = v_convert_from(self, c, (NSPoint){r.o.x + r.s.w, r.o.y + r.s.h}, from);
    r.o.x = a.x < b.x ? a.x : b.x;
    r.o.y = a.y < b.y ? a.y : b.y;
    return r;
}

/* ================================================================ controles */
static id dflt_font(void) { static id f; if (!f) f = ft_make(13, 0, 0); return f; }
static u32 accent = 0xFF007AFF;   /* cor de destaque do sistema (draw_builtin a lê do tema a cada desenho) */
static BOOL g_dark_draw;          /* aparência de desenho escura (draw_builtin) */
BOOL cx_appearance_dark(void);    /* appkit_appearance.c */
u32 cx_appearance_accent(void);
#define D(l, d) (g_dark_draw ? (u32)(d) : (u32)(l))
#define INK(a) ((u32)(a) << 24 | (g_dark_draw ? 0xFFFFFFu : 0u))   /* traço/sombra translúcido: preto no claro, branco no escuro */
static u32 accent_a(u32 a) { return (a << 24) | (accent & 0xFFFFFF); }
static u32 accent_dim(void) { u32 r = ((accent >> 16) & 255) * 4 / 5, g = ((accent >> 8) & 255) * 4 / 5, b = (accent & 255) * 4 / 5; return 0xFF000000u | r << 16 | g << 8 | b; }

static void draw_view_tree(id v, absr_t *a);
static void draw_builtin(id v, absr_t *a);
BOOL cx_is_secure_field(id v);
static int tf_sel(vstate_t *s, int *a, int *b);
static int tf_next(const char *t, int n, int k);
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
static void ctl_set_string(id self, SEL c, id s) { (void)c; vstate_t *v = vs(self); id nv = s ? MSG0(id, s, "copy") : cx_str(""); objc_release(v->value); v->value = nv; if (v->caret > (int)strlen(cx_cstr(v->value))) v->caret = (int)strlen(cx_cstr(v->value)); mark(self); cx_ax_note_view(self, "AXValueChanged"); }
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
static i64 bt_type_code(id self, SEL c) { (void)c; int t = vs(self)->btype; return t == BT_CHECK ? 3 : t == BT_RADIO ? 4 : 7; }   /* NSButtonType (acessibilidade) */
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
static id pg_init_frame(id self, SEL c, NSRect f) {
    v_init_frame(self, c, f);
    vstate_t *s = vs(self);
    s->kind = K_PROGRESS;
    s->minv = 0;
    s->maxv = 100;
    s->show_idle = YES;
    return self;
}
static void pg_set_idle(id self, SEL c, BOOL v) { (void)c; vs(self)->show_idle = v ? YES : NO; mark(self); }
static BOOL pg_idle(id self, SEL c) { (void)c; return vs(self)->show_idle; }
static void pg_inc(id self, SEL c, double d) { (void)c; vs(self)->dval += d; mark(self); }
static void pg_set_indet(id self, SEL c, BOOL v) { (void)c; vs(self)->pg_indet = v ? YES : NO; mark(self); }
static BOOL pg_indet(id self, SEL c) { (void)c; return vs(self)->pg_indet; }
static void pg_set_style(id self, SEL c, i64 st) {
    (void)c;
    vstate_t *s = vs(self);
    s->pg_style = st == 1 ? 1 : 0;   /* NSProgressIndicatorStyleBar 0, Spinning 1 */
    mark(self);
}
static i64 pg_style(id self, SEL c) { (void)c; return vs(self)->pg_style; }
static void pg_tick(id self, SEL c, id timer) { (void)c; (void)timer; mark(self); }
static void pg_start(id self, SEL c, id sender) {
    (void)c; (void)sender;
    vstate_t *s = vs(self);
    if (s->pg_anim) return;
    s->pg_anim = YES;
    if (s->pg_indet || s->pg_style == 1) {   /* a determinada não anima no Cocoa */
        id t = MSG(id, (id)cx_class_named("NSTimer"), "scheduledTimerWithTimeInterval:target:selector:userInfo:repeats:", 1.0 / 30, self,
                   sel_registerName("_cxProgressTick:"), (id)0, (BOOL)YES);
        s->pg_timer = t ? objc_retain(t) : 0;
    }
    mark(self);
}
static void pg_stop(id self, SEL c, id sender) {
    (void)c; (void)sender;
    vstate_t *s = vs(self);
    if (!s->pg_anim) return;
    s->pg_anim = NO;
    if (s->pg_timer) { MSG0(void, s->pg_timer, "invalidate"); objc_release(s->pg_timer); s->pg_timer = 0; }
    mark(self);
}
static void pg_set_threaded(id self, SEL c, BOOL v) { (void)self; (void)c; (void)v; }   /* a animação é sempre no laço principal */
static BOOL pg_threaded(id self, SEL c) { (void)self; (void)c; return NO; }
static NSSize pg_size_to_fit_size(id self) { return vs(self)->pg_style == 1 ? (NSSize){32, 32} : (NSSize){vs(self)->frame.s.w, 20}; }
static void pg_size_to_fit(id self, SEL c) {
    (void)c;
    NSSize z = pg_size_to_fit_size(self);
    ((void (*)(id, SEL, NSSize))objc_msgSend)(self, sel_registerName("setFrameSize:"), z);
}
static id bx_init_frame(id self, SEL c, NSRect f) { v_init_frame(self, c, f); vstate_t *s = vs(self); s->kind = K_BOX; s->title = cx_str(""); return self; }
static id st_init_frame(id self, SEL c, NSRect f) { v_init_frame(self, c, f); vstate_t *s = vs(self); s->kind = K_STACK; s->spacing = 8; s->orientation = 0; return self; }
static id st_with_views(id cls, SEL c, id views) {
    (void)c;
    id s = MSG(id, MSG0(id, cls, "alloc"), "initWithFrame:", r_make(0, 0, 0, 0));
    nsarr_t *a = (nsarr_t *)views;
    for (u64 i = 0; views && i < a->n; i++) v_add_sub(s, 0, a->items[i]);
    return s;
}
/* views arrumadas: lista própria (removeArrangedSubview: deixa a view como subview comum); sem lista, todas as subviews
 * (stacks vindas de nib que não trazem a lista) */
static char k_arranged;
static id st_list(id self, BOOL make) {
    id l = objc_getAssociatedObject(self, &k_arranged);
    if (!l && make) {
        l = cx_arr_new(&NSMutableArray_c);
        vstate_t *s = vs(self);
        for (u32 i = 0; i < s->nsub; i++) cx_arr_add(l, s->sub[i]);
        objc_setAssociatedObject(self, &k_arranged, l, 1);
        objc_release(l);
    }
    return l;
}
static void st_add(id self, SEL c, id v) {
    if (!v) return;
    id l = st_list(self, YES);
    if (!MSG(BOOL, l, "containsObject:", v)) MSG(void, l, "addObject:", v);
    if (vs(v)->superview != self) v_add_sub(self, c, v);
    mark(self);
}
static void st_insert(id self, SEL c, id v, i64 at) {
    (void)c;
    if (!v) return;
    id l = st_list(self, YES);
    MSG(void, l, "removeObject:", v);
    u64 n = MSG0(u64, l, "count");
    MSG(void, l, "insertObject:atIndex:", v, (u64)(at < 0 ? 0 : (u64)at > n ? n : (u64)at));
    if (vs(v)->superview != self) v_add_sub(self, 0, v);
    mark(self);
}
static void st_remove(id self, SEL c, id v) { (void)c; id l = st_list(self, YES); if (v) MSG(void, l, "removeObject:", v); mark(self); }
static id st_arranged(id self, SEL c) {
    (void)c;
    id l = st_list(self, NO), r = cx_arr_new(&NSMutableArray_c);
    vstate_t *s = vs(self);
    if (l) { u64 n = MSG0(u64, l, "count"); for (u64 i = 0; i < n; i++) { id v = MSG(id, l, "objectAtIndex:", i); if (vs(v)->superview == self) cx_arr_add(r, v); } }
    else for (u32 i = 0; i < s->nsub; i++) cx_arr_add(r, s->sub[i]);
    return objc_autorelease(r);
}
static void st_set_align(id self, SEL c, i64 a) { (void)c; vs(self)->st_align = (int)a; vs(self)->st_align_set = YES; mark(self); }
static i64 st_align(id self, SEL c) { (void)c; vstate_t *s = vs(self); return s->st_align_set ? s->st_align : (s->orientation == 0 ? 10 : 9); }
static void st_set_dist(id self, SEL c, i64 d) { (void)c; vs(self)->st_dist = (int)d + 1; mark(self); }   /* guarda +1: 0 = padrão (GravityAreas) */
static i64 st_dist(id self, SEL c) { (void)c; return vs(self)->st_dist ? vs(self)->st_dist - 1 : -1; }
typedef struct { double top, left, bottom, right; } st_insets_t;
static void st_set_insets(id self, SEL c, st_insets_t e) { (void)c; vstate_t *s = vs(self); s->st_ins[0] = e.top; s->st_ins[1] = e.left; s->st_ins[2] = e.bottom; s->st_ins[3] = e.right; mark(self); }
static st_insets_t st_insets(id self, SEL c) { (void)c; vstate_t *s = vs(self); return (st_insets_t){s->st_ins[0], s->st_ins[1], s->st_ins[2], s->st_ins[3]}; }
static double st_spacing(id self, SEL c) { (void)c; return vs(self)->spacing; }
static i64 st_orient(id self, SEL c) { (void)c; return vs(self)->orientation; }
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
        const char *txt = cx_cstr(s->value ? s->value : cx_str(""));
        NSSize t = text_size(txt, size, fnt(f)->weight);
        z.w = t.w + (s->bezeled ? 10 : 4);
        z.h = t.h + (s->bezeled ? 6 : 1);
        /* rótulo de várias linhas: preferredMaxLayoutWidth limita a largura e a altura cresce com as linhas */
        int lbm = s->tf_lbm1 - 1;
        if (s->tf_pref_w > 0 && !s->tf_single && s->tf_max_lines != 1 && (s->tf_lbm1 == 0 || lbm == TFIT_WORD || lbm == TFIT_CHAR)) {
            double pad = s->bezeled ? 10 : 4, inner = s->tf_pref_w - pad;
            if (inner < 1) inner = 1;
            tfm_ctx tcx = {size, fnt(f)->weight};
            i64 lines = tfit_lines(txt, inner, lbm == TFIT_CHAR ? TFIT_CHAR : TFIT_WORD, tfm_measure, &tcx);
            if (s->tf_max_lines > 0 && lines > s->tf_max_lines) lines = s->tf_max_lines;
            gui_metrics_t m0 = measure("Ag", 2, size, fnt(f)->weight);
            double lh = m0.ascent + m0.descent + 2;
            if (lines > 1 || t.w + pad > s->tf_pref_w) { z.w = s->tf_pref_w; z.h = lines * lh + (s->bezeled ? 6 : 1); }
        }
    } else if (s->kind == K_SLIDER) { z.w = 160; z.h = 22; }
    return z;
}

/* NSStackView: as views arrumadas lado a lado (horizontal) ou de cima para baixo (vertical), dentro das margens.
 * distribution: -1 GravityAreas (empacota no início), 0 Fill (a de menor hugging absorve a sobra), 1 FillEqually,
 * 2 FillProportionally, 3 EqualSpacing, 4 EqualCentering. alignment no eixo cruzado: Top 3 / Bottom 4 / CenterY 10 /
 * Height 8 (horizontal); Leading 5, Left 1 / Trailing 6, Right 2 / CenterX 9 / Width 7 (vertical). */
static void layout_stack(id v) {
    vstate_t *s = vs(v);
    id views[256];
    int n = 0;
    id l = st_list(v, NO);
    if (l) { u64 c = MSG0(u64, l, "count"); for (u64 i = 0; i < c && n < 256; i++) { id x = MSG(id, l, "objectAtIndex:", i); if (vs(x)->superview == v && !vs(x)->hidden) views[n++] = x; } }
    else for (u32 i = 0; i < s->nsub && n < 256; i++) if (!vs(s->sub[i])->hidden) views[n++] = s->sub[i];
    if (!n) return;
    BOOL hz = s->orientation == 0;
    double top = s->st_ins[0], left = s->st_ins[1], bottom = s->st_ins[2], right = s->st_ins[3];
    double W = s->frame.s.w, H = s->frame.s.h;
    double avail = hz ? W - left - right : H - top - bottom, cross_avail = hz ? H - top - bottom : W - left - right;
    double m[256], cr[256], sum = 0;
    for (int i = 0; i < n; i++) {
        vstate_t *cs = vs(views[i]);
        NSSize z = v_intrinsic(views[i], 0);
        if (z.w > 0 && cs->frame.s.w <= 0) cs->frame.s.w = z.w;
        if (z.h > 0 && cs->frame.s.h <= 0) cs->frame.s.h = z.h;
        m[i] = hz ? cs->frame.s.w : cs->frame.s.h;
        cr[i] = hz ? cs->frame.s.h : cs->frame.s.w;
        sum += m[i];
    }
    int dist = s->st_dist ? s->st_dist - 1 : -1;
    double gap = s->spacing, gaps = gap * (n - 1);
    if (dist == 0) {          /* Fill: a sobra (ou a falta) vai para a view de menor hugging; empate, a última */
        int k = n - 1;
        double best = 1e9;
        for (int i = 0; i < n; i++) { double hp = hz ? vs(views[i])->hug_h : vs(views[i])->hug_v; if (hp <= 0) hp = 250; if (hp <= best) { best = hp; k = i; } }
        m[k] += avail - sum - gaps;
        if (m[k] < 0) m[k] = 0;
    } else if (dist == 1) {   /* FillEqually */
        double e = (avail - gaps) / n;
        for (int i = 0; i < n; i++) m[i] = e > 0 ? e : 0;
    } else if (dist == 2 && sum > 0) {   /* FillProportionally */
        double f = (avail - gaps) / sum;
        for (int i = 0; i < n; i++) m[i] *= f > 0 ? f : 0;
    } else if (dist == 3 && n > 1) {     /* EqualSpacing: o espaçamento é o mínimo */
        double g = (avail - sum) / (n - 1);
        gap = g > gap ? g : gap;
    }
    double pos = 0;
    double step_c = 0;
    if (dist == 4 && n > 1) step_c = (avail - m[0] / 2 - m[n - 1] / 2) / (n - 1);   /* EqualCentering */
    int al = s->st_align_set ? s->st_align : (hz ? 10 : 9);
    for (int i = 0; i < n; i++) {
        vstate_t *cs = vs(views[i]);
        double a0 = dist == 4 && n > 1 ? m[0] / 2 + step_c * i - m[i] / 2 : pos;
        double c = cr[i], co;
        if ((hz && al == 8) || (!hz && al == 7)) c = cross_avail;   /* Height / Width: ocupa o eixo cruzado */
        if (hz) {
            co = al == 3 ? H - top - c : al == 4 || al == 11 || al == 12 ? bottom : bottom + (cross_avail - c) / 2;
            if (al == 8) co = bottom;
            cs->frame = r_make(left + a0, co, m[i], c);
        } else {
            co = al == 5 || al == 1 ? left : al == 6 || al == 2 ? W - right - c : left + (cross_avail - c) / 2;
            if (al == 7) co = left;
            cs->frame = r_make(co, H - top - a0 - m[i], c, m[i]);   /* de cima para baixo */
        }
        pos += m[i] + gap;
    }
}

/* desenho dos controles nativos (coordenadas em pixels do conteúdo) */
/* NSSecureTextField e subclasses: o texto não aparece nem sai por cópia */
BOOL cx_is_secure_field(id v) {
    static cx_class_t *sec;
    if (!sec) sec = cx_class_named("NSSecureTextField");
    return sec && v && cx_is_subclass(v->isa, sec);
}
static void draw_builtin(id v, absr_t *a) {
    g_dark_draw = cx_appearance_dark();
    accent = cx_appearance_accent();
    vstate_t *s = vs(v);
    id f = s->font ? s->font : dflt_font();
    double size = fnt(f)->size;
    int wt = fnt(f)->weight;
    double x = a->tlx, y = a->tly, w = a->w, h = a->h;
    switch (s->kind) {
    case K_BUTTON: {
        const char *t = cx_cstr(s->title ? s->title : cx_str(""));
        if (s->image && !t[0]) { draw_image_ctl(s->image, x + 2, y + 2, w - 4, h - 4, 0, !s->enabled ? INK(0x55) : s->tint ? argb(s->tint) : INK(0xD9)); break; }
        u32 n = (u32)strlen(t);
        gui_metrics_t m = measure(t, n, size, wt);
        u32 tc = s->enabled ? D(0xFF1D1D1F, 0xFFEBEBEB) : D(0xFF9A9A9E, 0xFF6E6E70);
        if (s->btype == BT_PUSH) {
            BOOL def = s->key_equiv && !strcmp(cx_cstr(s->key_equiv), "\r");
            double bh = h > 28 ? h : 22, by = y + (h - bh) / 2;
            if (s->bezeled) {
                if (def && s->enabled) g_rrect(x + 1, by + 1, w - 2, bh - 2, 6, s->highlighted ? accent_dim() : accent);
                else {
                    g_rrect(x + 1, by + 1, w - 2, bh - 2, 6, s->highlighted ? D(0xFFDCDCE0, 0xFF7A7A7C) : D(0xFFFFFFFF, 0xFF656567));
                    g_rstroke(x + 1, by + 1, w - 2, bh - 2, 6, 1, INK(0x30));
                }
                if (def && s->enabled) tc = 0xFFFFFFFF;
            } else if (s->highlighted) g_rrect(x, y, w, h, 5, INK(0x18));
            g_text(x + (w - m.width) / 2, by + (bh + m.ascent - m.descent) / 2, size, wt, tc, t, n);
        } else {
            double bs = 14, bx = x + 1, byy = y + (h - bs) / 2;
            BOOL on = s->state == 1;
            if (s->btype == BT_CHECK) {
                g_rrect(bx, byy, bs, bs, 3.5, on ? accent : D(0xFFFFFFFF, 0xFF3A3A3C));
                if (!on) g_rstroke(bx, byy, bs, bs, 3.5, 1, INK(0x40));
                if (on) { g_line(bx + 3.2, byy + 7.2, bx + 6, byy + 10, 2, 0xFFFFFFFF); g_line(bx + 6, byy + 10, bx + 11, byy + 4, 2, 0xFFFFFFFF); }
            } else {
                g_rrect(bx, byy, bs, bs, bs / 2, on ? accent : D(0xFFFFFFFF, 0xFF3A3A3C));
                if (!on) g_rstroke(bx, byy, bs, bs, bs / 2, 1, INK(0x40));
                if (on) g_rrect(bx + 4.5, byy + 4.5, 5, 5, 2.5, 0xFFFFFFFF);
            }
            g_text(bx + bs + 6, y + (h + m.ascent - m.descent) / 2, size, wt, tc, t, n);
        }
        break;
    }
    case K_TEXT: {
        const char *t = cx_cstr(s->value ? s->value : cx_str(""));
        u32 n = (u32)strlen(t);
        /* NSSecureTextField: um • por caractere, nunca o texto (antes a senha aparecia em claro) */
        static char masked[3 * 512 + 1];
        int secure_caret = -1;
        if (cx_is_secure_field(v)) {
            u32 o = 0, chars = 0;
            for (u32 i = 0; i < n && o + 3 < sizeof masked; i++) {
                if (((unsigned char)t[i] & 0xC0) == 0x80) continue;
                if ((int)i < s->caret) chars++;
                memcpy(masked + o, "\xE2\x80\xA2", 3); o += 3;
            }
            masked[o] = 0;
            secure_caret = (int)chars * 3;
            t = masked; n = o;
        }
        u32 tc = s->text_color ? argb(s->text_color) : (s->enabled ? INK(0xF0) : INK(0x80));
        BOOL focused = s->window && ws(s->window)->first == v && s->editable;
        if (s->draws_bg || s->bezeled) {
            u32 bg = s->bg_color ? argb(s->bg_color) : D(0xFFFFFFFF, 0xFF1E1E1E);
            if (s->bezeled) {
                g_rrect(x, y, w, h, 5, bg);
                g_rstroke(x + 0.5, y + 0.5, w - 1, h - 1, 5, focused ? 2.5 : 1, focused ? accent_a(0x99) : INK(0x33));
            } else g_fill(x, y, w, h, bg);
        }
        gui_metrics_t m = measure(t, n, size, wt);
        double pad = s->bezeled ? 5 : 2;
        set_clip(x + 1, y + 1, w - 2, h - 2);
        if (!n && s->placeholder) {
            const char *p = cx_cstr(s->placeholder);
            g_text(x + pad, y + (h + m.ascent - m.descent) / 2, size, wt, INK(0x55), p, (u32)strlen(p));
        } else if (!s->bezeled && !s->editable && h > (m.ascent + m.descent) * 1.8 && !s->tf_single && s->tf_max_lines != 1
                   && (s->tf_lbm1 == 0 || s->tf_lbm1 - 1 == TFIT_WORD || s->tf_lbm1 - 1 == TFIT_CHAR)) {
            int mode = s->tf_lbm1 - 1 == TFIT_CHAR ? TFIT_CHAR : TFIT_WORD;
            if (s->tf_lbm1 == 0 && s->tf_max_lines <= 0) draw_wrapped(t, x + pad, y + 1, w - 2 * pad, h, size, wt, tc, s->alignment);
            else draw_wrapped_mode(t, x + pad, y + 1, w - 2 * pad, h, size, wt, tc, s->alignment, mode, s->tf_max_lines);
        } else {
            /* uma linha: truncamento com reticências (fora da edição), como NSLineBreakByTruncating* */
            static char trunc[1024];
            int lbm = s->tf_lbm1 - 1;
            if (!focused && (lbm == TFIT_HEAD || lbm == TFIT_TAIL || lbm == TFIT_MIDDLE) && m.width > w - 2 * pad) {
                tfm_ctx tcx = {size, wt};
                int tn = tfit_truncate(t, (int)n, w - 2 * pad, lbm, tfm_measure, &tcx, trunc, (int)sizeof trunc);
                t = trunc; n = (u32)tn;
                m = measure(t, n, size, wt);
            }
            double tx = x + pad;
            if (s->alignment == 2) tx = x + (w - m.width) / 2;          /* NSTextAlignmentCenter */
            else if (s->alignment == 1) tx = x + w - pad - m.width;     /* direita */
            double base = y + (h + m.ascent - m.descent) / 2;
            int sa, sb;
            if (focused && tf_sel(s, &sa, &sb)) {
                if (secure_caret >= 0) {   /* posições em bytes do texto real -> bytes da máscara */
                    const char *rt = cx_cstr(s->value ? s->value : cx_str(""));
                    int ca = 0, cb = 0;
                    for (int k = 0; rt[k]; k++) if (((unsigned char)rt[k] & 0xC0) != 0x80) { if (k < sa) ca++; if (k < sb) cb++; }
                    sa = ca * 3; sb = cb * 3;
                }
                double x0 = measure(t, (u32)sa, size, wt).width, x1 = measure(t, (u32)sb, size, wt).width;
                g_fill(tx + x0, base - m.ascent, x1 - x0, m.ascent + m.descent, accent_a(0x55));
            }
            g_text(tx, base, size, wt, tc, t, n);
            if (focused) {
                gui_metrics_t cm = measure(t, (u32)(secure_caret >= 0 ? secure_caret : s->caret), size, wt);
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
        g_rrect(x0, cy - 2, x1 - x0, 4, 2, INK(0x26));
        g_rrect(x0, cy - 2, kx - x0, 4, 2, accent);
        g_rrect(kx - 9, cy - 9, 18, 18, 9, D(0xFFFFFFFF, 0xFFCFCFD1));
        g_rstroke(kx - 9, cy - 9, 18, 18, 9, 1, INK(0x33));
        break;
    }
    case K_PROGRESS: {
        /* parada: o giratório e a indeterminada somem com displayedWhenStopped = NO; a barra determinada segue a regra antiga */
        if (!s->pg_anim) {
            if (s->pg_style == 1 || s->pg_indet) { if (!s->show_idle) break; }
            else if (!cx_progress_draws(s->show_idle, s->dval, s->minv)) break;
        }
        double range = s->maxv - s->minv, t = range > 0 ? (s->dval - s->minv) / range : 0;
        if (t < 0) t = 0; if (t > 1) t = 1;
        double now = cx_now();
        if (s->pg_style == 1) {   /* giratório */
            double cx = x + w / 2, cy = y + h / 2, r = (w < h ? w : h) / 2 - 1;
            if (r < 3) break;
            if (s->pg_indet) {     /* 12 raios; o mais escuro gira (12 passos por segundo) */
                int head = s->pg_anim ? (int)(now * 12) % 12 : 0;
                for (int i = 0; i < 12; i++) {
                    double ang = i * 3.14159265358979 / 6 - 3.14159265358979 / 2, cs = cos(ang), sn = sin(ang);
                    int age = (head - i + 12) % 12;
                    u32 a = (u32)(0xD0 - age * 0x10);
                    g_line(cx + cs * r * 0.48, cy + sn * r * 0.48, cx + cs * r * 0.95, cy + sn * r * 0.95, r * 0.17 > 1.2 ? r * 0.17 : 1.2, (a << 24) | (g_dark_draw ? 0xEBEBF0u : 0x3C3C43u));
                }
            } else {               /* determinado: anel e pizza do valor, a partir do topo no sentido horário */
                g_rstroke(cx - r, cy - r, 2 * r, 2 * r, r, 1.2, INK(0x40));
                if (t > 0) {
                    float xy[2 * 66];
                    int n = 0;
                    xy[n++] = (float)cx; xy[n++] = (float)cy;
                    int steps = (int)(t * 64) + 1;
                    for (int k = 0; k <= steps; k++) {
                        double ang = -3.14159265358979 / 2 + (t * 2 * 3.14159265358979) * k / steps;
                        xy[n++] = (float)(cx + cos(ang) * (r - 2)); xy[n++] = (float)(cy + sin(ang) * (r - 2));
                    }
                    g_poly(xy, (u32)(n / 2), accent, 1, 0);
                }
            }
            break;
        }
        double cy = y + h / 2;
        g_rrect(x, cy - 3, w, 6, 3, INK(0x1F));
        if (s->pg_indet) {         /* faixa de 30% que corre da esquerda para a direita */
            if (s->pg_anim) {
                double ph = now * 0.7 - floor(now * 0.7), sw = w * 0.3, p0 = x + (ph * 1.3 - 0.3) * w;
                double a0 = p0 < x ? x : p0, a1 = p0 + sw > x + w ? x + w : p0 + sw;
                if (a1 > a0) g_rrect(a0, cy - 3, a1 - a0, 6, 3, accent);
            }
        } else if (t > 0) g_rrect(x, cy - 3, w * t, 6, 3, accent);
        break;
    }
    case K_TABLE: draw_table(v, a); break;
    case K_BOX: {
        if (s->btype == 2) { g_fill(x, y + h / 2, w, 1, INK(0x1F)); break; }            /* NSBoxSeparator */
        if (s->bg_color) { g_rrect(x, y, w, h, s->dval, argb(s->bg_color)); break; }   /* NSBoxCustom com fillColor */
        const char *t = cx_cstr(s->title ? s->title : cx_str(""));
        if (s->image && !t[0]) { draw_image_fit(s->image, x + 2, y + 2, w - 4, h - 4); break; }
        u32 n = (u32)strlen(t);
        gui_metrics_t m = measure(t, n, 11, 0);
        double top = n ? m.ascent + m.descent + 4 : 0;
        g_rrect(x, y + top, w, h - top, 6, INK(0x0D));
        g_rstroke(x + 0.5, y + top + 0.5, w - 1, h - top - 1, 6, 1, INK(0x1A));
        if (n) g_text(x + 4, y + m.ascent, 11, 0, INK(0xB0), t, n);
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
static void draw_view_tree_inner(id v, absr_t *a);
id cx_appearance_enter_view(id v);     /* appkit_appearance.c: aparência de desenho da view (com alguma explícita) */
void cx_appearance_leave(id saved);
static void draw_view_tree(id v, absr_t *a) {
    vstate_t *s = vs(v);
    double saved = g_view_alpha;
    if (s->valpha_set) {
        if (s->valpha <= 0.001) return;   /* alfa 0: nada a desenhar */
        g_view_alpha = saved * s->valpha;
    }
    id ap = cx_appearance_enter_view(v);
    draw_view_tree_inner(v, a);
    cx_appearance_leave(ap);
    g_view_alpha = saved;
}
static void draw_view_tree_inner(id v, absr_t *a) {
    vstate_t *s = vs(v);
    if (s->hidden || a->w <= 0 || a->h <= 0) return;
    if (s->kind == K_STACK) layout_stack(v);
    if (s->kind == K_SCROLL) layout_scroll(v);
    double sx0 = clip_x0, sy0 = clip_y0, sx1 = clip_x1, sy1 = clip_y1;
    ctx.tlx = a->tlx; ctx.tly = a->tly; ctx.w = a->w; ctx.h = a->h; ctx.flipped = a->flipped;
    set_clip(a->tlx, a->tly, a->w, a->h);
    if (cx_layer_draw_hook) cx_layer_draw_hook(v, 0, a->tlx, a->tly, a->w, a->h, clip_x0, clip_y0, clip_x1, clip_y1);   /* gancho QuartzCore */
    if (s->kind == K_IMAGE && s->image) draw_image_ctl(s->image, a->tlx, a->tly, a->w, a->h, s->img_scaling, s->tint ? argb(s->tint) : 0xD9000000);
    if (s->kind == K_SCROLL && s->draws_bg) g_fill(a->tlx, a->tly, a->w, a->h, s->bg_color ? argb(s->bg_color) : (cx_appearance_dark() ? 0xFF1E1E1E : 0xFFFFFFFF));
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
        BOOL vbar = !s->sv_vset || s->sv_v, hbar = !s->sv_hset || s->sv_h;
        g_dark_draw = cx_appearance_dark();
        BOOL track = s->sv_vset && s->sv_v && !s->sv_autohide;   /* sem esconder: a calha fica à vista */
        set_clip(a->tlx, a->tly, a->w, a->h);
        if (vbar && (d->frame.s.h > a->h || track)) {
            if (track) g_fill(a->tlx + a->w - 11, a->tly, 11, a->h, INK(0x0C));
            if (d->frame.s.h > a->h) {
                double vis = a->h / d->frame.s.h, bh = a->h * vis, by = a->tly + s->scroll_y / d->frame.s.h * a->h;
                g_rrect(a->tlx + a->w - 8, by + 2, 5, bh - 4 < 12 ? 12 : bh - 4, 2.5, INK(0x55));
            }
        }
        if (hbar && d->frame.s.w > a->w + 0.5) {
            double vis = a->w / d->frame.s.w, bw = a->w * vis, bx = a->tlx + s->scroll_x / d->frame.s.w * a->w;
            g_rrect(bx + 2, a->tly + a->h - 8, bw - 4 < 12 ? 12 : bw - 4, 5, 2.5, INK(0x55));
        }
    }
    if (s->kind == K_SCROLL && s->sv_border) {   /* NSLineBorder, NSBezelBorder, NSGrooveBorder */
        set_clip(a->tlx, a->tly, a->w, a->h);
        g_dark_draw = cx_appearance_dark();
        if (s->sv_border == 1) g_rstroke(a->tlx + 0.5, a->tly + 0.5, a->w - 1, a->h - 1, 0, 1, INK(0x80));
        else if (s->sv_border == 2) { g_rstroke(a->tlx + 0.5, a->tly + 0.5, a->w - 1, a->h - 1, 0, 1, INK(0x33)); g_fill(a->tlx, a->tly, a->w, 1, INK(0x40)); }
        else { g_rstroke(a->tlx + 0.5, a->tly + 0.5, a->w - 1, a->h - 1, 0, 1, INK(0x40)); g_rstroke(a->tlx + 1.5, a->tly + 1.5, a->w - 3, a->h - 3, 0, 1, g_dark_draw ? 0x60000000u : 0x60FFFFFFu); }
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
    s->restorable = YES;
    s->bg = MSG0(id, (id)&NSColor_c, "windowBackgroundColor");   /* do sistema: acompanha claro/escuro */
    if (!s->bg) s->bg = cx_color(0.93, 0.93, 0.93, 1);
    id cv = MSG(id, MSG0(id, &NSView_c, "alloc"), "initWithFrame:", r_make(0, 0, r.s.w, r.s.h));
    s->content = cv;
    set_window_rec(cv, self);
    vs(cv)->autoresize = 2 | 16;
    return self;
}
static id w_init_simple(id self, SEL c) { return w_init(self, c, r_make(0, 0, 480, 320), 15, 2, NO); }
static id w_with_vc(id cls, SEL c, id vc) { (void)vc; return w_init(MSG0(id, cls, "alloc"), c, r_make(0, 0, 480, 320), 15, 2, NO); }
/* texto da barra de título no window server: vazio com titleVisibility = NSWindowTitleHidden */
static const char *w_shown_title(id self) { wstate_t *s = ws(self); return s->title_hidden || !s->title ? "" : cx_cstr(s->title); }
static void w_set_title(id self, SEL c, id t) {
    (void)c;
    wstate_t *s = ws(self);
    { id nt = t ? MSG0(id, t, "copy") : cx_str(""); objc_release(s->title); s->title = nt; }
    if (s->gid > 0) SYS2(GUI_WIN_TITLE, s->gid, w_shown_title(self));
}
static id w_title(id self, SEL c) { (void)c; return ws(self)->title; }
static void w_set_next(id self, SEL c, id n) { (void)c; ws(self)->next_responder = n; }
static id w_next(id self, SEL c) { (void)c; wstate_t *s = ws(self); return s->next_responder ? s->next_responder : NSApp; }
static void w_set_subtitle(id self, SEL c, id t) {
    (void)c;
    wstate_t *s = ws(self);
    id nt = t ? MSG0(id, t, "copy") : cx_str("");
    objc_release(s->subtitle);
    s->subtitle = nt;
}
static id w_subtitle(id self, SEL c) { (void)c; id t = ws(self)->subtitle; return t ? t : cx_str(""); }
static void w_set_url(id self, SEL c, id u) { (void)c; keep(&ws(self)->represented_url, u); }
static id w_url(id self, SEL c) { (void)c; return ws(self)->represented_url; }
static void w_set_restorable(id self, SEL c, BOOL v) { (void)c; ws(self)->restorable = v ? YES : NO; }
static BOOL w_restorable(id self, SEL c) { (void)c; return ws(self)->restorable; }
static char tab_win_key;
static id tab_window(id self, SEL c) { (void)c; return objc_getAssociatedObject(self, &tab_win_key); }
static void tab_set_window(id self, SEL c, id w) { (void)c; objc_setAssociatedObject(self, &tab_win_key, w, 0); }
static id w_tab(id self, SEL c) {
    (void)c;
    wstate_t *s = ws(self);
    if (!s->tab) {
        cx_class_t *k = cx_class_named("NSWindowTab");
        if (k) {
            s->tab = cx_alloc_instance(k);
            if (s->tab) tab_set_window(s->tab, 0, self);
        }
    }
    return s->tab;
}
static void w_set_tab(id self, SEL c, id t) {
    (void)c;
    keep(&ws(self)->tab, t);
    if (t) tab_set_window(t, 0, self);
}
static id w_content(id self, SEL c) { (void)c; return ws(self)->content; }
static void w_set_content(id self, SEL c, id v) {
    (void)c;
    wstate_t *s = ws(self);
    if (!v) return;
    keep(&s->content, v);
    vs(v)->superview = 0;
    /* a view ganha o tamanho do conteúdo da janela e as subviews seguem o autoresizing: o Chromium cria a
     * ViewsCompositorSuperview (o layer do compositor da janela) com os bounds da BridgedContentView antes de ela entrar
     * na janela; sem o autoresizing aqui ela ficava 0x0 e o quadro do compositor nunca aparecia */
    NSSize old = vs(v)->frame.s;
    vs(v)->frame = r_make(0, 0, s->frame.s.w, s->frame.s.h);
    set_window_rec(v, self);
    if (old.w != vs(v)->frame.s.w || old.h != vs(v)->frame.s.h) resize_children(v, old);
    s->needs = YES;
}
static void w_set_delegate(id self, SEL c, id d) { (void)c; ws(self)->delegate = d; }
static id w_delegate(id self, SEL c) { (void)c; return ws(self)->delegate; }
/* alphaValue guardado (o mOS ainda não compõe a janela com transparência); parentWindow/addChildWindow: só registram */
static double w_alpha(id self, SEL c) { (void)c; double a = ws(self)->alpha; return a > 0 ? a : (a < 0 ? 0 : 1.0); }
static void w_set_alpha(id self, SEL c, double a) { (void)c; ws(self)->alpha = a <= 0 ? -1 : a; }
static id w_parent(id self, SEL c) { (void)c; return ws(self)->parent; }
/* a dona no window server (GUI_FOCUS_OWNER): a filha fica acima dela mesmo quando o usuário clica na dona ou o Dock a traz */
static void w_sync_owner(id child) {
    wstate_t *cs = ws(child);
    if (cs->gid <= 0) return;
    int pg = cs->parent ? ws(cs->parent)->gid : 0;
    SYS3(GUI_WIN_FOCUS, cs->gid, GUI_FOCUS_OWNER, pg > 0 ? pg : 0);
}
static void w_set_parent(id self, SEL c, id p) { (void)c; ws(self)->parent = p; w_sync_owner(self); }
static void w_add_child(id self, SEL c, id child, i64 ord) { (void)c; (void)ord; if (child) { ws(child)->parent = self; w_sync_owner(child); } }
static void w_remove_child(id self, SEL c, id child) { (void)c; if (child && ws(child)->parent == self) { ws(child)->parent = 0; w_sync_owner(child); } }
/* NSWindowStyleMaskFullSizeContentView (1 << 15), como o Electron usa com titleBarStyle "hidden" (o VS Code
 * desenha a própria barra de título): o conteúdo é a janela inteira, moldura = conteúdo, e o window server
 * só põe os semáforos por cima (GUI_WF_FULLSIZE). Nas outras janelas a moldura tem 22 pt a mais. */
#define WS_FULLSIZE (1ull << 15)
/* Janela sem moldura do Electron (frame: false, como a do VS Code): a ElectronNSWindow sobrescreve
 * contentRectForFrameRect: e devolve a moldura inteira, e o Chromium dimensiona as views por ela. O conteúdo
 * do mOS tem de ser então a janela toda, como numa FullSizeContentView; senão a RenderWidgetHostViewCocoa
 * ficava 22-44 pt mais alta que a janela (barra de status cortada e cliques deslocados). Descobre uma vez
 * por janela chamando a versão da subclasse (que, se chamar a nossa, recebe a resposta normal). */
static NSRect w_content_rect_for(id self, SEL c, NSRect r);
void objc_msgSend_stret(void);         /* retorno de NSRect (32 bytes) vem por ponteiro oculto */
static int probing_frame;
static BOOL w_frameless(id self, wstate_t *s) {
    if (s->frameless) return s->frameless == 2;
    if (probing_frame) return NO;
    SEL sel = sel_registerName("contentRectForFrameRect:");
    IMP imp = cx_lookup(self->isa, sel);
    if (!imp || imp == (IMP)w_content_rect_for) { s->frameless = 1; return NO; }
    probing_frame = 1;
    NSRect t = r_make(0, 0, 400, 300);
    NSRect o = ((NSRect (*)(id, SEL, NSRect))objc_msgSend_stret)(self, sel, t);
    probing_frame = 0;
    s->frameless = (o.s.w == t.s.w && o.s.h == t.s.h) ? 2 : 1;
    return s->frameless == 2;
}
/* altura da barra de título que o mOS põe: nenhuma em FullSizeContentView, na janela sem moldura do Electron e na
 * borderless (sem NSWindowStyleMaskTitled, bit 0), como no Cocoa: os diálogos e popups do Chromium (views::Widget)
 * desenham a própria moldura e o título dentro do conteúdo */
static int w_title_h(id self) { wstate_t *s = ws(self); return ((s->style & WS_FULLSIZE) || !(s->style & 1) || w_frameless(self, s)) ? 0 : 22; }
/* -setFrame:display: e -setContentSize: que mudam o tamanho avisam como no Cocoa: o NativeWidgetNSWindowBridge
 * do Chromium só refaz as contas de conteúdo (views e compositor) no windowDidResize: */
static void w_did_resize(id self) {
    wstate_t *s = ws(self);
    id note = cx_note(NSWindowDidResizeNotification, self, 0);
    if (s->delegate && cx_responds(s->delegate->isa, sel_registerName("windowDidResize:"))) MSG(void, s->delegate, "windowDidResize:", note);
    cx_post(note);
}
/* diagnóstico (ca-log): as contas de moldura x conteúdo que o Chromium faz, cada tipo com o seu limite de linhas */
static void geo_log(int *n, const char *what, id w, NSRect in, NSRect out) {
    extern int cx_ca_diag(void);
    if (*n < 12 && cx_ca_diag()) {
        (*n)++;
        cx_fdprintf(2, "[ca] %s: %dx%d -> %dx%d (janela %d)\n", what, (int)in.s.w, (int)in.s.h, (int)out.s.w, (int)out.s.h, ws(w)->gid);
    }
}
static char k_pos_set, k_level;
static void w_push_frame(id self, int resize);
static void w_pull_frame(id self);
static i64 w_level(id self, SEL c);
static NSRect w_frame(id self, SEL c) {
    (void)c;
    w_pull_frame(self);
    NSRect r = ws(self)->frame;
    r.s.h += w_title_h(self);
    static int n; geo_log(&n, "frame", self, ws(self)->frame, r);
    return r;
}
static NSRect w_content_rect_for(id self, SEL c, NSRect r) {
    (void)c;
    NSRect o = r; o.s.h -= w_title_h(self);
    static int n; geo_log(&n, "contentRectForFrameRect", self, r, o);
    return o;
}
static NSRect w_frame_rect_for(id self, SEL c, NSRect r) {
    (void)c;
    NSRect o = r; o.s.h += w_title_h(self);
    static int n; geo_log(&n, "frameRectForContentRect", self, r, o);
    return o;
}
static u64 w_style_mask(id self, SEL c) { (void)c; return ws(self)->style; }
static void w_set_style_mask(id self, SEL c, u64 m) {
    (void)c;
    wstate_t *s = ws(self);
    if (s->gid > 0) m = (m & ~WS_FULLSIZE) | (s->style & WS_FULLSIZE);   /* a janela do window server não muda */
    int before = w_title_h(self);
    s->style = m;
    int d = before - w_title_h(self);
    /* a moldura fica onde está e o conteúdo ganha (ou devolve) a faixa da barra de título. Depois que a
     * janela existe no window server o estilo dela não muda mais (GUI_WIN_CREATE já passou). */
    if (d && s->gid <= 0) {
        s->frame.s.h += d;
        if (s->frame.s.h < 1) s->frame.s.h = 1;
        if (s->content) v_set_frame(s->content, 0, r_make(0, 0, s->frame.s.w, s->frame.s.h));
    }
}
/* área do conteúdo fora da barra de título (coordenadas da janela, origem embaixo) */
static NSRect w_content_layout_rect(id self, SEL c) {
    (void)c;
    wstate_t *s = ws(self);
    double h = s->frame.s.h - (w_title_h(self) == 0 ? 28 : 0);
    return r_make(0, 0, s->frame.s.w, h > 0 ? h : 0);
}
/* -[NSWindow performWindowDragWithEvent:]: o Chromium chama no clique numa região de arrastar
 * (-webkit-app-region: drag); o window server passa a mover a janela com o botão que está apertado */
static void w_perform_drag(id self, SEL c, id ev) { (void)c; (void)ev; wstate_t *s = ws(self); if (s->gid > 0) SYS2(GUI_WIN_FOCUS, s->gid, GUI_FOCUS_MOVE); }
static void w_set_bg(id self, SEL c, id col_) { (void)c; keep(&ws(self)->bg, col_); ws(self)->needs = YES; }
static id w_bg(id self, SEL c) { (void)c; return ws(self)->bg; }
static BOOL w_visible(id self, SEL c) { (void)c; return ws(self)->visible; }
static void w_set_released(id self, SEL c, BOOL b) { (void)c; ws(self)->released_when_closed = b; }
static id w_first(id self, SEL c) { (void)c; return ws(self)->first; }
/* -[NSWindow makeFirstResponder:], como no Cocoa: o atual tem de aceitar resignFirstResponder, e o novo recebe
 * becomeFirstResponder (se recusar, a janela fica sem respondedor). É nesse becomeFirstResponder que a
 * RenderWidgetHostViewCocoa avisa o host (OnFirstResponderChanged -> GotFocus -> SetFocus no renderer): sem ele a página
 * nunca tinha foco, o Blink descartava o texto digitado e o editor do VS Code não mostrava o cursor. */
static BOOL w_make_first(id self, SEL c, id r) {
    (void)c;
    wstate_t *s = ws(self);
    if (s->first == r) return YES;
    id old = s->first;
    if (old && !MSG0(BOOL, old, "resignFirstResponder")) return NO;
    s->first = r;
    if (old) mark(old);
    if (r) {
        if (!MSG0(BOOL, r, "becomeFirstResponder")) { s->first = 0; return NO; }
        mark(r);
    }
    cx_ax_note_view(r ? r : self, "AXFocusedUIElementChanged");
    return YES;
}
/* windowNumber: como no macOS, cada janela tem o seu, único e fixo, mesmo antes de ir à tela e depois de fechada.
 * Antes era o gid do window server (0 sem janela): as NativeWidgetMacOverlayNSWindow do Chrome ficavam todas com 0,
 * e o mapa global do Chrome indexado por windowNumber apagava end() no fechamento (queda ao fechar a janela). */
static i64 g_next_wnum = 1000;
static i64 w_number(id self, SEL c) {
    (void)c;
    wstate_t *s = ws(self);
    if (!s->number) s->number = __sync_add_and_fetch(&g_next_wnum, 1);
    return s->number;
}
/* NSWindowOcclusionStateVisible = 2 enquanto a janela está na tela (o Chromium não compõe uma janela "coberta"); 0 antes
 * de aparecer e depois de um orderOut:, como no macOS. As duas transições postam NSWindowDidChangeOcclusionStateNotification. */
static u64 w_occlusion(id self, SEL c) { (void)c; return ws(self)->visible ? 2 : 0; }
/* r está na base da janela (origem embaixo à esquerda do content, que no mOS coincide com a base). */
static NSRect w_convert_to_screen(id self, SEL c, NSRect r) {
    (void)c;
    wstate_t *s = ws(self);
    r.o.x += s->frame.o.x;
    r.o.y += s->frame.o.y;
    return r;
}

static id app_windows;
/* Janela-chave, como no Cocoa: -makeKeyAndOrderFront: e -makeKeyWindow de uma janela que aceita ser chave
 * (canBecomeKeyWindow: as popups e dicas do Chromium dizem não) avisam windowDidBecomeMain:/windowDidBecomeKey:, e a
 * chave anterior recebe windowDidResignKey:. É no windowDidBecomeKey: que o Chromium devolve o foco à view web
 * (makeFirstResponder: na RenderWidgetHostViewCocoa); sem isso as teclas iam para a BridgedContentView até o 1º clique. */
static id g_key_window;
static void w_autosave(id self);   /* frameAutosaveName: grava o frame (mais abaixo) */
static void w_become_key(id self) {
    if (g_key_window == self) return;
    extern int cx_ca_diag(void);
    if (cx_responds(self->isa, sel_registerName("canBecomeKeyWindow")) && !MSG0(BOOL, self, "canBecomeKeyWindow")) {
        static int nr;
        if (cx_ca_diag() && nr < 20) { nr++; cx_fdprintf(2, "[ca] janela %d (%s) recusou ser chave\n", ws(self)->gid, cx_class_name(self->isa)); }
        return;
    }
    {   static int nk;   /* diagnóstico (ca-log): quem vira janela-chave (o teclado vai para ela) */
        if (cx_ca_diag() && nk < 30) { nk++; cx_fdprintf(2, "[ca] janela-chave: %d (%s), antes %d\n", ws(self)->gid, cx_class_name(self->isa), g_key_window ? ws(g_key_window)->gid : 0); } }
    static id main_note_name, resign_note_name;
    if (!main_note_name) { main_note_name = cx_str("NSWindowDidBecomeMainNotification"); resign_note_name = cx_str("NSWindowDidResignKeyNotification"); }
    id old = g_key_window;
    g_key_window = self;
    if (old) {
        id od = ws(old)->delegate;
        id n = cx_note(resign_note_name, old, 0);
        if (od && cx_responds(od->isa, sel_registerName("windowDidResignKey:"))) MSG(void, od, "windowDidResignKey:", n);
        cx_post(n);
    }
    id d = ws(self)->delegate;
    id nm = cx_note(main_note_name, self, 0), nk = cx_note(NSWindowDidBecomeKeyNotification, self, 0);
    if (d && cx_responds(d->isa, sel_registerName("windowDidBecomeMain:"))) MSG(void, d, "windowDidBecomeMain:", nm);
    cx_post(nm);
    if (d && cx_responds(d->isa, sel_registerName("windowDidBecomeKey:"))) MSG(void, d, "windowDidBecomeKey:", nk);
    cx_post(nk);
    cx_ax_note_view(self, "AXFocusedWindowChanged");
}
/* Como no Cocoa, a janela filha (addChildWindow:ordered:, ex.: o diálogo "Abrir o Slack?" do Chrome, que é outra
 * NSWindow) fica acima da mãe: quando a mãe volta à frente, as filhas visíveis voltam por cima dela. O window server
 * não conhece filhas; a ordem é refeita aqui. */
static BOOL w_descends(id w, id anc) {   /* w é filha (ou neta...) de anc? */
    for (int d = 0; w && d < 6; d++) { w = ws(w)->parent; if (w && w == anc) return YES; }
    return NO;
}
static void w_raise_children(id self, int depth) {
    nsarr_t *a = (nsarr_t *)app_windows;
    for (u64 i = 0; a && depth < 4 && i < a->n; i++) {
        id ch = a->items[i];
        wstate_t *cs = ws(ch);
        if (ch == self || cs->parent != self || !cs->visible || cs->gid <= 0) continue;
        SYS1(GUI_WIN_FOCUS, cs->gid);
        w_raise_children(ch, depth + 1);
    }
}
static void w_order_in(id self) {
    wstate_t *s = ws(self);
    if (s->gid <= 0) {
        u32 fl = (s->style & 8) ? GUI_WF_RESIZABLE : 0;
        if (w_title_h(self) == 0 && (s->style & 1)) fl |= GUI_WF_FULLSIZE;   /* FullSizeContentView ou sem moldura */
        if (!(s->style & 1)) fl |= GUI_WF_NO_TITLE;                          /* borderless: sem barra nem semáforos */
        long id_ = SYS4(GUI_WIN_CREATE, w_shown_title(self), (long)s->frame.s.w, (long)s->frame.s.h, fl);
        if (id_ <= 0) { cx_fdprintf(2, "AppKit[mOS]: não foi possível criar a janela (%ld)\n", id_); return; }
        s->gid = (int)id_;
        if (objc_getAssociatedObject(self, &k_pos_set)) w_push_frame(self, 0);   /* o app pediu um lugar */
        if (w_level(self, 0)) SYS3(GUI_WIN_FOCUS, s->gid, GUI_FOCUS_LEVEL, w_level(self, 0));
        if (cx_responds(self->isa, sel_registerName("_cxPushLimits"))) MSG0(void, self, "_cxPushLimits");
        w_sync_owner(self);   /* filha registrada (addChildWindow:) antes de ter janela no window server */
        { nsarr_t *aw = (nsarr_t *)app_windows;
          for (u64 i = 0; aw && i < aw->n; i++) if (ws(aw->items[i])->parent == self) w_sync_owner(aw->items[i]); }
        {   /* diagnóstico (ca-log): tamanho pedido ao window server x o que o compositor do app desenha */
            extern int cx_ca_diag(void);
            if (cx_ca_diag()) cx_fdprintf(2, "[ca] janela %d: conteúdo %dx%d estilo 0x%llx%s\n", s->gid, (int)s->frame.s.w,
                                          (int)s->frame.s.h, (unsigned long long)s->style, (fl & GUI_WF_FULLSIZE) ? " (tela cheia)" : "");
        }
        gui_wininfo_t i;
        if (SYS2(GUI_WIN_INFO, s->gid, &i) == 0 && (i.w != (int)s->frame.s.w || i.h != (int)s->frame.s.h)) {
            NSSize old = s->frame.s;
            s->frame.s.w = i.w; s->frame.s.h = i.h;
            (void)old;
            v_set_frame(s->content, 0, r_make(0, 0, i.w, i.h));
        }
        if (!app_windows) app_windows = cx_arr_new(&NSMutableArray_c);
        cx_arr_add(app_windows, self);
        cx_ax_note_view(self, "AXWindowCreated");
    } else {
        if (!s->visible) SYS2(GUI_WIN_FOCUS, s->gid, GUI_FOCUS_SHOW);   /* escondida por orderOut: volta na frente */
        else SYS1(GUI_WIN_FOCUS, s->gid);
        w_raise_children(self, 0);
    }
    s->visible = YES;
    s->closed = NO;
    s->needs = YES;
}
/* NSWindowDidChangeOcclusionStateNotification (e o delegate): a janela apareceu ou sumiu. O Chromium recalcula aí a
 * visibilidade das páginas (WebContentsViewCocoa) e solta ou suspende o compositor da janela. */
static void w_post_occlusion(id self) {
    static id occ;
    if (!occ) occ = cx_str("NSWindowDidChangeOcclusionStateNotification");
    id d = ws(self)->delegate, n = cx_note(occ, self, 0);
    if (d && cx_responds(d->isa, sel_registerName("windowDidChangeOcclusionState:"))) MSG(void, d, "windowDidChangeOcclusionState:", n);
    cx_post(n);
}
/* orderOut: some da tela sem ser destruída (o window server esconde a janela; conteúdo e posição ficam). Antes era
 * um no-op: popups de <select>, dicas e janelas auxiliares do Electron ficavam na tela depois de "escondidas". */
static void w_order_out_core(id self) {
    wstate_t *s = ws(self);
    if (!s->visible) return;
    w_autosave(self);
    s->visible = NO;
    if (s->gid > 0) SYS2(GUI_WIN_FOCUS, s->gid, GUI_FOCUS_HIDE);
    if (g_key_window == self) {
        g_key_window = 0;
        static id resign;
        if (!resign) resign = cx_str("NSWindowDidResignKeyNotification");
        id d = s->delegate, n = cx_note(resign, self, 0);
        if (d && cx_responds(d->isa, sel_registerName("windowDidResignKey:"))) MSG(void, d, "windowDidResignKey:", n);
        cx_post(n);
    }
    w_post_occlusion(self);
}
/* Como no Cocoa, orderFront:/makeKeyAndOrderFront:/orderOut: passam por -orderWindow:relativeTo: (despacho dinâmico),
 * e é ali que as subclasses observam a ordem: a NativeWidgetMacNSWindow do Chromium sobrescreve esse método e só então
 * nota a janela visível e solta o compositor (RecyclableCompositorMac::Unsuspend). Sem isso o cc ficava com o main frame
 * adiado para sempre (Scheduler::SetDeferBeginMainFrame(true), CanDraw "no root layer") e a janela vazia. */
static void w_order_window(id self, SEL c, i64 place, i64 rel) {
    (void)c; (void)rel;
    if (place == 0) { w_order_out_core(self); return; }
    BOOL was = ws(self)->visible;
    w_order_in(self);
    if (!was) w_post_occlusion(self);
}
static void w_show(id self, SEL c, id sender) {
    (void)sender;
    ((void (*)(id, SEL, i64, i64))objc_msgSend)(self, sel_registerName("orderWindow:relativeTo:"), 1, 0);   /* NSWindowAbove */
    if (c == sel_registerName("makeKeyAndOrderFront:") || c == sel_registerName("makeKeyWindow")) w_become_key(self);
    {   /* initialFirstResponder: vira o primeiro respondedor na primeira vez que a janela aparece */
        wstate_t *s = ws(self);
        if (!s->initial_done && s->visible) {
            s->initial_done = YES;
            if (s->initial_fr && (!s->first || s->first == self)) w_make_first(self, 0, s->initial_fr);
        }
    }
}
static void w_order_out(id self, SEL c, id sender) {
    (void)c; (void)sender;
    ((void (*)(id, SEL, i64, i64))objc_msgSend)(self, sel_registerName("orderWindow:relativeTo:"), 0, 0);   /* NSWindowOut */
}
/* ---- geometria da janela no window server (GUI_FOCUS_FRAME/GETFRAME, PR da Camila). O Cocoa conta y de baixo para
 * cima a partir da base da tela principal; o window server conta do topo da tela (com a barra de menus) e usa o canto
 * superior esquerdo do conteúdo. s->frame é o retângulo do conteúdo em coordenadas do Cocoa. */
static double screen_full_h(void) { gui_wininfo_t i = {1440, 876, 1, 1}; SYS1(GUI_SCREEN, &i); return i.h + 24; }
static void w_push_frame(id self, int resize) {
    wstate_t *s = ws(self);
    if (s->gid <= 0) return;
    gui_frame_t f = {(int)s->frame.o.x, (int)(screen_full_h() - (s->frame.o.y + s->frame.s.h)),
                     resize ? (int)s->frame.s.w : 0, resize ? (int)s->frame.s.h : 0};
    SYS3(GUI_WIN_FOCUS, s->gid, GUI_FOCUS_FRAME, &f);
}
/* lê a posição atual (o usuário pode ter arrastado a janela) */
static void w_pull_frame(id self) {
    wstate_t *s = ws(self);
    if (s->gid <= 0) return;
    gui_frame_t f;
    if (SYS3(GUI_WIN_FOCUS, s->gid, GUI_FOCUS_GETFRAME, &f) != 0 || f.w <= 0 || f.h <= 0) return;
    s->frame.o.x = f.x;
    s->frame.o.y = screen_full_h() - f.y - f.h;
}
/* posição pedida pelo app antes de a janela existir: aplicada logo depois do GUI_WIN_CREATE (que centraliza) */
static void mark_pos(id self) { objc_setAssociatedObject(self, &k_pos_set, (id)cx_num_i(1), 1); }
static void w_center(id self, SEL c) {
    (void)c;
    wstate_t *s = ws(self);
    gui_wininfo_t i = {1440, 876, 1, 1};
    SYS1(GUI_SCREEN, &i);
    double fh = s->frame.s.h + w_title_h(self);
    s->frame.o.x = (i.w - s->frame.s.w) / 2;
    /* como no Cocoa: um pouco acima do meio (a sobra de cima é metade da de baixo) */
    double top = (i.h - fh) / 3 + 24;
    s->frame.o.y = screen_full_h() - top - fh;
    mark_pos(self);
    w_push_frame(self, 0);
}
static void w_set_frame_origin(id self, SEL c, NSPoint p) {
    (void)c;
    wstate_t *s = ws(self);
    s->frame.o = p;                      /* a origem da moldura = a do conteúdo (a barra de título fica em cima) */
    mark_pos(self);
    w_push_frame(self, 0);
}
static void w_set_frame_top_left(id self, SEL c, NSPoint p) {
    (void)c;
    wstate_t *s = ws(self);
    s->frame.o.x = p.x;
    s->frame.o.y = p.y - s->frame.s.h - w_title_h(self);
    mark_pos(self);
    w_push_frame(self, 0);
}
static NSPoint w_cascade(id self, SEL c, NSPoint p) {
    (void)c;
    wstate_t *s = ws(self);
    if (p.x == 0 && p.y == 0) { w_pull_frame(self); return (NSPoint){s->frame.o.x + 22, s->frame.o.y + s->frame.s.h + w_title_h(self) - 22}; }
    w_set_frame_top_left(self, 0, p);
    return (NSPoint){p.x + 22, p.y - 22};
}
static void w_set_level(id self, SEL c, i64 level) {
    (void)c;
    objc_setAssociatedObject(self, &k_level, (id)cx_num_i(level), 1);
    if (ws(self)->gid > 0) SYS3(GUI_WIN_FOCUS, ws(self)->gid, GUI_FOCUS_LEVEL, level);
}
static i64 w_level(id self, SEL c) { (void)c; id v = objc_getAssociatedObject(self, &k_level); return v ? MSG0(i64, v, "longLongValue") : 0; }
/* limites do conteúdo para o redimensionamento pelo usuário (appkit_sheets.c guarda min/max e chama aqui) */
void cx_window_push_limits(id self, double min_w, double min_h, double max_w, double max_h) {
    if (ws(self)->gid <= 0) return;
    gui_limits_t l = {(int)min_w, (int)min_h, max_w >= 1e6 ? 0 : (int)max_w, max_h >= 1e6 ? 0 : (int)max_h};
    SYS3(GUI_WIN_FOCUS, ws(self)->gid, GUI_FOCUS_LIMITS, &l);
}
static void w_set_frame(id self, SEL c, NSRect r, BOOL disp) {
    (void)c; (void)disp;
    wstate_t *s = ws(self);
    NSRect in = r;
    r.s.h -= w_title_h(self);
    static int n; geo_log(&n, "setFrame:display:", self, in, r);
    s->frame.o = r.o;
    mark_pos(self);
    if (s->gid <= 0) {
        BOOL changed = s->frame.s.w != r.s.w || s->frame.s.h != r.s.h;
        s->frame = r;
        v_set_frame(s->content, 0, r_make(0, 0, r.s.w, r.s.h));
        if (changed) w_did_resize(self);
        return;
    }
    /* janela na tela: o window server move e redimensiona; a mudança de tamanho volta como GEV_RESIZE */
    BOOL resize = s->frame.s.w != r.s.w || s->frame.s.h != r.s.h;
    NSSize keep = s->frame.s;
    s->frame.s = r.s;
    w_push_frame(self, resize);
    if (resize) s->frame.s = keep;                /* o tamanho só muda quando o GEV_RESIZE chegar */
}
static void w_set_content_size(id self, SEL c, NSSize z) {
    (void)c;
    wstate_t *s = ws(self);
    static int n; geo_log(&n, "setContentSize:", self, r_make(0, 0, z.w, z.h), r_make(0, 0, z.w, z.h));
    if (s->gid <= 0) {
        BOOL changed = s->frame.s.w != z.w || s->frame.s.h != z.h;
        s->frame.s = z;
        v_set_frame(s->content, 0, r_make(0, 0, z.w, z.h));
        if (changed) w_did_resize(self);
        return;
    }
    /* na tela: o alto da janela fica no lugar (como no Cocoa) */
    w_pull_frame(self);
    NSRect old = s->frame;
    s->frame.o.y = old.o.y + old.s.h - z.h;
    s->frame.s = z;
    w_push_frame(self, 1);
    s->frame.s = old.s;
}
static void close_window(id self, BOOL from_wm) {
    wstate_t *s = ws(self);
    if (s->closed) return;
    w_autosave(self);   /* antes de destruir: o frame ainda está no window server */
    s->closed = YES;
    s->visible = NO;
    if (g_key_window == self) g_key_window = 0;
    cx_ax_note_view(self, "AXUIElementDestroyed");
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
    SEL q = sel_registerName("windowShouldClose:");
    /* como no Cocoa: pergunta ao delegate (ou à própria janela, se ela responder) e fecha por -close, que a subclasse
     * pode sobrescrever (o Chrome recusa aqui e fecha as abas antes, chamando -close depois) */
    if (s->delegate && cx_responds(s->delegate->isa, q)) { if (!MSG(BOOL, s->delegate, "windowShouldClose:", self)) return; }
    else if (cx_responds(self->isa, q) && !MSG(BOOL, self, "windowShouldClose:", self)) return;
    MSG(void, self, "close");
}
static void w_display(id self, SEL c) { (void)c; ws(self)->needs = YES; }

static void autolayout_window(id w);
static void v_update_constraints_subtree(id self, SEL c);
/* diagnóstico (ca-log): a árvore de views da janela (classe, frame, oculta), a cada ~15 s nas primeiras vezes.
 * Mostra se uma WebContentsView filha (Electron addChildView) entrou na árvore e com que frame. */
static void dump_view_tree(id v, int depth) {
    if (!v || depth > 12) return;
    vstate_t *s = vs(v);
    cx_fdprintf(2, "[ca] %*s%s (%.0f,%.0f %.0fx%.0f)%s subviews %u\n", depth * 2, "", cx_class_name(object_getClass(v)),
                s->frame.o.x, s->frame.o.y, s->frame.s.w, s->frame.s.h, s->hidden ? " OCULTA" : "", s->nsub);
    for (u32 i = 0; i < s->nsub; i++) dump_view_tree(s->sub[i], depth + 1);
}
static void maybe_dump_tree(id w) {
    extern int cx_ca_diag(void);
    /* por janela (gid): o 1º desenho de cada uma (diálogos e popups do Chrome) e depois 1 a cada 15 s, até 4 */
    static u64 last[16]; static int n[16];
    int g = ws(w)->gid > 0 && ws(w)->gid < 16 ? ws(w)->gid : 0;
    if (!cx_ca_diag() || n[g] >= 4) return;
    u64 now = (u64)SYS0(SYS_TIME);
    if (last[g] && now - last[g] < 15000) return;
    last[g] = now; n[g]++;
    cx_fdprintf(2, "[ca] árvore de views da janela %d (%.0fx%.0f, estilo 0x%llx, %s):\n", ws(w)->gid, ws(w)->frame.s.w, ws(w)->frame.s.h,
                (unsigned long long)ws(w)->style, cx_class_name(object_getClass(w)));
    dump_view_tree(ws(w)->content, 1);
}
/* NSToolbar visível: a barra fica no topo (TB_H) e o conteúdo começa embaixo dela, salvo com
 * NSWindowStyleMaskFullSizeContentView (1 << 15), em que o conteúdo passa por baixo da barra. Os itens
 * são views de verdade dentro da view hospedeira da toolbar (appkit_postico.c, -_mosHostViewWithWidth:):
 * desenho, clique e digitação seguem o caminho de qualquer view, com raiz nela. */
#define TB_H 28
double cx_toolbar_bar_height(id tb);   /* appkit_toolbar.c: 28 pt, ou 52 com ícone e rótulo */
static double host_h(wstate_t *s) { return s->tb_host ? vs(s->tb_host)->frame.s.h : TB_H; }
static id hit(id v, absr_t *a, double x, double y);
static void win_tb_update(id w) {
    wstate_t *s = ws(w);
    id tb = cx_responds(w->isa, sel_registerName("toolbar")) ? MSG0(id, w, "toolbar") : 0, host = 0;
    if (tb && cx_responds(tb->isa, sel_registerName("isVisible")) && !MSG0(BOOL, tb, "isVisible")) tb = 0;
    if (tb && cx_responds(tb->isa, sel_registerName("_mosHostViewWithWidth:")))
        host = MSG(id, tb, "_mosHostViewWithWidth:", s->frame.s.w);
    s->tb_host = host && cx_is_subclass(host->isa, &NSView_c) ? host : 0;
    double th = tb ? cx_toolbar_bar_height(tb) : TB_H;
    s->tb_top = s->tb_host && !(s->style & (1ull << 15)) ? th : 0;
    if (s->tb_host) {
        vs(s->tb_host)->frame = r_make(0, 0, s->frame.s.w, th);
        if (vs(s->tb_host)->window != w) set_window_rec(s->tb_host, w);
    }
}
/* view sob (x, y) da janela (origem em cima à esquerda): a faixa da toolbar tem a raiz própria */
static id win_hit_root(id w, double x, double y) {
    wstate_t *s = ws(w);
    if (s->tb_host && y >= 0 && y < host_h(s)) {
        absr_t ta = {0, 0, s->frame.s.w, host_h(s), is_flipped(s->tb_host)};
        id v = hit(s->tb_host, &ta, x, y);
        return v ? v : s->tb_host;
    }
    if (!s->content) return 0;
    absr_t a = {0, s->tb_top, s->frame.s.w, s->frame.s.h - s->tb_top, is_flipped(s->content)};
    id v = hit(s->content, &a, x, y);
    return v ? v : s->content;
}

static void render_window(id w) {
    wstate_t *s = ws(w);
    if (s->gid <= 0 || !s->visible) return;
    maybe_dump_tree(w);
    win_tb_update(w);
    double ch = s->frame.s.h - s->tb_top;
    if (ch < 0) ch = 0;
    if (vs(s->content)->frame.s.w != s->frame.s.w || vs(s->content)->frame.s.h != ch)
        v_set_frame(s->content, 0, r_make(0, 0, s->frame.s.w, ch));   /* com autoresizing das subviews */
    vs(s->content)->frame = r_make(0, 0, s->frame.s.w, ch);
    autolayout_window(w);
    ncmd = 0;
    u32 bg = argb(s->bg);
    put(GC_CLEAR, &bg, 4);
    absr_t a = {0, s->tb_top, s->frame.s.w, ch, is_flipped(s->content)};
    clip_x0 = clip_y0 = -1e9; clip_x1 = clip_y1 = 1e9;
    vs(s->content)->frame = r_make(0, 0, s->frame.s.w, ch);
    cx_gui_cur_gid = s->gid;                     /* gancho QuartzCore */
    draw_view_tree(s->content, &a);
    if (s->tb_host) {                            /* a barra por cima, com os itens que são views */
        absr_t ta = {0, 0, s->frame.s.w, host_h(s), is_flipped(s->tb_host)};
        clip_x0 = clip_y0 = -1e9; clip_x1 = clip_y1 = 1e9;
        draw_view_tree(s->tb_host, &ta);
    } else if (cx_responds(w->isa, sel_registerName("toolbar"))) {
        id tb = MSG0(id, w, "toolbar");
        if (tb && cx_responds(tb->isa, sel_registerName("isVisible")) && MSG0(BOOL, tb, "isVisible")
            && cx_responds(tb->isa, sel_registerName("drawInRect:"))) {
            ctx_t save = ctx;
            ctx.tlx = 0; ctx.tly = 0; ctx.w = s->frame.s.w; ctx.h = s->frame.s.h; ctx.flipped = 0; ctx.fill = 0xFF000000;
            ((void (*)(id, SEL, NSRect))objc_msgSend)(tb, sel_registerName("drawInRect:"), r_make(0, s->frame.s.h - cx_toolbar_bar_height(tb), s->frame.s.w, cx_toolbar_bar_height(tb)));
            ctx = save;
        }
    }
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
typedef struct { u64 type; double x, y; u64 mods; u16 keycode; id chars; i64 clicks; id window; double dy;
                 double ts; i64 wnum, d1, d2; short subtype; double dx;
                 gui_event_t g; u8 has_g; u8 repeat; } evstate_t;   /* repeat: tecla mantida (isARepeat) */   /* g: o evento do window server de origem (sendEvent:) */
static evstate_t *evs(id e) { return ST(e); }
static u8 g_key_held[128];       /* tecla (código do Mac) apertada sem keyUp ainda */
static double g_key_time[128];
static BOOL ev_is_repeat(id self, SEL c) { (void)c; return evs(self)->repeat; }
/* o Chromium lê estes em todo evento de teclado/mouse (ui/events/cocoa); os que devolvem double/float não podem cair no
 * "ainda não implementado" (que só zera rax: xmm0 voltaria com lixo) */
u64 mach_absolute_time(void);
static double ev_ts(id self, SEL c) { (void)c; return evs(self)->ts; }
static i64 ev_wnum(id self, SEL c) { (void)c; return evs(self)->wnum; }
static short ev_subtype(id self, SEL c) { (void)c; return evs(self)->subtype; }
static i64 ev_data1(id self, SEL c) { (void)c; return evs(self)->d1; }
static i64 ev_data2(id self, SEL c) { (void)c; return evs(self)->d2; }
static i64 ev_button(id self, SEL c) {
    (void)c;
    u64 t = evs(self)->type;
    return t == 3 || t == 4 || t == 7 ? 1 : t == 25 || t == 26 || t == 27 ? 2 : 0;
}
static float ev_pressure(id self, SEL c) {
    (void)c;
    u64 t = evs(self)->type;
    return t == 1 || t == 3 || t == 6 || t == 7 || t == 25 || t == 27 ? 1.0f : 0.0f;
}
static float ev_fzero(id self, SEL c) { (void)self; (void)c; return 0; }
static i64 ev_izero(id self, SEL c) { (void)self; (void)c; return 0; }
static id ev_other(id cls, SEL c, u64 type, NSPoint loc, u64 mods, double ts, i64 wnum, id ctx, short subtype, i64 d1, i64 d2) {
    (void)cls; (void)c; (void)ctx;
    id e = objc_autorelease(cx_alloc_instance(&NSEvent_c));
    evstate_t *s = calloc(1, sizeof *s);
    ST(e) = s;
    s->type = type; s->x = loc.x; s->y = loc.y; s->mods = mods;
    s->ts = ts > 0 ? ts : (double)mach_absolute_time() / 1e9;
    s->wnum = wnum; s->subtype = subtype; s->d1 = d1; s->d2 = d2;
    return e;
}
static u64 ev_type(id self, SEL c) { (void)c; return evs(self)->type; }
static NSPoint ev_loc(id self, SEL c) { (void)c; NSPoint p = {evs(self)->x, evs(self)->y}; return p; }
static u64 ev_mods(id self, SEL c) { (void)c; return evs(self)->mods; }
static u16 ev_keycode(id self, SEL c) { (void)c; return evs(self)->keycode; }
static id ev_chars(id self, SEL c) { (void)c; return evs(self)->chars ? evs(self)->chars : cx_str(""); }
static i64 ev_clicks(id self, SEL c) { (void)c; return evs(self)->clicks; }
static id ev_window(id self, SEL c) { (void)c; return evs(self)->window; }
static double ev_dy(id self, SEL c) { (void)c; return evs(self)->dy; }
static double ev_zero(id self, SEL c) { (void)self; (void)c; return 0; }
static double ev_dx(id self, SEL c) { (void)c; return evs(self)->dx; }
static u64 g_mods;
static u64 g_last_flags;
static u64 ev_cmods(id cls, SEL c) { (void)cls; (void)c; return g_mods; }
/* +[NSEvent pressedMouseButtons] e +[NSEvent mouseLocation], do último evento de mouse: o Chromium marca
 * kLeftButtonDown nos modificadores do evento pelo primeiro (o Blink tira daí o "buttons" e o pointerdown) e usa
 * o segundo para saber se o cursor está na janela. Bit 0 = esquerdo, 1 = direito. */
static u64 g_pressed_buttons;
static NSPoint g_mouse_screen;
static u64 ev_pressed_buttons(id cls, SEL c) { (void)cls; (void)c; return g_pressed_buttons; }
static NSPoint ev_mouse_location(id cls, SEL c) { (void)cls; (void)c; return g_mouse_screen; }

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
/* keyCode do flagsChanged: no macOS (Carbon / NSEvent). */
static u16 mac_mod_keycode(int k) {
    switch (k) {
    case GUI_KEY_LSHIFT: return 56;
    case GUI_KEY_RSHIFT: return 60;
    case GUI_KEY_LCTRL: return 59;
    case GUI_KEY_RCTRL: return 62;
    case GUI_KEY_LALT: return 58;
    case GUI_KEY_RALT: return 61;
    case GUI_KEY_LCMD: return 55;
    case GUI_KEY_RCMD: return 54;
    case GUI_KEY_CAPS: return 57;
    default: return 0x7F;
    }
}
static id g_current_event;
static void cx_set_current_event(id e) { if (e == g_current_event) return; objc_retain(e); if (g_current_event) objc_release(g_current_event); g_current_event = e; }
static id make_event(u64 type, id win, double px, double py, gui_event_t *g) {
    id e = objc_autorelease(cx_alloc_instance(&NSEvent_c));
    evstate_t *s = calloc(1, sizeof *s);
    ST(e) = s;
    s->type = type;
    s->window = win;
    s->wnum = win ? MSG0(i64, win, "windowNumber") : 0;
    s->x = px;
    s->y = win ? win_h(win) - py : py;
    s->mods = mac_mods(g->mods);
    s->clicks = g->clicks;
    s->dy = -g->w;
    s->dx = -g->h;   /* GEV_SCROLL: h > 0 = para a direita; no Cocoa deltaX > 0 rola para a esquerda (mesmo sinal do dy) */
    if (g->type == GEV_SCROLL && !g->h && (s->mods & (1u << 17))) { s->dx = s->dy; s->dy = 0; }   /* ⇧ + roda = horizontal, como no Mac */
    s->ts = (double)mach_absolute_time() / 1e9;
    if (g) { s->g = *g; s->has_g = 1; }
    cx_set_current_event(e);
    if (type == 10 || type == 11 || type == 12) {          /* keyDown / keyUp / flagsChanged */
        s->keycode = type == 12 ? mac_mod_keycode(g->key) : mac_keycode(g->key);
        if (type == 12) return e;
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
    static SEL ht;
    if (!ht) ht = sel_registerName("hitTest:");
    for (u32 i = s->nsub; i-- > 0;) {
        id cv = s->sub[i];
        vstate_t *cs = vs(cv);
        /* subview que sobrescreve -hitTest: (o Chromium: a ChromeNodeDragOverlayContainerView cobre a janela toda e
         * devolve nil fora de um arrasto, para o clique chegar à RenderWidgetHostViewCocoa): ela decide, com o ponto
         * nas coordenadas desta view (a superview dela), como no AppKit */
        if (cx_lookup(cv->isa, ht) != (IMP)v_hit_test) {
            if (vs(cv)->hidden) continue;
            NSPoint p = {x - a->tlx, a->flipped ? y - a->tly : a->h - (y - a->tly)};
            id r = ((id (*)(id, SEL, NSPoint))objc_msgSend)(cv, ht, p);
            if (r) return r;
            continue;
        }
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
static BOOL ctl_is_continuous_s(vstate_t *s) { return s->cont == 1 || (s->cont == 0 && s->kind == K_SLIDER); }
static void ctl_set_continuous(id self, SEL c, BOOL v) { (void)c; vs(self)->cont = v ? 1 : 2; }
static BOOL ctl_is_continuous(id self, SEL c) { (void)c; return ctl_is_continuous_s(vs(self)); }
static void ctl_mouse_down(id self, SEL c, id ev) {
    (void)c;
    vstate_t *s = vs(self);
    if (!s->enabled) return;
    if (s->kind == K_BUTTON) { s->highlighted = YES; mark(self); }
    else if (s->kind == K_SLIDER) {
        absr_t a;
        abs_rect(self, &a);
        s->dval = slider_value_at(self, evs(ev)->x);
        if (ctl_is_continuous_s(s)) send_action(self);   /* não contínuo: a ação sai só ao soltar */
        mark(self);
    } else if (s->kind == K_TEXT && s->editable) {
        id w = s->window;
        BOOL was_first = w && ws(w)->first == self;
        if (w) w_make_first(w, 0, self);
        const char *t = cx_cstr(s->value ? s->value : cx_str(""));
        int n = (int)strlen(t);
        if (!was_first) { s->anchor = 0; s->caret = n; s->has_anchor = n > 0; mark(self); return; }   /* 1º clique seleciona tudo, como o Cocoa */
        if (evs(ev)->clicks >= 2) { s->anchor = 0; s->caret = n; s->has_anchor = YES; mark(self); return; }
        absr_t ar;
        abs_rect(self, &ar);
        double size = s->font ? fnt(s->font)->size : 13;
        int wt = s->font ? fnt(s->font)->weight : 0;
        double px = evs(ev)->x - ar.tlx - (s->bezeled ? 5 : 2);
        int best = 0;
        if (!cx_is_secure_field(self))
            for (int k = 0; k <= n; k = tf_next(t, n, k)) { if (measure(t, (u32)k, size, wt).width <= px + 3) best = k; else break; if (k == n) break; }
        else { double bw = measure("\xE2\x80\xA2", 3, size, wt).width; int idx = bw > 0 ? (int)((px + bw / 2) / bw) : 0; for (int k = 0, c2 = 0; k <= n; k = tf_next(t, n, k), c2++) { if (c2 <= idx) best = k; if (k == n) break; } }
        if (evs(ev)->mods & (1u << 17)) { if (!s->has_anchor) { s->anchor = s->caret; s->has_anchor = YES; } }
        else s->has_anchor = NO;
        s->caret = best;
        mark(self);
    }
}
static void ctl_mouse_dragged(id self, SEL c, id ev) {
    (void)c;
    vstate_t *s = vs(self);
    if (s->kind == K_SLIDER && s->enabled) { s->dval = slider_value_at(self, evs(ev)->x); if (ctl_is_continuous_s(s)) send_action(self); mark(self); }
}
static void ctl_mouse_up(id self, SEL c, id ev) {
    (void)c;
    vstate_t *s = vs(self);
    if (s->kind == K_SLIDER && s->enabled && !ctl_is_continuous_s(s)) { send_action(self); return; }
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
static id pb_general(id cls, SEL c);
static i64 pb_clear(id self, SEL c);
static BOOL pb_set_string(id self, SEL c, id s, id type);
static id pb_string(id self, SEL c, id type);
extern id NSPasteboardTypeString;
/* seleção do campo: [anchor, caret) quando has_anchor */
static int tf_sel(vstate_t *s, int *a, int *b) {
    if (!s->has_anchor || s->anchor == s->caret) return 0;
    *a = s->anchor < s->caret ? s->anchor : s->caret;
    *b = s->anchor < s->caret ? s->caret : s->anchor;
    return 1;
}
/* troca [a, b) por ins (newlines viram espaço: o campo é de uma linha) */
static void tf_replace(vstate_t *s, int a, int b, const char *ins) {
    const char *t = cx_cstr(s->value ? s->value : cx_str(""));
    int n = (int)strlen(t);
    if (a < 0) a = 0;
    if (b > n) b = n;
    if (b < a) b = a;
    size_t il = strlen(ins);
    char *buf = malloc((size_t)n - (size_t)(b - a) + il + 1);
    if (!buf) return;
    memcpy(buf, t, (size_t)a);
    for (size_t k = 0; k < il; k++) buf[a + k] = (ins[k] == '\n' || ins[k] == '\r') ? ' ' : ins[k];
    strcpy(buf + a + il, t + b);
    keep(&s->value, cx_str(buf));
    free(buf);
    s->caret = a + (int)il;
    s->has_anchor = NO;
}
static int tf_prev(const char *t, int k) { if (k <= 0) return 0; k--; while (k > 0 && ((u8)t[k] & 0xC0) == 0x80) k--; return k; }
static int tf_next(const char *t, int n, int k) { if (k >= n) return n; k++; while (k < n && ((u8)t[k] & 0xC0) == 0x80) k++; return k; }
static void tf_move(vstate_t *s, int to, BOOL shift) {
    if (shift) { if (!s->has_anchor) { s->anchor = s->caret; s->has_anchor = YES; } }
    else s->has_anchor = NO;
    s->caret = to;
}
/* cadeia de teclado (mais abaixo) */
static void w_select_next_kv(id self, SEL c, id sender);
static void w_select_prev_kv(id self, SEL c, id sender);
static BOOL kv_tab_key(id view, id ev);
static void tf_key(id self, id ev) {
    vstate_t *s = vs(self);
    evstate_t *e = evs(ev);
    const char *t = cx_cstr(s->value ? s->value : cx_str(""));
    int n = (int)strlen(t);
    if (s->caret > n) s->caret = n;
    if (s->anchor > n) s->anchor = n;
    const char *ch = e->chars ? cx_cstr(e->chars) : "";
    BOOL cmd = (e->mods & (1u << 20)) != 0, shift = (e->mods & (1u << 17)) != 0, opt = (e->mods & (1u << 19)) != 0;
    int a, b;
    BOOL sel = tf_sel(s, &a, &b);
    BOOL secure = cx_is_secure_field(self);
    if (cmd) {
        char k = ch[0] >= 'A' && ch[0] <= 'Z' ? (char)(ch[0] + 32) : ch[0];
        if (e->keycode == 123) { tf_move(s, 0, shift); mark(self); return; }
        if (e->keycode == 124) { tf_move(s, n, shift); mark(self); return; }
        if (e->keycode == 51) { if (sel) tf_replace(s, a, b, ""); else tf_replace(s, 0, s->caret, ""); goto changed; }
        switch (k) {
        case 'a': s->anchor = 0; s->has_anchor = YES; s->caret = n; mark(self); return;
        case 'c': case 'x': {
            if (!sel || secure) { if (secure) NSBeep(); return; }   /* o campo seguro não entrega a senha */
            id g = pb_general(0, 0);
            pb_clear(g, 0);
            pb_set_string(g, 0, cx_str_n(t + a, (usize)(b - a)), NSPasteboardTypeString);
            if (k == 'c') return;
            tf_replace(s, a, b, "");
            goto changed;
        }
        case 'v': {
            id str = pb_string(pb_general(0, 0), 0, NSPasteboardTypeString);
            if (!str) return;
            if (sel) tf_replace(s, a, b, cx_cstr(str)); else tf_replace(s, s->caret, s->caret, cx_cstr(str));
            goto changed;
        }
        }
        return;
    }
    if (e->keycode == 51) {                       /* apagar */
        if (sel) tf_replace(s, a, b, "");
        else if (s->caret > 0) tf_replace(s, opt ? 0 : tf_prev(t, s->caret), s->caret, "");
        else return;
    } else if (e->keycode == 117) {               /* apagar para frente */
        if (sel) tf_replace(s, a, b, "");
        else if (s->caret < n) tf_replace(s, s->caret, tf_next(t, n, s->caret), "");
        else return;
    } else if (e->keycode == 123) {
        if (sel && !shift) { s->caret = a; s->has_anchor = NO; } else tf_move(s, tf_prev(t, s->caret), shift);
        mark(self); return;
    } else if (e->keycode == 124) {
        if (sel && !shift) { s->caret = b; s->has_anchor = NO; } else tf_move(s, tf_next(t, n, s->caret), shift);
        mark(self); return;
    } else if (e->keycode == 115 || e->keycode == 126) { tf_move(s, 0, shift); mark(self); return; }
    else if (e->keycode == 119 || e->keycode == 125) { tf_move(s, n, shift); mark(self); return; }
    else if (e->keycode == 36 || e->keycode == 76) { send_action(self); return; }
    else if (e->keycode == 48 && !cmd) {          /* Tab / ⇧Tab: fim da edição (NSTabTextMovement/NSBacktabTextMovement) */
        id d = s->delegate;
        if (d && cx_responds(d->isa, sel_registerName("controlTextDidEndEditing:"))) {
            id info = MSG(id, (id)cx_class_named("NSDictionary"), "dictionaryWithObject:forKey:", cx_num_i(shift ? 0x12 : 0x11), cx_str("NSTextMovement"));
            MSG(void, d, "controlTextDidEndEditing:", cx_note(cx_str("NSControlTextDidEndEditingNotification"), self, info));
        }
        if (s->window) { if (shift) w_select_prev_kv(s->window, 0, 0); else w_select_next_kv(s->window, 0, 0); }
        return;
    }
    else if (ch[0] && (u8)ch[0] >= 32 && (u8)ch[0] != 127) {
        if (sel) tf_replace(s, a, b, ch); else tf_replace(s, s->caret, s->caret, ch);
    } else return;
changed:
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
    g_dark_draw = cx_appearance_dark();
    accent = cx_appearance_accent();
    if (s->draws_bg) g_fill(a->tlx, a->tly, a->w, a->h, s->bg_color ? argb(s->bg_color) : D(0xFFFFFFFF, 0xFF1E1E1E));
    const char *t = cx_cstr(s->value ? s->value : cx_str(""));
    tx_line_t *L;
    u32 n = tx_lines(t, a->w - 2 * TV_INSET, size, wt, &L);
    gui_metrics_t m = measure("Ag", 2, size, wt);
    double lh = tx_line_h(v), y = a->tly + TV_INSET;
    u32 tc = s->text_color ? argb(s->text_color) : INK(0xE6);
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
    cx_ax_note_view(self, "AXValueChanged");
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
    cx_ax_note_view(self, "AXValueChanged");
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
/* Cada pasteboard tem uma lista de NSPasteboardItem (arrastar 3 arquivos = 3 itens, como no Cocoa); as chamadas de
 * um tipo só (setString:forType:, dataForType:...) usam o 1º item. Tipos legados viram a UTI (appkit_pb_core.c, teste
 * userland/tests-appkit/pasteboard_host.c); NSFilenamesPboardType é a lista dos public.file-url dos itens. */
#include "appkit_pb_core.h"
typedef struct { id name; i64 count; id list; id declared; id owner; char *disk; int used; } pbstate_t;
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
static id pb_self(pbstate_t *p) { return pb_obj[p - pb_tab]; }
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
static id canon(id type) { if (!type || !cx_is_str(type)) return type; const char *c = pbk_canonical(cx_cstr(type)); return c == cx_cstr(type) ? type : cx_str(c); }

/* ---- NSPasteboardItem: dicionário tipo → valor (ivar logo depois do isa) + provedor de dados por tipo */
typedef struct { void *isa; id items; } pbitem_t;
static char k_pi_prov, k_pi_ptypes, k_pi_pb;
static id pi_dict(id self) {
    pbitem_t *it = (pbitem_t *)self;
    if (!it->items) it->items = objc_retain(MSG0(id, pb_cls("NSMutableDictionary"), "dictionary"));
    return it->items;
}
static id pi_init(id self, SEL c) { (void)c; pi_dict(self); return self; }
static BOOL pi_set(id self, SEL c, id v, id type) { (void)c; if (!type || !v) return NO; MSG(void, pi_dict(self), "setObject:forKey:", v, canon(type)); return YES; }
static id pi_find(id self, id type) {
    id d = pi_dict(self);
    id v = MSG(id, d, "objectForKey:", canon(type));
    return v;
}
static id pi_get(id self, id type) {
    if (!type) return 0;
    id v = pi_find(self, type);
    if (v) return v;
    id prov = objc_getAssociatedObject(self, &k_pi_prov), pt = objc_getAssociatedObject(self, &k_pi_ptypes);
    if (!prov || !pt) return 0;
    nsarr_t *a = (nsarr_t *)pt;
    for (u64 i = 0; i < a->n; i++) {
        if (!pbk_same(cx_cstr(a->items[i]), cx_cstr(type))) continue;
        if (pb_resp(prov, "pasteboard:item:provideDataForType:"))   /* sob demanda, como o Cocoa */
            MSG(void, prov, "pasteboard:item:provideDataForType:", objc_getAssociatedObject(self, &k_pi_pb), self, a->items[i]);
        return pi_find(self, type);
    }
    return 0;
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
static id pi_data(id self, SEL c, id type) { (void)c; return pb_as_data(pi_get(self, type)); }
static id pi_string(id self, SEL c, id type) { (void)c; return pb_as_string(pi_get(self, type)); }
static id pi_plist(id self, SEL c, id type) { (void)c; return pi_get(self, type); }
static id pi_types(id self, SEL c) {
    (void)c;
    id a = MSG0(id, MSG0(id, pi_dict(self), "allKeys"), "mutableCopy");
    id pt = objc_getAssociatedObject(self, &k_pi_ptypes);
    nsarr_t *t = (nsarr_t *)pt;
    for (u64 i = 0; pt && i < t->n; i++) { id x = canon(t->items[i]); if (!MSG(BOOL, a, "containsObject:", x)) cx_arr_add(a, x); }
    return objc_autorelease(a);
}
static id pi_available(id self, SEL c, id want) {
    (void)c;
    nsarr_t *w = (nsarr_t *)want;
    id have = pi_types(self, 0);
    for (u64 i = 0; want && i < w->n; i++) if (MSG(BOOL, have, "containsObject:", canon(w->items[i]))) return w->items[i];
    return 0;
}
static BOOL pi_set_provider(id self, SEL c, id prov, id types) {
    (void)c;
    if (!prov || !types) return NO;
    objc_setAssociatedObject(self, &k_pi_prov, prov, 1);
    objc_setAssociatedObject(self, &k_pi_ptypes, types, 1);
    return YES;
}
static id pi_new(void) { return objc_autorelease(pi_init(cx_alloc_instance(&NSPasteboardItem_c), 0)); }
static BOOL pi_empty(id it) { return !MSG0(u64, pi_dict(it), "count") && !objc_getAssociatedObject(it, &k_pi_ptypes); }

/* ---- estado do pasteboard */
static id pb_list(pbstate_t *p) { if (!p->list) p->list = objc_retain(cx_arr_new(&NSMutableArray_c)); return p->list; }
static id pb_first(pbstate_t *p, BOOL make) {
    id l = pb_list(p);
    if (((nsarr_t *)l)->n) return ((nsarr_t *)l)->items[0];
    if (!make) return 0;
    id it = pi_new();
    objc_setAssociatedObject(it, &k_pi_pb, pb_self(p), 0);
    cx_arr_add(l, it);
    return it;
}
static id pb_dict(pbstate_t *p) { return pi_dict(pb_first(p, YES)); }
static void pb_remember(pbstate_t *p, const char *t, usize n) {
    free(p->disk);
    p->disk = malloc(n + 1);
    if (p->disk) { memcpy(p->disk, t, n); p->disk[n] = 0; }
}
static void pb_reset(pbstate_t *p) {
    MSG0(void, pb_list(p), "removeAllObjects");
    objc_release(p->declared);
    p->declared = 0;
    p->owner = 0;
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
        pb_reset(p);
        if (n) MSG(void, pb_dict(p), "setObject:forKey:", cx_str_n((const char *)b, n), NSPasteboardTypeString);
        p->count++;
    }
    free(b);
}
static void pb_store(pbstate_t *p, id v, id type) {
    if (!type) type = NSPasteboardTypeString;
    type = canon(type);
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
    pb_reset(p);
    if (pb_is_general(p)) { pb_write("", 0); pb_remember(p, "", 0); }
    return ++p->count;
}
static void pb_prepare(id self, SEL c, u64 opts) { (void)opts; pb_clear(self, c); }
static i64 pb_declare(id self, SEL c, id types, id owner) {
    i64 n = pb_clear(self, c);
    pbstate_t *p = pb_st(self);
    p->owner = owner;
    p->declared = types ? MSG0(id, types, "copy") : 0;   /* copy já vem com +1 */
    return n;
}
static i64 pb_add_types(id self, SEL c, id types, id owner) {
    (void)c;
    pbstate_t *p = pb_st(self);
    if (owner) p->owner = owner;
    if (types) {
        id a = p->declared ? MSG0(id, p->declared, "mutableCopy") : cx_arr_new(&NSMutableArray_c);
        nsarr_t *t = (nsarr_t *)types;
        for (u64 i = 0; i < t->n; i++) if (!MSG(BOOL, a, "containsObject:", t->items[i])) cx_arr_add(a, t->items[i]);
        objc_release(p->declared);
        p->declared = a;
    }
    return p->count;
}
static void pb_release(id self, SEL c) { (void)self; (void)c; }
/* lista de caminhos dos public.file-url de todos os itens (NSFilenamesPboardType) */
static id pb_filenames(pbstate_t *p) {
    id out = cx_arr_new(&NSMutableArray_c);
    nsarr_t *l = (nsarr_t *)pb_list(p);
    for (u64 i = 0; i < l->n; i++) {
        id u = pb_as_string(pi_get(l->items[i], cx_str("public.file-url")));
        char path[2048];
        if (u && pbk_url_to_path(cx_cstr(u), path, sizeof path)) cx_arr_add(out, cx_str(path));
    }
    return objc_autorelease(out);
}
static id pb_value(id self, id type) {
    pbstate_t *p = pb_st(self);
    pb_sync(p);
    if (!type) return 0;
    if (pbk_is_filenames(cx_cstr(type))) { id a = pb_filenames(p); return ((nsarr_t *)a)->n ? a : 0; }
    nsarr_t *l = (nsarr_t *)pb_list(p);
    for (u64 i = 0; i < l->n; i++) { id v = pi_get(l->items[i], type); if (v) return v; }
    /* tipo declarado e ainda sem dado: o dono fornece sob demanda (pasteboard:provideDataForType:) */
    if (pb_resp(p->owner, "pasteboard:provideDataForType:")) {
        MSG(void, p->owner, "pasteboard:provideDataForType:", self, type);
        if (l->n) return pi_find(l->items[0], type);
    }
    return 0;
}
static BOOL pb_set_data(id self, SEL c, id d, id type) { (void)c; pb_store(pb_st(self), d, type); return YES; }
static BOOL pb_set_string(id self, SEL c, id s, id type) { (void)c; if (!s) return NO; pb_store(pb_st(self), s, type); return YES; }
static BOOL pb_set_plist(id self, SEL c, id pl, id type) {
    (void)c;
    pbstate_t *p = pb_st(self);
    if (type && pbk_is_filenames(cx_cstr(type)) && pl && pb_resp(pl, "count")) {   /* um item public.file-url por caminho */
        nsarr_t *a = (nsarr_t *)pl;
        id first = pb_first(p, NO);
        if (first && pi_empty(first)) MSG(void, pb_list(p), "removeObject:", first);
        for (u64 i = 0; i < a->n; i++) {
            char url[4096];
            if (!cx_is_str(a->items[i]) || !pbk_path_to_url(cx_cstr(a->items[i]), url, sizeof url)) continue;
            id it = pi_new();
            objc_setAssociatedObject(it, &k_pi_pb, self, 0);
            pi_set(it, 0, cx_str(url), cx_str("public.file-url"));
            cx_arr_add(pb_list(p), it);
        }
        return YES;
    }
    pb_store(p, pl, type);
    return YES;
}
static id pb_data(id self, SEL c, id type) { (void)c; return pb_as_data(pb_value(self, type)); }
static id pb_string(id self, SEL c, id type) { (void)c; return pb_as_string(pb_value(self, type ? type : NSPasteboardTypeString)); }
static id pb_plist(id self, SEL c, id type) { (void)c; return pb_value(self, type); }
static id pb_types(id self, SEL c) {
    (void)c;
    pbstate_t *p = pb_st(self);
    pb_sync(p);
    id a = cx_arr_new(&NSMutableArray_c);
    nsarr_t *l = (nsarr_t *)pb_list(p);
    for (u64 i = 0; i < l->n; i++) {
        nsarr_t *t = (nsarr_t *)pi_types(l->items[i], 0);
        for (u64 j = 0; j < t->n; j++) if (!MSG(BOOL, a, "containsObject:", t->items[j])) cx_arr_add(a, t->items[j]);
    }
    nsarr_t *d = (nsarr_t *)p->declared;
    for (u64 i = 0; d && i < d->n; i++) { id x = canon(d->items[i]); if (!MSG(BOOL, a, "containsObject:", x)) cx_arr_add(a, x); }
    /* nomes legados que apps antigos procuram em -types */
    static const char *leg[][2] = {{"public.file-url", "NSFilenamesPboardType"}, {"public.url", "Apple URL pasteboard type"},
        {"public.rtf", "NeXT Rich Text Format v1.0 pasteboard type"}, {"public.tiff", "NeXT TIFF v4.0 pasteboard type"},
        {"public.html", "Apple HTML pasteboard type"}, {"com.adobe.pdf", "Apple PDF pasteboard type"}};
    for (unsigned k = 0; k < sizeof leg / sizeof leg[0]; k++)
        if (MSG(BOOL, a, "containsObject:", cx_str(leg[k][0])) && !MSG(BOOL, a, "containsObject:", cx_str(leg[k][1]))) cx_arr_add(a, cx_str(leg[k][1]));
    return objc_autorelease(a);
}
static id pb_available(id self, SEL c, id want) {
    (void)c;
    nsarr_t *w = (nsarr_t *)want;
    id have = pb_types(self, 0);
    for (u64 i = 0; want && i < w->n; i++)
        if (MSG(BOOL, have, "containsObject:", w->items[i]) || MSG(BOOL, have, "containsObject:", canon(w->items[i]))) return w->items[i];
    return 0;
}
static BOOL pb_can_conform(id self, SEL c, id want) { return pb_available(self, c, want) != 0; }

/* ---- writeObjects: um item por objeto (NSPasteboardWriting) */
static id item_for_object(id pbo, id o) {
    if (cx_is_subclass(object_getClass(o), &NSPasteboardItem_c)) { objc_setAssociatedObject(o, &k_pi_pb, pbo, 0); return o; }
    id it = pi_new();
    objc_setAssociatedObject(it, &k_pi_pb, pbo, 0);
    if (cx_is_str(o)) { pi_set(it, 0, o, NSPasteboardTypeString); return it; }
    if (pb_resp(o, "absoluteString") && pb_resp(o, "isFileURL")) {
        id u = MSG0(id, o, "absoluteString");
        if (MSG0(BOOL, o, "isFileURL")) pi_set(it, 0, u, cx_str("public.file-url"));
        pi_set(it, 0, u, cx_str("public.url"));
        pi_set(it, 0, u, NSPasteboardTypeString);
        return it;
    }
    if (pb_resp(o, "writableTypesForPasteboard:") && pb_resp(o, "pasteboardPropertyListForType:")) {   /* protocolo do app */
        id types = MSG(id, o, "writableTypesForPasteboard:", pbo);
        nsarr_t *t = (nsarr_t *)types;
        for (u64 i = 0; types && i < t->n; i++) { id v = MSG(id, o, "pasteboardPropertyListForType:", t->items[i]); if (v) pi_set(it, 0, v, t->items[i]); }
        return it;
    }
    if (pb_resp(o, "string") && pb_resp(o, "attributesAtIndex:effectiveRange:")) {   /* NSAttributedString */
        id str = MSG0(id, o, "string");
        if (str) pi_set(it, 0, str, NSPasteboardTypeString);
        if (pb_resp(o, "RTFFromRange:documentAttributes:")) {
            u64 len = MSG0(u64, o, "length");
            typedef struct { u64 loc, len; } rg_t;
            id rtf = ((id (*)(id, SEL, rg_t, id))objc_msgSend)(o, sel_registerName("RTFFromRange:documentAttributes:"), (rg_t){0, len}, (id)0);
            if (rtf) pi_set(it, 0, rtf, cx_str("public.rtf"));
        }
        return it;
    }
    if (cx_is_subclass(object_getClass(o), &NSImage_c)) {
        id d = pb_resp(o, "TIFFRepresentation") ? MSG0(id, o, "TIFFRepresentation") : 0;
        if (d) pi_set(it, 0, d, cx_str("public.tiff"));
        pi_set(it, 0, o, cx_str("com.apple.mos.nsimage"));      /* no mesmo processo: o objeto */
        return it;
    }
    if (cx_is_subclass(object_getClass(o), &NSColor_c)) { pi_set(it, 0, o, cx_str("com.apple.cocoa.pasteboard.color")); return it; }
    return 0;
}
static BOOL pb_write_objects(id self, SEL c, id arr) {
    (void)c;
    pbstate_t *p = pb_st(self);
    nsarr_t *a = (nsarr_t *)arr;
    BOOL any = NO;
    id first = pb_first(p, NO);
    if (first && pi_empty(first)) MSG(void, pb_list(p), "removeObject:", first);
    for (u64 i = 0; arr && i < a->n; i++) {
        id it = item_for_object(self, a->items[i]);
        if (!it) continue;
        cx_arr_add(pb_list(p), it);
        any = YES;
        id s = pi_find(it, NSPasteboardTypeString);   /* texto do 1º item também vai para os outros processos */
        if (i == 0 && pb_is_general(p) && s && cx_is_str(s)) { pb_write(cx_cstr(s), strlen(cx_cstr(s))); pb_remember(p, cx_cstr(s), strlen(cx_cstr(s))); }
    }
    return any;
}
static id pb_items(id self, SEL c) {
    (void)c;
    pbstate_t *p = pb_st(self);
    pb_sync(p);
    return objc_autorelease(MSG0(id, pb_list(p), "copy"));
}
static u64 pb_index_of_item(id self, SEL c, id it) {
    (void)c;
    i64 i = MSG(i64, pb_list(pb_st(self)), "indexOfObject:", it);
    return (u64)i;
}

/* ---- readObjectsForClasses:options: — por item, a 1ª classe que o item satisfaz */
static BOOL opt_bool(id opts, const char *k) { id v = opts ? MSG(id, opts, "objectForKey:", cx_str(k)) : 0; return v && MSG0(BOOL, v, "boolValue"); }
static BOOL url_conforms(id url, id utis) {
    if (!utis) return YES;
    id ext = MSG0(id, MSG0(id, url, "path"), "pathExtension");
    id ut = pb_cls("UTType") && ext ? MSG(id, pb_cls("UTType"), "typeWithFilenameExtension:", ext) : 0;
    nsarr_t *u = (nsarr_t *)utis;
    for (u64 i = 0; i < u->n; i++) {
        if (ut && pb_resp(ut, "conformsToType:")) {
            id want = MSG(id, pb_cls("UTType"), "typeWithIdentifier:", u->items[i]);
            if (want && MSG(BOOL, ut, "conformsToType:", want)) return YES;
        }
        id ident = ut && pb_resp(ut, "identifier") ? MSG0(id, ut, "identifier") : 0;
        if (ident && MSG(BOOL, ident, "isEqualToString:", u->items[i])) return YES;
        if (!strcmp(cx_cstr(u->items[i]), "public.item") || !strcmp(cx_cstr(u->items[i]), "public.data")) return YES;
    }
    return NO;
}
static id read_item_as(id item, id cls, id opts) {
    const char *n = cx_class_name((cx_class_t *)cls);
    if (!n) return 0;
    cx_class_t *k = (cx_class_t *)cls;
    if (cx_is_subclass(k, &NSPasteboardItem_c)) return item;
    cx_class_t *urlk = cx_class_named("NSURL");
    if (urlk && cx_is_subclass(k, urlk)) {
        id s = pb_as_string(pi_get(item, cx_str("public.file-url")));
        BOOL file = s != 0;
        if (!s && !opt_bool(opts, "NSPasteboardURLReadingFileURLsOnlyKey")) s = pb_as_string(pi_get(item, cx_str("public.url")));
        if (!s) return 0;
        id u = MSG(id, pb_cls("NSURL"), "URLWithString:", s);
        if (!u) return 0;
        id want = opts ? MSG(id, opts, "objectForKey:", cx_str("NSPasteboardURLReadingContentsConformToTypesKey")) : 0;
        if (want && file && !url_conforms(u, want)) return 0;
        return u;
    }
    if (strstr(n, "AttributedString")) {
        id rtf = pi_get(item, cx_str("public.rtf"));
        if (rtf && pb_is_data(rtf) && cx_responds(k, sel_registerName("initWithRTF:documentAttributes:"))) {
            id a = MSG(id, MSG0(id, cls, "alloc"), "initWithRTF:documentAttributes:", rtf, (id)0);
            if (a) return objc_autorelease(a);
        }
        id s = pb_as_string(pi_get(item, NSPasteboardTypeString));
        return s ? objc_autorelease(MSG(id, MSG0(id, cls, "alloc"), "initWithString:", s)) : 0;
    }
    if (strstr(n, "String")) {
        id s = pb_as_string(pi_get(item, NSPasteboardTypeString));
        if (!s) s = pb_as_string(pi_get(item, cx_str("public.url")));
        return s;
    }
    if (cx_is_subclass(k, &NSImage_c)) {
        id o = pi_get(item, cx_str("com.apple.mos.nsimage"));
        if (o) return o;
        static const char *imgt[] = {"public.png", "public.tiff", "public.jpeg", "com.compuserve.gif", "com.microsoft.bmp"};
        for (unsigned i = 0; i < sizeof imgt / sizeof imgt[0]; i++) {
            id d = pb_as_data(pi_get(item, cx_str(imgt[i])));
            if (d) { id img = MSG(id, MSG0(id, cls, "alloc"), "initWithData:", d); if (img) return objc_autorelease(img); }
        }
        return 0;
    }
    if (cx_is_subclass(k, &NSColor_c)) return pi_get(item, cx_str("com.apple.cocoa.pasteboard.color"));
    /* NSPasteboardReading do app */
    if (cx_responds(k->isa, sel_registerName("readableTypesForPasteboard:")) && cx_responds(k, sel_registerName("initWithPasteboardPropertyList:ofType:"))) {
        id types = MSG(id, cls, "readableTypesForPasteboard:", objc_getAssociatedObject(item, &k_pi_pb));
        nsarr_t *t = (nsarr_t *)types;
        for (u64 i = 0; types && i < t->n; i++) {
            id v = pi_get(item, t->items[i]);
            if (!v) continue;
            id o = MSG(id, MSG0(id, cls, "alloc"), "initWithPasteboardPropertyList:ofType:", v, t->items[i]);
            if (o) return objc_autorelease(o);
        }
    }
    return 0;
}
static id pb_read_objects(id self, SEL c, id classes, id opts) {
    (void)c;
    pbstate_t *p = pb_st(self);
    pb_sync(p);
    nsarr_t *cl = (nsarr_t *)classes;
    id out = cx_arr_new(0);
    nsarr_t *l = (nsarr_t *)pb_list(p);
    for (u64 i = 0; i < l->n; i++)
        for (u64 j = 0; classes && j < cl->n; j++) { id o = read_item_as(l->items[i], cl->items[j], opts); if (o) { cx_arr_add(out, o); break; } }
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
        id next = s->next_responder ? s->next_responder : s->superview;
        if (next && next != self) { MSG(void, next, "keyDown:", e); return; }
        kv_tab_key(self, e);   /* topo da cadeia de views: Tab muda o foco */
    }
}
static void r_key_up(id self, SEL c, id e) {
    (void)c;
    if (!cx_is_subclass(self->isa, &NSView_c)) return;
    vstate_t *s = vs(self);
    id next = s->next_responder ? s->next_responder : s->superview;
    if (next && next != self) MSG(void, next, "keyUp:", e);
}
static void r_flags_changed(id self, SEL c, id e) {
    (void)c;
    if (!cx_is_subclass(self->isa, &NSView_c)) return;
    vstate_t *s = vs(self);
    id next = s->next_responder ? s->next_responder : s->superview;
    if (next && next != self) MSG(void, next, "flagsChanged:", e);
}
static void v_set_ident(id self, SEL c, id i) { (void)c; keep(&vs(self)->identifier, i); }
static id v_ident(id self, SEL c) { (void)c; return vs(self)->identifier; }
static void v_set_next(id self, SEL c, id n) { (void)c; vs(self)->next_responder = n; }
static id v_next(id self, SEL c) { (void)c; vstate_t *s = vs(self); return s->next_responder ? s->next_responder : s->superview; }

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
    return win_hit_root(w, x, y);
}

/* ================================================================ NSApplication */
typedef struct { id delegate, menu, help; BOOL running, launched, terminating; int modal_result; BOOL in_modal; } appstate_t;
static appstate_t app;
static id app_singleton;
/* O renderer do Chromium (RendererMainPlatformDelegate::EnableSandbox) faz CHECK(NSApp == nil)
 * depois de o próprio Init já ter chamado sharedApplication. O singleton continua existindo para
 * quem manda a mensagem; o global exportado fica nulo nos helpers (--type=renderer/gpu/utility
 * ou executável "Code Helper ..."). O processo do app (sem --type=) publica NSApp normalmente. */
/* Só o renderer quer o global nulo. Os utilitários que rodam um laço de eventos do AppKit fazem CHECK(NSApp) em
 * base::MessagePumpNSApplication::DoRun: o de notificações do Chrome (mac_notifications) e, no Claude desktop, o
 * áudio (audio.mojom.AudioService) e a captura de vídeo (video_capture.mojom.VideoCaptureService). Sem o global
 * eles caíam ao subir, e o navegador os recriava sem parar (dezenas de "Claude Helper" por minuto). */
static int nsapp_is_helper(void) {
    extern int NXArgc;
    extern char **NXArgv;
    for (int i = 1; NXArgv && i < NXArgc; i++) {
        const char *a = NXArgv[i];
        if (a && !strcmp(a, "--type=renderer")) return 1;
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
static BOOL ap_set_policy(id self, SEL c, i64 p) {
    (void)self; (void)c;
    if (!cx_act_set_policy((int)p)) return NO;
    /* accessory e prohibited saem do Dock; regular volta. O selo do NSDockTile segue em MOS_DESK_BADGE. */
    cx_dock_set_visible(cx_act_hides_dock() ? 0 : 1);
    return YES;
}
static i64 ap_policy(id self, SEL c) { (void)self; (void)c; return cx_act_policy(); }
static void ap_set_menu(id self, SEL c, id m) { (void)self; (void)c; keep(&app.menu, m); menus_dirty = YES; }
static id ap_menu(id self, SEL c) { (void)self; (void)c; return app.menu; }
static void ap_set_help(id self, SEL c, id m) { (void)self; (void)c; keep(&app.help, m); }
static id ap_help(id self, SEL c) { (void)self; (void)c; return app.help; }
static id ap_windows(id self, SEL c) { (void)self; (void)c; return app_windows ? app_windows : cx_arr_new(&NSMutableArray_c); }
/* -orderedWindows: as janelas visíveis da frente para trás (a última trazida à frente fica no fim de app_windows).
 * O WebContentsOcclusionCheckerMac do Chromium percorre esta lista para decidir se cada WebContents está visível. */
static id ap_ordered_windows(id self, SEL c) {
    (void)self; (void)c;
    id r = cx_arr_new(&NSMutableArray_c);
    nsarr_t *a = (nsarr_t *)app_windows;
    for (u64 i = a ? a->n : 0; i-- > 0;) if (ws(a->items[i])->visible) cx_arr_add(r, a->items[i]);
    return r;
}
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
    { nsarr_t *aw = (nsarr_t *)app_windows; for (u64 i = 0; aw && i < aw->n; i++) w_autosave(aw->items[i]); }
    exit(0);
}
static void ap_stop(id self, SEL c, id sender) { (void)self; (void)c; (void)sender; app.running = NO; }
static void ap_finish_launching(id self, SEL c) {
    (void)c;
    if (app.launched) return;
    app.launched = YES;
    /* LSUIElement no Info.plist esconde o Dock mesmo se o app nunca chama setActivationPolicy */
    {
        id bundle = MSG0(id, cx_class_named("NSBundle"), "mainBundle");
        id v = bundle ? MSG(id, bundle, "objectForInfoDictionaryKey:", cx_str("LSUIElement")) : 0;
        int yes = 0;
        if (v && cx_is_str(v)) {
            const char *s = cx_cstr(v);
            yes = s && (s[0] == '1' || s[0] == 'Y' || s[0] == 'y' || s[0] == 't' || s[0] == 'T');
        } else if (v && cx_responds(v->isa, sel_registerName("longLongValue")))
            yes = MSG0(long long, v, "longLongValue") != 0;
        if (cx_act_note_lsuielement(yes)) cx_dock_set_visible(0);
    }
    { extern void cx_gc_app_launched(void); cx_gc_app_launched(); }   /* GameController: controles já ligados */
    id d = app.delegate;
    id note = cx_note(NSApplicationWillFinishLaunchingNotification, self, 0);
    if (d && cx_responds(d->isa, sel_registerName("applicationWillFinishLaunching:"))) MSG(void, d, "applicationWillFinishLaunching:", note);
    cx_post(note);
    /* o Cocoa restaura as janelas da sessão anterior aqui e avisa (o Postico só abre a janela de conexões depois disto) */
    cx_post(cx_note(cx_str("NSApplicationDidFinishRestoringWindowsNotification"), self, 0));
    note = cx_note(NSApplicationDidFinishLaunchingNotification, self, 0);
    if (d && cx_responds(d->isa, sel_registerName("applicationDidFinishLaunching:"))) MSG(void, d, "applicationDidFinishLaunching:", note);
    cx_post(note);
    /* como no Cocoa: sem arquivo para abrir na partida, o delegate pode abrir um "sem título" (o Postico abre a janela de
     * conexões aqui); applicationShouldOpenUntitledFile: ausente vale SIM */
    if (d && cx_responds(d->isa, sel_registerName("applicationOpenUntitledFile:")) &&
        (!cx_responds(d->isa, sel_registerName("applicationShouldOpenUntitledFile:")) || MSG(BOOL, d, "applicationShouldOpenUntitledFile:", self)))
        (void)MSG(BOOL, d, "applicationOpenUntitledFile:", self);
    { extern void cx_openurl_listen(void); cx_openurl_listen(); }   /* URLs de outros apps (slack://, claude://): openurl.c */
}
static id find_window(int gid) {
    nsarr_t *a = (nsarr_t *)app_windows;
    for (u64 i = 0; a && i < a->n; i++) if (ws(a->items[i])->gid == gid) return a->items[i];
    return 0;
}
static BOOL menu_shortcut(gui_event_t *g);
id cx_hover_view(id w);              /* appkit_input.c */
void cx_cursor_moved(id w, id hit, double x, double y);   /* appkit_cursor.c */
void cx_set_hover_view(id w, id v);
int cx_gr_mouse(id view, id ev, int phase);   /* appkit_more.c: NSGestureRecognizer */
int cx_carbon_key(int phase, u16 code, u64 nsmods);   /* carbon_events.c */
static void dispatch(gui_event_t *g) {
    if (g->type == GEV_QUIT) { MSG(void, NSApp, "terminate:", (id)0); return; }
    if (g->type == GEV_MENU) { perform_menu(g->key); return; }
    if (g->type == GEV_INPUT_SOURCE) { extern void cx_ime_source_changed(int src); cx_ime_source_changed(g->key); return; }
    /* um agente (LSUIElement) pode não ter janela; o clique na faixa mesmo assim abre o menu */
    if (g->type == GEV_STATUS) {
        extern void cx_status_kernel_click(unsigned id);
        cx_status_kernel_click((unsigned)g->key);
        return;
    }
    id w = find_window((int)g->win);
    if (g->type >= GEV_DRAG_ENTER && g->type <= GEV_DRAG_END) {
        /* arrastar e soltar (appkit_input.c); o fim de um arrasto chega mesmo se a janela da fonte já fechou */
        extern void cx_drag_event(id win, const gui_event_t *g);
        cx_drag_event(w, g);
        return;
    }
    if (!w) return;
    /* Teclas vão à janela-chave, como no Cocoa. O window server manda o teclado à janela do topo, e as popups do
     * Chromium (sugestões da barra de endereço, bolha de status, balões) são janelas filhas por cima da mãe que não
     * aceitam ser chave: sem isto as teclas iam para elas e se perdiam. Uma filha que aceita ser chave (o diálogo
     * "Abrir o X?") também fica por cima da mãe quando o clique tornou a mãe chave; aí as teclas são da mãe. */
    if ((g->type == GEV_KEY_DOWN || g->type == GEV_KEY_UP || g->type == GEV_FLAGS) && w != g_key_window && g_key_window &&
        ws(g_key_window)->visible && ws(g_key_window)->gid > 0 &&
        ((cx_responds(w->isa, sel_registerName("canBecomeKeyWindow")) && !MSG0(BOOL, w, "canBecomeKeyWindow")) ||
         w_descends(w, g_key_window)))
        w = g_key_window;
    {   /* janela com sheet aberta: cliques, rolagem e teclas param nela (appkit_sheets.c); a sheet vira chave */
        extern int cx_sheet_blocks(id w);
        if ((g->type == GEV_MOUSE_DOWN || g->type == GEV_RIGHT_DOWN || g->type == GEV_SCROLL || g->type == GEV_KEY_DOWN) && cx_sheet_blocks(w)) {
            if (g->type == GEV_MOUSE_DOWN || g->type == GEV_RIGHT_DOWN) NSBeep();
            return;
        }
    }
    wstate_t *s = ws(w);
    g_mods = mac_mods(g->mods);
    { extern void cx_gc_mouse_event(int type, int x, int y, int buttons, int scroll);   /* GCMouse */
      if (g->type >= GEV_MOUSE_DOWN && g->type <= GEV_SCROLL) cx_gc_mouse_event((int)g->type, g->x, g->y, g->buttons, g->w); }
    if (g->type >= GEV_MOUSE_DOWN && g->type <= GEV_SCROLL) {
        /* o direito chega só como GEV_RIGHT_DOWN (sem soltar): vale até o próximo evento de mouse */
        g_pressed_buttons = (g->type == GEV_MOUSE_UP ? 0 : (g->buttons & 1) | (g->type == GEV_MOUSE_DOWN ? 1 : 0)) |
                            (g->type == GEV_RIGHT_DOWN ? 2 : 0);
        g_mouse_screen.x = s->frame.o.x + g->x;
        g_mouse_screen.y = s->frame.o.y + (s->frame.s.h - g->y);
    }
    switch (g->type) {
    case GEV_CLOSE:   /* key = 1: pedido do botão vermelho/⌘W/Dock (a janela continua); 0: já destruída pelo sistema */
        if (g->key == 1) MSG(void, w, "performClose:", w);
        else close_window(w, YES);
        break;
    case GEV_RESIZE: {
        s->frame.s.w = g->w;
        s->frame.s.h = g->h;
        v_set_frame(s->content, 0, r_make(0, 0, g->w, g->h));
        if (s->delegate && cx_responds(s->delegate->isa, sel_registerName("windowDidResize:"))) MSG(void, s->delegate, "windowDidResize:", cx_note(NSWindowDidResizeNotification, w, 0));
        w_autosave(w);
        s->needs = YES;
        break;
    }
    case GEV_MOUSE_DOWN: case GEV_RIGHT_DOWN: case GEV_SCROLL: {
        if (!s->tb_host && g->type == GEV_MOUSE_DOWN && g->y >= 0 && cx_responds(w->isa, sel_registerName("toolbar")) && g->y < cx_toolbar_bar_height(MSG0(id, w, "toolbar"))) {
            id tb = MSG0(id, w, "toolbar");
            if (tb && cx_responds(tb->isa, sel_registerName("isVisible")) && MSG0(BOOL, tb, "isVisible")
                && cx_responds(tb->isa, sel_registerName("mouseDown:"))) {
                MSG(void, tb, "mouseDown:", make_event(1, w, g->x, g->y, g));
                break;
            }
        }
        id v = win_hit_root(w, g->x, g->y);
        if (!v) v = s->content;
        if (g->type == GEV_MOUSE_DOWN) {
            id cur = v;
            int inside = 0;
            for (int i = 0; cur && i < 32; i++) {
                if (cx_pop_is_content(cur)) { inside = 1; break; }
                if (!cx_is_subclass(cur->isa, &NSView_c)) break;
                cur = vs(cur)->superview;
            }
            if (!inside) cx_pop_close_transients(); /* NSPopoverBehaviorTransient */
            if (w != g_key_window) w_become_key(w);   /* o clique torna chave a janela que aceita (Cocoa); popups não */
            /* movableByWindowBackground: o clique num fundo que deixa (mouseDownCanMoveWindow) arrasta a janela */
            if (s->movable_bg && !(g->mods & 2) && s->gid > 0 && MSG0(BOOL, v, "mouseDownCanMoveWindow")
                && SYS2(GUI_WIN_FOCUS, s->gid, GUI_FOCUS_MOVE) == 0) break;
            if (MSG0(BOOL, v, "acceptsFirstResponder")) w_make_first(w, 0, v);
            else if (s->first && vs(s->first)->kind == K_TEXT) w_make_first(w, 0, 0);
            s->mouse_view = v;
            {   /* diagnóstico (ca-log): quem recebe os cliques */
                extern int cx_ca_diag(void);
                static int nlog;
                if (nlog < 40 && cx_ca_diag()) {
                    nlog++;
                    cx_fdprintf(2, "[ca] mouseDown (%d,%d) cliques %d -> %s %dx%d, primeiro respondedor %s\n", g->x, g->y, g->clicks,
                                cx_class_name(v->isa), (int)vs(v)->frame.s.w, (int)vs(v)->frame.s.h, s->first ? cx_class_name(s->first->isa) : "-");
                }
            }
            {   /* reconhecedores de gesto da view e dos ancestrais veem o clique antes (appkit_more.c) */
                id ev = make_event(1, w, g->x, g->y, g);
                extern int cx_ctrl_click_menu(id view, id e);   /* ⌃clique = menu de contexto (appkit_menupop.c) */
                if ((g->mods & 2) && cx_ctrl_click_menu(v, ev)) break;
                if (!cx_gr_mouse(v, ev, 0)) MSG(void, v, "mouseDown:", ev);
            }
        } else if (g->type == GEV_RIGHT_DOWN) {
            id ev = make_event(3, w, g->x, g->y, g);
            if (!cx_gr_mouse(v, ev, 3)) MSG(void, v, "rightMouseDown:", ev);
        }
        else MSG(void, v, "scrollWheel:", make_event(22, w, g->x, g->y, g));
        break;
    }
    case GEV_MOUSE_UP:
        if (s->mouse_view) {
            id ev = make_event(2, w, g->x, g->y, g);
            if (!cx_gr_mouse(s->mouse_view, ev, 2)) MSG(void, s->mouse_view, "mouseUp:", ev);
            s->mouse_view = 0;
        }
        break;
    case GEV_MOUSE_MOVE:
        if (g->buttons && s->mouse_view) {
            id ev = make_event(6, w, g->x, g->y, g);
            if (!cx_gr_mouse(s->mouse_view, ev, 1)) MSG(void, s->mouse_view, "mouseDragged:", ev);
        }
        else {
            /* sem botão: hover (o Chromium realça botões, mostra dicas e muda o cursor com estes eventos) */
            id v = win_hit_root(w, g->x, g->y);
            if (!v) v = s->content;
            id prev = cx_hover_view(w);
            if (prev != v) {
                if (prev) MSG(void, prev, "mouseExited:", make_event(9, w, g->x, g->y, g));
                MSG(void, v, "mouseEntered:", make_event(8, w, g->x, g->y, g));
                cx_set_hover_view(w, v);
            }
            MSG(void, v, "mouseMoved:", make_event(5, w, g->x, g->y, g));
            cx_cursor_moved(w, v, g->x, g->y);   /* appkit_cursor.c: cursor rects e setHiddenUntilMouseMoves: */
        }
        break;
    case GEV_FLAGS: {
        g_last_flags = mac_mods(g->mods);
        cx_carbon_key(2, 0, g_last_flags);   /* carbon_events.c: kEventRawKeyModifiersChanged */
        {
            id target = s->first ? s->first : s->content;
            if (target) MSG(void, target, "flagsChanged:", make_event(12, w, 0, 0, g));
        }
        break;
    }
    case GEV_KEY_UP: {
        extern void cx_gc_keyboard_event(u16 mac_keycode, u64 mods, int down);
        if (cx_mods_changed(g_last_flags, mac_mods(g->mods))) {
            g_last_flags = mac_mods(g->mods);
            id target = s->first ? s->first : s->content;
            if (target) MSG(void, target, "flagsChanged:", make_event(12, w, 0, 0, g));
        }
        id e = make_event(11, w, 0, 0, g);
        { u16 kc = mac_keycode(g->key); if (kc < 128) g_key_held[kc] = 0; }
        cx_gc_keyboard_event(mac_keycode(g->key), mac_mods(g->mods), 0);
        if (cx_carbon_key(1, mac_keycode(g->key), mac_mods(g->mods))) break;   /* atalho do Carbon solto */
        id target = s->first ? s->first : s->content;
        if (target) MSG(void, target, "keyUp:", e);
        break;
    }
    case GEV_KEY_DOWN: {
        if (cx_mods_changed(g_last_flags, mac_mods(g->mods))) {
            g_last_flags = mac_mods(g->mods);
            id target = s->first ? s->first : s->content;
            if (target) MSG(void, target, "flagsChanged:", make_event(12, w, 0, 0, g));
        }
        id e = make_event(10, w, 0, 0, g);
        {   /* repetição automática: a mesma tecla desce de novo sem ter subido, pouco depois */
            u16 kc = mac_keycode(g->key);
            double now = cx_now();
            if (kc < 128) { evs(e)->repeat = g_key_held[kc] && now - g_key_time[kc] < 0.75; g_key_held[kc] = 1; g_key_time[kc] = now; }
        }
        { extern void cx_gc_keyboard_event(u16 mac_keycode, u64 mods, int down); cx_gc_keyboard_event(mac_keycode(g->key), mac_mods(g->mods), 1); }
        if (cx_carbon_key(0, mac_keycode(g->key), mac_mods(g->mods))) break;   /* RegisterEventHotKey do app (carbon_events.c) */
        if (menu_shortcut(g)) break;
        if ((g->mods & 8) && (g->key == 'q' || g->key == 'Q')) { MSG(void, NSApp, "terminate:", (id)0); break; }
        if ((g->mods & 8) && (g->key == 'w' || g->key == 'W')) { MSG(void, w, "performClose:", w); break; }   /* pela mensagem: o NSDocument e as subclasses interceptam */
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
            {   /* diagnóstico (ca-log): para onde vão as teclas */
                extern int cx_ca_diag(void);
                static int nlog;
                if (nlog < 40 && cx_ca_diag()) { nlog++; cx_fdprintf(2, "[ca] keyDown tecla %d mods 0x%x -> %s (janela %u -> %d, chave %d)\n", g->key, g->mods, cx_class_name(target->isa), g->win, s->gid, g_key_window ? ws(g_key_window)->gid : 0); }
            }
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
#include "appkit_evq_core.h"
static evq_t g_pend;          /* eventos lidos por nextEventMatchingMask: que não casaram com a máscara */
static id g_posted;           /* NSEvents de -postEvent:atStart: (NSMutableArray) */
static int drain_posted(void);
static void deliver(gui_event_t *g);
static void render_due(void) {
    double now = cx_now();
    nsarr_t *a = (nsarr_t *)app_windows;
    for (u64 i = 0; a && i < a->n; i++) {
        id w = a->items[i];
        wstate_t *s = ws(w);
        if (!s->visible || !(s->needs || tree_needs(s->content) || (s->tb_host && tree_needs(s->tb_host)))) continue;
        if (now - s->last_render < 0.04) continue;
        s->last_render = now;
        render_window(w);
    }
}
static int pump(void) {
    void *pool = objc_autoreleasePoolPush();
    gui_event_t g;
    int got = 0;
    sync_menus();
    { extern void cx_app_more_tick(void); cx_app_more_tick(); }   /* appkit_app_more.c: ativação e menu Janela */
    bind_refresh();
    got += drain_posted();
    while (evq_pop_front(&g_pend, &g)) { deliver(&g); got++; }   /* o que um laço de rastreio deixou para depois */
    while (SYS1(GUI_POLL, &g) == 1) { deliver(&g); got++; }
    double next = cx_run_timers();
    cx_ca_poll();                              /* CALayerHost de outro processo: geração nova pede redesenho */
    /* teto de 25 quadros/s por janela: o compositor do Chromium (e cada IOSurfaceUnlock de escrita, quartzcore.c)
     * marca a janela a cada quadro, e o render_window (autolayout, composição inteira em software, flush, present)
     * roda na thread principal — a mesma do JS do Electron. Sem teto, a thread principal do VS Code vivia
     * redesenhando e a navegação da janela não andava. Cedo demais: s->needs fica ligado e o próximo pump desenha. */
    render_due();
    objc_autoreleasePoolPop(pool);
    if (got) return 0;
    int ms = next < 0 ? 16 : (int)(next * 1000);
    return ms > 16 ? 16 : ms < 1 ? 1 : ms;
}
/* ================================================================ fila de eventos: nextEventMatchingMask:, sendEvent:
 * Um laço de rastreio do app (arrastar, redimensionar um elemento, o -trackMouse: de uma célula) pede o próximo evento
 * de um tipo; o que vier antes e não casar fica em g_pend e o pump entrega depois, na ordem. Um NSApplication ou
 * NSWindow do app que sobrescreve -sendEvent: recebe o NSEvent antes do AppKit, como no macOS. */
static void ap_send_event(id self, SEL c, id ev);
static void w_send_event(id self, SEL c, id ev);
IMP cx_msg_lookup(cx_class_t *c, SEL sel);
static id window_by_number(i64 n) {
    nsarr_t *a = (nsarr_t *)app_windows;
    for (u64 i = 0; a && i < a->n; i++) if (MSG0(i64, a->items[i], "windowNumber") == n) return a->items[i];
    return 0;
}
static id ap_window_with_number(id self, SEL c, i64 n) { (void)self; (void)c; return window_by_number(n); }
static int overrides(id o, const char *sel, IMP ours) { return o && cx_msg_lookup(o->isa, sel_registerName(sel)) != ours; }
/* janela que recebe um evento: teclas vão à janela-chave (como no dispatch) */
static id event_window(const gui_event_t *g) {
    id w = find_window((int)g->win);
    unsigned t = evq_ns_type(g);
    if ((t == 10 || t == 11 || t == 12) && g_key_window && ws(g_key_window)->visible && ws(g_key_window)->gid > 0 && w != g_key_window &&
        (!w || (cx_responds(w->isa, sel_registerName("canBecomeKeyWindow")) && !MSG0(BOOL, w, "canBecomeKeyWindow"))))
        w = g_key_window;
    return w;
}
static id event_for(gui_event_t *g) {
    unsigned t = evq_ns_type(g);
    id w = t ? event_window(g) : 0;
    return w ? make_event(t, w, g->x, g->y, g) : 0;
}
/* estado global de entrada (modificadores, botões, posição) para um evento que não passou pelo dispatch */
static void note_input(const gui_event_t *g) {
    g_mods = mac_mods(g->mods);
    if (g->type >= GEV_MOUSE_DOWN && g->type <= GEV_SCROLL) {
        g_pressed_buttons = (g->type == GEV_MOUSE_UP ? 0 : (g->buttons & 1) | (g->type == GEV_MOUSE_DOWN ? 1 : 0)) | (g->type == GEV_RIGHT_DOWN ? 2 : 0);
        id w = find_window((int)g->win);
        if (w) {
            wstate_t *s = ws(w);
            g_mouse_screen.x = s->frame.o.x + g->x;
            g_mouse_screen.y = s->frame.o.y + (s->frame.s.h - g->y);
            if (g->type == GEV_MOUSE_UP) s->mouse_view = 0;   /* o laço de rastreio consumiu o soltar */
        }
    }
}
static void deliver(gui_event_t *g) {
    if (NSApp && evq_ns_type(g) && overrides(NSApp, "sendEvent:", (IMP)ap_send_event)) {
        id ev = event_for(g);
        if (ev) { MSG(void, NSApp, "sendEvent:", ev); return; }
    }
    id w = evq_ns_type(g) ? event_window(g) : 0;
    if (w && overrides(w, "sendEvent:", (IMP)w_send_event)) {
        id ev = event_for(g);
        if (ev) { MSG(void, w, "sendEvent:", ev); return; }
    }
    dispatch(g);
}
static int drain_posted(void) {
    int n = 0;
    while (g_posted && MSG0(u64, g_posted, "count")) {
        id ev = objc_retain(MSG(id, g_posted, "objectAtIndex:", (u64)0));
        MSG(void, g_posted, "removeObjectAtIndex:", (u64)0);
        cx_set_current_event(ev);
        MSG(void, NSApp, "sendEvent:", ev);
        objc_release(ev);
        n++;
    }
    return n;
}
/* NSEvent → gui_event_t: o original, quando veio do window server; senão montado dos campos (eventos criados pelo app) */
static int event_to_gui(id ev, gui_event_t *g) {
    evstate_t *s = evs(ev);
    if (s->has_g) { *g = s->g; return 1; }
    memset(g, 0, sizeof *g);
    g->type = evq_gev_from_ns((unsigned)s->type);
    if (!g->type) return 0;
    id w = s->window ? s->window : (s->wnum ? window_by_number(s->wnum) : 0);
    if (!w) w = g_key_window;
    if (!w) return 0;
    g->win = (unsigned)ws(w)->gid;
    g->x = (int)s->x;
    g->y = (int)(win_h(w) - s->y);
    g->mods = evq_mods_from_mac(s->mods);
    g->clicks = (int)(s->clicks ? s->clicks : 1);
    g->buttons = (s->type == 1 || s->type == 6) ? 1 : 0;
    if (s->type == 22) { g->w = (int)-s->dy; g->h = (int)-s->dx; }
    if (s->type == 10 || s->type == 11) {
        const char *t = s->chars ? cx_cstr(s->chars) : "";
        int n = 0;
        while (t[n] && n < (int)sizeof g->text - 1) { g->text[n] = t[n]; n++; }
        g->text[n] = 0;
        g->key = (unsigned char)t[0];
        u16 k = s->keycode;   /* setas e afins pelo código de tecla do Mac */
        if (k == 126) g->key = 0x100; else if (k == 125) g->key = 0x101; else if (k == 123) g->key = 0x102; else if (k == 124) g->key = 0x103;
        else if (k == 36) g->key = 10; else if (k == 51) g->key = 8; else if (k == 53) g->key = 27; else if (k == 48) g->key = 9;
    }
    return 1;
}
static void ap_send_event(id self, SEL c, id ev) {
    (void)self; (void)c;
    if (!ev) return;
    u64 t = evs(ev)->type;
    if (t == 15 || t == 16) return;                 /* app-defined/periódico: só quem os pediu (laço ou subclasse) */
    gui_event_t g;
    if (!event_to_gui(ev, &g)) return;
    id w = event_window(&g);
    if (w && overrides(w, "sendEvent:", (IMP)w_send_event)) { MSG(void, w, "sendEvent:", ev); return; }
    dispatch(&g);
}
static void w_send_event(id self, SEL c, id ev) {
    (void)c;
    gui_event_t g;
    if (!ev || !event_to_gui(ev, &g)) return;
    g.win = (unsigned)ws(self)->gid;
    dispatch(&g);
}
int CFRunLoopRunInMode(void *mode, double seconds, unsigned char return_after_source);
static id ap_next_event(id self, SEL c, u64 mask, id date, id mode, BOOL dequeue) {
    (void)self; (void)c; (void)mode;
    double deadline = cx_now() + (date ? MSG0(double, date, "timeIntervalSinceNow") : 0);
    for (;;) {
        for (u64 i = 0, n = g_posted ? MSG0(u64, g_posted, "count") : 0; i < n; i++) {
            id ev = MSG(id, g_posted, "objectAtIndex:", i);
            u64 t = evs(ev)->type;
            if (t >= 64 || !(mask & (1ULL << t))) continue;
            objc_autorelease(objc_retain(ev));
            if (dequeue) MSG(void, g_posted, "removeObjectAtIndex:", i);
            cx_set_current_event(ev);
            return ev;
        }
        gui_event_t g;
        while (g_pend.n < EVQ_MAX && SYS1(GUI_POLL, &g) == 1) evq_push(&g_pend, &g);
        int i = evq_find(&g_pend, mask);
        if (i >= 0) {
            gui_event_t ge = g_pend.ev[i];
            id ev = event_for(&ge);
            if (dequeue || !ev) { evq_remove(&g_pend, i); note_input(&ge); }
            if (ev) return ev;
            continue;
        }
        /* nada na máscara: uma volta do resto (fontes do CFRunLoop, timers, CALayerHost, desenho do que o laço mudou),
         * como o run loop do macOS no modo pedido; com prazo vencido (nil, distantPast) volta nil depois desta volta */
        void *pool = objc_autoreleasePoolPush();
        CFRunLoopRunInMode(0, 0, 1);
        cx_run_timers();
        cx_ca_poll();
        render_due();
        objc_autoreleasePoolPop(pool);
        if (g_posted && MSG0(u64, g_posted, "count")) continue;
        if (cx_now() >= deadline) return 0;
        double left = deadline - cx_now();
        SYS1(SYS_SLEEP, left > 0.004 ? 4 : 1);
    }
}
static id w_next_event(id self, SEL c, u64 mask) { (void)c; return ap_next_event(NSApp, 0, mask, MSG0(id, (id)cx_class_named("NSDate"), "distantFuture"), 0, YES); (void)self; }
static id w_next_event_full(id self, SEL c, u64 mask, id date, id mode, BOOL dq) { (void)self; (void)c; return ap_next_event(NSApp, 0, mask, date, mode, dq); }
static void ap_post_event(id self, SEL c, id ev, BOOL at_start) {
    (void)self; (void)c;
    if (!ev) return;
    if (!g_posted) g_posted = objc_retain(MSG0(id, (id)cx_class_named("NSMutableArray"), "array"));
    if (at_start) MSG(void, g_posted, "insertObject:atIndex:", ev, (u64)0);
    else MSG(void, g_posted, "addObject:", ev);
}
static void ap_discard_events(id self, SEL c, u64 mask, id before) {
    (void)self; (void)c;
    double limit = before ? evs(before)->ts : 1e300;
    evq_discard(&g_pend, mask);
    for (u64 i = g_posted ? MSG0(u64, g_posted, "count") : 0; i-- > 0;) {
        id ev = MSG(id, g_posted, "objectAtIndex:", i);
        u64 t = evs(ev)->type;
        if (t < 64 && (mask & (1ULL << t)) && evs(ev)->ts < limit) MSG(void, g_posted, "removeObjectAtIndex:", i);
    }
}
static id ap_current_event(id self, SEL c) { (void)self; (void)c; return g_current_event; }
/* NSEvent de mouse e teclado criados pelo app (para postEvent:/sendEvent: ou testes) */
static id ev_mouse_make(id cls, SEL c, u64 type, NSPoint loc, u64 mods, double ts, i64 wnum, id ctx, i64 evnum, i64 clicks, float pressure) {
    (void)cls; (void)c; (void)ctx; (void)evnum; (void)pressure;
    id e = objc_autorelease(cx_alloc_instance(&NSEvent_c));
    evstate_t *s = calloc(1, sizeof *s);
    ST(e) = s;
    s->type = type; s->x = loc.x; s->y = loc.y; s->mods = mods; s->clicks = clicks;
    s->ts = ts > 0 ? ts : (double)mach_absolute_time() / 1e9;
    s->wnum = wnum;
    s->window = wnum ? window_by_number(wnum) : 0;
    return e;
}
static id ev_key_make(id cls, SEL c, u64 type, NSPoint loc, u64 mods, double ts, i64 wnum, id ctx, id chars, id chars_nomods, BOOL rep, u16 code) {
    (void)chars_nomods;
    id e = ev_mouse_make(cls, c, type, loc, mods, ts, wnum, ctx, 0, 0, 0);
    evstate_t *s = evs(e);
    s->chars = chars ? objc_retain(chars) : 0;
    s->keycode = code;
    s->repeat = rep ? 1 : 0;
    return e;
}
static id ev_enter_exit_make(id cls, SEL c, u64 type, NSPoint loc, u64 mods, double ts, i64 wnum, id ctx, i64 evnum, i64 tag, void *ud) {
    (void)tag; (void)ud;
    return ev_mouse_make(cls, c, type, loc, mods, ts, wnum, ctx, evnum, 0, 0);
}
/* eventos periódicos (NSEventTypePeriodic 16) para laços de rastreio com auto-repetição */
static int g_periodic_on;
static double g_periodic_period;
static int g_periodic_gen;
void *dispatch_get_main_queue(void);
void dispatch_after_f(u64 when, void *q, void *ctx, void (*fn)(void *));
u64 dispatch_time(u64 when, long delta);
static void periodic_fire(void *ctx) {
    if (!g_periodic_on || (int)(long)ctx != g_periodic_gen) return;
    NSPoint p = {0, 0};
    id ev = ev_other(0, 0, 16, p, g_mods, 0, 0, 0, 0, 0, 0);
    ap_post_event(NSApp, 0, ev, NO);
    dispatch_after_f(dispatch_time(0, (long)(g_periodic_period * 1e9)), dispatch_get_main_queue(), ctx, periodic_fire);
}
static void ev_start_periodic(id cls, SEL c, double delay, double period) {
    (void)cls; (void)c;
    g_periodic_on = 1; g_periodic_period = period > 0.001 ? period : 0.001; g_periodic_gen++;
    dispatch_after_f(dispatch_time(0, (long)(delay * 1e9)), dispatch_get_main_queue(), (void *)(long)g_periodic_gen, periodic_fire);
}
static void ev_stop_periodic(id cls, SEL c) { (void)cls; (void)c; g_periodic_on = 0; g_periodic_gen++; }

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
                {   /* diagnóstico (ca-log): uma thread rodando um loop vazio não recebe as tarefas postadas para ela */
                    extern int cx_ca_diag(void);
                    static int seen[64], nseen;
                    int me = (int)SYS0(SYS_GETTID), k = 0;
                    while (k < nseen && seen[k] != me) k++;
                    if (k == nseen && nseen < 64 && cx_ca_diag()) { seen[nseen++] = me; cx_fdprintf(2, "[ca] NSRunLoop: thread %d roda um loop sem fontes nem timers\n", me); }
                }
                double l = until - cx_now();
                if (l > 0) SYS1(SYS_SLEEP, l > 0.05 ? 50 : (int)(l * 1000) + 1);
            }
        } while (until >= 1e299);                            /* -[NSRunLoop run]: para sempre, como antes */
        return;
    }
    /* thread principal: AppKit + CFRunLoop. -runMode:beforeDate: volta depois de atender uma entrada (evento de janela ou
     * fonte do run loop) ou no prazo, como no macOS; -[NSRunLoop run] (prazo infinito) segue para sempre */
    while (cx_now() < until) {
        int ms = pump();
        int r = CFRunLoopRunInMode(0, 0, 1);
        if (until < 1e299 && (!ms || r == 4)) return;
        if (r == 4) ms = 0;
        if (ms) SYS1(SYS_SLEEP, ms);
    }
}
static void ap_run(id self, SEL c) {
    ap_finish_launching(self, c);
    app.running = YES;
    BOOL had = NO;
    while (app.running) {
        int ms = pump();
        /* como o [NSApp run] do macOS: o run loop principal roda junto (fontes, timers do CF e a fila principal do
         * GCD). Sem isto, um dispatch_async(dispatch_get_main_queue()) vindo de outra thread nunca rodava num app
         * Cocoa comum (ex.: o documento aberto pelo Finder, entregue pela thread do openurl.c) */
        if (CFRunLoopRunInMode(0, 0, 1) == 4) ms = 0;
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
static void ap_activate(id self, SEL c, BOOL b) { (void)self; (void)c; (void)b; cx_act_activate(); }
static void ap_activate_simple(id self, SEL c) { (void)self; (void)c; cx_act_activate(); }
static BOOL ap_send_action(id self, SEL c, SEL a, id to, id from) {
    (void)self; (void)c;
    if (!to) to = app.delegate;
    if (to && cx_responds(to->isa, a)) { ((void (*)(id, SEL, id))objc_msgSend)(to, a, from); return YES; }
    return NO;
}
/* laço modal: como no AppKit, o run loop principal continua rodando (fontes, timers e a fila principal do GCD) — o
 * Chromium põe a fonte de trabalho da thread de UI nos modos comuns, que incluem o modo de painel modal. Sem isso, um
 * diálogo do Electron congelava a thread de UI inteira. -runModalForWindow: não fecha a janela ao sair (quem chamou
 * fecha); só o NSAlert fecha a dele. */
static i64 modal_loop(id w) {
    w_show(w, 0, 0);
    BOOL old = app.in_modal;
    int saved = app.modal_result;
    app.in_modal = YES;
    app.modal_result = -1000;
    while (app.modal_result == -1000 && ws(w)->visible) {
        int ms = pump();
        if (pthread_main_np() && CFRunLoopRunInMode(0, 0, 1) == 4) ms = 0;
        if (ms) SYS1(SYS_SLEEP, ms);
    }
    app.in_modal = old;
    i64 r = app.modal_result == -1000 ? -1001 : app.modal_result;
    app.modal_result = old ? saved : app.modal_result;
    return r;
}
static i64 ap_run_modal(id self, SEL c, id w) { (void)self; (void)c; return modal_loop(w); }
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

/* ================================================================ NSAlert
 * Os botões são NSButton de verdade, criados em -addButtonWithTitle: (o Electron faz [[alert addButtonWithTitle:t]
 * setTag:i] e depois troca a tecla padrão pelos -buttons; antes o método devolvia o título, uma NSString). Como no
 * AppKit, a tag padrão é 1000, 1001... e -runModal devolve a tag do botão clicado. */
typedef struct { id message, info, buttons, suppress, accessory, icon, window; i64 style; BOOL shows_suppress; } alertst_t;
static alertst_t *al(id a) { if (!ST(a)) { alertst_t *s = calloc(1, sizeof *s); s->buttons = cx_arr_new(&NSMutableArray_c); s->message = cx_str("Alerta"); ST(a) = s; } return ST(a); }
static void al_set_msg(id self, SEL c, id m) { (void)c; keep(&al(self)->message, m); }
static void al_set_info(id self, SEL c, id m) { (void)c; keep(&al(self)->info, m); }
static id al_msg(id self, SEL c) { (void)c; return al(self)->message; }
static id al_info(id self, SEL c) { (void)c; return al(self)->info; }
static void al_button_hit(id self, SEL c, id sender) { (void)self; (void)c; app.modal_result = vs(sender)->tag; }
static cx_class_t AlertTarget_c, AlertTarget_m;
static id g_alert_target;
static id al_add_button(id self, SEL c, id t) {
    (void)c;
    alertst_t *s = al(self);
    if (!g_alert_target) g_alert_target = cx_alloc_instance(&AlertTarget_c);
    id b = bt_with_title((id)&NSButton_c, 0, t ? t : cx_str(""), g_alert_target, sel_registerName("cxAlertButton:"));
    u64 n = ((nsarr_t *)s->buttons)->n;
    vs(b)->tag = (int)(1000 + n);
    if (!n) keep(&vs(b)->key_equiv, cx_str("\r"));
    cx_arr_add(s->buttons, b);
    return b;
}
static id al_buttons(id self, SEL c) { (void)c; return al(self)->buttons; }
static void al_set_style(id self, SEL c, i64 s) { (void)c; al(self)->style = s; }
static i64 al_style(id self, SEL c) { (void)c; return al(self)->style; }
static void al_set_icon(id self, SEL c, id i) { (void)c; keep(&al(self)->icon, i); }
static id al_icon(id self, SEL c) { (void)c; return al(self)->icon; }
static void al_set_accessory(id self, SEL c, id v) { (void)c; keep(&al(self)->accessory, v); }
static id al_accessory(id self, SEL c) { (void)c; return al(self)->accessory; }
static void al_set_shows_suppress(id self, SEL c, BOOL b) { (void)c; al(self)->shows_suppress = b; }
static BOOL al_shows_suppress(id self, SEL c) { (void)c; return al(self)->shows_suppress; }
static id bt_checkbox(id cls, SEL c, id title, id target, SEL action);
static id al_suppress(id self, SEL c) {
    (void)c;
    alertst_t *s = al(self);
    if (!s->suppress) s->suppress = objc_retain(bt_checkbox((id)&NSButton_c, 0, cx_str("Não perguntar novamente"), 0, 0));
    return s->suppress;
}
static id al_window(id self, SEL c) { (void)c; return al(self)->window; }
static void al_noop(id self, SEL c) { (void)self; (void)c; }
static void al_noop1(id self, SEL c, id a) { (void)self; (void)c; (void)a; }
static i64 al_run(id self, SEL c) {
    (void)c;
    alertst_t *s = al(self);
    if (!((nsarr_t *)s->buttons)->n) al_add_button(self, 0, cx_str("OK"));
    /* de baixo para cima: botões, "Não perguntar novamente", accessoryView, texto informativo, mensagem (como o
     * NSAlert do Cocoa; antes o accessoryView e o checkbox de supressão não entravam na janela) */
    NSSize acc = {0, 0};
    if (s->accessory) acc = vs(s->accessory)->frame.s;
    double W = acc.w + 40 > 420 ? acc.w + 40 : 420;
    NSSize ms = text_size(cx_cstr(s->message), 13, 3);
    double infoh = 0;
    if (s->info) { NSSize is = text_size(cx_cstr(s->info), 12, 0); infoh = (double)((int)(is.w / (W - 40)) + 1) * (is.h + 2); }
    double sup_h = s->shows_suppress ? 28 : 0, acc_h = s->accessory ? acc.h + 12 : 0;
    double H = 20 + 24 + 14 + sup_h + acc_h + (s->info ? infoh + 8 : 0) + ms.h + 4 + 20;
    id w = w_init(MSG0(id, &NSWindow_c, "alloc"), 0, r_make(0, 0, W, H), 1, 2, NO);
    w_set_title(w, 0, cx_str(""));
    id cv = ws(w)->content;
    double y = 16 + 24 + 14;
    if (s->shows_suppress) {
        id sb = al_suppress(self, 0);
        v_set_frame(sb, 0, r_make(20, y, W - 40, 20));
        v_add_sub(cv, 0, sb);
        y += sup_h;
    }
    if (s->accessory) {
        v_set_frame(s->accessory, 0, r_make(20, y, acc.w, acc.h));
        v_add_sub(cv, 0, s->accessory);
        y += acc_h;
    }
    if (s->info) {
        id inf = tf_label((id)&NSTextField_c, 0, s->info);
        vs(inf)->font = ft_make(12, 0, 0);
        v_set_frame(inf, 0, r_make(20, y, W - 40, infoh));
        v_add_sub(cv, 0, inf);
        y += infoh + 8;
    }
    id msg = tf_label((id)&NSTextField_c, 0, s->message);
    vs(msg)->font = ft_make(13, 3, 0);
    v_set_frame(msg, 0, r_make(20, y, W - 40, ms.h + 4));
    v_add_sub(cv, 0, msg);
    nsarr_t *bs = (nsarr_t *)s->buttons;
    double x = W - 20;
    for (u64 i = 0; i < bs->n; i++) {
        id b = bs->items[i];
        double bw = vs(b)->frame.s.w;
        x -= bw;
        v_set_frame(b, 0, r_make(x, 16, bw, 24));
        x -= 10;
        v_add_sub(cv, 0, b);
    }
    keep(&s->window, w);
    i64 r = modal_loop(w);
    /* o accessoryView e o checkbox são do alerta: saem da janela para o app poder ler o valor e reusar */
    if (s->accessory) v_remove(s->accessory, 0);
    if (s->suppress) v_remove(s->suppress, 0);
    if (ws(w)->visible) close_window(w, NO);
    return r;
}
/* folha: o macOS prende o alerta à janela e volta na hora; aqui o alerta roda como modal na próxima volta do run loop
 * principal e o bloco recebe a resposta. Assim o -[NSApp runModalForWindow:] que o Electron chama logo depois (caixa
 * síncrona) já está rodando quando o bloco chama -stopModal. */
typedef struct { id alert; void *handler; } alsheet_t;
void _Block_release(const void *b);
static void al_sheet_run(void *p) {
    alsheet_t *d = p;
    i64 r = al_run(d->alert, 0);
    if (d->handler) { void (*inv)(void *, i64) = *(void **)((u8 *)d->handler + 16); inv(d->handler, r); _Block_release(d->handler); }
    objc_release(d->alert);
    free(d);
}
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *));
void *dispatch_get_main_queue(void);
static void al_begin_sheet(id self, SEL c, id win, void *handler) {
    (void)c; (void)win;
    alsheet_t *d = calloc(1, sizeof *d);
    if (!d) return;
    d->alert = objc_retain(self);
    d->handler = handler ? _Block_copy(handler) : 0;
    dispatch_async_f(dispatch_get_main_queue(), d, al_sheet_run);
}
static void al_begin_sheet_delegate(id self, SEL c, id win, id delegate, SEL did_end, void *ctx) {
    (void)c; (void)win;
    i64 r = al_run(self, 0);
    if (delegate && did_end) ((void (*)(id, SEL, id, i64, void *))objc_msgSend)(delegate, did_end, self, r, ctx);
}

/* ================================================================ NSMenu (aceito, sem efeito visual) */
typedef struct { id title, attr_title, items, key, submenu; SEL action; id target; i64 tag; int state; BOOL sep, disabled; u64 mask; BOOL mask_set;
                 BOOL hidden, alternate, header; id image, represented;
                 id owner, delegate; BOOL auto_off; } menust_t;   /* auto_off: desligado pela validação (autoenablesItems); owner: o NSMenu que contém o item (fraco); delegate: do NSMenu (fraco) */
static menust_t *mn(id m) { if (!ST(m)) { menust_t *s = calloc(1, sizeof *s); s->items = cx_arr_new(&NSMutableArray_c); ST(m) = s; } return ST(m); }
static id mn_init_title(id self, SEL c, id t) { (void)c; keep(&mn(self)->title, t); menus_dirty = YES; return self; }
static void mn_add(id self, SEL c, id item) { (void)c; if (!item) return; cx_arr_add(mn(self)->items, item); mn(item)->owner = self; menus_dirty = YES; }
/* como no Cocoa, itemArray é uma cópia: o Chrome percorre o itemArray removendo itens (antes: "coleção alterada
 * durante a enumeração" e abort) */
static id mn_items(id self, SEL c) {
    (void)c;
    id src = mn(self)->items, a = cx_arr_new(&NSMutableArray_c);
    u64 n = MSG0(u64, src, "count");
    for (u64 i = 0; i < n; i++) cx_arr_add(a, MSG(id, src, "objectAtIndex:", i));
    return objc_autorelease(a);
}
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
static void mi_set_attr(id self, SEL c, id a) { (void)c; keep(&mn(self)->attr_title, a); menus_dirty = YES; }
static id mi_attr(id self, SEL c) { (void)c; return mn(self)->attr_title; }
static void mi_set_mask(id self, SEL c, u64 m) { (void)c; mn(self)->mask = m; mn(self)->mask_set = YES; menus_dirty = YES; }
static u64 mi_mask_get(id self, SEL c) {
    (void)c;
    menust_t *m = mn(self);
    int has = m->key && cx_is_str(m->key) && cx_cstr(m->key)[0];
    return cx_menu_key_mask(m->mask_set, m->mask, has);
}
static id mi_user_key(id self, SEL c) { (void)c; id k = mn(self)->key; return (k && cx_is_str(k)) ? k : cx_str(""); }
static u64 mi_user_mask(id self, SEL c) { return mi_mask_get(self, c); }
static void mi_set_action(id self, SEL c, SEL a) { (void)c; mn(self)->action = a; }
static void mi_set_title(id self, SEL c, id t) { (void)c; keep(&mn(self)->title, t); menus_dirty = YES; }
static void mi_set_key(id self, SEL c, id k) { (void)c; keep(&mn(self)->key, k); menus_dirty = YES; }
static void mi_set_tag(id self, SEL c, i64 t) { (void)c; mn(self)->tag = t; }
/* dica e atalho com o item escondido: a barra do mOS não mostra dicas nem atalhos ocultos; aceita e ignora */
static void mi_set_tooltip(id self, SEL c, id t) { (void)self; (void)c; (void)t; }
static id mi_tooltip(id self, SEL c) { (void)self; (void)c; return 0; }
static void mi_set_bool_ignored(id self, SEL c, BOOL b) { (void)self; (void)c; (void)b; }
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
static void mn_remove_at(id self, SEL c, i64 i) {
    if (i < 0 || i >= mn_count(self, c)) return;
    id it = mn_item_at(self, c, i);
    if (it && mn(it)->owner == self) mn(it)->owner = 0;
    MSG(void, mn(self)->items, "removeObjectAtIndex:", (u64)i);
    menus_dirty = YES;
}
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
    mn(item)->owner = self;
    menus_dirty = YES;
}
static void mn_remove_all(id self, SEL c) {
    while (mn_count(self, c) > 0) mn_remove_at(self, c, mn_count(self, c) - 1);
}
static id mi_menu(id self, SEL c) { (void)c; return mn(self)->owner; }
static id mn_delegate(id self, SEL c) { (void)c; return mn(self)->delegate; }
static void mn_set_delegate(id self, SEL c, id d) { (void)c; mn(self)->delegate = d; }
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

static u32 menu_id(id item);
u32 cx_menu_item_action(id item) {
    if (!item) return 0;
    if (mn(item)->sep) return 0;
    return menu_id(item);
}
void cx_menu_tab_forget(id item) {
    u32 i;
    if (!item) return;
    for (i = 0; i < nmenu_tab; i++)
        if (menu_tab[i] == item) { objc_release(item); menu_tab[i] = 0; return; }
}
static u32 menu_id(id item) {
    u32 i, hole = nmenu_tab;
    for (i = 0; i < nmenu_tab; i++) {
        if (menu_tab[i] == item) return i + 1;
        if (!menu_tab[i] && hole == nmenu_tab) hole = i;
    }
    if (hole < nmenu_tab) { menu_tab[hole] = objc_retain(item); return hole + 1; }
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
/* validação dos itens da barra (autoenablesItems + validateMenuItem: pela cadeia), como o Cocoa faz ao abrir o
 * menu: aqui a barra é desenhada pelo window server, então o estado é recalculado a cada 0,4 s e republicado só
 * quando muda */
int cx_menu_item_valid(id menu, id item, id from_view);
static void revalidate_menu(id menu, int depth) {
    if (!menu || depth > 6) return;
    nsarr_t *its = (nsarr_t *)mn(menu)->items;
    BOOL autoen = cx_responds(menu->isa, sel_registerName("autoenablesItems")) ? MSG0(BOOL, menu, "autoenablesItems") : YES;
    for (u64 i = 0; i < its->n; i++) {
        id it = its->items[i];
        menust_t *m = mn(it);
        if (m->submenu) revalidate_menu(m->submenu, depth + 1);
        if (m->sep || !m->action) continue;
        BOOL off = autoen ? !cx_menu_item_valid(menu, it, 0) : NO;
        if (off != m->auto_off) { m->auto_off = off; menus_dirty = YES; }
    }
}
static void sync_menus(void) {
    if (app.menu) {
        static double last;
        double now = cx_now();
        if (now - last > 0.4) { last = now; revalidate_menu(app.menu, 0); extern void cx_toolbar_revalidate(id win); cx_toolbar_revalidate(g_key_window); }
    }
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
            u32 act = m->sep ? 0 : menu_id(its->items[k]), fl = ((m->disabled || m->auto_off) ? 1 : 0) | (m->state ? 2 : 0);
            char sc[32];
            shortcut_text(m->key, mi_mask_get(its->items[k], 0), sc, sizeof sc);
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
    if (m->auto_off || m->disabled) return NO;
    id t = m->target;
    if (!t) {   /* cadeia completa: first responder → nextResponder… → janela → delegate → controlador → documento → NSApp → delegate → NSDocumentController */
        extern id cx_menu_find_target(SEL a, id from_view);
        id w = key_window_obj();
        t = cx_menu_find_target(m->action, w ? ws(w)->first : 0);
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
    if (k == GUI_MENU_HIDE) { extern void cx_app_menu_hide(void); cx_app_menu_hide(); return; }   /* ⌘H: appkit_app_more.c */
    if (k >= 1 && (u32)k <= nmenu_tab && menu_tab[k - 1]) perform_item(menu_tab[k - 1]);
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

typedef struct { int w, h; u32 *px; double dw, dh; void *cg; BOOL tmpl; } imgst_t;   /* cg: CGImage em cache (CGImageForProposedRect:); tmpl: isTemplate */
static imgst_t *im(id o) { if (!ST(o)) ST(o) = calloc(1, sizeof(imgst_t)); return ST(o); }
u32 *cx_icns_argb(const u8 *d, u64 n, int want, int *w, int *h);   /* appkit_icons.c */
static BOOL decode_into(id img, const u8 *data, u64 n) {
    if (n >= 8 && !memcmp(data, "icns", 4)) {   /* .icns: o melhor elemento até 512 px */
        int w = 0, h = 0;
        u32 *px = cx_icns_argb(data, n, 512, &w, &h);
        if (!px) return NO;
        imgst_t *s = im(img);
        free(s->px);
        s->px = px; s->w = w; s->h = h; s->dw = w; s->dh = h;
        return YES;
    }
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
/* ---- ponte com o Core Graphics: -initWithCGImage:size: e -CGImageForProposedRect:context:hints: */
#include "appkit_image_core.h"
int cx_cgimage_src(const void *img, const u8 **px, long *bpr, int *w, int *h, int *premul);   /* coregraphics.c */
void *CGImageCreate(size_t w, size_t h, size_t bpc, size_t bpp, size_t bpr, const void *space, u32 info,
                    void *provider, const void *decode, _Bool interp, int intent);
void *CGDataProviderCreateWithData(void *info, const void *data, size_t size, void (*release)(void *, const void *, size_t));
void CGDataProviderRelease(void *p);
void *CGColorSpaceCreateDeviceRGB(void);
void CGColorSpaceRelease(void *s);
static id im_init_cg(id self, SEL c, void *cg, NSSize z) {
    (void)c;
    const u8 *src; long bpr; int w, h, premul;
    if (!cg || !cx_cgimage_src(cg, &src, &bpr, &w, &h, &premul) || w <= 0 || h <= 0) return 0;
    imgst_t *s = im(self);
    u32 *px = malloc((size_t)w * (size_t)h * 4);
    if (!px) return 0;
    cx_px_import(src, bpr, w, h, premul, px);
    free(s->px);
    s->px = px; s->w = w; s->h = h;
    cx_px_size(z.w, z.h, w, h, &s->dw, &s->dh);
    s->cg = 0;
    return self;
}
/* appkit_offscreen.c (unlockFocus): troca os pixels da imagem pelos do bitmap em que o app desenhou */
void cx_nsimage_set_pixels(id img, const u8 *src, long bpr, int w, int h, int premul) {
    if (!img || !src || w <= 0 || h <= 0) return;
    imgst_t *s = im(img);
    u32 *px = malloc((size_t)w * (size_t)h * 4);
    if (!px) return;
    cx_px_import(src, bpr, w, h, premul, px);
    free(s->px);
    s->px = px; s->w = w; s->h = h;
    if (s->dw <= 0 || s->dh <= 0) { s->dw = w; s->dh = h; }
    s->cg = 0;   /* o CGImage em cache era dos pixels antigos (fica com quem já o pegou) */
}
static void im_cg_release(void *info, const void *data, size_t n) { (void)info; (void)n; free((void *)data); }
/* o CGImage pertence à imagem (como no Cocoa, quem chama não libera); ARGB sem pré-multiplicar, 32 bits little */
static void *im_cg_for_rect(id self, SEL c, NSRect *proposed, id ctx, id hints) {
    (void)c; (void)proposed; (void)ctx; (void)hints;
    imgst_t *s = im(self);
    if (s->cg) return s->cg;
    if (!s->px || s->w <= 0 || s->h <= 0) return 0;
    size_t n = (size_t)s->w * (size_t)s->h * 4;
    u32 *copy = malloc(n);
    if (!copy) return 0;
    memcpy(copy, s->px, n);
    void *prov = CGDataProviderCreateWithData(0, copy, n, im_cg_release);
    if (!prov) { free(copy); return 0; }
    void *space = CGColorSpaceCreateDeviceRGB();
    s->cg = CGImageCreate((size_t)s->w, (size_t)s->h, 8, 32, (size_t)s->w * 4, space, 4u | (2u << 12), prov, 0, 1, 0);
    CGDataProviderRelease(prov);
    CGColorSpaceRelease(space);
    return s->cg;
}
/* pelo initWithData: (que o appkit_more.c estende para PDF/EPS) */
static id im_init_file(id self, SEL c, id path) {
    (void)c;
    id d = path ? MSG(id, cx_class_named("NSData"), "dataWithContentsOfFile:", path) : 0;
    return d ? MSG(id, self, "initWithData:", d) : 0;
}
static id im_init_data(id self, SEL c, id data) { (void)c; nsdata_t *d = (nsdata_t *)data; return d && decode_into(self, d->bytes, d->len) ? self : 0; }
static id im_named(id cls, SEL c, id name) {
    (void)c;
    id bundle = MSG0(id, cx_class_named("NSBundle"), "mainBundle");
    id res = MSG0(id, bundle, "resourcePath");
    static const char *exts[] = {"", ".png", "@2x.png", ".jpg", ".jpeg", ".gif", ".bmp", ".tiff", ".tif", ".icns", ".pdf"};
    for (u32 i = 0; name && res && i < ARRAYN(exts); i++) {
        char p[1200];
        snprintf(p, sizeof p, "%s/%s%s", cx_cstr(res), cx_cstr(name), exts[i]);
        if (access(p, 0) == 0) { id o = im_init_file(MSG0(id, cls, "alloc"), 0, cx_str(p)); if (o) return o; }
    }
    extern id cx_named_image_more(id cls, id name);   /* appkit_icons.c: imagens do sistema e nomes registrados */
    return name ? cx_named_image_more(cls, name) : 0;
}
static id im_init_size(id self, SEL c, NSSize z) { (void)c; imgst_t *s = im(self); s->dw = z.w; s->dh = z.h; return self; }
static NSSize im_size(id self, SEL c) { (void)c; imgst_t *s = im(self); NSSize z = {s->dw, s->dh}; return z; }
static void im_set_size(id self, SEL c, NSSize z) { (void)c; im(self)->dw = z.w; im(self)->dh = z.h; }
static BOOL im_valid(id self, SEL c) { (void)c; return im(self)->px != 0; }
/* desenha a imagem escalada para um retângulo em pixels */
/* cor das imagens template desenhadas por controles (0 = sem tinta: a imagem vai com as cores dela) */
static u32 g_img_tint;
static void blit_image(imgst_t *s, double x, double y, double w, double h, double alpha, u32 op) {
    if (!s->px || w < 1 || h < 1) return;
    u32 tint = s->tmpl ? g_img_tint : 0;
    int dw = (int)(w + 0.5), dh = (int)(h + 0.5);
    if (dw > 2048) dw = 2048;
    if (dh > 2048) dh = 2048;
    if (op > 28) op = 2;
    u32 *buf = malloc((size_t)dw * dh * 4 + 16);
    if (!buf) return;
    u32 a8 = (u32)(alpha * 255 + 0.5);
    for (int j = 0; j < dh; j++) {
        int sy = (int)((j + 0.5) * s->h / dh);
        for (int i = 0; i < dw; i++) {
            int sx = (int)((i + 0.5) * s->w / dw);
            u32 p = s->px[sy * s->w + sx];
            if (tint) p = (((p >> 24) * (tint >> 24) / 255) << 24) | (tint & 0xFFFFFF);   /* template: só o alfa conta */
            if (a8 < 255) p = (((p >> 24) * a8 / 255) << 24) | (p & 0xFFFFFF);
            buf[j * dw + i] = p;
        }
    }
    u32 bytes = (u32)dw * dh * 4;
    /* op 2 continua no blit source-over do kernel (SSE). Os outros modos vão no caminho escalar. */
    if (op == 2) {
        u8 *b = malloc(16 + bytes);
        i32 r[4] = {(i32)x, (i32)y, dw, dh};
        if (b) {
            memcpy(b, r, 16);
            memcpy(b + 16, buf, bytes);
            put(GC_IMAGE, b, 16 + bytes);
            free(b);
        }
    } else {
        u8 *b = malloc(20 + bytes);
        i32 r[4] = {(i32)x, (i32)y, dw, dh};
        if (b) {
            memcpy(b, r, 16);
            memcpy(b + 16, &op, 4);
            memcpy(b + 20, buf, bytes);
            put(GC_COMP_IMAGE, b, 20 + bytes);
            free(b);
        }
    }
    free(buf);
}
static void im_draw_in(id self, SEL c, NSRect r) { (void)c; double x, y, w, h; px_rect(r, &x, &y, &w, &h); blit_image(im(self), x, y, w, h, 1, 2); }
static void im_draw_in_frac(id self, SEL c, NSRect r, NSRect from, u64 op, double f) { (void)c; (void)from; double x, y, w, h; px_rect(r, &x, &y, &w, &h); blit_image(im(self), x, y, w, h, f, (u32)op); }
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
    blit_image(s, x + (w - iw) / 2, y + (h - ih) / 2, iw, ih, 1, 2);
}
/* imagem de controle: NSImageScaling (0 proporcional só para baixo, 1 eixos independentes, 2 nenhuma, 3 proporcional
 * para cima ou para baixo), centrada; tint pinta as imagens template */
static void draw_image_ctl(id img, double x, double y, double w, double h, int scaling, u32 tint) {
    imgst_t *s = im(img);
    if (!s->px || s->dw <= 0 || s->dh <= 0) return;
    double iw = s->dw, ih = s->dh;
    if (scaling == 1) { iw = w; ih = h; }
    else if (scaling == 3 || (scaling == 0 && (s->dw > w || s->dh > h))) { double a = w / s->dw, b = h / s->dh, sc = a < b ? a : b; iw = s->dw * sc; ih = s->dh * sc; }
    g_img_tint = tint;
    blit_image(s, x + (w - iw) / 2, y + (h - ih) / 2, iw, ih, 1, 2);
    g_img_tint = 0;
}
static void tf_set_lbm(id self, SEL c, u64 m) { (void)c; vs(self)->tf_lbm1 = m <= 5 ? (int)m + 1 : 0; mark(self); }
static u64 tf_lbm(id self, SEL c) { (void)c; vstate_t *s = vs(self); return s->tf_lbm1 ? (u64)(s->tf_lbm1 - 1) : (s->editable || s->bezeled ? TFIT_CLIP : TFIT_WORD); }
static void tf_set_max_lines(id self, SEL c, i64 n) { (void)c; vs(self)->tf_max_lines = n < 0 ? 0 : n; mark(self); }
static i64 tf_max_lines(id self, SEL c) { (void)c; return vs(self)->tf_max_lines; }
static void tf_set_single(id self, SEL c, BOOL v) { (void)c; vs(self)->tf_single = v ? YES : NO; mark(self); }
static BOOL tf_single(id self, SEL c) { (void)c; return vs(self)->tf_single; }
static void tf_set_pref_w(id self, SEL c, double w) { (void)c; vs(self)->tf_pref_w = w > 0 ? w : 0; mark(self); }
static double tf_pref_w(id self, SEL c) { (void)c; return vs(self)->tf_pref_w; }
static void v_set_alpha(id self, SEL c, double a) {
    (void)c;
    vstate_t *s = vs(self);
    s->valpha = (float)(a < 0 ? 0 : a > 1 ? 1 : a);
    s->valpha_set = s->valpha < 0.999f;
    mark(self);
}
static double v_alpha(id self, SEL c) { (void)c; vstate_t *s = vs(self); return s->valpha_set ? s->valpha : 1.0; }
static void im_set_template(id self, SEL c, BOOL v) { (void)c; im(self)->tmpl = v ? YES : NO; }
static BOOL im_is_template(id self, SEL c) { (void)c; return im(self)->tmpl; }
static void iv_set_scaling(id self, SEL c, u64 v) { (void)c; vs(self)->img_scaling = v <= 3 ? (int)v : 0; mark(self); }
static u64 iv_scaling(id self, SEL c) { (void)c; return (u64)vs(self)->img_scaling; }
static void v_set_tint(id self, SEL c, id col) { (void)c; keep(&vs(self)->tint, col); mark(self); }
static id v_tint(id self, SEL c) { (void)c; return vs(self)->tint; }
const u32 *cx_nsimage_pixels(id img, int *w, int *h) {
    imgst_t *s;
    if (w) *w = 0;
    if (h) *h = 0;
    if (!img) return 0;
    s = im(img);
    if (!s->px || s->w <= 0 || s->h <= 0) return 0;
    if (w) *w = s->w;
    if (h) *h = s->h;
    return s->px;
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
static void sv_set_has_v(id self, SEL c, BOOL v) { (void)c; vstate_t *s = vs(self); s->sv_vset = YES; s->sv_v = v ? YES : NO; mark(self); }
static BOOL sv_has_v(id self, SEL c) { (void)c; vstate_t *s = vs(self); return s->sv_vset ? s->sv_v : NO; }
static void sv_set_has_h(id self, SEL c, BOOL v) { (void)c; vstate_t *s = vs(self); s->sv_hset = YES; s->sv_h = v ? YES : NO; mark(self); }
static BOOL sv_has_h(id self, SEL c) { (void)c; vstate_t *s = vs(self); return s->sv_hset ? s->sv_h : NO; }
static void sv_set_autohide(id self, SEL c, BOOL v) { (void)c; vs(self)->sv_autohide = v ? YES : NO; mark(self); }
static BOOL sv_autohide(id self, SEL c) { (void)c; return vs(self)->sv_autohide; }
static void sv_set_border(id self, SEL c, u64 b) { (void)c; vs(self)->sv_border = b <= 3 ? (int)b : 0; mark(self); }
static u64 sv_border(id self, SEL c) { (void)c; return (u64)vs(self)->sv_border; }
static void sv_set_draws_bg(id self, SEL c, BOOL v) { (void)c; vs(self)->draws_bg = v ? YES : NO; mark(self); }
static BOOL sv_draws_bg(id self, SEL c) { (void)c; return vs(self)->draws_bg; }
static void sv_set_bg(id self, SEL c, id col) { (void)c; keep(&vs(self)->bg_color, col); mark(self); }
static id sv_bg(id self, SEL c) { (void)c; id col = vs(self)->bg_color; return col ? col : cx_color(1, 1, 1, 1); }
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
    double maxx = d->frame.s.w - s->frame.s.w;   /* documento mais largo que a área: rola na horizontal */
    if (maxx < 0) maxx = 0;
    if (s->scroll_x > maxx) s->scroll_x = maxx;
    if (s->scroll_x < 0) s->scroll_x = 0;
    d->frame.o.x = -s->scroll_x;
    d->frame.o.y = s->frame.s.h - d->frame.s.h + s->scroll_y;   /* topo do documento alinhado ao topo visível */
    if (d->kind == K_TABLE) tv_layout_views(s->doc);
}
static void sv_scroll(id self, SEL c, id e) {
    (void)c;
    vstate_t *s = vs(self);
    s->scroll_y += -evs(e)->dy * 40;
    s->scroll_x += -evs(e)->dx * 40;
    layout_scroll(self);
    mark(self);
}

/* ================================================================ NSTableView */
typedef struct { id ident, title; double width; } colst_t;
/* appkit_table2.inc (incluído mais abaixo): geometria por linha, seleção e desenho extra */
static double tv_row_top(id t, i64 r);
static double tv_row_height(id t, i64 r);
static double tv_total_h(id t);
static i64 tv_row_at_y(id t, double y);
static void tv_draw_drop_line(id t, i64 r, double x0, double ry, double w, double h);
static int tv_draw_cell_based(id t, id col, i64 r, double x, double y, double w, double h, BOOL sel);
static void tv_draw_sort_indicator(id t, id col, double x, double hy, double w);
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
    s->kind = K_TABLE; s->row_h = 24; s->selected = -1; s->header = YES; s->alt_rows = NO; s->tv_dirty = 1;
    s->cols = objc_retain(cx_arr_new(&NSMutableArray_c));
    return self;
}
static void tv_add_col(id self, SEL c, id col) { (void)c; cx_arr_add(vs(self)->cols, col); vs(self)->tv_dirty = 1; mark(self); }
static id tv_cols(id self, SEL c) { (void)c; return vs(self)->cols; }
static void tv_set_ds(id self, SEL c, id d) { (void)c; vs(self)->data_source = d; vs(self)->tv_dirty = 1; mark(self); }
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
    s->frame.s.h = (s->header ? 24 : 0) + tv_total_h(t);
}
static void tv_reload(id self, SEL c) {
    (void)c;
    vstate_t *s = vs(self);
    s->tv_dirty = 1;
    if (s->selected >= tv_rows(self)) s->selected = -1;
    if (s->superview && vs(s->superview)->kind == K_SCROLL) layout_scroll(s->superview);
    else tv_layout_views(self);
    mark(s->superview ? s->superview : self);
}
static i64 tv_selected(id self, SEL c) { (void)c; return vs(self)->selected; }
static i64 tv_clicked(id self, SEL c) { (void)c; return vs(self)->clicked; }
static void tv_set_row_h(id self, SEL c, double h) { (void)c; vs(self)->row_h = h > 0 ? h : 24; vs(self)->tv_dirty = 1; mark(self); }
static double tv_row_h(id self, SEL c) { (void)c; return vs(self)->row_h > 0 ? vs(self)->row_h : 24; }
static BOOL tv_is_flipped(id self, SEL c) { (void)self; (void)c; return YES; }
const char cx_tv_header_key = 0;
static void tv_set_header(id self, SEL c, id h) {
    (void)c;
    vstate_t *s = vs(self);
    if (s->header_view && s->header_view != self) objc_setAssociatedObject(s->header_view, &cx_tv_header_key, 0, 0);
    s->header = h != 0;
    if (h && h != self && cx_is_subclass(h->isa, &NSView_c)) {
        keep(&s->header_view, h);
        objc_setAssociatedObject(h, &cx_tv_header_key, self, 0);   /* ASSIGN: o header não retém a tabela */
    } else keep(&s->header_view, 0);
    s->tv_dirty = 1;
    mark(self);
}
static id tv_header(id self, SEL c) { (void)c; vstate_t *s = vs(self); if (s->header_view) return s->header_view; return s->header ? self : 0; }
static void tv_set_delegate(id self, SEL c, id d) { ctl_set_delegate(self, c, d); vs(self)->tv_dirty = 1; mark(self); }
static void tv_set_alt(id self, SEL c, BOOL b) { (void)c; vs(self)->alt_rows = b; mark(self); }
static void tv_set_double(id self, SEL c, SEL a) { (void)c; vs(self)->double_action = a; }

static const char *tv_ident_cstr(id ident) {
    if (!ident || !cx_is_str(ident)) return 0;
    const char *s = cx_cstr(ident);
    return s && s[0] && strcmp(s, "(null)") ? s : 0;
}
static int tv_is_dict(id o) { return o && cx_responds(o->isa, sel_registerName("objectForKey:")) && cx_responds(o->isa, sel_registerName("allKeys")); }
static int tv_is_array(id o) { return o && !tv_is_dict(o) && cx_responds(o->isa, sel_registerName("objectAtIndex:")) && cx_responds(o->isa, sel_registerName("count")); }
static int tv_is_data(id o) { return o && !cx_is_str(o) && cx_responds(o->isa, sel_registerName("bytes")) && cx_responds(o->isa, sel_registerName("length")); }
static int tv_is_nib(id o) { return o && cx_responds(o->isa, sel_registerName("instantiateWithOwner:topLevelObjects:")); }
static int tv_is_outline(id self) {
    cx_class_t *c = cx_class_named("NSOutlineView");
    return c && self && cx_is_subclass(self->isa, c);
}
static void tv_register_token(id table, const char *ident, id token, int kind) {
    if (!table || !ident || !ident[0] || !token) return;
    cx_tv_engine_t *e = cx_tv_engine_get(table, 1);
    if (!e) return;
    if (cx_tv_engine_token(e, ident, 0) == (void *)token) return;
    id held = objc_retain(token);
    void *old = cx_tv_engine_register(e, ident, held, kind);
    if (cx_tv_engine_token(e, ident, 0) != (void *)held) { objc_release(held); return; }
    if (old) objc_release((id)old);
}
static void tv_install_one(id table, id key, id val) {
    if (!val) return;
    const char *ident = tv_ident_cstr(key);
    if (!ident && cx_is_subclass(val->isa, &NSView_c)) ident = tv_ident_cstr(vs(val)->identifier);
    int kind = tv_is_data(val) ? CX_TV_NIBDATA : tv_is_nib(val) ? CX_TV_NIB : cx_is_subclass(val->isa, &NSView_c) ? CX_TV_PROTO : 0;
    if (kind) tv_register_token(table, ident, val, kind);
}
static void tv_install_archived(id table, id arch) {
    if (!table || !arch) return;
    if (tv_is_dict(arch)) {
        id keys = MSG0(id, arch, "allKeys");
        u64 n = keys ? MSG0(u64, keys, "count") : 0;
        for (u64 i = 0; i < n; i++) {
            id k = MSG(id, keys, "objectAtIndex:", i);
            tv_install_one(table, k, MSG(id, arch, "objectForKey:", k));
        }
        return;
    }
    if (tv_is_array(arch)) {
        u64 n = MSG0(u64, arch, "count");
        for (u64 i = 0; i < n; i++) tv_install_one(table, 0, MSG(id, arch, "objectAtIndex:", i));
        return;
    }
    tv_install_one(table, 0, arch);
}
static void tv_register_nib(id self, SEL c, id nib, id ident) { (void)c; tv_register_token(self, tv_ident_cstr(ident), nib, CX_TV_NIB); }

/* cópia estrutural de um protótipo. Os outlets da subclasse do app (ivars que apontam para
 * subviews) passam a apontar para os clones, sem reinstanciar o nib. */
extern void **class_copyIvarList(cx_class_t *c, u32 *count);
extern const char *ivar_getTypeEncoding(void *iv);
extern id object_getIvar(id o, void *iv);
extern void object_setIvar(id o, void *iv, id v);
extern cx_class_t *class_getSuperclass(cx_class_t *c);
static int tv_collect_pair(id src, id dst, id *ss, id *dd, int n, int cap) {
    if (!src || !dst || n >= cap) return n;
    ss[n] = src;
    dd[n] = dst;
    n++;
    vstate_t *a = vs(src), *b = vs(dst);
    u32 m = a->nsub < b->nsub ? a->nsub : b->nsub;
    for (u32 i = 0; i < m; i++) n = tv_collect_pair(a->sub[i], b->sub[i], ss, dd, n, cap);
    return n;
}
static void tv_remap_ivars(id src, id dst, id *ss, id *dd, int n) {
    if (!src || !dst) return;
    for (cx_class_t *k = src->isa; k; k = class_getSuperclass(k)) {
        u32 ni = 0;
        void **ivs = class_copyIvarList(k, &ni);
        if (!ivs) continue;
        for (u32 i = 0; i < ni; i++) {
            const char *ty = ivar_getTypeEncoding(ivs[i]);
            if (!ty || ty[0] != '@') continue;
            id val = object_getIvar(src, ivs[i]);
            void *mapped = cx_tv_remap_outlet(val, (void *const *)ss, (void *const *)dd, n);
            if (mapped) object_setIvar(dst, ivs[i], (id)mapped);
        }
        free(ivs);
    }
}
static id tv_clone_view(id src) {
    if (!src || !cx_is_subclass(src->isa, &NSView_c)) return 0;
    id dst = MSG(id, MSG0(id, (id)src->isa, "alloc"), "initWithFrame:", vs(src)->frame);
    if (!dst || !cx_is_subclass(dst->isa, &NSView_c)) return dst;
    vstate_t *a = vs(src), *b = vs(dst);
    b->hidden = a->hidden; b->editable = a->editable; b->bezeled = a->bezeled; b->draws_bg = a->draws_bg; b->alignment = a->alignment;
    if (a->identifier) keep(&b->identifier, a->identifier);
    if (a->font && cx_responds(dst->isa, sel_registerName("setFont:"))) MSG(void, dst, "setFont:", a->font);
    if (a->title && cx_responds(dst->isa, sel_registerName("setTitle:"))) MSG(void, dst, "setTitle:", a->title);
    if (a->value && cx_responds(dst->isa, sel_registerName("setStringValue:"))) MSG(void, dst, "setStringValue:", a->value);
    if (a->image && cx_responds(dst->isa, sel_registerName("setImage:"))) MSG(void, dst, "setImage:", a->image);
    if (a->text_color && cx_responds(dst->isa, sel_registerName("setTextColor:"))) MSG(void, dst, "setTextColor:", a->text_color);
    if (a->bg_color && cx_responds(dst->isa, sel_registerName("setBackgroundColor:"))) MSG(void, dst, "setBackgroundColor:", a->bg_color);
    u32 n = a->nsub;
    id *clones = n ? calloc(n, sizeof(id)) : 0;
    for (u32 i = 0; i < n; i++) {
        clones[i] = tv_clone_view(a->sub[i]);
        if (clones[i]) { v_add_sub(dst, 0, clones[i]); objc_release(clones[i]); }
    }
    if (a->text_field && cx_responds(dst->isa, sel_registerName("setTextField:")))
        for (u32 i = 0; i < n; i++) if (a->sub[i] == a->text_field && clones[i]) MSG(void, dst, "setTextField:", clones[i]);
    if (cx_responds(src->isa, sel_registerName("imageView")) && cx_responds(dst->isa, sel_registerName("setImageView:"))) {
        id iv = MSG0(id, src, "imageView");
        for (u32 i = 0; iv && i < n; i++) if (a->sub[i] == iv && clones[i]) MSG(void, dst, "setImageView:", clones[i]);
    }
    free(clones);
    id ss[128], dd[128];
    int np = tv_collect_pair(src, dst, ss, dd, 0, 128);
    tv_remap_ivars(src, dst, ss, dd, np);
    return dst;
}
static id tv_materialize(id self, id ident, id owner, void *token, int kind);
static id tv_make_view(id self, SEL c, id ident, id owner) {
    (void)c;
    const char *cs = tv_ident_cstr(ident);
    if (!cs) return 0;
    cx_tv_engine_t *e = cx_tv_engine_get(self, 1);
    if (!e) return 0;
    id v = cx_tv_engine_take(e, cs);
    if (v) {
        if (cx_responds(v->isa, sel_registerName("prepareForReuse"))) MSG0(void, v, "prepareForReuse");
        return objc_autorelease(v);
    }
    int kind = 0;
    void *tok = cx_tv_engine_token(e, cs, &kind);
    if (!tok) return 0;
    v = tv_materialize(self, ident, owner ? owner : self, tok, kind);
    if (!v) return 0;
    if (cx_is_subclass(v->isa, &NSView_c)) MSG(void, v, "setIdentifier:", ident);
    return objc_autorelease(v);
}
static int tv_in_scroll(id self) {
    id sv = vs(self)->superview;
    return sv && vs(sv)->kind == K_SCROLL;
}
static void tv_scroll_row(id self, SEL c, i64 r) {
    (void)c;
    vstate_t *s = vs(self);
    cx_tv_engine_t *e = cx_tv_engine_get(self, 1);
    if (!e) return;
    int in_sc = tv_in_scroll(self);
    double header = s->header ? 24 : 0;
    double row_h = s->row_h > 0 ? s->row_h : 24;
    double cur = in_sc ? vs(s->superview)->scroll_y : cx_tv_engine_own_scroll(e);
    double view_h = in_sc ? vs(s->superview)->frame.s.h : s->frame.s.h;
    double next = cx_tv_reveal_row(e, in_sc, cur, view_h, row_h, header, (int)r, (int)tv_rows(self));
    if (r >= 0 && r < tv_rows(self)) {   /* alturas por linha */
        double top = header + tv_row_top(self, r), bot = top + tv_row_height(self, r);
        next = cur;
        if (top - header < cur) next = top - header;
        else if (bot > cur + view_h) next = bot - view_h;
    }
    if (in_sc) {
        vs(s->superview)->scroll_y = next;
        layout_scroll(s->superview);
        mark(s->superview);
    } else {
        s->tv_dirty = 1;
        mark(self);
    }
}
/* NSIndexSet: Foundation (fdn_indexset.c) */
static int tv_sel_contains(id t, i64 r);
static void tv_sel_only(id t, i64 r, BOOL notify);
static void select_row(id t, i64 r, BOOL notify) { tv_sel_only(t, r, notify); }
static void tv_select(id self, SEL c, id ixs, BOOL ext) { (void)c; (void)ext; select_row(self, ixs && MSG0(u64, ixs, "count") ? (i64)MSG0(u64, ixs, "firstIndex") : -1, NO); }
static void tv_deselect_all(id self, SEL c, id s) { (void)c; (void)s; select_row(self, -1, NO); }
/* texto da célula cell-based. A view-based não passa por aqui: o layout pergunta viewFor e as subviews se desenham. */
static id cell_text(id t, id col, i64 row) {
    vstate_t *s = vs(t);
    id ds = s->data_source;
    if (ds && cx_responds(ds->isa, sel_registerName("tableView:objectValueForTableColumn:row:"))) {
        id v = MSG(id, ds, "tableView:objectValueForTableColumn:row:", t, col, row);
        return v ? MSG0(id, v, "description") : 0;
    }
    return 0;
}
static int tv_in_layout;
static void tv_reclaim(id self) {
    cx_tv_engine_t *e = cx_tv_engine_get(self, 0);
    if (!e) return;
    int n = cx_tv_engine_placed_count(e);
    for (int i = 0; i < n; i++) {
        id v = cx_tv_engine_placed_at(e, i);
        if (!v || !cx_is_subclass(v->isa, &NSView_c)) continue;
        const char *cs = tv_ident_cstr(vs(v)->identifier);
        if (cs) { objc_retain(v); v_remove(v, 0); cx_tv_engine_put(e, cs, v); }
        else v_remove(v, 0);
    }
    cx_tv_engine_placed_clear(e);
}
static void tv_outline_metrics(id self, i64 row, int *level, double *per, id *ocol) {
    *level = -1; *per = 16; *ocol = 0;
    if (!tv_is_outline(self)) return;
    *level = 0;
    if (cx_responds(self->isa, sel_registerName("indentationPerLevel"))) {
        double p = MSG0(double, self, "indentationPerLevel");
        if (p > 0) *per = p;
    }
    if (cx_responds(self->isa, sel_registerName("levelForRow:"))) *level = (int)MSG(i64, self, "levelForRow:", row);
    if (*level < 0) *level = 0;
    if (cx_responds(self->isa, sel_registerName("outlineTableColumn"))) *ocol = MSG0(id, self, "outlineTableColumn");
}
/* tableView:isGroupRow: (o adaptador do outline traduz para outlineView:isGroupItem:) */
static int tv_is_group_row(id self, i64 r) {
    id d = vs(self)->delegate;
    return d && cx_responds(d->isa, sel_registerName("tableView:isGroupRow:")) && MSG(BOOL, d, "tableView:isGroupRow:", self, r);
}
/* como no Cocoa: a view de célula recebe objectValue do dataSource (no outline, o item quando o
 * dataSource não dá valor) */
static void tv_set_cell_value(id self, id v, id col, i64 r) {
    if (!v || !cx_responds(v->isa, sel_registerName("setObjectValue:"))) return;
    id ds = vs(self)->data_source, val = 0;
    if (ds && cx_responds(ds->isa, sel_registerName("tableView:objectValueForTableColumn:row:")))
        val = MSG(id, ds, "tableView:objectValueForTableColumn:row:", self, col, r);
    if (!val && tv_is_outline(self) && cx_responds(self->isa, sel_registerName("itemAtRow:"))) val = MSG(id, self, "itemAtRow:", r);
    MSG(void, v, "setObjectValue:", val);
}
static void tv_layout_views(id self) {
    if (!self || !ST(self) || tv_in_layout) return;
    vstate_t *s = vs(self);
    if (s->kind != K_TABLE) return;
    int in_sc = tv_in_scroll(self);
    cx_tv_engine_t *e = cx_tv_engine_get(self, 1);
    if (!e) return;
    double scroll_y = in_sc ? vs(s->superview)->scroll_y : cx_tv_engine_own_scroll(e);
    double view_h = in_sc ? vs(s->superview)->frame.s.h : s->frame.s.h;
    double header = s->header ? 24 : 0, row_h = s->row_h > 0 ? s->row_h : 24;
    i64 nrows = tv_rows(self);
    cx_tv_span_t sp = cx_tv_visible_rows(scroll_y, view_h, row_h, header, (int)nrows);
    {   /* alturas por linha: a faixa visível sai da geometria */
        i64 a0 = tv_row_at_y(self, scroll_y - header), b0 = tv_row_at_y(self, scroll_y - header + view_h);
        if (a0 < 0) a0 = scroll_y - header < 0 ? 0 : nrows;
        if (b0 < 0) b0 = nrows - 1;
        sp.first = nrows > 0 && a0 <= b0 ? (int)a0 : -1; sp.last = sp.first >= 0 ? (int)b0 : -1;
    }
    /* quem dá as views: o delegate ou, sem ele, a fonte de dados (NSTableViewDiffableDataSource do overlay Swift) */
    id vprov = s->delegate && cx_responds(s->delegate->isa, sel_registerName("tableView:viewForTableColumn:row:")) ? s->delegate :
               s->data_source && cx_responds(s->data_source->isa, sel_registerName("tableView:viewForTableColumn:row:")) ? s->data_source : 0;
    int want = vprov != 0;
    if (!want) {
        if (cx_tv_engine_placed_count(e)) { tv_in_layout = 1; tv_reclaim(self); tv_in_layout = 0; }
        cx_tv_engine_cells_clear(e);
        cx_tv_engine_note_span(e, -1, -1, scroll_y);
        s->tv_dirty = 0;
        return;
    }
    if (!s->tv_dirty && cx_tv_engine_same_span(e, sp.first, sp.last, scroll_y)) return;
    tv_in_layout = 1;
    tv_reclaim(self);
    cx_tv_engine_cells_clear(e);
    s->tv_dirty = 0;   /* um reload no meio do loop marca de novo e o próximo desenho refaz */
    nsarr_t *cols = (nsarr_t *)s->cols;
    u64 nc = cols ? cols->n : 0;
    for (int r = sp.first; r >= 0 && r <= sp.last; r++) {
        int level; double per; id ocol;
        tv_outline_metrics(self, r, &level, &per, &ocol);
        /* linha de grupo: uma view só, de coluna nil, na largura toda (tableView:isGroupRow:) */
        int group = tv_is_group_row(self, r);
        u64 passes = group ? 1 : nc ? nc : 1;
        double x = 0;
        for (u64 k = 0; k < passes; k++) {
            id col = nc && !group ? cols->items[k] : 0;
            double cw = col ? cs_(col)->width : s->frame.s.w;
            id v = MSG(id, vprov, "tableView:viewForTableColumn:row:", self, col, (i64)r);
            int got = v && cx_is_subclass(v->isa, &NSView_c);
            cx_tv_engine_note_cell(e, r, (int)k, got);
            if (got && !cx_tv_show_cell(got)) {
                int ocol_here = !group && level >= 0 && (ocol ? col == ocol : k == 0);
                double cell_x = cx_tv_cell_x(x, level, per, ocol_here);
                double rh = tv_row_height(self, r);
                double ry = header + tv_row_top(self, r) - (in_sc ? 0 : scroll_y);
                NSRect fr = r_make(cell_x, ry, cx_tv_cell_w(cw, x, cell_x), rh);
                ((void (*)(id, SEL, NSRect))objc_msgSend)(v, sel_registerName("setFrame:"), fr);
                tv_set_cell_value(self, v, col, r);
                if (vs(v)->superview != self) v_add_sub(self, 0, v);
                cx_tv_engine_placed_add_at(e, v, r, group ? -1 : (int)k);
            }
            x += cw;
        }
    }
    cx_tv_engine_note_span(e, sp.first, sp.last, scroll_y);
    tv_in_layout = 0;
}
static void draw_table(id t, absr_t *a) {
    tv_layout_views(t);
    g_dark_draw = cx_appearance_dark();
    accent = cx_appearance_accent();
    vstate_t *s = vs(t);
    nsarr_t *cols = (nsarr_t *)s->cols;
    i64 rows = tv_rows(t);
    cx_tv_engine_t *eng = cx_tv_engine_get(t, 0);
    int in_sc = s->superview && vs(s->superview)->kind == K_SCROLL;
    double own = (!in_sc && eng) ? cx_tv_engine_own_scroll(eng) : 0;
    double y = a->tly + (s->header ? 24 : 0), x0 = a->tlx;
    int outline = tv_is_outline(t);
    double row_hh = 0;
    for (i64 r = 0; r < rows; r++) {
        double ry = y + tv_row_top(t, r) - own;
        row_hh = tv_row_height(t, r);
        if (row_hh <= 0) continue;                                 /* linha oculta */
        if (ry + row_hh < clip_y0 || ry > clip_y1) continue;       /* fora da área visível da rolagem */
        BOOL sel = tv_sel_contains(t, r);
        if (sel) g_rrect(x0 + 4, ry + 1, a->w - 8, row_hh - 2, 5, accent);
        else if (s->alt_rows && (r & 1)) g_fill(x0, ry, a->w, row_hh, INK(0x08));
        tv_draw_drop_line(t, r, x0, ry, a->w, row_hh);
        int level = -1; double per = 16; id ocol = 0;
        if (outline) tv_outline_metrics(t, r, &level, &per, &ocol);
        double x = x0;
        for (u64 k = 0; cols && k < cols->n; k++) {
            colst_t *cs = cs_(cols->items[k]);
            int ocol_here = level >= 0 && (ocol ? cols->items[k] == ocol : k == 0);
            double local = x - x0;
            if (ocol_here && cx_responds(t->isa, sel_registerName("isExpandable:"))) {
                id item = cx_responds(t->isa, sel_registerName("itemAtRow:")) ? MSG(id, t, "itemAtRow:", r) : 0;
                if (MSG(BOOL, t, "isExpandable:", item)) {
                    double dx = x0 + cx_tv_disclosure_x(local, level, per, 1);
                    double mid = ry + row_hh / 2;
                    int open = cx_responds(t->isa, sel_registerName("isItemExpanded:")) && MSG(BOOL, t, "isItemExpanded:", item);
                    float tri[6];
                    if (open) { tri[0] = (float)(dx + 2); tri[1] = (float)(mid - 3); tri[2] = (float)(dx + 10); tri[3] = (float)(mid - 3); tri[4] = (float)(dx + 6); tri[5] = (float)(mid + 3); }
                    else { tri[0] = (float)(dx + 3); tri[1] = (float)(mid - 5); tri[2] = (float)(dx + 3); tri[3] = (float)(mid + 5); tri[4] = (float)(dx + 9); tri[5] = (float)mid; }
                    g_poly(tri, 3, sel ? 0xFFFFFFFF : D(0xFF666666, 0xFFAAAAAA), 1, 0);
                }
            }
            int got = eng ? cx_tv_engine_cell_view(eng, (int)r, (int)k) : 0;
            if (cx_tv_show_cell(got) && tv_draw_cell_based(t, cols->items[k], r, x0 + cx_tv_cell_x(local, level, per, ocol_here), ry,
                                                         cx_tv_cell_w(cs->width, local, cx_tv_cell_x(local, level, per, ocol_here)), row_hh, sel)) {
                /* célula de verdade (dataCell do app): já desenhada */
            } else if (cx_tv_show_cell(got)) {
                id txt = cell_text(t, cols->items[k], r);
                if (txt) {
                    const char *tt = cx_cstr(txt);
                    gui_metrics_t m = measure(tt, (u32)strlen(tt), 13, 0);
                    double tx = x0 + cx_tv_cell_x(local, level, per, ocol_here);
                    double tw = cx_tv_cell_w(cs->width, local, tx - x0);
                    set_clip(tx, ry, tw - 4 > 1 ? tw - 4 : 1, row_hh);
                    g_text(tx + 8, ry + (row_hh + m.ascent - m.descent) / 2, 13, 0, sel ? 0xFFFFFFFF : INK(0xE6), tt, (u32)strlen(tt));
                    set_clip(a->tlx, a->tly, a->w, a->h);
                }
            }
            x += cs->width;
        }
    }
    if (s->header) {                 /* cabeçalho fixo no topo da área visível (como no macOS) */
        double hy = a->tly;
        if (s->superview && vs(s->superview)->kind == K_SCROLL && clip_y0 > hy) hy = clip_y0;
        set_clip(a->tlx, a->tly, a->w, a->h);
        g_fill(x0, hy, a->w, 24, D(0xFFF5F5F7, 0xFF2A2A2C));
        g_fill(x0, hy + 23, a->w, 1, INK(0x22));
        double x = x0;
        for (u64 k = 0; k < cols->n; k++) {
            colst_t *cs = cs_(cols->items[k]);
            const char *tt = cs->title ? cx_cstr(cs->title) : "";
            gui_metrics_t m = measure(tt, (u32)strlen(tt), 11, 2);
            g_text(x + 8, hy + (24 + m.ascent - m.descent) / 2, 11, 2, INK(0xB0), tt, (u32)strlen(tt));
            tv_draw_sort_indicator(t, cols->items[k], x, hy, cs->width);
            x += cs->width;
            if (k + 1 < cols->n) g_fill(x - 1, hy + 4, 1, 16, INK(0x1A));
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
    if (!tv_in_scroll(self)) {
        cx_tv_engine_t *eng = cx_tv_engine_get(self, 0);
        if (eng) py += cx_tv_engine_own_scroll(eng);
    }
    i64 r = py < 0 ? -1 : (i64)(py / s->row_h);
    if (r >= tv_rows(self)) r = -1;
    if (r >= 0 && tv_is_outline(self)) {
        int level; double per; id ocol;
        tv_outline_metrics(self, r, &level, &per, &ocol);
        nsarr_t *cols = (nsarr_t *)s->cols;
        double x = 0, dx = 0;
        int found = 0;
        for (u64 k = 0; cols && k < cols->n; k++) {
            int here = ocol ? cols->items[k] == ocol : k == 0;
            if (here) { dx = cx_tv_disclosure_x(x, level, per, 1); found = 1; break; }
            x += cs_(cols->items[k])->width;
        }
        if (!found && (!cols || !cols->n)) dx = cx_tv_disclosure_x(0, level, per, 1);
        double lx = evs(e)->x - a.tlx;
        if (found || !cols || !cols->n) {
            if (lx >= dx && lx < dx + per && cx_responds(self->isa, sel_registerName("isExpandable:"))) {
                id item = cx_responds(self->isa, sel_registerName("itemAtRow:")) ? MSG(id, self, "itemAtRow:", r) : 0;
                if (MSG(BOOL, self, "isExpandable:", item)) {
                    if (cx_responds(self->isa, sel_registerName("isItemExpanded:")) && MSG(BOOL, self, "isItemExpanded:", item))
                        MSG(void, self, "collapseItem:", item);
                    else MSG(void, self, "expandItem:", item);
                }
            }
        }
    }
    s->clicked = r;
    if (r >= 0 && s->delegate && cx_responds(s->delegate->isa, sel_registerName("tableView:shouldSelectRow:")) &&
        !MSG(BOOL, s->delegate, "tableView:shouldSelectRow:", self, r))
        r = -2;                                   /* o delegate recusou: a seleção fica como estava */
    if (r >= 0) select_row(self, r, YES);
    if (evs(e)->clicks >= 2 && s->double_action && r >= 0) {
        id t = s->target ? s->target : (s->delegate ? s->delegate : app.delegate);
        if (t) ((void (*)(id, SEL, id))objc_msgSend)(t, s->double_action, self);
    } else if (s->action && r >= 0) send_action(self);
}
/* view de célula -> linha/coluna: sobe pelas superviews até achar uma view que o layout pôs */
#include "appkit_table2.inc"
static int tv_sel_contains(id t, i64 r) { return r >= 0 && MSG(BOOL, tv_selset(t), "containsIndex:", (u64)r); }
static void tv_sel_only(id t, i64 r, BOOL notify) {
    id one = r >= 0 ? MSG(id, (id)cx_class_named("NSIndexSet"), "indexSetWithIndex:", (u64)r) : new_indexset();
    if (r < 0) { BOOL keep = tvp_i(t, "allowsEmpty", 1); tvp_set_i(t, "allowsEmpty", 1); tv_apply_selection(t, one, notify); tvp_set_i(t, "allowsEmpty", keep); }
    else tv_apply_selection(t, one, notify);
}
/* célula do app (setDataCell: ou do nib) no modo cell-based: valor, willDisplayCell: e drawWithFrame:inView: */
static int tv_draw_cell_based(id t, id col, i64 r, double x, double y, double w, double h, BOOL sel) {
    if (!colp_o(col, "dataCellSet")) return 0;
    id cell = col2_data_cell(col, 0);
    vstate_t *s = vs(t);
    id v = s->data_source && dresp(s->data_source, "tableView:objectValueForTableColumn:row:") ? MSG(id, s->data_source, "tableView:objectValueForTableColumn:row:", t, col, r) : 0;
    MSG(void, cell, "setObjectValue:", v);
    if (dresp(cell, "setBackgroundStyle:")) MSG(void, cell, "setBackgroundStyle:", (i64)(sel ? 1 : 0));
    if (dresp(s->delegate, "tableView:willDisplayCell:forTableColumn:row:")) MSG(void, s->delegate, "tableView:willDisplayCell:forTableColumn:row:", t, cell, col, r);
    absr_t a;
    if (!abs_rect(t, &a)) return 1;
    NSRect fr = r_make(x - a.tlx, y - a.tly, w, h);   /* no sistema da tabela (invertida) */
    ((void (*)(id, SEL, NSRect, id))objc_msgSend)(cell, sel_registerName("drawWithFrame:inView:"), fr, t);
    return 1;
}
static void tv_draw_drop_line(id t, i64 r, double x0, double ry, double w, double h) {
    i64 dr = tvp_i(t, "dropRow", -2);
    if (dr == -2) return;
    u64 op = (u64)tvp_i(t, "dropOp", 1);
    if (op == 0 && dr == r) g_rstroke(x0 + 2, ry + 1, w - 4, h - 2, 5, 2, accent);
    else if (op == 1 && dr == r) g_fill(x0 + 4, ry - 1, w - 8, 2, accent);
    else if (op == 1 && dr == r + 1 && r + 1 == tv_rows(t)) g_fill(x0 + 4, ry + h - 1, w - 8, 2, accent);
}
static void tv_draw_sort_indicator(id t, id col, double x, double hy, double w) {
    id img = tv2_indicator(t, 0, col);
    id descs = tv2_sort(t, 0);
    id first = MSG0(u64, descs, "count") ? MSG(id, descs, "objectAtIndex:", (u64)0) : 0;
    id proto = colp_o(col, "sortProto");
    int show = img || (first && proto && MSG(BOOL, MSG0(id, first, "key"), "isEqual:", MSG0(id, proto, "key")));
    if (!show) return;
    BOOL asc = first ? MSG0(BOOL, first, "ascending") : YES;
    double cx = x + w - 14, cy = hy + 12;
    float tri[6];
    if (asc) { tri[0] = (float)(cx - 4); tri[1] = (float)(cy + 2); tri[2] = (float)(cx + 4); tri[3] = (float)(cy + 2); tri[4] = (float)cx; tri[5] = (float)(cy - 3); }
    else { tri[0] = (float)(cx - 4); tri[1] = (float)(cy - 2); tri[2] = (float)(cx + 4); tri[3] = (float)(cy - 2); tri[4] = (float)cx; tri[5] = (float)(cy + 3); }
    g_poly(tri, 3, 0xB0000000, 1, 0);
}
static int tv_cell_pos(id self, id v, int *row, int *col) {
    cx_tv_engine_t *e = cx_tv_engine_get(self, 0);
    *row = -1; *col = -1;
    for (int i = 0; v && v != self && i < 64; i++, v = vs(v)->superview)
        if (cx_tv_engine_placed_find(e, v, row, col) >= 0) return 1;
    return 0;
}
static i64 tv_row_for_view(id self, SEL c, id v) { (void)c; int r, k; return tv_cell_pos(self, v, &r, &k) ? r : -1; }
static i64 tv_col_for_view(id self, SEL c, id v) { (void)c; int r, k; return tv_cell_pos(self, v, &r, &k) ? k : -1; }
static id tv_view_at(id self, SEL c, i64 col, i64 row, BOOL make) {
    (void)c;
    vstate_t *s = vs(self);
    if (row < 0 || row >= tv_rows(self)) return 0;
    if (make) tv_layout_views(self);
    id v = cx_tv_engine_placed_view(cx_tv_engine_get(self, 0), (int)row, (int)col);
    if (!v) v = cx_tv_engine_placed_view(cx_tv_engine_get(self, 0), (int)row, -1);
    id vprov = s->delegate && cx_responds(s->delegate->isa, sel_registerName("tableView:viewForTableColumn:row:")) ? s->delegate :
               s->data_source && cx_responds(s->data_source->isa, sel_registerName("tableView:viewForTableColumn:row:")) ? s->data_source : 0;
    if (!v && make && vprov) {
        nsarr_t *cols = (nsarr_t *)s->cols;
        id cl = cols && col >= 0 && (u64)col < cols->n ? cols->items[col] : 0;
        v = MSG(id, vprov, "tableView:viewForTableColumn:row:", self, cl, row);   /* linha fora da área visível */
    }
    return v;
}
static i64 tv_num_cols(id self, SEL c) { (void)c; nsarr_t *cols = (nsarr_t *)vs(self)->cols; return cols ? (i64)cols->n : 0; }
static i64 tv_col_with_ident(id self, SEL c, id ident) {
    (void)c;
    nsarr_t *cols = (nsarr_t *)vs(self)->cols;
    for (u64 k = 0; cols && ident && k < cols->n; k++) {
        id ci = cs_(cols->items[k])->ident;
        if (ci == ident || (ci && MSG(BOOL, ci, "isEqual:", ident))) return (i64)k;
    }
    return -1;
}
static id tv_col_named(id self, SEL c, id ident) {
    i64 k = tv_col_with_ident(self, c, ident);
    return k >= 0 ? ((nsarr_t *)vs(self)->cols)->items[k] : 0;
}
static id tv_sel_indexes(id self, SEL c) {
    (void)c;
    cx_class_t *k = cx_class_named("NSMutableIndexSet");
    id set = k ? MSG0(id, (id)k, "indexSet") : 0;
    if (set && vs(self)->selected >= 0) MSG(void, set, "addIndex:", (u64)vs(self)->selected);
    return set;
}
static void tv_remove_col(id self, SEL c, id col) {
    (void)c;
    vstate_t *s = vs(self);
    if (col && s->cols) MSG(void, s->cols, "removeObject:", col);
    s->tv_dirty = 1;
    mark(self);
}
static void tv_note_rows(id self, SEL c) { tv_reload(self, c); }
static void tv_reload_rows(id self, SEL c, id rows, id cols) { (void)rows; (void)cols; tv_reload(self, c); }
/* editColumn:row:withEvent:select: — o campo de texto da célula vira o primeiro respondedor */
static void tv_edit(id self, SEL c, i64 col, i64 row, id ev, BOOL sel) {
    (void)ev; (void)sel;
    id v = tv_view_at(self, c, col, row, YES);
    if (!v) return;
    id tf = cx_responds(v->isa, sel_registerName("textField")) ? MSG0(id, v, "textField") : v;
    id w = vs(self)->window;
    if (tf && w) MSG(void, w, "makeFirstResponder:", tf);
}
/* NSTableCellView: guarda textField/imageView */
static void tcv_set_tf(id self, SEL c, id tf) { (void)c; vstate_t *s = vs(self); keep(&s->text_field, tf); if (tf) v_add_sub(self, c, tf); }
static id tcv_tf(id self, SEL c) { (void)c; return vs(self)->text_field; }

/* ================================================================ NSScreen, NSCursor, NSWorkspace, NSImage */
static id sc_main(id cls, SEL c) { (void)c; static id s; if (!s) s = cx_alloc_instance((cx_class_t *)cls); return s; }
static id sc_screens(id cls, SEL c) { id a = cx_arr_new(&NSMutableArray_c); cx_arr_add(a, sc_main(cls, c)); return a; }
static NSRect sc_frame(id self, SEL c) { (void)self; (void)c; gui_wininfo_t i = {1440, 876, 1, 1}; SYS1(GUI_SCREEN, &i); return r_make(0, 0, i.w, i.h + 24); }
/* sem a faixa do Dock (o kernel diz quanto ele reserva em .visible; os antigos devolviam 1): o Electron usa isto como
 * workArea, e o VS Code abria a janela até atrás do Dock, escondendo a barra de status */
static NSRect sc_visible(id self, SEL c) {
    (void)self; (void)c;
    gui_wininfo_t i = {1440, 876, 1, 1};
    SYS1(GUI_SCREEN, &i);
    int dock = i.visible > 1 && i.visible < i.h / 2 ? i.visible : 70;
    return r_make(0, dock, i.w, i.h - dock);
}
static double sc_scale(id self, SEL c) { (void)self; (void)c; return 1.0; }
static id generic_singleton(id cls, SEL c) { (void)c; static struct { id c, o; } t[8]; for (int i = 0; i < 8; i++) { if (t[i].c == cls) return t[i].o; if (!t[i].c) { t[i].c = cls; t[i].o = cx_alloc_instance((cx_class_t *)cls); return t[i].o; } } return 0; }
static BOOL ws_open_url(id self, SEL c, id u) { (void)self; (void)c; (void)u; return NO; }
static id key_window_obj(void) { return ap_key_window(NSApp, 0); }

/* ================================================================ registro */
#define M(cls, s, f) cx_add_method(&cls##_c, s, (void *)(f))
#define CM(cls, s, f) cx_add_cmethod(&cls##_c, s, (void *)(f))
#define DEF(cls, sup) cx_define_class(&cls##_c, &cls##_m, #cls, &sup##_c, 8)
int OBJC_IVAR_NSApplication__appFlags __asm__("OBJC_IVAR_$_NSApplication._appFlags") = 24;   /* recalculado no init */
u32 cx_instance_size(cx_class_t *c);

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
/* para o UIKit do mOS (userland/frameworks/UIKit): âncoras de qualquer objeto (UIView) e as restrições ativas, que o
 * UIKit resolve para as suas views (este resolvedor só cuida de NSView) */
id cx_layout_anchor(id v, int attr) { return anchor_for(v, attr); }
u32 cx_layout_constraints(id **out) { if (out) *out = lcs; return nlc; }
int cx_layout_constraint_info(id c, id *a, int *aa, int *rel, id *b, int *ba, double *mult, double *k, float *prio) {
    lc_t *s = c ? (lc_t *)ST(c) : 0;
    if (!s) return 0;
    *a = s->a; *aa = s->aa; *rel = s->rel; *b = s->b; *ba = s->ba; *mult = s->mult; *k = s->k; *prio = s->prio;
    return 1;
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
static float pri_get(float v, float def) { return v == 0 ? def : v; }
static void v_set_hug(id self, SEL c, float p, i64 o) {
    (void)c;
    if (p == 0) p = 0.001f;
    if (o == 1) vs(self)->hug_v = p; else vs(self)->hug_h = p;
    layout_dirty = YES;
}
static float v_hug(id self, SEL c, i64 o) { (void)c; vstate_t *s = vs(self); return pri_get(o == 1 ? s->hug_v : s->hug_h, 250); }
static void v_set_cr(id self, SEL c, float p, i64 o) {
    (void)c;
    if (p == 0) p = 0.001f;
    if (o == 1) vs(self)->cr_v = p; else vs(self)->cr_h = p;
    layout_dirty = YES;
}
static float v_cr(id self, SEL c, i64 o) { (void)c; vstate_t *s = vs(self); return pri_get(o == 1 ? s->cr_v : s->cr_h, 750); }
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
        float hugp = MSG(float, best->v, "contentHuggingPriorityForOrientation:", (i64)(best_h ? 0 : 1));
        set[3] = 1;
        /* hugging alto fica no tamanho intrínseco; hugging baixo preserva o frame se ele já for maior */
        sv[3] = iz > 0 ? iz : fz;
        if (hugp < 500 && fz > sv[3]) sv[3] = fz;
        if (hugp >= 750 && iz > 0) sv[3] = iz;
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
            {
                NSSize z = MSG0(NSSize, e->v, "intrinsicContentSize");
                double iz = h ? z.w : z.h;
                float cr = MSG(float, e->v, "contentCompressionResistancePriorityForOrientation:", (i64)(h ? 0 : 1));
                if (cr >= 750 && iz > 0 && *size < iz && s->prio < cr) *size = iz;
            }
        }
}
static int lv_depth_cmp(const void *a, const void *b) { return ((const lv_t *)a)->depth - ((const lv_t *)b)->depth; }
static void autolayout_window(id w) {
    if (ws(w)->content) v_update_constraints_subtree(ws(w)->content, 0);   /* updateConstraints antes do layout */
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

static int tv_nibarchive(const void *b, u64 n) {
    static const char mag[] = "NIBArchive";
    if (!b || n < 10) return 0;
    const char *p = b;
    for (int i = 0; i < 10; i++) if (p[i] != mag[i]) return 0;
    return 1;
}
static id tv_pick_view(id tops, id ident) {
    if (!tops || !cx_responds(tops->isa, sel_registerName("count"))) return 0;
    u64 n = MSG0(u64, tops, "count");
    id first = 0;
    const char *want = tv_ident_cstr(ident);
    for (u64 i = 0; i < n; i++) {
        id o = MSG(id, tops, "objectAtIndex:", i);
        if (!o || !cx_is_subclass(o->isa, &NSView_c)) continue;
        if (!first) first = o;
        if (want && tv_ident_cstr(vs(o)->identifier) && !strcmp(tv_ident_cstr(vs(o)->identifier), want)) return o;
    }
    return first;
}
static id tv_materialize(id self, id ident, id owner, void *token, int kind) {
    id tok = (id)token;
    if (!tok) return 0;
    if (!owner) owner = self;
    if (kind == CX_TV_PROTO) return tv_clone_view(tok);
    if (kind == CX_TV_NIB) {
        id tops = 0;
        BOOL (*inst)(id, SEL, id, id *) = (BOOL (*)(id, SEL, id, id *))objc_msgSend;
        if (!inst(tok, sel_registerName("instantiateWithOwner:topLevelObjects:"), owner, &tops)) return 0;
        id v = tv_pick_view(tops, ident);
        return v ? objc_retain(v) : 0;
    }
    if (kind == CX_TV_NIBDATA && tv_is_data(tok)) {
        const void *b = MSG0(const void *, tok, "bytes");
        u64 n = MSG0(u64, tok, "length");
        if (!tv_nibarchive(b, n)) return 0;
        id tops = 0;
        if (!nib_load_buf((u8 *)b, n, owner, 0, "", &tops)) return 0;
        id v = tv_pick_view(tops, ident);
        return v ? objc_retain(v) : 0;
    }
    return 0;
}

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

/* ================================================================ desenho fora da janela
 * Grava o lote de desenho (os mesmos comandos GC_* que iriam à janela) para o appkit_offscreen.c reproduzir num
 * CGContext: NSImage lockFocus, cacheDisplayInRect:, dataWithPDFInsideRect:, impressão. Aninha (lockFocus dentro de
 * um drawRect:). Enquanto grava, nada vai à janela e o gancho do QuartzCore fica desligado. */
typedef struct {
    u8 *cmd; u32 ncmd, capcmd;
    ctx_t ctx;
    double cx0, cy0, cx1, cy1;
    i32 lclip[4]; BOOL lclip_on;
    void (*hook)(id, int, double, double, double, double, double, double, double, double);
} gcsave_t;
void *cx_gc_record_begin(double w, double h, int flipped) {
    gcsave_t *sv = calloc(1, sizeof *sv);
    if (!sv) return 0;
    sv->cmd = cmd; sv->ncmd = ncmd; sv->capcmd = capcmd;
    sv->ctx = ctx;
    sv->cx0 = clip_x0; sv->cy0 = clip_y0; sv->cx1 = clip_x1; sv->cy1 = clip_y1;
    memcpy(sv->lclip, last_clip, sizeof last_clip); sv->lclip_on = last_clip_on;
    sv->hook = cx_layer_draw_hook;
    cmd = 0; ncmd = 0; capcmd = 0;
    cx_layer_draw_hook = 0;
    g_offscreen++;
    clip_x0 = 0; clip_y0 = 0; clip_x1 = w; clip_y1 = h;
    last_clip_on = NO;
    ctx.tlx = 0; ctx.tly = 0; ctx.w = w; ctx.h = h; ctx.flipped = flipped ? YES : NO;
    ctx.fill = ctx.stroke = 0xFF000000; ctx.line_width = 1; ctx.galpha = 1; ctx.blend = 0;
    return sv;
}
const u8 *cx_gc_record_buffer(u32 *n) { if (n) *n = ncmd; return cmd; }
void cx_gc_record_end(void *p) {
    gcsave_t *sv = p;
    if (!sv) return;
    free(cmd);
    cmd = sv->cmd; ncmd = sv->ncmd; capcmd = sv->capcmd;
    ctx = sv->ctx;
    clip_x0 = sv->cx0; clip_y0 = sv->cy0; clip_x1 = sv->cx1; clip_y1 = sv->cy1;
    memcpy(last_clip, sv->lclip, sizeof last_clip); last_clip_on = sv->lclip_on;
    cx_layer_draw_hook = sv->hook;
    if (g_offscreen > 0) g_offscreen--;
    free(sv);
}
/* desenha a view e as subviews no lote que está sendo gravado, com o retângulo r (coordenadas da view) em 0,0 */
void cx_gc_record_view(id v, double rx, double ry, double rw, double rh) {
    vstate_t *s = vs(v);
    absr_t a;
    a.w = s->frame.s.w; a.h = s->frame.s.h;
    a.flipped = is_flipped(v);
    a.tlx = -rx;
    a.tly = a.flipped ? -ry : -(a.h - (ry + rh));
    BOOL hid = s->hidden;
    s->hidden = NO;   /* -cacheDisplayInRect: desenha a view mesmo fora da janela */
    draw_view_tree(v, &a);
    s->hidden = hid;
    (void)rw;
}

/* ---- NSView: updateConstraints de baixo para cima nas views marcadas (setNeedsUpdateConstraints:), antes de cada
 * passada do Auto Layout, como no Cocoa */
static char k_needs_uc;
static void v_set_needs_uc(id self, SEL c, BOOL b) { (void)c; objc_setAssociatedObject(self, &k_needs_uc, b ? cx_num_i(1) : 0, 1); if (b) mark(self); }
static BOOL v_needs_uc(id self, SEL c) { (void)c; return objc_getAssociatedObject(self, &k_needs_uc) != 0; }
static void v_update_constraints_subtree(id self, SEL c) {
    (void)c;
    if (!self || !cx_is_subclass(self->isa, &NSView_c)) return;
    vstate_t *s = vs(self);
    for (u32 i = 0; i < s->nsub; i++) v_update_constraints_subtree(s->sub[i], 0);
    if (objc_getAssociatedObject(self, &k_needs_uc)) {
        objc_setAssociatedObject(self, &k_needs_uc, 0, 1);
        MSG0(void, self, "updateConstraints");
    }
}
static void v_update_constraints(id self, SEL c) { (void)c; objc_setAssociatedObject(self, &k_needs_uc, 0, 1); }   /* a base só limpa a marca */
/* ---- NSTableView: a última coluna ocupa o que sobra da largura visível */
static void tv_size_last(id self, SEL c) {
    (void)c;
    id cols = MSG0(id, self, "tableColumns");
    u64 n = cols ? MSG0(u64, cols, "count") : 0;
    if (!n) return;
    vstate_t *s = vs(self);
    double avail = s->superview && vs(s->superview)->kind == K_SCROLL ? vs(s->superview)->frame.s.w : s->frame.s.w;
    NSSize sp = cx_responds(self->isa, sel_registerName("intercellSpacing")) ? ((NSSize (*)(id, SEL))objc_msgSend)(self, sel_registerName("intercellSpacing")) : (NSSize){3, 2};
    double used = 0;
    for (u64 i = 0; i + 1 < n; i++) used += MSG0(double, MSG(id, cols, "objectAtIndex:", i), "width") + sp.w;
    id last = MSG(id, cols, "objectAtIndex:", n - 1);
    double w = avail - used - sp.w;
    double mn = cx_responds(last->isa, sel_registerName("minWidth")) ? MSG0(double, last, "minWidth") : 10;
    double mx = cx_responds(last->isa, sel_registerName("maxWidth")) ? MSG0(double, last, "maxWidth") : 1e6;
    if (w < mn) w = mn;
    if (w > mx) w = mx;
    MSG(void, last, "setWidth:", w);
    mark(self);
}
static char k_tv_style;
static void tv_set_style(id self, SEL c, i64 st) { (void)c; objc_setAssociatedObject(self, &k_tv_style, cx_num_i(st), 1); mark(self); }
static i64 tv_style(id self, SEL c) { (void)c; id v = objc_getAssociatedObject(self, &k_tv_style); return v ? MSG0(i64, v, "longLongValue") : 0; }
/* NSTableViewStyleAutomatic (0) vira Inset (2) em janelas com toolbar unificada e FullWidth (1) nas outras; SourceList
 * é a da barra lateral (NSTableViewSelectionHighlightStyleSourceList) */
static i64 tv_effective_style(id self, SEL c) {
    i64 st = tv_style(self, c);
    if (st) return st;
    if (cx_responds(self->isa, sel_registerName("selectionHighlightStyle")) && MSG0(i64, self, "selectionHighlightStyle") == 1) return 3;
    return 1;
}
/* ---- NSEvent: rolagem natural (preferência com.apple.swipescrolldirection do domínio global; ligada por padrão) */
static BOOL ev_direction_inverted(id self, SEL c) {
    (void)self; (void)c;
    id ud = MSG0(id, (id)cx_class_named("NSUserDefaults"), "standardUserDefaults");
    id v = ud ? MSG(id, ud, "objectForKey:", cx_str("com.apple.swipescrolldirection")) : 0;
    return v ? MSG0(BOOL, v, "boolValue") : YES;
}

/* ================================================================ cadeia de teclado (key view loop)
 * nextKeyView/previousKeyView explícitos (fracos, como no Cocoa); sem o seguinte explícito, a ordem automática é a do
 * recalculateKeyViewLoop: de cima para baixo e da esquerda para a direita, entre as views que podem ser chave.
 * Tab e ⇧Tab andam pela cadeia (NSTextField, NSTextView de campo e qualquer view que deixe o keyDown: subir). */
static char k_next_kv, k_prev_kv, k_auto_kv;
static BOOL v_hidden_or_ancestor(id v) {
    for (id cur = v; cur && cx_is_subclass(cur->isa, &NSView_c); cur = vs(cur)->superview) if (vs(cur)->hidden) return YES;
    return NO;
}
static BOOL v_can_become_key_view(id self, SEL c) {
    (void)c;
    if (!cx_is_subclass(self->isa, &NSView_c)) return NO;
    vstate_t *s = vs(self);
    if (!s->window || s->frame.s.w <= 0 || s->frame.s.h <= 0 || v_hidden_or_ancestor(self)) return NO;
    return MSG0(BOOL, self, "acceptsFirstResponder");
}
static void v_set_next_kv(id self, SEL c, id n) {
    (void)c;
    id old = objc_getAssociatedObject(self, &k_next_kv);
    if (old && objc_getAssociatedObject(old, &k_prev_kv) == self) objc_setAssociatedObject(old, &k_prev_kv, 0, 0);
    objc_setAssociatedObject(self, &k_next_kv, n, 0);
    if (n) objc_setAssociatedObject(n, &k_prev_kv, self, 0);
}
static id v_next_kv(id self, SEL c) { (void)c; return objc_getAssociatedObject(self, &k_next_kv); }
static id v_prev_kv(id self, SEL c) { (void)c; return objc_getAssociatedObject(self, &k_prev_kv); }
/* ordem automática: todas as views da janela que podem ser chave, por posição na janela */
typedef struct { id v; double top, left; } kv_ent;
static void kv_collect(id v, kv_ent *out, int *n, int cap) {
    if (!v || *n >= cap || vs(v)->hidden) return;
    if (MSG0(BOOL, v, "canBecomeKeyView")) {
        NSRect r = ((NSRect (*)(id, SEL, NSRect, id))objc_msgSend)(v, sel_registerName("convertRect:toView:"), vs(v)->frame.s.w > 0 ? r_make(0, 0, vs(v)->frame.s.w, vs(v)->frame.s.h) : r_make(0, 0, 0, 0), (id)0);
        out[*n].v = v; out[*n].top = r.o.y + r.s.h; out[*n].left = r.o.x;
        (*n)++;
    }
    vstate_t *s = vs(v);
    for (u32 i = 0; i < s->nsub && *n < cap; i++) kv_collect(s->sub[i], out, n, cap);
}
static int kv_auto(id w, kv_ent *out, int cap) {
    int n = 0;
    kv_collect(ws(w)->content, out, &n, cap);
    for (int i = 1; i < n; i++) {   /* inserção: de cima (top maior) para baixo, depois da esquerda */
        kv_ent e = out[i];
        int j = i - 1;
        while (j >= 0 && (out[j].top < e.top - 2 || (out[j].top <= e.top + 2 && out[j].top >= e.top - 2 && out[j].left > e.left))) { out[j + 1] = out[j]; j--; }
        out[j + 1] = e;
    }
    return n;
}
static id kv_step(id w, id from, int dir) {
    if (!w) return 0;
    /* cadeia explícita primeiro */
    if (from) {
        id cur = from;
        for (int guard = 0; guard < 256; guard++) {
            cur = objc_getAssociatedObject(cur, dir > 0 ? &k_next_kv : &k_prev_kv);
            if (!cur || cur == from) break;
            if (MSG0(BOOL, cur, "canBecomeKeyView")) return cur;
        }
        if (cur == from && cur) return 0;   /* laço explícito sem ninguém válido */
    }
    kv_ent e[512];
    int n = kv_auto(w, e, 512);
    if (!n) return 0;
    int at = -1;
    for (int i = 0; from && i < n; i++) if (e[i].v == from) { at = i; break; }
    if (at < 0) return e[dir > 0 ? 0 : n - 1].v;
    return e[(at + (dir > 0 ? 1 : n - 1)) % n].v;
}
static id v_next_valid_kv(id self, SEL c) { (void)c; return kv_step(vs(self)->window, self, 1); }
static id v_prev_valid_kv(id self, SEL c) { (void)c; return kv_step(vs(self)->window, self, -1); }
static void kv_select(id w, id v) {
    if (!v || !w_make_first(w, 0, v)) return;
    vstate_t *s = vs(v);
    if (s->kind == K_TEXT && s->editable) {   /* entrar num campo pelo Tab seleciona o texto todo */
        s->anchor = 0; s->has_anchor = YES; s->caret = s->value ? (int)strlen(cx_cstr(s->value)) : 0;
        mark(v);
    } else if (cx_responds(v->isa, sel_registerName("selectAll:")) && s->kind == K_TV && cx_responds(v->isa, sel_registerName("isFieldEditor")) && MSG0(BOOL, v, "isFieldEditor"))
        MSG(void, v, "selectAll:", (id)0);
}
static void w_select_following(id self, SEL c, id v) { (void)c; kv_select(self, kv_step(self, v, 1)); }
static void w_select_preceding(id self, SEL c, id v) { (void)c; kv_select(self, kv_step(self, v, -1)); }
static id kv_origin(id w) { id f = ws(w)->first; return f && f != w && cx_is_subclass(f->isa, &NSView_c) ? f : 0; }
static void w_select_next_kv(id self, SEL c, id sender) { (void)c; (void)sender; kv_select(self, kv_step(self, kv_origin(self), 1)); }
static void w_select_prev_kv(id self, SEL c, id sender) { (void)c; (void)sender; kv_select(self, kv_step(self, kv_origin(self), -1)); }
static void w_recalc_kv(id self, SEL c) {
    (void)c;
    kv_ent e[512];
    int n = kv_auto(self, e, 512);
    for (int i = 0; i < n; i++) v_set_next_kv(e[i].v, 0, e[(i + 1) % n].v);
}
static void w_set_auto_kv(id self, SEL c, BOOL v) { (void)c; objc_setAssociatedObject(self, &k_auto_kv, v ? cx_num_i(1) : 0, 1); }
static BOOL w_auto_kv(id self, SEL c) { (void)c; return objc_getAssociatedObject(self, &k_auto_kv) != 0; }
/* Tab/⇧Tab que ninguém tratou: a janela anda pela cadeia */
static BOOL kv_tab_key(id view, id ev) {
    evstate_t *e = evs(ev);
    if (e->keycode != 48 || (e->mods & ((1u << 20) | (1u << 18)))) return NO;   /* sem ⌘ nem ⌃ */
    id w = cx_is_subclass(view->isa, &NSView_c) ? vs(view)->window : 0;
    if (!w) return NO;
    if (e->mods & (1u << 17)) w_select_prev_kv(w, 0, 0); else w_select_next_kv(w, 0, 0);
    return YES;
}

/* ================================================================ NSWindow: chave/principal, título, autosave do frame
 * e as propriedades guardadas. A janela principal é a chave quando ela pode ser principal (NSPanel não pode); senão a
 * última janela visível que pode. O frame salvo segue o formato do Cocoa em NSUserDefaults ("NSWindow Frame <nome>":
 * x y w h da janela e x y w h da área visível da tela) e é gravado ao fechar, esconder, redimensionar e sair. */
static BOOL w_is_key(id self, SEL c) { (void)c; return g_key_window ? g_key_window == self : ap_key_window(NSApp, 0) == self; }
static BOOL w_can_main(id self, SEL c) {
    (void)c;
    if (cx_is_subclass(self->isa, &NSPanel_c)) return NO;
    return (ws(self)->style & (1 | 8)) != 0;   /* com barra de título ou redimensionável */
}
static BOOL can_be_main(id w) { return w && !ws(w)->closed && MSG0(BOOL, w, "canBecomeMainWindow"); }
static id ap_main_window(id self, SEL c) {
    (void)self; (void)c;
    if (g_key_window && can_be_main(g_key_window)) return g_key_window;
    nsarr_t *a = (nsarr_t *)app_windows;
    for (u64 i = a ? a->n : 0; i-- > 0;) if (ws(a->items[i])->visible && can_be_main(a->items[i])) return a->items[i];
    return 0;
}
static BOOL w_is_main(id self, SEL c) { (void)c; return ap_main_window(NSApp, 0) == self; }
static id w_repr_filename(id self, SEL c) {
    (void)c;
    id u = ws(self)->represented_url, p = u && cx_responds(u->isa, sel_registerName("path")) ? MSG0(id, u, "path") : 0;
    return p ? p : cx_str("");
}
static void w_set_repr_filename(id self, SEL c, id path) {
    (void)c;
    id u = path && MSG0(u64, path, "length") ? MSG(id, (id)cx_class_named("NSURL"), "fileURLWithPath:", path) : 0;
    MSG(void, self, "setRepresentedURL:", u);
}
static void w_set_title_vis(id self, SEL c, i64 v) {
    (void)c;
    wstate_t *s = ws(self);
    s->title_hidden = v == 1;   /* NSWindowTitleHidden */
    if (s->gid > 0) SYS2(GUI_WIN_TITLE, s->gid, w_shown_title(self));
}
static i64 w_title_vis(id self, SEL c) { (void)c; return ws(self)->title_hidden ? 1 : 0; }
#define WBOOL(get, set, field, inv) \
    static void set(id self, SEL c, BOOL v) { (void)c; ws(self)->field = (v ? YES : NO) ^ (inv); } \
    static BOOL get(id self, SEL c) { (void)c; return ws(self)->field ^ (inv); }
WBOOL(w_titlebar_transparent, w_set_titlebar_transparent, titlebar_transparent, 0)
WBOOL(w_movable_bg, w_set_movable_bg, movable_bg, 0)
WBOOL(w_accepts_moved, w_set_accepts_moved, accepts_moved, 0)
WBOOL(w_has_shadow, w_set_has_shadow, no_shadow, 1)
WBOOL(w_is_opaque, w_set_opaque, not_opaque, 1)
#undef WBOOL
static void w_set_collection(id self, SEL c, u64 v) { (void)c; ws(self)->collection = v; }
static u64 w_collection(id self, SEL c) { (void)c; return ws(self)->collection; }
static void w_set_initial_fr(id self, SEL c, id v) { (void)c; ws(self)->initial_fr = v; }
static id w_initial_fr(id self, SEL c) { (void)c; return ws(self)->initial_fr; }
static void w_layout_if_needed(id self, SEL c) {
    (void)c;
    id cv = ws(self)->content;
    if (cv && cx_responds(cv->isa, sel_registerName("layoutSubtreeIfNeeded"))) MSG0(void, cv, "layoutSubtreeIfNeeded");
}
/* NSView: o clique pode arrastar a janela (movableByWindowBackground) num fundo transparente; controles e texto não */
static BOOL v_can_move_window(id self, SEL c) {
    (void)c;
    int k = vs(self)->kind;
    if (k == K_BUTTON || k == K_TEXT || k == K_SLIDER || k == K_TV || k == K_TABLE || k == K_SCROLL || k == K_PROGRESS) return NO;
    if (cx_is_subclass(self->isa, &NSControl_c)) return NO;
    return cx_responds(self->isa, sel_registerName("isOpaque")) ? !MSG0(BOOL, self, "isOpaque") : YES;
}
static BOOL ctl_can_move_window(id self, SEL c) { (void)self; (void)c; return NO; }
/* touchBar do NSResponder: guardada; sem uma, pergunta a makeTouchBar uma vez (não há Touch Bar no hardware do mOS,
 * mas os apps montam e consultam a barra) */
static char k_touch_bar;
static id r_touch_bar(id self, SEL c) {
    (void)c;
    id b = objc_getAssociatedObject(self, &k_touch_bar);
    if (!b && cx_responds(self->isa, sel_registerName("makeTouchBar"))) {
        b = MSG0(id, self, "makeTouchBar");
        if (b) objc_setAssociatedObject(self, &k_touch_bar, b, 1);
    }
    return b;
}
static void r_set_touch_bar(id self, SEL c, id b) { (void)c; objc_setAssociatedObject(self, &k_touch_bar, b, 1); }

static id ud_std(void) { return MSG0(id, (id)cx_class_named("NSUserDefaults"), "standardUserDefaults"); }
static id frame_key(id name) {
    char k[320];
    snprintf(k, sizeof k, "NSWindow Frame %s", cx_cstr(name));
    return cx_str(k);
}
static NSRect screen_visible(void) {
    id sc = MSG0(id, (id)cx_class_named("NSScreen"), "mainScreen");
    return sc ? ((NSRect (*)(id, SEL))objc_msgSend)(sc, sel_registerName("visibleFrame")) : r_make(0, 0, 1440, 876);
}
static id w_saved_frame_string(id self, SEL c) {
    (void)c;
    NSRect f = ((NSRect (*)(id, SEL))objc_msgSend)(self, sel_registerName("frame")), v = screen_visible();
    char b[200];
    snprintf(b, sizeof b, "%d %d %d %d %d %d %d %d ", (int)f.o.x, (int)f.o.y, (int)f.s.w, (int)f.s.h, (int)v.o.x, (int)v.o.y,
             (int)v.s.w, (int)v.s.h);
    return cx_str(b);
}
static BOOL frame_from_string(id self, id str, BOOL force) {
    if (!str || !cx_is_str(str)) return NO;
    const char *p = cx_cstr(str);
    double a[8];
    int n = 0;
    while (n < 8 && *p) {
        while (*p == ' ') p++;
        if (!*p) break;
        char *e;
        long x = strtol(p, &e, 10);
        if (e == p) return NO;
        a[n++] = (double)x;
        p = e;
    }
    if (n < 4 || a[2] <= 0 || a[3] <= 0) return NO;
    NSRect cur = ((NSRect (*)(id, SEL))objc_msgSend)(self, sel_registerName("frame"));
    NSRect f = r_make(a[0], a[1], a[2], a[3]), v = screen_visible();
    if (!force && !(ws(self)->style & 8)) f.s = cur.s;   /* sem redimensionar: só a posição */
    /* a tela pode ter mudado: o topo fica abaixo da barra de menus e pelo menos 40 pontos à vista */
    if (f.o.y + f.s.h > v.o.y + v.s.h) f.o.y = v.o.y + v.s.h - f.s.h;
    if (f.o.y + f.s.h < v.o.y + 40) f.o.y = v.o.y + 40 - f.s.h;
    if (f.o.x > v.o.x + v.s.w - 40) f.o.x = v.o.x + v.s.w - 40;
    if (f.o.x + f.s.w < v.o.x + 40) f.o.x = v.o.x + 40 - f.s.w;
    ((void (*)(id, SEL, NSRect, BOOL))objc_msgSend)(self, sel_registerName("setFrame:display:"), f, YES);
    return YES;
}
static void w_set_frame_from_string(id self, SEL c, id str) { (void)c; frame_from_string(self, str, YES); }
static void w_save_frame_named(id self, SEL c, id name) {
    (void)c;
    if (!name || !cx_is_str(name) || !MSG0(u64, name, "length")) return;
    MSG(void, ud_std(), "setObject:forKey:", w_saved_frame_string(self, 0), frame_key(name));
}
static BOOL w_frame_named_force(id self, SEL c, id name, BOOL force) {
    (void)c;
    if (!name || !cx_is_str(name)) return NO;
    return frame_from_string(self, MSG(id, ud_std(), "stringForKey:", frame_key(name)), force);
}
static BOOL w_frame_named(id self, SEL c, id name) { return w_frame_named_force(self, c, name, NO); }
static void w_remove_frame_named(id cls, SEL c, id name) {
    (void)cls; (void)c;
    if (name && cx_is_str(name)) MSG(void, ud_std(), "removeObjectForKey:", frame_key(name));
}
static BOOL w_set_autosave(id self, SEL c, id name) {
    (void)c;
    wstate_t *s = ws(self);
    BOOL empty = !name || !cx_is_str(name) || !MSG0(u64, name, "length");
    if (!empty) {   /* o nome é único entre as janelas abertas do app */
        nsarr_t *a = (nsarr_t *)app_windows;
        for (u64 i = 0; a && i < a->n; i++) {
            wstate_t *o = ws(a->items[i]);
            if (a->items[i] != self && !o->closed && o->autosave && MSG(BOOL, o->autosave, "isEqualToString:", name)) return NO;
        }
    }
    id nn = empty ? 0 : MSG0(id, name, "copy");
    objc_release(s->autosave);
    s->autosave = nn;
    if (nn && !w_frame_named_force(self, 0, nn, NO)) w_save_frame_named(self, 0, nn);   /* sem frame salvo: grava o atual */
    return YES;
}
static id w_autosave_name(id self, SEL c) { (void)c; id n = ws(self)->autosave; return n ? n : cx_str(""); }
static void w_autosave(id self) {
    wstate_t *s = ws(self);
    if (s->autosave && !s->closed) w_save_frame_named(self, 0, s->autosave);
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
    M(NSResponder, "mouseMoved:", noop1_m); M(NSResponder, "keyUp:", r_key_up); M(NSResponder, "flagsChanged:", r_flags_changed); M(NSResponder, "acceptsFirstResponder", no_m);
    M(NSResponder, "becomeFirstResponder", yes_m); M(NSResponder, "resignFirstResponder", yes_m);

    /* 8 bytes a mais que o DEF comum para o _appFlags (OBJC_IVAR_$_NSApplication._appFlags, depois dos 8 de sempre).
     * Apps antigos leem esse campo direto; o layout dos bits é privado da Apple e o mOS não o reproduz, então ele fica
     * zerado. As subclasses do app deslizam os ivars delas para depois dele (realize_ro em objc.c). */
    cx_define_class(&NSApplication_c, &NSApplication_m, "NSApplication", &NSResponder_c, 16);
    OBJC_IVAR_NSApplication__appFlags = (int)cx_instance_size(&NSResponder_c) + 8;
    CM(NSApplication, "sharedApplication", ap_shared);
    M(NSApplication, "setDelegate:", ap_set_delegate); M(NSApplication, "delegate", ap_delegate); M(NSApplication, "run", ap_run);
    M(NSApplication, "setActivationPolicy:", ap_set_policy); M(NSApplication, "activationPolicy", ap_policy);
    M(NSApplication, "activateIgnoringOtherApps:", ap_activate); M(NSApplication, "activate", ap_activate_simple);
    M(NSApplication, "setMainMenu:", ap_set_menu); M(NSApplication, "mainMenu", ap_menu); M(NSApplication, "terminate:", ap_terminate);
    M(NSApplication, "nextEventMatchingMask:untilDate:inMode:dequeue:", ap_next_event); M(NSApplication, "postEvent:atStart:", ap_post_event);
    M(NSApplication, "discardEventsMatchingMask:beforeEvent:", ap_discard_events); M(NSApplication, "currentEvent", ap_current_event);
    M(NSApplication, "sendEvent:", ap_send_event); M(NSApplication, "windowWithWindowNumber:", ap_window_with_number);
    M(NSApplication, "stop:", ap_stop); M(NSApplication, "finishLaunching", ap_finish_launching); M(NSApplication, "windows", ap_windows);
    M(NSApplication, "orderedWindows", ap_ordered_windows);
    M(NSApplication, "keyWindow", ap_key_window); M(NSApplication, "mainWindow", ap_main_window); M(NSApplication, "sendAction:to:from:", ap_send_action);
    M(NSApplication, "runModalForWindow:", ap_run_modal); M(NSApplication, "stopModalWithCode:", ap_stop_modal_code); M(NSApplication, "stopModal", ap_stop_modal);
    M(NSApplication, "setServicesMenu:", noop1_m);   /* appkit_services.c; menu Janela, ocultar, ícone e atenção: appkit_app_more.c */
    M(NSApplication, "setHelpMenu:", ap_set_help); M(NSApplication, "helpMenu", ap_help);
    M(NSApplication, "orderFrontStandardAboutPanel:", ap_about_panel);

    DEF(NSWindow, NSResponder);
    DEF(NSPanel, NSWindow);
    DEF(NSWindowTab, NSObject);
    M(NSWindowTab, "window", tab_window); M(NSWindowTab, "setWindow:", tab_set_window);
    M(NSWindow, "initWithContentRect:styleMask:backing:defer:", w_init); M(NSWindow, "init", w_init_simple);
    CM(NSWindow, "windowWithContentViewController:", w_with_vc);
    M(NSWindow, "setTitle:", w_set_title); M(NSWindow, "title", w_title); M(NSWindow, "contentView", w_content); M(NSWindow, "setContentView:", w_set_content);
    M(NSWindow, "setSubtitle:", w_set_subtitle); M(NSWindow, "subtitle", w_subtitle);
    M(NSWindow, "setRepresentedURL:", w_set_url); M(NSWindow, "representedURL", w_url);
    M(NSWindow, "setRestorable:", w_set_restorable); M(NSWindow, "isRestorable", w_restorable);
    M(NSWindow, "tab", w_tab); M(NSWindow, "setTab:", w_set_tab);
    M(NSWindow, "isDocumentEdited", no_m); M(NSWindow, "representedFilename", w_repr_filename); M(NSWindow, "setRepresentedFilename:", w_set_repr_filename);
    M(NSResponder, "setTouchBar:", r_set_touch_bar); M(NSResponder, "touchBar", r_touch_bar);
    M(NSWindow, "setDelegate:", w_set_delegate); M(NSWindow, "delegate", w_delegate); M(NSWindow, "alphaValue", w_alpha); M(NSWindow, "setAlphaValue:", w_set_alpha);
    M(NSWindow, "parentWindow", w_parent); M(NSWindow, "setParentWindow:", w_set_parent); M(NSWindow, "addChildWindow:ordered:", w_add_child); M(NSWindow, "removeChildWindow:", w_remove_child); M(NSWindow, "frame", w_frame);
    M(NSWindow, "contentRectForFrameRect:", w_content_rect_for); M(NSWindow, "frameRectForContentRect:", w_frame_rect_for);
    M(NSWindow, "makeKeyAndOrderFront:", w_show); M(NSWindow, "orderFront:", w_show); M(NSWindow, "orderFrontRegardless", w_show); M(NSWindow, "makeKeyWindow", w_show);
    M(NSWindow, "orderWindow:relativeTo:", w_order_window);
    M(NSWindow, "orderOut:", w_order_out); M(NSWindow, "center", w_center); M(NSWindow, "close", w_close); M(NSWindow, "performClose:", w_perform_close);
    M(NSWindow, "setBackgroundColor:", w_set_bg); M(NSWindow, "backgroundColor", w_bg); M(NSWindow, "isVisible", w_visible);
    M(NSWindow, "setReleasedWhenClosed:", w_set_released); M(NSWindow, "firstResponder", w_first); M(NSWindow, "makeFirstResponder:", w_make_first);
     
    M(NSWindow, "setFrame:display:", w_set_frame); M(NSWindow, "setContentSize:", w_set_content_size); M(NSWindow, "display", w_display);
    M(NSWindow, "setFrameAutosaveName:", w_set_autosave); M(NSWindow, "frameAutosaveName", w_autosave_name);
    M(NSWindow, "saveFrameUsingName:", w_save_frame_named); M(NSWindow, "setFrameUsingName:", w_frame_named);
    M(NSWindow, "setFrameUsingName:force:", w_frame_named_force); CM(NSWindow, "removeFrameUsingName:", w_remove_frame_named);
    M(NSWindow, "stringWithSavedFrame", w_saved_frame_string); M(NSWindow, "setFrameFromString:", w_set_frame_from_string);
    M(NSWindow, "setTitlebarAppearsTransparent:", w_set_titlebar_transparent); M(NSWindow, "titlebarAppearsTransparent", w_titlebar_transparent);
    M(NSWindow, "setTitleVisibility:", w_set_title_vis); M(NSWindow, "titleVisibility", w_title_vis);
    M(NSWindow, "setLevel:", w_set_level); M(NSWindow, "level", w_level); M(NSWindow, "setOpaque:", w_set_opaque); M(NSWindow, "isOpaque", w_is_opaque);
    M(NSWindow, "setHasShadow:", w_set_has_shadow); M(NSWindow, "hasShadow", w_has_shadow);
    M(NSWindow, "setMovableByWindowBackground:", w_set_movable_bg); M(NSWindow, "isMovableByWindowBackground", w_movable_bg);
    M(NSWindow, "setAcceptsMouseMovedEvents:", w_set_accepts_moved); M(NSWindow, "acceptsMouseMovedEvents", w_accepts_moved);
    M(NSWindow, "setInitialFirstResponder:", w_set_initial_fr); M(NSWindow, "initialFirstResponder", w_initial_fr);
    M(NSWindow, "setStyleMask:", w_set_style_mask); M(NSWindow, "styleMask", w_style_mask);
    M(NSWindow, "contentLayoutRect", w_content_layout_rect); M(NSWindow, "performWindowDragWithEvent:", w_perform_drag);
    M(NSWindow, "setCollectionBehavior:", w_set_collection); M(NSWindow, "collectionBehavior", w_collection); M(NSWindow, "setAppearance:", noop1_m); M(NSWindow, "setFrameOrigin:", w_set_frame_origin);
    M(NSWindow, "setFrameTopLeftPoint:", w_set_frame_top_left); M(NSWindow, "cascadeTopLeftFromPoint:", w_cascade); M(NSWindow, "windowNumber", w_number);
    M(NSWindow, "nextEventMatchingMask:", w_next_event); M(NSWindow, "nextEventMatchingMask:untilDate:inMode:dequeue:", w_next_event_full);
    M(NSWindow, "postEvent:atStart:", ap_post_event); M(NSWindow, "discardEventsMatchingMask:beforeEvent:", ap_discard_events);
    M(NSWindow, "sendEvent:", w_send_event); M(NSWindow, "currentEvent", ap_current_event); M(NSWindow, "setTabbingMode:", noop1_m);
    M(NSWindow, "occlusionState", w_occlusion); M(NSWindow, "convertRectToScreen:", w_convert_to_screen);
    M(NSWindow, "isKeyWindow", w_is_key); M(NSWindow, "isMainWindow", w_is_main); M(NSWindow, "canBecomeMainWindow", w_can_main);
    M(NSWindow, "setNextResponder:", w_set_next); M(NSWindow, "nextResponder", w_next);

    DEF(NSView, NSResponder);
    M(NSView, "initWithFrame:", v_init_frame); M(NSView, "init", v_init); M(NSView, "frame", v_frame); M(NSView, "bounds", v_bounds);
    M(NSView, "setFrame:", v_set_frame); M(NSView, "setFrameSize:", v_set_size); M(NSView, "setFrameOrigin:", v_set_origin);
    M(NSView, "addSubview:", v_add_sub); M(NSView, "addSubview:positioned:relativeTo:", v_add_positioned);
    M(NSView, "sortSubviewsUsingFunction:context:", v_sort_sub); M(NSView, "setNextKeyView:", v_set_next_kv); M(NSView, "nextKeyView", v_next_kv);
    M(NSView, "previousKeyView", v_prev_kv); M(NSView, "nextValidKeyView", v_next_valid_kv); M(NSView, "previousValidKeyView", v_prev_valid_kv);
    M(NSView, "canBecomeKeyView", v_can_become_key_view);
    M(NSWindow, "selectNextKeyView:", w_select_next_kv); M(NSWindow, "selectPreviousKeyView:", w_select_prev_kv);
    M(NSWindow, "selectKeyViewFollowingView:", w_select_following); M(NSWindow, "selectKeyViewPrecedingView:", w_select_preceding);
    M(NSWindow, "recalculateKeyViewLoop", w_recalc_kv); M(NSWindow, "setAutorecalculatesKeyViewLoop:", w_set_auto_kv);
    M(NSWindow, "autorecalculatesKeyViewLoop", w_auto_kv);
    M(NSView, "removeFromSuperview", v_remove); M(NSView, "subviews", v_subviews); M(NSView, "dealloc", v_dealloc);
    M(NSView, "superview", v_superview); M(NSView, "window", v_window); M(NSView, "setNeedsDisplay:", v_set_needs); M(NSView, "hitTest:", v_hit_test);
    M(NSView, "setNeedsDisplayInRect:", v_set_needs_rect); M(NSView, "display", v_display); M(NSView, "needsDisplay", v_needs);
    M(NSView, "setHidden:", v_set_hidden); M(NSView, "isHidden", v_hidden); M(NSView, "setAutoresizingMask:", v_set_autoresize);
    M(NSView, "autoresizingMask", v_autoresize); M(NSView, "drawRect:", v_draw_rect); M(NSView, "isFlipped", no_m);
    M(NSView, "convertPoint:fromView:", v_convert_from); M(NSView, "convertPoint:toView:", v_convert_to); M(NSView, "convertRect:toView:", v_convert_rect_to);
    M(NSView, "convertRect:fromView:", v_convert_rect_from);
    M(NSView, "setTag:", v_set_tag); M(NSView, "tag", v_tag); M(NSView, "viewWithTag:", v_view_with_tag); M(NSView, "setToolTip:", v_set_tooltip);
    M(NSView, "setWantsLayer:", v_set_wants_layer); M(NSView, "layer", v_layer); M(NSView, "acceptsFirstResponder", v_accepts_first);
    M(NSView, "intrinsicContentSize", v_intrinsic); M(NSView, "sizeToFit", v_size_to_fit); M(NSView, "fittingSize", v_fitting);
    M(NSView, "setTranslatesAutoresizingMaskIntoConstraints:", v_set_tamic); M(NSView, "translatesAutoresizingMaskIntoConstraints", v_tamic); M(NSView, "viewDidMoveToWindow", noop_m);
    M(NSView, "setAlphaValue:", v_set_alpha); M(NSView, "alphaValue", v_alpha); M(NSView, "updateTrackingAreas", noop_m); M(NSView, "addTrackingArea:", noop1_m);
    M(NSView, "acceptsFirstMouse:", yes_m); M(NSView, "mouseDownCanMoveWindow", v_can_move_window);
    M(NSView, "setIdentifier:", v_set_ident); M(NSView, "identifier", v_ident);
    M(NSView, "setNextResponder:", v_set_next); M(NSView, "nextResponder", v_next);

    DEF(NSControl, NSView);
    M(NSControl, "setTarget:", ctl_set_target); M(NSControl, "target", ctl_target); M(NSControl, "setAction:", ctl_set_action); M(NSControl, "action", ctl_action);
    M(NSControl, "setEnabled:", ctl_set_enabled); M(NSControl, "isEnabled", ctl_enabled); M(NSControl, "setFont:", ctl_set_font); M(NSControl, "font", ctl_font);
    M(NSControl, "setAlignment:", ctl_set_align); M(NSControl, "setStringValue:", ctl_set_string); M(NSControl, "stringValue", ctl_string);
    M(NSControl, "setIntValue:", ctl_set_int); M(NSControl, "setIntegerValue:", ctl_set_int); M(NSControl, "setDoubleValue:", ctl_set_double);
    M(NSControl, "setFloatValue:", ctl_set_double); M(NSControl, "intValue", ctl_int); M(NSControl, "integerValue", ctl_int);
    M(NSControl, "doubleValue", ctl_double); M(NSControl, "floatValue", ctl_double); M(NSControl, "sendAction:to:", ctl_send);
    M(NSControl, "mouseDown:", ctl_mouse_down); M(NSControl, "mouseUp:", ctl_mouse_up); M(NSControl, "mouseDragged:", ctl_mouse_dragged);
    M(NSControl, "mouseDownCanMoveWindow", ctl_can_move_window); M(NSControl, "setDelegate:", ctl_set_delegate); M(NSControl, "delegate", ctl_delegate); M(NSControl, "setContinuous:", ctl_set_continuous); M(NSControl, "isContinuous", ctl_is_continuous);
    M(NSControl, "setControlSize:", noop1_m); M(NSControl, "setTextColor:", ctl_set_tcolor); M(NSControl, "textColor", ctl_tcolor);

    DEF(NSButton, NSControl);
    CM(NSButton, "buttonWithTitle:target:action:", bt_with_title); CM(NSButton, "checkboxWithTitle:target:action:", bt_checkbox);
    CM(NSButton, "radioButtonWithTitle:target:action:", bt_radio);
    M(NSButton, "initWithFrame:", bt_init_frame); M(NSButton, "setTitle:", bt_set_title); M(NSButton, "title", bt_title);
    M(NSButton, "setButtonType:", bt_set_type); M(NSButton, "_cxButtonType", bt_type_code); M(NSButton, "setState:", bt_set_state); M(NSButton, "state", bt_state);
    M(NSButton, "setKeyEquivalent:", bt_set_key); M(NSButton, "setBezelStyle:", bt_set_bezel); M(NSButton, "setBordered:", bt_set_bordered);
    M(NSButton, "setContentTintColor:", v_set_tint); M(NSButton, "contentTintColor", v_tint);

    DEF(NSTextField, NSControl);
    CM(NSTextField, "labelWithString:", tf_label); CM(NSTextField, "wrappingLabelWithString:", tf_wrapping_label); CM(NSTextField, "textFieldWithString:", tf_field);
    M(NSTextField, "initWithFrame:", tf_init_frame); M(NSTextField, "setEditable:", tf_set_editable); M(NSTextField, "isEditable", tf_editable);
    M(NSTextField, "setBezeled:", tf_set_bezeled); M(NSTextField, "setBordered:", tf_set_bezeled); M(NSTextField, "setDrawsBackground:", tf_set_draws_bg);
    M(NSTextField, "setBackgroundColor:", tf_set_bg); M(NSTextField, "setPlaceholderString:", tf_set_placeholder); M(NSTextField, "setSelectable:", tf_set_selectable);
    M(NSTextField, "setLineBreakMode:", tf_set_lbm); M(NSTextField, "lineBreakMode", tf_lbm);
    M(NSTextField, "setMaximumNumberOfLines:", tf_set_max_lines); M(NSTextField, "maximumNumberOfLines", tf_max_lines);
    M(NSTextField, "setUsesSingleLineMode:", tf_set_single); M(NSTextField, "usesSingleLineMode", tf_single);
    M(NSTextField, "setPreferredMaxLayoutWidth:", tf_set_pref_w); M(NSTextField, "preferredMaxLayoutWidth", tf_pref_w);

    DEF(NSSlider, NSControl);
    CM(NSSlider, "sliderWithValue:minValue:maxValue:target:action:", sl_with);
    M(NSSlider, "initWithFrame:", sl_init_frame); M(NSSlider, "setMinValue:", sl_set_min); M(NSSlider, "setMaxValue:", sl_set_max);
    M(NSSlider, "minValue", sl_min); M(NSSlider, "maxValue", sl_max);
    DEF(NSProgressIndicator, NSView);
    M(NSProgressIndicator, "initWithFrame:", pg_init_frame); M(NSProgressIndicator, "setDoubleValue:", ctl_set_double); M(NSProgressIndicator, "doubleValue", ctl_double);
    M(NSProgressIndicator, "setMinValue:", sl_set_min); M(NSProgressIndicator, "setMaxValue:", sl_set_max); M(NSProgressIndicator, "incrementBy:", pg_inc);
    M(NSProgressIndicator, "setIndeterminate:", pg_set_indet); M(NSProgressIndicator, "isIndeterminate", pg_indet);
    M(NSProgressIndicator, "startAnimation:", pg_start); M(NSProgressIndicator, "stopAnimation:", pg_stop);
    M(NSProgressIndicator, "_cxProgressTick:", pg_tick); M(NSProgressIndicator, "sizeToFit", pg_size_to_fit);
    M(NSProgressIndicator, "setStyle:", pg_set_style); M(NSProgressIndicator, "style", pg_style);
    M(NSProgressIndicator, "setUsesThreadedAnimation:", pg_set_threaded); M(NSProgressIndicator, "usesThreadedAnimation", pg_threaded);
    M(NSProgressIndicator, "setDisplayedWhenStopped:", pg_set_idle); M(NSProgressIndicator, "isDisplayedWhenStopped", pg_idle);
    DEF(NSBox, NSView);
    M(NSBox, "initWithFrame:", bx_init_frame); M(NSBox, "setTitle:", bt_set_title); M(NSBox, "setContentView:", v_add_sub); M(NSBox, "setBoxType:", bx_set_type); M(NSBox, "setFillColor:", tf_set_bg); M(NSBox, "setCornerRadius:", bx_set_radius);
    DEF(NSStackView, NSView);
    CM(NSStackView, "stackViewWithViews:", st_with_views);
    M(NSStackView, "initWithFrame:", st_init_frame); M(NSStackView, "addArrangedSubview:", st_add); M(NSStackView, "setOrientation:", st_set_orient);
    M(NSStackView, "setSpacing:", st_set_spacing); M(NSStackView, "setAlignment:", st_set_align); M(NSStackView, "alignment", st_align);
    M(NSStackView, "setDistribution:", st_set_dist); M(NSStackView, "distribution", st_dist);
    M(NSStackView, "setEdgeInsets:", st_set_insets); M(NSStackView, "edgeInsets", st_insets);
    M(NSStackView, "spacing", st_spacing); M(NSStackView, "orientation", st_orient);
    M(NSStackView, "arrangedSubviews", st_arranged); M(NSStackView, "insertArrangedSubview:atIndex:", st_insert);
    M(NSStackView, "removeArrangedSubview:", st_remove);
    DEF(NSImageView, NSView);
    M(NSImageView, "initWithFrame:", iv_init_frame); M(NSImageView, "setImage:", iv_set_image); M(NSImageView, "image", iv_image);
    CM(NSImageView, "imageViewWithImage:", iv_with_image); M(NSImageView, "setImageScaling:", iv_set_scaling); M(NSImageView, "imageScaling", iv_scaling); M(NSImageView, "setContentTintColor:", v_set_tint); M(NSImageView, "contentTintColor", v_tint);
    M(NSButton, "setImage:", iv_set_image); M(NSButton, "image", iv_image);
    DEF(NSScrollView, NSView);
    DEF(NSClipView, NSView);
    M(NSScrollView, "initWithFrame:", sv_init_frame); M(NSScrollView, "setDocumentView:", sv_set_doc); M(NSScrollView, "documentView", sv_doc);
    M(NSScrollView, "contentView", sv_content); M(NSScrollView, "scrollWheel:", sv_scroll); M(NSScrollView, "setHasVerticalScroller:", sv_set_has_v);
    M(NSScrollView, "hasVerticalScroller", sv_has_v); M(NSScrollView, "setHasHorizontalScroller:", sv_set_has_h); M(NSScrollView, "hasHorizontalScroller", sv_has_h);
    M(NSScrollView, "setAutohidesScrollers:", sv_set_autohide); M(NSScrollView, "autohidesScrollers", sv_autohide);
    M(NSScrollView, "setBorderType:", sv_set_border); M(NSScrollView, "borderType", sv_border);
    M(NSScrollView, "setDrawsBackground:", sv_set_draws_bg); M(NSScrollView, "drawsBackground", sv_draws_bg);
    M(NSScrollView, "setBackgroundColor:", sv_set_bg); M(NSScrollView, "backgroundColor", sv_bg);
    DEF(NSTableView, NSControl);
    M(NSTableView, "initWithFrame:", tv_init_frame); M(NSTableView, "addTableColumn:", tv_add_col); M(NSTableView, "tableColumns", tv_cols);
    M(NSTableView, "setDataSource:", tv_set_ds); M(NSTableView, "dataSource", tv_ds); M(NSTableView, "reloadData", tv_reload);
    M(NSTableView, "numberOfRows", tv_num_rows); M(NSTableView, "selectedRow", tv_selected); M(NSTableView, "clickedRow", tv_clicked);
    M(NSTableView, "setRowHeight:", tv_set_row_h); M(NSTableView, "rowHeight", tv_row_h); M(NSTableView, "isFlipped", tv_is_flipped);
    M(NSTableView, "setHeaderView:", tv_set_header); M(NSTableView, "headerView", tv_header); M(NSTableView, "setDelegate:", tv_set_delegate);
    M(NSTableView, "registerNib:forIdentifier:", tv_register_nib);
    M(NSTableView, "setUsesAlternatingRowBackgroundColors:", tv_set_alt); M(NSTableView, "setDoubleAction:", tv_set_double);
    M(NSTableView, "makeViewWithIdentifier:owner:", tv_make_view); M(NSTableView, "scrollRowToVisible:", tv_scroll_row);
    M(NSTableView, "selectRowIndexes:byExtendingSelection:", tv_select); M(NSTableView, "deselectAll:", tv_deselect_all);
    M(NSTableView, "mouseDown:", tv_mouse_down); M(NSTableView, "setStyle:", tv_set_style); M(NSTableView, "style", tv_style); M(NSTableView, "effectiveStyle", tv_effective_style); M(NSTableView, "setGridStyleMask:", noop1_m);
    M(NSTableView, "setAllowsMultipleSelection:", noop1_m); M(NSTableView, "sizeLastColumnToFit", tv_size_last);
    M(NSTableView, "rowForView:", tv_row_for_view); M(NSTableView, "columnForView:", tv_col_for_view);
    M(NSTableView, "viewAtColumn:row:makeIfNecessary:", tv_view_at); M(NSTableView, "numberOfColumns", tv_num_cols);
    M(NSTableView, "columnWithIdentifier:", tv_col_with_ident); M(NSTableView, "tableColumnWithIdentifier:", tv_col_named);
    M(NSTableView, "selectedRowIndexes", tv_sel_indexes); M(NSTableView, "removeTableColumn:", tv_remove_col);
    M(NSTableView, "noteNumberOfRowsChanged", tv_note_rows); M(NSTableView, "reloadDataForRowIndexes:columnIndexes:", tv_reload_rows);
    M(NSTableView, "editColumn:row:withEvent:select:", tv_edit);
    DEF(NSTableColumn, NSObject);
    M(NSTableColumn, "initWithIdentifier:", col_init); M(NSTableColumn, "identifier", col_ident); M(NSTableColumn, "setTitle:", col_set_title);
    M(NSTableColumn, "title", col_title); M(NSTableColumn, "setWidth:", col_set_width); M(NSTableColumn, "width", col_width);
    /* depois da NSTableColumn existir: o registro põe nela minWidth/maxWidth, editable, dataCell, headerCell... (antes
     * os métodos de coluna caíam numa classe ainda indefinida e a tabela ficava sem eles) */
    cx_table2_register();   /* appkit_table2.inc: seleção múltipla, teclado, ordenação, colunas, arrastar, geometria */
    DEF(NSTableCellView, NSView);
    M(NSTableCellView, "setTextField:", tcv_set_tf); M(NSTableCellView, "textField", tcv_tf);
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
    M(NSPasteboard, "prepareForNewContentsWithOptions:", pb_prepare); M(NSPasteboard, "indexOfPasteboardItem:", pb_index_of_item);
    DEF(NSPasteboardItem, NSObject);
    M(NSPasteboardItem, "init", pi_init); M(NSPasteboardItem, "types", pi_types); M(NSPasteboardItem, "availableTypeFromArray:", pi_available);
    M(NSPasteboardItem, "setData:forType:", pi_set); M(NSPasteboardItem, "setString:forType:", pi_set); M(NSPasteboardItem, "setPropertyList:forType:", pi_set);
    M(NSPasteboardItem, "dataForType:", pi_data); M(NSPasteboardItem, "stringForType:", pi_string); M(NSPasteboardItem, "propertyListForType:", pi_plist);
    M(NSPasteboardItem, "setDataProvider:forTypes:", pi_set_provider);
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
    CM(NSBezierPath, "clipRect:", bp_clip_rect); CM(NSBezierPath, "bezierPathWithCGPath:", bp_with_cgpath);
    CM(NSBezierPath, "defaultLineWidth", bp_def_lw); CM(NSBezierPath, "setDefaultLineWidth:", bp_set_def_lw);
    CM(NSBezierPath, "defaultLineCapStyle", bp_def_cap); CM(NSBezierPath, "setDefaultLineCapStyle:", bp_set_def_cap);
    CM(NSBezierPath, "defaultLineJoinStyle", bp_def_join); CM(NSBezierPath, "setDefaultLineJoinStyle:", bp_set_def_join);
    CM(NSBezierPath, "defaultWindingRule", bp_def_rule); CM(NSBezierPath, "setDefaultWindingRule:", bp_set_def_rule);
    CM(NSBezierPath, "defaultMiterLimit", bp_def_miter); CM(NSBezierPath, "setDefaultMiterLimit:", bp_set_def_miter);
    CM(NSBezierPath, "defaultFlatness", bp_def_flat); CM(NSBezierPath, "setDefaultFlatness:", bp_set_def_flat);
    M(NSBezierPath, "moveToPoint:", bp_move); M(NSBezierPath, "lineToPoint:", bp_line); M(NSBezierPath, "curveToPoint:controlPoint1:controlPoint2:", bp_curve);
    M(NSBezierPath, "relativeMoveToPoint:", bp_rmove); M(NSBezierPath, "relativeLineToPoint:", bp_rline);
    M(NSBezierPath, "relativeCurveToPoint:controlPoint1:controlPoint2:", bp_rcurve);
    M(NSBezierPath, "closePath", bp_close); M(NSBezierPath, "appendBezierPathWithRect:", bp_app_rect); M(NSBezierPath, "appendBezierPathWithOvalInRect:", bp_app_oval);
    M(NSBezierPath, "appendBezierPathWithRoundedRect:xRadius:yRadius:", bp_app_rrect); M(NSBezierPath, "fill", bp_fill); M(NSBezierPath, "stroke", bp_stroke);
    M(NSBezierPath, "appendBezierPath:", bp_append); M(NSBezierPath, "appendBezierPathWithPoints:count:", bp_append_points);
    M(NSBezierPath, "appendBezierPathWithArcWithCenter:radius:startAngle:endAngle:clockwise:", bp_arc_cw);
    M(NSBezierPath, "appendBezierPathWithArcWithCenter:radius:startAngle:endAngle:", bp_arc);
    M(NSBezierPath, "appendBezierPathWithArcFromPoint:toPoint:radius:", bp_arc_from);
    M(NSBezierPath, "setLineWidth:", bp_set_lw); M(NSBezierPath, "lineWidth", bp_lw); M(NSBezierPath, "removeAllPoints", bp_remove_all);
    M(NSBezierPath, "bounds", bp_bounds); M(NSBezierPath, "controlPointBounds", bp_cbounds); M(NSBezierPath, "addClip", bp_add_clip); M(NSBezierPath, "setClip", bp_set_clip);
    M(NSBezierPath, "lineCapStyle", bp_cap); M(NSBezierPath, "setLineCapStyle:", bp_set_cap);
    M(NSBezierPath, "lineJoinStyle", bp_join); M(NSBezierPath, "setLineJoinStyle:", bp_set_join);
    M(NSBezierPath, "windingRule", bp_rule); M(NSBezierPath, "setWindingRule:", bp_set_rule);
    M(NSBezierPath, "miterLimit", bp_miter); M(NSBezierPath, "setMiterLimit:", bp_set_miter);
    M(NSBezierPath, "flatness", bp_flat); M(NSBezierPath, "setFlatness:", bp_set_flat);
    M(NSBezierPath, "setLineDash:count:phase:", bp_set_dash); M(NSBezierPath, "getLineDash:count:phase:", bp_get_dash);
    M(NSBezierPath, "currentPoint", bp_current_point); M(NSBezierPath, "isEmpty", bp_is_empty); M(NSBezierPath, "elementCount", bp_count);
    M(NSBezierPath, "elementAtIndex:", bp_element); M(NSBezierPath, "elementAtIndex:associatedPoints:", bp_element_pts);
    M(NSBezierPath, "setAssociatedPoints:atIndex:", bp_set_assoc); M(NSBezierPath, "containsPoint:", bp_contains);
    M(NSBezierPath, "transformUsingAffineTransform:", bp_transform); M(NSBezierPath, "bezierPathByReversingPath", bp_reversed);
    M(NSBezierPath, "bezierPathByFlatteningPath", bp_flattened); M(NSBezierPath, "copyWithZone:", bp_copy);
    M(NSBezierPath, "CGPath", bp_cgpath); M(NSBezierPath, "setCGPath:", bp_set_cgpath);
    { cx_class_t *at = cx_class_named("NSAffineTransform"); if (at) cx_add_method(at, "transformBezierPath:", (void *)at_transform_path); }

    cx_add_method(&NSString_c, "drawAtPoint:withAttributes:", (void *)str_draw_at);
    cx_add_method(&NSString_c, "drawInRect:withAttributes:", (void *)str_draw_in);
    cx_add_method(&NSString_c, "sizeWithAttributes:", (void *)str_size);
    M(NSAttributedString, "drawAtPoint:", as_draw_at); M(NSAttributedString, "size", as_size);   /* categoria do AppKit */

    DEF(NSGraphicsContext, NSObject);
    CM(NSGraphicsContext, "currentContext", gc_current); CM(NSGraphicsContext, "saveGraphicsState", gc_save); CM(NSGraphicsContext, "restoreGraphicsState", gc_restore);
    M(NSGraphicsContext, "CGContext", gc_cg); M(NSGraphicsContext, "graphicsPort", gc_cg); M(NSGraphicsContext, "saveGraphicsState", gc_save);
    M(NSGraphicsContext, "restoreGraphicsState", gc_restore); M(NSGraphicsContext, "setShouldAntialias:", noop1_m);

    DEF(NSEvent, NSObject);
    CM(NSEvent, "modifierFlags", ev_cmods); CM(NSEvent, "pressedMouseButtons", ev_pressed_buttons); CM(NSEvent, "mouseLocation", ev_mouse_location);
    M(NSEvent, "type", ev_type); M(NSEvent, "locationInWindow", ev_loc); M(NSEvent, "modifierFlags", ev_mods); M(NSEvent, "keyCode", ev_keycode);
    M(NSEvent, "characters", ev_chars); M(NSEvent, "charactersIgnoringModifiers", ev_chars); M(NSEvent, "clickCount", ev_clicks);
    M(NSEvent, "window", ev_window); M(NSEvent, "scrollingDeltaY", ev_dy); M(NSEvent, "deltaY", ev_dy); M(NSEvent, "deltaX", ev_dx);
    M(NSEvent, "scrollingDeltaX", ev_dx); M(NSEvent, "isARepeat", ev_is_repeat);
    CM(NSEvent, "otherEventWithType:location:modifierFlags:timestamp:windowNumber:context:subtype:data1:data2:", ev_other);
    CM(NSEvent, "mouseEventWithType:location:modifierFlags:timestamp:windowNumber:context:eventNumber:clickCount:pressure:", ev_mouse_make);
    CM(NSEvent, "keyEventWithType:location:modifierFlags:timestamp:windowNumber:context:characters:charactersIgnoringModifiers:isARepeat:keyCode:", ev_key_make);
    CM(NSEvent, "enterExitEventWithType:location:modifierFlags:timestamp:windowNumber:context:eventNumber:trackingNumber:userData:", ev_enter_exit_make);
    CM(NSEvent, "startPeriodicEventsAfterDelay:withPeriod:", ev_start_periodic); CM(NSEvent, "stopPeriodicEvents", ev_stop_periodic);
    M(NSEvent, "timestamp", ev_ts); M(NSEvent, "windowNumber", ev_wnum); M(NSEvent, "subtype", ev_subtype);
    M(NSEvent, "data1", ev_data1); M(NSEvent, "data2", ev_data2); M(NSEvent, "buttonNumber", ev_button);
    M(NSEvent, "pressure", ev_pressure); M(NSEvent, "deltaZ", ev_zero); M(NSEvent, "magnification", ev_zero);
    M(NSEvent, "rotation", ev_fzero); M(NSEvent, "tangentialPressure", ev_fzero); M(NSEvent, "stageTransition", ev_zero);
    M(NSEvent, "eventNumber", ev_izero); M(NSEvent, "phase", ev_izero); M(NSEvent, "momentumPhase", ev_izero);
    M(NSEvent, "hasPreciseScrollingDeltas", no_m); M(NSEvent, "isDirectionInvertedFromDevice", ev_direction_inverted);
    M(NSEvent, "stage", ev_izero); M(NSEvent, "CGEvent", ev_izero);

    DEF(NSMenu, NSObject);
    M(NSMenu, "initWithTitle:", mn_init_title); M(NSMenu, "addItem:", mn_add); M(NSMenu, "itemArray", mn_items); M(NSMenu, "items", mn_items);
    M(NSMenu, "addItemWithTitle:action:keyEquivalent:", mn_add_titled); M(NSMenu, "setAutoenablesItems:", noop1_m); M(NSMenu, "removeAllItems", mn_remove_all);
    M(NSMenu, "numberOfItems", mn_count); M(NSMenu, "itemAtIndex:", mn_item_at); M(NSMenu, "removeItemAtIndex:", mn_remove_at);
    M(NSMenu, "removeItem:", mn_remove); M(NSMenu, "insertItem:atIndex:", mn_insert_at); M(NSMenu, "indexOfItem:", mn_index_of);
    M(NSMenu, "itemWithTag:", mn_item_tag); M(NSMenu, "indexOfItemWithTag:", mn_index_tag); M(NSMenu, "itemWithTitle:", mn_item_title);
    M(NSMenu, "title", mn_title_get); M(NSMenu, "setTitle:", mn_init_title);
    M(NSMenu, "delegate", mn_delegate); M(NSMenu, "setDelegate:", mn_set_delegate);
    DEF(NSMenuItem, NSObject);
    CM(NSMenuItem, "separatorItem", mi_sep);
    M(NSMenuItem, "initWithTitle:action:keyEquivalent:", mi_init); M(NSMenuItem, "setSubmenu:", mi_set_submenu); M(NSMenuItem, "submenu", mi_submenu);
    M(NSMenuItem, "setTarget:", mi_set_target); M(NSMenuItem, "title", mi_title); M(NSMenuItem, "setKeyEquivalentModifierMask:", mi_set_mask);
    M(NSMenuItem, "keyEquivalentModifierMask", mi_mask_get); M(NSMenuItem, "userKeyEquivalent", mi_user_key); M(NSMenuItem, "setUserKeyEquivalent:", mi_set_key);
    M(NSMenuItem, "userKeyEquivalentModifierMask", mi_user_mask); M(NSMenuItem, "setUserKeyEquivalentModifierMask:", mi_set_mask);
    M(NSMenuItem, "setAction:", mi_set_action); M(NSMenuItem, "setTitle:", mi_set_title); M(NSMenuItem, "setKeyEquivalent:", mi_set_key); M(NSMenuItem, "tag", mi_tag); M(NSMenuItem, "state", mi_state);
    M(NSMenuItem, "setAttributedTitle:", mi_set_attr); M(NSMenuItem, "attributedTitle", mi_attr);
    M(NSMenuItem, "setTag:", mi_set_tag); M(NSMenuItem, "setState:", mi_set_state); M(NSMenuItem, "setEnabled:", mi_set_enabled);
    M(NSMenuItem, "setToolTip:", mi_set_tooltip); M(NSMenuItem, "toolTip", mi_tooltip);
    M(NSMenuItem, "setAllowsKeyEquivalentWhenHidden:", mi_set_bool_ignored);
    M(NSMenuItem, "isSeparatorItem", mi_is_sep); M(NSMenuItem, "setHidden:", mi_set_hidden); M(NSMenuItem, "isHidden", mi_hidden);
    M(NSMenuItem, "setAlternate:", mi_set_alternate); M(NSMenuItem, "isAlternate", mi_alternate);
    M(NSMenuItem, "setImage:", mi_set_image); M(NSMenuItem, "image", mi_image);
    M(NSMenuItem, "setRepresentedObject:", mi_set_repr); M(NSMenuItem, "representedObject", mi_repr);
    M(NSMenuItem, "isSectionHeader", mi_is_header); M(NSMenuItem, "isEnabled", mi_enabled); M(NSMenuItem, "keyEquivalent", mi_key);
    M(NSMenuItem, "action", mi_action); M(NSMenuItem, "target", mi_target); M(NSMenuItem, "hasSubmenu", mi_has_submenu);
    CM(NSMenuItem, "sectionHeaderWithTitle:", mi_section_header);
    M(NSMenuItem, "menu", mi_menu);

    DEF(NSAlert, NSObject);
    M(NSAlert, "setMessageText:", al_set_msg); M(NSAlert, "setInformativeText:", al_set_info); M(NSAlert, "addButtonWithTitle:", al_add_button);
    M(NSAlert, "setAlertStyle:", al_set_style); M(NSAlert, "runModal", al_run);
    M(NSAlert, "messageText", al_msg); M(NSAlert, "informativeText", al_info); M(NSAlert, "buttons", al_buttons);
    M(NSAlert, "alertStyle", al_style); M(NSAlert, "setIcon:", al_set_icon); M(NSAlert, "icon", al_icon);
    M(NSAlert, "setAccessoryView:", al_set_accessory); M(NSAlert, "accessoryView", al_accessory);
    M(NSAlert, "setShowsSuppressionButton:", al_set_shows_suppress); M(NSAlert, "showsSuppressionButton", al_shows_suppress);
    M(NSAlert, "suppressionButton", al_suppress); M(NSAlert, "window", al_window); M(NSAlert, "layout", al_noop);
    M(NSAlert, "setShowsHelp:", al_noop1); M(NSAlert, "setHelpAnchor:", al_noop1); M(NSAlert, "setDelegate:", al_noop1);
    M(NSAlert, "beginSheetModalForWindow:completionHandler:", al_begin_sheet);
    M(NSAlert, "beginSheetModalForWindow:modalDelegate:didEndSelector:contextInfo:", al_begin_sheet_delegate);
    cx_define_class(&AlertTarget_c, &AlertTarget_m, "_CXAlertTarget", &NSObject_c, 0);
    cx_add_method(&AlertTarget_c, "cxAlertButton:", (void *)al_button_hit);

    DEF(NSScreen, NSObject);
    CM(NSScreen, "mainScreen", sc_main); CM(NSScreen, "screens", sc_screens);
    M(NSScreen, "frame", sc_frame); M(NSScreen, "visibleFrame", sc_visible); M(NSScreen, "backingScaleFactor", sc_scale);
    DEF(NSCursor, NSObject);
    /* cursores, set/push/pop e cursor rects: appkit_cursor.c */
    DEF(NSWorkspace, NSObject);
    CM(NSWorkspace, "sharedWorkspace", generic_singleton); M(NSWorkspace, "openURL:", ws_open_url);
    DEF(NSImage, NSObject);
    CM(NSImage, "imageNamed:", im_named); CM(NSImage, "imageWithSystemSymbolName:accessibilityDescription:", noop_ret_nil);
    M(NSImage, "initWithCGImage:size:", im_init_cg); M(NSImage, "CGImageForProposedRect:context:hints:", im_cg_for_rect);
    M(NSImage, "initWithContentsOfFile:", im_init_file); M(NSImage, "initWithData:", im_init_data); M(NSImage, "initWithSize:", im_init_size);
    M(NSImage, "size", im_size); M(NSImage, "setSize:", im_set_size); M(NSImage, "isValid", im_valid); M(NSImage, "drawInRect:", im_draw_in);
    M(NSImage, "drawInRect:fromRect:operation:fraction:", im_draw_in_frac); M(NSImage, "drawAtPoint:fromRect:operation:fraction:", im_draw_at);
    M(NSImage, "setTemplate:", im_set_template); M(NSImage, "isTemplate", im_is_template); M(NSImage, "lockFocus", noop_m); M(NSImage, "unlockFocus", noop_m);
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
    M(NSView, "layout", v_layout_now); M(NSView, "setNeedsLayout:", v_set_needs); 
    M(NSView, "updateConstraints", v_update_constraints); M(NSView, "updateConstraintsForSubtreeIfNeeded", v_update_constraints_subtree);
    M(NSView, "setNeedsUpdateConstraints:", v_set_needs_uc); M(NSView, "needsUpdateConstraints", v_needs_uc);
    DEF(NSLayoutGuide, NSView);
    M(NSLayoutGuide, "owningView", lg_owning); M(NSLayoutGuide, "layoutFrame", lg_frame); M(NSLayoutGuide, "frame", lg_frame);
    M(NSView, "addLayoutGuide:", v_add_guide); M(NSView, "removeLayoutGuide:", v_remove_guide);
    M(NSView, "safeAreaLayoutGuide", v_safe_area); M(NSView, "layoutMarginsGuide", v_margins);
    M(NSView, "setContentHuggingPriority:forOrientation:", v_set_hug); M(NSView, "contentHuggingPriorityForOrientation:", v_hug);
    M(NSView, "setContentCompressionResistancePriority:forOrientation:", v_set_cr); M(NSView, "contentCompressionResistancePriorityForOrientation:", v_cr);
    M(NSWindow, "layoutIfNeeded", w_layout_if_needed);
    nib_register();
    bindings_register();
    cx_appkit_input_init();     /* appkit_input.c: interpretKeyEvents:, NSTextInputContext, arrastar e soltar */
    (void)run_loop_until;
    cx_runloop_hook = run_loop_until;
    { extern void cx_un_install_runloop(void); cx_un_install_runloop(); }
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
