/* mOS — motor JavaScript "Brisa": expressões regulares (compilador para bytecode + máquina com
 * retrocesso explícito e limite de passos). Trabalha sobre UTF-8, casando por code point. */
#include "js_int.h"

enum { R_CHAR, R_ANY, R_CLASS, R_SPLIT, R_JMP, R_SAVE, R_MATCH, R_BOL, R_EOL, R_WORDB, R_NWORDB, R_BACKREF,
       R_LOOK, R_MARK, R_CHECK, R_FAIL };

typedef struct { u8 op, neg, behind, pad; int x, y; } rxins;
typedef struct { u32 *r; int n; bool neg; } rxclass;

struct rxprog {
    int refs;
    int flags;
    rxins *code;
    int ncode, ccode;
    rxclass *cls;
    int ncls, ccls;
    int ngroups;          /* inclui o grupo 0 */
    char **names;
    int nnames;
    int ncounters;
    bool anchored;        /* começa com ^ sem /m */
    int first;            /* primeiro caractere literal obrigatório (-1 = nenhum) */
};

/* ------------------------------------------------------------ árvore do padrão */
enum { X_CHAR, X_ANY, X_CLASS, X_SEQ, X_ALT, X_GROUP, X_NCGROUP, X_REPEAT, X_BOL, X_EOL, X_WORDB, X_NWORDB,
       X_BACKREF, X_LOOK, X_EMPTY };

typedef struct rxnode {
    int type;
    u32 cp;
    int idx;                 /* classe / grupo / backref */
    int min, max;            /* max -1 = infinito */
    bool greedy, neg, behind;
    struct rxnode **kids;
    int nk, ck;
} rxnode;

typedef struct {
    const char *s;
    int len, pos;
    int flags;
    rxprog *p;
    char *err;
    int errcap;
    bool failed;
    int depth;
    rxnode **all;
    int nall, call;
    char *pending_names[64];
    int npending;
} rxparse;

static rxnode *nnew(rxparse *P, int type) {
    rxnode *n = kcalloc(1, sizeof(rxnode));
    if (!n) { P->failed = true; return NULL; }
    n->type = type;
    n->max = -1;
    n->greedy = true;
    if (P->nall == P->call) {
        int nc = P->call ? P->call * 2 : 64;
        rxnode **na = krealloc(P->all, sizeof(rxnode *) * (usize)nc);
        if (!na) { kfree(n); P->failed = true; return NULL; }
        P->all = na;
        P->call = nc;
    }
    P->all[P->nall++] = n;
    return n;
}

static void nadd(rxparse *P, rxnode *p, rxnode *k) {
    if (!p || !k) return;
    if (p->nk == p->ck) {
        int nc = p->ck ? p->ck * 2 : 4;
        rxnode **na = krealloc(p->kids, sizeof(rxnode *) * (usize)nc);
        if (!na) { P->failed = true; return; }
        p->kids = na;
        p->ck = nc;
    }
    p->kids[p->nk++] = k;
}

static void perr(rxparse *P, const char *m) {
    if (!P->failed) snprintf(P->err, (usize)P->errcap, "%s", m);
    P->failed = true;
}

static u32 peekcp(rxparse *P) {
    if (P->pos >= P->len) return 0;
    int i = P->pos;
    return js_utf8_get(P->s, P->len, &i);
}

static u32 getcp(rxparse *P) {
    if (P->pos >= P->len) return 0;
    return js_utf8_get(P->s, P->len, &P->pos);
}

static int hexval(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if ((c | 32) >= 'a' && (c | 32) <= 'f') return (c | 32) - 'a' + 10;
    return -1;
}

static int new_class(rxparse *P) {
    rxprog *p = P->p;
    if (p->ncls == p->ccls) {
        int nc = p->ccls ? p->ccls * 2 : 8;
        rxclass *na = krealloc(p->cls, sizeof(rxclass) * (usize)nc);
        if (!na) { P->failed = true; return 0; }
        p->cls = na;
        p->ccls = nc;
    }
    p->cls[p->ncls].r = NULL;
    p->cls[p->ncls].n = 0;
    p->cls[p->ncls].neg = false;
    return p->ncls++;
}

