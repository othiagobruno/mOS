/*
 * mOS — transporte do mnet no libSystem: sockets BSD deste libSystem (IPv4 e IPv6, connect com O_NONBLOCK + poll)
 * e TLS pelo SecureTransport (motor único do kernel, raízes da Mozilla, ALPN h2/http/1.1).
 * NSURLSession, CFNetwork e libcurl pegam o cliente compartilhado com cx_mnet_client().
 */
#include "mnet.h"
#include "sys.h"
extern const char kCFTypeArrayCallBacks[];   /* cfcollections.c: callbacks de objeto CF (retain/isEqual:) */

typedef int OSStatus;
struct mos_ai { int flags, family, socktype, protocol; unsigned addrlen; char *canon; void *addr; struct mos_ai *next; };
typedef struct { int fd; short events, revents; } pfd_t;
int getaddrinfo(const char *, const char *, const void *, void **);
void freeaddrinfo(void *);
int socket(int, int, int);
int connect(int, const void *, unsigned);
long send(int, const void *, unsigned long, int);
long recv(int, void *, unsigned long, int);
int close(int);
int fcntl(int, int, ...);
int poll(pfd_t *, unsigned, int);
int getsockopt(int, int, int, void *, unsigned *);
int setsockopt(int, int, int, const void *, unsigned);
int *__error(void);
typedef struct { long long sec, nsec; } ts_t;
int clock_gettime(int, ts_t *);
long time(long *);
void os_unfair_lock_lock(void *l);
void os_unfair_lock_unlock(void *l);
void *SSLCreateContext(void *alloc, int side, int type);
OSStatus SSLSetIOFuncs(void *, void *, void *);
OSStatus SSLSetConnection(void *, const void *);
OSStatus SSLSetPeerDomainName(void *, const char *, unsigned long);
OSStatus SSLSetALPNProtocols(void *, void *);
OSStatus SSLCopyALPNProtocols(void *, void **);
OSStatus SSLHandshake(void *);
OSStatus SSLWrite(void *, const void *, unsigned long, unsigned long *);
OSStatus SSLRead(void *, void *, unsigned long, unsigned long *);
OSStatus SSLClose(void *);
OSStatus SSLCopyPeerTrust(void *, void **);
OSStatus SSLGetBufferedReadSize(void *, size_t *);
void CFRelease(void *);
void *CFArrayCreateMutable(void *, long, const void *);
void CFArrayAppendValue(void *, const void *);
long CFArrayGetCount(void *);
void *CFArrayGetValueAtIndex(void *, long);
void *CFStringCreateWithCString(void *, const char *, unsigned);
unsigned char CFStringGetCString(void *, char *, long, unsigned);

#define E_AGAIN 35
#define E_INPROGRESS 36
#define errSSLWouldBlock (-9803)
#define errSSLClosedGraceful (-9805)
#define errSSLClosedNoNotify (-9816)
#define errSSLXCertChainInvalid (-9807)
#define UTF8 0x08000100u

typedef struct { int fd; void *ssl; int keep_fd; } conn_t;   /* keep_fd: socket nativo do app (não fecha) */

static long long mono_ms(void *e) { (void)e; ts_t t; clock_gettime(6, &t); return t.sec * 1000 + t.nsec / 1000000; }
static long long unix_s(void *e) { (void)e; return (long long)time(0); }
static unsigned g_lock;
static void lk(void *e) { (void)e; os_unfair_lock_lock(&g_lock); }
static void ulk(void *e) { (void)e; os_unfair_lock_unlock(&g_lock); }
static int wait_fd(int fd, int ev, int ms) { pfd_t p = {fd, (short)ev, 0}; return poll(&p, 1, ms < 0 ? 0 : ms); }

