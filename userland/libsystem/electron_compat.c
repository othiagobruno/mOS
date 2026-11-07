/*
 * mOS libSystem — métodos do Foundation/AppKit que o Electron (VS Code, Slack, Claude) chama e que faltavam
 * (apareciam no log como "ainda não implementado no mOS"):
 *
 *  - KVC: -validateValue:forKey:error: (chama -validate<Chave>:error: se existir; senão aceita, como o Foundation),
 *    -validateValue:forKeyPath:error:, -dictionaryWithValuesForKeys:, -setValuesForKeysWithDictionary:. O Mantle
 *    (MTLModel, base do SQRLCodeSignature do Squirrel, o autoUpdater do Electron) valida cada chave antes de atribuir;
 *    sem o método, o "ainda não implementado" devolvia NO, -[SQRLCodeSignature initWithRequirement:] dava nil e o
 *    SQRLUpdater lançava NSInternalInconsistencyException ("Could not get code signature for running application").
 *    O Electron tem @try/@catch em volta, mas o mOS ainda não desenrola exceções: o VS Code abortava ao configurar o
 *    feed de atualização, logo depois de abrir o workbench.
 *  - -performSelector:withObject:afterDelay: (e inModes:), +cancelPreviousPerformRequestsWithTarget:[selector:object:]
 *    e -[NSRunLoop performSelector:target:argument:order:modes:] / -cancelPerformSelector:target:argument: /
 *    -cancelPerformSelectorsWithTarget:: um CFRunLoopTimer de um disparo no run loop da thread que pediu, como no macOS.
 *  - NSApplication -postEvent:atStart: (o MessagePumpNSApplication do Chromium posta um evento para acordar o laço ao
 *    sair; aqui não há fila de NSEvent, então só acorda o run loop principal) e -setAppearance:.
 *  - NSView -isHiddenOrHasHiddenAncestor e allowedTouchTypes; NSWorkspace accessibilityDisplayShould* (tudo NO);
 *    NSAppleEventManager -currentAppleEvent/-currentReplyAppleEvent (nil: nenhum Apple Event em andamento);
 *    NSFontManager -availableMembersOfFontFamily:; NSURL -getResourceValue:forKey:error:, -resourceValuesForKeys:error:
 *    e -setResourceValue:forKey:error:.
 */
#include "objc.h"
#include "libc.h"
#include "foundation.h"
#include "sys.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
extern id cx_num_bool(BOOL b);
int lstat(const char *p, void *st);

/* CFRunLoop (cfrunloop.c) */
typedef struct { long version; void *info; const void *(*retain)(const void *); void (*release)(const void *); void *desc; } rl_ctx_t;
void *CFRunLoopGetCurrent(void);
void *CFRunLoopGetMain(void);
void CFRunLoopWakeUp(void *rl);
double CFAbsoluteTimeGetCurrent(void);
void *CFRunLoopTimerCreate(void *alloc, double fire, double interval, unsigned long flags, long order,
                           void (*cb)(void *, void *), rl_ctx_t *ctx);
void CFRunLoopAddTimer(void *rl, void *t, void *mode);
void CFRunLoopRemoveTimer(void *rl, void *t, void *mode);
void CFRunLoopTimerInvalidate(void *t);

static cx_class_t *cls(const char *n) { return cx_class_named(n); }

