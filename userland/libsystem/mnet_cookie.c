/*
 * mOS — mnet: cookies HTTP (RFC 6265, com as regras de 6265bis que os navegadores seguem: SameSite,
 * prefixos __Secure-/__Host-, cookie Secure não vem nem é sobrescrito de origem insegura).
 * O domínio é conferido contra a Public Suffix List inteira (mnet_psl.inc). O arquivo é o formato
 * Netscape que o curl lê e grava (CURLOPT_COOKIEFILE/COOKIEJAR), com o prefixo #HttpOnly_.
 */
#include "mnet.h"
#include "mnet_psl.inc"

#define JAR_MAX 3000           /* total (RFC 6265 §6.1 pede ao menos 3000) */
#define JAR_PER_DOMAIN 180     /* por domínio (ao menos 50) */
#define COOKIE_MAX 4096        /* nome + valor + atributos (ao menos 4096) */

static int psl_has(const char *s, size_t n) {
    int lo = 0, hi = PSL_N - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        const char *r = psl_blob + psl_off[mid];
        size_t rl = strlen(r);
        int c = memcmp(r, s, rl < n ? rl : n);
        if (!c) c = rl < n ? -1 : rl > n ? 1 : 0;
        if (!c) return 1;
        if (c < 0) lo = mid + 1; else hi = mid - 1;
    }
    return 0;
}
static int psl_has2(const char *pre, const char *s, size_t n) {
    char t[300];
    size_t pl = strlen(pre);
    if (pl + n >= sizeof t) return 0;
    memcpy(t, pre, pl);
    memcpy(t + pl, s, n);
    return psl_has(t, pl + n);
}
/* tamanho do sufixo público de d (em bytes, a partir do fim) */
static size_t public_suffix_len(const char *d) {
    size_t n = strlen(d);
    size_t i = 0;
    while (i < n) {
        const char *c = d + i;
        size_t cl = n - i;
        const char *dot = memchr(c, '.', cl);
        if (psl_has2("!", c, cl)) return dot ? cl - (size_t)(dot + 1 - c) : cl;
        if (psl_has(c, cl)) return cl;
        if (dot && psl_has2("*.", dot + 1, cl - (size_t)(dot + 1 - c))) return cl;
        if (!dot) return cl;                              /* regra padrão "*": o último rótulo */
        i = (size_t)(dot + 1 - d);
    }
    return 0;
}
int mnet_public_suffix(const char *domain) {
    if (!domain || !*domain) return 0;
    return public_suffix_len(domain) == strlen(domain);
}

static int is_ip(const char *h) {
    if (strchr(h, ':')) return 1;                         /* IPv6 */
    int dots = 0;
    for (const char *p = h; *p; p++) {
        if (*p == '.') dots++;
        else if (*p < '0' || *p > '9') return 0;
    }
    return dots == 3;
}
int mnet_domain_match(const char *host, const char *domain) {
    if (mnet_ieq(host, domain)) return 1;
    size_t hl = strlen(host), dl = strlen(domain);
    if (dl >= hl || is_ip(host)) return 0;
    return host[hl - dl - 1] == '.' && mnet_ieq(host + hl - dl, domain);
}
int mnet_path_match(const char *rp, const char *cp) {
    size_t cl = strlen(cp);
    if (strncmp(rp, cp, cl)) return 0;
    return rp[cl] == 0 || cp[cl - 1] == '/' || rp[cl] == '/';
}
static char *default_path(const char *p) {
    if (!p || p[0] != '/') return mnet_strndup("/", 1);
    size_t n = strlen(p);
    const char *q = memchr(p, '?', n);
    if (q) n = (size_t)(q - p);
    size_t last = 0;
    for (size_t i = 0; i < n; i++) if (p[i] == '/') last = i;
    if (!last) return mnet_strndup("/", 1);
    return mnet_strndup(p, last);
}
static char *lower_dup(const char *s, size_t n) {
    char *d = mnet_strndup(s, n);
    if (d) for (char *p = d; *p; p++) *p = (char)mnet_lc((unsigned char)*p);
    return d;
}
static int wsp(int c) { return c == ' ' || c == '\t'; }
static void trim(const char **s, size_t *n) {
    while (*n && wsp(**s)) { (*s)++; (*n)--; }
    while (*n && wsp((*s)[*n - 1])) (*n)--;
}
static void cookie_free(mnet_cookie_t *c) {
    if (!c) return;
    free(c->name); free(c->value); free(c->domain); free(c->path);
    free(c);
}
static int secure_scheme(const char *s) { return s && (mnet_ieq(s, "https") || mnet_ieq(s, "wss")); }

