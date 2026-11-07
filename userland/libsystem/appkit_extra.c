/*
 * mOS libSystem — classes do AppKit que o Electron/VS Code importa e o mOS ainda não tinha (inventário do
 * tools/machocheck.c). Antes o carregador as ligava a NSObject: um app que as subclassifica (o Chromium faz
 * isso com NSAnimation, NSAccessibilityElement e NSTitlebarAccessoryViewController) ficava com a hierarquia
 * errada, e +alloc/-init devolviam um NSObject sem os métodos.
 *
 * Todas ficam na superclasse certa, com símbolo exportado. As que o Electron chama de fato têm o
 * comportamento de um Mac sem o recurso:
 *   NSOpenPanel/NSSavePanel: o diálogo é cancelado (NSModalResponseCancel); o Electron devolve "canceled".
 *   NSStatusBar/NSStatusItem: o item existe (botão, menu, título), sem barra de status no mOS.
 *   NSAnimation: termina na hora (progresso 1, animationDidEnd:). NSAnimationContext: grupos imediatos.
 *   NSParagraphStyle/NSMutableParagraphStyle, NSFontManager, NSPrintInfo, NSSpellChecker (sem erros),
 *   NSDocumentController (documentos recentes em memória), NSWorkspaceOpenConfiguration, NSToolbar,
 *   NSBitmapImageRep (buffer RGBA), Touch Bar (objetos sem barra).
 */
#include "foundation.h"

void *calloc(size_t n, size_t s);
void *memset(void *d, int c, size_t n);
int strcmp(const char *a, const char *b);
id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);

typedef struct { double x, y, w, h; } ax_rect_t;
typedef struct { double w, h; } ax_size_t;
typedef struct { long loc, len; } ax_range_t;
typedef struct { void *isa; int flags, reserved; void (*invoke)(void *, ...); } ax_block_t;

#define K(N) cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N); cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N);
K(NSWorkspaceOpenConfiguration) K(NSDocumentController) K(NSAnimationContext) K(NSParagraphStyle) K(NSMutableParagraphStyle)
K(NSSharingService) K(NSSharingServicePicker) K(NSTouchBar) K(NSTouchBarItem) K(NSCustomTouchBarItem) K(NSColorPickerTouchBarItem)
K(NSSliderTouchBarItem) K(NSPopoverTouchBarItem) K(NSGroupTouchBarItem) K(NSCandidateListTouchBarItem) K(NSColorList)
K(NSSegmentedControl) K(NSScrubber) K(NSScrubberItemView) K(NSScrubberTextItemView) K(NSScrubberImageItemView)
K(NSScrubberSelectionStyle) K(NSSavePanel) K(NSOpenPanel) K(NSPopUpButton) K(NSCell) K(NSActionCell) K(NSButtonCell)
K(NSMenuItemCell) K(NSPopUpButtonCell) K(NSStatusBar) K(NSStatusItem) K(NSStatusBarButton) K(NSImageRep) K(NSBitmapImageRep)
K(NSImageSymbolConfiguration) K(NSAccessibilityElement) K(NSAccessibilityRemoteUIElement) K(NSAccessibilityCustomAction)
K(NSSpellChecker) K(NSSpeechSynthesizer) K(NSFontManager) K(NSFontDescriptor) K(NSAnimation)
K(NSTitlebarAccessoryViewController) K(NSToolbar) K(NSTextAttachment) K(NSColorSampler) K(NSPrintInfo) K(NSPrintPanel)
K(NSText) K(NSSecureTextField)

