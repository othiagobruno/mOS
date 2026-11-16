/* mOS — URL e URLSearchParams do motor web (WHATWG URL Standard, implementação própria): esquemas especiais e
 * opacos, credenciais, hosts (domínio com punycode, IPv4 com hexa/octal, IPv6 comprimido, host opaco), porta padrão,
 * caminhos com . e .., conjuntos de escape por componente, resolução relativa, origin, setters e
 * application/x-www-form-urlencoded. */
#include "js_int.h"
#include "web.h"

#define NATIVE(name) static jsv name(jsv thisv, int argc, jsv *argv, jsobj *self, int flags)
#define UNUSED_ARGS (void)thisv; (void)argc; (void)argv; (void)self; (void)flags

/* ============================================================ registro de URL */
typedef struct {
    char scheme[40];
    sbuf_t user, pass, host, path, query, frag;
    bool has_host, opaque, has_query, has_frag;
    int port;                      /* -1 = nenhuma */
    /* segmentos do caminho (não opaco): offsets em path, reconstruído no fim */
} url_t;

static void url_free(url_t *u) { sb_free(&u->user); sb_free(&u->pass); sb_free(&u->host); sb_free(&u->path); sb_free(&u->query); sb_free(&u->frag); }
static const char *S(const sbuf_t *b) { return b->p ? b->p : ""; }
static void sb_set(sbuf_t *b, const char *s, int n) { sb_free(b); memset(b, 0, sizeof *b); if (n > 0) sb_putn(b, s, n); else sb_putn(b, "", 0); }
static void sb_copy(sbuf_t *d, const sbuf_t *s) { sb_set(d, S(s), s->len); }

static const struct { const char *s; int port; } specials[] = { { "ftp", 21 }, { "file", -1 }, { "http", 80 }, { "https", 443 }, { "ws", 80 }, { "wss", 443 } };
static bool is_special(const char *s) { for (u32 i = 0; i < ARRAY_LEN(specials); i++) if (!strcmp(s, specials[i].s)) return true; return false; }
static int default_port(const char *s) { for (u32 i = 0; i < ARRAY_LEN(specials); i++) if (!strcmp(s, specials[i].s)) return specials[i].port; return -1; }

/* ---------------------------------------------------------------- escape */
enum { SET_C0, SET_FRAG, SET_QUERY, SET_SQUERY, SET_PATH, SET_USER, SET_FORM };
static bool in_set(u8 c, int set) {
    if (c < 0x20 || c > 0x7E) return true;
    switch (set) {
    case SET_C0: return false;
    case SET_FRAG: return c == ' ' || c == '"' || c == '<' || c == '>' || c == '`';
    case SET_QUERY: return c == ' ' || c == '"' || c == '#' || c == '<' || c == '>';
    case SET_SQUERY: return c == ' ' || c == '"' || c == '#' || c == '<' || c == '>' || c == '\'';
    case SET_PATH: return c == ' ' || c == '"' || c == '#' || c == '<' || c == '>' || c == '?' || c == '`' || c == '{' || c == '}';
    case SET_USER: return c == ' ' || c == '"' || c == '#' || c == '<' || c == '>' || c == '?' || c == '`' || c == '{' || c == '}' ||
                          c == '/' || c == ':' || c == ';' || c == '=' || c == '@' || c == '[' || c == '\\' || c == ']' || c == '^' || c == '|';
    case SET_FORM: return !(isalnum(c) || c == '*' || c == '-' || c == '.' || c == '_');
    }
    return false;
}
static void pct_put(sbuf_t *o, const char *s, int n, int set) {
    static const char hx[] = "0123456789ABCDEF";
    for (int i = 0; i < n; i++) {
        u8 c = (u8)s[i];
        if (set == SET_FORM && c == ' ') { sb_putc(o, '+'); continue; }
        if (in_set(c, set)) { char e[3] = { '%', hx[c >> 4], hx[c & 15] }; sb_putn(o, e, 3); }
        else sb_putc(o, (char)c);
    }
}
static int hexv(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }
static void pct_decode(sbuf_t *o, const char *s, int n, bool plus) {
    for (int i = 0; i < n; i++) {
        if (s[i] == '%' && i + 2 < n && hexv(s[i + 1]) >= 0 && hexv(s[i + 2]) >= 0) {
            sb_putc(o, (char)(hexv(s[i + 1]) * 16 + hexv(s[i + 2])));
            i += 2;
        } else sb_putc(o, plus && s[i] == '+' ? ' ' : s[i]);
    }
}

/* ---------------------------------------------------------------- hosts */
static bool forbidden_host(u8 c) {
    return c == 0 || c == '\t' || c == '\n' || c == '\r' || c == ' ' || c == '#' || c == '/' || c == ':' || c == '<' || c == '>' ||
           c == '?' || c == '@' || c == '[' || c == '\\' || c == ']' || c == '^' || c == '|';
}
static bool forbidden_domain(u8 c) { return forbidden_host(c) || (c >= 1 && c <= 0x1F) || c == '%' || c == 0x7F; }

/* punycode (RFC 3492) de um rótulo em UTF-8 */
static bool punycode(const char *s, int n, sbuf_t *o) {
    u32 cps[256];
    int m = 0;
    for (int i = 0; i < n && m < 256;) { u32 cp = js_utf8_get(s, n, &i); cps[m++] = cp; }
    int b = 0;
    for (int i = 0; i < m; i++) if (cps[i] < 0x80) { sb_putc(o, (char)cps[i]); b++; }
    int h = b;
    if (b) sb_putc(o, '-');
    u32 nn = 128, delta = 0, bias = 72;
    while (h < m) {
        u32 mm = 0xFFFFFFFFu;
        for (int i = 0; i < m; i++) if (cps[i] >= nn && cps[i] < mm) mm = cps[i];
        if ((u64)(mm - nn) * (h + 1) + delta > 0xFFFFFFFFu) return false;
        delta += (mm - nn) * (h + 1);
        nn = mm;
        for (int i = 0; i < m; i++) {
            if (cps[i] < nn) delta++;
            if (cps[i] == nn) {
                u32 q = delta;
                for (u32 k = 36;; k += 36) {
                    u32 t = k <= bias ? 1 : k >= bias + 26 ? 26 : k - bias;
                    if (q < t) break;
                    u32 dgt = t + (q - t) % (36 - t);
                    sb_putc(o, (char)(dgt < 26 ? 'a' + dgt : '0' + dgt - 26));
                    q = (q - t) / (36 - t);
                }
                sb_putc(o, (char)(q < 26 ? 'a' + q : '0' + q - 26));
                /* adapta o viés */
                u32 d = h == b ? delta / 700 : delta / 2;
                d += d / (h + 1);
                u32 k = 0;
                while (d > 455) { d /= 35; k += 36; }
                bias = k + 36 * d / (d + 38);
                delta = 0;
                h++;
            }
        }
        delta++;
        nn++;
    }
    return true;
}

