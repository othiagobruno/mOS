/*
 * mOS libSystem — libproc completo (proc_pidinfo, proc_pidfdinfo, proc_pid_rusage, proc_listpids, proc_pidpath,
 * proc_name, proc_terminate, rastreio "dirty" e os controles de CPU/wakeup), com os dados reais do kernel
 * (SYS_DARWIN, kernel/proc/darwin.c). Layouts de bsd/sys/proc_info.h e bsd/sys/resource.h do XNU (x86_64 e arm64
 * iguais). docs/darwin-bsd-mach.md, fase F2.
 *
 * Tempos: o mach_absolute_time do mOS conta nanossegundos (timebase 1/1, libc.c), então os campos "abstime" e os de
 * tempo de CPU saem em ns. Sem SYS_DARWIN (harness tools/machotest.c) cai na lista curta do SYS_PROCLIST.
 */
#include "sys.h"

#define CX_ENOTSUP 45

size_t strlen(const char *s);
size_t strlcpy(char *d, const char *s, size_t n);
int strcmp(const char *a, const char *b);
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
int kill(int pid, int sig);
int getpid(void);
int stat(const char *p, void *st);
unsigned long long mach_absolute_time(void);

/* ------------------------------------------------------------------ acesso ao kernel */
static int dw_proc(int pid, mos_dw_proc_t *d) {
    long r = SYS3(SYS_DARWIN, MOS_DW_PROC, pid, d);
    if (r == 0) return 0;
    if (r != -MOS_ESRCH) {                      /* kernel sem SYS_DARWIN: lista curta */
        mos_procinfo_t pi;
        for (long i = 0; SYS2(SYS_PROCLIST, i, &pi) == 1; i++)
            if (pi.pid == pid) {
                memset(d, 0, sizeof *d);
                d->pid = pi.pid; d->ppid = pi.ppid; d->pgid = d->sid = pi.pid;
                d->uid = d->euid = d->suid = 501; d->gid = d->egid = d->sgid = 20;
                d->state = pi.state == 0 || pi.state == 1 ? MOS_DW_ST_RUN : MOS_DW_ST_SLEEP;
                d->nthreads = 1; d->cpu_ms = pi.cpu_ms; d->mem_bytes = pi.mem_kb * 1024;
                d->vsize = d->mem_bytes;
                memcpy(d->name, pi.name, sizeof pi.name < sizeof d->name ? sizeof pi.name : sizeof d->name);
                d->name[sizeof d->name - 1] = 0;
                return 0;
            }
    }
    cx_errno = CX_ESRCH;
    return -1;
}
static u64 boot_us(void) {
    mos_dw_sysinfo_t si;
    if (SYS2(SYS_DARWIN, MOS_DW_SYSINFO, &si) == 0) return si.boot_unix_us;
    return 0;
}
static long dw_pids(int *buf, int cap) {
    long n = SYS3(SYS_DARWIN, MOS_DW_PIDS, buf, cap);
    if (n >= 0) return n;
    mos_procinfo_t pi;
    n = 0;
    for (long i = 0; SYS2(SYS_PROCLIST, i, &pi) == 1; i++) { if (n < cap) buf[n] = pi.pid; n++; }
    return n;
}
static int pid_threads(int pid, mos_dw_thread_t *buf, int cap) {
    long n = SYS4(SYS_DARWIN, MOS_DW_THREADS, pid, buf, cap);
    return n < 0 ? 0 : (int)n;
}
static u64 footprint(int pid, u64 resident) {
    long r = SYS2(SYS_VMCTL, MOS_VM_FOOTPRINT, pid == getpid() ? 0 : pid);   /* docs/nucleo-plano.md */
    return r > 0 ? (u64)r : resident;           /* residente + comprimido; sem SYS_VMCTL, o residente */
}

/* ------------------------------------------------------------------ proc_bsdinfo, proc_taskinfo, ... */
#define PROC_FLAG_LP64    0x10
#define PROC_FLAG_SLEADER 0x20
#define PROC_FLAG_CTTY    0x40
#define PROC_FLAG_CONTROLT 0x80
static void put32(u8 *b, u32 o, u32 v) { memcpy(b + o, &v, 4); }
static void put64(u8 *b, u32 o, u64 v) { memcpy(b + o, &v, 8); }

