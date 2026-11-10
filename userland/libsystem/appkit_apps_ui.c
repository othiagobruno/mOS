/*
 * AppKit que o Postico 2.3.5 e o Setapp 4.0.0 (165) importam e o libSystem não exportava,
 * medido com tools/machocheck.c --disco.
 *
 * Texto (Postico): NSTextStorage, NSLayoutManager, NSTextContainer, NSTextTab.
 * Grade (Setapp): NSCollectionView e o layout em fluxo, itens, NSArrayController.
 * Folha: -[NSWindow beginSheet:completionHandler:] encosta a contentView no topo.
 * O resto são constantes, NSDataAsset, NSCIImageRep, NSClickGestureRecognizer e
 * NSDrawNinePartImage / NSBitsPerPixelFromDepth / NSAccessibilityFrameInView.
 */
#include "foundation.h"
#include "libc.h"
#include "apps_ui_geom.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void objc_setAssociatedObject(id obj, const void *key, id value, u64 policy);
id objc_getAssociatedObject(id obj, const void *key);
void *_Block_copy(const void *b);
void _Block_release(const void *b);
IMP class_getMethodImplementation(cx_class_t *c, SEL s);
const char *sel_getName(SEL s);

typedef struct { double x, y; } ui_pt;
typedef struct { double w, h; } ui_sz;
typedef struct { ui_pt o; ui_sz s; } ui_nsrect;
typedef struct { unsigned long loc, len; } ui_rng;

#define K(N) cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N); cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N);
K(NSTextStorage) K(NSLayoutManager)
K(NSCollectionView) K(NSCollectionViewItem) K(NSCollectionViewLayout)
K(NSCollectionViewFlowLayout) K(NSCollectionViewLayoutAttributes)
K(NSCollectionViewCompositionalLayout) K(NSCollectionViewCompositionalLayoutConfiguration)
K(NSCollectionLayoutSection) K(NSCollectionLayoutGroup) K(NSCollectionLayoutItem)
K(NSCollectionLayoutSize) K(NSCollectionLayoutDimension) K(NSCollectionLayoutSpacing)
K(NSCollectionLayoutGroupCustomItem) K(NSCollectionLayoutBoundarySupplementaryItem)
K(NSArrayController) K(NSDataAsset) K(NSCIImageRep)

static cx_class_t *cl(const char *n) { return cx_class_named(n); }
static void def(cx_class_t *k, cx_class_t *m, const char *name, const char *super) {
    if (cx_class_named(name) && cx_class_named(name)->vtable) return;
    cx_class_t *s = cx_class_named(super);
    if (!s) s = cx_class_named("NSObject");
    cx_define_class(k, m, name, s, 64);
}
#define DEF(N, S) def(&N##_c, &N##_m, #N, S)
#define IM(k, s, f) cx_add_method(&k##_c, s, (void *)(f))
#define CM(k, s, f) cx_add_cmethod(&k##_c, s, (void *)(f))

static char props_key;
static id props(id o) {
    id d = objc_getAssociatedObject(o, &props_key);
    if (!d) {
        d = ((id (*)(id, SEL))objc_msgSend)(((id (*)(id, SEL))objc_msgSend)((id)cl("NSMutableDictionary"), sel_registerName("alloc")), sel_registerName("init"));
        objc_setAssociatedObject(o, &props_key, d, 1);
        objc_release(d);
    }
    return d;
}
static id pget(id o, const char *k) {
    return ((id (*)(id, SEL, id))objc_msgSend)(props(o), sel_registerName("objectForKey:"), cx_str(k));
}
static void pset(id o, const char *k, id v) {
    if (v) ((void (*)(id, SEL, id, id))objc_msgSend)(props(o), sel_registerName("setObject:forKey:"), v, cx_str(k));
    else ((void (*)(id, SEL, id))objc_msgSend)(props(o), sel_registerName("removeObjectForKey:"), cx_str(k));
}
static id num_d(double v) {
    return ((id (*)(id, SEL, double))objc_msgSend)((id)cl("NSNumber"), sel_registerName("numberWithDouble:"), v);
}
static double as_d(id v, double d) {
    return v ? ((double (*)(id, SEL))objc_msgSend)(v, sel_registerName("doubleValue")) : d;
}

