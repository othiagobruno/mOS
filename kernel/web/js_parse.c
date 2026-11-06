/* mOS — motor JavaScript "Brisa": analisador léxico e sintático (descida recursiva → árvore sintática)
 * Subconjunto ES5 + ES6 comum: let/const, arrow functions, template literals, classes, for-of, spread,
 * desestruturação, parâmetros padrão, ?. e ??. Gramática de cobertura para parâmetros de arrow. */
#include "js_int.h"

enum { TK_EOF, TK_NUM, TK_STR, TK_TEMPLATE, TK_IDENT, TK_KW, TK_PUNCT, TK_REGEX, TK_BIGINT };

enum {
    KW_VAR = 1, KW_CONST, KW_FUNCTION, KW_RETURN, KW_IF, KW_ELSE, KW_FOR, KW_WHILE, KW_DO, KW_BREAK, KW_CONTINUE,
    KW_NEW, KW_DELETE, KW_TYPEOF, KW_INSTANCEOF, KW_IN, KW_THIS, KW_NULL, KW_TRUE, KW_FALSE, KW_THROW, KW_TRY,
    KW_CATCH, KW_FINALLY, KW_SWITCH, KW_CASE, KW_DEFAULT, KW_CLASS, KW_EXTENDS, KW_SUPER, KW_VOID, KW_DEBUGGER,
    KW_WITH, KW_IMPORT, KW_EXPORT, KW_ENUM
};

static const struct { const char *w; int k; } kwtab[] = {
    {"var", KW_VAR}, {"const", KW_CONST}, {"function", KW_FUNCTION}, {"return", KW_RETURN}, {"if", KW_IF},
    {"else", KW_ELSE}, {"for", KW_FOR}, {"while", KW_WHILE}, {"do", KW_DO}, {"break", KW_BREAK},
    {"continue", KW_CONTINUE}, {"new", KW_NEW}, {"delete", KW_DELETE}, {"typeof", KW_TYPEOF},
    {"instanceof", KW_INSTANCEOF}, {"in", KW_IN}, {"this", KW_THIS}, {"null", KW_NULL}, {"true", KW_TRUE},
    {"false", KW_FALSE}, {"throw", KW_THROW}, {"try", KW_TRY}, {"catch", KW_CATCH}, {"finally", KW_FINALLY},
    {"switch", KW_SWITCH}, {"case", KW_CASE}, {"default", KW_DEFAULT}, {"class", KW_CLASS}, {"extends", KW_EXTENDS},
    {"super", KW_SUPER}, {"void", KW_VOID}, {"debugger", KW_DEBUGGER}, {"with", KW_WITH}, {"import", KW_IMPORT},
    {"export", KW_EXPORT}, {"enum", KW_ENUM},
};

enum {
    P_LBRACE = 1, P_RBRACE, P_LPAREN, P_RPAREN, P_LBRACK, P_RBRACK, P_SEMI, P_COMMA, P_DOT, P_ELLIPSIS, P_QDOT,
    P_Q, P_COLON, P_ARROW, P_ASSIGN, P_OPASSIGN, P_EQ, P_NE, P_SEQ, P_SNE, P_LT, P_GT, P_LE, P_GE, P_PLUS, P_MINUS,
    P_STAR, P_SLASH, P_PCT, P_EXP, P_INC, P_DEC, P_SHL, P_SHR, P_USHR, P_AMP, P_BAR, P_CARET, P_NOT, P_TILDE,
    P_AND, P_OR, P_NULLISH
};

typedef struct {
    int type, op, aux;
    double num;
    jsstr *str;
    int start, end, line;
    bool nl, tail;
} token;

typedef struct {
    const char *s;
    int len, pos, line;
    token t;
    int prev_line;
    jsscript *sc;
    bool failed;
    char err[200];
    int errline;
    jsfunc *fn;
    jsfunc *fstack[256];
    int nf;
    jsblock *blk;          /* bloco que recebe as declarações léxicas atuais */
    jsblock *fnblk;        /* bloco do corpo da função atual */
    bool noin;
    bool in_class_ctor;
    bool next_gen, next_async;   /* a próxima função criada é geradora / async */
    bool module, anon_default;   /* analisando um módulo; export default function sem nome */
    uptr stack_top;
} parser;

static void lex(parser *P);

static void error_at(parser *P, int line, const char *fmt, ...) {
    if (P->failed) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(P->err, sizeof P->err, fmt, ap);
    va_end(ap);
    P->errline = line;
    P->failed = true;
    P->pos = P->len;
    P->t.type = TK_EOF;
}

static bool stack_ok(parser *P) {
    uptr sp;
    __asm__ volatile("movq %%rsp, %0" : "=r"(sp));
    if ((P->stack_top && P->stack_top - sp > 400u * 1024) || (J.stack_base && J.stack_limit && J.stack_base - sp > J.stack_limit)) {
        error_at(P, P->t.line, "Aninhamento profundo demais");
        return false;
    }
    return true;
}

/* ============================================================ memória da árvore */
static void *palloc(parser *P, usize n) {
    void *p = arena_alloc(&P->sc->arena, n);
    if (!p) { error_at(P, P->t.line, "Memória insuficiente"); return NULL; }
    return p;
}

static jsstr *pstr(parser *P, const char *s, int n) {
    jsstr *r = n <= 64 ? js_intern(s, n) : js_strn(s, n);
    jsscript *sc = P->sc;
    if (sc->nstrs == sc->cstrs) {
        int nc = sc->cstrs ? sc->cstrs * 2 : 64;
        jsstr **na = krealloc(sc->strs, sizeof(jsstr *) * (usize)nc);
        if (!na) { error_at(P, P->t.line, "Memória insuficiente"); return r; }
        sc->strs = na;
        sc->cstrs = nc;
    }
    sc->strs[sc->nstrs++] = r;
    return r;
}

static jsnode *mk(parser *P, int type) {
    jsnode *n = palloc(P, sizeof(jsnode));
    if (!n) {
        static jsnode dummy;
        memset(&dummy, 0, sizeof dummy);
        return &dummy;
    }
    n->type = (u8)type;
    n->line = P->t.line;
    return n;
}

typedef struct { jsnode **v; int n, cap; } nlist;

static void nl_push(parser *P, nlist *l, jsnode *n) {
    if (l->n == l->cap) {
        int nc = l->cap ? l->cap * 2 : 8;
        jsnode **na = krealloc(l->v, sizeof(jsnode *) * (usize)nc);
        if (!na) { error_at(P, P->t.line, "Memória insuficiente"); return; }
        l->v = na;
        l->cap = nc;
    }
    l->v[l->n++] = n;
}

static void nl_finish(parser *P, nlist *l, jsnode *owner) {
    owner->n = l->n;
    if (l->n) {
        owner->list = palloc(P, sizeof(jsnode *) * (usize)l->n);
        if (owner->list) memcpy(owner->list, l->v, sizeof(jsnode *) * (usize)l->n);
        else owner->n = 0;
    }
    kfree(l->v);
    l->v = NULL;
    l->n = l->cap = 0;
}

static void *grow_arena_array(parser *P, void *old, int n, int *cap, usize elem) {
    if (n < *cap) return old;
    int nc = *cap ? *cap * 2 : 8;
    void *na = palloc(P, elem * (usize)nc);
    if (!na) return old;
    if (old && n) memcpy(na, old, elem * (usize)n);
    *cap = nc;
    return na;
}

static void add_var(parser *P, jsstr *name) {
    jsfunc *f = P->fn;
    for (int i = 0; i < f->nvars; i++) if (f->vars[i] == name) return;
    f->vars = grow_arena_array(P, f->vars, f->nvars, &f->cvars, sizeof(jsstr *));
    if (f->nvars < f->cvars) f->vars[f->nvars++] = name;
}

static void add_lex(parser *P, jsstr *name, bool isconst) {
    jsblock *b = P->blk;
    for (int i = 0; i < b->n; i++) if (b->names[i] == name) { error_at(P, P->t.line, "Identificador '%s' já foi declarado", name->d); return; }
    if (b->n == b->cap) {
        int nc = b->cap ? b->cap * 2 : 4;
        jsstr **nn = palloc(P, sizeof(jsstr *) * (usize)nc);
        u8 *nc2 = palloc(P, (usize)nc);
        if (!nn || !nc2) return;
        if (b->n) { memcpy(nn, b->names, sizeof(jsstr *) * (usize)b->n); memcpy(nc2, b->isconst, (usize)b->n); }
        b->names = nn;
        b->isconst = nc2;
        b->cap = nc;
    }
    b->names[b->n] = name;
    b->isconst[b->n] = isconst;
    b->n++;
}

static void add_block_func(parser *P, jsnode *fd) {
    jsblock *b = P->blk;
    b->funcs = grow_arena_array(P, b->funcs, b->nfuncs, &b->cfuncs, sizeof(jsnode *));
    if (b->nfuncs < b->cfuncs) b->funcs[b->nfuncs++] = fd;
}

static void add_fn_func(parser *P, jsnode *fd) {
    jsfunc *f = P->fn;
    f->funcs = grow_arena_array(P, f->funcs, f->nfuncs, &f->cfuncs, sizeof(jsnode *));
    if (f->nfuncs < f->cfuncs) f->funcs[f->nfuncs++] = fd;
}

/* ============================================================ analisador léxico */
static bool id_start(int c) { return isalpha(c) || c == '_' || c == '$' || c >= 0x80 || c == '#'; }
static bool id_part(int c) { return isalnum(c) || c == '_' || c == '$' || c >= 0x80; }

static void skip_ws(parser *P) {
    const char *s = P->s;
    while (P->pos < P->len) {
        u8 c = (u8)s[P->pos];
        if (c == '\n') { P->line++; P->pos++; P->t.nl = true; }
        else if (c == ' ' || c == '\t' || c == '\r' || c == 0x0B || c == 0x0C) P->pos++;
        else if (c == 0xC2 && P->pos + 1 < P->len && (u8)s[P->pos + 1] == 0xA0) P->pos += 2;
        else if (c == 0xEF && P->pos + 2 < P->len && (u8)s[P->pos + 1] == 0xBB && (u8)s[P->pos + 2] == 0xBF) P->pos += 3;
        else if (c == 0xE2 && P->pos + 2 < P->len && (u8)s[P->pos + 1] == 0x80 && ((u8)s[P->pos + 2] == 0xA8 || (u8)s[P->pos + 2] == 0xA9)) {
            P->pos += 3; P->line++; P->t.nl = true;
        }
        else if (c == '/' && P->pos + 1 < P->len && s[P->pos + 1] == '/') {
            while (P->pos < P->len && s[P->pos] != '\n') P->pos++;
        } else if (c == '/' && P->pos + 1 < P->len && s[P->pos + 1] == '*') {
            P->pos += 2;
            while (P->pos + 1 < P->len && !(s[P->pos] == '*' && s[P->pos + 1] == '/')) {
                if (s[P->pos] == '\n') { P->line++; P->t.nl = true; }
                P->pos++;
            }
            P->pos += 2;
            if (P->pos > P->len) P->pos = P->len;
        } else if (c == '<' && P->pos + 3 < P->len && !strncmp(s + P->pos, "<!--", 4)) {
            while (P->pos < P->len && s[P->pos] != '\n') P->pos++;
        } else if (c == '-' && P->t.nl && P->pos + 2 < P->len && !strncmp(s + P->pos, "-->", 3)) {
            while (P->pos < P->len && s[P->pos] != '\n') P->pos++;
        } else break;
    }
}

