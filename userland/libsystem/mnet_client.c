/*
 * mOS — mnet: cliente HTTP. Um pedido por chamada (bloqueante, num fio do chamador), conexões reaproveitadas
 * num pool por origem (esquema, host, porta, proxy). HTTPS oferece ALPN h2 e http/1.1 e usa o que o servidor
 * escolher. Redirect (RFC 9110 §15.4), cookies (mnet_cookie.c), Content-Encoding (mnet_decode.c), proxy HTTP
 * (forma absoluta), túnel CONNECT para HTTPS e SOCKS5 (RFC 1928/1929, nome resolvido pelo proxy).
 *
 * HTTP/2: pedidos simultâneos para a mesma origem dividem uma conexão (um fluxo cada) enquanto o servidor aceitar
 * fluxos (SETTINGS_MAX_CONCURRENT_STREAMS); acima disso abre outra.
 */
#include "mnet.h"

#ifdef MNET_HOST
#include <pthread.h>
#include <unistd.h>
typedef pthread_mutex_t mn_mutex_t;
typedef pthread_cond_t mn_cond_t;
#else
typedef struct { _Alignas(8) unsigned char b[64]; } mn_mutex_t;
typedef struct { _Alignas(8) unsigned char b[48]; } mn_cond_t;
int pthread_mutex_init(void *m, const void *a);
int pthread_mutex_lock(void *m);
int pthread_mutex_unlock(void *m);
int pthread_mutex_destroy(void *m);
int pthread_cond_init(void *c, const void *a);
int pthread_cond_wait(void *c, void *m);
int pthread_cond_broadcast(void *c);
int pthread_cond_destroy(void *c);
int usleep(unsigned us);
#endif

#define MAX_REDIRECTS 16
#define IDLE_MAX 32
#define IDLE_TIMEOUT 90000

struct mnet_pool_ent {                 /* conexão HTTP/1.1 ociosa (as h2 ficam em h2live) */
    char key[300];
    void *conn;
    long long last;
    struct mnet_pool_ent *next;
};

void mnet_client_init(mnet_client_t *c, const mnet_io_t *io) {
    memset(c, 0, sizeof *c);
    c->io = io;
    c->max_idle = IDLE_MAX;
    c->idle_timeout_ms = IDLE_TIMEOUT;
    memcpy(c->user_agent, "mOS CFNetwork/1.0 Darwin", 25);
}
static void lk(mnet_client_t *c) { if (c->io->lock) c->io->lock(c->io->env); }
static void ulk(mnet_client_t *c) { if (c->io->unlock) c->io->unlock(c->io->env); }
static void ent_close(mnet_client_t *c, mnet_pool_ent_t *e) {
    if (e->conn) c->io->close(c->io->env, e->conn);
    free(e);
}
void mnet_client_close_idle(mnet_client_t *c) {
    lk(c);
    mnet_pool_ent_t *e = c->idle;
    c->idle = 0; c->idle_count = 0;
    ulk(c);
    while (e) { mnet_pool_ent_t *n = e->next; ent_close(c, e); e = n; }
    void hc_drop_idle(mnet_client_t *c);
    hc_drop_idle(c);
}
static mnet_pool_ent_t *pool_take(mnet_client_t *c, const char *key) {
    long long now = c->io->now_ms(c->io->env);
    mnet_pool_ent_t *got = 0, *stale = 0;
    lk(c);
    for (mnet_pool_ent_t **pp = &c->idle; *pp;) {
        mnet_pool_ent_t *e = *pp;
        if (now - e->last > c->idle_timeout_ms) { *pp = e->next; c->idle_count--; e->next = stale; stale = e; continue; }
        if (!got && !strcmp(e->key, key)) { *pp = e->next; c->idle_count--; got = e; continue; }
        pp = &e->next;
    }
    ulk(c);
    while (stale) { mnet_pool_ent_t *n = stale->next; ent_close(c, stale); stale = n; }
    return got;
}
static void pool_put(mnet_client_t *c, mnet_pool_ent_t *e) {
    e->last = c->io->now_ms(c->io->env);
    mnet_pool_ent_t *drop = 0;
    lk(c);
    if (c->idle_count >= c->max_idle) {                         /* tira o mais velho */
        mnet_pool_ent_t **pp = &c->idle, **old = 0;
        for (; *pp; pp = &(*pp)->next) old = pp;
        if (old) { drop = *old; *old = 0; c->idle_count--; }
    }
    e->next = c->idle;
    c->idle = e;
    c->idle_count++;
    ulk(c);
    if (drop) ent_close(c, drop);
}

void mnet_resp_free(mnet_resp_t *r) { mnet_hdrs_free(&r->hdrs); free(r->url); r->url = 0; }

/* ---- estado de um pedido */
typedef struct {
    mnet_client_t *c;
    const mnet_req_t *q;
    mnet_resp_t *r;
    char *msg; size_t msglen;
    long long deadline;              /* total_timeout, em ms monotônicos (0 = nenhum) */
    int idle_ms;
    int discard;                     /* corpo de um redirect seguido: descarta */
    int follow, hop, maxr, too_many;
    char *next_url;                  /* redirect decidido na cabeça */
    int cancelled, status_seen;
    mnet_dec_t *dec;
    int dec_err;
    /* h2 */
    mnet_h2_stream_t *st;
    int h2_head_ok;
    void *box;                       /* caixa do pedido na conexão h2 compartilhada */
} job_t;

#define JAR(j) ((j)->q->jar ? (j)->q->jar : (j)->c->jar)
static void setmsg(job_t *j, const char *m) {
    if (!j->msg || !j->msglen) return;
    size_t n = strlen(m);
    if (n >= j->msglen) n = j->msglen - 1;
    memcpy(j->msg, m, n);
    j->msg[n] = 0;
}
static int cancelled(job_t *j) { return j->cancelled || (j->q->cancel && *j->q->cancel); }
static int wait_ms(job_t *j) {
    int w = j->idle_ms;
    if (j->deadline) {
        long long left = j->deadline - j->c->io->now_ms(j->c->io->env);
        if (left <= 0) return 0;
        if (left < w) w = (int)left;
    }
    return w > 250 ? 250 : w;        /* acorda a cada 250 ms para ver cancelamento */
}
/* lê com o tempo ocioso do pedido; -2 = tempo esgotado de verdade, -3 = cancelado */
static long rd(job_t *j, void *conn, void *buf, size_t n) {
    long long start = j->c->io->now_ms(j->c->io->env);
    for (;;) {
        if (cancelled(j)) return -3;
        int w = wait_ms(j);
        if (w <= 0) return -2;
        long k = j->c->io->read(j->c->io->env, conn, buf, n, w);
        if (k != -2) return k;
        if (j->c->io->now_ms(j->c->io->env) - start >= j->idle_ms) return -2;
    }
}
static int wr(job_t *j, void *conn, const void *d, size_t n) {
    size_t off = 0;
    while (off < n) {
        if (cancelled(j)) return -3;
        long k = j->c->io->write(j->c->io->env, conn, (const uint8_t *)d + off, n - off, j->idle_ms);
        if (k == -2) return -2;
        if (k <= 0) return -1;
        off += (size_t)k;
    }
    return 0;
}

