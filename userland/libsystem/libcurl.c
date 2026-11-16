/*
 * mOS — libcurl.4.dylib sobre o mnet (docs/rede-apps.md): easy, multi, share, URL API, MIME, cabeçalhos, escape.
 * HTTP/1.1 e HTTP/2 (ALPN), TLS do motor do sistema (a verificação de certificado não desliga: CURLOPT_SSL_VERIFY*
 * em 0 é aceito e ignorado), redirect, cookies (motor e arquivo Netscape), Basic/Digest, proxy HTTP/CONNECT/SOCKS5 por
 * opção ou pelas variáveis http_proxy/https_proxy/all_proxy/no_proxy, gzip/deflate/br com CURLOPT_ACCEPT_ENCODING,
 * progresso, faixas e retomada. No multi, cada transferência roda num fio, mas todo callback do app roda no fio que
 * chama curl_multi_perform, como na libcurl.
 * Só HTTP(S): outros protocolos (ftp, smtp, file...) devolvem CURLE_UNSUPPORTED_PROTOCOL.
 */
#include <stdarg.h>
#include "mnet.h"

#ifdef MNET_HOST
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#define FILE_T FILE
#else
typedef void FILE_T;
FILE_T *fopen(const char *p, const char *m);
size_t fread(void *p, size_t s, size_t n, FILE_T *f);
size_t fwrite(const void *p, size_t s, size_t n, FILE_T *f);
int fclose(FILE_T *f);
int fputs(const char *s, FILE_T *f);
extern FILE_T *__stderrp;
#define stderr __stderrp
int snprintf(char *b, size_t n, const char *fmt, ...);
char *getenv(const char *n);
int usleep(unsigned us);
int pthread_create(unsigned long *t, const void *attr, void *(*fn)(void *), void *arg);
int pthread_join(unsigned long t, void **ret);
long long strtoll(const char *s, char **e, int b);
char *strrchr(const char *s, int c);
char *strstr(const char *h, const char *n);
int atoi(const char *s);
extern FILE_T *__stdoutp;
#define stdout __stdoutp
#endif
long long mnet_now_unix(void);
mnet_client_t *cx_mnet_client(void);

typedef long long curl_off_t;
struct curl_slist { char *data; struct curl_slist *next; };
typedef size_t (*write_cb)(char *, size_t, size_t, void *);
typedef size_t (*read_cb)(char *, size_t, size_t, void *);
typedef int (*xfer_cb)(void *, curl_off_t, curl_off_t, curl_off_t, curl_off_t);
typedef int (*prog_cb)(void *, double, double, double, double);
typedef int (*debug_cb)(void *, int, char *, size_t, void *);

enum {
    CURLE_OK = 0, CURLE_UNSUPPORTED_PROTOCOL = 1, CURLE_FAILED_INIT = 2, CURLE_URL_MALFORMAT = 3, CURLE_NOT_BUILT_IN = 4,
    CURLE_COULDNT_RESOLVE_PROXY = 5, CURLE_COULDNT_RESOLVE_HOST = 6, CURLE_COULDNT_CONNECT = 7, CURLE_WEIRD_SERVER_REPLY = 8,
    CURLE_HTTP2 = 16, CURLE_PARTIAL_FILE = 18, CURLE_HTTP_RETURNED_ERROR = 22, CURLE_WRITE_ERROR = 23,
    CURLE_UPLOAD_FAILED = 25, CURLE_READ_ERROR = 26, CURLE_OUT_OF_MEMORY = 27, CURLE_OPERATION_TIMEDOUT = 28,
    CURLE_RANGE_ERROR = 33, CURLE_SSL_CONNECT_ERROR = 35, CURLE_BAD_DOWNLOAD_RESUME = 36, CURLE_ABORTED_BY_CALLBACK = 42,
    CURLE_BAD_FUNCTION_ARGUMENT = 43, CURLE_TOO_MANY_REDIRECTS = 47, CURLE_UNKNOWN_OPTION = 48, CURLE_GOT_NOTHING = 52,
    CURLE_SEND_ERROR = 55, CURLE_RECV_ERROR = 56, CURLE_PEER_FAILED_VERIFICATION = 60, CURLE_BAD_CONTENT_ENCODING = 61,
    CURLE_LOGIN_DENIED = 67, CURLE_AGAIN = 81,
};
#define CURL_WRITEFUNC_PAUSE 0x10000001
#define CURL_READFUNC_ABORT 0x10000000
#define CURL_READFUNC_PAUSE 0x10000001

struct mime;
typedef struct easy {
    char *url, *post_copy, *agent, *referer, *cookie, *custom, *proxy, *noproxy, *userpwd, *user, *pass, *proxyuserpwd,
         *range, *accept_enc, *cookiejar, *cainfo;
    const char *post;                  /* CURLOPT_POSTFIELDS não copia */
    struct curl_slist *cookiefiles;    /* nomes guardados (cópia) */
    char *errorbuf;
    void *priv;
    long long postsize, infilesize, resume_from, maxfilesize;
    long timeout_ms, connect_ms, maxredirs, httpauth, proxytype, http_version, low_speed_limit, low_speed_time;
    int do_post, nobody, header_in_body, failonerror, upload, follow, verbose, noprogress, content_decoding, autoreferer,
        cookie_engine, cookiesession, unrestricted_auth, forbid_reuse;
    struct curl_slist *headers, *proxyheaders;
    write_cb write_fn; void *write_data;
    write_cb header_fn; void *header_data;
    read_cb read_fn; void *read_data;
    xfer_cb xfer_fn; prog_cb prog_fn; void *prog_data;
    debug_cb debug_fn; void *debug_data;
    struct share *share;
    struct mime *mime;
    mnet_jar_t *jar;                   /* motor de cookies próprio (sem share) */
    /* resultado */
    long status, redirects, header_size, http_ver, os_errno;
    char *eff_url, *content_type, *redirect_url, *scheme;
    long long size_dl, size_ul, cl_dl, cl_ul;
    double t_total, t_start, t_connect;
    mnet_hdrs_t last_hdrs;
    /* andamento */
    volatile int cancel;
    int result, paused, in_multi;
    struct transfer *xfer;
} easy_t;

/* ---------------------------------------------------------------- utilidades */
static char *dupc(const char *s) { return s ? mnet_strndup(s, strlen(s)) : 0; }
static void setp(char **d, const char *s) { free(*d); *d = dupc(s); }
static long long now_ms(void) { mnet_client_t *c = cx_mnet_client(); return c->io->now_ms(c->io->env); }
static void vlog(easy_t *e, const char *fmt, const char *a) {
    if (!e->verbose) return;
    char b[600];
    snprintf(b, sizeof b, fmt, a ? a : "");
    if (e->debug_fn) e->debug_fn(e, 0, b, strlen(b), e->debug_data);
    else fputs(b, stderr);
}
static void glock(void) { mnet_client_t *c = cx_mnet_client(); if (c->io->lock) c->io->lock(c->io->env); }
static void gunlock(void) { mnet_client_t *c = cx_mnet_client(); if (c->io->unlock) c->io->unlock(c->io->env); }

/* ---------------------------------------------------------------- slist, escape, versão, global */
struct curl_slist *curl_slist_append(struct curl_slist *list, const char *data) {
    struct curl_slist *n = calloc(1, sizeof *n);
    if (!n) return 0;
    n->data = dupc(data ? data : "");
    if (!list) return n;
    struct curl_slist *t = list;
    while (t->next) t = t->next;
    t->next = n;
    return list;
}
void curl_slist_free_all(struct curl_slist *l) { while (l) { struct curl_slist *n = l->next; free(l->data); free(l); l = n; } }
static int unreserved(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~'; }
char *curl_easy_escape(void *e, const char *s, int len) {
    (void)e;
    if (!s) return 0;
    size_t n = len > 0 ? (size_t)len : strlen(s);
    mnet_buf_t b = {0};
    static const char hx[] = "0123456789ABCDEF";
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (unreserved(c)) mnet_buf_putc(&b, c);
        else { mnet_buf_putc(&b, '%'); mnet_buf_putc(&b, hx[c >> 4]); mnet_buf_putc(&b, hx[c & 15]); }
    }
    if (!b.p) return dupc("");
    return (char *)b.p;
}
static int hexv(int c) { return c >= '0' && c <= '9' ? c - '0' : (c | 32) >= 'a' && (c | 32) <= 'f' ? (c | 32) - 'a' + 10 : -1; }
char *curl_easy_unescape(void *e, const char *s, int len, int *outlen) {
    (void)e;
    if (!s) return 0;
    size_t n = len > 0 ? (size_t)len : strlen(s);
    char *o = malloc(n + 1);
    if (!o) return 0;
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '%' && i + 2 < n && hexv(s[i + 1]) >= 0 && hexv(s[i + 2]) >= 0) {
            o[k++] = (char)(hexv(s[i + 1]) << 4 | hexv(s[i + 2])); i += 2;
        } else o[k++] = s[i];
    }
    o[k] = 0;
    if (outlen) *outlen = (int)k;
    return o;
}
char *curl_escape(const char *s, int len) { return curl_easy_escape(0, s, len); }
char *curl_unescape(const char *s, int len) { return curl_easy_unescape(0, s, len, 0); }
void curl_free(void *p) { free(p); }
int curl_global_init(long flags) { (void)flags; cx_mnet_client(); return CURLE_OK; }
int curl_global_init_mem(long flags, void *m, void *f, void *r, void *sd, void *c) { (void)m; (void)f; (void)r; (void)sd; (void)c; return curl_global_init(flags); }
void curl_global_cleanup(void) { mnet_client_close_idle(cx_mnet_client()); }
int curl_global_sslset(int id, const char *name, const void ***avail) { (void)name; if (avail) *avail = 0; return id == 0 || id == 13 ? 0 : 1; }   /* só o TLS do sistema */
long long curl_getdate(const char *s, const long long *unused) { (void)unused; long long t = s ? mnet_http_date(s, strlen(s)) : -1; return t; }
static const char VERSION[] = "libcurl/8.11.0 mOS-mnet zlib brotli nghttp2-compatível";
char *curl_version(void) { return (char *)VERSION; }
typedef struct {
    int age; const char *version; unsigned version_num; const char *host; int features; const char *ssl_version; long ssl_version_num;
    const char *libz_version; const char *const *protocols; const char *ares; int ares_num; const char *libidn; int iconv_ver_num;
    const char *libssh_version; unsigned brotli_ver_num; const char *brotli_version; unsigned nghttp2_ver_num; const char *nghttp2_version;
    const char *quic_version; const char *cainfo; const char *capath; unsigned zstd_ver_num; const char *zstd_version;
    const char *hyper_version; const char *gsasl_version; const char *const *feature_names;
} vinfo_t;
static const char *const PROTOS[] = {"http", "https", "ws", "wss", 0};
static const char *const FEATS[] = {"AsynchDNS", "brotli", "HTTP2", "IPv6", "Largefile", "libz", "SSL", "threadsafe", 0};
static vinfo_t VI = {11, "8.11.0", 0x080b00, "x86_64-apple-darwin-mos", (1 << 0) | (1 << 2) | (1 << 3) | (1 << 7) | (1 << 9) | (1 << 16) | (1 << 23) | (1 << 30),
                     "SecureTransport-mOS", 0, "1.3.2", PROTOS, 0, 0, 0, 0, 0, 0x010100, "1.1.0", 0x014400, "1.68.0-mnet", 0, 0, 0, 0, 0, 0, 0, FEATS};
