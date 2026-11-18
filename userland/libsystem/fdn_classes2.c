/*
 * mOS libSystem — classes do Foundation que os apps do corpus importam e não existiam (docs/lacunas/eduardo.md):
 *  - NSFileWrapper: árvore de arquivo/pasta/link em memória, lida de uma URL e gravada de volta (writeToURL…
 *    com originalContentsURL, atualização de nomes repetidos, matchesContentsOfURL:, readFromURL:);
 *  - NSFileSecurity: dono, grupo, modo e UUID do dono (o mesmo objeto do CFFileSecurity, em slack_frameworks.c);
 *  - NSBackgroundActivityScheduler: tarefa repetida ou única com intervalo, tolerância e adiamento, sobre a
 *    libdispatch, com o bloco de conclusão (Finished/Deferred) como no macOS;
 *  - NSExtensionItem: título, texto, anexos e userInfo de uma extensão (compartilhar);
 *  - NSURLFileTypeMappings: extensão <-> MIME pelo UTType.
 */
#include "foundation.h"
#include "libc.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void cx_object_free(id o);
void cx_kvo_forget(id o);
void *_Block_copy(const void *b);
void _Block_release(const void *b);
void dispatch_after_f(u64 when, void *q, void *ctx, void (*fn)(void *));
u64 dispatch_time(u64 when, i64 delta);
void *dispatch_get_global_queue(long id, unsigned long flags);
extern char _NSConcreteGlobalBlock[];

static id cls(const char *n) { return (id)cx_class_named(n); }
static void *blk_inv(void *b) { return *(void **)((u8 *)b + 16); }
static void set_obj(id *slot, id v) { id o = *slot; *slot = v ? objc_retain(v) : 0; if (o) objc_release(o); }
static id fm(void) { return MSG0(id, cls("NSFileManager"), "defaultManager"); }
static id err_cocoa(u64 code, id url) {
    id info = url ? MSG(id, cls("NSDictionary"), "dictionaryWithObject:forKey:", url, cx_str("NSURL")) : 0;
    return MSG(id, cls("NSError"), "errorWithDomain:code:userInfo:", cx_str("NSCocoaErrorDomain"), code, info);
}

/* ================================================================ NSFileWrapper */
enum { FW_FILE = 1, FW_DIR, FW_LINK };
typedef struct {
    cx_class_t *isa;
    int kind;
    id contents;        /* NSData (arquivo) */
    id children;        /* NSMutableDictionary nome -> NSFileWrapper (pasta) */
    id link;            /* NSURL de destino (link) */
    id filename, preferred, attrs, url;
} fw_t;
#define FW(o) ((fw_t *)(o))
cx_class_t NSFileWrapper_c __asm__("OBJC_CLASS_$_NSFileWrapper");
cx_class_t NSFileWrapper_m __asm__("OBJC_METACLASS_$_NSFileWrapper");