static int head_ready(job_t *j, int follow_possible);
static int deliver(void *ctx, const uint8_t *d, size_t n) {
    job_t *j = ctx;
    if (j->discard || !n) return 0;
    if (j->q->on_body && j->q->on_body(j->q->ctx, d, n)) { j->cancelled = 1; return -1; }
    return 0;
}
static int body_in(job_t *j, const uint8_t *d, size_t n) {
    if (j->discard) return 0;
    if (j->dec) {
        if (mnet_dec_feed(j->dec, d, n)) { if (!j->cancelled) j->dec_err = 1; return -1; }
        return 0;
    }
    return deliver(j, d, n);
}
static int h1_body(void *ctx, const uint8_t *d, size_t n) { return body_in(ctx, d, n); }
static int h1_head(void *ctx, mnet_h1_t *p) {
    job_t *j = ctx;
    j->r->status = p->status;
    j->r->version = p->minor ? 11 : 10;
    mnet_hdrs_free(&j->r->hdrs);
    for (int i = 0; i < p->hdrs.n; i++)
        mnet_hdrs_add(&j->r->hdrs, p->hdrs.v[i].name, strlen(p->hdrs.v[i].name), p->hdrs.v[i].value, strlen(p->hdrs.v[i].value));
    return head_ready(j, j->follow);
}

static int is_redirect(int st) { return st == 301 || st == 302 || st == 303 || st == 307 || st == 308; }

/* cabeçalhos finais conhecidos: decide redirect, cookies, decodificador; chama on_response */
static int head_ready(job_t *j, int follow_possible) {
    const mnet_req_t *q = j->q;
    mnet_resp_t *r = j->r;
    j->status_seen = 1;
    const char *cl = mnet_hdrs_get(&r->hdrs, "content-length");
    r->expected_len = -1;
    if (cl) { long long v = 0; const char *p = cl; while (*p >= '0' && *p <= '9') v = v * 10 + (*p++ - '0'); if (p != cl) r->expected_len = v; }
    /* cookies de toda resposta, inclusive redirect */
    if (JAR(j) && !q->no_cookies && !q->no_store_cookies) {
        mnet_url_t u;
        if (!mnet_url_parse(r->url, &u)) {
            long long now = j->c->io->unix_time(j->c->io->env);
            lk(j->c);                                             /* o jar é compartilhado entre os fios */
            for (int i = 0; i < r->hdrs.n; i++)
                if (mnet_ieq(r->hdrs.v[i].name, "set-cookie"))
                    mnet_jar_set(JAR(j), r->hdrs.v[i].value, u.scheme, u.host, u.target, 1, now);
            ulk(j->c);
            mnet_url_free(&u);
        }
    }
    const char *loc = mnet_hdrs_get(&r->hdrs, "location");
    if (follow_possible && is_redirect(r->status) && loc) {
        char *next = mnet_url_resolve(r->url, loc);
        if (next) {
            if (q->on_redirect) {
                int go = j->hop < MAX_REDIRECTS ? q->on_redirect(q->ctx, r, &next) : (j->too_many = 1, 1);
                if (go && next) { j->next_url = next; j->discard = 1; return 0; }
                free(next);                                       /* o delegate preferiu receber o 3xx */
            } else if (j->hop < j->maxr) { j->next_url = next; j->discard = 1; return 0; }
            else { free(next); j->too_many = 1; j->discard = 1; return 0; }
        }
    }
    j->discard = 0;
    if (!q->no_decode) {
        const char *ce = mnet_hdrs_get(&r->hdrs, "content-encoding");
        if (ce && !j->dec) j->dec = mnet_dec_new(ce, deliver, j);
    }
    if (q->on_response && q->on_response(q->ctx, r)) { j->cancelled = 1; return -1; }
    return 0;
}

/* ---- transporte: conexão direta, proxy HTTP, túnel CONNECT, SOCKS5 */
static int map_connect_err(int e) { return e ? e : MNET_E_CONNECT; }
static int socks5(job_t *j, void *conn, const char *host, int port) {
    const char *auth = j->q->proxy_auth;
    uint8_t b[600];
    b[0] = 5; b[1] = auth ? 2 : 1; b[2] = 0; b[3] = 2;
    if (wr(j, conn, b, auth ? 4 : 3)) return MNET_E_PROXY;
    if (rd(j, conn, b, 2) != 2 || b[0] != 5) return MNET_E_PROXY;
    if (b[1] == 2) {
        if (!auth) return MNET_E_PROXY;
        const char *col = strchr(auth, ':');
        size_t ul = col ? (size_t)(col - auth) : strlen(auth), pl = col ? strlen(col + 1) : 0;
        if (ul > 255 || pl > 255) return MNET_E_PROXY;
        size_t n = 0;
        b[n++] = 1; b[n++] = (uint8_t)ul; memcpy(b + n, auth, ul); n += ul;
        b[n++] = (uint8_t)pl; if (pl) memcpy(b + n, col + 1, pl); n += pl;
        if (wr(j, conn, b, n) || rd(j, conn, b, 2) != 2 || b[1] != 0) { setmsg(j, "proxy SOCKS recusou o usuário"); return MNET_E_PROXY; }
    } else if (b[1] != 0) { setmsg(j, "proxy SOCKS sem método aceito"); return MNET_E_PROXY; }
    size_t hl = strlen(host), n = 0;
    if (hl > 255) return MNET_E_BAD_URL;
    b[n++] = 5; b[n++] = 1; b[n++] = 0; b[n++] = 3; b[n++] = (uint8_t)hl;
    memcpy(b + n, host, hl); n += hl;
    b[n++] = (uint8_t)(port >> 8); b[n++] = (uint8_t)port;
    if (wr(j, conn, b, n)) return MNET_E_PROXY;
    if (rd(j, conn, b, 4) != 4 || b[0] != 5) return MNET_E_PROXY;
    if (b[1]) {
        setmsg(j, b[1] == 4 ? "proxy SOCKS: host inalcançável" : b[1] == 5 ? "proxy SOCKS: conexão recusada" : "proxy SOCKS falhou");
        return b[1] == 4 ? MNET_E_NO_HOST : MNET_E_CONNECT;
    }
    size_t rest = b[3] == 1 ? 6 : b[3] == 4 ? 18 : 0;
    if (b[3] == 3) { if (rd(j, conn, b, 1) != 1) return MNET_E_PROXY; rest = (size_t)b[0] + 2; }
    size_t got = 0;
    while (got < rest) { long k = rd(j, conn, b, rest - got); if (k <= 0) return MNET_E_PROXY; got += (size_t)k; }
    return 0;
}
static int tunnel(job_t *j, void *conn, const char *host, int port) {
    mnet_buf_t o = {0};
    mnet_buf_puts(&o, "CONNECT "); mnet_buf_puts(&o, host); mnet_buf_putc(&o, ':'); mnet_buf_putu(&o, (unsigned)port);
    mnet_buf_puts(&o, " HTTP/1.1\r\nHost: "); mnet_buf_puts(&o, host); mnet_buf_putc(&o, ':'); mnet_buf_putu(&o, (unsigned)port);
    mnet_buf_puts(&o, "\r\nUser-Agent: "); mnet_buf_puts(&o, j->c->user_agent); mnet_buf_puts(&o, "\r\n");
    if (j->q->proxy_auth && strchr(j->q->proxy_auth, ' ')) { mnet_buf_puts(&o, "Proxy-Authorization: "); mnet_buf_puts(&o, j->q->proxy_auth); mnet_buf_puts(&o, "\r\n"); }
    mnet_buf_puts(&o, "\r\n");
    int r = wr(j, conn, o.p, o.n);
    mnet_buf_free(&o);
    if (r) return MNET_E_PROXY;
    mnet_h1_t p = {0};
    mnet_h1_init(&p, 1);
    uint8_t b[1];                                                /* byte a byte: nada do TLS pode ser consumido */
    while (p.state != MNET_H1_DONE) {
        long k = rd(j, conn, b, 1);
        if (k <= 0 || mnet_h1_feed(&p, b, 1) < 0) { mnet_h1_free(&p); setmsg(j, "proxy fechou o túnel"); return k == -2 ? MNET_E_TIMEOUT : MNET_E_PROXY; }
    }
    int st = p.status;
    mnet_h1_free(&p);
    if (st / 100 != 2) { setmsg(j, st == 407 ? "o proxy pede autenticação (407)" : "o proxy recusou o túnel"); return MNET_E_PROXY; }
    return 0;
}

