/*
 * mOS libSystem — NSCollectionView completo:
 *   Layouts: NSCollectionViewFlowLayout (itemSize ou delegate sizeForItemAtIndexPath:, espaçamentos, sectionInset,
 *   cabeçalho/rodapé, rolagem vertical ou horizontal), NSCollectionViewGridLayout (linhas/colunas máximas, tamanhos
 *   mínimo/máximo, margens) e NSCollectionViewCompositionalLayout (seção → grupos horizontais/verticais → itens com
 *   dimensões absolutas, frações e estimadas; espaçamento entre grupos, insets e cabeçalho de limite).
 *   NSCollectionViewLayoutAttributes; a view pede ao layout os atributos do retângulo visível e põe os itens.
 *   Itens: registerClass:/registerNib:forItemWithIdentifier:, makeItemWithIdentifier:forIndexPath: com reuso,
 *   supplementary views (cabeçalhos/rodapés), itemAtIndexPath:, indexPathForItem:, visibleItems.
 *   Seleção: selectable, múltipla, vazia, shouldSelect/didSelect/didDeselect, setas e ⌘A, highlightState.
 *   Atualizações: reloadData, reloadItemsAtIndexPaths:, insert/delete/move, performBatchUpdates:completionHandler:,
 *   scrollToItemsAtIndexPaths:scrollPosition:, conteúdo legado (content + itemPrototype).
 * Registro: cx_appkit_collection_init(), chamado do cx_appkit_input_init.
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

typedef struct { double x, y; } P;
typedef struct { double w, h; } S;
typedef struct { double x, y, w, h; } R;
void NSRectFill(R r);

#define K(N) cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N); cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N);
K(NSCollectionViewGridLayout) K(NSCollectionLayoutEdgeSpacing) K(NSCollectionLayoutAnchor) K(NSCollectionLayoutSupplementaryItem)

static cx_class_t *kls(const char *n) { return cx_class_named(n); }
static SEL sl(const char *n) { return sel_registerName(n); }
static BOOL responds(id o, const char *s) { return o && cx_responds(object_getClass(o), sl(s)); }
static R msg_rect(id o, const char *s) { return ((R (*)(id, SEL))objc_msgSend_stret)(o, sl(s)); }
static R mkr(double x, double y, double w, double h) { R r = {x, y, w, h}; return r; }
static char k_props, k_weak;
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
static id rect_val(R r) { return MSG(id, (id)kls("NSValue"), "valueWithRect:", r); }
static R val_rect(id v) { return v ? ((R (*)(id, SEL))objc_msgSend_stret)(v, sl("rectValue")) : mkr(0, 0, 0, 0); }
static id ipath(i64 item, i64 section) { return MSG(id, (id)kls("NSIndexPath"), "indexPathForItem:inSection:", item, section); }
static i64 ip_item(id ip) { return ip ? (i64)MSG(u64, ip, "indexAtPosition:", (u64)1) : -1; }
static i64 ip_section(id ip) { return ip ? (i64)MSG(u64, ip, "indexAtPosition:", (u64)0) : -1; }
static BOOL rects_meet(R a, R b) { return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h; }
static void *block_fn(void *b) { return *(void **)((u8 *)b + 16); }

/* ================================================================ atributos */
static id attrs_make(id ip, R f, i64 cat, id kind) {
    id a = objc_autorelease(cx_alloc_instance(kls("NSCollectionViewLayoutAttributes")));
    pset(a, "ip", ip); pset(a, "frame", rect_val(f)); psd(a, "alpha", 1); psi(a, "cat", cat); pset(a, "kind", kind);
    return a;
}
static id la_for_item(id k, SEL c, id ip) { (void)c; id a = objc_autorelease(cx_alloc_instance((cx_class_t *)k)); pset(a, "ip", ip); psd(a, "alpha", 1); return a; }
static id la_for_supp(id k, SEL c, id kind, id ip) { (void)c; id a = la_for_item(k, 0, ip); pset(a, "kind", kind); psi(a, "cat", 1); return a; }
static R la_frame(id self, SEL c) { (void)c; return val_rect(pget(self, "frame")); }
static void la_set_frame(id self, SEL c, R r) { (void)c; pset(self, "frame", rect_val(r)); }
static S la_size(id self, SEL c) { (void)c; R f = la_frame(self, 0); return (S){f.w, f.h}; }
static void la_set_size(id self, SEL c, S z) { (void)c; R f = la_frame(self, 0); f.w = z.w; f.h = z.h; la_set_frame(self, 0, f); }
static id la_ip(id self, SEL c) { (void)c; return pget(self, "ip"); }
static void la_set_ip(id self, SEL c, id ip) { (void)c; pset(self, "ip", ip); }
static double la_alpha(id self, SEL c) { (void)c; return pgd(self, "alpha", 1); }
static void la_set_alpha(id self, SEL c, double a) { (void)c; psd(self, "alpha", a); }
static i64 la_z(id self, SEL c) { (void)c; return pgi(self, "z", 0); }
static void la_set_z(id self, SEL c, i64 z) { (void)c; psi(self, "z", z); }
static BOOL la_hidden(id self, SEL c) { (void)c; return pgi(self, "hidden", 0) != 0; }
static void la_set_hidden(id self, SEL c, BOOL b) { (void)c; psi(self, "hidden", b); }
static i64 la_cat(id self, SEL c) { (void)c; return pgi(self, "cat", 0); }
static id la_kind(id self, SEL c) { (void)c; return pget(self, "kind"); }
static id la_copy(id self, SEL c, void *z) {
    (void)c; (void)z;
    id n = cx_alloc_instance(object_getClass(self));
    id d = MSG0(id, props(self), "mutableCopy");
    objc_setAssociatedObject(n, &k_props, d, 1); objc_release(d);
    return n;
}

/* ================================================================ layouts */
static id lay_cv(id self, SEL c) { (void)c; return objc_getAssociatedObject(self, &k_weak); }
static void lay_set_cv(id self, SEL c, id cv) { (void)c; objc_setAssociatedObject(self, &k_weak, cv, 0); }
static i64 n_sections(id cv) { id ds = MSG0(id, cv, "dataSource"); return ds && responds(ds, "numberOfSectionsInCollectionView:") ? MSG(i64, ds, "numberOfSectionsInCollectionView:", cv) : 1; }
static i64 n_items(id cv, i64 s) {
    id ds = MSG0(id, cv, "dataSource");
    if (ds && responds(ds, "collectionView:numberOfItemsInSection:")) return MSG(i64, ds, "collectionView:numberOfItemsInSection:", cv, s);
    id content = pget(cv, "content");
    return content && s == 0 ? (i64)MSG0(u64, content, "count") : 0;
}
static R view_bounds(id cv) {
    id sv = MSG0(id, cv, "superview");
    R b = sv && responds(sv, "documentVisibleRect") ? msg_rect(sv, "bounds") : msg_rect(cv, "bounds");
    return b;
}
/* cache de atributos: calculado no prepareLayout e lido pelas consultas */
static id cache_of(id self) { id a = pget(self, "cache"); return a ? a : arr(); }
static void lay_invalidate(id self, SEL c) { (void)c; pset(self, "cache", 0); id cv = lay_cv(self, 0); if (cv) MSG(void, cv, "setNeedsLayout:", (BOOL)YES); }
static void lay_invalidate_ctx(id self, SEL c, id ctx) { (void)ctx; lay_invalidate(self, c); }
static S lay_content(id self, SEL c) { (void)c; MSG0(void, self, "prepareLayout"); return (S){pgd(self, "cw", 0), pgd(self, "ch", 0)}; }
static id lay_elements_in(id self, SEL c, R r) {
    (void)c;
    if (!pget(self, "cache")) MSG0(void, self, "prepareLayout");
    id out = arr(), all = cache_of(self);
    for (u64 i = 0, n = MSG0(u64, all, "count"); i < n; i++) { id a = MSG(id, all, "objectAtIndex:", i); if (rects_meet(la_frame(a, 0), r)) MSG(void, out, "addObject:", a); }
    return out;
}
static id lay_attrs_item(id self, SEL c, id ip) {
    (void)c;
    if (!pget(self, "cache")) MSG0(void, self, "prepareLayout");
    id all = cache_of(self);
    for (u64 i = 0, n = MSG0(u64, all, "count"); i < n; i++) {
        id a = MSG(id, all, "objectAtIndex:", i);
        if (pgi(a, "cat", 0) == 0 && MSG(BOOL, pget(a, "ip"), "isEqual:", ip)) return a;
    }
    return 0;
}
static id lay_attrs_supp(id self, SEL c, id kind, id ip) {
    (void)c;
    if (!pget(self, "cache")) MSG0(void, self, "prepareLayout");
    id all = cache_of(self);
    for (u64 i = 0, n = MSG0(u64, all, "count"); i < n; i++) {
        id a = MSG(id, all, "objectAtIndex:", i);
        if (pgi(a, "cat", 0) == 1 && MSG(BOOL, pget(a, "kind"), "isEqual:", kind) && ip_section(pget(a, "ip")) == ip_section(ip)) return a;
    }
    return 0;
}
static BOOL lay_should_invalidate_bounds(id self, SEL c, R nb) { (void)c; return nb.w != pgd(self, "lastW", -1) || (pgi(self, "horiz", 0) && nb.h != pgd(self, "lastH", -1)); }
static id lay_attrs_class(id k, SEL c) { (void)k; (void)c; return (id)kls("NSCollectionViewLayoutAttributes"); }
static void lay_prepare_noop(id self, SEL c) { (void)c; if (!pget(self, "cache")) pset(self, "cache", arr()); }
extern id NSCollectionElementKindSectionHeader, NSCollectionElementKindSectionFooter;