/* ================================================================ KVC */
static BOOL o_validate(id self, SEL c, id *io, id key, id *err) {
    (void)c;
    const char *k = key ? cx_cstr(key) : 0;
    size_t n = k ? strlen(k) : 0;
    if (!n || n > 200 || !self) return YES;
    char nm[224];
    memcpy(nm, "validate", 8);
    nm[8] = (char)(k[0] >= 'a' && k[0] <= 'z' ? k[0] - 32 : k[0]);
    memcpy(nm + 9, k + 1, n - 1);
    memcpy(nm + 8 + n, ":error:", 8);
    SEL s = sel_registerName(nm);
    if (cx_responds(self->isa, s)) return ((BOOL (*)(id, SEL, id *, id *))objc_msgSend)(self, s, io, err);
    return YES;
}
static BOOL o_validate_path(id self, SEL c, id *io, id path, id *err) {
    const char *p = path ? cx_cstr(path) : 0;
    const char *dot = p ? strrchr(p, '.') : 0;
    if (!dot) return o_validate(self, c, io, path, err);
    id owner = MSG(id, self, "valueForKeyPath:", cx_str_n(p, (u64)(dot - p)));
    return owner ? MSG(BOOL, owner, "validateValue:forKey:error:", io, cx_str(dot + 1), err) : YES;
}
static id o_dict_values(id self, SEL c, id keys) {
    (void)c;
    id d = MSG0(id, (id)cls("NSMutableDictionary"), "dictionary");
    id null = MSG0(id, (id)cls("NSNull"), "null");
    u64 n = keys ? MSG0(u64, keys, "count") : 0;
    for (u64 i = 0; i < n; i++) {
        id k = MSG(id, keys, "objectAtIndex:", i);
        id v = MSG(id, self, "valueForKey:", k);
        MSG(void, d, "setObject:forKey:", v ? v : null, k);
    }
    return d;
}
static void o_set_values(id self, SEL c, id dict) {
    (void)c;
    id keys = dict ? MSG0(id, dict, "allKeys") : 0;
    id null = MSG0(id, (id)cls("NSNull"), "null");
    u64 n = keys ? MSG0(u64, keys, "count") : 0;
    for (u64 i = 0; i < n; i++) {
        id k = MSG(id, keys, "objectAtIndex:", i);
        id v = MSG(id, dict, "objectForKey:", k);
        MSG(void, self, "setValue:forKey:", v == null ? (id)0 : v, k);
    }
}

/* ================================================================ envios adiados */
typedef struct dperf { id target, arg; SEL sel; void *timer, *loop; struct dperf *next; } dperf_t;
static dperf_t *g_dperf;
static volatile int g_dperf_lk;
static void dp_lock(void) { while (__sync_lock_test_and_set(&g_dperf_lk, 1)) SYS0(SYS_YIELD); }
static void dp_unlock(void) { __sync_lock_release(&g_dperf_lk); }
/* tira da lista; 1 se ainda estava lá (0: cancelado antes) */
static int dp_unlink(dperf_t *d) {
    int found = 0;
    dp_lock();
    for (dperf_t **pp = &g_dperf; *pp; pp = &(*pp)->next)
        if (*pp == d) { *pp = d->next; found = 1; break; }
    dp_unlock();
    return found;
}
static void dp_dispose(dperf_t *d) {
    CFRunLoopTimerInvalidate(d->timer);
    CFRunLoopRemoveTimer(d->loop, d->timer, 0);
    objc_release((id)d->timer);
    objc_release(d->target);
    if (d->arg) objc_release(d->arg);
    free(d);
}
static void dp_fire(void *timer, void *info) {
    (void)timer;
    dperf_t *d = info;
    if (!dp_unlink(d)) return;
    ((void (*)(id, SEL, id))objc_msgSend)(d->target, d->sel, d->arg);
    dp_dispose(d);
}
static void dp_schedule(void *loop, id target, SEL sel, id arg, double delay) {
    if (!target || !sel) return;
    dperf_t *d = calloc(1, sizeof *d);
    if (!d) return;
    d->target = objc_retain(target);
    d->arg = arg ? objc_retain(arg) : 0;
    d->sel = sel;
    d->loop = loop ? loop : CFRunLoopGetCurrent();
    rl_ctx_t ctx = {0, d, 0, 0, 0};
    d->timer = CFRunLoopTimerCreate(0, CFAbsoluteTimeGetCurrent() + (delay > 0 ? delay : 0), 0, 0, 0, dp_fire, &ctx);
    if (!d->timer) { objc_release(d->target); if (d->arg) objc_release(d->arg); free(d); return; }
    dp_lock();
    d->next = g_dperf;
    g_dperf = d;
    dp_unlock();
    CFRunLoopAddTimer(d->loop, d->timer, 0);
    CFRunLoopWakeUp(d->loop);
}
/* all = 1: todo pedido do alvo; senão o seletor e o argumento (isEqual:) também precisam bater */
static void dp_cancel(id target, SEL sel, id arg, int all) {
    dperf_t *hit = 0;
    dp_lock();
    for (dperf_t **pp = &g_dperf; *pp;) {
        dperf_t *d = *pp;
        int m = d->target == target &&
                (all || (d->sel == sel && (d->arg == arg || (d->arg && arg && MSG(BOOL, d->arg, "isEqual:", arg)))));
        if (m) { *pp = d->next; d->next = hit; hit = d; }
        else pp = &d->next;
    }
    dp_unlock();
    while (hit) { dperf_t *n = hit->next; dp_dispose(hit); hit = n; }
}
static void o_perform_delay(id self, SEL c, SEL s, id arg, double delay) { (void)c; dp_schedule(0, self, s, arg, delay); }
static void o_perform_delay_modes(id self, SEL c, SEL s, id arg, double delay, id modes) {
    (void)c; (void)modes;
    dp_schedule(0, self, s, arg, delay);
}
static void c_cancel_all(id cls_, SEL c, id target) { (void)cls_; (void)c; dp_cancel(target, 0, 0, 1); }
static void c_cancel(id cls_, SEL c, id target, SEL s, id arg) { (void)cls_; (void)c; dp_cancel(target, s, arg, 0); }
static void rl_perform(id self, SEL c, SEL s, id target, id arg, u64 order, id modes) {
    (void)self; (void)c; (void)order; (void)modes;
    dp_schedule(0, target, s, arg, 0);
}
static void rl_cancel(id self, SEL c, SEL s, id target, id arg) { (void)self; (void)c; dp_cancel(target, s, arg, 0); }
static void rl_cancel_all(id self, SEL c, id target) { (void)self; (void)c; dp_cancel(target, 0, 0, 1); }

