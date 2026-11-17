/*
 * mOS — /usr/bin/hdiutil (Mach-O): imagens de disco como o hdiutil(1) do macOS.
 *   attach <imagem> [-nobrowse] [-readonly] [-noverify] [-noautoopen] [-mountpoint p] [-mountrandom p] [-quiet] [-plist]
 *   detach <ponto de montagem | /dev/diskN[sM]> [-force] [-quiet]
 *   info [-plist]
 *   imageinfo <imagem> [-plist] [-format]
 *   verify <imagem>
 *   create <saída> -size N[b|k|m|g|t] [-fs HFS+|JHFS+|HFSX|JHFSX|MS-DOS|FAT32] [-volname nome]
 *          [-format UDRW|UDRO|UDZO|ULFO|ULMO|UDTO] [-layout GPTSPUD|NONE] [-srcfolder pasta] [-ov]
 *          (-srcfolder: UDRW temporária montada gravável pelo kernel, recebe a pasta e vira o formato pedido; padrão UDZO)
 *   convert <imagem> -format UDRW|UDRO|UDZO|ULFO|ULMO -o <saída> [-ov]
 * attach/detach/info pelo kernel (SYS_IMGATTACH/SYS_IMGDETACH, getfsstat); o resto no próprio processo
 * (userland/pkg/udif.c e volfmt.c). Discos: /dev/disk0s1 é o sistema; os de /Volumes são disk<N>s1 (N >= 2),
 * na ordem do getfsstat, os mesmos nomes do DiskArbitration do libSystem.
 */
#include "../pkg/udif.c"
#include "../pkg/volfmt.c"
#ifdef __APPLE__
#include <sys/mount.h>
/* <sys/xattr.h> não está no include dos programas do cli-macos: as assinaturas do Darwin, exportadas pelo libSystem */
ssize_t listxattr(const char *path, char *namebuf, size_t size, int options);
ssize_t getxattr(const char *path, const char *name, void *value, size_t size, unsigned int position, int options);
int setxattr(const char *path, const char *name, const void *value, size_t size, unsigned int position, int options);
#define XATTR_NOFOLLOW 0x0001
#endif

static int quiet, plist;

#if defined(__APPLE__) && defined(__x86_64__)
static long sys3(long n, long a, long b, long c) {
    long r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "D"(a), "S"(b), "d"(c) : "rcx", "r11", "memory");
    return r;
}
#else
static long sys3(long n, long a, long b, long c) { (void)n; (void)a; (void)b; (void)c; return -78; }
#endif

static u64 parse_size(const char *s) {
    char *e;
    double v = strtod(s, &e);
    u64 m = 1;
    switch (*e) { case 'b': case 'B': m = 512; break; case 'k': case 'K': m = 1024; break; case 'm': case 'M': m = 1 << 20; break;
                  case 'g': case 'G': m = 1ull << 30; break; case 't': case 'T': m = 1ull << 40; break; default: m = 512; }
    if (!*e) m = 512;                                     /* sem sufixo: setores, como o hdiutil */
    return (u64)(v * (double)m);
}
static u32 fmt_type(const char *f) {
    if (!strcmp(f, "UDZO")) return U_ZLIB;
    if (!strcmp(f, "ULFO")) return U_LZFSE;
    if (!strcmp(f, "ULMO")) return U_LZMA;
    if (!strcmp(f, "UDRW") || !strcmp(f, "UDRO")) return U_RAW;
    if (!strcmp(f, "UDTO")) return 0xFFFF;                /* imagem crua ("DVD/CD master", .cdr) */
    return 0;
}

/* ---- volumes montados (getfsstat): disco N >= 2 pela ordem */
typedef struct { char mnt[1024], from[1024], kind[16]; int unit; } vol_t;
static int volumes(vol_t *v, int max) {
#ifndef __APPLE__
    (void)v; (void)max;
    return 0;                                             /* teste de host: sem volumes montados */
#else
    int n = getfsstat(0, 0, MNT_NOWAIT);
    if (n <= 0) return 0;
    struct statfs *sf = calloc((size_t)n, sizeof *sf);
    n = getfsstat(sf, n * (int)sizeof *sf, MNT_NOWAIT);
    int k = 0, unit = 2;
    for (int i = 0; i < n && k < max; i++) {
        snprintf(v[k].mnt, sizeof v[k].mnt, "%s", sf[i].f_mntonname);
        snprintf(v[k].from, sizeof v[k].from, "%s", sf[i].f_mntfromname);
        snprintf(v[k].kind, sizeof v[k].kind, "%s", sf[i].f_fstypename);
        v[k].unit = strcmp(sf[i].f_mntonname, "/") ? unit++ : 0;
        k++;
    }
    free(sf);
    return k;
#endif
}
static const char *hint_of(const char *kind) { return !strcmp(kind, "msdos") ? "Microsoft Basic Data" : !strcmp(kind, "apfs") ? "Apple_APFS" : "Apple_HFS"; }

