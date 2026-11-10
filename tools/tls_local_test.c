/* Handshake HTTPS no host (Linux) com a pilha TLS do mOS (kernel/net/tls.c).
 * A AC de teste é gerada aqui e carregada só neste processo. A verificação
 * permanece ligada: g_tls_insecure não é alterado.
 *
 *   gcc -O2 -DMOS_HOST_TEST -Ikernel/include -Itools \
 *     tools/tls_local_test.c kernel/net/crypto.c kernel/net/tls.c \
 *     kernel/net/pk.c kernel/net/x509.c -o build/tls_local_test
 */
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
#include "x509.h"

extern bool g_tls_insecure;
extern u16 tls_cipher_prefs[4];
extern u8 tls_versions;

static int fails, passes;
static void check(const char *name, int cond) {
    if (cond) { passes++; printf("  ok    %s\n", name); }
    else { fails++; printf("  FALHA %s\n", name); }
}

u64 timer_ticks(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u64)ts.tv_sec * 1000ull + (u64)ts.tv_nsec / 1000000ull;
}
void rtc_now_utc(datetime_t *dt);
void rtc_now(datetime_t *dt) {
    time_t t = time(NULL);
    struct tm tm;
    gmtime_r(&t, &tm);
    dt->year = tm.tm_year + 1900; dt->month = tm.tm_mon + 1; dt->day = tm.tm_mday;
    dt->hour = tm.tm_hour; dt->minute = tm.tm_min; dt->second = tm.tm_sec; dt->weekday = tm.tm_wday;
}
void rtc_now_utc(datetime_t *dt) { rtc_now(dt); }

static int socks[8];
static int nsocks;
int tcp_state(int s) { return s >= 0 && s < nsocks && socks[s] >= 0 ? TCP_OPEN : TCP_ERROR; }
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

static int tcp_connect(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons((unsigned short)port);
    a.sin_addr.s_addr = htonl(0x7f000001);
    if (connect(fd, (struct sockaddr *)&a, sizeof a) != 0) { close(fd); return -1; }
    int fl = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, fl | O_NONBLOCK);
    for (int i = 0; i < nsocks; i++) if (socks[i] < 0) { socks[i] = fd; return i; }
    if (nsocks == 8) { close(fd); return -1; }
    socks[nsocks] = fd;
    return nsocks++;
}
static void tcp_drop(int s) { if (s >= 0 && s < nsocks && socks[s] >= 0) { close(socks[s]); socks[s] = -1; } }

static int run(const char *cmd) {
    int st = system(cmd);
    return st == 0;
}