/* abre a conexão do pedido; *h2 != NULL quando o ALPN escolheu h2 */
static int open_conn(job_t *j, const mnet_url_t *u, mnet_url_t *px, void **out, mnet_h2_t **h2) {
    const mnet_io_t *io = j->c->io;
    int err = 0;
    int tmo = j->idle_ms;
    void *conn = px ? io->connect(io->env, px->host, px->port, tmo, &err) : io->connect(io->env, u->host, u->port, tmo, &err);
    if (!conn) {
        setmsg(j, err == MNET_E_NO_HOST ? (px ? "o nome do proxy não resolveu" : "o nome do servidor não resolveu") :
                  err == MNET_E_TIMEOUT ? "tempo esgotado ao conectar" : px ? "não conectou ao proxy" : "não conectou ao servidor");
        return map_connect_err(err);
    }
    int tls = !strcmp(u->scheme, "https");
    int r = 0;
    if (px) {
        if (!strncmp(px->scheme, "socks", 5)) r = socks5(j, conn, u->host, u->port);
        else if (!strcmp(px->scheme, "https")) {
            char al[33];
            r = io->starttls(io->env, conn, px->host, 0, 0, al, tmo, j->msg, j->msglen);   /* TLS até o proxy */
            if (!r && tls) r = MNET_E_UNSUPPORTED;                /* TLS dentro de TLS: não suportado pelo transporte */
        } else if (tls) r = tunnel(j, conn, u->host, u->port);
    }
    if (!r && tls) {
        static const uint8_t both[] = {2, 'h', '2', 8, 'h', 't', 't', 'p', '/', '1', '.', '1'};
        static const uint8_t h11[] = {8, 'h', 't', 't', 'p', '/', '1', '.', '1'};
        char al[33] = {0};
        const uint8_t *a = j->q->http1_only ? h11 : both;
        size_t al_n = j->q->http1_only ? sizeof h11 : sizeof both;
        r = io->starttls(io->env, conn, u->host, a, al_n, al, tmo, j->msg, j->msglen);
        if (!r && !strcmp(al, "h2")) {
            mnet_h2_t *h = calloc(1, sizeof *h);
            if (!h || mnet_h2_init(h, 0)) { free(h); r = MNET_E_UNKNOWN; }
            else *h2 = h;
        }
        if (!r) memcpy(j->r->alpn, al, sizeof al);
        if (!r && j->q->on_tls) {
            void *tr = io->copy_trust ? io->copy_trust(io->env, conn) : 0;
            int stop = j->q->on_tls(j->q->ctx, u->host, u->port, tr);
            if (tr && j->q->release_trust) j->q->release_trust(tr);
            if (stop) { setmsg(j, "o desafio do servidor foi recusado"); r = MNET_E_CANCELLED; j->cancelled = 1; }
        }
    }
    if (r) { io->close(io->env, conn); return r; }
    *out = conn;
    return 0;
}

/* ---- cabeçalhos do pedido */
static void build_headers(job_t *j, const mnet_url_t *u, const char *method, mnet_hdrs_t *h, int *we_set_ae) {
    const mnet_req_t *q = j->q;
    for (int i = 0; q->hdrs && i < q->hdrs->n; i++) {
        const char *n = q->hdrs->v[i].name;
        /* o transporte decide estes */
        if (mnet_ieq(n, "host") || mnet_ieq(n, "content-length") || mnet_ieq(n, "connection") ||
            mnet_ieq(n, "transfer-encoding") || mnet_ieq(n, "keep-alive") || mnet_ieq(n, "proxy-connection") ||
            mnet_ieq(n, "upgrade") || mnet_ieq(n, "te")) continue;
        mnet_hdrs_add(h, n, strlen(n), q->hdrs->v[i].value, strlen(q->hdrs->v[i].value));
    }
    if (!q->bare_headers && !mnet_hdrs_get(h, "user-agent")) mnet_hdrs_add(h, "User-Agent", 10, j->c->user_agent, strlen(j->c->user_agent));
    if (!mnet_hdrs_get(h, "accept")) mnet_hdrs_add(h, "Accept", 6, "*/*", 3);
    if (!q->bare_headers && !mnet_hdrs_get(h, "accept-language")) mnet_hdrs_add(h, "Accept-Language", 15, "pt-BR,pt;q=0.9,en;q=0.8", 23);
    *we_set_ae = 0;
    if (!q->no_decode && !mnet_hdrs_get(h, "accept-encoding")) {
        mnet_hdrs_add(h, "Accept-Encoding", 15, "gzip, deflate, br", 17);
        *we_set_ae = 1;
    }
    /* valor "\x7f" no pedido = tirar o cabeçalho, inclusive os padrão (o "Nome:" vazio da libcurl) */
    for (int i = 0; q->hdrs && i < q->hdrs->n; i++)
        if (!strcmp(q->hdrs->v[i].value, "\x7f")) mnet_hdrs_del(h, q->hdrs->v[i].name);
    if (JAR(j) && !q->no_cookies && !mnet_hdrs_get(h, "cookie")) {
        lk(j->c);
        char *ck = mnet_jar_header(JAR(j), u->scheme, u->host, u->target, 1, j->c->io->unix_time(j->c->io->env));
        ulk(j->c);
        if (ck) { mnet_hdrs_add(h, "Cookie", 6, ck, strlen(ck)); free(ck); }
    }
    (void)method;
}

/* ---- corpo do pedido */
typedef struct { const mnet_req_t *q; size_t off; int ended; } body_t;
static long body_next(body_t *b, uint8_t *buf, size_t n) {
    if (b->ended) return 0;
    if (b->q->body_read) {
        long k = b->q->body_read(b->q->ctx, buf, n);
        if (k <= 0) b->ended = 1;
        return k;
    }
    size_t left = b->q->body_len - b->off;
    if (left > n) left = n;
    if (left) memcpy(buf, b->q->body + b->off, left);
    b->off += left;
    if (b->off == b->q->body_len) b->ended = 1;
    return (long)left;
}
static long long body_total(const mnet_req_t *q, int has_body) {
    if (!has_body) return -1;
    if (q->body_read) return q->body_stream_len;
    return (long long)q->body_len;
}

