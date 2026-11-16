/*
 * mOS libSystem — os últimos controles do AppKit:
 *   NSTokenField: objectValue é a lista de objetos representados (delegate representedObjectForEditingString: /
 *     displayStringForRepresentedObject:), separados pelo tokenizingCharacterSet; fora da edição os tokens se desenham
 *     como pílulas; shouldAddObjects:atIndex: filtra o que entra.
 *   NSPathControl: NSPathControlItem, pathStyle (padrão e pop-up com menu dos componentes), delegate, allowedTypes,
 *     placeholder, editável, doubleAction.
 *   NSBrowser: colunas com o delegate por item (rootItemForBrowser:, numberOfChildrenOfItem:, child:ofItem:,
 *     isLeafItem:, objectValueForItem:) ou o clássico (numberOfRowsInColumn:, willDisplayCell:atRow:column:); seleção
 *     por clique e setas, path/setPath:, índices, ação e duplo clique, títulos, larguras.
 *   NSRuleEditor: linhas de critérios pelo delegate (numberOfChildrenForCriterion:, child:forCriterion:,
 *     displayValueForCriterion:inRow:, predicatePartsForCriterion:...), pop-ups e views por linha, botões + e −,
 *     linhas compostas e simples, aninhamento e o NSPredicate montado das partes.
 *   NSPredicateEditor + NSPredicateEditorRowTemplate: modelos com expressões à esquerda, operadores e valor à direita
 *     (lista ou campo de texto), compostos E/OU/NÃO; objectValue é o NSPredicate.
 * Registro: cx_appkit_controls3_init(), chamado do cx_appkit_input_init.
 */
#include "foundation.h"
#include "sys.h"
#include "libc.h"

id objc_msgSend_stret(id self, SEL op, ...);
id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void objc_setAssociatedObject(id obj, const void *key, id value, u64 policy);
id objc_getAssociatedObject(id obj, const void *key);
IMP class_getMethodImplementation(cx_class_t *c, SEL s);
id cx_color(double r, double g, double b, double a);
void NSBeep(void);
extern id NSApp;
const char *sel_getName(SEL s);
extern id NSFontAttributeName, NSForegroundColorAttributeName;

typedef struct { double x, y; } P;
typedef struct { double w, h; } S;
typedef struct { double x, y, w, h; } R;
typedef struct { u64 loc, len; } RNG;
void NSRectFill(R r);

#define K(N) cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N); cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N);
K(NSPathControlItem) K(NSBrowser) K(NSRuleEditor) K(NSPredicateEditor) K(NSPredicateEditorRowTemplate)

static cx_class_t *kls(const char *n) { return cx_class_named(n); }
static SEL sl(const char *n) { return sel_registerName(n); }
static BOOL responds(id o, const char *s) { return o && cx_responds(object_getClass(o), sl(s)); }
static R msg_rect(id o, const char *s) { return ((R (*)(id, SEL))objc_msgSend_stret)(o, sl(s)); }
static R mkr(double x, double y, double w, double h) { R r = {x, y, w, h}; return r; }
static void redraw(id v) { MSG(void, v, "setNeedsDisplay:", (BOOL)YES); }
static char k_props, k_delegate;
static id props(id o) {
    id d = objc_getAssociatedObject(o, &k_props);
    if (!d) { d = MSG0(id, (id)kls("NSMutableDictionary"), "dictionary"); objc_setAssociatedObject(o, &k_props, d, 1); }
    return d;
}
static id pget(id o, const char *k) { return MSG(id, props(o), "objectForKey:", cx_str(k)); }
static void pset(id o, const char *k, id v) { if (v) MSG(void, props(o), "setObject:forKey:", v, cx_str(k)); else MSG(void, props(o), "removeObjectForKey:", cx_str(k)); }
static i64 pgi(id o, const char *k, i64 d) { id v = pget(o, k); return v ? MSG0(i64, v, "longLongValue") : d; }
static double pgd(id o, const char *k, double d) { id v = pget(o, k); return v ? MSG0(double, v, "doubleValue") : d; }
static void psi(id o, const char *k, i64 v) { pset(o, k, cx_num_i(v)); }
static void psd(id o, const char *k, double v) { pset(o, k, cx_num_d(v)); }
static id arr(void) { return MSG0(id, (id)kls("NSMutableArray"), "array"); }
static id dget(id self) { return objc_getAssociatedObject(self, &k_delegate); }
static id g_delegate(id self, SEL c) { (void)c; return dget(self); }
static void s_delegate(id self, SEL c, id d) { (void)c; objc_setAssociatedObject(self, &k_delegate, d, 0); }
static id font12(void) { return MSG(id, (id)kls("NSFont"), "systemFontOfSize:", 12.0); }
static id attrs(id color) {
    id d = MSG0(id, (id)kls("NSMutableDictionary"), "dictionary");
    MSG(void, d, "setObject:forKey:", font12(), NSFontAttributeName);
    MSG(void, d, "setObject:forKey:", color ? color : cx_color(0, 0, 0, 1), NSForegroundColorAttributeName);
    return d;
}
static S tsize(id s, id a) { return s ? ((S (*)(id, SEL, id))objc_msgSend)(s, sl("sizeWithAttributes:"), a) : (S){0, 0}; }
static void draw_at(id s, P p, id a) { if (s) ((void (*)(id, SEL, P, id))objc_msgSend)(s, sl("drawAtPoint:withAttributes:"), p, a); }
static id path_rr(R r, double rad) { return ((id (*)(id, SEL, R, double, double))objc_msgSend)((id)kls("NSBezierPath"), sl("bezierPathWithRoundedRect:xRadius:yRadius:"), r, rad, rad); }
static void fill_path(id p, id col) { MSG0(void, col, "setFill"); MSG0(void, p, "fill"); }
static P local(id v, id ev) { P w = ((P (*)(id, SEL))objc_msgSend)(ev, sl("locationInWindow")); return ((P (*)(id, SEL, P, id))objc_msgSend)(v, sl("convertPoint:fromView:"), w, (id)0); }
static BOOL flipped(id v) { return responds(v, "isFlipped") && MSG0(BOOL, v, "isFlipped"); }
static void send_action(id self, SEL a) { if (a) MSG(BOOL, self, "sendAction:to:", a, MSG0(id, self, "target")); }

/* ================================================================ NSTokenField */
static id tk_charset(id self) {
    id cs = pget(self, "tokChars");
    return cs ? cs : MSG(id, (id)kls("NSCharacterSet"), "characterSetWithCharactersInString:", cx_str(","));
}
static id tk_display(id self, id obj) {
    id d = dget(self);
    if (d && responds(d, "tokenField:displayStringForRepresentedObject:")) { id s = MSG(id, d, "tokenField:displayStringForRepresentedObject:", self, obj); if (s) return s; }
    return MSG(BOOL, obj, "isKindOfClass:", (id)kls("NSString")) ? obj : MSG0(id, obj, "description");
}
static id tk_objects(id self) {
    IMP sup = class_getMethodImplementation(kls("NSTextField"), sl("stringValue"));
    id s = ((id (*)(id, SEL))sup)(self, sl("stringValue"));
    id parts = MSG(id, s ? s : cx_str(""), "componentsSeparatedByCharactersInSet:", tk_charset(self));
    id out = arr(), d = dget(self);
    id ws = MSG0(id, (id)kls("NSCharacterSet"), "whitespaceAndNewlineCharacterSet");
    for (u64 i = 0, n = MSG0(u64, parts, "count"); i < n; i++) {
        id t = MSG(id, MSG(id, parts, "objectAtIndex:", i), "stringByTrimmingCharactersInSet:", ws);
        if (!MSG0(u64, t, "length")) continue;
        id obj = t;
        if (d && responds(d, "tokenField:representedObjectForEditingString:")) { id o = MSG(id, d, "tokenField:representedObjectForEditingString:", self, t); if (o) obj = o; }
        MSG(void, out, "addObject:", obj);
    }
    if (d && responds(d, "tokenField:shouldAddObjects:atIndex:")) {
        id ok = MSG(id, d, "tokenField:shouldAddObjects:atIndex:", self, out, (u64)0);
        if (ok) return ok;
    }
    return out;
}
static id tk_object_value(id self, SEL c) { (void)c; return tk_objects(self); }
static void tk_set_object_value(id self, SEL c, id v) {
    (void)c;
    id parts = arr();
    if (v && MSG(BOOL, v, "isKindOfClass:", (id)kls("NSArray")))
        for (u64 i = 0, n = MSG0(u64, v, "count"); i < n; i++) { id o = MSG(id, v, "objectAtIndex:", i); MSG(void, parts, "addObject:", tk_display(self, o)); }
    else if (v) MSG(void, parts, "addObject:", tk_display(self, v));
    id joined = MSG(id, parts, "componentsJoinedByString:", cx_str(", "));
    IMP sup = class_getMethodImplementation(kls("NSTextField"), sl("setStringValue:"));
    ((void (*)(id, SEL, id))sup)(self, sl("setStringValue:"), joined);
    redraw(self);
}
static BOOL is_editing(id self) { id w = MSG0(id, self, "window"); id fr = w ? MSG0(id, w, "firstResponder") : 0; return fr == self || (fr && responds(fr, "delegate") && MSG0(id, fr, "delegate") == self); }
static IMP tk_super_draw;
static void tk_draw(id self, SEL c, R dirty) {
    if (is_editing(self) || !tk_super_draw) { if (tk_super_draw) ((void (*)(id, SEL, R))tk_super_draw)(self, c, dirty); return; }
    R b = msg_rect(self, "bounds");
    MSG0(void, cx_color(1, 1, 1, 1), "setFill"); NSRectFill(b);
    MSG0(void, cx_color(0.75, 0.75, 0.77, 1), "set");
    id objs = tk_objects(self);
    id a = attrs(cx_color(0.1, 0.1, 0.12, 1));
    double x = 4;
    for (u64 i = 0, n = MSG0(u64, objs, "count"); i < n; i++) {
        id t = tk_display(self, MSG(id, objs, "objectAtIndex:", i));
        S z = tsize(t, a);
        R pill = mkr(x, (b.h - (z.h + 4)) / 2, z.w + 14, z.h + 4);
        if (pill.x + pill.w > b.w - 2) break;
        fill_path(path_rr(pill, pill.h / 2), cx_color(0.80, 0.88, 1.0, 1));
        draw_at(t, (P){pill.x + 7, pill.y + 2}, a);
        x += pill.w + 4;
    }
}
#define TKP(get, key, def) static i64 tkg_##get(id self, SEL c) { (void)c; return pgi(self, key, def); } static void tks_##get(id self, SEL c, i64 v) { (void)c; psi(self, key, v); redraw(self); }
TKP(style, "tokStyle", 1)
static double tk_delay(id self, SEL c) { (void)c; return pgd(self, "tokDelay", 0); }
static void tk_set_delay(id self, SEL c, double d) { (void)c; psd(self, "tokDelay", d); }
static id tk_get_charset(id self, SEL c) { (void)c; return tk_charset(self); }
static void tk_set_charset(id self, SEL c, id cs) { (void)c; pset(self, "tokChars", cs); }
static id tk_default_charset(id k, SEL c) { (void)k; (void)c; return MSG(id, (id)kls("NSCharacterSet"), "characterSetWithCharactersInString:", cx_str(",")); }
static double tk_default_delay(id k, SEL c) { (void)k; (void)c; return 0; }

