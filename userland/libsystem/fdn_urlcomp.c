/*
 * URLComponents do overlay Swift do Foundation. Não é @frozen: o app passa por endereço e lê o
 * tamanho na metadata (fdn_meta.S, tabela fdn_vwt_box). O valor é uma caixa com contagem de
 * referências e os componentes como o RFC 3986 os separa, guardados já com percent-encoding;
 * os getters sem "percentEncoded" decodificam e os setters codificam com o conjunto permitido
 * em cada componente (os mesmos do NSCharacterSet: urlUserAllowed, urlHostAllowed...).
 * Setter (mutating) copia a caixa se ela estiver compartilhada.
 * queryItems (Array de URLQueryItem) não está aqui: monta Array nativo, que pede o runtime.
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include "fdn_tipos.h"

char *strcat(char *d, const char *s);

#define SW __attribute__((swiftcall))
#define CTX __attribute__((swift_context))
#define USED __attribute__((used))
#if defined(__x86_64__)
#define SRET(sym, impl) __asm__(".text\n.p2align 4\n.globl \"" sym "\"\n\"" sym "\":\n" \
    "  movq %r8, %r9\n  movq %rcx, %r8\n  movq %rdx, %rcx\n  movq %rsi, %rdx\n" \
    "  movq %rdi, %rsi\n  movq %rax, %rdi\n  jmp " #impl "\n")
#else
#define SRET(sym, impl)
#endif

enum { C_SCHEME, C_USER, C_PASSWORD, C_HOST, C_PATH, C_QUERY, C_FRAGMENT, C_N };

typedef struct { fdn_box h; char *c[C_N]; long port; int has_port; } ucomp;

static void uc_free(void *p) {
    ucomp *u = p;
    for (int i = 0; i < C_N; i++) free(u->c[i]);
}
static char *sdup(const char *s, size_t n) {
    char *d;
    if (!s) return 0;
    d = malloc(n + 1);
    if (!d) return 0;
    memcpy(d, s, n);
    d[n] = 0;
    return d;
}
static ucomp *uc_new(void) {
    ucomp *u = fdn_box_new(sizeof *u, uc_free);
    if (u) u->c[C_PATH] = sdup("", 0);
    return u;
}
static ucomp *uc_clone(const ucomp *s) {
    ucomp *u = fdn_box_new(sizeof *u, uc_free);
    if (!u) return 0;
    for (int i = 0; i < C_N; i++) u->c[i] = s->c[i] ? sdup(s->c[i], strlen(s->c[i])) : 0;
    u->port = s->port;
    u->has_port = s->has_port;
    return u;
}
/* a caixa do valor passa a ser só dele antes de escrever */
static ucomp *uc_own(ucomp **self) {
    if ((*self)->h.rc > 1) {
        ucomp *n = uc_clone(*self);
        if (!n) return *self;
        fdn_box_release(*self);
        *self = n;
    }
    return *self;
}

/* ---------------------------------------------------------------- percent-encoding */

static int unreserved(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~';
}
static const char *const permitidos[C_N] = {
    "+-.",                        /* esquema: não é codificado, só validado */
    "!$&'()*+,;=",                /* user */
    "!$&'()*+,;=",                /* password */
    "!$&'()*+,;=[]:",             /* host */
    "!$&'()*+,;=:@/",             /* path */
    "!$&'()*+,;=:@/?",            /* query */
    "!$&'()*+,;=:@/?",            /* fragment */
};
static int allowed(int comp, int c) { return unreserved(c) || (c && strchr(permitidos[comp], c)); }

char *fdn_pct_encode(int comp, const char *s) {
    static const char hx[] = "0123456789ABCDEF";
    size_t n = strlen(s), k = 0;
    char *o = malloc(n * 3 + 1);
    if (!o) return 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (allowed(comp, c)) o[k++] = (char)c;
        else { o[k++] = '%'; o[k++] = hx[c >> 4]; o[k++] = hx[c & 15]; }
    }
    o[k] = 0;
    return o;
}
static int hexv(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
/* NULL se a sequência % for inválida (a Apple devolve nil no getter) */
char *fdn_pct_decode(const char *s) {
    size_t n = strlen(s), k = 0;
    char *o = malloc(n + 1);
    if (!o) return 0;
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '%') {
            int a = i + 2 < n ? hexv((unsigned char)s[i + 1]) : -1;
            int b = a >= 0 ? hexv((unsigned char)s[i + 2]) : -1;
            if (a < 0 || b < 0) { free(o); return 0; }
            o[k++] = (char)(a << 4 | b);
            i += 2;
        } else o[k++] = s[i];
    }
    o[k] = 0;
    return o;
}
/* o texto já codificado só pode ter caracteres permitidos e % seguido de dois hex */
static int valid_encoded(int comp, const char *s, size_t n) {
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '%') {
            if (i + 2 >= n || hexv(s[i + 1]) < 0 || hexv(s[i + 2]) < 0) return 0;
            i += 2;
        } else if (!allowed(comp, c)) return 0;
    }
    return 1;
}

