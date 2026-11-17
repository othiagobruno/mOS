/*
 * mOS libSystem — funções de CoreFoundation/Foundation que os apps do corpus importam e faltavam
 * (docs/lacunas/eduardo.md):
 *  - CFMachPort real: porta Mach com direito de recepção, uma thread por porta recebe as mensagens e o callout
 *    roda no run loop (fonte versão 0, como o CFSocket de fdn_cfsocket.c); invalidação com callback;
 *  - CFPlugIn: fábricas e tipos do Info.plist (CFPlugInFactories/CFPlugInTypes), fábricas registradas pelo app,
 *    CFPlugInInstanceCreate chama a função da fábrica, contagem de instâncias por fábrica;
 *  - CFURL: escapes de porcentagem com codificação, partes (usuário, senha, fragmento, parâmetros, porta, net
 *    location, caminho estrito, extensão), cópias com/sem extensão, relativo a base, dados e recurso;
 *  - NSMapTable em C (NSCreateMapTable…, enumeração) sobre um NSMapTable de ponteiros crus com callbacks;
 *  - CFNumberGetByteSize, kCFNumberNaN/±Infinity, CFPropertyList XML/stream, Format…AndArguments, Pascal NoCopy,
 *    CFUUIDCreateWithBytes, CFErrorCreateWithUserInfoKeysAndValues, CFDataFind, CFRunLoopCopyAllModes,
 *    CFNotificationCenterPostNotificationWithOptions, CFPreferencesRemoveSuitePreferencesFromApp,
 *    CFLocaleGetWindowsLocaleCodeFromLocaleIdentifier, NSIntegralRectWithOptions, NSEdgeInsetsEqual,
 *    NSOpenStepRootDirectory, NSZone*, NSZombieEnabled/NSDeallocateZombies, _NSSetLogCStringFunction.
 */
#include "foundation.h"
#include "libc.h"
#include "fdn_encoding_core.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
id cx_str_n(const char *s, u64 n);
id cx_str_vfmt(id fmt, va_list ap);
void cx_object_free(id o);
int pthread_create(unsigned long *t, const void *attr, void *(*fn)(void *), void *arg);
int mach_msg(void *msg, int option, u32 send_size, u32 rcv_size, u32 rcv_name, u32 timeout, u32 notify);
int mach_port_allocate(u32 task, u32 right, u32 *name);
int mach_port_insert_right(u32 task, u32 name, u32 poly, u32 poly_type);
int mach_port_mod_refs(u32 task, u32 name, u32 right, int delta);
int mach_port_deallocate(u32 task, u32 name);
extern unsigned int mach_task_self_;
void *CFRunLoopSourceCreate(void *alloc, long order, void *ctx);
void CFRunLoopSourceSignal(void *s);
void CFRunLoopSourceInvalidate(void *s);
void CFRunLoopWakeUp(void *rl);

typedef long CFIndex;
typedef struct { CFIndex loc, len; } CFRange;
static id cls(const char *n) { return (id)cx_class_named(n); }
static const char *C(id s) { const char *p = s ? MSG0(const char *, s, "UTF8String") : 0; return p ? p : ""; }

/* ================================================================ CFMachPort */
typedef struct { long version; void *info; const void *(*retain)(const void *); void (*release)(const void *); void *desc; } cfctx_t;
typedef struct {
    long version; void *info; const void *(*retain)(const void *); void (*release)(const void *);
    void *copyDescription, *equal, *hash;
    void (*schedule)(void *, void *, void *); void (*cancel)(void *, void *, void *);
    void (*perform)(void *);
} srcctx_t;
typedef void (*mp_callout)(void *port, void *msg, CFIndex size, void *info);
typedef void (*mp_invcb)(void *port, void *info);
typedef struct qmsg { struct qmsg *next; u32 size; u8 data[]; } qmsg_t;
typedef struct mport {
    cx_class_t *isa;
    u32 port;
    int valid, owns_receive, thread_started;
    mp_callout callout;
    mp_invcb inv;
    cfctx_t ctx;
    void *source;
    void *loops[8];
    int nloops;
    volatile int lock;
    qmsg_t *head, *tail;
} mport_t;
static cx_class_t mp_c, mp_m;
static mport_t *g_ports[256];
static volatile int g_plock;
static void lk(volatile int *l) { while (__atomic_exchange_n(l, 1, __ATOMIC_ACQUIRE)) { } }
static void ulk(volatile int *l) { __atomic_store_n(l, 0, __ATOMIC_RELEASE); }