static void fill_bsdinfo(u8 *b, const mos_dw_proc_t *d) {   /* struct proc_bsdinfo, 136 bytes */
    memset(b, 0, 136);
    u32 fl = PROC_FLAG_LP64 | (d->pid == d->sid ? PROC_FLAG_SLEADER : 0) | (d->flags & 2 ? PROC_FLAG_CTTY | PROC_FLAG_CONTROLT : 0);
    static const u32 st[] = {2, 2, 3, 4, 5};                   /* SRUN SRUN SSLEEP SSTOP SZOMB */
    put32(b, 0, fl);
    put32(b, 4, st[d->state >= 0 && d->state <= 4 ? d->state : 0]);
    put32(b, 12, (u32)d->pid); put32(b, 16, (u32)d->ppid);
    put32(b, 20, (u32)d->euid); put32(b, 24, (u32)d->egid);
    put32(b, 28, (u32)d->uid); put32(b, 32, (u32)d->gid);
    put32(b, 36, (u32)d->suid); put32(b, 40, (u32)d->sgid);
    { size_t n = strlen(d->name); memcpy(b + 48, d->name, n > 16 ? 16 : n); }   /* pbi_comm[MAXCOMLEN] */
    { size_t n = strlen(d->name); memcpy(b + 64, d->name, n > 31 ? 31 : n); }   /* pbi_name[2*MAXCOMLEN] */
    long nf = SYS4(SYS_DARWIN, MOS_DW_FDS, d->pid, 0, 0);
    put32(b, 96, nf > 0 ? (u32)nf : 0);                          /* pbi_nfiles */
    put32(b, 100, (u32)d->pgid);
    put32(b, 108, 0xffffffffu);                                  /* e_tdev = NODEV (o terminal está no kinfo_proc) */
    u64 st_us = d->start_unix_us;
    put64(b, 120, st_us / 1000000); put64(b, 128, st_us % 1000000);
}
static void fill_taskinfo(u8 *b, const mos_dw_proc_t *d) {  /* struct proc_taskinfo, 96 bytes */
    memset(b, 0, 96);
    mos_dw_thread_t th[64];
    int n = pid_threads(d->pid, th, 64), run = 0;
    for (int i = 0; i < n && i < 64; i++) if (th[i].state == MOS_DW_ST_RUN) run++;
    put64(b, 0, d->vsize);
    put64(b, 8, d->mem_bytes);
    put64(b, 16, d->cpu_ms * 1000000ull);                        /* pti_total_user (o kernel não separa sistema) */
    put64(b, 32, d->cpu_ms * 1000000ull);                        /* pti_threads_user: só das threads vivas */
    put32(b, 48, 1);                                             /* POLICY_TIMESHARE */
    put32(b, 84, (u32)(n ? n : d->nthreads));
    put32(b, 88, (u32)run);
    put32(b, 92, 31);                                            /* BASEPRI_DEFAULT */
}
static void fill_threadinfo(u8 *b, const mos_dw_thread_t *t) {   /* struct proc_threadinfo, 112 bytes */
    memset(b, 0, 112);
    put64(b, 0, t->cpu_ms * 1000000ull);
    put32(b, 20, 1);                                             /* POLICY_TIMESHARE */
    put32(b, 24, t->suspend ? 2 : t->state == MOS_DW_ST_RUN ? 1 : 3);   /* TH_STATE_STOPPED / RUNNING / WAITING */
    static const int qpri[] = {4, 20, 31, 37, 47};               /* BG, UT, DEF, UI/IN, USER_INTERACTIVE */
    int pr = qpri[t->qos < 5 ? t->qos : 2];
    put32(b, 36, (u32)pr); put32(b, 40, (u32)pr); put32(b, 44, 63);
    size_t n = strlen(t->name);
    memcpy(b + 48, t->name, n > 63 ? 63 : n);
}
static int find_thread(int pid, u64 id, mos_dw_thread_t *out) {
    mos_dw_thread_t th[256];
    int n = pid_threads(pid, th, 256);
    for (int i = 0; i < n && i < 256; i++) if (th[i].tid == id) { *out = th[i]; return 1; }
    return 0;
}

/* struct vinfo_stat (136) a partir do stat do macOS ($INODE64, 144) */
typedef struct {
    u32 st_dev; u16 st_mode, st_nlink; u64 st_ino; u32 st_uid, st_gid, st_rdev;
    i64 atime, atime_ns, mtime, mtime_ns, ctime, ctime_ns, btime, btime_ns;
    i64 st_size, st_blocks; i32 st_blksize; u32 st_flags, st_gen; i32 lspare; i64 qspare[2];
} pi_stat_t;
static void fill_vinfo(u8 *b, const char *path) {           /* struct vnode_info (152) */
    memset(b, 0, 152);
    pi_stat_t s;
    if (!path || !path[0] || stat(path, &s) != 0) return;
    put32(b, 0, s.st_dev);
    memcpy(b + 4, &s.st_mode, 2); memcpy(b + 6, &s.st_nlink, 2);
    put64(b, 8, s.st_ino); put32(b, 16, s.st_uid); put32(b, 20, s.st_gid);
    put64(b, 24, (u64)s.atime); put64(b, 32, (u64)s.atime_ns); put64(b, 40, (u64)s.mtime); put64(b, 48, (u64)s.mtime_ns);
    put64(b, 56, (u64)s.ctime); put64(b, 64, (u64)s.ctime_ns); put64(b, 72, (u64)s.btime); put64(b, 80, (u64)s.btime_ns);
    put64(b, 88, (u64)s.st_size); put64(b, 96, (u64)s.st_blocks); put32(b, 104, (u32)s.st_blksize);
    put32(b, 108, s.st_flags); put32(b, 112, s.st_gen); put32(b, 116, s.st_rdev);
    u32 m = s.st_mode & 0xf000;
    put32(b, 136, m == 0x4000 ? 2 : m == 0xa000 ? 5 : m == 0x2000 ? 4 : m == 0x1000 ? 7 : 1);   /* vi_type: VDIR/VLNK/VCHR/VFIFO/VREG */
    put32(b, 144, s.st_dev);                                     /* vi_fsid.val[0] */
}
static void fill_vpath(u8 *b, const char *path) {           /* struct vnode_info_path (1176) */
    fill_vinfo(b, path);
    memset(b + 152, 0, 1024);
    if (path) strlcpy((char *)b + 152, path, 1024);
}

