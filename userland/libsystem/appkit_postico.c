/*
 * mOS libSystem — classes do AppKit que o Postico 2 usa e o libSystem ainda não tinha (docs/swift-objc-runtime.md, fase 7):
 * as que o binário referencia (OBJC_CLASS_$_...) e as que os nibs dele instanciam pelo nome. Sem a classe, uma subclasse
 * Swift do app (FavoritesOutlineView: NSOutlineView, PCOToolbarItem: NSToolbarItem...) ficava pendurada no NSObject.
 *  - NSToolbarItem: identificador, rótulos, imagem, view, alvo/ação (a barra do mOS ainda não os desenha).
 *  - NSSplitView: dispõe as subviews lado a lado (ou empilhadas) com divisórias de 1 pt; as primeiras guardam a
 *    largura, a última fica com o resto (o comportamento padrão de quem não fixou prioridades).
 *  - NSOutlineView: NSTableView com um adaptador próprio como fonte de dados e delegate da tabela: achata a árvore
 *    (itens expandidos) em linhas e traduz numberOfRows/objectValue/viewFor... e a seleção para a API de outline.
 *  - NSTabView/NSTabViewItem/NSTabViewController: mostram a view do item selecionado.
 *  - Células (NSTextFieldCell, NSSearchFieldCell, NSTokenFieldCell, NSSegmentedCell, NSImageCell, NSTableHeaderCell...),
 *    NSSearchField/NSTokenField (campos de texto), NSTableRowView/NSTableHeaderView, NSGridView, NSDocument, NSGradient,
 *    NSColorPanel, NSRulerView, NSTextTable, NSFilePromise*: a hierarquia e o essencial.
 *  - Constantes (notificações, nomes de imagem, identificadores de item da barra), NSSetFocusRingStyle e as funções do
 *    overlay Swift do AppKit que o Postico importa (CGRect.fill(using:), CGRect.frame(withWidth:using:), NSSound.beep()).
 * Registro: cx_appkit_postico_init(), chamado no fim do cx_appkit_input_init (appkit_input.c).
 */
#include "foundation.h"
#include "sys.h"
#include "libc.h"
id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);

typedef struct { double x, y, w, h; } pr_rect_t;
typedef struct { double w, h; } pr_size_t;

#define K(N) cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N); cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N);
K(NSToolbarItem) K(NSSplitView) K(NSOutlineView) K(NSTableRowView) K(NSTableHeaderView) K(NSTableHeaderCell)
K(NSTextFieldCell) K(NSSecureTextFieldCell) K(NSSearchFieldCell) K(NSTokenFieldCell) K(NSSegmentedCell) K(NSImageCell)
K(NSSearchField) K(NSTokenField) K(NSTabView) K(NSTabViewItem) K(NSTabViewController) K(NSPageController)
K(NSGridView) K(NSGridCell) K(NSGridRow) K(NSGridColumn) K(NSDocument) K(NSGradient) K(NSColorPanel) K(NSRulerView)
K(NSRulerMarker) K(NSTextTable) K(NSTextTableBlock) K(NSTextAttachmentCell) K(NSDraggingImageComponent)
K(NSFilePromiseProvider) K(NSFilePromiseReceiver) K(_MOSOutlineAdapter)

void objc_setAssociatedObject(id obj, const void *key, id value, u64 policy);
id objc_getAssociatedObject(id obj, const void *key);
IMP class_getMethodImplementation(cx_class_t *c, SEL s);
id cx_arr_new(cx_class_t *cls);

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
/* chama o método da superclasse (classe dada) com o mesmo seletor */
#define SUPER(cls, T, self, sel, ...) ((T (*)(id, SEL, ...))class_getMethodImplementation(cl(cls), sel_registerName(sel)))(self, sel_registerName(sel), ##__VA_ARGS__)

/* ---- propriedades genéricas num dicionário associado ao objeto */
static char props_key;
static id props(id o) {
    id d = objc_getAssociatedObject(o, &props_key);
    if (!d) { d = MSG0(id, MSG0(id, (id)cl("NSMutableDictionary"), "alloc"), "init"); objc_setAssociatedObject(o, &props_key, d, 1); objc_release(d); }
    return d;
}
static id pget(id o, const char *k) { return MSG(id, props(o), "objectForKey:", cx_str(k)); }
static void pset(id o, const char *k, id v) { if (v) MSG(void, props(o), "setObject:forKey:", v, cx_str(k)); else MSG(void, props(o), "removeObjectForKey:", cx_str(k)); }
static i64 pget_i(id o, const char *k, i64 d) { id v = pget(o, k); return v ? MSG0(i64, v, "longLongValue") : d; }
static double pget_d(id o, const char *k, double d) { id v = pget(o, k); return v ? MSG0(double, v, "doubleValue") : d; }
#define OBJP(name, setter) \
    static id g_##name(id s, SEL c) { (void)c; return pget(s, #name); } \
    static void s_##name(id s, SEL c, id v) { (void)c; pset(s, #name, v); }
#define INTP(name, setter, def) \
    static i64 g_##name(id s, SEL c) { (void)c; return pget_i(s, #name, def); } \
    static void s_##name(id s, SEL c, i64 v) { (void)c; pset(s, #name, cx_num_i(v)); }
#define DBLP(name, setter, def) \
    static double g_##name(id s, SEL c) { (void)c; return pget_d(s, #name, def); } \
    static void s_##name(id s, SEL c, double v) { (void)c; pset(s, #name, cx_num_d(v)); }
#define REG(k, name, setter) cx_add_method(&k##_c, #name, (void *)g_##name); cx_add_method(&k##_c, #setter ":", (void *)s_##name);