static int hexd(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if ((c | 32) >= 'a' && (c | 32) <= 'f') return (c | 32) - 'a' + 10;
    return -1;
}

/* lê um escape após '\'; escreve UTF-8 em sb */
static void read_escape(parser *P, sbuf_t *sb) {
    const char *s = P->s;
    if (P->pos >= P->len) return;
    char c = s[P->pos++];
    char u8b[4];
    u32 cp;
    switch (c) {
    case 'n': sb_putc(sb, '\n'); return;
    case 't': sb_putc(sb, '\t'); return;
    case 'r': sb_putc(sb, '\r'); return;
    case 'b': sb_putc(sb, '\b'); return;
    case 'f': sb_putc(sb, '\f'); return;
    case 'v': sb_putc(sb, '\v'); return;
    case '0':
        if (P->pos < P->len && isdigit((u8)s[P->pos])) { /* octal legado */
            int v = 0;
            P->pos--;
            for (int i = 0; i < 3 && P->pos < P->len && s[P->pos] >= '0' && s[P->pos] <= '7'; i++) v = v * 8 + (s[P->pos++] - '0');
            sb_putn(sb, u8b, js_utf8_put(u8b, (u32)v));
            return;
        }
        sb_putc(sb, 0);
        return;
    case '\r': if (P->pos < P->len && s[P->pos] == '\n') P->pos++; P->line++; return;
    case '\n': P->line++; return;
    case 'x':
        if (P->pos + 1 < P->len && hexd(s[P->pos]) >= 0 && hexd(s[P->pos + 1]) >= 0) {
            cp = (u32)(hexd(s[P->pos]) * 16 + hexd(s[P->pos + 1]));
            P->pos += 2;
            sb_putn(sb, u8b, js_utf8_put(u8b, cp));
            return;
        }
        error_at(P, P->line, "Escape hexadecimal inválido");
        return;
    case 'u': {
        cp = 0;
        if (P->pos < P->len && s[P->pos] == '{') {
            P->pos++;
            while (P->pos < P->len && hexd(s[P->pos]) >= 0) cp = cp * 16 + (u32)hexd(s[P->pos++]);
            if (P->pos < P->len && s[P->pos] == '}') P->pos++;
            else { error_at(P, P->line, "Escape Unicode inválido"); return; }
        } else {
            for (int i = 0; i < 4; i++) {
                int h = P->pos < P->len ? hexd(s[P->pos]) : -1;
                if (h < 0) { error_at(P, P->line, "Escape Unicode inválido"); return; }
                cp = cp * 16 + (u32)h;
                P->pos++;
            }
            if (cp >= 0xD800 && cp < 0xDC00 && P->pos + 5 < P->len && s[P->pos] == '\\' && s[P->pos + 1] == 'u') {
                u32 lo = 0;
                bool ok = true;
                for (int i = 0; i < 4; i++) { int h = hexd(s[P->pos + 2 + i]); if (h < 0) { ok = false; break; } lo = lo * 16 + (u32)h; }
                if (ok && lo >= 0xDC00 && lo < 0xE000) { cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00); P->pos += 6; }
            }
        }
        if (cp > 0x10FFFF) cp = 0xFFFD;
        sb_putn(sb, u8b, js_utf8_put(u8b, cp));
        return;
    }
    default:
        if (c >= '1' && c <= '7') {
            int v = c - '0';
            for (int i = 0; i < 2 && P->pos < P->len && s[P->pos] >= '0' && s[P->pos] <= '7'; i++) v = v * 8 + (s[P->pos++] - '0');
            sb_putn(sb, u8b, js_utf8_put(u8b, (u32)v));
            return;
        }
        sb_putc(sb, c);
    }
}

static void lex_string(parser *P, char q) {
    sbuf_t sb = {0};
    const char *s = P->s;
    while (P->pos < P->len && s[P->pos] != q) {
        char c = s[P->pos];
        if (c == '\n') { error_at(P, P->line, "String não terminada"); sb_free(&sb); return; }
        if (c == '\\') { P->pos++; read_escape(P, &sb); continue; }
        int st = P->pos;
        while (P->pos < P->len && s[P->pos] != q && s[P->pos] != '\\' && s[P->pos] != '\n') P->pos++;
        sb_putn(&sb, s + st, P->pos - st);
    }
    if (P->pos >= P->len) { error_at(P, P->line, "String não terminada"); sb_free(&sb); return; }
    P->pos++;
    P->t.type = TK_STR;
    P->t.str = pstr(P, sb.p ? sb.p : "", sb.len);
    sb_free(&sb);
}

/* parte de template literal a partir de P->pos (logo após ` ou }) */
static void lex_template_part(parser *P) {
    sbuf_t sb = {0};
    const char *s = P->s;
    P->t.type = TK_TEMPLATE;
    P->t.tail = false;
    for (;;) {
        if (P->pos >= P->len) { error_at(P, P->line, "Template literal não terminado"); break; }
        char c = s[P->pos];
        if (c == '`') { P->pos++; P->t.tail = true; break; }
        if (c == '$' && P->pos + 1 < P->len && s[P->pos + 1] == '{') { P->pos += 2; break; }
        if (c == '\\') { P->pos++; read_escape(P, &sb); continue; }
        if (c == '\n') P->line++;
        if (c == '\r') { P->pos++; if (P->pos < P->len && s[P->pos] == '\n') { P->pos++; } sb_putc(&sb, '\n'); P->line++; continue; }
        sb_putc(&sb, c);
        P->pos++;
    }
    P->t.str = pstr(P, sb.p ? sb.p : "", sb.len);
    P->t.end = P->pos;
    sb_free(&sb);
}

static void lex_number(parser *P) {
    const char *s = P->s;
    int st = P->pos;
    double v = 0;
    if (s[st] == '0' && st + 1 < P->len && strchr("xXoObB", s[st + 1]) && s[st + 1]) {
        int radix = (s[st + 1] | 32) == 'x' ? 16 : (s[st + 1] | 32) == 'o' ? 8 : 2;
        P->pos += 2;
        int nd = 0;
        while (P->pos < P->len) {
            char c = s[P->pos];
            if (c == '_') { P->pos++; continue; }
            int d = hexd(c);
            if (d < 0 || d >= radix) break;
            v = v * radix + d;
            P->pos++;
            nd++;
        }
        if (!nd) error_at(P, P->line, "Número inválido");
    } else if (s[st] == '0' && st + 1 < P->len && isdigit((u8)s[st + 1])) {
        /* octal legado 017 (ou decimal se tiver 8/9) */
        bool oct = true;
        int q = st + 1;
        while (q < P->len && isdigit((u8)s[q])) { if (s[q] >= '8') oct = false; q++; }
        if (oct) { for (int i = st + 1; i < q; i++) v = v * 8 + (s[i] - '0'); P->pos = q; }
        else { int used; v = js_strtod(s + st, P->len - st, &used); P->pos = st + used; }
    } else {
        int used;
        v = js_strtod(s + st, P->len - st, &used);
        P->pos = st + used;
    }
    if (P->pos < P->len && s[P->pos] == 'n') {
        /* literal BigInt: só inteiros (decimal sem ponto/expoente, 0x, 0o, 0b) */
        char tmp[1024];
        int k = 0;
        bool bad = s[st] == '0' && P->pos - st > 1 && isdigit((u8)s[st + 1]);
        for (int i = st; i < P->pos && k < (int)sizeof tmp - 1; i++) {
            if (s[i] == '_') continue;
            if ((s[i] == '.' || s[i] == 'e' || s[i] == 'E') && !(s[st] == '0' && (s[st + 1] | 32) == 'x')) bad = true;
            tmp[k++] = s[i];
        }
        if (bad) { error_at(P, P->line, "Literal BigInt inválido"); return; }
        P->pos++;
        if (P->pos < P->len && (id_start((u8)s[P->pos]) || isdigit((u8)s[P->pos]))) { error_at(P, P->line, "Identificador logo após número"); return; }
        P->t.type = TK_BIGINT;
        P->t.str = pstr(P, tmp, k);
        return;
    }
    if (P->pos < P->len && (id_start((u8)s[P->pos]) || isdigit((u8)s[P->pos]))) { error_at(P, P->line, "Identificador logo após número"); return; }
    P->t.type = TK_NUM;
    P->t.num = v;
}

static void lex(parser *P) {
    P->prev_line = P->t.line;
    P->t.nl = false;
    skip_ws(P);
    P->t.start = P->pos;
    P->t.line = P->line;
    P->t.str = NULL;
    P->t.aux = 0;
    if (P->pos >= P->len) { P->t.type = TK_EOF; P->t.end = P->pos; return; }
    const char *s = P->s;
    u8 c = (u8)s[P->pos];
    if (id_start(c) || (c == '\\' && P->pos + 1 < P->len && s[P->pos + 1] == 'u')) {
        int st = P->pos;
        P->pos++;
        while (P->pos < P->len && id_part((u8)s[P->pos])) P->pos++;
        int n = P->pos - st;
        P->t.type = TK_IDENT;
        P->t.op = 0;
        if (n >= 2 && n <= 10 && c >= 'a' && c <= 'z') {
            for (u32 i = 0; i < ARRAY_LEN(kwtab); i++)
                if ((int)strlen(kwtab[i].w) == n && !memcmp(kwtab[i].w, s + st, (usize)n)) { P->t.type = TK_KW; P->t.op = kwtab[i].k; break; }
        }
        P->t.str = pstr(P, s + st, n);
        P->t.end = P->pos;
        return;
    }
    if (isdigit(c) || (c == '.' && P->pos + 1 < P->len && isdigit((u8)s[P->pos + 1]))) { lex_number(P); P->t.end = P->pos; return; }
    if (c == '"' || c == '\'') { P->pos++; lex_string(P, (char)c); P->t.end = P->pos; return; }
    if (c == '`') { P->pos++; lex_template_part(P); return; }
    P->t.type = TK_PUNCT;
    P->pos++;
    char n1 = P->pos < P->len ? s[P->pos] : 0, n2 = P->pos + 1 < P->len ? s[P->pos + 1] : 0, n3 = P->pos + 2 < P->len ? s[P->pos + 2] : 0;
    int op = 0, aux = 0, adv = 0;
    switch (c) {
    case '{': op = P_LBRACE; break;
    case '}': op = P_RBRACE; break;
    case '(': op = P_LPAREN; break;
    case ')': op = P_RPAREN; break;
    case '[': op = P_LBRACK; break;
    case ']': op = P_RBRACK; break;
    case ';': op = P_SEMI; break;
    case ',': op = P_COMMA; break;
    case ':': op = P_COLON; break;
    case '~': op = P_TILDE; break;
    case '.': if (n1 == '.' && n2 == '.') { op = P_ELLIPSIS; adv = 2; } else op = P_DOT; break;
    case '?':
        if (n1 == '?') { if (n2 == '=') { op = P_OPASSIGN; aux = OP_NULLISH; adv = 2; } else { op = P_NULLISH; adv = 1; } }
        else if (n1 == '.' && !isdigit((u8)n2)) { op = P_QDOT; adv = 1; }
        else op = P_Q;
        break;
    case '=':
        if (n1 == '=' && n2 == '=') { op = P_SEQ; adv = 2; }
        else if (n1 == '=') { op = P_EQ; adv = 1; }
        else if (n1 == '>') { op = P_ARROW; adv = 1; }
        else op = P_ASSIGN;
        break;
    case '!':
        if (n1 == '=' && n2 == '=') { op = P_SNE; adv = 2; }
        else if (n1 == '=') { op = P_NE; adv = 1; }
        else op = P_NOT;
        break;
    case '<':
        if (n1 == '<' && n2 == '=') { op = P_OPASSIGN; aux = OP_SHL; adv = 2; }
        else if (n1 == '<') { op = P_SHL; adv = 1; }
        else if (n1 == '=') { op = P_LE; adv = 1; }
        else op = P_LT;
        break;
    case '>':
        if (n1 == '>' && n2 == '>' && n3 == '=') { op = P_OPASSIGN; aux = OP_USHR; adv = 3; }
        else if (n1 == '>' && n2 == '>') { op = P_USHR; adv = 2; }
        else if (n1 == '>' && n2 == '=') { op = P_OPASSIGN; aux = OP_SHR; adv = 2; }
        else if (n1 == '>') { op = P_SHR; adv = 1; }
        else if (n1 == '=') { op = P_GE; adv = 1; }
        else op = P_GT;
        break;
    case '+':
        if (n1 == '+') { op = P_INC; adv = 1; } else if (n1 == '=') { op = P_OPASSIGN; aux = OP_ADD; adv = 1; } else op = P_PLUS;
        break;
    case '-':
        if (n1 == '-') { op = P_DEC; adv = 1; } else if (n1 == '=') { op = P_OPASSIGN; aux = OP_SUB; adv = 1; } else op = P_MINUS;
        break;
    case '*':
        if (n1 == '*' && n2 == '=') { op = P_OPASSIGN; aux = OP_EXP; adv = 2; }
        else if (n1 == '*') { op = P_EXP; adv = 1; }
        else if (n1 == '=') { op = P_OPASSIGN; aux = OP_MUL; adv = 1; }
        else op = P_STAR;
        break;
    case '/': if (n1 == '=') { op = P_OPASSIGN; aux = OP_DIV; adv = 1; } else op = P_SLASH; break;
    case '%': if (n1 == '=') { op = P_OPASSIGN; aux = OP_MOD; adv = 1; } else op = P_PCT; break;
    case '&':
        if (n1 == '&' && n2 == '=') { op = P_OPASSIGN; aux = OP_AND; adv = 2; }
        else if (n1 == '&') { op = P_AND; adv = 1; }
        else if (n1 == '=') { op = P_OPASSIGN; aux = OP_BAND; adv = 1; }
        else op = P_AMP;
        break;
    case '|':
        if (n1 == '|' && n2 == '=') { op = P_OPASSIGN; aux = OP_OR; adv = 2; }
        else if (n1 == '|') { op = P_OR; adv = 1; }
        else if (n1 == '=') { op = P_OPASSIGN; aux = OP_BOR; adv = 1; }
        else op = P_BAR;
        break;
    case '^': if (n1 == '=') { op = P_OPASSIGN; aux = OP_BXOR; adv = 1; } else op = P_CARET; break;
    default:
        error_at(P, P->line, "Caractere inesperado '%c'", c >= 32 && c < 127 ? c : '?');
        return;
    }
    P->pos += adv;
    P->t.op = op;
    P->t.aux = aux;
    P->t.end = P->pos;
}

