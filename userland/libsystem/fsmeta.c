/*
 * mOS libSystem — metadados de arquivo sobre o SYS_FSMETA do kernel (kernel/lib/vfsmeta.c):
 * getxattr/setxattr/listxattr/removexattr (e as variantes f*), chown/lchown/fchown/fchownat,
 * chflags/lchflags/fchflags/chflagsat, utimes/lutimes/futimes/utime/utimensat/futimens, e cx_statx para o stat.
 *
 * Links simbólicos do mOS são arquivos "#mOS-link:" e o kernel não segue o último componente: quem segue
 * (getxattr sem XATTR_NOFOLLOW, chown, utimes...) resolve antes com cx_follow_path (libc.c); as variantes l* e
 * XATTR_NOFOLLOW agem no próprio link. Erros com os valores do macOS (ENOATTR 93, ERANGE 34, EPERM 1...).
 */
#include "sys.h"
#include "libc.h"

#define SET(e) (cx_errno = (e))
#define XATTR_NOFOLLOW 0x0001
#define XATTR_SHOWCOMPRESSION 0x0020
#define AT_FDCWD_ -2
#define AT_SYMLINK_NOFOLLOW_ 0x0020
#define UTIME_NOW_  -1l
#define UTIME_OMIT_ -2l


int  cx_follow_path(const char *in, char *out, size_t cap);    /* libc.c: absoluto, seguindo links no fim */
const char *cx_at_path(int dirfd, const char *p, char *buf, size_t cap);   /* posixmore.c */

static long fsm(int op, mos_fsmeta_t *rq) {
    long r = SYS2(SYS_FSMETA, op, rq);
    if (r < 0) { SET((int)-r); return -1; }
    return r;
}

/* alvo: caminho (seguindo ou não o último link) */
static int target(mos_fsmeta_t *rq, const char *p, int follow, char *buf, size_t cap) {
    if (!p) { SET(14); return -1; }
    if (follow) { if (cx_follow_path(p, buf, cap) < 0) return -1; }
    else cx_path(p, buf, cap);
    rq->fd = -1;
    rq->path = (unsigned long long)buf;
    return 0;
}

int cx_statx(const char *abs, int fd, mos_statx_t *st) {
    mos_fsmeta_t rq = {0};
    rq.fd = abs ? -1 : fd;
    rq.path = (unsigned long long)abs;
    rq.buf = (unsigned long long)st;
    rq.size = sizeof *st;
    long r = SYS2(SYS_FSMETA, MOS_FSM_STATX, &rq);
    if (r < 0) { SET((int)-r); return -1; }              /* ENOENT etc.: quem chama devolve -1 com o errno certo */
    return 0;
}

/* ======================================================================== xattr */
static long xget(mos_fsmeta_t *rq, const char *n, void *v, size_t s, u32 pos) {
    rq->name = (unsigned long long)n;
    rq->buf = (unsigned long long)v;
    rq->size = v ? s : 0;
    rq->pos = pos;
    return fsm(MOS_FSM_GETXATTR, rq);
}
long getxattr(const char *p, const char *n, void *v, size_t s, u32 pos, int o) {
    char b[1024];
    mos_fsmeta_t rq = {0};
    if (target(&rq, p, !(o & XATTR_NOFOLLOW), b, sizeof b) < 0) return -1;
    return xget(&rq, n, v, s, pos);
}
long fgetxattr(int fd, const char *n, void *v, size_t s, u32 pos, int o) {
    (void)o;
    mos_fsmeta_t rq = {0};
    rq.fd = fd;
    return xget(&rq, n, v, s, pos);
}

static int xset(mos_fsmeta_t *rq, const char *n, const void *v, size_t s, u32 pos, int o) {
    rq->name = (unsigned long long)n;
    rq->buf = (unsigned long long)v;
    rq->size = s;
    rq->pos = pos;
    rq->opts = (unsigned)o & 0x6;   /* XATTR_CREATE | XATTR_REPLACE */
    return fsm(MOS_FSM_SETXATTR, rq) < 0 ? -1 : 0;
}
int setxattr(const char *p, const char *n, const void *v, size_t s, u32 pos, int o) {
    char b[1024];
    mos_fsmeta_t rq = {0};
    if (target(&rq, p, !(o & XATTR_NOFOLLOW), b, sizeof b) < 0) return -1;
    return xset(&rq, n, v, s, pos, o);
}
int fsetxattr(int fd, const char *n, const void *v, size_t s, u32 pos, int o) {
    mos_fsmeta_t rq = {0};
    rq.fd = fd;
    return xset(&rq, n, v, s, pos, o);
}

