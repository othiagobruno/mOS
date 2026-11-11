/*
 * mOS — libSystem: o que o Codex CLI (binário Rust, codex-x86_64-apple-darwin) importa e o mOS não tinha.
 * Medido com tools/machocheck.c (docs/compat-apps.md): 87 funções e constantes; vários servem também aos Electron
 * (FSEventStream* do policy-watcher do VS Code, IOPMAssertion*, CFError*, proxies do Chromium).
 *
 * Regra (a mesma de chrome.c/security.c): quando o mOS não tem o recurso, devolve o erro que a API documenta,
 * nunca sucesso sem preencher a saída. O que funciona de verdade: termios guardado por processo (tcgetattr/
 * tcsetattr/cfmakeraw), proc_pidpath/proc_listchildpids pela lista de processos do kernel, waitid sobre waitpid,
 * killpg, strsignal, umask, CFError, CFURL (as que faltavam em nsurl.c), CFTimeZone, CFPreferences (prefs.c),
 * proxies = "sem proxy". FSEvents (criar/editar/apagar) está em fsevents.c.
 */
#include "sys.h"

size_t strlen(const char *s);
size_t strlcpy(char *d, const char *s, size_t n);
int strcmp(const char *a, const char *b);
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void free(void *p);
char *getenv(const char *name);
int isatty(int fd);
int getpid(void);
int access(const char *p, int m);
int waitpid(int pid, int *st, int opt);
int _NSGetExecutablePath(char *buf, u32 *size);
void srandom(unsigned s);
u32 arc4random(void);
int cx_snprintf(char *buf, size_t n, const char *fmt, ...);
extern int cx_errno;
#define CX_ENOTSUP 45
#define CX_ECHILD 10
#define CX_ENOATTR 93
int open(const char *path, int flags, ...);
ssize_t read(int fd, void *b, size_t n);
ssize_t write(int fd, const void *b, size_t n);
int close(int fd);
int stat(const char *p, void *st);
int lstat(const char *p, void *st);
int fstat(int fd, void *st);
int kill(int pid, int sig);
int statfs(const char *path, void *buf);
int futimes(int fd, const void *tv);
char *strdup(const char *s);
const char *cx_at_path(int dirfd, const char *p, char *buf, size_t cap);
void cx_perm_set(const char *path, unsigned mode);

/* termios, pseudoterminais, getpgid e killpg: em pty.c (sobre o SYS_TTY do kernel) */

/* ================================================================ processos */
/* waitid(idtype, id, siginfo_t*, options) sobre waitpid. siginfo_t do macOS: signo errno code pid uid status ... */
#define P_ALL 0
#define P_PID 1
#define WEXITED 4
#define WNOHANG 1
#define WNOWAIT 0x20
int waitid(int idtype, unsigned id, int *info, int options) {
    if (options & WNOWAIT) { cx_errno = CX_EINVAL; return -1; }   /* não dá para espiar sem colher no mOS */
    int pid = idtype == P_PID ? (int)id : -1;
    if (idtype != P_PID && idtype != P_ALL) { cx_errno = CX_EINVAL; return -1; }
    int st = 0;
    int r = waitpid(pid, &st, options & WNOHANG);
    if (r < 0) return -1;
    if (info) {
        memset(info, 0, 104);                                   /* sizeof(siginfo_t) no macOS */
        if (r > 0) {
            info[0] = 20;                                       /* SIGCHLD */
            int sig = st & 0x7f;
            info[2] = sig ? 2 : 1;                              /* CLD_KILLED : CLD_EXITED */
            info[3] = r;
            info[4] = 501;
            info[5] = sig ? sig : (st >> 8) & 0xff;
        }
    }
    return 0;
}
static const char *const signames[32] = {
    "Unknown signal: 0", "Hangup: 1", "Interrupt: 2", "Quit: 3", "Illegal instruction: 4", "Trace/BPT trap: 5",
    "Abort trap: 6", "EMT trap: 7", "Floating point exception: 8", "Killed: 9", "Bus error: 10",
    "Segmentation fault: 11", "Bad system call: 12", "Broken pipe: 13", "Alarm clock: 14", "Terminated: 15",
    "Urgent I/O condition: 16", "Suspended (signal): 17", "Suspended: 18", "Continued: 19", "Child exited: 20",
    "Stopped (tty input): 21", "Stopped (tty output): 22", "I/O possible: 23", "Cputime limit exceeded: 24",
    "Filesize limit exceeded: 25", "Virtual timer expired: 26", "Profiling timer expired: 27",
    "Window size changes: 28", "Information request: 29", "User defined signal 1: 30", "User defined signal 2: 31",
};
char *strsignal(int sig) {
    static char other[32];
    if (sig >= 0 && sig < 32) return (char *)signames[sig];
    cx_snprintf(other, sizeof other, "Unknown signal: %d", sig);
    return other;
}
/* libproc: pela lista de processos do kernel (SYS_PROCLIST) */
static int proc_find(int pid, mos_procinfo_t *out) {
    mos_procinfo_t pi;
    for (long i = 0; SYS2(SYS_PROCLIST, i, &pi) == 1; i++) if (pi.pid == pid) { if (out) *out = pi; return 1; }
    return 0;
}
int proc_pidpath(int pid, char *buf, u32 size) {
    if (!buf || !size) { cx_errno = CX_EINVAL; return 0; }
    if (pid == getpid()) {
        u32 n = size;
        if (_NSGetExecutablePath(buf, &n) != 0) { cx_errno = CX_ENOMEM; return 0; }
        return (int)strlen(buf);
    }
    mos_procinfo_t pi;
    if (!proc_find(pid, &pi)) { cx_errno = CX_ESRCH; return 0; }
    pi.name[sizeof pi.name - 1] = 0;
    return (int)strlcpy(buf, pi.name, size) < (int)size ? (int)strlen(buf) : 0;   /* o kernel só guarda o nome curto */
}
/* proc_name: o último componente do caminho (o MAXCOMLEN do XNU é 16; quem pede mais recebe o nome inteiro) */
int proc_name(int pid, void *buf, u32 size) {
    char path[1024];
    if (!buf || !size) { cx_errno = CX_EINVAL; return 0; }
    if (proc_pidpath(pid, path, sizeof path) <= 0) return 0;
    const char *b = path;
    for (const char *p = path; *p; p++) if (*p == '/') b = p + 1;
    strlcpy(buf, b, size);
    return (int)strlen(buf);
}
int proc_listchildpids(int ppid, int *buf, int bytes) {
    int n = 0, cap = buf ? bytes / (int)sizeof(int) : 0;
    mos_procinfo_t pi;
    for (long i = 0; SYS2(SYS_PROCLIST, i, &pi) == 1; i++)
        if (pi.ppid == ppid) { if (n < cap) buf[n] = pi.pid; n++; }
    return buf ? (n < cap ? n : cap) * (int)sizeof(int) : n;   /* com buf: bytes preenchidos (como o libproc) */
}
int proc_listpgrppids(int pgrp, int *buf, int bytes) {
    /* cada processo é o próprio grupo */
    if (!proc_find(pgrp, 0)) return 0;
    if (buf && bytes >= (int)sizeof(int)) { buf[0] = pgrp; return (int)sizeof(int); }
    return 1;
}
/* proc_listpids(tipo, info, buf, bytes): PROC_ALL_PIDS 1, PGRP 2, TTY 3, UID 4, RUID 5, PPID 6. Sem buf: bytes
 * necessários (com folga, como o XNU). O mOS tem um usuário e cada processo é o próprio grupo. */
