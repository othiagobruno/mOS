/*
 * mOS libSystem — o resto do Carbon/ApplicationServices que o corpus pede:
 *   HITheme: trilhas (barra de rolagem, slider, progresso: desenho, limites, partes, acerto, polegar, marcas), foco,
 *     caixa de grupo, divisor, separador, abas, painel de abas, alça de redimensionar, placa, seta de pop-up, setas
 *     giratórias, botões da barra de título, moldura de janela, fundo, barra e separador de menu; pincéis e cores de
 *     texto do tema; fontes do tema; medidas de texto, de botão e de menu. Geometria das trilhas em carbon_core.c
 *     (teste userland/tests-appkit/carbon_host.c).
 *   Process Manager: GetFrontProcess, SameProcess, CopyProcessName, GetProcessBundleLocation, KillProcess, WakeUpProcess.
 *   Dock e cursor (privados): CoreDock* (preferências do Dock pelo window server), CoreCursorSet/UnregisterAll.
 *   Acessibilidade: kAX*TextAttribute, AXAPIEnabled, AXObserverGetTypeID, AXUIElementPostKeyboardEvent (no próprio app).
 *   Teclado e mouse: GetKeys, Button, HIGetMousePosition, HIPointConvert/HIRectConvert, HIMouseTrackingGetParameters;
 *   TIS: as propriedades que faltavam e TISRegisterInputSource. Apple Help (AHGotoPage, AHLookupAnchor), Internet Config
 *   (ICStart/ICStop/ICGetPref/ICMapFilename), interação de Apple Events, GetApplicationTextEncoding, temas.
 */
#include "foundation.h"
#include "sys.h"
#include "libc.h"
#include "carbon_core.h"
#include "../../kernel/include/compat_gui.h"

typedef int OSStatus;
#define noErr 0
#define paramErr (-50)
#define procNotFound (-600)
id objc_msgSend_stret(id self, SEL op, ...);
id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
int getpid(void);
int kill(int pid, int sig);

typedef struct { double x, y; } P;
typedef struct { double w, h; } S;
typedef struct { double x, y, w, h; } R;
void CGContextSaveGState(void *c);
void CGContextRestoreGState(void *c);
void CGContextFillRect(void *c, R r);
void CGContextStrokeRectWithWidth(void *c, R r, double w);
void CGContextSetRGBFillColor(void *c, double r, double g, double b, double a);
void CGContextSetRGBStrokeColor(void *c, double r, double g, double b, double a);
void CGContextFillEllipseInRect(void *c, R r);
void CGContextMoveToPoint(void *c, double x, double y);
void CGContextAddLineToPoint(void *c, double x, double y);
void CGContextClosePath(void *c);
void CGContextFillPath(void *c);
void CGContextStrokePath(void *c);
void CGContextBeginPath(void *c);
void CGContextSetLineWidth(void *c, double w);
id CGColorCreateGenericRGB(double r, double g, double b, double a);
id HIShapeCreateWithRect(const R *r);
extern id NSFontAttributeName;

static cx_class_t *kls(const char *n) { return cx_class_named(n); }
static SEL sl(const char *n) { return sel_registerName(n); }
static BOOL responds(id o, const char *s) { return o && cx_responds(object_getClass(o), sl(s)); }
static R mkr(double x, double y, double w, double h) { R r = {x, y, w, h}; return r; }
static void fill(void *c, R r, double g, double a) { CGContextSetRGBFillColor(c, g, g, g, a); CGContextFillRect(c, r); }
static void fillc(void *c, R r, double R_, double G, double B, double a) { CGContextSetRGBFillColor(c, R_, G, B, a); CGContextFillRect(c, r); }
static void frame(void *c, R r, double g) { CGContextSetRGBStrokeColor(c, g, g, g, 1); CGContextStrokeRectWithWidth(c, r, 1); }
static void line(void *c, double x0, double y0, double x1, double y1, double g) {
    CGContextSetRGBStrokeColor(c, g, g, g, 1); CGContextSetLineWidth(c, 1);
    CGContextBeginPath(c); CGContextMoveToPoint(c, x0, y0); CGContextAddLineToPoint(c, x1, y1); CGContextStrokePath(c);
}
static void tri(void *c, double ax, double ay, double bx, double by, double cx, double cy, double g, double a) {
    CGContextSetRGBFillColor(c, g, g, g, a);
    CGContextBeginPath(c); CGContextMoveToPoint(c, ax, ay); CGContextAddLineToPoint(c, bx, by); CGContextAddLineToPoint(c, cx, cy);
    CGContextClosePath(c); CGContextFillPath(c);
}

/* ================================================================ trilhas */
/* HIThemeTrackDrawInfo: version u32 @0, kind u16 @4, bounds @8, min @40, max @44, value @48, reserved @52,
 * attributes u16 @56, enableState u8 @58, filler @59, trackInfo @60 (scroll: viewsize i32, pressState u8; slider:
 * thumbDir u8, pressState u8; progress: phase u8) */
