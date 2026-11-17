/*
 * mOS libSystem — CoreUI: CUIDraw, o desenho de controles por um dicionário de opções (o renderizador de temas que
 * o AppKit da Apple e alguns apps chamam direto).
 *
 * As chaves e os valores do dicionário são privados da Apple; aqui eles são lidos pelo sentido: o widget pela chave
 * "widget" (ou que contenha "Widget"), o estado por "state" (pressionado, inativo, desabilitado), o valor por
 * "value" (desligado/ligado/misto ou número), o tamanho por "size" e a direção por "direction". O desenho é o do tema
 * do mOS, pelo HITheme (appkit_carbon.c e appkit_carbon2.c): botões, caixas de seleção, rádios, trilhas (rolagem,
 * slider, progresso), abas, setas de pop-up, triângulos de abertura, alça de redimensionar, foco, separadores e fundo.
 */
#include "foundation.h"
#include "libc.h"

typedef struct { double x, y, w, h; } R;
typedef struct { double x, y; } P;
typedef int OSStatus;

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
typedef struct { u32 version, state, kind, direction, size; } hi_grow_t;
OSStatus HIThemeDrawGrowBox(const P *origin, const hi_grow_t *info, void *c, u32 orient);
OSStatus HIThemeDrawPlacard(const R *r, const void *info, void *c, u32 orient);
OSStatus HIThemeDrawBackground(const R *r, const void *info, void *c, u32 orient);
OSStatus HIThemeDrawGroupBox(const R *r, const void *info, void *c, u32 orient);
void CGContextSetRGBFillColor(void *c, double r, double g, double b, double a);
void CGContextBeginPath(void *c);
void CGContextMoveToPoint(void *c, double x, double y);
void CGContextAddLineToPoint(void *c, double x, double y);
void CGContextClosePath(void *c);
void CGContextFillPath(void *c);

/* ---- leitura tolerante do dicionário */
static id opt(id d, const char *want) {
    if (!d) return 0;
    id v = MSG(id, d, "objectForKey:", cx_str(want));
    if (v) return v;
    id keys = MSG0(id, d, "allKeys");
    u64 n = keys ? MSG0(u64, keys, "count") : 0;
    for (u64 i = 0; i < n; i++) {
        id k = MSG(id, keys, "objectAtIndex:", i);
        if (!k || !cx_is_str(k)) continue;
        const char *ks = cx_cstr(k);
        /* kCUIWidgetKey, CUIWidgetKey, widget... sem diferenciar maiúsculas */
        char low[64];
        usize j = 0;
        for (; ks[j] && j < sizeof low - 1; j++) low[j] = (char)(ks[j] >= 'A' && ks[j] <= 'Z' ? ks[j] + 32 : ks[j]);
        low[j] = 0;
        if (strstr(low, want)) return MSG(id, d, "objectForKey:", k);
    }
    return 0;
}
static int has(id v, const char *needle) {
    if (!v) return 0;
    const char *s = cx_is_str(v) ? cx_cstr(v) : cx_cstr(MSG0(id, v, "description"));
    char low[128];
    usize j = 0;
    for (; s[j] && j < sizeof low - 1; j++) low[j] = (char)(s[j] >= 'A' && s[j] <= 'Z' ? s[j] + 32 : s[j]);
    low[j] = 0;
    return strstr(low, needle) != 0;
}
static double num(id v, double def) {
    if (!v) return def;
    if (cx_is_str(v)) {
        if (has(v, "mixed")) return 2;
        if (has(v, "on") || has(v, "true") || has(v, "yes")) return 1;
        if (has(v, "off") || has(v, "false") || has(v, "no")) return 0;
    }
    return MSG0(double, v, "doubleValue");
}