/* ------------------------------------------------------------------ proc_pidinfo */
enum {
    PROC_PIDLISTFDS = 1, PROC_PIDTASKALLINFO, PROC_PIDTBSDINFO, PROC_PIDTASKINFO, PROC_PIDTHREADINFO, PROC_PIDLISTTHREADS,
    PROC_PIDREGIONINFO, PROC_PIDREGIONPATHINFO, PROC_PIDVNODEPATHINFO, PROC_PIDTHREADPATHINFO, PROC_PIDPATHINFO,
    PROC_PIDWORKQUEUEINFO, PROC_PIDT_SHORTBSDINFO, PROC_PIDLISTFILEPORTS, PROC_PIDTHREADID64INFO, PROC_PID_RUSAGE,
    PROC_PIDUNIQIDENTIFIERINFO, PROC_PIDT_BSDINFOWITHUNIQID, PROC_PIDARCHINFO = 19, PROC_PIDCOALITIONINFO = 20,
    PROC_PIDNOTEEXIT = 21, PROC_PIDREGIONPATHINFO2 = 22, PROC_PIDREGIONPATHINFO3 = 23, PROC_PIDEXITREASONINFO = 24,
    PROC_PIDEXITREASONBASICINFO = 25, PROC_PIDLISTUPTRS = 26, PROC_PIDLISTDYNKQUEUES = 27, PROC_PIDLISTTHREADIDS = 28,
    PROC_PIDVMRTFAULTINFO = 29, PROC_PIDPLATFORMINFO = 30,
};
#define PROX_FDTYPE_VNODE 1
#define PROX_FDTYPE_SOCKET 2
#define PROX_FDTYPE_PIPE 6

static void fill_uniq(u8 *b, const mos_dw_proc_t *d) {      /* struct proc_uniqidentifierinfo (56) */
    memset(b, 0, 56);
    /* p_uuid: o LC_UUID do executável não está no kernel; um identificador estável por pid+início no lugar */
    u64 a = (u64)d->pid * 0x9e3779b97f4a7c15ull ^ d->start_unix_us, c = d->start_unix_us * 0xc2b2ae3d27d4eb4full;
    memcpy(b, &a, 8); memcpy(b + 8, &c, 8);
    b[6] = (u8)((b[6] & 0x0f) | 0x40); b[8] = (u8)((b[8] & 0x3f) | 0x80);
    put64(b, 16, ((u64)d->pid << 32) | (u32)(d->start_unix_us / 1000));   /* p_uniqueid: único enquanto o boot durar */
    mos_dw_proc_t pp;
    u64 pu = 0;
    if (d->ppid > 0 && SYS3(SYS_DARWIN, MOS_DW_PROC, d->ppid, &pp) == 0) pu = ((u64)pp.pid << 32) | (u32)(pp.start_unix_us / 1000);
    put64(b, 24, pu);
}