typedef struct {
    u32 version; u16 kind; u16 pad; R bounds; i32 min, max, value; u32 reserved; u16 attributes; u8 enable, filler;
    union { struct { i32 viewsize; u8 press; } scroll; struct { u8 dir, press; } slider; struct { u8 phase; } progress; } info;
} track_info;
static int track_class(const track_info *t) {
    switch (t->kind) {
    case 0: case 1: case 12: return 0;                         /* barras de rolagem */
    case 2: case 6: case 13: return 1;                         /* sliders */
    default: return 2;                                         /* progresso, indeterminado, relevância */
    }
}
static int horiz(const track_info *t) { return (t->attributes & 1) != 0; }
static double tlen(const track_info *t) { return horiz(t) ? t->bounds.w : t->bounds.h; }
static R thumb_rect(const track_info *t) {
    int k = track_class(t);
    cc_thumb th = cc_track_thumb(k, tlen(t), t->min, t->max, t->value, k == 0 ? t->info.scroll.viewsize : 0);
    R b = t->bounds;
    return horiz(t) ? mkr(b.x + th.pos, b.y, th.len, b.h) : mkr(b.x, b.y + th.pos, b.w, th.len);
}
OSStatus HIThemeGetTrackBounds(const track_info *t, R *out) { if (!t || !out) return paramErr; *out = t->bounds; return noErr; }
OSStatus HIThemeGetTrackDragRect(const track_info *t, R *out) { return HIThemeGetTrackBounds(t, out); }
OSStatus HIThemeGetTrackPartBounds(const track_info *t, i16 part, R *out) {
    if (!t || !out) return paramErr;
    R th = thumb_rect(t), b = t->bounds;
    switch (part) {
    case 129: *out = th; break;                                                    /* polegar */
    case 22: *out = horiz(t) ? mkr(b.x, b.y, th.x - b.x, b.h) : mkr(b.x, b.y, b.w, th.y - b.y); break;
    case 23: *out = horiz(t) ? mkr(th.x + th.w, b.y, b.x + b.w - th.x - th.w, b.h) : mkr(b.x, th.y + th.h, b.w, b.y + b.h - th.y - th.h); break;
    case 20: case 21: *out = mkr(0, 0, 0, 0); break;                              /* o tema do mOS não tem setas */
    default: return -30580;                                                        /* errInvalidPartCode */
    }
    return noErr;
}
OSStatus HIThemeGetTrackThumbShape(const track_info *t, id *out) {
    if (!t || !out) return paramErr;
    R th = thumb_rect(t);
    *out = HIShapeCreateWithRect(&th);
    return noErr;
}
u8 HIThemeHitTestTrack(const track_info *t, const P *p, i16 *part) {
    if (!t || !p) return 0;
    R b = t->bounds;
    if (p->x < b.x || p->y < b.y || p->x > b.x + b.w || p->y > b.y + b.h) { if (part) *part = 0; return 0; }
    int k = track_class(t);
    double at = horiz(t) ? p->x - b.x : p->y - b.y;
    int hit = cc_track_hit(k, tlen(t), t->min, t->max, t->value, k == 0 ? t->info.scroll.viewsize : 0, at);
    if (part) *part = (i16)hit;
    return hit != 0;
}
/* setas da barra de rolagem: o tema do mOS não desenha setas, então a área toda é trilha */
u8 HIThemeHitTestScrollBarArrows(const R *bounds, const void *info, u8 is_horiz, const P *p, R *track, i16 *part) {
    (void)info; (void)is_horiz; (void)p;
    if (track && bounds) *track = *bounds;
    if (part) *part = 0;
    return 0;
}
OSStatus HIThemeDrawTrack(const track_info *t, const R *ghost, void *c, u32 orient) {
    (void)orient;
    if (!t || !c) return paramErr;
    R b = t->bounds;
    int k = track_class(t);
    double alpha = t->enable == 0 ? 0.5 : 1;
    if (k == 0) {
        fill(c, b, 0.97, alpha);
        R th = thumb_rect(t);
        R in = horiz(t) ? mkr(th.x + 2, th.y + 3, th.w - 4, th.h - 6) : mkr(th.x + 3, th.y + 2, th.w - 6, th.h - 4);
        fill(c, in, t->info.scroll.press ? 0.45 : 0.62, alpha);
    } else if (k == 1) {
        R groove = horiz(t) ? mkr(b.x, b.y + b.h / 2 - 2, b.w, 4) : mkr(b.x + b.w / 2 - 2, b.y, 4, b.h);
        fill(c, groove, 0.82, alpha);
        R th = thumb_rect(t);
        R fillr = horiz(t) ? mkr(b.x, groove.y, th.x - b.x + th.w / 2, 4) : mkr(groove.x, b.y, 4, th.y - b.y + th.h / 2);
        fillc(c, fillr, 0.13, 0.45, 0.95, alpha);
        R knob = horiz(t) ? mkr(th.x, b.y + b.h / 2 - 7.5, 15, 15) : mkr(b.x + b.w / 2 - 7.5, th.y, 15, 15);
        CGContextSetRGBFillColor(c, 1, 1, 1, alpha); CGContextFillEllipseInRect(c, knob);
    } else {
        fill(c, b, 0.88, alpha);
        if (t->kind == 4 || t->kind == 8 || t->kind == 15) {   /* indeterminado: faixas que andam com a fase */
            double step = 16, off = (double)(t->info.progress.phase % 16);
            for (double x = b.x - step + off; x < b.x + b.w; x += step * 2) {
                double x0 = x < b.x ? b.x : x, x1 = x + step > b.x + b.w ? b.x + b.w : x + step;
                if (x1 > x0) fillc(c, mkr(x0, b.y, x1 - x0, b.h), 0.13, 0.45, 0.95, alpha);
            }
        } else {
            R th = thumb_rect(t);
            fillc(c, horiz(t) ? mkr(b.x, b.y, th.w, b.h) : mkr(b.x, b.y + b.h - th.h, b.w, th.h), 0.13, 0.45, 0.95, alpha);
        }
    }
    if (ghost && k != 2) frame(c, *ghost, 0.5);
    return noErr;
}
OSStatus HIThemeDrawTrackTickMarks(const track_info *t, u32 n, void *c, u32 orient) {
    (void)orient;
    if (!t || !c) return paramErr;
    R b = t->bounds;
    for (u32 i = 0; i < n; i++) {
        double f = n > 1 ? (double)i / (n - 1) : 0.5;
        if (horiz(t)) { double x = b.x + 7.5 + f * (b.w - 15); line(c, x, b.y + b.h - 4, x, b.y + b.h, 0.5); }
        else { double y = b.y + 7.5 + f * (b.h - 15); line(c, b.x + b.w - 4, y, b.x + b.w, y, 0.5); }
    }
    return noErr;
}

