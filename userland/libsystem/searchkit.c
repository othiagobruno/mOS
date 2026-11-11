/*
 * mOS — SearchKit (CoreServices): índice de texto e busca. O Setapp guarda nele a busca do catálogo de apps.
 *
 * SKIndex é um índice invertido simples em memória: cada documento (SKDocument: esquema, pai, nome) guarda os termos
 * do texto que recebeu (SKIndexAddDocumentWithText) com a contagem. Os termos saem do texto como no SearchKit:
 * letras e dígitos (e os bytes UTF-8 não ASCII), mais os caracteres de kSKTermChars; minúsculas; menores que
 * kSKMinTermLength e as palavras de kSKStopWords ficam de fora. O índice criado com uma URL é gravado nesse arquivo
 * no SKIndexFlush/SKIndexCompact/SKIndexClose e lido de volta pelo SKIndexOpenWithURL. Índices só em memória
 * (SKIndexCreateWithMutableData) não existem aqui.
 *
 * Consulta (SKSearchCreate): termos separados por espaço, todos obrigatórios (kSKSearchOptionSpaceMeansOR = 2 troca
 * por "qualquer um"); AND/&, OR/|, NOT/! entre termos; "*" no fim de um termo casa prefixo. A relevância é a soma das
 * contagens dos termos que casaram. SKSearchFindMatches entrega os resultados em ordem de relevância.
 */
#include "foundation.h"
#include "libc.h"

typedef unsigned char Boolean;
typedef long CFIndex;
cx_class_t *objc_allocateClassPair(cx_class_t *super, const char *name, u64 extra);
void objc_registerClassPair(cx_class_t *c);
BOOL class_addIvar(cx_class_t *c, const char *name, u64 size, u8 align, const char *types);
BOOL class_addMethod(cx_class_t *c, SEL s, IMP imp, const char *types);
IMP class_getMethodImplementation(cx_class_t *c, SEL s);
id class_createInstance(cx_class_t *c, u64 extra);
id CFRetain(id o);
void CFRelease(id o);
id CFStringCreateWithCString(void *alloc, const char *s, u32 enc);
int sched_yield(void);

/* ---- constantes das propriedades de análise de texto (CFString com o próprio nome, como na Apple) */
extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } sk_cfconst_t;
#define SKK(var) static sk_cfconst_t var##_cs = {cx_cfconst_isa, 0x7c8, #var, sizeof(#var) - 1}; const void *var = &var##_cs;
SKK(kSKStartTermChars) SKK(kSKEndTermChars) SKK(kSKTermChars) SKK(kSKMinTermLength) SKK(kSKStopWords)
SKK(kSKMaximumTerms) SKK(kSKProximityIndexing) SKK(kSKLanguageTypes) SKK(kSKSubstitutions)

/* ---- objetos: instâncias ObjC (CFRetain/CFRelease funcionam), com o estado C depois do isa */
typedef struct { char *t; int n; } sk_term_t;
typedef struct sk_doc {
    cx_class_t *isa;
    u32 magic;
    id scheme, name;          /* NSString */
    struct sk_doc *parent;    /* retido */
} sk_doc_t;
typedef struct {
    sk_doc_t *doc;            /* retido; NULL = removido */
    sk_term_t *terms;
    int nterms;
} sk_entry_t;
typedef struct {
    cx_class_t *isa;
    u32 magic;
    char *path;               /* arquivo do índice (NULL = sem arquivo) */
    id name;
    int writable, dirty;
    sk_entry_t *e;            /* documento de id i em e[i - 1] */
    int n, cap;
    char termchars[64], startchars[64], endchars[64];
    int minlen;
    id stop;                  /* NSSet ou nil */
    volatile int lk;
} sk_index_t;
typedef struct {
    cx_class_t *isa;
    u32 magic;
    sk_index_t *idx;          /* retido */
    long *ids;
    float *score;
    int n, pos, cancelled;
} sk_search_t;
#define DOC_MAGIC 0x534B444Fu
#define IDX_MAGIC 0x534B4958u
#define SRCH_MAGIC 0x534B5352u

