/* mOS — mdfind(1): busca do Spotlight pela API MDQuery do libSystem (userland/libsystem/mdquery.c).
 *   mdfind [-onlyin pasta]... [-name nome] [-count] [-0] [-literal] [-interpret] [-live] consulta
 * A consulta é a sintaxe do Spotlight (kMDItemContentType == "public.image" && kMDItemFSName == "*.png"c,
 * $time.today(-7), InRange(...)); uma palavra solta busca no nome. -name junta "kMDItemFSName == '*nome*'cd".
 * -live mostra o resultado e depois os caminhos novos, refazendo a coleta a cada segundo, até Ctrl-C.
 * Saída: um caminho por linha (ou separados por NUL com -0); -count só o número. Erro de sintaxe: código 1. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef const void *CFTypeRef;
typedef long CFIndex;
CFTypeRef CFStringCreateWithCString(CFTypeRef alloc, const char *s, unsigned enc);
CFTypeRef CFArrayCreate(CFTypeRef alloc, const void **vals, CFIndex n, const void *cb);
void CFRelease(CFTypeRef o);
unsigned char CFStringGetCString(CFTypeRef s, char *buf, CFIndex n, unsigned enc);
CFTypeRef MDQueryCreate(CFTypeRef alloc, CFTypeRef query, CFTypeRef vl, CFTypeRef sort);
void MDQuerySetSearchScope(CFTypeRef q, CFTypeRef scopes, unsigned opts);
unsigned char MDQueryExecute(CFTypeRef q, unsigned opts);
CFIndex MDQueryGetResultCount(CFTypeRef q);
CFTypeRef MDQueryGetResultAtIndex(CFTypeRef q, CFIndex i);
CFTypeRef MDItemCopyAttribute(CFTypeRef item, CFTypeRef name);
#define UTF8 0x08000100u
enum { kMDQuerySynchronous = 1 };

static void usage(void) {
    fprintf(stderr, "uso: mdfind [-onlyin pasta] [-name nome] [-count] [-0] [-live] consulta\n");
}
/* aspas simples do -name: escapa ' e \ */
static void quote(char *out, size_t cap, const char *s) {
    size_t o = 0;
    for (; *s && o + 3 < cap; s++) {
        if (*s == '\'' || *s == '\\') out[o++] = '\\';
        out[o++] = *s;
    }
    out[o] = 0;
}

static char **g_seen;
static size_t g_nseen, g_capseen;
static int seen(const char *p) {
    for (size_t i = 0; i < g_nseen; i++) if (!strcmp(g_seen[i], p)) return 1;
    if (g_nseen == g_capseen) { g_capseen = g_capseen ? g_capseen * 2 : 256; g_seen = realloc(g_seen, g_capseen * sizeof *g_seen); }
    g_seen[g_nseen++] = strdup(p);
    return 0;
}
static int print_results(CFTypeRef q, int nul, int only_new) {
    CFIndex n = MDQueryGetResultCount(q);
    CFTypeRef key = CFStringCreateWithCString(0, "kMDItemPath", UTF8);
    for (CFIndex i = 0; i < n; i++) {
        CFTypeRef it = MDQueryGetResultAtIndex(q, i);
        CFTypeRef p = it ? MDItemCopyAttribute(it, key) : 0;
        char buf[4096];
        if (p && CFStringGetCString(p, buf, sizeof buf, UTF8) && (!only_new || !seen(buf))) {
            fputs(buf, stdout);
            fputc(nul ? 0 : '\n', stdout);
        }
        if (p) CFRelease(p);
    }
    CFRelease(key);
    fflush(stdout);
    return (int)n;
}

int main(int argc, char **argv) {
    const char *dirs[64];
    int ndirs = 0, count = 0, nul = 0, live = 0;
    const char *name = 0;
    char query[8192] = "";
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "-onlyin") && i + 1 < argc) { if (ndirs < 64) dirs[ndirs++] = argv[++i]; continue; }
        if (!strcmp(a, "-name") && i + 1 < argc) { name = argv[++i]; continue; }
        if (!strcmp(a, "-count")) { count = 1; continue; }
        if (!strcmp(a, "-0")) { nul = 1; continue; }
        if (!strcmp(a, "-live")) { live = 1; continue; }
        if (!strcmp(a, "-literal") || !strcmp(a, "-interpret")) continue;   /* a sintaxe é uma só no mOS */
        if (!strcmp(a, "-s") && i + 1 < argc) { i++; continue; }            /* consultas salvas: não há */
        if (!strcmp(a, "-h") || !strcmp(a, "-help")) { usage(); return 0; }
        if (a[0] == '-' && a[1]) { fprintf(stderr, "mdfind: opção desconhecida %s\n", a); usage(); return 1; }
        size_t l = strlen(query);
        snprintf(query + l, sizeof query - l, "%s%s", l ? " " : "", a);
    }
    if (name) {
        char q[1024];
        quote(q, sizeof q, name);
        char tmp[sizeof query + 1100];
        if (query[0]) snprintf(tmp, sizeof tmp, "(%s) && kMDItemFSName == '*%s*'cd", query, q);
        else snprintf(tmp, sizeof tmp, "kMDItemFSName == '*%s*'cd", q);
        memcpy(query, tmp, sizeof query - 1);
        query[sizeof query - 1] = 0;
    }
    if (!query[0]) { usage(); return 1; }
    CFTypeRef qs = CFStringCreateWithCString(0, query, UTF8);
    CFTypeRef q = MDQueryCreate(0, qs, 0, 0);
    CFRelease(qs);
    if (!q) { fprintf(stderr, "mdfind: consulta inválida: %s\n", query); return 1; }
    if (ndirs) {
        const void *v[64];
        char abs[64][2048];
        for (int i = 0; i < ndirs; i++) {
            const char *d = dirs[i];
            if (d[0] != '/' && getcwd(abs[i], 1024)) {
                size_t l = strlen(abs[i]);
                snprintf(abs[i] + l, sizeof abs[i] - l, "/%s", d);
                d = abs[i];
            }
            v[i] = CFStringCreateWithCString(0, d, UTF8);
        }
        CFTypeRef arr = CFArrayCreate(0, v, ndirs, 0);
        MDQuerySetSearchScope(q, arr, 0);
        CFRelease(arr);
        for (int i = 0; i < ndirs; i++) CFRelease(v[i]);
    }
    if (!MDQueryExecute(q, kMDQuerySynchronous)) { fprintf(stderr, "mdfind: a busca falhou\n"); return 1; }
    if (count) { printf("%ld\n", (long)MDQueryGetResultCount(q)); if (!live) return 0; }
    else print_results(q, nul, live);
    if (!live) return 0;
    /* -live: refaz a coleta a cada segundo e mostra só os caminhos que ainda não saíram */
    for (;;) {
        sleep(1);
        MDQueryExecute(q, kMDQuerySynchronous);
        if (count) printf("%ld\n", (long)MDQueryGetResultCount(q));
        else print_results(q, nul, 1);
    }
}
