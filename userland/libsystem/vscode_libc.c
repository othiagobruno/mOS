/*
 * mOS libSystem — o que faltava do libSystem.B.dylib para o Electron/VS Code (inventário do
 * tools/machocheck.c: 186 símbolos). Cada função faz o que o macOS faz quando dá para fazer aqui;
 * quando o mOS não tem o recurso (exceções Mach por thread, suspender threads alheias, libunwind,
 * fileports), devolve o mesmo erro que o macOS devolve num ambiente restrito, que é o caminho que o
 * Chromium já trata (o profiler de amostragem, o crashpad e o QUIC caem para o modo sem o recurso).
 */
#include "sys.h"
#include "libc.h"

typedef int kern_return_t;
typedef u32 mach_port_t;
#define KERN_SUCCESS 0
#define KERN_INVALID_ADDRESS 1
#define KERN_INVALID_ARGUMENT 4
#define KERN_FAILURE 5
#define KERN_NOT_SUPPORTED 46
#define E_NOSYS 78
#define E_NOTSUP 45
#define E_PERM 1
#define E_INVAL 22
#define E_NOENT 2

typedef struct { long tv_sec; long tv_nsec; } vs_timespec_t;
typedef struct { long sec; int usec; } vs_timeval_t;
int clock_gettime(int clk, void *ts);
int gettimeofday(void *tv, void *tz);
int sched_yield(void);
int getpid(void);
int vsnprintf(char *b, size_t n, const char *fmt, va_list ap);
int vasprintf(char **out, const char *fmt, va_list ap);
mach_port_t mach_task_self(void);
mach_port_t mach_thread_self(void);
kern_return_t mach_vm_allocate(mach_port_t task, u64 *addr, u64 size, int flags);
off_t lseek(int fd, off_t off, int whence);
ssize_t pread(int fd, void *b, size_t n, off_t off);
ssize_t pwrite(int fd, const void *b, size_t n, off_t off);
int stat(const char *p, void *st);
void qsort(void *base, size_t n, size_t sz, int (*cmp)(const void *, const void *));
double pow(double b, double e);
double floor(double x);
double rint(double x);
void *opendir(const char *p);
void *readdir(void *d);
int closedir(void *d);

static void set_errno(int e) { cx_errno = e; }

