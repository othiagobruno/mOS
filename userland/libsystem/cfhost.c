/*
 * mOS libSystem — CFHost e NSHost para o Postico 2.3.5.
 *
 * O binário importa CFHostCreateWithName, StartInfoResolution, GetAddressing, SetClient,
 * ScheduleWithRunLoop, UnscheduleFromRunLoop, CancelInfoResolution e OBJC_CLASS_$_NSHost
 * (currentHost, name, address, localizedName). As constantes kCFStreamErrorDomainNetDB,
 * kCFStreamErrorDomainNetServices e kCFGetAddrInfoFailureKey ficam em postico_cf.c;
 * kCFErrorDomainCFNetwork fica em vscode_consts.c.
 *
 * Funcional: resolução síncrona por getaddrinfo (A e AAAA, sockaddr no layout do macOS),
 * callback do cliente só se houver SetClient e Schedule, NSHost com nome e endereços em texto.
 * Limitação: não há fila de DNS nem SCNetworkReachability; um run loop por host; o callback
 * entra por CFRunLoopPerformBlock (na hora, se o loop desta thread não está rodando).
 * kCFHostReachability só resolve o nome. localizedName é o hostname. hostWithAddress: não
 * faz DNS reverso. O texto IPv6 não comprime zeros.
 */
#include "foundation.h"

void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void cx_object_free(id o);

id CFDataCreate(void *alloc, const u8 *bytes, long n);
const u8 *CFDataGetBytePtr(id d);
long CFDataGetLength(id d);
void *CFRunLoopGetCurrent(void);
void CFRunLoopPerformBlock(void *rl, void *mode, void *block);

extern const void *kCFStreamErrorDomainNetDB;
int gethostname(char *b, size_t n);

/* addrinfo do macOS: canonname vem antes de addr (sockets_inet.inc). */
typedef struct cf_ai {
    int flags, family, socktype, protocol;
    u32 addrlen;
    char *canonname;
    void *addr;
    struct cf_ai *next;
} cf_ai;
int getaddrinfo(const char *host, const char *svc, const cf_ai *hints, cf_ai **res);
void freeaddrinfo(cf_ai *ai);

typedef struct { long domain; int error; } cf_err;
typedef struct {
    long version;
    void *info;
    const void *(*retain)(const void *);
    void (*release)(const void *);
    const void *(*copyDescription)(const void *);
} cfhost_ctx;
typedef void (*cfhost_cb)(id host, long type, const cf_err *err, void *info);

typedef struct {
    void *isa;
    int flags, reserved;
    void (*invoke)(void *);
    void *desc;
    id host;
} cfhost_blk;

typedef struct {
    char *name;
    id name_str;
    id addrs;
    int did, cancel, scheduled;
    long last_type;
    cf_err err;
    cfhost_cb cb;
    void *info;
    void (*info_release)(const void *);
    void *loop, *mode;
    cfhost_blk blk;
} cfhost_t;

typedef struct {
    id name, address, addresses, names;
} nshost_t;

cx_class_t NSHost_c __asm__("OBJC_CLASS_$_NSHost");
cx_class_t NSHost_m __asm__("OBJC_METACLASS_$_NSHost");
static cx_class_t CFHost_c, CFHost_m;

static void *slot(id o) { return o ? *(void **)((u8 *)o + 8) : 0; }
static void set_slot(id o, void *p) { *(void **)((u8 *)o + 8) = p; }

static id new_array(void) {
    id cls = (id)cx_class_named("NSMutableArray");
    if (!cls) return 0;
    id a = MSG0(id, cls, "alloc");
    return a ? MSG0(id, a, "init") : 0;
}

static void set_err(cf_err *e, long domain, int code) {
    if (!e) return;
    e->domain = domain;
    e->error = code;
}

static int parse_v4(const char *s, unsigned *o) {
    if (!s) return 0;
    unsigned v[4];
    int part = 0;
    while (part < 4) {
        if (*s < '0' || *s > '9') return 0;
        unsigned n = 0;
        int digits = 0;
        while (*s >= '0' && *s <= '9') {
            n = n * 10u + (unsigned)(*s - '0');
            if (n > 255 || ++digits > 3) return 0;
            s++;
        }
        v[part++] = n;
        if (part < 4) {
            if (*s != '.') return 0;
            s++;
        }
    }
    if (*s) return 0;
    o[0] = v[0];
    o[1] = v[1];
    o[2] = v[2];
    o[3] = v[3];
    return 1;
}

/* texto do sockaddr. IPv6 sai com os oito grupos, sem comprimir :: (limitação). */
static id addr_string(const u8 *p, long n) {
    char buf[80];
    if (!p || n < 8) return 0;
    if (p[1] == 2)
        snprintf(buf, sizeof buf, "%u.%u.%u.%u", p[4], p[5], p[6], p[7]);
    else if (p[1] == 30 && n >= 24) {
        int o = 0;
        for (int i = 0; i < 8 && o < (int)sizeof buf - 6; i++) {
            unsigned v = (unsigned)p[8 + i * 2] << 8 | p[9 + i * 2];
            int k = snprintf(buf + o, sizeof buf - (size_t)o, i ? ":%x" : "%x", v);
            if (k < 0) return 0;
            o += k;
        }
    } else return 0;
    return cx_str(buf);
}

