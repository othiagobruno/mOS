/*
 * mOS — streams do Foundation e do CoreFoundation/CFNetwork sobre mnet_stream.c (docs/rede-apps.md, item 9):
 * NSStream, NSInputStream, NSOutputStream (ponte gratuita: o CFReadStreamRef é o próprio NSInputStream),
 * CFReadStream e CFWriteStream (clientes, run loop e fila de verdade), CFStreamCreateBoundPair, os pares de socket
 * (host, CFHost, fd nativo, assinatura) com TLS por kCFStreamPropertySSLSettings/SocketSecurityLevel, inclusive depois
 * do open, e SOCKS4a/5 por kCFStreamPropertySOCKSProxy, +[NSStream getStreamsToHostWithName:...] e as constantes.
 *
 * Eventos: o núcleo avisa (de qualquer fio) que o estado mudou; aqui isso sinaliza a CFRunLoopSource do stream em cada
 * run loop agendado e/ou posta na fila (CFReadStreamSetDispatchQueue). A entrega pega os eventos do núcleo e chama o
 * cliente CF (se o evento está na máscara) e o delegate (stream:handleEvent:; o padrão é o próprio stream).
 * Subclasses do app (NSInputStream do HTTPBodyStream, AFMultipartBodyStream) recebem os pedidos CF pelos métodos
 * privados _scheduleInCFRunLoop:forMode:, _unscheduleFromCFRunLoop:forMode: e _setCFClientFlags:callback:context:.
 */
#include "foundation.h"
#include "libc.h"
#include "mnet.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void *CFRunLoopSourceCreate(void *alloc, long order, void *ctx);
void CFRunLoopSourceSignal(void *s);
void CFRunLoopSourceInvalidate(void *s);
void CFRunLoopAddSource(void *rl, void *s, void *mode);
void CFRunLoopRemoveSource(void *rl, void *s, void *mode);
void CFRunLoopWakeUp(void *rl);
void CFRelease(const void *o);
const void *CFRetain(const void *o);
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *));
void dispatch_retain(void *q);
void dispatch_release(void *q);
void os_unfair_lock_lock(void *l);
void os_unfair_lock_unlock(void *l);
long SecTrustGetCertificateCount(id t);
id SecTrustGetCertificateAtIndex(id t, long i);
id CFHostGetAddressing(id host, unsigned char *resolved);
id CFHostCopyNameString(id host);
int socket(int, int, int);
int connect(int, const void *, unsigned);
int close(int);

#define SEL_(s) sel_registerName(s)
static id cls(const char *n) { return (id)(void *)cx_class_named(n); }
static BOOL responds(id o, const char *s) { return o && MSG(BOOL, o, "respondsToSelector:", SEL_(s)); }
static id ns(const char *s) { return cx_str(s); }
static const char *cs(id o) { return o && cx_is_str(o) ? cx_cstr(o) : 0; }
static int str_eq(id a, const void *b) { return a && b && (a == (id)b || MSG(BOOL, a, "isEqual:", (id)b)); }
static long long numv(id v) {
    if (!v) return 0;
    if (responds(v, "longLongValue")) return MSG0(long long, v, "longLongValue");
    return 0;
}
static int boolv(id v) { return v && responds(v, "boolValue") && MSG0(BOOL, v, "boolValue"); }

/* ---------------------------------------------------------------- constantes */
extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } ks_t;
#define K(var, text) \
    static ks_t var##_cs = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; \
    const void *var = &var##_cs;
/* CFStream.h / CFSocketStream.h */
K(kCFStreamPropertyDataWritten, "kCFStreamPropertyDataWritten")
K(kCFStreamPropertyAppendToFile, "kCFStreamPropertyAppendToFile")
K(kCFStreamPropertyFileCurrentOffset, "kCFStreamPropertyFileCurrentOffset")
K(kCFStreamPropertySocketNativeHandle, "kCFStreamPropertySocketNativeHandle")
K(kCFStreamPropertySocketRemoteHostName, "kCFStreamPropertySocketRemoteHostName")
K(kCFStreamPropertySocketRemotePortNumber, "kCFStreamPropertySocketRemotePortNumber")
K(kCFStreamPropertyShouldCloseNativeSocket, "kCFStreamPropertyShouldCloseNativeSocket")
K(kCFStreamPropertySocketSecurityLevel, "kCFStreamPropertySocketSecurityLevel")
K(kCFStreamSocketSecurityLevelNone, "kCFStreamSocketSecurityLevelNone")
K(kCFStreamSocketSecurityLevelSSLv2, "kCFStreamSocketSecurityLevelSSLv2")
K(kCFStreamSocketSecurityLevelSSLv3, "kCFStreamSocketSecurityLevelSSLv3")
K(kCFStreamSocketSecurityLevelTLSv1, "kCFStreamSocketSecurityLevelTLSv1")
K(kCFStreamSocketSecurityLevelNegotiatedSSL, "kCFStreamSocketSecurityLevelNegotiatedSSL")
K(kCFStreamPropertySSLPeerCertificates, "kCFStreamPropertySSLPeerCertificates")
K(kCFStreamPropertySSLPeerTrust, "kCFStreamPropertySSLPeerTrust")
K(kCFStreamPropertySSLContext, "kCFStreamPropertySSLContext")
K(kCFStreamPropertySOCKSProxy, "kCFStreamPropertySOCKSProxy")
K(kCFStreamPropertySOCKSProxyHost, "SOCKSProxy")
K(kCFStreamPropertySOCKSProxyPort, "SOCKSPort")
K(kCFStreamPropertySOCKSVersion, "kCFStreamPropertySOCKSVersion")
K(kCFStreamSocketSOCKSVersion4, "kCFStreamSocketSOCKSVersion4")
K(kCFStreamSocketSOCKSVersion5, "kCFStreamSocketSOCKSVersion5")
K(kCFStreamPropertySOCKSUser, "kCFStreamPropertySOCKSUser")
K(kCFStreamPropertySOCKSPassword, "kCFStreamPropertySOCKSPassword")
K(kCFStreamPropertyConnectionIsCellular, "kCFStreamPropertyConnectionIsCellular")
K(kCFStreamPropertyNoCellular, "kCFStreamPropertyNoCellular")
K(kCFStreamPropertyConnectionIsExpensive, "kCFStreamPropertyConnectionIsExpensive")
K(kCFStreamPropertyAllowExpensiveNetworkAccess, "kCFStreamPropertyAllowExpensiveNetworkAccess")
K(kCFStreamPropertyAllowConstrainedNetworkAccess, "kCFStreamPropertyAllowConstrainedNetworkAccess")
/* NSStream.h */
K(NSStreamSocketSecurityLevelKey, "kCFStreamPropertySocketSecurityLevel")
K(NSStreamSocketSecurityLevelNone, "kCFStreamSocketSecurityLevelNone")
K(NSStreamSocketSecurityLevelSSLv2, "kCFStreamSocketSecurityLevelSSLv2")
K(NSStreamSocketSecurityLevelSSLv3, "kCFStreamSocketSecurityLevelSSLv3")
K(NSStreamSocketSecurityLevelTLSv1, "kCFStreamSocketSecurityLevelTLSv1")
K(NSStreamSocketSecurityLevelNegotiatedSSL, "kCFStreamSocketSecurityLevelNegotiatedSSL")
K(NSStreamSOCKSProxyConfigurationKey, "kCFStreamPropertySOCKSProxy")
K(NSStreamSOCKSProxyHostKey, "SOCKSProxy")
K(NSStreamSOCKSProxyPortKey, "SOCKSPort")
K(NSStreamSOCKSProxyVersionKey, "kCFStreamPropertySOCKSVersion")
K(NSStreamSOCKSProxyUserKey, "kCFStreamPropertySOCKSUser")
K(NSStreamSOCKSProxyPasswordKey, "kCFStreamPropertySOCKSPassword")
K(NSStreamSOCKSProxyVersion4, "kCFStreamSocketSOCKSVersion4")
K(NSStreamSOCKSProxyVersion5, "kCFStreamSocketSOCKSVersion5")
K(NSStreamNetworkServiceType, "kCFStreamNetworkServiceType")
K(NSStreamNetworkServiceTypeVoIP, "voip")
K(NSStreamNetworkServiceTypeVideo, "video")
K(NSStreamNetworkServiceTypeBackground, "background")
K(NSStreamNetworkServiceTypeVoice, "voice")
K(NSStreamNetworkServiceTypeCallSignaling, "callSignaling")
K(NSStreamSocketSSLErrorDomain, "NSStreamSocketSSLErrorDomain")
K(NSStreamSOCKSErrorDomain, "NSStreamSOCKSErrorDomain")
/* domínios de erro do CFStreamError (SInt32) */
const int kCFStreamErrorDomainSSL = 3;
const int kCFStreamErrorDomainHTTP = 4;
const int kCFStreamErrorDomainSOCKS = 5;
const int kCFStreamErrorDomainFTP = 6;
const int kCFStreamErrorDomainNetServices = 10;
const int kCFStreamErrorDomainMach = 11;
const int kCFStreamErrorDomainNetDB = 12;
const int kCFStreamErrorDomainSystemConfiguration = 17;
/* já definidas em outros arquivos do libSystem */
extern const void *kCFStreamPropertySSLSettings, *kCFStreamSSLPeerName, *kCFStreamSSLLevel, *kCFStreamSSLValidatesCertificateChain;
extern const void *NSStreamDataWrittenToMemoryStreamKey, *NSStreamFileCurrentOffsetKey;

