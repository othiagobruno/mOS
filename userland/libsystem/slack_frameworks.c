/*
 * mOS libSystem — o que o Slack (machocheck, nov. 2026) ainda importava e o mOS não tinha, fora a libc++ (que vem da
 * /usr/lib/libc++.1.dylib do LLVM), a libcompression e a libz (frameworks Mach-O de userland/frameworks):
 *
 *  - libSystem: setiopolicy_np, atexit_b, __c_locale, dispatch_data_create (dispatch_data = NSData, como no ObjC);
 *  - CFPreferences por domínio (CopyValue/SetValue/Synchronize), gravadas em ~/Library/Preferences/<app>.plist;
 *  - CFFileSecurity e NSURLFileSecurityKey (NSInputStream foi para cfnetstream.c);
 *  - UTTypeConformsTo e kUTType* de CoreServices; SMAppService (itens de login, guardados em memória);
 *  - CoreSpotlight (índice local em memória com busca por texto), AVCaptureAudioDataOutput e as chaves do AVFAudio;
 *  - NSPreviewRepresentingActivityItem, NSHapticFeedbackManager (o mOS não tem trackpad com atuador: nada vibra);
 *  - AXIsProcessTrusted, CGEvent de teclado (CGEventPost entrega a tecla à janela-chave deste app) e
 *    CGSessionCopyCurrentDictionary. CMTimeConvertScale está em cmtime.c.
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
/* dispatch_data_*: dispatch_data.c */

/* CFPreferences por domínio: prefs.c */

/* NSInputStream/NSOutputStream/NSStream: cfnetstream.c */

/* ================================================================ CFFileSecurity e NSURLFileSecurityKey */
id NSURLFileSecurityKey;
typedef struct { u16 mode; u32 owner, group; u8 has_mode, has_owner, has_group, has_uuid; u8 uuid[16]; } cffsec_t;
void *CFFileSecurityCreate(void *alloc) { (void)alloc; return calloc(1, sizeof(cffsec_t)); }
u8 CFFileSecurityGetMode(void *s, u16 *mode) { cffsec_t *x = s; if (!x || !x->has_mode) return 0; if (mode) *mode = x->mode; return 1; }
u8 CFFileSecuritySetMode(void *s, u16 mode) { cffsec_t *x = s; if (!x) return 0; x->mode = mode; x->has_mode = 1; return 1; }
u8 CFFileSecuritySetOwner(void *s, u32 uid) { cffsec_t *x = s; if (!x) return 0; x->owner = uid; x->has_owner = 1; return 1; }
u8 CFFileSecuritySetGroup(void *s, u32 gid) { cffsec_t *x = s; if (!x) return 0; x->group = gid; x->has_group = 1; return 1; }
u64 CFFileSecurityGetTypeID(void) { return 44; }
typedef struct { u8 b[16]; } gap_uuid_bytes;
id CFUUIDCreateFromUUIDBytes(void *alloc, gap_uuid_bytes b);
gap_uuid_bytes CFUUIDGetUUIDBytes(id u);
id CFFileSecurityCopyOwnerUUID(void *s) {
    cffsec_t *x = s;
    gap_uuid_bytes b;
    if (!x || !x->has_uuid) return 0;
    memcpy(b.b, x->uuid, 16);
    return CFUUIDCreateFromUUIDBytes(0, b);
}
u8 CFFileSecuritySetOwnerUUID(void *s, id uuid) {
    cffsec_t *x = s;
    gap_uuid_bytes b;
    if (!x || !uuid) return 0;
    b = CFUUIDGetUUIDBytes(uuid);
    memcpy(x->uuid, b.b, 16);
    x->has_uuid = 1;
    return 1;
}