static IMP nsobject_dealloc;
static void super_dealloc(id self) { if (nsobject_dealloc) ((void (*)(id, SEL))nsobject_dealloc)(self, sel_registerName("dealloc")); }
static void free_terms(sk_entry_t *e) { for (int i = 0; i < e->nterms; i++) free(e->terms[i].t); free(e->terms); e->terms = 0; e->nterms = 0; }
static void doc_dealloc(id self, SEL s) {
    (void)s;
    sk_doc_t *d = (sk_doc_t *)self;
    if (d->scheme) CFRelease(d->scheme);
    if (d->name) CFRelease(d->name);
    if (d->parent) CFRelease((id)d->parent);
    super_dealloc(self);
}
static void index_flush(sk_index_t *x);
static void index_dealloc(id self, SEL s) {
    (void)s;
    sk_index_t *x = (sk_index_t *)self;
    for (int i = 0; i < x->n; i++) { free_terms(&x->e[i]); if (x->e[i].doc) CFRelease((id)x->e[i].doc); }
    free(x->e);
    free(x->path);
    if (x->name) CFRelease(x->name);
    if (x->stop) CFRelease(x->stop);
    super_dealloc(self);
}
static void search_dealloc(id self, SEL s) {
    (void)s;
    sk_search_t *q = (sk_search_t *)self;
    free(q->ids);
    free(q->score);
    if (q->idx) CFRelease((id)q->idx);
    super_dealloc(self);
}
static cx_class_t *make_class(const char *name, u64 size, IMP dealloc) {
    cx_class_t *ns = cx_class_named("NSObject");
    if (!nsobject_dealloc && ns) nsobject_dealloc = class_getMethodImplementation(ns, sel_registerName("dealloc"));
    cx_class_t *k = objc_allocateClassPair(ns, name, 0);
    if (!k) return cx_class_named(name);
    class_addIvar(k, "_sk", size - 8, 3, "?");
    class_addMethod(k, sel_registerName("dealloc"), dealloc, "v@:");
    objc_registerClassPair(k);
    return k;
}
static id new_obj(const char *cls, u64 size, IMP dealloc, cx_class_t **cache, u32 magic) {
    static volatile int lk;
    while (__sync_lock_test_and_set(&lk, 1)) sched_yield();
    if (!*cache) *cache = make_class(cls, size, dealloc);
    __sync_lock_release(&lk);
    id o = class_createInstance(*cache, 0);
    if (!o) return 0;
    memset((u8 *)o + 8, 0, size - 8);
    *(u32 *)((u8 *)o + 8) = magic;
    return o;
}
static cx_class_t *doc_cls, *idx_cls, *srch_cls;
static sk_doc_t *D(void *p) { sk_doc_t *d = p; return d && d->magic == DOC_MAGIC ? d : 0; }
static sk_index_t *X(void *p) { sk_index_t *x = p; return x && x->magic == IDX_MAGIC ? x : 0; }
static sk_search_t *Q(void *p) { sk_search_t *q = p; return q && q->magic == SRCH_MAGIC ? q : 0; }
static void lock(sk_index_t *x) { while (__sync_lock_test_and_set(&x->lk, 1)) sched_yield(); }
static void unlock(sk_index_t *x) { __sync_lock_release(&x->lk); }

static id str_copy(id s) { return s ? MSG0(id, s, "copy") : 0; }
static int str_eq(id a, id b) {
    if (!a || !b) return a == b;
    const char *x = cx_cstr(a), *y = cx_cstr(b);
    return x && y && !strcmp(x, y);
}

