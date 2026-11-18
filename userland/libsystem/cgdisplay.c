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
/* do EDID do monitor (displayservices.c); 0 sem GPU AMD ou sem EDID, como o macOS num monitor sem EDID */
unsigned mos_display_vendor(unsigned d), mos_display_model(unsigned d), mos_display_serial(unsigned d);
u32 CGDisplayVendorNumber(u32 d) { return mos_display_vendor(d); }
u32 CGDisplayModelNumber(u32 d) { return mos_display_model(d); }
u32 CGDisplaySerialNumber(u32 d) { return mos_display_serial(d); }
u32 CGDisplayUnitNumber(u32 d) { (void)d; return 0; }
int CGDisplayRegisterReconfigurationCallback(void *cb, void *info) { (void)cb; (void)info; return 0; }
int CGDisplayRemoveReconfigurationCallback(void *cb, void *info) { (void)cb; (void)info; return 0; }
void *CGDisplayCopyColorSpace(u32 d) { (void)d; return 0; }
static const int cg_display_mode = 1;
void *CGDisplayCopyDisplayMode(u32 d) { (void)d; return (void *)&cg_display_mode; }
double CGDisplayModeGetRefreshRate(void *mode) { (void)mode; return 60.0; }
void CGDisplayModeRelease(void *mode) { (void)mode; }
size_t CGDisplayModeGetPixelWidth(void *mode) {
    if (!mode) return 0;
    return CGDisplayPixelsWide(MAIN_DISPLAY);
}
size_t CGDisplayModeGetPixelHeight(void *mode) {
    if (!mode) return 0;
    return CGDisplayPixelsHigh(MAIN_DISPLAY);
}
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
static id cs_named(int i);
static id sc_space(id self, SEL c) { (void)self; (void)c; return cs_named(0); }   /* a tela do mOS é sRGB */
/* ---- NSColorSpace: cada um embrulha o seu CGColorSpace (coregraphics.c cria os espaços pelo nome, com o perfil ICC
 * deles). -CGColorSpace devolve esse espaço CG, nunca o próprio NSColorSpace: as funções do motor ICC (cg_color.inc) leem
 * os campos do cgspace_t, e o Electron faz [[x colorSpace] CGColorSpace] + CGColorSpaceCopyICCData. Os espaços do sistema
 * são únicos por nome; initWithCGColorSpace: e initWithICCProfileData: criam espaços novos. */
cx_class_t NSColorSpace_c __asm__("OBJC_CLASS_$_NSColorSpace");
cx_class_t NSColorSpace_m __asm__("OBJC_METACLASS_$_NSColorSpace");
typedef struct { cx_class_t *isa; void *cg; id name; } nscs_t;
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, unsigned long n);
void objc_release(id o);
id objc_autorelease(id o);
void *CGColorSpaceCreateWithName(const void *name);
void *CGColorSpaceCreateWithICCData(id data);
void *CGColorSpaceRetain(void *s);
void CGColorSpaceRelease(void *s);
size_t CGColorSpaceGetNumberOfComponents(const void *s);
i32 CGColorSpaceGetModel(const void *s);
id CGColorSpaceCopyICCData(void *space);
const void *CGColorSpaceGetName(const void *s);
extern const void *kCGColorSpaceSRGB, *kCGColorSpaceExtendedSRGB, *kCGColorSpaceGenericRGB, *kCGColorSpaceDisplayP3,
    *kCGColorSpaceAdobeRGB1998, *kCGColorSpaceGenericGrayGamma2_2, *kCGColorSpaceExtendedGray, *kCGColorSpaceGenericGray,
    *kCGColorSpaceGenericCMYK;
