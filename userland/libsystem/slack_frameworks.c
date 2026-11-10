/*
 * mOS libSystem — o que o Slack (machocheck, nov. 2026) ainda importava e o mOS não tinha, fora a libc++ (que vem da
 * /usr/lib/libc++.1.dylib do LLVM), a libcompression e a libz (frameworks Mach-O de userland/frameworks):
 *
 *  - libSystem: setiopolicy_np, atexit_b, __c_locale, dispatch_data_create (dispatch_data = NSData, como no ObjC);
 *  - CFPreferences por domínio (CopyValue/SetValue/Synchronize), gravadas em ~/Library/Preferences/<app>.plist;
 *  - NSInputStream (dados, arquivo ou URL de arquivo), CFFileSecurity e NSURLFileSecurityKey;
 *  - UTTypeConformsTo e kUTType* de CoreServices; SMAppService (itens de login, guardados em memória);
 *  - CoreSpotlight (índice local em memória com busca por texto), AVCaptureAudioDataOutput e as chaves do AVFAudio;
 *  - NSPreviewRepresentingActivityItem, NSHapticFeedbackManager (o mOS não tem trackpad com atuador: nada vibra);
 *  - AXIsProcessTrusted, CGEvent de teclado (CGEventPost entrega a tecla à janela-chave deste app) e
 *    CGSessionCopyCurrentDictionary; CMTimeConvertScale.
 */
