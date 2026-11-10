#define _POSIX_C_SOURCE 200809L
/* Teste de host da decisão do NSURLSession (userland/libsystem/urlsession_auth.inc).
 * Sobe um servidor TCP em 127.0.0.1. O socket é HTTP puro: não há certificado local.
 * O desafio de servidor é o mesmo ponto do libSystem (antes do pedido, quando o
 * esquema é https). O handshake TLS em si não roda aqui.
 *
 *   gcc -std=c11 -Wall -Wextra -O1 tools/urlsessiontest.c -o /tmp/urlsessiontest
 *   /tmp/urlsessiontest
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "../userland/libsystem/urlsession_auth.inc"

static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

typedef struct {
    char ev[16][32];
    int nev;
    int server_n, basic_n, resp_n;
    char realm[64];
    char method_server[80];
    char method_basic[80];
    int cancel_next;
    char body[128];
    int body_kept;
} dlg_t;

static void ev(dlg_t *d, const char *s) {
    if (d->nev < 16) snprintf(d->ev[d->nev++], 32, "%s", s);
}

static int read_until(int fd, char *buf, size_t cap) {
    size_t n = 0;
    while (n + 1 < cap) {
        ssize_t k = read(fd, buf + n, 1);
        if (k <= 0) return -1;
        n += (size_t)k;
        buf[n] = 0;
        if (n >= 4 && memcmp(buf + n - 4, "\r\n\r\n", 4) == 0) return (int)n;
    }
    return -1;
}

static int header_len(const char *hdr, const char *name) {
    const char *p = hdr;
    size_t nl = strlen(name);
    while (p && *p) {
        if (us_casecmp_n(p, name, nl) == 0 && p[nl] == ':') {
            p += nl + 1;
            while (*p == ' ') p++;
            return atoi(p);
        }
        p = strstr(p, "\r\n");
        if (p) p += 2;
    }
    return -1;
}

static int read_body(int fd, int n, char *out, size_t cap) {
    if (n < 0) n = 0;
    if ((size_t)n >= cap) return -1;
    size_t got = 0;
    while ((int)got < n) {
        ssize_t k = read(fd, out + got, (size_t)n - got);
        if (k <= 0) return -1;
        got += (size_t)k;
    }
    out[got] = 0;
    return (int)got;
}

static void serve(int fd) {
    for (;;) {
        char req[4096];
        if (read_until(fd, req, sizeof req) < 0) break;
        int ok_path = strstr(req, "GET /ok ") == req;
        int has_auth = strstr(req, "\r\nAuthorization: Basic ") != 0;
        const char *msg;
        char built[256];
        if (!ok_path && !has_auth) {
            msg = "HTTP/1.1 401 Unauthorized\r\nWWW-Authenticate: Basic realm=\"mos\"\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n";
        } else {
            const char *body = ok_path ? "segredo" : "voz";
            snprintf(built, sizeof built,
                     "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nConnection: keep-alive\r\n\r\n%s",
                     (int)strlen(body), body);
            msg = built;
        }
        size_t left = strlen(msg);
        const char *p = msg;
        while (left) {
            ssize_t k = write(fd, p, left);
            if (k <= 0) return;
            p += k;
            left -= (size_t)k;
        }
    }
}

static int http_exchange(int fd, const char *req, int *status, char *www, size_t wwwcap, char *body, size_t bcap) {
    size_t left = strlen(req);
    const char *p = req;
    while (left) {
        ssize_t k = write(fd, p, left);
        if (k <= 0) return -1;
        p += k;
        left -= (size_t)k;
    }
    char hdr[2048];
    if (read_until(fd, hdr, sizeof hdr) < 0) return -1;
    *status = atoi(hdr + (strncmp(hdr, "HTTP/", 5) == 0 ? 9 : 0));
    if (www && wwwcap) {
        www[0] = 0;
        const char *w = strstr(hdr, "WWW-Authenticate:");
        if (!w) w = strstr(hdr, "Www-Authenticate:");
        if (w) {
            w = strchr(w, ':');
            if (w) {
                w++;
                while (*w == ' ') w++;
                size_t i = 0;
                while (w[i] && w[i] != '\r' && w[i] != '\n' && i + 1 < wwwcap) {
                    www[i] = w[i];
                    i++;
                }
                www[i] = 0;
            }
        }
    }
    int cl = header_len(hdr, "Content-Length");
    if (cl < 0) cl = header_len(hdr, "content-length");
    if (read_body(fd, cl, body, bcap) < 0) return -1;
    return 0;
}

/* Espelha task_thread + dlg_run: desafio de servidor antes do pedido https,
 * Basic no 401, e o delegate decide se o corpo fica. */