/* ---------------------------------------------------------------- estado por stream */
typedef struct { long version; void *info; const void *(*retain)(const void *); void (*release)(const void *);
                 const void *(*copyDescription)(const void *); } cl_ctx_t;           /* CFStreamClientContext */
typedef void (*cl_cb_t)(id stream, unsigned long ev, void *info);
typedef struct { long domain; int error; } cf_serr_t;                               /* CFStreamError */
typedef struct {
    long version; void *info; const void *(*retain)(const void *); void (*release)(const void *);
    void *copyDescription, *equal, *hash;
    void (*schedule)(void *, void *, void *); void (*cancel)(void *, void *, void *);
    void (*perform)(void *);
} src_ctx_t;
#define MAX_RL 8
/* stream com propriedades próprias (o do pedido HTTP em cfhttp.c) */
typedef struct {
    id (*get)(void *ext, id key);                       /* 0 = não é minha */
    int (*set)(void *ext, id key, id v);                /* -1 = não é minha, 0 recusou, 1 aceitou */
    void (*release)(void *ext);
} cx_stream_ext_t;

typedef struct ns {
    unsigned lock;                       /* os_unfair_lock: campos de agendamento e refs */
    int refs;                            /* objeto + entregas na fila em voo */
    id obj;                              /* sem posse; zerado no dealloc */
    mnet_stream_t *m;
    id delegate;                         /* sem posse; 0 = o próprio stream */
    id props;                            /* propriedades do app (NSMutableDictionary) */
    id err;                              /* NSError em cache */
    unsigned long cflags;
    cl_cb_t cb;
    cl_ctx_t ctx;
    void *src;
    void *rl[MAX_RL];
    int nrl;
    void *queue;
    int q_posted;
    int self_ret;                        /* o stream se segura enquanto está numa fila */
    id ssl_settings, sec_level, socks, svc_type;
    char host[256]; int port;
    int close_native;                    /* kCFStreamPropertyShouldCloseNativeSocket */
    int native;                          /* criado sobre um fd do app */
    void *ext;
    const cx_stream_ext_t *ext_ops;
} ns_t;

cx_class_t NSStream_c __asm__("OBJC_CLASS_$_NSStream"), NSStream_m __asm__("OBJC_METACLASS_$_NSStream");
cx_class_t NSInputStream_c __asm__("OBJC_CLASS_$_NSInputStream"), NSInputStream_m __asm__("OBJC_METACLASS_$_NSInputStream");
cx_class_t NSOutputStream_c __asm__("OBJC_CLASS_$_NSOutputStream"), NSOutputStream_m __asm__("OBJC_METACLASS_$_NSOutputStream");

static ns_t **slot(id o) { return (ns_t **)((u8 *)o + 8); }
/* o estado só existe em instâncias de NSStream (e subclasses); outro objeto devolve 0 */
static int is_stream(id o) { return o && MSG(BOOL, o, "isKindOfClass:", (id)(void *)&NSStream_c); }
static ns_t *NS(id o) {
    if (!o || !is_stream(o)) return 0;
    ns_t **p = slot(o);
    if (!*p) {
        ns_t *n = calloc(1, sizeof *n);
        if (!n) return 0;
        n->refs = 1; n->obj = o; n->close_native = 0;
        *p = n;
    }
    return *p;
}
static void ns_unref(ns_t *n) {
    os_unfair_lock_lock(&n->lock);
    int left = --n->refs;
    os_unfair_lock_unlock(&n->lock);
    if (left == 0) free(n);
}

