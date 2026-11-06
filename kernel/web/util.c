/* mOS — Horizonte: utilidades do motor (arena, buffers de texto, URLs, conjuntos de caracteres) */
#include "web.h"

/* ============================================================ arena */
void *arena_alloc(arena_t *a, usize n) {
    n = (n + 7) & ~(usize)7;
    achunk_t *c = a->head;
    if (!c || c->used + n > c->cap) {
        usize cap = MAX((usize)128 * 1024, n);
        c = kmalloc(sizeof(achunk_t) + cap);
        if (!c) return NULL;
        c->used = 0;
        c->cap = cap;
        c->next = a->head;
        a->head = c;
    }
    void *p = (u8 *)(c + 1) + c->used;
    c->used += n;
    a->total += n;
    memset(p, 0, n);
    return p;
}

char *arena_strndup(arena_t *a, const char *s, int n) {
    if (n < 0) n = (int)strlen(s);
    char *p = arena_alloc(a, (usize)n + 1);
    if (!p) return NULL;
    memcpy(p, s, n);
    p[n] = 0;
    return p;
}

void arena_free(arena_t *a) {
    achunk_t *c = a->head;
    while (c) { achunk_t *nx = c->next; kfree(c); c = nx; }
    a->head = NULL;
    a->total = 0;
}

/* ============================================================ sbuf */
static void sb_grow(sbuf_t *b, int need) {
    if (b->len + need + 1 <= b->cap) return;
    int nc = MAX(b->cap * 2, b->len + need + 64);
    char *np = kmalloc((usize)nc);
    if (!np) return;
    if (b->p) { memcpy(np, b->p, (usize)b->len); kfree(b->p); }
    b->p = np;
    b->cap = nc;
}
void sb_putc(sbuf_t *b, char c) { sb_grow(b, 1); if (b->len + 1 < b->cap) { b->p[b->len++] = c; b->p[b->len] = 0; } }
void sb_putn(sbuf_t *b, const char *s, int n) {
    if (n <= 0) return;
    sb_grow(b, n);
    if (b->len + n < b->cap) { memcpy(b->p + b->len, s, (usize)n); b->len += n; b->p[b->len] = 0; }
}
void sb_puts(sbuf_t *b, const char *s) { sb_putn(b, s, (int)strlen(s)); }
void sb_printf(sbuf_t *b, const char *fmt, ...) {
    /* formatos longos (folhas de estilo das páginas internas): buffer proporcional */
    usize cap = strlen(fmt) * 2 + 4096;
    char *tmp = kmalloc(cap);
    if (!tmp) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, cap, fmt, ap);
    va_end(ap);
    sb_puts(b, tmp);
    kfree(tmp);
}
void sb_html(sbuf_t *b, const char *s) {
    for (; *s; s++) {
        if (*s == '&') sb_puts(b, "&amp;");
        else if (*s == '<') sb_puts(b, "&lt;");
        else if (*s == '>') sb_puts(b, "&gt;");
        else if (*s == '"') sb_puts(b, "&quot;");
        else sb_putc(b, *s);
    }
}
void sb_urlenc(sbuf_t *b, const char *s) {
    static const char hx[] = "0123456789ABCDEF";
    for (; *s; s++) {
        u8 c = (u8)*s;
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '*') sb_putc(b, (char)c);
        else if (c == ' ') sb_putc(b, '+');
        else { sb_putc(b, '%'); sb_putc(b, hx[c >> 4]); sb_putc(b, hx[c & 15]); }
    }
}
void sb_free(sbuf_t *b) { kfree(b->p); b->p = NULL; b->len = b->cap = 0; }

/* ============================================================ URLs */
bool url_scheme(const char *u, char *out, int cap) {
    int i = 0;
    if (!isalpha(u[0])) return false;
    while (u[i] && (isalnum(u[i]) || u[i] == '+' || u[i] == '-' || u[i] == '.')) i++;
    if (u[i] != ':' || i >= cap) return false;
    for (int k = 0; k < i; k++) out[k] = (char)tolower(u[k]);
    out[i] = 0;
    return true;
}

bool url_host(const char *u, char *out, int cap) {
    out[0] = 0;
    const char *p = strstr(u, "://");
    if (!p) return false;
    p += 3;
    const char *at = NULL;
    for (const char *q = p; *q && *q != '/' && *q != '?' && *q != '#'; q++) if (*q == '@') at = q;
    if (at) p = at + 1;
    int n = 0;
    while (*p && *p != '/' && *p != '?' && *p != '#' && *p != ':' && n < cap - 1) out[n++] = (char)tolower(*p++);
    out[n] = 0;
    return n > 0;
}

const char *url_fragment(const char *u) { const char *h = strchr(u, '#'); return h ? h + 1 : NULL; }

bool url_same_doc(const char *a, const char *b) {
    int la = 0, lb = 0;
    while (a[la] && a[la] != '#') la++;
    while (b[lb] && b[lb] != '#') lb++;
    return la == lb && !strncmp(a, b, (usize)la);
}