OBJP(itemIdentifier, setItemIdentifier) OBJP(label, setLabel) OBJP(paletteLabel, setPaletteLabel) OBJP(toolTip, setToolTip)
OBJP(image, setImage) OBJP(view, setView) OBJP(target, setTarget) OBJP(menuFormRepresentation, setMenuFormRepresentation)
OBJP(title, setTitle) OBJP(possibleLabels, setPossibleLabels) OBJP(identifier, setIdentifier) OBJP(viewController, setViewController)
OBJP(tabView, setTabView) OBJP(delegate, setDelegate) OBJP(autosaveName, setAutosaveName) OBJP(placeholderString, setPlaceholderString)
OBJP(recentSearches, setRecentSearches) OBJP(recentsAutosaveName, setRecentsAutosaveName) OBJP(objectValue, setObjectValue)
OBJP(fileURL, setFileURL) OBJP(fileType, setFileType) OBJP(displayName, setDisplayName) OBJP(color, setColor)
OBJP(tokenizingCharacterSet, setTokenizingCharacterSet) OBJP(arrangedSubviewsStore, setArrangedSubviewsStore)
INTP(enabled, setEnabled, 1) INTP(tag, setTag, 0) INTP(visibilityPriority, setVisibilityPriority, 0) INTP(bordered, setBordered, 0)
INTP(navigational, setNavigational, 0) INTP(autovalidates, setAutovalidates, 1) INTP(dividerStyle, setDividerStyle, 2)
INTP(arrangesAllSubviews, setArrangesAllSubviews, 1) INTP(tabViewType, setTabViewType, 0) INTP(tabStyle, setTabStyle, 0)
INTP(sendsSearchStringImmediately, setSendsSearchStringImmediately, 0) INTP(sendsWholeSearchString, setSendsWholeSearchString, 0)
INTP(maximumRecents, setMaximumRecents, 10) INTP(state, setState, 0) INTP(segmentCount, setSegmentCount, 0)
INTP(selectedSegment, setSelectedSegment, -1) INTP(floatsGroupRows, setFloatsGroupRows, 0) INTP(autoresizesOutlineColumn, setAutoresizesOutlineColumn, 1)
INTP(selectionHighlightStyle, setSelectionHighlightStyle, 1) INTP(emphasized, setEmphasized, 0) INTP(selected, setSelected, 0)
INTP(groupRowStyle, setGroupRowStyle, 0) INTP(rowSizeStyle, setRowSizeStyle, -1) INTP(indentationMarkerFollowsCell, setIndentationMarkerFollowsCell, 1)
DBLP(indentationPerLevel, setIndentationPerLevel, 16) DBLP(rowSpacing, setRowSpacing, 6) DBLP(columnSpacing, setColumnSpacing, 6)
DBLP(alphaValueP, setAlphaValueP, 1)

static id ret_self(id s, SEL c) { (void)c; return s; }
static id ret_nil(id s, SEL c) { (void)s; (void)c; return 0; }
static BOOL ret_yes(id s, SEL c) { (void)s; (void)c; return 1; }
static BOOL ret_no(id s, SEL c) { (void)s; (void)c; return 0; }
static void noop(id s, SEL c) { (void)s; (void)c; }
static void noop1(id s, SEL c, id a) { (void)s; (void)c; (void)a; }

/* ================================================================ NSToolbarItem */
static id tbi_init(id self, SEL c, id ident) {
    (void)c;
    self = MSG0(id, self, "init");
    if (self) pset(self, "itemIdentifier", ident);
    return self;
}
static SEL tbi_action(id self, SEL c) { (void)c; id v = pget(self, "action"); return v ? (SEL)MSG0(void *, v, "pointerValue") : 0; }
static void tbi_set_action(id self, SEL c, SEL a) { (void)c; pset(self, "action", MSG(id, (id)cl("NSValue"), "valueWithPointer:", (void *)a)); }
static pr_size_t tbi_min(id s, SEL c) { (void)s; (void)c; pr_size_t z = {32, 28}; return z; }
static void tbi_set_size(id s, SEL c, pr_size_t z) { (void)s; (void)c; (void)z; }
static id tbi_toolbar(id s, SEL c) { (void)s; (void)c; return 0; }

/* ================================================================ NSSplitView */
static id sv_subviews(id self) { return MSG0(id, self, "subviews"); }
static void sv_adjust(id self, SEL c) {
    (void)c;
    id subs = sv_subviews(self);
    u64 n = subs ? MSG0(u64, subs, "count") : 0;
    if (!n) return;
    pr_rect_t b = MSG0(pr_rect_t, self, "bounds");
    BOOL vert = pget_i(self, "vertical", 1) != 0;
    double div = 1, total = vert ? b.w : b.h, used = 0;
    double *sz = calloc(n, sizeof *sz);
    for (u64 i = 0; i < n; i++) {
        id v = MSG(id, subs, "objectAtIndex:", i);
        pr_rect_t f = MSG0(pr_rect_t, v, "frame");
        sz[i] = vert ? f.w : f.h;
        if (i + 1 < n) used += sz[i] + div;
    }
    if (used > total - 40 && n > 1) {                 /* não cabe: divide igualmente */
        for (u64 i = 0; i < n; i++) sz[i] = (total - div * (double)(n - 1)) / (double)n;
        used = total - sz[n - 1];
    }
    sz[n - 1] = total - used;
    if (sz[n - 1] < 0) sz[n - 1] = 0;
    double pos = 0;
    for (u64 i = 0; i < n; i++) {
        id v = MSG(id, subs, "objectAtIndex:", i);
        pr_rect_t f = vert ? (pr_rect_t){pos, 0, sz[i], b.h} : (pr_rect_t){0, pos, b.w, sz[i]};
        MSG(void, v, "setFrame:", f);
        pos += sz[i] + div;
    }
    free(sz);
}
static void sv_set_frame_size(id self, SEL c, pr_size_t z) { SUPER("NSView", void, self, "setFrameSize:", z); (void)c; sv_adjust(self, 0); }
static void sv_set_frame(id self, SEL c, pr_rect_t f) { SUPER("NSView", void, self, "setFrame:", f); (void)c; sv_adjust(self, 0); }
static void sv_layout(id self, SEL c) { (void)c; sv_adjust(self, 0); }
static BOOL sv_vertical(id self, SEL c) { (void)c; return pget_i(self, "vertical", 1) != 0; }
static void sv_set_vertical(id self, SEL c, BOOL v) { (void)c; pset(self, "vertical", cx_num_i(v)); sv_adjust(self, 0); }
static double sv_div(id s, SEL c) { (void)s; (void)c; return 1; }
static id sv_arranged(id self, SEL c) { (void)c; return sv_subviews(self); }
static void sv_add_arranged(id self, SEL c, id v) { (void)c; MSG(void, self, "addSubview:", v); sv_adjust(self, 0); }
static void sv_insert_arranged(id self, SEL c, id v, i64 i) { (void)c; (void)i; MSG(void, self, "addSubview:", v); sv_adjust(self, 0); }
static void sv_remove_arranged(id self, SEL c, id v) { (void)c; (void)self; MSG0(void, v, "removeFromSuperview"); }
static void sv_set_pos(id self, SEL c, double p, i64 idx) {
    (void)c;
    id subs = sv_subviews(self);
    if (!subs || idx < 0 || (u64)idx + 1 >= MSG0(u64, subs, "count")) return;
    BOOL vert = pget_i(self, "vertical", 1) != 0;
    double start = 0;
    for (i64 i = 0; i < idx; i++) { pr_rect_t f = MSG0(pr_rect_t, MSG(id, subs, "objectAtIndex:", (u64)i), "frame"); start += (vert ? f.w : f.h) + 1; }
    id v = MSG(id, subs, "objectAtIndex:", (u64)idx);
    pr_rect_t f = MSG0(pr_rect_t, v, "frame");
    if (vert) f.w = p - start; else f.h = p - start;
    if (f.w < 0) f.w = 0;
    if (f.h < 0) f.h = 0;
    MSG(void, v, "setFrame:", f);
    sv_adjust(self, 0);
}
static double sv_min_pos(id s, SEL c, i64 i) { (void)s; (void)c; (void)i; return 0; }
static double sv_max_pos(id self, SEL c, i64 i) { (void)c; (void)i; pr_rect_t b = MSG0(pr_rect_t, self, "bounds"); return pget_i(self, "vertical", 1) ? b.w : b.h; }
static BOOL sv_collapsed(id s, SEL c, id v) { (void)s; (void)c; return MSG0(BOOL, v, "isHidden"); }
static float sv_holding(id s, SEL c, i64 i) { (void)s; (void)c; (void)i; return 250; }
static void sv_set_holding(id s, SEL c, float p, i64 i) { (void)s; (void)c; (void)p; (void)i; }

