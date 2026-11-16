/* Teste de host: vários pedidos HTTP/2 numa conexão só no cliente mnet (mnet_client.c), contra o http2 do Node.
 *   sh tools/tls_local_certs.sh /tmp/mos-tls-test
 *   gcc -std=gnu11 -O1 -g -Wall -fsanitize=address,undefined -DMNET_HOST -DMOS_HOST_TEST -I userland/libsystem \
 *       -I kernel/include -I tools userland/libsystem/mnet_*.c tools/mnet_h2mux_test.c kernel/net/crypto.c \
 *       kernel/net/tls.c kernel/net/pk.c kernel/net/x509.c -lz -l:libbrotlidec.so.1 -lpthread -o /tmp/mnet_h2mux_test
 *   /tmp/mnet_h2mux_test      (da raiz do repositório; sobe tools/mnet_h2_servidor.js em duas portas)
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <pthread.h>
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
#include "mnet.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FALHOU %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

#include "mnet_host_io.inc"
static int h_wait(void *e, void *cv, int ev, int ms) {
    (void)e;
    hc_t *c = cv;
    if (ev == 1 && c->tls && tls_pending(c->tls)) return 1;
    int r = wait_fd(c->fd, ev == 4 ? POLLOUT : POLLIN, ms);
    return r < 0 ? -1 : r > 0;
}
static mnet_client_t cl;
static int port_a, port_b;                 /* a: 100 fluxos; b: 4 fluxos por conexão */
static long long mono(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000LL + t.tv_nsec / 1000000; }

typedef struct {
    char url[160];
    const char *method;
    const uint8_t *body; size_t body_len;
    int slow_ms;                           /* leitor lento: ms a cada 64 KiB */
    long cancel_after;                     /* cancela depois de N bytes (0 = não) */
    int err, status, conn, reused, bad;
    size_t got;
    char alpn[8];
} req_t;
static int on_body(void *ctx, const uint8_t *d, size_t n) {
    req_t *q = ctx;
    for (size_t i = 0; i < n; i++) if (strstr(q->url, "/grande") && d[i] != (uint8_t)(((q->got + i) * 7 + 3) & 255)) q->bad++;
    if (strstr(q->url, "/eco")) for (size_t i = 0; i < n; i++) if (d[i] != q->body[q->got + i]) q->bad++;
    q->got += n;
    if (q->slow_ms) usleep((useconds_t)((double)n / 65536.0 * q->slow_ms * 1000));   /* slow_ms a cada 64 KiB */
    return q->cancel_after && (long)q->got >= q->cancel_after;
}
static int on_resp(void *ctx, const mnet_resp_t *r) {
    req_t *q = ctx;
    const char *c = mnet_hdrs_get(&r->hdrs, "x-conn");
    q->conn = c ? atoi(c) : -1;
    q->reused = r->reused;
    snprintf(q->alpn, sizeof q->alpn, "%s", r->alpn);
    return 0;
}
static void *run(void *a) {
    req_t *q = a;
    mnet_req_t r; mnet_resp_t rs; char msg[200];
    memset(&r, 0, sizeof r);
    r.method = q->method ? q->method : "GET"; r.url = q->url;
    r.body = q->body; r.body_len = q->body_len;
    r.ctx = q; r.on_body = on_body; r.on_response = on_resp;
    r.timeout_ms = 20000;
    q->err = mnet_fetch(&cl, &r, &rs, msg, sizeof msg);
    q->status = rs.status;
    if (q->err && q->err != MNET_E_CANCELLED) printf("  erro %d: %s (%s)\n", q->err, msg, q->url);
    mnet_resp_free(&rs);
    return 0;
}
static void run_all(req_t *qs, int n) {
    pthread_t t[64];
    for (int i = 0; i < n; i++) pthread_create(&t[i], 0, run, &qs[i]);
    for (int i = 0; i < n; i++) pthread_join(t[i], 0);
}
static pid_t start_server(int port, int maxs) {
    int p[2]; pipe(p);
    pid_t pid = fork();
    if (!pid) {
        dup2(p[1], 1); close(p[0]);
        char ps[16], ms[16]; snprintf(ps, sizeof ps, "%d", port); snprintf(ms, sizeof ms, "%d", maxs);
        execlp("node", "node", "tools/mnet_h2_servidor.js", "/tmp/mos-tls-test", ps, ms, (char *)0);
        _exit(127);
    }
    close(p[1]);
    char b[16] = {0};
    read(p[0], b, sizeof b - 1);
    close(p[0]);
    return strncmp(b, "pronto", 6) ? -1 : pid;
}
static int free_port(void) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a = {0}; a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bind(s, (void *)&a, sizeof a);
    socklen_t l = sizeof a; getsockname(s, (void *)&a, &l);
    close(s);
    return ntohs(a.sin_port);
}
static int count_conns(int port) {
    req_t q = {0};
    snprintf(q.url, sizeof q.url, "https://localhost:%d/contagem", port);
    mnet_req_t r; mnet_resp_t rs; char msg[100];
    memset(&r, 0, sizeof r); r.url = q.url; r.method = "GET";
    int n = -1;
    if (!mnet_fetch(&cl, &r, &rs, msg, sizeof msg)) { const char *v = mnet_hdrs_get(&rs.hdrs, "x-conns"); n = v ? atoi(v) : -1; }
    mnet_resp_free(&rs);
    return n;
}

