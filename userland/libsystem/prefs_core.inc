/* Núcleo das preferências (CFPreferences / NSUserDefaults), sem ObjC.
 * Um .inc para o libSystem (prefs.c) e para o teste de host (tools/prefstest.c).
 *
 * Funcional: dicionário de tipos de property list (bool, int, real, string, data,
 * array, dict), ordem de busca, domínio de registro que não vai para o disco,
 * gravação/leitura de plist XML, leitura de bplist00 e recusa de sobrescrever
 * um arquivo ilegível. O stamp é o hash do conteúdo, não o mtime.
 *
 * Não é um cfprefsd. Quem inclui este arquivo faz o I/O (prefs.c usa fopen/rename
 * do libSystem; o teste usa o POSIX do host).
 */
#ifdef PF_HOST
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#else
/* malloc, free, realloc, memcpy, memcmp, strlen, strcmp, strncmp, snprintf,
 * strtoll e strtod vêm de foundation.h / libc.h, incluídos antes deste arquivo. */
#endif

enum {
    PF_NONE = 0,
    PF_BOOL,
    PF_INT,
    PF_REAL,
    PF_STR,
    PF_DATA,
    PF_DATE,
    PF_ARRAY,
    PF_DICT
};

typedef struct pf_ent pf_ent;
typedef struct pf_val {
    int type;
    int b;
    long long i;
    double d;
    char *str;
    size_t n;
    pf_ent *ent;
} pf_val;

struct pf_ent {
    char *key;
    pf_val *v;
    pf_ent *next;
};

typedef struct pf_dom {
    char *scope;
    char *name;
    pf_val *root;
    int dirty;
    int loaded;
    int blocked; /* arquivo existe mas não é plist XML nosso: não sobrescrever */
    unsigned gen;
    unsigned long long stamp;
    struct pf_dom *next;
} pf_dom;

typedef struct pf_db {
    pf_dom *doms;
} pf_db;

typedef struct pf_fs {
    void *ctx;
    /* 0 = ok, -1 = não existe ou erro de leitura. *buf é malloc. */
    int (*read)(void *ctx, const char *path, char **buf, size_t *len, unsigned long long *stamp);
    /* 0 = ok. Substitui o arquivo inteiro (atômico, se o chamador fizer rename). */
    int (*write)(void *ctx, const char *path, const char *buf, size_t len);
    /* 0 = existe, -1 = não existe. Pode ser NULL: aí a carga compara lendo o arquivo. */
    int (*stamp)(void *ctx, const char *path, unsigned long long *stamp);
} pf_fs;

static pf_val *pf_new(int type) {
    pf_val *v = (pf_val *)calloc(1, sizeof *v);
    if (v) v->type = type;
    return v;
}

static pf_val *pf_bool(int b) {
    pf_val *v = pf_new(PF_BOOL);
    if (v) v->b = b ? 1 : 0;
    return v;
}
static pf_val *pf_int(long long i) {
    pf_val *v = pf_new(PF_INT);
    if (v) v->i = i;
    return v;
}
static pf_val *pf_real(double d) {
    pf_val *v = pf_new(PF_REAL);
    if (v) v->d = d;
    return v;
}
static pf_val *pf_str(const char *s) {
    pf_val *v = pf_new(PF_STR);
    if (!v) return 0;
    v->n = s ? strlen(s) : 0;
    v->str = (char *)malloc(v->n + 1);
    if (!v->str) { free(v); return 0; }
    if (v->n) memcpy(v->str, s, v->n);
    v->str[v->n] = 0;
    return v;
}
static pf_val *pf_data(const void *p, size_t n) {
    pf_val *v = pf_new(PF_DATA);
    if (!v) return 0;
    v->n = n;
    v->str = (char *)malloc(n + 1);
    if (!v->str) { free(v); return 0; }
    if (n && p) memcpy(v->str, p, n);
    v->str[n] = 0;
    return v;
}
static pf_val *pf_date(double unix_s) {
    pf_val *v = pf_new(PF_DATE);
    if (v) v->d = unix_s;
    return v;
}
static pf_val *pf_array(void) { return pf_new(PF_ARRAY); }
static pf_val *pf_dict(void) { return pf_new(PF_DICT); }

static void pf_free(pf_val *v) {
    if (!v) return;
    for (pf_ent *e = v->ent; e;) {
        pf_ent *n = e->next;
        free(e->key);
        pf_free(e->v);
        free(e);
        e = n;
    }
    free(v->str);
    free(v);
}

static pf_val *pf_copy(const pf_val *v) {
    if (!v) return 0;
    pf_val *o = pf_new(v->type);
    if (!o) return 0;
    o->b = v->b;
    o->i = v->i;
    o->d = v->d;
    o->n = v->n;
    if (v->str) {
        o->str = (char *)malloc(v->n + 1);
        if (!o->str) { free(o); return 0; }
        if (v->n) memcpy(o->str, v->str, v->n);
        o->str[v->n] = 0;
    }
    for (pf_ent *e = v->ent; e; e = e->next) {
        pf_ent *n = (pf_ent *)calloc(1, sizeof *n);
        pf_val *c = pf_copy(e->v);
        if (!n || !c) { free(n); pf_free(c); pf_free(o); return 0; }
        if (e->key) {
            n->key = strdup(e->key);
            if (!n->key) { pf_free(c); free(n); pf_free(o); return 0; }
        }
        n->v = c;
        n->next = o->ent;
        o->ent = n;
    }
    /* a cópia inverteu a lista; desinverte para a ordem de inserção ficar estável */
    pf_ent *prev = 0, *cur = o->ent;
    while (cur) {
        pf_ent *n = cur->next;
        cur->next = prev;
        prev = cur;
        cur = n;
    }
    o->ent = prev;
    return o;
}

