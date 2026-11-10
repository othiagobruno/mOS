/* Símbolos de AppKit que o machocheck --disco ainda marcava no Claude e no ChatGPT.
 * Constantes são CFString de verdade. As classes guardam o estado e devolvem o que receberam. */
#include "foundation.h"
#include "appkit_electron.h"
void objc_setAssociatedObject(id obj, const void *key, id value, unsigned long policy);
id objc_getAssociatedObject(id obj, const void *key);
void objc_release(id o);
const char *sel_getName(SEL s);

extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; unsigned long flags; const char *s; unsigned long n; } el_str_t;
#define CS(name) static el_str_t name##_b = {cx_cfconst_isa, 0x7c8, #name, sizeof(#name) - 1}; const void *name = &name##_b;

CS(NSAccessibilityActivationPointAttribute)
CS(NSAccessibilityAttachmentTextAttribute)
CS(NSAccessibilityFontFamilyKey)
CS(NSAccessibilityFontNameKey)
CS(NSAccessibilityLinkTextAttribute)
CS(NSAccessibilityListItemIndexTextAttribute)
CS(NSAccessibilityListItemLevelTextAttribute)
CS(NSAccessibilityListItemPrefixTextAttribute)
CS(NSAccessibilitySuperscriptTextAttribute)
CS(NSAccessibilityHandleRole)
CS(NSAppearanceNameAccessibilityHighContrastVibrantDark)
CS(NSAppearanceNameAccessibilityHighContrastVibrantLight)
CS(NSAppearanceNameVibrantDark)
CS(NSAppearanceNameVibrantLight)
CS(NSApplicationWillResignActiveNotification)
CS(NSApplicationDidUnhideNotification)
CS(NSCharacterEncodingDocumentOption)
CS(NSDefaultFontExcludedDocumentAttribute)
CS(NSDocumentTypeDocumentOption)
CS(NSFontDescriptorSystemDesignSerif)
CS(NSFontFamilyAttribute)
CS(NSFontFeatureSettingsAttribute)
CS(NSFontTextStyleBody)
CS(NSFontTextStyleHeadline)
CS(NSFontTextStyleLargeTitle)
CS(NSFontTextStyleSubheadline)
CS(NSFontTextStyleTitle1)
CS(NSFontTextStyleTitle2)
CS(NSFontTextStyleTitle3)
CS(NSFontWidthTrait)
CS(NSKernAttributeName)
CS(NSStrikethroughColorAttributeName)
CS(NSTextListMarkerDecimal)
CS(NSTextListMarkerHyphen)
CS(NSTimeoutDocumentOption)
CS(NSToolTipAttributeName)
CS(NSWebArchiveTextDocumentType)
CS(NSWebResourceLoadDelegateDocumentOption)

const double NSFontWidthExpanded = CX_FONT_WIDTH_EXPANDED;
int cx_services_epoch;

void NSUpdateDynamicServices(void) {
    id app = (id)cx_class_named("NSApplication");
    cx_services_epoch++;
    if (!app) return;
    app = MSG0(id, app, "sharedApplication");
    if (app && cx_responds(app->isa, sel_registerName("servicesMenu")))
        (void)MSG0(id, app, "servicesMenu");
}

int cx_appkit_swift_main(int argc, char **argv) __asm__("$s6AppKit17NSApplicationMainys5Int32VAD_SpySpys4Int8VGSgGtF");
int NSApplicationMain(int argc, const char **argv);
int cx_appkit_swift_main(int argc, char **argv) { return NSApplicationMain(argc, (const char **)argv); }

#define K(N) cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N); cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N);
K(NSSplitViewController) K(NSSplitViewItem)
K(NSTextTab) K(NSTextList) K(NSTextContainer) K(NSTextLayoutManager) K(NSTextContentStorage)
K(NSClickGestureRecognizer)

