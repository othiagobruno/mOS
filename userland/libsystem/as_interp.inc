/* mOS libSystem — interpretador de AppleScript (o subconjunto que scripts e apps usam), sem CF nem ObjC.
 * Incluído por applescript.c (NSAppleScript, /usr/bin/osascript) e tools/astest.c. As ações do sistema vêm de
 * as_host (enviar Apple Event, shell, diálogo, notificação, área de transferência, teclas, apps e processos).
 *
 * Linguagem: comentários -- # (* *), ¬ continua a linha; valores: números, "texto", true/false, missing value, listas
 * {..}, registros {nome:valor}; set/copy, if/else if/else, repeat (N times, with i from a to b [by c], with x in l,
 * while, until, para sempre), exit repeat, return, try/on error [msg] [number n], error "x" [number n], log, delay;
 * operadores & + - * / div mod ^, = ≠ is, is not, < > ≤ ≥, contains, does not contain, starts with, ends with,
 * is in, and, or, not, as (text, integer, real, number, list, boolean); acesso: item N of, items a thru b of,
 * character N of, characters of, word(s) of, paragraph(s) of, text items of (text item delimiters), first/last,
 * length of, count (of), number of, reverse of, rest of, every item of; the clipboard, current date, path to
 * <pasta>, POSIX path of, POSIX file, quoted form of, id/version/name/running of application "X", result.
 * Comandos: tell application "X" ... end tell (activate, quit, launch, run, open, open location: Apple Events para o
 * app; outros comandos dão o erro -1708 "não entende"), display dialog/alert/notification, do shell script, beep,
 * say, choose from list; dentro de tell application "System Events": keystroke, key code [using {...down}],
 * name/exists de process, frontmost. Handlers do usuário: on f(a, b) ... end f, chamada f(x, y). */

enum { AV_MISSING = 0, AV_BOOL, AV_NUM, AV_TEXT, AV_LIST, AV_RECORD, AV_APP, AV_FILE, AV_CONST, AV_DATE };
typedef struct as_val {
    int kind;
    double num;
    int is_int;
    char *s;                         /* texto, nome do app, caminho, constante */
    struct as_val *items;            /* lista / registro */
    char **keys;                     /* registro */
    int n;
} as_val;

typedef struct as_host {
    void *ctx;
    /* Apple Event para o app: cls/id em 4 letras; direct pode ser NULL. 0 = ok (result preenchido), senão o número
     * do erro e a mensagem em err */
    int (*send)(void *ctx, const char *app, unsigned cls, unsigned id, const as_val *direct, as_val *result, char *err, int errcap);
    int (*shell)(void *ctx, const char *cmd, char **out, int *status);
    /* diálogo: devolve o índice do botão escolhido (0..), -1 = cancelado, -2 = esgotou o tempo */
    int (*dialog)(void *ctx, const char *text, const char *title, const char **buttons, int nb, int def, int giving_up,
                  const char *answer_in, char **answer_out);
    void (*notify)(void *ctx, const char *text, const char *title, const char *subtitle, const char *sound);
    char *(*clip_get)(void *ctx);
    void (*clip_set)(void *ctx, const char *text);
    int (*keys)(void *ctx, const char *text, int keycode, unsigned mods);   /* 0 = ok; -1719 sem acessibilidade */
    /* info de app: what = "id", "version", "name", "running", "path" -> texto (malloc) ou NULL se não existe */
    char *(*app_info)(void *ctx, const char *app, const char *what);
    char *(*path_to)(void *ctx, const char *which);
    char **(*processes)(void *ctx, int *n);               /* nomes dos processos com janela (System Events) */
    int (*frontmost)(void *ctx, const char *process);      /* traz para a frente; 0 = ok */
    void (*log)(void *ctx, const char *text);
    void (*delay)(void *ctx, double seconds);
    char *(*choose)(void *ctx, const char **items, int n, const char *prompt);   /* NULL = cancelou */
    double (*now)(void *ctx);                              /* CFAbsoluteTime */
} as_host;

/* ---------------------------------------------------------------- valores */
static char *as_strdup(const char *s) { usize n = strlen(s ? s : ""); char *r = malloc(n + 1); if (r) memcpy(r, s ? s : "", n + 1); return r; }
static void as_free(as_val *v) {
    if (!v) return;
    free(v->s);
    for (int i = 0; i < v->n; i++) { as_free(&v->items[i]); if (v->keys) free(v->keys[i]); }
    free(v->items);
    free(v->keys);
    memset(v, 0, sizeof *v);
}
static as_val as_num(double d) { as_val v = {AV_NUM, d, d == (double)(long long)d && d > -1e15 && d < 1e15, 0, 0, 0, 0}; return v; }
static as_val as_real(double d) { as_val v = as_num(d); v.is_int = 0; return v; }
static as_val as_txt(const char *s) { as_val v = {AV_TEXT, 0, 0, as_strdup(s), 0, 0, 0}; return v; }
static as_val as_txt_take(char *s) { as_val v = {AV_TEXT, 0, 0, s ? s : as_strdup(""), 0, 0, 0}; return v; }
static as_val as_boolv(int b) { as_val v = {AV_BOOL, b ? 1 : 0, 1, 0, 0, 0, 0}; return v; }
static as_val as_missing(void) { as_val v; memset(&v, 0, sizeof v); return v; }
static as_val as_copy(const as_val *s) {
    as_val v = *s;
    v.s = s->s ? as_strdup(s->s) : 0;
    v.items = 0; v.keys = 0;
    if (s->n) {
        v.items = calloc((usize)s->n, sizeof *v.items);
        if (s->keys) v.keys = calloc((usize)s->n, sizeof *v.keys);
        for (int i = 0; i < s->n; i++) { v.items[i] = as_copy(&s->items[i]); if (s->keys) v.keys[i] = as_strdup(s->keys[i]); }
    }
    return v;
}
static void as_list_add(as_val *l, as_val it) {
    as_val *ni = realloc(l->items, sizeof *ni * (usize)(l->n + 1));
    if (!ni) { as_free(&it); return; }
    l->items = ni;
    l->items[l->n++] = it;
}
static void as_rec_set(as_val *r, const char *k, as_val it) {
    for (int i = 0; i < r->n; i++) if (!strcmp(r->keys[i], k)) { as_free(&r->items[i]); r->items[i] = it; return; }
    as_val *ni = realloc(r->items, sizeof *ni * (usize)(r->n + 1));
    char **nk = ni ? realloc(r->keys, sizeof *nk * (usize)(r->n + 1)) : 0;
    if (!ni || !nk) { if (ni) r->items = ni; as_free(&it); return; }
    r->items = ni; r->keys = nk;
    r->keys[r->n] = as_strdup(k);
    r->items[r->n++] = it;
}
typedef struct { char *b; usize n, cap; } as_sb;
static void sb_add(as_sb *s, const char *t, usize n) {
    if (s->n + n + 1 > s->cap) { usize nc = s->cap ? s->cap * 2 : 64; while (nc < s->n + n + 1) nc *= 2; s->b = realloc(s->b, nc); s->cap = nc; }
    memcpy(s->b + s->n, t, n); s->n += n; s->b[s->n] = 0;
}
static void sb_str(as_sb *s, const char *t) { sb_add(s, t, strlen(t)); }
static void as_fmt_number(double d, int is_int, char *b, usize cap) {
    if (is_int && d == (double)(long long)d) snprintf(b, cap, "%lld", (long long)d);
    else {
        snprintf(b, cap, "%.12g", d);
        if (!strchr(b, '.') && !strchr(b, 'e') && !strchr(b, 'E') && !strchr(b, 'n') && !strchr(b, 'i')) { usize l = strlen(b); if (l + 2 < cap) { b[l] = '.'; b[l + 1] = '0'; b[l + 2] = 0; } }
    }
}
/* forma "legível" (o que o osascript imprime) ou a de código (source = 1: textos entre aspas) */
static void as_repr(const as_val *v, as_sb *s, int source) {
    char b[64];
    switch (v->kind) {
    case AV_MISSING: sb_str(s, "missing value"); break;
    case AV_BOOL: sb_str(s, v->num ? "true" : "false"); break;
    case AV_NUM: as_fmt_number(v->num, v->is_int, b, sizeof b); sb_str(s, b); break;
    case AV_TEXT:
        if (source) { sb_str(s, "\""); for (const char *p = v->s; *p; p++) { if (*p == '"' || *p == '\\') sb_add(s, "\\", 1); sb_add(s, p, 1); } sb_str(s, "\""); }
        else sb_str(s, v->s);
        break;
    case AV_CONST: sb_str(s, v->s); break;
    case AV_APP: sb_str(s, "application \""); sb_str(s, v->s); sb_str(s, "\""); break;
    case AV_FILE: sb_str(s, source ? "file \"" : ""); sb_str(s, v->s); if (source) sb_str(s, "\""); break;
    case AV_DATE: {
        long long t = (long long)v->num + 978307200LL, days = t / 86400, rem = t % 86400;
        if (rem < 0) { rem += 86400; days--; }
        long long z = days + 719468, era = (z >= 0 ? z : z - 146096) / 146097, doe = z - era * 146097;
        long long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365, y = yoe + era * 400, doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
        long long mp = (5 * doy + 2) / 153, d = doy - (153 * mp + 2) / 5 + 1, m = mp < 10 ? mp + 3 : mp - 9;
        if (m <= 2) y++;
        snprintf(b, sizeof b, "%02lld/%02lld/%04lld %02lld:%02lld:%02lld", d, m, y, rem / 3600, rem / 60 % 60, rem % 60);
        if (source) sb_str(s, "date \"");
        sb_str(s, b);
        if (source) sb_str(s, "\"");
        break;
    }
    case AV_LIST:
        if (source) sb_str(s, "{");
        for (int i = 0; i < v->n; i++) { if (i) sb_str(s, ", "); as_repr(&v->items[i], s, source); }
        if (source) sb_str(s, "}");
        break;
    case AV_RECORD:
        if (source) sb_str(s, "{");
        for (int i = 0; i < v->n; i++) { if (i) sb_str(s, ", "); sb_str(s, v->keys[i]); sb_str(s, ":"); as_repr(&v->items[i], s, source); }
        if (source) sb_str(s, "}");
        break;
    }
}
static char *as_to_text(const as_val *v) { as_sb s = {0}; as_repr(v, &s, 0); return s.b ? s.b : as_strdup(""); }

/* ---------------------------------------------------------------- léxico */
enum { T_EOF = 0, T_NL, T_ID, T_NUM, T_STR, T_OP };
typedef struct { int t; char *s; double num; int line, col; } as_tok;
typedef struct {
    as_tok *tk; int n, pos;
    as_host *h;
    char err[400]; int errnum, err_line, err_col, failed;
    int returning, exiting;
    as_val retval, result;
    struct { char *name; as_val v; } vars[256]; int nvars;
    struct { char *name; char **params; int np; int body_start, body_end; } handlers[64]; int nhandlers;
    char *tell_app;                  /* alvo do tell atual (NULL = o próprio script) */
    char tid[64];                    /* text item delimiters */
    int depth;
    int props_only;                  /* só property/global (chamada de handler de fora) */
} as_ctx;