static bool parse_ipv4_num(const char *s, int n, u64 *out) {
    if (!n) return false;
    int base = 10;
    if (n >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) { base = 16; s += 2; n -= 2; }
    else if (n >= 2 && s[0] == '0') { base = 8; s++; n--; }
    u64 v = 0;
    for (int i = 0; i < n; i++) {
        int d = hexv(s[i]);
        if (d < 0 || d >= base) return false;
        v = v * base + d;
        if (v > 0xFFFFFFFFFFull) v = 0xFFFFFFFFFFull;
    }
    *out = v;
    return true;
}

/* 1 = IPv4 válido (out), 0 = não é número no fim (domínio), -1 = falha */
static int parse_ipv4(const char *s, int n, char *out) {
    char parts[4][64];
    int np = 0, start = 0;
    if (n && s[n - 1] == '.') n--;
    /* "termina em número": o último rótulo é decimal ou 0x... */
    int ls = n;
    while (ls > 0 && s[ls - 1] != '.') ls--;
    const char *last = s + ls;
    int ll = n - ls;
    bool num = ll > 0;
    for (int i = 0; i < ll; i++) if (!isdigit((u8)last[i])) num = false;
    if (!num) {
        u64 dummy;
        if (!(ll >= 2 && last[0] == '0' && (last[1] == 'x' || last[1] == 'X') && parse_ipv4_num(last, ll, &dummy))) return 0;
    }
    for (int i = 0; i <= n; i++) {
        if (i == n || s[i] == '.') {
            if (np >= 4 || i - start >= 64) return -1;
            memcpy(parts[np], s + start, i - start);
            parts[np][i - start] = 0;
            np++;
            start = i + 1;
        }
    }
    u64 v[4] = { 0, 0, 0, 0 };
    for (int i = 0; i < np; i++) if (!parse_ipv4_num(parts[i], (int)strlen(parts[i]), &v[i])) return -1;
    for (int i = 0; i < np - 1; i++) if (v[i] > 255) return -1;
    if (v[np - 1] >= (1ull << (8 * (5 - np)))) return -1;
    u64 ip = v[np - 1];
    for (int i = 0; i < np - 1; i++) ip += v[i] << (8 * (3 - i));
    snprintf(out, 16, "%u.%u.%u.%u", (u32)(ip >> 24) & 255, (u32)(ip >> 16) & 255, (u32)(ip >> 8) & 255, (u32)ip & 255);
    return 1;
}

static bool parse_ipv6(const char *s, int n, sbuf_t *o) {
    u16 a[8] = { 0 };
    int pi = 0, comp = -1, i = 0;
    if (n && s[0] == ':') { if (n < 2 || s[1] != ':') return false; i = 2; pi = 1; comp = 1; }
    while (i < n) {
        if (pi == 8) return false;
        if (s[i] == ':') { if (comp >= 0) return false; i++; pi++; comp = pi; continue; }
        u32 v = 0;
        int len = 0;
        while (len < 4 && i < n && hexv(s[i]) >= 0) { v = v * 16 + hexv(s[i]); i++; len++; }
        if (i < n && s[i] == '.') {
            /* IPv4 embutido */
            if (!len || pi > 6) return false;
            i -= len;
            int seen = 0;
            while (i < n) {
                int val = -1;
                if (seen > 0) { if (s[i] == '.' && seen < 4) i++; else return false; }
                if (i >= n || !isdigit((u8)s[i])) return false;
                while (i < n && isdigit((u8)s[i])) {
                    int d = s[i] - '0';
                    if (val == -1) val = d; else if (val == 0) return false; else val = val * 10 + d;
                    if (val > 255) return false;
                    i++;
                }
                a[pi] = (u16)(a[pi] * 256 + val);
                seen++;
                if (seen == 2 || seen == 4) pi++;
            }
            if (seen != 4) return false;
            break;
        }
        if (i < n && s[i] == ':') { i++; if (i >= n) return false; }
        else if (i < n) return false;
        a[pi++] = (u16)v;
    }
    if (comp >= 0) {
        int sw = pi - comp;
        pi = 7;
        while (pi != 0 && sw > 0) { u16 t = a[comp + sw - 1]; a[comp + sw - 1] = a[pi]; a[pi] = t; pi--; sw--; }
    } else if (pi != 8) return false;
    /* serialização: comprime a maior sequência (≥ 2) de zeros */
    int best = -1, bl = 1;
    for (int k = 0; k < 8;) {
        if (a[k]) { k++; continue; }
        int e = k;
        while (e < 8 && !a[e]) e++;
        if (e - k > bl) { best = k; bl = e - k; }
        k = e;
    }
    sb_putc(o, '[');
    bool ign = false;
    for (int k = 0; k < 8; k++) {
        if (ign && !a[k]) continue;
        ign = false;
        if (k == best) { sb_puts(o, k == 0 ? "::" : ":"); ign = true; continue; }
        char b[8];
        snprintf(b, sizeof b, "%x", a[k]);
        sb_puts(o, b);
        if (k != 7) sb_putc(o, ':');
    }
    sb_putc(o, ']');
    return true;
}

static bool parse_host(const char *s, int n, bool special, sbuf_t *out) {
    sb_set(out, "", 0);
    out->len = 0;
    if (n && s[0] == '[') {
        if (s[n - 1] != ']') return false;
        sbuf_t t = { 0 };
        bool ok = parse_ipv6(s + 1, n - 2, &t);
        if (ok) sb_copy(out, &t);
        sb_free(&t);
        return ok;
    }
    if (!special) {
        for (int i = 0; i < n; i++) if (forbidden_host((u8)s[i])) return false;
        sbuf_t t = { 0 };
        pct_put(&t, s, n, SET_C0);
        sb_copy(out, &t);
        sb_free(&t);
        return true;
    }
    sbuf_t dec = { 0 };
    pct_decode(&dec, s, n, false);
    const char *d = S(&dec);
    int dn = dec.len;
    /* domínio para ASCII: minúsculas e punycode por rótulo */
    sbuf_t asc = { 0 };
    int ls = 0;
    for (int i = 0; i <= dn; i++) {
        if (i < dn && d[i] != '.') continue;
        bool ascii = true;
        for (int k = ls; k < i; k++) if ((u8)d[k] >= 0x80) ascii = false;
        if (ascii) for (int k = ls; k < i; k++) sb_putc(&asc, (char)tolower((u8)d[k]));
        else {
            /* minúsculas Unicode simples para o latim antes do punycode */
            sbuf_t low = { 0 };
            for (int k = ls; k < i;) {
                int pos = k - ls;
                u32 cp = js_utf8_get(d + ls, i - ls, &pos);
                k = ls + pos;
                if (cp < 0x80) cp = (u32)tolower((int)cp);
                else if ((cp >= 0xC0 && cp <= 0xDE && cp != 0xD7) || (cp >= 0x391 && cp <= 0x3AB && cp != 0x3A2) || (cp >= 0x410 && cp <= 0x42F)) cp += 0x20;   /* latim-1, grego, cirílico */
                char tmp[4];
                sb_putn(&low, tmp, js_utf8_put(tmp, cp));
            }
            sb_puts(&asc, "xn--");
            if (!punycode(S(&low), low.len, &asc)) { sb_free(&low); sb_free(&asc); sb_free(&dec); return false; }
            sb_free(&low);
        }
        if (i < dn) sb_putc(&asc, '.');
        ls = i + 1;
    }
    sb_free(&dec);
    const char *a = S(&asc);
    int an = asc.len;
    if (!an) { sb_free(&asc); return false; }
    for (int i = 0; i < an; i++) if (forbidden_domain((u8)a[i])) { sb_free(&asc); return false; }
    char v4[16];
    int r = parse_ipv4(a, an, v4);
    if (r < 0) { sb_free(&asc); return false; }
    if (r > 0) sb_set(out, v4, (int)strlen(v4)); else sb_copy(out, &asc);
    sb_free(&asc);
    return true;
}

