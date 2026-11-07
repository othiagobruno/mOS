/*
 * mOS — teste dos pseudoterminais pelo caminho do node-pty (terminal integrado do VS Code). Compila igual no macOS:
 *   zig cc -target x86_64-macos -O1 userland/tests-pty/main.c -o PtyTest
 * Sem argumentos: o pai abre o par (posix_openpt/grantpt/unlockpt/TIOCPTYGNAME/open do escravo/tcsetattr/TIOCSWINSZ),
 * lança o próprio binário como "spawn-helper" com posix_spawn(POSIX_SPAWN_SETSID | CLOEXEC_DEFAULT) e dup2 do escravo
 * para 0/1/2, e conversa pelo mestre não-bloqueante com poll + read, como o libuv:
 *   1. "--filho": sessão, terminal de controle, grupo em frente, tamanho, linha canônica com eco, SIGWINCH, ^C;
 *   2. o bash (argumento 1, padrão /usr/local/bin/bash) interativo: prompt, eco, aritmética, ls, ^C num sleep, exit.
 * Imprime "ptytest: N/N ok" (com os passos que falharam antes).
 */
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

extern char **environ;
static int ok, total;
static void check(int c, const char *what) {
    total++;
    if (c) ok++;
    printf("ptytest: %s %s\n", c ? "ok   " : "FALHA", what);
    fflush(stdout);
}
static long now_ms(void) { struct timeval tv; gettimeofday(&tv, 0); return tv.tv_sec * 1000L + tv.tv_usec / 1000; }

/* ------------------------------------------------------------ o que roda dentro do terminal */
static volatile sig_atomic_t got_winch;
static void on_winch(int s) { (void)s; got_winch = 1; }

static int filho(void) {
    char tty[64] = "";
    struct winsize ws = {0};
    ttyname_r(0, tty, sizeof tty);
    ioctl(0, TIOCGWINSZ, &ws);
    printf("filho: tty=%s sid=%s pgrp=%s tamanho=%dx%d\n", tty, getsid(0) == getpid() ? "lider" : "outro",
           tcgetpgrp(0) == getpgrp() ? "em-frente" : "atras", ws.ws_row, ws.ws_col);
    printf("filho: isatty=%d%d%d\n", isatty(0), isatty(1), isatty(2));
    fflush(stdout);
    char line[256];
    if (!fgets(line, sizeof line, stdin)) return 3;
    line[strcspn(line, "\n")] = 0;
    printf("filho: linha=[%s]\n", line);
    signal(SIGWINCH, on_winch);
    printf("filho: pronto-winch\n");
    fflush(stdout);
    long t0 = now_ms();
    while (!got_winch && now_ms() - t0 < 15000) {
        struct timeval tv = {1, 0};
        select(0, 0, 0, 0, &tv);                     /* EINTR quando o SIGWINCH chega */
    }
    ioctl(0, TIOCGWINSZ, &ws);
    printf("filho: winch=%d tamanho=%dx%d\n", (int)got_winch, ws.ws_row, ws.ws_col);
    printf("filho: pronto-int\n");
    fflush(stdout);
    char c;
    read(0, &c, 1);                                  /* o ^C (SIGINT, ação padrão) encerra aqui */
    printf("filho: read voltou\n");
    return 4;
}

/* spawn-helper do node-pty: adota o terminal, muda de pasta e executa */
static int helper(int argc, char **argv) {
    if (argc < 4) return 1;
    char *slave = ttyname(0);
    if (slave) close(open(slave, O_RDWR));
    if (argv[2][0] && chdir(argv[2]) == -1) _exit(1);
    execvp(argv[3], &argv[3]);
    fprintf(stderr, "helper: execvp %s: %s\n", argv[3], strerror(errno));
    return 1;
}

