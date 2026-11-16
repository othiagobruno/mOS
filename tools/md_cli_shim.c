/* mOS — CF/MD mínimos de host para testar o mdfind(1) e o mdls(1) de userland/cli-macos (tools/mdclitest.sh).
 * Só a forma (argumentos, formatação, códigos de saída): a busca usa o mesmo predicado do libSystem (mdq_core.c) sobre
 * uma árvore de verdade do host, com kMDItemPath, kMDItemFSName, kMDItemFSSize, kMDItemContentModificationDate (data)
 * e kMDItemContentTypeTree (lista fixa); o MDQuery de verdade (mdquery.c) só roda no mOS. */
#define _GNU_SOURCE
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "mdq_core.h"

enum { T_STR = 1, T_NUM, T_DATE, T_ARR, T_ITEM, T_QUERY, T_BOOL };
typedef struct obj { int t; char *s; double d; struct obj **a; long n; void *q; } obj;
static obj *mk(int t) { obj *o = calloc(1, sizeof *o); o->t = t; return o; }
const void *CFStringCreateWithCString(const void *al, const char *s, unsigned e) { (void)al; (void)e; obj *o = mk(T_STR); o->s = strdup(s); return o; }
void CFRelease(const void *o) { (void)o; }   /* o teste é um processo curto */
unsigned char CFStringGetCString(const void *s, char *b, long n, unsigned e) { (void)e; snprintf(b, (size_t)n, "%s", ((const obj *)s)->s); return 1; }
const void *CFArrayCreate(const void *al, const void **v, long n, const void *cb) {
    (void)al; (void)cb;
    obj *o = mk(T_ARR);
    o->a = calloc((size_t)n + 1, sizeof *o->a);
    for (long i = 0; i < n; i++) o->a[i] = (obj *)v[i];
    o->n = n;
    return o;
}
unsigned long CFGetTypeID(const void *o) { return (unsigned long)((const obj *)o)->t; }
unsigned long CFStringGetTypeID(void) { return T_STR; }
unsigned long CFNumberGetTypeID(void) { return T_NUM; }
unsigned long CFArrayGetTypeID(void) { return T_ARR; }
unsigned long CFDateGetTypeID(void) { return T_DATE; }
unsigned long CFBooleanGetTypeID(void) { return T_BOOL; }
long CFArrayGetCount(const void *a) { return ((const obj *)a)->n; }
const void *CFArrayGetValueAtIndex(const void *a, long i) { return ((const obj *)a)->a[i]; }
unsigned char CFNumberIsFloatType(const void *n) { (void)n; return 0; }
unsigned char CFNumberGetValue(const void *n, long t, void *out) { (void)t; *(long long *)out = (long long)((const obj *)n)->d; return 1; }
double CFDateGetAbsoluteTime(const void *d) { return ((const obj *)d)->d; }
unsigned char CFBooleanGetValue(const void *b) { return ((const obj *)b)->d != 0; }