static void *mp_recv_thread(void *arg) {
    mport_t *o = arg;
    u32 cap = 64 * 1024;
    u8 *buf = malloc(cap);
    while (o->valid) {
        memset(buf, 0, 24);
        /* MACH_RCV_MSG (2) | MACH_RCV_LARGE (4): mensagem maior que o buffer volta com o tamanho, e cresce */
        int r = mach_msg(buf, 2 | 4, 0, cap, o->port, 0, 0);
        if (r == 0x10004004 /* MACH_RCV_TOO_LARGE */) {
            u32 need = ((u32 *)buf)[1] + 64;
            if (need > cap) { cap = need; buf = realloc(buf, cap); }
            continue;
        }
        if (r != 0) { if (!o->valid) break; continue; }
        u32 sz = ((u32 *)buf)[1];
        if (sz > cap) sz = cap;
        qmsg_t *q = malloc(sizeof *q + sz);
        q->next = 0; q->size = sz;
        memcpy(q->data, buf, sz);
        lk(&o->lock);
        if (o->tail) o->tail->next = q; else o->head = q;
        o->tail = q;
        void *src = o->source;
        int n = o->nloops;
        void *loops[8];
        memcpy(loops, o->loops, sizeof loops);
        ulk(&o->lock);
        if (src) { CFRunLoopSourceSignal(src); for (int i = 0; i < n; i++) CFRunLoopWakeUp(loops[i]); }
    }
    free(buf);
    objc_release((id)o);
    return 0;
}
static void mp_start(mport_t *o) {
    if (o->thread_started || !o->valid) return;
    o->thread_started = 1;
    objc_retain((id)o);
    unsigned long t;
    pthread_create(&t, 0, mp_recv_thread, o);
}
static void mp_schedule(void *info, void *rl, void *mode) {
    (void)mode;
    mport_t *o = info;
    lk(&o->lock);
    int have = 0;
    for (int i = 0; i < o->nloops; i++) if (o->loops[i] == rl) have = 1;
    if (!have && o->nloops < 8) o->loops[o->nloops++] = rl;
    ulk(&o->lock);
    mp_start(o);
}
static void mp_cancel(void *info, void *rl, void *mode) {
    (void)mode;
    mport_t *o = info;
    lk(&o->lock);
    for (int i = 0; i < o->nloops; i++) if (o->loops[i] == rl) { o->loops[i] = o->loops[--o->nloops]; break; }
    ulk(&o->lock);
}
static void mp_perform(void *info) {
    mport_t *o = info;
    for (;;) {
        lk(&o->lock);
        qmsg_t *q = o->head;
        if (q) { o->head = q->next; if (!o->head) o->tail = 0; }
        ulk(&o->lock);
        if (!q) break;
        if (o->valid && o->callout) o->callout(o, q->data, q->size, o->ctx.info);
        free(q);
    }
}
static mport_t *mp_lookup(u32 port) {
    mport_t *r = 0;
    lk(&g_plock);
    for (int i = 0; i < 256; i++) if (g_ports[i] && g_ports[i]->port == port && g_ports[i]->valid) { r = g_ports[i]; break; }
    ulk(&g_plock);
    return r;
}
static mport_t *mp_new(u32 port, void *callout, const cfctx_t *ctx, int owns) {
    mport_t *o = (mport_t *)cx_alloc_instance(&mp_c);
    o->port = port; o->valid = 1; o->owns_receive = owns; o->callout = (mp_callout)callout;
    if (ctx) {
        o->ctx = *ctx;
        if (ctx->retain && ctx->info) o->ctx.info = (void *)ctx->retain(ctx->info);
    }
    lk(&g_plock);
    for (int i = 0; i < 256; i++) if (!g_ports[i]) { g_ports[i] = o; break; }
    ulk(&g_plock);
    return o;
}
/* a mesma porta devolve o mesmo objeto (+1), como no CF; shouldFree = false nesse caso */
void *CFMachPortCreateWithPort(void *alloc, u32 port, void *callout, const cfctx_t *ctx, u8 *should_free) {
    (void)alloc;
    mport_t *e = mp_lookup(port);
    if (e) { if (should_free) *should_free = 0; return objc_retain((id)e); }
    if (should_free) *should_free = 1;
    return mp_new(port, callout, ctx, 0);
}
void *CFMachPortCreate(void *alloc, void *callout, const cfctx_t *ctx, u8 *should_free) {
    (void)alloc;
    u32 name = 0;
    if (mach_port_allocate(mach_task_self_, 1 /* RECEIVE */, &name) != 0) return 0;
    mach_port_insert_right(mach_task_self_, name, name, 20 /* MAKE_SEND */);
    if (should_free) *should_free = 1;
    return mp_new(name, callout, ctx, 1);
}
static int is_mp(void *p) { return p && cx_is_subclass(*(cx_class_t **)p, &mp_c); }
u32 CFMachPortGetPort(void *p) { return is_mp(p) ? ((mport_t *)p)->port : 0; }
u8 CFMachPortIsValid(void *p) { return is_mp(p) && ((mport_t *)p)->valid; }
void CFMachPortGetContext(void *p, cfctx_t *ctx) { if (is_mp(p) && ctx) *ctx = ((mport_t *)p)->ctx; }
void *CFMachPortGetInvalidationCallBack(void *p) { return is_mp(p) ? (void *)((mport_t *)p)->inv : 0; }
void CFMachPortSetInvalidationCallBack(void *p, void *cb) {
    if (!is_mp(p)) return;
    mport_t *o = p;
    o->inv = (mp_invcb)cb;
    if (!o->valid && cb) o->inv(o, o->ctx.info);   /* já inválida: chama na hora, como o CF */
}
void CFMachPortInvalidate(void *p) {
    if (!is_mp(p)) return;
    mport_t *o = p;
    if (!__atomic_exchange_n(&o->valid, 0, __ATOMIC_ACQ_REL)) return;
    objc_retain((id)o);
    lk(&g_plock);
    for (int i = 0; i < 256; i++) if (g_ports[i] == o) g_ports[i] = 0;
    ulk(&g_plock);
    if (o->source) CFRunLoopSourceInvalidate(o->source);
    if (o->inv) o->inv(o, o->ctx.info);
    /* a porta que criamos: solta o direito de recepção (a thread sai do mach_msg com erro) */
    if (o->owns_receive) { mach_port_mod_refs(mach_task_self_, o->port, 1, -1); mach_port_deallocate(mach_task_self_, o->port); }
    objc_release((id)o);
}
void *CFMachPortCreateRunLoopSource(void *alloc, void *p, CFIndex order) {
    (void)alloc;
    if (!is_mp(p)) return 0;
    mport_t *o = p;
    if (o->source) return objc_retain((id)o->source);
    srcctx_t c;
    memset(&c, 0, sizeof c);
    c.info = o; c.schedule = mp_schedule; c.cancel = mp_cancel; c.perform = mp_perform;
    o->source = CFRunLoopSourceCreate(0, order, &c);
    if (o->source) objc_retain((id)o->source);
    return o->source;
}
u64 CFMachPortGetTypeID(void) { return 35; }
static void mp_dealloc(id self, SEL c) {
    (void)c;
    mport_t *o = (mport_t *)self;
    CFMachPortInvalidate(o);
    if (o->ctx.release && o->ctx.info) o->ctx.release(o->ctx.info);
    while (o->head) { qmsg_t *q = o->head; o->head = q->next; free(q); }
    if (o->source) objc_release((id)o->source);
    cx_object_free(self);
}
static u64 mp_typeid(id s, SEL c) { (void)s; (void)c; return 35; }

