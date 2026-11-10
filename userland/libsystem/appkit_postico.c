/*
 * mOS libSystem — classes do AppKit que o Postico 2 usa e o libSystem ainda não tinha (docs/swift-objc-runtime.md, fase 7):
 * as que o binário referencia (OBJC_CLASS_$_...) e as que os nibs dele instanciam pelo nome. Sem a classe, uma subclasse
 * Swift do app (FavoritesOutlineView: NSOutlineView, PCOToolbarItem: NSToolbarItem...) ficava pendurada no NSObject.
 *  - NSToolbarItem: identificador, rótulos, imagem, view, alvo/ação, minSize/maxSize. A NSToolbar pega os itens
 *    padrão do delegate, desenha numa view hospedeira (_MOSToolbarView) que o appkit.c põe no topo da janela, com
 *    as views dos itens como subviews, e encaminha o clique para target/action (appkit_table.c: cx_tb_layout / cx_tb_click).
 *  - NSSplitView: dispõe as subviews pela holdingPriority, arrasta a divisória presa aos limites do delegate,
 *    colapsa (canCollapseSubview:, duplo clique) e desenha as divisórias (appkit_table.c: cx_sv_*).
 *  - NSOutlineView: NSTableView com um adaptador próprio como fonte de dados e delegate da tabela: achata a árvore
 *    (itens expandidos) em linhas e traduz numberOfRows/objectValue/viewFor... e a seleção para a API de outline.
 *    A indentação, o triângulo e o scroll das views ficam na NSTableView (appkit.c).
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
#include "appkit_table.h"
id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);

typedef struct { double x, y, w, h; } pr_rect_t;
void objc_msgSend_stret(void);   /* NSRect (32 bytes) volta por ponteiro oculto: MSG0 leria o isa do buffer */
#define MSGR(T, obj, sel) ((T (*)(id, SEL))objc_msgSend_stret)((id)(obj), sel_registerName(sel))
typedef struct { double x, y; } pr_pt;
typedef struct { double w, h; } pr_size_t;

#define K(N) cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N); cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N);
K(NSToolbarItem) K(NSSplitView) K(NSOutlineView) K(NSTableRowView) K(NSTableHeaderView) K(NSTableHeaderCell)
K(NSTextFieldCell) K(NSSecureTextFieldCell) K(NSSearchFieldCell) K(NSTokenFieldCell) K(NSSegmentedCell) K(NSImageCell)
K(NSSearchField) K(NSTokenField) K(NSTabView) K(NSTabViewItem) K(NSTabViewController) K(NSPageController)
K(NSGridView) K(NSGridCell) K(NSGridRow) K(NSGridColumn) K(NSDocument) K(NSGradient) K(NSColorPanel) K(NSRulerView)
K(NSRulerMarker) K(NSTextTable) K(NSTextTableBlock) K(NSTextAttachmentCell) K(NSDraggingImageComponent)
K(NSFilePromiseProvider) K(NSFilePromiseReceiver) K(_MOSOutlineAdapter) K(_MOSToolbarView)

void objc_setAssociatedObject(id obj, const void *key, id value, u64 policy);
id objc_getAssociatedObject(id obj, const void *key);
extern const char cx_tv_header_key;
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
static id th_table(id self, SEL c) { (void)c; return objc_getAssociatedObject(self, &cx_tv_header_key); }
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
static id tbi_toolbar(id s, SEL c) { (void)c; return pget(s, "toolbar"); }

/* ================================================================ NSSplitView
 * Tamanhos ao longo do eixo (largura se vertical). A conta (holdingPriority, posição, colapso) está em
 * appkit_table.c e tem teste de host; aqui só ler/gravar frames e perguntar ao delegate. Colapsada = oculta e 0. */
#define SV_MAX 32
void NSRectFill(pr_rect_t r);
static id sv_subviews(id self) { return MSG0(id, self, "subviews"); }
static BOOL sv_is_vert(id self) { return pget_i(self, "vertical", 1) != 0; }
static double sv_div(id self, SEL c) { (void)c; return cx_sv_thickness((int)pget_i(self, "dividerStyle", 2)); }
static id sv_dl(id self, const char *sel) { id d = pget(self, "delegate"); return d && cx_responds(d->isa, sel_registerName(sel)) ? d : 0; }
static void sv_note(id self, const char *sel, const char *name) {
    id d = sv_dl(self, sel);
    if (d) ((void (*)(id, SEL, id))objc_msgSend)(d, sel_registerName(sel), objc_autorelease(cx_note(cx_str(name), self, 0)));
}
static float sv_holding(id s, SEL c, i64 i) {
    (void)c;
    char k[32];
    snprintf(k, sizeof k, "hold%lld", (long long)i);
    id v = pget(s, k);
    return v ? (float)MSG0(double, v, "doubleValue") : 250;
}
static void sv_set_holding(id s, SEL c, float p, i64 i) {
    (void)c;
    char k[32];
    snprintf(k, sizeof k, "hold%lld", (long long)i);
    pset(s, k, cx_num_d(p));
}
static int sv_collect(id self, double *sz, float *hold, int *col) {
    id subs = sv_subviews(self);
    int n = subs ? (int)MSG0(u64, subs, "count") : 0;
    BOOL vert = sv_is_vert(self);
    id d = sv_dl(self, "splitView:shouldAdjustSizeOfSubview:");
    if (n > SV_MAX) n = SV_MAX;
    for (int i = 0; i < n; i++) {
        id v = MSG(id, subs, "objectAtIndex:", (u64)i);
        pr_rect_t f = MSGR(pr_rect_t, v, "frame");
        col[i] = MSG0(BOOL, v, "isHidden") ? 1 : 0;
        sz[i] = col[i] ? 0 : (vert ? f.w : f.h);
        hold[i] = sv_holding(self, 0, i);
        /* o delegate pediu para não mexer: só se não houver outra */
        if (d && !MSG(BOOL, d, "splitView:shouldAdjustSizeOfSubview:", self, v)) hold[i] += 10000;
    }
    return n;
}
static void sv_apply(id self, const double *sz, const int *col, int n) {
    id subs = sv_subviews(self);
    pr_rect_t b = MSGR(pr_rect_t, self, "bounds");
    BOOL vert = sv_is_vert(self);
    double div = sv_div(self, 0), pos = 0;
    for (int i = 0; i < n; i++) {
        id v = MSG(id, subs, "objectAtIndex:", (u64)i);
        double w = col[i] ? 0 : sz[i];
        pr_rect_t f = vert ? (pr_rect_t){pos, 0, w, b.h} : (pr_rect_t){0, pos, b.w, w};
        if (MSG0(BOOL, v, "isHidden") != (col[i] != 0)) MSG(void, v, "setHidden:", (BOOL)(col[i] != 0));
        MSG(void, v, "setFrame:", f);
        pos += w + div;
    }
    MSG(void, self, "setNeedsDisplay:", (BOOL)1);
}
static void sv_adjust(id self, SEL c) {
    (void)c;
    double sz[SV_MAX];
    float hold[SV_MAX];
    int col[SV_MAX];
    int n = sv_collect(self, sz, hold, col);
    if (!n) return;
    pr_rect_t b = MSGR(pr_rect_t, self, "bounds");
    double total = sv_is_vert(self) ? b.w : b.h, div = sv_div(self, 0), avail = total - div * (n - 1), cur = 0;
    int open = 0;
    for (int i = 0; i < n; i++) { cur += sz[i]; open += !col[i]; }
    /* frames do nib sem sentido (todas com 0, ou cada uma do tamanho inteiro): divide igualmente */
    if (open && (cur <= 0 || cur > avail * 1.5 + 1))
        for (int i = 0; i < n; i++) sz[i] = col[i] ? 0 : (avail > 0 ? avail : 0) / open;
    sv_note(self, "splitViewWillResizeSubviews:", "NSSplitViewWillResizeSubviewsNotification");
    cx_sv_resize(sz, hold, col, n, total, div);
    sv_apply(self, sz, col, n);
    sv_note(self, "splitViewDidResizeSubviews:", "NSSplitViewDidResizeSubviewsNotification");
}
/* mudou o tamanho: o delegate que implementa splitView:resizeSubviewsWithOldSize: decide (e costuma chamar
 * adjustSubviews); senão adjustSubviews */
