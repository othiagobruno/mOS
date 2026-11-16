/* Teste de host dos streams do mnet (userland/libsystem/mnet_stream.c), a base do CFStream/NSStream do mOS.
 *   gcc -std=gnu11 -O1 -g -Wall -DMNET_HOST -DMOS_HOST_TEST -I userland/libsystem -I kernel/include -I tools \
 *       userland/libsystem/mnet_*.c tools/mnet_stream_test.c kernel/net/crypto.c kernel/net/tls.c kernel/net/pk.c \
 *       kernel/net/x509.c -lz -l:libbrotlidec.so.1 -lpthread -lresolv -o /tmp/mnet_stream_test
 *   /tmp/mnet_stream_test          (memória, arquivo, par entre fios, socket com eco local, SOCKS4a/5, erros)
 *   /tmp/mnet_stream_test rede     (da raiz do repositório; + TLS de verdade no example.com, no open e por STARTTLS, e DNS reverso)
 */
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include "tls_shim.h"
#include "x509.h"
#include "mnet.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FALHOU %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

#include "mnet_host_io.inc"

/* transporte com wait (o caminho do mOS): só para TCP sem TLS */
static int h_wait(void *e, void *cv, int ev, int ms) { (void)e; hc_t *c = cv; int r = wait_fd(c->fd, ev == 4 ? POLLOUT : POLLIN, ms); return r < 0 ? -1 : r > 0; }
static mnet_io_t io_wait;

static volatile int notes;
static void note(void *ctx) { (void)ctx; __sync_fetch_and_add(&notes, 1); }

