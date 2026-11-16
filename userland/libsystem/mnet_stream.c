/*
 * mOS — mnet: streams de bytes do CFStream/NSStream sem ObjC (docs/rede-apps.md, item 9).
 *
 * Tipos: memória (entrada), arquivo (entrada e saída), memória crescente e buffer fixo (saída), par ligado
 * (CFStreamCreateBoundPair; também o corpo de CFReadStreamCreateForHTTPRequest) e socket TCP (host:porta ou conexão
 * já aberta) com SOCKS4/5 e TLS antes ou depois de abrir (STARTTLS). A camada ObjC (cfnetstream.c) só traduz.
 *
 * Eventos seguem o CFStream: OpenCompleted uma vez; HasBytesAvailable/CanAcceptBytes uma vez e de novo só depois de
 * um read/write; EndEncountered e ErrorOccurred uma vez. mnet_stream_events() devolve o que entregar agora; o
 * callback notify avisa (de qualquer fio) que o estado mudou.
 *
 * Socket: um fio por conexão conecta (proxy SOCKS, TLS) e depois lê para um buffer interno (até 256 KiB); read do
 * app consome desse buffer e bloqueia se vazio, como no macOS. write vai direto ao transporte, sob uma trava
 * compartilhada com o fio leitor.
 */
#include "mnet.h"

#ifdef MNET_HOST
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
typedef pthread_mutex_t ms_mutex_t;
typedef pthread_cond_t ms_cond_t;
typedef struct timespec ms_ts_t;
#define MS_REALTIME CLOCK_REALTIME
#define TS_SEC(t) (t).tv_sec
#define TS_NSEC(t) (t).tv_nsec
#include <time.h>
#define O_RD O_RDONLY
#define O_WR_NEW (O_WRONLY | O_CREAT | O_TRUNC)
#define O_WR_APP (O_WRONLY | O_CREAT | O_APPEND)
static int ms_errno(void) { return errno; }
#else
typedef struct { _Alignas(8) unsigned char b[64]; } ms_mutex_t;    /* pthread_mutex_t do macOS */
typedef struct { _Alignas(8) unsigned char b[48]; } ms_cond_t;     /* pthread_cond_t do macOS */
int pthread_mutex_init(void *m, const void *a);
int pthread_mutex_lock(void *m);
int pthread_mutex_unlock(void *m);
int pthread_mutex_destroy(void *m);
int pthread_cond_init(void *c, const void *a);
int pthread_cond_wait(void *c, void *m);
int pthread_cond_timedwait(void *c, void *m, const void *abstime);
typedef struct { long long sec, nsec; } ms_ts_t;
int clock_gettime(int clk, ms_ts_t *t);
#define MS_REALTIME 0
#define TS_SEC(t) (t).sec
#define TS_NSEC(t) (t).nsec
int pthread_cond_broadcast(void *c);
int pthread_cond_destroy(void *c);
int pthread_create(unsigned long *t, const void *attr, void *(*fn)(void *), void *arg);
int pthread_detach(unsigned long t);
int usleep(unsigned us);
int open(const char *p, int fl, ...);
long read(int fd, void *b, unsigned long n);
long write(int fd, const void *b, unsigned long n);
long long lseek(int fd, long long off, int wh);
int close(int fd);
int *__error(void);
int snprintf(char *b, size_t n, const char *f, ...);
#define O_RD 0
#define O_WR_NEW (1 | 0x200 | 0x400)          /* O_WRONLY | O_CREAT | O_TRUNC */
#define O_WR_APP (1 | 0x200 | 0x8)            /* O_WRONLY | O_CREAT | O_APPEND */
static int ms_errno(void) { return *__error(); }
typedef unsigned long pthread_t;
#endif

#define RX_MAX (256u * 1024u)
enum { K_MEM_IN, K_FILE_IN, K_MEM_OUT, K_BUF_OUT, K_FILE_OUT, K_PAIR_IN, K_PAIR_OUT, K_SOCK_IN, K_SOCK_OUT };
enum { C_IDLE, C_CONNECTING, C_UP, C_FAILED, C_DOWN };

typedef struct ms_shared {
    ms_mutex_t mu;
    ms_cond_t cv;
    int refs;
    /* par: ring simples (buffer linear com consumo pelo começo) */
    mnet_buf_t buf;
    size_t cap;
    int w_closed, r_closed;            /* lado de escrita fechou (fim), leitor fechou (EPIPE) */
    /* socket */
    const mnet_io_t *io;
    void *conn;
    int cstate;
    int rx_eof, rx_err;
    int tls_pending, tls_busy, tls_on;
    int err_domain; long err_code; char err_msg[160];
    char host[256]; int port;
    mnet_sockopts_t opt;
    int opened;                        /* algum lado já pediu open (o fio já saiu) */
    int closing;
    mnet_stream_t *rd, *wr;            /* para avisar (sem posse) */
    ms_mutex_t io_mu;                  /* serializa read/write/starttls no transporte */
} ms_shared_t;

