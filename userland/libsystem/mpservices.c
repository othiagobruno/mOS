/*
 * mOS libSystem — Multiprocessing Services do Carbon (CarbonCore/Multiprocessing.h) sobre pthreads.
 * Tarefas (MPCreateTask: thread com a pilha pedida; no fim, o resultado vai para a fila de aviso com os dois
 * parâmetros de término), filas de mensagem de 3 ponteiros, semáforos com valor máximo, regiões críticas
 * reentrantes, eventos (bits com OR) e o resto que os apps chamam (processadores, ID da tarefa atual, alocação).
 * Duration: 0 = não espera, 0x7FFFFFFF = sempre, positivo = milissegundos, negativo = microssegundos.
 * MPTerminateTask pede o cancelamento (efeito no próximo ponto de cancelamento, como o pthread_cancel do mOS) e já
 * avisa a fila com o motivo, como o Mac faz.
 */
#include "sys.h"
#include "libc.h"

typedef int OSStatus;
typedef i32 Duration;
typedef OSStatus (*TaskProc)(void *param);
int pthread_create(unsigned long *t, const void *attr, void *(*fn)(void *), void *arg);
int pthread_detach(unsigned long t);
int pthread_cancel(unsigned long t);
void pthread_exit(void *r) __attribute__((noreturn));
unsigned long pthread_self(void);
int pthread_attr_init(void *a);
int pthread_attr_setstacksize(void *a, size_t s);
int pthread_mutex_init(void *m, const void *a);
int pthread_mutex_lock(void *m);
int pthread_mutex_unlock(void *m);
int pthread_cond_init(void *c, const void *a);
int pthread_cond_wait(void *c, void *m);
int pthread_cond_broadcast(void *c);
int pthread_cond_signal(void *c);
int pthread_cond_timedwait_relative_np(void *c, void *m, const void *ts);
int pthread_key_create(unsigned long *k, void (*d)(void *));
int pthread_setspecific(unsigned long k, const void *v);
void *pthread_getspecific(unsigned long k);
int sched_yield(void);
int gettimeofday(void *tv, void *tz);
long sysconf(int name);

enum { noErr = 0, paramErr = -50, kMPDeletedErr = -29295, kMPTimeoutErr = -29296, kMPTaskAbortedErr = -29297,
       kMPInsufficientResourcesErr = -29298, kMPInvalidIDErr = -29299 };
#define kDurationForever 0x7FFFFFFF
#define MAGIC_Q 0x4d505175u
#define MAGIC_S 0x4d505365u
#define MAGIC_C 0x4d504372u
#define MAGIC_E 0x4d504576u
#define MAGIC_T 0x4d505461u

typedef struct { u8 m[64]; u8 c[48]; } sync_t;                                 /* pthread_mutex_t + pthread_cond_t do macOS */
static void sync_init(sync_t *s) { pthread_mutex_init(s->m, 0); pthread_cond_init(s->c, 0); }
/* espera na condição até cond() ou o prazo; 0 = ok, kMPTimeoutErr, kMPDeletedErr (*dead) */
typedef int (*ready_fn)(void *o);
static OSStatus wait_on(sync_t *s, Duration d, ready_fn ready, void *o, volatile int *dead) {
    if (ready(o)) return noErr;
    if (d == 0) return kMPTimeoutErr;
    long left_us = d == kDurationForever ? -1 : d > 0 ? (long)d * 1000 : -(long)d;
    while (!ready(o)) {
        if (*dead) return kMPDeletedErr;
        if (left_us < 0) { pthread_cond_wait(s->c, s->m); continue; }
        if (left_us == 0) return kMPTimeoutErr;
        struct { long sec, nsec; } ts = {left_us / 1000000, (left_us % 1000000) * 1000};
        struct { long sec; int usec; } a, b;
        gettimeofday(&a, 0);
        pthread_cond_timedwait_relative_np(s->c, s->m, &ts);
        gettimeofday(&b, 0);
        long spent = (b.sec - a.sec) * 1000000 + (b.usec - a.usec);
        left_us = spent >= left_us ? 0 : left_us - (spent > 0 ? spent : 0);
    }
    return *dead ? kMPDeletedErr : noErr;
}