void mnet_jar_init(mnet_jar_t *j) { j->head = 0; j->count = 0; j->max_count = JAR_MAX; }
void mnet_jar_clear(mnet_jar_t *j) {
    mnet_cookie_t *c = j->head;
    while (c) { mnet_cookie_t *n = c->next; cookie_free(c); c = n; }
    j->head = 0; j->count = 0;
}
static void unlink_c(mnet_jar_t *j, mnet_cookie_t *prev, mnet_cookie_t *c) {
    if (prev) prev->next = c->next; else j->head = c->next;
    j->count--;
    cookie_free(c);
}
void mnet_jar_expire(mnet_jar_t *j, long long now) {
    mnet_cookie_t *prev = 0, *c = j->head;
    while (c) {
        mnet_cookie_t *n = c->next;
        if (c->persistent && c->expires <= now) unlink_c(j, prev, c); else prev = c;
        c = n;
    }
}
/* tira o menos usado (do domínio d, ou de todos) */
static void evict_one(mnet_jar_t *j, const char *d) {
    mnet_cookie_t *prev = 0, *c = j->head, *best = 0, *bprev = 0;
    for (; c; prev = c, c = c->next)
        if ((!d || mnet_ieq(c->domain, d)) && (!best || c->accessed < best->accessed)) { best = c; bprev = prev; }
    if (best) unlink_c(j, bprev, best);
}
static int same_key(const mnet_cookie_t *a, const char *name, const char *domain, const char *path) {
    return !strcmp(a->name, name) && mnet_ieq(a->domain, domain) && !strcmp(a->path, path);
}
int mnet_jar_remove(mnet_jar_t *j, const char *name, const char *domain, const char *path) {
    mnet_cookie_t *prev = 0;
    for (mnet_cookie_t *c = j->head; c; prev = c, c = c->next)
        if (same_key(c, name, domain, path)) { unlink_c(j, prev, c); return 1; }
    return 0;
}
static mnet_cookie_t *dup_cookie(const mnet_cookie_t *s) {
    mnet_cookie_t *c = calloc(1, sizeof *c);
    if (!c) return 0;
    *c = *s;
    c->next = 0;
    c->name = mnet_strndup(s->name, strlen(s->name));
    c->value = mnet_strndup(s->value, strlen(s->value));
    c->domain = lower_dup(s->domain, strlen(s->domain));
    c->path = mnet_strndup(s->path, strlen(s->path));
    if (!c->name || !c->value || !c->domain || !c->path) { cookie_free(c); return 0; }
    return c;
}
int mnet_jar_insert(mnet_jar_t *j, const mnet_cookie_t *src, long long now) {
    mnet_cookie_t *c = dup_cookie(src);
    if (!c) return 0;
    mnet_cookie_t *prev = 0;
    for (mnet_cookie_t *o = j->head; o; prev = o, o = o->next)
        if (same_key(o, c->name, c->domain, c->path)) {
            if (o->created) { c->created = o->created; c->seq = o->seq; }
            unlink_c(j, prev, o);
            break;
        }
    if (c->persistent && c->expires <= now) { cookie_free(c); return 0; }     /* era uma remoção */
    static unsigned long long seq_next;
    if (!c->created) c->created = now;
    if (!c->seq) c->seq = ++seq_next;
    if (!c->accessed) c->accessed = now;
    int per = 0;
    for (mnet_cookie_t *o = j->head; o; o = o->next) if (mnet_ieq(o->domain, c->domain)) per++;
    if (per >= JAR_PER_DOMAIN) { mnet_jar_expire(j, now); evict_one(j, c->domain); }
    if (j->count >= (j->max_count > 0 ? j->max_count : JAR_MAX)) { mnet_jar_expire(j, now); evict_one(j, 0); }
    c->next = j->head;
    j->head = c;
    j->count++;
    return 1;
}