/* ================================================================ NSPathControl */
static id pci_title(id self, SEL c) { (void)c; return pget(self, "title"); }
static void pci_set_title(id self, SEL c, id t) { (void)c; pset(self, "title", t); }
static id pci_image(id self, SEL c) { (void)c; return pget(self, "image"); }
static void pci_set_image(id self, SEL c, id i) { (void)c; pset(self, "image", i); }
static id pci_url(id self, SEL c) { (void)c; return pget(self, "url"); }
static id pci_attr_title(id self, SEL c) { (void)c; id t = pget(self, "title"); return t ? objc_autorelease(MSG(id, MSG0(id, (id)kls("NSAttributedString"), "alloc"), "initWithString:", t)) : 0; }
static void pci_set_attr_title(id self, SEL c, id a) { (void)c; pset(self, "title", a ? MSG0(id, a, "string") : 0); }
static id items_from_url(id url) {
    id out = arr();
    id comps = url ? MSG0(id, url, "pathComponents") : 0;
    id acc = cx_str("");
    for (u64 i = 0, n = comps ? MSG0(u64, comps, "count") : 0; i < n; i++) {
        id comp = MSG(id, comps, "objectAtIndex:", i);
        acc = i == 0 ? comp : MSG(id, acc, "stringByAppendingPathComponent:", comp);
        id it = objc_autorelease(cx_alloc_instance(&NSPathControlItem_c));
        pset(it, "title", i == 0 && MSG(BOOL, comp, "isEqualToString:", cx_str("/")) ? cx_str("Disco") : comp);
        pset(it, "url", MSG(id, (id)kls("NSURL"), "fileURLWithPath:", acc));
        MSG(void, out, "addObject:", it);
    }
    return out;
}
static id pc_items(id self, SEL c) { (void)c; id it = pget(self, "items"); if (it) return it; id u = MSG0(id, self, "URL"); return items_from_url(u); }
static void pc_set_items(id self, SEL c, id items) { (void)c; pset(self, "items", items); redraw(self); }
static i64 pc_style(id self, SEL c) { (void)c; return pgi(self, "style", 0); }
static void pc_set_style(id self, SEL c, i64 s) { (void)c; psi(self, "style", s); redraw(self); }
static id pc_allowed(id self, SEL c) { (void)c; return pget(self, "allowed"); }
static void pc_set_allowed(id self, SEL c, id a) { (void)c; pset(self, "allowed", a); }
static id pc_placeholder(id self, SEL c) { (void)c; return pget(self, "placeholder"); }
static void pc_set_placeholder(id self, SEL c, id s) { (void)c; pset(self, "placeholder", s); redraw(self); }
static id pc_bg(id self, SEL c) { (void)c; return pget(self, "bg"); }
static void pc_set_bg(id self, SEL c, id col) { (void)c; pset(self, "bg", col); redraw(self); }
static BOOL pc_editable(id self, SEL c) { (void)c; return pgi(self, "editable", 0) != 0; }
static void pc_set_editable(id self, SEL c, BOOL b) { (void)c; psi(self, "editable", b); }
static SEL pc_double(id self, SEL c) { (void)c; id v = pget(self, "dbl"); return v ? sl(cx_cstr(v)) : 0; }
static void pc_set_double(id self, SEL c, SEL a) { (void)c; pset(self, "dbl", a ? cx_str(sel_getName(a)) : 0); }
static IMP pc_super_mouse;
static void pc_mouse(id self, SEL c, id ev) {
    if (pgi(self, "style", 0) == 2) {    /* NSPathStylePopUp: menu com os componentes */
        id items = pc_items(self, 0);
        id menu = MSG(id, MSG0(id, (id)kls("NSMenu"), "alloc"), "initWithTitle:", cx_str(""));
        u64 n = MSG0(u64, items, "count");
        for (u64 i = n; i-- > 0;) {
            id it = MSG(id, items, "objectAtIndex:", i);
            id mi = MSG(id, menu, "addItemWithTitle:action:keyEquivalent:", MSG0(id, it, "title"), (SEL)0, cx_str(""));
            MSG(void, mi, "setRepresentedObject:", it);
        }
        id d = dget(self);
        if (d && responds(d, "pathControl:willPopUpMenu:")) MSG(void, d, "pathControl:willPopUpMenu:", self, menu);
        R b = msg_rect(self, "bounds");
        BOOL ok = ((BOOL (*)(id, SEL, id, P, id))objc_msgSend)(menu, sl("popUpMenuPositioningItem:atLocation:inView:"), (id)0, (P){0, b.h}, self);
        id chosen = ok && responds(menu, "highlightedItem") ? MSG0(id, menu, "highlightedItem") : 0;
        if (chosen) { pset(self, "clicked", MSG0(id, chosen, "representedObject")); send_action(self, MSG0(SEL, self, "action")); }
        objc_release(menu);
        return;
    }
    if (pc_super_mouse) ((void (*)(id, SEL, id))pc_super_mouse)(self, c, ev);
    if (MSG0(i64, ev, "clickCount") >= 2) send_action(self, pc_double(self, 0));
}

