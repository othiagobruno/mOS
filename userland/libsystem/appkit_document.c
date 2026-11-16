/*
 * mOS libSystem — arquitetura de documentos do AppKit: NSDocument, NSDocumentController e os pedaços dela no
 * NSWindowController/NSWindow. Antes: NSDocument guardava fileURL/fileType num dicionário e mostrava janelas;
 * NSDocumentController só lembrava recentes em memória. Nada abria, salvava ou perguntava ao fechar.
 *
 * NSDocument: initWithType:error:, initWithContentsOfURL:ofType:error:, initForURL:withContentsOfURL:ofType:error:,
 *   leitura por readFromURL/readFromFileWrapper/readFromData, gravação por writeSafelyToURL → writeToURL → dataOfType
 *   ou fileWrapperOfType, saveDocument:/saveDocumentAs:/saveDocumentTo:/revertDocumentToSaved:, painel de salvar
 *   (prepareSavePanel:, escolha do tipo quando há mais de um gravável), contagem de mudanças (updateChangeCount:,
 *   tokens), undoManager ligado às notificações do NSUndoManager, autosave no lugar (autosavesInPlace), fechar com
 *   "Salvar / Cancelar / Não Salvar", janelas (windowNibName, makeWindowControllers, título "nome — Editado"),
 *   presentError:, validateUserInterfaceItem:.
 * NSDocumentController: compartilhado (a 1ª instância criada vira a compartilhada, como no Cocoa), documentos, novo,
 *   abrir (painel com as extensões do Info.plist), recentes gravados no NSUserDefaults, tipos (CFBundleDocumentTypes),
 *   revisão dos não salvos ao sair (NSApp terminate:) e o documento sem título no lançamento.
 * Decisões puras no núcleo testado no host: appkit_doc_core.c, userland/tests-appkit/document_host.c.
 * Registro: cx_appkit_document_init(), chamado do cx_appkit_input_init depois do appkit_postico.
 */
#include "foundation.h"
#include "sys.h"
#include "libc.h"
#include "appkit_doc_core.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void objc_setAssociatedObject(id obj, const void *key, id value, u64 policy);
id objc_getAssociatedObject(id obj, const void *key);
IMP class_getMethodImplementation(cx_class_t *c, SEL s);
void *_Block_copy(const void *b);
void _Block_release(const void *b);
extern id NSApp;
void free(void *p);

static cx_class_t *kls(const char *n) { return cx_class_named(n); }
static SEL sl(const char *n) { return sel_registerName(n); }
static BOOL responds(id o, const char *s) { return o && cx_responds(object_getClass(o), sl(s)); }
static id str(const char *s) { return cx_str(s); }
static const char *cs(id s) { return s && cx_is_str(s) ? cx_cstr(s) : ""; }
static id url_path(id u) { return u ? (cx_is_str(u) ? u : MSG0(id, u, "path")) : 0; }
static id file_url(id path) { return path ? MSG(id, (id)kls("NSURL"), "fileURLWithPath:", path) : 0; }
static void blk_call3(void *blk, id a, BOOL b, id c) { if (blk) ((void (*)(void *, id, BOOL, id))(*(void **)((u8 *)blk + 16)))(blk, a, b, c); }
static void blk_call1(void *blk, id a) { if (blk) ((void (*)(void *, id))(*(void **)((u8 *)blk + 16)))(blk, a); }
static id make_error(i64 code, const char *desc) {
    id info = MSG(id, (id)kls("NSDictionary"), "dictionaryWithObject:forKey:", str(desc), str("NSLocalizedDescription"));
    return MSG(id, (id)kls("NSError"), "errorWithDomain:code:userInfo:", str("NSCocoaErrorDomain"), code, info);
}
static IMP base_imp(const char *cls, const char *sel) { return class_getMethodImplementation(kls(cls), sl(sel)); }
static BOOL overrides(id o, const char *base, const char *sel) { return cx_lookup(object_getClass(o), sl(sel)) != base_imp(base, sel); }

/* ================================================================ tipos do Info.plist */
static dc_type_t g_types[64];
static int g_ntypes = -1;
static void load_types(void) {
    if (g_ntypes >= 0) return;
    g_ntypes = 0;
    id b = MSG0(id, (id)kls("NSBundle"), "mainBundle");
    id info = b ? MSG0(id, b, "infoDictionary") : 0;
    id arr = info ? MSG(id, info, "objectForKey:", str("CFBundleDocumentTypes")) : 0;
    u64 n = arr && responds(arr, "count") ? MSG0(u64, arr, "count") : 0;
    for (u64 i = 0; i < n && g_ntypes < 64; i++) {
        id d = MSG(id, arr, "objectAtIndex:", i);
        if (!d || !responds(d, "objectForKey:")) continue;
        id name = MSG(id, d, "objectForKey:", str("CFBundleTypeName"));
        id role = MSG(id, d, "objectForKey:", str("CFBundleTypeRole"));
        id cls = MSG(id, d, "objectForKey:", str("NSDocumentClass"));
        id utis = MSG(id, d, "objectForKey:", str("LSItemContentTypes"));
        id exts = MSG(id, d, "objectForKey:", str("CFBundleTypeExtensions"));
        if (!name && utis && MSG0(u64, utis, "count")) name = MSG(id, utis, "objectAtIndex:", (u64)0);
        if (!name) continue;
        dc_type_t *t = &g_types[g_ntypes++];
        dc_type_init(t, cs(name), role ? cs(role) : 0, cls ? cs(cls) : "");
        u64 ne = exts ? MSG0(u64, exts, "count") : 0;
        for (u64 k = 0; k < ne; k++) dc_type_add_ext(t, cs(MSG(id, exts, "objectAtIndex:", k)));
        u64 nu = utis ? MSG0(u64, utis, "count") : 0;
        for (u64 k = 0; k < nu; k++) {
            id u = MSG(id, utis, "objectAtIndex:", k);
            dc_type_add_uti(t, cs(u));
            /* extensão da UTI exportada/importada pelo app, ou do UTType do sistema */
            id ut = kls("UTType") ? MSG(id, (id)kls("UTType"), "typeWithIdentifier:", u) : 0;
            id pe = ut && responds(ut, "preferredFilenameExtension") ? MSG0(id, ut, "preferredFilenameExtension") : 0;
            if (pe && cx_is_str(pe) && !ne) dc_type_add_ext(t, cs(pe));
        }
        const char *keys[] = {"UTExportedTypeDeclarations", "UTImportedTypeDeclarations"};
        for (int kk = 0; kk < 2 && !t->next; kk++) {
            id decl = MSG(id, info, "objectForKey:", str(keys[kk]));
            u64 nd = decl ? MSG0(u64, decl, "count") : 0;
            for (u64 j = 0; j < nd; j++) {
                id dd = MSG(id, decl, "objectAtIndex:", j);
                id ident = MSG(id, dd, "objectForKey:", str("UTTypeIdentifier"));
                if (dc_type_named(t, 1, cs(ident)) != 0) continue;
                id spec = MSG(id, dd, "objectForKey:", str("UTTypeTagSpecification"));
                id fe = spec ? MSG(id, spec, "objectForKey:", str("public.filename-extension")) : 0;
                if (fe && cx_is_str(fe)) dc_type_add_ext(t, cs(fe));
                else { u64 nf = fe && responds(fe, "count") ? MSG0(u64, fe, "count") : 0; for (u64 q = 0; q < nf; q++) dc_type_add_ext(t, cs(MSG(id, fe, "objectAtIndex:", q))); }
            }
        }
    }
}
static const dc_type_t *type_named(id name) { load_types(); int i = dc_type_named(g_types, g_ntypes, cs(name)); return i >= 0 ? &g_types[i] : 0; }

/* ================================================================ estado do NSDocument */
typedef struct doc {
    struct doc *next;
    id obj, url, type, mdate, display, undo, wcs, print_info, autosave_timer, autosaved_url;
    dc_count_t count;
    BOOL has_undo, undo_observed, closing;
} doc_t;
static doc_t *g_docs;
static doc_t *ds(id o) {
    for (doc_t *d = g_docs; d; d = d->next) if (d->obj == o) return d;
    doc_t *d = calloc(1, sizeof *d);
    if (!d) return 0;
    d->obj = o;
    d->has_undo = YES;
    d->wcs = cx_arr_new(kls("NSMutableArray"));
    d->next = g_docs; g_docs = d;
    return d;
}
static void keep(id *slot, id v) { objc_retain(v); objc_release(*slot); *slot = v; }
static void ds_forget(id o) {
    for (doc_t **pp = &g_docs; *pp; pp = &(*pp)->next) if ((*pp)->obj == o) {
        doc_t *d = *pp; *pp = d->next;
        if (d->autosave_timer) MSG0(void, d->autosave_timer, "invalidate");
        id nc = MSG0(id, (id)kls("NSNotificationCenter"), "defaultCenter");
        if (nc && d->undo_observed) MSG(void, nc, "removeObserver:", o);
        id rel[9] = {d->url, d->type, d->mdate, d->display, d->undo, d->wcs, d->print_info, d->autosave_timer, d->autosaved_url};
        for (int i = 0; i < 9; i++) objc_release(rel[i]);
        free(d);
        return;
    }
}

