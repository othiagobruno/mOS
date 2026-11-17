/*
 * mOS libSystem — JavaRuntimeSupport.framework: JRSUI (controles nativos do Aqua Look and Feel do JDK) e JRSFont
 * (estilo de desenho de texto do Java2D).
 *
 * O JDK (libosxui, JRSUIConstantSync.m) muda o controle por setters com os valores das constantes Java
 * (apple.laf.JRSUIConstants: Widget 1..71, State ativo 1/inativo 2/desabilitado 3/pressionado 4..., Size, Direction,
 * Orientation, ScrollBarPart...) e por JRSUIControlSetValueByKey com chaves de JRSUIGetKey(número). As chaves de
 * número conhecido pelo lado Java são value 20, animationFrame 23, thumbProportion 24, thumbStart 25,
 * windowTitleBarHeight 28 e segmentLeadingSeparator 29; as outras são guardadas e devolvidas, sem efeito no desenho.
 * O desenho é o do tema do mOS (HITheme em appkit_carbon.c/appkit_carbon2.c; geometria das trilhas em carbon_core.c).
 * Barra de rolagem: thumbStart = valor/(máx−mín) e thumbProportion = visível/(máx−mín), como o AquaScrollBarUI manda.
 */
#include "foundation.h"
#include "libc.h"

typedef struct { double x, y, w, h; } R;
typedef struct { double x, y; } P;
typedef struct { double a, b, c, d, tx, ty; } AT;
typedef int OSStatus;
id objc_retain(id o);
void objc_release(id o);

/* HITheme do mOS */
typedef struct { u32 version; u32 state; u32 kind; u32 value; u32 adornment; } hi_button_t;
OSStatus HIThemeDrawButton(const R *bounds, const hi_button_t *info, id cgctx, u32 orient, R *label);
typedef struct {
    u32 version; u16 kind; u16 pad; R bounds; i32 min, max, value; u32 reserved; u16 attributes; u8 enable, filler;
    union { struct { i32 viewsize; u8 press; } scroll; struct { u8 dir, press; } slider; struct { u8 phase; } progress; } info;
} track_info;
OSStatus HIThemeDrawTrack(const track_info *t, const R *ghost, void *c, u32 orient);
typedef struct { u32 version; u32 style, direction, size, adornment, kind, position; } hi_tab_t;
OSStatus HIThemeDrawTab(const R *r, const hi_tab_t *info, void *c, u32 orient, R *label);
typedef struct { u32 version, state, orientation, size; } hi_popup_t;
OSStatus HIThemeDrawPopupArrow(const R *r, const hi_popup_t *info, void *c, u32 orient);
OSStatus HIThemeDrawFocusRect(const R *r, u8 has_focus, void *c, u32 orient);
OSStatus HIThemeDrawSeparator(const R *r, const void *info, void *c, u32 orient);
OSStatus HIThemeDrawPaneSplitter(const R *r, const void *info, void *c, u32 orient);
typedef struct { u32 version, state, kind, direction, size; } hi_grow_t;
OSStatus HIThemeDrawGrowBox(const P *origin, const hi_grow_t *info, void *c, u32 orient);
OSStatus HIThemeDrawPlacard(const R *r, const void *info, void *c, u32 orient);
typedef struct { u32 version, state, kind; } hi_bg_t;
OSStatus HIThemeDrawBackground(const R *r, const hi_bg_t *info, void *c, u32 orient);
OSStatus HIThemeDrawGroupBox(const R *r, const void *info, void *c, u32 orient);
OSStatus HIThemeDrawMenuBarBackground(const R *r, const void *info, void *c, u32 orient);
typedef struct { u32 version; i32 index; u32 state; } hi_chase_t;
OSStatus HIThemeDrawChasingArrows(const R *r, const hi_chase_t *info, void *c, u32 orient);
typedef struct { u32 version; u32 widget_state; u16 widget_type; u16 pad; u32 window_state; u32 attributes; double title_height, title_width; } hi_widget_t;
OSStatus HIThemeDrawTitleBarWidget(const R *r, const hi_widget_t *info, void *c, u32 orient);
OSStatus HIThemeDrawWindowFrame(const R *content, const hi_widget_t *info, void *c, u32 orient, R *title);
void CGContextSetRGBFillColor(void *c, double r, double g, double b, double a);
void CGContextSetRGBStrokeColor(void *c, double r, double g, double b, double a);
void CGContextFillRect(void *c, R r);
void CGContextStrokeRectWithWidth(void *c, R r, double w);
void CGContextFillEllipseInRect(void *c, R r);
void CGContextBeginPath(void *c);
void CGContextMoveToPoint(void *c, double x, double y);
void CGContextAddLineToPoint(void *c, double x, double y);
void CGContextClosePath(void *c);
void CGContextFillPath(void *c);
void CGContextStrokePath(void *c);
void CGContextSetLineWidth(void *c, double w);
void CGContextSetShouldAntialias(void *c, int v);
void CGContextSetShouldSmoothFonts(void *c, u8 v);
void CGContextSetAllowsFontSubpixelPositioning(void *c, u8 v);
void CGContextSetShouldSubpixelPositionFonts(void *c, u8 v);
typedef struct { i64 loc, len; } cfrange;
id CTFontCreateForString(id font, id string, cfrange range);
void CTFontGetBoundingRectsForGlyphs(id font, u32 orientation, const u16 *glyphs, R *rects, i64 count);
id CFStringCreateWithCharacters(void *alloc, const u16 *chars, i64 n);

