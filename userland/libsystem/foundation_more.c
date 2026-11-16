/*
 * mOS libSystem — o resto do Foundation que os apps do macOS importam de Foundation.framework.
 * Classes com o símbolo OBJC_CLASS_$_ da Apple, constantes NSString* e as funções C
 * (geometria que ainda faltava, NSDecimal, NSMap/NSHash, exceções, protocolos).
 * Registrado em cx_foundation_more_init, chamado no boot do runtime (cx_darwin_init),
 * depois do Foundation e do AppKit, porque algumas classes herdam dos dois.
 */
#include "foundation.h"
#include "libc.h"
#include "fdn_decimal_core.h"
#include "urlrequest_fields.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
extern id cx_plist_parse(const u8 *b, u64 n);
#define XCLASS(N) extern cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N);
#define XMETA(N) extern cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N);
XCLASS(NSObject) XCLASS(NSString) XCLASS(NSMutableString) XCLASS(NSNumber) XCLASS(NSArray) XCLASS(NSMutableArray)
XCLASS(NSDictionary) XCLASS(NSMutableDictionary) XCLASS(NSData) XCLASS(NSDate) XCLASS(NSNull) XCLASS(NSError) XMETA(NSError)
XCLASS(NSSet) XCLASS(NSMutableSet) XCLASS(NSValue)
XCLASS(NSAttributedString)
extern cx_class_t NSConstantString_c __asm__("__CFConstantStringClassReference");
char *strcasestr(const char *h, const char *n);
extern id NSHomeDirectory(void);
int pipe(int *fds);
#define NSNotFound ((u64)0x7fffffffffffffffULL)

#define CLASS(N) cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N); cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N);
CLASS(NSAffineTransform) CLASS(NSAppleEventDescriptor) CLASS(NSAssertionHandler) 
CLASS(NSBundleResourceRequest) CLASS(NSComparisonPredicate)
CLASS(NSCompoundPredicate) CLASS(NSCondition) CLASS(NSConditionLock) CLASS(NSDataDetector)

CLASS(NSExpression) CLASS(NSFileCoordinator) CLASS(NSFileHandle) CLASS(NSHashTable)
CLASS(NSHTTPURLResponse) CLASS(NSIndexPath)
CLASS(NSItemProvider) CLASS(NSJSONSerialization)
CLASS(NSKeyedArchiver) CLASS(NSKeyedUnarchiver) CLASS(NSLinguisticTagger) CLASS(NSLock)
CLASS(NSMachBootstrapServer) CLASS(NSMachPort) CLASS(NSMapTable) 
CLASS(NSMetadataItem) CLASS(NSMetadataQuery) CLASS(NSMutableAttributedString)
CLASS(NSMutableURLRequest) CLASS(NSNetService) CLASS(NSNetServiceBrowser)

CLASS(NSPipe) CLASS(NSPointerArray) CLASS(NSPredicate) CLASS(NSProgress) CLASS(NSPropertyListSerialization)
CLASS(NSProxy) CLASS(NSPurgeableData) CLASS(NSRecursiveLock) CLASS(NSRegularExpression)
CLASS(NSScriptClassDescription) CLASS(NSScriptCommand)
CLASS(NSSecureUnarchiveFromDataTransformer) CLASS(NSSortDescriptor) CLASS(NSTask) CLASS(NSTextCheckingResult)
CLASS(NSUniqueIDSpecifier) 

CLASS(NSURLComponents)
CLASS(NSURLQueryItem) CLASS(NSURLRequest) CLASS(NSURLResponse)


CLASS(NSUserActivity) CLASS(NSUserNotification) CLASS(NSUserNotificationCenter)
CLASS(NSUUID) CLASS(NSValueTransformer) CLASS(NSXMLParser) CLASS(NSXPCConnection)
CLASS(NSXPCInterface) CLASS(NSXPCListener) CLASS(NSXPCListenerEndpoint)

/* estado genérico: a maioria das classes guarda poucos campos */
typedef struct {
    cx_class_t *isa;
    id a, b, c, e;
    i64 i, j;
    double d, f;
    void *p;
    int fl, fl2;
} box_t;
#define BOX(o) ((box_t *)(o))
#define BEX (u32)(sizeof(box_t) - 8)
typedef struct { cx_class_t *isa; u32 kind; u32 pad; double d; i64 i; } nsnum_t; /* igual a foundation.c */
typedef struct { cx_class_t *isa; u8 *bytes; u64 len; } nsdata_t;
typedef struct { int sec, min, hour, mday, mon, year, wday, yday, isdst; long gmtoff; const char *zone; } mos_tm_t;
long mktime(mos_tm_t *tm);

static id retain(id o) { return o ? objc_retain(o) : 0; }
static void rel(id o) { if (o) objc_release(o); }
static id ar(id o) { return o ? objc_autorelease(o) : 0; }
static id ns(const char *s) { return s ? cx_str(s) : 0; }
static const char *cs(id o) { return cx_cstr(o); }
static id mutarr(void) { return cx_arr_new(&NSMutableArray_c); }
static id mutdict(void) { return cx_alloc_instance(&NSMutableDictionary_c); }
static void dset(id d, id k, id v) {
    if (!d || !k || !v) { rel(k); rel(v); return; }
    MSG(void, d, "setObject:forKey:", v, k);
    rel(k); rel(v);
}
static id dget(id d, const char *k) { id key = ns(k); id v = d ? MSG(id, d, "objectForKey:", key) : 0; rel(key); return v; }
static i64 ival(id o) { return o ? MSG0(i64, o, "integerValue") : 0; }
static double dval(id o) { return o ? MSG0(double, o, "doubleValue") : 0; }
static BOOL truth(id o) { return o ? MSG0(BOOL, o, "boolValue") : NO; }
static id num_i(i64 v) { return cx_num_i(v); }
static id num_d(double v) { return cx_num_d(v); }
static id num_b(BOOL v) { return MSG(id, (id)&NSNumber_c, "numberWithBool:", (int)v); }
static int is_cls(id o, cx_class_t *c) { return o && cx_is_subclass(o->isa, c); }
static id copystr(const char *s, u64 n) { char *b = malloc(n + 1); memcpy(b, s, n); b[n] = 0; id o = cx_str_n(b, n); free(b); return o; }

/* ---------------------------------------------------------------- constantes NSString (o valor é o da Apple) */
#define KNS(sym) static nsstr_t sym##_o = { &NSConstantString_c, 0, (char *)#sym, sizeof(#sym) - 1 }; id sym = (id)&sym##_o;
#define KNV(sym, lit) static nsstr_t sym##_o = { &NSConstantString_c, 0, (char *)lit, sizeof(lit) - 1 }; id sym = (id)&sym##_o;

KNS(NSFileCreationDate) KNS(NSFileGroupOwnerAccountID) KNS(NSFileHandleNotificationDataItem)
KNS(NSFileHandleOperationException) KNS(NSFileHandleReadCompletionNotification) KNS(NSFileModificationDate)
KNS(NSFileOwnerAccountID) KNS(NSFileProtectionComplete) KNS(NSFileProtectionCompleteUnlessOpen)
KNS(NSFileProtectionCompleteUntilFirstUserAuthentication) KNS(NSFileProtectionKey) KNS(NSFileProtectionNone)
KNS(NSFileReferenceCount) KNS(NSFileSize) KNS(NSFileSystemFileNumber) KNS(NSFileSystemFreeSize)
KNS(NSFileSystemNumber) KNS(NSFileSystemSize) KNS(NSFileType) KNS(NSFileTypeDirectory) KNS(NSFileTypeRegular)
KNS(NSFileTypeSymbolicLink) KNS(NSExtensionHostDidEnterBackgroundNotification)
KNV(NSHTTPCookieDomain, "Domain") KNV(NSHTTPCookieExpires, "Expires") KNV(NSHTTPCookieName, "Name")
KNV(NSHTTPCookiePath, "Path") KNV(NSHTTPCookieSecure, "Secure") KNV(NSHTTPCookieValue, "Value")
KNV(NSHTTPCookieVersion, "Version") KNS(NSInvalidUnarchiveOperationException)
KNV(NSKeyValueChangeKindKey, "kind") KNV(NSKeyValueChangeNewKey, "new") KNV(NSKeyValueChangeOldKey, "old")
KNV(NSKeyValueChangeNotificationIsPriorKey, "notificationIsPrior") KNV(NSKeyedArchiveRootObjectKey, "root")
KNV(NSLinguisticTagAdjective, "Adjective") KNV(NSLinguisticTagAdverb, "Adverb") KNV(NSLinguisticTagNoun, "Noun")
KNV(NSLinguisticTagOtherWord, "OtherWord") KNV(NSLinguisticTagSchemeLanguage, "Language")
KNV(NSLinguisticTagSchemeLexicalClass, "LexicalClass") KNV(NSLinguisticTagVerb, "Verb")
KNV(NSLocalizedRecoveryOptionsErrorKey, "NSLocalizedRecoveryOptions") KNV(NSDetailedErrorsKey, "NSDetailedErrors")
KNV(NSMultipleUnderlyingErrorsKey, "NSMultipleUnderlyingErrors")
KNV(NSMetadataItemDisplayNameKey, "kMDItemDisplayName") KNV(NSMetadataItemFSContentChangeDateKey, "kMDItemFSContentChangeDate")
KNV(NSMetadataItemFSNameKey, "kMDItemFSName") KNV(NSMetadataItemFSSizeKey, "kMDItemFSSize")
KNV(NSMetadataItemPathKey, "kMDItemPath") KNV(NSMetadataItemURLKey, "kMDItemURL")
KNS(NSMetadataQueryDidFinishGatheringNotification) KNS(NSMetadataQueryDidUpdateNotification)
KNS(NSMetadataQueryLocalComputerScope) KNS(NSMetadataQueryUbiquitousDataScope)
KNS(NSMetadataUbiquitousItemDownloadingErrorKey) KNS(NSMetadataUbiquitousItemDownloadingStatusCurrent)
KNS(NSMetadataUbiquitousItemDownloadingStatusKey) KNS(NSMetadataUbiquitousItemHasUnresolvedConflictsKey)
KNS(NSMetadataUbiquitousItemIsDownloadingKey) KNS(NSMetadataUbiquitousItemIsUploadedKey)
KNS(NSMetadataUbiquitousItemIsUploadingKey) KNS(NSMetadataUbiquitousItemPercentDownloadedKey)
KNS(NSMetadataUbiquitousItemPercentUploadedKey) KNS(NSMetadataUbiquitousItemUploadingErrorKey)
KNS(NSParseErrorException) KNS(NSProcessInfoPowerStateDidChangeNotification)
KNS(NSProcessInfoThermalStateDidChangeNotification)
KNV(NSTextCheckingCityKey, "City") KNV(NSTextCheckingCountryKey, "Country") KNV(NSTextCheckingStateKey, "State")
KNV(NSTextCheckingStreetKey, "Street") KNV(NSTextCheckingZIPKey, "ZIP")
KNS(NSUbiquityIdentityDidChangeNotification) KNS(NSUndoManagerDidCloseUndoGroupNotification)
KNS(NSUndoManagerDidRedoChangeNotification) KNS(NSUndoManagerDidUndoChangeNotification)
KNS(NSURLAuthenticationMethodClientCertificate) KNS(NSURLAuthenticationMethodHTTPBasic) KNS(NSURLAuthenticationMethodHTTPDigest)
KNS(NSURLAuthenticationMethodNegotiate) KNS(NSURLAuthenticationMethodNTLM) KNS(NSURLAuthenticationMethodServerTrust)
KNS(NSURLErrorDomain) KNV(NSURLErrorFailingURLErrorKey, "NSErrorFailingURLKey")
KNV(NSURLErrorFailingURLStringErrorKey, "NSErrorFailingURLStringKey") KNS(NSURLErrorKey)
KNS(NSURLErrorNetworkUnavailableReasonKey) KNS(NSURLSessionDownloadTaskResumeData)
KNS(NSUserDefaultsDidChangeNotification) KNS(NSUserNotificationDefaultSoundName) KNS(NSXMLParserErrorDomain)
const i64 NSURLSessionTransferSizeUnknown = -1;

typedef struct { double x, y; } pt_t;
typedef struct { double w, h; } sz_t;
typedef struct { double x, y, w, h; } rc_t;
typedef struct { u64 location, length; } rng_t;
const pt_t NSZeroPoint = {0, 0};
const sz_t NSZeroSize = {0, 0};
const rc_t NSZeroRect = {0, 0, 0, 0};
static double mind(double a, double b) { return a < b ? a : b; }
static double maxd(double a, double b) { return a > b ? a : b; }
rc_t NSUnionRect(rc_t a, rc_t b) {
    if (a.w <= 0 || a.h <= 0) return b;
    if (b.w <= 0 || b.h <= 0) return a;
    double x0 = mind(a.x, b.x), y0 = mind(a.y, b.y);
    rc_t r = {x0, y0, maxd(a.x + a.w, b.x + b.w) - x0, maxd(a.y + a.h, b.y + b.h) - y0};
    return r;
}
BOOL NSIsEmptyRect(rc_t r) { return r.w <= 0 || r.h <= 0; }
rng_t NSIntersectionRange(rng_t a, rng_t b) {
    u64 a1 = a.location + a.length, b1 = b.location + b.length, l = a.location > b.location ? a.location : b.location;
    u64 h = a1 < b1 ? a1 : b1;
    rng_t z = {0, 0};
    if (h <= l) return z;
    rng_t r = {l, h - l};
    return r;
}
rng_t NSUnionRange(rng_t a, rng_t b) {
    if (!a.length) return b;
    if (!b.length) return a;
    u64 l = a.location < b.location ? a.location : b.location;
    u64 h = a.location + a.length > b.location + b.length ? a.location + a.length : b.location + b.length;
    rng_t r = {l, h - l};
    return r;
}
id NSStringFromRange(rng_t r) { char b[64]; cx_snprintf(b, sizeof b, "{%llu, %llu}", (unsigned long long)r.location, (unsigned long long)r.length); return ar(ns(b)); }
rng_t NSRangeFromString(id s) {
    rng_t r = {0, 0};
    const char *p = cs(s);
    while (*p && (*p < '0' || *p > '9')) p++;
    r.location = (u64)strtoll(p, (char **)&p, 10);
    while (*p && (*p < '0' || *p > '9')) p++;
    r.length = (u64)strtoll(p, 0, 10);
    return r;
}

/* NSMapTable / NSHashTable em C (ponteiros). O objeto ObjC guarda este bloco. */
enum { MAP_MAGIC = 0x4D415054u, HASH_MAGIC = 0x48415348u };
typedef struct { u32 magic; const void **k; const void **v; u32 n, cap; } cxmap_t;
typedef u64 (*map_hash_f)(void *, const void *);
typedef BOOL (*map_eq_f)(void *, const void *, const void *);
typedef struct { map_hash_f hash; map_eq_f eq; void *retain_k; void *release_k; void *describe; const void *notAKey; } map_cbs_t;
static u64 map_hash_ptr(void *t, const void *k) { (void)t; return (u64)k >> 4; }
static BOOL map_eq_ptr(void *t, const void *a, const void *b) { (void)t; return a == b; }
const map_cbs_t NSAllMapTableKeys = { map_hash_ptr, map_eq_ptr, 0, 0, 0, (const void *)-1 };
static cxmap_t *map_new(void) { cxmap_t *m = calloc(1, sizeof *m); m->magic = MAP_MAGIC; return m; }
static void map_put(cxmap_t *m, const void *k, const void *v, int absent) {
    if (!m || m->magic != MAP_MAGIC) return;
    for (u32 i = 0; i < m->n; i++) if (m->k[i] == k) { if (!absent) m->v[i] = v; return; }
    if (m->n == m->cap) { m->cap = m->cap ? m->cap * 2 : 8; m->k = realloc(m->k, m->cap * sizeof(void *)); m->v = realloc(m->v, m->cap * sizeof(void *)); }
    m->k[m->n] = k; m->v[m->n++] = v;
}
void *NSMapGet(void *table, const void *key) {
    cxmap_t *m = table;
    if (!m || m->magic != MAP_MAGIC) return 0;
    for (u32 i = 0; i < m->n; i++) if (m->k[i] == key) return (void *)m->v[i];
    return 0;
}
void NSMapInsert(void *table, const void *key, const void *value) { map_put(table, key, value, 0); }
void NSMapInsertKnownAbsent(void *table, const void *key, const void *value) { map_put(table, key, value, 1); }
void NSMapRemove(void *table, const void *key) {
    cxmap_t *m = table;
    if (!m || m->magic != MAP_MAGIC) return;
    for (u32 i = 0; i < m->n; i++) if (m->k[i] == key) { m->k[i] = m->k[m->n - 1]; m->v[i] = m->v[--m->n]; return; }
}
void *NSHashInsertKnownAbsent(void *table, const void *pointer) {
    cxmap_t *m = table;
    if (!m || m->magic != HASH_MAGIC) return 0;
    for (u32 i = 0; i < m->n; i++) if (m->k[i] == pointer) return (void *)m->k[i];
    map_put(m, pointer, pointer, 0);
    m->magic = HASH_MAGIC;
    return (void *)pointer;
}
void NSHashRemove(void *table, const void *pointer) {
    cxmap_t *m = table;
    if (!m || m->magic != HASH_MAGIC) return;
    u32 mag = m->magic;
    NSMapRemove(m, pointer);
    m->magic = mag;
}

id NSAllocateObject(cx_class_t *cls, u64 extra, void *zone) { (void)extra; (void)zone; return cls ? cx_alloc_instance(cls) : 0; }
static int g_zone;
void *NSZoneFromPointer(const void *p) { (void)p; return &g_zone; }
id NSHomeDirectoryForUser(id user) {
    const char *u = user ? cs(user) : 0;
    if (!u || !u[0] || !strcmp(u, "Visitante")) return NSHomeDirectory();
    char b[256];
    cx_snprintf(b, sizeof b, "/Users/%s", u);
    return ar(ns(b));
}
static void (*exc_handler)(id);
void *NSGetUncaughtExceptionHandler(void) { return (void *)exc_handler; }
void NSSetUncaughtExceptionHandler(void (*h)(id)) { exc_handler = h; }
int NSExtensionMain(int argc, char **argv) { (void)argc; (void)argv; return 0; }
void *NSProtocolFromString(id s) { extern void *objc_getProtocol(const char *); return s ? objc_getProtocol(cs(s)) : 0; }
id NSStringFromProtocol(void *p) { extern const char *protocol_getName(void *); return ar(ns(p ? protocol_getName(p) : "nil")); }

/* ---------------------------------------------------------------- erros */
typedef struct { id obj, domain, info; i64 code; } erec_t;
static erec_t *erecs;
static u32 nerec, cerec;
static erec_t *erec_of(id o) { for (u32 i = 0; i < nerec; i++) if (erecs[i].obj == o) return &erecs[i]; return 0; }
static id make_err(const char *dom, i64 code, const char *desc) {
    id o = cx_alloc_instance(&NSError_c);
    if (nerec == cerec) { cerec = cerec ? cerec * 2 : 16; erecs = realloc(erecs, cerec * sizeof *erecs); }
    id info = mutdict();
    if (desc) dset(info, ns("NSLocalizedDescription"), ns(desc));
    erecs[nerec++] = (erec_t){o, ns(dom ? dom : "NSCocoaErrorDomain"), info, code};
    return o;
}
static id err_with(id cls, SEL c, id dom, i64 code, id info) {
    (void)c;
    id o = cx_alloc_instance(cls ? (cx_class_t *)cls : &NSError_c);
    if (nerec == cerec) { cerec = cerec ? cerec * 2 : 16; erecs = realloc(erecs, cerec * sizeof *erecs); }
    erecs[nerec++] = (erec_t){o, retain(dom), retain(info), code};
    return ar(o);
}
static id err_domain(id s, SEL c) { (void)c; erec_t *e = erec_of(s); return e ? e->domain : ns("NSCocoaErrorDomain"); }
static i64 err_code(id s, SEL c) { (void)c; erec_t *e = erec_of(s); return e ? e->code : 0; }
static id err_info(id s, SEL c) { (void)c; erec_t *e = erec_of(s); return e ? e->info : 0; }
/* -initWithDomain:code:userInfo: (o NSError(domain:code:userInfo:) do Swift; também em subclasses) */
static id err_init(id self, SEL c, id dom, i64 code, id info) {
    (void)c;
    if (!self) return 0;
    erec_t *e = erec_of(self);
    if (e) { e->domain = retain(dom); e->info = retain(info); e->code = code; return self; }
    if (nerec == cerec) { cerec = cerec ? cerec * 2 : 16; erecs = realloc(erecs, cerec * sizeof *erecs); }
    erecs[nerec++] = (erec_t){self, retain(dom), retain(info), code};
    return self;
}
/* as descrições perguntam ao próprio objeto (-userInfo/-domain/-code): o __SwiftNativeNSError do runtime Swift (erro
 * Swift visto como NSError) sobrescreve esses três e não tem registro aqui */
