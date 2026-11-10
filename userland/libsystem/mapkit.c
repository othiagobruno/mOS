/* MapKit do WhatsApp. A view do mapa é subclasse da NSView ou da UIView que o
 * time de Interface já registrou. Este arquivo não cria NSWindow nem outra classe
 * de janela. Sem rede, MKLocalSearch e MKDirections devolvem NSError em
 * MKErrorDomain e nenhum resultado inventado. CoreLocation não é reimplementado. */
#include "foundation.h"
#include <string.h>

void *malloc(size_t);
void *calloc(size_t, size_t);
void free(void *);

extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; unsigned long long flags; const char *s; unsigned long long n; } mk_cf_t;
#define CFCONST(var, text) \
    static mk_cf_t var##_s = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; \
    const void *var = &var##_s;

CFCONST(MKErrorDomain, "MKErrorDomain")
CFCONST(MKLaunchOptionsDirectionsModeKey, "MKLaunchOptionsDirectionsModeKey")
CFCONST(MKLaunchOptionsDirectionsModeDriving, "MKLaunchOptionsDirectionsModeDriving")
CFCONST(MKLaunchOptionsMapCenterKey, "MKLaunchOptionsMapCenterKey")
CFCONST(MKLaunchOptionsMapSpanKey, "MKLaunchOptionsMapSpanKey")
CFCONST(MKAnnotationCalloutInfoDidChangeNotification, "MKAnnotationCalloutInfoDidChangeNotification")

typedef struct { double latitude, longitude; } coord_t;
typedef struct { double latitudeDelta, longitudeDelta; } span_t;
typedef struct { coord_t center; span_t span; } region_t;
typedef struct { double x, y; } mpt_t;
typedef struct { double width, height; } msz_t;
typedef struct { mpt_t origin; msz_t size; } mrect_t;
typedef struct { char domain[32]; int code; } mk_fail_t;
void mk_search_fail(mk_fail_t *);
void mk_directions_fail(mk_fail_t *);

extern cx_class_t MKLocalSearch_c __asm__("OBJC_CLASS_$_MKLocalSearch");
extern cx_class_t MKLocalSearch_m __asm__("OBJC_METACLASS_$_MKLocalSearch");
extern cx_class_t MKLocalSearchRequest_c __asm__("OBJC_CLASS_$_MKLocalSearchRequest");
extern cx_class_t MKLocalSearchRequest_m __asm__("OBJC_METACLASS_$_MKLocalSearchRequest");

#define CLS(N) cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N); cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N)
CLS(MKAnnotationView);
CLS(MKCircle);
CLS(MKCircleRenderer);
CLS(MKCompassButton);
CLS(MKDirections);
CLS(MKDirectionsRequest);
CLS(MKDistanceFormatter);
CLS(MKLocalPointsOfInterestRequest);
CLS(MKMapCamera);
CLS(MKMapItem);
CLS(MKMapSnapshotOptions);
CLS(MKMapSnapshotter);
CLS(MKMapView);
CLS(MKMarkerAnnotationView);
CLS(MKOverlayRenderer);
CLS(MKPinAnnotationView);
CLS(MKPlacemark);
CLS(MKPointAnnotation);
CLS(MKPointOfInterestFilter);
CLS(MKUserLocation);

enum { S_ANN = 1, S_PLACE, S_ITEM, S_SREQ, S_DREQ, S_VIEW, S_CIRC, S_CAM, S_USER, S_SNAP };
typedef struct {
    id obj;
    int kind;
    coord_t coord;
    double radius, heading, pitch, alt;
    region_t region;
    char a[96], b[96], c[64];
    id placemark, source, dest, mapview;
    id anns[16];
    int nann;
} slot_t;
static slot_t slots[96];