/* ================================================================ abort/diagnóstico */
__attribute__((noreturn)) void abort_report_np(const char *fmt, ...) {
    char b[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    if (n > 0) { write(2, b, (size_t)(n < (int)sizeof b ? n : (int)sizeof b - 1)); write(2, "\n", 1); }
    abort();
}
/* syslog$DARWIN_EXTSN: oslog.c (vai ao logd) */
/* os_signpost: Instruments não existe aqui */
int os_signpost_enabled(void *log) { (void)log; return 0; }
void _os_signpost_emit_with_name_impl(void *dso, void *log, int type, u64 id, const char *name, const char *fmt, u8 *buf, u32 size) {
    (void)dso; (void)log; (void)type; (void)id; (void)name; (void)fmt; (void)buf; (void)size;
}

/* ================================================================ locale (_l) e catálogos */
int asprintf_l(char **out, void *loc, const char *fmt, ...) {
    (void)loc;
    va_list ap;
    va_start(ap, fmt);
    int r = vasprintf(out, fmt, ap);
    va_end(ap);
    return r;
}
size_t strftime(char *s, size_t max, const char *fmt, const void *tm);
size_t strftime_l(char *s, size_t max, const char *fmt, const void *tm, void *loc) { (void)loc; return strftime(s, max, fmt, tm); }
typedef long nl_catd;
nl_catd catopen(const char *name, int flag) { (void)name; (void)flag; set_errno(E_NOENT); return -1; }
char *catgets(nl_catd c, int set, int num, const char *def) { (void)c; (void)set; (void)num; return (char *)def; }
int catclose(nl_catd c) { (void)c; return 0; }

/* ================================================================ wchar (wchar_t = 4 bytes no macOS) */
typedef int wchar_t_;
typedef struct { int st; } mbstate_t_;
static int utf8_dec(const unsigned char *s, size_t n, wchar_t_ *out) {
    if (!n) return 0;
    unsigned c = s[0];
    if (c < 0x80) { *out = (wchar_t_)c; return 1; }
    int len = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 0;
    if (!len || (size_t)len > n) return -1;
    unsigned v = c & (0x7F >> len);
    for (int i = 1; i < len; i++) { if ((s[i] & 0xC0) != 0x80) return -1; v = v << 6 | (s[i] & 0x3F); }
    *out = (wchar_t_)v;
    return len;
}
static int utf8_enc(unsigned v, char *o) {
    if (v < 0x80) { o[0] = (char)v; return 1; }
    if (v < 0x800) { o[0] = (char)(0xC0 | v >> 6); o[1] = (char)(0x80 | (v & 0x3F)); return 2; }
    if (v < 0x10000) { o[0] = (char)(0xE0 | v >> 12); o[1] = (char)(0x80 | (v >> 6 & 0x3F)); o[2] = (char)(0x80 | (v & 0x3F)); return 3; }
    o[0] = (char)(0xF0 | v >> 18); o[1] = (char)(0x80 | (v >> 12 & 0x3F)); o[2] = (char)(0x80 | (v >> 6 & 0x3F)); o[3] = (char)(0x80 | (v & 0x3F));
    return 4;
}
size_t mbsnrtowcs_l(wchar_t_ *dst, const char **src, size_t nms, size_t len, mbstate_t_ *ps, void *loc) {
    (void)ps; (void)loc;
    const unsigned char *s = (const unsigned char *)*src;
    size_t w = 0, i = 0;
    while (i < nms && (!dst || w < len)) {
        wchar_t_ c;
        int k = utf8_dec(s + i, nms - i, &c);
        if (k < 0) { set_errno(92 /* EILSEQ */); *src = (const char *)(s + i); return (size_t)-1; }
        if (k == 0 || c == 0) { if (dst) { dst[w] = 0; *src = 0; } return w; }
        if (dst) dst[w] = c;
        w++; i += (size_t)k;
    }
    if (dst) *src = (const char *)(s + i);
    return w;
}
size_t mbsrtowcs_l(wchar_t_ *dst, const char **src, size_t len, mbstate_t_ *ps, void *loc) {
    return mbsnrtowcs_l(dst, src, strlen(*src) + 1, len, ps, loc);
}
size_t wcsnrtombs_l(char *dst, const wchar_t_ **src, size_t nwc, size_t len, mbstate_t_ *ps, void *loc) {
    (void)ps; (void)loc;
    const wchar_t_ *s = *src;
    size_t w = 0;
    for (size_t i = 0; i < nwc; i++) {
        char b[4];
        if (!s[i]) { if (dst) { if (w < len) dst[w] = 0; *src = 0; } return w; }
        int k = utf8_enc((unsigned)s[i], b);
        if (dst) {
            if (w + (size_t)k > len) { *src = s + i; return w; }
            memcpy(dst + w, b, (size_t)k);
        }
        w += (size_t)k;
    }
    if (dst) *src = s + nwc;
    return w;
}
int fputc(int c, void *f);
int fgetc(void *f);
int ungetc(int c, void *f);
int fputwc(wchar_t_ wc, void *f) {
    char b[4];
    int k = utf8_enc((unsigned)wc, b);
    for (int i = 0; i < k; i++) if (fputc((unsigned char)b[i], f) < 0) return -1;
    return wc;
}
int getwc(void *f) {
    unsigned char b[4];
    int c = fgetc(f);
    if (c < 0) return -1;
    b[0] = (unsigned char)c;
    int len = c < 0x80 ? 1 : c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : 2;
    for (int i = 1; i < len; i++) { int d = fgetc(f); if (d < 0) return -1; b[i] = (unsigned char)d; }
    wchar_t_ w;
    return utf8_dec(b, (size_t)len, &w) < 0 ? -1 : w;
}
int ungetwc(int wc, void *f) { return wc >= 0 && wc < 0x80 ? ungetc(wc, f) : -1; }
/* formato largo → estreito (ASCII), formata e alarga de volta */
static void narrow(char *o, size_t cap, const wchar_t_ *w) {
    size_t i = 0;
    for (; w && w[i] && i + 1 < cap; i++) o[i] = (char)(w[i] < 0x80 ? w[i] : '?');
    o[i] = 0;
}
static int widen(wchar_t_ *o, size_t cap, const char *s) {
    size_t w = 0, i = 0, n = strlen(s);
    while (i < n && w + 1 < cap) {
        wchar_t_ c;
        int k = utf8_dec((const unsigned char *)s + i, n - i, &c);
        if (k <= 0) { c = (unsigned char)s[i]; k = 1; }
        o[w++] = c; i += (size_t)k;
    }
    if (cap) o[w] = 0;
    return (int)w;
}
int vswprintf(wchar_t_ *s, size_t n, const wchar_t_ *fmt, va_list ap) {
    char f[512], b[2048];
    narrow(f, sizeof f, fmt);
    int r = vsnprintf(b, sizeof b, f, ap);
    if (r < 0) return -1;
    int w = widen(s, n, b);
    return (size_t)r >= n ? -1 : w;
}
size_t wcsftime(wchar_t_ *s, size_t max, const wchar_t_ *fmt, const void *tm) {
    char f[256], b[1024];
    narrow(f, sizeof f, fmt);
    size_t r = strftime(b, sizeof b, f, tm);
    if (!r) return 0;
    int w = widen(s, max, b);
    return (size_t)w < max ? (size_t)w : 0;
}

/* ================================================================ processos, usuários e prioridade */
int getpriority(int which, unsigned who) { (void)which; (void)who; set_errno(0); return 0; }
int setpriority(int which, unsigned who, int prio) { (void)which; (void)who; (void)prio; return 0; }
int initgroups(const char *name, int gid) { (void)name; (void)gid; return 0; }
int setegid(unsigned g) { long r = SYS1(SYS_SETEGID, g); if (r < 0) { set_errno(E_PERM); return -1; } return 0; }
int seteuid(unsigned u) { long r = SYS1(SYS_SETEUID, u); if (r < 0) { set_errno(E_PERM); return -1; } return 0; }
/* getsid: em pty.c (sessões do kernel, SYS_TTY) */
int getgrouplist(const char *name, int base, int *groups, int *ngroups) {
    (void)name;
    if (!ngroups) return -1;
    if (*ngroups < 1) { *ngroups = 1; return -1; }
    groups[0] = base;
    *ngroups = 1;
    return 0;
}
int getdomainname(char *name, int len) { if (len > 0) name[0] = 0; return 0; }
int responsibility_spawnattrs_setdisclaim(void *attr, int disclaim) { (void)attr; (void)disclaim; return 0; }
int posix_spawnattr_set_csm_np(void *attr, u32 flags) { (void)attr; (void)flags; return 0; }
int responsibility_get_pid_responsible_for_pid(int pid) { return pid; }
int proc_pidfdinfo(int pid, int fd, int flavor, void *buf, int size) { (void)pid; (void)fd; (void)flavor; (void)buf; (void)size; set_errno(E_NOTSUP); return 0; }
/* sem shell no caminho de popen: o Electron só usa para utilitários opcionais */
void *popen(const char *cmd, const char *mode) { (void)cmd; (void)mode; set_errno(E_NOSYS); return 0; }
int pclose(void *f) { (void)f; set_errno(10 /* ECHILD */); return -1; }

/* ================================================================ arquivos */
int futimens(int fd, const vs_timespec_t t[2]) { (void)fd; (void)t; return 0; }
int utimensat(int dfd, const char *p, const vs_timespec_t t[2], int flag) {
    (void)t; (void)flag;
    const char *cx_at_path(int dirfd, const char *p, char *buf, size_t cap);   /* posixmore.c: dirfd -> caminho (GNU tar) */
    char tp[1024], st[256];
    const char *q = cx_at_path(dfd, p, tp, sizeof tp);
    return q ? stat(q, st) : -1;
}
void cx_perm_set(const char *path, unsigned mode);   /* libc.c: o stat devolve o modo pedido */
int fchmodat(int dfd, const char *p, unsigned short mode, int flag) {
    (void)flag;
    const char *cx_at_path(int dirfd, const char *p, char *buf, size_t cap);
    char tp[1024], st[256];
    const char *q = cx_at_path(dfd, p, tp, sizeof tp);   /* relativo ao dirfd (GNU tar) */
    if (!q) return -1;
    int r = stat(q, st);
    if (r == 0) cx_perm_set(q, mode);
    return r;
}
int lchflags(const char *p, u32 flags) { (void)flags; char st[256]; return stat(p, st); }
int mknod(const char *p, unsigned short mode, int dev) { (void)p; (void)mode; (void)dev; set_errno(E_PERM); return -1; }
/* o clonefile do APFS: sem CoW aqui; quem chama (VS Code, Chromium) copia o arquivo */
int clonefile(const char *src, const char *dst, u32 flags) { (void)src; (void)dst; (void)flags; set_errno(E_NOTSUP); return -1; }
/* msync: extra.c (grava de volta os mapeamentos MAP_SHARED) */
int munlock(const void *a, size_t n) { (void)a; (void)n; return 0; }
int mincore(const void *a, size_t n, char *vec) {
    (void)a;
    for (size_t i = 0; i < (n + 4095) / 4096; i++) vec[i] = 1;     /* tudo residente (sem swap) */
    return 0;
}
int change_fdguard_np(int fd, const u64 *guard, u32 gflags, const u64 *nguard, u32 nflags, int *fdflagsp) {
    (void)fd; (void)guard; (void)gflags; (void)nguard; (void)nflags; (void)fdflagsp;
    return 0;
}
int guarded_close_np(int fd, const u64 *guard) { (void)guard; return close(fd); }
typedef struct { void *base; size_t len; } vs_iovec_t;
ssize_t preadv(int fd, const vs_iovec_t *iov, int n, off_t off) {
    ssize_t t = 0;
    for (int i = 0; i < n; i++) {
        ssize_t r = pread(fd, iov[i].base, iov[i].len, off + t);
        if (r < 0) return t ? t : r;
        t += r;
        if ((size_t)r < iov[i].len) break;
    }
    return t;
}
ssize_t pwritev(int fd, const vs_iovec_t *iov, int n, off_t off) {
    ssize_t t = 0;
    for (int i = 0; i < n; i++) {
        ssize_t r = pwrite(fd, iov[i].base, iov[i].len, off + t);
        if (r < 0) return t ? t : r;
        t += r;
        if ((size_t)r < iov[i].len) break;
    }
    return t;
}
/* sendfile(arquivo, socket, offset, *len, cabeçalhos, flags) do macOS, por read/write */
int sendfile(int fd, int s, off_t off, off_t *len, void *hdtr, int flags) {
    (void)hdtr; (void)flags;
    off_t want = len && *len ? *len : (off_t)1 << 62, done = 0;
    char b[16384];
    while (done < want) {
        size_t k = (size_t)(want - done < (off_t)sizeof b ? want - done : (off_t)sizeof b);
        ssize_t r = pread(fd, b, k, off + done);
        if (r <= 0) break;
        ssize_t w = write(s, b, (size_t)r);
        if (w < 0) { if (len) *len = done; return -1; }
        done += w;
        if (w < r) break;
    }
    if (len) *len = done;
    return 0;
}
/* scandir com o struct dirent de 64 bits do macOS (o readdir do mOS já devolve esse layout). Exportado como
 * "scandir": o import x86_64 scandir$INODE64 cai nele pela regra de variante do carregador, e o arm64 (que só tem
 * o dirent de 64 bits) importa o nome sem sufixo. */
typedef struct { u64 ino; u64 seekoff; u16 reclen; u16 namlen; u8 type; char name[1024]; } vs_dirent_t;
int scandir64(const char *dir, vs_dirent_t ***out, int (*sel)(const vs_dirent_t *),
              int (*cmp)(const vs_dirent_t **, const vs_dirent_t **)) __asm__("scandir");
int scandir64(const char *dir, vs_dirent_t ***out, int (*sel)(const vs_dirent_t *),
              int (*cmp)(const vs_dirent_t **, const vs_dirent_t **)) {
    void *d = opendir(dir);
    if (!d) return -1;
    int n = 0, cap = 16;
    vs_dirent_t **v = malloc(sizeof(*v) * (size_t)cap);
    vs_dirent_t *e;
    while (v && (e = readdir(d))) {
        if (sel && !sel(e)) continue;
        if (n == cap) { cap *= 2; vs_dirent_t **nv = realloc(v, sizeof(*v) * (size_t)cap); if (!nv) break; v = nv; }
        vs_dirent_t *c = malloc(sizeof *c);
        if (!c) break;
        memcpy(c, e, sizeof *c);
        v[n++] = c;
    }
    closedir(d);
    if (!v) return -1;
    if (cmp) qsort(v, (size_t)n, sizeof(*v), (int (*)(const void *, const void *))cmp);
    *out = v;
    return n;
}
/* getmntinfo: um só sistema de arquivos montado em "/" (statfs64 de 2168 bytes do macOS). Mesmo esquema de nome
 * do scandir: getmntinfo$INODE64 (x86_64) e getmntinfo (arm64) chegam aqui. */
typedef struct {
    u32 bsize; i32 iosize; u64 blocks, bfree, bavail, files, ffree; u64 fsid; u32 owner, type, flags, fssubtype;
    char fstypename[16]; char mntonname[1024]; char mntfromname[1024]; u32 reserved[8];
} vs_statfs_t;
int getmntinfo64(vs_statfs_t **mp, int flags) __asm__("getmntinfo");
int getmntinfo64(vs_statfs_t **mp, int flags) {
    (void)flags;
    static vs_statfs_t fs;
    memset(&fs, 0, sizeof fs);
    fs.bsize = 4096; fs.iosize = 65536; fs.blocks = 1u << 20; fs.bfree = fs.bavail = 1u << 19;
    fs.files = 1u << 20; fs.ffree = 1u << 19; fs.type = 0x1c; fs.flags = 0x00001000 /* MNT_LOCAL */;
    strlcpy(fs.fstypename, "msdos", sizeof fs.fstypename);
    strlcpy(fs.mntonname, "/", sizeof fs.mntonname);
    strlcpy(fs.mntfromname, "/dev/disk0s1", sizeof fs.mntfromname);
    *mp = &fs;
    return 1;
}
void *fopen(const char *p, const char *m);
int mkstemp(char *t);
int unlink(const char *p);
void *fdopen(int fd, const char *m);
void *tmpfile(void) {
    char t[] = "/tmp/mOS-tmpfile-XXXXXX";
    int fd = mkstemp(t);
    if (fd < 0) return fopen("/tmp/mOS-tmpfile", "w+");
    void *f = fdopen(fd, "w+");
    unlink(t);
    return f;
}

/* ================================================================ terminais (termios) e pty
 * cf*speed, tcdrain, posix_openpt, grantpt, unlockpt e ptsname: em pty.c, sobre os pseudoterminais do kernel
 * (kernel/proc/pty.c): o terminal integrado do VS Code abre o shell por eles (node-pty) */

/* ================================================================ rede */
typedef struct { u8 b[16]; } vs_in6_addr_t;
const vs_in6_addr_t in6addr_any = {{0}};
const vs_in6_addr_t in6addr_loopback = {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}};
typedef struct { u8 len, family; u16 port; u32 addr; u8 zero[8]; } vs_sin_t;
typedef struct vs_ifaddrs {
    struct vs_ifaddrs *next; char *name; u32 flags; void *addr, *netmask, *dstaddr; void *data;
} vs_ifaddrs_t;
/* só a interface de loopback e a da rede (virtio/e1000 do mOS, 10.0.2.15 como no QEMU usermode) */
int getifaddrs(vs_ifaddrs_t **out) {
    static const struct { const char *name; u32 flags; u32 ip, mask; } ifs[] = {
        {"lo0", 0x8049 /* UP|LOOPBACK|RUNNING|MULTICAST */, 0x0100007Fu, 0x000000FFu},
        {"en0", 0x8863 /* UP|BROADCAST|RUNNING|SIMPLEX|MULTICAST */, 0x0F02000Au, 0x00FFFFFFu},
    };
    vs_ifaddrs_t *head = 0, **tail = &head;
    for (unsigned i = 0; i < sizeof ifs / sizeof ifs[0]; i++) {
        vs_ifaddrs_t *a = calloc(1, sizeof *a + 2 * sizeof(vs_sin_t));
        if (!a) break;
        vs_sin_t *ad = (vs_sin_t *)(a + 1), *mk = ad + 1;
        ad->len = mk->len = 16; ad->family = mk->family = 2;
        ad->addr = ifs[i].ip; mk->addr = ifs[i].mask;
        a->name = strdup(ifs[i].name);
        a->flags = ifs[i].flags;
        a->addr = ad; a->netmask = mk;
        *tail = a; tail = &a->next;
    }
    *out = head;
    return 0;
}
void freeifaddrs(vs_ifaddrs_t *a) {
    while (a) { vs_ifaddrs_t *n = a->next; free(a->name); free(a); a = n; }
}
char *if_indextoname(unsigned idx, char *buf) {
    const char *n = idx == 1 ? "lo0" : idx == 2 ? "en0" : 0;
    if (!n) { set_errno(6 /* ENXIO */); return 0; }
    strlcpy(buf, n, 16);
    return buf;
}
/* recvmsg_x/sendmsg_x (lotes do QUIC): o Chromium cai para recvmsg/sendmsg com ENOSYS */
ssize_t recvmsg_x(int s, void *msgp, u32 cnt, int flags) { (void)s; (void)msgp; (void)cnt; (void)flags; set_errno(E_NOSYS); return -1; }
ssize_t sendmsg_x(int s, const void *msgp, u32 cnt, int flags) { (void)s; (void)msgp; (void)cnt; (void)flags; set_errno(E_NOSYS); return -1; }
int connectx(int s, const void *eps, u32 aid, u32 flags, const void *iov, u32 iovcnt, size_t *len, u32 *cid) {
    (void)s; (void)eps; (void)aid; (void)flags; (void)iov; (void)iovcnt; (void)len; (void)cid;
    set_errno(E_NOTSUP);
    return -1;
}
int disconnectx(int s, u32 aid, u32 cid) { (void)s; (void)aid; (void)cid; set_errno(E_NOTSUP); return -1; }
void *getservbyport(int port, const char *proto) { (void)port; (void)proto; return 0; }
void *getservbyname(const char *name, const char *proto) { (void)name; (void)proto; return 0; }
/* fileports: o kernel guarda uma cópia do descritor numa porta Mach (machipc.c, KP_FILE), que viaja numa mensagem
 * como qualquer direito de envio; quem recebe ganha um descritor novo para o mesmo arquivo/pipe. O Mojo do Chromium
 * manda arquivos para os helpers assim. Descritores que só existem no libSystem (kqueue 0x4000+, sockets 0x5000+)
 * não têm o que copiar: EBADF, e o motivo vai para o stderr. */