static int cmd_attach(int argc, char **argv) {
    const char *img = 0;
    int ro = 0;
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "-plist")) plist = 1;
        else if (!strcmp(argv[i], "-readonly")) ro = 1;
        else if (!strcmp(argv[i], "-readwrite")) ro = 0;
        else if (!strcmp(argv[i], "-quiet")) quiet = 1;
        else if (!strcmp(argv[i], "-mountpoint") || !strcmp(argv[i], "-mountroot") || !strcmp(argv[i], "-mountrandom") || !strcmp(argv[i], "-shadow")) i++;
        else if (argv[i][0] != '-') img = argv[i];
    }
    if (!img) { fprintf(stderr, "hdiutil: attach: falta a imagem\n"); return 1; }
    char abs[1024];
    if (!realpath(img, abs)) { fprintf(stderr, "hdiutil: attach failed - No such file or directory\n"); return 1; }
    char out[1024] = "";
    long n = sys3(54, (long)abs, (long)out, (long)(sizeof out | (ro ? 1ull << 62 : 0)));   /* UDRW/crua: leitura e escrita */
    if (n <= 0) {
        vol_t v[32];                                      /* já montada: o hdiutil devolve o mesmo volume */
        int k = volumes(v, 32), found = 0;
        for (int i = 0; i < k; i++) if (!strcmp(v[i].from, abs)) { snprintf(out + strlen(out), sizeof out - strlen(out), "%s\n", v[i].mnt); found = 1; }
        if (!found) { fprintf(stderr, "hdiutil: attach failed - not recognized\n"); return 1; }
    }
    vol_t v[32];
    int k = volumes(v, 32);
    if (plist) printf("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
                      "<plist version=\"1.0\">\n<dict>\n\t<key>system-entities</key>\n\t<array>\n");
    for (char *line = strtok(out, "\n"); line; line = strtok(0, "\n")) {
        int unit = 2;
        const char *kind = "hfs";
        for (int i = 0; i < k; i++) if (!strcmp(v[i].mnt, line)) { unit = v[i].unit; kind = v[i].kind; }
        if (plist) {
            printf("\t\t<dict>\n\t\t\t<key>content-hint</key>\n\t\t\t<string>GUID_partition_scheme</string>\n\t\t\t<key>dev-entry</key>\n"
                   "\t\t\t<string>/dev/disk%d</string>\n\t\t\t<key>potentially-mountable</key>\n\t\t\t<false/>\n\t\t</dict>\n", unit);
            printf("\t\t<dict>\n\t\t\t<key>content-hint</key>\n\t\t\t<string>%s</string>\n\t\t\t<key>dev-entry</key>\n\t\t\t<string>/dev/disk%ds1</string>\n"
                   "\t\t\t<key>mount-point</key>\n\t\t\t<string>%s</string>\n\t\t\t<key>potentially-mountable</key>\n\t\t\t<true/>\n"
                   "\t\t\t<key>volume-kind</key>\n\t\t\t<string>%s</string>\n\t\t</dict>\n", hint_of(kind), unit, line, kind);
        } else if (!quiet) {
            printf("/dev/disk%-11d\tGUID_partition_scheme          \t\n", unit);
            printf("/dev/disk%ds1       \t%-31s\t%s\n", unit, hint_of(kind), line);
        }
    }
    if (plist) printf("\t</array>\n</dict>\n</plist>\n");
    return 0;
}
static int cmd_detach(int argc, char **argv) {
    const char *t = 0;
    for (int i = 0; i < argc; i++) { if (!strcmp(argv[i], "-quiet")) quiet = 1; else if (argv[i][0] != '-') t = argv[i]; }
    if (!t) { fprintf(stderr, "hdiutil: detach: falta o dispositivo\n"); return 1; }
    char mnt[1024];
    snprintf(mnt, sizeof mnt, "%s", t);
    if (!strncmp(t, "/dev/", 5)) {
        const char *d = t + 5;
        if (*d == 'r') d++;
        int unit = atoi(d + 4);
        vol_t v[32];
        int k = volumes(v, 32), found = 0;
        for (int i = 0; i < k; i++) if (v[i].unit == unit && unit) { snprintf(mnt, sizeof mnt, "%s", v[i].mnt); found = 1; }
        if (!found) { fprintf(stderr, "hdiutil: detach failed - No such file or directory\n"); return 1; }
    }
    size_t l = strlen(mnt);
    if (l > 1 && mnt[l - 1] == '/') mnt[l - 1] = 0;
    if (sys3(55, (long)mnt, 0, 0) < 0) { fprintf(stderr, "hdiutil: detach failed - Resource busy\n"); return 1; }
    if (!quiet) printf("\"%s\" ejected.\n", strrchr(mnt, '/') ? strrchr(mnt, '/') + 1 : mnt);
    return 0;
}
static int cmd_info(int argc, char **argv) {
    for (int i = 0; i < argc; i++) if (!strcmp(argv[i], "-plist")) plist = 1;
    vol_t v[32];
    int k = volumes(v, 32);
    if (plist) printf("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<plist version=\"1.0\">\n<dict>\n\t<key>images</key>\n\t<array>\n");
    else printf("framework       : mOS\ndriver          : mOS\n");
    for (int i = 0; i < k; i++) {
        if (!v[i].unit || v[i].from[0] != '/' || !strncmp(v[i].from, "/dev/", 5)) continue;   /* só imagens */
        if (plist) printf("\t\t<dict>\n\t\t\t<key>image-path</key>\n\t\t\t<string>%s</string>\n\t\t\t<key>system-entities</key>\n\t\t\t<array>\n"
                          "\t\t\t\t<dict>\n\t\t\t\t\t<key>dev-entry</key>\n\t\t\t\t\t<string>/dev/disk%ds1</string>\n\t\t\t\t\t<key>mount-point</key>\n"
                          "\t\t\t\t\t<string>%s</string>\n\t\t\t\t</dict>\n\t\t\t</array>\n\t\t</dict>\n", v[i].from, v[i].unit, v[i].mnt);
        else printf("================================================\nimage-path      : %s\n/dev/disk%d\tGUID_partition_scheme\t\n/dev/disk%ds1\t%s\t%s\n",
                    v[i].from, v[i].unit, v[i].unit, hint_of(v[i].kind), v[i].mnt);
    }
    if (plist) printf("\t</array>\n</dict>\n</plist>\n");
    return 0;
}
static int cmd_imageinfo(int argc, char **argv) {
    const char *img = 0;
    int only_fmt = 0;
    for (int i = 0; i < argc; i++) { if (!strcmp(argv[i], "-plist")) plist = 1; else if (!strcmp(argv[i], "-format")) only_fmt = 1; else if (argv[i][0] != '-') img = argv[i]; }
    udif_t u;
    if (!img || udif_open(&u, img) < 0) { fprintf(stderr, "hdiutil: imageinfo failed - %s\n", pk_err[0] ? pk_err : "image not recognized"); return 1; }
    if (only_fmt) { printf("%s\n", u.format); udif_close(&u); return 0; }
    if (plist) {
        printf("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<plist version=\"1.0\">\n<dict>\n\t<key>Format</key>\n\t<string>%s</string>\n"
               "\t<key>Size Information</key>\n\t<dict>\n\t\t<key>Total Bytes</key>\n\t\t<integer>%llu</integer>\n\t\t<key>Compressed Bytes</key>\n"
               "\t\t<integer>%llu</integer>\n\t</dict>\n\t<key>partitions</key>\n\t<array>\n", u.format, (unsigned long long)(u.sectors * 512), (unsigned long long)u.dfork_len);
        for (int i = 0; i < u.nb; i++) printf("\t\t<dict>\n\t\t\t<key>partition-name</key>\n\t\t\t<string>%s</string>\n\t\t\t<key>partition-start</key>\n\t\t\t<integer>%llu</integer>\n"
                                               "\t\t\t<key>partition-length</key>\n\t\t\t<integer>%llu</integer>\n\t\t</dict>\n",
                                               u.b[i].name, (unsigned long long)u.b[i].first, (unsigned long long)u.b[i].count);
        printf("\t</array>\n</dict>\n</plist>\n");
    } else {
        printf("Format: %s\nChecksum Type: %s\nSize Information:\n\tTotal Bytes: %llu\n\tCompressed Bytes: %llu\n\tSector Count: %llu\npartitions:\n",
               u.format, u.data_crc_type == 2 ? "CRC32" : "none", (unsigned long long)(u.sectors * 512), (unsigned long long)u.dfork_len, (unsigned long long)u.sectors);
        for (int i = 0; i < u.nb; i++) printf("\t%d: %s, início %llu, %llu setores\n", i, u.b[i].name, (unsigned long long)u.b[i].first, (unsigned long long)u.b[i].count);
    }
    udif_close(&u);
    return 0;
}
static int cmd_verify(int argc, char **argv) {
    const char *img = argc ? argv[0] : 0;
    udif_t u;
    if (!img || udif_open(&u, img) < 0) { fprintf(stderr, "hdiutil: verify failed - %s\n", pk_err); return 1; }
    int ok = 1;
    u8 *d = udif_expand(&u, &ok);
    if (!d) { fprintf(stderr, "hdiutil: verify failed - %s\n", pk_err); udif_close(&u); return 1; }
    free(d);
    printf("%s\n", ok ? "hdiutil: verify: checksum of \"" : "hdiutil: verify: checksum of \"");
    printf("%s\" is %s\n", img, ok ? "VALID" : "INVALID");
    udif_close(&u);
    return ok ? 0 : 1;
}
/* imagem nova: disco formatado, com layout GPT (uma partição) ou o volume direto (NONE) */
static int write_raw(const char *out, const u8 *disk, u64 sectors) {
    return write_all(out, disk, sectors * 512, 0644) < 0 ? PKERR("não foi possível gravar %s", out) : 0;
}
static int write_image(const char *out, u8 *disk, u64 sectors, u32 type, int gpt) {
    if (type == 0xFFFF) return write_raw(out, disk, sectors);
    if (!type) {                                          /* nunca acontece: o formato já foi conferido */
        return PKERR("formato desconhecido");
    }
    if (gpt) {
        uregion_t r[8] = {
            {"Protective Master Boot Record (MBR : 0)", 0, 1}, {"GPT Header (Primary GPT Header : 1)", 1, 1},
            {"GPT Partition Data (Primary GPT Table : 2)", 2, 32}, {" (Apple_Free : 3)", 34, 6},
            {"disk image (Apple_HFS : 4)", 40, sectors - 34 - 40}, {"GPT Partition Data (Backup GPT Table : 5)", sectors - 33, 32},
            {"GPT Header (Backup GPT Header : 6)", sectors - 1, 1}};
        return udif_write(out, disk, sectors, type, r, 7);
    }
    uregion_t r[1] = {{"whole disk (Apple_HFS : 0)", 0, sectors}};
    return udif_write(out, disk, sectors, type, r, 1);
}
/* imagem vazia formatada em path */
static int create_blank(const char *path, u64 size, const char *fs, const char *vol, u32 type, int gpt) {
    u64 sectors = (size + 511) / 512;
    if (gpt) sectors += 40 + 34;
    u8 *disk = calloc(1, (size_t)(sectors * 512));
    if (!disk) { fprintf(stderr, "hdiutil: create failed - sem memória\n"); return 1; }
    u64 voff = gpt ? 40 * 512 : 0, vsize = gpt ? (sectors - 40 - 34) * 512 : sectors * 512;
    int fat = !strcmp(fs, "MS-DOS") || !strcmp(fs, "FAT32") || !strcmp(fs, "MS-DOS FAT32");
    int r;
    if (fat) r = vf_fat32(disk + voff, vsize, vol);
    else if (!strcmp(fs, "HFS+") || !strcmp(fs, "JHFS+") || !strcmp(fs, "HFSX") || !strcmp(fs, "JHFSX") || !strcmp(fs, "Case-sensitive Journaled HFS+"))
        r = vf_hfs(disk + voff, vsize, vol, fs[0] == 'J' || strstr(fs, "Journaled") != 0, strstr(fs, "HFSX") || strstr(fs, "Case-sensitive"));
    else { fprintf(stderr, "hdiutil: create: sistema de arquivos %s não suportado (HFS+, JHFS+, HFSX, JHFSX, MS-DOS FAT32)\n", fs); free(disk); return 1; }
    if (r < 0) { fprintf(stderr, "hdiutil: create failed - o tamanho é pequeno demais para %s\n", fs); free(disk); return 1; }
    if (gpt) gpt_write(disk, sectors, !fat, vol);
    r = write_image(path, disk, sectors, type, gpt);
    free(disk);
    if (r < 0) { fprintf(stderr, "hdiutil: create failed - %s\n", pk_err); return 1; }
    return 0;
}