unsigned getuid(void);
int proc_listpids(u32 type, u32 info, void *buf, int bytes) {
    int n = 0, cap = buf ? bytes / (int)sizeof(int) : 0, *out = buf;
    mos_procinfo_t pi;
    for (long i = 0; SYS2(SYS_PROCLIST, i, &pi) == 1; i++) {
        int ok = type == 1 || type == 3 || ((type == 4 || type == 5) && info == getuid()) ||
                 (type == 2 && (u32)pi.pid == info) || (type == 6 && (u32)pi.ppid == info);
        if (!ok) continue;
        if (n < cap) out[n] = pi.pid;
        n++;
    }
    if (!buf) return (n + 16) * (int)sizeof(int);
    return (n < cap ? n : cap) * (int)sizeof(int);
}
int proc_listallpids(void *buf, int bytes) { return proc_listpids(1, 0, buf, bytes) / (int)sizeof(int); }   /* em pids */
/* proc_pidinfo: PROC_PIDTBSDINFO (3, proc_bsdinfo, 136 bytes), PROC_PIDTASKINFO (4, proc_taskinfo, 96),
 * PROC_PIDTASKALLINFO (2, as duas, 232) e PROC_PIDT_SHORTBSDINFO (13, proc_bsdshortinfo, 64), pela lista do
 * kernel (nome curto, estado, memória e CPU). Os outros sabores: ENOTSUP, 0. Devolve os bytes escritos. */
