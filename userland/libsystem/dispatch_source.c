/*
 * mOS libSystem — fontes da libdispatch (dispatch_source_*).
 *
 * Tipos: DATA_ADD, DATA_OR, DATA_REPLACE (merge_data), TIMER (timer de dispatch.c), READ, WRITE, PROC, SIGNAL,
 * VNODE, MACH_RECV, MACH_SEND e MEMORYPRESSURE (nível do kernel, SYS_VMCTL). As que dependem de estado do sistema (fd, processo, sinal, arquivo,
 * porta) são vigiadas por uma thread monitora: enquanto a condição vale, a fonte recebe o evento (READ/WRITE são de
 * nível, como no kqueue). O evento acumula em `pending` (soma, OU ou troca, conforme o tipo) e um único item leva o
 * handler à fila de destino; dentro dele, dispatch_source_get_data devolve o valor acumulado.
 * Ciclo de vida como no macOS: criada inativa; dispatch_activate/dispatch_resume liga (registration handler primeiro);
 * dispatch_source_cancel para de entregar e roda o cancel handler uma vez, depois que nenhum handler está em curso.
 */
#include "dispatch_plat.h"

typedef void (*dispatch_function_t)(void *);
extern char OS_dispatch_source_c[] __asm__("OBJC_CLASS_$_OS_dispatch_source");

/* símbolos dos tipos (DISPATCH_SOURCE_TYPE_X = &_dispatch_source_type_x) */
enum { ST_DATA_ADD = 1, ST_DATA_OR, ST_DATA_REPLACE, ST_MACH_SEND, ST_MACH_RECV, ST_MEMORYPRESSURE, ST_PROC, ST_READ,
       ST_SIGNAL, ST_TIMER, ST_VNODE, ST_WRITE };
typedef struct { long kind; long reserved; } dp_stype_t;
const dp_stype_t _dispatch_source_type_data_add = {ST_DATA_ADD, 0};
const dp_stype_t _dispatch_source_type_data_or = {ST_DATA_OR, 0};
const dp_stype_t _dispatch_source_type_data_replace = {ST_DATA_REPLACE, 0};
const dp_stype_t _dispatch_source_type_mach_send = {ST_MACH_SEND, 0};
const dp_stype_t _dispatch_source_type_mach_recv = {ST_MACH_RECV, 0};
const dp_stype_t _dispatch_source_type_memorypressure = {ST_MEMORYPRESSURE, 0};
const dp_stype_t _dispatch_source_type_proc = {ST_PROC, 0};
const dp_stype_t _dispatch_source_type_read = {ST_READ, 0};
const dp_stype_t _dispatch_source_type_signal = {ST_SIGNAL, 0};
const dp_stype_t _dispatch_source_type_timer = {ST_TIMER, 0};
const dp_stype_t _dispatch_source_type_vnode = {ST_VNODE, 0};
const dp_stype_t _dispatch_source_type_write = {ST_WRITE, 0};

#define PROC_EXIT 0x80000000ul
#define VN_DELETE 0x1ul
#define VN_WRITE 0x2ul
#define VN_EXTEND 0x4ul
#define VN_ATTRIB 0x8ul
#define VN_LINK 0x10ul
#define VN_RENAME 0x20ul
#define VN_REVOKE 0x40ul
#define VN_FUNLOCK 0x100ul
#define MACH_SEND_DEAD 0x1ul
#define TIMER_STRICT 0x1ul

