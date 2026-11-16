/* mOS — lsregister: o banco do Launch Services, como o lsregister do macOS
 * (LaunchServices.framework/Support/lsregister; no mOS em /usr/bin).
 *   lsregister [-v] [-f] [-R] caminho...   registra (ou relê) os .app; -R entra nas pastas
 *   lsregister -u caminho...               desregistra
 *   lsregister -dump                       todos os apps: identificador, caminho, esquemas e tipos que abrem
 *   lsregister -kill | -r | -seed          relê as pastas de apps (/Aplicativos e as do sistema)
 * O banco fica no kernel (kernel/compat/launchsvc.c); a ponte é a SPI cx_ls_raw do libSystem (openurl.c).
 * Teste de host: tools/lsregistertest.sh. */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

long cx_ls_raw(int op, const char *key, unsigned roles, char *out, size_t cap);
enum { LS_APP_BY_PATH = 16, LS_DUMP = 19, LS_UNREGISTER = 20, LS_RESCAN = 21 };   /* MOS_LS_* (kernel/include/syscall.h) */

static int verbose;
static char out[8192];

static int usage(void) {
    fprintf(stderr, "usage: lsregister [-v] [-f] [-R] [-u] [-dump] [-kill] [-r] [-seed] [path ...]\n");
    return 1;
}
static int is_app(const char *p) { size_t n = strlen(p); return n > 4 && !strcmp(p + n - 4, ".app"); }
static void trim_slash(char *p) { size_t n = strlen(p); while (n > 1 && p[n - 1] == '/') p[--n] = 0; }

static int reg_one(const char *path) {
    long r = cx_ls_raw(LS_APP_BY_PATH, path, 0, out, sizeof out);
    if (r < 0) { fprintf(stderr, "lsregister: %s: não é um bundle de app (falta Contents/Info.plist)\n", path); return 1; }
    if (verbose) {
        char *t1 = strchr(out, '\t');
        if (t1) *t1 = 0;
        printf("registrado: %s (%s)\n", path, out);
    }
    return 0;
}
static int reg_tree(const char *dir, int depth) {
    if (is_app(dir)) return reg_one(dir);
    if (depth > 8) return 0;
    DIR *d = opendir(dir);
    if (!d) return 0;
    int bad = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char p[2048];
        snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
        struct stat st;
        if (stat(p, &st) || !S_ISDIR(st.st_mode)) continue;
        bad |= reg_tree(p, depth + 1);
    }
    closedir(d);
    return bad;
}

int main(int argc, char **argv) {
    int recursive = 0, unreg = 0, did = 0, rc = 0;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "-v")) verbose = 1;
        else if (!strcmp(a, "-f") || !strcmp(a, "-lazy") || !strcmp(a, "-trusted")) ;   /* sempre relê; não há "confiável" à parte */
        else if (!strcmp(a, "-R")) recursive = 1;
        else if (!strcmp(a, "-u")) unreg = 1;
        else if (!strcmp(a, "-dump") || !strcmp(a, "-dump-apps")) {
            long total = cx_ls_raw(LS_DUMP, "", 0, out, sizeof out);
            if (total < 0) { fprintf(stderr, "lsregister: o banco do Launch Services não respondeu\n"); return 1; }
            for (long k = 0; k < total; k++) {
                if (cx_ls_raw(LS_DUMP, "", (unsigned)k, out, sizeof out) < 0 || !out[0]) continue;
                printf("--------------------------------------------------------------------------------\n%s", out);
            }
            printf("--------------------------------------------------------------------------------\n%ld apps\n", total);
            did = 1;
        } else if (!strcmp(a, "-kill") || !strcmp(a, "-r") || !strcmp(a, "-seed") || !strcmp(a, "-delete")) {
            long n = cx_ls_raw(LS_RESCAN, "", 0, out, sizeof out);
            if (n < 0) { fprintf(stderr, "lsregister: falha ao reler as pastas de apps\n"); return 1; }
            if (verbose) printf("%ld apps registrados\n", n);
            did = 1;
        } else if (!strcmp(a, "-domain") || !strcmp(a, "-apps") || !strcmp(a, "-libs")) { if (i + 1 < argc && argv[i + 1][0] != '-') i++; }
        else if (a[0] == '-') return usage();
        else {
            char p[2048];
            if (a[0] == '/') snprintf(p, sizeof p, "%s", a);
            else {
                char cwd[1024];
                if (!getcwd(cwd, sizeof cwd)) cwd[0] = 0;
                snprintf(p, sizeof p, "%s/%s", cwd, a);
            }
            trim_slash(p);
            struct stat st;
            did = 1;
            if (stat(p, &st)) { fprintf(stderr, "lsregister: %s: No such file or directory\n", a); rc = 1; continue; }
            if (unreg) {
                if (cx_ls_raw(LS_UNREGISTER, p, 0, out, sizeof out) < 0) { fprintf(stderr, "lsregister: %s: não estava registrado\n", a); rc = 1; }
                else if (verbose) printf("desregistrado: %s\n", p);
            } else if (recursive && !is_app(p)) rc |= reg_tree(p, 0);
            else rc |= reg_one(p);
        }
    }
    if (!did) return usage();
    return rc;
}
