/*
 * Classes públicas do AppKit que os apps alvo chamam e o mOS ainda tratava como vazio:
 *   NSStepper, NSComboBox, NSLevelIndicator, NSPathControl — desenho, mouse e target/action.
 *   NSResponder tryToPerform:with: / noResponderFor: / insertText: sobem a cadeia
 *   (view → superview → janela → NSApp).
 *   NSAccessibility em NSView, NSControl, NSWindow e NSApplication (papel, rótulo,
 *   valor, filhos, quadro e AXPress).
 * O teste de host compila este arquivo com -DCX_AK_HOST e exercita a aritmética.
 */
#include "appkit_public.h"

double cx_step_apply(double v, double mn, double mx, double inc, int up) {
    if (inc < 0) inc = -inc;
    if (inc == 0) inc = 1;
    if (mx < mn) { double t = mn; mn = mx; mx = t; }
    v += up ? inc : -inc;
    if (v < mn) v = mn;
    if (v > mx) v = mx;
    return v;
}

int cx_path_split(const char *path, char parts[][CX_PATH_PART], int max) {
    const char *s = path ? path : "";
    int n = 0;
    if (s[0] == 'f' && s[1] == 'i' && s[2] == 'l' && s[3] == 'e' && s[4] == ':') {
        s += 5;
        while (*s == '/') s++;
    }
    while (*s == '/') s++;
    while (*s && n < max) {
        int k = 0;
        while (*s && *s != '/' && k < CX_PATH_PART - 1) parts[n][k++] = *s++;
        parts[n][k] = 0;
        if (k) n++;
        while (*s == '/') s++;
    }
    return n;
}

const char *cx_ax_role(const char *cls) {
    if (!cls) return "AXGroup";
    if (!strcmp(cls, "NSButton") || !strcmp(cls, "NSPopUpButton") || !strcmp(cls, "NSStatusBarButton")) return "AXButton";
    if (!strcmp(cls, "NSTextField") || !strcmp(cls, "NSSecureTextField") || !strcmp(cls, "NSSearchField") || !strcmp(cls, "NSTokenField") || !strcmp(cls, "NSComboBox")) return "AXTextField";
    if (!strcmp(cls, "NSTextView")) return "AXTextArea";
    if (!strcmp(cls, "NSSlider") || !strcmp(cls, "NSStepper")) return "AXSlider";
    if (!strcmp(cls, "NSProgressIndicator") || !strcmp(cls, "NSLevelIndicator")) return "AXProgressIndicator";
    if (!strcmp(cls, "NSTableView") || !strcmp(cls, "NSOutlineView")) return "AXTable";
    if (!strcmp(cls, "NSScrollView")) return "AXScrollArea";
    if (!strcmp(cls, "NSImageView")) return "AXImage";
    if (!strcmp(cls, "NSPathControl")) return "AXButton";
    if (!strcmp(cls, "NSWindow") || !strcmp(cls, "NSPanel")) return "AXWindow";
    if (!strcmp(cls, "NSApplication")) return "AXApplication";
    if (!strcmp(cls, "NSMenu")) return "AXMenu";
    if (!strcmp(cls, "NSMenuItem")) return "AXMenuItem";
    return "AXGroup";
}

int cx_chain_index(const int *next, int start, int count, int (*has)(int id, const char *sel, void *ctx), void *ctx, const char *sel) {
    int guard = 0;
    if (!next || !has || !sel || start < 0 || start >= count) return -1;
    while (start >= 0 && start < count && guard++ < count + 1) {
        if (has(start, sel, ctx)) return start;
        start = next[start];
    }
    return -1;
}

#ifndef CX_AK_HOST
#include "foundation.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void objc_setAssociatedObject(id obj, const void *key, id value, u64 policy);
id objc_getAssociatedObject(id obj, const void *key);
IMP class_getMethodImplementation(cx_class_t *c, SEL s);
void NSBeep(void);
extern id NSApp;

#define K(N) cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N); cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N);
K(NSStepper) K(NSComboBox) K(NSLevelIndicator) K(NSPathControl) K(NSPathComponentCell)

