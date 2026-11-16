/*
 * mOS libSystem — sheets do AppKit como no macOS: a sheet é uma janela filha da mãe (GUI_FOCUS_OWNER, sempre acima
 * dela), centrada no alto do conteúdo da mãe, chave enquanto aberta; a mãe não recebe cliques nem teclas até a sheet
 * terminar (cx_sheet_blocks, chamado do dispatch em appkit.c). Uma segunda sheet na mesma janela espera na fila e
 * abre quando a primeira termina.
 *   NSWindow: beginSheet:completionHandler:, beginCriticalSheet:completionHandler:, endSheet:, endSheet:returnCode:,
 *     attachedSheet, sheetParent, isSheet, sheets.
 *   NSApplication (legado): beginSheet:modalForWindow:modalDelegate:didEndSelector:contextInfo:, endSheet:,
 *     endSheet:returnCode:.
 * Registro: cx_appkit_sheets_init(), chamado do cx_appkit_input_init (depois do appkit_apps_ui.c).
 */
#include "foundation.h"
#include "sys.h"
#include "libc.h"

id objc_msgSend_stret(id self, SEL op, ...);
id objc_retain(id o);
void objc_release(id o);
void objc_setAssociatedObject(id obj, const void *key, id value, u64 policy);
id objc_getAssociatedObject(id obj, const void *key);
void *_Block_copy(const void *b);
void _Block_release(const void *b);
void NSBeep(void);
IMP class_getMethodImplementation(cx_class_t *c, SEL s);
extern id NSApp;

typedef struct { double x, y, w, h; } R;
static SEL sl(const char *n) { return sel_registerName(n); }
static R frame_of(id w) { return ((R (*)(id, SEL))objc_msgSend_stret)(w, sl("frame")); }

#define NSModalResponseStop (-1000)
static char k_sheet, k_parent, k_queue, k_handler;

/* fim da sheet: o que chamar (bloco ou delegate+seletor legado) */
typedef struct { void *block; id delegate; SEL did_end; void *ctx; } sheet_end_t;
static void end_free(void *p) { sheet_end_t *e = p; if (e->block) _Block_release(e->block); free(e); }
static id end_box(void *block, id delegate, SEL did_end, void *ctx) {
    sheet_end_t *e = calloc(1, sizeof *e);
    if (!e) return 0;
    e->block = block ? _Block_copy(block) : 0;
    e->delegate = delegate; e->did_end = did_end; e->ctx = ctx;
    /* o NSValue guarda o ponteiro; o dono libera em end_free */
    return MSG(id, (id)cx_class_named("NSValue"), "valueWithPointer:", (void *)e);
}

static id attached(id w) { return w ? objc_getAssociatedObject(w, &k_sheet) : 0; }
static id parent_of(id sheet) { return sheet ? objc_getAssociatedObject(sheet, &k_parent) : 0; }

/* a mãe está com sheet aberta: cliques e teclas param nela (appkit.c, dispatch) */
int cx_sheet_blocks(id w) {
    id s = attached(w);
    if (!s) return 0;
    MSG(void, s, "makeKeyAndOrderFront:", (id)0);
    return 1;
}