/* ---------------------------------------------------------------- entrega de eventos */
static void deliver(ns_t *n);
static void deliver_q(void *p) {
    ns_t *n = p;
    os_unfair_lock_lock(&n->lock);
    n->q_posted = 0;
    os_unfair_lock_unlock(&n->lock);
    deliver(n);
    ns_unref(n);
}
/* chamado pelo núcleo (qualquer fio, com a trava dele presa): só sinaliza */
static void notify(void *p) {
    ns_t *n = p;
    os_unfair_lock_lock(&n->lock);
    if (n->obj) {
        if (n->queue && !n->q_posted) { n->q_posted = 1; n->refs++; dispatch_async_f(n->queue, n, deliver_q); }
        if (n->src && n->nrl) {
            CFRunLoopSourceSignal(n->src);
            for (int i = 0; i < n->nrl; i++) CFRunLoopWakeUp(n->rl[i]);
        }
    }
    os_unfair_lock_unlock(&n->lock);
}
static void perform(void *p) { deliver((ns_t *)p); }
static void deliver(ns_t *n) {
    os_unfair_lock_lock(&n->lock);
    id o = n->obj;
    if (o) objc_retain(o);
    os_unfair_lock_unlock(&n->lock);
    if (!o) return;
    unsigned ev = n->m ? mnet_stream_events(n->m) : 0;
    static const unsigned order[] = {MNET_SE_OPEN, MNET_SE_BYTES, MNET_SE_SPACE, MNET_SE_END, MNET_SE_ERROR};
    for (int i = 0; i < 5 && n->obj; i++) {
        unsigned e = ev & order[i];
        if (!e) continue;
        if (n->cb && (n->cflags & e)) n->cb(o, e, n->ctx.info);
        if (!n->obj) break;
        id d = n->delegate ? n->delegate : o;
        if (responds(d, "stream:handleEvent:")) MSG(void, d, "stream:handleEvent:", o, (unsigned long)e);
    }
    objc_release(o);
}
static void ensure_src(ns_t *n) {
    if (n->src) return;
    src_ctx_t c;
    memset(&c, 0, sizeof c);
    c.info = n; c.perform = perform;
    n->src = CFRunLoopSourceCreate(0, 0, &c);
}
static void kick(ns_t *n) { if (n->m) notify(n); }        /* reavalia (agendou, trocou cliente) */
static void sched_rl(ns_t *n, void *rl, id mode) {
    if (!n || !rl) return;
    ensure_src(n);
    os_unfair_lock_lock(&n->lock);
    int have = 0;
    for (int i = 0; i < n->nrl; i++) if (n->rl[i] == rl) have = 1;
    if (!have && n->nrl < MAX_RL) n->rl[n->nrl++] = rl;
    os_unfair_lock_unlock(&n->lock);
    if (!have) CFRunLoopAddSource(rl, n->src, (void *)mode);
    kick(n);
}
static void unsched_rl(ns_t *n, void *rl, id mode) {
    if (!n || !rl) return;
    int had = 0;
    os_unfair_lock_lock(&n->lock);
    for (int i = 0; i < n->nrl; i++) if (n->rl[i] == rl) { n->rl[i] = n->rl[--n->nrl]; had = 1; break; }
    os_unfair_lock_unlock(&n->lock);
    if (had && n->src) CFRunLoopRemoveSource(rl, n->src, (void *)mode);
}
static void set_queue(ns_t *n, void *q) {
    if (!n) return;
    if (q) dispatch_retain(q);
    int st = n->m ? mnet_stream_status(n->m) : 0;                     /* fora da trava: o aviso do núcleo a pega */
    os_unfair_lock_lock(&n->lock);
    void *old = n->queue;
    n->queue = q;
    int want_ret = q && st != MNET_SS_CLOSED;
    int drop = 0;
    if (want_ret && !n->self_ret) { n->self_ret = 1; objc_retain(n->obj); }
    else if (!want_ret && n->self_ret) { n->self_ret = 0; drop = 1; }
    id o = n->obj;
    os_unfair_lock_unlock(&n->lock);
    if (old) dispatch_release(old);
    if (q) kick(n);
    if (drop) objc_release(o);
}
static void set_client(ns_t *n, unsigned long flags, cl_cb_t cb, const cl_ctx_t *ctx) {
    void *old_info = n->ctx.info;
    void (*old_rel)(const void *) = n->ctx.release;
    n->cflags = cb ? flags : 0;
    n->cb = cb;
    memset(&n->ctx, 0, sizeof n->ctx);
    if (cb && ctx) {
        n->ctx = *ctx;
        if (ctx->retain && ctx->info) n->ctx.info = (void *)ctx->retain(ctx->info);
    }
    if (old_rel && old_info) old_rel(old_info);
    kick(n);
}
static void attach_core(ns_t *n, mnet_stream_t *m) {
    n->m = m;
    if (m) mnet_stream_set_notify(m, notify, n);
}

/* ---------------------------------------------------------------- erros */
static void dput(id d, const char *k, long v) { id n = MSG(id, cls("NSNumber"), "numberWithLong:", v); MSG(void, d, "setObject:forKey:", n, ns(k)); }
static id make_error(int dom, long code, const char *msg) {
    const char *d = "NSPOSIXErrorDomain";
    long c = code;
    if (dom == MNET_SD_SSL || dom == MNET_SD_OSSTATUS) d = "NSOSStatusErrorDomain";
    else if (dom == MNET_SD_NETDB) { d = "kCFErrorDomainCFNetwork"; c = 2; }               /* kCFHostErrorUnknown */
    else if (dom == MNET_SD_SOCKS) {
        d = "kCFErrorDomainCFNetwork";
        int sub = (int)(code >> 16) & 0xffff;
        c = sub == 1 ? 101 : sub == 2 ? 110 : sub == 4 ? 124 : sub == 5 ? 122 : 120;   /* kCFSOCKS*Error */
    } else if (dom == MNET_SD_HTTP) d = "kCFErrorDomainCFNetwork";
    id info = MSG0(id, MSG0(id, cls("NSMutableDictionary"), "alloc"), "init");
    if (msg && *msg) MSG(void, info, "setObject:forKey:", ns(msg), ns("NSLocalizedDescription"));
    if (dom == MNET_SD_NETDB) dput(info, "kCFGetAddrInfoFailureKey", code);
    if (dom == MNET_SD_SOCKS) {
        dput(info, "kCFSOCKSStatusCodeKey", code & 0xffff);
        dput(info, "kCFSOCKSVersionKey", (code >> 16) & 0xffff);
    }
    id e = MSG(id, cls("NSError"), "errorWithDomain:code:userInfo:", ns(d), (long)c, info);
    objc_release(info);
    return e;
}
static id stream_error(ns_t *n) {
    long code = 0; const char *msg = 0;
    int dom = n && n->m ? mnet_stream_error(n->m, &code, &msg) : 0;
    if (!dom) return 0;
    if (!n->err) n->err = objc_retain(make_error(dom, code, msg));
    return n->err;
}