/* ================================================================ AppKit */
static void app_post_event(id self, SEL c, id ev, BOOL at_start) { (void)self; (void)c; (void)ev; (void)at_start; CFRunLoopWakeUp(CFRunLoopGetMain()); }
static id g_app_appearance;
static void app_set_appearance(id self, SEL c, id a) {
    (void)self; (void)c;
    if (a) objc_retain(a);
    if (g_app_appearance) objc_release(g_app_appearance);
    g_app_appearance = a;
}
static BOOL v_hidden_or_ancestor(id self, SEL c) {
    (void)c;
    int guard = 0;
    for (id v = self; v && guard < 256; v = MSG0(id, v, "superview"), guard++)
        if (MSG0(BOOL, v, "isHidden")) return YES;
    return NO;
}
static u64 v_touch_types(id self, SEL c) { (void)self; (void)c; return 2; }   /* NSTouchTypeMaskIndirect */
static void v_set_touch_types(id self, SEL c, u64 t) { (void)self; (void)c; (void)t; }
static BOOL no_m(id self, SEL c) { (void)self; (void)c; return NO; }
static id nil_m(id self, SEL c) { (void)self; (void)c; return 0; }

/* NSFontManager: membros da família no formato do AppKit: [nome PostScript, estilo, peso, traços] */
static id fm_members(id self, SEL c, id family) {
    (void)self; (void)c;
    const char *f = family ? cx_cstr(family) : 0;
    if (!f || !*f) return 0;
    id a = MSG0(id, (id)cls("NSMutableArray"), "array");
    static const struct { const char *suffix, *style; int weight; u64 traits; } st[] = {
        {"", "Regular", 5, 0}, {"-Bold", "Bold", 9, 2}, {"-Italic", "Italic", 5, 1}, {"-BoldItalic", "Bold Italic", 9, 3},
    };
    for (unsigned i = 0; i < sizeof st / sizeof st[0]; i++) {
        char name[160];
        snprintf(name, sizeof name, "%s%s", f, st[i].suffix);
        id m = MSG0(id, (id)cls("NSMutableArray"), "array");
        MSG(void, m, "addObject:", cx_str(name));
        MSG(void, m, "addObject:", cx_str(st[i].style));
        MSG(void, m, "addObject:", cx_num_i(st[i].weight));
        MSG(void, m, "addObject:", cx_num_i((i64)st[i].traits));
        MSG(void, a, "addObject:", m);
    }
    return a;
}

