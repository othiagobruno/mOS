/*
 * mOS libSystem — kqueue/kevent dentro do processo, para o porte do VS Code: a thread de E/S do
 * Chromium (MessagePumpKqueue) espera em kevent64 por portas Mach (EVFILT_MACHPORT), gatilhos
 * (EVFILT_USER) e timers.
 *
 * Filtros: USER (NOTE_TRIGGER), MACHPORT (filas do mach_msg de mach.c; com MACH_RCV_MSG a mensagem
 * já vem no buffer de ext[0]/ext[1]), TIMER (ms, us, ns ou s; repetitivo), WRITE (sempre pronto).
 * READ em descritores ainda nunca sinaliza: o kernel do mOS não tem poll.
 * Os descritores de kqueue são números a partir de KQ_FD_BASE (não passam pelo kernel).
 */
#include "sys.h"

void *calloc(size_t n, size_t s);
void free(void *p);
extern int cx_errno;
int cx_mach_pending(u32 port);
int mach_msg(void *msg, int option, u32 send_size, u32 rcv_size, u32 rcv_name, u32 timeout, u32 notify);

typedef struct { u64 ident; i16 filter; u16 flags; u32 fflags; i64 data; u64 udata; u64 ext[2]; } kevent64_s;
typedef struct { u64 ident; i16 filter; u16 flags; u32 fflags; i64 data; void *udata; } kevent_s;
typedef struct { long sec, nsec; } ts_t;

#define EVFILT_READ     (-1)
#define EVFILT_WRITE    (-2)
#define EVFILT_PROC     (-5)
#define NOTE_EXIT       0x80000000u
#define EVFILT_TIMER    (-7)
#define EVFILT_MACHPORT (-8)
#define EVFILT_USER     (-10)
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
#define MAX_KQ 32
#define MAX_KN 128