static void fill_bsdinfo(u8 *b, const mos_procinfo_t *pi) {
    memset(b, 0, 136);
    u32 *w = (u32 *)b;
    unsigned uid = getuid();
    w[1] = pi->state == 1 || pi->state == 0 ? 2 : 3;            /* pbi_status: SRUN / SSLEEP */
    w[3] = (u32)pi->pid; w[4] = (u32)pi->ppid;
    w[5] = w[7] = w[9] = uid; w[6] = w[8] = w[10] = 20;          /* uid, gid (staff), ruid, rgid, svuid, svgid */
    strlcpy((char *)b + 48, pi->name, 16);                       /* pbi_comm[MAXCOMLEN] */
    strlcpy((char *)b + 64, pi->name, 32);                       /* pbi_name[2*MAXCOMLEN] */
    w[25] = (u32)pi->pid;                                        /* pbi_pgid */
}
static void fill_taskinfo(u8 *b, const mos_procinfo_t *pi) {
    memset(b, 0, 96);
    u64 *q = (u64 *)b;
    q[0] = pi->mem_kb * 1024 * 4;                                /* virtual_size: sem medida, uma estimativa folgada */
    q[1] = pi->mem_kb * 1024;                                    /* resident_size */
    q[2] = pi->cpu_ms * 1000000ULL;                              /* total_user (ns) */
    u32 *w = (u32 *)(b + 48);
    w[0] = 0;                                                    /* policy */
    w[9] = 1; w[10] = pi->state == 1 ? 1 : 0; w[11] = 31;        /* threadnum, numrunning, priority */
}
int proc_pidinfo(int pid, int flavor, u64 arg, void *buf, int size) {
    (void)arg;
    mos_procinfo_t pi;
    int need = flavor == 3 ? 136 : flavor == 4 ? 96 : flavor == 2 ? 232 : flavor == 13 ? 64 : 0;
    if (!need) { cx_errno = CX_ENOTSUP; return 0; }
    if (!proc_find(pid, &pi)) { cx_errno = CX_ESRCH; return 0; }
    pi.name[sizeof pi.name - 1] = 0;
    if (!buf || size < need) { cx_errno = CX_ENOMEM; return 0; }
    u8 *b = buf;
    if (flavor == 3) fill_bsdinfo(b, &pi);
    else if (flavor == 4) fill_taskinfo(b, &pi);
    else if (flavor == 2) { fill_bsdinfo(b, &pi); fill_taskinfo(b + 136, &pi); }
    else {
        u8 t[136];
        fill_bsdinfo(t, &pi);
        memset(b, 0, 64);
        u32 *w = (u32 *)b, *s = (u32 *)t;
        w[0] = s[3]; w[1] = s[4]; w[2] = s[25]; w[3] = s[1];     /* pid, ppid, pgid, status */
        memcpy(b + 16, t + 48, 16);                              /* comm */
        w[9] = s[5]; w[10] = s[6]; w[11] = s[7]; w[12] = s[8]; w[13] = s[9]; w[14] = s[10];
    }
    return need;
}
/* rusage_info_v0..v6 de bsd/sys/resource.h (XNU). O prefixo é o v0: 16 bytes de uuid e 10 u64.
 * ri_user_time sai em nanossegundos (cpu_ms do kernel); o Mach usa tempo absoluto. O mOS não tem
 * esse relógio, então a unidade aqui é 1 ns. ri_resident_size e ri_phys_footprint são mem_kb*1024. */
static const int rusage_sz[] = {96, 144, 160, 232, 296, 304, 400};
int proc_pid_rusage(int pid, int flavor, void *buf) {
    mos_procinfo_t pi;
    if (!buf) { cx_errno = CX_EFAULT; return -1; }
    if (flavor < 0 || flavor > 6) { cx_errno = CX_EINVAL; return -1; }
    if (!proc_find(pid, &pi)) { cx_errno = CX_ESRCH; return -1; }
    unsigned char *b = buf;
    memset(b, 0, (size_t)rusage_sz[flavor]);
    unsigned long long *q = (unsigned long long *)(b + 16);
    q[0] = pi.cpu_ms * 1000000ULL;
    q[6] = pi.mem_kb * 1024ULL;
    q[7] = q[6];
    return 0;
}
int proc_signal_with_audittoken(const unsigned *tok, int sig) {
    if (!tok) { cx_errno = CX_EINVAL; return -1; }
    return kill((int)tok[5], sig);
}

/* ================================================================ arquivos */
static unsigned cur_umask = 022;
unsigned umask(unsigned m) { unsigned o = cur_umask; cur_umask = m & 0777; return o; }
/* travas de arquivo: o mOS tem um usuário e nenhum outro processo trava os mesmos arquivos; aceitar é o útil */
int flock(int fd, int op) { (void)fd; (void)op; return 0; }
int lockf(int fd, int cmd, long len) { (void)fd; (void)cmd; (void)len; return 0; }
/* SYS_FTRUNCATE (o SQLite trunca o journal a cada commit; antes isto dava ENOTSUP e o perfil do Chrome não abria) */
int ftruncate(int fd, long len) {
    if (len < 0) { cx_errno = CX_EINVAL; return -1; }
    long r = SYS2(SYS_FTRUNCATE, fd, len);
    if (r < 0) { cx_errno = (int)-r; return -1; }
    return 0;
}
/* fdopendir: libc.c (o caminho do fd vem do SYS_FDPATH) */
/* fclonefileat: o FAT não tem clone (COW). O conteúdo é copiado byte a byte para um arquivo novo.
 * Destino existente -> EEXIST, como o clonefile. CLONE_NOFOLLOW não muda o fd, que já está aberto. */
