/*
 * libcurl.4.dylib: easy e URL parser.
 * HTTP por getaddrinfo + socket. HTTPS pelo SecureTransport (SSLHandshake),
 * que verifica o certificado no kernel. CURLOPT_SSL_VERIFYPEER e
 * SSL_VERIFYHOST em 0 são ignorados: a verificação não desliga.
 * Com -DCURL_HOST o HTTPS usa kernel/net/tls.c (o mesmo motor) via tcp_*.
 */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef CURL_HOST
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
typedef struct tls_conn tls_conn_t;
tls_conn_t *tls_new(int tcp_sock, const char *sni);
int tls_handshake(tls_conn_t *c);
int tls_send(tls_conn_t *c, const void *data, int len);
int tls_recv(tls_conn_t *c, void *buf, int max);
void tls_free(tls_conn_t *c);
int curl_host_tcp(const char *host, int port);
void curl_host_drop(int slot);
int curl_host_send(int slot, const void *b, int n);
int curl_host_recv(int slot, void *b, int n);
#else
int getaddrinfo(const char *, const char *, const void *, void **);
void freeaddrinfo(void *);
int socket(int, int, int);
int connect(int, const void *, unsigned);
long send(int, const void *, unsigned long, int);
long recv(int, void *, unsigned long, int);
int close(int);
int fcntl(int, int, ...);
int usleep(unsigned);
int clock_gettime(int, void *);
int snprintf(char *d, unsigned long n, const char *fmt, ...);
int *__error(void);
#define errno (*__error())
typedef int OSStatus;
OSStatus SSLNewContext(int, void **);
OSStatus SSLDisposeContext(void *);
OSStatus SSLSetIOFuncs(void *, void *, void *);
OSStatus SSLSetConnection(void *, const void *);
OSStatus SSLSetPeerDomainName(void *, const char *, unsigned long);
OSStatus SSLHandshake(void *);
OSStatus SSLWrite(void *, const void *, unsigned long, unsigned long *);
OSStatus SSLRead(void *, void *, unsigned long, unsigned long *);
struct mos_ai { int flags, family, socktype, protocol; unsigned addrlen; char *canon; void *addr; void *next; };
#endif

enum {
    CURLE_OK = 0, CURLE_UNSUPPORTED_PROTOCOL = 1, CURLE_URL_MALFORMAT = 3,
    CURLE_COULDNT_RESOLVE_HOST = 6, CURLE_COULDNT_CONNECT = 7, CURLE_WRITE_ERROR = 23,
    CURLE_OPERATION_TIMEDOUT = 28, CURLE_SSL_CONNECT_ERROR = 35, CURLE_TOO_MANY_REDIRECTS = 47
};
enum {
    OPT_TIMEOUT = 13, OPT_POST = 47, OPT_FOLLOW = 52, OPT_POSTSIZE = 60,
    OPT_SSL_PEER = 64, OPT_HTTPGET = 80, OPT_SSL_HOST = 81, OPT_TIMEOUT_MS = 155,
    OPT_WRITEDATA = 10001, OPT_URL = 10002, OPT_POSTFIELDS = 10015, OPT_USERAGENT = 10018,
    OPT_HTTPHEADER = 10023, OPT_CUSTOM = 10036, OPT_WRITEFUNCTION = 20011
};

struct curl_slist { char *data; struct curl_slist *next; };
typedef unsigned long (*curl_write)(char *, unsigned long, unsigned long, void *);
struct easy {
    char *url, *post, *agent, *custom;
    long post_size, timeout_ms;
    int do_post, follow;
    struct curl_slist *headers;
    curl_write write_fn;
    void *write_data;
};
struct url { char *scheme, *user, *pass, *host, *port, *path, *query, *frag; };

static char *dupn(const char *s, int n) {
    char *p;
    if (!s) return 0;
    if (n < 0) n = (int)strlen(s);
    p = malloc((size_t)n + 1);
    if (!p) return 0;
    memcpy(p, s, (size_t)n);
    p[n] = 0;
    return p;
}
static void setp(char **d, const char *s) { free(*d); *d = dupn(s, -1); }

