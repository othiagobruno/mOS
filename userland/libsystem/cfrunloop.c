/*
 * mOS libSystem — CFRunLoop (o laço de eventos do CoreFoundation), para o porte do VS Code: o
 * MessagePump do Chromium no macOS é feito de fontes, timers e observadores de CFRunLoop.
 *
 * Um run loop por thread. Fontes da versão 0 (sinalizadas e executadas pelo perform), timers com
 * repetição, observadores das atividades padrão. A espera entre eventos dorme em fatias de 1 ms e
 * acorda com CFRunLoopWakeUp/CFRunLoopSourceSignal de qualquer thread. Os modos são aceitos, mas
 * tudo roda em todos os modos. Os timers do NSTimer (foundation.c) também rodam dentro deste laço.
 */
#include "foundation.h"

id objc_retain(id o);
void objc_release(id o);

/* CFAbsoluteTime: segundos desde 1/1/2001 */
#define CF_EPOCH 978307200.0
double CFAbsoluteTimeGetCurrent(void) { return cx_now() - CF_EPOCH; }

enum { T_LOOP = 1, T_SOURCE, T_TIMER, T_OBSERVER };
enum { ACT_ENTRY = 1, ACT_BEFORE_TIMERS = 2, ACT_BEFORE_SOURCES = 4, ACT_BEFORE_WAITING = 32, ACT_AFTER_WAITING = 64, ACT_EXIT = 128 };

typedef struct {   /* CFRunLoopSourceContext (versão 0) */
    long version; void *info; const void *(*retain)(const void *); void (*release)(const void *);
    void *copyDescription, *equal, *hash;
    void (*schedule)(void *info, void *rl, void *mode); void (*cancel)(void *info, void *rl, void *mode);
    void (*perform)(void *info);
} src_ctx_t;
typedef struct {   /* CFRunLoopTimerContext / CFRunLoopObserverContext */
    long version; void *info; const void *(*retain)(const void *); void (*release)(const void *); void *copyDescription;
} obj_ctx_t;

#define MAX_ITEMS 64
typedef struct cf_obj {
    cx_class_t *isa;
    int type, valid;
    /* fonte */
    src_ctx_t sctx; volatile int signaled; long order;
    /* timer */
    double fire, interval; void (*tcb)(void *timer, void *info); void *info;
    /* observador */
    unsigned long activities; int repeats; void (*ocb)(void *obs, unsigned long act, void *info);
    /* run loop */
    struct cf_obj *items[MAX_ITEMS]; int nitems;
    volatile int stop, wake, running;
    int tid;
    /* blocos de CFRunLoopPerformBlock pendentes */
    void *blocks[32]; int nblocks;
} cf_obj_t;
#define MAX_BLOCKS 32
void *_Block_copy(const void *b);
void _Block_release(const void *b);
typedef struct { void *isa; int flags, reserved; void (*invoke)(void *); } cf_blk_t;
#define RUN_BLK(b) (((cf_blk_t *)(b))->invoke(b))

static cx_class_t CFType_c, CFType_m;
static int class_ready;
static cf_obj_t *new_obj(int type) {
    if (!class_ready) {
        class_ready = 1;
        cx_define_class(&CFType_c, &CFType_m, "__NSCFType", cx_class_named("NSObject"), sizeof(cf_obj_t) - 8);
    }
    cf_obj_t *o = (cf_obj_t *)cx_alloc_instance(&CFType_c);
    o->type = type;
    o->valid = 1;
    return o;
}

/* ---- um run loop por thread (pelo tid do kernel) */
#define MAX_LOOPS 64
static cf_obj_t *loops[MAX_LOOPS];
static cf_obj_t *main_loop;
static int lock;
static void lk(void) { while (__sync_lock_test_and_set(&lock, 1)) SYS0(SYS_YIELD); }
static void ulk(void) { __sync_lock_release(&lock); }