static id cls(const char *n) { return (id)cx_class_named(n); }
static id ret_self(id self, SEL c) { (void)c; return self; }
static id ret_nil(id self, SEL c) { (void)self; (void)c; return 0; }
static void noop1(id self, SEL c, id a) { (void)self; (void)c; (void)a; }
static id new_inst(id k) { return cx_alloc_instance((cx_class_t *)k); }
static id k_new(id k, SEL c) { (void)c; return new_inst(k); }
static id k_new_ar(id k, SEL c) { (void)c; return objc_autorelease(new_inst(k)); }
/* propriedades genéricas guardadas num dicionário por objeto: setX:/x funcionam para qualquer nome */
#define PR_MAX 512
static id pr_obj[PR_MAX], pr_dict[PR_MAX];
static id props(id o, int make) {
    for (int i = 0; i < PR_MAX; i++) if (pr_obj[i] == o) return pr_dict[i];
    if (!make) return 0;
    for (int i = 0; i < PR_MAX; i++) if (!pr_obj[i]) { pr_obj[i] = o; pr_dict[i] = objc_retain(MSG0(id, cls("NSMutableDictionary"), "dictionary")); return pr_dict[i]; }
    return 0;
}
static void pset(id o, const char *k, id v) { id d = props(o, 1); if (!d) return; if (v) MSG(void, d, "setObject:forKey:", v, cx_str(k)); else MSG(void, d, "removeObjectForKey:", cx_str(k)); }
static id pget(id o, const char *k) { id d = props(o, 0); return d ? MSG(id, d, "objectForKey:", cx_str(k)) : 0; }
static i64 pget_i(id o, const char *k, i64 def) { id v = pget(o, k); return v ? MSG0(i64, v, "longLongValue") : def; }
static double pget_d(id o, const char *k, double def) { id v = pget(o, k); return v ? MSG0(double, v, "doubleValue") : def; }
#define OBJ_PROP(name, setter) \
    static id g_##name(id self, SEL c) { (void)c; return pget(self, #name); } \
    static void s_##name(id self, SEL c, id v) { (void)c; pset(self, #name, v); }
#define INT_PROP(name, setter, def) \
    static i64 g_##name(id self, SEL c) { (void)c; return pget_i(self, #name, def); } \
    static void s_##name(id self, SEL c, i64 v) { (void)c; pset(self, #name, cx_num_i(v)); }
#define DBL_PROP(name, setter, def) \
    static double g_##name(id self, SEL c) { (void)c; return pget_d(self, #name, def); } \
    static void s_##name(id self, SEL c, double v) { (void)c; pset(self, #name, cx_num_d(v)); }
#define REG_PROP(k, name, setter) cx_add_method(&k##_c, #name, (void *)g_##name); cx_add_method(&k##_c, #setter ":", (void *)s_##name);

OBJ_PROP(title, setTitle) OBJ_PROP(message, setMessage) OBJ_PROP(prompt, setPrompt) OBJ_PROP(nameFieldStringValue, setNameFieldStringValue)
OBJ_PROP(directoryURL, setDirectoryURL) OBJ_PROP(allowedFileTypes, setAllowedFileTypes) OBJ_PROP(allowedContentTypes, setAllowedContentTypes)
OBJ_PROP(accessoryView, setAccessoryView) OBJ_PROP(delegate, setDelegate) OBJ_PROP(menu, setMenu) OBJ_PROP(image, setImage)
OBJ_PROP(toolTip, setToolTip) OBJ_PROP(identifier, setIdentifier) OBJ_PROP(view, setView) OBJ_PROP(items, setItems)
OBJ_PROP(defaultItemIdentifiers, setDefaultItemIdentifiers) OBJ_PROP(customizationLabel, setCustomizationLabel)
OBJ_PROP(principalItemIdentifier, setPrincipalItemIdentifier) OBJ_PROP(representedObject, setRepresentedObject)
OBJ_PROP(attributedTitle, setAttributedTitle) OBJ_PROP(tabStops, setTabStops) OBJ_PROP(accessibilityLabel, setAccessibilityLabel)
OBJ_PROP(accessibilityParent, setAccessibilityParent) OBJ_PROP(accessibilityRole, setAccessibilityRole)
INT_PROP(alignment, setAlignment, 4) INT_PROP(lineBreakMode, setLineBreakMode, 0) INT_PROP(baseWritingDirection, setBaseWritingDirection, -1)
INT_PROP(layoutAttribute, setLayoutAttribute, 0) INT_PROP(segmentCount, setSegmentCount, 0) INT_PROP(selectedSegment, setSelectedSegment, -1)
INT_PROP(animationCurve, setAnimationCurve, 0) INT_PROP(animationBlockingMode, setAnimationBlockingMode, 0)
INT_PROP(canChooseFiles, setCanChooseFiles, 1) INT_PROP(canChooseDirectories, setCanChooseDirectories, 0)
INT_PROP(allowsMultipleSelection, setAllowsMultipleSelection, 0) INT_PROP(canCreateDirectories, setCanCreateDirectories, 1)
INT_PROP(showsHiddenFiles, setShowsHiddenFiles, 0) INT_PROP(treatsFilePackagesAsDirectories, setTreatsFilePackagesAsDirectories, 0)
INT_PROP(visible, setVisible, 1) INT_PROP(activates, setActivates, 1) INT_PROP(promptsUserIfNeeded, setPromptsUserIfNeeded, 1)
INT_PROP(createsNewApplicationInstance, setCreatesNewApplicationInstance, 0) INT_PROP(hides, setHides, 0)
INT_PROP(allowsImplicitAnimation, setAllowsImplicitAnimation, 0)
DBL_PROP(lineSpacing, setLineSpacing, 0) DBL_PROP(paragraphSpacing, setParagraphSpacing, 0) DBL_PROP(headIndent, setHeadIndent, 0)
DBL_PROP(tailIndent, setTailIndent, 0) DBL_PROP(firstLineHeadIndent, setFirstLineHeadIndent, 0) DBL_PROP(minimumLineHeight, setMinimumLineHeight, 0)
DBL_PROP(maximumLineHeight, setMaximumLineHeight, 0) DBL_PROP(lineHeightMultiple, setLineHeightMultiple, 0)
DBL_PROP(duration, setDuration, 0.25) DBL_PROP(frameRate, setFrameRate, 60) DBL_PROP(length, setLength, -1)