static pid_t serve(const char *cert, const char *key, int port) {
    pid_t p = fork();
    if (p < 0) return -1;
    if (p == 0) {
        char addr[64];
        snprintf(addr, sizeof addr, "127.0.0.1:%d", port);
        int fd = open("/tmp/mos-tls-s_server.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd >= 0) { dup2(fd, 1); dup2(fd, 2); }
        execlp("openssl", "openssl", "s_server", "-accept", addr, "-cert", cert, "-key", key,
               "-www", "-quiet", (char *)0);
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
    waitpid(p, NULL, 0);
    return -1;
}
static void stop(pid_t p) { if (p > 0) { kill(p, SIGTERM); waitpid(p, NULL, 0); } }

static int handshake(int port, const char *sni, char *err, size_t errlen, int *http_ok) {
    int s = tcp_connect(port);
    if (s < 0) { snprintf(err, errlen, "tcp"); return -1; }
    tls_conn_t *c = tls_new(s, sni);
    u64 t0 = timer_ticks();
    int r;
    while ((r = tls_handshake(c)) == 0) {
        if (timer_ticks() - t0 > 8000) break;
        usleep(1000);
    }
    if (r != 1) {
        snprintf(err, errlen, "%s", r < 0 ? tls_error(c) : "tempo esgotado");
        tls_free(c); tcp_drop(s);
        if (http_ok) *http_ok = 0;
        return -1;
    }
    char req[256];
    int rl = snprintf(req, sizeof req, "GET / HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", sni);
    int sent = 0;
    while (sent < rl) {
        int n = tls_send(c, req + sent, rl - sent);
        if (n < 0) break;
        if (!n) usleep(1000);
        sent += n;
    }
    char body[1024];
    int total = 0;
    t0 = timer_ticks();
    while (total < (int)sizeof body - 1) {
        int n = tls_recv(c, body + total, (int)sizeof body - 1 - total);
        if (n < 0) break;
        if (!n) {
            if (timer_ticks() - t0 > 3000) break;
            usleep(1000);
            continue;
        }
        total += n;
        t0 = timer_ticks();
    }
    body[total] = 0;
    if (http_ok) *http_ok = strncmp(body, "HTTP/1.", 7) == 0;
    tls_free(c);
    tcp_drop(s);
    err[0] = 0;
    return 0;
}

int main(void) {
    for (int i = 0; i < 8; i++) socks[i] = -1;
    check("verificação de certificado ligada", g_tls_insecure == false);
    check("certificados de teste gerados", run("sh tools/tls_local_certs.sh /tmp/mos-tls-test"));

    FILE *f = fopen("/tmp/mos-tls-test/ca.pem", "rb");
    check("PEM da AC de teste", f != NULL);
    if (!f) return 1;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *pem = malloc((size_t)n + 1);
    if (!pem || fread(pem, 1, (size_t)n, f) != (size_t)n) return 1;
    pem[n] = 0;
    fclose(f);
    int nroot = x509_trust_load_pem(pem, (u32)n);
    free(pem);
    check("só a AC de teste entrou na lista", nroot == 1 && x509_trust_count() == 1);

    char err[192];
    int http = 0;
    pid_t p = serve("/tmp/mos-tls-test/srv.pem", "/tmp/mos-tls-test/srv.key", 18443);
    check("servidor local no ar", p > 0);
    int hr = p > 0 ? handshake(18443, "localhost", err, sizeof err, &http) : -1;
    check("handshake HTTPS com a AC de teste", hr == 0);
    check("resposta HTTP depois do handshake", http == 1);
    if (hr != 0) printf("    motivo: %s\n", err);
    stop(p);

    p = serve("/tmp/mos-tls-test/exp.pem", "/tmp/mos-tls-test/srv.key", 18444);
    if (p <= 0) snprintf(err, sizeof err, "sem servidor");
    hr = p > 0 ? handshake(18444, "localhost", err, sizeof err, &http) : -1;
    check("recusa certificado expirado", hr != 0 && strstr(err, "expirad") != NULL);
    if (hr == 0 || !strstr(err, "expirad")) printf("    motivo: %s\n", err);
    stop(p);

    p = serve("/tmp/mos-tls-test/badname.pem", "/tmp/mos-tls-test/srv.key", 18445);
    hr = p > 0 ? handshake(18445, "localhost", err, sizeof err, &http) : -1;
    check("recusa nome errado", hr != 0 && strstr(err, "não corresponde") != NULL);
    if (hr == 0 || !strstr(err, "não corresponde")) printf("    motivo: %s\n", err);
    stop(p);

    p = serve("/tmp/mos-tls-test/unknown.pem", "/tmp/mos-tls-test/srv.key", 18446);
    if (p <= 0) snprintf(err, sizeof err, "sem servidor");
    hr = p > 0 ? handshake(18446, "localhost", err, sizeof err, &http) : -1;
    check("recusa AC desconhecida", hr != 0 && strstr(err, "não confiável") != NULL);
    if (hr == 0 || !strstr(err, "não confiável")) printf("    motivo: %s\n", err);
    stop(p);

    printf("tls_local: %d/%d\n", passes, passes + fails);
    return fails ? 1 : 0;
}