/* ---------------------------------------------------------------- propriedades */
static void apply_ssl(ns_t *n) {
    mnet_sockopts_t *o = mnet_stream_sockopts(n->m);
    if (!o) return;
    int want = 0;
    if (n->sec_level && !str_eq(n->sec_level, kCFStreamSocketSecurityLevelNone)) want = 1;
    if (n->ssl_settings) {
        want = 1;
        id lvl = MSG(id, n->ssl_settings, "objectForKey:", (id)kCFStreamSSLLevel);
        if (lvl && str_eq(lvl, kCFStreamSocketSecurityLevelNone)) want = 0;
        id peer = MSG(id, n->ssl_settings, "objectForKey:", (id)kCFStreamSSLPeerName);
        const char *p = cs(peer);
        if (p) { size_t l = strlen(p); if (l >= sizeof o->peer_name) l = sizeof o->peer_name - 1; memcpy(o->peer_name, p, l); o->peer_name[l] = 0; }
    }
    if (want && !o->tls) {
        int st = mnet_stream_status(n->m);
        if (st == MNET_SS_NOT_OPEN) o->tls = 1;
        else mnet_stream_starttls(n->m);                               /* STARTTLS num socket já aberto */
    } else if (!want && mnet_stream_status(n->m) == MNET_SS_NOT_OPEN) o->tls = 0;
}
static void apply_socks(ns_t *n, id d) {
    mnet_sockopts_t *o = mnet_stream_sockopts(n->m);
    if (!o) return;
    o->socks_host[0] = 0; o->socks_port = 0; o->socks_version = 5; o->socks_user[0] = o->socks_pass[0] = 0;
    if (!d) return;
    const char *h = cs(MSG(id, d, "objectForKey:", (id)kCFStreamPropertySOCKSProxyHost));
    if (!h || !*h) return;
    id en = MSG(id, d, "objectForKey:", ns("SOCKSEnable"));            /* dicionário do sistema */
    if (en && !numv(en)) return;
    size_t l = strlen(h); if (l >= sizeof o->socks_host) l = sizeof o->socks_host - 1;
    memcpy(o->socks_host, h, l); o->socks_host[l] = 0;
    long long p = numv(MSG(id, d, "objectForKey:", (id)kCFStreamPropertySOCKSProxyPort));
    o->socks_port = p > 0 && p < 65536 ? (int)p : 1080;
    id v = MSG(id, d, "objectForKey:", (id)kCFStreamPropertySOCKSVersion);
    if (v && str_eq(v, kCFStreamSocketSOCKSVersion4)) o->socks_version = 4;
    const char *u = cs(MSG(id, d, "objectForKey:", (id)kCFStreamPropertySOCKSUser));
    const char *pw = cs(MSG(id, d, "objectForKey:", (id)kCFStreamPropertySOCKSPassword));
    if (u) { l = strlen(u); if (l >= sizeof o->socks_user) l = sizeof o->socks_user - 1; memcpy(o->socks_user, u, l); o->socks_user[l] = 0; }
    if (pw) { l = strlen(pw); if (l >= sizeof o->socks_pass) l = sizeof o->socks_pass - 1; memcpy(o->socks_pass, pw, l); o->socks_pass[l] = 0; }
}
static void store(id *slotp, id v) { id old = *slotp; *slotp = v ? objc_retain(v) : 0; if (old) objc_release(old); }
static BOOL set_prop(ns_t *n, id key, id v) {
    if (!n || !key) return NO;
    mnet_stream_t *m = n->m;
    if (n->ext_ops && n->ext_ops->set) { int r = n->ext_ops->set(n->ext, key, v); if (r >= 0) return r ? YES : NO; }
    if (str_eq(key, kCFStreamPropertyFileCurrentOffset)) return m && mnet_stream_seek(m, numv(v)) == 0;
    if (str_eq(key, kCFStreamPropertyAppendToFile)) return m && mnet_stream_set_append(m, boolv(v)) == 0;   /* antes do open */
    if (mnet_stream_is_socket(m)) {
        if (str_eq(key, kCFStreamPropertySSLSettings)) { store(&n->ssl_settings, v); apply_ssl(n); return YES; }
        if (str_eq(key, kCFStreamPropertySocketSecurityLevel)) { store(&n->sec_level, v); apply_ssl(n); return YES; }
        if (str_eq(key, kCFStreamPropertySOCKSProxy)) {
            if (mnet_stream_status(m) != MNET_SS_NOT_OPEN) return NO;
            store(&n->socks, v); apply_socks(n, v); return YES;
        }
        if (str_eq(key, kCFStreamPropertyShouldCloseNativeSocket)) {
            n->close_native = boolv(v);
            if (n->native) cx_mnet_conn_set_close(mnet_stream_conn(m), n->close_native);
            return YES;
        }
        if (str_eq(key, NSStreamNetworkServiceType)) { store(&n->svc_type, v); return YES; }
    }
    if (!n->props) n->props = MSG0(id, MSG0(id, cls("NSMutableDictionary"), "alloc"), "init");
    if (v) MSG(void, n->props, "setObject:forKey:", v, key);
    else MSG(void, n->props, "removeObjectForKey:", key);
    return YES;
}
static id get_prop(ns_t *n, id key) {
    if (!n || !key) return 0;
    mnet_stream_t *m = n->m;
    if (n->ext_ops && n->ext_ops->get) { id v = n->ext_ops->get(n->ext, key); if (v) return v; }
    if (m && (str_eq(key, kCFStreamPropertyDataWritten) || str_eq(key, NSStreamDataWrittenToMemoryStreamKey))) {
        size_t len; const uint8_t *d = mnet_stream_mem_data(m, &len);
        if (d) return MSG(id, cls("NSData"), "dataWithBytes:length:", d, (u64)len);
    }
    if (m && str_eq(key, kCFStreamPropertyFileCurrentOffset)) {
        long long off = mnet_stream_offset(m);
        if (off >= 0) return MSG(id, cls("NSNumber"), "numberWithLongLong:", off);
    }
    if (mnet_stream_is_socket(m)) {
        void *conn = mnet_stream_conn(m);
        if (str_eq(key, kCFStreamPropertySSLSettings)) return n->ssl_settings;
        if (str_eq(key, kCFStreamPropertySOCKSProxy)) return n->socks;
        if (str_eq(key, NSStreamNetworkServiceType)) return n->svc_type;
        if (str_eq(key, kCFStreamPropertySocketSecurityLevel)) {
            if (n->sec_level) return n->sec_level;
            return mnet_stream_tls_active(m) ? (id)kCFStreamSocketSecurityLevelNegotiatedSSL : (id)kCFStreamSocketSecurityLevelNone;
        }
        if (str_eq(key, kCFStreamPropertySocketNativeHandle) && conn) {
            int fd = cx_mnet_conn_fd(conn);
            return MSG(id, cls("NSData"), "dataWithBytes:length:", &fd, (u64)sizeof fd);
        }
        if (str_eq(key, kCFStreamPropertySocketRemoteHostName) && n->host[0]) return ns(n->host);
        if (str_eq(key, kCFStreamPropertySocketRemotePortNumber) && n->port) return MSG(id, cls("NSNumber"), "numberWithInt:", n->port);
        if (str_eq(key, kCFStreamPropertyShouldCloseNativeSocket)) return MSG(id, cls("NSNumber"), "numberWithBool:", (BOOL)n->close_native);
        if (str_eq(key, kCFStreamPropertySSLContext) && conn) return (id)cx_mnet_conn_ssl(conn);
        if ((str_eq(key, kCFStreamPropertySSLPeerTrust) || str_eq(key, kCFStreamPropertySSLPeerCertificates)) && conn &&
            mnet_stream_tls_active(m)) {
            const mnet_io_t *io = cx_mnet_io();
            id t = io->copy_trust ? (id)io->copy_trust(io->env, conn) : 0;
            if (!t) return 0;
            objc_autorelease(t);
            if (str_eq(key, kCFStreamPropertySSLPeerTrust)) return t;
            id arr = MSG0(id, cls("NSMutableArray"), "array");
            long cnt = SecTrustGetCertificateCount(t);
            for (long i = 0; i < cnt; i++) { id c = SecTrustGetCertificateAtIndex(t, i); if (c) MSG(void, arr, "addObject:", c); }
            return arr;
        }
        if (str_eq(key, kCFStreamPropertyConnectionIsCellular) || str_eq(key, kCFStreamPropertyConnectionIsExpensive))
            return MSG(id, cls("NSNumber"), "numberWithBool:", (BOOL)0);
    }
    return n->props ? MSG(id, n->props, "objectForKey:", key) : 0;
}