/* ---------------------------------------------------------------- texto */
static id ts_string(id self, SEL c) { (void)c; id s = pget(self, "string"); return s ? s : cx_str(""); }
static void ts_edited(id self) {
    id name = cx_str("NSTextStorageDidProcessEditingNotification");
    id note = cx_note(name, self, 0);
    id center = ((id (*)(id, SEL))objc_msgSend)((id)cl("NSNotificationCenter"), sel_registerName("defaultCenter"));
    if (center && note) ((void (*)(id, SEL, id))objc_msgSend)(center, sel_registerName("postNotification:"), note);
}
static void ts_replace(id self, SEL c, ui_rng r, id str) {
    (void)c; (void)r;
    pset(self, "string", str ? str : cx_str(""));
    ts_edited(self);
}
static void ts_set_attr(id self, SEL c, ui_rng r) { (void)c; (void)r; (void)self; }
static void ts_add_attr(id self, SEL c, id name, id value, ui_rng r) {
    (void)c; (void)r;
    if (!name) return;
    id bag = pget(self, "attrs");
    if (!bag) {
        bag = ((id (*)(id, SEL))objc_msgSend)(((id (*)(id, SEL))objc_msgSend)((id)cl("NSMutableDictionary"), sel_registerName("alloc")), sel_registerName("init"));
        pset(self, "attrs", bag);
        objc_release(bag);
    }
    if (value) ((void (*)(id, SEL, id, id))objc_msgSend)(bag, sel_registerName("setObject:forKey:"), value, name);
}
static id ts_attrs(id self, SEL c, u64 idx, u64 *eff) {
    (void)c; (void)idx;
    if (eff) *eff = 0;
    id bag = pget(self, "attrs");
    return bag ? bag : ((id (*)(id, SEL))objc_msgSend)((id)cl("NSDictionary"), sel_registerName("dictionary"));
}
static u64 ts_length(id self, SEL c) {
    (void)c;
    id s = ts_string(self, 0);
    const char *p = cx_cstr(s);
    return p ? (u64)strlen(p) : 0;
}
static void ts_process(id self, SEL c) { (void)c; ts_edited(self); }

static id lm_init(id self, SEL c) { (void)c; return self; }
static void lm_set_storage(id self, SEL c, id st) {
    (void)c;
    pset(self, "textStorage", st);
    if (st) pset(st, "layoutManager", self);
}
static id lm_storage(id self, SEL c) { (void)c; return pget(self, "textStorage"); }
static void lm_add_tc(id self, SEL c, id tc) {
    (void)c;
    id a = pget(self, "containers");
    if (!a) {
        a = ((id (*)(id, SEL))objc_msgSend)(((id (*)(id, SEL))objc_msgSend)((id)cl("NSMutableArray"), sel_registerName("alloc")), sel_registerName("init"));
        pset(self, "containers", a);
        objc_release(a);
    }
    if (tc) {
        ((void (*)(id, SEL, id))objc_msgSend)(a, sel_registerName("addObject:"), tc);
        pset(tc, "layoutManager", self);
    }
}
static id lm_containers(id self, SEL c) { (void)c; id a = pget(self, "containers"); return a ? a : ((id (*)(id, SEL))objc_msgSend)((id)cl("NSArray"), sel_registerName("array")); }
static u64 lm_glyphs(id self, SEL c) { (void)c; id st = lm_storage(self, 0); return st ? ts_length(st, 0) : 0; }
static ui_rng lm_glyph_range(id self, SEL c, id tc) {
    (void)c; (void)tc;
    ui_rng r = {0, lm_glyphs(self, 0)};
    return r;
}

static id tc_init(id self, SEL c, ui_sz sz) {
    (void)c;
    pset(self, "w", num_d(sz.w));
    pset(self, "h", num_d(sz.h));
    pset(self, "pad", num_d(5));
    return self;
}
static ui_sz tc_size(id self, SEL c) { (void)c; return (ui_sz){as_d(pget(self, "w"), 0), as_d(pget(self, "h"), 0)}; }
static void tc_set_size(id self, SEL c, ui_sz sz) { (void)c; pset(self, "w", num_d(sz.w)); pset(self, "h", num_d(sz.h)); }
static id tc_lm(id self, SEL c) { (void)c; return pget(self, "layoutManager"); }
static double tc_pad(id self, SEL c) { (void)c; return as_d(pget(self, "pad"), 5); }
static void tc_set_pad(id self, SEL c, double v) { (void)c; pset(self, "pad", num_d(v)); }

static id tab_init(id self, SEL c, u64 align, double loc, id opts) {
    (void)c;
    pset(self, "align", num_d((double)align));
    pset(self, "loc", num_d(loc));
    if (opts) pset(self, "opts", opts);
    return self;
}
static u64 tab_align(id self, SEL c) { (void)c; return (u64)as_d(pget(self, "align"), 0); }
static double tab_loc(id self, SEL c) { (void)c; return as_d(pget(self, "loc"), 0); }
static id tab_opts(id self, SEL c) { (void)c; return pget(self, "opts"); }

/* ---------------------------------------------------------------- coleção */
static id idx_path(u64 item, u64 section) {
    u64 xs[2] = {item, section};
    return ((id (*)(id, SEL, const u64 *, u64))objc_msgSend)((id)cl("NSIndexPath"), sel_registerName("indexPathWithIndexes:length:"), xs, 2);
}
static u64 idx_at(id ip, u64 pos) {
    if (!ip) return 0;
    return ((u64 (*)(id, SEL, u64))objc_msgSend)(ip, sel_registerName("indexAtPosition:"), pos);
}