/* ------------------------------------------------------------ o pai (o pty host) */
static char acc[65536];
static size_t accn;
static int masterfd;
/* lê o mestre até aparecer `want` (procura a partir de `from`) ou o prazo vencer; 1 = achou */
static int wait_for(const char *want, size_t from, int ms) {
    long t0 = now_ms();
    for (;;) {
        acc[accn] = 0;
        if (from <= accn && strstr(acc + from, want)) return 1;
        long left = ms - (now_ms() - t0);
        if (left <= 0) return 0;
        struct pollfd p = {masterfd, POLLIN, 0};
        int r = poll(&p, 1, (int)(left > 200 ? 200 : left));
        if (r < 0 && errno != EINTR) return 0;
        if (r <= 0) continue;
        ssize_t n = read(masterfd, acc + accn, sizeof acc - 1 - accn);
        if (n > 0) accn += (size_t)n;
        else if (n == 0) { acc[accn] = 0; return strstr(acc + from, want) != 0; }   /* fim de arquivo */
        else if (errno != EAGAIN) return 0;
    }
}
static int read_eof(int ms) {
    long t0 = now_ms();
    while (now_ms() - t0 < ms) {
        struct pollfd p = {masterfd, POLLIN, 0};
        if (poll(&p, 1, 200) <= 0) continue;
        ssize_t n = read(masterfd, acc + accn, sizeof acc - 1 - accn);
        if (n == 0) return 1;
        if (n > 0) accn += (size_t)n;
        else if (errno != EAGAIN) return errno == EIO;   /* Linux dá EIO; o macOS e o mOS, 0 */
    }
    return 0;
}
static void type(const char *s) {
    size_t n = strlen(s), off = 0;
    while (off < n) {
        ssize_t w = write(masterfd, s + off, n - off);
        if (w > 0) off += (size_t)w;
        else if (errno == EAGAIN) usleep(2000);
        else return;
    }
}
static void dump_tail(const char *why) {
    size_t from = accn > 600 ? accn - 600 : 0;
    printf("ptytest: (%s) últimos bytes do mestre:\n", why);
    for (size_t i = from; i < accn; i++) {
        unsigned char c = (unsigned char)acc[i];
        if (c == '\n') printf("\\n\n");
        else if (c == '\r') printf("\\r");
        else if (c < 32 || c == 127) printf("\\x%02x", c);
        else putchar(c);
    }
    printf("\n");
    fflush(stdout);
}

