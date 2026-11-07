/*
 * mOS — pseudoterminais, terminal de controle, sessões e grupos de processos (POSIX).
 *
 * Um par é criado por SYS_OPEN("/dev/ptmx") (o mestre) e o escravo é "/dev/ttysNNN", como no macOS
 * (posix_openpt/grantpt/unlockpt/ptsname do libSystem: userland/libsystem/pty.c). É o que o node-pty usa no
 * terminal integrado do VS Code, do Cursor e do Claude: o mestre fica no pty host (Node, laço do libuv sobre o
 * kqueue do libSystem) e o escravo vira 0/1/2 do shell, via posix_spawn(POSIX_SPAWN_SETSID) do spawn-helper,
 * que adota o escravo como terminal de controle ao abri-lo de novo.
 *
 *   mestre --write--> disciplina de linha --> fila de entrada --read--> escravo
 *   mestre <--read--- fila de saída <--- OPOST/ONLCR <---write--------- escravo   (e o eco da entrada)
 *
 * Disciplina de linha (o subconjunto que shells e editores usam): ICANON com ERASE/KILL/WERASE/REPRINT/
 * LNEXT/EOF/EOL, ECHO/ECHOE/ECHOK/ECHOKE/ECHOCTL/ECHONL, ISIG (INTR/QUIT/SUSP viram sinais para o grupo em
 * primeiro plano), ICRNL/INLCR/IGNCR/ISTRIP, IXON/IXANY (^S/^Q), IMAXBEL, IUTF8 (o apagar tira o caractere
 * inteiro), OPOST/ONLCR/OCRNL, e no modo não canônico VMIN/VTIME. A struct termios e os números de ioctl são
 * os do macOS (o libSystem repassa o pedido como veio). Sem modo pacote (TIOCPKT) e sem SIGTTIN/SIGTTOU: o
 * mOS não para processos, então os sinais de parada são ignorados por padrão (process.c: default_ignored).
 *
 * Espera: leitura sem dados e escrita com a fila cheia bloqueiam a thread (wait_pid = -9, pty_wait), salvo com
 * MOS_O_NONBLOCK no fd (-EAGAIN); cada mudança no par tenta concluir as operações pendentes (pty_wake), como os
 * pipes de process.c. VMIN = 0 com VTIME > 0 dorme com prazo (TH_SLEEPING, rax = 0 se ninguém escrever).
 *
 * Fim de linha: fechar o último mestre manda SIGHUP ao líder da sessão e ao grupo em primeiro plano; o escravo
 * lê fim de arquivo e a escrita dá EIO. Fechar o último escravo faz o mestre ler fim de arquivo (o node-pty
 * fecha o terminal assim). Sem nenhum fd das duas pontas, o par é liberado e as sessões perdem o terminal.
 */
#include "proc.h"
#include "syscall.h"

process_t *proc_list(void);
process_t *proc_find(int pid);
bool proc_signal(int pid, int sig);

/* termios do macOS (x86_64): tcflag_t e speed_t são unsigned long, NCCS = 20 (72 bytes) */
typedef struct {
    u64 iflag, oflag, cflag, lflag;
    u8 cc[20];
    u8 pad[4];
    u64 ispeed, ospeed;
} mac_termios_t;
typedef struct { u16 row, col, xpixel, ypixel; } mac_winsize_t;
_Static_assert(sizeof(mac_termios_t) == 72, "termios do macOS");

/* sys/termios.h do macOS */
#define IGNBRK  0x1
#define BRKINT  0x2
#define ISTRIP  0x20
#define INLCR   0x40
#define IGNCR   0x80
#define ICRNL   0x100
#define IXON    0x200
#define IXANY   0x800
#define IMAXBEL 0x2000
#define IUTF8   0x4000
#define OPOST   0x1
#define ONLCR   0x2
#define OCRNL   0x10
#define CS8     0x300
#define CREAD   0x800
#define HUPCL   0x4000
#define ECHOKE  0x1
#define ECHOE   0x2
#define ECHOK   0x4
#define ECHO    0x8
#define ECHONL  0x10
#define ECHOCTL 0x40
#define ISIG    0x80
#define ICANON  0x100
#define ALTWERASE 0x200
#define IEXTEN  0x400
#define NOFLSH  0x80000000ULL
enum { VEOF = 0, VEOL, VEOL2, VERASE, VWERASE, VKILL, VREPRINT, VINTR = 8, VQUIT, VSUSP, VDSUSP, VSTART, VSTOP,
       VLNEXT, VDISCARD, VMIN, VTIME, VSTATUS };
#define VDIS 0xFF   /* _POSIX_VDISABLE */

/* sys/ttycom.h e sys/filio.h do macOS */
#define TIOCGETA     0x40487413u
#define TIOCSETA     0x80487414u
#define TIOCSETAW    0x80487415u
#define TIOCSETAF    0x80487416u
#define TIOCGWINSZ   0x40087468u
#define TIOCSWINSZ   0x80087467u
#define TIOCSCTTY    0x20007461u
#define TIOCNOTTY    0x20007471u
#define TIOCGPGRP    0x40047477u
#define TIOCSPGRP    0x80047476u
#define TIOCGSID     0x40047463u
#define TIOCOUTQ     0x40047473u
#define TIOCSTI      0x80017472u
#define TIOCFLUSH    0x80047410u
#define TIOCDRAIN    0x2000745eu
#define TIOCEXCL     0x2000740du
#define TIOCNXCL     0x2000740eu
#define TIOCSTOP     0x2000746fu
#define TIOCSTART    0x2000746eu
#define TIOCGETD     0x4004741au
#define TIOCSETD     0x8004741bu
#define TIOCSBRK     0x2000747bu
#define TIOCCBRK     0x2000747au
#define TIOCCONS     0x80047462u
#define TIOCPKT      0x80047470u
#define TIOCPTYGRANT 0x20007454u
#define TIOCPTYGNAME 0x40807453u
#define TIOCPTYUNLK  0x20007452u
#define FIONREAD     0x4004667fu
#define FIONBIO      0x8004667eu
#define FIOASYNC     0x8004667du