static void cv_apply(id self) {
    id ds = pget(self, "dataSource");
    id items = pget(self, "items");
    if (items) {
        u64 n = ((u64 (*)(id, SEL))objc_msgSend)(items, sel_registerName("count"));
        for (u64 i = 0; i < n; i++) {
            id it = ((id (*)(id, SEL, u64))objc_msgSend)(items, sel_registerName("objectAtIndex:"), i);
            id view = it ? pget(it, "view") : 0;
            if (view) ((void (*)(id, SEL))objc_msgSend)(view, sel_registerName("removeFromSuperview"));
        }
    }
    id fresh = ((id (*)(id, SEL))objc_msgSend)(((id (*)(id, SEL))objc_msgSend)((id)cl("NSMutableArray"), sel_registerName("alloc")), sel_registerName("init"));
    u64 count = 0;
    if (ds && cx_responds(ds->isa, sel_registerName("collectionView:numberOfItemsInSection:")))
        count = ((u64 (*)(id, SEL, id, i64))objc_msgSend)(ds, sel_registerName("collectionView:numberOfItemsInSection:"), self, 0);
    double iw = as_d(pget(self, "iw"), 120), ih = as_d(pget(self, "ih"), 80);
    double gx = as_d(pget(self, "gx"), 8), gy = as_d(pget(self, "gy"), 8);
    double inset = as_d(pget(self, "inset"), 8);
    ui_nsrect fr = ((ui_nsrect (*)(id, SEL))objc_msgSend)(self, sel_registerName("frame"));
    ui_rect *boxes = calloc(count ? count : 1, sizeof *boxes);
    int placed = cx_flow_place((int)count, iw, ih, gx, gy, inset, fr.s.w, boxes, (int)(count ? count : 1));
    for (int i = 0; i < placed; i++) {
        id ip = idx_path((u64)i, 0);
        id item = 0;
        if (ds && cx_responds(ds->isa, sel_registerName("collectionView:itemForRepresentedObjectAtIndexPath:")))
            item = ((id (*)(id, SEL, id, id))objc_msgSend)(ds, sel_registerName("collectionView:itemForRepresentedObjectAtIndexPath:"), self, ip);
        if (!item) {
            item = ((id (*)(id, SEL))objc_msgSend)(((id (*)(id, SEL))objc_msgSend)((id)&NSCollectionViewItem_c, sel_registerName("alloc")), sel_registerName("init"));
        }
        id view = pget(item, "view");
        if (!view) {
            view = ((id (*)(id, SEL, ui_nsrect))objc_msgSend)(((id (*)(id, SEL))objc_msgSend)((id)cl("NSView"), sel_registerName("alloc")), sel_registerName("initWithFrame:"),
                                                             (ui_nsrect){{0, 0}, {boxes[i].w, boxes[i].h}});
            pset(item, "view", view);
            objc_release(view);
        }
        ((void (*)(id, SEL, ui_nsrect))objc_msgSend)(view, sel_registerName("setFrame:"), (ui_nsrect){{boxes[i].x, boxes[i].y}, {boxes[i].w, boxes[i].h}});
        ((void (*)(id, SEL, id))objc_msgSend)(self, sel_registerName("addSubview:"), view);
        ((void (*)(id, SEL, id))objc_msgSend)(fresh, sel_registerName("addObject:"), item);
    }
    pset(self, "items", fresh);
    objc_release(fresh);
    free(boxes);
}
static void cv_reload(id self, SEL c) { (void)c; cv_apply(self); }
static void cv_set_ds(id self, SEL c, id ds) { (void)c; pset(self, "dataSource", ds); }
static id cv_ds(id self, SEL c) { (void)c; return pget(self, "dataSource"); }
static void cv_set_layout(id self, SEL c, id lay) {
    (void)c;
    pset(self, "layout", lay);
    if (lay) {
        if (pget(lay, "iw")) pset(self, "iw", pget(lay, "iw"));
        if (pget(lay, "ih")) pset(self, "ih", pget(lay, "ih"));
    }
}
static id cv_layout(id self, SEL c) { (void)c; return pget(self, "layout"); }
static id cv_item_at(id self, SEL c, id ip) {
    (void)c;
    id items = pget(self, "items");
    if (!items) return 0;
    u64 i = idx_at(ip, 0);
    u64 n = ((u64 (*)(id, SEL))objc_msgSend)(items, sel_registerName("count"));
    if (i >= n) return 0;
    return ((id (*)(id, SEL, u64))objc_msgSend)(items, sel_registerName("objectAtIndex:"), i);
}
static id cv_make_item(id self, SEL c, id ident, id ip) {
    (void)c; (void)ip; (void)ident;
    return ((id (*)(id, SEL))objc_msgSend)(((id (*)(id, SEL))objc_msgSend)((id)&NSCollectionViewItem_c, sel_registerName("alloc")), sel_registerName("init"));
}
static void cv_register(id self, SEL c, id cls, id ident) { (void)c; (void)cls; pset(self, "reg", ident); }
static id item_view(id self, SEL c) { (void)c; return pget(self, "view"); }
static void item_set_view(id self, SEL c, id v) { (void)c; pset(self, "view", v); }
static id item_rep(id self, SEL c) { (void)c; return pget(self, "representedObject"); }
static void item_set_rep(id self, SEL c, id v) { (void)c; pset(self, "representedObject", v); }
static char item_sel(id self, SEL c) { (void)c; return pget(self, "selected") ? 1 : 0; }
static void item_set_sel(id self, SEL c, char on) { (void)c; pset(self, "selected", on ? cx_str("1") : 0); }

