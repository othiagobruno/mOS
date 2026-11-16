/*
 * mOS — CFHost e NSHost (docs/rede-apps.md, item 9b).
 *
 * CFHost por nome, por endereço (CFData com sockaddr no layout do macOS) ou cópia. CFHostStartInfoResolution:
 *   kCFHostAddresses     getaddrinfo (A e AAAA);
 *   kCFHostNames         DNS reverso (PTR em in-addr.arpa/ip6.arpa pela libresolv do mOS) dos endereços; o host criado
 *                        por nome resolve os endereços antes;
 *   kCFHostReachability  CFData com SCNetworkReachabilityFlags (alcançável quando o nome resolve e há placa de pé).
 * Sem cliente, resolve na hora (bloqueia). Com cliente (CFHostSetClient), resolve num fio e o callback chega pelo run
 * loop agendado (CFRunLoopSource sinalizada), como no macOS; CFHostCancelInfoResolution descarta o resultado e o
 * callback não vem. Erros no domínio kCFStreamErrorDomainNetDB com o código do getaddrinfo.
 * NSHost: currentHost (nome da máquina e endereços das placas), hostWithName:, hostWithAddress: (IPv4 e IPv6, nomes por
 * DNS reverso), names, addresses, localizedName.
 */
#include "foundation.h"
#include "libc.h"
#include "mnet.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void *objc_autoreleasePoolPush(void);
void objc_autoreleasePoolPop(void *p);
void cx_object_free(id o);
id CFDataCreate(void *alloc, const u8 *bytes, long n);
const u8 *CFDataGetBytePtr(id d);
long CFDataGetLength(id d);
void *CFRunLoopSourceCreate(void *alloc, long order, void *ctx);
void CFRunLoopSourceSignal(void *s);
void CFRunLoopSourceInvalidate(void *s);
void CFRunLoopAddSource(void *rl, void *s, void *mode);
void CFRunLoopRemoveSource(void *rl, void *s, void *mode);
void CFRunLoopWakeUp(void *rl);
void CFRelease(const void *o);
int pthread_create(unsigned long *t, const void *attr, void *(*fn)(void *), void *arg);
int pthread_detach(unsigned long t);
void os_unfair_lock_lock(void *l);
void os_unfair_lock_unlock(void *l);
int gethostname(char *b, size_t n);
const char *inet_ntop(int af, const void *src, char *dst, unsigned n);
int inet_pton(int af, const char *src, void *dst);
int res_query(const char *name, int cls, int type, u8 *ans, int anslen);
typedef struct ifa { struct ifa *next; char *name; unsigned flags; u8 *addr, *netmask, *dst; void *data; } ifa_t;
int getifaddrs(ifa_t **out);
void freeifaddrs(ifa_t *p);
id SCDynamicStoreCopyComputerName(void *store, unsigned *enc);

extern const int kCFStreamErrorDomainNetDB;            /* SInt32 = 12 (cfnetstream.c) */

/* addrinfo do macOS: canonname vem antes de addr (sockets_inet.inc). */
typedef struct cf_ai { int flags, family, socktype, protocol; u32 addrlen; char *canonname; void *addr; struct cf_ai *next; } cf_ai;
int getaddrinfo(const char *host, const char *svc, const cf_ai *hints, cf_ai **res);
void freeaddrinfo(cf_ai *ai);

enum { T_ADDRS = 0, T_NAMES = 1, T_REACH = 2 };
enum { RF_REACHABLE = 1u << 1, RF_LOCAL = 1u << 16, RF_DIRECT = 1u << 17 };
typedef struct { long domain; int error; } cf_err;
typedef struct { long version; void *info; const void *(*retain)(const void *); void (*release)(const void *);
                 const void *(*copyDescription)(const void *); } cfhost_ctx;
typedef void (*cfhost_cb)(id host, long type, const cf_err *err, void *info);
typedef struct {
    long version; void *info; const void *(*retain)(const void *); void (*release)(const void *);
    void *copyDescription, *equal, *hash;
    void (*schedule)(void *, void *, void *); void (*cancel)(void *, void *, void *);
    void (*perform)(void *);
} src_ctx_t;
#define MAX_RL 8

