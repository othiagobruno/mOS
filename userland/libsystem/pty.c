/*
 * mOS libSystem — terminais sobre o SYS_TTY do kernel (kernel/proc/pty.c):
 *   - pseudoterminais: posix_openpt/grantpt/unlockpt/ptsname(_r), openpty/forkpty/login_tty (o node-pty do
 *     terminal integrado do VS Code, do Cursor e do Claude usa posix_openpt + TIOCPTYGNAME + posix_spawn);
 *   - termios: tcgetattr/tcsetattr/cfmakeraw/cf*speed/tcflush/tcdrain/tcflow/tcsendbreak/tcgetwinsize;
 *   - isatty/ttyname(_r)/ctermid e os pedidos ioctl de terminal (cx_tty_ioctl, chamado pelo ioctl de libc.c);
 *   - sessões e grupos: setsid/setpgid/getpgid/getpgrp/getsid/tcgetpgrp/tcsetpgrp/tcgetsid/killpg;
 *   - sysctl KERN_PROC_PID com nome, grupo e terminal (cx_kinfo_proc, de darwin.c): o node-pty dá o título da aba
 *     pelo processo em primeiro plano (tcgetpgrp do mestre + p_comm).
 * O console do mOS (fds 0-2 fora da tabela do kernel: o Terminal do mOS, o log) segue como antes: é terminal se TERM
 * existe e não é "dumb", e o termios fica guardado no processo sem mudar a entrada. No harness (tools/machotest.c,
 * sem SYS_TTY) vale só esse comportamento antigo.
 */
#include "sys.h"

size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
size_t strlcpy(char *d, const char *s, size_t n);
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
char *getenv(const char *name);
int open(const char *path, int flags, ...);
int close(int fd);
int dup2(int a, int b);
int fork(void);
ssize_t write(int fd, const void *b, size_t n);
__attribute__((noreturn)) void _exit(int code);
int getpid(void);
int cx_sock_is(int fd) __attribute__((weak));

typedef unsigned long tcflag_t;
typedef unsigned long speed_t;
typedef struct { tcflag_t iflag, oflag, cflag, lflag; u8 cc[20]; speed_t ispeed, ospeed; } cx_termios_t;
typedef struct { u16 row, col, xpixel, ypixel; } cx_winsize_t;

/* sys/ttycom.h e sys/filio.h do macOS */
#define TIOCGETA     0x40487413UL
#define TIOCSETA     0x80487414UL
#define TIOCSETAW    0x80487415UL
#define TIOCSETAF    0x80487416UL
#define TIOCGWINSZ   0x40087468UL
#define TIOCSWINSZ   0x80087467UL
#define TIOCSCTTY    0x20007461UL
#define TIOCGPGRP    0x40047477UL
#define TIOCSPGRP    0x80047476UL
#define TIOCGSID     0x40047463UL
#define TIOCFLUSH    0x80047410UL
#define TIOCDRAIN    0x2000745eUL
#define TIOCSTOP     0x2000746fUL
#define TIOCSTART    0x2000746eUL
#define TIOCSBRK     0x2000747bUL
#define TIOCCBRK     0x2000747aUL
#define TIOCPTYGRANT 0x20007454UL
#define TIOCPTYGNAME 0x40807453UL
#define TIOCPTYUNLK  0x20007452UL
/* sys/termios.h do macOS */
#define BRKINT 0x2
#define ICRNL 0x100
#define INPCK 0x10
#define ISTRIP 0x20
#define IXON 0x200
#define IGNBRK 0x1
#define PARMRK 0x8
#define INLCR 0x40
#define IGNCR 0x80
#define OPOST 0x1
#define ONLCR 0x2
#define CSIZE 0x300
#define CS8 0x300
#define CREAD 0x800
#define PARENB 0x1000
#define ECHO 0x8
#define ECHOE 0x2
#define ECHOK 0x4
#define ECHONL 0x10
#define ICANON 0x100
#define ISIG 0x80
#define IEXTEN 0x400
#define VEOF 0
#define VEOL 1
#define VERASE 3
#define VKILL 5
#define VINTR 8
#define VQUIT 9
#define VSUSP 10
#define VSTART 12
#define VSTOP 13
#define VMIN 16
#define VTIME 17
#define MAC_O_NOCTTY 0x20000

