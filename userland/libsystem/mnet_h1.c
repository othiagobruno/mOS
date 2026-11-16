/* mOS — mnet: HTTP/1.1 (RFC 9112). Montagem do pedido e parser incremental de resposta. */
#include "mnet.h"

#define HEAD_MAX (256 * 1024)        /* status + cabeçalhos (ou trailers) de uma resposta */

static int strchr_s(const char *s, int c) { for (; *s; s++) if (*s == c) return 1; return 0; }
static const uint8_t *memchr_s(const uint8_t *d, int c, size_t n) {
    for (size_t i = 0; i < n; i++) if (d[i] == c) return d + i;
    return 0;
}

int mnet_h1_request(mnet_buf_t *o, const char *method, const char *target, const char *host,
                    const mnet_hdrs_t *h, long long body_len) {
    int r = 0;
    r |= mnet_buf_puts(o, method); r |= mnet_buf_putc(o, ' ');
    r |= mnet_buf_puts(o, target && *target ? target : "/");
    r |= mnet_buf_puts(o, " HTTP/1.1\r\n");
    if (host && (!h || !mnet_hdrs_get(h, "host"))) { r |= mnet_buf_puts(o, "Host: "); r |= mnet_buf_puts(o, host); r |= mnet_buf_puts(o, "\r\n"); }
    int has_len = h && mnet_hdrs_get(h, "content-length"), has_te = h && mnet_hdrs_get(h, "transfer-encoding");
    for (int i = 0; h && i < h->n; i++) {
        /* nada de CR/LF dentro de um campo: corta (evita injeção de cabeçalho) */
        const char *nm = h->v[i].name, *v = h->v[i].value;
        size_t nl = 0, vl = 0;
        while (nm[nl] && nm[nl] != '\r' && nm[nl] != '\n' && nm[nl] != ':') nl++;
        while (v[vl] && v[vl] != '\r' && v[vl] != '\n') vl++;
        if (!nl) continue;
        r |= mnet_buf_put(o, nm, nl); r |= mnet_buf_put(o, ": ", 2); r |= mnet_buf_put(o, v, vl); r |= mnet_buf_put(o, "\r\n", 2);
    }
    if (body_len == -2 && !has_te) r |= mnet_buf_puts(o, "Transfer-Encoding: chunked\r\n");
    else if (body_len >= 0 && !has_len && !has_te) {
        /* GET/HEAD sem corpo não mandam Content-Length: 0 (alguns servidores recusam) */
        int bare = body_len == 0 && (mnet_ieq(method, "GET") || mnet_ieq(method, "HEAD") || mnet_ieq(method, "DELETE") ||
                                     mnet_ieq(method, "OPTIONS") || mnet_ieq(method, "CONNECT"));
        if (!bare) { r |= mnet_buf_puts(o, "Content-Length: "); r |= mnet_buf_putu(o, (unsigned long long)body_len); r |= mnet_buf_puts(o, "\r\n"); }
    }
    r |= mnet_buf_puts(o, "\r\n");
    return r ? -1 : 0;
}

int mnet_h1_chunk(mnet_buf_t *o, const void *d, size_t n) {
    static const char hx[] = "0123456789abcdef";
    char t[20]; int i = 20;
    size_t v = n;
    do { t[--i] = hx[v & 15]; v >>= 4; } while (v);
    int r = mnet_buf_put(o, t + i, (size_t)(20 - i));
    r |= mnet_buf_put(o, "\r\n", 2);
    if (n) { r |= mnet_buf_put(o, d, n); r |= mnet_buf_put(o, "\r\n", 2); }
    else r |= mnet_buf_put(o, "\r\n", 2);
    return r ? -1 : 0;
}

void mnet_h1_init(mnet_h1_t *p, int no_body) {
    void *ctx = p->ctx;
    void (*oi)(void *, int, const mnet_hdrs_t *) = p->on_info;
    int (*ob)(void *, const uint8_t *, size_t) = p->on_body;
    memset(p, 0, sizeof *p);
    p->ctx = ctx; p->on_info = oi; p->on_body = ob;
    p->no_body = no_body;
    p->length = -1;
}
void mnet_h1_free(mnet_h1_t *p) {
    mnet_hdrs_free(&p->hdrs);
    mnet_hdrs_free(&p->trailers);
    mnet_buf_free(&p->line);
}
static long fail(mnet_h1_t *p, const char *e) { p->state = MNET_H1_ERROR; p->err = e; return -1; }
static int tchar(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           (c && strchr_s("!#$%&'*+-.^_`|~", c));
}