/* ================================================================ NSBrowser */
#define BR_ROW_H 19.0
static id br_cols(id self) { id a = pget(self, "sel"); if (!a) { a = arr(); pset(self, "sel", a); } return a; }   /* linha escolhida por coluna */
static BOOL item_api(id self) { id d = dget(self); return d && responds(d, "browser:numberOfChildrenOfItem:"); }
static id br_root(id self) { id d = dget(self); return d && responds(d, "rootItemForBrowser:") ? MSG(id, d, "rootItemForBrowser:", self) : 0; }
/* item que é o pai da coluna k (a raiz para k = 0) */
static id parent_item(id self, i64 k) {
    id item = br_root(self), d = dget(self);
    id sel = br_cols(self);
    for (i64 i = 0; i < k && (u64)i < MSG0(u64, sel, "count"); i++) {
        i64 r = MSG0(i64, MSG(id, sel, "objectAtIndex:", (u64)i), "longLongValue");
        item = MSG(id, d, "browser:child:ofItem:", self, r, item);
    }
    return item;
}
static i64 rows_in(id self, i64 k) {
    id d = dget(self);
    if (item_api(self)) return MSG(i64, d, "browser:numberOfChildrenOfItem:", self, parent_item(self, k));
    if (d && responds(d, "browser:numberOfRowsInColumn:")) return MSG(i64, d, "browser:numberOfRowsInColumn:", self, k);
    return 0;
}
static id br_item(id self, i64 r, i64 k) { id d = dget(self); return item_api(self) ? MSG(id, d, "browser:child:ofItem:", self, r, parent_item(self, k)) : 0; }
static BOOL br_leaf(id self, i64 r, i64 k) {
    id d = dget(self);
    if (item_api(self)) return responds(d, "browser:isLeafItem:") ? MSG(BOOL, d, "browser:isLeafItem:", self, br_item(self, r, k)) : NO;
    id cell = MSG(id, self, "loadedCellAtRow:column:", r, k);
    return cell && responds(cell, "isLeaf") ? MSG0(BOOL, cell, "isLeaf") : NO;
}
static id br_cell(id self, i64 r, i64 k) {
    id cache = pget(self, "cells");
    if (!cache) { cache = MSG0(id, (id)kls("NSMutableDictionary"), "dictionary"); pset(self, "cells", cache); }
    char key[48]; snprintf(key, sizeof key, "%lld:%lld", (long long)k, (long long)r);
    id cell = MSG(id, cache, "objectForKey:", cx_str(key));
    if (cell) return cell;
    cell = objc_autorelease(MSG0(id, MSG0(id, (id)kls("NSTextFieldCell"), "alloc"), "init"));
    id d = dget(self);
    if (item_api(self)) {
        id it = br_item(self, r, k);
        id v = responds(d, "browser:objectValueForItem:") ? MSG(id, d, "browser:objectValueForItem:", self, it) : it;
        MSG(void, cell, "setObjectValue:", v);
        psi(cell, "brLeaf", responds(d, "browser:isLeafItem:") ? MSG(BOOL, d, "browser:isLeafItem:", self, it) : NO);
    } else if (d && responds(d, "browser:willDisplayCell:atRow:column:")) MSG(void, d, "browser:willDisplayCell:atRow:column:", self, cell, r, k);
    MSG(void, cache, "setObject:forKey:", cell, cx_str(key));
    return cell;
}
static id br_loaded_cell(id self, SEL c, i64 r, i64 k) { (void)c; return r >= 0 && r < rows_in(self, k) ? br_cell(self, r, k) : 0; }
static i64 br_last_col(id self, SEL c) {
    (void)c;
    i64 n = (i64)MSG0(u64, br_cols(self), "count");
    if (n == 0) return 0;
    i64 r = MSG0(i64, MSG0(id, br_cols(self), "lastObject"), "longLongValue");
    return br_leaf(self, r, n - 1) ? n - 1 : n;
}
static i64 br_max_visible(id self, SEL c) { (void)c; return pgi(self, "maxVis", 3); }
static void br_set_max_visible(id self, SEL c, i64 n) { (void)c; psi(self, "maxVis", n > 0 ? n : 1); redraw(self); }
static double col_w(id self) { R b = msg_rect(self, "bounds"); double mn = pgd(self, "minColW", 100); double w = b.w / (double)pgi(self, "maxVis", 3); return w < mn ? mn : w; }
static double title_h(id self) { return pgi(self, "titled", 0) ? 20 : 0; }
static i64 first_visible(id self) { i64 last = br_last_col(self, 0), vis = pgi(self, "maxVis", 3); return last - vis + 1 > 0 ? last - vis + 1 : 0; }
static void br_draw(id self, SEL c, R dirty) {
    (void)c; (void)dirty;
    R b = msg_rect(self, "bounds");
    MSG0(void, cx_color(1, 1, 1, 1), "setFill"); NSRectFill(b);
    double w = col_w(self), th = title_h(self);
    i64 first = first_visible(self), last = br_last_col(self, 0);
    BOOL fl = flipped(self);
    id a = attrs(0), aw = attrs(cx_color(1, 1, 1, 1));
    for (i64 k = first; k <= last; k++) {
        double x = (double)(k - first) * w;
        if (x > b.w) break;
        if (th > 0) {
            id t = MSG(id, self, "titleOfColumn:", k);
            MSG0(void, cx_color(0.95, 0.95, 0.96, 1), "setFill"); NSRectFill(mkr(x, fl ? 0 : b.h - th, w, th));
            if (t) draw_at(t, (P){x + 6, fl ? 3 : b.h - th + 3}, a);
        }
        i64 n = rows_in(self, k);
        i64 sel = (u64)k < MSG0(u64, br_cols(self), "count") ? MSG0(i64, MSG(id, br_cols(self), "objectAtIndex:", (u64)k), "longLongValue") : -1;
        for (i64 r = 0; r < n; r++) {
            double y = th + r * BR_ROW_H;
            if (y > b.h) break;
            R row = mkr(x, fl ? y : b.h - y - BR_ROW_H, w, BR_ROW_H);
            if (r == sel) { MSG0(void, cx_color(0.13, 0.45, 0.95, 1), "setFill"); NSRectFill(row); }
            id cell = br_cell(self, r, k);
            id s = MSG0(id, cell, "stringValue");
            draw_at(s, (P){row.x + 6, row.y + 2}, r == sel ? aw : a);
            BOOL leaf = item_api(self) ? pgi(cell, "brLeaf", 0) != 0 : (responds(cell, "isLeaf") && MSG0(BOOL, cell, "isLeaf"));
            if (!leaf) draw_at(cx_str("\xE2\x80\xBA"), (P){row.x + w - 12, row.y + 1}, r == sel ? aw : a);
        }
        if (pgi(self, "separates", 1)) { MSG0(void, cx_color(0.85, 0.85, 0.87, 1), "setFill"); NSRectFill(mkr(x + w - 1, 0, 1, b.h)); }
    }
}
static void br_select(id self, i64 r, i64 k, int notify) {
    id sel = br_cols(self);
    while ((i64)MSG0(u64, sel, "count") > k) MSG0(void, sel, "removeLastObject");
    if (r >= 0) MSG(void, sel, "addObject:", cx_num_i(r));
    id cache = pget(self, "cells");
    if (cache) {   /* as colunas depois da escolhida mudaram */
        id keys = MSG0(id, cache, "allKeys");
        for (u64 i = 0, n = MSG0(u64, keys, "count"); i < n; i++) {
            id key = MSG(id, keys, "objectAtIndex:", i);
            if (atoi(cx_cstr(key)) > k) MSG(void, cache, "removeObjectForKey:", key);
        }
    }
    redraw(self);
    if (notify) {
        id d = dget(self);
        if (d && responds(d, "browser:didChangeLastColumn:toColumn:")) MSG(void, d, "browser:didChangeLastColumn:toColumn:", self, k, br_last_col(self, 0));
    }
}
static void br_mouse(id self, SEL c, id ev) {
    (void)c;
    P p = local(self, ev);
    R b = msg_rect(self, "bounds");
    double y = flipped(self) ? p.y : b.h - p.y;
    i64 k = first_visible(self) + (i64)(p.x / col_w(self));
    i64 r = (i64)((y - title_h(self)) / BR_ROW_H);
    if (y < title_h(self) || r < 0 || r >= rows_in(self, k)) return;
    psi(self, "clickedRow", r); psi(self, "clickedCol", k);
    id d = dget(self);
    if (d && responds(d, "browser:shouldSelectRow:inColumn:") && !MSG(BOOL, d, "browser:shouldSelectRow:inColumn:", self, r, k)) return;
    br_select(self, r, k, 1);
    if (MSG0(i64, ev, "clickCount") >= 2) { id v = pget(self, "dbl"); send_action(self, v ? sl(cx_cstr(v)) : 0); }
    else send_action(self, MSG0(SEL, self, "action"));
}
static void br_key(id self, SEL c, id ev) {
    (void)c;
    id ch = MSG0(id, ev, "charactersIgnoringModifiers");
    unsigned short u = ch && MSG0(u64, ch, "length") ? MSG(unsigned short, ch, "characterAtIndex:", (u64)0) : 0;
    id sel = br_cols(self);
    i64 k = (i64)MSG0(u64, sel, "count") - 1;
    i64 r = k >= 0 ? MSG0(i64, MSG(id, sel, "objectAtIndex:", (u64)k), "longLongValue") : -1;
    if (k < 0) { if (rows_in(self, 0)) br_select(self, 0, 0, 1); return; }
    if (u == 0xF700 && r > 0) br_select(self, r - 1, k, 1);
    else if (u == 0xF701 && r + 1 < rows_in(self, k)) br_select(self, r + 1, k, 1);
    else if (u == 0xF702 && k > 0) br_select(self, MSG0(i64, MSG(id, sel, "objectAtIndex:", (u64)(k - 1)), "longLongValue"), k - 1, 1);
    else if (u == 0xF703 && !br_leaf(self, r, k) && rows_in(self, k + 1)) br_select(self, 0, k + 1, 1);
    else { NSBeep(); return; }
    if (pgi(self, "sendsArrows", 0)) send_action(self, MSG0(SEL, self, "action"));
}
static i64 br_selected_col(id self, SEL c) { (void)c; return (i64)MSG0(u64, br_cols(self), "count") - 1; }
static i64 br_selected_row(id self, SEL c, i64 k) { (void)c; id s = br_cols(self); return k >= 0 && (u64)k < MSG0(u64, s, "count") ? MSG0(i64, MSG(id, s, "objectAtIndex:", (u64)k), "longLongValue") : -1; }
static void br_select_row(id self, SEL c, i64 r, i64 k) { (void)c; br_select(self, r, k, 0); }
static id br_selected_cell(id self, SEL c) { (void)c; i64 k = br_selected_col(self, 0); return k >= 0 ? br_cell(self, br_selected_row(self, 0, k), k) : 0; }
static id br_selected_cell_in(id self, SEL c, i64 k) { (void)c; i64 r = br_selected_row(self, 0, k); return r >= 0 ? br_cell(self, r, k) : 0; }
static id br_item_at(id self, SEL c, i64 r, i64 k) { (void)c; return br_item(self, r, k); }
static id br_index_path(id self, SEL c) {
    (void)c;
    id s = br_cols(self);
    u64 n = MSG0(u64, s, "count");
    if (!n) return 0;
    u64 idx[64];
    for (u64 i = 0; i < n && i < 64; i++) idx[i] = (u64)MSG0(i64, MSG(id, s, "objectAtIndex:", i), "longLongValue");
    return MSG(id, (id)kls("NSIndexPath"), "indexPathWithIndexes:length:", idx, n < 64 ? n : 64);
}
static void br_set_index_path(id self, SEL c, id ip) {
    (void)c;
    id s = br_cols(self);
    MSG0(void, s, "removeAllObjects");
    for (u64 i = 0, n = ip ? MSG0(u64, ip, "length") : 0; i < n; i++) { u64 ix = MSG(u64, ip, "indexAtPosition:", i); MSG(void, s, "addObject:", cx_num_i((i64)ix)); }
    pset(self, "cells", 0);
    redraw(self);
}
static id br_item_at_path(id self, SEL c, id ip) {
    (void)c;
    id d = dget(self), item = br_root(self);
    for (u64 i = 0, n = ip ? MSG0(u64, ip, "length") : 0; i < n && item_api(self); i++) { i64 ix = (i64)MSG(u64, ip, "indexAtPosition:", i); item = MSG(id, d, "browser:child:ofItem:", self, ix, item); }
    return item;
}
static id br_sep(id self, SEL c) { (void)c; id s = pget(self, "pathSep"); return s ? s : cx_str("/"); }
static void br_set_sep(id self, SEL c, id s) { (void)c; pset(self, "pathSep", s); }
static id br_path(id self, SEL c) {
    (void)c;
    id parts = arr();
    id s = br_cols(self);
    for (u64 k = 0, n = MSG0(u64, s, "count"); k < n; k++) { i64 rr = MSG0(i64, MSG(id, s, "objectAtIndex:", k), "longLongValue"); id sv = MSG0(id, br_cell(self, rr, (i64)k), "stringValue"); MSG(void, parts, "addObject:", sv); }
    id joined = MSG(id, parts, "componentsJoinedByString:", br_sep(self, 0));
    return MSG(id, br_sep(self, 0), "stringByAppendingString:", joined);
}
static BOOL br_set_path(id self, SEL c, id path) {
    (void)c;
    id comps = MSG(id, path, "componentsSeparatedByString:", br_sep(self, 0));
    MSG0(void, br_cols(self), "removeAllObjects");
    pset(self, "cells", 0);
    i64 k = 0;
    for (u64 i = 0, n = MSG0(u64, comps, "count"); i < n; i++) {
        id want = MSG(id, comps, "objectAtIndex:", i);
        if (!MSG0(u64, want, "length")) continue;
        i64 rows = rows_in(self, k), found = -1;
        for (i64 r = 0; r < rows && found < 0; r++) if (MSG(BOOL, MSG0(id, br_cell(self, r, k), "stringValue"), "isEqualToString:", want)) found = r;
        if (found < 0) { redraw(self); return NO; }
        br_select(self, found, k, 0);
        k++;
    }
    redraw(self);
    return YES;
}
static void br_load_col0(id self, SEL c) { (void)c; MSG0(void, br_cols(self), "removeAllObjects"); pset(self, "cells", 0); psi(self, "loaded", 1); redraw(self); }
static void br_reload_col(id self, SEL c, i64 k) { (void)c; br_select(self, br_selected_row(self, 0, k), k, 0); }
static BOOL br_loaded(id self, SEL c) { (void)c; return pgi(self, "loaded", 1) != 0; }
static i64 br_num_visible(id self, SEL c) { (void)c; i64 n = br_last_col(self, 0) + 1, m = pgi(self, "maxVis", 3); return n < m ? n : m; }
static double br_col_width(id self, SEL c, i64 k) { (void)c; (void)k; return col_w(self); }
static void br_set_width(id self, SEL c, double w, i64 k) { (void)c; (void)k; psd(self, "minColW", w); redraw(self); }
static double br_min_col_w(id self, SEL c) { (void)c; return pgd(self, "minColW", 100); }
static void br_set_min_col_w(id self, SEL c, double w) { (void)c; psd(self, "minColW", w); redraw(self); }
static id br_title(id self, SEL c, i64 k) {
    (void)c;
    id d = dget(self);
    if (d && responds(d, "browser:titleOfColumn:")) return MSG(id, d, "browser:titleOfColumn:", self, k);
    id t = pget(self, "titles");
    if (t && (u64)k < MSG0(u64, t, "count")) return MSG(id, t, "objectAtIndex:", (u64)k);
    return k > 0 ? MSG0(id, br_cell(self, br_selected_row(self, 0, k - 1), k - 1), "stringValue") : 0;
}
static void br_set_title(id self, SEL c, id t, i64 k) {
    (void)c;
    id a = pget(self, "titles");
    if (!a) { a = arr(); pset(self, "titles", a); }
    while ((i64)MSG0(u64, a, "count") <= k) MSG(void, a, "addObject:", cx_str(""));
    MSG(void, a, "replaceObjectAtIndex:withObject:", (u64)k, t ? t : cx_str(""));
    redraw(self);
}
static BOOL br_titled(id self, SEL c) { (void)c; return pgi(self, "titled", 0) != 0; }
static void br_set_titled(id self, SEL c, BOOL b) { (void)c; psi(self, "titled", b); redraw(self); }
static BOOL br_separates(id self, SEL c) { (void)c; return pgi(self, "separates", 1) != 0; }
static void br_set_separates(id self, SEL c, BOOL b) { (void)c; psi(self, "separates", b); redraw(self); }
static BOOL br_sends_arrows(id self, SEL c) { (void)c; return pgi(self, "sendsArrows", 0) != 0; }
static void br_set_sends_arrows(id self, SEL c, BOOL b) { (void)c; psi(self, "sendsArrows", b); }
static SEL br_dbl(id self, SEL c) { (void)c; id v = pget(self, "dbl"); return v ? sl(cx_cstr(v)) : 0; }
static void br_set_dbl(id self, SEL c, SEL a) { (void)c; pset(self, "dbl", a ? cx_str(sel_getName(a)) : 0); }
static i64 br_clicked_row(id self, SEL c) { (void)c; return pgi(self, "clickedRow", -1); }
static i64 br_clicked_col(id self, SEL c) { (void)c; return pgi(self, "clickedCol", -1); }
static BOOL br_flipped(id self, SEL c) { (void)self; (void)c; return YES; }
static BOOL br_accepts(id self, SEL c) { (void)self; (void)c; return YES; }
static void br_noop1(id self, SEL c, id x) { (void)self; (void)c; (void)x; }
static void br_scroll_col(id self, SEL c, i64 k) { (void)self; (void)c; (void)k; redraw(self); }