int removexattr(const char *p, const char *n, int o) {
    char b[1024];
    mos_fsmeta_t rq = {0};
    if (target(&rq, p, !(o & XATTR_NOFOLLOW), b, sizeof b) < 0) return -1;
    rq.name = (unsigned long long)n;
    return fsm(MOS_FSM_REMOVEXATTR, &rq) < 0 ? -1 : 0;
}
int fremovexattr(int fd, const char *n, int o) {
    (void)o;
    mos_fsmeta_t rq = {0};
    rq.fd = fd;
    rq.name = (unsigned long long)n;
    return fsm(MOS_FSM_REMOVEXATTR, &rq) < 0 ? -1 : 0;
}

long listxattr(const char *p, char *buf, size_t s, int o) {
    char b[1024];
    mos_fsmeta_t rq = {0};
    if (target(&rq, p, !(o & XATTR_NOFOLLOW), b, sizeof b) < 0) return -1;
    rq.buf = (unsigned long long)buf;
    rq.size = buf ? s : 0;
    return fsm(MOS_FSM_LISTXATTR, &rq);
}
long flistxattr(int fd, char *buf, size_t s, int o) {
    (void)o;
    mos_fsmeta_t rq = {0};
    rq.fd = fd;
    rq.buf = (unsigned long long)buf;
    rq.size = buf ? s : 0;
    return fsm(MOS_FSM_LISTXATTR, &rq);
}

/* ======================================================================== dono */
static int own(mos_fsmeta_t *rq, unsigned u, unsigned g) {
    rq->a = u;
    rq->b = g;
    return fsm(MOS_FSM_CHOWN, rq) < 0 ? -1 : 0;
}
int chown(const char *p, unsigned u, unsigned g) {
    char b[1024]; mos_fsmeta_t rq = {0};
    return target(&rq, p, 1, b, sizeof b) < 0 ? -1 : own(&rq, u, g);
}
int lchown(const char *p, unsigned u, unsigned g) {
    char b[1024]; mos_fsmeta_t rq = {0};
    return target(&rq, p, 0, b, sizeof b) < 0 ? -1 : own(&rq, u, g);
}
int fchown(int fd, unsigned u, unsigned g) { mos_fsmeta_t rq = {0}; rq.fd = fd; return own(&rq, u, g); }
int fchownat(int fd, const char *p, unsigned u, unsigned g, int fl) {
    char t[1024];
    const char *q = cx_at_path(fd, p, t, sizeof t);
    if (!q) return -1;
    return (fl & AT_SYMLINK_NOFOLLOW_) ? lchown(q, u, g) : chown(q, u, g);
}

/* ======================================================================== flags (chflags) */
static int flg(mos_fsmeta_t *rq, unsigned f) { rq->a = f; return fsm(MOS_FSM_CHFLAGS, rq) < 0 ? -1 : 0; }
int chflags(const char *p, unsigned f) { char b[1024]; mos_fsmeta_t rq = {0}; return target(&rq, p, 1, b, sizeof b) < 0 ? -1 : flg(&rq, f); }
int lchflags(const char *p, unsigned f) { char b[1024]; mos_fsmeta_t rq = {0}; return target(&rq, p, 0, b, sizeof b) < 0 ? -1 : flg(&rq, f); }
int fchflags(int fd, unsigned f) { mos_fsmeta_t rq = {0}; rq.fd = fd; return flg(&rq, f); }
int chflagsat(int fd, const char *p, unsigned f, int fl) {
    char t[1024];
    const char *q = cx_at_path(fd, p, t, sizeof t);
    if (!q) return -1;
    return (fl & AT_SYMLINK_NOFOLLOW_) ? lchflags(q, f) : chflags(q, f);
}

/* ======================================================================== datas */
typedef struct { long tv_sec; int tv_usec; } fm_timeval_t;          /* struct timeval do macOS (usec é int) */
typedef struct { long tv_sec; long tv_nsec; } fm_timespec_t;