/* ---- SKDocument */
u64 SKDocumentGetTypeID(void) { return 0x5C0D0; }
id SKDocumentCreate(id scheme, id parent, id name) {
    if (!name) return 0;
    sk_doc_t *d = (sk_doc_t *)new_obj("SKDocument", sizeof(sk_doc_t), (IMP)doc_dealloc, &doc_cls, DOC_MAGIC);
    if (!d) return 0;
    d->scheme = str_copy(scheme ? scheme : (parent && D(parent) ? D(parent)->scheme : 0));
    if (!d->scheme) d->scheme = CFStringCreateWithCString(0, "data", 0x08000100);
    d->name = str_copy(name);
    d->parent = D(parent) ? (sk_doc_t *)CFRetain(parent) : 0;
    return (id)d;
}
id SKDocumentGetName(id doc) { sk_doc_t *d = D(doc); return d ? d->name : 0; }
id SKDocumentGetSchemeName(id doc) { sk_doc_t *d = D(doc); return d ? d->scheme : 0; }
id SKDocumentGetParent(id doc) { sk_doc_t *d = D(doc); return d ? (id)d->parent : 0; }
static int doc_eq(sk_doc_t *a, sk_doc_t *b) {
    for (; a && b; a = a->parent, b = b->parent)
        if (!str_eq(a->name, b->name) || !str_eq(a->scheme, b->scheme)) return 0;
    return a == b;
}

/* ---- termos */
static int in_set(const char *set, unsigned c) { return c && set[0] && strchr(set, (int)c) != 0; }
static int is_term_char(sk_index_t *x, unsigned c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c >= 0x80 || in_set(x->termchars, c);
}
static int stopword(sk_index_t *x, const char *w) {
    if (!x->stop) return 0;
    id s = cx_str(w);
    BOOL r = MSG(BOOL, x->stop, "containsObject:", s);
    return r;
}
static void add_term(sk_entry_t *e, const char *w, int len) {
    for (int i = 0; i < e->nterms; i++)
        if ((int)strlen(e->terms[i].t) == len && !memcmp(e->terms[i].t, w, (size_t)len)) { e->terms[i].n++; return; }
    if ((e->nterms & 15) == 0) {
        sk_term_t *n = realloc(e->terms, sizeof *n * (size_t)(e->nterms + 16));
        if (!n) return;
        e->terms = n;
    }
    char *t = malloc((size_t)len + 1);
    if (!t) return;
    memcpy(t, w, (size_t)len);
    t[len] = 0;
    e->terms[e->nterms].t = t;
    e->terms[e->nterms++].n = 1;
}
/* quebra o texto em termos minúsculos e chama fn para cada um */
static void tokens(sk_index_t *x, const char *s, void (*fn)(void *, const char *, int), void *ctx) {
    char w[256];
    int n = 0;
    for (const unsigned char *p = (const unsigned char *)s; ; p++) {
        unsigned c = *p;
        if (c && is_term_char(x, c)) {
            if (n < (int)sizeof w - 1) w[n++] = (char)(c >= 'A' && c <= 'Z' ? c + 32 : c);
            continue;
        }
        if (n) {
            /* caracteres que só valem no meio do termo (kSKStartTermChars/kSKEndTermChars) saem das pontas */
            int a = 0, b = n;
            while (a < b && in_set(x->termchars, (unsigned char)w[a]) && !in_set(x->startchars, (unsigned char)w[a])) a++;
            while (b > a && in_set(x->termchars, (unsigned char)w[b - 1]) && !in_set(x->endchars, (unsigned char)w[b - 1])) b--;
            w[b] = 0;
            if (b - a >= x->minlen && !stopword(x, w + a)) fn(ctx, w + a, b - a);
            n = 0;
        }
        if (!c) break;
    }
}
static void tok_add(void *ctx, const char *w, int len) { add_term(ctx, w, len); }

