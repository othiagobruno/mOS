/*
 * mOS — libSystem: SCDynamicStore (SystemConfiguration) sobre o configd (userland/configd.c).
 *
 * O store é um objeto CF de verdade (um NSMutableDictionary, como antes: o crate Rust system-configuration do VS Code
 * entra em pânico com NULL) e o estado de cada um fica numa tabela ao lado. Valores viajam como plist XML:
 * NSPropertyListSerialization na ida, cx_plist_parse na volta. As chaves de rede e proxies são as que o configd
 * publica a partir do kernel (State:/Network/Global/IPv4, .../DNS, .../Proxies...).
 * Notificações: SCDynamicStoreSetNotificationKeys registra no configd uma porta deste processo; uma thread recebe
 * os avisos e chama o callout pela fonte de run loop (SCDynamicStoreCreateRunLoopSource) ou pela fila
 * (SCDynamicStoreSetDispatchQueue), com o vetor das chaves que mudaram.
 */
#include "foundation.h"
#include "libc.h"
#include "../svcproto.h"

typedef void *id_;
int cx_svc_call(const char *svc, const svc_body_t *in, const void *s1, unsigned n1, const void *s2, unsigned n2,
                unsigned give_port, int give_disp, void *rbuf, unsigned rcap, unsigned timeout_ms);
id cx_plist_parse(const u8 *b, u64 n);
id objc_retain(id o);
void objc_release(id o);
cx_class_t *cx_class_named(const char *name);
void *CFRunLoopSourceCreate(void *alloc, long order, void *ctx);
void CFRunLoopSourceSignal(void *s);
void CFRunLoopWakeUp(void *rl);
void *CFRunLoopGetCurrent(void);
void *CFRunLoopGetMain(void);
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *));
int mach_port_allocate(unsigned task, int right, unsigned *name);
int mach_msg(void *msg, int option, unsigned send_size, unsigned rcv_size, unsigned rcv_name, unsigned timeout, unsigned notify);
int pthread_create(unsigned long *t, const void *attr, void *(*fn)(void *), void *arg);
int sched_yield(void);
int usleep(unsigned us);

#define kSCStatusOK 0
#define kSCStatusFailed 1001
#define kSCStatusInvalidArgument 1002
#define kSCStatusNoKey 1004
#define kSCStatusNoStoreServer 4004
static __thread int sc_err;
int SCError(void) { return sc_err; }
const char *SCErrorString(int status) {
    switch (status) {
    case kSCStatusOK: return "Success!";
    case kSCStatusNoKey: return "No such key";
    case kSCStatusInvalidArgument: return "Invalid argument";
    case kSCStatusNoStoreServer: return "Configuration daemon not (no longer) available";
    default: return "Failed!";
    }
}

/* ---------------------------------------------------------------- estado por store */
typedef struct {
    id_ store;
    void (*callout)(id_ store, id_ changed, void *info);
    void *info;
    unsigned port, id;
    void *source, *loop, *queue;
    id pending;                       /* NSMutableArray das chaves que mudaram, ainda não entregues */
    volatile int lk;
} scs_t;
#define NSTORE 32
static scs_t *stores[NSTORE];
static unsigned next_watch = 1;
static scs_t *scs_of(id_ s) { for (int i = 0; i < NSTORE; i++) if (stores[i] && stores[i]->store == s) return stores[i]; return 0; }

static id mdict(void) { return MSG0(id, MSG0(id, (id)cx_class_named("NSMutableDictionary"), "alloc"), "init"); }
static id marr(void) { return MSG0(id, MSG0(id, (id)cx_class_named("NSMutableArray"), "alloc"), "init"); }

