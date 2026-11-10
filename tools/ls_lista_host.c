/* Lista os bundles que o Launch Services registraria a partir de uma imagem do mkimg.
 * Mesma regra de kernel/compat/launchsvc.c (ls_rescan / register_bundle): .app com Contents/Info.plist
 * em /Aplicativos, /Aplicativos/Utilitários, /Applications e /Applications/Utilities.
 * O Info.plist é lido pelo parser do kernel (plist.c). Não sobe QEMU e não executa o app.
 * Uso: ls_lista_host <imagem.img>
 */
/* Sem cabeçalhos da libc: kernel.h (via plist.h) redeclararia memcpy/strlen. */
#include "plist.h"

void *malloc(usize n);
void free(void *p);
void *realloc(void *p, usize n);
void *kmalloc(usize n) { return malloc(n); }
void kfree(void *p) { free(p); }
void *kcalloc(usize n, usize sz) {
    usize b = n * sz;
    void *p = malloc(b);
    if (p) memset(p, 0, b);
    return p;
}
int open(const char *path, int flags);
int close(int fd);
isize pread(int fd, void *buf, usize n, i64 off);
isize write(int fd, const void *buf, usize n);

static void out(const char *s) { write(1, s, strlen(s)); }
static void err(const char *s) { write(2, s, strlen(s)); }

static int imgfd;

static int rd(u64 off, void *buf, usize n) {
    return pread(imgfd, buf, n, (i64)off) == (isize)n;
}

static u16 r16(const u8 *p) { return (u16)(p[0] | (p[1] << 8)); }
static u32 r32(const u8 *p) { return r16(p) | ((u32)r16(p + 2) << 16); }
static u64 r64(const u8 *p) { return r32(p) | ((u64)r32(p + 4) << 32); }

static u32 bps, spc, reserved, fats, fatsz, root_cl;
static u64 part_off;
static u32 *fat;
static u32 fat_n;

static int load_fat(void) {
    u8 sec[512], ent[128];
    if (!rd(2 * 512, ent, 128)) return 0;
    u64 lba = r64(ent + 32);
    part_off = lba * 512;
    if (!rd(part_off, sec, 512)) return 0;
    if (sec[510] != 0x55 || sec[511] != 0xAA) return 0;
    bps = r16(sec + 11);
    spc = sec[13];
    reserved = r16(sec + 14);
    fats = sec[16];
    fatsz = r32(sec + 36);
    root_cl = r32(sec + 44);
    if (!bps || !spc || bps > 4096 || fatsz > 2000000) return 0;
    fat_n = fatsz * (bps / 4);
    fat = malloc((usize)fat_n * 4);
    if (fat) memset(fat, 0, (usize)fat_n * 4);
    if (!fat) return 0;
    return rd(part_off + (u64)reserved * bps, fat, (usize)fatsz * bps);
}

static u64 cl_off(u32 cl) {
    u64 data = part_off + ((u64)reserved + (u64)fats * fatsz) * bps;
    return data + (u64)(cl - 2) * spc * bps;
}

static int read_chain(u32 cl, u8 **out, usize *outn) {
    usize cap = 0, n = 0;
    u8 *buf = NULL;
    usize csz = (usize)spc * bps;
    int guard = 0;
    while (cl >= 2 && cl < 0x0FFFFFF8 && cl < fat_n && guard++ < 1000000) {
        if (n + csz > cap) {
            cap = cap ? cap * 2 : csz * 4;
            if (cap < n + csz) cap = n + csz;
            u8 *nb = realloc(buf, cap);
            if (!nb) { free(buf); return 0; }
            buf = nb;
        }
        if (!rd(cl_off(cl), buf + n, csz)) { free(buf); return 0; }
        n += csz;
        cl = fat[cl];
    }
    *out = buf;
    *outn = n;
    return 1;
}