/* ================================================================ NSRuleEditor */
/* linha: dicionário { criteria: [...], values: [...], type: 0 simples / 1 composto, parent: índice } */
static id re_rows(id self) { id a = pget(self, "rows"); if (!a) { a = arr(); pset(self, "rows", a); } return a; }
static double re_row_h(id self, SEL c) { (void)c; return pgd(self, "rowH", 26); }
static void re_set_row_h(id self, SEL c, double h) { (void)c; psd(self, "rowH", h); MSG0(void, self, "_cxLayoutRows"); }
static i64 re_n_children(id self, id crit, i64 type) { id d = dget(self); return d && responds(d, "ruleEditor:numberOfChildrenForCriterion:withRowType:") ? MSG(i64, d, "ruleEditor:numberOfChildrenForCriterion:withRowType:", self, crit, type) : 0; }
static id re_child(id self, i64 i, id crit, i64 type) { id d = dget(self); return MSG(id, d, "ruleEditor:child:forCriterion:withRowType:", self, i, crit, type); }
static id re_display(id self, id crit, i64 row) { id d = dget(self); return MSG(id, d, "ruleEditor:displayValueForCriterion:inRow:", self, crit, row); }
/* completa a linha a partir de um critério: primeiro filho de cada nível até a folha */
static void re_fill_row(id self, id row, i64 idx, u64 from_level) {
    id crit = MSG(id, row, "objectForKey:", cx_str("criteria"));
    id vals = MSG(id, row, "objectForKey:", cx_str("values"));
    i64 type = MSG0(i64, MSG(id, row, "objectForKey:", cx_str("type")), "longLongValue");
    while (MSG0(u64, crit, "count") > from_level) { MSG0(void, crit, "removeLastObject"); MSG0(void, vals, "removeLastObject"); }
    id parent = MSG0(u64, crit, "count") ? MSG0(id, crit, "lastObject") : 0;
    for (int guard = 0; guard < 32; guard++) {
        if (re_n_children(self, parent, type) <= 0) break;
        id ch = re_child(self, 0, parent, type);
        MSG(void, crit, "addObject:", ch);
        MSG(void, vals, "addObject:", re_display(self, ch, idx));
        parent = ch;
    }
}
static id re_new_row(id self, i64 type, i64 parent, i64 idx) {
    id row = MSG0(id, (id)kls("NSMutableDictionary"), "dictionary");
    MSG(void, row, "setObject:forKey:", arr(), cx_str("criteria"));
    MSG(void, row, "setObject:forKey:", arr(), cx_str("values"));
    MSG(void, row, "setObject:forKey:", cx_num_i(type), cx_str("type"));
    MSG(void, row, "setObject:forKey:", cx_num_i(parent), cx_str("parent"));
    re_fill_row(self, row, idx, 0);
    return row;
}
static void re_changed(id self) {
    pset(self, "pred", 0);
    id d = dget(self);
    id n = MSG(id, (id)kls("NSNotification"), "notificationWithName:object:", cx_str("NSRuleEditorRowsDidChangeNotification"), self);
    if (d && responds(d, "ruleEditorRowsDidChange:")) MSG(void, d, "ruleEditorRowsDidChange:", n);
    MSG(void, MSG0(id, (id)kls("NSNotificationCenter"), "defaultCenter"), "postNotification:", n);
    MSG0(void, self, "_cxLayoutRows");
    send_action(self, MSG0(SEL, self, "action"));
}
static i64 re_count(id self, SEL c) { (void)c; return (i64)MSG0(u64, re_rows(self), "count"); }
static void re_insert(id self, SEL c, i64 at, i64 type, i64 parent, BOOL anim) {
    (void)c; (void)anim;
    id rows = re_rows(self);
    if (at < 0 || at > (i64)MSG0(u64, rows, "count")) at = (i64)MSG0(u64, rows, "count");
    MSG(void, rows, "insertObject:atIndex:", re_new_row(self, type, parent, at), (u64)at);
    for (u64 i = (u64)at + 1, n = MSG0(u64, rows, "count"); i < n; i++) {   /* pais depois do ponto de inserção andam um */
        id r = MSG(id, rows, "objectAtIndex:", i);
        i64 p = MSG0(i64, MSG(id, r, "objectForKey:", cx_str("parent")), "longLongValue");
        if (p >= at) MSG(void, r, "setObject:forKey:", cx_num_i(p + 1), cx_str("parent"));
    }
    re_changed(self);
}
static void re_add_row(id self, SEL c, id sender) {
    (void)c; (void)sender;
    i64 mode = pgi(self, "nesting", 3);   /* NSRuleEditorNestingModeCompound */
    i64 n = re_count(self, 0);
    if (!n && (mode == 3 || mode == 2)) { re_insert(self, 0, 0, 1, -1, NO); re_insert(self, 0, 1, 0, 0, NO); return; }
    re_insert(self, 0, n, 0, n ? (mode == 3 || mode == 2 ? 0 : -1) : -1, NO);
}
static void re_remove_rows(id self, SEL c, id ixs, BOOL subrows) {
    (void)c;
    id rows = re_rows(self);
    id gone = MSG0(id, ixs, "mutableCopy");
    if (subrows) {
        for (u64 i = 0, n = MSG0(u64, rows, "count"); i < n; i++) {
            i64 p = MSG0(i64, MSG(id, MSG(id, rows, "objectAtIndex:", i), "objectForKey:", cx_str("parent")), "longLongValue");
            if (p >= 0 && MSG(BOOL, gone, "containsIndex:", (u64)p)) MSG(void, gone, "addIndex:", i);
        }
    }
    if (!pgi(self, "canRemoveAll", 0) && MSG0(u64, gone, "count") >= MSG0(u64, rows, "count")) { objc_release(gone); NSBeep(); return; }
    MSG(void, rows, "removeObjectsAtIndexes:", gone);
    objc_release(gone);
    re_changed(self);
}
static void re_remove_row(id self, SEL c, i64 i) { (void)c; id ix = MSG(id, (id)kls("NSIndexSet"), "indexSetWithIndex:", (u64)i); re_remove_rows(self, 0, ix, YES); }
static id re_row_at(id self, i64 i) { id rows = re_rows(self); return i >= 0 && (u64)i < MSG0(u64, rows, "count") ? MSG(id, rows, "objectAtIndex:", (u64)i) : 0; }
static id re_criteria(id self, SEL c, i64 i) { (void)c; id r = re_row_at(self, i); return r ? MSG0(id, MSG(id, r, "objectForKey:", cx_str("criteria")), "copy") : 0; }
static id re_values(id self, SEL c, i64 i) { (void)c; id r = re_row_at(self, i); return r ? MSG0(id, MSG(id, r, "objectForKey:", cx_str("values")), "copy") : 0; }
static i64 re_type(id self, SEL c, i64 i) { (void)c; id r = re_row_at(self, i); return r ? MSG0(i64, MSG(id, r, "objectForKey:", cx_str("type")), "longLongValue") : 0; }
static i64 re_parent(id self, SEL c, i64 i) { (void)c; id r = re_row_at(self, i); return r ? MSG0(i64, MSG(id, r, "objectForKey:", cx_str("parent")), "longLongValue") : -1; }
static id re_subrows(id self, SEL c, i64 i) {
    (void)c;
    id out = MSG0(id, (id)kls("NSMutableIndexSet"), "indexSet");
    id rows = re_rows(self);
    for (u64 k = 0, n = MSG0(u64, rows, "count"); k < n; k++) if (MSG0(i64, MSG(id, MSG(id, rows, "objectAtIndex:", k), "objectForKey:", cx_str("parent")), "longLongValue") == i) MSG(void, out, "addIndex:", k);
    return out;
}
static void re_set_crit(id self, SEL c, id crit, id vals, i64 i) {
    (void)c;
    id r = re_row_at(self, i);
    if (!r) return;
    MSG(void, r, "setObject:forKey:", MSG0(id, crit, "mutableCopy"), cx_str("criteria"));
    MSG(void, r, "setObject:forKey:", MSG0(id, vals, "mutableCopy"), cx_str("values"));
    re_changed(self);
}
static id re_sel(id self, SEL c) { (void)c; id s = pget(self, "selRows"); return s ? s : MSG0(id, (id)kls("NSIndexSet"), "indexSet"); }
static void re_select(id self, SEL c, id ixs, BOOL ext) { (void)c; (void)ext; pset(self, "selRows", ixs); redraw(self); }
static i64 re_nesting(id self, SEL c) { (void)c; return pgi(self, "nesting", 3); }
static void re_set_nesting(id self, SEL c, i64 m) { (void)c; psi(self, "nesting", m); }
static BOOL re_can_remove_all(id self, SEL c) { (void)c; return pgi(self, "canRemoveAll", 0) != 0; }
static void re_set_can_remove_all(id self, SEL c, BOOL b) { (void)c; psi(self, "canRemoveAll", b); }
static BOOL re_editable(id self, SEL c) { (void)c; return pgi(self, "editable", 1) != 0; }
static void re_set_editable(id self, SEL c, BOOL b) { (void)c; psi(self, "editable", b); MSG0(void, self, "_cxLayoutRows"); }
static void re_reload_criteria(id self, SEL c) {
    (void)c;
    id rows = re_rows(self);
    for (u64 i = 0, n = MSG0(u64, rows, "count"); i < n; i++) re_fill_row(self, MSG(id, rows, "objectAtIndex:", i), (i64)i, 0);
    re_changed(self);
}
/* predicado de uma linha: partes do delegate por critério (NSRuleEditorPredicate*), juntas em comparação ou composto */
static id re_row_predicate(id self, i64 i) {
    id rows = re_rows(self), d = dget(self);
    id row = MSG(id, rows, "objectAtIndex:", (u64)i);
    id crit = MSG(id, row, "objectForKey:", cx_str("criteria")), vals = MSG(id, row, "objectForKey:", cx_str("values"));
    id parts = MSG0(id, (id)kls("NSMutableDictionary"), "dictionary");
    for (u64 k = 0, n = MSG0(u64, crit, "count"); k < n; k++) {
        if (!d || !responds(d, "ruleEditor:predicatePartsForCriterion:withDisplayValue:inRow:")) break;
        id dv = MSG(id, vals, "objectAtIndex:", k);
        if (responds(dv, "objectValue") && !MSG(BOOL, dv, "isKindOfClass:", (id)kls("NSString"))) dv = MSG0(id, dv, "objectValue");
        id ck = MSG(id, crit, "objectAtIndex:", k);
        id p = MSG(id, d, "ruleEditor:predicatePartsForCriterion:withDisplayValue:inRow:", self, ck, dv, i);
        if (p) MSG(void, parts, "addEntriesFromDictionary:", p);
    }
    i64 type = MSG0(i64, MSG(id, row, "objectForKey:", cx_str("type")), "longLongValue");
    if (type == 1) {
        id subs = arr();
        id kids = re_subrows(self, 0, i);
        for (u64 k = MSG0(u64, kids, "firstIndex"); k != (u64)0x7fffffffffffffffULL; k = MSG(u64, kids, "indexGreaterThanIndex:", k)) {
            id sp = re_row_predicate(self, (i64)k);
            if (sp) MSG(void, subs, "addObject:", sp);
        }
        id ct = MSG(id, parts, "objectForKey:", cx_str("NSRuleEditorPredicateCompoundType"));
        i64 t = ct ? MSG0(i64, ct, "longLongValue") : 1;    /* NSAndPredicateType */
        id cp = (id)kls("NSCompoundPredicate");
        if (t == 0) { if (!MSG0(u64, subs, "count")) return 0; id orp = MSG(id, cp, "orPredicateWithSubpredicates:", subs); return MSG(id, cp, "notPredicateWithSubpredicate:", orp); }
        return MSG(id, cp, t == 2 ? "orPredicateWithSubpredicates:" : "andPredicateWithSubpredicates:", subs);
    }
    id left = MSG(id, parts, "objectForKey:", cx_str("NSRuleEditorPredicateLeftExpression"));
    id right = MSG(id, parts, "objectForKey:", cx_str("NSRuleEditorPredicateRightExpression"));
    id op = MSG(id, parts, "objectForKey:", cx_str("NSRuleEditorPredicateOperatorType"));
    if (!left || !right || !op) return 0;
    id opts = MSG(id, parts, "objectForKey:", cx_str("NSRuleEditorPredicateOptions"));
    id mod = MSG(id, parts, "objectForKey:", cx_str("NSRuleEditorPredicateComparisonModifier"));
    return ((id (*)(id, SEL, id, id, u64, u64, u64))objc_msgSend)((id)kls("NSComparisonPredicate"), sl("predicateWithLeftExpression:rightExpression:modifier:type:options:"),
        left, right, mod ? (u64)MSG0(i64, mod, "longLongValue") : 0, (u64)MSG0(i64, op, "longLongValue"), opts ? (u64)MSG0(i64, opts, "longLongValue") : 0);
}
static id re_predicate(id self, SEL c) {
    (void)c;
    id p = pget(self, "pred");
    if (p) return p;
    id rows = re_rows(self);
    u64 n = MSG0(u64, rows, "count");
    if (!n) return 0;
    id tops = arr();
    for (u64 i = 0; i < n; i++) if (re_parent(self, 0, (i64)i) < 0) { id sp = re_row_predicate(self, (i64)i); if (sp) MSG(void, tops, "addObject:", sp); }
    p = MSG0(u64, tops, "count") == 1 ? MSG(id, tops, "objectAtIndex:", (u64)0) : MSG(id, (id)kls("NSCompoundPredicate"), "andPredicateWithSubpredicates:", tops);
    pset(self, "pred", p);
    return p;
}
static void re_reload_predicate(id self, SEL c) { (void)c; pset(self, "pred", 0); }
static id re_pred_for_row(id self, SEL c, i64 i) { (void)c; return re_row_at(self, i) ? re_row_predicate(self, i) : 0; }
/* desenho: pop-ups dos critérios e as views, + e − (views de verdade dentro do editor) */
static void re_popup_changed(id self, SEL c, id sender) {
    (void)c;
    i64 i = MSG0(i64, sender, "tag") >> 8, level = MSG0(i64, sender, "tag") & 0xFF;
    id row = re_row_at(self, i);
    if (!row) return;
    id crit = MSG(id, row, "objectForKey:", cx_str("criteria")), vals = MSG(id, row, "objectForKey:", cx_str("values"));
    i64 type = MSG0(i64, MSG(id, row, "objectForKey:", cx_str("type")), "longLongValue");
    id parent = level > 0 ? MSG(id, crit, "objectAtIndex:", (u64)(level - 1)) : 0;
    id ch = re_child(self, MSG0(i64, sender, "indexOfSelectedItem"), parent, type);
    while ((i64)MSG0(u64, crit, "count") > level) { MSG0(void, crit, "removeLastObject"); MSG0(void, vals, "removeLastObject"); }
    MSG(void, crit, "addObject:", ch);
    MSG(void, vals, "addObject:", re_display(self, ch, i));
    re_fill_row(self, row, i, (u64)level + 1);
    re_changed(self);
}
static void re_plus(id self, SEL c, id sender) {
    (void)c;
    i64 i = MSG0(i64, sender, "tag");
    i64 type = re_type(self, 0, i);
    re_insert(self, 0, i + 1, 0, type == 1 ? i : re_parent(self, 0, i), NO);
}
static void re_minus(id self, SEL c, id sender) { (void)c; re_remove_row(self, 0, MSG0(i64, sender, "tag")); }
static void re_layout_rows(id self, SEL c) {
    (void)c;
    id subs = MSG0(id, MSG0(id, self, "subviews"), "copy");
    for (u64 i = 0, n = MSG0(u64, subs, "count"); i < n; i++) MSG0(void, MSG(id, subs, "objectAtIndex:", i), "removeFromSuperview");
    objc_release(subs);
    id rows = re_rows(self);
    R b = msg_rect(self, "bounds");
    double rh = re_row_h(self, 0);
    BOOL ed = pgi(self, "editable", 1) != 0;
    for (u64 i = 0, n = MSG0(u64, rows, "count"); i < n; i++) {
        id row = MSG(id, rows, "objectAtIndex:", i);
        id crit = MSG(id, row, "objectForKey:", cx_str("criteria")), vals = MSG(id, row, "objectForKey:", cx_str("values"));
        i64 depth = 0;
        for (i64 p = re_parent(self, 0, (i64)i); p >= 0 && depth < 16; p = re_parent(self, 0, p)) depth++;
        double x = 8 + depth * 18, y = flipped(self) ? i * rh + 2 : b.h - (i + 1) * rh + 2;
        i64 type = MSG0(i64, MSG(id, row, "objectForKey:", cx_str("type")), "longLongValue");
        for (u64 k = 0, m = MSG0(u64, vals, "count"); k < m; k++) {
            id v = MSG(id, vals, "objectAtIndex:", k);
            if (MSG(BOOL, v, "isKindOfClass:", (id)kls("NSView"))) {
                R vf = msg_rect(v, "frame");
                ((void (*)(id, SEL, R))objc_msgSend)(v, sl("setFrame:"), mkr(x, y, vf.w > 10 ? vf.w : 100, rh - 4));
                MSG(void, self, "addSubview:", v);
                x += (vf.w > 10 ? vf.w : 100) + 6;
                continue;
            }
            id parent = k > 0 ? MSG(id, crit, "objectAtIndex:", k - 1) : 0;
            i64 nk = re_n_children(self, parent, type);
            if (nk <= 1) {   /* texto fixo */
                id lt = MSG(BOOL, v, "isKindOfClass:", (id)kls("NSString")) ? v : MSG0(id, v, "description");
                id lbl = MSG(id, (id)kls("NSTextField"), "labelWithString:", lt);
                ((void (*)(id, SEL))objc_msgSend)(lbl, sl("sizeToFit"));
                R lf = msg_rect(lbl, "frame");
                ((void (*)(id, SEL, R))objc_msgSend)(lbl, sl("setFrame:"), mkr(x, y + 2, lf.w, rh - 6));
                MSG(void, self, "addSubview:", lbl);
                x += lf.w + 6;
                continue;
            }
            id pop = objc_autorelease(((id (*)(id, SEL, R, BOOL))objc_msgSend)(MSG0(id, (id)kls("NSPopUpButton"), "alloc"), sl("initWithFrame:pullsDown:"), mkr(x, y, 120, rh - 4), (BOOL)NO));
            for (i64 j = 0; j < nk; j++) {
                id dv = re_display(self, re_child(self, j, parent, type), (i64)i);
                id dt = MSG(BOOL, dv, "isKindOfClass:", (id)kls("NSString")) ? dv : MSG0(id, dv, "description");
                MSG(void, pop, "addItemWithTitle:", dt);
            }
            id cur = MSG(id, crit, "objectAtIndex:", k);
            for (i64 j = 0; j < nk; j++) if (MSG(BOOL, re_child(self, j, parent, type), "isEqual:", cur)) { MSG(void, pop, "selectItemAtIndex:", j); break; }
            MSG(void, pop, "setTag:", (i64)((i << 8) | (i64)k));
            MSG(void, pop, "setTarget:", self); MSG(void, pop, "setAction:", sl("_cxPopupChanged:"));
            MSG(void, pop, "setEnabled:", ed);
            ((void (*)(id, SEL))objc_msgSend)(pop, sl("sizeToFit"));
            MSG(void, self, "addSubview:", pop);
            x += msg_rect(pop, "frame").w + 6;
        }
        if (ed) {
            const char *labels[2] = {"\xE2\x88\x92", "+"};
            SEL acts[2] = {sl("_cxMinus:"), sl("_cxPlus:")};
            for (int bi = 0; bi < 2; bi++) {
                id btn = MSG(id, (id)kls("NSButton"), "buttonWithTitle:target:action:", cx_str(labels[bi]), self, acts[bi]);
                ((void (*)(id, SEL, R))objc_msgSend)(btn, sl("setFrame:"), mkr(b.w - (bi ? 30 : 58), y, 24, rh - 4));
                MSG(void, btn, "setTag:", (i64)i);
                MSG(void, self, "addSubview:", btn);
            }
        }
    }
    redraw(self);
}
static BOOL re_flipped(id self, SEL c) { (void)self; (void)c; return YES; }
static id re_init_frame(id self, SEL c, R f) {
    (void)c;
    self = ((id (*)(id, SEL, R))class_getMethodImplementation(kls("NSControl"), sl("initWithFrame:")))(self, sl("initWithFrame:"), f);
    if (self) { psi(self, "nesting", 3); psi(self, "editable", 1); psd(self, "rowH", 26); }
    return self;
}
static id re_format_strings(id self, SEL c) { (void)c; return pget(self, "fmtStrings"); }
static void re_set_format_strings(id self, SEL c, id d) { (void)c; pset(self, "fmtStrings", d); }
static id re_row_class(id self, SEL c) { (void)c; id k = pget(self, "rowClass"); return k ? k : (id)kls("NSMutableDictionary"); }
static void re_set_row_class(id self, SEL c, id k) { (void)c; pset(self, "rowClass", k); }

