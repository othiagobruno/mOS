/*
 * mOS — PtyRun: roda um programa num pseudoterminal seguindo um roteiro e imprime a tela (um emulador VT100/xterm
 * mínimo) no stdout, que vai para o log serial. Serve para testar programas de tela cheia no mOS sem teclado nem
 * monitor: o modo interativo do Claude Code, vim, htop. Compila igual no macOS:
 *   zig cc -target x86_64-macos -O1 userland/tests-pty/ptyrun.c -o PtyRun
 * Uso: PtyRun <roteiro>   (montado por tools/build-ptyrun.ps1)
 * Roteiro, uma instrução por linha (# comenta):
 *   tamanho L C            linhas e colunas (antes de "comando"; padrão 24 80)
 *   ambiente NOME=valor    variável do filho (repetível; substitui a de mesmo nome)
 *   comando prog [args]    lança o programa no escravo, como o node-pty (palavras com espaço vão entre aspas)
 *   esperar TEXTO [ms]     lê até TEXTO estar na tela ou ter sido escrito desde a última tecla (padrão 120000)
 *   quieto ms [máx]        lê até passar ms sem saída (padrão máx 600000)
 *   pausa ms               só lê, por ms
 *   se TEXTO               a instrução seguinte só roda se TEXTO estiver na tela
 *   tecla SEQ              escreve no mestre: \r \n \t \e \xHH \\ e ^X (controle); setas \e[A..D
 *   redimensionar L C      TIOCSWINSZ no mestre (o filho recebe SIGWINCH)
 *   tela [rótulo]          imprime a tela
 *   eco                    daqui em diante, cada linha que o programa escreve sai também como "saida| ..."
 *   fim [ms]               espera o filho terminar (padrão 30000; senão SIGKILL) e imprime o código
 * Responde como um xterm às perguntas que os programas fazem ao terminal (DA, DSR da posição do cursor, XTVERSION,
 * cores de frente/fundo por OSC 10/11): sem resposta, alguns ficam esperando.
 * Saída: "ptyrun: ..." para eventos e "tela| ..." para as linhas da tela. Termina com "ptyrun: fim (N falha(s))".
 */
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

extern int _NSGetExecutablePath(char *, uint32_t *);

static long t_start;
static long now_ms(void) { struct timeval tv; gettimeofday(&tv, 0); return tv.tv_sec * 1000L + tv.tv_usec / 1000; }
static double secs(void) { return (double)(now_ms() - t_start) / 1000.0; }
static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

/* ------------------------------------------------------------ a tela */
#define MAXR 80
#define MAXC 256
typedef struct { char s[5]; } cell_t;            /* um caractere UTF-8 por célula (largura 1) */
static cell_t scr[MAXR][MAXC], main_save[MAXR][MAXC];
static int R = 24, C = 80, cy, cx, wrapnext, sy, sx, top, bot, alt, ckm;
static int masterfd = -1;
static unsigned long total_in;
static char plain[1 << 20];                       /* tudo o que foi escrito na tela, sem as sequências */
static size_t plainn, mark, eco_from;
static int eco;                                   /* "eco": as linhas do programa também vão para o stdout */
static void eco_line(void) {
    if (eco && plainn > eco_from) printf("saida| %.*s\n", (int)(plainn - eco_from), plain + eco_from);
    eco_from = plainn;
    if (eco && plainn > sizeof plain - 65536) plainn = mark = eco_from = 0;   /* muita saída: recomeça o histórico */
}