static void lfn_put(char *name, int *nn, u16 ch) {
    if (*nn >= 250) return;
    if (ch == 0 || ch == 0xFFFF) return;
    if (ch < 0x80) name[(*nn)++] = (char)ch;
    else if (ch < 0x800) {
        if (*nn + 2 > 250) return;
        name[(*nn)++] = (char)(0xC0 | (ch >> 6));
        name[(*nn)++] = (char)(0x80 | (ch & 0x3F));
    } else {
        if (*nn + 3 > 250) return;
        name[(*nn)++] = (char)(0xE0 | (ch >> 12));
        name[(*nn)++] = (char)(0x80 | ((ch >> 6) & 0x3F));
        name[(*nn)++] = (char)(0x80 | (ch & 0x3F));
    }
}

static int find_child(const u8 *dir, usize n, const char *want, int *is_dir, u32 *cl, u32 *sz) {
    char lfn[256];
    int have_lfn = 0;
    static const int off[13] = {1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30};
    for (usize i = 0; i + 32 <= n; i += 32) {
        const u8 *e = dir + i;
        if (e[0] == 0) break;
        if (e[0] == 0xE5) { have_lfn = 0; continue; }
        if ((e[11] & 0x0F) == 0x0F) {
            int ord = e[0] & 0x1F;
            if (e[0] & 0x40) have_lfn = 1;
            char piece[40];
            int pn = 0;
            for (int k = 0; k < 13; k++) {
                u16 ch = r16(e + off[k]);
                lfn_put(piece, &pn, ch);
            }
            piece[pn] = 0;
            if (ord >= 1 && ord <= 20) {
                char tmp[256];
                snprintf(tmp, sizeof tmp, "%s%s", piece, lfn);
                snprintf(lfn, sizeof lfn, "%s", tmp);
            }
            continue;
        }
        if (e[11] & 0x08) { have_lfn = 0; continue; }
        char sfn[13], name[256];
        int k = 0;
        for (int j = 0; j < 8 && e[j] != ' '; j++) sfn[k++] = e[j];
        int ext = 0;
        if (e[8] != ' ') { sfn[k++] = '.'; for (int j = 8; j < 11 && e[j] != ' '; j++) sfn[k++] = e[j]; ext = 1; }
        sfn[k] = 0;
        (void)ext;
        if (have_lfn && lfn[0]) snprintf(name, sizeof name, "%s", lfn);
        else snprintf(name, sizeof name, "%s", sfn);
        have_lfn = 0;
        lfn[0] = 0;
        if (!strcmp(name, ".") || !strcmp(name, "..")) continue;
        if (strcmp(name, want)) continue;
        *is_dir = (e[11] & 0x10) != 0;
        *cl = r16(e + 26) | ((u32)r16(e + 20) << 16);
        *sz = r32(e + 28);
        return 1;
    }
    return 0;
}

static int lookup(const char *path, int *is_dir, u32 *cl, u32 *sz, u8 **data, usize *dn) {
    u8 *dir = NULL;
    usize dnloc = 0;
    u32 cur = root_cl;
    if (!read_chain(cur, &dir, &dnloc)) return 0;
    const char *p = path;
    if (*p == '/') p++;
    while (*p) {
        char comp[256];
        int k = 0;
        while (*p && *p != '/' && k < 255) comp[k++] = *p++;
        comp[k] = 0;
        if (*p == '/') p++;
        int id = 0;
        u32 c = 0, s = 0;
        if (!find_child(dir, dnloc, comp, &id, &c, &s)) { free(dir); return 0; }
        free(dir);
        dir = NULL;
        if (!*p) {
            *is_dir = id;
            *cl = c;
            *sz = s;
            if (data) {
                if (!read_chain(c, data, dn)) return 0;
                if (!id && *dn > s) *dn = s;
            }
            return 1;
        }
        if (!id || !read_chain(c, &dir, &dnloc)) return 0;
    }
    free(dir);
    return 0;
}

static const char *app_dirs[] = {
    "/mOS/Aplicativos",
    "/mOS/Aplicativos/Utilitários",
    "/mOS/Applications",
    "/mOS/Applications/Utilities",
};

static int ends_app(const char *s) {
    usize n = strlen(s);
    return n > 4 && !strcasecmp(s + n - 4, ".app");
}