struct mnet_stream {
    int kind, status;
    int done;                          /* eventos de uma vez já entregues */
    int bytes_armed, space_armed;
    int opened_ok;
    int err_domain; long err_code; char err_msg[160];
    /* memória de entrada */
    uint8_t *data; size_t len, pos;
    /* arquivo */
    int fd; char *path; int append; long long fsize, foff, want_off;
    /* saída em memória / buffer do app */
    mnet_buf_t mem;
    uint8_t *ubuf; size_t ucap, upos;
    ms_shared_t *sh;
    void (*notify)(void *ctx);
    void *nctx;
    void (*on_open)(void *ctx, mnet_stream_t *s);
    void *octx;
};

static void lk(ms_shared_t *h) { pthread_mutex_lock(&h->mu); }
static void ulk(ms_shared_t *h) { pthread_mutex_unlock(&h->mu); }
static void wake(ms_shared_t *h) { pthread_cond_broadcast(&h->cv); }
static void cpys(char *d, size_t n, const char *s) {
    size_t l = s ? strlen(s) : 0;
    if (l >= n) l = n - 1;
    if (l) memcpy(d, s, l);
    d[l] = 0;
}
static void set_err(mnet_stream_t *s, int dom, long code, const char *msg) {
    s->status = MNET_SS_ERROR; s->err_domain = dom; s->err_code = code;
    cpys(s->err_msg, sizeof s->err_msg, msg ? msg : "");
}
static void ping(mnet_stream_t *s) { if (s && s->notify) s->notify(s->nctx); }

static mnet_stream_t *new_stream(int kind) {
    mnet_stream_t *s = calloc(1, sizeof *s);
    if (!s) return 0;
    s->kind = kind; s->fd = -1; s->bytes_armed = s->space_armed = 1; s->want_off = -1;
    return s;
}
static ms_shared_t *new_shared(void) {
    ms_shared_t *h = calloc(1, sizeof *h);
    if (!h) return 0;
    pthread_mutex_init(&h->mu, 0);
    pthread_mutex_init(&h->io_mu, 0);
    pthread_cond_init(&h->cv, 0);
    return h;
}
static void shared_unref(ms_shared_t *h) {
    if (!h) return;
    lk(h);
    int left = --h->refs;
    ulk(h);
    if (left > 0) return;
    if (h->conn && h->io) h->io->close(h->io->env, h->conn);
    mnet_buf_free(&h->buf);
    pthread_mutex_destroy(&h->mu);
    pthread_mutex_destroy(&h->io_mu);
    pthread_cond_destroy(&h->cv);
    free(h);
}

/* ---------------------------------------------------------------- criação */
mnet_stream_t *mnet_stream_mem_in(const void *d, size_t n) {
    mnet_stream_t *s = new_stream(K_MEM_IN);
    if (!s) return 0;
    s->data = malloc(n ? n : 1);
    if (!s->data) { free(s); return 0; }
    if (n) memcpy(s->data, d, n);
    s->len = n;
    return s;
}
mnet_stream_t *mnet_stream_file_in(const char *path) {
    mnet_stream_t *s = new_stream(K_FILE_IN);
    if (!s) return 0;
    s->path = mnet_strndup(path ? path : "", path ? strlen(path) : 0);
    return s;
}
mnet_stream_t *mnet_stream_mem_out(void) { return new_stream(K_MEM_OUT); }
mnet_stream_t *mnet_stream_buf_out(void *buf, size_t cap) {
    mnet_stream_t *s = new_stream(K_BUF_OUT);
    if (s) { s->ubuf = buf; s->ucap = buf ? cap : 0; }
    return s;
}
mnet_stream_t *mnet_stream_file_out(const char *path, int append) {
    mnet_stream_t *s = new_stream(K_FILE_OUT);
    if (!s) return 0;
    s->path = mnet_strndup(path ? path : "", path ? strlen(path) : 0);
    s->append = append;
    return s;
}
int mnet_stream_pair(size_t cap, mnet_stream_t **rd, mnet_stream_t **wr) {
    ms_shared_t *h = new_shared();
    mnet_stream_t *a = new_stream(K_PAIR_IN), *b = new_stream(K_PAIR_OUT);
    if (!h || !a || !b) { free(h); free(a); free(b); return -1; }
    h->cap = cap ? cap : 4096;
    h->refs = 2;
    a->sh = b->sh = h; h->rd = a; h->wr = b;
    *rd = a; *wr = b;
    return 0;
}
static int sock_common(const mnet_io_t *io, mnet_stream_t **rd, mnet_stream_t **wr, ms_shared_t **out) {
    ms_shared_t *h = new_shared();
    mnet_stream_t *a = new_stream(K_SOCK_IN), *b = new_stream(K_SOCK_OUT);
    if (!h || !a || !b || !io) { free(h); free(a); free(b); return -1; }
    h->io = io; h->refs = 2; h->opt.timeout_ms = 60000;
    a->sh = b->sh = h; h->rd = a; h->wr = b;
    *rd = a; *wr = b; *out = h;
    return 0;
}
int mnet_stream_socket(const mnet_io_t *io, const char *host, int port, mnet_stream_t **rd, mnet_stream_t **wr) {
    ms_shared_t *h;
    if (!host || port <= 0 || port > 65535 || sock_common(io, rd, wr, &h)) return -1;
    cpys(h->host, sizeof h->host, host);
    h->port = port;
    cpys(h->opt.peer_name, sizeof h->opt.peer_name, host);
    return 0;
}
int mnet_stream_socket_conn(const mnet_io_t *io, void *conn, int own, mnet_stream_t **rd, mnet_stream_t **wr) {
    ms_shared_t *h;
    if (!conn || sock_common(io, rd, wr, &h)) return -1;
    (void)own;                                                         /* quem não fecha o nativo é o transporte */
    h->conn = conn; h->cstate = C_UP;
    return 0;
}