/* IDs vivos: um ID qualquer devolve kMPInvalidIDErr em vez de ser desreferenciado */
static u8 g_idlock[64];
static int g_idlock_ok;
static void **g_ids;
static int g_nids, g_capids;
static void ids_lock(void) { if (!__atomic_exchange_n(&g_idlock_ok, 1, __ATOMIC_SEQ_CST)) pthread_mutex_init(g_idlock, 0); pthread_mutex_lock(g_idlock); }
static int id_add(void *p) {
    ids_lock();
    if (g_nids == g_capids) {
        int c = g_capids ? g_capids * 2 : 64;
        void **n = realloc(g_ids, (size_t)c * sizeof *n);
        if (!n) { pthread_mutex_unlock(g_idlock); return -1; }
        g_ids = n; g_capids = c;
    }
    g_ids[g_nids++] = p;
    pthread_mutex_unlock(g_idlock);
    return 0;
}
static void id_del(void *p) {
    ids_lock();
    for (int i = 0; i < g_nids; i++) if (g_ids[i] == p) { g_ids[i] = g_ids[--g_nids]; break; }
    pthread_mutex_unlock(g_idlock);
}
static int id_ok(void *p, u32 magic) {
    if (!p) return 0;
    ids_lock();
    int ok = 0;
    for (int i = 0; i < g_nids && !ok; i++) ok = g_ids[i] == p;
    pthread_mutex_unlock(g_idlock);
    return ok && *(u32 *)p == magic;
}

/* ================================================================ filas */
typedef struct { void *p[3]; } msg_t;
typedef struct { u32 magic; sync_t s; msg_t *m; int head, n, cap, reserved; volatile int dead, waiters; } queue_t;
static queue_t *q_of(void *id) { return id_ok(id, MAGIC_Q) ? id : 0; }
OSStatus MPCreateQueue(void **out) {
    if (!out) return paramErr;
    queue_t *q = calloc(1, sizeof *q);
    if (!q) return kMPInsufficientResourcesErr;
    q->magic = MAGIC_Q;
    id_add(q);
    sync_init(&q->s);
    *out = q;
    return noErr;
}
OSStatus MPDeleteQueue(void *id) {
    queue_t *q = q_of(id);
    if (!q) return kMPInvalidIDErr;
    pthread_mutex_lock(q->s.m);
    q->dead = 1;
    q->magic = 0;
    id_del(q);
    pthread_cond_broadcast(q->s.c);
    int w = q->waiters;
    pthread_mutex_unlock(q->s.m);
    if (!w) { free(q->m); free(q); }                                             /* com quem espera: o último solta */
    return noErr;
}
OSStatus MPSetQueueReserve(void *id, u32 count) {
    queue_t *q = q_of(id);
    if (!q) return kMPInvalidIDErr;
    q->reserved = (int)count;
    return noErr;
}
OSStatus MPNotifyQueue(void *id, void *p1, void *p2, void *p3) {
    queue_t *q = q_of(id);
    if (!q) return kMPInvalidIDErr;
    pthread_mutex_lock(q->s.m);
    if (q->n == q->cap) {
        int nc = q->cap ? q->cap * 2 : 16;
        msg_t *nm = malloc((size_t)nc * sizeof *nm);
        if (!nm) { pthread_mutex_unlock(q->s.m); return kMPInsufficientResourcesErr; }
        for (int i = 0; i < q->n; i++) nm[i] = q->m[(q->head + i) % q->cap];
        free(q->m);
        q->m = nm; q->cap = nc; q->head = 0;
    }
    msg_t *m = &q->m[(q->head + q->n) % q->cap];
    m->p[0] = p1; m->p[1] = p2; m->p[2] = p3;
    q->n++;
    pthread_cond_signal(q->s.c);
    pthread_mutex_unlock(q->s.m);
    return noErr;
}
static int q_ready(void *o) { return ((queue_t *)o)->n > 0; }
OSStatus MPWaitOnQueue(void *id, void **p1, void **p2, void **p3, Duration d) {
    queue_t *q = q_of(id);
    if (!q) return kMPInvalidIDErr;
    pthread_mutex_lock(q->s.m);
    q->waiters++;
    OSStatus st = wait_on(&q->s, d, q_ready, q, &q->dead);
    if (!st) {
        msg_t m = q->m[q->head];
        q->head = (q->head + 1) % q->cap;
        q->n--;
        if (p1) *p1 = m.p[0];
        if (p2) *p2 = m.p[1];
        if (p3) *p3 = m.p[2];
    }
    q->waiters--;
    int freeit = q->dead && !q->waiters;
    pthread_mutex_unlock(q->s.m);
    if (freeit) { free(q->m); free(q); }
    return st;
}