static void each_dir(const u8 *dir, usize n, void (*fn)(const char *name, void *ctx), void *ctx) {
    char lfn[256];
    int have_lfn = 0;
    static const int off[13] = {1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30};
    for (usize i = 0; i + 32 <= n; i += 32) {
        const u8 *e = dir + i;
        if (e[0] == 0) break;
        if (e[0] == 0xE5) { have_lfn = 0; continue; }
        if ((e[11] & 0x0F) == 0x0F) {
            if (e[0] & 0x40) { lfn[0] = 0; have_lfn = 1; }
            char piece[40];
            int pn = 0;
            for (int k = 0; k < 13; k++) lfn_put(piece, &pn, r16(e + off[k]));
            piece[pn] = 0;
            char tmp[256];
            snprintf(tmp, sizeof tmp, "%s%s", piece, lfn);
            snprintf(lfn, sizeof lfn, "%s", tmp);
            continue;
        }
        if (e[11] & 0x08) { have_lfn = 0; continue; }
        char name[256];
        if (have_lfn && lfn[0]) snprintf(name, sizeof name, "%s", lfn);
        else {
            int k = 0;
            for (int j = 0; j < 8 && e[j] != ' '; j++) name[k++] = e[j];
            if (e[8] != ' ') { name[k++] = '.'; for (int j = 8; j < 11 && e[j] != ' '; j++) name[k++] = e[j]; }
            name[k] = 0;
        }
        have_lfn = 0;
        if (name[0] && strcmp(name, ".") && strcmp(name, "..")) fn(name, ctx);
    }
}

typedef struct { const char *base; int n; } list_ctx;

static void on_name(const char *name, void *ctx) {
    list_ctx *c = ctx;
    if (!ends_app(name)) return;
    char path[512], ip[560];
    snprintf(path, sizeof path, "%s/%s", c->base, name);
    snprintf(ip, sizeof ip, "%s/Contents/Info.plist", path);
    int id = 0;
    u32 cl = 0, sz = 0;
    u8 *data = NULL;
    usize dn = 0;
    int contents = 0;
    u32 ccl, csz;
    char ct[512];
    snprintf(ct, sizeof ct, "%s/Contents", path);
    if (lookup(ct, &contents, &ccl, &csz, NULL, NULL) && contents) {
        if (!lookup(ip, &id, &cl, &sz, &data, &dn) || id) {
            err("ls: bundle sem Info.plist\n");
            free(data);
            return;
        }
        pl_t *info = pl_parse(data, dn);
        const char *bid = info ? pl_str(info, "CFBundleIdentifier") : NULL;
        const char *exe = info ? pl_str(info, "CFBundleExecutable") : NULL;
        char line[640];
        snprintf(line, sizeof line, "ls: %s (%s) exe=%s\n", name, bid && bid[0] ? bid : "?", exe && exe[0] ? exe : "?");
        out(line);
        c->n++;
        pl_free(info);
        free(data);
    }
}

int main(int argc, char **argv) {
    if (argc != 2) { err("uso: ls_lista_host <imagem.img>\n"); return 2; }
    imgfd = open(argv[1], 0);
    if (imgfd < 0) { err("ls_lista_host: não abriu a imagem\n"); return 1; }
    if (!load_fat()) { err("ls_lista_host: FAT32 ilegível\n"); return 1; }
    int total = 0;
    for (unsigned i = 0; i < sizeof app_dirs / sizeof app_dirs[0]; i++) {
        int id = 0;
        u32 cl = 0, sz = 0;
        u8 *dir = NULL;
        usize dn = 0;
        if (!lookup(app_dirs[i], &id, &cl, &sz, &dir, &dn) || !id) { free(dir); continue; }
        list_ctx c = {app_dirs[i], 0};
        each_dir(dir, dn, on_name, &c);
        total += c.n;
        free(dir);
    }
    char line[80];
    snprintf(line, sizeof line, "ls_lista_host: %d bundle(s)\n", total);
    out(line);
    return total ? 0 : 1;
}
