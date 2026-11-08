/* mOS — cliente HTTP/1.1 assíncrono (http:// e https://)
 * DNS → TCP → (TLS) → requisição; Content-Length, chunked ou leitura até o fechamento;
 * até 5 redirecionamentos (absolutos e relativos); tempo limite de inatividade de 15 s. */
#include "netpriv.h"

#define HTTP_TIMEOUT_MS   15000
#define HTTP_MAX_REDIR    5
#define HTTP_HDR_MAX      (32 * 1024)
#define HTTP_BODY_MAX     (128u * 1024 * 1024)
#define HTTP_READ_PER_POLL (256 * 1024)
#define HTTP_USER_AGENT   "mOS/0.1 Horizonte"

enum { P_RESOLVE, P_CONNECT, P_TLS, P_SEND, P_HEAD, P_BODY, P_DONE };
enum { CH_SIZE, CH_DATA, CH_DATA_END, CH_TRAILER, CH_DONE };

typedef struct {
    /* requisição */
    char method[8];
    char req_ct[96];
    u8  *req_body;
    u32  req_len;
    /* URL atual */
    char cur[512];
    bool https;
    char host[256];
    u16  port;
    char path[1024];
    /* conexão */
    int  phase;
    u32  ip;
    int  sock;
    tls_conn_t *tls;
    u64  deadline;
    int  redirects;
    u8  *out; u32 out_len, out_off;
    char *hdr; u32 hdr_len;
    /* resposta */
    i64  content_length;    /* -1 desconhecido */
    bool chunked;
    u32  cap;
    int  ch_state;
    u32  ch_left;
    char ch_line[80];
    u32  ch_line_len;
} hp_t;

/* =================================================================== URL */
static bool url_parse(const char *url, bool *https, char *host, u16 *port, char *path, char *err, usize errn) {
    while (*url == ' ' || *url == '\t') url++;
    const char *p = url;
    *https = false;
    if (!strncasecmp(p, "http://", 7)) p += 7;
    else if (!strncasecmp(p, "https://", 8)) { p += 8; *https = true; }
    else if (strstr(p, "://")) { snprintf(err, errn, "esquema de URL não suportado"); return false; }
    *port = *https ? 443 : 80;

    const char *a = p;
    while (*p && *p != '/' && *p != '?' && *p != '#') p++;
    const char *ae = p;
    for (const char *q = a; q < ae; q++) if (*q == '@') a = q + 1;   /* descarta credenciais */
    if (a < ae && *a == '[') { snprintf(err, errn, "endereços IPv6 não são suportados"); return false; }
    const char *colon = NULL;
    for (const char *q = a; q < ae; q++) if (*q == ':') colon = q;
    const char *he = colon ? colon : ae;
    usize hl = (usize)(he - a);
    if (hl == 0 || hl > 253) { snprintf(err, errn, "URL inválida"); return false; }
    for (usize i = 0; i < hl; i++) host[i] = (char)tolower(a[i]);
    host[hl] = 0;
    if (colon) {
        u32 pv = 0;
        const char *q = colon + 1;
        if (q == ae) pv = *port;
        for (; q < ae; q++) {
            if (!isdigit(*q)) { snprintf(err, errn, "porta inválida na URL"); return false; }
            pv = pv * 10 + (u32)(*q - '0');
            if (pv > 65535) { snprintf(err, errn, "porta inválida na URL"); return false; }
        }
        if (!pv) { snprintf(err, errn, "porta inválida na URL"); return false; }
        *port = (u16)pv;
    }
    /* caminho + consulta (sem fragmento), com escape de caracteres não permitidos */
    usize o = 0;
    if (*p != '/') path[o++] = '/';
    static const char hex[] = "0123456789ABCDEF";
    for (; *p && *p != '#'; p++) {
        u8 c = (u8)*p;
        if (o + 4 >= 1024) { snprintf(err, errn, "URL longa demais"); return false; }
        if (c <= 0x20 || c >= 0x7F || c == '"' || c == '<' || c == '>') {
            path[o++] = '%'; path[o++] = hex[c >> 4]; path[o++] = hex[c & 15];
        } else path[o++] = (char)c;
    }
    path[o] = 0;
    return true;
}

