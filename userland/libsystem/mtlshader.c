/*
 * mOS libSystem — compilador de Metal Shading Language (subconjunto do MSL 3) para o Metal por software.
 *
 * Fonte MSL -> tokens -> pré-processador (#define objeto e função, #if/#ifdef/#elif/#else, #include
 * pelo gancho msl_include_hook) -> árvore tipada. A árvore é executada pelo interpretador (mtlexec.c)
 * por vértice, fragmento ou thread de compute.
 *
 * Coberto: struct (layout de memória do Metal: float3 com 16 bytes, packed_float3 com 12), enum,
 * typedef/using, namespace, escalares bool/char/short/int/long/half/float com e sem sinal, vetores
 * 2..4 (e packed_), matrizes NxM de float/half, arrays (T[N] e array<T,N>), ponteiros e referências
 * nos espaços device/constant/threadgroup/thread, atomic_int/atomic_uint, texture1d/2d/2d_array/3d/
 * cube/buffer e depth2d, sampler e constexpr sampler, funções vertex/fragment/kernel e auxiliares
 * (sobrecarga por número/tipo de argumentos, recursão), todos os comandos de C (if, for, while, do,
 * switch, break, continue, return), operadores de C com swizzle de leitura e escrita, construtores
 * de vetor/matriz/struct, listas de inicialização, static_cast e as_type.
 * Fora: templates, lambdas, métodos em structs, argument buffers, mesh/tile shaders, raytracing.
 */
#include "mtlint.h"

char *(*msl_include_hook)(const char *name, u64 *len);

/* ================================================================ arena */
void *msl_alloc(msl_lib_t *l, u64 n) {
    n = (n + 15) & ~15ull;
    arena_blk_t *b = l->arena;
    if (!b || b->used + n > b->cap) {
        u64 cap = n > 65536 ? n : 65536;
        b = malloc(sizeof *b + cap);
        if (!b) return 0;
        b->next = l->arena;
        b->used = 0;
        b->cap = cap;
        l->arena = b;
    }
    void *p = b->data + b->used;
    b->used += n;
    memset(p, 0, n);
    return p;
}
static char *adup(msl_lib_t *l, const char *s, int n) {
    char *d = msl_alloc(l, n + 1);
    memcpy(d, s, n);
    d[n] = 0;
    return d;
}

/* ================================================================ tipos */
static ty_t *g_scal[12];
static ty_t *g_vec[12][5][2];
static ty_t *g_mat[12][5][5];
static ty_t *new_ty(int k) { ty_t *t = calloc(1, sizeof *t); t->k = k; return t; }
ty_t *msl_tscal(int k) {
    if (!g_scal[k]) {
        ty_t *t = new_ty(k);
        t->size = t->align = k == K_VOID ? 0 : scal_size(k);
        if (k == K_VOID) t->align = 1;
        g_scal[k] = t;
    }
    return g_scal[k];
}
ty_t *msl_tvec(int k, int n, int packed) {
    if (n <= 1) return msl_tscal(k);
    ty_t **c = &g_vec[k][n][packed ? 1 : 0];
    if (!*c) {
        ty_t *t = new_ty(K_VEC);
        int ss = scal_size(k);
        t->e = msl_tscal(k);
        t->n = n;
        t->packed = packed;
        t->size = packed ? n * ss : (n == 3 ? 4 : n) * ss;
        t->align = packed ? ss : t->size;
        *c = t;
    }
    return *c;
}
ty_t *msl_tmat(int k, int cols, int rows) {
    ty_t **c = &g_mat[k][cols][rows];
    if (!*c) {
        ty_t *t = new_ty(K_MAT);
        t->e = msl_tvec(k, rows, 0);
        t->n = cols;
        t->size = cols * t->e->size;
        t->align = t->e->align;
        *c = t;
    }
    return *c;
}
static ty_t *tarr(msl_lib_t *L, ty_t *e, int n) {
    ty_t *t = msl_alloc(L, sizeof *t);
    t->k = K_ARRAY;
    t->e = e;
    t->n = n;
    t->size = e->size * n;
    t->align = e->align ? e->align : 1;
    return t;
}
static ty_t *tptr(msl_lib_t *L, ty_t *e, int space) {
    ty_t *t = msl_alloc(L, sizeof *t);
    t->k = K_PTR;
    t->e = e;
    t->space = space;
    t->size = t->align = 8;
    return t;
}
static ty_t *topaque(msl_lib_t *L, int k) {
    ty_t *t = msl_alloc(L, sizeof *t);
    t->k = k;
    t->size = t->align = 8;
    return t;
}
static ty_t *tatomic(msl_lib_t *L, int k) {
    ty_t *t = msl_alloc(L, sizeof *t);
    t->k = K_ATOMIC;
    t->e = msl_tscal(k);
    t->size = t->align = scal_size(k);
    return t;
}