/* ================================================================ semáforos */
typedef struct { u32 magic; sync_t s; u32 max, value; volatile int dead, waiters; } sem_t_;
static sem_t_ *s_of(void *id) { return id_ok(id, MAGIC_S) ? id : 0; }
OSStatus MPCreateSemaphore(u32 max, u32 initial, void **out) {
    if (!out || initial > max) return paramErr;
    sem_t_ *s = calloc(1, sizeof *s);
    if (!s) return kMPInsufficientResourcesErr;
    s->magic = MAGIC_S;
    id_add(s); s->max = max; s->value = initial;
    sync_init(&s->s);
    *out = s;
    return noErr;
}
OSStatus MPDeleteSemaphore(void *id) {
    sem_t_ *s = s_of(id);
    if (!s) return kMPInvalidIDErr;
    pthread_mutex_lock(s->s.m);
    s->dead = 1; s->magic = 0;
    id_del(s);
    pthread_cond_broadcast(s->s.c);
    int w = s->waiters;
    pthread_mutex_unlock(s->s.m);
    if (!w) free(s);
    return noErr;
}
OSStatus MPSignalSemaphore(void *id) {
    sem_t_ *s = s_of(id);
    if (!s) return kMPInvalidIDErr;
    pthread_mutex_lock(s->s.m);
    OSStatus st = noErr;
    if (s->value >= s->max) st = kMPInsufficientResourcesErr;
    else { s->value++; pthread_cond_signal(s->s.c); }
    pthread_mutex_unlock(s->s.m);
    return st;
}
static int s_ready(void *o) { return ((sem_t_ *)o)->value > 0; }
OSStatus MPWaitOnSemaphore(void *id, Duration d) {
    sem_t_ *s = s_of(id);
    if (!s) return kMPInvalidIDErr;
    pthread_mutex_lock(s->s.m);
    s->waiters++;
    OSStatus st = wait_on(&s->s, d, s_ready, s, &s->dead);
    if (!st) s->value--;
    s->waiters--;
    int freeit = s->dead && !s->waiters;
    pthread_mutex_unlock(s->s.m);
    if (freeit) free(s);
    return st;
}

/* ================================================================ regiões críticas (reentrantes na mesma tarefa) */
typedef struct { u32 magic; sync_t s; unsigned long owner; u32 depth; volatile int dead, waiters; } crit_t;
static crit_t *c_of(void *id) { return id_ok(id, MAGIC_C) ? id : 0; }
OSStatus MPCreateCriticalRegion(void **out) {
    if (!out) return paramErr;
    crit_t *c = calloc(1, sizeof *c);
    if (!c) return kMPInsufficientResourcesErr;
    c->magic = MAGIC_C;
    id_add(c);
    sync_init(&c->s);
    *out = c;
    return noErr;
}
OSStatus MPDeleteCriticalRegion(void *id) {
    crit_t *c = c_of(id);
    if (!c) return kMPInvalidIDErr;
    pthread_mutex_lock(c->s.m);
    c->dead = 1; c->magic = 0;
    id_del(c);
    pthread_cond_broadcast(c->s.c);
    int w = c->waiters;
    pthread_mutex_unlock(c->s.m);
    if (!w) free(c);
    return noErr;
}
typedef struct { crit_t *c; unsigned long me; } cwait_t;
static int c_ready(void *o) { cwait_t *w = o; return w->c->depth == 0 || w->c->owner == w->me; }
OSStatus MPEnterCriticalRegion(void *id, Duration d) {
    crit_t *c = c_of(id);
    if (!c) return kMPInvalidIDErr;
    unsigned long me = pthread_self();
    pthread_mutex_lock(c->s.m);
    if (c->depth && c->owner == me) { c->depth++; pthread_mutex_unlock(c->s.m); return noErr; }
    c->waiters++;
    cwait_t w = {c, me};
    OSStatus st = wait_on(&c->s, d, c_ready, &w, &c->dead);
    if (!st) { c->owner = me; c->depth = 1; }
    c->waiters--;
    int freeit = c->dead && !c->waiters;
    pthread_mutex_unlock(c->s.m);
    if (freeit) free(c);
    return st;
}
OSStatus MPExitCriticalRegion(void *id) {
    crit_t *c = c_of(id);
    if (!c) return kMPInvalidIDErr;
    pthread_mutex_lock(c->s.m);
    OSStatus st = noErr;
    if (!c->depth || c->owner != pthread_self()) st = kMPInsufficientResourcesErr;
    else if (--c->depth == 0) { c->owner = 0; pthread_cond_signal(c->s.c); }
    pthread_mutex_unlock(c->s.m);
    return st;
}