typedef struct {
    unsigned lock;
    id self;                     /* sem posse */
    char *name;                  /* NULL no host criado por endereço */
    id name_str;
    id addrs, names, reach;      /* resultados (posse) */
    int has[3];
    cf_err err;
    cfhost_cb cb;
    void *info;
    void (*info_release)(const void *);
    void *src;
    void *rl[MAX_RL];
    int nrl;
    int busy, gen;               /* gen muda a cada start/cancel: resultado de geração velha é descartado */
    int pending;
    long done_type;
    cf_err done_err;
} cfhost_t;

typedef struct { id name, address, addresses, names; } nshost_t;

cx_class_t NSHost_c __asm__("OBJC_CLASS_$_NSHost");
cx_class_t NSHost_m __asm__("OBJC_METACLASS_$_NSHost");
static cx_class_t CFHost_c, CFHost_m;

static void *slot(id o) { return o ? *(void **)((u8 *)o + 8) : 0; }
static void set_slot(id o, void *p) { *(void **)((u8 *)o + 8) = p; }
static cfhost_t *H(id o) { return o && object_getClass(o) == &CFHost_c ? slot(o) : 0; }
static id new_array(void) { return MSG0(id, MSG0(id, (id)cx_class_named("NSMutableArray"), "alloc"), "init"); }
static void set_err(cf_err *e, long domain, int code) { if (e) { e->domain = domain; e->error = code; } }
static void store(id *p, id v) { id o = *p; *p = v; if (o) objc_release(o); }
static char *sdup(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *d = s ? malloc(n + 1) : 0;
    if (d) memcpy(d, s, n + 1);
    return d;
}

/* ---------------------------------------------------------------- endereços em texto */
static id addr_string(const u8 *p, long n) {
    char buf[64];
    if (!p || n < 8) return 0;
    if (p[1] == 2) { if (!inet_ntop(2, p + 4, buf, sizeof buf)) return 0; }
    else if (p[1] == 30 && n >= 24) { if (!inet_ntop(30, p + 8, buf, sizeof buf)) return 0; }   /* RFC 5952 */
    else return 0;
    return cx_str(buf);
}
/* texto -> sockaddr do macOS (sin 16 bytes, sin6 28 bytes), +1 */
static id sockaddr_from_text(const char *s) {
    u8 sa[28];
    memset(sa, 0, sizeof sa);
    if (s && inet_pton(2, s, sa + 4) == 1) { sa[0] = 16; sa[1] = 2; return CFDataCreate(0, sa, 16); }
    if (s && inet_pton(30, s, sa + 8) == 1) { sa[0] = 28; sa[1] = 30; return CFDataCreate(0, sa, 28); }
    return 0;
}