static void reply(const char *s) { if (masterfd >= 0) write(masterfd, s, strlen(s)); }
static int blank(const cell_t *c) { return !c->s[0] || (c->s[0] == ' ' && !c->s[1]); }
static void clear_cells(int r, int c0, int c1) {
    for (int c = c0 < 0 ? 0 : c0; c < c1 && c < C; c++) { scr[r][c].s[0] = ' '; scr[r][c].s[1] = 0; }
}
static void clear_rows(int r0, int r1) { for (int r = r0 < 0 ? 0 : r0; r < r1 && r < R; r++) clear_cells(r, 0, C); }
static void scroll_up(int t, int b, int n) {       /* linhas t..b sobem n */
    if (n > b - t + 1) n = b - t + 1;
    for (int r = t; r + n <= b; r++) memcpy(scr[r], scr[r + n], sizeof scr[r]);
    clear_rows(b - n + 1, b + 1);
}
static void scroll_down(int t, int b, int n) {
    if (n > b - t + 1) n = b - t + 1;
    for (int r = b; r - n >= t; r--) memcpy(scr[r], scr[r - n], sizeof scr[r]);
    clear_rows(t, t + n);
}
static void linefeed(void) { if (cy == bot) scroll_up(top, bot, 1); else if (cy < R - 1) cy++; }
static void put(const char *u) {
    if (wrapnext) { cx = 0; linefeed(); wrapnext = 0; }
    snprintf(scr[cy][cx].s, sizeof scr[cy][cx].s, "%s", u);
    if (cx >= C - 1) wrapnext = 1; else cx++;
    size_t l = strlen(u);
    if (plainn + l < sizeof plain - 1) { memcpy(plain + plainn, u, l); plainn += l; plain[plainn] = 0; }
}
static void screen_reset(int rows, int cols) {
    R = clampi(rows, 1, MAXR);
    C = clampi(cols, 1, MAXC);
    clear_rows(0, R);
    cy = cx = wrapnext = 0;
    top = 0;
    bot = R - 1;
}
static void screen_resize(int rows, int cols) {
    int nr = clampi(rows, 1, MAXR), nc = clampi(cols, 1, MAXC);
    for (int r = 0; r < MAXR; r++)
        for (int c = r < R ? C : 0; c < MAXC; c++) { scr[r][c].s[0] = ' '; scr[r][c].s[1] = 0; }
    R = nr;
    C = nc;
    top = 0;
    bot = R - 1;
    cy = clampi(cy, 0, R - 1);
    cx = clampi(cx, 0, C - 1);
    wrapnext = 0;
}
/* a tela como texto, uma linha por fileira (para o "esperar" e o "se") */
static char screen_txt[MAXR * (MAXC * 4 + 1) + 1];
static const char *screen_text(void) {
    size_t o = 0;
    for (int r = 0; r < R; r++) {
        for (int c = 0; c < C; c++) {
            const char *s = blank(&scr[r][c]) ? " " : scr[r][c].s;
            size_t l = strlen(s);
            memcpy(screen_txt + o, s, l);
            o += l;
        }
        screen_txt[o++] = '\n';
    }
    screen_txt[o] = 0;
    return screen_txt;
}
static void show(const char *label) {
    printf("ptyrun: [%.1f s] tela \"%s\" (%dx%d, cursor %d,%d, %lu bytes recebidos)\n", secs(), label, R, C, cy + 1, cx + 1,
           total_in);
    int last = -1;
    for (int r = 0; r < R; r++)
        for (int c = 0; c < C; c++) if (!blank(&scr[r][c])) last = r;
    for (int r = 0; r <= last; r++) {
        char line[MAXC * 4 + 1];
        size_t o = 0;
        int endc = 0;
        for (int c = 0; c < C; c++) if (!blank(&scr[r][c])) endc = c + 1;
        for (int c = 0; c < endc; c++) {
            const char *s = blank(&scr[r][c]) ? " " : scr[r][c].s;
            size_t l = strlen(s);
            memcpy(line + o, s, l);
            o += l;
        }
        line[o] = 0;
        printf("tela| %s\n", line);
    }
    fflush(stdout);
}

/* ------------------------------------------------------------ o intérprete das sequências */
static char seq[1024];
static int seqn, st, str_kind;                    /* st: 0 normal, 1 ESC, 2 CSI, 3 OSC, 4 DCS/APC/PM/SOS, 5 charset, 6 ESC em string */
static unsigned char u8[5];
static int u8n, u8need;
static char last_title[256];
static int titles;

