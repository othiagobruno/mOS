/*
 * mOS libSystem — traps Mach de tempo e de portas que faltavam: mk_timer_*, mach_wait_until, clock_sleep,
 * mach_reply_port, thread_get_special_reply_port, host/thread_self_trap, swtch, swtch_pri, mach_generate_activity_id.
 *
 * Contrato: o do XNU (osfmk/kern/mk_timer.c e clock_oldops.c), conferido com o que o Darling roda no
 * darlingserver. Teste: tools/test-machtraps.sh (userland/tests-machtraps).
 *
 * mk_timer: no XNU o kernel manda a mensagem de expiração. Aqui uma thread do libSystem ("mk_timer"), criada
 * no primeiro arm, dorme até o prazo mais próximo e envia a mensagem pela porta. Como no XNU:
 *   - a porta é um direito de RECEIVE comum; a mensagem tem 48 bytes (cabeçalho + uint64_t unused[3] zerados),
 *     msgh_id 0 e chega com msgh_local_port = a porta e msgh_remote_port nulo;
 *   - prazo no passado dispara na hora; um novo arm troca o prazo; cancel devolve o prazo armado (0 se não havia);
 *   - o XNU guarda uma só mensagem pré-alocada por timer: com uma expiração ainda na fila, a seguinte não entra;
 *   - nome que não existe é KERN_INVALID_NAME, sem RECEIVE é KERN_INVALID_RIGHT e RECEIVE que não é timer é
 *     KERN_INVALID_ARGUMENT.
 */
#include "sys.h"
#include "libc.h"

typedef u32 mach_port_t;
typedef int kern_return_t;

#define KERN_SUCCESS          0
#define KERN_INVALID_ARGUMENT 4
#define KERN_FAILURE          5
#define KERN_INVALID_NAME     15
#define KERN_INVALID_RIGHT    17
#define KERN_INVALID_VALUE    18
#define TASK_PORT             0x103u
#define MACH_PORT_RIGHT_RECEIVE 1
#define MACH_PORT_TYPE_RECEIVE  0x20000u
#define CLOCK_PORT_BASE       0x7E000000u      /* host_get_clock_service (vscode_libc.c): base | id */
#define SYSTEM_CLOCK          0
#define TIME_RELATIVE         1

kern_return_t mach_port_allocate(mach_port_t task, int right, mach_port_t *name);
kern_return_t mach_port_destroy(mach_port_t task, mach_port_t name);
kern_return_t mach_port_type(mach_port_t task, mach_port_t name, u32 *ptype);
kern_return_t mach_msg(void *msg, int option, u32 send_size, u32 rcv_size, mach_port_t rcv_name, u32 timeout, mach_port_t notify);
mach_port_t mach_host_self(void);
mach_port_t mach_thread_self(void);
u64 mach_absolute_time(void);
int pthread_create(unsigned long *t, const void *attr, void *(*fn)(void *), void *arg);
int cx_futex_wait(volatile int *w, int expected, long ms);
int cx_futex_wake(volatile int *w, int n);

/* ---- espera por prazo absoluto em unidades de mach_absolute_time (timebase 1/1: nanossegundos) */
static void sleep_until(u64 deadline) {
    for (;;) {
        u64 now = mach_absolute_time();
        if (now >= deadline) return;
        u64 ms = (deadline - now + 999999) / 1000000;
        SYS1(SYS_SLEEP, (long)(ms > 0x7FFFFFFF ? 0x7FFFFFFF : ms));
    }
}
kern_return_t mach_wait_until(u64 deadline) { sleep_until(deadline); return KERN_SUCCESS; }

/* ---- clock_sleep(clock, TIME_ABSOLUTE|TIME_RELATIVE, mach_timespec_t, *wakeup): o relógio é o SYSTEM_CLOCK
 * (tempo desde o boot). Porta nula também é o SYSTEM_CLOCK; o CALENDAR_CLOCK é KERN_FAILURE. A hora em que
 * acordou volta em *wakeup, menos nos erros de argumento. */