/* ---------------------------------------------------------------- memória e arquivo */
static void t_memory(void) {
    const char *txt = "olá, streams do mOS";
    mnet_stream_t *s = mnet_stream_mem_in(txt, strlen(txt));
    uint8_t b[64];
    CHECK(mnet_stream_status(s) == MNET_SS_NOT_OPEN);
    CHECK(mnet_stream_events(s) == 0);
    CHECK(mnet_stream_read(s, b, 4) == -1);                        /* antes do open */
    mnet_stream_set_notify(s, note, 0);
    notes = 0;
    CHECK(mnet_stream_open(s) == 0);
    CHECK(notes >= 1);
    CHECK(mnet_stream_events(s) == (MNET_SE_OPEN | MNET_SE_BYTES));
    CHECK(mnet_stream_events(s) == 0);                             /* não repete sem read */
    uint8_t *p; size_t n;
    CHECK(mnet_stream_get_buffer(s, &p, &n) && n == strlen(txt) && !memcmp(p, txt, n));
    CHECK(mnet_stream_read(s, b, 5) == 5 && !memcmp(b, "olá,", 5));
    CHECK(mnet_stream_offset(s) == 5);
    CHECK(mnet_stream_events(s) == MNET_SE_BYTES);                 /* rearmado pelo read */
    long k = mnet_stream_read(s, b, sizeof b);
    CHECK(k == (long)strlen(txt) - 5);
    CHECK(mnet_stream_status(s) == MNET_SS_AT_END);
    CHECK(mnet_stream_events(s) == MNET_SE_END);
    CHECK(mnet_stream_events(s) == 0);
    CHECK(mnet_stream_read(s, b, 4) == 0);
    CHECK(!mnet_stream_has_bytes(s));
    CHECK(mnet_stream_seek(s, 1) == 0 && mnet_stream_status(s) == MNET_SS_OPEN && mnet_stream_has_bytes(s));
    CHECK(mnet_stream_read(s, b, 2) == 2 && b[0] == 'l');
    mnet_stream_close(s);
    CHECK(mnet_stream_status(s) == MNET_SS_CLOSED);
    CHECK(mnet_stream_read(s, b, 2) == -1);
    mnet_stream_free(s);

    s = mnet_stream_mem_in("", 0);                                 /* vazio: abre já no fim */
    mnet_stream_open(s);
    CHECK(mnet_stream_events(s) == (MNET_SE_OPEN | MNET_SE_END));
    mnet_stream_free(s);

    s = mnet_stream_mem_out();
    mnet_stream_open(s);
    CHECK(mnet_stream_events(s) == (MNET_SE_OPEN | MNET_SE_SPACE));
    CHECK(mnet_stream_write(s, (const uint8_t *)"abc", 3) == 3);
    CHECK(mnet_stream_events(s) == MNET_SE_SPACE);
    CHECK(mnet_stream_write(s, (const uint8_t *)"defg", 4) == 4);
    size_t dn; const uint8_t *d = mnet_stream_mem_data(s, &dn);
    CHECK(dn == 7 && !memcmp(d, "abcdefg", 7));
    CHECK(mnet_stream_read(s, b, 1) == -1);                        /* escrita não lê */
    mnet_stream_free(s);

    uint8_t fixed[5];
    s = mnet_stream_buf_out(fixed, sizeof fixed);
    mnet_stream_open(s);
    CHECK(mnet_stream_write(s, (const uint8_t *)"123", 3) == 3);
    CHECK(mnet_stream_write(s, (const uint8_t *)"4567", 4) == 2);
    CHECK(!memcmp(fixed, "12345", 5));
    CHECK(mnet_stream_status(s) == MNET_SS_AT_END);
    CHECK(mnet_stream_write(s, (const uint8_t *)"8", 1) == 0);
    CHECK(mnet_stream_events(s) & MNET_SE_END);
    mnet_stream_free(s);
}
static void t_file(void) {
    char path[] = "/tmp/mnet_stream_XXXXXX";
    int fd = mkstemp(path); close(fd);
    mnet_stream_t *w = mnet_stream_file_out(path, 0);
    CHECK(mnet_stream_open(w) == 0);
    CHECK(mnet_stream_write(w, (const uint8_t *)"linha 1\n", 8) == 8);
    CHECK(mnet_stream_offset(w) == 8);
    mnet_stream_free(w);
    w = mnet_stream_file_out(path, 1);                             /* acrescenta */
    mnet_stream_open(w);
    CHECK(mnet_stream_offset(w) == 8);
    CHECK(mnet_stream_write(w, (const uint8_t *)"linha 2\n", 8) == 8);
    mnet_stream_free(w);

    mnet_stream_t *r = mnet_stream_file_in(path);
    CHECK(mnet_stream_seek(r, 6) == 0);                            /* FileCurrentOffset antes do open */
    CHECK(mnet_stream_open(r) == 0);
    CHECK(mnet_stream_events(r) == (MNET_SE_OPEN | MNET_SE_BYTES));
    uint8_t b[64];
    long k = mnet_stream_read(r, b, sizeof b);
    CHECK(k == 10 && !memcmp(b, "1\nlinha 2\n", 10));
    CHECK(mnet_stream_status(r) == MNET_SS_AT_END);
    CHECK(mnet_stream_events(r) == MNET_SE_END);
    CHECK(mnet_stream_seek(r, 0) == 0 && mnet_stream_read(r, b, 5) == 5 && !memcmp(b, "linha", 5));
    mnet_stream_free(r);
    w = mnet_stream_file_out(path, 0);                             /* sem append: trunca */
    mnet_stream_open(w); mnet_stream_write(w, (const uint8_t *)"x", 1); mnet_stream_free(w);
    r = mnet_stream_file_in(path); mnet_stream_open(r);
    CHECK(mnet_stream_read(r, b, sizeof b) == 1);
    mnet_stream_free(r);
    unlink(path);

    r = mnet_stream_file_in("/tmp/nao/existe/mesmo");
    CHECK(mnet_stream_open(r) == -1);
    long code = 0; const char *msg = 0;
    CHECK(mnet_stream_error(r, &code, &msg) == MNET_SD_POSIX && code == ENOENT && msg && *msg);
    CHECK(mnet_stream_events(r) == MNET_SE_ERROR);                 /* sem OpenCompleted */
    mnet_stream_free(r);
}

