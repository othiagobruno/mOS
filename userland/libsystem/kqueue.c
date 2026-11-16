/*
 * mOS libSystem — kqueue/kevent dentro do processo, para o porte do VS Code: a thread de E/S do
 * Chromium (MessagePumpKqueue) espera em kevent64 por portas Mach (EVFILT_MACHPORT), gatilhos
 * (EVFILT_USER) e timers.
 *
 * Filtros (docs/darwin-bsd-mach.md, F6): READ, WRITE, EXCEPT, AIO não, VNODE (journal do VFS do Vinicius), PROC
 * (EXIT, EXITSTATUS, FORK, EXEC, SIGNAL), SIGNAL (conta mesmo com SIG_IGN, libc.c), TIMER (s/ms/us/ns, absoluto,
 * repetitivo), MACHPORT (com MACH_RCV_MSG a mensagem vem no buffer de ext[0]/ext[1]), FS (montar/desmontar), USER,
 * VM e MEMORYSTATUS (nível de pressão do kernel, SYS_VMCTL do Gustavo). Flags: ADD, DELETE, ENABLE, DISABLE,
 * ONESHOT, CLEAR, RECEIPT, DISPATCH, DISPATCH2, UDATA_SPECIFIC, EOF, ERROR.
 * A espera ainda é consulta com cochilo crescente (1..8 ms, nunca além do próximo timer): o kernel não espera em
 * várias fontes de uma vez. Os descritores de kqueue são números a partir de KQ_FD_BASE (não passam pelo kernel).
 */
#include "sys.h"

void *calloc(size_t n, size_t s);
void free(void *p);
extern int cx_errno;
int cx_mach_pending(u32 port);
size_t strlen(const char *s);
int strncmp(const char *a, const char *b, size_t n);
char *strchr(const char *s, int c);
void *memset(void *d, int c, size_t n);
int mach_msg(void *msg, int option, u32 send_size, u32 rcv_size, u32 rcv_name, u32 timeout, u32 notify);

typedef struct { u64 ident; i16 filter; u16 flags; u32 fflags; i64 data; u64 udata; u64 ext[2]; } kevent64_s;
typedef struct { u64 ident; i16 filter; u16 flags; u32 fflags; i64 data; void *udata; } kevent_s;
typedef struct { long sec, nsec; } ts_t;

#define EVFILT_READ     (-1)
#define EVFILT_WRITE    (-2)
#define EVFILT_PROC     (-5)
#define NOTE_EXIT       0x80000000u
#define EVFILT_AIO      (-3)
#define EVFILT_VNODE    (-4)
#define EVFILT_SIGNAL   (-6)
#define EVFILT_TIMER    (-7)
#define EVFILT_MACHPORT (-8)
#define EVFILT_FS       (-9)
#define EVFILT_USER     (-10)
#define EVFILT_VM       (-12)
#define EVFILT_MEMORYSTATUS (-14)
#define EVFILT_EXCEPT   (-15)
#define NOTE_EXITSTATUS 0x04000000u
#define NOTE_FORK       0x40000000u
#define NOTE_EXEC       0x20000000u
#define NOTE_SIGNAL     0x08000000u
#define NOTE_ABSOLUTE   0x8u
#define NOTE_DELETE 0x1u
#define NOTE_WRITE  0x2u
#define NOTE_EXTEND 0x4u
#define NOTE_ATTRIB 0x8u
#define NOTE_LINK   0x10u
#define NOTE_RENAME 0x20u
#define NOTE_REVOKE 0x40u
#define NOTE_VM_PRESSURE 0x80000000u
#define VQ_MOUNT   0x8u
#define VQ_UNMOUNT 0x10u
#define EV_DISPATCH 0x80
#define EV_UDATA_SPECIFIC 0x100
#define EV_DISPATCH2 (EV_DISPATCH | EV_UDATA_SPECIFIC)
#define EV_EOF 0x8000
#define EV_ADD 0x1
#define EV_DELETE 0x2
#define EV_ENABLE 0x4
#define EV_DISABLE 0x8
#define EV_ONESHOT 0x10
#define EV_CLEAR 0x20
#define EV_RECEIPT 0x40
#define EV_ERROR 0x4000
#define NOTE_TRIGGER 0x01000000u
#define NOTE_FFCTRLMASK 0xc0000000u
#define NOTE_FFLAGSMASK 0x00ffffffu
#define NOTE_SECONDS  0x1u
#define NOTE_USECONDS 0x2u
#define NOTE_NSECONDS 0x4u
#define MACH_RCV_MSG 0x2
#define MACH_RCV_TIMEOUT 0x100

