/*
 * mOS libSystem — funções C e constantes do AppKit que os apps do corpus importam e não existiam
 * (docs/lacunas/beatriz.md, relatório do Igor): painéis de alerta em C (NSRunAlertPanel e família,
 * NSGetAlertPanel/NSReleaseAlertPanel, NSBeginAlertSheet e família), profundidade de tela (NSBestDepth,
 * NSAvailableWindowDepths, NSBitsPerSampleFromDepth, NSColorSpaceFromDepth), NSApplicationLoad,
 * NSDisable/EnableScreenUpdates, NSShowAnimationEffect, NSDrawThreePartImage, ajudas de acessibilidade, os
 * marcadores de binding (NSNoSelectionMarker...) e constantes de texto.
 * As de desenho que mexem no recorte (NSRectClip, NSRectFillList...) ficam no appkit.c, junto do contexto.
 */
#include "foundation.h"
#include "sys.h"
#include "libc.h"
#include <stdarg.h>

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
extern id NSApp;
id cx_str_vfmt(id fmt, va_list ap);

typedef struct { double x, y; } P;
typedef struct { double w, h; } S;
typedef struct { double x, y, w, h; } R;

static cx_class_t *kls(const char *n) { return cx_class_named(n); }
static SEL sl(const char *n) { return sel_registerName(n); }
static BOOL responds(id o, const char *s) { return o && cx_responds(object_getClass(o), sl(s)); }
static S msg_size(id o, const char *s) { return ((S (*)(id, SEL))objc_msgSend)(o, sl(s)); }

/* ---- constantes (NSString constantes do CF, como o resto do libSystem) */
extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } cf_const_t;
#define CS(var, text) static cf_const_t var##_s = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; const void *var = &var##_s;
CS(NSFileContentsPboardType, "NSFileContentsPboardType")
CS(NSControlTintDidChangeNotification, "NSControlTintDidChangeNotification")
CS(NSWorkspaceRecycleOperation, "recycle")
CS(NSAboutPanelOptionApplicationIcon, "ApplicationIcon")
CS(NSAboutPanelOptionApplicationName, "ApplicationName")
CS(NSAboutPanelOptionApplicationVersion, "ApplicationVersion")
CS(NSAccessibilityMisspelledTextAttribute, "AXMisspelled")
CS(NSAccessibilityShadowTextAttribute, "AXShadow")
CS(NSAccessibilityUnderlineColorTextAttribute, "AXUnderlineColor")
CS(NSAccessibilityVisibleNameKey, "AXVisibleName")
CS(NSAllRomanInputSourcesLocaleIdentifier, "NSAllRomanInputSourcesLocaleIdentifier")
CS(NSInkTextPboardType, "NSInkTextPboardType")
CS(NSMultipleTextSelectionPboardType, "NSMultipleTextSelectionPboardType")
CS(NSOutlineViewDisclosureButtonKey, "NSOutlineViewDisclosureButtonKey")
CS(NSPostScriptPboardType, "NeXT Encapsulated PostScript v1.2 pasteboard type")
CS(NSVCardPboardType, "Apple VCard pasteboard type")
CS(NSSelectedTagBinding, "selectedTag")
CS(NSTextContentTypeURL, "URL")
CS(NSImageNameTouchBarColorPickerFill, "NSTouchBarColorPickerFill")
CS(NSImageNameTouchBarExitFullScreenTemplate, "NSTouchBarExitFullScreenTemplate")
CS(NSImageNameTouchBarFastForwardTemplate, "NSTouchBarFastForwardTemplate")
CS(NSImageNameTouchBarGoDownTemplate, "NSTouchBarGoDownTemplate")
CS(NSImageNameTouchBarGoUpTemplate, "NSTouchBarGoUpTemplate")
CS(NSImageNameTouchBarIconViewTemplate, "NSTouchBarIconViewTemplate")
CS(NSImageNameTouchBarPauseTemplate, "NSTouchBarPauseTemplate")
CS(NSImageNameTouchBarPlayTemplate, "NSTouchBarPlayTemplate")
CS(NSImageNameTouchBarRemoveTemplate, "NSTouchBarRemoveTemplate")
CS(NSImageNameTouchBarRewindTemplate, "NSTouchBarRewindTemplate")
CS(NSImageNameTouchBarSkipAhead15SecondsTemplate, "NSTouchBarSkipAhead15SecondsTemplate")
CS(NSImageNameTouchBarSkipAhead30SecondsTemplate, "NSTouchBarSkipAhead30SecondsTemplate")
CS(NSImageNameTouchBarSkipAheadTemplate, "NSTouchBarSkipAheadTemplate")
CS(NSImageNameTouchBarSkipBack15SecondsTemplate, "NSTouchBarSkipBack15SecondsTemplate")
CS(NSImageNameTouchBarSkipBack30SecondsTemplate, "NSTouchBarSkipBack30SecondsTemplate")
CS(NSImageNameTouchBarSkipBackTemplate, "NSTouchBarSkipBackTemplate")
CS(NSImageNameTouchBarVolumeDownTemplate, "NSTouchBarVolumeDownTemplate")
CS(NSImageNameTouchBarVolumeUpTemplate, "NSTouchBarVolumeUpTemplate")
const double NSDarkGray = 1.0 / 3.0, NSLightGray = 2.0 / 3.0, NSWhite = 1.0, NSBlack = 0.0;
const double NSSplitViewItemUnspecifiedDimension = -1.0;