static void cls_add(rxparse *P, int ci, u32 lo, u32 hi) {
    rxclass *c = &P->p->cls[ci];
    u32 *nr = krealloc(c->r, sizeof(u32) * (usize)(c->n * 2 + 2));
    if (!nr) { P->failed = true; return; }
    c->r = nr;
    c->r[c->n * 2] = lo;
    c->r[c->n * 2 + 1] = hi;
    c->n++;
}

static const u32 sp_ranges[] = {9, 13, 32, 32, 0xA0, 0xA0, 0x1680, 0x1680, 0x2000, 0x200A, 0x2028, 0x2029, 0x202F, 0x202F,
                                0x205F, 0x205F, 0x3000, 0x3000, 0xFEFF, 0xFEFF};

static void cls_add_esc(rxparse *P, int ci, int e) {
    switch (e) {
    case 'd': cls_add(P, ci, '0', '9'); break;
    case 'w': cls_add(P, ci, 'a', 'z'); cls_add(P, ci, 'A', 'Z'); cls_add(P, ci, '0', '9'); cls_add(P, ci, '_', '_'); break;
    case 's': for (u32 i = 0; i < ARRAY_LEN(sp_ranges); i += 2) cls_add(P, ci, sp_ranges[i], sp_ranges[i + 1]); break;
    case 'D': cls_add(P, ci, 0, '0' - 1); cls_add(P, ci, '9' + 1, 0x10FFFF); break;
    case 'W': cls_add(P, ci, 0, '0' - 1); cls_add(P, ci, '9' + 1, 'A' - 1); cls_add(P, ci, 'Z' + 1, '_' - 1);
              cls_add(P, ci, '_' + 1, 'a' - 1); cls_add(P, ci, 'z' + 1, 0x10FFFF); break;
    case 'S': {
        u32 prev = 0;
        for (u32 i = 0; i < ARRAY_LEN(sp_ranges); i += 2) {
            if (sp_ranges[i] > prev) cls_add(P, ci, prev, sp_ranges[i] - 1);
            prev = sp_ranges[i + 1] + 1;
        }
        cls_add(P, ci, prev, 0x10FFFF);
        break;
    }
    }
}

/* escapes de caractere simples (fora e dentro de classes); devolve o code point */
static u32 parse_char_escape(rxparse *P, u32 c, bool in_class) {
    switch (c) {
    case 'n': return '\n';
    case 'r': return '\r';
    case 't': return '\t';
    case 'v': return 0x0B;
    case 'f': return 0x0C;
    case '0': return 0;
    case 'b': return in_class ? 8 : 'b';
    case 'c': {
        u32 l = peekcp(P);
        if ((l | 32) >= 'a' && (l | 32) <= 'z') { P->pos++; return l % 32; }
        return '\\';
    }
    case 'x': {
        if (P->pos + 1 < P->len && hexval(P->s[P->pos]) >= 0 && hexval(P->s[P->pos + 1]) >= 0) {
            u32 v = (u32)(hexval(P->s[P->pos]) * 16 + hexval(P->s[P->pos + 1]));
            P->pos += 2;
            return v;
        }
        return 'x';
    }
    case 'u': {
        if (P->pos < P->len && P->s[P->pos] == '{') {
            int q = P->pos + 1;
            u32 v = 0;
            while (q < P->len && hexval(P->s[q]) >= 0) { v = v * 16 + (u32)hexval(P->s[q]); q++; }
            if (q < P->len && P->s[q] == '}') { P->pos = q + 1; return v; }
            return 'u';
        }
        if (P->pos + 3 < P->len) {
            u32 v = 0;
            for (int i = 0; i < 4; i++) {
                int h = hexval(P->s[P->pos + i]);
                if (h < 0) return 'u';
                v = v * 16 + (u32)h;
            }
            P->pos += 4;
            /* par substituto 😀 */
            if (v >= 0xD800 && v < 0xDC00 && P->pos + 5 < P->len && P->s[P->pos] == '\\' && P->s[P->pos + 1] == 'u') {
                u32 w = 0;
                bool ok = true;
                for (int i = 0; i < 4; i++) { int h = hexval(P->s[P->pos + 2 + i]); if (h < 0) { ok = false; break; } w = w * 16 + (u32)h; }
                if (ok && w >= 0xDC00 && w < 0xE000) { P->pos += 6; v = 0x10000 + ((v - 0xD800) << 10) + (w - 0xDC00); }
            }
            return v;
        }
        return 'u';
    }
    default: return c;
    }
}

