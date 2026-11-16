/* Teste de host do cliente mnet (mnet_client.c) com o motor TLS do mOS (kernel/net/tls.c).
 *   gcc -std=gnu11 -O1 -g -Wall -DMNET_HOST -DMOS_HOST_TEST -I userland/libsystem -I kernel/include -I tools \
 *       userland/libsystem/mnet_*.c tools/mnet_client_test.c kernel/net/crypto.c kernel/net/tls.c kernel/net/pk.c \
 *       kernel/net/x509.c -lz -l:libbrotlidec.so.1 -lpthread -o /tmp/mnet_client_test
 *   /tmp/mnet_client_test          (local: servidor tools/mnet_http_servidor.py em 127.0.0.1)
 *   /tmp/mnet_client_test rede     (+ HTTPS de verdade com as raízes da Mozilla: h2 e http/1.1)
 * A verificação de certificado fica ligada (g_tls_insecure não muda).
 */
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

/* ---- coleta */
typedef struct { mnet_buf_t body; int responses; int redirects_seen; int stop_redirect; } col_t;
static int c_body(void *x, const uint8_t *d, size_t n) { return mnet_buf_put(&((col_t *)x)->body, d, n); }
static int c_resp(void *x, const mnet_resp_t *r) { (void)r; ((col_t *)x)->responses++; return 0; }
static int c_redir(void *x, const mnet_resp_t *r, char **u) { (void)r; (void)u; col_t *c = x; c->redirects_seen++; return !c->stop_redirect; }

static int get(mnet_client_t *cl, const char *method, const char *url, const char *body, col_t *c, mnet_resp_t *r,
               int timeout, int use_redir_cb) {
    mnet_req_t q = {0};
    q.method = method; q.url = url;
    q.body = (const uint8_t *)body; q.body_len = body ? strlen(body) : 0;
    q.timeout_ms = timeout;
    q.ctx = c; q.on_body = c_body; q.on_response = c_resp;
    if (use_redir_cb) q.on_redirect = c_redir;
    char msg[200];
    int e = mnet_fetch(cl, &q, r, msg, sizeof msg);
    if (e) printf("    %s %s -> %d (%s)\n", method, url, e, msg);
    mnet_buf_putc(&c->body, 0); c->body.n--;
    return e;
}