#define KQ_FD_BASE 0x4000
#define MAX_KQ 256
#define MAX_KN 1024

typedef struct {
    int used, enabled;
    kevent64_s ev;          /* como foi registrado */
    int triggered;          /* EVFILT_USER */
    u64 next_ms;            /* EVFILT_TIMER */
    u64 period_ms;
    u64 aux, aux2;          /* SIGNAL: contagem já vista; VNODE: inode e último evento; PROC: filhos/caminho; MEM: nível */
    char path[256];         /* VNODE: caminho do fd (pastas: filho criado/removido) */
} knote_t;
typedef struct { int used; knote_t kn[MAX_KN]; volatile int lock; int rr; int refs; int hi; } kq_t;   /* hi: knotes usados vão até aqui */   /* rr: onde a próxima varredura começa; refs: descritores (dup) */
static kq_t *kqs[MAX_KQ];

/* descritor pronto para leitura? pipes: bytes disponíveis ou a outra ponta fechou (SYS_FSTAT do
 * kernel: is_dir 2 = ponta de leitura); arquivos comuns: sempre. *avail = bytes, *eof = fim. */
/* sockets do libSystem (sockets.c, descritores 768-831): perguntam à conexão TCP do kernel */
int cx_sock_is(int fd) __attribute__((weak));
int cx_sock_readable(int fd, long *avail, int *eof) __attribute__((weak));
int cx_sock_writable(int fd) __attribute__((weak));
int cx_fd_writable(int fd) {
    if (fd >= 768 && cx_sock_is && cx_sock_is(fd)) return cx_sock_writable(fd);
    return 1;                                                    /* arquivos e pipes: sempre */
}
int cx_fd_readable(int fd, long *avail, int *eof) {
    mos_stat_t st;
    *avail = 0; *eof = 0;
    if (fd >= 768 && cx_sock_is && cx_sock_is(fd)) return cx_sock_readable(fd, avail, eof);
    if (fd < 0 || fd >= 0x4000 || SYS2(SYS_FSTAT, fd, &st) < 0) return 0;
    /* 2 = pipe de leitura; 6 = socket AF_UNIX (bytes prontos, ou conexões na fila do listen) */
    if (st.is_dir == 2 || st.is_dir == 6) { *avail = (long)st.size; *eof = st.mtime != 0; return st.size > 0 || st.mtime; }
    if (st.is_dir == 3) return 0;
    /* terminal (kernel/proc/pty.c): size = o que um read devolve agora; mtime = 1 se o read daria fim de arquivo */
    if (st.is_dir == 5) { *avail = (long)st.size; *eof = st.mtime != 0; return st.size > 0 || st.mtime; }
    *avail = (long)st.size;
    return 1;
}

/* EVFILT_PROC/NOTE_EXIT: nas plataformas Apple o libuv (Node/Electron) sabe que um filho terminou por aqui, não
 * por SIGCHLD; sem isto o child_process nunca dava 'exit' (o VS Code esgotava o prazo do shell de login e a busca
 * com o ripgrep não terminava). Pronto quando o kernel já não tem o processo como vivo (zumbi ou colhido): kill(pid, 0)
 * falha. Quem registrou colhe depois com waitpid(WNOHANG). */
static int proc_exited(u64 pid) { return SYS2(SYS_SIGKILL, (long)pid, 0) < 0; }

/* EVFILT_SIGNAL: libc.c chama cx_kq_signal_hit a cada entrega (cx_sigtramp) e cx_sig_watch liga o tratador interno */
static volatile int sig_count[32];
struct knote_s;
void cx_kq_signal_hit(int s) { if (s > 0 && s < 32) __sync_fetch_and_add(&sig_count[s], 1); }
void cx_sig_watch(int s, int on);