/* ---------------------------------------------------------------- caminhos */
static bool is_dot(const char *s, int n) { return (n == 1 && s[0] == '.') || (n == 3 && s[0] == '%' && s[1] == '2' && (s[2] == 'e' || s[2] == 'E')); }
static bool is_dotdot(const char *s, int n) {
    if (n == 2) return s[0] == '.' && s[1] == '.';
    if (n == 4) return (s[0] == '.' && is_dot(s + 1, 3)) || (is_dot(s, 3) && s[3] == '.');
    if (n == 6) return is_dot(s, 3) && is_dot(s + 3, 3);
    return false;
}
static bool drive_letter(const char *s, int n, bool normalized) { return n == 2 && isalpha((u8)s[0]) && (s[1] == ':' || (!normalized && s[1] == '|')); }

/* segmentos como lista de strings em sbuf separados por \0 */
typedef struct { sbuf_t b; int n; int off[512]; } segs_t;
static void segs_from_path(segs_t *g, const char *path) {
    memset(g, 0, sizeof *g);
    if (*path == '/') path++;
    else if (!*path) return;
    const char *p = path;
    for (;;) {
        const char *e = strchr(p, '/');
        int l = e ? (int)(e - p) : (int)strlen(p);
        if (g->n < 512) { g->off[g->n++] = g->b.len; sb_putn(&g->b, p, l); sb_putc(&g->b, 0); }
        if (!e) break;
        p = e + 1;
    }
}
static void segs_pop(segs_t *g, const char *scheme) {
    if (!g->n) return;
    const char *f = S(&g->b) + g->off[0];
    if (!strcmp(scheme, "file") && g->n == 1 && drive_letter(f, (int)strlen(f), true)) return;
    g->b.len = g->off[--g->n];
}
static void segs_push(segs_t *g, const char *s, int n) {
    if (g->n >= 512) return;
    g->off[g->n++] = g->b.len;
    sb_putn(&g->b, s, n);
    sb_putc(&g->b, 0);
}
static void segs_to_path(segs_t *g, sbuf_t *out) {
    sb_set(out, "", 0);
    out->len = 0;
    for (int i = 0; i < g->n; i++) { sb_putc(out, '/'); sb_puts(out, S(&g->b) + g->off[i]); }
    sb_putn(out, "", 0);
}

/* aplica o texto de caminho s (relativo aos segmentos atuais) */
static void path_apply(segs_t *g, const char *s, int n, bool special, const char *scheme) {
    int i = 0;
    if (n && (s[0] == '/' || (special && s[0] == '\\'))) i = 1;
    for (;;) {
        int st = i;
        while (i < n && s[i] != '/' && !(special && s[i] == '\\')) i++;
        int l = i - st;
        bool end = i >= n;
        if (is_dotdot(s + st, l)) { segs_pop(g, scheme); if (end) segs_push(g, "", 0); }
        else if (is_dot(s + st, l)) { if (end) segs_push(g, "", 0); }
        else {
            sbuf_t e = { 0 };
            if (!strcmp(scheme, "file") && !g->n && drive_letter(s + st, l, false)) { sb_putc(&e, s[st]); sb_putc(&e, ':'); }
            else pct_put(&e, s + st, l, SET_PATH);
            segs_push(g, S(&e), e.len);
            sb_free(&e);
        }
        if (end) break;
        i++;
    }
}

/* ---------------------------------------------------------------- análise */
static bool parse_port(const char *s, int n, const char *scheme, int *port) {
    if (!n) { *port = -1; return true; }
    u32 v = 0;
    for (int i = 0; i < n; i++) { if (!isdigit((u8)s[i])) return false; v = v * 10 + (s[i] - '0'); if (v > 65535) return false; }
    *port = (int)v == default_port(scheme) ? -1 : (int)v;
    return true;
}

/* autoridade [user[:pass]@]host[:port] em s[0..n) */
static bool parse_authority(url_t *u, const char *s, int n, bool special) {
    int at = -1;
    for (int i = 0; i < n; i++) if (s[i] == '@') at = i;
    sb_set(&u->user, "", 0); u->user.len = 0;
    sb_set(&u->pass, "", 0); u->pass.len = 0;
    if (at >= 0) {
        int colon = -1;
        for (int i = 0; i < at; i++) if (s[i] == ':') { colon = i; break; }
        pct_put(&u->user, s, colon >= 0 ? colon : at, SET_USER);
        if (colon >= 0) pct_put(&u->pass, s + colon + 1, at - colon - 1, SET_USER);
        s += at + 1;
        n -= at + 1;
    }
    int colon = -1;
    bool br = false;
    for (int i = 0; i < n; i++) { if (s[i] == '[') br = true; else if (s[i] == ']') br = false; else if (s[i] == ':' && !br) { colon = i; break; } }
    int hn = colon >= 0 ? colon : n;
    if (!hn) {
        if (special && strcmp(u->scheme, "file")) return false;
        if (at >= 0 || colon >= 0) return false;
        sb_set(&u->host, "", 0); u->host.len = 0;
        u->has_host = true;
        u->port = -1;
        return true;
    }
    if (!parse_host(s, hn, special, &u->host)) return false;
    if (!strcmp(u->scheme, "file") && !strcmp(S(&u->host), "localhost")) { sb_set(&u->host, "", 0); u->host.len = 0; }
    u->has_host = true;
    u->port = -1;
    if (colon >= 0 && !parse_port(s + colon + 1, n - colon - 1, u->scheme, &u->port)) return false;
    return true;
}

static void set_query(url_t *u, const char *s, int n) {
    sb_set(&u->query, "", 0); u->query.len = 0;
    pct_put(&u->query, s, n, is_special(u->scheme) ? SET_SQUERY : SET_QUERY);
    sb_putn(&u->query, "", 0);
    u->has_query = true;
}
static void set_frag(url_t *u, const char *s, int n) {
    sb_set(&u->frag, "", 0); u->frag.len = 0;
    pct_put(&u->frag, s, n, SET_FRAG);
    sb_putn(&u->frag, "", 0);
    u->has_frag = true;
}

/* resto depois da autoridade: caminho, ?consulta, #fragmento; base_segs = segmentos de partida */
static void parse_rest(url_t *u, const char *s, int n, segs_t *g, bool special) {
    int q = -1, h = -1;
    for (int i = 0; i < n; i++) { if (s[i] == '#') { h = i; break; } if (s[i] == '?' && q < 0) q = i; }
    int pe = q >= 0 ? q : h >= 0 ? h : n;
    if (pe > 0 || (special && g)) path_apply(g, s, pe, special, u->scheme);
    segs_to_path(g, &u->path);
    if (special && !g->n) { sb_set(&u->path, "/", 1); }
    if (q >= 0) set_query(u, s + q + 1, (h >= 0 ? h : n) - q - 1);
    if (h >= 0) set_frag(u, s + h + 1, n - h - 1);
}