/* ================================================================ eventos (bits; esperar devolve e zera) */
typedef struct { u32 magic; sync_t s; u32 flags; volatile int dead, waiters; } event_t;
static event_t *e_of(void *id) { return id_ok(id, MAGIC_E) ? id : 0; }
OSStatus MPCreateEvent(void **out) {
    if (!out) return paramErr;
    event_t *e = calloc(1, sizeof *e);
    if (!e) return kMPInsufficientResourcesErr;
    e->magic = MAGIC_E;
    id_add(e);
    sync_init(&e->s);
    *out = e;
    return noErr;
}
OSStatus MPDeleteEvent(void *id) {
    event_t *e = e_of(id);
    if (!e) return kMPInvalidIDErr;
    pthread_mutex_lock(e->s.m);
    e->dead = 1; e->magic = 0;
    id_del(e);
    pthread_cond_broadcast(e->s.c);
    int w = e->waiters;
    pthread_mutex_unlock(e->s.m);
    if (!w) free(e);
    return noErr;
}
OSStatus MPSetEvent(void *id, u32 flags) {
    event_t *e = e_of(id);
    if (!e) return kMPInvalidIDErr;
    pthread_mutex_lock(e->s.m);
    e->flags |= flags;
    pthread_cond_broadcast(e->s.c);
    pthread_mutex_unlock(e->s.m);
    return noErr;
}
static int e_ready(void *o) { return ((event_t *)o)->flags != 0; }
OSStatus MPWaitForEvent(void *id, u32 *flags, Duration d) {
    event_t *e = e_of(id);
    if (!e) return kMPInvalidIDErr;
    pthread_mutex_lock(e->s.m);
    e->waiters++;
    OSStatus st = wait_on(&e->s, d, e_ready, e, &e->dead);
    if (!st) { if (flags) *flags = e->flags; e->flags = 0; }
    e->waiters--;
    int freeit = e->dead && !e->waiters;
    pthread_mutex_unlock(e->s.m);
    if (freeit) free(e);
    return st;
}