typedef struct { double x, y; } pt_t;
typedef struct { double w, h; } sz_t;
typedef struct { pt_t o; sz_t s; } rc_t;
void NSRectFill(rc_t r);

static cx_class_t *cl(const char *n) { return cx_class_named(n); }
static BOOL responds(id o, const char *s) { return o && cx_responds(object_getClass(o), sel_registerName(s)); }
static id next_resp(id self) { return responds(self, "nextResponder") ? MSG0(id, self, "nextResponder") : 0; }

static char pk;
static id props(id o) {
    id d = objc_getAssociatedObject(o, &pk);
    if (!d) {
        d = MSG0(id, MSG0(id, (id)cl("NSMutableDictionary"), "alloc"), "init");
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
static double pget_d(id o, const char *k, double d) { id v = pget(o, k); return v ? MSG0(double, v, "doubleValue") : d; }
static i64 pget_i(id o, const char *k, i64 d) { id v = pget(o, k); return v ? MSG0(i64, v, "longLongValue") : d; }
static void mark_view(id v) { if (responds(v, "setNeedsDisplay:")) MSG(void, v, "setNeedsDisplay:", (i64)1); }
static void fire_action(id self) {
    if (!responds(self, "sendAction:to:")) return;
    SEL a = responds(self, "action") ? MSG0(SEL, self, "action") : 0;
    id t = responds(self, "target") ? MSG0(id, self, "target") : 0;
    if (a) MSG(BOOL, self, "sendAction:to:", a, t);
}
static id super_init_frame(id self, const char *super, rc_t f) {
    SEL s = sel_registerName("initWithFrame:");
    return ((id (*)(id, SEL, rc_t))class_getMethodImplementation(cl(super), s))(self, s, f);
}
static void fill_rect(rc_t r, double R, double G, double B, double A) {
    id c = MSG(id, (id)cl("NSColor"), "colorWithRed:green:blue:alpha:", R, G, B, A);
    MSG0(void, c, "setFill");
    NSRectFill(r);
}
static void draw_str(const char *t, rc_t r) {
    if (!t) return;
    id s = cx_str(t);
    id attrs = MSG0(id, (id)cl("NSMutableDictionary"), "dictionary");
    MSG(void, s, "drawInRect:withAttributes:", r, attrs);
}

/* ---- cadeia de resposta */
static BOOL try_perform(id self, SEL c, SEL sel, id arg) {
    (void)c;
    if (!self || !sel) return NO;
    IMP mine = cx_lookup(object_getClass(self), sel_registerName("tryToPerform:with:"));
    IMP imp = cx_lookup(object_getClass(self), sel);
    if (imp && imp != (IMP)try_perform && !(mine && imp == mine)) {
        ((void (*)(id, SEL, id))objc_msgSend)(self, sel, arg);
        return YES;
    }
    id n = next_resp(self);
    if (n && n != self && responds(n, "tryToPerform:with:")) return MSG(BOOL, n, "tryToPerform:with:", sel, arg);
    return NO;
}
static void no_responder(id self, SEL c, SEL sel) {
    (void)c;
    id n = next_resp(self);
    if (n && n != self && responds(n, "noResponderFor:")) { MSG(void, n, "noResponderFor:", sel); return; }
    if (self == NSApp) NSBeep();
}
static void insert_fwd(id self, SEL c, id text) {
    (void)c;
    id n = next_resp(self);
    if (n && n != self && responds(n, "insertText:")) MSG(void, n, "insertText:", text);
}
static void evt_fwd(id self, SEL c, id e) {
    id n = next_resp(self);
    if (n && n != self) objc_msgSend(n, c, e);
}

/* ---- NSStepper */
static id step_init(id self, SEL c, rc_t f) {
    (void)c;
    self = super_init_frame(self, "NSControl", f);
    if (!self) return 0;
    pset(self, "min", cx_num_d(0));
    pset(self, "max", cx_num_d(100));
    pset(self, "inc", cx_num_d(1));
    MSG(void, self, "setDoubleValue:", 0.0);
    return self;
}
static void step_mouse(id self, SEL c, id e) {
    (void)c; (void)e;
    if (responds(self, "isEnabled") && !MSG0(BOOL, self, "isEnabled")) return;
    rc_t b = MSG0(rc_t, self, "bounds");
    pt_t p = e && responds(e, "locationInWindow") ? MSG0(pt_t, e, "locationInWindow") : (pt_t){0, 0};
    pt_t local = responds(self, "convertPoint:fromView:") ? MSG(pt_t, self, "convertPoint:fromView:", p, (id)0) : p;
    int up = local.y >= b.s.h / 2;
    double v = cx_step_apply(MSG0(double, self, "doubleValue"), pget_d(self, "min", 0), pget_d(self, "max", 100), pget_d(self, "inc", 1), up);
    MSG(void, self, "setDoubleValue:", v);
    mark_view(self);
    fire_action(self);
}
static void step_draw(id self, SEL c, rc_t r) {
    (void)c;
    fill_rect(r, 0.95, 0.95, 0.97, 1);
    rc_t top = {{r.o.x + 1, r.o.y + r.s.h / 2}, {r.s.w - 2, r.s.h / 2 - 1}};
    rc_t bot = {{r.o.x + 1, r.o.y + 1}, {r.s.w - 2, r.s.h / 2 - 2}};
    fill_rect(top, 1, 1, 1, 1);
    fill_rect(bot, 1, 1, 1, 1);
    draw_str("+", top);
    draw_str("-", bot);
}
static double step_min(id s, SEL c) { (void)c; return pget_d(s, "min", 0); }
static void step_set_min(id s, SEL c, double v) { (void)c; pset(s, "min", cx_num_d(v)); }
static double step_max(id s, SEL c) { (void)c; return pget_d(s, "max", 100); }
static void step_set_max(id s, SEL c, double v) { (void)c; pset(s, "max", cx_num_d(v)); }
static double step_inc(id s, SEL c) { (void)c; return pget_d(s, "inc", 1); }
static void step_set_inc(id s, SEL c, double v) { (void)c; pset(s, "inc", cx_num_d(v)); }
static sz_t step_intrinsic(id s, SEL c) { (void)s; (void)c; sz_t z = {19, 28}; return z; }

/* ---- NSComboBox */
static id combo_items(id self) {
    id a = pget(self, "items");
    if (!a) { a = MSG0(id, MSG0(id, (id)cl("NSMutableArray"), "alloc"), "init"); pset(self, "items", a); }
    return a;
}
static id combo_init(id self, SEL c, rc_t f) {
    (void)c;
    self = super_init_frame(self, "NSTextField", f);
    if (self) pset(self, "index", cx_num_i(-1));
    return self;
}
static void combo_add(id self, SEL c, id v) {
    (void)c;
    if (v) MSG(void, combo_items(self), "addObject:", v);
    mark_view(self);
}
static void combo_remove_all(id self, SEL c) { (void)c; MSG0(void, combo_items(self), "removeAllObjects"); pset(self, "index", cx_num_i(-1)); }
static i64 combo_count(id self, SEL c) { (void)c; return (i64)MSG0(u64, combo_items(self), "count"); }
static i64 combo_index(id self, SEL c) { (void)c; return pget_i(self, "index", -1); }
static void combo_select(id self, SEL c, i64 i) {
    (void)c;
    id items = combo_items(self);
    u64 n = MSG0(u64, items, "count");
    if (i < 0 || (u64)i >= n) { pset(self, "index", cx_num_i(-1)); return; }
    pset(self, "index", cx_num_i(i));
    id v = MSG(id, items, "objectAtIndex:", (u64)i);
    if (responds(self, "setStringValue:")) MSG(void, self, "setStringValue:", v);
    mark_view(self);
}
static id combo_object(id self, SEL c) {
    (void)c;
    i64 i = combo_index(self, 0);
    id items = combo_items(self);
    if (i < 0 || (u64)i >= MSG0(u64, items, "count")) return 0;
    return MSG(id, items, "objectAtIndex:", (u64)i);
}
static void combo_mouse(id self, SEL c, id e) {
    (void)c; (void)e;
    u64 n = MSG0(u64, combo_items(self), "count");
    if (!n) return;
    i64 i = combo_index(self, 0);
    combo_select(self, 0, (i64)(((i < 0 ? 0 : i) + 1) % (i64)n));
    fire_action(self);
}
static BOOL combo_completes(id s, SEL c) { (void)s; (void)c; return YES; }
static void combo_set_completes(id s, SEL c, BOOL v) { (void)s; (void)c; (void)v; }
static void combo_draw(id self, SEL c, rc_t r) {
    SEL s = sel_registerName("drawRect:");
    ((void (*)(id, SEL, rc_t))class_getMethodImplementation(cl("NSTextField"), s))(self, s, r);
    rc_t arrow = {{r.o.x + r.s.w - 18, r.o.y + 2}, {16, r.s.h - 4}};
    fill_rect(arrow, 0.90, 0.90, 0.93, 1);
    draw_str("v", arrow);
}

/* ---- NSLevelIndicator */
static id lvl_init(id self, SEL c, rc_t f) {
    (void)c;
    self = super_init_frame(self, "NSControl", f);
    if (!self) return 0;
    pset(self, "min", cx_num_d(0));
    pset(self, "max", cx_num_d(1));
    pset(self, "warn", cx_num_d(0.7));
    pset(self, "crit", cx_num_d(0.9));
    return self;
}
static void lvl_draw(id self, SEL c, rc_t r) {
    (void)c;
    double mn = pget_d(self, "min", 0), mx = pget_d(self, "max", 1), v = MSG0(double, self, "doubleValue");
    double t = mx > mn ? (v - mn) / (mx - mn) : 0;
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    double warn = pget_d(self, "warn", 0.7), crit = pget_d(self, "crit", 0.9);
    double frac = mx > mn ? (v - mn) / (mx - mn) : 0;
    fill_rect(r, 0.90, 0.90, 0.92, 1);
    rc_t bar = r;
    bar.s.w = r.s.w * t;
    if (frac >= crit) fill_rect(bar, 0.86, 0.22, 0.20, 1);
    else if (frac >= warn) fill_rect(bar, 0.92, 0.62, 0.10, 1);
    else fill_rect(bar, 0.20, 0.65, 0.32, 1);
}
static double lvl_min(id s, SEL c) { (void)c; return pget_d(s, "min", 0); }
static void lvl_set_min(id s, SEL c, double v) { (void)c; pset(s, "min", cx_num_d(v)); mark_view(s); }
static double lvl_max(id s, SEL c) { (void)c; return pget_d(s, "max", 1); }
static void lvl_set_max(id s, SEL c, double v) { (void)c; pset(s, "max", cx_num_d(v)); mark_view(s); }
static double lvl_warn(id s, SEL c) { (void)c; return pget_d(s, "warn", 0.7); }
static void lvl_set_warn(id s, SEL c, double v) { (void)c; pset(s, "warn", cx_num_d(v)); mark_view(s); }
static double lvl_crit(id s, SEL c) { (void)c; return pget_d(s, "crit", 0.9); }
static void lvl_set_crit(id s, SEL c, double v) { (void)c; pset(s, "crit", cx_num_d(v)); mark_view(s); }

/* ---- NSPathControl */
static id path_init(id self, SEL c, rc_t f) {
    (void)c;
    self = super_init_frame(self, "NSControl", f);
    if (self) pset(self, "clicked", cx_num_i(-1));
    return self;
}
static void path_set_url(id self, SEL c, id url) {
    (void)c;
    id s = url && responds(url, "path") ? MSG0(id, url, "path") : url;
    pset(self, "path", s);
    pset(self, "clicked", cx_num_i(-1));
    mark_view(self);
}
static id path_url(id self, SEL c) {
    (void)c;
    id s = pget(self, "path");
    if (!s) return 0;
    return MSG(id, (id)cl("NSURL"), "fileURLWithPath:", s);
}
static id path_cells(id self, SEL c) {
    (void)c;
    id arr = MSG0(id, (id)cl("NSMutableArray"), "array");
    char parts[32][CX_PATH_PART];
    int n = cx_path_split(cx_cstr(pget(self, "path")), parts, 32);
    for (int i = 0; i < n; i++) {
        id cell = MSG0(id, MSG0(id, (id)&NSPathComponentCell_c, "alloc"), "init");
        if (!cell) continue;
        pset(cell, "title", cx_str(parts[i]));
        pset(cell, "index", cx_num_i(i));
        MSG(void, arr, "addObject:", cell);
    }
    return arr;
}
static id path_clicked(id self, SEL c) {
    (void)c;
    i64 i = pget_i(self, "clicked", -1);
    id cells = path_cells(self, 0);
    if (i < 0 || (u64)i >= MSG0(u64, cells, "count")) return 0;
    return MSG(id, cells, "objectAtIndex:", (u64)i);
}
static void path_mouse(id self, SEL c, id e) {
    (void)c;
    char parts[32][CX_PATH_PART];
    int n = cx_path_split(cx_cstr(pget(self, "path")), parts, 32);
    if (!n) return;
    rc_t b = MSG0(rc_t, self, "bounds");
    pt_t p = e && responds(e, "locationInWindow") ? MSG0(pt_t, e, "locationInWindow") : (pt_t){0, 0};
    pt_t local = responds(self, "convertPoint:fromView:") ? MSG(pt_t, self, "convertPoint:fromView:", p, (id)0) : p;
    double w = b.s.w > 0 ? b.s.w : 1;
    int i = (int)(local.x / (w / n));
    if (i < 0) i = 0;
    if (i >= n) i = n - 1;
    pset(self, "clicked", cx_num_i(i));
    mark_view(self);
    fire_action(self);
}
static void path_draw(id self, SEL c, rc_t r) {
    (void)c;
    fill_rect(r, 0.96, 0.96, 0.97, 1);
    char parts[32][CX_PATH_PART];
    int n = cx_path_split(cx_cstr(pget(self, "path")), parts, 32);
    if (!n) return;
    double w = r.s.w / n;
    i64 hit = pget_i(self, "clicked", -1);
    for (int i = 0; i < n; i++) {
        rc_t cell = {{r.o.x + w * i, r.o.y}, {w - 2, r.s.h}};
        if (i == hit) fill_rect(cell, 0.20, 0.45, 0.95, 1);
        draw_str(parts[i], cell);
    }
}
static id cell_title(id s, SEL c) { (void)c; return pget(s, "title"); }
static id cell_url(id s, SEL c) {
    (void)c;
    id t = pget(s, "title");
    return t ? MSG(id, (id)cl("NSURL"), "fileURLWithPath:", t) : 0;
}

/* ---- NSAccessibility */
static const char *role_of(id self) {
    const char *n = self && self->isa ? cx_class_name(self->isa) : 0;
    if (pget(self, "axRole")) return cx_cstr(pget(self, "axRole"));
    return cx_ax_role(n);
}
static id ax_role(id self, SEL c) { (void)c; return cx_str(role_of(self)); }
static void ax_set_role(id self, SEL c, id v) { (void)c; pset(self, "axRole", v); }
static id ax_label(id self, SEL c) {
    (void)c;
    id v = pget(self, "axLabel");
    if (v) return v;
    if (responds(self, "title")) { id t = MSG0(id, self, "title"); if (t) return t; }
    if (responds(self, "toolTip")) return MSG0(id, self, "toolTip");
    if (responds(self, "stringValue")) return MSG0(id, self, "stringValue");
    return 0;
}
static void ax_set_label(id self, SEL c, id v) { (void)c; pset(self, "axLabel", v); }
static id ax_value(id self, SEL c) {
    (void)c;
    id v = pget(self, "axValue");
    if (v) return v;
    if (responds(self, "stringValue")) return MSG0(id, self, "stringValue");
    return 0;
}
static void ax_set_value(id self, SEL c, id v) { (void)c; pset(self, "axValue", v); }
static id ax_children(id self, SEL c) {
    (void)c;
    if (responds(self, "contentView")) {
        id cv = MSG0(id, self, "contentView");
        if (cv && responds(cv, "subviews")) return MSG0(id, cv, "subviews");
    }
    if (responds(self, "subviews")) return MSG0(id, self, "subviews");
    if (self == NSApp && responds(self, "windows")) return MSG0(id, self, "windows");
    return MSG0(id, (id)cl("NSArray"), "array");
}
static id ax_parent(id self, SEL c) {
    (void)c;
    if (responds(self, "superview")) {
        id p = MSG0(id, self, "superview");
        if (p) return p;
    }
    if (responds(self, "window")) return MSG0(id, self, "window");
    return NSApp;
}
static rc_t ax_frame(id self, SEL c) {
    (void)c;
    if (responds(self, "frame")) return MSG0(rc_t, self, "frame");
    rc_t z = {{0, 0}, {0, 0}};
    return z;
}
static BOOL ax_element(id self, SEL c) {
    (void)c;
    const char *n = self && self->isa ? cx_class_name(self->isa) : "";
    if (!strcmp(n, "NSView") || !strcmp(n, "NSBox") || !strcmp(n, "NSStackView")) {
        if (responds(self, "subviews") && MSG0(u64, MSG0(id, self, "subviews"), "count") > 0) return NO;
    }
    return YES;
}
static BOOL ax_press(id self, SEL c) {
    (void)c;
    fire_action(self);
    if (responds(self, "performClick:")) { MSG(void, self, "performClick:", (id)0); return YES; }
    return responds(self, "action") && MSG0(SEL, self, "action") != 0;
}
static void add_ax(cx_class_t *k) {
    if (!k) return;
    if (!cx_responds(k, sel_registerName("accessibilityRole"))) cx_add_method(k, "accessibilityRole", (void *)ax_role);
    if (!cx_responds(k, sel_registerName("setAccessibilityRole:"))) cx_add_method(k, "setAccessibilityRole:", (void *)ax_set_role);
    if (!cx_responds(k, sel_registerName("accessibilityLabel"))) cx_add_method(k, "accessibilityLabel", (void *)ax_label);
    if (!cx_responds(k, sel_registerName("setAccessibilityLabel:"))) cx_add_method(k, "setAccessibilityLabel:", (void *)ax_set_label);
    if (!cx_responds(k, sel_registerName("accessibilityValue"))) cx_add_method(k, "accessibilityValue", (void *)ax_value);
    if (!cx_responds(k, sel_registerName("setAccessibilityValue:"))) cx_add_method(k, "setAccessibilityValue:", (void *)ax_set_value);
    if (!cx_responds(k, sel_registerName("accessibilityChildren"))) cx_add_method(k, "accessibilityChildren", (void *)ax_children);
    if (!cx_responds(k, sel_registerName("accessibilityParent"))) cx_add_method(k, "accessibilityParent", (void *)ax_parent);
    if (!cx_responds(k, sel_registerName("accessibilityFrame"))) cx_add_method(k, "accessibilityFrame", (void *)ax_frame);
    if (!cx_responds(k, sel_registerName("isAccessibilityElement"))) cx_add_method(k, "isAccessibilityElement", (void *)ax_element);
    if (!cx_responds(k, sel_registerName("accessibilityPerformPress"))) cx_add_method(k, "accessibilityPerformPress", (void *)ax_press);
}
static void add_miss(cx_class_t *k, const char *sel, void *imp) {
    if (k && !cx_responds(k, sel_registerName(sel))) cx_add_method(k, sel, imp);
}
static void def(cx_class_t *k, cx_class_t *m, const char *name, const char *super) {
    if (cl(name) && cl(name)->vtable) return;
    cx_class_t *s = cl(super);
    if (!s) s = cl("NSObject");
    cx_define_class(k, m, name, s, 8);
}

void cx_appkit_public_init(void) {
    cx_class_t *resp = cl("NSResponder");
    cx_class_t *app = cl("NSApplication");
    add_miss(resp, "tryToPerform:with:", (void *)try_perform);
    add_miss(resp, "noResponderFor:", (void *)no_responder);
    add_miss(resp, "insertText:", (void *)insert_fwd);
    add_miss(resp, "rightMouseUp:", (void *)evt_fwd);
    add_miss(resp, "otherMouseDown:", (void *)evt_fwd);
    add_miss(resp, "otherMouseUp:", (void *)evt_fwd);
    add_miss(resp, "otherMouseDragged:", (void *)evt_fwd);
    add_ax(cl("NSView"));
    add_ax(cl("NSWindow"));
    add_ax(app);
    add_ax(cl("NSMenuItem"));

    def(&NSStepper_c, &NSStepper_m, "NSStepper", "NSControl");
    cx_add_method(&NSStepper_c, "initWithFrame:", (void *)step_init);
    cx_add_method(&NSStepper_c, "mouseDown:", (void *)step_mouse);
    cx_add_method(&NSStepper_c, "drawRect:", (void *)step_draw);
    cx_add_method(&NSStepper_c, "minValue", (void *)step_min);
    cx_add_method(&NSStepper_c, "setMinValue:", (void *)step_set_min);
    cx_add_method(&NSStepper_c, "maxValue", (void *)step_max);
    cx_add_method(&NSStepper_c, "setMaxValue:", (void *)step_set_max);
    cx_add_method(&NSStepper_c, "increment", (void *)step_inc);
    cx_add_method(&NSStepper_c, "setIncrement:", (void *)step_set_inc);
    cx_add_method(&NSStepper_c, "intrinsicContentSize", (void *)step_intrinsic);

    def(&NSComboBox_c, &NSComboBox_m, "NSComboBox", "NSTextField");
    cx_add_method(&NSComboBox_c, "initWithFrame:", (void *)combo_init);
    cx_add_method(&NSComboBox_c, "addItemWithObjectValue:", (void *)combo_add);
    cx_add_method(&NSComboBox_c, "removeAllItems", (void *)combo_remove_all);
    cx_add_method(&NSComboBox_c, "numberOfItems", (void *)combo_count);
    cx_add_method(&NSComboBox_c, "indexOfSelectedItem", (void *)combo_index);
    cx_add_method(&NSComboBox_c, "selectItemAtIndex:", (void *)combo_select);
    cx_add_method(&NSComboBox_c, "objectValueOfSelectedItem", (void *)combo_object);
    cx_add_method(&NSComboBox_c, "mouseDown:", (void *)combo_mouse);
    cx_add_method(&NSComboBox_c, "completes", (void *)combo_completes);
    cx_add_method(&NSComboBox_c, "setCompletes:", (void *)combo_set_completes);
    cx_add_method(&NSComboBox_c, "drawRect:", (void *)combo_draw);

    def(&NSLevelIndicator_c, &NSLevelIndicator_m, "NSLevelIndicator", "NSControl");
    cx_add_method(&NSLevelIndicator_c, "initWithFrame:", (void *)lvl_init);
    cx_add_method(&NSLevelIndicator_c, "drawRect:", (void *)lvl_draw);
    cx_add_method(&NSLevelIndicator_c, "minValue", (void *)lvl_min);
    cx_add_method(&NSLevelIndicator_c, "setMinValue:", (void *)lvl_set_min);
    cx_add_method(&NSLevelIndicator_c, "maxValue", (void *)lvl_max);
    cx_add_method(&NSLevelIndicator_c, "setMaxValue:", (void *)lvl_set_max);
    cx_add_method(&NSLevelIndicator_c, "warningValue", (void *)lvl_warn);
    cx_add_method(&NSLevelIndicator_c, "setWarningValue:", (void *)lvl_set_warn);
    cx_add_method(&NSLevelIndicator_c, "criticalValue", (void *)lvl_crit);
    cx_add_method(&NSLevelIndicator_c, "setCriticalValue:", (void *)lvl_set_crit);

    def(&NSPathComponentCell_c, &NSPathComponentCell_m, "NSPathComponentCell", "NSObject");
    cx_add_method(&NSPathComponentCell_c, "title", (void *)cell_title);
    cx_add_method(&NSPathComponentCell_c, "URL", (void *)cell_url);
    def(&NSPathControl_c, &NSPathControl_m, "NSPathControl", "NSControl");
    cx_add_method(&NSPathControl_c, "initWithFrame:", (void *)path_init);
    cx_add_method(&NSPathControl_c, "setURL:", (void *)path_set_url);
    cx_add_method(&NSPathControl_c, "URL", (void *)path_url);
    cx_add_method(&NSPathControl_c, "pathItems", (void *)path_cells);
    cx_add_method(&NSPathControl_c, "clickedPathItem", (void *)path_clicked);
    cx_add_method(&NSPathControl_c, "mouseDown:", (void *)path_mouse);
    cx_add_method(&NSPathControl_c, "drawRect:", (void *)path_draw);
}
#endif