static bool url_parse(const char *in, const url_t *base, url_t *u) {
    memset(u, 0, sizeof *u);
    u->port = -1;
    /* tira espaços/controles das pontas e tab/LF/CR de dentro */
    sbuf_t clean = { 0 };
    int n = (int)strlen(in), a = 0, b = n;
    while (a < b && (u8)in[a] <= 0x20) a++;
    while (b > a && (u8)in[b - 1] <= 0x20) b--;
    for (int i = a; i < b; i++) if (in[i] != '\t' && in[i] != '\n' && in[i] != '\r') sb_putc(&clean, in[i]);
    sb_putn(&clean, "", 0);
    const char *s = S(&clean);
    n = clean.len;
    /* esquema */
    int i = 0;
    bool has_scheme = false;
    if (n && isalpha((u8)s[0])) {
        while (i < n && (isalnum((u8)s[i]) || s[i] == '+' || s[i] == '-' || s[i] == '.')) i++;
        if (i < n && s[i] == ':' && i < (int)sizeof u->scheme) {
            for (int k = 0; k < i; k++) u->scheme[k] = (char)tolower((u8)s[k]);
            u->scheme[i] = 0;
            has_scheme = true;
        }
    }
    bool ok = false;
    segs_t g;
    memset(&g, 0, sizeof g);
    if (has_scheme) {
        const char *r = s + i + 1;
        int rn = n - i - 1;
        bool special = is_special(u->scheme);
        if (!strcmp(u->scheme, "file")) {
            if (rn >= 2 && (r[0] == '/' || r[0] == '\\') && (r[1] == '/' || r[1] == '\\')) {
                int e = 2;
                while (e < rn && r[e] != '/' && r[e] != '\\' && r[e] != '?' && r[e] != '#') e++;
                /* file://C:/x: a letra de unidade é caminho, não host */
                if (e - 2 == 2 && drive_letter(r + 2, 2, false)) { u->has_host = true; sb_set(&u->host, "", 0); u->host.len = 0; parse_rest(u, r + 2, rn - 2, &g, true); }
                else { if (!parse_authority(u, r + 2, e - 2, true)) goto out; if (u->user.len || u->pass.len || u->port >= 0) goto out; parse_rest(u, r + e, rn - e, &g, true); }
            } else if (base && !strcmp(base->scheme, "file") && !(rn && (r[0] == '/' || r[0] == '\\'))) {
                u->has_host = true; sb_copy(&u->host, &base->host);
                if (rn && r[0] != '?' && r[0] != '#') { segs_from_path(&g, S(&base->path)); segs_pop(&g, "file"); }
                else segs_from_path(&g, S(&base->path));
                if (rn && (r[0] == '?' || r[0] == '#')) {
                    if (base->has_query && r[0] == '#') { sb_copy(&u->query, &base->query); u->has_query = true; }
                }
                parse_rest(u, r, rn, &g, true);
            } else { u->has_host = true; sb_set(&u->host, "", 0); u->host.len = 0; parse_rest(u, r, rn, &g, true); }
            ok = true;
        } else if (special) {
            if (base && !strcmp(base->scheme, u->scheme) && !(rn && (r[0] == '/' || r[0] == '\\'))) {
                /* mesmo esquema sem barras: relativo à base */
                url_free(u);
                bool r2 = url_parse(r, base, u);
                if (r2) strcpy(u->scheme, base->scheme);
                sb_free(&clean);
                return r2;
            }
            int k = 0;
            while (k < rn && (r[k] == '/' || r[k] == '\\')) k++;
            int e = k;
            while (e < rn && r[e] != '/' && r[e] != '\\' && r[e] != '?' && r[e] != '#') e++;
            if (!parse_authority(u, r + k, e - k, true)) goto out;
            parse_rest(u, r + e, rn - e, &g, true);
            ok = true;
        } else if (rn >= 2 && r[0] == '/' && r[1] == '/') {
            int e = 2;
            while (e < rn && r[e] != '/' && r[e] != '?' && r[e] != '#') e++;
            if (!parse_authority(u, r + 2, e - 2, false)) goto out;
            parse_rest(u, r + e, rn - e, &g, false);
            ok = true;
        } else if (rn && r[0] == '/') {
            parse_rest(u, r, rn, &g, false);
            ok = true;
        } else {
            /* caminho opaco (mailto:, data:, javascript:, blob: ...) */
            u->opaque = true;
            int q = -1, h = -1;
            for (int k = 0; k < rn; k++) { if (r[k] == '#') { h = k; break; } if (r[k] == '?' && q < 0) q = k; }
            int pe = q >= 0 ? q : h >= 0 ? h : rn;
            pct_put(&u->path, r, pe, SET_C0);
            sb_putn(&u->path, "", 0);
            if (q >= 0) set_query(u, r + q + 1, (h >= 0 ? h : rn) - q - 1);
            if (h >= 0) set_frag(u, r + h + 1, rn - h - 1);
            ok = true;
        }
    } else {
        if (!base) goto out;
        strcpy(u->scheme, base->scheme);
        bool special = is_special(u->scheme);
        if (base->opaque) {
            if (!n || s[0] != '#') goto out;
            sb_copy(&u->path, &base->path);
            u->opaque = true;
            if (base->has_query) { sb_copy(&u->query, &base->query); u->has_query = true; }
            set_frag(u, s + 1, n - 1);
            ok = true;
            goto out;
        }
        if (n >= 2 && (s[0] == '/' || (special && s[0] == '\\')) && (s[1] == '/' || (special && s[1] == '\\'))) {
            sb_free(&clean);
            char tmp[48];
            snprintf(tmp, sizeof tmp, "%s:", base->scheme);
            sbuf_t full = { 0 };
            sb_puts(&full, tmp);
            sb_puts(&full, in);
            bool r2 = url_parse(S(&full), NULL, u);
            sb_free(&full);
            return r2;
        }
        u->has_host = base->has_host;
        sb_copy(&u->user, &base->user);
        sb_copy(&u->pass, &base->pass);
        sb_copy(&u->host, &base->host);
        u->port = base->port;
        if (n && (s[0] == '/' || (special && s[0] == '\\'))) {
            if (!strcmp(u->scheme, "file")) { segs_from_path(&g, S(&base->path)); while (g.n > 1) segs_pop(&g, "file"); if (g.n == 1 && !drive_letter(S(&g.b), (int)strlen(S(&g.b)), true)) segs_pop(&g, "x"); }
            parse_rest(u, s, n, &g, special);
        } else if (n && (s[0] == '?' || s[0] == '#')) {
            sb_copy(&u->path, &base->path);
            if (s[0] == '#' && base->has_query) { sb_copy(&u->query, &base->query); u->has_query = true; }
            int h = -1;
            for (int k = 0; k < n; k++) if (s[k] == '#') { h = k; break; }
            if (s[0] == '?') set_query(u, s + 1, (h >= 0 ? h : n) - 1);
            if (h >= 0) set_frag(u, s + h + 1, n - h - 1);
        } else if (!n) {
            sb_copy(&u->path, &base->path);
            if (base->has_query) { sb_copy(&u->query, &base->query); u->has_query = true; }
        } else {
            segs_from_path(&g, S(&base->path));
            segs_pop(&g, u->scheme);
            parse_rest(u, s, n, &g, special);
        }
        ok = true;
    }
out:
    sb_free(&g.b);
    sb_free(&clean);
    if (!ok) url_free(u);
    return ok;
}

