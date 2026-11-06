/*
 * mOS libSystem — displays do CoreGraphics (CGDisplay*) e métodos de NSScreen que o Chromium consulta
 * para montar o display::Screen. O mOS tem uma tela (id 1); o tamanho vem do kernel (GUI_SCREEN).
 * Sem HDR, sem rotação, 60 Hz, 96 dpi.
 */
#include "foundation.h"
#include "../../kernel/include/compat_gui.h"

id objc_retain(id o);

typedef struct { double x, y, w, h; } cg_rect_t;
typedef struct { double w, h; } cg_size_t;
#define MAIN_DISPLAY 1u

static void screen_size(int *w, int *h) {
    gui_wininfo_t i = {1440, 876, 1, 1};
    SYS1(GUI_SCREEN, &i);
    *w = i.w;
    *h = i.h + 24;                                     /* + barra de menus, como NSScreen.frame */
}

u32 CGMainDisplayID(void) { return MAIN_DISPLAY; }
static int one_display(u32 max, u32 *ids, u32 *count) {
    if (ids && max) ids[0] = MAIN_DISPLAY;
    if (count) *count = ids && !max ? 0 : 1;
    return 0;                                          /* kCGErrorSuccess */
}
int CGGetActiveDisplayList(u32 max, u32 *ids, u32 *count) { return one_display(max, ids, count); }
int CGGetOnlineDisplayList(u32 max, u32 *ids, u32 *count) { return one_display(max, ids, count); }
int CGGetDisplaysWithPoint(double x, double y, u32 max, u32 *ids, u32 *count) { (void)x; (void)y; return one_display(max, ids, count); }
int CGGetDisplaysWithRect(cg_rect_t r, u32 max, u32 *ids, u32 *count) { (void)r; return one_display(max, ids, count); }
cg_rect_t CGDisplayBounds(u32 d) { (void)d; int w, h; screen_size(&w, &h); cg_rect_t r = {0, 0, w, h}; return r; }
size_t CGDisplayPixelsWide(u32 d) { (void)d; int w, h; screen_size(&w, &h); return (size_t)w; }
size_t CGDisplayPixelsHigh(u32 d) { (void)d; int w, h; screen_size(&w, &h); return (size_t)h; }
cg_size_t CGDisplayScreenSize(u32 d) { (void)d; int w, h; screen_size(&w, &h); cg_size_t s = {w * 25.4 / 96, h * 25.4 / 96}; return s; }
int CGDisplayIsBuiltin(u32 d) { return d == MAIN_DISPLAY; }
int CGDisplayIsMain(u32 d) { return d == MAIN_DISPLAY; }
int CGDisplayIsActive(u32 d) { return d == MAIN_DISPLAY; }
int CGDisplayIsOnline(u32 d) { return d == MAIN_DISPLAY; }
int CGDisplayIsAsleep(u32 d) { (void)d; return 0; }
int CGDisplayIsInMirrorSet(u32 d) { (void)d; return 0; }
u32 CGDisplayMirrorsDisplay(u32 d) { (void)d; return 0; }
int CGDisplayUsesForceToGray(void) { return 0; }
int CGDisplayUsesInvertedPolarity(void) { return 0; }
int CGDisplayUsesOpenGLAcceleration(u32 d) { (void)d; return 0; }
double CGDisplayRotation(u32 d) { (void)d; return 0.0; }
u32 CGDisplayVendorNumber(u32 d) { (void)d; return 0; }
u32 CGDisplayModelNumber(u32 d) { (void)d; return 0; }
u32 CGDisplaySerialNumber(u32 d) { (void)d; return 0; }
u32 CGDisplayUnitNumber(u32 d) { (void)d; return 0; }
int CGDisplayRegisterReconfigurationCallback(void *cb, void *info) { (void)cb; (void)info; return 0; }
int CGDisplayRemoveReconfigurationCallback(void *cb, void *info) { (void)cb; (void)info; return 0; }
void *CGDisplayCopyColorSpace(u32 d) { (void)d; return 0; }
void *CGDisplayCopyDisplayMode(u32 d) { (void)d; return 0; }
double CGDisplayModeGetRefreshRate(void *mode) { (void)mode; return 60.0; }
void CGDisplayModeRelease(void *mode) { (void)mode; }
void *CGDisplayCreateUUIDFromDisplayID(u32 d) { (void)d; return 0; }
u32 CGDisplayGetDisplayIDFromUUID(void *uuid) { (void)uuid; return MAIN_DISPLAY; }