/* ---------------------------------------------------------------- parse e texto */

/* 0 = ok; -1 = texto que o URLComponents(string:) recusa */
int fdn_urlcomp_parse(const char *s, void **out) {
    ucomp *u = uc_new();
    const char *p = s, *e;
    if (!u) return -1;
    *out = u;
    /* esquema */
    e = p;
    while (*e && *e != ':' && *e != '/' && *e != '?' && *e != '#') e++;
    if (*e == ':' && e > p && ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z'))) {
        for (const char *k = p; k < e; k++) if (!(unreserved((unsigned char)*k) || *k == '+')) goto ruim;
        u->c[C_SCHEME] = sdup(p, (size_t)(e - p));
        p = e + 1;
    }
    /* autoridade */
    if (p[0] == '/' && p[1] == '/') {
        const char *a = p + 2, *ae = a, *at = 0, *hs, *he, *pc = 0;
        while (*ae && *ae != '/' && *ae != '?' && *ae != '#') { if (*ae == '@') at = ae; ae++; }
        hs = a;
        if (at) {
            const char *col = memchr(a, ':', (size_t)(at - a));
            const char *ue = col ? col : at;
            if (!valid_encoded(C_USER, a, (size_t)(ue - a))) goto ruim;
            u->c[C_USER] = sdup(a, (size_t)(ue - a));
            if (col) {
                if (!valid_encoded(C_PASSWORD, col + 1, (size_t)(at - col - 1))) goto ruim;
                u->c[C_PASSWORD] = sdup(col + 1, (size_t)(at - col - 1));
            }
            hs = at + 1;
        }
        he = ae;
        if (*hs == '[') {
            const char *rb = memchr(hs, ']', (size_t)(ae - hs));
            if (!rb) goto ruim;
            if (rb + 1 < ae && rb[1] == ':') pc = rb + 1;
            he = rb + 1;
        } else {
            for (const char *k = ae; k > hs; k--) if (k[-1] == ':') { pc = k - 1; break; }
            if (pc) he = pc;
        }
        if (pc) {
            long v = 0;
            int nd = 0;
            for (const char *k = pc + 1; k < ae; k++) {
                if (*k < '0' || *k > '9') goto ruim;
                v = v * 10 + (*k - '0');
                nd++;
            }
            if (nd) { u->port = v; u->has_port = 1; }
        }
        if (!valid_encoded(C_HOST, hs, (size_t)(he - hs))) goto ruim;
        u->c[C_HOST] = sdup(hs, (size_t)(he - hs));
        p = ae;
    }
    /* caminho, consulta, fragmento */
    e = p;
    while (*e && *e != '?' && *e != '#') e++;
    if (!valid_encoded(C_PATH, p, (size_t)(e - p))) goto ruim;
    free(u->c[C_PATH]);
    u->c[C_PATH] = sdup(p, (size_t)(e - p));
    p = e;
    if (*p == '?') {
        e = ++p;
        while (*e && *e != '#') e++;
        if (!valid_encoded(C_QUERY, p, (size_t)(e - p))) goto ruim;
        u->c[C_QUERY] = sdup(p, (size_t)(e - p));
        p = e;
    }
    if (*p == '#') {
        p++;
        if (!valid_encoded(C_FRAGMENT, p, strlen(p))) goto ruim;
        u->c[C_FRAGMENT] = sdup(p, strlen(p));
    }
    return 0;
ruim:
    fdn_box_release(u);
    *out = 0;
    return -1;
}