/* marcadores de binding: objetos únicos (o Cocoa usa singletons; o app compara por ponteiro) */
CS(cx_mk_no_selection, "<NO SELECTION MARKER>")
CS(cx_mk_multiple, "<MULTIPLE VALUES MARKER>")
CS(cx_mk_not_applicable, "<NOT APPLICABLE MARKER>")
const void *NSNoSelectionMarker = &cx_mk_no_selection_s;
const void *NSMultipleValuesMarker = &cx_mk_multiple_s;
const void *NSNotApplicableMarker = &cx_mk_not_applicable_s;
BOOL NSIsControllerMarker(id o) { return o == (id)NSNoSelectionMarker || o == (id)NSMultipleValuesMarker || o == (id)NSNotApplicableMarker; }

/* ---- profundidade de tela: o mOS compõe em 32 bits (8 por componente, RGB) */
enum { DEPTH_24RGB = 0x208, DEPTH_64RGB = 0x210, DEPTH_128RGB = 0x220 };
static const int g_depths[] = {DEPTH_24RGB, 0};
const int *NSAvailableWindowDepths(void) { return g_depths; }
int NSBestDepth(id colorSpace, i64 bps, i64 bpp, BOOL planar, BOOL *exact) {
    (void)colorSpace; (void)planar;
    if (exact) *exact = (bps <= 0 || bps == 8) && (bpp <= 0 || bpp == 24 || bpp == 32);
    return DEPTH_24RGB;
}
i64 NSBitsPerSampleFromDepth(int depth) { return depth & 0xFF; }
id NSColorSpaceFromDepth(int depth) { (void)depth; return cx_str("NSCalibratedRGBColorSpace"); }

/* ---- aplicação */
BOOL NSApplicationLoad(void) { return MSG0(id, (id)kls("NSApplication"), "sharedApplication") != 0; }
static int g_updates_off;
/* o window server compõe por quadro: as atualizações já saem juntas; o contador só mantém o par equilibrado */
void NSDisableScreenUpdates(void) { g_updates_off++; }
void NSEnableScreenUpdates(void) { if (g_updates_off > 0) g_updates_off--; }
/* o "puf" não é desenhado; o delegate recebe o fim na hora, como depois da animação */
void NSShowAnimationEffect(u64 effect, P center, S size, id delegate, SEL did_end, void *ctx) {
    (void)effect; (void)center; (void)size;
    if (delegate && did_end && cx_responds(object_getClass(delegate), did_end)) ((void (*)(id, SEL, void *))objc_msgSend)(delegate, did_end, ctx);
}

