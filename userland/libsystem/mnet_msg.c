/*
 * mOS — mnet: mensagem HTTP/1.x solta, a base do CFHTTPMessage (docs/rede-apps.md, item 9b).
 *
 * Pedido ou resposta com linha inicial, cabeçalhos em ordem (nome sem caixa) e corpo. Montada pelo app
 * (CFHTTPMessageCreateRequest/Response) ou lida aos pedaços (CFHTTPMessageAppendBytes: linha inicial, cabeçalhos com
 * dobra obs-fold, fim em CRLF CRLF ou LF LF; o resto vai para o corpo como chegou, sem tirar o chunked, como no
 * CFNetwork). Serializa de volta (CFHTTPMessageCopySerializedMessage) com o alvo na forma de origem.
 */
#include "mnet.h"

#define HEAD_MAX (256u * 1024u)

static char *dup0(const char *s) { return s ? mnet_strndup(s, strlen(s)) : 0; }
static int set_str(char **dst, const char *s) {
    char *n = dup0(s ? s : "");
    if (!n) return -1;
    free(*dst);
    *dst = n;
    return 0;
}
int mnet_msg_parse_version(const char *v, int *maj, int *min) {
    /* "HTTP/1.1", "HTTP/1.0", "HTTP/2.0", "HTTP/2", "HTTP/3.0" */
    if (!v || strncmp(v, "HTTP/", 5) || v[5] < '0' || v[5] > '9') return -1;
    *maj = v[5] - '0';
    *min = 0;
    if (v[6] == '.' && v[7] >= '0' && v[7] <= '9' && !v[8]) *min = v[7] - '0';
    else if (v[6]) return -1;
    return 0;
}
int mnet_msg_init_request(mnet_msg_t *m, const char *method, const char *url, const char *version) {
    memset(m, 0, sizeof *m);
    m->is_request = 1;
    if (mnet_msg_parse_version(version ? version : "HTTP/1.1", &m->ver_major, &m->ver_minor)) { m->ver_major = 1; m->ver_minor = 1; }
    m->head_done = 1;
    return set_str(&m->method, method && *method ? method : "GET") || set_str(&m->target, url ? url : "") ? -1 : 0;
}
int mnet_msg_init_response(mnet_msg_t *m, int status, const char *reason, const char *version) {
    memset(m, 0, sizeof *m);
    m->status = status;
    if (mnet_msg_parse_version(version ? version : "HTTP/1.1", &m->ver_major, &m->ver_minor)) { m->ver_major = 1; m->ver_minor = 1; }
    m->head_done = 1;
    return reason ? set_str(&m->reason, reason) : 0;
}
void mnet_msg_init_empty(mnet_msg_t *m, int is_request) {
    memset(m, 0, sizeof *m);
    m->is_request = is_request;
    m->ver_major = 1; m->ver_minor = 1;
}
void mnet_msg_free(mnet_msg_t *m) {
    free(m->method); free(m->target); free(m->reason);
    mnet_hdrs_free(&m->hdrs);
    mnet_buf_free(&m->body);
    mnet_buf_free(&m->in);
    memset(m, 0, sizeof *m);
}
int mnet_msg_copy(mnet_msg_t *d, const mnet_msg_t *s) {
    memset(d, 0, sizeof *d);
    d->is_request = s->is_request; d->ver_major = s->ver_major; d->ver_minor = s->ver_minor;
    d->status = s->status; d->head_done = s->head_done; d->bad = s->bad;
    if ((s->method && set_str(&d->method, s->method)) || (s->target && set_str(&d->target, s->target)) ||
        (s->reason && set_str(&d->reason, s->reason))) goto fail;
    for (int i = 0; i < s->hdrs.n; i++)
        if (mnet_hdrs_add(&d->hdrs, s->hdrs.v[i].name, strlen(s->hdrs.v[i].name), s->hdrs.v[i].value, strlen(s->hdrs.v[i].value))) goto fail;
    if (s->body.n && mnet_buf_put(&d->body, s->body.p, s->body.n)) goto fail;
    if (s->in.n && mnet_buf_put(&d->in, s->in.p, s->in.n)) goto fail;
    return 0;
fail:
    mnet_msg_free(d);
    return -1;
}