/* ------------------------------------------------------------------ plataforma: fd, processo, arquivo */
typedef struct { u64 ino; u32 mode, nlink, uid, gid; u64 size, mtime_ns; int ok; char path[256]; } dp_vstat_t;
#ifdef MOS_HOST_TEST
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
static int fd_readable(int fd, long *avail) {
    struct pollfd p = {fd, POLLIN, 0};
    if (poll(&p, 1, 0) <= 0 || !(p.revents & (POLLIN | POLLHUP))) return 0;
    int n = 0;
    if (ioctl(fd, FIONREAD, &n) < 0) n = 0;
    *avail = n;
    return 1;
}
static int fd_writable(int fd) { struct pollfd p = {fd, POLLOUT, 0}; return poll(&p, 1, 0) > 0 && (p.revents & POLLOUT); }
static int proc_gone(long pid) { return kill((int)pid, 0) < 0 && errno == ESRCH; }
static int mach_pending(u32 port) { (void)port; return 0; }
static void vstat(int fd, dp_vstat_t *v) {
    struct stat st;
    v->ok = fstat(fd, &st) == 0;
    if (!v->ok) return;
    v->ino = st.st_ino; v->mode = st.st_mode; v->nlink = (u32)st.st_nlink; v->uid = st.st_uid; v->gid = st.st_gid;
#if defined(__APPLE__)                         /* host macOS (tools/build-mac.sh): st_mtimespec e F_GETPATH */
    v->size = (u64)st.st_size; v->mtime_ns = (u64)st.st_mtimespec.tv_sec * 1000000000ull + (u64)st.st_mtimespec.tv_nsec;
    char full[1024];
    if (fcntl(fd, F_GETPATH, full) == 0) { strncpy(v->path, full, sizeof v->path - 1); v->path[sizeof v->path - 1] = 0; }
    else v->path[0] = 0;
#else
    v->size = (u64)st.st_size; v->mtime_ns = (u64)st.st_mtim.tv_sec * 1000000000ull + (u64)st.st_mtim.tv_nsec;
    char lp[64];
    snprintf(lp, sizeof lp, "/proc/self/fd/%d", fd);
    long n = readlink(lp, v->path, sizeof v->path - 1);
    v->path[n > 0 ? n : 0] = 0;
#endif
}
typedef void (*dp_sig_t)(int);
static dp_sig_t sig_install(int s, dp_sig_t h) { return signal(s, h); }
static unsigned mem_pressure(void) { return 1; }   /* o host não informa pressão: sempre normal */
#else
int cx_fd_readable(int fd, long *avail, int *eof);   /* kqueue.c */
int cx_fd_writable(int fd);
int cx_mach_pending(u32 port);                        /* mach.c */
int kill(int pid, int s);
int fstat(int fd, void *st);
int fcntl(int fd, int cmd, ...);
void *signal(int s, void *h);
static int fd_readable(int fd, long *avail) { int eof = 0; *avail = 0; return cx_fd_readable(fd, avail, &eof); }
static int fd_writable(int fd) { return cx_fd_writable(fd); }
static int proc_gone(long pid) { return kill((int)pid, 0) < 0; }
static int mach_pending(u32 port) { return cx_mach_pending(port) > 0; }
/* struct stat do macOS x86_64 (64 bits de inode) */
typedef struct {
    int st_dev; unsigned short st_mode, st_nlink; u64 st_ino; u32 st_uid, st_gid; int st_rdev, pad;
    long at_s, at_ns, mt_s, mt_ns, ct_s, ct_ns, bt_s, bt_ns;
    long long st_size, st_blocks; int st_blksize; u32 st_flags, st_gen; int lspare; long long qspare[2];
} dp_mstat_t;
static void vstat(int fd, dp_vstat_t *v) {
    dp_mstat_t st;
    v->ok = fstat(fd, &st) == 0;
    if (!v->ok) return;
    v->ino = st.st_ino; v->mode = st.st_mode; v->nlink = st.st_nlink; v->uid = st.st_uid; v->gid = st.st_gid;
    v->size = (u64)st.st_size; v->mtime_ns = (u64)st.mt_s * 1000000000ull + (u64)st.mt_ns;
    char p[1024];
    if (fcntl(fd, 50 /* F_GETPATH */, p) == 0) { size_t n = strlen(p); if (n >= sizeof v->path) n = sizeof v->path - 1; memcpy(v->path, p, n); v->path[n] = 0; }
    else v->path[0] = 0;
}
typedef void (*dp_sig_t)(int);
static dp_sig_t sig_install(int s, dp_sig_t h) { return (dp_sig_t)signal(s, (void *)h); }
/* nível de pressão de memória do kernel (SYS_VMCTL/MOS_VM_STATS): 1 normal, 2 aviso, 4 crítico (DISPATCH_MEMORYPRESSURE_*) */
static unsigned mem_pressure(void) {
    mos_vmstats_t st;
    memset(&st, 0, sizeof st);
    if (SYS2(SYS_VMCTL, MOS_VM_STATS, &st) < 0 || !st.pressure_level) return 1;
    return st.pressure_level;
}
#endif