/* ================================================================ CFPlugIn */
typedef void *(*factory_fn)(void *alloc, id typeUUID);
typedef struct fac { struct fac *next; id uuid; id plugin; factory_fn fn; id fname; int instances; int enabled; } fac_t;
typedef struct ptype { struct ptype *next; id type, factory; } ptype_t;
static fac_t *g_facs;
static ptype_t *g_types;
static volatile int g_fl;
static id uuid_of(id s) {
    extern id CFUUIDCreateFromString(void *alloc, id s);
    return s ? CFUUIDCreateFromString(0, s) : 0;
}
static int ueq(id a, id b) { return a && b && (a == b || MSG(BOOL, a, "isEqual:", b)); }
static fac_t *fac_find(id uuid) { for (fac_t *f = g_facs; f; f = f->next) if (ueq(f->uuid, uuid)) return f; return 0; }
static fac_t *fac_add(id uuid, id plugin, factory_fn fn, id fname) {
    fac_t *f = fac_find(uuid);
    if (!f) { f = calloc(1, sizeof *f); f->uuid = objc_retain(uuid); f->next = g_facs; g_facs = f; }
    f->plugin = plugin; f->fn = fn; f->enabled = 1;
    if (fname) f->fname = objc_retain(fname);
    return f;
}
static void type_add(id type, id factory) {
    for (ptype_t *t = g_types; t; t = t->next) if (ueq(t->type, type) && ueq(t->factory, factory)) return;
    ptype_t *t = calloc(1, sizeof *t);
    t->type = objc_retain(type); t->factory = objc_retain(factory); t->next = g_types; g_types = t;
}
/* lê CFPlugInFactories (UUID -> nome da função) e CFPlugInTypes (tipo -> [fábricas]) do Info.plist */
static void plugin_register(id bundle) {
    id info = MSG0(id, bundle, "infoDictionary");
    if (!info) return;
    id facs = MSG(id, info, "objectForKey:", cx_str("CFPlugInFactories"));
    id types = MSG(id, info, "objectForKey:", cx_str("CFPlugInTypes"));
    lk(&g_fl);
    id keys = facs ? MSG0(id, facs, "allKeys") : 0;
    for (u64 i = 0, n = keys ? MSG0(u64, keys, "count") : 0; i < n; i++) {
        id k = MSG(id, keys, "objectAtIndex:", i);
        id u = uuid_of(k);
        if (u) { fac_add(u, bundle, 0, MSG(id, facs, "objectForKey:", k)); objc_release(u); }
    }
    keys = types ? MSG0(id, types, "allKeys") : 0;
    for (u64 i = 0, n = keys ? MSG0(u64, keys, "count") : 0; i < n; i++) {
        id k = MSG(id, keys, "objectAtIndex:", i);
        id tu = uuid_of(k), list = MSG(id, types, "objectForKey:", k);
        for (u64 j = 0, m = list ? MSG0(u64, list, "count") : 0; tu && j < m; j++) {
            id fu = uuid_of(MSG(id, list, "objectAtIndex:", j));
            if (fu) { type_add(tu, fu); objc_release(fu); }
        }
        if (tu) objc_release(tu);
    }
    ulk(&g_fl);
}
/* o plug-in é o próprio CFBundle */
id CFPlugInCreate(void *alloc, id url) {
    extern id CFBundleCreate(void *alloc, id url);
    id b = CFBundleCreate(alloc, url);
    if (b) plugin_register(b);
    return b;
}
id CFPlugInGetBundle(id plugin) { return plugin; }
u64 CFPlugInGetTypeID(void) { return 37; }
void CFPlugInSetLoadOnDemand(id plugin, u8 flag) { (void)plugin; (void)flag; }   /* carregamos na primeira fábrica */
u8 CFPlugInIsLoadOnDemand(id plugin) { (void)plugin; return 1; }
u8 CFPlugInRegisterFactoryFunction(id factory, void *fn) {
    if (!factory || !fn) return 0;
    lk(&g_fl); fac_add(factory, 0, (factory_fn)fn, 0); ulk(&g_fl);
    return 1;
}
u8 CFPlugInRegisterFactoryFunctionByName(id factory, id plugin, id name) {
    if (!factory || !name) return 0;
    lk(&g_fl); fac_add(factory, plugin, 0, name); ulk(&g_fl);
    return 1;
}
u8 CFPlugInUnregisterFactory(id factory) {
    lk(&g_fl);
    fac_t *f = fac_find(factory);
    if (f) f->enabled = 0;
    ulk(&g_fl);
    return f != 0;
}
u8 CFPlugInRegisterPlugInType(id factory, id type) { if (!factory || !type) return 0; lk(&g_fl); type_add(type, factory); ulk(&g_fl); return 1; }
u8 CFPlugInUnregisterPlugInType(id factory, id type) {
    lk(&g_fl);
    for (ptype_t **pp = &g_types; *pp; pp = &(*pp)->next)
        if (ueq((*pp)->type, type) && ueq((*pp)->factory, factory)) { ptype_t *t = *pp; *pp = t->next; objc_release(t->type); objc_release(t->factory); free(t); ulk(&g_fl); return 1; }
    ulk(&g_fl);
    return 0;
}
id CFPlugInFindFactoriesForPlugInType(id type) {
    id a = MSG0(id, cls("NSMutableArray"), "new");
    lk(&g_fl);
    for (ptype_t *t = g_types; t; t = t->next) if (ueq(t->type, type)) { fac_t *f = fac_find(t->factory); if (f && f->enabled) MSG(void, a, "addObject:", t->factory); }
    ulk(&g_fl);
    return a;
}
id CFPlugInFindFactoriesForPlugInTypeInPlugIn(id type, id plugin) {
    id a = MSG0(id, cls("NSMutableArray"), "new");
    lk(&g_fl);
    for (ptype_t *t = g_types; t; t = t->next)
        if (ueq(t->type, type)) { fac_t *f = fac_find(t->factory); if (f && f->enabled && f->plugin == plugin) MSG(void, a, "addObject:", t->factory); }
    ulk(&g_fl);
    return a;
}
void *CFPlugInInstanceCreate(void *alloc, id factory, id type) {
    extern void *CFBundleGetFunctionPointerForName(id bundle, id name);
    lk(&g_fl);
    fac_t *f = fac_find(factory);
    factory_fn fn = f && f->enabled ? f->fn : 0;
    id plugin = f ? f->plugin : 0, fname = f ? f->fname : 0;
    ulk(&g_fl);
    if (!f || !f->enabled) return 0;
    if (!fn && plugin && fname) {
        MSG0(BOOL, plugin, "load");
        fn = (factory_fn)CFBundleGetFunctionPointerForName(plugin, fname);
        if (fn) { lk(&g_fl); f->fn = fn; ulk(&g_fl); }
    }
    return fn ? fn(alloc, type) : 0;
}
/* contagem de instâncias vivas por fábrica (o plug-in não descarrega enquanto há instâncias) */
void CFPlugInAddInstanceForFactory(id factory) { lk(&g_fl); fac_t *f = fac_find(factory); if (f) f->instances++; ulk(&g_fl); }
void CFPlugInRemoveInstanceForFactory(id factory) { lk(&g_fl); fac_t *f = fac_find(factory); if (f && f->instances > 0) f->instances--; ulk(&g_fl); }

/* ================================================================ CFURL */
static id url_str(id u) { return u ? MSG0(id, u, "absoluteString") : 0; }
/* CFURLCreateStringByAddingPercentEscapes(alloc, str, leaveUnescaped, legalToEscape, encoding): escapa o que não é
 * permitido em URL (e os de legalToEscape), deixando os de leaveUnescaped; bytes na codificação pedida */