/* ================================================================ NSOutlineView
 * A tabela do mOS pergunta numberOfRowsInTableView:/objectValue.../viewForTableColumn: à sua fonte de dados e ao
 * delegate: aqui os dois são um _MOSOutlineAdapter, que traduz para a API de outline com a árvore achatada. */
static char ol_key;
typedef struct { id item; int level; } ol_row_t;
typedef struct { id outline, ds, dl; ol_row_t *rows; i64 n, cap; id expanded; } ol_t;
static ol_t *ol_of(id ov) {
    id box = objc_getAssociatedObject(ov, &ol_key);
    return box ? (ol_t *)MSG0(void *, box, "pointerValue") : 0;
}
static ol_t *ol_make(id ov) {
    ol_t *o = ol_of(ov);
    if (o) return o;
    o = calloc(1, sizeof *o);
    o->outline = ov;
    o->expanded = objc_retain(MSG0(id, (id)cl("NSMutableArray"), "array"));
    objc_setAssociatedObject(ov, &ol_key, MSG(id, (id)cl("NSValue"), "valueWithPointer:", (void *)o), 1);
    return o;
}
static BOOL ol_resp(id o, const char *s) { return o && cx_responds(o->isa, sel_registerName(s)); }
static BOOL ol_is_expanded(ol_t *o, id item) { return item && MSG(BOOL, o->expanded, "containsObject:", item); }
static void ol_push(ol_t *o, id item, int level) {
    if (o->n == o->cap) { o->cap = o->cap ? o->cap * 2 : 64; o->rows = realloc(o->rows, (size_t)o->cap * sizeof *o->rows); }
    o->rows[o->n].item = item; o->rows[o->n].level = level; o->n++;
}
static void ol_walk(ol_t *o, id parent, int level) {
    if (!ol_resp(o->ds, "outlineView:numberOfChildrenOfItem:") || !ol_resp(o->ds, "outlineView:child:ofItem:") || level > 32) return;
    i64 n = MSG(i64, o->ds, "outlineView:numberOfChildrenOfItem:", o->outline, parent);
    for (i64 i = 0; i < n; i++) {
        id child = MSG(id, o->ds, "outlineView:child:ofItem:", o->outline, i, parent);
        ol_push(o, child, level);
        if (ol_is_expanded(o, child)) ol_walk(o, child, level + 1);
    }
}
static void ol_rebuild(ol_t *o) { o->n = 0; ol_walk(o, 0, 0); }

static i64 oa_rows(id self, SEL c, id t) { (void)self; (void)c; ol_t *o = ol_of(t); if (!o) return 0; ol_rebuild(o); return o->n; }
static id oa_value(id self, SEL c, id t, id col, i64 row) {
    (void)self; (void)c;
    ol_t *o = ol_of(t);
    if (!o || row < 0 || row >= o->n || !ol_resp(o->ds, "outlineView:objectValueForTableColumn:byItem:")) return 0;
    return MSG(id, o->ds, "outlineView:objectValueForTableColumn:byItem:", t, col, o->rows[row].item);
}
static id oa_view(id self, SEL c, id t, id col, i64 row) {
    (void)self; (void)c;
    ol_t *o = ol_of(t);
    if (!o || row < 0 || row >= o->n || !ol_resp(o->dl, "outlineView:viewForTableColumn:item:")) return 0;
    return MSG(id, o->dl, "outlineView:viewForTableColumn:item:", t, col, o->rows[row].item);
}
static void oa_sel_changed(id self, SEL c, id note) {
    (void)self; (void)c;
    id t = MSG0(id, note, "object");
    ol_t *o = t ? ol_of(t) : 0;
    if (o && ol_resp(o->dl, "outlineViewSelectionDidChange:"))
        MSG(void, o->dl, "outlineViewSelectionDidChange:", objc_autorelease(cx_note(cx_str("NSOutlineViewSelectionDidChangeNotification"), t, 0)));
}
static id adapter(void) { static id a; if (!a) a = cx_alloc_instance(&_MOSOutlineAdapter_c); return a; }
static void ov_set_ds(id self, SEL c, id d) { (void)c; ol_make(self)->ds = d; SUPER("NSTableView", void, self, "setDataSource:", adapter()); }
static id ov_ds(id self, SEL c) { (void)c; ol_t *o = ol_of(self); return o ? o->ds : 0; }
static void ov_set_dl(id self, SEL c, id d) { (void)c; ol_make(self)->dl = d; SUPER("NSTableView", void, self, "setDelegate:", adapter()); }
static id ov_dl(id self, SEL c) { (void)c; ol_t *o = ol_of(self); return o ? o->dl : 0; }
static void ov_reload(id self, SEL c) { (void)c; ol_t *o = ol_make(self); ol_rebuild(o); SUPER("NSTableView", void, self, "reloadData"); }
static void ov_reload_item(id self, SEL c, id item) { (void)c; (void)item; ov_reload(self, 0); }
static void ov_reload_item2(id self, SEL c, id item, BOOL ch) { (void)c; (void)item; (void)ch; ov_reload(self, 0); }
static void ov_expand(id self, SEL c, id item, BOOL children) {
    (void)c; (void)children;
    ol_t *o = ol_make(self);
    if (!item) {                               /* nil: todos os itens de primeiro nível */
        ol_rebuild(o);
        for (i64 i = 0; i < o->n; i++) if (!ol_is_expanded(o, o->rows[i].item)) MSG(void, o->expanded, "addObject:", o->rows[i].item);
    } else if (!ol_is_expanded(o, item)) MSG(void, o->expanded, "addObject:", item);
    ov_reload(self, 0);
}
static void ov_expand1(id self, SEL c, id item) { ov_expand(self, c, item, 0); }
static void ov_collapse(id self, SEL c, id item, BOOL children) {
    (void)c; (void)children;
    ol_t *o = ol_make(self);
    if (item) MSG(void, o->expanded, "removeObject:", item); else MSG0(void, o->expanded, "removeAllObjects");
    ov_reload(self, 0);
}
static void ov_collapse1(id self, SEL c, id item) { ov_collapse(self, c, item, 0); }
static BOOL ov_is_exp(id self, SEL c, id item) { (void)c; ol_t *o = ol_of(self); return o && ol_is_expanded(o, item); }
static BOOL ov_is_expandable(id self, SEL c, id item) {
    (void)c;
    ol_t *o = ol_of(self);
    return o && ol_resp(o->ds, "outlineView:isItemExpandable:") && MSG(BOOL, o->ds, "outlineView:isItemExpandable:", self, item);
}
static id ov_item_at(id self, SEL c, i64 row) { (void)c; ol_t *o = ol_of(self); return o && row >= 0 && row < o->n ? o->rows[row].item : 0; }
static i64 ov_row_for(id self, SEL c, id item) {
    (void)c;
    ol_t *o = ol_of(self);
    if (!o) return -1;
    for (i64 i = 0; i < o->n; i++) if (o->rows[i].item == item || (item && MSG(BOOL, o->rows[i].item, "isEqual:", item))) return i;
    return -1;
}
static i64 ov_level(id self, SEL c, id item) { (void)c; ol_t *o = ol_of(self); i64 r = ov_row_for(self, 0, item); return o && r >= 0 ? o->rows[r].level : -1; }
static i64 ov_level_row(id self, SEL c, i64 row) { (void)c; ol_t *o = ol_of(self); return o && row >= 0 && row < o->n ? o->rows[row].level : -1; }
static id ov_parent(id self, SEL c, id item) {
    (void)c;
    ol_t *o = ol_of(self);
    i64 r = ov_row_for(self, 0, item);
    if (!o || r < 0) return 0;
    for (i64 i = r - 1; i >= 0; i--) if (o->rows[i].level < o->rows[r].level) return o->rows[i].item;
    return 0;
}
static i64 ov_children(id self, SEL c, id item) {
    (void)c;
    ol_t *o = ol_of(self);
    return o && ol_resp(o->ds, "outlineView:numberOfChildrenOfItem:") ? MSG(i64, o->ds, "outlineView:numberOfChildrenOfItem:", self, item) : 0;
}
static id ov_child(id self, SEL c, i64 i, id item) {
    (void)c;
    ol_t *o = ol_of(self);
    return o && ol_resp(o->ds, "outlineView:child:ofItem:") ? MSG(id, o->ds, "outlineView:child:ofItem:", self, i, item) : 0;
}
static id ov_outline_col(id self, SEL c) {
    (void)c;
    id v = pget(self, "outlineTableColumn");
    if (v) return v;
    id cols = MSG0(id, self, "tableColumns");
    return cols && MSG0(u64, cols, "count") ? MSG(id, cols, "objectAtIndex:", (u64)0) : 0;
}
static void ov_set_outline_col(id self, SEL c, id col) { (void)c; pset(self, "outlineTableColumn", col); }
static void ov_insert_items(id self, SEL c, id ixs, id parent, u64 opts) { (void)c; (void)ixs; (void)parent; (void)opts; ov_reload(self, 0); }
static void ov_move_item(id self, SEL c, i64 a, id p, i64 b, id q) { (void)c; (void)a; (void)p; (void)b; (void)q; ov_reload(self, 0); }