static long long now_ms(void) {
#ifdef CURL_HOST
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#else
    struct { long long sec, nsec; } ts;
    if (clock_gettime(6, &ts)) return 0;
    return ts.sec * 1000 + ts.nsec / 1000000;
#endif
}

static int i_port(int port, char *b, int cap) {
    char tmp[8];
    int k = 0, x = port;
    if (port < 0 || cap < 2) return -1;
    if (!x) tmp[k++] = '0';
    while (x && k < 7) { tmp[k++] = (char)('0' + x % 10); x /= 10; }
    if (x || k >= cap) return -1;
    for (x = 0; x < k; x++) b[x] = tmp[k - 1 - x];
    b[k] = 0;
    return 0;
}

static int split_url(const char *url, char *scheme, char *host, int *port, const char **path) {
    const char *p, *sl, *colon;
    if (url && !strncmp(url, "http://", 7)) { p = url + 7; memcpy(scheme, "http", 5); *port = 80; }
    else if (url && !strncmp(url, "https://", 8)) { p = url + 8; memcpy(scheme, "https", 6); *port = 443; }
    else return 0;
    if (!*p) return 0;
    sl = strchr(p, '/');
    colon = strchr(p, ':');
    if (colon && (!sl || colon < sl)) {
        int n = (int)(colon - p), v = 0, i;
        if (n <= 0 || n >= 255) return 0;
        memcpy(host, p, (size_t)n);
        host[n] = 0;
        for (i = 1; colon[i] && colon[i] != '/'; i++) {
            if (colon[i] < '0' || colon[i] > '9') return 0;
            v = v * 10 + colon[i] - '0';
        }
        *port = v;
        *path = colon[i] == '/' ? colon + i : "/";
    } else {
        int n = sl ? (int)(sl - p) : (int)strlen(p);
        if (n <= 0 || n >= 255) return 0;
        memcpy(host, p, (size_t)n);
        host[n] = 0;
        *path = sl ? sl : "/";
    }
    return 1;
}

#ifdef CURL_HOST
static int tcp_open(const char *host, int port) { return curl_host_tcp(host, port); }
static void tcp_close(int fd) { curl_host_drop(fd); }
#else
static int tls_read(const void *conn, void *data, unsigned long *len) {
    long n = recv(*(const int *)conn, data, *len, 0);
    if (n < 0) return -9803;
    if (n == 0) return -9805;
    *len = (unsigned long)n;
    return 0;
}
static int tls_write(const void *conn, const void *data, unsigned long *len) {
    long n = send(*(const int *)conn, data, *len, 0);
    if (n <= 0) return -36;
    *len = (unsigned long)n;
    return 0;
}
static int tcp_open(const char *host, int port) {
    char portb[8];
    struct mos_ai hint, *res = 0;
    int fd;
    if (i_port(port, portb, (int)sizeof portb)) return -1;
    memset(&hint, 0, sizeof hint);
    hint.family = 2;
    hint.socktype = 1;
    if (getaddrinfo(host, portb, &hint, (void **)&res) || !res) return -2;
    fd = socket(2, 1, 6);
    if (fd < 0 || connect(fd, res->addr, res->addrlen)) {
        if (fd >= 0) close(fd);
        freeaddrinfo(res);
        return -1;
    }
    freeaddrinfo(res);
    fcntl(fd, 4, 4);
    return fd;
}
static void tcp_close(int fd) { if (fd >= 0) close(fd); }
#endif

static int io_write_raw(void *tls, int fd, const void *b, unsigned long n, long long deadline) {
    unsigned long off = 0;
    while (off < n) {
        if (deadline && now_ms() > deadline) return -2;
#ifdef CURL_HOST
        if (tls) {
            int k = tls_send(tls, (const char *)b + off, (int)(n - off));
            if (k < 0) return -1;
            if (!k) { usleep(1000); continue; }
            off += (unsigned long)k;
        } else {
            int k = curl_host_send(fd, (const char *)b + off, (int)(n - off));
            if (k < 0) return -1;
            if (!k) { usleep(1000); continue; }
            off += (unsigned long)k;
        }
#else
        if (tls) {
            unsigned long d = 0;
            if (SSLWrite(tls, (const char *)b + off, n - off, &d)) return -1;
            off += d;
        } else {
            long k = send(fd, (const char *)b + off, n - off, 0);
            if (k <= 0) return -1;
            off += (unsigned long)k;
        }
#endif
    }
    return 0;
}

