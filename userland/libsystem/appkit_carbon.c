/*
 * mOS libSystem — pedaços do Carbon/ApplicationServices que os apps do corpus importam (Firefox, Krita, MuseScore,
 * OBS, LibreOffice, iTerm2, alt-tab... — docs/lacunas/beatriz.md):
 *   Pasteboard Manager (PasteboardCreate, Synchronize, Clear, Get…, Copy…, Put… e promessas) sobre o NSPasteboard;
 *   Text Input Sources que faltavam (TISCreateInputSourceList, TISSelectInputSource, propriedades e tipos);
 *   Process Manager (GetCurrentProcess, GetNextProcess, SetFrontProcess, ProcessInformationCopyDictionary)
 *     sobre o NSRunningApplication;
 *   eventos (GetEventDispatcherTarget, GetCurrentEventKeyModifiers/ButtonState, GetEventClass), TSMGetActiveDocument,
 *   SetSystemUIMode; HITheme (botão, moldura, menu, texto) e GetThemeMetric; CGDisplayFade; PlotIconRefInContext.
 * O resto do Carbon (InstallEventHandler, TISCopyCurrentKeyboardInputSource...) já existia em vscode_frameworks.c.
 */
#include "foundation.h"
#include "sys.h"
#include "libc.h"
extern const char kCFTypeArrayCallBacks[];
extern const char kCFTypeDictionaryKeyCallBacks[], kCFTypeDictionaryValueCallBacks[];   /* cfcollections.c: callbacks de objeto CF (retain/isEqual:) */

typedef int OSStatus;
id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void free(void *p);
extern id NSApp;
id CFArrayCreate(void *alloc, const void **vals, long n, const void *cb);
void CFDictionarySetValue(id d, const void *k, const void *v);
const void *CFDictionaryGetValue(id d, const void *k);
id CFDictionaryCreateMutable(void *alloc, long cap, const void *kcb, const void *vcb);
id TISCopyCurrentKeyboardInputSource(void);
id cx_color(double r, double g, double b, double a);

typedef struct { double x, y, w, h; } R;
void CGContextFillRect(id c, R r);
void CGContextStrokeRectWithWidth(id c, R r, double w);
void CGContextSetRGBFillColor(id c, double r, double g, double b, double a);
void CGContextSetRGBStrokeColor(id c, double r, double g, double b, double a);

static cx_class_t *kls(const char *n) { return cx_class_named(n); }
static SEL sl(const char *n) { return sel_registerName(n); }
static BOOL responds(id o, const char *s) { return o && cx_responds(object_getClass(o), sl(s)); }

extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } cf_const_t;
#define CS(var, text) static cf_const_t var##_s = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; const void *var = &var##_s;

/* ================================================================ Pasteboard Manager */
enum { badPasteboardSyncErr = -25130, badPasteboardIndexErr = -25131, badPasteboardItemErr = -25132,
       badPasteboardFlavorErr = -25133, noPasteboardPromiseKeeperErr = -25136, paramErr = -50 };
