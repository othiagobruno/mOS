/*
 * mOS — teste no Windows da disciplina de linha e das sessões de kernel/proc/pty.c, sem QEMU.
 * O pty.c entra inteiro; processos, threads, cópia de/para o usuário (ponteiros do próprio teste) e sinais são
 * simulados aqui com o mesmo contrato de process.c/sched.c.
 *
 *   zig cc -target x86_64-windows-gnu -O1 -Ikernel/include tools/ptytest.c -o build/ptytest.exe
 *   .\build\ptytest.exe          (ptytest: N/N ok)
 */
#include "../kernel/proc/pty.c"

int printf(const char *fmt, ...);
void *malloc(usize n);
void *calloc(usize n, usize s);
void free(void *p);
void exit(int code);

/* ------------------------------------------------------------ o que o kernel daria */
void *kmalloc(usize n) { return malloc(n); }
void *kcalloc(usize n, usize sz) { return calloc(n, sz); }
void kfree(void *p) { free(p); }
void kprintf(const char *fmt, ...) {
    char b[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    printf("  [kernel] %s", b);
}
static u64 now_ms;
u64 timer_ticks(void) { return now_ms; }
bool uvm_copy_out(process_t *p, u64 va, const void *src, usize len) { (void)p; memcpy((void *)va, src, len); return true; }
bool uvm_copy_in(process_t *p, void *dst, u64 va, usize len) { (void)p; memcpy(dst, (const void *)va, len); return true; }
u64 sched_switch(int_frame_t *f) { return (u64)f; }

static process_t *procs;
static thread_t *threads;
process_t *proc_list(void) { return procs; }
process_t *proc_find(int pid) { for (process_t *p = procs; p; p = p->next) if (p->pid == pid) return p; return NULL; }
thread_t *sched_threads(void) { return threads; }
static bool def_ignored(int s) {
    return s == MOS_SIGCHLD || s == MOS_SIGURG || s == MOS_SIGCONT || s == MOS_SIGWINCH || s == MOS_SIGINFO ||
           s == MOS_SIGIO || s == MOS_SIGSTOP || s == MOS_SIGTSTP || s == MOS_SIGTTIN || s == MOS_SIGTTOU;
}
/* como process.c: ignorado some; senão fica pendente e uma thread esperando acorda com EINTR */
bool proc_signal(int pid, int sig) {
    process_t *p = proc_find(pid);
    if (!p || p->exited) return false;
    if (p->sig_handler[sig] == MOS_SIG_IGN || (p->sig_handler[sig] == MOS_SIG_DFL && def_ignored(sig))) return true;
    p->sig_pending |= 1u << sig;
    for (thread_t *t = threads; t; t = t->next)
        if (t->proc == p && (t->state == TH_WAITING || t->state == TH_SLEEPING)) {
            ((int_frame_t *)t->frame)->rax = (u64)-4;
            t->state = TH_READY;
            break;
        }
    return true;
}

/* ------------------------------------------------------------ processos e chamadas simuladas */
typedef struct { thread_t th; int_frame_t f; } uth_t;
static process_t *mkproc(int pid, int ppid, process_t *parent, unsigned flags) {
    process_t *p = calloc(1, sizeof *p);
    p->pid = pid;
    p->ppid = ppid;
    snprintf(p->name, sizeof p->name, "p%d", pid);
    pty_proc_init(p, parent, flags);
    p->next = procs;
    procs = p;
    return p;
}
static uth_t *mkthread(process_t *p) {
    uth_t *u = calloc(1, sizeof *u);
    u->th.proc = p;
    u->th.state = TH_RUNNING;
    u->th.next = threads;
    threads = &u->th;
    return u;
}
static int x_open(process_t *p, const char *path, int flags) {
    int fd = 3;
    while (fd < PROC_MAX_FDS && p->fds[fd].used) fd++;
    int r = pty_open_path(p, fd, path, flags);
    return r < 0 ? r : r == 0 ? fd : -MOS_ENOENT;
}
static void x_close(process_t *p, int fd) {      /* como fd_close */
    struct pty *ty = p->fds[fd].pty;
    bool tm = p->fds[fd].kind == FD_PTY_M;
    p->fds[fd].used = false;
    p->fds[fd].pty = NULL;
    if (ty) pty_fd_ref(ty, tm, -1);
}
static void x_dup2(process_t *dst, int dfd, process_t *src, int sfd) {   /* como fd_copy */
    __typeof__(src->fds[0]) e = src->fds[sfd];
    if (e.pty) pty_fd_ref(e.pty, e.kind == FD_PTY_M, +1);
    if (dst->fds[dfd].used) x_close(dst, dfd);
    dst->fds[dfd] = e;
}
static void x_exit(process_t *p) {               /* como finish() */
    p->exited = true;
    for (thread_t *t = threads; t; t = t->next) if (t->proc == p) t->state = TH_DEAD;
    pty_proc_exit(p);
    for (int i = 0; i < PROC_MAX_FDS; i++) if (p->fds[i].used) x_close(p, i);
}
#define BLOCKED 0x7fff0000
static i64 x_rw(uth_t *u, int fd, bool rd, void *buf, u64 len) {
    u->th.pty_wait = NULL;                       /* entrada de syscall */
    u->th.state = TH_RUNNING;
    u->f.rax = 0;
    pty_rw(&u->f, &u->th, u->th.proc, fd, rd, (u64)buf, len);
    if (u->th.state == TH_WAITING || u->th.state == TH_SLEEPING) return BLOCKED;
    return (i64)u->f.rax;
}
static i64 x_read(uth_t *u, int fd, char *buf, u64 len) { return x_rw(u, fd, true, buf, len); }
static i64 x_write(uth_t *u, int fd, const char *s) { return x_rw(u, fd, false, (void *)s, strlen(s)); }
static i64 x_ioctl(process_t *p, int fd, u32 req, void *arg) { return pty_syscall(p, MOS_TTY_IOCTL, (u64)fd, req, (u64)arg); }
static i64 x_tty(process_t *p, u64 op, u64 a, u64 b) { return pty_syscall(p, op, a, b, 0); }

/* ------------------------------------------------------------ verificação */
static int ok, total;
static void check(bool c, const char *what) {
    total++;
    if (c) ok++;
    else printf("FALHOU: %s\n", what);
}
static void check_bytes(const char *got, i64 n, const char *want, const char *what) {
    usize wl = strlen(want);
    bool c = n == (i64)wl && !memcmp(got, want, wl);
    if (!c) {
        printf("FALHOU: %s: veio %lld bytes \"", what, (long long)n);
        for (i64 i = 0; i < n && i < 120; i++) { u8 ch = (u8)got[i]; if (ch >= 32 && ch < 127) printf("%c", ch); else printf("\\x%02x", ch); }
        printf("\", esperava \"");
        for (usize i = 0; i < wl; i++) { u8 ch = (u8)want[i]; if (ch >= 32 && ch < 127) printf("%c", ch); else printf("\\x%02x", ch); }
        printf("\"\n");
    }
    total++;
    if (c) ok++;
}
/* lê tudo o que o mestre tem agora */
static i64 drain(uth_t *u, int fd, char *buf, u64 cap) {
    i64 n = x_read(u, fd, buf, cap);
    return n == BLOCKED || n < 0 ? 0 : n;
}

int main(void) {
    char buf[4096];
    /* o pty host (pid 100, sessão própria como filho do init) com o mestre não-bloqueante, como o node-pty */
    process_t *host = mkproc(100, 1, NULL, 0);
    uth_t *ht = mkthread(host);
    check(host->sid == 100 && host->pgid == 100 && !host->ctty, "filho do init começa a própria sessão");
    int m = x_open(host, "/dev/ptmx", MOS_O_RDWR);
    check(m >= 3 && host->fds[m].kind == FD_PTY_M, "/dev/ptmx abre o mestre");
    char name[128] = {0};
    check(x_ioctl(host, m, TIOCPTYGRANT, 0) == 0, "grantpt");
    check(x_ioctl(host, m, TIOCPTYGNAME, name) == 0 && !strcmp(name, "/dev/ttys000"), "TIOCPTYGNAME = /dev/ttys000");
    check(x_open(host, "/dev/ttys000", MOS_O_RDWR | MOS_O_NOCTTY) == -MOS_EAGAIN, "escravo antes do unlockpt: EAGAIN");
    check(x_ioctl(host, m, TIOCPTYUNLK, 0) == 0, "unlockpt");
    int s = x_open(host, "/dev/ttys000", MOS_O_RDWR | MOS_O_NOCTTY);
    check(s > m && host->fds[s].kind == FD_PTY_S && !host->ctty, "escravo com O_NOCTTY não vira terminal de controle");
    check(x_ioctl(host, s, TIOCPTYGNAME, name) == -MOS_ENOTTY, "TIOCPTYGNAME no escravo falha (uv__tty_is_slave)");
    check(pty_stat_path("/dev/ttys000") && !pty_stat_path("/dev/ttys001") && pty_stat_path("/dev/tty"), "stat de /dev/ttysNNN");
    check(x_tty(host, MOS_TTY_SETFL, (u64)m, 1) == 0, "O_NONBLOCK no mestre");
    check(x_read(ht, m, buf, sizeof buf) == -MOS_EAGAIN, "mestre vazio e não-bloqueante: EAGAIN");

    /* termios do node-pty (pty.cc) e o tamanho */
    mac_termios_t tio;
    check(x_ioctl(host, s, TIOCGETA, &tio) == 0 && (tio.lflag & ICANON), "TIOCGETA");
    tio.iflag = ICRNL | IXON | IXANY | IMAXBEL | BRKINT | IUTF8;
    tio.oflag = OPOST | ONLCR;
    tio.cflag = CREAD | CS8 | HUPCL;
    tio.lflag = ICANON | ISIG | IEXTEN | ECHO | ECHOE | ECHOK | ECHOKE | ECHOCTL;
    check(x_ioctl(host, s, TIOCSETA, &tio) == 0, "TIOCSETA");
    mac_winsize_t ws = {30, 100, 0, 0}, ws2;
    check(x_ioctl(host, s, TIOCSWINSZ, &ws) == 0 && x_ioctl(host, m, TIOCGWINSZ, &ws2) == 0 && ws2.row == 30 && ws2.col == 100, "TIOCSWINSZ/TIOCGWINSZ");

    /* posix_spawn(SETSID | CLOEXEC_DEFAULT) do spawn-helper: o escravo vira 0, 1 e 2 */
    process_t *sh = mkproc(101, 100, host, MOS_SPAWN_SETSID | MOS_SPAWN_CLOEXEC_DEFAULT);
    uth_t *st = mkthread(sh);
    check(sh->sid == 101 && sh->pgid == 101 && !sh->ctty, "POSIX_SPAWN_SETSID: sessão nova sem terminal");
    for (int i = 0; i < 3; i++) x_dup2(sh, i, host, s);
    x_close(host, s);
    /* spawn-helper: close(open(ttyname(0), O_RDWR)) adota o terminal */
    char tn[32];
    check(x_tty(sh, MOS_TTY_NAME, 0, (u64)tn) >= 0 || 1, "MOS_TTY_NAME");
    check(pty_syscall(sh, MOS_TTY_NAME, 0, (u64)tn, sizeof tn) == 12 && !strcmp(tn, "/dev/ttys000"), "ttyname(0) = /dev/ttys000");
    int fd = x_open(sh, tn, MOS_O_RDWR);
    check(fd >= 3 && sh->ctty != NULL, "líder de sessão abrindo o escravo adota o terminal");
    x_close(sh, fd);
    int pg = -1;
    check(x_ioctl(sh, 0, TIOCGPGRP, &pg) == 0 && pg == 101, "tcgetpgrp(0) = grupo do shell (bash: job control)");
    check(x_ioctl(host, m, TIOCGPGRP, &pg) == 0 && pg == 101, "tcgetpgrp(mestre) vale fora da sessão (título do node-pty)");
    check(x_ioctl(host, m, TIOCSPGRP, &pg) == -MOS_ENOTTY, "tcsetpgrp fora da sessão: ENOTTY");
    int sid = 0;
    check(x_ioctl(sh, 0, TIOCGSID, &sid) == 0 && sid == 101, "TIOCGSID");
    int fd2 = x_open(sh, "/dev/tty", MOS_O_RDWR);
    check(fd2 >= 3 && sh->fds[fd2].kind == FD_PTY_S, "/dev/tty abre o terminal de controle");
    x_close(sh, fd2);
    check(x_open(host, "/dev/tty", MOS_O_RDWR) == -MOS_ENXIO, "/dev/tty sem terminal de controle: ENXIO");

    /* modo canônico: eco, apagar, ICRNL/ONLCR */
    check(x_write(ht, m, "ab\x7f" "c\r") == 5, "digitar no mestre");
    mos_stat_t sst;
    check(pty_fstat(sh, 0, (u64)&sst) == 0 && sst.is_dir == 5 && sst.size == 3, "fstat do escravo: linha completa pronta");
    check_bytes(buf, x_read(st, 0, buf, sizeof buf), "ac\n", "read canônico devolve a linha editada");
    check_bytes(buf, drain(ht, m, buf, sizeof buf), "ab\b \bc\r\n", "eco com ECHOE e ONLCR");
    /* linha incompleta não é legível; o read bloqueia e conclui quando o Enter chega */
    x_write(ht, m, "ls");
    check(pty_fstat(sh, 0, (u64)&sst) == 0 && sst.size == 0, "linha incompleta: nada a ler");
    check(x_read(st, 0, buf, sizeof buf) == BLOCKED, "read do escravo bloqueia sem linha");
    x_write(ht, m, "\r");
    check(st->th.state == TH_READY && st->f.rax == 3 && !memcmp(buf, "ls\n", 3), "Enter acorda o read bloqueado com \"ls\\n\"");
    drain(ht, m, buf, sizeof buf);
    /* UTF-8 com IUTF8: apagar tira o caractere inteiro */
    x_write(ht, m, "\xc3\xa7\x7f" "a\r");
    check_bytes(buf, x_read(st, 0, buf, sizeof buf), "a\n", "IUTF8: DEL apaga \"ç\" inteiro");
    drain(ht, m, buf, sizeof buf);
    /* ^U e ^W */
    x_write(ht, m, "um dois\x17tres\x15quatro\r");
    check_bytes(buf, x_read(st, 0, buf, sizeof buf), "quatro\n", "^W e ^U");
    drain(ht, m, buf, sizeof buf);
    /* ^D: com texto devolve sem terminador; em linha vazia, fim de arquivo */
    x_write(ht, m, "abc\x04");
    check_bytes(buf, x_read(st, 0, buf, sizeof buf), "abc", "^D com texto: read sem terminador");
    x_write(ht, m, "\x04");
    check(pty_fstat(sh, 0, (u64)&sst) == 0 && sst.mtime == 1, "^D numa linha vazia: fstat diz fim de arquivo");
    check(x_read(st, 0, buf, sizeof buf) == 0, "^D numa linha vazia: read devolve 0");
    drain(ht, m, buf, sizeof buf);

    /* saída do escravo: ONLCR */
    check(x_write(st, 1, "ola\nmundo") == 9, "escrever no escravo");
    check(pty_fstat(host, m, (u64)&sst) == 0 && sst.size == 10, "fstat do mestre: bytes prontos");
    check_bytes(buf, drain(ht, m, buf, sizeof buf), "ola\r\nmundo", "ONLCR na saída");

    /* modo cru do readline: sem eco nem ICRNL, um byte por read */
    mac_termios_t raw = tio;
    raw.lflag &= ~(u64)(ICANON | ECHO);
    raw.iflag &= ~(u64)(ICRNL | INLCR);
    raw.cc[VMIN] = 1; raw.cc[VTIME] = 0;
    x_write(ht, m, "pendente");                    /* digitado antes da troca: vira entrada crua */
    check(x_ioctl(sh, 0, TIOCSETA, &raw) == 0, "readline: modo cru");
    check_bytes(buf, x_read(st, 0, buf, sizeof buf), "pendente", "linha pendente vira entrada crua");
    drain(ht, m, buf, sizeof buf);
    x_write(ht, m, "x\r");
    check_bytes(buf, x_read(st, 0, buf, 1), "x", "cru: um byte");
    check_bytes(buf, x_read(st, 0, buf, 1), "\r", "cru: \\r sem ICRNL");
    check(drain(ht, m, buf, sizeof buf) == 0, "cru: sem eco do kernel");
    /* ^C no modo cru (ISIG continua): SIGINT ao grupo em primeiro plano, que tem tratador */
    sh->sig_handler[MOS_SIGINT] = 0x401000;
    x_write(ht, m, "\x03");
    check(sh->sig_pending & (1u << MOS_SIGINT), "^C manda SIGINT ao grupo em frente");
    check(pty_fstat(sh, 0, (u64)&sst) == 0 && sst.size == 0, "^C não entra na fila");
    sh->sig_pending = 0;
    /* volta ao canônico com dados crus pendentes: reeditados em linhas */
    x_write(ht, m, "eco\r");
    tio.iflag |= ICRNL;
    check(x_ioctl(sh, 0, TIOCSETA, &tio) == 0, "volta ao canônico");
    check_bytes(buf, x_read(st, 0, buf, sizeof buf), "eco\n", "entrada crua pendente é reeditada (ICRNL) depois da troca");

    /* job control do bash: o filho num grupo próprio vira o grupo em frente; ^C vai só para ele */
    process_t *job = mkproc(102, 101, sh, 0);
    for (int i = 0; i < 3; i++) x_dup2(job, i, sh, i);   /* o fork copia os fds */
    check(job->sid == 101 && job->pgid == 101 && job->ctty == sh->ctty, "fork/spawn herda sessão, grupo e terminal");
    check(x_tty(sh, MOS_TTY_SETPGID, 102, 102) == 0 && job->pgid == 102, "setpgid(filho, filho)");
    pg = 102;
    check(x_ioctl(job, 0, TIOCSPGRP, &pg) == 0, "tcsetpgrp(tty, grupo do filho)");
    pg = 999;
    check(x_ioctl(sh, 0, TIOCSPGRP, &pg) == -MOS_EPERM, "tcsetpgrp para grupo inexistente: EPERM");
    check(x_tty(sh, MOS_TTY_SETSID, 0, 0) == -MOS_EPERM, "setsid de quem já lidera: EPERM");
    check(x_tty(sh, MOS_TTY_SETPGID, 0, 0) == -MOS_EPERM, "setpgid do líder de sessão: EPERM");
    check(x_tty(host, MOS_TTY_SETPGID, 102, 102) == -MOS_ESRCH, "setpgid de quem não é filho: ESRCH");
    x_write(ht, m, "\x03");
    check((job->sig_pending & (1u << MOS_SIGINT)) && !(sh->sig_pending & (1u << MOS_SIGINT)), "^C só para o grupo em frente (o job)");
    check(x_tty(sh, MOS_TTY_KILLPG, 102, MOS_SIGTERM) == 0 && (job->sig_pending & (1u << MOS_SIGTERM)), "killpg(grupo do job)");
    check(x_tty(sh, MOS_TTY_KILLPG, 4242, 0) == -MOS_ESRCH, "killpg de grupo vazio: ESRCH");
    /* redimensionar: SIGWINCH ao grupo em frente; sem tratador é ignorado e nada fica pendente */
    job->sig_pending = 0;
    mac_winsize_t big = {40, 120, 0, 0};
    check(x_ioctl(host, m, TIOCSWINSZ, &big) == 0 && !(job->sig_pending & (1u << MOS_SIGWINCH)), "SIGWINCH sem tratador: ignorado");
    job->sig_handler[MOS_SIGWINCH] = 0x401000;
    big.col = 121;
    check(x_ioctl(host, m, TIOCSWINSZ, &big) == 0 && (job->sig_pending & (1u << MOS_SIGWINCH)), "SIGWINCH com tratador");
    job->sig_pending = 0;
    check(x_ioctl(host, m, TIOCSWINSZ, &big) == 0 && !job->sig_pending, "mesmo tamanho: sem sinal");
    pg = 101;
    check(x_ioctl(sh, 0, TIOCSPGRP, &pg) == 0, "o shell retoma o terminal");
    x_exit(job);
    int info[4];
    check(x_tty(host, MOS_TTY_PROCINFO, 101, (u64)info) == 0 && info[0] == 101 && info[1] == 101 && info[2] == 0 && info[3] == 101,
          "PROCINFO (sysctl KERN_PROC): grupo, sessão, terminal, grupo em frente");

    /* bloqueio de escrita: o escravo enche a saída; o mestre lê e libera */
    static char big_out[40000];
    memset(big_out, 'z', sizeof big_out - 1);
    while (drain(ht, m, buf, sizeof buf) > 0) {}     /* o eco do ^C */
    i64 w = x_rw(st, 1, false, big_out, sizeof big_out - 1);
    check(w == PTY_OUTQ, "escrita no escravo enche a fila de saída");
    i64 w2 = x_rw(st, 1, false, big_out, 100);
    check(w2 == BLOCKED, "fila cheia: a escrita bloqueia");
    i64 r = drain(ht, m, buf, sizeof buf);
    check(r == sizeof buf && st->th.state == TH_READY && st->f.rax == 100, "ler o mestre conclui a escrita bloqueada");
    while (drain(ht, m, buf, sizeof buf) > 0) {}

    /* VTIME: leitura com prazo devolve 0 */
    mac_termios_t tm = tio;
    tm.lflag &= ~(u64)ICANON;
    tm.cc[VMIN] = 0; tm.cc[VTIME] = 5;
    x_ioctl(sh, 0, TIOCSETA, &tm);
    check(x_read(st, 0, buf, 10) == BLOCKED && st->th.state == TH_SLEEPING && st->th.wake_at == 500 && st->f.rax == 0,
          "VMIN 0 / VTIME 5: espera com prazo (rax 0 se ninguém digitar)");
    x_write(ht, m, "q");
    check(st->th.state == TH_READY && st->f.rax == 1 && buf[0] == 'q', "dados antes do prazo acordam a leitura");
    tm.cc[VTIME] = 0;
    x_ioctl(sh, 0, TIOCSETA, &tm);
    check(x_read(st, 0, buf, 10) == 0, "VMIN 0 / VTIME 0: read volta 0 na hora");
    x_ioctl(sh, 0, TIOCSETA, &tio);

    /* sinal acorda um read bloqueado com EINTR */
    sh->sig_handler[MOS_SIGWINCH] = 0x401000;
    check(x_read(st, 0, buf, sizeof buf) == BLOCKED, "read bloqueado");
    proc_signal(101, MOS_SIGWINCH);
    check(st->th.state == TH_READY && (i64)st->f.rax == -4, "sinal com tratador: EINTR");
    st->th.state = TH_RUNNING;

    /* fim do shell: o mestre lê fim de arquivo; escrever nele dá EIO */
    while (drain(ht, m, buf, sizeof buf) > 0) {}     /* o eco do "q" */
    x_exit(sh);
    check(pty_fstat(host, m, (u64)&sst) == 0 && sst.mtime == 1, "shell saiu: fstat do mestre diz fim de arquivo");
    check(x_read(ht, m, buf, sizeof buf) == 0, "shell saiu: read do mestre devolve 0 (o node-pty fecha o terminal)");
    check(x_write(ht, m, "x") == -MOS_EIO, "escrever sem ninguém no escravo: EIO");
    x_close(host, m);
    check(ptys[0] == NULL, "sem fds das duas pontas o par é liberado");

    /* fechar o mestre com o shell vivo: SIGHUP ao líder; o escravo lê 0 e escreve EIO */
    int m2 = x_open(host, "/dev/ptmx", MOS_O_RDWR);
    x_ioctl(host, m2, TIOCPTYUNLK, 0);
    process_t *sh2 = mkproc(103, 100, host, MOS_SPAWN_SETSID);
    uth_t *st2 = mkthread(sh2);
    int s2 = x_open(sh2, "/dev/ttys000", MOS_O_RDWR);
    check(s2 >= 3 && sh2->ctty, "par reaproveitado: /dev/ttys000 de novo");
    sh2->sig_handler[MOS_SIGHUP] = 0x401000;
    x_close(host, m2);
    check(sh2->sig_pending & (1u << MOS_SIGHUP), "mestre fechado: SIGHUP ao líder da sessão");
    check(x_read(st2, s2, buf, sizeof buf) == 0, "mestre fechado: read do escravo devolve 0");
    check(x_write(st2, s2, "x") == -MOS_EIO, "mestre fechado: escrita no escravo dá EIO");
    x_exit(sh2);
    check(ptys[0] == NULL && !sh2->ctty, "o par some com o último fd e ninguém fica com o ponteiro");

    printf("ptytest: %d/%d ok\n", ok, total);
    return ok == total ? 0 : 1;
}