/* ================================================================ foco, grupos, separadores, abas, janelas */
static int g_focus_depth;
OSStatus HIThemeBeginFocus(void *c, u32 kind, void *reserved) { (void)kind; (void)reserved; if (!c) return paramErr; CGContextSaveGState(c); g_focus_depth++; return noErr; }
OSStatus HIThemeEndFocus(void *c) { if (!c || !g_focus_depth) return paramErr; CGContextRestoreGState(c); g_focus_depth--; return noErr; }
OSStatus HIThemeDrawFocusRect(const R *r, u8 has_focus, void *c, u32 orient) {
    (void)orient;
    if (!r || !c) return paramErr;
    if (!has_focus) return noErr;
    CGContextSetRGBStrokeColor(c, 0.25, 0.55, 1, 0.6);
    CGContextStrokeRectWithWidth(c, mkr(r->x - 1.5, r->y - 1.5, r->w + 3, r->h + 3), 3);
    return noErr;
}
typedef struct { u32 version; u32 state; u32 kind; } hi_group_t;
OSStatus HIThemeDrawGroupBox(const R *r, const hi_group_t *info, void *c, u32 orient) {
    (void)orient;
    if (!r || !c) return paramErr;
    int secondary = info && info->kind == 1;   /* kHIThemeGroupBoxKindSecondary */
    fill(c, *r, secondary ? 0.95 : 0.93, 1);
    frame(c, *r, 0.80);
    return noErr;
}
OSStatus HIThemeDrawPaneSplitter(const R *r, const void *info, void *c, u32 orient) {
    (void)info; (void)orient;
    if (!r || !c) return paramErr;
    fill(c, *r, 0.88, 1);
    double cx = r->x + r->w / 2, cy = r->y + r->h / 2;
    CGContextSetRGBFillColor(c, 0.55, 0.55, 0.57, 1);
    CGContextFillEllipseInRect(c, mkr(cx - 2, cy - 2, 4, 4));
    return noErr;
}
OSStatus HIThemeDrawSeparator(const R *r, const void *info, void *c, u32 orient) {
    (void)info; (void)orient;
    if (!r || !c) return paramErr;
    if (r->w >= r->h) line(c, r->x, r->y + r->h / 2, r->x + r->w, r->y + r->h / 2, 0.80);
    else line(c, r->x + r->w / 2, r->y, r->x + r->w / 2, r->y + r->h, 0.80);
    return noErr;
}
OSStatus HIThemeDrawMenuSeparator(const R *menu, const R *item, const void *info, void *c, u32 orient) {
    (void)menu; (void)info; (void)orient;
    if (!item || !c) return paramErr;
    line(c, item->x + 10, item->y + item->h / 2, item->x + item->w - 10, item->y + item->h / 2, 0.82);
    return noErr;
}
OSStatus HIThemeDrawMenuBarBackground(const R *r, const void *info, void *c, u32 orient) {
    (void)info; (void)orient;
    if (!r || !c) return paramErr;
    fill(c, *r, 0.96, 0.92);
    line(c, r->x, r->y + r->h - 0.5, r->x + r->w, r->y + r->h - 0.5, 0.80);
    return noErr;
}
OSStatus HIThemeGetMenuBackgroundShape(const R *menu, const void *info, id *out) { (void)info; if (!menu || !out) return paramErr; *out = HIShapeCreateWithRect(menu); return noErr; }
/* HIThemeTabDrawInfo: version, style (0 front, 1 back unpressed, 2 back pressed, 3 front inactive, 4 front unavailable),
 * direction, size, adornment, kind, position */