/* ---- NSParagraphStyle */
static id ps_default(id k, SEL c) { (void)k; (void)c; static id d; if (!d) d = new_inst((id)&NSParagraphStyle_c); return d; }
static id ps_mcopy(id self, SEL c) {
    (void)c;
    id n = new_inst((id)&NSMutableParagraphStyle_c);
    id d = props(self, 0);
    if (d) { id nd = props(n, 1); MSG(void, nd, "addEntriesFromDictionary:", d); }
    return n;
}
static id ps_copy(id self, SEL c) { (void)c; return objc_retain(self); }

/* ---- painéis de abrir/salvar: cancelados */
static id panel_new(id k, SEL c) { (void)c; return objc_autorelease(new_inst(k)); }
static i64 panel_run(id self, SEL c) { (void)self; (void)c; return 0; /* NSModalResponseCancel */ }
static void panel_begin(id self, SEL c, void *blk) { (void)self; (void)c; if (blk) ((ax_block_t *)blk)->invoke(blk, (i64)0); }
static void panel_begin_sheet(id self, SEL c, id win, void *blk) { (void)win; panel_begin(self, c, blk); }
static id panel_urls(id self, SEL c) { (void)self; (void)c; return objc_autorelease(cx_arr_new(0)); }

/* ---- barra de status */
static id sb_system(id k, SEL c) { (void)c; static id s; if (!s) s = new_inst(k); return s; }
static id sb_item(id self, SEL c, double len) {
    (void)self; (void)c;
    id it = new_inst((id)&NSStatusItem_c);
    pset(it, "length", cx_num_d(len));
    id b = MSG0(id, MSG0(id, (id)&NSStatusBarButton_c, "alloc"), "init");
    pset(it, "button", b);
    return it;
}
static void sb_remove(id self, SEL c, id it) { (void)self; (void)c; (void)it; }
static double sb_thickness(id self, SEL c) { (void)self; (void)c; return 24; }
static id si_button(id self, SEL c) { (void)c; return pget(self, "button"); }
static id si_bar(id self, SEL c) { (void)self; (void)c; return sb_system((id)&NSStatusBar_c, 0); }

/* ---- animações: terminam na hora */
static id an_init(id self, SEL c, double dur, i64 curve) { (void)c; s_duration(self, 0, dur); s_animationCurve(self, 0, curve); return self; }
static double an_progress(id self, SEL c) { (void)c; return pget_d(self, "progress", 0); }
static void an_set_progress(id self, SEL c, double p) { (void)c; pset(self, "progress", cx_num_d(p)); }
static void an_start(id self, SEL c) {
    (void)c;
    id d = pget(self, "delegate");
    if (d && cx_responds(object_getClass(d), sel_registerName("animationShouldStart:")) && !MSG(BOOL, d, "animationShouldStart:", self)) return;
    MSG(void, self, "setCurrentProgress:", 1.0);
    if (d && cx_responds(object_getClass(d), sel_registerName("animationDidEnd:"))) MSG(void, d, "animationDidEnd:", self);
}
static void an_stop(id self, SEL c) {
    (void)c;
    id d = pget(self, "delegate");
    if (d && cx_responds(object_getClass(d), sel_registerName("animationDidStop:"))) MSG(void, d, "animationDidStop:", self);
}
static BOOL an_animating(id self, SEL c) { (void)self; (void)c; return NO; }
static id ac_current(id k, SEL c) { (void)c; static id s; if (!s) s = new_inst(k); return s; }
static void ac_run(id k, SEL c, void *changes, void *done) {
    (void)c;
    if (changes) ((ax_block_t *)changes)->invoke(changes, ac_current(k, 0));
    if (done) ((ax_block_t *)done)->invoke(done);
}
static void ac_run1(id k, SEL c, void *changes) { ac_run(k, c, changes, 0); }
static void ac_group(id k, SEL c) { (void)k; (void)c; }

