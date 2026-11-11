/*
 * mOS libSystem: SCNetworkReachability com estado, CNCopyCurrentNetworkInfo, CFHostGetNames,
 * CFStreamCreatePairWithSocketToCFHost e nw_tls_copy_sec_protocol_options.
 * O WhatsApp cria a reachability por nome e põe numa fila (SetDispatchQueue); antes o mOS
 * devolvia um NSObject qualquer e nunca chamava a callback. Agora o objeto guarda alvo,
 * callback, contexto e fila; ao ganhar fila ou run loop, a callback recebe as flags atuais
 * uma vez (o estado da rede do mOS não muda durante a sessão: en0 está sempre de pé).
 */
#include "foundation.h"
#include "libc.h"

id objc_retain(id o);
void objc_release(id o);
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *));
void *dispatch_get_main_queue(void);
void CFStreamCreatePairWithSocketToHost(void *alloc, id host, int port, id *rd, id *wr);

enum { RF_REACHABLE = 1u << 1, RF_LOCAL = 1u << 16, RF_DIRECT = 1u << 17 };
typedef struct { long version; void *info; const void *(*retain)(const void *); void (*release)(const void *); void *desc; } sc_ctx_t;
typedef void (*sc_cb_t)(id target, u32 flags, void *info);
typedef struct { void *isa; char host[256]; u32 ip; int has_ip; sc_cb_t cb; sc_ctx_t ctx; void *queue; int scheduled; } reach_t;
static cx_class_t reach_c, reach_m;
static int reach_ready;

static void reach_dealloc(id self, SEL c) {
    reach_t *r = (reach_t *)self;
    if (r->ctx.release && r->ctx.info) r->ctx.release(r->ctx.info);
    IMP sup = cx_lookup(cx_class_named("NSObject"), c);
    if (sup) ((void (*)(id, SEL))sup)(self, c);
}
static reach_t *reach_new(void) {
    if (!reach_ready) {
        cx_define_class(&reach_c, &reach_m, "SCNetworkReachability", cx_class_named("NSObject"), (u32)(sizeof(reach_t) - sizeof(void *)));
        cx_add_method(&reach_c, "dealloc", (void *)reach_dealloc);
        reach_ready = 1;
    }
    return (reach_t *)cx_alloc_instance(&reach_c);
}
static reach_t *as_reach(id o) { return o && reach_ready && o->isa == &reach_c ? (reach_t *)o : 0; }
static u32 reach_flags(const reach_t *r) {
    if (r->has_ip && (r->ip & 0xFF) == 127) return RF_REACHABLE | RF_LOCAL | RF_DIRECT;
    if (!r->has_ip && (!strcmp(r->host, "localhost") || !strcmp(r->host, "localhost."))) return RF_REACHABLE | RF_LOCAL | RF_DIRECT;
    if (r->has_ip && r->ip == 0) return RF_REACHABLE;                   /* 0.0.0.0: "há rede" */
    return RF_REACHABLE;
}
id SCNetworkReachabilityCreateWithName(void *alloc, const char *name) {
    reach_t *r;
    (void)alloc;
    if (!name) return 0;
    if (!(r = reach_new())) return 0;
    strlcpy(r->host, name, sizeof r->host);
    return (id)r;
}
/* sockaddr_in: len, family, port, addr */
id SCNetworkReachabilityCreateWithAddress(void *alloc, const void *addr) {
    const u8 *sa = addr;
    reach_t *r;
    (void)alloc;
    if (!sa) return 0;
    if (!(r = reach_new())) return 0;
    if (sa[1] == 2) { memcpy(&r->ip, sa + 4, 4); r->has_ip = 1; }
    else if (sa[1] == 30) { r->has_ip = 0; strlcpy(r->host, "::", sizeof r->host); }
    return (id)r;
}
id SCNetworkReachabilityCreateWithAddressPair(void *alloc, const void *local, const void *remote) {
    return SCNetworkReachabilityCreateWithAddress(alloc, remote ? remote : local);
}
u8 SCNetworkReachabilityGetFlags(id t, u32 *flags) {
    reach_t *r = as_reach(t);
    if (!flags) return 0;
    *flags = r ? reach_flags(r) : RF_REACHABLE;
    return 1;
}
u8 SCNetworkReachabilitySetCallback(id t, sc_cb_t cb, sc_ctx_t *ctx) {
    reach_t *r = as_reach(t);
    if (!r) return 0;
    if (r->ctx.release && r->ctx.info) r->ctx.release(r->ctx.info);
    memset(&r->ctx, 0, sizeof r->ctx);
    r->cb = cb;
    if (ctx) {
        r->ctx = *ctx;
        if (r->ctx.retain && r->ctx.info) r->ctx.info = (void *)r->ctx.retain(r->ctx.info);
    }
    return 1;
}
static void reach_deliver(void *p) {
    reach_t *r = p;
    if (r->cb && (r->queue || r->scheduled)) r->cb((id)r, reach_flags(r), r->ctx.info);
    objc_release((id)r);
}
static void reach_kick(reach_t *r, void *q) {
    if (!r->cb) return;
    objc_retain((id)r);
    dispatch_async_f(q ? q : dispatch_get_main_queue(), r, reach_deliver);
}
u8 SCNetworkReachabilitySetDispatchQueue(id t, void *queue) {
    reach_t *r = as_reach(t);
    if (!r) return 0;
    r->queue = queue;
    if (queue) reach_kick(r, queue);
    return 1;
}
u8 SCNetworkReachabilityScheduleWithRunLoop(id t, void *rl, id mode) {
    reach_t *r = as_reach(t);
    (void)rl; (void)mode;
    if (!r) return 0;
    r->scheduled = 1;
    reach_kick(r, 0);
    return 1;
}
u8 SCNetworkReachabilityUnscheduleFromRunLoop(id t, void *rl, id mode) {
    reach_t *r = as_reach(t);
    (void)rl; (void)mode;
    if (!r) return 0;
    r->scheduled = 0;
    return 1;
}

/* Sem Wi-Fi (o mOS é cabo/virtio) e sem a permissão de localização, a Apple devolve NULL. */
id CNCopyCurrentNetworkInfo(id ifname) { (void)ifname; return 0; }
id CNCopySupportedInterfaces(void) { return 0; }

/* CFHostGetNames fica em cfhost.c, onde o host guarda o array. */
id CFHostCopyNameString(id host);
void CFStreamCreatePairWithSocketToCFHost(void *alloc, id host, int port, id *rd, id *wr) {
    id name = host ? CFHostCopyNameString(host) : 0;
    if (!name) { if (rd) *rd = 0; if (wr) *wr = 0; return; }
    CFStreamCreatePairWithSocketToHost(alloc, name, port, rd, wr);
    objc_release(name);
}

/* nw_protocol_options de TLS: o Network.framework do mOS não separa as opções de segurança;
 * a "sec_protocol_options" é o próprio objeto de opções (retido), como um handle opaco. */
id nw_tls_copy_sec_protocol_options(id options) { return options ? objc_retain(options) : 0; }