/* ---------------------------------------------------------------- resolução (sem trava; roda em qualquer fio) */
static int resolve_addrs(const char *name, id *out) {
    cf_ai hints, *res = 0;
    memset(&hints, 0, sizeof hints);
    hints.socktype = 1;
    *out = 0;
    int e = getaddrinfo(name, 0, &hints, &res);
    if (e || !res) { if (res) freeaddrinfo(res); return e ? e : 8; }
    id arr = new_array();
    int n = 0;
    for (cf_ai *ai = res; ai; ai = ai->next) {
        if (!ai->addr || ai->addrlen < 8) continue;
        int dup = 0;
        u64 cnt = MSG0(u64, arr, "count");
        for (u64 i = 0; i < cnt && !dup; i++) {
            id d = MSG(id, arr, "objectAtIndex:", i);
            dup = CFDataGetLength(d) == (long)ai->addrlen && !memcmp(CFDataGetBytePtr(d), ai->addr, ai->addrlen);
        }
        if (dup) continue;
        id d = CFDataCreate(0, ai->addr, (long)ai->addrlen);
        if (d) { MSG(void, arr, "addObject:", d); objc_release(d); n++; }
    }
    freeaddrinfo(res);
    if (!n) { objc_release(arr); return 8; }                          /* EAI_NONAME */
    *out = arr;
    return 0;
}
/* PTR de um sockaddr (nome e leitura da resposta no mnet_util.c) */
static int ptr_lookup(const u8 *sa, long n, char *out, int on) {
    char q[80];
    u8 ans[1024];
    if (mnet_dns_ptr_name(sa, (size_t)n, q, sizeof q)) return -1;
    int len = res_query(q, 1, 12, ans, sizeof ans);                   /* C_IN, T_PTR */
    return len < 12 ? -1 : mnet_dns_first_ptr(ans, (size_t)len, out, (size_t)on);
}
static id reverse_names(id addrs, const char *fallback) {          /* +1 */
    id arr = new_array();
    u64 n = addrs ? MSG0(u64, addrs, "count") : 0;
    for (u64 i = 0; i < n; i++) {
        id d = MSG(id, addrs, "objectAtIndex:", i);
        char nm[300];
        if (ptr_lookup(CFDataGetBytePtr(d), CFDataGetLength(d), nm, sizeof nm)) continue;
        id s = cx_str(nm);
        if (!MSG(BOOL, arr, "containsObject:", s)) MSG(void, arr, "addObject:", s);
    }
    if (!MSG0(u64, arr, "count") && fallback) MSG(void, arr, "addObject:", cx_str(fallback));
    return arr;
}
static int iface_up(void) {
    ifa_t *l = 0;
    int up = 0;
    if (getifaddrs(&l) == 0) {
        for (ifa_t *i = l; i; i = i->next)
            if ((i->flags & 1) && !(i->flags & 8) && i->addr && (i->addr[1] == 2 || i->addr[1] == 30)) up = 1;   /* IFF_UP, não loopback */
        freeifaddrs(l);
    }
    return up;
}
static int is_loopback(id addrs) {
    u64 n = addrs ? MSG0(u64, addrs, "count") : 0;
    for (u64 i = 0; i < n; i++) {
        const u8 *a = CFDataGetBytePtr(MSG(id, addrs, "objectAtIndex:", i));
        if (a[1] == 2 && a[4] == 127) return 1;
        if (a[1] == 30) { int z = 1; for (int k = 0; k < 15; k++) if (a[8 + k]) z = 0; if (z && a[23] == 1) return 1; }
    }
    return 0;
}
typedef struct { id addrs, names, reach; int has_addrs, has_names, has_reach; cf_err err; } result_t;
static void do_resolve(const char *name, id known_addrs, long type, result_t *r) {
    memset(r, 0, sizeof *r);
    id addrs = known_addrs ? objc_retain(known_addrs) : 0;
    int e = 0;
    if (!addrs && name) {
        e = resolve_addrs(name, &addrs);
        if (!e) r->has_addrs = 1;
    }
    if (e && type != T_REACH) { set_err(&r->err, kCFStreamErrorDomainNetDB, e); return; }
    if (type == T_ADDRS) { r->addrs = addrs; return; }
    if (type == T_NAMES) {
        r->names = reverse_names(addrs, known_addrs ? 0 : name);
        r->has_names = 1;
        if (r->has_addrs) r->addrs = addrs; else if (addrs) objc_release(addrs);
        if (!MSG0(u64, r->names, "count")) set_err(&r->err, kCFStreamErrorDomainNetDB, 8);
        return;
    }
    u32 flags = 0;
    if (addrs) flags = is_loopback(addrs) ? RF_REACHABLE | RF_LOCAL | RF_DIRECT : iface_up() ? RF_REACHABLE : 0;
    r->reach = CFDataCreate(0, (const u8 *)&flags, 4);
    r->has_reach = 1;
    if (r->has_addrs) r->addrs = addrs; else if (addrs) objc_release(addrs);
}
static void apply_result(cfhost_t *h, result_t *r) {                  /* com a trava; os resultados passam ao host */
    if (r->has_addrs) { store(&h->addrs, r->addrs); h->has[T_ADDRS] = 1; r->addrs = 0; }
    if (r->has_names) { store(&h->names, r->names); h->has[T_NAMES] = 1; r->names = 0; }
    if (r->has_reach) { store(&h->reach, r->reach); h->has[T_REACH] = 1; r->reach = 0; }
    h->err = r->err;
}
static void drop_result(result_t *r) {
    if (r->addrs) objc_release(r->addrs);
    if (r->names) objc_release(r->names);
    if (r->reach) objc_release(r->reach);
}