/* texto do URL (malloc); NULL se a combinação não forma URL (caminho sem / com autoridade) */
char *fdn_urlcomp_string(const void *v) {
    const ucomp *u = v;
    size_t n = 32;
    char *o;
    int auth = u->c[C_HOST] || u->c[C_USER] || u->c[C_PASSWORD] || u->has_port;
    const char *path = u->c[C_PATH] ? u->c[C_PATH] : "";
    if (auth && path[0] && path[0] != '/') return 0;
    if (!auth && path[0] == '/' && path[1] == '/') return 0;
    for (int i = 0; i < C_N; i++) n += u->c[i] ? strlen(u->c[i]) + 1 : 0;
    o = malloc(n);
    if (!o) return 0;
    o[0] = 0;
    if (u->c[C_SCHEME]) { strcat(o, u->c[C_SCHEME]); strcat(o, ":"); }
    if (auth) {
        strcat(o, "//");
        if (u->c[C_USER] || u->c[C_PASSWORD]) {
            if (u->c[C_USER]) strcat(o, u->c[C_USER]);
            if (u->c[C_PASSWORD]) { strcat(o, ":"); strcat(o, u->c[C_PASSWORD]); }
            strcat(o, "@");
        }
        if (u->c[C_HOST]) strcat(o, u->c[C_HOST]);
        if (u->has_port) {
            char b[24];
            int k = 0;
            long p = u->port < 0 ? 0 : u->port;
            char t[24];
            do { t[k++] = (char)('0' + p % 10); p /= 10; } while (p);
            b[0] = ':';
            for (int i = 0; i < k; i++) b[1 + i] = t[k - 1 - i];
            b[1 + k] = 0;
            strcat(o, b);
        }
    }
    strcat(o, path);
    if (u->c[C_QUERY]) { strcat(o, "?"); strcat(o, u->c[C_QUERY]); }
    if (u->c[C_FRAGMENT]) { strcat(o, "#"); strcat(o, u->c[C_FRAGMENT]); }
    return o;
}

/* ---------------------------------------------------------------- símbolos */

static fdn_str opt_take(char *s) {
    fdn_str z = { 0, 0 }, r;
    if (!s) return z;
    r = fdn_str_make(s, -1);
    free(s);
    return r;
}
static char *read_dup(fdn_str s) {
    long cap = 512, n;
    char *b = malloc((size_t)cap);
    while (b) {
        n = fdn_str_read(s, b, cap);
        if (n < 0) { free(b); return 0; }
        if (n < cap - 1) return b;
        cap *= 8;
        free(b);
        b = malloc((size_t)cap);
    }
    return 0;
}

SRET("$s10Foundation13URLComponentsVACycfC", uc_init);
static SW USED void uc_init(void **out) { *out = uc_new(); }

SRET("$s10Foundation13URLComponentsV6stringACSgSSh_tcfC", uc_init_string);
static SW USED void uc_init_string(void **out, fdn_str s) {
    char *t = read_dup(s);
    void *u = 0;
    if (t && fdn_urlcomp_parse(t, &u) == 0) { *out = u; fdn_enum_tag_store(out, 0, 1, 8); }
    else fdn_enum_tag_store(out, 1, 1, 8);
    free(t);
}
/* init?(url:resolvingAgainstBaseURL:): o URL (NSURL) chega por endereço */
SRET("$s10Foundation13URLComponentsV3url23resolvingAgainstBaseURLACSgAA0G0Vh_SbtcfC", uc_init_url);
static SW USED void uc_init_url(void **out, void *const *url, _Bool resolve) {
    const char *a = url && *url ? fdn_ns_url_part(*url, FDN_URL_ABS) : 0;
    void *u = 0;
    if (a && fdn_urlcomp_parse(a, &u) == 0) { *out = u; fdn_enum_tag_store(out, 0, 1, 8); }
    else fdn_enum_tag_store(out, 1, 1, 8);
}

SW fdn_str uc_get_string(void *const *self CTX) __asm__("$s10Foundation13URLComponentsV6stringSSSgvg");
SW fdn_str uc_get_string(void *const *self CTX) { return opt_take(fdn_urlcomp_string(*self)); }
SRET("$s10Foundation13URLComponentsV3urlAA3URLVSgvg", uc_get_url);
static SW USED void uc_get_url(void **out, void *const *self CTX) {
    char *s = fdn_urlcomp_string(*self);
    void *u = s && s[0] ? fdn_ns_url_string(s) : 0;
    free(s);
    if (u) { *out = u; fdn_enum_tag_store(out, 0, 1, 8); }
    else fdn_enum_tag_store(out, 1, 1, 8);
}