#define PTY_INQ   4096          /* entrada pronta para o escravo ler */
#define PTY_OUTQ  32768         /* saída do escravo (e eco) esperando o mestre */
#define PTY_CANON 1024          /* MAX_CANON do macOS: a linha em edição */
#define PTY_LINES 256           /* linhas completas na fila de entrada (modo canônico) */

enum { PTY_RD_MASTER, PTY_RD_SLAVE, PTY_WR_MASTER, PTY_WR_SLAVE };
#define PTY_WAIT (-9)           /* thread_t.wait_pid de quem espera num terminal */

typedef struct pty {
    int idx;                    /* /dev/ttys%03d */
    int mrefs, srefs;           /* fds abertos de cada ponta, em todos os processos */
    bool unlocked;              /* unlockpt */
    bool slave_seen;            /* o escravo já foi aberto (antes disso o mestre não lê fim de arquivo) */
    bool stopped;               /* ^S (IXON) ou TIOCSTOP: a saída espera um ^Q */
    bool lnext;                 /* ^V: o próximo caractere entra literal */
    mac_termios_t tio;
    mac_winsize_t ws;
    int sid, pgrp;              /* sessão que o tem como terminal de controle (0 = nenhuma) e grupo em primeiro plano */
    u8 in[PTY_INQ]; u32 ih, ic;         /* fila de entrada (início, bytes) */
    u16 lines[PTY_LINES]; u32 lh, lc;   /* canônico: bytes restantes de cada linha completa na fila; 0 = fim de arquivo */
    u8 line[PTY_CANON]; u32 ll;         /* linha em edição */
    u8 out[PTY_OUTQ]; u32 oh, oc;       /* fila de saída */
} pty_t;

static pty_t *ptys[MOS_PTY_MAX];

/* ------------------------------------------------------------ filas */
static bool out_put(pty_t *t, u8 c) {
    if (t->oc >= PTY_OUTQ) return false;
    t->out[(t->oh + t->oc) % PTY_OUTQ] = c;
    t->oc++;
    return true;
}
/* um byte de saída com o processamento do termios (OPOST); false = sem espaço */
static bool out_byte(pty_t *t, u8 c) {
    u64 of = t->tio.oflag;
    if (of & OPOST) {
        if (c == '\n' && (of & ONLCR)) {
            if (PTY_OUTQ - t->oc < 2) return false;
            out_put(t, '\r');
            return out_put(t, '\n');
        }
        if (c == '\r' && (of & OCRNL)) c = '\n';
    }
    return out_put(t, c);
}
static void out_str(pty_t *t, const char *s) { while (*s) out_byte(t, (u8)*s++); }

static bool in_put(pty_t *t, u8 c) {
    if (t->ic >= PTY_INQ) return false;
    t->in[(t->ih + t->ic) % PTY_INQ] = c;
    t->ic++;
    return true;
}
static void flush_input(pty_t *t) { t->ih = t->ic = 0; t->lh = t->lc = 0; t->ll = 0; t->lnext = false; }
static void flush_output(pty_t *t) { t->oh = t->oc = 0; }

/* ------------------------------------------------------------ sinais para grupos */
static int pgsignal(int pgrp, int sig) {
    int n = 0;
    if (pgrp <= 0) return 0;
    for (process_t *q = proc_list(); q; q = q->next)
        if (!q->exited && q->pgid == pgrp) { if (sig) proc_signal(q->pid, sig); n++; }
    return n;
}
static bool pgrp_in_session(int pgrp, int sid) {
    for (process_t *q = proc_list(); q; q = q->next)
        if (!q->exited && q->pgid == pgrp && q->sid == sid) return true;
    return false;
}

/* ------------------------------------------------------------ eco e edição de linha */
static bool ctl_char(u8 c) { return (c < 0x20 && c != '\t' && c != '\n') || c == 0x7F; }

static void echo_char(pty_t *t, u8 c) {
    u64 lf = t->tio.lflag;
    if (!(lf & ECHO)) {
        if (c == '\n' && (lf & ECHONL) && (lf & ICANON)) out_byte(t, '\n');
        return;
    }
    if ((lf & ECHOCTL) && ctl_char(c)) {
        out_byte(t, '^');
        out_byte(t, c == 0x7F ? '?' : (u8)(c + 0x40));
        return;
    }
    out_byte(t, c);
}

/* quantas colunas o caractere ocupou no eco (para apagar com "\b \b") */
static int echo_width(pty_t *t, u8 c) { return ((t->tio.lflag & ECHOCTL) && ctl_char(c)) ? 2 : 1; }