vinfo_t *curl_version_info(int age) { (void)age; return &VI; }
const char *curl_easy_strerror(int c) {
    switch (c) {
    case 0: return "No error";
    case 1: return "Unsupported protocol";
    case 2: return "Failed initialization";
    case 3: return "URL using bad/illegal format or missing URL";
    case 5: return "Could not resolve proxy name";
    case 6: return "Could not resolve hostname";
    case 7: return "Could not connect to server";
    case 8: return "Weird server reply";
    case 16: return "Error in the HTTP2 framing layer";
    case 18: return "Transferred a partial file";
    case 22: return "HTTP response code said error";
    case 23: return "Failed writing received data to disk/application";
    case 25: return "Upload failed";
    case 26: return "Failed to open/read local data from file/application";
    case 27: return "Out of memory";
    case 28: return "Timeout was reached";
    case 33: return "Requested range was not delivered by the server";
    case 35: return "SSL connect error";
    case 36: return "Could not resume download";
    case 42: return "Operation was aborted by an application callback";
    case 43: return "A libcurl function was given a bad argument";
    case 47: return "Number of redirects hit maximum amount";
    case 48: return "An unknown option was passed in to libcurl";
    case 52: return "Server returned nothing (no headers, no data)";
    case 55: return "Failed sending data to the peer";
    case 56: return "Failure when receiving data from the peer";
    case 60: return "SSL peer certificate or SSH remote key was not OK";
    case 61: return "Unrecognized or bad HTTP Content or Transfer-Encoding";
    case 67: return "Login denied";
    case 81: return "Socket not ready for send/recv";
    }
    return "Unknown error";
}

/* ---------------------------------------------------------------- share */
typedef struct share { int cookies; mnet_jar_t jar; int refs; } share_t;
void *curl_share_init(void) { share_t *s = calloc(1, sizeof *s); if (s) mnet_jar_init(&s->jar); return s; }
int curl_share_setopt(void *p, int opt, ...) {
    share_t *s = p;
    va_list ap; va_start(ap, opt);
    int r = 0;
    if (!s) r = 1;
    else if (opt == 1 || opt == 2) { long d = va_arg(ap, long); if (d == 2) s->cookies = opt == 1; }      /* SHARE/UNSHARE + CURL_LOCK_DATA_COOKIE */
    else if (opt == 3 || opt == 4 || opt == 5) (void)va_arg(ap, void *);                                   /* LOCKFUNC, UNLOCKFUNC, USERDATA: a trava é a nossa */
    else r = 2;
    va_end(ap);
    return r;
}
int curl_share_cleanup(void *p) { share_t *s = p; if (!s) return 1; if (s->refs) return 2; mnet_jar_clear(&s->jar); free(s); return 0; }
const char *curl_share_strerror(int c) { return c == 0 ? "No error" : c == 1 ? "Unknown share option" : c == 2 ? "Share currently in use" : "Invalid share handle"; }

/* ---------------------------------------------------------------- MIME (multipart/form-data) */
typedef struct mpart { char *name, *filename, *type, *path; mnet_buf_t data; struct curl_slist *headers; struct mpart *next; } mpart_t;
typedef struct mime { mpart_t *parts; char boundary[72]; } mime_t;
void *curl_mime_init(void *e) {
    (void)e;
    mime_t *m = calloc(1, sizeof *m);
    if (!m) return 0;
    static unsigned ctr = 0x51a7;
    ctr = ctr * 1103515245u + 12345u + (unsigned)(uintptr_t)m;
    snprintf(m->boundary, sizeof m->boundary, "------------------------mOS%08x%08x", ctr, (unsigned)(uintptr_t)m);
    return m;
}
void *curl_mime_addpart(void *mp) {
    mime_t *m = mp;
    if (!m) return 0;
    mpart_t *p = calloc(1, sizeof *p), **t = &m->parts;
    while (*t) t = &(*t)->next;
    *t = p;
    return p;
}
int curl_mime_name(void *pp, const char *n) { mpart_t *p = pp; if (!p) return 43; setp(&p->name, n); return 0; }
int curl_mime_filename(void *pp, const char *n) { mpart_t *p = pp; if (!p) return 43; setp(&p->filename, n); return 0; }
int curl_mime_type(void *pp, const char *t) { mpart_t *p = pp; if (!p) return 43; setp(&p->type, t); return 0; }
int curl_mime_data(void *pp, const char *d, size_t n) {
    mpart_t *p = pp;
    if (!p) return 43;
    p->data.n = 0;
    if (d) mnet_buf_put(&p->data, d, n == (size_t)-1 ? strlen(d) : n);
    return 0;
}
int curl_mime_filedata(void *pp, const char *path) {
    mpart_t *p = pp;
    if (!p || !path) return 43;
    FILE_T *f = fopen(path, "rb");
    if (!f) return CURLE_READ_ERROR;
    p->data.n = 0;
    char b[16384]; size_t k;
    while ((k = fread(b, 1, sizeof b, f)) > 0) mnet_buf_put(&p->data, b, k);
    fclose(f);
    setp(&p->path, path);
    if (!p->filename) { const char *s = strrchr(path, '/'); setp(&p->filename, s ? s + 1 : path); }
    return 0;
}
int curl_mime_headers(void *pp, struct curl_slist *h, int take) { mpart_t *p = pp; if (!p) return 43; p->headers = h; (void)take; return 0; }
int curl_mime_encoder(void *pp, const char *enc) { (void)pp; return enc ? CURLE_NOT_BUILT_IN : 0; }
void curl_mime_free(void *mp) {
    mime_t *m = mp;
    if (!m) return;
    for (mpart_t *p = m->parts, *n; p; p = n) { n = p->next; free(p->name); free(p->filename); free(p->type); free(p->path); mnet_buf_free(&p->data); curl_slist_free_all(p->headers); free(p); }
    free(m);
}
static void mime_body(mime_t *m, mnet_buf_t *o) {
    for (mpart_t *p = m->parts; p; p = p->next) {
        mnet_buf_puts(o, "--"); mnet_buf_puts(o, m->boundary); mnet_buf_puts(o, "\r\nContent-Disposition: form-data");
        if (p->name) { mnet_buf_puts(o, "; name=\""); mnet_buf_puts(o, p->name); mnet_buf_putc(o, '"'); }
        if (p->filename) { mnet_buf_puts(o, "; filename=\""); mnet_buf_puts(o, p->filename); mnet_buf_putc(o, '"'); }
        mnet_buf_puts(o, "\r\n");
        const char *t = p->type ? p->type : p->filename ? "application/octet-stream" : 0;
        if (t) { mnet_buf_puts(o, "Content-Type: "); mnet_buf_puts(o, t); mnet_buf_puts(o, "\r\n"); }
        for (struct curl_slist *h = p->headers; h; h = h->next) { mnet_buf_puts(o, h->data); mnet_buf_puts(o, "\r\n"); }
        mnet_buf_puts(o, "\r\n");
        mnet_buf_put(o, p->data.p, p->data.n);
        mnet_buf_puts(o, "\r\n");
    }
    mnet_buf_puts(o, "--"); mnet_buf_puts(o, m->boundary); mnet_buf_puts(o, "--\r\n");
}