typedef struct { u32 version; u32 style, direction, size, adornment, kind, position; } hi_tab_t;
OSStatus HIThemeDrawTab(const R *r, const hi_tab_t *info, void *c, u32 orient, R *label) {
    (void)orient;
    if (!r || !c) return paramErr;
    int front = !info || info->style == 0 || info->style == 3;
    fill(c, *r, front ? 1.0 : (info && info->style == 2 ? 0.80 : 0.90), 1);
    frame(c, *r, 0.72);
    if (label) *label = mkr(r->x + 8, r->y + 2, r->w - 16 > 0 ? r->w - 16 : 0, r->h - 4 > 0 ? r->h - 4 : 0);
    return noErr;
}
OSStatus HIThemeDrawTabPane(const R *r, const void *info, void *c, u32 orient) {
    (void)info; (void)orient;
    if (!r || !c) return paramErr;
    fill(c, *r, 0.95, 1);
    frame(c, *r, 0.78);
    return noErr;
}
OSStatus HIThemeDrawPlacard(const R *r, const void *info, void *c, u32 orient) {
    (void)info; (void)orient;
    if (!r || !c) return paramErr;
    fill(c, *r, 0.93, 1);
    frame(c, *r, 0.75);
    return noErr;
}
/* HIThemeGrowBoxDrawInfo: version, state, kind, direction, size; a alça fica no canto e mede 15 (pequena 11) */
typedef struct { u32 version, state, kind, direction, size; } hi_grow_t;
static double grow_size(const hi_grow_t *g) { return g && g->size == 1 ? 11 : 15; }
OSStatus HIThemeGetGrowBoxBounds(const P *origin, const hi_grow_t *info, R *out) {
    if (!origin || !out) return paramErr;
    double s = grow_size(info);
    *out = mkr(origin->x - s, origin->y - s, s, s);
    return noErr;
}
OSStatus HIThemeDrawGrowBox(const P *origin, const hi_grow_t *info, void *c, u32 orient) {
    (void)orient;
    if (!origin || !c) return paramErr;
    R r;
    HIThemeGetGrowBoxBounds(origin, info, &r);
    for (int i = 1; i <= 3; i++) line(c, r.x + r.w - i * 4, r.y + r.h, r.x + r.w, r.y + r.h - i * 4, 0.55);
    return noErr;
}
typedef struct { u32 version, state, orientation, size; } hi_popup_t;
OSStatus HIThemeDrawPopupArrow(const R *r, const hi_popup_t *info, void *c, u32 orient) {
    (void)orient;
    if (!r || !c) return paramErr;
    double a = info && info->state == 0 ? 0.4 : 1, cx = r->x + r->w / 2, cy = r->y + r->h / 2, s = r->w < r->h ? r->w / 2 : r->h / 2;
    if (info && info->orientation == 1) tri(c, cx - s, cy + s / 2, cx + s, cy + s / 2, cx, cy - s / 2, 0.2, a);   /* para cima */
    else tri(c, cx - s, cy - s / 2, cx + s, cy - s / 2, cx, cy + s / 2, 0.2, a);
    return noErr;
}
typedef struct { u32 version; i32 index; u32 state; } hi_chase_t;
OSStatus HIThemeDrawChasingArrows(const R *r, const hi_chase_t *info, void *c, u32 orient) {
    (void)orient;
    if (!r || !c) return paramErr;
    double cx = r->x + r->w / 2, cy = r->y + r->h / 2, rad = (r->w < r->h ? r->w : r->h) / 2 - 1;
    int idx = info ? info->index : 0;
    for (int i = 0; i < 12; i++) {
        double ang = (i + idx) * 3.14159265358979 / 6, g = 0.2 + 0.06 * i;
        double sx = __builtin_cos(ang), sy = __builtin_sin(ang);
        line(c, cx + sx * rad * 0.45, cy + sy * rad * 0.45, cx + sx * rad, cy + sy * rad, g > 0.9 ? 0.9 : g);
    }
    return noErr;
}
/* botões da barra de título: fechar 0 (vermelho), minimizar 1 (amarelo), zoom 2 (verde) */
typedef struct { u32 version; u32 widget_state; u16 widget_type; u16 pad; u32 window_state; u32 attributes; double title_height, title_width; } hi_widget_t;
OSStatus HIThemeDrawTitleBarWidget(const R *r, const hi_widget_t *info, void *c, u32 orient) {
    (void)orient;
    if (!r || !c) return paramErr;
    static const double col[3][3] = {{1, 0.37, 0.34}, {1, 0.74, 0.18}, {0.16, 0.79, 0.25}};
    int t = info ? info->widget_type % 3 : 0;
    double a = info && info->window_state == 0 ? 0.5 : 1;
    CGContextSetRGBFillColor(c, col[t][0], col[t][1], col[t][2], a);
    double s = r->w < r->h ? r->w : r->h;
    CGContextFillEllipseInRect(c, mkr(r->x + (r->w - s) / 2, r->y + (r->h - s) / 2, s, s));
    return noErr;
}
OSStatus HIThemeDrawWindowFrame(const R *content, const hi_widget_t *info, void *c, u32 orient, R *title) {
    (void)orient;
    if (!content || !c) return paramErr;
    double th = info && info->title_height > 0 ? info->title_height : 22;
    R bar = mkr(content->x, content->y - th, content->w, th);
    fill(c, bar, 0.93, 1);
    line(c, bar.x, content->y - 0.5, bar.x + bar.w, content->y - 0.5, 0.78);
    frame(c, mkr(content->x, content->y - th, content->w, content->h + th), 0.70);
    if (title) *title = mkr(bar.x + 70, bar.y, bar.w - 140 > 0 ? bar.w - 140 : 0, th);
    return noErr;
}
OSStatus HIThemeGetWindowShape(const R *content, const hi_widget_t *info, u32 region, id *out) {
    if (!content || !out) return paramErr;
    double th = info && info->title_height > 0 ? info->title_height : 22;
    R r = *content;
    if (region == 32 /* kWindowStructureRgn */ || region == 0 /* kWindowTitleBarRgn */) r = mkr(content->x, content->y - th, content->w, region == 0 ? th : content->h + th);
    *out = HIShapeCreateWithRect(&r);
    return noErr;
}
typedef struct { u32 version, state, kind; } hi_bg_t;
OSStatus HIThemeApplyBackground(const R *r, const hi_bg_t *info, void *c, u32 orient) { (void)info; (void)orient; if (!r || !c) return paramErr; fill(c, *r, 0.93, 1); return noErr; }
OSStatus HIThemeDrawBackground(const R *r, const hi_bg_t *info, void *c, u32 orient) {
    (void)orient;
    if (!r || !c) return paramErr;
    fill(c, *r, info && info->kind == 1 ? 0.88 : 0.93, 1);   /* kThemeBackgroundPlacard é um pouco mais escuro */
    return noErr;
}
/* botões: o fundo é o retângulo pedido; o conteúdo fica 6 por 2 para dentro */
OSStatus HIThemeGetButtonBackgroundBounds(const R *in, const void *info, R *out) { (void)info; if (!in || !out) return paramErr; *out = *in; return noErr; }
OSStatus HIThemeGetButtonContentBounds(const R *in, const void *info, R *out) {
    (void)info;
    if (!in || !out) return paramErr;
    *out = mkr(in->x + 6, in->y + 2, in->w - 12 > 0 ? in->w - 12 : 0, in->h - 4 > 0 ? in->h - 4 : 0);
    return noErr;
}
OSStatus HIThemeGetButtonShape(const R *in, const void *info, id *out) { (void)info; if (!in || !out) return paramErr; *out = HIShapeCreateWithRect(in); return noErr; }