static rxnode *parse_alt(rxparse *P);

static rxnode *parse_class(rxparse *P) {
    int ci = new_class(P);
    if (P->failed) return NULL;
    if (P->pos < P->len && P->s[P->pos] == '^') { P->p->cls[ci].neg = true; P->pos++; }
    bool first = true;
    while (P->pos < P->len && (P->s[P->pos] != ']' || (first && false))) {
        first = false;
        u32 c = getcp(P);
        u32 lo;
        bool is_set = false;
        if (c == '\\') {
            u32 e = getcp(P);
            if (e == 'd' || e == 'w' || e == 's' || e == 'D' || e == 'W' || e == 'S') { cls_add_esc(P, ci, (int)e); is_set = true; lo = 0; }
            else lo = parse_char_escape(P, e, true);
        } else lo = c;
        if (is_set) continue;
        if (P->pos + 1 < P->len && P->s[P->pos] == '-' && P->s[P->pos + 1] != ']') {
            P->pos++;
            u32 c2 = getcp(P);
            u32 hi;
            if (c2 == '\\') {
                u32 e = getcp(P);
                if (e == 'd' || e == 'w' || e == 's' || e == 'D' || e == 'W' || e == 'S') {
                    cls_add(P, ci, lo, lo); cls_add(P, ci, '-', '-'); cls_add_esc(P, ci, (int)e);
                    continue;
                }
                hi = parse_char_escape(P, e, true);
            } else hi = c2;
            if (hi < lo) { perr(P, "Intervalo fora de ordem na classe de caracteres"); return NULL; }
            cls_add(P, ci, lo, hi);
        } else cls_add(P, ci, lo, lo);
    }
    if (P->pos >= P->len) { perr(P, "Classe de caracteres não terminada"); return NULL; }
    P->pos++;
    rxnode *n = nnew(P, X_CLASS);
    if (n) n->idx = ci;
    return n;
}