int mnet_jar_set(mnet_jar_t *j, const char *sc, const char *scheme, const char *host, const char *rpath,
                 int http, long long now) {
    if (!sc || !host || !*host) return 0;
    size_t total = strlen(sc);
    if (total > COOKIE_MAX) return 0;
    for (size_t i = 0; i < total; i++) {
        unsigned char ch = (unsigned char)sc[i];
        if ((ch < 0x20 && ch != '\t') || ch == 0x7f) return 0;   /* CTL: recusa o cookie inteiro (6265bis) */
    }
    const char *semi = memchr(sc, ';', total);
    size_t pl = semi ? (size_t)(semi - sc) : total;
    const char *eq = memchr(sc, '=', pl);
    const char *nm = sc, *val;
    size_t nl, vl;
    if (eq) { nl = (size_t)(eq - sc); val = eq + 1; vl = pl - nl - 1; }
    else { nl = 0; val = sc; vl = pl; }                             /* sem '=': nome vazio (6265bis) */
    trim(&nm, &nl); trim(&val, &vl);
    if (!nl && !vl) return 0;
    if (!nl && vl >= 9 && (mnet_ieqn(val, "__secure-", 9) || (vl >= 7 && mnet_ieqn(val, "__host-", 7)))) return 0;

    mnet_cookie_t c;
    memset(&c, 0, sizeof c);
    c.expires = -1;
    long long max_age = 0; int have_max_age = 0;
    long long exp = -1; int have_exp = 0;
    char *domain_attr = 0, *path_attr = 0;
    const char *p = semi;
    while (p && *p == ';') {
        p++;
        const char *s = p;
        while (*p && *p != ';') p++;
        size_t al = (size_t)(p - s);
        const char *ae = memchr(s, '=', al);
        const char *an = s, *av = "";
        size_t anl = ae ? (size_t)(ae - s) : al, avl = 0;
        if (ae) { av = ae + 1; avl = al - anl - 1; }
        trim(&an, &anl); trim(&av, &avl);
        if (avl > 1024) continue;                                    /* atributo grande demais: ignora (6265bis) */
        if (anl == 7 && mnet_ieqn(an, "expires", 7)) {
            long long t = mnet_http_date(av, avl);
            if (t >= 0) { exp = t; have_exp = 1; }
        } else if (anl == 7 && mnet_ieqn(an, "max-age", 7)) {
            if (!avl) continue;
            size_t k = 0; int neg = 0;
            if (av[0] == '-') { neg = 1; k = 1; }
            if (k == avl) continue;
            long long v = 0; int ok = 1;
            for (; k < avl; k++) {
                if (av[k] < '0' || av[k] > '9') { ok = 0; break; }
                if (v < 400LL * 86400 * 1000) v = v * 10 + (av[k] - '0');
            }
            if (!ok) continue;
            max_age = neg ? -v : v;
            have_max_age = 1;
        } else if (anl == 6 && mnet_ieqn(an, "domain", 6)) {
            if (!avl) continue;
            if (av[0] == '.') { av++; avl--; }
            free(domain_attr);
            domain_attr = lower_dup(av, avl);
        } else if (anl == 4 && mnet_ieqn(an, "path", 4)) {
            free(path_attr);
            path_attr = avl && av[0] == '/' ? mnet_strndup(av, avl) : 0;
        } else if (anl == 6 && mnet_ieqn(an, "secure", 6)) {
            c.secure = 1;
        } else if (anl == 8 && mnet_ieqn(an, "httponly", 8)) {
            c.http_only = 1;
        } else if (anl == 8 && mnet_ieqn(an, "samesite", 8)) {
            if (avl == 6 && mnet_ieqn(av, "strict", 6)) c.same_site = MNET_SAMESITE_STRICT;
            else if (avl == 3 && mnet_ieqn(av, "lax", 3)) c.same_site = MNET_SAMESITE_LAX;
            else if (avl == 4 && mnet_ieqn(av, "none", 4)) c.same_site = MNET_SAMESITE_NONE;
        }
    }
    if (have_max_age) {
        c.persistent = 1;
        c.expires = max_age <= 0 ? 0 : now + max_age;               /* Max-Age vence Expires */
    } else if (have_exp) {
        c.persistent = 1;
        c.expires = exp;
    }
    int ok = 0;
    char *lhost = lower_dup(host, strlen(host));
    if (!lhost) goto out;
    if (lhost[0] && lhost[strlen(lhost) - 1] == '.') lhost[strlen(lhost) - 1] = 0;
    if (domain_attr && domain_attr[0] && domain_attr[strlen(domain_attr) - 1] == '.') goto out;
    if (domain_attr && !*domain_attr) { free(domain_attr); domain_attr = 0; }
    if (domain_attr && mnet_public_suffix(domain_attr)) {
        if (strcmp(domain_attr, lhost)) goto out;                    /* Domain=com: recusa */
        free(domain_attr); domain_attr = 0;                          /* o próprio host é sufixo: host-only */
    }
    if (domain_attr) {
        if (!mnet_domain_match(lhost, domain_attr)) goto out;
        c.host_only = 0;
        c.domain = domain_attr; domain_attr = 0;
    } else {
        c.host_only = 1;
        c.domain = lhost; lhost = 0;
    }
    c.path = path_attr ? path_attr : default_path(rpath);
    path_attr = 0;
    if (!c.path) goto out;
    if (c.secure && !secure_scheme(scheme) && !(scheme && mnet_ieq(scheme, "file"))) goto out;
    if (c.http_only && !http) goto out;
    if (c.same_site == MNET_SAMESITE_NONE && !c.secure) goto out;   /* SameSite=None exige Secure */
    /* prefixos (6265bis §4.1.3) */
    if (nl >= 9 && mnet_ieqn(nm, "__secure-", 9) && !c.secure) goto out;
    if (nl >= 7 && mnet_ieqn(nm, "__host-", 7) && (!c.secure || !c.host_only || strcmp(c.path, "/"))) goto out;
    c.name = mnet_strndup(nm, nl);
    c.value = mnet_strndup(val, vl);
    if (!c.name || !c.value) goto out;
    /* origem insegura não sobrescreve cookie Secure de mesmo nome cujo domínio/caminho casam (6265bis §5.7 passo 16) */
    if (!c.secure && !secure_scheme(scheme))
        for (mnet_cookie_t *o = j->head; o; o = o->next)
            if (o->secure && !strcmp(o->name, c.name) &&
                (mnet_domain_match(c.domain, o->domain) || mnet_domain_match(o->domain, c.domain)) &&
                mnet_path_match(c.path, o->path)) goto out;
    for (mnet_cookie_t *o = j->head; o; o = o->next)
        if (same_key(o, c.name, c.domain, c.path) && o->http_only && !http) goto out;
    c.created = now; c.accessed = now;
    ok = mnet_jar_insert(j, &c, now);          /* mantém a data de criação do antigo, ou remove se já venceu */
out:
    free(lhost); free(domain_attr); free(path_attr);
    free(c.name); free(c.value); free(c.domain); free(c.path);
    return ok;
}