/* ================================================================ NSWindowController ↔ documento ↔ janela */
static char k_wc_doc, k_wc_close_doc, k_win_wc, k_win_edited;
static IMP g_wc_window, g_wc_set_window, g_win_perform_close, g_app_terminate, g_win_set_title;
static id wc_document(id self, SEL c) { (void)c; return objc_getAssociatedObject(self, &k_wc_doc); }
static void wc_attach_window(id wc, id w) { if (w) objc_setAssociatedObject(w, &k_win_wc, wc, 0); }
static void sync_title(id wc);
static void wc_set_document(id self, SEL c, id doc) {
    (void)c;
    objc_setAssociatedObject(self, &k_wc_doc, doc, 0);   /* fraco: o documento retém o controlador */
    if (doc) sync_title(self);
}
static id wc_window_hook(id self, SEL c) {
    id w = g_wc_window ? ((id (*)(id, SEL))g_wc_window)(self, c) : 0;
    wc_attach_window(self, w);
    return w;
}
static void wc_set_window_hook(id self, SEL c, id w) {
    if (g_wc_set_window) ((void (*)(id, SEL, id))g_wc_set_window)(self, c, w);
    wc_attach_window(self, w);
    if (objc_getAssociatedObject(self, &k_wc_doc)) sync_title(self);
}
static BOOL wc_should_close_doc(id self, SEL c) { (void)c; id v = objc_getAssociatedObject(self, &k_wc_close_doc); return v && MSG0(BOOL, v, "boolValue"); }
static void wc_set_should_close_doc(id self, SEL c, BOOL b) { (void)c; objc_setAssociatedObject(self, &k_wc_close_doc, cx_num_i(b), 1); }
static id wc_title_for(id self, SEL c, id name) { (void)self; (void)c; return name; }
static void wc_sync_title_m(id self, SEL c) { (void)c; sync_title(self); }
static id win_wc(id self, SEL c) { (void)c; return objc_getAssociatedObject(self, &k_win_wc); }
static void win_set_wc(id self, SEL c, id wc) { (void)c; objc_setAssociatedObject(self, &k_win_wc, wc, 0); }
static BOOL win_edited(id self, SEL c) { (void)c; id v = objc_getAssociatedObject(self, &k_win_edited); return v && MSG0(BOOL, v, "boolValue"); }
static void win_set_edited(id self, SEL c, BOOL b) { (void)c; objc_setAssociatedObject(self, &k_win_edited, cx_num_i(b), 1); }

static id doc_display_name(id self, SEL c);
static BOOL doc_is_edited(id self, SEL c);
static void sync_title(id wc) {
    id doc = objc_getAssociatedObject(wc, &k_wc_doc);
    id w = g_wc_window ? ((id (*)(id, SEL))g_wc_window)(wc, sl("window")) : 0;
    if (!doc || !w) return;
    id name = MSG0(id, doc, "displayName");
    id t = MSG(id, wc, "windowTitleForDocumentDisplayName:", name ? name : str(""));
    BOOL ed = MSG0(BOOL, doc, "isDocumentEdited");
    char buf[600];
    dc_window_title(cs(t), ed, buf, sizeof buf);
    MSG(void, w, "setTitle:", str(buf));
    MSG(void, w, "setDocumentEdited:", ed);
    doc_t *d = ds(doc);
    if (d->url && responds(w, "setRepresentedURL:")) MSG(void, w, "setRepresentedURL:", d->url);
}
static void sync_all_titles(id doc) {
    doc_t *d = ds(doc);
    u64 n = MSG0(u64, d->wcs, "count");
    for (u64 i = 0; i < n; i++) sync_title(MSG(id, d->wcs, "objectAtIndex:", i));
}

/* ================================================================ NSDocumentController (estado) */
static id g_dc, g_dc_docs;
static dc_mru_t g_mru;
static BOOL g_mru_loaded;
static double g_autosave_delay = 5;
static void mru_load(void) {
    if (g_mru_loaded) return;
    g_mru_loaded = YES;
    dc_mru_init(&g_mru, 10);
    id ud = MSG0(id, (id)kls("NSUserDefaults"), "standardUserDefaults");
    id a = ud ? MSG(id, ud, "objectForKey:", str("NSRecentDocumentPaths")) : 0;
    id mx = ud ? MSG(id, ud, "objectForKey:", str("NSRecentDocumentsLimit")) : 0;
    if (mx && responds(mx, "integerValue")) dc_mru_set_max(&g_mru, (int)MSG0(i64, mx, "integerValue"));
    u64 n = a && responds(a, "count") ? MSG0(u64, a, "count") : 0;
    for (u64 i = n; i-- > 0;) dc_mru_add(&g_mru, cs(MSG(id, a, "objectAtIndex:", i)));
}
static void mru_save(void) {
    id ud = MSG0(id, (id)kls("NSUserDefaults"), "standardUserDefaults");
    if (!ud) return;
    id a = cx_arr_new(kls("NSMutableArray"));
    for (int i = 0; i < g_mru.n; i++) cx_arr_add(a, str(g_mru.item[i]));
    MSG(void, ud, "setObject:forKey:", a, str("NSRecentDocumentPaths"));
    objc_release(a);
}
static id dc_shared(id k, SEL c);
static void dc_add(id self, SEL c, id doc);
static void dc_remove(id self, SEL c, id doc);

/* ================================================================ NSDocument */
static id doc_init(id self, SEL c) {
    (void)c;
    IMP sup = base_imp("NSObject", "init");
    self = sup ? ((id (*)(id, SEL))sup)(self, sl("init")) : self;
    if (self) ds(self);
    return self;
}
static id doc_init_type(id self, SEL c, id type, id *err) {
    (void)c; (void)err;
    self = MSG0(id, self, "init");
    if (self) MSG(void, self, "setFileType:", type);
    return self;
}
static BOOL doc_read_url(id self, SEL c, id url, id type, id *err);
static id doc_init_url(id self, SEL c, id url, id type, id *err) {
    (void)c;
    self = MSG0(id, self, "init");
    if (!self) return 0;
    MSG(void, self, "setFileURL:", url);
    MSG(void, self, "setFileType:", type);
    id e = 0;
    if (!MSG(BOOL, self, "readFromURL:ofType:error:", url, type, &e)) { if (err) *err = e ? e : make_error(259, "Não foi possível abrir o documento."); objc_release(self); return 0; }
    id fm = MSG0(id, (id)kls("NSFileManager"), "defaultManager");
    id attrs = MSG(id, fm, "attributesOfItemAtPath:error:", url_path(url), (id)0);
    id md = attrs ? MSG(id, attrs, "objectForKey:", str("NSFileModificationDate")) : 0;
    if (md) MSG(void, self, "setFileModificationDate:", md);
    return self;
}
static id doc_init_for_url(id self, SEL c, id url, id contents, id type, id *err) {
    id me = doc_init_url(self, c, contents ? contents : url, type, err);
    if (me && url) MSG(void, me, "setFileURL:", url);
    if (me && contents && url) MSG(void, me, "updateChangeCount:", (i64)DC_READ_OTHER);
    return me;
}
static void doc_dealloc(id self, SEL c) {
    ds_forget(self);
    IMP sup = base_imp("NSObject", "dealloc");
    if (sup) ((void (*)(id, SEL))sup)(self, c);
}

static id doc_file_url(id s, SEL c) { (void)c; return ds(s)->url; }
static void doc_set_file_url(id s, SEL c, id u) { (void)c; keep(&ds(s)->url, u); sync_all_titles(s); }
static id doc_file_type(id s, SEL c) { (void)c; return ds(s)->type; }
static void doc_set_file_type(id s, SEL c, id t) { (void)c; keep(&ds(s)->type, t ? MSG0(id, t, "copy") : 0); objc_release(ds(s)->type); }
static id doc_mdate(id s, SEL c) { (void)c; return ds(s)->mdate; }
static void doc_set_mdate(id s, SEL c, id d) { (void)c; keep(&ds(s)->mdate, d); }
static id doc_display_name(id self, SEL c) {
    (void)c;
    doc_t *d = ds(self);
    if (d->display) return d->display;
    if (d->url) {
        char b[512];
        dc_display_name(cs(url_path(d->url)), 0, b, sizeof b);
        return str(b);
    }
    /* sem título: o 1º "Sem título N" livre entre os documentos abertos */
    const char *used[256];
    int nu = 0;
    id docs = g_dc_docs;
    u64 n = docs ? MSG0(u64, docs, "count") : 0;
    for (u64 i = 0; i < n && nu < 256; i++) {
        id o = MSG(id, docs, "objectAtIndex:", i);
        if (o == self) break;   /* só os que vieram antes: nomes estáveis */
        doc_t *od = ds(o);
        if (od->display) used[nu++] = cs(od->display);
        else if (!od->url) { id dn = MSG0(id, o, "displayName"); used[nu++] = cs(dn); }
    }
    char b[128];
    id base = responds(self, "defaultDraftName") ? MSG0(id, self, "defaultDraftName") : 0;
    dc_untitled_name(base ? cs(base) : "Sem título", used, nu, b, sizeof b);
    keep(&d->display, str(b));   /* fica fixo até ganhar URL */
    return d->display;
}
static void doc_set_display_name(id s, SEL c, id n) { (void)c; keep(&ds(s)->display, n); sync_all_titles(s); }
static id doc_draft_name(id s, SEL c) { (void)s; (void)c; return str("Sem título"); }

