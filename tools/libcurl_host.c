/* GET, POST, redirect e HTTPS com o motor TLS do mOS (kernel/net/tls.c).
 * A verificação de certificado fica ligada. A AC de teste só existe neste processo. */
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "tls_shim.h"
#include "x509.h"

extern bool g_tls_insecure;

u64 timer_ticks(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u64)ts.tv_sec * 1000ull + (u64)ts.tv_nsec / 1000000ull;
}
void rtc_now(datetime_t *dt) {
    time_t t = time(NULL);
    struct tm tm;
    gmtime_r(&t, &tm);
    dt->year = tm.tm_year + 1900; dt->month = tm.tm_mon + 1; dt->day = tm.tm_mday;
    dt->hour = tm.tm_hour; dt->minute = tm.tm_min; dt->second = tm.tm_sec; dt->weekday = tm.tm_wday;
}
void rtc_now_utc(datetime_t *dt) { rtc_now(dt); }

int curl_easy_setopt(void *, int, ...);
int curl_easy_perform(void *);
void *curl_easy_init(void);
void curl_easy_cleanup(void *);
char *curl_easy_escape(void *, const char *, int);
void curl_free(void *);
void *curl_url(void);
int curl_url_set(void *, int, const char *, unsigned);
int curl_url_get(void *, int, char **, unsigned);
void curl_url_cleanup(void *);

struct curl_slist { char *data; struct curl_slist *next; };

static int socks[8];
static int nsocks;
int tcp_state(int s) { return s >= 0 && s < nsocks && socks[s] >= 0 ? 2 : 4; }
int tcp_send(int s, const void *data, int len) {
    int n = (int)send(socks[s], data, (size_t)len, MSG_NOSIGNAL);
    if (n < 0) return errno == EAGAIN || errno == EWOULDBLOCK ? 0 : -1;
    return n;
}
int tcp_recv(int s, void *buf, int max) {
    int n = (int)recv(socks[s], buf, (size_t)max, 0);
    if (n == 0) return -1;
    if (n < 0) return errno == EAGAIN || errno == EWOULDBLOCK ? 0 : -1;
    return n;
}

int curl_host_tcp(const char *host, int port) {
    struct addrinfo_hint { int a, b, c, d; unsigned e; void *f, *g, *h; };
    int fd, slot;
    struct sockaddr_in a;
    (void)host;
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons((unsigned short)port);
    a.sin_addr.s_addr = htonl(0x7f000001);
    if (connect(fd, (struct sockaddr *)&a, sizeof a)) { close(fd); return -1; }
    {
        int fl = fcntl(fd, F_GETFL, 0);
        if (fl >= 0) fcntl(fd, F_SETFL, fl | O_NONBLOCK);
    }
    for (slot = 0; slot < nsocks; slot++) if (socks[slot] < 0) { socks[slot] = fd; return slot; }
    if (nsocks == 8) { close(fd); return -1; }
    socks[nsocks] = fd;
    return nsocks++;
}
void curl_host_drop(int slot) {
    if (slot >= 0 && slot < nsocks && socks[slot] >= 0) { close(socks[slot]); socks[slot] = -1; }
}
int curl_host_send(int slot, const void *b, int n) { return tcp_send(slot, b, n); }
int curl_host_recv(int slot, void *b, int n) {
    int k = (int)recv(socks[slot], b, (size_t)n, MSG_DONTWAIT);
    if (k > 0) return k;
    if (k == 0) return 0;
    if (errno == EAGAIN || errno == EWOULDBLOCK) return -2;
    return -1;
}

static int fails;
static void check(const char *name, int cond) {
    if (cond) printf("  ok    %s\n", name);
    else { fails++; printf("  FALHA %s\n", name); }
}

static pid_t serve_http(int port) {
    pid_t p = fork();
    char script[900];
    if (p < 0) return -1;
    if (p == 0) {
        snprintf(script, sizeof script,
                 "from http.server import BaseHTTPRequestHandler,HTTPServer\n"
                 "class H(BaseHTTPRequestHandler):\n"
                 " def do_GET(self):\n"
                 "  if self.path.startswith('/redir'):\n"
                 "   self.send_response(302); self.send_header('Location','/ok'); self.end_headers(); return\n"
                 "  self.send_response(200); self.end_headers(); self.wfile.write(b'hello' if self.path!='/ok' else b'landed')\n"
                 " def do_POST(self):\n"
                 "  n=int(self.headers.get('Content-Length','0')); b=self.rfile.read(n)\n"
                 "  ok=self.headers.get('X-Test')=='sim'\n"
                 "  self.send_response(200 if ok else 400); self.end_headers(); self.wfile.write(b if ok else b'no')\n"
                 " def log_message(self,*a): pass\n"
                 "HTTPServer(('127.0.0.1',%d),H).serve_forever()\n", port);
        execlp("python3", "python3", "-c", script, (char *)0);
        _exit(127);
    }
    for (int i = 0; i < 50; i++) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in a;
        memset(&a, 0, sizeof a);
        a.sin_family = AF_INET;
        a.sin_port = htons((unsigned short)port);
        a.sin_addr.s_addr = htonl(0x7f000001);
        int ok = fd >= 0 && connect(fd, (struct sockaddr *)&a, sizeof a) == 0;
        if (fd >= 0) close(fd);
        if (ok) return p;
        usleep(20000);
    }
    kill(p, SIGTERM);
    waitpid(p, 0, 0);
    return -1;
}