static slot_t *slot_of(id o) {
    int i;
    for (i = 0; i < 96; i++) if (slots[i].obj == o) return &slots[i];
    return 0;
}
static slot_t *slot_new(id o, int kind) {
    slot_t *s = slot_of(o);
    int i;
    if (s) { s->kind = kind; return s; }
    for (i = 0; i < 96; i++) if (!slots[i].obj) {
        memset(&slots[i], 0, sizeof slots[i]);
        slots[i].obj = o;
        slots[i].kind = kind;
        return &slots[i];
    }
    return 0;
}
static void copystr(char *d, int n, id o) {
    const char *s = o ? cx_cstr(o) : 0;
    int i = 0;
    if (!s) s = "";
    for (; s[i] && i < n - 1; i++) d[i] = s[i];
    d[i] = 0;
}
static id err_of(const mk_fail_t *e) {
    id ui = MSG(id, (id)cx_class_named("NSDictionary"), "dictionaryWithObject:forKey:", cx_str(e->domain), cx_str("NSLocalizedFailureReason"));
    return MSG(id, (id)cx_class_named("NSError"), "errorWithDomain:code:userInfo:", (id)MKErrorDomain, (long)e->code, ui);
}
static void call2(void *blk, id a, id b) {
    void (*inv)(void *, id, id);
    if (!blk) return;
    inv = *(void (**)(void *, id, id))((unsigned char *)blk + 16);
    if (inv) inv(blk, a, b);
}
static cx_class_t *super_view(void) {
    cx_class_t *s = cx_class_named("NSView");
    if (!s) s = cx_class_named("UIView");
    if (!s) s = cx_class_named("NSObject");
    return s;
}
static void ensure(cx_class_t *k, cx_class_t *m, const char *name, cx_class_t *sup) {
    if (!cx_class_named(name)) cx_define_class(k, m, name, sup ? sup : cx_class_named("NSObject"), 0);
}

static id ann_init(id s, SEL c) { (void)c; slot_new(s, S_ANN); return s; }
static void ann_setc(id s, SEL c, coord_t v) { slot_t *x = slot_new(s, S_ANN); (void)c; if (x) x->coord = v; }
static coord_t ann_c(id s, SEL c) { slot_t *x = slot_of(s); coord_t z = {0, 0}; (void)c; return x ? x->coord : z; }
static void ann_sett(id s, SEL c, id t) { slot_t *x = slot_new(s, S_ANN); (void)c; if (x) copystr(x->a, 96, t); }
static id ann_t(id s, SEL c) { slot_t *x = slot_of(s); (void)c; return cx_str(x ? x->a : ""); }
static void ann_sets(id s, SEL c, id t) { slot_t *x = slot_new(s, S_ANN); (void)c; if (x) copystr(x->b, 96, t); }
static id ann_s(id s, SEL c) { slot_t *x = slot_of(s); (void)c; return cx_str(x ? x->b : ""); }

static id place_init(id s, SEL c, coord_t v, id addr) {
    slot_t *x = slot_new(s, S_PLACE);
    (void)c; (void)addr;
    if (x) x->coord = v;
    return s;
}
static coord_t place_c(id s, SEL c) { return ann_c(s, c); }
static id place_title(id s, SEL c) { return ann_t(s, c); }
static void place_set_title(id s, SEL c, id t) { ann_sett(s, c, t); slot_t *x = slot_of(s); if (x) x->kind = S_PLACE; }
static id place_loc(id s, SEL c) { slot_t *x = slot_of(s); (void)c; return cx_str(x ? x->b : ""); }
static void place_set_loc(id s, SEL c, id t) { slot_t *x = slot_new(s, S_PLACE); (void)c; if (x) copystr(x->b, 96, t); }
static id place_country(id s, SEL c) { slot_t *x = slot_of(s); (void)c; return cx_str(x ? x->c : ""); }
static void place_set_country(id s, SEL c, id t) { slot_t *x = slot_new(s, S_PLACE); (void)c; if (x) copystr(x->c, 64, t); }

static id item_init(id s, SEL c, id pm) {
    slot_t *x = slot_new(s, S_ITEM);
    slot_t *p = slot_of(pm);
    (void)c;
    if (x) { x->placemark = pm; if (p) x->coord = p->coord; }
    return s;
}
static id item_pm(id s, SEL c) { slot_t *x = slot_of(s); (void)c; return x ? x->placemark : 0; }
static void item_name(id s, SEL c, id n) { slot_t *x = slot_new(s, S_ITEM); (void)c; if (x) copystr(x->a, 96, n); }
static id item_get_name(id s, SEL c) { return ann_t(s, c); }
static void item_phone(id s, SEL c, id n) { slot_t *x = slot_new(s, S_ITEM); (void)c; if (x) copystr(x->b, 96, n); }
static id item_get_phone(id s, SEL c) { slot_t *x = slot_of(s); (void)c; return cx_str(x ? x->b : ""); }
static BOOL item_open(id s, SEL c, id opts) { (void)s; (void)c; (void)opts; return NO; }

