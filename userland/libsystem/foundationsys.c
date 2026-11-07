/*
 * mOS libSystem — funções do macOS que o Foundation (swift-foundation: FileManager, Data, String, Calendar) chama e o
 * libSystem ainda não tinha. Muitas também aparecem em binários compilados com o SDK do macOS 14+ (malloc tipado,
 * variantes _l de locale, fts, *at). Ficam sobre a libc existente (open/stat/opendir/unlink...) e o VFS do mOS, que
 * não guarda permissões, datas por arquivo, links nem atributos estendidos: essas chamadas aceitam e não fazem nada,
 * ou respondem "não suportado", como um volume sem esses recursos.
 */
#include "sys.h"
#include "libc.h"

int stat(const char *p, void *st);
void *opendir(const char *p);
void *readdir(void *d);
int closedir(void *d);
int unlink(const char *p);
int rmdir(const char *p);
int mkdir(const char *p, int mode);
int rename(const char *a, const char *b);
int getpwuid_r(unsigned uid, void *pw, char *buf, size_t len, void **res);
long lseek(int fd, long off, int whence);
double modf(double x, double *i);
double fmod(double x, double y);
double round(double x);

#define AT_FDCWD (-2)
#define AT_REMOVEDIR 0x0080
#define MAC_ENOTSUP 45
#define MAC_ENOATTR 93
#define SET(e) (cx_errno = (e))

/* stat do macOS ($INODE64): 144 bytes; st_mode em +4 */
#define STAT_SIZE 144
static unsigned short st_mode(const void *st) { return *(const unsigned short *)((const char *)st + 4); }
static int is_dir_mode(unsigned short m) { return (m & 0170000) == 0040000; }

/* dirent do macOS ($INODE64) */
typedef struct { u64 d_ino, d_seekoff; u16 d_reclen, d_namlen; u8 d_type; char d_name[1024]; } mac_dirent_t;

/* ======================================================================== malloc tipado (macOS 14+) */
void *malloc_type_malloc(size_t n, u64 type_id) { (void)type_id; return malloc(n); }
void *malloc_type_calloc(size_t c, size_t n, u64 type_id) { (void)type_id; return calloc(c, n); }
void *malloc_type_realloc(void *p, size_t n, u64 type_id) { (void)type_id; return realloc(p, n); }
void malloc_type_free(void *p, u64 type_id) { (void)type_id; free(p); }
int posix_memalign(void **out, size_t align, size_t n);
int malloc_type_posix_memalign(void **out, size_t align, size_t n, u64 type_id) { (void)type_id; return posix_memalign(out, align, n); }
void *malloc_type_valloc(size_t n, u64 type_id) { void *p = 0; (void)type_id; return posix_memalign(&p, 4096, n) ? 0 : p; }
void *malloc_type_aligned_alloc(size_t align, size_t n, u64 type_id) { void *p = 0; (void)type_id; return posix_memalign(&p, align < 8 ? 8 : align, n) ? 0 : p; }
void *malloc_type_zone_malloc(void *zone, size_t n, u64 type_id) { (void)zone; (void)type_id; return malloc(n); }
void *malloc_type_zone_calloc(void *zone, size_t c, size_t n, u64 type_id) { (void)zone; (void)type_id; return calloc(c, n); }
void *malloc_type_zone_realloc(void *zone, void *p, size_t n, u64 type_id) { (void)zone; (void)type_id; return realloc(p, n); }
void malloc_type_zone_free(void *zone, void *p, u64 type_id) { (void)zone; (void)type_id; free(p); }

/* ======================================================================== matemática e locale */
float modff(float x, float *ip) { double i; double f = modf((double)x, &i); *ip = (float)i; return (float)f; }
double remquo(double x, double y, int *quo) {
    double q = round(x / y);
    double r = x - q * y;
    long long qi = (long long)q;
    int s = qi < 0 ? -1 : 1;
    *quo = s * (int)((qi < 0 ? -qi : qi) & 0x7fffffff);
    return r;
}
double strtod_l(const char *s, char **end, void *loc) { (void)loc; return strtod(s, end); }
float strtof_l(const char *s, char **end, void *loc) { (void)loc; return (float)strtod(s, end); }
int strncasecmp_l(const char *a, const char *b, size_t n, void *loc) { (void)loc; return strncasecmp(a, b, n); }