/* leitura */
static BOOL doc_read_data(id self, SEL c, id data, id type, id *err) {
    (void)self; (void)c; (void)data; (void)type;
    if (err) *err = make_error(259, "O app não implementou a leitura deste tipo de documento.");
    return NO;
}
static BOOL doc_read_wrapper(id self, SEL c, id wrapper, id type, id *err) {
    (void)c;
    id data = wrapper && responds(wrapper, "regularFileContents") ? MSG0(id, wrapper, "regularFileContents") : 0;
    if (!data) { if (err) *err = make_error(259, "Formato de pacote não suportado."); return NO; }
    return MSG(BOOL, self, "readFromData:ofType:error:", data, type, err);
}
static BOOL doc_read_url(id self, SEL c, id url, id type, id *err) {
    (void)c;
    id path = url_path(url);
    if (overrides(self, "NSDocument", "readFromFileWrapper:ofType:error:") && kls("NSFileWrapper")) {
        id w = ((id (*)(id, SEL, id, u64, id *))objc_msgSend)(MSG0(id, (id)kls("NSFileWrapper"), "alloc"), sl("initWithURL:options:error:"), url, (u64)0, err);
        if (!w) return NO;
        BOOL ok = MSG(BOOL, self, "readFromFileWrapper:ofType:error:", w, type, err);
        objc_release(w);
        return ok;
    }
    id data = MSG(id, (id)kls("NSData"), "dataWithContentsOfFile:", path);
    if (!data) { if (err) *err = make_error(260, "O arquivo não pôde ser lido."); return NO; }
    return MSG(BOOL, self, "readFromData:ofType:error:", data, type, err);
}
/* gravação */
static id doc_data_of_type(id self, SEL c, id type, id *err) {
    (void)self; (void)c; (void)type;
    if (err) *err = make_error(513, "O app não implementou a gravação deste tipo de documento.");
    return 0;
}
static id doc_wrapper_of_type(id self, SEL c, id type, id *err) {
    (void)c;
    id data = MSG(id, self, "dataOfType:error:", type, err);
    if (!data || !kls("NSFileWrapper")) return 0;
    return objc_autorelease(MSG(id, MSG0(id, (id)kls("NSFileWrapper"), "alloc"), "initRegularFileWithContents:", data));
}
static BOOL doc_write_url(id self, SEL c, id url, id type, id *err) {
    (void)c;
    id path = url_path(url);
    if (overrides(self, "NSDocument", "fileWrapperOfType:error:") && kls("NSFileWrapper")) {
        id w = MSG(id, self, "fileWrapperOfType:error:", type, err);
        if (!w) return NO;
        return ((BOOL (*)(id, SEL, id, u64, id, id *))objc_msgSend)(w, sl("writeToURL:options:originalContentsURL:error:"), url, (u64)1, (id)0, err);
    }
    id data = MSG(id, self, "dataOfType:error:", type, err);
    if (!data) return NO;
    if (!MSG(BOOL, data, "writeToFile:atomically:", path, (BOOL)YES)) { if (err) *err = make_error(513, "Não foi possível gravar o arquivo."); return NO; }
    return YES;
}
static BOOL doc_write_url_op(id self, SEL c, id url, id type, i64 op, id orig, id *err) {
    (void)c; (void)op; (void)orig;
    return MSG(BOOL, self, "writeToURL:ofType:error:", url, type, err);
}
static BOOL doc_write_safely(id self, SEL c, id url, id type, i64 op, id *err) {
    (void)c;
    return ((BOOL (*)(id, SEL, id, id, i64, id, id *))objc_msgSend)(self, sl("writeToURL:ofType:forSaveOperation:originalContentsURL:error:"), url, type, op, ds(self)->url, err);
}

/* mudanças */
static void doc_autosave_fire(id self, SEL c, id timer);
static void schedule_autosave(id self) {
    doc_t *d = ds(self);
    cx_class_t *k = object_getClass(self);
    BOOL in_place = cx_responds(k->isa, sl("autosavesInPlace")) && MSG0(BOOL, (id)k, "autosavesInPlace");
    if (!in_place || !d->url || g_autosave_delay <= 0) return;
    if (d->autosave_timer) { MSG0(void, d->autosave_timer, "invalidate"); objc_release(d->autosave_timer); d->autosave_timer = 0; }
    id t = MSG(id, (id)kls("NSTimer"), "scheduledTimerWithTimeInterval:target:selector:userInfo:repeats:", g_autosave_delay, self, sl("_cxAutosaveFire:"), (id)0, (BOOL)NO);
    d->autosave_timer = objc_retain(t);
}
static void doc_update_count(id self, SEL c, i64 type) {
    (void)c;
    doc_t *d = ds(self);
    BOOL was = dc_edited(&d->count);
    dc_update(&d->count, (int)type);
    if (was != dc_edited(&d->count)) sync_all_titles(self);
    if ((type & ~DC_DISCARDABLE) != DC_CLEARED && (type & ~DC_DISCARDABLE) != DC_AUTOSAVED && dc_has_unautosaved(&d->count)) schedule_autosave(self);
}
static BOOL doc_is_edited(id self, SEL c) { (void)c; return (BOOL)dc_edited(&ds(self)->count); }
static BOOL doc_unautosaved(id self, SEL c) { (void)c; return (BOOL)dc_has_unautosaved(&ds(self)->count); }
static id doc_count_token(id self, SEL c, i64 op) { (void)op; (void)c; return cx_num_i(ds(self)->count.change); }
static void doc_update_with_token(id self, SEL c, id token, i64 op) {
    (void)c;
    doc_t *d = ds(self);
    long at = token ? (long)MSG0(i64, token, "longLongValue") : d->count.change;
    BOOL was = dc_edited(&d->count);
    if (op == 3 || op == 4) d->count.saved_at_autosave = at;
    else if (op != 2) { d->count.change -= at; d->count.saved_at_autosave -= at; d->count.read_other = 0; }
    if (was != dc_edited(&d->count)) sync_all_titles(self);
}
/* desfazer: as notificações do NSUndoManager viram updateChangeCount: */
static void doc_undo_note(id self, SEL c, id note) {
    (void)c;
    id n = note ? MSG0(id, note, "name") : 0;
    const char *s = cs(n);
    if (!strcmp(s, "NSUndoManagerDidUndoChangeNotification")) doc_update_count(self, 0, DC_UNDONE);
    else if (!strcmp(s, "NSUndoManagerDidRedoChangeNotification")) doc_update_count(self, 0, DC_REDONE);
    else if (!strcmp(s, "NSUndoManagerDidCloseUndoGroupNotification") || !strcmp(s, "NSUndoManagerCheckpointNotification")) {
        id um = note ? MSG0(id, note, "object") : 0;
        if (!um || (responds(um, "isUndoing") && MSG0(BOOL, um, "isUndoing")) || (responds(um, "isRedoing") && MSG0(BOOL, um, "isRedoing"))) return;
        if (strcmp(s, "NSUndoManagerDidCloseUndoGroupNotification")) return;
        /* o NSUndoManager posta também ao fechar grupos aninhados: só o de fora (nível 0 depois de fechar) é mudança */
        if (responds(um, "groupingLevel") && MSG0(i64, um, "groupingLevel") > 0) return;
        id info = MSG0(id, note, "userInfo");
        id disc = info ? MSG(id, info, "objectForKey:", cx_str("NSUndoManagerGroupIsDiscardableKey")) : 0;
        doc_update_count(self, 0, DC_DONE | (disc && MSG0(BOOL, disc, "boolValue") ? DC_DISCARDABLE : 0));
    }
}
static void observe_undo(id self, id um) {
    doc_t *d = ds(self);
    id nc = MSG0(id, (id)kls("NSNotificationCenter"), "defaultCenter");
    if (!nc || !um) return;
    const char *names[] = {"NSUndoManagerDidUndoChangeNotification", "NSUndoManagerDidRedoChangeNotification", "NSUndoManagerDidCloseUndoGroupNotification"};
    for (int i = 0; i < 3; i++) MSG(void, nc, "addObserver:selector:name:object:", self, sl("_cxUndoNote:"), str(names[i]), um);
    d->undo_observed = YES;
}
static id doc_undo(id self, SEL c) {
    (void)c;
    doc_t *d = ds(self);
    if (!d->undo && d->has_undo) {
        id um = MSG0(id, MSG0(id, (id)kls("NSUndoManager"), "alloc"), "init");
        d->undo = um;
        observe_undo(self, um);
    }
    return d->undo;
}
static void doc_set_undo(id self, SEL c, id um) {
    (void)c;
    doc_t *d = ds(self);
    id nc = MSG0(id, (id)kls("NSNotificationCenter"), "defaultCenter");
    if (nc && d->undo_observed && d->undo) MSG(void, nc, "removeObserver:name:object:", self, (id)0, d->undo);
    keep(&d->undo, um);
    if (um) observe_undo(self, um);
}
static BOOL doc_has_undo(id s, SEL c) { (void)c; return ds(s)->has_undo; }
static void doc_set_has_undo(id s, SEL c, BOOL b) { (void)c; ds(s)->has_undo = b; if (!b) doc_set_undo(s, 0, 0); }

/* erros */
static BOOL doc_present_error(id self, SEL c, id err) {
    (void)c;
    if (responds(self, "willPresentError:")) err = MSG(id, self, "willPresentError:", err);
    id al = MSG0(id, MSG0(id, (id)kls("NSAlert"), "alloc"), "init");
    id desc = err && responds(err, "localizedDescription") ? MSG0(id, err, "localizedDescription") : str("Ocorreu um erro.");
    MSG(void, al, "setMessageText:", desc ? desc : str("Ocorreu um erro."));
    id rec = err && responds(err, "localizedRecoverySuggestion") ? MSG0(id, err, "localizedRecoverySuggestion") : 0;
    if (rec) MSG(void, al, "setInformativeText:", rec);
    MSG(id, al, "addButtonWithTitle:", str("OK"));
    MSG0(i64, al, "runModal");
    objc_release(al);
    return NO;
}
static void doc_present_error_sheet(id self, SEL c, id err, id win, id dl, SEL sel, void *ctx) {
    (void)c; (void)win;
    BOOL rec = doc_present_error(self, 0, err);
    if (dl && sel) ((void (*)(id, SEL, BOOL, void *))objc_msgSend)(dl, sel, rec, ctx);
}
static id doc_will_present(id s, SEL c, id e) { (void)s; (void)c; return e; }