id CFURLCreateStringByAddingPercentEscapes(void *alloc, id s, id leave, id legal, u32 enc) {
    (void)alloc;
    if (!s) return 0;
    int32_t n, used = 0;
    UChar *u = fdn_u16_dup(C(s), &n);
    int32_t cap = enc_max_bytes(enc, n) + 8;
    u8 *b = malloc((u64)cap);
    int32_t k = enc_encode(u, n, enc, 0, 0, b, cap, &used);
    free(u);
    if (k != n) { free(b); return 0; }
    const char *lv = leave ? C(leave) : "", *lg = legal ? C(legal) : "";
    static const char ok[] = "!$&'()*+,-./:;=?@_~#[]";
    char *o = malloc((u64)used * 3 + 1);
    int32_t w = 0;
    for (int32_t i = 0; i < used; i++) {
        u8 c = b[i];
        int alnum = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        int keep = c < 0x80 && c > 0x20 && (alnum || strchr(ok, c));
        if (c && strchr(lg, c)) keep = 0;
        if (c && c < 0x80 && strchr(lv, c)) keep = 1;
        if (c == '%' && !strchr(lv, '%')) keep = 0;
        if (keep) o[w++] = (char)c;
        else { static const char hx[] = "0123456789ABCDEF"; o[w++] = '%'; o[w++] = hx[c >> 4]; o[w++] = hx[c & 15]; }
    }
    o[w] = 0;
    id r = cx_str(o);
    free(o); free(b);
    return r;
}
static int hexv(char c) { return c >= '0' && c <= '9' ? c - '0' : (c | 32) >= 'a' && (c | 32) <= 'f' ? (c | 32) - 'a' + 10 : -1; }
/* charsToLeaveEscaped: NULL = não tira nenhum escape; "" = tira todos */
id CFURLCreateStringByReplacingPercentEscapesUsingEncoding(void *alloc, id s, id leave, u32 enc) {
    (void)alloc;
    if (!s) return 0;
    if (!leave) return MSG0(id, s, "copy");
    const char *p = C(s), *lv = C(leave);
    u64 n = strlen(p);
    u8 *b = malloc(n + 1);
    int32_t k = 0;
    for (u64 i = 0; i < n; i++) {
        if (p[i] == '%') {
            int h = i + 2 < n + 1 && i + 1 < n ? hexv(p[i + 1]) : -1, l = i + 2 < n ? hexv(p[i + 2]) : -1;
            if (h < 0 || l < 0) { free(b); return 0; }
            u8 c = (u8)(h * 16 + l);
            if (c && c < 0x80 && strchr(lv, c)) { b[k++] = '%'; b[k++] = (u8)p[i + 1]; b[k++] = (u8)p[i + 2]; }
            else b[k++] = c;
            i += 2;
        } else b[k++] = (u8)p[i];
    }
    int32_t m;
    UChar *u = enc_decode(b, k, enc, &m);
    free(b);
    if (!u) return 0;
    char *c8 = fdn_utf8_dup(u, m);
    id r = cx_str(c8);
    free(u); free(c8);
    return r;
}
id CFURLCreateStringByReplacingPercentEscapes(void *alloc, id s, id leave) { return CFURLCreateStringByReplacingPercentEscapesUsingEncoding(alloc, s, leave, ENC_UTF8); }
static id part(id u, const char *sel) { id v = u ? MSG0(id, u, sel) : 0; return v ? MSG0(id, v, "copy") : 0; }
id CFURLCopyUserName(id u) { return part(u, "user"); }
id CFURLCopyPassword(id u) { return part(u, "password"); }
id CFURLCopyFragment(id u, id leave) {
    id f = part(u, "fragment");
    if (!f || !leave) return f;
    /* fragment do NSURL vem sem escapes tirados; com charactersToLeaveEscaped tira o resto */
    id r = CFURLCreateStringByReplacingPercentEscapesUsingEncoding(0, f, leave, ENC_UTF8);
    objc_release(f);
    return r;
}
/* parâmetros (;p) do último segmento do caminho, sem o ';' */
id CFURLCopyParameterString(id u, id leave) {
    (void)leave;
    const char *s = C(url_str(u));
    const char *q = strchr(s, '?'), *h = strchr(s, '#');
    const char *end = q ? q : h ? h : s + strlen(s);
    const char *slash = s;
    for (const char *t = s; t < end; t++) if (*t == '/') slash = t;
    const char *semi = 0;
    for (const char *t = slash; t < end; t++) if (*t == ';') { semi = t; break; }
    return semi ? cx_str_n(semi + 1, (u64)(end - semi - 1)) : 0;
}
CFIndex CFURLGetPortNumber(id u) { id p = u ? MSG0(id, u, "port") : 0; return p ? MSG0(long, p, "longValue") : -1; }
/* "usuário:senha@host:porta" */
id CFURLCopyNetLocation(id u) {
    const char *s = C(url_str(u));
    const char *a = strstr(s, "://");
    if (!a) return 0;
    a += 3;
    const char *e = a;
    while (*e && *e != '/' && *e != '?' && *e != '#') e++;
    return cx_str_n(a, (u64)(e - a));
}
/* o caminho como está na URL (com escapes), sem a barra inicial; isAbsolute diz se tinha */
id CFURLCopyStrictPath(id u, u8 *abs) {
    const char *s = C(url_str(u));
    const char *a = strstr(s, "://");
    const char *p = a ? a + 3 : s;
    if (a) while (*p && *p != '/' && *p != '?' && *p != '#') p++;
    const char *e = p;
    while (*e && *e != '?' && *e != '#' && *e != ';') e++;
    if (abs) *abs = *p == '/';
    if (*p == '/') p++;
    return e > p ? cx_str_n(p, (u64)(e - p)) : 0;
}
id CFURLCopyPathExtension(id u) {
    id e = u ? MSG0(id, u, "pathExtension") : 0;
    return e && MSG0(u64, e, "length") ? MSG0(id, e, "copy") : 0;
}
id CFURLCreateCopyAppendingPathExtension(void *alloc, id u, id ext) {
    (void)alloc;
    id r = u && ext ? MSG(id, u, "URLByAppendingPathExtension:", ext) : 0;
    return r ? objc_retain(r) : 0;
}
id CFURLCreateCopyDeletingPathExtension(void *alloc, id u) {
    (void)alloc;
    id r = u ? MSG0(id, u, "URLByDeletingPathExtension") : 0;
    return r ? objc_retain(r) : 0;
}
/* pathStyle 0 POSIX, 2 Windows (\ vira /) */
id CFURLCreateWithFileSystemPathRelativeToBase(void *alloc, id path, long style, u8 isDir, id base) {
    (void)alloc;
    if (!path) return 0;
    id p = path;
    if (style == 2) p = MSG(id, path, "stringByReplacingOccurrencesOfString:withString:", cx_str("\\"), cx_str("/"));
    id r;
    if (C(p)[0] == '/' || !base) r = MSG(id, cls("NSURL"), "fileURLWithPath:isDirectory:", p, (BOOL)isDir);
    else r = MSG(id, cls("NSURL"), "fileURLWithPath:isDirectory:relativeToURL:", p, (BOOL)isDir, base);
    return r ? objc_retain(r) : 0;
}
/* os bytes da URL na codificação; escapeWhitespace troca espaço por %20 */
id CFURLCreateData(void *alloc, id u, u32 enc, u8 escape_ws) {
    (void)alloc;
    id s = u ? MSG0(id, u, "relativeString") : 0;
    if (!s) return 0;
    if (escape_ws) s = MSG(id, s, "stringByReplacingOccurrencesOfString:withString:", cx_str(" "), cx_str("%20"));
    int32_t n, used = 0;
    UChar *w = fdn_u16_dup(C(s), &n);
    u8 *b = malloc((u64)enc_max_bytes(enc, n) + 8);
    int32_t k = enc_encode(w, n, enc, '?', 0, b, enc_max_bytes(enc, n) + 8, &used);
    (void)k;
    id d = MSG(id, cls("NSData"), "dataWithBytes:length:", (const void *)b, (u64)used);
    free(w); free(b);
    return objc_retain(d);
}
void CFURLClearResourcePropertyCache(id u) { if (u) MSG0(void, u, "removeAllCachedResourceValues"); }
/* API antiga de recursos: só file:, com as propriedades kCFURLFileExists/Directory/Length/LastModificationTime/POSIXMode */
u8 CFURLCreateDataAndPropertiesFromResource(void *alloc, id u, id *data, id *props, id want, int *err) {
    (void)alloc; (void)want;
    if (err) *err = 0;
    if (!u || !MSG0(BOOL, u, "isFileURL")) { if (err) *err = -10; return 0; }   /* kCFURLUnknownSchemeError */
    id path = MSG0(id, u, "path");
    id fm = MSG0(id, cls("NSFileManager"), "defaultManager");
    BOOL dir = NO;
    BOOL exists = ((BOOL (*)(id, SEL, id, BOOL *))objc_msgSend)(fm, sel_registerName("fileExistsAtPath:isDirectory:"), path, &dir);
    if (props) {
        id d = MSG0(id, cls("NSMutableDictionary"), "new");
        id ex = MSG(id, cls("NSNumber"), "numberWithBool:", exists);
        MSG(void, d, "setObject:forKey:", ex, cx_str("kCFURLFileExists"));
        if (exists) {
            id at = ((id (*)(id, SEL, id, id *))objc_msgSend)(fm, sel_registerName("attributesOfItemAtPath:error:"), path, (id *)0);
            if (dir) {   /* kCFURLFileDirectoryContents: as URLs do conteúdo da pasta */
                id names = ((id (*)(id, SEL, id, id *))objc_msgSend)(fm, sel_registerName("contentsOfDirectoryAtPath:error:"), path, (id *)0);
                id urls = MSG0(id, cls("NSMutableArray"), "array");
                for (u64 i = 0, n = names ? MSG0(u64, names, "count") : 0; i < n; i++) {
                    id nm = MSG(id, names, "objectAtIndex:", i);
                    id cu = MSG(id, u, "URLByAppendingPathComponent:", nm);
                    MSG(void, urls, "addObject:", cu);
                }
                MSG(void, d, "setObject:forKey:", urls, cx_str("kCFURLFileDirectoryContents"));
            }
            if (at) {
                id sz = MSG(id, at, "objectForKey:", cx_str("NSFileSize"));
                id md = MSG(id, at, "objectForKey:", cx_str("NSFileModificationDate"));
                id pm = MSG(id, at, "objectForKey:", cx_str("NSFilePosixPermissions"));
                if (sz) MSG(void, d, "setObject:forKey:", sz, cx_str("kCFURLFileLength"));
                if (md) MSG(void, d, "setObject:forKey:", md, cx_str("kCFURLFileLastModificationTime"));
                if (pm) MSG(void, d, "setObject:forKey:", pm, cx_str("kCFURLFilePOSIXMode"));
            }
        }
        *props = d;
    }
    if (data) {
        *data = 0;
        if (!exists) { if (err) *err = -12; return 0; }   /* kCFURLResourceNotFoundError */
        if (dir) *data = objc_retain(MSG0(id, cls("NSData"), "data"));
        else {
            id dt = MSG(id, cls("NSData"), "dataWithContentsOfFile:", path);
            if (!dt) { if (err) *err = -13; return 0; }   /* kCFURLResourceAccessViolationError */
            *data = objc_retain(dt);
        }
    }
    return 1;
}
u8 CFURLWriteDataAndPropertiesToResource(id u, id data, id props, int *err) {
    (void)props;
    if (err) *err = 0;
    if (!u || !MSG0(BOOL, u, "isFileURL")) { if (err) *err = -10; return 0; }
    id path = MSG0(id, u, "path");
    BOOL ok;
    if (data) ok = MSG(BOOL, data, "writeToFile:atomically:", path, YES);
    else {   /* sem dados: cria a pasta */
        id fm = MSG0(id, cls("NSFileManager"), "defaultManager");
        ok = ((BOOL (*)(id, SEL, id, BOOL, id, id *))objc_msgSend)(fm, sel_registerName("createDirectoryAtPath:withIntermediateDirectories:attributes:error:"), path, NO, (id)0, (id *)0);
    }
    if (!ok && err) *err = -13;
    return ok ? 1 : 0;
}