static int pf_equal(const pf_val *a, const pf_val *b) {
    if (a == b) return 1;
    if (!a || !b || a->type != b->type) return 0;
    switch (a->type) {
    case PF_BOOL: return a->b == b->b;
    case PF_INT: return a->i == b->i;
    case PF_REAL: return a->d == b->d;
    case PF_DATE: return a->d == b->d;
    case PF_STR:
    case PF_DATA:
        return a->n == b->n && (!a->n || !memcmp(a->str, b->str, a->n));
    case PF_ARRAY:
    case PF_DICT: {
        const pf_ent *x = a->ent, *y = b->ent;
        for (; x && y; x = x->next, y = y->next) {
            if ((x->key == 0) != (y->key == 0)) return 0;
            if (x->key && strcmp(x->key, y->key) != 0) return 0;
            if (!pf_equal(x->v, y->v)) return 0;
        }
        return !x && !y;
    }
    default: return 0;
    }
}

static void pf_dict_put(pf_val *d, const char *key, pf_val *v) {
    if (!d || d->type != PF_DICT || !key) { pf_free(v); return; }
    pf_ent **pp = &d->ent;
    for (; *pp; pp = &(*pp)->next) {
        if ((*pp)->key && strcmp((*pp)->key, key) == 0) {
            if (!v) {
                pf_ent *e = *pp;
                *pp = e->next;
                free(e->key);
                pf_free(e->v);
                free(e);
            } else {
                pf_free((*pp)->v);
                (*pp)->v = v;
            }
            return;
        }
    }
    if (!v) return;
    pf_ent *e = (pf_ent *)calloc(1, sizeof *e);
    if (!e) { pf_free(v); return; }
    e->key = strdup(key);
    e->v = v;
    if (!e->key) { pf_free(v); free(e); return; }
    *pp = e;
}

static void pf_array_add(pf_val *a, pf_val *v) {
    if (!a || a->type != PF_ARRAY || !v) { pf_free(v); return; }
    pf_ent **pp = &a->ent;
    while (*pp) pp = &(*pp)->next;
    pf_ent *e = (pf_ent *)calloc(1, sizeof *e);
    if (!e) { pf_free(v); return; }
    e->v = v;
    *pp = e;
}

static pf_val *pf_dict_get(const pf_val *d, const char *key) {
    if (!d || d->type != PF_DICT || !key) return 0;
    for (pf_ent *e = d->ent; e; e = e->next)
        if (e->key && strcmp(e->key, key) == 0) return e->v;
    return 0;
}

/* nome de arquivo: identificador de bundle, ou exatamente .GlobalPreferences.
 * Recusa vazio, barra e "..", para o caminho não sair de Library/Preferences. */
static int pf_domain_ok(const char *name) {
    if (!name || !name[0] || strlen(name) > 200) return 0;
    if (strcmp(name, ".GlobalPreferences") == 0) return 1;
    if (name[0] == '.') return 0;
    if (strstr(name, "..")) return 0;
    for (const char *p = name; *p; p++) {
        char c = *p;
        int ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-' || c == '+';
        if (!ok) return 0;
    }
    return 1;
}

static pf_db *pf_db_new(void) {
    return (pf_db *)calloc(1, sizeof(pf_db));
}

static void pf_db_free(pf_db *db) {
    if (!db) return;
    for (pf_dom *d = db->doms; d;) {
        pf_dom *n = d->next;
        free(d->scope);
        free(d->name);
        pf_free(d->root);
        free(d);
        d = n;
    }
    free(db);
}

static pf_dom *pf_dom_find(pf_db *db, const char *scope, const char *name, int create) {
    if (!db || !scope || !name) return 0;
    for (pf_dom *d = db->doms; d; d = d->next)
        if (strcmp(d->scope, scope) == 0 && strcmp(d->name, name) == 0) return d;
    if (!create) return 0;
    if ((strcmp(scope, "reg") != 0) && !pf_domain_ok(name)) return 0;
    pf_dom *d = (pf_dom *)calloc(1, sizeof *d);
    if (!d) return 0;
    d->scope = strdup(scope);
    d->name = strdup(name);
    d->root = pf_dict();
    if (!d->scope || !d->name || !d->root) {
        free(d->scope);
        free(d->name);
        pf_free(d->root);
        free(d);
        return 0;
    }
    d->next = db->doms;
    db->doms = d;
    return d;
}

static unsigned pf_gen(pf_db *db, const char *scope, const char *name) {
    pf_dom *d = pf_dom_find(db, scope, name, 0);
    return d ? d->gen : 0;
}

