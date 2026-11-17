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
kern_return_t mach_vm_deallocate(mach_port_t task, u64 addr, u64 size);
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
/* vswprintf: libc_corpus3.c */
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
/* sem shell no caminho de popen: o Electron só usa para utilitários opcionais */
void *popen(const char *cmd, const char *mode) { (void)cmd; (void)mode; set_errno(E_NOSYS); return 0; }
int pclose(void *f) { (void)f; set_errno(10 /* ECHILD */); return -1; }

/* ================================================================ arquivos */
/* futimens, utimensat e lchflags: fsmeta.c (SYS_FSMETA) */
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
int mknod(const char *p, unsigned short mode, int dev) { (void)p; (void)mode; (void)dev; set_errno(E_PERM); return -1; }
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
/* getmntinfo: attrlist.c (um por volume) */
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
/* getifaddrs, freeifaddrs, if_indextoname e if_nametoindex: netif.c (lista real do kernel) */
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
        /* vm_statistics(64): free, active, inactive, wire, depois contadores. Números do kernel (SYS_VMCTL).
         * Layout do vm_statistics64 (38 palavras): u64 a partir da palavra 4 (zero_fill, reactivations, pageins, pageouts...),
         * decompressions 24, compressions 26, compressor_page_count 32, internal 35, total_uncompressed 36. */
        u32 n = count ? *count : 0;
        u32 *w = info;
        for (u32 i = 0; i < n; i++) w[i] = 0;
        mos_vmstats_t st;
        if (SYS2(SYS_VMCTL, MOS_VM_STATS, &st) != 0) { st.free_pages = 65536; st.resident_pages = 32768; st.total_pages = 131072; st.compressed_pages = st.compressor_pages = st.pageins = st.pageouts = 0; }
        u64 wired = st.total_pages > st.free_pages + st.resident_pages ? st.total_pages - st.free_pages - st.resident_pages : 0;
        if (n >= 4) { w[0] = (u32)st.free_pages; w[1] = (u32)st.resident_pages; w[2] = 0; w[3] = (u32)wired; }
        if (flavor == HOST_VM_INFO64 && n >= 38) {
            u64 *q = (u64 *)(w + 4);
            q[2] = st.pageins; q[3] = st.pageouts;                      /* pageins, pageouts (u64) */
            *(u64 *)(w + 24) = st.pageins;                              /* decompressions */
            *(u64 *)(w + 26) = st.pageouts;                             /* compressions */
            w[32] = (u32)st.compressor_pages;                           /* compressor_page_count */
            w[35] = (u32)st.resident_pages;                             /* internal_page_count */
            *(u64 *)(w + 36) = st.compressed_pages;                     /* total_uncompressed_pages_in_compressor */
        }
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
static long port_tid(mach_port_t t) { return t >= CX_THREAD_PORT_BASE && t < 0x10000u ? (long)(t - CX_THREAD_PORT_BASE) : -1; }
/* operação MOS_THR_* numa thread: a do próprio processo (0x1000 + tid) pelo SYS_THREADCTL; uma porta de thread de
 * outra task (recebida numa exceção ou no task_threads de outra task) pelo kernel Mach (MACH_VM_THREAD_CTL) */