static void *c_connect(void *e, const char *host, int port, int tmo, int *err) {
    (void)e;
    char ps[8]; int n = 0, v = port;
    char t[8]; do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v && n < 6);
    for (int i = 0; i < n; i++) ps[i] = t[n - 1 - i];
    ps[n] = 0;
    struct mos_ai hint, *res = 0;
    memset(&hint, 0, sizeof hint);
    hint.socktype = 1;
    if (getaddrinfo(host, ps, &hint, (void **)&res) || !res) { *err = MNET_E_NO_HOST; return 0; }
    int fd = -1, timed_out = 0;
    long long end = mono_ms(0) + tmo;
    for (struct mos_ai *a = res; a && fd < 0; a = a->next) {
        if (a->family != 2 && a->family != 30) continue;
        fd = socket(a->family, 1, 6);
        if (fd < 0) continue;
        fcntl(fd, 4, 4);                                         /* F_SETFL O_NONBLOCK */
        int r = connect(fd, a->addr, a->addrlen);
        if (r && *__error() == E_INPROGRESS) {
            int left = (int)(end - mono_ms(0));
            if (left > 0 && wait_fd(fd, 4, left) == 1) {
                int so = 0; unsigned sl = sizeof so;
                getsockopt(fd, 0xffff, 0x1007, &so, &sl);          /* SO_ERROR */
                r = so ? -1 : 0;
            } else { r = -1; timed_out = 1; }
        }
        if (r) { close(fd); fd = -1; }
    }
    freeaddrinfo(res);
    if (fd < 0) { *err = timed_out ? MNET_E_TIMEOUT : MNET_E_CONNECT; return 0; }
    int one = 1;
    setsockopt(fd, 6, 1, &one, sizeof one);                      /* TCP_NODELAY */
    conn_t *c = calloc(1, sizeof *c);
    if (!c) { close(fd); *err = MNET_E_UNKNOWN; return 0; }
    c->fd = fd;
    return c;
}

/* E/S do SecureTransport sobre o socket não bloqueante */
static OSStatus ssl_rd(const void *cn, void *d, size_t *len) {
    const conn_t *c = cn;
    size_t want = *len, got = 0;
    while (got < want) {
        long k = recv(c->fd, (char *)d + got, want - got, 0);
        if (k > 0) { got += (size_t)k; continue; }
        if (k == 0) { *len = got; return errSSLClosedNoNotify; }
        *len = got;
        return *__error() == E_AGAIN ? errSSLWouldBlock : -36;
    }
    *len = got;
    return 0;
}
static OSStatus ssl_wr(const void *cn, const void *d, size_t *len) {
    const conn_t *c = cn;
    size_t want = *len, put = 0;
    while (put < want) {
        long k = send(c->fd, (const char *)d + put, want - put, 0);
        if (k > 0) { put += (size_t)k; continue; }
        if (k < 0 && *__error() == E_AGAIN && wait_fd(c->fd, 4, 30000) == 1) continue;   /* espera o buffer esvaziar */
        *len = put;
        return -36;
    }
    *len = put;
    return 0;
}

static void msgcpy(char *m, size_t ml, const char *s) {
    if (!m || !ml) return;
    size_t n = strlen(s);
    if (n >= ml) n = ml - 1;
    memcpy(m, s, n); m[n] = 0;
}
static int c_starttls(void *e, void *cv, const char *sni, const uint8_t *alpn, size_t al, char out[33], int tmo,
                      char *msg, size_t ml) {
    (void)e;
    conn_t *c = cv;
    out[0] = 0;
    c->ssl = SSLCreateContext(0, 1, 0);                           /* cliente, stream */
    if (!c->ssl) { msgcpy(msg, ml, "TLS indisponível"); return MNET_E_TLS; }
    SSLSetIOFuncs(c->ssl, (void *)ssl_rd, (void *)ssl_wr);
    SSLSetConnection(c->ssl, c);
    SSLSetPeerDomainName(c->ssl, sni, strlen(sni));
    if (alpn && al) {
        void *arr = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
        for (size_t i = 0; i < al; i += 1u + alpn[i]) {
            char nm[40];
            size_t l = alpn[i] < 39 ? alpn[i] : 39;
            memcpy(nm, alpn + i + 1, l); nm[l] = 0;
            void *s = CFStringCreateWithCString(0, nm, UTF8);
            if (s) { CFArrayAppendValue(arr, s); CFRelease(s); }
        }
        SSLSetALPNProtocols(c->ssl, arr);
        CFRelease(arr);
    }
    long long end = mono_ms(0) + tmo;
    OSStatus st;
    while ((st = SSLHandshake(c->ssl)) == errSSLWouldBlock) {
        int left = (int)(end - mono_ms(0));
        if (left <= 0) { msgcpy(msg, ml, "tempo esgotado no handshake TLS"); return MNET_E_TIMEOUT; }
        wait_fd(c->fd, 1, left > 250 ? 250 : left);
    }
    if (st) {
        msgcpy(msg, ml, st == errSSLXCertChainInvalid ? "certificado do servidor não confiável" : "o handshake TLS falhou");
        return st == errSSLXCertChainInvalid ? MNET_E_CERT_UNTRUSTED : MNET_E_TLS;
    }
    void *got = 0;
    if (!SSLCopyALPNProtocols(c->ssl, &got) && got) {
        if (CFArrayGetCount(got) > 0) CFStringGetCString(CFArrayGetValueAtIndex(got, 0), out, 33, UTF8);
        CFRelease(got);
    }
    return 0;
}
static long c_read(void *e, void *cv, void *b, size_t n, int tmo) {
    (void)e;
    conn_t *c = cv;
    for (int pass = 0; pass < 2; pass++) {
        if (c->ssl) {
            unsigned long d = 0;
            OSStatus st = SSLRead(c->ssl, b, n, &d);
            if (d) return (long)d;
            if (st == errSSLClosedGraceful || st == errSSLClosedNoNotify) return 0;
            if (st && st != errSSLWouldBlock) return -1;
        } else {
            long k = recv(c->fd, b, n, 0);
            if (k >= 0) return k;
            if (*__error() != E_AGAIN) return -1;
        }
        if (pass == 0 && wait_fd(c->fd, 1, tmo) <= 0) return -2;
    }
    return -2;
}
static long c_write(void *e, void *cv, const void *b, size_t n, int tmo) {
    (void)e;
    conn_t *c = cv;
    if (c->ssl) {
        unsigned long d = 0;
        OSStatus st = SSLWrite(c->ssl, b, n, &d);
        if (d) return (long)d;
        return st == errSSLWouldBlock ? -2 : -1;
    }
    for (;;) {
        long k = send(c->fd, b, n, 0);
        if (k > 0) return k;
        if (k < 0 && *__error() != E_AGAIN) return -1;
        if (wait_fd(c->fd, 4, tmo) <= 0) return -2;
    }
}
static void c_close(void *e, void *cv) {
    (void)e;
    conn_t *c = cv;
    if (c->ssl) { SSLClose(c->ssl); CFRelease(c->ssl); }
    if (!c->keep_fd) close(c->fd);
    free(c);
}