static void next(parser *P) { if (!P->failed) lex(P); }

typedef struct { int pos, line; token t; } lexstate;
static lexstate save_lex(parser *P) { lexstate s = {P->pos, P->line, P->t}; return s; }
static void restore_lex(parser *P, lexstate s) { P->pos = s.pos; P->line = s.line; P->t = s.t; }

static token peek(parser *P) {
    lexstate s = save_lex(P);
    lex(P);
    token r = P->t;
    restore_lex(P, s);
    return r;
}

static bool is_p(parser *P, int op) { return P->t.type == TK_PUNCT && P->t.op == op; }
static bool is_kw(parser *P, int k) { return P->t.type == TK_KW && P->t.op == k; }
static bool is_id(parser *P, jsstr *name) { return P->t.type == TK_IDENT && P->t.str == name; }

static const char *tok_desc(parser *P, char *buf, int cap) {
    token *t = &P->t;
    if (t->type == TK_EOF) return "fim do script";
    int n = MIN(t->end - t->start, cap - 1);
    if (n <= 0) return "?";
    memcpy(buf, P->s + t->start, (usize)n);
    buf[n] = 0;
    return buf;
}

static void unexpected(parser *P) {
    char b[40];
    if (P->t.type == TK_EOF) error_at(P, P->t.line, "Fim inesperado do script");
    else error_at(P, P->t.line, "Token inesperado '%s'", tok_desc(P, b, sizeof b));
}

static bool expect(parser *P, int op) {
    if (is_p(P, op)) { next(P); return true; }
    unexpected(P);
    return false;
}

static void consume_semi(parser *P) {
    if (is_p(P, P_SEMI)) { next(P); return; }
    if (is_p(P, P_RBRACE) || P->t.type == TK_EOF || P->t.nl) return;
    unexpected(P);
}

/* nome de propriedade após '.': identificadores e palavras reservadas */
static bool is_name_tok(parser *P) { return P->t.type == TK_IDENT || P->t.type == TK_KW; }

/* ============================================================ expressões */
static jsnode *parse_expr(parser *P);
static jsnode *parse_assign(parser *P);
static jsnode *parse_unary(parser *P);
static jsnode *parse_lhs(parser *P);
static jsnode *parse_statement(parser *P);
static jsnode *parse_function(parser *P, jsstr *name, int kind, int line);
static jsnode *parse_class(parser *P, bool is_decl);
static jsnode *parse_block_body(parser *P, jsnode *blk, bool fnbody);

enum { FK_NORMAL, FK_ARROW, FK_METHOD, FK_GETTER, FK_SETTER, FK_CTOR, FK_DERIVED_CTOR };

static jsnode *to_pattern(parser *P, jsnode *n, bool binding) {
    if (!n || P->failed) return n;
    switch (n->type) {
    case N_IDENT: return n;
    case N_MEMBER:
        if (binding) break;
        if (n->flags & NF_OPTIONAL) break;
        return n;
    case N_ARRAY:
        for (int i = 0; i < n->n; i++) {
            jsnode *e = n->list[i];
            if (e->type == N_ELISION) continue;
            if (e->type == N_SPREAD) { e->a = to_pattern(P, e->a, binding); continue; }
            n->list[i] = to_pattern(P, e, binding);
        }
        return n;
    case N_OBJECT:
        for (int i = 0; i < n->n; i++) {
            jsnode *p = n->list[i];
            if (p->flags & (NF_GETTER | NF_SETTER | NF_METHOD)) { error_at(P, p->line, "Padrão de desestruturação inválido"); return n; }
            p->b = to_pattern(P, p->b, binding);
        }
        return n;
    case N_ASSIGN:
        if (n->op == OP_ASSIGN) { n->a = to_pattern(P, n->a, binding); return n; }
        break;
    }
    error_at(P, n->line, "Alvo de atribuição inválido");
    return n;
}

/* coleta os nomes declarados por um padrão */
static void pattern_names(parser *P, jsnode *n, int kind) {
    if (!n || P->failed) return;
    switch (n->type) {
    case N_IDENT:
        if (kind == VK_VAR) add_var(P, n->str);
        else add_lex(P, n->str, kind == VK_CONST);
        break;
    case N_ARRAY:
        for (int i = 0; i < n->n; i++) {
            jsnode *e = n->list[i];
            if (e->type == N_ELISION) continue;
            pattern_names(P, e->type == N_SPREAD ? e->a : e, kind);
        }
        break;
    case N_OBJECT:
        for (int i = 0; i < n->n; i++) pattern_names(P, n->list[i]->b, kind);
        break;
    case N_ASSIGN: pattern_names(P, n->a, kind); break;
    }
}

static void set_fn_name(jsnode *v, jsstr *name) {
    if (!v || !name) return;
    if (v->type == N_FUNC) { jsfunc *f = v->x; if (f && !f->name) f->name = name; }
    else if (v->type == N_CLASS && !v->str) v->str = name;
}

static jsnode *parse_template(parser *P, jsnode *tag) {
    jsnode *t = mk(P, N_TEMPLATE);
    nlist l = {0};
    for (;;) {
        if (P->t.type != TK_TEMPLATE) { unexpected(P); break; }
        jsnode *q = mk(P, N_STR);
        q->str = P->t.str;
        nl_push(P, &l, q);
        if (P->t.tail) { next(P); break; }
        next(P);
        bool sv = P->noin;
        P->noin = false;
        jsnode *e = parse_expr(P);
        P->noin = sv;
        nl_push(P, &l, e);
        if (P->failed) break;
        if (!is_p(P, P_RBRACE)) { unexpected(P); break; }
        P->pos = P->t.start + 1;
        P->t.start = P->pos;
        lex_template_part(P);
        if (P->failed) break;
    }
    nl_finish(P, &l, t);
    if (tag) {
        jsnode *tg = mk(P, N_TAGGED);
        tg->a = tag;
        tg->b = t;
        return tg;
    }
    return t;
}

static jsnode *parse_regex(parser *P) {
    const char *s = P->s;
    int st = P->t.start + 1;
    int i = st;
    bool cls = false;
    while (i < P->len) {
        char c = s[i];
        if (c == '\n') break;
        if (c == '\\') { i += 2; continue; }
        if (c == '[') cls = true;
        else if (c == ']') cls = false;
        else if (c == '/' && !cls) break;
        i++;
    }
    if (i >= P->len || s[i] != '/') { error_at(P, P->t.line, "Expressão regular não terminada"); return mk(P, N_EMPTY); }
    int pend = i;
    i++;
    int fst = i;
    int flags = 0;
    while (i < P->len && id_part((u8)s[i])) {
        switch (s[i]) {
        case 'g': flags |= RXF_G; break;
        case 'i': flags |= RXF_I; break;
        case 'm': flags |= RXF_M; break;
        case 's': flags |= RXF_S; break;
        case 'y': flags |= RXF_Y; break;
        case 'u': flags |= RXF_U; break;
        default: error_at(P, P->t.line, "Flag de expressão regular inválida '%c'", s[i]); return mk(P, N_EMPTY);
        }
        i++;
    }
    jsnode *n = mk(P, N_REGEX);
    n->str = pstr(P, s + st, pend - st);
    jsstr *fl = pstr(P, s + fst, i - fst);
    char err[120];
    rxprog *prog = rx_compile(s + st, pend - st, flags, err, sizeof err);
    if (!prog) { error_at(P, P->t.line, "Expressão regular inválida: /%s/: %s", n->str->d, err); return n; }
    jsscript *sc = P->sc;
    if (sc->nrx == sc->crx) {
        int nc = sc->crx ? sc->crx * 2 : 8;
        rxprog **na = krealloc(sc->rx, sizeof(rxprog *) * (usize)nc);
        if (!na) { rx_unref(prog); error_at(P, P->t.line, "Memória insuficiente"); return n; }
        sc->rx = na;
        sc->crx = nc;
    }
    sc->rx[sc->nrx++] = prog;
    void **pair = palloc(P, sizeof(void *) * 2);
    if (pair) { pair[0] = prog; pair[1] = fl; }
    n->x = pair;
    n->num = flags;
    P->pos = i;
    lex(P);
    return n;
}

static jsnode *parse_array_lit(parser *P) {
    jsnode *a = mk(P, N_ARRAY);
    next(P);
    nlist l = {0};
    bool sv = P->noin;
    P->noin = false;
    while (!is_p(P, P_RBRACK) && !P->failed) {
        if (is_p(P, P_COMMA)) { nl_push(P, &l, mk(P, N_ELISION)); next(P); continue; }
        jsnode *e;
        if (is_p(P, P_ELLIPSIS)) { e = mk(P, N_SPREAD); next(P); e->a = parse_assign(P); }
        else e = parse_assign(P);
        nl_push(P, &l, e);
        if (is_p(P, P_COMMA)) next(P);
        else if (!is_p(P, P_RBRACK)) { unexpected(P); break; }
    }
    P->noin = sv;
    next(P);
    nl_finish(P, &l, a);
    return a;
}