static rxnode *parse_atom(rxparse *P) {
    u32 c = getcp(P);
    rxnode *n;
    switch (c) {
    case '.': return nnew(P, X_ANY);
    case '^': return nnew(P, X_BOL);
    case '$': return nnew(P, X_EOL);
    case '[': return parse_class(P);
    case '(': {
        if (++P->depth > 200) { perr(P, "Expressão regular aninhada demais"); return NULL; }
        int type = X_GROUP;
        bool neg = false, behind = false;
        char *gname = NULL;
        if (P->pos < P->len && P->s[P->pos] == '?') {
            P->pos++;
            u32 k = getcp(P);
            if (k == ':') type = X_NCGROUP;
            else if (k == '=') type = X_LOOK;
            else if (k == '!') { type = X_LOOK; neg = true; }
            else if (k == '<' && P->pos < P->len && (P->s[P->pos] == '=' || P->s[P->pos] == '!')) {
                type = X_LOOK; behind = true; neg = P->s[P->pos] == '!'; P->pos++;
            } else if (k == '<') {
                int st = P->pos;
                while (P->pos < P->len && P->s[P->pos] != '>') P->pos++;
                if (P->pos >= P->len) { perr(P, "Nome de grupo inválido"); return NULL; }
                gname = kmalloc((usize)(P->pos - st) + 1);
                if (gname) { memcpy(gname, P->s + st, (usize)(P->pos - st)); gname[P->pos - st] = 0; }
                P->pos++;
            } else { perr(P, "Grupo inválido"); return NULL; }
        }
        int gi = 0;
        if (type == X_GROUP) {
            gi = P->p->ngroups++;
            if (gname) {
                rxprog *p = P->p;
                char **nn = kcalloc((usize)p->ngroups, sizeof(char *));
                if (nn) {
                    if (p->names) { memcpy(nn, p->names, sizeof(char *) * (usize)p->nnames); kfree(p->names); }
                    p->names = nn;
                    p->nnames = p->ngroups;
                    nn[gi] = gname;
                    gname = NULL;
                }
            }
        }
        kfree(gname);
        rxnode *inner = parse_alt(P);
        if (P->failed) return NULL;
        if (P->pos >= P->len || P->s[P->pos] != ')') { perr(P, "Parêntese não fechado na expressão regular"); return NULL; }
        P->pos++;
        P->depth--;
        n = nnew(P, type);
        if (!n) return NULL;
        n->idx = gi;
        n->neg = neg;
        n->behind = behind;
        nadd(P, n, inner);
        return n;
    }
    case '\\': {
        if (P->pos >= P->len) { perr(P, "\\ no fim do padrão"); return NULL; }
        u32 e = getcp(P);
        if (e == 'b') return nnew(P, X_WORDB);
        if (e == 'B') return nnew(P, X_NWORDB);
        if (e == 'd' || e == 'w' || e == 's' || e == 'D' || e == 'W' || e == 'S') {
            int ci = new_class(P);
            cls_add_esc(P, ci, (int)e);
            n = nnew(P, X_CLASS);
            if (n) n->idx = ci;
            return n;
        }
        if (e >= '1' && e <= '9') {
            int v = (int)(e - '0');
            while (P->pos < P->len && isdigit((u8)P->s[P->pos]) && v < 100) v = v * 10 + (P->s[P->pos++] - '0');
            n = nnew(P, X_BACKREF);
            if (n) n->idx = v;
            return n;
        }
        if (e == 'k' && P->pos < P->len && P->s[P->pos] == '<') {
            int st = ++P->pos;
            while (P->pos < P->len && P->s[P->pos] != '>') P->pos++;
            int ln = P->pos - st;
            if (P->pos < P->len) P->pos++;
            n = nnew(P, X_BACKREF);
            if (!n) return NULL;
            n->idx = -1;
            for (int i = 0; i < P->p->nnames; i++)
                if (P->p->names[i] && (int)strlen(P->p->names[i]) == ln && !strncmp(P->p->names[i], P->s + st, (usize)ln)) n->idx = i;
            if (n->idx < 0) n->type = X_EMPTY;
            return n;
        }
        n = nnew(P, X_CHAR);
        if (n) n->cp = parse_char_escape(P, e, false);
        return n;
    }
    case ')': case '|':
        perr(P, "Erro interno do analisador de expressões");
        return NULL;
    case '*': case '+': case '?':
        perr(P, "Nada para repetir na expressão regular");
        return NULL;
    default:
        n = nnew(P, X_CHAR);
        if (n) n->cp = c;
        return n;
    }
}

static bool parse_braces(rxparse *P, int *mn, int *mx) {
    int q = P->pos + 1;
    if (q >= P->len || !isdigit((u8)P->s[q])) return false;
    int a = 0, b;
    while (q < P->len && isdigit((u8)P->s[q])) { if (a < 100000) a = a * 10 + (P->s[q] - '0'); q++; }
    b = a;
    if (q < P->len && P->s[q] == ',') {
        q++;
        if (q < P->len && isdigit((u8)P->s[q])) {
            b = 0;
            while (q < P->len && isdigit((u8)P->s[q])) { if (b < 100000) b = b * 10 + (P->s[q] - '0'); q++; }
        } else b = -1;
    }
    if (q >= P->len || P->s[q] != '}') return false;
    P->pos = q + 1;
    *mn = a;
    *mx = b;
    return true;
}