static void flow_set_size(id self, SEL c, ui_sz sz) { (void)c; pset(self, "iw", num_d(sz.w)); pset(self, "ih", num_d(sz.h)); }
static ui_sz flow_size(id self, SEL c) { (void)c; return (ui_sz){as_d(pget(self, "iw"), 120), as_d(pget(self, "ih"), 80)}; }
static void flow_set_gap(id self, SEL c, double g) { (void)c; pset(self, "gap", num_d(g)); }

static id lay_dim(id self, SEL c, double v) { (void)self; (void)c; id o = ((id (*)(id, SEL))objc_msgSend)(((id (*)(id, SEL))objc_msgSend)((id)&NSCollectionLayoutDimension_c, sel_registerName("alloc")), sel_registerName("init")); pset(o, "v", num_d(v)); objc_autorelease(o); return o; }
static id lay_size(id self, SEL c, id w, id h) { (void)self; (void)c; id o = ((id (*)(id, SEL))objc_msgSend)(((id (*)(id, SEL))objc_msgSend)((id)&NSCollectionLayoutSize_c, sel_registerName("alloc")), sel_registerName("init")); pset(o, "w", w); pset(o, "h", h); objc_autorelease(o); return o; }

/* ---------------------------------------------------------------- controlador e gesto */
static id ac_arranged(id self, SEL c) {
    (void)c;
    id a = pget(self, "content");
    return a ? a : ((id (*)(id, SEL))objc_msgSend)((id)cl("NSArray"), sel_registerName("array"));
}
static void ac_set_content(id self, SEL c, id a) { (void)c; pset(self, "content", a); }
static void ac_add(id self, SEL c, id obj) {
    (void)c;
    if (!obj) return;
    id a = pget(self, "content");
    if (!a || !cx_responds(a->isa, sel_registerName("addObject:"))) {
        a = ((id (*)(id, SEL))objc_msgSend)(((id (*)(id, SEL))objc_msgSend)((id)cl("NSMutableArray"), sel_registerName("alloc")), sel_registerName("init"));
        pset(self, "content", a);
        objc_release(a);
    }
    ((void (*)(id, SEL, id))objc_msgSend)(a, sel_registerName("addObject:"), obj);
}
static void ac_remove(id self, SEL c, id obj) {
    (void)c;
    id a = pget(self, "content");
    if (a && obj) ((void (*)(id, SEL, id))objc_msgSend)(a, sel_registerName("removeObject:"), obj);
}
static i64 ac_sel(id self, SEL c) { (void)c; id v = pget(self, "sel"); return v ? (i64)as_d(v, 0) : -1; }
static void ac_set_sel(id self, SEL c, i64 i) { (void)c; pset(self, "sel", num_d((double)i)); }

static char gr_view_key;
static id gr_init(id self, SEL c, id target, SEL action) {
    (void)c;
    pset(self, "target", target);
    if (action) pset(self, "action", cx_str(sel_getName(action)));
    return self;
}
static void gr_set_target(id self, SEL c, id t) { (void)c; pset(self, "target", t); }
static id gr_target(id self, SEL c) { (void)c; return pget(self, "target"); }
static void gr_set_action(id self, SEL c, SEL a) { (void)c; pset(self, "action", a ? cx_str(sel_getName(a)) : 0); }
static SEL gr_action(id self, SEL c) { (void)c; id s = pget(self, "action"); return s ? sel_registerName(cx_cstr(s)) : 0; }