/* ---------------------------------------------------------------- par ligado entre fios */
typedef struct { mnet_stream_t *w; size_t total; } prod_t;
static void *producer(void *a) {
    prod_t *p = a;
    uint8_t chunk[1000];
    size_t sent = 0;
    while (sent < p->total) {
        size_t n = p->total - sent < sizeof chunk ? p->total - sent : sizeof chunk;
        for (size_t i = 0; i < n; i++) chunk[i] = (uint8_t)((sent + i) * 7);
        size_t off = 0;
        while (off < n) { long k = mnet_stream_write(p->w, chunk + off, n - off); if (k <= 0) return 0; off += (size_t)k; }
        sent += n;
    }
    mnet_stream_close(p->w);
    return 0;
}
static void t_pair(void) {
    mnet_stream_t *r, *w;
    CHECK(mnet_stream_pair(4096, &r, &w) == 0);
    mnet_stream_open(r); mnet_stream_open(w);
    CHECK(mnet_stream_events(r) == MNET_SE_OPEN);                  /* nada para ler ainda */
    CHECK(mnet_stream_events(w) == (MNET_SE_OPEN | MNET_SE_SPACE));
    mnet_stream_set_notify(r, note, 0);
    notes = 0;
    CHECK(mnet_stream_write(w, (const uint8_t *)"oi", 2) == 2);
    CHECK(notes >= 1);                                             /* o leitor foi avisado */
    CHECK(mnet_stream_events(r) == MNET_SE_BYTES);
    uint8_t b[8192];
    CHECK(mnet_stream_read(r, b, sizeof b) == 2 && !memcmp(b, "oi", 2));
    /* 1 MiB passando por um buffer de 4 KiB, com o escritor em outro fio (bloqueia quando cheio) */
    prod_t p = {w, 1 << 20};
    pthread_t t;
    pthread_create(&t, 0, producer, &p);
    size_t got = 0; int bad = 0;
    for (;;) {
        long k = mnet_stream_read(r, b, sizeof b);
        if (k <= 0) { CHECK(k == 0); break; }
        for (long i = 0; i < k; i++) if (b[i] != (uint8_t)((got + (size_t)i) * 7)) bad++;
        got += (size_t)k;
    }
    pthread_join(t, 0);
    CHECK(got == p.total && !bad);
    CHECK(mnet_stream_status(r) == MNET_SS_AT_END);
    mnet_stream_free(r); mnet_stream_free(w);

    /* leitura com prazo */
    mnet_stream_pair(64, &r, &w);
    mnet_stream_open(r); mnet_stream_open(w);
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    CHECK(mnet_stream_read_wait(r, b, 8, 120) == -2);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    long ms = (t1.tv_sec - t0.tv_sec) * 1000 + (t1.tv_nsec - t0.tv_nsec) / 1000000;
    CHECK(ms >= 100 && ms < 1000);
    mnet_stream_write(w, (const uint8_t *)"abc", 3);
    CHECK(mnet_stream_read_wait(r, b, 8, 120) == 3);
    mnet_stream_close(w);
    CHECK(mnet_stream_read_wait(r, b, 8, 120) == 0);
    mnet_stream_free(r); mnet_stream_free(w);

    /* leitor fecha: o escritor recebe EPIPE */
    mnet_stream_pair(16, &r, &w);
    mnet_stream_open(r); mnet_stream_open(w);
    mnet_stream_close(r);
    CHECK(mnet_stream_write(w, (const uint8_t *)"x", 1) == -1);
    long code = 0;
    CHECK(mnet_stream_error(w, &code, 0) == MNET_SD_POSIX && code == 32);
    CHECK(mnet_stream_events(w) & MNET_SE_ERROR);
    mnet_stream_free(w); mnet_stream_free(r);

    /* produtor interno marca erro no leitor (corpo HTTP que falhou) */
    mnet_stream_pair(16, &r, &w);
    mnet_stream_open(r); mnet_stream_open(w);
    mnet_stream_fail(r, MNET_SD_HTTP, -4, "falhou");
    CHECK(mnet_stream_read(r, b, 4) == -1);
    CHECK(mnet_stream_error(r, &code, 0) == MNET_SD_HTTP && code == -4);
    mnet_stream_free(r); mnet_stream_free(w);
}

