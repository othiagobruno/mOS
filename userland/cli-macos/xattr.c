/*
 * mOS — /usr/bin/xattr (Mach-O): lê, grava e apaga atributos estendidos, como o xattr(1) do macOS. É o que o Homebrew,
 * os instaladores e o usuário usam para tirar a quarentena (xattr -d com.apple.quarantine App.app).
 *   xattr [-lrsvx] arquivo...                 lista os nomes (-l: com os valores)
 *   xattr -p [-lrsvx] nome arquivo...         mostra o valor
 *   xattr -w [-rsx] nome valor arquivo...     grava (-x: o valor em hexadecimal)
 *   xattr -d [-rsv] nome arquivo...           apaga
 *   xattr -c [-rsv] arquivo...                apaga todos
 * -r recursivo, -s age no próprio link simbólico, -v mostra o arquivo antes, -x hexadecimal.
 */
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <stdint.h>
/* <sys/xattr.h> do macOS (o SDK do zig não traz) */
#define XATTR_NOFOLLOW 0x0001
#ifndef ENOATTR
#define ENOATTR 93
#endif
ssize_t getxattr(const char *path, const char *name, void *value, size_t size, uint32_t position, int options);
int setxattr(const char *path, const char *name, const void *value, size_t size, uint32_t position, int options);
int removexattr(const char *path, const char *name, int options);
ssize_t listxattr(const char *path, char *namebuf, size_t size, int options);

static int opt_l, opt_r, opt_s, opt_v, opt_x, many, err;
static char mode = 0;
static const char *aname, *avalue;

static int printable(const unsigned char *b, ssize_t n) {
    for (ssize_t i = 0; i < n; i++) if ((b[i] < 32 && b[i] != '\n' && b[i] != '\t') || b[i] == 127) return 0;
    return 1;
}
static void hexdump(const unsigned char *b, ssize_t n) {
    for (ssize_t i = 0; i < n; i += 16) {
        printf("%08zx ", (size_t)i);
        for (ssize_t k = i; k < i + 16; k++) { if (k < n) printf(" %02X", b[k]); else printf("   "); }
        printf("  |");
        for (ssize_t k = i; k < i + 16 && k < n; k++) putchar(b[k] >= 32 && b[k] < 127 ? b[k] : '.');
        printf("|\n");
    }
}
static void show_value(const unsigned char *b, ssize_t n, int inline_) {
    if (opt_x || !printable(b, n)) {
        if (inline_) putchar('\n');
        hexdump(b, n);
        return;
    }
    fwrite(b, 1, (size_t)n, stdout);
    putchar('\n');
}
static int get(const char *f, const char *nm, unsigned char **out) {
    int o = opt_s ? XATTR_NOFOLLOW : 0;
    ssize_t n = getxattr(f, nm, 0, 0, 0, o);
    if (n < 0) return -1;
    *out = malloc((size_t)n + 1);
    if (!*out) return -1;
    n = getxattr(f, nm, *out, (size_t)n, 0, o);
    return (int)n;
}
static int hexparse(const char *s, unsigned char **out) {
    size_t cap = strlen(s) / 2 + 1, n = 0;
    unsigned char *b = malloc(cap);
    int hi = -1;
    for (; *s; s++) {
        int v = *s >= '0' && *s <= '9' ? *s - '0' : *s >= 'a' && *s <= 'f' ? *s - 'a' + 10 : *s >= 'A' && *s <= 'F' ? *s - 'A' + 10 : -1;
        if (v < 0) { if (*s == ' ' || *s == '\n' || *s == '\t') continue; free(b); return -1; }
        if (hi < 0) hi = v; else { b[n++] = (unsigned char)(hi << 4 | v); hi = -1; }
    }
    if (hi >= 0) { free(b); return -1; }
    *out = b;
    return (int)n;
}