/* tira o último caractere da linha (inteiro em UTF-8 com IUTF8) e o apaga da tela */
static bool erase_one(pty_t *t) {
    if (!t->ll) return false;
    u32 end = t->ll, st = end - 1;
    if (t->tio.iflag & IUTF8)
        while (st > 0 && (t->line[st] & 0xC0) == 0x80) st--;
    u8 first = t->line[st];
    t->ll = st;
    u64 lf = t->tio.lflag;
    if (lf & ECHO) {
        if (lf & ECHOE) for (int w = echo_width(t, first); w > 0; w--) out_str(t, "\b \b");
        else echo_char(t, t->tio.cc[VERASE]);
    }
    return true;
}
static bool word_char(pty_t *t, u8 c) {
    if (t->tio.lflag & ALTWERASE) return (c >= '0' && c <= '9') || ((c | 32) >= 'a' && (c | 32) <= 'z') || c == '_' || c >= 0x80;
    return c != ' ' && c != '\t';
}
static void erase_word(pty_t *t) {
    while (t->ll && (t->line[t->ll - 1] == ' ' || t->line[t->ll - 1] == '\t')) erase_one(t);
    while (t->ll && word_char(t, t->line[t->ll - 1])) erase_one(t);
}
static void kill_line(pty_t *t) {
    u64 lf = t->tio.lflag;
    if ((lf & ECHO) && (lf & ECHOKE) && (lf & ECHOE)) { while (t->ll) erase_one(t); return; }
    if (lf & ECHO) {
        echo_char(t, t->tio.cc[VKILL]);
        if (lf & ECHOK) out_byte(t, '\n');
    }
    t->ll = 0;
}
static void reprint(pty_t *t) {
    if (!(t->tio.lflag & ECHO)) return;
    echo_char(t, t->tio.cc[VREPRINT]);
    out_byte(t, '\n');
    for (u32 i = 0; i < t->ll; i++) echo_char(t, t->line[i]);
}

/* a linha em edição vai para a fila de entrada (com o terminador, se veio um); ll = 0 marca fim de arquivo */
static bool line_done(pty_t *t) {
    if (t->lc >= PTY_LINES || PTY_INQ - t->ic < t->ll) return false;
    for (u32 i = 0; i < t->ll; i++) in_put(t, t->line[i]);
    t->lines[(t->lh + t->lc) % PTY_LINES] = (u16)t->ll;
    t->lc++;
    t->ll = 0;
    return true;
}

static bool is_eol(pty_t *t, u8 c) {
    const u8 *cc = t->tio.cc;
    return c == '\n' || (c == cc[VEOL] && c != VDIS) || (c == cc[VEOL2] && c != VDIS);
}

/* cabe mais este byte digitado? (escrita no mestre). No canônico, um caractere comum sempre "cabe": com a linha
 * cheia ele é descartado (IMAXBEL toca o sino), como no BSD; o terminador precisa de espaço para a linha inteira. */
static bool can_take(pty_t *t, u8 c) {
    const u8 *cc = t->tio.cc;
    u64 lf = t->tio.lflag, ifl = t->tio.iflag;
    if (!t->lnext && c != VDIS) {                     /* ^C, ^\, ^Z, ^S, ^Q não ocupam a fila: passam sempre */
        if ((lf & ISIG) && (c == cc[VINTR] || c == cc[VQUIT] || c == cc[VSUSP])) return true;
        if ((ifl & IXON) && (c == cc[VSTOP] || c == cc[VSTART])) return true;
    }
    if (!(lf & ICANON)) return t->ic < PTY_INQ;
    if (t->lnext) return true;
    if (c == '\r' && (ifl & ICRNL) && !(ifl & IGNCR)) c = '\n';
    if (is_eol(t, c)) return t->lc < PTY_LINES && PTY_INQ - t->ic >= t->ll + 1;
    if (c == t->tio.cc[VEOF] && c != VDIS) return t->lc < PTY_LINES && PTY_INQ - t->ic >= t->ll;
    return true;
}

/* um byte digitado (escrito no mestre), com a disciplina de linha */
static void pty_input(pty_t *t, u8 c) {
    mac_termios_t *o = &t->tio;
    u64 ifl = o->iflag, lfl = o->lflag;
    const u8 *cc = o->cc;
    if (ifl & ISTRIP) c &= 0x7F;
    bool canon = (lfl & ICANON) != 0;
    if (t->lnext) {                                   /* depois do ^V: literal */
        t->lnext = false;
        if (canon) {
            if (t->ll < PTY_CANON - 1) { t->line[t->ll++] = c; echo_char(t, c); }
        } else if (in_put(t, c)) echo_char(t, c);
        return;
    }
    if ((lfl & IEXTEN) && c == cc[VLNEXT] && c != VDIS) {
        t->lnext = true;
        if (lfl & ECHO) { if (lfl & ECHOE) out_str(t, "^\b"); else echo_char(t, c); }
        return;
    }
    if (lfl & ISIG) {
        int sig = 0;
        if (c == cc[VINTR] && c != VDIS) sig = MOS_SIGINT;
        else if (c == cc[VQUIT] && c != VDIS) sig = MOS_SIGQUIT;
        else if (c == cc[VSUSP] && c != VDIS) sig = MOS_SIGTSTP;
        if (sig) {
            if (!(lfl & NOFLSH)) { flush_input(t); if (sig != MOS_SIGTSTP) flush_output(t); }
            echo_char(t, c);
            pgsignal(t->pgrp, sig);
            return;
        }
    }
    if (ifl & IXON) {
        if (c == cc[VSTOP] && c != VDIS) { t->stopped = true; return; }
        if (c == cc[VSTART] && c != VDIS) { t->stopped = false; return; }
        if (t->stopped && (ifl & IXANY)) t->stopped = false;
    }
    if (c == '\r') { if (ifl & IGNCR) return; if (ifl & ICRNL) c = '\n'; }
    else if (c == '\n' && (ifl & INLCR)) c = '\r';
    if (!canon) {
        if (in_put(t, c)) echo_char(t, c);
        return;
    }
    if (c == cc[VERASE] && c != VDIS) { erase_one(t); return; }
    if (c == cc[VKILL] && c != VDIS) { kill_line(t); return; }
    if (lfl & IEXTEN) {
        if (c == cc[VWERASE] && c != VDIS) { erase_word(t); return; }
        if (c == cc[VREPRINT] && c != VDIS) { reprint(t); return; }
    }
    if (c == cc[VEOF] && c != VDIS) { line_done(t); return; }   /* o ^D não entra: linha vazia = fim de arquivo */
    if (is_eol(t, c)) {
        t->line[t->ll++] = c;                         /* can_take garantiu o espaço */
        if (!line_done(t)) t->ll--;
        echo_char(t, c);
        return;
    }
    if (t->ll >= PTY_CANON - 1) { if (ifl & IMAXBEL) out_put(t, '\a'); return; }
    t->line[t->ll++] = c;
    echo_char(t, c);
}