static void host_port(hp_t *p, char *out, usize n) {
    bool def = (p->https && p->port == 443) || (!p->https && p->port == 80);
    if (def) snprintf(out, n, "%s", p->host);
    else snprintf(out, n, "%s:%u", p->host, p->port);
}

/* resolve um Location relativo à URL atual */
static void resolve_location(hp_t *p, const char *loc, char *out, usize n) {
    while (*loc == ' ') loc++;
    const char *scheme = p->https ? "https" : "http";
    char hp[280];
    host_port(p, hp, sizeof hp);
    const char *sch = strstr(loc, "://");
    const char *slash = strchr(loc, '/');
    if (sch && slash && sch < slash) {
        snprintf(out, n, "%s", loc);    /* absoluto (esquema://...) */
    } else if (!strncasecmp(loc, "http://", 7) || !strncasecmp(loc, "https://", 8)) {
        snprintf(out, n, "%s", loc);
    } else if (loc[0] == '/' && loc[1] == '/') {
        snprintf(out, n, "%s:%s", scheme, loc);
    } else if (loc[0] == '/') {
        snprintf(out, n, "%s://%s%s", scheme, hp, loc);
    } else {
        char base[1024];
        strncpy(base, p->path, sizeof base - 1);
        base[sizeof base - 1] = 0;
        char *q = strchr(base, '?');
        if (loc[0] == '?') { if (q) *q = 0; }
        else {
            if (q) *q = 0;
            char *sl = strrchr(base, '/');
            if (sl) sl[1] = 0; else strcpy(base, "/");
        }
        snprintf(out, n, "%s://%s%s%s", scheme, hp, base, loc);
    }
}

/* =================================================================== transporte (TCP ou TLS) */
static int xp_send(hp_t *p, const void *d, int n) { return p->tls ? tls_send(p->tls, d, n) : tcp_send(p->sock, d, n); }
static int xp_recv(hp_t *p, void *b, int n) { return p->tls ? tls_recv(p->tls, b, n) : tcp_recv(p->sock, b, n); }
static void xp_close(hp_t *p) {
    if (p->tls) { tls_free(p->tls); p->tls = NULL; }
    if (p->sock >= 0) { tcp_close(p->sock); p->sock = -1; }
}

/* =================================================================== estado */
static int fail(http_req_t *r, const char *fmt, ...) {
    hp_t *p = r->priv;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(r->error, sizeof r->error, fmt, ap);
    va_end(ap);
    if (p) xp_close(p);
    r->state = -1;
    kprintf("http: %s: %s\n", r->url, r->error);
    return -1;
}

static void touch(hp_t *p) { p->deadline = timer_ticks() + HTTP_TIMEOUT_MS; }

static bool start_url(http_req_t *r, const char *url) {
    hp_t *p = r->priv;
    strncpy(p->cur, url, sizeof p->cur - 1);
    p->cur[sizeof p->cur - 1] = 0;
    if (!url_parse(url, &p->https, p->host, &p->port, p->path, r->error, sizeof r->error)) { r->state = -1; return false; }
    xp_close(p);
    kfree(p->out); p->out = NULL;
    kfree(p->hdr); p->hdr = NULL;
    p->hdr_len = 0;
    kfree(r->body); r->body = NULL;
    r->body_len = 0;
    r->status = 0;
    r->content_type[0] = 0;
    p->cap = 0;
    p->content_length = -1;
    p->chunked = false;
    p->ch_state = CH_SIZE;
    p->ch_line_len = 0;
    p->phase = P_RESOLVE;
    touch(p);

    /* monta a requisição */
    char hp[280];
    host_port(p, hp, sizeof hp);
    char head[2048];
    int hl;
    bool has_body = !strcmp(p->method, "POST");
    if (has_body)
        hl = snprintf(head, sizeof head,
                      "%s %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: " HTTP_USER_AGENT "\r\nAccept: */*\r\n"
                      "Accept-Encoding: identity\r\nConnection: close\r\nContent-Type: %s\r\nContent-Length: %u\r\n\r\n",
                      p->method, p->path, hp, p->req_ct[0] ? p->req_ct : "application/octet-stream", p->req_len);
    else
        hl = snprintf(head, sizeof head,
                      "%s %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: " HTTP_USER_AGENT "\r\nAccept: */*\r\n"
                      "Accept-Encoding: identity\r\nConnection: close\r\n\r\n",
                      p->method, p->path, hp);
    if (hl < 0 || hl >= (int)sizeof head) { snprintf(r->error, sizeof r->error, "URL longa demais"); r->state = -1; return false; }
    u32 bl = has_body ? p->req_len : 0;
    p->out = kmalloc((usize)hl + bl);
    if (!p->out) { snprintf(r->error, sizeof r->error, "sem memória"); r->state = -1; return false; }
    memcpy(p->out, head, hl);
    if (bl) memcpy(p->out + hl, p->req_body, bl);
    p->out_len = (u32)hl + bl;
    p->out_off = 0;
    return true;
}