/* ---- uma ida HTTP/1.1. *retry = 1 quando a conexão reaproveitada morreu antes da resposta */
static int h1_once(job_t *j, void *conn, const mnet_url_t *u, const mnet_url_t *px, const char *method, int has_body,
                   int *keep, int *retry) {
    mnet_hdrs_t h = {0};
    int ae;
    build_headers(j, u, method, &h, &ae);
    if (px && !strcmp(u->scheme, "http") && strncmp(px->scheme, "socks", 5) && j->q->proxy_auth && strchr(j->q->proxy_auth, ' '))
        mnet_hdrs_add(&h, "Proxy-Authorization", 19, j->q->proxy_auth, strlen(j->q->proxy_auth));
    char *auth = mnet_url_authority(u);
    mnet_buf_t o = {0};
    mnet_buf_t target = {0};
    if (px && !strcmp(u->scheme, "http") && strncmp(px->scheme, "socks", 5)) {   /* proxy HTTP: forma absoluta */
        mnet_buf_puts(&target, "http://"); mnet_buf_puts(&target, auth); mnet_buf_puts(&target, u->target);
    } else mnet_buf_puts(&target, u->target);
    long long total = body_total(j->q, has_body);
    mnet_h1_request(&o, method, (char *)target.p, auth, &h, has_body ? (total >= 0 ? total : -2) : 0);
    free(auth); mnet_buf_free(&target); mnet_hdrs_free(&h);
    int r = wr(j, conn, o.p, o.n);
    mnet_buf_free(&o);
    if (r) { *retry = 1; return r == -3 ? MNET_E_CANCELLED : r == -2 ? MNET_E_TIMEOUT : MNET_E_LOST; }
    if (has_body) {
        body_t b = {j->q, 0, 0};
        uint8_t buf[16384];
        long long sent = 0;
        for (;;) {
            long k = body_next(&b, buf, sizeof buf);
            if (k < 0) { setmsg(j, "o corpo do pedido falhou"); return MNET_E_UNKNOWN; }
            if (total < 0) {
                mnet_buf_t ch = {0};
                mnet_h1_chunk(&ch, buf, (size_t)k);
                r = wr(j, conn, ch.p, ch.n);
                mnet_buf_free(&ch);
            } else if (k) r = wr(j, conn, buf, (size_t)k);
            if (r) return r == -3 ? MNET_E_CANCELLED : r == -2 ? MNET_E_TIMEOUT : MNET_E_LOST;
            sent += k;
            if (j->q->on_sent && k) j->q->on_sent(j->q->ctx, sent, total);
            if (!k) break;
        }
    }
    mnet_h1_t p = {0};
    p.ctx = j; p.on_body = h1_body; p.on_head = h1_head;
    mnet_h1_init(&p, mnet_ieq(method, "HEAD"));
    uint8_t buf[32768];
    int got_any = 0, ret = 0;
    for (;;) {
        long k = rd(j, conn, buf, sizeof buf);
        if (k == -3) { ret = MNET_E_CANCELLED; break; }
        if (k == -2) { setmsg(j, "tempo esgotado"); ret = MNET_E_TIMEOUT; break; }
        if (k <= 0) {
            if (!got_any) { *retry = 1; setmsg(j, "a conexão caiu antes da resposta"); ret = MNET_E_LOST; break; }
            if (mnet_h1_eof(&p) == 0) break;
            setmsg(j, p.err ? p.err : "conexão perdida"); ret = MNET_E_LOST; break;
        }
        got_any = 1;
        long used = mnet_h1_feed(&p, buf, (size_t)k);
        if (used < 0) {
            if (j->cancelled) { ret = MNET_E_CANCELLED; break; }
            if (j->dec_err) { setmsg(j, "corpo comprimido inválido"); ret = MNET_E_DECODE; break; }
            setmsg(j, p.err ? p.err : "resposta inválida"); ret = MNET_E_PARSE; break;
        }
        if (p.state == MNET_H1_DONE) { if (used < k) p.keep_alive = 0; break; }
    }
    *keep = !ret && p.keep_alive && p.state == MNET_H1_DONE;
    mnet_h1_free(&p);
    return ret;
}

/* ---- uma ida HTTP/2 */
/* ---- HTTP/2 com vários pedidos numa conexão (RFC 9113 §5): a conexão fica em c->h2live enquanto houver fluxo
 * livre. Não há fio próprio: o pedido que precisa de dados e não acha ninguém lendo vira o leitor da vez, alimenta
 * o mnet_h2 e põe cabeçalhos, dados e fim na caixa de cada pedido (pelo s->user). Cada pedido chama os callbacks do
 * app no próprio fio. A janela de cada fluxo só volta quando o pedido consome os dados (manual_win): quem lê devagar
 * segura só o próprio fluxo. Travas: io_mu serializa E/S no transporte e vem antes de mu, que guarda o estado h2. */
typedef struct h2conn {
    char key[300];
    void *conn;
    mnet_h2_t h2;
    mn_mutex_t mu, io_mu;
    mn_cond_t cv;
    int users, pumping, dead, priv;                /* priv: no_reuse, ninguém mais entra */
    long long last;
    struct h2conn *next;
} h2conn_t;
typedef struct {
    int head;                        /* cabeçalho final chegou, ainda não tratado */
    mnet_hdrs_t hdrs;
    mnet_buf_t data;
    int closed, code;
} h2box_t;
#define BOX(j) ((h2box_t *)(j)->box)