/* ================================================================ números, listas de propriedades, texto */
CFIndex CFNumberGetByteSize(id n) {
    extern long CFNumberGetType(id n);
    static const signed char sz[] = { 0, 1, 2, 4, 8, 4, 8, 1, 2, 4, 8, 8, 4, 8, 8, 8, 8 };
    long t = n ? CFNumberGetType(n) : 0;
    return t > 0 && t < (long)sizeof sz ? sz[t] : 8;
}
static double nan_v(void) { union { u64 u; double d; } x = { 0x7ff8000000000000ULL }; return x.d; }
static double inf_v(int neg) { union { u64 u; double d; } x = { neg ? 0xfff0000000000000ULL : 0x7ff0000000000000ULL }; return x.d; }
id kCFNumberNaN, kCFNumberPositiveInfinity, kCFNumberNegativeInfinity;
id CFPropertyListCreateFromXMLData(void *alloc, id data, unsigned long opt, id *err_str) {
    extern id CFPropertyListCreateWithData(void *alloc, id data, unsigned long opt, long *fmt, id *err);
    id e = 0;
    id r = CFPropertyListCreateWithData(alloc, data, opt, 0, &e);
    if (err_str) *err_str = !r && e ? objc_retain(MSG0(id, e, "localizedDescription")) : 0;
    return r;
}
id CFPropertyListCreateXMLData(void *alloc, id plist) {
    extern id CFPropertyListCreateData(void *alloc, id plist, long format, unsigned long opt, id *err);
    return CFPropertyListCreateData(alloc, plist, 100 /* XML */, 0, 0);
}
/* lê até streamLength bytes (0 = até o fim) e interpreta */
id CFPropertyListCreateFromStream(void *alloc, id stream, CFIndex len, unsigned long opt, long *fmt, id *err_str) {
    extern long CFReadStreamRead(id s, u8 *buf, long n);
    extern id CFPropertyListCreateWithData(void *alloc, id data, unsigned long opt, long *fmt, id *err);
    if (!stream) return 0;
    id d = MSG0(id, cls("NSMutableData"), "data");
    u8 buf[16384];
    for (;;) {
        long want = len > 0 ? (len - (long)MSG0(u64, d, "length")) : (long)sizeof buf;
        if (want <= 0) break;
        if (want > (long)sizeof buf) want = sizeof buf;
        long r = CFReadStreamRead(stream, buf, want);
        if (r <= 0) break;
        MSG(void, d, "appendBytes:length:", (const void *)buf, (u64)r);
    }
    id e = 0;
    id r = CFPropertyListCreateWithData(alloc, d, opt, fmt, &e);
    if (err_str) *err_str = !r && e ? objc_retain(MSG0(id, e, "localizedDescription")) : 0;
    return r;
}
void *CFMakeCollectable(void *o) { return o; }   /* sem coletor de lixo: o próprio objeto, como no macOS atual */
id CFStringCreateWithFormatAndArguments(void *alloc, void *opts, id fmt, va_list ap) {
    (void)alloc; (void)opts;
    id r = fmt ? cx_str_vfmt(fmt, ap) : 0;
    return r ? objc_retain(r) : 0;
}
void CFStringAppendFormatAndArguments(id m, void *opts, id fmt, va_list ap) {
    (void)opts;
    if (!m || !fmt) return;
    id s = cx_str_vfmt(fmt, ap);
    if (s) MSG(void, m, "appendString:", s);
}
id CFStringCreateWithPascalStringNoCopy(void *alloc, const u8 *p, u32 enc, void *dealloc) {
    extern id CFStringCreateWithPascalString(void *alloc, const u8 *p, u32 enc);
    (void)dealloc;   /* copiamos o texto: o buffer do chamador continua dele */
    return CFStringCreateWithPascalString(alloc, p, enc);
}
typedef struct { u8 b[16]; } uuid_bytes;
id CFUUIDCreateWithBytes(void *alloc, u8 b0, u8 b1, u8 b2, u8 b3, u8 b4, u8 b5, u8 b6, u8 b7, u8 b8, u8 b9,
                         u8 b10, u8 b11, u8 b12, u8 b13, u8 b14, u8 b15) {
    extern id CFUUIDCreateFromUUIDBytes(void *alloc, uuid_bytes b);
    uuid_bytes u = { { b0, b1, b2, b3, b4, b5, b6, b7, b8, b9, b10, b11, b12, b13, b14, b15 } };
    return CFUUIDCreateFromUUIDBytes(alloc, u);
}
id CFErrorCreateWithUserInfoKeysAndValues(void *alloc, id domain, CFIndex code, const id *keys, const id *vals, CFIndex n) {
    extern id CFErrorCreate(void *alloc, id domain, long code, id info);
    id d = MSG0(id, cls("NSMutableDictionary"), "new");
    for (CFIndex i = 0; i < n; i++) if (keys && vals && keys[i] && vals[i]) MSG(void, d, "setObject:forKey:", vals[i], keys[i]);
    id e = CFErrorCreate(alloc, domain, code, d);
    objc_release(d);
    return e;
}
/* kCFDataSearchBackwards 1, kCFDataSearchAnchored 2 */
CFRange CFDataFind(id data, id find, CFRange r, unsigned long opts) {
    CFRange none = { -1, 0 };
    if (!data || !find) return none;
    const u8 *h = MSG0(const u8 *, data, "bytes"), *f = MSG0(const u8 *, find, "bytes");
    CFIndex hn = (CFIndex)MSG0(u64, data, "length"), fn = (CFIndex)MSG0(u64, find, "length");
    if (r.loc < 0 || r.len < 0 || r.loc + r.len > hn || fn == 0 || fn > r.len) return none;
    int back = opts & 1, anch = (opts & 2) != 0;
    CFIndex first = r.loc, last = r.loc + r.len - fn;
    for (CFIndex k = 0; k <= last - first; k++) {
        CFIndex i = back ? last - k : first + k;
        if (!memcmp(h + i, f, (u64)fn)) { CFRange o = { i, fn }; return o; }
        if (anch) break;
    }
    return none;
}
id CFRunLoopCopyAllModes(void *rl) {
    (void)rl;
    id a = MSG0(id, cls("NSMutableArray"), "new");
    MSG(void, a, "addObject:", cx_str("kCFRunLoopDefaultMode"));
    return a;
}
/* kCFNotificationDeliverImmediately 1, kCFNotificationPostToAllSessions 2 */
void CFNotificationCenterPostNotificationWithOptions(id c, id name, const void *obj, id info, unsigned long opts) {
    extern void CFNotificationCenterPostNotification(id c, id name, const void *obj, id info, unsigned char now);
    CFNotificationCenterPostNotification(c, name, obj, info, (opts & 1) != 0);
}
/* tira o domínio "suiteName" da lista de suítes que o app procura */
void CFPreferencesRemoveSuitePreferencesFromApp(id app, id suite) {
    extern void CFPreferencesSetAppValue(id key, id value, id app);
    extern id CFPreferencesCopyAppValue(id key, id app);
    extern unsigned char CFPreferencesAppSynchronize(id app);
    if (!suite) return;
    id key = cx_str("NSUserDefaultsSuites");
    id cur = CFPreferencesCopyAppValue(key, app);
    if (!cur) return;
    id m = MSG0(id, cur, "mutableCopy");
    MSG(void, m, "removeObject:", suite);
    CFPreferencesSetAppValue(key, MSG0(u64, m, "count") ? m : 0, app);
    CFPreferencesAppSynchronize(app);
    objc_release(m); objc_release(cur);
}
u32 CFLocaleGetWindowsLocaleCodeFromLocaleIdentifier(id ident) {
    const fdn_icu_t *I = fdn_icu();
    if (!ident || !I || !I->ok || !I->uloc_getLCID) return 0;
    return I->uloc_getLCID(C(ident));
}
id CFLocaleCreateLocaleIdentifierFromWindowsLocaleCode(void *alloc, u32 lcid) {
    (void)alloc;
    const fdn_icu_t *I = fdn_icu();
    if (!I || !I->ok || !I->uloc_getLocaleForLCID) return 0;
    char b[96];
    UErrorCode e = 0;
    int32_t n = I->uloc_getLocaleForLCID(lcid, b, sizeof b - 1, &e);
    if (e > 0 || n <= 0) return 0;
    b[n] = 0;
    return cx_str(b);
}