/* ---------------------------------------------------------------- socket: fio de conexão e leitura */
static int sock_wr_all(ms_shared_t *h, const void *d, size_t n) {
    size_t off = 0;
    while (off < n) {
        long k = h->io->write(h->io->env, h->conn, (const uint8_t *)d + off, n - off, h->opt.timeout_ms);
        if (k <= 0) return -1;
        off += (size_t)k;
    }
    return 0;
}
static int sock_rd_n(ms_shared_t *h, void *d, size_t n) {
    size_t got = 0;
    while (got < n) {
        long k = h->io->read(h->io->env, h->conn, (uint8_t *)d + got, n - got, h->opt.timeout_ms);
        if (k <= 0) return -1;
        got += (size_t)k;
    }
    return 0;
}
/* SOCKS4a e SOCKS5 (RFC 1928/1929). 0 ok; -1 falha de E/S; senão o erro do domínio SOCKS do CFNetwork, com o
   subdomínio nos 16 bits altos (CFSocketStreamSOCKSGetErrorSubdomain): 1 versão, 2 resultado SOCKS4, 4 método SOCKS5,
   5 usuário/senha; subdomínio 0 = código de resposta do servidor SOCKS5 (RFC 1928 §6). */
#define SOCKS_ERR(sub, code) (((sub) << 16) | (code))
static int socks_handshake(ms_shared_t *h) {
    uint8_t b[600];
    const char *u = h->opt.socks_user, *p = h->opt.socks_pass;
    size_t hl = strlen(h->host), n = 0;
    if (hl > 255) return SOCKS_ERR(0, 1);
    if (h->opt.socks_version == 4) {
        b[n++] = 4; b[n++] = 1; b[n++] = (uint8_t)(h->port >> 8); b[n++] = (uint8_t)h->port;
        b[n++] = 0; b[n++] = 0; b[n++] = 0; b[n++] = 1;              /* 0.0.0.1: SOCKS4a, o proxy resolve o nome */
        size_t ul = strlen(u); if (ul > 200) ul = 200;
        memcpy(b + n, u, ul); n += ul; b[n++] = 0;
        memcpy(b + n, h->host, hl); n += hl; b[n++] = 0;
        if (sock_wr_all(h, b, n) || sock_rd_n(h, b, 8)) return -1;
        return b[1] == 0x5a ? 0 : SOCKS_ERR(2, b[1]);                 /* 91 = pedido recusado */
    }
    int auth = u[0] != 0;
    b[0] = 5; b[1] = auth ? 2 : 1; b[2] = 0; b[3] = 2;
    if (sock_wr_all(h, b, auth ? 4 : 3) || sock_rd_n(h, b, 2)) return -1;
    if (b[0] != 5) return SOCKS_ERR(1, b[0]);
    if (b[1] == 2) {
        size_t ul = strlen(u), pl = strlen(p);
        if (!auth || ul > 255 || pl > 255) return SOCKS_ERR(5, 1);
        n = 0; b[n++] = 1; b[n++] = (uint8_t)ul; memcpy(b + n, u, ul); n += ul;
        b[n++] = (uint8_t)pl; memcpy(b + n, p, pl); n += pl;
        if (sock_wr_all(h, b, n) || sock_rd_n(h, b, 2)) return -1;
        if (b[1] != 0) return SOCKS_ERR(5, b[1]);
    } else if (b[1] != 0) return SOCKS_ERR(4, b[1]);
    n = 0;
    b[n++] = 5; b[n++] = 1; b[n++] = 0; b[n++] = 3; b[n++] = (uint8_t)hl;
    memcpy(b + n, h->host, hl); n += hl;
    b[n++] = (uint8_t)(h->port >> 8); b[n++] = (uint8_t)h->port;
    if (sock_wr_all(h, b, n) || sock_rd_n(h, b, 4)) return -1;
    if (b[0] != 5) return SOCKS_ERR(1, b[0]);
    if (b[1]) return SOCKS_ERR(0, b[1]);
    size_t rest = b[3] == 1 ? 6 : b[3] == 4 ? 18 : 0;
    if (b[3] == 3) { if (sock_rd_n(h, b, 1)) return -1; rest = (size_t)b[0] + 2; }
    return rest && sock_rd_n(h, b, rest) ? -1 : 0;
}
static void fail_both(ms_shared_t *h, int dom, long code, const char *msg) {
    h->cstate = C_FAILED; h->err_domain = dom; h->err_code = code;
    cpys(h->err_msg, sizeof h->err_msg, msg);
}
static void notify_both(ms_shared_t *h) { ping(h->rd); ping(h->wr); }
/* handshake TLS (sem a trava de estado); o resultado entra no estado com tls_result */
static int do_tls(ms_shared_t *h, char *msg, size_t ml) {
    char alpn[33];
    msg[0] = 0;
    if (!h->io->starttls) { cpys(msg, ml, "TLS indisponível"); return MNET_E_TLS; }
    const char *sni = h->opt.peer_name[0] ? h->opt.peer_name : h->host;
    return h->io->starttls(h->io->env, h->conn, sni, 0, 0, alpn, h->opt.timeout_ms, msg, ml);
}
static void tls_result(ms_shared_t *h, int r, const char *msg) {          /* com a trava */
    if (!r) { h->tls_on = 1; return; }
    fail_both(h, MNET_SD_SSL, r == MNET_E_CERT_UNTRUSTED ? -9807 : -9806, msg[0] ? msg : "o handshake TLS falhou");
}
static void *sock_thread(void *arg) {
    ms_shared_t *h = arg;
    if (h->cstate == C_CONNECTING) {
        int err = 0;
        const char *target = h->opt.socks_host[0] ? h->opt.socks_host : h->host;
        int tport = h->opt.socks_host[0] ? h->opt.socks_port : h->port;
        void *c = h->io->connect(h->io->env, target, tport, h->opt.timeout_ms, &err);
        lk(h);
        if (!c) {
            if (err == MNET_E_NO_HOST) fail_both(h, MNET_SD_NETDB, 8, "nome não encontrado");     /* EAI_NONAME */
            else if (err == MNET_E_TIMEOUT) fail_both(h, MNET_SD_POSIX, 60, "tempo esgotado ao conectar");
            else fail_both(h, MNET_SD_POSIX, 61, "conexão recusada");
        } else {
            h->conn = c;
        }
        ulk(h);
        if (c && h->opt.socks_host[0]) {
            int r = socks_handshake(h);
            lk(h);
            if (r < 0) fail_both(h, MNET_SD_POSIX, 54, "o proxy SOCKS fechou a conexão");
            else if (r) fail_both(h, MNET_SD_SOCKS, r, "o proxy SOCKS recusou a conexão");
            ulk(h);
        }
        char msg[160];
        int tr = h->cstate != C_FAILED && h->opt.tls ? do_tls(h, msg, sizeof msg) : 0;
        lk(h);
        if (tr) tls_result(h, tr, msg);
        else if (h->cstate != C_FAILED && h->opt.tls) tls_result(h, 0, "");
        if (h->cstate != C_FAILED) h->cstate = C_UP;
        wake(h);
        notify_both(h);
        ulk(h);
    }
    /* leitura */
    uint8_t tmp[16384];
    for (;;) {
        lk(h);
        while (!h->closing && h->cstate == C_UP && !h->tls_pending && h->buf.n >= RX_MAX) pthread_cond_wait(&h->cv, &h->mu);
        int stop = h->closing || h->cstate != C_UP || h->rx_eof;
        int tls_now = h->tls_pending;
        ulk(h);
        if (stop) break;
        if (tls_now) {
            char msg[160];
            pthread_mutex_lock(&h->io_mu);
            int r = do_tls(h, msg, sizeof msg);
            pthread_mutex_unlock(&h->io_mu);
            lk(h);
            tls_result(h, r, msg);
            h->tls_pending = 0;
            wake(h);
            notify_both(h);
            ulk(h);
            if (r) break;
            continue;
        }
        long k;
        if (h->io->wait) {
            int w = h->io->wait(h->io->env, h->conn, 1, 250);
            if (w == 0) continue;
            pthread_mutex_lock(&h->io_mu);
            k = h->io->read(h->io->env, h->conn, tmp, sizeof tmp, 0);
            pthread_mutex_unlock(&h->io_mu);
        } else {                                                       /* sem wait: tenta sem esperar e dorme fora da trava */
            pthread_mutex_lock(&h->io_mu);
            k = h->io->read(h->io->env, h->conn, tmp, sizeof tmp, 0);
            pthread_mutex_unlock(&h->io_mu);
            if (k == -2) usleep(10000);
        }
        if (k == -2) continue;
        lk(h);
        if (k > 0) mnet_buf_put(&h->buf, tmp, (size_t)k);
        else if (k == 0) h->rx_eof = 1;
        else { h->rx_err = 1; h->rx_eof = 1; }
        wake(h);
        ping(h->rd);
        ulk(h);
    }
    shared_unref(h);
    return 0;
}
static int sock_start(ms_shared_t *h) {
    pthread_t t;
    if (h->opened) return 0;
    h->opened = 1;
    if (h->cstate == C_IDLE) h->cstate = C_CONNECTING;
    if (h->opt.tls && h->cstate == C_UP) h->tls_pending = 1;          /* conexão nativa com TLS pedido antes do open */
    h->refs++;
    if (pthread_create(&t, 0, sock_thread, h)) { h->refs--; fail_both(h, MNET_SD_POSIX, 35, "sem fio"); return -1; }
    pthread_detach(t);
    return 0;
}