/* ---------------------------------------------------------------- easy: criar, opções */
void *curl_easy_init(void) {
    curl_global_init(0);
    easy_t *e = calloc(1, sizeof *e);
    if (!e) return 0;
    e->postsize = -1; e->infilesize = -1; e->maxredirs = 30; e->httpauth = 1; e->noprogress = 1; e->content_decoding = 1;
    return e;
}
static void cookies_load(easy_t *e);
static void easy_free_opts(easy_t *e) {
    free(e->url); free(e->post_copy); free(e->agent); free(e->referer); free(e->cookie); free(e->custom); free(e->proxy);
    free(e->noproxy); free(e->userpwd); free(e->user); free(e->pass); free(e->proxyuserpwd); free(e->range); free(e->accept_enc);
    free(e->cookiejar); free(e->cainfo);
    curl_slist_free_all(e->cookiefiles);
    free(e->eff_url); free(e->content_type); free(e->redirect_url); free(e->scheme);
    mnet_hdrs_free(&e->last_hdrs);
}
static mnet_jar_t *easy_jar(easy_t *e) {
    if (e->share && e->share->cookies) return &e->share->jar;
    return e->jar;
}
static void cookies_save(easy_t *e) {
    if (!e->cookiejar || !easy_jar(e)) return;
    mnet_buf_t b = {0};
    glock(); mnet_jar_save(easy_jar(e), &b, 1); gunlock();
    FILE_T *f = !strcmp(e->cookiejar, "-") ? 0 : fopen(e->cookiejar, "wb");
    if (f) { fwrite(b.p, 1, b.n, f); fclose(f); }
    else if (!strcmp(e->cookiejar, "-")) fputs((char *)b.p, stderr);
    mnet_buf_free(&b);
}
void curl_easy_cleanup(void *p) {
    easy_t *e = p;
    if (!e) return;
    cookies_save(e);
    if (e->share) e->share->refs--;
    if (e->jar) { mnet_jar_clear(e->jar); free(e->jar); }
    easy_free_opts(e);
    free(e);
}
void curl_easy_reset(void *p) {
    easy_t *e = p;
    if (!e) return;
    mnet_jar_t *jar = e->jar;
    easy_free_opts(e);
    memset(e, 0, sizeof *e);
    e->jar = jar;
    e->postsize = -1; e->infilesize = -1; e->maxredirs = 30; e->httpauth = 1; e->noprogress = 1; e->content_decoding = 1;
}
void *curl_easy_duphandle(void *p) {
    easy_t *s = p, *e = curl_easy_init();
    if (!s || !e) return e;
    memcpy(e, s, sizeof *e);
    char **strs[] = {&e->url, &e->post_copy, &e->agent, &e->referer, &e->cookie, &e->custom, &e->proxy, &e->noproxy, &e->userpwd, &e->user,
                     &e->pass, &e->proxyuserpwd, &e->range, &e->accept_enc, &e->cookiejar, &e->cainfo};
    for (unsigned i = 0; i < sizeof strs / sizeof *strs; i++) *strs[i] = dupc(*strs[i]);
    if (e->post_copy) e->post = e->post_copy;
    e->cookiefiles = 0;
    for (struct curl_slist *c = s->cookiefiles; c; c = c->next) e->cookiefiles = curl_slist_append(e->cookiefiles, c->data);
    e->eff_url = e->content_type = e->redirect_url = e->scheme = 0;
    memset(&e->last_hdrs, 0, sizeof e->last_hdrs);
    e->xfer = 0;
    if (s->jar) {
        e->jar = calloc(1, sizeof *e->jar); mnet_jar_init(e->jar);
        for (mnet_cookie_t *c = s->jar->head; c; c = c->next) mnet_jar_insert(e->jar, c, c->created);
    }
    if (e->share) e->share->refs++;
    return e;
}
int curl_easy_setopt(void *p, int opt, ...) {
    easy_t *e = p;
    if (!e) return CURLE_BAD_FUNCTION_ARGUMENT;
    va_list ap;
    va_start(ap, opt);
    int r = CURLE_OK;
    if (opt >= 40000) { (void)va_arg(ap, void *); va_end(ap); return CURLE_OK; }                 /* blobs: CAINFO_BLOB etc. */
    if (opt >= 30000) {
        curl_off_t v = va_arg(ap, curl_off_t);
        switch (opt) {
        case 30115: e->infilesize = v; break;          /* INFILESIZE_LARGE */
        case 30116: e->resume_from = v; break;         /* RESUME_FROM_LARGE */
        case 30117: e->maxfilesize = v; break;         /* MAXFILESIZE_LARGE */
        case 30120: e->postsize = v; break;            /* POSTFIELDSIZE_LARGE */
        default: break;                                /* limites de velocidade e afins */
        }
        va_end(ap);
        return r;
    }
    if (opt >= 20000) {
        void *f = va_arg(ap, void *);
        switch (opt) {
        case 20011: e->write_fn = (write_cb)f; break;
        case 20012: e->read_fn = (read_cb)f; break;
        case 20079: e->header_fn = (write_cb)f; break;
        case 20219: e->xfer_fn = (xfer_cb)f; break;
        case 20056: e->prog_fn = (prog_cb)f; break;
        case 20094: e->debug_fn = (debug_cb)f; break;
        default: break;                                /* SSL_CTX, sockopt, seek...: aceitos, não usados */
        }
        va_end(ap);
        return r;
    }
    if (opt >= 10000) {
        void *v = va_arg(ap, void *);
        const char *s = v;
        switch (opt) {
        case 10001: e->write_data = v; break;
        case 10002: setp(&e->url, s); break;
        case 10004: setp(&e->proxy, s); break;
        case 10005: setp(&e->userpwd, s); break;
        case 10006: setp(&e->proxyuserpwd, s); break;
        case 10007: setp(&e->range, s); break;
        case 10009: e->read_data = v; break;
        case 10010: e->errorbuf = v; if (e->errorbuf) e->errorbuf[0] = 0; break;
        case 10015: e->post = s; e->do_post = 1; break;
        case 10165: setp(&e->post_copy, s); e->post = e->post_copy; e->do_post = 1; break;   /* COPYPOSTFIELDS */
        case 10016: setp(&e->referer, s); break;
        case 10018: setp(&e->agent, s); break;
        case 10022: setp(&e->cookie, s); break;
        case 10023: e->headers = v; break;
        case 10228: e->proxyheaders = v; break;
        case 10029: e->header_data = v; break;
        case 10031:                                    /* COOKIEFILE: liga o motor; "" só liga */
            if (!e->jar) { e->jar = calloc(1, sizeof *e->jar); mnet_jar_init(e->jar); }
            e->cookie_engine = 1;
            if (s && *s) e->cookiefiles = curl_slist_append(e->cookiefiles, s);
            break;
        case 10082: setp(&e->cookiejar, s); if (!e->jar) { e->jar = calloc(1, sizeof *e->jar); mnet_jar_init(e->jar); } e->cookie_engine = 1; break;
        case 10135:                                    /* COOKIELIST: "ALL", "SESS", "FLUSH", "RELOAD" ou uma linha */
            if (!e->jar) { e->jar = calloc(1, sizeof *e->jar); mnet_jar_init(e->jar); }
            e->cookie_engine = 1;
            if (s && !strcmp(s, "ALL")) { glock(); mnet_jar_clear(easy_jar(e)); gunlock(); }
            else if (s && !strcmp(s, "FLUSH")) cookies_save(e);
            else if (s && !strcmp(s, "RELOAD")) cookies_load(e);
            else if (s && !strcmp(s, "SESS")) {
                glock();
                mnet_jar_t *j = easy_jar(e);
                for (mnet_cookie_t *c = j->head, *n; c; c = n) { n = c->next; if (!c->persistent) mnet_jar_remove(j, c->name, c->domain, c->path); }
                gunlock();
            } else if (s && mnet_ieqn(s, "Set-Cookie:", 11)) {
                mnet_url_t u; int ok = e->url && !mnet_url_parse(e->url, &u);
                glock(); mnet_jar_set(easy_jar(e), s + 11, ok ? u.scheme : "http", ok ? u.host : "", ok ? u.target : "/", 1, mnet_now_unix()); gunlock();
                if (ok) mnet_url_free(&u);
            } else if (s) { glock(); mnet_jar_load(easy_jar(e), s, strlen(s), mnet_now_unix()); gunlock(); }
            break;
        case 10036: setp(&e->custom, s); break;
        case 10057: e->prog_data = v; break;           /* PROGRESSDATA = XFERINFODATA */
        case 10065: setp(&e->cainfo, s); break;        /* as raízes do sistema valem sempre */
        case 10100: if (e->share) e->share->refs--; e->share = v; if (e->share) e->share->refs++; break;
        case 10102: setp(&e->accept_enc, s ? s : 0); if (!s) { free(e->accept_enc); e->accept_enc = 0; } break;
        case 10103: e->priv = v; break;
        case 10173: setp(&e->user, s); break;
        case 10174: setp(&e->pass, s); break;
        case 10177: setp(&e->noproxy, s); break;
        case 10095: e->debug_data = v; break;
        case 10269: e->mime = v; break;                /* MIMEPOST */
        case 10318: case 10319:                        /* PROTOCOLS_STR, REDIR_PROTOCOLS_STR */
            break;
        default: break;
        }
        va_end(ap);
        return r;
    }
    long v = va_arg(ap, long);
    switch (opt) {
    case 13: e->timeout_ms = v * 1000; break;
    case 155: e->timeout_ms = v; break;
    case 78: e->connect_ms = v * 1000; break;
    case 156: e->connect_ms = v; break;
    case 14: e->infilesize = v; break;
    case 21: e->resume_from = v; break;
    case 41: e->verbose = v != 0; break;
    case 42: e->header_in_body = v != 0; break;
    case 43: e->noprogress = v != 0; break;
    case 44: e->nobody = v != 0; break;
    case 45: e->failonerror = v != 0; break;
    case 46: e->upload = v != 0; break;
    case 47: e->do_post = v != 0; break;
    case 52: e->follow = v != 0; break;
    case 54: e->upload = v != 0; break;              /* PUT */
    case 58: e->autoreferer = v != 0; break;
    case 60: e->postsize = v; break;
    case 64: case 81: break;                        /* SSL_VERIFYPEER/HOST: a verificação não desliga */
    case 68: e->maxredirs = v; break;
    case 75: e->forbid_reuse = v != 0; break;
    case 80: if (v) { e->do_post = 0; e->upload = 0; e->nobody = 0; e->post = 0; e->mime = 0; } break;
    case 84: e->http_version = v; break;
    case 96: e->cookiesession = v != 0; break;
    case 101: e->proxytype = v; break;
    case 105: e->unrestricted_auth = v != 0; break;
    case 107: e->httpauth = v; break;
    case 114: e->maxfilesize = v; break;
    case 157: break;                                /* TRANSFER_DECODING: chunked sempre é desfeito */
    case 158: e->content_decoding = v != 0; break;
    case 19: e->low_speed_limit = v; break;
    case 20: e->low_speed_time = v; break;
    default: break;                                 /* NOSIGNAL, TCP_*, BUFFERSIZE, IPRESOLVE...: sem efeito aqui */
    }
    va_end(ap);
    return r;
}
long long mnet_now_unix(void) { mnet_client_t *c = cx_mnet_client(); return c->io->unix_time(c->io->env); }
static void cookies_load(easy_t *e) {
    if (!e->jar && !(e->share && e->share->cookies)) return;
    for (struct curl_slist *c = e->cookiefiles; c; c = c->next) {
        FILE_T *f = !strcmp(c->data, "-") ? 0 : fopen(c->data, "rb");
        if (!f) continue;
        mnet_buf_t b = {0}; char t[8192]; size_t k;
        while ((k = fread(t, 1, sizeof t, f)) > 0) mnet_buf_put(&b, t, k);
        fclose(f);
        if (b.n) {
            /* arquivo Netscape; se for cabeçalhos HTTP crus (Set-Cookie: ...), a libcurl também aceita */
            glock();
            if (strstr((char *)b.p, "Set-Cookie:")) {
                char *ln = (char *)b.p;
                while (ln && *ln) {
                    char *nl = strchr(ln, '\n'); if (nl) *nl = 0;
                    if (mnet_ieqn(ln, "Set-Cookie:", 11)) {
                        mnet_url_t u; int ok = e->url && !mnet_url_parse(e->url, &u);
                        mnet_jar_set(easy_jar(e), ln + 11, ok ? u.scheme : "http", ok ? u.host : "", ok ? u.target : "/", 1, mnet_now_unix());
                        if (ok) mnet_url_free(&u);
                    }
                    ln = nl ? nl + 1 : 0;
                }
            } else mnet_jar_load(easy_jar(e), (char *)b.p, b.n, mnet_now_unix());
            gunlock();
        }
        mnet_buf_free(&b);
    }
    curl_slist_free_all(e->cookiefiles);
    e->cookiefiles = 0;
    if (e->cookiesession) {
        glock();
        mnet_jar_t *j = easy_jar(e);
        for (mnet_cookie_t *c = j->head, *n; c; c = n) { n = c->next; if (!c->persistent) mnet_jar_remove(j, c->name, c->domain, c->path); }
        gunlock();
    }
}