/* troca de modo: canônico -> cru, o que estava pendente vira entrada crua; cru -> canônico, a entrada crua ainda
 * não lida é reeditada em linhas (sem eco nem sinais: já passou pela disciplina ao ser digitada) */
static void set_termios(pty_t *t, const mac_termios_t *n, bool flush) {
    bool was = (t->tio.lflag & ICANON) != 0, now = (n->lflag & ICANON) != 0;
    if (flush) flush_input(t);
    t->tio = *n;
    if (was && !now) {
        for (u32 i = 0; i < t->ll && in_put(t, t->line[i]); i++) {}
        t->ll = 0;
        t->lh = t->lc = 0;
    } else if (!was && now && t->ic) {
        u32 n2 = t->ic;
        u8 *tmp = kmalloc(n2);                        /* fora da pilha do kernel (32 KiB) */
        if (tmp) {
            for (u32 i = 0; i < n2; i++) tmp[i] = t->in[(t->ih + i) % PTY_INQ];
            t->ih = t->ic = 0;
            t->lh = t->lc = 0;
            for (u32 i = 0; i < n2; i++) {
                u8 c = tmp[i];                        /* como o PENDIN do BSD: a conversão de \r vale com o termios novo */
                if (c == '\r') { if (n->iflag & IGNCR) continue; if (n->iflag & ICRNL) c = '\n'; }
                else if (c == '\n' && (n->iflag & INLCR)) c = '\r';
                if (t->ll < PTY_CANON - 1) t->line[t->ll++] = c;
                if (is_eol(t, c) || t->ll >= PTY_CANON - 1) line_done(t);
            }
            kfree(tmp);
        }
    }
    if (!(n->iflag & IXON)) t->stopped = false;
}

/* ------------------------------------------------------------ leitura e escrita */
/* copia até n bytes da fila circular para o usuário (no máximo dois trechos contíguos) */
static i64 ring_out(process_t *p, u64 buf, const u8 *ring, u32 cap, u32 *head, u32 *count, u32 n) {
    u32 done = 0;
    while (done < n) {
        u32 seg = MIN(n - done, cap - *head);
        if (!uvm_copy_out(p, buf + done, ring + *head, seg)) return done ? (i64)done : -MOS_EFAULT;
        *head = (*head + seg) % cap;
        *count -= seg;
        done += seg;
    }
    return (i64)done;
}

static bool slave_hup(pty_t *t) { return t->mrefs <= 0; }                    /* o mestre fechou */
static bool master_eof(pty_t *t) { return t->slave_seen && t->srefs <= 0; }  /* ninguém mais com o escravo */