static void *c_trust(void *e, void *cv) {
    (void)e;
    conn_t *c = cv;
    void *t = 0;
    if (c->ssl && SSLCopyPeerTrust(c->ssl, &t)) t = 0;
    return t;
}
/* legível: bytes já decifrados no TLS ou dados no socket */
static int c_wait(void *e, void *cv, int ev, int tmo) {
    (void)e;
    conn_t *c = cv;
    size_t buffered = 0;
    if (ev == 1 && c->ssl && !SSLGetBufferedReadSize(c->ssl, &buffered) && buffered) return 1;
    int r = wait_fd(c->fd, ev == 4 ? 4 : 1, tmo);
    return r < 0 ? -1 : r > 0;
}
static const mnet_io_t g_io = {0, c_connect, c_starttls, c_read, c_write, c_close, mono_ms, unix_s, lk, ulk, c_trust, c_wait};

/* socket nativo do app (CFStreamCreatePairWithSocket): conexão do transporte sobre o fd, que fica não bloqueante */
void *cx_mnet_conn_from_fd(int fd, int close_fd) {
    if (fd < 0) return 0;
    conn_t *c = calloc(1, sizeof *c);
    if (!c) return 0;
    c->fd = fd; c->keep_fd = !close_fd;
    fcntl(fd, 4, fcntl(fd, 3) | 4);                                /* F_SETFL O_NONBLOCK */
    return c;
}
int cx_mnet_conn_fd(void *cv) { return cv ? ((conn_t *)cv)->fd : -1; }
void cx_mnet_conn_set_close(void *cv, int close_fd) { if (cv) ((conn_t *)cv)->keep_fd = !close_fd; }
void *cx_mnet_conn_ssl(void *cv) { return cv ? ((conn_t *)cv)->ssl : 0; }
static mnet_client_t g_client;
static mnet_jar_t g_jar;
static int g_ready;
static unsigned g_init_lock;

const mnet_io_t *cx_mnet_io(void) { return &g_io; }
mnet_jar_t *cx_mnet_jar(void) { cx_mnet_client(); return &g_jar; }
mnet_client_t *cx_mnet_client(void) {
    if (g_ready) return &g_client;
    os_unfair_lock_lock(&g_init_lock);
    if (!g_ready) {
        mnet_client_init(&g_client, &g_io);
        mnet_jar_init(&g_jar);
        g_client.jar = &g_jar;
        __sync_synchronize();
        g_ready = 1;
    }
    os_unfair_lock_unlock(&g_init_lock);
    return &g_client;
}