/* abre um par como o pty.cc do node-pty (macOS) e lança helper + programa com o escravo em 0/1/2 */
static pid_t spawn_in_pty(char *self, char *const prog[], char *const env[], int rows, int cols, char *name, size_t ncap) {
    int m = posix_openpt(O_RDWR);
    if (m < 0) { printf("ptytest: posix_openpt: %s\n", strerror(errno)); return -1; }
    if (grantpt(m) < 0 || unlockpt(m) < 0) { printf("ptytest: grantpt/unlockpt: %s\n", strerror(errno)); return -1; }
    char sname[128] = "";
    if (ioctl(m, TIOCPTYGNAME, sname) < 0) { printf("ptytest: TIOCPTYGNAME: %s\n", strerror(errno)); return -1; }
    snprintf(name, ncap, "%s", sname);
    int s = open(sname, O_RDWR | O_NOCTTY);
    if (s < 0) { printf("ptytest: open %s: %s\n", sname, strerror(errno)); return -1; }
    struct termios t;
    memset(&t, 0, sizeof t);
    t.c_iflag = ICRNL | IXON | IXANY | IMAXBEL | BRKINT | IUTF8;
    t.c_oflag = OPOST | ONLCR;
    t.c_cflag = CREAD | CS8 | HUPCL;
    t.c_lflag = ICANON | ISIG | IEXTEN | ECHO | ECHOE | ECHOK | ECHOKE | ECHOCTL;
    t.c_cc[VEOF] = 4; t.c_cc[VEOL] = 0xff; t.c_cc[VEOL2] = 0xff; t.c_cc[VERASE] = 0x7f; t.c_cc[VWERASE] = 23;
    t.c_cc[VKILL] = 21; t.c_cc[VREPRINT] = 18; t.c_cc[VINTR] = 3; t.c_cc[VQUIT] = 0x1c; t.c_cc[VSUSP] = 26;
    t.c_cc[VSTART] = 17; t.c_cc[VSTOP] = 19; t.c_cc[VLNEXT] = 22; t.c_cc[VDISCARD] = 15; t.c_cc[VMIN] = 1;
    t.c_cc[VTIME] = 0; t.c_cc[VDSUSP] = 25; t.c_cc[VSTATUS] = 20;
    cfsetispeed(&t, B38400);
    cfsetospeed(&t, B38400);
    if (tcsetattr(s, TCSANOW, &t) < 0) { printf("ptytest: tcsetattr: %s\n", strerror(errno)); return -1; }
    struct winsize ws = {(unsigned short)rows, (unsigned short)cols, 0, 0};
    if (ioctl(s, TIOCSWINSZ, &ws) < 0) { printf("ptytest: TIOCSWINSZ: %s\n", strerror(errno)); return -1; }
    posix_spawn_file_actions_t acts;
    posix_spawn_file_actions_init(&acts);
    posix_spawn_file_actions_adddup2(&acts, s, 0);
    posix_spawn_file_actions_adddup2(&acts, s, 1);
    posix_spawn_file_actions_adddup2(&acts, s, 2);
    posix_spawn_file_actions_addclose(&acts, s);
    posix_spawn_file_actions_addclose(&acts, m);
    posix_spawnattr_t at;
    posix_spawnattr_init(&at);
    posix_spawnattr_setflags(&at, POSIX_SPAWN_CLOEXEC_DEFAULT | POSIX_SPAWN_SETSIGDEF | POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSID);
    sigset_t ss;
    sigfillset(&ss);
    posix_spawnattr_setsigdefault(&at, &ss);
    sigemptyset(&ss);
    posix_spawnattr_setsigmask(&at, &ss);
    char *argv[16];
    int n = 0;
    argv[n++] = self;
    argv[n++] = "--helper";
    argv[n++] = "/";
    for (int i = 0; prog[i] && n < 15; i++) argv[n++] = prog[i];
    argv[n] = 0;
    pid_t pid = -1;
    int e = posix_spawn(&pid, self, &acts, &at, argv, env);
    close(s);
    if (e) { printf("ptytest: posix_spawn: %s\n", strerror(e)); return -1; }
    int fl = fcntl(m, F_GETFL);
    fcntl(m, F_SETFL, fl | O_NONBLOCK);
    masterfd = m;
    accn = 0;
    return pid;
}