/* ======================================================================== processo / usuários */
int issetugid(void) { return 0; }
int pthread_getugid_np(unsigned *uid, unsigned *gid) { *uid = 501; *gid = 20; return 0; }
int getpwnam_r(const char *name, void *pw, char *buf, size_t len, void **res) { (void)name; return getpwuid_r(501, pw, buf, len, res); }

/* struct group do macOS: gr_name, gr_passwd, gr_gid, gr_mem. Um só grupo, "staff" (20), como o usuário do mOS. */
typedef struct { char *gr_name, *gr_passwd; unsigned gr_gid; char **gr_mem; } mac_group_t;
static int fill_group(mac_group_t *g, char *buf, size_t len, mac_group_t **res) {
    if (len < 32) { *res = 0; return 34; }   /* ERANGE */
    memcpy(buf, "staff", 6);
    memcpy(buf + 8, "*", 2);
    char **mem = (char **)(((size_t)buf + 16 + 7) & ~(size_t)7);
    if ((char *)(mem + 1) > buf + len) { *res = 0; return 34; }
    mem[0] = 0;
    g->gr_name = buf; g->gr_passwd = buf + 8; g->gr_gid = 20; g->gr_mem = mem;
    *res = g;
    return 0;
}
int getgrgid_r(unsigned gid, mac_group_t *g, char *buf, size_t len, mac_group_t **res) {
    if (gid != 20) { *res = 0; return 0; }
    return fill_group(g, buf, len, res);
}
int getgrnam_r(const char *name, mac_group_t *g, char *buf, size_t len, mac_group_t **res) {
    if (strcmp(name, "staff")) { *res = 0; return 0; }
    return fill_group(g, buf, len, res);
}

/* ======================================================================== metadados que o VFS não guarda */
void cx_perm_set(const char *path, unsigned mode);   /* libc.c: o stat devolve o modo pedido */
int chmod(const char *p, unsigned short m) { char st[STAT_SIZE]; int r = stat(p, st); if (r == 0) cx_perm_set(p, m); return r; }
int fchmod(int fd, unsigned short m) { (void)fd; (void)m; return 0; }
int lchmod(const char *p, unsigned short m) { return chmod(p, m); }
int chown(const char *p, unsigned u, unsigned g) { char st[STAT_SIZE]; (void)u; (void)g; return stat(p, st); }
int fchown(int fd, unsigned u, unsigned g) { (void)fd; (void)u; (void)g; return 0; }
int lchown(const char *p, unsigned u, unsigned g) { return chown(p, u, g); }
int chflags(const char *p, unsigned f) { char st[STAT_SIZE]; (void)f; return stat(p, st); }
int fchflags(int fd, unsigned f) { (void)fd; (void)f; return 0; }
int utimes(const char *p, const void *tv) { char st[STAT_SIZE]; (void)tv; return stat(p, st); }
int futimes(int fd, const void *tv) { (void)fd; (void)tv; return 0; }
int lutimes(const char *p, const void *tv) { return utimes(p, tv); }

/* atributos estendidos: o volume não tem nenhum */
long getxattr(const char *p, const char *n, void *v, size_t s, u32 pos, int o) { (void)p; (void)n; (void)v; (void)s; (void)pos; (void)o; SET(MAC_ENOATTR); return -1; }
long fgetxattr(int fd, const char *n, void *v, size_t s, u32 pos, int o) { (void)fd; (void)n; (void)v; (void)s; (void)pos; (void)o; SET(MAC_ENOATTR); return -1; }
int setxattr(const char *p, const char *n, const void *v, size_t s, u32 pos, int o) { (void)p; (void)n; (void)v; (void)s; (void)pos; (void)o; SET(MAC_ENOTSUP); return -1; }
int fsetxattr(int fd, const char *n, const void *v, size_t s, u32 pos, int o) { (void)fd; (void)n; (void)v; (void)s; (void)pos; (void)o; SET(MAC_ENOTSUP); return -1; }
int removexattr(const char *p, const char *n, int o) { (void)p; (void)n; (void)o; SET(MAC_ENOATTR); return -1; }
int fremovexattr(int fd, const char *n, int o) { (void)fd; (void)n; (void)o; SET(MAC_ENOATTR); return -1; }
long listxattr(const char *p, char *b, size_t s, int o) { char st[STAT_SIZE]; (void)b; (void)s; (void)o; return stat(p, st) < 0 ? -1 : 0; }
long flistxattr(int fd, char *b, size_t s, int o) { (void)fd; (void)b; (void)s; (void)o; return 0; }
int quotactl(const char *p, int cmd, int id, char *addr) { (void)p; (void)cmd; (void)id; (void)addr; SET(MAC_ENOTSUP); return -1; }