static int as_fail(as_ctx *c, int num, const char *msg) {
    if (c->failed) return -1;
    c->failed = 1;
    c->errnum = num;
    snprintf(c->err, sizeof c->err, "%s", msg);
    if (c->pos < c->n) { c->err_line = c->tk[c->pos].line; c->err_col = c->tk[c->pos].col; }
    return -1;
}
static int lower_eq(const char *a, const char *b) {
    for (; *a && *b; a++, b++) { char x = *a, y = *b; if (x >= 'A' && x <= 'Z') x += 32; if (y >= 'A' && y <= 'Z') y += 32; if (x != y) return 0; }
    return !*a && !*b;
}
static int lex(as_ctx *c, const char *src) {
    int cap = 64, line = 1;
    const char *ls = src;
    c->tk = malloc(sizeof *c->tk * (usize)cap);
    const char *p = src;
    while (*p) {
        if (c->n + 2 >= cap) { cap *= 2; c->tk = realloc(c->tk, sizeof *c->tk * (usize)cap); }
        as_tok t = {0, 0, 0, line, (int)(p - ls) + 1};
        if (*p == ' ' || *p == '\t' || *p == '\r') { p++; continue; }
        if (!strncmp(p, "\xc2\xac", 2)) { p += 2; while (*p == ' ' || *p == '\t' || *p == '\r') p++; if (*p == '\n') { p++; line++; ls = p; } continue; }   /* ¬ */
        if (*p == '\n') { t.t = T_NL; c->tk[c->n++] = t; p++; line++; ls = p; continue; }
        if ((p[0] == '-' && p[1] == '-') || *p == '#') { while (*p && *p != '\n') p++; continue; }
        if (p[0] == '(' && p[1] == '*') {
            int depth = 0;
            while (*p) {
                if (p[0] == '(' && p[1] == '*') { depth++; p += 2; continue; }
                if (p[0] == '*' && p[1] == ')') { p += 2; if (--depth == 0) break; continue; }
                if (*p == '\n') { line++; ls = p + 1; }
                p++;
            }
            continue;
        }
        if (*p == '"' || !strncmp(p, "\xe2\x80\x9c", 3)) {               /* "texto" e “texto” */
            int curly = *p != '"';
            p += curly ? 3 : 1;
            as_sb s = {0};
            while (*p && !(curly ? !strncmp(p, "\xe2\x80\x9d", 3) : *p == '"')) {
                if (*p == '\\' && p[1]) {
                    p++;
                    char e = *p == 'n' ? '\n' : *p == 't' ? '\t' : *p == 'r' ? '\r' : *p;
                    sb_add(&s, &e, 1);
                    p++;
                    continue;
                }
                if (*p == '\n') { line++; ls = p + 1; }
                sb_add(&s, p, 1);
                p++;
            }
            if (!*p) { c->pos = c->n; free(s.b); return as_fail(c, -2741, "texto sem fechar as aspas"); }
            p += curly ? 3 : 1;
            t.t = T_STR; t.s = s.b ? s.b : as_strdup("");
            c->tk[c->n++] = t;
            continue;
        }
        if ((*p >= '0' && *p <= '9') || (*p == '.' && p[1] >= '0' && p[1] <= '9')) {
            double v = 0, f = 0.1;
            int real = 0;
            while (*p >= '0' && *p <= '9') v = v * 10 + (*p++ - '0');
            if (*p == '.' && p[1] >= '0' && p[1] <= '9') { real = 1; p++; while (*p >= '0' && *p <= '9') { v += f * (*p++ - '0'); f /= 10; } }
            if ((*p == 'e' || *p == 'E') && ((p[1] >= '0' && p[1] <= '9') || ((p[1] == '-' || p[1] == '+') && p[2] >= '0' && p[2] <= '9'))) {
                p++;
                int neg = *p == '-';
                if (*p == '-' || *p == '+') p++;
                int e = 0;
                while (*p >= '0' && *p <= '9') e = e * 10 + (*p++ - '0');
                while (e--) v = neg ? v / 10 : v * 10;
                real = 1;
            }
            t.t = T_NUM; t.num = v; t.s = real ? as_strdup("r") : 0;
            c->tk[c->n++] = t;
            continue;
        }
        int op_utf8 = !strncmp(p, "\xe2\x89", 2);            /* ≠ ≤ ≥ são operadores, não letras */
        if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || *p == '_' || ((u8)*p >= 0xc0 && !op_utf8) || *p == '|') {
            const char *st = p;
            if (*p == '|') { p++; st = p; while (*p && *p != '|') p++; t.t = T_ID; t.s = malloc((usize)(p - st) + 1); memcpy(t.s, st, (usize)(p - st)); t.s[p - st] = 0; if (*p) p++; c->tk[c->n++] = t; continue; }
            while ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_' || ((u8)*p >= 0x80 && strncmp(p, "\xe2\x89", 2) && strncmp(p, "\xc2\xac", 2))) p++;
            if (p == st) { c->pos = c->n; return as_fail(c, -2741, "caractere inesperado"); }
            t.t = T_ID;
            t.s = malloc((usize)(p - st) + 1);
            memcpy(t.s, st, (usize)(p - st));
            t.s[p - st] = 0;
            c->tk[c->n++] = t;
            continue;
        }
        /* operadores (≠ ≤ ≥ em UTF-8) */
        static const char *ops[] = {"\xe2\x89\xa0", "\xe2\x89\xa4", "\xe2\x89\xa5", "<=", ">=", "<>", "/=", "&", "+", "-", "*", "/",
                                    "^", "=", "<", ">", "(", ")", "{", "}", ",", ":", "'s", 0};
        int matched = 0;
        for (int i = 0; ops[i]; i++) {
            usize l = strlen(ops[i]);
            if (!strncmp(p, ops[i], l)) {
                t.t = T_OP;
                const char *norm = !strcmp(ops[i], "\xe2\x89\xa0") || !strcmp(ops[i], "<>") || !strcmp(ops[i], "/=") ? "!=" :
                                   !strcmp(ops[i], "\xe2\x89\xa4") ? "<=" : !strcmp(ops[i], "\xe2\x89\xa5") ? ">=" : ops[i];
                t.s = as_strdup(norm);
                c->tk[c->n++] = t;
                p += l;
                matched = 1;
                break;
            }
        }
        if (!matched) { c->pos = c->n; return as_fail(c, -2741, "caractere inesperado"); }
    }
    as_tok e = {T_EOF, 0, 0, line, 1};
    c->tk[c->n] = e;
    return 0;
}

/* ---------------------------------------------------------------- análise e execução (de uma passada) */
static as_tok *cur(as_ctx *c) { return &c->tk[c->pos]; }
static int is_id(as_ctx *c, int off, const char *w) { as_tok *t = &c->tk[c->pos + off]; return c->pos + off <= c->n && t->t == T_ID && lower_eq(t->s, w); }
static int is_op(as_ctx *c, const char *o) { as_tok *t = cur(c); return t->t == T_OP && !strcmp(t->s, o); }
static int accept_id(as_ctx *c, const char *w) { if (is_id(c, 0, w)) { c->pos++; return 1; } return 0; }
static int accept_op(as_ctx *c, const char *o) { if (is_op(c, o)) { c->pos++; return 1; } return 0; }
static int expect_id(as_ctx *c, const char *w) { if (accept_id(c, w)) return 0; char m[96]; snprintf(m, sizeof m, "esperava \"%s\"", w); return as_fail(c, -2741, m); }
static void skip_nl(as_ctx *c) { while (cur(c)->t == T_NL) c->pos++; }
static int at_end_of_stmt(as_ctx *c) { return cur(c)->t == T_NL || cur(c)->t == T_EOF; }

static as_val *var_ref(as_ctx *c, const char *name, int create) {
    for (int i = c->nvars - 1; i >= 0; i--) if (lower_eq(c->vars[i].name, name)) return &c->vars[i].v;
    if (!create || c->nvars >= 256) return 0;
    c->vars[c->nvars].name = as_strdup(name);
    c->vars[c->nvars].v = as_missing();
    return &c->vars[c->nvars++].v;
}
static void var_set(as_ctx *c, const char *name, as_val v) {
    as_val *r = var_ref(c, name, 1);
    if (!r) { as_free(&v); return; }
    as_free(r);
    *r = v;
}

static int expr(as_ctx *c, as_val *out, int run);
static int block(as_ctx *c, int run, const char *const *enders);

static int to_number(as_ctx *c, const as_val *v, double *d) {
    if (v->kind == AV_NUM || v->kind == AV_BOOL) { *d = v->num; return 0; }
    if (v->kind == AV_TEXT) {
        const char *s = v->s;
        while (*s == ' ') s++;
        char *e;
        double x = 0, f = 0.1;
        int neg = 0, any = 0;
        const char *q = s;
        if (*q == '-') { neg = 1; q++; }
        while (*q >= '0' && *q <= '9') { x = x * 10 + (*q - '0'); q++; any = 1; }
        if (*q == '.') { q++; while (*q >= '0' && *q <= '9') { x += f * (*q - '0'); f /= 10; q++; any = 1; } }
        e = (char *)q;
        while (*e == ' ') e++;
        if (any && !*e) { *d = neg ? -x : x; return 0; }
    }
    if (v->kind == AV_LIST && v->n == 1) return to_number(c, &v->items[0], d);
    char m[200], *t = as_to_text(v);
    snprintf(m, sizeof m, "Não é possível transformar \"%.60s\" em number.", t);
    free(t);
    return as_fail(c, -1700, m);
}
static int truthy(as_ctx *c, const as_val *v, int *b) {
    if (v->kind == AV_BOOL) { *b = v->num != 0; return 0; }
    if (v->kind == AV_TEXT && (lower_eq(v->s, "true") || lower_eq(v->s, "false"))) { *b = lower_eq(v->s, "true"); return 0; }
    char m[200], *t = as_to_text(v);
    snprintf(m, sizeof m, "Não é possível transformar \"%.60s\" em boolean.", t);
    free(t);
    return as_fail(c, -1700, m);
}
static int vals_equal(const as_val *a, const as_val *b) {
    if (a->kind == AV_NUM && b->kind == AV_NUM) return a->num == b->num;
    if (a->kind == AV_TEXT && b->kind == AV_TEXT) return lower_eq(a->s, b->s);   /* sem diferença de caixa, como o AppleScript */
    if (a->kind != b->kind) return 0;
    if (a->kind == AV_BOOL) return a->num == b->num;
    if (a->kind == AV_MISSING) return 1;
    if (a->kind == AV_LIST || a->kind == AV_RECORD) {
        if (a->n != b->n) return 0;
        for (int i = 0; i < a->n; i++) if (!vals_equal(&a->items[i], &b->items[i])) return 0;
        return 1;
    }
    return a->s && b->s && !strcmp(a->s, b->s);
}
static int contains_ci(const char *hay, const char *needle) {
    usize n = strlen(needle);
    for (; *hay; hay++) {
        usize i = 0;
        while (i < n && hay[i]) { char x = hay[i], y = needle[i]; if (x >= 'A' && x <= 'Z') x += 32; if (y >= 'A' && y <= 'Z') y += 32; if (x != y) break; i++; }
        if (i == n) return 1;
    }
    return !n;
}
static int prefix_ci(const char *s, const char *p) {
    for (; *p; s++, p++) { char x = *s, y = *p; if (x >= 'A' && x <= 'Z') x += 32; if (y >= 'A' && y <= 'Z') y += 32; if (x != y) return 0; }
    return 1;
}

/* ---- texto: pedaços (caracteres, palavras, parágrafos, text items) */
static int utf8_len(const char *s) { int n = 0; for (; *s; s++) if (((u8)*s & 0xc0) != 0x80) n++; return n; }
static as_val split_text(as_ctx *c, const char *s, int what) {   /* 0 caracteres, 1 palavras, 2 parágrafos, 3 text items */
    as_val l = {AV_LIST, 0, 0, 0, 0, 0, 0};
    if (what == 0) {
        for (const char *p = s; *p;) {
            const char *st = p++;
            while (((u8)*p & 0xc0) == 0x80) p++;
            char *ch = malloc((usize)(p - st) + 1);
            memcpy(ch, st, (usize)(p - st));
            ch[p - st] = 0;
            as_list_add(&l, as_txt_take(ch));
        }
    } else if (what == 1) {
        const char *p = s;
        while (*p) {
            while (*p && !((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || (u8)*p >= 0x80)) p++;
            const char *st = p;
            while (*p && ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || (u8)*p >= 0x80 || *p == '\'')) p++;
            if (p > st) { char *w = malloc((usize)(p - st) + 1); memcpy(w, st, (usize)(p - st)); w[p - st] = 0; as_list_add(&l, as_txt_take(w)); }
        }
    } else {
        const char *sep = what == 2 ? "\n" : c->tid;
        usize sl = strlen(sep);
        const char *p = s, *st = s;
        if (!sl) { as_list_add(&l, as_txt(s)); return l; }
        while (1) {
            const char *hit = strstr(p, sep);
            if (what == 2 && hit && hit > st && hit[-1] == '\r') { /* \r\n */ }
            if (!hit) { as_list_add(&l, as_txt(st)); break; }
            usize n = (usize)(hit - st);
            if (what == 2 && n && st[n - 1] == '\r') n--;
            char *w = malloc(n + 1);
            memcpy(w, st, n);
            w[n] = 0;
            as_list_add(&l, as_txt_take(w));
            p = st = hit + sl;
        }
    }
    return l;
}