/* ---------------------------------------------------------------- uma transferência */
enum { EV_HEADER, EV_BODY, EV_PROGRESS };
typedef struct ev { int type; mnet_buf_t b; struct ev *next; } ev_t;
typedef struct transfer {
    easy_t *e;
    mnet_buf_t body;                   /* corpo do pedido já pronto (POSTFIELDS, MIME ou lido no multi) */
    size_t body_off;
    long long body_total;
    int streaming;                     /* corpo pelo READFUNCTION, no fio do chamador (só no easy) */
    int auth_retry, auth_count, redirects, too_many, http_err, write_err, cb_abort, got_head, got_any;
    char *auth;                        /* Authorization calculado */
    long long last_prog;
    long long dl_now, ul_now, dl_total, ul_total;
    /* multi */
    int threaded, done;
    ev_t *evh, *evt;
    unsigned long thread;
} transfer_t;

static void push_ev(transfer_t *t, int type, const void *d, size_t n) {
    ev_t *v = calloc(1, sizeof *v);
    if (!v) return;
    v->type = type;
    if (n) mnet_buf_put(&v->b, d, n);
    glock();
    if (t->evt) t->evt->next = v; else t->evh = v;
    t->evt = v;
    gunlock();
}
/* entrega no fio de quem chama: easy direto; multi pelo curl_multi_perform */
static int deliver_header(transfer_t *t, const char *line, size_t n) {
    easy_t *e = t->e;
    e->header_size += (long)n;
    if (t->threaded) { push_ev(t, EV_HEADER, line, n); return 0; }
    if (e->header_fn) { if (e->header_fn((char *)line, 1, n, e->header_data ? e->header_data : e->write_data) != n) { t->write_err = 1; return 1; } }
    if (e->header_in_body && e->write_fn) { if (e->write_fn((char *)line, 1, n, e->write_data) != n) { t->write_err = 1; return 1; } }
    return 0;
}
static const char *reason(int s) {
    switch (s) {
    case 100: return "Continue"; case 101: return "Switching Protocols"; case 200: return "OK"; case 201: return "Created";
    case 202: return "Accepted"; case 204: return "No Content"; case 206: return "Partial Content"; case 301: return "Moved Permanently";
    case 302: return "Found"; case 303: return "See Other"; case 304: return "Not Modified"; case 307: return "Temporary Redirect";
    case 308: return "Permanent Redirect"; case 400: return "Bad Request"; case 401: return "Unauthorized"; case 403: return "Forbidden";
    case 404: return "Not Found"; case 405: return "Method Not Allowed"; case 407: return "Proxy Authentication Required";
    case 409: return "Conflict"; case 410: return "Gone"; case 412: return "Precondition Failed"; case 413: return "Payload Too Large";
    case 416: return "Range Not Satisfiable"; case 429: return "Too Many Requests"; case 500: return "Internal Server Error";
    case 501: return "Not Implemented"; case 502: return "Bad Gateway"; case 503: return "Service Unavailable"; case 504: return "Gateway Timeout";
    }
    return "";
}
static int emit_head(transfer_t *t, const mnet_resp_t *r) {
    char st[64];
    if (r->version == 20) snprintf(st, sizeof st, "HTTP/2 %d \r\n", r->status);
    else snprintf(st, sizeof st, "HTTP/1.%d %d %s\r\n", r->version == 10 ? 0 : 1, r->status, reason(r->status));
    if (deliver_header(t, st, strlen(st))) return 1;
    for (int i = 0; i < r->hdrs.n; i++) {
        mnet_buf_t l = {0};
        mnet_buf_puts(&l, r->hdrs.v[i].name); mnet_buf_put(&l, ": ", 2); mnet_buf_puts(&l, r->hdrs.v[i].value); mnet_buf_put(&l, "\r\n", 2);
        int x = deliver_header(t, (char *)l.p, l.n);
        mnet_buf_free(&l);
        if (x) return 1;
    }
    return deliver_header(t, "\r\n", 2);
}
static int progress(transfer_t *t, int force) {
    easy_t *e = t->e;
    if (e->noprogress || (!e->xfer_fn && !e->prog_fn)) return 0;
    long long n = now_ms();
    if (!force && n - t->last_prog < 100) return 0;
    t->last_prog = n;
    if (t->threaded) { push_ev(t, EV_PROGRESS, 0, 0); return 0; }
    int r = e->xfer_fn ? e->xfer_fn(e->prog_data, t->dl_total, t->dl_now, t->ul_total, t->ul_now)
                       : e->prog_fn(e->prog_data, (double)t->dl_total, (double)t->dl_now, (double)t->ul_total, (double)t->ul_now);
    if (r) { t->cb_abort = 1; return 1; }
    return 0;
}
static int on_resp(void *ctx, const mnet_resp_t *r) {
    transfer_t *t = ctx;
    easy_t *e = t->e;
    t->got_head = 1;
    e->status = r->status;
    e->http_ver = r->version == 20 ? 3 : r->version == 10 ? 1 : 2;   /* CURL_HTTP_VERSION_2_0 = 3, 1_1 = 2, 1_0 = 1 */
    /* 401/407 com Digest pedido: refaz com Authorization */
    if ((r->status == 401 || r->status == 407) && (e->userpwd || e->user) && t->auth_count < 2) {
        int sc; const char *ch = mnet_auth_pick(&r->hdrs, r->status == 401 ? "www-authenticate" : "proxy-authenticate", &sc);
        int allow = (sc >= 2 && (e->httpauth & 2)) || (sc == 1 && (e->httpauth & 1) && !t->auth);
        if (ch && allow) {
            char user[256] = "", pass[256] = "";
            if (e->userpwd) { const char *c = strchr(e->userpwd, ':'); size_t ul = c ? (size_t)(c - e->userpwd) : strlen(e->userpwd); if (ul > 255) ul = 255; memcpy(user, e->userpwd, ul); user[ul] = 0; if (c) snprintf(pass, sizeof pass, "%s", c + 1); }
            if (e->user) snprintf(user, sizeof user, "%s", e->user);
            if (e->pass) snprintf(pass, sizeof pass, "%s", e->pass);
            mnet_url_t u; int ok = !mnet_url_parse(r->url, &u);
            free(t->auth);
            uint8_t cn[16]; long long x = now_ms(); for (int i = 0; i < 16; i++) cn[i] = (uint8_t)(x >> (i % 8 * 8)) ^ (uint8_t)(i * 151 + t->auth_count);
            t->auth = mnet_auth_value(ch, user, pass, e->custom ? e->custom : e->nobody ? "HEAD" : e->upload ? "PUT" : (e->do_post || e->mime) ? "POST" : "GET",
                                      ok ? u.target : "/", t->auth_count + 1, cn);
            if (ok) mnet_url_free(&u);
            if (t->auth) { t->auth_retry = r->status; t->auth_count++; return 1; }
        }
    }
    if (emit_head(t, r)) return 1;
    free(e->content_type);
    const char *ct = mnet_hdrs_get(&r->hdrs, "content-type");
    e->content_type = dupc(ct);
    e->cl_dl = r->expected_len;
    t->dl_total = r->expected_len > 0 ? r->expected_len : 0;
    mnet_hdrs_free(&e->last_hdrs);
    for (int i = 0; i < r->hdrs.n; i++) mnet_hdrs_add(&e->last_hdrs, r->hdrs.v[i].name, strlen(r->hdrs.v[i].name), r->hdrs.v[i].value, strlen(r->hdrs.v[i].value));
    if (e->failonerror && r->status >= 400) { t->http_err = 1; return 1; }
    if (e->maxfilesize > 0 && r->expected_len > e->maxfilesize) { t->http_err = 2; return 1; }
    if (!e->follow) { free(e->redirect_url); const char *l = mnet_hdrs_get(&r->hdrs, "location"); e->redirect_url = l ? mnet_url_resolve(r->url, l) : 0; }
    return 0;
}
static int on_body(void *ctx, const uint8_t *d, size_t n) {
    transfer_t *t = ctx;
    easy_t *e = t->e;
    t->got_any = 1;
    t->dl_now += (long long)n;
    e->size_dl += (long long)n;
    if (e->maxfilesize > 0 && e->size_dl > e->maxfilesize) { t->http_err = 2; return 1; }
    if (t->threaded) push_ev(t, EV_BODY, d, n);
    else if (e->write_fn) {
        size_t w = e->write_fn((char *)d, 1, n, e->write_data);
        if (w != n) { t->write_err = 1; return 1; }
    } else {
        fwrite(d, 1, n, e->write_data ? (FILE_T *)e->write_data : stdout);   /* sem callback: FILE* (padrão stdout) */
    }
    return progress(t, 0);
}
static int on_redir(void *ctx, const mnet_resp_t *r, char **url) {
    transfer_t *t = ctx;
    easy_t *e = t->e;
    if (!e->follow) return 0;
    if (e->maxredirs >= 0 && t->redirects >= e->maxredirs) { t->too_many = 1; return 0; }
    if (emit_head(t, r)) return 0;
    t->redirects++;
    e->redirects = t->redirects;
    /* credenciais não vão para outra origem sem CURLOPT_UNRESTRICTED_AUTH */
    if (!e->unrestricted_auth && t->auth) {
        mnet_url_t a, b; int sa = mnet_url_parse(r->url, &a), sb = mnet_url_parse(*url, &b);
        if (sa || sb || strcmp(a.host, b.host) || a.port != b.port) { free(t->auth); t->auth = 0; }
        if (!sa) mnet_url_free(&a);
        if (!sb) mnet_url_free(&b);
    }
    vlog(e, "* Seguindo o redirect para %s\n", *url);
    return 1;
}
static long body_read(void *ctx, uint8_t *b, size_t n) {
    transfer_t *t = ctx;
    if (!t->streaming) {
        size_t k = t->body.n - t->body_off < n ? t->body.n - t->body_off : n;
        memcpy(b, t->body.p + t->body_off, k);
        t->body_off += k;
        return (long)k;
    }
    size_t k = t->e->read_fn((char *)b, 1, n, t->e->read_data);
    if (k == CURL_READFUNC_ABORT) { t->cb_abort = 1; return -1; }
    if (k == CURL_READFUNC_PAUSE) return -1;
    return (long)k;
}
static int body_rewind(void *ctx) { transfer_t *t = ctx; if (t->streaming) return 1; t->body_off = 0; return 0; }
static void on_sent(void *ctx, long long sent, long long total) {
    transfer_t *t = ctx;
    t->ul_now = sent; t->ul_total = total > 0 ? total : 0;
    t->e->size_ul = sent;
    progress(t, 0);
}
/* proxy: opção, depois o ambiente (como a libcurl) */
static int no_proxy_match(const char *list, const char *host) {
    if (!list) return 0;
    if (!strcmp(list, "*")) return 1;
    const char *p = list;
    while (*p) {
        while (*p == ' ' || *p == ',') p++;
        const char *s = p;
        while (*p && *p != ',' && *p != ' ') p++;
        size_t l = (size_t)(p - s);
        if (!l) continue;
        if (*s == '.') { s++; l--; }
        size_t hl = strlen(host);
        if (hl >= l && mnet_ieqn(host + hl - l, s, l) && (hl == l || host[hl - l - 1] == '.')) return 1;
    }
    return 0;
}
static char *pick_proxy(easy_t *e, const char *url) {
    mnet_url_t u;
    if (mnet_url_parse(url, &u)) return 0;
    const char *px = e->proxy, *np = e->noproxy;
    int https = !strcmp(u.scheme, "https");
    if (!px) {
        px = https ? getenv("https_proxy") : getenv("http_proxy");
        if (!px && https) px = getenv("HTTPS_PROXY");
        if (!px) px = getenv("all_proxy");
        if (!px) px = getenv("ALL_PROXY");
    }
    if (!np) { np = getenv("no_proxy"); if (!np) np = getenv("NO_PROXY"); }
    char *out = 0;
    if (px && *px && !no_proxy_match(np, u.host) && strcmp(u.host, "localhost") && strcmp(u.host, "127.0.0.1")) {
        if (strstr(px, "://")) {
            if (!strncmp(px, "socks5h://", 10) || !strncmp(px, "socks5://", 9) || !strncmp(px, "http://", 7) || !strncmp(px, "https://", 8)) {
                const char *rest = strstr(px, "://") + 3;
                mnet_buf_t b = {0};
                mnet_buf_puts(&b, !strncmp(px, "socks", 5) ? "socks5://" : !strncmp(px, "https", 5) ? "https://" : "http://");
                mnet_buf_puts(&b, rest);
                out = (char *)b.p;
            }
        } else {
            mnet_buf_t b = {0};
            mnet_buf_puts(&b, e->proxytype == 5 || e->proxytype == 7 ? "socks5://" : "http://");   /* CURLPROXY_SOCKS5(_HOSTNAME) */
            mnet_buf_puts(&b, px);
            if (!strchr(px, ':')) mnet_buf_puts(&b, e->proxytype >= 4 ? ":1080" : ":1080");
            out = (char *)b.p;
        }
    }
    mnet_url_free(&u);
    return out;
}
static int map_err(int r) {
    switch (r) {
    case 0: return CURLE_OK;
    case MNET_E_BAD_URL: return CURLE_URL_MALFORMAT;
    case MNET_E_UNSUPPORTED: return CURLE_UNSUPPORTED_PROTOCOL;
    case MNET_E_TIMEOUT: return CURLE_OPERATION_TIMEDOUT;
    case MNET_E_NO_HOST: return CURLE_COULDNT_RESOLVE_HOST;
    case MNET_E_CONNECT: return CURLE_COULDNT_CONNECT;
    case MNET_E_LOST: return CURLE_RECV_ERROR;
    case MNET_E_REDIRECTS: return CURLE_TOO_MANY_REDIRECTS;
    case MNET_E_BAD_RESPONSE: case MNET_E_PARSE: return CURLE_WEIRD_SERVER_REPLY;
    case MNET_E_DECODE: return CURLE_BAD_CONTENT_ENCODING;
    case MNET_E_TLS: return CURLE_SSL_CONNECT_ERROR;
    case MNET_E_CERT_UNTRUSTED: return CURLE_PEER_FAILED_VERIFICATION;
    case MNET_E_OFFLINE: return CURLE_COULDNT_CONNECT;
    }
    return CURLE_RECV_ERROR;
}
static int run(transfer_t *t) {
    easy_t *e = t->e;
    if (!e->url) return CURLE_URL_MALFORMAT;
    if (strncmp(e->url, "http://", 7) && strncmp(e->url, "https://", 8)) {
        if (!strstr(e->url, "://")) {                       /* "exemplo.com/x": a libcurl supõe http */
            mnet_buf_t b = {0}; mnet_buf_puts(&b, "http://"); mnet_buf_puts(&b, e->url); free(e->url); e->url = (char *)b.p;
        } else return CURLE_UNSUPPORTED_PROTOCOL;
    }
    if (e->cookiefiles) cookies_load(e);
    e->status = 0; e->redirects = 0; e->size_dl = e->size_ul = 0; e->header_size = 0; e->cl_dl = -1;
    double t0 = (double)now_ms();
    e->t_start = t0;
    int ret = 0;
    char msg[256] = "";
    for (;;) {
        mnet_hdrs_t h = {0};
        if (e->agent) mnet_hdrs_add(&h, "User-Agent", 10, e->agent, strlen(e->agent));
        if (e->referer) mnet_hdrs_add(&h, "Referer", 7, e->referer, strlen(e->referer));
        if (e->cookie) mnet_hdrs_add(&h, "Cookie", 6, e->cookie, strlen(e->cookie));
        if (e->accept_enc && e->content_decoding) {
            const char *ae = *e->accept_enc ? e->accept_enc : "gzip, deflate, br";
            mnet_hdrs_add(&h, "Accept-Encoding", 15, ae, strlen(ae));
        }
        if (e->range) { mnet_buf_t b = {0}; mnet_buf_puts(&b, "bytes="); mnet_buf_puts(&b, e->range); mnet_hdrs_add(&h, "Range", 5, (char *)b.p, b.n); mnet_buf_free(&b); }
        else if (e->resume_from > 0) { char b[48]; snprintf(b, sizeof b, "bytes=%lld-", e->resume_from); mnet_hdrs_add(&h, "Range", 5, b, strlen(b)); }
        /* Basic sem desafio (o padrão da libcurl), ou o que o desafio pediu */
        if (t->auth) mnet_hdrs_add(&h, t->auth_retry == 407 ? "Proxy-Authorization" : "Authorization", t->auth_retry == 407 ? 19 : 13, t->auth, strlen(t->auth));
        else if ((e->userpwd || e->user) && (e->httpauth & 1) && !(e->httpauth & ~1L & 0xff)) {
            char up[600];
            snprintf(up, sizeof up, "%s", e->userpwd ? e->userpwd : "");
            if (e->user) snprintf(up, sizeof up, "%s:%s", e->user, e->pass ? e->pass : "");
            char *col = strchr(up, ':');
            if (col) *col = 0;
            char *v = mnet_auth_value("Basic", up, col ? col + 1 : "", "GET", "/", 1, 0);
            if (v) { mnet_hdrs_add(&h, "Authorization", 13, v, strlen(v)); free(v); }
        }
        if (e->mime) { char ct[128]; snprintf(ct, sizeof ct, "multipart/form-data; boundary=%s", e->mime->boundary); mnet_hdrs_add(&h, "Content-Type", 12, ct, strlen(ct)); }
        else if ((e->do_post) && !e->upload) mnet_hdrs_add(&h, "Content-Type", 12, "application/x-www-form-urlencoded", 33);
        for (struct curl_slist *s = e->headers; s; s = s->next) {
            const char *d = s->data;
            const char *col = strchr(d, ':'), *semi = strchr(d, ';');
            if (col && (!semi || col < semi)) {
                const char *v = col + 1;
                while (*v == ' ' || *v == '\t') v++;
                char *nm = mnet_strndup(d, (size_t)(col - d));
                if (nm) { mnet_hdrs_del(&h, nm); free(nm); }
                if (*v) mnet_hdrs_add(&h, d, (size_t)(col - d), v, strlen(v));
                else mnet_hdrs_add(&h, d, (size_t)(col - d), "\x7f", 1);       /* "Nome:" tira o cabeçalho */
            } else if (semi && semi[1] == 0) mnet_hdrs_add(&h, d, (size_t)(semi - d), "", 0);   /* "Nome;" manda vazio */
        }
        mnet_req_t q = {0};
        q.url = e->url;
        q.method = e->custom ? e->custom : e->nobody ? "HEAD" : e->upload ? "PUT" : (e->do_post || e->mime) ? "POST" : "GET";
        q.hdrs = &h;
        q.bare_headers = 1;
        q.no_decode = !e->accept_enc || !e->content_decoding;
        if (t->body.p || e->post || e->mime) {
            if (!t->body.p && !t->streaming) {
                if (e->mime) mime_body(e->mime, &t->body);
                else { size_t n = e->postsize >= 0 ? (size_t)e->postsize : strlen(e->post); mnet_buf_put(&t->body, e->post, n); }
                if (!t->body.p) mnet_buf_put(&t->body, "", 0), t->body.p = (uint8_t *)calloc(1, 1);
            }
            q.body_read = body_read; q.body_rewind = body_rewind; q.body_stream_len = (long long)t->body.n;
            t->body_off = 0;
            if (!t->body.n) { q.body_read = 0; q.body = (const uint8_t *)""; q.body_len = 0; }
        } else if (e->upload || (e->do_post && e->read_fn)) {
            if (!t->threaded) { t->streaming = 1; q.body_read = body_read; q.body_rewind = body_rewind; q.body_stream_len = e->upload ? e->infilesize : e->postsize; }
            else { q.body_read = body_read; q.body_rewind = body_rewind; q.body_stream_len = (long long)t->body.n; }
        }
        q.timeout_ms = e->timeout_ms > 0 ? (int)e->timeout_ms : e->connect_ms > 0 ? (int)e->connect_ms : 300000;
        q.total_timeout_ms = e->timeout_ms > 0 ? (int)e->timeout_ms : 0;
        q.max_redirects = e->follow ? 0 : -1;
        q.on_redirect = on_redir;
        q.on_response = on_resp; q.on_body = on_body; q.on_sent = on_sent;
        q.cancel = &e->cancel;
        q.http1_only = e->http_version == 1 || e->http_version == 2;    /* CURL_HTTP_VERSION_1_0/1_1 */
        q.no_reuse = e->forbid_reuse;
        mnet_jar_t *jar = easy_jar(e);
        q.no_cookies = !jar;
        q.jar = jar;
        q.ctx = t;
        char *px = pick_proxy(e, e->url);
        q.proxy = px;
        if (px) vlog(e, "* Usando o proxy %s\n", px);
        if (e->proxyuserpwd && px) {
            if (!strncmp(px, "socks", 5)) q.proxy_auth = e->proxyuserpwd;
            else { char *col = strchr(e->proxyuserpwd, ':'); char u[256]; size_t ul = col ? (size_t)(col - e->proxyuserpwd) : strlen(e->proxyuserpwd); if (ul > 255) ul = 255; memcpy(u, e->proxyuserpwd, ul); u[ul] = 0;
                   char *v = mnet_auth_value("Basic", u, col ? col + 1 : "", "GET", "/", 1, 0); q.proxy_auth = v; }
        }
        vlog(e, "* Pedido para %s\n", e->url);
        t->auth_retry = 0;
        mnet_resp_t r;
        int rr = mnet_fetch(cx_mnet_client(), &q, &r, msg, sizeof msg);
        if (e->proxyuserpwd && px && strncmp(px, "socks", 5)) free((char *)q.proxy_auth);
        free(px);
        free(e->eff_url); e->eff_url = dupc(r.url ? r.url : e->url);
        free(e->scheme); e->scheme = dupc(!strncmp(e->eff_url ? e->eff_url : "", "https", 5) ? "HTTPS" : "HTTP");
        mnet_resp_free(&r);
        mnet_hdrs_free(&h);
        if (rr == MNET_E_CANCELLED && t->auth_retry && !e->cancel) continue;   /* 401/407: de novo com Authorization */
        if (rr == MNET_E_CANCELLED) {
            ret = t->write_err ? CURLE_WRITE_ERROR : t->cb_abort ? CURLE_ABORTED_BY_CALLBACK : t->http_err == 1 ? CURLE_HTTP_RETURNED_ERROR :
                  t->http_err == 2 ? 63 /* CURLE_FILESIZE_EXCEEDED */ : CURLE_ABORTED_BY_CALLBACK;
            if (t->http_err == 1) snprintf(msg, sizeof msg, "The requested URL returned error: %ld", e->status);
        } else if (rr) {
            ret = map_err(rr);
            if (rr == MNET_E_NO_HOST && (e->proxy || getenv("http_proxy") || getenv("https_proxy"))) ret = CURLE_COULDNT_RESOLVE_PROXY;
            if (rr == MNET_E_LOST && !t->got_head) ret = CURLE_GOT_NOTHING;
        } else if (t->too_many) { ret = CURLE_TOO_MANY_REDIRECTS; snprintf(msg, sizeof msg, "Maximum (%ld) redirects followed", e->maxredirs); }
        else if (e->resume_from > 0 && e->status == 200) ret = CURLE_BAD_DOWNLOAD_RESUME;
        break;
    }
    progress(t, 1);
    e->t_total = ((double)now_ms() - t0) / 1000.0;
    if (ret && e->errorbuf) { snprintf(e->errorbuf, 256, "%s", msg[0] ? msg : curl_easy_strerror(ret)); }
    vlog(e, "* Fim: %s\n", ret ? curl_easy_strerror(ret) : "ok");
    free(t->auth); t->auth = 0;
    return ret;
}
int curl_easy_perform(void *p) {
    easy_t *e = p;
    if (!e) return CURLE_BAD_FUNCTION_ARGUMENT;
    if (e->in_multi) return CURLE_FAILED_INIT;          /* CURLE_RECURSIVE_API_CALL na libcurl nova */
    transfer_t t;
    memset(&t, 0, sizeof t);
    t.e = e;
    e->cancel = 0;
    int r = run(&t);
    mnet_buf_free(&t.body);
    e->result = r;
    return r;
}
int curl_easy_pause(void *p, int bits) { easy_t *e = p; if (!e) return 43; e->paused = bits; return 0; }
int curl_easy_upkeep(void *p) { (void)p; return 0; }
int curl_easy_recv(void *p, void *b, size_t n, size_t *got) { (void)p; (void)b; (void)n; if (got) *got = 0; return CURLE_UNSUPPORTED_PROTOCOL; }  /* CONNECT_ONLY */
int curl_easy_send(void *p, const void *b, size_t n, size_t *sent) { (void)p; (void)b; (void)n; if (sent) *sent = 0; return CURLE_UNSUPPORTED_PROTOCOL; }