/* ================================================================ chaves */
#define NKEYS 64
static id g_keys[NKEYS];
id JRSUIGetKey(i64 n) {
    if (n < 0 || n >= NKEYS) return 0;
    if (!g_keys[n]) {
        char b[32];
        snprintf(b, sizeof b, "JRSUIKey%lld", (long long)n);
        g_keys[n] = objc_retain(cx_str(b));
    }
    return g_keys[n];
}
static int key_num(id k) { for (int i = 0; i < NKEYS; i++) if (g_keys[i] && g_keys[i] == k) return i; return -1; }
enum { KEY_VALUE = 20, KEY_ANIM_FRAME = 23, KEY_THUMB_PROP = 24, KEY_THUMB_START = 25, KEY_TITLE_H = 28 };

/* ================================================================ renderizador e controle */
typedef struct { int dummy; } jrs_renderer;
typedef struct {
    int flipped, widget, state, size, direction, orientation, valign, halign, segment, scroll_part, variant, window_type, show_arrows, animating;
    double numbers[NKEYS]; u8 has[NKEYS];
    id others;   /* chaves sem efeito no desenho: guardadas */
} jrs_control;
void *JRSUIRendererCreate(void) { return calloc(1, sizeof(jrs_renderer)); }
void JRSUIRendererRelease(void *r) { free(r); }
jrs_control *JRSUIControlCreate(u8 flipped) {
    jrs_control *c = calloc(1, sizeof *c);
    if (c) { c->flipped = flipped; c->state = 1; c->size = 3; c->orientation = 1; }
    return c;
}
void JRSUIControlRelease(jrs_control *c) { if (!c) return; if (c->others) objc_release(c->others); free(c); }
void JRSUIControlSetWidget(jrs_control *c, i64 v) { if (c) c->widget = (int)v; }
void JRSUIControlSetState(jrs_control *c, i64 v) { if (c) c->state = (int)v; }
void JRSUIControlSetSize(jrs_control *c, i64 v) { if (c) c->size = (int)v; }
void JRSUIControlSetDirection(jrs_control *c, i64 v) { if (c) c->direction = (int)v; }
void JRSUIControlSetOrientation(jrs_control *c, i64 v) { if (c) c->orientation = (int)v; }
void JRSUIControlSetAlignmentVertical(jrs_control *c, i64 v) { if (c) c->valign = (int)v; }
void JRSUIControlSetAlignmentHorizontal(jrs_control *c, i64 v) { if (c) c->halign = (int)v; }
void JRSUIControlSetSegmentPosition(jrs_control *c, i64 v) { if (c) c->segment = (int)v; }
void JRSUIControlSetScrollBarPart(jrs_control *c, i64 v) { if (c) c->scroll_part = (int)v; }
void JRSUIControlSetVariant(jrs_control *c, i64 v) { if (c) c->variant = (int)v; }
void JRSUIControlSetWindowType(jrs_control *c, i64 v) { if (c) c->window_type = (int)v; }
void JRSUIControlSetShowArrows(jrs_control *c, u8 v) { if (c) c->show_arrows = v; }
void JRSUIControlSetAnimating(jrs_control *c, u8 v) { if (c) c->animating = v; }
void JRSUIControlSetValueByKey(jrs_control *c, id key, id value) {
    if (!c || !key) return;
    int k = key_num(key);
    if (k >= 0 && value && cx_responds(object_getClass(value), sel_registerName("doubleValue"))) {
        c->numbers[k] = MSG0(double, value, "doubleValue");
        c->has[k] = 1;
        return;
    }
    if (!c->others) c->others = MSG0(id, MSG0(id, cx_class_named("NSMutableDictionary"), "alloc"), "init");
    if (value) MSG(void, c->others, "setObject:forKey:", value, key); else MSG(void, c->others, "removeObjectForKey:", key);
}
static double num(jrs_control *c, int k, double def) { return c->has[k] ? c->numbers[k] : def; }