/* 1 = mudou, 0 = igual, -1 = domínio rejeitado */
static int pf_put(pf_db *db, const char *scope, const char *name, const char *key, pf_val *v) {
    pf_dom *d = pf_dom_find(db, scope, name, 1);
    if (!d || !key) { pf_free(v); return -1; }
    pf_val *old = pf_dict_get(d->root, key);
    if (!v && !old) return 0;
    if (v && old && pf_equal(old, v)) { pf_free(v); return 0; }
    pf_dict_put(d->root, key, v);
    d->dirty = 1;
    d->loaded = 1;
    d->gen++;
    return 1;
}

static int pf_replace(pf_db *db, const char *scope, const char *name, pf_val *dict) {
    pf_dom *d = pf_dom_find(db, scope, name, 1);
    if (!d || !dict || dict->type != PF_DICT) { pf_free(dict); return -1; }
    if (pf_equal(d->root, dict)) { pf_free(dict); return 0; }
    pf_free(d->root);
    d->root = dict;
    d->dirty = 1;
    d->loaded = 1;
    d->gen++;
    return 1;
}

static void pf_clear(pf_db *db, const char *scope, const char *name) {
    pf_dom *d = pf_dom_find(db, scope, name, 0);
    if (!d) return;
    if (d->root && !d->root->ent) return;
    pf_free(d->root);
    d->root = pf_dict();
    d->dirty = 1;
    d->gen++;
}

static pf_val *pf_get_mem(pf_db *db, const char *scope, const char *name, const char *key) {
    pf_dom *d = pf_dom_find(db, scope, name, 0);
    if (!d) return 0;
    return pf_dict_get(d->root, key);
}

/* primeira ocorrência na lista scope/name (pares). n é o número de pares. */
static pf_val *pf_search(pf_db *db, const char **scope, const char **name, int n, const char *key, int *which) {
    if (which) *which = -1;
    for (int i = 0; i < n; i++) {
        pf_val *v = pf_get_mem(db, scope[i], name[i], key);
        if (v) {
            if (which) *which = i;
            return v;
        }
    }
    return 0;
}

/* ---------------- plist XML (subconjunto Apple: dict/array/string/integer/real/true/false/data/date) ---------------- */