/* ---------------------------------------------------------------- leitura aos pedaços */
static int is_tchar(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || (c && strchr("!#$%&'*+-.^_`|~", c));
}
static size_t trim_end(const char *s, size_t n) { while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r')) n--; return n; }
static int parse_start(mnet_msg_t *m, const char *l, size_t n) {
    n = trim_end(l, n);
    if (m->is_request) {                                   /* MÉTODO SP alvo SP HTTP/x.y */
        const char *sp1 = memchr(l, ' ', n);
        if (!sp1 || sp1 == l) return -1;
        for (const char *p = l; p < sp1; p++) if (!is_tchar((unsigned char)*p)) return -1;
        const char *rest = sp1 + 1, *end = l + n;
        while (rest < end && *rest == ' ') rest++;
        const char *sp2 = end;
        while (sp2 > rest && sp2[-1] != ' ') sp2--;
        if (sp2 <= rest) return -1;
        char ver[16];
        size_t vl = (size_t)(end - sp2);
        if (vl >= sizeof ver) return -1;
        memcpy(ver, sp2, vl); ver[vl] = 0;
        if (mnet_msg_parse_version(ver, &m->ver_major, &m->ver_minor)) return -1;
        size_t tl = (size_t)(sp2 - 1 - rest);
        while (tl && rest[tl - 1] == ' ') tl--;
        if (!tl) return -1;
        free(m->method); free(m->target);
        m->method = mnet_strndup(l, (size_t)(sp1 - l));
        m->target = mnet_strndup(rest, tl);
        return m->method && m->target ? 0 : -1;
    }
    /* HTTP/x.y SP status [SP motivo] */
    const char *sp1 = memchr(l, ' ', n);
    if (!sp1) return -1;
    char ver[16];
    size_t vl = (size_t)(sp1 - l);
    if (vl >= sizeof ver) return -1;
    memcpy(ver, l, vl); ver[vl] = 0;
    if (mnet_msg_parse_version(ver, &m->ver_major, &m->ver_minor)) return -1;
    const char *p = sp1 + 1, *end = l + n;
    if (end - p < 3) return -1;
    int st = 0;
    for (int i = 0; i < 3; i++) { if (p[i] < '0' || p[i] > '9') return -1; st = st * 10 + (p[i] - '0'); }
    p += 3;
    if (p < end && *p != ' ') return -1;
    if (p < end) p++;
    m->status = st;
    free(m->reason);
    m->reason = mnet_strndup(p, (size_t)(end - p));
    return m->reason ? 0 : -1;
}
static int parse_head(mnet_msg_t *m, const char *h, size_t n) {
    size_t pos = 0;
    int first = 1;
    char *last_name = 0;
    mnet_buf_t val = {0};
    while (pos < n) {
        const char *ls = h + pos;
        const char *nl = memchr(ls, '\n', n - pos);
        size_t ll = nl ? (size_t)(nl - ls) : n - pos;
        pos += ll + (nl ? 1 : 0);
        size_t tl = trim_end(ls, ll);
        if (first) {
            if (tl == 0) continue;                         /* linhas vazias antes da inicial (RFC 9112 §2.2) */
            first = 0;
            if (parse_start(m, ls, ll)) goto bad;
            continue;
        }
        if (tl == 0) break;
        if (ls[0] == ' ' || ls[0] == '\t') {               /* obs-fold: continua o valor anterior */
            if (!last_name) goto bad;
            size_t s = 0;
            while (s < tl && (ls[s] == ' ' || ls[s] == '\t')) s++;
            if (mnet_buf_putc(&val, ' ') || mnet_buf_put(&val, ls + s, tl - s)) goto bad;
            continue;
        }
        if (last_name) {
            if (mnet_hdrs_add(&m->hdrs, last_name, strlen(last_name), val.p ? (char *)val.p : "", val.n)) goto bad;
            free(last_name); last_name = 0; val.n = 0;
        }
        const char *colon = memchr(ls, ':', tl);
        if (!colon || colon == ls) goto bad;
        for (const char *p = ls; p < colon; p++) if (!is_tchar((unsigned char)*p)) goto bad;
        last_name = mnet_strndup(ls, (size_t)(colon - ls));
        const char *v = colon + 1, *ve = ls + tl;
        while (v < ve && (*v == ' ' || *v == '\t')) v++;
        if (!last_name || mnet_buf_put(&val, v, (size_t)(ve - v))) goto bad;
    }
    if (first) goto bad;
    if (last_name && mnet_hdrs_add(&m->hdrs, last_name, strlen(last_name), val.p ? (char *)val.p : "", val.n)) goto bad;
    free(last_name);
    mnet_buf_free(&val);
    return 0;
bad:
    free(last_name);
    mnet_buf_free(&val);
    return -1;
}
/* fim da cabeça: CRLF CRLF ou LF LF (também misturados); devolve o índice logo depois, ou 0 */
static size_t head_end(const uint8_t *d, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (d[i] != '\n') continue;
        size_t j = i + 1;
        if (j < n && d[j] == '\r') j++;
        if (j < n && d[j] == '\n') {
            /* ignora as linhas vazias antes da linha inicial */
            int only_blank = 1;
            for (size_t k = 0; k < i; k++) if (d[k] != '\r' && d[k] != '\n') { only_blank = 0; break; }
            if (!only_blank) return j + 1;
        }
    }
    return 0;
}
int mnet_msg_append(mnet_msg_t *m, const uint8_t *d, size_t n) {
    if (m->bad) return -1;
    if (m->head_done) return n ? mnet_buf_put(&m->body, d, n) : 0;
    if (n && mnet_buf_put(&m->in, d, n)) return -1;
    size_t e = head_end(m->in.p, m->in.n);
    if (!e) {
        if (m->in.n > HEAD_MAX) { m->bad = 1; return -1; }
        return 0;
    }
    if (parse_head(m, (const char *)m->in.p, e)) { m->bad = 1; return -1; }
    m->head_done = 1;
    if (m->in.n > e && mnet_buf_put(&m->body, m->in.p + e, m->in.n - e)) return -1;
    mnet_buf_free(&m->in);
    return 0;
}