/* -MOS_EAGAIN = esperaria */
static i64 try_op(pty_t *t, int mode, process_t *p, u64 buf, u64 len) {
    switch (mode) {
    case PTY_RD_MASTER:
        if (t->oc && !t->stopped) return ring_out(p, buf, t->out, PTY_OUTQ, &t->oh, &t->oc, (u32)MIN(len, (u64)t->oc));
        if (master_eof(t) && !t->oc) return 0;
        return -MOS_EAGAIN;
    case PTY_RD_SLAVE:
        if (slave_hup(t)) return 0;
        if (t->tio.lflag & ICANON) {
            if (!t->lc) return -MOS_EAGAIN;
            u32 left = t->lines[t->lh];
            if (!left) { t->lh = (t->lh + 1) % PTY_LINES; t->lc--; return 0; }   /* ^D numa linha vazia */
            i64 r = ring_out(p, buf, t->in, PTY_INQ, &t->ih, &t->ic, (u32)MIN(len, (u64)left));
            if (r > 0) {
                t->lines[t->lh] = (u16)(left - (u32)r);
                if (!t->lines[t->lh]) { t->lh = (t->lh + 1) % PTY_LINES; t->lc--; }
            }
            return r;
        } else {
            u8 vmin = t->tio.cc[VMIN], vtime = t->tio.cc[VTIME];
            u32 need = vmin ? (u32)MIN((u64)vmin, len) : 1;
            if (vtime) need = 1;                      /* com VTIME o temporizador vale a partir do primeiro byte */
            if (t->ic < need) return (!vmin && !vtime) ? 0 : -MOS_EAGAIN;
            return ring_out(p, buf, t->in, PTY_INQ, &t->ih, &t->ic, (u32)MIN(len, (u64)t->ic));
        }
    case PTY_WR_SLAVE: {
        if (slave_hup(t)) return -MOS_EIO;
        u8 tmp[256];
        u64 done = 0;
        while (done < len) {
            u32 room = PTY_OUTQ - t->oc;
            if (!room) break;
            u32 k = (u32)MIN(MIN(len - done, (u64)sizeof tmp), (u64)room);
            if (!uvm_copy_in(p, tmp, buf + done, k)) return done ? (i64)done : -MOS_EFAULT;
            u32 i = 0;
            while (i < k && out_byte(t, tmp[i])) i++;
            done += i;
            if (i < k) break;
        }
        return done ? (i64)done : -MOS_EAGAIN;
    }
    case PTY_WR_MASTER: {
        if (master_eof(t)) return -MOS_EIO;           /* o shell e os filhos já fecharam o escravo */
        u8 tmp[256];
        u64 done = 0;
        while (done < len) {
            u32 k = (u32)MIN(len - done, (u64)sizeof tmp);
            if (!uvm_copy_in(p, tmp, buf + done, k)) return done ? (i64)done : -MOS_EFAULT;
            u32 i = 0;
            while (i < k && can_take(t, tmp[i])) pty_input(t, tmp[i++]);
            done += i;
            if (i < k) break;
        }
        return done ? (i64)done : -MOS_EAGAIN;
    }
    }
    return -MOS_EINVAL;
}

/* conclui as operações bloqueadas que agora podem andar (uma pode liberar a outra: algumas voltas) */
static void pty_wake(pty_t *t) {
    for (int round = 0; round < 4; round++) {
        bool progress = false;
        for (thread_t *w = sched_threads(); w; w = w->next) {
            if (w->pty_wait != t || w->wait_pid != PTY_WAIT || !w->proc) continue;
            if (w->state != TH_WAITING && w->state != TH_SLEEPING) continue;
            i64 n = try_op(t, w->pty_mode, w->proc, w->read_buf, w->read_len);
            if (n == -MOS_EAGAIN) continue;
            ((int_frame_t *)w->frame)->rax = (u64)n;
            w->pty_wait = NULL;
            w->wake_at = 0;
            w->state = TH_READY;
            progress = true;
        }
        if (!progress) break;
    }
}

/* acorda com erro quem ainda espera num par que vai sumir */
static void pty_fail_waiters(pty_t *t, i64 err) {
    for (thread_t *w = sched_threads(); w; w = w->next) {
        if (w->pty_wait != t || w->wait_pid != PTY_WAIT) continue;
        if (w->state == TH_WAITING || w->state == TH_SLEEPING) {
            ((int_frame_t *)w->frame)->rax = (u64)err;
            w->wake_at = 0;
            w->state = TH_READY;
        }
        w->pty_wait = NULL;
    }
}

u64 pty_rw(int_frame_t *f, thread_t *th, process_t *p, int fd, bool rd, u64 buf, u64 len) {
    pty_t *t = p->fds[fd].pty;
    bool master = p->fds[fd].kind == FD_PTY_M;
    int fl = p->fds[fd].flags;
    if (rd ? (fl & MOS_O_WRONLY) != 0 : !(fl & (MOS_O_WRONLY | MOS_O_RDWR))) { f->rax = (u64)-MOS_EBADF; return (u64)f; }
    if (!len) { f->rax = 0; return (u64)f; }
    int mode = rd ? (master ? PTY_RD_MASTER : PTY_RD_SLAVE) : (master ? PTY_WR_MASTER : PTY_WR_SLAVE);
    i64 n = try_op(t, mode, p, buf, len);
    if (n != -MOS_EAGAIN) {
        if (n >= 0) pty_wake(t);                      /* espaço ou dados novos (e eco) para a outra ponta */
        f->rax = (u64)n;
        return (u64)f;
    }
    if (fl & MOS_O_NONBLOCK) { f->rax = (u64)-MOS_EAGAIN; return (u64)f; }
    th->frame = (u64)f;
    th->read_buf = buf;
    th->read_len = len;
    th->wait_pid = PTY_WAIT;
    th->pty_wait = t;
    th->pty_mode = (u8)mode;
    if (mode == PTY_RD_SLAVE && !(t->tio.lflag & ICANON) && !t->tio.cc[VMIN] && t->tio.cc[VTIME]) {
        f->rax = 0;                                   /* VMIN = 0, VTIME > 0: sem dados no prazo, read devolve 0 */
        th->wake_at = timer_ticks() + (u64)t->tio.cc[VTIME] * 100;
        th->state = TH_SLEEPING;
    } else th->state = TH_WAITING;
    return sched_switch(f);
}