/* ======================================================================== variantes *at (só relativo ao cwd ou absoluto) */
static int at_ok(int fd, const char *p) {
    if (fd == AT_FDCWD || (p && p[0] == '/')) return 1;
    SET(CX_ENOSYS);
    return 0;
}
const char *cx_at_path(int dirfd, const char *p, char *buf, size_t cap);   /* posixmore.c: dirfd -> caminho (SYS_FDPATH) */
int openat(int fd, const char *p, int flags, ...) { char t[1024]; const char *q = cx_at_path(fd, p, t, sizeof t); return q ? open(q, flags, 0644) : -1; }
int unlinkat(int fd, const char *p, int flag) {
    char t[1024];
    const char *q = cx_at_path(fd, p, t, sizeof t);
    if (!q) return -1;
    return (flag & AT_REMOVEDIR) ? rmdir(q) : unlink(q);
}
int renameat(int fa, const char *a, int fb, const char *b) {
    char ta[1024], tb[1024];
    const char *qa = cx_at_path(fa, a, ta, sizeof ta), *qb = qa ? cx_at_path(fb, b, tb, sizeof tb) : 0;
    return qb ? rename(qa, qb) : -1;
}
int renamex_np(const char *a, const char *b, unsigned flags) { (void)flags; return rename(a, b); }
int renameatx_np(int fa, const char *a, int fb, const char *b, unsigned flags) { (void)flags; return renameat(fa, a, fb, b); }

/* ======================================================================== cópia de arquivos */
static int copy_fd(int in, int out) {
    char buf[16384];
    long n;
    while ((n = read(in, buf, sizeof buf)) > 0)
        for (long o = 0; o < n;) { long w = write(out, buf + o, (size_t)(n - o)); if (w <= 0) return -1; o += w; }
    return n < 0 ? -1 : 0;
}
static int copy_file(const char *from, const char *to) {
    int in = open(from, 0);
    if (in < 0) return -1;
    int out = open(to, 0x0601, 0644);   /* O_WRONLY|O_CREAT|O_TRUNC */
    if (out < 0) { close(in); return -1; }
    int r = copy_fd(in, out);
    close(in); close(out);
    return r;
}
static int copy_tree(const char *from, const char *to) {
    char st[STAT_SIZE];
    if (stat(from, st) < 0) return -1;
    if (!is_dir_mode(st_mode(st))) return copy_file(from, to);
    if (mkdir(to, 0755) < 0 && stat(to, st) < 0) return -1;
    void *d = opendir(from);
    if (!d) return -1;
    int r = 0;
    for (mac_dirent_t *e; r == 0 && (e = readdir(d));) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char *a = malloc(2048), *b = a + 1024;
        strlcpy(a, from, 1024); strlcat(a, "/", 1024); strlcat(a, e->d_name, 1024);
        strlcpy(b, to, 1024); strlcat(b, "/", 1024); strlcat(b, e->d_name, 1024);
        r = copy_tree(a, b);
        free(a);
    }
    closedir(d);
    return r;
}
#define COPYFILE_RECURSIVE (1 << 15)
#define COPYFILE_DATA (1 << 3)
int copyfile(const char *from, const char *to, void *state, u32 flags) {
    (void)state;
    char st[STAT_SIZE];
    if (stat(from, st) < 0) return -1;
    if (is_dir_mode(st_mode(st))) {
        if (flags & COPYFILE_RECURSIVE) return copy_tree(from, to);
        if (mkdir(to, 0755) < 0 && stat(to, st) < 0) return -1;
        return 0;
    }
    return copy_file(from, to);
}
int fcopyfile(int from, int to, void *state, u32 flags) {
    (void)state; (void)flags;
    lseek(from, 0, 0);
    return copy_fd(from, to);
}
/* link físico: o VFS não tem; uma cópia mantém o conteúdo visível pelos dois nomes */
int link(const char *a, const char *b) { char st[STAT_SIZE]; if (stat(b, st) == 0) { SET(CX_EEXIST); return -1; } return copy_file(a, b); }
int linkat(int fa, const char *a, int fb, const char *b, int flag) {
    (void)flag;
    char ta[1024], tb[1024];
    const char *qa = cx_at_path(fa, a, ta, sizeof ta), *qb = qa ? cx_at_path(fb, b, tb, sizeof tb) : 0;
    return qb ? link(qa, qb) : -1;
}

