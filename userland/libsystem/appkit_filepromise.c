/*
 * mOS libSystem — NSFilePromiseProvider / NSFilePromiseReceiver (arrastar um arquivo que ainda não existe: o
 * destino escolhe a pasta e a fonte grava lá). Antes as duas classes não tinham métodos.
 *
 * Provider (NSPasteboardWriting): initWithFileType:delegate:, fileType, delegate, userInfo; no pasteboard escreve a
 *   UTI do conteúdo prometido e, para o mesmo processo, o próprio provider (com.apple.mos.filepromise).
 * Receiver (NSPasteboardReading): readableDraggedTypes / readableTypesForPasteboard:, fileTypes, fileNames,
 *   receivePromisedFilesAtDestination:options:operationQueue:reader: — pede o nome ao delegate
 *   (filePromiseProvider:fileNameForType:), chama filePromiseProvider:writePromiseToURL:completionHandler: e, quando o
 *   app termina de gravar, entrega reader(URL, erro) na fila pedida.
 * Limite: o arrasto de promessa só funciona entre views do mesmo processo (não há transporte de objetos entre
 * processos); o resto do arrasto (URLs, texto, imagens) já passa pelo pasteboard.
 * Registro: cx_appkit_filepromise_init(), chamado do cx_appkit_input_init depois do appkit_postico.
 */
#include "foundation.h"
#include "sys.h"
#include "libc.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void objc_setAssociatedObject(id obj, const void *key, id value, u64 policy);
id objc_getAssociatedObject(id obj, const void *key);
void *_Block_copy(const void *b);
void _Block_release(const void *b);
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *));
void *dispatch_get_main_queue(void);
extern char _NSConcreteGlobalBlock[];
void free(void *p);

static cx_class_t *kls(const char *n) { return cx_class_named(n); }
static SEL sl(const char *n) { return sel_registerName(n); }
static BOOL responds(id o, const char *s) { return o && cx_responds(object_getClass(o), sl(s)); }
#define T_PROMISE "com.apple.mos.filepromise"
#define T_CONTENT "com.apple.pasteboard.promised-file-content-type"
#define T_URL "com.apple.pasteboard.promised-file-url"
#define T_META "com.apple.NSFilePromiseItemMetaData"

static char k_type, k_dl, k_info, k_providers;

/* ---- provider */
static id fpp_init(id self, SEL c, id type, id dl) {
    (void)c;
    self = MSG0(id, self, "init");
    if (!self) return 0;
    objc_setAssociatedObject(self, &k_type, type, 1);
    objc_setAssociatedObject(self, &k_dl, dl, 0);   /* fraco, como no Cocoa */
    return self;
}
static id fpp_type(id s, SEL c) { (void)c; return objc_getAssociatedObject(s, &k_type); }
static void fpp_set_type(id s, SEL c, id t) { (void)c; objc_setAssociatedObject(s, &k_type, t, 1); }
static id fpp_dl(id s, SEL c) { (void)c; return objc_getAssociatedObject(s, &k_dl); }
static void fpp_set_dl(id s, SEL c, id d) { (void)c; objc_setAssociatedObject(s, &k_dl, d, 0); }
static id fpp_info(id s, SEL c) { (void)c; return objc_getAssociatedObject(s, &k_info); }
static void fpp_set_info(id s, SEL c, id i) { (void)c; objc_setAssociatedObject(s, &k_info, i, 1); }
static id fpp_writable(id s, SEL c, id pb) {
    (void)s; (void)c; (void)pb;
    id a = MSG0(id, (id)kls("NSMutableArray"), "array");
    const char *t[] = {T_PROMISE, T_CONTENT, T_URL, T_META};
    for (int i = 0; i < 4; i++) MSG(void, a, "addObject:", cx_str(t[i]));
    return a;
}
static id fpp_plist(id self, SEL c, id type) {
    (void)c;
    const char *t = type ? cx_cstr(type) : "";
    if (!strcmp(t, T_PROMISE)) return self;            /* no mesmo processo: o objeto */
    if (!strcmp(t, T_CONTENT) || !strcmp(t, T_META)) return objc_getAssociatedObject(self, &k_type);
    if (!strcmp(t, T_URL)) return cx_str("");          /* a URL só existe quando o destino escolhe a pasta */
    return 0;
}

/* ---- receiver */
static id fpr_readable(id k, SEL c) {
    (void)k; (void)c;
    id a = MSG0(id, (id)kls("NSMutableArray"), "array");
    MSG(void, a, "addObject:", cx_str(T_PROMISE));
    MSG(void, a, "addObject:", cx_str(T_URL));
    MSG(void, a, "addObject:", cx_str(T_META));
    return a;
}
static id fpr_readable_pb(id k, SEL c, id pb) { (void)pb; return fpr_readable(k, c); }
static id fpr_init_plist(id self, SEL c, id plist, id type) {
    (void)c; (void)type;
    self = MSG0(id, self, "init");
    if (!self) return 0;
    id a = MSG0(id, (id)kls("NSMutableArray"), "array");
    if (plist && cx_is_subclass(object_getClass(plist), kls("NSFilePromiseProvider"))) MSG(void, a, "addObject:", plist);
    objc_setAssociatedObject(self, &k_providers, a, 1);
    return self;
}
static id provs(id self) { id a = objc_getAssociatedObject(self, &k_providers); return a ? a : MSG0(id, (id)kls("NSArray"), "array"); }
static id fpr_types(id self, SEL c) {
    (void)c;
    id out = MSG0(id, (id)kls("NSMutableArray"), "array");
    id a = provs(self);
    u64 n = MSG0(u64, a, "count");
    for (u64 i = 0; i < n; i++) { id t = fpp_type(MSG(id, a, "objectAtIndex:", i), 0); if (t) MSG(void, out, "addObject:", t); }
    return out;
}
static id prov_name(id p) {
    id dl = fpp_dl(p, 0);
    id t = fpp_type(p, 0);
    id n = responds(dl, "filePromiseProvider:fileNameForType:") ? MSG(id, dl, "filePromiseProvider:fileNameForType:", p, t) : 0;
    return n ? n : cx_str("Sem t\xC3\xADtulo");
}
static id fpr_names(id self, SEL c) {
    (void)c;
    id out = MSG0(id, (id)kls("NSMutableArray"), "array");
    id a = provs(self);
    u64 n = MSG0(u64, a, "count");
    for (u64 i = 0; i < n; i++) { id p = MSG(id, a, "objectAtIndex:", i); MSG(void, out, "addObject:", prov_name(p)); }
    return out;
}