/* ================================================================ pincéis, texto e fontes do tema */
static void brush_rgb(i16 brush, int active, double *r, double *g, double *b) {
    double v = 0.93;
    *r = *g = *b = v;
    switch (brush) {
    case -1: *r = *g = *b = 0; return;                         /* kThemeBrushBlack */
    case -2: *r = *g = *b = 1; return;                         /* kThemeBrushWhite */
    case -3: *r = 0.70; *g = 0.84; *b = 1.0; return;           /* kThemeBrushPrimaryHighlightColor */
    case -4: *r = *g = *b = 0.86; return;                      /* kThemeBrushSecondaryHighlightColor */
    case 10: case 11: case 15: case 16: *r = *g = *b = 1; return;   /* lista e documento: branco */
    case 13: case 19: case 20: *r = *g = *b = 0.80; return;    /* separadores e delimitadores */
    case 14: case 22: *r = *g = *b = 0.1; return;              /* setas */
    case 21: case 52: *r = 0.25; *g = 0.55; *b = 1; return;    /* foco */
    default: break;
    }
    if (!active) { *r = *g = *b = 0.95; }
}
OSStatus HIThemeBrushCreateCGColor(i16 brush, id *out) {
    if (!out) return paramErr;
    double r, g, b;
    brush_rgb(brush, 1, &r, &g, &b);
    *out = CGColorCreateGenericRGB(r, g, b, 1);
    return noErr;
}
OSStatus HIThemeSetFill(i16 brush, void *info, void *c, u32 orient) { (void)info; (void)orient; if (!c) return paramErr; double r, g, b; brush_rgb(brush, 1, &r, &g, &b); CGContextSetRGBFillColor(c, r, g, b, 1); return noErr; }
OSStatus HIThemeSetStroke(i16 brush, void *info, void *c, u32 orient) { (void)info; (void)orient; if (!c) return paramErr; double r, g, b; brush_rgb(brush, 1, &r, &g, &b); CGContextSetRGBStrokeColor(c, r, g, b, 1); return noErr; }
/* cores de texto: -1 preto, -2 branco; as de estado inativo/desligado ficam cinza */
OSStatus HIThemeSetTextFill(i16 color, void *info, void *c, u32 orient) {
    (void)info; (void)orient;
    if (!c) return paramErr;
    double g = color == -2 ? 1 : 0;
    if (color > 0 && (color % 3) == 2) g = 0.55;   /* as variantes Inactive/Disabled do tema */
    CGContextSetRGBFillColor(c, g, g, g, 1);
    return noErr;
}
/* ThemeFontID → CTFontUIFontType */
u32 HIThemeGetUIFontType(u16 font) {
    switch (font) {
    case 0: return 2; case 1: return 4; case 2: return 5; case 3: return 8; case 4: return 3; case 5: return 9; case 6: return 10;
    case 100: return 11; case 101: return 12; case 102: return 13; case 103: return 14; case 104: return 15; case 105: return 16;
    case 106: return 17; case 107: return 18; case 108: return 21; case 109: return 6;
    default: return 2;
    }
}
static id theme_font(u16 id_) {
    double sz = id_ == 1 || id_ == 2 ? 11 : id_ == 109 ? 9 : id_ == 107 ? 13 : 13;
    BOOL bold = id_ == 2 || id_ == 4 || id_ == 107;
    return MSG(id, (id)kls("NSFont"), bold ? "boldSystemFontOfSize:" : "systemFontOfSize:", sz);
}
/* HIThemeTextInfo: version u32, state u32, fontID u16 @8, horizontalFlushness u16, verticalFlushness u16, options u32... */
typedef struct { u32 version, state; u16 font, hflush, vflush, pad; u32 options, truncation; u32 max_lines; u8 truncated; } hi_text_t;
OSStatus HIThemeGetTextDimensions(id str, double width, const hi_text_t *info, double *out_w, double *out_h, double *out_base) {
    if (!str) return paramErr;
    id f = theme_font(info ? info->font : 0);
    id a = MSG(id, (id)kls("NSDictionary"), "dictionaryWithObject:forKey:", f, NSFontAttributeName);
    S z = ((S (*)(id, SEL, id))objc_msgSend)(str, sl("sizeWithAttributes:"), a);
    double w = z.w, h = z.h;
    if (width > 0 && w > width) { double lines = (double)(long)(w / width + 0.999); h *= lines; w = width; }
    if (out_w) *out_w = w;
    if (out_h) *out_h = h;
    if (out_base) *out_base = -(MSG0(double, f, "descender"));
    return noErr;
}
OSStatus GetThemeMenuSeparatorHeight(i16 *out) { if (!out) return paramErr; *out = 12; return noErr; }
OSStatus GetThemeMenuItemExtra(u16 type, i16 *h, i16 *w) { (void)type; if (h) *h = 4; if (w) *w = 32; return noErr; }
OSStatus CopyThemeIdentifier(id *out) { if (!out) return paramErr; *out = objc_retain(cx_str("com.apple.theme.appearance.aqua")); return noErr; }

