/* mOS libSystem — predicado de consulta do Spotlight (ver mdq_core.h). */
#include "mdq_core.h"

void *malloc(unsigned long n);
void *calloc(unsigned long n, unsigned long s);
void free(void *p);

enum { N_AND = 1, N_OR, N_NOT, N_CMP, N_TEXT, N_CONST };
enum { OP_EQ = 1, OP_NE, OP_LT, OP_GT, OP_LE, OP_GE };

struct mdq_node {
    int kind, op, nocase, nodiac, word, num_ok, konst;
    double num;
    char *attr, *val;
    mdq_node *a, *b;
};

typedef struct { const char *p; char *err; int errcap; int bad; } ps_t;

static char *sdup(const char *s, int n) {
    char *r = malloc((unsigned long)n + 1);
    if (!r) return 0;
    for (int i = 0; i < n; i++) r[i] = s[i];
    r[n] = 0;
    return r;
}
static void fail(ps_t *s, const char *m) {
    if (s->bad) return;
    s->bad = 1;
    if (s->err && s->errcap > 0) {
        int i = 0;
        for (; m[i] && i + 1 < s->errcap; i++) s->err[i] = m[i];
        s->err[i] = 0;
    }
}
static void ws(ps_t *s) { while (*s->p == ' ' || *s->p == '\t' || *s->p == '\n' || *s->p == '\r') s->p++; }
static int isid(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == ':'; }

void mdq_free(mdq_node *n) {
    if (!n) return;
    mdq_free(n->a);
    mdq_free(n->b);
    free(n->attr);
    free(n->val);
    free(n);
}

static mdq_node *node(int kind) { mdq_node *n = calloc(1, sizeof *n); if (n) n->kind = kind; return n; }

/* "..." ou '...' com \ ; depois os modificadores [cdw] colados */
static char *quoted(ps_t *s, mdq_node *n) {
    char q = *s->p++;
    const char *st = s->p;
    int cap = 0;
    while (s->p[cap] && s->p[cap] != q) { if (s->p[cap] == '\\' && s->p[cap + 1]) cap++; cap++; }
    if (s->p[cap] != q) { fail(s, "aspas sem fechar"); return 0; }
    char *v = malloc((unsigned long)cap + 1);
    int o = 0;
    if (!v) { fail(s, "sem memória"); return 0; }
    for (int i = 0; i < cap; i++) { if (st[i] == '\\' && i + 1 < cap) i++; v[o++] = st[i]; }
    v[o] = 0;
    s->p += cap + 1;
    while (*s->p == 'c' || *s->p == 'd' || *s->p == 'w') {
        if (*s->p == 'c') n->nocase = 1;
        else if (*s->p == 'd') n->nodiac = 1;
        else n->word = 1;
        s->p++;
    }
    return v;
}
static int number(ps_t *s, double *out) {
    const char *p = s->p;
    double v = 0, f = 0.1;
    int neg = 0, any = 0;
    if (*p == '-') { neg = 1; p++; }
    while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); p++; any = 1; }
    if (*p == '.') { p++; while (*p >= '0' && *p <= '9') { v += f * (*p - '0'); f /= 10; p++; any = 1; } }
    if (!any) return 0;
    *out = neg ? -v : v;
    s->p = p;
    return 1;
}