static char pk;
static id props(id o) {
    id d = objc_getAssociatedObject(o, &pk);
    if (!d) {
        d = MSG0(id, MSG0(id, (id)cx_class_named("NSMutableDictionary"), "alloc"), "init");
        objc_setAssociatedObject(o, &pk, d, 1);
        objc_release(d);
    }
    return d;
}
static id pget(id o, const char *k) { return MSG(id, props(o), "objectForKey:", cx_str(k)); }
static void pset(id o, const char *k, id v) {
    if (v) MSG(void, props(o), "setObject:forKey:", v, cx_str(k));
    else MSG(void, props(o), "removeObjectForKey:", cx_str(k));
}
static id gobj(id s, SEL c) { (void)c; return pget(s, sel_getName(c)); }
static void sobj(id s, SEL c, id v) { const char *n = sel_getName(c); char b[64]; int i = 3; while (n[i] && n[i] != ':') { b[i - 3] = n[i]; i++; } b[i - 3] = 0; if (b[0] >= 'A' && b[0] <= 'Z') b[0] += 32; pset(s, b, v); }

static id items_of(id self) {
    id a = pget(self, "items");
    if (!a) { a = MSG0(id, (id)cx_class_named("NSMutableArray"), "array"); pset(self, "items", a); }
    return a;
}
static id svc_split(id self, SEL c) {
    id v;
    (void)c;
    v = pget(self, "splitView");
    if (!v) {
        cx_class_t *k = cx_class_named("NSSplitView");
        if (k) v = MSG(id, MSG0(id, (id)k, "alloc"), "init");
        pset(self, "splitView", v);
    }
    return v;
}
static id svc_items(id self, SEL c) { (void)c; return items_of(self); }
static void svc_add(id self, SEL c, id item) {
    id view, split;
    (void)c;
    if (!item) return;
    MSG(void, items_of(self), "addObject:", item);
    view = pget(item, "viewController");
    view = view ? MSG0(id, view, "view") : 0;
    split = svc_split(self, 0);
    if (view && split) MSG(void, split, "addSubview:", view);
}
static void svc_remove(id self, SEL c, id item) {
    (void)c;
    if (item) MSG(void, items_of(self), "removeObject:", item);
}
static id svi_with(id cls, SEL c, id vc) {
    id s = MSG0(id, MSG0(id, cls, "alloc"), "init");
    (void)c;
    pset(s, "viewController", vc);
    pset(s, "holdingPriority", cx_num_d(250));
    return s;
}
static void svi_set_collapsed(id self, SEL c, int on) {
    id vc, view;
    (void)c;
    pset(self, "collapsed", cx_num_i(on ? 1 : 0));
    vc = pget(self, "viewController");
    view = vc ? MSG0(id, vc, "view") : 0;
    if (view) MSG(void, view, "setHidden:", on ? 1 : 0);
}
static int svi_collapsed(id self, SEL c) { (void)c; id v = pget(self, "collapsed"); return v ? (int)MSG0(long long, v, "longLongValue") : 0; }

typedef struct { double x, y, w, h; } el_rect_t;
typedef struct { double w, h; } el_size_t;
static id tab_init(id self, SEL c, long long align, double loc, id opt) {
    (void)c;
    self = MSG0(id, self, "init");
    pset(self, "alignment", cx_num_i(align));
    pset(self, "location", cx_num_d(loc));
    pset(self, "options", opt);
    return self;
}
static long long tab_align(id s, SEL c) { (void)c; id v = pget(s, "alignment"); return v ? MSG0(long long, v, "longLongValue") : 0; }
static double tab_loc(id s, SEL c) { (void)c; id v = pget(s, "location"); return v ? MSG0(double, v, "doubleValue") : 0; }