static void view_add_gr(id self, SEL c, id gr) {
    (void)c;
    if (!gr) return;
    id a = objc_getAssociatedObject(self, &gr_view_key);
    if (!a) {
        a = ((id (*)(id, SEL))objc_msgSend)(((id (*)(id, SEL))objc_msgSend)((id)cl("NSMutableArray"), sel_registerName("alloc")), sel_registerName("init"));
        objc_setAssociatedObject(self, &gr_view_key, a, 1);
        objc_release(a);
    }
    ((void (*)(id, SEL, id))objc_msgSend)(a, sel_registerName("addObject:"), gr);
    objc_setAssociatedObject(gr, &gr_view_key, self, 0);
}
static void view_mouse_down(id self, SEL c, id ev) {
    (void)c;
    id a = objc_getAssociatedObject(self, &gr_view_key);
    if (a) {
        u64 n = ((u64 (*)(id, SEL))objc_msgSend)(a, sel_registerName("count"));
        for (u64 i = 0; i < n; i++) {
            id gr = ((id (*)(id, SEL, u64))objc_msgSend)(a, sel_registerName("objectAtIndex:"), i);
            id target = pget(gr, "target");
            id an = pget(gr, "action");
            if (target && an) ((void (*)(id, SEL, id))objc_msgSend)(target, sel_registerName(cx_cstr(an)), gr);
        }
    }
    id sv = ((id (*)(id, SEL))objc_msgSend)(self, sel_registerName("superview"));
    if (sv) ((void (*)(id, SEL, id))objc_msgSend)(sv, sel_registerName("mouseDown:"), ev);
}

/* ---------------------------------------------------------------- folha, asset, imagem */
static char sheet_key, sheet_block_key;
static void win_begin_sheet(id self, SEL c, id sheet, void *handler) {
    (void)c;
    id cv = ((id (*)(id, SEL))objc_msgSend)(self, sel_registerName("contentView"));
    id scv = sheet ? ((id (*)(id, SEL))objc_msgSend)(sheet, sel_registerName("contentView")) : 0;
    if (cv && scv) {
        ui_nsrect wf = ((ui_nsrect (*)(id, SEL))objc_msgSend)(cv, sel_registerName("frame"));
        ui_nsrect sf = ((ui_nsrect (*)(id, SEL))objc_msgSend)(scv, sel_registerName("frame"));
        double x, y, w, h;
        cx_sheet_frame(wf.s.w, wf.s.h, sf.s.w, sf.s.h, &x, &y, &w, &h);
        ((void (*)(id, SEL, ui_nsrect))objc_msgSend)(scv, sel_registerName("setFrame:"), (ui_nsrect){{x, y}, {w, h}});
        ((void (*)(id, SEL, id))objc_msgSend)(cv, sel_registerName("addSubview:"), scv);
    }
    if (sheet) objc_setAssociatedObject(self, &sheet_key, sheet, 1);
    if (handler) {
        void *b = _Block_copy(handler);
        objc_setAssociatedObject(self, &sheet_block_key, (id)b, 1);
    }
}
static void win_end_sheet(id self, SEL c, id sheet, i64 code) {
    (void)c;
    id scv = sheet ? ((id (*)(id, SEL))objc_msgSend)(sheet, sel_registerName("contentView")) : 0;
    if (scv) ((void (*)(id, SEL))objc_msgSend)(scv, sel_registerName("removeFromSuperview"));
    void *b = (void *)objc_getAssociatedObject(self, &sheet_block_key);
    if (b) {
        void (*inv)(void *, i64) = *(void **)((u8 *)b + 16);
        if (inv) inv(b, code);
        _Block_release(b);
        objc_setAssociatedObject(self, &sheet_block_key, 0, 1);
    }
    objc_setAssociatedObject(self, &sheet_key, 0, 1);
}
static void win_end_sheet0(id self, SEL c, id sheet) { win_end_sheet(self, c, sheet, 0); }

static id asset_init(id self, SEL c, id name, id bundle) {
    (void)c;
    pset(self, "name", name);
    if (!bundle) bundle = ((id (*)(id, SEL))objc_msgSend)((id)cl("NSBundle"), sel_registerName("mainBundle"));
    id data = 0;
    if (bundle && name && cx_responds(bundle->isa, sel_registerName("URLForResource:withExtension:"))) {
        id url = ((id (*)(id, SEL, id, id))objc_msgSend)(bundle, sel_registerName("URLForResource:withExtension:"), name, 0);
        if (url) data = ((id (*)(id, SEL, id))objc_msgSend)((id)cl("NSData"), sel_registerName("dataWithContentsOfURL:"), url);
    }
    if (data) pset(self, "data", data);
    return self;
}
static id asset_init_name(id self, SEL c, id name) { return asset_init(self, c, name, 0); }
static id asset_data(id self, SEL c) { (void)c; return pget(self, "data"); }
static id asset_name(id self, SEL c) { (void)c; return pget(self, "name"); }

static id ci_init(id self, SEL c, id img) { (void)c; pset(self, "ci", img); return self; }
static id ci_image(id self, SEL c) { (void)c; return pget(self, "ci"); }