enum { kPasteboardModified = 1, kPasteboardClientIsOwner = 2 };
CS(kPasteboardClipboard, "com.apple.pasteboard.clipboard")
CS(kPasteboardFind, "com.apple.pasteboard.find")
CS(kPasteboardUniqueName, "com.apple.pasteboard.unique")
typedef OSStatus (*promise_fn)(id pb, void *item, id flavor, void *ctx);
typedef struct pbref {
    struct pbref *next;
    id ns;                       /* o NSPasteboard */
    i64 seen, wrote;             /* changeCount visto no último Synchronize e o do último Clear nosso */
    void *ids[256]; id items[256]; int nids;   /* PasteboardItemID do app → NSPasteboardItem */
    promise_fn keeper; void *keeper_ctx;
    struct { void *item; id flavor; } promised[256]; int npromised;
} pbref_t;
static pbref_t *g_pbrefs;
static pbref_t *pr_of(id ref) { for (pbref_t *p = g_pbrefs; p; p = p->next) if ((id)p == ref) return p; return 0; }
OSStatus PasteboardCreate(id name, id *out) {
    if (!out) return paramErr;
    const char *n = name && cx_is_str(name) ? cx_cstr(name) : 0;
    id ns;
    if (n && !strcmp(n, "com.apple.pasteboard.clipboard")) ns = MSG0(id, (id)kls("NSPasteboard"), "generalPasteboard");
    else if (n && !strcmp(n, "com.apple.pasteboard.find")) ns = MSG(id, (id)kls("NSPasteboard"), "pasteboardWithName:", cx_str("Apple CFPasteboard find"));
    else if (!n || !strcmp(n, "com.apple.pasteboard.unique")) ns = MSG0(id, (id)kls("NSPasteboard"), "pasteboardWithUniqueName");
    else ns = MSG(id, (id)kls("NSPasteboard"), "pasteboardWithName:", name);
    pbref_t *p = calloc(1, sizeof *p);
    if (!p || !ns) { free(p); return paramErr; }
    p->ns = objc_retain(ns);
    p->seen = MSG0(i64, ns, "changeCount");
    p->wrote = -1;
    p->next = g_pbrefs; g_pbrefs = p;
    *out = (id)p;
    return 0;
}
/* o PasteboardRef é um CFType para o app (CFRelease): sem isa, CFRelease vira nada e a ref fica viva */
u32 PasteboardSynchronize(id ref) {
    pbref_t *p = pr_of(ref);
    if (!p) return 0;
    i64 c = MSG0(i64, p->ns, "changeCount");
    u32 f = 0;
    if (c != p->seen) { f |= kPasteboardModified; p->seen = c; p->nids = 0; p->npromised = 0; }
    if (c == p->wrote) f |= kPasteboardClientIsOwner;
    return f;
}
OSStatus PasteboardClear(id ref) {
    pbref_t *p = pr_of(ref);
    if (!p) return paramErr;
    p->wrote = MSG0(i64, p->ns, "clearContents");
    p->seen = p->wrote;
    p->nids = 0; p->npromised = 0;
    return 0;
}
OSStatus PasteboardCopyName(id ref, id *out) { pbref_t *p = pr_of(ref); if (!p || !out) return paramErr; *out = objc_retain(MSG0(id, p->ns, "name")); return 0; }
static id items(pbref_t *p) { return MSG0(id, p->ns, "pasteboardItems"); }
OSStatus PasteboardGetItemCount(id ref, u64 *count) {
    pbref_t *p = pr_of(ref);
    if (!p || !count) return paramErr;
    *count = MSG0(u64, items(p), "count");
    return 0;
}
/* ItemID: o índice (1…n) para os itens que já estão no pasteboard */
OSStatus PasteboardGetItemIdentifier(id ref, u64 index, void **out) {
    pbref_t *p = pr_of(ref);
    if (!p || !out) return paramErr;
    if (index < 1 || index > MSG0(u64, items(p), "count")) return badPasteboardIndexErr;
    *out = (void *)(uintptr_t)index;
    return 0;
}
static id item_for(pbref_t *p, void *item) {
    for (int i = 0; i < p->nids; i++) if (p->ids[i] == item) return p->items[i];
    u64 idx = (u64)(uintptr_t)item;
    id all = items(p);
    if (idx >= 1 && idx <= MSG0(u64, all, "count")) return MSG(id, all, "objectAtIndex:", idx - 1);
    return 0;
}
OSStatus PasteboardCopyItemFlavors(id ref, void *item, id *out) {
    pbref_t *p = pr_of(ref);
    if (!p || !out) return paramErr;
    id it = item_for(p, item);
    if (!it) return badPasteboardItemErr;
    *out = objc_retain(MSG0(id, it, "types"));
    return 0;
}
static int promised(pbref_t *p, void *item, id flavor) {
    for (int i = 0; i < p->npromised; i++) if (p->promised[i].item == item && MSG(BOOL, p->promised[i].flavor, "isEqualToString:", flavor)) return i;
    return -1;
}
OSStatus PasteboardCopyItemFlavorData(id ref, void *item, id flavor, id *out) {
    pbref_t *p = pr_of(ref);
    if (!p || !out) return paramErr;
    id it = item_for(p, item);
    if (!it) return badPasteboardItemErr;
    id d = MSG(id, it, "dataForType:", flavor);
    int k;
    if (!d && (k = promised(p, item, flavor)) >= 0) {   /* promessa: o keeper grava agora */
        if (!p->keeper) return noPasteboardPromiseKeeperErr;
        p->keeper(ref, item, flavor, p->keeper_ctx);
        d = MSG(id, it, "dataForType:", flavor);
    }
    if (!d) return badPasteboardFlavorErr;
    *out = objc_retain(d);
    return 0;
}
OSStatus PasteboardGetItemFlavorFlags(id ref, void *item, id flavor, u32 *flags) {
    pbref_t *p = pr_of(ref);
    if (!p) return paramErr;
    id it = item_for(p, item);
    if (!it) return badPasteboardItemErr;
    id types = MSG0(id, it, "types");
    if (!MSG(BOOL, types, "containsObject:", flavor) && promised(p, item, flavor) < 0) return badPasteboardFlavorErr;
    if (flags) *flags = 0;   /* kPasteboardFlavorNoFlags */
    return 0;
}
OSStatus PasteboardPutItemFlavor(id ref, void *item, id flavor, id data, u32 flags) {
    (void)flags;
    pbref_t *p = pr_of(ref);
    if (!p || !flavor) return paramErr;
    id it = 0;
    for (int i = 0; i < p->nids; i++) if (p->ids[i] == item) it = p->items[i];
    if (!it) {
        if (p->nids >= 256) return paramErr;
        it = MSG0(id, MSG0(id, (id)kls("NSPasteboardItem"), "alloc"), "init");
        id one = MSG(id, (id)kls("NSArray"), "arrayWithObject:", it);
        MSG(BOOL, p->ns, "writeObjects:", one);
        p->ids[p->nids] = item; p->items[p->nids] = it; p->nids++;
        objc_release(it);
    }
    if (!data) {   /* NULL = promessa: o keeper fornece depois */
        if (p->npromised < 256) { p->promised[p->npromised].item = item; p->promised[p->npromised].flavor = objc_retain(flavor); p->npromised++; }
        MSG(void, it, "setString:forType:", cx_str(""), flavor);
        MSG(void, it, "setData:forType:", (id)0, flavor);
        return 0;
    }
    MSG(BOOL, it, "setData:forType:", data, flavor);
    p->wrote = MSG0(i64, p->ns, "changeCount");
    return 0;
}
OSStatus PasteboardSetPromiseKeeper(id ref, promise_fn keeper, void *ctx) {
    pbref_t *p = pr_of(ref);
    if (!p) return paramErr;
    p->keeper = keeper; p->keeper_ctx = ctx;
    return 0;
}
OSStatus PasteboardResolvePromises(id ref) {
    for (pbref_t *p = g_pbrefs; p; p = p->next) {
        if (ref && (id)p != ref) continue;
        if (!p->keeper) continue;
        for (int i = 0; i < p->npromised; i++) {
            id it = item_for(p, p->promised[i].item);
            if (it && !MSG(id, it, "dataForType:", p->promised[i].flavor)) p->keeper((id)p, p->promised[i].item, p->promised[i].flavor, p->keeper_ctx);
        }
    }
    return 0;
}
/* local de colagem (promessas de arquivo do Finder): o mOS não tem; o app recebe o erro e cai no caminho normal */
OSStatus PasteboardCopyPasteLocation(id ref, id *out) { (void)ref; if (out) *out = 0; return noPasteboardPromiseKeeperErr; }
OSStatus PasteboardSetPasteLocation(id ref, id url) { (void)ref; (void)url; return 0; }