/* EVFILT_VNODE: o journal do VFS (SYS_FSMETA MOS_FSM_EVENTS) */
static u64 fs_last_id(void) { mos_fsmeta_t m; memset(&m, 0, sizeof m); m.fd = -1; long r = SYS2(SYS_FSMETA, MOS_FSM_EVENTID, &m); return r > 0 ? (u64)r : 0; }
static u64 fd_ino(int fd) {
    mos_fsmeta_t m; mos_statx_t x;
    memset(&m, 0, sizeof m); memset(&x, 0, sizeof x);
    m.fd = fd; m.buf = (u64)&x; m.size = sizeof x;
    return SYS2(SYS_FSMETA, MOS_FSM_STATX, &m) == 0 ? x.ino : 0;
}
static int parent_is(const char *child, const char *dir) {
    size_t n = strlen(dir);
    if (!n || strncmp(child, dir, n) || child[n] != '/') return 0;
    return !strchr(child + n + 1, '/');
}
/* eventos do journal desde k->aux2 para o inode k->aux; devolve as NOTE_* e avança */
static u32 vnode_notes(knote_t *k) {
    u64 last = fs_last_id();
    if (!last || last == k->aux2) return 0;
    static mos_fsevent_t ev[8];
    u32 notes = 0;
    for (int round = 0; round < 64 && k->aux2 < last; round++) {
        mos_fsmeta_t m;
        memset(&m, 0, sizeof m);
        m.fd = -1; m.pos = k->aux2; m.buf = (u64)ev; m.size = sizeof ev;
        long n = SYS2(SYS_FSMETA, MOS_FSM_EVENTS, &m);
        if (n <= 0) { k->aux2 = last; break; }
        for (long i = 0; i < n; i++) {
            const mos_fsevent_t *e = &ev[i];
            k->aux2 = e->id;
            if (e->flags & (FSEV_KERNEL_DROPPED | FSEV_USER_DROPPED)) { notes |= NOTE_ATTRIB; continue; }
            if (e->ino == k->aux) {
                if (e->flags & FSEV_REMOVED) notes |= NOTE_DELETE;
                if (e->flags & FSEV_RENAMED) notes |= NOTE_RENAME;
                if (e->flags & FSEV_MODIFIED) notes |= NOTE_WRITE | NOTE_EXTEND;
                if (e->flags & (FSEV_INODE_META | FSEV_OWNER | FSEV_XATTR | FSEV_FINDER)) notes |= NOTE_ATTRIB;
                if (e->flags & FSEV_UNMOUNT) notes |= NOTE_REVOKE;
            } else if (k->path[0] && parent_is(e->path, k->path) && (e->flags & (FSEV_CREATED | FSEV_REMOVED | FSEV_RENAMED))) {
                notes |= NOTE_WRITE;                      /* pasta: entrada nova, removida ou renomeada */
                if (e->flags & FSEV_IS_DIR) notes |= NOTE_LINK;
            }
        }
    }
    return notes & k->ev.fflags;
}
/* EVFILT_PROC: filhos vivos de pid (NOTE_FORK) e um hash do caminho (NOTE_EXEC) */
static u64 child_count(u64 pid) {
    int pids[512];
    long n = SYS3(SYS_DARWIN, MOS_DW_PIDS, pids, 512);
    u64 c = 0;
    for (long i = 0; i < n && i < 512; i++) {
        mos_dw_proc_t d;
        if (SYS3(SYS_DARWIN, MOS_DW_PROC, pids[i], &d) == 0 && (u64)d.ppid == pid) c++;
    }
    return c;
}
static u64 exec_id(u64 pid) {
    mos_dw_proc_t d;
    if (SYS3(SYS_DARWIN, MOS_DW_PROC, (long)pid, &d) != 0) return 0;
    u64 h = 1469598103934665603ull;
    for (const char *c = d.path; *c; c++) h = (h ^ (u8)*c) * 1099511628211ull;
    return h ^ d.start_unix_us;
}
static u32 mem_level(void) {
    mos_vmstats_t v;
    return SYS2(SYS_VMCTL, MOS_VM_STATS, &v) == 0 && v.pressure_level ? v.pressure_level : 1;
}

static void kl(kq_t *q) { while (__sync_lock_test_and_set(&q->lock, 1)) SYS0(SYS_YIELD); }
static void ku(kq_t *q) { __sync_lock_release(&q->lock); }
static u64 now_ms(void) { return (u64)SYS0(SYS_TIME); }

/* as vagas de kqs[] são tomadas com CAS e soltas com troca atômica: duas threads chamando kqueue() juntas (o
 * EnsureProcessTerminated do Chromium cria um por filho encerrado) achavam a mesma vaga livre e recebiam o mesmo
 * descritor, e o close de uma deixava a outra com uma fila solta */