static int pf_b64val(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static char *pf_b64enc(const unsigned char *p, size_t n, size_t *outn) {
    static const char *t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t o = 4 * ((n + 2) / 3);
    char *s = (char *)malloc(o + 1);
    if (!s) return 0;
    size_t j = 0;
    for (size_t i = 0; i < n; i += 3) {
        unsigned a = p[i];
        unsigned b = i + 1 < n ? p[i + 1] : 0;
        unsigned c = i + 2 < n ? p[i + 2] : 0;
        unsigned v = (a << 16) | (b << 8) | c;
        s[j++] = t[(v >> 18) & 63];
        s[j++] = t[(v >> 12) & 63];
        s[j++] = (i + 1 < n) ? t[(v >> 6) & 63] : '=';
        s[j++] = (i + 2 < n) ? t[v & 63] : '=';
    }
    s[j] = 0;
    if (outn) *outn = j;
    return s;
}

typedef struct { const char *p, *e; int err; } pf_x;

static void pf_skip(pf_x *x) {
    for (;;) {
        while (x->p < x->e && (*x->p == ' ' || *x->p == '\t' || *x->p == '\n' || *x->p == '\r')) x->p++;
        if (x->e - x->p >= 4 && strncmp(x->p, "<!--", 4) == 0) {
            const char *q = x->p;
            while (q + 2 < x->e && !(q[0] == '-' && q[1] == '-' && q[2] == '>')) q++;
            x->p = q + 2 < x->e ? q + 3 : x->e;
            continue;
        }
        if (x->e - x->p >= 2 && x->p[0] == '<' && (x->p[1] == '?' || x->p[1] == '!')) {
            while (x->p < x->e && *x->p != '>') x->p++;
            if (x->p < x->e) x->p++;
            continue;
        }
        return;
    }
}

/* 1 = abre, 2 = vazia, 0 = erro ou fecha. nome em tag. */
static int pf_open(pf_x *x, char *tag, int cap) {
    pf_skip(x);
    if (x->p >= x->e || *x->p != '<' || x->p[1] == '/') return 0;
    x->p++;
    int n = 0;
    while (x->p < x->e && *x->p != '>' && *x->p != ' ' && *x->p != '/' && *x->p != '\n' && n < cap - 1)
        tag[n++] = *x->p++;
    tag[n] = 0;
    while (x->p < x->e && *x->p != '>' && *x->p != '/') x->p++;
    if (x->p < x->e && *x->p == '/') {
        while (x->p < x->e && *x->p != '>') x->p++;
        if (x->p < x->e) x->p++;
        return 2;
    }
    if (x->p < x->e) x->p++;
    return 1;
}

static void pf_close(pf_x *x) {
    pf_skip(x);
    if (x->p + 1 < x->e && x->p[0] == '<' && x->p[1] == '/') {
        while (x->p < x->e && *x->p != '>') x->p++;
        if (x->p < x->e) x->p++;
    }
}

static char *pf_text(pf_x *x, size_t *n) {
    const char *s = x->p;
    while (x->p < x->e && *x->p != '<') x->p++;
    size_t raw = (size_t)(x->p - s);
    char *b = (char *)malloc(raw + 1);
    if (!b) { x->err = 1; return 0; }
    size_t o = 0;
    for (size_t i = 0; i < raw;) {
        if (s[i] == '&') {
            if (i + 4 <= raw && strncmp(s + i, "&lt;", 4) == 0) { b[o++] = '<'; i += 4; continue; }
            if (i + 4 <= raw && strncmp(s + i, "&gt;", 4) == 0) { b[o++] = '>'; i += 4; continue; }
            if (i + 5 <= raw && strncmp(s + i, "&amp;", 5) == 0) { b[o++] = '&'; i += 5; continue; }
            if (i + 6 <= raw && strncmp(s + i, "&quot;", 6) == 0) { b[o++] = '"'; i += 6; continue; }
            if (i + 6 <= raw && strncmp(s + i, "&apos;", 6) == 0) { b[o++] = '\''; i += 6; continue; }
            if (i + 2 <= raw && s[i + 1] == '#') {
                unsigned long cp = 0;
                size_t j = i + 2;
                int hex = 0;
                if (j < raw && (s[j] == 'x' || s[j] == 'X')) { hex = 1; j++; }
                while (j < raw && s[j] != ';') {
                    char c = s[j++];
                    if (hex) {
                        int h = (c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
                        if (h < 0) break;
                        cp = (cp << 4) + (unsigned)h;
                    } else {
                        if (c < '0' || c > '9') break;
                        cp = cp * 10 + (unsigned)(c - '0');
                    }
                }
                if (j < raw && s[j] == ';') {
                    if (cp < 0x80) b[o++] = (char)cp;
                    else if (cp < 0x800) { b[o++] = (char)(0xC0 | (cp >> 6)); b[o++] = (char)(0x80 | (cp & 0x3F)); }
                    else if (cp < 0x10000) {
                        b[o++] = (char)(0xE0 | (cp >> 12));
                        b[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                        b[o++] = (char)(0x80 | (cp & 0x3F));
                    }
                    i = j + 1;
                    continue;
                }
            }
        }
        b[o++] = s[i++];
    }
    b[o] = 0;
    if (n) *n = o;
    return b;
}

static int pf_parse_int_n(const char *s, int n, int *out) {
    int v = 0;
    if (!s || n <= 0) return 0;
    for (int i = 0; i < n; i++) {
        if (s[i] < '0' || s[i] > '9') return 0;
        v = v * 10 + (s[i] - '0');
    }
    *out = v;
    return 1;
}

/* Dias civis no calendário gregoriano (Howard Hinnant). 1970-01-01 = 0. */
static long long pf_days_from_civil(int y, int m, int d) {
    y -= m <= 2;
    int era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + (unsigned)d - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return (long long)era * 146097 + (long long)doe - 719468;
}

/* ISO-8601 (YYYY-MM-DD ou YYYY-MM-DDTHH:MM:SSZ) vira segundos desde 1970, UTC.
 * Texto numérico (o que este núcleo grava) cai no strtod. */
static double pf_date_unix(const char *s) {
    int y = 0, mo = 0, d = 0, h = 0, mi = 0, se = 0;
    size_t n = s ? strlen(s) : 0;
    if (n >= 10 && s[4] == '-' && s[7] == '-' &&
        pf_parse_int_n(s, 4, &y) && pf_parse_int_n(s + 5, 2, &mo) && pf_parse_int_n(s + 8, 2, &d)) {
        if (n >= 19 && s[10] == 'T' && s[13] == ':' && s[16] == ':') {
            pf_parse_int_n(s + 11, 2, &h);
            pf_parse_int_n(s + 14, 2, &mi);
            pf_parse_int_n(s + 17, 2, &se);
        }
        if (mo >= 1 && mo <= 12 && d >= 1 && d <= 31 && h >= 0 && h <= 23 && mi >= 0 && mi <= 59 && se >= 0 && se <= 60)
            return (double)pf_days_from_civil(y, mo, d) * 86400.0 + (double)(h * 3600 + mi * 60 + se);
    }
    return strtod(s ? s : "", 0);
}

static pf_val *pf_parse_val(pf_x *x, int depth);

static pf_val *pf_parse_val(pf_x *x, int depth) {
    char tag[32];
    if (depth > 32) { x->err = 1; return 0; }
    int k = pf_open(x, tag, sizeof tag);
    if (!k) { x->err = 1; return 0; }
    if (strcmp(tag, "true") == 0) return pf_bool(1);
    if (strcmp(tag, "false") == 0) return pf_bool(0);
    if (strcmp(tag, "dict") == 0) {
        pf_val *d = pf_dict();
        if (!d) { x->err = 1; return 0; }
        if (k == 2) return d;
        for (;;) {
            pf_skip(x);
            if (x->p + 1 < x->e && x->p[0] == '<' && x->p[1] == '/') break;
            char kt[32];
            int kk = pf_open(x, kt, sizeof kt);
            if (kk != 1 || strcmp(kt, "key") != 0) { x->err = 1; pf_free(d); return 0; }
            size_t kn = 0;
            char *ks = pf_text(x, &kn);
            pf_close(x);
            pf_val *v = pf_parse_val(x, depth + 1);
            if (!ks || !v || x->err) { free(ks); pf_free(v); pf_free(d); x->err = 1; return 0; }
            pf_dict_put(d, ks, v);
            free(ks);
        }
        pf_close(x);
        return d;
    }
    if (strcmp(tag, "array") == 0) {
        pf_val *a = pf_array();
        if (!a) { x->err = 1; return 0; }
        if (k == 2) return a;
        for (;;) {
            pf_skip(x);
            if (x->p + 1 < x->e && x->p[0] == '<' && x->p[1] == '/') break;
            pf_val *v = pf_parse_val(x, depth + 1);
            if (!v || x->err) { pf_free(v); pf_free(a); x->err = 1; return 0; }
            pf_array_add(a, v);
        }
        pf_close(x);
        return a;
    }
    if (k == 2) {
        if (strcmp(tag, "string") == 0) return pf_str("");
        if (strcmp(tag, "data") == 0) return pf_data("", 0);
        x->err = 1;
        return 0;
    }
    size_t tn = 0;
    char *tx = pf_text(x, &tn);
    pf_close(x);
    if (!tx) { x->err = 1; return 0; }
    pf_val *v = 0;
    if (strcmp(tag, "string") == 0) v = pf_str(tx);
    else if (strcmp(tag, "integer") == 0) v = pf_int(strtoll(tx, 0, 10));
    else if (strcmp(tag, "real") == 0) v = pf_real(strtod(tx, 0));
    else if (strcmp(tag, "date") == 0) {
        /* YYYY-MM-DDTHH:MM:SSZ interpretado como segundos desde 1970, UTC, sem fuso além de Z.
         * O bridge guarda a data como double; aqui só precisa sobreviver o texto se não for essa forma.
         * Guardamos o texto em str e também tentamos um double em d quando o texto é numérico. */
        v = pf_new(PF_DATE);
        if (v) {
            v->n = tn;
            v->str = tx;
            tx = 0;
            v->d = pf_date_unix(v->str);
        }
    } else if (strcmp(tag, "data") == 0) {
        unsigned char *bin = (unsigned char *)malloc(tn + 1);
        size_t o = 0;
        unsigned acc = 0;
        int bits = 0;
        if (!bin) x->err = 1;
        else {
            for (size_t i = 0; i < tn; i++) {
                int c = pf_b64val(tx[i]);
                if (c < 0) continue;
                acc = (acc << 6) | (unsigned)c;
                bits += 6;
                if (bits >= 8) {
                    bits -= 8;
                    bin[o++] = (unsigned char)((acc >> bits) & 0xFF);
                }
            }
            v = pf_data(bin, o);
            free(bin);
        }
    } else x->err = 1;
    free(tx);
    if (x->err) { pf_free(v); return 0; }
    return v;
}

/* FNV-1a de 64 bits, misturado com o tamanho. 0 vira 1 para não colidir com "arquivo ausente". */
static unsigned long long pf_fnv(const void *p, size_t n) {
    const unsigned char *b = (const unsigned char *)p;
    unsigned long long h = 14695981039346656037ull;
    for (size_t i = 0; i < n; i++) {
        h ^= b[i];
        h *= 1099511628211ull;
    }
    h ^= (unsigned long long)n + 1ull;
    return h ? h : 1ull;
}

/* scope: user (ou NULL) = ~/Library/Preferences, host = ByHost do usuário,
 * any = /Library/Preferences, any.host = /Library/Preferences/ByHost. */
static int pf_pref_path(const char *home, const char *token, const char *scope, const char *name, char *out, size_t cap) {
    if (!name || !name[0] || !out || cap < 8) return -1;
    if (!token || !token[0]) token = "mos";
    if (!home || !home[0]) home = "/Users/Visitante";
    int k;
    if (scope && strcmp(scope, "any") == 0)
        k = snprintf(out, cap, "/Library/Preferences/%s.plist", name);
    else if (scope && strcmp(scope, "any.host") == 0)
        k = snprintf(out, cap, "/Library/Preferences/ByHost/%s.%s.plist", name, token);
    else if (scope && strcmp(scope, "host") == 0)
        k = snprintf(out, cap, "%s/Library/Preferences/ByHost/%s.%s.plist", home, name, token);
    else
        k = snprintf(out, cap, "%s/Library/Preferences/%s.plist", home, name);
    return (k > 0 && (size_t)k < cap) ? 0 : -1;
}

static unsigned long long pf_be(const unsigned char *p, int n) {
    unsigned long long v = 0;
    for (int i = 0; i < n && i < 8; i++) v = (v << 8) | p[i];
    return v;
}

typedef struct {
    const unsigned char *b;
    size_t n;
    int osz, rsz;
    unsigned long long nobj, top, otab;
} pf_bp;

static pf_val *pf_bp_obj(pf_bp *bp, unsigned long long idx, int depth);

static int pf_bp_len(pf_bp *bp, const unsigned char **p, unsigned long long *len) {
    if (*p >= bp->b + bp->n) return 0;
    unsigned m = **p & 0x0Fu;
    (*p)++;
    if (m != 0x0Fu) { *len = m; return 1; }
    if (*p >= bp->b + bp->n) return 0;
    unsigned t = **p;
    if ((t & 0xF0u) != 0x10u) return 0;
    int nb = 1 << (t & 0x0Fu);
    if (nb < 1 || nb > 8 || *p + 1 + nb > bp->b + bp->n) return 0;
    *len = pf_be(*p + 1, nb);
    *p += 1 + nb;
    return 1;
}

static pf_val *pf_from_utf16(const unsigned char *p, unsigned long long nchars) {
    char *o = (char *)malloc((size_t)nchars * 3 + 1);
    if (!o) return 0;
    size_t k = 0;
    for (unsigned long long i = 0; i < nchars; i++) {
        unsigned c = (unsigned)pf_be(p + (size_t)i * 2, 2);
        if (c >= 0xD800 && c < 0xDC00 && i + 1 < nchars) {
            unsigned d = (unsigned)pf_be(p + (size_t)(i + 1) * 2, 2);
            if (d >= 0xDC00 && d < 0xE000) {
                c = 0x10000u + ((c - 0xD800u) << 10) + (d - 0xDC00u);
                i++;
            }
        }
        if (c < 0x80) o[k++] = (char)c;
        else if (c < 0x800) { o[k++] = (char)(0xC0 | (c >> 6)); o[k++] = (char)(0x80 | (c & 63)); }
        else if (c < 0x10000) {
            o[k++] = (char)(0xE0 | (c >> 12));
            o[k++] = (char)(0x80 | ((c >> 6) & 63));
            o[k++] = (char)(0x80 | (c & 63));
        } else {
            o[k++] = (char)(0xF0 | (c >> 18));
            o[k++] = (char)(0x80 | ((c >> 12) & 63));
            o[k++] = (char)(0x80 | ((c >> 6) & 63));
            o[k++] = (char)(0x80 | (c & 63));
        }
    }
    o[k] = 0;
    pf_val *v = pf_str(o);
    free(o);
    return v;
}

static pf_val *pf_bp_obj(pf_bp *bp, unsigned long long idx, int depth) {
    if (!bp || idx >= bp->nobj || depth > 32) return 0;
    if (bp->osz < 1 || bp->osz > 8) return 0;
    if (bp->otab + (idx + 1) * (unsigned long long)bp->osz > bp->n) return 0;
    unsigned long long off = pf_be(bp->b + bp->otab + (size_t)idx * (size_t)bp->osz, bp->osz);
    if (off >= bp->n) return 0;
    const unsigned char *p = bp->b + off;
    const unsigned char *end = bp->b + bp->n;
    unsigned hi = *p >> 4;
    switch (hi) {
    case 0x0:
        if ((*p & 0x0F) == 9) return pf_bool(1);
        if ((*p & 0x0F) == 8) return pf_bool(0);
        return 0;
    case 0x1: {
        int nb = 1 << (*p & 0x0F);
        if (nb < 1 || nb > 8 || p + 1 + nb > end) return 0;
        unsigned long long v = pf_be(p + 1, nb);
        if (nb < 8) {
            unsigned bits = (unsigned)nb * 8u;
            if (v & (1ull << (bits - 1))) v |= (~0ull) << bits;
        }
        return pf_int((long long)v);
    }
    case 0x2: {
        int nb = 1 << (*p & 0x0F);
        if ((nb != 4 && nb != 8) || p + 1 + nb > end) return 0;
        unsigned long long u = pf_be(p + 1, nb);
        double d;
        if (nb == 4) {
            union { unsigned u; float f; } x;
            x.u = (unsigned)u;
            d = (double)x.f;
        } else {
            union { unsigned long long u; double d; } x;
            x.u = u;
            d = x.d;
        }
        return pf_real(d);
    }
    case 0x3: {
        if ((*p & 0x0F) != 3 || p + 9 > end) return 0;
        union { unsigned long long u; double d; } x;
        x.u = pf_be(p + 1, 8);
        return pf_date(x.d + 978307200.0);
    }
    case 0x4: {
        const unsigned char *q = p;
        unsigned long long len = 0;
        if (!pf_bp_len(bp, &q, &len) || q + len > end) return 0;
        return pf_data(q, (size_t)len);
    }
    case 0x5: {
        const unsigned char *q = p;
        unsigned long long len = 0;
        if (!pf_bp_len(bp, &q, &len) || q + len > end) return 0;
        char *s = (char *)malloc((size_t)len + 1);
        if (!s) return 0;
        if (len) memcpy(s, q, (size_t)len);
        s[len] = 0;
        pf_val *v = pf_str(s);
        free(s);
        return v;
    }
    case 0x6: {
        const unsigned char *q = p;
        unsigned long long len = 0;
        if (!pf_bp_len(bp, &q, &len) || q + len * 2 > end) return 0;
        return pf_from_utf16(q, len);
    }
    case 0xA:
    case 0xC: {
        const unsigned char *q = p;
        unsigned long long len = 0;
        if (!pf_bp_len(bp, &q, &len)) return 0;
        if (len > 100000 || q + len * (unsigned long long)bp->rsz > end) return 0;
        pf_val *a = pf_array();
        if (!a) return 0;
        for (unsigned long long i = 0; i < len; i++) {
            pf_val *v = pf_bp_obj(bp, pf_be(q + (size_t)i * (size_t)bp->rsz, bp->rsz), depth + 1);
            if (!v) { pf_free(a); return 0; }
            pf_array_add(a, v);
        }
        return a;
    }
    case 0xD: {
        const unsigned char *q = p;
        unsigned long long len = 0;
        if (!pf_bp_len(bp, &q, &len)) return 0;
        if (len > 100000 || q + len * 2 * (unsigned long long)bp->rsz > end) return 0;
        pf_val *d = pf_dict();
        if (!d) return 0;
        for (unsigned long long i = 0; i < len; i++) {
            unsigned long long ki = pf_be(q + (size_t)i * (size_t)bp->rsz, bp->rsz);
            unsigned long long vi = pf_be(q + (size_t)(len + i) * (size_t)bp->rsz, bp->rsz);
            pf_val *k = pf_bp_obj(bp, ki, depth + 1);
            pf_val *v = pf_bp_obj(bp, vi, depth + 1);
            if (!k || k->type != PF_STR || !k->str || !v) {
                pf_free(k);
                pf_free(v);
                pf_free(d);
                return 0;
            }
            pf_dict_put(d, k->str, v);
            pf_free(k);
        }
        return d;
    }
    default:
        return 0;
    }
}

/* Raiz tem de ser dicionário. Arquivo com mágica e trailer inválido devolve 0. */
static pf_val *pf_bplist_parse(const unsigned char *b, size_t n) {
    if (!b || n < 40 || memcmp(b, "bplist00", 8) != 0) return 0;
    const unsigned char *t = b + n - 32;
    int osz = t[6], rsz = t[7];
    unsigned long long nobj = pf_be(t + 8, 8);
    unsigned long long top = pf_be(t + 16, 8);
    unsigned long long otab = pf_be(t + 24, 8);
    if (osz < 1 || osz > 8 || rsz < 1 || rsz > 8 || otab >= n || top >= nobj) return 0;
    if (nobj > 1000000 || otab + nobj * (unsigned long long)osz > n) return 0;
    pf_bp bp = {b, n, osz, rsz, nobj, top, otab};
    pf_val *v = pf_bp_obj(&bp, top, 0);
    if (!v || v->type != PF_DICT) { pf_free(v); return 0; }
    return v;
}

/* 0 = ok. bplist00 válido vira dicionário; mágica sem trailer válido fica err_kind 2.
 * *err_kind: 1 sintaxe, 2 binário ilegível. */
static pf_val *pf_xml_parse(const char *buf, size_t len, int *err_kind) {
    if (err_kind) *err_kind = 0;
    if (!buf || !len) { if (err_kind) *err_kind = 1; return 0; }
    if (len >= 8 && memcmp(buf, "bplist00", 8) == 0) {
        pf_val *bin = pf_bplist_parse((const unsigned char *)buf, len);
        if (!bin) { if (err_kind) *err_kind = 2; return 0; }
        return bin;
    }
    pf_x x = {buf, buf + len, 0};
    char tag[32];
    pf_skip(&x);
    const char *save = x.p;
    int k = pf_open(&x, tag, sizeof tag);
    pf_val *v = 0;
    if (k && strcmp(tag, "plist") == 0) v = pf_parse_val(&x, 0);
    else {
        x.p = save;
        x.err = 0;
        v = pf_parse_val(&x, 0);
    }
    if (x.err || !v || v->type != PF_DICT) {
        pf_free(v);
        if (err_kind && !*err_kind) *err_kind = 1;
        return 0;
    }
    return v;
}

typedef struct { char *b; size_t n, cap; } pf_buf;

static void pf_add(pf_buf *o, const char *s, size_t n) {
    if (o->n + n + 1 > o->cap) {
        size_t c = o->cap ? o->cap * 2 : 256;
        while (c < o->n + n + 1) c *= 2;
        char *nb = (char *)realloc(o->b, c);
        if (!nb) return;
        o->b = nb;
        o->cap = c;
    }
    if (n) memcpy(o->b + o->n, s, n);
    o->n += n;
    o->b[o->n] = 0;
}
static void pf_adds(pf_buf *o, const char *s) { pf_add(o, s, strlen(s)); }

static void pf_esc(pf_buf *o, const char *s, size_t n) {
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '<') pf_adds(o, "&lt;");
        else if (c == '>') pf_adds(o, "&gt;");
        else if (c == '&') pf_adds(o, "&amp;");
        else if (c < 0x20 && c != '\t' && c != '\n' && c != '\r') {
            char t[8];
            int k = snprintf(t, sizeof t, "&#%u;", c);
            if (k > 0) pf_add(o, t, (size_t)k);
        } else {
            char ch = (char)c;
            pf_add(o, &ch, 1);
        }
    }
}

static void pf_write_val(pf_buf *o, const pf_val *v) {
    char num[64];
    if (!v) { pf_adds(o, "<string></string>"); return; }
    switch (v->type) {
    case PF_BOOL: pf_adds(o, v->b ? "<true/>" : "<false/>"); return;
    case PF_INT: {
        int k = snprintf(num, sizeof num, "<integer>%lld</integer>", v->i);
        if (k > 0) pf_add(o, num, (size_t)k);
        return;
    }
    case PF_REAL: {
        int k = snprintf(num, sizeof num, "<real>%.17g</real>", v->d);
        if (k > 0) pf_add(o, num, (size_t)k);
        return;
    }
    case PF_DATE: {
        /* o bridge grava o double em texto; se houver str (lido de um plist alheio), preserva */
        pf_adds(o, "<date>");
        if (v->str) pf_esc(o, v->str, v->n);
        else {
            int k = snprintf(num, sizeof num, "%.17g", v->d);
            if (k > 0) pf_add(o, num, (size_t)k);
        }
        pf_adds(o, "</date>");
        return;
    }
    case PF_STR:
        pf_adds(o, "<string>");
        pf_esc(o, v->str ? v->str : "", v->n);
        pf_adds(o, "</string>");
        return;
    case PF_DATA: {
        size_t bn = 0;
        char *b = pf_b64enc((const unsigned char *)(v->str ? v->str : ""), v->n, &bn);
        pf_adds(o, "<data>");
        if (b) pf_add(o, b, bn);
        pf_adds(o, "</data>");
        free(b);
        return;
    }
    case PF_ARRAY:
        pf_adds(o, "<array>");
        for (pf_ent *e = v->ent; e; e = e->next) pf_write_val(o, e->v);
        pf_adds(o, "</array>");
        return;
    case PF_DICT:
        pf_adds(o, "<dict>");
        for (pf_ent *e = v->ent; e; e = e->next) {
            pf_adds(o, "<key>");
            pf_esc(o, e->key ? e->key : "", e->key ? strlen(e->key) : 0);
            pf_adds(o, "</key>");
            pf_write_val(o, e->v);
        }
        pf_adds(o, "</dict>");
        return;
    default:
        pf_adds(o, "<string></string>");
        return;
    }
}

static char *pf_xml_write(const pf_val *dict, size_t *len) {
    pf_buf o = {0, 0, 0};
    pf_adds(&o, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    pf_adds(&o, "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n");
    pf_adds(&o, "<plist version=\"1.0\">");
    pf_write_val(&o, dict && dict->type == PF_DICT ? dict : 0);
    pf_adds(&o, "</plist>\n");
    if (len) *len = o.n;
    return o.b;
}

/* Carrega o arquivo em path para o domínio. force ignora o stamp.
 * Retorno: 0 ok (inclusive arquivo ausente = domínio vazio), -1 erro de I/O,
 * -2 arquivo ilegível (blocked: memória anterior conservada, disco intocado).
 * Se o domínio está dirty, não substitui a memória. */
static int pf_load(pf_db *db, const char *scope, const char *name, const char *path, const pf_fs *fs, int force) {
    pf_dom *d = pf_dom_find(db, scope, name, 1);
    if (!d) return -1;
    if (!path || !fs || !fs->read) { d->loaded = 1; return 0; }
    if (d->dirty) return 0;
    if (!force && d->loaded && !d->blocked && fs->stamp) {
        unsigned long long st = 0;
        int sr = fs->stamp(fs->ctx, path, &st);
        if (sr != 0 && d->loaded && !d->root->ent) return 0; /* continua ausente */
        if (sr == 0 && st == d->stamp) return 0;
    }
    char *buf = 0;
    size_t n = 0;
    unsigned long long st = 0;
    int rr = fs->read(fs->ctx, path, &buf, &n, &st);
    if (rr != 0) {
        /* ausente: domínio vazio. Não mexe se já estava vazio (evita invalidar o cache à toa). */
        if (!d->loaded) {
            d->loaded = 1;
            d->blocked = 0;
            d->stamp = 0;
        } else if (!d->blocked && d->root && d->root->ent) {
            pf_free(d->root);
            d->root = pf_dict();
            d->gen++;
            d->stamp = 0;
        }
        free(buf);
        return 0;
    }
    int kind = 0;
    pf_val *parsed = (n == 0) ? pf_dict() : pf_xml_parse(buf, n, &kind);
    if (!parsed) {
        d->blocked = 1;
        d->loaded = 1;
        free(buf);
        return -2;
    }
    pf_free(d->root);
    d->root = parsed;
    d->blocked = 0;
    d->loaded = 1;
    d->dirty = 0;
    d->stamp = st;
    d->gen++;
    free(buf);
    return 0;
}

/* 0 gravou ou não havia mudança, -1 I/O, -2 bloqueado (não sobrescreve). */
static int pf_save(pf_db *db, const char *scope, const char *name, const char *path, const pf_fs *fs) {
    pf_dom *d = pf_dom_find(db, scope, name, 0);
    if (!d || !d->dirty) return 0;
    if (d->blocked) return -2;
    if (!path || !fs || !fs->write) return -1;
    size_t n = 0;
    char *xml = pf_xml_write(d->root, &n);
    if (!xml) return -1;
    int wr = fs->write(fs->ctx, path, xml, n);
    if (wr != 0) { free(xml); return -1; }
    d->dirty = 0;
    if (fs->stamp) {
        unsigned long long st = 0;
        if (fs->stamp(fs->ctx, path, &st) == 0) d->stamp = st;
    }
    free(xml);
    return 0;
}

static int pf_is_dirty(pf_db *db, const char *scope, const char *name) {
    pf_dom *d = pf_dom_find(db, scope, name, 0);
    return d && d->dirty;
}
static int pf_is_blocked(pf_db *db, const char *scope, const char *name) {
    pf_dom *d = pf_dom_find(db, scope, name, 0);
    return d && d->blocked;
}