extern int cx_main_tid;                                    /* dispatch.c / cx_darwin_init */
int cx_futex_wait(volatile int *w, int expected, long ms);   /* extra.c: espera no kernel (SYS_FUTEX_WAIT) */
int cx_futex_wake(volatile int *w, int n);
static cf_obj_t *loop_for(int tid) {
    lk();
    cf_obj_t *r = 0;
    for (int i = 0; i < MAX_LOOPS; i++) if (loops[i] && loops[i]->tid == tid) { r = loops[i]; break; }
    if (!r)
        for (int i = 0; i < MAX_LOOPS; i++)
            if (!loops[i]) { r = loops[i] = new_obj(T_LOOP); r->tid = tid; break; }
    if (r && !main_loop && (tid == cx_main_tid || !cx_main_tid)) main_loop = r;
    ulk();
    return r;
}
void *CFRunLoopGetCurrent(void) { return loop_for((int)SYS0(SYS_GETTID)); }
void *CFRunLoopGetMain(void) { if (!main_loop) loop_for(cx_main_tid ? cx_main_tid : (int)SYS0(SYS_GETTID)); return main_loop; }

static void add_item(cf_obj_t *rl, cf_obj_t *it) {
    if (!rl || !it) return;
    cf_obj_t *dead[MAX_ITEMS];
    int ndead = 0;
    lk();
    for (int i = 0; i < rl->nitems; i++) if (rl->items[i] == it) { ulk(); return; }
    /* lista cheia: timers de um disparo já vencidos e observadores invalidados nunca saíam do loop (ninguém chama
     * Remove depois de um timer de um disparo); abre espaço com eles, soltos fora da trava */
    if (rl->nitems >= MAX_ITEMS)
        for (int i = 0; i < rl->nitems;) {
            cf_obj_t *x = rl->items[i];
            if (!x->valid && (x->type == T_TIMER || x->type == T_OBSERVER)) { rl->items[i] = rl->items[--rl->nitems]; dead[ndead++] = x; }
            else i++;
        }
    if (rl->nitems < MAX_ITEMS) { rl->items[rl->nitems++] = (cf_obj_t *)objc_retain((id)it); }
    ulk();
    for (int i = 0; i < ndead; i++) objc_release((id)dead[i]);
    if (it->type == T_SOURCE && it->sctx.schedule) it->sctx.schedule(it->sctx.info, rl, 0);
}
static void remove_item(cf_obj_t *rl, cf_obj_t *it) {
    if (!rl || !it) return;
    lk();
    for (int i = 0; i < rl->nitems; i++)
        if (rl->items[i] == it) {
            rl->items[i] = rl->items[--rl->nitems];
            ulk();
            if (it->type == T_SOURCE && it->sctx.cancel) it->sctx.cancel(it->sctx.info, rl, 0);
            objc_release((id)it);
            return;
        }
    ulk();
}
static int contains(cf_obj_t *rl, cf_obj_t *it) {
    for (int i = 0; rl && i < rl->nitems; i++) if (rl->items[i] == it) return 1;
    return 0;
}

/* ---- fontes */
void *CFRunLoopSourceCreate(void *alloc, long order, src_ctx_t *ctx) {
    (void)alloc;
    if (!ctx || ctx->version != 0) return 0;          /* versão 1 (porta Mach) ainda não */
    cf_obj_t *s = new_obj(T_SOURCE);
    s->sctx = *ctx;
    s->order = order;
    if (ctx->retain && ctx->info) ctx->retain(ctx->info);
    return s;
}
void CFRunLoopSourceSignal(void *s) { if (s) ((cf_obj_t *)s)->signaled = 1; }
void CFRunLoopSourceInvalidate(void *s) { if (s) ((cf_obj_t *)s)->valid = 0; }
unsigned char CFRunLoopSourceIsValid(void *s) { return s && ((cf_obj_t *)s)->valid; }
void CFRunLoopSourceGetContext(void *s, src_ctx_t *ctx) { if (s && ctx) *ctx = ((cf_obj_t *)s)->sctx; }
void CFRunLoopAddSource(void *rl, void *s, void *mode) {
    (void)mode;
    {   /* diagnóstico (ca-log): em que loop cada fonte entra e quem pede (o MessagePumpNSRunLoop do Chromium põe as fontes
         * de trabalho no loop da thread dele; se cair no de outra, as tarefas postadas para ela nunca rodam) */
        extern int cx_ca_diag(void);
        extern int cx_fdprintf(int fd, const char *fmt, ...);
        static int n;
        if (rl && cx_ca_diag() && ++n <= 40)
            cx_fdprintf(2, "[ca] CFRunLoopAddSource %p (perform %p) no loop da thread %d, pedido pela thread %d\n", s,
                        s ? (void *)((cf_obj_t *)s)->sctx.perform : 0, ((cf_obj_t *)rl)->tid, (int)SYS0(SYS_GETTID));
    }
    add_item(rl, s);
}
void CFRunLoopRemoveSource(void *rl, void *s, void *mode) { (void)mode; remove_item(rl, s); }
unsigned char CFRunLoopContainsSource(void *rl, void *s, void *mode) { (void)mode; return (unsigned char)contains(rl, s); }