/* ======================================================================== removefile */
typedef int (*removefile_cb)(void *state, const char *path, void *ctx);
typedef struct { removefile_cb confirm, error, status; void *confirm_ctx, *error_ctx, *status_ctx; int err; int cancel; } rf_state_t;
enum { RF_CONFIRM_CB = 1, RF_CONFIRM_CTX, RF_ERROR_CB, RF_ERROR_CTX, RF_ERRNO, RF_STATUS_CB, RF_STATUS_CTX, RF_FTSENT };
#define REMOVEFILE_RECURSIVE (1 << 0)
#define REMOVEFILE_KEEP_PARENT (1 << 1)

void *removefile_state_alloc(void) { return calloc(1, sizeof(rf_state_t)); }
int removefile_state_free(void *s) { free(s); return 0; }
int removefile_state_get(void *sp, u32 key, void *dst) {
    rf_state_t *s = sp;
    switch (key) {
    case RF_CONFIRM_CB: *(removefile_cb *)dst = s->confirm; return 0;
    case RF_CONFIRM_CTX: *(void **)dst = s->confirm_ctx; return 0;
    case RF_ERROR_CB: *(removefile_cb *)dst = s->error; return 0;
    case RF_ERROR_CTX: *(void **)dst = s->error_ctx; return 0;
    case RF_ERRNO: *(int *)dst = s->err; return 0;
    case RF_STATUS_CB: *(removefile_cb *)dst = s->status; return 0;
    case RF_STATUS_CTX: *(void **)dst = s->status_ctx; return 0;
    case RF_FTSENT: *(void **)dst = 0; return 0;
    }
    SET(CX_EINVAL);
    return -1;
}
int removefile_state_set(void *sp, u32 key, const void *v) {
    rf_state_t *s = sp;
    switch (key) {
    case RF_CONFIRM_CB: s->confirm = (removefile_cb)v; return 0;
    case RF_CONFIRM_CTX: s->confirm_ctx = (void *)v; return 0;
    case RF_ERROR_CB: s->error = (removefile_cb)v; return 0;
    case RF_ERROR_CTX: s->error_ctx = (void *)v; return 0;
    case RF_STATUS_CB: s->status = (removefile_cb)v; return 0;
    case RF_STATUS_CTX: s->status_ctx = (void *)v; return 0;
    }
    SET(CX_EINVAL);
    return -1;
}
/* devolve 0 (seguir), 1 (pular este item) ou 2 (parar tudo), como REMOVEFILE_PROCEED/SKIP/STOP */
static int rf_confirm(rf_state_t *s, const char *p) { return s && s->confirm ? s->confirm(s, p, s->confirm_ctx) : 0; }
static int rf_fail(rf_state_t *s, const char *p) {
    if (!s) return 2;
    s->err = cx_errno;
    return s->error ? s->error(s, p, s->error_ctx) : 2;
}
static int rf_remove(rf_state_t *s, const char *p, int recursive, int keep) {
    char st[STAT_SIZE];
    if (s && s->cancel) return 2;
    int c = rf_confirm(s, p);
    if (c) return c;
    if (stat(p, st) < 0) return rf_fail(s, p);
    if (is_dir_mode(st_mode(st))) {
        if (recursive) {
            void *d = opendir(p);
            if (!d) return rf_fail(s, p);
            /* lê os nomes antes de apagar: o índice do readdir do mOS muda quando a pasta encolhe */
            char **names = 0; int n = 0, cap = 0;
            for (mac_dirent_t *e; (e = readdir(d));) {
                if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
                if (n == cap) { cap = cap ? cap * 2 : 16; names = realloc(names, (size_t)cap * sizeof *names); }
                size_t l = strlen(p) + strlen(e->d_name) + 2;
                names[n] = malloc(l);
                strlcpy(names[n], p, l); strlcat(names[n], "/", l); strlcat(names[n], e->d_name, l);
                n++;
            }
            closedir(d);
            int r = 0;
            for (int i = 0; i < n; i++) { if (r != 2) { int q = rf_remove(s, names[i], 1, 0); if (q == 2) r = 2; } free(names[i]); }
            free(names);
            if (r == 2) return 2;
        }
        if (!keep && rmdir(p) < 0) return rf_fail(s, p);
    } else if (unlink(p) < 0) return rf_fail(s, p);
    if (s && s->status) s->status(s, p, s->status_ctx);
    return 0;
}
int removefile(const char *path, void *state, u32 flags) {
    rf_state_t *s = state;
    int r = rf_remove(s, path, (flags & REMOVEFILE_RECURSIVE) != 0, (flags & REMOVEFILE_KEEP_PARENT) != 0);
    if (r == 2) { if (s && s->err) cx_errno = s->err; return -1; }
    return 0;
}
int removefileat(int fd, const char *path, void *state, u32 flags) { return at_ok(fd, path) ? removefile(path, state, flags) : -1; }
int removefile_cancel(void *state) { if (state) ((rf_state_t *)state)->cancel = 1; return 0; }