static void one(const char *f) {
    int o = opt_s ? XATTR_NOFOLLOW : 0;
    const char *pre = many || opt_r ? f : 0;
    if (mode == 'w') {
        unsigned char *v = (unsigned char *)avalue;
        int n = (int)strlen(avalue);
        if (opt_x && (n = hexparse(avalue, &v)) < 0) { fprintf(stderr, "xattr: valor hexadecimal inválido\n"); err = 1; return; }
        if (setxattr(f, aname, v, (size_t)n, 0, o) < 0) { fprintf(stderr, "xattr: [Errno %d] %s: '%s'\n", errno, strerror(errno), f); err = 1; }
        if (opt_x) free(v);
        return;
    }
    if (mode == 'd') {
        if (opt_v) printf("%s\n", f);
        if (removexattr(f, aname, o) < 0) {
            if (errno == ENOATTR) fprintf(stderr, "xattr: %s: No such xattr: %s\n", f, aname);
            else fprintf(stderr, "xattr: [Errno %d] %s: '%s'\n", errno, strerror(errno), f);
            err = 1;
        }
        return;
    }
    ssize_t ln = listxattr(f, 0, 0, o);
    if (ln < 0) { fprintf(stderr, "xattr: %s: %s\n", f, strerror(errno)); err = 1; return; }
    char *names = malloc((size_t)ln + 1);
    ln = listxattr(f, names, (size_t)ln, o);
    if (mode == 'c') {
        for (ssize_t i = 0; i < ln; i += (ssize_t)strlen(names + i) + 1) removexattr(f, names + i, o);
        free(names);
        return;
    }
    if (mode == 'p') {
        unsigned char *v;
        int n = get(f, aname, &v);
        if (n < 0) { fprintf(stderr, "xattr: %s: No such xattr: %s\n", f, aname); err = 1; free(names); return; }
        if (pre) printf("%s: ", pre);
        show_value(v, n, pre != 0);
        free(v);
        free(names);
        return;
    }
    for (ssize_t i = 0; i < ln; i += (ssize_t)strlen(names + i) + 1) {
        if (pre) printf("%s: ", pre);
        if (!opt_l) { printf("%s\n", names + i); continue; }
        unsigned char *v;
        int n = get(f, names + i, &v);
        printf("%s:", names + i);
        if (n >= 0) {
            if (opt_x || !printable(v, n)) { putchar('\n'); hexdump(v, n); }
            else { putchar(' '); fwrite(v, 1, (size_t)n, stdout); putchar('\n'); }
            free(v);
        } else putchar('\n');
    }
    free(names);
}
static void walk(const char *f, int depth) {
    one(f);
    if (!opt_r || depth > 64) return;
    struct stat st;
    if (lstat(f, &st) < 0 || !S_ISDIR(st.st_mode)) return;
    DIR *d = opendir(f);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char p[2048];
        snprintf(p, sizeof p, "%s/%s", f, e->d_name);
        walk(p, depth + 1);
    }
    closedir(d);
}
static void usage(void) {
    fprintf(stderr, "uso: xattr [-lrsvx] arquivo [arquivo ...]\n"
                    "     xattr -p [-lrsvx] nome arquivo [arquivo ...]\n"
                    "     xattr -w [-rsx] nome valor arquivo [arquivo ...]\n"
                    "     xattr -d [-rsv] nome arquivo [arquivo ...]\n"
                    "     xattr -c [-rsv] arquivo [arquivo ...]\n");
}
int main(int argc, char **argv) {
    int i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        if (!strcmp(argv[i], "--")) { i++; break; }
        for (const char *c = argv[i] + 1; *c; c++) switch (*c) {
            case 'l': opt_l = 1; break; case 'r': opt_r = 1; break; case 's': opt_s = 1; break;
            case 'v': opt_v = 1; break; case 'x': opt_x = 1; break;
            case 'p': case 'w': case 'd': case 'c': mode = *c; break;
            case 'h': usage(); return 0;
            default: usage(); return 2;
        }
    }
    if (mode == 'p' || mode == 'w' || mode == 'd') { if (i >= argc) { usage(); return 2; } aname = argv[i++]; }
    if (mode == 'w') { if (i >= argc) { usage(); return 2; } avalue = argv[i++]; }
    if (i >= argc) { usage(); return 2; }
    many = argc - i > 1;
    for (; i < argc; i++) walk(argv[i], 0);
    return err;
}