/* chave de propriedade: devolve N_STR (estática) ou expressão computada (flags NF_COMPUTED no dono) */
static jsnode *parse_prop_key(parser *P, bool *computed) {
    *computed = false;
    jsnode *k;
    if (is_p(P, P_LBRACK)) {
        next(P);
        bool sv = P->noin;
        P->noin = false;
        k = parse_assign(P);
        P->noin = sv;
        expect(P, P_RBRACK);
        *computed = true;
        return k;
    }
    k = mk(P, N_STR);
    if (P->t.type == TK_IDENT || P->t.type == TK_KW || P->t.type == TK_STR) k->str = P->t.str;
    else if (P->t.type == TK_NUM) { jsstr *ns = js_num_tostr(P->t.num); k->str = pstr(P, ns->d, (int)ns->len); }
    else { unexpected(P); return k; }
    next(P);
    return k;
}

static jsnode *parse_object_lit(parser *P) {
    jsnode *o = mk(P, N_OBJECT);
    next(P);
    nlist l = {0};
    bool sv = P->noin;
    P->noin = false;
    while (!is_p(P, P_RBRACE) && !P->failed) {
        jsnode *p = mk(P, N_PROP);
        if (is_p(P, P_ELLIPSIS)) {
            next(P);
            p->flags |= NF_SPREADPROP;
            p->b = parse_assign(P);
        } else {
            int kind = 0;
            if (P->t.type == TK_IDENT && (P->t.str == A[A_get] || P->t.str == A[A_set])) {
                token nx = peek(P);
                if (!(nx.type == TK_PUNCT && (nx.op == P_COLON || nx.op == P_LPAREN || nx.op == P_COMMA || nx.op == P_RBRACE || nx.op == P_ASSIGN))) {
                    kind = P->t.str == A[A_get] ? 1 : 2;
                    next(P);
                }
            }
            bool m_async = false, m_gen = false;
            if (!kind && P->t.type == TK_IDENT && P->t.str == A[A_async]) {
                token nx = peek(P);
                if (!(nx.type == TK_PUNCT && (nx.op == P_COLON || nx.op == P_LPAREN || nx.op == P_COMMA || nx.op == P_RBRACE || nx.op == P_ASSIGN)) && !nx.nl) {
                    m_async = true;
                    next(P);
                }
            }
            if (!kind && is_p(P, P_STAR)) {
                m_gen = true;
                next(P);
            }
            bool is_ident = P->t.type == TK_IDENT;
            jsstr *idname = P->t.str;
            int kline = P->t.line;
            bool comp;
            p->a = parse_prop_key(P, &comp);
            if (comp) p->flags |= NF_COMPUTED;
            if (kind) {
                p->flags |= kind == 1 ? NF_GETTER : NF_SETTER;
                p->b = parse_function(P, comp ? NULL : p->a->str, kind == 1 ? FK_GETTER : FK_SETTER, kline);
            } else if (is_p(P, P_LPAREN)) {
                p->flags |= NF_METHOD;
                P->next_gen = m_gen;
                P->next_async = m_async;
                p->b = parse_function(P, comp ? NULL : p->a->str, FK_METHOD, kline);
            } else if (m_gen || m_async) {
                unexpected(P);
                break;
            } else if (is_p(P, P_COLON)) {
                next(P);
                p->b = parse_assign(P);
                if (!comp) set_fn_name(p->b, p->a->str);
            } else if (is_ident && !comp) {
                jsnode *id = mk(P, N_IDENT);
                id->str = idname;
                id->line = kline;
                p->flags |= NF_SHORTHAND;
                if (is_p(P, P_ASSIGN)) {
                    /* {a = 1}: só válido como padrão (gramática de cobertura) */
                    next(P);
                    jsnode *as = mk(P, N_ASSIGN);
                    as->op = OP_ASSIGN;
                    as->a = id;
                    as->b = parse_assign(P);
                    p->b = as;
                } else p->b = id;
            } else { unexpected(P); break; }
        }
        nl_push(P, &l, p);
        if (is_p(P, P_COMMA)) next(P);
        else if (!is_p(P, P_RBRACE)) { unexpected(P); break; }
    }
    P->noin = sv;
    next(P);
    nl_finish(P, &l, o);
    return o;
}

static jsnode *make_arrow(parser *P, nlist *params, int line) {
    /* params já convertidos em padrões */
    jsnode *fnode = mk(P, N_FUNC);
    fnode->line = line;
    jsfunc *f = palloc(P, sizeof(jsfunc));
    if (!f) return fnode;
    f->is_arrow = 1;
    f->is_async = P->next_async;
    P->next_async = false;
    f->line = line;
    fnode->x = f;
    f->nparams = params->n;
    f->simple_params = 1;
    f->length = -1;
    if (params->n) {
        f->params = palloc(P, sizeof(jsnode *) * (usize)params->n);
        if (f->params) memcpy(f->params, params->v, sizeof(jsnode *) * (usize)params->n);
        for (int i = 0; i < params->n; i++) {
            jsnode *pn = params->v[i];
            if (pn->type != N_IDENT) { f->simple_params = 0; if (f->length < 0 && (pn->type == N_ASSIGN || pn->type == N_SPREAD)) f->length = i; }
        }
    }
    if (f->length < 0) f->length = params->n;
    kfree(params->v);
    params->v = NULL;
    /* corpo */
    if (P->nf >= 255) { error_at(P, line, "Funções aninhadas demais"); return fnode; }
    jsfunc *savef = P->fn;
    jsblock *saveb = P->blk, *savefb = P->fnblk;
    bool sv = P->noin;
    P->fstack[P->nf++] = f;
    P->fn = f;
    /* arrow: var/argumentos pertencem à própria arrow para var; arguments/this vêm de fora */
    if (is_p(P, P_LBRACE)) {
        jsnode *body = mk(P, N_BLOCK);
        jsblock *b = palloc(P, sizeof(jsblock));
        body->x = b;
        P->blk = b;
        P->fnblk = b;
        P->noin = false;
        parse_block_body(P, body, true);
        f->body = body;
    } else {
        jsblock *b = palloc(P, sizeof(jsblock));
        P->blk = b;
        P->fnblk = b;
        f->expr_body = 1;
        f->body = parse_assign(P);
    }
    for (int i = 0; i < f->nparams; i++) pattern_names(P, f->params[i]->type == N_SPREAD ? f->params[i]->a : f->params[i], VK_VAR);
    P->noin = sv;
    P->nf--;
    P->fn = savef;
    P->blk = saveb;
    P->fnblk = savefb;
    return fnode;
}

static jsnode *parse_paren(parser *P) {
    int line = P->t.line;
    bool as = P->next_async;
    P->next_async = false;
    next(P);
    bool sv = P->noin;
    P->noin = false;
    nlist items = {0};
    bool rest = false, trailing = false;
    if (is_p(P, P_RPAREN)) {
        next(P);
        P->noin = sv;
        if (!is_p(P, P_ARROW) || P->t.nl) { unexpected(P); return mk(P, N_EMPTY); }
        next(P);
        P->next_async = as;
        return make_arrow(P, &items, line);
    }
    while (!P->failed) {
        if (is_p(P, P_ELLIPSIS)) {
            jsnode *s = mk(P, N_SPREAD);
            next(P);
            s->a = parse_assign(P);
            nl_push(P, &items, s);
            rest = true;
            if (!is_p(P, P_RPAREN)) { unexpected(P); break; }
            break;
        }
        nl_push(P, &items, parse_assign(P));
        if (is_p(P, P_COMMA)) {
            next(P);
            if (is_p(P, P_RPAREN)) { trailing = true; break; }
            continue;
        }
        break;
    }
    if (!expect(P, P_RPAREN)) { kfree(items.v); return mk(P, N_EMPTY); }
    P->noin = sv;
    if (is_p(P, P_ARROW) && !P->t.nl) {
        next(P);
        for (int i = 0; i < items.n; i++) {
            jsnode *it = items.v[i];
            if (it->type == N_SPREAD) it->a = to_pattern(P, it->a, true);
            else items.v[i] = to_pattern(P, it, true);
        }
        P->next_async = as;
        return make_arrow(P, &items, line);
    }
    if (rest || trailing || !items.n) { kfree(items.v); unexpected(P); return mk(P, N_EMPTY); }
    if (items.n == 1) { jsnode *r = items.v[0]; kfree(items.v); return r; }
    jsnode *seq = mk(P, N_SEQ);
    seq->line = line;
    nl_finish(P, &items, seq);
    return seq;
}

static jsnode *parse_primary(parser *P) {
    jsnode *n;
    if (!stack_ok(P)) return mk(P, N_EMPTY);
    switch (P->t.type) {
    case TK_NUM: n = mk(P, N_NUM); n->num = P->t.num; next(P); return n;
    case TK_BIGINT: n = mk(P, N_BIGINT); n->str = P->t.str; next(P); return n;
    case TK_STR: n = mk(P, N_STR); n->str = P->t.str; next(P); return n;
    case TK_TEMPLATE: return parse_template(P, NULL);
    case TK_IDENT: {
        jsstr *name = P->t.str;
        int line = P->t.line;
        if (name == A[A_async]) {
            token nx = peek(P);
            if (nx.type == TK_KW && nx.op == KW_FUNCTION && !nx.nl) {
                next(P);
                next(P);
                if (is_p(P, P_STAR)) { next(P); P->next_gen = true; }
                jsstr *fname = NULL;
                if (P->t.type == TK_IDENT) { fname = P->t.str; next(P); }
                P->next_async = true;
                return parse_function(P, fname, FK_NORMAL, line);
            }
            if (nx.type == TK_IDENT && !nx.nl) {
                next(P);
                jsstr *pname = P->t.str;
                int pline = P->t.line;
                next(P);
                if (!is_p(P, P_ARROW) || P->t.nl) { unexpected(P); return mk(P, N_EMPTY); }
                next(P);
                nlist ps = {0};
                jsnode *id = mk(P, N_IDENT);
                id->str = pname;
                id->line = pline;
                nl_push(P, &ps, id);
                P->next_async = true;
                return make_arrow(P, &ps, line);
            }
            if (nx.type == TK_PUNCT && nx.op == P_LPAREN && !nx.nl) {
                next(P);
                P->next_async = true;
                jsnode *r = parse_paren(P);
                P->next_async = false;
                if (r->type == N_FUNC && r->x && ((jsfunc *)r->x)->is_arrow) return r;
                /* era uma chamada comum: async(...) */
                jsnode *call = mk(P, N_CALL);
                call->line = line;
                call->a = mk(P, N_IDENT);
                call->a->str = name;
                if (r->type == N_SEQ) { call->list = r->list; call->n = r->n; }
                else if (r->type != N_EMPTY) {
                    call->list = palloc(P, sizeof(jsnode *));
                    if (call->list) { call->list[0] = r; call->n = 1; }
                }
                return call;
            }
        }
        next(P);
        if (is_p(P, P_ARROW) && !P->t.nl) {
            next(P);
            nlist ps = {0};
            jsnode *id = mk(P, N_IDENT);
            id->str = name;
            id->line = line;
            nl_push(P, &ps, id);
            return make_arrow(P, &ps, line);
        }
        n = mk(P, N_IDENT);
        n->str = name;
        n->line = line;
        if (name == A[A_arguments]) {
            for (int i = P->nf - 1; i >= 0; i--) if (!P->fstack[i]->is_arrow) { P->fstack[i]->uses_args = 1; break; }
        }
        return n;
    }
    case TK_KW:
        switch (P->t.op) {
        case KW_THIS: n = mk(P, N_THIS); next(P); return n;
        case KW_NULL: n = mk(P, N_IDENT); n->op = 1; n->str = A[A_null]; next(P); return n;
        case KW_TRUE: n = mk(P, N_IDENT); n->op = 2; n->str = A[A_true]; next(P); return n;
        case KW_FALSE: n = mk(P, N_IDENT); n->op = 3; n->str = A[A_false]; next(P); return n;
        case KW_FUNCTION: {
            int line = P->t.line;
            next(P);
            if (is_p(P, P_STAR)) { next(P); P->next_gen = true; }
            jsstr *name = NULL;
            if (P->t.type == TK_IDENT) { name = P->t.str; next(P); }
            return parse_function(P, name, FK_NORMAL, line);
        }
        case KW_CLASS: return parse_class(P, false);
        case KW_SUPER: n = mk(P, N_SUPER); next(P); return n;
        case KW_NEW: return parse_lhs(P);
        case KW_IMPORT: {
            int line = P->t.line;
            next(P);
            if (is_p(P, P_LPAREN)) {
                next(P);
                n = mk(P, N_IMPORTCALL);
                n->line = line;
                bool sv = P->noin;
                P->noin = false;
                n->a = parse_assign(P);
                P->noin = sv;
                expect(P, P_RPAREN);
                return n;
            }
            if (is_p(P, P_DOT)) {
                next(P);
                if (P->t.type == TK_IDENT && !strcmp(P->t.str->d, "meta")) { next(P); n = mk(P, N_IMPORTMETA); n->line = line; return n; }
            }
            unexpected(P);
            return mk(P, N_EMPTY);
        }
        }
        break;
    case TK_PUNCT:
        switch (P->t.op) {
        case P_LPAREN: return parse_paren(P);
        case P_LBRACK: return parse_array_lit(P);
        case P_LBRACE: return parse_object_lit(P);
        case P_SLASH: return parse_regex(P);
        case P_OPASSIGN: if (P->t.aux == OP_DIV) return parse_regex(P); break;
        }
        break;
    }
    unexpected(P);
    return mk(P, N_EMPTY);
}