static void csi(void) {
    char final = seq[seqn - 1], priv = 0, inter = 0;
    int i = 0, p[16] = {0}, np = 1;
    if (seqn > 1 && strchr("?><=", seq[0])) { priv = seq[0]; i = 1; }
    for (; i < seqn - 1; i++) {
        char ch = seq[i];
        if (ch >= '0' && ch <= '9') p[np - 1] = p[np - 1] * 10 + (ch - '0');
        else if (ch == ';' || ch == ':') { if (np < 16) np++; }
        else if (ch >= 0x20 && ch <= 0x2f) inter = ch;
    }
    int n = p[0] ? p[0] : 1;
    if (inter) return;                            /* DECSCUSR e afins: nada na tela */
    if (priv == '?' && (final == 'h' || final == 'l')) {
        for (int k = 0; k < np; k++) {
            int m = p[k];
            if (m == 1) ckm = final == 'h';      /* setas no modo de aplicação: ESC O A..D */
            else if (m == 1049 || m == 1047 || m == 47) {
                if (final == 'h' && !alt) {
                    if (m == 1049) { sy = cy; sx = cx; }
                    memcpy(main_save, scr, sizeof scr);
                    alt = 1;
                    clear_rows(0, R);
                } else if (final == 'l' && alt) {
                    memcpy(scr, main_save, sizeof scr);
                    alt = 0;
                    if (m == 1049) { cy = sy; cx = sx; }
                }
            }
        }
        return;
    }
    if (priv == '>' && final == 'q') { reply("\x1bP>|PtyRun(1.0)\x1b\\"); return; }
    if (priv == '>' && final == 'c') { reply("\x1b[>0;10;0c"); return; }
    if (priv) return;                             /* ?u (teclado do kitty) e outros: sem suporte, sem resposta */
    switch (final) {
    case 'm': return;                             /* cores: a tela é só texto */
    case 'A': cy = clampi(cy - n, 0, R - 1); break;
    case 'B': case 'e': cy = clampi(cy + n, 0, R - 1); break;
    case 'C': case 'a': cx = clampi(cx + n, 0, C - 1); break;
    case 'D': cx = clampi(cx - n, 0, C - 1); break;
    case 'E': cy = clampi(cy + n, 0, R - 1); cx = 0; break;
    case 'F': cy = clampi(cy - n, 0, R - 1); cx = 0; break;
    case 'G': case '`': cx = clampi(n - 1, 0, C - 1); break;
    case 'd': cy = clampi(n - 1, 0, R - 1); break;
    case 'H': case 'f':
        cy = clampi((p[0] ? p[0] : 1) - 1, 0, R - 1);
        cx = clampi((np > 1 && p[1] ? p[1] : 1) - 1, 0, C - 1);
        break;
    case 'J':
        if (p[0] == 0) { clear_cells(cy, cx, C); clear_rows(cy + 1, R); }
        else if (p[0] == 1) { clear_rows(0, cy); clear_cells(cy, 0, cx + 1); }
        else clear_rows(0, R);
        break;
    case 'K':
        if (p[0] == 0) clear_cells(cy, cx, C);
        else if (p[0] == 1) clear_cells(cy, 0, cx + 1);
        else clear_cells(cy, 0, C);
        break;
    case 'X': clear_cells(cy, cx, cx + n); break;
    case 'P':
        for (int c = cx; c < C; c++) {
            if (c + n < C) scr[cy][c] = scr[cy][c + n];
            else { scr[cy][c].s[0] = ' '; scr[cy][c].s[1] = 0; }
        }
        break;
    case '@':
        for (int c = C - 1; c >= cx; c--) {
            if (c - n >= cx) scr[cy][c] = scr[cy][c - n];
            else { scr[cy][c].s[0] = ' '; scr[cy][c].s[1] = 0; }
        }
        break;
    case 'L': if (cy >= top && cy <= bot) scroll_down(cy, bot, n); break;
    case 'M': if (cy >= top && cy <= bot) scroll_up(cy, bot, n); break;
    case 'S': scroll_up(top, bot, n); break;
    case 'T': scroll_down(top, bot, n); break;
    case 'r':
        top = clampi((p[0] ? p[0] : 1) - 1, 0, R - 1);
        bot = clampi((np > 1 && p[1] ? p[1] : R) - 1, 0, R - 1);
        if (top >= bot) { top = 0; bot = R - 1; }
        cy = cx = 0;
        break;
    case 's': sy = cy; sx = cx; break;
    case 'u': cy = sy; cx = sx; break;
    case 'n':
        if (p[0] == 6) { char b[32]; snprintf(b, sizeof b, "\x1b[%d;%dR", cy + 1, cx + 1); reply(b); }
        else if (p[0] == 5) reply("\x1b[0n");
        break;
    case 'c': reply("\x1b[?62;22c"); break;
    case 't': if (p[0] == 18) { char b[32]; snprintf(b, sizeof b, "\x1b[8;%d;%dt", R, C); reply(b); } break;
    default: break;
    }
    wrapnext = 0;
}
static void osc(void) {
    seq[seqn] = 0;
    if ((seq[0] == '0' || seq[0] == '2') && seq[1] == ';') {
        if (strcmp(last_title, seq + 2) && titles < 30) {
            titles++;
            snprintf(last_title, sizeof last_title, "%s", seq + 2);
            printf("ptyrun: [%.1f s] título: %s\n", secs(), last_title);
        }
    } else if (!strcmp(seq, "11;?")) reply("\x1b]11;rgb:1e1e/1e1e/1e1e\x1b\\");   /* fundo escuro */
    else if (!strcmp(seq, "10;?")) reply("\x1b]10;rgb:d4d4/d4d4/d4d4\x1b\\");
}
static void feed(const unsigned char *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        unsigned char ch = b[i];
        total_in++;
        switch (st) {
        case 0:
            if (u8need) {
                if ((ch & 0xC0) == 0x80) {
                    u8[u8n++] = ch;
                    if (u8n == u8need) { u8[u8n] = 0; put((const char *)u8); u8need = 0; }
                    continue;
                }
                u8need = 0;                       /* sequência quebrada: segue com este byte */
            }
            if (ch == 0x1b) st = 1;
            else if (ch == '\r') { cx = 0; wrapnext = 0; }
            else if (ch == '\n' || ch == 0x0b || ch == 0x0c) {
                linefeed();
                wrapnext = 0;
                eco_line();
                if (plainn + 1 < sizeof plain) { plain[plainn++] = '\n'; plain[plainn] = 0; }
                eco_from = plainn;
            }
            else if (ch == '\b') { if (cx > 0) cx--; wrapnext = 0; }
            else if (ch == '\t') cx = clampi((cx / 8 + 1) * 8, 0, C - 1);
            else if (ch < 0x20 || ch == 0x7f) { }
            else if (ch < 0x80) { char s[2] = {(char)ch, 0}; put(s); }
            else if ((ch & 0xE0) == 0xC0) { u8[0] = ch; u8n = 1; u8need = 2; }
            else if ((ch & 0xF0) == 0xE0) { u8[0] = ch; u8n = 1; u8need = 3; }
            else if ((ch & 0xF8) == 0xF0) { u8[0] = ch; u8n = 1; u8need = 4; }
            break;
        case 1:
            st = 0;
            if (ch == '[') { st = 2; seqn = 0; }
            else if (ch == ']') { st = 3; seqn = 0; }
            else if (ch == 'P' || ch == 'X' || ch == '^' || ch == '_') { st = 4; seqn = 0; }
            else if (strchr("()*+#%", ch)) st = 5;
            else if (ch == '7') { sy = cy; sx = cx; }
            else if (ch == '8') { cy = sy; cx = sx; wrapnext = 0; }
            else if (ch == 'M') { if (cy == top) scroll_down(top, bot, 1); else if (cy > 0) cy--; }
            else if (ch == 'D') linefeed();
            else if (ch == 'E') { cx = 0; linefeed(); }
            else if (ch == 'c') screen_reset(R, C);
            break;
        case 2:
            if (ch == 0x1b) { st = 1; break; }   /* sequência abortada */
            if (seqn < (int)sizeof seq - 1) seq[seqn++] = (char)ch;
            if (ch >= 0x40 && ch <= 0x7e) { csi(); st = 0; }
            break;
        case 3: case 4:
            if (ch == 0x07 && st == 3) { osc(); st = 0; }
            else if (ch == 0x1b) { str_kind = st; st = 6; }
            else if (seqn < (int)sizeof seq - 1) seq[seqn++] = (char)ch;
            break;
        case 6:
            if (str_kind == 3) osc();
            st = 0;
            if (ch != '\\') { st = 1; i--; total_in--; }   /* ESC sem '\': um ESC novo, reprocessa o byte */
            break;
        case 5:
            st = 0;                               /* designação de conjunto: um byte */
            break;
        }
    }
}