static long thr_ctl(mach_port_t t, unsigned op, void *arg) {
    long tid = port_tid(t);
    if (tid >= 0) return SYS3(SYS_THREADCTL, op, tid, arg);
    u64 q[8] = {t, op, (u64)(usize)arg, 0, 0, 0, 0, 0};
    if (cx_syscall(SYS_MACH_VM, (long)MACH_VM_THREAD_CTL, (long)q, 0, 0, 0) != KERN_SUCCESS) return -MOS_ESRCH;
    return (long)q[3];
}
static int own_thread(mach_port_t t) { return port_tid(t) >= 0; }
kern_return_t thread_get_state(mach_port_t t, int flavor, void *st, u32 *count) {
    if (!t || !st || !count) return KERN_INVALID_ARGUMENT;
    u32 *out = st;
    if (flavor == X86_THREAD_STATE) {
        if (*count < X86_THREAD_STATE64_COUNT + 2) return KERN_INVALID_ARGUMENT;
        out[0] = X86_THREAD_STATE64;
        out[1] = X86_THREAD_STATE64_COUNT;
        out += 2;
    } else if (flavor != X86_THREAD_STATE64 || *count < X86_THREAD_STATE64_COUNT) return KERN_INVALID_ARGUMENT;
    u64 regs[21];
    long r = thr_ctl(t, MOS_THR_GETSTATE, regs);
    if (r < 0) return KERN_FAILURE;            /* não existe mais, ou não está suspensa e parada */
    memcpy(out, regs, sizeof regs);
    *count = flavor == X86_THREAD_STATE ? X86_THREAD_STATE64_COUNT + 2 : X86_THREAD_STATE64_COUNT;
    return KERN_SUCCESS;
}
/* thread_set_state: a thread tem de estar suspensa e fora da CPU (o mesmo layout do get) */
kern_return_t thread_set_state(mach_port_t t, int flavor, const void *st, u32 count) {
    const u32 *in = st;
    if (!t || !st) return KERN_INVALID_ARGUMENT;
    if (flavor == X86_THREAD_STATE) {
        if (count < X86_THREAD_STATE64_COUNT + 2 || in[0] != X86_THREAD_STATE64) return KERN_INVALID_ARGUMENT;
        in += 2;
    } else if (flavor != X86_THREAD_STATE64 || count < X86_THREAD_STATE64_COUNT) return KERN_INVALID_ARGUMENT;
    u64 regs[21];
    memcpy(regs, in, sizeof regs);
    return thr_ctl(t, MOS_THR_SETSTATE, regs) < 0 ? KERN_FAILURE : KERN_SUCCESS;
}
/* como no XNU, só volta com a thread parada (fora de qualquer CPU): quem chama lê a pilha e os registradores em seguida */
kern_return_t thread_suspend(mach_port_t t) {
    if (!t) return KERN_INVALID_ARGUMENT;
    if (thr_ctl(t, MOS_THR_SUSPEND, 0) < 0) return KERN_FAILURE;   /* thread que já terminou */
    if (!own_thread(t) || port_tid(t) != SYS0(SYS_GETTID))
        while (thr_ctl(t, MOS_THR_STOPPED, 0) == 0) sched_yield();
    return KERN_SUCCESS;
}
kern_return_t thread_resume(mach_port_t t) {
    if (!t) return KERN_INVALID_ARGUMENT;
    return thr_ctl(t, MOS_THR_RESUME, 0) < 0 ? KERN_FAILURE : KERN_SUCCESS;
}
/* a espera em curso da thread termina: EINTR numa syscall, MACH_RCV_INTERRUPTED num mach_msg */
kern_return_t thread_abort(mach_port_t t) { return !t ? KERN_INVALID_ARGUMENT : thr_ctl(t, MOS_THR_ABORT, 0) < 0 ? KERN_FAILURE : KERN_SUCCESS; }
kern_return_t thread_abort_safely(mach_port_t t) { return thread_abort(t); }
/* thread_terminate: a própria sai como no pthread_exit; outra, pelo kernel */
kern_return_t thread_terminate(mach_port_t t) {
    long tid = port_tid(t);
    if (!t) return KERN_INVALID_ARGUMENT;
    if (tid == SYS0(SYS_GETTID)) { SYS1(SYS_THREAD_EXIT, 0); for (;;) {} }
    return thr_ctl(t, MOS_THR_TERMINATE, 0) < 0 ? KERN_FAILURE : KERN_SUCCESS;
}
kern_return_t thread_get_register_pointer_values(mach_port_t t, usize *sp, usize *len, usize *vals) {
    u64 regs[21];
    if (!len) return KERN_INVALID_ARGUMENT;
    if (thr_ctl(t, MOS_THR_GETSTATE, regs) < 0) { *len = 0; return KERN_FAILURE; }
    if (sp) *sp = (usize)regs[7];
    /* os registradores que podem guardar ponteiros (todos menos rsp, rip e rflags), como o XNU */
    static const int idx[16] = {0, 1, 2, 3, 4, 5, 6, 8, 9, 10, 11, 12, 13, 14, 15, 18};
    usize cap = *len, n = 0;
    for (int i = 0; i < 16; i++) { if (vals && n < cap) vals[n] = (usize)regs[idx[i]]; n++; }
    *len = n;
    return vals && cap < n ? 52 /* KERN_INSUFFICIENT_BUFFER_SIZE */ : KERN_SUCCESS;
}
/* todas as threads da task: a própria (0x1000 + tid) ou outra task (portas de thread novas no nosso espaço) */
kern_return_t task_threads(mach_port_t task, mach_port_t **list, u32 *count) {
    if (!list || !count) return KERN_INVALID_ARGUMENT;
    for (u32 cap = 64;; cap *= 4) {
        u64 a = 0;
        if (mach_vm_allocate(mach_task_self(), &a, ((u64)cap * 4 + 0xFFF) & ~0xFFFULL, 1) != KERN_SUCCESS) return 6 /* KERN_RESOURCE_SHORTAGE */;
        u64 q[8] = {task ? task : mach_task_self(), a, cap, 0, 0, 0, 0, 0};
        kern_return_t kr = (kern_return_t)cx_syscall(SYS_MACH_VM, (long)MACH_VM_TASK_THREADS, (long)q, 0, 0, 0);
        if (kr != KERN_SUCCESS || q[3] > cap) {
            mach_vm_deallocate(mach_task_self(), a, ((u64)cap * 4 + 0xFFF) & ~0xFFFULL);
            if (kr != KERN_SUCCESS) return kr;
            continue;                                  /* mais threads que o buffer: pede de novo com mais espaço */
        }
        *list = (mach_port_t *)(usize)a;
        *count = (u32)q[3];
        return KERN_SUCCESS;
    }
}
/* portas de exceção da task e das threads: guardadas pelo kernel, que manda exception_raise / mach_exception_raise
 * na falta (antes do sinal) e EXC_CRASH na morte por sinal (kernel/proc/machipc.c, machipc_raise) */