/* ---------------------------------------------------------------- NSStream */
static void st_dealloc(id s, SEL c) {
    ns_t *n = *slot(s);
    if (n) {
        os_unfair_lock_lock(&n->lock);
        n->obj = 0;
        void *rls[MAX_RL]; int nrl = n->nrl;
        memcpy(rls, n->rl, sizeof rls);
        n->nrl = 0;
        void *q = n->queue; n->queue = 0;
        os_unfair_lock_unlock(&n->lock);
        if (n->m) mnet_stream_set_notify(n->m, 0, 0);                 /* nenhum aviso em voo depois daqui */
        for (int i = 0; i < nrl; i++) CFRunLoopRemoveSource(rls[i], n->src, 0);
        if (n->src) { CFRunLoopSourceInvalidate(n->src); CFRelease(n->src); n->src = 0; }
        if (q) dispatch_release(q);
        if (n->ctx.release && n->ctx.info) n->ctx.release(n->ctx.info);
        mnet_stream_free(n->m); n->m = 0;
        if (n->ext_ops && n->ext_ops->release) n->ext_ops->release(n->ext);
        n->ext = 0; n->ext_ops = 0;
        if (n->props) objc_release(n->props);
        if (n->err) objc_release(n->err);
        if (n->ssl_settings) objc_release(n->ssl_settings);
        if (n->sec_level) objc_release(n->sec_level);
        if (n->socks) objc_release(n->socks);
        if (n->svc_type) objc_release(n->svc_type);
        *slot(s) = 0;
        ns_unref(n);
    }
    IMP sup = cx_lookup(cx_class_named("NSObject"), c);
    if (sup) ((void (*)(id, SEL))sup)(s, c);
}
static void st_open(id s, SEL c) { (void)c; ns_t *n = NS(s); if (n && n->m) mnet_stream_open(n->m); }
static void st_close(id s, SEL c) {
    (void)c;
    ns_t *n = NS(s);
    if (!n || !n->m) return;
    mnet_stream_close(n->m);
    os_unfair_lock_lock(&n->lock);
    int drop = n->self_ret;
    n->self_ret = 0;
    os_unfair_lock_unlock(&n->lock);
    if (drop) objc_release(s);
}
static id st_delegate(id s, SEL c) { (void)c; ns_t *n = NS(s); return n && n->delegate ? n->delegate : s; }
static void st_set_delegate(id s, SEL c, id d) { (void)c; ns_t *n = NS(s); if (n) n->delegate = d == s ? 0 : d; }
static id st_prop(id s, SEL c, id key) { (void)c; return get_prop(NS(s), key); }
static BOOL st_set_prop(id s, SEL c, id v, id key) { (void)c; return set_prop(NS(s), key, v); }
static void *cf_loop_of(id rl) {
    if (!rl) return 0;
    if (responds(rl, "getCFRunLoop")) return MSG0(void *, rl, "getCFRunLoop");
    return (void *)rl;                                                 /* já é CFRunLoop */
}
static void st_schedule(id s, SEL c, id rl, id mode) { (void)c; sched_rl(NS(s), cf_loop_of(rl), mode); }
static void st_unschedule(id s, SEL c, id rl, id mode) { (void)c; unsched_rl(NS(s), cf_loop_of(rl), mode); }
static unsigned long st_status(id s, SEL c) { (void)c; ns_t *n = NS(s); return n && n->m ? (unsigned long)mnet_stream_status(n->m) : 0; }
static id st_error(id s, SEL c) { (void)c; return stream_error(NS(s)); }
/* métodos privados que o CFNetwork usa para subclasses; nas nossas, fazem o mesmo que as funções CF */
static void st_cf_sched(id s, SEL c, void *rl, id mode) { (void)c; sched_rl(NS(s), rl, mode); }
static void st_cf_unsched(id s, SEL c, void *rl, id mode) { (void)c; unsched_rl(NS(s), rl, mode); }
static BOOL st_cf_client(id s, SEL c, unsigned long flags, cl_cb_t cb, cl_ctx_t *ctx) {
    (void)c; ns_t *n = NS(s); if (!n) return NO; set_client(n, flags, cb, ctx); return YES;
}

/* ---------------------------------------------------------------- NSInputStream */
static id with_core(id s, mnet_stream_t *m) {
    if (!s) { mnet_stream_free(m); return 0; }
    ns_t *n = NS(s);
    if (!m || !n) { mnet_stream_free(m); objc_release(s); return 0; }
    attach_core(n, m);
    return s;
}
static id is_init_data(id s, SEL c, id d) {
    (void)c;
    u64 len = d ? MSG0(u64, d, "length") : 0;
    return with_core(s, mnet_stream_mem_in(len ? MSG0(const void *, d, "bytes") : "", (size_t)len));
}
static id is_init_file(id s, SEL c, id path) { (void)c; const char *p = cs(path); return with_core(s, p ? mnet_stream_file_in(p) : 0); }
static id url_path(id url) { return url && responds(url, "isFileURL") && MSG0(BOOL, url, "isFileURL") ? MSG0(id, url, "path") : 0; }
static id is_init_url(id s, SEL c, id url) { return is_init_file(s, c, url_path(url)); }
static id is_with_data(id k, SEL c, id d) { id s = is_init_data(cx_alloc_instance((cx_class_t *)k), c, d); return s ? objc_autorelease(s) : 0; }
static id is_with_file(id k, SEL c, id p) { id s = is_init_file(cx_alloc_instance((cx_class_t *)k), c, p); return s ? objc_autorelease(s) : 0; }
static id is_with_url(id k, SEL c, id u) { id s = is_init_url(cx_alloc_instance((cx_class_t *)k), c, u); return s ? objc_autorelease(s) : 0; }
static long is_read(id s, SEL c, u8 *b, u64 max) { (void)c; ns_t *n = NS(s); return n && n->m ? mnet_stream_read(n->m, b, (size_t)max) : -1; }
static BOOL is_has_bytes(id s, SEL c) { (void)c; ns_t *n = NS(s); return n && n->m && mnet_stream_has_bytes(n->m); }
static BOOL is_get_buffer(id s, SEL c, u8 **b, u64 *len) {
    (void)c;
    ns_t *n = NS(s);
    uint8_t *p = 0; size_t l = 0;
    if (!n || !n->m || !mnet_stream_get_buffer(n->m, &p, &l)) return NO;
    if (b) *b = p;
    if (len) *len = l;
    return YES;
}