/* ---- timers */
void *CFRunLoopTimerCreate(void *alloc, double fire, double interval, unsigned long flags, long order,
                           void (*cb)(void *, void *), obj_ctx_t *ctx) {
    (void)alloc; (void)flags; (void)order;
    cf_obj_t *t = new_obj(T_TIMER);
    t->fire = fire;
    t->interval = interval;
    t->tcb = cb;
    t->info = ctx ? ctx->info : 0;
    if (ctx && ctx->retain && ctx->info) ctx->retain(ctx->info);
    return t;
}
void CFRunLoopTimerSetNextFireDate(void *t, double fire) { if (t) ((cf_obj_t *)t)->fire = fire; }
double CFRunLoopTimerGetNextFireDate(void *t) { return t ? ((cf_obj_t *)t)->fire : 0; }
double CFRunLoopTimerGetInterval(void *t) { return t ? ((cf_obj_t *)t)->interval : 0; }
void CFRunLoopTimerInvalidate(void *t) { if (t) ((cf_obj_t *)t)->valid = 0; }
unsigned char CFRunLoopTimerIsValid(void *t) { return t && ((cf_obj_t *)t)->valid; }
void CFRunLoopTimerSetTolerance(void *t, double tol) { (void)t; (void)tol; }
double CFRunLoopTimerGetTolerance(void *t) { (void)t; return 0; }
void CFRunLoopAddTimer(void *rl, void *t, void *mode) { (void)mode; add_item(rl, t); }
void CFRunLoopRemoveTimer(void *rl, void *t, void *mode) { (void)mode; remove_item(rl, t); }
unsigned char CFRunLoopContainsTimer(void *rl, void *t, void *mode) { (void)mode; return (unsigned char)contains(rl, t); }

/* ---- observadores */
void *CFRunLoopObserverCreate(void *alloc, unsigned long activities, unsigned char repeats, long order,
                              void (*cb)(void *, unsigned long, void *), obj_ctx_t *ctx) {
    (void)alloc; (void)order;
    cf_obj_t *o = new_obj(T_OBSERVER);
    o->activities = activities;
    o->repeats = repeats;
    o->ocb = cb;
    o->info = ctx ? ctx->info : 0;
    if (ctx && ctx->retain && ctx->info) ctx->retain(ctx->info);
    return o;
}
void CFRunLoopObserverInvalidate(void *o) { if (o) ((cf_obj_t *)o)->valid = 0; }
unsigned char CFRunLoopObserverIsValid(void *o) { return o && ((cf_obj_t *)o)->valid; }
void CFRunLoopAddObserver(void *rl, void *o, void *mode) { (void)mode; add_item(rl, o); }
void CFRunLoopRemoveObserver(void *rl, void *o, void *mode) { (void)mode; remove_item(rl, o); }
unsigned char CFRunLoopContainsObserver(void *rl, void *o, void *mode) { (void)mode; return (unsigned char)contains(rl, o); }