static int io_read_raw(void *tls, int fd, void *b, int n, long long deadline) {
    for (;;) {
        if (deadline && now_ms() > deadline) return -2;
#ifdef CURL_HOST
        if (tls) {
            int k = tls_recv(tls, b, n);
            if (k > 0) return k;
            if (k < 0) return 0;
        } else {
            int k = curl_host_recv(fd, b, n);
            if (k > 0) return k;
            if (k == 0) return 0;
            if (k != -2) return -1;
        }
        usleep(1000);
#else
        if (tls) {
            unsigned long d = 0;
            OSStatus st = SSLRead(tls, b, (unsigned long)n, &d);
            if (d) return (int)d;
            if (st == -9805 || st == -9816) return 0;
            if (st && st != -9803) return -1;
        } else {
            long k = recv(fd, b, (unsigned long)n, 0x80);
            if (k > 0) return (int)k;
            if (k == 0) return 0;
            if (errno != 35) return -1;
        }
        usleep(2000);
#endif
    }
}

static int hdr_is(const char *line, const char *name) {
    unsigned long i;
    for (i = 0; name[i]; i++) {
        char a = line[i], b = name[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b) return 0;
    }
    return line[i] == ':';
}

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int read_more(unsigned char **buf, unsigned long *n, void *tls, int fd, long long deadline) {
    unsigned char tmp[2048];
    int k = io_read_raw(tls, fd, tmp, (int)sizeof tmp, deadline);
    unsigned char *nb;
    if (k == -2) return -2;
    if (k <= 0) return 0;
    nb = realloc(*buf, *n + (unsigned)k + 1);
    if (!nb) return -1;
    *buf = nb;
    memcpy(*buf + *n, tmp, (unsigned)k);
    *n += (unsigned)k;
    (*buf)[*n] = 0;
    return k;
}

static int take_body(unsigned char *raw, unsigned long raw_n, unsigned long hdr, int chunked, long clen, char **body, unsigned long *body_n) {
    unsigned long i = hdr, outn = 0;
    char *out = 0;
    if (chunked) {
        int finished = 0;
        while (i < raw_n && !finished) {
            unsigned long size = 0, j;
            int hv;
            char *nl;
            char *grown;
            while (i < raw_n && (raw[i] == '\r' || raw[i] == '\n')) i++;
            if (i >= raw_n) break;
            j = i;
            while (j < raw_n && raw[j] != '\n' && raw[j] != ';') {
                hv = hexval((char)raw[j]);
                if (hv < 0) break;
                size = (size << 4) + (unsigned)hv;
                j++;
            }
            nl = memchr(raw + i, '\n', raw_n - i);
            if (!nl) { free(out); return -1; }
            i = (unsigned long)(nl - (char *)raw) + 1;
            if (size == 0) { finished = 1; break; }
            if (i + size > raw_n) { free(out); return -1; }
            grown = realloc(out, outn + size + 1);
            if (!grown) { free(out); return -1; }
            out = grown;
            memcpy(out + outn, raw + i, size);
            outn += size;
            out[outn] = 0;
            i += size;
            if (i < raw_n && raw[i] == '\r') i++;
            if (i < raw_n && raw[i] == '\n') i++;
        }
        if (!finished) { free(out); return -1; }
    } else if (clen >= 0) {
        if (hdr + (unsigned long)clen > raw_n) return -1;
        out = dupn((char *)raw + hdr, (int)clen);
        outn = (unsigned long)clen;
    } else {
        outn = raw_n > hdr ? raw_n - hdr : 0;
        out = dupn((char *)raw + hdr, (int)outn);
    }
    *body = out;
    *body_n = outn;
    return 0;
}