/* ------------------------------------------------------------ vida do par */
static pty_t *pty_new(int idx) {
    pty_t *t = kcalloc(1, sizeof *t);
    if (!t) return NULL;
    t->idx = idx;
    /* padrões do macOS para um terminal novo (TTYDEF_* de sys/ttydefaults.h) */
    t->tio.iflag = BRKINT | ICRNL | IMAXBEL | IXON | IXANY;
    t->tio.oflag = OPOST | ONLCR;
    t->tio.cflag = CREAD | CS8 | HUPCL;
    t->tio.lflag = ECHO | ICANON | ISIG | IEXTEN | ECHOE | ECHOKE | ECHOCTL;
    static const u8 defcc[20] = {4, VDIS, VDIS, 0x7F, 0x17, 0x15, 0x12, VDIS, 3, 0x1C, 0x1A, 0x19, 0x11, 0x13,
                                 0x16, 0x0F, 1, 0, 0x14, VDIS};
    memcpy(t->tio.cc, defcc, sizeof defcc);
    t->tio.ispeed = t->tio.ospeed = 9600;
    t->ws.row = 24;
    t->ws.col = 80;
    ptys[idx] = t;
    return t;
}

static void pty_free(pty_t *t) {
    pty_fail_waiters(t, -MOS_EBADF);
    for (process_t *q = proc_list(); q; q = q->next) if (q->ctty == t) q->ctty = NULL;
    if (t->idx >= 0 && t->idx < MOS_PTY_MAX && ptys[t->idx] == t) ptys[t->idx] = NULL;
    kfree(t);
}

/* a sessão perde o terminal de controle */
static void detach_session(pty_t *t) {
    int sid = t->sid;
    t->sid = t->pgrp = 0;
    if (sid) for (process_t *q = proc_list(); q; q = q->next) if (q->sid == sid && q->ctty == t) q->ctty = NULL;
}

void pty_fd_ref(struct pty *t, bool master, int delta) {
    if (!t) return;
    if (master) t->mrefs += delta; else t->srefs += delta;
    if (delta > 0) return;
    if (master && t->mrefs <= 0 && t->sid) {          /* fim de linha: o shell recebe SIGHUP (bash sai) */
        int leader = t->sid, pg = t->pgrp;
        kprintf("pty: mestre de /dev/ttys%03d fechado: SIGHUP para a sessão %d\n", t->idx, leader);
        proc_signal(leader, MOS_SIGHUP);
        if (pg > 0 && pg != leader) { pgsignal(pg, MOS_SIGHUP); pgsignal(pg, MOS_SIGCONT); }
    }
    if (t->mrefs <= 0 && t->srefs <= 0) { pty_free(t); return; }
    pty_wake(t);                                      /* leitores veem o fim de arquivo, escritores o EIO */
}

static void set_name(char *dst, usize cap, pty_t *t, bool master) {
    if (master) snprintf(dst, cap, "/dev/ptmx");
    else snprintf(dst, cap, "/dev/ttys%03d", t->idx);
}

/* "/dev/ttysNNN" -> índice, ou -1 */
static int slave_index(const char *n) {
    if (strncmp(n, "/dev/ttys", 9)) return -1;
    int v = 0, k = 0;
    for (const char *s = n + 9; *s; s++, k++) {
        if (*s < '0' || *s > '9' || k >= 4) return -1;
        v = v * 10 + (*s - '0');
    }
    return k ? v : -1;
}

bool pty_stat_path(const char *path) {
    if (!strcmp(path, "/dev/ptmx") || !strcmp(path, "/dev/tty")) return true;
    int i = slave_index(path);
    return i >= 0 && i < MOS_PTY_MAX && ptys[i];
}

/* adota t como terminal de controle (líder de sessão sem terminal, terminal livre) */
static bool acquire_ctty(process_t *p, pty_t *t) {
    if (p->sid != p->pid || p->ctty || t->sid) return p->ctty == t;
    p->ctty = t;
    t->sid = p->sid;
    t->pgrp = p->pgid;
    kprintf("pty: /dev/ttys%03d é o terminal de controle da sessão de %s (pid %d)\n", t->idx, p->name, p->pid);
    return true;
}

int pty_open_path(process_t *p, int fd, const char *path, int flags) {
    if (strncmp(path, "/dev/", 5)) return 1;
    pty_t *t = NULL;
    bool master = false;
    if (!strcmp(path, "/dev/ptmx")) {
        int i = 0;
        while (i < MOS_PTY_MAX && ptys[i]) i++;
        if (i == MOS_PTY_MAX) return -MOS_EAGAIN;     /* posix_openpt: sem pares livres */
        t = pty_new(i);
        if (!t) return -MOS_ENOMEM;
        master = true;
    } else if (!strcmp(path, "/dev/tty")) {
        t = p->ctty;
        if (!t || slave_hup(t)) return -MOS_ENXIO;    /* sem terminal de controle */
    } else {
        int i = slave_index(path);
        if (i < 0) return 1;
        if (i >= MOS_PTY_MAX || !ptys[i]) return -MOS_ENOENT;
        t = ptys[i];
        if (!t->unlocked) return -MOS_EAGAIN;         /* como o macOS: falta o unlockpt */
        if (slave_hup(t)) return -MOS_EIO;
    }
    memset(&p->fds[fd], 0, sizeof p->fds[fd]);
    p->fds[fd].used = true;
    p->fds[fd].kind = master ? FD_PTY_M : FD_PTY_S;
    p->fds[fd].flags = flags & ~(MOS_O_CREAT | MOS_O_TRUNC | MOS_O_APPEND);
    p->fds[fd].pty = t;
    set_name(p->fds[fd].path, sizeof p->fds[fd].path, t, master);
    pty_fd_ref(t, master, +1);
    if (!master) {
        t->slave_seen = true;
        if (!(flags & MOS_O_NOCTTY)) acquire_ctty(p, t);
    } else kprintf("pty: %s (pid %d) abriu o par /dev/ttys%03d\n", p->name, p->pid, t->idx);
    return 0;
}