/* ---- SKIndex */
u64 SKIndexGetTypeID(void) { return 0x5C1D0; }
static void copy_chars(char *dst, id props, const void *key) {
    dst[0] = 0;
    id v = props ? MSG(id, props, "objectForKey:", (id)key) : 0;
    const char *s = v ? cx_cstr(v) : 0;
    if (s) { size_t n = strlen(s); if (n > 63) n = 63; memcpy(dst, s, n); dst[n] = 0; }
}
static sk_index_t *index_new(const char *path, id name, id props, int writable) {
    sk_index_t *x = (sk_index_t *)new_obj("SKIndex", sizeof(sk_index_t), (IMP)index_dealloc, &idx_cls, IDX_MAGIC);
    if (!x) return 0;
    x->path = path ? strdup(path) : 0;
    x->name = str_copy(name);
    x->writable = writable;
    x->minlen = 1;
    copy_chars(x->termchars, props, kSKTermChars);
    copy_chars(x->startchars, props, kSKStartTermChars);
    copy_chars(x->endchars, props, kSKEndTermChars);
    id ml = props ? MSG(id, props, "objectForKey:", (id)kSKMinTermLength) : 0;
    if (ml) x->minlen = MSG0(int, ml, "intValue");
    if (x->minlen < 1) x->minlen = 1;
    id st = props ? MSG(id, props, "objectForKey:", (id)kSKStopWords) : 0;
    if (st) x->stop = CFRetain(st);
    return x;
}
static int entry_add(sk_index_t *x, sk_doc_t *d) {
    if (x->n == x->cap) {
        int nc = x->cap ? x->cap * 2 : 32;
        sk_entry_t *ne = realloc(x->e, sizeof *ne * (size_t)nc);
        if (!ne) return -1;
        x->e = ne;
        x->cap = nc;
    }
    memset(&x->e[x->n], 0, sizeof x->e[0]);
    x->e[x->n].doc = d ? (sk_doc_t *)CFRetain((id)d) : 0;          /* nulo: id de documento removido */
    return x->n++;
}
static const char *url_path(id url) {
    if (!url) return 0;
    id p = MSG0(id, url, "path");
    return p ? cx_cstr(p) : 0;
}

/* arquivo: "SKIDX 1", depois por documento "D <esquema> <nº de nomes> <nome>... (do pai para o filho)" e
 * "T <contagem> <termo>"; campos separados por TAB, uma linha por registro; documento removido = "D -" */