static http_req_t *req_new(const char *url, const char *method) {
    http_req_t *r = kcalloc(1, sizeof(http_req_t));
    if (!r) return NULL;
    hp_t *p = kcalloc(1, sizeof(hp_t));
    if (!p) { kfree(r); return NULL; }
    r->priv = p;
    p->sock = -1;
    strncpy(r->url, url ? url : "", sizeof r->url - 1);
    strcpy(p->method, method);
    return r;
}

http_req_t *http_get(const char *url) {
    http_req_t *r = req_new(url, "GET");
    if (r) start_url(r, r->url);
    return r;
}

http_req_t *http_post(const char *url, const char *content_type, const void *body, u32 len) {
    http_req_t *r = req_new(url, "POST");
    if (!r) return NULL;
    hp_t *p = r->priv;
    if (content_type) strncpy(p->req_ct, content_type, sizeof p->req_ct - 1);
    if (len) {
        p->req_body = kmalloc(len);
        if (!p->req_body) { snprintf(r->error, sizeof r->error, "sem memória"); r->state = -1; return r; }
        memcpy(p->req_body, body, len);
        p->req_len = len;
    }
    start_url(r, r->url);
    return r;
}

void http_free(http_req_t *r) {
    if (!r) return;
    hp_t *p = r->priv;
    if (p) {
        xp_close(p);
        kfree(p->out);
        kfree(p->hdr);
        kfree(p->req_body);
        kfree(p);
    }
    kfree(r->body);
    kfree(r);
}

float http_progress(http_req_t *r) {
    if (!r) return 0;
    if (r->state == 1) return 1.0f;
    hp_t *p = r->priv;
    if (!p || r->state < 0) return 0;
    switch (p->phase) {
    case P_RESOLVE: return 0.02f;
    case P_CONNECT: return 0.05f;
    case P_TLS:     return 0.08f;
    case P_SEND:    return 0.10f;
    case P_HEAD:    return 0.12f;
    case P_BODY:
        if (p->content_length > 0) return 0.15f + 0.85f * (float)r->body_len / (float)p->content_length;
        return 0.15f + 0.8f * (1.0f - 1.0f / (1.0f + (float)r->body_len / 131072.0f));
    default: return 1.0f;
    }
}

/* =================================================================== corpo */
static bool body_append(http_req_t *r, const u8 *d, u32 n) {
    hp_t *p = r->priv;
    if (!n) return true;
    u64 lim = r->max_body ? r->max_body : HTTP_BODY_MAX;
    if ((u64)r->body_len + n + 1 > lim) { fail(r, "resposta grande demais"); return false; }
    if (r->body_len + n + 1 > p->cap) {
        u32 nc = p->cap ? p->cap : 16384;
        /* tamanho conhecido (Content-Length): uma alocação só, sem dobrar até o dobro do necessário */
        if (!p->cap && p->content_length > 0 && (u64)p->content_length + 1 <= lim) nc = (u32)p->content_length + 1;
        while (nc < r->body_len + n + 1) nc *= 2;
        u8 *nb = krealloc(r->body, nc);
        if (!nb) { fail(r, "sem memória para a resposta (%u bytes)", nc); return false; }
        r->body = nb;
        p->cap = nc;
    }
    memcpy(r->body + r->body_len, d, n);
    r->body_len += n;
    r->body[r->body_len] = 0;
    return true;
}