static void parse_args(parser *P, jsnode *call) {
    next(P);   /* ( */
    nlist l = {0};
    bool sv = P->noin;
    P->noin = false;
    while (!is_p(P, P_RPAREN) && !P->failed) {
        jsnode *e;
        if (is_p(P, P_ELLIPSIS)) { e = mk(P, N_SPREAD); next(P); e->a = parse_assign(P); }
        else e = parse_assign(P);
        nl_push(P, &l, e);
        if (is_p(P, P_COMMA)) next(P);
        else if (!is_p(P, P_RPAREN)) { unexpected(P); break; }
    }
    P->noin = sv;
    next(P);
    nl_finish(P, &l, call);
}

static jsnode *parse_member_tail(parser *P, jsnode *e, bool allow_call, bool *has_opt) {
    for (;;) {
        if (P->failed) return e;
        int line = P->t.line;
        if (is_p(P, P_DOT)) {
            next(P);
            if (!is_name_tok(P)) { unexpected(P); return e; }
            jsnode *m = mk(P, N_MEMBER);
            m->line = line;
            m->a = e;
            m->b = mk(P, N_STR);
            m->b->str = P->t.str;
            next(P);
            e = m;
        } else if (is_p(P, P_QDOT)) {
            if (!allow_call) { unexpected(P); return e; }
            next(P);
            *has_opt = true;
            if (is_p(P, P_LPAREN)) {
                jsnode *c = mk(P, N_CALL);
                c->line = line;
                c->a = e;
                c->flags |= NF_OPTIONAL;
                parse_args(P, c);
                e = c;
            } else if (is_p(P, P_LBRACK)) {
                next(P);
                jsnode *m = mk(P, N_MEMBER);
                m->line = line;
                m->a = e;
                m->flags |= NF_COMPUTED | NF_OPTIONAL;
                m->b = parse_expr(P);
                expect(P, P_RBRACK);
                e = m;
            } else if (is_name_tok(P)) {
                jsnode *m = mk(P, N_MEMBER);
                m->line = line;
                m->a = e;
                m->flags |= NF_OPTIONAL;
                m->b = mk(P, N_STR);
                m->b->str = P->t.str;
                next(P);
                e = m;
            } else { unexpected(P); return e; }
        } else if (is_p(P, P_LBRACK)) {
            next(P);
            jsnode *m = mk(P, N_MEMBER);
            m->line = line;
            m->a = e;
            m->flags |= NF_COMPUTED;
            bool sv = P->noin;
            P->noin = false;
            m->b = parse_expr(P);
            P->noin = sv;
            expect(P, P_RBRACK);
            e = m;
        } else if (is_p(P, P_LPAREN) && allow_call) {
            jsnode *c = mk(P, N_CALL);
            c->line = line;
            c->a = e;
            parse_args(P, c);
            e = c;
        } else if (P->t.type == TK_TEMPLATE) {
            e = parse_template(P, e);
        } else return e;
    }
}

static jsnode *parse_lhs(parser *P) {
    jsnode *e;
    bool has_opt = false;
    if (!stack_ok(P)) return mk(P, N_EMPTY);
    if (is_kw(P, KW_NEW)) {
        int line = P->t.line;
        next(P);
        if (is_p(P, P_DOT)) {
            next(P);
            if (P->t.type == TK_IDENT && !strcmp(P->t.str->d, "target")) { next(P); e = mk(P, N_NEWTARGET); }
            else { unexpected(P); return mk(P, N_EMPTY); }
        } else {
            jsnode *callee;
            if (is_kw(P, KW_NEW)) callee = parse_lhs(P);
            else {
                callee = parse_primary(P);
                bool dummy = false;
                callee = parse_member_tail(P, callee, false, &dummy);
            }
            e = mk(P, N_NEW);
            e->line = line;
            e->a = callee;
            if (is_p(P, P_LPAREN)) parse_args(P, e);
        }
    } else e = parse_primary(P);
    e = parse_member_tail(P, e, true, &has_opt);
    if (has_opt) {
        jsnode *oc = mk(P, N_OPTCHAIN);
        oc->a = e;
        oc->line = e->line;
        return oc;
    }
    return e;
}

static jsnode *parse_postfix(parser *P) {
    jsnode *e = parse_lhs(P);
    if ((is_p(P, P_INC) || is_p(P, P_DEC)) && !P->t.nl) {
        if (e->type != N_IDENT && e->type != N_MEMBER) { error_at(P, P->t.line, "Operando inválido para ++/--"); return e; }
        jsnode *u = mk(P, N_UPDATE);
        u->op = is_p(P, P_INC) ? OP_INC : OP_DEC;
        u->a = e;
        next(P);
        return u;
    }
    return e;
}

static jsnode *parse_unary(parser *P) {
    if (!stack_ok(P)) return mk(P, N_EMPTY);
    int op = 0;
    if (P->t.type == TK_PUNCT) {
        switch (P->t.op) {
        case P_NOT: op = OP_NOT; break;
        case P_MINUS: op = OP_NEG; break;
        case P_PLUS: op = OP_PLUS; break;
        case P_TILDE: op = OP_BNOT; break;
        case P_INC: case P_DEC: {
            jsnode *u = mk(P, N_UPDATE);
            u->op = P->t.op == P_INC ? OP_INC : OP_DEC;
            u->flags |= NF_PREFIX;
            next(P);
            u->a = parse_unary(P);
            if (u->a->type != N_IDENT && u->a->type != N_MEMBER) error_at(P, u->line, "Operando inválido para ++/--");
            return u;
        }
        }
    } else if (P->t.type == TK_KW) {
        if (P->t.op == KW_TYPEOF) op = OP_TYPEOF;
        else if (P->t.op == KW_VOID) op = OP_VOID;
        else if (P->t.op == KW_DELETE) op = OP_DELETE;
    } else if (P->t.type == TK_IDENT && P->t.str == A[A_await] && P->fn && P->fn->is_async) {
        jsnode *u = mk(P, N_AWAIT);
        next(P);
        u->a = parse_unary(P);
        return u;
    }
    if (op) {
        jsnode *u = mk(P, N_UNARY);
        u->op = (u8)op;
        next(P);
        u->a = parse_unary(P);
        return u;
    }
    return parse_postfix(P);
}

static int binprec(parser *P, int *op, int *ntype) {
    *ntype = N_BINARY;
    if (P->t.type == TK_KW) {
        if (P->t.op == KW_INSTANCEOF) { *op = OP_INSTANCEOF; return 10; }
        if (P->t.op == KW_IN && !P->noin) { *op = OP_IN; return 10; }
        return -1;
    }
    if (P->t.type != TK_PUNCT) return -1;
    switch (P->t.op) {
    case P_NULLISH: *op = OP_NULLISH; *ntype = N_LOGICAL; return 1;
    case P_OR: *op = OP_OR; *ntype = N_LOGICAL; return 2;
    case P_AND: *op = OP_AND; *ntype = N_LOGICAL; return 3;
    case P_BAR: *op = OP_BOR; return 4;
    case P_CARET: *op = OP_BXOR; return 5;
    case P_AMP: *op = OP_BAND; return 6;
    case P_EQ: *op = OP_EQ; return 7;
    case P_NE: *op = OP_NE; return 7;
    case P_SEQ: *op = OP_SEQ; return 7;
    case P_SNE: *op = OP_SNE; return 7;
    case P_LT: *op = OP_LT; return 10;
    case P_GT: *op = OP_GT; return 10;
    case P_LE: *op = OP_LE; return 10;
    case P_GE: *op = OP_GE; return 10;
    case P_SHL: *op = OP_SHL; return 11;
    case P_SHR: *op = OP_SHR; return 11;
    case P_USHR: *op = OP_USHR; return 11;
    case P_PLUS: *op = OP_ADD; return 12;
    case P_MINUS: *op = OP_SUB; return 12;
    case P_STAR: *op = OP_MUL; return 13;
    case P_SLASH: *op = OP_DIV; return 13;
    case P_PCT: *op = OP_MOD; return 13;
    case P_EXP: *op = OP_EXP; return 14;
    }
    return -1;
}

static jsnode *parse_binary(parser *P, int minprec) {
    jsnode *left = parse_unary(P);
    for (;;) {
        if (P->failed) return left;
        int op, nt;
        int prec = binprec(P, &op, &nt);
        if (prec < 0 || prec < minprec) return left;
        int line = P->t.line;
        next(P);
        jsnode *right = parse_binary(P, op == OP_EXP ? prec : prec + 1);
        jsnode *b = mk(P, nt);
        b->line = line;
        b->op = (u8)op;
        b->a = left;
        b->b = right;
        left = b;
    }
}

static jsnode *parse_cond(parser *P) {
    jsnode *c = parse_binary(P, 1);
    if (!is_p(P, P_Q)) return c;
    jsnode *n = mk(P, N_COND);
    next(P);
    n->a = c;
    bool sv = P->noin;
    P->noin = false;
    n->b = parse_assign(P);
    P->noin = sv;
    expect(P, P_COLON);
    n->c = parse_assign(P);
    return n;
}

