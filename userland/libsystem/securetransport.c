/*
 * mOS — libSystem: SecureTransport (SSLCreateContext, SSLHandshake, SSLRead, SSLWrite...) e as raízes do sistema
 * (SecTrustSettingsCopyCertificates).
 *
 * Por quê: o Codex CLI (docs/compat-apps.md) fala HTTPS pelo native-tls, que no macOS é o SecureTransport do crate
 * security-framework; o rustls que ele também traz pega as raízes com rustls-native-certs, que lê
 * SecTrustSettingsCopyCertificates(kSecTrustSettingsDomainSystem). Os dois eram stubs.
 *
 * Como funciona: o app é dono do socket e passa callbacks de E/S (SSLSetIOFuncs). Este arquivo lê um registro TLS
 * por vez pela callback de leitura (5 bytes de cabeçalho, depois exatamente o corpo, como o SecureTransport faz:
 * o read_func do security-framework repete a leitura até encher o pedido, então pedir mais que um registro num
 * socket bloqueante travaria) e entrega ao kernel (SYS_NET, MOS_NET_TLS_*, kernel/net/usersock.c). O kernel roda
 * o motor do navegador (kernel/net/tls.c: TLS 1.3/1.2, X25519/P-256/P-384, AES-GCM/ChaCha20, cadeia até as raízes
 * da Mozilla, nome, datas, OCSP grampeado) e devolve o que precisa ir para a rede, que sai pela callback de escrita.
 * Um único motor TLS no sistema, com a mesma lista de confiança do navegador.
 *
 * Semântica que os clientes esperam (security-framework/src/secure_transport.rs):
 *  - errSSLWouldBlock quando a callback diz que bloquearia; a chamada pode ser repetida (o estado fica aqui).
 *  - kSSLSessionOptionBreakOnServerAuth: o handshake para uma vez com errSSLPeerAuthCompleted para o app avaliar
 *    a confiança (SSLCopyPeerTrust + SecTrustEvaluate). O kernel já validou a cadeia e o nome, então a confiança
 *    devolvida é marcada como válida (cx_trust_mark_verified, security.c).
 *  - SSLRead devolve o que tiver assim que tiver algo; fim do fluxo = errSSLClosedGraceful/NoNotify.
 * Limites: só cliente (lado servidor: SSLCreateContext = NULL), sem certificado de cliente, sem ALPN negociado
 * (SSLCopyALPNProtocols devolve vazio: o app cai para HTTP/1.1). SSLSetSessionTicketsEnabled guarda o
 * pedido; o motor (kernel/net/tls.c) não envia nem retoma ticket, então desligar coincide com o fio e
 * ligar não cria retomada.
 */
#include "foundation.h"
#include "libc.h"

typedef int OSStatus;
typedef unsigned char Boolean;
typedef OSStatus (*ssl_read_fn)(const void *conn, void *data, size_t *len);
typedef OSStatus (*ssl_write_fn)(const void *conn, const void *data, size_t *len);

enum {
    errSecSuccess = 0, errSecUnimplemented = -4, errSecParam = -50, errSecAllocate = -108, errSecIO = -36,
    errSecItemNotFound = -25300, errSecNoTrustSettings = -25263,
    errSSLProtocol = -9800, errSSLNegotiation = -9801, errSSLFatalAlert = -9802, errSSLWouldBlock = -9803,
    errSSLClosedGraceful = -9805, errSSLClosedAbort = -9806, errSSLXCertChainInvalid = -9807, errSSLInternal = -9810,
    errSSLClosedNoNotify = -9816, errSSLPeerAuthCompleted = -9841, errSSLBadConfiguration = -9848,
};
enum { kSSLIdle, kSSLHandshake, kSSLConnected, kSSLClosed, kSSLAborted };
enum { kSSLSessionOptionBreakOnServerAuth = 0 };
enum { kTLSProtocol12 = 8, kTLSProtocol13 = 10 };