static id sreq_init(id s, SEL c) { (void)c; slot_new(s, S_SREQ); return s; }
static void sreq_q(id s, SEL c, id q) { slot_t *x = slot_new(s, S_SREQ); (void)c; if (x) copystr(x->a, 96, q); }
static id sreq_getq(id s, SEL c) { return ann_t(s, c); }
static void sreq_reg(id s, SEL c, region_t r) { slot_t *x = slot_new(s, S_SREQ); (void)c; if (x) x->region = r; }
static region_t sreq_getreg(id s, SEL c) { slot_t *x = slot_of(s); region_t z; memset(&z, 0, sizeof z); (void)c; return x ? x->region : z; }

static id search_init(id s, SEL c, id req) {
    slot_t *x = slot_new(s, S_SREQ);
    slot_t *r = slot_of(req);
    (void)c;
    if (x && r) { memcpy(x->a, r->a, sizeof x->a); x->region = r->region; }
    return s;
}
static void search_start(id s, SEL c, void *blk) {
    mk_fail_t e;
    (void)s; (void)c;
    mk_search_fail(&e);
    call2(blk, 0, err_of(&e));
}
static id dreq_init(id s, SEL c) { (void)c; slot_new(s, S_DREQ); return s; }
static void dreq_src(id s, SEL c, id it) { slot_t *x = slot_new(s, S_DREQ); (void)c; if (x) x->source = it; }
static void dreq_dst(id s, SEL c, id it) { slot_t *x = slot_new(s, S_DREQ); (void)c; if (x) x->dest = it; }
static id dreq_getsrc(id s, SEL c) { slot_t *x = slot_of(s); (void)c; return x ? x->source : 0; }
static id dreq_getdst(id s, SEL c) { slot_t *x = slot_of(s); (void)c; return x ? x->dest : 0; }
static id dir_init(id s, SEL c, id req) {
    slot_t *x = slot_new(s, S_DREQ);
    slot_t *r = slot_of(req);
    (void)c;
    if (x && r) { x->source = r->source; x->dest = r->dest; }
    return s;
}
static void dir_calc(id s, SEL c, void *blk) {
    mk_fail_t e;
    (void)s; (void)c;
    mk_directions_fail(&e);
    call2(blk, 0, err_of(&e));
}

static id view_init(id s, SEL c) { (void)c; slot_new(s, S_VIEW); return s; }
static void view_setreg(id s, SEL c, region_t r) { slot_t *x = slot_new(s, S_VIEW); (void)c; if (x) x->region = r; }
static region_t view_reg(id s, SEL c) { return sreq_getreg(s, c); }
static void view_add(id s, SEL c, id ann) {
    slot_t *x = slot_new(s, S_VIEW);
    (void)c;
    if (x && ann && x->nann < 16) x->anns[x->nann++] = ann;
}
static id view_anns(id s, SEL c) {
    slot_t *x = slot_of(s);
    id arr = MSG0(id, (id)cx_class_named("NSMutableArray"), "array");
    int i;
    (void)c;
    if (!x || !arr) return arr;
    for (i = 0; i < x->nann; i++) MSG(void, arr, "addObject:", x->anns[i]);
    return arr;
}