typedef struct { unsigned mask, port, behavior, flavor; } cx_excp_t;
static kern_return_t exc_set(mach_port_t target, int thread, u32 mask, mach_port_t p, int b, int f) {
    u64 q[8] = {target, mask, p, (u64)(u32)b, (u64)(u32)f, (u64)thread, 0, 0};
    return (kern_return_t)cx_syscall(SYS_MACH_VM, (long)MACH_VM_EXC_SET, (long)q, 0, 0, 0);
}
static kern_return_t exc_get(mach_port_t target, int thread, u32 mask, u32 *masks, u32 *count, mach_port_t *ports, int *b, int *f) {
    cx_excp_t e[14];
    u32 cap = count ? *count : 0;
    u64 q[8] = {target, mask, (u64)(usize)e, 14, 0, (u64)thread, 0, 0};
    kern_return_t kr = (kern_return_t)cx_syscall(SYS_MACH_VM, (long)MACH_VM_EXC_GET, (long)q, 0, 0, 0);
    if (kr != KERN_SUCCESS) { if (count) *count = 0; return kr; }
    u32 n = (u32)q[4] < cap ? (u32)q[4] : cap;
    for (u32 i = 0; i < n; i++) {
        if (masks) masks[i] = e[i].mask;
        if (ports) ports[i] = e[i].port;
        if (b) b[i] = (int)e[i].behavior;
        if (f) f[i] = (int)e[i].flavor;
    }
    if (count) *count = n;
    return KERN_SUCCESS;
}
kern_return_t thread_set_exception_ports(mach_port_t t, u32 mask, mach_port_t p, int b, int f) { return exc_set(t, 1, mask, p, b, f); }
kern_return_t task_set_exception_ports(mach_port_t t, u32 mask, mach_port_t p, int b, int f) { return exc_set(t ? t : mach_task_self(), 0, mask, p, b, f); }
/* porta de exceção do host: só com host_priv no XNU; aceita e não guarda (nada a entregar para fora da task) */
kern_return_t host_set_exception_ports(mach_port_t t, u32 mask, mach_port_t p, int b, int f) { (void)t; (void)mask; (void)p; (void)b; (void)f; return KERN_SUCCESS; }
kern_return_t thread_get_exception_ports(mach_port_t t, u32 mask, u32 *masks, u32 *count, mach_port_t *ports, int *b, int *f) { return exc_get(t, 1, mask, masks, count, ports, b, f); }
kern_return_t task_get_exception_ports(mach_port_t t, u32 mask, u32 *masks, u32 *count, mach_port_t *ports, int *b, int *f) { return exc_get(t ? t : mach_task_self(), 0, mask, masks, count, ports, b, f); }
kern_return_t host_get_exception_ports(mach_port_t t, u32 mask, u32 *masks, u32 *count, mach_port_t *ports, int *b, int *f) { (void)t; (void)mask; (void)masks; (void)ports; (void)b; (void)f; if (count) *count = 0; return KERN_SUCCESS; }
kern_return_t thread_swap_exception_ports(mach_port_t t, u32 mask, mach_port_t p, int b, int f, u32 *masks, u32 *count, mach_port_t *ports, int *ob, int *of) {
    kern_return_t kr = exc_get(t, 1, mask, masks, count, ports, ob, of);
    return kr != KERN_SUCCESS ? kr : exc_set(t, 1, mask, p, b, f);
}
kern_return_t task_swap_exception_ports(mach_port_t t, u32 mask, mach_port_t p, int b, int f, u32 *masks, u32 *count, mach_port_t *ports, int *ob, int *of) {
    if (!t) t = mach_task_self();
    kern_return_t kr = exc_get(t, 0, mask, masks, count, ports, ob, of);
    return kr != KERN_SUCCESS ? kr : exc_set(t, 0, mask, p, b, f);
}
kern_return_t host_swap_exception_ports(mach_port_t t, u32 mask, mach_port_t p, int b, int f, u32 *masks, u32 *count, mach_port_t *ports, int *ob, int *of) { (void)t; (void)mask; (void)p; (void)b; (void)f; (void)masks; (void)ports; (void)ob; (void)of; if (count) *count = 0; return KERN_SUCCESS; }
kern_return_t task_suspend(mach_port_t t) {
    if (t == mach_task_self()) return KERN_SUCCESS;   /* a própria: pararia quem chama (o Chromium só suspende outra) */
    u64 q[8] = {t, 1, 0, 0, 0, 0, 0, 0};
    return (kern_return_t)cx_syscall(SYS_MACH_VM, (long)MACH_VM_TASK_SUSPEND, (long)q, 0, 0, 0);
}
kern_return_t task_resume(mach_port_t t) {
    if (t == mach_task_self()) return KERN_SUCCESS;
    u64 q[8] = {t, 0, 0, 0, 0, 0, 0, 0};
    return (kern_return_t)cx_syscall(SYS_MACH_VM, (long)MACH_VM_TASK_SUSPEND, (long)q, 0, 0, 0);
}
/* memória de outra task, pela porta dela (o crashpad lê o processo que caiu); a própria, direto */
static kern_return_t task_rw(mach_port_t task, u64 addr, u64 local, u64 size, int write) {
    u64 q[8] = {task, addr, local, size, (u64)write, 0, 0, 0};
    return (kern_return_t)cx_syscall(SYS_MACH_VM, (long)MACH_VM_TASK_RW, (long)q, 0, 0, 0);
}
kern_return_t mach_vm_read(mach_port_t task, u64 addr, u64 size, u64 *data, u32 *count) {
    if (!data || !count) return KERN_INVALID_ARGUMENT;
    u64 a = 0;
    if (mach_vm_allocate(mach_task_self(), &a, size ? size : 1, 1) != KERN_SUCCESS) return 6 /* KERN_RESOURCE_SHORTAGE */;
    if (task == mach_task_self()) memcpy((void *)(usize)a, (const void *)(usize)addr, (size_t)size);
    else {
        kern_return_t kr = task_rw(task, addr, a, size, 0);
        if (kr != KERN_SUCCESS) { mach_vm_deallocate(mach_task_self(), a, size ? size : 1); return kr; }
    }
    *data = a;
    *count = (u32)size;
    return KERN_SUCCESS;
}
kern_return_t vm_read_overwrite(mach_port_t task, u64 addr, u64 size, u64 dst, u64 *out) {
    if (task == mach_task_self()) memmove((void *)(usize)dst, (const void *)(usize)addr, (size_t)size);
    else {
        kern_return_t kr = task_rw(task, addr, dst, size, 0);
        if (kr != KERN_SUCCESS) return kr;
    }
    if (out) *out = size;
    return KERN_SUCCESS;
}
kern_return_t mach_vm_write(mach_port_t task, u64 addr, u64 data, u32 count) {
    if (task == mach_task_self()) { memmove((void *)(usize)addr, (const void *)(usize)data, count); return KERN_SUCCESS; }
    return task_rw(task, addr, data, count, 1);
}
kern_return_t vm_write(mach_port_t task, u64 addr, u64 data, u32 count) { return mach_vm_write(task, addr, data, count); }
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
/* clock_alarm pede ao serviço de relógio uma mensagem Mach na hora marcada; o mOS não tem esse serviço */
kern_return_t clock_alarm(mach_port_t clock, int type, vs_mach_ts_t t, mach_port_t reply) { (void)clock; (void)type; (void)t; (void)reply; return KERN_NOT_SUPPORTED; }
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
/* Cancelamento adiado: pthread_cancel marca a thread e ela sai (PTHREAD_CANCELED) no próximo pthread_testcancel com o
 * cancelamento ligado. As outras funções que no macOS são pontos de cancelamento (read, sleep, cond_wait...) não olham a
 * marca. Tabela fixa de tids: com ela cheia, pthread_cancel devolve EAGAIN em vez de perder o pedido. */