/* ---- fluxo */
static S flow_item_size(id self, id cv, id ip) {
    id d = MSG0(id, cv, "delegate");
    if (d && responds(d, "collectionView:layout:sizeForItemAtIndexPath:")) return ((S (*)(id, SEL, id, id, id))objc_msgSend)(d, sl("collectionView:layout:sizeForItemAtIndexPath:"), cv, self, ip);
    return (S){pgd(self, "iw", 50), pgd(self, "ih", 50)};
}
static double flow_spacing(id self, id cv, i64 s, const char *sel_, const char *key, double def) {
    id d = MSG0(id, cv, "delegate");
    if (d && responds(d, sel_)) return MSG(double, d, sel_, cv, self, s);
    return pgd(self, key, def);
}
typedef struct { double top, left, bottom, right; } EI;
static EI flow_inset(id self, id cv, i64 s) {
    id d = MSG0(id, cv, "delegate");
    if (d && responds(d, "collectionView:layout:insetForSectionAtIndex:")) return ((EI (*)(id, SEL, id, id, i64))objc_msgSend_stret)(d, sl("collectionView:layout:insetForSectionAtIndex:"), cv, self, s);
    return (EI){pgd(self, "it", 0), pgd(self, "il", 0), pgd(self, "ib", 0), pgd(self, "ir", 0)};
}
static S flow_ref(id self, id cv, i64 s, int header) {
    id d = MSG0(id, cv, "delegate");
    const char *ds = header ? "collectionView:layout:referenceSizeForHeaderInSection:" : "collectionView:layout:referenceSizeForFooterInSection:";
    if (d && responds(d, ds)) return ((S (*)(id, SEL, id, id, i64))objc_msgSend)(d, sl(ds), cv, self, s);
    return header ? (S){pgd(self, "hw", 0), pgd(self, "hh", 0)} : (S){pgd(self, "fw", 0), pgd(self, "fh", 0)};
}
static void flow_prepare(id self, SEL c) {
    (void)c;
    id cv = lay_cv(self, 0);
    id cache = arr();
    pset(self, "cache", cache);
    if (!cv) return;
    R vb = view_bounds(cv);
    BOOL horiz = pgi(self, "horiz", 0) != 0;
    psd(self, "lastW", vb.w); psd(self, "lastH", vb.h);
    double extent = horiz ? vb.h : vb.w;   /* largura das linhas (vertical) ou altura das colunas (horizontal) */
    double pos = 0, cross_max = 0;
    for (i64 s = 0, ns = n_sections(cv); s < ns; s++) {
        EI in = flow_inset(self, cv, s);
        double ispace = flow_spacing(self, cv, s, "collectionView:layout:minimumInteritemSpacingForSectionAtIndex:", "interSp", 10);
        double lspace = flow_spacing(self, cv, s, "collectionView:layout:minimumLineSpacingForSectionAtIndex:", "lineSp", 10);
        S hs = flow_ref(self, cv, s, 1);
        if ((horiz ? hs.w : hs.h) > 0) {
            R hf = horiz ? mkr(pos, 0, hs.w, vb.h) : mkr(0, pos, vb.w, hs.h);
            MSG(void, cache, "addObject:", attrs_make(ipath(0, s), hf, 1, NSCollectionElementKindSectionHeader));
            pos += horiz ? hs.w : hs.h;
        }
        pos += horiz ? in.left : in.top;
        double lead = horiz ? in.top : in.left, avail = extent - lead - (horiz ? in.bottom : in.right);
        double cursor = 0, line_depth = 0;
        i64 ni = n_items(cv, s);
        for (i64 i = 0; i < ni; i++) {
            id ip = ipath(i, s);
            S z = flow_item_size(self, cv, ip);
            double along = horiz ? z.h : z.w, depth = horiz ? z.w : z.h;
            if (cursor > 0 && cursor + along > avail) { pos += line_depth + lspace; cursor = 0; line_depth = 0; }
            R f = horiz ? mkr(pos, lead + cursor, z.w, z.h) : mkr(lead + cursor, pos, z.w, z.h);
            MSG(void, cache, "addObject:", attrs_make(ip, f, 0, 0));
            cursor += along + ispace;
            if (depth > line_depth) line_depth = depth;
            if (cursor > cross_max) cross_max = cursor;
        }
        pos += line_depth + (horiz ? in.right : in.bottom);
        S fs = flow_ref(self, cv, s, 0);
        if ((horiz ? fs.w : fs.h) > 0) {
            R ff = horiz ? mkr(pos, 0, fs.w, vb.h) : mkr(0, pos, vb.w, fs.h);
            MSG(void, cache, "addObject:", attrs_make(ipath(0, s), ff, 1, NSCollectionElementKindSectionFooter));
            pos += horiz ? fs.w : fs.h;
        }
    }
    if (horiz) { psd(self, "cw", pos); psd(self, "ch", vb.h); } else { psd(self, "cw", vb.w); psd(self, "ch", pos); }
}
#define FP(get, key, def) static double flg_##get(id self, SEL c) { (void)c; return pgd(self, key, def); } static void fls_##get(id self, SEL c, double v) { (void)c; psd(self, key, v); lay_invalidate(self, 0); }
FP(inter, "interSp", 10) FP(line, "lineSp", 10)
static S flow_get_size(id self, SEL c) { (void)c; return (S){pgd(self, "iw", 50), pgd(self, "ih", 50)}; }
static void flow_set_size(id self, SEL c, S z) { (void)c; psd(self, "iw", z.w); psd(self, "ih", z.h); lay_invalidate(self, 0); }
static S flow_est(id self, SEL c) { (void)c; return (S){pgd(self, "ew", 0), pgd(self, "eh", 0)}; }
static void flow_set_est(id self, SEL c, S z) { (void)c; psd(self, "ew", z.w); psd(self, "eh", z.h); if (z.w > 0) flow_set_size(self, 0, z); }
static EI flow_get_inset(id self, SEL c) { (void)c; return (EI){pgd(self, "it", 0), pgd(self, "il", 0), pgd(self, "ib", 0), pgd(self, "ir", 0)}; }
static void flow_set_inset(id self, SEL c, EI e) { (void)c; psd(self, "it", e.top); psd(self, "il", e.left); psd(self, "ib", e.bottom); psd(self, "ir", e.right); lay_invalidate(self, 0); }
static S flow_header(id self, SEL c) { (void)c; return (S){pgd(self, "hw", 0), pgd(self, "hh", 0)}; }
static void flow_set_header(id self, SEL c, S z) { (void)c; psd(self, "hw", z.w); psd(self, "hh", z.h); lay_invalidate(self, 0); }
static S flow_footer(id self, SEL c) { (void)c; return (S){pgd(self, "fw", 0), pgd(self, "fh", 0)}; }
static void flow_set_footer(id self, SEL c, S z) { (void)c; psd(self, "fw", z.w); psd(self, "fh", z.h); lay_invalidate(self, 0); }
static i64 flow_dir(id self, SEL c) { (void)c; return pgi(self, "horiz", 0); }
static void flow_set_dir(id self, SEL c, i64 d) { (void)c; psi(self, "horiz", d); lay_invalidate(self, 0); }
static BOOL flow_pins(id self, SEL c) { (void)c; return pgi(self, "pinH", 0) != 0; }
static void flow_set_pins(id self, SEL c, BOOL b) { (void)c; psi(self, "pinH", b); }