int proc_pidinfo(int pid, int flavor, u64 arg, void *buf, int size) {
    static const int need_of[] = {0, 8, 232, 136, 96, 112, 8, 0, 0, 2352, 0, 1024, 16, 64, 8, 112, 0, 56, 192, 8};
    mos_dw_proc_t d;
    if (flavor <= 0 || flavor > PROC_PIDPLATFORMINFO) { cx_errno = CX_EINVAL; return 0; }
    if (dw_proc(pid, &d) < 0) return 0;
    u8 *b = buf;
    switch (flavor) {
    case PROC_PIDLISTFDS: {                     /* struct proc_fdinfo {int fd; u32 type} por descritor */
        mos_dw_fd_t fds[256];
        long n = SYS4(SYS_DARWIN, MOS_DW_FDS, pid, fds, 256);
        if (n < 0) { cx_errno = n == -MOS_EPERM ? CX_EPERM : CX_ESRCH; return 0; }
        if (!buf) return (int)(n + 20) * 8;     /* sem buffer: tamanho com folga, como o XNU */
        int k = 0;
        for (long i = 0; i < n && i < 256 && (k + 1) * 8 <= size; i++, k++) {
            u32 t = fds[i].kind == MOS_DW_FD_PIPE ? PROX_FDTYPE_PIPE : fds[i].kind == MOS_DW_FD_SOCK ? PROX_FDTYPE_SOCKET : PROX_FDTYPE_VNODE;
            put32(b, (u32)k * 8, (u32)fds[i].fd); put32(b, (u32)k * 8 + 4, t);
        }
        return k * 8;
    }
    case PROC_PIDLISTTHREADS: case PROC_PIDLISTTHREADIDS: {   /* u64 por thread (o handle é o tid) */
        mos_dw_thread_t th[256];
        int n = pid_threads(pid, th, 256);
        if (!buf) return (n + 4) * 8;
        int k = 0;
        for (int i = 0; i < n && i < 256 && (k + 1) * 8 <= size; i++, k++) put64(b, (u32)k * 8, th[i].tid);
        return k * 8;
    }
    case PROC_PIDPATHINFO: {
        if (!buf || size < 1024) { cx_errno = CX_ENOMEM; return 0; }
        memset(b, 0, 1024);
        if (pid == getpid()) {
            extern int _NSGetExecutablePath(char *buf, u32 *size);
            u32 n = 1024;
            if (_NSGetExecutablePath((char *)b, &n) == 0) return (int)strlen((char *)b);
        }
        strlcpy((char *)b, d.path[0] ? d.path : d.name, 1024);
        return (int)strlen((char *)b);
    }
    case PROC_PIDREGIONINFO: case PROC_PIDREGIONPATHINFO: case PROC_PIDREGIONPATHINFO2: case PROC_PIDREGIONPATHINFO3:
    case PROC_PIDTHREADPATHINFO: case PROC_PIDLISTFILEPORTS: case PROC_PIDCOALITIONINFO: case PROC_PIDNOTEEXIT:
    case PROC_PIDEXITREASONINFO: case PROC_PIDEXITREASONBASICINFO: case PROC_PIDLISTUPTRS: case PROC_PIDLISTDYNKQUEUES:
    case PROC_PIDVMRTFAULTINFO: case PROC_PIDPLATFORMINFO:
        /* regiões de memória vêm do mach_vm_region (frente do Henrique, docs/darwin-bsd-mach.md); os outros não têm
         * objeto no kernel do mOS (sem coalizões, sem razão de saída guardada, sem kqueue dinâmico) */
        cx_errno = CX_ENOTSUP;
        return 0;
    }
    int need = flavor < (int)(sizeof need_of / sizeof need_of[0]) ? need_of[flavor] : 0;
    if (!buf || size < need) { cx_errno = CX_ENOMEM; return 0; }
    switch (flavor) {
    case PROC_PIDTBSDINFO: fill_bsdinfo(b, &d); break;
    case PROC_PIDTASKINFO: fill_taskinfo(b, &d); break;
    case PROC_PIDTASKALLINFO: fill_bsdinfo(b, &d); fill_taskinfo(b + 136, &d); break;
    case PROC_PIDTHREADINFO: case PROC_PIDTHREADID64INFO: {
        mos_dw_thread_t t;
        if (!find_thread(pid, arg, &t)) { cx_errno = CX_ESRCH; return 0; }
        fill_threadinfo(b, &t);
        break;
    }
    case PROC_PIDVNODEPATHINFO: {               /* pasta atual e raiz */
        char cwd[1024];
        long n = SYS4(SYS_DARWIN, MOS_DW_CWD, pid, cwd, sizeof cwd);
        if (n < 0) { cx_errno = n == -MOS_EPERM ? CX_EPERM : CX_ESRCH; return 0; }
        fill_vpath(b, cwd);
        fill_vpath(b + 1176, "/");
        break;
    }
    case PROC_PIDWORKQUEUEINFO: {               /* struct proc_workqueueinfo: as threads do processo fazem o papel */
        mos_dw_thread_t th[256];
        int n = pid_threads(pid, th, 256), run = 0, blk = 0;
        for (int i = 0; i < n && i < 256; i++) { if (th[i].state == MOS_DW_ST_RUN) run++; else blk++; }
        memset(b, 0, 16);
        put32(b, 0, (u32)n); put32(b, 4, (u32)run); put32(b, 8, (u32)blk);
        break;
    }
    case PROC_PIDT_SHORTBSDINFO: {              /* struct proc_bsdshortinfo, 64 bytes */
        u8 t[136];
        fill_bsdinfo(t, &d);
        memset(b, 0, 64);
        memcpy(b + 0, t + 12, 8);               /* pid, ppid */
        memcpy(b + 8, t + 100, 4);              /* pgid */
        memcpy(b + 12, t + 4, 4);               /* status */
        memcpy(b + 16, t + 48, 16);             /* comm */
        memcpy(b + 32, t + 0, 4);               /* flags */
        memcpy(b + 36, t + 20, 24);             /* uid, gid, ruid, rgid, svuid, svgid */
        break;
    }
    case PROC_PIDUNIQIDENTIFIERINFO: fill_uniq(b, &d); break;
    case PROC_PIDT_BSDINFOWITHUNIQID: fill_bsdinfo(b, &d); fill_uniq(b + 136, &d); break;
    case PROC_PIDARCHINFO:                      /* struct proc_archinfo {cpu_type, cpu_subtype} */
#if defined(__aarch64__)
        put32(b, 0, 0x0100000c); put32(b, 4, 0);
#else
        put32(b, 0, 0x01000007); put32(b, 4, 3);
#endif
        break;
    case PROC_PID_RUSAGE: cx_errno = CX_EINVAL; return 0;   /* o XNU manda usar proc_pid_rusage */
    default: cx_errno = CX_EINVAL; return 0;
    }
    return need;
}
int proc_pidinfo_with_audittoken(int pid, int flavor, u64 arg, void *buf, int size, const u32 *tok) {
    (void)tok;
    return proc_pidinfo(pid, flavor, arg, buf, size);
}

/* ------------------------------------------------------------------ proc_pidfdinfo */
enum { PROC_PIDFDVNODEINFO = 1, PROC_PIDFDVNODEPATHINFO = 2, PROC_PIDFDSOCKETINFO = 3, PROC_PIDFDPSEMINFO = 4,
       PROC_PIDFDPSHMINFO = 5, PROC_PIDFDPIPEINFO = 6, PROC_PIDFDKQUEUEINFO = 7, PROC_PIDFDATALKINFO = 8,
       PROC_PIDFDCHANNELINFO = 10 };
