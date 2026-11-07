/* mOS — leitor mínimo de plist XML para os programas nativos (launchd lê os .plist de LaunchDaemons/LaunchAgents,
 * configd lê as preferências de rede). Árvore de nós numa arena (px_alloc); sem plist binário (os plists do
 * launchd são XML; o libSystem lê os binários dos apps pelo CoreFoundation). */
#pragma once

enum { PN_NONE, PN_DICT, PN_ARRAY, PN_STRING, PN_INT, PN_REAL, PN_TRUE, PN_FALSE, PN_DATA, PN_DATE };
typedef struct pnode {
    int type;
    char *key;                 /* filho de dict: a chave */
    char *str;                 /* string/data/date/real: o texto */
    i64 num;
    struct pnode *child, *next;
} pnode_t;

/* memória: arena de blocos de 256 KiB (o malloc do mos.h mapeia 64 KiB por bloco, caro para muitos nós). O nó raiz
 * guarda a arena; plist_free devolve tudo de uma vez. */
typedef struct parena { struct parena *next; usize used, cap; } parena_t;
static parena_t *px_arena;
static void *px_alloc(usize n) {
    n = (n + 15) & ~(usize)15;
    if (!px_arena || px_arena->used + n > px_arena->cap) {
        usize cap = n + sizeof(parena_t) > 262144 ? n + sizeof(parena_t) + 4096 : 262144;
        parena_t *a = (parena_t *)mmap(cap);
        if (!a) return 0;
        a->next = px_arena; a->used = (sizeof(parena_t) + 15) & ~(usize)15; a->cap = cap;
        px_arena = a;
    }
    void *p = (char *)px_arena + px_arena->used;
    px_arena->used += n;
    return p;
}
typedef struct { const char *p, *end; int err; } pxml_t;