/* ---- coerção "as" */
static int coerce_as(as_ctx *c, as_val *v, const char *cls) {
    if (lower_eq(cls, "text") || lower_eq(cls, "string") || lower_eq(cls, "unicode")) {
        if (v->kind == AV_LIST) {                       /* lista vira texto juntada pelos delimitadores */
            as_sb s = {0};
            for (int i = 0; i < v->n; i++) { if (i) sb_str(&s, c->tid); char *t = as_to_text(&v->items[i]); sb_str(&s, t); free(t); }
            as_free(v);
            *v = as_txt_take(s.b);
            return 0;
        }
        char *t = as_to_text(v);
        as_free(v);
        *v = as_txt_take(t);
        return 0;
    }
    if (lower_eq(cls, "integer") || lower_eq(cls, "number") || lower_eq(cls, "real")) {
        double d;
        if (to_number(c, v, &d)) return -1;
        as_free(v);
        if (lower_eq(cls, "integer")) d = d < 0 ? -(double)(long long)(-d + 0.5) : (double)(long long)(d + 0.5);
        *v = lower_eq(cls, "real") ? as_real(d) : as_num(d);
        return 0;
    }
    if (lower_eq(cls, "boolean")) { int b; if (truthy(c, v, &b)) return -1; as_free(v); *v = as_boolv(b); return 0; }
    if (lower_eq(cls, "list")) {
        if (v->kind == AV_LIST) return 0;
        as_val l = {AV_LIST, 0, 0, 0, 0, 0, 0};
        if (v->kind == AV_RECORD) { for (int i = 0; i < v->n; i++) as_list_add(&l, as_copy(&v->items[i])); as_free(v); }
        else as_list_add(&l, *v);
        *v = l;
        return 0;
    }
    if (lower_eq(cls, "alias") || lower_eq(cls, "file")) { char *t = as_to_text(v); as_free(v); as_val f = {AV_FILE, 0, 0, t, 0, 0, 0}; *v = f; return 0; }
    char m[120];
    snprintf(m, sizeof m, "Não é possível transformar o valor em %s.", cls);
    return as_fail(c, -1700, m);
}

/* ---- elementos: item/character/word/paragraph/text item N of x, items a thru b, first, last, every */
static const char *elem_kind(as_ctx *c, int *what) {
    static const struct { const char *one, *many; int what; } k[] = {
        {"item", "items", -1}, {"character", "characters", 0}, {"word", "words", 1}, {"paragraph", "paragraphs", 2}};
    for (unsigned i = 0; i < sizeof k / sizeof k[0]; i++) {
        if (is_id(c, 0, k[i].one)) { *what = k[i].what; return "one"; }
        if (is_id(c, 0, k[i].many)) { *what = k[i].what; return "many"; }
    }
    if (is_id(c, 0, "text") && (is_id(c, 1, "item") || is_id(c, 1, "items"))) { *what = 3; return is_id(c, 1, "items") ? "many2" : "one2"; }
    return 0;
}
static as_val listify(as_ctx *c, const as_val *of, int what) {
    if (what == -1) {
        if (of->kind == AV_LIST || of->kind == AV_RECORD) { as_val l = as_copy(of); if (l.kind == AV_RECORD) { coerce_as(c, &l, "list"); } return l; }
        if (of->kind == AV_TEXT) return split_text(c, of->s, 0);
        as_val l = {AV_LIST, 0, 0, 0, 0, 0, 0};
        as_list_add(&l, as_copy(of));
        return l;
    }
    char *t = as_to_text(of);
    as_val l = split_text(c, t, what);
    free(t);
    return l;
}
static int index_of(as_ctx *c, double d, int n, int *idx) {
    int i = (int)d;
    if (i < 0) i = n + i + 1;
    if (i < 1 || i > n) return as_fail(c, -1728, "Não foi possível obter o item: índice fora do intervalo.");
    *idx = i - 1;
    return 0;
}

/* ---- comandos que viram Apple Events (tell application) */
typedef struct { const char *name; unsigned cls, id; int takes_direct; } as_cmd;
static const as_cmd CMDS[] = {
    {"activate", AE_FCC('m', 'i', 's', 'c'), AE_FCC('a', 'c', 't', 'v'), 0},
    {"quit", AE_FCC('a', 'e', 'v', 't'), AE_FCC('q', 'u', 'i', 't'), 0},
    {"launch", AE_FCC('a', 's', 'c', 'r'), AE_FCC('n', 'o', 'o', 'p'), 0},
    {"run", AE_FCC('a', 'e', 'v', 't'), AE_FCC('o', 'a', 'p', 'p'), 0},
    {"reopen", AE_FCC('a', 'e', 'v', 't'), AE_FCC('r', 'a', 'p', 'p'), 0},
    {"print", AE_FCC('a', 'e', 'v', 't'), AE_FCC('p', 'd', 'o', 'c'), 1},
    {"open", AE_FCC('a', 'e', 'v', 't'), AE_FCC('o', 'd', 'o', 'c'), 1},
};
static int app_event(as_ctx *c, const char *app, unsigned cls, unsigned id, const as_val *direct, as_val *out) {
    if (!c->h->send) return as_fail(c, -1708, "Apple Events indisponíveis.");
    char err[300] = "";
    as_val res = as_missing();
    int e = c->h->send(c->h->ctx, app, cls, id, direct, &res, err, sizeof err);
    if (e) { as_free(&res); return as_fail(c, e, err[0] ? err : "O app respondeu com erro."); }
    if (out) *out = res; else as_free(&res);
    return 0;
}

/* lista de textos {..} para os botões do diálogo */
static int text_list(as_ctx *c, const as_val *v, const char **out, int cap, int *n) {
    *n = 0;
    if (v->kind == AV_TEXT) { out[(*n)++] = v->s; return 0; }
    if (v->kind != AV_LIST) return as_fail(c, -1700, "esperava uma lista de textos");
    for (int i = 0; i < v->n && *n < cap; i++) { if (v->items[i].kind != AV_TEXT) return as_fail(c, -1700, "esperava uma lista de textos"); out[(*n)++] = v->items[i].s; }
    return 0;
}
static int modifiers(as_ctx *c, unsigned *mods, int run) {
    *mods = 0;
    if (!accept_id(c, "using")) return 0;
    int list = accept_op(c, "{");
    do {
        static const struct { const char *w; unsigned m; } md[] = {{"command", 1u << 20}, {"shift", 1u << 17}, {"option", 1u << 19}, {"control", 1u << 18}};
        int ok = 0;
        for (unsigned i = 0; i < 4; i++) if (accept_id(c, md[i].w)) { *mods |= md[i].m; ok = 1; break; }
        if (!ok) return as_fail(c, -2741, "esperava command/shift/option/control down");
        if (expect_id(c, "down")) return -1;
    } while (list && accept_op(c, ","));
    if (list && !accept_op(c, "}")) return as_fail(c, -2741, "esperava }");
    (void)run;
    return 0;
}