/* ---------------------------------------------------------------- escrita */
int mnet_msg_version(const mnet_msg_t *m, char out[12]) {
    int k = 0;
    memcpy(out, "HTTP/", 5); k = 5;
    out[k++] = (char)('0' + m->ver_major);
    out[k++] = '.';
    out[k++] = (char)('0' + m->ver_minor);
    out[k] = 0;
    return k;
}
/* alvo da linha de pedido: forma de origem para URL absoluta http(s); "*" e o alvo lido ficam como estão */
char *mnet_msg_request_target(const mnet_msg_t *m) {
    const char *t = m->target ? m->target : "";
    const char *sch = t;
    while (*sch && *sch != ':' && *sch != '/') sch++;
    if (sch[0] == ':' && sch[1] == '/' && sch[2] == '/') {
        mnet_url_t u;
        if (!mnet_url_parse(t, &u)) {
            char *r = dup0(u.target ? u.target : "/");
            mnet_url_free(&u);
            return r;
        }
    }
    return dup0(*t ? t : "/");
}
int mnet_msg_serialize(const mnet_msg_t *m, mnet_buf_t *out) {
    char ver[12];
    mnet_msg_version(m, ver);
    if (m->is_request) {
        char *t = mnet_msg_request_target(m);
        int r = !t || mnet_buf_puts(out, m->method ? m->method : "GET") || mnet_buf_putc(out, ' ') || mnet_buf_puts(out, t) ||
                mnet_buf_putc(out, ' ') || mnet_buf_puts(out, ver);
        free(t);
        if (r) return -1;
    } else {
        char st[4] = {(char)('0' + m->status / 100 % 10), (char)('0' + m->status / 10 % 10), (char)('0' + m->status % 10), 0};
        if (mnet_buf_puts(out, ver) || mnet_buf_putc(out, ' ') || mnet_buf_puts(out, st)) return -1;
        const char *why = m->reason && *m->reason ? m->reason : mnet_status_text(m->status);
        if (why && *why && (mnet_buf_putc(out, ' ') || mnet_buf_puts(out, why))) return -1;
    }
    if (mnet_buf_put(out, "\r\n", 2)) return -1;
    for (int i = 0; i < m->hdrs.n; i++)
        if (mnet_buf_puts(out, m->hdrs.v[i].name) || mnet_buf_put(out, ": ", 2) || mnet_buf_puts(out, m->hdrs.v[i].value) ||
            mnet_buf_put(out, "\r\n", 2)) return -1;
    if (mnet_buf_put(out, "\r\n", 2)) return -1;
    return m->body.n ? mnet_buf_put(out, m->body.p, m->body.n) : 0;
}
/* frase padrão do código (RFC 9110 §15), a que o CFNetwork põe quando o app não deu uma */
const char *mnet_status_text(int st) {
    switch (st) {
    case 100: return "Continue"; case 101: return "Switching Protocols"; case 102: return "Processing"; case 103: return "Early Hints";
    case 200: return "OK"; case 201: return "Created"; case 202: return "Accepted"; case 203: return "Non-Authoritative Information";
    case 204: return "No Content"; case 205: return "Reset Content"; case 206: return "Partial Content"; case 207: return "Multi-Status";
    case 300: return "Multiple Choices"; case 301: return "Moved Permanently"; case 302: return "Found"; case 303: return "See Other";
    case 304: return "Not Modified"; case 305: return "Use Proxy"; case 307: return "Temporary Redirect"; case 308: return "Permanent Redirect";
    case 400: return "Bad Request"; case 401: return "Unauthorized"; case 402: return "Payment Required"; case 403: return "Forbidden";
    case 404: return "Not Found"; case 405: return "Method Not Allowed"; case 406: return "Not Acceptable";
    case 407: return "Proxy Authentication Required"; case 408: return "Request Timeout"; case 409: return "Conflict";
    case 410: return "Gone"; case 411: return "Length Required"; case 412: return "Precondition Failed"; case 413: return "Content Too Large";
    case 414: return "URI Too Long"; case 415: return "Unsupported Media Type"; case 416: return "Range Not Satisfiable";
    case 417: return "Expectation Failed"; case 421: return "Misdirected Request"; case 422: return "Unprocessable Content";
    case 425: return "Too Early"; case 426: return "Upgrade Required"; case 428: return "Precondition Required";
    case 429: return "Too Many Requests"; case 431: return "Request Header Fields Too Large"; case 451: return "Unavailable For Legal Reasons";
    case 500: return "Internal Server Error"; case 501: return "Not Implemented"; case 502: return "Bad Gateway";
    case 503: return "Service Unavailable"; case 504: return "Gateway Timeout"; case 505: return "HTTP Version Not Supported";
    case 511: return "Network Authentication Required";
    }
    return "";
}
