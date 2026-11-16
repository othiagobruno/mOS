/* mOS — mdls(1): atributos de metadados de um arquivo pela API MDItem do libSystem (userland/libsystem/mdquery.c).
 *   mdls [-name atributo]... [-raw [-nullMarker texto]] arquivo...
 * Saída no formato do macOS: "kMDItemFSName = "a.txt"" com os nomes alinhados, listas entre parênteses, datas
 * "AAAA-MM-DD hh:mm:ss +0000". Atributo sem valor: "(null)". -raw: só os valores, separados por NUL.
 * Arquivo inexistente: "<arq>: could not find <arq>." e código 1. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef const void *CFTypeRef;
typedef long CFIndex;
typedef unsigned long CFTypeID;
CFTypeRef CFStringCreateWithCString(CFTypeRef alloc, const char *s, unsigned enc);
void CFRelease(CFTypeRef o);
unsigned char CFStringGetCString(CFTypeRef s, char *buf, CFIndex n, unsigned enc);
CFTypeID CFGetTypeID(CFTypeRef o);
CFTypeID CFStringGetTypeID(void);
CFTypeID CFNumberGetTypeID(void);
CFTypeID CFArrayGetTypeID(void);
CFTypeID CFDateGetTypeID(void);
CFTypeID CFBooleanGetTypeID(void);
CFIndex CFArrayGetCount(CFTypeRef a);
CFTypeRef CFArrayGetValueAtIndex(CFTypeRef a, CFIndex i);
unsigned char CFNumberIsFloatType(CFTypeRef n);
unsigned char CFNumberGetValue(CFTypeRef n, long type, void *out);
double CFDateGetAbsoluteTime(CFTypeRef d);
unsigned char CFBooleanGetValue(CFTypeRef b);
CFTypeRef MDItemCreate(CFTypeRef alloc, CFTypeRef path);
CFTypeRef MDItemCopyAttribute(CFTypeRef item, CFTypeRef name);
CFTypeRef MDItemCopyAttributeNames(CFTypeRef item);
#define UTF8 0x08000100u
enum { kCFNumberSInt64Type = 4, kCFNumberFloat64Type = 6 };

static void put_value(CFTypeRef v, int raw, int indent) {
    char buf[8192];
    CFTypeID t = CFGetTypeID(v);
    if (t == CFStringGetTypeID()) {
        CFStringGetCString(v, buf, sizeof buf, UTF8);
        printf(raw && !indent ? "%s" : "\"%s\"", buf);
    } else if (t == CFNumberGetTypeID()) {
        if (CFNumberIsFloatType(v)) { double d = 0; CFNumberGetValue(v, kCFNumberFloat64Type, &d); printf("%g", d); }
        else { long long n = 0; CFNumberGetValue(v, kCFNumberSInt64Type, &n); printf("%lld", n); }
    } else if (t == CFBooleanGetTypeID()) {
        printf("%d", CFBooleanGetValue(v) ? 1 : 0);
    } else if (t == CFDateGetTypeID()) {
        time_t s = (time_t)(CFDateGetAbsoluteTime(v) + 978307200.0);
        struct tm tm;
        gmtime_r(&s, &tm);
        strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S +0000", &tm);
        printf("%s", buf);
    } else if (t == CFArrayGetTypeID()) {
        CFIndex n = CFArrayGetCount(v);
        printf("(\n");
        for (CFIndex i = 0; i < n; i++) {
            printf("    ");
            put_value(CFArrayGetValueAtIndex(v, i), 0, 1);
            printf(i + 1 < n ? ",\n" : "\n");
        }
        printf(")");
    } else printf("(desconhecido)");
}

static int name_cmp(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

int main(int argc, char **argv) {
    const char *names[128];
    int nnames = 0, raw = 0, files = 0, rc = 0;
    const char *null_marker = "(null)";
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-name") && i + 1 < argc) { if (nnames < 128) names[nnames++] = argv[++i]; continue; }
        if (!strcmp(argv[i], "-raw") || !strcmp(argv[i], "-r")) { raw = 1; continue; }
        if (!strcmp(argv[i], "-nullMarker") && i + 1 < argc) { null_marker = argv[++i]; continue; }
        if (!strcmp(argv[i], "-plist") && i + 1 < argc) { i++; continue; }   /* exportar plist: não há */
        files++;
        char full[2048];
        const char *path = argv[i];
        if (path[0] != '/' && getcwd(full, 1024)) {
            size_t l = strlen(full);
            snprintf(full + l, sizeof full - l, "/%s", path);
            path = full;
        }
        CFTypeRef ps = CFStringCreateWithCString(0, path, UTF8);
        CFTypeRef item = MDItemCreate(0, ps);
        CFRelease(ps);
        if (!item) { fprintf(stderr, "%s: could not find %s.\n", argv[i], argv[i]); rc = 1; continue; }
        /* os nomes: os pedidos com -name, ou todos os do item em ordem alfabética */
        char *list[256];
        int nlist = 0;
        if (nnames) for (int k = 0; k < nnames; k++) list[nlist++] = strdup(names[k]);
        else {
            CFTypeRef all = MDItemCopyAttributeNames(item);
            CFIndex n = all ? CFArrayGetCount(all) : 0;
            for (CFIndex k = 0; k < n && nlist < 256; k++) {
                char b[256];
                if (CFStringGetCString(CFArrayGetValueAtIndex(all, k), b, sizeof b, UTF8)) list[nlist++] = strdup(b);
            }
            if (all) CFRelease(all);
            qsort(list, (size_t)nlist, sizeof *list, name_cmp);
        }
        int w = 0;
        for (int k = 0; k < nlist; k++) if ((int)strlen(list[k]) > w) w = (int)strlen(list[k]);
        for (int k = 0; k < nlist; k++) {
            CFTypeRef key = CFStringCreateWithCString(0, list[k], UTF8);
            CFTypeRef v = MDItemCopyAttribute(item, key);
            CFRelease(key);
            if (raw) {
                if (v) put_value(v, 1, 0); else fputs(null_marker, stdout);
                fputc(0, stdout);
            } else {
                printf("%-*s = ", w, list[k]);
                if (v) put_value(v, 0, 0); else fputs("(null)", stdout);
                fputc('\n', stdout);
            }
            if (v) CFRelease(v);
            free(list[k]);
        }
        CFRelease(item);
    }
    if (!files) { fprintf(stderr, "uso: mdls [-name atributo] [-raw [-nullMarker texto]] arquivo ...\n"); return 1; }
    return rc;
}