/* CUIDraw(renderer, rect, context, options, result): desenha e devolve 0; result fica sem informação extra */
void CUIDraw(id renderer, R frame, void *cg, id options, id *result) {
    (void)renderer;
    if (result) *result = 0;
    if (!cg) return;
    id w = opt(options, "widget"), st = opt(options, "state"), val = opt(options, "value"), dir = opt(options, "direction");
    id focus = opt(options, "focus");
    u32 state = has(st, "pressed") ? 2 : (has(st, "disabled") || has(st, "inactive")) ? 0 : 1;
    double value = num(val, 0);
    R r = frame;
    if (has(w, "check")) {
        hi_button_t b = {0, state, 1, (u32)value, 0};
        HIThemeDrawButton(&r, &b, (id)cg, 0, 0);
    } else if (has(w, "radio")) {
        hi_button_t b = {0, state, 2, (u32)value, 0};
        HIThemeDrawButton(&r, &b, (id)cg, 0, 0);
    } else if (has(w, "disclosure")) {
        /* triângulo de abertura: aponta para a direita (fechado) ou para baixo (aberto) */
        double cx = r.x + r.w / 2, cy = r.y + r.h / 2, s = (r.w < r.h ? r.w : r.h) * 0.3;
        CGContextSetRGBFillColor(cg, 0.35, 0.35, 0.37, state ? 1 : 0.4);
        CGContextBeginPath(cg);
        if (value >= 1) { CGContextMoveToPoint(cg, cx - s, cy + s / 2); CGContextAddLineToPoint(cg, cx + s, cy + s / 2); CGContextAddLineToPoint(cg, cx, cy - s / 2); }
        else { CGContextMoveToPoint(cg, cx - s / 2, cy - s); CGContextAddLineToPoint(cg, cx - s / 2, cy + s); CGContextAddLineToPoint(cg, cx + s / 2, cy); }
        CGContextClosePath(cg);
        CGContextFillPath(cg);
    } else if (has(w, "scroll") || has(w, "slider") || has(w, "progress") || has(w, "level")) {
        int horiz = !has(dir, "vert") && r.w >= r.h;
        track_info t;
        memset(&t, 0, sizeof t);
        t.kind = has(w, "scroll") ? 0 : has(w, "slider") ? 2 : (has(st, "indeterminate") || has(w, "indeterminate")) ? 4 : 3;
        t.bounds = r;
        t.min = 0; t.max = 1000;
        t.value = (i32)(value <= 1 ? value * 1000 : value);
        t.attributes = (u16)((horiz ? 1 : 0) | 4);
        t.enable = state ? 1 : 0;
        if (t.kind == 0) {
            id knob = opt(options, "proportion");
            t.info.scroll.viewsize = (i32)(num(knob, 0.2) * 1000);
            t.info.scroll.press = state == 2;
        }
        HIThemeDrawTrack(&t, 0, cg, 0);
    } else if (has(w, "tab")) {
        hi_tab_t tb = {0, state == 2 ? 2u : value >= 1 ? 0u : 1u, 0, 0, 0, 0, 0};
        HIThemeDrawTab(&r, &tb, cg, 0, 0);
    } else if (has(w, "popup") || has(w, "combo") || has(w, "pulldown")) {
        hi_button_t b = {0, state, 0, 0, 0};
        HIThemeDrawButton(&r, &b, (id)cg, 0, 0);
        R ar = {r.x + r.w - r.h, r.y, r.h, r.h};
        hi_popup_t pa = {0, state, 0, 0};
        HIThemeDrawPopupArrow(&ar, &pa, cg, 0);
    } else if (has(w, "grow") || has(w, "resize")) {
        P o = {r.x + r.w, r.y + r.h};
        hi_grow_t g = {0, state, 0, 0, r.w < 13 ? 1u : 0u};
        HIThemeDrawGrowBox(&o, &g, cg, 0);
    } else if (has(w, "separator")) {
        HIThemeDrawSeparator(&r, 0, cg, 0);
    } else if (has(w, "placard")) {
        HIThemeDrawPlacard(&r, 0, cg, 0);
    } else if (has(w, "group") || has(w, "box")) {
        HIThemeDrawGroupBox(&r, 0, cg, 0);
    } else if (has(w, "background") || has(w, "window") || has(w, "frame")) {
        HIThemeDrawBackground(&r, 0, cg, 0);
    } else {
        /* botões e o que mais for um "bezel": o botão do tema */
        hi_button_t b = {0, state, 0, (u32)value, 0};
        HIThemeDrawButton(&r, &b, (id)cg, 0, 0);
    }
    if (focus && num(focus, 0) >= 1) HIThemeDrawFocusRect(&r, 1, cg, 0);
}