/* ---- o laço */
static void notify(cf_obj_t *rl, unsigned long act) {
    cf_obj_t *snap[MAX_ITEMS];
    int n = 0;
    lk();
    for (int i = 0; i < rl->nitems; i++)
        if (rl->items[i]->type == T_OBSERVER && rl->items[i]->valid && (rl->items[i]->activities & act))
            snap[n++] = (cf_obj_t *)objc_retain((id)rl->items[i]);
    ulk();
    for (int i = 0; i < n; i++) {
        if (snap[i]->valid && snap[i]->ocb) snap[i]->ocb(snap[i], act, snap[i]->info);
        if (!snap[i]->repeats) snap[i]->valid = 0;
        objc_release((id)snap[i]);
    }
}
/* dispara os timers vencidos; devolve a hora do próximo (ou 1e300) */
static double run_timers(cf_obj_t *rl) {
    double now = CFAbsoluteTimeGetCurrent(), next = 1e300;
    cf_obj_t *due[MAX_ITEMS];
    int n = 0;
    lk();
    for (int i = 0; i < rl->nitems; i++) {
        cf_obj_t *t = rl->items[i];
        if (t->type != T_TIMER || !t->valid) continue;
        if (t->fire <= now) due[n++] = (cf_obj_t *)objc_retain((id)t);
        else if (t->fire < next) next = t->fire;
    }
    ulk();
    for (int i = 0; i < n; i++) {
        cf_obj_t *t = due[i];
        double was = t->fire;
        if (t->tcb) t->tcb(t, t->info);
        if (t->interval > 0 && t->valid) {
            if (t->fire == was) t->fire = was + t->interval * (1 + (long)((now - was) / t->interval));
        } else if (t->fire == was) t->valid = 0;              /* timer de um disparo */
        if (t->valid && t->fire < next) next = t->fire;
        objc_release((id)t);
    }
    return next;
}
static int run_sources(cf_obj_t *rl) {
    cf_obj_t *ready[MAX_ITEMS];
    int n = 0;
    lk();
    for (int i = 0; i < rl->nitems; i++) {
        cf_obj_t *s = rl->items[i];
        if (s->type == T_SOURCE && s->valid && s->signaled) { s->signaled = 0; ready[n++] = (cf_obj_t *)objc_retain((id)s); }
    }
    ulk();
    for (int i = 0; i < n; i++) {
        if (ready[i]->sctx.perform) ready[i]->sctx.perform(ready[i]->sctx.info);
        objc_release((id)ready[i]);
    }
    return n;
}
static int run_blocks(cf_obj_t *rl) {
    void *b[MAX_BLOCKS];
    lk();
    int n = rl->nblocks;
    for (int i = 0; i < n; i++) b[i] = rl->blocks[i];
    rl->nblocks = 0;
    ulk();
    for (int i = 0; i < n; i++) { RUN_BLK(b[i]); _Block_release(b[i]); }
    return n;
}
static int has_work(cf_obj_t *rl) {
    if (rl->nblocks) return 1;
    for (int i = 0; i < rl->nitems; i++) if (rl->items[i]->valid && rl->items[i]->type != T_OBSERVER) return 1;
    return 0;
}