/* seletor de classe → espaço CG e nome mostrado (os device* são os mesmos espaços do CGColorSpaceCreateDevice*) */
static const struct { const char *sel; const void *const *cgname; const char *label; } cs_tab[] = {
    {"sRGBColorSpace", &kCGColorSpaceSRGB, "sRGB IEC61966-2.1"},
    {"extendedSRGBColorSpace", &kCGColorSpaceExtendedSRGB, "sRGB IEC61966-2.1 (estendido)"},
    {"genericRGBColorSpace", &kCGColorSpaceGenericRGB, "RGB genérico"},
    {"deviceRGBColorSpace", &kCGColorSpaceSRGB, "RGB do dispositivo"},
    {"displayP3ColorSpace", &kCGColorSpaceDisplayP3, "Display P3"},
    {"adobeRGB1998ColorSpace", &kCGColorSpaceAdobeRGB1998, "Adobe RGB (1998)"},
    {"genericGamma22GrayColorSpace", &kCGColorSpaceGenericGrayGamma2_2, "Cinza genérico gama 2.2"},
    {"extendedGenericGamma22GrayColorSpace", &kCGColorSpaceExtendedGray, "Cinza genérico gama 2.2 (estendido)"},
    {"genericGrayColorSpace", &kCGColorSpaceGenericGray, "Cinza genérico"},
    {"deviceGrayColorSpace", &kCGColorSpaceGenericGrayGamma2_2, "Cinza do dispositivo"},
    {"genericCMYKColorSpace", &kCGColorSpaceGenericCMYK, "CMYK genérico"},
    {"deviceCMYKColorSpace", &kCGColorSpaceGenericCMYK, "CMYK do dispositivo"},
};
#define NCS (sizeof cs_tab / sizeof cs_tab[0])
static id cs_shared_obj[NCS];
static nscs_t *cs_st(id o) { return (nscs_t *)o; }
static id cs_make(void *cg, const char *label) {
    id o = cx_alloc_instance(&NSColorSpace_c);
    if (!o) { CGColorSpaceRelease(cg); return 0; }
    cs_st(o)->cg = cg;
    cs_st(o)->name = label ? objc_retain(cx_str(label)) : 0;
    return o;
}
static int cs_index(const char *sel) {
    for (u32 i = 0; i < NCS; i++) if (!strcmp(sel, cs_tab[i].sel)) return (int)i;
    return -1;
}
static id cs_named(int i) {
    if (i < 0 || i >= (int)NCS) i = 0;
    if (!cs_shared_obj[i]) cs_shared_obj[i] = cs_make(CGColorSpaceCreateWithName(*cs_tab[i].cgname), cs_tab[i].label);
    return cs_shared_obj[i];
}
static id cs_shared(id cls, SEL c) { (void)cls; return cs_named(cs_index((const char *)c)); }   /* SEL é o nome no runtime do mOS */
static id cs_init_cg(id self, SEL c, void *cg) {
    (void)c;
    if (!cg) { objc_release(self); return 0; }
    cs_st(self)->cg = CGColorSpaceRetain(cg);
    return self;
}
static id cs_init_icc(id self, SEL c, id data) {
    (void)c;
    void *cg = data ? CGColorSpaceCreateWithICCData(data) : 0;
    if (!cg) { objc_release(self); return 0; }
    cs_st(self)->cg = cg;
    return self;
}
static void *cs_cg(id self, SEL c) { (void)c; return cs_st(self)->cg; }
static id cs_icc(id self, SEL c) { (void)c; id d = cs_st(self)->cg ? CGColorSpaceCopyICCData(cs_st(self)->cg) : 0; return d ? objc_autorelease(d) : 0; }
static long cs_components(id self, SEL c) { (void)c; return cs_st(self)->cg ? (long)CGColorSpaceGetNumberOfComponents(cs_st(self)->cg) : 0; }
/* NSColorSpaceModel: Unknown -1, Gray 0, RGB 1, CMYK 2, LAB 3, DeviceN 4, Indexed 5, Patterned 6 — os mesmos números do CG */
static long cs_model(id self, SEL c) { (void)c; return cs_st(self)->cg ? (long)CGColorSpaceGetModel(cs_st(self)->cg) : -1; }
static id cs_name(id self, SEL c) {
    (void)c;
    if (cs_st(self)->name) return cs_st(self)->name;
    const void *n = cs_st(self)->cg ? CGColorSpaceGetName(cs_st(self)->cg) : 0;
    return n ? (id)n : cx_str("Perfil ICC");
}
static BOOL cs_equal(id self, SEL c, id other) {
    (void)c;
    if (other == self) return YES;
    if (!other || object_getClass(other) != object_getClass(self)) return NO;
    void *a = cs_st(self)->cg, *b = cs_st(other)->cg;
    if (a == b) return YES;
    const void *na = a ? CGColorSpaceGetName(a) : 0, *nb = b ? CGColorSpaceGetName(b) : 0;
    return na && nb && MSG(BOOL, (id)na, "isEqual:", (id)nb);
}
static u64 cs_hash(id self, SEL c) { (void)c; return (u64)cs_model(self, 0) * 31 + (u64)cs_components(self, 0); }
static void cs_dealloc(id self, SEL c) {
    (void)c;
    for (u32 i = 0; i < NCS; i++) if (cs_shared_obj[i] == self) return;   /* os do sistema vivem para sempre */
    if (cs_st(self)->cg) CGColorSpaceRelease(cs_st(self)->cg);
    if (cs_st(self)->name) objc_release(cs_st(self)->name);
    IMP sup = cx_lookup(cx_class_named("NSObject"), sel_registerName("dealloc"));   /* o dealloc do NSObject libera */
    if (sup) ((void (*)(id, SEL))sup)(self, sel_registerName("dealloc"));
}
static id cs_available(id cls, SEL c, long model) {
    (void)cls; (void)c;
    id a = MSG0(id, (id)cx_class_named("NSMutableArray"), "array");
    for (u32 i = 0; i < NCS; i++) {
        if (!strncmp(cs_tab[i].sel, "device", 6)) continue;   /* os device* repetem os genéricos */
        id s = cs_named((int)i);
        if (model < 0 || cs_model(s, 0) == model) MSG(void, a, "addObject:", s);
    }
    return a;
}