/* ================================================================ Process Manager */
typedef struct { u32 hi, lo; } psn_t;
static i64 psn_pid(const psn_t *p) { if (!p) return -1; if (p->hi == 0 && p->lo == 2) return getpid(); return p->hi == 1 ? (i64)p->lo : -1; }
static id app_of(const psn_t *p) {
    i64 pid = psn_pid(p);
    if (pid < 0) return 0;
    return MSG(id, (id)kls("NSRunningApplication"), "runningApplicationWithProcessIdentifier:", (int)pid);
}
OSStatus GetFrontProcess(psn_t *out) {
    if (!out) return paramErr;
    id a = MSG0(id, MSG0(id, (id)kls("NSWorkspace"), "sharedWorkspace"), "frontmostApplication");
    if (!a) return procNotFound;
    out->hi = 1; out->lo = (u32)MSG0(int, a, "processIdentifier");
    return noErr;
}
OSStatus SameProcess(const psn_t *a, const psn_t *b, u8 *out) {
    if (!a || !b || !out) return paramErr;
    i64 pa = psn_pid(a), pb = psn_pid(b);
    if (pa < 0 || pb < 0) return procNotFound;
    *out = pa == pb;
    return noErr;
}
OSStatus CopyProcessName(const psn_t *p, id *out) {
    if (!out) return paramErr;
    id a = app_of(p);
    if (!a) return procNotFound;
    id n = MSG0(id, a, "localizedName");
    *out = n ? objc_retain(n) : objc_retain(cx_str(""));
    return noErr;
}
typedef struct { u8 b[80]; } FSRef;
OSStatus FSPathMakeRef(const u8 *path, FSRef *ref, u8 *is_dir);
OSStatus GetProcessBundleLocation(const psn_t *p, FSRef *out) {
    if (!out) return paramErr;
    id a = app_of(p);
    id url = a ? MSG0(id, a, "bundleURL") : 0;
    if (!url) url = a ? MSG0(id, a, "executableURL") : 0;
    if (!url) return procNotFound;
    return FSPathMakeRef((const u8 *)cx_cstr(MSG0(id, url, "path")), out, 0);
}
OSStatus KillProcess(const psn_t *p) {
    i64 pid = psn_pid(p);
    if (pid <= 0) return procNotFound;
    return kill((int)pid, 9) == 0 ? noErr : procNotFound;
}
OSStatus WakeUpProcess(const psn_t *p) { return app_of(p) ? noErr : procNotFound; }   /* processos do mOS não dormem à espera do Process Manager */

/* ================================================================ Dock e cursor */
/* o Dock é do window server: as preferências passam por GUI_DEVICE/DEV_DOCK (o desktop grava no disco) */
u8 CoreDockGetAutoHideEnabled(void) { return SYS3(GUI_DEVICE, DEV_DOCK, DK_AUTOHIDE, (u64)-1) == 1; }
void CoreDockSetAutoHideEnabled(u8 on) { SYS3(GUI_DEVICE, DEV_DOCK, DK_AUTOHIDE, on ? 1 : 0); }
u8 CoreDockIsMagnificationEnabled(void) { return SYS3(GUI_DEVICE, DEV_DOCK, DK_MAGNIFY, (u64)-1) == 1; }
void CoreDockSetMagnificationEnabled(u8 on) { SYS3(GUI_DEVICE, DEV_DOCK, DK_MAGNIFY, on ? 1 : 0); }
/* o Dock do mOS fica sempre embaixo e centrado: kCoreDockOrientationBottom (2), kCoreDockPinningMiddle (2) */
void CoreDockGetOrientationAndPinning(i32 *orient, i32 *pin) { if (orient) *orient = 2; if (pin) *pin = 2; }
OSStatus CoreDockSendNotification(id name, void *arg) {
    (void)arg;
    if (!name) return paramErr;
    MSG(void, MSG0(id, (id)kls("NSDistributedNotificationCenter"), "defaultCenter"), "postNotificationName:object:", name, (id)0);
    return noErr;
}
/* CoreCursorSet(conexão, id do cursor): os ids do sistema para os cursores do NSCursor */
OSStatus CoreCursorSet(int conn, int cid) {
    (void)conn;
    static const char *const by_id[] = {"arrowCursor", "IBeamCursor", "crosshairCursor", "closedHandCursor", "openHandCursor",
                                        "pointingHandCursor", "resizeLeftCursor", "resizeRightCursor", "resizeLeftRightCursor",
                                        "resizeUpCursor", "resizeDownCursor", "resizeUpDownCursor", "disappearingItemCursor",
                                        "operationNotAllowedCursor", "dragLinkCursor", "dragCopyCursor", "contextualMenuCursor"};
    if (cid < 0 || (u32)cid >= sizeof by_id / sizeof by_id[0]) return paramErr;
    MSG0(void, MSG0(id, (id)kls("NSCursor"), by_id[cid]), "set");
    return noErr;
}
OSStatus CoreCursorUnregisterAll(int conn) { (void)conn; return noErr; }

/* ================================================================ acessibilidade */
extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } cfs_t;
#define AXK(var, text) static cfs_t var##_s = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; const void *var = &var##_s;
AXK(kAXAttachmentTextAttribute, "AXAttachment") AXK(kAXAutocorrectedTextAttribute, "AXAutocorrected")
AXK(kAXBackgroundColorTextAttribute, "AXBackgroundColor") AXK(kAXFontTextAttribute, "AXFont")
AXK(kAXForegroundColorTextAttribute, "AXForegroundColor") AXK(kAXLinkTextAttribute, "AXLink")
AXK(kAXListItemIndexTextAttribute, "AXListItemIndex") AXK(kAXListItemLevelTextAttribute, "AXListItemLevel")
AXK(kAXListItemPrefixTextAttribute, "AXListItemPrefix") AXK(kAXMarkedMisspelledTextAttribute, "AXMarkedMisspelled")
AXK(kAXMisspelledTextAttribute, "AXMisspelled") AXK(kAXNaturalLanguageTextAttribute, "AXNaturalLanguageText")
AXK(kAXReplacementStringTextAttribute, "AXReplacementString") AXK(kAXShadowTextAttribute, "AXShadow")
AXK(kAXStrikethroughColorTextAttribute, "AXStrikethroughColor") AXK(kAXStrikethroughTextAttribute, "AXStrikethrough")
AXK(kAXSuperscriptTextAttribute, "AXSuperscript") AXK(kAXUnderlineColorTextAttribute, "AXUnderlineColor")
AXK(kAXUnderlineTextAttribute, "AXUnderline")
AXK(kTISPropertyInputModeID, "TISPropertyInputModeID") AXK(kTISPropertyInputSourceIsEnableCapable, "TISPropertyInputSourceIsEnableCapable")
AXK(kTISPropertyIconRef, "TISPropertyIconRef") AXK(kTISPropertyKeyLayoutName, "TISPropertyKeyLayoutName")
AXK(kTISTypeKeyboardInputMethodModeEnabled, "TISTypeKeyboardInputMethodModeEnabled")
u8 AXAPIEnabled(void) { return 1; }   /* desde o 10.9 é sempre ligado; a permissão por app é o AXIsProcessTrusted */
u64 AXObserverGetTypeID(void) { return 0x4158; }
int cx_ax_element_pid(id elem);
void cx_post_key_event(u16 mac, u64 flags, int down);
/* tecla no app do elemento: no próprio processo entra na fila do AppKit; em outro, kAXErrorCannotComplete */
i32 AXUIElementPostKeyboardEvent(id app, u16 ch, u16 vk, u8 down) {
    (void)ch;
    int pid = app && responds(app, "processIdentifier") ? MSG0(int, app, "processIdentifier") : -1;
    if (pid < 0 && app) { extern i32 AXUIElementGetPid(id e, int *pid); int p = -1; if (AXUIElementGetPid(app, &p) == 0) pid = p; }
    if (pid != getpid() && pid != -1) return -25204;
    cx_post_key_event(vk, 0, down);
    return 0;
}