static id fw_new(void) { return cx_alloc_instance(&NSFileWrapper_c); }
static id fw_read(id self, id url, u64 opts, id *err);
static id k_alloc(id k, SEL c) { (void)c; return cx_alloc_instance((cx_class_t *)k); }
static id fw_init_url(id self, SEL c, id url, u64 opts, id *err) {
    (void)c;
    if (!fw_read(self, url, opts, err)) { objc_release(self); return 0; }
    return self;
}
/* lê a URL: pasta (filhos recursivos), link ou arquivo; NSFileWrapperReadingImmediate = 1 lê os dados já */
static id fw_read(id self, id url, u64 opts, id *err) {
    (void)opts;
    if (err) *err = 0;
    id path = url ? MSG0(id, url, "path") : 0;
    if (!path) { if (err) *err = err_cocoa(260, url); return 0; }
    id at = ((id (*)(id, SEL, id, id *))objc_msgSend)(fm(), sel_registerName("attributesOfItemAtPath:error:"), path, (id *)0);
    if (!at) { if (err) *err = err_cocoa(260, url); return 0; }
    fw_t *w = FW(self);
    set_obj(&w->attrs, at);
    set_obj(&w->url, url);
    id last = MSG0(id, path, "lastPathComponent");
    if (!w->filename) set_obj(&w->filename, last);
    if (!w->preferred) set_obj(&w->preferred, last);
    id type = MSG(id, at, "objectForKey:", cx_str("NSFileType"));
    set_obj(&w->contents, 0); set_obj(&w->children, 0); set_obj(&w->link, 0);
    if (type && MSG(BOOL, type, "isEqualToString:", cx_str("NSFileTypeSymbolicLink"))) {
        w->kind = FW_LINK;
        id dest = ((id (*)(id, SEL, id, id *))objc_msgSend)(fm(), sel_registerName("destinationOfSymbolicLinkAtPath:error:"), path, (id *)0);
        if (dest) set_obj(&w->link, MSG(id, cls("NSURL"), "fileURLWithPath:", dest));
    } else if (type && MSG(BOOL, type, "isEqualToString:", cx_str("NSFileTypeDirectory"))) {
        w->kind = FW_DIR;
        id d = MSG0(id, cls("NSMutableDictionary"), "new");
        w->children = d;
        id names = ((id (*)(id, SEL, id, id *))objc_msgSend)(fm(), sel_registerName("contentsOfDirectoryAtPath:error:"), path, (id *)0);
        for (u64 i = 0, n = names ? MSG0(u64, names, "count") : 0; i < n; i++) {
            id nm = MSG(id, names, "objectAtIndex:", i);
            id cu = MSG(id, url, "URLByAppendingPathComponent:", nm);
            id ch = fw_new();
            set_obj(&FW(ch)->filename, nm);
            set_obj(&FW(ch)->preferred, nm);
            if (fw_read(ch, cu, opts, 0)) MSG(void, d, "setObject:forKey:", ch, nm);
            objc_release(ch);
        }
    } else {
        w->kind = FW_FILE;
        id data = MSG(id, cls("NSData"), "dataWithContentsOfURL:", url);
        if (!data) { if (err) *err = err_cocoa(256, url); return 0; }
        set_obj(&w->contents, data);
    }
    return self;
}
static id fw_init_dir(id self, SEL c, id children) {
    (void)c;
    FW(self)->kind = FW_DIR;
    FW(self)->children = MSG0(id, cls("NSMutableDictionary"), "new");
    id keys = children ? MSG0(id, children, "allKeys") : 0;
    for (u64 i = 0, n = keys ? MSG0(u64, keys, "count") : 0; i < n; i++) {
        id k = MSG(id, keys, "objectAtIndex:", i), ch = MSG(id, children, "objectForKey:", k);
        if (!FW(ch)->preferred) set_obj(&FW(ch)->preferred, k);
        set_obj(&FW(ch)->filename, k);
        MSG(void, FW(self)->children, "setObject:forKey:", ch, k);
    }
    return self;
}
static id fw_init_file(id self, SEL c, id data) {
    (void)c;
    FW(self)->kind = FW_FILE;
    set_obj(&FW(self)->contents, data ? data : MSG0(id, cls("NSData"), "data"));
    return self;
}
static id fw_init_link(id self, SEL c, id url) { (void)c; FW(self)->kind = FW_LINK; set_obj(&FW(self)->link, url); return self; }
static void fw_dealloc(id self, SEL c) {
    (void)c;
    fw_t *w = FW(self);
    set_obj(&w->contents, 0); set_obj(&w->children, 0); set_obj(&w->link, 0);
    set_obj(&w->filename, 0); set_obj(&w->preferred, 0); set_obj(&w->attrs, 0); set_obj(&w->url, 0);
    cx_kvo_forget(self);
    cx_object_free(self);
}
static BOOL fw_is_dir(id s, SEL c) { (void)c; return FW(s)->kind == FW_DIR; }
static BOOL fw_is_file(id s, SEL c) { (void)c; return FW(s)->kind == FW_FILE; }
static BOOL fw_is_link(id s, SEL c) { (void)c; return FW(s)->kind == FW_LINK; }
static id fw_children(id s, SEL c) { (void)c; return FW(s)->kind == FW_DIR ? FW(s)->children : 0; }
static id fw_contents(id s, SEL c) {
    (void)c;
    if (FW(s)->kind == FW_FILE) return FW(s)->contents;
    if (FW(s)->kind == FW_DIR) {
        id e = MSG(id, cls("NSException"), "exceptionWithName:reason:userInfo:", cx_str("NSInternalInconsistencyException"),
                   cx_str("-[NSFileWrapper regularFileContents]: not a regular file"), (id)0);
        MSG0(void, e, "raise");
    }
    return 0;
}
static id fw_link(id s, SEL c) { (void)c; return FW(s)->link; }
static id fw_filename(id s, SEL c) { (void)c; return FW(s)->filename; }
static void fw_set_filename(id s, SEL c, id v) { (void)c; set_obj(&FW(s)->filename, v); }
static id fw_preferred(id s, SEL c) { (void)c; return FW(s)->preferred; }
static void fw_set_preferred(id s, SEL c, id v) { (void)c; set_obj(&FW(s)->preferred, v); }
static id fw_attrs(id s, SEL c) { (void)c; return FW(s)->attrs ? FW(s)->attrs : MSG0(id, cls("NSDictionary"), "dictionary"); }
static void fw_set_attrs(id s, SEL c, id v) { (void)c; set_obj(&FW(s)->attrs, v); }
/* nome único na pasta: "nome", "2__#$!@%!#__nome"… como a Apple */
static id unique_key(id dir, id want) {
    if (!MSG(id, dir, "objectForKey:", want)) return want;
    for (int i = 2;; i++) {
        char b[600];
        snprintf(b, sizeof b, "%d__#$!@%%!#__%s", i, MSG0(const char *, want, "UTF8String"));
        id k = cx_str(b);
        if (!MSG(id, dir, "objectForKey:", k)) return k;
    }
}
static id fw_add(id s, SEL c, id child) {
    (void)c;
    if (FW(s)->kind != FW_DIR || !child) return 0;
    id want = FW(child)->preferred ? FW(child)->preferred : FW(child)->filename;
    if (!want) {
        id e = MSG(id, cls("NSException"), "exceptionWithName:reason:userInfo:", cx_str("NSInvalidArgumentException"),
                   cx_str("-[NSFileWrapper addFileWrapper:]: child has no preferredFilename"), (id)0);
        MSG0(void, e, "raise");
        return 0;
    }
    id k = unique_key(FW(s)->children, want);
    set_obj(&FW(child)->filename, k);
    MSG(void, FW(s)->children, "setObject:forKey:", child, k);
    return k;
}
static id fw_add_file(id s, SEL c, id data, id name) {
    id ch = fw_init_file(fw_new(), 0, data);
    set_obj(&FW(ch)->preferred, name);
    id k = fw_add(s, c, ch);
    objc_release(ch);
    return k;
}
static void fw_remove(id s, SEL c, id child) {
    (void)c;
    if (FW(s)->kind != FW_DIR || !child) return;
    id keys = MSG(id, FW(s)->children, "allKeysForObject:", child);
    for (u64 i = 0, n = keys ? MSG0(u64, keys, "count") : 0; i < n; i++) {
        id k = MSG(id, keys, "objectAtIndex:", i);
        MSG(void, FW(s)->children, "removeObjectForKey:", k);
    }
}
static id fw_key_for(id s, SEL c, id child) {
    (void)c;
    if (FW(s)->kind != FW_DIR) return 0;
    id keys = MSG(id, FW(s)->children, "allKeysForObject:", child);
    return keys && MSG0(u64, keys, "count") ? MSG(id, keys, "objectAtIndex:", (u64)0) : 0;
}
static BOOL fw_write_at(id s, id url, int with_names, id *err) {
    fw_t *w = FW(s);
    id path = MSG0(id, url, "path");
    BOOL ok = YES;
    if (w->kind == FW_FILE) ok = MSG(BOOL, w->contents ? w->contents : MSG0(id, cls("NSData"), "data"), "writeToURL:atomically:", url, YES);
    else if (w->kind == FW_LINK) {
        id dest = w->link ? MSG0(id, w->link, "path") : cx_str("");
        ok = ((BOOL (*)(id, SEL, id, id, id *))objc_msgSend)(fm(), sel_registerName("createSymbolicLinkAtPath:withDestinationPath:error:"), path, dest, (id *)0);
    } else {
        ok = ((BOOL (*)(id, SEL, id, BOOL, id, id *))objc_msgSend)(fm(), sel_registerName("createDirectoryAtPath:withIntermediateDirectories:attributes:error:"), path, YES, (id)0, (id *)0);
        id keys = MSG0(id, w->children, "allKeys");
        for (u64 i = 0, n = MSG0(u64, keys, "count"); ok && i < n; i++) {
            id k = MSG(id, keys, "objectAtIndex:", i);
            id ch = MSG(id, w->children, "objectForKey:", k);
            ok = fw_write_at(ch, MSG(id, url, "URLByAppendingPathComponent:", k), with_names, err);
        }
    }
    if (!ok) { if (err && !*err) *err = err_cocoa(512, url); return NO; }
    if (w->attrs && w->kind != FW_LINK) {
        id keep = MSG0(id, cls("NSMutableDictionary"), "dictionary");
        static const char *ks[] = { "NSFilePosixPermissions", "NSFileModificationDate", "NSFileCreationDate", "NSFileExtensionHidden" };
        for (int i = 0; i < 4; i++) { id v = MSG(id, w->attrs, "objectForKey:", cx_str(ks[i])); if (v) MSG(void, keep, "setObject:forKey:", v, cx_str(ks[i])); }
        ((BOOL (*)(id, SEL, id, id, id *))objc_msgSend)(fm(), sel_registerName("setAttributes:ofItemAtPath:error:"), keep, path, (id *)0);
    }
    if (with_names) set_obj(&w->filename, MSG0(id, path, "lastPathComponent"));
    set_obj(&w->url, url);
    return YES;
}
/* NSFileWrapperWritingAtomic 1, WithNameUpdating 2 */
static BOOL fw_write(id s, SEL c, id url, u64 opts, id orig, id *err) {
    (void)c; (void)orig;
    if (err) *err = 0;
    if (!url) return NO;
    if (!(opts & 1)) return fw_write_at(s, url, (opts & 2) != 0, err);
    /* atômico: grava ao lado e troca */
    id tmp = MSG(id, url, "URLByAppendingPathExtension:", cx_str("mos-tmp"));
    ((BOOL (*)(id, SEL, id, id *))objc_msgSend)(fm(), sel_registerName("removeItemAtURL:error:"), tmp, (id *)0);
    if (!fw_write_at(s, tmp, 0, err)) return NO;
    ((BOOL (*)(id, SEL, id, id *))objc_msgSend)(fm(), sel_registerName("removeItemAtURL:error:"), url, (id *)0);
    BOOL ok = ((BOOL (*)(id, SEL, id, id, id *))objc_msgSend)(fm(), sel_registerName("moveItemAtURL:toURL:error:"), tmp, url, err);
    if (ok) { if (opts & 2) set_obj(&FW(s)->filename, MSG0(id, url, "lastPathComponent")); set_obj(&FW(s)->url, url); }
    return ok;
}
static BOOL fw_matches(id s, SEL c, id url) {
    (void)c;
    id path = url ? MSG0(id, url, "path") : 0;
    id at = path ? ((id (*)(id, SEL, id, id *))objc_msgSend)(fm(), sel_registerName("attributesOfItemAtPath:error:"), path, (id *)0) : 0;
    if (!at || !FW(s)->attrs) return NO;
    id a = MSG(id, at, "objectForKey:", cx_str("NSFileModificationDate")), b = MSG(id, FW(s)->attrs, "objectForKey:", cx_str("NSFileModificationDate"));
    id sa = MSG(id, at, "objectForKey:", cx_str("NSFileSize")), sb = MSG(id, FW(s)->attrs, "objectForKey:", cx_str("NSFileSize"));
    return a && b && MSG(BOOL, a, "isEqual:", b) && (!sa || !sb || MSG(BOOL, sa, "isEqual:", sb));
}
static BOOL fw_read_url(id s, SEL c, id url, u64 opts, id *err) { (void)c; return fw_read(s, url, opts, err) != 0; }

