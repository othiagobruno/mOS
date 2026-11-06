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
static id sc_nil(id self, SEL c) { (void)self; (void)c; return 0; }

void cx_cgdisplay_init(void) {
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
    cx_add_method(s, "colorSpace", (void *)sc_nil);
}