/* comando: devolve 1 se reconheceu (e executou quando run) */
static int command(as_ctx *c, as_val *out, int run) {
    *out = as_missing();
    /* display dialog / alert / notification */
    if (is_id(c, 0, "display") && (is_id(c, 1, "dialog") || is_id(c, 1, "alert"))) {
        int alert = is_id(c, 1, "alert");
        c->pos += 2;
        as_val text, title = as_missing(), buttons = as_missing(), defb = as_missing(), answer = as_missing(), msg = as_missing();
        if (expr(c, &text, run)) return -1;
        double giving = 0;
        int has_answer = 0;
        for (;;) {
            if (accept_id(c, "with")) {
                if (accept_id(c, "title")) { if (expr(c, &title, run)) return -1; continue; }
                if (accept_id(c, "icon")) { as_val ic; if (expr(c, &ic, run)) return -1; as_free(&ic); continue; }
                if (accept_id(c, "hidden")) { expect_id(c, "answer"); continue; }
                return as_fail(c, -2741, "opção desconhecida depois de with");
            }
            if (accept_id(c, "buttons")) { if (expr(c, &buttons, run)) return -1; continue; }
            if (accept_id(c, "default")) {
                if (accept_id(c, "button")) { if (expr(c, &defb, run)) return -1; continue; }
                if (accept_id(c, "answer")) { if (expr(c, &answer, run)) return -1; has_answer = 1; continue; }
                return as_fail(c, -2741, "esperava button ou answer");
            }
            if (accept_id(c, "cancel")) { expect_id(c, "button"); as_val cb; if (expr(c, &cb, run)) return -1; as_free(&cb); continue; }
            if (accept_id(c, "giving")) { expect_id(c, "up"); expect_id(c, "after"); as_val g; if (expr(c, &g, run)) return -1; if (run && to_number(c, &g, &giving)) return -1; as_free(&g); continue; }
            if (accept_id(c, "message")) { if (expr(c, &msg, run)) return -1; continue; }
            if (accept_id(c, "as")) { c->pos++; continue; }
            break;
        }
        if (run) {
            const char *bl[8];
            int nb = 0, def = -1;
            if (buttons.kind != AV_MISSING) { if (text_list(c, &buttons, bl, 8, &nb)) return -1; }
            else if (alert) { bl[0] = "OK"; nb = 1; def = 0; }
            else { bl[0] = "Cancel"; bl[1] = "OK"; nb = 2; def = 1; }
            if (defb.kind == AV_TEXT) for (int i = 0; i < nb; i++) if (lower_eq(bl[i], defb.s)) def = i;
            if (defb.kind == AV_NUM) def = (int)defb.num - 1;
            char *t = as_to_text(&text), *ttl = title.kind != AV_MISSING ? as_to_text(&title) : 0, *ans = 0;
            if (msg.kind != AV_MISSING) { char *m2 = as_to_text(&msg); as_sb s = {0}; sb_str(&s, t); sb_str(&s, "\n\n"); sb_str(&s, m2); free(t); free(m2); t = s.b; }
            char *ain = has_answer ? as_to_text(&answer) : 0;
            int r = c->h->dialog ? c->h->dialog(c->h->ctx, t, ttl, bl, nb, def, (int)giving, ain, has_answer ? &ans : 0) : -1;
            free(t); free(ttl); free(ain);
            if (r == -1) {
                free(ans);
                as_free(&text); as_free(&title); as_free(&buttons); as_free(&defb); as_free(&answer); as_free(&msg);
                return as_fail(c, -128, "O usuário cancelou.");
            }
            as_val rec = {AV_RECORD, 0, 0, 0, 0, 0, 0};
            as_rec_set(&rec, "button returned", as_txt(r >= 0 && r < nb ? bl[r] : ""));
            if (has_answer) as_rec_set(&rec, "text returned", as_txt_take(ans ? ans : as_strdup("")));
            if (giving > 0) as_rec_set(&rec, "gave up", as_boolv(r == -2));
            *out = rec;
        }
        as_free(&text); as_free(&title); as_free(&buttons); as_free(&defb); as_free(&answer); as_free(&msg);
        return 1;
    }
    if (is_id(c, 0, "display") && is_id(c, 1, "notification")) {
        c->pos += 2;
        as_val text = as_missing(), title = as_missing(), sub = as_missing(), snd = as_missing();
        if (!is_id(c, 0, "with") && !is_id(c, 0, "subtitle") && !is_id(c, 0, "sound") && expr(c, &text, run)) return -1;
        for (;;) {
            if (accept_id(c, "with")) { if (expect_id(c, "title") || expr(c, &title, run)) return -1; continue; }
            if (accept_id(c, "subtitle")) { if (expr(c, &sub, run)) return -1; continue; }
            if (accept_id(c, "sound")) { if (expect_id(c, "name") || expr(c, &snd, run)) return -1; continue; }
            break;
        }
        if (run && c->h->notify) {
            char *a = as_to_text(&text), *b = title.kind ? as_to_text(&title) : 0, *d = sub.kind ? as_to_text(&sub) : 0, *e = snd.kind ? as_to_text(&snd) : 0;
            c->h->notify(c->h->ctx, a, b, d, e);
            free(a); free(b); free(d); free(e);
        }
        as_free(&text); as_free(&title); as_free(&sub); as_free(&snd);
        return 1;
    }
    if (is_id(c, 0, "do") && is_id(c, 1, "shell") && is_id(c, 2, "script")) {
        c->pos += 3;
        as_val cmd;
        if (expr(c, &cmd, run)) return -1;
        while (accept_id(c, "with") || accept_id(c, "without")) {             /* administrator privileges, altering line endings */
            if (accept_id(c, "administrator")) { expect_id(c, "privileges"); continue; }
            if (accept_id(c, "altering")) { expect_id(c, "line"); expect_id(c, "endings"); continue; }
            c->pos++;
        }
        if (run) {
            char *t = as_to_text(&cmd), *o = 0;
            int st = 0;
            if (!c->h->shell || c->h->shell(c->h->ctx, t, &o, &st)) { free(t); as_free(&cmd); return as_fail(c, -1, "não foi possível rodar o shell"); }
            free(t);
            if (o) { usize l = strlen(o); while (l && (o[l - 1] == '\n' || o[l - 1] == '\r')) o[--l] = 0; }
            if (st) { char m[600]; snprintf(m, sizeof m, "%s", o && o[0] ? o : "o comando falhou"); free(o); as_free(&cmd); return as_fail(c, st, m); }
            *out = as_txt_take(o ? o : as_strdup(""));
        }
        as_free(&cmd);
        return 1;
    }
    if (is_id(c, 0, "delay")) {
        c->pos++;
        as_val d;
        double s = 0;
        if (expr(c, &d, run)) return -1;
        if (run) { if (to_number(c, &d, &s)) return -1; if (c->h->delay) c->h->delay(c->h->ctx, s); }
        as_free(&d);
        return 1;
    }
    if (is_id(c, 0, "beep")) { c->pos++; if (!at_end_of_stmt(c)) { as_val n; if (expr(c, &n, run)) return -1; as_free(&n); } return 1; }
    if (is_id(c, 0, "say")) {                  /* sem síntese de voz no mOS: o texto vai para o log */
        c->pos++;
        as_val t;
        if (expr(c, &t, run)) return -1;
        while (accept_id(c, "using") || accept_id(c, "speaking")) { as_val x; if (accept_id(c, "rate") || 1) {} if (expr(c, &x, run)) return -1; as_free(&x); }
        if (run && c->h->log) { char *s = as_to_text(&t); c->h->log(c->h->ctx, s); free(s); }
        as_free(&t);
        return 1;
    }
    if (is_id(c, 0, "choose") && is_id(c, 1, "from") && is_id(c, 2, "list")) {
        c->pos += 3;
        as_val l, prompt = as_missing();
        if (expr(c, &l, run)) return -1;
        while (accept_id(c, "with")) { if (accept_id(c, "prompt")) { if (expr(c, &prompt, run)) return -1; } else if (accept_id(c, "title")) { as_val t; if (expr(c, &t, run)) return -1; as_free(&t); } else c->pos++; }
        while (accept_id(c, "default")) { expect_id(c, "items"); as_val t; if (expr(c, &t, run)) return -1; as_free(&t); }
        if (run) {
            const char *it[64];
            int n;
            if (text_list(c, &l, it, 64, &n)) return -1;
            char *p = prompt.kind ? as_to_text(&prompt) : 0;
            char *ch = c->h->choose ? c->h->choose(c->h->ctx, it, n, p) : 0;
            free(p);
            if (!ch) *out = as_boolv(0);
            else { as_val r = {AV_LIST, 0, 0, 0, 0, 0, 0}; as_list_add(&r, as_txt_take(ch)); *out = r; }
        }
        as_free(&l); as_free(&prompt);
        return 1;
    }
    /* System Events: teclas */
    if (is_id(c, 0, "keystroke") || (is_id(c, 0, "key") && is_id(c, 1, "code"))) {
        int code = is_id(c, 0, "key");
        c->pos += code ? 2 : 1;
        as_val v;
        unsigned mods;
        if (expr(c, &v, run) || modifiers(c, &mods, run)) return -1;
        if (run) {
            if (!c->tell_app || !lower_eq(c->tell_app, "System Events")) { as_free(&v); return as_fail(c, -1708, "keystroke só existe no System Events."); }
            int e;
            if (code) { double d; if (to_number(c, &v, &d)) return -1; e = c->h->keys ? c->h->keys(c->h->ctx, 0, (int)d, mods) : -1719; }
            else { char *t = as_to_text(&v); e = c->h->keys ? c->h->keys(c->h->ctx, t, -1, mods) : -1719; free(t); }
            if (e) { as_free(&v); return as_fail(c, e, "O System Events não tem permissão para mandar teclas (Acessibilidade)."); }
        }
        as_free(&v);
        return 1;
    }
    /* open location "url" */
    if (is_id(c, 0, "open") && is_id(c, 1, "location")) {
        c->pos += 2;
        as_val u;
        if (expr(c, &u, run)) return -1;
        if (run) {
            const char *app = c->tell_app ? c->tell_app : "";
            int e = app_event(c, app, AE_FCC('G', 'U', 'R', 'L'), AE_FCC('G', 'U', 'R', 'L'), &u, out);
            as_free(&u);
            return e ? -1 : 1;
        }
        as_free(&u);
        return 1;
    }
    for (unsigned i = 0; i < sizeof CMDS / sizeof CMDS[0]; i++) {
        if (!is_id(c, 0, CMDS[i].name)) continue;
        c->pos++;
        as_val direct = as_missing();
        int has = 0;
        if (CMDS[i].takes_direct && !at_end_of_stmt(c) && !is_id(c, 0, "end")) { if (expr(c, &direct, run)) return -1; has = 1; }
        if (run) {
            if (!c->tell_app && strcmp(CMDS[i].name, "run")) {
                /* fora de tell: open "x" abre pelo Finder (o Launch Services); activate/quit no próprio script não fazem nada */
                if (!strcmp(CMDS[i].name, "open") && has) {
                    int e = app_event(c, "Finder", CMDS[i].cls, CMDS[i].id, &direct, out);
                    as_free(&direct);
                    return e ? -1 : 1;
                }
                as_free(&direct);
                return 1;
            }
            int e = app_event(c, c->tell_app ? c->tell_app : "", CMDS[i].cls, CMDS[i].id, has ? &direct : 0, out);
            as_free(&direct);
            return e ? -1 : 1;
        }
        as_free(&direct);
        return 1;
    }
    return 0;
}

/* ---- primários */
static int expr_prim_text(as_ctx *c, as_val *out, int run);
static int app_ref(as_ctx *c, as_val *out, int run) {
    /* application "Nome" | application id "bundle" */
    int by_id = accept_id(c, "id");
    as_val n;
    if (expr_prim_text(c, &n, run)) return -1;
    if (run && by_id && c->h->app_info) {
        char *nm = c->h->app_info(c->h->ctx, n.s ? n.s : "", "name-for-id");
        if (nm) { free(n.s); n.s = nm; }
    }
    as_val a = {AV_APP, 0, 0, n.s ? n.s : as_strdup(""), 0, 0, 0};
    n.s = 0;
    as_free(&n);
    *out = a;
    return 0;
}
static int primary(as_ctx *c, as_val *out, int run);
static int expr_prim_text(as_ctx *c, as_val *out, int run) {
    if (primary(c, out, run)) return -1;
    if (run && out->kind != AV_TEXT) { char *t = as_to_text(out); as_free(out); *out = as_txt_take(t); }
    if (!run && out->kind != AV_TEXT) { as_free(out); *out = as_txt(""); }
    return 0;
}
static double now_abs(as_ctx *c) { return c->h->now ? c->h->now(c->h->ctx) : 0; }
static int property_of(as_ctx *c, const char *prop, as_val *out, int run);