/* ---------------------------------------------------------------- servidores locais: eco e SOCKS */
static int listen_on(int *port) {
    int s = socket(AF_INET, SOCK_STREAM, 0), one = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bind(s, (void *)&a, sizeof a);
    listen(s, 8);
    socklen_t l = sizeof a; getsockname(s, (void *)&a, &l);
    *port = ntohs(a.sin_port);
    return s;
}
static int full_read(int fd, void *b, size_t n) { size_t g = 0; while (g < n) { ssize_t k = read(fd, (char *)b + g, n - g); if (k <= 0) return -1; g += (size_t)k; } return 0; }
static void *echo_conn(void *a) {
    int c = (int)(long)a;
    char b[4096];
    for (;;) { ssize_t k = read(c, b, sizeof b); if (k <= 0) break; if (!memcmp(b, "tchau", 5)) break; write(c, b, (size_t)k); }
    close(c);
    return 0;
}
static void *echo_server(void *a) {
    int s = (int)(long)a;
    for (;;) { int c = accept(s, 0, 0); if (c < 0) break; pthread_t t; pthread_create(&t, 0, echo_conn, (void *)(long)c); pthread_detach(t); }
    return 0;
}
static int g_echo_port;
static int g_socks_seen;                    /* versão e destino que o proxy recebeu */
static char g_socks_dest[64];
static int connect_local(int port) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK); a.sin_port = htons((uint16_t)port);
    if (connect(s, (void *)&a, sizeof a)) { close(s); return -1; }
    return s;
}
static void relay(int a, int b) {
    struct pollfd p[2] = {{a, POLLIN, 0}, {b, POLLIN, 0}};
    char buf[4096];
    for (;;) {
        if (poll(p, 2, 5000) <= 0) break;
        if (p[0].revents) { ssize_t k = read(a, buf, sizeof buf); if (k <= 0 || write(b, buf, (size_t)k) != k) break; }
        if (p[1].revents) { ssize_t k = read(b, buf, sizeof buf); if (k <= 0 || write(a, buf, (size_t)k) != k) break; }
    }
}
static void *socks_conn(void *x) {
    int c = (int)(long)x, up = -1;
    uint8_t b[600];
    if (full_read(c, b, 1)) goto out;
    if (b[0] == 4) {                                               /* SOCKS4a: VN CD PORT IP USER\0 HOST\0 */
        if (full_read(c, b + 1, 7)) goto out;
        int port = (b[2] << 8) | b[3];
        char user[64] = "", host[64] = ""; size_t i = 0;
        do { if (full_read(c, user + i, 1)) goto out; } while (user[i++] && i < sizeof user - 1);
        i = 0;
        do { if (full_read(c, host + i, 1)) goto out; } while (host[i++] && i < sizeof host - 1);
        g_socks_seen = 4; snprintf(g_socks_dest, sizeof g_socks_dest, "%s:%d/%s", host, port, user);
        up = strcmp(host, "localhost") ? -1 : connect_local(port);
        uint8_t rep[8] = {0, (uint8_t)(up >= 0 ? 0x5a : 0x5b), 0, 0, 0, 0, 0, 0};
        write(c, rep, 8);
    } else if (b[0] == 5) {
        if (full_read(c, b + 1, 1) || full_read(c, b + 2, b[1])) goto out;
        int want_auth = memchr(b + 2, 2, b[1]) != 0;
        uint8_t m[2] = {5, (uint8_t)(want_auth ? 2 : 0)};
        write(c, m, 2);
        if (want_auth) {
            uint8_t ul, pl; char u[256] = "", p[256] = "";
            if (full_read(c, b, 1) || full_read(c, &ul, 1) || full_read(c, u, ul) || full_read(c, &pl, 1) || full_read(c, p, pl)) goto out;
            int ok = !strcmp(u, "ana") && !strcmp(p, "segredo");
            uint8_t r[2] = {1, (uint8_t)(ok ? 0 : 1)};
            write(c, r, 2);
            if (!ok) goto out;
        }
        if (full_read(c, b, 5)) goto out;                          /* VER CMD RSV ATYP LEN */
        uint8_t hl = b[4]; char host[256] = "";
        if (full_read(c, host, hl) || full_read(c, b, 2)) goto out;
        int port = (b[0] << 8) | b[1];
        g_socks_seen = want_auth ? 6 : 5; snprintf(g_socks_dest, sizeof g_socks_dest, "%s:%d", host, port);
        up = strcmp(host, "localhost") ? -1 : connect_local(port);
        uint8_t rep[10] = {5, (uint8_t)(up >= 0 ? 0 : 4), 0, 1, 127, 0, 0, 1, 0, 0};
        write(c, rep, 10);
    }
    if (up >= 0) { relay(c, up); close(up); }
out:
    close(c);
    return 0;
}
static void *socks_server(void *a) {
    int s = (int)(long)a;
    for (;;) { int c = accept(s, 0, 0); if (c < 0) break; pthread_t t; pthread_create(&t, 0, socks_conn, (void *)(long)c); pthread_detach(t); }
    return 0;
}

