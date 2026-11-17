/* mOS libSystem — iCloud no Foundation sem conta da Apple.
 *
 * NSUbiquitousKeyValueStore: funciona como no Mac sem iCloud: os valores ficam só neste computador, em
 * ~/Library/SyncedPreferences/<CFBundleIdentifier>.plist (o mesmo lugar do cache local do macOS), com os limites da
 * Apple (1024 chaves, 64 bytes de chave, 1 MB no total). Passar do limite não grava e avisa com
 * NSUbiquitousKeyValueStoreDidChangeExternallyNotification (motivo NSUbiquitousKeyValueStoreQuotaViolationChange).
 * synchronize grava o arquivo e devolve YES.
 *
 * NSFileManager: sem contêiner do iCloud (URLForUbiquityContainerIdentifier: = nil, ubiquityIdentityToken = nil em
 * compat_objc_extra.c); mover para o iCloud, baixar, despejar e publicar falham com NSCocoaErrorDomain 4353
 * (NSUbiquitousFileUnavailableError), como o macOS sem conta. */
#include "foundation.h"
#include "libc.h"

id NSHomeDirectory(void);
int cx_snprintf(char *buf, unsigned long n, const char *fmt, ...);
id objc_retain(id o);
void objc_release(id o);
void cx_post(id note);
id cx_note(id name, id object, id info);

cx_class_t KVS_c __asm__("OBJC_CLASS_$_NSUbiquitousKeyValueStore");
cx_class_t KVS_m __asm__("OBJC_METACLASS_$_NSUbiquitousKeyValueStore");
typedef struct { cx_class_t *isa; id dict; id path; } kvs_t;

/* constantes (NSUbiquitousKeyValueStore.h) */
typedef struct { void *isa; u64 flags; const char *s; u64 n; } kcs_t;
extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
#define KS(var, text) static kcs_t var##_s = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; const void *var = &var##_s;
KS(NSUbiquitousKeyValueStoreDidChangeExternallyNotification, "NSUbiquitousKeyValueStoreDidChangeExternallyNotification")
KS(NSUbiquitousKeyValueStoreChangeReasonKey, "NSUbiquitousKeyValueStoreChangeReasonKey")
KS(NSUbiquitousKeyValueStoreChangedKeysKey, "NSUbiquitousKeyValueStoreChangedKeysKey")
enum { kServerChange = 0, kInitialSync = 1, kQuotaViolation = 2, kAccountChange = 3 };
enum { KVS_MAX_KEYS = 1024, KVS_MAX_KEY = 64, KVS_MAX_TOTAL = 1024 * 1024 };