/* ---- geometria da barra de rolagem */
static int horiz(jrs_control *c, R f) { return c->orientation == 1 ? 1 : c->orientation == 2 ? 0 : f.w >= f.h; }
static void scroll_thumb(jrs_control *c, R f, double *pos, double *len) {
    double L = horiz(c, f) ? f.w : f.h;
    double prop = num(c, KEY_THUMB_PROP, 1), start = num(c, KEY_THUMB_START, 0);
    double tl = prop * L;
    if (tl < 16) tl = 16;
    if (tl > L) tl = L;
    double p = start * L;
    if (prop < 1 && 1 - prop > 0) p = start / (1 - prop) * (L - tl);   /* start vai de 0 a 1−prop */
    if (p < 0) p = 0;
    if (p > L - tl) p = L - tl;
    *pos = p; *len = tl;
}
/* ScrollBarPart do Java: 1 nenhuma, 2 polegar, 3/4 setas, 7 trilha antes, 8 trilha depois */
R JRSUIControlGetScrollBarPartBounds(jrs_control *c, R f, i64 part) {
    R z = {0, 0, 0, 0};
    if (!c) return z;
    double p, l;
    scroll_thumb(c, f, &p, &l);
    int h = horiz(c, f);
    switch (part) {
    case 2: return h ? (R){f.x + p, f.y, l, f.h} : (R){f.x, f.y + p, f.w, l};
    case 7: return h ? (R){f.x, f.y, p, f.h} : (R){f.x, f.y, f.w, p};
    case 8: return h ? (R){f.x + p + l, f.y, f.w - p - l, f.h} : (R){f.x, f.y + p + l, f.w, f.h - p - l};
    default: return z;   /* o tema do mOS não tem setas na barra */
    }
}
/* Hit do Java: 0 nenhum, 1 acertou, 2 polegar, 3 trilha antes, 4 trilha depois */
i64 JRSUIControlGetHitPart(void *r, jrs_control *c, R f, P pt) {
    (void)r;
    if (!c || pt.x < f.x || pt.y < f.y || pt.x > f.x + f.w || pt.y > f.y + f.h) return 0;
    if (c->widget != 60) return 1;
    double p, l, at = horiz(c, f) ? pt.x - f.x : pt.y - f.y;
    scroll_thumb(c, f, &p, &l);
    return at < p ? 3 : at > p + l ? 4 : 2;
}
/* fração da área rolável que um arrasto de offset pixels anda (AquaScrollBarUI.getValueFromOffset) */
double JRSUIControlGetScrollBarOffsetFor(jrs_control *c, R f, i64 offset, i64 visible, i64 extent) {
    if (!c || extent <= 0) return 0;
    double L = horiz(c, f) ? f.w : f.h, tl = (double)visible / (double)extent * L;
    if (tl < 16) tl = 16;
    if (L - tl <= 0) return 0;
    return (double)offset / (L - tl);
}
/* clicar na trilha pula para o ponto (preferência AppleScrollerPagingBehavior do sistema) */
u8 JRSUIControlShouldScrollToClick(void) {
    id ud = MSG0(id, cx_class_named("NSUserDefaults"), "standardUserDefaults");
    return ud && MSG(BOOL, ud, "boolForKey:", cx_str("AppleScrollerPagingBehavior"));
}