/* ---- NSScreen (métodos que o appkit.c ainda não tem) */
id cx_num_i(i64 v);
static id sc_device(id self, SEL c) {
    (void)self; (void)c;
    static id desc;
    if (!desc) {
        id d = MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary");
        MSG(void, d, "setObject:forKey:", cx_num_i(MAIN_DISPLAY), cx_str("NSScreenNumber"));
        MSG(void, d, "setObject:forKey:", cx_str("YES"), cx_str("NSDeviceIsScreen"));
        MSG(void, d, "setObject:forKey:", cx_num_i(32), cx_str("NSDeviceBitsPerSample"));
        desc = objc_retain(d);
    }
    return desc;
}
static double sc_one(id self, SEL c) { (void)self; (void)c; return 1.0; }
static double sc_refresh(id self, SEL c) { (void)self; (void)c; return 1.0 / 60.0; }
static id sc_name(id self, SEL c) { (void)self; (void)c; return cx_str("Tela do mOS"); }
/* ---- NSColorSpace (o mOS só tem sRGB): todos os espaços são o mesmo objeto; -CGColorSpace devolve
 * o próprio objeto (as funções CGColorSpace* do coregraphics.c aceitam qualquer ponteiro) */
cx_class_t NSColorSpace_c __asm__("OBJC_CLASS_$_NSColorSpace");
cx_class_t NSColorSpace_m __asm__("OBJC_METACLASS_$_NSColorSpace");
static id cs_shared(id cls, SEL c) { (void)c; static id s; if (!s) s = cx_alloc_instance((cx_class_t *)cls); return s; }
static id cs_self(id self, SEL c) { (void)c; return self; }
static long cs_components(id self, SEL c) { (void)self; (void)c; return 3; }
static long cs_model(id self, SEL c) { (void)self; (void)c; return 1; }          /* NSColorSpaceModelRGB */
static id cs_name(id self, SEL c) { (void)self; (void)c; return cx_str("sRGB IEC61966-2.1"); }

/* ---- cores do sistema que o appkit.c não tem: uma IMP só, escolhida pelo nome do seletor (no runtime
 * do mOS um SEL é o próprio nome). Valores do tema claro do macOS. O Chromium converte cada cor do
 * sistema para CGColor e faz CHECK do resultado: nil encerrava o VS Code. */