static void fill_fileinfo(u8 *b, const mos_dw_fd_t *f, u32 type) {   /* struct proc_fileinfo (24) */
    memset(b, 0, 24);
    u32 of = f->flags & 3;                      /* O_RDONLY/WRONLY/RDWR: iguais no Linux e no BSD */
    if (f->flags & 0x400) of |= 0x8;           /* O_APPEND */
    if (f->flags & 0x800) of |= 0x4;           /* O_NONBLOCK */
    put32(b, 0, of);
    put32(b, 4, f->cloexec ? 1 : 0);           /* fi_status: PROC_FP_CLEXEC */
    put64(b, 8, f->off);
    put32(b, 16, type);
}
int proc_pidfdinfo(int pid, int fd, int flavor, void *buf, int size) {
    mos_dw_fd_t fds[256];
    long n = SYS4(SYS_DARWIN, MOS_DW_FDS, pid, fds, 256);
    if (n < 0) { cx_errno = n == -MOS_EPERM ? CX_EPERM : CX_ESRCH; return 0; }
    const mos_dw_fd_t *f = 0;
    for (long i = 0; i < n && i < 256; i++) if (fds[i].fd == fd) f = &fds[i];
    if (!f) { cx_errno = CX_EBADF; return 0; }
    u8 *b = buf;
    switch (flavor) {
    case PROC_PIDFDVNODEINFO:                   /* struct vnode_fdinfo: proc_fileinfo + vnode_info = 176 */
        if (f->kind != MOS_DW_FD_FILE && f->kind != MOS_DW_FD_PTY) { cx_errno = CX_EBADF; return 0; }
        if (!buf || size < 176) { cx_errno = CX_ENOMEM; return 0; }
        fill_fileinfo(b, f, PROX_FDTYPE_VNODE);
        fill_vinfo(b + 24, f->path);
        return 176;
    case PROC_PIDFDVNODEPATHINFO:               /* struct vnode_fdinfowithpath: 24 + 1176 = 1200 */
        if (f->kind != MOS_DW_FD_FILE && f->kind != MOS_DW_FD_PTY) { cx_errno = CX_EBADF; return 0; }
        if (!buf || size < 1200) { cx_errno = CX_ENOMEM; return 0; }
        fill_fileinfo(b, f, PROX_FDTYPE_VNODE);
        fill_vpath(b + 24, f->path);
        return 1200;
    case PROC_PIDFDPIPEINFO:                    /* struct pipe_fdinfo: 24 + pipe_info (vinfo_stat 136 + handle, peer, status, rfu = 160) */
        if (f->kind != MOS_DW_FD_PIPE) { cx_errno = CX_EBADF; return 0; }
        if (!buf || size < 184) { cx_errno = CX_ENOMEM; return 0; }
        memset(b, 0, 184);
        fill_fileinfo(b, f, PROX_FDTYPE_PIPE);
        put32(b, 24 + 4, 0x1000 | 0600);        /* vst_mode: S_IFIFO */
        put64(b, 24 + 88, 0);
        put64(b, 24 + 136, ((u64)pid << 16) | (u32)fd);   /* pipe_handle */
        return 184;
    case PROC_PIDFDSOCKETINFO: case PROC_PIDFDPSEMINFO: case PROC_PIDFDPSHMINFO: case PROC_PIDFDKQUEUEINFO:
    case PROC_PIDFDATALKINFO: case PROC_PIDFDCHANNELINFO:
        /* sockets TCP/UDP do mOS vivem no libSystem (sockets.c), não na tabela de fds do kernel; kqueue idem
         * (kqueue.c). O XNU devolve EBADF quando o tipo do fd não bate com o sabor. */
        cx_errno = CX_EBADF;
        return 0;
    }
    cx_errno = CX_EINVAL;
    return 0;
}
int proc_pidfileportinfo(int pid, u32 port, int flavor, void *buf, int size) {
    (void)pid; (void)port; (void)flavor; (void)buf; (void)size;
    cx_errno = CX_ENOTSUP;                      /* fileports (fileport.c) não guardam o dono depois de criados */
    return 0;
}

/* ------------------------------------------------------------------ proc_pid_rusage */
static const int rusage_sz[] = {96, 144, 160, 232, 296, 304, 400};
int proc_pid_rusage(int pid, int flavor, void *buf) {
    mos_dw_proc_t d;
    if (!buf) { cx_errno = CX_EFAULT; return -1; }
    if (flavor < 0 || flavor > 6) { cx_errno = CX_EINVAL; return -1; }
    if (dw_proc(pid, &d) < 0) return -1;
    u8 *b = buf;
    memset(b, 0, (size_t)rusage_sz[flavor]);
    mos_dw_proc_t tmp = d;
    u8 uq[56];
    fill_uniq(uq, &tmp);
    memcpy(b, uq, 16);                          /* ri_uuid */
    u64 fp = footprint(pid, d.mem_bytes);
    u64 boot = boot_us();
    put64(b, 16, d.cpu_ms * 1000000ull);        /* ri_user_time */
    put64(b, 64, d.mem_bytes);                  /* ri_resident_size */
    put64(b, 72, fp);                           /* ri_phys_footprint */
    put64(b, 80, boot && d.start_unix_us > boot ? (d.start_unix_us - boot) * 1000ull : 0);   /* ri_proc_start_abstime */
    if (flavor >= 3) {                          /* v3: ri_cpu_time_qos_default (o kernel não separa por QoS) */
        put64(b, 160, d.cpu_ms * 1000000ull);
    }
    if (flavor >= 4) put64(b, 248, fp);         /* v4: ri_lifetime_max_phys_footprint (o pico não é guardado: o atual) */
    return 0;
}

