/* mOS libSystem — NSTableView/NSOutlineView completas (incluído em appkit.c, depois do código base da tabela):
 *   seleção múltipla (NSIndexSet; shift estende da âncora, ⌘ alterna; selectionIndexesForProposedSelection:,
 *   shouldSelectRow:, selectionShouldChangeInTableView:, notificações), teclado (setas, Home/End, PageUp/Down, ⌘A,
 *   busca por digitação), altura por linha (tableView:heightOfRow:) e linhas ocultas, cabeçalho com ordenação
 *   (sortDescriptorPrototype → sortDescriptors → tableView:sortDescriptorsDidChange:), indicador de ordem, redimensionar e
 *   reordenar colunas arrastando o cabeçalho, células de verdade no modo cell-based (dataCell + willDisplayCell:),
 *   arrastar linhas (pasteboardWriterForRow: / writeRowsWithIndexes:toPasteboard:) e soltar (validateDrop:/acceptDrop:),
 *   e a geometria pública (rectOfRow:, rowAtPoint:, rowsInRect:, frameOfCellAtColumn:row:...). */

static double tv_abs(double v) { return v < 0 ? -v : v; }
static char k_tv_sel, k_tv_anchor, k_tv_heights, k_tv_hidden, k_tv_sort, k_tv_props, k_tv_type, k_tv_drop;
static id tvp(id t) {   /* propriedades soltas da tabela */
    id d = objc_getAssociatedObject(t, &k_tv_props);
    if (!d) { d = MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary"); objc_setAssociatedObject(t, &k_tv_props, d, 1); }
    return d;
}
static i64 tvp_i(id t, const char *k, i64 def) { id v = MSG(id, tvp(t), "objectForKey:", cx_str(k)); return v ? MSG0(i64, v, "longLongValue") : def; }
static void tvp_set_i(id t, const char *k, i64 v) { MSG(void, tvp(t), "setObject:forKey:", cx_num_i(v), cx_str(k)); }
static id tvp_o(id t, const char *k) { return MSG(id, tvp(t), "objectForKey:", cx_str(k)); }
static void tvp_set_o(id t, const char *k, id v) { if (v) MSG(void, tvp(t), "setObject:forKey:", v, cx_str(k)); else MSG(void, tvp(t), "removeObjectForKey:", cx_str(k)); }
static id new_indexset(void) { return MSG0(id, (id)cx_class_named("NSMutableIndexSet"), "indexSet"); }
static BOOL dresp(id o, const char *s) { return o && cx_responds(o->isa, sel_registerName(s)); }

/* ---- geometria: deslocamento de cada linha (altura do delegate; linhas ocultas têm altura 0) */
static double tv_base_h(vstate_t *s) { return s->row_h > 0 ? s->row_h : 24; }
static double tv_header_h(vstate_t *s) { return s->header ? 24 : 0; }
static double *tv_offsets(id t, i64 *n_out) {
    vstate_t *s = vs(t);
    i64 n = tv_rows(t);
    *n_out = n;
    id hidden = objc_getAssociatedObject(t, &k_tv_hidden);
    int var = dresp(s->delegate, "tableView:heightOfRow:") || dresp(s->delegate, "outlineView:heightOfRowByItem:") || (hidden && MSG0(u64, hidden, "count"));
    if (!var) return 0;
    id cache = objc_getAssociatedObject(t, &k_tv_heights);
    if (cache && (i64)(MSG0(u64, cache, "length") / sizeof(double)) == n + 1) return MSG0(double *, cache, "mutableBytes");
    cache = MSG(id, (id)cx_class_named("NSMutableData"), "dataWithLength:", (u64)((n + 1) * sizeof(double)));
    double *o = MSG0(double *, cache, "mutableBytes");
    double y = 0;
    for (i64 r = 0; r < n; r++) {
        o[r] = y;
        double h = tv_base_h(s);
        if (hidden && MSG(BOOL, hidden, "containsIndex:", (u64)r)) h = 0;
        else if (dresp(s->delegate, "tableView:heightOfRow:")) h = MSG(double, s->delegate, "tableView:heightOfRow:", t, r);
        else if (dresp(s->delegate, "outlineView:heightOfRowByItem:") && dresp(t, "itemAtRow:")) { id it = MSG(id, t, "itemAtRow:", r); h = MSG(double, s->delegate, "outlineView:heightOfRowByItem:", t, it); }
        y += h > 0 ? h : 0;
    }
    o[n] = y;
    objc_setAssociatedObject(t, &k_tv_heights, cache, 1);
    return o;
}
static void tv_heights_dirty(id t) { objc_setAssociatedObject(t, &k_tv_heights, 0, 1); }
static double tv_row_top(id t, i64 r) { i64 n; double *o = tv_offsets(t, &n); if (r < 0) r = 0; if (r > n) r = n; return o ? o[r] : r * tv_base_h(vs(t)); }
static double tv_row_height(id t, i64 r) { i64 n; double *o = tv_offsets(t, &n); return o ? (r >= 0 && r < n ? o[r + 1] - o[r] : 0) : tv_base_h(vs(t)); }
static double tv_total_h(id t) { i64 n; double *o = tv_offsets(t, &n); return o ? o[n] : n * tv_base_h(vs(t)); }
/* linha sob y (documento, sem o cabeçalho); -1 fora */
static i64 tv_row_at_y(id t, double y) {
    i64 n; double *o = tv_offsets(t, &n);
    if (y < 0) return -1;
    if (!o) { i64 r = (i64)(y / tv_base_h(vs(t))); return r < n ? r : -1; }
    i64 lo = 0, hi = n - 1;
    while (lo <= hi) { i64 m = (lo + hi) / 2; if (y < o[m]) hi = m - 1; else if (y >= o[m + 1]) lo = m + 1; else return o[m + 1] > o[m] ? m : -1; }
    return -1;
}

/* ---- seleção */
static id tv_selset(id t) {
    id s = objc_getAssociatedObject(t, &k_tv_sel);
    if (!s) { s = new_indexset(); objc_setAssociatedObject(t, &k_tv_sel, s, 1); i64 r = vs(t)->selected; if (r >= 0) MSG(void, s, "addIndex:", (u64)r); }
    return s;
}
static void tv_post(id t, const char *name, SEL dsel) {
    id note = objc_autorelease(cx_note(cx_str(name), t, 0));
    vstate_t *s = vs(t);
    if (s->delegate && dsel && cx_responds(s->delegate->isa, dsel)) ((void (*)(id, SEL, id))objc_msgSend)(s->delegate, dsel, note);
    cx_post(note);
}
/* aplica uma seleção proposta: delegate decide; devolve 1 se mudou */
static int tv_apply_selection(id t, id proposed, BOOL notify) {
    vstate_t *s = vs(t);
    id cur = tv_selset(t);
    if (MSG(BOOL, cur, "isEqualToIndexSet:", proposed)) return 0;
    if (dresp(s->delegate, "selectionShouldChangeInTableView:") && !MSG(BOOL, s->delegate, "selectionShouldChangeInTableView:", t)) return 0;
    id want = proposed;
    if (dresp(s->delegate, "tableView:selectionIndexesForProposedSelection:"))
        want = MSG(id, s->delegate, "tableView:selectionIndexesForProposedSelection:", t, proposed);
    else if (dresp(s->delegate, "tableView:shouldSelectRow:")) {
        id f = new_indexset();
        for (u64 i = MSG0(u64, proposed, "firstIndex"); i != (u64)0x7fffffffffffffffULL; i = MSG(u64, proposed, "indexGreaterThanIndex:", i))
            if (MSG(BOOL, cur, "containsIndex:", i) || MSG(BOOL, s->delegate, "tableView:shouldSelectRow:", t, (i64)i)) MSG(void, f, "addIndex:", i);
        want = f;
    }
    if (!want) want = new_indexset();
    if (!tvp_i(t, "allowsEmpty", 1) && !MSG0(u64, want, "count") && MSG0(u64, cur, "count")) return 0;
    if (MSG(BOOL, cur, "isEqualToIndexSet:", want)) return 0;
    if (notify) tv_post(t, "NSTableViewSelectionIsChangingNotification", sel_registerName("tableViewSelectionIsChanging:"));
    MSG0(void, cur, "removeAllIndexes");
    MSG(void, cur, "addIndexes:", want);
    u64 last = MSG0(u64, cur, "lastIndex");
    s->selected = MSG0(u64, cur, "count") ? (i64)last : -1;
    mark(t);
    if (notify) tv_post(t, "NSTableViewSelectionDidChangeNotification", sel_registerName("tableViewSelectionDidChange:"));
    return 1;
}
static void select_rows_set(id t, id set, BOOL extend, BOOL notify) {
    id p = new_indexset();
    if (extend) MSG(void, p, "addIndexes:", tv_selset(t));
    if (set) MSG(void, p, "addIndexes:", set);
    if (!tvp_i(t, "multi", 0) && MSG0(u64, p, "count") > 1) {
        u64 keep = set && MSG0(u64, set, "count") ? MSG0(u64, set, "lastIndex") : MSG0(u64, p, "lastIndex");
        MSG0(void, p, "removeAllIndexes"); MSG(void, p, "addIndex:", keep);
    }
    tv_apply_selection(t, p, notify);
}
static void tv2_select(id self, SEL c, id ixs, BOOL ext) { (void)c; select_rows_set(self, ixs, ext, NO); }
static void tv2_deselect_all(id self, SEL c, id sender) {
    (void)c; (void)sender;
    BOOL keep = tvp_i(self, "allowsEmpty", 1);
    tvp_set_i(self, "allowsEmpty", 1);
    tv_apply_selection(self, new_indexset(), NO);
    tvp_set_i(self, "allowsEmpty", keep);
}
static void tv2_deselect_row(id self, SEL c, i64 r) {
    (void)c;
    id p = MSG0(id, tv_selset(self), "mutableCopy");
    MSG(void, p, "removeIndex:", (u64)r);
    tv_apply_selection(self, p, NO);
    objc_release(p);
}
static void tv2_select_all(id self, SEL c, id sender) {
    (void)c; (void)sender;
    if (!tvp_i(self, "multi", 0)) return;
    id p = new_indexset();
    i64 n = tv_rows(self);
    if (n > 0) MSG(void, p, "addIndexesInRange:", (u64)0, (u64)n);
    tv_apply_selection(self, p, YES);
}
static id tv2_sel_indexes(id self, SEL c) { (void)c; return MSG0(id, tv_selset(self), "copy"); }
static i64 tv2_num_selected(id self, SEL c) { (void)c; return (i64)MSG0(u64, tv_selset(self), "count"); }
static BOOL tv2_row_selected(id self, SEL c, i64 r) { (void)c; return r >= 0 && MSG(BOOL, tv_selset(self), "containsIndex:", (u64)r); }
static BOOL tv2_multi(id self, SEL c) { (void)c; return tvp_i(self, "multi", 0) != 0; }
static void tv2_set_multi(id self, SEL c, BOOL b) { (void)c; tvp_set_i(self, "multi", b); }
static BOOL tv2_empty(id self, SEL c) { (void)c; return tvp_i(self, "allowsEmpty", 1) != 0; }
static void tv2_set_empty(id self, SEL c, BOOL b) { (void)c; tvp_set_i(self, "allowsEmpty", b); }
#define TVP_BOOL(get, set, key, def) \
    static BOOL tv2_g_##get(id self, SEL c) { (void)c; return tvp_i(self, key, def) != 0; } \
    static void tv2_s_##get(id self, SEL c, BOOL b) { (void)c; tvp_set_i(self, key, b); mark(self); }
TVP_BOOL(colsel, setColsel, "colSelection", 0) TVP_BOOL(reorder, setReorder, "colReorder", 1) TVP_BOOL(resize, setResize, "colResize", 1)
TVP_BOOL(typeSelect, setTypeSelect, "typeSelect", 1) TVP_BOOL(floats, setFloats, "floatsGroup", 1) TVP_BOOL(autosaveCols, setAutosaveCols, "autosaveCols", 0)
TVP_BOOL(vertMotion, setVertMotion, "vertMotion", 1) TVP_BOOL(altRows, setAltRows, "altRowsP", 0)
static i64 tv2_selected_col(id self, SEL c) { (void)c; return tvp_i(self, "selCol", -1); }
static void tv2_select_cols(id self, SEL c, id ixs, BOOL ext) {
    (void)c; (void)ext;
    if (!tvp_i(self, "colSelection", 0)) return;
    tvp_set_i(self, "selCol", ixs && MSG0(u64, ixs, "count") ? (i64)MSG0(u64, ixs, "firstIndex") : -1);
    mark(self);
    tv_post(self, "NSTableViewSelectionDidChangeNotification", sel_registerName("tableViewSelectionDidChange:"));
}
static id tv2_selected_cols(id self, SEL c) { (void)c; id s = new_indexset(); i64 k = tvp_i(self, "selCol", -1); if (k >= 0) MSG(void, s, "addIndex:", (u64)k); return s; }
static BOOL tv2_col_selected(id self, SEL c, i64 k) { (void)c; return k >= 0 && tvp_i(self, "selCol", -1) == k; }
static void tv2_deselect_col(id self, SEL c, i64 k) { (void)c; if (tvp_i(self, "selCol", -1) == k) { tvp_set_i(self, "selCol", -1); mark(self); } }

/* ---- geometria pública */
static double tv_col_x(id t, i64 k) { nsarr_t *cols = (nsarr_t *)vs(t)->cols; double x = 0; for (i64 i = 0; cols && i < k && i < (i64)cols->n; i++) x += cs_(cols->items[i])->width; return x; }
static NSRect tv2_rect_of_row(id self, SEL c, i64 r) {
    (void)c;
    vstate_t *s = vs(self);
    if (r < 0 || r >= tv_rows(self)) return r_make(0, 0, 0, 0);
    return r_make(0, tv_header_h(s) + tv_row_top(self, r), s->frame.s.w, tv_row_height(self, r));
}
static NSRect tv2_rect_of_col(id self, SEL c, i64 k) {
    (void)c;
    nsarr_t *cols = (nsarr_t *)vs(self)->cols;
    if (!cols || k < 0 || k >= (i64)cols->n) return r_make(0, 0, 0, 0);
    return r_make(tv_col_x(self, k), 0, cs_(cols->items[k])->width, tv_header_h(vs(self)) + tv_total_h(self));
}
static NSRect tv2_cell_frame(id self, SEL c, i64 k, i64 r) {
    (void)c;
    NSRect rr = tv2_rect_of_row(self, 0, r), cr = tv2_rect_of_col(self, 0, k);
    return r_make(cr.o.x, rr.o.y, cr.s.w, rr.s.h);
}
static i64 tv2_row_at_point(id self, SEL c, NSPoint p) { (void)c; return tv_row_at_y(self, p.y - tv_header_h(vs(self))); }
static i64 tv2_col_at_point(id self, SEL c, NSPoint p) {
    (void)c;
    nsarr_t *cols = (nsarr_t *)vs(self)->cols;
    double x = 0;
    for (u64 k = 0; cols && k < cols->n; k++) { double w = cs_(cols->items[k])->width; if (p.x >= x && p.x < x + w) return (i64)k; x += w; }
    return -1;
}
typedef struct { u64 loc, len; } tv_range_t;
static tv_range_t tv2_rows_in_rect(id self, SEL c, NSRect r) {
    (void)c;
    double h = tv_header_h(vs(self));
    i64 a = tv_row_at_y(self, r.o.y - h), b = tv_row_at_y(self, r.o.y + r.s.h - h - 0.001);
    i64 n = tv_rows(self);
    if (a < 0) a = r.o.y - h < 0 ? 0 : n;
    if (b < 0) b = n - 1;
    return b >= a ? (tv_range_t){(u64)a, (u64)(b - a + 1)} : (tv_range_t){0, 0};
}
static id tv2_cols_in_rect(id self, SEL c, NSRect r) {
    (void)c;
    id s = new_indexset();
    nsarr_t *cols = (nsarr_t *)vs(self)->cols;
    double x = 0;
    for (u64 k = 0; cols && k < cols->n; k++) { double w = cs_(cols->items[k])->width; if (x + w > r.o.x && x < r.o.x + r.s.w) MSG(void, s, "addIndex:", k); x += w; }
    return s;
}

/* ---- linhas: inserir, remover, mover, ocultar, altura */
static void tv_rows_changed(id t) { tv_heights_dirty(t); vs(t)->tv_dirty = 1; tv_reload(t, 0); }
static void tv2_insert_rows(id self, SEL c, id ixs, u64 anim) {
    (void)c; (void)anim;
    id sel = tv_selset(self), moved = new_indexset();
    for (u64 i = MSG0(u64, sel, "firstIndex"); i != (u64)0x7fffffffffffffffULL; i = MSG(u64, sel, "indexGreaterThanIndex:", i)) {
        u64 shift = MSG(u64, ixs, "countOfIndexesInRange:", (u64)0, i + 1);
        MSG(void, moved, "addIndex:", i + shift);
    }
    MSG0(void, sel, "removeAllIndexes"); MSG(void, sel, "addIndexes:", moved);
    tv_rows_changed(self);
}
static void tv2_remove_rows(id self, SEL c, id ixs, u64 anim) {
    (void)c; (void)anim;
    id sel = tv_selset(self), kept = new_indexset();
    for (u64 i = MSG0(u64, sel, "firstIndex"); i != (u64)0x7fffffffffffffffULL; i = MSG(u64, sel, "indexGreaterThanIndex:", i)) {
        if (MSG(BOOL, ixs, "containsIndex:", i)) continue;
        u64 shift = MSG(u64, ixs, "countOfIndexesInRange:", (u64)0, i);
        MSG(void, kept, "addIndex:", i - shift);
    }
    BOOL changed = MSG0(u64, kept, "count") != MSG0(u64, sel, "count");
    MSG0(void, sel, "removeAllIndexes"); MSG(void, sel, "addIndexes:", kept);
    vs(self)->selected = MSG0(u64, sel, "count") ? (i64)MSG0(u64, sel, "lastIndex") : -1;
    tv_rows_changed(self);
    if (changed) tv_post(self, "NSTableViewSelectionDidChangeNotification", sel_registerName("tableViewSelectionDidChange:"));
}
static void tv2_move_row(id self, SEL c, i64 from, i64 to) {
    (void)c;
    id sel = tv_selset(self);
    BOOL was = MSG(BOOL, sel, "containsIndex:", (u64)from);
    if (was) MSG(void, sel, "removeIndex:", (u64)from);
    MSG(void, sel, "shiftIndexesStartingAtIndex:by:", (u64)(from + 1), (i64)-1);
    MSG(void, sel, "shiftIndexesStartingAtIndex:by:", (u64)to, (i64)1);
    if (was) MSG(void, sel, "addIndex:", (u64)to);
    tv_rows_changed(self);
}
static void tv2_begin_updates(id self, SEL c) { (void)c; tvp_set_i(self, "updates", tvp_i(self, "updates", 0) + 1); }
static void tv2_end_updates(id self, SEL c) { (void)c; i64 n = tvp_i(self, "updates", 0) - 1; tvp_set_i(self, "updates", n < 0 ? 0 : n); if (n <= 0) tv_rows_changed(self); }
static void tv2_note_heights(id self, SEL c, id ixs) { (void)c; (void)ixs; tv_rows_changed(self); }
static id tv2_hidden(id self, SEL c) { (void)c; id h = objc_getAssociatedObject(self, &k_tv_hidden); return h ? MSG0(id, h, "copy") : new_indexset(); }
static void tv2_hide_rows(id self, SEL c, id ixs, u64 anim) {
    (void)c; (void)anim;
    id h = objc_getAssociatedObject(self, &k_tv_hidden);
    if (!h) { h = new_indexset(); objc_setAssociatedObject(self, &k_tv_hidden, h, 1); }
    MSG(void, h, "addIndexes:", ixs);
    id p = MSG0(id, tv_selset(self), "mutableCopy");
    MSG(void, p, "removeIndexes:", ixs);
    tv_apply_selection(self, p, YES);
    objc_release(p);
    tv_rows_changed(self);
}
static void tv2_unhide_rows(id self, SEL c, id ixs, u64 anim) { (void)c; (void)anim; id h = objc_getAssociatedObject(self, &k_tv_hidden); if (h) MSG(void, h, "removeIndexes:", ixs); tv_rows_changed(self); }

/* ---- colunas: ordenação, indicador, mover, largura */
static id tv2_sort(id self, SEL c) { (void)c; id s = objc_getAssociatedObject(self, &k_tv_sort); return s ? s : MSG0(id, (id)cx_class_named("NSArray"), "array"); }
static void tv2_set_sort(id self, SEL c, id descs) {
    (void)c;
    id old = objc_retain(tv2_sort(self, 0));
    objc_setAssociatedObject(self, &k_tv_sort, descs ? MSG0(id, descs, "copy") : 0, 1);
    if (descs) objc_release(objc_getAssociatedObject(self, &k_tv_sort));
    vstate_t *s = vs(self);
    if (!MSG(BOOL, old, "isEqual:", tv2_sort(self, 0))) {
        id ds = s->data_source;
        if (dresp(ds, "tableView:sortDescriptorsDidChange:")) MSG(void, ds, "tableView:sortDescriptorsDidChange:", self, old);
        else if (dresp(ds, "outlineView:sortDescriptorsDidChange:")) MSG(void, ds, "outlineView:sortDescriptorsDidChange:", self, old);
    }
    objc_release(old);
    mark(self);
}
static char k_col_props;
static id colp(id col) {
    id d = objc_getAssociatedObject(col, &k_col_props);
    if (!d) { d = MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary"); objc_setAssociatedObject(col, &k_col_props, d, 1); }
    return d;
}
static id colp_o(id col, const char *k) { return MSG(id, colp(col), "objectForKey:", cx_str(k)); }
static void colp_set(id col, const char *k, id v) { if (v) MSG(void, colp(col), "setObject:forKey:", v, cx_str(k)); else MSG(void, colp(col), "removeObjectForKey:", cx_str(k)); }
static double colp_d(id col, const char *k, double def) { id v = colp_o(col, k); return v ? MSG0(double, v, "doubleValue") : def; }
#define COLP_OBJ(get, set, key) \
    static id col2_g_##get(id self, SEL c) { (void)c; return colp_o(self, key); } \
    static void col2_s_##get(id self, SEL c, id v) { (void)c; colp_set(self, key, v); }
#define COLP_DBL(get, set, key, def) \
    static double col2_g_##get(id self, SEL c) { (void)c; return colp_d(self, key, def); } \
    static void col2_s_##get(id self, SEL c, double v) { (void)c; colp_set(self, key, cx_num_d(v)); }
#define COLP_INT(get, set, key, def) \
    static i64 col2_g_##get(id self, SEL c) { (void)c; id v = colp_o(self, key); return v ? MSG0(i64, v, "longLongValue") : def; } \
    static void col2_s_##get(id self, SEL c, i64 v) { (void)c; colp_set(self, key, cx_num_i(v)); }
COLP_OBJ(sortProto, setSortProto, "sortProto") COLP_OBJ(headerToolTip, setHeaderToolTip, "toolTip") COLP_OBJ(tableView, setTableView, "tableViewW")
COLP_DBL(minWidth, setMinWidth, "minWidth", 10) COLP_DBL(maxWidth, setMaxWidth, "maxWidth", 100000)
COLP_INT(resizingMask, setResizingMask, "resizingMask", 3) COLP_INT(editable, setEditable, "editable", 1) COLP_INT(hidden, setHidden, "hidden", 0)
static id col2_data_cell(id self, SEL c) {
    (void)c;
    id cell = colp_o(self, "dataCell");
    if (!cell) { cell = objc_autorelease(MSG0(id, MSG0(id, (id)cx_class_named("NSTextFieldCell"), "alloc"), "init")); colp_set(self, "dataCell", cell); }
    return cell;
}
static void col2_set_data_cell(id self, SEL c, id cell) { (void)c; colp_set(self, "dataCell", cell); colp_set(self, "dataCellSet", cell ? cx_num_i(1) : 0); }
static id col2_data_cell_for_row(id self, SEL c, i64 r) { (void)r; return col2_data_cell(self, c); }
static id col2_header_cell(id self, SEL c) {
    (void)c;
    id cell = colp_o(self, "headerCell");
    if (!cell) {
        cell = objc_autorelease(MSG0(id, MSG0(id, (id)cx_class_named("NSTableHeaderCell"), "alloc"), "init"));
        if (cs_(self)->title) MSG(void, cell, "setStringValue:", cs_(self)->title);
        colp_set(self, "headerCell", cell);
    }
    return cell;
}
static void col2_set_header_cell(id self, SEL c, id cell) { (void)c; colp_set(self, "headerCell", cell); }
static void col2_size_to_fit(id self, SEL c) {
    (void)c;
    const char *t = cs_(self)->title ? cx_cstr(cs_(self)->title) : "";
    gui_metrics_t m = measure(t, (u32)strlen(t), 11, 2);
    double w = m.width + 24, mn = colp_d(self, "minWidth", 10), mx = colp_d(self, "maxWidth", 100000);
    cs_(self)->width = w < mn ? mn : w > mx ? mx : w;
}
static id tv2_indicator(id self, SEL c, id col) { (void)c; id d = tvp_o(self, "indicators"); if (!d || !col) return 0; id key = MSG(id, (id)cx_class_named("NSValue"), "valueWithNonretainedObject:", col); return MSG(id, d, "objectForKey:", key); }
static void tv2_set_indicator(id self, SEL c, id img, id col) {
    (void)c;
    id d = tvp_o(self, "indicators");
    if (!d) { d = MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary"); tvp_set_o(self, "indicators", d); }
    id key = MSG(id, (id)cx_class_named("NSValue"), "valueWithNonretainedObject:", col);
    if (img) MSG(void, d, "setObject:forKey:", img, key); else MSG(void, d, "removeObjectForKey:", key);
    mark(self);
}
static id tv2_highlighted_col(id self, SEL c) { (void)c; return tvp_o(self, "hlCol"); }
static void tv2_set_highlighted_col(id self, SEL c, id col) { (void)c; tvp_set_o(self, "hlCol", col); mark(self); }
static void tv2_move_col(id self, SEL c, i64 from, i64 to) {
    (void)c;
    nsarr_t *cols = (nsarr_t *)vs(self)->cols;
    if (!cols || from < 0 || to < 0 || from >= (i64)cols->n || to >= (i64)cols->n || from == to) return;
    id col = cols->items[from];
    if (from < to) for (i64 i = from; i < to; i++) cols->items[i] = cols->items[i + 1];
    else for (i64 i = from; i > to; i--) cols->items[i] = cols->items[i - 1];
    cols->items[to] = col;
    vs(self)->tv_dirty = 1;
    mark(self);
    id info = MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary");
    MSG(void, info, "setObject:forKey:", cx_num_i(from), cx_str("NSOldColumn"));
    MSG(void, info, "setObject:forKey:", cx_num_i(to), cx_str("NSNewColumn"));
    id note = objc_autorelease(cx_note(cx_str("NSTableViewColumnDidMoveNotification"), self, info));
    if (dresp(vs(self)->delegate, "tableViewColumnDidMove:")) MSG(void, vs(self)->delegate, "tableViewColumnDidMove:", note);
    cx_post(note);
}
static void tv_col_resized(id t, id col, double old) {
    id info = MSG(id, (id)cx_class_named("NSDictionary"), "dictionaryWithObject:forKey:", col, cx_str("NSTableColumn"));
    (void)old;
    id note = objc_autorelease(cx_note(cx_str("NSTableViewColumnDidResizeNotification"), t, info));
    if (dresp(vs(t)->delegate, "tableViewColumnDidResize:")) MSG(void, vs(t)->delegate, "tableViewColumnDidResize:", note);
    cx_post(note);
    vs(t)->tv_dirty = 1;
    mark(t);
}
TVP_BOOL(usesAutomaticRowHeights, setUsesAutomaticRowHeights, "autoHeights", 0)
static NSSize tv2_spacing(id self, SEL c) { (void)c; return (NSSize){(double)tvp_i(self, "spX", 3), (double)tvp_i(self, "spY", 2)}; }
static void tv2_set_spacing(id self, SEL c, NSSize z) { (void)c; tvp_set_i(self, "spX", (i64)z.w); tvp_set_i(self, "spY", (i64)z.h); mark(self); }
static id tv2_grid_color(id self, SEL c) { (void)c; id v = tvp_o(self, "gridColor"); return v ? v : cx_color(0.85, 0.85, 0.87, 1); }
static void tv2_set_grid_color(id self, SEL c, id col) { (void)c; tvp_set_o(self, "gridColor", col); mark(self); }
static u64 tv2_grid_mask(id self, SEL c) { (void)c; return (u64)tvp_i(self, "gridMask", 0); }
static void tv2_set_grid_mask(id self, SEL c, u64 m) { (void)c; tvp_set_i(self, "gridMask", (i64)m); mark(self); }
static id tv2_bg(id self, SEL c) { (void)c; id v = tvp_o(self, "bgColor"); return v ? v : cx_color(1, 1, 1, 1); }
static void tv2_set_bg(id self, SEL c, id col) { (void)c; tvp_set_o(self, "bgColor", col); mark(self); }
static id tv2_autosave(id self, SEL c) { (void)c; return tvp_o(self, "autosaveName"); }
static void tv2_set_autosave(id self, SEL c, id n) { (void)c; tvp_set_o(self, "autosaveName", n); }
static SEL tv2_double_action(id self, SEL c) { (void)c; return vs(self)->double_action; }
static i64 tv2_edited_row(id self, SEL c) { (void)c; return tvp_i(self, "editRow", -1); }
static i64 tv2_edited_col(id self, SEL c) { (void)c; return tvp_i(self, "editCol", -1); }
static BOOL tv2_alt(id self, SEL c) { (void)c; return vs(self)->alt_rows; }
static double tv2_row_height_get(id self, SEL c) { (void)c; return tv_base_h(vs(self)); }
static void tv2_set_row_height(id self, SEL c, double h) { (void)c; vs(self)->row_h = h > 0 ? h : 24; tv_rows_changed(self); }
static void tv2_reload_rows(id self, SEL c, id rows, id cols) { (void)c; (void)rows; (void)cols; tv_rows_changed(self); }
static void tv2_note_rows(id self, SEL c) { (void)c; tv_rows_changed(self); }
static void tv2_reload(id self, SEL c) {
    (void)c;
    tv_heights_dirty(self);
    i64 n = tv_rows(self);
    id sel = tv_selset(self);
    if (n >= 0 && MSG0(u64, sel, "count") && (i64)MSG0(u64, sel, "lastIndex") >= n) MSG(void, sel, "removeIndexesInRange:", (u64)n, (u64)0x7fffffffffffffffULL - (u64)n);
    vs(self)->selected = MSG0(u64, sel, "count") ? (i64)MSG0(u64, sel, "lastIndex") : -1;
    tv_reload(self, 0);
}
static void tv2_enum_row_views(id self, SEL c, void *block) {
    (void)c;
    /* sem NSTableRowView próprias: cada linha visível vem como a primeira view de célula dela */
    cx_tv_engine_t *e = cx_tv_engine_get(self, 0);
    if (!e || !block) return;
    void (*inv)(void *, id, i64) = *(void (**)(void *, id, i64))((u8 *)block + 16);
    int n = cx_tv_engine_placed_count(e);
    for (int i = 0; i < n; i++) { int r, k; id v = (id)cx_tv_engine_placed_at(e, i); if (v && cx_tv_engine_placed_find(e, v, &r, &k) >= 0 && k <= 0) inv(block, v, r); }
}

/* ---- arrastar e soltar linhas */
static void tv2_set_drag_mask(id self, SEL c, u64 mask, BOOL local) { (void)c; tvp_set_i(self, local ? "dragLocal" : "dragRemote", (i64)mask); }
static u64 tv2_drag_mask(id self, SEL c, id session, i64 ctx) { (void)c; (void)session; return (u64)tvp_i(self, ctx == 1 ? "dragLocal" : "dragRemote", ctx == 1 ? -1 : 1); }
static void tv2_set_drop(id self, SEL c, i64 row, u64 op) { (void)c; tvp_set_i(self, "dropRow", row); tvp_set_i(self, "dropOp", (i64)op); mark(self); }
static i64 tv2_feedback(id self, SEL c) { (void)c; return tvp_i(self, "feedback", 0); }
static void tv2_set_feedback(id self, SEL c, i64 v) { (void)c; tvp_set_i(self, "feedback", v); }
static NSPoint tv_local_pt(id t, id info) {
    NSPoint w = MSG0(NSPoint, info, "draggingLocation");
    return MSG(NSPoint, t, "convertPoint:fromView:", w, (id)0);
}
/* linha e operação propostas: acima (entre linhas) perto da borda, sobre a linha no meio */
static void tv_drop_target(id t, id info, i64 *row, u64 *op) {
    NSPoint p = tv_local_pt(t, info);
    double y = p.y - tv_header_h(vs(t));
    i64 n = tv_rows(t), r = tv_row_at_y(t, y);
    if (r < 0) { *row = y < 0 ? 0 : n; *op = 1; return; }   /* NSTableViewDropAbove */
    double top = tv_row_top(t, r), h = tv_row_height(t, r);
    if (y - top < h * 0.25) { *row = r; *op = 1; }
    else if (y - top > h * 0.75) { *row = r + 1; *op = 1; }
    else { *row = r; *op = 0; }                                /* NSTableViewDropOn */
}
static u64 tv_validate(id t, id info) {
    id ds = vs(t)->data_source;
    i64 row; u64 op;
    tv_drop_target(t, info, &row, &op);
    tvp_set_i(t, "dropRow", row); tvp_set_i(t, "dropOp", (i64)op);
    u64 res = 0;
    if (dresp(ds, "tableView:validateDrop:proposedRow:proposedDropOperation:"))
        res = MSG(u64, ds, "tableView:validateDrop:proposedRow:proposedDropOperation:", t, info, row, op);
    if (!res) tvp_set_i(t, "dropRow", -2);
    mark(t);
    return res;
}
static u64 tv2_dragging_entered(id self, SEL c, id info) { (void)c; return tv_validate(self, info); }
static u64 tv2_dragging_updated(id self, SEL c, id info) { (void)c; return tv_validate(self, info); }
static void tv2_dragging_exited(id self, SEL c, id info) { (void)c; (void)info; tvp_set_i(self, "dropRow", -2); mark(self); }
static BOOL tv2_prepare_drop(id self, SEL c, id info) { (void)c; (void)info; return tvp_i(self, "dropRow", -2) != -2; }
static BOOL tv2_perform_drop(id self, SEL c, id info) {
    (void)c;
    id ds = vs(self)->data_source;
    i64 row = tvp_i(self, "dropRow", -2);
    u64 op = (u64)tvp_i(self, "dropOp", 1);
    tvp_set_i(self, "dropRow", -2);
    mark(self);
    if (row == -2 || !dresp(ds, "tableView:acceptDrop:row:dropOperation:")) return NO;
    return MSG(BOOL, ds, "tableView:acceptDrop:row:dropOperation:", self, info, row, op);
}
static void tv2_conclude(id self, SEL c, id info) { (void)c; (void)info; tvp_set_i(self, "dropRow", -2); mark(self); }
/* começa a arrastar as linhas selecionadas (do mouseDown, quando o mouse anda com o botão apertado) */
static int tv_begin_row_drag(id t, id ev) {
    id ds = vs(t)->data_source;
    id rows = tv_selset(t);
    if (!MSG0(u64, rows, "count")) return 0;
    id items = MSG0(id, (id)cx_class_named("NSMutableArray"), "array");
    if (dresp(ds, "tableView:pasteboardWriterForRow:")) {
        for (u64 i = MSG0(u64, rows, "firstIndex"); i != (u64)0x7fffffffffffffffULL; i = MSG(u64, rows, "indexGreaterThanIndex:", i)) {
            id w = MSG(id, ds, "tableView:pasteboardWriterForRow:", t, (i64)i);
            if (!w) continue;
            id it = MSG(id, MSG0(id, (id)cx_class_named("NSDraggingItem"), "alloc"), "initWithPasteboardWriter:", w);
            NSRect fr = tv2_rect_of_row(t, 0, (i64)i);
            MSG(void, it, "setDraggingFrame:contents:", fr, (id)0);
            MSG(void, items, "addObject:", it);
            objc_release(it);
        }
    } else if (dresp(ds, "tableView:writeRowsWithIndexes:toPasteboard:")) {
        id pb = MSG(id, (id)cx_class_named("NSPasteboard"), "pasteboardWithName:", cx_str("Apple CFPasteboard drag"));
        MSG(i64, pb, "clearContents");
        if (!MSG(BOOL, ds, "tableView:writeRowsWithIndexes:toPasteboard:", t, rows, pb)) return 0;
        id item = MSG0(id, (id)cx_class_named("NSPasteboardItem"), "new");
        id types = MSG0(id, pb, "types");
        for (u64 k = 0, n = types ? MSG0(u64, types, "count") : 0; k < n; k++) {
            id ty = MSG(id, types, "objectAtIndex:", k);
            id d = MSG(id, pb, "dataForType:", ty);
            if (d) MSG(BOOL, item, "setData:forType:", d, ty);
        }
        id it = MSG(id, MSG0(id, (id)cx_class_named("NSDraggingItem"), "alloc"), "initWithPasteboardWriter:", item);
        NSRect fr = tv2_rect_of_row(t, 0, (i64)MSG0(u64, rows, "firstIndex"));
        MSG(void, it, "setDraggingFrame:contents:", fr, (id)0);
        MSG(void, items, "addObject:", it);
        objc_release(it); objc_release(item);
    } else return 0;
    if (!MSG0(u64, items, "count")) return 0;
    if (dresp(ds, "tableView:draggingSession:willBeginAtPoint:forRowIndexes:")) {
        NSPoint w = MSG0(NSPoint, ev, "locationInWindow");
        MSG(void, ds, "tableView:draggingSession:willBeginAtPoint:forRowIndexes:", t, (id)0, w, rows);
    }
    MSG(id, t, "beginDraggingSessionWithItems:event:source:", items, ev, t);
    return 1;
}

/* ---- busca por digitação */
static id tv_row_text(id t, i64 r) {
    vstate_t *s = vs(t);
    nsarr_t *cols = (nsarr_t *)s->cols;
    id col = cols && cols->n ? cols->items[0] : 0;
    if (dresp(s->delegate, "tableView:typeSelectStringForTableColumn:row:")) return MSG(id, s->delegate, "tableView:typeSelectStringForTableColumn:row:", t, col, r);
    return cell_text(t, col, r);
}
static void tv_type_select(id t, id chars) {
    double now = cx_now();
    id buf = tvp_o(t, "typeBuf");
    if (!buf || now - (double)tvp_i(t, "typeT", 0) / 1000.0 > 1.0) buf = cx_str("");
    buf = MSG(id, buf, "stringByAppendingString:", chars);
    tvp_set_o(t, "typeBuf", buf);
    tvp_set_i(t, "typeT", (i64)(now * 1000));
    i64 n = tv_rows(t);
    for (i64 r = 0; r < n; r++) {
        id txt = tv_row_text(t, r);
        if (txt && MSG(BOOL, MSG0(id, txt, "lowercaseString"), "hasPrefix:", MSG0(id, buf, "lowercaseString"))) {
            id one = MSG(id, (id)cx_class_named("NSIndexSet"), "indexSetWithIndex:", (u64)r);
            select_rows_set(t, one, NO, YES);
            tv_scroll_row(t, 0, r);
            return;
        }
    }
}

/* ---- teclado */
static void tv2_key_down(id self, SEL c, id ev) {
    (void)c;
    id ch = MSG0(id, ev, "charactersIgnoringModifiers");
    unsigned short u = ch && MSG0(u64, ch, "length") ? MSG(unsigned short, ch, "characterAtIndex:", (u64)0) : 0;
    u64 mods = MSG0(u64, ev, "modifierFlags");
    BOOL shift = (mods & (1u << 17)) != 0, cmd = (mods & (1u << 20)) != 0;
    i64 n = tv_rows(self), cur = vs(self)->selected;
    if (n <= 0) { NSBeep(); return; }
    if (cmd && (u == 'a' || u == 'A')) { tv2_select_all(self, 0, 0); return; }
    i64 target = -2;
    double view_h = tv_in_scroll(self) ? vs(vs(self)->superview)->frame.s.h : vs(self)->frame.s.h;
    i64 page = (i64)(view_h / tv_base_h(vs(self)));
    if (page < 1) page = 1;
    switch (u) {
    case 0xF700: target = cur < 0 ? n - 1 : cur > 0 ? cur - 1 : 0; break;            /* ↑ */
    case 0xF701: target = cur < 0 ? 0 : cur + 1 < n ? cur + 1 : n - 1; break;        /* ↓ */
    case 0xF729: target = 0; break;                                                  /* Home */
    case 0xF72B: target = n - 1; break;                                              /* End */
    case 0xF72C: target = cur - page < 0 ? 0 : cur - page; break;                    /* PageUp */
    case 0xF72D: target = cur + page >= n ? n - 1 : cur + page; break;               /* PageDown */
    }
    if (target == -2) {
        if (u >= 0x20 && u < 0xF700 && !cmd && tvp_i(self, "typeSelect", 1)) { tv_type_select(self, ch); return; }
        id nr = MSG0(id, self, "nextResponder");
        if (nr) MSG(void, nr, "keyDown:", ev); else NSBeep();
        return;
    }
    id want = new_indexset();
    if (shift && tvp_i(self, "multi", 0)) {
        i64 anchor = tvp_i(self, "anchor", cur < 0 ? target : cur);
        i64 lo = anchor < target ? anchor : target, hi = anchor < target ? target : anchor;
        MSG(void, want, "addIndexesInRange:", (u64)lo, (u64)(hi - lo + 1));
        tvp_set_i(self, "anchor", anchor);
    } else { MSG(void, want, "addIndex:", (u64)target); tvp_set_i(self, "anchor", target); }
    tv_apply_selection(self, want, YES);
    vs(self)->selected = target;
    tv_scroll_row(self, 0, target);
}

/* ---- cabeçalho: ordenar, redimensionar e mover colunas */
static void tv_header_click(id t, id col, i64 k) {
    vstate_t *s = vs(t);
    if (dresp(s->delegate, "tableView:mouseDownInHeaderOfTableColumn:")) MSG(void, s->delegate, "tableView:mouseDownInHeaderOfTableColumn:", t, col);
    id proto = colp_o(col, "sortProto");
    if (proto) {
        id cur = tv2_sort(t, 0);
        id first = MSG0(u64, cur, "count") ? MSG(id, cur, "objectAtIndex:", (u64)0) : 0;
        id nd = proto;
        if (first && MSG(BOOL, MSG0(id, first, "key"), "isEqual:", MSG0(id, proto, "key"))) nd = MSG0(id, first, "reversedSortDescriptor");
        id list = MSG(id, (id)cx_class_named("NSMutableArray"), "arrayWithObject:", nd);
        for (u64 i = 0, n = MSG0(u64, cur, "count"); i < n && i < 4; i++) {
            id d = MSG(id, cur, "objectAtIndex:", i);
            if (!MSG(BOOL, MSG0(id, d, "key"), "isEqual:", MSG0(id, nd, "key"))) MSG(void, list, "addObject:", d);
        }
        tv2_set_sort(t, 0, list);
        tvp_set_o(t, "hlCol", col);
    }
    if (tvp_i(t, "colSelection", 0)) { id one = MSG(id, (id)cx_class_named("NSIndexSet"), "indexSetWithIndex:", (u64)k); tv2_select_cols(t, 0, one, NO); }
    if (dresp(s->delegate, "tableView:didClickTableColumn:")) MSG(void, s->delegate, "tableView:didClickTableColumn:", t, col);
}
/* clique no cabeçalho em x (local): perto de uma divisa redimensiona, senão arrasta para mover ou clica */
static void tv_header_mouse(id t, id ev, double x) {
    vstate_t *s = vs(t);
    nsarr_t *cols = (nsarr_t *)s->cols;
    if (!cols || !cols->n) return;
    double cx = 0;
    i64 hit_k = -1, edge_k = -1;
    for (u64 k = 0; k < cols->n; k++) {
        double w = cs_(cols->items[k])->width;
        if (tv_abs(x - (cx + w)) <= 3 && (colp_o(cols->items[k], "resizingMask") ? MSG0(i64, colp_o(cols->items[k], "resizingMask"), "longLongValue") & 2 : 1)
            && tvp_i(t, "colResize", 1)) { edge_k = (i64)k; break; }
        if (x >= cx && x < cx + w) hit_k = (i64)k;
        cx += w;
    }
    id until = MSG0(id, (id)cx_class_named("NSDate"), "distantFuture");
    u64 mask = (1ULL << 6) | (1ULL << 2);
    double x0 = MSG0(NSPoint, ev, "locationInWindow").x;
    if (edge_k >= 0) {                                    /* redimensionar */
        id col = cols->items[edge_k];
        double w0 = cs_(col)->width, mn = colp_d(col, "minWidth", 10), mx = colp_d(col, "maxWidth", 100000);
        for (;;) {
            id e = ((id (*)(id, SEL, u64, id, id, BOOL))objc_msgSend)(NSApp, sel_registerName("nextEventMatchingMask:untilDate:inMode:dequeue:"), mask, until, cx_str("NSEventTrackingRunLoopMode"), (BOOL)YES);
            if (!e) break;
            double w = w0 + MSG0(NSPoint, e, "locationInWindow").x - x0;
            cs_(col)->width = w < mn ? mn : w > mx ? mx : w;
            s->tv_dirty = 1;
            mark(t);
            if (MSG0(u64, e, "type") == 2) break;
        }
        if (cs_(col)->width != w0) tv_col_resized(t, col, w0);
        return;
    }
    if (hit_k < 0) return;
    id col = cols->items[hit_k];
    i64 cur_k = hit_k;
    int moved = 0;
    for (;;) {                                            /* soltar = clique; arrastar = mover a coluna */
        id e = ((id (*)(id, SEL, u64, id, id, BOOL))objc_msgSend)(NSApp, sel_registerName("nextEventMatchingMask:untilDate:inMode:dequeue:"), mask, until, cx_str("NSEventTrackingRunLoopMode"), (BOOL)YES);
        if (!e) break;
        double dx = MSG0(NSPoint, e, "locationInWindow").x - x0;
        if (MSG0(u64, e, "type") == 2) break;
        if (tv_abs(dx) < 4 || !tvp_i(t, "colReorder", 1)) continue;
        moved = 1;
        NSPoint lw = MSG0(NSPoint, e, "locationInWindow");
        NSPoint lp = MSG(NSPoint, t, "convertPoint:fromView:", lw, (id)0);
        i64 over = tv2_col_at_point(t, 0, lp);
        if (over >= 0 && over != cur_k) {
            if (dresp(s->delegate, "tableView:shouldReorderColumn:toColumn:") && !MSG(BOOL, s->delegate, "tableView:shouldReorderColumn:toColumn:", t, cur_k, over)) continue;
            tv2_move_col(t, 0, cur_k, over);
            cur_k = over;
        }
    }
    if (moved) { if (dresp(s->delegate, "tableView:didDragTableColumn:")) MSG(void, s->delegate, "tableView:didDragTableColumn:", t, col); }
    else tv_header_click(t, col, hit_k);
}

/* ---- clique nas linhas: seleção, arrasto, duplo clique */
static void tv2_mouse_down(id self, SEL c, id e) {
    vstate_t *s = vs(self);
    absr_t a;
    if (!abs_rect(self, &a)) return;
    double wy = win_h(s->window) - evs(e)->y;
    double hy = a.tly;
    absr_t sa;
    int in_sc = tv_in_scroll(self);
    if (s->header && in_sc && abs_rect(s->superview, &sa) && sa.tly > hy) hy = sa.tly;
    if (s->header && wy - hy >= 0 && wy - hy < 24) { tv_header_mouse(self, e, evs(e)->x - a.tlx); return; }
    double py = wy - a.tly - tv_header_h(s);
    if (!in_sc) { cx_tv_engine_t *eng = cx_tv_engine_get(self, 0); if (eng) py += cx_tv_engine_own_scroll(eng); }
    i64 r = tv_row_at_y(self, py);
    /* triângulo da outline: o código base abre/fecha e mantém o clicked */
    if (r >= 0 && tv_is_outline(self)) {
        double lx = evs(e)->x - a.tlx;
        int level; double per; id ocol;
        tv_outline_metrics(self, r, &level, &per, &ocol);
        double dx = cx_tv_disclosure_x(0, level, per, 1);
        if (lx >= dx && lx < dx + per) { tv_mouse_down(self, c, e); return; }
    }
    s->clicked = r;
    tvp_set_i(self, "clickedCol", tv2_col_at_point(self, 0, (NSPoint){evs(e)->x - a.tlx, 0}));
    u64 mods = evs(e)->mods;
    BOOL shift = (mods & (1u << 17)) != 0, cmd = (mods & (1u << 20)) != 0;
    BOOL was_selected = r >= 0 && MSG(BOOL, tv_selset(self), "containsIndex:", (u64)r);
    if (r < 0) { if (tvp_i(self, "allowsEmpty", 1)) tv2_deselect_all(self, 0, 0); }
    else if (shift && tvp_i(self, "multi", 0)) {
        i64 anchor = tvp_i(self, "anchor", s->selected >= 0 ? s->selected : r);
        id want = new_indexset();
        i64 lo = anchor < r ? anchor : r, hi = anchor < r ? r : anchor;
        MSG(void, want, "addIndexesInRange:", (u64)lo, (u64)(hi - lo + 1));
        tv_apply_selection(self, want, YES);
    } else if (cmd && tvp_i(self, "multi", 0)) {
        id want = MSG0(id, tv_selset(self), "mutableCopy");
        if (was_selected) MSG(void, want, "removeIndex:", (u64)r); else MSG(void, want, "addIndex:", (u64)r);
        tv_apply_selection(self, want, YES);
        objc_release(want);
        tvp_set_i(self, "anchor", r);
    } else if (!was_selected || evs(e)->clicks >= 2) {
        id one = MSG(id, (id)cx_class_named("NSIndexSet"), "indexSetWithIndex:", (u64)r);
        tv_apply_selection(self, one, YES);
        tvp_set_i(self, "anchor", r);
    }
    /* arrasto das linhas: espera o mouse andar ou soltar */
    if (r >= 0 && evs(e)->clicks < 2) {
        id until = MSG0(id, (id)cx_class_named("NSDate"), "distantFuture");
        u64 mask = (1ULL << 6) | (1ULL << 2);
        NSPoint p0 = MSG0(NSPoint, e, "locationInWindow");
        for (;;) {
            id ne = ((id (*)(id, SEL, u64, id, id, BOOL))objc_msgSend)(NSApp, sel_registerName("nextEventMatchingMask:untilDate:inMode:dequeue:"), mask, until, cx_str("NSEventTrackingRunLoopMode"), (BOOL)YES);
            if (!ne) break;
            if (MSG0(u64, ne, "type") == 2) {
                /* clique simples numa linha já selecionada (de várias): fica só ela */
                if (was_selected && !shift && !cmd) { id one = MSG(id, (id)cx_class_named("NSIndexSet"), "indexSetWithIndex:", (u64)r); tv_apply_selection(self, one, YES); tvp_set_i(self, "anchor", r); }
                break;
            }
            NSPoint p = MSG0(NSPoint, ne, "locationInWindow");
            if (tv_abs(p.x - p0.x) + tv_abs(p.y - p0.y) > 4) {
                if (tv_begin_row_drag(self, e)) return;
                if (tvp_i(self, "multi", 0) && !tvp_i(self, "vertMotion", 1)) continue;
                /* sem arrasto: arrastar estende a seleção (como no AppKit) */
                NSPoint lp = MSG(NSPoint, self, "convertPoint:fromView:", p, (id)0);
                i64 rr = tv_row_at_y(self, lp.y - tv_header_h(s));
                if (rr >= 0 && tvp_i(self, "multi", 0)) {
                    id want = new_indexset();
                    i64 lo = r < rr ? r : rr, hi = r < rr ? rr : r;
                    MSG(void, want, "addIndexesInRange:", (u64)lo, (u64)(hi - lo + 1));
                    tv_apply_selection(self, want, YES);
                }
            }
        }
    }
    if (evs(e)->clicks >= 2 && s->double_action && r >= 0) {
        id t = s->target ? s->target : (s->delegate ? s->delegate : app.delegate);
        if (t) ((void (*)(id, SEL, id))objc_msgSend)(t, s->double_action, self);
    } else if (s->action) send_action(self);
}
static i64 tv2_clicked_col(id self, SEL c) { (void)c; return tvp_i(self, "clickedCol", -1); }

static void cx_table2_register(void) {
    cx_class_t *k = &NSTableView_c;
#define T(sel, f) cx_add_method(k, sel, (void *)(f))
    T("selectRowIndexes:byExtendingSelection:", tv2_select); T("deselectAll:", tv2_deselect_all); T("deselectRow:", tv2_deselect_row);
    T("selectAll:", tv2_select_all); T("selectedRowIndexes", tv2_sel_indexes); T("numberOfSelectedRows", tv2_num_selected);
    T("isRowSelected:", tv2_row_selected); T("allowsMultipleSelection", tv2_multi); T("setAllowsMultipleSelection:", tv2_set_multi);
    T("allowsEmptySelection", tv2_empty); T("setAllowsEmptySelection:", tv2_set_empty);
    T("allowsColumnSelection", tv2_g_colsel); T("setAllowsColumnSelection:", tv2_s_colsel);
    T("allowsColumnReordering", tv2_g_reorder); T("setAllowsColumnReordering:", tv2_s_reorder);
    T("allowsColumnResizing", tv2_g_resize); T("setAllowsColumnResizing:", tv2_s_resize);
    T("allowsTypeSelect", tv2_g_typeSelect); T("setAllowsTypeSelect:", tv2_s_typeSelect);
    T("floatsGroupRows", tv2_g_floats); T("setFloatsGroupRows:", tv2_s_floats);
    T("autosaveTableColumns", tv2_g_autosaveCols); T("setAutosaveTableColumns:", tv2_s_autosaveCols);
    T("verticalMotionCanBeginDrag", tv2_g_vertMotion); T("setVerticalMotionCanBeginDrag:", tv2_s_vertMotion);
    T("usesAutomaticRowHeights", tv2_g_usesAutomaticRowHeights); T("setUsesAutomaticRowHeights:", tv2_s_usesAutomaticRowHeights);
    T("selectedColumn", tv2_selected_col); T("selectColumnIndexes:byExtendingSelection:", tv2_select_cols); T("selectedColumnIndexes", tv2_selected_cols);
    T("isColumnSelected:", tv2_col_selected); T("deselectColumn:", tv2_deselect_col); T("numberOfSelectedColumns", tv2_num_selected);
    T("rectOfRow:", tv2_rect_of_row); T("rectOfColumn:", tv2_rect_of_col); T("frameOfCellAtColumn:row:", tv2_cell_frame);
    T("rowAtPoint:", tv2_row_at_point); T("columnAtPoint:", tv2_col_at_point); T("rowsInRect:", tv2_rows_in_rect); T("columnIndexesInRect:", tv2_cols_in_rect);
    T("insertRowsAtIndexes:withAnimation:", tv2_insert_rows); T("removeRowsAtIndexes:withAnimation:", tv2_remove_rows); T("moveRowAtIndex:toIndex:", tv2_move_row);
    T("beginUpdates", tv2_begin_updates); T("endUpdates", tv2_end_updates); T("noteHeightOfRowsWithIndexesChanged:", tv2_note_heights);
    T("hiddenRowIndexes", tv2_hidden); T("hideRowsAtIndexes:withAnimation:", tv2_hide_rows); T("unhideRowsAtIndexes:withAnimation:", tv2_unhide_rows);
    T("sortDescriptors", tv2_sort); T("setSortDescriptors:", tv2_set_sort);
    T("indicatorImageInTableColumn:", tv2_indicator); T("setIndicatorImage:inTableColumn:", tv2_set_indicator);
    T("highlightedTableColumn", tv2_highlighted_col); T("setHighlightedTableColumn:", tv2_set_highlighted_col); T("moveColumn:toColumn:", tv2_move_col);
    T("intercellSpacing", tv2_spacing); T("setIntercellSpacing:", tv2_set_spacing); T("gridColor", tv2_grid_color); T("setGridColor:", tv2_set_grid_color);
    T("gridStyleMask", tv2_grid_mask); T("setGridStyleMask:", tv2_set_grid_mask); T("backgroundColor", tv2_bg); T("setBackgroundColor:", tv2_set_bg);
    T("autosaveName", tv2_autosave); T("setAutosaveName:", tv2_set_autosave); T("doubleAction", tv2_double_action);
    T("editedRow", tv2_edited_row); T("editedColumn", tv2_edited_col); T("usesAlternatingRowBackgroundColors", tv2_alt);
    T("rowHeight", tv2_row_height_get); T("setRowHeight:", tv2_set_row_height); T("reloadData", tv2_reload);
    T("reloadDataForRowIndexes:columnIndexes:", tv2_reload_rows); T("noteNumberOfRowsChanged", tv2_note_rows);
    T("enumerateAvailableRowViewsUsingBlock:", tv2_enum_row_views); T("clickedColumn", tv2_clicked_col);
    T("setDraggingSourceOperationMask:forLocal:", tv2_set_drag_mask); T("draggingSession:sourceOperationMaskForDraggingContext:", tv2_drag_mask);
    T("setDropRow:dropOperation:", tv2_set_drop); T("draggingDestinationFeedbackStyle", tv2_feedback); T("setDraggingDestinationFeedbackStyle:", tv2_set_feedback);
    T("draggingEntered:", tv2_dragging_entered); T("draggingUpdated:", tv2_dragging_updated); T("draggingExited:", tv2_dragging_exited);
    T("prepareForDragOperation:", tv2_prepare_drop); T("performDragOperation:", tv2_perform_drop); T("concludeDragOperation:", tv2_conclude);
    T("keyDown:", tv2_key_down); T("mouseDown:", tv2_mouse_down); T("acceptsFirstResponder", yes_m);
#undef T
    cx_class_t *col = &NSTableColumn_c;
#define C(sel, f) cx_add_method(col, sel, (void *)(f))
    C("sortDescriptorPrototype", col2_g_sortProto); C("setSortDescriptorPrototype:", col2_s_sortProto);
    C("headerToolTip", col2_g_headerToolTip); C("setHeaderToolTip:", col2_s_headerToolTip);
    C("minWidth", col2_g_minWidth); C("setMinWidth:", col2_s_minWidth); C("maxWidth", col2_g_maxWidth); C("setMaxWidth:", col2_s_maxWidth);
    C("resizingMask", col2_g_resizingMask); C("setResizingMask:", col2_s_resizingMask);
    C("isEditable", col2_g_editable); C("setEditable:", col2_s_editable); C("isHidden", col2_g_hidden); C("setHidden:", col2_s_hidden);
    C("dataCell", col2_data_cell); C("setDataCell:", col2_set_data_cell); C("dataCellForRow:", col2_data_cell_for_row);
    C("headerCell", col2_header_cell); C("setHeaderCell:", col2_set_header_cell); C("sizeToFit", col2_size_to_fit);
    C("tableView", col2_g_tableView); C("setTableView:", col2_s_tableView);
#undef C
}