id CFArrayCreateMutable(void *alloc, long cap, const void *cb);
void CFArrayAppendValue(id a, id v);
long CFArrayGetCount(id a);
id CFArrayGetValueAtIndex(id a, long i);
id CFRetain(id o);
void CFRelease(id o);
id CFDataCreate(void *alloc, const u8 *bytes, long n);
id SecCertificateCreateWithData(void *alloc, id der);
id objc_retain(id o);
cx_class_t *objc_allocateClassPair(cx_class_t *super, const char *name, u64 extra);
void objc_registerClassPair(cx_class_t *c);
BOOL class_addIvar(cx_class_t *c, const char *name, u64 size, u8 align, const char *types);
BOOL class_addMethod(cx_class_t *c, SEL s, IMP imp, const char *types);
IMP class_getMethodImplementation(cx_class_t *c, SEL s);
id class_createInstance(cx_class_t *c, u64 extra);
void cx_trust_mark_verified(id trust);                 /* security.c */

char *strstr(const char *h, const char *n);
static long net(long op, long a, long b, long c) { return SYS4(SYS_NET, op, a, b, c); }

#define REC_MAX (5 + 16384 + 2048)       /* maior registro aceito (TLSCiphertext: 2^14 + 2048) */
#define OUT_CAP (128 * 1024)
#define PLAIN_CAP (16384 + 64)
#define SSL_MAGIC 0x53534C43u            /* 'SSLC' */

typedef struct {
    cx_class_t *isa;
    u32 magic;
    int state;
    long h;                              /* sessão no kernel, -1 = nenhuma */
    ssl_read_fn rd;
    ssl_write_fn wr;
    const void *conn;
    char peer[256];
    Boolean break_auth, auth_done, saw_eof, tickets;
    OSStatus err;                        /* erro fatal guardado (as chamadas seguintes o repetem) */
    int vmin, vmax;
    u8 *rec, *out, *plain;
    u32 rec_have, out_off, out_len, p_off, p_len;
} ssl_t;

/* ---------------------------------------------------------------- o objeto (CFType: o app faz CFRelease) */
static cx_class_t *ssl_cls;
static IMP nsobject_dealloc;

static void ssl_teardown(ssl_t *s, Boolean send_close) {
    if (s->h >= 0) {
        long k = net(MOS_NET_TLS_FREE, s->h, (long)s->out, OUT_CAP);
        s->h = -1;
        if (send_close && k > 0 && s->wr) {      /* close_notify: melhor esforço */
            size_t n = (size_t)k;
            s->wr(s->conn, s->out, &n);
        }
    }
}
static void ssl_dealloc(id self, SEL c) {
    ssl_t *s = (ssl_t *)self;
    ssl_teardown(s, 0);
    free(s->rec); free(s->out); free(s->plain);
    s->rec = s->out = s->plain = 0;
    s->magic = 0;
    if (nsobject_dealloc) ((void (*)(id, SEL))nsobject_dealloc)(self, c);
}
static cx_class_t *ssl_class(void) {
    if (ssl_cls) return ssl_cls;
    cx_class_t *ns = cx_class_named("NSObject");
    nsobject_dealloc = ns ? class_getMethodImplementation(ns, sel_registerName("dealloc")) : 0;
    cx_class_t *k = objc_allocateClassPair(ns, "SSLContext", 0);
    if (!k) k = cx_class_named("SSLContext");
    class_addIvar(k, "_ssl", sizeof(ssl_t) - 8, 3, "?");
    class_addMethod(k, sel_registerName("dealloc"), (IMP)ssl_dealloc, "v@:");
    objc_registerClassPair(k);
    ssl_cls = k;
    return k;
}
static ssl_t *S(void *ctx) {
    ssl_t *s = ctx;
    return s && s->magic == SSL_MAGIC ? s : 0;
}

u64 SSLContextGetTypeID(void) { return 0x5EC10; }

