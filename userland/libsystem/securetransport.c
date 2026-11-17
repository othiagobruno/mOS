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
 * (ALPN: SSLSetALPNProtocols vai para o motor e SSLCopyALPNProtocols devolve o escolhido, MOS_NET_TLS_ALPN/INFO;
 * versão e cifra negociadas também vêm de MOS_NET_TLS_INFO). SSLSetSessionTicketsEnabled guarda o
 * pedido; o motor (kernel/net/tls.c) não envia nem retoma ticket, então desligar coincide com o fio e
 * ligar não cria retomada.
 */
#include "foundation.h"
#include "libc.h"
extern const char kCFTypeArrayCallBacks[];   /* cfcollections.c: callbacks de objeto CF (retain/isEqual:) */

typedef int OSStatus;
typedef unsigned char Boolean;
typedef OSStatus (*ssl_read_fn)(const void *conn, void *data, size_t *len);
typedef OSStatus (*ssl_write_fn)(const void *conn, const void *data, size_t *len);

enum {
    errSecSuccess = 0, errSecUnimplemented = -4, errSecParam = -50, errSecAllocate = -108, errSecIO = -36,
    errSecItemNotFound = -25300, errSecBadReq = -909, errSecNoTrustSettings = -25263,
    errSSLProtocol = -9800, errSSLNegotiation = -9801, errSSLFatalAlert = -9802, errSSLWouldBlock = -9803,
    errSSLClosedGraceful = -9805, errSSLClosedAbort = -9806, errSSLXCertChainInvalid = -9807, errSSLInternal = -9810,
    errSSLClosedNoNotify = -9816, errSSLPeerAuthCompleted = -9841, errSSLBadConfiguration = -9848, errSSLBadCert = -9808,
    errSSLUnknownRootCert = -9812, errSSLNoRootCert = -9813, errSSLCertExpired = -9814, errSSLCertNotYetValid = -9815,
};
enum { kSSLIdle, kSSLHandshake, kSSLConnected, kSSLClosed, kSSLAborted };
enum { kSSLSessionOptionBreakOnServerAuth = 0 };
enum { kSSLProtocol2 = 1, kSSLProtocol3 = 2, kTLSProtocol1 = 4, kSSLProtocolAll = 6, kTLSProtocol11 = 7, kTLSProtocol12 = 8,
       kTLSProtocol13 = 10 };
/* validação da cadeia pedida pelo app (API antiga): sem nenhuma destas, o kernel valida como sempre */
enum { V_OFF = 1, V_ANY_ROOT = 2, V_EXP_CERTS = 4, V_EXP_ROOTS = 8 };