/* ---- grade */
static void grid_prepare(id self, SEL c) {
    (void)c;
    id cv = lay_cv(self, 0);
    id cache = arr();
    pset(self, "cache", cache);
    if (!cv) return;
    R vb = view_bounds(cv);
    psd(self, "lastW", vb.w);
    i64 n = n_items(cv, 0);
    double mx = pgd(self, "marX", 10), my = pgd(self, "marY", 10), sx = pgd(self, "spX", 10), sy = pgd(self, "spY", 10);
    S mn = {pgd(self, "minW", 50), pgd(self, "minH", 50)}, mxs = {pgd(self, "maxW", 0), pgd(self, "maxH", 0)};
    i64 maxc = pgi(self, "maxCols", 0), maxr = pgi(self, "maxRows", 0);
    i64 cols = (i64)((vb.w - 2 * mx + sx) / (mn.w + sx));
    if (cols < 1) cols = 1;
    if (maxc > 0 && cols > maxc) cols = maxc;
    double w = (vb.w - 2 * mx - (cols - 1) * sx) / cols;
    if (mxs.w > 0 && w > mxs.w) w = mxs.w;
    double h = mn.h;
    if (mxs.h > 0 && h > mxs.h) h = mxs.h;
    i64 rows = (n + cols - 1) / cols;
    if (maxr > 0 && rows > maxr) rows = maxr;
    for (i64 i = 0; i < n && i / cols < rows; i++) {
        i64 r = i / cols, k = i % cols;
        MSG(void, cache, "addObject:", attrs_make(ipath(i, 0), mkr(mx + k * (w + sx), my + r * (h + sy), w, h), 0, 0));
    }
    psd(self, "cw", vb.w); psd(self, "ch", 2 * my + rows * h + (rows > 1 ? (rows - 1) * sy : 0));
}
#define GP(get, key, def) static double grg_##get(id self, SEL c) { (void)c; return pgd(self, key, def); } static void grs_##get(id self, SEL c, double v) { (void)c; psd(self, key, v); lay_invalidate(self, 0); }
GP(marX, "marX", 10) GP(marY, "marY", 10)
static i64 grid_maxc(id self, SEL c) { (void)c; return pgi(self, "maxCols", 0); }
static void grid_set_maxc(id self, SEL c, i64 v) { (void)c; psi(self, "maxCols", v); lay_invalidate(self, 0); }
static i64 grid_maxr(id self, SEL c) { (void)c; return pgi(self, "maxRows", 0); }
static void grid_set_maxr(id self, SEL c, i64 v) { (void)c; psi(self, "maxRows", v); lay_invalidate(self, 0); }
static S grid_min(id self, SEL c) { (void)c; return (S){pgd(self, "minW", 50), pgd(self, "minH", 50)}; }
static void grid_set_min(id self, SEL c, S z) { (void)c; psd(self, "minW", z.w); psd(self, "minH", z.h); lay_invalidate(self, 0); }
static S grid_max(id self, SEL c) { (void)c; return (S){pgd(self, "maxW", 0), pgd(self, "maxH", 0)}; }
static void grid_set_max(id self, SEL c, S z) { (void)c; psd(self, "maxW", z.w); psd(self, "maxH", z.h); lay_invalidate(self, 0); }
static S grid_margins_get(id self, SEL c) { (void)c; return (S){pgd(self, "marX", 10), pgd(self, "marY", 10)}; }
static S grid_spacing(id self, SEL c) { (void)c; return (S){pgd(self, "spX", 10), pgd(self, "spY", 10)}; }
static void grid_set_spacing(id self, SEL c, S z) { (void)c; psd(self, "spX", z.w); psd(self, "spY", z.h); lay_invalidate(self, 0); }
static EI grid_margins(id self, SEL c) { (void)c; double x = pgd(self, "marX", 10), y = pgd(self, "marY", 10); return (EI){y, x, y, x}; }
static void grid_set_margins(id self, SEL c, EI e) { (void)c; psd(self, "marX", e.left); psd(self, "marY", e.top); lay_invalidate(self, 0); }

/* ---- compositional: dimensões, tamanhos, itens, grupos, seções */
/* dimensão: tipo 0 absoluta, 1 fração da largura, 2 fração da altura, 3 estimada */
static id dim_make(id k, double v, int t) { id d = objc_autorelease(cx_alloc_instance((cx_class_t *)k)); psd(d, "v", v); psi(d, "t", t); return d; }
static id dim_abs(id k, SEL c, double v) { (void)c; return dim_make(k, v, 0); }
static id dim_fw(id k, SEL c, double v) { (void)c; return dim_make(k, v, 1); }
static id dim_fh(id k, SEL c, double v) { (void)c; return dim_make(k, v, 2); }
static id dim_est(id k, SEL c, double v) { (void)c; return dim_make(k, v, 3); }
static double dim_value(id self, SEL c) { (void)c; return pgd(self, "v", 0); }
static BOOL dim_is(id self, int t) { return pgi(self, "t", 0) == t; }
static BOOL dim_is_abs(id self, SEL c) { (void)c; return dim_is(self, 0); }
static BOOL dim_is_fw(id self, SEL c) { (void)c; return dim_is(self, 1); }
static BOOL dim_is_fh(id self, SEL c) { (void)c; return dim_is(self, 2); }
static BOOL dim_is_est(id self, SEL c) { (void)c; return dim_is(self, 3); }
static double resolve(id d, double cw, double ch) { if (!d) return 0; double v = pgd(d, "v", 0); int t = (int)pgi(d, "t", 0); return t == 1 ? v * cw : t == 2 ? v * ch : v; }
static id size_make(id k, SEL c, id w, id h) { (void)c; id s = objc_autorelease(cx_alloc_instance((cx_class_t *)k)); pset(s, "w", w); pset(s, "h", h); return s; }
static id size_w(id self, SEL c) { (void)c; return pget(self, "w"); }
static id size_h(id self, SEL c) { (void)c; return pget(self, "h"); }
static id item_make(id k, SEL c, id size) { (void)c; id i = objc_autorelease(cx_alloc_instance((cx_class_t *)k)); pset(i, "size", size); return i; }
static id item_make_supp(id k, SEL c, id size, id supps) { id i = item_make(k, c, size); pset(i, "supps", supps); return i; }
static id item_size(id self, SEL c) { (void)c; return pget(self, "size"); }
static EI item_insets(id self, SEL c) { (void)c; return (EI){pgd(self, "it", 0), pgd(self, "il", 0), pgd(self, "ib", 0), pgd(self, "ir", 0)}; }
static void item_set_insets(id self, SEL c, EI e) { (void)c; psd(self, "it", e.top); psd(self, "il", e.left); psd(self, "ib", e.bottom); psd(self, "ir", e.right); }
/* grupo: horizontal (eixo 0) ou vertical (1), subitens ou um item repetido count vezes */
static id group_make(id k, id size, id items, i64 count, int axis) {
    id g = objc_autorelease(cx_alloc_instance((cx_class_t *)k));
    pset(g, "size", size); pset(g, "items", items); psi(g, "count", count); psi(g, "axis", axis);
    return g;
}
static id group_h(id k, SEL c, id size, id items) { (void)c; return group_make(k, size, items, 0, 0); }
static id group_v(id k, SEL c, id size, id items) { (void)c; return group_make(k, size, items, 0, 1); }
static id group_h_count(id k, SEL c, id size, id item, i64 n) { (void)c; return group_make(k, size, MSG(id, (id)kls("NSArray"), "arrayWithObject:", item), n, 0); }
static id group_v_count(id k, SEL c, id size, id item, i64 n) { (void)c; return group_make(k, size, MSG(id, (id)kls("NSArray"), "arrayWithObject:", item), n, 1); }
static id group_h_repeat(id k, SEL c, id size, id item, i64 n) { return group_h_count(k, c, size, item, n); }
static id group_items(id self, SEL c) { (void)c; return pget(self, "items"); }
static id group_spacing(id self, SEL c) { (void)c; return pget(self, "spacing"); }
static void group_set_spacing(id self, SEL c, id sp) { (void)c; pset(self, "spacing", sp); }
static id spacing_fixed(id k, SEL c, double v) { (void)c; id s = objc_autorelease(cx_alloc_instance((cx_class_t *)k)); psd(s, "v", v); return s; }
static id spacing_flexible(id k, SEL c, double v) { (void)c; id s = objc_autorelease(cx_alloc_instance((cx_class_t *)k)); psd(s, "v", v); psi(s, "flex", 1); return s; }
static double spacing_value(id self, SEL c) { (void)c; return pgd(self, "v", 0); }
static id section_make(id k, SEL c, id group) { (void)c; id s = objc_autorelease(cx_alloc_instance((cx_class_t *)k)); pset(s, "group", group); return s; }
static double section_spacing(id self, SEL c) { (void)c; return pgd(self, "interGroup", 0); }
static void section_set_spacing(id self, SEL c, double v) { (void)c; psd(self, "interGroup", v); }
static EI section_insets(id self, SEL c) { return item_insets(self, c); }
static void section_set_insets(id self, SEL c, EI e) { item_set_insets(self, c, e); }
static id section_supps(id self, SEL c) { (void)c; return pget(self, "supps"); }
static void section_set_supps(id self, SEL c, id a) { (void)c; pset(self, "supps", a); }
static i64 section_scrolling(id self, SEL c) { (void)c; return pgi(self, "ortho", 0); }
static void section_set_scrolling(id self, SEL c, i64 v) { (void)c; psi(self, "ortho", v); }
static id boundary_make(id k, SEL c, id size, id kind, i64 align) { (void)c; id b = objc_autorelease(cx_alloc_instance((cx_class_t *)k)); pset(b, "size", size); pset(b, "kind", kind); psi(b, "align", align); return b; }
static id boundary_kind(id self, SEL c) { (void)c; return pget(self, "kind"); }
static id cl_init_section(id self, SEL c, id section) { (void)c; pset(self, "section", section); return self; }
static id cl_init_provider(id self, SEL c, void *block) { (void)c; if (block) { extern void *_Block_copy(const void *); pset(self, "providerV", MSG(id, (id)kls("NSValue"), "valueWithPointer:", _Block_copy(block))); } return self; }
static id cl_init_section_cfg(id self, SEL c, id section, id cfg) { (void)cfg; return cl_init_section(self, c, section); }
static id cl_init_provider_cfg(id self, SEL c, void *block, id cfg) { (void)cfg; return cl_init_provider(self, c, block); }
static id section_for(id self, i64 s, double width) {
    id pv = pget(self, "providerV");
    if (pv) {
        void *b = MSG0(void *, pv, "pointerValue");
        id env = 0;
        (void)width;
        return ((id (*)(void *, i64, id))block_fn(b))(b, s, env);
    }
    return pget(self, "section");
}
/* põe um grupo em (x, y) com a largura/altura do contêiner; devolve o tamanho usado e consome itens a partir de *next */
static S place_group(id g, double x, double y, double cw, double ch, i64 sec, i64 *next, i64 total, id cache) {
    id gs = pget(g, "size");
    double gw = resolve(pget(gs, "w"), cw, ch), gh = resolve(pget(gs, "h"), cw, ch);
    int axis = (int)pgi(g, "axis", 0);
    id items = pget(g, "items");
    i64 count = pgi(g, "count", 0);
    double sp = pget(g, "spacing") ? pgd(pget(g, "spacing"), "v", 0) : 0;
    u64 nsub = items ? MSG0(u64, items, "count") : 0;
    i64 slots = count > 0 ? count : (i64)nsub;
    double cur = 0, deep = 0;
    for (i64 k = 0; k < slots && *next < total; k++) {
        id it = MSG(id, items, "objectAtIndex:", (u64)(count > 0 ? 0 : k));
        if (pget(it, "items")) {   /* grupo aninhado */
            S z = place_group(it, axis ? x : x + cur, axis ? y + cur : y, gw, gh, sec, next, total, cache);
            cur += (axis ? z.h : z.w) + sp;
            double d = axis ? z.w : z.h; if (d > deep) deep = d;
            continue;
        }
        id isz = pget(it, "size");
        double iw = resolve(pget(isz, "w"), gw, gh), ih = resolve(pget(isz, "h"), gw, gh);
        if (count > 0) { if (axis == 0) iw = (gw - sp * (count - 1)) / count; else ih = (gh - sp * (count - 1)) / count; }
        EI in = item_insets(it, 0);
        R f = axis ? mkr(x + in.left, y + cur + in.top, iw - in.left - in.right, ih - in.top - in.bottom)
                   : mkr(x + cur + in.left, y + in.top, iw - in.left - in.right, ih - in.top - in.bottom);
        MSG(void, cache, "addObject:", attrs_make(ipath(*next, sec), f, 0, 0));
        (*next)++;
        cur += (axis ? ih : iw) + sp;
        double d = axis ? iw : ih; if (d > deep) deep = d;
    }
    if (axis == 0 && gh <= 0) gh = deep;
    if (axis == 1 && gw <= 0) gw = deep;
    return (S){gw, gh};
}
static void cl_prepare(id self, SEL c) {
    (void)c;
    id cv = lay_cv(self, 0);
    id cache = arr();
    pset(self, "cache", cache);
    if (!cv) return;
    R vb = view_bounds(cv);
    psd(self, "lastW", vb.w);
    double y = 0;
    for (i64 s = 0, ns = n_sections(cv); s < ns; s++) {
        id sec = section_for(self, s, vb.w);
        if (!sec) continue;
        EI in = item_insets(sec, 0);
        double cw = vb.w - in.left - in.right;
        id supps = pget(sec, "supps");
        for (u64 b = 0, nb = supps ? MSG0(u64, supps, "count") : 0; b < nb; b++) {   /* cabeçalho no topo da seção */
            id bi = MSG(id, supps, "objectAtIndex:", b);
            if (pgi(bi, "align", 1) != 1) continue;   /* NSRectAlignmentTop */
            double h = resolve(pget(pget(bi, "size"), "h"), cw, vb.h);
            MSG(void, cache, "addObject:", attrs_make(ipath(0, s), mkr(in.left, y, cw, h), 1, pget(bi, "kind")));
            y += h;
        }
        y += in.top;
        i64 next = 0, total = n_items(cv, s);
        double gap = pgd(sec, "interGroup", 0);
        while (next < total) {
            i64 before = next;
            S z = place_group(pget(sec, "group"), in.left, y, cw, vb.h, s, &next, total, cache);
            y += z.h + gap;
            if (next == before) break;
        }
        y += in.bottom;
        for (u64 b = 0, nb = supps ? MSG0(u64, supps, "count") : 0; b < nb; b++) {
            id bi = MSG(id, supps, "objectAtIndex:", b);
            if (pgi(bi, "align", 1) != 5) continue;   /* NSRectAlignmentBottom */
            double h = resolve(pget(pget(bi, "size"), "h"), cw, vb.h);
            MSG(void, cache, "addObject:", attrs_make(ipath(0, s), mkr(in.left, y, cw, h), 1, pget(bi, "kind")));
            y += h;
        }
    }
    psd(self, "cw", vb.w); psd(self, "ch", y);
}