void *SSLCreateContext(void *alloc, int side, int type) {
    (void)alloc;
    if (side != 1 || type != 0) return 0;          /* só cliente (kSSLClientSide) e fluxo (kSSLStreamType) */
    ssl_t *s = (ssl_t *)class_createInstance(ssl_class(), 0);
    if (!s) return 0;
    memset((u8 *)s + 8, 0, sizeof *s - 8);
    s->magic = SSL_MAGIC;
    s->h = -1;
    s->vmin = kTLSProtocol12;
    s->vmax = kTLSProtocol13;
    s->tickets = 1;
    s->rec = malloc(REC_MAX);
    s->out = malloc(OUT_CAP);
    s->plain = malloc(PLAIN_CAP);
    if (!s->rec || !s->out || !s->plain) { CFRelease((id)s); return 0; }
    return s;
}
/* API antiga (SSLNewContext/SSLDisposeContext) */
OSStatus SSLNewContext(Boolean server, void **ctx) {
    if (!ctx) return errSecParam;
    *ctx = server ? 0 : SSLCreateContext(0, 1, 0);
    return *ctx ? errSecSuccess : errSecUnimplemented;
}
OSStatus SSLDisposeContext(void *ctx) { if (S(ctx)) CFRelease((id)ctx); return errSecSuccess; }

/* ---------------------------------------------------------------- configuração */
OSStatus SSLSetIOFuncs(void *ctx, void *r, void *w) {
    ssl_t *s = S(ctx);
    if (!s) return errSecParam;
    s->rd = (ssl_read_fn)r;
    s->wr = (ssl_write_fn)w;
    return errSecSuccess;
}
OSStatus SSLSetConnection(void *ctx, const void *conn) {
    ssl_t *s = S(ctx);
    if (!s) return errSecParam;
    s->conn = conn;
    return errSecSuccess;
}
OSStatus SSLGetConnection(void *ctx, const void **conn) {
    ssl_t *s = S(ctx);
    if (!s || !conn) return errSecParam;
    *conn = s->conn;
    return errSecSuccess;
}
OSStatus SSLSetPeerDomainName(void *ctx, const char *name, size_t len) {
    ssl_t *s = S(ctx);
    if (!s) return errSecParam;
    if (s->state != kSSLIdle) return errSSLBadConfiguration;
    if (!name) len = 0;
    if (len && name[len - 1] == 0) len--;
    if (len >= sizeof s->peer) return errSecParam;
    memcpy(s->peer, name, len);
    s->peer[len] = 0;
    return errSecSuccess;
}
OSStatus SSLGetPeerDomainNameLength(void *ctx, size_t *len) {
    ssl_t *s = S(ctx);
    if (!s || !len) return errSecParam;
    *len = strlen(s->peer);
    return errSecSuccess;
}
OSStatus SSLGetPeerDomainName(void *ctx, char *buf, size_t *len) {
    ssl_t *s = S(ctx);
    if (!s || !buf || !len) return errSecParam;
    size_t n = strlen(s->peer);
    if (*len < n) return errSecParam;
    memcpy(buf, s->peer, n);
    *len = n;
    return errSecSuccess;
}
OSStatus SSLSetPeerID(void *ctx, const void *id_, size_t len) { (void)id_; (void)len; return S(ctx) ? errSecSuccess : errSecParam; }
OSStatus SSLSetCertificate(void *ctx, id certs) {
    if (!S(ctx)) return errSecParam;
    return certs ? errSecUnimplemented : errSecSuccess;   /* certificado de cliente: não suportado */
}
OSStatus SSLSetProtocolVersionMin(void *ctx, int v) {
    ssl_t *s = S(ctx);
    if (!s) return errSecParam;
    if (v > kTLSProtocol13 && v != 999) return errSSLBadConfiguration;
    s->vmin = v;
    return errSecSuccess;
}
OSStatus SSLSetProtocolVersionMax(void *ctx, int v) {
    ssl_t *s = S(ctx);
    if (!s) return errSecParam;
    if (v != 999 && v < kTLSProtocol12) return errSSLBadConfiguration;   /* o motor só fala 1.2 e 1.3 */
    s->vmax = v == 999 ? kTLSProtocol13 : v;
    return errSecSuccess;
}
OSStatus SSLGetProtocolVersionMin(void *ctx, int *v) { ssl_t *s = S(ctx); if (!s || !v) return errSecParam; *v = s->vmin; return errSecSuccess; }
OSStatus SSLGetProtocolVersionMax(void *ctx, int *v) { ssl_t *s = S(ctx); if (!s || !v) return errSecParam; *v = s->vmax; return errSecSuccess; }
OSStatus SSLGetNegotiatedProtocolVersion(void *ctx, int *v) {
    ssl_t *s = S(ctx);
    if (!s || !v) return errSecParam;
    *v = s->state == kSSLConnected ? kTLSProtocol12 : 0;  /* o kernel não informa qual; 1.2 é o mínimo garantido */
    return errSecSuccess;
}
OSStatus SSLSetSessionOption(void *ctx, int opt, Boolean v) {
    ssl_t *s = S(ctx);
    if (!s) return errSecParam;
    if (opt == kSSLSessionOptionBreakOnServerAuth) s->break_auth = v;
    return errSecSuccess;                          /* os outros (false start, tickets...) não mudam nada aqui */
}
OSStatus SSLGetSessionOption(void *ctx, int opt, Boolean *v) {
    ssl_t *s = S(ctx);
    if (!s || !v) return errSecParam;
    *v = opt == kSSLSessionOptionBreakOnServerAuth ? s->break_auth : 0;
    return errSecSuccess;
}
/* O kernel ignora NewSessionTicket. O flag fica no contexto; não muda o handshake. */
OSStatus SSLSetSessionTicketsEnabled(void *ctx, Boolean enabled) {
    ssl_t *s = S(ctx);
    if (!s) return errSecParam;
    s->tickets = enabled ? 1 : 0;
    return errSecSuccess;
}