char *mnet_jar_header(mnet_jar_t *j, const char *scheme, const char *host, const char *path, int http, long long now) {
    if (!host) return 0;
    mnet_jar_expire(j, now);
    char *lhost = lower_dup(host, strlen(host));
    if (!lhost) return 0;
    size_t hl = strlen(lhost);
    if (hl && lhost[hl - 1] == '.') lhost[hl - 1] = 0;
    char *rp = 0;
    if (!path || path[0] != '/') rp = mnet_strndup("/", 1);
    else {
        size_t n = strlen(path);
        const char *q = memchr(path, '?', n);
        const char *f = memchr(path, '#', n);
        if (f && (!q || f < q)) q = f;
        rp = mnet_strndup(path, q ? (size_t)(q - path) : n);
    }
    mnet_cookie_t **sel = 0;
    int ns = 0, cap = 0;
    for (mnet_cookie_t *c = j->head; c; c = c->next) {
        if (c->host_only ? !mnet_ieq(lhost, c->domain) : !mnet_domain_match(lhost, c->domain)) continue;
        if (!rp || !mnet_path_match(rp, c->path)) continue;
        if (c->secure && !secure_scheme(scheme) && !(scheme && mnet_ieq(scheme, "file"))) continue;
        if (c->http_only && !http) continue;
        if (ns == cap) {
            cap = cap ? cap * 2 : 16;
            mnet_cookie_t **n2 = realloc(sel, (size_t)cap * sizeof *sel);
            if (!n2) break;
            sel = n2;
        }
        sel[ns++] = c;
    }
    /* caminho mais longo primeiro; empate: criado antes primeiro (§5.4 passo 2) — ordenação por inserção */
    for (int i = 1; i < ns; i++) {
        mnet_cookie_t *x = sel[i];
        size_t xl = strlen(x->path);
        int k = i - 1;
        while (k >= 0) {
            size_t kl = strlen(sel[k]->path);
            if (kl > xl || (kl == xl && (sel[k]->created < x->created ||
                                         (sel[k]->created == x->created && sel[k]->seq < x->seq)))) break;
            sel[k + 1] = sel[k];
            k--;
        }
        sel[k + 1] = x;
    }
    mnet_buf_t b = {0};
    for (int i = 0; i < ns; i++) {
        if (i) mnet_buf_put(&b, "; ", 2);
        if (*sel[i]->name) { mnet_buf_puts(&b, sel[i]->name); mnet_buf_putc(&b, '='); }
        mnet_buf_puts(&b, sel[i]->value);
        sel[i]->accessed = now;
    }
    free(sel); free(lhost); free(rp);
    return (char *)b.p;
}

