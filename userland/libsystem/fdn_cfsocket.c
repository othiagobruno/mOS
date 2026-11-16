/*
 * mOS libSystem — CFSocket e CFFileDescriptor, ligados ao CFRunLoop como no macOS.
 *
 * O CFRunLoop do mOS só executa fontes da versão 0 que alguém sinalizou. Uma thread gerente faz poll() dos
 * descritores com callbacks ligados; quando um fica pronto, ela desliga aquele tipo de callback, marca o evento,
 * sinaliza a fonte e acorda os run loops onde ela está. O perform (na thread do run loop) faz o trabalho do tipo
 * (accept, recv, conexão concluída) e chama o callout; os tipos com reativação automática voltam a ser vigiados.
 *
 * CFSocket: tipos de callback Read 1, Accept 2, Data 3, Connect 4, Write 8; flags AutomaticallyReenable*
 * (Read 1, Accept 2, Data 3, Write 8), LeaveErrors 64, CloseOnInvalidate 128 (padrão 0x83).
 * CFFileDescriptor: Read 1, Write 2, sem reativação automática.
 */
#include "foundation.h"
#include "libc.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
IMP cx_lookup(cx_class_t *cls, SEL sel);
int *__error(void);
int socket(int d, int t, int p);
int bind(int fd, const void *a, unsigned l);
int listen(int fd, int n);
int accept(int fd, void *a, unsigned *l);
int connect(int fd, const void *a, unsigned l);
int getsockname(int fd, void *a, unsigned *l);
int getpeername(int fd, void *a, unsigned *l);
int getsockopt(int fd, int lvl, int opt, void *v, unsigned *l);
int setsockopt(int fd, int lvl, int opt, const void *v, unsigned l);
long recv(int fd, void *b, unsigned long n, int f);
long recvfrom(int fd, void *b, unsigned long n, int f, void *a, unsigned *l);
long send(int fd, const void *b, unsigned long n, int f);
long sendto(int fd, const void *b, unsigned long n, int f, const void *a, unsigned l);
int fcntl(int fd, int cmd, ...);
int pipe(int *fds);
long write(int fd, const void *b, size_t n);
long read(int fd, void *b, size_t n);
int close(int fd);
int pthread_create(unsigned long *t, const void *attr, void *(*fn)(void *), void *arg);
typedef struct { int fd; short events, revents; } pfd_t;
int poll(pfd_t *p, unsigned long n, int timeout);
void *CFRunLoopSourceCreate(void *alloc, long order, void *ctx);
void CFRunLoopSourceSignal(void *s);
void CFRunLoopWakeUp(void *rl);
void CFRunLoopSourceInvalidate(void *s);
id CFDataCreate(void *alloc, const void *b, long n);
const u8 *CFDataGetBytePtr(id d);
long CFDataGetLength(id d);
double cx_now(void);

#define POLLIN 1
#define POLLOUT 4
#define POLLERR 8
#define POLLHUP 16
#define F_GETFL 3
#define F_SETFL 4
#define O_NONBLOCK 4
#define SOL_SOCKET 0xFFFF
#define SO_ERROR 0x1007
#define SO_REUSEADDR 4

enum { CB_READ = 1, CB_ACCEPT = 2, CB_DATA = 3, CB_CONNECT = 4, CB_WRITE = 8 };
enum { SF_REENABLE_READ = 1, SF_REENABLE_ACCEPT = 2, SF_REENABLE_DATA = 3, SF_REENABLE_WRITE = 8, SF_LEAVE_ERRORS = 64,
       SF_CLOSE_ON_INVALIDATE = 128 };

typedef struct { long version; void *info; const void *(*retain)(const void *); void (*release)(const void *); void *desc; } cfctx_t;
typedef struct {
    long version; void *info; const void *(*retain)(const void *); void (*release)(const void *);
    void *copyDescription, *equal, *hash;
    void (*schedule)(void *, void *, void *); void (*cancel)(void *, void *, void *);
    void (*perform)(void *);
} srcctx_t;