static void sv_resized(id self, pr_size_t old) {
    id d = sv_dl(self, "splitView:resizeSubviewsWithOldSize:");
    if (d) MSG(void, d, "splitView:resizeSubviewsWithOldSize:", self, old);
    else sv_adjust(self, 0);
}
static void sv_set_frame_size(id self, SEL c, pr_size_t z) {
    (void)c;
    pr_rect_t o = MSGR(pr_rect_t, self, "frame");
    SUPER("NSView", void, self, "setFrameSize:", z);
    sv_resized(self, (pr_size_t){o.w, o.h});
}
static void sv_set_frame(id self, SEL c, pr_rect_t f) {
    (void)c;
    pr_rect_t o = MSGR(pr_rect_t, self, "frame");
    SUPER("NSView", void, self, "setFrame:", f);
    sv_resized(self, (pr_size_t){o.w, o.h});
}
static void sv_layout(id self, SEL c) { (void)c; sv_adjust(self, 0); }
static BOOL sv_vertical(id self, SEL c) { (void)c; return sv_is_vert(self); }
static void sv_set_vertical(id self, SEL c, BOOL v) { (void)c; pset(self, "vertical", cx_num_i(v)); sv_adjust(self, 0); }
static BOOL sv_flipped(id s, SEL c) { (void)s; (void)c; return 1; }     /* como no Cocoa: a primeira view fica em cima */
static id sv_arranged(id self, SEL c) { (void)c; return sv_subviews(self); }
static void sv_add_arranged(id self, SEL c, id v) { (void)c; MSG(void, self, "addSubview:", v); sv_adjust(self, 0); }
static void sv_insert_arranged(id self, SEL c, id v, i64 i) {
    (void)c;
    id subs = sv_subviews(self);
    u64 n = subs ? MSG0(u64, subs, "count") : 0;
    if (i >= 0 && (u64)i < n && cx_responds(self->isa, sel_registerName("addSubview:positioned:relativeTo:"))) {
        id rel = MSG(id, subs, "objectAtIndex:", (u64)i);
        MSG(void, self, "addSubview:positioned:relativeTo:", v, (i64)-1, rel);   /* NSWindowBelow */
    } else MSG(void, self, "addSubview:", v);
    sv_adjust(self, 0);
}
static void sv_remove_arranged(id self, SEL c, id v) { (void)c; MSG0(void, v, "removeFromSuperview"); sv_adjust(self, 0); }
/* pontas possíveis da divisória idx: lo = a view idx com 0, hi = a view idx+1 com 0 */
static void sv_span(const double *sz, int idx, double div, double *lo, double *hi) {
    double p = 0;
    for (int i = 0; i < idx; i++) p += sz[i] + div;
    *lo = p;
    *hi = p + sz[idx] + sz[idx + 1];
}
static double sv_cb_min(void *ctx, int index, double proposed) {
    id self = ctx, d = sv_dl(self, "splitView:constrainMinCoordinate:ofSubviewAt:");
    if (d) return ((double (*)(id, SEL, id, double, i64))objc_msgSend)(d, sel_registerName("splitView:constrainMinCoordinate:ofSubviewAt:"), self, proposed, (i64)index);
    return proposed;
}
static double sv_cb_max(void *ctx, int index, double proposed) {
    id self = ctx, d = sv_dl(self, "splitView:constrainMaxCoordinate:ofSubviewAt:");
    if (d) return ((double (*)(id, SEL, id, double, i64))objc_msgSend)(d, sel_registerName("splitView:constrainMaxCoordinate:ofSubviewAt:"), self, proposed, (i64)index);
    return proposed;
}
static double sv_cb_pos(void *ctx, int index, double proposed) {
    id self = ctx, d = sv_dl(self, "splitView:constrainSplitPosition:ofSubviewAt:");
    if (d) return ((double (*)(id, SEL, id, double, i64))objc_msgSend)(d, sel_registerName("splitView:constrainSplitPosition:ofSubviewAt:"), self, proposed, (i64)index);
    return proposed;
}
static void sv_set_pos(id self, SEL c, double p, i64 idx) {
    (void)c;
    double sz[SV_MAX];
    float hold[SV_MAX];
    int col[SV_MAX];
    int n = sv_collect(self, sz, hold, col);
    if (idx < 0 || idx + 1 >= n) return;
    cx_sv_set_position(sz, n, (int)idx, p, sv_div(self, 0));
    sv_apply(self, sz, col, n);
    sv_note(self, "splitViewDidResizeSubviews:", "NSSplitViewDidResizeSubviewsNotification");
}
static double sv_min_pos(id self, SEL c, i64 i) {
    (void)c;
    double sz[SV_MAX], lo, hi;
    float hold[SV_MAX];
    int col[SV_MAX], n = sv_collect(self, sz, hold, col);
    if (i < 0 || i + 1 >= n) return 0;
    sv_span(sz, (int)i, sv_div(self, 0), &lo, &hi);
    return sv_cb_min(self, (int)i, lo);
}
static double sv_max_pos(id self, SEL c, i64 i) {
    (void)c;
    double sz[SV_MAX], lo, hi;
    float hold[SV_MAX];
    int col[SV_MAX], n = sv_collect(self, sz, hold, col);
    if (i < 0 || i + 1 >= n) return 0;
    sv_span(sz, (int)i, sv_div(self, 0), &lo, &hi);
    return sv_cb_max(self, (int)i, hi);
}
static BOOL sv_collapsed(id s, SEL c, id v) { (void)s; (void)c; return v && MSG0(BOOL, v, "isHidden"); }
static BOOL sv_can_collapse(id self, id v) {
    id d = sv_dl(self, "splitView:canCollapseSubview:");
    return d && v && MSG(BOOL, d, "splitView:canCollapseSubview:", self, v);
}
static int sv_divider_at(id self, double mouse, double *out_pos) {
    double sz[SV_MAX];
    float hold[SV_MAX];
    int col[SV_MAX], n = sv_collect(self, sz, hold, col);
    double div = sv_div(self, 0), pos = 0;
    for (int i = 0; i + 1 < n; i++) {
        pos += sz[i];
        if (cx_sv_divider_hit(mouse, pos, div)) {
            if (out_pos) *out_pos = pos;
            return i;
        }
        pos += div;
    }
    return -1;
}
static pr_pt sv_local_pt(id self, id e) {
    pr_pt p = MSG0(pr_pt, e, "locationInWindow");
    return ((pr_pt (*)(id, SEL, pr_pt, id))objc_msgSend)(self, sel_registerName("convertPoint:fromView:"), p, (id)0);
}
static id sv_hit_test(id self, SEL c, pr_pt p) {
    pr_rect_t f = MSGR(pr_rect_t, self, "frame");
    id sup = MSG0(id, self, "superview");
    int fs = sup && MSG0(BOOL, sup, "isFlipped");
    int fm = MSG0(BOOL, self, "isFlipped");
    pr_pt q = {p.x - f.x, p.y - f.y};
    if (fs != fm) q.y = f.h - q.y;
    if (q.x >= 0 && q.y >= 0 && q.x < f.w && q.y < f.h && sv_divider_at(self, sv_is_vert(self) ? q.x : q.y, 0) >= 0) return self;
    return ((id (*)(id, SEL, pr_pt))class_getMethodImplementation(cl("NSView"), c))(self, c, p);
}
/* colapsa (ou reabre) a view k: a vizinha do outro lado da divisória idx fica com o par inteiro */
static void sv_collapse_view(id self, int idx, int k, int on) {
    double sz[SV_MAX];
    float hold[SV_MAX];
    int col[SV_MAX], n = sv_collect(self, sz, hold, col);
    if (idx < 0 || idx + 1 >= n || (k != idx && k != idx + 1)) return;
    int other = k == idx ? idx + 1 : idx;
    if (on) {
        char key[32];
        snprintf(key, sizeof key, "expanded%d", k);
        pset(self, key, cx_num_d(sz[k]));                 /* para reabrir no mesmo tamanho */
        sz[other] += sz[k];
        sz[k] = 0;
        col[k] = 1;
    } else if (col[k]) {
        char key[32];
        snprintf(key, sizeof key, "expanded%d", k);
        double want = pget_d(self, key, sz[other] / 3);
        if (want > sz[other]) want = sz[other];
        sz[k] = want;
        sz[other] -= want;
        col[k] = 0;
    }
    sv_apply(self, sz, col, n);
    sv_note(self, "splitViewDidResizeSubviews:", "NSSplitViewDidResizeSubviewsNotification");
}
static void sv_mouse_down(id self, SEL c, id e) {
    pr_pt q = sv_local_pt(self, e);
    double mouse = sv_is_vert(self) ? q.x : q.y;
    double dpos = 0;
    int idx = sv_divider_at(self, mouse, &dpos);
    if (idx < 0) {
        ((void (*)(id, SEL, id))class_getMethodImplementation(cl("NSView"), c))(self, c, e);
        return;
    }
    if (MSG0(i64, e, "clickCount") >= 2) {           /* duplo clique: o delegate escolhe quem colapsa */
        id d = sv_dl(self, "splitView:shouldCollapseSubview:forDoubleClickOnDividerAtIndex:");
        id subs = sv_subviews(self);
        for (int k = idx; d && k <= idx + 1; k++) {
            id v = MSG(id, subs, "objectAtIndex:", (u64)k);
            if (sv_can_collapse(self, v) && MSG(BOOL, d, "splitView:shouldCollapseSubview:forDoubleClickOnDividerAtIndex:", self, v, (i64)idx)) {
                sv_collapse_view(self, idx, k, !MSG0(BOOL, v, "isHidden"));
                return;
            }
        }
    }
    pset(self, "dragIndex", cx_num_i(idx));
    pset(self, "dragMouse0", cx_num_d(mouse));
    pset(self, "dragPos0", cx_num_d(dpos));
}
static void sv_mouse_drag(id self, SEL c, id e) {
    (void)c;
    id ix = pget(self, "dragIndex");
    if (!ix) return;
    int idx = (int)MSG0(i64, ix, "longLongValue");
    double sz[SV_MAX], lo, hi;
    float hold[SV_MAX];
    int col[SV_MAX], n = sv_collect(self, sz, hold, col);
    if (idx < 0 || idx + 1 >= n) return;
    pr_pt q = sv_local_pt(self, e);
    double mouse = sv_is_vert(self) ? q.x : q.y;
    double proposed = pget_d(self, "dragPos0", 0) + mouse - pget_d(self, "dragMouse0", mouse);
    double div = sv_div(self, 0);
    sv_span(sz, idx, div, &lo, &hi);
    double mn = sv_cb_min(self, idx, lo), mx = sv_cb_max(self, idx, hi);
    id subs = sv_subviews(self);
    id va = MSG(id, subs, "objectAtIndex:", (u64)idx), vb = MSG(id, subs, "objectAtIndex:", (u64)(idx + 1));
    int k = cx_sv_collapse(proposed, lo, hi, mn, mx, sv_can_collapse(self, va), sv_can_collapse(self, vb));
    if (k) {
        int v = k < 0 ? idx : idx + 1;
        if (!col[v]) sv_collapse_view(self, idx, v, 1);
        return;
    }
    col[idx] = col[idx + 1] = 0;                      /* arrastou de volta: reabre */
    cx_sv_lim lim = {sv_cb_min, sv_cb_max, sv_cb_pos};
    double np = cx_sv_drag(proposed, 0, lo, hi, &lim, self, idx);
    cx_sv_set_position(sz, n, idx, np, div);
    sv_apply(self, sz, col, n);
    sv_note(self, "splitViewDidResizeSubviews:", "NSSplitViewDidResizeSubviewsNotification");
}
static void sv_mouse_up(id self, SEL c, id e) {
    (void)e;
    (void)c;
    pset(self, "dragIndex", 0);
}
/* as divisórias: fina = linha cinza; grossa/pane = faixa clara com a linha no meio */
static void sv_draw(id self, SEL c, pr_rect_t r) {
    (void)c; (void)r;
    double sz[SV_MAX];
    float hold[SV_MAX];
    int col[SV_MAX], n = sv_collect(self, sz, hold, col);
    pr_rect_t b = MSGR(pr_rect_t, self, "bounds");
    BOOL vert = sv_is_vert(self);
    double div = sv_div(self, 0), pos = 0;
    id line = MSG(id, (id)cl("NSColor"), "colorWithCalibratedWhite:alpha:", 0.80, 1.0);
    id band = MSG(id, (id)cl("NSColor"), "colorWithCalibratedWhite:alpha:", 0.93, 1.0);
    for (int i = 0; i + 1 < n; i++) {
        pos += sz[i];
        pr_rect_t d = vert ? (pr_rect_t){pos, 0, div, b.h} : (pr_rect_t){0, pos, b.w, div};
        if (div > 1 && band) { MSG0(void, band, "setFill"); NSRectFill(d); }
        if (line) MSG0(void, line, "setFill");
        if (div > 1) d = vert ? (pr_rect_t){pos + div / 2 - 0.5, 0, 1, b.h} : (pr_rect_t){0, pos + div / 2 - 0.5, b.w, 1};
        NSRectFill(d);
        pos += div;
    }
}

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
static int ol_nchild(void *ctx, const void *parent) {
    ol_t *o = ctx;
    if (!ol_resp(o->ds, "outlineView:numberOfChildrenOfItem:") || !ol_resp(o->ds, "outlineView:child:ofItem:")) return 0;
    i64 n = MSG(i64, o->ds, "outlineView:numberOfChildrenOfItem:", o->outline, (id)parent);
    if (n < 0) return 0;
    if (n > 4096) return 4096;
    return (int)n;
}
static const void *ol_child_at(void *ctx, int index, const void *parent) {
    ol_t *o = ctx;
    return MSG(id, o->ds, "outlineView:child:ofItem:", o->outline, (i64)index, (id)parent);
}
static int ol_exp_cb(void *ctx, const void *item) { return ol_is_expanded(ctx, (id)item); }
static void ol_emit(void *ctx, const void *item, int level) { ol_push(ctx, (id)item, level); }
static void ol_rebuild(ol_t *o) {
    o->n = 0;
    cx_ol_ops ops = {ol_nchild, ol_child_at, ol_exp_cb};
    cx_ol_flatten(&ops, o, ol_emit, o, 0);
}

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
static BOOL oa_group(id self, SEL c, id t, i64 row) {
    (void)self; (void)c;
    ol_t *o = ol_of(t);
    if (!o || row < 0 || row >= o->n || !ol_resp(o->dl, "outlineView:isGroupItem:")) return 0;
    return MSG(BOOL, o->dl, "outlineView:isGroupItem:", t, o->rows[row].item);
}
static BOOL oa_should_select(id self, SEL c, id t, i64 row) {
    (void)self; (void)c;
    ol_t *o = ol_of(t);
    if (!o || row < 0 || row >= o->n || !ol_resp(o->dl, "outlineView:shouldSelectItem:")) return 1;
    return MSG(BOOL, o->dl, "outlineView:shouldSelectItem:", t, o->rows[row].item);
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
static void ol_expand_deep(ol_t *o, id item, int depth) {
    if (depth > 32) return;
    if (item && !ol_is_expanded(o, item)) MSG(void, o->expanded, "addObject:", item);
    if (!ol_resp(o->ds, "outlineView:numberOfChildrenOfItem:") || !ol_resp(o->ds, "outlineView:child:ofItem:")) return;
    i64 n = MSG(i64, o->ds, "outlineView:numberOfChildrenOfItem:", o->outline, item);
    if (n > 4096) n = 4096;
    for (i64 i = 0; i < n; i++) ol_expand_deep(o, MSG(id, o->ds, "outlineView:child:ofItem:", o->outline, i, item), depth + 1);
}
static void ol_collapse_deep(ol_t *o, id item, int depth) {
    if (!item || depth > 32) return;
    if (ol_resp(o->ds, "outlineView:numberOfChildrenOfItem:") && ol_resp(o->ds, "outlineView:child:ofItem:")) {
        i64 n = MSG(i64, o->ds, "outlineView:numberOfChildrenOfItem:", o->outline, item);
        if (n > 4096) n = 4096;
        for (i64 i = 0; i < n; i++) ol_collapse_deep(o, MSG(id, o->ds, "outlineView:child:ofItem:", o->outline, i, item), depth + 1);
    }
    MSG(void, o->expanded, "removeObject:", item);
}
/* aviso de expandir/recolher: ao delegate (outlineViewItemDidExpand: etc.) e à central, com o item em
 * userInfo["NSObject"], como no Cocoa */
static void ov_note(id self, ol_t *o, id item, const char *dsel, const char *name) {
    id info = MSG0(id, (id)cl("NSMutableDictionary"), "dictionary");
    if (info && item) MSG(void, info, "setObject:forKey:", item, cx_str("NSObject"));
    id note = objc_autorelease(cx_note(cx_str(name), self, info));
    if (o->dl && cx_responds(o->dl->isa, sel_registerName(dsel)))
        ((void (*)(id, SEL, id))objc_msgSend)(o->dl, sel_registerName(dsel), note);
    id nc = cl("NSNotificationCenter") ? MSG0(id, (id)cl("NSNotificationCenter"), "defaultCenter") : 0;
    if (nc) MSG(void, nc, "postNotification:", note);
}
static void ov_expand(id self, SEL c, id item, BOOL children) {
    (void)c;
    ol_t *o = ol_make(self);
    if (item && ol_resp(o->dl, "outlineView:shouldExpandItem:") && !MSG(BOOL, o->dl, "outlineView:shouldExpandItem:", self, item)) return;
    if (item) ov_note(self, o, item, "outlineViewItemWillExpand:", "NSOutlineViewItemWillExpandNotification");
    if (children) ol_expand_deep(o, item, 0);
    else if (!item) {
        ol_rebuild(o);
        for (i64 i = 0; i < o->n; i++) if (!ol_is_expanded(o, o->rows[i].item)) MSG(void, o->expanded, "addObject:", o->rows[i].item);
    } else if (!ol_is_expanded(o, item)) MSG(void, o->expanded, "addObject:", item);
    ov_reload(self, 0);
    if (item) ov_note(self, o, item, "outlineViewItemDidExpand:", "NSOutlineViewItemDidExpandNotification");
}
static void ov_expand1(id self, SEL c, id item) { ov_expand(self, c, item, 0); }
static void ov_collapse(id self, SEL c, id item, BOOL children) {
    (void)c;
    ol_t *o = ol_make(self);
    if (item && ol_resp(o->dl, "outlineView:shouldCollapseItem:") && !MSG(BOOL, o->dl, "outlineView:shouldCollapseItem:", self, item)) return;
    if (item) ov_note(self, o, item, "outlineViewItemWillCollapse:", "NSOutlineViewItemWillCollapseNotification");
    if (!item) MSG0(void, o->expanded, "removeAllObjects");
    else if (children) ol_collapse_deep(o, item, 0);
    else MSG(void, o->expanded, "removeObject:", item);
    ov_reload(self, 0);
    if (item) ov_note(self, o, item, "outlineViewItemDidCollapse:", "NSOutlineViewItemDidCollapseNotification");
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
    if (v) { MSG(void, v, "setFrame:", MSGR(pr_rect_t, self, "bounds")); MSG(void, self, "addSubview:", v); }
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
    if (v) MSG(void, v, "setFrame:", MSGR(pr_rect_t, self, "bounds"));
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
void ak_rect_fill(double x, double y, double w, double h, u64 op) __asm__("$sSo6CGRectV6AppKitE4fill5usingySo22NSCompositingOperationV_tF");
void ak_rect_fill(double x, double y, double w, double h, u64 op) { NSRectFillUsingOperation((pr_rect_t){x, y, w, h}, op); }
void ak_rect_frame(double x, double y, double w, double h, double width, u64 op)
    __asm__("$sSo6CGRectV6AppKitE5frame9withWidth5usingy12CoreGraphics7CGFloatV_So22NSCompositingOperationVtF");
void ak_rect_frame(double x, double y, double w, double h, double width, u64 op) {
    (void)op;                                  /* as quatro bordas por dentro do retângulo */
    extern void NSRectFill(pr_rect_t r);
    NSRectFill((pr_rect_t){x, y, w, width}); NSRectFill((pr_rect_t){x, y + h - width, w, width});
    NSRectFill((pr_rect_t){x, y, width, h}); NSRectFill((pr_rect_t){x + w - width, y, width, h});
}
void ak_sound_beep(void) __asm__("$sSo7NSSoundC6AppKitE4beepyyFZ");
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
    for (id v = MSG0(id, self, "superview"); v; v = MSG0(id, v, "superview")) if (sc && cx_is_subclass(v->isa, sc)) return v;
    return 0;
}
static id v_menu(id self, SEL c) { (void)c; return pget(self, "menu"); }
static void v_set_menu(id self, SEL c, id m) { (void)c; pset(self, "menu", m); }
static id tcv_image_view(id self, SEL c) { (void)c; return pget(self, "imageView"); }
static void tcv_set_image_view(id self, SEL c, id v) { (void)c; pset(self, "imageView", v); }
static void tv_drag_mask(id s, SEL c, u64 m, BOOL l) { (void)s; (void)c; (void)m; (void)l; }

/* NSPageController: mostra a view do controller que o delegate dá para o objeto selecionado */
static id pc_arranged(id self, SEL c) { (void)c; id a = pget(self, "arrangedObjects"); return a ? a : MSG0(id, (id)cl("NSArray"), "array"); }
static void pc_show(id self) {
    id objs = pget(self, "arrangedObjects");
    i64 i = pget_i(self, "selectedIndex", 0);
    if (!objs || i < 0 || (u64)i >= MSG0(u64, objs, "count")) return;
    id obj = MSG(id, objs, "objectAtIndex:", (u64)i);
    id d = MSG0(id, self, "delegate");
    if (!d) return;
    id ident = cx_responds(d->isa, sel_registerName("pageController:identifierForObject:")) ? MSG(id, d, "pageController:identifierForObject:", self, obj) : cx_str("");
    id vc = cx_responds(d->isa, sel_registerName("pageController:viewControllerForIdentifier:")) ? MSG(id, d, "pageController:viewControllerForIdentifier:", self, ident) : 0;
    if (!vc) return;
    pset(self, "selectedViewController", vc);
    if (cx_responds(d->isa, sel_registerName("pageController:prepareViewController:withObject:"))) MSG(void, d, "pageController:prepareViewController:withObject:", self, vc, obj);
    id host = MSG0(id, self, "view"), v = MSG0(id, vc, "view");
    if (!host || !v) return;
    id subs = MSG0(id, host, "subviews");
    for (u64 k = subs ? MSG0(u64, subs, "count") : 0; k > 0; k--) MSG0(void, MSG(id, subs, "objectAtIndex:", k - 1), "removeFromSuperview");
    MSG(void, v, "setFrame:", MSGR(pr_rect_t, host, "bounds"));
    MSG(void, host, "addSubview:", v);
    if (cx_responds(d->isa, sel_registerName("pageControllerDidEndLiveTransition:"))) MSG(void, d, "pageControllerDidEndLiveTransition:", self);
}
static void pc_set_arranged(id self, SEL c, id a) { (void)c; pset(self, "arrangedObjects", a); pc_show(self); }
static i64 pc_sel(id self, SEL c) { (void)c; return pget_i(self, "selectedIndex", 0); }
static void pc_set_sel(id self, SEL c, i64 i) { (void)c; pset(self, "selectedIndex", cx_num_i(i)); pc_show(self); }
static id pc_sel_vc(id self, SEL c) { (void)c; return pget(self, "selectedViewController"); }
static void pc_noop_i(id s, SEL c, i64 v) { (void)s; (void)c; (void)v; }
static void pc_nav_back(id self, SEL c, id s) { (void)c; (void)s; i64 i = pc_sel(self, 0); if (i > 0) pc_set_sel(self, 0, i - 1); }
static void pc_nav_fwd(id self, SEL c, id s) { (void)c; (void)s; id a = pc_arranged(self, 0); i64 i = pc_sel(self, 0); if ((u64)(i + 1) < MSG0(u64, a, "count")) pc_set_sel(self, 0, i + 1); }
static void pc_complete(id s, SEL c) { (void)s; (void)c; }
static u64 gc_compositing(id s, SEL c) { (void)s; (void)c; return 2; }   /* NSCompositingOperationSourceOver */
static void gc_set_compositing(id s, SEL c, u64 v) { (void)s; (void)c; (void)v; }

/* ================================================================ NSToolbar: desenho e clique
 * A barra é a view hospedeira (_MOSToolbarView, filha de NSView): o appkit.c a desenha no topo da janela e manda
 * para ela os eventos da faixa. Item com view (campo de busca, controle segmentado...) vira subview dela e recebe
 * clique e teclado como qualquer view; os outros são desenhados aqui (imagem ou rótulo) e o clique vai ao target/action. */
static BOOL tb_is_visible(id self, SEL c) {
    (void)c;
    id v = pget(self, "visible");
    if (!v) return YES;
    return MSG0(i64, v, "longLongValue") != 0;
}
static void tb_set_visible(id self, SEL c, BOOL v) { (void)c; pset(self, "visible", cx_num_i(v ? 1 : 0)); }
static id tb_visible_obj(id self, SEL c) { (void)c; return pget(self, "visible"); }
static int tb_fill_spec(id item, cx_tb_spec *sp) {
    memset(sp, 0, sizeof *sp);
    if (!item) return 0;
    sp->enabled = MSG0(BOOL, item, "isEnabled") ? 1 : 0;
    const char *ident = cx_cstr(MSG0(id, item, "itemIdentifier"));
    if (ident && !strcmp(ident, "NSToolbarFlexibleSpaceItem")) { sp->flexible = 1; sp->min_w = 0; }
    else if (ident && !strcmp(ident, "NSToolbarSpaceItem")) sp->min_w = 12;
    else {
        pr_size_t z = MSG0(pr_size_t, item, "minSize");
        sp->min_w = z.w > 1 ? z.w + 4 : 72;
    }
    sp->target = MSG0(id, item, "target");
    sp->action = (void *)((SEL (*)(id, SEL))objc_msgSend)(item, sel_registerName("action"));
    return 1;
}
static id tb_items_get(id self, SEL c);
static int tb_specs(id self, cx_tb_spec *spec, id *items_out, int cap) {
    id items = tb_items_get(self, 0);
    u64 n = items ? MSG0(u64, items, "count") : 0;
    if ((int)n > cap) n = (u64)cap;
    for (u64 i = 0; i < n; i++) {
        id it = MSG(id, items, "objectAtIndex:", i);
        if (items_out) items_out[i] = it;
        tb_fill_spec(it, &spec[i]);
    }
    return (int)n;
}
static void tb_send(void *target, void *action, int index, void *ctx) {
    id *items = ctx;
    id item = items && index >= 0 ? items[index] : 0;
    if (target && action) ((void (*)(id, SEL, id))objc_msgSend)((id)target, (SEL)action, item ? item : (id)target);
}
/* r: a faixa da barra no espaço de desenho atual (a view hospedeira é invertida: y de cima para baixo) */
static void tb_paint(id self, pr_rect_t r) {
    cx_tb_spec spec[64];
    cx_tb_rect rects[64];
    id objs[64];
    int n = tb_specs(self, spec, objs, 64);
    cx_tb_layout(spec, n, r.w, r.h, rects);
    id bar = MSG(id, (id)cl("NSColor"), "colorWithCalibratedWhite:alpha:", 0.93, 1.0);
    if (bar) MSG0(void, bar, "setFill");
    NSRectFill(r);
    id sep = MSG(id, (id)cl("NSColor"), "colorWithCalibratedWhite:alpha:", 0.80, 1.0);
    if (sep) { MSG0(void, sep, "setFill"); NSRectFill((pr_rect_t){r.x, r.y + r.h - 1, r.w, 1}); }
    id chip = MSG(id, (id)cl("NSColor"), "colorWithCalibratedWhite:alpha:", 0.84, 1.0);
    for (int i = 0; i < n; i++) {
        if (spec[i].flexible || rects[i].w < 8 || MSG0(id, objs[i], "view")) continue;   /* a view se desenha */
        pr_rect_t fr = {r.x + rects[i].x + 2, r.y + 4, rects[i].w - 4, r.h - 8};
        if (fr.w < 1 || fr.h < 1) continue;
        id img = MSG0(id, objs[i], "image");
        if (img) {
            double s = fr.h < 16 ? fr.h : 16;
            pr_rect_t ir = {fr.x + (fr.w - s) / 2, fr.y + (fr.h - s) / 2, s, s};
            MSG(void, img, "drawInRect:", ir);
            continue;
        }
        if (MSG0(BOOL, objs[i], "isBordered") || spec[i].target) { if (chip) MSG0(void, chip, "setFill"); NSRectFill(fr); }
        id label = MSG0(id, objs[i], "label");
        if (!label || !cx_is_str(label) || !cx_cstr(label)[0]) label = MSG0(id, objs[i], "title");
        if (label && cx_is_str(label)) {
            pr_pt pt = {fr.x + 6, fr.y + 4};
            ((void (*)(id, SEL, pr_pt, id))objc_msgSend)(label, sel_registerName("drawAtPoint:withAttributes:"), pt, (id)0);
        }
    }
}
static void tb_draw(id self, SEL c, pr_rect_t r) {
    (void)c;
    if (!tb_is_visible(self, 0) || r.w < 1 || r.h < 1) return;
    tb_paint(self, r);
}
/* clique na faixa (x, y a partir do canto de cima, largura w) */
static void tb_click_at(id self, double x, double y, double w) {
    cx_tb_spec spec[64];
    cx_tb_rect rects[64];
    id objs[64];
    int n = tb_specs(self, spec, objs, 64);
    cx_tb_layout(spec, n, w > 1 ? w : 200, 28, rects);
    cx_tb_click(spec, rects, n, x, y, tb_send, objs);
}
static void tb_mouse(id self, SEL c, id e) {
    (void)c;
    if (!tb_is_visible(self, 0) || !e) return;
    id win = MSG0(id, e, "window");
    pr_rect_t wf = win ? MSGR(pr_rect_t, win, "frame") : (pr_rect_t){0, 0, 0, 0};
    pr_pt p = MSG0(pr_pt, e, "locationInWindow");
    double local_y = p.y - (wf.h - 28);
    if (local_y < 0 || local_y >= 28) return;
    tb_click_at(self, p.x, local_y, wf.w);
}
/* ---- view hospedeira */
static char tbh_toolbar_key;
static BOOL tbh_flipped(id s, SEL c) { (void)s; (void)c; return 1; }
static void tbh_draw(id self, SEL c, pr_rect_t r) {
    (void)c; (void)r;
    id tb = objc_getAssociatedObject(self, &tbh_toolbar_key);
    if (tb) tb_paint(tb, MSGR(pr_rect_t, self, "bounds"));
}
static void tbh_mouse(id self, SEL c, id e) {
    (void)c;
    id tb = objc_getAssociatedObject(self, &tbh_toolbar_key);
    if (!tb || !e) return;
    pr_pt p = ((pr_pt (*)(id, SEL, pr_pt, id))objc_msgSend)(self, sel_registerName("convertPoint:fromView:"), MSG0(pr_pt, e, "locationInWindow"), (id)0);
    tb_click_at(tb, p.x, p.y, MSGR(pr_rect_t, self, "bounds").w);
}
/* -_mosHostViewWithWidth: (appkit.c): cria a view, põe as views dos itens nos retângulos do layout e tira as que
 * saíram da barra */
static id tb_host(id self, SEL c, double width) {
    (void)c;
    id host = pget(self, "hostView");
    if (!host) {
        host = MSG(id, MSG0(id, (id)cl("_MOSToolbarView"), "alloc"), "initWithFrame:", ((pr_rect_t){0, 0, width, 28}));
        if (!host) return 0;
        objc_setAssociatedObject(host, &tbh_toolbar_key, self, 0);      /* fraco: a toolbar retém a view */
        pset(self, "hostView", host);
        objc_release(host);
    }
    MSG(void, host, "setFrame:", ((pr_rect_t){0, 0, width, 28}));
    cx_tb_spec spec[64];
    cx_tb_rect rects[64];
    id objs[64];
    int n = tb_specs(self, spec, objs, 64);
    cx_tb_layout(spec, n, width, 28, rects);
    id subs = MSG0(id, host, "subviews");
    for (u64 k = subs ? MSG0(u64, subs, "count") : 0; k > 0; k--) {
        id sv = MSG(id, subs, "objectAtIndex:", k - 1);
        int keep = 0;
        for (int i = 0; i < n && !keep; i++) keep = MSG0(id, objs[i], "view") == sv;
        if (!keep) MSG0(void, sv, "removeFromSuperview");
    }
    for (int i = 0; i < n; i++) {
        id v = MSG0(id, objs[i], "view");
        if (!v) continue;
        pr_rect_t vf = MSGR(pr_rect_t, v, "frame");
        double h = vf.h > 1 && vf.h < 24 ? vf.h : 22;
        pr_rect_t f = {rects[i].x + 2, (28 - h) / 2, rects[i].w - 4 > 1 ? rects[i].w - 4 : 1, h};
        if (MSG0(id, v, "superview") != host) MSG(void, host, "addSubview:", v);
        MSG(void, v, "setFrame:", f);
    }
    return host;
}
static void tb_note_add(id self, id item) {
    id d = pget(self, "delegate");
    if (!d || !item || !cx_responds(d->isa, sel_registerName("toolbarWillAddItem:"))) return;
    id info = MSG0(id, (id)cl("NSMutableDictionary"), "dictionary");
    if (info) MSG(void, info, "setObject:forKey:", item, cx_str("item"));
    MSG(void, d, "toolbarWillAddItem:", objc_autorelease(cx_note(cx_str("NSToolbarWillAddItemNotification"), self, info)));
}
static void tb_insert(id self, SEL c, id ident, i64 idx) {
    (void)c;
    id d = pget(self, "delegate");
    id item = 0;
    if (d && cx_responds(d->isa, sel_registerName("toolbar:itemForItemIdentifier:willBeInsertedIntoToolbar:")))
        item = MSG(id, d, "toolbar:itemForItemIdentifier:willBeInsertedIntoToolbar:", self, ident, YES);
    if (!item) item = objc_autorelease(MSG(id, MSG0(id, (id)cl("NSToolbarItem"), "alloc"), "initWithItemIdentifier:", ident));
    if (!item) return;
    pset(item, "toolbar", self);
    id items = pget(self, "items");
    if (!items) {
        items = MSG0(id, (id)cl("NSMutableArray"), "array");
        pset(self, "items", items);
    }
    if (!items) return;
    tb_note_add(self, item);
    u64 n = MSG0(u64, items, "count");
    if (idx < 0 || (u64)idx >= n) MSG(void, items, "addObject:", item);
    else MSG(void, items, "insertObject:atIndex:", item, (u64)idx);
}
/* sem itens ainda: os padrões do delegate (toolbarDefaultItemIdentifiers:), como o Cocoa faz ao mostrar a barra */
static id tb_items_get(id self, SEL c) {
    (void)c;
    id items = pget(self, "items");
    if (items || pget(self, "loadedDefaults")) return items;
    pset(self, "loadedDefaults", cx_num_i(1));
    id d = pget(self, "delegate");
    if (!d || !cx_responds(d->isa, sel_registerName("toolbarDefaultItemIdentifiers:"))) return 0;
    id ids = MSG(id, d, "toolbarDefaultItemIdentifiers:", self);
    u64 n = ids ? MSG0(u64, ids, "count") : 0;
    for (u64 i = 0; i < n && i < 64; i++) tb_insert(self, 0, MSG(id, ids, "objectAtIndex:", i), -1);
    return pget(self, "items");
}
static void tb_items_set(id self, SEL c, id a) { (void)c; pset(self, "items", a); }
static void tb_remove_at(id self, SEL c, i64 idx) {
    (void)c;
    id items = pget(self, "items");
    if (!items || idx < 0 || (u64)idx >= MSG0(u64, items, "count")) return;
    id it = MSG(id, items, "objectAtIndex:", (u64)idx);
    id d = pget(self, "delegate");
    if (d && cx_responds(d->isa, sel_registerName("toolbarDidRemoveItem:"))) {
        id info = MSG0(id, (id)cl("NSMutableDictionary"), "dictionary");
        if (info) MSG(void, info, "setObject:forKey:", it, cx_str("item"));
        MSG(void, d, "toolbarDidRemoveItem:", objc_autorelease(cx_note(cx_str("NSToolbarDidRemoveItemNotification"), self, info)));
    }
    MSG(void, items, "removeObjectAtIndex:", (u64)idx);
}
static id tb_centered(id self, SEL c) { (void)c; return pget(self, "centeredItemIdentifiers"); }
static void tb_set_centered(id self, SEL c, id a) { (void)c; pset(self, "centeredItemIdentifiers", a); }
OBJP(selectedItemIdentifier, setSelectedItemIdentifier)
INTP(displayMode, setDisplayMode, 0) INTP(sizeMode, setSizeMode, 0) INTP(allowsUserCustomization, setAllowsUserCustomization, 0)
INTP(autosavesConfiguration, setAutosavesConfiguration, 0) INTP(showsBaselineSeparator, setShowsBaselineSeparator, 1)
INTP(controlSize, setControlSize, 0)   /* NSControlSizeRegular */
/* NSToolbarItem: minSize/maxSize guardados; sem valor, o tamanho da view ou do rótulo */
static pr_size_t tbi_min(id s, SEL c) {
    (void)c;
    id w = pget(s, "minW");
    if (w) return (pr_size_t){MSG0(double, w, "doubleValue"), pget_d(s, "minH", 28)};
    id v = pget(s, "view");
    if (v) { pr_rect_t f = MSGR(pr_rect_t, v, "frame"); if (f.w > 1) return (pr_size_t){f.w, f.h}; }
    if (pget(s, "image")) return (pr_size_t){32, 28};
    id l = pget(s, "label");
    size_t n = l && cx_is_str(l) ? strlen(cx_cstr(l)) : 0;
    return (pr_size_t){n ? 7.0 * (double)n + 16 : 32, 28};
}
static pr_size_t tbi_max(id s, SEL c) {
    id w = pget(s, "maxW");
    if (w) return (pr_size_t){MSG0(double, w, "doubleValue"), pget_d(s, "maxH", 28)};
    return tbi_min(s, c);
}
static void tbi_set_min(id s, SEL c, pr_size_t z) { (void)c; pset(s, "minW", cx_num_d(z.w)); pset(s, "minH", cx_num_d(z.h)); }
static void tbi_set_max(id s, SEL c, pr_size_t z) { (void)c; pset(s, "maxW", cx_num_d(z.w)); pset(s, "maxH", cx_num_d(z.h)); }
static char win_tb_key;
static id win_toolbar(id self, SEL c) { (void)c; return objc_getAssociatedObject(self, &win_tb_key); }
static void win_set_toolbar(id self, SEL c, id tb) { (void)c; objc_setAssociatedObject(self, &win_tb_key, tb, 1); }

/* ================================================================ registro */
void cx_appkit_postico_init(void) {
    { cx_class_t *k = cl("NSGraphicsContext"); if (k) { cx_add_method(k, "compositingOperation", (void *)gc_compositing); cx_add_method(k, "setCompositingOperation:", (void *)gc_set_compositing); } }
    { cx_class_t *v = cl("NSView"); if (v) { if (!cx_responds(v, sel_registerName("enclosingScrollView"))) cx_add_method(v, "enclosingScrollView", (void *)v_enclosing_scroll); if (!cx_responds(v, sel_registerName("menu"))) { cx_add_method(v, "menu", (void *)v_menu); cx_add_method(v, "setMenu:", (void *)v_set_menu); } } }
    { cx_class_t *k = cl("NSTableCellView"); if (k && !cx_responds(k, sel_registerName("imageView"))) { cx_add_method(k, "imageView", (void *)tcv_image_view); cx_add_method(k, "setImageView:", (void *)tcv_set_image_view); } }
    { cx_class_t *k = cl("NSTableCellView"); if (k && !cx_responds(k, sel_registerName("objectValue"))) { cx_add_method(k, "objectValue", (void *)g_objectValue); cx_add_method(k, "setObjectValue:", (void *)s_objectValue); } }
    { cx_class_t *k = cl("NSTableView"); if (k) cx_add_method(k, "setDraggingSourceOperationMask:forLocal:", (void *)tv_drag_mask); }
    { cx_class_t *k = cl("NSObject"); if (k && !cx_responds(k, sel_registerName("awakeFromNib"))) cx_add_method(k, "awakeFromNib", (void *)noop); }
    { cx_class_t *k = cl("NSToolbar"); if (k) {
        cx_add_method(k, "isVisible", (void *)tb_is_visible); cx_add_method(k, "visible", (void *)tb_visible_obj);
        cx_add_method(k, "setVisible:", (void *)tb_set_visible); cx_add_method(k, "items", (void *)tb_items_get);
        cx_add_method(k, "setItems:", (void *)tb_items_set); cx_add_method(k, "drawInRect:", (void *)tb_draw);
        cx_add_method(k, "mouseDown:", (void *)tb_mouse); cx_add_method(k, "insertItemWithItemIdentifier:atIndex:", (void *)tb_insert);
        cx_add_method(k, "centeredItemIdentifiers", (void *)tb_centered); cx_add_method(k, "setCenteredItemIdentifiers:", (void *)tb_set_centered);
        cx_add_method(k, "_mosHostViewWithWidth:", (void *)tb_host); cx_add_method(k, "removeItemAtIndex:", (void *)tb_remove_at);
        cx_add_method(k, "selectedItemIdentifier", (void *)g_selectedItemIdentifier); cx_add_method(k, "setSelectedItemIdentifier:", (void *)s_selectedItemIdentifier);
        cx_add_method(k, "displayMode", (void *)g_displayMode); cx_add_method(k, "setDisplayMode:", (void *)s_displayMode);
        cx_add_method(k, "sizeMode", (void *)g_sizeMode); cx_add_method(k, "setSizeMode:", (void *)s_sizeMode);
        cx_add_method(k, "allowsUserCustomization", (void *)g_allowsUserCustomization); cx_add_method(k, "setAllowsUserCustomization:", (void *)s_allowsUserCustomization);
        cx_add_method(k, "autosavesConfiguration", (void *)g_autosavesConfiguration); cx_add_method(k, "setAutosavesConfiguration:", (void *)s_autosavesConfiguration);
        cx_add_method(k, "showsBaselineSeparator", (void *)g_showsBaselineSeparator); cx_add_method(k, "setShowsBaselineSeparator:", (void *)s_showsBaselineSeparator);
    } }
    { cx_class_t *k = cl("NSControl"); if (k) { cx_add_method(k, "controlSize", (void *)g_controlSize); cx_add_method(k, "setControlSize:", (void *)s_controlSize); } }
    DEF(_MOSToolbarView, "NSView");
    IM(_MOSToolbarView, "isFlipped", tbh_flipped); IM(_MOSToolbarView, "drawRect:", tbh_draw); IM(_MOSToolbarView, "mouseDown:", tbh_mouse);
    { cx_class_t *k = cl("NSWindow"); if (k) { cx_add_method(k, "toolbar", (void *)win_toolbar); cx_add_method(k, "setToolbar:", (void *)win_set_toolbar); } }
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
    IM(NSToolbarItem, "minSize", tbi_min); IM(NSToolbarItem, "maxSize", tbi_max);
    IM(NSToolbarItem, "setMinSize:", tbi_set_min); IM(NSToolbarItem, "setMaxSize:", tbi_set_max);
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
    IM(NSSplitView, "isFlipped", sv_flipped); IM(NSSplitView, "drawRect:", sv_draw);
    IM(NSSplitView, "hitTest:", sv_hit_test); IM(NSSplitView, "mouseDown:", sv_mouse_down);
    IM(NSSplitView, "mouseDragged:", sv_mouse_drag); IM(NSSplitView, "mouseUp:", sv_mouse_up);
    REG(NSSplitView, dividerStyle, setDividerStyle); REG(NSSplitView, arrangesAllSubviews, setArrangesAllSubviews);
    REG(NSSplitView, autosaveName, setAutosaveName);
    IM(NSSplitView, "delegate", g_delegate); IM(NSSplitView, "setDelegate:", s_delegate);

    DEF(_MOSOutlineAdapter, "NSObject");
    IM(_MOSOutlineAdapter, "numberOfRowsInTableView:", oa_rows);
    IM(_MOSOutlineAdapter, "tableView:objectValueForTableColumn:row:", oa_value);
    IM(_MOSOutlineAdapter, "tableView:viewForTableColumn:row:", oa_view);
    IM(_MOSOutlineAdapter, "tableViewSelectionDidChange:", oa_sel_changed);
    IM(_MOSOutlineAdapter, "tableView:isGroupRow:", oa_group); IM(_MOSOutlineAdapter, "tableView:shouldSelectRow:", oa_should_select);
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
    DEF(NSTableHeaderView, "NSView");
    IM(NSTableHeaderView, "tableView", th_table);

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
    IM(NSPageController, "arrangedObjects", pc_arranged); IM(NSPageController, "setArrangedObjects:", pc_set_arranged);
    IM(NSPageController, "selectedIndex", pc_sel); IM(NSPageController, "setSelectedIndex:", pc_set_sel);
    IM(NSPageController, "selectedViewController", pc_sel_vc); IM(NSPageController, "setTransitionStyle:", pc_noop_i);
    IM(NSPageController, "navigateBack:", pc_nav_back); IM(NSPageController, "navigateForward:", pc_nav_fwd);
    IM(NSPageController, "completeTransition", pc_complete);
    IM(NSPageController, "delegate", g_delegate); IM(NSPageController, "setDelegate:", s_delegate);
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
    (void)ret_self; (void)ret_yes; (void)g_alphaValueP; (void)s_alphaValueP; 
    (void)g_state; (void)s_state; (void)g_arrangedSubviewsStore; (void)s_arrangedSubviewsStore; (void)g_viewController;
    (void)s_tabView;
}