static jsnode *parse_assign(parser *P) {
    if (!stack_ok(P)) return mk(P, N_EMPTY);
    if (P->t.type == TK_IDENT && P->t.str == A[A_yield] && P->fn && P->fn->is_gen) {
        jsnode *y = mk(P, N_YIELD);
        next(P);
        if (is_p(P, P_STAR)) { y->op = 1; next(P); y->a = parse_assign(P); return y; }
        bool end = P->t.type == TK_EOF || P->t.nl ||
                   (P->t.type == TK_PUNCT && (P->t.op == P_RPAREN || P->t.op == P_RBRACK || P->t.op == P_RBRACE ||
                                              P->t.op == P_COMMA || P->t.op == P_SEMI || P->t.op == P_COLON));
        if (!end) y->a = parse_assign(P);
        return y;
    }
    jsnode *left = parse_cond(P);
    if (P->failed) return left;
    if (is_p(P, P_ASSIGN) || is_p(P, P_OPASSIGN)) {
        int op = is_p(P, P_ASSIGN) ? OP_ASSIGN : P->t.aux;
        int line = P->t.line;
        if (op == OP_ASSIGN && (left->type == N_ARRAY || left->type == N_OBJECT)) left = to_pattern(P, left, false);
        else if (left->type != N_IDENT && left->type != N_MEMBER) { error_at(P, line, "Lado esquerdo inválido na atribuição"); return left; }
        else if (left->type == N_IDENT && left->op) { error_at(P, line, "Lado esquerdo inválido na atribuição"); return left; }
        next(P);
        jsnode *n = mk(P, N_ASSIGN);
        n->line = line;
        n->op = (u8)op;
        n->a = left;
        n->b = parse_assign(P);
        if (left->type == N_IDENT) set_fn_name(n->b, left->str);
        return n;
    }
    return left;
}

static jsnode *parse_expr(parser *P) {
    jsnode *e = parse_assign(P);
    if (!is_p(P, P_COMMA)) return e;
    jsnode *seq = mk(P, N_SEQ);
    nlist l = {0};
    nl_push(P, &l, e);
    while (is_p(P, P_COMMA) && !P->failed) {
        next(P);
        nl_push(P, &l, parse_assign(P));
    }
    nl_finish(P, &l, seq);
    return seq;
}

/* ============================================================ funções e classes */
static jsnode *parse_function(parser *P, jsstr *name, int kind, int line) {
    jsnode *fnode = mk(P, N_FUNC);
    fnode->line = line;
    jsfunc *f = palloc(P, sizeof(jsfunc));
    if (!f) return fnode;
    fnode->x = f;
    f->name = name;
    f->line = line;
    f->is_gen = P->next_gen;
    f->is_async = P->next_async;
    P->next_gen = P->next_async = false;
    f->is_method = kind == FK_METHOD || kind == FK_GETTER || kind == FK_SETTER;
    f->is_getter = kind == FK_GETTER;
    f->is_ctor = kind == FK_CTOR || kind == FK_DERIVED_CTOR;
    f->is_derived = kind == FK_DERIVED_CTOR;
    if (P->nf >= 255) { error_at(P, line, "Funções aninhadas demais"); return fnode; }
    jsfunc *savef = P->fn;
    jsblock *saveb = P->blk, *savefb = P->fnblk;
    bool sv = P->noin;
    P->noin = false;
    P->fstack[P->nf++] = f;
    P->fn = f;
    jsnode *body = mk(P, N_BLOCK);
    jsblock *b = palloc(P, sizeof(jsblock));
    body->x = b;
    P->blk = b;
    P->fnblk = b;
    /* parâmetros */
    nlist ps = {0};
    f->simple_params = 1;
    f->length = -1;
    if (expect(P, P_LPAREN)) {
        while (!is_p(P, P_RPAREN) && !P->failed) {
            jsnode *pn;
            if (is_p(P, P_ELLIPSIS)) {
                pn = mk(P, N_SPREAD);
                next(P);
                pn->a = to_pattern(P, parse_assign(P), true);
                f->simple_params = 0;
                if (f->length < 0) f->length = ps.n;
                nl_push(P, &ps, pn);
                break;
            }
            pn = to_pattern(P, parse_assign(P), true);
            if (pn->type != N_IDENT) { f->simple_params = 0; if (pn->type == N_ASSIGN && f->length < 0) f->length = ps.n; }
            nl_push(P, &ps, pn);
            if (is_p(P, P_COMMA)) next(P);
            else if (!is_p(P, P_RPAREN)) { unexpected(P); break; }
        }
        expect(P, P_RPAREN);
    }
    if (f->length < 0) f->length = ps.n;
    f->nparams = ps.n;
    if (ps.n) {
        f->params = palloc(P, sizeof(jsnode *) * (usize)ps.n);
        if (f->params) memcpy(f->params, ps.v, sizeof(jsnode *) * (usize)ps.n);
    }
    kfree(ps.v);
    for (int i = 0; i < f->nparams && f->params; i++) pattern_names(P, f->params[i]->type == N_SPREAD ? f->params[i]->a : f->params[i], VK_VAR);
    if (!is_p(P, P_LBRACE)) unexpected(P);
    else parse_block_body(P, body, true);
    f->body = body;
    P->noin = sv;
    P->nf--;
    P->fn = savef;
    P->blk = saveb;
    P->fnblk = savefb;
    return fnode;
}

static jsnode *parse_class(parser *P, bool is_decl) {
    jsnode *c = mk(P, N_CLASS);
    next(P);   /* class */
    if (P->t.type == TK_IDENT) { c->str = P->t.str; next(P); }
    else if (is_decl) { unexpected(P); return c; }
    if (is_kw(P, KW_EXTENDS)) {
        next(P);
        c->a = parse_lhs(P);
    }
    if (!expect(P, P_LBRACE)) return c;
    nlist l = {0};
    while (!is_p(P, P_RBRACE) && !P->failed) {
        if (is_p(P, P_SEMI)) { next(P); continue; }
        jsnode *m = mk(P, N_CLASSMEMBER);
        int line = P->t.line;
        if (P->t.type == TK_IDENT && P->t.str == A[A_static]) {
            token nx = peek(P);
            if (!(nx.type == TK_PUNCT && (nx.op == P_LPAREN || nx.op == P_ASSIGN || nx.op == P_SEMI))) { m->flags |= NF_STATIC; next(P); }
        }
        int kind = 0;
        if (P->t.type == TK_IDENT && (P->t.str == A[A_get] || P->t.str == A[A_set])) {
            token nx = peek(P);
            if (!(nx.type == TK_PUNCT && (nx.op == P_LPAREN || nx.op == P_ASSIGN || nx.op == P_SEMI || nx.op == P_RBRACE)) && !(nx.nl && nx.type != TK_IDENT)) {
                kind = P->t.str == A[A_get] ? 1 : 2;
                next(P);
            }
        }
        bool c_async = false, c_gen = false;
        if (!kind && P->t.type == TK_IDENT && P->t.str == A[A_async]) {
            token nx = peek(P);
            if (!(nx.type == TK_PUNCT && (nx.op == P_LPAREN || nx.op == P_ASSIGN || nx.op == P_SEMI)) && !nx.nl) { c_async = true; next(P); }
        }
        if (!kind && is_p(P, P_STAR)) {
            c_gen = true;
            next(P);
        }
        bool comp;
        m->a = parse_prop_key(P, &comp);
        if (comp) m->flags |= NF_COMPUTED;
        if (is_p(P, P_LPAREN)) {
            bool is_ctor = !comp && !(m->flags & NF_STATIC) && !kind && m->a->str && !strcmp(m->a->str->d, "constructor");
            if (is_ctor) {
                if (c->c) { error_at(P, line, "Classe com mais de um construtor"); break; }
                c->c = parse_function(P, c->str, c->a ? FK_DERIVED_CTOR : FK_CTOR, line);
                continue;
            }
            m->flags |= kind == 1 ? NF_GETTER : kind == 2 ? NF_SETTER : NF_METHOD;
            P->next_gen = c_gen;
            P->next_async = c_async;
            m->b = parse_function(P, comp ? NULL : m->a->str, kind == 1 ? FK_GETTER : kind == 2 ? FK_SETTER : FK_METHOD, line);
            jsfunc *mf = m->b->x;
            if (mf && (m->flags & NF_STATIC)) mf->is_static = 1;
        } else {
            /* campo */
            m->flags |= NF_FIELD;
            if (is_p(P, P_ASSIGN)) {
                next(P);
                /* o inicializador é avaliado como o corpo de um método (this = instância) */
                jsfunc *savef = P->fn;
                jsfunc *ff = P->nf < 250 ? palloc(P, sizeof(jsfunc)) : NULL;
                if (ff) {
                    ff->is_method = 1;
                    ff->expr_body = 1;
                    ff->line = line;
                    P->fstack[P->nf++] = ff;
                    P->fn = ff;
                    jsblock *saveb = P->blk;
                    P->blk = palloc(P, sizeof(jsblock));
                    jsnode *init = parse_assign(P);
                    P->blk = saveb;
                    P->nf--;
                    P->fn = savef;
                    ff->body = init;
                    if (!comp) set_fn_name(init, m->a->str);
                    jsnode *fn = mk(P, N_FUNC);
                    fn->x = ff;
                    m->b = fn;
                }
            }
            consume_semi(P);
        }
        nl_push(P, &l, m);
    }
    next(P);
    nl_finish(P, &l, c);
    return c;
}

/* ============================================================ instruções */
static jsnode *parse_var(parser *P, int kind, bool for_head) {
    jsnode *v = mk(P, N_VAR);
    v->op = (u8)kind;
    next(P);
    nlist l = {0};
    for (;;) {
        jsnode *d = mk(P, N_DECL);
        if (P->t.type == TK_IDENT) {
            d->a = mk(P, N_IDENT);
            d->a->str = P->t.str;
            next(P);
        } else if (is_p(P, P_LBRACK)) d->a = to_pattern(P, parse_array_lit(P), true);
        else if (is_p(P, P_LBRACE)) d->a = to_pattern(P, parse_object_lit(P), true);
        else { unexpected(P); break; }
        if (P->failed) break;
        if (is_p(P, P_ASSIGN)) {
            next(P);
            d->b = parse_assign(P);
            if (d->a->type == N_IDENT) set_fn_name(d->b, d->a->str);
        } else if (!for_head) {
            if (kind == VK_CONST) { error_at(P, d->line, "Falta o inicializador na declaração const"); break; }
            if (d->a->type != N_IDENT) { error_at(P, d->line, "Falta o inicializador na desestruturação"); break; }
        }
        pattern_names(P, d->a, kind);
        nl_push(P, &l, d);
        if (!is_p(P, P_COMMA)) break;
        next(P);
    }
    nl_finish(P, &l, v);
    return v;
}

static jsnode *parse_block_body(parser *P, jsnode *blk, bool fnbody) {
    (void)fnbody;
    next(P);   /* { */
    nlist l = {0};
    while (!is_p(P, P_RBRACE) && P->t.type != TK_EOF && !P->failed) nl_push(P, &l, parse_statement(P));
    if (!P->failed) expect(P, P_RBRACE);
    nl_finish(P, &l, blk);
    return blk;
}

static jsnode *parse_block(parser *P) {
    jsnode *b = mk(P, N_BLOCK);
    jsblock *jb = palloc(P, sizeof(jsblock));
    jsblock *save = P->blk;
    P->blk = jb;
    parse_block_body(P, b, false);
    P->blk = save;
    if (jb && (jb->n || jb->nfuncs)) b->x = jb;
    return b;
}