/* ---------------------------------------------------------------- entrega do callback */
static void perform(void *p) {
    cfhost_t *h = p;
    os_unfair_lock_lock(&h->lock);
    int go = h->pending;
    h->pending = 0;
    long type = h->done_type;
    cf_err err = h->done_err;
    cfhost_cb cb = h->cb;
    void *info = h->info;
    id self = h->self;
    if (go && self) objc_retain(self);
    os_unfair_lock_unlock(&h->lock);
    if (go && self) {
        if (cb) cb(self, type, &err, info);
        objc_release(self);
    }
}
static void signal_loops(cfhost_t *h) {                              /* com a trava */
    if (!h->src || !h->nrl) return;
    CFRunLoopSourceSignal(h->src);
    for (int i = 0; i < h->nrl; i++) CFRunLoopWakeUp(h->rl[i]);
}
typedef struct { id host; long type; int gen; char *name; id known; } job_t;
static void *resolve_thread(void *p) {
    job_t *j = p;
    void *pool = objc_autoreleasePoolPush();
    result_t r;
    do_resolve(j->name, j->known, j->type, &r);
    cfhost_t *h = slot(j->host);
    os_unfair_lock_lock(&h->lock);
    if (h->gen == j->gen) {
        apply_result(h, &r);
        h->busy = 0;
        h->done_type = j->type;
        h->done_err = r.err;
        h->pending = 1;
        signal_loops(h);
    }
    os_unfair_lock_unlock(&h->lock);
    drop_result(&r);
    if (j->known) objc_release(j->known);
    free(j->name);
    objc_autoreleasePoolPop(pool);
    objc_release(j->host);
    free(j);
    return 0;
}

/* ---------------------------------------------------------------- CFHost */
static void clear_client(cfhost_t *h) {
    if (h->info_release && h->info) h->info_release(h->info);
    h->info = 0; h->info_release = 0; h->cb = 0;
}
static void cfhost_dealloc(id self, SEL c) {
    (void)c;
    cfhost_t *h = slot(self);
    if (h) {
        os_unfair_lock_lock(&h->lock);
        h->self = 0;
        int nrl = h->nrl;
        void *rls[MAX_RL];
        memcpy(rls, h->rl, sizeof rls);
        h->nrl = 0;
        os_unfair_lock_unlock(&h->lock);
        for (int i = 0; i < nrl; i++) CFRunLoopRemoveSource(rls[i], h->src, 0);
        if (h->src) { CFRunLoopSourceInvalidate(h->src); CFRelease(h->src); }
        clear_client(h);
        if (h->name_str) objc_release(h->name_str);
        if (h->addrs) objc_release(h->addrs);
        if (h->names) objc_release(h->names);
        if (h->reach) objc_release(h->reach);
        free(h->name);
        free(h);
        set_slot(self, 0);
    }
    cx_object_free(self);
}
static id host_new(void) {
    if (!CFHost_c.vtable) return 0;
    id o = cx_alloc_instance(&CFHost_c);
    cfhost_t *h = o ? calloc(1, sizeof *h) : 0;
    if (!h) { if (o) objc_release(o); return 0; }
    h->self = o;
    set_slot(o, h);
    return o;
}
id CFHostCreateWithName(void *alloc, id name) {
    (void)alloc;
    const char *s = name ? cx_cstr(name) : 0;
    if (!s || !s[0]) return 0;
    id o = host_new();
    if (!o) return 0;
    cfhost_t *h = slot(o);
    h->name = sdup(s);
    if (!h->name) { objc_release(o); return 0; }
    h->name_str = objc_retain(name);
    return o;
}
id CFHostCreateWithAddress(void *alloc, id addr) {
    (void)alloc;
    long n = addr ? CFDataGetLength(addr) : 0;
    const u8 *p = addr ? CFDataGetBytePtr(addr) : 0;
    if (!p || n < 8 || (p[1] != 2 && p[1] != 30)) return 0;
    id o = host_new();
    if (!o) return 0;
    cfhost_t *h = slot(o);
    id copy = CFDataCreate(0, p, n);
    h->addrs = new_array();
    MSG(void, h->addrs, "addObject:", copy);
    objc_release(copy);
    h->has[T_ADDRS] = 1;
    return o;
}
id CFHostCreateCopy(void *alloc, id host) {
    cfhost_t *s = H(host);
    if (!s) return 0;
    id o = s->name_str ? CFHostCreateWithName(alloc, s->name_str) : host_new();
    cfhost_t *h = o ? slot(o) : 0;
    if (!h) return o;
    os_unfair_lock_lock(&s->lock);
    if (s->addrs) h->addrs = MSG0(id, s->addrs, "mutableCopy");
    if (s->names) h->names = MSG0(id, s->names, "mutableCopy");
    if (s->reach) h->reach = objc_retain(s->reach);
    memcpy(h->has, s->has, sizeof h->has);
    os_unfair_lock_unlock(&s->lock);
    return o;
}
u64 CFHostGetTypeID(void) { return 0x2a2; }