/* ================================================================ Foundation: geometria, zonas, log */
typedef struct { double x, y, w, h; } nsrect_t;
typedef struct { double top, left, bottom, right; } nsinsets_t;
double floor(double), ceil(double), round(double);
/* NSAlignMin{X,Y}/Max{X,Y}/Width/Height: Inward bits 0-5, Outward 8-13, Nearest 16-21 */
nsrect_t NSIntegralRectWithOptions(nsrect_t r, unsigned long long o) {
    double minx = r.x, miny = r.y, maxx = r.x + r.w, maxy = r.y + r.h;
    /* cada aresta: Inward arredonda para dentro, Outward para fora, Nearest para o mais perto */
#define EDGE(v, inb, outb, nearb, lo) do { \
        if (o & (1ULL << (inb))) v = (lo) ? ceil(v) : floor(v); \
        else if (o & (1ULL << (outb))) v = (lo) ? floor(v) : ceil(v); \
        else if (o & (1ULL << (nearb))) v = round(v); } while (0)
    EDGE(minx, 0, 8, 16, 1);
    EDGE(maxx, 2, 10, 18, 0);
    EDGE(miny, 1, 9, 17, 1);
    EDGE(maxy, 3, 11, 19, 0);
#undef EDGE
    double w = maxx - minx, h = maxy - miny;
    /* largura/altura (bits 4-7, 12-15, 20-23) quando o alinhamento é por tamanho */
    if (o & (1ULL << 4)) w = floor(r.w); else if (o & (1ULL << 12)) w = ceil(r.w); else if (o & (1ULL << 20)) w = round(r.w);
    if (o & (1ULL << 5)) h = floor(r.h); else if (o & (1ULL << 13)) h = ceil(r.h); else if (o & (1ULL << 21)) h = round(r.h);
    if (o & ((1ULL << 4) | (1ULL << 12) | (1ULL << 20))) { if (!(o & ((1ULL << 0) | (1ULL << 8) | (1ULL << 16)))) minx = maxx - w; maxx = minx + w; }
    if (o & ((1ULL << 5) | (1ULL << 13) | (1ULL << 21))) { if (!(o & ((1ULL << 1) | (1ULL << 9) | (1ULL << 17)))) miny = maxy - h; maxy = miny + h; }
    nsrect_t out = { minx, miny, maxx - minx, maxy - miny };
    if (out.w < 0) out.w = 0;
    if (out.h < 0) out.h = 0;
    return out;
}
u8 NSEdgeInsetsEqual(nsinsets_t a, nsinsets_t b) { return a.top == b.top && a.left == b.left && a.bottom == b.bottom && a.right == b.right; }
id NSOpenStepRootDirectory(void) { return cx_str("/"); }
void *NSZoneMalloc(void *zone, u64 n) { (void)zone; return malloc(n); }
void *NSZoneCalloc(void *zone, u64 n, u64 sz) { (void)zone; return calloc(n, sz); }
void *NSZoneRealloc(void *zone, void *p, u64 n) { (void)zone; return realloc(p, n); }
void NSZoneFree(void *zone, void *p) { (void)zone; free(p); }
u8 NSZombieEnabled = 0, NSDeallocateZombies = 0;
void (*cx_nslog_cstring_fn)(const char *msg, unsigned len, u8 withSyslogBanner);
void _NSSetLogCStringFunction(void (*fn)(const char *, unsigned, u8)) { cx_nslog_cstring_fn = fn; }
void (*_NSLogCStringFunction(void))(const char *, unsigned, u8) { return cx_nslog_cstring_fn; }