i64 pty_fstat(process_t *p, int fd, u64 st_va) {
    pty_t *t = p->fds[fd].pty;
    mos_stat_t st = {0};
    st.is_dir = 5;
    if (p->fds[fd].kind == FD_PTY_M) {
        st.size = t->stopped ? 0 : t->oc;
        st.mtime = master_eof(t) && !t->oc;
    } else if (slave_hup(t)) {
        st.mtime = 1;
    } else if (t->tio.lflag & ICANON) {
        st.size = t->ic;                              /* só linhas completas estão na fila */
        st.mtime = t->lc && !t->lines[t->lh];         /* ^D numa linha vazia: o read devolve 0 */
    } else {
        st.size = t->ic;
    }
    return uvm_copy_out(p, st_va, &st, sizeof st) ? 0 : -MOS_EFAULT;
}

/* ------------------------------------------------------------ sessões e grupos */
void pty_proc_init(process_t *child, process_t *parent, unsigned spawn_flags) {
    if (parent && parent->pid > 1 && parent->sid > 0) {
        child->pgid = parent->pgid;
        child->sid = parent->sid;
        child->ctty = parent->ctty;
    } else {                                          /* do kernel ou do init: sessão própria, como os jobs do launchd */
        child->pgid = child->sid = child->pid;
        child->ctty = NULL;
    }
    if (spawn_flags & MOS_SPAWN_SETSID) {
        child->pgid = child->sid = child->pid;
        child->ctty = NULL;
    } else if (spawn_flags & MOS_SPAWN_SETPGROUP) child->pgid = child->pid;
}

void pty_proc_exit(process_t *p) {
    pty_t *t = p->ctty;
    if (t && p->sid == p->pid && t->sid == p->sid) {  /* processo de controle: o grupo em primeiro plano recebe SIGHUP */
        int pg = t->pgrp;
        detach_session(t);
        if (pg > 0 && pg != p->pid) pgsignal(pg, MOS_SIGHUP);
        pty_wake(t);
    }
    p->ctty = NULL;
}

static i64 copy_int_out(process_t *p, u64 va, int v) { return uvm_copy_out(p, va, &v, sizeof v) ? 0 : -MOS_EFAULT; }

static i64 tty_ioctl(process_t *p, int fd, u32 req, u64 arg) {
    if (fd < 0 || fd >= PROC_MAX_FDS || !p->fds[fd].used) return -MOS_EBADF;
    pty_t *t = p->fds[fd].pty;
    if (!t) return -MOS_ENOTTY;
    bool master = p->fds[fd].kind == FD_PTY_M;
    switch (req) {
    case TIOCGETA:
        return uvm_copy_out(p, arg, &t->tio, sizeof t->tio) ? 0 : -MOS_EFAULT;
    case TIOCSETA: case TIOCSETAW: case TIOCSETAF: {
        mac_termios_t n;
        if (!uvm_copy_in(p, &n, arg, sizeof n)) return -MOS_EFAULT;
        set_termios(t, &n, req == TIOCSETAF);
        pty_wake(t);
        return 0;
    }
    case TIOCGWINSZ:
        return uvm_copy_out(p, arg, &t->ws, sizeof t->ws) ? 0 : -MOS_EFAULT;
    case TIOCSWINSZ: {
        mac_winsize_t w;
        if (!uvm_copy_in(p, &w, arg, sizeof w)) return -MOS_EFAULT;
        if (memcmp(&w, &t->ws, sizeof w)) {           /* o shell redesenha: readline relê o tamanho */
            t->ws = w;
            pgsignal(t->pgrp, MOS_SIGWINCH);
        }
        return 0;
    }
    case TIOCSCTTY:
        if (master) return -MOS_ENOTTY;
        return acquire_ctty(p, t) ? 0 : -MOS_EPERM;
    case TIOCNOTTY:
        if (p->ctty != t) return -MOS_ENOTTY;
        if (p->sid == p->pid) { int pg = t->pgrp; detach_session(t); if (pg > 0 && pg != p->pid) pgsignal(pg, MOS_SIGHUP); }
        p->ctty = NULL;
        return 0;
    case TIOCGPGRP:                                   /* no mestre vale sempre (o node-pty pergunta o processo em frente) */
        if (!master && p->ctty != t) return -MOS_ENOTTY;
        return copy_int_out(p, arg, t->pgrp);
    case TIOCSPGRP: {
        int pg;
        if (p->ctty != t || t->sid != p->sid) return -MOS_ENOTTY;
        if (!uvm_copy_in(p, &pg, arg, sizeof pg)) return -MOS_EFAULT;
        if (pg <= 0) return -MOS_EINVAL;
        if (!pgrp_in_session(pg, p->sid)) return -MOS_EPERM;
        t->pgrp = pg;
        return 0;
    }
    case TIOCGSID:
        if (!t->sid) return -MOS_ENOTTY;
        return copy_int_out(p, arg, t->sid);
    case FIONREAD:
        return copy_int_out(p, arg, (int)(master ? t->oc : t->ic));
    case TIOCOUTQ:
        return copy_int_out(p, arg, (int)(master ? t->ic : t->oc));
    case TIOCSTI: {
        u8 c;
        if (!master && p->ctty != t) return -MOS_EACCES;
        if (!uvm_copy_in(p, &c, arg, 1)) return -MOS_EFAULT;
        if (can_take(t, c)) pty_input(t, c);
        pty_wake(t);
        return 0;
    }
    case TIOCFLUSH: {
        int which = 0;
        if (arg && !uvm_copy_in(p, &which, arg, sizeof which)) return -MOS_EFAULT;
        if (!which) which = 3;                        /* FREAD | FWRITE */
        if (which & 1) flush_input(t);
        if (which & 2) flush_output(t);
        pty_wake(t);
        return 0;
    }
    case TIOCSTOP: t->stopped = true; return 0;
    case TIOCSTART: t->stopped = false; pty_wake(t); return 0;
    case TIOCDRAIN: case TIOCEXCL: case TIOCNXCL: case TIOCSBRK: case TIOCCBRK: case TIOCCONS: case FIOASYNC:
        return 0;
    case TIOCGETD:
        return copy_int_out(p, arg, 0);               /* TTYDISC */
    case TIOCSETD:
        return 0;
    case TIOCPKT: {                                   /* modo pacote: não existe aqui; desligar é aceito */
        int on = 0;
        if (arg && !uvm_copy_in(p, &on, arg, sizeof on)) return -MOS_EFAULT;
        return on ? -MOS_EINVAL : 0;
    }
    case TIOCPTYGRANT:
        return master ? 0 : -MOS_ENOTTY;
    case TIOCPTYUNLK:
        if (!master) return -MOS_ENOTTY;
        t->unlocked = true;
        return 0;
    case TIOCPTYGNAME: {                              /* só o mestre: o libuv distingue mestre e escravo assim */
        char name[128];
        if (!master) return -MOS_ENOTTY;
        memset(name, 0, sizeof name);
        set_name(name, sizeof name, t, false);
        return uvm_copy_out(p, arg, name, sizeof name) ? 0 : -MOS_EFAULT;
    }
    case FIONBIO: {
        int on = 0;
        if (!uvm_copy_in(p, &on, arg, sizeof on)) return -MOS_EFAULT;
        if (on) p->fds[fd].flags |= MOS_O_NONBLOCK; else p->fds[fd].flags &= ~MOS_O_NONBLOCK;
        return 0;
    }
    }
    return -MOS_ENOTTY;
}