/* ------------------------------------------------------------------ objeto fonte */
typedef struct dp_source {
    dp_obj_t h;
    int type;
    unsigned long handle, mask;
    dp_lock_t lock;
    void *ev_blk, *cancel_blk, *reg_blk;
    dispatch_function_t ev_fn, cancel_fn, reg_fn;
    unsigned long pending, data;
    int armed;                         /* item do handler a caminho da fila */
    volatile int canceled;
    int cancel_sent, started, in_monitor;
    u64 t_next, t_interval, t_leeway;
    int t_gen;
    dp_timer_t *t_entry;
    dp_vstat_t vs;
    struct dp_source *mnext;
} dp_source_t;

static int is_src(void *s) { return cx_dispatch_is_obj(s) && ((dp_obj_t *)s)->kind == DK_SOURCE; }
static void src_dispose(dp_obj_t *h) {
    dp_source_t *s = (dp_source_t *)h;
    if (s->ev_blk) _Block_release(s->ev_blk);
    if (s->cancel_blk) _Block_release(s->cancel_blk);
    if (s->reg_blk) _Block_release(s->reg_blk);
}
static void monitor_add(dp_source_t *s);
static void monitor_remove(dp_source_t *s);
static void timer_arm_locked(dp_source_t *s);

static void call_handler(dp_source_t *s, void *blk, dispatch_function_t fn) {
    if (blk) DP_RUN_BLOCK(blk);
    else if (fn) fn(s->h.ctx);
}
/* cancel handler: uma vez, depois que o último handler em curso terminou */
static void src_cancel_run(void *p) {
    dp_source_t *s = p;
    dp_lock(&s->lock);
    void *b = s->cancel_blk; dispatch_function_t f = s->cancel_fn;
    s->cancel_blk = 0; s->cancel_fn = 0;
    void *eb = s->ev_blk, *rb = s->reg_blk;
    s->ev_blk = 0; s->ev_fn = 0; s->reg_blk = 0; s->reg_fn = 0;
    dp_unlock(&s->lock);
    if (eb) _Block_release(eb);
    if (rb) _Block_release(rb);
    call_handler(s, b, f);
    if (b) _Block_release(b);
    cx_dispatch_release(s);
}
static void src_send_cancel_locked(dp_source_t *s) {
    if (s->cancel_sent || s->armed || !s->started) return;
    s->cancel_sent = 1;
    cx_dispatch_retain(s);
    dispatch_async_f(s->h.tq, s, src_cancel_run);
}
static void src_deliver(void *p) {
    dp_source_t *s = p;
    dp_lock(&s->lock);
    s->data = s->pending;
    s->pending = 0;
    int cancel = s->canceled;
    void *b = s->ev_blk; dispatch_function_t f = s->ev_fn;
    dp_unlock(&s->lock);
    if (!cancel && (b || f)) call_handler(s, b, f);
    dp_lock(&s->lock);
    s->armed = 0;
    if (s->canceled) src_send_cancel_locked(s);
    else if (s->pending && !s->h.susp && !s->h.inactive) {
        s->armed = 1;
        cx_dispatch_retain(s);
        dp_unlock(&s->lock);
        dispatch_async_f(s->h.tq, s, src_deliver);
        cx_dispatch_release(s);
        return;
    }
    dp_unlock(&s->lock);
    cx_dispatch_release(s);
}
static void arm_locked(dp_source_t *s) {
    if (s->armed || !s->pending || s->canceled || s->h.susp || s->h.inactive) return;
    s->armed = 1;
    cx_dispatch_retain(s);
    dispatch_async_f(s->h.tq, s, src_deliver);
}
/* junta um evento: op 0 soma, 1 OU, 2 troca */
static void src_merge(dp_source_t *s, unsigned long v, int op) {
    dp_lock(&s->lock);
    if (!s->canceled) {
        if (op == 0) s->pending += v;
        else if (op == 1) s->pending |= v;
        else s->pending = v;
        arm_locked(s);
    }
    dp_unlock(&s->lock);
}

