/*
 * ASWebAuthenticationSession: abre a URL de login no navegador externo
 * (NSWorkspace openURL:, que é o LSOpen) e completa quando a URL de callback
 * chega (kAEGetURL / application:openURLs:, via cx_webauth_offer_url).
 * O token vai para o Keychain (kSecClassGenericPassword, serviço mos.oauth.<esquema>).
 * Sem conta real: o teste de host entrega uma URL local; nada aqui fala com a rede.
 */
#include "foundation.h"
typedef int OSStatus;
#include "sys.h"
#include "webauth_core.inc"

static id cls(const char *n) { return (id)cx_class_named(n); }

typedef struct { void *isa; int flags, reserved; void (*invoke)(void *, id, id); } wa_blk_t;

extern OSStatus SecItemAdd(id attrs, id *result);
extern OSStatus SecItemUpdate(id query, id changes);
extern const void *kSecClass, *kSecClassGenericPassword, *kSecAttrService, *kSecAttrAccount, *kSecValueData;
extern id CFDictionaryCreateMutable(void *a, long n, const void *k, const void *v);
extern void CFDictionarySetValue(id d, const void *k, const void *v);
extern const void *kCFTypeDictionaryKeyCallBacks, *kCFTypeDictionaryValueCallBacks;
extern id cx_data_new(const void *b, u64 n);

static id g_mgr;
static id g_bind_obj[WA_N];
static id g_bind_block[WA_N];
static id g_bind_provider[WA_N];
static int g_bind_eph[WA_N];

static int bind_of(id obj) {
    for (int i = 0; i < WA_N; i++) if (g_bind_obj[i] == obj) return i;
    return -1;
}

static const char *str_c(id o) { return (o && cx_is_str(o)) ? cx_cstr(o) : 0; }

static id url_abs(id url) {
    if (!url) return 0;
    if (cx_is_str(url)) return url;
    id s = MSG0(id, url, "absoluteString");
    return cx_is_str(s) ? s : 0;
}