/* ================================================================ NSCollectionView */
static id cv_layout_obj(id self) { return pget(self, "layoutObj"); }
static void cv_set_layout(id self, SEL c, id lay) { (void)c; pset(self, "layoutObj", lay); if (lay) MSG(void, lay, "setCollectionView:", self); MSG0(void, self, "reloadData"); }
static id cv_get_layout(id self, SEL c) { (void)c; return cv_layout_obj(self); }
static id cv_ds(id self, SEL c) { (void)c; return objc_getAssociatedObject(self, &k_weak); }
static void cv_set_ds(id self, SEL c, id ds) { (void)c; objc_setAssociatedObject(self, &k_weak, ds, 0); }
static char k_delegate;
static id cv_delegate(id self, SEL c) { (void)c; return objc_getAssociatedObject(self, &k_delegate); }
static void cv_set_delegate(id self, SEL c, id d) { (void)c; objc_setAssociatedObject(self, &k_delegate, d, 0); }
static id reg(id self) { id d = pget(self, "reg"); if (!d) { d = MSG0(id, (id)kls("NSMutableDictionary"), "dictionary"); pset(self, "reg", d); } return d; }
static id reuse(id self) { id d = pget(self, "reuse"); if (!d) { d = MSG0(id, (id)kls("NSMutableDictionary"), "dictionary"); pset(self, "reuse", d); } return d; }
static void cv_register_class(id self, SEL c, id k, id ident) { (void)c; if (ident) { if (k) MSG(void, reg(self), "setObject:forKey:", k, ident); else MSG(void, reg(self), "removeObjectForKey:", ident); } }
static void cv_register_nib(id self, SEL c, id nib, id ident) { (void)c; if (ident && nib) MSG(void, reg(self), "setObject:forKey:", nib, ident); }
static id key_supp(id kind, id ident) { return MSG(id, kind, "stringByAppendingFormat:", cx_str("/%@"), ident); }
static void cv_register_supp(id self, SEL c, id k, id kind, id ident) { (void)c; if (k && kind && ident) MSG(void, reg(self), "setObject:forKey:", k, key_supp(kind, ident)); }
static void cv_register_supp_nib(id self, SEL c, id nib, id kind, id ident) { cv_register_supp(self, c, nib, kind, ident); }
static id instantiate(id self, id key) {
    id q = MSG(id, reuse(self), "objectForKey:", key);
    if (q && MSG0(u64, q, "count")) { id o = objc_retain(MSG0(id, q, "lastObject")); MSG0(void, q, "removeLastObject"); if (responds(o, "prepareForReuse")) MSG0(void, o, "prepareForReuse"); return objc_autorelease(o); }
    id k = MSG(id, reg(self), "objectForKey:", key);
    if (!k) return 0;
    if (responds(k, "instantiateWithOwner:topLevelObjects:")) {   /* nib: o primeiro objeto que é item/view */
        id tops = 0;
        MSG(BOOL, k, "instantiateWithOwner:topLevelObjects:", (id)0, &tops);
        for (u64 i = 0, n = tops ? MSG0(u64, tops, "count") : 0; i < n; i++) {
            id o = MSG(id, tops, "objectAtIndex:", i);
            if (MSG(BOOL, o, "isKindOfClass:", (id)kls("NSCollectionViewItem")) || MSG(BOOL, o, "isKindOfClass:", (id)kls("NSView"))) return o;
        }
        return 0;
    }
    id o = objc_autorelease(MSG0(id, MSG0(id, k, "alloc"), "init"));
    if (MSG(BOOL, o, "isKindOfClass:", (id)kls("NSCollectionViewItem")) && !MSG0(id, o, "view")) {
        id v = objc_autorelease(((id (*)(id, SEL, R))objc_msgSend)(MSG0(id, (id)kls("NSView"), "alloc"), sl("initWithFrame:"), mkr(0, 0, 50, 50)));
        MSG(void, o, "setView:", v);
    }
    pset(o, "reuseKey", key);
    return o;
}
static id cv_make_item(id self, SEL c, id ident, id ip) { (void)c; (void)ip; id o = instantiate(self, ident); if (o) pset(o, "reuseKey", ident); return o; }
static id cv_make_supp(id self, SEL c, id kind, id ident, id ip) { (void)c; (void)ip; id key = key_supp(kind, ident); id o = instantiate(self, key); if (o) pset(o, "reuseKey", key); return o; }
/* itens na tela: chave "s:i" → item */
static id visible(id self) { id d = pget(self, "visible"); if (!d) { d = MSG0(id, (id)kls("NSMutableDictionary"), "dictionary"); pset(self, "visible", d); } return d; }
static id ipkey(id ip) { char b[48]; snprintf(b, sizeof b, "%lld:%lld", (long long)ip_section(ip), (long long)ip_item(ip)); return cx_str(b); }
static id item_view_of(id it) { return MSG(BOOL, it, "isKindOfClass:", (id)kls("NSView")) ? it : MSG0(id, it, "view"); }
static void recycle(id self, id it) {
    id key = pget(it, "reuseKey");
    id v = item_view_of(it);
    if (v) MSG0(void, v, "removeFromSuperview");
    if (!key) return;
    id q = MSG(id, reuse(self), "objectForKey:", key);
    if (!q) { q = arr(); MSG(void, reuse(self), "setObject:forKey:", q, key); }
    MSG(void, q, "addObject:", it);
}
static id selset(id self) { id s = pget(self, "sel"); if (!s) { s = MSG0(id, (id)kls("NSMutableSet"), "set"); pset(self, "sel", s); } return s; }
static void apply_selected(id self, id it, id ip) {
    BOOL on = MSG(BOOL, selset(self), "containsObject:", ip);
    if (responds(it, "setSelected:")) MSG(void, it, "setSelected:", on);
}
static void cv_tile(id self) {
    id lay = cv_layout_obj(self);
    if (!lay || pgi(self, "tiling", 0)) return;
    psi(self, "tiling", 1);
    pset(lay, "cache", 0);
    MSG0(void, lay, "prepareLayout");
    S cs = ((S (*)(id, SEL))objc_msgSend)(lay, sl("collectionViewContentSize"));
    R f = msg_rect(self, "frame");
    R vb = view_bounds(self);
    double h = cs.h > vb.h ? cs.h : vb.h, w = cs.w > vb.w ? cs.w : vb.w;
    if (f.w != w || f.h != h) ((void (*)(id, SEL, S))objc_msgSend)(self, sl("setFrameSize:"), (S){w, h});
    id ds = cv_ds(self, 0);
    id vis = visible(self);
    id keep = MSG0(id, (id)kls("NSMutableSet"), "set");
    id all = ((id (*)(id, SEL, R))objc_msgSend)(lay, sl("layoutAttributesForElementsInRect:"), mkr(0, 0, w, h));
    for (u64 i = 0, n = MSG0(u64, all, "count"); i < n; i++) {
        id a = MSG(id, all, "objectAtIndex:", i);
        id ip = pget(a, "ip");
        int supp = pgi(a, "cat", 0) == 1;
        id key = supp ? MSG(id, pget(a, "kind"), "stringByAppendingString:", ipkey(ip)) : ipkey(ip);
        id it = MSG(id, vis, "objectForKey:", key);
        if (!it) {
            if (supp) it = ds && responds(ds, "collectionView:viewForSupplementaryElementOfKind:atIndexPath:") ? MSG(id, ds, "collectionView:viewForSupplementaryElementOfKind:atIndexPath:", self, pget(a, "kind"), ip) : 0;
            else if (ds && responds(ds, "collectionView:itemForRepresentedObjectAtIndexPath:")) it = MSG(id, ds, "collectionView:itemForRepresentedObjectAtIndexPath:", self, ip);
            else if (pget(self, "content")) {   /* conteúdo legado: cópia do itemPrototype */
                id proto = pget(self, "proto");
                it = proto ? objc_autorelease(MSG(id, proto, "copyWithZone:", (void *)0)) : instantiate(self, cx_str(""));
                if (it) { id ro = MSG(id, pget(self, "content"), "objectAtIndex:", (u64)ip_item(ip)); MSG(void, it, "setRepresentedObject:", ro); }
            }
            if (!it) continue;
            MSG(void, vis, "setObject:forKey:", it, key);
            if (responds(it, "setCollectionView:")) MSG(void, it, "setCollectionView:", self);
            id d = cv_delegate(self, 0);
            if (!supp && d && responds(d, "collectionView:willDisplayItem:forRepresentedObjectAtIndexPath:")) MSG(void, d, "collectionView:willDisplayItem:forRepresentedObjectAtIndexPath:", self, it, ip);
        }
        pset(it, "ip", ip);
        id v = item_view_of(it);
        if (v) {
            ((void (*)(id, SEL, R))objc_msgSend)(v, sl("setFrame:"), la_frame(a, 0));
            if (MSG0(id, v, "superview") != self) MSG(void, self, "addSubview:", v);
            MSG(void, v, "setHidden:", (BOOL)pgi(a, "hidden", 0));
        }
        if (!supp) apply_selected(self, it, ip);
        MSG(void, keep, "addObject:", key);
    }
    id keys = MSG0(id, vis, "allKeys");
    for (u64 i = 0, n = MSG0(u64, keys, "count"); i < n; i++) {
        id k = MSG(id, keys, "objectAtIndex:", i);
        if (MSG(BOOL, keep, "containsObject:", k)) continue;
        recycle(self, MSG(id, vis, "objectForKey:", k));
        MSG(void, vis, "removeObjectForKey:", k);
    }
    psi(self, "tiling", 0);
    MSG(void, self, "setNeedsDisplay:", (BOOL)YES);
}
static void cv_reload(id self, SEL c) {
    (void)c;
    id vis = visible(self);
    id vals = MSG0(id, vis, "allValues");
    for (u64 i = 0, n = MSG0(u64, vals, "count"); i < n; i++) recycle(self, MSG(id, vals, "objectAtIndex:", i));
    MSG0(void, vis, "removeAllObjects");
    cv_tile(self);
}
static void cv_reload_items(id self, SEL c, id ips) {
    (void)c;
    id vis = visible(self);
    id list = MSG0(id, ips, "allObjects");
    for (u64 i = 0, n = MSG0(u64, list, "count"); i < n; i++) {
        id k = ipkey(MSG(id, list, "objectAtIndex:", i));
        id it = MSG(id, vis, "objectForKey:", k);
        if (it) { recycle(self, it); MSG(void, vis, "removeObjectForKey:", k); }
    }
    cv_tile(self);
}
static void cv_changed(id self, SEL c, id x) { (void)x; cv_reload(self, c); }
static void cv_move_item(id self, SEL c, id a, id b) { (void)a; (void)b; cv_reload(self, c); }
static void cv_batch(id self, SEL c, void *updates, void *done) {
    (void)c;
    psi(self, "batch", 1);
    if (updates) ((void (*)(void *))block_fn(updates))(updates);
    psi(self, "batch", 0);
    cv_reload(self, 0);
    if (done) ((void (*)(void *, BOOL))block_fn(done))(done, YES);
}
static i64 cv_n_sections(id self, SEL c) { (void)c; return n_sections(self); }
static i64 cv_n_items(id self, SEL c, i64 s) { (void)c; return n_items(self, s); }
static id cv_item_at(id self, SEL c, id ip) { (void)c; return MSG(id, visible(self), "objectForKey:", ipkey(ip)); }
static id cv_ip_for_item(id self, SEL c, id it) { (void)c; return it ? pget(it, "ip") : 0; }
static id cv_visible_items(id self, SEL c) {
    (void)c;
    id out = arr(), vals = MSG0(id, visible(self), "allValues");
    for (u64 i = 0, n = MSG0(u64, vals, "count"); i < n; i++) { id it = MSG(id, vals, "objectAtIndex:", i); if (MSG(BOOL, it, "isKindOfClass:", (id)kls("NSCollectionViewItem"))) MSG(void, out, "addObject:", it); }
    return out;
}
static id cv_visible_ips(id self, SEL c) {
    (void)c;
    id out = MSG0(id, (id)kls("NSMutableSet"), "set"), items = cv_visible_items(self, 0);
    for (u64 i = 0, n = MSG0(u64, items, "count"); i < n; i++) { id ipv = pget(MSG(id, items, "objectAtIndex:", i), "ip"); MSG(void, out, "addObject:", ipv); }
    return out;
}
static id cv_ip_at_point(id self, SEL c, P p) {
    (void)c;
    id all = cache_of(cv_layout_obj(self));
    for (u64 i = 0, n = MSG0(u64, all, "count"); i < n; i++) {
        id a = MSG(id, all, "objectAtIndex:", i);
        R f = la_frame(a, 0);
        if (pgi(a, "cat", 0) == 0 && p.x >= f.x && p.y >= f.y && p.x < f.x + f.w && p.y < f.y + f.h) return pget(a, "ip");
    }
    return 0;
}
static id cv_attrs_for(id self, SEL c, id ip) { (void)c; return MSG(id, cv_layout_obj(self), "layoutAttributesForItemAtIndexPath:", ip); }
static R cv_frame_for_index(id self, SEL c, u64 i) { (void)c; id a = cv_attrs_for(self, 0, ipath((i64)i, 0)); return a ? la_frame(a, 0) : mkr(0, 0, 0, 0); }
static BOOL cv_flipped(id self, SEL c) { (void)self; (void)c; return YES; }
static void cv_set_frame_size(id self, SEL c, S z) {
    IMP sup = class_getMethodImplementation(kls("NSView"), c);
    ((void (*)(id, SEL, S))sup)(self, c, z);
    id lay = cv_layout_obj(self);
    if (lay && !pgi(self, "tiling", 0) && ((BOOL (*)(id, SEL, R))objc_msgSend)(lay, sl("shouldInvalidateLayoutForBoundsChange:"), view_bounds(self))) cv_tile(self);
}
static void cv_layout_subtree(id self, SEL c) { (void)c; cv_tile(self); }
static void cv_scroll_to(id self, SEL c, id ips, u64 pos) {
    (void)c; (void)pos;
    id list = MSG0(id, ips, "allObjects");
    if (!MSG0(u64, list, "count")) return;
    id a = cv_attrs_for(self, 0, MSG(id, list, "objectAtIndex:", (u64)0));
    if (a) MSG(BOOL, self, "scrollRectToVisible:", la_frame(a, 0));
}
/* seleção */
#define CVB(get, key, def) static BOOL cvg_##get(id self, SEL c) { (void)c; return pgi(self, key, def) != 0; } static void cvs_##get(id self, SEL c, BOOL b) { (void)c; psi(self, key, b); }
CVB(selectable, "selectable", 0) CVB(multi, "multi", 0) CVB(empty, "allowsEmpty", 1)
static id cv_sel_ips(id self, SEL c) { (void)c; return MSG0(id, selset(self), "copy"); }
static void cv_set_sel_ips(id self, SEL c, id s) { (void)c; MSG0(void, selset(self), "removeAllObjects"); if (s) MSG(void, selset(self), "unionSet:", s); cv_tile(self); }
static id cv_sel_indexes(id self, SEL c) {
    (void)c;
    id out = MSG0(id, (id)kls("NSMutableIndexSet"), "indexSet"), all = MSG0(id, selset(self), "allObjects");
    for (u64 i = 0, n = MSG0(u64, all, "count"); i < n; i++) { id ip = MSG(id, all, "objectAtIndex:", i); if (ip_section(ip) == 0) MSG(void, out, "addIndex:", (u64)ip_item(ip)); }
    return out;
}
static void cv_set_sel_indexes(id self, SEL c, id ixs) {
    (void)c;
    MSG0(void, selset(self), "removeAllObjects");
    for (u64 i = ixs ? MSG0(u64, ixs, "firstIndex") : (u64)0x7fffffffffffffffULL; i != (u64)0x7fffffffffffffffULL; i = MSG(u64, ixs, "indexGreaterThanIndex:", i)) MSG(void, selset(self), "addObject:", ipath((i64)i, 0));
    cv_tile(self);
}
static void select_ips(id self, id ips, int notify, int deselect) {
    id d = cv_delegate(self, 0);
    id want = ips;
    const char *should = deselect ? "collectionView:shouldDeselectItemsAtIndexPaths:" : "collectionView:shouldSelectItemsAtIndexPaths:";
    if (notify && d && responds(d, should)) want = MSG(id, d, should, self, ips);
    if (!want || !MSG0(u64, want, "count")) return;
    if (deselect) MSG(void, selset(self), "minusSet:", want); else MSG(void, selset(self), "unionSet:", want);
    cv_tile(self);
    const char *did = deselect ? "collectionView:didDeselectItemsAtIndexPaths:" : "collectionView:didSelectItemsAtIndexPaths:";
    if (notify && d && responds(d, did)) MSG(void, d, did, self, want);
}
static void cv_select(id self, SEL c, id ips, u64 pos) { (void)c; (void)pos; select_ips(self, ips, 0, 0); }
static void cv_deselect(id self, SEL c, id ips) { (void)c; select_ips(self, ips, 0, 1); }
static void cv_select_all(id self, SEL c, id sender) {
    (void)c; (void)sender;
    if (!pgi(self, "multi", 0)) return;
    id all = MSG0(id, (id)kls("NSMutableSet"), "set");
    for (i64 s = 0, ns = n_sections(self); s < ns; s++) for (i64 i = 0, n = n_items(self, s); i < n; i++) MSG(void, all, "addObject:", ipath(i, s));
    select_ips(self, all, 1, 0);
}
static void cv_deselect_all(id self, SEL c, id sender) { (void)c; (void)sender; id cur = MSG0(id, selset(self), "copy"); select_ips(self, cur, 1, 1); objc_release(cur); }
static void click_select(id self, id ip, u64 mods) {
    if (!pgi(self, "selectable", 0)) return;
    BOOL ext = (mods & ((1u << 17) | (1u << 20))) && pgi(self, "multi", 0);
    BOOL was = ip && MSG(BOOL, selset(self), "containsObject:", ip);
    if (!ip) { if (pgi(self, "allowsEmpty", 1)) cv_deselect_all(self, 0, 0); return; }
    id one = MSG(id, (id)kls("NSSet"), "setWithObject:", ip);
    if (ext && was && (mods & (1u << 20))) { select_ips(self, one, 1, 1); return; }
    if (!ext) {
        id others = MSG0(id, selset(self), "mutableCopy");
        MSG(void, others, "removeObject:", ip);
        if (MSG0(u64, others, "count")) select_ips(self, others, 1, 1);
        objc_release(others);
    }
    if (!was) select_ips(self, one, 1, 0);
    pset(self, "anchor", ip);
}
static void cv_mouse(id self, SEL c, id ev) {
    (void)c;
    id w = MSG0(id, self, "window");
    if (w) MSG(BOOL, w, "makeFirstResponder:", self);
    P wp = ((P (*)(id, SEL))objc_msgSend)(ev, sl("locationInWindow"));
    P p = ((P (*)(id, SEL, P, id))objc_msgSend)(self, sl("convertPoint:fromView:"), wp, (id)0);
    click_select(self, cv_ip_at_point(self, 0, p), MSG0(u64, ev, "modifierFlags"));
}
static void cv_key(id self, SEL c, id ev) {
    (void)c;
    id ch = MSG0(id, ev, "charactersIgnoringModifiers");
    unsigned short u = ch && MSG0(u64, ch, "length") ? MSG(unsigned short, ch, "characterAtIndex:", (u64)0) : 0;
    u64 mods = MSG0(u64, ev, "modifierFlags");
    if ((mods & (1u << 20)) && (u == 'a' || u == 'A')) { cv_select_all(self, 0, 0); return; }
    id cur = pget(self, "anchor");
    i64 s = cur ? ip_section(cur) : 0, i = cur ? ip_item(cur) : -1;
    i64 n = n_items(self, s);
    if (!n) { NSBeep(); return; }
    id lay = cv_layout_obj(self);
    double rowlen = 1;
    if (lay && n > 1) {   /* itens por linha: quantos têm o mesmo y do primeiro */
        id a0 = cv_attrs_for(self, 0, ipath(0, s));
        double y0 = a0 ? la_frame(a0, 0).y : 0;
        rowlen = 0;
        for (i64 k = 0; k < n; k++) { id a = cv_attrs_for(self, 0, ipath(k, s)); if (!a || la_frame(a, 0).y != y0) break; rowlen++; }
        if (rowlen < 1) rowlen = 1;
    }
    i64 t = i;
    if (u == 0xF702) t = i - 1; else if (u == 0xF703) t = i + 1;
    else if (u == 0xF700) t = i - (i64)rowlen; else if (u == 0xF701) t = i + (i64)rowlen;
    else { id nr = MSG0(id, self, "nextResponder"); if (nr) MSG(void, nr, "keyDown:", ev); return; }
    if (t < 0) t = 0;
    if (t >= n) t = n - 1;
    id ip = ipath(t, s);
    click_select(self, ip, mods & (1u << 17));
    id one = MSG(id, (id)kls("NSSet"), "setWithObject:", ip);
    cv_scroll_to(self, 0, one, 0);
}
static BOOL cv_accepts(id self, SEL c) { (void)c; return pgi(self, "selectable", 0) != 0; }
static id cv_content(id self, SEL c) { (void)c; id a = pget(self, "content"); return a ? a : MSG0(id, (id)kls("NSArray"), "array"); }
static void cv_set_content(id self, SEL c, id a) { (void)c; pset(self, "content", a); cv_reload(self, 0); }
static id cv_proto(id self, SEL c) { (void)c; return pget(self, "proto"); }
static void cv_set_proto(id self, SEL c, id p) { (void)c; pset(self, "proto", p); }
static id cv_bg_colors(id self, SEL c) { (void)c; id a = pget(self, "bgColors"); return a ? a : MSG(id, (id)kls("NSArray"), "arrayWithObject:", cx_color(1, 1, 1, 1)); }
static void cv_set_bg_colors(id self, SEL c, id a) { (void)c; pset(self, "bgColors", a); MSG(void, self, "setNeedsDisplay:", (BOOL)YES); }
static id cv_bg_view(id self, SEL c) { (void)c; return pget(self, "bgView"); }
static void cv_set_bg_view(id self, SEL c, id v) { (void)c; pset(self, "bgView", v); }
static void cv_draw(id self, SEL c, R dirty) {
    (void)c; (void)dirty;
    id cols = cv_bg_colors(self, 0);
    MSG0(void, MSG(id, cols, "objectAtIndex:", (u64)0), "setFill");
    NSRectFill(msg_rect(self, "bounds"));
}