/* ================================================================ NSFileSecurity (= CFFileSecurity) */
cx_class_t NSFileSecurity_c __asm__("OBJC_CLASS_$_NSFileSecurity");
cx_class_t NSFileSecurity_m __asm__("OBJC_METACLASS_$_NSFileSecurity");
/* o estado de CFFileSecurity (slack_frameworks.c) mora logo depois do isa */
void *cx_filesecurity_new(void) { return cx_alloc_instance(&NSFileSecurity_c); }
static id fs_copy(id s, SEL c) {
    (void)c;
    id r = cx_alloc_instance(&NSFileSecurity_c);
    memcpy((u8 *)r + 8, (u8 *)s + 8, 40);
    return r;
}
static id fs_copy_zone(id s, SEL c, void *z) { (void)z; return fs_copy(s, c); }
static u64 fs_typeid(id s, SEL c) { (void)s; (void)c; return 44; }

/* ================================================================ NSBackgroundActivityScheduler */
typedef struct {
    cx_class_t *isa;
    id identifier;
    double interval, tolerance, delay;
    long qos;
    BOOL repeats, valid, running;
    void *block;
    unsigned gen;
} bas_t;
#define BS(o) ((bas_t *)(o))
cx_class_t NSBackgroundActivityScheduler_c __asm__("OBJC_CLASS_$_NSBackgroundActivityScheduler");
cx_class_t NSBackgroundActivityScheduler_m __asm__("OBJC_METACLASS_$_NSBackgroundActivityScheduler");
typedef struct { void *isa; int flags, reserved; void *invoke; void *desc; id sched; unsigned gen; } done_blk_t;
static struct { u64 r, sz; } done_desc = { 0, sizeof(done_blk_t) };
static void bas_arm(id self, double after);
static void bas_tick(void *ctx);
/* o bloco de conclusão: NSBackgroundActivityResultFinished 1, Deferred 2 */
static void done_invoke(done_blk_t *b, long result) {
    id s = b->sched;
    BS(s)->running = NO;
    if (BS(s)->valid && b->gen == BS(s)->gen) {
        if (result == 2) bas_arm(s, BS(s)->tolerance > 0 ? BS(s)->tolerance : 60);
        else if (BS(s)->repeats) bas_arm(s, BS(s)->interval > 0 ? BS(s)->interval : 600);
    }
    objc_release(s);
    free(b);
}
typedef struct { id s; unsigned gen; } tick_t;
static void bas_arm(id self, double after) {
    tick_t *t = malloc(sizeof *t);
    t->s = objc_retain(self); t->gen = BS(self)->gen;
    void *q = dispatch_get_global_queue(BS(self)->qos == 33 ? 2 : BS(self)->qos == 9 ? -32768 : -2, 0);
    dispatch_after_f(dispatch_time(0, (i64)(after * 1e9)), q, t, bas_tick);
}
static void bas_tick(void *ctx) {
    tick_t *t = ctx;
    id s = t->s;
    if (BS(s)->valid && t->gen == BS(s)->gen && BS(s)->block && !BS(s)->running) {
        BS(s)->running = YES;
        done_blk_t *d = calloc(1, sizeof *d);
        d->isa = _NSConcreteGlobalBlock; d->flags = 1 << 28; d->invoke = (void *)done_invoke; d->desc = &done_desc;
        d->sched = objc_retain(s); d->gen = t->gen;
        ((void (*)(void *, void *))blk_inv(BS(s)->block))(BS(s)->block, d);
    }
    objc_release(s);
    free(t);
}
static id bas_init(id self, SEL c, id ident) {
    (void)c;
    set_obj(&BS(self)->identifier, ident);
    BS(self)->qos = 9;   /* NSQualityOfServiceBackground */
    BS(self)->tolerance = -1;
    return self;
}
static void bas_schedule(id self, SEL c, void *blk) {
    (void)c;
    if (!blk) return;
    if (BS(self)->block) _Block_release(BS(self)->block);
    BS(self)->block = _Block_copy(blk);
    BS(self)->valid = YES;
    BS(self)->gen++;
    double first = BS(self)->delay > 0 ? BS(self)->delay : BS(self)->repeats ? (BS(self)->interval > 0 ? BS(self)->interval : 600) : 0;
    bas_arm(self, first);
}
static void bas_invalidate(id self, SEL c) { (void)c; BS(self)->valid = NO; BS(self)->gen++; }
static BOOL bas_should_defer(id self, SEL c) { (void)self; (void)c; return NO; }   /* sem bateria/carga para ceder */
static id bas_ident(id s, SEL c) { (void)c; return BS(s)->identifier; }
static double bas_interval(id s, SEL c) { (void)c; return BS(s)->interval; }
static void bas_set_interval(id s, SEL c, double v) { (void)c; BS(s)->interval = v; if (BS(s)->tolerance < 0) BS(s)->tolerance = v / 2; }
static double bas_tolerance(id s, SEL c) { (void)c; return BS(s)->tolerance < 0 ? BS(s)->interval / 2 : BS(s)->tolerance; }
static void bas_set_tolerance(id s, SEL c, double v) { (void)c; BS(s)->tolerance = v; }
static double bas_delay(id s, SEL c) { (void)c; return BS(s)->delay; }
static void bas_set_delay(id s, SEL c, double v) { (void)c; BS(s)->delay = v; }
static BOOL bas_repeats(id s, SEL c) { (void)c; return BS(s)->repeats; }
static void bas_set_repeats(id s, SEL c, BOOL v) { (void)c; BS(s)->repeats = v; }
static long bas_qos(id s, SEL c) { (void)c; return BS(s)->qos; }
static void bas_set_qos(id s, SEL c, long v) { (void)c; BS(s)->qos = v; }
static void bas_dealloc(id s, SEL c) {
    (void)c;
    set_obj(&BS(s)->identifier, 0);
    if (BS(s)->block) _Block_release(BS(s)->block);
    cx_object_free(s);
}