static int append(char *d, unsigned long cap, const char *s) {
    unsigned long n = strlen(d), m = strlen(s);
    if (n + m + 1 > cap) return -1;
    memcpy(d + n, s, m + 1);
    return 0;
}

static int http_once(struct easy *e, const char *url, long long deadline, int *status, char **loc, char **body, unsigned long *body_n) {
    char scheme[8], host[256], head[4096], line[512];
    const char *path = "/", *method;
    int port = 0, fd, https, rc = 0;
    void *tls = 0;
    unsigned char *buf = 0;
    unsigned long n = 0, cap = 0, blen = 0;
    *status = 0; *loc = 0; *body = 0; *body_n = 0;
    if (!split_url(url, scheme, host, &port, &path)) return CURLE_URL_MALFORMAT;
    https = scheme[4] == 's';
    fd = tcp_open(host, port);
    if (fd == -2) return CURLE_COULDNT_RESOLVE_HOST;
    if (fd < 0) return CURLE_COULDNT_CONNECT;
    if (https) {
#ifdef CURL_HOST
        tls_conn_t *c = tls_new(fd, host);
        long long t0 = now_ms();
        int hr;
        if (!c) { tcp_close(fd); return CURLE_SSL_CONNECT_ERROR; }
        while ((hr = tls_handshake(c)) == 0) {
            if (deadline && now_ms() > deadline) { tls_free(c); tcp_close(fd); return CURLE_OPERATION_TIMEDOUT; }
            if (now_ms() - t0 > 8000) { tls_free(c); tcp_close(fd); return CURLE_SSL_CONNECT_ERROR; }
            usleep(1000);
        }
        if (hr != 1) { tls_free(c); tcp_close(fd); return CURLE_SSL_CONNECT_ERROR; }
        tls = c;
#else
        if (SSLNewContext(0, &tls) || !tls) { tcp_close(fd); return CURLE_SSL_CONNECT_ERROR; }
        SSLSetIOFuncs(tls, (void *)tls_read, (void *)tls_write);
        SSLSetConnection(tls, &fd);
        SSLSetPeerDomainName(tls, host, strlen(host));
        if (SSLHandshake(tls)) { SSLDisposeContext(tls); tcp_close(fd); return CURLE_SSL_CONNECT_ERROR; }
#endif
    }
    method = e->custom ? e->custom : (e->do_post || e->post ? "POST" : "GET");
    if (e->post) blen = e->post_size >= 0 ? (unsigned long)e->post_size : strlen(e->post);
    snprintf(head, sizeof head, "%s %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n", method, path, host);
    if (e->agent) { snprintf(line, sizeof line, "User-Agent: %s\r\n", e->agent); if (append(head, sizeof head, line)) rc = -1; }
    {
        struct curl_slist *h;
        for (h = e->headers; h; h = h->next) if (h->data) {
            if (append(head, sizeof head, h->data) || append(head, sizeof head, "\r\n")) { rc = -1; break; }
        }
    }
    if (blen) { snprintf(line, sizeof line, "Content-Length: %lu\r\n", blen); append(head, sizeof head, line); }
    append(head, sizeof head, "\r\n");
    rc = io_write_raw(tls, fd, head, strlen(head), deadline);
    if (!rc && blen) rc = io_write_raw(tls, fd, e->post, blen, deadline);
    if (rc) {
#ifdef CURL_HOST
        if (tls) tls_free(tls);
#else
        if (tls) SSLDisposeContext(tls);
#endif
        tcp_close(fd);
        return rc == -2 ? CURLE_OPERATION_TIMEDOUT : CURLE_COULDNT_CONNECT;
    }
    {
        int chunked = 0;
        long clen = -1;
        unsigned long hdr = 0;
        int saw = 0;
        for (;;) {
            unsigned long i;
            int k;
            for (i = 0; i + 3 < n; i++) if (buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' && buf[i + 3] == '\n') {
                hdr = i + 4; saw = 1; break;
            }
            if (saw) break;
            k = read_more(&buf, &n, tls, fd, deadline);
            if (k == -2) { rc = CURLE_OPERATION_TIMEDOUT; goto done; }
            if (k < 0) { rc = CURLE_WRITE_ERROR; goto done; }
            if (k == 0) break;
            if (n > 1024 * 1024) break;
        }
        if (buf && n >= 12 && !memcmp(buf, "HTTP/", 5)) {
            char *sp = memchr(buf, ' ', n < hdr ? n : hdr);
            char *linep;
            if (sp) {
                int v = 0;
                char *d = sp + 1;
                while (*d >= '0' && *d <= '9') { v = v * 10 + *d - '0'; d++; }
                *status = v;
            }
            linep = (char *)buf;
            if (saw) buf[hdr - 4] = 0;
            while (linep && *linep) {
                char *nl = strchr(linep, '\n');
                char *v;
                if (nl) *nl = 0;
                if (hdr_is(linep, "location")) {
                    v = strchr(linep, ':');
                    if (v) {
                        char *end;
                        v++;
                        while (*v == ' ' || *v == '\t') v++;
                        end = v + strlen(v);
                        while (end > v && (end[-1] == '\r' || end[-1] == ' ')) end--;
                        *loc = dupn(v, (int)(end - v));
                    }
                } else if (hdr_is(linep, "content-length")) {
                    v = strchr(linep, ':');
                    clen = 0;
                    if (v) { v++; while (*v == ' ') v++; while (*v >= '0' && *v <= '9') { clen = clen * 10 + *v - '0'; v++; } }
                } else if (hdr_is(linep, "transfer-encoding")) {
                    v = strchr(linep, ':');
                    if (v && strstr(v, "chunked")) chunked = 1;
                }
                if (!nl) break;
                linep = nl + 1;
            }
            if (saw) buf[hdr - 4] = '\r';
        }
        if (chunked) {
            for (;;) {
                if (take_body(buf, n, hdr, 1, -1, body, body_n) == 0) break;
                free(*body); *body = 0;
                {
                    int k = read_more(&buf, &n, tls, fd, deadline);
                    if (k == -2) { rc = CURLE_OPERATION_TIMEDOUT; goto done; }
                    if (k <= 0) { rc = CURLE_WRITE_ERROR; goto done; }
                }
            }
        } else if (clen >= 0) {
            while (n < hdr + (unsigned long)clen) {
                int k = read_more(&buf, &n, tls, fd, deadline);
                if (k == -2) { rc = CURLE_OPERATION_TIMEDOUT; goto done; }
                if (k <= 0) { rc = CURLE_WRITE_ERROR; goto done; }
            }
            if (take_body(buf, n, hdr, 0, clen, body, body_n)) { rc = CURLE_WRITE_ERROR; goto done; }
        } else if (take_body(buf, n, hdr, 0, -1, body, body_n)) {
            rc = CURLE_WRITE_ERROR;
        }
    }
done:
#ifdef CURL_HOST
    if (tls) tls_free(tls);
#else
    if (tls) SSLDisposeContext(tls);
#endif
    tcp_close(fd);
    free(buf);
    (void)cap;
    return rc;
}