id_ SCDynamicStoreCreate(void *alloc, id_ name, void *cb, void *ctx) {
    (void)alloc; (void)name;
    id s = mdict();
    scs_t *st = calloc(1, sizeof *st);
    if (st) {
        st->store = s;
        st->callout = (void (*)(id_, id_, void *))cb;
        if (ctx) st->info = ((void **)ctx)[1];     /* SCDynamicStoreContext: version, info, retain, release, desc */
        for (int i = 0; i < NSTORE; i++) if (!stores[i]) { stores[i] = st; break; }
    }
    return s;
}
id_ SCDynamicStoreCreateWithOptions(void *alloc, id_ name, id_ opts, void *cb, void *ctx) { (void)opts; return SCDynamicStoreCreate(alloc, name, cb, ctx); }
unsigned long SCDynamicStoreGetTypeID(void) { return 0x5c0; }

/* ---------------------------------------------------------------- valores */
static int sc_req(unsigned op, const char *k, const void *s2, unsigned n2, unsigned give, unsigned a, u8 **out, unsigned *on) {
    svc_body_t q = { op, a, 0, 0, 0, 0, 0 };
    u8 *r = malloc(SVC_MAX_MSG);
    if (!r) { sc_err = kSCStatusFailed; return -1; }
    int kr = cx_svc_call(CONFIGD_SERVICE, &q, k, k ? (unsigned)strlen(k) + 1 : 0, s2, n2, give, give ? 20 : 0, r, SVC_MAX_MSG, 5000);
    if (kr) { free(r); sc_err = kSCStatusNoStoreServer; return -1; }
    svc_body_t *b = (svc_body_t *)(r + 24);
    sc_err = b->op == 1003 ? kSCStatusNoKey : (int)b->op;
    if (b->op) { free(r); return -1; }
    if (out) {
        *out = malloc(b->n1 + 1);
        if (*out) { memcpy(*out, b + 1, b->n1); (*out)[b->n1] = 0; }
        if (on) *on = b->n1;
    }
    free(r);
    return 0;
}
id_ SCDynamicStoreCopyValue(id_ store, id_ key) {
    (void)store;
    const char *k = key ? cx_cstr(key) : 0;
    if (!k) { sc_err = kSCStatusInvalidArgument; return 0; }
    u8 *x = 0;
    unsigned n = 0;
    if (sc_req(CF_GET, k, 0, 0, 0, 0, &x, &n) != 0 || !x) return 0;
    id v = n ? cx_plist_parse(x, n) : 0;
    free(x);
    if (!v) sc_err = kSCStatusNoKey;
    return v ? objc_retain(v) : 0;
}
static int set_value(const char *k, id v) {
    id data = MSG(id, (id)cx_class_named("NSPropertyListSerialization"), "dataWithPropertyList:format:options:error:", v, (u64)100, (u64)0, (id *)0);
    if (!data) { sc_err = kSCStatusInvalidArgument; return 0; }
    const void *p = MSG0(const void *, data, "bytes");
    u64 n = MSG0(u64, data, "length");
    return sc_req(CF_SET, k, p, (unsigned)n, 0, 0, 0, 0) == 0;
}
u8 SCDynamicStoreSetValue(id_ store, id_ key, id_ value) { (void)store; const char *k = key ? cx_cstr(key) : 0; return k && value && set_value(k, value); }
u8 SCDynamicStoreAddValue(id_ store, id_ key, id_ value) {
    id_ old = SCDynamicStoreCopyValue(store, key);
    if (old) { objc_release(old); sc_err = 1005; return 0; }            /* kSCStatusKeyExists */
    return SCDynamicStoreSetValue(store, key, value);
}
u8 SCDynamicStoreAddTemporaryValue(id_ store, id_ key, id_ value) { return SCDynamicStoreAddValue(store, key, value); }
u8 SCDynamicStoreRemoveValue(id_ store, id_ key) { (void)store; const char *k = key ? cx_cstr(key) : 0; return k && sc_req(CF_REMOVE, k, 0, 0, 0, 0, 0, 0) == 0; }
u8 SCDynamicStoreNotifyValue(id_ store, id_ key) { (void)store; const char *k = key ? cx_cstr(key) : 0; return k && sc_req(CF_NOTIFY, k, 0, 0, 0, 0, 0, 0) == 0; }
u8 SCDynamicStoreSetMultiple(id_ store, id_ set, id_ remove, id_ notify) {
    u8 ok = 1;
    if (set) {
        id keys = MSG0(id, set, "allKeys");
        u64 n = keys ? MSG0(u64, keys, "count") : 0;
        for (u64 i = 0; i < n; i++) { id k = MSG(id, keys, "objectAtIndex:", i); ok &= SCDynamicStoreSetValue(store, k, MSG(id, set, "objectForKey:", k)); }
    }
    u64 n = remove ? MSG0(u64, remove, "count") : 0;
    for (u64 i = 0; i < n; i++) SCDynamicStoreRemoveValue(store, MSG(id, remove, "objectAtIndex:", i));
    n = notify ? MSG0(u64, notify, "count") : 0;
    for (u64 i = 0; i < n; i++) SCDynamicStoreNotifyValue(store, MSG(id, notify, "objectAtIndex:", i));
    return ok;
}
id_ SCDynamicStoreCopyKeyList(id_ store, id_ pattern) {
    (void)store;
    u8 *x = 0;
    if (sc_req(CF_LIST, pattern ? cx_cstr(pattern) : "", 0, 0, 0, 0, &x, 0) != 0) return 0;
    id a = marr();
    for (char *l = (char *)x; l && *l;) {
        char *e = strchr(l, '\n');
        if (e) *e = 0;
        if (*l) MSG(void, a, "addObject:", cx_str(l));
        l = e ? e + 1 : 0;
    }
    free(x);
    return a;
}
id_ SCDynamicStoreCopyMultiple(id_ store, id_ keys, id_ patterns) {
    id d = mdict();
    u64 n = keys ? MSG0(u64, keys, "count") : 0;
    for (u64 i = 0; i < n; i++) {
        id k = MSG(id, keys, "objectAtIndex:", i);
        id_ v = SCDynamicStoreCopyValue(store, k);
        if (v) { MSG(void, d, "setObject:forKey:", v, k); objc_release(v); }
    }
    n = patterns ? MSG0(u64, patterns, "count") : 0;
    for (u64 i = 0; i < n; i++) {
        id_ list = SCDynamicStoreCopyKeyList(store, MSG(id, patterns, "objectAtIndex:", i));
        u64 m = list ? MSG0(u64, list, "count") : 0;
        for (u64 j = 0; j < m; j++) {
            id k = MSG(id, list, "objectAtIndex:", j);
            id_ v = SCDynamicStoreCopyValue(store, k);
            if (v) { MSG(void, d, "setObject:forKey:", v, k); objc_release(v); }
        }
        if (list) objc_release(list);
    }
    return d;
}