/* getters: decodificado (nil se o % for inválido) e percentEncoded (como está) */
static fdn_str get_dec(void *v, int c) {
    const char *s = ((ucomp *)v)->c[c];
    return opt_take(s ? (c == C_SCHEME ? sdup(s, strlen(s)) : fdn_pct_decode(s)) : 0);
}
static fdn_str get_raw(void *v, int c) {
    const char *s = ((ucomp *)v)->c[c];
    return opt_take(s ? sdup(s, strlen(s)) : 0);
}
static void set_comp(ucomp **self, int c, fdn_str v, int encoded) {
    ucomp *u = uc_own(self);
    char *t = (v.w0 || v.w1) ? read_dup(v) : 0;
    free(u->c[c]);
    u->c[c] = 0;
    if (t) {
        u->c[c] = encoded || c == C_SCHEME ? t : fdn_pct_encode(c, t);
        if (u->c[c] != t) free(t);
    }
    if (c == C_PATH && !u->c[c]) u->c[c] = sdup("", 0);
}

#define GETSET(nome, comp) \
    SW fdn_str uc_get_##nome(void *const *self CTX) __asm__("$s10Foundation13URLComponentsV" #nome "SSSgvg"); \
    SW fdn_str uc_get_##nome(void *const *self CTX) { return get_dec(*self, comp); } \
    SW void uc_set_##nome(fdn_str v, ucomp **self CTX) __asm__("$s10Foundation13URLComponentsV" #nome "SSSgvs"); \
    SW void uc_set_##nome(fdn_str v, ucomp **self CTX) { set_comp(self, comp, v, 0); }
#define GETSET_PE(nome, comp) \
    SW fdn_str uc_get_pe_##nome(void *const *self CTX) __asm__("$s10Foundation13URLComponentsV" #nome "SSSgvg"); \
    SW fdn_str uc_get_pe_##nome(void *const *self CTX) { return get_raw(*self, comp); } \
    SW void uc_set_pe_##nome(fdn_str v, ucomp **self CTX) __asm__("$s10Foundation13URLComponentsV" #nome "SSSgvs"); \
    SW void uc_set_pe_##nome(fdn_str v, ucomp **self CTX) { set_comp(self, comp, v, 1); }

GETSET(6scheme, C_SCHEME)
GETSET(4user, C_USER)
GETSET(8password, C_PASSWORD)
GETSET(4host, C_HOST)
GETSET(5query, C_QUERY)
GETSET(8fragment, C_FRAGMENT)
GETSET_PE(18percentEncodedUser, C_USER)
GETSET_PE(22percentEncodedPassword, C_PASSWORD)
GETSET_PE(18percentEncodedHost, C_HOST)
GETSET_PE(19percentEncodedQuery, C_QUERY)
GETSET_PE(22percentEncodedFragment, C_FRAGMENT)

/* path não é opcional */
SW fdn_str uc_get_path(void *const *self CTX) __asm__("$s10Foundation13URLComponentsV4pathSSvg");
SW fdn_str uc_get_path(void *const *self CTX) {
    char *d = fdn_pct_decode(((ucomp *)*self)->c[C_PATH]);
    fdn_str r = fdn_str_make(d ? d : "", -1);
    free(d);
    return r;
}
SW void uc_set_path(fdn_str v, ucomp **self CTX) __asm__("$s10Foundation13URLComponentsV4pathSSvs");
SW void uc_set_path(fdn_str v, ucomp **self CTX) { set_comp(self, C_PATH, v, 0); }
SW fdn_str uc_get_pe_path(void *const *self CTX) __asm__("$s10Foundation13URLComponentsV18percentEncodedPathSSvg");
SW fdn_str uc_get_pe_path(void *const *self CTX) { return fdn_str_make(((ucomp *)*self)->c[C_PATH], -1); }
SW void uc_set_pe_path(fdn_str v, ucomp **self CTX) __asm__("$s10Foundation13URLComponentsV18percentEncodedPathSSvs");
SW void uc_set_pe_path(fdn_str v, ucomp **self CTX) { set_comp(self, C_PATH, v, 1); }

/* port: Int? em (valor, tag; 1 = nil) */
typedef struct { long v; _Bool none; } opt_int;
SW opt_int uc_get_port(void *const *self CTX) __asm__("$s10Foundation13URLComponentsV4portSiSgvg");
SW opt_int uc_get_port(void *const *self CTX) {
    opt_int r;
    r.v = ((ucomp *)*self)->has_port ? ((ucomp *)*self)->port : 0;
    r.none = !((ucomp *)*self)->has_port;
    return r;
}
SW void uc_set_port(long v, _Bool none, ucomp **self CTX) __asm__("$s10Foundation13URLComponentsV4portSiSgvs");
SW void uc_set_port(long v, _Bool none, ucomp **self CTX) {
    ucomp *u = uc_own(self);
    u->has_port = !(none & 1);
    u->port = u->has_port ? v : 0;
}