id CFArrayCreateMutable(void *alloc, long cap, const void *cb);
void CFArrayAppendValue(id a, id v);
long CFArrayGetCount(id a);
id CFArrayGetValueAtIndex(id a, long i);
id CFRetain(id o);
void CFRelease(id o);
long CFDataGetLength(id d);
const u8 *CFDataGetBytePtr(id d);
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
void cx_trust_set_host(id trust, const char *host);

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
    u8 alpn[128]; u32 alpn_len;          /* lista pedida pelo app, formato do fio */
    u8 *ident; u32 ident_len;            /* SSLSetCertificate: cadeia + chave para MOS_NET_TLS_IDENTITY */
    u8 vflags;                           /* V_*: SSLSetEnableCertVerify, SSLSetAllowsAnyRoot, SSLSetAllowsExpired* */
    Boolean roots_only, chain_ok;        /* SSLSetTrustedRoots(replace); cadeia válida pela política padrão */
    id roots;                            /* SSLSetTrustedRoots: CFArray de SecCertificate (âncoras do app) */
    u8 *dh; u32 dh_len;                  /* SSLSetDiffieHellmanParams (só servidor usa; guardado como na Apple) */
    u8 *peerid; u32 peerid_len;          /* SSLSetPeerID */
    u16 pmask;                           /* SSLSetProtocolVersionEnabled: bit = número do protocolo */
    mos_net_tls_info_t info;             /* preenchido no fim do handshake */
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
    if (s->ident) { memset(s->ident, 0, s->ident_len); free(s->ident); }
    if (s->roots) CFRelease(s->roots);
    free(s->dh); free(s->peerid);
    s->roots = 0; s->dh = s->peerid = 0;
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
    s->pmask = 1u << kTLSProtocol1 | 1u << kTLSProtocol11 | 1u << kTLSProtocol12 | 1u << kTLSProtocol13;
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
id SecCertificateCopyData(id cert);
double SecCertificateNotValidBefore(id cert);
double SecCertificateNotValidAfter(id cert);
/* PeerID: chave de cache de sessão do app. O kernel não retoma sessões, então ele só fica guardado (SSLGetPeerID) */
static OSStatus set_blob(u8 **dst, u32 *dl, const void *src, size_t n) {
    if (n > 64 * 1024 || (n && !src)) return errSecParam;
    u8 *b = n ? malloc(n) : 0;
    if (n && !b) return errSecAllocate;
    if (n) memcpy(b, src, n);
    free(*dst);
    *dst = b; *dl = (u32)n;
    return errSecSuccess;
}
OSStatus SSLSetPeerID(void *ctx, const void *id_, size_t len) { ssl_t *s = S(ctx); return s ? set_blob(&s->peerid, &s->peerid_len, id_, len) : errSecParam; }
OSStatus SSLGetPeerID(void *ctx, const void **id_, size_t *len) {
    ssl_t *s = S(ctx);
    if (!s || !id_ || !len) return errSecParam;
    *id_ = s->peerid; *len = s->peerid_len;
    return errSecSuccess;
}
/* parâmetros DH (PKCS#3): só o lado servidor usa; o mOS só tem cliente, e o cliente da Apple também só os guarda */
OSStatus SSLSetDiffieHellmanParams(void *ctx, const void *dh, size_t len) {
    ssl_t *s = S(ctx);
    if (!s) return errSecParam;
    if (s->h >= 0) return errSecBadReq;
    return set_blob(&s->dh, &s->dh_len, dh, len);
}
OSStatus SSLGetDiffieHellmanParams(void *ctx, const void **dh, size_t *len) {
    ssl_t *s = S(ctx);
    if (!s || !dh || !len) return errSecParam;
    *dh = s->dh; *len = s->dh_len;
    return errSecSuccess;
}
/* opções antigas de validação: valem para o próximo handshake */
static OSStatus vflag(void *ctx, u8 bit, Boolean on) {
    ssl_t *s = S(ctx);
    if (!s) return errSecParam;
    if (s->h >= 0) return errSecBadReq;
    s->vflags = on ? (u8)(s->vflags | bit) : (u8)(s->vflags & ~bit);
    return errSecSuccess;
}
static OSStatus vget(void *ctx, u8 bit, Boolean *on, int invert) {
    ssl_t *s = S(ctx);
    if (!s || !on) return errSecParam;
    *on = (Boolean)(((s->vflags & bit) != 0) ^ invert);
    return errSecSuccess;
}
OSStatus SSLSetEnableCertVerify(void *ctx, Boolean on) { return vflag(ctx, V_OFF, !on); }
OSStatus SSLGetEnableCertVerify(void *ctx, Boolean *on) { return vget(ctx, V_OFF, on, 1); }
OSStatus SSLSetAllowsAnyRoot(void *ctx, Boolean on) { return vflag(ctx, V_ANY_ROOT, on); }
OSStatus SSLGetAllowsAnyRoot(void *ctx, Boolean *on) { return vget(ctx, V_ANY_ROOT, on, 0); }
OSStatus SSLSetAllowsExpiredCerts(void *ctx, Boolean on) { return vflag(ctx, V_EXP_CERTS, on); }
OSStatus SSLGetAllowsExpiredCerts(void *ctx, Boolean *on) { return vget(ctx, V_EXP_CERTS, on, 0); }
OSStatus SSLSetAllowsExpiredRoots(void *ctx, Boolean on) { return vflag(ctx, V_EXP_ROOTS, on); }
OSStatus SSLGetAllowsExpiredRoots(void *ctx, Boolean *on) { return vget(ctx, V_EXP_ROOTS, on, 0); }
/* âncoras do app: replace = só elas; senão, elas mais as raízes do sistema */
OSStatus SSLSetTrustedRoots(void *ctx, id roots, Boolean replace) {
    ssl_t *s = S(ctx);
    if (!s) return errSecParam;
    if (s->h >= 0) return errSecBadReq;
    id a = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
    if (!a) return errSecAllocate;
    long n = roots ? CFArrayGetCount(roots) : 0;
    for (long i = 0; i < n; i++) {
        id c = (id)CFArrayGetValueAtIndex(roots, i);
        id d = c ? SecCertificateCopyData(c) : 0;
        if (!d) { CFRelease(a); return errSecParam; }
        CFRelease(d);
        CFArrayAppendValue(a, c);
    }
    if (s->roots) CFRelease(s->roots);
    s->roots = a;
    s->roots_only = replace;
    return errSecSuccess;
}
OSStatus SSLCopyTrustedRoots(void *ctx, id *roots) {
    ssl_t *s = S(ctx);
    if (!s || !roots) return errSecParam;
    *roots = s->roots ? CFRetain(s->roots) : 0;
    return errSecSuccess;
}
/* autoridades aceitas para o certificado de cliente: é lista do lado servidor (SSLSetCertificateAuthorities); no
 * cliente não há nenhuma, e a Apple devolve NULL */