static void store_secret(const wa_secret *sec) {
    if (!sec || !sec->secret[0]) return;
    id d = CFDictionaryCreateMutable(0, 0, kCFTypeDictionaryKeyCallBacks, kCFTypeDictionaryValueCallBacks);
    if (!d) return;
    id svc = cx_str(sec->service);
    id acct = cx_str(sec->account);
    id data = cx_data_new(sec->secret, strlen(sec->secret));
    CFDictionarySetValue(d, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(d, kSecAttrService, svc);
    CFDictionarySetValue(d, kSecAttrAccount, acct);
    CFDictionarySetValue(d, kSecValueData, data);
    id result = 0;
    OSStatus st = SecItemAdd(d, &result);
    if (st == -25299) {
        id ch = CFDictionaryCreateMutable(0, 0, kCFTypeDictionaryKeyCallBacks, kCFTypeDictionaryValueCallBacks);
        if (ch) {
            CFDictionarySetValue(ch, kSecValueData, data);
            SecItemUpdate(d, ch);
        }
    }
}

static void finish_slot(int slot, id cb_url, id err) {
    id blk = g_bind_block[slot];
    g_bind_block[slot] = 0;
    if (blk) {
        wa_blk_t *b = (wa_blk_t *)blk;
        if (b->invoke) b->invoke(blk, cb_url, err);
    }
}

static id err_cancel(void) {
    return MSG(id, cls("NSError"), "errorWithDomain:code:userInfo:",
               cx_str("com.apple.AuthenticationServices.WebAuthenticationSession"), (i64)1, (id)0);
}

int cx_webauth_offer_url(const char *url) {
    if (!url) return 0;
    int slot = -1;
    wa_secret sec;
    if (!wa_offer(url, &slot, &sec)) return 0;
    store_secret(&sec);
    id u = MSG(id, cls("NSURL"), "URLWithString:", cx_str(url));
    finish_slot(slot, u, (id)0);
    g_bind_obj[slot] = 0;
    return 1;
}

static id sess_init(id self, SEL c, id url, id scheme, id block) {
    (void)c;
    const char *us = str_c(url_abs(url));
    const char *ss = str_c(scheme);
    if (!us) return 0;
    int slot = wa_create(us, ss);
    if (slot < 0) return 0;
    g_bind_obj[slot] = self;
    g_bind_block[slot] = block ? (id)_Block_copy(block) : (id)0;
    return self;
}

static id sess_init_url(id self, SEL c, id url, id scheme, id block) {
    return sess_init(self, c, url, scheme, block);
}

static id sess_init_cb(id self, SEL c, id url, id callback, id block) {
    id scheme = 0;
    if (callback) scheme = MSG0(id, callback, "customScheme");
    return sess_init(self, c, url, scheme, block);
}

static BOOL sess_start(id self, SEL c) {
    (void)c;
    int slot = bind_of(self);
    if (slot < 0 || !wa_mark_open(slot)) return NO;
    id ws = MSG0(id, cls("NSWorkspace"), "sharedWorkspace");
    id u = MSG(id, cls("NSURL"), "URLWithString:", cx_str(wa_slots[slot].auth_url));
    if (ws && u) MSG(BOOL, ws, "openURL:", u);
    return YES;
}

static void sess_cancel(id self, SEL c) {
    (void)c;
    int slot = bind_of(self);
    if (slot < 0 || !wa_cancel(slot)) return;
    finish_slot(slot, (id)0, err_cancel());
    g_bind_obj[slot] = 0;
}

static void sess_set_provider(id self, SEL c, id p) { (void)c; int slot = bind_of(self); if (slot >= 0) g_bind_provider[slot] = p; }
static id sess_provider(id self, SEL c) { (void)c; int slot = bind_of(self); return slot >= 0 ? g_bind_provider[slot] : (id)0; }
static void sess_set_eph(id self, SEL c, BOOL v) { (void)c; int slot = bind_of(self); if (slot >= 0) g_bind_eph[slot] = v ? 1 : 0; }
static BOOL sess_eph(id self, SEL c) { (void)c; int slot = bind_of(self); return slot >= 0 && g_bind_eph[slot] ? YES : NO; }

static cx_class_t CB_c, CB_m;
static char g_cb_scheme[WA_N][WA_SCH];
static id g_cb_obj[WA_N];

static int cb_of(id o) {
    for (int i = 0; i < WA_N; i++) if (g_cb_obj[i] == o) return i;
    return -1;
}

static id cb_with_scheme(id selfcls, SEL c, id scheme) {
    (void)selfcls; (void)c;
    int free_i = -1;
    for (int i = 0; i < WA_N; i++) if (!g_cb_obj[i]) { free_i = i; break; }
    if (free_i < 0) return 0;
    id o = cx_alloc_instance(&CB_c);
    g_cb_obj[free_i] = o;
    g_cb_scheme[free_i][0] = 0;
    const char *s = str_c(scheme);
    if (s && strlen(s) < WA_SCH) snprintf(g_cb_scheme[free_i], WA_SCH, "%s", s);
    return o;
}

static id cb_scheme(id self, SEL c) {
    (void)c;
    int slot = cb_of(self);
    return slot >= 0 && g_cb_scheme[slot][0] ? cx_str(g_cb_scheme[slot]) : (id)0;
}

static id mgr_shared(id selfcls, SEL c) {
    (void)c;
    (void)selfcls;
    if (!g_mgr) g_mgr = cx_alloc_instance(cx_class_named("ASWebAuthenticationSessionWebBrowserSessionManager"));
    return g_mgr;
}

static void addm(const char *name, const char *sel, void *fn, int meta) {
    cx_class_t *k = cx_class_named(name);
    if (!k) return;
    if (meta) k = (cx_class_t *)object_getClass((id)k);
    cx_add_method(k, sel, fn);
}

void cx_webauth_init(void) {
    if (!cx_class_named("ASWebAuthenticationSessionCallback"))
        cx_define_class(&CB_c, &CB_m, "ASWebAuthenticationSessionCallback", cx_class_named("NSObject"), 8);
    addm("ASWebAuthenticationSession", "initWithURL:callbackURLScheme:completionHandler:", (void *)sess_init_url, 0);
    addm("ASWebAuthenticationSession", "initWithURL:callback:completionHandler:", (void *)sess_init_cb, 0);
    addm("ASWebAuthenticationSession", "start", (void *)sess_start, 0);
    addm("ASWebAuthenticationSession", "cancel", (void *)sess_cancel, 0);
    addm("ASWebAuthenticationSession", "setPresentationContextProvider:", (void *)sess_set_provider, 0);
    addm("ASWebAuthenticationSession", "presentationContextProvider", (void *)sess_provider, 0);
    addm("ASWebAuthenticationSession", "setPrefersEphemeralWebBrowserSession:", (void *)sess_set_eph, 0);
    addm("ASWebAuthenticationSession", "prefersEphemeralWebBrowserSession", (void *)sess_eph, 0);
    addm("ASWebAuthenticationSessionCallback", "callbackWithCustomScheme:", (void *)cb_with_scheme, 1);
    addm("ASWebAuthenticationSessionCallback", "customScheme", (void *)cb_scheme, 0);
    addm("ASWebAuthenticationSessionWebBrowserSessionManager", "sharedManager", (void *)mgr_shared, 1);
}