/* ---- cores do sistema que o appkit.c não tem: uma IMP só, escolhida pelo nome do seletor (no runtime
 * do mOS um SEL é o próprio nome). Valores do tema claro do macOS. O Chromium converte cada cor do
 * sistema para CGColor e faz CHECK do resultado: nil encerrava o VS Code. */
id cx_color(double r, double g, double b, double a);
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
/* o espaço da cor: o do CGColor (as do appkit.c são sRGB; as de CGColorCreate guardam o seu); o do sistema quando o
 * nome bate, senão um NSColorSpace novo em volta dele */
void *CGColorGetColorSpace(const void *c);
static id cl_space(id self, SEL c) {
    (void)c;
    void *cg = CGColorGetColorSpace(self);
    const void *n = cg ? CGColorSpaceGetName(cg) : 0;
    for (u32 i = 0; n && i < NCS; i++) {
        if (!strncmp(cs_tab[i].sel, "device", 6)) continue;
        if (MSG(BOOL, (id)n, "isEqual:", (id)*cs_tab[i].cgname)) return cs_named((int)i);
    }
    if (!cg) return cs_named(0);
    id o = cx_alloc_instance(&NSColorSpace_c);
    return o ? objc_autorelease(cs_init_cg(o, 0, cg)) : cs_named(0);
}
static void colors_init(void) {
    cx_class_t *cl = cx_class_named("NSColor");
    cx_define_class(&NSColorSpace_c, &NSColorSpace_m, "NSColorSpace", cx_class_named("NSObject"), sizeof(nscs_t) - 8);
    for (u32 i = 0; i < NCS; i++) cx_add_cmethod(&NSColorSpace_c, cs_tab[i].sel, (void *)cs_shared);
    cx_add_cmethod(&NSColorSpace_c, "availableColorSpacesWithModel:", (void *)cs_available);
    cx_add_method(&NSColorSpace_c, "initWithCGColorSpace:", (void *)cs_init_cg);
    cx_add_method(&NSColorSpace_c, "initWithICCProfileData:", (void *)cs_init_icc);
    cx_add_method(&NSColorSpace_c, "CGColorSpace", (void *)cs_cg);
    cx_add_method(&NSColorSpace_c, "ICCProfileData", (void *)cs_icc);
    cx_add_method(&NSColorSpace_c, "numberOfColorComponents", (void *)cs_components);
    cx_add_method(&NSColorSpace_c, "colorSpaceModel", (void *)cs_model);
    cx_add_method(&NSColorSpace_c, "localizedName", (void *)cs_name);
    cx_add_method(&NSColorSpace_c, "isEqual:", (void *)cs_equal);
    cx_add_method(&NSColorSpace_c, "hash", (void *)cs_hash);
    cx_add_method(&NSColorSpace_c, "dealloc", (void *)cs_dealloc);
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
/* avisos de acessibilidade: vão aos observadores AX deste processo e, pelo servidor AX, aos de outros processos
 * (axobserver.c); sem elemento, o próprio app */
void cx_ax_note_view(id obj, const char *note);
void NSAccessibilityPostNotification(id element, id notification) {
    extern id NSApp;
    if (notification && cx_is_str(notification)) cx_ax_note_view(element ? element : NSApp, cx_cstr(notification));
}
void NSAccessibilityPostNotificationWithUserInfo(id element, id notification, id info) { (void)info; NSAccessibilityPostNotification(element, notification); }
/* pedido de anúncio (framework Accessibility do Swift: AccessibilityNotification.Announcement.post()); o transporte AX
 * do mOS leva só o nome do aviso, então o texto não chega aos observadores de outros processos */
void cx_ax_announce(const char *text, long priority) {
    (void)priority;
    extern id NSApp;
    if (text && *text) cx_ax_note_view(NSApp, "AXAnnouncementRequested");
}
void cx_ax_post_named(const char *note) { extern id NSApp; if (note && *note) cx_ax_note_view(NSApp, note); }
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
    cx_add_method(s, "colorSpace", (void *)sc_space);
}