OSStatus SSLCopyCertificateAuthorities(void *ctx, id *names) {
    if (!S(ctx) || !names) return errSecParam;
    *names = 0;
    return errSecSuccess;
}
/* erro que as próximas chamadas devolvem (usado por quem embrulha o SecureTransport para abortar a sessão) */
OSStatus SSLSetError(void *ctx, OSStatus err) {
    ssl_t *s = S(ctx);
    if (!s) return errSecParam;
    s->err = err;
    if (err) s->state = kSSLAborted;
    return errSecSuccess;
}
/* certificado de cliente: certs[0] = SecIdentityRef, depois os intermediários (SecCertificateRef). A cadeia vai no
 * formato de MOS_NET_TLS_CERTS e a chave como SecKeyCopyExternalRepresentation (PKCS#1 no RSA, X9.63 no EC). */
OSStatus SecIdentityCopyCertificate(id ident, id *cert);
OSStatus SecIdentityCopyPrivateKey(id ident, id *key);
id SecKeyCopyExternalRepresentation(id key, id *err);
OSStatus SSLSetCertificate(void *ctx, id certs) {
    ssl_t *s = S(ctx);
    if (!s) return errSecParam;
    if (s->h >= 0) return errSecBadReq;                         /* depois do início do handshake não muda */
    if (s->ident) { memset(s->ident, 0, s->ident_len); free(s->ident); s->ident = 0; s->ident_len = 0; }
    long n = certs ? CFArrayGetCount(certs) : 0;
    if (!n) return errSecSuccess;
    if (n > 10) return errSecParam;
    id ident = (id)CFArrayGetValueAtIndex(certs, 0), leaf = 0, key = 0;
    if (SecIdentityCopyCertificate(ident, &leaf) || SecIdentityCopyPrivateKey(ident, &key) || !leaf || !key) {
        if (leaf) CFRelease(leaf);
        if (key) CFRelease(key);
        return errSecParam;
    }
    id ders[10] = {0};
    ders[0] = SecCertificateCopyData(leaf);
    for (long i = 1; i < n; i++) ders[i] = SecCertificateCopyData((id)CFArrayGetValueAtIndex(certs, i));
    id kd = SecKeyCopyExternalRepresentation(key, 0);
    CFRelease(leaf); CFRelease(key);
    u32 chain = 4;
    OSStatus st = kd ? errSecSuccess : errSecParam;
    for (long i = 0; i < n; i++) { if (!ders[i]) st = errSecParam; else chain += 4 + (u32)CFDataGetLength(ders[i]); }
    if (!st) {
        u32 kl = (u32)CFDataGetLength(kd), total = 4 + chain + kl;
        u8 *b = malloc(total);
        if (!b) st = errSecAllocate;
        else {
            u32 o = 0;
            b[o++] = (u8)(chain >> 24); b[o++] = (u8)(chain >> 16); b[o++] = (u8)(chain >> 8); b[o++] = (u8)chain;
            b[o++] = 0; b[o++] = 0; b[o++] = 0; b[o++] = (u8)n;
            for (long i = 0; i < n; i++) {
                u32 l = (u32)CFDataGetLength(ders[i]);
                b[o++] = (u8)(l >> 24); b[o++] = (u8)(l >> 16); b[o++] = (u8)(l >> 8); b[o++] = (u8)l;
                memcpy(b + o, CFDataGetBytePtr(ders[i]), l); o += l;
            }
            memcpy(b + o, CFDataGetBytePtr(kd), kl);
            s->ident = b; s->ident_len = total;
        }
    }
    for (long i = 0; i < n; i++) if (ders[i]) CFRelease(ders[i]);
    if (kd) CFRelease(kd);
    return st;
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
/* API antiga por protocolo: o motor do kernel fala 1.2 e 1.3; min/max saem do que ficou ligado entre os dois */
static int proto_ok(int p) { return p == kSSLProtocol2 || p == kSSLProtocol3 || p == kTLSProtocol1 || p == kTLSProtocol11 ||
                                    p == kTLSProtocol12 || p == kTLSProtocol13 || p == kSSLProtocolAll; }
OSStatus SSLSetProtocolVersionEnabled(void *ctx, int p, Boolean on) {
    ssl_t *s = S(ctx);
    if (!s) return errSecParam;
    if (s->h >= 0) return errSecBadReq;
    if (!proto_ok(p)) return errSecParam;
    u16 bits = p == kSSLProtocolAll ? (u16)(1u << kSSLProtocol3 | 1u << kTLSProtocol1 | 1u << kTLSProtocol11 | 1u << kTLSProtocol12 |
                                            1u << kTLSProtocol13) : (u16)(1u << p);
    s->pmask = on ? (u16)(s->pmask | bits) : (u16)(s->pmask & ~bits);
    int lo = s->pmask & 1u << kTLSProtocol12 ? kTLSProtocol12 : s->pmask & 1u << kTLSProtocol13 ? kTLSProtocol13 : 0;
    int hi = s->pmask & 1u << kTLSProtocol13 ? kTLSProtocol13 : s->pmask & 1u << kTLSProtocol12 ? kTLSProtocol12 : 0;
    s->vmin = lo; s->vmax = hi;               /* 0 = nenhuma versão que o motor fala: o handshake recusa */
    return errSecSuccess;
}
OSStatus SSLGetProtocolVersionEnabled(void *ctx, int p, Boolean *on) {
    ssl_t *s = S(ctx);
    if (!s || !on || !proto_ok(p)) return errSecParam;
    *on = p == kSSLProtocolAll ? (s->pmask & 0x594) == 0x594 : (s->pmask >> p) & 1;
    return errSecSuccess;
}
OSStatus SSLGetProtocolVersionMin(void *ctx, int *v) { ssl_t *s = S(ctx); if (!s || !v) return errSecParam; *v = s->vmin; return errSecSuccess; }
OSStatus SSLGetProtocolVersionMax(void *ctx, int *v) { ssl_t *s = S(ctx); if (!s || !v) return errSecParam; *v = s->vmax; return errSecSuccess; }
OSStatus SSLGetNegotiatedProtocolVersion(void *ctx, int *v) {
    ssl_t *s = S(ctx);
    if (!s || !v) return errSecParam;
    *v = s->state != kSSLConnected ? 0 : s->info.version == 0x0304 ? kTLSProtocol13 : kTLSProtocol12;
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
    *c = s->state == kSSLConnected ? s->info.suite : 0;
    return errSecSuccess;
}
Boolean CFStringGetCString(id str, char *buf, long size, unsigned enc);
id CFStringCreateWithCString(void *alloc, const char *s, unsigned enc);
OSStatus SSLSetALPNProtocols(void *ctx, id protos) {
    ssl_t *s = S(ctx);
    if (!s) return errSecParam;
    if (s->state != kSSLIdle) return errSecBadReq;
    u32 n = 0;
    long cnt = protos ? CFArrayGetCount(protos) : 0;
    for (long i = 0; i < cnt; i++) {
        char nm[64];
        id str = CFArrayGetValueAtIndex(protos, i);
        if (!str || !CFStringGetCString(str, nm, sizeof nm, 0x08000100)) return errSecParam;
        u32 l = (u32)strlen(nm);
        if (!l || l > 32 || n + 1 + l > sizeof s->alpn) return errSecParam;
        s->alpn[n++] = (u8)l;
        memcpy(s->alpn + n, nm, l);
        n += l;
    }
    s->alpn_len = n;
    return errSecSuccess;
}
OSStatus SSLCopyALPNProtocols(void *ctx, id *protos) {
    ssl_t *s = S(ctx);
    if (!s || !protos) return errSecParam;
    id a = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
    if (s->state == kSSLConnected && s->info.alpn[0]) {
        id str = CFStringCreateWithCString(0, s->info.alpn, 0x08000100);
        if (str) { CFArrayAppendValue(a, str); CFRelease(str); }
    }
    *protos = a;
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
    if (s->state != kSSLConnected || s->h < 0) return errSecSuccess;
    long need = net(MOS_NET_TLS_CERTS, s->h, 0, 0);
    if (need < 8) return errSecSuccess;
    u8 *buf = malloc((size_t)need);
    if (!buf) return errSecAllocate;
    if (net(MOS_NET_TLS_CERTS, s->h, (long)buf, need) != need) { free(buf); return errSSLInternal; }
    u32 count = (u32)buf[0] | (u32)buf[1] << 8 | (u32)buf[2] << 16 | (u32)buf[3] << 24;
    id t = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
    u32 o = 4;
    for (u32 i = 0; t && i < count && o + 4 <= (u32)need; i++) {
        u32 L = (u32)buf[o] | (u32)buf[o + 1] << 8 | (u32)buf[o + 2] << 16 | (u32)buf[o + 3] << 24;
        o += 4;
        if (o + L > (u32)need) break;
        id data = CFDataCreate(0, buf + o, L);
        id cert = data ? SecCertificateCreateWithData(0, data) : 0;
        if (cert) { CFArrayAppendValue(t, cert); CFRelease(cert); }
        if (data) CFRelease(data);
        o += L;
    }
    free(buf);
    if (!t || !CFArrayGetCount(t)) { if (t) CFRelease(t); return errSecSuccess; }
    cx_trust_set_host(t, s->peer);
    if (s->chain_ok) cx_trust_mark_verified(t);
    *trust = t;
    return errSecSuccess;
}
OSStatus SSLCopyPeerCertificates(void *ctx, id *certs) {
    if (!S(ctx) || !certs) return errSecParam;
    *certs = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
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

/* ---------------------------------------------------------------- validação pedida pelo app
 * Com V_* ou âncoras próprias, o kernel não valida (MOS_NET_TLS_NOVERIFY) e a cadeia é conferida aqui com o mesmo
 * x509 do SecTrust (sectrust.c), como o SecureTransport da Apple faz com o SecTrust:
 *   - AllowsAnyRoot: a ponta de cima da cadeia vira âncora, e o resto (nome, datas, assinaturas) continua conferido;
 *   - AllowsExpiredCerts/Roots: data fora da validade é aceita, conferindo de novo num instante em que todos valem;
 *   - EnableCertVerify(false) e BreakOnServerAuth: não confere (o app decide com SSLCopyPeerTrust).
 * chain_ok diz se a cadeia passa pela política padrão (raízes do sistema), e só aí a confiança devolvida por
 * SSLCopyPeerTrust já vem marcada como avaliada. */
extern int cx_sectrust_eval(const u8 *const *ders, const u32 *lens, unsigned n, const u8 *const *anc, const u32 *alens,
                            unsigned na, int use_system, int policy, const char *host, long long at_unix, char *err,
                            unsigned errlen);
enum { XE_OK = 0, XE_PARSE, XE_UNKNOWN_ISSUER, XE_SELF_SIGNED, XE_UNTRUSTED_ROOT, XE_EXPIRED, XE_NOT_YET_VALID };
static int app_verifies(ssl_t *s) { return s->vflags || s->roots || s->break_auth; }
static OSStatus verify_chain(ssl_t *s) {
    long need = net(MOS_NET_TLS_CERTS, s->h, 0, 0);
    if (need < 8) return errSSLXCertChainInvalid;
    u8 *buf = malloc((size_t)need);
    if (!buf) return errSecAllocate;
    if (net(MOS_NET_TLS_CERTS, s->h, (long)buf, need) != need) { free(buf); return errSSLInternal; }
    const u8 *ders[10];
    u32 lens[10], n = 0, o = 4, count = (u32)buf[0] | (u32)buf[1] << 8 | (u32)buf[2] << 16 | (u32)buf[3] << 24;
    for (u32 i = 0; i < count && n < 10 && o + 4 <= (u32)need; i++) {
        u32 L = (u32)buf[o] | (u32)buf[o + 1] << 8 | (u32)buf[o + 2] << 16 | (u32)buf[o + 3] << 24;
        o += 4;
        if (o + L > (u32)need) break;
        ders[n] = buf + o; lens[n++] = L;
        o += L;
    }
    const char *host = s->peer[0] ? s->peer : 0;
    char err[160];
    s->chain_ok = n && cx_sectrust_eval(ders, lens, n, 0, 0, 0, 1, 1, host, -1, err, sizeof err) == XE_OK;
    if (!n) { free(buf); return errSSLXCertChainInvalid; }
    if ((s->vflags & V_OFF) || s->break_auth) { free(buf); return errSecSuccess; }
    /* âncoras: as do app e, com AllowsAnyRoot, o certificado do topo da cadeia */
    const u8 *anc[34];
    u32 alens[34], na = 0;
    id rd[32];
    long nr = s->roots ? CFArrayGetCount(s->roots) : 0;
    if (nr > 32) nr = 32;
    for (long i = 0; i < nr; i++) {
        rd[i] = SecCertificateCopyData((id)CFArrayGetValueAtIndex(s->roots, i));
        if (rd[i]) { anc[na] = CFDataGetBytePtr(rd[i]); alens[na++] = (u32)CFDataGetLength(rd[i]); }
    }
    int sys = !s->roots_only;
    int e = cx_sectrust_eval(ders, lens, n, anc, alens, na, sys, 1, host, -1, err, sizeof err);
    if ((e == XE_UNKNOWN_ISSUER || e == XE_SELF_SIGNED || e == XE_UNTRUSTED_ROOT) && (s->vflags & V_ANY_ROOT)) {
        anc[na] = ders[n - 1]; alens[na++] = lens[n - 1];
        e = cx_sectrust_eval(ders, lens, n, anc, alens, na, sys, 1, host, -1, err, sizeof err);
    }
    if ((e == XE_EXPIRED || e == XE_NOT_YET_VALID) && (s->vflags & (V_EXP_CERTS | V_EXP_ROOTS))) {
        /* um instante em que todos os certificados valem: o maior "não antes de" */
        double from = -1e300, to = 1e300;
        for (u32 i = 0; i < n; i++) {
            id d = CFDataCreate(0, ders[i], lens[i]), c = d ? SecCertificateCreateWithData(0, d) : 0;
            if (c) {
                double a = SecCertificateNotValidBefore(c), b = SecCertificateNotValidAfter(c);
                if (a > from) from = a;
                if (b < to) to = b;
                CFRelease(c);
            }
            if (d) CFRelease(d);
        }
        if (from <= to) e = cx_sectrust_eval(ders, lens, n, anc, alens, na, sys, 1, host, (long long)(from + 978307200.0) + 1, err,
                                             sizeof err);
    }
    for (long i = 0; i < nr; i++) if (rd[i]) CFRelease(rd[i]);
    free(buf);
    if (e == XE_OK) return errSecSuccess;
    if (getenv("MOS_TLS_DEBUG")) {
        char line[256];
        int k = snprintf(line, sizeof line, "[SecureTransport] %s: cadeia recusada: %s\n", s->peer, err);
        if (k > 0) write(2, line, (size_t)k);
    }
    return e == XE_EXPIRED ? errSSLCertExpired : e == XE_NOT_YET_VALID ? errSSLCertNotYetValid
         : e == XE_UNKNOWN_ISSUER ? errSSLNoRootCert : e == XE_SELF_SIGNED || e == XE_UNTRUSTED_ROOT ? errSSLUnknownRootCert
         : errSSLXCertChainInvalid;
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
        if (s->alpn_len && net(MOS_NET_TLS_ALPN, h, (long)s->alpn, s->alpn_len) < 0) { s->err = errSSLBadConfiguration; return s->err; }
        if (s->ident && net(MOS_NET_TLS_IDENTITY, h, (long)s->ident, s->ident_len) < 0) { s->err = errSSLBadCert; return s->err; }
        if (!s->vmin) { s->err = errSSLBadConfiguration; return s->err; }   /* 1.2 e 1.3 desligados */
        if (app_verifies(s) && net(MOS_NET_TLS_NOVERIFY, h, 1, 0) < 0) { s->err = errSSLInternal; return s->err; }
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
            if (app_verifies(s)) {
                OSStatus v = verify_chain(s);
                if (v) { s->state = kSSLAborted; s->err = v; return v; }
            } else s->chain_ok = 1;                    /* o kernel validou pela política padrão */
            s->state = kSSLConnected;
            net(MOS_NET_TLS_INFO, s->h, (long)&s->info, 0);
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
    id arr = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
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
OSStatus cx_ts_copy_certs(int domain, id *certs);                 /* seccert.c: usuário e admin */
OSStatus cx_ts_copy_settings(id cert, int domain, id *settings);
OSStatus SecTrustSettingsCopyCertificates(int domain, id *certs) {
    if (certs) *certs = 0;
    if (domain != kSecTrustSettingsDomainSystem) return cx_ts_copy_certs(domain, certs);
    id roots = system_roots();
    if (!roots || !CFArrayGetCount(roots)) return errSecNoTrustSettings;
    if (certs) {
        id copy = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
        for (long i = 0; i < CFArrayGetCount(roots); i++) CFArrayAppendValue(copy, CFArrayGetValueAtIndex(roots, i));
        *certs = copy;
    }
    return errSecSuccess;
}
/* SecTrustCopyAnchorCertificates: as mesmas raízes do sistema (as âncoras padrão do SecTrust) */
OSStatus SecTrustCopyAnchorCertificates(id *anchors) {
    if (!anchors) return errSecParam;
    *anchors = 0;
    OSStatus r = SecTrustSettingsCopyCertificates(kSecTrustSettingsDomainSystem, anchors);
    if (r == errSecNoTrustSettings) { *anchors = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks); return errSecSuccess; }
    return r;
}
/* raiz do sistema sem ajuste específico = array vazio ("confiar como raiz"), o que o rustls-native-certs espera */
OSStatus SecTrustSettingsCopyTrustSettings(id cert, int domain, id *settings) {
    if (settings) *settings = 0;
    if (cert && domain != kSecTrustSettingsDomainSystem) return cx_ts_copy_settings(cert, domain, settings);
    if (!cert) return errSecNoTrustSettings;
    if (settings) *settings = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
    return errSecSuccess;
}