/* ---------------------------------------------------------------- NSOutputStream */
static id os_init_mem(id s, SEL c) { (void)c; return with_core(s, mnet_stream_mem_out()); }
static id os_init_buf(id s, SEL c, u8 *b, u64 cap) { (void)c; return with_core(s, mnet_stream_buf_out(b, (size_t)cap)); }
static id os_init_file(id s, SEL c, id path, BOOL app) { (void)c; const char *p = cs(path); return with_core(s, p ? mnet_stream_file_out(p, app) : 0); }
static id os_init_url(id s, SEL c, id url, BOOL app) { return os_init_file(s, c, url_path(url), app); }
static id os_mem(id k, SEL c) { id s = os_init_mem(cx_alloc_instance((cx_class_t *)k), c); return s ? objc_autorelease(s) : 0; }
static id os_buf(id k, SEL c, u8 *b, u64 cap) { id s = os_init_buf(cx_alloc_instance((cx_class_t *)k), c, b, cap); return s ? objc_autorelease(s) : 0; }
static id os_file(id k, SEL c, id p, BOOL a) { id s = os_init_file(cx_alloc_instance((cx_class_t *)k), c, p, a); return s ? objc_autorelease(s) : 0; }
static id os_url(id k, SEL c, id u, BOOL a) { id s = os_init_url(cx_alloc_instance((cx_class_t *)k), c, u, a); return s ? objc_autorelease(s) : 0; }
static long os_write(id s, SEL c, const u8 *b, u64 len) { (void)c; ns_t *n = NS(s); return n && n->m ? mnet_stream_write(n->m, b, (size_t)len) : -1; }
static BOOL os_has_space(id s, SEL c) { (void)c; ns_t *n = NS(s); return n && n->m && mnet_stream_has_space(n->m); }

/* ---------------------------------------------------------------- pares (sockets e ligado) */
static id new_in(mnet_stream_t *m) { return with_core(cx_alloc_instance(&NSInputStream_c), m); }
static id new_out(mnet_stream_t *m) { return with_core(cx_alloc_instance(&NSOutputStream_c), m); }
static void pair_out(mnet_stream_t *r, mnet_stream_t *w, id *rd, id *wr, const char *host, int port, int native) {
    id a = new_in(r), b = new_out(w);
    ns_t *na = NS(a), *nb = NS(b);
    if (host) for (ns_t *x = na; x; x = x == na ? nb : 0) {
        size_t l = strlen(host); if (l >= sizeof x->host) l = sizeof x->host - 1;
        memcpy(x->host, host, l); x->host[l] = 0; x->port = port;
    }
    if (na) na->native = native;
    if (nb) nb->native = native;
    if (rd) *rd = a; else if (a) objc_release(a);
    if (wr) *wr = b; else if (b) objc_release(b);
}
void CFStreamCreatePairWithSocketToHost(void *alloc, id host, unsigned port, id *rd, id *wr) {
    (void)alloc;
    mnet_stream_t *r, *w;
    const char *h = cs(host);
    if (rd) *rd = 0;
    if (wr) *wr = 0;
    if (!h || mnet_stream_socket(cx_mnet_io(), h, (int)port, &r, &w)) return;
    pair_out(r, w, rd, wr, h, (int)port, 0);
}
void CFStreamCreatePairWithSocketToCFHost(void *alloc, id host, int port, id *rd, id *wr) {
    unsigned char ok = 0;
    id name = host ? CFHostCopyNameString(host) : 0;                  /* +1 */
    if (name) objc_autorelease(name);
    if (!name && host) {                                               /* só endereços: o primeiro em texto */
        id addrs = CFHostGetAddressing(host, &ok);
        if (addrs && MSG0(u64, addrs, "count")) {
            id d = MSG(id, addrs, "objectAtIndex:", (u64)0);
            const u8 *sa = MSG0(const u8 *, d, "bytes");
            char txt[64];
            if (sa && sa[1] == 2) snprintf(txt, sizeof txt, "%u.%u.%u.%u", sa[4], sa[5], sa[6], sa[7]);
            else if (sa && sa[1] == 30) {
                int k = 0;
                for (int i = 0; i < 8; i++) k += snprintf(txt + k, sizeof txt - (size_t)k, i ? ":%x" : "%x", (sa[8 + 2 * i] << 8) | sa[9 + 2 * i]);
            } else txt[0] = 0;
            if (txt[0]) name = ns(txt);
        }
    }
    CFStreamCreatePairWithSocketToHost(alloc, name, (unsigned)port, rd, wr);
}
void CFStreamCreatePairWithSocket(void *alloc, int fd, id *rd, id *wr) {
    (void)alloc;
    mnet_stream_t *r, *w;
    if (rd) *rd = 0;
    if (wr) *wr = 0;
    void *conn = cx_mnet_conn_from_fd(fd, 0);                        /* padrão do CFNetwork: não fecha o nativo */
    if (!conn) return;
    if (mnet_stream_socket_conn(cx_mnet_io(), conn, 0, &r, &w)) { cx_mnet_io()->close(0, conn); return; }
    pair_out(r, w, rd, wr, 0, 0, 1);
}
typedef struct { int family, socktype, protocol; id address; } sock_sig_t;      /* CFSocketSignature */
void CFStreamCreatePairWithPeerSocketSignature(void *alloc, const sock_sig_t *sig, id *rd, id *wr) {
    if (rd) *rd = 0;
    if (wr) *wr = 0;
    if (!sig || !sig->address) return;
    const u8 *sa = MSG0(const u8 *, sig->address, "bytes");
    unsigned len = (unsigned)MSG0(u64, sig->address, "length");
    int fd = socket(sig->family ? sig->family : sa[1], sig->socktype ? sig->socktype : 1, sig->protocol);
    if (fd < 0) return;
    if (connect(fd, sa, len)) { close(fd); return; }
    CFStreamCreatePairWithSocket(alloc, fd, rd, wr);
    ns_t *a = rd && *rd ? NS(*rd) : 0, *b = wr && *wr ? NS(*wr) : 0;
    if (a) a->close_native = 1;                                        /* o socket é nosso */
    if (b) b->close_native = 1;
    if (a || b) cx_mnet_conn_set_close(mnet_stream_conn(a ? a->m : b->m), 1);
}
void CFStreamCreateBoundPair(void *alloc, id *rd, id *wr, long bufsz) {
    (void)alloc;
    mnet_stream_t *r, *w;
    if (rd) *rd = 0;
    if (wr) *wr = 0;
    if (mnet_stream_pair(bufsz > 0 ? (size_t)bufsz : 4096, &r, &w)) return;
    pair_out(r, w, rd, wr, 0, 0, 0);
}
static void ns_get_streams(id k, SEL c, id host, long port, id *in, id *out) {
    (void)k; (void)c;
    id r = 0, w = 0;
    CFStreamCreatePairWithSocketToHost(0, host, (unsigned)port, in ? &r : 0, out ? &w : 0);
    if (in) *in = r ? objc_autorelease(r) : 0;
    if (out) *out = w ? objc_autorelease(w) : 0;
}
static void ns_get_streams_host(id k, SEL c, id nshost, long port, id *in, id *out) {
    id name = nshost && responds(nshost, "name") ? MSG0(id, nshost, "name") : 0;
    if (!name && nshost && responds(nshost, "address")) name = MSG0(id, nshost, "address");
    ns_get_streams(k, c, name, port, in, out);
}
static void ns_get_bound(id k, SEL c, u64 size, id *in, id *out) {
    (void)k; (void)c;
    id r = 0, w = 0;
    CFStreamCreateBoundPair(0, in ? &r : 0, out ? &w : 0, (long)size);
    if (in) *in = r ? objc_autorelease(r) : 0;
    if (out) *out = w ? objc_autorelease(w) : 0;
}