static int wait_status(mnet_stream_t *s, int not_this, int ms) {
    for (int i = 0; i < ms / 5; i++) { int st = mnet_stream_status(s); if (st != not_this) return st; usleep(5000); }
    return mnet_stream_status(s);
}
static int echo_roundtrip(mnet_stream_t *r, mnet_stream_t *w, const char *msg) {
    size_t n = strlen(msg);
    if (mnet_stream_write(w, (const uint8_t *)msg, n) != (long)n) return 0;
    char b[256]; size_t got = 0;
    while (got < n) { long k = mnet_stream_read(r, (uint8_t *)b + got, sizeof b - got); if (k <= 0) return 0; got += (size_t)k; }
    return got == n && !memcmp(b, msg, n);
}
static void t_socket(const mnet_io_t *io) {
    mnet_stream_t *r, *w;
    CHECK(mnet_stream_socket(io, "localhost", g_echo_port, &r, &w) == 0);
    CHECK(mnet_stream_is_socket(r) && mnet_stream_is_input(r) && !mnet_stream_is_input(w));
    mnet_stream_set_notify(r, note, 0);
    notes = 0;
    CHECK(mnet_stream_open(r) == 0);
    mnet_stream_open(w);
    CHECK(wait_status(w, MNET_SS_OPENING, 3000) == MNET_SS_OPEN);
    CHECK(wait_status(r, MNET_SS_OPENING, 3000) == MNET_SS_OPEN);
    CHECK(notes >= 1);
    CHECK(mnet_stream_events(r) == MNET_SE_OPEN);
    CHECK(mnet_stream_events(w) == (MNET_SE_OPEN | MNET_SE_SPACE));
    CHECK(echo_roundtrip(r, w, "primeira linha\n"));
    CHECK(mnet_stream_events(w) == MNET_SE_SPACE);                 /* rearmado pelo write */
    /* HasBytesAvailable chega pelo fio leitor */
    mnet_stream_write(w, (const uint8_t *)"x", 1);
    for (int i = 0; i < 400 && !mnet_stream_has_bytes(r); i++) usleep(5000);
    CHECK(mnet_stream_has_bytes(r));
    CHECK(mnet_stream_events(r) == MNET_SE_BYTES);
    uint8_t b[16];
    CHECK(mnet_stream_read(r, b, sizeof b) == 1 && b[0] == 'x');
    CHECK(echo_roundtrip(r, w, "segunda"));
    /* o servidor fecha: EndEncountered */
    mnet_stream_write(w, (const uint8_t *)"tchau", 5);
    CHECK(mnet_stream_read(r, b, sizeof b) == 0);
    CHECK(mnet_stream_status(r) == MNET_SS_AT_END);
    CHECK(mnet_stream_events(r) & MNET_SE_END);
    mnet_stream_close(r); mnet_stream_close(w);
    mnet_stream_free(r); mnet_stream_free(w);

    /* write antes de conectar espera a conexão */
    mnet_stream_socket(io, "127.0.0.1", g_echo_port, &r, &w);
    mnet_stream_open(w); mnet_stream_open(r);
    CHECK(echo_roundtrip(r, w, "sem esperar o OpenCompleted"));
    mnet_stream_free(w); mnet_stream_free(r);

    /* porta fechada: erro POSIX ECONNREFUSED (61, como no macOS) */
    int dead_port; int ds = listen_on(&dead_port); close(ds);
    mnet_stream_socket(io, "127.0.0.1", dead_port, &r, &w);
    mnet_stream_open(r);
    CHECK(wait_status(r, MNET_SS_OPENING, 3000) == MNET_SS_ERROR);
    long code = 0;
    CHECK(mnet_stream_error(r, &code, 0) == MNET_SD_POSIX && code == 61);
    CHECK(mnet_stream_events(r) == MNET_SE_ERROR);
    CHECK(mnet_stream_read(r, b, 1) == -1);
    mnet_stream_open(w);
    CHECK(mnet_stream_status(w) == MNET_SS_ERROR);                 /* o outro lado já nasce com o erro */
    CHECK(mnet_stream_write(w, b, 1) == -1);
    mnet_stream_free(r); mnet_stream_free(w);

    /* nome que não existe: domínio NetDB */
    mnet_stream_socket(io, "nao-existe.invalid", 80, &r, &w);
    mnet_stream_open(r);
    CHECK(wait_status(r, MNET_SS_OPENING, 8000) == MNET_SS_ERROR);
    CHECK(mnet_stream_error(r, &code, 0) == MNET_SD_NETDB);
    mnet_stream_free(r); mnet_stream_free(w);

    /* conexão já aberta pelo app (CFStreamCreatePairWithSocket) */
    int dummy = 0;
    void *conn = io->connect(io->env, "127.0.0.1", g_echo_port, 3000, &dummy);
    CHECK(conn != 0);
    CHECK(mnet_stream_socket_conn(io, conn, 1, &r, &w) == 0);
    mnet_stream_open(r); mnet_stream_open(w);
    CHECK(mnet_stream_status(r) == MNET_SS_OPEN && mnet_stream_conn(r) == conn);
    CHECK(mnet_stream_events(r) == MNET_SE_OPEN);
    CHECK(echo_roundtrip(r, w, "nativo"));
    mnet_stream_free(r); mnet_stream_free(w);
}
static void t_socks(const mnet_io_t *io, int socks_port) {
    mnet_stream_t *r, *w;
    for (int v = 0; v < 3; v++) {
        mnet_stream_socket(io, "localhost", g_echo_port, &r, &w);
        mnet_sockopts_t *o = mnet_stream_sockopts(r);
        CHECK(o && o == mnet_stream_sockopts(w));
        snprintf(o->socks_host, sizeof o->socks_host, "127.0.0.1");
        o->socks_port = socks_port;
        o->socks_version = v == 0 ? 4 : 5;
        if (v == 2) { strcpy(o->socks_user, "ana"); strcpy(o->socks_pass, "segredo"); }
        if (v == 0) strcpy(o->socks_user, "eu");
        g_socks_seen = 0;
        mnet_stream_open(r); mnet_stream_open(w);
        CHECK(echo_roundtrip(r, w, "pelo proxy"));
        char want[64];
        if (v == 0) snprintf(want, sizeof want, "localhost:%d/eu", g_echo_port);
        else snprintf(want, sizeof want, "localhost:%d", g_echo_port);
        CHECK(g_socks_seen == (v == 0 ? 4 : v == 1 ? 5 : 6) && !strcmp(g_socks_dest, want));
        mnet_stream_free(r); mnet_stream_free(w);
    }
    /* senha errada e destino recusado: erro no domínio SOCKS */
    mnet_stream_socket(io, "localhost", g_echo_port, &r, &w);
    mnet_sockopts_t *o = mnet_stream_sockopts(r);
    strcpy(o->socks_host, "127.0.0.1"); o->socks_port = socks_port; o->socks_version = 5;
    strcpy(o->socks_user, "ana"); strcpy(o->socks_pass, "errada");
    mnet_stream_open(r);
    CHECK(wait_status(r, MNET_SS_OPENING, 3000) == MNET_SS_ERROR);
    long code = 0;
    CHECK(mnet_stream_error(r, &code, 0) == MNET_SD_SOCKS && code == ((5 << 16) | 1));   /* usuário/senha recusados */
    mnet_stream_free(r); mnet_stream_free(w);
    mnet_stream_socket(io, "outro.host", 1, &r, &w);
    o = mnet_stream_sockopts(r);
    strcpy(o->socks_host, "127.0.0.1"); o->socks_port = socks_port; o->socks_version = 5;
    mnet_stream_open(r);
    CHECK(wait_status(r, MNET_SS_OPENING, 3000) == MNET_SS_ERROR);
    CHECK(mnet_stream_error(r, &code, 0) == MNET_SD_SOCKS && code == 4);     /* resposta 4: host inalcançável */
    mnet_stream_free(r); mnet_stream_free(w);
}