static rxnode *parse_seq(rxparse *P) {
    rxnode *seq = nnew(P, X_SEQ);
    while (!P->failed && P->pos < P->len && P->s[P->pos] != '|' && P->s[P->pos] != ')') {
        rxnode *a;
        if (P->s[P->pos] == '{') {
            int mn, mx;
            int save = P->pos;
            if (parse_braces(P, &mn, &mx)) { perr(P, "Nada para repetir na expressão regular"); return NULL; }
            P->pos = save + 1;
            a = nnew(P, X_CHAR);
            if (a) a->cp = '{';
        } else a = parse_atom(P);
        if (P->failed || !a) return NULL;
        for (;;) {
            if (P->pos >= P->len) break;
            char q = P->s[P->pos];
            int mn, mx;
            if (q == '*') { mn = 0; mx = -1; P->pos++; }
            else if (q == '+') { mn = 1; mx = -1; P->pos++; }
            else if (q == '?') { mn = 0; mx = 1; P->pos++; }
            else if (q == '{' && parse_braces(P, &mn, &mx)) {
                if (mx >= 0 && mx < mn) { perr(P, "Quantificador fora de ordem"); return NULL; }
            } else break;
            if (a->type == X_BOL || a->type == X_EOL || a->type == X_WORDB || a->type == X_NWORDB) { perr(P, "Nada para repetir"); return NULL; }
            rxnode *r = nnew(P, X_REPEAT);
            if (!r) return NULL;
            r->min = mn;
            r->max = mx;
            if (P->pos < P->len && P->s[P->pos] == '?') { r->greedy = false; P->pos++; }
            nadd(P, r, a);
            a = r;
            break;
        }
        nadd(P, seq, a);
    }
    return seq;
}

static rxnode *parse_alt(rxparse *P) {
    rxnode *first = parse_seq(P);
    if (P->failed) return NULL;
    if (P->pos >= P->len || P->s[P->pos] != '|') return first;
    rxnode *alt = nnew(P, X_ALT);
    nadd(P, alt, first);
    while (!P->failed && P->pos < P->len && P->s[P->pos] == '|') {
        P->pos++;
        nadd(P, alt, parse_seq(P));
    }
    return alt;
}

/* ------------------------------------------------------------ geração de código */
static int emit(rxparse *P, int op, int x, int y) {
    rxprog *p = P->p;
    if (p->ncode == p->ccode) {
        int nc = p->ccode ? p->ccode * 2 : 64;
        if (nc > 200000) { perr(P, "Expressão regular grande demais"); return 0; }
        rxins *na = krealloc(p->code, sizeof(rxins) * (usize)nc);
        if (!na) { P->failed = true; return 0; }
        p->code = na;
        p->ccode = nc;
    }
    rxins *i = &p->code[p->ncode];
    i->op = (u8)op;
    i->neg = 0;
    i->behind = 0;
    i->x = x;
    i->y = y;
    return p->ncode++;
}

static void gen(rxparse *P, rxnode *n) {
    if (!n || P->failed) return;
    switch (n->type) {
    case X_CHAR: emit(P, R_CHAR, (int)n->cp, 0); break;
    case X_ANY: emit(P, R_ANY, 0, 0); break;
    case X_CLASS: emit(P, R_CLASS, n->idx, 0); break;
    case X_BOL: emit(P, R_BOL, 0, 0); break;
    case X_EOL: emit(P, R_EOL, 0, 0); break;
    case X_WORDB: emit(P, R_WORDB, 0, 0); break;
    case X_NWORDB: emit(P, R_NWORDB, 0, 0); break;
    case X_BACKREF: emit(P, R_BACKREF, n->idx, 0); break;
    case X_EMPTY: break;
    case X_SEQ: for (int i = 0; i < n->nk; i++) gen(P, n->kids[i]); break;
    case X_ALT: {
        int jumps[256], nj = 0;
        for (int i = 0; i < n->nk; i++) {
            if (i < n->nk - 1) {
                int sp = emit(P, R_SPLIT, 0, 0);
                P->p->code[sp].x = sp + 1;
                gen(P, n->kids[i]);
                if (nj < 256) jumps[nj++] = emit(P, R_JMP, 0, 0);
                P->p->code[sp].y = P->p->ncode;
            } else gen(P, n->kids[i]);
        }
        for (int i = 0; i < nj; i++) P->p->code[jumps[i]].x = P->p->ncode;
        break;
    }
    case X_GROUP:
        emit(P, R_SAVE, n->idx * 2, 0);
        gen(P, n->kids[0]);
        emit(P, R_SAVE, n->idx * 2 + 1, 0);
        break;
    case X_NCGROUP: gen(P, n->kids[0]); break;
    case X_LOOK: {
        int l = emit(P, R_LOOK, 0, 0);
        P->p->code[l].neg = n->neg;
        P->p->code[l].behind = n->behind;
        gen(P, n->kids[0]);
        emit(P, R_MATCH, 0, 0);
        P->p->code[l].x = l + 1;
        P->p->code[l].y = P->p->ncode;   /* continua aqui */
        break;
    }
    case X_REPEAT: {
        rxnode *k = n->kids[0];
        int mn = n->min, mx = n->max;
        if (mn > 1000 || mx > 1000) { perr(P, "Quantificador grande demais"); return; }
        for (int i = 0; i < mn; i++) gen(P, k);
        if (mx < 0) {
            int cnt = P->p->ncounters++;
            int L = emit(P, R_SPLIT, 0, 0);
            emit(P, R_MARK, cnt, 0);
            gen(P, k);
            emit(P, R_CHECK, cnt, 0);
            emit(P, R_JMP, L, 0);
            int out = P->p->ncode;
            if (n->greedy) { P->p->code[L].x = L + 1; P->p->code[L].y = out; }
            else { P->p->code[L].x = out; P->p->code[L].y = L + 1; }
        } else {
            int splits[1001], ns = 0;
            for (int i = mn; i < mx; i++) {
                int s = emit(P, R_SPLIT, 0, 0);
                splits[ns++] = s;
                gen(P, k);
            }
            int out = P->p->ncode;
            for (int i = 0; i < ns; i++) {
                int s = splits[i];
                if (n->greedy) { P->p->code[s].x = s + 1; P->p->code[s].y = out; }
                else { P->p->code[s].x = out; P->p->code[s].y = s + 1; }
            }
        }
        break;
    }
    }
}