/* ---- serviços: fontes, ortografia, impressão, documentos recentes */
static id fm_shared(id k, SEL c) { (void)c; static id s; if (!s) s = new_inst(k); return s; }
static id fm_families(id self, SEL c) {
    (void)self; (void)c;
    id a = cx_arr_new(0);
    cx_arr_add(a, cx_str("Inter")); cx_arr_add(a, cx_str(".AppleSystemUIFont")); cx_arr_add(a, cx_str("Menlo"));
    return objc_autorelease(a);
}
static id fm_font_family(id self, SEL c, id fam, u64 traits, i64 weight, double size) {
    (void)self; (void)c; (void)fam; (void)traits; (void)weight;
    return MSG(id, cls("NSFont"), "systemFontOfSize:", size);
}
static id fm_convert(id self, SEL c, id font, u64 trait) { (void)self; (void)c; (void)trait; return font; }
static u64 fm_traits(id self, SEL c, id font) { (void)self; (void)c; (void)font; return 0; }
static i64 fm_weight(id self, SEL c, id font) { (void)self; (void)c; (void)font; return 5; }
static id sc_shared(id k, SEL c) { (void)c; static id s; if (!s) s = new_inst(k); return s; }
static ax_range_t sc_check(id self, SEL c, id s, i64 start) { (void)self; (void)c; (void)s; (void)start; ax_range_t r = {0x7fffffffffffffffL, 0}; return r; }
static id sc_guesses(id self, SEL c, ax_range_t r, id s, id lang, i64 tag) { (void)self; (void)c; (void)r; (void)s; (void)lang; (void)tag; return objc_autorelease(cx_arr_new(0)); }
static id sc_langs(id self, SEL c) { (void)self; (void)c; id a = cx_arr_new(0); cx_arr_add(a, cx_str("pt_BR")); cx_arr_add(a, cx_str("en")); return objc_autorelease(a); }
static id sc_lang(id self, SEL c) { (void)self; (void)c; return cx_str("pt_BR"); }
static BOOL sc_set_lang(id self, SEL c, id l) { (void)self; (void)c; (void)l; return YES; }
static BOOL sc_automatic(id self, SEL c) { (void)self; (void)c; return NO; }
static id pi_shared(id k, SEL c) { (void)c; static id s; if (!s) s = new_inst(k); return s; }
static ax_size_t pi_paper(id self, SEL c) { (void)self; (void)c; ax_size_t s = {595, 842}; return s; }   /* A4 em pontos */
static double pi_margin(id self, SEL c) { (void)self; (void)c; return 36; }
static id pi_dict(id self, SEL c) { (void)c; id d = props(self, 1); return d; }
static id dc_shared(id k, SEL c) { (void)c; static id s; if (!s) s = new_inst(k); return s; }
static void dc_note_recent(id self, SEL c, id url) {
    (void)c;
    id a = pget(self, "recent");
    if (!a) { a = cx_arr_new(0); pset(self, "recent", a); objc_release(a); }
    if (url && !MSG(BOOL, a, "containsObject:", url)) MSG(void, a, "insertObject:atIndex:", url, (u64)0);
}
static id dc_recent(id self, SEL c) { (void)c; id a = pget(self, "recent"); return a ? a : objc_autorelease(cx_arr_new(0)); }
static void dc_clear(id self, SEL c, id sender) { (void)c; (void)sender; pset(self, "recent", 0); }
static u64 dc_max(id self, SEL c) { (void)self; (void)c; return 10; }
static id woc_config(id k, SEL c) { (void)c; return objc_autorelease(new_inst(k)); }
/* -[NSWorkspace openURL:configuration:completionHandler:] e openApplicationAtURL: com a configuração */
static void ws_open_cfg(id self, SEL c, id url, id cfg, void *done) {
    (void)c; (void)cfg;
    BOOL ok = url && MSG(BOOL, self, "openURL:", url);
    if (done) ((ax_block_t *)done)->invoke(done, (id)0, ok ? (id)0 : (id)0);
}