/* salvar */
static id ext_for_type(id type) {
    const dc_type_t *t = type_named(type);
    if (t && t->next && strcmp(t->ext[0], "*")) return str(t->ext[0]);
    return 0;
}
static id doc_ext_for_type(id self, SEL c, id type, i64 op) { (void)self; (void)c; (void)op; return ext_for_type(type); }
static id doc_writable_types(id self, SEL c, i64 op) {
    (void)c; (void)op;
    id a = MSG0(id, (id)kls("NSMutableArray"), "array");
    load_types();
    const char *cls = cx_class_name(object_getClass(self));
    for (int i = 0; i < g_ntypes; i++) if (dc_type_writable(&g_types[i]) && (!g_types[i].cls[0] || !strcmp(g_types[i].cls, cls))) MSG(void, a, "addObject:", str(g_types[i].name));
    if (!MSG0(u64, a, "count") && ds(self)->type) MSG(void, a, "addObject:", ds(self)->type);
    return a;
}
static BOOL save_to(id self, id url, id type, i64 op) {
    doc_t *d = ds(self);
    id token = MSG(id, self, "changeCountTokenForSaveOperation:", op);
    id err = 0;
    BOOL ok = ((BOOL (*)(id, SEL, id, id, i64, id *))objc_msgSend)(self, sl("writeSafelyToURL:ofType:forSaveOperation:error:"), url, type, op, &err);
    if (!ok) { MSG(BOOL, self, "presentError:", err ? err : make_error(513, "Não foi possível salvar o documento.")); return NO; }
    if (op != 2 && op != 3) {   /* Save, SaveAs, AutosaveInPlace mudam o arquivo do documento */
        MSG(void, self, "setFileURL:", url);
        MSG(void, self, "setFileType:", type);
        keep(&d->display, 0);
        id fm = MSG0(id, (id)kls("NSFileManager"), "defaultManager");
        id attrs = MSG(id, fm, "attributesOfItemAtPath:error:", url_path(url), (id)0);
        id md = attrs ? MSG(id, attrs, "objectForKey:", str("NSFileModificationDate")) : 0;
        if (md) MSG(void, self, "setFileModificationDate:", md);
        if (op != 4) { mru_load(); dc_mru_add(&g_mru, cs(url_path(url))); mru_save(); }
    }
    MSG(void, self, "updateChangeCountWithToken:forSaveOperation:", token, op);
    sync_all_titles(self);
    return YES;
}
static id g_type_popup;
static BOOL run_save_panel(id self, i64 op, id *out_url, id *out_type) {
    doc_t *d = ds(self);
    id sp = MSG0(id, (id)kls("NSSavePanel"), "savePanel");
    if (!sp) return NO;
    id type = d->type;
    id wt = MSG(id, self, "writableTypesForSaveOperation:", op);
    if ((!type || !type_named(type)) && wt && MSG0(u64, wt, "count")) type = MSG(id, wt, "objectAtIndex:", (u64)0);
    id ext = MSG(id, self, "fileNameExtensionForType:saveOperation:", type, op);
    char nm[512];
    dc_display_name(cs(MSG0(id, self, "displayName")), 1, nm, sizeof nm);
    MSG(void, sp, "setNameFieldStringValue:", str(nm));
    if (ext) { id one = MSG(id, (id)kls("NSArray"), "arrayWithObject:", ext); MSG(void, sp, "setAllowedFileTypes:", one); }
    if (d->url) { id par = MSG0(id, url_path(d->url), "stringByDeletingLastPathComponent"); if (par) MSG(void, sp, "setDirectoryURL:", file_url(par)); }
    if (op == 2) MSG(void, sp, "setTitle:", str("Exportar"));
    u64 nwt = wt ? MSG0(u64, wt, "count") : 0;
    g_type_popup = 0;
    if (nwt > 1 && MSG0(BOOL, self, "shouldRunSavePanelWithAccessoryView")) {
        typedef struct { double x, y, w, h; } rect_t;
        id pb = ((id (*)(id, SEL, rect_t, BOOL))objc_msgSend)(MSG0(id, (id)kls("NSPopUpButton"), "alloc"), sl("initWithFrame:pullsDown:"), (rect_t){0, 0, 260, 26}, NO);
        for (u64 i = 0; i < nwt; i++) {
            id tn = MSG(id, wt, "objectAtIndex:", i);
            id shown = g_dc ? MSG(id, g_dc, "displayNameForType:", tn) : tn;
            MSG(void, pb, "addItemWithTitle:", shown ? shown : tn);
            id it = MSG(id, pb, "lastItem");
            MSG(void, it, "setRepresentedObject:", tn);
            if (type && MSG(BOOL, tn, "isEqualToString:", type)) MSG(void, pb, "selectItemAtIndex:", (i64)i);
        }
        MSG(void, sp, "setAccessoryView:", pb);
        g_type_popup = pb;
    }
    if (responds(self, "prepareSavePanel:") && !MSG(BOOL, self, "prepareSavePanel:", sp)) { objc_release(g_type_popup); g_type_popup = 0; return NO; }
    i64 r = MSG0(i64, sp, "runModal");
    if (g_type_popup) {
        id it = MSG0(id, g_type_popup, "selectedItem");
        id tn = it ? MSG0(id, it, "representedObject") : 0;
        if (tn) type = tn;
        objc_release(g_type_popup);
        g_type_popup = 0;
    }
    if (r != 1) return NO;
    id u = MSG0(id, sp, "URL");
    if (!u) return NO;
    *out_url = u;
    *out_type = type ? type : str("public.data");
    return YES;
}
static void doc_save_to_url_handler(id self, SEL c, id url, id type, i64 op, void *blk) {
    (void)c;
    BOOL ok = save_to(self, url, type, op);
    blk_call1(blk, ok ? 0 : make_error(513, "Não foi possível salvar o documento."));
}
static void doc_save_to_url_delegate(id self, SEL c, id url, id type, i64 op, id dl, SEL sel, void *ctx) {
    (void)c;
    BOOL ok = save_to(self, url, type, op);
    if (dl && sel) ((void (*)(id, SEL, id, BOOL, void *))objc_msgSend)(dl, sel, self, ok, ctx);
}
static BOOL save_with_op(id self, i64 op) {
    doc_t *d = ds(self);
    if (op == 0 && d->url) return save_to(self, d->url, d->type ? d->type : str("public.data"), 0);
    id url = 0, type = 0;
    if (!run_save_panel(self, op == 0 ? 1 : op, &url, &type)) return NO;
    return save_to(self, url, type, op == 0 ? 1 : op);
}
static void doc_save(id self, SEL c, id sender) { (void)c; (void)sender; save_with_op(self, 0); }
static void doc_save_as(id self, SEL c, id sender) { (void)c; (void)sender; save_with_op(self, 1); }
static void doc_save_to(id self, SEL c, id sender) { (void)c; (void)sender; save_with_op(self, 2); }
static void doc_save_with_delegate(id self, SEL c, id dl, SEL sel, void *ctx) {
    (void)c;
    BOOL ok = save_with_op(self, 0);
    if (dl && sel) ((void (*)(id, SEL, id, BOOL, void *))objc_msgSend)(dl, sel, self, ok, ctx);
}
static void doc_run_save_panel(id self, SEL c, i64 op, id dl, SEL sel, void *ctx) {
    (void)c;
    id url = 0, type = 0;
    BOOL ok = run_save_panel(self, op, &url, &type) && save_to(self, url, type, op);
    if (dl && sel) ((void (*)(id, SEL, id, BOOL, void *))objc_msgSend)(dl, sel, self, ok, ctx);
}
static BOOL doc_prepare(id s, SEL c, id p) { (void)s; (void)c; (void)p; return YES; }
static BOOL doc_should_accessory(id s, SEL c) { (void)s; (void)c; return YES; }
static BOOL doc_revert_to(id self, SEL c, id url, id type, id *err) {
    (void)c;
    BOOL ok = MSG(BOOL, self, "readFromURL:ofType:error:", url, type, err);
    if (ok) { MSG(void, self, "updateChangeCount:", (i64)DC_CLEARED); doc_t *d = ds(self); if (d->undo) MSG0(void, d->undo, "removeAllActions"); }
    return ok;
}
static void doc_revert(id self, SEL c, id sender) {
    (void)c; (void)sender;
    doc_t *d = ds(self);
    if (!d->url) return;
    id al = MSG0(id, MSG0(id, (id)kls("NSAlert"), "alloc"), "init");
    char q[700];
    snprintf(q, sizeof q, "Reverter para a versão salva de \xE2\x80\x9C%s\xE2\x80\x9D?", cs(MSG0(id, self, "displayName")));
    MSG(void, al, "setMessageText:", str(q));
    MSG(void, al, "setInformativeText:", str("As alterações desde o último salvamento serão perdidas."));
    MSG(id, al, "addButtonWithTitle:", str("Reverter"));
    MSG(id, al, "addButtonWithTitle:", str("Cancelar"));
    i64 r = MSG0(i64, al, "runModal");
    objc_release(al);
    if (r != 1000) return;
    id err = 0;
    if (!MSG(BOOL, self, "revertToContentsOfURL:ofType:error:", d->url, d->type, &err)) MSG(BOOL, self, "presentError:", err);
}

/* autosave no lugar */
static void doc_autosave_fire(id self, SEL c, id timer) {
    (void)c; (void)timer;
    doc_t *d = ds(self);
    if (d->autosave_timer) { objc_release(d->autosave_timer); d->autosave_timer = 0; }
    if (!d->url || !dc_has_unautosaved(&d->count)) return;
    save_to(self, d->url, d->type ? d->type : str("public.data"), 4);
}
static void doc_autosave_handler(id self, SEL c, BOOL implicit, void *blk) {
    (void)c; (void)implicit;
    doc_t *d = ds(self);
    BOOL ok = YES;
    if (d->url && dc_has_unautosaved(&d->count)) ok = save_to(self, d->url, d->type ? d->type : str("public.data"), 4);
    blk_call1(blk, ok ? 0 : make_error(513, "Não foi possível salvar automaticamente."));
}
static void doc_autosave_delegate(id self, SEL c, id dl, SEL sel, void *ctx) {
    (void)c;
    doc_autosave_handler(self, 0, YES, 0);
    if (dl && sel) ((void (*)(id, SEL, id, BOOL, void *))objc_msgSend)(dl, sel, self, YES, ctx);
}
static BOOL cls_no(id k, SEL c) { (void)k; (void)c; return NO; }
static BOOL cls_yes(id k, SEL c) { (void)k; (void)c; return YES; }
static BOOL cls_yes_type(id k, SEL c, id t) { (void)k; (void)c; (void)t; return YES; }