static long tty_k(long op, long a, long b, long c) { return cx_syscall(SYS_TTY, op, a, b, c, 0); }
static int set_err(long r) { cx_errno = (int)-r; return -1; }
/* o kernel tem SYS_TTY? (o harness machotest não tem: devolve -EINVAL para tudo) */
static int ktty(void) {
    static int has = -1;
    if (has < 0) has = tty_k(MOS_TTY_GETPGID, 0, 0, 0) > 0;
    return has;
}
/* descritores que não são do kernel: kqueue (0x4000+) e sockets do libSystem (768-1023) */
static int libsys_fd(int fd) { return fd >= 0x4000 || (fd >= 768 && fd < 1024 && cx_sock_is && cx_sock_is(fd)); }
/* console: 0-2 sem entrada na tabela do kernel */
static int console_fd(int fd) { mos_stat_t st; return fd >= 0 && fd <= 2 && SYS2(SYS_FSTAT, fd, &st) < 0; }
static int console_tty(int fd) {
    const char *t = getenv("TERM");
    return console_fd(fd) && t && strcmp(t, "dumb");   /* o Terminal do mOS não interpreta ANSI: com TERM=dumb, não é tty */
}

/* ================================================================ console (comportamento antigo) */
static cx_termios_t con_tio;
static int con_tio_init;
static cx_termios_t *con_termios(void) {
    if (!con_tio_init) {
        con_tio_init = 1;
        con_tio.iflag = BRKINT | ICRNL | IXON;
        con_tio.oflag = OPOST | ONLCR;
        con_tio.cflag = CS8 | CREAD;
        con_tio.lflag = ECHO | ECHOE | ECHOK | ICANON | ISIG | IEXTEN;
        con_tio.cc[VEOF] = 4; con_tio.cc[VEOL] = 0xff; con_tio.cc[VERASE] = 0x7f; con_tio.cc[VKILL] = 21;
        con_tio.cc[VINTR] = 3; con_tio.cc[VQUIT] = 28; con_tio.cc[VSUSP] = 26; con_tio.cc[VSTART] = 17;
        con_tio.cc[VSTOP] = 19; con_tio.cc[VMIN] = 1; con_tio.cc[VTIME] = 0;
        con_tio.ispeed = con_tio.ospeed = 38400;
    }
    return &con_tio;
}
/* o terminal do mOS não muda de modo pelo processo: o estado é guardado e devolvido (crossterm, rustyline e afins
 * leem de volta o que pediram), e a entrada continua chegando como o Terminal do mOS a entrega */
static int console_ioctl(int fd, unsigned long req, void *arg) {
    (void)fd;
    switch (req) {
    case TIOCGETA: if (!arg) break; *(cx_termios_t *)arg = *con_termios(); return 0;
    case TIOCSETA: case TIOCSETAW: case TIOCSETAF: if (!arg) break; con_termios(); con_tio = *(const cx_termios_t *)arg; return 0;
    case TIOCGPGRP: if (!arg) break; *(int *)arg = getpid(); return 0;
    case TIOCSPGRP: case TIOCFLUSH: case TIOCDRAIN: return 0;
    default: cx_errno = CX_ENOTTY; return -1;
    }
    cx_errno = CX_EFAULT;
    return -1;
}

/* pedidos ioctl de terminal num descritor (libc.c: ioctl manda para cá o que não é FIONBIO/FIOCLEX) */
int cx_tty_ioctl(int fd, unsigned long req, void *arg) {
    if (fd < 0) { cx_errno = CX_EBADF; return -1; }
    if (libsys_fd(fd)) { cx_errno = CX_ENOTTY; return -1; }
    if (ktty()) {
        long r = tty_k(MOS_TTY_IOCTL, fd, (long)req, (long)arg);
        if (r >= 0) return (int)r;
        if (r != -MOS_EBADF) return set_err(r);
    }
    if (console_tty(fd)) return console_ioctl(fd, req, arg);
    cx_errno = ktty() && !console_fd(fd) ? CX_EBADF : CX_ENOTTY;   /* fora da tabela e não é o console: fd inválido */
    return -1;
}