static int block(as_ctx *c, int run, const char *const *enders);
/* roda o handler i com os argumentos (que passam a ser dele); o valor de return em *out */
static int call_handler(as_ctx *c, int i, as_val *args, int na, as_val *out) {
    if (na != c->handlers[i].np) { for (int k = 0; k < na; k++) as_free(&args[k]); return as_fail(c, -1721, "número de argumentos errado"); }
    if (++c->depth > 200) { for (int k = 0; k < na; k++) as_free(&args[k]); return as_fail(c, -2706, "recursão profunda demais"); }
    int save_vars = c->nvars, save_pos = c->pos;
    for (int k = 0; k < na; k++) {
        if (c->nvars >= 256) { as_free(&args[k]); continue; }
        c->vars[c->nvars].name = as_strdup(c->handlers[i].params[k]);
        c->vars[c->nvars++].v = args[k];
    }
    c->pos = c->handlers[i].body_start;
    static const char *const end_h[] = {"end", 0};
    int r = block(c, 1, end_h);
    c->pos = save_pos;
    as_val rv = c->returning ? c->retval : as_missing();
    if (c->returning) { c->returning = 0; memset(&c->retval, 0, sizeof c->retval); }
    c->exiting = 0;
    for (int k = save_vars; k < c->nvars; k++) { free(c->vars[k].name); as_free(&c->vars[k].v); }
    c->nvars = save_vars;
    c->depth--;
    if (r) { as_free(&rv); return -1; }
    *out = rv;
    return 0;
}
static int primary(as_ctx *c, as_val *out, int run) {
    as_tok *t = cur(c);
    *out = as_missing();
    if (c->failed) return -1;
    if (t->t == T_NUM) { *out = t->s ? as_real(t->num) : as_num(t->num); c->pos++; return 0; }
    if (t->t == T_STR) { *out = as_txt(t->s); c->pos++; return 0; }
    if (accept_op(c, "(")) {
        skip_nl(c);
        if (expr(c, out, run)) return -1;
        skip_nl(c);
        if (!accept_op(c, ")")) return as_fail(c, -2741, "esperava )");
        return 0;
    }
    if (accept_op(c, "{")) {
        skip_nl(c);
        as_val l = {AV_LIST, 0, 0, 0, 0, 0, 0};
        if (accept_op(c, "}")) { *out = l; return 0; }
        /* registro: {nome:valor, ...} (o nome pode ter várias palavras) */
        int save = c->pos, is_rec = 0;
        while (cur(c)->t == T_ID) c->pos++;
        is_rec = c->pos > save && is_op(c, ":");
        c->pos = save;
        if (is_rec) l.kind = AV_RECORD;
        do {
            skip_nl(c);
            if (is_rec) {
                as_sb k = {0};
                while (cur(c)->t == T_ID) { if (k.n) sb_str(&k, " "); sb_str(&k, cur(c)->s); c->pos++; }
                if (!accept_op(c, ":")) { free(k.b); return as_fail(c, -2741, "esperava : no registro"); }
                as_val v;
                if (expr(c, &v, run)) { free(k.b); return -1; }
                as_rec_set(&l, k.b ? k.b : "", v);
                free(k.b);
            } else {
                as_val v;
                if (expr(c, &v, run)) return -1;
                as_list_add(&l, v);
            }
            skip_nl(c);
        } while (accept_op(c, ","));
        if (!accept_op(c, "}")) { as_free(&l); return as_fail(c, -2741, "esperava }"); }
        *out = l;
        return 0;
    }
    if (accept_op(c, "-")) {
        if (primary(c, out, run)) return -1;
        if (run) { double d; if (to_number(c, out, &d)) return -1; int ii = out->is_int; as_free(out); *out = as_num(-d); out->is_int = ii; }
        return 0;
    }
    if (t->t != T_ID) return as_fail(c, -2741, "esperava um valor");
    accept_id(c, "the");
    t = cur(c);
    if (accept_id(c, "true")) { *out = as_boolv(1); return 0; }
    if (accept_id(c, "false")) { *out = as_boolv(0); return 0; }
    if (is_id(c, 0, "missing") && is_id(c, 1, "value")) { c->pos += 2; return 0; }
    if (accept_id(c, "return") || accept_id(c, "linefeed")) { *out = as_txt("\n"); return 0; }
    if (accept_id(c, "tab")) { *out = as_txt("\t"); return 0; }
    if (accept_id(c, "space")) { *out = as_txt(" "); return 0; }
    if (accept_id(c, "quote")) { *out = as_txt("\""); return 0; }
    if (accept_id(c, "pi")) { *out = as_real(3.14159265358979); return 0; }
    if (accept_id(c, "result")) { *out = as_copy(&c->result); return 0; }
    if (accept_id(c, "clipboard")) { if (run) { char *s = c->h->clip_get ? c->h->clip_get(c->h->ctx) : 0; *out = as_txt_take(s ? s : as_strdup("")); } return 0; }
    if (is_id(c, 0, "current") && is_id(c, 1, "date")) { c->pos += 2; out->kind = AV_DATE; out->num = now_abs(c); return 0; }
    if (is_id(c, 0, "text") && is_id(c, 1, "item") && is_id(c, 2, "delimiters")) { c->pos += 3; *out = as_txt(c->tid); return 0; }
    if (accept_id(c, "application") || accept_id(c, "app")) return app_ref(c, out, run);
    if (is_id(c, 0, "POSIX") && is_id(c, 1, "file")) {
        c->pos += 2;
        as_val p;
        if (expr_prim_text(c, &p, run)) return -1;
        as_val f = {AV_FILE, 0, 0, p.s, 0, 0, 0};
        *out = f;
        return 0;
    }
    if ((is_id(c, 0, "file") || is_id(c, 0, "alias")) && c->tk[c->pos + 1].t == T_STR) {
        c->pos++;
        as_val p;
        if (expr_prim_text(c, &p, run)) return -1;
        /* caminho HFS ("Macintosh HD:Users:...") vira POSIX */
        if (p.s && strchr(p.s, ':') && p.s[0] != '/') {
            as_sb s = {0};
            const char *q = strchr(p.s, ':') + 1;
            sb_str(&s, "/");
            for (; *q; q++) sb_add(&s, *q == ':' ? "/" : q, 1);
            free(p.s);
            p.s = s.b;
        }
        as_val f = {AV_FILE, 0, 0, p.s, 0, 0, 0};
        *out = f;
        return 0;
    }
    if (is_id(c, 0, "path") && is_id(c, 1, "to")) {
        c->pos += 2;
        as_sb w = {0};
        while (cur(c)->t == T_ID && !is_id(c, 0, "as") && !is_id(c, 0, "from")) { if (w.n) sb_str(&w, " "); sb_str(&w, cur(c)->s); c->pos++; }
        while (accept_id(c, "from")) { c->pos++; if (accept_id(c, "domain")) {} }
        if (run) {
            char *p = c->h->path_to ? c->h->path_to(c->h->ctx, w.b ? w.b : "") : 0;
            if (!p) { char m[160]; snprintf(m, sizeof m, "Pasta desconhecida: %s", w.b ? w.b : ""); free(w.b); return as_fail(c, -1728, m); }
            as_val f = {AV_FILE, 0, 0, p, 0, 0, 0};
            *out = f;
        }
        free(w.b);
        return 0;
    }
    /* chamada de handler do usuário: nome(args) */
    if (c->tk[c->pos + 1].t == T_OP && !strcmp(c->tk[c->pos + 1].s, "(")) {
        for (int i = 0; i < c->nhandlers; i++) {
            if (!lower_eq(c->handlers[i].name, t->s)) continue;
            c->pos += 2;
            as_val args[16];
            int na = 0;
            if (!accept_op(c, ")")) {
                do { if (na >= 16 || expr(c, &args[na], run)) return -1; na++; } while (accept_op(c, ","));
                if (!accept_op(c, ")")) return as_fail(c, -2741, "esperava )");
            }
            if (run) { if (call_handler(c, i, args, na, out)) return -1; }
            else for (int k = 0; k < na; k++) as_free(&args[k]);
            return 0;
        }
    }
    /* comando em posição de valor (set x to display dialog ...) */
    int cr = command(c, out, run);
    if (cr < 0) return -1;
    if (cr > 0) return 0;
    /* propriedades: "x of y" */
    static const char *props[] = {"length", "count", "POSIX", "quoted", "id", "version", "name", "running", "frontmost",
                                  "every", "first", "last", "rest", "reverse", "number", "exists", "item", "items", "character",
                                  "characters", "word", "words", "paragraph", "paragraphs", "text", "some", 0};
    for (int i = 0; props[i]; i++)
        if (is_id(c, 0, props[i])) {
            int what;
            const char *ek = elem_kind(c, &what);
            if (ek || !lower_eq(props[i], "text") || is_id(c, 1, "of")) return property_of(c, props[i], out, run);
        }
    if (is_id(c, 0, "process") && c->tell_app && lower_eq(c->tell_app, "System Events")) {
        c->pos++;
        as_val n;
        if (expr_prim_text(c, &n, run)) return -1;
        as_val p = {AV_CONST, 0, 0, n.s, 0, 0, 0};
        *out = p;
        return 0;
    }
    /* "chave of registro" de uma palavra */
    if (is_id(c, 1, "of") && !var_ref(c, t->s, 0)) {
        const char *key = t->s;
        c->pos += 2;
        as_val y;
        if (primary(c, &y, run)) return -1;
        if (!run) { as_free(&y); return 0; }
        if (y.kind == AV_RECORD) for (int i = 0; i < y.n; i++) if (lower_eq(y.keys[i], key)) { *out = as_copy(&y.items[i]); as_free(&y); return 0; }
        char m[160];
        snprintf(m, sizeof m, "Não foi possível obter %s.", key);
        as_free(&y);
        return as_fail(c, -1728, m);
    }
    /* variável */
    as_val *v = var_ref(c, t->s, 0);
    if (v) { *out = as_copy(v); c->pos++; return 0; }
    if (!run) { c->pos++; return 0; }
    char m[160];
    snprintf(m, sizeof m, "A variável %s não está definida.", t->s);
    return as_fail(c, -2753, m);
}