static int session_once(int fd, dlg_t *d, const char *path, const char *extra_hdr) {
    ev(d, "server");
    d->server_n++;
    snprintf(d->method_server, sizeof d->method_server, "%s", US_METHOD_SERVER);
    int disp = US_AUTH_USE;
    if (disp == US_AUTH_CANCEL || disp == US_AUTH_REJECT) return 0;

    char req[1024];
    snprintf(req, sizeof req, "GET %s HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: keep-alive\r\n%s\r\n",
             path, extra_hdr ? extra_hdr : "");
    ev(d, "request");
    int status = 0;
    char www[256], body[128];
    www[0] = body[0] = 0;
    if (http_exchange(fd, req, &status, www, sizeof www, body, sizeof body) != 0) return -1;

    char realm[64];
    if (status == 401 && us_is_basic(www, realm, sizeof realm)) {
        ev(d, "basic");
        d->basic_n++;
        snprintf(d->method_basic, sizeof d->method_basic, "%s", US_METHOD_BASIC);
        snprintf(d->realm, sizeof d->realm, "%s", realm);
        char auth[512];
        if (us_basic_value("Aladdin", "open sesame", auth, sizeof auth) != 0) return -1;
        char hdr[600];
        snprintf(hdr, sizeof hdr, "Authorization: %s\r\n", auth);
        snprintf(req, sizeof req, "GET %s HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: keep-alive\r\n%s\r\n", path, hdr);
        ev(d, "retry");
        if (http_exchange(fd, req, &status, www, sizeof www, body, sizeof body) != 0) return -1;
    }

    ev(d, d->cancel_next ? "response-cancel" : "response-allow");
    d->resp_n++;
    int rd = d->cancel_next ? US_DISP_CANCEL : US_DISP_ALLOW;
    d->cancel_next = 0;
    if (rd == US_DISP_CANCEL) {
        d->body[0] = 0;
        d->body_kept = 0;
    } else if (rd == US_DISP_ALLOW) {
        snprintf(d->body, sizeof d->body, "%s", body);
        d->body_kept = 1;
    }
    return status;
}

int main(void) {
    signal(SIGPIPE, SIG_IGN);
    char auth[512];
    CHECK(us_basic_value("Aladdin", "open sesame", auth, sizeof auth) == 0);
    CHECK(strcmp(auth, "Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==") == 0);
    char realm[32];
    CHECK(us_is_basic("Basic realm=\"mos\"", realm, sizeof realm) == 1);
    CHECK(strcmp(realm, "mos") == 0);
    CHECK(us_is_basic("Digest realm=\"x\"", realm, sizeof realm) == 0);
    CHECK(us_casecmp("Basic", "basic") == 0);

    int srv = socket(AF_INET, SOCK_STREAM, 0);
    CHECK(srv >= 0);
    int yes = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(0x7f000001);
    addr.sin_port = 0;
    CHECK(bind(srv, (struct sockaddr *)&addr, sizeof addr) == 0);
    CHECK(listen(srv, 1) == 0);
    socklen_t alen = sizeof addr;
    CHECK(getsockname(srv, (struct sockaddr *)&addr, &alen) == 0);

    pid_t pid = fork();
    CHECK(pid >= 0);
    if (pid == 0) {
        int c = accept(srv, 0, 0);
        if (c >= 0) serve(c);
        _exit(0);
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    CHECK(fd >= 0);
    CHECK(connect(fd, (struct sockaddr *)&addr, sizeof addr) == 0);

    dlg_t d;
    memset(&d, 0, sizeof d);
    int st = session_once(fd, &d, "/", 0);
    CHECK(st == 200);
    CHECK(d.server_n == 1 && d.basic_n == 1 && d.resp_n == 1);
    CHECK(strcmp(d.method_server, "NSURLAuthenticationMethodServerTrust") == 0);
    CHECK(strcmp(d.method_basic, "NSURLAuthenticationMethodHTTPBasic") == 0);
    CHECK(strcmp(d.realm, "mos") == 0);
    CHECK(d.body_kept && strcmp(d.body, "voz") == 0);
    CHECK(d.nev >= 5);
    CHECK(strcmp(d.ev[0], "server") == 0);
    CHECK(strcmp(d.ev[1], "request") == 0);
    CHECK(strcmp(d.ev[2], "basic") == 0);
    CHECK(strcmp(d.ev[3], "retry") == 0);
    CHECK(strcmp(d.ev[4], "response-allow") == 0);

    d.cancel_next = 1;
    int before = d.nev;
    st = session_once(fd, &d, "/ok", 0);
    CHECK(st == 200);
    CHECK(d.server_n == 2 && d.basic_n == 1 && d.resp_n == 2);
    CHECK(!d.body_kept && d.body[0] == 0);
    CHECK(strcmp(d.ev[before], "server") == 0);
    CHECK(strcmp(d.ev[before + 1], "request") == 0);
    CHECK(strcmp(d.ev[before + 2], "response-cancel") == 0);

    close(fd);
    close(srv);

    if (fails) {
        fprintf(stderr, "urlsessiontest: %d falhas em %d checagens\n", fails, checks);
        return 1;
    }
    printf("urlsessiontest: %d/%d PASS\n", checks, checks);
    return 0;
}