/* ---- NSBitmapImageRep: buffer RGBA */
typedef struct { void *isa; u8 *data; i64 w, h, bpr; } bir_t;
static id bir_init_planes(id self, SEL c, u8 **planes, i64 w, i64 h, i64 bps, i64 spp, BOOL alpha, BOOL planar, id space, i64 bpr, i64 bpp) {
    (void)c; (void)bps; (void)alpha; (void)planar; (void)space; (void)bpp;
    bir_t *b = (bir_t *)self;
    b->w = w; b->h = h; b->bpr = bpr ? bpr : w * (spp ? spp : 4);
    b->data = planes && planes[0] ? planes[0] : calloc((size_t)(b->bpr * (h ? h : 1)), 1);
    return self;
}
static u8 *bir_data(id self, SEL c) { (void)c; return ((bir_t *)self)->data; }
static i64 bir_w(id self, SEL c) { (void)c; return ((bir_t *)self)->w; }
static i64 bir_h(id self, SEL c) { (void)c; return ((bir_t *)self)->h; }
static i64 bir_bpr(id self, SEL c) { (void)c; return ((bir_t *)self)->bpr; }
static ax_size_t bir_size(id self, SEL c) { (void)c; ax_size_t s = {(double)((bir_t *)self)->w, (double)((bir_t *)self)->h}; return s; }
static id bir_repr(id self, SEL c, u64 type, id props_) { (void)self; (void)c; (void)type; (void)props_; return 0; }   /* sem codificador PNG/JPEG aqui */

/* ---- toolbar e Touch Bar */
static id tb_init_ident(id self, SEL c, id ident) { (void)c; pset(self, "identifier", ident); return self; }
static id tbi_init_ident(id self, SEL c, id ident) { (void)c; pset(self, "identifier", ident); return self; }

/* ---- NSViewController */
static id vc_init_nib(id self, SEL c, id nib, id bundle) { (void)c; (void)nib; (void)bundle; return self; }
static void vc_load(id self, SEL c) { (void)c; if (!pget(self, "view")) { id v = MSG0(id, MSG0(id, cls("NSView"), "alloc"), "init"); pset(self, "view", v); objc_release(v); } }
static id vc_view(id self, SEL c) { (void)c; if (!pget(self, "view")) MSG0(void, self, "loadView"); return pget(self, "view"); }

/* ---- acessibilidade */
static id ae_with_role(id k, SEL c, id role, ax_rect_t frame, id label, id parent) {
    (void)c; (void)frame;
    id e = objc_autorelease(new_inst(k));
    pset(e, "accessibilityRole", role); pset(e, "accessibilityLabel", label); pset(e, "accessibilityParent", parent);
    return e;
}
static id aca_init_name(id self, SEL c, id name, id target, SEL sel) { (void)c; (void)target; (void)sel; pset(self, "title", name); return self; }