/* ---------------------------------------------------------------- TLS de verdade */
static int http_get(mnet_stream_t *r, mnet_stream_t *w, const char *host, char *out, size_t cap) {
    char req[256];
    int n = snprintf(req, sizeof req, "GET / HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", host);
    if (mnet_stream_write(w, (const uint8_t *)req, (size_t)n) != n) return -1;
    size_t got = 0;
    for (;;) {
        long k = mnet_stream_read(r, (uint8_t *)out + got, cap - 1 - got);
        if (k <= 0) break;
        got += (size_t)k;
        if (got >= cap - 1) break;
    }
    out[got] = 0;
    return (int)got;
}
static void rede(void) {
    static char page[1 << 16];
    FILE *f = fopen("assets/certs/cacert.pem", "rb");
    if (!f) { printf("  sem assets/certs/cacert.pem (rode da raiz do repositório)\n"); fails++; return; }
    fseek(f, 0, SEEK_END); long pn = ftell(f); fseek(f, 0, SEEK_SET);
    char *pem = malloc((size_t)pn + 1); pn = (long)fread(pem, 1, (size_t)pn, f); pem[pn] = 0; fclose(f);
    x509_trust_load_pem(pem, (u32)pn);
    free(pem);
    mnet_stream_t *r, *w;
    /* TLS pedido antes do open */
    mnet_stream_socket(&io, "example.com", 443, &r, &w);
    mnet_stream_sockopts(r)->tls = 1;
    mnet_stream_open(r); mnet_stream_open(w);
    CHECK(wait_status(r, MNET_SS_OPENING, 15000) == MNET_SS_OPEN);
    CHECK(mnet_stream_tls_active(r));
    int n = http_get(r, w, "example.com", page, sizeof page);
    CHECK(n > 0 && !strncmp(page, "HTTP/1.1 200", 12) && strstr(page, "Example Domain"));
    CHECK(mnet_stream_status(r) == MNET_SS_AT_END);
    mnet_stream_free(r); mnet_stream_free(w);
    /* STARTTLS: abre em claro e liga o TLS depois */
    mnet_stream_socket(&io, "example.com", 443, &r, &w);
    mnet_stream_open(r); mnet_stream_open(w);
    CHECK(wait_status(r, MNET_SS_OPENING, 15000) == MNET_SS_OPEN);
    CHECK(!mnet_stream_tls_active(r));
    mnet_stream_starttls(w);
    n = http_get(r, w, "example.com", page, sizeof page);
    CHECK(mnet_stream_tls_active(r));
    CHECK(n > 0 && !strncmp(page, "HTTP/1.1 200", 12));
    mnet_stream_free(r); mnet_stream_free(w);
    /* nome errado no certificado: erro no domínio SSL (-9807) */
    mnet_stream_socket(&io, "example.com", 443, &r, &w);
    mnet_sockopts_t *o = mnet_stream_sockopts(r);
    o->tls = 1; strcpy(o->peer_name, "outro-nome.example");
    mnet_stream_open(r);
    CHECK(wait_status(r, MNET_SS_OPENING, 15000) == MNET_SS_ERROR);
    long code = 0;
    CHECK(mnet_stream_error(r, &code, 0) == MNET_SD_SSL);
    printf("  (erro TLS esperado: %ld)\n", code);
    mnet_stream_free(r); mnet_stream_free(w);
    /* DNS reverso de verdade (o caminho do CFHost kCFHostNames): 8.8.8.8 -> dns.google */
    {
        extern int res_query(const char *, int, int, unsigned char *, int);
        uint8_t sa[16] = {16, 2, 0, 0, 8, 8, 8, 8};
        char q[80], nm[256];
        uint8_t ans[1024];
        CHECK(!mnet_dns_ptr_name(sa, 16, q, sizeof q));
        int len = res_query(q, 1, 12, ans, sizeof ans);
        CHECK(len > 12 && !mnet_dns_first_ptr(ans, (size_t)len, nm, sizeof nm) && !strcmp(nm, "dns.google"));
    }
}