/* ================================================================ NSTabView / NSTabViewItem / NSTabViewController */
static id tvi_init(id self, SEL c, id ident) { (void)c; self = MSG0(id, self, "init"); if (self) pset(self, "identifier", ident); return self; }
static id tvi_with_vc(id cls, SEL c, id vc) {
    (void)c;
    id it = MSG0(id, MSG0(id, cls, "alloc"), "init");
    pset(it, "viewController", vc);
    if (vc) { pset(it, "label", MSG0(id, vc, "title")); pset(it, "view", MSG0(id, vc, "view")); }
    return objc_autorelease(it);
}
static id tv_items(id self) { id a = pget(self, "tabViewItems"); if (!a) { a = MSG0(id, (id)cl("NSMutableArray"), "array"); pset(self, "tabViewItems", a); } return a; }
static id tab_items(id self, SEL c) { (void)c; return tv_items(self); }
static i64 tab_count(id self, SEL c) { (void)c; return (i64)MSG0(u64, tv_items(self), "count"); }
static void tab_show(id self, id item) {
    id subs = MSG0(id, self, "subviews");
    u64 n = subs ? MSG0(u64, subs, "count") : 0;
    for (u64 i = n; i > 0; i--) MSG0(void, MSG(id, subs, "objectAtIndex:", i - 1), "removeFromSuperview");
    id v = item ? pget(item, "view") : 0;
    if (!v && item && pget(item, "viewController")) v = MSG0(id, pget(item, "viewController"), "view");
    if (v) { MSG(void, v, "setFrame:", MSG0(pr_rect_t, self, "bounds")); MSG(void, self, "addSubview:", v); }
    pset(self, "selectedTabViewItem", item);
    id d = pget(self, "delegate");
    if (d && cx_responds(d->isa, sel_registerName("tabView:didSelectTabViewItem:"))) MSG(void, d, "tabView:didSelectTabViewItem:", self, item);
}
static void tab_add(id self, SEL c, id item) {
    (void)c;
    MSG(void, tv_items(self), "addObject:", item);
    pset(item, "tabView", self);
    if (!pget(self, "selectedTabViewItem")) tab_show(self, item);
}
static void tab_insert(id self, SEL c, id item, i64 i) { (void)c; MSG(void, tv_items(self), "insertObject:atIndex:", item, (u64)i); pset(item, "tabView", self); if (!pget(self, "selectedTabViewItem")) tab_show(self, item); }
static void tab_remove(id self, SEL c, id item) { (void)c; MSG(void, tv_items(self), "removeObject:", item); if (pget(self, "selectedTabViewItem") == item) tab_show(self, 0); }
static id tab_selected(id self, SEL c) { (void)c; return pget(self, "selectedTabViewItem"); }
static void tab_select(id self, SEL c, id item) { (void)c; tab_show(self, item); }
static id tab_item_at(id self, SEL c, i64 i) { (void)c; id a = tv_items(self); return i >= 0 && (u64)i < MSG0(u64, a, "count") ? MSG(id, a, "objectAtIndex:", (u64)i) : 0; }
static void tab_select_at(id self, SEL c, i64 i) { (void)c; tab_show(self, tab_item_at(self, 0, i)); }
static i64 tab_index_of(id self, SEL c, id item) { (void)c; u64 r = MSG(u64, tv_items(self), "indexOfObject:", item); return r == (u64)-1 >> 1 || r == ~0ull ? -1 : (i64)r; }
static void tab_select_ident(id self, SEL c, id ident) {
    (void)c;
    id a = tv_items(self);
    for (u64 i = 0; i < MSG0(u64, a, "count"); i++) { id it = MSG(id, a, "objectAtIndex:", i); id x = pget(it, "identifier"); if (x == ident || (x && MSG(BOOL, x, "isEqual:", ident))) { tab_show(self, it); return; } }
}
static void tab_set_frame_size(id self, SEL c, pr_size_t z) {
    (void)c;
    SUPER("NSView", void, self, "setFrameSize:", z);
    id it = pget(self, "selectedTabViewItem");
    id v = it ? pget(it, "view") : 0;
    if (v) MSG(void, v, "setFrame:", MSG0(pr_rect_t, self, "bounds"));
}
/* NSTabViewController: a view é um NSTabView */
static id tvc_tab_view(id self, SEL c) {
    (void)c;
    id t = pget(self, "tabViewP");
    if (!t) {
        t = MSG(id, MSG0(id, (id)&NSTabView_c, "alloc"), "initWithFrame:", ((pr_rect_t){0, 0, 480, 270}));
        pset(self, "tabViewP", t);
        objc_release(t);
    }
    return t;
}
static id tvc_view(id self, SEL c) { (void)c; return tvc_tab_view(self, 0); }
static id tvc_items(id self, SEL c) { (void)c; return tab_items(tvc_tab_view(self, 0), 0); }
static void tvc_add(id self, SEL c, id item) { (void)c; tab_add(tvc_tab_view(self, 0), 0, item); }
static void tvc_insert(id self, SEL c, id item, i64 i) { (void)c; tab_insert(tvc_tab_view(self, 0), 0, item, i); }
static void tvc_remove(id self, SEL c, id item) { (void)c; tab_remove(tvc_tab_view(self, 0), 0, item); }
static i64 tvc_sel_index(id self, SEL c) { (void)c; id t = tvc_tab_view(self, 0); return tab_index_of(t, 0, tab_selected(t, 0)); }
static void tvc_set_sel_index(id self, SEL c, i64 i) { (void)c; tab_select_at(tvc_tab_view(self, 0), 0, i); }
static id tvc_item_for_vc(id self, SEL c, id vc) {
    (void)c;
    id a = tvc_items(self, 0);
    for (u64 i = 0; i < MSG0(u64, a, "count"); i++) { id it = MSG(id, a, "objectAtIndex:", i); if (pget(it, "viewController") == vc) return it; }
    return 0;
}