static void hc_free(mnet_client_t *c, h2conn_t *hc) {
    if (hc->conn) c->io->close(c->io->env, hc->conn);
    mnet_h2_free(&hc->h2);
    pthread_mutex_destroy(&hc->mu); pthread_mutex_destroy(&hc->io_mu); pthread_cond_destroy(&hc->cv);
    free(hc);
}
/* callbacks do mnet_h2 (com hc->mu presa): só enchem as caixas */
static void hc_head(void *ctx, mnet_h2_stream_t *s, const mnet_hdrs_t *h, int end) {
    (void)ctx; (void)end;
    job_t *j = s->user;
    if (!j) return;
    const char *st = mnet_hdrs_get(h, ":status");
    if (!st || st[0] < '2') return;                               /* 1xx e trailers */
    h2box_t *b = BOX(j);
    mnet_hdrs_free(&b->hdrs);
    for (int i = 0; i < h->n; i++) mnet_hdrs_add(&b->hdrs, h->v[i].name, strlen(h->v[i].name), h->v[i].value, strlen(h->v[i].value));
    b->head = 1;
}
static int hc_data(void *ctx, mnet_h2_stream_t *s, const uint8_t *d, size_t n, int end) {
    (void)ctx; (void)end;
    job_t *j = s->user;
    if (!j) return 1;                                             /* ninguém quer: RST_STREAM CANCEL */
    return mnet_buf_put(&BOX(j)->data, d, n) ? 1 : 0;
}
static void hc_close(void *ctx, mnet_h2_stream_t *s, int code) {
    (void)ctx;
    job_t *j = s->user;
    if (!j) return;
    BOX(j)->closed = 1; BOX(j)->code = code;
    j->st = 0;
}
/* escreve tudo na conexão compartilhada, sem olhar o cancelamento de um pedido (os quadros são de todos) */
static int hc_write(mnet_client_t *c, h2conn_t *hc, const uint8_t *d, size_t n) {
    size_t off = 0;
    while (off < n) {
        long k = c->io->write(c->io->env, hc->conn, d + off, n - off, 30000);
        if (k <= 0) return -1;
        off += (size_t)k;
    }
    return 0;
}
/* manda o que o mnet_h2 enfileirou; chame sem mu */
static int hc_flush(job_t *j, h2conn_t *hc) {
    pthread_mutex_lock(&hc->io_mu);
    pthread_mutex_lock(&hc->mu);
    mnet_buf_t out = hc->h2.out;
    memset(&hc->h2.out, 0, sizeof hc->h2.out);
    pthread_mutex_unlock(&hc->mu);
    int r = out.n ? hc_write(j->c, hc, out.p, out.n) : 0;
    pthread_mutex_unlock(&hc->io_mu);
    mnet_buf_free(&out);
    if (r) { pthread_mutex_lock(&hc->mu); hc->dead = 1; pthread_cond_broadcast(&hc->cv); pthread_mutex_unlock(&hc->mu); }
    return r;
}
/* uma leitura da conexão (o leitor da vez); chame sem mu. 0 ok/sem dados, -3 cancelado */
static int hc_pump(job_t *j, h2conn_t *hc) {
    const mnet_io_t *io = j->c->io;
    uint8_t buf[32768];
    long k;
    if (cancelled(j)) return -3;
    int w = wait_ms(j);
    if (w <= 0) w = 1;
    if (io->wait) {
        int ready = io->wait(io->env, hc->conn, 1, w);
        if (ready < 0) { pthread_mutex_lock(&hc->mu); hc->dead = 1; pthread_mutex_unlock(&hc->mu); }
        if (ready <= 0) return 0;
        pthread_mutex_lock(&hc->io_mu);
        k = io->read(io->env, hc->conn, buf, sizeof buf, 0);
        pthread_mutex_unlock(&hc->io_mu);
    } else {
        pthread_mutex_lock(&hc->io_mu);
        k = io->read(io->env, hc->conn, buf, sizeof buf, 0);
        pthread_mutex_unlock(&hc->io_mu);
        if (k == -2) { usleep(10000); return 0; }
    }
    if (k == -2) return 0;
    pthread_mutex_lock(&hc->mu);
    if (k <= 0) hc->dead = 1;
    else if (mnet_h2_feed(&hc->h2, buf, (size_t)k) < 0) hc->dead = 1;
    if (hc->h2.dead) hc->dead = 1;
    pthread_mutex_unlock(&hc->mu);
    hc_flush(j, hc);
    return 0;
}
/* espera algo mudar: vira o leitor da vez ou dorme até o leitor avisar. Chame com mu presa; volta com mu presa. */
static int hc_progress(job_t *j, h2conn_t *hc) {
    if (!hc->pumping) {
        hc->pumping = 1;
        pthread_mutex_unlock(&hc->mu);
        int r = hc_pump(j, hc);
        pthread_mutex_lock(&hc->mu);
        hc->pumping = 0;
        pthread_cond_broadcast(&hc->cv);
        return r;
    }
    pthread_cond_wait(&hc->cv, &hc->mu);
    return cancelled(j) ? -3 : 0;
}
/* conexão h2 viva da origem com fluxo livre (users++), limpando as ociosas vencidas */
static h2conn_t *hc_get(mnet_client_t *c, const char *key) {
    long long now = c->io->now_ms(c->io->env);
    h2conn_t *got = 0, *stale = 0;
    lk(c);
    for (h2conn_t **pp = (h2conn_t **)&c->h2live; *pp;) {
        h2conn_t *h = *pp;
        pthread_mutex_lock(&h->mu);
        int gone = !h->users && (h->dead || h->h2.goaway_recv || now - h->last > c->idle_timeout_ms);
        int ok = !gone && !got && !h->dead && !h->priv && !h->h2.goaway_recv && !strcmp(h->key, key) &&
                 (unsigned)h->users < (h->h2.peer_max_concurrent ? h->h2.peer_max_concurrent : 100);   /* um fluxo por usuário */
        if (ok) h->users++;
        pthread_mutex_unlock(&h->mu);
        if (gone) { *pp = h->next; h->next = stale; stale = h; continue; }
        if (ok) got = h;
        pp = &h->next;
    }
    ulk(c);
    while (stale) { h2conn_t *n = stale->next; hc_free(c, stale); stale = n; }
    return got;
}
static h2conn_t *hc_add(mnet_client_t *c, const char *key, void *conn, mnet_h2_t *h2, int priv) {
    h2conn_t *hc = calloc(1, sizeof *hc);
    if (!hc) return 0;
    memcpy(hc->key, key, sizeof hc->key);
    hc->conn = conn;
    hc->h2 = *h2;                                                 /* passa a ser dono do estado */
    free(h2);
    hc->h2.manual_win = 1;
    hc->h2.ctx = hc; hc->h2.on_headers = hc_head; hc->h2.on_data = hc_data; hc->h2.on_close = hc_close;
    hc->h2.on_goaway = 0; hc->h2.on_ping_ack = 0;
    pthread_mutex_init(&hc->mu, 0); pthread_mutex_init(&hc->io_mu, 0); pthread_cond_init(&hc->cv, 0);
    hc->users = 1;
    hc->priv = priv;
    lk(c);
    hc->next = c->h2live;
    c->h2live = hc;
    ulk(c);
    return hc;
}
static void hc_release(mnet_client_t *c, h2conn_t *hc) {
    pthread_mutex_lock(&hc->mu);
    hc->users--;
    hc->last = c->io->now_ms(c->io->env);
    int drop = !hc->users && (hc->dead || hc->priv || hc->h2.goaway_recv);
    pthread_mutex_unlock(&hc->mu);
    if (!drop) return;
    lk(c);
    int found = 0;
    for (h2conn_t **pp = (h2conn_t **)&c->h2live; *pp; pp = &(*pp)->next) if (*pp == hc) { *pp = hc->next; found = 1; break; }
    ulk(c);
    if (found) hc_free(c, hc);
}
/* trata o que chegou na caixa (sem mu): cabeçalho e dados, chamando o app */
static void box_take(job_t *j, h2box_t *b, int *have_head, mnet_hdrs_t *hdrs, mnet_buf_t *data) {
    *have_head = b->head;
    if (b->head) { *hdrs = b->hdrs; memset(&b->hdrs, 0, sizeof b->hdrs); b->head = 0; }
    *data = b->data; memset(&b->data, 0, sizeof b->data);
    (void)j;
}
static void box_head(job_t *j, const mnet_hdrs_t *h) {
    const char *st = mnet_hdrs_get(h, ":status");
    int code = (st[0] - '0') * 100 + (st[1] - '0') * 10 + (st[2] - '0');
    j->r->status = code;
    j->r->version = 20;
    mnet_hdrs_free(&j->r->hdrs);
    for (int i = 0; i < h->n; i++)
        if (h->v[i].name[0] != ':') mnet_hdrs_add(&j->r->hdrs, h->v[i].name, strlen(h->v[i].name), h->v[i].value, strlen(h->v[i].value));
    j->h2_head_ok = 1;
    if (head_ready(j, j->follow)) j->cancelled = 1;
}
static int h2_once(job_t *j, h2conn_t *hc, const mnet_url_t *u, const char *method, int has_body, int *retry) {
    h2box_t box;
    memset(&box, 0, sizeof box);
    j->box = &box;
    j->h2_head_ok = 0;
    mnet_hdrs_t base = {0}, hh = {0};
    int ae;
    build_headers(j, u, method, &base, &ae);
    char *auth = mnet_url_authority(u);
    mnet_hdrs_add(&hh, ":method", 7, method, strlen(method));
    mnet_hdrs_add(&hh, ":scheme", 7, u->scheme, strlen(u->scheme));
    mnet_hdrs_add(&hh, ":authority", 10, auth, strlen(auth));
    mnet_hdrs_add(&hh, ":path", 5, u->target, strlen(u->target));
    free(auth);
    long long total = body_total(j->q, has_body);
    for (int i = 0; i < base.n; i++) {
        /* nomes em minúsculas; Cookie dividido em campos (§8.2.3) ajuda a tabela HPACK */
        char *n = base.v[i].name;
        for (char *p = n; *p; p++) *p = (char)mnet_lc((unsigned char)*p);
        mnet_hdrs_add(&hh, n, strlen(n), base.v[i].value, strlen(base.v[i].value));
    }
    if (has_body && total >= 0) {
        mnet_buf_t t = {0}; mnet_buf_putu(&t, (unsigned long long)total);
        mnet_hdrs_add(&hh, "content-length", 14, (const char *)t.p, t.n);
        mnet_buf_free(&t);
    }
    mnet_hdrs_free(&base);
    int r = 0;
    pthread_mutex_lock(&hc->io_mu);                               /* HPACK: a ordem de codificação é a de envio */
    pthread_mutex_lock(&hc->mu);
    if (hc->dead || !mnet_h2_can_open(&hc->h2)) { *retry = 1; r = MNET_E_LOST; }
    else {
        j->st = mnet_h2_request(&hc->h2, &hh, !has_body);
        if (!j->st) { *retry = 1; r = MNET_E_LOST; }
        else j->st->user = j;
    }
    mnet_buf_t out = hc->h2.out;
    memset(&hc->h2.out, 0, sizeof hc->h2.out);
    pthread_mutex_unlock(&hc->mu);
    if (out.n && hc_write(j->c, hc, out.p, out.n)) r = r ? r : MNET_E_LOST;
    pthread_mutex_unlock(&hc->io_mu);
    mnet_buf_free(&out);
    mnet_hdrs_free(&hh);
    if (r) { j->box = 0; mnet_hdrs_free(&box.hdrs); return r; }
    long long idle_start = j->c->io->now_ms(j->c->io->env);
    if (has_body) {
        body_t b = {j->q, 0, 0};
        uint8_t buf[16384];
        size_t have = 0, off = 0;
        long long sent = 0;
        int ended = 0;
        while (!r && !(ended && off == have)) {
            if (off == have && !ended) {
                long k = body_next(&b, buf, sizeof buf);
                if (k < 0) { setmsg(j, "o corpo do pedido falhou"); r = MNET_E_UNKNOWN; break; }
                have = (size_t)k; off = 0;
                if (!k) ended = 1;
            }
            pthread_mutex_lock(&hc->mu);
            if (!j->st || hc->dead) { pthread_mutex_unlock(&hc->mu); break; }   /* o servidor já respondeu ou caiu */
            long k = mnet_h2_send_data(&hc->h2, j->st, buf + off, have - off,
                                       ended || (b.ended && !j->q->body_read && b.off == j->q->body_len));
            int fin = j->st && (j->st->state & 1);
            if (k == 0 && !fin) {
                if (hc_progress(j, hc) == -3) r = MNET_E_CANCELLED;  /* janela cheia: espera WINDOW_UPDATE */
                pthread_mutex_unlock(&hc->mu);
                continue;
            }
            pthread_mutex_unlock(&hc->mu);
            if (k < 0) { r = MNET_E_LOST; break; }
            off += (size_t)k; sent += k;
            if (j->q->on_sent && k) j->q->on_sent(j->q->ctx, sent, total);
            if (hc_flush(j, hc)) r = MNET_E_LOST;
            if (fin) break;
        }
        pthread_mutex_lock(&hc->mu);
        if (!r && j->st && !(j->st->state & 1)) mnet_h2_send_data(&hc->h2, j->st, 0, 0, 1);
        pthread_mutex_unlock(&hc->mu);
        if (!r) hc_flush(j, hc);
    }
    /* resposta */
    while (!r) {
        int have_head; mnet_hdrs_t hdrs = {0}; mnet_buf_t data = {0};
        pthread_mutex_lock(&hc->mu);
        while (!box.head && !box.data.n && !box.closed && !hc->dead && !r) {
            if (hc_progress(j, hc) == -3) r = MNET_E_CANCELLED;
            if (j->c->io->now_ms(j->c->io->env) - idle_start >= j->idle_ms || (j->deadline && j->c->io->now_ms(j->c->io->env) >= j->deadline)) {
                if (!box.head && !box.data.n && !box.closed) { setmsg(j, "tempo esgotado"); r = MNET_E_TIMEOUT; }
            }
        }
        box_take(j, &box, &have_head, &hdrs, &data);
        int closed = box.closed, code = box.code, dead = hc->dead;
        pthread_mutex_unlock(&hc->mu);
        if (have_head || data.n) idle_start = j->c->io->now_ms(j->c->io->env);
        if (have_head) box_head(j, &hdrs);
        mnet_hdrs_free(&hdrs);
        if (data.n && !j->cancelled) body_in(j, data.p, data.n);
        if (data.n) {
            pthread_mutex_lock(&hc->mu);
            if (j->st) mnet_h2_consumed(&hc->h2, j->st, data.n);
            pthread_mutex_unlock(&hc->mu);
            hc_flush(j, hc);
        }
        mnet_buf_free(&data);
        if (j->cancelled && !r) r = MNET_E_CANCELLED;
        if (r) break;
        if (closed) {
            if (code) {
                if (code == MNET_H2_REFUSED_STREAM && !j->status_seen) *retry = 1;
                setmsg(j, "o servidor encerrou o fluxo HTTP/2");
                r = MNET_E_LOST;
            }
            break;
        }
        if (dead) {
            pthread_mutex_lock(&hc->mu);
            int more = box.head || box.data.n || box.closed;
            pthread_mutex_unlock(&hc->mu);
            if (more) continue;
            if (!j->status_seen) *retry = 1;
            setmsg(j, hc->h2.err ? hc->h2.err : "a conexão HTTP/2 caiu");
            r = MNET_E_LOST;
        }
    }
    /* solta o fluxo: se ainda está aberto (cancelado, erro), RST_STREAM */
    pthread_mutex_lock(&hc->mu);
    if (j->st) {
        j->st->user = 0;
        if (!hc->dead) mnet_h2_reset(&hc->h2, j->st, MNET_H2_CANCEL);
        j->st = 0;
    }
    mnet_hdrs_free(&box.hdrs);
    mnet_buf_free(&box.data);
    j->box = 0;
    pthread_cond_broadcast(&hc->cv);
    pthread_mutex_unlock(&hc->mu);
    hc_flush(j, hc);
    if (!r && j->dec_err) { setmsg(j, "corpo comprimido inválido"); r = MNET_E_DECODE; }
    if (!r && !j->h2_head_ok) { setmsg(j, "resposta HTTP/2 sem cabeçalho"); r = MNET_E_BAD_RESPONSE; }
    return r;
}