static char *join_loc(const char *base, const char *loc) {
    char scheme[8], host[256], out[1200];
    const char *path = "/";
    int port = 0;
    if (!strncmp(loc, "http://", 7) || !strncmp(loc, "https://", 8)) return dupn(loc, -1);
    if (!split_url(base, scheme, host, &port, &path)) return dupn(loc, -1);
    if (loc[0] == '/') snprintf(out, sizeof out, "%s://%s:%d%s", scheme, host, port, loc);
    else snprintf(out, sizeof out, "%s://%s:%d/%s", scheme, host, port, loc);
    return dupn(out, -1);
}

void *curl_easy_init(void) {
    struct easy *e = calloc(1, sizeof *e);
    if (e) e->post_size = -1;
    return e;
}
void curl_easy_cleanup(void *p) {
    struct easy *e = p;
    if (!e) return;
    free(e->url); free(e->post); free(e->agent); free(e->custom);
    free(e);
}
int curl_easy_setopt(void *p, int opt, ...) {
    struct easy *e = p;
    va_list ap;
    if (!e) return CURLE_URL_MALFORMAT;
    va_start(ap, opt);
    switch (opt) {
    case OPT_URL: setp(&e->url, va_arg(ap, const char *)); break;
    case OPT_POSTFIELDS: setp(&e->post, va_arg(ap, const char *)); e->do_post = 1; break;
    case OPT_POSTSIZE: e->post_size = va_arg(ap, long); break;
    case OPT_POST: e->do_post = va_arg(ap, long) != 0; break;
    case OPT_HTTPGET:
        if (va_arg(ap, long)) { e->do_post = 0; free(e->post); e->post = 0; }
        break;
    case OPT_FOLLOW: e->follow = va_arg(ap, long) != 0; break;
    case OPT_TIMEOUT: e->timeout_ms = va_arg(ap, long) * 1000; break;
    case OPT_TIMEOUT_MS: e->timeout_ms = va_arg(ap, long); break;
    case OPT_USERAGENT: setp(&e->agent, va_arg(ap, const char *)); break;
    case OPT_CUSTOM: setp(&e->custom, va_arg(ap, const char *)); break;
    case OPT_HTTPHEADER: e->headers = va_arg(ap, struct curl_slist *); break;
    case OPT_WRITEFUNCTION: e->write_fn = va_arg(ap, curl_write); break;
    case OPT_WRITEDATA: e->write_data = va_arg(ap, void *); break;
    case OPT_SSL_PEER:
    case OPT_SSL_HOST:
        (void)va_arg(ap, long);
        break;
    default:
        if (opt >= 30000) (void)va_arg(ap, long long);
        else if (opt >= 10000) (void)va_arg(ap, void *);
        else (void)va_arg(ap, long);
        break;
    }
    va_end(ap);
    return CURLE_OK;
}