static int resolve_name(const char *name, id *out) {
    cf_ai hints, *res = 0;
    memset(&hints, 0, sizeof hints);
    hints.socktype = 1;
    *out = 0;
    int e = getaddrinfo(name, "0", &hints, &res);
    if (e || !res) {
        freeaddrinfo(res);
        return e ? e : 8;
    }
    id arr = new_array();
    int n = 0;
    for (cf_ai *ai = res; ai; ai = ai->next) {
        if (!arr || !ai->addr || ai->addrlen < 8) continue;
        id d = CFDataCreate(0, ai->addr, (long)ai->addrlen);
        if (!d) continue;
        MSG(void, arr, "addObject:", d);
        objc_release(d);
        n++;
    }
    freeaddrinfo(res);
    if (!n) {
        objc_release(arr);
        return 8;
    }
    *out = arr;
    return 0;
}

static void clear_client(cfhost_t *h) {
    if (!h) return;
    if (h->info_release && h->info) h->info_release(h->info);
    h->info = 0;
    h->info_release = 0;
    h->cb = 0;
}

static void cfhost_invoke(void *p) {
    cfhost_blk *b = p;
    id host = b ? b->host : 0;
    if (!host) return;
    objc_retain(host);
    cfhost_t *h = slot(host);
    if (h && h->cb && h->scheduled) h->cb(host, h->last_type, &h->err, h->info);
    objc_release(host);
    objc_release(host); /* o retain feito ao enfileirar o bloco */
}

static void cfhost_dealloc(id self, SEL c) {
    (void)c;
    cfhost_t *h = slot(self);
    if (h) {
        clear_client(h);
        objc_release(h->name_str);
        objc_release(h->addrs);
        free(h->name);
        free(h);
        set_slot(self, 0);
    }
    cx_object_free(self);
}

id CFHostCreateWithName(void *alloc, id name) {
    (void)alloc;
    if (!name) return 0;
    const char *s = cx_cstr(name);
    if (!s || !s[0]) return 0;
    cx_class_t *ns = cx_class_named("NSObject");
    if (!ns || !CFHost_c.vtable) return 0;
    id o = cx_alloc_instance(&CFHost_c);
    if (!o) return 0;
    cfhost_t *h = calloc(1, sizeof *h);
    size_t n = strlen(s);
    h->name = malloc(n + 1);
    if (!h->name) { free(h); objc_release(o); return 0; }
    memcpy(h->name, s, n + 1);
    h->name_str = objc_retain(name);
    h->blk.flags = 1 << 28;
    h->blk.invoke = cfhost_invoke;
    h->blk.host = o;
    set_slot(o, h);
    return o;
}

BOOL CFHostStartInfoResolution(id host, long info, cf_err *err) {
    cfhost_t *h = slot(host);
    if (!h) return NO;
    objc_retain(host);
    if (h->cancel) {
        h->cancel = 0;
        set_err(err, (long)kCFStreamErrorDomainNetDB, 1);
        objc_release(host);
        return NO;
    }
    h->last_type = info;
    objc_release(h->addrs);
    h->addrs = 0;
    int e = resolve_name(h->name, &h->addrs);
    h->did = 1;
    if (e) {
        h->err.domain = (long)kCFStreamErrorDomainNetDB;
        h->err.error = e;
        set_err(err, h->err.domain, e);
    } else {
        h->err.domain = 0;
        h->err.error = 0;
        set_err(err, 0, 0);
    }
    if (h->cb && h->scheduled) {
        void *loop = h->loop ? h->loop : CFRunLoopGetCurrent();
        objc_retain(host);
        CFRunLoopPerformBlock(loop, h->mode, &h->blk);
    }
    BOOL ok = e == 0;
    objc_release(host);
    return ok;
}

id CFHostGetAddressing(id host, BOOL *resolved) {
    cfhost_t *h = slot(host);
    if (!h || !h->did) {
        if (resolved) *resolved = NO;
        return 0;
    }
    if (resolved) *resolved = YES;
    return h->addrs;
}

BOOL CFHostSetClient(id host, cfhost_cb cb, cfhost_ctx *ctx) {
    cfhost_t *h = slot(host);
    if (!h) return NO;
    clear_client(h);
    if (!cb) return YES;
    h->cb = cb;
    if (ctx) {
        h->info = ctx->retain ? (void *)ctx->retain(ctx->info) : ctx->info;
        h->info_release = ctx->release;
    }
    return YES;
}

void CFHostScheduleWithRunLoop(id host, void *rl, void *mode) {
    cfhost_t *h = slot(host);
    if (!h) return;
    h->loop = rl ? rl : CFRunLoopGetCurrent();
    h->mode = mode;
    h->scheduled = 1;
}