void rx_ref(rxprog *p) { if (p) p->refs++; }

void rx_unref(rxprog *p) {
    if (!p || --p->refs > 0) return;
    for (int i = 0; i < p->ncls; i++) kfree(p->cls[i].r);
    kfree(p->cls);
    kfree(p->code);
    if (p->names) { for (int i = 0; i < p->nnames; i++) kfree(p->names[i]); kfree(p->names); }
    kfree(p);
}

int rx_ngroups(rxprog *p) { return p->ngroups; }
const char *rx_group_name(rxprog *p, int i) { return p->names && i < p->nnames ? p->names[i] : NULL; }
int rx_flags(rxprog *p) { return p->flags; }

rxprog *rx_compile(const char *src, int len, int flags, char *err, int errcap) {
    rxprog *p = kcalloc(1, sizeof(rxprog));
    if (!p) { snprintf(err, (usize)errcap, "memória insuficiente"); return NULL; }
    p->refs = 1;
    p->flags = flags;
    p->ngroups = 1;
    p->first = -1;
    rxparse P = {0};
    P.s = src;
    P.len = len;
    P.flags = flags;
    P.p = p;
    P.err = err;
    P.errcap = errcap;
    rxnode *root = parse_alt(&P);
    if (!P.failed && P.pos < P.len) perr(&P, "Parêntese ')' sem par na expressão regular");
    if (!P.failed) {
        emit(&P, R_SAVE, 0, 0);
        gen(&P, root);
        emit(&P, R_SAVE, 1, 0);
        emit(&P, R_MATCH, 0, 0);
    }
    if (!P.failed && root) {
        rxnode *f = root;
        while (f && f->type == X_SEQ && f->nk) f = f->kids[0];
        if (f && f->type == X_BOL && !(flags & RXF_M)) p->anchored = true;
        if (f && f->type == X_CHAR && !(flags & RXF_I) && f->cp < 128) p->first = (int)f->cp;
    }
    for (int i = 0; i < P.nall; i++) { kfree(P.all[i]->kids); kfree(P.all[i]); }
    kfree(P.all);
    if (P.failed) { rx_unref(p); return NULL; }
    return p;
}

/* ------------------------------------------------------------ execução */
static u32 fold(u32 c) {
    if (c >= 'A' && c <= 'Z') return c + 32;
    if (c >= 0xC0 && c <= 0xDE && c != 0xD7) return c + 32;
    if (c >= 0x391 && c <= 0x3A9) return c + 32;
    if (c >= 0x410 && c <= 0x42F) return c + 32;
    return c;
}