/* ================================================================ NSExtensionItem */
typedef struct { cx_class_t *isa; id title, content, attachments, info; } ei_t;
#define EI(o) ((ei_t *)(o))
cx_class_t NSExtensionItem_c __asm__("OBJC_CLASS_$_NSExtensionItem");
cx_class_t NSExtensionItem_m __asm__("OBJC_METACLASS_$_NSExtensionItem");
static id ei_title(id s, SEL c) { (void)c; return EI(s)->title; }
static void ei_set_title(id s, SEL c, id v) { (void)c; id x = v ? MSG0(id, v, "copy") : 0; set_obj(&EI(s)->title, x); if (x) objc_release(x); }
static id ei_content(id s, SEL c) { (void)c; return EI(s)->content; }
static void ei_set_content(id s, SEL c, id v) { (void)c; id x = v ? MSG0(id, v, "copy") : 0; set_obj(&EI(s)->content, x); if (x) objc_release(x); }
static id ei_att(id s, SEL c) { (void)c; return EI(s)->attachments; }
static void ei_set_att(id s, SEL c, id v) { (void)c; id x = v ? MSG0(id, v, "copy") : 0; set_obj(&EI(s)->attachments, x); if (x) objc_release(x); }
static id ei_info(id s, SEL c) { (void)c; return EI(s)->info; }
static void ei_set_info(id s, SEL c, id v) { (void)c; id x = v ? MSG0(id, v, "copy") : 0; set_obj(&EI(s)->info, x); if (x) objc_release(x); }
static id ei_copy(id s, SEL c) {
    (void)c;
    id r = cx_alloc_instance(&NSExtensionItem_c);
    set_obj(&EI(r)->title, EI(s)->title); set_obj(&EI(r)->content, EI(s)->content);
    set_obj(&EI(r)->attachments, EI(s)->attachments); set_obj(&EI(r)->info, EI(s)->info);
    return r;
}
static id ei_copy_zone(id s, SEL c, void *z) { (void)z; return ei_copy(s, c); }
static void ei_dealloc(id s, SEL c) {
    (void)c;
    set_obj(&EI(s)->title, 0); set_obj(&EI(s)->content, 0); set_obj(&EI(s)->attachments, 0); set_obj(&EI(s)->info, 0);
    cx_object_free(s);
}

