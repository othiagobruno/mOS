/* mOS libSystem — predicado de consulta do Spotlight (ver mdq_core.h). */
#include "mdq_core.h"

void *malloc(unsigned long n);
void *calloc(unsigned long n, unsigned long s);
void free(void *p);

enum { N_AND = 1, N_OR, N_NOT, N_CMP, N_TEXT, N_CONST, N_RANGE };
enum { OP_EQ = 1, OP_NE, OP_LT, OP_GT, OP_LE, OP_GE };

struct mdq_node {
    int kind, op, nocase, nodiac, word, num_ok, konst;
    double num, hi;
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

/* ---- $time: datas como CFAbsoluteTime (segundos desde 2001-01-01 UTC), no fuso de mdq_set_clock */
static double g_now;
static long g_tz;
void mdq_set_clock(double now_abs, long tz_offset) { g_now = now_abs; g_tz = tz_offset; }
static long long fl_div(long long a, long long b) { long long q = a / b; return (a % b && (a < 0) != (b < 0)) ? q - 1 : q; }
/* dias desde 1970-01-01 <-> data civil (H. Hinnant) */
static long long days_from_civil(long long y, int m, int d) {
    y -= m <= 2;
    long long era = fl_div(y, 400), yoe = y - era * 400;
    long long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1, doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}
static void civil_from_days(long long z, long long *y, int *m, int *d) {
    z += 719468;
    long long era = fl_div(z, 146097), doe = z - era * 146097;
    long long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365, yy = yoe + era * 400;
    long long doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;
    *d = (int)(doy - (153 * mp + 2) / 5 + 1);
    *m = (int)(mp < 10 ? mp + 3 : mp - 9);
    *y = yy + (*m <= 2);
}
#define EPOCH_DIFF 978307200.0
/* início do dia local que contém t (abs), mais n dias */
static double day_start(double t, long long n) {
    double local = t + EPOCH_DIFF + (double)g_tz;
    long long days = fl_div((long long)local, 86400);
    return (double)((days + n) * 86400) - (double)g_tz - EPOCH_DIFF;
}
static double month_start(double t, long long n, int year) {
    double local = t + EPOCH_DIFF + (double)g_tz;
    long long y; int m, d;
    civil_from_days(fl_div((long long)local, 86400), &y, &m, &d);
    if (year) { y += n; m = 1; }
    else { long long mm = (long long)(m - 1) + n; y += fl_div(mm, 12); m = (int)(mm - fl_div(mm, 12) * 12) + 1; }
    return (double)(days_from_civil(y, m, 1) * 86400) - (double)g_tz - EPOCH_DIFF;
}
/* $time.now, now(s), today, today(d), yesterday, this_week(w), this_month(m), this_year(y), iso(AAAA-MM-DD[Thh:mm:ss[Z]]) */
static int time_value(ps_t *s, double *out) {
    if (s->p[0] != '$' || __builtin_memcmp(s->p, "$time.", 6)) return 0;
    s->p += 6;
    const char *st = s->p;
    while (isid(*s->p) && *s->p != '.') s->p++;
    int n = (int)(s->p - st);
    double arg = 0, iso = 0;
    int have_arg = 0, iso_ok = 0;
    if (*s->p == '(') {
        s->p++;
        ws(s);
        if (n == 3 && !__builtin_memcmp(st, "iso", 3)) {
            const char *q = s->p;
            int f[6] = {0, 1, 1, 0, 0, 0}, k = 0;
            while (k < 6) {
                int v = 0, dg = 0;
                while (*q >= '0' && *q <= '9') { v = v * 10 + (*q - '0'); q++; dg++; }
                if (!dg) break;
                f[k++] = v;
                if (*q == '-' || *q == ':' || *q == 'T' || *q == ' ') q++;
                else break;
            }
            if (*q == 'Z') q++;
            if (k >= 3) {
                iso = (double)(days_from_civil(f[0], f[1], f[2]) * 86400 + f[3] * 3600 + f[4] * 60 + f[5]) - EPOCH_DIFF;
                iso_ok = 1;
            }
            s->p = q;
        } else if (number(s, &arg)) have_arg = 1;
        ws(s);
        if (*s->p != ')') { fail(s, "esperava ) em $time"); return 1; }
        s->p++;
    }
    double now = g_now;
    if (n == 3 && !__builtin_memcmp(st, "now", 3)) *out = now + (have_arg ? arg : 0);
    else if (n == 5 && !__builtin_memcmp(st, "today", 5)) *out = day_start(now, have_arg ? (long long)arg : 0);
    else if (n == 9 && !__builtin_memcmp(st, "yesterday", 9)) *out = day_start(now, -1);
    else if (n == 9 && !__builtin_memcmp(st, "this_week", 9)) {
        double local = now + EPOCH_DIFF + (double)g_tz;
        long long days = fl_div((long long)local, 86400);
        long long since_sunday = ((days + 4) % 7 + 7) % 7;      /* 1970-01-01 foi quinta */
        *out = day_start(now, -since_sunday + (have_arg ? (long long)arg * 7 : 0));
    }
    else if (n == 10 && !__builtin_memcmp(st, "this_month", 10)) *out = month_start(now, have_arg ? (long long)arg : 0, 0);
    else if (n == 9 && !__builtin_memcmp(st, "this_year", 9)) *out = month_start(now, have_arg ? (long long)arg : 0, 1);
    else if (n == 3 && !__builtin_memcmp(st, "iso", 3) && iso_ok) *out = iso;
    else { fail(s, "$time desconhecido"); return 1; }
    return 1;
}
/* valor de comparação: "texto", número ou $time.* (os dois últimos viram número) */
static int value(ps_t *s, mdq_node *n, double *num, int *num_ok) {
    if (*s->p == '"' || *s->p == '\'') {
        n->val = quoted(s, n);
        double d;
        ps_t t = {n->val ? n->val : "", 0, 0, 0};
        if (n->val && number(&t, &d) && !*t.p) { *num = d; *num_ok = 1; }
        return 1;
    }
    if (number(s, num) || time_value(s, num)) {
        *num_ok = 1;
        if (!n->val) n->val = sdup("", 0);
        return 1;
    }
    return 0;
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
    if (idl == 7 && !__builtin_memcmp(st, "InRange", 7)) {   /* InRange(atributo, mínimo, máximo), extremos inclusos */
        ws(s);
        if (*s->p != '(') { fail(s, "esperava ( depois de InRange"); return 0; }
        s->p++;
        ws(s);
        const char *as = s->p;
        while (isid(*s->p)) s->p++;
        mdq_node *n = node(N_RANGE);
        if (!n) { fail(s, "sem memória"); return 0; }
        n->attr = sdup(as, (int)(s->p - as));
        int ok1 = 0, ok2 = 0;
        ws(s);
        if (*s->p == ',') { s->p++; ws(s); value(s, n, &n->num, &ok1); ws(s); }
        if (*s->p == ',') { s->p++; ws(s); free(n->val); n->val = 0; value(s, n, &n->hi, &ok2); ws(s); }
        if (!ok1 || !ok2 || *s->p != ')' || !n->attr[0]) { fail(s, "InRange pede (atributo, número, número)"); mdq_free(n); return 0; }
        s->p++;
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
    if (!value(s, n, &n->num, &n->num_ok) && !s->bad) fail(s, "esperava um valor");
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
    case N_RANGE: {
        const char *v;
        double d;
        for (int i = 0; (v = get(ctx, n->attr, i)) != 0; i++)
            if (to_num(v, &d) && d >= n->num && d <= n->hi) return 1;
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