id cx_color(double r, double g, double b, double a);
int strcmp(const char *a, const char *b);
static const struct { const char *sel; double r, g, b, a; } sys_colors[] = {
    {"alternateSelectedControlTextColor", 1, 1, 1, 1}, {"disabledControlTextColor", 0, 0, 0, 0.25},
    {"findHighlightColor", 1, 1, 0, 1}, {"headerTextColor", 0, 0, 0, 0.85}, {"highlightColor", 1, 1, 1, 1},
    {"keyboardFocusIndicatorColor", 0, 0.404, 0.851, 0.5}, {"quaternaryLabelColor", 0, 0, 0, 0.1},
    {"scrollBarColor", 0.667, 0.667, 0.667, 1}, {"selectedContentBackgroundColor", 0, 0.388, 0.882, 1},
    {"selectedControlColor", 0.702, 0.843, 1, 1}, {"selectedControlTextColor", 0, 0, 0, 0.85},
    {"selectedMenuItemTextColor", 1, 1, 1, 1}, {"selectedTextBackgroundColor", 0.702, 0.843, 1, 1},
    {"selectedTextColor", 0, 0, 0, 1}, {"shadowColor", 0, 0, 0, 1}, {"textInsertionPointColor", 0, 0, 0, 1},
    {"unemphasizedSelectedContentBackgroundColor", 0.863, 0.863, 0.863, 1},
    {"unemphasizedSelectedTextBackgroundColor", 0.863, 0.863, 0.863, 1}, {"unemphasizedSelectedTextColor", 0, 0, 0, 1},
    {"windowFrameColor", 0.667, 0.667, 0.667, 1}, {"windowFrameTextColor", 0, 0, 0, 0.85},
    {"controlHighlightColor", 1, 1, 1, 1}, {"controlLightHighlightColor", 1, 1, 1, 1},
    {"controlShadowColor", 0.5, 0.5, 0.5, 1}, {"controlDarkShadowColor", 0, 0, 0, 1}, {"knobColor", 0, 0, 0, 0.5},
    {"selectedKnobColor", 0, 0, 0, 0.7}, {"systemBrownColor", 0.635, 0.518, 0.369, 1}, {"systemMintColor", 0, 0.78, 0.745, 1},
    {"systemCyanColor", 0.333, 0.745, 0.941, 1}, {"systemFillColor", 0, 0, 0, 0.1}, {"secondarySystemFillColor", 0, 0, 0, 0.08},
    {"tertiarySystemFillColor", 0, 0, 0, 0.05}, {"quaternarySystemFillColor", 0, 0, 0, 0.03},
    {"scrubberTexturedBackgroundColor", 0.5, 0.5, 0.5, 1}, {"windowBackgroundColor", 0.925, 0.925, 0.925, 1},
};
static id sys_color(id cls, SEL c) {
    (void)cls;
    for (u32 i = 0; i < sizeof sys_colors / sizeof sys_colors[0]; i++)
        if (!strcmp(c, sys_colors[i].sel)) return cx_color(sys_colors[i].r, sys_colors[i].g, sys_colors[i].b, sys_colors[i].a);
    return cx_color(0.5, 0.5, 0.5, 1);
}
static id cl_catalog(id cls, SEL c, id catalog, id name) { (void)cls; (void)c; (void)catalog; (void)name; return cx_color(0.5, 0.5, 0.5, 1); }
static id cl_alternating(id cls, SEL c) {
    (void)cls; (void)c;
    id a = MSG0(id, (id)cx_class_named("NSMutableArray"), "array");
    MSG(void, a, "addObject:", cx_color(1, 1, 1, 1));
    MSG(void, a, "addObject:", cx_color(0.957, 0.961, 0.961, 1));
    return a;
}
typedef struct { cx_class_t *isa; double r, g, b, a; } cl_rgba_t;     /* cgcolor_t do appkit/coregraphics */
static void cl_get_rgba(id self, SEL c, double *r, double *g, double *b, double *a) {
    (void)c;
    cl_rgba_t *k = (cl_rgba_t *)self;
    if (r) *r = k->r; if (g) *g = k->g; if (b) *b = k->b; if (a) *a = k->a;
}
static void cl_get_components(id self, SEL c, double *out) { (void)c; cl_rgba_t *k = (cl_rgba_t *)self; if (out) { out[0] = k->r; out[1] = k->g; out[2] = k->b; out[3] = k->a; } }
static long cl_ncomp(id self, SEL c) { (void)self; (void)c; return 4; }
static id cl_from_cg(id cls, SEL c, id cg) { (void)cls; (void)c; return cg; }   /* NSColor e CGColor são o mesmo objeto */
static id cl_space(id self, SEL c) { (void)self; (void)c; return cs_shared((id)&NSColorSpace_c, 0); }
static void colors_init(void) {
    cx_class_t *cl = cx_class_named("NSColor");
    cx_define_class(&NSColorSpace_c, &NSColorSpace_m, "NSColorSpace", cx_class_named("NSObject"), 0);
    static const char *spaces[] = {"deviceRGBColorSpace", "sRGBColorSpace", "genericRGBColorSpace", "extendedSRGBColorSpace",
                                   "displayP3ColorSpace", "genericGamma22GrayColorSpace", "deviceGrayColorSpace", "genericGrayColorSpace"};
    for (u32 i = 0; i < sizeof spaces / sizeof spaces[0]; i++) cx_add_cmethod(&NSColorSpace_c, spaces[i], (void *)cs_shared);
    cx_add_method(&NSColorSpace_c, "CGColorSpace", (void *)cs_self);
    cx_add_method(&NSColorSpace_c, "numberOfColorComponents", (void *)cs_components);
    cx_add_method(&NSColorSpace_c, "colorSpaceModel", (void *)cs_model);
    cx_add_method(&NSColorSpace_c, "localizedName", (void *)cs_name);
    if (!cl) return;
    cx_class_t *meta = *(cx_class_t **)cl;
    for (u32 i = 0; i < sizeof sys_colors / sizeof sys_colors[0]; i++)
        if (!cx_responds(meta, sel_registerName(sys_colors[i].sel))) cx_add_cmethod(cl, sys_colors[i].sel, (void *)sys_color);
    if (!cx_responds(meta, sel_registerName("colorWithCatalogName:colorName:"))) cx_add_cmethod(cl, "colorWithCatalogName:colorName:", (void *)cl_catalog);
    if (!cx_responds(meta, sel_registerName("alternatingContentBackgroundColors"))) cx_add_cmethod(cl, "alternatingContentBackgroundColors", (void *)cl_alternating);
    if (!cx_responds(meta, sel_registerName("colorWithCGColor:"))) cx_add_cmethod(cl, "colorWithCGColor:", (void *)cl_from_cg);
    if (!cx_responds(cl, sel_registerName("getRed:green:blue:alpha:"))) cx_add_method(cl, "getRed:green:blue:alpha:", (void *)cl_get_rgba);
    if (!cx_responds(cl, sel_registerName("getComponents:"))) cx_add_method(cl, "getComponents:", (void *)cl_get_components);
    if (!cx_responds(cl, sel_registerName("numberOfComponents"))) cx_add_method(cl, "numberOfComponents", (void *)cl_ncomp);
    if (!cx_responds(cl, sel_registerName("colorSpace"))) cx_add_method(cl, "colorSpace", (void *)cl_space);
}