/* ================================================================ NSGradient, NSDocument, NSColorPanel e afins */
static id gr_init_colors(id self, SEL c, id colors) { (void)c; self = MSG0(id, self, "init"); if (self) pset(self, "colors", colors); return self; }
static id gr_init_two(id self, SEL c, id a, id b) {
    (void)c;
    id arr = MSG0(id, (id)cl("NSMutableArray"), "array");
    if (a) MSG(void, arr, "addObject:", a);
    if (b) MSG(void, arr, "addObject:", b);
    return gr_init_colors(self, 0, arr);
}
/* desenha com a primeira cor (sem degradê no mOS ainda) */
static void gr_draw_rect(id self, SEL c, pr_rect_t r, double angle) {
    (void)c; (void)angle;
    id cs = pget(self, "colors");
    if (!cs || !MSG0(u64, cs, "count")) return;
    MSG0(void, MSG(id, cs, "objectAtIndex:", (u64)0), "set");
    extern void NSRectFill(pr_rect_t r);
    NSRectFill(r);
}
static void gr_draw_path(id self, SEL c, id path, double angle) { (void)self; (void)c; (void)path; (void)angle; }
static id cp_shared(id cls, SEL c) { (void)c; static id p; if (!p) p = MSG0(id, MSG0(id, cls, "alloc"), "init"); return p; }
static id doc_init(id self, SEL c) { (void)c; return SUPER("NSObject", id, self, "init"); }
static id doc_wcs(id self, SEL c) { (void)c; id a = pget(self, "windowControllers"); if (!a) { a = MSG0(id, (id)cl("NSMutableArray"), "array"); pset(self, "windowControllers", a); } return a; }
static void doc_add_wc(id self, SEL c, id wc) { (void)c; MSG(void, doc_wcs(self, 0), "addObject:", wc); if (cx_responds(wc->isa, sel_registerName("setDocument:"))) MSG(void, wc, "setDocument:", self); }
static void doc_show(id self, SEL c) {
    (void)c;
    id a = doc_wcs(self, 0);
    for (u64 i = 0; i < MSG0(u64, a, "count"); i++) MSG(void, MSG(id, a, "objectAtIndex:", i), "showWindow:", self);
}
static void doc_make_wcs(id self, SEL c) { (void)self; (void)c; }
static BOOL doc_edited(id s, SEL c) { (void)s; (void)c; return 0; }

/* ================================================================ funções e constantes */
void NSSetFocusRingStyle(u64 placement) { (void)placement; }
typedef struct { void *isa; u64 flags; const char *s; u64 n; } ap_const_t;
extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
#define AK(var, text) static ap_const_t var##_s = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; const void *var = &var##_s;
AK(NSApplicationDidFinishRestoringWindowsNotification, "NSApplicationDidFinishRestoringWindowsNotification")
AK(NSContinuouslyUpdatesValueBindingOption, "NSContinuouslyUpdatesValue")
AK(NSControlTextDidBeginEditingNotification, "NSControlTextDidBeginEditingNotification")
AK(NSControlTextDidEndEditingNotification, "NSControlTextDidEndEditingNotification")
AK(NSImageNameActionTemplate, "NSActionTemplate") AK(NSImageNameAddTemplate, "NSAddTemplate") AK(NSImageNameCaution, "NSCaution")
AK(NSImageNameIconViewTemplate, "NSIconViewTemplate") AK(NSImageNameLeftFacingTriangleTemplate, "NSLeftFacingTriangleTemplate")
AK(NSImageNameRefreshTemplate, "NSRefreshTemplate") AK(NSImageNameRemoveTemplate, "NSRemoveTemplate")
AK(NSImageNameRightFacingTriangleTemplate, "NSRightFacingTriangleTemplate")
AK(NSImageNameStopProgressFreestandingTemplate, "NSStopProgressFreestandingTemplate") AK(NSImageNameStopProgressTemplate, "NSStopProgressTemplate")
AK(NSOutlineViewItemDidCollapseNotification, "NSOutlineViewItemDidCollapseNotification")
AK(NSOutlineViewItemDidExpandNotification, "NSOutlineViewItemDidExpandNotification")
AK(NSPopoverCloseReasonDetachToWindow, "NSPopoverCloseReasonDetachToWindow") AK(NSPopoverCloseReasonKey, "NSPopoverCloseReasonKey")
AK(NSSplitViewDidResizeSubviewsNotification, "NSSplitViewDidResizeSubviewsNotification")
AK(NSTableViewColumnDidResizeNotification, "NSTableViewColumnDidResizeNotification")
AK(NSTextDidBeginEditingNotification, "NSTextDidBeginEditingNotification") AK(NSTextDidEndEditingNotification, "NSTextDidEndEditingNotification")
AK(NSToolbarFlexibleSpaceItemIdentifier, "NSToolbarFlexibleSpaceItem") AK(NSToolbarSpaceItemIdentifier, "NSToolbarSpaceItem")
AK(NSToolbarToggleInspectorItemIdentifier, "NSToolbarToggleInspectorItem")
AK(NSValueBinding, "value") AK(NSValueTransformerNameBindingOption, "NSValueTransformerName")
AK(NSViewBoundsDidChangeNotification, "NSViewBoundsDidChangeNotification")
const double NSViewNoIntrinsicMetric = -1;