/* ================================================================ UTType (CoreServices, a API antiga em C) */
id kUTTypeApplicationBundle, kUTTypeData;
id kUTTypeFolder, kUTTypePlainText, kUTTypeApplication, kUTTypeBundle, kUTTypePNG;
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
int cx_login_add(const char *path);
int cx_login_remove(const char *path);
int cx_login_has(const char *path);
int cx_smj_login_helper(const char *ident, char *path, size_t cap);    /* smjob.c */
int cx_open_target(const char *target, const char *app, int by_ident, int timeout_ms);
static void sm_key(smapp_t *m, char *out, size_t cap) {
    snprintf(out, cap, "%s:%s", m->cat[0] ? m->cat : "main", m->ident ? cx_cstr(m->ident) : "");
}
/* SMAppServiceStatus: 0 notRegistered, 1 enabled, 2 requiresApproval, 3 notFound */
enum { SM_NOT_REG = 0, SM_ENABLED = 1, SM_NEEDS_OK = 2, SM_NOT_FOUND = 3 };
/* SMAppServiceErrorDomain (= NSOSStatusErrorDomain no macOS) e os kSMError* */
enum { kSMErrorInternalFailure = 2, kSMErrorInvalidSignature = 3, kSMErrorJobNotFound = 6, kSMErrorJobPlistNotFound = 8,
       kSMErrorInvalidPlist = 10, kSMErrorLaunchDeniedByUser = 11 };