static void url_href(const url_t *u, sbuf_t *o, bool frag) {
    sb_puts(o, u->scheme);
    sb_putc(o, ':');
    if (u->has_host) {
        sb_puts(o, "//");
        if (u->user.len || u->pass.len) {
            sb_putn(o, S(&u->user), u->user.len);
            if (u->pass.len) { sb_putc(o, ':'); sb_putn(o, S(&u->pass), u->pass.len); }
            sb_putc(o, '@');
        }
        sb_putn(o, S(&u->host), u->host.len);
        if (u->port >= 0) { char b[12]; snprintf(b, sizeof b, ":%d", u->port); sb_puts(o, b); }
    } else if (!u->opaque && u->path.len >= 2 && S(&u->path)[0] == '/' && S(&u->path)[1] == '/') sb_puts(o, "/.");
    sb_putn(o, S(&u->path), u->path.len);
    if (u->has_query) { sb_putc(o, '?'); sb_putn(o, S(&u->query), u->query.len); }
    if (frag && u->has_frag) { sb_putc(o, '#'); sb_putn(o, S(&u->frag), u->frag.len); }
}

static void url_origin(const url_t *u, sbuf_t *o) {
    if (!strcmp(u->scheme, "blob")) {
        url_t in;
        if (url_parse(S(&u->path), NULL, &in)) {
            if (!strcmp(in.scheme, "http") || !strcmp(in.scheme, "https")) { url_origin(&in, o); url_free(&in); return; }
            url_free(&in);
        }
        sb_puts(o, "null");
        return;
    }
    if (!strcmp(u->scheme, "http") || !strcmp(u->scheme, "https") || !strcmp(u->scheme, "ws") || !strcmp(u->scheme, "wss") || !strcmp(u->scheme, "ftp")) {
        sb_puts(o, u->scheme);
        sb_puts(o, "://");
        sb_putn(o, S(&u->host), u->host.len);
        if (u->port >= 0) { char b[12]; snprintf(b, sizeof b, ":%d", u->port); sb_puts(o, b); }
        return;
    }
    sb_puts(o, "null");
}

/* análise com a base em texto, para o resto do motor: devolve false se inválida */
bool web_url_parse(const char *in, const char *base, char *out, int cap) {
    url_t b, u;
    bool hb = base && *base && url_parse(base, NULL, &b);
    bool ok = url_parse(in, hb ? &b : NULL, &u);
    if (hb) url_free(&b);
    if (!ok) return false;
    sbuf_t o = { 0 };
    url_href(&u, &o, true);
    url_free(&u);
    snprintf(out, cap, "%s", S(&o));
    sb_free(&o);
    return true;
}

/* ============================================================ application/x-www-form-urlencoded */
static void form_parse(jsobj *list, const char *s, int n) {
    int i = 0;
    while (i <= n) {
        int st = i;
        while (i < n && s[i] != '&') i++;
        if (i > st) {
            int eq = st;
            while (eq < i && s[eq] != '=') eq++;
            sbuf_t k = { 0 }, v = { 0 };
            pct_decode(&k, s + st, eq - st, true);
            if (eq < i) pct_decode(&v, s + eq + 1, i - eq - 1, true);
            bool bad;
            jsstr *ks = js_utf8_decode((const u8 *)S(&k), (u32)k.len, false, &bad);
            jsstr *vs = js_utf8_decode((const u8 *)S(&v), (u32)v.len, false, &bad);
            js_arr_push(list, jv_str(ks ? ks : A[A_empty]));
            js_arr_push(list, jv_str(vs ? vs : A[A_empty]));
            sb_free(&k); sb_free(&v);
        }
        i++;
    }
}

/* USVString: surrogates soltos em WTF-8 viram U+FFFD */
static void usv_put(sbuf_t *o, jsstr *s, int set) {
    sbuf_t t = { 0 };
    for (u32 k = 0; k < s->len; k++) {
        u8 c = (u8)s->d[k];
        if (c == 0xED && k + 2 < s->len && (u8)s->d[k + 1] >= 0xA0) { sb_puts(&t, "\xEF\xBF\xBD"); k += 2; }
        else sb_putc(&t, (char)c);
    }
    if (set < 0) sb_putn(o, S(&t), t.len); else pct_put(o, S(&t), t.len, set);
    sb_free(&t);
}

static jsstr *form_serialize(jsobj *list) {
    sbuf_t o = { 0 };
    for (u32 i = 0; i + 1 < list->alen; i += 2) {
        if (i) sb_putc(&o, '&');
        usv_put(&o, jv_s(list->arr[i]), SET_FORM);
        sb_putc(&o, '=');
        usv_put(&o, jv_s(list->arr[i + 1]), SET_FORM);
    }
    jsstr *r = js_strn(S(&o), o.len);
    sb_free(&o);
    return r;
}

/* ============================================================ objetos JS */
/* URL: extra = [href, URLSearchParams]; URLSearchParams: extra = [lista nome/valor, URL dono ou undefined] */
static bool has_proto(jsobj *o, jsobj *p) {
    for (int i = 0; o && i < 32; i++, o = o->proto) if (o->proto == p) return true;
    return false;
}

static jsobj *this_url(jsv v, url_t *u) {
    if (!jv_iscls(v, CL_HOST) || !has_proto(jv_o(v), J.realm->p_url) || !jv_isobj(jv_o(v)->u.host.extra)) {
        js_throw_err(E_TYPE, "Invocação ilegal: não é um URL");
        return NULL;
    }
    jsobj *x = jv_o(jv_o(v)->u.host.extra);
    if (u && !url_parse(jv_s(js_arr_get(x, 0))->d, NULL, u)) { js_throw_err(E_TYPE, "URL interno inválido"); return NULL; }
    return x;
}

static void url_store(jsobj *x, const url_t *u) {
    sbuf_t o = { 0 };
    url_href(u, &o, true);
    js_arr_set(x, 0, jv_str(js_strn(S(&o), o.len)));
    sb_free(&o);
    /* a lista do searchParams acompanha a consulta */
    jsv sp = js_arr_get(x, 1);
    if (jv_iscls(sp, CL_HOST) && jv_isobj(jv_o(sp)->u.host.extra)) {
        jsobj *l = js_newarr(0);
        if (u->has_query) form_parse(l, S(&u->query), u->query.len);
        js_arr_set(jv_o(jv_o(sp)->u.host.extra), 0, jv_obj(l));
    }
}

static jsobj *new_params(jsobj *list, jsv owner) {
    jsobj *o = js_newobj_cls(CL_HOST, J.realm->p_usp);
    o->u.host.page = js_cur_page();
    jsv a[2] = { jv_obj(list), owner };
    o->u.host.extra = jv_obj(js_arr_from(a, 2));
    return o;
}