static pid_t serve_tls(const char *cert, const char *key, int port) {
    pid_t p = fork();
    if (p < 0) return -1;
    if (p == 0) {
        char addr[64];
        snprintf(addr, sizeof addr, "127.0.0.1:%d", port);
        int fd = open("/tmp/mos-curl-s_server.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) { dup2(fd, 1); dup2(fd, 2); }
        execlp("openssl", "openssl", "s_server", "-accept", addr, "-cert", cert, "-key", key, "-www", "-quiet", (char *)0);
        _exit(127);
    }
    for (int i = 0; i < 50; i++) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in a;
        memset(&a, 0, sizeof a);
        a.sin_family = AF_INET;
        a.sin_port = htons((unsigned short)port);
        a.sin_addr.s_addr = htonl(0x7f000001);
        int ok = fd >= 0 && connect(fd, (struct sockaddr *)&a, sizeof a) == 0;
        if (fd >= 0) close(fd);
        if (ok) return p;
        usleep(20000);
    }
    kill(p, SIGTERM);
    waitpid(p, 0, 0);
    return -1;
}
static void stop(pid_t p) { if (p > 0) { kill(p, SIGTERM); waitpid(p, 0, 0); } }

struct buf { char d[8192]; unsigned long n; };
static unsigned long wr(char *p, unsigned long s, unsigned long n, void *ud) {
    struct buf *b = ud;
    unsigned long k = s * n;
    if (b->n + k >= sizeof b->d) return 0;
    memcpy(b->d + b->n, p, k);
    b->n += k;
    b->d[b->n] = 0;
    return k;
}

int main(void) {
    pid_t hp, tp;
    void *e, *u;
    struct buf b;
    struct curl_slist h = { "X-Test: sim", 0 };
    char *esc, *got = 0;
    int rc;
    for (int i = 0; i < 8; i++) socks[i] = -1;
    check("verificação de certificado ligada", g_tls_insecure == false);

    hp = serve_http(18771);
    check("HTTP local no ar", hp > 0);
    e = curl_easy_init();
    memset(&b, 0, sizeof b);
    curl_easy_setopt(e, 10002, "http://127.0.0.1:18771/");
    curl_easy_setopt(e, 20011, wr);
    curl_easy_setopt(e, 10001, &b);
    rc = curl_easy_perform(e);
    check("GET", rc == 0 && strstr(b.d, "hello"));

    memset(&b, 0, sizeof b);
    curl_easy_setopt(e, 10002, "http://127.0.0.1:18771/echo");
    curl_easy_setopt(e, 10015, "abc=1");
    curl_easy_setopt(e, 10023, &h);
    rc = curl_easy_perform(e);
    check("POST com cabeçalho", rc == 0 && strstr(b.d, "abc=1"));

    memset(&b, 0, sizeof b);
    curl_easy_setopt(e, 80, 1L);
    curl_easy_setopt(e, 52, 1L);
    curl_easy_setopt(e, 10002, "http://127.0.0.1:18771/redir");
    rc = curl_easy_perform(e);
    check("redirect", rc == 0 && strstr(b.d, "landed"));
    curl_easy_cleanup(e);
    stop(hp);

    esc = curl_easy_escape(0, "a b", 3);
    check("escape", esc && !strcmp(esc, "a%20b"));
    curl_free(esc);
    u = curl_url();
    check("url_set", curl_url_set(u, 0, "https://exemplo:8443/x?y=1#z", 0) == 0);
    check("url_get", curl_url_get(u, 5, &got, 0) == 0 && got && !strcmp(got, "exemplo"));
    curl_free(got);
    curl_url_cleanup(u);

    check("AC de teste", system("sh tools/tls_local_certs.sh /tmp/mos-tls-test") == 0);
    tp = serve_tls("/tmp/mos-tls-test/srv.pem", "/tmp/mos-tls-test/srv.key", 18449);
    check("HTTPS local no ar", tp > 0);
    e = curl_easy_init();
    memset(&b, 0, sizeof b);
    curl_easy_setopt(e, 10002, "https://127.0.0.1:18449/");
    curl_easy_setopt(e, 64, 0L);
    curl_easy_setopt(e, 81, 0L);
    curl_easy_setopt(e, 20011, wr);
    curl_easy_setopt(e, 10001, &b);
    rc = curl_easy_perform(e);
    check("TLS recusa AC que não está na lista", rc != 0);

    {
        FILE *f = fopen("/tmp/mos-tls-test/ca.pem", "rb");
        char *pem = 0;
        long n = 0;
        if (f) {
            fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
            pem = malloc((size_t)n + 1);
            if (pem && fread(pem, 1, (size_t)n, f) == (size_t)n) pem[n] = 0;
            fclose(f);
        }
        check("AC carregada", pem && x509_trust_load_pem(pem, (unsigned)n) == 1);
        free(pem);
    }
    memset(&b, 0, sizeof b);
    curl_easy_setopt(e, 10002, "https://localhost:18449/");
    rc = curl_easy_perform(e);
    check("HTTPS com a AC de teste", rc == 0 && b.n > 0);
    curl_easy_cleanup(e);
    stop(tp);

    printf("libcurl_host: %s\n", fails ? "FALHA" : "ok");
    return fails ? 1 : 0;
}