int curl_easy_perform(void *p) {
    struct easy *e = p;
    char *url;
    long long deadline = 0;
    int hop;
    if (!e || !e->url) return CURLE_URL_MALFORMAT;
    if (strncmp(e->url, "http://", 7) && strncmp(e->url, "https://", 8)) return CURLE_UNSUPPORTED_PROTOCOL;
    if (e->timeout_ms > 0) deadline = now_ms() + e->timeout_ms;
    url = dupn(e->url, -1);
    if (!url) return CURLE_WRITE_ERROR;
    for (hop = 0; hop < 8; hop++) {
        int status = 0, rc;
        char *loc = 0, *body = 0, *next;
        unsigned long bn = 0, w;
        rc = http_once(e, url, deadline, &status, &loc, &body, &bn);
        if (rc) { free(url); free(loc); free(body); return rc; }
        if (e->follow && status >= 300 && status < 400 && loc) {
            next = join_loc(url, loc);
            free(loc); free(body); free(url);
            url = next;
            if (!url) return CURLE_WRITE_ERROR;
            if (status == 303) { e->do_post = 0; free(e->post); e->post = 0; }
            continue;
        }
        free(loc);
        if (e->write_fn) {
            w = body ? e->write_fn(body, 1, bn, e->write_data) : e->write_fn("", 1, 0, e->write_data);
            free(body); free(url);
            return w == bn ? CURLE_OK : CURLE_WRITE_ERROR;
        }
        free(body); free(url);
        return CURLE_OK;
    }
    free(url);
    return CURLE_TOO_MANY_REDIRECTS;
}

char *curl_easy_escape(void *e, const char *s, int len) {
    static const char hex[] = "0123456789ABCDEF";
    char *o;
    int i, n = 0;
    (void)e;
    if (!s) return 0;
    if (len < 0) len = (int)strlen(s);
    o = malloc((size_t)len * 3 + 1);
    if (!o) return 0;
    for (i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '.' || c == '_' || c == '~') o[n++] = (char)c;
        else { o[n++] = '%'; o[n++] = hex[c >> 4]; o[n++] = hex[c & 15]; }
    }
    o[n] = 0;
    return o;
}
void curl_free(void *p) { free(p); }