/* ---------------------------------------------------------------- corpo HTTP por um par (o stream de CFReadStreamCreateForHTTPRequest) */
static void *http_conn(void *a) {
    int c = (int)(long)a;
    char req[2048]; size_t n = 0;
    while (n < sizeof req - 1) { ssize_t k = read(c, req + n, sizeof req - 1 - n); if (k <= 0) break; n += (size_t)k; req[n] = 0; if (strstr(req, "\r\n\r\n")) break; }
    if (!strncmp(req, "GET /grande", 11)) {
        size_t len = 600 * 1024;
        char h[128]; int hl = snprintf(h, sizeof h, "HTTP/1.1 200 OK\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n", len);
        write(c, h, (size_t)hl);
        char *b = malloc(len);
        for (size_t i = 0; i < len; i++) b[i] = (char)(i * 13);
        size_t off = 0;
        while (off < len) { ssize_t k = write(c, b + off, len - off); if (k <= 0) break; off += (size_t)k; }
        free(b);
    } else {
        const char *r = "HTTP/1.1 404 Not Found\r\nContent-Length: 3\r\nX-Teste: sim\r\nConnection: close\r\n\r\nnao";
        write(c, r, strlen(r));
    }
    close(c);
    return 0;
}
static void *http_server(void *a) {
    int s = (int)(long)a;
    for (;;) { int c = accept(s, 0, 0); if (c < 0) break; pthread_t t; pthread_create(&t, 0, http_conn, (void *)(long)c); pthread_detach(t); }
    return 0;
}
typedef struct { mnet_client_t *cl; const char *url; mnet_stream_t *w; int err, status; char hdr[32]; } hjob_t;
static int hj_resp(void *ctx, const mnet_resp_t *r) {
    hjob_t *j = ctx; j->status = r->status;
    const char *x = mnet_hdrs_get(&r->hdrs, "x-teste");
    snprintf(j->hdr, sizeof j->hdr, "%s", x ? x : "");
    return mnet_stream_peer_gone(j->w);
}
static int hj_body(void *ctx, const uint8_t *d, size_t n) {
    hjob_t *j = ctx; size_t off = 0;
    while (off < n) { long k = mnet_stream_write(j->w, d + off, n - off); if (k <= 0) return 1; off += (size_t)k; }
    return 0;
}
static void *hj_thread(void *a) {
    hjob_t *j = a;
    mnet_req_t q; mnet_resp_t r; char msg[128];
    memset(&q, 0, sizeof q); memset(&r, 0, sizeof r);
    q.method = "GET"; q.url = j->url; q.max_redirects = -1; q.no_cookies = 1;
    q.ctx = j; q.on_response = hj_resp; q.on_body = hj_body;
    j->err = mnet_fetch(j->cl, &q, &r, msg, sizeof msg);
    if (j->err && j->err != MNET_E_CANCELLED && !mnet_stream_peer_gone(j->w)) mnet_stream_fail_peer(j->w, MNET_SD_POSIX, 61, msg);
    mnet_resp_free(&r);
    mnet_stream_close(j->w);
    return 0;
}
static void t_http_pipe(void) {
    int hp, hs = listen_on(&hp);
    pthread_t t;
    pthread_create(&t, 0, http_server, (void *)(long)hs); pthread_detach(t);
    mnet_client_t cl; mnet_client_init(&cl, &io_wait);
    char url[128];
    /* 600 KiB por um par de 256 KiB: o fio do fetch espera o leitor */
    mnet_stream_t *r, *w;
    mnet_stream_pair(256 * 1024, &r, &w);
    mnet_stream_open(r); mnet_stream_open(w);
    snprintf(url, sizeof url, "http://127.0.0.1:%d/grande", hp);
    hjob_t j = {&cl, url, w, 0, 0, ""};
    pthread_create(&t, 0, hj_thread, &j);
    static uint8_t b[70000];
    size_t got = 0; int bad = 0;
    for (;;) {
        long k = mnet_stream_read(r, b, sizeof b);
        if (k <= 0) { CHECK(k == 0); break; }
        for (long i = 0; i < k; i++) if (b[i] != (uint8_t)((got + (size_t)i) * 13)) bad++;
        got += (size_t)k;
    }
    pthread_join(t, 0);
    CHECK(j.err == 0 && j.status == 200 && got == 600 * 1024 && !bad);
    CHECK(mnet_stream_status(r) == MNET_SS_AT_END);
    mnet_stream_free(r); mnet_stream_free(w);
    /* resposta 404 com cabeçalho: o corpo chega e o status fica com o produtor */
    mnet_stream_pair(4096, &r, &w);
    mnet_stream_open(r); mnet_stream_open(w);
    snprintf(url, sizeof url, "http://127.0.0.1:%d/x", hp);
    hjob_t j2 = {&cl, url, w, 0, 0, ""};
    pthread_create(&t, 0, hj_thread, &j2);
    long k = mnet_stream_read(r, b, sizeof b);
    pthread_join(t, 0);
    CHECK(k == 3 && !memcmp(b, "nao", 3) && j2.status == 404 && !strcmp(j2.hdr, "sim"));
    CHECK(mnet_stream_read(r, b, sizeof b) == 0);
    mnet_stream_free(r); mnet_stream_free(w);
    /* o app larga o stream no meio: o fetch é cancelado (sem travar) */
    mnet_stream_pair(4096, &r, &w);
    mnet_stream_open(r); mnet_stream_open(w);
    snprintf(url, sizeof url, "http://127.0.0.1:%d/grande", hp);
    hjob_t j3 = {&cl, url, w, 0, 0, ""};
    pthread_create(&t, 0, hj_thread, &j3);
    CHECK(mnet_stream_read(r, b, 100) == 100);
    mnet_stream_free(r);
    pthread_join(t, 0);
    CHECK(j3.err == MNET_E_CANCELLED);
    mnet_stream_free(w);
    /* conexão recusada: o erro chega ao leitor */
    int dead; int ds = listen_on(&dead); close(ds);
    mnet_stream_pair(4096, &r, &w);
    mnet_stream_open(r); mnet_stream_open(w);
    snprintf(url, sizeof url, "http://127.0.0.1:%d/", dead);
    hjob_t j4 = {&cl, url, w, 0, 0, ""};
    pthread_create(&t, 0, hj_thread, &j4);
    CHECK(mnet_stream_read(r, b, 10) == -1);
    pthread_join(t, 0);
    long code = 0;
    CHECK(j4.err == MNET_E_CONNECT && mnet_stream_error(r, &code, 0) == MNET_SD_POSIX && code == 61);
    CHECK(mnet_stream_events(r) == (MNET_SE_OPEN | MNET_SE_ERROR));
    mnet_stream_free(r); mnet_stream_free(w);
    mnet_client_close_idle(&cl);
}

int main(int argc, char **argv) {
    signal(SIGPIPE, SIG_IGN);
    for (int i = 0; i < 64; i++) socks[i] = -1;
    io_wait = io; io_wait.wait = h_wait;
    t_memory();
    t_file();
    t_pair();
    pthread_t t;
    int es = listen_on(&g_echo_port);
    pthread_create(&t, 0, echo_server, (void *)(long)es); pthread_detach(t);
    int sp, ss = listen_on(&sp);
    pthread_create(&t, 0, socks_server, (void *)(long)ss); pthread_detach(t);
    t_socket(&io);           /* leitura com tempo (transporte sem wait) */
    t_socket(&io_wait);      /* com wait, como no mOS */
    t_socks(&io_wait, sp);
    t_http_pipe();
    if (argc > 1 && !strcmp(argv[1], "rede")) rede();
    printf("mnet_stream: %d/%d ok\n", checks - fails, checks);
    return fails ? 1 : 0;
}
