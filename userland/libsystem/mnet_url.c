/* mOS — mnet: partes de URL e resolução de referência (RFC 3986 §5.2, com remove_dot_segments). */
#include "mnet.h"

int mnet_default_port(const char *s) {
    if (mnet_ieq(s, "http") || mnet_ieq(s, "ws")) return 80;
    if (mnet_ieq(s, "https") || mnet_ieq(s, "wss")) return 443;
    if (mnet_ieq(s, "ftp")) return 21;
    if (mnet_ieq(s, "socks5") || mnet_ieq(s, "socks5h") || mnet_ieq(s, "socks")) return 1080;
    return 0;
}
void mnet_url_free(mnet_url_t *u) { free(u->target); free(u->userinfo); u->target = u->userinfo = 0; }

int mnet_url_parse(const char *url, mnet_url_t *u) {
    memset(u, 0, sizeof *u);
    if (!url) return -1;
    while (*url == ' ' || *url == '\t') url++;
    const char *p = url;
    if (!((*p | 32) >= 'a' && (*p | 32) <= 'z')) return -1;
    while (*p && *p != ':') {
        int c = *p;
        if (!((c | 32) >= 'a' && (c | 32) <= 'z') && !(c >= '0' && c <= '9') && c != '+' && c != '-' && c != '.') return -1;
        p++;
    }
    if (*p != ':' || (size_t)(p - url) >= sizeof u->scheme) return -1;
    for (size_t i = 0; i < (size_t)(p - url); i++) u->scheme[i] = (char)mnet_lc((unsigned char)url[i]);
    p++;
    if (p[0] != '/' || p[1] != '/') return -1;          /* só URLs hierárquicas com autoridade */
    p += 2;
    const char *ae = p;
    while (*ae && *ae != '/' && *ae != '?' && *ae != '#') ae++;
    const char *at = 0;
    for (const char *q = p; q < ae; q++) if (*q == '@') at = q;
    if (at) { u->userinfo = mnet_strndup(p, (size_t)(at - p)); p = at + 1; }
    const char *hs = p, *he, *pe = 0;
    if (*p == '[') {
        const char *rb = p;
        while (rb < ae && *rb != ']') rb++;
        if (rb == ae) goto bad;
        hs = p + 1; he = rb;
        u->ipv6 = 1;
        p = rb + 1;
        if (p < ae && *p == ':') pe = p + 1;
        else if (p != ae) goto bad;
    } else {
        he = p;
        while (he < ae && *he != ':') he++;
        if (he < ae) pe = he + 1;
    }
    if (he - hs <= 0 || (size_t)(he - hs) >= sizeof u->host) goto bad;
    for (const char *q = hs; q < he; q++) {
        unsigned char c = (unsigned char)*q;
        if (c <= ' ' || c == '<' || c == '>' || c == '\\' || c == '^' || c == '|') goto bad;
        u->host[q - hs] = (char)mnet_lc(c);
    }
    size_t hl = strlen(u->host);
    if (hl && u->host[hl - 1] == '.' && hl > 1) u->host[hl - 1] = 0;   /* "exemplo.com." = "exemplo.com" */
    if (pe && pe < ae) {
        long v = 0;
        for (const char *q = pe; q < ae; q++) {
            if (*q < '0' || *q > '9') goto bad;
            v = v * 10 + (*q - '0');
            if (v > 65535) goto bad;
        }
        if (!v) goto bad;
        u->port = (int)v;
        u->port_explicit = 1;
    } else u->port = mnet_default_port(u->scheme);
    const char *t = ae, *te = t;
    while (*te && *te != '#') te++;
    if (t == te || *t != '/') {
        size_t n = (size_t)(te - t);
        u->target = malloc(n + 2);
        if (!u->target) goto bad;
        u->target[0] = '/';
        memcpy(u->target + 1, t, n);
        u->target[n + 1] = 0;
    } else u->target = mnet_strndup(t, (size_t)(te - t));
    if (!u->target) goto bad;
    /* espaço e controle no caminho: codifica (o NSURL do app às vezes manda assim) */
    {
        mnet_buf_t b = {0};
        static const char hx[] = "0123456789ABCDEF";
        int need = 0;
        for (const char *q = u->target; *q; q++) if ((unsigned char)*q <= ' ' || (unsigned char)*q >= 0x7f) need = 1;
        if (need) {
            for (const char *q = u->target; *q; q++) {
                unsigned char c = (unsigned char)*q;
                if (c <= ' ' || c >= 0x7f) { mnet_buf_putc(&b, '%'); mnet_buf_putc(&b, hx[c >> 4]); mnet_buf_putc(&b, hx[c & 15]); }
                else mnet_buf_putc(&b, c);
            }
            free(u->target);
            u->target = (char *)b.p;
        }
    }
    return 0;
bad:
    mnet_url_free(u);
    return -1;
}