/* ------------------------------------------------------------------ listas */
#define PROC_ALL_PIDS 1
#define PROC_PGRP_ONLY 2
#define PROC_TTY_ONLY 3
#define PROC_UID_ONLY 4
#define PROC_RUID_ONLY 5
#define PROC_PPID_ONLY 6
#define PROC_KDBG_ONLY 7
int proc_listpids(u32 type, u32 info, void *buf, int bytes) {
    static int all[2048];
    long np = dw_pids(all, 2048);
    if (np > 2048) np = 2048;
    int n = 0, cap = buf ? bytes / (int)sizeof(int) : 0, *out = buf;
    for (long i = 0; i < np; i++) {
        int ok = type == PROC_ALL_PIDS;
        if (!ok && type != PROC_KDBG_ONLY) {
            mos_dw_proc_t d;
            if (dw_proc(all[i], &d) < 0) continue;
            ok = (type == PROC_PGRP_ONLY && (u32)d.pgid == info) || (type == PROC_UID_ONLY && (u32)d.euid == info) ||
                 (type == PROC_RUID_ONLY && (u32)d.uid == info) || (type == PROC_PPID_ONLY && (u32)d.ppid == info);
            if (type == PROC_TTY_ONLY) {        /* info = dev_t do terminal: o kinfo_proc tem o e_tdev */
                int mib[4] = {1, 14, 1, all[i]};
                u8 k[648];
                size_t l = sizeof k;
                extern int sysctl(int *, unsigned, void *, size_t *, void *, size_t);
                i32 dev = -1;
                if (sysctl(mib, 4, k, &l, 0, 0) == 0 && l == sizeof k) memcpy(&dev, k + 572, 4);
                ok = (u32)dev == info;
            }
        }
        if (!ok) continue;
        if (n < cap) out[n] = all[i];
        n++;
    }
    if (!buf) return (n + 16) * (int)sizeof(int);
    return (n < cap ? n : cap) * (int)sizeof(int);
}
int proc_listallpids(void *buf, int bytes) { return proc_listpids(PROC_ALL_PIDS, 0, buf, bytes) / (int)sizeof(int); }
int proc_listpgrppids(int pgrp, void *buf, int bytes) { return proc_listpids(PROC_PGRP_ONLY, (u32)pgrp, buf, bytes) / (int)sizeof(int); }
int proc_listchildpids(int ppid, void *buf, int bytes) {
    int r = proc_listpids(PROC_PPID_ONLY, (u32)ppid, buf, bytes);
    return buf ? r : r / (int)sizeof(int) - 16;   /* com buf: bytes preenchidos (como o libproc); sem buf: quantos */
}

int proc_pidpath(int pid, void *buf, u32 size) {
    if (!buf || !size) { cx_errno = CX_EINVAL; return 0; }
    char tmp[1024];
    int n = proc_pidinfo(pid, PROC_PIDPATHINFO, 0, tmp, sizeof tmp);
    if (n <= 0) return 0;
    if ((u32)n + 1 > size) { cx_errno = CX_ENOMEM; return 0; }
    memcpy(buf, tmp, (size_t)n + 1);
    return n;
}
int proc_pidpath_audittoken(const u32 *tok, void *buf, u32 size) {
    if (!tok) { cx_errno = CX_EINVAL; return 0; }
    return proc_pidpath((int)tok[5], buf, size);
}
/* proc_name: o pbi_name (até 2*MAXCOMLEN); com nome vazio, o último componente do caminho */
int proc_name(int pid, void *buf, u32 size) {
    mos_dw_proc_t d;
    if (!buf || !size) { cx_errno = CX_EINVAL; return 0; }
    if (dw_proc(pid, &d) < 0) return 0;
    const char *nm = d.name;
    char path[1024];
    if (!nm[0] && proc_pidpath(pid, path, sizeof path) > 0) {
        nm = path;
        for (const char *p = path; *p; p++) if (*p == '/') nm = p + 1;
    }
    strlcpy(buf, nm, size);
    return (int)strlen(buf);
}
int proc_regionfilename(int pid, u64 addr, void *buf, u32 size);   /* posixmore.c */
int proc_libversion(int *major, int *minor) { if (major) *major = 1; if (minor) *minor = 1; return 0; }
int proc_kmsgbuf(void *buf, u32 size) { (void)buf; (void)size; cx_errno = CX_EPERM; return 0; }   /* só root no XNU; o mOS não tem msgbuf */
int proc_listcoalitions(int type, int typeorder, void *buf, int size) { (void)type; (void)typeorder; (void)buf; (void)size; return 0; }

/* ------------------------------------------------------------------ sinais e término */
int proc_signal_with_audittoken(const u32 *tok, int sig) {
    if (!tok) { cx_errno = CX_EINVAL; return -1; }
    return kill((int)tok[5], sig);
}
int proc_terminate_with_audittoken(const u32 *tok, int *sig) {
    if (!tok) { cx_errno = CX_EINVAL; return -1; }
    extern int proc_terminate(int pid, int *sig);
    return proc_terminate((int)tok[5], sig);
}

/* ---- rastreio "dirty" (proc_track_dirty/set/get/clear). O estado é deste processo; o launchd do mOS lê pelo
 * proc_get_dirty do próprio processo. Outro pid: EPERM, como no XNU para quem não é o launchd. */