/* ---------------------------------------------------------------- getinfo */
int curl_easy_getinfo(void *p, int info, ...) {
    easy_t *e = p;
    if (!e) return CURLE_BAD_FUNCTION_ARGUMENT;
    va_list ap;
    va_start(ap, info);
    void *out = va_arg(ap, void *);
    va_end(ap);
    if (!out) return CURLE_BAD_FUNCTION_ARGUMENT;
    int type = info & 0xf00000, id = info & 0xfffff;
    double total = e->t_total;
    switch (type) {
    case 0x100000: {                                    /* string */
        const char *s = 0;
        switch (id) {
        case 1: s = e->eff_url ? e->eff_url : e->url; break;
        case 18: s = e->content_type; break;
        case 21: *(void **)out = e->priv; return 0;     /* PRIVATE */
        case 31: s = e->redirect_url; break;
        case 32: s = 0; break;                          /* PRIMARY_IP: o transporte não expõe o endereço */
        case 49: s = e->scheme; break;
        case 41: s = 0; break;
        default: s = 0;
        }
        *(const char **)out = s;
        return 0;
    }
    case 0x200000: {                                    /* long */
        long v = 0;
        switch (id) {
        case 2: v = e->status; break;
        case 11: v = e->header_size; break;
        case 13: v = 0; break;                          /* SSL_VERIFYRESULT: 0 = ok (o motor recusa o resto) */
        case 14: v = -1; break;                         /* FILETIME */
        case 20: v = e->redirects; break;
        case 22: v = 0; break;
        case 23: v = 1 | 2; break;                      /* HTTPAUTH_AVAIL: Basic e Digest */
        case 25: v = e->os_errno; break;
        case 26: v = e->redirects + 1; break;
        case 46: v = e->http_ver; break;                /* HTTP_VERSION */
        case 48: v = e->scheme && !strcmp(e->scheme, "HTTPS") ? 2 : 1; break;   /* PROTOCOL: CURLPROTO_HTTP(S) */
        case 40: v = 0; break;
        default: v = 0;
        }
        *(long *)out = v;
        return 0;
    }
    case 0x300000: {                                    /* double */
        double v = 0;
        switch (id) {
        case 3: v = total; break;
        case 4: case 5: case 6: case 17: case 33: v = 0; break;
        case 7: v = (double)e->size_ul; break;
        case 8: v = (double)e->size_dl; break;
        case 9: v = total > 0 ? (double)e->size_dl / total : 0; break;
        case 10: v = total > 0 ? (double)e->size_ul / total : 0; break;
        case 15: v = (double)e->cl_dl; break;
        case 16: v = -1; break;
        default: v = 0;
        }
        *(double *)out = v;
        return 0;
    }
    case 0x400000: {                                    /* slist / ponteiro */
        if (id == 28) {                                 /* COOKIELIST */
            struct curl_slist *l = 0;
            mnet_jar_t *j = easy_jar(e);
            if (j) {
                mnet_buf_t b = {0};
                glock(); mnet_jar_save(j, &b, 1); gunlock();
                char *ln = (char *)b.p;
                while (ln && *ln) { char *nl = strchr(ln, '\n'); if (nl) *nl = 0; if (*ln && *ln != '#' ) l = curl_slist_append(l, ln); else if (!strncmp(ln, "#HttpOnly_", 10)) l = curl_slist_append(l, ln); ln = nl ? nl + 1 : 0; }
                mnet_buf_free(&b);
            }
            *(struct curl_slist **)out = l;
            return 0;
        }
        *(void **)out = 0;
        return 0;
    }
    case 0x600000: {                                    /* curl_off_t */
        curl_off_t v = 0;
        switch (id) {
        case 7: v = e->size_ul; break;
        case 8: v = e->size_dl; break;
        case 9: v = total > 0 ? (curl_off_t)(e->size_dl / total) : 0; break;
        case 10: v = total > 0 ? (curl_off_t)(e->size_ul / total) : 0; break;
        case 15: v = e->cl_dl; break;
        case 16: v = -1; break;
        case 50: v = (curl_off_t)(total * 1e6); break;  /* TOTAL_TIME_T (µs) */
        default: v = 0;
        }
        *(curl_off_t *)out = v;
        return 0;
    }
    case 0x500000: *(int *)out = -1; return 0;          /* SOCKET */
    }
    return CURLE_UNKNOWN_OPTION;
}
/* API de cabeçalhos (curl_easy_header, 7.83): a resposta final */
typedef struct { char *name, *value; size_t amount, index; unsigned origin; void *anchor; } curl_header_t;
static curl_header_t g_hdr;
int curl_easy_header(void *p, const char *name, size_t index, unsigned origin, int request, curl_header_t **h) {
    easy_t *e = p;
    (void)origin;
    if (!e || !name || !h) return 1;                    /* CURLHE_BAD_ARGUMENT */
    if (request > 0 && request != -1) return 2;         /* só a última resposta guardada (CURLHE_NOHEADERS para pedidos antigos) */
    size_t amount = 0, k = 0;
    for (int i = 0; i < e->last_hdrs.n; i++) if (mnet_ieq(e->last_hdrs.v[i].name, name)) amount++;
    if (!amount) return e->last_hdrs.n ? 3 : 4;         /* CURLHE_MISSING / CURLHE_NOHEADERS */
    if (index >= amount) return 5;                      /* CURLHE_BADINDEX */
    for (int i = 0; i < e->last_hdrs.n; i++)
        if (mnet_ieq(e->last_hdrs.v[i].name, name) && k++ == index) {
            g_hdr.name = e->last_hdrs.v[i].name; g_hdr.value = e->last_hdrs.v[i].value;
            g_hdr.amount = amount; g_hdr.index = index; g_hdr.origin = 1; g_hdr.anchor = 0;
            *h = &g_hdr;
            return 0;
        }
    return 3;
}
curl_header_t *curl_easy_nextheader(void *p, unsigned origin, int request, curl_header_t *prev) {
    easy_t *e = p;
    (void)origin; (void)request;
    if (!e) return 0;
    int start = 0;
    if (prev) for (int i = 0; i < e->last_hdrs.n; i++) if (e->last_hdrs.v[i].name == prev->name) { start = i + 1; break; }
    if (start >= e->last_hdrs.n) return 0;
    g_hdr.name = e->last_hdrs.v[start].name; g_hdr.value = e->last_hdrs.v[start].value;
    g_hdr.amount = 1; g_hdr.index = 0; g_hdr.origin = 1;
    return &g_hdr;
}