/* ---------------------------------------------------------------- estado */
static int is_input(const mnet_stream_t *s) { return s->kind == K_MEM_IN || s->kind == K_FILE_IN || s->kind == K_PAIR_IN || s->kind == K_SOCK_IN; }
/* status atualizado (chamar com a trava do compartilhado, se houver) */
static int refresh(mnet_stream_t *s) {
    ms_shared_t *h = s->sh;
    if (s->status == MNET_SS_NOT_OPEN || s->status == MNET_SS_CLOSED || s->status == MNET_SS_ERROR) return s->status;
    switch (s->kind) {
    case K_PAIR_IN:
        if (h->buf.n == 0 && h->w_closed) s->status = MNET_SS_AT_END;
        break;
    case K_PAIR_OUT:
        if (h->r_closed) set_err(s, MNET_SD_POSIX, 32, "o outro lado fechou");
        break;
    case K_SOCK_IN: case K_SOCK_OUT:
        if (h->cstate == C_FAILED) { set_err(s, h->err_domain, h->err_code, h->err_msg); break; }
        if (h->cstate == C_UP && s->status == MNET_SS_OPENING) { s->status = MNET_SS_OPEN; s->opened_ok = 1; }
        if (s->kind == K_SOCK_IN && s->status == MNET_SS_OPEN && h->buf.n == 0 && h->rx_eof) {
            if (h->rx_err) set_err(s, MNET_SD_POSIX, 54, "conexão interrompida");
            else s->status = MNET_SS_AT_END;
        }
        break;
    }
    return s->status;
}
static int has_bytes_l(mnet_stream_t *s) {
    if (s->status != MNET_SS_OPEN) return 0;
    switch (s->kind) {
    case K_MEM_IN: return s->pos < s->len;
    case K_FILE_IN: return s->foff < s->fsize || s->fsize < 0;
    case K_PAIR_IN: case K_SOCK_IN: return s->sh->buf.n > 0;
    }
    return 0;
}
static int has_space_l(mnet_stream_t *s) {
    if (s->status != MNET_SS_OPEN) return 0;
    switch (s->kind) {
    case K_MEM_OUT: case K_FILE_OUT: return 1;
    case K_BUF_OUT: return s->upos < s->ucap;
    case K_PAIR_OUT: return s->sh->buf.n < s->sh->cap && !s->sh->r_closed;
    case K_SOCK_OUT: return s->sh->cstate == C_UP && !s->sh->tls_pending;
    }
    return 0;
}
int mnet_stream_status(mnet_stream_t *s) {
    if (!s) return MNET_SS_ERROR;
    if (s->sh) lk(s->sh);
    int st = refresh(s);
    if (s->sh) ulk(s->sh);
    return st;
}
int mnet_stream_has_bytes(mnet_stream_t *s) {
    if (!s) return 0;
    if (s->sh) lk(s->sh);
    refresh(s);
    int r = has_bytes_l(s);
    if (s->sh) ulk(s->sh);
    return r;
}
int mnet_stream_has_space(mnet_stream_t *s) {
    if (!s) return 0;
    if (s->sh) lk(s->sh);
    refresh(s);
    int r = has_space_l(s);
    if (s->sh) ulk(s->sh);
    return r;
}
int mnet_stream_error(mnet_stream_t *s, long *code, const char **msg) {
    if (!s) return 0;
    if (s->sh) lk(s->sh);
    refresh(s);
    if (s->sh) ulk(s->sh);
    if (s->status != MNET_SS_ERROR) return 0;
    if (code) *code = s->err_code;
    if (msg) *msg = s->err_msg;
    return s->err_domain;
}
unsigned mnet_stream_events(mnet_stream_t *s) {
    unsigned ev = 0;
    if (!s) return 0;
    if (s->sh) lk(s->sh);
    int st = refresh(s);
    if (st == MNET_SS_NOT_OPEN || st == MNET_SS_OPENING || st == MNET_SS_CLOSED) { if (s->sh) ulk(s->sh); return 0; }
    if (s->opened_ok && !(s->done & MNET_SE_OPEN)) { ev |= MNET_SE_OPEN; s->done |= MNET_SE_OPEN; }
    if (st == MNET_SS_ERROR) {
        if (!(s->done & MNET_SE_ERROR)) { ev |= MNET_SE_ERROR; s->done |= MNET_SE_ERROR; }
    } else if (is_input(s)) {
        if (s->bytes_armed && has_bytes_l(s)) { ev |= MNET_SE_BYTES; s->bytes_armed = 0; }
        if (st == MNET_SS_AT_END && !(s->done & MNET_SE_END)) { ev |= MNET_SE_END; s->done |= MNET_SE_END; }
    } else {
        if (s->space_armed && has_space_l(s)) { ev |= MNET_SE_SPACE; s->space_armed = 0; }
        if (st == MNET_SS_AT_END && !(s->done & MNET_SE_END)) { ev |= MNET_SE_END; s->done |= MNET_SE_END; }
    }
    if (s->sh) ulk(s->sh);
    return ev;
}
void mnet_stream_set_notify(mnet_stream_t *s, void (*fn)(void *), void *ctx) {
    if (!s) return;
    if (s->sh) lk(s->sh);
    s->notify = fn; s->nctx = ctx;
    if (s->sh) ulk(s->sh);
}
void mnet_stream_set_on_open(mnet_stream_t *s, void (*fn)(void *, mnet_stream_t *), void *ctx) {
    if (s) { s->on_open = fn; s->octx = ctx; }
}