void *dispatch_source_create(const void *type, unsigned long handle, unsigned long mask, void *queue) {
    if (!type) return 0;
    int t = (int)((const dp_stype_t *)type)->kind;
    if (t < ST_DATA_ADD || t > ST_WRITE) return 0;
    switch (t) {
    case ST_DATA_ADD: case ST_DATA_OR: case ST_DATA_REPLACE: case ST_MEMORYPRESSURE: if (handle) return 0; break;
    case ST_SIGNAL: if (handle == 0 || handle >= 65 || mask) return 0; break;
    case ST_READ: case ST_WRITE: if ((long)handle < 0 || mask) return 0; break;
    case ST_TIMER: if (handle || (mask & ~3ul)) return 0; break;
    default: break;
    }
    dp_source_t *s = calloc(1, sizeof *s);
    if (!s) return 0;
    dp_obj_init(s, OS_dispatch_source_c, DK_SOURCE, src_dispose);
    s->type = t; s->handle = handle; s->mask = mask;
    s->h.inactive = 1;                                     /* fontes nascem inativas, como no macOS */
    void *tq = queue ? queue : dp_default_target();
    cx_dispatch_retain(tq);
    s->h.tq = tq;
    s->t_next = ~0ull;
    return s;
}

static void reg_run(void *p) {
    dp_source_t *s = p;
    dp_lock(&s->lock);
    void *b = s->reg_blk; dispatch_function_t f = s->reg_fn;
    s->reg_blk = 0; s->reg_fn = 0;
    int c = s->canceled;
    dp_unlock(&s->lock);
    if (!c) call_handler(s, b, f);
    if (b) _Block_release(b);
    cx_dispatch_release(s);
}
/* chamado por dispatch.c quando a fonte é ativada ou volta da suspensão */
void cx_dispatch_source_resumed(void *p) {
    dp_source_t *s = p;
    dp_lock(&s->lock);
    if (s->h.inactive) { dp_unlock(&s->lock); return; }
    if (!s->started) {
        s->started = 1;
        if (s->reg_blk || s->reg_fn) { cx_dispatch_retain(s); dispatch_async_f(s->h.tq, s, reg_run); }
        if (s->canceled) { src_send_cancel_locked(s); dp_unlock(&s->lock); return; }
        if (s->type == ST_TIMER) timer_arm_locked(s);
        dp_unlock(&s->lock);
        if (s->type != ST_TIMER && s->type > ST_DATA_REPLACE) monitor_add(s);
        return;
    }
    if (!s->h.susp) arm_locked(s);
    dp_unlock(&s->lock);
}