int kqueue(void) {
    kq_t *q = calloc(1, sizeof *q);
    if (!q) { cx_errno = CX_ENOMEM; return -1; }
    q->used = 1;
    q->refs = 1;
    for (int i = 0; i < MAX_KQ; i++)
        if (!kqs[i] && __sync_bool_compare_and_swap(&kqs[i], (kq_t *)0, q)) return KQ_FD_BASE + i;
    free(q);
    cx_errno = CX_EMFILE;
    return -1;
}
/* close() de um kqueue (chamado por close em libc quando o fd é deste intervalo) */
static void knote_drop(knote_t *k);
int cx_kqueue_close(int fd) {
    int i = fd - KQ_FD_BASE;
    if (i < 0 || i >= MAX_KQ) return -1;
    kq_t *q = __sync_lock_test_and_set(&kqs[i], (kq_t *)0);
    if (!q) return -1;
    if (__sync_sub_and_fetch(&q->refs, 1) <= 0) {
        for (int k = 0; k < q->hi; k++) knote_drop(&q->kn[k]);   /* sinais vigiados voltam à ação do usuário */
        free(q);
    }
    return 0;
}
/* dup/F_DUPFD de um kqueue: outro descritor para a MESMA fila (o mio do tokio clona o Registry assim; sem isto o
 * dup caía no kernel, que não conhece estes descritores, e o runtime do tokio falhava com EBADF: o file_service.node
 * do Cursor entrava em pânico e derrubava o Extension Host). -1 se fd não é um kqueue; EMFILE sem vaga. */
int cx_kqueue_dup(int fd) {
    int i = fd - KQ_FD_BASE;
    kq_t *q = i >= 0 && i < MAX_KQ ? kqs[i] : 0;
    if (!q) return -1;
    __sync_add_and_fetch(&q->refs, 1);
    for (int j = 0; j < MAX_KQ; j++)
        if (!kqs[j] && __sync_bool_compare_and_swap(&kqs[j], (kq_t *)0, q)) return KQ_FD_BASE + j;
    if (__sync_sub_and_fetch(&q->refs, 1) <= 0) free(q);
    cx_errno = CX_EMFILE;
    return -2;
}

static u64 timer_ms(const kevent64_s *e) {
    i64 d = e->data < 0 ? 0 : e->data;
    if (e->fflags & NOTE_SECONDS) return (u64)d * 1000;
    if (e->fflags & NOTE_USECONDS) return (u64)d / 1000;
    if (e->fflags & NOTE_NSECONDS) return (u64)d / 1000000;
    return (u64)d;
}
static knote_t *find(kq_t *q, const kevent64_s *c) {
    for (int i = 0; i < q->hi; i++) {
        knote_t *k = &q->kn[i];
        if (!k->used || k->ev.ident != c->ident || k->ev.filter != c->filter) continue;
        if ((k->ev.flags & EV_UDATA_SPECIFIC) && k->ev.udata != c->udata) continue;   /* identidade inclui o udata */
        return k;
    }
    return 0;
}
static void knote_drop(knote_t *k) {
    if (k->used && k->ev.filter == EVFILT_SIGNAL) cx_sig_watch((int)k->ev.ident, 0);
    k->used = 0;
}
static int valid_fd(u64 fd) {
    mos_stat_t st;
    if ((int)fd >= 768 && cx_sock_is && cx_sock_is((int)fd)) return 1;
    if (fd >= KQ_FD_BASE && fd < KQ_FD_BASE + MAX_KQ) return kqs[fd - KQ_FD_BASE] != 0;
    return fd < 0x4000 && SYS2(SYS_FSTAT, (long)fd, &st) >= 0;
}