typedef struct { u32 sec; int nsec; } mach_ts_t;
static mach_ts_t ts_from_abs(u64 t) { mach_ts_t r = {(u32)(t / 1000000000ull), (int)(t % 1000000000ull)}; return r; }
kern_return_t clock_sleep_trap(mach_port_t clock, int type, int sec, int nsec, mach_ts_t *wakeup) {
    if (clock != 0 && (clock & 0xFFFFFF00u) != CLOCK_PORT_BASE) return KERN_INVALID_ARGUMENT;
    if (clock != 0 && (clock & 0xFF) != SYSTEM_CLOCK) return KERN_FAILURE;
    if (type & ~TIME_RELATIVE) return KERN_INVALID_VALUE;
    if (nsec < 0 || nsec >= 1000000000) return KERN_INVALID_VALUE;
    u64 now = mach_absolute_time();
    u64 want = (u64)(u32)sec * 1000000000ull + (u64)nsec;
    if (type & TIME_RELATIVE) want += now;
    sleep_until(want);
    if (wakeup) *wakeup = ts_from_abs(mach_absolute_time());
    return KERN_SUCCESS;
}
/* mach_timespec_t {unsigned sec; int nsec} passado por valor (cabe em um registrador) */
kern_return_t clock_sleep(mach_port_t clock, int type, u64 ts, mach_ts_t *wakeup) {
    return clock_sleep_trap(clock, type, (int)(u32)ts, (int)(u32)(ts >> 32), wakeup);
}

/* ---- portas */
mach_port_t host_self_trap(void) { return mach_host_self(); }
mach_port_t thread_self_trap(void) { return mach_thread_self(); }
/* um RECEIVE novo a cada chamada; MACH_PORT_NULL se não deu */
mach_port_t mach_reply_port(void) {
    mach_port_t p = 0;
    return mach_port_allocate(TASK_PORT, MACH_PORT_RIGHT_RECEIVE, &p) == KERN_SUCCESS ? p : 0;
}
/* o XNU troca a porta especial da thread por uma nova a cada chamada */
mach_port_t thread_get_special_reply_port(void) { return mach_reply_port(); }

/* swtch/swtch_pri: cedem a CPU. Voltam se ainda há outra thread pronta; o mOS não diz, então FALSE. */
int swtch(void) { SYS0(SYS_YIELD); return 0; }
int swtch_pri(int pri) { (void)pri; SYS0(SYS_YIELD); return 0; }

/* ids de atividade (os_activity): únicos no processo, em blocos de count */
kern_return_t mach_generate_activity_id(mach_port_t task, int count, u64 *id) {
    static u64 next = 1;
    if (task == 0 || count <= 0 || !id) return KERN_INVALID_ARGUMENT;
    *id = __atomic_fetch_add(&next, (u64)count, __ATOMIC_RELAXED);
    return KERN_SUCCESS;
}

/* ---- mk_timer */
#define MAX_TIMERS 512
typedef struct { mach_port_t port; int armed; u64 deadline; } mkt_t;
static mkt_t timers[MAX_TIMERS];
static volatile int mkt_lk;
static volatile int mkt_gen;          /* muda a cada arm/cancel/destroy: acorda a thread para recalcular */
static volatile long mkt_pid;         /* processo dono da thread (depois de fork o filho cria a dele) */

static void mkt_lock(void) {
    while (__sync_lock_test_and_set(&mkt_lk, 1))
        while (mkt_lk) SYS0(SYS_YIELD);
}
static void mkt_unlock(void) { __sync_lock_release(&mkt_lk); }
static mkt_t *mkt_find(mach_port_t name) {
    if (!name) return 0;
    for (int i = 0; i < MAX_TIMERS; i++) if (timers[i].port == name) return &timers[i];
    return 0;
}
/* o erro do XNU para um nome que não é timer */
static kern_return_t not_timer(mach_port_t name) {
    u32 type = 0;
    /* task, host e threads são direitos de SEND com nome fixo no libSystem (mach.c) */
    if (name == TASK_PORT || name == 0x203u || (name >= 0x1000u && name < 0x20000u)) return KERN_INVALID_RIGHT;
    if (!name || mach_port_type(TASK_PORT, name, &type) != KERN_SUCCESS) return KERN_INVALID_NAME;
    return (type & MACH_PORT_TYPE_RECEIVE) ? KERN_INVALID_ARGUMENT : KERN_INVALID_RIGHT;
}
static void mkt_kick(void) { __atomic_add_fetch(&mkt_gen, 1, __ATOMIC_RELEASE); cx_futex_wake(&mkt_gen, 1); }

typedef struct { u32 bits, size; mach_port_t remote, local; u32 voucher; int id; u64 unused[3]; } __attribute__((packed, aligned(4))) mkt_msg_t;
_Static_assert(sizeof(mkt_msg_t) == 48, "mk_timer_expire_msg_t tem 48 bytes");
#define MACH_SEND_MSG      1
#define MACH_SEND_TIMEOUT  0x10
#define MT_MAKE_SEND       20