/* ================================================================ NSURL: valores de recurso */
typedef struct {
    u32 st_dev; u16 st_mode, st_nlink; u64 st_ino; u32 st_uid, st_gid, st_rdev;
    i64 atime, atime_ns, mtime, mtime_ns, ctime, ctime_ns, btime, btime_ns;
    i64 st_size, st_blocks; i32 st_blksize; u32 st_flags, st_gen; i32 lspare; i64 qspare[2];
} cmp_stat_t;
static int key_is(const char *k, const char *name) { return k && !strcmp(k, name); }
static id url_value(id url, const char *k, int *ok) {
    *ok = 1;
    id path = MSG0(id, url, "path");
    const char *p = path ? cx_cstr(path) : 0;
    if (!p) { *ok = 0; return 0; }
    const char *slash = strrchr(p, '/');
    const char *name = slash && slash[1] ? slash + 1 : p;
    if (key_is(k, "NSURLNameKey") || key_is(k, "NSURLLocalizedNameKey")) return cx_str(name);
    if (key_is(k, "NSURLPathKey") || key_is(k, "_NSURLPathKey") || key_is(k, "NSURLCanonicalPathKey")) return path;
    if (key_is(k, "NSURLIsHiddenKey")) return cx_num_bool(name[0] == '.');
    if (key_is(k, "NSURLHasHiddenExtensionKey") || key_is(k, "NSURLIsExcludedFromBackupKey") ||
        key_is(k, "NSURLIsVolumeKey") || key_is(k, "NSURLIsAliasFileKey") || key_is(k, "NSURLIsUbiquitousItemKey") ||
        key_is(k, "NSURLVolumeIsRemovableKey") || key_is(k, "NSURLVolumeIsEjectableKey") ||
        key_is(k, "NSURLIsSystemImmutableKey") || key_is(k, "NSURLIsUserImmutableKey") || key_is(k, "NSURLVolumeIsReadOnlyKey"))
        return cx_num_bool(NO);
    if (key_is(k, "NSURLVolumeIsLocalKey") || key_is(k, "NSURLVolumeIsInternalKey")) return cx_num_bool(YES);
    if (key_is(k, "NSURLTagNamesKey") || key_is(k, "NSURLContentTypeKey") || key_is(k, "NSURLFileProtectionKey") ||
        key_is(k, "NSURLQuarantinePropertiesKey") || key_is(k, "NSURLLabelColorKey"))
        return 0;
    cmp_stat_t st;
    int link = lstat(p, &st) == 0 && (st.st_mode & 0170000) == 0120000;
    if (stat(p, &st) != 0 && !link) { *ok = 0; return 0; }
    int dir = (st.st_mode & 0170000) == 0040000, reg = (st.st_mode & 0170000) == 0100000;
    if (key_is(k, "NSURLIsDirectoryKey")) return cx_num_bool(dir);
    if (key_is(k, "NSURLIsRegularFileKey")) return cx_num_bool(reg);
    if (key_is(k, "NSURLIsSymbolicLinkKey")) return cx_num_bool(link);
    if (key_is(k, "NSURLIsPackageKey")) {
        const char *dot = strrchr(name, '.');
        return cx_num_bool(dir && dot && (!strcmp(dot, ".app") || !strcmp(dot, ".bundle") || !strcmp(dot, ".framework")));
    }
    if (key_is(k, "NSURLIsReadableKey")) return cx_num_bool(access(p, 4) == 0);
    if (key_is(k, "NSURLIsWritableKey")) return cx_num_bool(access(p, 2) == 0);
    if (key_is(k, "NSURLIsExecutableKey")) return cx_num_bool(access(p, 1) == 0);
    if (key_is(k, "NSURLFileSizeKey") || key_is(k, "NSURLTotalFileSizeKey") || key_is(k, "NSURLFileAllocatedSizeKey") ||
        key_is(k, "NSURLTotalFileAllocatedSizeKey"))
        return dir ? 0 : cx_num_i(st.st_size);
    if (key_is(k, "NSURLContentModificationDateKey") || key_is(k, "NSURLAttributeModificationDateKey") ||
        key_is(k, "NSURLContentAccessDateKey") || key_is(k, "NSURLCreationDateKey")) {
        i64 t = key_is(k, "NSURLCreationDateKey") ? (st.btime ? st.btime : st.mtime) : key_is(k, "NSURLContentAccessDateKey") ? st.atime : st.mtime;
        return MSG(id, (id)cls("NSDate"), "dateWithTimeIntervalSince1970:", (double)t);
    }
    if (key_is(k, "NSURLFileResourceTypeKey"))
        return cx_str(dir ? "NSURLFileResourceTypeDirectory" : link ? "NSURLFileResourceTypeSymbolicLink" :
                      reg ? "NSURLFileResourceTypeRegular" : "NSURLFileResourceTypeUnknown");
    if (key_is(k, "NSURLTypeIdentifierKey")) return cx_str(dir ? "public.folder" : "public.data");
    if (key_is(k, "NSURLLinkCountKey")) return cx_num_i(st.st_nlink);
    if (key_is(k, "NSURLFileResourceIdentifierKey")) return cx_num_i((i64)st.st_ino);
    return 0;   /* chave desconhecida: sucesso com valor nil, como o macOS faz com chaves sem valor */
}
static BOOL url_get_value(id self, SEL c, id *value, id key, id *err) {
    (void)c;
    if (err) *err = 0;
    if (!value) return NO;
    *value = 0;
    int ok;
    id v = url_value(self, key ? cx_cstr(key) : 0, &ok);
    if (!ok) return NO;
    *value = v;
    return YES;
}
static id url_values(id self, SEL c, id keys, id *err) {
    (void)c;
    if (err) *err = 0;
    id d = MSG0(id, (id)cls("NSMutableDictionary"), "dictionary");
    u64 n = keys ? MSG0(u64, keys, "count") : 0;
    for (u64 i = 0; i < n; i++) {
        id k = MSG(id, keys, "objectAtIndex:", i);
        int ok;
        id v = url_value(self, cx_cstr(k), &ok);
        if (!ok) return 0;
        if (v) MSG(void, d, "setObject:forKey:", v, k);
    }
    return d;
}
static BOOL url_set_value(id self, SEL c, id value, id key, id *err) { (void)self; (void)c; (void)value; (void)key; if (err) *err = 0; return YES; }