/* ---- coalescência: uma origem HTTPS sem conexão abre uma só vez; quem chega junto espera o ALPN */
enum { OR_OPENING = 1, OR_H1 = 2, OR_H2 = 3 };
typedef struct origin { char key[300]; int state; long long t; struct origin *next; } origin_t;
static origin_t *or_find(mnet_client_t *c, const char *key) {
    for (origin_t *o = c->origins; o; o = o->next) if (!strcmp(o->key, key)) return o;
    return 0;
}
/* 1 = este pedido abre (marcou OR_OPENING); 0 = já se sabe que é HTTP/1.1, ou o outro terminou: procure de novo */
static int or_claim(job_t *j, const char *key) {
    mnet_client_t *c = j->c;
    int waited = 0;
    for (;;) {
        lk(c);
        origin_t *o = or_find(c, key);
        if (o && o->state == OR_OPENING) {
            ulk(c);
            if (cancelled(j) || wait_ms(j) <= 0) return 0;
            usleep(5000);
            waited = 1;
            continue;
        }
        if (waited || (o && o->state == OR_H1)) { ulk(c); return 0; }
        if (!o) {
            int n = 0;
            origin_t **old = 0;
            for (origin_t **pp = (origin_t **)&c->origins; *pp; pp = &(*pp)->next) {
                n++;
                if ((*pp)->state != OR_OPENING && (!old || (*pp)->t < (*old)->t)) old = pp;
            }
            if (n >= 64 && old) { origin_t *d = *old; *old = d->next; free(d); }   /* tira a mais velha */
            o = calloc(1, sizeof *o);
            if (o) { memcpy(o->key, key, sizeof o->key); o->next = c->origins; c->origins = o; }
        }
        if (o) { o->state = OR_OPENING; o->t = c->io->now_ms(c->io->env); }
        ulk(c);
        return o != 0;
    }
}
static void or_done(mnet_client_t *c, const char *key, int state) {
    lk(c);
    origin_t *o = or_find(c, key);
    if (o) { o->state = state; o->t = c->io->now_ms(c->io->env); }
    ulk(c);
}