BOOL CFHostStartInfoResolution(id host, long type, cf_err *err) {
    cfhost_t *h = H(host);
    set_err(err, 0, 0);
    if (!h || type < 0 || type > 2) { set_err(err, kCFStreamErrorDomainNetDB, 4); return NO; }   /* EAI_FAIL */
    os_unfair_lock_lock(&h->lock);
    if (h->busy) { os_unfair_lock_unlock(&h->lock); set_err(err, kCFStreamErrorDomainNetDB, 4); return NO; }
    int async = h->cb != 0;
    char *name = sdup(h->name);
    id known = h->name ? 0 : (h->addrs ? objc_retain(h->addrs) : 0);
    int gen = ++h->gen;
    if (async) h->busy = 1;
    os_unfair_lock_unlock(&h->lock);
    if (!async) {
        result_t r;
        do_resolve(name, known, type, &r);
        os_unfair_lock_lock(&h->lock);
        apply_result(h, &r);
        os_unfair_lock_unlock(&h->lock);
        drop_result(&r);
        if (known) objc_release(known);
        free(name);
        if (err) *err = r.err;
        return r.err.domain == 0;
    }
    job_t *j = calloc(1, sizeof *j);
    unsigned long t;
    if (!j) goto fail;
    j->host = objc_retain(host); j->type = type; j->gen = gen; j->name = name; j->known = known;
    if (pthread_create(&t, 0, resolve_thread, j)) { objc_release(host); free(j); goto fail; }
    pthread_detach(t);
    return YES;
fail:
    free(name);
    if (known) objc_release(known);
    os_unfair_lock_lock(&h->lock);
    h->busy = 0;
    os_unfair_lock_unlock(&h->lock);
    set_err(err, kCFStreamErrorDomainNetDB, 4);
    return NO;
}
void CFHostCancelInfoResolution(id host, long type) {
    (void)type;
    cfhost_t *h = H(host);
    if (!h) return;
    os_unfair_lock_lock(&h->lock);
    h->gen++;                                                          /* o resultado em voo é descartado */
    h->busy = 0;
    h->pending = 0;
    os_unfair_lock_unlock(&h->lock);
}
static id get_result(id host, int t, BOOL *resolved) {
    cfhost_t *h = H(host);
    if (resolved) *resolved = NO;
    if (!h) return 0;
    os_unfair_lock_lock(&h->lock);
    id v = h->has[t] ? (t == T_ADDRS ? h->addrs : t == T_NAMES ? h->names : h->reach) : 0;
    if (h->has[t] && resolved) *resolved = YES;
    os_unfair_lock_unlock(&h->lock);
    return v;
}
id CFHostGetAddressing(id host, BOOL *resolved) { return get_result(host, T_ADDRS, resolved); }
id CFHostGetNames(id host, BOOL *resolved) {
    cfhost_t *h = H(host);
    if (h && h->name_str && !h->has[T_NAMES]) {                       /* o host por nome já sabe o próprio nome */
        if (resolved) *resolved = YES;
        os_unfair_lock_lock(&h->lock);
        if (!h->names) { h->names = new_array(); MSG(void, h->names, "addObject:", h->name_str); }
        id v = h->names;
        os_unfair_lock_unlock(&h->lock);
        return v;
    }
    return get_result(host, T_NAMES, resolved);
}
id CFHostGetReachability(id host, BOOL *resolved) { return get_result(host, T_REACH, resolved); }
id CFHostCopyNameString(id host) { cfhost_t *h = H(host); return h && h->name_str ? objc_retain(h->name_str) : 0; }

