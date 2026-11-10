/* QuickLookUI no libSystem.
 *
 * QLPreviewPanel já é classe (vscode_fwclasses.c, superclasse NSPanel). Os
 * métodos abaixo não abrem janela, não entram em loop modal e não chamam o
 * NSWindow: makeKeyAndOrderFront: só marca o painel visível e volta.
 * reloadData pergunta a contagem uma vez e, no máximo, o item 0.
 */
#include "foundation.h"
#include "libc.h"

void cx_mark_stub_class(cx_class_t *c);
id objc_retain(id o);

extern cx_class_t QLPreviewPanel_c __asm__("OBJC_CLASS_$_QLPreviewPanel");
extern cx_class_t NSView_c __asm__("OBJC_CLASS_$_NSView");

cx_class_t QLPreviewView_c __asm__("OBJC_CLASS_$_QLPreviewView");
static cx_class_t QLPreviewView_m __asm__("OBJC_METACLASS_$_QLPreviewView");

static id g_panel, g_ds, g_del, g_item;
static int g_exists, g_visible, g_auto;
static long g_index, g_count;

static int responds(id o, const char *sel) {
    return o && cx_responds(o->isa, sel_registerName(sel));
}

static id ql_shared(id cls, SEL c) {
    (void)c;
    g_exists = 1;
    if (!g_panel) g_panel = cx_alloc_instance((cx_class_t *)cls);
    return g_panel;
}
static BOOL ql_exists(id cls, SEL c) { (void)cls; (void)c; return g_exists ? YES : NO; }
static BOOL ql_visible(id self, SEL c) { (void)self; (void)c; return g_visible ? YES : NO; }
static void ql_front(id self, SEL c, id sender) { (void)self; (void)c; (void)sender; g_visible = 1; }
static void ql_out(id self, SEL c, id sender) { (void)self; (void)c; (void)sender; g_visible = 0; }
static void ql_close(id self, SEL c) { (void)self; (void)c; g_visible = 0; }
static void ql_set_ds(id self, SEL c, id ds) { (void)self; (void)c; g_ds = ds ? objc_retain(ds) : 0; }
static id ql_ds(id self, SEL c) { (void)self; (void)c; return g_ds; }
static void ql_set_del(id self, SEL c, id d) { (void)self; (void)c; g_del = d ? objc_retain(d) : 0; }
static id ql_del(id self, SEL c) { (void)self; (void)c; return g_del; }
static void ql_set_index(id self, SEL c, long i) { (void)self; (void)c; g_index = i < 0 ? 0 : i; }
static long ql_index(id self, SEL c) { (void)self; (void)c; return g_index; }
static void ql_reload(id self, SEL c) {
    (void)c;
    g_count = 0;
    g_item = 0;
    if (g_ds && responds(g_ds, "numberOfPreviewItemsInPreviewPanel:"))
        g_count = MSG(long, g_ds, "numberOfPreviewItemsInPreviewPanel:", self);
    if (g_count < 0) g_count = 0;
    if (g_count > 0 && g_ds && responds(g_ds, "previewItemAtIndex:"))
        g_item = MSG(id, g_ds, "previewItemAtIndex:", (long)0);
    if (g_item && responds(g_item, "previewItemURL"))
        (void)MSG0(id, g_item, "previewItemURL");
}
static void ql_refresh(id self, SEL c) { ql_reload(self, c); }
static id ql_current(id self, SEL c) {
    (void)c;
    if (!g_ds || g_index < 0 || !responds(g_ds, "previewItemAtIndex:")) return g_item;
    if (g_index == 0) return g_item;
    return MSG(id, g_ds, "previewItemAtIndex:", g_index);
}
static void ql_update(id self, SEL c) { (void)self; (void)c; }
static BOOL ql_no(id self, SEL c, id sender) { (void)self; (void)c; (void)sender; return NO; }
static BOOL ql_fs(id self, SEL c) { (void)self; (void)c; return NO; }

static id qv_init(id self, SEL c) { (void)c; return self; }
static void qv_refresh(id self, SEL c) { (void)self; (void)c; }
static void qv_close(id self, SEL c) { (void)self; (void)c; }
static id qv_item(id self, SEL c) { (void)self; (void)c; return 0; }
static void qv_set_item(id self, SEL c, id item) { (void)self; (void)c; (void)item; }
static BOOL qv_auto(id self, SEL c) { (void)self; (void)c; return g_auto ? YES : NO; }
static void qv_set_auto(id self, SEL c, BOOL v) { (void)self; (void)c; g_auto = v ? 1 : 0; }

void cx_quicklook_init(void) {
    cx_define_class(&QLPreviewView_c, &QLPreviewView_m, "QLPreviewView", &NSView_c, 0);
    cx_mark_stub_class(&QLPreviewView_c);
    cx_add_cmethod(&QLPreviewPanel_c, "sharedPreviewPanel", (void *)ql_shared);
    cx_add_cmethod(&QLPreviewPanel_c, "sharedPreviewPanelExists", (void *)ql_exists);
    cx_add_method(&QLPreviewPanel_c, "isVisible", (void *)ql_visible);
    cx_add_method(&QLPreviewPanel_c, "makeKeyAndOrderFront:", (void *)ql_front);
    cx_add_method(&QLPreviewPanel_c, "orderFront:", (void *)ql_front);
    cx_add_method(&QLPreviewPanel_c, "orderOut:", (void *)ql_out);
    cx_add_method(&QLPreviewPanel_c, "close", (void *)ql_close);
    cx_add_method(&QLPreviewPanel_c, "setDataSource:", (void *)ql_set_ds);
    cx_add_method(&QLPreviewPanel_c, "dataSource", (void *)ql_ds);
    cx_add_method(&QLPreviewPanel_c, "setDelegate:", (void *)ql_set_del);
    cx_add_method(&QLPreviewPanel_c, "delegate", (void *)ql_del);
    cx_add_method(&QLPreviewPanel_c, "setCurrentPreviewItemIndex:", (void *)ql_set_index);
    cx_add_method(&QLPreviewPanel_c, "currentPreviewItemIndex", (void *)ql_index);
    cx_add_method(&QLPreviewPanel_c, "currentPreviewItem", (void *)ql_current);
    cx_add_method(&QLPreviewPanel_c, "reloadData", (void *)ql_reload);
    cx_add_method(&QLPreviewPanel_c, "refreshCurrentPreviewItem", (void *)ql_refresh);
    cx_add_method(&QLPreviewPanel_c, "updateController", (void *)ql_update);
    cx_add_method(&QLPreviewPanel_c, "enterFullScreenMode:withOptions:", (void *)ql_no);
    cx_add_method(&QLPreviewPanel_c, "exitFullScreenModeWithOptions:", (void *)ql_out);
    cx_add_method(&QLPreviewPanel_c, "isInFullScreenMode", (void *)ql_fs);
    cx_add_method(&QLPreviewView_c, "init", (void *)qv_init);
    cx_add_method(&QLPreviewView_c, "refreshPreviewItem", (void *)qv_refresh);
    cx_add_method(&QLPreviewView_c, "close", (void *)qv_close);
    cx_add_method(&QLPreviewView_c, "previewItem", (void *)qv_item);
    cx_add_method(&QLPreviewView_c, "setPreviewItem:", (void *)qv_set_item);
    cx_add_method(&QLPreviewView_c, "autostarts", (void *)qv_auto);
    cx_add_method(&QLPreviewView_c, "setAutostarts:", (void *)qv_set_auto);
}