/* ================================================================ teclado e mouse */
extern int cx_carbon_key_down(u16 code);   /* carbon_events.c: estado das teclas */
void GetKeys(u32 map[4]) {
    if (!map) return;
    map[0] = map[1] = map[2] = map[3] = 0;
    for (u16 k = 0; k < 128; k++) if (cx_carbon_key_down(k)) {
        /* KeyMap: big-endian por UInt32; a tecla k é o bit (k % 32) do byte certo */
        u8 *b = (u8 *)map;
        b[k >> 3] |= (u8)(1u << (k & 7));
    }
}
u8 Button(void) { return (MSG0(u64, (id)kls("NSEvent"), "pressedMouseButtons") & 1) != 0; }
/* espaços: 1 global de 72 dpi (origem no alto à esquerda da tela principal), 2 pixels da tela; janelas e views do
 * Carbon (HIWindowRef/HIViewRef) não existem no mOS */
static double screen_h(void) { gui_wininfo_t i = {1440, 876, 0, 0}; SYS1(GUI_SCREEN, &i); return i.h + 24; }
OSStatus HIGetMousePosition(u32 space, void *obj, P *out) {
    (void)obj;
    if (!out) return paramErr;
    if (space != 1 && space != 2) return paramErr;
    P m = ((P (*)(id, SEL))objc_msgSend)((id)kls("NSEvent"), sl("mouseLocation"));
    out->x = m.x; out->y = screen_h() - m.y;
    return noErr;
}
OSStatus HIPointConvert(P *p, u32 from, void *fo, u32 to, void *tobj) {
    (void)fo; (void)tobj;
    if (!p) return paramErr;
    if ((from != 1 && from != 2) || (to != 1 && to != 2)) return paramErr;
    return noErr;   /* escala 1: global e pixels coincidem */
}
OSStatus HIRectConvert(R *r, u32 from, void *fo, u32 to, void *tobj) { if (!r) return paramErr; P p = {r->x, r->y}; return HIPointConvert(&p, from, fo, to, tobj); }
/* kMouseParamsSticky / kMouseParamsDragInitiation / kMouseParamsClickAndHold / kMouseParamsProxyIcon */
OSStatus HIMouseTrackingGetParameters(u32 selector, double *time, double *dist) {
    double t = 0.5, d = 4;
    if (selector == 'clhd') t = 0.6;
    if (time) *time = t;
    if (dist) *dist = d;
    return noErr;
}
u32 GetApplicationTextEncoding(void) { return 0x08000100; }   /* kCFStringEncodingUTF8 */
OSStatus TISRegisterInputSource(id url) { return url ? noErr : paramErr; }

/* ================================================================ Apple Help e Internet Config */
static id help_book_folder(id book) {
    id mb = MSG0(id, (id)kls("NSBundle"), "mainBundle");
    id folder = MSG(id, mb, "objectForInfoDictionaryKey:", cx_str("CFBundleHelpBookFolder"));
    (void)book;
    return folder ? MSG(id, MSG0(id, mb, "resourcePath"), "stringByAppendingPathComponent:", folder) : 0;
}
OSStatus AHGotoPage(id book, id path, id anchor) {
    id dir = help_book_folder(book);
    if (!dir) return -50;
    id p = MSG(id, dir, "stringByAppendingPathComponent:", path ? path : cx_str("index.html"));
    id url = MSG(id, (id)kls("NSURL"), "fileURLWithPath:", p);
    if (anchor) { id s = MSG(id, MSG0(id, url, "absoluteString"), "stringByAppendingFormat:", cx_str("#%@"), anchor); url = MSG(id, (id)kls("NSURL"), "URLWithString:", s); }
    return MSG(BOOL, MSG0(id, (id)kls("NSWorkspace"), "sharedWorkspace"), "openURL:", url) ? noErr : -50;
}
OSStatus AHLookupAnchor(id book, id anchor) { return AHGotoPage(book, 0, anchor); }
/* Internet Config: as preferências antigas do ICPrefs não existem no mOS; o que o app pede volta icPrefNotFoundErr */
typedef struct { u32 creator; } ic_inst;
OSStatus ICStart(ic_inst **out, u32 creator) { if (!out) return paramErr; *out = calloc(1, sizeof **out); if (!*out) return -108; (*out)->creator = creator; return noErr; }
OSStatus ICStop(ic_inst *i) { free(i); return noErr; }
OSStatus ICGetPref(ic_inst *i, const u8 *key, i32 *attr, void *buf, i32 *size) { (void)i; (void)key; (void)buf; if (attr) *attr = 0; if (size) *size = 0; return -666; }
OSStatus ICMapFilename(ic_inst *i, const u8 *name, void *entry) { (void)i; (void)name; (void)entry; return -666; }