/* as suítes do motor (kernel/net/tls.c) */
static const u32 SUITES[] = { 0x1301, 0x1302, 0x1303, 0xC02B, 0xC02C, 0xC02F, 0xC030, 0xCCA9, 0xCCA8 };
#define NSUITES (sizeof SUITES / sizeof SUITES[0])
OSStatus SSLGetNumberSupportedCiphers(void *ctx, size_t *n) { if (!S(ctx) || !n) return errSecParam; *n = NSUITES; return errSecSuccess; }
OSStatus SSLGetSupportedCiphers(void *ctx, u32 *c, size_t *n) {
    if (!S(ctx) || !c || !n) return errSecParam;
    size_t k = *n < NSUITES ? *n : NSUITES;
    for (size_t i = 0; i < k; i++) c[i] = SUITES[i];
    *n = k;
    return errSecSuccess;
}
OSStatus SSLGetNumberEnabledCiphers(void *ctx, size_t *n) { return SSLGetNumberSupportedCiphers(ctx, n); }
OSStatus SSLGetEnabledCiphers(void *ctx, u32 *c, size_t *n) { return SSLGetSupportedCiphers(ctx, c, n); }
OSStatus SSLSetEnabledCiphers(void *ctx, const u32 *c, size_t n) {
    if (!S(ctx)) return errSecParam;
    for (size_t i = 0; c && i < n; i++)                 /* basta uma suíte que o motor conheça */
        for (size_t j = 0; j < NSUITES; j++) if (c[i] == SUITES[j]) return errSecSuccess;
    return errSSLBadConfiguration;
}
OSStatus SSLGetNegotiatedCipher(void *ctx, u32 *c) {
    ssl_t *s = S(ctx);
    if (!s || !c) return errSecParam;
    *c = s->state == kSSLConnected ? 0x1301 : 0;
    return errSecSuccess;
}
OSStatus SSLSetALPNProtocols(void *ctx, id protos) { (void)protos; return S(ctx) ? errSecSuccess : errSecParam; }
OSStatus SSLCopyALPNProtocols(void *ctx, id *protos) {
    if (!S(ctx) || !protos) return errSecParam;
    *protos = CFArrayCreateMutable(0, 0, 0);      /* nenhum negociado */
    return errSecSuccess;
}
OSStatus SSLGetSessionState(void *ctx, int *st) {
    ssl_t *s = S(ctx);
    if (!s || !st) return errSecParam;
    *st = s->state;
    return errSecSuccess;
}
OSStatus SSLGetBufferedReadSize(void *ctx, size_t *n) {
    ssl_t *s = S(ctx);
    if (!s || !n) return errSecParam;
    *n = s->p_len - s->p_off;
    return errSecSuccess;
}
OSStatus SSLCopyPeerTrust(void *ctx, id *trust) {
    ssl_t *s = S(ctx);
    if (!s || !trust) return errSecParam;
    *trust = 0;
    if (s->state != kSSLConnected) return errSecSuccess;
    id t = CFArrayCreateMutable(0, 0, 0);         /* SecTrust do mOS = CFArray dos certificados (security.c) */
    cx_trust_mark_verified(t);                     /* o kernel validou cadeia, datas e nome */
    *trust = t;
    return errSecSuccess;
}
OSStatus SSLCopyPeerCertificates(void *ctx, id *certs) {
    if (!S(ctx) || !certs) return errSecParam;
    *certs = CFArrayCreateMutable(0, 0, 0);
    return errSecSuccess;
}

