/*
 * mOS — mnet: escolha de proxy como o CFNetwork faz (CFNetworkCopyProxiesForURL): exceções (curinga, domínio,
 * rede IPv4 em CIDR, nomes simples, loopback sempre direto), depois PAC, depois HTTP/HTTPS pelo esquema, depois
 * SOCKS, e por fim conexão direta. O resultado de um PAC ("PROXY h:p; SOCKS h:p; DIRECT") também é lido aqui.
 */
#include "mnet.h"

void mnet_proxy_cfg_free(mnet_proxy_cfg_t *c) {
    free(c->pac_url); free(c->pac_js);
    for (int i = 0; i < c->nexc; i++) free(c->exceptions[i]);
    free(c->exceptions);
    memset(c, 0, sizeof *c);
}
int mnet_proxy_add_exception(mnet_proxy_cfg_t *c, const char *p) {
    while (*p == ' ') p++;
    size_t n = strlen(p);
    while (n && p[n - 1] == ' ') n--;
    if (!n) return 0;
    char **v = realloc(c->exceptions, (size_t)(c->nexc + 1) * sizeof *v);
    if (!v) return -1;
    c->exceptions = v;
    c->exceptions[c->nexc] = mnet_strndup(p, n);
    for (char *q = c->exceptions[c->nexc]; q && *q; q++) *q = (char)mnet_lc((unsigned char)*q);
    return c->exceptions[c->nexc++] ? 0 : -1;
}
/* curinga simples: '*' qualquer sequência, '?' um caractere */
static int glob(const char *p, const char *s) {
    if (!*p) return !*s;
    if (*p == '*') { for (;;) { if (glob(p + 1, s)) return 1; if (!*s) return 0; s++; } }
    if (!*s) return 0;
    if (*p != '?' && mnet_lc((unsigned char)*p) != mnet_lc((unsigned char)*s)) return 0;
    return glob(p + 1, s + 1);
}
static int ipv4(const char *s, uint32_t *ip, int *parts) {
    uint32_t v = 0; int n = 0;
    while (*s) {
        if (*s < '0' || *s > '9') return 0;
        unsigned o = 0; int d = 0;
        while (*s >= '0' && *s <= '9') { o = o * 10 + (unsigned)(*s++ - '0'); if (++d > 3 || o > 255) return 0; }
        v = v << 8 | o; n++;
        if (*s == '.') { s++; if (!*s) return 0; } else if (*s) return 0;
        if (n > 4) return 0;
    }
    if (!n) return 0;
    *parts = n;
    *ip = v << (8 * (4 - n));                                        /* "169.254" = 169.254.0.0 */
    return 1;
}
int mnet_proxy_bypass(const mnet_proxy_cfg_t *c, const char *host) {
    char h[256];
    size_t hl = strlen(host);
    if (hl >= sizeof h) hl = sizeof h - 1;
    for (size_t i = 0; i < hl; i++) h[i] = (char)mnet_lc((unsigned char)host[i]);
    h[hl] = 0;
    if (hl && h[hl - 1] == '.') h[--hl] = 0;
    uint32_t hip = 0; int hp = 0;
    int is_ip = ipv4(h, &hip, &hp) && hp == 4;
    if (!strcmp(h, "localhost") || !strcmp(h, "::1") || (is_ip && hip >> 24 == 127)) return 1;   /* loopback sempre direto */
    if (c->exclude_simple && !strchr(h, '.') && !strchr(h, ':')) return 1;
    for (int i = 0; i < c->nexc; i++) {
        const char *e = c->exceptions[i];
        if (!strcmp(e, "<local>")) { if (!strchr(h, '.') && !strchr(h, ':')) return 1; continue; }
        const char *sl = strchr(e, '/');
        if (sl) {                                                   /* rede: "169.254/16", "10.0.0.0/8" */
            char net[64]; size_t nl = (size_t)(sl - e) < sizeof net - 1 ? (size_t)(sl - e) : sizeof net - 1;
            memcpy(net, e, nl); net[nl] = 0;
            uint32_t nip; int np;
            int bits = 0; for (const char *q = sl + 1; *q >= '0' && *q <= '9'; q++) bits = bits * 10 + (*q - '0');
            if (is_ip && ipv4(net, &nip, &np) && bits <= 32) {
                uint32_t mask = bits ? 0xffffffffu << (32 - bits) : 0;
                if ((hip & mask) == (nip & mask)) return 1;
            }
            continue;
        }
        if (strchr(e, '*') || strchr(e, '?')) { if (glob(e, h)) return 1; continue; }
        uint32_t eip; int ep;
        if (ipv4(e, &eip, &ep)) {                                   /* "192.168.1" = prefixo de rede */
            if (is_ip) { uint32_t mask = 0xffffffffu << (8 * (4 - ep)); if (ep == 4) mask = 0xffffffffu; if ((hip & mask) == eip) return 1; }
            continue;
        }
        const char *d = e[0] == '.' ? e + 1 : e;                     /* "exemplo.com" e ".exemplo.com": domínio e subdomínios */
        if (mnet_domain_match(h, d)) return 1;
    }
    return 0;
}
static int add(mnet_proxy_t *o, int n, int max, int type, const char *host, int port) {
    if (n >= max) return n;
    o[n].type = type; o[n].port = port;
    size_t l = host ? strlen(host) : 0;
    if (l >= sizeof o[n].host) l = sizeof o[n].host - 1;
    if (l) memcpy(o[n].host, host, l);
    o[n].host[l] = 0;
    return n + 1;
}
int mnet_proxy_for_url(const mnet_proxy_cfg_t *c, const char *url, mnet_proxy_t *o, int max) {
    int n = 0;
    mnet_url_t u;
    if (mnet_url_parse(url, &u)) return add(o, 0, max, MNET_PX_NONE, 0, 0);
    if (!mnet_proxy_bypass(c, u.host)) {
        if (c->pac_on && c->pac_js) n = add(o, n, max, MNET_PX_PAC_JS, 0, 0);
        else if (c->pac_on && c->pac_url) n = add(o, n, max, MNET_PX_PAC_URL, c->pac_url, 0);
        int secure = !strcmp(u.scheme, "https") || !strcmp(u.scheme, "wss");
        int plain = !strcmp(u.scheme, "http") || !strcmp(u.scheme, "ws");
        if (secure && c->https_on && c->https_host[0]) n = add(o, n, max, MNET_PX_HTTPS, c->https_host, c->https_port ? c->https_port : 8080);
        if (plain && c->http_on && c->http_host[0]) n = add(o, n, max, MNET_PX_HTTP, c->http_host, c->http_port ? c->http_port : 8080);
        if (c->socks_on && c->socks_host[0]) n = add(o, n, max, MNET_PX_SOCKS, c->socks_host, c->socks_port ? c->socks_port : 1080);
    }
    n = add(o, n, max, MNET_PX_NONE, 0, 0);
    mnet_url_free(&u);
    return n;
}
int mnet_proxy_parse_pac_result(const char *s, mnet_proxy_t *o, int max) {
    int n = 0;
    const char *p = s ? s : "DIRECT";
    while (*p && n < max) {
        while (*p == ' ' || *p == ';' || *p == '\t') p++;
        if (!*p) break;
        const char *w = p;
        while (*p && *p != ' ' && *p != ';') p++;
        size_t wl = (size_t)(p - w);
        int type = -1;
        if (wl == 6 && mnet_ieqn(w, "DIRECT", 6)) { n = add(o, n, max, MNET_PX_NONE, 0, 0); continue; }
        if (wl == 5 && mnet_ieqn(w, "PROXY", 5)) type = MNET_PX_HTTP;
        else if (wl == 4 && mnet_ieqn(w, "HTTP", 4)) type = MNET_PX_HTTP;
        else if (wl == 5 && mnet_ieqn(w, "HTTPS", 5)) type = MNET_PX_HTTPS;
        else if ((wl == 5 && mnet_ieqn(w, "SOCKS", 5)) || (wl == 6 && (mnet_ieqn(w, "SOCKS5", 6) || mnet_ieqn(w, "SOCKS4", 6)))) type = MNET_PX_SOCKS;
        while (*p == ' ') p++;
        const char *a = p;
        while (*p && *p != ';' && *p != ' ') p++;
        if (type < 0 || p == a) { while (*p && *p != ';') p++; continue; }
        char host[256]; int port = type == MNET_PX_SOCKS ? 1080 : 80;
        const char *col = 0;
        for (const char *q = a; q < p; q++) if (*q == ':') col = q;
        if (*a == '[') { const char *rb = a; while (rb < p && *rb != ']') rb++; col = rb + 1 < p && rb[1] == ':' ? rb + 1 : 0; a++; size_t l = (size_t)(rb - a); if (l > 255) l = 255; memcpy(host, a, l); host[l] = 0; }
        else { size_t l = (size_t)((col ? col : p) - a); if (l > 255) l = 255; memcpy(host, a, l); host[l] = 0; }
        if (col) { port = 0; for (const char *q = col + 1; q < p && *q >= '0' && *q <= '9'; q++) port = port * 10 + (*q - '0'); }
        n = add(o, n, max, type, host, port);
    }
    if (!n) n = add(o, 0, max, MNET_PX_NONE, 0, 0);
    return n;
}