static id list_init(id self, SEL c, id fmt, unsigned long opt, long long start) {
    (void)c;
    self = MSG0(id, self, "init");
    pset(self, "markerFormat", fmt ? fmt : (id)NSTextListMarkerDecimal);
    pset(self, "listOptions", cx_num_i((long long)opt));
    pset(self, "startingItemNumber", cx_num_i(start > 0 ? start : 1));
    return self;
}
static id list_init2(id self, SEL c, id fmt, unsigned long opt) { return list_init(self, c, fmt, opt, 1); }
static id list_marker(id self, SEL c, long long n) {
    char buf[64];
    id fmt = pget(self, "markerFormat");
    (void)c;
    cx_textlist_marker(fmt ? cx_cstr(fmt) : "{decimal}.", (int)n, buf, 64);
    return cx_str(buf);
}

static id box_init(id self, SEL c, el_size_t sz) {
    (void)c;
    self = MSG0(id, self, "init");
    pset(self, "width", cx_num_d(sz.w));
    pset(self, "height", cx_num_d(sz.h));
    pset(self, "pad", cx_num_d(5));
    return self;
}
static el_size_t box_size(id self, SEL c) {
    id w = pget(self, "width"), h = pget(self, "height");
    el_size_t z;
    (void)c;
    z.w = w ? MSG0(double, w, "doubleValue") : 0;
    z.h = h ? MSG0(double, h, "doubleValue") : 0;
    return z;
}
static void box_set_size(id self, SEL c, el_size_t sz) {
    (void)c;
    pset(self, "width", cx_num_d(sz.w));
    pset(self, "height", cx_num_d(sz.h));
}
static double box_pad(id self, SEL c) { (void)c; id v = pget(self, "pad"); return v ? MSG0(double, v, "doubleValue") : 5; }
static void box_set_pad(id self, SEL c, double p) { (void)c; pset(self, "pad", cx_num_d(p)); }
static el_rect_t box_frag(id self, SEL c, el_rect_t proposed, unsigned long idx, long long dir, el_rect_t *rest) {
    el_size_t sz = box_size(self, c);
    double w;
    (void)idx; (void)dir;
    cx_text_fragment(sz.w, box_pad(self, 0), proposed.w, &w);
    proposed.w = w;
    if (rest) *rest = proposed;
    return proposed;
}

static id click_init(id self, SEL c) { (void)c; self = MSG0(id, self, "init"); pset(self, "numberOfClicksRequired", cx_num_i(1)); return self; }
static void click_down(id self, SEL c, id ev) {
    long long need = 1, got;
    id v;
    (void)c;
    v = pget(self, "numberOfClicksRequired");
    if (v) need = MSG0(long long, v, "longLongValue");
    got = ev ? MSG0(long long, ev, "clickCount") : 1;
    pset(self, "state", cx_num_i(got >= need ? 3 : 1));
}