/* ---------------------------------------------------------------- funções C */
int NSBitsPerPixelFromDepth(int depth) { return cx_depth_bits(depth); }

ui_nsrect NSAccessibilityFrameInView(id view, ui_nsrect frame) {
    if (!view) return frame;
    SEL sel = sel_registerName("convertRect:toView:");
    if (view->isa && cx_responds(view->isa, sel))
        return ((ui_nsrect (*)(id, SEL, ui_nsrect, id))objc_msgSend)(view, sel, frame, 0);
    return frame;
}

void NSDrawNinePartImage(ui_nsrect frame, id tl, id t, id tr, id l, id c, id r, id bl, id b, id br,
                         u64 op, double alpha, char flipped) {
    (void)op; (void)flipped;
    ui_rect parts[9];
    ui_sz corner = {0, 0};
    if (tl && cx_responds(tl->isa, sel_registerName("size"))) corner = ((ui_sz (*)(id, SEL))objc_msgSend)(tl, sel_registerName("size"));
    cx_nine_rects((ui_rect){frame.o.x, frame.o.y, frame.s.w, frame.s.h}, corner.w, corner.h, parts);
    id imgs[9] = {bl, b, br, l, c, r, tl, t, tr};
    SEL draw = sel_registerName("drawInRect:fromRect:operation:fraction:");
    for (int i = 0; i < 9; i++) {
        if (!imgs[i] || !cx_responds(imgs[i]->isa, draw)) continue;
        ui_rect p = parts[i];
        if (p.w < 0.5 || p.h < 0.5) continue;
        ((void (*)(id, SEL, ui_nsrect, ui_nsrect, u64, double))objc_msgSend)(
            imgs[i], draw, (ui_nsrect){{p.x, p.y}, {p.w, p.h}}, (ui_nsrect){{0, 0}, {0, 0}}, op, alpha);
    }
}

typedef struct { void *isa; u64 flags; const char *s; u64 n; } ap_const_t;
extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
#define AK(var, text) static ap_const_t var##_s = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; const void *var = &var##_s;
AK(NSAttachmentAttributeName, "NSAttachment")
AK(NSBaselineOffsetAttributeName, "NSBaselineOffset")
AK(NSLigatureAttributeName, "NSLigature")
AK(NSStrokeWidthAttributeName, "NSStrokeWidth")
AK(NSStrokeColorAttributeName, "NSStrokeColor")
AK(NSSuperscriptAttributeName, "NSSuperscript")
AK(NSRTFTextDocumentType, "NSRTF")
AK(NSTextStorageDidProcessEditingNotification, "NSTextStorageDidProcessEditingNotification")
AK(NSTableViewSelectionDidChangeNotification, "NSTableViewSelectionDidChangeNotification")
AK(NSTableViewSelectionIsChangingNotification, "NSTableViewSelectionIsChangingNotification")
AK(NSScrollViewDidEndLiveScrollNotification, "NSScrollViewDidEndLiveScrollNotification")
AK(NSScrollViewWillStartLiveScrollNotification, "NSScrollViewWillStartLiveScrollNotification")
AK(NSAccessibilityWindowRole, "AXWindow")
AK(NSAccessibilitySheetRole, "AXSheet")
AK(NSAccessibilityDialogSubrole, "AXDialog")
AK(NSAccessibilitySearchFieldSubrole, "AXSearchField")
AK(NSCollectionElementKindSectionHeader, "UICollectionElementKindSectionHeader")
AK(NSCollectionElementKindSectionFooter, "UICollectionElementKindSectionFooter")
AK(NSContentBinding, "content")
AK(NSEnabledBinding, "enabled")
AK(NSObservedObjectKey, "NSObservedObject")
AK(NSObservedKeyPathKey, "NSObservedKeyPath")
AK(NSOptionsKey, "NSOptions")
AK(NSRaisesForNotApplicableKeysBindingOption, "NSRaisesForNotApplicableKeys")
AK(NSValueTransformerBindingOption, "NSValueTransformer")
AK(NSImageHintInterpolation, "NSImageHintInterpolation")
AK(NSSharingServiceNameComposeMessage, "NSSharingServiceNameComposeMessage")
AK(NSWorkspaceLaunchConfigurationArguments, "NSWorkspaceLaunchConfigurationArguments")