/* ================================================================ lexer */
enum { TK_EOF = 0, TK_ID, TK_INT, TK_FLT, TK_P, TK_STR };
typedef struct {
    int k, n, fl, line, bol;
    const char *p;
    double f;
    u64 i;
    int hide;                 /* macro que não pode ser reexpandida neste token */
} tok_t;
typedef struct { tok_t *v; int n, cap; } tvec_t;
static void tpush(tvec_t *v, tok_t t) {
    if (v->n == v->cap) { v->cap = v->cap ? v->cap * 2 : 256; v->v = realloc(v->v, v->cap * sizeof(tok_t)); }
    v->v[v->n++] = t;
}
static int is_id0(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
static int is_idc(int c) { return is_id0(c) || (c >= '0' && c <= '9'); }
static const char *puncts[] = {"<<=", ">>=", "...", "->", "++", "--", "<<", ">>", "<=", ">=", "==", "!=", "&&", "||",
                               "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "::", "##", 0};
static int tok_eq(const tok_t *t, const char *s) { int n = (int)strlen(s); return (t->k == TK_ID || t->k == TK_P) && t->n == n && !memcmp(t->p, s, n); }

static void lex(const char *s, u64 len, tvec_t *out) {
    const char *e = s + len;
    int line = 1, bol = 1;
    while (s < e) {
        char c = *s;
        if (c == '\n') { line++; bol = 1; s++; continue; }
        if (c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v') { s++; continue; }
        if (c == '\\' && s + 1 < e && (s[1] == '\n' || s[1] == '\r')) { s++; if (*s == '\r') s++; if (s < e && *s == '\n') { s++; line++; } continue; }
        if (c == '/' && s + 1 < e && s[1] == '/') { while (s < e && *s != '\n') s++; continue; }
        if (c == '/' && s + 1 < e && s[1] == '*') {
            s += 2;
            while (s + 1 < e && !(s[0] == '*' && s[1] == '/')) { if (*s == '\n') line++; s++; }
            s += 2;
            continue;
        }
        tok_t t = {0};
        t.line = line;
        t.bol = bol;
        bol = 0;
        t.p = s;
        if (is_id0(c)) {
            while (s < e && is_idc(*s)) s++;
            t.k = TK_ID;
        } else if ((c >= '0' && c <= '9') || (c == '.' && s + 1 < e && s[1] >= '0' && s[1] <= '9')) {
            const char *q = s;
            int flt = 0;
            if (c == '0' && s + 1 < e && (s[1] == 'x' || s[1] == 'X')) {
                q += 2;
                u64 v = 0;
                while (q < e && isxdigit(*q)) { v = v * 16 + (*q <= '9' ? *q - '0' : (*q | 32) - 'a' + 10); q++; }
                t.i = v;
            } else {
                while (q < e && ((*q >= '0' && *q <= '9') || *q == '.' || *q == 'e' || *q == 'E' ||
                                 ((*q == '+' || *q == '-') && (q[-1] == 'e' || q[-1] == 'E')))) {
                    if (*q == '.' || *q == 'e' || *q == 'E') flt = 1;
                    q++;
                }
                char buf[64];
                int n = (int)(q - s) < 63 ? (int)(q - s) : 63;
                memcpy(buf, s, n);
                buf[n] = 0;
                if (flt) t.f = strtod(buf, 0);
                else t.i = strtoull(buf, 0, buf[0] == '0' && n > 1 ? 8 : 10);
            }
            t.k = flt ? TK_FLT : TK_INT;
            while (q < e && is_idc(*q)) {
                char x = *q | 32;
                if (x == 'u') t.fl |= 1;
                else if (x == 'h') { t.fl |= 2; if (t.k == TK_INT) { t.k = TK_FLT; t.f = (double)t.i; } }
                else if (x == 'f') { if (t.k == TK_INT) { t.k = TK_FLT; t.f = (double)t.i; } }
                q++;
            }
            s = q;
        } else if (c == '"' || c == '\'') {
            s++;
            while (s < e && *s != c) { if (*s == '\\') s++; s++; }
            s++;
            t.k = TK_STR;
            if (c == '\'') { t.k = TK_INT; t.i = (u8)t.p[1]; }
        } else if (c == '<' && out->n && tok_eq(&out->v[out->n - 1], "include")) {
            while (s < e && *s != '>' && *s != '\n') s++;
            s++;
            t.k = TK_STR;
        } else {
            int m = 0;
            for (int i = 0; puncts[i]; i++) {
                int n = (int)strlen(puncts[i]);
                if (s + n <= e && !memcmp(s, puncts[i], n)) { m = n; break; }
            }
            s += m ? m : 1;
            t.k = TK_P;
        }
        t.n = (int)(s - t.p);
        tpush(out, t);
    }
}

/* ================================================================ pré-processador */
typedef struct macro {
    const char *name;
    int nn, fn, np;
    const char *params[16];
    int pn[16];
    tok_t *body;
    int nb;
    struct macro *next;
} macro_t;
typedef struct {
    msl_lib_t *L;
    macro_t *macros;
    char *err;
    int errn, failed, depth;
    void *jb[5];
    tvec_t keep;              /* fontes incluídas (mantidas vivas) */
    char *bufs[32];
    int nbufs;
} pp_t;
static void pp_err(pp_t *P, int line, const char *msg, const char *a, int an) {
    if (!P->failed && P->err) snprintf(P->err, P->errn, "linha %d: %s%s%.*s", line, msg, an ? " " : "", an, a ? a : "");
    P->failed = 1;
    __builtin_longjmp(P->jb, 1);
}
static macro_t *mfind(pp_t *P, const char *s, int n) {
    for (macro_t *m = P->macros; m; m = m->next) if (m->nn == n && !memcmp(m->name, s, n)) return m;
    return 0;
}
static void mdef_str(pp_t *P, const char *name, const char *body) {
    macro_t *m = calloc(1, sizeof *m);
    m->name = name;
    m->nn = (int)strlen(name);
    tvec_t v = {0};
    lex(body, strlen(body), &v);
    m->body = v.v;
    m->nb = v.n;
    m->next = P->macros;
    P->macros = m;
}
static void mundef(pp_t *P, const char *s, int n) {
    for (macro_t **pm = &P->macros; *pm; pm = &(*pm)->next)
        if ((*pm)->nn == n && !memcmp((*pm)->name, s, n)) { *pm = (*pm)->next; return; }
}

static void expand(pp_t *P, tok_t *in, int n, tvec_t *out, int hide);
static void expand_macro(pp_t *P, macro_t *m, tok_t *args[16], int an[16], int line, tvec_t *out) {
    tvec_t tmp = {0};
    for (int i = 0; i < m->nb; i++) {
        tok_t t = m->body[i];
        t.line = line;
        t.bol = 0;
        if (t.k == TK_ID && m->fn) {
            int pi = -1;
            for (int j = 0; j < m->np; j++) if (m->pn[j] == t.n && !memcmp(m->params[j], t.p, t.n)) pi = j;
            if (pi >= 0) {
                /* argumento expandido antes da substituição (menos com ##) */
                int paste = (i + 1 < m->nb && tok_eq(&m->body[i + 1], "##")) || (i > 0 && tok_eq(&m->body[i - 1], "##"));
                if (paste) for (int j = 0; j < an[pi]; j++) tpush(&tmp, args[pi][j]);
                else expand(P, args[pi], an[pi], &tmp, 0);
                continue;
            }
        }
        if (tok_eq(&t, "##") && tmp.n && i + 1 < m->nb) {
            /* colagem de tokens: só identificador ## identificador/número */
            tok_t *l = &tmp.v[tmp.n - 1];
            tok_t r = m->body[++i];
            if (m->fn && r.k == TK_ID)
                for (int j = 0; j < m->np; j++)
                    if (m->pn[j] == r.n && !memcmp(m->params[j], r.p, r.n) && an[j]) { r = args[j][0]; break; }
            char *s = malloc(l->n + r.n + 1);
            memcpy(s, l->p, l->n);
            memcpy(s + l->n, r.p, r.n);
            s[l->n + r.n] = 0;
            if (P->nbufs < 32) P->bufs[P->nbufs++] = s;
            tvec_t one = {0};
            lex(s, l->n + r.n, &one);
            if (one.n) { one.v[0].line = line; *l = one.v[0]; }
            free(one.v);
            continue;
        }
        if (tok_eq(&t, "#") && m->fn && i + 1 < m->nb) { /* #param: vira string (ignorada pelo parser) */
            i++;
            t.k = TK_STR;
        }
        tpush(&tmp, t);
    }
    if (++P->depth > 64) pp_err(P, line, "expansão de macro recursiva demais", 0, 0);
    /* reexpande sem a própria macro */
    for (int i = 0; i < tmp.n; i++) if (tmp.v[i].k == TK_ID && tmp.v[i].n == m->nn && !memcmp(tmp.v[i].p, m->name, m->nn)) tmp.v[i].hide = 1;
    expand(P, tmp.v, tmp.n, out, 0);
    P->depth--;
    free(tmp.v);
}
static void expand(pp_t *P, tok_t *in, int n, tvec_t *out, int hide) {
    for (int i = 0; i < n; i++) {
        tok_t *t = &in[i];
        macro_t *m = t->k == TK_ID && !t->hide && !hide ? mfind(P, t->p, t->n) : 0;
        if (!m) { tpush(out, *t); continue; }
        if (!m->fn) { expand_macro(P, m, 0, 0, t->line, out); continue; }
        if (i + 1 >= n || !tok_eq(&in[i + 1], "(")) { tpush(out, *t); continue; }
        tok_t *args[16] = {0};
        int an[16] = {0}, na = 0, depth = 0, j = i + 2;
        args[0] = &in[j];
        for (; j < n; j++) {
            if (tok_eq(&in[j], "(")) depth++;
            else if (tok_eq(&in[j], ")")) { if (!depth) break; depth--; }
            else if (tok_eq(&in[j], ",") && !depth) {
                if (++na >= 16) pp_err(P, t->line, "argumentos demais na macro", 0, 0);
                args[na] = &in[j + 1];
                continue;
            }
            an[na]++;
        }
        if (j >= n) pp_err(P, t->line, "chamada de macro sem ')'", m->name, m->nn);
        expand_macro(P, m, args, an, t->line, out);
        i = j;
    }
}

/* avaliação de #if */
typedef struct { tok_t *v; int n, i; } cx_t;
static i64 cond_expr(cx_t *c, int prec);
static i64 cond_prim(cx_t *c) {
    if (c->i >= c->n) return 0;
    tok_t *t = &c->v[c->i++];
    if (tok_eq(t, "(")) { i64 v = cond_expr(c, 0); if (c->i < c->n && tok_eq(&c->v[c->i], ")")) c->i++; return v; }
    if (tok_eq(t, "!")) return !cond_prim(c);
    if (tok_eq(t, "-")) return -cond_prim(c);
    if (tok_eq(t, "~")) return ~cond_prim(c);
    if (t->k == TK_INT) return (i64)t->i;
    if (t->k == TK_FLT) return (i64)t->f;
    return 0;                 /* identificador não definido */
}
static int cond_prec(tok_t *t, const char **op) {
    static const char *ops[] = {"||", "&&", "|", "^", "&", "==", "!=", "<", ">", "<=", ">=", "<<", ">>", "+", "-", "*", "/", "%", 0};
    static const int pr[] = {1, 2, 3, 4, 5, 6, 6, 7, 7, 7, 7, 8, 8, 9, 9, 10, 10, 10};
    for (int i = 0; ops[i]; i++) if (tok_eq(t, ops[i])) { *op = ops[i]; return pr[i]; }
    return 0;
}
static i64 cond_expr(cx_t *c, int prec) {
    i64 l = cond_prim(c);
    for (;;) {
        if (c->i >= c->n) return l;
        if (tok_eq(&c->v[c->i], "?") && prec == 0) {
            c->i++;
            i64 a = cond_expr(c, 0);
            if (c->i < c->n && tok_eq(&c->v[c->i], ":")) c->i++;
            i64 b = cond_expr(c, 0);
            l = l ? a : b;
            continue;
        }
        const char *op;
        int p = cond_prec(&c->v[c->i], &op);
        if (!p || p <= prec) return l;
        c->i++;
        i64 r = cond_expr(c, p);
        switch (op[0]) {
        case '|': l = op[1] ? (l || r) : (l | r); break;
        case '&': l = op[1] ? (l && r) : (l & r); break;
        case '^': l ^= r; break;
        case '=': l = l == r; break;
        case '!': l = l != r; break;
        case '<': l = op[1] == '=' ? l <= r : op[1] == '<' ? l << r : l < r; break;
        case '>': l = op[1] == '=' ? l >= r : op[1] == '>' ? l >> r : l > r; break;
        case '+': l += r; break;
        case '-': l -= r; break;
        case '*': l *= r; break;
        case '/': l = r ? l / r : 0; break;
        case '%': l = r ? l % r : 0; break;
        }
    }
}
static int eval_if(pp_t *P, tok_t *v, int n) {
    tvec_t a = {0}, b = {0};
    for (int i = 0; i < n; i++) {
        if (tok_eq(&v[i], "defined")) {
            int par = i + 1 < n && tok_eq(&v[i + 1], "(");
            tok_t *id = &v[i + 1 + par];
            tok_t r = {0};
            r.k = TK_INT;
            r.i = i + 1 + par < n && mfind(P, id->p, id->n) ? 1 : 0;
            tpush(&a, r);
            i += 1 + par + par;
            continue;
        }
        tpush(&a, v[i]);
    }
    expand(P, a.v, a.n, &b, 0);
    cx_t c = {b.v, b.n, 0};
    int r = cond_expr(&c, 0) != 0;
    free(a.v);
    free(b.v);
    return r;
}

static void preprocess(pp_t *P, tok_t *in, int n, tvec_t *out, int level) {
    u8 pact_s[64], taken_s[64];
    int sp = 0, act = 1;
    int run = -1;
    for (int i = 0; i <= n; i++) {
        int dir = i < n && in[i].bol && tok_eq(&in[i], "#");
        if (i == n || dir) {
            if (run >= 0 && act) expand(P, in + run, i - run, out, 0);
            run = -1;
            if (i == n) break;
            int j = i + 1;
            int e = j;
            while (e < n && !in[e].bol) e++;
            tok_t *d = j < e ? &in[j] : 0;
            if (!d) { i = e - 1; continue; }
            if (tok_eq(d, "ifdef") || tok_eq(d, "ifndef") || tok_eq(d, "if")) {
                int v;
                if (!act) v = 0;
                else if (tok_eq(d, "if")) v = eval_if(P, in + j + 1, e - j - 1);
                else { int def = j + 1 < e && mfind(P, in[j + 1].p, in[j + 1].n) != 0; v = tok_eq(d, "ifdef") ? def : !def; }
                if (sp >= 63) pp_err(P, d->line, "#if aninhado demais", 0, 0);
                pact_s[sp] = (u8)act;
                taken_s[sp++] = (u8)v;
                act = v;
            } else if (tok_eq(d, "elif")) {
                if (!sp) pp_err(P, d->line, "#elif sem #if", 0, 0);
                if (taken_s[sp - 1]) act = 0;
                else { act = pact_s[sp - 1] && eval_if(P, in + j + 1, e - j - 1); if (act) taken_s[sp - 1] = 1; }
            } else if (tok_eq(d, "else")) {
                if (!sp) pp_err(P, d->line, "#else sem #if", 0, 0);
                act = pact_s[sp - 1] && !taken_s[sp - 1];
                taken_s[sp - 1] = 1;
            } else if (tok_eq(d, "endif")) {
                if (!sp) pp_err(P, d->line, "#endif sem #if", 0, 0);
                act = pact_s[--sp];
            } else if (!act) {
            } else if (tok_eq(d, "define") && j + 1 < e) {
                tok_t *nm = &in[j + 1];
                mundef(P, nm->p, nm->n);
                macro_t *m = calloc(1, sizeof *m);
                m->name = nm->p;
                m->nn = nm->n;
                int b = j + 2;
                if (b < e && tok_eq(&in[b], "(") && in[b].p == nm->p + nm->n) {
                    m->fn = 1;
                    b++;
                    while (b < e && !tok_eq(&in[b], ")")) {
                        if (in[b].k == TK_ID && m->np < 16) { m->params[m->np] = in[b].p; m->pn[m->np++] = in[b].n; }
                        b++;
                    }
                    b++;
                }
                m->body = in + b;
                m->nb = e - b > 0 ? e - b : 0;
                m->next = P->macros;
                P->macros = m;
            } else if (tok_eq(d, "undef") && j + 1 < e) {
                mundef(P, in[j + 1].p, in[j + 1].n);
            } else if (tok_eq(d, "include") && j + 1 < e && in[j + 1].k == TK_STR) {
                tok_t *f = &in[j + 1];
                char name[256];
                int fn = f->n - 2 < 255 ? f->n - 2 : 255;
                memcpy(name, f->p + 1, fn);
                name[fn] = 0;
                int sys = f->p[0] == '<';
                if (!(sys && (!strncmp(name, "metal_", 6) || !strncmp(name, "simd/", 5) || !strcmp(name, "metal")))) {
                    u64 len = 0;
                    char *src = msl_include_hook && level < 16 ? msl_include_hook(name, &len) : 0;
                    if (!src && !sys) pp_err(P, d->line, "#include não encontrado:", name, (int)strlen(name));
                    if (src) {
                        if (P->nbufs < 32) P->bufs[P->nbufs++] = src;
                        tvec_t v = {0};
                        lex(src, len, &v);
                        preprocess(P, v.v, v.n, out, level + 1);
                        tpush(&P->keep, (tok_t){.p = (const char *)v.v});
                    }
                }
            } else if (tok_eq(d, "error")) {
                pp_err(P, d->line, "#error", in[j].p, (int)(in[e - 1].p + in[e - 1].n - in[j].p));
            }
            i = e - 1;
            continue;
        }
        if (run < 0) run = i;
    }
    if (sp) pp_err(P, n ? in[n - 1].line : 0, "#if sem #endif", 0, 0);
}

/* ================================================================ parser */
typedef struct sym { const char *name; ty_t *t; i64 ival; int kind; struct sym *next; } sym_t;   /* kind: 1 tipo, 2 constante de enum */
typedef struct {
    pp_t *pp;
    msl_lib_t *L;
    tok_t *t;
    int n, pos;
    sym_t *types;             /* structs, typedefs, enums, constantes de enum */
    var_t **scope;
    int nscope, capscope;
    var_t **globals;
    int nglob, capglob;
    msl_fn_t *cur;
    int frame;
    u32 tgoff;
    node_t **ginit;           /* inicializadores de globais (avaliados no fim) */
    var_t **ginitv;
    int nginit, capginit;
} P_t;

static tok_t *cur(P_t *P) { static tok_t eof; return P->pos < P->n ? &P->t[P->pos] : &eof; }
static tok_t *peek(P_t *P, int k) { static tok_t eof; return P->pos + k < P->n ? &P->t[P->pos + k] : &eof; }
static int line(P_t *P) { return P->pos < P->n ? P->t[P->pos].line : (P->n ? P->t[P->n - 1].line : 0); }
static __attribute__((noreturn)) void perr(P_t *P, const char *msg) {
    tok_t *t = cur(P);
    char m[160];
    snprintf(m, sizeof m, "%s perto de", msg);
    pp_err(P->pp, line(P), m, t->k ? t->p : "fim do fonte", t->k ? (t->n < 40 ? t->n : 40) : 12);
    __builtin_unreachable();
}
static __attribute__((noreturn)) void perr2(P_t *P, const char *msg, const char *a) {
    pp_err(P->pp, line(P), msg, a, (int)strlen(a));
    __builtin_unreachable();
}
static int is(P_t *P, const char *s) { return tok_eq(cur(P), s); }
static int accept(P_t *P, const char *s) { if (is(P, s)) { P->pos++; return 1; } return 0; }
static void expect(P_t *P, const char *s) {
    if (accept(P, s)) return;
    char m[64];
    snprintf(m, sizeof m, "esperava '%s'", s);
    tok_t *t = cur(P);
    pp_err(P->pp, line(P), m, t->p, t->k ? (t->n < 40 ? t->n : 40) : 0);
}
static void expect_gt(P_t *P) {
    tok_t *t = cur(P);
    if (tok_eq(t, ">")) { P->pos++; return; }
    if (tok_eq(t, ">>")) { t->p++; t->n = 1; return; }
    if (tok_eq(t, ">=")) { t->p++; t->n = 1; return; }
    expect(P, ">");
}
static char *ident(P_t *P) {
    tok_t *t = cur(P);
    if (t->k != TK_ID) perr(P, "esperava um identificador");
    P->pos++;
    return adup(P->L, t->p, t->n);
}
static node_t *mk(P_t *P, int op, ty_t *t) {
    node_t *n = msl_alloc(P->L, sizeof *n);
    n->op = op;
    n->t = t;
    n->line = line(P);
    return n;
}
static void push_list(P_t *P, node_t ***v, int *n, int *cap, node_t *x) {
    if (*n == *cap) {
        int nc = *cap ? *cap * 2 : 8;
        node_t **nv = msl_alloc(P->L, nc * sizeof *nv);
        if (*n) memcpy(nv, *v, *n * sizeof *nv);
        *v = nv;
        *cap = nc;
    }
    (*v)[(*n)++] = x;
}

/* ---- nomes de tipos */
static const char *strip_ns(const char *s) {
    static const char *ns[] = {"metal::", "simd::", "fast::", "precise::", "std::", 0};
    for (int again = 1; again;) {
        again = 0;
        for (int i = 0; ns[i]; i++) { int n = (int)strlen(ns[i]); if (!strncmp(s, ns[i], n)) { s += n; again = 1; } }
    }
    return s;
}
static int scal_by_name(const char *s, int n) {
    static const struct { const char *s; int k; } t[] = {
        {"bool", K_BOOL}, {"char", K_CHAR}, {"uchar", K_UCHAR}, {"short", K_SHORT}, {"ushort", K_USHORT}, {"int", K_INT},
        {"uint", K_UINT}, {"long", K_LONG}, {"ulong", K_ULONG}, {"half", K_HALF}, {"float", K_FLOAT}, {"int8_t", K_CHAR},
        {"uint8_t", K_UCHAR}, {"int16_t", K_SHORT}, {"uint16_t", K_USHORT}, {"int32_t", K_INT}, {"uint32_t", K_UINT},
        {"int64_t", K_LONG}, {"uint64_t", K_ULONG}, {"size_t", K_ULONG}, {"ptrdiff_t", K_LONG}, {"double", K_FLOAT},
        {"bfloat", K_FLOAT}, {0, 0}};
    for (int i = 0; t[i].s; i++) if ((int)strlen(t[i].s) == n && !memcmp(t[i].s, s, n)) return t[i].k;
    return -1;
}
/* "float", "float4", "packed_half3", "float4x4", "vector_float2", "simd_float4x4", "matrix_float3x3" */
static ty_t *builtin_type(const char *s) {
    s = strip_ns(s);
    if (!strcmp(s, "void")) return msl_tscal(K_VOID);
    int packed = 0;
    if (!strncmp(s, "packed_", 7)) { packed = 1; s += 7; }
    if (!strncmp(s, "vector_", 7)) s += 7;
    else if (!strncmp(s, "matrix_", 7)) s += 7;
    else if (!strncmp(s, "simd_", 5)) s += 5;
    if (!strncmp(s, "packed_", 7)) { packed = 1; s += 7; }
    int n = (int)strlen(s), b = n;
    while (b > 0 && ((s[b - 1] >= '0' && s[b - 1] <= '9') || s[b - 1] == 'x')) b--;
    /* não confundir o 'x' final de nomes (nenhum escalar termina em x) */
    int k = scal_by_name(s, b);
    if (k < 0) {
        /* sufixo numérico parte do nome (int8_t etc. já tratados) */
        k = scal_by_name(s, n);
        return k < 0 ? 0 : msl_tscal(k);
    }
    if (b == n) return msl_tscal(k);
    const char *d = s + b;
    if (d[0] >= '2' && d[0] <= '4' && !d[1]) return msl_tvec(k, d[0] - '0', packed);
    if (d[0] >= '2' && d[0] <= '4' && d[1] == 'x' && d[2] >= '2' && d[2] <= '4' && !d[3] && (k == K_FLOAT || k == K_HALF))
        return msl_tmat(k, d[0] - '0', d[2] - '0');
    if (d[0] == '1' && !d[1]) return msl_tscal(k);
    return 0;
}
static sym_t *sym_find(P_t *P, const char *s) {
    s = strip_ns(s);
    for (sym_t *y = P->types; y; y = y->next) if (!strcmp(y->name, s)) return y;
    return 0;
}
static void sym_add(P_t *P, const char *name, ty_t *t, int kind, i64 v) {
    sym_t *y = msl_alloc(P->L, sizeof *y);
    y->name = name;
    y->t = t;
    y->kind = kind;
    y->ival = v;
    y->next = P->types;
    P->types = y;
}
static const char *tex_names[] = {"texture1d", "texture1d_array", "texture2d", "texture2d_array", "texture2d_ms", "texturecube",
                                  "texturecube_array", "texture3d", "texture2d_ms_array", "texture_buffer", 0};
static const char *qualifiers[] = {"const", "constexpr", "static", "inline", "volatile", "thread", "device", "constant",
                                   "threadgroup", "threadgroup_imageblock", "ray_data", "object_data", "typename",
                                   "METAL_FUNC", "__attribute__", "restrict", "__restrict", "unsigned", "signed", 0};
/* nome composto com :: (metal::float4, Enum::Valor) */
static char *qual_name(P_t *P) {
    tok_t *t = cur(P);
    if (t->k != TK_ID) return 0;
    char buf[256];
    int n = 0;
    int i = 0;
    for (;;) {
        tok_t *a = peek(P, i);
        if (a->k != TK_ID || n + a->n + 3 >= (int)sizeof buf) break;
        memcpy(buf + n, a->p, a->n);
        n += a->n;
        if (tok_eq(peek(P, i + 1), "::") && peek(P, i + 2)->k == TK_ID) { memcpy(buf + n, "::", 2); n += 2; i += 2; continue; }
        i++;
        break;
    }
    buf[n] = 0;
    char *r = adup(P->L, buf, n);
    (void)i;
    return r;
}
static int qual_len(P_t *P) {
    int i = 0;
    for (;;) {
        if (peek(P, i)->k != TK_ID) return i;
        if (tok_eq(peek(P, i + 1), "::") && peek(P, i + 2)->k == TK_ID) { i += 2; continue; }
        return i + 1;
    }
}
static int is_type_start(P_t *P) {
    tok_t *t = cur(P);
    if (t->k != TK_ID) return 0;
    for (int i = 0; qualifiers[i]; i++) if (tok_eq(t, qualifiers[i])) return 1;
    if (tok_eq(t, "struct") || tok_eq(t, "auto") || tok_eq(t, "sampler") || tok_eq(t, "array") || tok_eq(t, "vec") ||
        tok_eq(t, "atomic") || tok_eq(t, "atomic_int") || tok_eq(t, "atomic_uint") || tok_eq(t, "atomic_bool") ||
        tok_eq(t, "atomic_float") || tok_eq(t, "matrix") || tok_eq(t, "packed_vec"))
        return 1;
    char *q = qual_name(P);
    const char *s = strip_ns(q);
    if (builtin_type(s)) return 1;
    for (int i = 0; tex_names[i]; i++) if (!strcmp(s, tex_names[i])) return 1;
    if (!strncmp(s, "depth2d", 7) || !strncmp(s, "depthcube", 9)) return 1;
    sym_t *y = sym_find(P, s);
    return y && y->kind == 1;
}

static i64 const_expr(P_t *P);
static ty_t *parse_type(P_t *P, int *space, int *isconst);
static ty_t *parse_targ(P_t *P) { int sp, c; return parse_type(P, &sp, &c); }
static void skip_attr_paren(P_t *P) {
    int d = 0;
    do {
        if (is(P, "(")) d++;
        else if (is(P, ")")) d--;
        P->pos++;
    } while (d > 0 && cur(P)->k);
}
static ty_t *parse_type(P_t *P, int *space, int *isconst) {
    *space = SP_THREAD;
    *isconst = 0;
    int uns = 0, sgn = 0;
    for (;;) {
        if (accept(P, "const") || accept(P, "constexpr")) { *isconst = 1; continue; }
        if (accept(P, "static") || accept(P, "inline") || accept(P, "volatile") || accept(P, "typename") ||
            accept(P, "METAL_FUNC") || accept(P, "restrict") || accept(P, "__restrict") || accept(P, "thread"))
            continue;
        if (accept(P, "__attribute__")) { skip_attr_paren(P); continue; }
        if (accept(P, "device")) { *space = SP_DEVICE; continue; }
        if (accept(P, "constant")) { *space = SP_CONSTANT; continue; }
        if (accept(P, "threadgroup") || accept(P, "threadgroup_imageblock")) { *space = SP_THREADGROUP; continue; }
        if (accept(P, "ray_data") || accept(P, "object_data")) continue;
        if (accept(P, "unsigned")) { uns = 1; continue; }
        if (accept(P, "signed")) { sgn = 1; continue; }
        break;
    }
    ty_t *t = 0;
    if (uns || sgn) {
        if (accept(P, "char")) t = msl_tscal(uns ? K_UCHAR : K_CHAR);
        else if (accept(P, "short")) t = msl_tscal(uns ? K_USHORT : K_SHORT);
        else if (accept(P, "long")) t = msl_tscal(uns ? K_ULONG : K_LONG);
        else { accept(P, "int"); t = msl_tscal(uns ? K_UINT : K_INT); }
        accept(P, "int");
        return t;
    }
    accept(P, "struct");
    accept(P, "enum");
    tok_t *tk = cur(P);
    if (tk->k != TK_ID) perr(P, "esperava um tipo");
    char *q = qual_name(P);
    const char *s = strip_ns(q);
    int ql = qual_len(P);
    if (!strcmp(s, "auto")) { P->pos += ql; return 0; }
    if (!strcmp(s, "array")) {
        P->pos += ql;
        expect(P, "<");
        ty_t *e = parse_targ(P);
        expect(P, ",");
        int n = (int)const_expr(P);
        expect_gt(P);
        return tarr(P->L, e, n);
    }
    if (!strcmp(s, "vec") || !strcmp(s, "packed_vec")) {
        P->pos += ql;
        expect(P, "<");
        ty_t *e = parse_targ(P);
        expect(P, ",");
        int n = (int)const_expr(P);
        expect_gt(P);
        return msl_tvec(e->k, n, s[0] == 'p');
    }
    if (!strcmp(s, "matrix")) {
        P->pos += ql;
        expect(P, "<");
        ty_t *e = parse_targ(P);
        expect(P, ",");
        int c = (int)const_expr(P);
        expect(P, ",");
        int r = (int)const_expr(P);
        expect_gt(P);
        return msl_tmat(e->k, c, r);
    }
    if (!strcmp(s, "atomic")) {
        P->pos += ql;
        expect(P, "<");
        ty_t *e = parse_targ(P);
        expect_gt(P);
        return tatomic(P->L, e->k);
    }
    if (!strcmp(s, "atomic_int")) { P->pos += ql; return tatomic(P->L, K_INT); }
    if (!strcmp(s, "atomic_uint")) { P->pos += ql; return tatomic(P->L, K_UINT); }
    if (!strcmp(s, "atomic_bool")) { P->pos += ql; return tatomic(P->L, K_BOOL); }
    if (!strcmp(s, "atomic_float")) { P->pos += ql; return tatomic(P->L, K_FLOAT); }
    if (!strcmp(s, "sampler")) { P->pos += ql; return topaque(P->L, K_SAMP); }
    int tk_i = -1, depth = 0;
    for (int i = 0; tex_names[i]; i++) if (!strcmp(s, tex_names[i])) tk_i = i;
    if (tk_i < 0) {
        static const struct { const char *s; int t; } dn[] = {{"depth2d", MSL_TEX_2D}, {"depth2d_array", MSL_TEX_2D_ARRAY},
            {"depthcube", MSL_TEX_CUBE}, {"depthcube_array", MSL_TEX_CUBE_ARRAY}, {"depth2d_ms", MSL_TEX_2D_MS},
            {"depth2d_ms_array", MSL_TEX_2D_MS_ARRAY}, {0, 0}};
        for (int i = 0; dn[i].s; i++) if (!strcmp(s, dn[i].s)) { tk_i = dn[i].t; depth = 1; }
    }
    if (tk_i >= 0) {
        P->pos += ql;
        ty_t *t = topaque(P->L, K_TEX);
        t->tex = tk_i;
        t->depth = depth;
        t->e = msl_tscal(K_FLOAT);
        if (accept(P, "<")) {
            ty_t *e = parse_targ(P);
            if (e) t->e = e;
            while (accept(P, ",")) { while (cur(P)->k == TK_ID || is(P, "::")) P->pos++; }
            expect_gt(P);
        }
        return t;
    }
    t = builtin_type(s);
    if (!t) {
        sym_t *y = sym_find(P, s);
        if (y && y->kind == 1) t = y->t;
    }
    if (!t) perr2(P, "tipo desconhecido:", q);
    P->pos += ql;
    if (t->k == K_LONG && accept(P, "long")) {}
    return t;
}

/* ---- atributos [[...]] */
typedef struct { int attr, idx, interp, stage, fc; const char *user; } attr_t;
static int at_attr(P_t *P) { return is(P, "[") && tok_eq(peek(P, 1), "["); }
static void parse_attrs(P_t *P, attr_t *a) {
    while (at_attr(P)) {
        P->pos += 2;
        while (!is(P, "]") && cur(P)->k) {
            char *nm = ident(P);
            const char *s = strip_ns(nm);
            int arg = -1;
            const char *sarg = 0;
            if (accept(P, "(")) {
                if (cur(P)->k == TK_INT) { arg = (int)cur(P)->i; P->pos++; }
                else if (cur(P)->k == TK_ID) {
                    sarg = ident(P);
                    sym_t *y = sym_find(P, sarg);
                    if (y && y->kind == 2) arg = (int)y->ival;
                    while (accept(P, "::")) { char *x = ident(P); sym_t *z = sym_find(P, x); if (z && z->kind == 2) arg = (int)z->ival; sarg = x; }
                } else arg = (int)const_expr(P);
                while (!is(P, ")") && cur(P)->k) P->pos++;
                expect(P, ")");
            }
            static const struct { const char *s; int a; } m[] = {
                {"position", A_POSITION}, {"attribute", A_ATTRIBUTE}, {"user", A_USER}, {"color", A_COLOR}, {"buffer", A_BUFFER},
                {"texture", A_TEXTURE}, {"sampler", A_SAMPLER}, {"stage_in", A_STAGE_IN}, {"vertex_id", A_VID},
                {"instance_id", A_IID}, {"base_vertex", A_BASE_VERTEX}, {"base_instance", A_BASE_INSTANCE},
                {"thread_position_in_grid", A_TPIG}, {"thread_position_in_threadgroup", A_TPITG},
                {"threadgroup_position_in_grid", A_TGPIG}, {"threads_per_threadgroup", A_TPTG}, {"threads_per_grid", A_TPG},
                {"threadgroups_per_grid", A_TGPG}, {"thread_index_in_threadgroup", A_TIITG}, {"point_size", A_POINT_SIZE},
                {"depth", A_DEPTH}, {"front_facing", A_FRONT_FACING}, {"point_coord", A_POINT_COORD}, {"sample_id", A_SAMPLE_ID},
                {"threadgroup", A_THREADGROUP}, {"clip_distance", A_CLIP_DISTANCE}, {"render_target_array_index", A_RT_INDEX},
                {"thread_index_in_simdgroup", A_SIMD_LANE}, {"simdgroup_index_in_threadgroup", A_SIMD_GROUP},
                {"threads_per_simdgroup", A_SIMD_SIZE}, {"thread_execution_width", A_SIMD_SIZE}, {"sample_mask", A_SAMPLE_MASK},
                {"primitive_id", A_PRIMITIVE_ID}, {"viewport_array_index", A_VIEWPORT_INDEX},
                {"dispatch_threads_per_threadgroup", A_TPTG}, {0, 0}};
            int found = 0;
            for (int i = 0; m[i].s; i++)
                if (!strcmp(s, m[i].s)) {
                    a->attr = m[i].a;
                    a->idx = arg < 0 ? 0 : arg;
                    if (m[i].a == A_USER) a->user = sarg;
                    found = 1;
                }
            if (!found) {
                if (!strcmp(s, "flat")) a->interp = MSL_INTERP_FLAT;
                else if (strstr(s, "no_perspective")) a->interp = MSL_INTERP_NOPERSP;
                else if (!strcmp(s, "vertex")) a->stage = MSL_VERTEX;
                else if (!strcmp(s, "fragment")) a->stage = MSL_FRAGMENT;
                else if (!strcmp(s, "kernel")) a->stage = MSL_KERNEL;
                else if (!strcmp(s, "function_constant")) a->fc = 1;
                /* id(n), raster_order_group, invariant, max_total_threads_per_threadgroup...: ignorados */
            }
            if (!accept(P, ",")) break;
        }
        expect(P, "]");
        expect(P, "]");
    }
}

/* ---- escopo */
static var_t *new_var(P_t *P, const char *name, ty_t *t) {
    var_t *v = msl_alloc(P->L, sizeof *v);
    v->name = name;
    v->t = t;
    return v;
}
static int alloc_frame(P_t *P, int size, int align) {
    if (align < 1) align = 1;
    if (align > 16) align = 16;
    P->frame = (P->frame + align - 1) & ~(align - 1);
    int o = P->frame;
    P->frame += size ? size : 1;
    return o;
}
static void scope_push(P_t *P, var_t *v) {
    if (P->nscope == P->capscope) {
        P->capscope = P->capscope ? P->capscope * 2 : 64;
        var_t **n = msl_alloc(P->L, P->capscope * sizeof *n);
        if (P->nscope) memcpy(n, P->scope, P->nscope * sizeof *n);
        P->scope = n;
    }
    P->scope[P->nscope++] = v;
}
static var_t *lookup(P_t *P, const char *s) {
    for (int i = P->nscope - 1; i >= 0; i--) if (!strcmp(P->scope[i]->name, s)) return P->scope[i];
    for (int i = P->nglob - 1; i >= 0; i--) if (!strcmp(P->globals[i]->name, s)) return P->globals[i];
    return 0;
}
static void glob_push(P_t *P, var_t *v) {
    if (P->nglob == P->capglob) {
        P->capglob = P->capglob ? P->capglob * 2 : 32;
        var_t **n = msl_alloc(P->L, P->capglob * sizeof *n);
        if (P->nglob) memcpy(n, P->globals, P->nglob * sizeof *n);
        P->globals = n;
    }
    P->globals[P->nglob++] = v;
}
static int alloc_global(P_t *P, int size, int align) {
    msl_lib_t *L = P->L;
    if (align < 1) align = 1;
    int o = (L->gsize + align - 1) & ~(align - 1);
    if (o + size > L->gcap) {
        int nc = L->gcap ? L->gcap * 2 : 4096;
        while (nc < o + size) nc *= 2;
        L->gmem = realloc(L->gmem, nc);
        memset(L->gmem + L->gcap, 0, nc - L->gcap);
        L->gcap = nc;
    }
    L->gsize = o + size;
    return o;
}

/* ================================================================ expressões */
static node_t *expr(P_t *P);
static node_t *assign_expr(P_t *P);
void msl_global_init(msl_lib_t *L, node_t *init, ty_t *t, void *dst);
i64 msl_const_eval(msl_lib_t *L, node_t *n, int *ok);

static i64 const_expr(P_t *P) {
    node_t *n = assign_expr(P);
    int ok = 1;
    i64 v = msl_const_eval(P->L, n, &ok);
    if (!ok) perr(P, "esperava uma expressão constante");
    return v;
}
static ty_t *bool_shape(int n) { return msl_tvec(K_BOOL, n, 0); }
static int shape(ty_t *t) { return t->k == K_VEC ? t->n : 1; }
static int is_num(ty_t *t) { return t && (IS_NUM(t) || t->k == K_ATOMIC); }
static ty_t *num_of(ty_t *t) { return t->k == K_ATOMIC ? t->e : t; }

/* tipo de uma operação aritmética entre a e b */
static ty_t *arith(P_t *P, ty_t *a, ty_t *b, int op) {
    a = num_of(a);
    b = num_of(b);
    if (!IS_NUM(a) || !IS_NUM(b)) perr(P, "operandos inválidos");
    if (a->k == K_MAT || b->k == K_MAT) {
        int k = ty_scal(a) > ty_scal(b) ? ty_scal(a) : ty_scal(b);
        if (k < K_HALF) k = K_FLOAT;
        if (op == '*') {
            if (a->k == K_MAT && b->k == K_MAT) return msl_tmat(k, b->n, a->e->n);
            if (a->k == K_MAT && b->k == K_VEC) return msl_tvec(k, a->e->n, 0);
            if (a->k == K_VEC && b->k == K_MAT) return msl_tvec(k, b->n, 0);
        }
        ty_t *m = a->k == K_MAT ? a : b;
        return msl_tmat(k, m->n, m->e->n);
    }
    int na = shape(a), nb = shape(b);
    if (na != nb && na != 1 && nb != 1) perr(P, "vetores de tamanhos diferentes");
    int n = na > nb ? na : nb;
    int ka = ty_scal(a), kb = ty_scal(b);
    int k = ka > kb ? ka : kb;
    if (k == K_BOOL || (n == 1 && k < K_INT)) k = K_INT;
    return msl_tvec(k, n, 0);
}
static int lvalue(node_t *n) {
    return n->op == E_VAR || n->op == E_MEMBER || n->op == E_INDEX || n->op == E_DEREF ||
           (n->op == E_SWZ && lvalue(n->a));
}
static node_t *mk_lit_i(P_t *P, i64 v, int k) { node_t *n = mk(P, E_LIT, msl_tscal(k)); n->iv = v; n->fv = (double)v; return n; }

static int bi_lookup(const char *s, int *rule);
enum { R_GEN = 1, R_GENF, R_SCALAR, R_SAME, R_BOOLV, R_BOOL, R_VOID, R_TRANSPOSE, R_DET, R_ATOMIC, R_UINT, R_F4, R_F2, R_OPT, R_GEN0 };

static node_t *call_args(P_t *P, node_t *n) {
    int cap = 0;
    expect(P, "(");
    if (!is(P, ")"))
        do { push_list(P, &n->v, &n->nv, &cap, assign_expr(P)); } while (accept(P, ","));
    expect(P, ")");
    return n;
}
static ty_t *tex_result(ty_t *tt, int m) {
    int k = tt->e ? tt->e->k : K_FLOAT;
    switch (m) {
    case T_SAMPLE: case T_READ: case T_GATHER: return tt->depth && m != T_GATHER ? msl_tscal(K_FLOAT) : msl_tvec(k, 4, 0);
    case T_SAMPLE_CMP: return msl_tscal(K_FLOAT);
    case T_WRITE: case T_FENCE: return msl_tscal(K_VOID);
    default: return msl_tscal(K_UINT);
    }
}

/* escolhe a função do usuário (sobrecarga) */
static msl_fn_t *pick_fn(P_t *P, const char *name, node_t *call) {
    msl_fn_t *best = 0;
    int bs = -1;
    for (int i = 0; i < P->L->nfn; i++) {
        msl_fn_t *f = P->L->fns[i];
        if (strcmp(f->name, name) || f->np != call->nv) continue;
        int sc = 1;
        for (int j = 0; j < f->np; j++) {
            ty_t *pt = f->params[j]->t, *at = call->v[j]->t;
            if (!at) continue;
            if (pt == at) sc += 4;
            else if (is_num(pt) && is_num(at)) {
                if (shape(pt) == shape(at) && pt->k == at->k) sc += 2;
                else if (shape(pt) == shape(at) || shape(at) == 1) sc += 1;
                else { sc = -100; break; }
            } else if (pt->k == K_PTR && at->k == K_PTR) sc += 2;
            else if (pt->k == at->k && pt->k != K_STRUCT) sc += 2;
            else { sc = -100; break; }
        }
        if (sc > bs) { bs = sc; best = f; }
    }
    return bs >= 0 ? best : 0;
}

static node_t *member(P_t *P, node_t *b, const char *name) {
    ty_t *t = b->t;
    if (t->k == K_STRUCT) {
        for (int i = 0; i < t->nf; i++)
            if (!strcmp(t->f[i].name, name)) {
                node_t *n = mk(P, E_MEMBER, t->f[i].t);
                n->a = b;
                n->off = t->f[i].off;
                return n;
            }
        perr2(P, "membro desconhecido:", name);
    }
    if (t->k == K_VEC || IS_SCALAR(t)) {
        int len = (int)strlen(name), nc = shape(t);
        if (len < 1 || len > 4) perr2(P, "swizzle inválido:", name);
        node_t *n = mk(P, E_SWZ, msl_tvec(ty_scal(t), len, 0));
        n->a = b;
        n->nswz = len;
        for (int i = 0; i < len; i++) {
            const char *xy = "xyzw", *rg = "rgba";
            const char *p = strchr(xy, name[i]);
            int c = p ? (int)(p - xy) : (strchr(rg, name[i]) ? (int)(strchr(rg, name[i]) - rg) : -1);
            if (c < 0 || c >= nc) perr2(P, "swizzle inválido:", name);
            n->swz[i] = (u8)c;
        }
        return n;
    }
    perr2(P, "acesso a membro em tipo que não é struct:", name);
}

static node_t *postfix(P_t *P, node_t *n) {
    for (;;) {
        if (accept(P, "[")) {
            node_t *i = expr(P);
            expect(P, "]");
            ty_t *t = n->t, *rt;
            if (t->k == K_ARRAY || t->k == K_PTR) rt = t->e;
            else if (t->k == K_VEC) rt = t->e;
            else if (t->k == K_MAT) rt = t->e;
            else perr(P, "índice em tipo que não é array/vetor/ponteiro");
            node_t *x = mk(P, E_INDEX, rt);
            x->a = n;
            x->b = i;
            n = x;
        } else if (is(P, ".") || is(P, "->")) {
            int arrow = is(P, "->");
            P->pos++;
            char *name = ident(P);
            if (arrow) {
                if (n->t->k != K_PTR) perr(P, "'->' em algo que não é ponteiro");
                node_t *d = mk(P, E_DEREF, n->t->e);
                d->a = n;
                n = d;
            }
            if (n->t->k == K_TEX && is(P, "(")) {
                static const struct { const char *s; int m; } tm[] = {{"sample", T_SAMPLE}, {"read", T_READ}, {"write", T_WRITE},
                    {"get_width", T_GET_W}, {"get_height", T_GET_H}, {"get_depth", T_GET_D}, {"get_array_size", T_GET_ARR},
                    {"get_num_mip_levels", T_GET_MIPS}, {"sample_compare", T_SAMPLE_CMP}, {"gather", T_GATHER},
                    {"fence", T_FENCE}, {"get_num_samples", T_GET_SAMPLES}, {0, 0}};
                int m = 0;
                for (int i = 0; tm[i].s; i++) if (!strcmp(name, tm[i].s)) m = tm[i].m;
                if (!m) perr2(P, "método de textura não suportado:", name);
                node_t *c = mk(P, E_TEX, tex_result(n->t, m));
                c->sub = m;
                c->a = n;
                call_args(P, c);
                /* level()/bias()/gradient2d()/min_lod_clamp() viram opção */
                for (int i = 0; i < c->nv; i++)
                    if (c->v[i]->op == E_BI && (c->v[i]->bi >= B_LEVEL && c->v[i]->bi <= B_MINLOD)) {
                        c->d = c->v[i];
                        for (int j = i; j + 1 < c->nv; j++) c->v[j] = c->v[j + 1];
                        c->nv--;
                        i--;
                    }
                n = c;
                continue;
            }
            if (n->t->k == K_ATOMIC) n->t = n->t->e;
            n = member(P, n, name);
        } else if (is(P, "++") || is(P, "--")) {
            int inc = is(P, "++");
            P->pos++;
            if (!lvalue(n)) perr(P, "++/-- em algo que não é lvalue");
            node_t *x = mk(P, inc ? E_POSTINC : E_POSTDEC, n->t);
            x->a = n;
            n = x;
        } else return n;
    }
}

static node_t *init_list(P_t *P) {
    node_t *n = mk(P, E_INIT, 0);
    int cap = 0;
    expect(P, "{");
    while (!is(P, "}")) {
        push_list(P, &n->v, &n->nv, &cap, is(P, "{") ? init_list(P) : assign_expr(P));
        if (!accept(P, ",")) break;
    }
    expect(P, "}");
    return n;
}

static node_t *primary(P_t *P) {
    tok_t *t = cur(P);
    if (t->k == TK_INT) {
        P->pos++;
        int k = t->fl & 1 ? K_UINT : (t->i > 0x7fffffffull ? (t->i > 0xffffffffull ? K_LONG : K_UINT) : K_INT);
        return mk_lit_i(P, (i64)t->i, k);
    }
    if (t->k == TK_FLT) {
        P->pos++;
        node_t *n = mk(P, E_LIT, msl_tscal(t->fl & 2 ? K_HALF : K_FLOAT));
        n->fv = t->f;
        return n;
    }
    if (accept(P, "(")) {
        /* cast (tipo) expr */
        if (is_type_start(P)) {
            int save = P->pos;
            int sp, c;
            ty_t *ct = parse_type(P, &sp, &c);
            if (ct && accept(P, ")")) {
                node_t *x = mk(P, E_CAST, ct);
                extern node_t *msl_unary(P_t *P);
                x->a = msl_unary(P);
                return x;
            }
            P->pos = save;
        }
        node_t *e = expr(P);
        expect(P, ")");
        return e;
    }
    if (is(P, "{")) return init_list(P);
    if (t->k != TK_ID) perr(P, "expressão inválida");
    if (tok_eq(t, "true") || tok_eq(t, "false")) { P->pos++; return mk_lit_i(P, tok_eq(t, "true"), K_BOOL); }
    if (tok_eq(t, "sizeof")) {
        P->pos++;
        expect(P, "(");
        ty_t *st;
        if (is_type_start(P)) { int sp, c; st = parse_type(P, &sp, &c); while (accept(P, "*")) st = tptr(P->L, st, 0); }
        else st = expr(P)->t;
        expect(P, ")");
        return mk_lit_i(P, st ? st->size : 0, K_ULONG);
    }
    if (tok_eq(t, "static_cast") || tok_eq(t, "reinterpret_cast") || tok_eq(t, "as_type") || tok_eq(t, "const_cast")) {
        int bit = tok_eq(t, "as_type");
        P->pos++;
        expect(P, "<");
        ty_t *ct = parse_targ(P);
        while (accept(P, "*")) ct = tptr(P->L, ct, 0);
        accept(P, "&");
        expect_gt(P);
        expect(P, "(");
        node_t *x = mk(P, bit ? E_BITCAST : E_CAST, ct);
        x->a = expr(P);
        expect(P, ")");
        if (ct->k == K_PTR && x->a->t->k == K_PTR) x->op = E_BITCAST;
        return x;
    }
    /* construtor: tipo(...) ou tipo{...} */
    if (is_type_start(P) && !lookup(P, strip_ns(qual_name(P)))) {
        int save = P->pos;
        int sp, c;
        ty_t *ct = parse_type(P, &sp, &c);
        if (ct && is(P, "(")) {
            node_t *x = mk(P, E_CTOR, ct);
            call_args(P, x);
            return x;
        }
        if (ct && is(P, "{")) {
            node_t *x = init_list(P);
            x->t = ct;
            return x;
        }
        P->pos = save;
    }
    char *q = qual_name(P);
    int ql = qual_len(P);
    const char *s = strip_ns(q);
    /* Enum::Valor e constantes de enum */
    const char *last = s;
    for (const char *p = s; *p; p++) if (p[0] == ':' && p[1] == ':') last = p + 2;
    var_t *v = lookup(P, s);
    if (!v && last != s) v = lookup(P, last);
    if (v) {
        P->pos += ql;
        node_t *n = mk(P, E_VAR, v->t);
        n->var = v;
        return n;
    }
    sym_t *y = sym_find(P, s);
    if (!y && last != s) y = sym_find(P, last);
    if (y && y->kind == 2) { P->pos += ql; return mk_lit_i(P, y->ival, y->t ? y->t->k : K_INT); }
    if (!strncmp(last, "memory_order", 12) || !strcmp(last, "mem_none") || !strcmp(last, "mem_device") ||
        !strcmp(last, "mem_threadgroup") || !strcmp(last, "mem_texture") || !strcmp(last, "thread_scope_device")) {
        P->pos += ql;
        return mk_lit_i(P, 0, K_INT);
    }
    if (!strcmp(last, "M_PI_F") || !strcmp(last, "M_PI_H") || !strcmp(last, "M_PI")) { P->pos += ql; node_t *n = mk(P, E_LIT, msl_tscal(K_FLOAT)); n->fv = 3.14159265358979323846; return n; }
    if (!strcmp(last, "MAXFLOAT") || !strcmp(last, "FLT_MAX") || !strcmp(last, "HUGE_VALF") || !strcmp(last, "INFINITY")) {
        P->pos += ql;
        node_t *n = mk(P, E_LIT, msl_tscal(K_FLOAT));
        n->fv = last[0] == 'I' || last[0] == 'H' ? __builtin_inf() : 3.402823466e+38;
        return n;
    }
    if (!strcmp(last, "FLT_EPSILON")) { P->pos += ql; node_t *n = mk(P, E_LIT, msl_tscal(K_FLOAT)); n->fv = 1.1920929e-7; return n; }
    if (!strcmp(last, "FLT_MIN")) { P->pos += ql; node_t *n = mk(P, E_LIT, msl_tscal(K_FLOAT)); n->fv = 1.17549435e-38; return n; }
    if (!strcmp(last, "NAN")) { P->pos += ql; node_t *n = mk(P, E_LIT, msl_tscal(K_FLOAT)); n->fv = __builtin_nan(""); return n; }
    if (!strcmp(last, "UINT_MAX")) { P->pos += ql; return mk_lit_i(P, 0xffffffffll, K_UINT); }
    if (!strcmp(last, "INT_MAX")) { P->pos += ql; return mk_lit_i(P, 0x7fffffffll, K_INT); }
    if (!strcmp(last, "INT_MIN")) { P->pos += ql; return mk_lit_i(P, -0x80000000ll, K_INT); }
    P->pos += ql;
    if (!is(P, "(")) perr2(P, "identificador desconhecido:", q);
    /* chamada: função do usuário ou embutida */
    node_t *c = mk(P, E_CALL, 0);
    call_args(P, c);
    msl_fn_t *f = pick_fn(P, last, c);
    if (f) {
        c->fn = f;
        c->t = f->ret;
        for (int i = 0; i < f->np; i++)
            if (f->params[i]->ref && !lvalue(c->v[i]) && f->params[i]->t->k != K_TEX)
                ; /* const & a temporário: o interpretador copia */
        return c;
    }
    int rule = 0;
    int b = bi_lookup(last, &rule);
    if (!b) perr2(P, "função desconhecida:", q);
    c->op = E_BI;
    c->bi = b;
    if (b == B_BARRIER && P->cur) P->cur->barrier = 1;
    if (b == B_DISCARD && P->cur) P->cur->discard = 1;
    for (int i = 0; i < c->nv; i++) if (!c->v[i]->t) perr(P, "argumento sem tipo");
    ty_t *a0 = c->nv ? num_of(c->v[0]->t) : 0;
    switch (rule) {
    case R_GEN: case R_GENF: case R_GEN0: {
        if (!c->nv) perr(P, "função sem argumentos");
        ty_t *r = a0;
        for (int i = 1; i < c->nv; i++) {
            if (b == B_SELECT && i == 2) break;
            r = arith(P, r, c->v[i]->t, '+');
        }
        if (IS_SCALAR(r) || r->k == K_VEC) {
            int k = ty_scal(r);
            if (rule == R_GENF && IS_INTK(k)) k = K_FLOAT;
            if (rule == R_GEN0 && IS_SCALAR(a0)) k = ty_scal(a0) < K_INT && IS_INTK(ty_scal(a0)) ? K_INT : k;
            r = msl_tvec(k, shape(r), 0);
        }
        c->t = r;
        break;
    }
    case R_SCALAR: { int k = ty_scal(a0); c->t = msl_tscal(IS_INTK(k) ? K_FLOAT : k); break; }
    case R_SAME: { int k = ty_scal(a0); c->t = (b >= B_NORMALIZE && b <= B_FACEFORWARD && IS_INTK(k)) ? msl_tvec(K_FLOAT, shape(a0), 0) : (a0->k == K_ATOMIC ? a0->e : a0); break; }
    case R_BOOLV: c->t = bool_shape(shape(a0)); break;
    case R_BOOL: c->t = msl_tscal(K_BOOL); break;
    case R_VOID: c->t = msl_tscal(K_VOID); break;
    case R_TRANSPOSE: if (a0->k != K_MAT) perr(P, "transpose sem matriz"); c->t = msl_tmat(ty_scal(a0), a0->e->n, a0->n); break;
    case R_DET: c->t = msl_tscal(ty_scal(a0)); break;
    case R_ATOMIC: {
        ty_t *p = c->nv ? c->v[0]->t : 0;
        if (!p || p->k != K_PTR) perr(P, "atomic_* espera um ponteiro");
        c->t = b == B_ATOMIC_STORE ? msl_tscal(K_VOID) : b == B_ATOMIC_CAS ? msl_tscal(K_BOOL) : msl_tscal(num_of(p->e)->k);
        break;
    }
    case R_UINT: c->t = msl_tscal(K_UINT); break;
    case R_F4: c->t = msl_tvec(K_FLOAT, 4, 0); break;
    case R_F2: c->t = msl_tvec(K_FLOAT, 2, 0); break;
    case R_OPT: c->t = msl_tscal(K_VOID); break;
    }
    if (b == B_CROSS) c->t = msl_tvec(ty_scal(c->t), 3, 0);
    if (b == B_DOT || b == B_LENGTH || b == B_LENGTH2 || b == B_DISTANCE || b == B_DISTANCE2) {
        int k = ty_scal(a0);
        if (c->nv > 1 && ty_scal(c->v[1]->t) > k) k = ty_scal(c->v[1]->t);
        c->t = msl_tscal(IS_INTK(k) && b != B_DOT ? K_FLOAT : k);
    }
    return c;
}

node_t *msl_unary(P_t *P) {
    if (is(P, "-") || is(P, "+") || is(P, "!") || is(P, "~")) {
        int op = cur(P)->p[0];
        P->pos++;
        node_t *a = msl_unary(P);
        if (op == '+') return a;
        if (!is_num(a->t)) perr(P, "operador unário em tipo não numérico");
        ty_t *t = num_of(a->t);
        if (op == '!') t = bool_shape(shape(t));
        else if (ty_scal(t) == K_BOOL) t = msl_tvec(K_INT, shape(t), 0);
        if (a->op == E_LIT && op == '-') {     /* dobra -literal */
            node_t *n = mk(P, E_LIT, t);
            n->fv = -a->fv;
            n->iv = -a->iv;
            return n;
        }
        node_t *n = mk(P, E_UN, t);
        n->sub = op;
        n->a = a;
        return n;
    }
    if (is(P, "++") || is(P, "--")) {
        int inc = is(P, "++");
        P->pos++;
        node_t *a = msl_unary(P);
        if (!lvalue(a)) perr(P, "++/-- em algo que não é lvalue");
        node_t *n = mk(P, inc ? E_PREINC : E_PREDEC, a->t);
        n->a = a;
        return n;
    }
    if (accept(P, "*")) {
        node_t *a = msl_unary(P);
        if (a->t->k != K_PTR) perr(P, "'*' em algo que não é ponteiro");
        node_t *n = mk(P, E_DEREF, a->t->e);
        n->a = a;
        return n;
    }
    if (accept(P, "&")) {
        node_t *a = msl_unary(P);
        if (!lvalue(a)) perr(P, "'&' em algo que não é lvalue");
        node_t *n = mk(P, E_ADDR, tptr(P->L, a->t, 0));
        n->a = a;
        return n;
    }
    return postfix(P, primary(P));
}

static int bin_prec(tok_t *t, int *op) {
    static const struct { const char *s; int p, op; } ops[] = {
        {"||", 1, 'O'}, {"&&", 2, 'A'}, {"|", 3, '|'}, {"^", 4, '^'}, {"&", 5, '&'}, {"==", 6, 'e'}, {"!=", 6, 'n'},
        {"<", 7, '<'}, {">", 7, '>'}, {"<=", 7, 'l'}, {">=", 7, 'g'}, {"<<", 8, 'L'}, {">>", 8, 'R'}, {"+", 9, '+'},
        {"-", 9, '-'}, {"*", 10, '*'}, {"/", 10, '/'}, {"%", 10, '%'}, {0, 0, 0}};
    if (t->k != TK_P) return 0;
    for (int i = 0; ops[i].s; i++) if (tok_eq(t, ops[i].s)) { *op = ops[i].op; return ops[i].p; }
    return 0;
}
node_t *msl_binop(P_t *P, int op, node_t *l, node_t *r) {
    if (op == 'O' || op == 'A') {
        node_t *n = mk(P, op == 'O' ? E_LOR : E_LAND, msl_tscal(K_BOOL));
        n->a = l;
        n->b = r;
        if (shape(l->t) > 1 || shape(r->t) > 1) { n->op = E_BIN; n->sub = op; n->t = bool_shape(shape(l->t) > shape(r->t) ? shape(l->t) : shape(r->t)); }
        return n;
    }
    if (l->t && l->t->k == K_PTR && (op == '+' || op == '-') && is_num(r->t)) {
        node_t *n = mk(P, E_BIN, l->t);
        n->sub = op;
        n->a = l;
        n->b = r;
        return n;
    }
    if (!is_num(l->t) || !is_num(r->t)) perr(P, "operandos inválidos para o operador");
    ty_t *t;
    if (op == 'e' || op == 'n' || op == '<' || op == '>' || op == 'l' || op == 'g') {
        ty_t *a = arith(P, l->t, r->t, op);
        t = bool_shape(shape(a));
    } else if (op == 'L' || op == 'R') {
        t = num_of(l->t);
        if (shape(r->t) > shape(t)) t = msl_tvec(ty_scal(t), shape(r->t), 0);
        if (IS_SCALAR(t) && t->k < K_INT) t = msl_tscal(K_INT);
    } else t = arith(P, l->t, r->t, op);
    if ((op == '%' || op == '&' || op == '|' || op == '^' || op == 'L' || op == 'R') && !IS_INTK(ty_scal(t)))
        perr(P, "operador inteiro em valor de ponto flutuante");
    node_t *n = mk(P, E_BIN, t);
    n->sub = op;
    n->a = l;
    n->b = r;
    return n;
}
static node_t *binary(P_t *P, int prec) {
    node_t *l = msl_unary(P);
    for (;;) {
        int op;
        int p = bin_prec(cur(P), &op);
        if (!p || p <= prec) return l;
        P->pos++;
        node_t *r = binary(P, p);
        l = msl_binop(P, op, l, r);
    }
}
static node_t *cond_e(P_t *P) {
    node_t *c = binary(P, 0);
    if (!accept(P, "?")) return c;
    node_t *a = assign_expr(P);
    expect(P, ":");
    node_t *b = assign_expr(P);
    ty_t *t = a->t;
    if (is_num(a->t) && is_num(b->t)) t = arith(P, a->t, b->t, '+');
    if (is_num(a->t) && is_num(b->t) && ty_scal(a->t) == K_BOOL && ty_scal(b->t) == K_BOOL) t = a->t;
    node_t *n = mk(P, E_COND, t);
    n->a = c;
    n->b = a;
    n->c = b;
    return n;
}
static node_t *assign_expr(P_t *P) {
    node_t *l = cond_e(P);
    static const struct { const char *s; int op; } ops[] = {{"=", '='}, {"+=", '+'}, {"-=", '-'}, {"*=", '*'}, {"/=", '/'},
        {"%=", '%'}, {"&=", '&'}, {"|=", '|'}, {"^=", '^'}, {"<<=", 'L'}, {">>=", 'R'}, {0, 0}};
    for (int i = 0; ops[i].s; i++)
        if (is(P, ops[i].s)) {
            P->pos++;
            if (!lvalue(l)) perr(P, "atribuição a algo que não é lvalue");
            node_t *r = assign_expr(P);
            node_t *n = mk(P, E_ASSIGN, l->t);
            n->sub = ops[i].op;
            n->a = l;
            n->b = r;
            if (r->op == E_INIT && !r->t) r->t = l->t;
            if (ops[i].op != '=' && (!is_num(l->t) || !is_num(r->t))) perr(P, "atribuição composta em tipo não numérico");
            if (ops[i].op == '*' && l->t->k == K_VEC && r->t->k == K_MAT) {}
            return n;
        }
    return l;
}
static node_t *expr(P_t *P) {
    node_t *e = assign_expr(P);
    while (accept(P, ",")) {
        node_t *n = mk(P, E_COMMA, 0);
        n->a = e;
        n->b = assign_expr(P);
        n->t = n->b->t;
        e = n;
    }
    return e;
}

/* ---- constexpr sampler s(filter::linear, address::repeat, ...) */
static msl_samp_t *parse_sampler_args(P_t *P) {
    msl_samp_t *s = msl_alloc(P->L, sizeof *s);
    s->normalized = 1;
    s->lodmax = 3.4e38f;
    if (!accept(P, "(")) { if (is(P, "{")) { P->pos++; expect(P, "}"); } return s; }
    while (!is(P, ")") && cur(P)->k) {
        char *a = qual_name(P);
        P->pos += qual_len(P);
        const char *v = strip_ns(a);
        const char *key = v, *val = v;
        char kb[64] = {0};
        const char *cc = strstr(v, "::");
        if (cc) { int kn = (int)(cc - v) < 63 ? (int)(cc - v) : 63; memcpy(kb, v, kn); key = kb; val = cc + 2; }
        if (accept(P, "(")) {             /* lod_clamp(a, b), max_anisotropy(n) */
            double x[2] = {0, 0};
            int i = 0;
            while (!is(P, ")") && cur(P)->k) {
                tok_t *t = cur(P);
                if (t->k == TK_FLT || t->k == TK_INT) { if (i < 2) x[i++] = t->k == TK_FLT ? t->f : (double)t->i; }
                P->pos++;
            }
            expect(P, ")");
            if (!strcmp(v, "lod_clamp")) { s->lodmin = (float)x[0]; s->lodmax = (float)x[1]; }
        } else if (accept(P, "=")) {
            /* coord = normalized, filter = linear... */
            char *b = qual_name(P);
            P->pos += qual_len(P);
            key = v;
            val = strip_ns(b);
            const char *c2 = strstr(val, "::");
            if (c2) val = c2 + 2;
        }
        int lin = !strcmp(val, "linear");
        if (!strcmp(key, "filter")) {
            if (!strcmp(val, "linear") || !strcmp(val, "nearest")) {
                if (!strcmp(v, key) || !cc) {}
                s->minf = s->magf = lin;
            }
        } else if (!strcmp(key, "min_filter")) s->minf = lin;
        else if (!strcmp(key, "mag_filter")) s->magf = lin;
        else if (!strcmp(key, "mip_filter")) s->mipf = !strcmp(val, "none") ? 0 : lin ? 2 : 1;
        else if (!strcmp(key, "coord")) s->normalized = strcmp(val, "pixel") != 0;
        else if (!strcmp(key, "compare_func")) {
            static const char *cf[] = {"never", "less", "equal", "less_equal", "greater", "not_equal", "greater_equal", "always", 0};
            for (int i = 0; cf[i]; i++) if (!strcmp(val, cf[i])) s->compare = i;
        } else if (!strcmp(key, "border_color")) s->border = !strcmp(val, "opaque_black") ? 1 : !strcmp(val, "opaque_white") ? 2 : 0;
        else if (!strcmp(key, "address") || !strcmp(key, "s_address") || !strcmp(key, "t_address") || !strcmp(key, "r_address")) {
            static const char *am[] = {"clamp_to_edge", "mirrored_clamp_to_edge", "repeat", "mirrored_repeat", "clamp_to_zero", "clamp_to_border", 0};
            int m = 0;
            for (int i = 0; am[i]; i++) if (!strcmp(val, am[i])) m = i;
            if (key[0] == 'a') s->addr[0] = s->addr[1] = s->addr[2] = m;
            else s->addr[key[0] == 's' ? 0 : key[0] == 't' ? 1 : 2] = m;
        }
        if (!accept(P, ",")) break;
    }
    expect(P, ")");
    return s;
}

/* ================================================================ comandos */
static node_t *stmt(P_t *P);
static node_t *block(P_t *P) {
    node_t *b = mk(P, S_BLOCK, 0);
    int cap = 0, save = P->nscope;
    expect(P, "{");
    while (!is(P, "}")) {
        if (!cur(P)->k) perr(P, "bloco sem '}'");
        push_list(P, &b->v, &b->nv, &cap, stmt(P));
    }
    expect(P, "}");
    P->nscope = save;
    return b;
}
static void fix_init(P_t *P, node_t *init, ty_t *t);
/* tamanho de array implícito: T a[] = {...} */
static ty_t *complete_array(P_t *P, ty_t *t, node_t *init) {
    if (t->k == K_ARRAY && t->n == 0 && init && init->op == E_INIT) return tarr(P->L, t->e, init->nv);
    return t;
}
static void fix_init(P_t *P, node_t *init, ty_t *t) {
    if (!init || init->op != E_INIT) return;
    if (!init->t) init->t = t;
    ty_t *it = init->t;
    for (int i = 0; i < init->nv; i++) {
        node_t *c = init->v[i];
        if (c->op != E_INIT || c->t) continue;
        if (it->k == K_ARRAY) fix_init(P, c, it->e);
        else if (it->k == K_STRUCT && i < it->nf) fix_init(P, c, it->f[i].t);
        else if (it->k == K_MAT) fix_init(P, c, it->e);
        else c->t = it;
    }
}
static node_t *decl(P_t *P, int global) {
    int sp, isc;
    ty_t *base = parse_type(P, &sp, &isc);
    node_t *list = mk(P, S_BLOCK, 0);
    list->sub = 1;                /* bloco sem escopo próprio */
    int cap = 0;
    do {
        ty_t *t = base;
        int ref = 0;
        while (accept(P, "*")) { int s2 = sp; t = tptr(P->L, t, s2); accept(P, "const"); }
        if (accept(P, "&")) ref = 1;
        char *name = ident(P);
        int dims[8], nd = 0;
        while (!at_attr(P) && accept(P, "[")) {
            if (nd >= 8) perr(P, "dimensões demais");
            dims[nd++] = is(P, "]") ? 0 : (int)const_expr(P);
            expect(P, "]");
        }
        for (int i = nd - 1; i >= 0; i--) t = tarr(P->L, t, dims[i]);
        attr_t at = {0};
        parse_attrs(P, &at);
        node_t *init = 0;
        msl_samp_t *samp = 0;
        if (t && t->k == K_SAMP) {
            samp = parse_sampler_args(P);
            if (accept(P, "=")) { (void)assign_expr(P); }
        } else if (accept(P, "=")) {
            init = is(P, "{") ? init_list(P) : assign_expr(P);
        } else if (is(P, "{")) {
            init = init_list(P);
        } else if (is(P, "(") && t && IS_NUM(t)) {
            node_t *x = mk(P, E_CTOR, t);
            call_args(P, x);
            init = x;
        }
        if (!t) {                    /* auto */
            if (!init || !init->t) perr(P, "auto sem inicializador");
            t = init->t;
        }
        t = complete_array(P, t, init);
        fix_init(P, init, t);
        var_t *v = new_var(P, name, t);
        v->ref = ref;
        if (global || (sp == SP_CONSTANT && !P->cur)) {
            v->where = 1;
            v->off = alloc_global(P, t->size, t->align);
            if (samp) *(msl_samp_t **)(P->L->gmem + v->off) = samp;
            glob_push(P, v);
            if (init) msl_global_init(P->L, init, t, P->L->gmem + v->off);
            continue;
        }
        if (sp == SP_THREADGROUP && P->cur) {
            v->where = 2;
            P->tgoff = (P->tgoff + (t->align ? t->align : 1) - 1) & ~(u32)((t->align ? t->align : 1) - 1);
            v->off = (int)P->tgoff;
            P->tgoff += t->size;
            scope_push(P, v);
            continue;
        }
        if (samp) {
            v->where = 1;
            v->off = alloc_global(P, 8, 8);
            *(msl_samp_t **)(P->L->gmem + v->off) = samp;
            scope_push(P, v);
            continue;
        }
        v->off = alloc_frame(P, ref ? 8 : t->size, ref ? 8 : t->align);
        node_t *d = mk(P, S_DECL, t);
        d->var = v;
        d->a = init;
        if (ref && (!init || !lvalue(init))) perr(P, "referência local precisa de um lvalue");
        scope_push(P, v);
        push_list(P, &list->v, &list->nv, &cap, d);
    } while (accept(P, ","));
    expect(P, ";");
    return list;
}
static node_t *stmt(P_t *P) {
    if (is(P, "{")) return block(P);
    if (accept(P, ";")) return mk(P, S_NOP, 0);
    if (accept(P, "if")) {
        node_t *n = mk(P, S_IF, 0);
        expect(P, "(");
        n->a = expr(P);
        expect(P, ")");
        int save = P->nscope;
        n->b = stmt(P);
        P->nscope = save;
        if (accept(P, "else")) { n->c = stmt(P); P->nscope = save; }
        return n;
    }
    if (accept(P, "for")) {
        node_t *n = mk(P, S_FOR, 0);
        int save = P->nscope;
        expect(P, "(");
        if (!accept(P, ";")) {
            if (is_type_start(P)) n->a = decl(P, 0);
            else { node_t *e = mk(P, S_EXPR, 0); e->a = expr(P); n->a = e; expect(P, ";"); }
        }
        if (!is(P, ";")) n->b = expr(P);
        expect(P, ";");
        if (!is(P, ")")) n->c = expr(P);
        expect(P, ")");
        n->d = stmt(P);
        P->nscope = save;
        return n;
    }
    if (accept(P, "while")) {
        node_t *n = mk(P, S_WHILE, 0);
        expect(P, "(");
        n->a = expr(P);
        expect(P, ")");
        int save = P->nscope;
        n->b = stmt(P);
        P->nscope = save;
        return n;
    }
    if (accept(P, "do")) {
        node_t *n = mk(P, S_DO, 0);
        int save = P->nscope;
        n->b = stmt(P);
        P->nscope = save;
        expect(P, "while");
        expect(P, "(");
        n->a = expr(P);
        expect(P, ")");
        expect(P, ";");
        return n;
    }
    if (accept(P, "switch")) {
        node_t *n = mk(P, S_SWITCH, 0);
        expect(P, "(");
        n->a = expr(P);
        expect(P, ")");
        expect(P, "{");
        int cap = 0, nc = 0, capc = 0, save = P->nscope;
        n->sub = -1;
        i64 *cs = 0;
        while (!is(P, "}")) {
            if (!cur(P)->k) perr(P, "switch sem '}'");
            if (accept(P, "case")) {
                i64 v = const_expr(P);
                expect(P, ":");
                if (nc + 2 > capc) { capc = capc ? capc * 2 : 16; i64 *x = msl_alloc(P->L, capc * sizeof *x); if (nc) memcpy(x, cs, nc * sizeof *x); cs = x; }
                cs[nc++] = v;
                cs[nc++] = n->nv;
                continue;
            }
            if (accept(P, "default")) { expect(P, ":"); n->sub = n->nv; continue; }
            push_list(P, &n->v, &n->nv, &cap, stmt(P));
        }
        expect(P, "}");
        P->nscope = save;
        n->cases = cs;
        n->off = nc / 2;
        return n;
    }
    if (accept(P, "return")) {
        node_t *n = mk(P, S_RET, 0);
        if (!is(P, ";")) {
            n->a = is(P, "{") ? init_list(P) : expr(P);
            if (n->a->op == E_INIT && !n->a->t && P->cur) { n->a->t = P->cur->ret; fix_init(P, n->a, P->cur->ret); }
        }
        expect(P, ";");
        return n;
    }
    if (accept(P, "break")) { expect(P, ";"); return mk(P, S_BREAK, 0); }
    if (accept(P, "continue")) { expect(P, ";"); return mk(P, S_CONT, 0); }
    if (is_type_start(P)) {
        /* "float4(...)" como expressão começa igual a uma declaração: decide pelo token depois do tipo */
        int save = P->pos;
        int sp, c;
        int istype = 1;
        ty_t *t = parse_type(P, &sp, &c);
        (void)t;
        if (is(P, "(") || is(P, "{") || is(P, ".") || is(P, ";")) istype = 0;
        P->pos = save;
        if (istype) return decl(P, 0);
    }
    node_t *n = mk(P, S_EXPR, 0);
    n->a = expr(P);
    expect(P, ";");
    return n;
}

/* ================================================================ declarações de topo */
static void struct_body(P_t *P, ty_t *t) {
    int cap = 0, off = 0, al = 1;
    expect(P, "{");
    while (!accept(P, "}")) {
        if (!cur(P)->k) perr(P, "struct sem '}'");
        if (accept(P, ";")) continue;
        int sp, c;
        ty_t *base = parse_type(P, &sp, &c);
        if (!base) perr(P, "membro sem tipo");
        do {
            ty_t *ft = base;
            while (accept(P, "*")) ft = tptr(P->L, ft, sp);
            char *name = ident(P);
            if (is(P, "(")) perr(P, "métodos em struct não são suportados");
            int dims[8], nd = 0;
            while (!at_attr(P) && accept(P, "[")) { dims[nd++] = (int)const_expr(P); expect(P, "]"); if (nd >= 8) break; }
            for (int i = nd - 1; i >= 0; i--) ft = tarr(P->L, ft, dims[i]);
            attr_t at = {0};
            parse_attrs(P, &at);
            if (accept(P, "=")) (void)assign_expr(P);   /* inicializador padrão: ignorado */
            if (t->nf == cap) {
                cap = cap ? cap * 2 : 8;
                fld_t *nf = msl_alloc(P->L, cap * sizeof *nf);
                if (t->nf) memcpy(nf, t->f, t->nf * sizeof *nf);
                t->f = nf;
            }
            int a = ft->align ? ft->align : 1;
            off = (off + a - 1) & ~(a - 1);
            fld_t *f = &t->f[t->nf++];
            f->name = name;
            f->t = ft;
            f->off = off;
            f->attr = at.attr;
            f->idx = at.idx;
            f->interp = at.interp;
            f->user = at.user;
            off += ft->size;
            if (a > al) al = a;
        } while (accept(P, ","));
        expect(P, ";");
    }
    t->align = al;
    t->size = (off + al - 1) & ~(al - 1);
    if (!t->size) t->size = al;
}
static void enum_body(P_t *P, ty_t *et) {
    i64 v = 0;
    expect(P, "{");
    while (!accept(P, "}")) {
        char *name = ident(P);
        if (accept(P, "=")) v = const_expr(P);
        sym_add(P, name, et, 2, v);
        v++;
        if (!accept(P, ",")) { expect(P, "}"); break; }
    }
}
static void fn_fields(P_t *P, ty_t *t, int attr0, msl_field_t **out, int *n) {
    if (t->k == K_VOID) { *n = 0; return; }
    if (t->k != K_STRUCT) {
        msl_field_t *f = msl_alloc(P->L, sizeof *f);
        f->name = "";
        f->size = t->size;
        f->scal = ty_scal(t);
        f->n = ty_ncomp(t);
        f->attr = attr0;
        *out = f;
        *n = 1;
        return;
    }
    msl_field_t *f = msl_alloc(P->L, (t->nf ? t->nf : 1) * sizeof *f);
    for (int i = 0; i < t->nf; i++) {
        f[i].name = t->f[i].name;
        f[i].user = t->f[i].user;
        f[i].off = t->f[i].off;
        f[i].size = t->f[i].t->size;
        f[i].scal = IS_NUM(t->f[i].t) ? ty_scal(t->f[i].t) : 0;
        f[i].n = IS_NUM(t->f[i].t) ? ty_ncomp(t->f[i].t) : 0;
        f[i].attr = t->f[i].attr;
        f[i].idx = t->f[i].idx;
        f[i].interp = t->f[i].interp;
    }
    *out = f;
    *n = t->nf;
}
static void add_fn(msl_lib_t *L, msl_fn_t *f) {
    if (L->nfn == L->capfn) { L->capfn = L->capfn ? L->capfn * 2 : 16; L->fns = realloc(L->fns, L->capfn * sizeof *L->fns); }
    L->fns[L->nfn++] = f;
}
static void function(P_t *P, ty_t *ret, int stage, char *name) {
    msl_fn_t *f = msl_alloc(P->L, sizeof *f);
    f->name = name;
    f->stage = stage;
    f->ret = ret;
    f->lib = P->L;
    P->cur = f;
    P->frame = 0;
    P->tgoff = 0;
    P->nscope = 0;
    expect(P, "(");
    var_t *ps[64];
    int np = 0;
    if (!is(P, ")") && !(is(P, "void") && tok_eq(peek(P, 1), ")"))) {
        do {
            int sp, c;
            ty_t *t = parse_type(P, &sp, &c);
            if (!t) perr(P, "parâmetro sem tipo");
            int ref = 0;
            while (accept(P, "*")) { t = tptr(P->L, t, sp); accept(P, "const"); }
            if (accept(P, "&")) ref = 1;
            char *pn = is(P, ",") || is(P, ")") || at_attr(P) ? "_" : ident(P);
            while (!at_attr(P) && accept(P, "[")) { int d = (int)const_expr(P); expect(P, "]"); t = tarr(P->L, t, d); }
            attr_t at = {0};
            parse_attrs(P, &at);
            if (accept(P, "=")) (void)assign_expr(P);
            var_t *v = new_var(P, pn, t);
            v->ref = ref;
            v->attr = at.attr;
            v->idx = at.idx;
            v->user = at.user;
            if (sp == SP_THREADGROUP && t->k == K_PTR && !v->attr) v->attr = A_THREADGROUP;
            v->off = alloc_frame(P, ref ? 8 : t->size, ref ? 8 : t->align);
            if (np >= 64) perr(P, "parâmetros demais");
            ps[np++] = v;
            scope_push(P, v);
        } while (accept(P, ","));
    } else accept(P, "void");
    expect(P, ")");
    attr_t fa = {0};
    parse_attrs(P, &fa);
    f->np = np;
    f->params = msl_alloc(P->L, (np ? np : 1) * sizeof *f->params);
    memcpy(f->params, ps, np * sizeof *ps);
    /* protótipo já declarado: completa aquele */
    msl_fn_t *proto = 0;
    for (int i = 0; i < P->L->nfn; i++) {
        msl_fn_t *g = P->L->fns[i];
        if (!strcmp(g->name, name) && g->np == np && !g->defined) {
            int same = 1;
            for (int j = 0; j < np; j++) if (g->params[j]->t != ps[j]->t) same = 0;
            if (same) proto = g;
        }
    }
    if (accept(P, ";")) {
        if (!proto) { f->defined = 0; add_fn(P->L, f); }
        P->cur = 0;
        return;
    }
    if (proto) {
        /* reaproveita o objeto do protótipo (chamadas já apontam para ele) */
        msl_fn_t keep = *proto;
        *proto = *f;
        (void)keep;
        f = proto;
        P->cur = f;
    } else add_fn(P->L, f);
    f->defined = 1;
    if (ret->k != K_VOID) f->retoff = alloc_frame(P, ret->size, ret->align);
    f->body = block(P);
    f->frame = P->frame;
    f->tgstatic = P->tgoff;
    if (stage == MSL_VERTEX || stage == MSL_FRAGMENT) {
        fn_fields(P, ret, stage == MSL_VERTEX ? A_POSITION : A_COLOR, &f->out, &f->nout);
        f->outsize = ret->size;
        for (int i = 0; i < np; i++)
            if (ps[i]->attr == A_STAGE_IN && ps[i]->t->k == K_STRUCT) {
                fn_fields(P, ps[i]->t, 0, &f->in, &f->nin);
                f->insize = ps[i]->t->size;
            }
    }
    P->cur = 0;
    P->nscope = 0;
}
static void top(P_t *P) {
    if (accept(P, ";")) return;
    if (accept(P, "using")) {
        if (accept(P, "namespace")) { while (!accept(P, ";")) { if (!cur(P)->k) return; P->pos++; } return; }
        char *name = ident(P);
        expect(P, "=");
        ty_t *t = parse_targ(P);
        while (accept(P, "*")) t = tptr(P->L, t, 0);
        expect(P, ";");
        sym_add(P, name, t, 1, 0);
        return;
    }
    if (accept(P, "namespace")) {
        if (cur(P)->k == TK_ID) P->pos++;
        expect(P, "{");
        while (!accept(P, "}")) { if (!cur(P)->k) perr(P, "namespace sem '}'"); top(P); }
        return;
    }
    if (is(P, "template")) perr(P, "templates não são suportados");
    int td = accept(P, "typedef");
    if (is(P, "struct") && (peek(P, 1)->k == TK_ID || tok_eq(peek(P, 1), "{")) &&
        (tok_eq(peek(P, 2), "{") || tok_eq(peek(P, 1), "{") || tok_eq(peek(P, 2), ";"))) {
        P->pos++;
        char *name = is(P, "{") ? 0 : ident(P);
        ty_t *t = 0;
        if (name) { sym_t *y = sym_find(P, name); if (y && y->kind == 1 && y->t->k == K_STRUCT && !y->t->size) t = y->t; }
        if (!t) { t = msl_alloc(P->L, sizeof *t); t->k = K_STRUCT; t->name = name; if (name) sym_add(P, name, t, 1, 0); }
        if (accept(P, ";")) return;     /* declaração adiantada */
        struct_body(P, t);
        if (td) {
            do { char *alias = ident(P); sym_add(P, alias, t, 1, 0); } while (accept(P, ","));
            expect(P, ";");
            return;
        }
        if (!accept(P, ";")) {
            /* struct S {...} var; — raro em MSL */
            perr(P, "esperava ';' depois da struct");
        }
        return;
    }
    if (is(P, "enum")) {
        P->pos++;
        accept(P, "class");
        accept(P, "struct");
        char *name = cur(P)->k == TK_ID ? ident(P) : 0;
        ty_t *et = msl_tscal(K_INT);
        if (accept(P, ":")) et = parse_targ(P);
        if (name) sym_add(P, name, et, 1, 0);
        if (is(P, "{")) enum_body(P, et);
        if (td && cur(P)->k == TK_ID) { char *alias = ident(P); sym_add(P, alias, et, 1, 0); }
        expect(P, ";");
        return;
    }
    if (td) {
        int sp, c;
        ty_t *t = parse_type(P, &sp, &c);
        while (accept(P, "*")) t = tptr(P->L, t, sp);
        char *alias = ident(P);
        while (!at_attr(P) && accept(P, "[")) { int d = (int)const_expr(P); expect(P, "]"); t = tarr(P->L, t, d); }
        parse_attrs(P, &(attr_t){0});
        expect(P, ";");
        sym_add(P, alias, t, 1, 0);
        return;
    }
    attr_t a0 = {0};
    parse_attrs(P, &a0);
    int stage = a0.stage;
    if (accept(P, "vertex")) stage = MSL_VERTEX;
    else if (accept(P, "fragment")) stage = MSL_FRAGMENT;
    else if (accept(P, "kernel")) stage = MSL_KERNEL;
    /* decide entre função e variável global */
    int save = P->pos;
    int sp, c;
    ty_t *t = parse_type(P, &sp, &c);
    attr_t a1 = {0};
    parse_attrs(P, &a1);
    if (a1.stage) stage = a1.stage;
    while (t && accept(P, "*")) t = tptr(P->L, t, sp);
    accept(P, "&");
    if (cur(P)->k == TK_ID && tok_eq(peek(P, 1), "(") && (!t || t->k != K_SAMP)) {
        char *name = ident(P);
        if (!t) perr(P, "função sem tipo de retorno");
        function(P, t, stage, name);
        return;
    }
    P->pos = save;
    (void)decl(P, 1);
}

/* ================================================================ API */
msl_lib_t *msl_compile(const char *src, u64 n, const char *defines, char *err, int errn) {
    msl_lib_t *L = calloc(1, sizeof *L);
    pp_t pp = {0};
    pp.L = L;
    pp.err = err;
    pp.errn = errn;
    if (err && errn) err[0] = 0;
    tvec_t raw = {0}, toks = {0};
    P_t P = {0};
    if (__builtin_setjmp(pp.jb)) {
        free(raw.v);
        free(toks.v);
        for (int i = 0; i < pp.nbufs; i++) free(pp.bufs[i]);
        for (int i = 0; i < pp.keep.n; i++) free((void *)pp.keep.v[i].p);
        free(pp.keep.v);
        msl_lib_free(L);
        return 0;
    }
    mdef_str(&pp, "__METAL_VERSION__", "310");
    mdef_str(&pp, "__METAL_MACOS__", "1");
    mdef_str(&pp, "__METAL__", "1");
    mdef_str(&pp, "__HAVE_MESH__", "0");
    mdef_str(&pp, "__METAL_IOS__", "0");
    if (defines) {
        /* "NOME=VALOR\n..." */
        const char *s = defines;
        while (*s) {
            const char *e = strchr(s, '\n');
            if (!e) e = s + strlen(s);
            const char *eq = memchr(s, '=', e - s);
            char *name = adup(L, s, (int)((eq ? eq : e) - s));
            char *val = eq ? adup(L, eq + 1, (int)(e - eq - 1)) : "1";
            if (name[0]) mdef_str(&pp, name, val);
            s = *e ? e + 1 : e;
        }
    }
    lex(src, n, &raw);
    preprocess(&pp, raw.v, raw.n, &toks, 0);
    P.pp = &pp;
    P.L = L;
    P.t = toks.v;
    P.n = toks.n;
    while (P.pos < P.n) top(&P);
    for (int i = 0; i < L->nfn; i++)
        if (!L->fns[i]->defined) {
            /* protótipo sem corpo só é erro se for chamado: o interpretador avisa */
        }
    /* threadgroup_barrier em qualquer função conta para todos os kernels (pode vir de uma auxiliar) */
    int anybar = 0, anydisc = 0;
    for (int i = 0; i < L->nfn; i++) { anybar |= L->fns[i]->barrier; anydisc |= L->fns[i]->discard; }
    for (int i = 0; i < L->nfn; i++) {
        if (L->fns[i]->stage == MSL_KERNEL) L->fns[i]->barrier |= anybar;
        if (L->fns[i]->stage == MSL_FRAGMENT) L->fns[i]->discard |= anydisc;
    }
    free(raw.v);
    free(toks.v);
    for (int i = 0; i < pp.nbufs; i++) free(pp.bufs[i]);
    for (int i = 0; i < pp.keep.n; i++) free((void *)pp.keep.v[i].p);
    free(pp.keep.v);
    for (macro_t *m = pp.macros, *nx; m; m = nx) { nx = m->next; free(m); }
    return L;
}
void msl_lib_free(msl_lib_t *L) {
    if (!L) return;
    for (arena_blk_t *b = L->arena, *n; b; b = n) { n = b->next; free(b); }
    free(L->fns);
    free(L->gmem);
    free(L);
}
int msl_fn_count(msl_lib_t *l) { return l ? l->nfn : 0; }
msl_fn_t *msl_fn_at(msl_lib_t *l, int i) { return l && i >= 0 && i < l->nfn ? l->fns[i] : 0; }
msl_fn_t *msl_find(msl_lib_t *l, const char *name) {
    if (!l) return 0;
    for (int i = 0; i < l->nfn; i++) if (l->fns[i]->stage && !strcmp(l->fns[i]->name, name)) return l->fns[i];
    for (int i = 0; i < l->nfn; i++) if (!strcmp(l->fns[i]->name, name) && l->fns[i]->defined) return l->fns[i];
    return 0;
}
const char *msl_fn_name(msl_fn_t *f) { return f->name; }
int msl_fn_stage(msl_fn_t *f) { return f->stage; }
int msl_fn_out(msl_fn_t *f, const msl_field_t **fields, int *size) { *fields = f->out; *size = f->outsize; return f->nout; }
int msl_fn_stagein(msl_fn_t *f, const msl_field_t **fields, int *size) { *fields = f->in; *size = f->insize; return f->nin; }
u32 msl_fn_tgstatic(msl_fn_t *f) { return f->tgstatic; }
int msl_fn_barrier(msl_fn_t *f) { return f->barrier; }
int msl_fn_discards(msl_fn_t *f) { return f->discard; }

/* ---- tabela das embutidas */
static int bi_lookup(const char *s, int *rule) {
    static const struct { const char *s; int b, r; } t[] = {
        {"sin", B_SIN, R_GENF}, {"cos", B_COS, R_GENF}, {"tan", B_TAN, R_GENF}, {"asin", B_ASIN, R_GENF}, {"acos", B_ACOS, R_GENF},
        {"atan", B_ATAN, R_GENF}, {"sinh", B_SINH, R_GENF}, {"cosh", B_COSH, R_GENF}, {"tanh", B_TANH, R_GENF},
        {"asinh", B_ASINH, R_GENF}, {"acosh", B_ACOSH, R_GENF}, {"atanh", B_ATANH, R_GENF}, {"exp", B_EXP, R_GENF},
        {"exp2", B_EXP2, R_GENF}, {"exp10", B_EXP10, R_GENF}, {"log", B_LOG, R_GENF}, {"log2", B_LOG2, R_GENF},
        {"log10", B_LOG10, R_GENF}, {"sqrt", B_SQRT, R_GENF}, {"rsqrt", B_RSQRT, R_GENF}, {"floor", B_FLOOR, R_GENF},
        {"ceil", B_CEIL, R_GENF}, {"round", B_ROUND, R_GENF}, {"rint", B_RINT, R_GENF}, {"trunc", B_TRUNC, R_GENF},
        {"fract", B_FRACT, R_GENF}, {"saturate", B_SATURATE, R_GENF}, {"sign", B_SIGN, R_GENF}, {"abs", B_ABS, R_GEN},
        {"fabs", B_ABS, R_GENF}, {"sinpi", B_SINPI, R_GENF}, {"cospi", B_COSPI, R_GENF}, {"tanpi", B_TANPI, R_GENF},
        {"pow", B_POW, R_GENF}, {"powr", B_POW, R_GENF}, {"atan2", B_ATAN2, R_GENF}, {"fmod", B_FMOD, R_GENF},
        {"min", B_MIN, R_GEN}, {"max", B_MAX, R_GEN}, {"fmin", B_MIN, R_GENF}, {"fmax", B_MAX, R_GENF},
        {"step", B_STEP, R_GENF}, {"copysign", B_COPYSIGN, R_GENF}, {"fdim", B_FDIM, R_GENF}, {"clamp", B_CLAMP, R_GEN},
        {"mix", B_MIX, R_GENF}, {"smoothstep", B_SMOOTHSTEP, R_GENF}, {"fma", B_FMA, R_GENF}, {"mad", B_FMA, R_GENF},
        {"select", B_SELECT, R_GEN}, {"dot", B_DOT, R_SCALAR}, {"length", B_LENGTH, R_SCALAR},
        {"length_squared", B_LENGTH2, R_SCALAR}, {"distance", B_DISTANCE, R_SCALAR}, {"distance_squared", B_DISTANCE2, R_SCALAR},
        {"normalize", B_NORMALIZE, R_SAME}, {"cross", B_CROSS, R_SAME}, {"reflect", B_REFLECT, R_SAME},
        {"refract", B_REFRACT, R_SAME}, {"faceforward", B_FACEFORWARD, R_SAME}, {"any", B_ANY, R_BOOL}, {"all", B_ALL, R_BOOL},
        {"isnan", B_ISNAN, R_BOOLV}, {"isinf", B_ISINF, R_BOOLV}, {"isfinite", B_ISFINITE, R_BOOLV},
        {"signbit", B_SIGNBIT, R_BOOLV}, {"transpose", B_TRANSPOSE, R_TRANSPOSE}, {"determinant", B_DETERMINANT, R_DET},
        {"popcount", B_POPCOUNT, R_SAME}, {"clz", B_CLZ, R_SAME}, {"ctz", B_CTZ, R_SAME}, {"reverse_bits", B_REVERSE_BITS, R_SAME},
        {"mulhi", B_MULHI, R_GEN}, {"rotate", B_ROTATE, R_GEN}, {"absdiff", B_ABSDIFF, R_GEN}, {"hadd", B_HADD, R_GEN},
        {"extract_bits", B_EXTRACT_BITS, R_SAME}, {"insert_bits", B_INSERT_BITS, R_SAME},
        {"dfdx", B_DFDX, R_SAME}, {"dfdy", B_DFDY, R_SAME}, {"fwidth", B_FWIDTH, R_SAME},
        {"discard_fragment", B_DISCARD, R_VOID}, {"threadgroup_barrier", B_BARRIER, R_VOID},
        {"simdgroup_barrier", B_SIMD_BARRIER, R_VOID}, {"atomic_thread_fence", B_SIMD_BARRIER, R_VOID},
        {"atomic_load_explicit", B_ATOMIC_LOAD, R_ATOMIC}, {"atomic_store_explicit", B_ATOMIC_STORE, R_ATOMIC},
        {"atomic_exchange_explicit", B_ATOMIC_XCHG, R_ATOMIC}, {"atomic_compare_exchange_weak_explicit", B_ATOMIC_CAS, R_ATOMIC},
        {"atomic_fetch_add_explicit", B_ATOMIC_ADD, R_ATOMIC}, {"atomic_fetch_sub_explicit", B_ATOMIC_SUB, R_ATOMIC},
        {"atomic_fetch_min_explicit", B_ATOMIC_MIN, R_ATOMIC}, {"atomic_fetch_max_explicit", B_ATOMIC_MAX, R_ATOMIC},
        {"atomic_fetch_and_explicit", B_ATOMIC_AND, R_ATOMIC}, {"atomic_fetch_or_explicit", B_ATOMIC_OR, R_ATOMIC},
        {"atomic_fetch_xor_explicit", B_ATOMIC_XOR, R_ATOMIC},
        {"pack_float_to_unorm4x8", B_PACK_UNORM4X8, R_UINT}, {"pack_float_to_snorm4x8", B_PACK_SNORM4X8, R_UINT},
        {"pack_float_to_unorm2x16", B_PACK_UNORM2X16, R_UINT}, {"pack_float_to_snorm2x16", B_PACK_SNORM2X16, R_UINT},
        {"pack_half_to_unorm2x16", B_PACK_UNORM2X16, R_UINT},
        {"unpack_unorm4x8_to_float", B_UNPACK_UNORM4X8, R_F4}, {"unpack_snorm4x8_to_float", B_UNPACK_SNORM4X8, R_F4},
        {"unpack_unorm4x8_to_half", B_UNPACK_UNORM4X8, R_F4}, {"unpack_unorm2x16_to_float", B_UNPACK_UNORM2X16, R_F2},
        {"unpack_snorm2x16_to_float", B_UNPACK_SNORM2X16, R_F2},
        {"level", B_LEVEL, R_OPT}, {"bias", B_BIAS, R_OPT}, {"gradient2d", B_GRAD, R_OPT}, {"gradientcube", B_GRAD, R_OPT},
        {"gradient3d", B_GRAD, R_OPT}, {"min_lod_clamp", B_MINLOD, R_OPT},
        {"simd_broadcast_first", B_SIMD_IDENT, R_SAME}, {"simd_broadcast", B_SIMD_IDENT, R_SAME},
        {"simd_shuffle", B_SIMD_IDENT, R_SAME}, {"simd_max", B_SIMD_IDENT, R_SAME}, {"simd_min", B_SIMD_IDENT, R_SAME},
        {"simd_sum", B_SIMD_SUM, R_SAME}, {"simd_prefix_exclusive_sum", B_SIMD_SUM, R_SAME},
        {"quad_broadcast", B_SIMD_IDENT, R_SAME}, {0, 0, 0}};
    for (int i = 0; t[i].s; i++) if (!strcmp(t[i].s, s)) { *rule = t[i].r; return t[i].b; }
    return 0;
}