static void set_blk(dp_source_t *s, void **slot, dispatch_function_t *fslot, void *blk, dispatch_function_t fn) {
    void *nb = blk ? _Block_copy(blk) : 0;
    dp_lock(&s->lock);
    void *old = *slot;
    *slot = nb;
    *fslot = fn;
    dp_unlock(&s->lock);
    if (old) _Block_release(old);
}
void dispatch_source_set_event_handler(void *s, void *b) { if (is_src(s)) set_blk(s, &((dp_source_t *)s)->ev_blk, &((dp_source_t *)s)->ev_fn, b, 0); }
void dispatch_source_set_event_handler_f(void *s, dispatch_function_t f) { if (is_src(s)) set_blk(s, &((dp_source_t *)s)->ev_blk, &((dp_source_t *)s)->ev_fn, 0, f); }
void dispatch_source_set_cancel_handler(void *s, void *b) { if (is_src(s)) set_blk(s, &((dp_source_t *)s)->cancel_blk, &((dp_source_t *)s)->cancel_fn, b, 0); }
void dispatch_source_set_cancel_handler_f(void *s, dispatch_function_t f) { if (is_src(s)) set_blk(s, &((dp_source_t *)s)->cancel_blk, &((dp_source_t *)s)->cancel_fn, 0, f); }
void dispatch_source_set_mandatory_cancel_handler(void *s, void *b) { dispatch_source_set_cancel_handler(s, b); }
void dispatch_source_set_mandatory_cancel_handler_f(void *s, dispatch_function_t f) { dispatch_source_set_cancel_handler_f(s, f); }
void dispatch_source_set_registration_handler(void *s, void *b) { if (is_src(s)) set_blk(s, &((dp_source_t *)s)->reg_blk, &((dp_source_t *)s)->reg_fn, b, 0); }
void dispatch_source_set_registration_handler_f(void *s, dispatch_function_t f) { if (is_src(s)) set_blk(s, &((dp_source_t *)s)->reg_blk, &((dp_source_t *)s)->reg_fn, 0, f); }

void dispatch_source_cancel(void *p) {
    if (!is_src(p)) return;
    dp_source_t *s = p;
    dp_lock(&s->lock);
    if (s->canceled) { dp_unlock(&s->lock); return; }
    s->canceled = 1;
    s->pending = 0;
    s->t_gen++;
    dp_timer_t *te = s->t_entry;
    s->t_entry = 0;
    src_send_cancel_locked(s);
    dp_unlock(&s->lock);
    if (te && dp_timer_cancel(te)) cx_dispatch_release(s);
    monitor_remove(s);
}
long dispatch_source_testcancel(void *s) { return is_src(s) ? ((dp_source_t *)s)->canceled != 0 : 0; }
unsigned long dispatch_source_get_handle(void *s) { return is_src(s) ? ((dp_source_t *)s)->handle : 0; }
unsigned long dispatch_source_get_mask(void *s) { return is_src(s) ? ((dp_source_t *)s)->mask : 0; }
unsigned long dispatch_source_get_data(void *s) { return is_src(s) ? ((dp_source_t *)s)->data : 0; }
/* struct dispatch_source_extended_data_s {data, status}: copia até size bytes e devolve quantos. status só tem
 * valor nas fontes que a Apple preenche pelo kevent (código de saída em DISPATCH_PROC_EXIT); o mOS não espia o
 * código de saída sem colher o processo (waitid sem WNOWAIT), então ele sai 0, como numa fonte sem status. */
size_t dispatch_source_get_extended_data(void *s, void *out, size_t size) {
    unsigned long d[2] = {is_src(s) ? ((dp_source_t *)s)->data : 0, 0};
    if (!out) return 0;
    if (size > sizeof d) size = sizeof d;
    memcpy(out, d, size);
    return size;
}
void dispatch_source_merge_data(void *p, unsigned long v) {
    if (!is_src(p)) return;
    dp_source_t *s = p;
    if (s->type == ST_DATA_ADD) { if (v) src_merge(s, v, 0); }
    else if (s->type == ST_DATA_OR) { if (v) src_merge(s, v, 1); }
    else if (s->type == ST_DATA_REPLACE) { if (v) src_merge(s, v, 2); }
}