/* proxies: State:/Network/Global/Proxies; sem configd, o padrão do macOS sem proxy */
static id default_proxies(void) {
    id d = mdict();
    id ex = marr();
    MSG(void, ex, "addObject:", cx_str("*.local"));
    MSG(void, ex, "addObject:", cx_str("169.254/16"));
    MSG(void, d, "setObject:forKey:", ex, cx_str("ExceptionsList"));
    MSG(void, d, "setObject:forKey:", cx_num_i(1), cx_str("FTPPassive"));
    MSG(void, d, "setObject:forKey:", cx_num_i(0), cx_str("HTTPEnable"));
    MSG(void, d, "setObject:forKey:", cx_num_i(0), cx_str("HTTPSEnable"));
    return d;
}
id_ SCDynamicStoreCopyProxies(id_ store) {
    id_ v = SCDynamicStoreCopyValue(store, cx_str("State:/Network/Global/Proxies"));
    return v ? v : default_proxies();
}
id_ CFNetworkCopySystemProxySettings(void) { return SCDynamicStoreCopyProxies(0); }

static id_ setup_string(const char *key, const char *field, const char *def) {
    id_ v = SCDynamicStoreCopyValue(0, cx_str(key));
    id s = v ? MSG(id, v, "objectForKey:", cx_str(field)) : 0;
    id r = s ? objc_retain(s) : objc_retain(cx_str(def));
    if (v) objc_release(v);
    return r;
}
id_ SCDynamicStoreCopyComputerName(id_ s, u32 *enc) { (void)s; if (enc) *enc = 0x08000100; return setup_string("Setup:/System", "ComputerName", "mOS"); }
id_ SCDynamicStoreCopyLocalHostName(id_ s) { (void)s; return setup_string("Setup:/Network/HostNames", "LocalHostName", "mos"); }