/* ------------------------------------------------------------ o filho */
static pid_t child = -1;
static int child_done, child_status, master_eof, failures;
static long last_out;
static char *envv[40];
static int envc;
static int rows = 24, cols = 80;

static void env_set(const char *kv) {
    const char *eq = strchr(kv, '=');
    size_t kl = eq ? (size_t)(eq - kv) : strlen(kv);
    for (int i = 0; i < envc; i++)
        if (!strncmp(envv[i], kv, kl) && envv[i][kl] == '=') { envv[i] = strdup(kv); return; }
    if (envc < (int)(sizeof envv / sizeof *envv) - 1) { envv[envc++] = strdup(kv); envv[envc] = 0; }
}

static void pump(int ms) {
    if (!master_eof && masterfd >= 0) {
        struct pollfd p = {masterfd, POLLIN, 0};
        int r = poll(&p, 1, ms);
        if (r > 0) {
            unsigned char b[16384];
            ssize_t n = read(masterfd, b, sizeof b);
            if (n > 0) { feed(b, (size_t)n); last_out = now_ms(); }
            else if (n == 0 || (errno != EAGAIN && errno != EINTR)) {
                master_eof = 1;
                printf("ptyrun: [%.1f s] o mestre leu fim de arquivo\n", secs());
            }
        }
    } else usleep((useconds_t)ms * 1000);
    if (child > 0 && !child_done) {
        int s = 0;
        if (waitpid(child, &s, WNOHANG) == child) {
            child_done = 1;
            child_status = s;
            if (WIFEXITED(s)) printf("ptyrun: [%.1f s] o filho terminou com código %d\n", secs(), WEXITSTATUS(s));
            else if (WIFSIGNALED(s)) printf("ptyrun: [%.1f s] o filho terminou pelo sinal %d\n", secs(), WTERMSIG(s));
        }
    }
}
static int gone(void) { return child_done && (master_eof || masterfd < 0); }