static id kvs_path(void) {
    const char *home = getenv("HOME");
    if (!home || !*home) home = cx_cstr(NSHomeDirectory());
    id bundle = MSG(id, (id)cx_class_named("NSBundle"), "mainBundle");
    id bid = bundle ? MSG(id, bundle, "bundleIdentifier") : 0;
    const char *b = bid ? cx_cstr(bid) : 0;
    char dir[600], p[700];
    cx_snprintf(dir, sizeof dir, "%s/Library/SyncedPreferences", home ? home : "/Users/Visitante");
    mkdir(dir, 0700);
    cx_snprintf(p, sizeof p, "%s/%s.plist", dir, b && *b ? b : "mos.app");
    return objc_retain(cx_str(p));
}
static kvs_t *kvs_default(void) {
    static kvs_t *g;
    if (g) return g;
    kvs_t *k = (kvs_t *)cx_alloc_instance(&KVS_c);
    k->path = kvs_path();
    id loaded = MSG(id, (id)cx_class_named("NSDictionary"), "dictionaryWithContentsOfFile:", k->path);
    k->dict = objc_retain(loaded ? MSG(id, loaded, "mutableCopy") : MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary"));
    if (loaded) objc_release(k->dict);                 /* mutableCopy já devolve +1 */
    g = k;
    return g;
}
static id kvs_default_m(id cls, SEL c) { (void)cls; (void)c; return (id)kvs_default(); }
static id kvs_init(id self, SEL c) { (void)c; objc_release(self); return objc_retain((id)kvs_default()); }

/* tamanho aproximado de um valor de plist em bytes (para a cota de 1 MB) */
static u64 val_size(id v) {
    if (!v) return 0;
    if (cx_is_str(v)) return strlen(cx_cstr(v));
    if (MSG(BOOL, v, "isKindOfClass:", (id)cx_class_named("NSData"))) return MSG0(u64, v, "length");
    if (MSG(BOOL, v, "isKindOfClass:", (id)cx_class_named("NSArray"))) {
        u64 n = MSG0(u64, v, "count"), s = 0;
        for (u64 i = 0; i < n; i++) s += val_size(MSG(id, v, "objectAtIndex:", i));
        return s;
    }
    if (MSG(BOOL, v, "isKindOfClass:", (id)cx_class_named("NSDictionary"))) {
        id keys = MSG0(id, v, "allKeys");
        u64 n = MSG0(u64, keys, "count"), s = 0;
        for (u64 i = 0; i < n; i++) { id k = MSG(id, keys, "objectAtIndex:", i); s += val_size(k) + val_size(MSG(id, v, "objectForKey:", k)); }
        return s;
    }
    return 8;                                           /* número, data, booleano */
}
static void kvs_notify(kvs_t *k, int reason, id key) {
    id info = MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary");
    id r = cx_num_i(reason);
    MSG(void, info, "setObject:forKey:", r, (id)NSUbiquitousKeyValueStoreChangeReasonKey);
    objc_release(r);
    if (key) {
        id keys = MSG(id, (id)cx_class_named("NSArray"), "arrayWithObject:", key);
        MSG(void, info, "setObject:forKey:", keys, (id)NSUbiquitousKeyValueStoreChangedKeysKey);
    }
    cx_post(cx_note((id)NSUbiquitousKeyValueStoreDidChangeExternallyNotification, (id)k, info));
}
static void kvs_set(id self, SEL c, id v, id key) {
    (void)c;
    kvs_t *k = (kvs_t *)self;
    if (!key || !cx_is_str(key)) return;
    if (!v) { MSG(void, k->dict, "removeObjectForKey:", key); return; }
    u64 total = val_size(k->dict) - val_size(MSG(id, k->dict, "objectForKey:", key)) + val_size(v) + val_size(key);
    int new_key = MSG(id, k->dict, "objectForKey:", key) == 0;
    if (strlen(cx_cstr(key)) > KVS_MAX_KEY || total > KVS_MAX_TOTAL ||
        (new_key && MSG0(u64, k->dict, "count") >= KVS_MAX_KEYS)) {
        kvs_notify(k, kQuotaViolation, key);
        return;
    }
    MSG(void, k->dict, "setObject:forKey:", v, key);
}
static id kvs_get(id self, SEL c, id key) { (void)c; return key ? MSG(id, ((kvs_t *)self)->dict, "objectForKey:", key) : 0; }
static id kvs_get_typed(id self, const char *cls, id key) {
    id v = kvs_get(self, 0, key);
    return v && MSG(BOOL, v, "isKindOfClass:", (id)cx_class_named(cls)) ? v : 0;
}
static id g_str(id s, SEL c, id k) { (void)c; return kvs_get_typed(s, "NSString", k); }
static id g_arr(id s, SEL c, id k) { (void)c; return kvs_get_typed(s, "NSArray", k); }
static id g_dict(id s, SEL c, id k) { (void)c; return kvs_get_typed(s, "NSDictionary", k); }
static id g_data(id s, SEL c, id k) { (void)c; return kvs_get_typed(s, "NSData", k); }
static i64 g_ll(id s, SEL c, id k) { (void)c; id v = kvs_get_typed(s, "NSNumber", k); return v ? MSG0(i64, v, "longLongValue") : 0; }
static double g_dbl(id s, SEL c, id k) { (void)c; id v = kvs_get_typed(s, "NSNumber", k); return v ? MSG0(double, v, "doubleValue") : 0; }
static BOOL g_bool(id s, SEL c, id k) { (void)c; id v = kvs_get_typed(s, "NSNumber", k); return v ? MSG0(BOOL, v, "boolValue") : NO; }
static void s_ll(id s, SEL c, i64 v, id k) { (void)c; id n = cx_num_i(v); kvs_set(s, 0, n, k); objc_release(n); }   /* cx_num_* = +1 */
static void s_dbl(id s, SEL c, double v, id k) { (void)c; id n = cx_num_d(v); kvs_set(s, 0, n, k); objc_release(n); }
static void s_bool(id s, SEL c, BOOL v, id k) { (void)c; kvs_set(s, 0, MSG(id, (id)cx_class_named("NSNumber"), "numberWithBool:", v), k); }
static void kvs_remove(id s, SEL c, id k) { (void)c; if (k) MSG(void, ((kvs_t *)s)->dict, "removeObjectForKey:", k); }
static id kvs_rep(id s, SEL c) { (void)c; return MSG0(id, ((kvs_t *)s)->dict, "copy"); }
static BOOL kvs_sync(id s, SEL c) {
    (void)c;
    kvs_t *k = (kvs_t *)s;
    return MSG(BOOL, k->dict, "writeToFile:atomically:", k->path, YES);
}

/* ---- NSFileManager sem contêiner do iCloud */
static id fm_ubiq_url(id s, SEL c, id ident) { (void)s; (void)c; (void)ident; return 0; }
static BOOL fm_is_ubiq(id s, SEL c, id url) { (void)s; (void)c; (void)url; return NO; }
static void ubiq_error(id *err) {
    if (!err) return;
    id info = MSG(id, (id)cx_class_named("NSDictionary"), "dictionaryWithObject:forKey:",
                  cx_str("O iCloud não está disponível neste computador (o mOS não tem iCloud)."), cx_str("NSLocalizedDescription"));
    *err = MSG(id, (id)cx_class_named("NSError"), "errorWithDomain:code:userInfo:", cx_str("NSCocoaErrorDomain"), (i64)4353, info);
}
static BOOL fm_set_ubiq(id s, SEL c, BOOL flag, id url, id dest, id *err) { (void)s; (void)c; (void)flag; (void)url; (void)dest; ubiq_error(err); return NO; }
static BOOL fm_start_dl(id s, SEL c, id url, id *err) { (void)s; (void)c; (void)url; ubiq_error(err); return NO; }
static BOOL fm_evict(id s, SEL c, id url, id *err) { (void)s; (void)c; (void)url; ubiq_error(err); return NO; }
static id fm_publish(id s, SEL c, id url, id *date, id *err) { (void)s; (void)c; (void)url; if (date) *date = 0; ubiq_error(err); return 0; }

void cx_icloud_init(void) {
    static int done;
    if (done++) return;
    cx_define_class(&KVS_c, &KVS_m, "NSUbiquitousKeyValueStore", cx_class_named("NSObject"), (u32)(sizeof(kvs_t) - 8));
    cx_add_cmethod(&KVS_c, "defaultStore", (void *)kvs_default_m);
    cx_add_method(&KVS_c, "init", (void *)kvs_init);
    cx_add_method(&KVS_c, "objectForKey:", (void *)kvs_get);
    cx_add_method(&KVS_c, "setObject:forKey:", (void *)kvs_set);
    cx_add_method(&KVS_c, "removeObjectForKey:", (void *)kvs_remove);
    cx_add_method(&KVS_c, "stringForKey:", (void *)g_str);
    cx_add_method(&KVS_c, "arrayForKey:", (void *)g_arr);
    cx_add_method(&KVS_c, "dictionaryForKey:", (void *)g_dict);
    cx_add_method(&KVS_c, "dataForKey:", (void *)g_data);
    cx_add_method(&KVS_c, "longLongForKey:", (void *)g_ll);
    cx_add_method(&KVS_c, "doubleForKey:", (void *)g_dbl);
    cx_add_method(&KVS_c, "boolForKey:", (void *)g_bool);
    cx_add_method(&KVS_c, "setString:forKey:", (void *)kvs_set);
    cx_add_method(&KVS_c, "setData:forKey:", (void *)kvs_set);
    cx_add_method(&KVS_c, "setArray:forKey:", (void *)kvs_set);
    cx_add_method(&KVS_c, "setDictionary:forKey:", (void *)kvs_set);
    cx_add_method(&KVS_c, "setLongLong:forKey:", (void *)s_ll);
    cx_add_method(&KVS_c, "setDouble:forKey:", (void *)s_dbl);
    cx_add_method(&KVS_c, "setBool:forKey:", (void *)s_bool);
    cx_add_method(&KVS_c, "dictionaryRepresentation", (void *)kvs_rep);
    cx_add_method(&KVS_c, "synchronize", (void *)kvs_sync);
    cx_class_t *fm = cx_class_named("NSFileManager");
    if (fm) {
        cx_add_method(fm, "URLForUbiquityContainerIdentifier:", (void *)fm_ubiq_url);
        cx_add_method(fm, "isUbiquitousItemAtURL:", (void *)fm_is_ubiq);
        cx_add_method(fm, "setUbiquitous:itemAtURL:destinationURL:error:", (void *)fm_set_ubiq);
        cx_add_method(fm, "startDownloadingUbiquitousItemAtURL:error:", (void *)fm_start_dl);
        cx_add_method(fm, "evictUbiquitousItemAtURL:error:", (void *)fm_evict);
        cx_add_method(fm, "URLForPublishingUbiquitousItemAtURL:expirationDate:error:", (void *)fm_publish);
    }
}