/* ================================================================ tarefas */
typedef struct task {
    u32 magic;
    TaskProc entry;
    void *param, *t1, *t2;
    queue_t *notify;
    unsigned long thread;
    u32 weight;
    volatile int notified;
    void *storage[16];
} task_t;
static unsigned long g_key;
static int g_key_ok;
static task_t g_main_task = {MAGIC_T, 0, 0, 0, 0, 0, 0, 100, 0, {0}};
static void notify_end(task_t *t, OSStatus result) {
    if (__atomic_exchange_n(&t->notified, 1, __ATOMIC_SEQ_CST)) return;
    if (t->notify && q_of(t->notify)) MPNotifyQueue(t->notify, t->t1, t->t2, (void *)(long)result);
}
static void *task_main(void *arg) {
    task_t *t = arg;
    pthread_setspecific(g_key, t);
    OSStatus r = t->entry(t->param);
    notify_end(t, r);
    return 0;
}
OSStatus MPCreateTask(TaskProc entry, void *param, size_t stack, void *notifyQueue, void *t1, void *t2, u32 options, void **out) {
    (void)options;
    if (!entry || !out) return paramErr;
    if (notifyQueue && !q_of(notifyQueue)) return kMPInvalidIDErr;
    if (!g_key_ok) { pthread_key_create(&g_key, 0); g_key_ok = 1; }
    task_t *t = calloc(1, sizeof *t);
    if (!t) return kMPInsufficientResourcesErr;
    id_add(t);
    t->magic = MAGIC_T; t->entry = entry; t->param = param; t->t1 = t1; t->t2 = t2; t->notify = notifyQueue; t->weight = 100;
    u8 attr[64] __attribute__((aligned(16)));
    pthread_attr_init(attr);
    if (stack) pthread_attr_setstacksize(attr, stack < 65536 ? 65536 : stack);
    if (pthread_create(&t->thread, attr, task_main, t)) { free(t); return kMPInsufficientResourcesErr; }
    pthread_detach(t->thread);
    *out = t;
    return noErr;
}
static task_t *t_of(void *id) { return id_ok(id, MAGIC_T) ? id : 0; }
void *MPCurrentTaskID(void) {
    task_t *t = g_key_ok ? pthread_getspecific(g_key) : 0;
    static int main_reg;
    if (!t && !__atomic_exchange_n(&main_reg, 1, __ATOMIC_SEQ_CST)) id_add(&g_main_task);
    return t ? t : &g_main_task;
}
OSStatus MPTerminateTask(void *id, OSStatus reason) {
    task_t *t = t_of(id);
    if (!t || t == &g_main_task) return kMPInvalidIDErr;
    notify_end(t, reason);
    if (t == MPCurrentTaskID()) pthread_exit(0);
    pthread_cancel(t->thread);
    return noErr;
}
void MPExit(OSStatus status) {
    task_t *t = MPCurrentTaskID();
    if (t != &g_main_task) notify_end(t, status);
    pthread_exit(0);
}
OSStatus MPSetTaskWeight(void *id, u32 weight) {
    task_t *t = t_of(id);
    if (!t) return kMPInvalidIDErr;
    t->weight = weight;                                                          /* o escalonador do mOS não usa pesos */
    return noErr;
}
void MPYield(void) { sched_yield(); }
u8 MPTaskIsPreemptive(void *id) { (void)id; return 1; }
u32 MPProcessors(void) { long n = sysconf(58); return n > 0 ? (u32)n : 1; }      /* _SC_NPROCESSORS_ONLN */
u32 MPProcessorsScheduled(void) { return MPProcessors(); }
u8 _MPIsFullyInitialized(void) { return 1; }
OSStatus MPGetNextCpuID(void *group, void **cpu) { (void)group; (void)cpu; return kMPInvalidIDErr; }

/* armazenamento por tarefa (índices) */
static u32 g_tls_next;
OSStatus MPAllocateTaskStorageIndex(u32 *index) {
    if (!index) return paramErr;
    u32 i = __atomic_fetch_add(&g_tls_next, 1, __ATOMIC_SEQ_CST);
    if (i >= 16) return kMPInsufficientResourcesErr;
    *index = i;
    return noErr;
}
OSStatus MPDeallocateTaskStorageIndex(u32 index) { return index < 16 ? noErr : paramErr; }
OSStatus MPSetTaskStorageValue(u32 index, void *v) {
    if (index >= 16) return paramErr;
    task_t *t = MPCurrentTaskID();
    t->storage[index] = v;
    return noErr;
}
void *MPGetTaskStorageValue(u32 index) { return index < 16 ? ((task_t *)MPCurrentTaskID())->storage[index] : 0; }

/* memória */
enum { kMPAllocateClearMask = 0x0010 };
void *MPAllocateAligned(size_t size, u8 alignment, u32 options) {
    size_t a = (size_t)1 << (alignment > 12 ? 12 : alignment);
    if (a < 16) a = 16;
    u8 *raw = malloc(size + a + sizeof(void *));
    if (!raw) return 0;
    u8 *p = (u8 *)(((uintptr_t)raw + sizeof(void *) + a - 1) & ~(uintptr_t)(a - 1));
    ((void **)p)[-1] = raw;
    if (options & kMPAllocateClearMask) memset(p, 0, size);
    return p;
}
void *MPAllocate(size_t size) { return MPAllocateAligned(size, 5, 0); }
void MPFree(void *p) { if (p) free(((void **)p)[-1]); }
void MPBlockCopy(const void *s, void *d, size_t n) { memmove(d, s, n); }
void MPBlockClear(void *d, size_t n) { memset(d, 0, n); }
void MPDataToCode(void *a, size_t n) { (void)a; (void)n; }