/* ---------------------------------------------------------------- E/S */
static Boolean is_closed(OSStatus st) {
    return st == errSSLClosedGraceful || st == errSSLClosedAbort || st == errSSLClosedNoNotify;
}

/* manda o que o motor quer pôr na rede; errSSLWouldBlock deixa o resto guardado para a próxima chamada */
static OSStatus flush(ssl_t *s) {
    for (;;) {
        if (s->out_off == s->out_len) {
            s->out_off = s->out_len = 0;
            if (s->h < 0) return errSecSuccess;
            long k = net(MOS_NET_TLS_DRAIN, s->h, (long)s->out, OUT_CAP);
            if (k <= 0) return errSecSuccess;
            s->out_len = (u32)k;
        }
        size_t n = s->out_len - s->out_off;
        OSStatus st = s->wr(s->conn, s->out + s->out_off, &n);
        if (n > s->out_len - s->out_off) n = s->out_len - s->out_off;
        s->out_off += (u32)n;
        if (st) return st;
        if (!n) return errSSLWouldBlock;
    }
}

/* lê exatamente um registro pela callback e o entrega ao kernel */
static OSStatus read_record(ssl_t *s) {
    for (;;) {
        u32 need = 5;
        if (s->rec_have >= 5) {
            need = 5 + ((u32)s->rec[3] << 8 | s->rec[4]);
            if (need > REC_MAX) return errSSLProtocol;
        }
        if (s->rec_have >= need) break;
        size_t n = need - s->rec_have;
        OSStatus st = s->rd(s->conn, s->rec + s->rec_have, &n);
        if (n > need - s->rec_have) n = need - s->rec_have;
        s->rec_have += (u32)n;
        if (st) {
            if (is_closed(st)) {
                s->saw_eof = 1;
                net(MOS_NET_TLS_FEED, s->h, 0, 0);
            }
            return st;
        }
        if (!n) return errSSLWouldBlock;
    }
    u32 off = 0;
    while (off < s->rec_have) {
        long k = net(MOS_NET_TLS_FEED, s->h, (long)(s->rec + off), s->rec_have - off);
        if (k <= 0) return errSSLInternal;
        off += (u32)k;
    }
    s->rec_have = 0;
    return errSecSuccess;
}

static OSStatus fatal(ssl_t *s) {
    char msg[200];
    long k = s->h >= 0 ? net(MOS_NET_TLS_ERROR, s->h, (long)msg, sizeof msg - 1) : 0;
    msg[k > 0 ? k : 0] = 0;
    OSStatus st = errSSLProtocol;
    if (strstr(msg, "certificad") || strstr(msg, "cadeia") || strstr(msg, "raiz") || strstr(msg, "nome") ||
        strstr(msg, "expirad") || strstr(msg, "revogad"))
        st = errSSLXCertChainInvalid;
    else if (strstr(msg, "alerta")) st = errSSLFatalAlert;
    else if (strstr(msg, "encerrou") || strstr(msg, "encerrada")) st = errSSLClosedAbort;
    if (getenv("MOS_TLS_DEBUG")) {
        char line[512];
        int n = snprintf(line, sizeof line, "[SecureTransport] %s: %s (%d)\n", s->peer, msg, st);
        if (n > 0) write(2, line, (size_t)n);
    }
    s->state = kSSLAborted;
    s->err = st;
    return st;
}