static mdq_node *expr(ps_t *s);
static mdq_node *unary(ps_t *s) {
    ws(s);
    if (s->bad) return 0;
    if (*s->p == '!' && s->p[1] != '=') {
        s->p++;
        mdq_node *n = node(N_NOT);
        if (!n) { fail(s, "sem memória"); return 0; }
        n->a = unary(s);
        return n;
    }
    if (*s->p == '(') {
        s->p++;
        mdq_node *n = expr(s);
        ws(s);
        if (*s->p != ')') { fail(s, "parêntese sem fechar"); mdq_free(n); return 0; }
        s->p++;
        return n;
    }
    if (*s->p == '"' || *s->p == '\'') {           /* texto solto: nome do item */
        mdq_node *n = node(N_TEXT);
        if (!n) { fail(s, "sem memória"); return 0; }
        n->val = quoted(s, n);
        return n;
    }
    if (*s->p == '$') { fail(s, "variável ($time...) não suportada"); return 0; }
    const char *st = s->p;
    while (isid(*s->p)) s->p++;
    int idl = (int)(s->p - st);
    if (!idl) { fail(s, "esperava um atributo"); return 0; }
    if ((idl == 4 && !__builtin_memcmp(st, "true", 4)) || (idl == 5 && !__builtin_memcmp(st, "false", 5)) ||
        (idl == 4 && !__builtin_memcmp(st, "TRUE", 4)) || (idl == 5 && !__builtin_memcmp(st, "FALSE", 5))) {
        mdq_node *n = node(N_CONST);
        if (n) n->konst = idl == 4;
        return n;
    }
    mdq_node *n = node(N_CMP);
    if (!n) { fail(s, "sem memória"); return 0; }
    n->attr = sdup(st, idl);
    ws(s);
    if (s->p[0] == '=' && s->p[1] == '=') { n->op = OP_EQ; s->p += 2; }
    else if (s->p[0] == '!' && s->p[1] == '=') { n->op = OP_NE; s->p += 2; }
    else if (s->p[0] == '<' && s->p[1] == '=') { n->op = OP_LE; s->p += 2; }
    else if (s->p[0] == '>' && s->p[1] == '=') { n->op = OP_GE; s->p += 2; }
    else if (s->p[0] == '<') { n->op = OP_LT; s->p++; }
    else if (s->p[0] == '>') { n->op = OP_GT; s->p++; }
    else if (s->p[0] == '=') { n->op = OP_EQ; s->p++; }
    else { fail(s, "esperava um operador"); mdq_free(n); return 0; }
    ws(s);
    if (*s->p == '"' || *s->p == '\'') {
        n->val = quoted(s, n);
        double d;
        ps_t t = {n->val ? n->val : "", 0, 0, 0};
        if (n->val && number(&t, &d) && !*t.p) { n->num = d; n->num_ok = 1; }
    } else if (number(s, &n->num)) {
        n->num_ok = 1;
        n->val = sdup("", 0);
    } else if (*s->p == '$') {
        fail(s, "variável ($time...) não suportada");
    } else {
        fail(s, "esperava um valor");
    }
    if (!s->bad && n->op >= OP_LT && !n->num_ok) fail(s, "comparação de ordem pede um número");
    return n;
}
static mdq_node *and_(ps_t *s) {
    mdq_node *l = unary(s);
    for (;;) {
        ws(s);
        if (s->bad || s->p[0] != '&' || s->p[1] != '&') return l;
        s->p += 2;
        mdq_node *n = node(N_AND);
        if (!n) { fail(s, "sem memória"); return l; }
        n->a = l;
        n->b = unary(s);
        l = n;
    }
}
static mdq_node *expr(ps_t *s) {
    mdq_node *l = and_(s);
    for (;;) {
        ws(s);
        if (s->bad || s->p[0] != '|' || s->p[1] != '|') return l;
        s->p += 2;
        mdq_node *n = node(N_OR);
        if (!n) { fail(s, "sem memória"); return l; }
        n->a = l;
        n->b = and_(s);
        l = n;
    }
}

mdq_node *mdq_parse(const char *q, char *err, int errcap) {
    ps_t s = {q ? q : "", err, errcap, 0};
    if (err && errcap > 0) err[0] = 0;
    mdq_node *n = expr(&s);
    ws(&s);
    if (!s.bad && *s.p) fail(&s, "texto sobrando no fim da consulta");
    if (!s.bad && !n) fail(&s, "consulta vazia");
    if (s.bad) { mdq_free(n); return 0; }
    return n;
}