static void local(void) {
    int port = 18000 + (getpid() % 1000);
    char cmd[256];
    snprintf(cmd, sizeof cmd, "exec python3 -I tools/mnet_http_servidor.py %d 2>/dev/null", port);
    FILE *srv = popen(cmd, "r");
    char line[32] = {0};
    CHECK(srv && fgets(line, sizeof line, srv) && !strncmp(line, "pronto", 6));
    int srv_pid = atoi(line + 7);
    mnet_client_t cl; mnet_jar_t jar;
    mnet_client_init(&cl, &io);
    mnet_jar_init(&jar);
    cl.jar = &jar;
    char u[128];
#define URL(p) (snprintf(u, sizeof u, "http://127.0.0.1:%d%s", port, p), u)
    col_t c = {0}; mnet_resp_t r;
    CHECK(get(&cl, "GET", URL("/ola"), 0, &c, &r, 5000, 0) == 0 && r.status == 200 && !strcmp((char *)c.body.p, "ola mundo"));
    CHECK(r.version == 11 && c.responses == 1 && r.expected_len == 9);
    mnet_resp_free(&r); mnet_buf_free(&c.body); memset(&c, 0, sizeof c);
    /* keep-alive: 5 pedidos, uma conexão nova só */
    int before = 0;
    get(&cl, "GET", URL("/conta"), 0, &c, &r, 5000, 0); before = atoi((char *)c.body.p); mnet_resp_free(&r); mnet_buf_free(&c.body); memset(&c, 0, sizeof c);
    for (int i = 0; i < 4; i++) { get(&cl, "GET", URL("/ola"), 0, &c, &r, 5000, 0); CHECK(r.reused); mnet_resp_free(&r); mnet_buf_free(&c.body); memset(&c, 0, sizeof c); }
    get(&cl, "GET", URL("/conta"), 0, &c, &r, 5000, 0);
    CHECK(atoi((char *)c.body.p) == before);
    mnet_resp_free(&r); mnet_buf_free(&c.body); memset(&c, 0, sizeof c);
    /* redirects */
    CHECK(get(&cl, "GET", URL("/redir/5"), 0, &c, &r, 5000, 0) == 0 && r.status == 200 && r.redirects == 5 && strstr(r.url, "/ola"));
    CHECK(c.responses == 1 && !strcmp((char *)c.body.p, "ola mundo"));
    mnet_resp_free(&r); mnet_buf_free(&c.body); memset(&c, 0, sizeof c);
    CHECK(get(&cl, "GET", URL("/laco"), 0, &c, &r, 5000, 0) == MNET_E_REDIRECTS);
    mnet_resp_free(&r); mnet_buf_free(&c.body); memset(&c, 0, sizeof c);
    CHECK(get(&cl, "POST", URL("/303"), "x=1", &c, &r, 5000, 0) == 0 && !strcmp((char *)c.body.p, "GET"));
    mnet_resp_free(&r); mnet_buf_free(&c.body); memset(&c, 0, sizeof c);
    CHECK(get(&cl, "POST", URL("/307"), "dados", &c, &r, 5000, 0) == 0 && !strcmp((char *)c.body.p, "eco:POST:dados"));
    mnet_resp_free(&r); mnet_buf_free(&c.body); memset(&c, 0, sizeof c);
    c.stop_redirect = 1;                                     /* delegate recusa: recebe o 302 */
    CHECK(get(&cl, "GET", URL("/redir/2"), 0, &c, &r, 5000, 1) == 0 && r.status == 302 && c.redirects_seen == 1 && !strcmp((char *)c.body.p, "vai"));
    mnet_resp_free(&r); mnet_buf_free(&c.body); memset(&c, 0, sizeof c);
    /* gzip e chunked */
    CHECK(get(&cl, "GET", URL("/gzip"), 0, &c, &r, 5000, 0) == 0 && c.body.n == 55000 && !strncmp((char *)c.body.p, "comprimido comprimido", 21));
    mnet_resp_free(&r); mnet_buf_free(&c.body); memset(&c, 0, sizeof c);
    CHECK(get(&cl, "GET", URL("/chunked"), 0, &c, &r, 5000, 0) == 0 && strstr((char *)c.body.p, "pedaco 49;") && !strncmp((char *)c.body.p, "pedaco 0;", 9));
    mnet_resp_free(&r); mnet_buf_free(&c.body); memset(&c, 0, sizeof c);
    /* cookies: Set-Cookie no redirect, mandados no próximo salto */
    CHECK(get(&cl, "GET", URL("/cookie/set"), 0, &c, &r, 5000, 0) == 0 && strstr((char *)c.body.p, "sessao=abc123") && strstr((char *)c.body.p, "tema=escuro"));
    mnet_resp_free(&r); mnet_buf_free(&c.body); memset(&c, 0, sizeof c);
    /* tempo esgotado e erros */
    CHECK(get(&cl, "GET", URL("/lento"), 0, &c, &r, 800, 0) == MNET_E_TIMEOUT);
    mnet_resp_free(&r); mnet_buf_free(&c.body); memset(&c, 0, sizeof c);
    CHECK(get(&cl, "GET", "http://127.0.0.1:1/", 0, &c, &r, 2000, 0) == MNET_E_CONNECT);
    mnet_resp_free(&r);
    CHECK(get(&cl, "GET", "ftp://x/", 0, &c, &r, 2000, 0) == MNET_E_UNSUPPORTED);
    mnet_resp_free(&r);
    CHECK(get(&cl, "GET", "http://nao-existe.invalid/", 0, &c, &r, 2000, 0) == MNET_E_NO_HOST);
    mnet_resp_free(&r); mnet_buf_free(&c.body);
    /* HEAD */
    memset(&c, 0, sizeof c);
    CHECK(get(&cl, "HEAD", URL("/ola"), 0, &c, &r, 5000, 0) == 0 && r.status == 200 && c.body.n == 0);
    mnet_resp_free(&r); mnet_buf_free(&c.body);
    mnet_client_close_idle(&cl);
    mnet_jar_clear(&jar);
    if (srv_pid > 0) kill(srv_pid, SIGTERM);
    pclose(srv);
}