OSStatus SSLHandshake(void *ctx) {
    ssl_t *s = S(ctx);
    if (!s) return errSecParam;
    if (s->err) return s->err;
    if (!s->rd || !s->wr) return errSSLBadConfiguration;
    if (s->state == kSSLClosed) return errSSLClosedGraceful;
    if (s->h < 0) {
        if (s->state != kSSLIdle) return errSSLClosedAbort;
        long h = net(MOS_NET_TLS_NEW, s->peer[0] ? (long)s->peer : 0, 0, 0);
        if (h < 0) { s->err = errSSLInternal; return s->err; }
        s->h = h;
        s->state = kSSLHandshake;
    }
    for (;;) {
        long r = net(MOS_NET_TLS_HANDSHAKE, s->h, 0, 0);
        OSStatus st = flush(s);
        if (r < 0) return fatal(s);
        if (st) {
            if (st != errSSLWouldBlock) { s->state = kSSLAborted; s->err = st; }
            return st;
        }
        if (r == 1) {
            s->state = kSSLConnected;
            if (s->break_auth && !s->auth_done) { s->auth_done = 1; return errSSLPeerAuthCompleted; }
            return errSecSuccess;
        }
        st = read_record(s);
        if (st) {
            if (is_closed(st)) {
                net(MOS_NET_TLS_HANDSHAKE, s->h, 0, 0);       /* deixa o motor registrar o motivo */
                fatal(s);
                s->err = errSSLClosedAbort;
                return s->err;
            }
            return st;
        }
    }
}

static OSStatus ensure_connected(ssl_t *s) {
    if (s->err) return s->err;
    if (s->state == kSSLConnected) return errSecSuccess;
    if (s->state == kSSLClosed) return errSSLClosedGraceful;
    OSStatus st = SSLHandshake(s);
    return st == errSSLPeerAuthCompleted ? SSLHandshake(s) : st;
}

OSStatus SSLRead(void *ctx, void *buf, size_t len, size_t *done) {
    ssl_t *s = S(ctx);
    if (done) *done = 0;
    if (!s || !done || (!buf && len)) return errSecParam;
    if (s->p_len == s->p_off) {
        OSStatus st = ensure_connected(s);
        if (st) return st;
    }
    if (!len) return errSecSuccess;
    if (s->h >= 0) {
        OSStatus st = flush(s);                      /* algo pendente de uma escrita anterior */
        if (st && st != errSSLWouldBlock) return st;
    }
    for (;;) {
        if (s->p_len > s->p_off) {
            size_t k = s->p_len - s->p_off;
            if (k > len) k = len;
            memcpy(buf, s->plain + s->p_off, k);
            s->p_off += (u32)k;
            if (s->p_off == s->p_len) s->p_off = s->p_len = 0;
            *done = k;
            return errSecSuccess;
        }
        if (s->state == kSSLClosed || s->h < 0) return s->saw_eof ? errSSLClosedNoNotify : errSSLClosedGraceful;
        long k = net(MOS_NET_TLS_RECV, s->h, (long)s->plain, PLAIN_CAP);
        OSStatus fst = flush(s);                     /* KeyUpdate/respostas que o motor enfileirou */
        if (fst && fst != errSSLWouldBlock) return fst;
        if (k > 0) { s->p_off = 0; s->p_len = (u32)k; continue; }
        if (k < 0) {
            char msg[64];
            long m = net(MOS_NET_TLS_ERROR, s->h, (long)msg, sizeof msg - 1);
            msg[m > 0 ? m : 0] = 0;
            if (!strcmp(msg, "sem erro")) {          /* fim limpo (close_notify ou fim do TCP) */
                s->state = kSSLClosed;
                return s->saw_eof ? errSSLClosedNoNotify : errSSLClosedGraceful;
            }
            return fatal(s);
        }
        OSStatus st = read_record(s);
        if (st && !is_closed(st)) return st;          /* fechado: o motor responde -1 na volta */
    }
}