/* x of y / x in y */
static int of_target(as_ctx *c, as_val *y, int run) {
    if (!accept_id(c, "of") && !accept_id(c, "in")) return as_fail(c, -2741, "esperava of");
    return primary(c, y, run);
}
static int property_of(as_ctx *c, const char *prop, as_val *out, int run) {
    int what;
    const char *ek = elem_kind(c, &what);
    if (ek && (ek[0] == 'o' || ek[0] == 'm')) {
        int many = ek[0] == 'm', two = ek[strlen(ek) - 1] == '2';
        c->pos += two ? 2 : 1;
        as_val i1 = as_missing(), i2 = as_missing(), y;
        int range = 0, has_index = 0;
        if (!is_id(c, 0, "of") && !is_id(c, 0, "in")) {
            if (primary(c, &i1, run)) return -1;
            has_index = 1;
            if (accept_id(c, "thru") || accept_id(c, "through")) { range = 1; if (primary(c, &i2, run)) return -1; }
        }
        if (of_target(c, &y, run)) return -1;
        if (run) {
            as_val l = listify(c, &y, what);
            if (!has_index && many) { *out = l; }
            else if (!has_index) { as_free(&l); as_free(&y); return as_fail(c, -1728, "falta o índice"); }
            else {
                double a, b;
                if (to_number(c, &i1, &a)) return -1;
                int ia, ib;
                if (index_of(c, a, l.n, &ia)) { as_free(&l); return -1; }
                if (range) {
                    if (to_number(c, &i2, &b) || index_of(c, b, l.n, &ib)) { as_free(&l); return -1; }
                    if (ib < ia) { int t2 = ia; ia = ib; ib = t2; }
                    as_val r = {AV_LIST, 0, 0, 0, 0, 0, 0};
                    for (int k = ia; k <= ib; k++) as_list_add(&r, as_copy(&l.items[k]));
                    if (what >= 0 && what != 3 && y.kind == AV_TEXT && what == 0) {   /* characters a thru b de um texto: texto */
                        as_sb s = {0};
                        for (int k = 0; k < r.n; k++) sb_str(&s, r.items[k].s);
                        as_free(&r);
                        *out = as_txt_take(s.b ? s.b : as_strdup(""));
                    } else *out = r;
                } else *out = as_copy(&l.items[ia]);
                as_free(&l);
            }
        }
        as_free(&i1); as_free(&i2); as_free(&y);
        return 0;
    }
    c->pos++;
    if (lower_eq(prop, "POSIX")) { if (expect_id(c, "path")) return -1; }
    if (lower_eq(prop, "quoted")) { if (expect_id(c, "form")) return -1; }
    if (lower_eq(prop, "number") && !accept_id(c, "of")) return as_fail(c, -2741, "esperava of");
    if ((lower_eq(prop, "every") || lower_eq(prop, "first") || lower_eq(prop, "last") || lower_eq(prop, "some")) && (accept_id(c, "item") || accept_id(c, "process") || accept_id(c, "character") || accept_id(c, "word"))) {}
    if (lower_eq(prop, "exists")) {
        as_val y;
        if (primary(c, &y, run)) return -1;
        if (run) {
            int ex = 0;
            if (y.kind == AV_CONST && c->h->processes) {        /* exists process "X" */
                int n = 0;
                char **ps = c->h->processes(c->h->ctx, &n);
                for (int i = 0; i < n; i++) { if (lower_eq(ps[i], y.s)) ex = 1; free(ps[i]); }
                free(ps);
            } else if (y.kind == AV_APP && c->h->app_info) { char *p = c->h->app_info(c->h->ctx, y.s, "path"); ex = p != 0; free(p); }
            else if (y.kind == AV_FILE && c->h->app_info) { char *p = c->h->app_info(c->h->ctx, y.s, "file-exists"); ex = p != 0; free(p); }
            else ex = y.kind != AV_MISSING;
            *out = as_boolv(ex);
        }
        as_free(&y);
        return 0;
    }
    if (lower_eq(prop, "count") && !is_id(c, 0, "of")) {                         /* count x */
        as_val y;
        if (primary(c, &y, run)) return -1;
        if (run) { as_val l = listify(c, &y, -1); *out = as_num(l.n); as_free(&l); }
        as_free(&y);
        return 0;
    }
    /* "every process" no System Events: os processos */
    if (lower_eq(prop, "every") && c->tell_app && lower_eq(c->tell_app, "System Events") && !is_id(c, 0, "of")) {
        if (run && c->h->processes) {
            int n = 0;
            char **ps = c->h->processes(c->h->ctx, &n);
            as_val l = {AV_LIST, 0, 0, 0, 0, 0, 0};
            for (int i = 0; i < n; i++) { as_val p = {AV_CONST, 0, 0, ps[i], 0, 0, 0}; as_list_add(&l, p); }
            free(ps);
            *out = l;
        }
        return 0;
    }
    as_val y;
    if (of_target(c, &y, run)) return -1;
    if (!run) { as_free(&y); return 0; }
    int rc = 0;
    if (y.kind == AV_LIST && (lower_eq(prop, "name") || lower_eq(prop, "frontmost") || lower_eq(prop, "id"))) {
        as_val l = {AV_LIST, 0, 0, 0, 0, 0, 0};            /* name of every process */
        for (int i = 0; i < y.n; i++) as_list_add(&l, as_txt(y.items[i].s ? y.items[i].s : ""));
        *out = l;
    } else if (lower_eq(prop, "length") || lower_eq(prop, "count") || lower_eq(prop, "number")) {
        if (y.kind == AV_TEXT) *out = as_num(utf8_len(y.s));
        else { as_val l = listify(c, &y, -1); *out = as_num(l.n); as_free(&l); }
    } else if (lower_eq(prop, "POSIX")) {
        char *t = as_to_text(&y);
        *out = as_txt_take(t);
    } else if (lower_eq(prop, "quoted")) {
        char *t = as_to_text(&y);
        as_sb s = {0};
        sb_str(&s, "'");
        for (char *p = t; *p; p++) { if (*p == '\'') sb_str(&s, "'\\''"); else sb_add(&s, p, 1); }
        sb_str(&s, "'");
        free(t);
        *out = as_txt_take(s.b);
    } else if (lower_eq(prop, "first") || lower_eq(prop, "last") || lower_eq(prop, "some")) {
        as_val l = listify(c, &y, -1);
        if (!l.n) { as_free(&l); as_free(&y); return as_fail(c, -1728, "Não foi possível obter o item: a lista está vazia."); }
        *out = as_copy(&l.items[lower_eq(prop, "last") ? l.n - 1 : 0]);
        as_free(&l);
    } else if (lower_eq(prop, "rest") || lower_eq(prop, "reverse") || lower_eq(prop, "every")) {
        as_val l = listify(c, &y, -1), r = {AV_LIST, 0, 0, 0, 0, 0, 0};
        if (lower_eq(prop, "rest")) for (int i = 1; i < l.n; i++) as_list_add(&r, as_copy(&l.items[i]));
        else if (lower_eq(prop, "reverse")) for (int i = l.n - 1; i >= 0; i--) as_list_add(&r, as_copy(&l.items[i]));
        else for (int i = 0; i < l.n; i++) as_list_add(&r, as_copy(&l.items[i]));
        as_free(&l);
        *out = r;
    } else if (y.kind == AV_APP && (lower_eq(prop, "id") || lower_eq(prop, "version") || lower_eq(prop, "name") || lower_eq(prop, "running"))) {
        char *v = c->h->app_info ? c->h->app_info(c->h->ctx, y.s, prop) : 0;
        if (lower_eq(prop, "running")) *out = as_boolv(v && !strcmp(v, "true"));
        else if (!v) { char m[200]; snprintf(m, sizeof m, "Não foi possível achar o aplicativo \"%s\".", y.s); rc = as_fail(c, -600, m); }
        else *out = as_txt(v);
        free(v);
    } else if (y.kind == AV_CONST && lower_eq(prop, "name")) *out = as_txt(y.s);
    else if (y.kind == AV_CONST && lower_eq(prop, "frontmost")) {
        char *v = c->h->app_info ? c->h->app_info(c->h->ctx, y.s, "frontmost") : 0;
        *out = as_boolv(v && !strcmp(v, "true"));
        free(v);
    } else if (y.kind == AV_RECORD) {
        int found = 0;
        for (int i = 0; i < y.n; i++) if (lower_eq(y.keys[i], prop)) { *out = as_copy(&y.items[i]); found = 1; }
        if (!found) { char m[160]; snprintf(m, sizeof m, "Não foi possível obter %s do registro.", prop); rc = as_fail(c, -1728, m); }
    } else { char m[160]; snprintf(m, sizeof m, "Não foi possível obter %s.", prop); rc = as_fail(c, -1728, m); }
    as_free(&y);
    return rc;
}
/* registro com chave de várias palavras: button returned of r */
static int known_prop(as_ctx *c) {
    static const char *w[] = {"first", "last", "some", "every", "POSIX", "quoted", "current", "the", "AppleScript", 0};
    for (int i = 0; w[i]; i++) if (is_id(c, 0, w[i])) return 1;
    return is_id(c, 0, "text") && (is_id(c, 1, "item") || is_id(c, 1, "items"));
}
static int postfix(as_ctx *c, as_val *out, int run) {
    /* "<palavras> of <registro>": tenta antes de primary */
    int save = c->pos;
    if (known_prop(c)) goto plain;
    as_sb k = {0};
    while (cur(c)->t == T_ID && !is_id(c, 0, "of")) { if (k.n) sb_str(&k, " "); sb_str(&k, cur(c)->s); c->pos++; }
    if (k.n && is_id(c, 0, "of") && c->pos - save >= 2) {
        int words = c->pos - save;
        (void)words;
        c->pos++;
        as_val y;
        if (primary(c, &y, run)) { free(k.b); return -1; }
        if (!run) { free(k.b); as_free(&y); *out = as_missing(); return 0; }
        if (y.kind == AV_RECORD) {
            for (int i = 0; i < y.n; i++) if (lower_eq(y.keys[i], k.b)) { *out = as_copy(&y.items[i]); free(k.b); as_free(&y); return 0; }
        }
        char m[200];
        snprintf(m, sizeof m, "Não foi possível obter %s.", k.b);
        free(k.b);
        as_free(&y);
        return as_fail(c, -1728, m);
    }
    free(k.b);
    c->pos = save;
plain:
    if (primary(c, out, run)) return -1;
    while (accept_id(c, "as")) {
        if (cur(c)->t != T_ID) return as_fail(c, -2741, "esperava uma classe depois de as");
        const char *cls = cur(c)->s;
        c->pos++;
        if (run && coerce_as(c, out, cls)) return -1;
    }
    return 0;
}
static int power(as_ctx *c, as_val *out, int run) {
    if (postfix(c, out, run)) return -1;
    while (accept_op(c, "^")) {
        as_val r;
        if (postfix(c, &r, run)) return -1;
        if (run) {
            double a, b;
            if (to_number(c, out, &a) || to_number(c, &r, &b)) { as_free(&r); return -1; }
            double x = 1;
            int ib = (int)b;
            if ((double)ib == b && ib >= 0) for (int i = 0; i < ib; i++) x *= a;
            else { as_free(&r); return as_fail(c, -1700, "expoente fracionário não suportado"); }
            as_free(out);
            *out = as_real(x);
        }
        as_free(&r);
    }
    return 0;
}
static int term(as_ctx *c, as_val *out, int run) {
    if (power(c, out, run)) return -1;
    for (;;) {
        int op = is_op(c, "*") ? '*' : is_op(c, "/") ? '/' : is_id(c, 0, "div") ? 'd' : is_id(c, 0, "mod") ? 'm' : 0;
        if (!op) return 0;
        c->pos++;
        as_val r;
        if (power(c, &r, run)) return -1;
        if (run) {
            double a, b;
            if (to_number(c, out, &a) || to_number(c, &r, &b)) { as_free(&r); return -1; }
            int ints = out->is_int && r.is_int;
            if ((op == '/' || op == 'd' || op == 'm') && b == 0) { as_free(&r); return as_fail(c, -2701, "Não é possível dividir por zero."); }
            as_free(out);
            if (op == '*') { *out = as_num(a * b); out->is_int = ints; }
            else if (op == '/') *out = as_real(a / b);
            else if (op == 'd') *out = as_num((double)(long long)(a / b));
            else { double q = (double)(long long)(a / b); *out = as_num(a - q * b); out->is_int = ints; }
        }
        as_free(&r);
    }
}
static int sum(as_ctx *c, as_val *out, int run) {
    if (term(c, out, run)) return -1;
    for (;;) {
        int op = is_op(c, "+") ? '+' : is_op(c, "-") ? '-' : 0;
        if (!op) return 0;
        c->pos++;
        as_val r;
        if (term(c, &r, run)) return -1;
        if (run) {
            double a, b;
            if (to_number(c, out, &a) || to_number(c, &r, &b)) { as_free(&r); return -1; }
            int ints = out->is_int && r.is_int;
            as_free(out);
            *out = as_num(op == '+' ? a + b : a - b);
            out->is_int = ints;
        }
        as_free(&r);
    }
}
static int concat(as_ctx *c, as_val *out, int run) {
    if (sum(c, out, run)) return -1;
    while (accept_op(c, "&")) {
        as_val r;
        if (sum(c, &r, run)) return -1;
        if (run) {
            if (out->kind == AV_LIST || r.kind == AV_LIST) {      /* lista & item = lista maior */
                as_val l = out->kind == AV_LIST ? *out : (as_val){AV_LIST, 0, 0, 0, 0, 0, 0};
                if (out->kind != AV_LIST) as_list_add(&l, *out);
                if (r.kind == AV_LIST) { for (int i = 0; i < r.n; i++) as_list_add(&l, as_copy(&r.items[i])); as_free(&r); }
                else as_list_add(&l, r);
                *out = l;
                continue;
            }
            if (out->kind == AV_RECORD && r.kind == AV_RECORD) {
                for (int i = 0; i < r.n; i++) { int dup = 0; for (int k = 0; k < out->n; k++) if (lower_eq(out->keys[k], r.keys[i])) dup = 1; if (!dup) as_rec_set(out, r.keys[i], as_copy(&r.items[i])); }
                as_free(&r);
                continue;
            }
            char *a = as_to_text(out), *b = as_to_text(&r);
            as_sb s = {0};
            sb_str(&s, a);
            sb_str(&s, b);
            free(a); free(b);
            as_free(out);
            *out = as_txt_take(s.b ? s.b : as_strdup(""));
        }
        as_free(&r);
    }
    return 0;
}
static int comparison(as_ctx *c, as_val *out, int run) {
    if (concat(c, out, run)) return -1;
    for (;;) {
        int op = 0, neg = 0;
        if (accept_op(c, "=")) op = '=';
        else if (accept_op(c, "!=")) { op = '='; neg = 1; }
        else if (accept_op(c, "<=")) op = 'l';
        else if (accept_op(c, ">=")) op = 'g';
        else if (accept_op(c, "<")) op = '<';
        else if (accept_op(c, ">")) op = '>';
        else if (accept_id(c, "contains")) op = 'c';
        else if (is_id(c, 0, "does") && is_id(c, 1, "not") && is_id(c, 2, "contain")) { c->pos += 3; op = 'c'; neg = 1; }
        else if ((is_id(c, 0, "starts") || is_id(c, 0, "begins")) && is_id(c, 1, "with")) { c->pos += 2; op = 's'; }
        else if (is_id(c, 0, "ends") && is_id(c, 1, "with")) { c->pos += 2; op = 'e'; }
        else if (accept_id(c, "is") || accept_id(c, "equals")) {
            if (accept_id(c, "not")) neg = 1;
            if (accept_id(c, "in")) op = 'i';
            else if (is_id(c, 0, "greater") && is_id(c, 1, "than")) { c->pos += 2; op = '>'; if (accept_id(c, "or")) { expect_id(c, "equal"); accept_id(c, "to"); op = 'g'; } }
            else if (is_id(c, 0, "less") && is_id(c, 1, "than")) { c->pos += 2; op = '<'; if (accept_id(c, "or")) { expect_id(c, "equal"); accept_id(c, "to"); op = 'l'; } }
            else { accept_id(c, "equal"); accept_id(c, "to"); op = '='; }
        } else return 0;
        as_val r;
        if (concat(c, &r, run)) return -1;
        if (run) {
            int b = 0;
            if (op == '=') b = vals_equal(out, &r);
            else if (op == 'c' || op == 'i') {
                as_val *hay = op == 'c' ? out : &r, *nd = op == 'c' ? &r : out;
                if (hay->kind == AV_LIST) { for (int i = 0; i < hay->n; i++) if (vals_equal(&hay->items[i], nd)) b = 1; }
                else { char *h = as_to_text(hay), *n = as_to_text(nd); b = contains_ci(h, n); free(h); free(n); }
            } else if (op == 's' || op == 'e') {
                char *h = as_to_text(out), *n = as_to_text(&r);
                usize hl = strlen(h), nl = strlen(n);
                b = op == 's' ? prefix_ci(h, n) : (nl <= hl && prefix_ci(h + hl - nl, n));
                free(h); free(n);
            } else if (out->kind == AV_TEXT && r.kind == AV_TEXT) {
                int k = strcmp(out->s, r.s);
                b = op == '<' ? k < 0 : op == '>' ? k > 0 : op == 'l' ? k <= 0 : k >= 0;
            } else {
                double x, y;
                if (to_number(c, out, &x) || to_number(c, &r, &y)) return -1;
                b = op == '<' ? x < y : op == '>' ? x > y : op == 'l' ? x <= y : x >= y;
            }
            as_free(out);
            *out = as_boolv(neg ? !b : b);
        }
        as_free(&r);
    }
}
static int notexpr(as_ctx *c, as_val *out, int run) {
    if (accept_id(c, "not")) {
        if (notexpr(c, out, run)) return -1;
        if (run) { int b; if (truthy(c, out, &b)) return -1; as_free(out); *out = as_boolv(!b); }
        return 0;
    }
    return comparison(c, out, run);
}
static int andexpr(as_ctx *c, as_val *out, int run) {
    if (notexpr(c, out, run)) return -1;
    while (accept_id(c, "and")) {
        int b = 0;
        if (run && truthy(c, out, &b)) return -1;
        as_val r;
        if (notexpr(c, &r, run && b)) return -1;           /* curto-circuito */
        if (run) { int b2 = 0; if (b && truthy(c, &r, &b2)) return -1; as_free(out); *out = as_boolv(b && b2); }
        as_free(&r);
    }
    return 0;
}
static int expr(as_ctx *c, as_val *out, int run) {
    if (andexpr(c, out, run)) return -1;
    while (accept_id(c, "or")) {
        int b = 0;
        if (run && truthy(c, out, &b)) return -1;
        as_val r;
        if (andexpr(c, &r, run && !b)) return -1;
        if (run) { int b2 = 0; if (!b && truthy(c, &r, &b2)) return -1; as_free(out); *out = as_boolv(b || b2); }
        as_free(&r);
    }
    return 0;
}