/* ---------------------------------------------------------------- multi */
typedef struct mhandle { easy_t *e; transfer_t *t; int reported; struct mhandle *next; } mhandle_t;
typedef struct { int msg; void *easy; union { void *whatever; int result; } data; } curl_msg_t;
typedef struct {
    mhandle_t *h;
    curl_msg_t msgs[64]; int nmsg, rmsg;
    int (*timer_cb)(void *, long, void *); void *timer_data;
    int (*sock_cb)(void *, int, int, void *, void *); void *sock_data;
    volatile int woken;
} multi_t;
void *curl_multi_init(void) { curl_global_init(0); return calloc(1, sizeof(multi_t)); }
static void *xfer_thread(void *arg) {
    transfer_t *t = arg;
    int r = run(t);
    t->e->result = r;
    __sync_synchronize();
    t->done = 1;
    return 0;
}
int curl_multi_add_handle(void *mp, void *ep) {
    multi_t *m = mp; easy_t *e = ep;
    if (!m || !e) return 1;                             /* CURLM_BAD_HANDLE */
    if (e->in_multi) return 7;                          /* CURLM_ADDED_ALREADY */
    mhandle_t *h = calloc(1, sizeof *h);
    h->e = e;
    h->t = calloc(1, sizeof *h->t);
    h->t->e = e; h->t->threaded = 1;
    e->in_multi = 1; e->cancel = 0; e->xfer = h->t;
    /* corpo por READFUNCTION: lido aqui, no fio do app (os callbacks não podem rodar no fio da transferência) */
    if ((e->upload || (e->do_post && !e->post && !e->mime)) && e->read_fn) {
        char b[16384]; size_t k;
        while ((k = e->read_fn(b, 1, sizeof b, e->read_data)) > 0 && k != CURL_READFUNC_ABORT && k != CURL_READFUNC_PAUSE) mnet_buf_put(&h->t->body, b, k);
        if (!h->t->body.p) h->t->body.p = (uint8_t *)calloc(1, 1);
    }
    h->next = m->h; m->h = h;
    unsigned long th;
#ifdef MNET_HOST
    pthread_create((pthread_t *)&th, 0, xfer_thread, h->t);
#else
    pthread_create(&th, 0, xfer_thread, h->t);
#endif
    h->t->thread = th;
    if (m->timer_cb) m->timer_cb(m, 10, m->timer_data);
    return 0;
}
static void drain(mhandle_t *h) {
    transfer_t *t = h->t;
    easy_t *e = h->e;
    for (;;) {
        if (e->paused & 1) return;                      /* CURLPAUSE_RECV: segura até curl_easy_pause(CONT) */
        glock();
        ev_t *v = t->evh;
        if (v) { t->evh = v->next; if (!t->evh) t->evt = 0; }
        gunlock();
        if (!v) return;
        if (!t->write_err && !t->cb_abort) {
            if (v->type == EV_HEADER) {
                if (e->header_fn && e->header_fn((char *)v->b.p, 1, v->b.n, e->header_data ? e->header_data : e->write_data) != v->b.n) { t->write_err = 1; e->cancel = 1; }
                if (e->header_in_body && e->write_fn) e->write_fn((char *)v->b.p, 1, v->b.n, e->write_data);
            } else if (v->type == EV_BODY) {
                if (e->write_fn) {
                    size_t w = e->write_fn((char *)v->b.p, 1, v->b.n, e->write_data);
                    if (w == CURL_WRITEFUNC_PAUSE) { e->paused |= 1; }   /* reentrega este pedaço depois */
                    else if (w != v->b.n) { t->write_err = 1; e->cancel = 1; }
                    if (w == CURL_WRITEFUNC_PAUSE) {
                        glock(); v->next = t->evh; t->evh = v; if (!t->evt) t->evt = v; gunlock();
                        return;
                    }
                }
                else fwrite(v->b.p, 1, v->b.n, e->write_data ? (FILE_T *)e->write_data : stdout);
            } else if (v->type == EV_PROGRESS && !e->noprogress) {
                int r = e->xfer_fn ? e->xfer_fn(e->prog_data, t->dl_total, t->dl_now, t->ul_total, t->ul_now)
                      : e->prog_fn ? e->prog_fn(e->prog_data, (double)t->dl_total, (double)t->dl_now, (double)t->ul_total, (double)t->ul_now) : 0;
                if (r) { t->cb_abort = 1; e->cancel = 1; }
            }
        }
        mnet_buf_free(&v->b); free(v);
    }
}
int curl_multi_perform(void *mp, int *running) {
    multi_t *m = mp;
    if (!m) return 1;
    int run_n = 0;
    for (mhandle_t *h = m->h; h; h = h->next) {
        drain(h);
        if (!h->t->done) { run_n++; continue; }
        drain(h);
        if (!h->reported && !h->t->evh) {
            h->reported = 1;
            int res = h->e->result;
            if (h->t->write_err) res = CURLE_WRITE_ERROR;
            else if (h->t->cb_abort) res = CURLE_ABORTED_BY_CALLBACK;
            h->e->result = res;
            if (m->nmsg < 64) { curl_msg_t *g = &m->msgs[m->nmsg++]; g->msg = 1; g->easy = h->e; g->data.result = res; }
        } else if (!h->reported) run_n++;
    }
    if (running) *running = run_n;
    if (run_n && m->timer_cb) m->timer_cb(m, 20, m->timer_data);
    return 0;
}
curl_msg_t *curl_multi_info_read(void *mp, int *left) {
    multi_t *m = mp;
    if (!m || m->rmsg >= m->nmsg) { if (left) *left = 0; if (m) { m->nmsg = m->rmsg = 0; } return 0; }
    curl_msg_t *g = &m->msgs[m->rmsg++];
    if (left) *left = m->nmsg - m->rmsg;
    return g;
}
static int pending(multi_t *m) {
    for (mhandle_t *h = m->h; h; h = h->next) if ((h->t->evh && !(h->e->paused & 1)) || (h->t->done && !h->reported)) return 1;
    return 0;
}
int curl_multi_wait(void *mp, void *extra, unsigned n, int timeout_ms, int *numfds) {
    multi_t *m = mp;
    (void)extra; (void)n;
    if (!m) return 1;
    long long end = now_ms() + (timeout_ms > 0 ? timeout_ms : 0);
    int any = 0;
    m->woken = 0;
    while (!(any = pending(m)) && !m->woken && now_ms() < end) usleep(2000);
    if (numfds) *numfds = any ? 1 : 0;
    return 0;
}
int curl_multi_poll(void *mp, void *extra, unsigned n, int timeout_ms, int *numfds) { return curl_multi_wait(mp, extra, n, timeout_ms, numfds); }
int curl_multi_wakeup(void *mp) { multi_t *m = mp; if (!m) return 1; m->woken = 1; return 0; }
int curl_multi_fdset(void *mp, void *r, void *w, void *x, int *maxfd) { (void)mp; (void)r; (void)w; (void)x; if (maxfd) *maxfd = -1; return 0; }  /* sem socket do app: o chamador espera pelo timeout */
int curl_multi_timeout(void *mp, long *ms) { multi_t *m = mp; if (!m || !ms) return 1; *ms = m->h ? (pending(m) ? 0 : 20) : -1; return 0; }
int curl_multi_remove_handle(void *mp, void *ep) {
    multi_t *m = mp; easy_t *e = ep;
    if (!m || !e) return 1;
    for (mhandle_t **pp = &m->h; *pp; pp = &(*pp)->next) {
        mhandle_t *h = *pp;
        if (h->e != e) continue;
        *pp = h->next;
        if (!h->t->done) e->cancel = 1;
#ifdef MNET_HOST
        pthread_join((pthread_t)h->t->thread, 0);
#else
        pthread_join(h->t->thread, 0);
#endif
        for (ev_t *v = h->t->evh, *n; v; v = n) { n = v->next; mnet_buf_free(&v->b); free(v); }
        mnet_buf_free(&h->t->body);
        free(h->t); free(h);
        e->in_multi = 0; e->xfer = 0;
        return 0;
    }
    return 0;
}
int curl_multi_cleanup(void *mp) {
    multi_t *m = mp;
    if (!m) return 1;
    while (m->h) curl_multi_remove_handle(m, m->h->e);
    free(m);
    return 0;
}
int curl_multi_setopt(void *mp, int opt, ...) {
    multi_t *m = mp;
    if (!m) return 1;
    va_list ap; va_start(ap, opt);
    void *v = va_arg(ap, void *);
    va_end(ap);
    switch (opt) {
    case 20001: m->sock_cb = v; break;                   /* SOCKETFUNCTION: nenhum socket é exposto ao app */
    case 10002: m->sock_data = v; break;
    case 20004: m->timer_cb = v; break;                  /* TIMERFUNCTION */
    case 10005: m->timer_data = v; break;
    default: break;                                     /* PIPELINING, MAXCONNECTS, MAX_*_CONNECTIONS */
    }
    return 0;
}
int curl_multi_socket_action(void *mp, int s, int ev, int *running) { (void)s; (void)ev; return curl_multi_perform(mp, running); }
int curl_multi_socket(void *mp, int s, int *running) { (void)s; return curl_multi_perform(mp, running); }
int curl_multi_socket_all(void *mp, int *running) { return curl_multi_perform(mp, running); }
int curl_multi_assign(void *mp, int s, void *p) { (void)mp; (void)s; (void)p; return 0; }
const char *curl_multi_strerror(int c) { return c == 0 ? "No error" : c == 1 ? "Invalid multi handle" : c == 2 ? "Invalid easy handle" : c == 7 ? "The easy handle is already added to a multi handle" : "Unknown error"; }