/* janelas */
static id doc_wcs(id s, SEL c) { (void)c; return ds(s)->wcs; }
static void doc_add_wc(id self, SEL c, id wc) {
    (void)c;
    doc_t *d = ds(self);
    if (!wc || MSG(BOOL, d->wcs, "containsObject:", wc)) return;
    cx_arr_add(d->wcs, wc);
    MSG(void, wc, "setDocument:", self);
}
static void doc_remove_wc(id self, SEL c, id wc) {
    (void)c;
    doc_t *d = ds(self);
    if (!wc) return;
    MSG(void, wc, "setDocument:", (id)0);
    MSG(void, d->wcs, "removeObject:", wc);
}
static id doc_nib_name(id s, SEL c) { (void)s; (void)c; return 0; }
static void doc_make_wcs(id self, SEL c) {
    (void)c;
    id nib = MSG0(id, self, "windowNibName");
    if (!nib) return;
    id wc = MSG(id, MSG0(id, (id)kls("NSWindowController"), "alloc"), "initWithWindowNibName:owner:", nib, self);
    MSG(void, self, "addWindowController:", wc);
    objc_release(wc);
    if (responds(self, "windowControllerWillLoadNib:")) MSG(void, self, "windowControllerWillLoadNib:", wc);
    MSG0(id, wc, "window");
    if (responds(self, "windowControllerDidLoadNib:")) MSG(void, self, "windowControllerDidLoadNib:", wc);
    sync_title(wc);
}
static void doc_show(id self, SEL c) {
    (void)c;
    doc_t *d = ds(self);
    u64 n = MSG0(u64, d->wcs, "count");
    for (u64 i = 0; i < n; i++) { id wc = MSG(id, d->wcs, "objectAtIndex:", i); MSG(void, wc, "showWindow:", self); sync_title(wc); }
}
static id doc_window_for_sheet(id self, SEL c) {
    (void)c;
    doc_t *d = ds(self);
    if (MSG0(u64, d->wcs, "count")) { id w = MSG0(id, MSG(id, d->wcs, "objectAtIndex:", (u64)0), "window"); if (w) return w; }
    return NSApp ? MSG0(id, NSApp, "mainWindow") : 0;
}
static void doc_set_window(id self, SEL c, id w) {   /* outlet "window" do nib do documento */
    (void)c;
    doc_t *d = ds(self);
    if (!MSG0(u64, d->wcs, "count")) return;
    id first = MSG(id, d->wcs, "objectAtIndex:", (u64)0);
    MSG(void, first, "setWindow:", w);
}
static void doc_close(id self, SEL c) {
    (void)c;
    doc_t *d = ds(self);
    if (d->closing) return;
    d->closing = YES;
    objc_retain(self);
    id wcs = MSG0(id, d->wcs, "copy");
    u64 n = MSG0(u64, wcs, "count");
    for (u64 i = 0; i < n; i++) {
        id wc = MSG(id, wcs, "objectAtIndex:", i);
        id w = g_wc_window ? ((id (*)(id, SEL))g_wc_window)(wc, sl("window")) : 0;
        if (w) { MSG(void, w, "orderOut:", (id)0); MSG0(void, w, "close"); }
        doc_remove_wc(self, 0, wc);
    }
    objc_release(wcs);
    if (g_dc) dc_remove(g_dc, 0, self);
    d->closing = NO;
    objc_release(self);
}
/* pergunta antes de fechar: 1 = pode fechar, 0 = cancelado */
static int ask_save_on_close(id self) {
    if (!MSG0(BOOL, self, "isDocumentEdited")) return 1;
    id al = MSG0(id, MSG0(id, (id)kls("NSAlert"), "alloc"), "init");
    char q[700];
    snprintf(q, sizeof q, "Deseja salvar as altera\xC3\xA7\xC3\xB5" "es feitas no documento \xE2\x80\x9C%s\xE2\x80\x9D?", cs(MSG0(id, self, "displayName")));
    MSG(void, al, "setMessageText:", str(q));
    MSG(void, al, "setInformativeText:", str("Suas altera\xC3\xA7\xC3\xB5" "es ser\xC3\xA3o perdidas se voc\xC3\xAA n\xC3\xA3o as salvar."));
    MSG(id, al, "addButtonWithTitle:", str("Salvar"));
    MSG(id, al, "addButtonWithTitle:", str("Cancelar"));
    MSG(id, al, "addButtonWithTitle:", str("N\xC3\xA3o Salvar"));
    i64 r = MSG0(i64, al, "runModal");
    objc_release(al);
    if (r == 1000) return save_with_op(self, 0) ? 1 : 0;
    if (r == 1002) return 1;
    return 0;
}
static void doc_can_close(id self, SEL c, id dl, SEL sel, void *ctx) {
    (void)c;
    BOOL ok = ask_save_on_close(self) != 0;
    if (dl && sel) ((void (*)(id, SEL, id, BOOL, void *))objc_msgSend)(dl, sel, self, ok, ctx);
}
static void doc_should_close_wc(id self, SEL c, id wc, id dl, SEL sel, void *ctx) {
    (void)c;
    doc_t *d = ds(self);
    BOOL last = MSG0(u64, d->wcs, "count") <= 1 || MSG0(BOOL, wc, "shouldCloseDocument");
    BOOL ok = !last || ask_save_on_close(self);
    if (dl && sel) ((void (*)(id, SEL, id, BOOL, void *))objc_msgSend)(dl, sel, self, ok, ctx);
}

/* NSWindow -performClose: com documento: pergunta, fecha a janela e o documento quando é a última */
static void win_perform_close(id self, SEL c, id sender) {
    id wc = objc_getAssociatedObject(self, &k_win_wc);
    id doc = wc ? objc_getAssociatedObject(wc, &k_wc_doc) : 0;
    if (!doc) { if (g_win_perform_close) ((void (*)(id, SEL, id))g_win_perform_close)(self, c, sender); return; }
    id dl = responds(self, "delegate") ? MSG0(id, self, "delegate") : 0;
    if (dl && responds(dl, "windowShouldClose:") && !MSG(BOOL, dl, "windowShouldClose:", self)) return;
    doc_t *d = ds(doc);
    BOOL last = MSG0(u64, d->wcs, "count") <= 1 || MSG0(BOOL, wc, "shouldCloseDocument");
    if (last) {
        if (!ask_save_on_close(doc)) return;
        MSG0(void, doc, "close");
        return;
    }
    doc_remove_wc(doc, 0, wc);
    MSG0(void, self, "close");
}

/* validação dos itens de menu do documento */
static BOOL doc_validate(id self, SEL c, id item) {
    (void)c;
    SEL a = item && responds(item, "action") ? MSG0(SEL, item, "action") : 0;
    doc_t *d = ds(self);
    if (a == sl("revertDocumentToSaved:")) return d->url && MSG0(BOOL, self, "isDocumentEdited");
    if (a == sl("saveDocument:")) return YES;
    if (a == sl("undo:") || a == sl("redo:")) return YES;
    return YES;
}
static id doc_print_info(id self, SEL c) {
    (void)c;
    doc_t *d = ds(self);
    if (!d->print_info) d->print_info = objc_retain(MSG0(id, (id)kls("NSPrintInfo"), "sharedPrintInfo"));
    return d->print_info;
}
static void doc_set_print_info(id s, SEL c, id p) { (void)c; keep(&ds(s)->print_info, p); }
static void doc_print(id self, SEL c, id sender) {
    (void)c; (void)sender;
    if (responds(self, "printDocumentWithSettings:showPrintPanel:delegate:didPrintSelector:contextInfo:")
        && overrides(self, "NSDocument", "printOperationWithSettings:error:")) {
        id err = 0;
        id op = MSG(id, self, "printOperationWithSettings:error:", MSG0(id, (id)kls("NSDictionary"), "dictionary"), &err);
        if (op) MSG(BOOL, op, "runOperation");
        else if (err) MSG(BOOL, self, "presentError:", err);
    }
}
static id doc_print_op(id self, SEL c, id settings, id *err) {
    (void)self; (void)c; (void)settings;
    if (err) *err = make_error(3072, "Este documento não define como imprimir.");
    return 0;
}
static void doc_print_with(id self, SEL c, id settings, BOOL show, id dl, SEL sel, void *ctx) {
    (void)c; (void)show;
    id err = 0;
    id op = MSG(id, self, "printOperationWithSettings:error:", settings, &err);
    BOOL ok = op ? MSG(BOOL, op, "runOperation") : NO;
    if (!op && err) MSG(BOOL, self, "presentError:", err);
    if (dl && sel) ((void (*)(id, SEL, id, BOOL, void *))objc_msgSend)(dl, sel, self, ok, ctx);
}
static void doc_page_layout(id self, SEL c, id sender) {
    (void)c; (void)sender;
    cx_class_t *pl = kls("NSPageLayout");
    if (!pl) return;
    id l = MSG0(id, (id)pl, "pageLayout");
    if (l && responds(l, "runModalWithPrintInfo:")) MSG(i64, l, "runModalWithPrintInfo:", doc_print_info(self, 0));
}
static BOOL doc_is_draft(id s, SEL c) { (void)c; return ds(s)->url == 0; }
static BOOL doc_no(id s, SEL c) { (void)s; (void)c; return NO; }
static void doc_noop1(id s, SEL c, id a) { (void)s; (void)c; (void)a; }
static id doc_nil(id s, SEL c) { (void)s; (void)c; return 0; }
static void doc_duplicate(id self, SEL c, id sender) {
    (void)c; (void)sender;
    id type = ds(self)->type;
    id err = 0;
    id data = MSG(id, self, "dataOfType:error:", type, &err);
    if (!data) { if (err) MSG(BOOL, self, "presentError:", err); return; }
    id cls = (id)object_getClass(self);
    id nd = MSG(id, MSG0(id, cls, "alloc"), "initWithType:error:", type, &err);
    if (!nd) return;
    if (!MSG(BOOL, nd, "readFromData:ofType:error:", data, type, &err)) { objc_release(nd); return; }
    MSG(void, nd, "updateChangeCount:", (i64)DC_DONE);
    if (g_dc) MSG(void, g_dc, "addDocument:", nd);
    MSG0(void, nd, "makeWindowControllers");
    MSG0(void, nd, "showWindows");
    objc_release(nd);
}
static id cls_types(id k, SEL c, BOOL writable) {
    (void)c;
    load_types();
    id a = MSG0(id, (id)kls("NSMutableArray"), "array");
    const char *cn = cx_class_name((cx_class_t *)k);
    for (int i = 0; i < g_ntypes; i++) {
        if (g_types[i].cls[0] && strcmp(g_types[i].cls, cn)) continue;
        if (writable && !dc_type_writable(&g_types[i])) continue;
        MSG(void, a, "addObject:", str(g_types[i].name));
    }
    return a;
}
static id cls_readable(id k, SEL c) { return cls_types(k, c, NO); }
static id cls_writable(id k, SEL c) { return cls_types(k, c, YES); }
static BOOL cls_native(id k, SEL c, id type) {
    (void)c;
    const dc_type_t *t = type_named(type);
    return t && (!t->cls[0] || !strcmp(t->cls, cx_class_name((cx_class_t *)k))) && dc_type_writable(t);
}