/* ---------------------------------------------------------------- abrir e fechar */
int mnet_stream_open(mnet_stream_t *s) {
    if (!s || s->status != MNET_SS_NOT_OPEN) return -1;
    switch (s->kind) {
    case K_FILE_IN:
        s->fd = open(s->path, O_RD);
        if (s->fd < 0) { set_err(s, MNET_SD_POSIX, ms_errno(), "não foi possível abrir o arquivo"); break; }
        s->fsize = lseek(s->fd, 0, 2);
        if (s->fsize < 0) s->fsize = -1;                               /* sem tamanho (pipe): lê até o fim */
        s->foff = s->want_off > 0 ? s->want_off : 0;
        lseek(s->fd, s->foff, 0);
        s->status = MNET_SS_OPEN; s->opened_ok = 1;
        break;
    case K_FILE_OUT:
        s->fd = open(s->path, s->append ? O_WR_APP : O_WR_NEW, 0644);
        if (s->fd < 0) { set_err(s, MNET_SD_POSIX, ms_errno(), "não foi possível criar o arquivo"); break; }
        s->foff = s->append ? lseek(s->fd, 0, 2) : 0;
        s->status = MNET_SS_OPEN; s->opened_ok = 1;
        break;
    case K_PAIR_IN: case K_PAIR_OUT:
        lk(s->sh); s->status = MNET_SS_OPEN; s->opened_ok = 1; ulk(s->sh);
        break;
    case K_SOCK_IN: case K_SOCK_OUT:
        lk(s->sh);
        s->status = MNET_SS_OPENING;
        if (sock_start(s->sh)) set_err(s, MNET_SD_POSIX, 35, "sem fio");
        refresh(s);
        ulk(s->sh);
        break;
    default:
        s->status = MNET_SS_OPEN; s->opened_ok = 1;
        if (s->kind == K_MEM_IN && s->len == 0) s->status = MNET_SS_AT_END;
        break;
    }
    if (s->on_open && s->status != MNET_SS_ERROR) s->on_open(s->octx, s);
    if (s->sh) lk(s->sh);
    ping(s);
    if (s->sh) ulk(s->sh);
    return s->status == MNET_SS_ERROR ? -1 : 0;
}
void mnet_stream_close(mnet_stream_t *s) {
    if (!s || s->status == MNET_SS_CLOSED) return;
    int was = s->status;
    s->status = MNET_SS_CLOSED;
    if (s->fd >= 0) { close(s->fd); s->fd = -1; }
    ms_shared_t *h = s->sh;
    if (!h || was == MNET_SS_NOT_OPEN) return;
    mnet_stream_t *other = 0;
    lk(h);
    if (s->kind == K_PAIR_OUT) { h->w_closed = 1; other = h->rd; }
    else if (s->kind == K_PAIR_IN) { h->r_closed = 1; other = h->wr; }
    else {
        int both = (!h->rd || h->rd->status == MNET_SS_CLOSED || h->rd->status == MNET_SS_NOT_OPEN) &&
                   (!h->wr || h->wr->status == MNET_SS_CLOSED || h->wr->status == MNET_SS_NOT_OPEN);
        if (both) h->closing = 1;
    }
    wake(h);
    ping(other);
    ulk(h);
}
void mnet_stream_free(mnet_stream_t *s) {
    if (!s) return;
    mnet_stream_close(s);
    ms_shared_t *h = s->sh;
    if (h) {
        lk(h);
        if (h->rd == s) h->rd = 0;
        if (h->wr == s) h->wr = 0;
        if (!h->rd && !h->wr) { h->closing = 1; wake(h); }
        if (s->kind == K_PAIR_OUT) h->w_closed = 1;
        if (s->kind == K_PAIR_IN) h->r_closed = 1;
        ping(h->rd ? h->rd : h->wr);
        ulk(h);
        shared_unref(h);
    }
    free(s->data);
    free(s->path);
    mnet_buf_free(&s->mem);
    free(s);
}