typedef struct obj {
    cx_class_t *isa;
    int kind;                /* 0 socket, 1 descritor */
    int fd, valid;
    unsigned long cbtypes;   /* pedidos na criação */
    unsigned long enabled;   /* vigiados agora */
    unsigned long pending;   /* prontos, a entregar no perform */
    unsigned long sflags;
    int connecting;          /* connect() não bloqueante em andamento */
    void *callout;
    cfctx_t ctx;
    void *source;
    void *loops[8];
    int nloops;
    struct obj *next;        /* lista da thread gerente */
} obj_t;

static cx_class_t sock_c, sock_m, fdo_c, fdo_m;
static obj_t *g_list;
static volatile int g_lock;
static int g_wake[2] = {-1, -1};
static int g_started;
static void lk(void) { while (__atomic_exchange_n(&g_lock, 1, __ATOMIC_ACQUIRE)) { } }
static void ulk(void) { __atomic_store_n(&g_lock, 0, __ATOMIC_RELEASE); }
static void poke(void) { if (g_wake[1] >= 0) { char c = 1; write(g_wake[1], &c, 1); } }

static void signal_obj(obj_t *o) {
    if (!o->source) return;
    CFRunLoopSourceSignal(o->source);
    for (int i = 0; i < o->nloops; i++) CFRunLoopWakeUp(o->loops[i]);
}
/* tipo de callback de socket -> eventos de poll */
static short events_of(obj_t *o) {
    short ev = 0;
    if (o->kind == 1) {
        if (o->enabled & 1) ev |= POLLIN;
        if (o->enabled & 2) ev |= POLLOUT;
        return ev;
    }
    unsigned long rd = o->enabled & 3;   /* read, accept e data são exclusivos */
    if (rd) ev |= POLLIN;
    if ((o->enabled & CB_WRITE) || (o->connecting && (o->enabled & CB_CONNECT))) ev |= POLLOUT;
    return ev;
}
static void *manager(void *arg) {
    (void)arg;
    pfd_t p[256];
    obj_t *who[256];
    for (;;) {
        int n = 0;
        p[n].fd = g_wake[0]; p[n].events = POLLIN; p[n].revents = 0; who[n++] = 0;
        lk();
        for (obj_t *o = g_list; o && n < 256; o = o->next) {
            short ev = o->valid && o->source ? events_of(o) : 0;
            if (!ev) continue;
            p[n].fd = o->fd; p[n].events = ev; p[n].revents = 0; who[n++] = o;
        }
        ulk();
        if (poll(p, (unsigned long)n, -1) <= 0) continue;
        if (p[0].revents) { char b[64]; read(g_wake[0], b, sizeof b); }
        lk();
        for (int i = 1; i < n; i++) {
            obj_t *o = who[i];
            short r = p[i].revents;
            if (!r || !o->valid) continue;
            if (o->kind == 1) {
                if ((r & (POLLIN | POLLERR | POLLHUP)) && (o->enabled & 1)) { o->enabled &= ~1ul; o->pending |= 1; }
                if ((r & (POLLOUT | POLLERR | POLLHUP)) && (o->enabled & 2)) { o->enabled &= ~2ul; o->pending |= 2; }
            } else {
                unsigned long rd = o->enabled & 3;
                if ((r & (POLLIN | POLLERR | POLLHUP)) && rd) { o->enabled &= ~3ul; o->pending |= rd; }
                if ((r & (POLLOUT | POLLERR | POLLHUP)) && o->connecting && (o->enabled & CB_CONNECT)) {
                    o->enabled &= ~(unsigned long)CB_CONNECT; o->pending |= CB_CONNECT;
                }
                if ((r & POLLOUT) && (o->enabled & CB_WRITE) && !o->connecting) { o->enabled &= ~(unsigned long)CB_WRITE; o->pending |= CB_WRITE; }
            }
            if (o->pending) signal_obj(o);
        }
        ulk();
    }
    return 0;
}
static void start_manager(void) {
    if (__atomic_exchange_n(&g_started, 1, __ATOMIC_ACQ_REL)) return;
    if (pipe(g_wake) == 0) {
        fcntl(g_wake[0], F_SETFL, O_NONBLOCK);
        unsigned long t;
        pthread_create(&t, 0, manager, 0);
    }
}
static void track(obj_t *o) {
    start_manager();
    lk();
    o->next = g_list;
    g_list = o;
    ulk();
}
static void untrack(obj_t *o) {
    lk();
    for (obj_t **pp = &g_list; *pp; pp = &(*pp)->next) if (*pp == o) { *pp = o->next; break; }
    ulk();
}