static id circ_c(id cls, SEL c, coord_t v, double radius) {
    id s = cx_alloc_instance((cx_class_t *)cls);
    slot_t *x;
    (void)c;
    x = slot_new(s, S_CIRC);
    if (x) { x->coord = v; x->radius = radius; }
    return s;
}
static double circ_r(id s, SEL c) { slot_t *x = slot_of(s); (void)c; return x ? x->radius : 0; }
static id cam_init(id s, SEL c) { (void)c; slot_new(s, S_CAM); return s; }
static void cam_center(id s, SEL c, coord_t v) { slot_t *x = slot_new(s, S_CAM); (void)c; if (x) x->coord = v; }
static coord_t cam_get(id s, SEL c) { return ann_c(s, c); }
static id user_init(id s, SEL c) { (void)c; slot_new(s, S_USER); return s; }
static coord_t user_c(id s, SEL c) {
    coord_t z;
    (void)s; (void)c;
    z.latitude = 0.0 / 0.0;
    z.longitude = 0.0 / 0.0;
    return z;
}
static id snap_init(id s, SEL c) { (void)c; slot_new(s, S_SNAP); return s; }
static void snap_start(id s, SEL c, void *blk) {
    mk_fail_t e;
    (void)s; (void)c;
    mk_search_fail(&e);
    call2(blk, 0, err_of(&e));
}
static id fmt_string(id s, SEL c, double meters) {
    char buf[64];
    int n;
    (void)s; (void)c;
    n = 0;
    if (meters < 1000) {
        int m = (int)(meters + 0.5);
        if (m < 0) m = 0;
        if (m >= 1000) { buf[n++] = '1'; buf[n++] = '0'; buf[n++] = '0'; buf[n++] = '0'; }
        else {
            char tmp[16]; int k = 0, t = m;
            if (t == 0) tmp[k++] = '0';
            while (t) { tmp[k++] = (char)('0' + t % 10); t /= 10; }
            while (k) buf[n++] = tmp[--k];
        }
        buf[n++] = ' '; buf[n++] = 'm';
    } else {
        int km = (int)(meters / 1000.0 + 0.5);
        char tmp[16]; int k = 0, t = km;
        if (t == 0) tmp[k++] = '0';
        while (t) { tmp[k++] = (char)('0' + t % 10); t /= 10; }
        while (k) buf[n++] = tmp[--k];
        buf[n++] = ' '; buf[n++] = 'k'; buf[n++] = 'm';
    }
    buf[n] = 0;
    return cx_str(buf);
}

static void addm(cx_class_t *k, const char *s, void *f) { cx_add_method(k, s, f); }
static void addc(cx_class_t *k, const char *s, void *f) { cx_add_cmethod(k, s, f); }