/* NSInputStream (+1) sobre um núcleo, com propriedades próprias */
id cx_netstream_new_input(mnet_stream_t *m, void *ext, const cx_stream_ext_t *ops) {
    id s = new_in(m);
    ns_t *n = s ? NS(s) : 0;
    if (n) { n->ext = ext; n->ext_ops = ops; }
    else if (ops && ops->release) ops->release(ext);
    return s;
}

/* ---------------------------------------------------------------- CFReadStream / CFWriteStream */
u64 CFReadStreamGetTypeID(void) { return 38; }
u64 CFWriteStreamGetTypeID(void) { return 39; }
id CFReadStreamCreateWithBytesNoCopy(void *alloc, const void *b, long n, void *dealloc) {
    (void)alloc; (void)dealloc;                                        /* copia: no mOS todo alocador CF é NULL */
    return new_in(mnet_stream_mem_in(b ? b : "", n > 0 ? (size_t)n : 0));
}
static const char *file_path_of(id url, char *buf, size_t n) {
    id p = url_path(url);
    const char *s = cs(p);
    if (!s) return 0;
    snprintf(buf, n, "%s", s);
    return buf;
}
id CFReadStreamCreateWithFile(void *alloc, id url) {
    (void)alloc;
    char p[1024];
    return file_path_of(url, p, sizeof p) ? new_in(mnet_stream_file_in(p)) : 0;
}
id CFWriteStreamCreateWithFile(void *alloc, id url) {
    (void)alloc;
    char p[1024];
    /* kCFStreamPropertyAppendToFile vale até o open: o arquivo só é aberto no open */
    return file_path_of(url, p, sizeof p) ? new_out(mnet_stream_file_out(p, 0)) : 0;
}
id CFWriteStreamCreateWithBuffer(void *alloc, u8 *buf, long cap) { (void)alloc; return new_out(mnet_stream_buf_out(buf, cap > 0 ? (size_t)cap : 0)); }
id CFWriteStreamCreateWithAllocatedBuffers(void *alloc, void *buf_alloc) { (void)alloc; (void)buf_alloc; return new_out(mnet_stream_mem_out()); }

static ns_t *core_of(id s) { ns_t *n = NS(s); return n && n->m ? n : 0; }
unsigned char CFReadStreamOpen(id s) {
    if (!s) return 0;
    ns_t *n = core_of(s);
    if (!n) { MSG0(void, s, "open"); return 1; }
    int st = mnet_stream_status(n->m);
    if (st != MNET_SS_NOT_OPEN) return 0;
    mnet_stream_open(n->m);
    return mnet_stream_status(n->m) != MNET_SS_ERROR;
}
unsigned char CFWriteStreamOpen(id s) { return CFReadStreamOpen(s); }
void CFReadStreamClose(id s) { if (s) MSG0(void, s, "close"); }
void CFWriteStreamClose(id s) { if (s) MSG0(void, s, "close"); }
long CFReadStreamRead(id s, u8 *buf, long n) {
    if (!s || !buf || n < 0) return -1;
    return MSG(long, s, "read:maxLength:", buf, (u64)n);
}
long CFWriteStreamWrite(id s, const u8 *buf, long n) {
    if (!s || (!buf && n) || n < 0) return -1;
    return MSG(long, s, "write:maxLength:", buf, (u64)n);
}
const u8 *CFReadStreamGetBuffer(id s, long max, long *got) {
    u8 *p = 0; u64 l = 0;
    if (got) *got = 0;
    if (!s || !responds(s, "getBuffer:length:") || !MSG(BOOL, s, "getBuffer:length:", &p, &l) || !p) {
        if (got) *got = core_of(s) && mnet_stream_status(core_of(s)->m) == MNET_SS_ERROR ? -1 : 0;
        return 0;
    }
    if (max > 0 && l > (u64)max) l = (u64)max;
    ns_t *n = core_of(s);
    if (n) mnet_stream_seek(n->m, mnet_stream_offset(n->m) + (long long)l);    /* o buffer é consumido */
    if (got) *got = (long)l;
    return p;
}
unsigned char CFReadStreamHasBytesAvailable(id s) { return s && MSG0(BOOL, s, "hasBytesAvailable"); }
unsigned char CFWriteStreamCanAcceptBytes(id s) { return s && MSG0(BOOL, s, "hasSpaceAvailable"); }
long CFReadStreamGetStatus(id s) { return s ? (long)MSG0(unsigned long, s, "streamStatus") : MNET_SS_ERROR; }
long CFWriteStreamGetStatus(id s) { return CFReadStreamGetStatus(s); }
static cf_serr_t get_err(id s) {
    cf_serr_t e = {0, 0};
    ns_t *n = core_of(s);
    long code = 0;
    int dom = n ? mnet_stream_error(n->m, &code, 0) : 0;
    if (dom) { e.domain = dom; e.error = (int)code; }
    else if (s && !n) {
        id err = MSG0(id, s, "streamError");
        if (err) { e.domain = -1; e.error = (int)MSG0(long, err, "code"); }     /* kCFStreamErrorDomainCustom */
    }
    return e;
}
cf_serr_t CFReadStreamGetError(id s) { return get_err(s); }
cf_serr_t CFWriteStreamGetError(id s) { return get_err(s); }
id CFReadStreamCopyError(id s) { id e = s ? MSG0(id, s, "streamError") : 0; return e ? objc_retain(e) : 0; }
id CFWriteStreamCopyError(id s) { return CFReadStreamCopyError(s); }
id CFReadStreamCopyProperty(id s, id key) { id v = s ? MSG(id, s, "propertyForKey:", key) : 0; return v ? objc_retain(v) : 0; }
id CFWriteStreamCopyProperty(id s, id key) { return CFReadStreamCopyProperty(s, key); }
unsigned char CFReadStreamSetProperty(id s, id key, id v) { return s && MSG(BOOL, s, "setProperty:forKey:", v, key); }
unsigned char CFWriteStreamSetProperty(id s, id key, id v) { return CFReadStreamSetProperty(s, key, v); }
unsigned char CFReadStreamSetClient(id s, unsigned long flags, cl_cb_t cb, cl_ctx_t *ctx) {
    if (!s) return 0;
    ns_t *n = core_of(s);
    if (n) { set_client(n, flags, cb, ctx); return 1; }
    if (responds(s, "_setCFClientFlags:callback:context:")) return MSG(BOOL, s, "_setCFClientFlags:callback:context:", flags, cb, ctx);
    return 0;
}
unsigned char CFWriteStreamSetClient(id s, unsigned long flags, cl_cb_t cb, cl_ctx_t *ctx) { return CFReadStreamSetClient(s, flags, cb, ctx); }
void CFReadStreamScheduleWithRunLoop(id s, void *rl, id mode) {
    if (!s) return;
    if (core_of(s)) sched_rl(NS(s), rl, mode);
    else if (responds(s, "_scheduleInCFRunLoop:forMode:")) MSG(void, s, "_scheduleInCFRunLoop:forMode:", rl, mode);
}
void CFWriteStreamScheduleWithRunLoop(id s, void *rl, id mode) { CFReadStreamScheduleWithRunLoop(s, rl, mode); }
void CFReadStreamUnscheduleFromRunLoop(id s, void *rl, id mode) {
    if (!s) return;
    if (core_of(s)) unsched_rl(NS(s), rl, mode);
    else if (responds(s, "_unscheduleFromCFRunLoop:forMode:")) MSG(void, s, "_unscheduleFromCFRunLoop:forMode:", rl, mode);
}
void CFWriteStreamUnscheduleFromRunLoop(id s, void *rl, id mode) { CFReadStreamUnscheduleFromRunLoop(s, rl, mode); }
void CFReadStreamSetDispatchQueue(id s, void *q) { ns_t *n = core_of(s); if (n) set_queue(n, q); }
void CFWriteStreamSetDispatchQueue(id s, void *q) { CFReadStreamSetDispatchQueue(s, q); }
void *CFReadStreamCopyDispatchQueue(id s) {
    ns_t *n = core_of(s);
    void *q = n ? n->queue : 0;
    if (q) dispatch_retain(q);
    return q;
}
void *CFWriteStreamCopyDispatchQueue(id s) { return CFReadStreamCopyDispatchQueue(s); }
/* CFSocketStream.h: o subdomínio fica nos 16 bits altos do erro SOCKS */
int CFSocketStreamSOCKSGetErrorSubdomain(const cf_serr_t *e) { return e ? (e->error >> 16) & 0xffff : 0; }
int CFSocketStreamSOCKSGetError(const cf_serr_t *e) { return e ? e->error & 0xffff : 0; }