/* ---------------------------------------------------------------- ler e escrever */
long mnet_stream_read(mnet_stream_t *s, uint8_t *b, size_t n) {
    long r = -1;
    if (!s || !is_input(s)) return -1;
    ms_shared_t *h = s->sh;
    if (h) lk(h);
    if (s->kind == K_SOCK_IN)                                       /* espera conectar (read bloqueante) */
        while (refresh(s) == MNET_SS_OPENING) pthread_cond_wait(&h->cv, &h->mu);
    int st = refresh(s);
    if (st == MNET_SS_AT_END) { if (h) ulk(h); return 0; }
    if (st != MNET_SS_OPEN) { if (h) ulk(h); return -1; }
    if (n == 0) { if (h) ulk(h); return 0; }
    switch (s->kind) {
    case K_MEM_IN: {
        size_t k = s->len - s->pos < n ? s->len - s->pos : n;
        memcpy(b, s->data + s->pos, k);
        s->pos += k;
        if (s->pos >= s->len) s->status = MNET_SS_AT_END;
        r = (long)k;
        break;
    }
    case K_FILE_IN: {
        long k = read(s->fd, b, n);
        if (k < 0) { set_err(s, MNET_SD_POSIX, ms_errno(), "erro de leitura"); r = -1; break; }
        s->foff += k;
        if (k == 0 || (s->fsize >= 0 && s->foff >= s->fsize)) s->status = MNET_SS_AT_END;
        r = k;
        break;
    }
    case K_PAIR_IN: case K_SOCK_IN:
        while (h->buf.n == 0 && refresh(s) == MNET_SS_OPEN && !(s->kind == K_PAIR_IN ? h->w_closed : h->rx_eof))
            pthread_cond_wait(&h->cv, &h->mu);
        if (h->buf.n) {
            size_t k = h->buf.n < n ? h->buf.n : n;
            memcpy(b, h->buf.p, k);
            mnet_buf_consume(&h->buf, k);
            r = (long)k;
            wake(h);                                                  /* escritor do par / leitor do socket */
        } else r = refresh(s) == MNET_SS_AT_END ? 0 : -1;
        refresh(s);
        break;
    }
    s->bytes_armed = 1;
    if (h && s->kind == K_PAIR_IN) ping(h->wr);                        /* espaço livre no par */
    ping(s);
    if (h) ulk(h);
    return r;
}
long mnet_stream_write(mnet_stream_t *s, const uint8_t *b, size_t n) {
    long r = -1;
    if (!s || is_input(s)) return -1;
    ms_shared_t *h = s->sh;
    if (h) lk(h);
    if (s->kind == K_SOCK_OUT)
        while (refresh(s) == MNET_SS_OPENING || (s->status == MNET_SS_OPEN && (h->tls_pending || h->cstate == C_CONNECTING)))
            pthread_cond_wait(&h->cv, &h->mu);
    int st = refresh(s);
    if (st == MNET_SS_AT_END) { if (h) ulk(h); return 0; }
    if (st != MNET_SS_OPEN) { if (h) ulk(h); return -1; }
    if (n == 0) { if (h) ulk(h); return 0; }
    switch (s->kind) {
    case K_MEM_OUT:
        r = mnet_buf_put(&s->mem, b, n) ? -1 : (long)n;
        break;
    case K_BUF_OUT: {
        size_t k = s->ucap - s->upos < n ? s->ucap - s->upos : n;
        memcpy(s->ubuf + s->upos, b, k);
        s->upos += k;
        if (s->upos >= s->ucap) s->status = MNET_SS_AT_END;
        r = (long)k;
        break;
    }
    case K_FILE_OUT: {
        long k = write(s->fd, b, n);
        if (k < 0) { set_err(s, MNET_SD_POSIX, ms_errno(), "erro de escrita"); r = -1; }
        else { s->foff += k; r = k; }
        break;
    }
    case K_PAIR_OUT:
        while (h->buf.n >= h->cap && !h->r_closed && s->status == MNET_SS_OPEN) pthread_cond_wait(&h->cv, &h->mu);
        if (h->r_closed) { set_err(s, MNET_SD_POSIX, 32, "o outro lado fechou"); r = -1; break; }
        {
            size_t k = h->cap - h->buf.n < n ? h->cap - h->buf.n : n;
            if (mnet_buf_put(&h->buf, b, k)) { r = -1; break; }
            r = (long)k;
            wake(h);
        }
        break;
    case K_SOCK_OUT:
        ulk(h);
        pthread_mutex_lock(&h->io_mu);
        r = h->io->write(h->io->env, h->conn, b, n, h->opt.timeout_ms);
        pthread_mutex_unlock(&h->io_mu);
        lk(h);
        if (r <= 0) { set_err(s, MNET_SD_POSIX, r == -2 ? 60 : 32, r == -2 ? "tempo esgotado na escrita" : "conexão fechada"); r = -1; }
        break;
    }
    s->space_armed = 1;
    if (h && s->kind == K_PAIR_OUT) ping(h->rd);
    ping(s);
    if (h) ulk(h);
    return r;
}