/* aplica uma mudança; devolve 0 ou um errno */
static int apply(kq_t *q, const kevent64_s *c) {
    knote_t *k = find(q, c);
    if (c->filter == EVFILT_MACHPORT && (c->flags & (EV_ADD | EV_DELETE))) {   /* diagnóstico (ca-log): portas vigiadas */
        extern int cx_ca_diag(void);
        extern int cx_fdprintf(int fd, const char *fmt, ...);
        static int n;
        if (cx_ca_diag() && ++n <= 60)
            cx_fdprintf(2, "[ca] kqueue %p: EVFILT_MACHPORT %s porta 0x%llx (fflags 0x%x, thread %ld)\n", (void *)q,
                        (c->flags & EV_DELETE) ? "tira" : "põe", (unsigned long long)c->ident, (unsigned)c->fflags, SYS0(SYS_GETTID));
    }
    if (c->flags & EV_DELETE) { if (!k) return CX_ENOENT; knote_drop(k); return 0; }
    if (c->flags & EV_ADD) {
        if (!k) {
            switch (c->filter) {                  /* o alvo tem de existir, como no XNU */
            case EVFILT_READ: case EVFILT_WRITE: case EVFILT_EXCEPT: case EVFILT_VNODE:
                if (!valid_fd(c->ident)) return CX_EBADF;
                break;
            case EVFILT_PROC: {
                mos_dw_proc_t d;
                long r = SYS3(SYS_DARWIN, MOS_DW_PROC, (long)c->ident, &d);
                if (r == -MOS_ESRCH && SYS3(SYS_DARWIN, MOS_DW_EXITSTATUS, (long)c->ident, 0) < 0) return CX_ESRCH;
                break;
            }
            case EVFILT_SIGNAL: if (c->ident == 0 || c->ident >= 32) return CX_EINVAL; break;
            case EVFILT_USER: case EVFILT_TIMER: case EVFILT_MACHPORT: case EVFILT_FS: case EVFILT_VM:
            case EVFILT_MEMORYSTATUS: break;
            default: return CX_EINVAL;            /* EVFILT_AIO e desconhecidos */
            }
            for (int i = 0; i < MAX_KN && !k; i++) if (!q->kn[i].used) k = &q->kn[i];
            if (!k) return CX_ENOMEM;
            if (k - q->kn >= q->hi) q->hi = (int)(k - q->kn) + 1;
            memset(k, 0, sizeof *k);
            k->used = 1;
            k->ev = *c;
            k->enabled = 1;
            switch (c->filter) {
            case EVFILT_SIGNAL: k->aux = (u64)sig_count[c->ident]; cx_sig_watch((int)c->ident, 1); break;
            case EVFILT_VNODE: {
                k->aux = fd_ino((int)c->ident);
                k->aux2 = fs_last_id();
                if (SYS3(SYS_FDPATH, (long)c->ident, k->path, sizeof k->path) < 0) k->path[0] = 0;
                break;
            }
            case EVFILT_PROC: k->aux = child_count(c->ident); k->aux2 = exec_id(c->ident); break;
            case EVFILT_FS: k->aux2 = fs_last_id(); break;
            case EVFILT_VM: case EVFILT_MEMORYSTATUS: k->aux = mem_level(); break;
            }
        } else {
            u32 keep = k->ev.fflags;
            k->ev = *c;
            if (c->filter == EVFILT_USER) k->ev.fflags = keep;
        }
        if (c->filter == EVFILT_TIMER) {
            k->period_ms = timer_ms(c);
            if (c->fflags & NOTE_ABSOLUTE) {      /* prazo absoluto no relógio do sistema (gettimeofday) */
                u64 unow = (u64)SYS0(SYS_UNIXTIME) * 1000, abs = k->period_ms;
                k->next_ms = now_ms() + (abs > unow ? abs - unow : 0);
                k->period_ms = 0;
            } else k->next_ms = now_ms() + k->period_ms;
        }
    }
    if (!k) return CX_ENOENT;
    if (c->flags & EV_ENABLE) k->enabled = 1;
    if (c->flags & EV_DISABLE) k->enabled = 0;
    if (c->filter == EVFILT_USER) {
        u32 ff = c->fflags & NOTE_FFLAGSMASK;
        switch (c->fflags & NOTE_FFCTRLMASK) {
        case 0x40000000u: k->ev.fflags &= ff; break;   /* NOTE_FFAND */
        case 0x80000000u: k->ev.fflags |= ff; break;   /* NOTE_FFOR */
        case 0xc0000000u: k->ev.fflags = ff; break;    /* NOTE_FFCOPY */
        }
        if (c->fflags & NOTE_TRIGGER) k->triggered = 1;
    }
    return 0;
}