typedef struct {
    int used, enabled;
    kevent64_s ev;          /* como foi registrado */
    int triggered;          /* EVFILT_USER */
    u64 next_ms;            /* EVFILT_TIMER */
    u64 period_ms;
} knote_t;
typedef struct { int used; knote_t kn[MAX_KN]; volatile int lock; int rr; int refs; } kq_t;   /* rr: onde a próxima varredura começa; refs: descritores (dup) */
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
    if (st.is_dir == 2) { *avail = (long)st.size; *eof = st.mtime != 0; return st.size > 0 || st.mtime; }
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
int cx_kqueue_close(int fd) {
    int i = fd - KQ_FD_BASE;
    if (i < 0 || i >= MAX_KQ) return -1;
    kq_t *q = __sync_lock_test_and_set(&kqs[i], (kq_t *)0);
    if (!q) return -1;
    if (__sync_sub_and_fetch(&q->refs, 1) <= 0) free(q);
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
static knote_t *find(kq_t *q, u64 ident, i16 filter) {
    for (int i = 0; i < MAX_KN; i++) if (q->kn[i].used && q->kn[i].ev.ident == ident && q->kn[i].ev.filter == filter) return &q->kn[i];
    return 0;
}

/* aplica uma mudança; devolve 0 ou um errno */
static int apply(kq_t *q, const kevent64_s *c) {
    knote_t *k = find(q, c->ident, c->filter);
    if (c->filter == EVFILT_MACHPORT && (c->flags & (EV_ADD | EV_DELETE))) {   /* diagnóstico (ca-log): portas vigiadas */
        extern int cx_ca_diag(void);
        extern int cx_fdprintf(int fd, const char *fmt, ...);
        static int n;
        if (cx_ca_diag() && ++n <= 60)
            cx_fdprintf(2, "[ca] kqueue %p: EVFILT_MACHPORT %s porta 0x%llx (fflags 0x%x, thread %ld)\n", (void *)q,
                        (c->flags & EV_DELETE) ? "tira" : "põe", (unsigned long long)c->ident, (unsigned)c->fflags, SYS0(SYS_GETTID));
    }
    if (c->flags & EV_DELETE) { if (!k) return CX_ENOENT; k->used = 0; return 0; }
    if (c->flags & EV_ADD) {
        if (!k) {
            for (int i = 0; i < MAX_KN && !k; i++) if (!q->kn[i].used) k = &q->kn[i];
            if (!k) return CX_ENOMEM;
            k->used = 1;
            k->triggered = 0;
            k->ev = *c;
            k->enabled = 1;
        } else {
            u32 keep = k->ev.fflags;
            k->ev = *c;
            if (c->filter == EVFILT_USER) k->ev.fflags = keep;
        }
        if (c->filter == EVFILT_TIMER) { k->period_ms = timer_ms(c); k->next_ms = now_ms() + k->period_ms; }
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

/* evento pronto? preenche out */
static int ready(knote_t *k, kevent64_s *out) {
    if (!k->used || !k->enabled) return 0;
    *out = k->ev;
    out->flags = k->ev.flags & (EV_ONESHOT | EV_CLEAR);
    switch (k->ev.filter) {
    case EVFILT_USER:
        if (!k->triggered) return 0;
        out->fflags = k->ev.fflags & NOTE_FFLAGSMASK;
        if (k->ev.flags & EV_CLEAR) k->triggered = 0;
        return 1;
    case EVFILT_TIMER: {
        u64 t = now_ms();
        if (t < k->next_ms) return 0;
        out->data = 1;
        if (k->period_ms && !(k->ev.flags & EV_ONESHOT)) k->next_ms = t + k->period_ms;
        else k->enabled = 0;
        return 1;
    }
    case EVFILT_MACHPORT: {
        int n = cx_mach_pending((u32)k->ev.ident);
        {   /* diagnóstico (ca-log): a 1ª vez que cada porta aparece pronta (a porta do canal Mojo do renderer ficava com
             * mensagens paradas mesmo vigiada) */
            extern int cx_ca_diag(void);
            extern int cx_fdprintf(int fd, const char *fmt, ...);
            static u32 seen[256]; static int nseen;
            if (n && cx_ca_diag()) {
                int s = 0;
                while (s < nseen && seen[s] != (u32)k->ev.ident) s++;
                if (s == nseen && nseen < 256) { seen[nseen++] = (u32)k->ev.ident; cx_fdprintf(2, "[ca] kevent64: porta 0x%x pronta (%d msg, fflags 0x%x, thread %ld)\n", (u32)k->ev.ident, n, (unsigned)k->ev.fflags, SYS0(SYS_GETTID)); }
            }
        }
        if (!n) return 0;
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
        out->data = avail;
        if (eof) out->flags |= 0x8000;                   /* EV_EOF */
        return 1;
    }
    case EVFILT_PROC:
        if (!(k->ev.fflags & NOTE_EXIT) || !proc_exited(k->ev.ident)) return 0;
        out->fflags = NOTE_EXIT;
        out->flags |= 0x8000;                            /* EV_EOF, como no macOS */
        out->data = 0;
        k->enabled = 0;                                  /* uma vez só (o libuv ainda pede EV_ONESHOT) */
        return 1;
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
    kl(q);
    for (int k = 0; k < MAX_KN && !r; k++) {
        knote_t *n = &q->kn[k];
        if (!n->used || !n->enabled) continue;
        long avail; int eof;
        switch (n->ev.filter) {
        case EVFILT_USER: r = n->triggered; break;
        case EVFILT_TIMER: r = now_ms() >= n->next_ms; break;
        case EVFILT_MACHPORT: r = cx_mach_pending((u32)n->ev.ident) > 0; break;
        case EVFILT_WRITE: r = cx_fd_writable((int)n->ev.ident); break;
        case EVFILT_READ: r = cx_fd_readable((int)n->ev.ident, &avail, &eof); break;
        case EVFILT_PROC: r = (n->ev.fflags & NOTE_EXIT) && proc_exited(n->ev.ident); break;
        }
    }
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
        for (int j = 0; j < MAX_KN && out < nevents; j++) {
            int k = (start + j) % MAX_KN;
            knote_t *n = &q->kn[k];
            if (!ready(n, &events[out])) {
                if (n->used && n->enabled && n->ev.filter == EVFILT_TIMER && n->next_ms < next_timer) next_timer = n->next_ms;
                continue;
            }
            if (n->ev.flags & EV_ONESHOT) n->used = 0;
            out++;
            last = k;
        }
        if (last >= 0) q->rr = (last + 1) % MAX_KN;
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