/* WebSocket real: wss://echo.websocket.org (o eco manda uma saudação e depois repete) */
typedef struct { int n; mnet_buf_t last; } wse_t;
static int wse_msg(void *c, int op, const uint8_t *d, size_t n) { (void)op; wse_t *e = c; e->n++; e->last.n = 0; mnet_buf_put(&e->last, d, n); mnet_buf_putc(&e->last, 0); e->last.n--; return 0; }
static void wse_rand(void *c, uint8_t *b, size_t n) { (void)c; FILE *f = fopen("/dev/urandom", "rb"); if (f) { if (fread(b, 1, n, f) != n) memset(b, 7, n); fclose(f); } }
static void ws_rede(mnet_client_t *cl) {
    mnet_wsconn_t w; memset(&w, 0, sizeof w);
    wse_t e = {0, {0}};
    w.ws.ctx = &e; w.ws.on_message = wse_msg; w.ws.random = wse_rand;
    mnet_req_t q = {0};
    q.url = "wss://echo.websocket.org/"; q.timeout_ms = 15000;
    char msg[200];
    int r = mnet_ws_open(cl, &q, 0, &w, msg, sizeof msg);
    printf("  wss://echo.websocket.org -> %d status %d deflate %d %s\n", r, w.resp.status, w.ws.deflate, msg);
    CHECK(r == 0 && w.resp.status == 101);
    if (!r) {
        const char *txt = "olá do mOS sobre TLS";
        mnet_ws_send(&w.ws, MNET_WS_TEXT, txt, strlen(txt));
        mnet_wsconn_flush(&w, 5000);
        for (int i = 0; i < 50 && !(e.last.p && !strcmp((char *)e.last.p, txt)); i++) if (mnet_wsconn_poll(&w, 300) < 0) break;
        CHECK(e.last.p && !strcmp((char *)e.last.p, txt));
        mnet_ws_close(&w.ws, 1000, "fim"); mnet_wsconn_flush(&w, 5000);
        for (int i = 0; i < 20 && !w.ws.close_recv; i++) if (mnet_wsconn_poll(&w, 300) < 0) break;
        CHECK(w.ws.close_recv);
    }
    mnet_wsconn_free(&w); mnet_buf_free(&e.last);
}

static void rede(void) {
    FILE *f = fopen("assets/certs/cacert.pem", "rb");
    if (!f) { printf("  sem assets/certs/cacert.pem\n"); fails++; return; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *pem = malloc((size_t)n + 1); n = (long)fread(pem, 1, (size_t)n, f); pem[n] = 0; fclose(f);
    printf("  raízes: %d\n", x509_trust_load_pem(pem, (u32)n));
    free(pem);
    mnet_client_t cl; mnet_jar_t jar;
    mnet_client_init(&cl, &io); mnet_jar_init(&jar); cl.jar = &jar;
    const char *sites[] = {"https://www.google.com/", "https://www.cloudflare.com/", "https://github.com/", "https://example.com/"};
    for (int i = 0; i < 4; i++) {
        for (int k = 0; k < 2; k++) {
            col_t c = {0}; mnet_resp_t r;
            int e = get(&cl, "GET", sites[i], 0, &c, &r, 15000, 0);
            printf("  %s -> %d status %d alpn %s versão %d corpo %zu reusada %d\n", sites[i], e, r.status, r.alpn, r.version, c.body.n, r.reused);
            CHECK(e == 0 && r.status >= 200 && r.status < 400 && c.body.n > 100);
            if (k) CHECK(r.reused);
            mnet_resp_free(&r); mnet_buf_free(&c.body);
        }
    }
    ws_rede(&cl);
    col_t c = {0}; mnet_resp_t r;
    CHECK(get(&cl, "GET", "https://expired.badssl.com/", 0, &c, &r, 15000, 0) == MNET_E_TLS);
    mnet_resp_free(&r); mnet_buf_free(&c.body);
    mnet_client_close_idle(&cl);
    mnet_jar_clear(&jar);
}

int main(int argc, char **argv) {
    signal(SIGPIPE, SIG_IGN);
    for (int i = 0; i < 64; i++) socks[i] = -1;
    local();
    if (argc > 1 && !strcmp(argv[1], "rede")) rede();
    printf("mnet_client: %d/%d ok\n", checks - fails, checks);
    return fails ? 1 : 0;
}