/* ------------------------------------------------------------------ casamento */
static unsigned next_cp(const unsigned char **p) {
    const unsigned char *s = *p;
    unsigned c = *s;
    if (c < 0x80) { *p = s + 1; return c; }
    if ((c & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) { *p = s + 2; return ((c & 0x1F) << 6) | (s[1] & 0x3F); }
    if ((c & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) { *p = s + 3; return ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F); }
    if ((c & 0xF8) == 0xF0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80 && (s[3] & 0xC0) == 0x80) {
        *p = s + 4;
        return ((c & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
    }
    *p = s + 1;
    return c;
}
/* Latin-1 sem acento: À..ÿ para a letra base */
static unsigned undiac(unsigned c) {
    static const char base[] = "AAAAAAACEEEEIIIIDNOOOOO*OUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";
    if (c >= 0xC0 && c <= 0xFF) { char b = base[c - 0xC0]; if (b != '*' && b != '/') return (unsigned char)b; }
    return c;
}
static unsigned fold(unsigned c, int nocase, int nodiac) {
    if (nodiac) c = undiac(c);
    if (nocase) {
        if (c >= 'A' && c <= 'Z') c += 32;
        else if (c >= 0xC0 && c <= 0xDE && c != 0xD7) c += 32;
    }
    return c;
}
static int to_cps(const char *s, unsigned *out, int cap, int nocase, int nodiac) {
    const unsigned char *p = (const unsigned char *)s;
    int n = 0;
    while (*p && n < cap) out[n++] = fold(next_cp(&p), nocase, nodiac);
    return n;
}
static int wild(const unsigned *v, int nv, const unsigned *pt, int np) {
    int i = 0, j = 0, star = -1, mark = 0;
    while (i < nv) {
        if (j < np && pt[j] != '*' && pt[j] == v[i]) { i++; j++; }
        else if (j < np && pt[j] == '*') { star = j++; mark = i; }
        else if (star >= 0) { j = star + 1; i = ++mark; }
        else return 0;
    }
    while (j < np && pt[j] == '*') j++;
    return j == np;
}
static int isword(unsigned c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c >= 0xC0; }

int mdq_match(const char *value, const char *pattern, int nocase, int nodiac, int word) {
    enum { CAP = 1024 };
    unsigned v[CAP], pt[CAP + 1];
    int nv = to_cps(value ? value : "", v, CAP, nocase, nodiac);
    int np = to_cps(pattern ? pattern : "", pt, CAP, nocase, nodiac);
    if (!word) return wild(v, nv, pt, np);
    pt[np++] = '*';                                   /* por palavra: prefixo de alguma palavra */
    for (int i = 0; i < nv; i++) {
        if (i > 0 && isword(v[i - 1])) continue;
        if (!isword(v[i]) && !(np > 1 && pt[0] == v[i])) continue;
        if (wild(v + i, nv - i, pt, np)) return 1;
    }
    return 0;
}

static int to_num(const char *s, double *out) {
    ps_t t = {s ? s : "", 0, 0, 0};
    return number(&t, out) && !*t.p;
}
static int cmp_one(const mdq_node *n, const char *val) {
    if (n->op >= OP_LT || (n->num_ok && n->op <= OP_NE && !n->val[0])) {
        double d;
        if (!to_num(val, &d)) return 0;
        switch (n->op) {
        case OP_LT: return d < n->num;
        case OP_GT: return d > n->num;
        case OP_LE: return d <= n->num;
        case OP_GE: return d >= n->num;
        case OP_EQ: return d == n->num;
        default: return d != n->num;
        }
    }
    return mdq_match(val, n->val, n->nocase, n->nodiac, n->word);
}

int mdq_eval(const mdq_node *n, mdq_get_fn get, void *ctx) {
    if (!n) return 0;
    switch (n->kind) {
    case N_CONST: return n->konst;
    case N_NOT: return !mdq_eval(n->a, get, ctx);
    case N_AND: return mdq_eval(n->a, get, ctx) && mdq_eval(n->b, get, ctx);
    case N_OR: return mdq_eval(n->a, get, ctx) || mdq_eval(n->b, get, ctx);
    case N_TEXT: {
        static const char *names[] = {"kMDItemDisplayName", "kMDItemFSName"};
        for (int k = 0; k < 2; k++) {
            const char *v = get(ctx, names[k], 0);
            if (v && mdq_match(v, n->val, n->nocase, n->nodiac, 1)) return 1;
        }
        return 0;
    }
    case N_CMP: {
        int any = 0;
        const char *v;
        for (int i = 0; (v = get(ctx, n->attr, i)) != 0; i++) {
            mdq_node eq = *n;
            if (n->op == OP_NE) eq.op = OP_EQ;
            if (cmp_one(&eq, v)) { any = 1; break; }
        }
        if (n->op == OP_NE) return !any;
        return any;
    }
    }
    return 0;
}