int main(void) {
    signal(SIGPIPE, SIG_IGN);
    for (int i = 0; i < 64; i++) socks[i] = -1;
    if (system("sh tools/tls_local_certs.sh /tmp/mos-tls-test")) { printf("sem certificados\n"); return 1; }
    FILE *f = fopen("/tmp/mos-tls-test/ca.pem", "rb");
    char pem[8192]; size_t pn = fread(pem, 1, sizeof pem - 1, f); pem[pn] = 0; fclose(f);
    x509_trust_load_pem(pem, (u32)pn);
    port_a = free_port(); port_b = free_port();
    pid_t sa = start_server(port_a, 100), sb = start_server(port_b, 4);
    CHECK(sa > 0 && sb > 0);
    static mnet_io_t iow;
    iow = io; iow.wait = h_wait;      /* com wait, como no mOS (tls_pending vê os registros já lidos do TCP) */
    mnet_client_init(&cl, &iow);

    /* 1. 10 pedidos lentos ao mesmo tempo: uma conexão, todos em paralelo */
    req_t q[16];
    memset(q, 0, sizeof q);
    for (int i = 0; i < 10; i++) snprintf(q[i].url, sizeof q[i].url, "https://localhost:%d/lento?ms=400", port_a);
    long long t0 = mono();
    run_all(q, 10);
    long long dt = mono() - t0;
    int ok = 1, same = 1;
    for (int i = 0; i < 10; i++) { if (q[i].err || q[i].status != 200 || strcmp(q[i].alpn, "h2")) ok = 0; if (q[i].conn != q[0].conn) same = 0; }
    CHECK(ok);
    CHECK(same);
    CHECK(dt < 2500);                                              /* em série seriam 4 s */
    printf("  10 pedidos em %lld ms numa conexão\n", dt);
    CHECK(count_conns(port_a) == 1);

    /* 2. servidor com 4 fluxos: 8 pedidos usam 2 conexões */
    memset(q, 0, sizeof q);
    for (int i = 0; i < 8; i++) snprintf(q[i].url, sizeof q[i].url, "https://localhost:%d/lento?ms=600", port_b);
    run_all(q, 8);
    int c1 = q[0].conn, c2 = -1, all_ok = 1, other = 1;
    for (int i = 0; i < 8; i++) {
        if (q[i].err || q[i].status != 200) all_ok = 0;
        if (q[i].conn != c1) { if (c2 < 0) c2 = q[i].conn; else if (q[i].conn != c2) other = 0; }
    }
    CHECK(all_ok && c2 > 0 && other);
    if (!(all_ok && c2 > 0 && other)) for (int i = 0; i < 8; i++) printf("  [%d] err %d st %d conn %d\n", i, q[i].err, q[i].status, q[i].conn);
    int nb = count_conns(port_b);
    CHECK(nb >= 2 && nb <= 3);

    /* 3. downloads grandes ao mesmo tempo, um leitor lento: os rápidos terminam antes, sem misturar bytes */
    memset(q, 0, sizeof q);
    for (int i = 0; i < 3; i++) snprintf(q[i].url, sizeof q[i].url, "https://localhost:%d/grande?n=%d", port_a, i ? 2 << 20 : 6 << 20);
    q[0].slow_ms = 150;                                            /* 96 x 150 ms: ~14 s só para ler */
    pthread_t t[3];
    long long done_at[3];
    t0 = mono();
    for (int i = 0; i < 3; i++) pthread_create(&t[i], 0, run, &q[i]);
    volatile int fin[3] = {0, 0, 0};
    for (;;) {                                                     /* hora em que cada um terminou, sem depender da ordem do join */
        int all = 1;
        for (int i = 0; i < 3; i++) {
            if (!fin[i] && pthread_tryjoin_np(t[i], 0) == 0) { fin[i] = 1; done_at[i] = mono() - t0; }
            if (!fin[i]) all = 0;
        }
        if (all) break;
        usleep(20000);
    }
    for (int i = 0; i < 3; i++) CHECK(!q[i].err && q[i].got == (size_t)(i ? 2 << 20 : 6 << 20) && !q[i].bad && q[i].conn == q[1].conn);
    printf("  6 MiB lento + 2 x 2 MiB: lento %lld ms, rápidos %lld/%lld ms\n", done_at[0], done_at[1], done_at[2]);
    CHECK(done_at[0] > 13000 && done_at[1] < done_at[0] - 4000 && done_at[2] < done_at[0] - 4000);   /* o lento só segura o próprio fluxo */

    /* 4. upload de 3 MiB (janela de envio) junto com GETs */
    size_t big = 3u << 20;
    uint8_t *body = malloc(big);
    for (size_t i = 0; i < big; i++) body[i] = (uint8_t)(i * 31 + 5);
    memset(q, 0, sizeof q);
    snprintf(q[0].url, sizeof q[0].url, "https://localhost:%d/eco", port_a);
    q[0].method = "POST"; q[0].body = body; q[0].body_len = big;
    for (int i = 1; i < 5; i++) snprintf(q[i].url, sizeof q[i].url, "https://localhost:%d/lento?ms=100", port_a);
    run_all(q, 5);
    CHECK(!q[0].err && q[0].status == 200 && q[0].got == big && !q[0].bad);
    ok = 1;
    for (int i = 1; i < 5; i++) if (q[i].err || q[i].status != 200 || q[i].conn != q[0].conn) ok = 0;
    CHECK(ok);
    free(body);

    /* 5. um pedido cancela no meio; os outros seguem e a conexão continua servindo */
    memset(q, 0, sizeof q);
    snprintf(q[0].url, sizeof q[0].url, "https://localhost:%d/grande?n=%d", port_a, 8 << 20);
    q[0].cancel_after = 100000;
    for (int i = 1; i < 4; i++) snprintf(q[i].url, sizeof q[i].url, "https://localhost:%d/grande?n=%d", port_a, 1 << 20);
    run_all(q, 4);
    CHECK(q[0].err == MNET_E_CANCELLED);
    ok = 1;
    for (int i = 1; i < 4; i++) if (q[i].err || q[i].got != (1u << 20) || q[i].bad) ok = 0;
    CHECK(ok);
    req_t one = {0};
    snprintf(one.url, sizeof one.url, "https://localhost:%d/lento?ms=0", port_a);
    run(&one);
    CHECK(!one.err && one.reused && one.conn == q[1].conn);
    CHECK(count_conns(port_a) == 1);

    /* 6. GOAWAY: o próximo pedido abre outra conexão */
    memset(&one, 0, sizeof one);
    snprintf(one.url, sizeof one.url, "https://localhost:%d/goaway", port_a);
    run(&one);
    CHECK(!one.err && one.status == 200);
    usleep(300000);
    req_t two = {0};
    snprintf(two.url, sizeof two.url, "https://localhost:%d/lento?ms=0", port_a);
    run(&two);
    CHECK(!two.err && two.status == 200 && two.conn != one.conn);

    mnet_client_close_idle(&cl);
    kill(sa, SIGTERM); kill(sb, SIGTERM);
    waitpid(sa, 0, 0); waitpid(sb, 0, 0);
    printf("mnet_h2mux: %d/%d ok\n", checks - fails, checks);
    return fails ? 1 : 0;
}