/* ---------------------------------------------------------------- comandos e blocos */
static int ender(as_ctx *c, const char *const *enders) {
    for (int i = 0; enders && enders[i]; i++) {
        const char *e = enders[i];
        if (!strcmp(e, "else") && is_id(c, 0, "else")) return 1;
        if (!strcmp(e, "on error") && is_id(c, 0, "on") && is_id(c, 1, "error")) return 1;
        if (!strcmp(e, "end") && is_id(c, 0, "end")) return 1;
    }
    return 0;
}
static void end_kw(as_ctx *c, const char *what) {   /* end [if|repeat|tell|try|<handler>] */
    accept_id(c, "end");
    if (what && is_id(c, 0, what)) c->pos++;
    else if (cur(c)->t == T_ID && !at_end_of_stmt(c)) c->pos++;
}
static int statement(as_ctx *c, int run);
static int simple_or_block(as_ctx *c, int run, const char *endword) {
    if (at_end_of_stmt(c)) {
        static const char *const end_only[] = {"end", 0};
        if (block(c, run, end_only)) return -1;
        end_kw(c, endword);
        return 0;
    }
    return statement(c, run);
}
static int statement(as_ctx *c, int run) {
    if (c->failed) return -1;
    if (run && (c->returning || c->exiting)) run = 0;   /* depois de return/exit: só analisa */
    as_val v = as_missing();
    if (c->props_only && !is_id(c, 0, "property") && !is_id(c, 0, "prop") && !is_id(c, 0, "global")) run = 0;
    if (is_id(c, 0, "property") || is_id(c, 0, "prop")) {          /* property nome : valor (vale uma vez) */
        c->pos++;
        if (cur(c)->t != T_ID) return as_fail(c, -2741, "esperava o nome da propriedade");
        const char *name = cur(c)->s;
        c->pos++;
        if (!accept_op(c, ":")) return as_fail(c, -2741, "esperava :");
        if (expr(c, &v, run)) return -1;
        if (run && !var_ref(c, name, 0)) var_set(c, name, v); else as_free(&v);
        return 0;
    }
    if (is_id(c, 0, "global") || is_id(c, 0, "local")) {             /* local cria no escopo do handler */
        int local = is_id(c, 0, "local");
        c->pos++;
        do {
            if (cur(c)->t != T_ID) return as_fail(c, -2741, "esperava o nome da variável");
            if (run && (local || !var_ref(c, cur(c)->s, 0))) {
                if (local && c->nvars < 256) { c->vars[c->nvars].name = as_strdup(cur(c)->s); c->vars[c->nvars++].v = as_missing(); }
                else var_set(c, cur(c)->s, as_missing());
            }
            c->pos++;
        } while (accept_op(c, ","));
        return 0;
    }
    if (accept_id(c, "tell")) {
        as_val tgt;
        if (primary(c, &tgt, run)) return -1;
        char *prev = c->tell_app;
        char *mine = tgt.kind == AV_APP ? as_strdup(tgt.s) : 0;
        if (run && tgt.kind != AV_APP && tgt.kind != AV_CONST) { as_free(&tgt); return as_fail(c, -1708, "tell espera application \"Nome\""); }
        if (mine) c->tell_app = mine;
        int r;
        if (accept_id(c, "to")) r = statement(c, run);
        else r = simple_or_block(c, run, "tell");
        c->tell_app = prev;
        free(mine);
        as_free(&tgt);
        return r;
    }
    if (accept_id(c, "set")) {
        /* set the clipboard to x | set text item delimiters to x | set frontmost of process "X" to true | set x to v |
         * set {a, b} to {1, 2} | set item N of x to v */
        accept_id(c, "the");
        if (is_id(c, 0, "AppleScript") && c->tk[c->pos + 1].t == T_OP && !strcmp(c->tk[c->pos + 1].s, "'s")) c->pos += 2;
        if (accept_id(c, "clipboard")) {
            if (expect_id(c, "to") || expr(c, &v, run)) return -1;
            if (run && c->h->clip_set) { char *t = as_to_text(&v); c->h->clip_set(c->h->ctx, t); free(t); }
            as_free(&v);
            return 0;
        }
        if (is_id(c, 0, "text") && is_id(c, 1, "item") && is_id(c, 2, "delimiters")) {
            c->pos += 3;
            if (expect_id(c, "to") || expr(c, &v, run)) return -1;
            if (run) { char *t = v.kind == AV_LIST && v.n ? as_to_text(&v.items[0]) : as_to_text(&v); snprintf(c->tid, sizeof c->tid, "%s", t); free(t); }
            as_free(&v);
            return 0;
        }
        if (is_id(c, 0, "frontmost") && is_id(c, 1, "of")) {
            c->pos += 2;
            as_val p;
            if (primary(c, &p, run) || expect_id(c, "to") || expr(c, &v, run)) return -1;
            if (run) { int b; if (truthy(c, &v, &b)) return -1; if (b && c->h->frontmost && c->h->frontmost(c->h->ctx, p.s ? p.s : "")) { as_free(&p); return as_fail(c, -1728, "processo não encontrado"); } }
            as_free(&p); as_free(&v);
            return 0;
        }
        if (is_id(c, 0, "item") && c->tk[c->pos + 1].t != T_OP) {
            c->pos++;
            as_val idx;
            if (primary(c, &idx, run) || expect_id(c, "of")) return -1;
            if (cur(c)->t != T_ID) return as_fail(c, -2741, "esperava uma variável");
            const char *name = cur(c)->s;
            c->pos++;
            if (expect_id(c, "to") || expr(c, &v, run)) return -1;
            if (run) {
                as_val *l = var_ref(c, name, 0);
                double d;
                int i;
                if (!l || l->kind != AV_LIST) { as_free(&v); return as_fail(c, -1728, "não é uma lista"); }
                if (to_number(c, &idx, &d) || index_of(c, d, l->n, &i)) return -1;
                as_free(&l->items[i]);
                l->items[i] = v;
                v = as_missing();
            }
            as_free(&idx); as_free(&v);
            return 0;
        }
        if (accept_op(c, "{")) {                           /* set {a, b} to {1, 2} */
            const char *names[16];
            int nn = 0;
            do { if (cur(c)->t != T_ID || nn >= 16) return as_fail(c, -2741, "esperava variáveis"); names[nn++] = cur(c)->s; c->pos++; } while (accept_op(c, ","));
            if (!accept_op(c, "}") || expect_id(c, "to") || expr(c, &v, run)) return -1;
            if (run) {
                if (v.kind != AV_LIST || v.n < nn) { as_free(&v); return as_fail(c, -1728, "a lista tem itens de menos"); }
                for (int i = 0; i < nn; i++) var_set(c, names[i], as_copy(&v.items[i]));
            }
            as_free(&v);
            return 0;
        }
        if (cur(c)->t != T_ID) return as_fail(c, -2741, "esperava uma variável depois de set");
        const char *name = cur(c)->s;
        c->pos++;
        if (expect_id(c, "to") || expr(c, &v, run)) return -1;
        if (run) var_set(c, name, v); else as_free(&v);
        return 0;
    }
    if (accept_id(c, "copy")) {
        if (expr(c, &v, run) || expect_id(c, "to")) return -1;
        if (cur(c)->t != T_ID) return as_fail(c, -2741, "esperava uma variável");
        const char *name = cur(c)->s;
        c->pos++;
        if (run) var_set(c, name, v); else as_free(&v);
        return 0;
    }
    if (accept_id(c, "if")) {
        int done = 0;
        for (;;) {
            as_val cond;
            if (expr(c, &cond, run && !done)) return -1;
            int b = 0;
            if (run && !done && truthy(c, &cond, &b)) return -1;
            as_free(&cond);
            accept_id(c, "then");
            int take = run && !done && b;
            if (!at_end_of_stmt(c)) {                       /* if x then y (uma linha) */
                if (statement(c, take)) return -1;
                if (take) done = 1;
                if (!is_id(c, 0, "else")) return 0;
            } else {
                static const char *const e[] = {"else", "end", 0};
                if (block(c, take, e)) return -1;
                if (take) done = 1;
            }
            if (accept_id(c, "else")) {
                if (accept_id(c, "if")) continue;
                if (!at_end_of_stmt(c)) return statement(c, run && !done);
                static const char *const e2[] = {"end", 0};
                if (block(c, run && !done, e2)) return -1;
            }
            end_kw(c, "if");
            return 0;
        }
    }
    if (accept_id(c, "repeat")) {
        static const char *const e[] = {"end", 0};
        int start;
        if (accept_id(c, "with")) {
            if (cur(c)->t != T_ID) return as_fail(c, -2741, "esperava a variável do repeat");
            const char *var = cur(c)->s;
            c->pos++;
            if (accept_id(c, "from")) {
                as_val a, b, st = as_num(1);
                if (expr(c, &a, run) || expect_id(c, "to") || expr(c, &b, run)) return -1;
                if (accept_id(c, "by") && expr(c, &st, run)) return -1;
                double x = 0, y = 0, s = 1;
                if (run && (to_number(c, &a, &x) || to_number(c, &b, &y) || to_number(c, &st, &s))) return -1;
                as_free(&a); as_free(&b); as_free(&st);
                start = c->pos;
                if (!run || s == 0) { if (block(c, 0, e)) return -1; end_kw(c, "repeat"); return 0; }
                for (double i = x; s > 0 ? i <= y : i >= y; i += s) {
                    var_set(c, var, as_num(i));
                    c->pos = start;
                    if (block(c, 1, e)) return -1;
                    if (c->exiting || c->returning) break;
                }
                c->exiting = 0;
                c->pos = start;
                if (block(c, 0, e)) return -1;
                end_kw(c, "repeat");
                return 0;
            }
            if (expect_id(c, "in")) return -1;
            as_val l;
            if (expr(c, &l, run)) return -1;
            start = c->pos;
            if (run) {
                as_val items = listify(c, &l, -1);
                for (int i = 0; i < items.n; i++) {
                    var_set(c, var, as_copy(&items.items[i]));
                    c->pos = start;
                    if (block(c, 1, e)) { as_free(&items); as_free(&l); return -1; }
                    if (c->exiting || c->returning) break;
                }
                as_free(&items);
                c->exiting = 0;
            }
            as_free(&l);
            c->pos = start;
            if (block(c, 0, e)) return -1;
            end_kw(c, "repeat");
            return 0;
        }
        int mode = accept_id(c, "while") ? 'w' : accept_id(c, "until") ? 'u' : 0, cond_pos = c->pos;
        double times = -1;
        if (!mode && !at_end_of_stmt(c)) {
            as_val n;
            if (expr(c, &n, run)) return -1;
            if (run && to_number(c, &n, &times)) return -1;
            as_free(&n);
            accept_id(c, "times");
        } else if (mode) {
            as_val cv;
            if (expr(c, &cv, 0)) return -1;
            as_free(&cv);
        }
        start = c->pos;
        long iter = 0;
        while (run) {
            if (times >= 0 && iter >= (long)times) break;
            if (mode) {
                c->pos = cond_pos;
                as_val cv;
                int b;
                if (expr(c, &cv, 1) || truthy(c, &cv, &b)) return -1;
                as_free(&cv);
                if ((mode == 'w') != (b != 0)) break;
            }
            c->pos = start;
            if (block(c, 1, e)) return -1;
            if (c->exiting || c->returning) break;
            if (++iter > 10000000) return as_fail(c, -2706, "repeat sem fim");
        }
        c->exiting = 0;
        c->pos = start;
        if (block(c, 0, e)) return -1;
        end_kw(c, "repeat");
        return 0;
    }
    if (accept_id(c, "exit")) { accept_id(c, "repeat"); if (run) c->exiting = 1; return 0; }
    if (accept_id(c, "return")) {
        if (!at_end_of_stmt(c) && expr(c, &v, run)) return -1;
        if (run) { as_free(&c->retval); c->retval = v; c->returning = 1; } else as_free(&v);
        return 0;
    }
    if (accept_id(c, "log")) {
        if (expr(c, &v, run)) return -1;
        if (run && c->h->log) { as_sb s = {0}; sb_str(&s, "(*"); char *t = as_to_text(&v); sb_str(&s, t); free(t); sb_str(&s, "*)"); c->h->log(c->h->ctx, s.b); free(s.b); }
        as_free(&v);
        return 0;
    }
    if (accept_id(c, "error")) {
        as_val num = as_num(-2700);
        if (!at_end_of_stmt(c) && !is_id(c, 0, "number") && expr(c, &v, run)) return -1;
        if (accept_id(c, "number") && expr(c, &num, run)) return -1;
        if (run) {
            char *t = v.kind != AV_MISSING ? as_to_text(&v) : as_strdup("Ocorreu um erro.");
            double n = -2700;
            to_number(c, &num, &n);
            as_fail(c, (int)n, t);
            free(t);
            as_free(&v); as_free(&num);
            return -1;
        }
        as_free(&v); as_free(&num);
        return 0;
    }
    if (accept_id(c, "try")) {
        static const char *const e[] = {"on error", "end", 0};
        int failed = 0;
        if (block(c, run, e) && c->failed) { failed = 1; }
        char saved_err[400];
        int saved_num = c->errnum;
        snprintf(saved_err, sizeof saved_err, "%s", c->err);
        if (failed) {                                       /* pula até o on error / end do try */
            c->failed = 0;
            int depth = 0;
            while (cur(c)->t != T_EOF) {
                if (at_end_of_stmt(c)) { c->pos++; continue; }
                if (is_id(c, 0, "try") || (is_id(c, 0, "tell") && !is_id(c, 2, "to")) || is_id(c, 0, "repeat")) depth++;
                if (depth == 0 && ((is_id(c, 0, "on") && is_id(c, 1, "error")) || is_id(c, 0, "end"))) break;
                if (is_id(c, 0, "end")) depth--;
                c->pos++;
            }
        }
        if (is_id(c, 0, "on") && is_id(c, 1, "error")) {
            c->pos += 2;
            const char *msgv = 0, *numv = 0;
            if (cur(c)->t == T_ID && !is_id(c, 0, "number") && !at_end_of_stmt(c)) { msgv = cur(c)->s; c->pos++; }
            if (accept_id(c, "number") && cur(c)->t == T_ID) { numv = cur(c)->s; c->pos++; }
            if (failed && run) { if (msgv) var_set(c, msgv, as_txt(saved_err)); if (numv) var_set(c, numv, as_num(saved_num)); }
            static const char *const e2[] = {"end", 0};
            if (block(c, run && failed, e2)) return -1;
        }
        end_kw(c, "try");
        return 0;
    }
    if (is_id(c, 0, "on") || is_id(c, 0, "to")) {                  /* handler: on nome(a, b) ... end nome */
        if (is_id(c, 0, "to") && c->tk[c->pos + 2].t != T_OP) goto expression;
        c->pos++;
        if (cur(c)->t != T_ID) return as_fail(c, -2741, "esperava o nome do handler");
        const char *name = cur(c)->s;
        c->pos++;
        char *params[16];
        int np = 0;
        if (accept_op(c, "(")) {
            if (!accept_op(c, ")")) {
                do { if (cur(c)->t != T_ID || np >= 16) return as_fail(c, -2741, "esperava parâmetros"); params[np++] = as_strdup(cur(c)->s); c->pos++; } while (accept_op(c, ","));
                if (!accept_op(c, ")")) return as_fail(c, -2741, "esperava )");
            }
        } else if (cur(c)->t == T_ID) {                     /* on run argv / on open itens: o parâmetro direto */
            params[np++] = as_strdup(cur(c)->s);
            c->pos++;
        }
        int body = c->pos;
        /* registra antes do corpo: o handler pode chamar a si mesmo */
        int known = 0;
        for (int k = 0; k < c->nhandlers; k++) if (lower_eq(c->handlers[k].name, name)) known = 1;
        if (known) for (int k = 0; k < np; k++) free(params[k]);
        else if (c->nhandlers < 64) {
            int i = c->nhandlers++;
            c->handlers[i].name = as_strdup(name);
            c->handlers[i].params = malloc(sizeof(char *) * (usize)(np ? np : 1));
            for (int k = 0; k < np; k++) c->handlers[i].params[k] = params[k];
            c->handlers[i].np = np;
            c->handlers[i].body_start = body;
        }
        static const char *const e[] = {"end", 0};
        if (block(c, 0, e)) return -1;
        end_kw(c, name);
        return 0;
    }
expression: {
        accept_id(c, "get");
        int cr = command(c, &v, run);
        if (cr < 0) { as_free(&v); return -1; }
        if (!cr && expr(c, &v, run)) { as_free(&v); return -1; }
        if (run) { as_free(&c->result); c->result = v; } else as_free(&v);
        if (!at_end_of_stmt(c) && !is_id(c, 0, "else") && !is_id(c, 0, "end")) return as_fail(c, -2741, "esperava fim de linha");
        return 0;
    }
}
static int block(as_ctx *c, int run, const char *const *enders) {
    for (;;) {
        skip_nl(c);
        if (cur(c)->t == T_EOF) return enders && enders[0] ? as_fail(c, -2741, "esperava end") : 0;
        if (ender(c, enders)) return 0;
        if (statement(c, run)) return -1;
        if (!at_end_of_stmt(c) && !ender(c, enders)) return as_fail(c, -2741, "esperava fim de linha");
    }
}
/* registra os handlers antes de rodar (podem ser chamados antes da definição) */
static void prescan_handlers(as_ctx *c) {
    int save = c->pos;
    while (cur(c)->t != T_EOF) {
        if ((c->pos == 0 || c->tk[c->pos - 1].t == T_NL) && is_id(c, 0, "on") && !is_id(c, 1, "error") && c->tk[c->pos + 1].t == T_ID) {
            if (statement(c, 0)) { c->failed = 0; c->pos++; continue; }
            continue;
        }
        c->pos++;
    }
    c->pos = save;
}