int fclonefileat(int src, int dirfd, const char *dst, unsigned flags) {
    (void)flags;
    if (!dst) { cx_errno = CX_EINVAL; return -1; }
    char sp[1024], dp[1024];
    if (SYS3(SYS_FDPATH, src, sp, sizeof sp) < 0) { cx_errno = CX_EBADF; return -1; }
    const char *full = cx_at_path(dirfd, dst, dp, sizeof dp);
    if (!full) return -1;
    char st[144];
    if (stat(full, st) == 0) { cx_errno = CX_EEXIST; return -1; }
    int in = open(sp, 0);
    if (in < 0) return -1;
    int out = open(full, MAC_O_WRONLY | MAC_O_CREAT | MAC_O_EXCL, 0644);
    if (out < 0) { close(in); return -1; }
    char buf[16384];
    int rc = 0;
    for (;;) {
        ssize_t n = read(in, buf, sizeof buf);
        if (n < 0) { rc = -1; break; }
        if (n == 0) break;
        for (ssize_t o = 0; o < n;) {
            ssize_t w = write(out, buf + o, (size_t)(n - o));
            if (w <= 0) { rc = -1; break; }
            o += w;
        }
        if (rc) break;
    }
    close(in);
    close(out);
    return rc;
}
/* fsctl: o caminho é conferido. Opcode de clone/snapshot não existe neste FAT; ENOTSUP é o que o
 * macOS devolve para um opcode que o volume não implementa. */
int fsctl(const char *p, unsigned long req, void *data, unsigned opts) {
    (void)req; (void)data; (void)opts;
    char st[144];
    if (!p) { cx_errno = CX_EFAULT; return -1; }
    if (stat(p, st) < 0) return -1;
    cx_errno = CX_ENOTSUP;
    return -1;
}
int fsetattrlist(int fd, void *al, void *buf, size_t n, unsigned opts) {
    (void)opts;
    typedef struct { unsigned short bitmapcount, reserved; unsigned common, vol, dir, file, fork; } al_t;
    const al_t *a = al;
    char path[1024];
    if (!a || !buf) { cx_errno = CX_EFAULT; return -1; }
    if (a->bitmapcount != 5) { cx_errno = CX_EINVAL; return -1; }
    if (SYS3(SYS_FDPATH, fd, path, sizeof path) < 0) { cx_errno = CX_EBADF; return -1; }
    /* bits que o FAT guarda de verdade: modo (cx_perm_set, o stat devolve). Horários: futimes é no-op
     * (o volume não persiste tempo); o pedido é aceito. Qualquer outro bit continua ENOTSUP. */
    const unsigned supported = 0x200u | 0x400u | 0x800u | 0x1000u | 0x20000u;
    if (a->vol || a->dir || a->file || a->fork || (a->common & ~supported)) { cx_errno = CX_ENOTSUP; return -1; }
    const unsigned char *p = buf;
    size_t off = 0;
    unsigned mode = 0;
    int saw_mode = 0, saw_time = 0;
    for (unsigned bit = 1; bit; bit <<= 1) {
        if (!(a->common & bit)) continue;
        size_t sz = bit == 0x20000u ? 4 : 16;
        if (off + sz > n) { cx_errno = CX_ERANGE; return -1; }
        if (bit == 0x20000u) { memcpy(&mode, p + off, 4); saw_mode = 1; }
        else saw_time = 1;
        off += sz;
    }
    if (saw_mode) cx_perm_set(path, mode & 07777);
    if (saw_time) futimes(fd, 0);
    return 0;
}

#define CP_MAGIC 0x43504653u
typedef struct {
    u32 magic;
    int src_fd, dst_fd;
    char *src, *dst;
    void *quar, *status_cb, *status_ctx;
    long long copied;
    int cloned;
    u32 bsize, src_bsize, dst_bsize;
} cpstate_t;
void *copyfile_state_alloc(void) {
    cpstate_t *s = calloc(1, sizeof *s);
    if (!s) return 0;
    s->magic = CP_MAGIC;
    s->src_fd = s->dst_fd = -1;
    return s;
}
int copyfile_state_free(void *p) {
    cpstate_t *s = p;
    if (!s || s->magic != CP_MAGIC) { cx_errno = CX_EINVAL; return -1; }
    free(s->src); free(s->dst);
    s->magic = 0;
    free(s);
    return 0;
}
static int cp_ok(void *p, cpstate_t **o) {
    cpstate_t *s = p;
    if (!s || s->magic != CP_MAGIC) { cx_errno = CX_EINVAL; return -1; }
    *o = s;
    return 0;
}
int copyfile_state_get(void *p, u32 flag, void *dst) {
    cpstate_t *s;
    if (cp_ok(p, &s) < 0 || !dst) { cx_errno = CX_EINVAL; return -1; }
    switch (flag) {
    case 1: *(int *)dst = s->src_fd; break;
    case 2: *(char **)dst = s->src; break;
    case 3: *(int *)dst = s->dst_fd; break;
    case 4: *(char **)dst = s->dst; break;
    case 5: *(void **)dst = s->quar; break;
    case 6: *(void **)dst = s->status_cb; break;
    case 7: *(void **)dst = s->status_ctx; break;
    case 8: *(long long *)dst = s->copied; break;
    case 9: *(char **)dst = 0; break;          /* xattr em cópia: o FAT não tem */
    case 10: *(int *)dst = s->cloned; break;
    case 11: *(u32 *)dst = s->src_bsize; break;
    case 12: *(u32 *)dst = s->dst_bsize; break;
    case 13: *(u32 *)dst = s->bsize; break;
    default: cx_errno = CX_EINVAL; return -1;
    }
    return 0;
}
int copyfile_state_set(void *p, u32 flag, const void *src) {
    cpstate_t *s;
    if (cp_ok(p, &s) < 0) return -1;
    switch (flag) {
    case 1: if (!src) { cx_errno = CX_EINVAL; return -1; } s->src_fd = *(const int *)src; break;
    case 2: free(s->src); s->src = src ? strdup((const char *)src) : 0; break;
    case 3: if (!src) { cx_errno = CX_EINVAL; return -1; } s->dst_fd = *(const int *)src; break;
    case 4: free(s->dst); s->dst = src ? strdup((const char *)src) : 0; break;
    case 5: s->quar = (void *)src; break;
    case 6: s->status_cb = (void *)src; break;
    case 7: s->status_ctx = (void *)src; break;
    case 8: if (!src) { cx_errno = CX_EINVAL; return -1; } s->copied = *(const long long *)src; break;
    case 10: if (!src) { cx_errno = CX_EINVAL; return -1; } s->cloned = *(const int *)src; break;
    case 11: if (!src) { cx_errno = CX_EINVAL; return -1; } s->src_bsize = *(const u32 *)src; break;
    case 12: if (!src) { cx_errno = CX_EINVAL; return -1; } s->dst_bsize = *(const u32 *)src; break;
    case 13: if (!src) { cx_errno = CX_EINVAL; return -1; } s->bsize = *(const u32 *)src; break;
    default: cx_errno = CX_EINVAL; return -1;
    }
    return 0;
}
void cx_copyfile_note(void *p, int src_fd, const char *src, int dst_fd, const char *dst, long copied) {
    cpstate_t *s = p;
    if (!s || s->magic != CP_MAGIC) return;
    if (src_fd >= 0) s->src_fd = src_fd;
    if (dst_fd >= 0) s->dst_fd = dst_fd;
    if (src) { free(s->src); s->src = strdup(src); }
    if (dst) { free(s->dst); s->dst = strdup(dst); }
    if (copied >= 0) { s->copied = copied; s->cloned = 0; }
}