/* corpo de laço/if: uma instrução (declarações léxicas soltas não são permitidas) */
static jsnode *parse_sub(parser *P) {
    jsblock *jb = palloc(P, sizeof(jsblock));
    jsblock *save = P->blk;
    P->blk = jb;
    jsnode *s = parse_statement(P);
    P->blk = save;
    if (jb && (jb->n || jb->nfuncs)) {
        jsnode *b = mk(P, N_BLOCK);
        b->line = s->line;
        b->n = 1;
        b->list = palloc(P, sizeof(jsnode *));
        if (b->list) b->list[0] = s;
        b->x = jb;
        return b;
    }
    return s;
}

static bool is_let_decl(parser *P) {
    if (P->t.type != TK_IDENT || P->t.str != A[A_let]) return false;
    token nx = peek(P);
    return nx.type == TK_IDENT || (nx.type == TK_PUNCT && (nx.op == P_LBRACK || nx.op == P_LBRACE)) ||
           (nx.type == TK_KW && (nx.op != KW_IN && nx.op != KW_INSTANCEOF));
}

static jsnode *parse_for(parser *P) {
    int line = P->t.line;
    next(P);
    bool is_await = false;
    if (P->t.type == TK_IDENT && P->t.str == A[A_await]) {
        if (!P->fn || !P->fn->is_async) { error_at(P, line, "for await só é válido dentro de funções async"); return mk(P, N_EMPTY); }
        is_await = true;
        next(P);
    }
    if (!expect(P, P_LPAREN)) return mk(P, N_EMPTY);
    jsblock *jb = palloc(P, sizeof(jsblock));
    jsblock *save = P->blk;
    P->blk = jb;
    jsnode *init = NULL;
    int kind = -1;
    P->noin = true;
    if (is_kw(P, KW_VAR)) { kind = VK_VAR; init = parse_var(P, VK_VAR, true); }
    else if (is_kw(P, KW_CONST)) { kind = VK_CONST; init = parse_var(P, VK_CONST, true); }
    else if (is_let_decl(P)) { kind = VK_LET; init = parse_var(P, VK_LET, true); }
    else if (!is_p(P, P_SEMI)) init = parse_expr(P);
    P->noin = false;
    jsnode *res;
    bool is_of = P->t.type == TK_IDENT && P->t.str == A[A_of];
    if (init && (is_of || is_kw(P, KW_IN))) {
        res = mk(P, is_of ? N_FOROF : N_FORIN);
        res->line = line;
        if (is_await) { if (!is_of) { error_at(P, line, "for await exige 'of'"); P->blk = save; return res; } res->flags |= NF_AWAIT; }
        if (kind >= 0) {
            if (init->n != 1 || init->list[0]->b) { error_at(P, line, "Declaração inválida no laço for-%s", is_of ? "of" : "in"); P->blk = save; return res; }
        } else if (init->type == N_ARRAY || init->type == N_OBJECT) init = to_pattern(P, init, false);
        else if (init->type != N_IDENT && init->type != N_MEMBER) { error_at(P, line, "Alvo inválido no laço for"); P->blk = save; return res; }
        res->a = init;
        next(P);
        res->b = is_of ? parse_assign(P) : parse_expr(P);
        expect(P, P_RPAREN);
        res->d = parse_sub(P);
    } else {
        res = mk(P, N_FOR);
        res->line = line;
        res->a = init;
        if (kind < 0 && init && init->type == N_VAR) {}
        expect(P, P_SEMI);
        if (!is_p(P, P_SEMI)) res->b = parse_expr(P);
        expect(P, P_SEMI);
        if (!is_p(P, P_RPAREN)) res->c = parse_expr(P);
        expect(P, P_RPAREN);
        res->d = parse_sub(P);
        if (kind == VK_LET || kind == VK_CONST) res->flags |= NF_PERITER;
    }
    P->blk = save;
    if (jb && jb->n) res->x = jb;
    return res;
}

/* ------------------------------------------------------------ módulos */
static void mod_req(parser *P, jsstr *spec) {
    jsscript *sc = P->sc;
    for (int i = 0; i < sc->nmreq; i++) if (js_str_eq(sc->mreq[i], spec)) return;
    if (sc->nmreq == sc->cmreq) {
        int nc = sc->cmreq ? sc->cmreq * 2 : 8;
        jsstr **na = krealloc(sc->mreq, sizeof(jsstr *) * (usize)nc);
        if (!na) { error_at(P, P->t.line, "Memória insuficiente"); return; }
        sc->mreq = na;
        sc->cmreq = nc;
    }
    sc->mreq[sc->nmreq++] = spec;
}

static void mod_import(parser *P, jsstr *spec, jsstr *imp, jsstr *local) {
    jsscript *sc = P->sc;
    if (sc->nimports == sc->cimports) {
        int nc = sc->cimports ? sc->cimports * 2 : 8;
        jsimport *na = krealloc(sc->imports, sizeof(jsimport) * (usize)nc);
        if (!na) { error_at(P, P->t.line, "Memória insuficiente"); return; }
        sc->imports = na;
        sc->cimports = nc;
    }
    for (int i = 0; i < sc->nimports; i++)
        if (js_str_eq(sc->imports[i].local, local)) { error_at(P, P->t.line, "Identificador '%s' já foi importado", local->d); return; }
    sc->imports[sc->nimports].spec = spec;
    sc->imports[sc->nimports].imp = imp;
    sc->imports[sc->nimports].local = local;
    sc->nimports++;
}

static void mod_export(parser *P, jsstr *exp, jsstr *local, jsstr *spec, jsstr *imp) {
    jsscript *sc = P->sc;
    if (exp) for (int i = 0; i < sc->nexports; i++)
        if (sc->exports[i].exp && js_str_eq(sc->exports[i].exp, exp)) { error_at(P, P->t.line, "Exportação duplicada de '%s'", exp->d); return; }
    if (sc->nexports == sc->cexports) {
        int nc = sc->cexports ? sc->cexports * 2 : 8;
        jsexport *na = krealloc(sc->exports, sizeof(jsexport) * (usize)nc);
        if (!na) { error_at(P, P->t.line, "Memória insuficiente"); return; }
        sc->exports = na;
        sc->cexports = nc;
    }
    jsexport *e = &sc->exports[sc->nexports++];
    e->exp = exp;
    e->local = local;
    e->spec = spec;
    e->imp = imp;
}

static void export_pattern(parser *P, jsnode *n) {
    if (!n) return;
    switch (n->type) {
    case N_IDENT: mod_export(P, n->str, n->str, NULL, NULL); break;
    case N_ARRAY:
        for (int i = 0; i < n->n; i++) {
            jsnode *e = n->list[i];
            if (e->type != N_ELISION) export_pattern(P, e->type == N_SPREAD ? e->a : e);
        }
        break;
    case N_OBJECT: for (int i = 0; i < n->n; i++) export_pattern(P, n->list[i]->b); break;
    case N_ASSIGN: export_pattern(P, n->a); break;
    }
}

static bool is_word(parser *P, const char *w) { return P->t.type == TK_IDENT && !strcmp(P->t.str->d, w); }

/* nome em lista de import/export: identificador, palavra reservada ou string */
static jsstr *module_name(parser *P) {
    if (P->t.type == TK_IDENT || P->t.type == TK_KW || P->t.type == TK_STR) { jsstr *s = P->t.str; next(P); return s; }
    unexpected(P);
    return NULL;
}

static jsstr *module_spec(parser *P) {
    if (!is_word(P, "from")) { unexpected(P); return NULL; }
    next(P);
    if (P->t.type != TK_STR) { unexpected(P); return NULL; }
    jsstr *s = P->t.str;
    next(P);
    return s;
}

static jsnode *parse_import_decl(parser *P) {
    jsnode *n = mk(P, N_EMPTY);
    next(P);
    if (P->t.type == TK_STR) { mod_req(P, P->t.str); next(P); consume_semi(P); return n; }
    jsstr *defl = NULL, *nsname = NULL;
    jsstr *imps[64], *locs[64];
    int ni = 0;
    if (P->t.type == TK_IDENT) {
        defl = P->t.str;
        next(P);
        if (is_p(P, P_COMMA)) next(P);
    }
    if (is_p(P, P_STAR)) {
        next(P);
        if (!is_word(P, "as")) { unexpected(P); return n; }
        next(P);
        if (P->t.type != TK_IDENT) { unexpected(P); return n; }
        nsname = P->t.str;
        next(P);
    } else if (is_p(P, P_LBRACE)) {
        next(P);
        while (!is_p(P, P_RBRACE) && !P->failed) {
            jsstr *imp = module_name(P);
            if (!imp) return n;
            jsstr *loc = imp;
            if (is_word(P, "as")) {
                next(P);
                if (P->t.type != TK_IDENT) { unexpected(P); return n; }
                loc = P->t.str;
                next(P);
            }
            if (ni >= 64) { error_at(P, P->t.line, "Importações demais em uma declaração"); return n; }
            imps[ni] = imp;
            locs[ni++] = loc;
            if (is_p(P, P_COMMA)) next(P);
            else if (!is_p(P, P_RBRACE)) { unexpected(P); return n; }
        }
        next(P);
    }
    jsstr *spec = module_spec(P);
    if (!spec) return n;
    consume_semi(P);
    mod_req(P, spec);
    if (defl) mod_import(P, spec, pstr(P, "default", 7), defl);
    if (nsname) mod_import(P, spec, NULL, nsname);
    for (int i = 0; i < ni; i++) mod_import(P, spec, imps[i], locs[i]);
    return n;
}

static jsnode *parse_export_decl(parser *P) {
    int line = P->t.line;
    next(P);
    jsstr *dflt = pstr(P, "default", 7), *dbind = pstr(P, "*default*", 9);
    if (is_kw(P, KW_DEFAULT)) {
        next(P);
        bool isfn = is_kw(P, KW_FUNCTION);
        if (!isfn && P->t.type == TK_IDENT && P->t.str == A[A_async]) {
            token nx = peek(P);
            isfn = nx.type == TK_KW && nx.op == KW_FUNCTION && !nx.nl;
        }
        if (isfn) {
            P->anon_default = true;
            jsnode *d = parse_statement(P);
            P->anon_default = false;
            if (d->type == N_FUNCDECL) mod_export(P, dflt, d->str, NULL, NULL);
            return d;
        }
        if (is_kw(P, KW_CLASS)) {
            jsnode *c = parse_class(P, false);
            if (c->str) {
                jsnode *d = mk(P, N_CLASSDECL);
                d->line = line;
                d->a = c;
                d->str = c->str;
                add_lex(P, c->str, false);
                mod_export(P, dflt, c->str, NULL, NULL);
                return d;
            }
            jsnode *e = mk(P, N_EXPORTDEF);
            e->line = line;
            e->a = c;
            add_lex(P, dbind, false);
            mod_export(P, dflt, dbind, NULL, NULL);
            return e;
        }
        jsnode *e = mk(P, N_EXPORTDEF);
        e->line = line;
        e->a = parse_assign(P);
        consume_semi(P);
        add_lex(P, dbind, false);
        mod_export(P, dflt, dbind, NULL, NULL);
        return e;
    }
    if (is_p(P, P_STAR)) {
        next(P);
        jsstr *as = NULL;
        if (is_word(P, "as")) { next(P); as = module_name(P); if (!as) return mk(P, N_EMPTY); }
        jsstr *spec = module_spec(P);
        if (!spec) return mk(P, N_EMPTY);
        consume_semi(P);
        mod_req(P, spec);
        mod_export(P, as, NULL, spec, as ? pstr(P, "*", 1) : NULL);
        return mk(P, N_EMPTY);
    }
    if (is_p(P, P_LBRACE)) {
        next(P);
        jsstr *locs[64], *exps[64];
        int ne = 0;
        while (!is_p(P, P_RBRACE) && !P->failed) {
            jsstr *loc = module_name(P);
            if (!loc) return mk(P, N_EMPTY);
            jsstr *exp = loc;
            if (is_word(P, "as")) { next(P); exp = module_name(P); if (!exp) return mk(P, N_EMPTY); }
            if (ne >= 64) { error_at(P, P->t.line, "Exportações demais em uma declaração"); return mk(P, N_EMPTY); }
            locs[ne] = loc;
            exps[ne++] = exp;
            if (is_p(P, P_COMMA)) next(P);
            else if (!is_p(P, P_RBRACE)) { unexpected(P); return mk(P, N_EMPTY); }
        }
        next(P);
        jsstr *spec = NULL;
        if (is_word(P, "from")) { spec = module_spec(P); if (!spec) return mk(P, N_EMPTY); mod_req(P, spec); }
        consume_semi(P);
        for (int i = 0; i < ne; i++) {
            if (spec) mod_export(P, exps[i], NULL, spec, locs[i]);
            else mod_export(P, exps[i], locs[i], NULL, NULL);
        }
        return mk(P, N_EMPTY);
    }
    jsnode *d = parse_statement(P);
    if (d->type == N_VAR) { for (int i = 0; i < d->n; i++) export_pattern(P, d->list[i]->a); }
    else if ((d->type == N_FUNCDECL || d->type == N_CLASSDECL) && d->str) mod_export(P, d->str, d->str, NULL, NULL);
    else error_at(P, line, "Declaração inválida após export");
    return d;
}