/* ================================================================ NSPredicateEditor e row templates */
/* modelo: left (expressões), ops (operadores), right (expressões ou tipo do atributo), compound (tipos) */
static id rt_init_left(id self, SEL c, id left, id right, u64 mod, id ops, u64 opts) {
    (void)c;
    pset(self, "left", left); pset(self, "right", right); pset(self, "ops", ops);
    psi(self, "mod", (i64)mod); psi(self, "opts", (i64)opts); psi(self, "rtype", 0);
    return self;
}
static id rt_init_attr(id self, SEL c, id left, u64 attr, u64 mod, id ops, u64 opts) {
    (void)c;
    pset(self, "left", left); pset(self, "ops", ops); psi(self, "rtype", (i64)attr);
    psi(self, "mod", (i64)mod); psi(self, "opts", (i64)opts);
    return self;
}
static id rt_init_compound(id self, SEL c, id types) { (void)c; pset(self, "compound", types); return self; }
static id rt_left(id self, SEL c) { (void)c; return pget(self, "left"); }
static id rt_right(id self, SEL c) { (void)c; return pget(self, "right"); }
static id rt_ops(id self, SEL c) { (void)c; return pget(self, "ops"); }
static id rt_compound(id self, SEL c) { (void)c; return pget(self, "compound"); }
static u64 rt_mod(id self, SEL c) { (void)c; return (u64)pgi(self, "mod", 0); }
static u64 rt_opts(id self, SEL c) { (void)c; return (u64)pgi(self, "opts", 0); }
static u64 rt_rtype(id self, SEL c) { (void)c; return (u64)pgi(self, "rtype", 0); }
static const char *op_name(i64 op) {
    switch (op) {
    case 0: return "é menor que"; case 1: return "é menor ou igual a"; case 2: return "é maior que"; case 3: return "é maior ou igual a";
    case 4: return "é"; case 5: return "não é"; case 6: return "corresponde a"; case 7: return "é como"; case 8: return "começa com";
    case 9: return "termina com"; case 10: return "está em"; case 99: return "contém"; case 100: return "está entre";
    default: return "?";
    }
}
static id expr_title(id e) {
    id kp = responds(e, "keyPath") ? MSG0(id, e, "keyPath") : 0;
    if (kp) return kp;
    id cv = responds(e, "constantValue") ? MSG0(id, e, "constantValue") : 0;
    return cv ? (MSG(BOOL, cv, "isKindOfClass:", (id)kls("NSString")) ? cv : MSG0(id, cv, "description")) : MSG0(id, e, "description");
}
/* views do modelo: pop-up da esquerda, dos operadores e da direita (ou campo de texto) */
static id rt_views(id self, SEL c) {
    (void)c;
    id views = pget(self, "views");
    if (views) return views;
    views = arr();
    id comp = pget(self, "compound");
    if (comp) {
        id pop = objc_autorelease(((id (*)(id, SEL, R, BOOL))objc_msgSend)(MSG0(id, (id)kls("NSPopUpButton"), "alloc"), sl("initWithFrame:pullsDown:"), mkr(0, 0, 80, 22), (BOOL)NO));
        for (u64 i = 0, n = MSG0(u64, comp, "count"); i < n; i++) {
            i64 t = MSG0(i64, MSG(id, comp, "objectAtIndex:", i), "longLongValue");
            MSG(void, pop, "addItemWithTitle:", cx_str(t == 1 ? "Todos" : t == 2 ? "Qualquer" : "Nenhum"));
            { id ro = MSG(id, comp, "objectAtIndex:", i); MSG(void, MSG0(id, pop, "lastItem"), "setRepresentedObject:", ro); }
        }
        MSG(void, views, "addObject:", pop);
        id lbl = MSG(id, (id)kls("NSTextField"), "labelWithString:", cx_str("dos seguintes são verdadeiros"));
        MSG(void, views, "addObject:", lbl);
    } else {
        id left = pget(self, "left"), ops = pget(self, "ops"), right = pget(self, "right");
        id lp = objc_autorelease(((id (*)(id, SEL, R, BOOL))objc_msgSend)(MSG0(id, (id)kls("NSPopUpButton"), "alloc"), sl("initWithFrame:pullsDown:"), mkr(0, 0, 120, 22), (BOOL)NO));
        for (u64 i = 0, n = left ? MSG0(u64, left, "count") : 0; i < n; i++) {
            id e = MSG(id, left, "objectAtIndex:", i);
            MSG(void, lp, "addItemWithTitle:", expr_title(e));
            MSG(void, MSG0(id, lp, "lastItem"), "setRepresentedObject:", e);
        }
        MSG(void, views, "addObject:", lp);
        id op = objc_autorelease(((id (*)(id, SEL, R, BOOL))objc_msgSend)(MSG0(id, (id)kls("NSPopUpButton"), "alloc"), sl("initWithFrame:pullsDown:"), mkr(0, 0, 120, 22), (BOOL)NO));
        for (u64 i = 0, n = ops ? MSG0(u64, ops, "count") : 0; i < n; i++) {
            id o = MSG(id, ops, "objectAtIndex:", i);
            MSG(void, op, "addItemWithTitle:", cx_str(op_name(MSG0(i64, o, "longLongValue"))));
            MSG(void, MSG0(id, op, "lastItem"), "setRepresentedObject:", o);
        }
        MSG(void, views, "addObject:", op);
        if (right) {
            id rp = objc_autorelease(((id (*)(id, SEL, R, BOOL))objc_msgSend)(MSG0(id, (id)kls("NSPopUpButton"), "alloc"), sl("initWithFrame:pullsDown:"), mkr(0, 0, 120, 22), (BOOL)NO));
            for (u64 i = 0, n = MSG0(u64, right, "count"); i < n; i++) {
                id e = MSG(id, right, "objectAtIndex:", i);
                MSG(void, rp, "addItemWithTitle:", expr_title(e));
                MSG(void, MSG0(id, rp, "lastItem"), "setRepresentedObject:", e);
            }
            MSG(void, views, "addObject:", rp);
        } else {
            id tf = objc_autorelease(((id (*)(id, SEL, R))objc_msgSend)(MSG0(id, (id)kls("NSTextField"), "alloc"), sl("initWithFrame:"), mkr(0, 0, 140, 22)));
            MSG(void, views, "addObject:", tf);
        }
    }
    pset(self, "views", views);
    return views;
}
static id rt_predicate(id self, SEL c, id subs) {
    (void)c;
    id views = rt_views(self, 0);
    id comp = pget(self, "compound");
    id cp = (id)kls("NSCompoundPredicate");
    if (comp) {
        id rep = MSG0(id, MSG0(id, MSG(id, views, "objectAtIndex:", (u64)0), "selectedItem"), "representedObject");
        i64 t = rep ? MSG0(i64, rep, "longLongValue") : 1;
        if (t == 0) { id orp = MSG(id, cp, "orPredicateWithSubpredicates:", subs ? subs : arr()); return MSG(id, cp, "notPredicateWithSubpredicate:", orp); }
        return MSG(id, cp, t == 2 ? "orPredicateWithSubpredicates:" : "andPredicateWithSubpredicates:", subs ? subs : arr());
    }
    id left = MSG0(id, MSG0(id, MSG(id, views, "objectAtIndex:", (u64)0), "selectedItem"), "representedObject");
    id opv = MSG0(id, MSG0(id, MSG(id, views, "objectAtIndex:", (u64)1), "selectedItem"), "representedObject");
    id rv = MSG(id, views, "objectAtIndex:", (u64)2), right;
    if (responds(rv, "selectedItem")) right = MSG0(id, MSG0(id, rv, "selectedItem"), "representedObject");
    else {
        id s = MSG0(id, rv, "stringValue");
        i64 at = pgi(self, "rtype", 0);
        id val = s;
        if (at == 200 || at == 300 || at == 400 || at == 500 || at == 600) val = cx_num_d(MSG0(double, s, "doubleValue"));   /* tipos numéricos do Core Data */
        else if (at == 800) val = cx_num_i(MSG0(BOOL, s, "boolValue"));
        right = MSG(id, (id)kls("NSExpression"), "expressionForConstantValue:", val);
    }
    if (!left || !opv || !right) return 0;
    return ((id (*)(id, SEL, id, id, u64, u64, u64))objc_msgSend)((id)kls("NSComparisonPredicate"), sl("predicateWithLeftExpression:rightExpression:modifier:type:options:"),
        left, right, (u64)pgi(self, "mod", 0), (u64)MSG0(i64, opv, "longLongValue"), (u64)pgi(self, "opts", 0));
}
static double rt_match(id self, SEL c, id pred) {
    (void)c;
    if (pget(self, "compound")) return responds(pred, "subpredicates") ? 1.0 : 0;
    if (!responds(pred, "leftExpression")) return 0;
    id le = MSG0(id, pred, "leftExpression");
    id left = pget(self, "left");
    for (u64 i = 0, n = left ? MSG0(u64, left, "count") : 0; i < n; i++) if (MSG(BOOL, MSG(id, left, "objectAtIndex:", i), "isEqual:", le)) return 0.75;
    return 0;
}
static void rt_set_predicate(id self, SEL c, id pred) {
    (void)c;
    id views = rt_views(self, 0);
    if (pget(self, "compound") && responds(pred, "compoundPredicateType")) {
        i64 t = MSG0(i64, pred, "compoundPredicateType");
        id pop = MSG(id, views, "objectAtIndex:", (u64)0);
        i64 ix = MSG(i64, pop, "indexOfItemWithRepresentedObject:", cx_num_i(t)); MSG(void, pop, "selectItemAtIndex:", ix);
        return;
    }
    if (responds(pred, "leftExpression")) {
        id lp = MSG(id, views, "objectAtIndex:", (u64)0);
        id le = MSG0(id, pred, "leftExpression"); i64 li = MSG(i64, lp, "indexOfItemWithRepresentedObject:", le); MSG(void, lp, "selectItemAtIndex:", li);
        id op = MSG(id, views, "objectAtIndex:", (u64)1);
        i64 pot = MSG0(i64, pred, "predicateOperatorType"); i64 oi = MSG(i64, op, "indexOfItemWithRepresentedObject:", cx_num_i(pot)); MSG(void, op, "selectItemAtIndex:", oi);
        id rv = MSG(id, views, "objectAtIndex:", (u64)2), re = MSG0(id, pred, "rightExpression");
        if (responds(rv, "selectItemAtIndex:")) { i64 ri = MSG(i64, rv, "indexOfItemWithRepresentedObject:", re); MSG(void, rv, "selectItemAtIndex:", ri); }
        else { id cv = responds(re, "constantValue") ? MSG0(id, re, "constantValue") : 0; MSG(void, rv, "setStringValue:", cv ? MSG0(id, cv, "description") : cx_str("")); }
    }
}
static id rt_displayable_subs(id self, SEL c, id pred) { (void)self; (void)c; return responds(pred, "subpredicates") ? MSG0(id, pred, "subpredicates") : 0; }
static id rt_copy(id self, SEL c, void *z) {
    (void)c; (void)z;
    id n = cx_alloc_instance(object_getClass(self));
    id d = MSG0(id, props(self), "mutableCopy");
    MSG(void, d, "removeObjectForKey:", cx_str("views"));
    objc_setAssociatedObject(n, &k_props, d, 1);
    objc_release(d);
    return n;
}
/* o editor de predicados é um NSRuleEditor cujo "delegate" são os modelos: cada linha é uma cópia do modelo */
static id pe_templates(id self, SEL c) { (void)c; id t = pget(self, "templates"); return t ? t : arr(); }
static void pe_set_templates(id self, SEL c, id t) { (void)c; pset(self, "templates", t); }
static id pe_row_template(id self, i64 i) { id rt = pget(self, "rowTemplates"); return rt && i >= 0 && (u64)i < MSG0(u64, rt, "count") ? MSG(id, rt, "objectAtIndex:", (u64)i) : 0; }
static id pick_template(id self, int compound) {
    id ts = pe_templates(self, 0);
    for (u64 i = 0, n = MSG0(u64, ts, "count"); i < n; i++) {
        id t = MSG(id, ts, "objectAtIndex:", i);
        if ((pget(t, "compound") != 0) == compound) return objc_autorelease(MSG(id, t, "copyWithZone:", (void *)0));
    }
    return 0;
}
static void pe_rebuild_rows(id self) {
    id rows = arr(), rts = arr();
    pset(self, "rows", rows); pset(self, "rowTemplates", rts);
    id pred = pget(self, "pePred");
    /* linha composta no topo e uma linha simples por subpredicado (ou uma nova) */
    id ct = pick_template(self, 1);
    i64 top = -1;
    if (ct) {
        id row = MSG0(id, (id)kls("NSMutableDictionary"), "dictionary");
        MSG(void, row, "setObject:forKey:", arr(), cx_str("criteria"));
        MSG(void, row, "setObject:forKey:", MSG0(id, MSG0(id, rt_views(ct, 0), "mutableCopy"), "autorelease"), cx_str("values"));
        MSG(void, row, "setObject:forKey:", cx_num_i(1), cx_str("type")); MSG(void, row, "setObject:forKey:", cx_num_i(-1), cx_str("parent"));
        MSG(void, rows, "addObject:", row); MSG(void, rts, "addObject:", ct);
        if (pred && responds(pred, "compoundPredicateType")) rt_set_predicate(ct, 0, pred);
        top = 0;
    }
    id subs = pred ? (responds(pred, "subpredicates") ? MSG0(id, pred, "subpredicates") : MSG(id, (id)kls("NSArray"), "arrayWithObject:", pred)) : 0;
    u64 n = subs ? MSG0(u64, subs, "count") : 0;
    for (u64 i = 0; i < (n ? n : 1); i++) {
        id sp = n ? MSG(id, subs, "objectAtIndex:", i) : 0;
        id st = 0;
        id ts = pe_templates(self, 0);
        double best = 0;
        for (u64 k = 0, m = MSG0(u64, ts, "count"); k < m && sp; k++) {
            id t = MSG(id, ts, "objectAtIndex:", k);
            if (pget(t, "compound")) continue;
            double s = rt_match(t, 0, sp);
            if (s > best) { best = s; st = t; }
        }
        st = st ? objc_autorelease(MSG(id, st, "copyWithZone:", (void *)0)) : pick_template(self, 0);
        if (!st) break;
        if (sp) rt_set_predicate(st, 0, sp);
        id row = MSG0(id, (id)kls("NSMutableDictionary"), "dictionary");
        MSG(void, row, "setObject:forKey:", arr(), cx_str("criteria"));
        MSG(void, row, "setObject:forKey:", MSG0(id, MSG0(id, rt_views(st, 0), "mutableCopy"), "autorelease"), cx_str("values"));
        MSG(void, row, "setObject:forKey:", cx_num_i(0), cx_str("type")); MSG(void, row, "setObject:forKey:", cx_num_i(top), cx_str("parent"));
        MSG(void, rows, "addObject:", row); MSG(void, rts, "addObject:", st);
    }
    MSG0(void, self, "_cxLayoutRows");
}
static id pe_object_value(id self, SEL c) {
    (void)c;
    id rows = pget(self, "rows");
    u64 n = rows ? MSG0(u64, rows, "count") : 0;
    if (!n) return 0;
    id subs = arr();
    for (u64 i = 0; i < n; i++) {
        if (MSG0(i64, MSG(id, MSG(id, rows, "objectAtIndex:", i), "objectForKey:", cx_str("type")), "longLongValue") == 1) continue;
        id p = rt_predicate(pe_row_template(self, (i64)i), 0, 0);
        if (p) MSG(void, subs, "addObject:", p);
    }
    id top = MSG0(i64, MSG(id, MSG(id, rows, "objectAtIndex:", (u64)0), "objectForKey:", cx_str("type")), "longLongValue") == 1 ? pe_row_template(self, 0) : 0;
    if (top) return rt_predicate(top, 0, subs);
    return MSG0(u64, subs, "count") == 1 ? MSG(id, subs, "objectAtIndex:", (u64)0) : MSG(id, (id)kls("NSCompoundPredicate"), "andPredicateWithSubpredicates:", subs);
}
static void pe_set_object_value(id self, SEL c, id pred) { (void)c; pset(self, "pePred", pred); pe_rebuild_rows(self); }
static void pe_plus(id self, SEL c, id sender) {
    (void)c;
    i64 i = MSG0(i64, sender, "tag");
    id st = pick_template(self, 0);
    if (!st) return;
    id rows = pget(self, "rows"), rts = pget(self, "rowTemplates");
    id row = MSG0(id, (id)kls("NSMutableDictionary"), "dictionary");
    MSG(void, row, "setObject:forKey:", arr(), cx_str("criteria"));
    MSG(void, row, "setObject:forKey:", MSG0(id, MSG0(id, rt_views(st, 0), "mutableCopy"), "autorelease"), cx_str("values"));
    MSG(void, row, "setObject:forKey:", cx_num_i(0), cx_str("type"));
    id src = MSG(id, rows, "objectAtIndex:", (u64)i);
    i64 stype = MSG0(i64, MSG(id, src, "objectForKey:", cx_str("type")), "longLongValue"), spar = MSG0(i64, MSG(id, src, "objectForKey:", cx_str("parent")), "longLongValue");
    MSG(void, row, "setObject:forKey:", cx_num_i(stype == 1 ? i : spar), cx_str("parent"));
    MSG(void, rows, "insertObject:atIndex:", row, (u64)(i + 1));
    MSG(void, rts, "insertObject:atIndex:", st, (u64)(i + 1));
    MSG0(void, self, "_cxLayoutRows");
    send_action(self, MSG0(SEL, self, "action"));
}
static void pe_minus(id self, SEL c, id sender) {
    (void)c;
    i64 i = MSG0(i64, sender, "tag");
    id rows = pget(self, "rows"), rts = pget(self, "rowTemplates");
    if (MSG0(u64, rows, "count") <= 2 && !pgi(self, "canRemoveAll", 0)) { NSBeep(); return; }
    MSG(void, rows, "removeObjectAtIndex:", (u64)i); MSG(void, rts, "removeObjectAtIndex:", (u64)i);
    MSG0(void, self, "_cxLayoutRows");
    send_action(self, MSG0(SEL, self, "action"));
}
static id pe_predicate(id self, SEL c) { return pe_object_value(self, c); }
static void pe_layout(id self, SEL c) {
    /* as views de cada linha são as do modelo da linha; o resto (posição, + e −) é o do NSRuleEditor */
    re_layout_rows(self, c);
    id subs = MSG0(id, self, "subviews");
    for (u64 i = 0, n = MSG0(u64, subs, "count"); i < n; i++) {
        id v = MSG(id, subs, "objectAtIndex:", i);
        if (!MSG(BOOL, v, "isKindOfClass:", (id)kls("NSButton")) || MSG(BOOL, v, "isKindOfClass:", (id)kls("NSPopUpButton"))) continue;
        id t = MSG0(id, v, "title");
        BOOL plus = MSG(BOOL, t, "isEqualToString:", cx_str("+"));
        MSG(void, v, "setAction:", plus ? sl("_cxPEPlus:") : sl("_cxPEMinus:"));
    }
}
static i64 pe_n_children(id self, SEL c, id crit, i64 type) { (void)self; (void)c; (void)crit; (void)type; return 0; }