/* getfsstat$INODE64: o carregador casa com getfsstat. Um volume, o mesmo de statfs. */
int getfsstat(void *buf, int bufsize, int flags) {
    (void)flags;
    if (!buf) return 1;
    if (bufsize < 2168) return 0;
    return statfs("/", buf) < 0 ? -1 : 1;
}

/* ACL no processo (o FAT não guarda). Chave: st_ino, o mesmo de stat e fstat. Sem ACL -> ENOENT. */
#define ACL_MAGIC 0x41434C31u
#define ENT_MAGIC 0x41434C45u
#define ACL_MAXE 32
typedef struct { u32 magic; int tag; u64 perm; } acl_ent_t;
typedef struct { u32 magic; int n, cursor; acl_ent_t *e[ACL_MAXE]; } acl_t;
static struct { u64 ino; acl_t *acl; int used; } acl_tab[64];

static int acl_ino(int follow, int fd, const char *path, u64 *ino) {
    char st[144];
    int r = fd >= 0 ? fstat(fd, st) : follow ? stat(path, st) : lstat(path, st);
    if (r < 0) return -1;
    *ino = *(u64 *)(st + 8);
    return 0;
}
static acl_t *acl_new(void) {
    acl_t *a = calloc(1, sizeof *a);
    if (a) a->magic = ACL_MAGIC;
    return a;
}
static acl_t *acl_clone(const acl_t *s) {
    if (!s || s->magic != ACL_MAGIC) return 0;
    acl_t *a = acl_new();
    if (!a) return 0;
    for (int i = 0; i < s->n; i++) {
        acl_ent_t *e = malloc(sizeof *e);
        if (!e) break;
        *e = *s->e[i];
        a->e[a->n++] = e;
    }
    return a;
}
static void acl_release(acl_t *a) {
    if (!a) return;
    for (int i = 0; i < a->n; i++) free(a->e[i]);
    a->magic = 0;
    free(a);
}
static acl_t **acl_slot(u64 ino, int create) {
    int free_i = -1;
    for (int i = 0; i < 64; i++) {
        if (acl_tab[i].used && acl_tab[i].ino == ino) return &acl_tab[i].acl;
        if (!acl_tab[i].used && free_i < 0) free_i = i;
    }
    if (!create || free_i < 0) return 0;
    acl_tab[free_i].used = 1;
    acl_tab[free_i].ino = ino;
    acl_tab[free_i].acl = 0;
    return &acl_tab[free_i].acl;
}
void *acl_init(int n) {
    if (n < 0) { cx_errno = CX_EINVAL; return 0; }
    (void)n;
    return acl_new();
}
int acl_free(void *p) {
    acl_t *a = p;
    if (!a) return 0;
    if (a->magic != ACL_MAGIC) { free(a); return 0; }
    acl_release(a);
    return 0;
}
int acl_valid(void *p) {
    acl_t *a = p;
    if (!a || a->magic != ACL_MAGIC || a->n < 0 || a->n > ACL_MAXE) { cx_errno = CX_EINVAL; return -1; }
    for (int i = 0; i < a->n; i++) {
        acl_ent_t *e = a->e[i];
        if (!e || e->magic != ENT_MAGIC || (e->tag != 1 && e->tag != 2)) { cx_errno = CX_EINVAL; return -1; }
    }
    return 0;
}
int acl_create_entry(void **acl, void **ent) {
    acl_t *a = acl ? *acl : 0;
    if (!a || a->magic != ACL_MAGIC || a->n >= ACL_MAXE) { cx_errno = CX_EINVAL; return -1; }
    acl_ent_t *e = calloc(1, sizeof *e);
    if (!e) { cx_errno = CX_ENOMEM; return -1; }
    e->magic = ENT_MAGIC;
    e->tag = 0;
    a->e[a->n++] = e;
    if (ent) *ent = e;
    return 0;
}
int acl_set_tag_type(void *ent, int tag) {
    acl_ent_t *e = ent;
    if (!e || e->magic != ENT_MAGIC || tag < 0 || tag > 2) { cx_errno = CX_EINVAL; return -1; }
    e->tag = tag;
    return 0;
}
int acl_get_entry(void *p, int id, void **ent) {
    acl_t *a = p;
    if (ent) *ent = 0;
    if (!a || a->magic != ACL_MAGIC) { cx_errno = CX_EINVAL; return -1; }
    if (id == 0) a->cursor = 0;
    else if (id != -1) { cx_errno = CX_EINVAL; return -1; }
    if (a->cursor < 0 || a->cursor >= a->n) return -1;
    if (ent) *ent = a->e[a->cursor];
    a->cursor++;
    return 0;
}
int acl_get_tag_type(void *ent, int *tag) {
    acl_ent_t *e = ent;
    if (!e || e->magic != ENT_MAGIC) { cx_errno = CX_EINVAL; return -1; }
    if (tag) *tag = e->tag;
    return 0;
}
int acl_get_permset_mask_np(void *ent, u64 *m) {
    acl_ent_t *e = ent;
    if (!e || e->magic != ENT_MAGIC) { cx_errno = CX_EINVAL; return -1; }
    if (m) *m = e->perm;
    return 0;
}
static int acl_store(u64 ino, void *p) {
    acl_t *a = p;
    if (!a || a->magic != ACL_MAGIC) { cx_errno = CX_EINVAL; return -1; }
    acl_t *c = acl_clone(a);
    if (!c) { cx_errno = CX_ENOMEM; return -1; }
    acl_t **s = acl_slot(ino, 1);
    if (!s) { acl_release(c); cx_errno = CX_ENOMEM; return -1; }
    acl_release(*s);
    *s = c;
    return 0;
}
static void *acl_load(u64 ino) {
    acl_t **s = acl_slot(ino, 0);
    if (!s || !*s) { cx_errno = CX_ENOENT; return 0; }
    acl_t *c = acl_clone(*s);
    if (!c) { cx_errno = CX_ENOMEM; return 0; }
    return c;
}
void *acl_get_fd(int fd) {
    u64 ino;
    if (acl_ino(1, fd, 0, &ino) < 0) return 0;
    return acl_load(ino);
}
int acl_set_fd(int fd, void *a) {
    u64 ino;
    if (!a) { cx_errno = CX_EINVAL; return -1; }
    if (acl_ino(1, fd, 0, &ino) < 0) return -1;
    return acl_store(ino, a);
}
void *cx_acl_get_path(const char *path, int follow) {
    u64 ino;
    if (!path) { cx_errno = CX_EINVAL; return 0; }
    if (acl_ino(follow, -1, path, &ino) < 0) return 0;
    return acl_load(ino);
}
int cx_acl_set_path(const char *path, void *a, int follow) {
    u64 ino;
    if (!path || !a) { cx_errno = CX_EINVAL; return -1; }
    if (acl_ino(follow, -1, path, &ino) < 0) return -1;
    return acl_store(ino, a);
}