static u32 unfold(u32 c) {
    if (c >= 'a' && c <= 'z') return c - 32;
    if (c >= 0xE0 && c <= 0xFE && c != 0xF7) return c - 32;
    if (c >= 0x3B1 && c <= 0x3C9) return c - 32;
    if (c >= 0x430 && c <= 0x44F) return c - 32;
    return c;
}

static bool in_class(rxclass *c, u32 cp, bool icase) {
    bool hit = false;
    for (int i = 0; i < c->n && !hit; i++) {
        u32 lo = c->r[i * 2], hi = c->r[i * 2 + 1];
        if (cp >= lo && cp <= hi) hit = true;
        else if (icase) {
            u32 a = fold(cp), b = unfold(cp);
            if ((a >= lo && a <= hi) || (b >= lo && b <= hi)) hit = true;
        }
    }
    return c->neg ? !hit : hit;
}

static bool is_word(const char *s, int len, int i) {
    if (i < 0 || i >= len) return false;
    char c = s[i];
    return isalnum((u8)c) || c == '_';
}

static bool is_lt(u32 c) { return c == '\n' || c == '\r' || c == 0x2028 || c == 0x2029; }

typedef struct { int kind, a, b; } bt_t;   /* kind 0: ramo (pc, sp); 1: restaura captura; 2: restaura contador */

typedef struct {
    rxprog *p;
    const char *s;
    int len;
    long steps;
    bt_t *st;
    int nst, cst;
    bool overflow;
} rxvm;

static bool push(rxvm *V, int kind, int a, int b) {
    if (V->nst == V->cst) {
        int nc = V->cst ? V->cst * 2 : 256;
        if (nc > (4 << 20)) { V->overflow = true; return false; }
        bt_t *na = krealloc(V->st, sizeof(bt_t) * (usize)nc);
        if (!na) { V->overflow = true; return false; }
        V->st = na;
        V->cst = nc;
    }
    V->st[V->nst].kind = kind;
    V->st[V->nst].a = a;
    V->st[V->nst].b = b;
    V->nst++;
    return true;
}

static int prev_cp_start(const char *s, int i) {
    if (i <= 0) return -1;
    i--;
    while (i > 0 && ((u8)s[i] & 0xC0) == 0x80) i--;
    return i;
}

#define RX_STEP_LIMIT 20000000L