static id err_desc(id s, SEL c) {
    (void)c;
    id info = MSG0(id, s, "userInfo");
    id d = info ? dget(info, "NSLocalizedDescription") : 0;
    if (d) return d;
    id dom = MSG0(id, s, "domain");
    char b[160];
    cx_snprintf(b, sizeof b, "A operação não pôde ser concluída. (%s erro %lld.)", dom ? cs(dom) : "erro", (long long)MSG0(i64, s, "code"));
    return ar(ns(b));
}
static id err_reason(id s, SEL c) { (void)c; id info = MSG0(id, s, "userInfo"); return info ? dget(info, "NSLocalizedFailureReason") : 0; }
static id err_suggestion(id s, SEL c) { (void)c; id info = MSG0(id, s, "userInfo"); return info ? dget(info, "NSLocalizedRecoverySuggestion") : 0; }

/* ---------------------------------------------------------------- JSON */
static const char *js_skip(const char *p, const char *e) { while (p < e && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++; return p; }
static int js_hex(char c) { if (c >= '0' && c <= '9') return c - '0'; if (c >= 'a' && c <= 'f') return c - 'a' + 10; if (c >= 'A' && c <= 'F') return c - 'A' + 10; return -1; }
static id js_parse(const char **pp, const char *e, int *bad);
static id js_string(const char **pp, const char *e, int *bad) {
    const char *p = *pp + 1;
    char *buf = malloc((u64)(e - p) + 1);
    u64 n = 0;
    while (p < e && *p != '"') {
        if (*p == '\\' && p + 1 < e) {
            p++;
            char c = *p++;
            if (c == 'n') buf[n++] = '\n';
            else if (c == 'r') buf[n++] = '\r';
            else if (c == 't') buf[n++] = '\t';
            else if (c == 'u' && p + 4 <= e) {
                int cp = 0;
                for (int i = 0; i < 4; i++) { int h = js_hex(*p++); if (h < 0) { *bad = 1; free(buf); return 0; } cp = cp * 16 + h; }
                if (cp < 0x80) buf[n++] = (char)cp;
                else if (cp < 0x800) { buf[n++] = (char)(0xC0 | cp >> 6); buf[n++] = (char)(0x80 | (cp & 63)); }
                else { buf[n++] = (char)(0xE0 | cp >> 12); buf[n++] = (char)(0x80 | (cp >> 6 & 63)); buf[n++] = (char)(0x80 | (cp & 63)); }
            } else buf[n++] = c;
        } else buf[n++] = *p++;
    }
    if (p >= e || *p != '"') { *bad = 1; free(buf); return 0; }
    *pp = p + 1;
    id s = copystr(buf, n);
    free(buf);
    return s;
}
static id js_parse(const char **pp, const char *e, int *bad) {
    const char *p = js_skip(*pp, e);
    if (p >= e) { *bad = 1; return 0; }
    if (*p == '"') { *pp = p; return js_string(pp, e, bad); }
    if (*p == '{') {
        id d = mutdict();
        p = js_skip(p + 1, e);
        if (p < e && *p == '}') { *pp = p + 1; return d; }
        for (;;) {
            *pp = p;
            id k = js_parse(pp, e, bad);
            if (*bad || !k) { rel(d); return 0; }
            p = js_skip(*pp, e);
            if (p >= e || *p != ':') { *bad = 1; rel(k); rel(d); return 0; }
            *pp = js_skip(p + 1, e);
            id v = js_parse(pp, e, bad);
            if (*bad) { rel(k); rel(d); return 0; }
            if (!v) v = MSG0(id, (id)&NSNull_c, "null");
            dset(d, k, v);
            p = js_skip(*pp, e);
            if (p < e && *p == ',') { p = js_skip(p + 1, e); continue; }
            if (p < e && *p == '}') { *pp = p + 1; return d; }
            *bad = 1; rel(d); return 0;
        }
    }
    if (*p == '[') {
        id a = mutarr();
        p = js_skip(p + 1, e);
        if (p < e && *p == ']') { *pp = p + 1; return a; }
        for (;;) {
            *pp = p;
            id v = js_parse(pp, e, bad);
            if (*bad) { rel(a); return 0; }
            if (!v) v = MSG0(id, (id)&NSNull_c, "null");
            cx_arr_add(a, v);
            rel(v);
            p = js_skip(*pp, e);
            if (p < e && *p == ',') { p = js_skip(p + 1, e); continue; }
            if (p < e && *p == ']') { *pp = p + 1; return a; }
            *bad = 1; rel(a); return 0;
        }
    }
    if (p + 4 <= e && !strncmp(p, "true", 4)) { *pp = p + 4; return num_b(YES); }
    if (p + 5 <= e && !strncmp(p, "false", 5)) { *pp = p + 5; return num_b(NO); }
    if (p + 4 <= e && !strncmp(p, "null", 4)) { *pp = p + 4; return 0; }
    if (*p == '-' || (*p >= '0' && *p <= '9')) {
        char *end = 0;
        int flt = 0;
        for (const char *q = p; q < e && *q != ',' && *q != '}' && *q != ']' && *q != ' ' && *q != '\n'; q++) if (*q == '.' || *q == 'e' || *q == 'E') flt = 1;
        if (flt) { double v = strtod(p, &end); *pp = end ? end : p + 1; return num_d(v); }
        i64 v = strtoll(p, &end, 10); *pp = end ? end : p + 1; return num_i(v);
    }
    *bad = 1;
    return 0;
}
static void js_escape(char **o, u64 *n, u64 *cap, const char *s) {
    #define JPUSH(ch) do { if (*n + 2 > *cap) { *cap = *cap * 2 + 32; *o = realloc(*o, *cap); } (*o)[(*n)++] = (ch); } while (0)
    JPUSH('"');
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (*p == '"' || *p == '\\') { JPUSH('\\'); JPUSH((char)*p); }
        else if (*p == '\n') { JPUSH('\\'); JPUSH('n'); }
        else if (*p == '\r') { JPUSH('\\'); JPUSH('r'); }
        else if (*p == '\t') { JPUSH('\\'); JPUSH('t'); }
        else if (*p < 0x20) { char b[8]; cx_snprintf(b, sizeof b, "\\u%04x", *p); for (int i = 0; b[i]; i++) JPUSH(b[i]); }
        else JPUSH((char)*p);
    }
    JPUSH('"');
    #undef JPUSH
}
static void js_write(char **o, u64 *n, u64 *cap, id v, int pretty, int depth);
static void js_pushs(char **o, u64 *n, u64 *cap, const char *s) {
    u64 l = strlen(s);
    if (*n + l + 1 > *cap) { *cap = *n + l + 64; *o = realloc(*o, *cap); }
    memcpy(*o + *n, s, l); *n += l;
}
static void js_write(char **o, u64 *n, u64 *cap, id v, int pretty, int depth) {
    if (!v || is_cls(v, &NSNull_c)) { js_pushs(o, n, cap, "null"); return; }
    if (cx_is_str(v)) { js_escape(o, n, cap, cs(v)); return; }
    if (is_cls(v, &NSNumber_c)) {
        nsnum_t *nn = (nsnum_t *)v;
        char b[64];
        if (v->isa == &NSNumber_c && nn->kind == 2) js_pushs(o, n, cap, nn->i ? "true" : "false");
        else if (v->isa == &NSNumber_c && nn->kind == 1) { cx_snprintf(b, sizeof b, "%.16g", nn->d); js_pushs(o, n, cap, b); }
        else { cx_snprintf(b, sizeof b, "%lld", (long long)ival(v)); js_pushs(o, n, cap, b); }
        return;
    }
    if (is_cls(v, &NSArray_c)) {
        js_pushs(o, n, cap, "[");
        u64 cnt = MSG0(u64, v, "count");
        for (u64 i = 0; i < cnt; i++) {
            if (i) js_pushs(o, n, cap, pretty ? ", " : ",");
            js_write(o, n, cap, MSG(id, v, "objectAtIndex:", i), pretty, depth + 1);
        }
        js_pushs(o, n, cap, "]");
        return;
    }
    if (is_cls(v, &NSDictionary_c)) {
        js_pushs(o, n, cap, "{");
        id keys = MSG0(id, v, "allKeys");
        u64 cnt = keys ? MSG0(u64, keys, "count") : 0;
        for (u64 i = 0; i < cnt; i++) {
            if (i) js_pushs(o, n, cap, pretty ? ", " : ",");
            id k = MSG(id, keys, "objectAtIndex:", i);
            js_escape(o, n, cap, cs(k));
            js_pushs(o, n, cap, ":");
            js_write(o, n, cap, MSG(id, v, "objectForKey:", k), pretty, depth + 1);
        }
        rel(keys);
        js_pushs(o, n, cap, "}");
        return;
    }
    js_escape(o, n, cap, cs(v));
}
static id json_obj(id cls, SEL c, id data, u64 opt, id *err) {
    (void)cls; (void)c;
    if (err) *err = 0;
    if (!data) { if (err) *err = ar(make_err("NSCocoaErrorDomain", 3840, "JSON vazio")); return 0; }
    const char *p = MSG0(const char *, data, "bytes");
    u64 n = MSG0(u64, data, "length");
    int bad = 0;
    const char *q = p;
    id v = js_parse(&q, p + n, &bad);
    q = js_skip(q, p + n);
    if (bad || q != p + n) { rel(v); if (err) *err = ar(make_err("NSCocoaErrorDomain", 3840, "JSON inválido")); return 0; }
    if (!v) v = retain((id)MSG0(id, (id)&NSNull_c, "null"));
    if ((opt & 1) && (is_cls(v, &NSDictionary_c) || is_cls(v, &NSArray_c))) return ar(v);
    return ar(v);
}
static id json_data(id cls, SEL c, id obj, u64 opt, id *err) {
    (void)cls; (void)c;
    if (err) *err = 0;
    if (!obj) { if (err) *err = ar(make_err("NSCocoaErrorDomain", 3840, "objeto nulo")); return 0; }
    char *o = malloc(64);
    u64 n = 0, cap = 64;
    js_write(&o, &n, &cap, obj, (opt & 1) != 0, 0);
    if (n + 1 > cap) o = realloc(o, n + 1);
    o[n] = 0;
    id d = MSG(id, (id)&NSData_c, "dataWithBytes:length:", o, n);
    free(o);
    return ar(d);
}