/* ---- arquivo Netscape: domínio \t subdomínios \t caminho \t seguro \t validade \t nome \t valor */
int mnet_jar_save(const mnet_jar_t *j, mnet_buf_t *o, int include_session) {
    int r = mnet_buf_puts(o, "# Netscape HTTP Cookie File\n# Gerado pelo mOS (mnet).\n\n");
    for (const mnet_cookie_t *c = j->head; c; c = c->next) {
        if (!c->persistent && !include_session) continue;
        if (c->http_only) r |= mnet_buf_puts(o, "#HttpOnly_");
        if (!c->host_only) r |= mnet_buf_putc(o, '.');
        r |= mnet_buf_puts(o, c->domain);
        r |= mnet_buf_puts(o, c->host_only ? "\tFALSE\t" : "\tTRUE\t");
        r |= mnet_buf_puts(o, c->path);
        r |= mnet_buf_puts(o, c->secure ? "\tTRUE\t" : "\tFALSE\t");
        r |= mnet_buf_putu(o, (unsigned long long)(c->persistent && c->expires > 0 ? c->expires : 0));
        r |= mnet_buf_putc(o, '\t');
        r |= mnet_buf_puts(o, c->name);
        r |= mnet_buf_putc(o, '\t');
        r |= mnet_buf_puts(o, c->value);
        r |= mnet_buf_putc(o, '\n');
    }
    return r ? -1 : 0;
}
int mnet_jar_load(mnet_jar_t *j, const char *t, size_t n, long long now) {
    int added = 0;
    size_t i = 0;
    while (i < n) {
        size_t s = i;
        while (i < n && t[i] != '\n') i++;
        size_t e = i;
        if (i < n) i++;
        if (e > s && t[e - 1] == '\r') e--;
        const char *l = t + s;
        size_t ll = e - s;
        int http_only = 0;
        if (ll >= 10 && !memcmp(l, "#HttpOnly_", 10)) { http_only = 1; l += 10; ll -= 10; }
        else if (!ll || l[0] == '#') continue;
        const char *f[7]; size_t fl[7]; int nf = 0;
        size_t k = 0;
        while (nf < 7) {
            size_t a = k;
            while (k < ll && l[k] != '\t') k++;
            f[nf] = l + a; fl[nf] = k - a; nf++;
            if (k >= ll) break;
            k++;
        }
        if (nf == 6) { f[6] = ""; fl[6] = 0; nf = 7; }     /* valor vazio no fim da linha */
        if (nf != 7) continue;
        mnet_cookie_t c;
        memset(&c, 0, sizeof c);
        const char *dm = f[0]; size_t dl = fl[0];
        if (dl && dm[0] == '.') { dm++; dl--; }
        c.host_only = !(fl[1] == 4 && mnet_ieqn(f[1], "TRUE", 4));
        c.secure = fl[3] == 4 && mnet_ieqn(f[3], "TRUE", 4);
        c.http_only = http_only;
        long long ex = 0;
        for (size_t q = 0; q < fl[4] && f[4][q] >= '0' && f[4][q] <= '9'; q++) ex = ex * 10 + (f[4][q] - '0');
        c.persistent = ex != 0;
        c.expires = ex ? ex : -1;
        c.domain = lower_dup(dm, dl);
        c.path = mnet_strndup(f[2], fl[2]);
        c.name = mnet_strndup(f[5], fl[5]);
        c.value = mnet_strndup(f[6], fl[6]);
        if (c.domain && c.path && c.name && c.value && *c.domain) added += mnet_jar_insert(j, &c, now);
        free(c.domain); free(c.path); free(c.name); free(c.value);
    }
    return added;
}