/* ================================================================ Text Input Sources */
CS(kTISPropertyInputSourceIsASCIICapable, "TISPropertyInputSourceIsASCIICapable")
CS(kTISPropertyBundleID, "TISPropertyBundleID")
CS(kTISPropertyInputSourceIsEnabled, "TISPropertyInputSourceIsEnabled")
CS(kTISPropertyInputSourceIsSelected, "TISPropertyInputSourceIsSelected")
CS(kTISPropertyInputSourceIsSelectCapable, "TISPropertyInputSourceIsSelectCapable")
CS(kTISPropertyInputSourceType, "TISPropertyInputSourceType")
CS(kTISPropertyInputSourceCategory, "TISPropertyInputSourceCategory")
CS(kTISTypeKeyboardInputMode, "TISTypeKeyboardInputMode")
CS(kTISTypeKeyboardLayout, "TISTypeKeyboardLayout")
CS(kTISTypeKeyboardInputMethodWithoutModes, "TISTypeKeyboardInputMethodWithoutModes")
CS(kTISCategoryKeyboardInputSource, "TISCategoryKeyboardInputSource")
CS(kTISNotifyEnabledKeyboardInputSourcesChanged, "AppleEnabledInputSourcesChangedNotification")
/* a fonte única do mOS (teclado ABNT2) com as propriedades que os apps perguntam */
static id tis_src(void) {
    static id src;
    if (src) return src;
    src = TISCopyCurrentKeyboardInputSource();
    if (!src) return 0;
    id yes = MSG(id, (id)kls("NSNumber"), "numberWithBool:", (BOOL)YES);
    CFDictionarySetValue(src, kTISPropertyInputSourceIsASCIICapable, yes);
    CFDictionarySetValue(src, kTISPropertyInputSourceIsEnabled, yes);
    CFDictionarySetValue(src, kTISPropertyInputSourceIsSelected, yes);
    CFDictionarySetValue(src, kTISPropertyInputSourceIsSelectCapable, yes);
    CFDictionarySetValue(src, kTISPropertyInputSourceType, kTISTypeKeyboardLayout);
    CFDictionarySetValue(src, kTISPropertyInputSourceCategory, kTISCategoryKeyboardInputSource);
    CFDictionarySetValue(src, kTISPropertyBundleID, cx_str("com.apple.keylayout.Brazilian-ABNT2"));
    return src;
}
/* ---- métodos de entrada de terceiros (InputMethodKit, imk.c): instalados em /Library/Input Methods,
 * ~/Library/Input Methods ou registrados por TISRegisterInputSource; habilitados e o escolhido ficam nas preferências
 * com.apple.HIToolbox (AppleEnabledInputSources/AppleSelectedInputSources), lidas por todos os apps */