/* ---- -srcfolder: a árvore vai para o volume de uma UDRW montada gravável pelo kernel */
static u64 tree_bytes(const char *p, int depth) {
    struct stat st;
    if (lstat(p, &st) < 0) return 0;
    u64 n = 4096 + (((u64)st.st_size + 4095) & ~4095ull);
    if (!S_ISDIR(st.st_mode) || depth > 64) return n;
    DIR *d = opendir(p);
    if (!d) return n;
    struct dirent *e;
    char c[2048];
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        snprintf(c, sizeof c, "%s/%s", p, e->d_name);
        n += tree_bytes(c, depth + 1);
    }
    closedir(d);
    return n;
}

static void copy_meta(const char *s, const char *t, const struct stat *st) {
    if (S_ISLNK(st->st_mode)) return;
    chmod(t, st->st_mode & 07777);
#ifdef __APPLE__
    ssize_t ln = listxattr(s, NULL, 0, XATTR_NOFOLLOW);
    char *names = ln > 0 ? malloc((size_t)ln) : NULL;
    if (names && listxattr(s, names, (size_t)ln, XATTR_NOFOLLOW) == ln)
        for (char *k = names; k < names + ln; k += strlen(k) + 1) {
            ssize_t vl = getxattr(s, k, NULL, 0, 0, XATTR_NOFOLLOW);
            char *v = vl >= 0 ? malloc((size_t)vl + 1) : NULL;
            if (v && getxattr(s, k, v, (size_t)vl, 0, XATTR_NOFOLLOW) == vl) setxattr(t, k, v, (size_t)vl, 0, XATTR_NOFOLLOW);
            free(v);
        }
    free(names);
#endif
    struct timeval tv[2] = {{st->st_atime, 0}, {st->st_mtime, 0}};
    utimes(t, tv);
}