/* ======================================================================== sysdir (pastas padrão do sistema) */
/* domínios na ordem do macOS: usuário (~), local (/), rede (sem), sistema (/System) */
typedef struct { int dir; const char *user, *local, *system; } sysdir_row_t;
static const sysdir_row_t sysdir_rows[] = {
    {1, "~/Applications", "/Applications", "/System/Applications"},
    {2, "~/Applications/Demos", "/Applications/Demos", 0},
    {3, 0, "/Developer/Applications", 0},
    {4, "~/Applications/Utilities", "/Applications/Utilities", "/System/Applications/Utilities"},
    {5, "~/Library", "/Library", "/System/Library"},
    {6, 0, "/Developer", 0},
    {7, 0, "/Users", 0},
    {8, "~/Library/Documentation", "/Library/Documentation", "/System/Library/Documentation"},
    {9, "~/Documents", 0, 0},
    {10, 0, 0, "/System/Library/CoreServices"},
    {11, "~/Library/Autosave Information", 0, 0},
    {12, "~/Desktop", 0, 0},
    {13, "~/Library/Caches", "/Library/Caches", "/System/Library/Caches"},
    {14, "~/Library/Application Support", "/Library/Application Support", 0},
    {15, "~/Downloads", 0, 0},
    {16, "~/Library/Input Methods", "/Library/Input Methods", "/System/Library/Input Methods"},
    {17, "~/Movies", 0, 0},
    {18, "~/Music", 0, 0},
    {19, "~/Pictures", 0, 0},
    {20, 0, "/Library/Printers/PPDs", "/System/Library/Printers/PPDs"},
    {21, "~/Public", 0, 0},
    {22, "~/Library/PreferencePanes", "/Library/PreferencePanes", "/System/Library/PreferencePanes"},
    {23, "~/Library/Application Scripts", 0, 0},
    {100, "~/Applications", "/Applications", "/System/Applications"},
    {101, "~/Library", "/Library", "/System/Library"},
};
/* estado: diretório nos bits 0-7, máscara de domínios nos bits 8-23, próximo domínio nos bits 24-27 (0 = fim) */
u32 sysdir_start_search_path_enumeration(u32 dir, u32 mask) { return (dir & 0xff) | ((mask & 0xffff) << 8); }
u32 sysdir_get_next_search_path_enumeration(u32 state, char *path) {
    u32 dir = state & 0xff, mask = (state >> 8) & 0xffff, next = state >> 24;
    const sysdir_row_t *row = 0;
    for (unsigned i = 0; i < sizeof sysdir_rows / sizeof *sysdir_rows; i++) if ((u32)sysdir_rows[i].dir == dir) row = &sysdir_rows[i];
    if (!row) return 0;
    for (u32 dom = next; dom < 4; dom++) {
        const char *p = dom == 0 ? row->user : dom == 1 ? row->local : dom == 3 ? row->system : 0;
        if (!p || !(mask & (1u << dom))) continue;
        strlcpy(path, p, 1024);
        return dir | (mask << 8) | ((dom + 1) << 24);
    }
    return 0;
}