/* ================================================================ isatty, ttyname */
int isatty(int fd) {
    if (fd < 0) { cx_errno = CX_EBADF; return 0; }
    if (libsys_fd(fd)) { cx_errno = CX_ENOTTY; return 0; }
    if (ktty()) {
        cx_termios_t t;
        long r = tty_k(MOS_TTY_IOCTL, fd, (long)TIOCGETA, (long)&t);
        if (r == 0) return 1;
        if (r != -MOS_EBADF) { cx_errno = CX_ENOTTY; return 0; }
    }
    if (console_tty(fd)) return 1;
    cx_errno = ktty() && !console_fd(fd) ? CX_EBADF : CX_ENOTTY;
    return 0;
}
int ttyname_r(int fd, char *buf, size_t n) {
    if (fd < 0) return CX_EBADF;
    if (!buf) return CX_EINVAL;
    if (libsys_fd(fd)) return CX_ENOTTY;
    if (ktty()) {
        char tmp[64];
        long r = tty_k(MOS_TTY_NAME, fd, (long)tmp, sizeof tmp);
        if (r >= 0) return strlcpy(buf, tmp, n) >= n ? CX_ERANGE : 0;
        if (r != -MOS_EBADF) return CX_ENOTTY;
    }
    /* console: um nome que não é de nenhum pseudoterminal (o libuv reabre o escravo pelo nome: "/dev/ttys000" levaria
     * um programa do Terminal do mOS para o terminal de outra pessoa) */
    if (console_tty(fd)) return strlcpy(buf, "/dev/console", n) >= n ? CX_ERANGE : 0;
    return CX_ENOTTY;
}
char *ttyname(int fd) {
    static char b[64];
    int e = ttyname_r(fd, b, sizeof b);
    if (e) { cx_errno = e; return 0; }
    return b;
}
char *ctermid(char *buf) {
    static char b[16];
    char *d = buf ? buf : b;
    strlcpy(d, "/dev/tty", 16);
    return d;
}