long lseek(int fd, long off, int whence);
/* evento pronto? preenche out. peek = 1: só pergunta (não consome gatilho, timer, mensagem nem evento do journal) */
static int ready(knote_t *k, kevent64_s *out, int peek) {
    if (!k->used || !k->enabled) return 0;
    *out = k->ev;
    out->flags = k->ev.flags & (EV_ONESHOT | EV_CLEAR | EV_DISPATCH | EV_UDATA_SPECIFIC);
    switch (k->ev.filter) {
    case EVFILT_USER:
        if (!k->triggered) return 0;
        if (peek) return 1;
        out->fflags = k->ev.fflags & NOTE_FFLAGSMASK;
        if (k->ev.flags & EV_CLEAR) k->triggered = 0;
        return 1;
    case EVFILT_TIMER: {
        u64 t = now_ms();
        if (t < k->next_ms) return 0;
        if (peek) return 1;
        u64 n = 1;
        if (k->period_ms && t >= k->next_ms + k->period_ms) n += (t - k->next_ms) / k->period_ms;   /* disparos perdidos */
        out->data = (i64)n;
        if (k->period_ms && !(k->ev.flags & EV_ONESHOT)) k->next_ms = t + k->period_ms;
        else k->enabled = 0;
        return 1;
    }
    case EVFILT_MACHPORT: {
        int n = cx_mach_pending((u32)k->ev.ident);
        if (!n) return 0;
        if (peek) return 1;
        {   /* diagnóstico (ca-log): a 1ª vez que cada porta aparece pronta */
            extern int cx_ca_diag(void);
            extern int cx_fdprintf(int fd, const char *fmt, ...);
            static u32 seen[256]; static int nseen;
            if (cx_ca_diag()) {
                int s = 0;
                while (s < nseen && seen[s] != (u32)k->ev.ident) s++;
                if (s == nseen && nseen < 256) { seen[nseen++] = (u32)k->ev.ident; cx_fdprintf(2, "[ca] kevent64: porta 0x%x pronta (%d msg, fflags 0x%x, thread %ld)\n", (u32)k->ev.ident, n, (unsigned)k->ev.fflags, SYS0(SYS_GETTID)); }
            }
        }
        out->data = (i64)k->ev.ident;
        if ((k->ev.fflags & MACH_RCV_MSG) && k->ev.ext[0] && k->ev.ext[1]) {
            int r = mach_msg((void *)k->ev.ext[0], MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0, (u32)k->ev.ext[1], (u32)k->ev.ident, 0, 0);
            out->fflags = (u32)r;
            out->ext[1] = r == 0 ? *(u32 *)(k->ev.ext[0] + 4) : 0;   /* msgh_size */
        }
        return 1;
    }
    case EVFILT_WRITE: if (!cx_fd_writable((int)k->ev.ident)) return 0; out->data = 65536; return 1;
    case EVFILT_READ: {
        long avail; int eof;
        if (!cx_fd_readable((int)k->ev.ident, &avail, &eof)) return 0;
        if (!peek) {
            mos_stat_t st;   /* arquivo comum: o que falta ler a partir da posição atual (o XNU dá isso em data) */
            if (SYS2(SYS_FSTAT, (long)k->ev.ident, &st) >= 0 && st.is_dir == 0) {
                long pos = lseek((int)k->ev.ident, 0, 1);
                avail = pos >= 0 && (long)st.size > pos ? (long)st.size - pos : 0;
                if (!avail) eof = 1;
            }
        }
        out->data = avail;
        if (eof) out->flags |= EV_EOF;
        return 1;
    }
    case EVFILT_EXCEPT: return 0;                 /* sem dados fora de banda nos sockets do mOS */
    case EVFILT_SIGNAL: {
        int c = sig_count[k->ev.ident];
        if ((u64)c == k->aux) return 0;
        if (peek) return 1;
        out->data = (i64)((u32)c - (u32)k->aux);  /* quantas vezes desde a última entrega (EV_CLEAR implícito) */
        k->aux = (u64)c;
        return 1;
    }
    case EVFILT_PROC: {
        u32 want = k->ev.fflags, got = 0;
        int dead = proc_exited(k->ev.ident);
        if (dead && (want & NOTE_EXIT)) got |= NOTE_EXIT;
        if (!dead && (want & NOTE_FORK)) { u64 c = child_count(k->ev.ident); if (c > k->aux) got |= NOTE_FORK; if (!peek) k->aux = c; }
        if (!dead && (want & NOTE_EXEC)) { u64 e = exec_id(k->ev.ident); if (e && e != k->aux2) got |= NOTE_EXEC; if (!peek && e) k->aux2 = e; }
        if (!got) return 0;
        if (peek) return 1;
        out->fflags = got;
        out->data = 0;
        if (got & NOTE_EXIT) {
            if (want & NOTE_EXITSTATUS) {         /* status do wait(2), sem colher; o sinal no número do macOS */
                extern int cx_sig_from_mos(int);
                long st = SYS2(SYS_DARWIN, MOS_DW_EXITSTATUS, (long)k->ev.ident);
                if (st >= 0) {
                    if ((st & 0x7f) && (st & 0x7f) != 0x7f) st = (st & ~0x7fl) | cx_sig_from_mos((int)(st & 0x7f));
                    out->data = st;
                    out->fflags |= NOTE_EXITSTATUS;
                }
            }
            out->flags |= EV_EOF;                 /* como no macOS */
            k->enabled = 0;                       /* uma vez só (o libuv ainda pede EV_ONESHOT) */
        }
        return 1;
    }
    case EVFILT_VNODE: {
        if (peek) { if (fs_last_id() == k->aux2) return 0; knote_t tmp = *k; return vnode_notes(&tmp) != 0; }
        u32 n = vnode_notes(k);
        if (!n) return 0;
        out->fflags = n;
        if (n & (NOTE_DELETE | NOTE_REVOKE)) out->flags |= EV_EOF;
        return 1;
    }
    case EVFILT_FS: {
        u64 last = fs_last_id();
        if (last == k->aux2) return 0;
        static mos_fsevent_t ev[8];
        u32 got = 0;
        u64 from = k->aux2;
        for (int round = 0; round < 64 && from < last; round++) {
            mos_fsmeta_t m;
            memset(&m, 0, sizeof m);
            m.fd = -1; m.pos = from; m.buf = (u64)ev; m.size = sizeof ev;
            long n = SYS2(SYS_FSMETA, MOS_FSM_EVENTS, &m);
            if (n <= 0) { from = last; break; }
            for (long i = 0; i < n; i++) {
                from = ev[i].id;
                if (ev[i].flags & FSEV_MOUNT) got |= VQ_MOUNT;
                if (ev[i].flags & FSEV_UNMOUNT) got |= VQ_UNMOUNT;
            }
        }
        if (!peek) k->aux2 = from;
        if (!got) return 0;
        out->fflags = got;
        return 1;
    }
    case EVFILT_VM: case EVFILT_MEMORYSTATUS: {
        u32 lv = mem_level();
        if (lv == (u32)k->aux) return 0;
        if (k->ev.filter == EVFILT_VM && (lv >= 2) == (k->aux >= 2)) { if (!peek) k->aux = lv; return 0; }   /* VM: só ao entrar em pressão */
        u32 note = k->ev.filter == EVFILT_VM ? (lv >= 2 ? NOTE_VM_PRESSURE : 0) : lv;   /* MEMORYSTATUS: 1 normal, 2 aviso, 4 crítico */
        if (!(note & k->ev.fflags)) { if (!peek) k->aux = lv; return 0; }
        if (peek) return 1;
        k->aux = lv;
        out->fflags = note;
        return 1;
    }
    default: return 0;
    }
}