int fileport_makeport(int fd, mach_port_t *port) {
    if (!port) { set_errno(E_INVAL); return -1; }
    u64 a[8] = {(u64)(long)fd, 0, 0, 0, 0, 0, 0, 0};
    long r = cx_syscall(SYS_MACH_PORT, (long)MACH_OP_FILEPORT_MAKE, (long)a, 0, 0, 0);
    if (r != 0) {
        static const char m[] = "fileport_makeport: descritor sem cópia no kernel (kqueue/socket do libSystem ou fechado)\n";
        if (fd >= 0x4000) SYS3(SYS_WRITE, 2, m, sizeof m - 1);
        set_errno(9);                                  /* EBADF */
        return -1;
    }
    *port = (mach_port_t)a[1];
    return 0;
}
int fileport_makefd(mach_port_t port) {
    u64 a[8] = {port, 0, 0, 0, 0, 0, 0, 0};
    long r = cx_syscall(SYS_MACH_PORT, (long)MACH_OP_FILEPORT_FD, (long)a, 0, 0, 0);
    if (r != 0) { set_errno(r == 6 ? 24 : 9); return -1; }   /* KERN_RESOURCE_SHORTAGE -> EMFILE; senão EBADF */
    return (int)a[1];
}
/* notify_register_file_descriptor: svcclient.c (notifyd) */

