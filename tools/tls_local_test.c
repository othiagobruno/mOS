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

static const char *srv_extra[6];   /* argumentos extras do s_server (ALPN, versão) */
static pid_t serve(const char *cert, const char *key, int port) {
    pid_t p = fork();
    if (p < 0) return -1;
    if (p == 0) {
        char addr[64];
        snprintf(addr, sizeof addr, "127.0.0.1:%d", port);
        int fd = open("/tmp/mos-tls-s_server.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd >= 0) { dup2(fd, 1); dup2(fd, 2); }
        const char *av[20] = {"openssl", "s_server", "-accept", addr, "-cert", cert, "-key", key, "-www", "-quiet"};
        int ac = 10;
        for (int i = 0; i < 6 && srv_extra[i]; i++) av[ac++] = srv_extra[i];
        av[ac] = 0;
        execvp("openssl", (char *const *)av);
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

static const u8 *cli_alpn; static u32 cli_alpn_len;   /* lista ALPN do cliente (0 = padrão) */
static u8 *cli_chain, *cli_key; static u32 cli_chain_len, cli_key_len;   /* certificado de cliente (0 = nenhum) */
static int cli_id_ret;
static bool cli_skip; static int got_chain;          /* tls_set_skip_chain e tamanho da cadeia guardada */
static char got_alpn[33]; static int got_ver;
static int handshake(int port, const char *sni, char *err, size_t errlen, int *http_ok) {
    int s = tcp_connect(port);
    if (s < 0) { snprintf(err, errlen, "tcp"); return -1; }
    tls_conn_t *c = tls_new(s, sni);
    if (cli_alpn) tls_set_alpn(c, cli_alpn, cli_alpn_len);
    if (cli_skip) tls_set_skip_chain(c, true);
    cli_id_ret = cli_chain ? tls_set_client_identity(c, cli_chain, cli_chain_len, cli_key, cli_key_len) : 0;
    u64 t0 = timer_ticks();
    int r;
    while ((r = tls_handshake(c)) == 0) {
        if (timer_ticks() - t0 > 8000) break;
        usleep(1000);
    }
    snprintf(got_alpn, sizeof got_alpn, "%s", tls_get_alpn(c));
    got_ver = tls_get_version(c);
    got_chain = tls_copy_peer_chain(c, NULL, 0);
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

    /* tls_set_skip_chain (MOS_NET_TLS_NOVERIFY do SecureTransport): a cadeia não é validada no kernel, mas é
     * guardada para o app validar; a assinatura do handshake continua conferida */
    cli_skip = true;
    p = serve("/tmp/mos-tls-test/unknown.pem", "/tmp/mos-tls-test/srv.key", 18452);
    hr = p > 0 ? handshake(18452, "localhost", err, sizeof err, &http) : -1;
    check("sem validar no kernel: AC desconhecida passa e a cadeia fica guardada", hr == 0 && got_chain > 0);
    if (hr) printf("    motivo: %s\n", err);
    stop(p);
    p = serve("/tmp/mos-tls-test/badname.pem", "/tmp/mos-tls-test/srv.key", 18453);
    hr = p > 0 ? handshake(18453, "localhost", err, sizeof err, &http) : -1;
    check("sem validar no kernel: nome errado passa", hr == 0 && got_chain > 0);
    stop(p);
    cli_skip = false;
    p = serve("/tmp/mos-tls-test/unknown.pem", "/tmp/mos-tls-test/srv.key", 18454);
    hr = p > 0 ? handshake(18454, "localhost", err, sizeof err, &http) : -1;
    check("sessão seguinte volta a validar", hr != 0 && strstr(err, "não confiável") != NULL);
    stop(p);

    /* ALPN (RFC 7301) em 1.3 e 1.2 */
    static const u8 h2h1[] = {2, 'h', '2', 8, 'h', 't', 't', 'p', '/', '1', '.', '1'};
    static const u8 only_x[] = {3, 'x', 'y', 'z'};
    const char *vers[2] = {"-tls1_3", "-tls1_2"};
    for (int v = 0; v < 2; v++) {
        srv_extra[0] = "-alpn"; srv_extra[1] = "h2,http/1.1"; srv_extra[2] = vers[v]; srv_extra[3] = 0;
        p = serve("/tmp/mos-tls-test/srv.pem", "/tmp/mos-tls-test/srv.key", 18447 + v);
        cli_alpn = h2h1; cli_alpn_len = sizeof h2h1;
        hr = p > 0 ? handshake(18447 + v, "localhost", err, sizeof err, NULL) : -1;
        check(v ? "ALPN h2 negociado (TLS 1.2)" : "ALPN h2 negociado (TLS 1.3)",
              hr == 0 && !strcmp(got_alpn, "h2") && got_ver == (v ? 0x0303 : 0x0304));
        if (hr) printf("    motivo: %s\n", err);
        stop(p);
    }
    srv_extra[0] = "-alpn"; srv_extra[1] = "http/1.1"; srv_extra[2] = 0;
    p = serve("/tmp/mos-tls-test/srv.pem", "/tmp/mos-tls-test/srv.key", 18449);
    cli_alpn = h2h1; cli_alpn_len = sizeof h2h1;
    hr = p > 0 ? handshake(18449, "localhost", err, sizeof err, NULL) : -1;
    check("ALPN cai para http/1.1 quando o servidor não tem h2", hr == 0 && !strcmp(got_alpn, "http/1.1"));
    stop(p);
    srv_extra[0] = 0;
    p = serve("/tmp/mos-tls-test/srv.pem", "/tmp/mos-tls-test/srv.key", 18450);
    cli_alpn = only_x; cli_alpn_len = sizeof only_x;
    hr = p > 0 ? handshake(18450, "localhost", err, sizeof err, NULL) : -1;
    check("servidor sem ALPN: handshake segue, nenhum protocolo", hr == 0 && got_alpn[0] == 0);
    stop(p);
    cli_alpn = 0;

    /* certificado de cliente: o s_server exige (-Verify) e confere a cadeia e a assinatura */
    check("certificados de cliente gerados", run(
        "cd /tmp/mos-tls-test && printf 'extendedKeyUsage=clientAuth\\n' > cli.ext && "
        "openssl genpkey -algorithm EC -pkeyopt ec_paramgen_curve:P-256 -out ec256.key 2>/dev/null && "
        "openssl genpkey -algorithm EC -pkeyopt ec_paramgen_curve:P-384 -out ec384.key 2>/dev/null && "
        "openssl genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:2048 -out rsa.key 2>/dev/null && "
        "for k in ec256 ec384 rsa; do openssl req -new -key $k.key -subj /CN=cliente-$k -out $k.csr 2>/dev/null && "
        "openssl x509 -req -in $k.csr -CA ca.pem -CAkey ca.key -set_serial 7$RANDOM -days 30 -extfile cli.ext -out $k.pem 2>/dev/null && "
        "openssl x509 -in $k.pem -outform DER -out $k.crt.der && openssl pkey -in $k.key -outform DER -out $k.p8.der || exit 1; done && "
        "openssl ec -in ec256.key -outform DER -out ec256.sec1.der 2>/dev/null && "
        "openssl rsa -in rsa.key -traditional -outform DER -out rsa.p1.der 2>/dev/null && cd - >/dev/null && "
        "python3 -I tools/tls_x963.py /tmp/mos-tls-test/ec384.key /tmp/mos-tls-test/ec384.x963"));
    static const char *ids[][3] = {{"ec256", "ec256.p8.der", "EC P-256 (PKCS#8)"}, {"ec256", "ec256.sec1.der", "EC P-256 (SEC1)"},
                                   {"ec384", "ec384.p8.der", "EC P-384"}, {"rsa", "rsa.p8.der", "RSA 2048 (PKCS#8)"},
                                   {"rsa", "rsa.p1.der", "RSA 2048 (PKCS#1)"}, {"ec384", "ec384.x963", "EC P-384 (X9.63 do SecKey)"}};
    for (int v = 0; v < 2; v++) {
        const char *ex[6] = {"-Verify", "1", "-CAfile", "/tmp/mos-tls-test/ca.pem", v ? "-tls1_2" : "-tls1_3", 0};
        for (int i = 0; i < 6; i++) srv_extra[i] = ex[i];
        p = serve("/tmp/mos-tls-test/srv.pem", "/tmp/mos-tls-test/srv.key", 18450 + v);
        for (int k = 0; k < 6; k++) {
            char path[128]; FILE *ff;
            u8 der[4096]; size_t dl;
            snprintf(path, sizeof path, "/tmp/mos-tls-test/%s.crt.der", ids[k][0]);
            ff = fopen(path, "rb"); dl = ff ? fread(der, 1, sizeof der, ff) : 0; if (ff) fclose(ff);
            static u8 chainbuf[4200], keybuf[4096];
            chainbuf[0] = 0; chainbuf[1] = 0; chainbuf[2] = 0; chainbuf[3] = 1;
            chainbuf[4] = (u8)(dl >> 24); chainbuf[5] = (u8)(dl >> 16); chainbuf[6] = (u8)(dl >> 8); chainbuf[7] = (u8)dl;
            memcpy(chainbuf + 8, der, dl);
            snprintf(path, sizeof path, "/tmp/mos-tls-test/%s", ids[k][1]);
            ff = fopen(path, "rb"); size_t kl = ff ? fread(keybuf, 1, sizeof keybuf, ff) : 0; if (ff) fclose(ff);
            cli_chain = chainbuf; cli_chain_len = (u32)(8 + dl); cli_key = keybuf; cli_key_len = (u32)kl;
            hr = handshake(18450 + v, "localhost", err, sizeof err, &http);
            char name[96];
            snprintf(name, sizeof name, "certificado de cliente %s, TLS 1.%d", ids[k][2], v ? 2 : 3);
            check(name, dl && kl && cli_id_ret == 0 && hr == 0 && http == 1 && got_ver == (v ? 0x0303 : 0x0304));
            if (hr) printf("        (%s)\n", err);
        }
        /* chave de outro certificado: recusada já no tls_set_client_identity */
        FILE *ff = fopen("/tmp/mos-tls-test/rsa.p8.der", "rb");   /* a cadeia em uso é a do último caso (EC P-384) */
        static u8 other[2048]; size_t ol = ff ? fread(other, 1, sizeof other, ff) : 0; if (ff) fclose(ff);
        cli_key = other; cli_key_len = (u32)ol;
        hr = handshake(18450 + v, "localhost", err, sizeof err, &http);
        check(v ? "chave que não é do certificado é recusada (1.2)" : "chave que não é do certificado é recusada (1.3)", cli_id_ret != 0 && (hr != 0 || http == 0));
        cli_chain = 0;
        hr = handshake(18450 + v, "localhost", err, sizeof err, &http);
        check(v ? "sem certificado o servidor recusa (1.2)" : "sem certificado o servidor recusa (1.3)", hr != 0 || http == 0);
        stop(p);
    }
    for (int i = 0; i < 6; i++) srv_extra[i] = 0;

    printf("tls_local: %d/%d\n", passes, passes + fails);
    return fails ? 1 : 0;
}