static int parse_status(mnet_h1_t *p, const char *l, size_t n) {
    if (n < 12 || memcmp(l, "HTTP/1.", 7) || l[7] < '0' || l[7] > '9' || l[8] != ' ') return -1;
    p->minor = l[7] - '0';
    if (l[9] < '0' || l[9] > '9' || l[10] < '0' || l[10] > '9' || l[11] < '0' || l[11] > '9') return -1;
    if (n > 12 && l[12] != ' ') return -1;
    p->status = (l[9] - '0') * 100 + (l[10] - '0') * 10 + (l[11] - '0');
    if (p->status < 100) return -1;
    size_t rl = n > 13 ? n - 13 : 0;
    if (rl >= sizeof p->reason) rl = sizeof p->reason - 1;
    if (rl) memcpy(p->reason, l + 13, rl);
    p->reason[rl] = 0;
    return 0;
}
/* uma linha de campo; fold = linha que começa com espaço (obs-fold: junta à anterior) */
static int parse_field(mnet_hdrs_t *h, const char *l, size_t n) {
    if (n && (l[0] == ' ' || l[0] == '\t')) {
        if (!h->n) return -1;
        while (n && (*l == ' ' || *l == '\t')) { l++; n--; }
        while (n && (l[n - 1] == ' ' || l[n - 1] == '\t')) n--;
        mnet_hdr_t *last = &h->v[h->n - 1];
        size_t ol = strlen(last->value);
        char *v = realloc(last->value, ol + 1 + n + 1);
        if (!v) return -1;
        v[ol] = ' '; memcpy(v + ol + 1, l, n); v[ol + 1 + n] = 0;
        last->value = v;
        return 0;
    }
    size_t c = 0;
    while (c < n && l[c] != ':') { if (!tchar((unsigned char)l[c])) return -1; c++; }
    if (c == n || c == 0) return -1;            /* sem ':' ou nome vazio; espaço antes do ':' também cai aqui */
    const char *v = l + c + 1;
    size_t vl = n - c - 1;
    while (vl && (*v == ' ' || *v == '\t')) { v++; vl--; }
    while (vl && (v[vl - 1] == ' ' || v[vl - 1] == '\t')) vl--;
    return mnet_hdrs_add(h, l, c, v, vl);
}
static int parse_len(const char *s, long long *out) {
    /* "10" ou "10, 10" (lista repetida, RFC 9110 §8.6): todos iguais */
    long long val = -1;
    const char *p = s;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == ',') p++;
        if (!*p) break;
        if (*p < '0' || *p > '9') return -1;
        long long v = 0;
        while (*p >= '0' && *p <= '9') { if (v > (1LL << 60)) return -1; v = v * 10 + (*p++ - '0'); }
        while (*p == ' ' || *p == '\t') p++;
        if (*p && *p != ',') return -1;
        if (val >= 0 && v != val) return -1;
        val = v;
    }
    if (val < 0) return -1;
    *out = val;
    return 0;
}
static int last_coding_chunked(const char *te) {
    const char *e = te + strlen(te);
    while (e > te && (e[-1] == ' ' || e[-1] == '\t')) e--;
    const char *s = e;
    while (s > te && s[-1] != ',') s--;
    while (s < e && (*s == ' ' || *s == '\t')) s++;
    return e - s == 7 && mnet_ieqn(s, "chunked", 7);
}

/* cabeçalhos completos: decide como o corpo termina */
static int head_end(mnet_h1_t *p) {
    if (p->status >= 100 && p->status < 200 && p->status != 101) {
        if (p->on_info) p->on_info(p->ctx, p->status, &p->hdrs);
        mnet_hdrs_free(&p->hdrs);
        p->state = MNET_H1_STATUS;
        return 0;
    }
    p->head_done = 1;
    const char *conn = mnet_hdrs_get(&p->hdrs, "connection");
    char *cj = mnet_hdrs_join(&p->hdrs, "connection");
    p->keep_alive = p->minor >= 1 ? !(cj && mnet_tok_has(cj, "close")) : (cj && mnet_tok_has(cj, "keep-alive"));
    free(cj);
    (void)conn;
    if (p->status == 101) { p->upgrade = 1; p->keep_alive = 0; p->state = MNET_H1_DONE; return 0; }
    if (p->no_body || p->status == 204 || p->status == 304) { p->state = MNET_H1_DONE; return 0; }
    char *te = mnet_hdrs_join(&p->hdrs, "transfer-encoding");
    if (te) {
        p->chunked = last_coding_chunked(te);
        free(te);
        if (p->chunked) { p->state = MNET_H1_CHUNK_SIZE; return 0; }
        p->keep_alive = 0;
        p->state = MNET_H1_BODY_CLOSE;
        return 0;
    }
    char *cl = mnet_hdrs_join(&p->hdrs, "content-length");
    if (cl) {
        long long v;
        int bad = parse_len(cl, &v);
        free(cl);
        if (bad) return -1;
        p->length = p->left = v;
        p->state = v ? MNET_H1_BODY_LEN : MNET_H1_DONE;
        return 0;
    }
    p->keep_alive = 0;
    p->state = MNET_H1_BODY_CLOSE;
    return 0;
}