i64 pty_syscall(process_t *p, u64 op, u64 a, u64 b, u64 c) {
    switch (op) {
    case MOS_TTY_IOCTL:
        return tty_ioctl(p, (int)(i64)a, (u32)b, c);
    case MOS_TTY_NAME: {
        int fd = (int)(i64)a;
        if (fd < 0 || fd >= PROC_MAX_FDS || !p->fds[fd].used) return -MOS_EBADF;
        pty_t *t = p->fds[fd].pty;
        if (!t) return -MOS_ENOTTY;
        char name[32];
        set_name(name, sizeof name, t, false);
        usize n = strlen(name) + 1;
        if (n > c) return -34;                        /* ERANGE */
        return uvm_copy_out(p, b, name, n) ? (i64)(n - 1) : -MOS_EFAULT;
    }
    case MOS_TTY_SETFL: {
        int fd = (int)(i64)a;
        if (fd < 0 || fd >= PROC_MAX_FDS || !p->fds[fd].used) return -MOS_EBADF;
        if (b) p->fds[fd].flags |= MOS_O_NONBLOCK; else p->fds[fd].flags &= ~MOS_O_NONBLOCK;
        return 0;
    }
    case MOS_TTY_SETSID:
        for (process_t *q = proc_list(); q; q = q->next)
            if (!q->exited && q->pgid == p->pid) return -MOS_EPERM;   /* já lidera um grupo */
        p->sid = p->pgid = p->pid;
        p->ctty = NULL;
        return p->sid;
    case MOS_TTY_SETPGID: {
        process_t *q = a ? proc_find((int)a) : p;
        if (!q || q->exited) return -MOS_ESRCH;
        if (q != p && q->ppid != p->pid) return -MOS_ESRCH;
        if (q->sid != p->sid || q->pid == q->sid) return -MOS_EPERM;
        int pg = b ? (int)b : q->pid;
        if (pg < 0) return -MOS_EINVAL;
        if (pg != q->pid && !pgrp_in_session(pg, q->sid)) return -MOS_EPERM;
        q->pgid = pg;
        return 0;
    }
    case MOS_TTY_GETPGID: case MOS_TTY_GETSID: {
        process_t *q = a ? proc_find((int)a) : p;
        if (!q) return -MOS_ESRCH;
        return op == MOS_TTY_GETPGID ? q->pgid : q->sid;
    }
    case MOS_TTY_KILLPG: {
        int pg = a ? (int)a : p->pgid;
        if ((int)b < 0 || (int)b >= MOS_NSIG) return -MOS_EINVAL;
        return pgsignal(pg, (int)b) ? 0 : -MOS_ESRCH;
    }
    case MOS_TTY_PROCINFO: {
        process_t *q = a ? proc_find((int)a) : p;
        if (!q) return -MOS_ESRCH;
        int out[4] = {q->pgid, q->sid, q->ctty ? q->ctty->idx : -1, q->ctty && q->ctty->sid ? q->ctty->pgrp : -1};
        return uvm_copy_out(p, b, out, sizeof out) ? 0 : -MOS_EFAULT;
    }
    }
    return -MOS_EINVAL;
}