#define CX_CANCEL_MAX 128
static volatile long cancel_pend[CX_CANCEL_MAX];         /* tid + 1 com cancelamento pedido */
static volatile long cancel_off[CX_CANCEL_MAX];          /* tid + 1 com PTHREAD_CANCEL_DISABLE */
static int tid_tem(volatile long *tab, long tid) { for (int i = 0; i < CX_CANCEL_MAX; i++) if (tab[i] == tid + 1) return i; return -1; }
static int tid_poe(volatile long *tab, long tid) {
    if (tid_tem(tab, tid) >= 0) return 0;
    for (int i = 0; i < CX_CANCEL_MAX; i++) if (__sync_bool_compare_and_swap(&tab[i], 0, tid + 1)) return 0;
    return 35;                                           /* EAGAIN */
}
static void tid_tira(volatile long *tab, long tid) { int i = tid_tem(tab, tid); if (i >= 0) __sync_bool_compare_and_swap(&tab[i], tid + 1, 0); }
int pthread_setcancelstate(int st, int *old) {
    long me = SYS0(SYS_GETTID);
    if (st != 0 && st != 1) return 22;                   /* macOS: PTHREAD_CANCEL_ENABLE 1, DISABLE 0 */
    if (old) *old = tid_tem(cancel_off, me) >= 0 ? 0 : 1;
    if (st == 0) return tid_poe(cancel_off, me);
    tid_tira(cancel_off, me);
    return 0;
}
/* tipo de cancelamento (deferido 2 / assíncrono 0): o mOS só cancela em pontos (deferido), então guarda o valor
 * para devolver mas trata tudo como deferido. Devolve o antigo (padrão: deferido). */