/* ---- funções geométricas do AppKit (NSRect = 4 doubles, passado e devolvido em memória, como no macOS) */
typedef struct { double x, y; } ns_pt_t;
typedef struct { double w, h; } ns_sz_t;
typedef struct { double x, y, w, h; } ns_rect_t;
static double mind(double a, double b) { return a < b ? a : b; }
static double maxd(double a, double b) { return a > b ? a : b; }
BOOL NSEqualRects(ns_rect_t a, ns_rect_t b) { return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h; }
BOOL NSEqualSizes(ns_sz_t a, ns_sz_t b) { return a.w == b.w && a.h == b.h; }
BOOL NSPointInRect(ns_pt_t p, ns_rect_t r) { return p.x >= r.x && p.y >= r.y && p.x < r.x + r.w && p.y < r.y + r.h; }
BOOL NSMouseInRect(ns_pt_t p, ns_rect_t r, BOOL flipped) { (void)flipped; return NSPointInRect(p, r); }
BOOL NSContainsRect(ns_rect_t a, ns_rect_t b) {
    return b.w > 0 && b.h > 0 && a.x <= b.x && a.y <= b.y && a.x + a.w >= b.x + b.w && a.y + a.h >= b.y + b.h;
}
ns_rect_t NSInsetRect(ns_rect_t r, double dx, double dy) { r.x += dx; r.y += dy; r.w -= 2 * dx; r.h -= 2 * dy; return r; }
ns_rect_t NSOffsetRect(ns_rect_t r, double dx, double dy) { r.x += dx; r.y += dy; return r; }
ns_rect_t NSIntersectionRect(ns_rect_t a, ns_rect_t b) {
    double x0 = maxd(a.x, b.x), y0 = maxd(a.y, b.y), x1 = mind(a.x + a.w, b.x + b.w), y1 = mind(a.y + a.h, b.y + b.h);
    ns_rect_t z = {0, 0, 0, 0};
    if (x1 <= x0 || y1 <= y0) return z;
    ns_rect_t r = {x0, y0, x1 - x0, y1 - y0};
    return r;
}
/* acessibilidade: sem leitor de tela no mOS; as notificações não vão a lugar nenhum */
void NSAccessibilityPostNotification(id element, id notification) { (void)element; (void)notification; }
void NSAccessibilityPostNotificationWithUserInfo(id element, id notification, id info) { (void)element; (void)notification; (void)info; }
id NSAccessibilityUnignoredAncestor(id element) { return element; }
id NSAccessibilityUnignoredChildren(id children) { return children; }

void cx_cgdisplay_init(void) {
    colors_init();
    cx_class_t *s = cx_class_named("NSScreen");
    if (!s) return;
    cx_add_method(s, "deviceDescription", (void *)sc_device);
    cx_add_method(s, "maximumPotentialExtendedDynamicRangeColorComponentValue", (void *)sc_one);
    cx_add_method(s, "maximumExtendedDynamicRangeColorComponentValue", (void *)sc_one);
    cx_add_method(s, "maximumReferenceExtendedDynamicRangeColorComponentValue", (void *)sc_one);
    cx_add_method(s, "minimumRefreshInterval", (void *)sc_refresh);
    cx_add_method(s, "maximumRefreshInterval", (void *)sc_refresh);
    cx_add_method(s, "displayUpdateGranularity", (void *)sc_refresh);
    cx_add_method(s, "localizedName", (void *)sc_name);
    cx_add_method(s, "colorSpace", (void *)cl_space);
}
