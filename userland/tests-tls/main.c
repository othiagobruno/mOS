/*
 * mOS — teste do SecureTransport do libSystem (userland/libsystem/securetransport.c, kernel/net/usersock.c).
 * Compila igual no macOS (clang main.c -framework Security -framework CoreFoundation). No mOS:
 *   .\tools\build-tlstest-app.ps1 -OutDir build-net   (abre no boot; saída na serial: procure "tlstest:")
 * Usa o SecureTransport do jeito do crate security-framework (o native-tls do Codex CLI): callbacks que repetem a
 * leitura até encher o pedido, kSSLSessionOptionBreakOnServerAuth e avaliação da confiança pelo app.
 *   1. SecTrustSettingsCopyCertificates(sistema): as raízes, como o rustls-native-certs lê
 *   2. HTTPS bloqueante: handshake, errSSLPeerAuthCompleted, SecTrustEvaluate, GET, leitura até o fim
 *   3. HTTPS não-bloqueante: errSSLWouldBlock + poll até concluir
 *   4. nome que o servidor não serve: o handshake é recusado (alerta do servidor ou errSSLXCertChainInvalid)
 *   5. SecTrust montado pelo próprio app continua "não confiável" (só o do SSL vem validado pelo kernel)
 * Imprime "tlstest: N/N ok". Argumento opcional: o host (padrão example.com).
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

typedef const void *CFTypeRef, *CFArrayRef, *SecTrustRef;
typedef void *SSLContextRef;
typedef int OSStatus;
typedef unsigned char Boolean;
typedef long CFIndex;
typedef OSStatus (*SSLReadFunc)(const void *, void *, size_t *);
typedef OSStatus (*SSLWriteFunc)(const void *, const void *, size_t *);

SSLContextRef SSLCreateContext(const void *alloc, int side, int type);
OSStatus SSLSetIOFuncs(SSLContextRef, SSLReadFunc, SSLWriteFunc);
OSStatus SSLSetConnection(SSLContextRef, const void *);
OSStatus SSLSetPeerDomainName(SSLContextRef, const char *, size_t);
OSStatus SSLSetSessionOption(SSLContextRef, int, Boolean);
OSStatus SSLSetProtocolVersionMin(SSLContextRef, int);
OSStatus SSLSetProtocolVersionMax(SSLContextRef, int);
OSStatus SSLGetNumberEnabledCiphers(SSLContextRef, size_t *);
OSStatus SSLGetEnabledCiphers(SSLContextRef, unsigned *, size_t *);
OSStatus SSLSetEnabledCiphers(SSLContextRef, const unsigned *, size_t);
OSStatus SSLHandshake(SSLContextRef);
OSStatus SSLCopyPeerTrust(SSLContextRef, SecTrustRef *);
OSStatus SSLWrite(SSLContextRef, const void *, size_t, size_t *);
OSStatus SSLRead(SSLContextRef, void *, size_t, size_t *);
OSStatus SSLGetSessionState(SSLContextRef, int *);
OSStatus SSLClose(SSLContextRef);
OSStatus SecTrustSetAnchorCertificates(SecTrustRef, CFArrayRef);
OSStatus SecTrustSetAnchorCertificatesOnly(SecTrustRef, Boolean);
OSStatus SecTrustEvaluate(SecTrustRef, unsigned *);
OSStatus SecTrustCreateWithCertificates(CFTypeRef, CFTypeRef, SecTrustRef *);
OSStatus SecTrustSettingsCopyCertificates(int, CFArrayRef *);
OSStatus SecTrustSettingsCopyTrustSettings(CFTypeRef, int, CFArrayRef *);
CFIndex CFArrayGetCount(CFArrayRef);
const void *CFArrayGetValueAtIndex(CFArrayRef, CFIndex);
void CFRelease(CFTypeRef);

enum { errSSLWouldBlock = -9803, errSSLClosedGraceful = -9805, errSSLClosedAbort = -9806,
       errSSLXCertChainInvalid = -9807, errSSLClosedNoNotify = -9816, errSSLPeerAuthCompleted = -9841 };

static int ok, total;
static void check(int cond, const char *what) {
    total++;
    if (cond) ok++;
    printf("%s %s\n", cond ? "  ok  " : "  FALHA", what);
    fflush(stdout);
}

/* as callbacks do security-framework: repetem até encher; EAGAIN = errSSLWouldBlock; 0 = fechado */
static OSStatus rd(const void *conn, void *data, size_t *len) {
    int fd = (int)(long)conn;
    size_t got = 0;
    OSStatus st = 0;
    while (got < *len) {
        ssize_t k = read(fd, (char *)data + got, *len - got);
        if (k == 0) { st = errSSLClosedNoNotify; break; }
        if (k < 0) { st = errno == EAGAIN ? errSSLWouldBlock : -36; break; }
        got += (size_t)k;
    }
    *len = got;
    return st;
}
static OSStatus wr(const void *conn, const void *data, size_t *len) {
    int fd = (int)(long)conn;
    size_t put = 0;
    OSStatus st = 0;
    while (put < *len) {
        ssize_t k = write(fd, (const char *)data + put, *len - put);
        if (k <= 0) { st = k < 0 && errno == EAGAIN ? errSSLWouldBlock : -36; break; }
        put += (size_t)k;
    }
    *len = put;
    return st;
}