/* ---------------------------------------------------------------- base64 e atributos de arquivo */
static const char b64t[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static id data_b64(id self, SEL c, u64 opt) {
    (void)c; (void)opt;
    const u8 *p = MSG0(const u8 *, self, "bytes");
    u64 n = MSG0(u64, self, "length"), o = 0;
    char *b = malloc((n + 2) / 3 * 4 + 1);
    for (u64 i = 0; i < n; i += 3) {
        u32 v = (u32)p[i] << 16;
        if (i + 1 < n) v |= (u32)p[i + 1] << 8;
        if (i + 2 < n) v |= p[i + 2];
        b[o++] = b64t[(v >> 18) & 63]; b[o++] = b64t[(v >> 12) & 63];
        b[o++] = i + 1 < n ? b64t[(v >> 6) & 63] : '=';
        b[o++] = i + 2 < n ? b64t[v & 63] : '=';
    }
    b[o] = 0;
    id s = cx_str_n(b, o);
    free(b);
    return ar(s);
}
static int b64v(int c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}
static id data_from_b64(id cls, SEL c, id str, u64 opt) {
    (void)cls; (void)c; (void)opt;
    const char *s = cs(str);
    u64 n = strlen(s), o = 0;
    u8 *b = malloc(n);
    int val = 0, bits = 0;
    for (u64 i = 0; i < n; i++) {
        if (s[i] == '=' || s[i] == '\n' || s[i] == '\r') continue;
        int v = b64v((unsigned char)s[i]);
        if (v < 0) continue;
        val = (val << 6) | v; bits += 6;
        if (bits >= 8) { bits -= 8; b[o++] = (u8)((val >> bits) & 0xFF); }
    }
    id d = MSG(id, (id)&NSData_c, "dataWithBytes:length:", b, o);
    free(b);
    return d;
}
static id data_b64_new(id cls, SEL c, id str, u64 opt) { return ar(data_from_b64(cls, c, str, opt)); }
static id data_init_b64(id self, SEL c, id str, u64 opt) {
    id d = data_from_b64((id)&NSData_c, c, str, opt);
    rel(self);
    return d;
}
static id fm_attrs(id self, SEL c, id path, id *err) {
    (void)self; (void)c;
    if (err) *err = 0;
    mos_stat_t st;
    if (SYS2(SYS_STAT, cs(path), &st) < 0) { if (err) *err = ar(make_err("NSCocoaErrorDomain", 260, "arquivo não encontrado")); return 0; }
    id d = mutdict();
    dset(d, retain(NSFileSize), num_i((i64)st.size));
    dset(d, retain(NSFileType), retain(st.is_dir ? NSFileTypeDirectory : NSFileTypeRegular));
    if (st.mtime) dset(d, retain(NSFileModificationDate), MSG(id, (id)&NSDate_c, "dateWithTimeIntervalSince1970:", (double)st.mtime));
    return ar(d);
}

/* ---------------------------------------------------------------- UUID */
static id uuid_new(id cls, SEL c) {
    (void)c;
    box_t *u = (box_t *)cx_alloc_instance((cx_class_t *)cls);
    u8 *b = malloc(16);
    u64 t = (u64)SYS0(SYS_TIME) ^ (u64)u ^ (u64)SYS0(SYS_GETPID);
    for (int i = 0; i < 16; i++) { t = t * 6364136223846793005ULL + 1; b[i] = (u8)(t >> 33); }
    b[6] = (u8)((b[6] & 0x0F) | 0x40);
    b[8] = (u8)((b[8] & 0x3F) | 0x80);
    u->p = b;
    return (id)u;
}
static id uuid_str(id self, SEL c) {
    (void)c;
    static const char h[] = "0123456789ABCDEF";
    const u8 *b = BOX(self)->p;
    char s[37];
    int o = 0;
    for (int i = 0; i < 16; i++) {
        if (i == 4 || i == 6 || i == 8 || i == 10) s[o++] = '-';
        s[o++] = h[b[i] >> 4]; s[o++] = h[b[i] & 15];
    }
    s[o] = 0;
    return ar(ns(s));
}
static id uuid_desc(id self, SEL c) { return uuid_str(self, c); }
static const u8 *uuid_bytes(id self, SEL c) { (void)c; return BOX(self)->p; }

/* ---------------------------------------------------------------- travas */
static i64 tid_now(void) { return SYS0(SYS_GETTID); }
static void spin_lock(volatile int *l) { while (__sync_lock_test_and_set(l, 1)) SYS0(SYS_YIELD); }
static void spin_unlock(volatile int *l) { __sync_lock_release(l); }
static id lock_alloc(id cls, SEL c) { (void)c; return cx_alloc_instance((cx_class_t *)cls); }
static void lk_lock(id s, SEL c) { (void)c; spin_lock((volatile int *)&BOX(s)->fl); }
static void lk_unlock(id s, SEL c) { (void)c; spin_unlock((volatile int *)&BOX(s)->fl); }
static BOOL lk_try(id s, SEL c) { (void)c; return __sync_lock_test_and_set(&BOX(s)->fl, 1) ? NO : YES; }
static void lk_rec(id s, SEL c) {
    (void)c;
    i64 t = tid_now();
    if (BOX(s)->i == t && BOX(s)->j) { BOX(s)->j++; return; }
    spin_lock((volatile int *)&BOX(s)->fl);
    BOX(s)->i = t; BOX(s)->j = 1;
}
static void lk_unrec(id s, SEL c) {
    (void)c;
    if (--BOX(s)->j > 0) return;
    BOX(s)->i = 0;
    spin_unlock((volatile int *)&BOX(s)->fl);
}
static void cond_wait(id s, SEL c) {
    (void)c;
    int seq = BOX(s)->fl2;
    spin_unlock((volatile int *)&BOX(s)->fl);
    while (BOX(s)->fl2 == seq) SYS0(SYS_YIELD);
    spin_lock((volatile int *)&BOX(s)->fl);
}
static void cond_signal(id s, SEL c) { (void)c; __sync_fetch_and_add(&BOX(s)->fl2, 1); }
static void cond_bcast(id s, SEL c) { (void)c; __sync_fetch_and_add(&BOX(s)->fl2, 1); }
static id condlock_init(id s, SEL c, long cond) { (void)c; BOX(s)->j = cond; return s; }
static BOOL condlock_lockwhen(id s, SEL c, long cond) {
    (void)c;
    lk_lock(s, c);
    BOX(s)->j = cond;
    return YES;
}
static void condlock_unlockwith(id s, SEL c, long cond) { (void)c; BOX(s)->j = cond; cond_signal(s, c); lk_unlock(s, c); }
static long condlock_cond(id s, SEL c) { (void)c; return (long)BOX(s)->j; }

/* ---------------------------------------------------------------- datas */
#define REF_1970 978307200.0
static mos_tm_t *tm_of(double unix_s) { long s = (long)unix_s; return (mos_tm_t *)gmtime(&s); }
static int fmt_n(const char *p, int max) { int n = 1; while (n < max && p[n] == p[0]) n++; return n; }
static void fmt_put(char **o, u64 *n, u64 *cap, const char *s) {
    u64 l = strlen(s);
    if (*n + l + 1 > *cap) { *cap = *n + l + 32; *o = realloc(*o, *cap); }
    memcpy(*o + *n, s, l + 1); *n += l;
}
static const char *mos_meses[] = {"jan", "fev", "mar", "abr", "mai", "jun", "jul", "ago", "set", "out", "nov", "dez"};
static const char *mos_meses_l[] = {"janeiro", "fevereiro", "março", "abril", "maio", "junho", "julho", "agosto", "setembro", "outubro", "novembro", "dezembro"};
static const char *mos_dias[] = {"dom", "seg", "ter", "qua", "qui", "sex", "sáb"};
static id date_format(double unix_s, const char *pat) {
    mos_tm_t *t = tm_of(unix_s);
    if (!t) return ns("");
    if (!pat || !pat[0]) pat = "dd/MM/yyyy HH:mm:ss";
    char *o = malloc(64);
    u64 n = 0, cap = 64;
    o[0] = 0;
    for (const char *p = pat; *p;) {
        char tmp[32];
        int k;
        if (*p == '\'') {
            p++;
            while (*p && *p != '\'') { tmp[0] = *p++; tmp[1] = 0; fmt_put(&o, &n, &cap, tmp); }
            if (*p == '\'') p++;
            continue;
        }
        k = fmt_n(p, 8);
        switch (*p) {
        case 'y': cx_snprintf(tmp, sizeof tmp, k >= 4 ? "%04d" : "%02d", t->year + 1900); fmt_put(&o, &n, &cap, k <= 2 ? tmp + 2 : tmp); break;
        case 'M':
            if (k >= 4) fmt_put(&o, &n, &cap, mos_meses_l[t->mon]);
            else if (k == 3) fmt_put(&o, &n, &cap, mos_meses[t->mon]);
            else { cx_snprintf(tmp, sizeof tmp, k >= 2 ? "%02d" : "%d", t->mon + 1); fmt_put(&o, &n, &cap, tmp); }
            break;
        case 'd': cx_snprintf(tmp, sizeof tmp, k >= 2 ? "%02d" : "%d", t->mday); fmt_put(&o, &n, &cap, tmp); break;
        case 'E': fmt_put(&o, &n, &cap, mos_dias[t->wday]); break;
        case 'H': case 'h': cx_snprintf(tmp, sizeof tmp, "%02d", *p == 'h' ? (t->hour % 12 ? t->hour % 12 : 12) : t->hour); fmt_put(&o, &n, &cap, tmp); break;
        case 'm': cx_snprintf(tmp, sizeof tmp, "%02d", t->min); fmt_put(&o, &n, &cap, tmp); break;
        case 's': cx_snprintf(tmp, sizeof tmp, "%02d", t->sec); fmt_put(&o, &n, &cap, tmp); break;
        case 'a': fmt_put(&o, &n, &cap, t->hour < 12 ? "AM" : "PM"); break;
        case 'S': cx_snprintf(tmp, sizeof tmp, "%03d", (int)((unix_s - (long)unix_s) * 1000)); fmt_put(&o, &n, &cap, tmp); break;
        case 'Z': fmt_put(&o, &n, &cap, "+0000"); break;
        case 'z': case 'X': fmt_put(&o, &n, &cap, "GMT"); break;
        default: tmp[0] = *p; tmp[1] = 0; k = 1; fmt_put(&o, &n, &cap, tmp); break;
        }
        p += k;
    }
    id s = ns(o);
    free(o);
    return s;
}
/* NSDateFormatter e os outros formatters estão em fdn_formatters.c (ICU) */

/* NSScanner está em fdn_scanner.c (UTF-16, charactersToBeSkipped, caseSensitive, locale) */

/* ---------------------------------------------------------------- expressão regular (retrocesso) */
static int re_class(const char **pp, int ch, int fold) {
    const char *p = *pp;
    int neg = 0, ok = 0;
    if (*p == '^') { neg = 1; p++; }
    while (*p && *p != ']') {
        int a = (unsigned char)*p++;
        if (*p == '-' && p[1] && p[1] != ']') { int b = (unsigned char)p[1]; p += 2; if (ch >= a && ch <= b) ok = 1; }
        else if (fold && a >= 'A' && a <= 'Z' && (ch == a || ch == a + 32)) ok = 1;
        else if (ch == a) ok = 1;
    }
    if (*p == ']') p++;
    *pp = p;
    return neg ? !ok : ok;
}
static int re_atom(const char **pp, int ch, int fold) {
    const char *p = *pp;
    if (*p == '\\') {
        char e = p[1];
        *pp = p + 2;
        if (e == 'd') return ch >= '0' && ch <= '9';
        if (e == 'D') return !(ch >= '0' && ch <= '9');
        if (e == 's') return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
        if (e == 'w') return (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '_';
        if (e == 'n') return ch == '\n';
        if (e == 't') return ch == '\t';
        return ch == (unsigned char)e || (fold && ((ch ^ e) == 32));
    }
    if (*p == '.') { *pp = p + 1; return ch != '\n'; }
    if (*p == '[') { p++; int ok = re_class(&p, ch, fold); *pp = p; return ok; }
    if (*p == ')' || *p == '|' || *p == 0 || *p == '*' || *p == '+' || *p == '?') return 0;
    *pp = p + 1;
    if (fold && ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z'))) return (ch | 32) == (*p | 32);
    return ch == (unsigned char)*p;
}
static const char *re_skip_atom(const char *p) {
    if (*p == '\\') return p + 2;
    if (*p == '[') { p++; if (*p == '^') p++; while (*p && *p != ']') p++; return *p ? p + 1 : p; }
    if (*p == '(') {
        int d = 1; p++;
        while (*p && d) { if (*p == '\\') p += 2; else { if (*p == '(') d++; else if (*p == ')') d--; p++; } }
        return p;
    }
    return p + 1;
}
static int re_match(const char *pat, const char *s, const char *end, const char *start, int fold, const char **out);
static int re_quant(const char *pat, const char *rest, const char *s, const char *end, const char *start, int fold, const char **out, int min, int max) {
    const char *atom_end = re_skip_atom(pat);
    for (int n = 0; n < max; n++) {
        const char *np = pat;
        if (s >= end || !re_atom(&np, (unsigned char)*s, fold)) break;
        s++;
        if (n + 1 >= min && re_match(rest, s, end, start, fold, out)) return 1;
    }
    (void)atom_end;
    return min == 0 && re_match(rest, s, end, start, fold, out);
}
static int re_match(const char *pat, const char *s, const char *end, const char *start, int fold, const char **out) {
    for (;;) {
        if (*pat == 0 || *pat == ')') { if (out) *out = s; return 1; }
        if (*pat == '$') { if (s == end) { pat++; continue; } return 0; }
        if (*pat == '^') { if (s == start) { pat++; continue; } return 0; }
        if (*pat == '|') return re_match(pat + 1, s, end, start, fold, out);
        const char *next = re_skip_atom(pat);
        int min = 1, max = 1;
        if (*next == '*') { min = 0; max = 100000; next++; }
        else if (*next == '+') { min = 1; max = 100000; next++; }
        else if (*next == '?') { min = 0; max = 1; next++; }
        else if (*next == '{') {
            next++;
            min = 0; while (*next >= '0' && *next <= '9') min = min * 10 + *next++ - '0';
            max = min;
            if (*next == ',') { next++; max = *next == '}' ? 100000 : 0; while (*next >= '0' && *next <= '9') max = max * 10 + *next++ - '0'; }
            if (*next == '}') next++;
        }
        if (min == 1 && max == 1) {
            if (*pat == '(') {
                const char *inner = pat + 1;
                if (!strncmp(inner, "?:", 2)) inner += 2;
                if (!re_match(inner, s, end, start, fold, &s)) return 0;
                pat = next;
                continue;
            }
            if (s >= end || !re_atom(&(const char *){pat}, (unsigned char)*s, fold)) {
                /* tenta o outro lado de um | neste nível */
                const char *alt = pat;
                int d = 0;
                while (*alt) {
                    if (*alt == '\\') { alt += 2; continue; }
                    if (*alt == '(') d++;
                    else if (*alt == ')' && d) d--;
                    else if (*alt == '|' && !d) return re_match(alt + 1, s, end, start, fold, out);
                    alt++;
                }
                return 0;
            }
            const char *np = pat;
            re_atom(&np, (unsigned char)*s, fold);
            s++;
            pat = next;
            continue;
        }
        return re_quant(pat, next, s, end, start, fold, out, min, max);
    }
}
static id re_with(id cls, SEL c, id pat, u64 opt, id *err) {
    (void)c;
    const char *p = cs(pat);
    int d = 0;
    for (const char *q = p; *q; q++) { if (*q == '\\') { q++; continue; } if (*q == '(') d++; else if (*q == ')') d--; }
    if (d) { if (err) *err = ar(make_err("NSCocoaErrorDomain", 2048, "expressão regular inválida")); return 0; }
    if (err) *err = 0;
    box_t *o = (box_t *)cx_alloc_instance((cx_class_t *)cls);
    o->a = retain(pat);
    o->i = (i64)opt;
    return ar((id)o);
}
static id tcr_make(u64 loc, u64 len, int type, id extra) {
    box_t *o = (box_t *)cx_alloc_instance(&NSTextCheckingResult_c);
    o->i = (i64)loc; o->j = (i64)len; o->fl = type; o->a = retain(extra);
    return (id)o;
}
static id re_matches(id self, SEL c, id str, u64 opt, rng_t range) {
    (void)c; (void)opt;
    const char *s = cs(str);
    u64 n = strlen(s);
    if (range.location > n) range.location = n;
    if (range.location + range.length > n) range.length = n - range.location;
    id a = mutarr();
    int fold = (BOX(self)->i & 1) != 0;
    const char *pat = cs(BOX(self)->a);
    for (u64 i = range.location; i <= range.location + range.length;) {
        const char *end = 0;
        const char *from = s + i;
        if (re_match(pat, from, s + range.location + range.length, from, fold, &end) && end) {
            u64 len = (u64)(end - from);
            if (!len) { i++; continue; }
            cx_arr_add(a, tcr_make(i, len, 0x400, 0)); /* NSTextCheckingTypeRegularExpression */
            i += len;
        } else i++;
        if (i >= n) break;
    }
    return ar(a);
}
static u64 re_count(id self, SEL c, id str, u64 opt, rng_t range) {
    id a = re_matches(self, c, str, opt, range);
    u64 n = a ? MSG0(u64, a, "count") : 0;
    return n;
}
static id re_replace(id self, SEL c, id str, u64 opt, rng_t range, id templ) {
    id ms = re_matches(self, c, str, opt, range);
    u64 n = ms ? MSG0(u64, ms, "count") : 0;
    const char *s = cs(str);
    char *o = malloc(strlen(s) + n * (strlen(cs(templ)) + 8) + 1);
    u64 w = 0, prev = 0;
    for (u64 i = 0; i < n; i++) {
        id m = MSG(id, ms, "objectAtIndex:", i);
        u64 loc = (u64)BOX(m)->i, len = (u64)BOX(m)->j;
        memcpy(o + w, s + prev, loc - prev); w += loc - prev;
        const char *t = cs(templ);
        u64 tl = strlen(t);
        memcpy(o + w, t, tl); w += tl;
        prev = loc + len;
    }
    u64 sl = strlen(s);
    memcpy(o + w, s + prev, sl - prev); w += sl - prev; o[w] = 0;
    id r = cx_str_n(o, w);
    free(o);
    return ar(r);
}
static id dd_with(id cls, SEL c, u64 types, id *err) {
    (void)types;
    /* link, telefone e data: um padrão só, largo o bastante para os três */
    return re_with(cls, c, ns("https?://[^\\s]+|\\b\\d{2,4}[- .]\\d{3,5}[- .]\\d{4}\\b|\\b\\d{1,2}/\\d{1,2}/\\d{2,4}\\b"), 1, err);
}
static rng_t tcr_range(id s, SEL c) { (void)c; rng_t r = {(u64)BOX(s)->i, (u64)BOX(s)->j}; return r; }
static u64 tcr_type(id s, SEL c) { (void)c; return (u64)BOX(s)->fl; }
static id tcr_extra(id s, SEL c) { (void)c; return BOX(s)->a; }

/* ---------------------------------------------------------------- predicados */
enum { PN_TRUE = 1, PN_FALSE, PN_NOT, PN_AND, PN_OR, PN_CMP, PN_IN };
enum { PC_EQ, PC_NE, PC_LT, PC_GT, PC_LE, PC_GE, PC_CONTAINS, PC_BEGINS, PC_ENDS, PC_LIKE, PC_MATCHES };
typedef struct pnode { int kind, cmp, fold; id key, val; struct pnode *a, *b; } pnode_t;
static pnode_t *pn(int k) { pnode_t *n = calloc(1, sizeof *n); n->kind = k; return n; }
static const char *pskip(const char *p) { while (*p == ' ' || *p == '\t' || *p == '\n') p++; return p; }
static pnode_t *pparse(const char **pp, id *args, int *ai, int nargs);
static id pnext(const char **pp, id *args, int *ai, int nargs) {
    const char *p = pskip(*pp);
    if (p[0] == '%' && p[1]) {
        char k = p[1];
        *pp = p + 2;
        if (k == '@' || k == 'K') { if (*ai < nargs) return retain(args[(*ai)++]); return ns(""); }
        if (k == 'd' || k == 'i') { if (*ai < nargs) return num_i((i64)(long)args[(*ai)++]); return num_i(0); }
        if (k == 'f') { if (*ai < nargs) return num_d(*(double *)&args[(*ai)++]); return num_d(0); }
    }
    if (*p == '"' || *p == '\'') {
        char q = *p++;
        const char *s = p;
        while (*p && *p != q) p++;
        id o = copystr(s, (u64)(p - s));
        *pp = *p ? p + 1 : p;
        return o;
    }
    if ((*p >= '0' && *p <= '9') || (*p == '-' && p[1] >= '0')) {
        char *e = 0; double v = strtod(p, &e); *pp = e; return num_d(v);
    }
    if (!strncmp(p, "SELF", 4) && (p[4] == 0 || p[4] == ' ' || p[4] == ')')) { *pp = p + 4; return ns("SELF"); }
    if (!strncmp(p, "NULL", 4) || !strncmp(p, "nil", 3)) { *pp = p + (p[0] == 'N' ? 4 : 3); return 0; }
    if (!strncmp(p, "TRUE", 4)) { *pp = p + 4; return num_b(YES); }
    if (!strncmp(p, "FALSE", 5)) { *pp = p + 5; return num_b(NO); }
    const char *s = p;
    while (*p && *p != ' ' && *p != ')' && *p != '=' && *p != '!' && *p != '<' && *p != '>') p++;
    *pp = p;
    return copystr(s, (u64)(p - s));
}
static int pcmpop(const char **pp) {
    const char *p = pskip(*pp);
    int fold = 0, op = -1;
    if (!strncmp(p, "==", 2) || !strncmp(p, "=", 1)) { op = PC_EQ; p += p[1] == '=' ? 2 : 1; }
    else if (!strncmp(p, "!=", 2) || !strncmp(p, "<>", 2)) { op = PC_NE; p += 2; }
    else if (!strncmp(p, ">=", 2)) { op = PC_GE; p += 2; }
    else if (!strncmp(p, "<=", 2)) { op = PC_LE; p += 2; }
    else if (*p == '>') { op = PC_GT; p++; }
    else if (*p == '<') { op = PC_LT; p++; }
    else if (!strncasecmp(p, "CONTAINS", 8)) { op = PC_CONTAINS; p += 8; }
    else if (!strncasecmp(p, "BEGINSWITH", 10)) { op = PC_BEGINS; p += 10; }
    else if (!strncasecmp(p, "ENDSWITH", 8)) { op = PC_ENDS; p += 8; }
    else if (!strncasecmp(p, "LIKE", 4)) { op = PC_LIKE; p += 4; }
    else if (!strncasecmp(p, "MATCHES", 7)) { op = PC_MATCHES; p += 7; }
    else if (!strncasecmp(p, "IN", 2)) { op = 100; p += 2; }
    if (op >= 0 && *p == '[') {
        p++;
        while (*p && *p != ']') { if (*p == 'c' || *p == 'C' || *p == 'd' || *p == 'D') fold = 1; p++; }
        if (*p == ']') p++;
    }
    *pp = p;
    return op < 0 ? -1 : op + fold * 1000;
}
static pnode_t *pparse(const char **pp, id *args, int *ai, int nargs) {
    const char *p = pskip(*pp);
    if (!strncasecmp(p, "TRUEPREDICATE", 13)) { *pp = p + 13; return pn(PN_TRUE); }
    if (!strncasecmp(p, "FALSEPREDICATE", 14)) { *pp = p + 14; return pn(PN_FALSE); }
    if (!strncasecmp(p, "NOT", 3) && (p[3] == ' ' || p[3] == '(')) {
        p += 3; *pp = p;
        pnode_t *n = pn(PN_NOT); n->a = pparse(pp, args, ai, nargs); return n;
    }
    if (*p == '(') { p++; *pp = p; pnode_t *n = pparse(pp, args, ai, nargs); p = pskip(*pp); if (*p == ')') p++; *pp = p; return n; }
    id left = pnext(&p, args, ai, nargs);
    p = pskip(p);
    if (!strncasecmp(p, "AND", 3) || !strncasecmp(p, "OR", 2)) {
        /* "TRUE AND ..." não acontece com left já consumido; trata depois */
    }
    int op = pcmpop(&p);
    pnode_t *node;
    if (op >= 0) {
        int fold = op >= 1000; op %= 1000;
        if (op == 100) {
            node = pn(PN_IN); node->key = left; node->val = pnext(&p, args, ai, nargs); node->fold = fold;
        } else {
            node = pn(PN_CMP); node->cmp = op; node->fold = fold; node->key = left; node->val = pnext(&p, args, ai, nargs);
        }
    } else node = pn(truth(left) ? PN_TRUE : PN_FALSE);
    p = pskip(p);
    if (!strncasecmp(p, "AND", 3)) { p += 3; *pp = p; pnode_t *n = pn(PN_AND); n->a = node; n->b = pparse(pp, args, ai, nargs); return n; }
    if (!strncasecmp(p, "OR", 2)) { p += 2; *pp = p; pnode_t *n = pn(PN_OR); n->a = node; n->b = pparse(pp, args, ai, nargs); return n; }
    *pp = p;
    return node;
}
static id pval(id obj, id key) {
    if (!key || !strcmp(cs(key), "SELF")) return obj;
    return obj ? MSG(id, obj, "valueForKey:", key) : 0;
}
static int pcmp_vals(id a, id b, int op, int fold) {
    const char *as = a ? cs(a) : "", *bs = b ? cs(b) : "";
    int cmp = fold ? strcasecmp(as, bs) : strcmp(as, bs);
    if (is_cls(a, &NSNumber_c) || is_cls(b, &NSNumber_c)) {
        double d = dval(a) - dval(b);
        cmp = d < 0 ? -1 : d > 0 ? 1 : 0;
    }
    switch (op) {
    case PC_EQ: return cmp == 0;
    case PC_NE: return cmp != 0;
    case PC_LT: return cmp < 0;
    case PC_GT: return cmp > 0;
    case PC_LE: return cmp <= 0;
    case PC_GE: return cmp >= 0;
    case PC_CONTAINS: return fold ? (strcasestr(as, bs) != 0) : (strstr(as, bs) != 0);
    case PC_BEGINS: return fold ? !strncasecmp(as, bs, strlen(bs)) : !strncmp(as, bs, strlen(bs));
    case PC_ENDS: {
        u64 la = strlen(as), lb = strlen(bs);
        if (lb > la) return 0;
        return fold ? !strcasecmp(as + la - lb, bs) : !strcmp(as + la - lb, bs);
    }
    case PC_LIKE: case PC_MATCHES: return re_match(bs, as, as + strlen(as), as, fold, 0);
    default: return 0;
    }
}
static BOOL peval(pnode_t *n, id obj) {
    if (!n) return NO;
    switch (n->kind) {
    case PN_TRUE: return YES;
    case PN_FALSE: return NO;
    case PN_NOT: return !peval(n->a, obj);
    case PN_AND: return peval(n->a, obj) && peval(n->b, obj);
    case PN_OR: return peval(n->a, obj) || peval(n->b, obj);
    case PN_CMP: return pcmp_vals(pval(obj, n->key), n->val, n->cmp, n->fold);
    case PN_IN: {
        id set = n->val;
        id v = pval(obj, n->key);
        if (is_cls(set, &NSArray_c) || is_cls(set, &NSSet_c)) return MSG(BOOL, set, "containsObject:", v);
        return NO;
    }
    default: return NO;
    }
}
static id pred_make(id cls, pnode_t *n) { box_t *o = (box_t *)cx_alloc_instance((cx_class_t *)cls); o->p = n; return ar((id)o); }
static id pred_fmt(id cls, SEL c, id fmt, ...) {
    (void)c;
    id args[16];
    int n = 0;
    va_list ap;
    va_start(ap, fmt);
    /* conta os especificadores para saber quantos argumentos buscar */
    for (const char *p = cs(fmt); *p && n < 16; p++) if (*p == '%' && p[1] && p[1] != '%') {
        if (p[1] == 'f') { double d = va_arg(ap, double); memcpy(&args[n], &d, sizeof d); }
        else if (p[1] == 'd' || p[1] == 'i') args[n] = (id)(long)va_arg(ap, int);
        else args[n] = va_arg(ap, id);
        n++; p++;
    }
    va_end(ap);
    const char *p = cs(fmt);
    int ai = 0;
    return pred_make(cls, pparse(&p, args, &ai, n));
}
static id pred_value(id cls, SEL c, BOOL v) { (void)c; return pred_make(cls, pn(v ? PN_TRUE : PN_FALSE)); }
static id pred_join(id cls, int kind, id subs) {
    pnode_t *acc = 0;
    u64 n = subs ? MSG0(u64, subs, "count") : 0;
    for (u64 i = 0; i < n; i++) {
        id sp = MSG(id, subs, "objectAtIndex:", i);
        pnode_t *node = (sp && sp->isa && cx_is_subclass(sp->isa, &NSPredicate_c)) ? (pnode_t *)((box_t *)sp)->p : pn(PN_TRUE);
        if (!acc) acc = node;
        else { pnode_t *j = pn(kind); j->a = acc; j->b = node; acc = j; }
    }
    return pred_make(cls, acc ? acc : pn(PN_TRUE));
}
static id pred_and(id cls, SEL c, id subs) { (void)c; return pred_join(cls, PN_AND, subs); }
static id pred_or(id cls, SEL c, id subs) { (void)c; return pred_join(cls, PN_OR, subs); }
static id pred_not(id cls, SEL c, id p) {
    (void)c;
    pnode_t *n = pn(PN_NOT);
    n->a = (p && p->isa && cx_is_subclass(p->isa, &NSPredicate_c)) ? (pnode_t *)((box_t *)p)->p : pn(PN_TRUE);
    return pred_make(cls, n);
}
static BOOL pred_eval(id s, SEL c, id obj) { (void)c; return peval(BOX(s)->p, obj); }
static id cmp_pred(id cls, SEL c, id left, id right, u64 mod, u64 type, u64 opt) {
    (void)c; (void)mod;
    pnode_t *n = pn(PN_CMP);
    n->cmp = type <= 5 ? (int)type : PC_EQ;
    n->fold = (opt & 1) != 0;
    n->key = retain(left ? MSG0(id, left, "keyPath") : 0);
    if (!n->key) n->key = ns("SELF");
    n->val = retain(right ? MSG0(id, right, "constantValue") : 0);
    return pred_make(cls, n);
}
static id expr_key(id cls, SEL c, id key) { (void)c; box_t *o = (box_t *)cx_alloc_instance((cx_class_t *)cls); o->a = retain(key); o->fl = 1; return ar((id)o); }
static id expr_const(id cls, SEL c, id v) { (void)c; box_t *o = (box_t *)cx_alloc_instance((cx_class_t *)cls); o->b = retain(v); o->fl = 2; return ar((id)o); }
static id expr_keypath(id s, SEL c) { (void)c; return BOX(s)->a; }
static id expr_cval(id s, SEL c) { (void)c; return BOX(s)->b; }
static id expr_eval(id s, SEL c, id obj, id ctx) {
    (void)c; (void)ctx;
    if (BOX(s)->fl == 2) return BOX(s)->b;
    return pval(obj, BOX(s)->a);
}

/* ---------------------------------------------------------------- arquivo, pipe, tarefa */
static id fh_make(int fd) { box_t *o = (box_t *)cx_alloc_instance(&NSFileHandle_c); o->i = fd; o->fl = 1; return (id)o; }
static id fh_read_path(id cls, SEL c, id path) { (void)cls; (void)c; int fd = open(cs(path), 0); return fd < 0 ? 0 : ar(fh_make(fd)); }
static id fh_write_path(id cls, SEL c, id path) { (void)cls; (void)c; int fd = open(cs(path), 0x1 | 0x200 | 0x400, 0644); return fd < 0 ? 0 : ar(fh_make(fd)); }
static id fh_std(id cls, SEL c, int fd) { (void)cls; (void)c; return ar(fh_make(fd)); }
static id fh_read_all(id s, SEL c) {
    (void)c;
    u8 *b = 0; u64 n = 0, cap = 0; u8 tmp[4096];
    for (;;) {
        long k = read((int)BOX(s)->i, tmp, sizeof tmp);
        if (k <= 0) break;
        if (n + (u64)k > cap) { cap = (n + (u64)k) * 2 + 64; b = realloc(b, cap); }
        memcpy(b + n, tmp, (u64)k); n += (u64)k;
    }
    id d = MSG(id, (id)&NSData_c, "dataWithBytes:length:", b ? b : (void *)"", n);
    free(b);
    return d;
}
static id fh_read_n(id s, SEL c, u64 len) {
    (void)c;
    u8 *b = malloc(len ? len : 1);
    long k = read((int)BOX(s)->i, b, len);
    if (k < 0) k = 0;
    id d = MSG(id, (id)&NSData_c, "dataWithBytes:length:", b, (u64)k);
    free(b);
    return d;
}
static id fh_avail(id s, SEL c) { return fh_read_n(s, c, 65536); }
static void fh_write(id s, SEL c, id data) {
    (void)c;
    const void *b = MSG0(const void *, data, "bytes");
    u64 n = MSG0(u64, data, "length"), o = 0;
    while (o < n) { long k = write((int)BOX(s)->i, (const u8 *)b + o, n - o); if (k <= 0) break; o += (u64)k; }
}
static void fh_close(id s, SEL c) { (void)c; if (BOX(s)->fl && BOX(s)->i > 2) close((int)BOX(s)->i); BOX(s)->fl = 0; }
static u64 fh_seek_end(id s, SEL c) { (void)c; return (u64)lseek((int)BOX(s)->i, 0, 2); }
static u64 fh_offset(id s, SEL c) { (void)c; return (u64)lseek((int)BOX(s)->i, 0, 1); }
static void fh_seek(id s, SEL c, u64 off) { (void)c; lseek((int)BOX(s)->i, (off_t)off, 0); }
static int fh_fd(id s, SEL c) { (void)c; return (int)BOX(s)->i; }

static id pipe_new(id cls, SEL c) {
    (void)c;
    int fds[2] = {-1, -1};
    pipe(fds);
    box_t *o = (box_t *)cx_alloc_instance((cx_class_t *)cls);
    o->a = fh_make(fds[0]);
    o->b = fh_make(fds[1]);
    return ar((id)o);
}
static id pipe_rd(id s, SEL c) { (void)c; return BOX(s)->a; }
static id pipe_wr(id s, SEL c) { (void)c; return BOX(s)->b; }

static id task_alloc(id cls, SEL c) { (void)c; box_t *o = (box_t *)cx_alloc_instance((cx_class_t *)cls); o->i = -1; return (id)o; }
static void task_set_path(id s, SEL c, id p) { (void)c; rel(BOX(s)->a); BOX(s)->a = retain(p); }
static id task_path(id s, SEL c) { (void)c; return BOX(s)->a; }
static void task_set_args(id s, SEL c, id a) { (void)c; rel(BOX(s)->b); BOX(s)->b = retain(a); }
static id task_args(id s, SEL c) { (void)c; return BOX(s)->b; }
static void task_set_out(id s, SEL c, id h) { (void)c; rel(BOX(s)->c); BOX(s)->c = retain(h); }
static void task_set_in(id s, SEL c, id h) { (void)c; rel(BOX(s)->e); BOX(s)->e = retain(h); }
static id task_launch_err(id s, SEL c, id *err) {
    (void)c;
    if (err) *err = 0;
    const char *path = cs(BOX(s)->a);
    char args[2048] = "";
    id arr = BOX(s)->b;
    u64 n = arr ? MSG0(u64, arr, "count") : 0;
    for (u64 i = 0; i < n; i++) {
        strlcat(args, i ? " \"" : "\"", sizeof args);
        strlcat(args, cs(MSG(id, arr, "objectAtIndex:", i)), sizeof args);
        strlcat(args, "\"", sizeof args);
    }
    int io[3] = {-1, -1, -1};
    if (BOX(s)->e && is_cls(BOX(s)->e, &NSFileHandle_c)) io[0] = (int)BOX(BOX(s)->e)->i;
    if (BOX(s)->c && is_cls(BOX(s)->c, &NSFileHandle_c)) io[1] = (int)BOX(BOX(s)->c)->i;
    if (BOX(s)->c && is_cls(BOX(s)->c, &NSPipe_c)) io[1] = (int)BOX(pipe_wr(BOX(s)->c, c))->i;
    long pid = SYS3(SYS_SPAWNIO, path, args, io);
    if (pid < 0) { if (err) *err = ar(make_err("NSPOSIXErrorDomain", 2, "não foi possível lançar")); return 0; }
    BOX(s)->i = pid;
    BOX(s)->fl = 1;
    return s;
}
static void task_launch(id s, SEL c) { task_launch_err(s, c, 0); }
static void task_wait(id s, SEL c) { (void)c; if (BOX(s)->i > 0) { long st = SYS1(SYS_WAIT, BOX(s)->i); BOX(s)->j = st & 0xFF; BOX(s)->fl = 0; } }
static int task_status(id s, SEL c) { (void)c; return (int)BOX(s)->j; }
static BOOL task_running(id s, SEL c) { (void)c; return BOX(s)->fl ? YES : NO; }
static void task_term(id s, SEL c) { (void)c; if (BOX(s)->i > 0) SYS2(SYS_KILL, BOX(s)->i, 15); BOX(s)->fl = 0; }

/* ---------------------------------------------------------------- URL e sessão */
static int url_split(const char *s, char *scheme, char *host, int *port, const char **path) {
    const char *p = strstr(s, "://");
    if (!p) return 0;
    u64 sl = (u64)(p - s); if (sl > 15) sl = 15;
    memcpy(scheme, s, sl); scheme[sl] = 0;
    p += 3;
    const char *h = p, *slash = strchr(p, '/'), *colon = 0;
    const char *end = slash ? slash : p + strlen(p);
    for (const char *q = h; q < end; q++) if (*q == ':') colon = q;
    u64 hl = (u64)((colon ? colon : end) - h); if (hl > 255) hl = 255;
    memcpy(host, h, hl); host[hl] = 0;
    *port = colon ? atoi(colon + 1) : (!strcmp(scheme, "https") ? 443 : 80);
    *path = slash ? slash : "/";
    return 1;
}
static void req_ext_copy(id dst, id src);
static void resp_parse_ct(id s, id ct);
static id hdr_value(id dict, id field);
static id req_with(id cls, SEL c, id url) { (void)c; box_t *o = (box_t *)cx_alloc_instance((cx_class_t *)cls); o->a = retain(url); o->b = ns("GET"); o->c = mutdict(); return ar((id)o); }
static id req_url(id s, SEL c) { (void)c; return BOX(s)->a; }
static id req_method(id s, SEL c) { (void)c; return BOX(s)->b; }
static void req_set_method(id s, SEL c, id m) { (void)c; rel(BOX(s)->b); BOX(s)->b = retain(m); }
static urlreq_opts_t req_opts(id s) {
    urlreq_opts_t o;
    o.timeout = BOX(s)->d;
    o.policy = (unsigned long)BOX(s)->fl;
    return o;
}
static void req_put_opts(id s, urlreq_opts_t o) {
    BOX(s)->d = o.timeout;
    BOX(s)->fl = (int)o.policy;
}
static void req_set_policy(id s, SEL c, u64 p) {
    urlreq_opts_t o;
    (void)c;
    o = req_opts(s);
    urlreq_set_policy(&o, (unsigned long)p);
    req_put_opts(s, o);
}
static u64 req_policy(id s, SEL c) {
    urlreq_opts_t o = req_opts(s);
    (void)c;
    return urlreq_policy(&o);
}
static id req_headers(id s, SEL c) { (void)c; return BOX(s)->c; }
static void req_set_header(id s, SEL c, id v, id f) { (void)c; if (BOX(s)->c && v && f) MSG(void, BOX(s)->c, "setObject:forKey:", v, f); }
/* inicializadores de instância (antes o initWithURL: reaproveitava o req_with, que trata o primeiro argumento como a
 * classe: com uma instância ali, o objeto saía malformado), as variantes com política e prazo, e cópias que retêm os
 * campos (o Sparkle copia a requisição antes de baixar; sem isto ela dividia URL e corpo com a original) */
static id req_init_url(id s, SEL c, id url) {
    (void)c;
    rel(BOX(s)->a); BOX(s)->a = retain(url);
    if (!BOX(s)->b) BOX(s)->b = ns("GET");
    if (!BOX(s)->c) BOX(s)->c = mutdict();
    BOX(s)->d = 60;
    return s;
}
static id req_init_url_pt(id s, SEL c, id url, u64 policy, double timeout) {
    req_init_url(s, c, url);
    BOX(s)->d = timeout > 0 ? timeout : 60;
    BOX(s)->fl = (int)policy;
    return s;
}
static id req_with_pt(id cls, SEL c, id url, u64 policy, double timeout) {
    id r = req_with(cls, c, url);
    BOX(r)->d = timeout > 0 ? timeout : 60;
    BOX(r)->fl = (int)policy;
    return r;
}
static double req_timeout(id s, SEL c) {
    urlreq_opts_t o;
    (void)c;
    o = req_opts(s);
    return urlreq_timeout(&o);
}
static void req_set_timeout(id s, SEL c, double t) {
    urlreq_opts_t o;
    (void)c;
    o = req_opts(s);
    urlreq_set_timeout(&o, t);
    req_put_opts(s, o);
}
static id req_copy_as(id s, cx_class_t *cls) {
    box_t *o = (box_t *)cx_alloc_instance(cls);
    o->a = retain(BOX(s)->a);
    o->b = retain(BOX(s)->b);
    o->c = BOX(s)->c ? MSG0(id, BOX(s)->c, "mutableCopy") : mutdict();
    o->e = retain(BOX(s)->e);
    o->d = BOX(s)->d;
    o->fl = BOX(s)->fl;
    req_ext_copy((id)o, s);
    return (id)o;
}
static id req_copy(id s, SEL c) { (void)c; return req_copy_as(s, (cx_class_t *)object_getClass(s)); }
static id req_copy_zone(id s, SEL c, void *z) { (void)z; return req_copy(s, c); }
static id req_mcopy(id s, SEL c) { (void)c; return req_copy_as(s, &NSMutableURLRequest_c); }
static id req_mcopy_zone(id s, SEL c, void *z) { (void)z; return req_mcopy(s, c); }
static id req_body(id s, SEL c) { (void)c; return BOX(s)->e; }
static void req_set_body(id s, SEL c, id d) { (void)c; rel(BOX(s)->e); BOX(s)->e = retain(d); }
static id resp_url(id s, SEL c) { (void)c; return BOX(s)->a; }
static long resp_status(id s, SEL c) { (void)c; return (long)BOX(s)->i; }
static id resp_headers(id s, SEL c) { (void)c; return BOX(s)->b; }
static long long resp_len(id s, SEL c) { (void)c; return BOX(s)->j; }

/* ---- NSURLRequest: o resto das propriedades (box: a URL, b método, c cabeçalhos, e corpo, d prazo, fl política,
 * p extensão abaixo) e os cabeçalhos sem diferença de caixa, como no macOS */
typedef struct {
    int cookies, cellular, expensive, constrained, pipelining, http3, dnssec;
    u64 service, attribution;
    id stream, main_doc;
} req_ext_t;
static void box_dealloc(id s, SEL c);
static req_ext_t *rx(id s) {
    req_ext_t *x = (req_ext_t *)BOX(s)->p;
    if (!x) {
        x = calloc(1, sizeof *x);
        if (!x) return 0;
        x->cookies = x->cellular = x->expensive = x->constrained = 1;
        BOX(s)->p = x;
    }
    return x;
}
/* chave já presente no dicionário com outra caixa (nil se não há) */
static id hdr_key(id dict, id field) {
    if (!dict || !field) return 0;
    id keys = MSG0(id, dict, "allKeys");
    u64 n = keys ? MSG0(u64, keys, "count") : 0;
    const char *f = cs(field);
    for (u64 i = 0; i < n; i++) {
        id k = MSG(id, keys, "objectAtIndex:", i);
        if (k && !strcasecmp(cs(k), f)) return k;
    }
    return 0;
}
static id hdr_value(id dict, id field) { id k = hdr_key(dict, field); return k ? MSG(id, dict, "objectForKey:", k) : 0; }
static id req_value_for(id s, SEL c, id field) { (void)c; return hdr_value(BOX(s)->c, field); }
static void req_set_value(id s, SEL c, id v, id field) {
    (void)c;
    if (!field) return;
    if (!BOX(s)->c) BOX(s)->c = mutdict();
    id old = hdr_key(BOX(s)->c, field);
    if (old) MSG(void, BOX(s)->c, "removeObjectForKey:", old);
    if (v) MSG(void, BOX(s)->c, "setObject:forKey:", v, field);
}
static void req_add_value(id s, SEL c, id v, id field) {
    if (!v || !field) return;
    id cur = hdr_value(BOX(s)->c, field);
    if (!cur) { req_set_value(s, c, v, field); return; }
    char *j = 0;
    const char *a = cs(cur), *b = cs(v);
    size_t n = strlen(a) + strlen(b) + 3;
    j = malloc(n);
    if (!j) return;
    cx_snprintf(j, n, "%s,%s", a, b);
    id joined = ns(j);
    free(j);
    req_set_value(s, c, joined, hdr_key(BOX(s)->c, field));
}
static void req_set_all_headers(id s, SEL c, id d) {
    (void)c;
    rel(BOX(s)->c);
    BOX(s)->c = mutdict();
    id keys = d ? MSG0(id, d, "allKeys") : 0;
    u64 n = keys ? MSG0(u64, keys, "count") : 0;
    for (u64 i = 0; i < n; i++) {
        id k = MSG(id, keys, "objectAtIndex:", i);
        req_set_value(s, c, MSG(id, d, "objectForKey:", k), k);
    }
}
#define RXB(get, set, f) \
    static BOOL req_g_##get(id s, SEL c) { (void)c; req_ext_t *x = rx(s); return x ? (BOOL)x->f : NO; } \
    static void req_s_##get(id s, SEL c, BOOL v) { (void)c; req_ext_t *x = rx(s); if (x) x->f = v ? 1 : 0; }
RXB(cookies, setHTTPShouldHandleCookies, cookies) RXB(cellular, setAllowsCellularAccess, cellular)
RXB(expensive, setAllowsExpensiveNetworkAccess, expensive) RXB(constrained, setAllowsConstrainedNetworkAccess, constrained)
RXB(pipelining, setHTTPShouldUsePipelining, pipelining) RXB(http3, setAssumesHTTP3Capable, http3)
RXB(dnssec, setRequiresDNSSECValidation, dnssec)
#undef RXB
static u64 req_service(id s, SEL c) { (void)c; req_ext_t *x = rx(s); return x ? x->service : 0; }
static void req_set_service(id s, SEL c, u64 v) { (void)c; req_ext_t *x = rx(s); if (x) x->service = v; }
static u64 req_attribution(id s, SEL c) { (void)c; req_ext_t *x = rx(s); return x ? x->attribution : 0; }
static void req_set_attribution(id s, SEL c, u64 v) { (void)c; req_ext_t *x = rx(s); if (x) x->attribution = v; }
static id req_stream(id s, SEL c) { (void)c; req_ext_t *x = rx(s); return x ? x->stream : 0; }
static void req_set_stream(id s, SEL c, id v) {
    (void)c;
    req_ext_t *x = rx(s);
    if (!x) return;
    id o = x->stream; x->stream = retain(v); rel(o);
    /* corpo e stream se excluem, como no macOS */
    if (v) { rel(BOX(s)->e); BOX(s)->e = 0; }
}
static id req_main_doc(id s, SEL c) { (void)c; req_ext_t *x = rx(s); return x ? x->main_doc : 0; }
static void req_set_main_doc(id s, SEL c, id v) { (void)c; req_ext_t *x = rx(s); if (!x) return; id o = x->main_doc; x->main_doc = retain(v); rel(o); }
static void req_ext_copy(id dst, id src) {
    req_ext_t *a = (req_ext_t *)BOX(src)->p;
    if (!a) return;
    req_ext_t *b = rx(dst);
    if (!b) return;
    *b = *a;
    b->stream = retain(a->stream);
    b->main_doc = retain(a->main_doc);
}
static void req_dealloc(id s, SEL c) {
    req_ext_t *x = (req_ext_t *)BOX(s)->p;
    if (x) { rel(x->stream); rel(x->main_doc); free(x); BOX(s)->p = 0; }
    box_dealloc(s, c);
}
static BOOL req_equal(id s, SEL c, id o) {
    (void)c;
    if (s == o) return YES;
    if (!o || !cx_is_subclass(object_getClass(o), &NSURLRequest_c)) return NO;
    if (!MSG(BOOL, BOX(s)->a, "isEqual:", BOX(o)->a)) return NO;
    if (strcmp(cs(BOX(s)->b), cs(BOX(o)->b))) return NO;
    id ha = BOX(s)->c, hb = BOX(o)->c;
    if ((ha ? MSG0(u64, ha, "count") : 0) != (hb ? MSG0(u64, hb, "count") : 0)) return NO;
    if (ha && hb && !MSG(BOOL, ha, "isEqualToDictionary:", hb)) return NO;
    if ((BOX(s)->e != 0) != (BOX(o)->e != 0) || (BOX(s)->e && !MSG(BOOL, BOX(s)->e, "isEqual:", BOX(o)->e))) return NO;
    return BOX(s)->d == BOX(o)->d && BOX(s)->fl == BOX(o)->fl;
}
static u64 req_hash(id s, SEL c) { (void)c; return BOX(s)->a ? MSG0(u64, BOX(s)->a, "hash") : 0; }
static BOOL req_secure(id k, SEL c) { (void)k; (void)c; return YES; }

/* ---- NSURLResponse / NSHTTPURLResponse (box: a URL, b cabeçalhos, c MIME, e codificação, i status, j tamanho,
 * p versão HTTP) */
static void resp_parse_ct(id s, id ct) {
    char mime[128] = "", enc[64] = "";
    const char *v = ct ? cs(ct) : "";
    int k = 0;
    while (*v == ' ') v++;
    while (*v && *v != ';' && *v != ' ' && k < 127) { char ch = *v++; mime[k++] = (char)(ch >= 'A' && ch <= 'Z' ? ch + 32 : ch); }
    mime[k] = 0;
    const char *cp = strstr(v, "charset=");
    if (!cp) cp = strstr(v, "Charset=");
    if (!cp) cp = strstr(v, "CHARSET=");
    if (cp) {
        cp += 8;
        if (*cp == '"') cp++;
        k = 0;
        while (*cp && *cp != ';' && *cp != '"' && *cp != ' ' && k < 63) { char ch = *cp++; enc[k++] = (char)(ch >= 'A' && ch <= 'Z' ? ch + 32 : ch); }
        enc[k] = 0;
    }
    rel(BOX(s)->c); rel(BOX(s)->e);
    BOX(s)->c = mime[0] ? retain(ns(mime)) : 0;
    BOX(s)->e = enc[0] ? retain(ns(enc)) : 0;
}
static id hresp_init(id s, SEL c, id url, i64 status, id version, id fields) {
    (void)c;
    rel(BOX(s)->a); BOX(s)->a = retain(url);
    BOX(s)->i = status;
    id d = mutdict();
    id keys = fields ? MSG0(id, fields, "allKeys") : 0;
    u64 n = keys ? MSG0(u64, keys, "count") : 0;
    for (u64 i = 0; i < n; i++) {
        id k = MSG(id, keys, "objectAtIndex:", i);
        id v = MSG(id, fields, "objectForKey:", k);
        MSG(void, d, "setObject:forKey:", v, k);
    }
    rel(BOX(s)->b); BOX(s)->b = d;
    if (BOX(s)->p) rel((id)BOX(s)->p);
    BOX(s)->p = retain(version ? version : ns("HTTP/1.1"));
    id cl = hdr_value(d, ns("Content-Length"));
    BOX(s)->j = cl ? (i64)strtoll(cs(cl), 0, 10) : -1;
    resp_parse_ct(s, hdr_value(d, ns("Content-Type")));
    return s;
}
static id resp_init_mime(id s, SEL c, id url, id mime, i64 len, id enc) {
    (void)c;
    rel(BOX(s)->a); BOX(s)->a = retain(url);
    rel(BOX(s)->c); BOX(s)->c = retain(mime);
    rel(BOX(s)->e); BOX(s)->e = retain(enc);
    BOX(s)->j = len;
    return s;
}
static id hresp_value_for(id s, SEL c, id f) { (void)c; return hdr_value(BOX(s)->b, f); }
static id resp_suggested(id s, SEL c) {
    (void)c;
    id cd = BOX(s)->b ? hdr_value(BOX(s)->b, ns("Content-Disposition")) : 0;
    if (cd) {
        const char *v = cs(cd), *f = strstr(v, "filename=");
        if (f) {
            char name[256];
            int k = 0;
            f += 9;
            int q = *f == '"';
            if (q) f++;
            while (*f && k < 255 && (q ? *f != '"' : (*f != ';' && *f != ' '))) name[k++] = *f++;
            name[k] = 0;
            const char *slash = strrchr(name, '/');
            if (name[0]) return ns(slash ? slash + 1 : name);
        }
    }
    id last = BOX(s)->a ? MSG0(id, BOX(s)->a, "lastPathComponent") : 0;
    if (last && cs(last)[0] && strcmp(cs(last), "/")) return last;
    return ns("Unknown");
}
static id hresp_s_status_string(id k, SEL c, i64 code) {
    (void)k; (void)c;
    static const struct { int code; const char *s; } t[] = {
        {100, "continue"}, {101, "switching protocols"}, {200, "no error"}, {201, "created"}, {202, "accepted"},
        {203, "non-authoritative information"}, {204, "no content"}, {205, "reset content"}, {206, "partial content"},
        {300, "multiple choices"}, {301, "moved permanently"}, {302, "found"}, {303, "see other"}, {304, "not modified"},
        {305, "needs proxy"}, {307, "temporarily redirected"}, {308, "permanently redirected"}, {400, "bad request"},
        {401, "unauthorized"}, {402, "payment required"}, {403, "forbidden"}, {404, "not found"},
        {405, "method not allowed"}, {406, "unacceptable"}, {407, "proxy authentication required"},
        {408, "request timed out"}, {409, "conflict"}, {410, "no longer exists"}, {411, "length required"},
        {412, "precondition failed"}, {413, "request too large"}, {414, "requested URL too long"},
        {415, "unsupported media type"}, {416, "requested range not satisfiable"}, {417, "expectation failed"},
        {422, "unprocessable entity"}, {429, "too many requests"}, {500, "internal server error"},
        {501, "unimplemented"}, {502, "bad gateway"}, {503, "service unavailable"}, {504, "gateway timed out"},
        {505, "unsupported version"},
    };
    for (size_t i = 0; i < sizeof t / sizeof t[0]; i++) if (t[i].code == code) return ns(t[i].s);
    return ns(code < 200 ? "informational" : code < 300 ? "success" : code < 400 ? "redirected" : code < 500 ? "client error" : "server error");
}
int pthread_create(unsigned long *t, const void *attr, void *(*fn)(void *), void *arg);   /* NSOperationQueue */
static id resp_copy(id s, SEL c) { (void)c; return retain(s); }
static id resp_copy_zone(id s, SEL c, void *z) { (void)z; return resp_copy(s, c); }
static void resp_dealloc(id s, SEL c) { if (BOX(s)->p) { rel((id)BOX(s)->p); BOX(s)->p = 0; } box_dealloc(s, c); }


static id comp_make(id cls, SEL c, id s) {
    (void)c;
    char scheme[16], host[256]; int port = 0; const char *path = "/";
    if (!url_split(cs(s), scheme, host, &port, &path)) { scheme[0] = 0; strlcpy(host, "", 2); }
    box_t *o = (box_t *)cx_alloc_instance((cx_class_t *)cls);
    o->a = ns(scheme); o->b = ns(host); o->c = ns(path); o->i = port; o->e = retain(s);
    return ar((id)o);
}
static id comp_scheme(id s, SEL c) { (void)c; return BOX(s)->a; }
static id comp_host(id s, SEL c) { (void)c; return BOX(s)->b; }
static id comp_path(id s, SEL c) { (void)c; return BOX(s)->c; }
static long comp_port(id s, SEL c) { (void)c; return (long)BOX(s)->i; }
static id comp_url(id s, SEL c) { (void)c; return BOX(s)->e ? MSG(id, (id)cx_class_named("NSURL"), "URLWithString:", BOX(s)->e) : 0; }
/* queryItems / setQueryItems: a consulta (depois do '?', antes do '#') da URL guardada em e, com % decodificado nos
 * itens lidos e codificado nos gravados (o Sparkle monta a URL do appcast com eles) */
static id qitem_make(id cls, SEL c, id n, id v);
static id comp_query_items(id s, SEL c) {
    const char *u = BOX(s)->e ? cs(BOX(s)->e) : 0;
    const char *q = u ? strchr(u, '?') : 0;
    if (!q) return 0;
    q++;
    const char *end = strchr(q, '#');
    u64 qn = end ? (u64)(end - q) : strlen(q);
    id arr = MSG0(id, (id)cx_class_named("NSMutableArray"), "array");
    for (u64 i = 0; i <= qn;) {
        u64 j = i;
        while (j < qn && q[j] != '&') j++;
        if (j > i) {
            u64 eq = i;
            while (eq < j && q[eq] != '=') eq++;
            id name = MSG0(id, cx_str_n(q + i, eq - i), "stringByRemovingPercentEncoding");
            id val = eq < j ? MSG0(id, cx_str_n(q + eq + 1, j - eq - 1), "stringByRemovingPercentEncoding") : 0;
            MSG(void, arr, "addObject:", qitem_make((id)&NSURLQueryItem_c, c, name ? name : cx_str_n(q + i, eq - i), val));
        }
        i = j + 1;
    }
    return arr;
}
static void comp_set_query_items(id s, SEL c, id items) {
    (void)c;
    const char *u = BOX(s)->e ? cs(BOX(s)->e) : "";
    const char *q = strchr(u, '?'), *h = strchr(u, '#');
    u64 base = q ? (u64)(q - u) : h ? (u64)(h - u) : strlen(u);
    id out = MSG(id, (id)cx_class_named("NSMutableString"), "stringWithString:", cx_str_n(u, base));
    id allowed = MSG0(id, (id)cx_class_named("NSCharacterSet"), "URLQueryAllowedCharacterSet");
    u64 n = items ? MSG0(u64, items, "count") : 0;
    for (u64 i = 0; i < n; i++) {
        id it = MSG(id, items, "objectAtIndex:", i);
        id nm = MSG0(id, it, "name"), v = MSG0(id, it, "value");
        MSG(void, out, "appendString:", cx_str(i ? "&" : "?"));
        id en = allowed ? MSG(id, nm, "stringByAddingPercentEncodingWithAllowedCharacters:", allowed) : nm;
        MSG(void, out, "appendString:", en ? en : nm);
        if (v) {
            id ev = allowed ? MSG(id, v, "stringByAddingPercentEncodingWithAllowedCharacters:", allowed) : v;
            MSG(void, out, "appendString:", cx_str("="));
            MSG(void, out, "appendString:", ev ? ev : v);
        }
    }
    if (h) MSG(void, out, "appendString:", cx_str(h));
    id old = BOX(s)->e;
    BOX(s)->e = retain(MSG0(id, out, "copy"));
    if (old) objc_release(old);
}
static id qitem_make(id cls, SEL c, id n, id v) { (void)c; box_t *o = (box_t *)cx_alloc_instance((cx_class_t *)cls); o->a = retain(n); o->b = retain(v); return ar((id)o); }
static id qitem_name(id s, SEL c) { (void)c; return BOX(s)->a; }
static id qitem_value(id s, SEL c) { (void)c; return BOX(s)->b; }


/* ---------------------------------------------------------------- arquivo coordenado, operações, progresso, undo */
static void coord_read(id s, SEL c, id url, u64 opt, id *err, void *block) {
    (void)s; (void)c; (void)opt;
    if (err) *err = 0;
    if (!block) return;
    void (*inv)(void *, id) = *(void **)((u8 *)block + 16);
    inv(block, url);
}
/* NSOperation, NSBlockOperation, NSInvocationOperation e NSOperationQueue estão em fdn_operation.c (libdispatch) */
static void prog_set_total(id s, SEL c, long long n) { (void)c; BOX(s)->j = n; }
static void prog_set_done(id s, SEL c, long long n) { (void)c; BOX(s)->i = n; }
static long long prog_total(id s, SEL c) { (void)c; return BOX(s)->j; }
static long long prog_done(id s, SEL c) { (void)c; return BOX(s)->i; }
static double prog_frac(id s, SEL c) { (void)c; return BOX(s)->j ? (double)BOX(s)->i / (double)BOX(s)->j : 0; }
static id prog_with(id cls, SEL c, long long n) { box_t *o = (box_t *)cx_alloc_instance((cx_class_t *)cls); o->j = n; (void)c; return ar((id)o); }
static void prog_cancel(id s, SEL c) { (void)c; BOX(s)->fl = 1; }
static BOOL prog_cancelled(id s, SEL c) { (void)c; return BOX(s)->fl ? YES : NO; }

typedef struct undo_item { id target; char sel[128]; id arg; struct undo_item *next; } undo_item_t;
/* NSUndoManager está em fdn_undo.c */

/* ---------------------------------------------------------------- keyed archiver */
static int is_uid(id o) { return is_cls(o, &NSDictionary_c) && MSG0(u64, o, "count") == 1 && dget(o, "CF$UID"); }
static i64 uid_of(id o) { id n = dget(o, "CF$UID"); return n ? ival(n) : 0; }
static id uid_obj(i64 i) { id d = mutdict(); dset(d, ns("CF$UID"), num_i(i)); return d; }

static id arch_alloc(id cls, SEL c) {
    (void)c;
    box_t *o = (box_t *)cx_alloc_instance((cx_class_t *)cls);
    o->a = mutarr();
    cx_arr_add(o->a, ns("$null"));
    o->b = mutdict();
    return (id)o;
}
static i64 arch_intern(id arch, id obj);
static void arch_encode(id arch, id obj) {
    if (!obj || is_cls(obj, &NSString_c) || is_cls(obj, &NSNumber_c) || is_cls(obj, &NSData_c) || is_cls(obj, &NSNull_c)) return;
    if (is_cls(obj, &NSDate_c)) return;
    if (is_cls(obj, &NSArray_c)) {
        u64 n = MSG0(u64, obj, "count");
        for (u64 i = 0; i < n; i++) arch_intern(arch, MSG(id, obj, "objectAtIndex:", i));
        return;
    }
    if (is_cls(obj, &NSDictionary_c)) {
        id keys = MSG0(id, obj, "allKeys");
        u64 n = keys ? MSG0(u64, keys, "count") : 0;
        for (u64 i = 0; i < n; i++) {
            id k = MSG(id, keys, "objectAtIndex:", i);
            arch_intern(arch, k);
            arch_intern(arch, MSG(id, obj, "objectForKey:", k));
        }
        rel(keys);
    }
}
static i64 arch_intern(id arch, id obj) {
    if (!obj) return 0;
    id arr = BOX(arch)->a;
    u64 n = MSG0(u64, arr, "count");
    for (u64 i = 0; i < n; i++) if (MSG(id, arr, "objectAtIndex:", i) == obj) return (i64)i;
    if (is_cls(obj, &NSDate_c)) {
        id d = mutdict();
        dset(d, ns("$classname"), ns("NSDate"));
        dset(d, ns("NS.time"), num_d(MSG0(double, obj, "timeIntervalSinceReferenceDate")));
        obj = d;
    }
    cx_arr_add(arr, obj);
    i64 idx = (i64)MSG0(u64, arr, "count") - 1;
    arch_encode(arch, obj);
    if (MSG(BOOL, obj, "respondsToSelector:", sel_registerName("encodeWithCoder:"))) MSG(void, obj, "encodeWithCoder:", arch);
    return idx;
}
static id arch_data(id cls, SEL c, id root, BOOL secure, id *err) {
    (void)secure;
    if (err) *err = 0;
    id arch = arch_alloc(cls, c);
    i64 root_i = arch_intern(arch, root);
    id top = mutdict();
    dset(top, ns("root"), uid_obj(root_i));
    id out = mutdict();
    dset(out, ns("$version"), num_i(100000));
    dset(out, ns("$archiver"), ns("NSKeyedArchiver"));
    dset(out, retain(NSKeyedArchiveRootObjectKey), retain(top));
    /* $top e $objects, que é o que o desarquivador lê */
    dset(out, ns("$top"), retain(top));
    dset(out, ns("$objects"), retain(BOX(arch)->a));
    rel(top);
    return out; /* o chamador serializa; arch_finish devolve NSData */
}
static void xml_escape(char **o, u64 *n, u64 *cap, const char *s) {
    for (const char *p = s ? s : ""; *p; p++) {
        const char *r = *p == '<' ? "&lt;" : *p == '>' ? "&gt;" : *p == '&' ? "&amp;" : 0;
        const char *w = r ? r : p;
        u64 l = r ? strlen(r) : 1;
        if (*n + l + 1 > *cap) { *cap = *n + l + 64; *o = realloc(*o, *cap); }
        memcpy(*o + *n, w, l); *n += l;
        if (!r) continue;
    }
}
static void xml_obj(char **o, u64 *n, u64 *cap, id v);
static void xml_add(char **o, u64 *n, u64 *cap, const char *s) {
    u64 l = strlen(s);
    if (*n + l + 1 > *cap) { *cap = *n + l + 64; *o = realloc(*o, *cap); }
    memcpy(*o + *n, s, l); *n += l;
}
static void xml_obj(char **o, u64 *n, u64 *cap, id v) {
    if (!v || is_cls(v, &NSNull_c)) { xml_add(o, n, cap, "<null/>"); return; }
    if (is_uid(v)) { char b[64]; cx_snprintf(b, sizeof b, "<dict><key>CF$UID</key><integer>%lld</integer></dict>", (long long)uid_of(v)); xml_add(o, n, cap, b); return; }
    if (cx_is_str(v)) { xml_add(o, n, cap, "<string>"); xml_escape(o, n, cap, cs(v)); xml_add(o, n, cap, "</string>"); return; }
    if (is_cls(v, &NSNumber_c)) {
        nsnum_t *nn = (nsnum_t *)v;
        char b[80];
        if (v->isa == &NSNumber_c && nn->kind == 2) xml_add(o, n, cap, nn->i ? "<true/>" : "<false/>");
        else if (v->isa == &NSNumber_c && nn->kind == 1) { cx_snprintf(b, sizeof b, "<real>%g</real>", nn->d); xml_add(o, n, cap, b); }
        else { cx_snprintf(b, sizeof b, "<integer>%lld</integer>", (long long)ival(v)); xml_add(o, n, cap, b); }
        return;
    }
    if (is_cls(v, &NSData_c)) { id s = data_b64(v, 0, 0); xml_add(o, n, cap, "<data>"); xml_add(o, n, cap, cs(s)); xml_add(o, n, cap, "</data>"); return; }
    if (is_cls(v, &NSDate_c)) { char b[80]; cx_snprintf(b, sizeof b, "<date>%s</date>", cs(date_format(MSG0(double, v, "timeIntervalSince1970"), "yyyy-MM-dd'T'HH:mm:ss'Z'"))); xml_add(o, n, cap, b); return; }
    if (is_cls(v, &NSArray_c)) {
        xml_add(o, n, cap, "<array>");
        u64 cnt = MSG0(u64, v, "count");
        for (u64 i = 0; i < cnt; i++) xml_obj(o, n, cap, MSG(id, v, "objectAtIndex:", i));
        xml_add(o, n, cap, "</array>");
        return;
    }
    if (is_cls(v, &NSDictionary_c)) {
        xml_add(o, n, cap, "<dict>");
        id keys = MSG0(id, v, "allKeys");
        u64 cnt = keys ? MSG0(u64, keys, "count") : 0;
        for (u64 i = 0; i < cnt; i++) {
            id k = MSG(id, keys, "objectAtIndex:", i);
            xml_add(o, n, cap, "<key>"); xml_escape(o, n, cap, cs(k)); xml_add(o, n, cap, "</key>");
            xml_obj(o, n, cap, MSG(id, v, "objectForKey:", k));
        }
        rel(keys);
        xml_add(o, n, cap, "</dict>");
    }
}
static id plist_data(id obj) {
    char *o = malloc(128);
    u64 n = 0, cap = 128;
    xml_add(&o, &n, &cap, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<plist version=\"1.0\">");
    xml_obj(&o, &n, &cap, obj);
    xml_add(&o, &n, &cap, "</plist>");
    id d = MSG(id, (id)&NSData_c, "dataWithBytes:length:", o, n);
    free(o);
    return d;
}
static id arch_finished(id cls, SEL c, id root, BOOL secure, id *err) {
    id graph = arch_data(cls, c, root, secure, err);
    id data = plist_data(graph);
    rel(graph);
    return ar(data);
}
static id arch_one(id cls, SEL c, id root) { return arch_finished(cls, c, root, 0, 0); }
static id arch_encode_obj(id s, SEL c, id obj, id key) {
    (void)c;
    if (!BOX(s)->b || !key) return 0;
    i64 idn = arch_intern(s, obj);
    dset(BOX(s)->b, retain(key), uid_obj(idn));
    return 0;
}
static void arch_encode_bool(id s, SEL c, BOOL v, id key) { arch_encode_obj(s, c, num_b(v), key); }
static void arch_encode_int(id s, SEL c, i64 v, id key) { arch_encode_obj(s, c, num_i(v), key); }
static void arch_encode_d(id s, SEL c, double v, id key) { arch_encode_obj(s, c, num_d(v), key); }

static id unarch_resolve(id ua, i64 idx);
static id unarch_value(id ua, id v) {
    if (is_uid(v)) return unarch_resolve(ua, uid_of(v));
    if (is_cls(v, &NSArray_c)) {
        id a = mutarr();
        u64 n = MSG0(u64, v, "count");
        for (u64 i = 0; i < n; i++) { id x = unarch_value(ua, MSG(id, v, "objectAtIndex:", i)); if (x) cx_arr_add(a, x); }
        return a;
    }
    if (is_cls(v, &NSDictionary_c) && dget(v, "$classname")) {
        const char *name = cs(dget(v, "$classname"));
        if (!strcmp(name, "NSDate")) {
            double ref = dval(dget(v, "NS.time"));
            return MSG(id, (id)&NSDate_c, "dateWithTimeIntervalSince1970:", ref + REF_1970);
        }
        cx_class_t *cls = cx_class_named(name);
        if (!cls) return 0;
        box_t *coder = (box_t *)cx_alloc_instance(&NSKeyedUnarchiver_c);
        coder->a = BOX(ua)->a;
        coder->b = v;
        coder->p = BOX(ua)->p;
        id obj = MSG0(id, MSG0(id, (id)cls, "alloc"), "init");
        if (MSG(BOOL, obj, "respondsToSelector:", sel_registerName("initWithCoder:"))) {
            rel(obj);
            obj = MSG(id, MSG0(id, (id)cls, "alloc"), "initWithCoder:", coder);
        }
        rel((id)coder);
        return obj;
    }
    if (is_cls(v, &NSDictionary_c) && (dget(v, "NS.keys") || dget(v, "NS.objects"))) {
        id keys = unarch_value(ua, dget(v, "NS.keys"));
        id vals = unarch_value(ua, dget(v, "NS.objects"));
        id d = mutdict();
        u64 n = keys ? MSG0(u64, keys, "count") : 0;
        for (u64 i = 0; i < n; i++) dset(d, retain(MSG(id, keys, "objectAtIndex:", i)), retain(MSG(id, vals, "objectAtIndex:", i)));
        return d;
    }
    return retain(v);
}
static id unarch_resolve(id ua, i64 idx) {
    id objs = BOX(ua)->a;
    id cache = BOX(ua)->c;
    id key = num_i(idx);
    id hit = cache ? MSG(id, cache, "objectForKey:", key) : 0;
    rel(key);
    if (hit) return hit;
    if (!objs || idx < 0 || (u64)idx >= MSG0(u64, objs, "count")) return 0;
    id raw = MSG(id, objs, "objectAtIndex:", (u64)idx);
    id v = unarch_value(ua, raw);
    if (v && cache) { id k = num_i(idx); MSG(void, cache, "setObject:forKey:", v, k); rel(k); }
    return v;
}
static id dec_obj(id s, SEL c, id key) {
    (void)c;
    if (!BOX(s)->b || !key) return 0;
    return unarch_value(s, MSG(id, BOX(s)->b, "objectForKey:", key));
}
static id dec_obj_cls(id s, SEL c, cx_class_t *cls, id key) { (void)cls; return dec_obj(s, c, key); }
static BOOL dec_bool(id s, SEL c, id key) { id o = dec_obj(s, c, key); return truth(o); }
static i64 dec_int(id s, SEL c, id key) { id o = dec_obj(s, c, key); return ival(o); }
static double dec_d(id s, SEL c, id key) { id o = dec_obj(s, c, key); return dval(o); }
static BOOL dec_has(id s, SEL c, id key) { (void)c; return BOX(s)->b && key && MSG(id, BOX(s)->b, "objectForKey:", key) ? YES : NO; }
static BOOL dec_keyed(id s, SEL c) { (void)s; (void)c; return YES; }
static id unarch_root(id data) {
    if (!data) return 0;
    id plist = cx_plist_parse(MSG0(const u8 *, data, "bytes"), MSG0(u64, data, "length"));
    if (!plist) return 0;
    id objs = dget(plist, "$objects");
    id top = dget(plist, "$top");
    if (!objs || !top) return plist;
    box_t *ua = (box_t *)cx_alloc_instance(&NSKeyedUnarchiver_c);
    ua->a = retain(objs);
    ua->c = mutdict();
    id rootref = dget(top, "root");
    id root = rootref ? unarch_value((id)ua, rootref) : 0;
    rel((id)ua);
    return root;
}
static id unarch_data(id cls, SEL c, id data) { (void)cls; (void)c; id o = unarch_root(data); return o ? ar(o) : 0; }
static id unarch_cls(id cls, SEL c, cx_class_t *want, id data, id *err) {
    (void)cls; (void)c;
    if (err) *err = 0;
    id o = unarch_root(data);
    if (!o) { if (err) *err = ar(make_err("NSCocoaErrorDomain", 4864, "arquivo ilegível")); return 0; }
    if (want && !is_cls(o, want) && o->isa != want) { rel(o); if (err) *err = ar(make_err("NSCocoaErrorDomain", 4864, "classe inesperada")); return 0; }
    return ar(o);
}

/* ---------------------------------------------------------------- coleções que faltavam */
static id sort_with(id cls, SEL c, id key, BOOL asc) {
    (void)c;
    box_t *o = (box_t *)cx_alloc_instance((cx_class_t *)cls);
    o->a = retain(key); o->fl = asc ? 1 : 0;
    return ar((id)o);
}
static id sort_key(id s, SEL c) { (void)c; return BOX(s)->a; }
static BOOL sort_asc(id s, SEL c) { (void)c; return BOX(s)->fl ? YES : NO; }
static long sort_cmp(id s, SEL c, id a, id b) {
    (void)c;
    id ka = a ? MSG(id, a, "valueForKey:", BOX(s)->a) : 0;
    id kb = b ? MSG(id, b, "valueForKey:", BOX(s)->a) : 0;
    long r = pcmp_vals(ka, kb, PC_LT, 0) ? -1 : pcmp_vals(ka, kb, PC_GT, 0) ? 1 : 0;
    return BOX(s)->fl ? r : -r;
}
static id ip_with(id cls, SEL c, u64 i) {
    (void)c;
    box_t *o = (box_t *)cx_alloc_instance((cx_class_t *)cls);
    u64 *xs = calloc(1, sizeof(u64));
    xs[0] = i; o->p = xs; o->i = 1;
    return ar((id)o);
}
static id ip_with_n(id cls, SEL c, const u64 *xs, u64 n) {
    (void)c;
    box_t *o = (box_t *)cx_alloc_instance((cx_class_t *)cls);
    u64 *d = calloc(n ? n : 1, sizeof(u64));
    if (xs && n) memcpy(d, xs, n * sizeof(u64));
    o->p = d; o->i = (i64)n;
    return ar((id)o);
}
static u64 ip_len(id s, SEL c) { (void)c; return (u64)BOX(s)->i; }
static u64 ip_at(id s, SEL c, u64 i) { (void)c; return i < (u64)BOX(s)->i ? ((u64 *)BOX(s)->p)[i] : (u64)NSNotFound; }
static id ip_add(id s, SEL c, u64 i) {
    u64 n = (u64)BOX(s)->i;
    u64 *d = malloc((n + 1) * sizeof(u64));
    if (n) memcpy(d, BOX(s)->p, n * sizeof(u64));
    d[n] = i;
    return ip_with_n((id)s->isa, c, d, n + 1);
}

/* NSIndexSet/NSMutableIndexSet: fdn_indexset.c */

static id ht_alloc(id cls, SEL c) { (void)c; box_t *o = (box_t *)cx_alloc_instance((cx_class_t *)cls); cxmap_t *m = map_new(); m->magic = HASH_MAGIC; o->p = m; return (id)o; }
static void ht_add(id s, SEL c, id o) { (void)c; NSHashInsertKnownAbsent(BOX(s)->p, o); retain(o); }
static void ht_remove(id s, SEL c, id o) { (void)c; NSHashRemove(BOX(s)->p, o); rel(o); }
static id ht_member(id s, SEL c, id o) { (void)c; return NSMapGet(BOX(s)->p, o) ? o : 0; }
static u64 ht_count(id s, SEL c) { (void)c; cxmap_t *m = BOX(s)->p; return m ? m->n : 0; }
static id ht_all(id s, SEL c) {
    (void)c;
    cxmap_t *m = BOX(s)->p;
    id a = mutarr();
    if (m) for (u32 i = 0; i < m->n; i++) cx_arr_add(a, (id)m->k[i]);
    return ar(a);
}
static id mt_alloc(id cls, SEL c) { (void)c; box_t *o = (box_t *)cx_alloc_instance((cx_class_t *)cls); o->p = map_new(); return (id)o; }
static void mt_set(id s, SEL c, id v, id k) { (void)c; map_put(BOX(s)->p, k, v, 0); retain(k); retain(v); }
static id mt_get(id s, SEL c, id k) { (void)c; return NSMapGet(BOX(s)->p, k); }
static void mt_del(id s, SEL c, id k) { (void)c; NSMapRemove(BOX(s)->p, k); }
static id pa_alloc(id cls, SEL c) { (void)c; box_t *o = (box_t *)cx_alloc_instance((cx_class_t *)cls); o->p = map_new(); ((cxmap_t *)o->p)->magic = HASH_MAGIC; return (id)o; }
static void pa_add(id s, SEL c, void *p) { (void)c; NSHashInsertKnownAbsent(BOX(s)->p, p); }
static u64 pa_count(id s, SEL c) { return ht_count(s, c); }
static void *pa_at(id s, SEL c, u64 i) { (void)c; cxmap_t *m = BOX(s)->p; return m && i < m->n ? (void *)m->k[i] : 0; }

/* ---------------------------------------------------------------- transformada afim, unidades, vários sacos de propriedades */
static id at_ident(id cls, SEL c) {
    (void)c;
    box_t *o = (box_t *)cx_alloc_instance((cx_class_t *)cls);
    o->d = 1; o->f = 1; /* m11, m22 */
    return ar((id)o);
}
static void at_translate(id s, SEL c, double x, double y) { (void)c; BOX(s)->i += (i64)(x * 1000); BOX(s)->j += (i64)(y * 1000); }
static void at_scale(id s, SEL c, double x, double y) { (void)c; BOX(s)->d *= x; BOX(s)->f *= y; }
static void at_rotate(id s, SEL c, double deg) {
    (void)c;
    (void)deg;
}
static pt_t at_pt(id s, SEL c, pt_t p) { (void)c; pt_t r = {p.x * BOX(s)->d + (double)BOX(s)->i / 1000.0, p.y * BOX(s)->f + (double)BOX(s)->j / 1000.0}; return r; }
static sz_t at_sz(id s, SEL c, sz_t z) { (void)c; sz_t r = {z.w * BOX(s)->d, z.h * BOX(s)->f}; return r; }

/* NSUnit, NSDimension, NSMeasurement e NSMeasurementFormatter estão em fdn_units.c */

static void set_a(id s, SEL c, id v) { (void)c; rel(BOX(s)->a); BOX(s)->a = retain(v); }
static void set_b(id s, SEL c, id v) { (void)c; rel(BOX(s)->b); BOX(s)->b = retain(v); }
static void set_c(id s, SEL c, id v) { (void)c; rel(BOX(s)->c); BOX(s)->c = retain(v); }
static void box_dealloc(id s, SEL c);
static void box_dealloc(id s, SEL c) {
    rel(BOX(s)->a); rel(BOX(s)->b); rel(BOX(s)->c); rel(BOX(s)->e);
    BOX(s)->a = BOX(s)->b = BOX(s)->c = BOX(s)->e = 0;
    void (*d)(id, SEL) = (void (*)(id, SEL))cx_lookup(&NSObject_c, sel_registerName("dealloc"));
    if (d) d(s, c);
}
static id get_e(id s, SEL c) { (void)c; return BOX(s)->e; }
static id get_a(id s, SEL c) { (void)c; return BOX(s)->a; }
static id get_b(id s, SEL c) { (void)c; return BOX(s)->b; }
static id get_c(id s, SEL c) { (void)c; return BOX(s)->c; }
static id act_init(id s, SEL c, id type) { (void)c; BOX(s)->a = retain(type); return s; }
static id act_with(id cls, SEL c, id type) { return ar(act_init(cx_alloc_instance((cx_class_t *)cls), c, type)); }
static void act_become(id s, SEL c) { (void)s; (void)c; }
static void act_resign(id s, SEL c) { (void)s; (void)c; }
static void act_inv(id s, SEL c) { (void)c; BOX(s)->fl = 1; }

static id item_init(id s, SEL c, id obj) { (void)c; BOX(s)->a = retain(obj); return s; }
static void item_load(id s, SEL c, id type, void *block) {
    (void)c; (void)type;
    if (!block) return;
    id data = 0;
    if (BOX(s)->a && is_cls(BOX(s)->a, &NSData_c)) data = BOX(s)->a;
    else if (BOX(s)->a && cx_is_str(BOX(s)->a)) data = MSG(id, (id)&NSData_c, "dataWithBytes:length:", cs(BOX(s)->a), strlen(cs(BOX(s)->a)));
    void (*inv)(void *, id, id) = *(void **)((u8 *)block + 16);
    inv(block, data, 0);
}

static id aed_with_str(id cls, SEL c, id s) { (void)c; box_t *o = (box_t *)cx_alloc_instance((cx_class_t *)cls); o->a = retain(s); return ar((id)o); }
static id aed_string(id s, SEL c) { (void)c; return BOX(s)->a; }
static id assert_current(id cls, SEL c) { (void)c; static id s; if (!s) s = cx_alloc_instance((cx_class_t *)cls); return s; }
static void assert_fail(id s, SEL c, SEL sel, id obj, const char *file, long line, id desc) {
    (void)s; (void)c;
    cx_fdprintf(2, "*** asserção em %s:%ld -[%s %s] %s\n", file ? file : "?", line, obj ? cx_class_name(obj->isa) : "?", sel ? sel : "?", cs(desc));
    abort();
}

static id meta_item(id cls, SEL c, id path) {
    (void)c;
    box_t *o = (box_t *)cx_alloc_instance((cx_class_t *)cls);
    o->a = retain(path);
    return ar((id)o);
}
static id meta_value(id s, SEL c, id key) {
    (void)c;
    if (key == NSMetadataItemPathKey || key == NSMetadataItemFSNameKey || (key && !strcmp(cs(key), "kMDItemPath"))) return BOX(s)->a;
    return 0;
}
static id mq_alloc(id cls, SEL c) { (void)c; box_t *o = (box_t *)cx_alloc_instance((cx_class_t *)cls); o->a = mutarr(); return (id)o; }
static void mq_start(id s, SEL c) { (void)c; BOX(s)->fl = 1; id n = ns(cs(NSMetadataQueryDidFinishGatheringNotification)); MSG(void, (id)cx_class_named("NSNotificationCenter"), "postNotificationName:object:", n, s); rel(n); }
static void mq_stop(id s, SEL c) { (void)c; BOX(s)->fl = 0; }
static id mq_results(id s, SEL c) { (void)c; return BOX(s)->a; }
static u64 mq_count(id s, SEL c) { (void)c; return BOX(s)->a ? MSG0(u64, BOX(s)->a, "count") : 0; }
static BOOL mq_started(id s, SEL c) { (void)c; return BOX(s)->fl ? YES : NO; }
static void mq_set_pred(id s, SEL c, id p) { (void)c; rel(BOX(s)->b); BOX(s)->b = retain(p); }

static void tagger_enum(id s, SEL c, rng_t range, u64 opt, id scheme, void *block) {
    (void)c; (void)opt; (void)scheme;
    if (!block || !BOX(s)->a) return;
    const char *str = cs(BOX(s)->a);
    u64 n = strlen(str);
    if (range.location > n) return;
    void (*inv)(void *, id, rng_t, rng_t, BOOL *) = *(void **)((u8 *)block + 16);
    u64 i = range.location, end = range.location + range.length;
    if (end > n) end = n;
    BOOL stop = NO;
    while (i < end && !stop) {
        while (i < end && (str[i] == ' ' || str[i] == '\n' || str[i] == '\t')) i++;
        u64 a = i;
        while (i < end && str[i] != ' ' && str[i] != '\n' && str[i] != '\t') i++;
        if (i > a) { rng_t r = {a, i - a}; inv(block, NSLinguisticTagOtherWord, r, r, &stop); }
    }
}
static id tagger_init(id s, SEL c, id str) { (void)c; BOX(s)->a = retain(str); return s; }

static BOOL xml_parse(id s, SEL c) {
    (void)c;
    const char *p = cs(BOX(s)->a);
    id del = BOX(s)->b;
    if (del && MSG(BOOL, del, "respondsToSelector:", sel_registerName("parserDidStartDocument:"))) MSG(void, del, "parserDidStartDocument:", s);
    while (p && *p) {
        const char *lt = strchr(p, '<');
        if (!lt) break;
        if (lt > p && del && MSG(BOOL, del, "respondsToSelector:", sel_registerName("parser:foundCharacters:"))) {
            id t = copystr(p, (u64)(lt - p));
            MSG(void, del, "parser:foundCharacters:", s, t);
            rel(t);
        }
        if (lt[1] == '/') {
            const char *gt = strchr(lt, '>');
            char name[64]; u64 n = gt ? (u64)(gt - lt - 2) : 0; if (n > 63) n = 63;
            memcpy(name, lt + 2, n); name[n] = 0;
            if (del) MSG(void, del, "parser:didEndElement:namespaceURI:qualifiedName:", s, ns(name), 0, ns(name));
            p = gt ? gt + 1 : lt + 1;
        } else if (lt[1] == '!' || lt[1] == '?') {
            const char *gt = strchr(lt, '>'); p = gt ? gt + 1 : lt + 1;
        } else {
            const char *gt = strchr(lt, '>');
            char name[64]; u64 n = 0;
            const char *q = lt + 1;
            while (q < gt && *q != ' ' && *q != '/' && n < 63) name[n++] = *q++;
            name[n] = 0;
            if (del) MSG(void, del, "parser:didStartElement:namespaceURI:qualifiedName:attributes:", s, ns(name), 0, ns(name), mutdict());
            int empty = gt && gt > lt && gt[-1] == '/';
            if (empty && del) MSG(void, del, "parser:didEndElement:namespaceURI:qualifiedName:", s, ns(name), 0, ns(name));
            p = gt ? gt + 1 : lt + 1;
        }
    }
    if (del && MSG(BOOL, del, "respondsToSelector:", sel_registerName("parserDidEndDocument:"))) MSG(void, del, "parserDidEndDocument:", s);
    return YES;
}
static id xml_init_str(id s, SEL c, id str) { (void)c; BOX(s)->a = retain(str); return s; }
static void xml_set_del(id s, SEL c, id d) { (void)c; rel(BOX(s)->b); BOX(s)->b = retain(d); }
static id xml_del(id s, SEL c) { (void)c; return BOX(s)->b; }

static id xpc_init_name(id s, SEL c, id name) { (void)c; BOX(s)->a = retain(name); return s; }
static id xpc_with(id cls, SEL c, id name) { return ar(xpc_init_name(cx_alloc_instance((cx_class_t *)cls), c, name)); }
static void xpc_resume(id s, SEL c) {
    (void)c;
    BOX(s)->fl = 1;
    /* sem launchd: a conexão fica inválida, como um Mac sem o serviço */
    if (BOX(s)->p) { void (*inv)(void *) = *(void **)((u8 *)BOX(s)->p + 16); inv(BOX(s)->p); }
}
static void xpc_inv(id s, SEL c) { (void)c; BOX(s)->fl = 0; }
static void xpc_set_iface(id s, SEL c, id i) { (void)c; rel(BOX(s)->b); BOX(s)->b = retain(i); }
static void xpc_set_int(id s, SEL c, void *b) { (void)c; BOX(s)->p = b ? _Block_copy(b) : 0; }
static id xpc_proxy(id s, SEL c) { (void)c; if (!BOX(s)->c) BOX(s)->c = cx_alloc_instance(&NSProxy_c); return BOX(s)->c; }
static id xpc_iface(id cls, SEL c, void *proto) { (void)c; box_t *o = (box_t *)cx_alloc_instance((cx_class_t *)cls); o->p = proto; return ar((id)o); }
static id xpc_listener(id cls, SEL c) { (void)c; static id s; if (!s) s = cx_alloc_instance((cx_class_t *)cls); return s; }
static void xpc_set_del(id s, SEL c, id d) { set_b(s, c, d); }
static id mach_boot(id cls, SEL c) { (void)c; static id s; if (!s) s = cx_alloc_instance((cx_class_t *)cls); return s; }
static id mach_lookup(id s, SEL c, id name) { (void)s; (void)c; (void)name; return 0; }
static id port_new(id cls, SEL c) { (void)c; return ar(cx_alloc_instance((cx_class_t *)cls)); }
static u32 port_mach(id s, SEL c) { (void)s; (void)c; return 0; }

static id ns_init(id s, SEL c, id domain, id type, id name, int port) {
    (void)c;
    BOX(s)->a = retain(domain); BOX(s)->b = retain(type); BOX(s)->c = retain(name); BOX(s)->i = port;
    return s;
}
static id ns_with(id cls, SEL c, id d, id t, id n, int port) { return ar(ns_init(cx_alloc_instance((cx_class_t *)cls), c, d, t, n, port)); }
static void ns_publish(id s, SEL c) {
    (void)c;
    id del = BOX(s)->e;
    if (del && MSG(BOOL, del, "respondsToSelector:", sel_registerName("netService:didNotPublish:"))) {
        id err = make_err("NSNetServicesErrorDomain", -72000, "sem mDNS");
        MSG(void, del, "netService:didNotPublish:", s, err);
        rel(err);
    }
}
static void ns_stop(id s, SEL c) { (void)c; BOX(s)->fl = 0; }
static void ns_set_del(id s, SEL c, id d) { (void)c; rel(BOX(s)->e); BOX(s)->e = retain(d); }

static id vt_set(id cls, SEL c, id t, id name) {
    (void)cls; (void)c;
    static id tab;
    if (!tab) tab = mutdict();
    if (t && name) MSG(void, tab, "setObject:forKey:", t, name);
    return 0;
}
static id vt_for(id cls, SEL c, id name) {
    (void)cls; (void)c;
    static id tab;
    if (!tab) tab = mutdict();
    return name ? MSG(id, tab, "objectForKey:", name) : 0;
}
static id vt_fwd(id s, SEL c, id v) { (void)s; (void)c; return v; }
static id sec_fwd(id s, SEL c, id v) { (void)s; (void)c; id o = unarch_root(v); return o ? ar(o) : 0; }

static id resreq_init(id s, SEL c, id tags, id bundle) { (void)c; BOX(s)->a = retain(tags); BOX(s)->b = retain(bundle); return s; }
static void resreq_begin(id s, SEL c, void *block) {
    (void)s; (void)c;
    if (!block) return;
    void (*inv)(void *, id) = *(void **)((u8 *)block + 16);
    inv(block, 0);
}
/* A base não encaminha. methodSignature nil faz o _objc_msgForward parar; a subclasse
 * implementa os dois. Não é o doesNotRecognizeSelector: da Apple. */
static id proxy_sig(id s, SEL c, SEL sel) { (void)s; (void)c; (void)sel; return nil; }
static void proxy_fwd(id s, SEL c, id inv) { (void)s; (void)c; (void)inv; }
static id proxy_desc(id s, SEL c) { (void)s; (void)c; return ar(ns("<NSProxy>")); }
static BOOL proxy_kind(id s, SEL c, cx_class_t *k) { (void)c; return cx_is_subclass(s->isa, k); }

static id pls_read(id cls, SEL c, id data, u64 opt, u64 *fmt, id *err) {
    (void)cls; (void)c; (void)opt;
    if (err) *err = 0;
    if (!data) { if (fmt) *fmt = 100; return 0; }
    const u8 *pb = MSG0(const u8 *, data, "bytes");
    u64 pn = MSG0(u64, data, "length");
    if (fmt) *fmt = (pn >= 8 && pb && !memcmp(pb, "bplist00", 8)) ? 200 : 100;
    id r = cx_plist_parse(pb, pn);
    return r ? ar(r) : 0;
}
static id pls_write(id cls, SEL c, id obj, u64 fmt, u64 opt, id *err) {
    (void)cls; (void)c; (void)fmt; (void)opt;
    if (err) *err = 0;
    return obj ? plist_data(obj) : 0;
}

static void mas_replace(id s, SEL c, rng_t r, id str) {
    (void)c; (void)r;
    id *slot = (id *)((u8 *)s + 8);
    id st = *slot ? *(id *)*slot : 0;
    /* estado do AppKit: {id str, attrs} logo após o ponteiro */
    if (*slot) {
        id *fields = (id *)(void *)(*slot);
        rel(fields[0]);
        fields[0] = retain(str);
    }
    (void)st;
}
static void mas_set_attr(id s, SEL c, id name, id value, rng_t r) { (void)s; (void)c; (void)name; (void)value; (void)r; }

/* NSCharacterSet/NSMutableCharacterSet: fdn_charset.c */

/* ---------------------------------------------------------------- registro */
#define M(cls, s, f) cx_add_method(&cls##_c, s, (void *)(f))
#define CM(cls, s, f) cx_add_cmethod(&cls##_c, s, (void *)(f))
static cx_class_t *DEF(cx_class_t *c, cx_class_t *m, const char *n, cx_class_t *s, u32 ex) { return cx_define_class(c, m, n, s, ex); }
static id singleton(id cls, SEL c) { (void)c; static struct { cx_class_t *k; id o; } tab[24]; cx_class_t *k = (cx_class_t *)cls; for (int i = 0; i < 24; i++) { if (tab[i].k == k) return tab[i].o; if (!tab[i].k) { tab[i].k = k; tab[i].o = cx_alloc_instance(k); return tab[i].o; } } return cx_alloc_instance(k); }
static id box_init(id s, SEL c) { (void)c; return s; }
static BOOL yes_m(id s, SEL c) { (void)s; (void)c; return YES; }
static void noop(id s, SEL c) { (void)s; (void)c; }

static id fh_stdin(id cls, SEL c) { return fh_std(cls, c, 0); }
static id fh_stdout(id cls, SEL c) { return fh_std(cls, c, 1); }
static id fh_stderr(id cls, SEL c) { return fh_std(cls, c, 2); }

void cx_foundation_more_init(void) {
    cx_class_t *ob = &NSObject_c;

    DEF(&NSProxy_c, &NSProxy_m, "NSProxy", 0, BEX);
    CM(NSProxy, "alloc", lock_alloc);
    M(NSProxy, "init", box_init); M(NSProxy, "dealloc", noop); M(NSProxy, "description", proxy_desc);
    M(NSProxy, "isKindOfClass:", proxy_kind); M(NSProxy, "forwardInvocation:", proxy_fwd);
    M(NSProxy, "methodSignatureForSelector:", proxy_sig);

    /* NSError já existe desde o Foundation; estes métodos ficam por cima dos antigos */
    CM(NSError, "errorWithDomain:code:userInfo:", err_with);
    M(NSError, "domain", err_domain); M(NSError, "code", err_code); M(NSError, "userInfo", err_info);
    M(NSError, "localizedDescription", err_desc); M(NSError, "description", err_desc);
    M(NSError, "initWithDomain:code:userInfo:", err_init);
    M(NSError, "localizedFailureReason", err_reason); M(NSError, "localizedRecoverySuggestion", err_suggestion);

    DEF(&NSJSONSerialization_c, &NSJSONSerialization_m, "NSJSONSerialization", ob, 0);
    CM(NSJSONSerialization, "JSONObjectWithData:options:error:", json_obj);
    CM(NSJSONSerialization, "dataWithJSONObject:options:error:", json_data);
    CM(NSJSONSerialization, "isValidJSONObject:", yes_m);

    DEF(&NSPropertyListSerialization_c, &NSPropertyListSerialization_m, "NSPropertyListSerialization", ob, 0);
    CM(NSPropertyListSerialization, "propertyListWithData:options:format:error:", pls_read);
    CM(NSPropertyListSerialization, "dataWithPropertyList:format:options:error:", pls_write);

    DEF(&NSUUID_c, &NSUUID_m, "NSUUID", ob, BEX);
    CM(NSUUID, "UUID", uuid_new); CM(NSUUID, "alloc", lock_alloc);
    M(NSUUID, "init", uuid_new); M(NSUUID, "UUIDString", uuid_str); M(NSUUID, "description", uuid_desc);
    M(NSUUID, "getUUIDBytes:", uuid_bytes);

    DEF(&NSLock_c, &NSLock_m, "NSLock", ob, BEX);
    CM(NSLock, "alloc", lock_alloc); M(NSLock, "lock", lk_lock); M(NSLock, "unlock", lk_unlock); M(NSLock, "tryLock", lk_try);
    M(NSLock, "name", get_a); M(NSLock, "setName:", set_a);
    DEF(&NSRecursiveLock_c, &NSRecursiveLock_m, "NSRecursiveLock", ob, BEX);
    CM(NSRecursiveLock, "alloc", lock_alloc); M(NSRecursiveLock, "lock", lk_rec); M(NSRecursiveLock, "unlock", lk_unrec); M(NSRecursiveLock, "tryLock", lk_try);
    M(NSRecursiveLock, "name", get_a); M(NSRecursiveLock, "setName:", set_a);
    DEF(&NSCondition_c, &NSCondition_m, "NSCondition", ob, BEX);
    CM(NSCondition, "alloc", lock_alloc); M(NSCondition, "lock", lk_lock); M(NSCondition, "unlock", lk_unlock);
    M(NSCondition, "wait", cond_wait); M(NSCondition, "signal", cond_signal); M(NSCondition, "broadcast", cond_bcast);
    M(NSCondition, "name", get_a); M(NSCondition, "setName:", set_a);
    DEF(&NSConditionLock_c, &NSConditionLock_m, "NSConditionLock", ob, BEX);
    CM(NSConditionLock, "alloc", lock_alloc);
    M(NSConditionLock, "initWithCondition:", condlock_init); M(NSConditionLock, "lock", lk_lock); M(NSConditionLock, "unlock", lk_unlock);
    M(NSConditionLock, "lockWhenCondition:", condlock_lockwhen); M(NSConditionLock, "unlockWithCondition:", condlock_unlockwith);
    M(NSConditionLock, "condition", condlock_cond);



    DEF(&NSRegularExpression_c, &NSRegularExpression_m, "NSRegularExpression", ob, BEX);
    CM(NSRegularExpression, "regularExpressionWithPattern:options:error:", re_with);
    M(NSRegularExpression, "pattern", get_a);
    M(NSRegularExpression, "matchesInString:options:range:", re_matches);
    M(NSRegularExpression, "numberOfMatchesInString:options:range:", re_count);
    M(NSRegularExpression, "stringByReplacingMatchesInString:options:range:withTemplate:", re_replace);
    DEF(&NSDataDetector_c, &NSDataDetector_m, "NSDataDetector", &NSRegularExpression_c, 0);
    CM(NSDataDetector, "dataDetectorWithTypes:error:", dd_with);
    DEF(&NSTextCheckingResult_c, &NSTextCheckingResult_m, "NSTextCheckingResult", ob, BEX);
    M(NSTextCheckingResult, "range", tcr_range); M(NSTextCheckingResult, "resultType", tcr_type);
    M(NSTextCheckingResult, "URL", tcr_extra); M(NSTextCheckingResult, "phoneNumber", tcr_extra);
    M(NSTextCheckingResult, "replacementString", tcr_extra);

    DEF(&NSExpression_c, &NSExpression_m, "NSExpression", ob, BEX);
    CM(NSExpression, "expressionForKeyPath:", expr_key); CM(NSExpression, "expressionForConstantValue:", expr_const);
    M(NSExpression, "keyPath", expr_keypath); M(NSExpression, "constantValue", expr_cval);
    M(NSExpression, "expressionValueWithObject:context:", expr_eval);
    DEF(&NSPredicate_c, &NSPredicate_m, "NSPredicate", ob, BEX);
    CM(NSPredicate, "predicateWithFormat:", pred_fmt); CM(NSPredicate, "predicateWithValue:", pred_value);
    M(NSPredicate, "evaluateWithObject:", pred_eval); M(NSPredicate, "evaluateWithObject:substitutionVariables:", pred_eval);
    DEF(&NSComparisonPredicate_c, &NSComparisonPredicate_m, "NSComparisonPredicate", &NSPredicate_c, 0);
    CM(NSComparisonPredicate, "predicateWithLeftExpression:rightExpression:modifier:type:options:", cmp_pred);
    DEF(&NSCompoundPredicate_c, &NSCompoundPredicate_m, "NSCompoundPredicate", &NSPredicate_c, 0);
    CM(NSCompoundPredicate, "andPredicateWithSubpredicates:", pred_and);
    CM(NSCompoundPredicate, "orPredicateWithSubpredicates:", pred_or);
    CM(NSCompoundPredicate, "notPredicateWithSubpredicate:", pred_not);

    DEF(&NSFileHandle_c, &NSFileHandle_m, "NSFileHandle", ob, BEX);
    CM(NSFileHandle, "fileHandleForReadingAtPath:", fh_read_path);
    CM(NSFileHandle, "fileHandleForWritingAtPath:", fh_write_path);
    CM(NSFileHandle, "fileHandleWithStandardInput", fh_stdin);
    CM(NSFileHandle, "fileHandleWithStandardOutput", fh_stdout);
    CM(NSFileHandle, "fileHandleWithStandardError", fh_stderr);
    M(NSFileHandle, "readDataToEndOfFile", fh_read_all); M(NSFileHandle, "readDataOfLength:", fh_read_n);
    M(NSFileHandle, "availableData", fh_avail); M(NSFileHandle, "writeData:", fh_write); M(NSFileHandle, "closeFile", fh_close);
    M(NSFileHandle, "seekToEndOfFile", fh_seek_end); M(NSFileHandle, "offsetInFile", fh_offset);
    M(NSFileHandle, "seekToFileOffset:", fh_seek); M(NSFileHandle, "fileDescriptor", fh_fd);
    DEF(&NSPipe_c, &NSPipe_m, "NSPipe", ob, BEX);
    CM(NSPipe, "pipe", pipe_new); M(NSPipe, "fileHandleForReading", pipe_rd); M(NSPipe, "fileHandleForWriting", pipe_wr);
    DEF(&NSTask_c, &NSTask_m, "NSTask", ob, BEX);
    CM(NSTask, "alloc", task_alloc);
    M(NSTask, "setLaunchPath:", task_set_path); M(NSTask, "launchPath", task_path);
    M(NSTask, "setExecutableURL:", task_set_path); M(NSTask, "executableURL", task_path);
    M(NSTask, "setArguments:", task_set_args); M(NSTask, "arguments", task_args);
    M(NSTask, "setStandardOutput:", task_set_out); M(NSTask, "setStandardError:", task_set_out);
    M(NSTask, "setStandardInput:", task_set_in);
    M(NSTask, "launch", task_launch); M(NSTask, "launchAndReturnError:", task_launch_err);
    M(NSTask, "waitUntilExit", task_wait); M(NSTask, "terminationStatus", task_status);
    M(NSTask, "isRunning", task_running); M(NSTask, "terminate", task_term);

    DEF(&NSURLRequest_c, &NSURLRequest_m, "NSURLRequest", ob, BEX);
    CM(NSURLRequest, "requestWithURL:", req_with); CM(NSURLRequest, "alloc", lock_alloc);
    M(NSURLRequest, "initWithURL:", req_init_url); M(NSURLRequest, "URL", req_url); M(NSURLRequest, "HTTPMethod", req_method);
    M(NSURLRequest, "initWithURL:cachePolicy:timeoutInterval:", req_init_url_pt); CM(NSURLRequest, "requestWithURL:cachePolicy:timeoutInterval:", req_with_pt);
    M(NSURLRequest, "timeoutInterval", req_timeout);
    M(NSURLRequest, "copy", req_copy); M(NSURLRequest, "copyWithZone:", req_copy_zone);
    M(NSURLRequest, "mutableCopy", req_mcopy); M(NSURLRequest, "mutableCopyWithZone:", req_mcopy_zone);
    M(NSURLRequest, "allHTTPHeaderFields", req_headers); M(NSURLRequest, "HTTPBody", req_body);
    DEF(&NSMutableURLRequest_c, &NSMutableURLRequest_m, "NSMutableURLRequest", &NSURLRequest_c, 0);
    M(NSURLRequest, "setTimeoutInterval:", req_set_timeout);
    M(NSMutableURLRequest, "setTimeoutInterval:", req_set_timeout);
    M(NSMutableURLRequest, "setHTTPMethod:", req_set_method); M(NSMutableURLRequest, "setValue:forHTTPHeaderField:", req_set_header);
    M(NSMutableURLRequest, "setHTTPBody:", req_set_body); M(NSMutableURLRequest, "setURL:", set_a);
    M(NSMutableURLRequest, "setCachePolicy:", req_set_policy); M(NSURLRequest, "cachePolicy", req_policy);
    M(NSURLRequest, "valueForHTTPHeaderField:", req_value_for); M(NSMutableURLRequest, "setValue:forHTTPHeaderField:", req_set_value);
    M(NSMutableURLRequest, "addValue:forHTTPHeaderField:", req_add_value); M(NSMutableURLRequest, "setAllHTTPHeaderFields:", req_set_all_headers);
    M(NSURLRequest, "HTTPShouldHandleCookies", req_g_cookies); M(NSMutableURLRequest, "setHTTPShouldHandleCookies:", req_s_cookies);
    M(NSURLRequest, "allowsCellularAccess", req_g_cellular); M(NSMutableURLRequest, "setAllowsCellularAccess:", req_s_cellular);
    M(NSURLRequest, "allowsExpensiveNetworkAccess", req_g_expensive); M(NSMutableURLRequest, "setAllowsExpensiveNetworkAccess:", req_s_expensive);
    M(NSURLRequest, "allowsConstrainedNetworkAccess", req_g_constrained); M(NSMutableURLRequest, "setAllowsConstrainedNetworkAccess:", req_s_constrained);
    M(NSURLRequest, "HTTPShouldUsePipelining", req_g_pipelining); M(NSMutableURLRequest, "setHTTPShouldUsePipelining:", req_s_pipelining);
    M(NSURLRequest, "assumesHTTP3Capable", req_g_http3); M(NSMutableURLRequest, "setAssumesHTTP3Capable:", req_s_http3);
    M(NSURLRequest, "requiresDNSSECValidation", req_g_dnssec); M(NSMutableURLRequest, "setRequiresDNSSECValidation:", req_s_dnssec);
    M(NSURLRequest, "networkServiceType", req_service); M(NSMutableURLRequest, "setNetworkServiceType:", req_set_service);
    M(NSURLRequest, "attribution", req_attribution); M(NSMutableURLRequest, "setAttribution:", req_set_attribution);
    M(NSURLRequest, "HTTPBodyStream", req_stream); M(NSMutableURLRequest, "setHTTPBodyStream:", req_set_stream);
    M(NSURLRequest, "mainDocumentURL", req_main_doc); M(NSMutableURLRequest, "setMainDocumentURL:", req_set_main_doc);
    M(NSURLRequest, "isEqual:", req_equal); M(NSURLRequest, "hash", req_hash); M(NSURLRequest, "dealloc", req_dealloc);
    CM(NSURLRequest, "supportsSecureCoding", req_secure);
    DEF(&NSURLResponse_c, &NSURLResponse_m, "NSURLResponse", ob, BEX);
    M(NSURLResponse, "URL", resp_url); M(NSURLResponse, "expectedContentLength", resp_len);
    M(NSURLResponse, "MIMEType", get_c); M(NSURLResponse, "textEncodingName", get_e);
    DEF(&NSHTTPURLResponse_c, &NSHTTPURLResponse_m, "NSHTTPURLResponse", &NSURLResponse_c, 0);
    M(NSHTTPURLResponse, "statusCode", resp_status); M(NSHTTPURLResponse, "allHeaderFields", resp_headers);
    M(NSHTTPURLResponse, "initWithURL:statusCode:HTTPVersion:headerFields:", hresp_init);
    M(NSHTTPURLResponse, "valueForHTTPHeaderField:", hresp_value_for);
    CM(NSHTTPURLResponse, "localizedStringForStatusCode:", hresp_s_status_string);
    M(NSURLResponse, "initWithURL:MIMEType:expectedContentLength:textEncodingName:", resp_init_mime);
    M(NSURLResponse, "suggestedFilename", resp_suggested);
    M(NSURLResponse, "copy", resp_copy); M(NSURLResponse, "copyWithZone:", resp_copy_zone); M(NSURLResponse, "dealloc", resp_dealloc);
    CM(NSURLResponse, "supportsSecureCoding", req_secure);


    DEF(&NSURLComponents_c, &NSURLComponents_m, "NSURLComponents", ob, BEX);
    CM(NSURLComponents, "componentsWithString:", comp_make);
    M(NSURLComponents, "scheme", comp_scheme); M(NSURLComponents, "host", comp_host); M(NSURLComponents, "path", comp_path);
    M(NSURLComponents, "port", comp_port); M(NSURLComponents, "URL", comp_url); M(NSURLComponents, "string", get_e);
    M(NSURLComponents, "queryItems", comp_query_items); M(NSURLComponents, "setQueryItems:", comp_set_query_items);
    DEF(&NSURLQueryItem_c, &NSURLQueryItem_m, "NSURLQueryItem", ob, BEX);
    CM(NSURLQueryItem, "queryItemWithName:value:", qitem_make);
    M(NSURLQueryItem, "name", qitem_name); M(NSURLQueryItem, "value", qitem_value);


    DEF(&NSProgress_c, &NSProgress_m, "NSProgress", ob, BEX);
    CM(NSProgress, "progressWithTotalUnitCount:", prog_with); CM(NSProgress, "discreteProgressWithTotalUnitCount:", prog_with);
    M(NSProgress, "setTotalUnitCount:", prog_set_total); M(NSProgress, "setCompletedUnitCount:", prog_set_done);
    M(NSProgress, "totalUnitCount", prog_total); M(NSProgress, "completedUnitCount", prog_done);
    M(NSProgress, "fractionCompleted", prog_frac); M(NSProgress, "cancel", prog_cancel); M(NSProgress, "isCancelled", prog_cancelled);
    M(NSProgress, "setLocalizedDescription:", set_a); M(NSProgress, "localizedDescription", get_a);


    DEF(&NSKeyedArchiver_c, &NSKeyedArchiver_m, "NSKeyedArchiver", ob, BEX);
    CM(NSKeyedArchiver, "archivedDataWithRootObject:requiringSecureCoding:error:", arch_finished);
    CM(NSKeyedArchiver, "archivedDataWithRootObject:", arch_one);
    CM(NSKeyedArchiver, "alloc", arch_alloc);
    M(NSKeyedArchiver, "encodeObject:forKey:", arch_encode_obj); M(NSKeyedArchiver, "encodeBool:forKey:", arch_encode_bool);
    M(NSKeyedArchiver, "encodeInteger:forKey:", arch_encode_int); M(NSKeyedArchiver, "encodeInt:forKey:", arch_encode_int);
    M(NSKeyedArchiver, "encodeDouble:forKey:", arch_encode_d); M(NSKeyedArchiver, "finishEncoding", noop);
    M(NSKeyedArchiver, "allowsKeyedCoding", yes_m);
    DEF(&NSKeyedUnarchiver_c, &NSKeyedUnarchiver_m, "NSKeyedUnarchiver", ob, BEX);
    CM(NSKeyedUnarchiver, "unarchivedObjectOfClass:fromData:error:", unarch_cls);
    CM(NSKeyedUnarchiver, "unarchiveObjectWithData:", unarch_data);
    M(NSKeyedUnarchiver, "decodeObjectForKey:", dec_obj); M(NSKeyedUnarchiver, "decodeObjectOfClass:forKey:", dec_obj_cls);
    M(NSKeyedUnarchiver, "decodeBoolForKey:", dec_bool); M(NSKeyedUnarchiver, "decodeIntegerForKey:", dec_int);
    M(NSKeyedUnarchiver, "decodeIntForKey:", dec_int); M(NSKeyedUnarchiver, "decodeDoubleForKey:", dec_d);
    M(NSKeyedUnarchiver, "containsValueForKey:", dec_has); M(NSKeyedUnarchiver, "allowsKeyedCoding", dec_keyed);

    DEF(&NSSortDescriptor_c, &NSSortDescriptor_m, "NSSortDescriptor", ob, BEX);
    CM(NSSortDescriptor, "sortDescriptorWithKey:ascending:", sort_with);
    CM(NSSortDescriptor, "sortDescriptorWithKey:ascending:selector:", sort_with);
    M(NSSortDescriptor, "key", sort_key); M(NSSortDescriptor, "ascending", sort_asc);
    M(NSSortDescriptor, "compareObject:toObject:", sort_cmp);
    DEF(&NSIndexPath_c, &NSIndexPath_m, "NSIndexPath", ob, BEX);
    CM(NSIndexPath, "indexPathWithIndex:", ip_with); CM(NSIndexPath, "indexPathWithIndexes:length:", ip_with_n);
    M(NSIndexPath, "length", ip_len); M(NSIndexPath, "indexAtPosition:", ip_at); M(NSIndexPath, "indexPathByAddingIndex:", ip_add);
    /* NSCountedSet: fdn_set.c */
    DEF(&NSHashTable_c, &NSHashTable_m, "NSHashTable", ob, BEX);
    CM(NSHashTable, "weakObjectsHashTable", ht_alloc); CM(NSHashTable, "alloc", ht_alloc);
    M(NSHashTable, "addObject:", ht_add); M(NSHashTable, "removeObject:", ht_remove); M(NSHashTable, "member:", ht_member);
    M(NSHashTable, "count", ht_count); M(NSHashTable, "allObjects", ht_all);
    DEF(&NSMapTable_c, &NSMapTable_m, "NSMapTable", ob, BEX);
    CM(NSMapTable, "strongToStrongObjectsMapTable", mt_alloc); CM(NSMapTable, "alloc", mt_alloc);
    M(NSMapTable, "setObject:forKey:", mt_set); M(NSMapTable, "objectForKey:", mt_get); M(NSMapTable, "removeObjectForKey:", mt_del);
    DEF(&NSPointerArray_c, &NSPointerArray_m, "NSPointerArray", ob, BEX);
    CM(NSPointerArray, "strongObjectsPointerArray", pa_alloc); CM(NSPointerArray, "alloc", pa_alloc);
    M(NSPointerArray, "addPointer:", pa_add); M(NSPointerArray, "count", pa_count); M(NSPointerArray, "pointerAtIndex:", pa_at);

    DEF(&NSAffineTransform_c, &NSAffineTransform_m, "NSAffineTransform", ob, BEX);
    CM(NSAffineTransform, "transform", at_ident);
    M(NSAffineTransform, "translateXBy:yBy:", at_translate); M(NSAffineTransform, "scaleXBy:yBy:", at_scale);
    M(NSAffineTransform, "rotateByDegrees:", at_rotate); M(NSAffineTransform, "transformPoint:", at_pt);
    M(NSAffineTransform, "transformSize:", at_sz);

    /* NSUserNotification e NSUserNotificationCenter: nsusernotification.c (definidos lá, no init do AppKit) */
    DEF(&NSUserActivity_c, &NSUserActivity_m, "NSUserActivity", ob, BEX);
    CM(NSUserActivity, "alloc", lock_alloc);
    M(NSUserActivity, "initWithActivityType:", act_init);
    CM(NSUserActivity, "activityWithActivityType:", act_with);
    M(NSUserActivity, "activityType", get_a); M(NSUserActivity, "setTitle:", set_b); M(NSUserActivity, "title", get_b);
    M(NSUserActivity, "setUserInfo:", set_c); M(NSUserActivity, "userInfo", get_c);
    M(NSUserActivity, "setWebpageURL:", noop); M(NSUserActivity, "becomeCurrent", act_become);
    M(NSUserActivity, "resignCurrent", act_resign); M(NSUserActivity, "invalidate", act_inv);
    DEF(&NSItemProvider_c, &NSItemProvider_m, "NSItemProvider", ob, BEX);
    CM(NSItemProvider, "alloc", lock_alloc);
    M(NSItemProvider, "initWithObject:", item_init);
    M(NSItemProvider, "loadDataRepresentationForTypeIdentifier:completionHandler:", item_load);
    M(NSItemProvider, "registerDataRepresentationForTypeIdentifier:visibility:loadHandler:", noop);

    DEF(&NSAppleEventDescriptor_c, &NSAppleEventDescriptor_m, "NSAppleEventDescriptor", ob, BEX);
    CM(NSAppleEventDescriptor, "descriptorWithString:", aed_with_str);
    M(NSAppleEventDescriptor, "stringValue", aed_string);
    DEF(&NSAssertionHandler_c, &NSAssertionHandler_m, "NSAssertionHandler", ob, 0);
    CM(NSAssertionHandler, "currentHandler", assert_current);
    M(NSAssertionHandler, "handleFailureInMethod:object:file:lineNumber:description:", assert_fail);
    M(NSAssertionHandler, "handleFailureInFunction:file:lineNumber:description:", assert_fail);

    DEF(&NSMetadataItem_c, &NSMetadataItem_m, "NSMetadataItem", ob, BEX);
    CM(NSMetadataItem, "alloc", lock_alloc);
    M(NSMetadataItem, "valueForAttribute:", meta_value); M(NSMetadataItem, "attributes", ht_all);
    (void)meta_item;
    DEF(&NSMetadataQuery_c, &NSMetadataQuery_m, "NSMetadataQuery", ob, BEX);
    CM(NSMetadataQuery, "alloc", mq_alloc);
    M(NSMetadataQuery, "startQuery", mq_start); M(NSMetadataQuery, "stopQuery", mq_stop);
    M(NSMetadataQuery, "results", mq_results); M(NSMetadataQuery, "isStarted", mq_started);
    M(NSMetadataQuery, "setPredicate:", mq_set_pred); M(NSMetadataQuery, "setSearchScopes:", noop);
    cx_add_method(&NSMetadataQuery_c, "resultCount", (void *)mq_count);

    DEF(&NSLinguisticTagger_c, &NSLinguisticTagger_m, "NSLinguisticTagger", ob, BEX);
    CM(NSLinguisticTagger, "alloc", lock_alloc);
    M(NSLinguisticTagger, "initWithTagSchemes:options:", tagger_init);
    M(NSLinguisticTagger, "setString:", set_a); M(NSLinguisticTagger, "string", get_a);
    M(NSLinguisticTagger, "enumerateTagsInRange:scheme:options:usingBlock:", tagger_enum);

    DEF(&NSXMLParser_c, &NSXMLParser_m, "NSXMLParser", ob, BEX);
    CM(NSXMLParser, "alloc", lock_alloc);
    M(NSXMLParser, "initWithData:", xml_init_str); M(NSXMLParser, "initWithContentsOfURL:", xml_init_str);
    M(NSXMLParser, "setDelegate:", xml_set_del); M(NSXMLParser, "delegate", xml_del); M(NSXMLParser, "parse", xml_parse);
    { extern void cx_xmldom_init(void); cx_xmldom_init(); }   /* NSXMLDocument/NSXMLElement/NSXMLNode: xmldom.c */

    DEF(&NSXPCInterface_c, &NSXPCInterface_m, "NSXPCInterface", ob, BEX);
    CM(NSXPCInterface, "interfaceWithProtocol:", xpc_iface);
    DEF(&NSXPCListenerEndpoint_c, &NSXPCListenerEndpoint_m, "NSXPCListenerEndpoint", ob, BEX);
    DEF(&NSXPCConnection_c, &NSXPCConnection_m, "NSXPCConnection", ob, BEX);
    CM(NSXPCConnection, "alloc", lock_alloc);
    M(NSXPCConnection, "initWithServiceName:", xpc_init_name);
    M(NSXPCConnection, "initWithMachServiceName:options:", xpc_init_name);
    CM(NSXPCConnection, "connectionWithServiceName:", xpc_with);
    M(NSXPCConnection, "resume", xpc_resume); M(NSXPCConnection, "invalidate", xpc_inv); M(NSXPCConnection, "suspend", noop);
    M(NSXPCConnection, "setRemoteObjectInterface:", xpc_set_iface); M(NSXPCConnection, "remoteObjectProxy", xpc_proxy);
    M(NSXPCConnection, "setInterruptionHandler:", xpc_set_int); M(NSXPCConnection, "setInvalidationHandler:", xpc_set_int);
    DEF(&NSXPCListener_c, &NSXPCListener_m, "NSXPCListener", ob, BEX);
    CM(NSXPCListener, "serviceListener", xpc_listener); CM(NSXPCListener, "anonymousListener", xpc_listener);
    M(NSXPCListener, "setDelegate:", xpc_set_del); M(NSXPCListener, "resume", xpc_resume); M(NSXPCListener, "invalidate", xpc_inv);
    DEF(&NSMachBootstrapServer_c, &NSMachBootstrapServer_m, "NSMachBootstrapServer", ob, 0);
    CM(NSMachBootstrapServer, "sharedInstance", mach_boot);
    M(NSMachBootstrapServer, "servicePortWithName:", mach_lookup);
    DEF(&NSMachPort_c, &NSMachPort_m, "NSMachPort", ob, BEX);
    CM(NSMachPort, "port", port_new); M(NSMachPort, "machPort", port_mach);

    DEF(&NSNetService_c, &NSNetService_m, "NSNetService", ob, BEX);
    CM(NSNetService, "alloc", lock_alloc);
    M(NSNetService, "initWithDomain:type:name:port:", ns_with);
    M(NSNetService, "publish", ns_publish); M(NSNetService, "resolve", ns_publish); M(NSNetService, "stop", ns_stop);
    M(NSNetService, "setDelegate:", ns_set_del); M(NSNetService, "name", get_c); M(NSNetService, "type", get_b);
    DEF(&NSNetServiceBrowser_c, &NSNetServiceBrowser_m, "NSNetServiceBrowser", ob, BEX);
    CM(NSNetServiceBrowser, "alloc", lock_alloc);
    M(NSNetServiceBrowser, "searchForServicesOfType:inDomain:", ns_publish); M(NSNetServiceBrowser, "stop", ns_stop);
    M(NSNetServiceBrowser, "setDelegate:", ns_set_del);

    DEF(&NSValueTransformer_c, &NSValueTransformer_m, "NSValueTransformer", ob, BEX);
    CM(NSValueTransformer, "setValueTransformer:forName:", vt_set);
    CM(NSValueTransformer, "valueTransformerForName:", vt_for);
    M(NSValueTransformer, "transformedValue:", vt_fwd); M(NSValueTransformer, "reverseTransformedValue:", vt_fwd);
    DEF(&NSSecureUnarchiveFromDataTransformer_c, &NSSecureUnarchiveFromDataTransformer_m, "NSSecureUnarchiveFromDataTransformer", &NSValueTransformer_c, 0);
    M(NSSecureUnarchiveFromDataTransformer, "transformedValue:", sec_fwd);

    DEF(&NSBundleResourceRequest_c, &NSBundleResourceRequest_m, "NSBundleResourceRequest", ob, BEX);
    CM(NSBundleResourceRequest, "alloc", lock_alloc);
    M(NSBundleResourceRequest, "initWithTags:bundle:", resreq_init);
    M(NSBundleResourceRequest, "beginAccessingResourcesWithCompletionHandler:", resreq_begin);
    M(NSBundleResourceRequest, "endAccessingResources", noop);

    DEF(&NSFileCoordinator_c, &NSFileCoordinator_m, "NSFileCoordinator", ob, BEX);
    CM(NSFileCoordinator, "alloc", lock_alloc);
    M(NSFileCoordinator, "coordinateReadingItemAtURL:options:error:byAccessor:", coord_read);
    M(NSFileCoordinator, "coordinateWritingItemAtURL:options:error:byAccessor:", coord_read);

    DEF(&NSScriptClassDescription_c, &NSScriptClassDescription_m, "NSScriptClassDescription", ob, BEX);
    M(NSScriptClassDescription, "className", get_a); M(NSScriptClassDescription, "suiteName", get_b);
    DEF(&NSScriptCommand_c, &NSScriptCommand_m, "NSScriptCommand", ob, BEX);
    M(NSScriptCommand, "executeCommand", get_a); M(NSScriptCommand, "setDirectParameter:", set_a);
    DEF(&NSUniqueIDSpecifier_c, &NSUniqueIDSpecifier_m, "NSUniqueIDSpecifier", ob, BEX);
    M(NSUniqueIDSpecifier, "uniqueID", get_a); M(NSUniqueIDSpecifier, "initWithContainerClassDescription:containerSpecifier:key:uniqueID:", box_init);

    if (cx_class_named("NSAttributedString")) {
        DEF(&NSMutableAttributedString_c, &NSMutableAttributedString_m, "NSMutableAttributedString", &NSAttributedString_c, 0);
        M(NSMutableAttributedString, "replaceCharactersInRange:withString:", mas_replace);
        M(NSMutableAttributedString, "setAttributes:range:", mas_set_attr);
        M(NSMutableAttributedString, "addAttribute:value:range:", mas_set_attr);
        M(NSMutableAttributedString, "mutableString", get_a);
    }

    DEF(&NSPurgeableData_c, &NSPurgeableData_m, "NSPurgeableData", &NSData_c, 0);
    M(NSPurgeableData, "beginContentAccess", yes_m); M(NSPurgeableData, "endContentAccess", noop);
    M(NSPurgeableData, "discardContentIfPossible", noop);

    CM(NSData, "dataWithBase64EncodedString:options:", data_b64_new);
    M(NSData, "base64EncodedStringWithOptions:", data_b64);
    M(NSData, "initWithBase64EncodedString:options:", data_init_b64);
    cx_class_t *fm = cx_class_named("NSFileManager");
    if (fm) {
        cx_add_method(fm, "attributesOfItemAtPath:error:", (void *)fm_attrs);
    }
    (void)singleton; (void)box_init;
}