static int copy_file(const char *s, const char *t) {
    int in = open(s, O_RDONLY), out = in >= 0 ? open(t, O_WRONLY | O_CREAT | O_TRUNC, 0644) : -1;
    char *b = out >= 0 ? malloc(1 << 20) : NULL;
    int ok = b != NULL;
    ssize_t n;
    while (ok && (n = read(in, b, 1 << 20)) > 0) ok = write(out, b, (size_t)n) == n;
    if (n < 0) ok = 0;
    free(b);
    if (in >= 0) close(in);
    if (out >= 0 && close(out) < 0) ok = 0;
    return ok ? 0 : -1;
}

/* copia o item s para t (pastas recursivamente): dados, links simbólicos, modo, datas e atributos estendidos */
static int copy_item(const char *s, const char *t, int depth) {
    struct stat st;
    if (lstat(s, &st) < 0) return PKERR("%s: %s", s, strerror(errno));
    if (S_ISLNK(st.st_mode)) {
        char l[1024];
        ssize_t k = readlink(s, l, sizeof l - 1);
        if (k < 0) return PKERR("%s: %s", s, strerror(errno));
        l[k] = 0;
        return symlink(l, t) < 0 ? PKERR("%s: %s", t, strerror(errno)) : 0;
    }
    if (S_ISREG(st.st_mode)) {
        if (copy_file(s, t) < 0) return PKERR("não foi possível copiar %s", s);
        copy_meta(s, t, &st);
        return 0;
    }
    if (!S_ISDIR(st.st_mode)) return 0;                   /* fifo, socket, dispositivo: o hdiutil também pula */
    if (depth > 64) return PKERR("%s: pastas aninhadas demais", s);
    if (depth && mkdir(t, 0755) < 0 && errno != EEXIST) return PKERR("%s: %s", t, strerror(errno));
    DIR *d = opendir(s);
    if (!d) return PKERR("%s: %s", s, strerror(errno));
    struct dirent *e;
    char cs[2048], ct[2048];
    int r = 0;
    while (!r && (e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        snprintf(cs, sizeof cs, "%s/%s", s, e->d_name);
        snprintf(ct, sizeof ct, "%s/%s", t, e->d_name);
        r = copy_item(cs, ct, depth + 1);
    }
    closedir(d);
    if (!r) copy_meta(s, t, &st);
    return r;
}

static int convert_file(const char *in, const char *path, u32 type);

static int create_srcfolder(const char *path, const char *src, u64 size, const char *fs, const char *vol, u32 type, int gpt) {
    struct stat st;
    if (stat(src, &st) < 0) { fprintf(stderr, "hdiutil: create failed - %s: No such file or directory\n", src); return 1; }
    u64 need = tree_bytes(src, 0);
    need = need + need / 4 + (16ull << 20);               /* catálogo, journal e folga, como o hdiutil */
    if (size < need) size = need;
    char tmp[1200], abs[1024], mnt[1024] = "";
    snprintf(tmp, sizeof tmp, "%s.srcfolder-%d.dmg", path, (int)getpid());
    if (create_blank(tmp, size, fs, vol, U_RAW, gpt)) return 1;
    if (!realpath(tmp, abs)) { unlink(tmp); fprintf(stderr, "hdiutil: create failed - %s\n", strerror(errno)); return 1; }
    long n = sys3(54, (long)abs, (long)mnt, sizeof mnt);  /* UDRW: o kernel monta para leitura e escrita */
    char *nl = strchr(mnt, '\n');
    if (nl) *nl = 0;
    if (n <= 0 || !mnt[0]) { unlink(tmp); fprintf(stderr, "hdiutil: create failed - não foi possível montar a imagem temporária\n"); return 1; }
    int r;
    if (S_ISDIR(st.st_mode)) r = copy_item(src, mnt, 0);
    else {
        char t[2048];
        const char *b = strrchr(src, '/');
        snprintf(t, sizeof t, "%s/%s", mnt, b ? b + 1 : src);
        r = copy_item(src, t, 1);
    }
    if (sys3(55, (long)mnt, 0, 0) < 0 && !r) r = PKERR("não foi possível ejetar %s", mnt);   /* o eject grava tudo no .dmg */
    if (r < 0) { unlink(tmp); fprintf(stderr, "hdiutil: create failed - %s\n", pk_err); return 1; }
    if (type == U_RAW) r = rename(tmp, path) < 0 ? PKERR("%s: %s", path, strerror(errno)) : 0;
    else { r = convert_file(tmp, path, type); unlink(tmp); }
    if (r) { if (type == U_RAW) unlink(tmp); fprintf(stderr, "hdiutil: create failed - %s\n", pk_err); return 1; }
    return 0;
}

static int cmd_create(int argc, char **argv) {
    const char *out = 0, *fs = "HFS+", *vol = 0, *fmt = 0, *layout = "GPTSPUD", *src = 0;
    u64 size = 0;
    int ov = 0;
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "-size") && i + 1 < argc) size = parse_size(argv[++i]);
        else if (!strcmp(argv[i], "-fs") && i + 1 < argc) fs = argv[++i];
        else if (!strcmp(argv[i], "-volname") && i + 1 < argc) vol = argv[++i];
        else if (!strcmp(argv[i], "-format") && i + 1 < argc) fmt = argv[++i];
        else if (!strcmp(argv[i], "-layout") && i + 1 < argc) layout = argv[++i];
        else if (!strcmp(argv[i], "-type") && i + 1 < argc) { if (strcmp(argv[++i], "UDIF")) { fprintf(stderr, "hdiutil: create: só -type UDIF\n"); return 1; } }
        else if (!strcmp(argv[i], "-srcfolder") && i + 1 < argc) src = argv[++i];
        else if (!strcmp(argv[i], "-ov")) ov = 1;
        else if (argv[i][0] != '-') out = argv[i];
    }
    if (!out || (!size && !src)) { fprintf(stderr, "hdiutil: create: falta a saída ou -size\n"); return 1; }
    if (!fmt) fmt = src ? "UDZO" : "UDRW";                /* padrões do hdiutil */
    u32 type = fmt_type(fmt);
    if (!type) { fprintf(stderr, "hdiutil: create: formato %s não suportado\n", fmt); return 1; }
    char path[1100], vbuf[256];
    snprintf(path, sizeof path, "%s%s", out, strchr(out, '.') && strrchr(out, '.') > strrchr(out, '/') ? "" : type == 0xFFFF ? ".cdr" : ".dmg");
    if (!vol && src) {                                    /* volume com o nome da pasta */
        snprintf(vbuf, sizeof vbuf, "%s", src);
        size_t l = strlen(vbuf);
        while (l > 1 && vbuf[l - 1] == '/') vbuf[--l] = 0;
        char *b = strrchr(vbuf, '/');
        vol = b ? b + 1 : vbuf;
    }
    if (!vol) vol = "untitled";
    struct stat st;
    if (!ov && stat(path, &st) == 0) { fprintf(stderr, "hdiutil: create failed - File exists\n"); return 1; }
    int gpt = strcmp(layout, "NONE") != 0;
    int r = src ? create_srcfolder(path, src, size, fs, vol, type, gpt) : create_blank(path, size, fs, vol, type, gpt);
    if (r) return r;
    if (!quiet) printf("created: %s\n", path);
    return 0;
}
static int convert_file(const char *in, const char *path, u32 type) {
    udif_t u;
    if (udif_open(&u, in) < 0) return -1;
    int ok = 1;
    u8 *disk = udif_expand(&u, &ok);
    if (!disk) { udif_close(&u); return -1; }
    if (!ok) { free(disk); udif_close(&u); return PKERR("checksum da imagem de origem não confere"); }
    int r;
    if (type == 0xFFFF) r = write_raw(path, disk, u.sectors);
    else if (u.nb) {                                      /* mantém as partições da origem */
        uregion_t *reg = calloc((size_t)u.nb, sizeof *reg);
        for (int i = 0; i < u.nb; i++) { reg[i].name = u.b[i].name; reg[i].first = u.b[i].first; reg[i].count = u.b[i].count; }
        r = udif_write(path, disk, u.sectors, type, reg, u.nb);
        free(reg);
    } else {
        uregion_t reg[1] = {{"whole disk (Apple_HFS : 0)", 0, u.sectors}};
        r = udif_write(path, disk, u.sectors, type, reg, 1);
    }
    free(disk);
    udif_close(&u);
    return r < 0 ? -1 : 0;
}

