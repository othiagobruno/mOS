/* AVKit em cima do AVPlayer de avfaudio.c. A janela e a view são as classes que o
 * AppKit ou o UIKit do mOS já registraram. Este arquivo não cria NSView nem NSWindow. */
#include "foundation.h"
#include "avf_core.h"

void objc_setAssociatedObject(id o, const void *key, id value, int policy);
id objc_getAssociatedObject(id o, const void *key);

static const char k_player = 0, k_shows = 0, k_view = 0, k_delegate = 0;

avk_transport_t cx_avplayer_transport(id player, int controls);

static cx_class_t *super_named(const char *a, const char *b) {
    cx_class_t *s = cx_class_named(a);
    if (!s && b) s = cx_class_named(b);
    if (!s) s = cx_class_named("NSObject");
    return s;
}
static id existing_view(void) {
    cx_class_t *v = cx_class_named("NSView");
    if (!v) v = cx_class_named("UIView");
    if (!v) return 0;
    return MSG0(id, MSG0(id, (id)v, "alloc"), "init");
}
static int shows_of(id s) {
    id n = objc_getAssociatedObject(s, &k_shows);
    return n ? MSG0(int, n, "intValue") : 1;
}

cx_class_t AVPlayerViewController_c __asm__("OBJC_CLASS_$_AVPlayerViewController");
cx_class_t AVPlayerViewController_m __asm__("OBJC_METACLASS_$_AVPlayerViewController");
cx_class_t AVPictureInPictureController_c __asm__("OBJC_CLASS_$_AVPictureInPictureController");
cx_class_t AVPictureInPictureController_m __asm__("OBJC_METACLASS_$_AVPictureInPictureController");
cx_class_t AVRoutePickerView_c __asm__("OBJC_CLASS_$_AVRoutePickerView");
cx_class_t AVRoutePickerView_m __asm__("OBJC_METACLASS_$_AVRoutePickerView");

static id pvc_init(id s, SEL c) {
    (void)c;
    objc_setAssociatedObject(s, &k_shows, cx_num_i(1), 1);
    return s;
}
static id pvc_init_player(id s, SEL c, id player) {
    pvc_init(s, c);
    if (player) objc_setAssociatedObject(s, &k_player, player, 1);
    return s;
}
static id pvc_player(id s, SEL c) { (void)c; return objc_getAssociatedObject(s, &k_player); }
static void pvc_set_player(id s, SEL c, id p) { (void)c; objc_setAssociatedObject(s, &k_player, p, 1); }
static BOOL pvc_shows(id s, SEL c) { (void)c; return shows_of(s) ? YES : NO; }
static void pvc_set_shows(id s, SEL c, BOOL v) { (void)c; objc_setAssociatedObject(s, &k_shows, cx_num_i(v ? 1 : 0), 1); }
static id pvc_view(id s, SEL c) {
    (void)c;
    id v = objc_getAssociatedObject(s, &k_view);
    if (v) return v;
    v = existing_view();
    if (v) objc_setAssociatedObject(s, &k_view, v, 1);
    return v;
}
static void pvc_play(id s, SEL c) {
    (void)c;
    id p = pvc_player(s, 0);
    avk_transport_t t = cx_avplayer_transport(p, shows_of(s));
    if (!avk_play(&t)) return;
    MSG(void, p, "setRate:", 1.0f);
}
static void pvc_pause(id s, SEL c) {
    (void)c;
    id p = pvc_player(s, 0);
    avk_transport_t t = cx_avplayer_transport(p, shows_of(s));
    if (!avk_pause(&t)) return;
    MSG(void, p, "setRate:", 0.0f);
}
typedef struct { long long value; int timescale; unsigned flags; long long epoch; } avk_cmtime_t;
static double cm_sec(avk_cmtime_t t) { return t.timescale > 0 && (t.flags & 1) ? (double)t.value / t.timescale : 0; }
static void pvc_seek(id s, SEL c, avk_cmtime_t time) {
    (void)c;
    id p = pvc_player(s, 0);
    avk_transport_t t = cx_avplayer_transport(p, shows_of(s));
    avk_seek(&t, cm_sec(time));
}
static BOOL pip_supported(id k, SEL c) { (void)k; (void)c; return avk_pip_supported() ? YES : NO; }
static BOOL pip_active(id s, SEL c) { (void)s; (void)c; return NO; }
static id pip_delegate(id s, SEL c) { (void)c; return objc_getAssociatedObject(s, &k_delegate); }
static void pip_set_delegate(id s, SEL c, id d) { (void)c; objc_setAssociatedObject(s, &k_delegate, d, 0); }
static void pip_start(id s, SEL c) {
    (void)c;
    int code = 0;
    avk_pip_start(&code);
    id d = pip_delegate(s, 0);
    if (!d || !cx_responds(object_getClass(d), sel_registerName("pictureInPictureController:failedToStartPictureInPictureWithError:"))) return;
    id info = MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary");
    if (info) MSG(void, info, "setObject:forKey:", cx_str("Picture-in-picture não está disponível no mOS."), cx_str("NSLocalizedDescription"));
    id err = MSG(id, (id)cx_class_named("NSError"), "errorWithDomain:code:userInfo:", cx_str("AVFoundationErrorDomain"), (long long)code, info);
    MSG(void, d, "pictureInPictureController:failedToStartPictureInPictureWithError:", s, err);
}
static void pip_stop(id s, SEL c) { (void)s; (void)c; }
static int route_count(id s, SEL c) { (void)s; (void)c; return avk_route_count(); }
static id route_port(id s, SEL c, int i) { (void)s; (void)c; const char *p = avk_route_port(i); return p ? cx_str(p) : 0; }
static id route_name(id s, SEL c, int i) { (void)s; (void)c; const char *p = avk_route_name(i); return p ? cx_str(p) : 0; }