/* NSCollectionViewItem: estado de destaque e a view de coleção */
static i64 item_hl(id self, SEL c) { (void)c; return pgi(self, "hl", 0); }
static void item_set_hl(id self, SEL c, i64 h) { (void)c; psi(self, "hl", h); }
static id item_cv(id self, SEL c) { (void)c; return objc_getAssociatedObject(self, &k_weak); }
static void item_set_cv(id self, SEL c, id cv) { (void)c; objc_setAssociatedObject(self, &k_weak, cv, 0); }
static void item_prepare(id self, SEL c) { (void)c; psi(self, "hl", 0); if (responds(self, "setSelected:")) MSG(void, self, "setSelected:", (BOOL)NO); }
static id item_copy(id self, SEL c, void *z) {
    (void)c; (void)z;
    id n = MSG0(id, MSG0(id, (id)object_getClass(self), "alloc"), "init");
    id v = MSG0(id, self, "view");
    if (v) {   /* a view do protótipo é arquivada e desarquivada (cópia profunda) quando dá; senão, nova do mesmo tipo */
        id data = MSG(id, (id)kls("NSKeyedArchiver"), "archivedDataWithRootObject:", v);
        id cp = data ? MSG(id, (id)kls("NSKeyedUnarchiver"), "unarchiveObjectWithData:", data) : 0;
        if (!cp) cp = objc_autorelease(((id (*)(id, SEL, R))objc_msgSend)(MSG0(id, (id)object_getClass(v), "alloc"), sl("initWithFrame:"), msg_rect(v, "frame")));
        MSG(void, n, "setView:", cp);
    }
    return n;
}