/* ================================================================ termios */
int tcgetattr(int fd, cx_termios_t *t) {
    if (!t) { cx_errno = CX_EINVAL; return -1; }
    return cx_tty_ioctl(fd, TIOCGETA, t);
}
int tcsetattr(int fd, int act, const cx_termios_t *t) {
    if (!t) { cx_errno = CX_EINVAL; return -1; }
    unsigned long req;
    switch (act & ~0x10) {                                       /* TCSASOFT: o mOS não tem "hardware" para preservar */
    case 0: req = TIOCSETA; break;                               /* TCSANOW */
    case 1: req = TIOCSETAW; break;                              /* TCSADRAIN */
    case 2: req = TIOCSETAF; break;                              /* TCSAFLUSH */
    default: cx_errno = CX_EINVAL; return -1;
    }
    return cx_tty_ioctl(fd, req, (void *)t);
}
void cfmakeraw(cx_termios_t *t) {
    if (!t) return;
    t->iflag &= ~(tcflag_t)(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    t->oflag &= ~(tcflag_t)OPOST;
    t->lflag &= ~(tcflag_t)(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    t->cflag &= ~(tcflag_t)(CSIZE | PARENB);
    t->cflag |= CS8;
    t->cc[VMIN] = 1; t->cc[VTIME] = 0;
}
speed_t cfgetispeed(const cx_termios_t *t) { return t ? t->ispeed : 0; }
speed_t cfgetospeed(const cx_termios_t *t) { return t ? t->ospeed : 0; }
int cfsetispeed(cx_termios_t *t, speed_t s) { if (!t) { cx_errno = CX_EINVAL; return -1; } t->ispeed = s; return 0; }
int cfsetospeed(cx_termios_t *t, speed_t s) { if (!t) { cx_errno = CX_EINVAL; return -1; } t->ospeed = s; return 0; }
int cfsetspeed(cx_termios_t *t, speed_t s) { if (!t) { cx_errno = CX_EINVAL; return -1; } t->ispeed = t->ospeed = s; return 0; }
int tcflush(int fd, int which) {
    int com;
    switch (which) {
    case 1: com = 1; break;                                      /* TCIFLUSH -> FREAD */
    case 2: com = 2; break;                                      /* TCOFLUSH -> FWRITE */
    case 3: com = 3; break;                                      /* TCIOFLUSH */
    default: cx_errno = CX_EINVAL; return -1;
    }
    return cx_tty_ioctl(fd, TIOCFLUSH, &com);
}
int tcdrain(int fd) { return cx_tty_ioctl(fd, TIOCDRAIN, 0); }
int tcflow(int fd, int action) {
    cx_termios_t t;
    u8 c;
    switch (action) {
    case 1: return cx_tty_ioctl(fd, TIOCSTOP, 0);               /* TCOOFF */
    case 2: return cx_tty_ioctl(fd, TIOCSTART, 0);              /* TCOON */
    case 3: case 4:                                              /* TCIOFF / TCION: manda o caractere de STOP/START */
        if (tcgetattr(fd, &t) < 0) return -1;
        c = t.cc[action == 3 ? VSTOP : VSTART];
        if (c != 0xff && write(fd, &c, 1) != 1) return -1;
        return 0;
    }
    cx_errno = CX_EINVAL;
    return -1;
}
int tcsendbreak(int fd, int dur) {
    (void)dur;
    if (cx_tty_ioctl(fd, TIOCSBRK, 0) < 0) return -1;
    return cx_tty_ioctl(fd, TIOCCBRK, 0);
}
int tcgetwinsize(int fd, cx_winsize_t *w) { return cx_tty_ioctl(fd, TIOCGWINSZ, w); }
int tcsetwinsize(int fd, const cx_winsize_t *w) { return cx_tty_ioctl(fd, TIOCSWINSZ, (void *)w); }

/* ================================================================ pseudoterminais */
int posix_openpt(int flags) { return open("/dev/ptmx", flags); }
int grantpt(int fd) { return cx_tty_ioctl(fd, TIOCPTYGRANT, 0); }
int unlockpt(int fd) { return cx_tty_ioctl(fd, TIOCPTYUNLK, 0); }
int ptsname_r(int fd, char *buf, size_t len) {                   /* como o do macOS: -1 e errno */
    char n[128];
    if (!buf) { cx_errno = CX_EINVAL; return -1; }
    if (cx_tty_ioctl(fd, TIOCPTYGNAME, n) < 0) return -1;
    if (strlcpy(buf, n, len) >= len) { cx_errno = CX_ERANGE; return -1; }
    return 0;
}
char *ptsname(int fd) {
    static char b[128];
    return ptsname_r(fd, b, sizeof b) ? 0 : b;
}
int openpty(int *amaster, int *aslave, char *name, const cx_termios_t *termp, const cx_winsize_t *winp) {
    char n[128];
    int m = posix_openpt(MAC_O_RDWR | MAC_O_NOCTTY);
    if (m < 0) return -1;
    if (grantpt(m) < 0 || unlockpt(m) < 0 || ptsname_r(m, n, sizeof n) < 0) { int e = cx_errno; close(m); cx_errno = e; return -1; }
    int s = open(n, MAC_O_RDWR | MAC_O_NOCTTY);
    if (s < 0) { int e = cx_errno; close(m); cx_errno = e; return -1; }
    if (termp) tcsetattr(s, 2, termp);
    if (winp) cx_tty_ioctl(s, TIOCSWINSZ, (void *)winp);
    if (amaster) *amaster = m;
    if (aslave) *aslave = s;
    if (name) strlcpy(name, n, sizeof n);                        /* o chamador dá pelo menos o tamanho de um nome de tty */
    return 0;
}
int login_tty(int fd) {
    extern int setsid(void);
    setsid();
    if (cx_tty_ioctl(fd, TIOCSCTTY, 0) < 0) return -1;
    dup2(fd, 0);
    dup2(fd, 1);
    dup2(fd, 2);
    if (fd > 2) close(fd);
    return 0;
}
int forkpty(int *amaster, char *name, const cx_termios_t *termp, const cx_winsize_t *winp) {
    int m, s;
    if (openpty(&m, &s, name, termp, winp) < 0) return -1;
    int pid = fork();
    if (pid < 0) { int e = cx_errno; close(m); close(s); cx_errno = e; return -1; }
    if (pid == 0) {
        close(m);
        if (login_tty(s) < 0) _exit(1);
        return 0;
    }
    close(s);
    if (amaster) *amaster = m;
    return pid;
}

/* ================================================================ sessões e grupos de processos */
int setsid(void) {
    if (!ktty()) return getpid();
    long r = tty_k(MOS_TTY_SETSID, 0, 0, 0);
    return r < 0 ? set_err(r) : (int)r;
}
int setpgid(int pid, int pgid) {
    if (!ktty()) return 0;
    if (pid < 0 || pgid < 0) { cx_errno = CX_EINVAL; return -1; }
    long r = tty_k(MOS_TTY_SETPGID, pid, pgid, 0);
    return r < 0 ? set_err(r) : 0;
}
int getpgid(int pid) {
    if (!ktty()) return pid ? pid : getpid();
    long r = tty_k(MOS_TTY_GETPGID, pid, 0, 0);
    return r < 0 ? set_err(r) : (int)r;
}
int getpgrp(void) { return getpgid(0); }
int getsid(int pid) {
    if (!ktty()) return pid ? pid : getpid();
    long r = tty_k(MOS_TTY_GETSID, pid, 0, 0);
    return r < 0 ? set_err(r) : (int)r;
}
int tcgetpgrp(int fd) {
    int pg = -1;
    if (cx_tty_ioctl(fd, TIOCGPGRP, &pg) < 0) return -1;
    return pg;
}
int tcsetpgrp(int fd, int pg) { return cx_tty_ioctl(fd, TIOCSPGRP, &pg); }
int tcgetsid(int fd) {
    int s = -1;
    if (cx_tty_ioctl(fd, TIOCGSID, &s) < 0) return -1;
    return s;
}
/* números do macOS -> do kernel (a mesma bijeção de libc.c: USR1/USR2/CHLD/STOP/BUS/SYS) */
static int sig_mos(int s) {
    switch (s) { case 10: return 30; case 12: return 31; case 30: return 10; case 31: return 12; case 20: return 17; case 17: return 20; }
    return s;
}
/* kill(0, s), kill(-pg, s) e killpg: o grupo inteiro (pg 0 = o de quem chama) */
int cx_kill_group(int pg, int sig) {
    if (sig < 0 || sig >= 32) { cx_errno = CX_EINVAL; return -1; }
    if (!ktty()) {                                               /* sem grupos no kernel: o próprio processo/o líder */
        long r = SYS2(SYS_SIGKILL, pg ? pg : getpid(), sig ? sig_mos(sig) : 0);
        if (r < 0) { cx_errno = CX_ESRCH; return -1; }
        return 0;
    }
    long r = tty_k(MOS_TTY_KILLPG, pg, sig ? sig_mos(sig) : 0, 0);
    return r < 0 ? set_err(r) : 0;
}
int killpg(int pg, int sig) {
    if (pg < 0) { cx_errno = CX_EINVAL; return -1; }
    return cx_kill_group(pg, sig);
}

/* ================================================================ sysctl KERN_PROC (darwin.c)
 * struct kinfo_proc do macOS x86_64 (648 bytes): extern_proc (p_flag @32, p_stat @36, p_pid @40, p_comm[17] @243) e
 * eproc a partir de 296 (e_ppid @560, e_pgid @564, e_tdev @572, e_tpgid @576). KERN_PROC_PID preenche esses campos;
 * o resto (KERN_PROC_ALL...) continua como antes: a struct zerada, que diz "sem depurador" (p_flag sem P_TRACED). */
int cx_kinfo_proc(int *mib, unsigned n, void *old, size_t *oldlen) {
    enum { KP_SIZE = 648 };
    if (!oldlen) return 0;
    if (n >= 4 && mib[2] == 1) {                                 /* KERN_PROC_PID */
        if (!old) { *oldlen = KP_SIZE; return 0; }
        if (*oldlen < KP_SIZE) { cx_errno = CX_ENOMEM; return -1; }
        u8 *k = old;
        memset(k, 0, KP_SIZE);
        int pid = mib[3];
        mos_procinfo_t pi;
        int found = 0;
        for (long i = 0; SYS2(SYS_PROCLIST, i, &pi) == 1; i++) if (pi.pid == pid) { found = 1; break; }
        if (!found) { *oldlen = 0; return 0; }                   /* o macOS devolve 0 bytes para um pid que não existe */
        *(i32 *)(k + 40) = pid;                                  /* p_pid */
        k[36] = 2;                                               /* p_stat = SRUN */
        size_t ln = strlen(pi.name);
        memcpy(k + 243, pi.name, ln > 16 ? 16 : ln);             /* p_comm (MAXCOMLEN = 16) */
        *(i32 *)(k + 560) = pi.ppid > 0 ? pi.ppid : 1;           /* e_ppid */
        *(i32 *)(k + 572) = -1;                                  /* e_tdev = NODEV */
        i32 info[4];
        if (ktty() && tty_k(MOS_TTY_PROCINFO, pid, (long)info, 0) == 0) {
            *(i32 *)(k + 564) = info[0];                         /* e_pgid */
            if (info[2] >= 0) {
                *(i32 *)(k + 572) = (16 << 24) | info[2];        /* makedev(16, n): /dev/ttysNNN */
                *(i32 *)(k + 32) |= 0x2;                         /* P_CONTROLT */
            }
            *(i32 *)(k + 576) = info[3];                         /* e_tpgid */
        }
        *oldlen = KP_SIZE;
        return 0;
    }
    if (old) memset(old, 0, *oldlen); else *oldlen = KP_SIZE;
    return 0;
}