/* ---- fonte do run loop: guarda os loops (schedule/cancel) e entrega no perform */
static void src_schedule(void *info, void *rl, void *mode) {
    (void)mode;
    obj_t *o = info;
    lk();
    int have = 0;
    for (int i = 0; i < o->nloops; i++) if (o->loops[i] == rl) have = 1;
    if (!have && o->nloops < 8) o->loops[o->nloops++] = rl;
    ulk();
    poke();
}
static void src_cancel(void *info, void *rl, void *mode) {
    (void)mode;
    obj_t *o = info;
    lk();
    for (int i = 0; i < o->nloops; i++) if (o->loops[i] == rl) { o->loops[i] = o->loops[--o->nloops]; break; }
    ulk();
}
typedef void (*sock_cb_t)(void *s, unsigned long type, id address, const void *data, void *info);
typedef void (*fd_cb_t)(void *f, unsigned long types, void *info);
static id sockaddr_data(int fd, int peer) {
    u8 a[128];
    unsigned l = sizeof a;
    if ((peer ? getpeername(fd, a, &l) : getsockname(fd, a, &l)) != 0) return 0;
    return CFDataCreate(0, a, l);
}
static void src_perform(void *info) {
    obj_t *o = info;
    if (!o->valid) return;
    objc_retain((id)o);
    lk();
    unsigned long p = o->pending;
    o->pending = 0;
    ulk();
    if (o->kind == 1) {
        if (o->callout && p) ((fd_cb_t)o->callout)(o, p, o->ctx.info);
        objc_release((id)o);
        return;
    }
    sock_cb_t cb = (sock_cb_t)o->callout;
    if ((p & 3) == CB_ACCEPT) {
        u8 a[128];
        unsigned l = sizeof a;
        int nfd = accept(o->fd, a, &l);
        if (nfd >= 0 && cb) { id ad = CFDataCreate(0, a, l); cb(o, CB_ACCEPT, ad, &nfd, o->ctx.info); objc_release(ad); }
        if (o->sflags & SF_REENABLE_ACCEPT) o->enabled |= CB_ACCEPT;
    } else if ((p & 3) == CB_DATA) {
        u8 buf[65536];
        u8 a[128];
        unsigned l = sizeof a;
        long n = recvfrom(o->fd, buf, sizeof buf, 0, a, &l);
        id data = CFDataCreate(0, buf, n > 0 ? n : 0);   /* dados vazios = fim do fluxo, como no macOS */
        id ad = l ? CFDataCreate(0, a, l) : 0;
        if (cb) cb(o, CB_DATA, ad, data, o->ctx.info);
        if (ad) objc_release(ad);
        objc_release(data);
        if (n > 0 && (o->sflags & SF_REENABLE_DATA) == SF_REENABLE_DATA) o->enabled |= CB_DATA;
    } else if ((p & 3) == CB_READ) {
        if (cb) cb(o, CB_READ, 0, 0, o->ctx.info);
        if (o->sflags & SF_REENABLE_READ) o->enabled |= CB_READ;
    }
    if (p & CB_CONNECT) {
        int err = 0;
        unsigned l = sizeof err;
        getsockopt(o->fd, SOL_SOCKET, SO_ERROR, &err, &l);
        o->connecting = 0;
        if (cb) cb(o, CB_CONNECT, 0, err ? &err : 0, o->ctx.info);
    }
    if (p & CB_WRITE) {
        if (cb) cb(o, CB_WRITE, 0, 0, o->ctx.info);
        if (o->sflags & SF_REENABLE_WRITE) o->enabled |= CB_WRITE;
    }
    poke();
    objc_release((id)o);
}
static void *make_source(obj_t *o, long order) {
    if (o->source) return objc_retain((id)o->source);
    srcctx_t c;
    memset(&c, 0, sizeof c);
    c.info = o;
    c.schedule = src_schedule;
    c.cancel = src_cancel;
    c.perform = src_perform;
    o->source = CFRunLoopSourceCreate(0, order, &c);
    if (o->source) objc_retain((id)o->source);   /* a do objeto; a outra é de quem pediu */
    poke();
    return o->source;
}
static void copy_ctx(obj_t *o, const cfctx_t *c) {
    if (!c) return;
    o->ctx = *c;
    if (o->ctx.retain && o->ctx.info) o->ctx.info = (void *)o->ctx.retain(o->ctx.info);
}
static void invalidate(obj_t *o) {
    if (!o->valid) return;
    o->valid = 0;
    untrack(o);
    poke();
    if (o->source) { CFRunLoopSourceInvalidate(o->source); objc_release((id)o->source); o->source = 0; }
    if (o->kind == 0 ? (o->sflags & SF_CLOSE_ON_INVALIDATE) : (o->sflags & 1)) { if (o->fd >= 0) close(o->fd); }
    o->fd = -1;
    if (o->ctx.release && o->ctx.info) o->ctx.release(o->ctx.info);
    o->ctx.info = 0;
}
static void obj_dealloc(id s, SEL c) {
    (void)c;
    invalidate((obj_t *)s);
    IMP sup = cx_lookup(cx_class_named("NSObject"), sel_registerName("dealloc"));
    if (sup) sup(s, sel_registerName("dealloc"));
}