void cx_mapkit_init(void) {
    cx_class_t *obj = cx_class_named("NSObject");
    cx_class_t *view = super_view();
    ensure(&MKLocalSearch_c, &MKLocalSearch_m, "MKLocalSearch", obj);
    ensure(&MKLocalSearchRequest_c, &MKLocalSearchRequest_m, "MKLocalSearchRequest", obj);
    ensure(&MKPointAnnotation_c, &MKPointAnnotation_m, "MKPointAnnotation", obj);
    ensure(&MKPlacemark_c, &MKPlacemark_m, "MKPlacemark", obj);
    ensure(&MKMapItem_c, &MKMapItem_m, "MKMapItem", obj);
    ensure(&MKDirections_c, &MKDirections_m, "MKDirections", obj);
    ensure(&MKDirectionsRequest_c, &MKDirectionsRequest_m, "MKDirectionsRequest", obj);
    ensure(&MKMapView_c, &MKMapView_m, "MKMapView", view);
    ensure(&MKAnnotationView_c, &MKAnnotationView_m, "MKAnnotationView", view);
    ensure(&MKMarkerAnnotationView_c, &MKMarkerAnnotationView_m, "MKMarkerAnnotationView", &MKAnnotationView_c);
    ensure(&MKPinAnnotationView_c, &MKPinAnnotationView_m, "MKPinAnnotationView", &MKAnnotationView_c);
    ensure(&MKCircle_c, &MKCircle_m, "MKCircle", obj);
    ensure(&MKCircleRenderer_c, &MKCircleRenderer_m, "MKCircleRenderer", obj);
    ensure(&MKOverlayRenderer_c, &MKOverlayRenderer_m, "MKOverlayRenderer", obj);
    ensure(&MKCompassButton_c, &MKCompassButton_m, "MKCompassButton", view);
    ensure(&MKMapCamera_c, &MKMapCamera_m, "MKMapCamera", obj);
    ensure(&MKMapSnapshotter_c, &MKMapSnapshotter_m, "MKMapSnapshotter", obj);
    ensure(&MKMapSnapshotOptions_c, &MKMapSnapshotOptions_m, "MKMapSnapshotOptions", obj);
    ensure(&MKDistanceFormatter_c, &MKDistanceFormatter_m, "MKDistanceFormatter", obj);
    ensure(&MKLocalPointsOfInterestRequest_c, &MKLocalPointsOfInterestRequest_m, "MKLocalPointsOfInterestRequest", obj);
    ensure(&MKPointOfInterestFilter_c, &MKPointOfInterestFilter_m, "MKPointOfInterestFilter", obj);
    ensure(&MKUserLocation_c, &MKUserLocation_m, "MKUserLocation", obj);

    addm(&MKPointAnnotation_c, "init", (void *)ann_init);
    addm(&MKPointAnnotation_c, "setCoordinate:", (void *)ann_setc);
    addm(&MKPointAnnotation_c, "coordinate", (void *)ann_c);
    addm(&MKPointAnnotation_c, "setTitle:", (void *)ann_sett);
    addm(&MKPointAnnotation_c, "title", (void *)ann_t);
    addm(&MKPointAnnotation_c, "setSubtitle:", (void *)ann_sets);
    addm(&MKPointAnnotation_c, "subtitle", (void *)ann_s);

    addm(&MKPlacemark_c, "initWithCoordinate:addressDictionary:", (void *)place_init);
    addm(&MKPlacemark_c, "coordinate", (void *)place_c);
    addm(&MKPlacemark_c, "setTitle:", (void *)place_set_title);
    addm(&MKPlacemark_c, "title", (void *)place_title);
    addm(&MKPlacemark_c, "setLocality:", (void *)place_set_loc);
    addm(&MKPlacemark_c, "locality", (void *)place_loc);
    addm(&MKPlacemark_c, "setCountry:", (void *)place_set_country);
    addm(&MKPlacemark_c, "country", (void *)place_country);

    addm(&MKMapItem_c, "initWithPlacemark:", (void *)item_init);
    addm(&MKMapItem_c, "placemark", (void *)item_pm);
    addm(&MKMapItem_c, "setName:", (void *)item_name);
    addm(&MKMapItem_c, "name", (void *)item_get_name);
    addm(&MKMapItem_c, "setPhoneNumber:", (void *)item_phone);
    addm(&MKMapItem_c, "phoneNumber", (void *)item_get_phone);
    addm(&MKMapItem_c, "openInMapsWithLaunchOptions:", (void *)item_open);

    addm(&MKLocalSearchRequest_c, "init", (void *)sreq_init);
    addm(&MKLocalSearchRequest_c, "setNaturalLanguageQuery:", (void *)sreq_q);
    addm(&MKLocalSearchRequest_c, "naturalLanguageQuery", (void *)sreq_getq);
    addm(&MKLocalSearchRequest_c, "setRegion:", (void *)sreq_reg);
    addm(&MKLocalSearchRequest_c, "region", (void *)sreq_getreg);
    addm(&MKLocalSearch_c, "initWithRequest:", (void *)search_init);
    addm(&MKLocalSearch_c, "startWithCompletionHandler:", (void *)search_start);

    addm(&MKDirectionsRequest_c, "init", (void *)dreq_init);
    addm(&MKDirectionsRequest_c, "setSource:", (void *)dreq_src);
    addm(&MKDirectionsRequest_c, "setDestination:", (void *)dreq_dst);
    addm(&MKDirectionsRequest_c, "source", (void *)dreq_getsrc);
    addm(&MKDirectionsRequest_c, "destination", (void *)dreq_getdst);
    addm(&MKDirections_c, "initWithRequest:", (void *)dir_init);
    addm(&MKDirections_c, "calculateDirectionsWithCompletionHandler:", (void *)dir_calc);

    addm(&MKMapView_c, "init", (void *)view_init);
    addm(&MKMapView_c, "setRegion:", (void *)view_setreg);
    addm(&MKMapView_c, "region", (void *)view_reg);
    addm(&MKMapView_c, "addAnnotation:", (void *)view_add);
    addm(&MKMapView_c, "annotations", (void *)view_anns);
    addm(&MKAnnotationView_c, "init", (void *)view_init);
    addc(&MKCircle_c, "circleWithCenterCoordinate:radius:", (void *)circ_c);
    addm(&MKCircle_c, "radius", (void *)circ_r);
    addm(&MKCircle_c, "coordinate", (void *)ann_c);
    addm(&MKMapCamera_c, "init", (void *)cam_init);
    addm(&MKMapCamera_c, "setCenterCoordinate:", (void *)cam_center);
    addm(&MKMapCamera_c, "centerCoordinate", (void *)cam_get);
    addm(&MKUserLocation_c, "init", (void *)user_init);
    addm(&MKUserLocation_c, "coordinate", (void *)user_c);
    addm(&MKMapSnapshotter_c, "initWithOptions:", (void *)snap_init);
    addm(&MKMapSnapshotter_c, "startWithCompletionHandler:", (void *)snap_start);
    addm(&MKDistanceFormatter_c, "stringFromDistance:", (void *)fmt_string);
}