static jsv url_make(jsobj *proto, const url_t *u) {
    jsobj *o = js_newobj_cls(CL_HOST, proto);
    o->u.host.page = js_cur_page();
    jsobj *x = js_newarr(0);
    js_arr_push(x, jv_str(A[A_empty]));
    js_arr_push(x, JS_UNDEF);
    o->u.host.extra = jv_obj(x);
    jsobj *l = js_newarr(0);
    js_arr_set(x, 1, jv_obj(new_params(l, jv_obj(o))));
    url_store(x, u);
    return jv_obj(o);
}

static int parse_args(int argc, jsv *argv, url_t *u) {   /* 1 ok, 0 inválida, -1 exceção */
    jsstr *s = js_tostr(ARG(0));
    if (!s) return -1;
    url_t b;
    bool hb = false;
    if (argc > 1 && ARG(1) != JS_UNDEF) {
        jsstr *bs = js_tostr(ARG(1));
        if (!bs) return -1;
        if (!url_parse(bs->d, NULL, &b)) return 0;
        hb = true;
    }
    bool ok = url_parse(s->d, hb ? &b : NULL, u);
    if (hb) url_free(&b);
    return ok ? 1 : 0;
}

NATIVE(url_ctor) {
    UNUSED_ARGS;
    if (!(flags & NF_NEW)) return js_throw_err(E_TYPE, "O construtor URL exige 'new'");
    if (argc < 1) return js_throw_err(E_TYPE, "URL: é necessário 1 argumento");
    url_t u;
    int r = parse_args(argc, argv, &u);
    if (r < 0) return JS_EXC;
    if (!r) { jsstr *s = js_tostr(ARG(0)); return js_throw_err(E_TYPE, "URL inválido: %s", s ? s->d : "?"); }
    jsv o = url_make(jv_isobj(thisv) && jv_o(thisv)->proto ? jv_o(thisv)->proto : J.realm->p_url, &u);
    url_free(&u);
    return o;
}

/* magic 0 canParse, 1 parse */
NATIVE(url_static) {
    UNUSED_ARGS;
    if (argc < 1) return js_throw_err(E_TYPE, "URL: é necessário 1 argumento");
    url_t u;
    int r = parse_args(argc, argv, &u);
    if (r < 0) return JS_EXC;
    if (self->u.nat.magic == 0) { if (r) url_free(&u); return jv_bool(r == 1); }
    if (!r) return JS_NULL;
    jsv o = url_make(J.realm->p_url, &u);
    url_free(&u);
    return o;
}

enum { UP_HREF, UP_ORIGIN, UP_PROTOCOL, UP_USER, UP_PASS, UP_HOST, UP_HOSTNAME, UP_PORT, UP_PATH, UP_SEARCH, UP_SEARCHPARAMS, UP_HASH, UP_COUNT };
static const char *up_names[UP_COUNT] = { "href", "origin", "protocol", "username", "password", "host", "hostname", "port", "pathname", "search", "searchParams", "hash" };

NATIVE(url_get) {
    UNUSED_ARGS;
    url_t u;
    jsobj *x = this_url(thisv, self->u.nat.magic == UP_HREF || self->u.nat.magic == UP_SEARCHPARAMS ? NULL : &u);
    if (!x) return JS_EXC;
    int m = self->u.nat.magic;
    if (m == UP_HREF) return js_arr_get(x, 0);
    if (m == UP_SEARCHPARAMS) return js_arr_get(x, 1);
    sbuf_t o = { 0 };
    switch (m) {
    case UP_ORIGIN: url_origin(&u, &o); break;
    case UP_PROTOCOL: sb_puts(&o, u.scheme); sb_putc(&o, ':'); break;
    case UP_USER: sb_putn(&o, S(&u.user), u.user.len); break;
    case UP_PASS: sb_putn(&o, S(&u.pass), u.pass.len); break;
    case UP_HOST: case UP_HOSTNAME:
        sb_putn(&o, S(&u.host), u.host.len);
        if (m == UP_HOST && u.port >= 0) { char b[12]; snprintf(b, sizeof b, ":%d", u.port); sb_puts(&o, b); }
        break;
    case UP_PORT: if (u.port >= 0) { char b[12]; snprintf(b, sizeof b, "%d", u.port); sb_puts(&o, b); } break;
    case UP_PATH: sb_putn(&o, S(&u.path), u.path.len); break;
    case UP_SEARCH: if (u.has_query && u.query.len) { sb_putc(&o, '?'); sb_putn(&o, S(&u.query), u.query.len); } break;
    case UP_HASH: if (u.has_frag && u.frag.len) { sb_putc(&o, '#'); sb_putn(&o, S(&u.frag), u.frag.len); } break;
    }
    url_free(&u);
    jsstr *r = js_strn(S(&o), o.len);
    sb_free(&o);
    return jv_str(r);
}

NATIVE(url_set) {
    UNUSED_ARGS;
    int m = self->u.nat.magic;
    jsstr *v = js_tostr(ARG(0));
    if (!v) return JS_EXC;
    if (m == UP_HREF) {
        jsobj *x = this_url(thisv, NULL);
        if (!x) return JS_EXC;
        url_t u;
        if (!url_parse(v->d, NULL, &u)) return js_throw_err(E_TYPE, "URL inválido: %s", v->d);
        url_store(x, &u);
        url_free(&u);
        return JS_UNDEF;
    }
    url_t u;
    jsobj *x = this_url(thisv, &u);
    if (!x) return JS_EXC;
    bool special = is_special(u.scheme);
    const char *s = v->d;
    int n = (int)v->len;
    switch (m) {
    case UP_PROTOCOL: {
        char sc[40];
        int k = 0;
        while (k < n && k < 39 && s[k] != ':') { if (!(isalnum((u8)s[k]) || s[k] == '+' || s[k] == '-' || s[k] == '.')) break; sc[k] = (char)tolower((u8)s[k]); k++; }
        sc[k] = 0;
        if (!k || !isalpha((u8)sc[0]) || (k < n && s[k] != ':')) break;
        if (is_special(sc) != special) break;
        if (!strcmp(sc, "file") && (u.user.len || u.pass.len || u.port >= 0)) break;
        if (!strcmp(u.scheme, "file") && !u.host.len && strcmp(sc, "file")) break;
        strcpy(u.scheme, sc);
        if (u.port == default_port(sc)) u.port = -1;
        break;
    }
    case UP_USER: case UP_PASS:
        if (!u.has_host || !u.host.len || !strcmp(u.scheme, "file")) break;
        sb_set(m == UP_USER ? &u.user : &u.pass, "", 0);
        (m == UP_USER ? &u.user : &u.pass)->len = 0;
        pct_put(m == UP_USER ? &u.user : &u.pass, s, n, SET_USER);
        break;
    case UP_HOST: case UP_HOSTNAME: {
        if (u.opaque) break;
        int e = 0;
        bool br = false;
        while (e < n && s[e] != '/' && s[e] != '?' && s[e] != '#' && !(special && s[e] == '\\')) {
            if (s[e] == '[') br = true; else if (s[e] == ']') br = false;
            else if (s[e] == ':' && !br) break;
            e++;
        }
        if (!e && special) break;
        sbuf_t h = { 0 };
        if (e && !parse_host(s, e, special, &h)) { sb_free(&h); break; }
        if (!strcmp(u.scheme, "file") && !strcmp(S(&h), "localhost")) { sb_free(&h); memset(&h, 0, sizeof h); }
        sb_copy(&u.host, &h);
        if (!e) u.host.len = 0;
        sb_free(&h);
        u.has_host = true;
        if (m == UP_HOST && e < n && s[e] == ':') {
            int p0 = e + 1, p1 = p0;
            while (p1 < n && isdigit((u8)s[p1])) p1++;
            int port;
            if (p1 > p0 && parse_port(s + p0, p1 - p0, u.scheme, &port)) u.port = port;
        }
        break;
    }
    case UP_PORT: {
        if (!u.has_host || !u.host.len || !strcmp(u.scheme, "file")) break;
        if (!n) { u.port = -1; break; }
        int p1 = 0;
        while (p1 < n && isdigit((u8)s[p1])) p1++;
        int port;
        if (p1 && parse_port(s, p1, u.scheme, &port)) u.port = port;
        break;
    }
    case UP_PATH: {
        if (u.opaque) break;
        segs_t g;
        memset(&g, 0, sizeof g);
        path_apply(&g, s, n, special, u.scheme);
        segs_to_path(&g, &u.path);
        if (special && !g.n) sb_set(&u.path, "/", 1);
        sb_free(&g.b);
        break;
    }
    case UP_SEARCH:
        if (!n) { u.has_query = false; sb_set(&u.query, "", 0); u.query.len = 0; break; }
        if (s[0] == '?') { s++; n--; }
        set_query(&u, s, n);
        break;
    case UP_HASH:
        if (!n) { u.has_frag = false; sb_set(&u.frag, "", 0); u.frag.len = 0; break; }
        if (s[0] == '#') { s++; n--; }
        set_frag(&u, s, n);
        break;
    }
    url_store(x, &u);
    url_free(&u);
    return JS_UNDEF;
}