/* ======================================================================== CFSocket */
static obj_t *as_sock(id s) { return s && object_getClass(s) == &sock_c ? (obj_t *)s : 0; }
u64 CFSocketGetTypeID(void) { return 37; }
id CFSocketCreateWithNative(void *alloc, int fd, unsigned long cbtypes, void *callout, const cfctx_t *ctx) {
    (void)alloc;
    if (fd < 0) return 0;
    obj_t *o = (obj_t *)cx_alloc_instance(&sock_c);
    o->kind = 0; o->fd = fd; o->valid = 1;
    o->cbtypes = o->enabled = cbtypes;
    o->sflags = SF_REENABLE_READ | SF_REENABLE_ACCEPT | SF_CLOSE_ON_INVALIDATE;   /* 0x83 */
    o->callout = callout;
    copy_ctx(o, ctx);
    track(o);
    return (id)o;
}
id CFSocketCreate(void *alloc, int family, int type, int proto, unsigned long cbtypes, void *callout, const cfctx_t *ctx) {
    int fd = socket(family > 0 ? family : 2, type > 0 ? type : 1, proto);
    if (fd < 0) return 0;
    if (family == 2 || family == 30 || family <= 0) { int one = 1; setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one); }
    return CFSocketCreateWithNative(alloc, fd, cbtypes, callout, ctx);
}
typedef struct { int family, type, proto; id address; } sig_t;
long CFSocketSetAddress(id s, id address) {
    obj_t *o = as_sock(s);
    if (!o || !o->valid || !address) return -1;
    if (bind(o->fd, CFDataGetBytePtr(address), (unsigned)CFDataGetLength(address)) != 0) return -1;
    /* stream: escuta (o macOS faz o listen no SetAddress) */
    int type = 0;
    unsigned l = sizeof type;
    getsockopt(o->fd, SOL_SOCKET, 0x1008 /* SO_TYPE */, &type, &l);
    if (type == 1) listen(o->fd, 256);
    return 0;
}
id CFSocketCreateWithSocketSignature(void *alloc, const sig_t *sig, unsigned long cbtypes, void *callout, const cfctx_t *ctx) {
    if (!sig) return 0;
    id s = CFSocketCreate(alloc, sig->family, sig->type, sig->proto, cbtypes, callout, ctx);
    if (s && sig->address && CFSocketSetAddress(s, sig->address) != 0) { invalidate((obj_t *)s); objc_release(s); return 0; }
    return s;
}
long CFSocketConnectToAddress(id s, id address, double timeout) {
    obj_t *o = as_sock(s);
    if (!o || !o->valid || !address) return -1;
    int fl = fcntl(o->fd, F_GETFL, 0);
    if (timeout < 0) {
        /* não bloqueante: o fim chega pelo callback de conexão */
        fcntl(o->fd, F_SETFL, fl | O_NONBLOCK);
        int r = connect(o->fd, CFDataGetBytePtr(address), (unsigned)CFDataGetLength(address));
        if (r == 0) { if (o->cbtypes & CB_CONNECT) { lk(); o->pending |= CB_CONNECT; o->connecting = 1; signal_obj(o); ulk(); } return 0; }
        if (*__error() == 36 /* EINPROGRESS */ || *__error() == 35) { o->connecting = 1; poke(); return 0; }
        return -1;
    }
    if (timeout == 0) return connect(o->fd, CFDataGetBytePtr(address), (unsigned)CFDataGetLength(address)) == 0 ? 0 : -1;
    /* com prazo: não bloqueante + espera */
    fcntl(o->fd, F_SETFL, fl | O_NONBLOCK);
    int r = connect(o->fd, CFDataGetBytePtr(address), (unsigned)CFDataGetLength(address));
    if (r != 0 && *__error() != 36 && *__error() != 35) { fcntl(o->fd, F_SETFL, fl); return -1; }
    if (r != 0) {
        pfd_t p = {o->fd, POLLOUT, 0};
        int w = poll(&p, 1, (int)(timeout * 1000));
        fcntl(o->fd, F_SETFL, fl);
        if (w == 0) return -2;   /* kCFSocketTimeout */
        int err = 0;
        unsigned l = sizeof err;
        getsockopt(o->fd, SOL_SOCKET, SO_ERROR, &err, &l);
        if (err) return -1;
    } else fcntl(o->fd, F_SETFL, fl);
    return 0;
}
id CFSocketCreateConnectedToSocketSignature(void *alloc, const sig_t *sig, unsigned long cbtypes, void *callout, const cfctx_t *ctx, double timeout) {
    if (!sig) return 0;
    id s = CFSocketCreate(alloc, sig->family, sig->type, sig->proto, cbtypes, callout, ctx);
    if (s && sig->address && CFSocketConnectToAddress(s, sig->address, timeout) != 0) { invalidate((obj_t *)s); objc_release(s); return 0; }
    return s;
}
void CFSocketInvalidate(id s) { obj_t *o = as_sock(s); if (o) invalidate(o); }
unsigned char CFSocketIsValid(id s) { obj_t *o = as_sock(s); return o && o->valid; }
id CFSocketCopyAddress(id s) { obj_t *o = as_sock(s); return o && o->valid ? sockaddr_data(o->fd, 0) : 0; }
id CFSocketCopyPeerAddress(id s) { obj_t *o = as_sock(s); return o && o->valid ? sockaddr_data(o->fd, 1) : 0; }
void CFSocketGetContext(id s, cfctx_t *ctx) { obj_t *o = as_sock(s); if (o && ctx) { *ctx = o->ctx; ctx->version = 0; } }
int CFSocketGetNative(id s) { obj_t *o = as_sock(s); return o ? o->fd : -1; }
void *CFSocketCreateRunLoopSource(void *alloc, id s, long order) { (void)alloc; obj_t *o = as_sock(s); return o && o->valid ? make_source(o, order) : 0; }
unsigned long CFSocketGetSocketFlags(id s) { obj_t *o = as_sock(s); return o ? o->sflags : 0; }
void CFSocketSetSocketFlags(id s, unsigned long f) { obj_t *o = as_sock(s); if (o) o->sflags = f; }
void CFSocketDisableCallBacks(id s, unsigned long types) {
    obj_t *o = as_sock(s);
    if (!o) return;
    lk();
    if ((types & 3) && (o->enabled & 3) == (types & 3)) o->enabled &= ~3ul;
    o->enabled &= ~(types & ~3ul);
    ulk();
    poke();
}
void CFSocketEnableCallBacks(id s, unsigned long types) {
    obj_t *o = as_sock(s);
    if (!o) return;
    lk();
    o->enabled |= types & o->cbtypes;
    ulk();
    poke();
}
long CFSocketSendData(id s, id address, id data, double timeout) {
    obj_t *o = as_sock(s);
    if (!o || !o->valid || !data) return -1;
    const u8 *b = CFDataGetBytePtr(data);
    long n = CFDataGetLength(data), sent = 0;
    double until = timeout > 0 ? cx_now() + timeout : 0;
    while (sent < n) {
        long w = address ? sendto(o->fd, b + sent, (unsigned long)(n - sent), 0, CFDataGetBytePtr(address), (unsigned)CFDataGetLength(address))
                         : send(o->fd, b + sent, (unsigned long)(n - sent), 0);
        if (w > 0) { sent += w; continue; }
        if (w < 0 && (*__error() == 35 || *__error() == 4)) {
            if (until && cx_now() > until) return -2;
            pfd_t p = {o->fd, POLLOUT, 0};
            poll(&p, 1, 50);
            continue;
        }
        return -1;
    }
    return 0;
}