int pthread_setcanceltype(int type, int *old) {
    if (type != 0 && type != 2) return 22;               /* PTHREAD_CANCEL_ASYNCHRONOUS 0, DEFERRED 2 */
    if (old) *old = 2;
    return 0;
}
int pthread_cancel(unsigned long t) {
    long tid = cx_pthread_tid((void *)t);
    if (tid <= 0) return 3;                              /* ESRCH */
    return tid_poe(cancel_pend, tid);
}
void pthread_testcancel(void) {
    long me = SYS0(SYS_GETTID);
    if (tid_tem(cancel_pend, me) < 0 || tid_tem(cancel_off, me) >= 0) return;
    tid_tira(cancel_pend, me);
    pthread_exit((void *)1);                             /* PTHREAD_CANCELED */
}
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
/* sem_open e os outros semáforos POSIX nomeados: posix_ipc.c */

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
/* variantes que os apps do corpus (firefox, obs, thunderbird, vlc) importam: add32/64 com barreira, CAS de int/long/
 * ptr/64 com barreira, test-and-set/clear e o OSSpinLock */
i32 OSAtomicAdd32Barrier(i32 v, volatile i32 *p) { return __atomic_add_fetch(p, v, __ATOMIC_SEQ_CST); }
i64 OSAtomicAdd64(i64 v, volatile i64 *p) { return __atomic_add_fetch(p, v, __ATOMIC_RELAXED); }
i64 OSAtomicAdd64Barrier(i64 v, volatile i64 *p) { return __atomic_add_fetch(p, v, __ATOMIC_SEQ_CST); }
i64 OSAtomicIncrement64(volatile i64 *p) { return __atomic_add_fetch(p, 1, __ATOMIC_RELAXED); }
i64 OSAtomicIncrement64Barrier(volatile i64 *p) { return __atomic_add_fetch(p, 1, __ATOMIC_SEQ_CST); }
i64 OSAtomicDecrement64(volatile i64 *p) { return __atomic_sub_fetch(p, 1, __ATOMIC_RELAXED); }
i64 OSAtomicDecrement64Barrier(volatile i64 *p) { return __atomic_sub_fetch(p, 1, __ATOMIC_SEQ_CST); }
int OSAtomicCompareAndSwapIntBarrier(int o, int n, volatile int *p) { return __atomic_compare_exchange_n(p, &o, n, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); }
int OSAtomicCompareAndSwapInt(int o, int n, volatile int *p) { return __atomic_compare_exchange_n(p, &o, n, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED); }
int OSAtomicCompareAndSwapLongBarrier(long o, long n, volatile long *p) { return __atomic_compare_exchange_n(p, &o, n, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); }
int OSAtomicCompareAndSwapLong(long o, long n, volatile long *p) { return __atomic_compare_exchange_n(p, &o, n, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED); }
int OSAtomicCompareAndSwap64Barrier(i64 o, i64 n, volatile i64 *p) { return __atomic_compare_exchange_n(p, &o, n, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); }
int OSAtomicCompareAndSwap64(i64 o, i64 n, volatile i64 *p) { return __atomic_compare_exchange_n(p, &o, n, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED); }
int OSAtomicCompareAndSwapPtr(void *o, void *n, void *volatile *p) { return __atomic_compare_exchange_n(p, &o, n, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED); }
i32 OSAtomicAnd32(u32 m, volatile u32 *p) { return (i32)__atomic_and_fetch(p, m, __ATOMIC_RELAXED); }
i32 OSAtomicOr32(u32 m, volatile u32 *p) { return (i32)__atomic_or_fetch(p, m, __ATOMIC_RELAXED); }
i32 OSAtomicXor32(u32 m, volatile u32 *p) { return (i32)__atomic_xor_fetch(p, m, __ATOMIC_RELAXED); }
i32 OSAtomicXor32Barrier(u32 m, volatile u32 *p) { return (i32)__atomic_xor_fetch(p, m, __ATOMIC_SEQ_CST); }
/* bit n (ordem big-endian por byte, como no libkern): devolve o valor antigo do bit */
static int os_bit(u32 n, volatile void *addr, int set, int barrier) {
    volatile u8 *b = (volatile u8 *)addr + (n >> 3);
    u8 mask = (u8)(0x80 >> (n & 7));
    u8 old = set ? __atomic_fetch_or(b, mask, barrier ? __ATOMIC_SEQ_CST : __ATOMIC_RELAXED)
                 : __atomic_fetch_and(b, (u8)~mask, barrier ? __ATOMIC_SEQ_CST : __ATOMIC_RELAXED);
    return (old & mask) != 0;
}
int OSAtomicTestAndSet(u32 n, volatile void *a) { return os_bit(n, a, 1, 0); }
int OSAtomicTestAndSetBarrier(u32 n, volatile void *a) { return os_bit(n, a, 1, 1); }
int OSAtomicTestAndClear(u32 n, volatile void *a) { return os_bit(n, a, 0, 0); }
int OSAtomicTestAndClearBarrier(u32 n, volatile void *a) { return os_bit(n, a, 0, 1); }
/* OSSpinLock (int32: 0 livre): giro simples, como a libkern. OSSpinLockTry devolve true se travou */
void OSSpinLockLock(volatile i32 *l) { while (__atomic_exchange_n(l, 1, __ATOMIC_ACQUIRE)) while (*l) __asm__ __volatile__("" ::: "memory"); }
void OSSpinLockUnlock(volatile i32 *l) { __atomic_store_n(l, 0, __ATOMIC_RELEASE); }
int OSSpinLockTry(volatile i32 *l) { return __atomic_exchange_n(l, 1, __ATOMIC_ACQUIRE) == 0; }
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
/* atributos, tipos de fonte, contexto/finalizador e dispatch_walltime: dispatch.c e dispatch_source.c */

/* qsort_b está em sortb.c (heapsort via qsort_r, bloco no thunk). A trava global antiga
 * ficava presa se o comparador saía por exceção ou longjmp, e um qsort_b aninhado girava. */

/* ================================================================ números aleatórios */
int rand_r(unsigned *seed) { *seed = *seed * 1103515245u + 12345u; return (int)((*seed >> 16) & 0x7FFF); }
/* gerador da família drand48 (srand48/drand48/lrand48/mrand48/erand48/nrand48/jrand48/seed48/lcong48): rand48.c */

/* ================================================================ matemática */
long timezone = 0;     /* o mOS roda em UTC */
int daylight = 0;
/* libm e fenv: userland/third_party/musl e fenv.c */

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