/* ======================================================================== fts (percurso de árvore de diretórios) */
/* FTSENT do macOS ($INODE64, fts_compat.h): layout x86-64 exato, lido pelo Swift */
typedef struct mac_ftsent {
    struct mac_ftsent *fts_cycle, *fts_parent, *fts_link;
    long fts_number;
    void *fts_pointer;
    char *fts_accpath, *fts_path;
    int fts_errno, fts_symfd;
    unsigned short fts_pathlen, fts_namelen;
    u64 fts_ino;
    i32 fts_dev;
    u16 fts_nlink;
    short fts_level;
    unsigned short fts_info, fts_flags, fts_instr;
    void *fts_statp;
    char fts_name[1];
} mac_ftsent_t;
_Static_assert(__builtin_offsetof(mac_ftsent_t, fts_ino) == 72, "FTSENT.fts_ino");
_Static_assert(__builtin_offsetof(mac_ftsent_t, fts_statp) == 96, "FTSENT.fts_statp");
_Static_assert(__builtin_offsetof(mac_ftsent_t, fts_name) == 104, "FTSENT.fts_name");

enum { FTS_D = 1, FTS_DEFAULT = 3, FTS_DNR = 4, FTS_DP = 6, FTS_F = 8, FTS_NS = 10, FTS_SKIP = 4 };
#define FTS_SEEDOT 0x020

typedef struct fts_frame { mac_ftsent_t *dir; void *d; struct fts_frame *up; } fts_frame_t;
/* o FTS de fora é opaco para quem chama; os primeiros campos imitam o do macOS (fts_cur, fts_child) */
typedef struct {
    mac_ftsent_t *fts_cur, *fts_child;
    char **roots;
    int nroots, next_root, options;
    fts_frame_t *top;
    mac_ftsent_t *pending;        /* pasta devolvida em pré-ordem: abrir na próxima leitura (salvo FTS_SKIP) */
    mac_ftsent_t *to_free;        /* entrada devolvida antes, liberada na próxima leitura */
} mos_fts_t;

static mac_ftsent_t *fts_make(const char *path, const char *name, short level, mac_ftsent_t *parent) {
    size_t nl = strlen(name), pl = strlen(path);
    mac_ftsent_t *e = calloc(1, sizeof *e + nl + 1 + pl + 1 + STAT_SIZE + 8);
    if (!e) return 0;
    memcpy(e->fts_name, name, nl + 1);
    e->fts_path = e->fts_name + nl + 1;
    memcpy(e->fts_path, path, pl + 1);
    e->fts_accpath = e->fts_path;
    e->fts_statp = (void *)(((size_t)(e->fts_path + pl + 1) + 7) & ~(size_t)7);
    e->fts_pathlen = (unsigned short)pl;
    e->fts_namelen = (unsigned short)nl;
    e->fts_level = level;
    e->fts_parent = parent;
    e->fts_symfd = -1;
    e->fts_nlink = 1;
    e->fts_instr = 3;   /* FTS_NOINSTR */
    if (stat(path, e->fts_statp) < 0) { e->fts_info = FTS_NS; e->fts_errno = cx_errno; return e; }
    e->fts_ino = *(u64 *)((char *)e->fts_statp + 8);
    e->fts_info = is_dir_mode(st_mode(e->fts_statp)) ? FTS_D : FTS_F;
    return e;
}

void *fts_open(char *const *argv, int options, void *compar) {
    (void)compar;   /* a ordem é a do diretório */
    mos_fts_t *f = calloc(1, sizeof *f);
    if (!f) return 0;
    while (argv[f->nroots]) f->nroots++;
    f->roots = argv;
    f->options = options;
    return f;
}
void *fts_open_b(char *const *argv, int options, void *block) { (void)block; return fts_open(argv, options, 0); }