static int queued_msgs(mach_port_t port) {
    u64 a[8] = {port, 0, 0, 0, 0, 0, 0, 0};
    if (cx_syscall(SYS_MACH_PORT, MACH_OP_STATUS, (long)a, 0, 0, 0) != KERN_SUCCESS) return -1;
    return (int)a[1];
}
static void mkt_fire(mach_port_t port) {
    if (queued_msgs(port) != 0) return;      /* já há uma expiração na fila (ou a porta morreu) */
    mkt_msg_t m = {MT_MAKE_SEND, sizeof m, port, 0, 0, 0, {0, 0, 0}};
    mach_msg(&m, MACH_SEND_MSG | MACH_SEND_TIMEOUT, sizeof m, 0, 0, 0, 0);
}
static void *mkt_loop(void *arg) {
    (void)arg;
    mach_port_t due[MAX_TIMERS];
    for (;;) {
        int gen = __atomic_load_n(&mkt_gen, __ATOMIC_ACQUIRE), n = 0;
        u64 next = 0;
        mkt_lock();
        u64 now = mach_absolute_time();
        for (int i = 0; i < MAX_TIMERS; i++) {
            mkt_t *t = &timers[i];
            if (!t->port || !t->armed) continue;
            if (t->deadline <= now) { t->armed = 0; due[n++] = t->port; }
            else if (!next || t->deadline < next) next = t->deadline;
        }
        mkt_unlock();
        for (int i = 0; i < n; i++) mkt_fire(due[i]);
        if (n) continue;
        long ms = next ? (long)((next - now + 999999) / 1000000) : -1;
        cx_futex_wait(&mkt_gen, gen, ms);
    }
    return 0;
}
static void mkt_start(void) {
    long pid = SYS0(SYS_GETPID);
    if (__atomic_load_n(&mkt_pid, __ATOMIC_ACQUIRE) == pid) return;
    mkt_lock();
    if (mkt_pid != pid) {
        unsigned long th;
        if (pthread_create(&th, 0, mkt_loop, 0) == 0) __atomic_store_n(&mkt_pid, pid, __ATOMIC_RELEASE);
    }
    mkt_unlock();
}

mach_port_t mk_timer_create(void) {
    mach_port_t p = mach_reply_port();
    if (!p) return 0;
    mkt_lock();
    for (int i = 0; i < MAX_TIMERS; i++)
        if (!timers[i].port) { timers[i].port = p; timers[i].armed = 0; timers[i].deadline = 0; mkt_unlock(); return p; }
    mkt_unlock();
    mach_port_destroy(TASK_PORT, p);
    return 0;
}
kern_return_t mk_timer_arm_leeway(mach_port_t name, u64 flags, u64 expire, u64 leeway) {
    (void)flags; (void)leeway;
    mkt_lock();
    mkt_t *t = mkt_find(name);
    if (!t) { mkt_unlock(); return not_timer(name); }
    t->armed = 1;
    t->deadline = expire;
    mkt_unlock();
    mkt_start();
    mkt_kick();
    return KERN_SUCCESS;
}
kern_return_t mk_timer_arm(mach_port_t name, u64 expire) { return mk_timer_arm_leeway(name, 0, expire, 0); }
kern_return_t mk_timer_cancel(mach_port_t name, u64 *result_time) {
    mkt_lock();
    mkt_t *t = mkt_find(name);
    if (!t) { mkt_unlock(); return not_timer(name); }
    u64 armed = t->armed ? t->deadline : 0;
    t->armed = 0;
    mkt_unlock();
    mkt_kick();
    if (result_time) *result_time = armed;
    return KERN_SUCCESS;
}
kern_return_t mk_timer_destroy(mach_port_t name) {
    mkt_lock();
    mkt_t *t = mkt_find(name);
    if (!t) { mkt_unlock(); return not_timer(name); }
    t->port = 0;
    t->armed = 0;
    mkt_unlock();
    mkt_kick();
    return mach_port_destroy(TASK_PORT, name);
}
/* nomes das traps no libsystem_kernel */
mach_port_t mk_timer_create_trap(void) { return mk_timer_create(); }
kern_return_t mk_timer_arm_trap(mach_port_t n, u64 t) { return mk_timer_arm(n, t); }
kern_return_t mk_timer_arm_leeway_trap(mach_port_t n, u64 f, u64 t, u64 l) { return mk_timer_arm_leeway(n, f, t, l); }
kern_return_t mk_timer_cancel_trap(mach_port_t n, u64 *r) { return mk_timer_cancel(n, r); }
kern_return_t mk_timer_destroy_trap(mach_port_t n) { return mk_timer_destroy(n); }
kern_return_t mach_wait_until_trap(u64 deadline) { return mach_wait_until(deadline); }
mach_port_t mach_reply_port_trap(void) { return mach_reply_port(); }