static id sm_error(long code, const char *msg) {
    id ui = MSG(id, cls("NSDictionary"), "dictionaryWithObject:forKey:", cx_str(msg), cx_str("NSLocalizedDescription"));
    return MSG(id, cls("NSError"), "errorWithDomain:code:userInfo:", cx_str("SMAppServiceErrorDomain"), code, ui);
}
/* o que o serviço aponta: o .app (principal ou ajudante de Contents/Library/LoginItems) ou o plist do agente/daemon */
int cx_launchd_cmd(unsigned op, const char *arg, char *out, unsigned cap);
static BOOL sm_target(smapp_t *m, char *path, size_t cap) {
    id b = MSG0(id, (id)cx_class_named("NSBundle"), "mainBundle");
    id bp = b ? MSG0(id, b, "bundlePath") : 0;
    if (!bp) return NO;
    if (!strcmp(m->cat, "main")) { snprintf(path, cap, "%s", cx_cstr(bp)); return YES; }
    if (!strcmp(m->cat, "login")) return m->ident && cx_smj_login_helper(cx_cstr(m->ident), path, cap) == 0;
    if (!m->ident) return NO;
    snprintf(path, cap, "%s/Contents/Library/%s/%s", cx_cstr(bp), m->kind == 2 ? "LaunchDaemons" : "LaunchAgents", cx_cstr(m->ident));
    return access(path, 0) == 0;
}
static i64 sm_status(id s, SEL c) {
    (void)c;
    smapp_t *m = (smapp_t *)s;
    char path[1200], key[300];
    if (!sm_target(m, path, sizeof path)) return m->status = SM_NOT_FOUND;
    /* app e ajudante: a verdade é a lista de login items (o usuário pode tirar nos Ajustes) */
    if (!m->kind) return m->status = cx_login_has(path) ? SM_ENABLED : SM_NOT_REG;
    sm_key(m, key, sizeof key);
    int disk = cx_sm_recall(key);
    return m->status = disk == SM_ENABLED ? SM_ENABLED : SM_NOT_REG;
}
static BOOL sm_register(id s, SEL c, id *err) {
    (void)c;
    smapp_t *m = (smapp_t *)s;
    if (err) *err = 0;
    char path[1200], out[256], key[300];
    sm_key(m, key, sizeof key);
    if (!sm_target(m, path, sizeof path)) {
        m->status = SM_NOT_FOUND;
        if (err) *err = sm_error(m->kind ? kSMErrorJobPlistNotFound : kSMErrorJobNotFound,
                                 m->kind ? "O plist do serviço não está em Contents/Library." : "O app ajudante não está em Contents/Library/LoginItems.");
        return NO;
    }
    if (!m->kind) {
        if (cx_login_add(path) != 0) { if (err) *err = sm_error(kSMErrorInternalFailure, "Não foi possível gravar a lista de itens de login."); return NO; }
    } else {
        out[0] = 0;
        int r = cx_launchd_cmd(2 /* LC_LOAD */, path, out, sizeof out);
        if (r != 0 && !strstr(out, "carregado")) {
            if (err) *err = sm_error(kSMErrorInvalidPlist, out[0] ? out : "O launchd recusou o plist.");
            return NO;
        }
        cx_sm_publish_file(m->kind == 2, path);
    }
    m->status = SM_ENABLED;
    cx_sm_remember(key, SM_ENABLED);
    return YES;
}
static BOOL sm_unregister(id s, SEL c, id *err) {
    (void)c;
    smapp_t *m = (smapp_t *)s;
    if (err) *err = 0;
    char path[1200], key[300];
    sm_key(m, key, sizeof key);
    if (!sm_target(m, path, sizeof path)) {
        if (err) *err = sm_error(kSMErrorJobNotFound, "O serviço não existe neste app.");
        return NO;
    }
    if (!m->kind) cx_login_remove(path);
    else {
        cx_launchd_cmd(3 /* LC_UNLOAD */, path, 0, 0);
        cx_sm_unpublish_file(m->kind == 2, path);
    }
    m->status = SM_NOT_REG;
    cx_sm_remember(key, SM_NOT_REG);
    return YES;
}
static void sm_unregister_cb(id s, SEL c, void *done) { id err = 0; sm_unregister(s, c, &err); call_blk1(done, err); }
/* item de login antigo (LSSharedFileList/SMLoginItemSetEnabled) num caminho qualquer */
static i64 sm_legacy_status(id k, SEL c, id url) {
    (void)k; (void)c;
    id p = url ? MSG0(id, url, "path") : 0;
    return p && cx_is_str(p) && cx_login_has(cx_cstr(p)) ? SM_ENABLED : SM_NOT_REG;
}
/* Ajustes > Geral > Itens de Início */
static void sm_open_settings(id k, SEL c) {
    (void)k; (void)c;
    cx_open_target("x-apple.systempreferences:com.apple.LoginItems-Settings.extension", 0, 0, 5000);
}

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
    kUTTypeFolder = S("public.folder");
    kUTTypePlainText = S("public.plain-text");
    kUTTypeApplication = S("com.apple.application");
    kUTTypeBundle = S("com.apple.bundle");
    kUTTypePNG = S("public.png");
    AVCaptureSessionInterruptionEndedNotification = S("AVCaptureSessionInterruptionEndedNotification");
    AVCaptureSessionWasInterruptedNotification = S("AVCaptureSessionWasInterruptedNotification");
    AVFormatIDKey = S("AVFormatIDKey"); AVLinearPCMBitDepthKey = S("AVLinearPCMBitDepthKey");
    AVLinearPCMIsBigEndianKey = S("AVLinearPCMIsBigEndianKey"); AVLinearPCMIsFloatKey = S("AVLinearPCMIsFloatKey");
    AVLinearPCMIsNonInterleaved = S("AVLinearPCMIsNonInterleaved"); AVSampleRateKey = S("AVSampleRateKey");

    K(&SMAppService_c, &SMAppService_m, "SMAppService", obj, (u32)(sizeof(smapp_t) - 8));
    AC(&SMAppService_c, "mainAppService", sm_main); AC(&SMAppService_c, "loginItemServiceWithIdentifier:", sm_login);
    AC(&SMAppService_c, "agentServiceWithPlistName:", sm_agent); AC(&SMAppService_c, "daemonServiceWithPlistName:", sm_daemon);
    AC(&SMAppService_c, "openSystemSettingsLoginItems", sm_open_settings);
    A(&SMAppService_c, "status", sm_status); A(&SMAppService_c, "registerAndReturnError:", sm_register);
    A(&SMAppService_c, "unregisterAndReturnError:", sm_unregister); A(&SMAppService_c, "unregisterWithCompletionHandler:", sm_unregister_cb);
    AC(&SMAppService_c, "statusForLegacyURL:", sm_legacy_status);

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