/* bloco de conclusão que o app chama: completionHandler(NSError *) */
typedef struct { void *isa; int flags, reserved; void *invoke; void *desc; } blk_t;
typedef struct { blk_t b; id url; void *reader; void *queue; int fired; } done_t;
static struct { unsigned long reserved, size; } done_desc = {0, sizeof(done_t)};
typedef struct { done_t *d; id err; } deliver_t;
static void deliver(void *ctx) {
    deliver_t *x = ctx;
    done_t *d = x->d;
    if (d->reader) ((void (*)(void *, id, id))(((blk_t *)d->reader)->invoke))(d->reader, d->url, x->err);
    objc_release(x->err);
    objc_release(d->url);
    if (d->reader) _Block_release(d->reader);
    free(x);
    free(d);   /* o app já chamou o handler: o bloco global não é mais usado */
}
static void done_inv(void *b, id err) {
    done_t *d = b;
    if (d->fired) return;
    d->fired = 1;
    deliver_t *x = calloc(1, sizeof *x);
    if (!x) return;
    x->d = d;
    x->err = objc_retain(err);
    void *q = d->queue;
    if (q && responds((id)q, "addOperationWithBlock:")) {   /* NSOperationQueue: a fila principal do GCD basta para o reader */
        id mq = MSG0(id, (id)kls("NSOperationQueue"), "mainQueue");
        if ((id)q != mq) { dispatch_async_f(dispatch_get_main_queue(), x, deliver); return; }
    }
    dispatch_async_f(dispatch_get_main_queue(), x, deliver);
}
static void fpr_receive(id self, SEL c, id dest, id opts, id queue, void *reader) {
    (void)c; (void)opts;
    id a = provs(self);
    u64 n = MSG0(u64, a, "count");
    id dpath = dest ? MSG0(id, dest, "path") : 0;
    for (u64 i = 0; i < n; i++) {
        id p = MSG(id, a, "objectAtIndex:", i);
        id dl = fpp_dl(p, 0);
        id name = prov_name(p);
        id full = dpath ? MSG(id, dpath, "stringByAppendingPathComponent:", name) : name;
        id url = MSG(id, (id)kls("NSURL"), "fileURLWithPath:", full);
        done_t *d = calloc(1, sizeof *d);
        if (!d) continue;
        d->b.isa = _NSConcreteGlobalBlock; d->b.flags = 1 << 28; d->b.invoke = (void *)done_inv; d->b.desc = &done_desc;
        d->url = objc_retain(url);
        d->reader = reader ? _Block_copy(reader) : 0;
        d->queue = queue;
        if (responds(dl, "filePromiseProvider:writePromiseToURL:completionHandler:"))
            MSG(void, dl, "filePromiseProvider:writePromiseToURL:completionHandler:", p, url, (void *)d);
        else {
            id info = MSG(id, (id)kls("NSDictionary"), "dictionaryWithObject:forKey:", cx_str("A fonte n\xC3\xA3o grava o arquivo prometido."), cx_str("NSLocalizedDescription"));
            done_inv(d, MSG(id, (id)kls("NSError"), "errorWithDomain:code:userInfo:", cx_str("NSCocoaErrorDomain"), (i64)512, info));
        }
    }
}

#define ADD(cls, sel, fn) cx_add_method(cls, sel, (void *)(fn))
void cx_appkit_filepromise_init(void) {
    cx_class_t *p = kls("NSFilePromiseProvider"), *r = kls("NSFilePromiseReceiver");
    if (p) {
        ADD(p, "initWithFileType:delegate:", fpp_init); ADD(p, "fileType", fpp_type); ADD(p, "setFileType:", fpp_set_type);
        ADD(p, "delegate", fpp_dl); ADD(p, "setDelegate:", fpp_set_dl); ADD(p, "userInfo", fpp_info); ADD(p, "setUserInfo:", fpp_set_info);
        ADD(p, "writableTypesForPasteboard:", fpp_writable); ADD(p, "pasteboardPropertyListForType:", fpp_plist);
    }
    if (r) {
        cx_add_cmethod(r, "readableDraggedTypes", (void *)fpr_readable);
        cx_add_cmethod(r, "readableTypesForPasteboard:", (void *)fpr_readable_pb);
        ADD(r, "initWithPasteboardPropertyList:ofType:", fpr_init_plist);
        ADD(r, "fileTypes", fpr_types); ADD(r, "fileNames", fpr_names);
        ADD(r, "receivePromisedFilesAtDestination:options:operationQueue:reader:", fpr_receive);
    }
}