static int find_handler_named(as_ctx *c, const char *name) {
    for (int i = 0; i < c->nhandlers; i++) if (lower_eq(c->handlers[i].name, name)) return i;
    return -1;
}
/* roda um script: 0 = ok e o resultado em *result; senão o número do erro (msg em err).
 * call = NULL: o nível de cima e depois o "on run" (com argv: a lista args, como o osascript); call = nome: só as
 * property/global e o handler pedido com args (NSAppleScript -executeAppleEvent:, evento 'ascr'/'psbr'). args é
 * copiado. */
static int as_run_ex(const char *src, as_host *h, const char *call, const as_val *args, int nargs, as_val *result,
                     char *err, int errcap, int *err_line, int *err_col) {
    as_ctx *c = calloc(1, sizeof *c);
    if (!c) return -108;
    c->h = h;
    as_val callres = as_missing();
    int called = 0;
    if (lex(c, src) == 0) {
        prescan_handlers(c);
        c->failed = 0;
        c->pos = 0;
        /* 1ª passada só analisa: erro de sintaxe antes de qualquer efeito */
        static const char *const none[] = {0};
        block(c, 0, none);
        int syntax = c->failed;
        if (syntax && c->errnum != -2741) syntax = 0;
        if (!syntax) {
            c->failed = 0;
            c->pos = 0;
            for (int i = 0; i < c->nvars; i++) { free(c->vars[i].name); as_free(&c->vars[i].v); }
            c->nvars = 0;
            c->props_only = call != 0;
            block(c, 1, none);
            c->props_only = 0;
            int hi = find_handler_named(c, call ? call : "run");
            if (!c->failed && call && hi < 0) {
                char m[200];
                snprintf(m, sizeof m, "O script não entende a mensagem “%s”.", call);
                as_fail(c, -1708, m);
            } else if (!c->failed && hi >= 0) {
                as_val a[16];
                int na = 0;
                if (!call && c->handlers[hi].np == 1) {           /* on run argv */
                    as_val l = {AV_LIST, 0, 0, 0, 0, 0, 0};
                    for (int k = 0; k < nargs; k++) as_list_add(&l, as_copy(&args[k]));
                    a[na++] = l;
                } else if (call) for (int k = 0; k < nargs && na < 16; k++) a[na++] = as_copy(&args[k]);
                c->returning = 0;
                if (!call_handler(c, hi, a, na, &callres)) called = 1;
            }
        }
    }
    int rc = c->failed ? (c->errnum ? c->errnum : -2700) : 0;
    if (rc) {
        snprintf(err, (usize)errcap, "%s", c->err);
        if (err_line) *err_line = c->err_line;
        if (err_col) *err_col = c->err_col;
        *result = as_missing();
    } else if (called) { *result = callres; callres = as_missing(); }
    else *result = c->returning ? as_copy(&c->retval) : as_copy(&c->result);
    as_free(&callres);
    for (int i = 0; i < c->n; i++) free(c->tk[i].s);
    free(c->tk);
    for (int i = 0; i < c->nvars; i++) { free(c->vars[i].name); as_free(&c->vars[i].v); }
    for (int i = 0; i < c->nhandlers; i++) {
        free(c->handlers[i].name);
        for (int k = 0; k < c->handlers[i].np; k++) free(c->handlers[i].params[k]);
        free(c->handlers[i].params);
    }
    as_free(&c->retval);
    as_free(&c->result);
    free(c);
    return rc;
}
static int as_run(const char *src, as_host *h, as_val *result, char *err, int errcap, int *err_line, int *err_col) {
    return as_run_ex(src, h, 0, 0, 0, result, err, errcap, err_line, err_col);
}
