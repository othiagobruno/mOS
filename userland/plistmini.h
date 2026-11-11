/* mOS — leitor mínimo de plist XML e binário (bplist00) para os programas nativos (launchd lê os .plist de
 * LaunchDaemons/LaunchAgents, configd lê as preferências de rede). Árvore de nós numa arena (px_alloc). O libSystem
 * lê os plists dos apps pelo CoreFoundation. */
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
/* ---- plist binário (bplist00), como o launchd da Apple aceita em LaunchAgents/LaunchDaemons.
 * Trailer de 32 bytes: 6 livres, tamanho do offset, tamanho da referência, nº de objetos, objeto do topo e
 * início da tabela de offsets (inteiros big-endian). Marcador: tipo no nibble alto, tamanho no baixo (0xF =
 * tamanho num inteiro a seguir). Strings UTF-16 viram UTF-8. Real e data guardam a parte inteira em num.
 * Tudo é conferido contra o tamanho do arquivo; ciclo ou profundidade > 32 é erro. */
typedef struct { const unsigned char *b; usize len, offs; unsigned osz, rsz; u64 nobj; int err; unsigned visits; } pbin_t;
static u64 pb_be(const unsigned char *p, unsigned n) { u64 v = 0; while (n--) v = (v << 8) | *p++; return v; }
static int pb_off(pbin_t *x, u64 ref, usize *at) {
    if (ref >= x->nobj) return 0;
    usize o = x->offs + (usize)ref * x->osz;
    if (o + x->osz > x->len - 32) return 0;
    u64 v = pb_be(x->b + o, x->osz);
    if (v < 8 || v >= x->offs) return 0;
    *at = (usize)v;
    return 1;
}
/* tamanho do objeto em *at (após o marcador); *at avança para o conteúdo */
static int pb_count(pbin_t *x, usize *at, unsigned lo, u64 *n) {
    if (lo != 0xF) { *n = lo; return 1; }
    if (*at >= x->offs) return 0;
    unsigned m = x->b[*at];
    if ((m >> 4) != 1 || (m & 0xF) > 3) return 0;
    unsigned w = 1u << (m & 0xF);
    if (*at + 1 + w > x->offs) return 0;
    *n = pb_be(x->b + *at + 1, w);
    *at += 1 + w;
    return 1;
}
static pnode_t *pb_obj(pbin_t *x, u64 ref, int depth);
static char *pb_utf16(const unsigned char *p, u64 n) {
    char *d = px_alloc((usize)n * 3 + 1);
    if (!d) return 0;
    usize o = 0;
    for (u64 i = 0; i < n; i++) {
        unsigned c = (unsigned)pb_be(p + i * 2, 2);
        if (c >= 0xD800 && c < 0xDC00 && i + 1 < n) {
            unsigned c2 = (unsigned)pb_be(p + (i + 1) * 2, 2);
            if (c2 >= 0xDC00 && c2 < 0xE000) { c = 0x10000 + ((c - 0xD800) << 10) + (c2 - 0xDC00); i++; }
        }
        if (c < 0x80) d[o++] = (char)c;
        else if (c < 0x800) { d[o++] = (char)(0xC0 | (c >> 6)); d[o++] = (char)(0x80 | (c & 0x3F)); }
        else if (c < 0x10000) { d[o++] = (char)(0xE0 | (c >> 12)); d[o++] = (char)(0x80 | ((c >> 6) & 0x3F)); d[o++] = (char)(0x80 | (c & 0x3F)); }
        else {
            if (o + 4 > (usize)n * 3) break;
            d[o++] = (char)(0xF0 | (c >> 18)); d[o++] = (char)(0x80 | ((c >> 12) & 0x3F));
            d[o++] = (char)(0x80 | ((c >> 6) & 0x3F)); d[o++] = (char)(0x80 | (c & 0x3F));
        }
    }
    d[o] = 0;
    return d;
}
static pnode_t *pb_obj(pbin_t *x, u64 ref, int depth) {
    usize at;
    /* referências repetidas formam um DAG: o teto de visitas impede a explosão de um arquivo hostil */
    if (depth > 32 || ++x->visits > 65536 || !pb_off(x, ref, &at)) { x->err = 1; return 0; }
    unsigned m = x->b[at++], hi = m >> 4, lo = m & 0xF;
    pnode_t *n = 0;
    u64 cnt;
    switch (hi) {
    case 0x0:
        if (m == 0x08) return px_new(PN_FALSE);
        if (m == 0x09) return px_new(PN_TRUE);
        x->err = 1; return 0;
    case 0x1: {                                         /* inteiro de 1, 2, 4, 8 (ou 16) bytes */
        unsigned w = 1u << lo;
        if (lo > 4 || at + w > x->offs) { x->err = 1; return 0; }
        if (!(n = px_new(PN_INT))) return 0;
        u64 v = pb_be(x->b + at + (w > 8 ? w - 8 : 0), w > 8 ? 8 : w);
        n->num = (i64)v;                                /* 1, 2 e 4 bytes sem sinal; 8 com sinal */
        return n;
    }
    case 0x2: case 0x3: {                               /* real e data: a parte inteira do double */
        unsigned w = hi == 3 ? 8 : 1u << lo;
        if ((w != 4 && w != 8) || at + w > x->offs) { x->err = 1; return 0; }
        if (!(n = px_new(hi == 3 ? PN_DATE : PN_REAL))) return 0;
        if (w == 8) { union { u64 u; double d; } c; c.u = pb_be(x->b + at, 8); n->num = (i64)c.d; }
        else { union { unsigned u; float f; } c; c.u = (unsigned)pb_be(x->b + at, 4); n->num = (i64)c.f; }
        return n;
    }
    case 0x4: case 0x5: case 0x6: {
        if (!pb_count(x, &at, lo, &cnt)) { x->err = 1; return 0; }
        u64 bytes = hi == 6 ? cnt * 2 : cnt;
        if (cnt > x->len || at + bytes > x->offs) { x->err = 1; return 0; }
        if (!(n = px_new(hi == 4 ? PN_DATA : PN_STRING))) return 0;
        if (hi == 4) { n->num = (i64)cnt; return n; }
        if (hi == 5) {
            char *d = px_alloc((usize)cnt + 1);
            if (!d) return 0;
            for (u64 i = 0; i < cnt; i++) d[i] = (char)x->b[at + i];
            d[cnt] = 0;
            n->str = d;
        } else n->str = pb_utf16(x->b + at, cnt);
        return n;
    }
    case 0xA: case 0xD: {
        if (!pb_count(x, &at, lo, &cnt)) { x->err = 1; return 0; }
        u64 refs = hi == 0xD ? cnt * 2 : cnt;
        if (cnt > x->nobj || at + refs * x->rsz > x->offs) { x->err = 1; return 0; }
        if (!(n = px_new(hi == 0xD ? PN_DICT : PN_ARRAY))) return 0;
        pnode_t **tail = &n->child;
        for (u64 i = 0; i < cnt; i++) {
            u64 vr = pb_be(x->b + at + (usize)((hi == 0xD ? cnt + i : i) * x->rsz), x->rsz);
            pnode_t *v = pb_obj(x, vr, depth + 1);
            if (!v) { x->err = 1; return n; }
            if (hi == 0xD) {
                pnode_t *k = pb_obj(x, pb_be(x->b + at + (usize)(i * x->rsz), x->rsz), depth + 1);
                if (!k || k->type != PN_STRING) { x->err = 1; return n; }
                v->key = k->str;
            }
            *tail = v;
            tail = &v->next;
        }
        return n;
    }
    }
    x->err = 1;                                          /* UID (0x8) e set (0xC) não aparecem em plist de launchd */
    return 0;
}
static pnode_t *plist_parse_binary(const unsigned char *b, usize len) {
    if (len < 8 + 1 + 32 || memcmp(b, "bplist00", 8)) return 0;
    const unsigned char *t = b + len - 32;
    pbin_t x = { b, len, 0, t[6], t[7], pb_be(t + 8, 8), 0, 0 };
    u64 top = pb_be(t + 16, 8), offs = pb_be(t + 24, 8);
    if (x.osz < 1 || x.osz > 8 || x.rsz < 1 || x.rsz > 8 || offs < 9 || offs >= len - 32 || x.nobj == 0
        || x.nobj > (len - 32 - offs) / x.osz)
        return 0;
    x.offs = (usize)offs;
    pnode_t *n = pb_obj(&x, top, 0);
    return x.err ? 0 : n;
}

static pnode_t *plist_parse(const char *txt, usize len) {
    if (len >= 8 && !memcmp(txt, "bplist00", 8)) return plist_parse_binary((const unsigned char *)txt, len);
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