/* o spawn-helper do node-pty: adota o escravo como terminal de controle e executa o programa */
static int helper(int argc, char **argv) {
    if (argc < 3) return 1;
    char *slave = ttyname(0);
    if (slave) close(open(slave, O_RDWR));
    execvp(argv[2], &argv[2]);
    fprintf(stderr, "ptyrun: execvp %s: %s\n", argv[2], strerror(errno));
    return 127;
}

static pid_t spawn_in_pty(char *self, char **prog) {
    int m = posix_openpt(O_RDWR);
    if (m < 0) { printf("ptyrun: posix_openpt: %s\n", strerror(errno)); return -1; }
    if (grantpt(m) < 0 || unlockpt(m) < 0) { printf("ptyrun: grantpt/unlockpt: %s\n", strerror(errno)); return -1; }
    char sname[128] = "";
    if (ioctl(m, TIOCPTYGNAME, sname) < 0) { printf("ptyrun: TIOCPTYGNAME: %s\n", strerror(errno)); return -1; }
    int s = open(sname, O_RDWR | O_NOCTTY);
    if (s < 0) { printf("ptyrun: open %s: %s\n", sname, strerror(errno)); return -1; }
    struct termios t;                             /* os valores do pty.cc do node-pty */
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
    if (tcsetattr(s, TCSANOW, &t) < 0) { printf("ptyrun: tcsetattr: %s\n", strerror(errno)); return -1; }
    struct winsize ws = {(unsigned short)rows, (unsigned short)cols, 0, 0};
    if (ioctl(s, TIOCSWINSZ, &ws) < 0) { printf("ptyrun: TIOCSWINSZ: %s\n", strerror(errno)); return -1; }
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
    char *argv[40];
    int n = 0;
    argv[n++] = self;
    argv[n++] = "--helper";
    for (int i = 0; prog[i] && n < 39; i++) argv[n++] = prog[i];
    argv[n] = 0;
    pid_t pid = -1;
    int e = posix_spawn(&pid, self, &acts, &at, argv, envv);
    close(s);
    if (e) { printf("ptyrun: posix_spawn: %s\n", strerror(e)); close(m); return -1; }
    fcntl(m, F_SETFL, fcntl(m, F_GETFL) | O_NONBLOCK);
    masterfd = m;
    printf("ptyrun: [%.1f s] %s rodando no %s (pid %d, %dx%d)\n", secs(), prog[0], sname, (int)pid, rows, cols);
    fflush(stdout);
    return pid;
}