/* ---- registro */
static void def(cx_class_t *k, cx_class_t *m, const char *name, const char *super) {
    if (cx_class_named(name)) return;
    cx_class_t *s = cx_class_named(super);
    if (!s) s = cx_class_named("NSObject");
    cx_define_class(k, m, name, s, 64);
    cx_add_cmethod(k, "new", (void *)k_new);
}
#define DEF(N, S) def(&N##_c, &N##_m, #N, S)
#define IM(k, s, f) cx_add_method(&k##_c, s, (void *)(f))
#define CM(k, s, f) cx_add_cmethod(&k##_c, s, (void *)(f))
void cx_appkit_extra_init(void) {
    DEF(NSCell, "NSObject"); DEF(NSActionCell, "NSCell"); DEF(NSButtonCell, "NSActionCell");
    DEF(NSMenuItemCell, "NSButtonCell"); DEF(NSPopUpButtonCell, "NSMenuItemCell");
    DEF(NSTitlebarAccessoryViewController, "NSViewController");
    REG_PROP(NSTitlebarAccessoryViewController, layoutAttribute, setLayoutAttribute);

    DEF(NSParagraphStyle, "NSObject"); DEF(NSMutableParagraphStyle, "NSParagraphStyle");
    CM(NSParagraphStyle, "defaultParagraphStyle", ps_default);
    IM(NSParagraphStyle, "mutableCopy", ps_mcopy); IM(NSParagraphStyle, "copy", ps_copy); IM(NSMutableParagraphStyle, "copy", ps_mcopy);
    REG_PROP(NSParagraphStyle, alignment, setAlignment); REG_PROP(NSParagraphStyle, lineBreakMode, setLineBreakMode);
    REG_PROP(NSParagraphStyle, baseWritingDirection, setBaseWritingDirection); REG_PROP(NSParagraphStyle, lineSpacing, setLineSpacing);
    REG_PROP(NSParagraphStyle, paragraphSpacing, setParagraphSpacing); REG_PROP(NSParagraphStyle, headIndent, setHeadIndent);
    REG_PROP(NSParagraphStyle, tailIndent, setTailIndent); REG_PROP(NSParagraphStyle, firstLineHeadIndent, setFirstLineHeadIndent);
    REG_PROP(NSParagraphStyle, minimumLineHeight, setMinimumLineHeight); REG_PROP(NSParagraphStyle, maximumLineHeight, setMaximumLineHeight);
    REG_PROP(NSParagraphStyle, lineHeightMultiple, setLineHeightMultiple); REG_PROP(NSParagraphStyle, tabStops, setTabStops);

    DEF(NSSavePanel, "NSPanel"); DEF(NSOpenPanel, "NSSavePanel");
    CM(NSSavePanel, "savePanel", panel_new); CM(NSOpenPanel, "openPanel", panel_new);
    IM(NSSavePanel, "runModal", panel_run); IM(NSSavePanel, "beginWithCompletionHandler:", panel_begin);
    IM(NSSavePanel, "beginSheetModalForWindow:completionHandler:", panel_begin_sheet);
    IM(NSSavePanel, "URL", ret_nil); IM(NSOpenPanel, "URLs", panel_urls);
    REG_PROP(NSSavePanel, title, setTitle); REG_PROP(NSSavePanel, message, setMessage); REG_PROP(NSSavePanel, prompt, setPrompt);
    REG_PROP(NSSavePanel, nameFieldStringValue, setNameFieldStringValue); REG_PROP(NSSavePanel, directoryURL, setDirectoryURL);
    REG_PROP(NSSavePanel, allowedFileTypes, setAllowedFileTypes); REG_PROP(NSSavePanel, allowedContentTypes, setAllowedContentTypes);
    REG_PROP(NSSavePanel, accessoryView, setAccessoryView); REG_PROP(NSSavePanel, delegate, setDelegate);
    REG_PROP(NSSavePanel, canCreateDirectories, setCanCreateDirectories); REG_PROP(NSSavePanel, showsHiddenFiles, setShowsHiddenFiles);
    REG_PROP(NSSavePanel, treatsFilePackagesAsDirectories, setTreatsFilePackagesAsDirectories);
    REG_PROP(NSOpenPanel, canChooseFiles, setCanChooseFiles); REG_PROP(NSOpenPanel, canChooseDirectories, setCanChooseDirectories);
    REG_PROP(NSOpenPanel, allowsMultipleSelection, setAllowsMultipleSelection);

    DEF(NSPopUpButton, "NSButton"); DEF(NSSegmentedControl, "NSControl"); DEF(NSText, "NSView"); DEF(NSSecureTextField, "NSTextField");
    REG_PROP(NSSegmentedControl, segmentCount, setSegmentCount); REG_PROP(NSSegmentedControl, selectedSegment, setSelectedSegment);

    DEF(NSStatusBar, "NSObject"); DEF(NSStatusItem, "NSObject"); DEF(NSStatusBarButton, "NSButton");
    CM(NSStatusBar, "systemStatusBar", sb_system); IM(NSStatusBar, "statusItemWithLength:", sb_item);
    IM(NSStatusBar, "removeStatusItem:", sb_remove); IM(NSStatusBar, "thickness", sb_thickness);
    IM(NSStatusItem, "button", si_button); IM(NSStatusItem, "statusBar", si_bar);
    REG_PROP(NSStatusItem, menu, setMenu); REG_PROP(NSStatusItem, length, setLength); REG_PROP(NSStatusItem, visible, setVisible);
    REG_PROP(NSStatusItem, title, setTitle); REG_PROP(NSStatusItem, image, setImage); REG_PROP(NSStatusItem, toolTip, setToolTip);

    DEF(NSAnimation, "NSObject"); DEF(NSAnimationContext, "NSObject");
    IM(NSAnimation, "initWithDuration:animationCurve:", an_init); IM(NSAnimation, "startAnimation", an_start);
    IM(NSAnimation, "stopAnimation", an_stop); IM(NSAnimation, "isAnimating", an_animating);
    IM(NSAnimation, "currentProgress", an_progress); IM(NSAnimation, "setCurrentProgress:", an_set_progress);
    REG_PROP(NSAnimation, duration, setDuration); REG_PROP(NSAnimation, frameRate, setFrameRate);
    REG_PROP(NSAnimation, delegate, setDelegate); REG_PROP(NSAnimation, animationCurve, setAnimationCurve);
    REG_PROP(NSAnimation, animationBlockingMode, setAnimationBlockingMode);
    CM(NSAnimationContext, "currentContext", ac_current); CM(NSAnimationContext, "runAnimationGroup:completionHandler:", ac_run);
    CM(NSAnimationContext, "runAnimationGroup:", ac_run1); CM(NSAnimationContext, "beginGrouping", ac_group); CM(NSAnimationContext, "endGrouping", ac_group);
    REG_PROP(NSAnimationContext, duration, setDuration); REG_PROP(NSAnimationContext, allowsImplicitAnimation, setAllowsImplicitAnimation);

    DEF(NSFontManager, "NSObject"); DEF(NSFontDescriptor, "NSObject");
    CM(NSFontManager, "sharedFontManager", fm_shared); IM(NSFontManager, "availableFontFamilies", fm_families);
    IM(NSFontManager, "availableFonts", fm_families); IM(NSFontManager, "fontWithFamily:traits:weight:size:", fm_font_family);
    IM(NSFontManager, "convertFont:toHaveTrait:", fm_convert); IM(NSFontManager, "convertFont:toNotHaveTrait:", fm_convert);
    IM(NSFontManager, "traitsOfFont:", fm_traits); IM(NSFontManager, "weightOfFont:", fm_weight);
    DEF(NSSpellChecker, "NSObject");
    CM(NSSpellChecker, "sharedSpellChecker", sc_shared); CM(NSSpellChecker, "sharedSpellCheckerExists", sc_automatic);
    CM(NSSpellChecker, "isAutomaticTextReplacementEnabled", sc_automatic); CM(NSSpellChecker, "isAutomaticSpellingCorrectionEnabled", sc_automatic);
    IM(NSSpellChecker, "checkSpellingOfString:startingAt:", sc_check);
    IM(NSSpellChecker, "guessesForWordRange:inString:language:inSpellDocumentWithTag:", sc_guesses);
    IM(NSSpellChecker, "availableLanguages", sc_langs); IM(NSSpellChecker, "userPreferredLanguages", sc_langs);
    IM(NSSpellChecker, "language", sc_lang); IM(NSSpellChecker, "setLanguage:", sc_set_lang);
    IM(NSSpellChecker, "automaticallyIdentifiesLanguages", sc_automatic);
    DEF(NSPrintInfo, "NSObject"); DEF(NSPrintPanel, "NSObject");
    CM(NSPrintInfo, "sharedPrintInfo", pi_shared); IM(NSPrintInfo, "paperSize", pi_paper);
    IM(NSPrintInfo, "leftMargin", pi_margin); IM(NSPrintInfo, "rightMargin", pi_margin); IM(NSPrintInfo, "topMargin", pi_margin);
    IM(NSPrintInfo, "bottomMargin", pi_margin); IM(NSPrintInfo, "dictionary", pi_dict); IM(NSPrintInfo, "printSettings", pi_dict);
    CM(NSPrintPanel, "printPanel", k_new_ar); IM(NSPrintPanel, "runModalWithPrintInfo:", panel_run);
    DEF(NSDocumentController, "NSObject");
    CM(NSDocumentController, "sharedDocumentController", dc_shared); IM(NSDocumentController, "noteNewRecentDocumentURL:", dc_note_recent);
    IM(NSDocumentController, "recentDocumentURLs", dc_recent); IM(NSDocumentController, "clearRecentDocuments:", dc_clear);
    IM(NSDocumentController, "maximumRecentDocumentCount", dc_max);
    DEF(NSWorkspaceOpenConfiguration, "NSObject");
    CM(NSWorkspaceOpenConfiguration, "configuration", woc_config);
    REG_PROP(NSWorkspaceOpenConfiguration, activates, setActivates); REG_PROP(NSWorkspaceOpenConfiguration, promptsUserIfNeeded, setPromptsUserIfNeeded);
    REG_PROP(NSWorkspaceOpenConfiguration, createsNewApplicationInstance, setCreatesNewApplicationInstance);
    REG_PROP(NSWorkspaceOpenConfiguration, hides, setHides);
    { cx_class_t *ws = cx_class_named("NSWorkspace"); if (ws) cx_add_method(ws, "openURL:configuration:completionHandler:", (void *)ws_open_cfg); }

    DEF(NSImageRep, "NSObject"); DEF(NSBitmapImageRep, "NSImageRep");
    IM(NSBitmapImageRep, "initWithBitmapDataPlanes:pixelsWide:pixelsHigh:bitsPerSample:samplesPerPixel:hasAlpha:isPlanar:colorSpaceName:bytesPerRow:bitsPerPixel:", bir_init_planes);
    IM(NSBitmapImageRep, "bitmapData", bir_data); IM(NSBitmapImageRep, "pixelsWide", bir_w); IM(NSBitmapImageRep, "pixelsHigh", bir_h);
    IM(NSBitmapImageRep, "bytesPerRow", bir_bpr); IM(NSBitmapImageRep, "size", bir_size);
    IM(NSBitmapImageRep, "representationUsingType:properties:", bir_repr);
    DEF(NSImageSymbolConfiguration, "NSObject"); DEF(NSColorList, "NSObject"); DEF(NSTextAttachment, "NSObject");
    DEF(NSColorSampler, "NSObject"); DEF(NSSpeechSynthesizer, "NSObject");
    REG_PROP(NSTextAttachment, image, setImage);

    DEF(NSToolbar, "NSObject");
    IM(NSToolbar, "initWithIdentifier:", tb_init_ident); REG_PROP(NSToolbar, delegate, setDelegate);
    REG_PROP(NSToolbar, visible, setVisible); REG_PROP(NSToolbar, items, setItems);
    DEF(NSTouchBar, "NSObject"); DEF(NSTouchBarItem, "NSObject");
    DEF(NSCustomTouchBarItem, "NSTouchBarItem"); DEF(NSColorPickerTouchBarItem, "NSTouchBarItem"); DEF(NSSliderTouchBarItem, "NSTouchBarItem");
    DEF(NSPopoverTouchBarItem, "NSTouchBarItem"); DEF(NSGroupTouchBarItem, "NSTouchBarItem"); DEF(NSCandidateListTouchBarItem, "NSTouchBarItem");
    REG_PROP(NSTouchBar, delegate, setDelegate); REG_PROP(NSTouchBar, defaultItemIdentifiers, setDefaultItemIdentifiers);
    REG_PROP(NSTouchBar, customizationLabel, setCustomizationLabel); REG_PROP(NSTouchBar, principalItemIdentifier, setPrincipalItemIdentifier);
    IM(NSTouchBarItem, "initWithIdentifier:", tbi_init_ident); REG_PROP(NSTouchBarItem, identifier, setIdentifier);
    REG_PROP(NSCustomTouchBarItem, view, setView);
    DEF(NSScrubber, "NSView"); DEF(NSScrubberItemView, "NSView"); DEF(NSScrubberTextItemView, "NSScrubberItemView");
    DEF(NSScrubberImageItemView, "NSScrubberItemView"); DEF(NSScrubberSelectionStyle, "NSObject");
    DEF(NSSharingService, "NSObject"); DEF(NSSharingServicePicker, "NSObject");

    DEF(NSAccessibilityElement, "NSObject"); DEF(NSAccessibilityRemoteUIElement, "NSObject"); DEF(NSAccessibilityCustomAction, "NSObject");
    CM(NSAccessibilityElement, "accessibilityElementWithRole:frame:label:parent:", ae_with_role);
    REG_PROP(NSAccessibilityElement, accessibilityLabel, setAccessibilityLabel); REG_PROP(NSAccessibilityElement, accessibilityRole, setAccessibilityRole);
    REG_PROP(NSAccessibilityElement, accessibilityParent, setAccessibilityParent);
    IM(NSAccessibilityCustomAction, "initWithName:target:selector:", aca_init_name); REG_PROP(NSAccessibilityCustomAction, title, setTitle);
    (void)vc_init_nib; (void)vc_load; (void)vc_view; (void)g_representedObject; (void)s_representedObject;
    (void)ret_self; (void)noop1; (void)memset; (void)strcmp;
}