double CFAbsoluteTimeGetCurrent(void);
extern id NSHomeDirectory(void);
static id hit_domain(void) {
    id ud = MSG0(id, (id)kls("NSUserDefaults"), "standardUserDefaults");
    id d = MSG(id, ud, "persistentDomainForName:", cx_str("com.apple.HIToolbox"));
    return d ? MSG0(id, d, "mutableCopy") : MSG0(id, MSG0(id, (id)kls("NSMutableDictionary"), "alloc"), "init");
}
static void hit_save(id d) {
    id ud = MSG0(id, (id)kls("NSUserDefaults"), "standardUserDefaults");
    MSG(void, ud, "setPersistentDomain:forName:", d, cx_str("com.apple.HIToolbox"));
}
static id entry_bundle(id e) { return e ? MSG(id, e, "objectForKey:", cx_str("Bundle ID")) : 0; }
static id ime_entry(id bid) {
    id d = MSG0(id, MSG0(id, (id)kls("NSMutableDictionary"), "alloc"), "init");
    MSG(void, d, "setObject:forKey:", cx_str("Keyboard Input Method"), cx_str("InputSourceKind"));
    MSG(void, d, "setObject:forKey:", bid, cx_str("Bundle ID"));
    return objc_autorelease(d);
}
/* bundle id do método de entrada escolhido (vazio: o teclado); cache de meio segundo */
static double g_tis_at;
void cx_tis_cache_reset(void) { g_tis_at = 0; }
const char *cx_tis_selected_ime(void) {
    static char cur[256];
    double at = g_tis_at;
    double now = CFAbsoluteTimeGetCurrent();
    if (now - at < 0.5 && at) return cur[0] ? cur : 0;
    g_tis_at = now;
    cur[0] = 0;
    id d = hit_domain();
    id sel = MSG(id, d, "objectForKey:", cx_str("AppleSelectedInputSources"));
    u64 n = sel ? MSG0(u64, sel, "count") : 0;
    for (u64 i = 0; i < n; i++) {
        id e = MSG(id, sel, "objectAtIndex:", i);
        id kind = MSG(id, e, "objectForKey:", cx_str("InputSourceKind"));
        id bid = entry_bundle(e);
        if (bid && kind && !strcmp(cx_cstr(kind), "Keyboard Input Method")) { snprintf(cur, sizeof cur, "%s", cx_cstr(bid)); break; }
    }
    objc_release(d);
    return cur[0] ? cur : 0;
}
static void tis_changed(void) {
    id nc = MSG0(id, (id)kls("NSDistributedNotificationCenter"), "defaultCenter");
    if (nc) MSG(void, nc, "postNotificationName:object:", cx_str("com.apple.Carbon.TISNotifySelectedKeyboardInputSourceChanged"), (id)0);
    if (nc) MSG(void, nc, "postNotificationName:object:", cx_str("AppleSelectedInputSourcesChangedNotification"), (id)0);
}
/* caminhos dos métodos de entrada instalados (bundle id → caminho) */
static id ime_paths(void) {
    id out = MSG0(id, MSG0(id, (id)kls("NSMutableDictionary"), "alloc"), "init");
    id dirs[2] = {cx_str("/Library/Input Methods"), MSG(id, NSHomeDirectory(), "stringByAppendingPathComponent:", cx_str("Library/Input Methods"))};
    id fm = MSG0(id, (id)kls("NSFileManager"), "defaultManager");
    for (int k = 0; k < 2; k++) {
        id items = ((id (*)(id, SEL, id, id *))objc_msgSend)(fm, sel_registerName("contentsOfDirectoryAtPath:error:"), dirs[k], (id *)0);
        u64 n = items ? MSG0(u64, items, "count") : 0;
        for (u64 i = 0; i < n; i++) {
            id item = MSG(id, items, "objectAtIndex:", i);
            id path = MSG(id, dirs[k], "stringByAppendingPathComponent:", item);
            id b = MSG(id, (id)kls("NSBundle"), "bundleWithPath:", path);
            id bid = b ? MSG0(id, b, "bundleIdentifier") : 0;
            if (bid) MSG(void, out, "setObject:forKey:", path, bid);
        }
    }
    id d = hit_domain();
    id reg = MSG(id, d, "objectForKey:", cx_str("_mOSRegisteredInputMethods"));   /* TISRegisterInputSource: bundle id → caminho */
    if (reg) MSG(void, out, "addEntriesFromDictionary:", reg);
    objc_release(d);
    return objc_autorelease(out);
}
static BOOL list_has(id list, id bid) {
    u64 n = list ? MSG0(u64, list, "count") : 0;
    for (u64 i = 0; i < n; i++) { id b = entry_bundle(MSG(id, list, "objectAtIndex:", i)); if (b && MSG(BOOL, b, "isEqualToString:", bid)) return YES; }
    return NO;
}
extern id kTISPropertyInputSourceID, kTISPropertyLocalizedName;
static id ime_source(id bid, id path, BOOL enabled, BOOL selected) {
    id src = CFDictionaryCreateMutable(0, 12, kCFTypeDictionaryKeyCallBacks, kCFTypeDictionaryValueCallBacks);
    id yes = MSG(id, (id)kls("NSNumber"), "numberWithBool:", (BOOL)YES), no = MSG(id, (id)kls("NSNumber"), "numberWithBool:", (BOOL)NO);
    id b = MSG(id, (id)kls("NSBundle"), "bundleWithPath:", path);
    id name = b ? MSG(id, b, "objectForInfoDictionaryKey:", cx_str("CFBundleDisplayName")) : 0;
    if (!name && b) name = MSG(id, b, "objectForInfoDictionaryKey:", cx_str("CFBundleName"));
    CFDictionarySetValue(src, kTISPropertyInputSourceID, bid);
    CFDictionarySetValue(src, kTISPropertyBundleID, bid);
    CFDictionarySetValue(src, kTISPropertyLocalizedName, name ? name : bid);
    CFDictionarySetValue(src, kTISPropertyInputSourceType, kTISTypeKeyboardInputMethodWithoutModes);
    CFDictionarySetValue(src, kTISPropertyInputSourceCategory, kTISCategoryKeyboardInputSource);
    CFDictionarySetValue(src, kTISPropertyInputSourceIsASCIICapable, no);
    CFDictionarySetValue(src, kTISPropertyInputSourceIsEnabled, enabled ? yes : no);
    CFDictionarySetValue(src, kTISPropertyInputSourceIsSelectCapable, yes);
    CFDictionarySetValue(src, kTISPropertyInputSourceIsSelected, selected ? yes : no);
    CFDictionarySetValue(src, cx_str("_mOSBundlePath"), path);
    return objc_autorelease(src);
}
/* todas as fontes: o teclado e os métodos de entrada (habilitados, ou todos com include_all) */
static id all_sources(BOOL include_all) {
    id out = MSG0(id, (id)kls("NSMutableArray"), "array");
    const char *sel = cx_tis_selected_ime();
    id kb = tis_src();
    if (kb) {
        CFDictionarySetValue(kb, kTISPropertyInputSourceIsSelected, MSG(id, (id)kls("NSNumber"), "numberWithBool:", (BOOL)(sel == 0)));
        MSG(void, out, "addObject:", kb);
    }
    id d = hit_domain();
    id enabled = MSG(id, d, "objectForKey:", cx_str("AppleEnabledInputSources"));
    id paths = ime_paths();
    id keys = MSG0(id, paths, "allKeys");
    u64 n = MSG0(u64, keys, "count");
    for (u64 i = 0; i < n; i++) {
        id bid = MSG(id, keys, "objectAtIndex:", i);
        BOOL en = list_has(enabled, bid);
        if (!en && !include_all) continue;
        id ipath = MSG(id, paths, "objectForKey:", bid);
        MSG(void, out, "addObject:", ime_source(bid, ipath, en, sel && !strcmp(sel, cx_cstr(bid))));
    }
    objc_release(d);
    return out;
}
/* a fonte atual quando é um método de entrada (vscode_frameworks.c: TISCopyCurrentKeyboardInputSource) */
id cx_tis_current_ime_source(void) {
    const char *sel = cx_tis_selected_ime();
    if (!sel) return 0;
    id paths = ime_paths();
    id path = MSG(id, paths, "objectForKey:", cx_str(sel));
    return path ? ime_source(cx_str(sel), path, YES, YES) : 0;
}
id TISCopyInputMethodKeyboardLayoutOverride(void) { return 0; }   /* os métodos de entrada usam o layout do teclado */
id TISCopyCurrentASCIICapableKeyboardInputSource(void) { id s = tis_src(); return s ? objc_retain(s) : 0; }
id TISCopyInputSourceForLanguage(id lang) { (void)lang; id s = tis_src(); return s ? objc_retain(s) : 0; }
/* lista filtrada pelas propriedades pedidas (cada chave tem que bater com a da fonte) */
id TISCreateInputSourceList(id props, BOOL include_all) {
    id all = all_sources(include_all);
    id keys = props ? MSG0(id, props, "allKeys") : 0;
    u64 n = keys ? MSG0(u64, keys, "count") : 0, m = MSG0(u64, all, "count");
    const void *vals[64];
    long k = 0;
    for (u64 j = 0; j < m && k < 64; j++) {
        id s = MSG(id, all, "objectAtIndex:", j);
        BOOL ok = YES;
        for (u64 i = 0; ok && i < n; i++) {
            id key = MSG(id, keys, "objectAtIndex:", i);
            id want = MSG(id, props, "objectForKey:", key);
            id have = (id)CFDictionaryGetValue(s, key);
            ok = have && want && MSG(BOOL, have, "isEqual:", want);
        }
        if (ok) vals[k++] = s;
    }
    return CFArrayCreate(0, vals, k, &kCFTypeArrayCallBacks);
}
static id src_bundle(id src) { return src ? (id)CFDictionaryGetValue(src, kTISPropertyBundleID) : 0; }
static BOOL is_keyboard(id src) { return src == tis_src() || (src && MSG(BOOL, src, "isEqual:", tis_src())); }
OSStatus TISSelectInputSource(id src) {
    if (!src) return paramErr;
    id d = hit_domain();
    if (is_keyboard(src)) MSG(void, d, "removeObjectForKey:", cx_str("AppleSelectedInputSources"));
    else {
        id bid = src_bundle(src);
        if (!bid || !MSG(id, ime_paths(), "objectForKey:", bid)) { objc_release(d); return paramErr; }
        id one = MSG(id, (id)kls("NSArray"), "arrayWithObject:", ime_entry(bid));
        MSG(void, d, "setObject:forKey:", one, cx_str("AppleSelectedInputSources"));
        id en = MSG(id, d, "objectForKey:", cx_str("AppleEnabledInputSources"));
        if (!list_has(en, bid)) {
            id l = en ? MSG0(id, en, "mutableCopy") : MSG0(id, MSG0(id, (id)kls("NSMutableArray"), "alloc"), "init");
            MSG(void, l, "addObject:", ime_entry(bid));
            MSG(void, d, "setObject:forKey:", l, cx_str("AppleEnabledInputSources"));
            objc_release(l);
        }
    }
    hit_save(d);
    objc_release(d);
    extern void cx_tis_cache_reset(void);
    cx_tis_cache_reset();
    tis_changed();
    return 0;
}
OSStatus TISDeselectInputSource(id src) { return src && !is_keyboard(src) ? TISSelectInputSource(tis_src()) : paramErr; }
OSStatus TISEnableInputSource(id src) {
    if (is_keyboard(src)) return 0;
    id bid = src_bundle(src);
    if (!bid) return paramErr;
    id d = hit_domain();
    id en = MSG(id, d, "objectForKey:", cx_str("AppleEnabledInputSources"));
    if (!list_has(en, bid)) {
        id l = en ? MSG0(id, en, "mutableCopy") : MSG0(id, MSG0(id, (id)kls("NSMutableArray"), "alloc"), "init");
        MSG(void, l, "addObject:", ime_entry(bid));
        MSG(void, d, "setObject:forKey:", l, cx_str("AppleEnabledInputSources"));
        objc_release(l);
        hit_save(d);
    }
    objc_release(d);
    return 0;
}
OSStatus TISDisableInputSource(id src) {
    if (is_keyboard(src)) return paramErr;   /* o teclado não sai */
    id bid = src_bundle(src);
    if (!bid) return paramErr;
    id d = hit_domain();
    id en = MSG(id, d, "objectForKey:", cx_str("AppleEnabledInputSources"));
    id l = MSG0(id, MSG0(id, (id)kls("NSMutableArray"), "alloc"), "init");
    u64 n = en ? MSG0(u64, en, "count") : 0;
    for (u64 i = 0; i < n; i++) { id e = MSG(id, en, "objectAtIndex:", i); id b = entry_bundle(e); if (!b || !MSG(BOOL, b, "isEqualToString:", bid)) MSG(void, l, "addObject:", e); }
    MSG(void, d, "setObject:forKey:", l, cx_str("AppleEnabledInputSources"));
    objc_release(l);
    const char *sel = cx_tis_selected_ime();
    if (sel && !strcmp(sel, cx_cstr(bid))) MSG(void, d, "removeObjectForKey:", cx_str("AppleSelectedInputSources"));
    hit_save(d);
    objc_release(d);
    tis_changed();
    return 0;
}
/* registra um método de entrada fora das pastas padrão (o instalador do app chama com a URL do bundle) */
OSStatus TISRegisterInputSource(id url) {
    if (!url) return paramErr;
    id path = MSG0(id, url, "path");
    id b = path ? MSG(id, (id)kls("NSBundle"), "bundleWithPath:", path) : 0;
    id bid = b ? MSG0(id, b, "bundleIdentifier") : 0;
    if (!bid) return paramErr;
    id d = hit_domain();
    id reg = MSG(id, d, "objectForKey:", cx_str("_mOSRegisteredInputMethods"));
    id r = reg ? MSG0(id, reg, "mutableCopy") : MSG0(id, MSG0(id, (id)kls("NSMutableDictionary"), "alloc"), "init");
    MSG(void, r, "setObject:forKey:", path, bid);
    MSG(void, d, "setObject:forKey:", r, cx_str("_mOSRegisteredInputMethods"));
    objc_release(r);
    hit_save(d);
    objc_release(d);
    return 0;
}