/* ================================================================ desenho */
static u32 hi_state(jrs_control *c) { return c->state == 4 ? 2 : (c->state == 2 || c->state == 3) ? 0 : 1; }
static void triangle(void *g, R r, int open, int enabled) {
    double cx = r.x + r.w / 2, cy = r.y + r.h / 2, s = (r.w < r.h ? r.w : r.h) * 0.3;
    CGContextSetRGBFillColor(g, 0.35, 0.35, 0.37, enabled ? 1 : 0.4);
    CGContextBeginPath(g);
    if (open) { CGContextMoveToPoint(g, cx - s, cy - s / 2); CGContextAddLineToPoint(g, cx + s, cy - s / 2); CGContextAddLineToPoint(g, cx, cy + s / 2); }
    else { CGContextMoveToPoint(g, cx - s / 2, cy - s); CGContextAddLineToPoint(g, cx - s / 2, cy + s); CGContextAddLineToPoint(g, cx + s / 2, cy); }
    CGContextClosePath(g);
    CGContextFillPath(g);
}
static void field(void *g, R r) {
    CGContextSetRGBFillColor(g, 1, 1, 1, 1);
    CGContextFillRect(g, r);
    CGContextSetRGBStrokeColor(g, 0.70, 0.70, 0.72, 1);
    CGContextStrokeRectWithWidth(g, r, 1);
}
void JRSUIControlDraw(void *renderer, jrs_control *c, void *g, R f) {
    (void)renderer;
    if (!c || !g) return;
    u32 st = hi_state(c);
    double value = num(c, KEY_VALUE, 0);
    int w = c->widget;
    if (w == 1 || w == 51) { hi_bg_t b = {0, st, w == 51 ? 1u : 0u}; HIThemeDrawBackground(&f, &b, g, 0); }
    else if (w == 5 || w == 23) { hi_button_t b = {0, st, w == 5 ? 1u : 2u, (u32)(value >= 2 ? 2 : value >= 1 ? 1 : 0), 0}; HIThemeDrawButton(&f, &b, (id)g, 0, 0); }
    else if (w == 6 || w == 7 || (w >= 11 && w <= 16)) {
        hi_button_t b = {0, st, 0, 0, 0};
        HIThemeDrawButton(&f, &b, (id)g, 0, 0);
        R ar = {f.x + f.w - f.h, f.y, f.h, f.h};
        hi_popup_t pa = {0, st, 0, 0};
        HIThemeDrawPopupArrow(&ar, &pa, g, 0);
    } else if (w == 8 || w == 37) triangle(g, f, value >= 1, st != 0);
    else if (w == 9 || w == 45) HIThemeDrawPlacard(&f, 0, g, 0);
    else if (w == 10) {   /* setinhas (stepper) */
        hi_button_t b = {0, st, 0, 0, 0};
        HIThemeDrawButton(&f, &b, (id)g, 0, 0);
        R up = {f.x, f.y + f.h / 2, f.w, f.h / 2}, dn = {f.x, f.y, f.w, f.h / 2};
        hi_popup_t u = {0, st, 1, 0}, d = {0, st, 0, 0};
        HIThemeDrawPopupArrow(c->flipped ? &dn : &up, &u, g, 0);
        HIThemeDrawPopupArrow(c->flipped ? &up : &dn, &d, g, 0);
    } else if (w == 28 || w == 29) {   /* botões do campo de busca: limpar (círculo com x) e lupa */
        CGContextSetRGBStrokeColor(g, 0.5, 0.5, 0.52, st ? 1 : 0.5);
        CGContextSetLineWidth(g, 1.5);
        double s = (f.w < f.h ? f.w : f.h) * 0.3, cx = f.x + f.w / 2, cy = f.y + f.h / 2;
        CGContextBeginPath(g);
        if (w == 28) { CGContextMoveToPoint(g, cx - s, cy - s); CGContextAddLineToPoint(g, cx + s, cy + s); CGContextMoveToPoint(g, cx + s, cy - s); CGContextAddLineToPoint(g, cx - s, cy + s); }
        else { CGContextMoveToPoint(g, cx + s * 0.5, cy + s * 0.5); CGContextAddLineToPoint(g, cx + s * 1.3, cy + s * 1.3); }
        CGContextStrokePath(g);
        if (w == 29) { CGContextSetRGBStrokeColor(g, 0.5, 0.5, 0.52, 1); CGContextStrokeRectWithWidth(g, (R){cx - s, cy - s, 1.5 * s, 1.5 * s}, 1.5); }
    } else if (w == 36) { CGContextSetRGBFillColor(g, 0.9, 0.9, 0.92, 1); CGContextFillEllipseInRect(g, f); }
    else if (w == 38 || w == 40) HIThemeDrawPaneSplitter(&f, 0, g, 0);
    else if (w == 39 || w == 61) HIThemeDrawSeparator(&f, 0, g, 0);
    else if (w == 41) HIThemeDrawFocusRect(&f, 1, g, 0);
    else if (w == 42 || w == 43) HIThemeDrawGroupBox(&f, 0, g, 0);
    else if (w == 44 || w == 46 || w == 47 || w == 48 || w == 70) field(g, f);
    else if (w == 49 || w == 50) { P o = {f.x + f.w, c->flipped ? f.y + f.h : f.y + f.h}; hi_grow_t gb = {0, st, 0, 0, c->size <= 2 ? 1u : 0u}; HIThemeDrawGrowBox(&o, &gb, g, 0); }
    else if (w == 52 || w == 55) { CGContextSetRGBFillColor(g, 0.97, 0.97, 0.98, 0.98); CGContextFillRect(g, f); }
    else if (w == 53) {
        if (c->state == 4 || c->state == 6) { CGContextSetRGBFillColor(g, 0.2, 0.5, 0.95, 1); CGContextFillRect(g, f); }
        else { CGContextSetRGBFillColor(g, 0.97, 0.97, 0.98, 0.98); CGContextFillRect(g, f); }
    } else if (w == 54) HIThemeDrawMenuBarBackground(&f, 0, g, 0);
    else if (w >= 56 && w <= 58) {
        track_info t;
        memset(&t, 0, sizeof t);
        t.kind = w == 57 ? 4 : 3;
        t.bounds = f; t.min = 0; t.max = 1000; t.value = (i32)(value * 1000);
        t.attributes = (u16)(horiz(c, f) ? 1 : 0);
        t.enable = st ? 1 : 0;
        t.info.progress.phase = (u8)num(c, KEY_ANIM_FRAME, 0);
        HIThemeDrawTrack(&t, 0, g, 0);
    } else if (w == 59 || w == 64) { hi_chase_t ch = {0, (i32)num(c, KEY_ANIM_FRAME, 0), st}; HIThemeDrawChasingArrows(&f, &ch, g, 0); }
    else if (w == 60) {
        CGContextSetRGBFillColor(g, 0.97, 0.97, 0.97, 1);
        CGContextFillRect(g, f);
        R th = JRSUIControlGetScrollBarPartBounds(c, f, 2);
        int h = horiz(c, f);
        R in = h ? (R){th.x + 2, th.y + 3, th.w - 4, th.h - 6} : (R){th.x + 3, th.y + 2, th.w - 6, th.h - 4};
        CGContextSetRGBFillColor(g, c->state == 4 ? 0.45 : 0.62, c->state == 4 ? 0.45 : 0.62, c->state == 4 ? 0.45 : 0.62, st ? 1 : 0.5);
        CGContextFillRect(g, in);
    } else if (w == 62) {
        track_info t;
        memset(&t, 0, sizeof t);
        t.kind = 2; t.bounds = f; t.min = 0; t.max = 1000; t.value = (i32)(value * 1000);
        t.attributes = (u16)((horiz(c, f) ? 1 : 0) | 4);
        t.enable = st ? 1 : 0;
        HIThemeDrawTrack(&t, 0, g, 0);
    } else if (w == 63) {
        double s = f.w < f.h ? f.w : f.h;
        CGContextSetRGBFillColor(g, 1, 1, 1, 1);
        CGContextFillEllipseInRect(g, (R){f.x + (f.w - s) / 2, f.y + (f.h - s) / 2, s, s});
    } else if (w == 65) {
        hi_tab_t tb = {0, c->state == 4 ? 2u : value >= 1 ? 0u : 1u, 0, 0, 0, 0, 0};
        HIThemeDrawTab(&f, &tb, g, 0, 0);
    } else if (w >= 66 && w <= 68) {
        hi_widget_t wi = {0, st, (u16)(w - 66), 0, st ? 1u : 0u, 0, 0, 0};
        HIThemeDrawTitleBarWidget(&f, &wi, g, 0);
    } else if (w == 71) {
        double th = num(c, KEY_TITLE_H, 22);
        hi_widget_t wi = {0, st, 0, 0, st ? 1u : 0u, 0, th, f.w};
        R content = {f.x, f.y + th, f.w, f.h - th};
        HIThemeDrawWindowFrame(&content, &wi, g, 0, 0);
    } else {   /* botões (bevel, push, round, segmentados, toolbar, list header...) */
        hi_button_t b = {0, st, 0, (u32)(value >= 1), 0};
        HIThemeDrawButton(&f, &b, (id)g, 0, 0);
    }
}