static void present(id parent, id sheet, id endv) {
    objc_setAssociatedObject(parent, &k_sheet, sheet, 1);
    objc_setAssociatedObject(sheet, &k_parent, parent, 0);
    objc_setAssociatedObject(sheet, &k_handler, endv, 1);
    /* no alto do conteúdo da mãe, centrada (antes de aparecer a moldura ainda é livre) */
    R pf = frame_of(parent), sf = frame_of(sheet);
    R nf = {pf.x + (pf.w - sf.w) / 2, pf.y + pf.h - sf.h, sf.w, sf.h};
    MSG(void, sheet, "setFrame:display:", nf, (BOOL)NO);
    MSG(void, parent, "addChildWindow:ordered:", sheet, (i64)1);
    MSG(void, sheet, "makeKeyAndOrderFront:", (id)0);
    extern id NSWindowWillBeginSheetNotification;
    id nc = MSG0(id, (id)cx_class_named("NSNotificationCenter"), "defaultCenter");
    MSG(void, nc, "postNotificationName:object:", NSWindowWillBeginSheetNotification, parent);
    id d = MSG0(id, parent, "delegate");
    if (d && cx_responds(object_getClass(d), sl("windowWillBeginSheet:")))
    {
        id note = MSG(id, (id)cx_class_named("NSNotification"), "notificationWithName:object:", NSWindowWillBeginSheetNotification, parent);
        MSG(void, d, "windowWillBeginSheet:", note);
    }
}
static void begin(id parent, id sheet, id endv, int critical) {
    if (!parent || !sheet) return;
    if (attached(parent)) {           /* já há uma: esta espera (a crítica passa à frente na fila) */
        id q = objc_getAssociatedObject(parent, &k_queue);
        if (!q) { q = MSG0(id, (id)cx_class_named("NSMutableArray"), "array"); objc_setAssociatedObject(parent, &k_queue, q, 1); }
        id two[2] = {sheet, endv};
        id pair = MSG(id, (id)cx_class_named("NSArray"), "arrayWithObjects:count:", (id *)two, (u64)2);
        if (critical) MSG(void, q, "insertObject:atIndex:", pair, (u64)0); else MSG(void, q, "addObject:", pair);
        return;
    }
    present(parent, sheet, endv);
}
static void finish(id parent, id sheet, i64 code) {
    if (!parent || attached(parent) != sheet) return;
    objc_retain(sheet);
    id endv = objc_retain(objc_getAssociatedObject(sheet, &k_handler));
    objc_setAssociatedObject(sheet, &k_handler, 0, 1);
    objc_setAssociatedObject(sheet, &k_parent, 0, 0);
    objc_setAssociatedObject(parent, &k_sheet, 0, 1);
    MSG(void, parent, "removeChildWindow:", sheet);
    MSG(void, sheet, "orderOut:", (id)0);
    MSG(void, parent, "makeKeyAndOrderFront:", (id)0);
    extern id NSWindowDidEndSheetNotification;
    id nc = MSG0(id, (id)cx_class_named("NSNotificationCenter"), "defaultCenter");
    MSG(void, nc, "postNotificationName:object:", NSWindowDidEndSheetNotification, parent);
    id d = MSG0(id, parent, "delegate");
    if (d && cx_responds(object_getClass(d), sl("windowDidEndSheet:")))
    {
        id note = MSG(id, (id)cx_class_named("NSNotification"), "notificationWithName:object:", NSWindowDidEndSheetNotification, parent);
        MSG(void, d, "windowDidEndSheet:", note);
    }
    sheet_end_t *e = endv ? MSG0(sheet_end_t *, endv, "pointerValue") : 0;
    if (e) {
        if (e->block) { void (*inv)(void *, i64) = *(void (**)(void *, i64))((u8 *)e->block + 16); if (inv) inv(e->block, code); }
        if (e->delegate && e->did_end) ((void (*)(id, SEL, id, i64, void *))objc_msgSend)(e->delegate, e->did_end, sheet, code, e->ctx);
        end_free(e);
    }
    if (endv) objc_release(endv);
    objc_release(sheet);
    /* próxima da fila */
    id q = objc_getAssociatedObject(parent, &k_queue);
    if (q && MSG0(u64, q, "count")) {
        id pair = objc_retain(MSG(id, q, "objectAtIndex:", (u64)0));
        MSG(void, q, "removeObjectAtIndex:", (u64)0);
        id nsheet = MSG(id, pair, "objectAtIndex:", (u64)0), nend = MSG(id, pair, "objectAtIndex:", (u64)1);
        present(parent, nsheet, nend);
        objc_release(pair);
    }
}

static void w_begin_sheet(id self, SEL c, id sheet, void *handler) { (void)c; begin(self, sheet, end_box(handler, 0, 0, 0), 0); }
static void w_begin_critical(id self, SEL c, id sheet, void *handler) { (void)c; begin(self, sheet, end_box(handler, 0, 0, 0), 1); }
static void w_end_sheet_code(id self, SEL c, id sheet, i64 code) { (void)c; finish(self, sheet, code); }
static void w_end_sheet(id self, SEL c, id sheet) { (void)c; finish(self, sheet, NSModalResponseStop); }
static id w_attached(id self, SEL c) { (void)c; return attached(self); }
static id w_sheet_parent(id self, SEL c) { (void)c; return parent_of(self); }
static BOOL w_is_sheet(id self, SEL c) { (void)c; return parent_of(self) != 0; }
static id w_sheets(id self, SEL c) {
    (void)c;
    id a = MSG0(id, (id)cx_class_named("NSMutableArray"), "array");
    id s = attached(self);
    if (s) MSG(void, a, "addObject:", s);
    id q = objc_getAssociatedObject(self, &k_queue);
    for (u64 i = 0, n = q ? MSG0(u64, q, "count") : 0; i < n; i++) {
        id pair = MSG(id, q, "objectAtIndex:", i);
        id sh = MSG(id, pair, "objectAtIndex:", (u64)0);
        MSG(void, a, "addObject:", sh);
    }
    return a;
}
/* legado: -[NSApp beginSheet:modalForWindow:modalDelegate:didEndSelector:contextInfo:] e -[NSApp endSheet:...] */
static void ap_begin_sheet(id self, SEL c, id sheet, id win, id delegate, SEL did_end, void *ctx) {
    (void)self; (void)c;
    if (!win) { MSG(i64, NSApp, "runModalForWindow:", sheet); return; }
    begin(win, sheet, end_box(0, delegate, did_end, ctx), 0);
}
static void ap_end_sheet_code(id self, SEL c, id sheet, i64 code) { (void)self; (void)c; finish(parent_of(sheet), sheet, code); }
static void ap_end_sheet(id self, SEL c, id sheet) { (void)self; (void)c; finish(parent_of(sheet), sheet, NSModalResponseStop); }

/* ---- limites de tamanho e nível: guardados por janela; o setFrame: respeita os limites. Mover ou redimensionar uma
 * janela já na tela depende do window server (pedido à frente da Camila). */