/* ---------------------------------------------------------------- propriedades */
int mnet_stream_get_buffer(mnet_stream_t *s, uint8_t **p, size_t *n) {
    if (!s || s->kind != K_MEM_IN || s->status != MNET_SS_OPEN) return 0;
    if (p) *p = s->data + s->pos;
    if (n) *n = s->len - s->pos;
    return 1;
}
const uint8_t *mnet_stream_mem_data(mnet_stream_t *s, size_t *n) {
    if (!s || s->kind != K_MEM_OUT) { if (n) *n = 0; return 0; }
    if (n) *n = s->mem.n;
    return s->mem.p ? s->mem.p : (const uint8_t *)"";
}
long long mnet_stream_offset(mnet_stream_t *s) {
    if (!s) return -1;
    if (s->kind == K_FILE_IN || s->kind == K_FILE_OUT) return s->status == MNET_SS_NOT_OPEN ? (s->want_off > 0 ? s->want_off : 0) : s->foff;
    if (s->kind == K_MEM_IN) return (long long)s->pos;
    if (s->kind == K_BUF_OUT) return (long long)s->upos;
    if (s->kind == K_MEM_OUT) return (long long)s->mem.n;
    return -1;
}
int mnet_stream_seek(mnet_stream_t *s, long long off) {
    if (!s || off < 0) return -1;
    if (s->kind == K_FILE_IN || s->kind == K_FILE_OUT) {
        if (s->status == MNET_SS_NOT_OPEN) { s->want_off = off; return 0; }
        if (s->fd < 0 || lseek(s->fd, off, 0) < 0) return -1;
        s->foff = off;
        if (s->kind == K_FILE_IN) {
            if (s->status == MNET_SS_AT_END && (s->fsize < 0 || off < s->fsize)) { s->status = MNET_SS_OPEN; s->done &= ~MNET_SE_END; }
            s->bytes_armed = 1;
        }
        return 0;
    }
    if (s->kind == K_MEM_IN && (size_t)off <= s->len && s->status != MNET_SS_NOT_OPEN && s->status != MNET_SS_CLOSED) {
        s->pos = (size_t)off;
        if (s->status == MNET_SS_AT_END && s->pos < s->len) { s->status = MNET_SS_OPEN; s->done &= ~MNET_SE_END; }
        else if (s->pos >= s->len) s->status = MNET_SS_AT_END;
        s->bytes_armed = 1;
        return 0;
    }
    return -1;
}
int mnet_stream_set_append(mnet_stream_t *s, int append) {
    if (!s || s->kind != K_FILE_OUT || s->status != MNET_SS_NOT_OPEN) return -1;
    s->append = append != 0;
    return 0;
}
int mnet_stream_is_socket(const mnet_stream_t *s) { return s && (s->kind == K_SOCK_IN || s->kind == K_SOCK_OUT); }
int mnet_stream_is_input(const mnet_stream_t *s) { return s && is_input(s); }
mnet_sockopts_t *mnet_stream_sockopts(mnet_stream_t *s) { return mnet_stream_is_socket(s) ? &s->sh->opt : 0; }
void *mnet_stream_conn(mnet_stream_t *s) { return mnet_stream_is_socket(s) ? s->sh->conn : 0; }
int mnet_stream_tls_active(mnet_stream_t *s) { return mnet_stream_is_socket(s) && s->sh->tls_on; }
/* TLS pedido depois do open (STARTTLS): o fio leitor faz o handshake antes da próxima leitura; escritas esperam */
int mnet_stream_starttls(mnet_stream_t *s) {
    if (!mnet_stream_is_socket(s)) return -1;
    ms_shared_t *h = s->sh;
    lk(h);
    h->opt.tls = 1;
    if (h->opened && h->cstate == C_UP && !h->tls_on) { h->tls_pending = 1; wake(h); }
    ulk(h);
    return 0;
}
/* produtor interno de um par (corpo do pedido HTTP): marca erro no leitor */
void mnet_stream_fail(mnet_stream_t *s, int domain, long code, const char *msg) {
    if (!s) return;
    if (s->sh) lk(s->sh);
    if (s->status != MNET_SS_CLOSED) set_err(s, domain, code, msg);
    ping(s);
    if (s->sh) { wake(s->sh); ulk(s->sh); }
}
/* produtor pelo lado do escritor: marca o erro no leitor do par, se ele ainda existe */
void mnet_stream_fail_peer(mnet_stream_t *w, int domain, long code, const char *msg) {
    if (!w || w->kind != K_PAIR_OUT) return;
    ms_shared_t *h = w->sh;
    lk(h);
    if (h->rd && h->rd->status != MNET_SS_CLOSED) { set_err(h->rd, domain, code, msg); ping(h->rd); }
    wake(h);
    ulk(h);
}
int mnet_stream_peer_gone(mnet_stream_t *w) {
    if (!w || !w->sh) return 1;
    lk(w->sh);
    int gone = !w->sh->rd || w->sh->r_closed;
    ulk(w->sh);
    return gone;
}
/* leitura com prazo (NSURLSessionStreamTask readData...timeout:): >0 bytes, 0 fim, -1 erro, -2 prazo (<= 0 = sem prazo) */
long mnet_stream_read_wait(mnet_stream_t *s, uint8_t *b, size_t n, int timeout_ms) {
    if (!s || timeout_ms <= 0 || !s->sh || (s->kind != K_SOCK_IN && s->kind != K_PAIR_IN)) return mnet_stream_read(s, b, n);
    ms_shared_t *h = s->sh;
    ms_ts_t until;
    clock_gettime(MS_REALTIME, &until);
    long long ns = (long long)TS_NSEC(until) + (long long)(timeout_ms % 1000) * 1000000LL;
    TS_SEC(until) += timeout_ms / 1000 + ns / 1000000000LL;
    TS_NSEC(until) = ns % 1000000000LL;
    lk(h);
    for (;;) {
        int st = refresh(s);
        if (st != MNET_SS_OPEN && st != MNET_SS_OPENING) break;
        if (h->buf.n || (s->kind == K_PAIR_IN ? h->w_closed : h->rx_eof)) break;
        if (pthread_cond_timedwait(&h->cv, &h->mu, &until) != 0) {
            st = refresh(s);
            if (!h->buf.n && st == MNET_SS_OPEN && !(s->kind == K_PAIR_IN ? h->w_closed : h->rx_eof)) { ulk(h); return -2; }
        }
    }
    ulk(h);
    return mnet_stream_read(s, b, n);                                  /* agora não bloqueia */
}
