/*
 * mOS — /usr/sbin/pkgutil (Mach-O): consulta os recibos de pacotes instalados e abre pacotes, como o pkgutil(1).
 *   --pkgs[=regex]  --pkg-info <id>  --pkg-info-plist <id>  --files <id>  --export-plist <id>  --forget <id>
 *   --expand <pacote> <pasta>  --payload-files <pacote>  --check-signature <pacote>  --file-info <caminho>
 *   --volume <caminho>, --only-files, --only-dirs
 * Os recibos ficam em <volume>/var/db/receipts/<id>.plist e .bom (gravados por /usr/sbin/installer).
 */
#include "../pkg/pkgengine.c"
#include <regex.h>

static const char *vol = "/";
static int only_files, only_dirs;

static void usage(void) {
    fprintf(stderr, "uso: pkgutil [--volume caminho] --pkgs[=regex] | --pkg-info id | --pkg-info-plist id | --files id |\n"
                    "              --export-plist id | --forget id | --expand pacote pasta | --payload-files pacote |\n"
                    "              --check-signature pacote | --file-info caminho\n");
}
static int bom_files(const char *id, strv_t *out) {
    char p[1300], dir[1024];
    u64 n;
    path_join(dir, sizeof dir, vol, "var/db/receipts");
    snprintf(p, sizeof p, "%s/%s.bom", dir, id);
    u8 *b = read_file(p, &n);
    if (!b) return -1;
    int r = bom_paths(b, n, out);
    free(b);
    return r;
}
static int is_dir_at(const receipt_t *r, const char *rel) {
    char a[2048], b[2048];
    path_join(a, sizeof a, vol, r->location);
    path_join(b, sizeof b, a, rel);
    struct stat st;
    return lstat(b, &st) == 0 && S_ISDIR(st.st_mode);
}
static int cmd_files(const char *id) {
    receipt_t r;
    if (receipt_read(vol, id, &r) < 0) { fprintf(stderr, "No receipt for '%s' found at '%s'.\n", id, vol); return 1; }
    strv_t f = {0};
    if (bom_files(id, &f) < 0) { fprintf(stderr, "pkgutil: o recibo de %s não tem Bom\n", id); return 1; }
    for (int i = 0; i < f.n; i++) {
        const char *s = f.v[i];
        while (s[0] == '.' && s[1] == '/') s += 2;
        if (!strcmp(s, ".") || !s[0]) continue;
        int d = is_dir_at(&r, s);
        if ((only_files && d) || (only_dirs && !d)) continue;
        printf("%s\n", s);
    }
    strv_free(&f);
    return 0;
}
static int cmd_info(const char *id, int plist) {
    receipt_t r;
    if (receipt_read(vol, id, &r) < 0) { fprintf(stderr, "No receipt for '%s' found at '%s'.\n", id, vol); return 1; }
    if (plist) {
        char dir[1024], p[1300];
        u64 n;
        path_join(dir, sizeof dir, vol, "var/db/receipts");
        snprintf(p, sizeof p, "%s/%s.plist", dir, id);
        u8 *d = read_file(p, &n);
        if (d) { fwrite(d, 1, (size_t)n, stdout); free(d); }
        return 0;
    }
    printf("package-id: %s\nversion: %s\nvolume: %s\nlocation: %s\ninstall-time: %ld\n", r.id, r.version, vol, r.location, r.date);
    return 0;
}
static int cmd_forget(const char *id) {
    receipt_t r;
    if (receipt_read(vol, id, &r) < 0) { fprintf(stderr, "No receipt for '%s' found at '%s'.\n", id, vol); return 1; }
    char dir[1024], p[1300];
    path_join(dir, sizeof dir, vol, "var/db/receipts");
    snprintf(p, sizeof p, "%s/%s.plist", dir, id); unlink(p);
    snprintf(p, sizeof p, "%s/%s.bom", dir, id); unlink(p);
    printf("Forgot package '%s' on '%s'.\n", id, vol);
    return 0;
}
/* --expand: o conteúdo do xar como pastas (Payload e Scripts ficam como arquivos, como no macOS) */
static int cmd_expand(const char *pkgp, const char *out) {
    xar_t x;
    if (xar_open(&x, pkgp) < 0) { fprintf(stderr, "pkgutil: %s\n", pk_err); return 1; }
    struct stat st;
    if (stat(out, &st) == 0) { fprintf(stderr, "pkgutil: %s já existe\n", out); xar_close(&x); return 1; }
    if (mkdirs(out, 0755) < 0) { fprintf(stderr, "pkgutil: não foi possível criar %s\n", out); xar_close(&x); return 1; }
    for (int i = 0; i < x.ne; i++) {
        if (!rel_ok(x.e[i].path)) continue;
        char p[2048];
        path_join(p, sizeof p, out, x.e[i].path);
        if (x.e[i].dir) { mkdirs(p, 0755); continue; }
        u64 n;
        u8 *d = xar_read(&x, &x.e[i], &n);
        if (!d) { fprintf(stderr, "pkgutil: %s\n", pk_err); xar_close(&x); return 1; }
        char par[2048];
        snprintf(par, sizeof par, "%s", p);
        char *sl = strrchr(par, '/');
        if (sl) { *sl = 0; mkdirs(par, 0755); }
        write_all(p, d, n, x.e[i].mode ? x.e[i].mode & 0777 : 0644);
        free(d);
    }
    xar_close(&x);
    return 0;
}
static int list_cb(void *ctx, const cpio_ent_t *e) {
    (void)ctx;
    const char *s = e->name;
    if (!strcmp(s, ".")) { printf(".\n"); return 0; }
    printf("%s%s\n", s[0] == '.' ? "" : "./", s);
    return 0;
}
static int cmd_payload_files(const char *pkgp) {
    pkg_t p;
    if (pkg_open(&p, pkgp) < 0) { fprintf(stderr, "pkgutil: %s\n", pk_err); return 1; }
    for (int i = 0; i < p.nc; i++) {
        char m[700];
        snprintf(m, sizeof m, "%s%sPayload", p.c[i].dir, p.c[i].dir[0] ? "/" : "");
        xent_t *e = xar_find(&p.x, m);
        if (!e) continue;
        u64 n, cn;
        u8 *raw = xar_read(&p.x, e, &n), *cp = raw ? unwrap(raw, n, &cn) : 0;
        free(raw);
        if (!cp) { fprintf(stderr, "pkgutil: não foi possível ler o Payload de %s\n", p.c[i].id); pkg_close(&p); return 1; }
        cpio_each(cp, cn, list_cb, 0);
        free(cp);
    }
    pkg_close(&p);
    return 0;
}
static int cmd_check_signature(const char *pkgp) {
    xar_t x;
    if (xar_open(&x, pkgp) < 0) { fprintf(stderr, "pkgutil: %s\n", pk_err); return 1; }
    printf("Package \"%s\":\n", strrchr(pkgp, '/') ? strrchr(pkgp, '/') + 1 : pkgp);
    if (!x.signed_) printf("   Status: no signature\n");
    else printf("   Status: signed (%d certificado(s) no pacote; a cadeia não é conferida pelo mOS)\n", x.ncerts);
    int r = x.signed_ ? 0 : 1;
    xar_close(&x);
    return r;
}
static int cmd_file_info(const char *path) {
    strv_t ids = {0};
    receipts_list(vol, &ids);
    const char *rel = path;
    while (*rel == '/') rel++;
    int found = 0;
    for (int i = 0; i < ids.n; i++) {
        receipt_t r;
        strv_t f = {0};
        if (receipt_read(vol, ids.v[i], &r) < 0 || bom_files(ids.v[i], &f) < 0) continue;
        char pre[1024];
        snprintf(pre, sizeof pre, "%s", r.location);
        for (int k = 0; k < f.n; k++) {
            const char *s = f.v[k];
            while (s[0] == '.' && s[1] == '/') s += 2;
            char full[2048];
            path_join(full, sizeof full, pre[0] ? pre : "", s);
            const char *fp = full;
            while (*fp == '/') fp++;
            if (strcmp(fp, rel)) continue;
            if (!found) printf("volume: %s\npath: %s\n\n", vol, rel);
            printf("pkgid: %s\npkg-version: %s\ninstall-time: %ld\n\n", r.id, r.version, r.date);
            found = 1;
            break;
        }
        strv_free(&f);
    }
    strv_free(&ids);
    if (!found) printf("volume: %s\npath: %s\n", vol, rel);
    return 0;
}
int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "--volume") && i + 1 < argc) { vol = argv[++i]; continue; }
        if (!strcmp(a, "--only-files")) { only_files = 1; continue; }
        if (!strcmp(a, "--only-dirs")) { only_dirs = 1; continue; }
        if (!strncmp(a, "--pkgs", 6) || !strcmp(a, "--packages")) {
            const char *re = a[6] == '=' ? a + 7 : 0;
            regex_t rx;
            if (re && regcomp(&rx, re, REG_EXTENDED | REG_NOSUB)) { fprintf(stderr, "pkgutil: regex inválida\n"); return 1; }
            strv_t ids = {0};
            receipts_list(vol, &ids);
            for (int k = 0; k < ids.n; k++) if (!re || !regexec(&rx, ids.v[k], 0, 0, 0)) printf("%s\n", ids.v[k]);
            if (re) regfree(&rx);
            strv_free(&ids);
            return 0;
        }
        if (i + 1 >= argc) { usage(); return 1; }
        const char *v = argv[i + 1];
        if (!strcmp(a, "--pkg-info")) return cmd_info(v, 0);
        if (!strcmp(a, "--pkg-info-plist") || !strcmp(a, "--export-plist")) return cmd_info(v, 1);
        if (!strcmp(a, "--files") || !strcmp(a, "--lsbom")) return cmd_files(v);
        if (!strcmp(a, "--forget")) return cmd_forget(v);
        if (!strcmp(a, "--payload-files")) return cmd_payload_files(v);
        if (!strcmp(a, "--check-signature")) return cmd_check_signature(v);
        if (!strcmp(a, "--file-info")) return cmd_file_info(v);
        if (!strcmp(a, "--expand") || !strcmp(a, "--expand-full")) { if (i + 2 >= argc) { usage(); return 1; } return cmd_expand(v, argv[i + 2]); }
        usage();
        return 1;
    }
    usage();
    return 1;
}