/* ================================================================ registro */
#define ADD(k, s, f) cx_add_method(k, s, (void *)(f))
static cx_class_t *ensure(cx_class_t *k, cx_class_t *m, const char *name, const char *super) {
    cx_class_t *e = kls(name);
    if (e) return e;
    cx_class_t *s = kls(super);
    return cx_define_class(k, m, name, s ? s : kls("NSObject"), 0);
}
static cx_class_t *ensure_pool(const char *name, const char *super) {
    static cx_class_t pc[8], pm[8];
    static int used;
    cx_class_t *e = kls(name);
    if (e || used >= 8) return e;
    cx_class_t *s = kls(super);
    int k = used++;
    return cx_define_class(&pc[k], &pm[k], name, s ? s : kls("NSObject"), 0);
}
void cx_appkit_collection_init(void) {
    cx_class_t *la = ensure_pool("NSCollectionViewLayoutAttributes", "NSObject");
    cx_class_t *lay = ensure_pool("NSCollectionViewLayout", "NSObject");
    cx_class_t *flow = ensure_pool("NSCollectionViewFlowLayout", "NSCollectionViewLayout");
    cx_class_t *grid = ensure(&NSCollectionViewGridLayout_c, &NSCollectionViewGridLayout_m, "NSCollectionViewGridLayout", "NSCollectionViewLayout");
    cx_class_t *comp = ensure_pool("NSCollectionViewCompositionalLayout", "NSCollectionViewLayout");
    cx_class_t *cv = kls("NSCollectionView"), *item = kls("NSCollectionViewItem");
    if (!la || !lay || !flow || !comp || !cv) return;
    cx_add_cmethod(la, "layoutAttributesForItemWithIndexPath:", (void *)la_for_item);
    cx_add_cmethod(la, "layoutAttributesForSupplementaryViewOfKind:withIndexPath:", (void *)la_for_supp);
    ADD(la, "frame", la_frame); ADD(la, "setFrame:", la_set_frame); ADD(la, "size", la_size); ADD(la, "setSize:", la_set_size);
    ADD(la, "indexPath", la_ip); ADD(la, "setIndexPath:", la_set_ip); ADD(la, "alpha", la_alpha); ADD(la, "setAlpha:", la_set_alpha);
    ADD(la, "zIndex", la_z); ADD(la, "setZIndex:", la_set_z); ADD(la, "isHidden", la_hidden); ADD(la, "setHidden:", la_set_hidden);
    ADD(la, "representedElementCategory", la_cat); ADD(la, "representedElementKind", la_kind); ADD(la, "copyWithZone:", la_copy);
    cx_add_cmethod(lay, "layoutAttributesClass", (void *)lay_attrs_class);
    ADD(lay, "collectionView", lay_cv); ADD(lay, "setCollectionView:", lay_set_cv); ADD(lay, "prepareLayout", lay_prepare_noop);
    ADD(lay, "invalidateLayout", lay_invalidate); ADD(lay, "invalidateLayoutWithContext:", lay_invalidate_ctx);
    ADD(lay, "collectionViewContentSize", lay_content); ADD(lay, "layoutAttributesForElementsInRect:", lay_elements_in);
    ADD(lay, "layoutAttributesForItemAtIndexPath:", lay_attrs_item); ADD(lay, "layoutAttributesForSupplementaryViewOfKind:atIndexPath:", lay_attrs_supp);
    ADD(lay, "shouldInvalidateLayoutForBoundsChange:", lay_should_invalidate_bounds);
    ADD(flow, "prepareLayout", flow_prepare);
    ADD(flow, "itemSize", flow_get_size); ADD(flow, "setItemSize:", flow_set_size); ADD(flow, "estimatedItemSize", flow_est); ADD(flow, "setEstimatedItemSize:", flow_set_est);
    ADD(flow, "minimumInteritemSpacing", flg_inter); ADD(flow, "setMinimumInteritemSpacing:", fls_inter);
    ADD(flow, "minimumLineSpacing", flg_line); ADD(flow, "setMinimumLineSpacing:", fls_line);
    ADD(flow, "sectionInset", flow_get_inset); ADD(flow, "setSectionInset:", flow_set_inset);
    ADD(flow, "headerReferenceSize", flow_header); ADD(flow, "setHeaderReferenceSize:", flow_set_header);
    ADD(flow, "footerReferenceSize", flow_footer); ADD(flow, "setFooterReferenceSize:", flow_set_footer);
    ADD(flow, "scrollDirection", flow_dir); ADD(flow, "setScrollDirection:", flow_set_dir);
    ADD(flow, "sectionHeadersPinToVisibleBounds", flow_pins); ADD(flow, "setSectionHeadersPinToVisibleBounds:", flow_set_pins);
    ADD(grid, "prepareLayout", grid_prepare); ADD(grid, "maximumNumberOfColumns", grid_maxc); ADD(grid, "setMaximumNumberOfColumns:", grid_set_maxc);
    ADD(grid, "maximumNumberOfRows", grid_maxr); ADD(grid, "setMaximumNumberOfRows:", grid_set_maxr);
    ADD(grid, "minimumItemSize", grid_min); ADD(grid, "setMinimumItemSize:", grid_set_min); ADD(grid, "maximumItemSize", grid_max); ADD(grid, "setMaximumItemSize:", grid_set_max);
    ADD(grid, "margins", grid_margins); ADD(grid, "setMargins:", grid_set_margins);
    ADD(grid, "minimumInteritemSpacing", grg_marX); ADD(grid, "setMinimumInteritemSpacing:", grs_marX);
    ADD(grid, "minimumLineSpacing", grg_marY); ADD(grid, "setMinimumLineSpacing:", grs_marY);
    (void)grid_margins_get; (void)grid_spacing; (void)grid_set_spacing;
    ADD(comp, "initWithSection:", cl_init_section); ADD(comp, "initWithSectionProvider:", cl_init_provider);
    ADD(comp, "initWithSection:configuration:", cl_init_section_cfg); ADD(comp, "initWithSectionProvider:configuration:", cl_init_provider_cfg);
    ADD(comp, "prepareLayout", cl_prepare);
    cx_class_t *dim = ensure_pool("NSCollectionLayoutDimension", "NSObject");
    if (dim) {
        cx_add_cmethod(dim, "absoluteDimension:", (void *)dim_abs); cx_add_cmethod(dim, "fractionalWidthDimension:", (void *)dim_fw);
        cx_add_cmethod(dim, "fractionalHeightDimension:", (void *)dim_fh); cx_add_cmethod(dim, "estimatedDimension:", (void *)dim_est);
        ADD(dim, "dimension", dim_value); ADD(dim, "isAbsolute", dim_is_abs); ADD(dim, "isFractionalWidth", dim_is_fw);
        ADD(dim, "isFractionalHeight", dim_is_fh); ADD(dim, "isEstimated", dim_is_est);
    }
    cx_class_t *sz = ensure_pool("NSCollectionLayoutSize", "NSObject");
    if (sz) { cx_add_cmethod(sz, "sizeWithWidthDimension:heightDimension:", (void *)size_make); ADD(sz, "widthDimension", size_w); ADD(sz, "heightDimension", size_h); }
    cx_class_t *li = ensure_pool("NSCollectionLayoutItem", "NSObject");
    if (li) {
        cx_add_cmethod(li, "itemWithLayoutSize:", (void *)item_make); cx_add_cmethod(li, "itemWithLayoutSize:supplementaryItems:", (void *)item_make_supp);
        ADD(li, "layoutSize", item_size); ADD(li, "contentInsets", item_insets); ADD(li, "setContentInsets:", item_set_insets);
    }
    cx_class_t *gr = ensure_pool("NSCollectionLayoutGroup", "NSCollectionLayoutItem");
    if (gr) {
        cx_add_cmethod(gr, "horizontalGroupWithLayoutSize:subitems:", (void *)group_h); cx_add_cmethod(gr, "verticalGroupWithLayoutSize:subitems:", (void *)group_v);
        cx_add_cmethod(gr, "horizontalGroupWithLayoutSize:subitem:count:", (void *)group_h_count); cx_add_cmethod(gr, "verticalGroupWithLayoutSize:subitem:count:", (void *)group_v_count);
        cx_add_cmethod(gr, "horizontalGroupWithLayoutSize:repeatingSubitem:count:", (void *)group_h_repeat);
        cx_add_cmethod(gr, "verticalGroupWithLayoutSize:repeatingSubitem:count:", (void *)group_v_count);
        ADD(gr, "subitems", group_items); ADD(gr, "interItemSpacing", group_spacing); ADD(gr, "setInterItemSpacing:", group_set_spacing);
    }
    cx_class_t *spc = ensure_pool("NSCollectionLayoutSpacing", "NSObject");
    if (spc) { cx_add_cmethod(spc, "fixedSpacing:", (void *)spacing_fixed); cx_add_cmethod(spc, "flexibleSpacing:", (void *)spacing_flexible); ADD(spc, "spacing", spacing_value); }
    cx_class_t *sec = ensure_pool("NSCollectionLayoutSection", "NSObject");
    if (sec) {
        cx_add_cmethod(sec, "sectionWithGroup:", (void *)section_make);
        ADD(sec, "interGroupSpacing", section_spacing); ADD(sec, "setInterGroupSpacing:", section_set_spacing);
        ADD(sec, "contentInsets", section_insets); ADD(sec, "setContentInsets:", section_set_insets);
        ADD(sec, "boundarySupplementaryItems", section_supps); ADD(sec, "setBoundarySupplementaryItems:", section_set_supps);
        ADD(sec, "orthogonalScrollingBehavior", section_scrolling); ADD(sec, "setOrthogonalScrollingBehavior:", section_set_scrolling);
    }
    cx_class_t *bi = ensure_pool("NSCollectionLayoutBoundarySupplementaryItem", "NSCollectionLayoutItem");
    if (bi) { cx_add_cmethod(bi, "boundarySupplementaryItemWithLayoutSize:elementKind:alignment:", (void *)boundary_make); ADD(bi, "elementKind", boundary_kind); }
    ADD(cv, "collectionViewLayout", cv_get_layout); ADD(cv, "setCollectionViewLayout:", cv_set_layout);
    ADD(cv, "dataSource", cv_ds); ADD(cv, "setDataSource:", cv_set_ds); ADD(cv, "delegate", cv_delegate); ADD(cv, "setDelegate:", cv_set_delegate);
    ADD(cv, "registerClass:forItemWithIdentifier:", cv_register_class); ADD(cv, "registerNib:forItemWithIdentifier:", cv_register_nib);
    ADD(cv, "registerClass:forSupplementaryViewOfKind:withIdentifier:", cv_register_supp);
    ADD(cv, "registerNib:forSupplementaryViewOfKind:withIdentifier:", cv_register_supp_nib);
    ADD(cv, "makeItemWithIdentifier:forIndexPath:", cv_make_item); ADD(cv, "makeSupplementaryViewOfKind:withIdentifier:forIndexPath:", cv_make_supp);
    ADD(cv, "reloadData", cv_reload); ADD(cv, "reloadItemsAtIndexPaths:", cv_reload_items); ADD(cv, "reloadSections:", cv_changed);
    ADD(cv, "insertItemsAtIndexPaths:", cv_changed); ADD(cv, "deleteItemsAtIndexPaths:", cv_changed); ADD(cv, "moveItemAtIndexPath:toIndexPath:", cv_move_item);
    ADD(cv, "insertSections:", cv_changed); ADD(cv, "deleteSections:", cv_changed); ADD(cv, "performBatchUpdates:completionHandler:", cv_batch);
    ADD(cv, "numberOfSections", cv_n_sections); ADD(cv, "numberOfItemsInSection:", cv_n_items);
    ADD(cv, "itemAtIndexPath:", cv_item_at); ADD(cv, "itemAtIndex:", cv_item_at); ADD(cv, "indexPathForItem:", cv_ip_for_item);
    ADD(cv, "visibleItems", cv_visible_items); ADD(cv, "indexPathsForVisibleItems", cv_visible_ips); ADD(cv, "indexPathForItemAtPoint:", cv_ip_at_point);
    ADD(cv, "layoutAttributesForItemAtIndexPath:", cv_attrs_for); ADD(cv, "frameForItemAtIndex:", cv_frame_for_index);
    ADD(cv, "isFlipped", cv_flipped); ADD(cv, "setFrameSize:", cv_set_frame_size); ADD(cv, "layoutSubtreeIfNeeded", cv_layout_subtree);
    ADD(cv, "scrollToItemsAtIndexPaths:scrollPosition:", cv_scroll_to);
    ADD(cv, "isSelectable", cvg_selectable); ADD(cv, "setSelectable:", cvs_selectable);
    ADD(cv, "allowsMultipleSelection", cvg_multi); ADD(cv, "setAllowsMultipleSelection:", cvs_multi);
    ADD(cv, "allowsEmptySelection", cvg_empty); ADD(cv, "setAllowsEmptySelection:", cvs_empty);
    ADD(cv, "selectionIndexPaths", cv_sel_ips); ADD(cv, "setSelectionIndexPaths:", cv_set_sel_ips);
    ADD(cv, "selectionIndexes", cv_sel_indexes); ADD(cv, "setSelectionIndexes:", cv_set_sel_indexes);
    ADD(cv, "selectItemsAtIndexPaths:scrollPosition:", cv_select); ADD(cv, "deselectItemsAtIndexPaths:", cv_deselect);
    ADD(cv, "selectAll:", cv_select_all); ADD(cv, "deselectAll:", cv_deselect_all);
    ADD(cv, "mouseDown:", cv_mouse); ADD(cv, "keyDown:", cv_key); ADD(cv, "acceptsFirstResponder", cv_accepts);
    ADD(cv, "content", cv_content); ADD(cv, "setContent:", cv_set_content); ADD(cv, "itemPrototype", cv_proto); ADD(cv, "setItemPrototype:", cv_set_proto);
    ADD(cv, "backgroundColors", cv_bg_colors); ADD(cv, "setBackgroundColors:", cv_set_bg_colors);
    ADD(cv, "backgroundView", cv_bg_view); ADD(cv, "setBackgroundView:", cv_set_bg_view); ADD(cv, "drawRect:", cv_draw);
    if (item) {
        ADD(item, "highlightState", item_hl); ADD(item, "setHighlightState:", item_set_hl);
        ADD(item, "collectionView", item_cv); ADD(item, "setCollectionView:", item_set_cv);
        ADD(item, "prepareForReuse", item_prepare); ADD(item, "copyWithZone:", item_copy);
    }
    (void)NSCollectionLayoutEdgeSpacing_c; (void)NSCollectionLayoutAnchor_c; (void)NSCollectionLayoutSupplementaryItem_c;
}