#define PROC_DIRTY_TRACK 0x1
#define PROC_DIRTY_ALLOW_IDLE_EXIT 0x2
#define PROC_DIRTY_DEFER 0x4
#define PROC_DIRTY_TRACKED 0x1
#define PROC_DIRTY_ALLOWS_IDLE_EXIT 0x2
#define PROC_DIRTY_IS_DIRTY 0x4
static u32 dirty_track, dirty_flags;
static int self_or_eperm(int pid) {
    if (pid == getpid() || pid == 0) return 0;
    mos_dw_proc_t d;
    if (dw_proc(pid, &d) < 0) return -1;
    cx_errno = CX_EPERM;
    return -1;
}
int proc_track_dirty(int pid, u32 flags) {
    if (self_or_eperm(pid) < 0) return cx_errno;
    if (flags & ~(PROC_DIRTY_TRACK | PROC_DIRTY_ALLOW_IDLE_EXIT | PROC_DIRTY_DEFER | 0x8u)) return CX_EINVAL;
    dirty_track = flags;
    return 0;
}
int proc_set_dirty(int pid, int dirty) {
    if (self_or_eperm(pid) < 0) return cx_errno;
    if (!(dirty_track & PROC_DIRTY_TRACK)) return CX_EINVAL;
    if (dirty) dirty_flags |= PROC_DIRTY_IS_DIRTY; else dirty_flags &= ~(u32)PROC_DIRTY_IS_DIRTY;
    return 0;
}
int proc_get_dirty(int pid, u32 *flags) {
    if (!flags) return CX_EINVAL;
    if (self_or_eperm(pid) < 0) return cx_errno;
    *flags = (dirty_track & PROC_DIRTY_TRACK ? PROC_DIRTY_TRACKED : 0) |
             (dirty_track & PROC_DIRTY_ALLOW_IDLE_EXIT ? PROC_DIRTY_ALLOWS_IDLE_EXIT : 0) | (dirty_flags & PROC_DIRTY_IS_DIRTY);
    return 0;
}
int proc_clear_dirty(int pid, u32 flags) {
    if (self_or_eperm(pid) < 0) return cx_errno;
    dirty_flags &= ~flags;
    return 0;
}
int proc_set_dirty_tracked(int pid, int tracked) { return proc_track_dirty(pid, tracked ? PROC_DIRTY_TRACK : 0); }

/* proc_terminate: SIGKILL se o alvo é rastreado e está limpo com saída ociosa permitida (só sabemos isso de nós
 * mesmos), senão SIGTERM; devolve o sinal usado em *sig */
int proc_terminate(int pid, int *sig) {
    if (pid <= 0 || !sig) { cx_errno = CX_EINVAL; return -1; }
    mos_dw_proc_t d;
    if (dw_proc(pid, &d) < 0) return -1;
    int me = getpid();
    if (d.uid != (int)SYS0(SYS_GETEUID) && SYS0(SYS_GETEUID) != 0) { cx_errno = CX_EPERM; return -1; }
    int s = 15;
    if (pid == me && (dirty_track & PROC_DIRTY_TRACK) && (dirty_track & PROC_DIRTY_ALLOW_IDLE_EXIT) && !(dirty_flags & PROC_DIRTY_IS_DIRTY)) s = 9;
    if (kill(pid, s) != 0) return -1;
    *sig = s;
    return 0;
}

/* ---- monitores de CPU e de wakeups (o mOS não pune processos por consumo): os parâmetros ficam guardados e o
 * get devolve o que foi pedido, como o XNU quando o monitor está ligado e nada disparou. */