static void def(cx_class_t *k, cx_class_t *m, const char *name, const char *super) {
    cx_class_t *s;
    if (cx_class_named(name)) return;
    s = cx_class_named(super);
    if (!s) s = cx_class_named("NSObject");
    cx_define_class(k, m, name, s, 64);
}
void cx_appkit_electron_init(void) {
    def(&NSSplitViewController_c, &NSSplitViewController_m, "NSSplitViewController", "NSViewController");
    cx_add_method(&NSSplitViewController_c, "splitView", (void *)svc_split);
    cx_add_method(&NSSplitViewController_c, "splitViewItems", (void *)svc_items);
    cx_add_method(&NSSplitViewController_c, "addSplitViewItem:", (void *)svc_add);
    cx_add_method(&NSSplitViewController_c, "removeSplitViewItem:", (void *)svc_remove);
    def(&NSSplitViewItem_c, &NSSplitViewItem_m, "NSSplitViewItem", "NSObject");
    cx_add_cmethod(&NSSplitViewItem_c, "splitViewItemWithViewController:", (void *)svi_with);
    cx_add_method(&NSSplitViewItem_c, "viewController", (void *)gobj);
    cx_add_method(&NSSplitViewItem_c, "setViewController:", (void *)sobj);
    cx_add_method(&NSSplitViewItem_c, "setCollapsed:", (void *)svi_set_collapsed);
    cx_add_method(&NSSplitViewItem_c, "isCollapsed", (void *)svi_collapsed);
    cx_add_method(&NSSplitViewItem_c, "holdingPriority", (void *)gobj);
    cx_add_method(&NSSplitViewItem_c, "setHoldingPriority:", (void *)sobj);

    def(&NSTextTab_c, &NSTextTab_m, "NSTextTab", "NSObject");
    cx_add_method(&NSTextTab_c, "initWithTextAlignment:location:options:", (void *)tab_init);
    cx_add_method(&NSTextTab_c, "alignment", (void *)tab_align);
    cx_add_method(&NSTextTab_c, "location", (void *)tab_loc);
    cx_add_method(&NSTextTab_c, "options", (void *)gobj);

    def(&NSTextList_c, &NSTextList_m, "NSTextList", "NSObject");
    cx_add_method(&NSTextList_c, "initWithMarkerFormat:options:", (void *)list_init2);
    cx_add_method(&NSTextList_c, "initWithMarkerFormat:options:startingItemNumber:", (void *)list_init);
    cx_add_method(&NSTextList_c, "markerFormat", (void *)gobj);
    cx_add_method(&NSTextList_c, "markerForItemNumber:", (void *)list_marker);

    def(&NSTextContainer_c, &NSTextContainer_m, "NSTextContainer", "NSObject");
    cx_add_method(&NSTextContainer_c, "initWithSize:", (void *)box_init);
    cx_add_method(&NSTextContainer_c, "initWithContainerSize:", (void *)box_init);
    cx_add_method(&NSTextContainer_c, "size", (void *)box_size);
    cx_add_method(&NSTextContainer_c, "setSize:", (void *)box_set_size);
    cx_add_method(&NSTextContainer_c, "containerSize", (void *)box_size);
    cx_add_method(&NSTextContainer_c, "lineFragmentPadding", (void *)box_pad);
    cx_add_method(&NSTextContainer_c, "setLineFragmentPadding:", (void *)box_set_pad);
    cx_add_method(&NSTextContainer_c, "lineFragmentRectForProposedRect:atIndex:writingDirection:remainingRect:", (void *)box_frag);
    cx_add_method(&NSTextContainer_c, "textView", (void *)gobj);
    cx_add_method(&NSTextContainer_c, "setTextView:", (void *)sobj);

    def(&NSTextLayoutManager_c, &NSTextLayoutManager_m, "NSTextLayoutManager", "NSObject");
    cx_add_method(&NSTextLayoutManager_c, "textContainer", (void *)gobj);
    cx_add_method(&NSTextLayoutManager_c, "setTextContainer:", (void *)sobj);
    cx_add_method(&NSTextLayoutManager_c, "textContentStorage", (void *)gobj);
    cx_add_method(&NSTextLayoutManager_c, "setTextContentStorage:", (void *)sobj);

    def(&NSTextContentStorage_c, &NSTextContentStorage_m, "NSTextContentStorage", "NSObject");
    cx_add_method(&NSTextContentStorage_c, "attributedString", (void *)gobj);
    cx_add_method(&NSTextContentStorage_c, "setAttributedString:", (void *)sobj);
    cx_add_method(&NSTextContentStorage_c, "textStorage", (void *)gobj);
    cx_add_method(&NSTextContentStorage_c, "setTextStorage:", (void *)sobj);

    def(&NSClickGestureRecognizer_c, &NSClickGestureRecognizer_m, "NSClickGestureRecognizer", "NSGestureRecognizer");
    cx_add_method(&NSClickGestureRecognizer_c, "init", (void *)click_init);
    cx_add_method(&NSClickGestureRecognizer_c, "numberOfClicksRequired", (void *)gobj);
    cx_add_method(&NSClickGestureRecognizer_c, "setNumberOfClicksRequired:", (void *)sobj);
    cx_add_method(&NSClickGestureRecognizer_c, "mouseDown:", (void *)click_down);
    cx_add_method(&NSClickGestureRecognizer_c, "state", (void *)gobj);
}