/* ================================================================ JRSFont
 * Estilo (bits do mOS): 1 suavizado, 2 métricas fracionárias, 4 alinhado para medir */
enum { JRS_AA = 1, JRS_FRAC = 2, JRS_ALIGN = 4 };
u32 JRSFontGetRenderingStyleForHints(i32 fm_hint, i32 aa_hint) {
    u32 s = 0;
    if (aa_hint != 1) s |= JRS_AA;      /* SunHints INTVAL_TEXT_ANTIALIAS_OFF = 1 */
    if (fm_hint == 2) s |= JRS_FRAC;    /* INTVAL_FRACTIONALMETRICS_ON = 2 */
    return s;
}
u8 JRSFontStyleUsesFractionalMetrics(u32 s) { return (s & JRS_FRAC) != 0; }
u32 JRSFontAlignStyleForFractionalMeasurement(u32 s) { return s | JRS_ALIGN; }
void JRSFontSetRenderingStyleOnContext(void *g, u32 s) {
    if (!g) return;
    CGContextSetShouldAntialias(g, (s & JRS_AA) != 0);
    CGContextSetShouldSmoothFonts(g, (s & JRS_AA) != 0);
    CGContextSetAllowsFontSubpixelPositioning(g, (s & JRS_FRAC) != 0);
    CGContextSetShouldSubpixelPositionFonts(g, (s & JRS_FRAC) != 0);
}
/* caixas dos glifos com a transformação; sem métricas fracionárias (e sem o alinhamento de medida), arredonda para
 * fora, em pixels inteiros */