/* ================================================================ Process Manager */
typedef struct { u32 hi, lo; } psn_t;
enum { procNotFound = -600, kCurrentProcess = 2 };
OSStatus GetCurrentProcess(psn_t *psn) { if (!psn) return paramErr; psn->hi = 0; psn->lo = kCurrentProcess; return 0; }
static id running(void) { return MSG0(id, (id)kls("NSRunningApplication"), "runningApplications"); }
static i64 pid_of(psn_t *psn) {
    if (!psn) return -1;
    if (psn->hi == 0 && psn->lo == kCurrentProcess) return (i64)getpid();
    return psn->lo;
}
/* o psn de um app é {1, pid} (0,0 = nenhum; 0,2 = o atual) */
OSStatus GetNextProcess(psn_t *psn) {
    if (!psn) return paramErr;
    id apps = running();
    u64 n = apps ? MSG0(u64, apps, "count") : 0;
    i64 cur = (psn->hi == 0 && psn->lo == 0) ? -1 : pid_of(psn);
    BOOL take = cur < 0;
    for (u64 i = 0; i < n; i++) {
        i64 p = MSG0(i64, MSG(id, apps, "objectAtIndex:", i), "processIdentifier");
        if (take) { psn->hi = 1; psn->lo = (u32)p; return 0; }
        if (p == cur) take = YES;
    }
    psn->hi = psn->lo = 0;
    return procNotFound;
}
static id app_for(psn_t *psn) {
    i64 pid = pid_of(psn);
    id apps = running();
    u64 n = apps ? MSG0(u64, apps, "count") : 0;
    for (u64 i = 0; i < n; i++) { id a = MSG(id, apps, "objectAtIndex:", i); if (MSG0(i64, a, "processIdentifier") == pid) return a; }
    return pid == (i64)getpid() ? MSG0(id, (id)kls("NSRunningApplication"), "currentApplication") : 0;
}
OSStatus SetFrontProcess(psn_t *psn) {
    id a = app_for(psn);
    if (!a) return procNotFound;
    MSG(BOOL, a, "activateWithOptions:", (u64)2 /* ignorando os outros apps */);
    return 0;
}
OSStatus SetFrontProcessWithOptions(psn_t *psn, u32 opts) { (void)opts; return SetFrontProcess(psn); }
id ProcessInformationCopyDictionary(psn_t *psn, u32 mask) {
    (void)mask;
    id a = app_for(psn);
    if (!a) return 0;
    id d = MSG0(id, MSG0(id, (id)kls("NSMutableDictionary"), "alloc"), "init");
    id pidn = MSG(id, (id)kls("NSNumber"), "numberWithLongLong:", MSG0(i64, a, "processIdentifier"));
    MSG(void, d, "setObject:forKey:", pidn, cx_str("pid"));
    id bid = MSG0(id, a, "bundleIdentifier"), name = MSG0(id, a, "localizedName");
    if (bid) MSG(void, d, "setObject:forKey:", bid, cx_str("CFBundleIdentifier"));
    if (name) { MSG(void, d, "setObject:forKey:", name, cx_str("CFBundleName")); MSG(void, d, "setObject:forKey:", name, cx_str("CFBundleExecutable")); }
    id url = responds(a, "bundleURL") ? MSG0(id, a, "bundleURL") : 0;
    if (url) MSG(void, d, "setObject:forKey:", MSG0(id, url, "path"), cx_str("BundlePath"));
    id t = MSG(id, (id)kls("NSNumber"), "numberWithBool:", (BOOL)MSG0(BOOL, a, "isActive"));
    MSG(void, d, "setObject:forKey:", t, cx_str("IsActive"));
    return d;
}