/* ---------------------------------------------------------------- chaves */
id_ SCDynamicStoreKeyCreate(void *alloc, id_ fmt, ...) {
    (void)alloc;
    va_list ap;
    va_start(ap, fmt);
    id s = cx_str_vfmt(fmt, ap);
    va_end(ap);
    return s ? objc_retain(s) : 0;
}
static id_ keyf(const char *a, id_ domain, const char *b, id_ x, const char *c, id_ y) {
    char buf[512];
    snprintf(buf, sizeof buf, "%s%s%s%s%s%s", a, domain ? cx_cstr(domain) : "State:", b, x ? cx_cstr(x) : "", c ? c : "", y ? cx_cstr(y) : "");
    return objc_retain(cx_str(buf));
}
id_ SCDynamicStoreKeyCreateNetworkGlobalEntity(void *alloc, id_ domain, id_ entity) { (void)alloc; return keyf("", domain, "/Network/Global/", entity, 0, 0); }
id_ SCDynamicStoreKeyCreateNetworkInterface(void *alloc, id_ domain) { (void)alloc; return keyf("", domain, "/Network/Interface", 0, 0, 0); }
id_ SCDynamicStoreKeyCreateNetworkInterfaceEntity(void *alloc, id_ domain, id_ ifname, id_ entity) {
    (void)alloc;
    return entity ? keyf("", domain, "/Network/Interface/", ifname, "/", entity) : keyf("", domain, "/Network/Interface/", ifname, 0, 0);
}
id_ SCDynamicStoreKeyCreateNetworkServiceEntity(void *alloc, id_ domain, id_ service, id_ entity) {
    (void)alloc;
    return entity ? keyf("", domain, "/Network/Service/", service, "/", entity) : keyf("", domain, "/Network/Service/", service, 0, 0);
}
id_ SCDynamicStoreKeyCreateComputerName(void *alloc) { (void)alloc; return objc_retain(cx_str("Setup:/System")); }
id_ SCDynamicStoreKeyCreateHostNames(void *alloc) { (void)alloc; return objc_retain(cx_str("Setup:/Network/HostNames")); }