static void put_doc(FILE_ *f, sk_doc_t *d) {
    sk_doc_t *chain[32];
    int k = 0;
    for (sk_doc_t *p = d; p && k < 32; p = p->parent) chain[k++] = p;
    char b[64];
    snprintf(b, sizeof b, "D\t%s\t%d", cx_cstr(d->scheme) ? cx_cstr(d->scheme) : "data", k);
    fwrite(b, 1, strlen(b), f);
    while (k--) {
        const char *n = cx_cstr(chain[k]->name);
        fwrite("\t", 1, 1, f);
        if (n) fwrite(n, 1, strlen(n), f);
    }
    fwrite("\n", 1, 1, f);
}
static void index_flush(sk_index_t *x) {
    if (!x->path || !x->writable || !x->dirty) return;
    char tmp[1100];
    snprintf(tmp, sizeof tmp, "%s.tmp", x->path);
    FILE_ *f = fopen(tmp, "w");
    if (!f) return;
    fwrite("SKIDX 1\n", 1, 8, f);
    for (int i = 0; i < x->n; i++) {
        if (!x->e[i].doc) { fwrite("D -\n", 1, 4, f); continue; }
        put_doc(f, x->e[i].doc);
        for (int j = 0; j < x->e[i].nterms; j++) {
            char b[32];
            snprintf(b, sizeof b, "T\t%d\t", x->e[i].terms[j].n);
            fwrite(b, 1, strlen(b), f);
            fwrite(x->e[i].terms[j].t, 1, strlen(x->e[i].terms[j].t), f);
            fwrite("\n", 1, 1, f);
        }
    }
    fclose(f);
    int rename(const char *a, const char *b);
    if (rename(tmp, x->path) == 0) x->dirty = 0;
}
static int index_load(sk_index_t *x) {
    FILE_ *f = fopen(x->path, "r");
    if (!f) return 0;
    size_t cap = 1 << 16, len = 0;
    char *buf = malloc(cap + 1);
    size_t r;
    while (buf && (r = fread(buf + len, 1, cap - len, f)) > 0) {
        len += r;
        if (len == cap) { char *nb = realloc(buf, cap * 2 + 1); if (!nb) break; buf = nb; cap *= 2; }
    }
    fclose(f);
    if (!buf) return 0;
    buf[len] = 0;
    if (strncmp(buf, "SKIDX 1\n", 8)) { free(buf); return 0; }
    sk_entry_t *cur = 0;
    for (char *l = buf + 8, *nl; *l; l = nl) {
        nl = strchr(l, '\n');
        if (nl) *nl++ = 0; else nl = l + strlen(l);
        if (l[0] == 'D' && l[1] == ' ' && l[2] == '-') {          /* removido: guarda o id */
            int i = entry_add(x, 0);
            if (i < 0) break;
            cur = 0;
            continue;
        }
        if (l[0] == 'D' && l[1] == '\t') {
            char *f2[40];
            int k = 0;
            for (char *p = l + 2; k < 40; ) { f2[k++] = p; p = strchr(p, '\t'); if (!p) break; *p++ = 0; }
            if (k < 3) { cur = 0; continue; }
            id scheme = cx_str(f2[0]);
            int nn = atoi(f2[1]);
            sk_doc_t *d = 0;
            for (int j = 0; j < nn && 2 + j < k; j++) {
                sk_doc_t *nd = (sk_doc_t *)SKDocumentCreate(scheme, (id)d, cx_str(f2[2 + j]));
                if (d) CFRelease((id)d);
                d = nd;
            }
            if (!d) { cur = 0; continue; }
            int i = entry_add(x, d);
            CFRelease((id)d);
            cur = i >= 0 ? &x->e[i] : 0;
            continue;
        }
        if (l[0] == 'T' && l[1] == '\t' && cur) {
            char *t = strchr(l + 2, '\t');
            if (!t) continue;
            *t++ = 0;
            add_term(cur, t, (int)strlen(t));
            cur->terms[cur->nterms - 1].n = atoi(l + 2);
        }
    }
    free(buf);
    return 1;
}
id SKIndexCreateWithURL(id url, id name, int type, id props) {
    (void)type;
    const char *p = url_path(url);
    if (!p || access(p, 0) == 0) return 0;                       /* o arquivo não pode existir (como na Apple) */
    sk_index_t *x = index_new(p, name, props, 1);
    if (!x) return 0;
    x->dirty = 1;
    index_flush(x);
    if (access(p, 0) != 0) { CFRelease((id)x); return 0; }
    return (id)x;
}
id SKIndexOpenWithURL(id url, id name, Boolean writable) {
    const char *p = url_path(url);
    if (!p) return 0;
    sk_index_t *x = index_new(p, name, 0, writable);
    if (!x) return 0;
    if (!index_load(x)) { CFRelease((id)x); return 0; }
    return (id)x;
}
id SKIndexCreateWithMutableData(id data, id name, int type, id props) { (void)data; (void)name; (void)type; (void)props; return 0; }
id SKIndexOpenWithData(id data, id name) { (void)data; (void)name; return 0; }
id SKIndexOpenWithMutableData(id data, id name) { (void)data; (void)name; return 0; }
Boolean SKIndexFlush(id index) {
    sk_index_t *x = X(index);
    if (!x) return 0;
    lock(x);
    index_flush(x);
    int ok = !x->dirty || !x->path;
    unlock(x);
    return (Boolean)ok;
}
Boolean SKIndexCompact(id index) { return SKIndexFlush(index); }
void SKIndexClose(id index) {
    sk_index_t *x = X(index);
    if (!x) return;
    SKIndexFlush(index);
    CFRelease(index);
}
static int find_doc(sk_index_t *x, sk_doc_t *d) {
    for (int i = 0; i < x->n; i++) if (x->e[i].doc && doc_eq(x->e[i].doc, d)) return i;
    return -1;
}
Boolean SKIndexAddDocumentWithText(id index, id doc, id text, Boolean canReplace) {
    sk_index_t *x = X(index);
    sk_doc_t *d = D(doc);
    if (!x || !d || !x->writable) return 0;
    lock(x);
    int i = find_doc(x, d);
    if (i >= 0 && !canReplace) { unlock(x); return 0; }
    if (i < 0) i = entry_add(x, d);
    if (i < 0) { unlock(x); return 0; }
    free_terms(&x->e[i]);
    const char *s = text ? cx_cstr(text) : 0;
    if (s) tokens(x, s, tok_add, &x->e[i]);
    x->dirty = 1;
    unlock(x);
    return 1;
}
Boolean SKIndexRemoveDocument(id index, id doc) {
    sk_index_t *x = X(index);
    sk_doc_t *d = D(doc);
    if (!x || !d || !x->writable) return 0;
    lock(x);
    int ok = 0;
    for (int i = 0; i < x->n; i++) {
        sk_doc_t *e = x->e[i].doc;
        if (!e) continue;
        /* o documento e os que estão abaixo dele */
        for (sk_doc_t *p = e; p; p = p->parent)
            if (doc_eq(p, d)) { free_terms(&x->e[i]); CFRelease((id)e); x->e[i].doc = 0; ok = 1; break; }
    }
    if (ok) x->dirty = 1;
    unlock(x);
    return (Boolean)ok;
}
id SKIndexCopyDocumentForDocumentID(id index, long docID) {
    sk_index_t *x = X(index);
    if (!x || docID < 1) return 0;
    lock(x);
    sk_doc_t *d = docID <= x->n ? x->e[docID - 1].doc : 0;
    if (d) CFRetain((id)d);
    unlock(x);
    return (id)d;
}
long SKIndexGetDocumentID(id index, id doc) {
    sk_index_t *x = X(index);
    sk_doc_t *d = D(doc);
    if (!x || !d) return 0;
    lock(x);
    int i = find_doc(x, d);
    unlock(x);
    return i < 0 ? 0 : i + 1;
}
CFIndex SKIndexGetDocumentCount(id index) {
    sk_index_t *x = X(index);
    if (!x) return 0;
    CFIndex n = 0;
    for (int i = 0; i < x->n; i++) if (x->e[i].doc) n++;
    return n;
}