/* ================================================================ Mach: host, threads, VM */
typedef struct { u32 free_count, active_count, inactive_count, wire_count; u64 zero_fill_count, reactivations, pageins,
                 pageouts, faults, cow_faults, lookups, hits, purges; u32 purgeable_count, speculative_count;
                 u64 decompressions, compressions, swapins, swapouts; u32 compressor_page_count, throttled_count,
                 external_page_count, internal_page_count; u64 total_uncompressed_pages_in_compressor; } vs_vmstat64_t;
#define HOST_VM_INFO 2
#define HOST_VM_INFO64 4
#define HOST_CPU_LOAD_INFO 3
static u32 ncpu(void) { return 1; }
kern_return_t host_statistics64(mach_port_t host, int flavor, void *info, u32 *count) {
    (void)host;
    if (flavor == HOST_VM_INFO64 || flavor == HOST_VM_INFO) {
        u32 n = count ? *count : 0;
        u32 *w = info;
        for (u32 i = 0; i < n; i++) w[i] = 0;
        if (n >= 4) { w[0] = 65536; w[1] = 32768; w[2] = 16384; w[3] = 16384; }   /* páginas livres/ativas/... */
        return KERN_SUCCESS;
    }
    if (flavor == HOST_CPU_LOAD_INFO) {
        u32 *t = info;
        vs_timespec_t ts;
        clock_gettime(6 /* CLOCK_UPTIME_RAW */, &ts);
        u32 ticks = (u32)(ts.tv_sec * 100 + ts.tv_nsec / 10000000);
        t[0] = ticks / 4; t[1] = ticks / 8; t[2] = ticks - ticks / 4 - ticks / 8; t[3] = 0;   /* user, system, idle, nice */
        if (count) *count = 4;
        return KERN_SUCCESS;
    }
    return KERN_INVALID_ADDRESS;
}
kern_return_t host_statistics(mach_port_t host, int flavor, void *info, u32 *count) { return host_statistics64(host, flavor, info, count); }
#define PROCESSOR_CPU_LOAD_INFO 2
kern_return_t host_processor_info(mach_port_t host, int flavor, u32 *ncount, u32 **info, u32 *icount) {
    (void)host;
    if (flavor != PROCESSOR_CPU_LOAD_INFO) return KERN_INVALID_ADDRESS;
    u32 n = ncpu();
    u64 a = 0;
    if (mach_vm_allocate(mach_task_self(), &a, 4096, 1) != KERN_SUCCESS) return KERN_FAILURE;
    u32 *t = (u32 *)(usize)a;
    u32 c = 4;
    host_statistics64(host, HOST_CPU_LOAD_INFO, t, &c);
    *ncount = n;
    *info = t;
    *icount = 4 * n;
    return KERN_SUCCESS;
}
kern_return_t task_name_for_pid(mach_port_t task, int pid, mach_port_t *out) {
    (void)task;
    if (pid == getpid()) { *out = mach_task_self(); return KERN_SUCCESS; }
    return KERN_FAILURE;
}
mach_port_t task_self_trap(void) { return mach_task_self(); }
/* NDR_record do MIG: little-endian, inteiros 2-complemento, IEEE */
const struct { u8 mig_vers, if_vers, reserved1, mig_encoding, int_rep, char_rep, float_rep, reserved2; } NDR_record = {0, 0, 0, 0, 1, 0, 0, 0};
kern_return_t thread_switch(mach_port_t t, int opt, u32 ms) { (void)t; (void)opt; (void)ms; sched_yield(); return KERN_SUCCESS; }
kern_return_t thread_policy_set(mach_port_t t, u32 flavor, void *info, u32 count) { (void)t; (void)flavor; (void)info; (void)count; return KERN_SUCCESS; }
kern_return_t thread_policy_get(mach_port_t t, u32 flavor, void *info, u32 *count, int *get_default) {
    (void)t; (void)flavor;
    if (info && count) memset(info, 0, *count * 4u);
    if (get_default) *get_default = 1;
    return KERN_SUCCESS;
}
/* outras threads do processo (kernel: SYS_THREADCTL). A porta de uma thread é 0x1000 + tid (THREAD_BASE, mach.c).
 * O coletor do JavaScriptCore suspende cada thread que entrou na VM, lê os registradores (x86_THREAD_STATE64) e varre a
 * pilha a partir do rsp. Quando o thread_suspend falhava, ele pulava a thread ("invalid thread") e liberava objetos que
 * ela ainda usava: no Claude Code com 4 CPUs, listas livres do mimalloc com ponteiros 1 GiB abaixo e SIGSEGV. */