#include "foundation.h"
#include "sys.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
const char *cx_cstr(id s);
id cx_str(const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
void *malloc(size_t n);
void *calloc(size_t n, size_t m);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
size_t strlen(const char *s);
int atexit(void (*f)(void));
extern u8 *cx_read_file(const char *path, u64 *len);
id NSHomeDirectory(void);
void *_Block_copy(const void *b);
void _Block_release(const void *b);
typedef void (*dispatch_function_t)(void *);
void dispatch_async_f(void *q, void *ctx, dispatch_function_t fn);
void *dispatch_get_main_queue(void);
id CFDataCreate(void *alloc, const u8 *bytes, long n);
void cx_post_key_event(u16 mac, u64 flags, int down);
const char *sel_getName(SEL s);
int snprintf(char *b, size_t n, const char *f, ...);

static id cls(const char *n) { return (id)cx_class_named(n); }
static BOOL resp(id o, const char *s) { return o && cx_responds(object_getClass(o), sel_registerName(s)); }
static id ret(id o) { return o ? objc_retain(o) : 0; }
static void setobj(id *slot, id v) { if (*slot == v) return; if (v) objc_retain(v); if (*slot) objc_release(*slot); *slot = v; }
static id mdict(void) { return MSG0(id, cls("NSMutableDictionary"), "dictionary"); }
typedef struct { void *isa; int flags, reserved; void *invoke; } blk_t;
static void call_blk1(void *b, id a) { if (b) ((void (*)(void *, id))((blk_t *)b)->invoke)(b, a); }

/* ================================================================ libSystem */
int setiopolicy_np(int iotype, int scope, int policy) { (void)iotype; (void)scope; (void)policy; return 0; }   /* sem prioridade de E/S */
int getiopolicy_np(int iotype, int scope) { (void)iotype; (void)scope; return 0; }   /* IOPOL_DEFAULT (claude-native do Claude desktop) */
static void *g_exit_blocks[64];
static int g_nexit;
static void run_exit_blocks(void) { while (g_nexit) { void *b = g_exit_blocks[--g_nexit]; ((void (*)(void *))((blk_t *)b)->invoke)(b); } }
int atexit_b(void *block) {
    if (g_nexit >= 64 || !block) return -1;
    if (!g_nexit) atexit(run_exit_blocks);
    g_exit_blocks[g_nexit++] = _Block_copy(block);
    return 0;
}
/* locale "C" do xlocale (o &__c_locale da libc++); as funções *_l do libSystem tratam qualquer locale como "C" */
u8 _c_locale[512];                     /* no Mach-O: __c_locale */
/* dispatch_data: no ObjC é um NSData. Destrutor padrão (NULL) copia; DISPATCH_DATA_DESTRUCTOR_FREE libera o buffer;
 * um bloco é chamado na fila assim que o conteúdo foi copiado */
const void *_dispatch_data_destructor_free = (const void *)1;      /* no Mach-O: __dispatch_data_destructor_free */
const void *_dispatch_data_destructor_munmap = (const void *)2;
static void run_destructor(void *b) { ((void (*)(void *))((blk_t *)b)->invoke)(b); _Block_release(b); }
id dispatch_data_create(const void *buf, size_t n, void *queue, void *destructor) {
    id d = CFDataCreate(0, (const u8 *)buf, (long)n);
    if (destructor == (void *)&_dispatch_data_destructor_free || destructor == (void *)1) free((void *)buf);
    else if (destructor && destructor != (void *)&_dispatch_data_destructor_munmap && destructor != (void *)2)
        dispatch_async_f(queue ? queue : dispatch_get_main_queue(), _Block_copy(destructor), run_destructor);
    return d;
}
size_t dispatch_data_get_size(id d) { return d ? (size_t)MSG0(u64, d, "length") : 0; }

/* CFPreferences por domínio: prefs.c */

/* ================================================================ NSInputStream */
cx_class_t NSInputStream_c __asm__("OBJC_CLASS_$_NSInputStream"), NSInputStream_m __asm__("OBJC_METACLASS_$_NSInputStream");
typedef struct { void *isa; u8 *buf; u64 len, pos; int status; id delegate, error; } nsin_t;
#define IS(o) ((nsin_t *)(o))
static id is_init_data(id s, SEL c, id data) {
    (void)c;
    u64 n = data ? MSG0(u64, data, "length") : 0;
    IS(s)->buf = malloc(n ? n : 1);
    if (n) memcpy(IS(s)->buf, MSG0(const void *, data, "bytes"), n);
    IS(s)->len = n;
    return s;
}
static id is_init_file(id s, SEL c, id path) {
    (void)c;
    u64 n = 0;
    u8 *b = path ? cx_read_file(cx_cstr(path), &n) : 0;
    if (!b) { objc_release(s); return 0; }
    IS(s)->buf = b; IS(s)->len = n;
    return s;
}
static id is_init_url(id s, SEL c, id url) { return is_init_file(s, c, url && resp(url, "path") ? MSG0(id, url, "path") : 0); }
static id is_with_data(id k, SEL c, id d) { return objc_autorelease(is_init_data(cx_alloc_instance((cx_class_t *)k), c, d)); }
static id is_with_file(id k, SEL c, id p) { id s = is_init_file(cx_alloc_instance((cx_class_t *)k), c, p); return s ? objc_autorelease(s) : 0; }
static id is_with_url(id k, SEL c, id u) { id s = is_init_url(cx_alloc_instance((cx_class_t *)k), c, u); return s ? objc_autorelease(s) : 0; }
static void is_open(id s, SEL c) { (void)c; IS(s)->status = 2; }      /* NSStreamStatusOpen */
static void is_close(id s, SEL c) { (void)c; IS(s)->status = 6; }     /* NSStreamStatusClosed */
static i64 is_read(id s, SEL c, u8 *buf, u64 max) {
    (void)c;
    nsin_t *x = IS(s);
    if (x->status != 2 && x->status != 3) return -1;
    u64 n = x->len - x->pos < max ? x->len - x->pos : max;
    memcpy(buf, x->buf + x->pos, n);
    x->pos += n;
    if (x->pos >= x->len) x->status = 5;                               /* NSStreamStatusAtEnd */
    return (i64)n;
}
static BOOL is_has_bytes(id s, SEL c) { (void)c; return IS(s)->pos < IS(s)->len; }
static u64 is_status(id s, SEL c) { (void)c; return (u64)IS(s)->status; }
static id is_error(id s, SEL c) { (void)c; return IS(s)->error; }
static id is_delegate(id s, SEL c) { (void)c; return IS(s)->delegate; }
static void is_set_delegate(id s, SEL c, id d) { (void)c; IS(s)->delegate = d; }
static BOOL is_get_buffer(id s, SEL c, u8 **b, u64 *n) { (void)c; if (b) *b = IS(s)->buf + IS(s)->pos; if (n) *n = IS(s)->len - IS(s)->pos; return YES; }
static void is_schedule(id s, SEL c, id rl, id mode) { (void)s; (void)c; (void)rl; (void)mode; }
static id is_prop(id s, SEL c, id key) { (void)s; (void)c; (void)key; return 0; }
static BOOL is_set_prop(id s, SEL c, id v, id key) { (void)s; (void)c; (void)v; (void)key; return NO; }

/* ================================================================ CFFileSecurity e NSURLFileSecurityKey */
id NSURLFileSecurityKey;
typedef struct { u16 mode; u32 owner, group; u8 has_mode, has_owner, has_group; } cffsec_t;
void *CFFileSecurityCreate(void *alloc) { (void)alloc; return calloc(1, sizeof(cffsec_t)); }
u8 CFFileSecurityGetMode(void *s, u16 *mode) { cffsec_t *x = s; if (!x || !x->has_mode) return 0; if (mode) *mode = x->mode; return 1; }
u8 CFFileSecuritySetMode(void *s, u16 mode) { cffsec_t *x = s; if (!x) return 0; x->mode = mode; x->has_mode = 1; return 1; }
u8 CFFileSecuritySetOwner(void *s, u32 uid) { cffsec_t *x = s; if (!x) return 0; x->owner = uid; x->has_owner = 1; return 1; }
u8 CFFileSecuritySetGroup(void *s, u32 gid) { cffsec_t *x = s; if (!x) return 0; x->group = gid; x->has_group = 1; return 1; }

/* ================================================================ UTType (CoreServices, a API antiga em C) */
id kUTTypeApplicationBundle, kUTTypeData;
/* hierarquia mínima: os tipos que o macOS declara como "conforma a" */
static const char *uti_parent(const char *t) {
    static const char *map[][2] = {
        {"com.apple.application-bundle", "com.apple.application"}, {"com.apple.application", "com.apple.bundle"},
        {"com.apple.bundle", "com.apple.package"}, {"com.apple.package", "public.directory"}, {"public.directory", "public.item"},
        {"public.folder", "public.directory"}, {"public.plain-text", "public.text"}, {"public.utf8-plain-text", "public.plain-text"},
        {"public.text", "public.data"}, {"public.html", "public.text"}, {"public.json", "public.text"}, {"public.png", "public.image"},
        {"public.jpeg", "public.image"}, {"public.image", "public.data"}, {"com.adobe.pdf", "public.data"}, {"public.data", "public.item"},
        {"public.zip-archive", "public.data"}, {"public.movie", "public.audiovisual-content"}, {"public.audiovisual-content", "public.data"},
    };
    for (size_t i = 0; i < sizeof map / sizeof *map; i++) if (!strcmp(map[i][0], t)) return map[i][1];
    return 0;
}
int cx_ls_conforms(const char *a, const char *b);   /* openurl.c: árvore de tipos do Launch Services (kernel) */
u8 UTTypeConformsTo(id a, id b) {
    if (!a || !b) return 0;
    const char *t = cx_cstr(a), *want = cx_cstr(b);
    if (cx_ls_conforms(t, want)) return 1;
    for (int depth = 0; t && depth < 16; depth++, t = uti_parent(t)) if (!strcmp(t, want)) return 1;
    return 0;
}

/* ================================================================ SMAppService */
cx_class_t SMAppService_c __asm__("OBJC_CLASS_$_SMAppService"), SMAppService_m __asm__("OBJC_METACLASS_$_SMAppService");
typedef struct { void *isa; id ident; i64 status; int kind; char cat[16]; } smapp_t;     /* kind: 1 agente, 2 daemon */
static id g_services;
static id sm_named(id k, const char *kind, id ident) {
    if (!g_services) g_services = ret(mdict());
    char key[300];
    snprintf(key, sizeof key, "%s:%s", kind, ident ? cx_cstr(ident) : "");
    id ks = cx_str(key), s = MSG(id, g_services, "objectForKey:", ks);
    if (!s) {
        s = cx_alloc_instance((cx_class_t *)k);
        ((smapp_t *)s)->ident = ret(ident);
        snprintf(((smapp_t *)s)->cat, sizeof ((smapp_t *)s)->cat, "%s", kind);
        MSG(void, g_services, "setObject:forKey:", s, ks);
        objc_release(s);
    }
    return s;
}
static id sm_main(id k, SEL c) { (void)c; return sm_named(k, "main", 0); }
static id sm_login(id k, SEL c, id i) { (void)c; return sm_named(k, "login", i); }
static id sm_agent(id k, SEL c, id p) { (void)c; id s = sm_named(k, "agent", p); ((smapp_t *)s)->kind = 1; return s; }
static id sm_daemon(id k, SEL c, id p) { (void)c; id s = sm_named(k, "daemon", p); ((smapp_t *)s)->kind = 2; return s; }
int cx_sm_remember(const char *key, int status);
int cx_sm_recall(const char *key);
int cx_sm_publish_file(int daemon, const char *plist_path);
int cx_sm_unpublish_file(int daemon, const char *plist_path);
static void sm_key(smapp_t *m, char *out, size_t cap) {
    snprintf(out, cap, "%s:%s", m->cat[0] ? m->cat : "main", m->ident ? cx_cstr(m->ident) : "");
}
static i64 sm_status(id s, SEL c) {
    (void)c;
    char key[300];
    sm_key((smapp_t *)s, key, sizeof key);
    int disk = cx_sm_recall(key);
    if (disk >= 0) ((smapp_t *)s)->status = disk;
    return ((smapp_t *)s)->status;      /* 0 notRegistered, 1 enabled, 3 notFound */
}
/* agente/daemon: o plist em Contents/Library/LaunchAgents|LaunchDaemons do app vai ao launchd (launchctl load)
 * e uma cópia fica em ~/Library/LaunchAgents ou /Library/LaunchDaemons para o próximo boot. */
int cx_launchd_cmd(unsigned op, const char *arg, char *out, unsigned cap);
static BOOL sm_plist_path(smapp_t *m, char *path, size_t cap) {
    id b = MSG0(id, (id)cx_class_named("NSBundle"), "mainBundle");
    id bp = b ? MSG0(id, b, "bundlePath") : 0;
    if (!bp || !m->ident) return NO;
    snprintf(path, cap, "%s/Contents/Library/%s/%s", cx_cstr(bp), m->kind == 2 ? "LaunchDaemons" : "LaunchAgents", cx_cstr(m->ident));
    return YES;
}
static BOOL sm_register(id s, SEL c, id *err) {
    (void)c;
    smapp_t *m = (smapp_t *)s;
    if (err) *err = 0;
    char path[1024], out[256];
    char key[300];
    sm_key(m, key, sizeof key);
    if (m->kind && sm_plist_path(m, path, sizeof path)) {
        int r = cx_launchd_cmd(2 /* LC_LOAD */, path, out, sizeof out);
        if (r != 0 && !strstr(out, "carregado")) { m->status = 3; cx_sm_remember(key, 3); return NO; }
        cx_sm_publish_file(m->kind == 2, path);
    }
    m->status = 1;
    cx_sm_remember(key, 1);
    return YES;
}
static BOOL sm_unregister(id s, SEL c, id *err) {
    (void)c;
    smapp_t *m = (smapp_t *)s;
    if (err) *err = 0;
    char path[1024], key[300];
    sm_key(m, key, sizeof key);
    if (m->kind && sm_plist_path(m, path, sizeof path)) {
        cx_launchd_cmd(3 /* LC_UNLOAD */, path, 0, 0);
        cx_sm_unpublish_file(m->kind == 2, path);
    }
    m->status = 0;
    cx_sm_remember(key, 0);
    return YES;
}
static void sm_unregister_cb(id s, SEL c, void *done) { sm_unregister(s, c, 0); call_blk1(done, 0); }
static void sm_open_settings(id k, SEL c) { (void)k; (void)c; }

/* ================================================================ CoreSpotlight: índice local */
cx_class_t CSSearchableIndex_c __asm__("OBJC_CLASS_$_CSSearchableIndex"), CSSearchableIndex_m __asm__("OBJC_METACLASS_$_CSSearchableIndex");
cx_class_t CSSearchableItem_c __asm__("OBJC_CLASS_$_CSSearchableItem"), CSSearchableItem_m __asm__("OBJC_METACLASS_$_CSSearchableItem");
cx_class_t CSSearchableItemAttributeSet_c __asm__("OBJC_CLASS_$_CSSearchableItemAttributeSet"), CSSearchableItemAttributeSet_m __asm__("OBJC_METACLASS_$_CSSearchableItemAttributeSet");
typedef struct { void *isa; id items; id name; } csindex_t;
typedef struct { void *isa; id uid, domain, attrs; id expiration; } csitem_t;
typedef struct { void *isa; id type; id props; } csattrs_t;
static id g_default_index;
static id cs_default(id k, SEL c) {
    (void)c;
    if (!g_default_index) { g_default_index = cx_alloc_instance((cx_class_t *)k); ((csindex_t *)g_default_index)->items = ret(mdict()); }
    return g_default_index;
}
static BOOL cs_available(id k, SEL c) { (void)k; (void)c; return YES; }
static id cs_init_name(id s, SEL c, id name) { (void)c; ((csindex_t *)s)->items = ret(mdict()); ((csindex_t *)s)->name = ret(name); return s; }
static void cs_index(id s, SEL c, id items, void *done) {
    (void)c;
    u64 n = items ? MSG0(u64, items, "count") : 0;
    for (u64 i = 0; i < n; i++) {
        id it = MSG(id, items, "objectAtIndex:", i);
        if (((csitem_t *)it)->uid) MSG(void, ((csindex_t *)s)->items, "setObject:forKey:", it, ((csitem_t *)it)->uid);
    }
    call_blk1(done, 0);
}
static void cs_delete_ids(id s, SEL c, id ids, void *done) {
    (void)c;
    u64 n = ids ? MSG0(u64, ids, "count") : 0;
    for (u64 i = 0; i < n; i++) { id k = MSG(id, ids, "objectAtIndex:", i); MSG(void, ((csindex_t *)s)->items, "removeObjectForKey:", k); }
    call_blk1(done, 0);
}
static void cs_delete_domains(id s, SEL c, id domains, void *done) {
    (void)c;
    id d = ((csindex_t *)s)->items, keys = MSG0(id, d, "allKeys");
    u64 n = MSG0(u64, keys, "count");
    for (u64 i = 0; i < n; i++) {
        id k = MSG(id, keys, "objectAtIndex:", i);
        csitem_t *it = (csitem_t *)MSG(id, d, "objectForKey:", k);
        if (it->domain && domains && MSG(BOOL, domains, "containsObject:", it->domain)) MSG(void, d, "removeObjectForKey:", k);
    }
    call_blk1(done, 0);
}
static void cs_delete_all(id s, SEL c, void *done) { (void)c; MSG0(void, ((csindex_t *)s)->items, "removeAllObjects"); call_blk1(done, 0); }
static id ci_init(id s, SEL c, id uid, id domain, id attrs) { (void)c; csitem_t *x = (csitem_t *)s; x->uid = ret(uid); x->domain = ret(domain); x->attrs = ret(attrs); return s; }
static id ci_uid(id s, SEL c) { (void)c; return ((csitem_t *)s)->uid; }
static id ci_domain(id s, SEL c) { (void)c; return ((csitem_t *)s)->domain; }
static id ci_attrs(id s, SEL c) { (void)c; return ((csitem_t *)s)->attrs; }
static void ci_set_attrs(id s, SEL c, id a) { (void)c; setobj(&((csitem_t *)s)->attrs, a); }
static id ci_exp(id s, SEL c) { (void)c; return ((csitem_t *)s)->expiration; }
static void ci_set_exp(id s, SEL c, id d) { (void)c; setobj(&((csitem_t *)s)->expiration, d); }
static id ca_init(id s, SEL c, id type) { (void)c; ((csattrs_t *)s)->type = ret(type); ((csattrs_t *)s)->props = ret(mdict()); return s; }
/* propriedades (title, contentDescription, keywords, thumbnailData, ...): guardadas pelo nome do seletor */
static id ca_get(id s, SEL c) { csattrs_t *x = (csattrs_t *)s; return x->props ? MSG(id, x->props, "objectForKey:", cx_str(sel_getName(c))) : 0; }
static void ca_set(id s, SEL c, id v) {
    csattrs_t *x = (csattrs_t *)s;
    if (!x->props) x->props = ret(mdict());
    const char *n = sel_getName(c);                                   /* "setTitle:" → "title" */
    char k[64];
    size_t l = strlen(n);
    if (l < 5 || l - 4 >= sizeof k) return;
    memcpy(k, n + 3, l - 4); k[l - 4] = 0;
    if (k[0] >= 'A' && k[0] <= 'Z') k[0] = (char)(k[0] + 32);
    if (v) MSG(void, x->props, "setObject:forKey:", v, cx_str(k)); else MSG(void, x->props, "removeObjectForKey:", cx_str(k));
}

/* ================================================================ AVFoundation / AVFAudio */
id AVCaptureSessionInterruptionEndedNotification, AVCaptureSessionWasInterruptedNotification;
id AVFormatIDKey, AVLinearPCMBitDepthKey, AVLinearPCMIsBigEndianKey, AVLinearPCMIsFloatKey, AVLinearPCMIsNonInterleaved, AVSampleRateKey;
cx_class_t AVCaptureAudioDataOutput_c __asm__("OBJC_CLASS_$_AVCaptureAudioDataOutput"), AVCaptureAudioDataOutput_m __asm__("OBJC_METACLASS_$_AVCaptureAudioDataOutput");
typedef struct { void *isa; id delegate; void *queue; id settings; } avado_t;
static void ad_set_delegate(id s, SEL c, id d, void *q) { (void)c; ((avado_t *)s)->delegate = d; ((avado_t *)s)->queue = q; }
static id ad_delegate(id s, SEL c) { (void)c; return ((avado_t *)s)->delegate; }
static void *ad_queue(id s, SEL c) { (void)c; return ((avado_t *)s)->queue; }
static id ad_settings(id s, SEL c) { (void)c; return ((avado_t *)s)->settings; }
static void ad_set_settings(id s, SEL c, id d) { (void)c; setobj(&((avado_t *)s)->settings, d); }
static id ad_recommended(id s, SEL c, id type) { (void)s; (void)c; (void)type; return mdict(); }
static id ad_connections(id s, SEL c) { (void)s; (void)c; return MSG0(id, cls("NSArray"), "array"); }

/* ================================================================ AppKit */
cx_class_t NSPreviewRepresentingActivityItem_c __asm__("OBJC_CLASS_$_NSPreviewRepresentingActivityItem"), NSPreviewRepresentingActivityItem_m __asm__("OBJC_METACLASS_$_NSPreviewRepresentingActivityItem");
cx_class_t NSHapticFeedbackManager_c __asm__("OBJC_CLASS_$_NSHapticFeedbackManager"), NSHapticFeedbackManager_m __asm__("OBJC_METACLASS_$_NSHapticFeedbackManager");
typedef struct { void *isa; id item, title, image, icon; } nspra_t;
static id pr_init(id s, SEL c, id item, id title, id image, id icon) {
    (void)c;
    nspra_t *x = (nspra_t *)s;
    x->item = ret(item); x->title = ret(title); x->image = ret(image); x->icon = ret(icon);
    return s;
}
static id pr_init2(id s, SEL c, id item, id title, id image, id icon) { return pr_init(s, c, item, title, image, icon); }
static id pr_item(id s, SEL c) { (void)c; return ((nspra_t *)s)->item; }
static id pr_title(id s, SEL c) { (void)c; return ((nspra_t *)s)->title; }
static id pr_image(id s, SEL c) { (void)c; return ((nspra_t *)s)->image; }
static id pr_icon(id s, SEL c) { (void)c; return ((nspra_t *)s)->icon; }
static id g_performer;
static id hf_default(id k, SEL c) { (void)c; if (!g_performer) g_performer = cx_alloc_instance((cx_class_t *)k); return g_performer; }
static void hf_perform(id s, SEL c, i64 pattern, u64 when) { (void)s; (void)c; (void)pattern; (void)when; }

/* ================================================================ ApplicationServices / CoreGraphics */
int cx_tcc_preflight(const char *service);
u8 AXIsProcessTrusted(void) { return (u8)cx_tcc_preflight("kTCCServiceAccessibility"); }
id CGSessionCopyCurrentDictionary(void) {
    id d = mdict();
    id yes = MSG(id, cls("NSNumber"), "numberWithBool:", YES), uid = MSG(id, cls("NSNumber"), "numberWithInt:", 501);
    MSG(void, d, "setObject:forKey:", yes, cx_str("kCGSSessionOnConsoleKey"));
    MSG(void, d, "setObject:forKey:", yes, cx_str("kCGSessionLoginDoneKey"));
    MSG(void, d, "setObject:forKey:", uid, cx_str("kCGSSessionUserIDKey"));
    MSG(void, d, "setObject:forKey:", cx_str("Visitante"), cx_str("kCGSSessionUserNameKey"));
    return objc_retain(d);
}

/* ================================================================ CoreMedia */
typedef struct { i64 value; i32 timescale; u32 flags; i64 epoch; } cmtime_t;
/* método de arredondamento: 1 meio para longe de zero (padrão), 2 para zero, 3 para longe de zero, 4 para −∞, 5 para +∞ */
cmtime_t CMTimeConvertScale(cmtime_t t, i32 scale, u32 method) {
    if (!(t.flags & 1) || t.timescale <= 0 || scale <= 0 || (t.flags & 0x1C)) { t.timescale = scale > 0 ? scale : t.timescale; return t; }
/* t.value * scale / t.timescale sem __int128: quociente e resto separados (cabe em 64 bits para escalas reais) */    i64 den = t.timescale, qa = t.value / den, ra = t.value % den;    i64 q = qa * scale + (ra * scale) / den, r = (ra * scale) % den;    int rounded = r != 0;    if (r) {        int neg = t.value < 0;        switch (method) {        case 2: break;        case 3: q += neg ? -1 : 1; break;        case 4: if (neg) q -= 1; break;        case 5: if (!neg) q += 1; break;        default: { i64 ar = r < 0 ? -r : r; if (2 * ar >= den) q += neg ? -1 : 1; }        }    }    t.value = q;
    t.timescale = scale;
    if (rounded) t.flags |= 2;                                       /* kCMTimeFlags_HasBeenRounded */
    return t;
}

/* ================================================================ registro */
static void K(cx_class_t *k, cx_class_t *m, const char *name, cx_class_t *super, u32 size) { if (!cx_class_named(name)) cx_define_class(k, m, name, super, size); }
#define A(k, s, f) cx_add_method(k, s, (void *)(f))
#define AC(k, s, f) cx_add_cmethod(k, s, (void *)(f))
static id S(const char *s) { return ret(cx_str(s)); }
void cx_slack_frameworks_init(void) {
    cx_class_t *obj = cx_class_named("NSObject");
    NSURLFileSecurityKey = S("NSURLFileSecurityKey");
    kUTTypeApplicationBundle = S("com.apple.application-bundle");
    kUTTypeData = S("public.data");
    AVCaptureSessionInterruptionEndedNotification = S("AVCaptureSessionInterruptionEndedNotification");
    AVCaptureSessionWasInterruptedNotification = S("AVCaptureSessionWasInterruptedNotification");
    AVFormatIDKey = S("AVFormatIDKey"); AVLinearPCMBitDepthKey = S("AVLinearPCMBitDepthKey");
    AVLinearPCMIsBigEndianKey = S("AVLinearPCMIsBigEndianKey"); AVLinearPCMIsFloatKey = S("AVLinearPCMIsFloatKey");
    AVLinearPCMIsNonInterleaved = S("AVLinearPCMIsNonInterleaved"); AVSampleRateKey = S("AVSampleRateKey");

    cx_class_t *stream = cx_class_named("NSStream");
    K(&NSInputStream_c, &NSInputStream_m, "NSInputStream", stream ? stream : obj, (u32)(sizeof(nsin_t) - 8));
    cx_class_t *is = &NSInputStream_c;
    AC(is, "inputStreamWithData:", is_with_data); AC(is, "inputStreamWithFileAtPath:", is_with_file); AC(is, "inputStreamWithURL:", is_with_url);
    A(is, "initWithData:", is_init_data); A(is, "initWithFileAtPath:", is_init_file); A(is, "initWithURL:", is_init_url);
    A(is, "open", is_open); A(is, "close", is_close); A(is, "read:maxLength:", is_read); A(is, "hasBytesAvailable", is_has_bytes);
    A(is, "streamStatus", is_status); A(is, "streamError", is_error); A(is, "delegate", is_delegate); A(is, "setDelegate:", is_set_delegate);
    A(is, "getBuffer:length:", is_get_buffer); A(is, "scheduleInRunLoop:forMode:", is_schedule); A(is, "removeFromRunLoop:forMode:", is_schedule);
    A(is, "propertyForKey:", is_prop); A(is, "setProperty:forKey:", is_set_prop);

    K(&SMAppService_c, &SMAppService_m, "SMAppService", obj, (u32)(sizeof(smapp_t) - 8));
    AC(&SMAppService_c, "mainAppService", sm_main); AC(&SMAppService_c, "loginItemServiceWithIdentifier:", sm_login);
    AC(&SMAppService_c, "agentServiceWithPlistName:", sm_agent); AC(&SMAppService_c, "daemonServiceWithPlistName:", sm_daemon);
    AC(&SMAppService_c, "openSystemSettingsLoginItems", sm_open_settings);
    A(&SMAppService_c, "status", sm_status); A(&SMAppService_c, "registerAndReturnError:", sm_register);
    A(&SMAppService_c, "unregisterAndReturnError:", sm_unregister); A(&SMAppService_c, "unregisterWithCompletionHandler:", sm_unregister_cb);

    K(&CSSearchableIndex_c, &CSSearchableIndex_m, "CSSearchableIndex", obj, (u32)(sizeof(csindex_t) - 8));
    AC(&CSSearchableIndex_c, "defaultSearchableIndex", cs_default); AC(&CSSearchableIndex_c, "isIndexingAvailable", cs_available);
    A(&CSSearchableIndex_c, "initWithName:", cs_init_name);
    A(&CSSearchableIndex_c, "indexSearchableItems:completionHandler:", cs_index);
    A(&CSSearchableIndex_c, "deleteSearchableItemsWithIdentifiers:completionHandler:", cs_delete_ids);
    A(&CSSearchableIndex_c, "deleteSearchableItemsWithDomainIdentifiers:completionHandler:", cs_delete_domains);
    A(&CSSearchableIndex_c, "deleteAllSearchableItemsWithCompletionHandler:", cs_delete_all);
    K(&CSSearchableItem_c, &CSSearchableItem_m, "CSSearchableItem", obj, (u32)(sizeof(csitem_t) - 8));
    A(&CSSearchableItem_c, "initWithUniqueIdentifier:domainIdentifier:attributeSet:", ci_init);
    A(&CSSearchableItem_c, "uniqueIdentifier", ci_uid); A(&CSSearchableItem_c, "domainIdentifier", ci_domain);
    A(&CSSearchableItem_c, "attributeSet", ci_attrs); A(&CSSearchableItem_c, "setAttributeSet:", ci_set_attrs);
    A(&CSSearchableItem_c, "expirationDate", ci_exp); A(&CSSearchableItem_c, "setExpirationDate:", ci_set_exp);
    K(&CSSearchableItemAttributeSet_c, &CSSearchableItemAttributeSet_m, "CSSearchableItemAttributeSet", obj, (u32)(sizeof(csattrs_t) - 8));
    cx_class_t *ca = &CSSearchableItemAttributeSet_c;
    A(ca, "initWithItemContentType:", ca_init); A(ca, "initWithContentType:", ca_init);
    static const char *props[] = {"title", "contentDescription", "keywords", "thumbnailData", "thumbnailURL", "displayName", "contentURL",
                                  "contentCreationDate", "contentModificationDate", "authorNames", "textContent", "identifier", "relatedUniqueIdentifier"};
    for (size_t i = 0; i < sizeof props / sizeof *props; i++) {
        char set[80];
        snprintf(set, sizeof set, "set%c%s:", props[i][0] - 32, props[i] + 1);
        A(ca, props[i], ca_get); A(ca, set, ca_set);
    }

    K(&AVCaptureAudioDataOutput_c, &AVCaptureAudioDataOutput_m, "AVCaptureAudioDataOutput", cx_class_named("AVCaptureOutput") ? cx_class_named("AVCaptureOutput") : obj, (u32)(sizeof(avado_t) - 8));
    cx_class_t *ad = &AVCaptureAudioDataOutput_c;
    A(ad, "setSampleBufferDelegate:queue:", ad_set_delegate); A(ad, "sampleBufferDelegate", ad_delegate); A(ad, "sampleBufferCallbackQueue", ad_queue);
    A(ad, "audioSettings", ad_settings); A(ad, "setAudioSettings:", ad_set_settings);
    A(ad, "recommendedAudioSettingsForAssetWriterWithOutputFileType:", ad_recommended); A(ad, "connections", ad_connections);

    K(&NSPreviewRepresentingActivityItem_c, &NSPreviewRepresentingActivityItem_m, "NSPreviewRepresentingActivityItem", obj, (u32)(sizeof(nspra_t) - 8));
    cx_class_t *pr = &NSPreviewRepresentingActivityItem_c;
    A(pr, "initWithItem:title:image:icon:", pr_init); A(pr, "initWithItem:title:imageProvider:iconProvider:", pr_init2);
    A(pr, "item", pr_item); A(pr, "title", pr_title); A(pr, "imageProvider", pr_image); A(pr, "iconProvider", pr_icon);
    K(&NSHapticFeedbackManager_c, &NSHapticFeedbackManager_m, "NSHapticFeedbackManager", obj, 0);
    AC(&NSHapticFeedbackManager_c, "defaultPerformer", hf_default);
    A(&NSHapticFeedbackManager_c, "performFeedbackPattern:performanceTime:", hf_perform);
}