/* ---------------------------------------------------------------- registro */
void cx_netstream_init(void) {
    static int done;
    if (done) return;
    done = 1;
    cx_class_t *obj = cx_class_named("NSObject");
    { void cx_cfhttp_init(void); cx_cfhttp_init(); }                  /* CFHTTPMessage/Authentication (cfhttp.c) */
    cx_define_class(&NSStream_c, &NSStream_m, "NSStream", obj, 8);
    cx_class_t *k = &NSStream_c;
    cx_add_method(k, "dealloc", (void *)st_dealloc);
    cx_add_method(k, "open", (void *)st_open);
    cx_add_method(k, "close", (void *)st_close);
    cx_add_method(k, "delegate", (void *)st_delegate);
    cx_add_method(k, "setDelegate:", (void *)st_set_delegate);
    cx_add_method(k, "propertyForKey:", (void *)st_prop);
    cx_add_method(k, "setProperty:forKey:", (void *)st_set_prop);
    cx_add_method(k, "scheduleInRunLoop:forMode:", (void *)st_schedule);
    cx_add_method(k, "removeFromRunLoop:forMode:", (void *)st_unschedule);
    cx_add_method(k, "streamStatus", (void *)st_status);
    cx_add_method(k, "streamError", (void *)st_error);
    cx_add_method(k, "_scheduleInCFRunLoop:forMode:", (void *)st_cf_sched);
    cx_add_method(k, "_unscheduleFromCFRunLoop:forMode:", (void *)st_cf_unsched);
    cx_add_method(k, "_setCFClientFlags:callback:context:", (void *)st_cf_client);
    cx_add_cmethod(k, "getStreamsToHostWithName:port:inputStream:outputStream:", (void *)ns_get_streams);
    cx_add_cmethod(k, "getStreamsToHost:port:inputStream:outputStream:", (void *)ns_get_streams_host);
    cx_add_cmethod(k, "getBoundStreamsWithBufferSize:inputStream:outputStream:", (void *)ns_get_bound);

    cx_define_class(&NSInputStream_c, &NSInputStream_m, "NSInputStream", &NSStream_c, 0);
    k = &NSInputStream_c;
    cx_add_cmethod(k, "inputStreamWithData:", (void *)is_with_data);
    cx_add_cmethod(k, "inputStreamWithFileAtPath:", (void *)is_with_file);
    cx_add_cmethod(k, "inputStreamWithURL:", (void *)is_with_url);
    cx_add_method(k, "initWithData:", (void *)is_init_data);
    cx_add_method(k, "initWithFileAtPath:", (void *)is_init_file);
    cx_add_method(k, "initWithURL:", (void *)is_init_url);
    cx_add_method(k, "read:maxLength:", (void *)is_read);
    cx_add_method(k, "getBuffer:length:", (void *)is_get_buffer);
    cx_add_method(k, "hasBytesAvailable", (void *)is_has_bytes);

    cx_define_class(&NSOutputStream_c, &NSOutputStream_m, "NSOutputStream", &NSStream_c, 0);
    k = &NSOutputStream_c;
    cx_add_cmethod(k, "outputStreamToMemory", (void *)os_mem);
    cx_add_cmethod(k, "outputStreamToBuffer:capacity:", (void *)os_buf);
    cx_add_cmethod(k, "outputStreamToFileAtPath:append:", (void *)os_file);
    cx_add_cmethod(k, "outputStreamWithURL:append:", (void *)os_url);
    cx_add_method(k, "initToMemory", (void *)os_init_mem);
    cx_add_method(k, "initToBuffer:capacity:", (void *)os_init_buf);
    cx_add_method(k, "initToFileAtPath:append:", (void *)os_init_file);
    cx_add_method(k, "initWithURL:append:", (void *)os_init_url);
    cx_add_method(k, "write:maxLength:", (void *)os_write);
    cx_add_method(k, "hasSpaceAvailable", (void *)os_has_space);
}