void CFHostUnscheduleFromRunLoop(id host, void *rl, void *mode) {
    (void)mode;
    cfhost_t *h = slot(host);
    if (!h) return;
    if (!rl || h->loop == rl) {
        h->scheduled = 0;
        h->loop = 0;
        h->mode = 0;
    }
}

void CFHostCancelInfoResolution(id host, long info) {
    (void)info;
    cfhost_t *h = slot(host);
    if (h) h->cancel = 1;
}

static id strings_from_datas(id datas, id *first) {
    id arr = new_array();
    if (first) *first = 0;
    if (!datas || !arr) return arr;
    u64 n = MSG0(u64, datas, "count");
    for (u64 i = 0; i < n; i++) {
        id d = MSG(id, datas, "objectAtIndex:", i);
        id s = addr_string(CFDataGetBytePtr(d), CFDataGetLength(d));
        if (!s) continue;
        MSG(void, arr, "addObject:", s);
        if (first && !*first) *first = objc_retain(s);
        objc_release(s);
    }
    return arr;
}

static id nsh_new(id name, id datas) {
    id o = cx_alloc_instance(&NSHost_c);
    if (!o) return 0;
    nshost_t *b = calloc(1, sizeof *b);
    b->name = name ? objc_retain(name) : 0;
    b->addresses = strings_from_datas(datas, &b->address);
    set_slot(o, b);
    return o;
}

static void nsh_dealloc(id self, SEL c) {
    (void)c;
    nshost_t *b = slot(self);
    if (b) {
        objc_release(b->name);
        objc_release(b->address);
        objc_release(b->addresses);
        objc_release(b->names);
        free(b);
        set_slot(self, 0);
    }
    cx_object_free(self);
}

static id nsh_current(id cls, SEL c) {
    (void)cls;
    (void)c;
    static id one;
    if (!one) {
        char buf[256];
        buf[0] = 0;
        if (gethostname(buf, sizeof buf) != 0) buf[0] = 0;
        buf[sizeof buf - 1] = 0;
        if (!buf[0]) memcpy(buf, "mos", 4);
        id name = cx_str(buf);
        u8 sin[16] = {16, 2, 0, 0, 127, 0, 0, 1};
        id data = CFDataCreate(0, sin, 16);
        id arr = new_array();
        if (arr && data) MSG(void, arr, "addObject:", data);
        one = nsh_new(name, arr);
        objc_release(name);
        objc_release(data);
        objc_release(arr);
    }
    return objc_autorelease(objc_retain(one));
}

static id nsh_with_name(id cls, SEL c, id name) {
    (void)cls;
    (void)c;
    if (!name) return 0;
    id host = CFHostCreateWithName(0, name);
    if (!host) return 0;
    cf_err err;
    BOOL ok = CFHostStartInfoResolution(host, 0, &err);
    BOOL resolved = NO;
    id datas = ok ? CFHostGetAddressing(host, &resolved) : 0;
    id o = nsh_new(name, datas);
    objc_release(host);
    return objc_autorelease(o);
}

static id nsh_with_addr(id cls, SEL c, id addr) {
    (void)cls;
    (void)c;
    if (!addr) return 0;
    const char *s = cx_cstr(addr);
    unsigned v[4];
    id datas = 0;
    id shown = objc_retain(addr);
    if (parse_v4(s, v)) {
        u8 sin[16] = {16, 2, 0, 0, (u8)v[0], (u8)v[1], (u8)v[2], (u8)v[3]};
        char buf[32];
        snprintf(buf, sizeof buf, "%u.%u.%u.%u", v[0], v[1], v[2], v[3]);
        objc_release(shown);
        shown = cx_str(buf);
        id data = CFDataCreate(0, sin, 16);
        datas = new_array();
        if (datas && data) MSG(void, datas, "addObject:", data);
        objc_release(data);
    }
    id o = nsh_new(shown, datas);
    objc_release(shown);
    objc_release(datas);
    return objc_autorelease(o);
}

static id nsh_name(id self, SEL c) { (void)c; nshost_t *b = slot(self); return b ? b->name : 0; }
static id nsh_address(id self, SEL c) { (void)c; nshost_t *b = slot(self); return b ? b->address : 0; }
static id nsh_addresses(id self, SEL c) { (void)c; nshost_t *b = slot(self); return b ? b->addresses : 0; }
static id nsh_localized(id self, SEL c) { return nsh_name(self, c); }
static id nsh_names(id self, SEL c) {
    (void)c;
    nshost_t *b = slot(self);
    if (!b) return 0;
    if (!b->names) {
        b->names = new_array();
        if (b->names && b->name) MSG(void, b->names, "addObject:", b->name);
    }
    return b->names;
}
static BOOL nsh_eq(id self, SEL c, id other) {
    (void)c;
    if (self == other) return YES;
    if (!other || object_getClass(other) != &NSHost_c) return NO;
    nshost_t *a = slot(self), *b = slot(other);
    if (!a || !b) return NO;
    if (a->address && b->address) return MSG(BOOL, a->address, "isEqual:", b->address);
    if (a->name && b->name) return MSG(BOOL, a->name, "isEqual:", b->name);
    return NO;
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