/* ================================================================ registro */
#define ADD(k, s, f) cx_add_method(k, s, (void *)(f))
static cx_class_t *ensure(cx_class_t *k, cx_class_t *m, const char *name, const char *super) {
    cx_class_t *e = kls(name);
    if (e) return e;
    cx_class_t *s = kls(super);
    return cx_define_class(k, m, name, s ? s : kls("NSObject"), 0);
}
void cx_appkit_controls3_init(void) {
    cx_class_t *k = kls("NSTokenField");
    if (k) {
        tk_super_draw = class_getMethodImplementation(k, sl("drawRect:"));
        ADD(k, "objectValue", tk_object_value); ADD(k, "setObjectValue:", tk_set_object_value); ADD(k, "drawRect:", tk_draw);
        ADD(k, "tokenStyle", tkg_style); ADD(k, "setTokenStyle:", tks_style); ADD(k, "completionDelay", tk_delay); ADD(k, "setCompletionDelay:", tk_set_delay);
        ADD(k, "tokenizingCharacterSet", tk_get_charset); ADD(k, "setTokenizingCharacterSet:", tk_set_charset);
        cx_add_cmethod(k, "defaultTokenizingCharacterSet", (void *)tk_default_charset); cx_add_cmethod(k, "defaultCompletionDelay", (void *)tk_default_delay);
    }
    k = ensure(&NSPathControlItem_c, &NSPathControlItem_m, "NSPathControlItem", "NSObject");
    ADD(k, "title", pci_title); ADD(k, "setTitle:", pci_set_title); ADD(k, "image", pci_image); ADD(k, "setImage:", pci_set_image);
    ADD(k, "URL", pci_url); ADD(k, "attributedTitle", pci_attr_title); ADD(k, "setAttributedTitle:", pci_set_attr_title);
    k = kls("NSPathControl");
    if (k) {
        pc_super_mouse = class_getMethodImplementation(k, sl("mouseDown:"));
        ADD(k, "pathItems", pc_items); ADD(k, "setPathItems:", pc_set_items); ADD(k, "pathStyle", pc_style); ADD(k, "setPathStyle:", pc_set_style);
        ADD(k, "allowedTypes", pc_allowed); ADD(k, "setAllowedTypes:", pc_set_allowed); ADD(k, "placeholderString", pc_placeholder);
        ADD(k, "setPlaceholderString:", pc_set_placeholder); ADD(k, "backgroundColor", pc_bg); ADD(k, "setBackgroundColor:", pc_set_bg);
        ADD(k, "isEditable", pc_editable); ADD(k, "setEditable:", pc_set_editable); ADD(k, "doubleAction", pc_double); ADD(k, "setDoubleAction:", pc_set_double);
        ADD(k, "delegate", g_delegate); ADD(k, "setDelegate:", s_delegate); ADD(k, "mouseDown:", pc_mouse);
    }
    k = ensure(&NSBrowser_c, &NSBrowser_m, "NSBrowser", "NSControl");
    ADD(k, "delegate", g_delegate); ADD(k, "setDelegate:", s_delegate); ADD(k, "drawRect:", br_draw); ADD(k, "mouseDown:", br_mouse); ADD(k, "keyDown:", br_key);
    ADD(k, "isFlipped", br_flipped); ADD(k, "acceptsFirstResponder", br_accepts);
    ADD(k, "loadedCellAtRow:column:", br_loaded_cell); ADD(k, "lastColumn", br_last_col); ADD(k, "lastVisibleColumn", br_last_col);
    ADD(k, "maxVisibleColumns", br_max_visible); ADD(k, "setMaxVisibleColumns:", br_set_max_visible); ADD(k, "numberOfVisibleColumns", br_num_visible);
    ADD(k, "selectedColumn", br_selected_col); ADD(k, "selectedRowInColumn:", br_selected_row); ADD(k, "selectRow:inColumn:", br_select_row);
    ADD(k, "selectedCell", br_selected_cell); ADD(k, "selectedCellInColumn:", br_selected_cell_in); ADD(k, "itemAtRow:inColumn:", br_item_at);
    ADD(k, "selectionIndexPath", br_index_path); ADD(k, "setSelectionIndexPath:", br_set_index_path); ADD(k, "itemAtIndexPath:", br_item_at_path);
    ADD(k, "path", br_path); ADD(k, "setPath:", br_set_path); ADD(k, "pathSeparator", br_sep); ADD(k, "setPathSeparator:", br_set_sep);
    ADD(k, "loadColumnZero", br_load_col0); ADD(k, "reloadColumn:", br_reload_col); ADD(k, "isLoaded", br_loaded);
    ADD(k, "widthOfColumn:", br_col_width); ADD(k, "setWidth:ofColumn:", br_set_width); ADD(k, "minColumnWidth", br_min_col_w); ADD(k, "setMinColumnWidth:", br_set_min_col_w);
    ADD(k, "titleOfColumn:", br_title); ADD(k, "setTitle:ofColumn:", br_set_title); ADD(k, "isTitled", br_titled); ADD(k, "setTitled:", br_set_titled);
    ADD(k, "separatesColumns", br_separates); ADD(k, "setSeparatesColumns:", br_set_separates);
    ADD(k, "sendsActionOnArrowKeys", br_sends_arrows); ADD(k, "setSendsActionOnArrowKeys:", br_set_sends_arrows);
    ADD(k, "doubleAction", br_dbl); ADD(k, "setDoubleAction:", br_set_dbl); ADD(k, "clickedRow", br_clicked_row); ADD(k, "clickedColumn", br_clicked_col);
    ADD(k, "setHasHorizontalScroller:", br_noop1); ADD(k, "setColumnResizingType:", br_noop1); ADD(k, "scrollColumnToVisible:", br_scroll_col);
    ADD(k, "setAllowsMultipleSelection:", br_noop1); ADD(k, "setAutohidesScroller:", br_noop1);
    k = ensure(&NSRuleEditor_c, &NSRuleEditor_m, "NSRuleEditor", "NSControl");
    ADD(k, "initWithFrame:", re_init_frame); ADD(k, "delegate", g_delegate); ADD(k, "setDelegate:", s_delegate); ADD(k, "isFlipped", re_flipped);
    ADD(k, "numberOfRows", re_count); ADD(k, "addRow:", re_add_row); ADD(k, "insertRowAtIndex:withType:asSubrowOfRow:animate:", re_insert);
    ADD(k, "removeRowAtIndex:", re_remove_row); ADD(k, "removeRowsAtIndexes:includeSubrows:", re_remove_rows);
    ADD(k, "criteriaForRow:", re_criteria); ADD(k, "displayValuesForRow:", re_values); ADD(k, "rowTypeForRow:", re_type); ADD(k, "parentRowForRow:", re_parent);
    ADD(k, "subrowIndexesForRow:", re_subrows); ADD(k, "setCriteria:andDisplayValues:forRowAtIndex:", re_set_crit);
    ADD(k, "selectedRowIndexes", re_sel); ADD(k, "selectRowIndexes:byExtendingSelection:", re_select);
    ADD(k, "nestingMode", re_nesting); ADD(k, "setNestingMode:", re_set_nesting); ADD(k, "canRemoveAllRows", re_can_remove_all); ADD(k, "setCanRemoveAllRows:", re_set_can_remove_all);
    ADD(k, "isEditable", re_editable); ADD(k, "setEditable:", re_set_editable); ADD(k, "rowHeight", re_row_h); ADD(k, "setRowHeight:", re_set_row_h);
    ADD(k, "reloadCriteria", re_reload_criteria); ADD(k, "reloadPredicate", re_reload_predicate); ADD(k, "predicate", re_predicate);
    ADD(k, "predicateForRow:", re_pred_for_row); ADD(k, "formattingStringsFilename", re_format_strings); ADD(k, "setFormattingStringsFilename:", re_set_format_strings);
    ADD(k, "rowClass", re_row_class); ADD(k, "setRowClass:", re_set_row_class);
    ADD(k, "_cxLayoutRows", re_layout_rows); ADD(k, "_cxPopupChanged:", re_popup_changed); ADD(k, "_cxPlus:", re_plus); ADD(k, "_cxMinus:", re_minus);
    k = ensure(&NSPredicateEditorRowTemplate_c, &NSPredicateEditorRowTemplate_m, "NSPredicateEditorRowTemplate", "NSObject");
    ADD(k, "initWithLeftExpressions:rightExpressions:modifier:operators:options:", rt_init_left);
    ADD(k, "initWithLeftExpressions:rightExpressionAttributeType:modifier:operators:options:", rt_init_attr);
    ADD(k, "initWithCompoundTypes:", rt_init_compound); ADD(k, "leftExpressions", rt_left); ADD(k, "rightExpressions", rt_right);
    ADD(k, "operators", rt_ops); ADD(k, "compoundTypes", rt_compound); ADD(k, "modifier", rt_mod); ADD(k, "options", rt_opts);
    ADD(k, "rightExpressionAttributeType", rt_rtype); ADD(k, "templateViews", rt_views); ADD(k, "predicateWithSubpredicates:", rt_predicate);
    ADD(k, "matchForPredicate:", rt_match); ADD(k, "setPredicate:", rt_set_predicate); ADD(k, "displayableSubpredicatesOfPredicate:", rt_displayable_subs);
    ADD(k, "copyWithZone:", rt_copy);
    k = ensure(&NSPredicateEditor_c, &NSPredicateEditor_m, "NSPredicateEditor", "NSRuleEditor");
    ADD(k, "rowTemplates", pe_templates); ADD(k, "setRowTemplates:", pe_set_templates); ADD(k, "objectValue", pe_object_value);
    ADD(k, "setObjectValue:", pe_set_object_value); ADD(k, "predicate", pe_predicate); ADD(k, "_cxLayoutRows", pe_layout);
    ADD(k, "_cxPEPlus:", pe_plus); ADD(k, "_cxPEMinus:", pe_minus); ADD(k, "ruleEditor:numberOfChildrenForCriterion:withRowType:", pe_n_children);
}