static int line(mnet_h1_t *p, const char *l, size_t n) {
    switch (p->state) {
    case MNET_H1_STATUS:
        if (!n && !p->hdrs.n) return 0;              /* CRLF solto antes da linha de status: tolera */
        return parse_status(p, l, n) ? -1 : (p->state = MNET_H1_HEADERS, 0);
    case MNET_H1_HEADERS:
        if (!n) return head_end(p);
        return parse_field(&p->hdrs, l, n);
    case MNET_H1_CHUNK_SIZE: {
        size_t i = 0; long long v = 0;
        while (i < n && (l[i] == ' ' || l[i] == '\t')) i++;
        size_t st = i;
        for (; i < n; i++) {
            int c = l[i], d = c >= '0' && c <= '9' ? c - '0' : (c | 32) >= 'a' && (c | 32) <= 'f' ? (c | 32) - 'a' + 10 : -1;
            if (d < 0) break;
            if (v >> 56) return -1;
            v = v * 16 + d;
        }
        if (i == st) return -1;
        while (i < n && (l[i] == ' ' || l[i] == '\t')) i++;
        if (i < n && l[i] != ';') return -1;          /* extensões depois de ';' são ignoradas */
        p->left = v;
        p->state = v ? MNET_H1_CHUNK_DATA : MNET_H1_TRAILERS;
        return 0;
    }
    case MNET_H1_CHUNK_CRLF:
        if (n) return -1;
        p->state = MNET_H1_CHUNK_SIZE;
        return 0;
    case MNET_H1_TRAILERS:
        if (!n) { p->state = MNET_H1_DONE; return 0; }
        return parse_field(&p->trailers, l, n);
    }
    return -1;
}

long mnet_h1_feed(mnet_h1_t *p, const uint8_t *d, size_t n) {
    size_t i = 0;
    while (i < n) {
        switch (p->state) {
        case MNET_H1_DONE: return (long)i;
        case MNET_H1_ERROR: return -1;
        case MNET_H1_BODY_LEN:
        case MNET_H1_CHUNK_DATA:
        case MNET_H1_BODY_CLOSE: {
            size_t k = n - i;
            if (p->state != MNET_H1_BODY_CLOSE && (long long)k > p->left) k = (size_t)p->left;
            if (k && p->on_body && p->on_body(p->ctx, d + i, k)) return fail(p, "corpo recusado pelo cliente");
            i += k;
            if (p->state != MNET_H1_BODY_CLOSE) {
                p->left -= (long long)k;
                if (!p->left) p->state = p->state == MNET_H1_BODY_LEN ? MNET_H1_DONE : MNET_H1_CHUNK_CRLF;
            }
            break;
        }
        default: {
            const uint8_t *nl = memchr_s(d + i, '\n', n - i);
            size_t take = nl ? (size_t)(nl - (d + i)) + 1 : n - i;
            p->head_bytes += take;
            if (p->head_bytes > HEAD_MAX) return fail(p, "cabeçalho grande demais");
            if (!nl) {
                if (mnet_buf_put(&p->line, d + i, take)) return fail(p, "sem memória");
                i += take;
                break;
            }
            const char *l; size_t ln;
            if (p->line.n) {
                if (mnet_buf_put(&p->line, d + i, take)) return fail(p, "sem memória");
                l = (const char *)p->line.p; ln = p->line.n;
            } else { l = (const char *)d + i; ln = take; }
            i += take;
            ln--;                                     /* '\n' */
            if (ln && l[ln - 1] == '\r') ln--;
            for (size_t k = 0; k < ln; k++)
                if (l[k] == 0 || l[k] == '\r') { p->line.n = 0; return fail(p, "caractere inválido no cabeçalho"); }
            int st = p->state;
            int r = line(p, l, ln);
            p->line.n = 0;
            if (r) return fail(p, st == MNET_H1_STATUS ? "linha de status inválida" :
                                  st == MNET_H1_HEADERS ? "cabeçalho inválido" : "chunked inválido");
            if (st == MNET_H1_HEADERS && p->state != MNET_H1_HEADERS && p->state != MNET_H1_STATUS) p->head_bytes = 0;
            break;
        }
        }
    }
    return (long)i;
}

int mnet_h1_eof(mnet_h1_t *p) {
    if (p->state == MNET_H1_BODY_CLOSE) { p->state = MNET_H1_DONE; return 0; }
    if (p->state == MNET_H1_DONE) return 0;
    p->err = p->head_done ? "conexão fechada no meio do corpo" : "conexão fechada antes da resposta";
    p->state = MNET_H1_ERROR;
    return -1;
}