/* ---- SKSearch */
u64 SKSearchGetTypeID(void) { return 0x5C5D0; }
typedef struct { char *t[64]; int len[64]; int pref[64]; int op[64]; int n; } sk_query_t;   /* op: 0 E, 1 OU, 2 NÃO */
static void tok_query(void *ctx, const char *w, int len) {
    sk_query_t *q = ctx;
    if (q->n >= 64) return;
    q->t[q->n] = malloc((size_t)len + 1);
    if (!q->t[q->n]) return;
    memcpy(q->t[q->n], w, (size_t)len);
    q->t[q->n][len] = 0;
    q->len[q->n] = len;
    q->n++;
}
static int term_count(sk_entry_t *e, const char *t, int len, int pref) {
    int c = 0;
    for (int i = 0; i < e->nterms; i++) {
        const char *x = e->terms[i].t;
        if (pref ? !strncmp(x, t, (size_t)len) : !strcmp(x, t)) c += e->terms[i].n;
    }
    return c;
}
id SKSearchCreate(id index, id query, u32 options) {
    sk_index_t *x = X(index);
    const char *s = query ? cx_cstr(query) : 0;
    if (!x || !s) return 0;
    sk_search_t *q = (sk_search_t *)new_obj("SKSearch", sizeof(sk_search_t), (IMP)search_dealloc, &srch_cls, SRCH_MAGIC);
    if (!q) return 0;
    q->idx = (sk_index_t *)CFRetain(index);
    /* palavra a palavra: operadores, prefixo "*" e os termos (o mesmo tokenizador do índice) */
    sk_query_t qq;
    memset(&qq, 0, sizeof qq);
    int next_op = 0;
    char w[256];
    const char *p = s;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == '(' || *p == ')' || *p == '"') p++;
        if (!*p) break;
        int n = 0;
        while (p[n] && p[n] != ' ' && p[n] != '\t' && p[n] != ')' && p[n] != '"' && n < (int)sizeof w - 1) { w[n] = p[n]; n++; }
        w[n] = 0;
        p += n;
        if (!strcmp(w, "AND") || !strcmp(w, "&")) { next_op = 0; continue; }
        if (!strcmp(w, "OR") || !strcmp(w, "|")) { next_op = 1; continue; }
        if (!strcmp(w, "NOT") || !strcmp(w, "!")) { next_op = 2; continue; }
        int neg = w[0] == '!' || w[0] == '-';
        int pref = n > 0 && w[n - 1] == '*';
        int before = qq.n;
        tokens(x, w + neg, tok_query, &qq);
        for (int i = before; i < qq.n; i++) {
            qq.op[i] = neg ? 2 : (i == before ? (next_op == 0 && (options & 2) ? 1 : next_op) : 0);
            qq.pref[i] = pref && i == qq.n - 1;
        }
        next_op = 0;
    }
    lock(x);
    q->ids = malloc(sizeof *q->ids * (size_t)(x->n + 1));
    q->score = malloc(sizeof *q->score * (size_t)(x->n + 1));
    for (int i = 0; q->ids && q->score && i < x->n; i++) {
        sk_entry_t *e = &x->e[i];
        if (!e->doc || !qq.n) continue;
        int ok = -1, sc = 0;
        for (int k = 0; k < qq.n; k++) {
            int c = term_count(e, qq.t[k], qq.len[k], qq.pref[k]);
            int hit = c > 0;
            if (qq.op[k] == 2) { ok = (ok < 0 ? 1 : ok) && !hit; continue; }
            if (ok < 0) ok = hit;
            else if (qq.op[k] == 1) ok = ok || hit;
            else ok = ok && hit;
            sc += c;
        }
        if (ok > 0) { q->ids[q->n] = i + 1; q->score[q->n] = (float)sc; q->n++; }
    }
    unlock(x);
    for (int k = 0; k < qq.n; k++) free(qq.t[k]);
    /* relevância decrescente (inserção: os resultados são poucos) */
    for (int i = 1; i < q->n; i++) {
        long id_ = q->ids[i];
        float s2 = q->score[i];
        int j = i - 1;
        while (j >= 0 && q->score[j] < s2) { q->ids[j + 1] = q->ids[j]; q->score[j + 1] = q->score[j]; j--; }
        q->ids[j + 1] = id_;
        q->score[j + 1] = s2;
    }
    return (id)q;
}
Boolean SKSearchFindMatches(id search, CFIndex maxCount, long *ids, float *scores, double maxTime, CFIndex *found) {
    (void)maxTime;
    sk_search_t *q = Q(search);
    if (found) *found = 0;
    if (!q || q->cancelled) return 0;
    CFIndex k = 0;
    while (k < maxCount && q->pos < q->n) {
        if (ids) ids[k] = q->ids[q->pos];
        if (scores) scores[k] = q->score[q->pos];
        k++;
        q->pos++;
    }
    if (found) *found = k;
    return q->pos < q->n;
}
void SKSearchCancel(id search) { sk_search_t *q = Q(search); if (q) q->cancelled = 1; }