char *mnet_url_authority(const mnet_url_t *u) {
    mnet_buf_t b = {0};
    if (u->ipv6) mnet_buf_putc(&b, '[');
    mnet_buf_puts(&b, u->host);
    if (u->ipv6) mnet_buf_putc(&b, ']');
    if (u->port && u->port != mnet_default_port(u->scheme)) { mnet_buf_putc(&b, ':'); mnet_buf_putu(&b, (unsigned)u->port); }
    return (char *)b.p;
}

/* ---- RFC 3986 §5.2 */
typedef struct { const char *s, *a, *p, *q, *f; size_t sl, al, pl, ql, fl; int hs, ha, hq, hf; } parts_t;
static void split(const char *r, parts_t *x) {
    memset(x, 0, sizeof *x);
    const char *p = r;
    const char *c = r;
    while (*c && *c != ':' && *c != '/' && *c != '?' && *c != '#') c++;
    if (*c == ':' && c > r) { x->s = r; x->sl = (size_t)(c - r); x->hs = 1; p = c + 1; }
    if (p[0] == '/' && p[1] == '/') {
        p += 2;
        x->a = p;
        while (*p && *p != '/' && *p != '?' && *p != '#') p++;
        x->al = (size_t)(p - x->a); x->ha = 1;
    }
    x->p = p;
    while (*p && *p != '?' && *p != '#') p++;
    x->pl = (size_t)(p - x->p);
    if (*p == '?') { x->q = ++p; while (*p && *p != '#') p++; x->ql = (size_t)(p - x->q); x->hq = 1; }
    if (*p == '#') { x->f = ++p; x->fl = strlen(p); x->hf = 1; }
}
static void remove_dots(mnet_buf_t *out, const char *in, size_t n) {
    /* §5.2.4, sobre uma cópia */
    char *b = mnet_strndup(in, n);
    if (!b) return;
    char *i = b;
    out->n = 0;
    while (*i) {
        if (!strncmp(i, "../", 3)) i += 3;
        else if (!strncmp(i, "./", 2)) i += 2;
        else if (!strncmp(i, "/./", 3)) i += 2;
        else if (!strcmp(i, "/.")) { i[1] = 0; }
        else if (!strncmp(i, "/../", 4) || !strcmp(i, "/..")) {
            if (i[3] == '/') i += 3; else { i += 2; *i = '/'; }
            while (out->n && out->p[out->n - 1] != '/') out->n--;
            if (out->n) out->n--;
        } else if (!strcmp(i, ".") || !strcmp(i, "..")) break;
        else {
            size_t k = 0;
            if (i[0] == '/') k = 1;
            while (i[k] && i[k] != '/') k++;
            mnet_buf_put(out, i, k);
            i += k;
        }
    }
    if (out->p) out->p[out->n] = 0;
    free(b);
}
char *mnet_url_resolve(const char *base, const char *ref) {
    if (!ref) return 0;
    parts_t B, R;
    split(base, &B);
    split(ref, &R);
    if (!B.hs) return 0;
    mnet_buf_t o = {0}, path = {0}, tmp = {0};
    const char *s = R.hs ? R.s : B.s; size_t sl = R.hs ? R.sl : B.sl;
    int ha; const char *a; size_t al; const char *q = 0; size_t ql = 0; int hq = 0;
    if (R.hs) { ha = R.ha; a = R.a; al = R.al; remove_dots(&path, R.p, R.pl); hq = R.hq; q = R.q; ql = R.ql; }
    else if (R.ha) { ha = 1; a = R.a; al = R.al; remove_dots(&path, R.p, R.pl); hq = R.hq; q = R.q; ql = R.ql; }
    else {
        ha = B.ha; a = B.a; al = B.al;
        if (!R.pl) {
            mnet_buf_put(&path, B.p, B.pl);
            if (R.hq) { hq = 1; q = R.q; ql = R.ql; } else { hq = B.hq; q = B.q; ql = B.ql; }
        } else {
            if (R.p[0] == '/') remove_dots(&path, R.p, R.pl);
            else {
                if (B.ha && !B.pl) mnet_buf_putc(&tmp, '/');
                else {
                    size_t k = B.pl;
                    while (k && B.p[k - 1] != '/') k--;
                    mnet_buf_put(&tmp, B.p, k);
                }
                mnet_buf_put(&tmp, R.p, R.pl);
                remove_dots(&path, tmp.p ? (char *)tmp.p : "", tmp.n);
            }
            hq = R.hq; q = R.q; ql = R.ql;
        }
    }
    for (size_t i = 0; i < sl; i++) mnet_buf_putc(&o, mnet_lc((unsigned char)s[i]));
    mnet_buf_putc(&o, ':');
    if (ha) { mnet_buf_put(&o, "//", 2); mnet_buf_put(&o, a, al); }
    mnet_buf_put(&o, path.p, path.n);
    if (hq) { mnet_buf_putc(&o, '?'); mnet_buf_put(&o, q, ql); }
    if (R.hf) { mnet_buf_putc(&o, '#'); mnet_buf_put(&o, R.f, R.fl); }
    mnet_buf_free(&path); mnet_buf_free(&tmp);
    return (char *)o.p;
}