struct curl_slist *curl_slist_append(struct curl_slist *list, const char *data) {
    struct curl_slist *n, *p;
    if (!data) return list;
    n = malloc(sizeof *n);
    if (!n) return 0;
    n->data = dupn(data, -1);
    n->next = 0;
    if (!n->data) { free(n); return 0; }
    if (!list) return n;
    for (p = list; p->next; p = p->next) {}
    p->next = n;
    return list;
}

void curl_slist_free_all(struct curl_slist *list) {
    while (list) {
        struct curl_slist *n = list->next;
        free(list->data);
        free(list);
        list = n;
    }
}

void *curl_url(void) { return calloc(1, sizeof(struct url)); }
static int parse_into(struct url *u, const char *s) {
    char scheme[8], host[256], portb[8], tmp[1024];
    const char *path = "/", *q, *f;
    int port = 0, pn, qn;
    if (!split_url(s, scheme, host, &port, &path)) return 3;
    setp(&u->scheme, scheme);
    setp(&u->host, host);
    if (i_port(port, portb, (int)sizeof portb)) return 3;
    setp(&u->port, portb);
    q = strchr(path, '?');
    f = strchr(path, '#');
    pn = q ? (int)(q - path) : (f ? (int)(f - path) : (int)strlen(path));
    if (pn < 0 || pn >= (int)sizeof tmp) return 3;
    memcpy(tmp, path, (size_t)pn);
    tmp[pn] = 0;
    setp(&u->path, tmp[0] ? tmp : "/");
    if (q) {
        const char *qe = f && f > q ? f : q + strlen(q);
        qn = (int)(qe - (q + 1));
        if (qn < 0 || qn >= (int)sizeof tmp) return 3;
        memcpy(tmp, q + 1, (size_t)qn);
        tmp[qn] = 0;
        setp(&u->query, tmp);
    }
    if (f) setp(&u->frag, f + 1);
    return 0;
}
int curl_url_set(void *p, int part, const char *content, unsigned flags) {
    struct url *u = p;
    char **slot[10];
    (void)flags;
    if (!u) return 1;
    if (part == 0) return content ? parse_into(u, content) : 3;
    if (part < 1 || part > 9 || part == 4) return 9;
    slot[1] = &u->scheme; slot[2] = &u->user; slot[3] = &u->pass; slot[5] = &u->host;
    slot[6] = &u->port; slot[7] = &u->path; slot[8] = &u->query; slot[9] = &u->frag;
    setp(slot[part], content);
    return 0;
}
int curl_url_get(void *p, int part, char **out, unsigned flags) {
    struct url *u = p;
    char b[1536];
    char **slot[10];
    (void)flags;
    if (!u || !out) return 1;
    *out = 0;
    if (part == 0) {
        if (!u->scheme || !u->host) return 10;
        snprintf(b, sizeof b, "%s://%s%s%s%s%s%s%s%s",
                 u->scheme, u->host, u->port ? ":" : "", u->port ? u->port : "",
                 u->path ? u->path : "/", u->query ? "?" : "", u->query ? u->query : "",
                 u->frag ? "#" : "", u->frag ? u->frag : "");
        *out = dupn(b, -1);
        return *out ? 0 : 7;
    }
    if (part < 1 || part > 9 || part == 4) return 9;
    slot[1] = &u->scheme; slot[2] = &u->user; slot[3] = &u->pass; slot[5] = &u->host;
    slot[6] = &u->port; slot[7] = &u->path; slot[8] = &u->query; slot[9] = &u->frag;
    if (!*slot[part]) return 11;
    *out = dupn(*slot[part], -1);
    return *out ? 0 : 7;
}
void curl_url_cleanup(void *p) {
    struct url *u = p;
    if (!u) return;
    free(u->scheme); free(u->user); free(u->pass); free(u->host);
    free(u->port); free(u->path); free(u->query); free(u->frag);
    free(u);
}