/* ------------------------------------------------------------------ timer */
static void timer_cb(void *p);
typedef struct { dp_source_t *s; int gen; } dp_tref_t;
static void timer_arm_locked(dp_source_t *s) {
    if (s->t_next == ~0ull || s->canceled || !s->started) return;
    dp_tref_t *r = malloc(sizeof *r);
    if (!r) abort();
    r->s = s; r->gen = s->t_gen;
    cx_dispatch_retain(s);
    s->t_entry = dp_timer_add(s->t_next, timer_cb, r);
}
static void timer_cb(void *p) {
    dp_tref_t *r = p;
    dp_source_t *s = r->s;
    int gen = r->gen;
    free(r);
    dp_lock(&s->lock);
    if (gen != s->t_gen || s->canceled) { dp_unlock(&s->lock); cx_dispatch_release(s); return; }
    s->t_entry = 0;
    unsigned long fires = 1;
    u64 now = dp_now_ns();
    if (s->t_interval && s->t_interval != ~0ull) {
        u64 next = s->t_next + s->t_interval;
        if (next <= now) {                               /* atrasou: os disparos perdidos entram na contagem */
            u64 late = (now - s->t_next) / s->t_interval;
            fires += (unsigned long)late;
            next = s->t_next + (late + 1) * s->t_interval;
        }
        s->t_next = next;
    } else s->t_next = ~0ull;                            /* uma vez só */
    s->pending += fires;
    arm_locked(s);
    timer_arm_locked(s);
    dp_unlock(&s->lock);
    cx_dispatch_release(s);
}
void dispatch_source_set_timer(void *p, u64 start, u64 interval, u64 leeway) {
    if (!is_src(p) || ((dp_source_t *)p)->type != ST_TIMER) return;
    dp_source_t *s = p;
    dp_lock(&s->lock);
    s->t_gen++;
    dp_timer_t *te = s->t_entry;
    s->t_entry = 0;
    s->t_next = start == ~0ull ? ~0ull : cx_dispatch_deadline_ns(start);
    if (interval == 0) interval = 1;                     /* intervalo 0 vira o mínimo, como o macOS */
    s->t_interval = interval;
    s->t_leeway = leeway;
    s->pending = 0;
    timer_arm_locked(s);
    dp_unlock(&s->lock);
    if (te && dp_timer_cancel(te)) cx_dispatch_release(s);
}

/* ------------------------------------------------------------------ sinais */
static volatile int sig_count[65];
static dp_sig_t sig_prev[65];
static volatile int sig_hooked[65];
static void sig_handler(int sig) {
    if (sig > 0 && sig < 65) __sync_fetch_and_add(&sig_count[sig], 1);
    dp_sig_t p = sig > 0 && sig < 65 ? sig_prev[sig] : 0;
    if (p && p != (dp_sig_t)0 && p != (dp_sig_t)1 && p != sig_handler) p(sig);   /* SIG_DFL 0, SIG_IGN 1 */
}
static void sig_hook(int sig) {
    if (__sync_lock_test_and_set(&sig_hooked[sig], 1)) return;
    sig_prev[sig] = sig_install(sig, sig_handler);
}