/* ================================================================ diversos */
void srandomdev(void) { srandom(arc4random()); }
/* identidade da máquina: estável por instalação (o mOS não tem número de série) */
int gethostuuid(u8 *uuid, const void *timeout) {
    (void)timeout;
    static const u8 id[16] = {0x6d, 0x4f, 0x53, 0x00, 0x4d, 0x4f, 0x40, 0x53, 0x8a, 0x21, 0x37, 0x10, 0x51, 0x0c, 0x2e, 0x01};
    memcpy(uuid, id, 16);
    return 0;
}
int getpeereid(int fd, unsigned *uid, unsigned *gid) { (void)fd; if (uid) *uid = 501; if (gid) *gid = 20; return 0; }
unsigned if_nametoindex(const char *name) {
    if (!name) return 0;
    if (!strcmp(name, "lo0")) return 1;
    if (!strcmp(name, "en0")) return 4;
    return 0;
}
/* gerência de energia: aceita (o mOS não dorme sozinho com apps abertos) */
int IOPMAssertionCreateWithName(void *type, u32 level, void *name, u32 *id) { (void)type; (void)level; (void)name; if (id) *id = 1; return 0; }
int IOPMAssertionRelease(u32 id) { (void)id; return 0; }

/* ================================================================ CoreFoundation, CFNetwork, SystemConfiguration */
typedef void *id_;
typedef struct cx_class cx_class_t;
cx_class_t *cx_class_named(const char *name);
id_ objc_msgSend(id_ self, void *op, ...);
void *sel_registerName(const char *name);
id_ objc_retain(id_ o);
#define MSG0(T, o, s) ((T (*)(id_, void *))objc_msgSend)((id_)(o), sel_registerName(s))
#define MSG1(T, o, s, a) ((T (*)(id_, void *, __typeof__(a)))objc_msgSend)((id_)(o), sel_registerName(s), (a))
#define MSG2(T, o, s, a, b) ((T (*)(id_, void *, __typeof__(a), __typeof__(b)))objc_msgSend)((id_)(o), sel_registerName(s), (a), (b))
#define CLS(n) ((id_)cx_class_named(n))
id_ CFRetain(id_ o);
id_ CFDictionaryCreateMutable(void *alloc, long cap, const void *kcb, const void *vcb);
void CFDictionarySetValue(id_ d, id_ key, id_ val);
id_ CFArrayCreateMutable(void *alloc, long cap, const void *cb);
void CFArrayAppendValue(id_ a, id_ v);
id_ CFArrayCreateCopy(void *alloc, id_ src);
static id_ str1(const char *s) { id_ r = MSG1(id_, CLS("NSString"), "stringWithUTF8String:", s); return r ? objc_retain(r) : 0; }
static const char *utf8(id_ s) { return s ? MSG0(const char *, s, "UTF8String") : 0; }

extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } cfconst_t;
#define K(var, text) \
    static cfconst_t var##_cs = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; \
    const void *var = &var##_cs;
K(kCFProxyTypeKey, "kCFProxyTypeKey") K(kCFProxyTypeNone, "kCFProxyTypeNone") K(kCFProxyTypeHTTP, "kCFProxyTypeHTTP")
K(kCFProxyTypeHTTPS, "kCFProxyTypeHTTPS") K(kCFProxyTypeSOCKS, "kCFProxyTypeSOCKS") K(kCFProxyHostNameKey, "kCFProxyHostNameKey")
K(kCFProxyPortNumberKey, "kCFProxyPortNumberKey") K(kCFProxyAutoConfigurationURLKey, "kCFProxyAutoConfigurationURLKey")
K(kCFProxyAutoConfigurationJavaScriptKey, "kCFProxyAutoConfigurationJavaScriptKey")
K(kSCPropNetProxiesHTTPEnable, "HTTPEnable") K(kSCPropNetProxiesHTTPProxy, "HTTPProxy") K(kSCPropNetProxiesHTTPPort, "HTTPPort")
K(kSCPropNetProxiesHTTPSEnable, "HTTPSEnable") K(kSCPropNetProxiesHTTPSProxy, "HTTPSProxy") K(kSCPropNetProxiesHTTPSPort, "HTTPSPort")
K(kCFNetworkProxiesSOCKSEnable, "SOCKSEnable") K(kCFNetworkProxiesProxyAutoDiscoveryEnable, "ProxyAutoDiscoveryEnable")
K(kSCDynamicStoreUseSessionKeys, "UseSessionKeys")
K(NSPasteboardTypeHTML, "public.html") K(NSPasteboardTypeTIFF, "public.tiff")
K(NSPasteboardURLReadingFileURLsOnlyKey, "NSPasteboardURLReadingFileURLsOnlyKey")

/* proxies: o mOS não tem configuração de proxy; a resposta é a lista com um "sem proxy", como num Mac sem proxy */
id_ CFNetworkCopyProxiesForURL(id_ url, id_ settings) {
    (void)url; (void)settings;
    id_ d = CFDictionaryCreateMutable(0, 0, 0, 0);
    CFDictionarySetValue(d, (id_)kCFProxyTypeKey, (id_)kCFProxyTypeNone);
    id_ a = CFArrayCreateMutable(0, 0, 0);
    CFArrayAppendValue(a, d);
    return a;
}
id_ CFNetworkExecuteProxyAutoConfigurationScript(id_ script, id_ url, void *cb, void *ctx) { (void)script; (void)url; (void)cb; (void)ctx; return 0; }
id_ CFNetworkExecuteProxyAutoConfigurationURL(id_ pac, id_ url, void *cb, void *ctx) { (void)pac; (void)url; (void)cb; (void)ctx; return 0; }

/* CFError: um NSError do mOS (CFGetTypeID = TID_ERROR); domínio e código numa tabela ao lado, porque o NSError do
 * mOS não guarda campos. A tabela é circular (256 erros vivos bastam: erros CF são curtos). */