/* ================================================================ NSArray com blocos, NSMapTable */
/* -indexOfObjectPassingTest: / -indexOfObjectWithOptions:passingTest: (NSEnumerationReverse = 2): sem o método, o
 * "ainda não implementado" devolvia 0, que quer dizer "achou no índice 0" (o certo é NSNotFound) */
#define NS_NOT_FOUND 0x7FFFFFFFFFFFFFFFULL
static u64 a_index_test_opts(id self, SEL c, u64 opts, void *blk) {
    (void)c;
    if (!blk) return NS_NOT_FOUND;
    BOOL (*inv)(void *, id, u64, BOOL *) = *(void **)((u8 *)blk + 16);
    u64 n = MSG0(u64, self, "count");
    BOOL stop = NO;
    for (u64 k = 0; k < n && !stop; k++) {
        u64 i = (opts & 2) ? n - 1 - k : k;
        if (inv(blk, MSG(id, self, "objectAtIndex:", i), i, &stop)) return i;
    }
    return NS_NOT_FOUND;
}
static u64 a_index_test(id self, SEL c, void *blk) { return a_index_test_opts(self, c, 0, blk); }
/* NSMapTable: as variantes com referência fraca usam a tabela forte do foundation_more.c (o pior caso é segurar um
 * objeto a mais, nunca um ponteiro solto) */
static id mt_strong(id cls_, SEL c) { (void)c; return MSG0(id, cls_, "strongToStrongObjectsMapTable"); }
static id mt_with_opts(id cls_, SEL c, u64 ko, u64 vo) { (void)c; (void)ko; (void)vo; return MSG0(id, cls_, "strongToStrongObjectsMapTable"); }