/* ---- MDItem */
typedef struct { char path[2048]; char name[512]; char size[32]; char mtime[32]; } item_t;
static const char *TREE[] = {"public.plain-text", "public.text", "public.data", 0};
static const char *get(void *ctx, const char *attr, int i) {
    item_t *it = ctx;
    if (!strcmp(attr, "kMDItemContentTypeTree")) return TREE[i];
    if (i) return 0;
    if (!strcmp(attr, "kMDItemPath")) return it->path;
    if (!strcmp(attr, "kMDItemFSName") || !strcmp(attr, "kMDItemDisplayName")) return it->name;
    if (!strcmp(attr, "kMDItemFSSize")) return it->size;
    if (!strcmp(attr, "kMDItemContentModificationDate")) return it->mtime;
    return 0;
}
static obj *item_of_path(const char *p) {
    struct stat st;
    if (stat(p, &st)) return 0;
    obj *o = mk(T_ITEM);
    item_t *it = calloc(1, sizeof *it);
    snprintf(it->path, sizeof it->path, "%s", p);
    const char *sl = strrchr(p, '/');
    snprintf(it->name, sizeof it->name, "%s", sl ? sl + 1 : p);
    snprintf(it->size, sizeof it->size, "%lld", (long long)st.st_size);
    snprintf(it->mtime, sizeof it->mtime, "%.0f", (double)st.st_mtime - 978307200.0);
    o->q = it;
    return o;
}
const void *MDItemCreate(const void *al, const void *path) { (void)al; return item_of_path(((const obj *)path)->s); }
const void *MDItemCopyAttribute(const void *item, const void *name) {
    item_t *it = ((const obj *)item)->q;
    const char *a = ((const obj *)name)->s;
    if (!strcmp(a, "kMDItemContentTypeTree")) {
        const void *v[3];
        for (int i = 0; i < 3; i++) v[i] = CFStringCreateWithCString(0, TREE[i], 0);
        return CFArrayCreate(0, v, 3, 0);
    }
    if (!strcmp(a, "kMDItemFSSize")) { obj *o = mk(T_NUM); o->d = atof(it->size); return o; }
    if (!strcmp(a, "kMDItemContentModificationDate")) { obj *o = mk(T_DATE); o->d = atof(it->mtime); return o; }
    const char *v = get(it, a, 0);
    return v ? CFStringCreateWithCString(0, v, 0) : 0;
}
const void *MDItemCopyAttributeNames(const void *item) {
    (void)item;
    static const char *n[] = {"kMDItemPath", "kMDItemFSName", "kMDItemFSSize", "kMDItemContentModificationDate", "kMDItemContentTypeTree"};
    const void *v[5];
    for (int i = 0; i < 5; i++) v[i] = CFStringCreateWithCString(0, n[i], 0);
    return CFArrayCreate(0, v, 5, 0);
}

/* ---- MDQuery: varre os escopos (o padrão é a pasta MDCLI_HOME) */
typedef struct { mdq_node *pred; char *scopes[16]; int ns; obj *res[4096]; long n; } query_t;
const void *MDQueryCreate(const void *al, const void *q, const void *vl, const void *so) {
    (void)al; (void)vl; (void)so;
    char err[96];
    mdq_node *p = mdq_parse(((const obj *)q)->s, err, sizeof err);
    if (!p) return 0;
    obj *o = mk(T_QUERY);
    query_t *qq = calloc(1, sizeof *qq);
    qq->pred = p;
    o->q = qq;
    return o;
}
void MDQuerySetSearchScope(const void *q, const void *scopes, unsigned opts) {
    (void)opts;
    query_t *qq = ((const obj *)q)->q;
    const obj *a = scopes;
    for (long i = 0; i < a->n && qq->ns < 16; i++) qq->scopes[qq->ns++] = strdup(a->a[i]->s);
}
static void walk(query_t *q, const char *dir) {
    DIR *d = opendir(dir);
    struct dirent *e;
    while (d && (e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char p[2048];
        snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
        obj *it = item_of_path(p);
        if (it && mdq_eval(q->pred, get, it->q) && q->n < 4096) q->res[q->n++] = it;
        struct stat st;
        if (!stat(p, &st) && S_ISDIR(st.st_mode)) walk(q, p);
    }
    if (d) closedir(d);
}
static int cmpres(const void *a, const void *b) { return strcmp(((item_t *)(*(obj *const *)a)->q)->path, ((item_t *)(*(obj *const *)b)->q)->path); }
unsigned char MDQueryExecute(const void *q, unsigned opts) {
    (void)opts;
    query_t *qq = ((const obj *)q)->q;
    qq->n = 0;
    if (!qq->ns) qq->scopes[qq->ns++] = getenv("MDCLI_HOME");
    for (int i = 0; i < qq->ns; i++) walk(qq, qq->scopes[i]);
    qsort(qq->res, (size_t)qq->n, sizeof *qq->res, cmpres);
    return 1;
}
long MDQueryGetResultCount(const void *q) { return ((query_t *)((const obj *)q)->q)->n; }
const void *MDQueryGetResultAtIndex(const void *q, long i) { return ((query_t *)((const obj *)q)->q)->res[i]; }