#define CX_THREAD_PORT_BASE 0x1000u
#define X86_THREAD_STATE64 4
#define X86_THREAD_STATE64_COUNT 42u               /* 21 u64 em unidades de 32 bits */
#define X86_THREAD_STATE 7                         /* genérico: {flavor, count} + o de 64 bits */
static long port_tid(mach_port_t t) { return t >= CX_THREAD_PORT_BASE ? (long)(t - CX_THREAD_PORT_BASE) : -1; }
kern_return_t thread_get_state(mach_port_t t, int flavor, void *st, u32 *count) {
    long tid = port_tid(t);
    if (tid < 0 || !st || !count) return KERN_INVALID_ARGUMENT;
    u32 *out = st;
    if (flavor == X86_THREAD_STATE) {
        if (*count < X86_THREAD_STATE64_COUNT + 2) return KERN_INVALID_ARGUMENT;
        out[0] = X86_THREAD_STATE64;
        out[1] = X86_THREAD_STATE64_COUNT;
        out += 2;
    } else if (flavor != X86_THREAD_STATE64 || *count < X86_THREAD_STATE64_COUNT) return KERN_INVALID_ARGUMENT;
    u64 regs[21];
    long r = SYS3(SYS_THREADCTL, MOS_THR_GETSTATE, tid, regs);
    if (r < 0) return KERN_FAILURE;            /* não existe mais, ou não está suspensa e parada */
    memcpy(out, regs, sizeof regs);
    *count = flavor == X86_THREAD_STATE ? X86_THREAD_STATE64_COUNT + 2 : X86_THREAD_STATE64_COUNT;
    return KERN_SUCCESS;
}
kern_return_t thread_set_state(mach_port_t t, int flavor, const void *st, u32 count) { (void)t; (void)flavor; (void)st; (void)count; return KERN_NOT_SUPPORTED; }
/* como no XNU, só volta com a thread parada (fora de qualquer CPU): quem chama lê a pilha e os registradores em seguida */
kern_return_t thread_suspend(mach_port_t t) {
    long tid = port_tid(t);
    if (tid < 0) return KERN_INVALID_ARGUMENT;
    if (SYS3(SYS_THREADCTL, MOS_THR_SUSPEND, tid, 0) < 0) return KERN_FAILURE;   /* thread que já terminou */
    if (tid != SYS0(SYS_GETTID))
        while (SYS3(SYS_THREADCTL, MOS_THR_STOPPED, tid, 0) == 0) sched_yield();
    return KERN_SUCCESS;
}
kern_return_t thread_resume(mach_port_t t) {
    long tid = port_tid(t);
    if (tid < 0) return KERN_INVALID_ARGUMENT;
    return SYS3(SYS_THREADCTL, MOS_THR_RESUME, tid, 0) < 0 ? KERN_FAILURE : KERN_SUCCESS;
}
kern_return_t thread_abort_safely(mach_port_t t) { (void)t; return KERN_NOT_SUPPORTED; }
kern_return_t thread_get_register_pointer_values(mach_port_t t, usize *sp, usize *len, usize *vals) {
    (void)t; (void)sp; (void)vals;
    if (len) *len = 0;
    return KERN_NOT_SUPPORTED;
}
kern_return_t task_threads(mach_port_t task, mach_port_t **list, u32 *count) {
    (void)task;
    u64 a = 0;
    if (mach_vm_allocate(mach_task_self(), &a, 4096, 1) != KERN_SUCCESS) return KERN_FAILURE;
    mach_port_t *l = (mach_port_t *)(usize)a;
    l[0] = mach_thread_self();
    *list = l;
    *count = 1;
    return KERN_SUCCESS;
}
/* portas de exceção: aceitas e lembradas como "nenhuma" (o crashpad registra o handler e segue) */
static kern_return_t exc_none(u32 *count) { if (count) *count = 0; return KERN_SUCCESS; }
kern_return_t thread_set_exception_ports(mach_port_t t, u32 mask, mach_port_t p, int b, int f) { (void)t; (void)mask; (void)p; (void)b; (void)f; return KERN_SUCCESS; }
kern_return_t task_set_exception_ports(mach_port_t t, u32 mask, mach_port_t p, int b, int f) { (void)t; (void)mask; (void)p; (void)b; (void)f; return KERN_SUCCESS; }
kern_return_t host_set_exception_ports(mach_port_t t, u32 mask, mach_port_t p, int b, int f) { (void)t; (void)mask; (void)p; (void)b; (void)f; return KERN_SUCCESS; }
kern_return_t thread_get_exception_ports(mach_port_t t, u32 mask, u32 *masks, u32 *count, mach_port_t *ports, int *b, int *f) { (void)t; (void)mask; (void)masks; (void)ports; (void)b; (void)f; return exc_none(count); }
kern_return_t task_get_exception_ports(mach_port_t t, u32 mask, u32 *masks, u32 *count, mach_port_t *ports, int *b, int *f) { (void)t; (void)mask; (void)masks; (void)ports; (void)b; (void)f; return exc_none(count); }
kern_return_t host_get_exception_ports(mach_port_t t, u32 mask, u32 *masks, u32 *count, mach_port_t *ports, int *b, int *f) { (void)t; (void)mask; (void)masks; (void)ports; (void)b; (void)f; return exc_none(count); }
kern_return_t thread_swap_exception_ports(mach_port_t t, u32 mask, mach_port_t p, int b, int f, u32 *masks, u32 *count, mach_port_t *ports, int *ob, int *of) { (void)t; (void)mask; (void)p; (void)b; (void)f; (void)masks; (void)ports; (void)ob; (void)of; return exc_none(count); }
kern_return_t task_swap_exception_ports(mach_port_t t, u32 mask, mach_port_t p, int b, int f, u32 *masks, u32 *count, mach_port_t *ports, int *ob, int *of) { (void)t; (void)mask; (void)p; (void)b; (void)f; (void)masks; (void)ports; (void)ob; (void)of; return exc_none(count); }
kern_return_t host_swap_exception_ports(mach_port_t t, u32 mask, mach_port_t p, int b, int f, u32 *masks, u32 *count, mach_port_t *ports, int *ob, int *of) { (void)t; (void)mask; (void)p; (void)b; (void)f; (void)masks; (void)ports; (void)ob; (void)of; return exc_none(count); }
/* leitura de memória: só da própria task (a única que o mOS deixa ver) */
kern_return_t mach_vm_read(mach_port_t task, u64 addr, u64 size, u64 *data, u32 *count) {
    if (task != mach_task_self()) return KERN_FAILURE;
    u64 a = 0;
    if (mach_vm_allocate(task, &a, size ? size : 1, 1) != KERN_SUCCESS) return KERN_FAILURE;
    memcpy((void *)(usize)a, (const void *)(usize)addr, (size_t)size);
    *data = a;
    *count = (u32)size;
    return KERN_SUCCESS;
}
kern_return_t vm_read_overwrite(mach_port_t task, u64 addr, u64 size, u64 dst, u64 *out) {
    if (task != mach_task_self()) return KERN_FAILURE;
    memmove((void *)(usize)dst, (const void *)(usize)addr, (size_t)size);
    if (out) *out = size;
    return KERN_SUCCESS;
}
kern_return_t mach_vm_remap(mach_port_t t, u64 *addr, u64 size, u64 mask, int flags, mach_port_t src, u64 saddr, int copy,
                            int *cur, int *max, int inh) {
    (void)mask; (void)inh;
    if (src != mach_task_self()) return KERN_NOT_SUPPORTED;
    /* copy=0: as mesmas páginas físicas nos dois endereços (o caminho W^X do V8 no Linux/Windows;
     * no arm64 o V8 usa MAP_JIT, mas mach_vm_remap sem cópia tem que existir). */
    if (!copy) {
        if (!addr || !size || (size & 0xfff) || (saddr & 0xfff) || (*addr & 0xfff)) return KERN_INVALID_ARGUMENT;
        u64 q[8] = {*addr, size, saddr, (u64)flags, 0, 0, 0, 0};
        long r = cx_syscall(SYS_MACH_VM, (long)MACH_VM_REMAP, (long)q, 0, 0, 0);
        if (r) return (kern_return_t)r;
        *addr = q[0];
#if defined(__aarch64__)
        { void a64rt_jit_alias(unsigned long long dst, unsigned long long src, unsigned long long len);
          a64rt_jit_alias(q[0], saddr, size); }
#endif
        if (cur) *cur = 3;
        if (max) *max = 7;
        return KERN_SUCCESS;
    }
    if (mach_vm_allocate(t, addr, size, 1) != KERN_SUCCESS) return KERN_FAILURE;
    memcpy((void *)(usize)*addr, (const void *)(usize)saddr, (size_t)size);
    if (cur) *cur = 3;
    if (max) *max = 7;
    return KERN_SUCCESS;
}
kern_return_t vm_remap(mach_port_t t, u64 *addr, u64 size, u64 mask, int flags, mach_port_t src, u64 saddr, int copy,
                       int *cur, int *max, int inh) { return mach_vm_remap(t, addr, size, mask, flags, src, saddr, copy, cur, max, inh); }