/* ================================================================ NSDocumentController */
static id dc_init(id self, SEL c) {
    (void)c;
    IMP sup = base_imp("NSObject", "init");
    self = sup ? ((id (*)(id, SEL))sup)(self, sl("init")) : self;
    if (self && !g_dc) g_dc = objc_retain(self);
    if (!g_dc_docs) g_dc_docs = cx_arr_new(kls("NSMutableArray"));
    return self;
}
static id dc_shared(id k, SEL c) {
    (void)c;
    if (!g_dc) { id o = MSG0(id, MSG0(id, k, "alloc"), "init"); if (o != g_dc) objc_release(o); }
    return g_dc;
}
static id dc_documents(id s, SEL c) { (void)s; (void)c; if (!g_dc_docs) g_dc_docs = cx_arr_new(kls("NSMutableArray")); return g_dc_docs; }
static void dc_add(id self, SEL c, id doc) { (void)c; id a = dc_documents(self, 0); if (doc && !MSG(BOOL, a, "containsObject:", doc)) cx_arr_add(a, doc); }
static void dc_remove(id self, SEL c, id doc) { (void)c; if (doc) MSG(void, dc_documents(self, 0), "removeObject:", doc); }
static id dc_doc_for_window(id s, SEL c, id w) {
    (void)s; (void)c;
    id wc = w ? objc_getAssociatedObject(w, &k_win_wc) : 0;
    return wc ? objc_getAssociatedObject(wc, &k_wc_doc) : 0;
}
static id dc_current(id s, SEL c) {
    (void)c;
    id w = NSApp ? MSG0(id, NSApp, "mainWindow") : 0;
    if (!w && NSApp) w = MSG0(id, NSApp, "keyWindow");
    return w ? dc_doc_for_window(s, 0, w) : 0;
}
static id dc_doc_for_url(id s, SEL c, id url) {
    (void)c;
    const char *p = cs(url_path(url));
    id a = dc_documents(s, 0);
    u64 n = MSG0(u64, a, "count");
    for (u64 i = 0; i < n; i++) { id d = MSG(id, a, "objectAtIndex:", i); id du = ds(d)->url; if (du && !strcmp(cs(url_path(du)), p)) return d; }
    return 0;
}
static id dcm_default_type(id s, SEL c) { (void)s; (void)c; load_types(); int i = dc_default_type(g_types, g_ntypes); return i >= 0 ? str(g_types[i].name) : 0; }
static id dc_class_names(id s, SEL c) {
    (void)s; (void)c;
    load_types();
    id a = MSG0(id, (id)kls("NSMutableArray"), "array");
    for (int i = 0; i < g_ntypes; i++) if (g_types[i].cls[0]) { id n = str(g_types[i].cls); if (!MSG(BOOL, a, "containsObject:", n)) MSG(void, a, "addObject:", n); }
    return a;
}
static id dc_class_for_type(id s, SEL c, id type) {
    (void)s; (void)c;
    const dc_type_t *t = type_named(type);
    cx_class_t *k = t && t->cls[0] ? kls(t->cls) : 0;
    if (!k) { load_types(); int i = dc_default_type(g_types, g_ntypes); if (i >= 0 && g_types[i].cls[0]) k = kls(g_types[i].cls); }
    return (id)k;
}
static id dc_display_for_type(id s, SEL c, id type) { (void)s; (void)c; return type; }
static id dc_exts_for_type(id s, SEL c, id type) {
    (void)s; (void)c;
    id a = MSG0(id, (id)kls("NSMutableArray"), "array");
    const dc_type_t *t = type_named(type);
    for (int i = 0; t && i < t->next; i++) MSG(void, a, "addObject:", str(t->ext[i]));
    return a;
}
static id dc_type_from_ext(id s, SEL c, id ext) { (void)s; (void)c; load_types(); int i = dc_type_for_ext(g_types, g_ntypes, cs(ext)); return i >= 0 ? str(g_types[i].name) : 0; }
static id dc_type_for_url(id self, SEL c, id url, id *err) {
    (void)c;
    id ext = MSG0(id, url_path(url), "pathExtension");
    id t = dc_type_from_ext(self, 0, ext);
    if (!t) {
        load_types();
        if (g_ntypes == 1) t = str(g_types[0].name);   /* app de um tipo só abre o que for */
    }
    if (!t && err) *err = make_error(256, "O documento não pôde ser aberto: o app não abre arquivos deste tipo.");
    return t;
}
static id dc_make_untitled(id self, SEL c, id type, id *err) {
    (void)c;
    id cls = MSG(id, self, "documentClassForType:", type);
    if (!cls) { if (err) *err = make_error(256, "O app não definiu a classe do documento."); return 0; }
    return objc_autorelease(MSG(id, MSG0(id, cls, "alloc"), "initWithType:error:", type, err));
}
static id dc_make_with_url(id self, SEL c, id url, id type, id *err) {
    (void)c;
    id cls = MSG(id, self, "documentClassForType:", type);
    if (!cls) { if (err) *err = make_error(256, "O app não definiu a classe do documento."); return 0; }
    return objc_autorelease(MSG(id, MSG0(id, cls, "alloc"), "initWithContentsOfURL:ofType:error:", url, type, err));
}
static id dc_make_for_url(id self, SEL c, id url, id contents, id type, id *err) {
    (void)c;
    id cls = MSG(id, self, "documentClassForType:", type);
    if (!cls) return 0;
    return objc_autorelease(((id (*)(id, SEL, id, id, id, id *))objc_msgSend)(MSG0(id, cls, "alloc"), sl("initForURL:withContentsOfURL:ofType:error:"), url, contents, type, err));
}
static void present_dc_error(id err) {
    id al = MSG0(id, MSG0(id, (id)kls("NSAlert"), "alloc"), "init");
    id desc = err && responds(err, "localizedDescription") ? MSG0(id, err, "localizedDescription") : str("O documento não pôde ser aberto.");
    MSG(void, al, "setMessageText:", desc);
    MSG(id, al, "addButtonWithTitle:", str("OK"));
    MSG0(i64, al, "runModal");
    objc_release(al);
}
static id dc_open_untitled(id self, SEL c, BOOL display, id *err) {
    (void)c;
    id type = MSG0(id, self, "defaultType");
    id doc = MSG(id, self, "makeUntitledDocumentOfType:error:", type, err);
    if (!doc) return 0;
    MSG(void, self, "addDocument:", doc);
    if (display) { MSG0(void, doc, "makeWindowControllers"); MSG0(void, doc, "showWindows"); }
    return doc;
}
static void dc_new(id self, SEL c, id sender) {
    (void)c; (void)sender;
    id err = 0;
    if (!MSG(id, self, "openUntitledDocumentAndDisplay:error:", (BOOL)YES, &err) && err) present_dc_error(err);
}
static id open_url(id self, id url, BOOL display, BOOL *already, id *err) {
    *already = NO;
    id doc = MSG(id, self, "documentForURL:", url);
    if (doc) { *already = YES; if (display) MSG0(void, doc, "showWindows"); return doc; }
    id type = MSG(id, self, "typeForContentsOfURL:error:", url, err);
    if (!type) return 0;
    doc = MSG(id, self, "makeDocumentWithContentsOfURL:ofType:error:", url, type, err);
    if (!doc) return 0;
    MSG(void, self, "addDocument:", doc);
    MSG(void, self, "noteNewRecentDocumentURL:", url);
    if (display) { MSG0(void, doc, "makeWindowControllers"); MSG0(void, doc, "showWindows"); }
    return doc;
}
static void dc_open_url_handler(id self, SEL c, id url, BOOL display, void *blk) {
    (void)c;
    BOOL already = NO;
    id err = 0;
    id doc = open_url(self, url, display, &already, &err);
    if (blk) blk_call3(blk, doc, already, doc ? 0 : err);
    else if (!doc) present_dc_error(err);
}
static id dc_open_url_legacy(id self, SEL c, id url, BOOL display, id *err) { (void)c; BOOL a; return open_url(self, url, display, &a, err); }
static void dc_reopen(id self, SEL c, id url, id contents, BOOL display, void *blk) {
    (void)c;
    id err = 0;
    id doc = MSG(id, self, "documentForURL:", url);
    BOOL already = doc != 0;
    if (!doc) {
        id type = MSG(id, self, "typeForContentsOfURL:error:", contents ? contents : url, &err);
        doc = type ? MSG(id, self, "makeDocumentForURL:withContentsOfURL:ofType:error:", url, contents, type, &err) : 0;
        if (doc) MSG(void, self, "addDocument:", doc);
        if (doc && display) { MSG0(void, doc, "makeWindowControllers"); MSG0(void, doc, "showWindows"); }
    }
    blk_call3(blk, doc, already, doc ? 0 : err);
}
static i64 dc_run_open_panel(id self, SEL c, id panel, id types) {
    (void)self; (void)c;
    if (types && MSG0(u64, types, "count")) MSG(void, panel, "setAllowedFileTypes:", types);
    return MSG0(i64, panel, "runModal");
}
static id dc_urls_from_panel(id self, SEL c) {
    (void)c;
    id p = MSG0(id, (id)kls("NSOpenPanel"), "openPanel");
    MSG(void, p, "setAllowsMultipleSelection:", (BOOL)YES);
    load_types();
    id exts = MSG0(id, (id)kls("NSMutableArray"), "array");
    BOOL any = NO;
    for (int i = 0; i < g_ntypes; i++) {
        for (int k = 0; k < g_types[i].next; k++) { if (!strcmp(g_types[i].ext[k], "*")) any = YES; else MSG(void, exts, "addObject:", str(g_types[i].ext[k])); }
        for (int k = 0; k < g_types[i].nuti; k++) MSG(void, exts, "addObject:", str(g_types[i].uti[k]));
    }
    i64 r = MSG(i64, self, "runModalOpenPanel:forTypes:", p, any ? (id)0 : exts);
    return r == 1 ? MSG0(id, p, "URLs") : 0;
}
static void dc_open(id self, SEL c, id sender) {
    (void)c; (void)sender;
    id urls = MSG0(id, self, "URLsFromRunningOpenPanel");
    u64 n = urls ? MSG0(u64, urls, "count") : 0;
    for (u64 i = 0; i < n; i++) dc_open_url_handler(self, 0, MSG(id, urls, "objectAtIndex:", i), YES, 0);
}
static void dc_begin_open(id self, SEL c, void *blk) {
    (void)c;
    id urls = MSG0(id, self, "URLsFromRunningOpenPanel");
    blk_call1(blk, urls);
}
static BOOL dc_has_edited(id s, SEL c) {
    (void)c;
    id a = dc_documents(s, 0);
    u64 n = MSG0(u64, a, "count");
    for (u64 i = 0; i < n; i++) if (MSG0(BOOL, MSG(id, a, "objectAtIndex:", i), "isDocumentEdited")) return YES;
    return NO;
}
static void dc_save_all(id s, SEL c, id sender) {
    (void)c; (void)sender;
    id a = MSG0(id, dc_documents(s, 0), "copy");
    u64 n = MSG0(u64, a, "count");
    for (u64 i = 0; i < n; i++) { id d = MSG(id, a, "objectAtIndex:", i); if (MSG0(BOOL, d, "isDocumentEdited")) MSG(void, d, "saveDocument:", (id)0); }
    objc_release(a);
}
/* fecha todos, perguntando dos editados; 0 se o usuário cancelou */
static int close_all(id s) {
    id a = MSG0(id, dc_documents(s, 0), "copy");
    u64 n = MSG0(u64, a, "count");
    int ok = 1;
    for (u64 i = 0; i < n && ok; i++) {
        id d = MSG(id, a, "objectAtIndex:", i);
        if (!ask_save_on_close(d)) { ok = 0; break; }
        MSG0(void, d, "close");
    }
    objc_release(a);
    return ok;
}
static void dc_close_all(id s, SEL c, id dl, SEL sel, void *ctx) {
    (void)c;
    BOOL ok = close_all(s) != 0;
    if (dl && sel) ((void (*)(id, SEL, id, BOOL, void *))objc_msgSend)(dl, sel, s, ok, ctx);
}
static void dc_review(id s, SEL c, id title, BOOL cancellable, id dl, SEL sel, void *ctx) {
    (void)c; (void)title; (void)cancellable;
    dc_close_all(s, 0, dl, sel, ctx);
}
static void dc_note_recent_url(id s, SEL c, id url) { (void)s; (void)c; if (!url) return; mru_load(); dc_mru_add(&g_mru, cs(url_path(url))); mru_save(); }
static void dc_note_recent_doc(id s, SEL c, id doc) { (void)c; id u = doc ? ds(doc)->url : 0; if (u) dc_note_recent_url(s, 0, u); }
static id dc_recent(id s, SEL c) {
    (void)s; (void)c;
    mru_load();
    id a = MSG0(id, (id)kls("NSMutableArray"), "array");
    for (int i = 0; i < g_mru.n; i++) MSG(void, a, "addObject:", file_url(str(g_mru.item[i])));
    return a;
}
static void dc_clear_recent(id s, SEL c, id sender) { (void)s; (void)c; (void)sender; mru_load(); dc_mru_clear(&g_mru); mru_save(); }
static u64 dc_max_recent(id s, SEL c) { (void)s; (void)c; mru_load(); return (u64)g_mru.max; }
static double dc_autosave_delay(id s, SEL c) { (void)s; (void)c; return g_autosave_delay; }
static void dc_set_autosave_delay(id s, SEL c, double d) { (void)s; (void)c; g_autosave_delay = d; }
static BOOL dc_validate(id s, SEL c, id item) {
    (void)c;
    SEL a = item && responds(item, "action") ? MSG0(SEL, item, "action") : 0;
    if (a == sl("saveAllDocuments:")) return dc_has_edited(s, 0);
    if (a == sl("clearRecentDocuments:")) { mru_load(); return g_mru.n > 0; }
    return YES;
}
static BOOL dc_present_error(id s, SEL c, id err) { (void)s; (void)c; present_dc_error(err); return NO; }