/* ================================================================ NSURLFileTypeMappings */
cx_class_t NSURLFileTypeMappings_c __asm__("OBJC_CLASS_$_NSURLFileTypeMappings");
cx_class_t NSURLFileTypeMappings_m __asm__("OBJC_METACLASS_$_NSURLFileTypeMappings");
static id ftm_shared(id k, SEL c) {
    (void)c;
    static id one;
    if (!one) one = cx_alloc_instance((cx_class_t *)k);
    return one;
}
static id uttype_ext(id ext) { id UT = cls("UTType"); return UT && ext ? MSG(id, UT, "typeWithFilenameExtension:", ext) : 0; }
static id uttype_mime(id m) { id UT = cls("UTType"); return UT && m ? MSG(id, UT, "typeWithMIMEType:", m) : 0; }
static id ftm_mime_for_ext(id s, SEL c, id ext) { (void)s; (void)c; id t = uttype_ext(ext); return t ? MSG0(id, t, "preferredMIMEType") : 0; }
static id ftm_ext_for_mime(id s, SEL c, id m) { (void)s; (void)c; id t = uttype_mime(m); return t ? MSG0(id, t, "preferredFilenameExtension") : 0; }
static id ftm_exts_for_mime(id s, SEL c, id m) {
    (void)s; (void)c;
    id t = uttype_mime(m);
    id tags = t ? MSG0(id, t, "tags") : 0;
    id e = tags ? MSG(id, tags, "objectForKey:", cx_str("public.filename-extension")) : 0;
    if (e) return e;
    id one = t ? MSG0(id, t, "preferredFilenameExtension") : 0;
    return one ? MSG(id, cls("NSArray"), "arrayWithObject:", one) : MSG0(id, cls("NSArray"), "array");
}

