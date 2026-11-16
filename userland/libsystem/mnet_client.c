/*
 * mOS — mnet: cliente HTTP. Um pedido por chamada (bloqueante, num fio do chamador), conexões reaproveitadas
 * num pool por origem (esquema, host, porta, proxy). HTTPS oferece ALPN h2 e http/1.1 e usa o que o servidor
 * escolher. Redirect (RFC 9110 §15.4), cookies (mnet_cookie.c), Content-Encoding (mnet_decode.c), proxy HTTP
 * (forma absoluta), túnel CONNECT para HTTPS e SOCKS5 (RFC 1928/1929, nome resolvido pelo proxy).
 *
 * HTTP/2: a conexão sai do pool enquanto um pedido a usa; pedidos simultâneos para a mesma origem abrem outra
 * conexão (como no 1.1). A multiplexação de vários pedidos numa só conexão fica com quem chamar mnet_h2_* direto.
 */
#include "mnet.h"

#define MAX_REDIRECTS 16
#define IDLE_MAX 32
#define IDLE_TIMEOUT 90000

struct mnet_pool_ent {
    char key[300];
    void *conn;
    mnet_h2_t *h2;
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
    if (e->h2) { mnet_h2_free(e->h2); free(e->h2); }
    if (e->conn) c->io->close(c->io->env, e->conn);
    free(e);
}
void mnet_client_close_idle(mnet_client_t *c) {
    lk(c);
    mnet_pool_ent_t *e = c->idle;
    c->idle = 0; c->idle_count = 0;
    ulk(c);
    while (e) { mnet_pool_ent_t *n = e->next; ent_close(c, e); e = n; }
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
    int h2_done, h2_code, h2_head_ok;
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
    if (!mnet_hdrs_get(h, "user-agent")) mnet_hdrs_add(h, "User-Agent", 10, j->c->user_agent, strlen(j->c->user_agent));
    if (!mnet_hdrs_get(h, "accept")) mnet_hdrs_add(h, "Accept", 6, "*/*", 3);
    if (!mnet_hdrs_get(h, "accept-language")) mnet_hdrs_add(h, "Accept-Language", 15, "pt-BR,pt;q=0.9,en;q=0.8", 23);
    *we_set_ae = 0;
    if (!q->no_decode && !mnet_hdrs_get(h, "accept-encoding")) {
        mnet_hdrs_add(h, "Accept-Encoding", 15, "gzip, deflate, br", 17);
        *we_set_ae = 1;
    }
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
static void h2_head(void *ctx, mnet_h2_stream_t *s, const mnet_hdrs_t *h, int end) {
    job_t *j = ctx;
    (void)end;
    if (s != j->st) return;
    const char *st = mnet_hdrs_get(h, ":status");
    if (!st) return;                                              /* trailers: nada a fazer */
    int code = (st[0] - '0') * 100 + (st[1] - '0') * 10 + (st[2] - '0');
    if (code < 200) return;
    j->r->status = code;
    j->r->version = 20;
    mnet_hdrs_free(&j->r->hdrs);
    for (int i = 0; i < h->n; i++)
        if (h->v[i].name[0] != ':') mnet_hdrs_add(&j->r->hdrs, h->v[i].name, strlen(h->v[i].name), h->v[i].value, strlen(h->v[i].value));
    j->h2_head_ok = 1;
    if (head_ready(j, j->follow)) j->cancelled = 1;
}
static int h2_dat(void *ctx, mnet_h2_stream_t *s, const uint8_t *d, size_t n, int end) {
    job_t *j = ctx;
    (void)end;
    if (s != j->st) return 0;
    return body_in(j, d, n) ? 1 : 0;
}
static void h2_cls(void *ctx, mnet_h2_stream_t *s, int code) {
    job_t *j = ctx;
    if (s == j->st) { j->h2_done = 1; j->h2_code = code; j->st = 0; }
}
static int h2_flush(job_t *j, void *conn, mnet_h2_t *h) {
    if (!h->out.n) return 0;
    int r = wr(j, conn, h->out.p, h->out.n);
    h->out.n = 0;
    return r;
}
static int h2_pump(job_t *j, void *conn, mnet_h2_t *h) {
    uint8_t buf[32768];
    if (h2_flush(j, conn, h)) return MNET_E_LOST;
    long k = rd(j, conn, buf, sizeof buf);
    if (k == -3) return MNET_E_CANCELLED;
    if (k == -2) { setmsg(j, "tempo esgotado"); return MNET_E_TIMEOUT; }
    if (k <= 0) { setmsg(j, "a conexão HTTP/2 caiu"); return MNET_E_LOST; }
    if (mnet_h2_feed(h, buf, (size_t)k) < 0) { h2_flush(j, conn, h); setmsg(j, h->err ? h->err : "erro HTTP/2"); return MNET_E_BAD_RESPONSE; }
    return h2_flush(j, conn, h) ? MNET_E_LOST : 0;
}
static int h2_once(job_t *j, void *conn, mnet_h2_t *h, const mnet_url_t *u, const char *method, int has_body,
                   int *keep, int *retry) {
    h->ctx = j; h->on_headers = h2_head; h->on_data = h2_dat; h->on_close = h2_cls;
    h->on_goaway = 0; h->on_ping_ack = 0;
    j->h2_done = 0; j->h2_code = 0; j->h2_head_ok = 0;
    if (!mnet_h2_can_open(h)) { *retry = 1; return MNET_E_LOST; }
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
    char lenbuf[24];
    for (int i = 0; i < base.n; i++) {
        /* nomes em minúsculas; Cookie dividido em campos (§8.2.3) ajuda a tabela HPACK */
        char *n = base.v[i].name;
        for (char *p = n; *p; p++) *p = (char)mnet_lc((unsigned char)*p);
        mnet_hdrs_add(&hh, n, strlen(n), base.v[i].value, strlen(base.v[i].value));
    }
    if (has_body && total >= 0) {
        mnet_buf_t t = {0}; mnet_buf_putu(&t, (unsigned long long)total);
        size_t l = t.n < sizeof lenbuf ? t.n : sizeof lenbuf - 1;
        memcpy(lenbuf, t.p, l); lenbuf[l] = 0; mnet_buf_free(&t);
        mnet_hdrs_add(&hh, "content-length", 14, lenbuf, strlen(lenbuf));
    }
    mnet_hdrs_free(&base);
    j->st = mnet_h2_request(h, &hh, !has_body);
    mnet_hdrs_free(&hh);
    if (!j->st) { *retry = 1; return MNET_E_LOST; }
    int r = 0;
    if (has_body) {
        body_t b = {j->q, 0, 0};
        uint8_t buf[16384];
        size_t have = 0, off = 0;
        long long sent = 0;
        int ended = 0;
        while (!r && j->st && !(ended && off == have)) {
            if (off == have && !ended) {
                long k = body_next(&b, buf, sizeof buf);
                if (k < 0) { setmsg(j, "o corpo do pedido falhou"); r = MNET_E_UNKNOWN; break; }
                have = (size_t)k; off = 0;
                if (!k) ended = 1;
            }
            long k = mnet_h2_send_data(h, j->st, buf + off, have - off, ended || (b.ended && !j->q->body_read && b.off == j->q->body_len));
            if (k < 0) { r = MNET_E_LOST; break; }
            off += (size_t)k; sent += k;
            if (j->q->on_sent && k) j->q->on_sent(j->q->ctx, sent, total);
            if (j->st && (j->st->state & 1)) break;                 /* END_STREAM foi */
            if (!k || off < have) r = h2_pump(j, conn, h);           /* janela cheia: espera WINDOW_UPDATE */
            else if (h2_flush(j, conn, h)) r = MNET_E_LOST;
        }
        if (!r && j->st && !(j->st->state & 1)) { mnet_h2_send_data(h, j->st, 0, 0, 1); }
    }
    while (!r && !j->h2_done) {
        r = h2_pump(j, conn, h);
        if (j->cancelled) r = MNET_E_CANCELLED;
    }
    if (r == MNET_E_CANCELLED && j->st) { mnet_h2_reset(h, j->st, MNET_H2_CANCEL); h2_flush(j, conn, h); }
    if (!r && j->h2_code) {
        if (j->h2_code == MNET_H2_REFUSED_STREAM && !j->status_seen) *retry = 1;
        setmsg(j, "o servidor encerrou o fluxo HTTP/2");
        r = j->cancelled ? MNET_E_CANCELLED : MNET_E_LOST;
    }
    if (!r && j->dec_err) { setmsg(j, "corpo comprimido inválido"); r = MNET_E_DECODE; }
    if (!r && !j->h2_head_ok) { setmsg(j, "resposta HTTP/2 sem cabeçalho"); r = MNET_E_BAD_RESPONSE; }
    *keep = !h->dead && !h->goaway_recv && (!r || r == MNET_E_CANCELLED);
    return r;
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
            mnet_pool_ent_t *e = q->no_reuse ? 0 : pool_take(c, key);
            int reused = e != 0;
            if (!e) {
                e = calloc(1, sizeof *e);
                if (!e) { ret = MNET_E_UNKNOWN; break; }
                memcpy(e->key, key, sizeof key);
                ret = open_conn(&j, &u, have_px ? &px : 0, &e->conn, &e->h2);
                if (ret) { free(e); break; }
            } else if (e->h2) memcpy(r->alpn, "h2", 3);
            else memcpy(r->alpn, !strcmp(u.scheme, "https") ? "http/1.1" : "", 9);
            r->reused = reused;
            if (has_body && tries && q->body_read && (!q->body_rewind || q->body_rewind(q->ctx))) {
                ent_close(c, e); setmsg(&j, "o corpo do pedido não pode ser reenviado"); ret = MNET_E_LOST; break;
            }
            int keep = 0, retry = 0;
            j.discard = 0; j.status_seen = 0; j.too_many = 0;
            free(j.next_url); j.next_url = 0;
            if (j.dec) { mnet_dec_free(j.dec); j.dec = 0; }
            ret = e->h2 ? h2_once(&j, e->conn, e->h2, &u, method, has_body, &keep, &retry)
                        : h1_once(&j, e->conn, &u, have_px ? &px : 0, method, has_body, &keep, &retry);
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