/* ---- pedido inteiro, com redirects */
int mnet_fetch(mnet_client_t *c, const mnet_req_t *q, mnet_resp_t *r, char *msg, size_t msglen) {
    memset(r, 0, sizeof *r);
    if (msg && msglen) msg[0] = 0;
    job_t j = {0};
    j.c = c; j.q = q; j.r = r; j.msg = msg; j.msglen = msglen;
    j.idle_ms = q->timeout_ms > 0 ? q->timeout_ms : 60000;
    if (q->total_timeout_ms > 0) j.deadline = c->io->now_ms(c->io->env) + q->total_timeout_ms;
    char *url = mnet_strndup(q->url ? q->url : "", strlen(q->url ? q->url : ""));
    char *method = mnet_strndup(q->method ? q->method : "GET", strlen(q->method ? q->method : "GET"));
    int has_body = q->body_read != 0 || q->body_len > 0;
    int maxr = q->max_redirects < 0 ? 0 : q->max_redirects ? q->max_redirects : MAX_REDIRECTS;
    int ret = 0;
    mnet_url_t px; int have_px = 0;
    if (q->proxy && *q->proxy) {
        if (mnet_url_parse(q->proxy, &px)) { setmsg(&j, "proxy inválido"); free(url); free(method); return MNET_E_BAD_URL; }
        have_px = 1;
    }
    j.maxr = maxr;
    for (int hop = 0;; hop++) {
        mnet_url_t u;
        if (mnet_url_parse(url, &u)) { setmsg(&j, "URL inválida"); ret = MNET_E_BAD_URL; break; }
        if (strcmp(u.scheme, "http") && strcmp(u.scheme, "https")) { mnet_url_free(&u); setmsg(&j, "esquema não suportado"); ret = MNET_E_UNSUPPORTED; break; }
        free(r->url);
        r->url = mnet_strndup(url, strlen(url));
        r->redirects = hop;
        j.hop = hop;
        j.follow = q->on_redirect ? 1 : q->max_redirects >= 0;
        char key[300];
        {
            mnet_buf_t k = {0};
            mnet_buf_puts(&k, u.scheme); mnet_buf_puts(&k, "://"); mnet_buf_puts(&k, u.host); mnet_buf_putc(&k, ':');
            mnet_buf_putu(&k, (unsigned)u.port);
            if (have_px) { mnet_buf_puts(&k, "|"); mnet_buf_puts(&k, q->proxy); }
            size_t l = k.n < sizeof key - 1 ? k.n : sizeof key - 1;
            memcpy(key, k.p, l); key[l] = 0;
            mnet_buf_free(&k);
        }
        int tries = 0;
        for (;;) {
            int keep = 0, retry = 0;
            j.discard = 0; j.status_seen = 0; j.too_many = 0;
            free(j.next_url); j.next_url = 0;
            if (j.dec) { mnet_dec_free(j.dec); j.dec = 0; }
            /* HTTP/2 já aberto para a origem: mais um fluxo nele */
            h2conn_t *hc = q->no_reuse || q->http1_only ? 0 : hc_get(c, key);
            if (hc) {
                memcpy(r->alpn, "h2", 3);
                r->reused = 1;
                if (has_body && tries && q->body_read && (!q->body_rewind || q->body_rewind(q->ctx))) {
                    hc_release(c, hc); setmsg(&j, "o corpo do pedido não pode ser reenviado"); ret = MNET_E_LOST; break;
                }
                ret = h2_once(&j, hc, &u, method, has_body, &retry);
                hc_release(c, hc);
                if (ret && retry && tries == 0 && !j.status_seen) { tries++; if (msg) msg[0] = 0; continue; }
                break;
            }
            mnet_pool_ent_t *e = q->no_reuse ? 0 : pool_take(c, key);
            int reused = e != 0;
            int claimed = 0;
            if (!e && !q->no_reuse && !q->http1_only && !have_px && !strcmp(u.scheme, "https")) {
                claimed = or_claim(&j, key);
                if (!claimed && (hc = hc_get(c, key))) {             /* quem abria deu h2: entra nela */
                    memcpy(r->alpn, "h2", 3);
                    r->reused = 1;
                    ret = h2_once(&j, hc, &u, method, has_body, &retry);
                    hc_release(c, hc);
                    if (ret && retry && tries == 0 && !j.status_seen) { tries++; if (msg) msg[0] = 0; continue; }
                    break;
                }
                if (!claimed) e = pool_take(c, key);                 /* deu HTTP/1.1: talvez sobre uma ociosa */
                reused = e != 0;
            }
            if (!e) {
                e = calloc(1, sizeof *e);
                if (!e) { ret = MNET_E_UNKNOWN; break; }
                memcpy(e->key, key, sizeof key);
                mnet_h2_t *h2 = 0;
                ret = open_conn(&j, &u, have_px ? &px : 0, &e->conn, &h2);
                if (ret) { if (claimed) or_done(c, key, 0); free(e); break; }
                if (claimed && !h2) or_done(c, key, OR_H1);
                if (h2) {                                         /* ALPN h2: a conexão vira compartilhada */
                    h2conn_t *nh = hc_add(c, key, e->conn, h2, q->no_reuse);
                    if (claimed) or_done(c, key, OR_H2);           /* depois do hc_add: quem espera já a encontra */
                    free(e);
                    if (!nh) { ret = MNET_E_UNKNOWN; break; }
                    r->reused = 0;
                    if (has_body && tries && q->body_read && (!q->body_rewind || q->body_rewind(q->ctx))) {
                        hc_release(c, nh); setmsg(&j, "o corpo do pedido não pode ser reenviado"); ret = MNET_E_LOST; break;
                    }
                    ret = h2_once(&j, nh, &u, method, has_body, &retry);
                    hc_release(c, nh);
                    break;                                       /* conexão nova: não repete */
                }
            } else memcpy(r->alpn, !strcmp(u.scheme, "https") ? "http/1.1" : "", 9);
            r->reused = reused;
            if (has_body && tries && q->body_read && (!q->body_rewind || q->body_rewind(q->ctx))) {
                ent_close(c, e); setmsg(&j, "o corpo do pedido não pode ser reenviado"); ret = MNET_E_LOST; break;
            }
            ret = h1_once(&j, e->conn, &u, have_px ? &px : 0, method, has_body, &keep, &retry);
            if (keep && !ret && !q->no_reuse) pool_put(c, e); else ent_close(c, e);
            if (ret && retry && reused && tries == 0 && !j.status_seen) { tries++; if (msg) msg[0] = 0; continue; }
            break;
        }
        mnet_url_free(&u);
        if (ret) break;
        if (j.too_many) { setmsg(&j, "redirects demais"); ret = MNET_E_REDIRECTS; break; }
        if (j.next_url) {
            /* 301/302 com POST e 303 viram GET sem corpo; 307/308 repetem tudo */
            if ((r->status == 303 && !mnet_ieq(method, "HEAD")) || ((r->status == 301 || r->status == 302) && mnet_ieq(method, "POST"))) {
                free(method); method = mnet_strndup("GET", 3); has_body = 0;
            } else if (has_body && q->body_read && (!q->body_rewind || q->body_rewind(q->ctx))) {
                setmsg(&j, "o corpo não pode ser reenviado no redirect"); ret = MNET_E_UNKNOWN; break;
            }
            free(url);
            url = j.next_url; j.next_url = 0;
            mnet_hdrs_free(&r->hdrs);
            continue;
        }
        if (j.dec && mnet_dec_finish(j.dec)) {
            setmsg(&j, j.cancelled ? "cancelado" : "corpo comprimido truncado");
            ret = j.cancelled ? MNET_E_CANCELLED : MNET_E_DECODE;
        }
        break;
    }
    free(j.next_url);
    if (j.dec) mnet_dec_free(j.dec);
    if (have_px) mnet_url_free(&px);
    free(url); free(method);
    if (ret == MNET_E_CANCELLED) setmsg(&j, "cancelado");
    return ret;
}