kern_return_t mach_vm_region_recurse(mach_port_t t, u64 *addr, u64 *size, u32 *depth, void *info, u32 *count) {
    (void)t; (void)addr; (void)size; (void)depth; (void)info; (void)count;
    return KERN_INVALID_ADDRESS;
}
kern_return_t mach_vm_purgable_control(mach_port_t t, u64 addr, int control, int *state) {
    (void)t; (void)addr; (void)control;
    if (state) *state = 0;    /* VM_PURGABLE_NONVOLATILE: nada é descartado sob pressão */
    return KERN_SUCCESS;
}
typedef struct { u32 tv_sec; i32 tv_nsec; } vs_mach_ts_t;
kern_return_t host_get_clock_service(mach_port_t host, int id, mach_port_t *clock) { (void)host; *clock = 0x7E000000u | (u32)id; return KERN_SUCCESS; }
kern_return_t clock_get_time(mach_port_t clock, vs_mach_ts_t *t) {
    vs_timespec_t ts;
    clock_gettime((clock & 0xFF) == 1 ? 0 /* CALENDAR = REALTIME */ : 6, &ts);
    t->tv_sec = (u32)ts.tv_sec;
    t->tv_nsec = (i32)ts.tv_nsec;
    return KERN_SUCCESS;
}
struct vs_mstats { size_t bytes_total, chunks_used, bytes_used, chunks_free, bytes_free; };
struct vs_mstats mstats(void) { struct vs_mstats m; memset(&m, 0, sizeof m); return m; }
void *getsectdata(const char *seg, const char *sect, unsigned long *size) { (void)seg; (void)sect; if (size) *size = 0; return 0; }

/* ================================================================ pthread */
void pthread_exit(void *r) __attribute__((noreturn));
void cx_thread_run_dtors(void);                  /* extra.c: destrutores das chaves pthread */
void pthread_exit(void *r) { cx_thread_run_dtors(); SYS1(SYS_THREAD_EXIT, (long)r); for (;;) {} }
int kill(int pid, int s);
/* pthread_t daqui é o tid (pthread_create) ou o bloco da thread (pthread_self, extra.c): cx_pthread_tid acha o tid */
int cx_pthread_kill_tid(long tid, int s);
long cx_pthread_tid(void *t);
int pthread_kill(unsigned long t, int sig) {
    long tid = cx_pthread_tid((void *)t);
    int e = cx_pthread_kill_tid(tid, sig);
    if (e == 3 && sig) return kill(getpid(), sig) ? 3 : 0;   /* tid desconhecido: manda ao processo, como antes */
    return e;
}
void *pthread_override_qos_class_start_np(unsigned long t, unsigned qos, int prio) { (void)t; (void)qos; (void)prio; static int dummy; return &dummy; }
int pthread_override_qos_class_end_np(void *o) { (void)o; return 0; }
int pthread_condattr_init(void *a) { memset(a, 0, 16); return 0; }
int pthread_condattr_destroy(void *a) { if (a) memset(a, 0, 16); return a ? 0 : 22; }
/* O atributo é guardado. Cond_wait/signal deste processo não mudam; entre processos o futex não é compartilhado. */
int pthread_condattr_setpshared(void *a, int p) {
    if (!a || (p != 0 && p != 1)) return 22;
    ((int *)a)[1] = p;
    return 0;
}
int pthread_condattr_getpshared(const void *a, int *p) { if (!a) return 22; if (p) *p = ((const int *)a)[1]; return 0; }
int pthread_setcancelstate(int st, int *old) { (void)st; if (old) *old = 0; return 0; }
/* quadro de pilha: [rbp] = rbp anterior, [rbp+8] = endereço de retorno */
usize pthread_stack_frame_decode_np(usize frame, usize *ret) {
    usize *f = (usize *)frame;
    if (ret) *ret = f[1];
    return f[0];
}

/* ================================================================ ulock (futex do macOS) e semáforos */
/* __ulock_wait(op, addr, valor, timeout_us): dorme enquanto *addr == valor (timeout 0 = sem prazo). Os tipos privados de
 * 32 bits (UL_COMPARE_AND_WAIT 1, UL_UNFAIR_LOCK 2) esperam no futex do kernel (cx_futex_wait, extra.c) e o __ulock_wake
 * os acorda: girando com sched_yield, as threads ociosas do Bun (pool, GC do JavaScriptCore) disputavam a trava grande do
 * kernel com a thread principal. Os de 64 bits (4, 5, 6) e o compartilhado entre processos (3; o futex do kernel é por
 * processo) ainda cedem a CPU em laço, e para eles o __ulock_wake não precisa fazer nada (quem espera vê o valor mudar).
 * ULF_NO_ERRNO (0x01000000): o erro volta como -errno no retorno, sem mexer no errno (o Futex do Zig no Bun do Claude Code
 * usa assim e lia o -1 de antes como -EPERM: "Unexpected __ulock_wait() return code"). Valor já diferente devolve 0, como
 * no XNU: o Zig trata como inalcançável qualquer erro fora EINTR e ETIMEDOUT. */
#define ULF_NO_ERRNO    0x01000000u
#define ULF_WAKE_ALL    0x00000100u
#define ULF_WAKE_THREAD 0x00000200u
int cx_futex_wait(volatile int *w, int expected, long ms);
int cx_futex_wake(volatile int *w, int n);
/* CX_ULOCK_SPIN=1 no ambiente: todos os tipos voltam ao laço com sched_yield (diagnóstico, para comparar com o futex) */
static int ulock_spin(void) {
    static int on = -1;
    if (on < 0) { char *getenv(const char *); on = getenv("CX_ULOCK_SPIN") != 0; }
    return on;
}
static int ulock_private32(u32 op, const void *addr) { u32 k = op & 0xff; return (k == 1 || k == 2) && !((u64)addr & 3) && !ulock_spin(); }
static int ulock_err(u32 op, int e) { if (op & ULF_NO_ERRNO) return -e; set_errno(e); return -1; }
int __ulock_wait(u32 op, void *addr, u64 value, u32 timeout_us) {
    if (!addr) return ulock_err(op, 22 /* EINVAL */);
    if (ulock_private32(op, addr)) {
        int r = cx_futex_wait((volatile int *)addr, (int)(u32)value, timeout_us ? (long)((timeout_us + 999u) / 1000u) : -1);
        if (r == -60) return ulock_err(op, 60 /* ETIMEDOUT */);
        if (r == -4) return ulock_err(op, 4 /* EINTR: o chamador confere o valor e espera de novo */);
        return 0;                                   /* acordada, ou o valor já tinha mudado (-35) */
    }
    u32 k = op & 0xff;
    int wide = k == 4 || k == 5 || k == 6;
    vs_timespec_t t0, t1;
    clock_gettime(6, &t0);
    while (wide ? __atomic_load_n((volatile u64 *)addr, __ATOMIC_ACQUIRE) == value
                : __atomic_load_n((volatile u32 *)addr, __ATOMIC_ACQUIRE) == (u32)value) {
        if (timeout_us) {
            clock_gettime(6, &t1);
            long us = (t1.tv_sec - t0.tv_sec) * 1000000 + (t1.tv_nsec - t0.tv_nsec) / 1000;
            if (us >= (long)timeout_us) return ulock_err(op, 60 /* ETIMEDOUT */);
        }
        sched_yield();
    }
    return 0;
}
/* acorda uma (ou todas, com ULF_WAKE_ALL) das threads no futex; ULF_WAKE_THREAD (uma thread certa, pela porta) acorda
 * todas, e as outras voltam a esperar. Sempre 0: o -ENOENT do XNU (ninguém esperando) não muda nada para quem chama */