NATIVE(url_tostring) {
    UNUSED_ARGS;
    jsobj *x = this_url(thisv, NULL);
    return x ? js_arr_get(x, 0) : JS_EXC;
}

/* ---------------------------------------------------------------- URLSearchParams */
static jsobj *this_params(jsv v, jsobj **list) {
    if (!jv_iscls(v, CL_HOST) || !has_proto(jv_o(v), J.realm->p_usp) || !jv_isobj(jv_o(v)->u.host.extra)) {
        js_throw_err(E_TYPE, "Invocação ilegal: não é um URLSearchParams");
        return NULL;
    }
    jsobj *x = jv_o(jv_o(v)->u.host.extra);
    *list = jv_o(js_arr_get(x, 0));
    return x;
}

/* lista mudou: a consulta do URL dono acompanha */
static void params_update(jsobj *x, jsobj *list) {
    jsv owner = js_arr_get(x, 1);
    if (!jv_iscls(owner, CL_HOST)) return;
    jsobj *ux = jv_o(jv_o(owner)->u.host.extra);
    url_t u;
    if (!url_parse(jv_s(js_arr_get(ux, 0))->d, NULL, &u)) return;
    jsstr *q = form_serialize(list);
    if (q->len) { sb_set(&u.query, q->d, (int)q->len); u.has_query = true; }
    else { u.has_query = false; sb_set(&u.query, "", 0); u.query.len = 0; }
    sbuf_t o = { 0 };
    url_href(&u, &o, true);
    js_arr_set(ux, 0, jv_str(js_strn(S(&o), o.len)));
    sb_free(&o);
    url_free(&u);
}

static jsstr *usv(jsv v) {   /* ToString + USVString */
    jsstr *s = js_tostr(v);
    if (!s) return NULL;
    bool lone = false;
    for (u32 k = 0; k + 2 < s->len; k++) if ((u8)s->d[k] == 0xED && (u8)s->d[k + 1] >= 0xA0) lone = true;
    if (!lone) return s;
    sbuf_t o = { 0 };
    usv_put(&o, s, -1);
    jsstr *r = js_strn(S(&o), o.len);
    sb_free(&o);
    return r;
}

NATIVE(usp_ctor) {
    UNUSED_ARGS;
    if (!(flags & NF_NEW)) return js_throw_err(E_TYPE, "O construtor URLSearchParams exige 'new'");
    jsobj *list = js_newarr(0);
    jsv init = ARG(0);
    if (jv_isobj(init) && !jv_iscls(init, CL_STRING)) {
        jsv itf = js_getk(init, A[A_sym_iterator]);
        if (js_pending()) return JS_EXC;
        if (js_iscallable(itf)) {
            /* sequência de pares */
            jsv from = js_getn(jv_obj(J.realm->c_array), "from");
            if (js_pending()) return JS_EXC;
            jsv items = js_call(from, jv_obj(J.realm->c_array), 1, &init);
            if (items == JS_EXC) return JS_EXC;
            u32 n = js_lengthof(items);
            for (u32 i = 0; i < n; i++) {
                jsv pair = js_getidx(items, i);
                if (js_pending()) return JS_EXC;
                jsv pa = js_call(from, jv_obj(J.realm->c_array), 1, &pair);
                if (pa == JS_EXC) return JS_EXC;
                if (js_lengthof(pa) != 2) return js_throw_err(E_TYPE, "URLSearchParams: cada par precisa de 2 itens");
                jsstr *k = usv(js_getidx(pa, 0)), *v = k ? usv(js_getidx(pa, 1)) : NULL;
                if (!k || !v) return JS_EXC;
                js_arr_push(list, jv_str(k));
                js_arr_push(list, jv_str(v));
            }
        } else {
            /* registro: chaves próprias enumeráveis */
            jsobj *keys = js_keylist(jv_o(init), true, false);
            for (u32 i = 0; keys && i < keys->alen; i++) {
                jsv k = keys->arr[i];
                jsv v = js_get(init, k);
                if (js_pending()) return JS_EXC;
                jsstr *ks = usv(k), *vs = ks ? usv(v) : NULL;
                if (!ks || !vs) return JS_EXC;
                js_arr_push(list, jv_str(ks));
                js_arr_push(list, jv_str(vs));
            }
        }
    } else if (init != JS_UNDEF) {
        jsstr *s = usv(init);
        if (!s) return JS_EXC;
        const char *p = s->d;
        int n = (int)s->len;
        if (n && p[0] == '?') { p++; n--; }
        form_parse(list, p, n);
    }
    jsobj *o = new_params(list, JS_UNDEF);
    if (jv_isobj(thisv) && jv_o(thisv)->proto && jv_o(thisv)->proto != J.realm->p_object) o->proto = jv_o(thisv)->proto;
    return jv_obj(o);
}