/* ---------------------------------------------------------------- notificações */
static void deliver(scs_t *st) {
    while (__sync_lock_test_and_set(&st->lk, 1)) sched_yield();
    id keys = st->pending;
    st->pending = 0;
    __sync_lock_release(&st->lk);
    if (!keys) return;
    if (st->callout) st->callout(st->store, keys, st->info);
    objc_release(keys);
}
static void perform(void *info) { deliver(info); }
static void q_fn(void *info) { deliver(info); }
static void *listen_main(void *arg) {
    scs_t *st = arg;
    u8 *buf = malloc(4096);
    if (!buf) return 0;
    for (;;) {
        memset(buf, 0, 24);
        int kr = mach_msg(buf, 2, 0, 4096, st->port, 0, 0);
        if (kr) { if (kr != 0x10004004) usleep(10000); continue; }
        svc_body_t *b = (svc_body_t *)(buf + 24);
        if (*(unsigned *)(buf + 20) != SVC_NOTE_ID || b->n1 > 4000) continue;
        char *list = (char *)(b + 1);
        list[b->n1 ? b->n1 - 1 : 0] = 0;
        while (__sync_lock_test_and_set(&st->lk, 1)) sched_yield();
        if (!st->pending) st->pending = marr();
        for (char *l = list; l && *l;) {
            char *e = strchr(l, '\n');
            if (e) *e = 0;
            if (*l) MSG(void, st->pending, "addObject:", cx_str(l));
            l = e ? e + 1 : 0;
        }
        __sync_lock_release(&st->lk);
        if (st->queue) dispatch_async_f(st->queue, st, q_fn);
        else if (st->source) {
            CFRunLoopSourceSignal(st->source);
            if (st->loop) CFRunLoopWakeUp(st->loop);
            CFRunLoopWakeUp(CFRunLoopGetMain());
        }
    }
    return 0;
}
static void join_list(id arr, char *out, size_t cap) {
    size_t o = 0;
    out[0] = 0;
    u64 n = arr ? MSG0(u64, arr, "count") : 0;
    for (u64 i = 0; i < n; i++) {
        const char *s = cx_cstr(MSG(id, arr, "objectAtIndex:", i));
        size_t l = s ? strlen(s) : 0;
        if (!l || o + l + 2 >= cap) continue;
        memcpy(out + o, s, l); o += l; out[o++] = '\n'; out[o] = 0;
    }
}
u8 SCDynamicStoreSetNotificationKeys(id_ s, id_ keys, id_ patterns) {
    scs_t *st = scs_of(s);
    if (!st) return s != 0;
    if (!st->port) {
        if (mach_port_allocate(0x103, 1, &st->port) != 0) return 0;
        st->id = __sync_fetch_and_add(&next_watch, 1);
        unsigned long tid;
        pthread_create(&tid, 0, listen_main, st);
    }
    char k[1024], p[1024];
    join_list(keys, k, sizeof k);
    join_list(patterns, p, sizeof p);
    svc_body_t q = { CF_WATCH, st->id, 0, 0, 0, 0, 0 };
    u8 r[256];
    int kr = cx_svc_call(CONFIGD_SERVICE, &q, k, (unsigned)strlen(k) + 1, p, (unsigned)strlen(p) + 1, st->port, 20, r, sizeof r, 5000);
    if (kr) { sc_err = kSCStatusNoStoreServer; return 1; }      /* sem configd: aceita, como um store que nunca muda */
    return 1;
}
u8 SCDynamicStoreAddWatchedKey(id_ s, id_ key, u8 is_regex) {
    id a = marr();
    MSG(void, a, "addObject:", key);
    u8 r = is_regex ? SCDynamicStoreSetNotificationKeys(s, 0, a) : SCDynamicStoreSetNotificationKeys(s, a, 0);
    objc_release(a);
    return r;
}
id_ SCDynamicStoreCreateRunLoopSource(void *alloc, id_ s, long order) {
    if (!s) return 0;
    scs_t *st = scs_of(s);
    void *ctx[10] = { 0 };       /* CFRunLoopSourceContext versão 0 */
    ctx[1] = st;
    ctx[9] = st ? (void *)perform : 0;
    void *src = CFRunLoopSourceCreate(alloc, order, ctx);
    if (st) { st->source = src; st->loop = CFRunLoopGetCurrent(); }
    return (id_)src;
}
u8 SCDynamicStoreSetDispatchQueue(id_ s, void *queue) { scs_t *st = scs_of(s); if (st) st->queue = queue; return 1; }
id_ SCDynamicStoreCopyNotifiedKeys(id_ s) {
    scs_t *st = scs_of(s);
    id r = 0;
    if (st) { while (__sync_lock_test_and_set(&st->lk, 1)) sched_yield(); r = st->pending; st->pending = 0; __sync_lock_release(&st->lk); }
    return r ? r : marr();
}