static void px_skipws(pxml_t *x) { while (x->p < x->end && (*x->p == ' ' || *x->p == '\t' || *x->p == '\n' || *x->p == '\r')) x->p++; }
static int px_starts(pxml_t *x, const char *s) { usize n = strlen(s); return (usize)(x->end - x->p) >= n && !strncmp(x->p, s, n); }
/* pula <?xml ?>, <!DOCTYPE >, comentários e espaços */
static void px_skipmisc(pxml_t *x) {
    for (;;) {
        px_skipws(x);
        if (px_starts(x, "<?")) { while (x->p < x->end && !px_starts(x, "?>")) x->p++; x->p += 2; continue; }
        if (px_starts(x, "<!--")) { while (x->p < x->end && !px_starts(x, "-->")) x->p++; x->p += 3; continue; }
        if (px_starts(x, "<!")) { while (x->p < x->end && *x->p != '>') x->p++; x->p++; continue; }
        break;
    }
    if (x->p > x->end) x->p = x->end;
}
static char *px_dup(const char *s, usize n) {
    char *d = px_alloc(n + 1);
    if (!d) return 0;
    usize o = 0;
    for (usize i = 0; i < n; i++) {
        if (s[i] == '&') {
            const char *e = s + i;
            usize rest = n - i;
            if (rest >= 5 && !strncmp(e, "&amp;", 5)) { d[o++] = '&'; i += 4; continue; }
            if (rest >= 4 && !strncmp(e, "&lt;", 4)) { d[o++] = '<'; i += 3; continue; }
            if (rest >= 4 && !strncmp(e, "&gt;", 4)) { d[o++] = '>'; i += 3; continue; }
            if (rest >= 6 && !strncmp(e, "&quot;", 6)) { d[o++] = '"'; i += 5; continue; }
            if (rest >= 6 && !strncmp(e, "&apos;", 6)) { d[o++] = '\''; i += 5; continue; }
        }
        d[o++] = s[i];
    }
    d[o] = 0;
    return d;
}
/* lê <tag> ... </tag>: devolve o nome da tag em tag[] e se é vazia (<tag/>) */
static int px_open(pxml_t *x, char *tag, usize cap, int *empty) {
    px_skipmisc(x);
    if (x->p >= x->end || *x->p != '<') return 0;
    x->p++;
    usize n = 0;
    while (x->p < x->end && *x->p != '>' && *x->p != '/' && *x->p != ' ') { if (n + 1 < cap) tag[n++] = *x->p; x->p++; }
    tag[n] = 0;
    while (x->p < x->end && *x->p != '>' && *x->p != '/') x->p++;      /* atributos (plist version="1.0") */
    *empty = 0;
    if (x->p < x->end && *x->p == '/') { *empty = 1; x->p++; }
    if (x->p < x->end && *x->p == '>') x->p++;
    return 1;
}
/* texto até </tag> */
static char *px_text(pxml_t *x, const char *tag) {
    const char *s = x->p;
    while (x->p < x->end && !(x->p[0] == '<' && x->p + 1 < x->end && x->p[1] == '/')) x->p++;
    char *t = px_dup(s, (usize)(x->p - s));
    if (x->p < x->end) { while (x->p < x->end && *x->p != '>') x->p++; x->p++; }
    (void)tag;
    return t;
}
static void px_close(pxml_t *x) {      /* consome </...> */
    px_skipmisc(x);
    if (px_starts(x, "</")) { while (x->p < x->end && *x->p != '>') x->p++; if (x->p < x->end) x->p++; }
}
static pnode_t *px_value(pxml_t *x, int depth);
static pnode_t *px_new(int type) { pnode_t *n = px_alloc(sizeof *n); if (n) memset(n, 0, sizeof *n), n->type = type; return n; }
static pnode_t *px_value(pxml_t *x, int depth) {
    char tag[32];
    int empty;
    if (depth > 32 || !px_open(x, tag, sizeof tag, &empty)) { x->err = 1; return 0; }
    if (!strcmp(tag, "plist")) {
        pnode_t *v = empty ? 0 : px_value(x, depth + 1);
        if (!empty) px_close(x);
        return v;
    }
    if (!strcmp(tag, "true")) return px_new(PN_TRUE);
    if (!strcmp(tag, "false")) return px_new(PN_FALSE);
    if (!strcmp(tag, "dict") || !strcmp(tag, "array")) {
        int isdict = tag[0] == 'd';
        pnode_t *n = px_new(isdict ? PN_DICT : PN_ARRAY), **tail = n ? &n->child : 0;
        if (!n || empty) return n;
        for (;;) {
            px_skipmisc(x);
            if (x->p >= x->end) { x->err = 1; break; }
            if (px_starts(x, "</")) { px_close(x); break; }
            char *key = 0;
            if (isdict) {
                char kt[16];
                int ke;
                if (!px_open(x, kt, sizeof kt, &ke) || strcmp(kt, "key")) { x->err = 1; break; }
                key = ke ? px_dup("", 0) : px_text(x, "key");
            }
            pnode_t *v = px_value(x, depth + 1);
            if (!v) { x->err = 1; break; }
            v->key = key;
            *tail = v;
            tail = &v->next;
        }
        return n;
    }
    int t = !strcmp(tag, "string") ? PN_STRING : !strcmp(tag, "integer") ? PN_INT : !strcmp(tag, "real") ? PN_REAL
          : !strcmp(tag, "data") ? PN_DATA : !strcmp(tag, "date") ? PN_DATE : PN_NONE;
    if (t == PN_NONE) { x->err = 1; return 0; }
    pnode_t *n = px_new(t);
    if (!n) return 0;
    n->str = empty ? px_dup("", 0) : px_text(x, tag);
    if (t == PN_INT && n->str) n->num = atoi(n->str);
    return n;
}
static pnode_t *plist_parse(const char *txt, usize len) {
    pxml_t x = { txt, txt + len, 0 };
    pnode_t *n = px_value(&x, 0);
    return x.err && !n ? 0 : n;
}
static pnode_t *plist_get(pnode_t *d, const char *key) {
    if (!d || d->type != PN_DICT) return 0;
    for (pnode_t *c = d->child; c; c = c->next) if (c->key && !strcmp(c->key, key)) return c;
    return 0;
}
static const char *plist_str(pnode_t *d, const char *key) { pnode_t *n = plist_get(d, key); return n && n->type == PN_STRING ? n->str : 0; }
static int plist_bool(pnode_t *d, const char *key, int def) {
    pnode_t *n = plist_get(d, key);
    if (!n) return def;
    if (n->type == PN_TRUE) return 1;
    if (n->type == PN_FALSE) return 0;
    if (n->type == PN_INT) return n->num != 0;
    return 1;                                 /* dict (KeepAlive com condições): conta como ligado */
}
/* solta TODAS as árvores lidas desde o último plist_free (um plist por vez: leia, copie o que precisa, solte) */
static void plist_free(pnode_t *n) {
    (void)n;
    while (px_arena) { parena_t *a = px_arena; px_arena = a->next; munmap(a, a->cap); }
}