static int cpumon_pct = -1, cpumon_int = 0, wake_rate = -1, wake_flags = 0, cpu_limit = 0;
int proc_set_cpumon_params(int pid, int pct, int interval) {
    if (self_or_eperm(pid) < 0) return -1;
    if (pct < 0 || pct > 100 || interval <= 0) { cx_errno = CX_EINVAL; return -1; }
    cpumon_pct = pct; cpumon_int = interval;
    return 0;
}
int proc_get_cpumon_params(int pid, int *pct, int *interval) {
    if (self_or_eperm(pid) < 0) return -1;
    if (pct) *pct = cpumon_pct < 0 ? 0 : cpumon_pct;
    if (interval) *interval = cpumon_int;
    return 0;
}
int proc_set_cpumon_defaults(int pid) { if (self_or_eperm(pid) < 0) return -1; cpumon_pct = 50; cpumon_int = 180; return 0; }
int proc_disable_cpumon(int pid) { if (self_or_eperm(pid) < 0) return -1; cpumon_pct = -1; cpumon_int = 0; return 0; }
int proc_set_wakemon_params(int pid, int rate, int flags) {
    if (self_or_eperm(pid) < 0) return -1;
    wake_rate = rate; wake_flags = flags;
    return 0;
}
int proc_get_wakemon_params(int pid, int *rate, int *flags) {
    if (self_or_eperm(pid) < 0) return -1;
    if (wake_rate < 0) { cx_errno = CX_ENOTSUP; return -1; }   /* nunca ligado: como antes */
    if (rate) *rate = wake_rate;
    if (flags) *flags = wake_flags;
    return 0;
}
int proc_disable_wakemon(int pid) { if (self_or_eperm(pid) < 0) return -1; wake_rate = -1; wake_flags = 0; return 0; }
int proc_setcpu_percentage(int pid, int action, int pct) {
    (void)action;
    if (self_or_eperm(pid) < 0) return -1;
    if (pct < 0 || pct > 100) { cx_errno = CX_EINVAL; return -1; }
    cpu_limit = pct;
    return 0;
}
int proc_clearcpulimits(int pid) { if (self_or_eperm(pid) < 0) return -1; cpu_limit = 0; return 0; }
int proc_setthread_cpupercent(u8 pct, u32 ms) { (void)ms; if (pct > 100) { cx_errno = CX_EINVAL; return -1; } return 0; }
int proc_set_no_smt(void) { return 0; }
int proc_setthread_no_smt(void) { return 0; }
int proc_set_csm(u32 flags) { (void)flags; return 0; }
int proc_setthread_csm(u32 flags) { (void)flags; return 0; }
int proc_get_csm(int pid, u32 *flags) { if (flags) *flags = 0; return self_or_eperm(pid) < 0 ? -1 : 0; }
int proc_suppress(int pid, u64 *gen) { if (gen) *gen = 0; return self_or_eperm(pid) < 0 ? -1 : 0; }
int proc_setpcontrol(int ctl) { return ctl < 0 || ctl > 3 ? (cx_errno = CX_EINVAL, -1) : 0; }
int proc_reset_footprint_interval(int pid) { return self_or_eperm(pid) < 0 ? -1 : 0; }
int proc_uuid_policy(u32 op, const void *uuid, size_t len, u32 flags) {
    (void)op; (void)uuid; (void)flags;
    if (len != 16) { cx_errno = CX_EINVAL; return -1; }
    cx_errno = CX_EPERM;                        /* só o kernel/launchd mexem na política de UUID */
    return -1;
}
int proc_rlimit_control(int pid, int flavor, void *arg) {
    (void)arg;
    if (self_or_eperm(pid) < 0) return -1;
    if (flavor < 0 || flavor > 6) { cx_errno = CX_EINVAL; return -1; }
    return 0;                                   /* RLIMIT_WAKEUPS_MONITOR, CPU_USAGE_MONITOR...: sem punição no mOS */
}
int proc_current_thread_schedinfo(void *info, size_t size) {   /* struct proc_threadschedinfo {u64 int_time_ns} */
    if (!info || size < 8) { cx_errno = CX_EINVAL; return -1; }
    memset(info, 0, size);
    return 0;
}
int proc_donate_importance_boost(void) { return 0; }
int proc_denap_assertion_begin_with_msg(void *msg, u64 *token) { (void)msg; if (token) *token = 1; return 0; }
int proc_denap_assertion_complete(u64 token) { (void)token; return 0; }
int proc_appstate(int pid, int *state) { if (state) *state = 0; return self_or_eperm(pid) < 0 ? -1 : 0; }
int proc_udata_info(int pid, int flavor, void *buf, int size) { (void)pid; (void)flavor; (void)buf; (void)size; cx_errno = CX_ENOTSUP; return -1; }
int proc_list_uptrs(int pid, u64 *buf, u32 size) { (void)buf; (void)size; return self_or_eperm(pid) < 0 ? -1 : 0; }
int proc_list_dynkqueueids(int pid, u64 *buf, u32 size) { (void)buf; (void)size; return self_or_eperm(pid) < 0 ? -1 : 0; }
int proc_piddynkqueueinfo(int pid, int flavor, u64 id, void *buf, int size) { (void)pid; (void)flavor; (void)id; (void)buf; (void)size; cx_errno = CX_ESRCH; return 0; }
int proc_pidoriginatorinfo(int flavor, void *buf, int size) {   /* sem voucher de origem: o próprio processo */
    if (!buf) { cx_errno = CX_EINVAL; return 0; }
    if (flavor == 1 && size >= 16) { u8 uq[56]; mos_dw_proc_t d; if (dw_proc(getpid(), &d) < 0) return 0; fill_uniq(uq, &d); memcpy(buf, uq, 16); return 16; }   /* PROC_PIDORIGINATOR_UUID */
    if (flavor == 3 && size >= 4) { int p = getpid(); memcpy(buf, &p, 4); return 4; }   /* PROC_PIDORIGINATOR_PID_UUID: pid */
    cx_errno = CX_EINVAL;
    return 0;
}

/* os_proc_available_memory: RAM livre do sistema (sem limite por processo), em bytes */
size_t os_proc_available_memory(void) {
    mos_dw_sysinfo_t si;
    if (SYS2(SYS_DARWIN, MOS_DW_SYSINFO, &si) == 0) return (size_t)si.mem_free;
    return (size_t)1 << 30;
}

/* pid_suspend / pid_resume (privadas do launchd e do Activity Monitor): parada do processo inteiro, que o kernel
 * do mOS já tem (SIGSTOP/SIGCONT, proc_hold). Mesmo dono ou root, como o task_for_pid exige no XNU. */
int pid_suspend(int pid) {
    mos_dw_proc_t d;
    if (dw_proc(pid, &d) < 0) return -1;
    long eu = SYS0(SYS_GETEUID);
    if (eu != 0 && eu != d.uid) { cx_errno = CX_EPERM; return -1; }
    return kill(pid, 17);                       /* SIGSTOP */
}
int pid_resume(int pid) {
    mos_dw_proc_t d;
    if (dw_proc(pid, &d) < 0) return -1;
    long eu = SYS0(SYS_GETEUID);
    if (eu != 0 && eu != d.uid) { cx_errno = CX_EPERM; return -1; }
    return kill(pid, 19);                       /* SIGCONT */
}