mac_ftsent_t *fts_read(void *fp) {
    mos_fts_t *f = fp;
    if (f->to_free) { free(f->to_free); f->to_free = 0; }
    if (f->pending) {
        mac_ftsent_t *dir = f->pending;
        f->pending = 0;
        if (dir->fts_instr != FTS_SKIP) {
            void *d = opendir(dir->fts_path);
            if (!d) { dir->fts_info = FTS_DNR; dir->fts_errno = cx_errno; return f->fts_cur = dir; }
            fts_frame_t *fr = calloc(1, sizeof *fr);
            fr->dir = dir; fr->d = d; fr->up = f->top;
            f->top = fr;
        } else if (!f->top || f->top->dir != dir) {
            /* pasta pulada: sem pós-ordem; libera se não for raiz de quadro aberto */
            f->to_free = dir;
        }
    }
    while (f->top) {
        fts_frame_t *fr = f->top;
        mac_dirent_t *de = readdir(fr->d);
        if (de) {
            if (!(f->options & FTS_SEEDOT) && (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))) continue;
            size_t l = strlen(fr->dir->fts_path) + strlen(de->d_name) + 2;
            char *p = malloc(l);
            strlcpy(p, fr->dir->fts_path, l);
            if (l > 2 && p[strlen(p) - 1] != '/') strlcat(p, "/", l);
            strlcat(p, de->d_name, l);
            mac_ftsent_t *e = fts_make(p, de->d_name, (short)(fr->dir->fts_level + 1), fr->dir);
            free(p);
            if (!e) continue;
            if (e->fts_info == FTS_D) f->pending = e; else f->to_free = e;
            return f->fts_cur = e;
        }
        closedir(fr->d);
        f->top = fr->up;
        mac_ftsent_t *dir = fr->dir;
        free(fr);
        dir->fts_info = FTS_DP;
        f->to_free = dir;
        return f->fts_cur = dir;
    }
    if (f->next_root < f->nroots) {
        const char *r = f->roots[f->next_root++];
        const char *name = r;
        for (const char *q = r; *q; q++) if (q[0] == '/' && q[1]) name = q + 1;
        mac_ftsent_t *e = fts_make(r, name, 0, 0);
        if (!e) return 0;
        if (e->fts_info == FTS_D) f->pending = e; else f->to_free = e;
        return f->fts_cur = e;
    }
    cx_errno = 0;
    return 0;
}
int fts_set(void *fp, mac_ftsent_t *e, int instr) { (void)fp; e->fts_instr = (unsigned short)instr; return 0; }
mac_ftsent_t *fts_children(void *fp, int opts) { (void)fp; (void)opts; cx_errno = 0; return 0; }
int fts_close(void *fp) {
    mos_fts_t *f = fp;
    if (!f) return 0;
    while (f->top) { fts_frame_t *fr = f->top; f->top = fr->up; closedir(fr->d); free(fr->dir); free(fr); }
    if (f->pending) free(f->pending);
    if (f->to_free) free(f->to_free);
    free(f);
    return 0;
}

/* ======================================================================== statfs ($INODE64, 2168 bytes) */
/* o volume do sistema, com os mesmos números do statvfs de darwin.c (512 MiB, metade livre) */
typedef struct {
    u32 f_bsize; i32 f_iosize; u64 f_blocks, f_bfree, f_bavail, f_files, f_ffree; i32 f_fsid[2]; u32 f_owner;
    u32 f_type, f_flags, f_fssubtype; char f_fstypename[16], f_mntonname[1024], f_mntfromname[1024]; u32 f_flags_ext, f_reserved[7];
} mac_statfs_t;
_Static_assert(sizeof(mac_statfs_t) == 2168, "statfs do macOS");
int statfs(const char *path, mac_statfs_t *s) {
    char st[STAT_SIZE];
    if (stat(path, st) < 0) return -1;
    memset(s, 0, sizeof *s);
    s->f_bsize = 4096; s->f_iosize = 65536;
    s->f_blocks = 131072; s->f_bfree = s->f_bavail = 65536;
    s->f_files = s->f_ffree = 65536;
    s->f_owner = 0; s->f_type = 0x1c; s->f_flags = 0x00001000 | 0x00004000;   /* MNT_LOCAL | MNT_ROOTFS */
    strlcpy(s->f_fstypename, "msdos", sizeof s->f_fstypename);
    strlcpy(s->f_mntonname, "/", sizeof s->f_mntonname);
    strlcpy(s->f_mntfromname, "/dev/disk0s1", sizeof s->f_mntfromname);
    return 0;
}
int fstatfs(int fd, mac_statfs_t *s) { (void)fd; return statfs("/", s); }

/* ======================================================================== Mach VM */
int vm_copy(u32 task, u64 src, u64 size, u64 dst) { (void)task; memmove((void *)dst, (const void *)src, (size_t)size); return 0; }