void cx_avkit_init(void) {
    cx_class_t *obj = cx_class_named("NSObject");
    if (!AVPlayerViewController_c.vtable)
        cx_define_class(&AVPlayerViewController_c, &AVPlayerViewController_m, "AVPlayerViewController", super_named("NSViewController", "UIViewController"), 0);
    cx_add_method(&AVPlayerViewController_c, "init", (void *)pvc_init);
    cx_add_method(&AVPlayerViewController_c, "initWithPlayer:", (void *)pvc_init_player);
    cx_add_method(&AVPlayerViewController_c, "player", (void *)pvc_player);
    cx_add_method(&AVPlayerViewController_c, "setPlayer:", (void *)pvc_set_player);
    cx_add_method(&AVPlayerViewController_c, "showsPlaybackControls", (void *)pvc_shows);
    cx_add_method(&AVPlayerViewController_c, "setShowsPlaybackControls:", (void *)pvc_set_shows);
    cx_add_method(&AVPlayerViewController_c, "view", (void *)pvc_view);
    cx_add_method(&AVPlayerViewController_c, "contentOverlayView", (void *)pvc_view);
    cx_add_method(&AVPlayerViewController_c, "play:", (void *)pvc_play);
    cx_add_method(&AVPlayerViewController_c, "pause:", (void *)pvc_pause);
    cx_add_method(&AVPlayerViewController_c, "seekToTime:", (void *)pvc_seek);
    if (!AVPictureInPictureController_c.vtable)
        cx_define_class(&AVPictureInPictureController_c, &AVPictureInPictureController_m, "AVPictureInPictureController", obj, 0);
    cx_add_cmethod(&AVPictureInPictureController_c, "isPictureInPictureSupported", (void *)pip_supported);
    cx_add_method(&AVPictureInPictureController_c, "isPictureInPictureActive", (void *)pip_active);
    cx_add_method(&AVPictureInPictureController_c, "delegate", (void *)pip_delegate);
    cx_add_method(&AVPictureInPictureController_c, "setDelegate:", (void *)pip_set_delegate);
    cx_add_method(&AVPictureInPictureController_c, "player", (void *)pvc_player);
    cx_add_method(&AVPictureInPictureController_c, "setPlayer:", (void *)pvc_set_player);
    cx_add_method(&AVPictureInPictureController_c, "startPictureInPicture", (void *)pip_start);
    cx_add_method(&AVPictureInPictureController_c, "stopPictureInPicture", (void *)pip_stop);
    if (!AVRoutePickerView_c.vtable)
        cx_define_class(&AVRoutePickerView_c, &AVRoutePickerView_m, "AVRoutePickerView", super_named("NSView", "UIView"), 0);
    cx_add_method(&AVRoutePickerView_c, "numberOfRoutes", (void *)route_count);
    cx_add_method(&AVRoutePickerView_c, "routePortAtIndex:", (void *)route_port);
    cx_add_method(&AVRoutePickerView_c, "routeNameAtIndex:", (void *)route_name);
}