/* ================================================================ eventos, TSM, modo da interface */
u32 GetCurrentEventKeyModifiers(void) {
    u64 m = MSG0(u64, (id)kls("NSEvent"), "modifierFlags");
    u32 c = 0;
    if (m & (1u << 20)) c |= 0x100;   /* cmdKey */
    if (m & (1u << 17)) c |= 0x200;   /* shiftKey */
    if (m & (1u << 16)) c |= 0x400;   /* alphaLock */
    if (m & (1u << 19)) c |= 0x800;   /* optionKey */
    if (m & (1u << 18)) c |= 0x1000;  /* controlKey */
    return c;
}
u32 GetCurrentEventButtonState(void) { return (u32)MSG0(u64, (id)kls("NSEvent"), "pressedMouseButtons"); }
u32 GetCurrentButtonState(void) { return GetCurrentEventButtonState(); }
u32 GetCurrentKeyModifiers(void) { return GetCurrentEventKeyModifiers(); }
void *TSMGetActiveDocument(void) { return 0; }
static u32 g_ui_mode, g_ui_opts;
OSStatus SetSystemUIMode(u32 mode, u32 opts) { g_ui_mode = mode; g_ui_opts = opts; return 0; }   /* a barra e o Dock são do window server */
void GetSystemUIMode(u32 *mode, u32 *opts) { if (mode) *mode = g_ui_mode; if (opts) *opts = g_ui_opts; }