BOOL CFHostSetClient(id host, cfhost_cb cb, cfhost_ctx *ctx) {
    cfhost_t *h = H(host);
    if (!h) return NO;
    os_unfair_lock_lock(&h->lock);
    void *old = h->info;
    void (*rel)(const void *) = h->info_release;
    h->info = 0; h->info_release = 0; h->cb = cb;
    if (cb && ctx) {
        h->info = ctx->retain && ctx->info ? (void *)ctx->retain(ctx->info) : ctx->info;
        h->info_release = ctx->release;
    }
    if (!cb) { h->gen++; h->busy = 0; h->pending = 0; }               /* sem cliente: cancela a resolução em voo */
    os_unfair_lock_unlock(&h->lock);
    if (rel && old) rel(old);
    return YES;
}
void CFHostScheduleWithRunLoop(id host, void *rl, void *mode) {
    cfhost_t *h = H(host);
    if (!h || !rl) return;
    if (!h->src) {
        src_ctx_t c;
        memset(&c, 0, sizeof c);
        c.info = h; c.perform = perform;
        h->src = CFRunLoopSourceCreate(0, 0, &c);
    }
    os_unfair_lock_lock(&h->lock);
    int have = 0;
    for (int i = 0; i < h->nrl; i++) if (h->rl[i] == rl) have = 1;
    if (!have && h->nrl < MAX_RL) h->rl[h->nrl++] = rl;
    os_unfair_lock_unlock(&h->lock);
    if (!have) CFRunLoopAddSource(rl, h->src, mode);
    os_unfair_lock_lock(&h->lock);
    if (h->pending) signal_loops(h);                                  /* terminou antes de agendar */
    os_unfair_lock_unlock(&h->lock);
}
void CFHostUnscheduleFromRunLoop(id host, void *rl, void *mode) {
    cfhost_t *h = H(host);
    if (!h) return;
    int had = 0;
    os_unfair_lock_lock(&h->lock);
    for (int i = 0; i < h->nrl; i++) if (h->rl[i] == rl) { h->rl[i] = h->rl[--h->nrl]; had = 1; break; }
    os_unfair_lock_unlock(&h->lock);
    if (had) CFRunLoopRemoveSource(rl, h->src, mode);
}