void JRSFontGetBoundingBoxesForGlyphsAndStyle(id font, const AT *tx, u32 style, const u16 *glyphs, i64 count, R *out) {
    if (!font || !glyphs || !out || count <= 0) return;
    CTFontGetBoundingRectsForGlyphs(font, 0, glyphs, out, count);
    for (i64 i = 0; i < count; i++) {
        R r = out[i];
        if (tx) {
            double xs[4] = {r.x, r.x + r.w, r.x, r.x + r.w}, ys[4] = {r.y, r.y, r.y + r.h, r.y + r.h};
            double x0 = 1e300, y0 = 1e300, x1 = -1e300, y1 = -1e300;
            for (int k = 0; k < 4; k++) {
                double X = tx->a * xs[k] + tx->c * ys[k] + tx->tx, Y = tx->b * xs[k] + tx->d * ys[k] + tx->ty;
                if (X < x0) x0 = X;
                if (X > x1) x1 = X;
                if (Y < y0) y0 = Y;
                if (Y > y1) y1 = Y;
            }
            r = (R){x0, y0, x1 - x0, y1 - y0};
        }
        if (!(style & JRS_FRAC) && !(style & JRS_ALIGN)) {
            double x0 = __builtin_floor(r.x), y0 = __builtin_floor(r.y);
            double x1 = __builtin_ceil(r.x + r.w), y1 = __builtin_ceil(r.y + r.h);
            r = (R){x0, y0, x1 - x0, y1 - y0};
        }
        out[i] = r;
    }
}
/* fonte que tem os caracteres (a pilha de substituição do Core Text) */
id JRSFontCreateFallbackFontForCharacters(id font, const u16 *chars, i64 n) {
    if (!font || !chars || n <= 0) return font ? objc_retain(font) : 0;
    id s = CFStringCreateWithCharacters(0, chars, n);
    cfrange rg = {0, n};
    id f = s ? CTFontCreateForString(font, s, rg) : 0;
    if (s) objc_release(s);
    return f ? f : objc_retain(font);
}