/* executa a partir de pc/sp; target >= 0 exige terminar exatamente em target (lookbehind) */
static int run(rxvm *V, int pc, int sp, int *caps, int *cnt, int target, int depth) {
    rxprog *p = V->p;
    const char *s = V->s;
    int len = V->len;
    bool icase = (p->flags & RXF_I) != 0;
    int base = V->nst;
    if (depth > 50) return -1;
    for (;;) {
        if (++V->steps > RX_STEP_LIMIT || V->overflow) return -1;
        rxins *in = &p->code[pc];
        switch (in->op) {
        case R_CHAR: {
            if (sp >= len) goto fail;
            int q = sp;
            u32 c = (u8)s[sp] < 0x80 ? (u32)(u8)s[q++] : js_utf8_get(s, len, &q);
            u32 w = (u32)in->x;
            if (c != w && !(icase && fold(c) == fold(w))) goto fail;
            sp = q;
            pc++;
            break;
        }
        case R_ANY: {
            if (sp >= len) goto fail;
            int q = sp;
            u32 c = js_utf8_get(s, len, &q);
            if (!(p->flags & RXF_S) && is_lt(c)) goto fail;
            sp = q;
            pc++;
            break;
        }
        case R_CLASS: {
            if (sp >= len) goto fail;
            int q = sp;
            u32 c = js_utf8_get(s, len, &q);
            if (!in_class(&p->cls[in->x], c, icase)) goto fail;
            sp = q;
            pc++;
            break;
        }
        case R_SPLIT:
            if (!push(V, 0, in->y, sp)) return -1;
            pc = in->x;
            break;
        case R_JMP: pc = in->x; break;
        case R_SAVE:
            if (!push(V, 1, in->x, caps[in->x])) return -1;
            caps[in->x] = sp;
            pc++;
            break;
        case R_MARK:
            if (!push(V, 2, in->x, cnt[in->x])) return -1;
            cnt[in->x] = sp;
            pc++;
            break;
        case R_CHECK:
            if (cnt[in->x] == sp) goto fail;
            pc++;
            break;
        case R_MATCH:
            if (target >= 0 && sp != target) goto fail;
            V->nst = base;
            return 1;
        case R_BOL:
            if (sp == 0 || ((p->flags & RXF_M) && is_lt((u8)s[sp - 1]))) { pc++; break; }
            goto fail;
        case R_EOL:
            if (sp == len || ((p->flags & RXF_M) && is_lt((u8)s[sp]))) { pc++; break; }
            goto fail;
        case R_WORDB: case R_NWORDB: {
            bool a = is_word(s, len, sp - 1), b = is_word(s, len, sp);
            if ((a != b) == (in->op == R_WORDB)) { pc++; break; }
            goto fail;
        }
        case R_BACKREF: {
            int g = in->x;
            if (g >= p->ngroups) { pc++; break; }
            int a = caps[g * 2], b = caps[g * 2 + 1];
            if (a < 0 || b < 0) { pc++; break; }
            int n = b - a;
            if (sp + n > len) goto fail;
            bool ok = true;
            for (int i = 0; i < n; i++) {
                u8 x = (u8)s[a + i], y = (u8)s[sp + i];
                if (x != y && !(icase && fold(x) == fold(y))) { ok = false; break; }
            }
            if (!ok) goto fail;
            sp += n;
            pc++;
            break;
        }
        case R_LOOK: {
            int ng = p->ngroups * 2;
            int saved[64];
            int nsave = MIN(ng, 64);
            memcpy(saved, caps, sizeof(int) * (usize)nsave);
            int r = 0;
            if (!in->behind) {
                r = run(V, in->x, sp, caps, cnt, -1, depth + 1);
            } else {
                for (int k = sp; k >= 0 && r == 0; k = prev_cp_start(s, k)) {
                    r = run(V, in->x, k, caps, cnt, sp, depth + 1);
                    if (k == 0) break;
                }
            }
            if (r < 0) return -1;
            bool ok = in->neg ? r == 0 : r == 1;
            if (in->neg || !ok) memcpy(caps, saved, sizeof(int) * (usize)nsave);
            if (!ok) goto fail;
            pc = in->y;
            break;
        }
        case R_FAIL:
        default:
            goto fail;
        }
        continue;
    fail:
        for (;;) {
            if (V->nst <= base) return 0;
            bt_t e = V->st[--V->nst];
            if (e.kind == 1) caps[e.a] = e.b;
            else if (e.kind == 2) cnt[e.a] = e.b;
            else { pc = e.a; sp = e.b; break; }
        }
    }
}

int rx_exec(rxprog *p, const char *s, int len, int start, int *caps, bool sticky) {
    rxvm V = {0};
    V.p = p;
    V.s = s;
    V.len = len;
    int ng = p->ngroups * 2;
    int cnt_buf[64];
    int *cnt = p->ncounters <= 64 ? cnt_buf : kmalloc(sizeof(int) * (usize)p->ncounters);
    if (!cnt) return -1;
    int result = 0;
    for (int pos = start; pos <= len; ) {
        if (p->first >= 0 && !sticky) {
            while (pos < len && (u8)s[pos] != (u8)p->first) pos++;
            if (pos >= len) break;
        }
        for (int i = 0; i < ng; i++) caps[i] = -1;
        for (int i = 0; i < p->ncounters; i++) cnt[i] = -1;
        V.nst = 0;
        int r = run(&V, 0, pos, caps, cnt, -1, 0);
        if (r != 0) { result = r; break; }
        if (sticky || p->anchored) break;
        if (pos >= len) break;
        pos++;
        while (pos < len && ((u8)s[pos] & 0xC0) == 0x80) pos++;
    }
    if (result <= 0) for (int i = 0; i < ng; i++) caps[i] = -1;
    kfree(V.st);
    if (cnt != cnt_buf) kfree(cnt);
    return result;
}