OSStatus SSLWrite(void *ctx, const void *data, size_t len, size_t *done) {
    ssl_t *s = S(ctx);
    if (done) *done = 0;
    if (!s || !done || (!data && len)) return errSecParam;
    OSStatus st = ensure_connected(s);
    if (st) return st;
    st = flush(s);                                   /* primeiro o que ficou da vez anterior */
    if (st) return st;
    size_t sent = 0;
    while (sent < len) {
        size_t n = len - sent;
        if (n > 64 * 1024) n = 64 * 1024;
        long k = net(MOS_NET_TLS_SEND, s->h, (long)((const u8 *)data + sent), (long)n);
        if (k < 0) { *done = sent; return fatal(s); }
        sent += (size_t)k;
        st = flush(s);
        if (st == errSSLWouldBlock) break;           /* o resto cifrado sai na próxima chamada */
        if (st) { *done = sent; return st; }
        if (!k) break;
    }
    *done = sent;
    return sent ? errSecSuccess : errSSLWouldBlock;
}

OSStatus SSLClose(void *ctx) {
    ssl_t *s = S(ctx);
    if (!s) return errSecParam;
    if (s->h >= 0) flush(s);
    ssl_teardown(s, s->state == kSSLConnected);
    s->state = kSSLClosed;
    return errSecSuccess;
}

/* ---------------------------------------------------------------- raízes do sistema
 * O mOS confia na lista da Mozilla embutida no kernel (assets/certs/cacert.pem). Ela é o domínio "sistema"; os
 * domínios do usuário e do administrador não têm ajustes, como num Mac recém-instalado. */
static id g_roots;                       /* CFArray de SecCertificate, montado uma vez */

static int b64v(int c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}
static id system_roots(void) {
    if (g_roots) return g_roots;
    long len = net(MOS_NET_TLS_ROOTS, 0, 0, 0);
    if (len <= 0) return 0;
    char *pem = malloc((size_t)len + 1);
    u8 *der = malloc((size_t)len);
    if (!pem || !der) { free(pem); free(der); return 0; }
    if (net(MOS_NET_TLS_ROOTS, (long)pem, len, 0) != len) { free(pem); free(der); return 0; }
    pem[len] = 0;
    static const char BEGIN[] = "-----BEGIN CERTIFICATE-----", END[] = "-----END CERTIFICATE-----";
    id arr = CFArrayCreateMutable(0, 0, 0);
    char *p = pem;
    while ((p = strstr(p, BEGIN))) {
        p += sizeof BEGIN - 1;
        char *e = strstr(p, END);
        if (!e) break;
        u32 n = 0, acc = 0, bits = 0;
        for (; p < e; p++) {
            int v = b64v((u8)*p);
            if (v < 0) continue;
            acc = acc << 6 | (u32)v;
            bits += 6;
            if (bits >= 8) { bits -= 8; der[n++] = (u8)(acc >> bits); }
        }
        p = e + sizeof END - 1;
        id data = CFDataCreate(0, der, n);
        id cert = data ? SecCertificateCreateWithData(0, data) : 0;
        if (cert) { CFArrayAppendValue(arr, cert); CFRelease(cert); }
        if (data) CFRelease(data);
    }
    free(pem);
    free(der);
    g_roots = arr;
    return arr;
}

enum { kSecTrustSettingsDomainUser, kSecTrustSettingsDomainAdmin, kSecTrustSettingsDomainSystem };
OSStatus SecTrustSettingsCopyCertificates(int domain, id *certs) {
    if (certs) *certs = 0;
    if (domain != kSecTrustSettingsDomainSystem) return errSecNoTrustSettings;
    id roots = system_roots();
    if (!roots || !CFArrayGetCount(roots)) return errSecNoTrustSettings;
    if (certs) {
        id copy = CFArrayCreateMutable(0, 0, 0);
        for (long i = 0; i < CFArrayGetCount(roots); i++) CFArrayAppendValue(copy, CFArrayGetValueAtIndex(roots, i));
        *certs = copy;
    }
    return errSecSuccess;
}
/* raiz do sistema sem ajuste específico = array vazio ("confiar como raiz"), o que o rustls-native-certs espera */
OSStatus SecTrustSettingsCopyTrustSettings(id cert, int domain, id *settings) {
    if (settings) *settings = 0;
    if (!cert || domain != kSecTrustSettingsDomainSystem) return errSecNoTrustSettings;
    if (settings) *settings = CFArrayCreateMutable(0, 0, 0);
    return errSecSuccess;
}