/* o kqueue "está legível" (poll/select sobre o fd dele) quando algum evento registrado está pronto.
 * Sem efeitos colaterais (não limpa gatilhos, não reprograma timers, não recebe mensagens Mach):
 * a thread de polling do Electron faz poll(uv_backend_fd) e só então acorda o laço do Node. */
int cx_kqueue_peek(int fd) {
    int i = fd - KQ_FD_BASE;
    if (i < 0 || i >= MAX_KQ || !kqs[i]) return -1;
    kq_t *q = kqs[i];
    int r = 0;
    kevent64_s tmp;
    kl(q);
    for (int k = 0; k < q->hi && !r; k++) r = ready(&q->kn[k], &tmp, 1);
    ku(q);
    return r;
}

/* flags do kevent64 (sys/event.h): IMMEDIATE = não espera; ERROR_EVENTS = só aplica as mudanças e devolve os erros,
 * sem esperar nem colher eventos (o waker do Bun registra a porta Mach assim; esperando, o Claude Code travava) */
#define KEVENT_FLAG_IMMEDIATE    0x1
#define KEVENT_FLAG_ERROR_EVENTS 0x2
int kevent64(int fd, const kevent64_s *changes, int nchanges, kevent64_s *events, int nevents, unsigned flags, const ts_t *timeout) {
    static const ts_t zero_ts;
    if (flags & KEVENT_FLAG_IMMEDIATE) timeout = &zero_ts;
    int i = fd - KQ_FD_BASE;
    if (i < 0 || i >= MAX_KQ || !kqs[i]) { cx_errno = CX_EBADF; return -1; }
    kq_t *q = kqs[i];
    int out = 0;
    kl(q);
    for (int c = 0; c < nchanges; c++) {
        int err = apply(q, &changes[c]);
        if ((err || (changes[c].flags & EV_RECEIPT)) && out < nevents) {
            events[out] = changes[c];
            events[out].flags = EV_ERROR;
            events[out].data = err;
            out++;
        }
    }
    ku(q);
    if (out || nevents == 0 || (flags & KEVENT_FLAG_ERROR_EVENTS)) return out;
    u64 deadline = timeout ? now_ms() + (u64)timeout->sec * 1000 + (u64)timeout->nsec / 1000000 : ~0ull;
    /* ainda é consulta periódica (o kernel não espera em várias portas e descritores de uma vez), mas o cochilo
     * dobra enquanto nada acontece (1, 2, 4, 8 ms) e nunca passa do próximo timer: com 1 ms fixo, as ~10 threads
     * de E/S do Chromium e do Node acordavam 10 mil vezes por segundo e tomavam a CPU de quem tinha trabalho */
    u32 nap = 1;
    for (;;) {
        u64 next_timer = ~0ull;
        kl(q);
        /* rodízio: a varredura começa depois do último entregue. Começando sempre do knote 0, um evento sempre pronto
         * (porta de nível alto que o dono não esvazia) enchia o buffer do chamador a cada volta e os de índice maior
         * nunca eram entregues */
        int start = q->rr, last = -1;
        int hi = q->hi ? q->hi : 1;
        if (start >= hi) start = 0;
        for (int j = 0; j < hi && out < nevents; j++) {
            int k = (start + j) % hi;
            knote_t *n = &q->kn[k];
            if (!ready(n, &events[out], 0)) {
                if (n->used && n->enabled && n->ev.filter == EVFILT_TIMER && n->next_ms < next_timer) next_timer = n->next_ms;
                continue;
            }
            if (n->ev.flags & EV_ONESHOT) knote_drop(n);
            else if (n->ev.flags & EV_DISPATCH) n->enabled = 0;   /* EV_DISPATCH: desliga até o próximo EV_ENABLE */
            out++;
            last = k;
        }
        if (last >= 0) q->rr = (last + 1) % hi;
        ku(q);
        if (out) return out;
        u64 now = now_ms();
        if (now >= deadline) return 0;
        u64 until = deadline < next_timer ? deadline : next_timer;
        u64 left = until > now ? until - now : 1;
        SYS1(SYS_SLEEP, nap < left ? nap : left);
        if (nap < 8) nap <<= 1;
    }
}