/* ------------------------------------------------------------ o roteiro */
/* palavras separadas por espaço; "..." agrupa */
static int split(char *s, char **out, int max) {
    int n = 0;
    while (*s && n < max) {
        while (*s == ' ' || *s == '\t') s++;
        if (!*s) break;
        if (*s == '"') {
            out[n++] = ++s;
            while (*s && *s != '"') s++;
        } else {
            out[n++] = s;
            while (*s && *s != ' ' && *s != '\t') s++;
        }
        if (*s) *s++ = 0;
    }
    out[n] = 0;
    return n;
}
static size_t unescape(const char *in, char *out, size_t cap) {
    size_t o = 0;
    for (const char *s = in; *s && o + 1 < cap; s++) {
        if (*s == '^' && s[1] && s[1] != '^') { out[o++] = (char)(s[1] & 0x1f); s++; continue; }
        if (*s == '^' && s[1] == '^') { out[o++] = '^'; s++; continue; }
        if (*s != '\\' || !s[1]) { out[o++] = *s; continue; }
        s++;
        switch (*s) {
        case 'r': out[o++] = '\r'; break;
        case 'n': out[o++] = '\n'; break;
        case 't': out[o++] = '\t'; break;
        case 'e': out[o++] = 0x1b; break;
        case 'x': {
            unsigned v = 0;
            int k = 0;
            while (k < 2 && s[1] && strchr("0123456789abcdefABCDEF", s[1])) {
                char h = *++s;
                v = v * 16 + (unsigned)(h <= '9' ? h - '0' : (h | 0x20) - 'a' + 10);
                k++;
            }
            out[o++] = (char)v;
            break;
        }
        default: out[o++] = *s; break;
        }
    }
    out[o] = 0;
    return o;
}
static int screen_has(const char *txt) { return strstr(screen_text(), txt) != 0; }