/* ---------------------------------------------------------------- URL API */
typedef struct { char *scheme, *user, *pass, *options, *host, *port, *path, *query, *frag, *zone; } curlu_t;
void *curl_url(void) { return calloc(1, sizeof(curlu_t)); }
static void curlu_clear(curlu_t *u) { char **p[] = {&u->scheme, &u->user, &u->pass, &u->options, &u->host, &u->port, &u->path, &u->query, &u->frag, &u->zone}; for (int i = 0; i < 10; i++) { free(*p[i]); *p[i] = 0; } }
void curl_url_cleanup(void *p) { if (p) { curlu_clear(p); free(p); } }
void *curl_url_dup(void *p) {
    curlu_t *s = p, *u = curl_url();
    if (!s || !u) return u;
    u->scheme = dupc(s->scheme); u->user = dupc(s->user); u->pass = dupc(s->pass); u->options = dupc(s->options); u->host = dupc(s->host);
    u->port = dupc(s->port); u->path = dupc(s->path); u->query = dupc(s->query); u->frag = dupc(s->frag); u->zone = dupc(s->zone);
    return u;
}
static char *url_join(curlu_t *u, unsigned fl) {
    if (!u->scheme || !u->host) return 0;
    mnet_buf_t b = {0};
    mnet_buf_puts(&b, u->scheme); mnet_buf_puts(&b, "://");
    if (u->user) { mnet_buf_puts(&b, u->user); if (u->pass) { mnet_buf_putc(&b, ':'); mnet_buf_puts(&b, u->pass); } mnet_buf_putc(&b, '@'); }
    int v6 = strchr(u->host, ':') != 0;
    if (v6) mnet_buf_putc(&b, '[');
    mnet_buf_puts(&b, u->host);
    if (v6) mnet_buf_putc(&b, ']');
    int dp = mnet_default_port(u->scheme);
    if (u->port && !((fl & 2) && dp && atoi(u->port) == dp)) { mnet_buf_putc(&b, ':'); mnet_buf_puts(&b, u->port); }
    else if (!u->port && (fl & 1) && dp) { mnet_buf_putc(&b, ':'); mnet_buf_putu(&b, (unsigned)dp); }
    mnet_buf_puts(&b, u->path ? u->path : "/");
    if (u->query) { mnet_buf_putc(&b, '?'); mnet_buf_puts(&b, u->query); }
    if (u->frag) { mnet_buf_putc(&b, '#'); mnet_buf_puts(&b, u->frag); }
    return (char *)b.p;
}
static int url_parse_into(curlu_t *u, const char *s, unsigned fl) {
    char *tmp = 0;
    if (!strstr(s, "://")) {
        if (!(fl & (4 | 512))) return 10;                 /* CURLUE_NO_SCHEME (sem DEFAULT_SCHEME nem GUESS_SCHEME) */
        mnet_buf_t b = {0}; mnet_buf_puts(&b, "https://"); if (fl & 512) { b.n = 0; mnet_buf_puts(&b, "http://"); } mnet_buf_puts(&b, s); tmp = (char *)b.p; s = tmp;
    }
    mnet_url_t m;
    if (mnet_url_parse(s, &m)) { free(tmp); return 3; }
    const char *sch = s; size_t sl = (size_t)(strstr(s, "://") - s);
    if (!(fl & 8) && !mnet_default_port(m.scheme)) { mnet_url_free(&m); free(tmp); return 5; }   /* CURLUE_UNSUPPORTED_SCHEME */
    curlu_clear(u);
    u->scheme = mnet_strndup(sch, sl);
    for (char *q = u->scheme; *q; q++) *q = (char)mnet_lc((unsigned char)*q);
    if (m.userinfo) {
        char *c = strchr(m.userinfo, ':');
        if (c) { u->user = mnet_strndup(m.userinfo, (size_t)(c - m.userinfo)); u->pass = dupc(c + 1); } else u->user = dupc(m.userinfo);
    }
    u->host = dupc(m.host);
    if (m.port_explicit) { char b[8]; snprintf(b, sizeof b, "%d", m.port); u->port = dupc(b); }
    const char *t = m.target, *qm = strchr(t, '?');
    u->path = qm ? mnet_strndup(t, (size_t)(qm - t)) : dupc(t);
    if (qm) u->query = dupc(qm + 1);
    const char *hash = strchr(s, '#');
    if (hash) u->frag = dupc(hash + 1);
    mnet_url_free(&m);
    free(tmp);
    return 0;
}
int curl_url_set(void *p, int part, const char *content, unsigned fl) {
    curlu_t *u = p;
    if (!u) return 1;
    if (part == 0) {
        if (!content) { curlu_clear(u); return 0; }
        if (u->scheme && u->host && !strstr(content, "://")) {     /* relativa: resolve contra a atual */
            char *base = url_join(u, 0), *abs = base ? mnet_url_resolve(base, content) : 0;
            free(base);
            if (!abs) return 3;
            int r = url_parse_into(u, abs, fl | 8);
            free(abs);
            return r;
        }
        return url_parse_into(u, content, fl);
    }
    char **slot = 0;
    switch (part) {
    case 1: slot = &u->scheme; if (content && !(fl & 8) && !mnet_default_port(content)) return 5; break;
    case 2: slot = &u->user; break;
    case 3: slot = &u->pass; break;
    case 4: slot = &u->options; break;
    case 5: slot = &u->host; break;
    case 6:
        if (content) { for (const char *c = content; *c; c++) if (*c < '0' || *c > '9') return 4; if (atoi(content) > 65535 || !*content) return 4; }
        slot = &u->port; break;
    case 7: slot = &u->path; break;
    case 8:
        if (content && (fl & 256) && u->query) {                 /* APPENDQUERY */
            mnet_buf_t b = {0}; mnet_buf_puts(&b, u->query); mnet_buf_putc(&b, '&');
            char *enc = (fl & 128) ? curl_easy_escape(0, content, 0) : dupc(content);
            if (fl & 128) { char *eq = strchr(content, '='); if (eq) { free(enc); char *k = curl_easy_escape(0, content, (int)(eq - content)), *v = curl_easy_escape(0, eq + 1, 0); mnet_buf_puts(&b, k); mnet_buf_putc(&b, '='); mnet_buf_puts(&b, v); free(k); free(v); enc = 0; } }
            if (enc) { mnet_buf_puts(&b, enc); free(enc); }
            free(u->query); u->query = (char *)b.p;
            return 0;
        }
        slot = &u->query; break;
    case 9: slot = &u->frag; break;
    case 10: slot = &u->zone; break;
    default: return 9;
    }
    if (content && (fl & 128) && part != 1 && part != 6) { char *e = curl_easy_escape(0, content, 0); free(*slot); *slot = e; }
    else setp(slot, content);
    if (part == 7 && u->path && u->path[0] != '/') { mnet_buf_t b = {0}; mnet_buf_putc(&b, '/'); mnet_buf_puts(&b, u->path); free(u->path); u->path = (char *)b.p; }
    return 0;
}
int curl_url_get(void *p, int part, char **out, unsigned fl) {
    curlu_t *u = p;
    if (!u || !out) return 1;
    *out = 0;
    if (part == 0) { *out = url_join(u, fl); return *out ? 0 : u->host ? 10 : 14; }
    char *v = 0; int miss = 0;
    switch (part) {
    case 1: v = u->scheme; miss = 10; break;
    case 2: v = u->user; miss = 11; break;
    case 3: v = u->pass; miss = 12; break;
    case 4: v = u->options; miss = 13; break;
    case 5: v = u->host; miss = 14; break;
    case 6:
        if (!u->port && (fl & 1) && u->scheme && mnet_default_port(u->scheme)) { char b[8]; snprintf(b, sizeof b, "%d", mnet_default_port(u->scheme)); *out = dupc(b); return 0; }
        v = u->port; miss = 15; break;
    case 7: *out = dupc(u->path ? u->path : "/"); return 0;
    case 8: v = u->query; miss = 16; break;
    case 9: v = u->frag; miss = 17; break;
    case 10: v = u->zone; miss = 18; break;
    default: return 9;
    }
    if (!v) return miss;
    if (fl & 64) { int ol; *out = curl_easy_unescape(0, v, 0, &ol); }     /* URLDECODE */
    else *out = dupc(v);
    return *out ? 0 : 7;
}
const char *curl_url_strerror(int c) {
    static const char *t[] = {"No error", "An invalid CURLU pointer was passed as argument", "An invalid 'part' argument was passed as argument",
                              "Malformed input to a URL function", "Port number was not a decimal number between 0 and 65535", "Unsupported URL scheme",
                              "URL decode error, most likely because of rubbish in the input", "A memory function failed", "Credentials was passed in the URL when prohibited",
                              "An unknown part ID was passed to a URL API function", "No scheme part in the URL", "No user part in the URL", "No password part in the URL",
                              "No options part in the URL", "No host part in the URL", "No port part in the URL", "No query part in the URL", "No fragment part in the URL",
                              "No zoneid part in the URL"};
    return c >= 0 && c < (int)(sizeof t / sizeof *t) ? t[c] : "CURLUcode unknown";
}