static int cmd_convert(int argc, char **argv) {
    const char *in = 0, *out = 0, *fmt = 0;
    int ov = 0;
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "-format") && i + 1 < argc) fmt = argv[++i];
        else if (!strcmp(argv[i], "-o") && i + 1 < argc) out = argv[++i];
        else if (!strcmp(argv[i], "-ov")) ov = 1;
        else if (argv[i][0] != '-') in = argv[i];
    }
    if (!in || !out || !fmt) { fprintf(stderr, "hdiutil: convert: uso: convert <imagem> -format <fmt> -o <saída>\n"); return 1; }
    u32 type = fmt_type(fmt);
    if (!type) { fprintf(stderr, "hdiutil: convert: formato %s não suportado\n", fmt); return 1; }
    char path[1100];
    snprintf(path, sizeof path, "%s%s", out, strchr(out, '.') && strrchr(out, '.') > strrchr(out, '/') ? "" : type == 0xFFFF ? ".cdr" : ".dmg");
    struct stat st;
    if (!ov && stat(path, &st) == 0) { fprintf(stderr, "hdiutil: convert failed - File exists\n"); return 1; }
    int r = convert_file(in, path, type);
    if (r < 0) { fprintf(stderr, "hdiutil: convert failed - %s\n", pk_err); return 1; }
    if (!quiet) printf("created: %s\n", path);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "uso: hdiutil attach|detach|eject|info|imageinfo|verify|create|convert ...\n");
        return 1;
    }
    for (int i = 2; i < argc; i++) if (!strcmp(argv[i], "-quiet")) quiet = 1;
    const char *c = argv[1];
    if (!strcmp(c, "attach") || !strcmp(c, "mount")) return cmd_attach(argc - 2, argv + 2);
    if (!strcmp(c, "detach") || !strcmp(c, "eject") || !strcmp(c, "unmount")) return cmd_detach(argc - 2, argv + 2);
    if (!strcmp(c, "info")) return cmd_info(argc - 2, argv + 2);
    if (!strcmp(c, "imageinfo")) return cmd_imageinfo(argc - 2, argv + 2);
    if (!strcmp(c, "verify")) return cmd_verify(argc - 2, argv + 2);
    if (!strcmp(c, "create")) return cmd_create(argc - 2, argv + 2);
    if (!strcmp(c, "convert")) return cmd_convert(argc - 2, argv + 2);
    fprintf(stderr, "hdiutil: verbo desconhecido: %s\n", c);
    return 1;
}