static void finish(http_req_t *r) {
    hp_t *p = r->priv;
    xp_close(p);
    if (!r->body) { r->body = kmalloc(1); if (r->body) r->body[0] = 0; }
    strncpy(r->location, p->cur, sizeof r->location - 1);
    p->phase = P_DONE;
    r->state = 1;
}

/* consome bytes do corpo; devolve false em erro */
static bool body_feed(http_req_t *r, const u8 *d, u32 n) {
    hp_t *p = r->priv;
    if (!p->chunked) {
        if (p->content_length >= 0) {
            u64 left = (u64)p->content_length - r->body_len;
            if (n > left) n = (u32)left;
        }
        if (!body_append(r, d, n)) return false;
        if (p->content_length >= 0 && r->body_len >= (u64)p->content_length) finish(r);
        return true;
    }
    u32 i = 0;
    while (i < n && p->ch_state != CH_DONE) {
        switch (p->ch_state) {
        case CH_SIZE:
        case CH_TRAILER: {
            char c = (char)d[i++];
            if (c != '\n') {
                if (p->ch_line_len < sizeof p->ch_line - 1) p->ch_line[p->ch_line_len++] = c;
                break;
            }
            p->ch_line[p->ch_line_len] = 0;
            if (p->ch_line_len && p->ch_line[p->ch_line_len - 1] == '\r') p->ch_line[--p->ch_line_len] = 0;
            if (p->ch_state == CH_TRAILER) {
                if (p->ch_line_len == 0) p->ch_state = CH_DONE;
                p->ch_line_len = 0;
                break;
            }
            u32 sz = 0;
            int digits = 0;
            for (char *q = p->ch_line; *q; q++) {
                int v = isdigit(*q) ? *q - '0' : (tolower(*q) >= 'a' && tolower(*q) <= 'f') ? tolower(*q) - 'a' + 10 : -1;
                if (v < 0) break;
                if (sz > 0x0FFFFFFF) { fail(r, "codificação chunked inválida"); return false; }
                sz = sz * 16 + (u32)v;
                digits++;
            }
            p->ch_line_len = 0;
            if (!digits) {
                if (p->ch_line[0] == 0) break;   /* linha vazia extra: tolera */
                fail(r, "codificação chunked inválida");
                return false;
            }
            if (sz == 0) p->ch_state = CH_TRAILER;
            else { p->ch_left = sz; p->ch_state = CH_DATA; }
            break;
        }
        case CH_DATA: {
            u32 k = MIN(n - i, p->ch_left);
            if (!body_append(r, d + i, k)) return false;
            i += k;
            p->ch_left -= k;
            if (!p->ch_left) p->ch_state = CH_DATA_END;
            break;
        }
        case CH_DATA_END:
            if (d[i++] == '\n') p->ch_state = CH_SIZE;
            break;
        }
    }
    if (p->ch_state == CH_DONE) finish(r);
    return true;
}