static int dial(const char *host, int nonblock) {
    struct addrinfo hints, *res = 0;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, "443", &hints, &res) || !res) return -1;
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return -1;
    if (connect(s, res->ai_addr, res->ai_addrlen) != 0) { close(s); return -1; }
    if (nonblock) fcntl(s, F_SETFL, fcntl(s, F_GETFL) | O_NONBLOCK);
    return s;
}

static SSLContextRef make_ctx(int s, const char *name) {
    SSLContextRef c = SSLCreateContext(0, 1, 0);
    if (!c) return 0;
    SSLSetIOFuncs(c, rd, wr);
    SSLSetConnection(c, (const void *)(long)s);
    SSLSetPeerDomainName(c, name, strlen(name));
    SSLSetSessionOption(c, 0, 1);                 /* kSSLSessionOptionBreakOnServerAuth */
    SSLSetProtocolVersionMin(c, 8);               /* kTLSProtocol12 */
    SSLSetProtocolVersionMax(c, 10);              /* kTLSProtocol13 */
    return c;
}

static void wait_fd(int s) {
    struct pollfd pf = { s, POLLIN, 0 };
    poll(&pf, 1, 200);
}

int main(int argc, char **argv) {
    const char *host = argc > 1 ? argv[1] : "example.com";
    char req[256];
    snprintf(req, sizeof req, "GET / HTTP/1.1\r\nHost: %s\r\nUser-Agent: mOS-tlstest\r\nConnection: close\r\n\r\n", host);

    /* 1. raízes do sistema */
    CFArrayRef roots = 0;
    OSStatus st = SecTrustSettingsCopyCertificates(2, &roots);
    CFIndex nroots = roots ? CFArrayGetCount(roots) : 0;
    check(st == 0 && nroots > 100, "SecTrustSettingsCopyCertificates(sistema) > 100 raízes");
    CFArrayRef ts = 0;
    check(nroots && SecTrustSettingsCopyTrustSettings(CFArrayGetValueAtIndex(roots, 0), 2, &ts) == 0 && ts && CFArrayGetCount(ts) == 0,
          "SecTrustSettingsCopyTrustSettings = vazio (confiar como raiz)");
    CFArrayRef user = 0;
    check(SecTrustSettingsCopyCertificates(0, &user) == -25263, "domínio do usuário sem ajustes (errSecNoTrustSettings)");
    printf("  (%ld raízes)\n", (long)nroots);

    /* 2. bloqueante */
    int s = dial(host, 0);
    check(s >= 0, "conexão TCP na porta 443");
    SSLContextRef c = make_ctx(s, host);
    check(c != 0, "SSLCreateContext (cliente)");
    size_t nc = 0;
    unsigned ciphers[32];
    SSLGetNumberEnabledCiphers(c, &nc);
    size_t got = nc < 32 ? nc : 32;
    check(nc > 0 && SSLGetEnabledCiphers(c, ciphers, &got) == 0 && SSLSetEnabledCiphers(c, ciphers, got) == 0, "suítes habilitadas");
    st = SSLHandshake(c);
    check(st == errSSLPeerAuthCompleted, "handshake para em errSSLPeerAuthCompleted");
    if (st != errSSLPeerAuthCompleted) printf("  SSLHandshake = %d\n", (int)st);
    SecTrustRef trust = 0;
    unsigned result = 0;
    check(SSLCopyPeerTrust(c, &trust) == 0 && trust && SecTrustSetAnchorCertificates(trust, roots) == 0 &&
          SecTrustSetAnchorCertificatesOnly(trust, 0) == 0 && SecTrustEvaluate(trust, &result) == 0 && (result == 1 || result == 4),
          "SSLCopyPeerTrust + SecTrustEvaluate = confiável");
    if (trust) CFRelease(trust);
    st = SSLHandshake(c);
    int state = -1;
    SSLGetSessionState(c, &state);
    check(st == 0 && state == 2, "handshake concluído (kSSLConnected)");
    size_t w = 0;
    check(SSLWrite(c, req, strlen(req), &w) == 0 && w == strlen(req), "SSLWrite da requisição");
    static char buf[256 * 1024];
    size_t n = 0;
    for (;;) {
        size_t k = 0;
        st = SSLRead(c, buf + n, sizeof buf - 1 - n, &k);
        n += k;
        if (st || n >= sizeof buf - 1) break;
    }
    buf[n] = 0;
    check(n > 0 && !strncmp(buf, "HTTP/1.1 ", 9), "resposta HTTPS lida");
    check(st == errSSLClosedGraceful || st == errSSLClosedNoNotify || n >= sizeof buf - 1, "fim do fluxo (errSSLClosed*)");
    char *e = strstr(buf, "\r\n");
    if (e) printf("  %.*s (%zu bytes)\n", (int)(e - buf), buf, n);
    SSLClose(c);
    CFRelease(c);
    close(s);

    /* 3. não-bloqueante */
    s = dial(host, 1);
    c = s >= 0 ? make_ctx(s, host) : 0;
    int would = 0;
    st = -1;
    for (int i = 0; c && i < 400; i++) {
        st = SSLHandshake(c);
        if (st == errSSLWouldBlock) { would++; wait_fd(s); continue; }
        if (st == errSSLPeerAuthCompleted) continue;
        break;
    }
    check(st == 0 && would > 0, "handshake não-bloqueante (errSSLWouldBlock + poll)");
    w = 0;
    if (c) SSLWrite(c, req, strlen(req), &w);
    n = 0;
    for (int i = 0; c && i < 2000; i++) {
        size_t k = 0;
        st = SSLRead(c, buf + n, sizeof buf - 1 - n, &k);
        n += k;
        if (st == errSSLWouldBlock) { wait_fd(s); continue; }
        if (st || n >= sizeof buf - 1) break;
    }
    buf[n] = 0;
    check(n > 0 && !strncmp(buf, "HTTP/1.1 ", 9), "resposta HTTPS em modo não-bloqueante");
    if (c) { SSLClose(c); CFRelease(c); }
    if (s >= 0) close(s);

    /* 4. nome que o servidor não serve: ele recusa (alerta) ou o certificado não bate; nunca conecta */
    s = dial(host, 0);
    c = s >= 0 ? make_ctx(s, "nome-errado.mos.invalid") : 0;
    st = c ? SSLHandshake(c) : 0;
    check(st == errSSLXCertChainInvalid || st == -9802 || st == errSSLClosedAbort, "nome errado: handshake recusado");
    printf("  SSLHandshake = %d\n", (int)st);
    if (c) CFRelease(c);
    if (s >= 0) close(s);

    /* 5. uma confiança montada pelo app (fora do SSL) não é dada como válida */
    SecTrustRef own = 0;
    result = 0;
    check(nroots && SecTrustCreateWithCertificates(CFArrayGetValueAtIndex(roots, 0), 0, &own) == 0 && own &&
          SecTrustEvaluate(own, &result) == 0 && result == 5, "SecTrustCreateWithCertificates avalia como não confiável");
    if (own) CFRelease(own);

    if (roots) CFRelease(roots);
    printf("tlstest: %d/%d ok\n", ok, total);
    return ok == total ? 0 : 1;
}