int main(int argc, char **argv) {
    if (argc > 1 && !strcmp(argv[1], "--helper")) return helper(argc, argv);
    setvbuf(stdout, 0, _IOLBF, 0);
    t_start = now_ms();
    char self[1024];
    uint32_t sz = sizeof self;
    if (_NSGetExecutablePath(self, &sz) != 0) snprintf(self, sizeof self, "%s", argv[0]);
    signal(SIGPIPE, SIG_IGN);
    const char *path = argc > 1 ? argv[1] : "roteiro.txt";
    FILE *f = fopen(path, "r");
    if (!f) { printf("ptyrun: %s: %s\n", path, strerror(errno)); return 2; }
    static char script[65536];
    size_t len = fread(script, 1, sizeof script - 1, f);
    fclose(f);
    script[len] = 0;
    printf("ptyrun: roteiro %s (%zu bytes)\n", path, len);
    env_set("TERM=xterm-256color");
    env_set("HOME=/Users/Visitante");
    env_set("USER=visitante");
    env_set("PATH=/usr/local/bin:/Sistema/Programas:/usr/bin:/bin");
    env_set("LANG=en_US.UTF-8");
    int skip = 0;
    for (char *line = script; line && *line;) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = 0;
        char *next = nl ? nl + 1 : 0;
        size_t ll = strlen(line);
        while (ll && (line[ll - 1] == '\r' || line[ll - 1] == ' ')) line[--ll] = 0;
        while (*line == ' ' || *line == '\t') line++;
        if (!*line || *line == '#') { line = next; continue; }
        char raw[1024];
        snprintf(raw, sizeof raw, "%s", line);
        char *w[40];
        int nw = split(line, w, 39);
        const char *cmd = w[0];
        const char *rest = raw + strlen(cmd);
        while (*rest == ' ') rest++;
        if (skip) { printf("ptyrun: [%.1f s] pulado: %s\n", secs(), raw); skip = 0; line = next; continue; }
        if (!strcmp(cmd, "tamanho") && nw >= 3) {
            rows = clampi(atoi(w[1]), 1, MAXR);
            cols = clampi(atoi(w[2]), 1, MAXC);
        } else if (!strcmp(cmd, "ambiente") && nw >= 2) {
            env_set(rest);
        } else if (!strcmp(cmd, "comando") && nw >= 2) {
            screen_reset(rows, cols);
            child = spawn_in_pty(self, &w[1]);
            if (child < 0) { failures++; break; }
            last_out = now_ms();
        } else if (!strcmp(cmd, "esperar") && nw >= 2) {
            int ms = nw >= 3 ? atoi(w[2]) : 120000;
            long t0 = now_ms();
            int found = 0;
            while (!(found = screen_has(w[1]) || strstr(plain + mark, w[1]) != 0) && now_ms() - t0 < ms && !gone()) pump(200);
            printf("ptyrun: [%.1f s] esperar \"%s\": %s\n", secs(), w[1], found ? "apareceu" : "não apareceu");
            if (!found) failures++;
        } else if (!strcmp(cmd, "quieto") && nw >= 2) {
            int ms = atoi(w[1]), max = nw >= 3 ? atoi(w[2]) : 600000;
            long t0 = now_ms();
            while (now_ms() - last_out < ms && now_ms() - t0 < max && !gone()) pump(100);
            printf("ptyrun: [%.1f s] quieto %d ms%s\n", secs(), ms, now_ms() - last_out >= ms ? "" : " (prazo esgotado)");
        } else if (!strcmp(cmd, "pausa") && nw >= 2) {
            long t0 = now_ms();
            while (now_ms() - t0 < atoi(w[1])) pump(100);
        } else if (!strcmp(cmd, "se") && nw >= 2) {
            skip = !screen_has(rest);
            printf("ptyrun: [%.1f s] se \"%s\": %s\n", secs(), rest, skip ? "não está na tela" : "está na tela");
        } else if (!strcmp(cmd, "tecla") && nw >= 2) {
            char k[256];
            size_t n = unescape(rest, k, sizeof k);
            if (ckm)                                /* modo de aplicação das setas */
                for (size_t i = 0; i + 2 < n; i++) if (k[i] == 0x1b && k[i + 1] == '[' && strchr("ABCD", k[i + 2])) k[i + 1] = 'O';
            mark = plainn;
            printf("ptyrun: [%.1f s] tecla %s\n", secs(), rest);
            if (masterfd >= 0 && write(masterfd, k, n) != (ssize_t)n) printf("ptyrun:   write: %s\n", strerror(errno));
        } else if (!strcmp(cmd, "redimensionar") && nw >= 3) {
            rows = clampi(atoi(w[1]), 1, MAXR);
            cols = clampi(atoi(w[2]), 1, MAXC);
            struct winsize ws = {(unsigned short)rows, (unsigned short)cols, 0, 0};
            int r = masterfd >= 0 ? ioctl(masterfd, TIOCSWINSZ, &ws) : -1;
            screen_resize(rows, cols);
            printf("ptyrun: [%.1f s] redimensionar %dx%d%s\n", secs(), rows, cols, r == 0 ? "" : " (falhou)");
        } else if (!strcmp(cmd, "tela")) {
            show(nw >= 2 ? rest : "");
        } else if (!strcmp(cmd, "eco")) {
            eco = 1;
            eco_from = plainn;
        } else if (!strcmp(cmd, "fim")) {
            int ms = nw >= 2 ? atoi(w[1]) : 30000;
            long t0 = now_ms();
            while (!child_done && now_ms() - t0 < ms) pump(100);
            for (int k = 0; k < 20 && !master_eof && masterfd >= 0; k++) pump(50);   /* o resto da saída */
            eco_line();
            if (!child_done && child > 0) {
                printf("ptyrun: [%.1f s] o filho não terminou em %d ms: SIGKILL\n", secs(), ms);
                show("antes do SIGKILL");
                kill(child, SIGKILL);
                failures++;
                for (int k = 0; k < 50 && !child_done; k++) pump(100);
            }
            show("final");
            break;
        } else {
            printf("ptyrun: instrução desconhecida: %s\n", raw);
            failures++;
        }
        fflush(stdout);
        line = next;
    }
    printf("ptyrun: fim (%d falha(s))\n", failures);
    fflush(stdout);
    return failures ? 1 : 0;
}