void cx_appkit_apps_ui_init(void) {
    DEF(NSTextStorage, "NSMutableAttributedString");
    IM(NSTextStorage, "string", ts_string); IM(NSTextStorage, "length", ts_length);
    IM(NSTextStorage, "replaceCharactersInRange:withString:", ts_replace);
    IM(NSTextStorage, "setAttributes:range:", ts_set_attr);
    IM(NSTextStorage, "addAttribute:value:range:", ts_add_attr);
    IM(NSTextStorage, "attributesAtIndex:effectiveRange:", ts_attrs);
    IM(NSTextStorage, "processEditing", ts_process);

    DEF(NSLayoutManager, "NSObject");
    IM(NSLayoutManager, "init", lm_init);
    IM(NSLayoutManager, "setTextStorage:", lm_set_storage); IM(NSLayoutManager, "textStorage", lm_storage);
    IM(NSLayoutManager, "addTextContainer:", lm_add_tc); IM(NSLayoutManager, "textContainers", lm_containers);
    IM(NSLayoutManager, "numberOfGlyphs", lm_glyphs);
    IM(NSLayoutManager, "glyphRangeForTextContainer:", lm_glyph_range);

    { cx_class_t *tc = cl("NSTextContainer"); if (tc) {
        cx_add_method(tc, "initWithSize:", (void *)tc_init); cx_add_method(tc, "initWithContainerSize:", (void *)tc_init);
        cx_add_method(tc, "containerSize", (void *)tc_size); cx_add_method(tc, "setContainerSize:", (void *)tc_set_size);
        cx_add_method(tc, "size", (void *)tc_size); cx_add_method(tc, "setSize:", (void *)tc_set_size);
        cx_add_method(tc, "layoutManager", (void *)tc_lm);
        cx_add_method(tc, "lineFragmentPadding", (void *)tc_pad); cx_add_method(tc, "setLineFragmentPadding:", (void *)tc_set_pad);
    } }
    { cx_class_t *tt = cl("NSTextTab"); if (tt) {
        cx_add_method(tt, "initWithTextAlignment:location:options:", (void *)tab_init);
        cx_add_method(tt, "alignment", (void *)tab_align); cx_add_method(tt, "location", (void *)tab_loc);
        cx_add_method(tt, "options", (void *)tab_opts);
    } }

    DEF(NSCollectionViewLayout, "NSObject");
    DEF(NSCollectionViewFlowLayout, "NSCollectionViewLayout");
    IM(NSCollectionViewFlowLayout, "setItemSize:", flow_set_size); IM(NSCollectionViewFlowLayout, "itemSize", flow_size);
    IM(NSCollectionViewFlowLayout, "setMinimumInteritemSpacing:", flow_set_gap);
    IM(NSCollectionViewFlowLayout, "setMinimumLineSpacing:", flow_set_gap);
    DEF(NSCollectionViewLayoutAttributes, "NSObject");
    DEF(NSCollectionViewItem, "NSObject");
    IM(NSCollectionViewItem, "view", item_view); IM(NSCollectionViewItem, "setView:", item_set_view);
    IM(NSCollectionViewItem, "representedObject", item_rep); IM(NSCollectionViewItem, "setRepresentedObject:", item_set_rep);
    IM(NSCollectionViewItem, "isSelected", item_sel); IM(NSCollectionViewItem, "setSelected:", item_set_sel);
    DEF(NSCollectionView, "NSView");
    IM(NSCollectionView, "reloadData", cv_reload);
    IM(NSCollectionView, "setDataSource:", cv_set_ds); IM(NSCollectionView, "dataSource", cv_ds);
    IM(NSCollectionView, "setCollectionViewLayout:", cv_set_layout); IM(NSCollectionView, "collectionViewLayout", cv_layout);
    IM(NSCollectionView, "itemAtIndexPath:", cv_item_at);
    IM(NSCollectionView, "makeItemWithIdentifier:forIndexPath:", cv_make_item);
    IM(NSCollectionView, "registerClass:forItemWithIdentifier:", cv_register);

    DEF(NSCollectionLayoutDimension, "NSObject");
    CM(NSCollectionLayoutDimension, "absoluteDimension:", lay_dim);
    CM(NSCollectionLayoutDimension, "fractionalWidthDimension:", lay_dim);
    CM(NSCollectionLayoutDimension, "fractionalHeightDimension:", lay_dim);
    DEF(NSCollectionLayoutSize, "NSObject");
    CM(NSCollectionLayoutSize, "sizeWithWidthDimension:heightDimension:", lay_size);
    DEF(NSCollectionLayoutSpacing, "NSObject");
    DEF(NSCollectionLayoutItem, "NSObject");
    DEF(NSCollectionLayoutGroup, "NSCollectionLayoutItem");
    DEF(NSCollectionLayoutGroupCustomItem, "NSObject");
    DEF(NSCollectionLayoutSection, "NSObject");
    DEF(NSCollectionLayoutBoundarySupplementaryItem, "NSObject");
    DEF(NSCollectionViewCompositionalLayoutConfiguration, "NSObject");
    DEF(NSCollectionViewCompositionalLayout, "NSCollectionViewLayout");

    DEF(NSArrayController, "NSObject");
    IM(NSArrayController, "arrangedObjects", ac_arranged); IM(NSArrayController, "content", ac_arranged);
    IM(NSArrayController, "setContent:", ac_set_content);
    IM(NSArrayController, "addObject:", ac_add); IM(NSArrayController, "removeObject:", ac_remove);
    IM(NSArrayController, "selectionIndex", ac_sel); IM(NSArrayController, "setSelectionIndex:", ac_set_sel);

    { cx_class_t *g = cl("NSClickGestureRecognizer"); if (g) {
        cx_add_method(g, "initWithTarget:action:", (void *)gr_init);
        cx_add_method(g, "setTarget:", (void *)gr_set_target); cx_add_method(g, "target", (void *)gr_target);
        cx_add_method(g, "setAction:", (void *)gr_set_action); cx_add_method(g, "action", (void *)gr_action);
    } }

    DEF(NSDataAsset, "NSObject");
    IM(NSDataAsset, "initWithName:", asset_init_name); IM(NSDataAsset, "initWithName:bundle:", asset_init);
    IM(NSDataAsset, "data", asset_data); IM(NSDataAsset, "name", asset_name);

    DEF(NSCIImageRep, "NSImageRep");
    IM(NSCIImageRep, "initWithCIImage:", ci_init); IM(NSCIImageRep, "CIImage", ci_image);

    { cx_class_t *v = cl("NSView"); if (v) cx_add_method(v, "addGestureRecognizer:", (void *)view_add_gr); if (v) cx_add_method(v, "mouseDown:", (void *)view_mouse_down); }
    /* Símbolos Swift do overlay AppKit. O genérico (snapshot, diffable, metadata Mn)
     * não tem runtime Swift aqui: devolve vazio. NSApplicationMain chama a função C. */
    { cx_class_t *w = cl("NSWindow"); if (w) {
        cx_add_method(w, "beginSheet:completionHandler:", (void *)win_begin_sheet);
        cx_add_method(w, "endSheet:", (void *)win_end_sheet0);
        cx_add_method(w, "endSheet:returnCode:", (void *)win_end_sheet);
    } }
}