/* =================================================================== cabeçalho */
static void trim(char **s, char *e) {
    while (**s == ' ' || **s == '\t') (*s)++;
    while (e > *s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r')) *--e = 0;
}

/* processa o cabeçalho completo (hdr[0..end)) e o restante já recebido */
static int on_header(http_req_t *r, u32 end) {
    hp_t *p = r->priv;
    p->hdr[end - 1] = 0;
    char *h = p->hdr;
    if (strncmp(h, "HTTP/", 5)) return fail(r, "resposta HTTP inválida");
    char *sp = strchr(h, ' ');
    if (!sp || !isdigit(sp[1]) || !isdigit(sp[2]) || !isdigit(sp[3])) return fail(r, "resposta HTTP inválida");
    int status = (sp[1] - '0') * 100 + (sp[2] - '0') * 10 + (sp[3] - '0');
    u32 rest_off = end, rest_len = p->hdr_len - end;

    if (status >= 100 && status < 200) {
        /* resposta provisória (100 Continue): descarta e espera a definitiva */
        memmove(p->hdr, p->hdr + rest_off, rest_len);
        p->hdr_len = rest_len;
        return 0;
    }
    r->status = status;
    char location[512] = "";
    p->content_length = -1;
    p->chunked = false;
    char *line = strchr(h, '\n');
    while (line) {
        line++;
        char *nl = strchr(line, '\n');
        if (nl) *nl = 0;
        char *colon = strchr(line, ':');
        if (colon) {
            *colon = 0;
            char *name = line, *val = colon + 1;
            trim(&name, colon);
            trim(&val, val + strlen(val));
            if (!strcasecmp(name, "Content-Length")) {
                i64 v = 0;
                bool ok = isdigit(*val);
                for (char *q = val; isdigit(*q); q++) v = v * 10 + (*q - '0');
                if (ok) p->content_length = v;
            } else if (!strcasecmp(name, "Transfer-Encoding")) {
                for (char *q = val; *q; q++) if (!strncasecmp(q, "chunked", 7)) p->chunked = true;
            } else if (!strcasecmp(name, "Content-Type")) {
                strncpy(r->content_type, val, sizeof r->content_type - 1);
                r->content_type[sizeof r->content_type - 1] = 0;
            } else if (!strcasecmp(name, "Location")) {
                strncpy(location, val, sizeof location - 1);
            }
        }
        line = nl;
    }
    if (p->chunked) p->content_length = -1;

    bool redirect = (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) && location[0];
    if (redirect) {
        if (p->redirects >= HTTP_MAX_REDIR) return fail(r, "redirecionamentos em excesso");
        p->redirects++;
        char next[512];
        resolve_location(p, location, next, sizeof next);
        kprintf("http: %d → %s\n", status, next);
        if (status == 303 || ((status == 301 || status == 302) && !strcmp(p->method, "POST"))) {
            strcpy(p->method, "GET");
            kfree(p->req_body); p->req_body = NULL; p->req_len = 0;
        }
        if (!start_url(r, next)) { kprintf("http: %s\n", r->error); xp_close(p); return -1; }
        return 0;
    }

    bool no_body = status == 204 || status == 304 || !strcmp(p->method, "HEAD");
    p->phase = P_BODY;
    if (no_body || p->content_length == 0) { finish(r); return 1; }
    if (p->content_length > 0) {
        if ((u64)p->content_length + 1 > HTTP_BODY_MAX) return fail(r, "resposta grande demais (%llu bytes)", (u64)p->content_length);
        p->cap = (u32)p->content_length + 1;
        r->body = kmalloc(p->cap);
        if (!r->body) return fail(r, "sem memória para a resposta (%llu bytes)", (u64)p->content_length);
    }
    if (rest_len && !body_feed(r, (u8 *)p->hdr + rest_off, rest_len)) return -1;
    kfree(p->hdr); p->hdr = NULL; p->hdr_len = 0;
    return r->state == 1 ? 1 : 0;
}

/* =================================================================== máquina de estados */
int http_poll(http_req_t *r) {
    if (!r) return -1;
    if (r->state) return r->state;
    hp_t *p = r->priv;
    u64 now = timer_ticks();
    static u8 buf[16384];

    for (int iter = 0; iter < 8 && r->state == 0; iter++) {
        switch (p->phase) {
        case P_RESOLVE: {
            int d = dns_resolve(p->host, &p->ip);
            if (d < 0) return fail(r, "não foi possível resolver %s", p->host);
            if (d == 0) {
                if (now > p->deadline) return fail(r, net_configured() ? "tempo esgotado ao resolver %s" : "sem conexão de rede (%s)", p->host);
                return 0;
            }
            if (!net_configured()) return fail(r, "sem conexão de rede");
            p->sock = tcp_connect(p->ip, p->port);
            if (p->sock < 0) return fail(r, "não foi possível abrir conexão com %s", p->host);
            p->phase = P_CONNECT;
            touch(p);
            break;
        }
        case P_CONNECT: {
            int st = tcp_state(p->sock);
            if (st == TCP_CONNECTING) {
                if (now > p->deadline) return fail(r, "tempo esgotado ao conectar a %s", p->host);
                return 0;
            }
            if (st == TCP_ERROR) return fail(r, "%s:%u: %s", p->host, p->port, tcp_error_reason(p->sock));
            if (st != TCP_OPEN) return fail(r, "conexão encerrada por %s", p->host);
            if (p->https) {
                p->tls = tls_new(p->sock, p->host);
                if (!p->tls) return fail(r, "não foi possível iniciar a sessão TLS com %s", p->host);
                p->phase = P_TLS;
            } else p->phase = P_SEND;
            touch(p);
            break;
        }
        case P_TLS: {
            int h = tls_handshake(p->tls);
            if (h < 0) {
                const char *e = tls_error(p->tls);
                return fail(r, "TLS: %s", e && *e ? e : "falha na negociação");
            }
            if (h == 0) {
                if (now > p->deadline) return fail(r, "tempo esgotado na negociação TLS com %s", p->host);
                return 0;
            }
            p->phase = P_SEND;
            touch(p);
            break;
        }
        case P_SEND: {
            if (p->out_off < p->out_len) {
                int n = xp_send(p, p->out + p->out_off, (int)(p->out_len - p->out_off));
                if (n < 0) return fail(r, "falha ao enviar a requisição");
                if (n > 0) { p->out_off += (u32)n; touch(p); }
                else if (now > p->deadline) return fail(r, "tempo esgotado ao enviar a requisição");
            }
            if (p->out_off < p->out_len) return 0;
            kfree(p->out); p->out = NULL;
            p->phase = P_HEAD;
            p->hdr = kmalloc(HTTP_HDR_MAX + 1);
            if (!p->hdr) return fail(r, "sem memória");
            p->hdr_len = 0;
            touch(p);
            break;
        }
        case P_HEAD: {
            /* procura o fim do cabeçalho no que já foi recebido (inclusive após 100 Continue) */
            for (;;) {
                u32 end = 0;
                for (u32 i = 3; i < p->hdr_len; i++)
                    if (p->hdr[i] == '\n' && p->hdr[i - 1] == '\r' && p->hdr[i - 2] == '\n' && p->hdr[i - 3] == '\r') { end = i + 1; break; }
                if (!end)
                    for (u32 i = 1; i < p->hdr_len; i++) if (p->hdr[i] == '\n' && p->hdr[i - 1] == '\n') { end = i + 1; break; }
                if (!end) break;
                int rr = on_header(r, end);
                if (rr < 0) return -1;
                if (rr > 0 || p->phase != P_HEAD) break;
            }
            if (r->state || p->phase != P_HEAD) break;
            if (p->hdr_len >= HTTP_HDR_MAX) return fail(r, "cabeçalho HTTP grande demais");
            int n = xp_recv(p, p->hdr + p->hdr_len, (int)(HTTP_HDR_MAX - p->hdr_len));
            if (n < 0) return fail(r, p->hdr_len ? "resposta HTTP incompleta" : "conexão encerrada antes da resposta");
            if (n == 0) {
                if (now > p->deadline) return fail(r, "tempo esgotado aguardando resposta de %s", p->host);
                return 0;
            }
            p->hdr_len += (u32)n;
            touch(p);
            break;
        }
        case P_BODY: {
            u32 got = 0;
            while (r->state == 0 && got < HTTP_READ_PER_POLL) {
                int n = xp_recv(p, buf, sizeof buf);
                if (n < 0) {
                    if (p->chunked) return fail(r, "conexão encerrada prematuramente (%u bytes recebidos)", r->body_len);
                    if (p->content_length >= 0 && (i64)r->body_len < p->content_length)
                        return fail(r, "conexão encerrada prematuramente (%u de %llu bytes)", r->body_len, (u64)p->content_length);
                    finish(r);
                    break;
                }
                if (n == 0) {
                    if (now > p->deadline) return fail(r, "tempo esgotado recebendo dados de %s", p->host);
                    return 0;
                }
                got += (u32)n;
                touch(p);
                if (!body_feed(r, buf, (u32)n)) return -1;
            }
            if (r->state == 0) return 0;
            break;
        }
        default:
            break;
        }
    }
    return r->state;
}