typedef struct { double w, h; } S;
static char k_min, k_max, k_cmin, k_cmax, k_level, k_aspect;
static id size_val(S z) { return MSG(id, (id)cx_class_named("NSValue"), "valueWithSize:", z); }
static S val_size(id v, S def) { return v ? ((S (*)(id, SEL))objc_msgSend)(v, sl("sizeValue")) : def; }
static void set_size(id w, char *k, S z) { objc_setAssociatedObject(w, k, size_val(z), 1); }
static S get_size(id w, char *k, S def) { return val_size(objc_getAssociatedObject(w, k), def); }
static void w_set_min(id self, SEL c, S z) { (void)c; set_size(self, &k_min, z); }
static void w_set_max(id self, SEL c, S z) { (void)c; set_size(self, &k_max, z); }
static void w_set_cmin(id self, SEL c, S z) { (void)c; set_size(self, &k_cmin, z); }
static void w_set_cmax(id self, SEL c, S z) { (void)c; set_size(self, &k_cmax, z); }
static S w_min(id self, SEL c) { (void)c; return get_size(self, &k_min, (S){0, 0}); }
static S w_max(id self, SEL c) { (void)c; return get_size(self, &k_max, (S){3.40282347e38, 3.40282347e38}); }
static S w_cmin(id self, SEL c) { (void)c; return get_size(self, &k_cmin, (S){0, 0}); }
static S w_cmax(id self, SEL c) { (void)c; return get_size(self, &k_cmax, (S){3.40282347e38, 3.40282347e38}); }
static void w_set_aspect(id self, SEL c, S z) { (void)c; set_size(self, &k_aspect, z); }
static S w_aspect(id self, SEL c) { (void)c; return get_size(self, &k_aspect, (S){0, 0}); }
static void w_set_level(id self, SEL c, i64 l) { (void)c; objc_setAssociatedObject(self, &k_level, cx_num_i(l), 1); }
static i64 w_level(id self, SEL c) { (void)c; id v = objc_getAssociatedObject(self, &k_level); return v ? MSG0(i64, v, "longLongValue") : 0; }
static double clampd(double v, double lo, double hi) { return v < lo ? lo : v > hi ? hi : v; }
static IMP old_set_frame;
static void w_set_frame_clamped(id self, SEL c, R r, BOOL disp) {
    S mn = w_min(self, 0), mx = w_max(self, 0);
    double nw = clampd(r.w, mn.w, mx.w), nh = clampd(r.h, mn.h, mx.h);
    r.y += r.h - nh;                 /* mantém o alto da janela no lugar */
    r.w = nw; r.h = nh;
    ((void (*)(id, SEL, R, BOOL))old_set_frame)(self, c, r, disp);
}

void cx_appkit_sheets_init(void) {
    cx_class_t *w = cx_class_named("NSWindow"), *app = cx_class_named("NSApplication");
    if (w) {
        cx_add_method(w, "beginSheet:completionHandler:", (void *)w_begin_sheet);
        cx_add_method(w, "beginCriticalSheet:completionHandler:", (void *)w_begin_critical);
        cx_add_method(w, "endSheet:", (void *)w_end_sheet);
        cx_add_method(w, "endSheet:returnCode:", (void *)w_end_sheet_code);
        cx_add_method(w, "attachedSheet", (void *)w_attached);
        cx_add_method(w, "sheetParent", (void *)w_sheet_parent);
        cx_add_method(w, "isSheet", (void *)w_is_sheet);
        cx_add_method(w, "sheets", (void *)w_sheets);
        cx_add_method(w, "setMinSize:", (void *)w_set_min); cx_add_method(w, "minSize", (void *)w_min);
        cx_add_method(w, "setMaxSize:", (void *)w_set_max); cx_add_method(w, "maxSize", (void *)w_max);
        cx_add_method(w, "setContentMinSize:", (void *)w_set_cmin); cx_add_method(w, "contentMinSize", (void *)w_cmin);
        cx_add_method(w, "setContentMaxSize:", (void *)w_set_cmax); cx_add_method(w, "contentMaxSize", (void *)w_cmax);
        cx_add_method(w, "setAspectRatio:", (void *)w_set_aspect); cx_add_method(w, "aspectRatio", (void *)w_aspect);
        cx_add_method(w, "setContentAspectRatio:", (void *)w_set_aspect); cx_add_method(w, "contentAspectRatio", (void *)w_aspect);
        cx_add_method(w, "setLevel:", (void *)w_set_level); cx_add_method(w, "level", (void *)w_level);
        old_set_frame = class_getMethodImplementation(w, sl("setFrame:display:"));
        cx_add_method(w, "setFrame:display:", (void *)w_set_frame_clamped);
    }
    if (app) {
        cx_add_method(app, "beginSheet:modalForWindow:modalDelegate:didEndSelector:contextInfo:", (void *)ap_begin_sheet);
        cx_add_method(app, "endSheet:", (void *)ap_end_sheet);
        cx_add_method(app, "endSheet:returnCode:", (void *)ap_end_sheet_code);
    }
}