/* overlay Swift do AppKit (o Postico importa estes do AppKit.framework): CGRect.fill(using:),
 * CGRect.frame(withWidth:using:) e NSSound.beep(). Swift passa o CGRect como 4 doubles (xmm0-3) e a operação em rdi,
 * como a chamada C abaixo; o self de um método estático (a metaclasse) vai em r13 e não é usado. */
extern void NSRectFillUsingOperation(pr_rect_t r, u64 op);
extern void NSBeep(void);
void ak_rect_fill(double x, double y, double w, double h, u64 op) __asm__("_$sSo6CGRectV6AppKitE4fill5usingySo22NSCompositingOperationV_tF");
void ak_rect_fill(double x, double y, double w, double h, u64 op) { NSRectFillUsingOperation((pr_rect_t){x, y, w, h}, op); }
void ak_rect_frame(double x, double y, double w, double h, double width, u64 op)
    __asm__("_$sSo6CGRectV6AppKitE5frame9withWidth5usingy12CoreGraphics7CGFloatV_So22NSCompositingOperationVtF");
void ak_rect_frame(double x, double y, double w, double h, double width, u64 op) {
    (void)op;                                  /* as quatro bordas por dentro do retângulo */
    extern void NSRectFill(pr_rect_t r);
    NSRectFill((pr_rect_t){x, y, w, width}); NSRectFill((pr_rect_t){x, y + h - width, w, width});
    NSRectFill((pr_rect_t){x, y, width, h}); NSRectFill((pr_rect_t){x + w - width, y, width, h});
}
void ak_sound_beep(void) __asm__("_$sSo7NSSoundC6AppKitE4beepyyFZ");
void ak_sound_beep(void) { NSBeep(); }

/* -[NSDictionary __swift_objectForKeyedSubscript:]: o subscript do overlay Foundation da Apple (código inlinável dos apps) */
static id dict_swift_subscript(id self, SEL c, id key) { (void)c; return key ? MSG(id, self, "objectForKey:", key) : 0; }

/* NSImage(systemSymbolName:): o mOS não tem os SF Symbols; uma imagem vazia de 16x16 no lugar de nil (o Postico força o
 * desembrulho: "sidebar.leading") */
static id img_symbol(id cls, SEL c, id name, id desc) {
    (void)c; (void)name; (void)desc;
    return objc_autorelease(MSG(id, MSG0(id, cls, "alloc"), "initWithSize:", ((pr_size_t){16, 16})));
}
static id img_symbol3(id cls, SEL c, id name, double v, id desc) { (void)v; return img_symbol(cls, c, name, desc); }
static id img_symbol_cfg(id self, SEL c, id cfg) { (void)c; (void)cfg; return self; }

static id fpr_types(id cls, SEL c) { (void)cls; (void)c; id a = MSG0(id, (id)cl("NSMutableArray"), "array"); MSG(void, a, "addObject:", cx_str("com.apple.pasteboard.promised-file-url")); MSG(void, a, "addObject:", cx_str("com.apple.NSFilePromiseItemMetaData")); return a; }

/* cores do catálogo de recursos (Assets.car, que o mOS ainda não lê): uma cor neutra no lugar de nil */
static id color_named(id cls, SEL c, id name) {
    (void)c;
    const char *n = name ? cx_cstr(name) : "";
    double g = (strstr(n, "ackground") || strstr(n, "Fill")) ? 0.96 : (strstr(n, "Text") || strstr(n, "Label")) ? 0.1 : 0.55;
    return MSG(id, cls, "colorWithSRGBRed:green:blue:alpha:", g, g, g, 1.0);
}
static id color_named_b(id cls, SEL c, id name, id bundle) { (void)bundle; return color_named(cls, c, name); }
/* +[NSButton buttonWithImage:target:action:] */
static id bt_with_image(id cls, SEL c, id image, id target, SEL action) {
    (void)c;
    id b = MSG(id, MSG0(id, cls, "alloc"), "initWithFrame:", ((pr_rect_t){0, 0, 28, 24}));
    MSG(void, b, "setTitle:", cx_str(""));
    if (image) MSG(void, b, "setImage:", image);
    MSG(void, b, "setTarget:", target);
    MSG(void, b, "setAction:", action);
    return objc_autorelease(b);
}

/* NSView: enclosingScrollView, menu; NSTableCellView.imageView; NSObject.awakeFromNib */
static id v_enclosing_scroll(id self, SEL c) {
    (void)c;
    cx_class_t *sc = cl("NSScrollView");
    for (id v = MSG0(id, self, "superview"); v; v = MSG0(id, v, "superview")) { extern const char *object_getClassName(id o); cx_fdprintf(2, "DBG esv %s\n", object_getClassName(v)); }
    cx_fdprintf(2, "DBG esv fim (%p)\n", (void *)self);
    for (id v = MSG0(id, self, "superview"); v; v = MSG0(id, v, "superview")) if (sc && cx_is_subclass(v->isa, sc)) return v;
    return 0;
}
static id v_menu(id self, SEL c) { (void)c; return pget(self, "menu"); }
static void v_set_menu(id self, SEL c, id m) { (void)c; pset(self, "menu", m); }
static id tcv_image_view(id self, SEL c) { (void)c; return pget(self, "imageView"); }
static void tcv_set_image_view(id self, SEL c, id v) { (void)c; pset(self, "imageView", v); }
static void tv_drag_mask(id s, SEL c, u64 m, BOOL l) { (void)s; (void)c; (void)m; (void)l; }