/* ---------------------------------------------------------------- WebSocket cliente */
int mnet_ws_open(mnet_client_t *c, const mnet_req_t *q, const char *protocols, mnet_wsconn_t *w, char *msg, size_t msglen) {
    mnet_ws_t keep = w->ws;                                       /* callbacks já preenchidos pelo chamador */
    memset(w, 0, sizeof *w);
    w->ws = keep;
    w->c = c;
    if (msg && msglen) msg[0] = 0;
    job_t j = {0};
    j.c = c; j.q = q; j.r = &w->resp; j.msg = msg; j.msglen = msglen;
    j.idle_ms = q->timeout_ms > 0 ? q->timeout_ms : 60000;
    mnet_url_t u, px; int have_px = 0;
    if (mnet_url_parse(q->url, &u)) { setmsg(&j, "URL inválida"); return MNET_E_BAD_URL; }
    int tls;
    if (!strcmp(u.scheme, "ws")) tls = 0;
    else if (!strcmp(u.scheme, "wss")) tls = 1;
    else { mnet_url_free(&u); setmsg(&j, "esquema não suportado"); return MNET_E_UNSUPPORTED; }
    /* o transporte é o do http/https correspondente */
    memcpy(u.scheme, tls ? "https" : "http", tls ? 6 : 5);
    if (!u.port_explicit) u.port = tls ? 443 : 80;
    if (q->proxy && *q->proxy) { if (mnet_url_parse(q->proxy, &px)) { mnet_url_free(&u); return MNET_E_BAD_URL; } have_px = 1; }
    mnet_req_t q1 = *q;
    q1.http1_only = 1;                                            /* WebSocket sobre HTTP/2 (RFC 8441) não é oferecido */
    j.q = &q1;
    mnet_h2_t *h2 = 0;
    int r = open_conn(&j, &u, have_px ? &px : 0, &w->conn, &h2);
    if (h2) { mnet_h2_free(h2); free(h2); }
    if (r) { mnet_url_free(&u); if (have_px) mnet_url_free(&px); return r; }
    uint8_t rnd[16];
    if (w->ws.random) w->ws.random(w->ws.ctx, rnd, 16);
    else { long long t = c->io->now_ms(c->io->env); for (int i = 0; i < 16; i++) rnd[i] = (uint8_t)(t >> (i % 8 * 8)) ^ (uint8_t)(i * 37); }
    char key[25];
    mnet_ws_key(rnd, key);
    mnet_hdrs_t h = {0};
    int ae;
    build_headers(&j, &u, "GET", &h, &ae);
    mnet_hdrs_del(&h, "accept-encoding");
    char *auth = mnet_url_authority(&u);
    /* Origin padrão: o próprio servidor em http(s) */
    char origin[300];
    {
        mnet_buf_t ob = {0};
        mnet_buf_puts(&ob, tls ? "https://" : "http://"); mnet_buf_puts(&ob, auth);
        size_t l = ob.n < sizeof origin - 1 ? ob.n : sizeof origin - 1;
        memcpy(origin, ob.p, l); origin[l] = 0;
        mnet_buf_free(&ob);
    }
    const char *org = mnet_hdrs_get(&h, "origin");
    mnet_buf_t o = {0};
    mnet_ws_request(&o, u.target, auth, org ? 0 : origin, key, protocols, &h, 1);
    free(auth); mnet_hdrs_free(&h);
    r = wr(&j, w->conn, o.p, o.n);
    mnet_buf_free(&o);
    if (r) { mnet_url_free(&u); if (have_px) mnet_url_free(&px); return r == -2 ? MNET_E_TIMEOUT : MNET_E_LOST; }
    mnet_h1_t p = {0};
    mnet_h1_init(&p, 0);
    uint8_t buf[4096];
    int ret = 0;
    long used = 0, k = 0;
    while (!p.head_done) {
        k = rd(&j, w->conn, buf, sizeof buf);
        if (k == -3) { ret = MNET_E_CANCELLED; break; }
        if (k == -2) { setmsg(&j, "tempo esgotado"); ret = MNET_E_TIMEOUT; break; }
        if (k <= 0) { setmsg(&j, "a conexão caiu no handshake"); ret = MNET_E_LOST; break; }
        used = mnet_h1_feed(&p, buf, (size_t)k);
        if (used < 0) { setmsg(&j, p.err ? p.err : "resposta inválida"); ret = MNET_E_BAD_RESPONSE; break; }
    }
    if (!ret) {
        w->resp.status = p.status;
        w->resp.version = 11;
        for (int i = 0; i < p.hdrs.n; i++) mnet_hdrs_add(&w->resp.hdrs, p.hdrs.v[i].name, strlen(p.hdrs.v[i].name), p.hdrs.v[i].value, strlen(p.hdrs.v[i].value));
        w->resp.url = mnet_strndup(q->url, strlen(q->url));
        head_ready(&j, 0);                                        /* Set-Cookie do 101 */
        const char *why = 0;
        if (mnet_ws_client_accept(&w->ws, &p, key, protocols, &w->protocol, &why)) { setmsg(&j, why); ret = MNET_E_BAD_RESPONSE; }
    }
    if (!ret) {
        mnet_ws_init(&w->ws, 0);
        if (used < k) mnet_ws_feed(&w->ws, buf + used, (size_t)(k - used));   /* quadros que vieram junto do 101 */
    }
    mnet_h1_free(&p);
    mnet_url_free(&u);
    if (have_px) mnet_url_free(&px);
    if (ret) { c->io->close(c->io->env, w->conn); w->conn = 0; }
    return ret;
}
int mnet_wsconn_flush(mnet_wsconn_t *w, int tmo) {
    size_t off = 0;
    while (off < w->ws.out.n) {
        long k = w->c->io->write(w->c->io->env, w->conn, w->ws.out.p + off, w->ws.out.n - off, tmo);
        if (k == -2) { mnet_buf_consume(&w->ws.out, off); return -2; }
        if (k <= 0) return -1;
        off += (size_t)k;
    }
    w->ws.out.n = 0;
    return 0;
}
long mnet_wsconn_poll(mnet_wsconn_t *w, int tmo) {
    uint8_t buf[32768];
    long k = w->c->io->read(w->c->io->env, w->conn, buf, sizeof buf, tmo);
    if (k == -2) return 0;
    if (k <= 0) return -1;
    if (mnet_ws_feed(&w->ws, buf, (size_t)k) < 0) { mnet_wsconn_flush(w, 1000); return -1; }
    return k;
}
void mnet_wsconn_free(mnet_wsconn_t *w) {
    if (w->conn) w->c->io->close(w->c->io->env, w->conn);
    w->conn = 0;
    mnet_ws_free(&w->ws);
    free(w->protocol); w->protocol = 0;
    mnet_resp_free(&w->resp);
}
/* fecha as conexões h2 sem pedido em andamento */
void hc_drop_idle(mnet_client_t *c) {
    h2conn_t *drop = 0;
    lk(c);
    for (h2conn_t **pp = (h2conn_t **)&c->h2live; *pp;) {
        h2conn_t *h = *pp;
        pthread_mutex_lock(&h->mu);
        int idle = !h->users;
        pthread_mutex_unlock(&h->mu);
        if (idle) { *pp = h->next; h->next = drop; drop = h; continue; }
        pp = &h->next;
    }
    ulk(c);
    while (drop) { h2conn_t *n = drop->next; hc_free(c, drop); drop = n; }
}