static int hexv(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    c = tolower(c);
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

void url_decode_path(const char *in, char *out, int cap) {
    int n = 0;
    while (*in && *in != '?' && *in != '#' && n < cap - 1) {
        if (*in == '%' && hexv(in[1]) >= 0 && hexv(in[2]) >= 0) { out[n++] = (char)(hexv(in[1]) * 16 + hexv(in[2])); in += 3; }
        else out[n++] = *in++;
    }
    out[n] = 0;
}

/* remove segmentos "." e ".." do caminho (sem query/fragmento) */
static void normalize_path(char *path) {
    char tmp[1024];
    char *segs[128];
    int ns = 0;
    strncpy(tmp, path, sizeof tmp - 1);
    tmp[sizeof tmp - 1] = 0;
    bool trailing = false;
    char *p = tmp;
    if (*p == '/') p++;
    while (*p && ns < 128) {
        char *s = p;
        while (*p && *p != '/') p++;
        bool more = *p == '/';
        if (*p) *p++ = 0;
        trailing = more && !*p;
        if (!strcmp(s, ".")) { trailing = true; continue; }
        if (!strcmp(s, "..")) { if (ns) ns--; trailing = true; continue; }
        segs[ns++] = s;
        if (!more) trailing = false;
    }
    int n = 0;
    path[n++] = '/';
    for (int i = 0; i < ns; i++) {
        int l = (int)strlen(segs[i]);
        if (n + l + 2 >= 1024) break;
        memcpy(path + n, segs[i], (usize)l);
        n += l;
        if (i < ns - 1 || trailing) path[n++] = '/';
    }
    path[n] = 0;
}

void url_resolve(const char *base, const char *rel_in, char *out, int cap) {
    char rel[1024];
    int rn = 0;
    while (*rel_in && isspace(*rel_in)) rel_in++;
    for (const char *p = rel_in; *p && rn < (int)sizeof rel - 1; p++) if (*p != '\n' && *p != '\r' && *p != '\t') rel[rn++] = *p;
    while (rn && rel[rn - 1] == ' ') rn--;
    rel[rn] = 0;
    char sch[16];
    if (url_scheme(rel, sch, sizeof sch)) { strncpy(out, rel, cap - 1); out[cap - 1] = 0; return; }
    if (!base || !url_scheme(base, sch, sizeof sch) || !strstr(base, "://")) { strncpy(out, rel, cap - 1); out[cap - 1] = 0; return; }
    /* divide base em scheme://autoridade + caminho + query */
    const char *auth = strstr(base, "://") + 3;
    const char *pe = auth;
    while (*pe && *pe != '/' && *pe != '?' && *pe != '#') pe++;
    int prefix_len = (int)(pe - base);
    char path[1024];
    int pn = 0;
    const char *q = pe;
    while (*q && *q != '?' && *q != '#' && pn < 1000) path[pn++] = *q++;
    path[pn] = 0;
    if (!pn) { path[0] = '/'; path[1] = 0; pn = 1; }
    char res[2048];
    if (rel[0] == '/' && rel[1] == '/') {
        snprintf(res, sizeof res, "%s:%s", sch, rel);
    } else if (rel[0] == '#') {
        int bl = 0;
        while (base[bl] && base[bl] != '#') bl++;
        snprintf(res, sizeof res, "%.*s%s", bl, base, rel);
    } else if (rel[0] == '?') {
        snprintf(res, sizeof res, "%.*s%s%s", prefix_len, base, path, rel);
    } else if (!rel[0]) {
        int bl = 0;
        while (base[bl] && base[bl] != '#') bl++;
        snprintf(res, sizeof res, "%.*s", bl, base);
    } else {
        char np[1024];
        const char *rq = rel;
        while (*rq && *rq != '?' && *rq != '#') rq++;
        if (rel[0] == '/') snprintf(np, sizeof np, "%.*s", (int)(rq - rel), rel);
        else {
            char *slash = strrchr(path, '/');
            int dl = slash ? (int)(slash - path) + 1 : 1;
            snprintf(np, sizeof np, "%.*s%.*s", dl, path, (int)(rq - rel), rel);
        }
        normalize_path(np);
        snprintf(res, sizeof res, "%.*s%s%s", prefix_len, base, np, rq);
    }
    strncpy(out, res, cap - 1);
    out[cap - 1] = 0;
}

/* ============================================================ conjuntos de caracteres */
static const u16 cp1252[32] = {
    0x20AC, 0x81, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x8D, 0x017D, 0x8F,
    0x90, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x9D, 0x017E, 0x0178};

bool utf8_valid(const u8 *d, usize n) {
    usize i = 0;
    while (i < n) {
        u8 c = d[i];
        if (c < 0x80) { i++; continue; }
        int k = c >= 0xF0 && c < 0xF5 ? 3 : c >= 0xE0 ? 2 : c >= 0xC2 && c < 0xE0 ? 1 : -1;
        if (c >= 0xF5) k = -1;
        if (k < 0) return false;
        if (i + k >= n) return true; /* truncado no fim: aceita */
        for (int j = 1; j <= k; j++) if ((d[i + j] & 0xC0) != 0x80) return false;
        i += k + 1;
    }
    return true;
}

char *charset_to_utf8(const u8 *d, usize n, const char *hint, usize *outlen) {
    bool latin = false;
    if (hint && hint[0]) {
        char h[32];
        int i = 0;
        for (; hint[i] && i < 31; i++) h[i] = (char)tolower(hint[i]);
        h[i] = 0;
        if (strstr(h, "8859") || strstr(h, "1252") || strstr(h, "latin") || strstr(h, "ascii")) latin = true;
        if (strstr(h, "utf")) latin = false;
    }
    /* BOM UTF-8 */
    if (n >= 3 && d[0] == 0xEF && d[1] == 0xBB && d[2] == 0xBF) { d += 3; n -= 3; latin = false; }
    else if (!latin && !utf8_valid(d, n)) latin = true;
    if (!latin) {
        char *o = kmalloc(n + 1);
        if (!o) return NULL;
        memcpy(o, d, n);
        o[n] = 0;
        *outlen = n;
        return o;
    }
    char *o = kmalloc(n * 3 + 1);
    if (!o) return NULL;
    usize k = 0;
    for (usize i = 0; i < n; i++) {
        u32 c = d[i];
        if (c >= 0x80 && c < 0xA0) c = cp1252[c - 0x80];
        k += (usize)utf8_encode(c, o + k);
    }
    o[k] = 0;
    *outlen = k;
    return o;
}