/* NSApp -terminate: pergunta dos documentos editados antes de sair */
static void app_terminate(id self, SEL c, id sender) {
    if (g_dc && dc_has_edited(g_dc, 0) && !close_all(g_dc)) return;
    if (g_app_terminate) ((void (*)(id, SEL, id))g_app_terminate)(self, c, sender);
}
/* lançamento: app de documentos sem documento aberto abre um sem título (applicationShouldOpenUntitledFile:) */
static void did_finish_launching(id self, SEL c, id note) {
    (void)self; (void)c; (void)note;
    load_types();
    if (dc_default_type(g_types, g_ntypes) < 0) return;
    int i = dc_default_type(g_types, g_ntypes);
    if (!g_types[i].cls[0] || !kls(g_types[i].cls)) return;
    id dc = dc_shared((id)kls("NSDocumentController"), 0);
    if (MSG0(u64, dc_documents(dc, 0), "count")) return;
    id d = NSApp ? MSG0(id, NSApp, "delegate") : 0;
    if (d && responds(d, "applicationShouldOpenUntitledFile:") && !MSG(BOOL, d, "applicationShouldOpenUntitledFile:", NSApp)) return;
    if (d && responds(d, "applicationOpenUntitledFile:")) return;   /* o NSApplication chama logo depois desta notificação */
    dc_new(dc, 0, 0);
}