void cx_fdn_classes2_init(void) {
    cx_class_t *ob = cx_class_named("NSObject");
    if (!ob || cx_class_named("NSFileWrapper") == &NSFileWrapper_c) return;
    cx_class_t *k;
#define M(s, f) cx_add_method(k, s, (void *)(f))
    k = &NSFileWrapper_c;
    cx_define_class(k, &NSFileWrapper_m, "NSFileWrapper", ob, (u32)(sizeof(fw_t) - 8));
    cx_add_cmethod(k, "alloc", (void *)k_alloc);
    M("initWithURL:options:error:", fw_init_url); M("initDirectoryWithFileWrappers:", fw_init_dir);
    M("initRegularFileWithContents:", fw_init_file); M("initSymbolicLinkWithDestinationURL:", fw_init_link);
    M("dealloc", fw_dealloc); M("isDirectory", fw_is_dir); M("isRegularFile", fw_is_file); M("isSymbolicLink", fw_is_link);
    M("fileWrappers", fw_children); M("regularFileContents", fw_contents); M("symbolicLinkDestinationURL", fw_link);
    M("filename", fw_filename); M("setFilename:", fw_set_filename);
    M("preferredFilename", fw_preferred); M("setPreferredFilename:", fw_set_preferred);
    M("fileAttributes", fw_attrs); M("setFileAttributes:", fw_set_attrs);
    M("addFileWrapper:", fw_add); M("addRegularFileWithContents:preferredFilename:", fw_add_file);
    M("removeFileWrapper:", fw_remove); M("keyForFileWrapper:", fw_key_for);
    M("writeToURL:options:originalContentsURL:error:", fw_write); M("matchesContentsOfURL:", fw_matches);
    M("readFromURL:options:error:", fw_read_url);
    k = &NSFileSecurity_c;
    cx_define_class(k, &NSFileSecurity_m, "NSFileSecurity", ob, 40);
    M("copy", fs_copy); M("copyWithZone:", fs_copy_zone); M("_cfTypeID", fs_typeid);
    k = &NSBackgroundActivityScheduler_c;
    cx_define_class(k, &NSBackgroundActivityScheduler_m, "NSBackgroundActivityScheduler", ob, (u32)(sizeof(bas_t) - 8));
    cx_add_cmethod(k, "alloc", (void *)k_alloc);
    M("initWithIdentifier:", bas_init); M("scheduleWithBlock:", bas_schedule); M("invalidate", bas_invalidate);
    M("shouldDefer", bas_should_defer); M("identifier", bas_ident);
    M("interval", bas_interval); M("setInterval:", bas_set_interval); M("tolerance", bas_tolerance); M("setTolerance:", bas_set_tolerance);
    M("delay", bas_delay); M("setDelay:", bas_set_delay); M("repeats", bas_repeats); M("setRepeats:", bas_set_repeats);
    M("qualityOfService", bas_qos); M("setQualityOfService:", bas_set_qos); M("dealloc", bas_dealloc);
    k = &NSExtensionItem_c;
    cx_define_class(k, &NSExtensionItem_m, "NSExtensionItem", ob, (u32)(sizeof(ei_t) - 8));
    M("attributedTitle", ei_title); M("setAttributedTitle:", ei_set_title);
    M("attributedContentText", ei_content); M("setAttributedContentText:", ei_set_content);
    M("attachments", ei_att); M("setAttachments:", ei_set_att); M("userInfo", ei_info); M("setUserInfo:", ei_set_info);
    M("copy", ei_copy); M("copyWithZone:", ei_copy_zone); M("dealloc", ei_dealloc);
    k = &NSURLFileTypeMappings_c;
    cx_define_class(k, &NSURLFileTypeMappings_m, "NSURLFileTypeMappings", ob, 0);
    cx_add_cmethod(k, "sharedMappings", (void *)ftm_shared);
    M("MIMETypeForExtension:", ftm_mime_for_ext); M("preferredExtensionForMIMEType:", ftm_ext_for_mime);
    M("extensionsForMIMEType:", ftm_exts_for_mime);
#undef M
}