/* acesso e modificação; NULL = agora (os dois) */
static int times2(mos_fsmeta_t *rq, const fm_timespec_t *ts) {
    rq->a = MOS_FSM_ATIME | MOS_FSM_MTIME;
    rq->b = 0;
    if (!ts) rq->b = MOS_FSM_ATIME | MOS_FSM_MTIME;
    else for (int i = 0; i < 2; i++) {
        unsigned bit = i ? MOS_FSM_MTIME : MOS_FSM_ATIME;
        if (ts[i].tv_nsec == UTIME_OMIT_) { rq->a &= ~bit; continue; }
        if (ts[i].tv_nsec == UTIME_NOW_) { rq->b |= bit; continue; }
        if (ts[i].tv_nsec < 0 || ts[i].tv_nsec >= 1000000000l) { SET(22); return -1; }
        rq->t[i] = ts[i].tv_sec * 1000000000ll + ts[i].tv_nsec;
    }
    if (!rq->a) return 0;
    return fsm(MOS_FSM_SETTIMES, rq) < 0 ? -1 : 0;
}
static int tv_to_ts(const fm_timeval_t *tv, fm_timespec_t ts[2]) {
    for (int i = 0; i < 2; i++) {
        if (tv[i].tv_usec < 0 || tv[i].tv_usec >= 1000000) { SET(22); return -1; }
        ts[i].tv_sec = tv[i].tv_sec;
        ts[i].tv_nsec = (long)tv[i].tv_usec * 1000;
    }
    return 0;
}
static int path_times(const char *p, int follow, const fm_timespec_t *ts) {
    char b[1024]; mos_fsmeta_t rq = {0};
    if (target(&rq, p, follow, b, sizeof b) < 0) return -1;
    return times2(&rq, ts);
}
int utimes(const char *p, const void *tv) {
    fm_timespec_t ts[2];
    if (tv && tv_to_ts(tv, ts) < 0) return -1;
    return path_times(p, 1, tv ? ts : 0);
}
int lutimes(const char *p, const void *tv) {
    fm_timespec_t ts[2];
    if (tv && tv_to_ts(tv, ts) < 0) return -1;
    return path_times(p, 0, tv ? ts : 0);
}
int futimes(int fd, const void *tv) {
    fm_timespec_t ts[2];
    mos_fsmeta_t rq = {0};
    if (tv && tv_to_ts(tv, ts) < 0) return -1;
    rq.fd = fd;
    return times2(&rq, tv ? ts : 0);
}
int utime(const char *p, const void *times) {          /* struct utimbuf { time_t actime, modtime; } */
    if (!times) return path_times(p, 1, 0);
    const long *u = times;
    fm_timespec_t ts[2] = {{u[0], 0}, {u[1], 0}};
    return path_times(p, 1, ts);
}
int futimens(int fd, const void *t) { mos_fsmeta_t rq = {0}; rq.fd = fd; return times2(&rq, t); }
int utimensat(int dfd, const char *p, const void *t, int flag) {
    char tp[1024];
    const char *q = cx_at_path(dfd, p, tp, sizeof tp);
    return q ? path_times(q, !(flag & AT_SYMLINK_NOFOLLOW_), t) : -1;
}
/* data de criação (setattrlist ATTR_CMN_CRTIME e NSFileManager setAttributes: NSFileCreationDate) */
int cx_set_birthtime(const char *abs, int fd, long long ns) {
    mos_fsmeta_t rq = {0};
    rq.fd = abs ? -1 : fd;
    rq.path = (unsigned long long)abs;
    rq.a = MOS_FSM_BTIME;
    rq.t[3] = ns;
    return fsm(MOS_FSM_SETTIMES, &rq) < 0 ? -1 : 0;
}

/* ======================================================================== copyfile: xattrs e stat */
#define COPYFILE_ACL_   (1 << 0)
#define COPYFILE_STAT_  (1 << 1)
#define COPYFILE_XATTR_ (1 << 2)
void *malloc(size_t n);
void free(void *p);
int chmod(const char *p, unsigned short m);
/* depois de copiar os dados: xattrs (COPYFILE_XATTR) e modo, dono e datas (COPYFILE_STAT), como o copyfile(3) */
int cx_copy_meta(const char *from, const char *to, unsigned flags) {
    char a[1024], b[1024];
    int r = 0;
    if (cx_follow_path(from, a, sizeof a) < 0) return -1;
    cx_path(to, b, sizeof b);
    if (flags & COPYFILE_XATTR_) {
        long n = listxattr(a, 0, 0, 0);
        char *names = n > 0 ? malloc((size_t)n) : 0;
        if (names && listxattr(a, names, (size_t)n, 0) == n) {
            for (long o = 0; o < n; o += (long)strlen(names + o) + 1) {
                const char *nm = names + o;
                long vl = getxattr(a, nm, 0, 0, 0, 0);
                if (vl < 0) continue;
                void *v = malloc(vl ? (size_t)vl : 1);
                if (!v) { r = -1; break; }
                if (getxattr(a, nm, v, (size_t)vl, 0, 0) == vl && setxattr(b, nm, v, (size_t)vl, 0, XATTR_NOFOLLOW) < 0) r = -1;
                free(v);
            }
        }
        free(names);
    }
    if (flags & COPYFILE_STAT_) {
        mos_statx_t x;
        if (cx_statx(a, -1, &x) == 0) {
            chmod(b, (unsigned short)(x.mode & 07777));
            chflags(b, x.flags & 0xffff);               /* SF_* só com root: o resto vai */
            fm_timespec_t ts[2] = {{x.atime_ns / 1000000000ll, x.atime_ns % 1000000000ll},
                                   {x.mtime_ns / 1000000000ll, x.mtime_ns % 1000000000ll}};
            path_times(b, 0, ts);
            cx_set_birthtime(b, -1, x.btime_ns);
        }
    }
    return r;
}