/* ================================================================ registro */
void cx_appkit_postico_init(void) {
    { cx_class_t *v = cl("NSView"); if (v) { if (!cx_responds(v, sel_registerName("enclosingScrollView"))) cx_add_method(v, "enclosingScrollView", (void *)v_enclosing_scroll); if (!cx_responds(v, sel_registerName("menu"))) { cx_add_method(v, "menu", (void *)v_menu); cx_add_method(v, "setMenu:", (void *)v_set_menu); } } }
    { cx_class_t *k = cl("NSTableCellView"); if (k && !cx_responds(k, sel_registerName("imageView"))) { cx_add_method(k, "imageView", (void *)tcv_image_view); cx_add_method(k, "setImageView:", (void *)tcv_set_image_view); } }
    { cx_class_t *k = cl("NSTableView"); if (k) cx_add_method(k, "setDraggingSourceOperationMask:forLocal:", (void *)tv_drag_mask); }
    { cx_class_t *k = cl("NSObject"); if (k && !cx_responds(k, sel_registerName("awakeFromNib"))) cx_add_method(k, "awakeFromNib", (void *)noop); }
    { cx_class_t *k = cl("NSColor"); if (k) { cx_add_cmethod(k, "colorNamed:", (void *)color_named); cx_add_cmethod(k, "colorNamed:bundle:", (void *)color_named_b); } }
    { cx_class_t *k = cl("NSButton"); if (k) cx_add_cmethod(k, "buttonWithImage:target:action:", (void *)bt_with_image); }
    { cx_class_t *im = cl("NSImage"); if (im) { cx_add_cmethod(im, "imageWithSystemSymbolName:accessibilityDescription:", (void *)img_symbol); cx_add_cmethod(im, "imageWithSystemSymbolName:variableValue:accessibilityDescription:", (void *)img_symbol3); cx_add_method(im, "imageWithSymbolConfiguration:", (void *)img_symbol_cfg); } }
    { cx_class_t *d = cl("NSDictionary"); if (d) cx_add_method(d, "__swift_objectForKeyedSubscript:", (void *)dict_swift_subscript); }
    DEF(NSToolbarItem, "NSObject");
    IM(NSToolbarItem, "initWithItemIdentifier:", tbi_init);
    REG(NSToolbarItem, itemIdentifier, setItemIdentifier); REG(NSToolbarItem, label, setLabel); REG(NSToolbarItem, paletteLabel, setPaletteLabel);
    REG(NSToolbarItem, toolTip, setToolTip); REG(NSToolbarItem, image, setImage); REG(NSToolbarItem, view, setView);
    REG(NSToolbarItem, target, setTarget); REG(NSToolbarItem, menuFormRepresentation, setMenuFormRepresentation);
    REG(NSToolbarItem, title, setTitle); REG(NSToolbarItem, possibleLabels, setPossibleLabels); REG(NSToolbarItem, tag, setTag);
    REG(NSToolbarItem, visibilityPriority, setVisibilityPriority); REG(NSToolbarItem, bordered, setBordered);
    REG(NSToolbarItem, navigational, setNavigational); REG(NSToolbarItem, autovalidates, setAutovalidates);
    IM(NSToolbarItem, "isEnabled", g_enabled); IM(NSToolbarItem, "setEnabled:", s_enabled); IM(NSToolbarItem, "isBordered", g_bordered);
    IM(NSToolbarItem, "isNavigational", g_navigational);
    IM(NSToolbarItem, "action", tbi_action); IM(NSToolbarItem, "setAction:", tbi_set_action);
    IM(NSToolbarItem, "minSize", tbi_min); IM(NSToolbarItem, "maxSize", tbi_min);
    IM(NSToolbarItem, "setMinSize:", tbi_set_size); IM(NSToolbarItem, "setMaxSize:", tbi_set_size);
    IM(NSToolbarItem, "toolbar", tbi_toolbar); IM(NSToolbarItem, "validate", noop);

    DEF(NSSplitView, "NSView");
    IM(NSSplitView, "adjustSubviews", sv_adjust); IM(NSSplitView, "setFrameSize:", sv_set_frame_size); IM(NSSplitView, "setFrame:", sv_set_frame);
    IM(NSSplitView, "layout", sv_layout); IM(NSSplitView, "isVertical", sv_vertical); IM(NSSplitView, "setVertical:", sv_set_vertical);
    IM(NSSplitView, "dividerThickness", sv_div); IM(NSSplitView, "arrangedSubviews", sv_arranged);
    IM(NSSplitView, "addArrangedSubview:", sv_add_arranged); IM(NSSplitView, "insertArrangedSubview:atIndex:", sv_insert_arranged);
    IM(NSSplitView, "removeArrangedSubview:", sv_remove_arranged); IM(NSSplitView, "setPosition:ofDividerAtIndex:", sv_set_pos);
    IM(NSSplitView, "minPossiblePositionOfDividerAtIndex:", sv_min_pos); IM(NSSplitView, "maxPossiblePositionOfDividerAtIndex:", sv_max_pos);
    IM(NSSplitView, "isSubviewCollapsed:", sv_collapsed); IM(NSSplitView, "holdingPriorityForSubviewAtIndex:", sv_holding);
    IM(NSSplitView, "setHoldingPriority:forSubviewAtIndex:", sv_set_holding);
    REG(NSSplitView, dividerStyle, setDividerStyle); REG(NSSplitView, arrangesAllSubviews, setArrangesAllSubviews);
    REG(NSSplitView, autosaveName, setAutosaveName);
    IM(NSSplitView, "delegate", g_delegate); IM(NSSplitView, "setDelegate:", s_delegate);

    DEF(_MOSOutlineAdapter, "NSObject");
    IM(_MOSOutlineAdapter, "numberOfRowsInTableView:", oa_rows);
    IM(_MOSOutlineAdapter, "tableView:objectValueForTableColumn:row:", oa_value);
    IM(_MOSOutlineAdapter, "tableView:viewForTableColumn:row:", oa_view);
    IM(_MOSOutlineAdapter, "tableViewSelectionDidChange:", oa_sel_changed);
    DEF(NSOutlineView, "NSTableView");
    IM(NSOutlineView, "setDataSource:", ov_set_ds); IM(NSOutlineView, "dataSource", ov_ds);
    IM(NSOutlineView, "setDelegate:", ov_set_dl); IM(NSOutlineView, "delegate", ov_dl);
    IM(NSOutlineView, "reloadData", ov_reload); IM(NSOutlineView, "reloadItem:", ov_reload_item);
    IM(NSOutlineView, "reloadItem:reloadChildren:", ov_reload_item2);
    IM(NSOutlineView, "expandItem:", ov_expand1); IM(NSOutlineView, "expandItem:expandChildren:", ov_expand);
    IM(NSOutlineView, "collapseItem:", ov_collapse1); IM(NSOutlineView, "collapseItem:collapseChildren:", ov_collapse);
    IM(NSOutlineView, "isItemExpanded:", ov_is_exp); IM(NSOutlineView, "isExpandable:", ov_is_expandable);
    IM(NSOutlineView, "itemAtRow:", ov_item_at); IM(NSOutlineView, "rowForItem:", ov_row_for);
    IM(NSOutlineView, "levelForItem:", ov_level); IM(NSOutlineView, "levelForRow:", ov_level_row); IM(NSOutlineView, "parentForItem:", ov_parent);
    IM(NSOutlineView, "numberOfChildrenOfItem:", ov_children); IM(NSOutlineView, "child:ofItem:", ov_child);
    IM(NSOutlineView, "outlineTableColumn", ov_outline_col); IM(NSOutlineView, "setOutlineTableColumn:", ov_set_outline_col);
    IM(NSOutlineView, "insertItemsAtIndexes:inParent:withAnimation:", ov_insert_items);
    IM(NSOutlineView, "removeItemsAtIndexes:inParent:withAnimation:", ov_insert_items);
    IM(NSOutlineView, "moveItemAtIndex:inParent:toIndex:inParent:", ov_move_item);
    REG(NSOutlineView, indentationPerLevel, setIndentationPerLevel); REG(NSOutlineView, autoresizesOutlineColumn, setAutoresizesOutlineColumn);
    REG(NSOutlineView, indentationMarkerFollowsCell, setIndentationMarkerFollowsCell); REG(NSOutlineView, floatsGroupRows, setFloatsGroupRows);
    REG(NSOutlineView, selectionHighlightStyle, setSelectionHighlightStyle); REG(NSOutlineView, rowSizeStyle, setRowSizeStyle);
    REG(NSOutlineView, autosaveName, setAutosaveName);
    IM(NSOutlineView, "autosaveExpandedItems", ret_no); IM(NSOutlineView, "setAutosaveExpandedItems:", noop1);
    IM(NSOutlineView, "beginUpdates", noop); IM(NSOutlineView, "endUpdates", noop);

    DEF(NSTableRowView, "NSView");
    REG(NSTableRowView, selectionHighlightStyle, setSelectionHighlightStyle); REG(NSTableRowView, emphasized, setEmphasized);
    REG(NSTableRowView, groupRowStyle, setGroupRowStyle);
    IM(NSTableRowView, "isSelected", g_selected); IM(NSTableRowView, "setSelected:", s_selected); IM(NSTableRowView, "isEmphasized", g_emphasized);
    IM(NSTableRowView, "drawSelectionInRect:", noop1); IM(NSTableRowView, "drawBackgroundInRect:", noop1);
    DEF(NSTableHeaderView, "NSView"); IM(NSTableHeaderView, "tableView", ret_nil);

    DEF(NSTextFieldCell, "NSActionCell"); DEF(NSSecureTextFieldCell, "NSTextFieldCell"); DEF(NSSearchFieldCell, "NSTextFieldCell");
    DEF(NSTokenFieldCell, "NSTextFieldCell"); DEF(NSTableHeaderCell, "NSTextFieldCell"); DEF(NSSegmentedCell, "NSActionCell");
    DEF(NSImageCell, "NSCell"); DEF(NSTextAttachmentCell, "NSCell");
    REG(NSTextFieldCell, placeholderString, setPlaceholderString); REG(NSSegmentedCell, segmentCount, setSegmentCount);
    REG(NSSegmentedCell, selectedSegment, setSelectedSegment);

    DEF(NSSearchField, "NSTextField"); DEF(NSTokenField, "NSTextField");
    REG(NSSearchField, recentSearches, setRecentSearches); REG(NSSearchField, recentsAutosaveName, setRecentsAutosaveName);
    REG(NSSearchField, sendsSearchStringImmediately, setSendsSearchStringImmediately);
    REG(NSSearchField, sendsWholeSearchString, setSendsWholeSearchString); REG(NSSearchField, maximumRecents, setMaximumRecents);
    REG(NSTokenField, tokenizingCharacterSet, setTokenizingCharacterSet);

    DEF(NSTabView, "NSView"); DEF(NSTabViewItem, "NSObject"); DEF(NSTabViewController, "NSViewController"); DEF(NSPageController, "NSViewController");
    IM(NSTabView, "tabViewItems", tab_items); IM(NSTabView, "numberOfTabViewItems", tab_count); IM(NSTabView, "addTabViewItem:", tab_add);
    IM(NSTabView, "insertTabViewItem:atIndex:", tab_insert); IM(NSTabView, "removeTabViewItem:", tab_remove);
    IM(NSTabView, "selectedTabViewItem", tab_selected); IM(NSTabView, "selectTabViewItem:", tab_select);
    IM(NSTabView, "selectTabViewItemAtIndex:", tab_select_at); IM(NSTabView, "tabViewItemAtIndex:", tab_item_at);
    IM(NSTabView, "indexOfTabViewItem:", tab_index_of); IM(NSTabView, "selectTabViewItemWithIdentifier:", tab_select_ident);
    IM(NSTabView, "setFrameSize:", tab_set_frame_size); REG(NSTabView, tabViewType, setTabViewType);
    IM(NSTabView, "delegate", g_delegate); IM(NSTabView, "setDelegate:", s_delegate);
    IM(NSTabViewItem, "initWithIdentifier:", tvi_init); CM(NSTabViewItem, "tabViewItemWithViewController:", tvi_with_vc);
    REG(NSTabViewItem, identifier, setIdentifier); REG(NSTabViewItem, label, setLabel); REG(NSTabViewItem, view, setView);
    REG(NSTabViewItem, viewController, setViewController); REG(NSTabViewItem, image, setImage); REG(NSTabViewItem, toolTip, setToolTip);
    IM(NSTabViewItem, "tabView", g_tabView);
    IM(NSTabViewController, "tabView", tvc_tab_view); IM(NSTabViewController, "view", tvc_view); IM(NSTabViewController, "tabViewItems", tvc_items);
    IM(NSTabViewController, "addTabViewItem:", tvc_add); IM(NSTabViewController, "insertTabViewItem:atIndex:", tvc_insert);
    IM(NSTabViewController, "removeTabViewItem:", tvc_remove); IM(NSTabViewController, "selectedTabViewItemIndex", tvc_sel_index);
    IM(NSTabViewController, "setSelectedTabViewItemIndex:", tvc_set_sel_index); IM(NSTabViewController, "tabViewItemForViewController:", tvc_item_for_vc);
    REG(NSTabViewController, tabStyle, setTabStyle);

    DEF(NSGridView, "NSView"); DEF(NSGridCell, "NSObject"); DEF(NSGridRow, "NSObject"); DEF(NSGridColumn, "NSObject");
    REG(NSGridView, rowSpacing, setRowSpacing); REG(NSGridView, columnSpacing, setColumnSpacing);
    DEF(NSDocument, "NSObject");
    IM(NSDocument, "init", doc_init); IM(NSDocument, "windowControllers", doc_wcs); IM(NSDocument, "addWindowController:", doc_add_wc);
    IM(NSDocument, "showWindows", doc_show); IM(NSDocument, "makeWindowControllers", doc_make_wcs); IM(NSDocument, "isDocumentEdited", doc_edited);
    REG(NSDocument, fileURL, setFileURL); REG(NSDocument, fileType, setFileType); REG(NSDocument, displayName, setDisplayName);
    DEF(NSGradient, "NSObject");
    IM(NSGradient, "initWithColors:", gr_init_colors); IM(NSGradient, "initWithStartingColor:endingColor:", gr_init_two);
    IM(NSGradient, "drawInRect:angle:", gr_draw_rect); IM(NSGradient, "drawInBezierPath:angle:", gr_draw_path);
    DEF(NSColorPanel, "NSPanel"); CM(NSColorPanel, "sharedColorPanel", cp_shared); REG(NSColorPanel, color, setColor);
    DEF(NSRulerView, "NSView"); DEF(NSRulerMarker, "NSObject");
    { cx_class_t *tb = cl("NSTextBlock"); DEF(NSTextTable, tb ? "NSTextBlock" : "NSObject"); DEF(NSTextTableBlock, tb ? "NSTextBlock" : "NSObject"); }
    DEF(NSDraggingImageComponent, "NSObject"); DEF(NSFilePromiseProvider, "NSObject"); DEF(NSFilePromiseReceiver, "NSObject"); CM(NSFilePromiseReceiver, "readableDraggedTypes", fpr_types);
    (void)ret_self; (void)ret_yes; (void)g_alphaValueP; (void)s_alphaValueP; (void)g_objectValue; (void)s_objectValue;
    (void)g_state; (void)s_state; (void)g_arrangedSubviewsStore; (void)s_arrangedSubviewsStore; (void)g_viewController;
    (void)s_tabView;
}