/* CFRunLoopRunResult: 1 Finished, 2 Stopped, 3 TimedOut, 4 HandledSource */
extern volatile int cx_main_queue_live;
int cx_dispatch_drain_main(void);
int cx_dispatch_pending(void);
extern volatile int cx_dispatch_main_used;   /* dispatch.c: a fila principal virou fonte do run loop principal */
int CFRunLoopRunInMode(void *mode, double seconds, unsigned char return_after_source) {
    (void)mode;
    cf_obj_t *rl = CFRunLoopGetCurrent();
    int is_main = rl == main_loop;
    if (is_main) cx_main_queue_live = 1;                   /* a fila principal do GCD passa a valer */
    double deadline = CFAbsoluteTimeGetCurrent() + (seconds < 0 ? 0 : seconds);
    int result = 3;
    rl->running++;
    notify(rl, ACT_ENTRY);
    for (;;) {
        notify(rl, ACT_BEFORE_TIMERS);
        double next = run_timers(rl);
        if (is_main) cx_run_timers();                      /* NSTimer do foundation.c: são da thread principal */
        notify(rl, ACT_BEFORE_SOURCES);
        int handled = run_blocks(rl) + run_sources(rl);
        if (is_main) handled += cx_dispatch_drain_main();   /* blocos da fila principal do GCD */
        if (rl->stop) { rl->stop = 0; result = 2; break; }
        if (handled && return_after_source) { result = 4; break; }
        if (!has_work(rl) && !(is_main && (cx_timers || cx_dispatch_pending() || cx_dispatch_main_used))) { result = 1; break; }
        double now = CFAbsoluteTimeGetCurrent();
        if (now >= deadline) { result = 3; break; }
        notify(rl, ACT_BEFORE_WAITING);
        double until = next < deadline ? next : deadline;
        /* espera no futex sobre rl->wake: CFRunLoopWakeUp/Stop/PerformBlock acordam na hora. Fontes sinalizadas
         * sem WakeUp e a fila principal do GCD ainda são conferidas a cada volta; o cochilo dobra até 8 ms enquanto
         * nada acontece (com 1 ms fixo, cada run loop ocioso acordava mil vezes por segundo) */
        u32 nap = 1;
        while (!rl->wake && !rl->stop && CFAbsoluteTimeGetCurrent() < until) {
            int pending = 0;
            for (int i = 0; i < rl->nitems; i++) if (rl->items[i]->type == T_SOURCE && rl->items[i]->signaled) { pending = 1; break; }
            if (pending || rl->nblocks || (is_main && cx_dispatch_pending())) break;
            double left = (until - CFAbsoluteTimeGetCurrent()) * 1000.0;
            long ms = left < 1 ? 1 : left < (double)nap ? (long)left : (long)nap;
            cx_futex_wait(&rl->wake, 0, ms);
            /* a principal ainda consulta eventos de janela e a fila principal do GCD: até 8 ms. As outras só acordam
             * por CFRunLoopWakeUp (instantâneo no futex) ou timer: até 128 ms (com 8 ms, as threads do
             * MessagePumpNSRunLoop do Chromium gastavam ~15 s de CPU cada em 4 min só acordando) */
            if (nap < (is_main ? 8u : 128u)) nap <<= 1;
        }
        rl->wake = 0;
        notify(rl, ACT_AFTER_WAITING);
    }
    notify(rl, ACT_EXIT);
    rl->running--;
    return result;
}
void CFRunLoopRun(void) { while (CFRunLoopRunInMode(0, 1e10, 0) == 3) {} }
void CFRunLoopStop(void *rl) { if (rl) { ((cf_obj_t *)rl)->stop = 1; ((cf_obj_t *)rl)->wake = 1; cx_futex_wake(&((cf_obj_t *)rl)->wake, 0x7FFFFFFF); } }
void CFRunLoopWakeUp(void *rl) { if (rl) { ((cf_obj_t *)rl)->wake = 1; cx_futex_wake(&((cf_obj_t *)rl)->wake, 0x7FFFFFFF); } }
unsigned char CFRunLoopIsWaiting(void *rl) { return rl && ((cf_obj_t *)rl)->running; }
id CFRunLoopCopyCurrentMode(void *rl) { (void)rl; return objc_retain(cx_str("kCFRunLoopDefaultMode")); }
void CFRunLoopAddCommonMode(void *rl, void *mode) { (void)rl; (void)mode; }
/* CFRunLoopPerformBlock: o bloco roda dentro do run loop indicado, na thread dele (antes rodava na
 * hora, na thread de quem chamou). Fica numa lista do loop, drenada em CFRunLoopRunInMode. */
void CFRunLoopPerformBlock(void *rl, void *mode, void *block) {
    (void)mode;
    cf_obj_t *l = rl;
    if (!block) return;
    if (!l) l = CFRunLoopGetCurrent();
    /* loop da própria thread que nunca rodou (apps do mOS sem CFRunLoop): executa na hora, como antes */
    if (l == CFRunLoopGetCurrent() && !l->running && !(l == main_loop && cx_main_queue_live)) { RUN_BLK(block); return; }
    lk();
    if (l->nblocks < MAX_BLOCKS) { l->blocks[l->nblocks++] = _Block_copy(block); ulk(); }
    else { ulk(); if (l == CFRunLoopGetCurrent()) RUN_BLK(block); }   /* lista cheia: só na própria thread */
    l->wake = 1;
    cx_futex_wake(&l->wake, 0x7FFFFFFF);
}