/* ================================================================ Appearance Manager / HITheme */
/* ThemeMetric: valores do tema do mOS em pontos */
OSStatus GetThemeMetric(u32 metric, i32 *out) {
    static const i32 v[] = {15, 11, 14, 16, 2, 1, 1, 4, 1, -1, 21, 18, 4, 20, 2, 14, 12, 18, 5, 20, 18, 15, 12, 2, 1, 22};
    if (!out) return paramErr;
    if (metric >= sizeof v / sizeof v[0]) { *out = 0; return paramErr; }
    *out = v[metric];
    return 0;
}
typedef struct { u32 version; u32 state; u32 kind; u32 value; u32 adornment; } hi_button_t;
typedef struct { u32 version; u32 state; u32 kind; u8 is_focused; } hi_frame_t;
static void stroke(id c, R r, double g) { CGContextSetRGBStrokeColor(c, g, g, g, 1); CGContextStrokeRectWithWidth(c, r, 1); }
OSStatus HIThemeDrawButton(const R *bounds, const hi_button_t *info, id cgctx, u32 orient, R *label) {
    (void)orient;
    if (!bounds || !cgctx) return paramErr;
    BOOL pressed = info && info->state == 2, disabled = info && info->state == 0;
    BOOL on = info && info->value == 1;
    double g = pressed ? 0.78 : 0.97;
    CGContextSetRGBFillColor(cgctx, on ? 0.0 : g, on ? 0.40 : g, on ? 0.86 : g, disabled ? 0.5 : 1);
    CGContextFillRect(cgctx, *bounds);
    stroke(cgctx, *bounds, 0.62);
    if (label) *label = (R){bounds->x + 6, bounds->y + 2, bounds->w - 12 > 0 ? bounds->w - 12 : 0, bounds->h - 4 > 0 ? bounds->h - 4 : 0};
    return 0;
}
OSStatus HIThemeDrawFrame(const R *r, const hi_frame_t *info, id cgctx, u32 orient) {
    (void)orient;
    if (!r || !cgctx) return paramErr;
    stroke(cgctx, *r, info && info->is_focused ? 0.35 : 0.66);
    return 0;
}
OSStatus HIThemeDrawMenuBackground(const R *r, const void *info, id cgctx, u32 orient) {
    (void)info; (void)orient;
    if (!r || !cgctx) return paramErr;
    CGContextSetRGBFillColor(cgctx, 0.965, 0.965, 0.97, 1);
    CGContextFillRect(cgctx, *r);
    return 0;
}
typedef struct { u32 version; u32 menu_type; u32 item_type; } hi_menuitem_t;
OSStatus HIThemeDrawMenuItem(const R *menu, const R *item, const hi_menuitem_t *info, id cgctx, u32 orient, R *content) {
    (void)menu; (void)orient;
    if (!item || !cgctx) return paramErr;
    BOOL selected = info && (info->item_type & 0x01);   /* kThemeMenuSelected */
    if (selected) { CGContextSetRGBFillColor(cgctx, 0.0, 0.40, 0.86, 1); CGContextFillRect(cgctx, *item); }
    if (content) *content = (R){item->x + 20, item->y, item->w - 40 > 0 ? item->w - 40 : 0, item->h};
    return 0;
}
/* texto: CFString num retângulo, com o NSString do AppKit (o contexto é o da view que está desenhando) */
OSStatus HIThemeDrawTextBox(id str, const R *r, const void *info, id cgctx, u32 orient) {
    (void)info; (void)orient; (void)cgctx;
    if (!str || !r) return paramErr;
    ((void (*)(id, SEL, R, id))objc_msgSend)(str, sl("drawInRect:withAttributes:"), *r, (id)0);
    return 0;
}