/* ================================================================ NSMapTable (API C) */
typedef struct {
    u64 (*hash)(void *table, const void *);
    u8 (*isEqual)(void *table, const void *, const void *);
    void (*retain)(void *table, const void *);
    void (*release)(void *table, void *);
    id (*describe)(void *table, const void *);
    const void *notAKeyMarker;
} nsmap_kcb;
typedef struct {
    void (*retain)(void *table, const void *);
    void (*release)(void *table, void *);
    id (*describe)(void *table, const void *);
} nsmap_vcb;
typedef struct mtab {
    cx_class_t *isa;
    nsmap_kcb k; nsmap_vcb v;
    const void **keys, **vals; u64 *hs;
    u64 n, cap;
} mtab_t;
typedef struct { mtab_t *t; u64 i; u64 pad[2]; } nsmap_enum;   /* NSMapEnumerator tem 3 palavras */
static cx_class_t mt_c, mt_m;
static u64 ptr_hash(void *t, const void *p) { (void)t; return (u64)p >> 3; }
static u8 ptr_eq(void *t, const void *a, const void *b) { (void)t; return a == b; }
static u64 int_hash(void *t, const void *p) { (void)t; return (u64)p; }
static u64 obj_hash(void *t, const void *p) { (void)t; return p ? MSG0(u64, (id)p, "hash") : 0; }
static u8 obj_eq(void *t, const void *a, const void *b) { (void)t; return a == b || (a && b && MSG(BOOL, (id)a, "isEqual:", (id)b)); }
static void obj_ret(void *t, const void *p) { (void)t; if (p) objc_retain((id)p); }
static void obj_rel(void *t, void *p) { (void)t; if (p) objc_release((id)p); }
static void own_rel(void *t, void *p) { (void)t; free(p); }
const nsmap_kcb NSIntegerMapKeyCallBacks = { int_hash, ptr_eq, 0, 0, 0, 0 };
const nsmap_kcb NSNonOwnedPointerMapKeyCallBacks = { ptr_hash, ptr_eq, 0, 0, 0, 0 };
const nsmap_kcb NSNonOwnedPointerOrNullMapKeyCallBacks = { ptr_hash, ptr_eq, 0, 0, 0, (const void *)-1 };
const nsmap_kcb NSNonRetainedObjectMapKeyCallBacks = { obj_hash, obj_eq, 0, 0, 0, 0 };
const nsmap_kcb NSObjectMapKeyCallBacks = { obj_hash, obj_eq, obj_ret, obj_rel, 0, 0 };
const nsmap_kcb NSOwnedPointerMapKeyCallBacks = { ptr_hash, ptr_eq, 0, own_rel, 0, 0 };
const nsmap_vcb NSIntegerMapValueCallBacks = { 0, 0, 0 };
const nsmap_vcb NSNonOwnedPointerMapValueCallBacks = { 0, 0, 0 };
const nsmap_vcb NSObjectMapValueCallBacks = { obj_ret, obj_rel, 0 };
const nsmap_vcb NSNonRetainedObjectMapValueCallBacks = { 0, 0, 0 };
const nsmap_vcb NSOwnedPointerMapValueCallBacks = { 0, own_rel, 0 };
static u64 mt_h(mtab_t *t, const void *k) { return t->k.hash ? t->k.hash(t, k) : (u64)k >> 3; }
static int mt_eq(mtab_t *t, const void *a, const void *b) { return a == b || (t->k.isEqual && t->k.isEqual(t, a, b)); }
static long mt_find(mtab_t *t, const void *k) {
    u64 h = mt_h(t, k);
    for (u64 i = 0; i < t->n; i++) if (t->hs[i] == h && mt_eq(t, t->keys[i], k)) return (long)i;
    return -1;
}
void *NSCreateMapTableWithZone(nsmap_kcb k, nsmap_vcb v, u64 cap, void *zone) {
    (void)zone;
    mtab_t *t = (mtab_t *)cx_alloc_instance(&mt_c);
    t->k = k; t->v = v;
    if (cap) { t->cap = cap; t->keys = malloc(cap * sizeof(void *)); t->vals = malloc(cap * sizeof(void *)); t->hs = malloc(cap * 8); }
    return t;
}
void *NSCreateMapTable(nsmap_kcb k, nsmap_vcb v, u64 cap) { return NSCreateMapTableWithZone(k, v, cap, 0); }
static void mt_del(mtab_t *t, u64 i) {
    const void *k = t->keys[i], *v = t->vals[i];
    t->n--;
    t->keys[i] = t->keys[t->n]; t->vals[i] = t->vals[t->n]; t->hs[i] = t->hs[t->n];
    if (t->k.release) t->k.release(t, (void *)k);
    if (t->v.release) t->v.release(t, (void *)v);
}
void NSResetMapTable(void *p) { mtab_t *t = p; while (t && t->n) mt_del(t, t->n - 1); }
void NSFreeMapTable(void *p) { if (p) objc_release((id)p); }
static int is_mt(void *p) { return p && cx_is_subclass(*(cx_class_t **)p, &mt_c); }
/* um NSMapTable de Objective-C também vale aqui (a API C e a classe são o mesmo objeto no macOS) */
void NSMapInsert(void *p, const void *k, const void *v) {
    mtab_t *t = p;
    if (!t) return;
    if (!is_mt(p)) { MSG(void, (id)p, "setObject:forKey:", (id)v, (id)k); return; }
    long i = mt_find(t, k);
    if (t->v.retain) t->v.retain(t, v);
    if (i >= 0) {
        const void *old = t->vals[i];
        t->vals[i] = v;
        if (t->v.release) t->v.release(t, (void *)old);
        return;
    }
    if (t->n == t->cap) {
        t->cap = t->cap ? t->cap * 2 : 16;
        t->keys = realloc(t->keys, t->cap * sizeof(void *)); t->vals = realloc(t->vals, t->cap * sizeof(void *)); t->hs = realloc(t->hs, t->cap * 8);
    }
    if (t->k.retain) t->k.retain(t, k);
    t->keys[t->n] = k; t->vals[t->n] = v; t->hs[t->n] = mt_h(t, k);
    t->n++;
}
void *NSMapInsertIfAbsent(void *p, const void *k, const void *v) {
    mtab_t *t = p;
    if (p && !is_mt(p)) { if (MSG(id, (id)p, "objectForKey:", (id)k)) return (void *)k; NSMapInsert(p, k, v); return 0; }
    long i = t ? mt_find(t, k) : -1;
    if (i >= 0) return (void *)t->keys[i];
    NSMapInsert(p, k, v);
    return 0;
}
void NSMapInsertKnownAbsent(void *p, const void *k, const void *v) { NSMapInsert(p, k, v); }
void *NSMapGet(void *p, const void *k) { if (p && !is_mt(p)) return MSG(id, (id)p, "objectForKey:", (id)k); mtab_t *t = p; long i = t ? mt_find(t, k) : -1; return i >= 0 ? (void *)t->vals[i] : 0; }
u8 NSMapMember(void *p, const void *k, void **ok, void **ov) {
    mtab_t *t = p;
    long i = t ? mt_find(t, k) : -1;
    if (i < 0) return 0;
    if (ok) *ok = (void *)t->keys[i];
    if (ov) *ov = (void *)t->vals[i];
    return 1;
}
void NSMapRemove(void *p, const void *k) { if (p && !is_mt(p)) { MSG(void, (id)p, "removeObjectForKey:", (id)k); return; } mtab_t *t = p; long i = t ? mt_find(t, k) : -1; if (i >= 0) mt_del(t, (u64)i); }
u64 NSCountMapTable(void *p) { return !p ? 0 : is_mt(p) ? ((mtab_t *)p)->n : MSG0(u64, (id)p, "count"); }
u8 NSCompareMapTables(void *a, void *b) {
    mtab_t *x = a, *y = b;
    if (!x || !y || x->n != y->n) return 0;
    for (u64 i = 0; i < x->n; i++) { long j = mt_find(y, x->keys[i]); if (j < 0 || y->vals[j] != x->vals[i]) return 0; }
    return 1;
}
void *NSCopyMapTableWithZone(void *p, void *zone) {
    mtab_t *t = p;
    if (!t) return 0;
    mtab_t *c = NSCreateMapTableWithZone(t->k, t->v, t->n, zone);
    for (u64 i = 0; i < t->n; i++) NSMapInsert(c, t->keys[i], t->vals[i]);
    return c;
}
nsmap_enum NSEnumerateMapTable(void *p) { nsmap_enum e = { p, 0, { 0, 0 } }; if (p) objc_retain((id)p); return e; }
u8 NSNextMapEnumeratorPair(nsmap_enum *e, void **k, void **v) {
    if (!e || !e->t || e->i >= e->t->n) return 0;
    if (k) *k = (void *)e->t->keys[e->i];
    if (v) *v = (void *)e->t->vals[e->i];
    e->i++;
    return 1;
}
void NSEndMapTableEnumeration(nsmap_enum *e) { if (e && e->t) { objc_release((id)e->t); e->t = 0; } }
id NSAllMapTableKeys(void *p) {
    mtab_t *t = p;
    id a = MSG0(id, cls("NSMutableArray"), "array");
    for (u64 i = 0; t && i < t->n; i++) if (t->keys[i]) MSG(void, a, "addObject:", (id)t->keys[i]);
    return a;
}
id NSAllMapTableValues(void *p) {
    mtab_t *t = p;
    id a = MSG0(id, cls("NSMutableArray"), "array");
    for (u64 i = 0; t && i < t->n; i++) if (t->vals[i]) MSG(void, a, "addObject:", (id)t->vals[i]);
    return a;
}
id NSStringFromMapTable(void *p) {
    mtab_t *t = p;
    id s = MSG0(id, cls("NSMutableString"), "string");
    for (u64 i = 0; t && i < t->n; i++) {
        char b[96];
        snprintf(b, sizeof b, "%p = %p;\n", t->keys[i], t->vals[i]);
        MSG(void, s, "appendString:", cx_str(b));
    }
    return s;
}
static void mt_dealloc(id self, SEL c) {
    (void)c;
    mtab_t *t = (mtab_t *)self;
    NSResetMapTable(t);
    free(t->keys); free(t->vals); free(t->hs);
    cx_object_free(self);
}
static u64 mt_count(id self, SEL c) { (void)c; return ((mtab_t *)self)->n; }

void cx_fdn_cfextra_init(void) {
    cx_class_t *ob = cx_class_named("NSObject");
    if (!ob || cx_class_named("__NSCFMachPort") == &mp_c) return;
    cx_define_class(&mp_c, &mp_m, "__NSCFMachPort", ob, (u32)(sizeof(mport_t) - 8));
    cx_add_method(&mp_c, "dealloc", (void *)mp_dealloc);
    cx_add_method(&mp_c, "_cfTypeID", (void *)mp_typeid);
    cx_define_class(&mt_c, &mt_m, "__NSCMapTable", ob, (u32)(sizeof(mtab_t) - 8));
    cx_add_method(&mt_c, "dealloc", (void *)mt_dealloc);
    cx_add_method(&mt_c, "count", (void *)mt_count);
    if (!kCFNumberNaN) {
        kCFNumberNaN = objc_retain(MSG(id, cls("NSNumber"), "numberWithDouble:", nan_v()));
        kCFNumberPositiveInfinity = objc_retain(MSG(id, cls("NSNumber"), "numberWithDouble:", inf_v(0)));
        kCFNumberNegativeInfinity = objc_retain(MSG(id, cls("NSNumber"), "numberWithDouble:", inf_v(1)));
    }
}