int main(int argc, char **argv) {
    if (argc > 1 && !strcmp(argv[1], "--helper")) return helper(argc, argv);
    if (argc > 1 && !strcmp(argv[1], "--filho")) return filho();
    setvbuf(stdout, 0, _IOLBF, 0);
    char self[1024];
    uint32_t sz = sizeof self;
    extern int _NSGetExecutablePath(char *, uint32_t *);
    if (_NSGetExecutablePath(self, &sz) != 0) snprintf(self, sizeof self, "%s", argv[0]);
    const char *bash = argc > 1 ? argv[1] : "/usr/local/bin/bash";

    /* 0. o básico do par, sem filho */
    {
        int m = posix_openpt(O_RDWR | O_NOCTTY);
        check(m >= 0, "posix_openpt");
        check(grantpt(m) == 0 && unlockpt(m) == 0, "grantpt + unlockpt");
        char *pn = ptsname(m);
        check(pn && !strncmp(pn, "/dev/ttys", 9), "ptsname = /dev/ttysNNN");
        int s = open(pn ? pn : "/dev/ttys000", O_RDWR | O_NOCTTY);
        check(s >= 0 && isatty(s) && isatty(m), "isatty do mestre e do escravo");
        int pfd[2];
        pipe(pfd);
        check(!isatty(pfd[0]) && errno == ENOTTY, "pipe não é terminal (ENOTTY)");
        close(pfd[0]); close(pfd[1]);
        char tn[64] = "";
        check(ttyname_r(s, tn, sizeof tn) == 0 && pn && !strcmp(tn, pn), "ttyname_r do escravo");
        char dummy[128];
        check(ioctl(s, TIOCPTYGNAME, dummy) < 0 && ioctl(m, TIOCPTYGNAME, dummy) == 0, "TIOCPTYGNAME só no mestre (libuv)");
        struct stat st;
        check(fstat(s, &st) == 0 && S_ISCHR(st.st_mode), "fstat: dispositivo de caracteres");
        check(stat(pn ? pn : "/dev/ttys000", &st) == 0 && S_ISCHR(st.st_mode), "stat do caminho do escravo");
        fcntl(m, F_SETFL, O_NONBLOCK);
        char b[64];
        check(read(m, b, sizeof b) < 0 && errno == EAGAIN, "mestre vazio e O_NONBLOCK: EAGAIN");
        write(m, "ab\x7f" "c\r", 5);
        ssize_t n = read(s, b, sizeof b);
        check(n == 3 && !memcmp(b, "ac\n", 3), "linha canônica editada (DEL) chega ao escravo");
        n = read(m, b, sizeof b);
        check(n == 9 && !memcmp(b, "ab\b \bc\r\n", 9), "eco com ECHOE e ONLCR no mestre");
        struct pollfd p = {m, POLLIN, 0};
        check(poll(&p, 1, 0) == 0, "poll: mestre sem dados");
        write(s, "x\n", 2);
        check(poll(&p, 1, 100) == 1 && (p.revents & POLLIN), "poll: saída do escravo deixa o mestre legível");
        n = read(m, b, sizeof b);
        check(n == 3 && !memcmp(b, "x\r\n", 3), "ONLCR na saída");
        int avail = -1;
        write(m, "linha\r", 6);
        check(ioctl(s, FIONREAD, &avail) == 0 && avail == 6, "FIONREAD no escravo");
        tcflush(s, TCIFLUSH);
        check(ioctl(s, FIONREAD, &avail) == 0 && avail == 0, "tcflush(TCIFLUSH)");
        while (read(m, b, sizeof b) > 0) {}
        close(s);
        check(read(m, b, sizeof b) == 0, "sem ninguém no escravo, o mestre lê fim de arquivo");
        close(m);
    }

    char name[128];
    char *env[] = {"TERM=xterm-256color", "HOME=/Users/Visitante", "PATH=/usr/local/bin:/Sistema/Programas:/usr/bin:/bin",
                   "PS1=PTY$ ", "LANG=pt_BR.UTF-8", "BASH_SILENCE_DEPRECATION_WARNING=1", 0};

    /* 1. o próprio teste como filho: sessão, terminal de controle, eco, SIGWINCH, ^C */
    {
        char *prog[] = {self, "--filho", 0};
        pid_t pid = spawn_in_pty(self, prog, env, 30, 100, name, sizeof name);
        check(pid > 0, "posix_spawn(SETSID) do helper");
        if (pid > 0) {
            check(wait_for("filho: isatty=111", 0, 20000), "filho: 0/1/2 são o terminal");
            check(strstr(acc, "sid=lider pgrp=em-frente tamanho=30x100") != 0, "filho: líder da sessão, grupo em frente, 30x100");
            check(strstr(acc, name) != 0, "filho: ttyname(0) é o escravo do par");
            size_t mark = accn;
            type("ola mundo\r");
            check(wait_for("filho: linha=[ola mundo]", mark, 10000), "linha digitada no mestre chega ao fgets do filho");
            check(strstr(acc + mark, "ola mundo\r\n") != 0, "eco da linha (canônico)");
            check(wait_for("filho: pronto-winch", mark, 10000), "filho com tratador de SIGWINCH");
            struct winsize ws = {40, 120, 0, 0};
            mark = accn;
            check(ioctl(masterfd, TIOCSWINSZ, &ws) == 0, "TIOCSWINSZ no mestre (pty.resize)");
            check(wait_for("filho: winch=1 tamanho=40x120", mark, 10000), "SIGWINCH chega e o filho lê 40x120");
            pid_t pg = tcgetpgrp(masterfd);
            check(pg == pid, "tcgetpgrp(mestre) = o filho (título do node-pty)");
            check(wait_for("filho: pronto-int", mark, 10000), "filho esperando no read");
            mark = accn;
            type("\x03");
            int stv = 0;
            pid_t w = -1;
            long t0 = now_ms();
            while (now_ms() - t0 < 10000 && (w = waitpid(pid, &stv, WNOHANG)) == 0) usleep(20000);
            check(w == pid && WIFSIGNALED(stv) && WTERMSIG(stv) == SIGINT, "^C encerra o filho com SIGINT");
            wait_for("^C", mark, 2000);
            check(strstr(acc + mark, "^C") != 0, "eco do ^C (ECHOCTL)");
            check(read_eof(5000), "filho morto: o mestre lê fim de arquivo");
            if (ok != total) dump_tail("filho");
            close(masterfd);
        }
    }

    /* 2. o bash interativo, como o terminal do VS Code */
    if (access(bash, X_OK) == 0) {
        char *prog[] = {(char *)bash, "--noprofile", "--norc", "-i", 0};
        pid_t pid = spawn_in_pty(self, prog, env, 24, 80, name, sizeof name);
        check(pid > 0, "bash: posix_spawn");
        if (pid > 0) {
            check(wait_for("PTY$ ", 0, 60000), "bash: prompt");
            check(!strstr(acc, "no job control") && !strstr(acc, "cannot set terminal process group"), "bash: job control sem avisos");
            size_t mark = accn;
            type("echo ola-$((40+2))\r");
            check(wait_for("ola-42", mark, 20000), "bash: eco e aritmética");
            check(wait_for("PTY$ ", mark + 1, 10000), "bash: prompt de novo");
            mark = accn;
            type("cd / && ls\r");
            check(wait_for("Sistema", mark, 30000) && wait_for("PTY$ ", mark + 1, 30000), "bash: ls lista /");
            mark = accn;
            type("tty; stty size\r");
            wait_for("PTY$ ", mark + 1, 20000);
            check(strstr(acc + mark, name) != 0, "bash: tty mostra o escravo");
            check(strstr(acc + mark, "24 80") != 0 || !strstr(acc + mark, "stty"), "bash: stty size = 24 80");
            struct winsize ws = {33, 111, 0, 0};
            ioctl(masterfd, TIOCSWINSZ, &ws);
            usleep(300000);
            mark = accn;
            type("echo cols=$COLUMNS lines=$LINES\r");
            check(wait_for("cols=111 lines=33", mark, 20000), "bash: SIGWINCH atualiza COLUMNS/LINES");
            wait_for("PTY$ ", mark + 1, 10000);
            mark = accn;
            type("sleep 30\r");
            usleep(1500000);
            type("\x03");
            check(wait_for("PTY$ ", mark + 1, 20000), "bash: ^C interrompe o sleep e volta ao prompt");
            mark = accn;
            type("exit\r");
            int stv = 0;
            pid_t w = -1;
            long t0 = now_ms();
            while (now_ms() - t0 < 15000 && (w = waitpid(pid, &stv, WNOHANG)) == 0) usleep(20000);
            check(w == pid && WIFEXITED(stv) && WEXITSTATUS(stv) == 0, "bash: exit termina com código 0");
            check(read_eof(5000), "bash: o mestre lê fim de arquivo");
            if (ok != total) dump_tail("bash");
            close(masterfd);
        }
    } else printf("ptytest: sem %s: parte do bash pulada\n", bash);

    printf("ptytest: %d/%d ok\n", ok, total);
    fflush(stdout);
    return ok == total ? 0 : 1;
}