/* ---- imagem em três partes (começo e fim no tamanho natural, meio esticado) */
void NSDrawThreePartImage(R frame, id start, id center, id end, BOOL vertical, u64 op, double alpha, BOOL flipped) {
    (void)flipped;
    S a = start ? msg_size(start, "size") : (S){0, 0}, b = end ? msg_size(end, "size") : (S){0, 0};
    R pr[3];
    if (!vertical) {
        double aw = a.w * (a.h > 0 ? frame.h / a.h : 1), bw = b.w * (b.h > 0 ? frame.h / b.h : 1);
        if (aw + bw > frame.w) { double k = frame.w / (aw + bw); aw *= k; bw *= k; }
        pr[0] = (R){frame.x, frame.y, aw, frame.h};
        pr[1] = (R){frame.x + aw, frame.y, frame.w - aw - bw, frame.h};
        pr[2] = (R){frame.x + frame.w - bw, frame.y, bw, frame.h};
    } else {
        double ah = a.h * (a.w > 0 ? frame.w / a.w : 1), bh = b.h * (b.w > 0 ? frame.w / b.w : 1);
        if (ah + bh > frame.h) { double k = frame.h / (ah + bh); ah *= k; bh *= k; }
        pr[0] = (R){frame.x, frame.y + frame.h - ah, frame.w, ah};      /* o começo fica em cima */
        pr[1] = (R){frame.x, frame.y + bh, frame.w, frame.h - ah - bh};
        pr[2] = (R){frame.x, frame.y, frame.w, bh};
    }
    id parts[3] = {start, center, end};
    for (int i = 0; i < 3; i++) {
        if (!parts[i] || pr[i].w <= 0 || pr[i].h <= 0) continue;
        ((void (*)(id, SEL, R, R, u64, double))objc_msgSend)(parts[i], sl("drawInRect:fromRect:operation:fraction:"), pr[i], (R){0, 0, 0, 0}, op, alpha);
    }
}

/* ---- acessibilidade */
id NSAccessibilityRoleDescription(id role, id subrole);
id NSAccessibilityRoleDescriptionForUIElement(id el) {
    if (responds(el, "accessibilityRoleDescription")) { id d = MSG0(id, el, "accessibilityRoleDescription"); if (d) return d; }
    id role = responds(el, "accessibilityRole") ? MSG0(id, el, "accessibilityRole") : 0;
    id sub = responds(el, "accessibilitySubrole") ? MSG0(id, el, "accessibilitySubrole") : 0;
    return role ? NSAccessibilityRoleDescription(role, sub) : 0;
}
/* um filho ignorado é trocado pelos filhos dele que não são ignorados */
id NSAccessibilityUnignoredChildrenForOnlyChild(id child) {
    id out = MSG0(id, (id)kls("NSMutableArray"), "array");
    if (!child) return out;
    BOOL ignored = responds(child, "isAccessibilityElement") && !MSG0(BOOL, child, "isAccessibilityElement");
    if (!ignored) { MSG(void, out, "addObject:", child); return out; }
    id kids = responds(child, "accessibilityChildren") ? MSG0(id, child, "accessibilityChildren") : 0;
    u64 n = kids ? MSG0(u64, kids, "count") : 0;
    for (u64 i = 0; i < n; i++) {
        id k = MSG(id, kids, "objectAtIndex:", i);
        id sub = NSAccessibilityUnignoredChildrenForOnlyChild(k);
        MSG(void, out, "addObjectsFromArray:", sub);
    }
    return out;
}