/* ------------------------------------------------------------------ monitor */
static dp_source_t *mon_head;
static dp_lock_t mon_lock;
static volatile int mon_seq, mon_started;
static void *monitor_main(void *a) {
    (void)a;
    int seen[65] = {0};
    long nap = 2;
    for (;;) {
        int hit = 0;
        dp_lock(&mon_lock);
        for (dp_source_t *s = mon_head; s; s = s->mnext) {
            if (s->canceled) continue;
            long avail = 0;
            switch (s->type) {
            case ST_READ:
                if (!s->armed && !s->h.susp && fd_readable((int)s->handle, &avail)) { src_merge(s, (unsigned long)avail, 2); hit = 1; }
                break;
            case ST_WRITE:
                if (!s->armed && !s->h.susp && fd_writable((int)s->handle)) { src_merge(s, 65536, 2); hit = 1; }
                break;
            case ST_PROC:
                if ((s->mask & PROC_EXIT) && !s->vs.ok && proc_gone((long)s->handle)) { s->vs.ok = 1; src_merge(s, PROC_EXIT, 1); hit = 1; }
                break;
            case ST_MACH_RECV:
                if (!s->armed && !s->h.susp && mach_pending((u32)s->handle)) { src_merge(s, 1, 2); hit = 1; }
                break;
            case ST_SIGNAL: {
                int c = sig_count[s->handle], d = c - (int)s->t_leeway;
                if (d > 0) { s->t_leeway = (u64)c; src_merge(s, (unsigned long)d, 0); hit = 1; }
                break;
            }
            case ST_VNODE: {
                dp_vstat_t n;
                vstat((int)s->handle, &n);
                unsigned long ev = 0;
                if (!n.ok) ev |= VN_REVOKE;
                else if (s->vs.ok) {
                    if (n.nlink == 0 && s->vs.nlink != 0) ev |= VN_DELETE;
                    if (n.mtime_ns != s->vs.mtime_ns || n.size != s->vs.size) ev |= VN_WRITE;
                    if (n.size > s->vs.size) ev |= VN_EXTEND;
                    if (n.mode != s->vs.mode || n.uid != s->vs.uid || n.gid != s->vs.gid) ev |= VN_ATTRIB;
                    if (n.nlink != s->vs.nlink) ev |= VN_LINK;
                    if (n.path[0] && s->vs.path[0] && strcmp(n.path, s->vs.path)) ev |= VN_RENAME;
                }
                if (n.ok || s->vs.ok) s->vs = n;
                ev &= s->mask;
                if (ev) { src_merge(s, ev, 1); hit = 1; }
                break;
            }
            case ST_MEMORYPRESSURE: {                  /* a cada ~1 s: dispara quando o nível muda para um que a máscara pede */
                u64 now = dp_now_ns();
                if (now < s->t_next) break;
                s->t_next = now + 1000000000ull;
                unsigned lv = mem_pressure();
                if (lv != (unsigned)s->t_interval) {
                    s->t_interval = lv;
                    if (lv & s->mask) { src_merge(s, lv, 2); hit = 1; }
                }
                break;
            }
            default: break;
            }
        }
        int any = mon_head != 0;
        int seq = mon_seq;
        dp_unlock(&mon_lock);
        (void)seen;
        nap = hit ? 1 : (nap < 20 ? nap + 1 : 20);
        if (!any) dp_wait(&mon_seq, seq, -1);
        else dp_wait(&mon_seq, seq, nap);
    }
    return 0;
}
static void monitor_add(dp_source_t *s) {
    if (s->type == ST_SIGNAL) { s->t_leeway = (u64)sig_count[s->handle]; sig_hook((int)s->handle); }
    if (s->type == ST_VNODE) vstat((int)s->handle, &s->vs);
    if (s->type == ST_PROC) s->vs.ok = 0;
    if (s->type == ST_MEMORYPRESSURE) { s->t_next = 0; s->t_interval = 1; }   /* começa em normal: o aviso vem na mudança */
    cx_dispatch_retain(s);
    dp_lock(&mon_lock);
    s->in_monitor = 1;
    s->mnext = mon_head;
    mon_head = s;
    int first = !mon_started;
    mon_started = 1;
    dp_unlock(&mon_lock);
    if (first && dp_thread(monitor_main, 0) != 0) abort();
    __sync_fetch_and_add(&mon_seq, 1);
    dp_wake(&mon_seq, 1);
}
static void monitor_remove(dp_source_t *s) {
    int found = 0;
    dp_lock(&mon_lock);
    for (dp_source_t **pp = &mon_head; *pp; pp = &(*pp)->mnext)
        if (*pp == s) { *pp = s->mnext; found = 1; break; }
    s->in_monitor = 0;
    dp_unlock(&mon_lock);
    if (found) cx_dispatch_release(s);
}