/* ================================================================ registro */
static void add_if_missing(cx_class_t *k, const char *sel, void *imp) {
    if (k && !cx_responds(k, sel_registerName(sel))) cx_add_method(k, sel, imp);
}
static void add_cls_if_missing(cx_class_t *k, const char *sel, void *imp) {
    if (k && !cx_responds(k->isa, sel_registerName(sel))) cx_add_cmethod(k, sel, imp);
}
void cx_electron_compat_init(void) {
    cx_class_t *ob = cls("NSObject");
    add_if_missing(ob, "validateValue:forKey:error:", (void *)o_validate);
    add_if_missing(ob, "validateValue:forKeyPath:error:", (void *)o_validate_path);
    add_if_missing(ob, "dictionaryWithValuesForKeys:", (void *)o_dict_values);
    add_if_missing(ob, "setValuesForKeysWithDictionary:", (void *)o_set_values);
    add_if_missing(ob, "performSelector:withObject:afterDelay:", (void *)o_perform_delay);
    add_if_missing(ob, "performSelector:withObject:afterDelay:inModes:", (void *)o_perform_delay_modes);
    add_cls_if_missing(ob, "cancelPreviousPerformRequestsWithTarget:", (void *)c_cancel_all);
    add_cls_if_missing(ob, "cancelPreviousPerformRequestsWithTarget:selector:object:", (void *)c_cancel);
    cx_class_t *rl = cls("NSRunLoop");
    add_if_missing(rl, "performSelector:target:argument:order:modes:", (void *)rl_perform);
    add_if_missing(rl, "cancelPerformSelector:target:argument:", (void *)rl_cancel);
    add_if_missing(rl, "cancelPerformSelectorsWithTarget:", (void *)rl_cancel_all);
    cx_class_t *app = cls("NSApplication");
    add_if_missing(app, "postEvent:atStart:", (void *)app_post_event);
    add_if_missing(app, "setAppearance:", (void *)app_set_appearance);
    cx_class_t *view = cls("NSView");
    add_if_missing(view, "isHiddenOrHasHiddenAncestor", (void *)v_hidden_or_ancestor);
    add_if_missing(view, "allowedTouchTypes", (void *)v_touch_types);
    add_if_missing(view, "setAllowedTouchTypes:", (void *)v_set_touch_types);
    cx_class_t *ws = cls("NSWorkspace");
    static const char *a11y[] = {
        "accessibilityDisplayShouldReduceTransparency", "accessibilityDisplayShouldInvertColors",
        "accessibilityDisplayShouldIncreaseContrast", "accessibilityDisplayShouldReduceMotion",
        "accessibilityDisplayShouldDifferentiateWithoutColor", "isVoiceOverEnabled", "isSwitchControlEnabled",
    };
    for (unsigned i = 0; i < sizeof a11y / sizeof a11y[0]; i++) add_if_missing(ws, a11y[i], (void *)no_m);
    cx_class_t *aem = cls("NSAppleEventManager");
    add_if_missing(aem, "currentAppleEvent", (void *)nil_m);
    add_if_missing(aem, "currentReplyAppleEvent", (void *)nil_m);
    add_if_missing(cls("NSFontManager"), "availableMembersOfFontFamily:", (void *)fm_members);
    cx_class_t *arr = cls("NSArray");
    add_if_missing(arr, "indexOfObjectPassingTest:", (void *)a_index_test);
    add_if_missing(arr, "indexOfObjectWithOptions:passingTest:", (void *)a_index_test_opts);
    cx_class_t *mt = cls("NSMapTable");
    if (mt && cx_responds(mt->isa, sel_registerName("strongToStrongObjectsMapTable"))) {
        add_cls_if_missing(mt, "strongToWeakObjectsMapTable", (void *)mt_strong);
        add_cls_if_missing(mt, "weakToStrongObjectsMapTable", (void *)mt_strong);
        add_cls_if_missing(mt, "weakToWeakObjectsMapTable", (void *)mt_strong);
        add_cls_if_missing(mt, "mapTableWithKeyOptions:valueOptions:", (void *)mt_with_opts);
    }
    cx_class_t *url = cls("NSURL");
    add_if_missing(url, "getResourceValue:forKey:error:", (void *)url_get_value);
    add_if_missing(url, "resourceValuesForKeys:error:", (void *)url_values);
    add_if_missing(url, "setResourceValue:forKey:error:", (void *)url_set_value);
}