/* Overlay Swift do AppKit (libswiftAppKit). Sem metadata de tipo genérico: o dyld resolve
 * e a função devolve 0. NSApplicationMain encaminha para a entrada C. */
#define SWIFT_STUB(fn, sym) void fn(void) __asm__(sym); void fn(void) {}
SWIFT_STUB(sw_ip_item_section, "$s10Foundation9IndexPathV6AppKitE4item7sectionACSi_SitcfC")
SWIFT_STUB(sw_ip_item, "$s10Foundation9IndexPathV6AppKitE4itemSivg")
SWIFT_STUB(sw_ip_section, "$s10Foundation9IndexPathV6AppKitE7sectionSivg")
SWIFT_STUB(sw_num_add, "$s6AppKit01_abB23NumericRawRepresentablePAAE1poiyxx_0D5ValueQztFZ")
SWIFT_STUB(sw_num_sub, "$s6AppKit01_abB23NumericRawRepresentablePAAE1soiyxx_0D5ValueQztFZ")
SWIFT_STUB(sw_snap_items, "$s6AppKit28NSDiffableDataSourceSnapshotV11appendItems_9toSectionySayq_G_xSgtF")
SWIFT_STUB(sw_snap_secs, "$s6AppKit28NSDiffableDataSourceSnapshotV14appendSectionsyySayxGF")
SWIFT_STUB(sw_snap_init, "$s6AppKit28NSDiffableDataSourceSnapshotVACyxq_GycfC")
SWIFT_STUB(sw_snap_mn, "$s6AppKit28NSDiffableDataSourceSnapshotVMn")
SWIFT_STUB(sw_diff_init, "$s6AppKit34NSCollectionViewDiffableDataSourceC010collectionD012itemProviderACyxq_GSo0cD0C_So0cD4ItemCSgAH_10Foundation9IndexPathVq_tctcfc")
SWIFT_STUB(sw_diff_nitems, "$s6AppKit34NSCollectionViewDiffableDataSourceC010collectionD0_22numberOfItemsInSectionSiSo0cD0C_SitFTj")
SWIFT_STUB(sw_diff_nsecs, "$s6AppKit34NSCollectionViewDiffableDataSourceC16numberOfSections2inSiSo0cD0C_tFTj")
SWIFT_STUB(sw_diff_apply, "$s6AppKit34NSCollectionViewDiffableDataSourceC5apply_20animatingDifferences10completionyAA010NSDiffablefG8SnapshotVyxq_G_SbyycSgtFTj")
SWIFT_STUB(sw_diff_mn, "$s6AppKit34NSCollectionViewDiffableDataSourceCMn")
SWIFT_STUB(sw_prio_mc, "$sSo16NSLayoutPrioritya6AppKit01_cdD23NumericRawRepresentableACMc")
SWIFT_STUB(sw_img_lit, "$sSo7NSImageC6AppKitE24imageLiteralResourceNameABSS_tcfC")