#define ADD(cls, sel, fn) cx_add_method(cls, sel, (void *)(fn))
#define CADD(cls, sel, fn) cx_add_cmethod(cls, sel, (void *)(fn))
void cx_appkit_document_init(void) {
    cx_class_t *doc = kls("NSDocument"), *dcc = kls("NSDocumentController"), *wc = kls("NSWindowController"), *win = kls("NSWindow"), *app = kls("NSApplication");
    if (!doc || !dcc) return;
    ADD(doc, "init", doc_init); ADD(doc, "initWithType:error:", doc_init_type);
    ADD(doc, "initWithContentsOfURL:ofType:error:", doc_init_url); ADD(doc, "initForURL:withContentsOfURL:ofType:error:", doc_init_for_url);
    ADD(doc, "dealloc", doc_dealloc);
    ADD(doc, "fileURL", doc_file_url); ADD(doc, "setFileURL:", doc_set_file_url); ADD(doc, "fileType", doc_file_type); ADD(doc, "setFileType:", doc_set_file_type);
    ADD(doc, "fileModificationDate", doc_mdate); ADD(doc, "setFileModificationDate:", doc_set_mdate);
    ADD(doc, "displayName", doc_display_name); ADD(doc, "setDisplayName:", doc_set_display_name); ADD(doc, "defaultDraftName", doc_draft_name);
    ADD(doc, "readFromData:ofType:error:", doc_read_data); ADD(doc, "readFromFileWrapper:ofType:error:", doc_read_wrapper);
    ADD(doc, "readFromURL:ofType:error:", doc_read_url); ADD(doc, "dataOfType:error:", doc_data_of_type);
    ADD(doc, "fileWrapperOfType:error:", doc_wrapper_of_type); ADD(doc, "writeToURL:ofType:error:", doc_write_url);
    ADD(doc, "writeToURL:ofType:forSaveOperation:originalContentsURL:error:", doc_write_url_op);
    ADD(doc, "writeSafelyToURL:ofType:forSaveOperation:error:", doc_write_safely);
    ADD(doc, "updateChangeCount:", doc_update_count); ADD(doc, "isDocumentEdited", doc_is_edited); ADD(doc, "hasUnautosavedChanges", doc_unautosaved);
    ADD(doc, "changeCountTokenForSaveOperation:", doc_count_token); ADD(doc, "updateChangeCountWithToken:forSaveOperation:", doc_update_with_token);
    ADD(doc, "_cxUndoNote:", doc_undo_note); ADD(doc, "_cxAutosaveFire:", doc_autosave_fire);
    ADD(doc, "undoManager", doc_undo); ADD(doc, "setUndoManager:", doc_set_undo); ADD(doc, "hasUndoManager", doc_has_undo); ADD(doc, "setHasUndoManager:", doc_set_has_undo);
    ADD(doc, "presentError:", doc_present_error); ADD(doc, "presentError:modalForWindow:delegate:didPresentSelector:contextInfo:", doc_present_error_sheet);
    ADD(doc, "willPresentError:", doc_will_present);
    ADD(doc, "fileNameExtensionForType:saveOperation:", doc_ext_for_type); ADD(doc, "writableTypesForSaveOperation:", doc_writable_types);
    ADD(doc, "saveDocument:", doc_save); ADD(doc, "saveDocumentAs:", doc_save_as); ADD(doc, "saveDocumentTo:", doc_save_to);
    ADD(doc, "saveDocumentWithDelegate:didSaveSelector:contextInfo:", doc_save_with_delegate);
    ADD(doc, "saveToURL:ofType:forSaveOperation:completionHandler:", doc_save_to_url_handler);
    ADD(doc, "saveToURL:ofType:forSaveOperation:delegate:didSaveSelector:contextInfo:", doc_save_to_url_delegate);
    ADD(doc, "runModalSavePanelForSaveOperation:delegate:didSaveSelector:contextInfo:", doc_run_save_panel);
    ADD(doc, "prepareSavePanel:", doc_prepare); ADD(doc, "shouldRunSavePanelWithAccessoryView", doc_should_accessory);
    ADD(doc, "revertToContentsOfURL:ofType:error:", doc_revert_to); ADD(doc, "revertDocumentToSaved:", doc_revert);
    ADD(doc, "autosaveWithImplicitCancellability:completionHandler:", doc_autosave_handler);
    ADD(doc, "autosaveDocumentWithDelegate:didAutosaveSelector:contextInfo:", doc_autosave_delegate);
    ADD(doc, "windowControllers", doc_wcs); ADD(doc, "addWindowController:", doc_add_wc); ADD(doc, "removeWindowController:", doc_remove_wc);
    ADD(doc, "windowNibName", doc_nib_name); ADD(doc, "makeWindowControllers", doc_make_wcs); ADD(doc, "showWindows", doc_show);
    ADD(doc, "windowForSheet", doc_window_for_sheet); ADD(doc, "setWindow:", doc_set_window);
    ADD(doc, "close", doc_close); ADD(doc, "canCloseDocumentWithDelegate:shouldCloseSelector:contextInfo:", doc_can_close);
    ADD(doc, "shouldCloseWindowController:delegate:shouldCloseSelector:contextInfo:", doc_should_close_wc);
    ADD(doc, "validateUserInterfaceItem:", doc_validate); ADD(doc, "validateMenuItem:", doc_validate);
    ADD(doc, "printInfo", doc_print_info); ADD(doc, "setPrintInfo:", doc_set_print_info);
    ADD(doc, "printDocument:", doc_print); ADD(doc, "printOperationWithSettings:error:", doc_print_op);
    ADD(doc, "printDocumentWithSettings:showPrintPanel:delegate:didPrintSelector:contextInfo:", doc_print_with);
    ADD(doc, "runPageLayout:", doc_page_layout);
    ADD(doc, "isDraft", doc_is_draft); ADD(doc, "isInViewingMode", doc_no); ADD(doc, "isLocked", doc_no); ADD(doc, "keepBackupFile", doc_no);
    ADD(doc, "duplicateDocument:", doc_duplicate); ADD(doc, "browseDocumentVersions:", doc_noop1); ADD(doc, "lockDocument:", doc_noop1);
    ADD(doc, "unlockDocument:", doc_noop1); ADD(doc, "moveDocumentToUbiquityContainer:", doc_noop1); ADD(doc, "autosavedContentsFileURL", doc_nil);
    ADD(doc, "backupFileURL", doc_nil); ADD(doc, "userActivity", doc_nil);
    CADD(doc, "autosavesInPlace", cls_no); CADD(doc, "autosavesDrafts", cls_no); CADD(doc, "preservesVersions", cls_no);
    CADD(doc, "usesUbiquitousStorage", cls_no); CADD(doc, "canConcurrentlyReadDocumentsOfType:", cls_yes_type);
    CADD(doc, "readableTypes", cls_readable); CADD(doc, "writableTypes", cls_writable); CADD(doc, "isNativeType:", cls_native);
    CADD(doc, "shouldRunSavePanelWithAccessoryView", cls_yes);

    ADD(dcc, "init", dc_init); CADD(dcc, "sharedDocumentController", dc_shared);
    ADD(dcc, "documents", dc_documents); ADD(dcc, "addDocument:", dc_add); ADD(dcc, "removeDocument:", dc_remove);
    ADD(dcc, "currentDocument", dc_current); ADD(dcc, "documentForWindow:", dc_doc_for_window); ADD(dcc, "documentForURL:", dc_doc_for_url);
    ADD(dcc, "defaultType", dcm_default_type); ADD(dcc, "documentClassNames", dc_class_names); ADD(dcc, "documentClassForType:", dc_class_for_type);
    ADD(dcc, "displayNameForType:", dc_display_for_type); ADD(dcc, "fileExtensionsFromType:", dc_exts_for_type);
    ADD(dcc, "typeFromFileExtension:", dc_type_from_ext); ADD(dcc, "typeForContentsOfURL:error:", dc_type_for_url);
    ADD(dcc, "makeUntitledDocumentOfType:error:", dc_make_untitled); ADD(dcc, "makeDocumentWithContentsOfURL:ofType:error:", dc_make_with_url);
    ADD(dcc, "makeDocumentForURL:withContentsOfURL:ofType:error:", dc_make_for_url);
    ADD(dcc, "openUntitledDocumentAndDisplay:error:", dc_open_untitled); ADD(dcc, "newDocument:", dc_new);
    ADD(dcc, "openDocumentWithContentsOfURL:display:completionHandler:", dc_open_url_handler);
    ADD(dcc, "openDocumentWithContentsOfURL:display:error:", dc_open_url_legacy);
    ADD(dcc, "reopenDocumentForURL:withContentsOfURL:display:completionHandler:", dc_reopen);
    ADD(dcc, "runModalOpenPanel:forTypes:", dc_run_open_panel); ADD(dcc, "URLsFromRunningOpenPanel", dc_urls_from_panel);
    ADD(dcc, "openDocument:", dc_open); ADD(dcc, "beginOpenPanelWithCompletionHandler:", dc_begin_open);
    ADD(dcc, "hasEditedDocuments", dc_has_edited); ADD(dcc, "saveAllDocuments:", dc_save_all);
    ADD(dcc, "closeAllDocumentsWithDelegate:didCloseAllSelector:contextInfo:", dc_close_all);
    ADD(dcc, "reviewUnsavedDocumentsWithAlertTitle:cancellable:delegate:didReviewAllSelector:contextInfo:", dc_review);
    ADD(dcc, "noteNewRecentDocumentURL:", dc_note_recent_url); ADD(dcc, "noteNewRecentDocument:", dc_note_recent_doc);
    ADD(dcc, "recentDocumentURLs", dc_recent); ADD(dcc, "clearRecentDocuments:", dc_clear_recent); ADD(dcc, "maximumRecentDocumentCount", dc_max_recent);
    ADD(dcc, "autosavingDelay", dc_autosave_delay); ADD(dcc, "setAutosavingDelay:", dc_set_autosave_delay);
    ADD(dcc, "validateUserInterfaceItem:", dc_validate); ADD(dcc, "validateMenuItem:", dc_validate); ADD(dcc, "presentError:", dc_present_error);

    if (wc) {
        g_wc_window = cx_lookup(wc, sl("window"));
        g_wc_set_window = cx_lookup(wc, sl("setWindow:"));
        ADD(wc, "window", wc_window_hook); ADD(wc, "setWindow:", wc_set_window_hook);
        ADD(wc, "document", wc_document); ADD(wc, "setDocument:", wc_set_document);
        ADD(wc, "shouldCloseDocument", wc_should_close_doc); ADD(wc, "setShouldCloseDocument:", wc_set_should_close_doc);
        ADD(wc, "windowTitleForDocumentDisplayName:", wc_title_for); ADD(wc, "synchronizeWindowTitleWithDocumentName", wc_sync_title_m);
    }
    if (win) {
        g_win_perform_close = cx_lookup(win, sl("performClose:"));
        ADD(win, "performClose:", win_perform_close);
        ADD(win, "windowController", win_wc); ADD(win, "setWindowController:", win_set_wc);
        ADD(win, "isDocumentEdited", win_edited); ADD(win, "setDocumentEdited:", win_set_edited);
    }
    if (app) {
        g_app_terminate = cx_lookup(app, sl("terminate:"));
        ADD(app, "terminate:", app_terminate);
    }
    cx_class_t *obj = kls("NSObject");
    static cx_class_t obs_c, obs_m;
    if (obj) {
        cx_define_class(&obs_c, &obs_m, "_MOSDocumentLaunchObserver", obj, 0);
        cx_add_method(&obs_c, "cxDidFinishLaunching:", (void *)did_finish_launching);
        id o = cx_alloc_instance(&obs_c);
        id nc = MSG0(id, (id)kls("NSNotificationCenter"), "defaultCenter");
        if (nc) MSG(void, nc, "addObserver:selector:name:object:", o, sl("cxDidFinishLaunching:"), str("NSApplicationDidFinishLaunchingNotification"), (id)0);
    }
    (void)g_win_set_title;
}