/* ================================================================ telas e ícones */
/* fade de tela: o window server não escurece a tela; a reserva e o fade dão certo sem efeito visual */
OSStatus CGAcquireDisplayFadeReservation(float seconds, u32 *token) { (void)seconds; if (token) *token = 1; return 0; }
OSStatus CGReleaseDisplayFadeReservation(u32 token) { (void)token; return 0; }
OSStatus CGDisplayFade(u32 token, float dur, float from, float to, float r, float g, float b, BOOL sync) {
    (void)token; (void)dur; (void)from; (void)to; (void)r; (void)g; (void)b; (void)sync;
    return 0;
}
/* cursor registrado do sistema (privado): o mOS não exporta as imagens; kCGErrorFailure */
OSStatus CGSCreateRegisteredCursorImage(int cid, id name, void *hot) { (void)cid; (void)name; (void)hot; return 1000; }
/* PlotIconRefInContext: um ícone genérico de documento (o mOS não tem IconRef de arquivo) */
OSStatus PlotIconRefInContext(id cgctx, const R *r, u32 align, u32 transform, const void *label, u32 flags, void *icon) {
    (void)align; (void)label; (void)flags; (void)icon;
    if (!cgctx || !r) return paramErr;
    double a = (transform & 0x1) ? 0.5 : 1;   /* kTransformDisabled */
    R page = {r->x + r->w * 0.2, r->y + r->h * 0.08, r->w * 0.6, r->h * 0.84};
    CGContextSetRGBFillColor(cgctx, 1, 1, 1, a);
    CGContextFillRect(cgctx, page);
    CGContextSetRGBStrokeColor(cgctx, 0.55, 0.55, 0.58, a);
    CGContextStrokeRectWithWidth(cgctx, page, 1);
    return 0;
}