/* ---- painéis de alerta em C (API antiga, ainda usada por LibreOffice, Krita, iTerm2...) */
enum { RET_DEFAULT = 1, RET_ALTERNATE = 0, RET_OTHER = -1 };
static id make_alert(id title, id msg, id def, id alt, id other, int style) {
    id al = MSG0(id, MSG0(id, (id)kls("NSAlert"), "alloc"), "init");
    if (title) MSG(void, al, "setMessageText:", title);
    if (msg) MSG(void, al, "setInformativeText:", msg);
    MSG(id, al, "addButtonWithTitle:", def ? def : cx_str("OK"));
    if (alt) MSG(id, al, "addButtonWithTitle:", alt);
    if (other) MSG(id, al, "addButtonWithTitle:", other);
    MSG(void, al, "setAlertStyle:", (i64)style);
    return al;
}
static i64 map_ret(i64 r, id alt) { return r == 1000 ? RET_DEFAULT : r == 1001 ? (alt ? RET_ALTERNATE : RET_OTHER) : r == 1002 ? RET_OTHER : RET_ALTERNATE; }
static i64 run_panel(id title, id fmt, id def, id alt, id other, int style, va_list ap) {
    id msg = fmt ? cx_str_vfmt(fmt, ap) : 0;
    id al = make_alert(title, msg, def, alt, other, style);
    i64 r = MSG0(i64, al, "runModal");
    objc_release(al);
    return map_ret(r, alt);
}
i64 NSRunAlertPanel(id title, id fmt, id def, id alt, id other, ...) { va_list ap; va_start(ap, other); i64 r = run_panel(title, fmt, def, alt, other, 1, ap); va_end(ap); return r; }
i64 NSRunInformationalAlertPanel(id title, id fmt, id def, id alt, id other, ...) { va_list ap; va_start(ap, other); i64 r = run_panel(title, fmt, def, alt, other, 1, ap); va_end(ap); return r; }
i64 NSRunCriticalAlertPanel(id title, id fmt, id def, id alt, id other, ...) { va_list ap; va_start(ap, other); i64 r = run_panel(title, fmt, def, alt, other, 2, ap); va_end(ap); return r; }

/* NSGetAlertPanel: um NSPanel que o app roda com -[NSApp runModalForWindow:]; os botões param o modal com
 * NSAlertDefaultReturn / Alternate / Other */
static cx_class_t ap_target_c, ap_target_m;
static void ap_hit(id self, SEL c, id sender) { (void)self; (void)c; MSG(void, NSApp, "stopModalWithCode:", MSG0(i64, sender, "tag")); }
static id panel_label(id text, double size, BOOL bold) {
    id l = MSG(id, (id)kls("NSTextField"), "wrappingLabelWithString:", text ? text : cx_str(""));
    id f = bold ? MSG(id, (id)kls("NSFont"), "boldSystemFontOfSize:", size) : MSG(id, (id)kls("NSFont"), "systemFontOfSize:", size);
    MSG(void, l, "setFont:", f);
    return l;
}
static id get_panel(id title, id msg, id def, id alt, id other) {
    if (!ap_target_c.isa) { cx_define_class(&ap_target_c, &ap_target_m, "_MOSAlertPanelTarget", kls("NSObject"), 0); cx_add_method(&ap_target_c, "cxAlertPanelHit:", (void *)ap_hit); }
    static id tgt;
    if (!tgt) tgt = cx_alloc_instance(&ap_target_c);
    double W = 420, H = 150;
    id w = ((id (*)(id, SEL, R, u64, u64, BOOL))objc_msgSend)(MSG0(id, (id)kls("NSPanel"), "alloc"), sl("initWithContentRect:styleMask:backing:defer:"), (R){0, 0, W, H}, (u64)1, (u64)2, NO);
    MSG(void, w, "setTitle:", cx_str(""));
    MSG(void, w, "setReleasedWhenClosed:", (BOOL)NO);
    id cv = MSG0(id, w, "contentView");
    id t = panel_label(title, 13, YES);
    ((void (*)(id, SEL, R))objc_msgSend)(t, sl("setFrame:"), (R){20, H - 42, W - 40, 22});
    MSG(void, cv, "addSubview:", t);
    if (msg) {
        id m = panel_label(msg, 12, NO);
        ((void (*)(id, SEL, R))objc_msgSend)(m, sl("setFrame:"), (R){20, 56, W - 40, H - 104});
        MSG(void, cv, "addSubview:", m);
    }
    struct { id t; i64 tag; } bt[3] = {{def ? def : cx_str("OK"), RET_DEFAULT}, {alt, RET_ALTERNATE}, {other, RET_OTHER}};
    double x = W - 20;
    for (int i = 0; i < 3; i++) {
        if (!bt[i].t) continue;
        id b = MSG(id, (id)kls("NSButton"), "buttonWithTitle:target:action:", bt[i].t, tgt, sl("cxAlertPanelHit:"));
        MSG(void, b, "setTag:", bt[i].tag);
        if (!i) MSG(void, b, "setKeyEquivalent:", cx_str("\r"));
        double bw = 96;
        x -= bw;
        ((void (*)(id, SEL, R))objc_msgSend)(b, sl("setFrame:"), (R){x, 14, bw, 24});
        x -= 10;
        MSG(void, cv, "addSubview:", b);
    }
    MSG0(void, w, "center");
    return w;
}
id NSGetAlertPanel(id title, id fmt, id def, id alt, id other, ...) {
    va_list ap; va_start(ap, other);
    id msg = fmt ? cx_str_vfmt(fmt, ap) : 0;
    va_end(ap);
    return get_panel(title, msg, def, alt, other);
}
id NSGetInformationalAlertPanel(id title, id fmt, id def, id alt, id other, ...) {
    va_list ap; va_start(ap, other);
    id msg = fmt ? cx_str_vfmt(fmt, ap) : 0;
    va_end(ap);
    return get_panel(title, msg, def, alt, other);
}
id NSGetCriticalAlertPanel(id title, id fmt, id def, id alt, id other, ...) {
    va_list ap; va_start(ap, other);
    id msg = fmt ? cx_str_vfmt(fmt, ap) : 0;
    va_end(ap);
    return get_panel(title, msg, def, alt, other);
}
void NSReleaseAlertPanel(id panel) {
    if (!panel) return;
    MSG(void, panel, "orderOut:", (id)0);
    MSG0(void, panel, "close");
    objc_release(panel);
}