static struct { id_ e; id_ domain; long code; } errs[256];
static int nerrs;
static volatile int errs_lock;
id_ CFErrorCreate(void *alloc, id_ domain, long code, id_ info) {
    (void)alloc; (void)info;
    id_ e = MSG0(id_, CLS("NSError"), "alloc");
    if (e) e = MSG0(id_, e, "init");
    if (!e) return 0;
    while (__sync_lock_test_and_set(&errs_lock, 1)) ;
    int i = nerrs++ & 255;
    errs[i].e = e; errs[i].domain = domain ? CFRetain(domain) : 0; errs[i].code = code;
    __sync_lock_release(&errs_lock);
    return e;
}
static int err_find(id_ e, id_ *domain, long *code) {
    int r = 0;
    while (__sync_lock_test_and_set(&errs_lock, 1)) ;
    for (int i = 0; i < 256; i++)
        if (errs[i].e == e && e) { if (domain) *domain = errs[i].domain; if (code) *code = errs[i].code; r = 1; break; }
    __sync_lock_release(&errs_lock);
    return r;
}
long CFErrorGetCode(id_ e) { long c = 0; err_find(e, 0, &c); return c; }
id_ CFErrorGetDomain(id_ e) { id_ d = 0; err_find(e, &d, 0); return d; }

/* CFURL: as que nsurl.c (sessão do VS Code) ainda não tem */
id_ CFURLCreateWithString(void *alloc, id_ s, id_ base) {
    (void)alloc; (void)base;
    if (!s) return 0;
    id_ u = MSG1(id_, CLS("NSURL"), "URLWithString:", s);
    return u ? objc_retain(u) : 0;
}
/* CFURLCreateWithBytes: os bytes na codificação dada viram a string da URL, relativa à base se houver (o codex da
 * OpenAI, binário Rust, monta as URLs assim) */
id_ CFStringCreateWithBytes(void *alloc, const u8 *b, long n, u32 enc, u8 ext);
void CFRelease(id_ o);
id_ CFURLCreateWithBytes(void *alloc, const u8 *bytes, long len, u32 enc, id_ base) {
    (void)alloc;
    if (!bytes || len < 0) return 0;
    id_ s = CFStringCreateWithBytes(0, bytes, len, enc, 0);
    if (!s) return 0;
    id_ u = base ? MSG2(id_, CLS("NSURL"), "URLWithString:relativeToURL:", s, base) : MSG1(id_, CLS("NSURL"), "URLWithString:", s);
    CFRelease(s);
    return u ? objc_retain(u) : 0;
}
id_ CFURLCopyAbsoluteURL(id_ u) { return u ? CFRetain(u) : 0; }
/* a mesma URL resolvida contra a base (absoluteURL); o modo de compatibilidade (RFC 1808) não muda nada aqui */
id_ CFURLCreateAbsoluteURLWithBytes(void *alloc, const u8 *bytes, long len, u32 enc, id_ base, u8 compat) {
    (void)compat;
    id_ u = CFURLCreateWithBytes(alloc, bytes, len, enc, base);
    if (!u) return 0;
    id_ a = MSG0(id_, u, "absoluteURL");
    if (a) a = objc_retain(a);
    CFRelease(u);
    return a;
}
id_ CFURLCreateFilePathURL(void *alloc, id_ u, id_ *err) { (void)alloc; if (err) *err = 0; return u ? CFRetain(u) : 0; }
id_ CFURLCreateFileReferenceURL(void *alloc, id_ u, id_ *err) { (void)alloc; if (err) *err = 0; return u ? CFRetain(u) : 0; }
id_ CFURLCreateCopyAppendingPathComponent(void *alloc, id_ u, id_ comp, unsigned char dir) {
    (void)alloc;
    if (!u || !comp) return 0;
    id_ r = MSG2(id_, u, "URLByAppendingPathComponent:isDirectory:", comp, (signed char)dir);
    return r ? objc_retain(r) : 0;
}
id_ CFURLCreateCopyDeletingLastPathComponent(void *alloc, id_ u) {
    (void)alloc;
    if (!u) return 0;
    id_ r = MSG0(id_, u, "URLByDeletingLastPathComponent");
    return r ? objc_retain(r) : 0;
}
unsigned char CFURLResourceIsReachable(id_ u, id_ *err) {
    if (err) *err = 0;
    const char *p = u ? utf8(MSG0(id_, u, "path")) : 0;
    return p && access(p, 0) == 0;
}

/* idiomas, fuso, preferências */
id_ CFLocaleCopyPreferredLanguages(void) {
    id_ a = MSG0(id_, CLS("NSLocale"), "preferredLanguages");
    return a ? CFArrayCreateCopy(0, a) : CFArrayCreateMutable(0, 0, 0);
}
/* CFTimeZone = o nome IANA (CFString); TZ do ambiente, senão o do Brasil (padrão do mOS) */
id_ CFTimeZoneCopySystem(void) {
    const char *tz = getenv("TZ");
    if (tz && *tz == ':') tz++;
    return str1(tz && *tz ? tz : "America/Sao_Paulo");
}
id_ CFTimeZoneGetName(id_ tz) { return tz; }
void CFTimeZoneResetSystem(void) {}
/* CFPreferencesCopyAppValue / CFPreferencesAppSynchronize: prefs.c */

/* FSEvents: userland/libsystem/fsevents.c (varredura stat/readdir; o kernel não tem fila de vnode). */