int kevent(int fd, const kevent_s *changes, int nchanges, kevent_s *events, int nevents, const ts_t *timeout) {
    kevent64_s c[64], e[64];
    if (nchanges > 64) nchanges = 64;
    if (nevents > 64) nevents = 64;
    for (int i = 0; i < nchanges; i++) {
        c[i].ident = changes[i].ident; c[i].filter = changes[i].filter; c[i].flags = changes[i].flags;
        c[i].fflags = changes[i].fflags; c[i].data = changes[i].data; c[i].udata = (u64)changes[i].udata;
        c[i].ext[0] = c[i].ext[1] = 0;
    }
    int n = kevent64(fd, c, nchanges, e, nevents, 0, timeout);
    for (int i = 0; i < n; i++) {
        events[i].ident = e[i].ident; events[i].filter = e[i].filter; events[i].flags = e[i].flags;
        events[i].fflags = e[i].fflags; events[i].data = e[i].data; events[i].udata = (void *)e[i].udata;
    }
    return n;
}
int kevent_qos(int fd, const kevent64_s *changes, int nchanges, kevent64_s *events, int nevents, void *data_out, size_t *data_avail, unsigned flags) {
    (void)data_out; (void)data_avail;
    return kevent64(fd, changes, nchanges, events, nevents, flags, 0);
}