/* folhas antigas: rodam como modal e chamam didEnd: e didDismiss: (sheet, código, contexto) */
static void begin_sheet(id title, id def, id alt, id other, id win, id dl, SEL did_end, SEL did_dismiss, void *ctx, int style, id fmt, va_list ap) {
    (void)win;
    id msg = fmt ? cx_str_vfmt(fmt, ap) : 0;
    id al = make_alert(title, msg, def, alt, other, style);
    i64 r = map_ret(MSG0(i64, al, "runModal"), alt);
    id sheet = MSG0(id, al, "window");
    if (dl && did_end) ((void (*)(id, SEL, id, i64, void *))objc_msgSend)(dl, did_end, sheet, r, ctx);
    if (dl && did_dismiss) ((void (*)(id, SEL, id, i64, void *))objc_msgSend)(dl, did_dismiss, sheet, r, ctx);
    objc_release(al);
}
void NSBeginAlertSheet(id title, id def, id alt, id other, id win, id dl, SEL did_end, SEL did_dismiss, void *ctx, id fmt, ...) {
    va_list ap; va_start(ap, fmt); begin_sheet(title, def, alt, other, win, dl, did_end, did_dismiss, ctx, 1, fmt, ap); va_end(ap);
}
void NSBeginInformationalAlertSheet(id title, id def, id alt, id other, id win, id dl, SEL did_end, SEL did_dismiss, void *ctx, id fmt, ...) {
    va_list ap; va_start(ap, fmt); begin_sheet(title, def, alt, other, win, dl, did_end, did_dismiss, ctx, 1, fmt, ap); va_end(ap);
}
void NSBeginCriticalAlertSheet(id title, id def, id alt, id other, id win, id dl, SEL did_end, SEL did_dismiss, void *ctx, id fmt, ...) {
    va_list ap; va_start(ap, fmt); begin_sheet(title, def, alt, other, win, dl, did_end, did_dismiss, ctx, 2, fmt, ap); va_end(ap);
}