/* ================================================================ interação dos Apple Events */
static i32 g_interaction = 2;   /* kAEInteractWithAll */
OSStatus AEGetInteractionAllowed(i32 *out) { if (!out) return paramErr; *out = g_interaction; return noErr; }
OSStatus AESetInteractionAllowed(i32 level) { g_interaction = level; return noErr; }
/* AEInteractWithUser: o app vem para a frente (no mOS não há a notificação de pedir atenção do Carbon) */
OSStatus AEInteractWithUser(i32 timeout, void *nm, void *idle) {
    (void)timeout; (void)nm; (void)idle;
    extern id NSApp;
    if (g_interaction == 0 /* kAEInteractWithSelf */) return -1713;   /* errAENoUserInteraction */
    MSG(void, NSApp, "activateIgnoringOtherApps:", (BOOL)YES);
    return noErr;
}
OSStatus AEResetTimer(const void *reply) { (void)reply; return noErr; }   /* a resposta espera sem prazo fixo no mOS */
OSStatus AEProcessAppleEvent(const void *event_record) { (void)event_record; return -1708; }   /* errAEEventNotHandled: EventRecord do Mac clássico */

/* ================================================================ menus, busca, ícones e Launch */
/* MenuTrackingData: menu, itemSelected e itemUnderMouse (1 em diante; 0 nenhum), itemRect (Rect: topo, esquerda, base,
 * direita, em coordenadas globais), virtualMenuTop/Bottom. O MenuRef do mOS é o próprio NSMenu. */
typedef struct { id menu; u16 selected, under_mouse; i16 top, left, bottom, right; i32 vtop, vbottom; } menu_tracking_t;
int cx_menu_tracking_info(id *menu, int *hl, double frame[4], double *item_top, double *item_h);
OSStatus GetMenuTrackingData(id menu, menu_tracking_t *out) {
    id m = 0;
    int hl = -1;
    double f[4], it = 0, ih = 0;
    if (!cx_menu_tracking_info(&m, &hl, f, &it, &ih) || (menu && menu != m)) return -5623;   /* menuNotFoundErr */
    if (!out) return paramErr;
    double top = screen_h() - (f[1] + f[3]);
    out->menu = m;
    out->selected = out->under_mouse = (u16)(hl >= 0 ? hl + 1 : 0);
    out->top = (i16)(top + it); out->bottom = (i16)(top + it + ih);
    out->left = (i16)f[0]; out->right = (i16)(f[0] + f[2]);
    out->vtop = (i32)top; out->vbottom = (i32)(top + f[3]);
    return noErr;
}
/* a janela de busca do sistema (Spotlight do mOS) com o texto */
#include "../../kernel/include/syscall.h"
OSStatus HISearchWindowShow(id text, u32 flags) {
    (void)flags;
    const char *q = text ? cx_cstr(text) : "";
    return SYS2(SYS_DESKTOP, MOS_DESK_SEARCH, q) == 0 ? noErr : paramErr;
}
/* IconFamilyHandle: troca ou lê um elemento dos dados 'icns' (carbon_core.c, teste em carbon_host.c) */
typedef char **Handle;
Handle NewHandle(long size);
long GetHandleSize(Handle h);
void SetHandleSize(Handle h, long size);
OSStatus SetIconFamilyData(Handle fam, u32 type, Handle data) {
    if (!fam || !data) return paramErr;
    long n = GetHandleSize(fam), dn = GetHandleSize(data);
    u8 *tmp = malloc((usize)(n + dn + 16));
    if (!tmp) return -108;
    long w = cc_icns_set((const u8 *)*fam, n, type, (const u8 *)*data, dn, tmp);
    if (w < 0) { free(tmp); return paramErr; }
    SetHandleSize(fam, w);
    if (GetHandleSize(fam) != w) { free(tmp); return -108; }
    memcpy(*fam, tmp, (usize)w);
    free(tmp);
    return noErr;
}
OSStatus GetIconFamilyData(Handle fam, u32 type, Handle data) {
    if (!fam || !data) return paramErr;
    long off = 0, len = 0;
    if (!cc_icns_find((const u8 *)*fam, GetHandleSize(fam), type, &off, &len)) { SetHandleSize(data, 0); return noErr; }   /* sem esse tipo: dados vazios */
    SetHandleSize(data, len);
    if (GetHandleSize(data) != len) return -108;
    memcpy(*data, *fam + off, (usize)len);
    return noErr;
}
/* LaunchApplication: LaunchParamBlockRec (pack 2) com o FSRef do app; devolve o PSN do processo aberto */
typedef struct __attribute__((packed, aligned(2))) {
    u32 reserved1; u16 reserved2, block_id; u32 epb_len; u16 file_flags, control_flags;
    const FSRef *app; psn_t psn; u32 preferred, minimum, available; void *params;
} launch_pb_t;
int FSRefMakePath(const FSRef *ref, u8 *path, u32 max);
OSStatus LaunchApplication(launch_pb_t *pb) {
    if (!pb || !pb->app) return paramErr;
    char path[1024];
    if (FSRefMakePath(pb->app, (u8 *)path, sizeof path)) return -43;   /* fnfErr */
    id url = MSG(id, (id)kls("NSURL"), "fileURLWithPath:", cx_str(path));
    id ws = MSG0(id, (id)kls("NSWorkspace"), "sharedWorkspace");
    /* launchDontSwitch (0x0200) → NSWorkspaceLaunchWithoutActivation; launchContinue e os tamanhos de memória do Mac
     * clássico não valem mais (como no macOS: quem chama segue rodando) */
    u64 opts = (pb->control_flags & 0x0200) ? 0x200 : 0;
    id err = 0;
    id app = ((id (*)(id, SEL, id, u64, id, id *))objc_msgSend)(ws, sl("launchApplicationAtURL:options:configuration:error:"), url, opts, (id)0, &err);
    if (!app) return -10810;   /* kLSUnknownErr / procNotFound */
    pb->psn.hi = 1; pb->psn.lo = (u32)MSG0(int, app, "processIdentifier");
    return noErr;
}