/* magic: 0 append 1 delete 2 get 3 getAll 4 has 5 set 6 sort 7 toString 8 forEach 9 entries 10 keys 11 values 12 size */
NATIVE(usp_method) {
    UNUSED_ARGS;
    jsobj *list;
    jsobj *x = this_params(thisv, &list);
    if (!x) return JS_EXC;
    int m = self->u.nat.magic;
    static const int need[] = { 2, 1, 1, 1, 1, 2, 0, 0, 1, 0, 0, 0, 0 };
    if (argc < need[m]) return js_throw_err(E_TYPE, "URLSearchParams: faltam argumentos");
    jsstr *k = NULL, *v = NULL;
    if (m <= 5) { k = usv(ARG(0)); if (!k) return JS_EXC; }
    if (m == 0 || m == 5 || ((m == 1 || m == 4) && argc > 1 && ARG(1) != JS_UNDEF)) { v = usv(ARG(1)); if (!v) return JS_EXC; }
    switch (m) {
    case 0: js_arr_push(list, jv_str(k)); js_arr_push(list, jv_str(v)); params_update(x, list); return JS_UNDEF;
    case 1: case 4: {
        jsobj *out = js_newarr(0);
        bool found = false;
        for (u32 i = 0; i + 1 < list->alen; i += 2) {
            bool hit = js_str_eq(jv_s(list->arr[i]), k) && (!v || js_str_eq(jv_s(list->arr[i + 1]), v));
            if (hit) found = true;
            if (m == 1 && !hit) { js_arr_push(out, list->arr[i]); js_arr_push(out, list->arr[i + 1]); }
        }
        if (m == 4) return jv_bool(found);
        js_arr_set(x, 0, jv_obj(out));
        params_update(x, out);
        return JS_UNDEF;
    }
    case 2: for (u32 i = 0; i + 1 < list->alen; i += 2) if (js_str_eq(jv_s(list->arr[i]), k)) return list->arr[i + 1]; return JS_NULL;
    case 3: {
        jsobj *r = js_newarr(0);
        for (u32 i = 0; i + 1 < list->alen; i += 2) if (js_str_eq(jv_s(list->arr[i]), k)) js_arr_push(r, list->arr[i + 1]);
        return jv_obj(r);
    }
    case 5: {
        jsobj *out = js_newarr(0);
        bool done = false;
        for (u32 i = 0; i + 1 < list->alen; i += 2) {
            if (js_str_eq(jv_s(list->arr[i]), k)) { if (!done) { js_arr_push(out, jv_str(k)); js_arr_push(out, jv_str(v)); done = true; } continue; }
            js_arr_push(out, list->arr[i]);
            js_arr_push(out, list->arr[i + 1]);
        }
        if (!done) { js_arr_push(out, jv_str(k)); js_arr_push(out, jv_str(v)); }
        js_arr_set(x, 0, jv_obj(out));
        params_update(x, out);
        return JS_UNDEF;
    }
    case 6: {
        /* ordenação estável por unidades UTF-16 do nome (inserção) */
        u32 n = list->alen / 2;
        for (u32 i = 1; i < n; i++) {
            jsv kk = list->arr[2 * i], vv = list->arr[2 * i + 1];
            u32 j = i;
            while (j > 0 && js_str_cmp(jv_s(list->arr[2 * (j - 1)]), jv_s(kk)) > 0) {
                list->arr[2 * j] = list->arr[2 * (j - 1)];
                list->arr[2 * j + 1] = list->arr[2 * (j - 1) + 1];
                j--;
            }
            list->arr[2 * j] = kk;
            list->arr[2 * j + 1] = vv;
        }
        params_update(x, list);
        return JS_UNDEF;
    }
    case 7: return jv_str(form_serialize(list));
    case 8: {
        jsv fn = ARG(0);
        if (!js_iscallable(fn)) return js_throw_err(E_TYPE, "URLSearchParams.forEach: função esperada");
        for (u32 i = 0; i + 1 < list->alen; i += 2) {
            jsv a[3] = { list->arr[i + 1], list->arr[i], thisv };
            if (js_call(fn, ARG(1), 3, a) == JS_EXC) return JS_EXC;
            jsobj *l2;
            this_params(thisv, &l2);
            list = l2;
        }
        return JS_UNDEF;
    }
    case 9: case 10: case 11: {
        jsobj *arr = js_newarr(0);
        for (u32 i = 0; i + 1 < list->alen; i += 2) {
            if (m == 9) { jsv pr[2] = { list->arr[i], list->arr[i + 1] }; js_arr_push(arr, jv_obj(js_arr_from(pr, 2))); }
            else js_arr_push(arr, list->arr[m == 10 ? i : i + 1]);
        }
        return js_invoke(jv_obj(arr), js_intern("values", -1), 0, NULL);
    }
    case 12: return jv_int((int)(list->alen / 2));
    }
    return JS_UNDEF;
}

jsstr *js_usp_string(jsv v) {
    if (!jv_iscls(v, CL_HOST) || !J.realm->p_usp || !has_proto(jv_o(v), J.realm->p_usp) || !jv_isobj(jv_o(v)->u.host.extra)) return NULL;
    return form_serialize(jv_o(js_arr_get(jv_o(jv_o(v)->u.host.extra), 0)));
}

/* ============================================================ registro */
void js_url_init(jsrealm *r) {
    jsobj *g = r->global, *op = r->p_object;
    r->p_url = js_newobj(op);
    jsobj *uc = js_newctor(url_ctor, "URL", 1, r->p_url);
    js_defn(g, "URL", jv_obj(uc), PF_NOENUM);
    js_defn(g, "webkitURL", jv_obj(uc), PF_NOENUM);
    js_defprop(r->p_url, A[A_sym_toStringTag], jv_str(js_intern("URL", -1)), PF_NOENUM | PF_RO);
    for (int i = 0; i < UP_COUNT; i++) {
        bool ro = i == UP_ORIGIN || i == UP_SEARCHPARAMS;
        js_defaccessor(r->p_url, js_intern(up_names[i], -1), jv_obj(js_newnative(url_get, up_names[i], 0, i)),
                       ro ? JS_UNDEF : jv_obj(js_newnative(url_set, up_names[i], 1, i)), 0);
    }
    js_defnative(r->p_url, "toString", url_tostring, 0, 0);
    js_defnative(r->p_url, "toJSON", url_tostring, 0, 0);
    js_defnative(uc, "canParse", url_static, 1, 0);
    js_defnative(uc, "parse", url_static, 1, 1);
    js_blob_url_statics(uc);
    r->p_usp = js_newobj(op);
    jsobj *pc = js_newctor(usp_ctor, "URLSearchParams", 0, r->p_usp);
    js_defn(g, "URLSearchParams", jv_obj(pc), PF_NOENUM);
    js_defprop(r->p_usp, A[A_sym_toStringTag], jv_str(js_intern("URLSearchParams", -1)), PF_NOENUM | PF_RO);
    static const char *mn[] = { "append", "delete", "get", "getAll", "has", "set", "sort", "toString", "forEach", "entries", "keys", "values" };
    static const int ma[] = { 2, 1, 1, 1, 1, 2, 0, 0, 1, 0, 0, 0 };
    for (int i = 0; i < 12; i++) js_defnative(r->p_usp, mn[i], usp_method, ma[i], i);
    js_defgetter(r->p_usp, "size", usp_method, 12);
    jsprop *ent = js_ownprop(r->p_usp, js_intern("entries", -1));
    if (ent) js_defprop(r->p_usp, A[A_sym_iterator], ent->v, PF_NOENUM);
}