/* ======================================================================== CFFileDescriptor */
static obj_t *as_fdo(id s) { return s && object_getClass(s) == &fdo_c ? (obj_t *)s : 0; }
u64 CFFileDescriptorGetTypeID(void) { return 35; }
id CFFileDescriptorCreate(void *alloc, int fd, unsigned char close_on, void *callout, const cfctx_t *ctx) {
    (void)alloc;
    if (fd < 0) return 0;
    obj_t *o = (obj_t *)cx_alloc_instance(&fdo_c);
    o->kind = 1; o->fd = fd; o->valid = 1;
    o->sflags = close_on ? 1 : 0;
    o->callout = callout;
    copy_ctx(o, ctx);
    track(o);
    return (id)o;
}
int CFFileDescriptorGetNativeDescriptor(id f) { obj_t *o = as_fdo(f); return o ? o->fd : -1; }
void CFFileDescriptorGetContext(id f, cfctx_t *ctx) { obj_t *o = as_fdo(f); if (o && ctx) { *ctx = o->ctx; ctx->version = 0; } }
void CFFileDescriptorEnableCallBacks(id f, unsigned long t) { obj_t *o = as_fdo(f); if (!o) return; lk(); o->enabled |= t & 3; ulk(); poke(); }
void CFFileDescriptorDisableCallBacks(id f, unsigned long t) { obj_t *o = as_fdo(f); if (!o) return; lk(); o->enabled &= ~(t & 3); ulk(); poke(); }
void CFFileDescriptorInvalidate(id f) { obj_t *o = as_fdo(f); if (o) invalidate(o); }
unsigned char CFFileDescriptorIsValid(id f) { obj_t *o = as_fdo(f); return o && o->valid; }
void *CFFileDescriptorCreateRunLoopSource(void *alloc, id f, long order) { (void)alloc; obj_t *o = as_fdo(f); return o && o->valid ? make_source(o, order) : 0; }

void cx_fdn_cfsocket_init(void) {
    cx_class_t *ob = cx_class_named("NSObject");
    if (!ob || cx_class_named("__NSCFSocket") == &sock_c) return;
    cx_define_class(&sock_c, &sock_m, "__NSCFSocket", ob, (u32)(sizeof(obj_t) - 8));
    cx_add_method(&sock_c, "dealloc", (void *)obj_dealloc);
    cx_define_class(&fdo_c, &fdo_m, "__NSCFFileDescriptor", ob, (u32)(sizeof(obj_t) - 8));
    cx_add_method(&fdo_c, "dealloc", (void *)obj_dealloc);
}