int __ulock_wake(u32 op, void *addr, u64 wake_value) {
    (void)wake_value;
    if (addr && ulock_private32(op, addr)) cx_futex_wake((volatile int *)addr, (op & (ULF_WAKE_ALL | ULF_WAKE_THREAD)) ? 0x7FFFFFFF : 1);
    return 0;
}
typedef struct vs_sem { char name[64]; volatile int value; struct vs_sem *next; } vs_sem_t;
static vs_sem_t *sems;
static volatile int sem_lock;
static void sl_lock(void) { while (__atomic_exchange_n(&sem_lock, 1, __ATOMIC_ACQUIRE)) sched_yield(); }
static void sl_unlock(void) { __atomic_store_n(&sem_lock, 0, __ATOMIC_RELEASE); }
void *sem_open(const char *name, int oflag, ...) {
    unsigned value = 0;
    if (oflag & 0x200 /* O_CREAT */) { va_list ap; va_start(ap, oflag); (void)va_arg(ap, int); value = va_arg(ap, unsigned); va_end(ap); }
    sl_lock();
    for (vs_sem_t *s = sems; s; s = s->next)
        if (!strcmp(s->name, name)) {
            sl_unlock();
            if ((oflag & 0x200) && (oflag & 0x800 /* O_EXCL */)) { set_errno(17); return (void *)-1; }
            return s;
        }
    if (!(oflag & 0x200)) { sl_unlock(); set_errno(E_NOENT); return (void *)-1; }
    vs_sem_t *s = calloc(1, sizeof *s);
    if (!s) { sl_unlock(); return (void *)-1; }
    strlcpy(s->name, name, sizeof s->name);
    s->value = (int)value;
    s->next = sems;
    sems = s;
    sl_unlock();
    return s;
}
int sem_close(void *s) { (void)s; return 0; }
int sem_post(void *p) { vs_sem_t *s = p; __atomic_add_fetch(&s->value, 1, __ATOMIC_RELEASE); return 0; }
int sem_wait(void *p) {
    vs_sem_t *s = p;
    for (;;) {
        int v = __atomic_load_n(&s->value, __ATOMIC_ACQUIRE);
        if (v > 0 && __atomic_compare_exchange_n(&s->value, &v, v - 1, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) return 0;
        sched_yield();
    }
}

/* ================================================================ OSAtomic (libkern) e libatomic */
i32 OSAtomicAdd32(i32 v, volatile i32 *p) { return __atomic_add_fetch(p, v, __ATOMIC_RELAXED); }
i32 OSAtomicIncrement32(volatile i32 *p) { return __atomic_add_fetch(p, 1, __ATOMIC_RELAXED); }
i32 OSAtomicDecrement32(volatile i32 *p) { return __atomic_sub_fetch(p, 1, __ATOMIC_RELAXED); }
i32 OSAtomicIncrement32Barrier(volatile i32 *p) { return __atomic_add_fetch(p, 1, __ATOMIC_SEQ_CST); }
i32 OSAtomicDecrement32Barrier(volatile i32 *p) { return __atomic_sub_fetch(p, 1, __ATOMIC_SEQ_CST); }
i32 OSAtomicAnd32Barrier(u32 m, volatile u32 *p) { return (i32)__atomic_and_fetch(p, m, __ATOMIC_SEQ_CST); }
i32 OSAtomicOr32Barrier(u32 m, volatile u32 *p) { return (i32)__atomic_or_fetch(p, m, __ATOMIC_SEQ_CST); }
int OSAtomicCompareAndSwap32Barrier(i32 o, i32 n, volatile i32 *p) { return __atomic_compare_exchange_n(p, &o, n, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); }
int OSAtomicCompareAndSwapPtrBarrier(void *o, void *n, void *volatile *p) { return __atomic_compare_exchange_n(p, &o, n, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); }
void OSMemoryBarrier(void) { __atomic_thread_fence(__ATOMIC_SEQ_CST); }
/* __atomic_load/__atomic_store genéricos (objetos de tamanho qualquer): uma trava global */
static volatile int ga_lock;
void vs_atomic_load(size_t n, void *src, void *dst, int order) __asm__("__atomic_load");
void vs_atomic_load(size_t n, void *src, void *dst, int order) {
    (void)order;
    while (__atomic_exchange_n(&ga_lock, 1, __ATOMIC_ACQUIRE)) {}
    memcpy(dst, src, n);
    __atomic_store_n(&ga_lock, 0, __ATOMIC_RELEASE);
}
void vs_atomic_store(size_t n, void *dst, void *src, int order) __asm__("__atomic_store");
void vs_atomic_store(size_t n, void *dst, void *src, int order) {
    (void)order;
    while (__atomic_exchange_n(&ga_lock, 1, __ATOMIC_ACQUIRE)) {}
    memcpy(dst, src, n);
    __atomic_store_n(&ga_lock, 0, __ATOMIC_RELEASE);
}
int memset_s(void *s, size_t smax, int c, size_t n) {
    volatile u8 *p = s;
    size_t k = n < smax ? n : smax;
    for (size_t i = 0; i < k; i++) p[i] = (u8)c;
    return n > smax ? 84 /* EOVERFLOW */ : 0;
}

/* ================================================================ libdispatch */
char _dispatch_queue_attr_concurrent[8];       /* DISPATCH_QUEUE_CONCURRENT: só o endereço importa */
char _dispatch_source_type_memorypressure[8];  /* fontes que dispatch_source_create ainda devolve NULL */
char _dispatch_source_type_proc[8];
typedef void (*vs_dfn_t)(void *);
#define DCTX_MAX 256
static struct { void *obj, *ctx; vs_dfn_t fin; } dctx[DCTX_MAX];
static int dctx_slot(void *o, int make) {
    for (int i = 0; i < DCTX_MAX; i++) if (dctx[i].obj == o) return i;
    if (!make) return -1;
    for (int i = 0; i < DCTX_MAX; i++) if (!dctx[i].obj) { dctx[i].obj = o; return i; }
    return -1;
}
void dispatch_set_context(void *o, void *ctx) { int i = dctx_slot(o, 1); if (i >= 0) dctx[i].ctx = ctx; }
void dispatch_set_finalizer_f(void *o, vs_dfn_t f) { int i = dctx_slot(o, 1); if (i >= 0) dctx[i].fin = f; }
void *dispatch_get_context(void *o) { int i = dctx_slot(o, 0); return i >= 0 ? dctx[i].ctx : 0; }
/* dispatch_walltime(when, delta): tempo de parede em ns com o bit de "walltime" (negativo), como o libdispatch */
u64 dispatch_walltime(const vs_timespec_t *when, i64 delta) {
    u64 ns;
    if (when) ns = (u64)when->tv_sec * 1000000000ull + (u64)when->tv_nsec;
    else { vs_timespec_t t; clock_gettime(0, &t); ns = (u64)t.tv_sec * 1000000000ull + (u64)t.tv_nsec; }
    ns += (u64)delta;
    if (ns >= (1ull << 62)) return ~0ull;     /* DISPATCH_TIME_FOREVER */
    return (u64)(-(i64)ns);
}

/* qsort_b está em sortb.c (heapsort via qsort_r, bloco no thunk). A trava global antiga
 * ficava presa se o comparador saía por exceção ou longjmp, e um qsort_b aninhado girava. */

/* ================================================================ números aleatórios */
int rand_r(unsigned *seed) { *seed = *seed * 1103515245u + 12345u; return (int)((*seed >> 16) & 0x7FFF); }
static u64 rand48_x = 0x1234ABCD330Eull;
static u32 rand48_next(void) { rand48_x = (rand48_x * 0x5DEECE66Dull + 0xB) & 0xFFFFFFFFFFFFull; return (u32)(rand48_x >> 16); }
void srand48(long s) { rand48_x = ((u64)(u32)s << 16 | 0x330E) & 0xFFFFFFFFFFFFull; }
long mrand48(void) { return (long)(i32)rand48_next(); }

/* ================================================================ matemática */
long timezone = 0;     /* o mOS roda em UTC */
int daylight = 0;
int __math_errhandling(void) { return 2; }    /* MATH_ERREXCEPT */
double nan(const char *tag) { (void)tag; return __builtin_nan(""); }
float truncf(float x) { return (float)(long long)x == x || x != x ? x : (float)(x < 0 ? -floor(-(double)x) : floor((double)x)); }
float nearbyintf(float x) { return (float)rint((double)x); }
long lroundf(float x) { return (long)(x < 0 ? -floor(-(double)x + 0.5) : floor((double)x + 0.5)); }
long double powl(long double b, long double e) { return (long double)pow((double)b, (double)e); }
float __exp10f(float x) { return (float)pow(10.0, (double)x); }
float frexpf(float x, int *e) {
    union { float f; u32 u; } v = {x};
    int ex = (int)(v.u >> 23 & 0xFF);
    if (!ex) { if (x == 0) { *e = 0; return x; } v.f = x * 8388608.0f; ex = (int)(v.u >> 23 & 0xFF) - 23; *e = ex - 126; v.u = (v.u & 0x807FFFFFu) | 0x3F000000u; return v.f; }
    if (ex == 0xFF) { *e = 0; return x; }
    *e = ex - 126;
    v.u = (v.u & 0x807FFFFFu) | 0x3F000000u;
    return v.f;
}
float ldexpf(float x, int e) {
    double r = (double)x;
    while (e > 0) { r *= 2; e--; }
    while (e < 0) { r /= 2; e++; }
    return (float)r;
}
int ilogbf(float x) {
    if (x == 0) return (int)0x80000001;
    int e;
    frexpf(x, &e);
    return e - 1;
}
int __fpclassifyf(float x) {
    union { float f; u32 u; } v = {x};
    u32 ex = v.u >> 23 & 0xFF, m = v.u & 0x7FFFFF;
    if (ex == 0xFF) return m ? 1 /* FP_NAN */ : 2 /* FP_INFINITE */;
    if (ex == 0) return m ? 5 /* FP_SUBNORMAL */ : 3 /* FP_ZERO */;
    return 4;   /* FP_NORMAL */
}
#if defined(__aarch64__)
/* arm64 (fenv.h da Apple): FE_INVALID 1, DIVBYZERO 2, OVERFLOW 4, UNDERFLOW 8, INEXACT 16 e FE_FLUSHTOZERO 0x80 são os
 * bits do FPSR (IOC DZC OFC UFC IXC, IDC no bit 7) */
int feclearexcept(int e) {
    u64 m;
    __asm__ volatile("mrs %0, fpsr" : "=r"(m));
    m &= ~(u64)(e & 0x9F);
    __asm__ volatile("msr fpsr, %0" : : "r"(m));
    return 0;
}
int feraiseexcept(int e) {
    u64 m;
    __asm__ volatile("mrs %0, fpsr" : "=r"(m));
    m |= (u64)(e & 0x9F);
    __asm__ volatile("msr fpsr, %0" : : "r"(m));
    return 0;
}
#else
int feclearexcept(int e) {
    u32 m;
    __asm__ volatile("stmxcsr %0" : "=m"(m));
    m &= ~(u32)(e & 0x3F);
    __asm__ volatile("ldmxcsr %0" : : "m"(m));
    return 0;
}
int feraiseexcept(int e) {
    u32 m;
    __asm__ volatile("stmxcsr %0" : "=m"(m));
    m |= (u32)(e & 0x3F);
    __asm__ volatile("ldmxcsr %0" : : "m"(m));
    return 0;
}
#endif
/* fetestexcept: o Zig (compiler_rt) consulta as flags de exceção do ponto flutuante */
int fetestexcept(int e) {
#if defined(__aarch64__)
    u64 m;
    __asm__ volatile("mrs %0, fpsr" : "=r"(m));
    return (int)(m & (u64)(e & 0x9F));
#else
    u32 m;
    __asm__ volatile("stmxcsr %0" : "=m"(m));
    return (int)(m & (u32)(e & 0x3F));
#endif
}

/* ================================================================ sandbox (constantes de extensão) */
const char *const APP_SANDBOX_READ = "com.apple.app-sandbox.read";
const char *const APP_SANDBOX_READ_WRITE = "com.apple.app-sandbox.read-write";

/* ================================================================ libunwind (só o crashpad/backtrace usa) */
/* unw_*: API pública em cxx_unwind.c, em cima do mesmo motor de CFI/compact unwind. */

/* ================================================================ libc++: std::to_string(int) (kerberos.node) */
/* std::string do libc++ x86_64: curto = byte 0 com tamanho << 1, texto a partir do byte 1 (até 22 chars) */
typedef struct { u8 b[24]; } vs_stdstring_t;
vs_stdstring_t vs_to_string_i(int v) __asm__("_ZNSt3__19to_stringEi");
vs_stdstring_t vs_to_string_i(int v) {
    vs_stdstring_t s;
    memset(&s, 0, sizeof s);
    char t[16];
    int n = 0;
    unsigned u = v < 0 ? 0u - (unsigned)v : (unsigned)v;
    do { t[n++] = (char)('0' + u % 10); u /= 10; } while (u);
    if (v < 0) t[n++] = '-';
    s.b[0] = (u8)(n << 1);
    for (int i = 0; i < n; i++) s.b[1 + i] = (u8)t[n - 1 - i];
    return s;
}

/* ================================================================ xlocale (_l) que a libc++ do LLVM importa */
/* o mOS só tem a locale "C"/UTF-8: as variantes _l delegam para as funções sem locale */
int __tolower(int c);
int strcoll(const char *a, const char *b);
size_t strxfrm(char *d, const char *s, size_t n);
int wcscoll(const wchar_t_ *a, const wchar_t_ *b);
wchar_t_ btowc(int c);
int wctob(wchar_t_ c);
void *localeconv(void);
int vs___tolower_l(int c, void *loc) __asm__("___tolower_l");
int vs___tolower_l(int c, void *loc) { (void)loc; return __tolower(c); }
int vs___toupper_l(int c, void *loc) __asm__("___toupper_l");
int vs___toupper_l(int c, void *loc) { (void)loc; return c >= 'a' && c <= 'z' ? c - 32 : c; }
wchar_t_ btowc_l(int c, void *loc) { (void)loc; return btowc(c); }
int wctob_l(wchar_t_ c, void *loc) { (void)loc; return wctob(c); }
void *localeconv_l(void *loc) { (void)loc; return localeconv(); }
int strcoll_l(const char *a, const char *b, void *loc) { (void)loc; return strcoll(a, b); }
size_t strxfrm_l(char *d, const char *s, size_t n, void *loc) { (void)loc; return strxfrm(d, s, n); }
int wcscoll_l(const wchar_t_ *a, const wchar_t_ *b, void *loc) { (void)loc; return wcscoll(a, b); }
size_t wcsxfrm_l(wchar_t_ *d, const wchar_t_ *s, size_t n, void *loc) {
    (void)loc;
    size_t l = 0;
    while (s[l]) l++;
    if (n) { size_t k = l < n - 1 ? l : n - 1; for (size_t i = 0; i < k; i++) d[i] = s[i]; d[k] = 0; }
    return l;
}