/* ---------------------------------------------------------------- NSHost */
static id strings_from_datas(id datas, id *first) {                  /* +1 */
    id arr = new_array();
    if (first) *first = 0;
    u64 n = datas ? MSG0(u64, datas, "count") : 0;
    for (u64 i = 0; i < n; i++) {
        id d = MSG(id, datas, "objectAtIndex:", i);
        id s = addr_string(CFDataGetBytePtr(d), CFDataGetLength(d));
        if (!s) continue;
        MSG(void, arr, "addObject:", s);
        if (first && !*first) *first = objc_retain(s);
    }
    return arr;
}
static id nsh_new(id name, id names, id datas) {
    id o = cx_alloc_instance(&NSHost_c);
    nshost_t *b = o ? calloc(1, sizeof *b) : 0;
    if (!b) { if (o) objc_release(o); return 0; }
    b->name = name ? objc_retain(name) : 0;
    b->names = names ? objc_retain(names) : 0;
    b->addresses = strings_from_datas(datas, &b->address);
    set_slot(o, b);
    return o;
}
static void nsh_dealloc(id self, SEL c) {
    (void)c;
    nshost_t *b = slot(self);
    if (b) {
        if (b->name) objc_release(b->name);
        if (b->address) objc_release(b->address);
        if (b->addresses) objc_release(b->addresses);
        if (b->names) objc_release(b->names);
        free(b);
        set_slot(self, 0);
    }
    cx_object_free(self);
}
static id nsh_current(id cls, SEL c) {
    (void)cls; (void)c;
    static id one;
    static unsigned lk;
    os_unfair_lock_lock(&lk);
    if (!one) {
        char buf[256] = "";
        if (gethostname(buf, sizeof buf) != 0 || !buf[0]) memcpy(buf, "mos", 4);
        buf[sizeof buf - 1] = 0;
        id datas = new_array();
        ifa_t *l = 0;
        if (getifaddrs(&l) == 0) {                                     /* endereços das placas, loopback incluso */
            for (ifa_t *i = l; i; i = i->next) {
                if (!i->addr || (i->addr[1] != 2 && i->addr[1] != 30)) continue;
                id d = CFDataCreate(0, i->addr, i->addr[1] == 2 ? 16 : 28);
                if (d) { MSG(void, datas, "addObject:", d); objc_release(d); }
            }
            freeifaddrs(l);
        }
        if (!MSG0(u64, datas, "count")) {
            u8 sin[16] = {16, 2, 0, 0, 127, 0, 0, 1};
            id d = CFDataCreate(0, sin, 16);
            MSG(void, datas, "addObject:", d);
            objc_release(d);
        }
        id name = cx_str(buf);
        id names = new_array();
        MSG(void, names, "addObject:", name);
        if (strcmp(buf, "localhost")) MSG(void, names, "addObject:", cx_str("localhost"));
        one = nsh_new(name, names, datas);
        objc_release(names);
        objc_release(datas);
    }
    os_unfair_lock_unlock(&lk);
    return objc_autorelease(objc_retain(one));
}
static id nsh_with_name(id cls, SEL c, id name) {
    (void)cls; (void)c;
    const char *s = name ? cx_cstr(name) : 0;
    if (!s || !*s) return 0;
    id datas = 0;
    if (resolve_addrs(s, &datas)) return 0;                            /* o macOS devolve nil para nome que não resolve */
    id names = new_array();
    MSG(void, names, "addObject:", name);
    id o = nsh_new(name, names, datas);
    objc_release(names);
    objc_release(datas);
    return o ? objc_autorelease(o) : 0;
}
static id nsh_with_addr(id cls, SEL c, id addr) {
    (void)cls; (void)c;
    const char *s = addr ? cx_cstr(addr) : 0;
    id sa = s ? sockaddr_from_text(s) : 0;
    if (!sa) return 0;
    id datas = new_array();
    MSG(void, datas, "addObject:", sa);
    objc_release(sa);
    id names = reverse_names(datas, 0);
    id name = MSG0(u64, names, "count") ? MSG(id, names, "objectAtIndex:", (u64)0) : 0;
    id o = nsh_new(name, names, datas);
    objc_release(names);
    objc_release(datas);
    return o ? objc_autorelease(o) : 0;
}
static id nsh_name(id self, SEL c) { (void)c; nshost_t *b = slot(self); return b ? (b->name ? b->name : b->address) : 0; }
static id nsh_address(id self, SEL c) { (void)c; nshost_t *b = slot(self); return b ? b->address : 0; }
static id nsh_addresses(id self, SEL c) { (void)c; nshost_t *b = slot(self); return b ? b->addresses : 0; }
static id nsh_names(id self, SEL c) {
    (void)c;
    nshost_t *b = slot(self);
    if (!b) return 0;
    if (!b->names) b->names = new_array();
    return b->names;
}
static id nsh_localized(id self, SEL c) {
    if (self == nsh_current(0, 0)) {                                   /* nome do computador (SystemConfiguration) */
        id cn = SCDynamicStoreCopyComputerName(0, 0);
        if (cn) return objc_autorelease(cn);
    }
    return nsh_name(self, c);
}
static BOOL nsh_eq(id self, SEL c, id other) {
    (void)c;
    if (self == other) return YES;
    if (!other || object_getClass(other) != &NSHost_c) return NO;
    nshost_t *a = slot(self), *b = slot(other);
    if (!a || !b) return NO;
    u64 n = a->addresses ? MSG0(u64, a->addresses, "count") : 0;     /* iguais se têm algum endereço em comum */
    for (u64 i = 0; i < n; i++) {
        id s = MSG(id, a->addresses, "objectAtIndex:", i);
        if (b->addresses && MSG(BOOL, b->addresses, "containsObject:", s)) return YES;
    }
    return a->name && b->name && MSG(BOOL, a->name, "isEqual:", b->name);
}

void cx_cfhost_init(void) {
    static int once;
    if (once) return;
    cx_class_t *ns = cx_class_named("NSObject");
    if (!ns) return;
    once = 1;
    cx_define_class(&CFHost_c, &CFHost_m, "CFHost", ns, 8);
    cx_add_method(&CFHost_c, "dealloc", (void *)cfhost_dealloc);
    cx_define_class(&NSHost_c, &NSHost_m, "NSHost", ns, 8);
    cx_add_cmethod(&NSHost_c, "currentHost", (void *)nsh_current);
    cx_add_cmethod(&NSHost_c, "hostWithName:", (void *)nsh_with_name);
    cx_add_cmethod(&NSHost_c, "hostWithAddress:", (void *)nsh_with_addr);
    cx_add_method(&NSHost_c, "dealloc", (void *)nsh_dealloc);
    cx_add_method(&NSHost_c, "name", (void *)nsh_name);
    cx_add_method(&NSHost_c, "address", (void *)nsh_address);
    cx_add_method(&NSHost_c, "addresses", (void *)nsh_addresses);
    cx_add_method(&NSHost_c, "names", (void *)nsh_names);
    cx_add_method(&NSHost_c, "localizedName", (void *)nsh_localized);
    cx_add_method(&NSHost_c, "isEqualToHost:", (void *)nsh_eq);
}