static jsnode *parse_statement(parser *P) {
    jsnode *n;
    if (!stack_ok(P)) return mk(P, N_EMPTY);
    int line = P->t.line;
    if (P->t.type == TK_PUNCT) {
        switch (P->t.op) {
        case P_LBRACE: return parse_block(P);
        case P_SEMI: n = mk(P, N_EMPTY); next(P); return n;
        }
    }
    if (P->t.type == TK_KW) {
        switch (P->t.op) {
        case KW_VAR: n = parse_var(P, VK_VAR, false); consume_semi(P); return n;
        case KW_CONST: n = parse_var(P, VK_CONST, false); consume_semi(P); return n;
        case KW_FUNCTION: {
        func_decl:
            next(P);
            if (is_p(P, P_STAR)) {
                next(P);
                P->next_gen = true;
            }
            jsstr *name;
            bool anon = false;
            if (P->t.type != TK_IDENT) {
                if (!P->anon_default) { unexpected(P); return mk(P, N_EMPTY); }
                name = pstr(P, "*default*", 9);
                anon = true;
            } else {
                name = P->t.str;
                next(P);
            }
            jsnode *f = parse_function(P, name, FK_NORMAL, line);
            if (anon && f->x) ((jsfunc *)f->x)->name = pstr(P, "default", 7);
            n = mk(P, N_FUNCDECL);
            n->line = line;
            n->a = f;
            n->str = name;
            if (P->blk == P->fnblk) { add_fn_func(P, n); add_var(P, name); }
            else { add_block_func(P, n); add_lex(P, name, false); }
            return n;
        }
        case KW_CLASS: {
            jsnode *c = parse_class(P, true);
            n = mk(P, N_CLASSDECL);
            n->line = line;
            n->a = c;
            n->str = c->str;
            if (c->str) add_lex(P, c->str, false);
            return n;
        }
        case KW_IF:
            n = mk(P, N_IF);
            next(P);
            expect(P, P_LPAREN);
            n->a = parse_expr(P);
            expect(P, P_RPAREN);
            n->b = parse_sub(P);
            if (is_kw(P, KW_ELSE)) { next(P); n->c = parse_sub(P); }
            return n;
        case KW_FOR: return parse_for(P);
        case KW_WHILE:
            n = mk(P, N_WHILE);
            next(P);
            expect(P, P_LPAREN);
            n->a = parse_expr(P);
            expect(P, P_RPAREN);
            n->d = parse_sub(P);
            return n;
        case KW_DO:
            n = mk(P, N_DOWHILE);
            next(P);
            n->d = parse_sub(P);
            if (!is_kw(P, KW_WHILE)) { unexpected(P); return n; }
            next(P);
            expect(P, P_LPAREN);
            n->a = parse_expr(P);
            expect(P, P_RPAREN);
            if (is_p(P, P_SEMI)) next(P);
            return n;
        case KW_RETURN:
            if (P->nf <= 1) { error_at(P, line, "'return' fora de função"); return mk(P, N_EMPTY); }
            n = mk(P, N_RETURN);
            next(P);
            if (!is_p(P, P_SEMI) && !is_p(P, P_RBRACE) && P->t.type != TK_EOF && !P->t.nl) n->a = parse_expr(P);
            consume_semi(P);
            return n;
        case KW_BREAK: case KW_CONTINUE:
            n = mk(P, P->t.op == KW_BREAK ? N_BREAK : N_CONTINUE);
            next(P);
            if (P->t.type == TK_IDENT && !P->t.nl) { n->str = P->t.str; next(P); }
            consume_semi(P);
            return n;
        case KW_THROW:
            n = mk(P, N_THROW);
            next(P);
            if (P->t.nl) { error_at(P, line, "Quebra de linha após 'throw'"); return n; }
            n->a = parse_expr(P);
            consume_semi(P);
            return n;
        case KW_TRY: {
            n = mk(P, N_TRY);
            next(P);
            if (!is_p(P, P_LBRACE)) { unexpected(P); return n; }
            n->a = parse_block(P);
            if (is_kw(P, KW_CATCH)) {
                next(P);
                jsblock *jb = palloc(P, sizeof(jsblock));
                jsblock *save = P->blk;
                P->blk = jb;
                if (is_p(P, P_LPAREN)) {
                    next(P);
                    if (P->t.type == TK_IDENT) { n->b = mk(P, N_IDENT); n->b->str = P->t.str; next(P); }
                    else if (is_p(P, P_LBRACK)) n->b = to_pattern(P, parse_array_lit(P), true);
                    else if (is_p(P, P_LBRACE)) n->b = to_pattern(P, parse_object_lit(P), true);
                    else unexpected(P);
                    expect(P, P_RPAREN);
                }
                P->blk = save;
                if (!is_p(P, P_LBRACE)) { unexpected(P); return n; }
                n->c = parse_block(P);
            }
            if (is_kw(P, KW_FINALLY)) {
                next(P);
                if (!is_p(P, P_LBRACE)) { unexpected(P); return n; }
                n->d = parse_block(P);
            }
            if (!n->c && !n->d) error_at(P, line, "'try' sem 'catch' ou 'finally'");
            return n;
        }
        case KW_SWITCH: {
            n = mk(P, N_SWITCH);
            next(P);
            expect(P, P_LPAREN);
            n->a = parse_expr(P);
            expect(P, P_RPAREN);
            if (!expect(P, P_LBRACE)) return n;
            jsblock *jb = palloc(P, sizeof(jsblock));
            jsblock *save = P->blk;
            P->blk = jb;
            nlist cases = {0};
            bool has_default = false;
            while (!is_p(P, P_RBRACE) && !P->failed) {
                jsnode *c = mk(P, N_CASE);
                if (is_kw(P, KW_CASE)) { next(P); c->a = parse_expr(P); }
                else if (is_kw(P, KW_DEFAULT)) {
                    if (has_default) { error_at(P, P->t.line, "Mais de um 'default' no switch"); break; }
                    has_default = true;
                    next(P);
                } else { unexpected(P); break; }
                expect(P, P_COLON);
                nlist body = {0};
                while (!is_kw(P, KW_CASE) && !is_kw(P, KW_DEFAULT) && !is_p(P, P_RBRACE) && P->t.type != TK_EOF && !P->failed)
                    nl_push(P, &body, parse_statement(P));
                nl_finish(P, &body, c);
                nl_push(P, &cases, c);
            }
            next(P);
            P->blk = save;
            nl_finish(P, &cases, n);
            if (jb && (jb->n || jb->nfuncs)) n->x = jb;
            return n;
        }
        case KW_DEBUGGER: n = mk(P, N_EMPTY); next(P); consume_semi(P); return n;
        case KW_WITH: error_at(P, line, "'with' não é suportado"); return mk(P, N_EMPTY);
        case KW_IMPORT: {
            token nx = peek(P);
            if (nx.type == TK_PUNCT && (nx.op == P_LPAREN || nx.op == P_DOT)) break;   /* import() / import.meta */
            if (!P->module || P->nf != 1 || P->blk != P->fnblk) {
                error_at(P, line, "import só é permitido no nível superior de módulos (use <script type=\"module\">)");
                return mk(P, N_EMPTY);
            }
            return parse_import_decl(P);
        }
        case KW_EXPORT:
            if (!P->module || P->nf != 1 || P->blk != P->fnblk) {
                error_at(P, line, "export só é permitido no nível superior de módulos");
                return mk(P, N_EMPTY);
            }
            return parse_export_decl(P);
        }
    }
    if (P->t.type == TK_IDENT && P->t.str == A[A_async]) {
        token nx = peek(P);
        if (nx.type == TK_KW && nx.op == KW_FUNCTION && !nx.nl) { next(P); P->next_async = true; goto func_decl; }
    }
    if (is_let_decl(P)) { n = parse_var(P, VK_LET, false); consume_semi(P); return n; }
    if (P->t.type == TK_IDENT) {
        token nx = peek(P);
        if (nx.type == TK_PUNCT && nx.op == P_COLON) {
            n = mk(P, N_LABELED);
            n->str = P->t.str;
            next(P);
            next(P);
            n->a = parse_statement(P);
            return n;
        }
    }
    n = mk(P, N_EXPR);
    n->a = parse_expr(P);
    consume_semi(P);
    return n;
}

jsscript *js_parse(const char *src, int len, const char *name, int line0, jsstr **err) {
    return js_parse_ex(src, len, name, line0, false, err);
}

jsscript *js_parse_ex(const char *src, int len, const char *name, int line0, bool module, jsstr **err) {
    jsscript *sc = gc_alloc(GT_SCRIPT, sizeof(jsscript));
    sc->is_module = module;
    parser *P = kcalloc(1, sizeof(parser));
    if (!P) { if (err) *err = js_strz("memória insuficiente"); return NULL; }
    __asm__ volatile("movq %%rsp, %0" : "=r"(P->stack_top));
    P->s = src;
    P->len = len;
    P->line = line0 > 0 ? line0 : 1;
    P->sc = sc;
    P->module = module;
    sc->name = js_strz(name ? name : "script");
    jsfunc *top = palloc(P, sizeof(jsfunc));
    jsblock *tb = palloc(P, sizeof(jsblock));
    if (!top || !tb) { kfree(P); if (err) *err = js_strz("memória insuficiente"); return NULL; }
    sc->top = top;
    P->fn = top;
    P->fstack[P->nf++] = top;
    P->blk = tb;
    P->fnblk = tb;
    jsnode *prog = mk(P, N_PROGRAM);
    prog->x = tb;
    lex(P);
    nlist l = {0};
    while (P->t.type != TK_EOF && !P->failed) nl_push(P, &l, parse_statement(P));
    nl_finish(P, &l, prog);
    sc->body = prog;
    top->body = prog;
    jsscript *r = sc;
    if (P->failed) {
        if (err) *err = js_str_fmt("%s (linha %d)", P->err, P->errline);
        r = NULL;
    }
    kfree(P);
    return r;
}
