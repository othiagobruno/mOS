/*
 * mOS libSystem — o começo da libdispatch (GCD) para o porte do VS Code (docs/porte-vscode.md).
 *
 * dispatch_once é completo (atômico, várias threads). As filas ainda não têm threads próprias nem
 * run loop: dispatch_async/sync/after executam o trabalho na hora, na thread de quem chamou. Isso
 * basta para a inicialização; trabalho realmente assíncrono fica para quando houver CFRunLoop.
 * Semáforos e dispatch_time ficam em extra.c.
 */
#include "sys.h"

void *malloc(size_t n);
void free(void *p);
size_t strlen(const char *s);
char *strdup(const char *s);

typedef void (*dispatch_function_t)(void *);
typedef struct { void *isa; int flags, reserved; void (*invoke)(void *, ...); } cx_block_t;
#define RUN_BLOCK(b) ((void (*)(void *))((cx_block_t *)(b))->invoke)(b)

/* ---- dispatch_once: predicado 0 = nunca rodou, ~0 = pronto (como no macOS) */
void dispatch_once_f(long *pred, void *ctx, dispatch_function_t fn) {
    if (*(volatile long *)pred == ~0l) return;
    if (__sync_bool_compare_and_swap(pred, 0, 1)) {
        fn(ctx);
        __sync_synchronize();
        *(volatile long *)pred = ~0l;
        return;
    }
    while (*(volatile long *)pred != ~0l) SYS0(SYS_YIELD);
}
static void run_block_fn(void *b) { RUN_BLOCK(b); }
void dispatch_once(long *pred, void *block) { dispatch_once_f(pred, block, run_block_fn); }

/* ---- filas */
typedef struct cx_queue { u64 magic; char *label; int serial; } cx_queue_t;
#define QMAGIC 0x6D4F5351756575ull
cx_queue_t _dispatch_main_q = {QMAGIC, "com.apple.main-thread", 1};   /* dispatch_get_main_queue() = &_dispatch_main_q */
static cx_queue_t global_q[4] = {{QMAGIC, "com.apple.root.default-qos", 0}, {QMAGIC, "com.apple.root.user-initiated-qos", 0},
                                 {QMAGIC, "com.apple.root.utility-qos", 0}, {QMAGIC, "com.apple.root.background-qos", 0}};

extern volatile int cx_dispatch_main_used;   /* abaixo, junto do mq_push */
void *dispatch_get_main_queue(void) { cx_dispatch_main_used = 1; return &_dispatch_main_q; }
void *dispatch_get_global_queue(long priority, unsigned long flags) {
    (void)flags;
    return &global_q[priority > 0 ? 1 : priority == 0 ? 0 : priority == -2 ? 2 : 3];
}
void *dispatch_queue_create(const char *label, void *attr) {
    cx_queue_t *q = malloc(sizeof *q);
    if (!q) return 0;
    q->magic = QMAGIC;
    q->label = strdup(label ? label : "");
    q->serial = attr == 0;
    return q;
}
void *dispatch_queue_create_with_target$V2(const char *label, void *attr, void *target) { (void)target; return dispatch_queue_create(label, attr); }
const char *dispatch_queue_get_label(void *q) { return q ? ((cx_queue_t *)q)->label : _dispatch_main_q.label; }
void dispatch_set_target_queue(void *o, void *q) { (void)o; (void)q; }
void *dispatch_get_current_queue(void) { return &_dispatch_main_q; }
void *dispatch_queue_attr_make_with_qos_class(void *attr, unsigned qos, int prio) { (void)qos; (void)prio; return attr; }
void *dispatch_queue_attr_make_with_autorelease_frequency(void *attr, unsigned long f) { (void)f; return attr; }

/* ---- fila principal de verdade: trabalho para a thread principal é enfileirado e executado pelo
 * CFRunLoop dela (cx_dispatch_drain_main, chamado em cfrunloop.c). As outras filas ainda executam
 * na hora, na thread de quem chamou. */
int cx_main_tid;                                   /* definido em cx_darwin_init (thread principal) */
static int on_main(void) { return !cx_main_tid || (int)SYS0(SYS_GETTID) == cx_main_tid; }
typedef struct mq_item { void *ctx; dispatch_function_t fn; void *block; u64 when_ms; volatile int *done; struct mq_item *next; } mq_item_t;
static mq_item_t *mq_head, *mq_tail;
static volatile int mq_lock;
static void mql(void) { while (__sync_lock_test_and_set(&mq_lock, 1)) SYS0(SYS_YIELD); }
static void mqu(void) { __sync_lock_release(&mq_lock); }
void *_Block_copy(const void *b);
void _Block_release(const void *b);
void *CFRunLoopGetMain(void);
void CFRunLoopWakeUp(void *rl);
/* a fila principal já recebeu trabalho: como o port do libdispatch no macOS, ela passa a ser fonte permanente do
 * run loop principal (CFRunLoopRun não volta por "sem fontes"; o MainActor do Swift conta com isso) */
volatile int cx_dispatch_main_used;
static void mq_push(void *ctx, dispatch_function_t fn, void *block, u64 when_ms, volatile int *done) {
    cx_dispatch_main_used = 1;
    mq_item_t *it = malloc(sizeof *it);
    if (!it) return;
    it->ctx = ctx; it->fn = fn; it->block = block ? _Block_copy(block) : 0; it->when_ms = when_ms; it->done = done; it->next = 0;
    mql();
    if (mq_tail) mq_tail->next = it; else mq_head = it;
    mq_tail = it;
    mqu();
    CFRunLoopWakeUp(CFRunLoopGetMain());
}
/* executa o que estiver vencido; devolve quantos itens rodaram */
int cx_dispatch_drain_main(void) {
    int ran = 0;
    for (int guard = 0; guard < 256; guard++) {
        u64 now = (u64)SYS0(SYS_TIME);
        mql();
        mq_item_t *prev = 0, *it = mq_head;
        while (it && it->when_ms > now) { prev = it; it = it->next; }
        if (!it) { mqu(); break; }
        if (prev) prev->next = it->next; else mq_head = it->next;
        if (mq_tail == it) mq_tail = prev;
        mqu();
        if (it->block) { RUN_BLOCK(it->block); _Block_release(it->block); } else it->fn(it->ctx);
        if (it->done) *it->done = 1;
        free(it);
        ran++;
    }
    return ran;
}
/* só depois que a thread principal rodou um CFRunLoop (cfrunloop.c liga isto): antes, e nos apps
 * cujo laço não passa pelo CFRunLoop, a fila principal continua executando na hora */
volatile int cx_main_queue_live;
int cx_dispatch_pending(void) { return mq_head != 0; }
/* vai para a fila principal: pedidos de outras threads sempre; da própria thread principal, só
 * depois que o CFRunLoop dela existe (antes disso executa na hora, como apps sem CFRunLoop esperam) */
static int is_main_q(void *q) { return q == &_dispatch_main_q && (cx_main_queue_live || !on_main()); }
/* trabalho com atraso fora da fila principal (dispatch_after em fila global/própria, Task.sleep do Swift): as filas
 * ainda não têm threads, então cada item adiado espera na sua própria thread e roda lá ao vencer */
typedef struct { void *ctx; dispatch_function_t fn; void *block; u64 delay_ms; } delayed_t;
int pthread_create(unsigned long *t, const void *attr, void *(*fn)(void *), void *arg);
int pthread_detach(unsigned long t);
static void *delayed_run(void *arg) {
    delayed_t *d = arg;
    if (d->delay_ms) SYS1(SYS_SLEEP, d->delay_ms);
    if (d->block) { RUN_BLOCK(d->block); _Block_release(d->block); } else d->fn(d->ctx);
    free(d);
    return 0;
}
/* blocos da fila principal executados na hora (antes do CFRunLoop): um dispatch_async para a fila principal feito
 * DE DENTRO de um deles vai para a fila, como no macOS. Senão quem se reagenda (o Chrome faz
 * -rebuildMenu -> dispatch_async(main, ^{ [self rebuildMenu] }) para juntar atualizações) vira recursão sem fim
 * e a thread principal nunca chega ao laço de eventos. */
static int main_inline_depth;
static void q_async(void *q, void *ctx, dispatch_function_t fn, void *block, u64 delay_ms) {
    if (is_main_q(q) || (q == &_dispatch_main_q && delay_ms) ||
        (q == &_dispatch_main_q && main_inline_depth > 0 && on_main())) {
        mq_push(ctx, fn, block, (u64)SYS0(SYS_TIME) + delay_ms, 0);
        return;
    }
    if (q == &_dispatch_main_q && on_main()) {
        main_inline_depth++;
        if (block) RUN_BLOCK(block); else fn(ctx);
        main_inline_depth--;
        return;
    }
    if (delay_ms && delay_ms < (~0ull >> 3)) {
        delayed_t *d = malloc(sizeof *d);
        unsigned long t;
        if (d) {
            d->ctx = ctx; d->fn = fn; d->block = block ? _Block_copy(block) : 0; d->delay_ms = delay_ms;
            if (pthread_create(&t, 0, delayed_run, d) == 0) { pthread_detach(t); return; }
            if (d->block) _Block_release(d->block);
            free(d);
        }
    }
    if (block) RUN_BLOCK(block); else fn(ctx);
}
static void q_sync(void *q, void *ctx, dispatch_function_t fn, void *block) {
    if (is_main_q(q) && !on_main()) {
        volatile int done = 0;
        mq_push(ctx, fn, block, 0, &done);
        while (!done) SYS0(SYS_YIELD);
        return;
    }
    if (block) RUN_BLOCK(block); else fn(ctx);
}
/* dispatch_time: nanossegundos a partir de "agora" (DISPATCH_TIME_NOW = 0) */
u64 cx_dispatch_deadline_ns(u64 t);   /* extra.c: dispatch_time é absoluto (ns), como no macOS */
static u64 delay_of(u64 when) {
    if (when == ~0ull) return ~0ull >> 2;                       /* DISPATCH_TIME_FOREVER */
    u64 at = cx_dispatch_deadline_ns(when), now = (u64)SYS0(SYS_TIME) * 1000000ull;
    return at > now ? (at - now + 999999ull) / 1000000ull : 0;
}
void dispatch_async_f(void *q, void *ctx, dispatch_function_t fn) { q_async(q, ctx, fn, 0, 0); }
void dispatch_sync_f(void *q, void *ctx, dispatch_function_t fn) { q_sync(q, ctx, fn, 0); }
void dispatch_barrier_async_f(void *q, void *ctx, dispatch_function_t fn) { q_async(q, ctx, fn, 0, 0); }
void dispatch_barrier_sync_f(void *q, void *ctx, dispatch_function_t fn) { q_sync(q, ctx, fn, 0); }
void dispatch_async(void *q, void *block) { q_async(q, 0, 0, block, 0); }
void dispatch_sync(void *q, void *block) { q_sync(q, 0, 0, block); }
void dispatch_barrier_async(void *q, void *block) { q_async(q, 0, 0, block, 0); }
void dispatch_barrier_sync(void *q, void *block) { q_sync(q, 0, 0, block); }
void dispatch_after_f(u64 when, void *q, void *ctx, dispatch_function_t fn) { q_async(q, ctx, fn, 0, delay_of(when)); }
void dispatch_after(u64 when, void *q, void *block) { q_async(q, 0, 0, block, delay_of(when)); }
void dispatch_apply_f(size_t n, void *q, void *ctx, void (*fn)(void *, size_t)) { (void)q; for (size_t i = 0; i < n; i++) fn(ctx, i); }

/* grupos: como tudo roda na hora, um grupo está sempre vazio */
typedef struct { u64 magic; long n; } cx_group_t;
void *dispatch_group_create(void) { cx_group_t *g = malloc(sizeof *g); if (g) { g->magic = QMAGIC; g->n = 0; } return g; }
void dispatch_group_enter(void *g) { __sync_fetch_and_add(&((cx_group_t *)g)->n, 1); }
void dispatch_group_leave(void *g) { __sync_fetch_and_sub(&((cx_group_t *)g)->n, 1); }
long dispatch_group_wait(void *g, u64 timeout) {
    (void)timeout;
    while (((cx_group_t *)g)->n > 0) SYS0(SYS_YIELD);
    return 0;
}
void dispatch_group_async_f(void *g, void *q, void *ctx, dispatch_function_t fn) { (void)g; (void)q; fn(ctx); }
void dispatch_group_async(void *g, void *q, void *block) { (void)g; (void)q; RUN_BLOCK(block); }
void dispatch_group_notify_f(void *g, void *q, void *ctx, dispatch_function_t fn) { (void)g; (void)q; fn(ctx); }
void dispatch_group_notify(void *g, void *q, void *block) { (void)g; (void)q; RUN_BLOCK(block); }

/* ---- fontes de eventos do GCD.
 * Só o tipo DISPATCH_SOURCE_TYPE_MACH_RECV tem fonte de verdade: o servidor do MachPortRendezvous do
 * Chromium escuta uma porta Mach com dispatch_source_create(MACH_RECV, porta, ...). Uma thread espera
 * mensagem na porta (SYS_MACH_PORT/MACH_OP_WAIT, bloqueante) e, a cada chegada, chama o handler. Se a
 * fila da fonte for a principal, o handler é enfileirado com dispatch_async_f (o V8 só existe na thread
 * principal; chamar na thread errada quebra). Os outros tipos continuam NULL, como antes. */
int cx_mach_pending(u32 port);
int pthread_create(unsigned long *t, const void *attr, void *(*fn)(void *), void *arg);
int pthread_detach(unsigned long t);

/* símbolos dos tipos de fonte (o endereço é o que importa; o conteúdo não é lido) */
char _dispatch_source_type_mach_recv[8];
char _dispatch_source_type_mach_send[8];
char _dispatch_source_type_timer[8];
char _dispatch_source_type_read[8];
char _dispatch_source_type_write[8];
char _dispatch_source_type_signal[8];
char _dispatch_source_type_data_add[8];
char _dispatch_source_type_data_or[8];

typedef struct cx_source {
    u64 magic;
    const void *type;
    u32 port;                 /* MACH_RECV: porta escutada */
    void *queue;
    void *handler_block;      /* _Block_copy'd */
    dispatch_function_t handler_fn;
    void *ctx;
    void *cancel_block;
    dispatch_function_t cancel_fn;
    volatile int active, canceled;
    int tid;
    volatile u64 start, interval;   /* TIMER: dispatch_source_set_timer (start absoluto em ns; ~0 = sem repetição) */
    volatile int armed;             /* TIMER: há um disparo programado */
} cx_source_t;
#define SMAGIC 0x6D4F53737263ull

void *dispatch_get_context(void *o);   /* vscode_libc.c */
static void source_fire(void *arg) {
    cx_source_t *s = arg;
    if (s->canceled) return;
    if (s->handler_block) RUN_BLOCK(s->handler_block);
    else if (s->handler_fn) s->handler_fn(s->ctx ? s->ctx : dispatch_get_context(s));   /* _f recebe o dispatch_set_context */
}

/* thread da fonte TIMER: espera o prazo (em fatias, para obedecer cancel/suspend), dispara na fila da fonte e,
 * com intervalo, reprograma; sem intervalo (DISPATCH_TIME_FOREVER), fica desarmada até outro set_timer */
static void timer_thread(void *arg) {
    cx_source_t *s = arg;
    while (!s->canceled) {
        if (!s->active || !s->armed) { SYS1(SYS_SLEEP, 2); continue; }
        u64 d = delay_of(s->start);
        if (d) { SYS1(SYS_SLEEP, d > 20 ? 20 : d); continue; }
        if (s->interval && s->interval < (~0ull >> 1)) s->start = (u64)SYS0(SYS_TIME) * 1000000ull + s->interval;
        else s->armed = 0;
        if (s->queue == &_dispatch_main_q) dispatch_async_f(s->queue, s, source_fire);
        else source_fire(s);
    }
    if (s->cancel_block) RUN_BLOCK(s->cancel_block);
    else if (s->cancel_fn) s->cancel_fn(s->ctx ? s->ctx : dispatch_get_context(s));
}

/* thread da fonte MACH_RECV: espera mensagem na porta e dispara o handler na fila certa */
static void source_thread(void *arg) {
    cx_source_t *s = arg;
    u64 a[8];
    while (!s->canceled) {
        if (!s->active) { SYS1(SYS_SLEEP, 2); continue; }
        a[0] = s->port; a[1] = 50;                 /* espera com prazo curto: deixa cancelar */
        long r = cx_syscall(SYS_MACH_PORT, (long)MACH_OP_WAIT, (long)a, 0, 0, 0);
        (void)r;
        if (s->canceled) break;
        if (!cx_mach_pending(s->port)) continue;   /* só prazo; nada chegou */
        /* o handler lê a mensagem (dispatch_source_merge_data/mach_msg); dispara na fila da fonte */
        if (is_main_q(s->queue)) dispatch_async_f(s->queue, s, source_fire);
        else source_fire(s);
    }
    if (s->cancel_block) RUN_BLOCK(s->cancel_block);
    else if (s->cancel_fn) s->cancel_fn(s->ctx);
}

void *dispatch_source_create(const void *type, unsigned long handle, unsigned long mask, void *queue) {
    (void)mask;
    if (type != _dispatch_source_type_mach_recv && type != _dispatch_source_type_timer) return 0;   /* MACH_RECV e TIMER */
    cx_source_t *s = malloc(sizeof *s);
    if (!s) return 0;
    for (unsigned i = 0; i < sizeof *s; i++) ((char *)s)[i] = 0;
    s->magic = SMAGIC;
    s->type = type;
    s->port = (u32)handle;
    s->queue = queue ? queue : dispatch_get_main_queue();
    return s;
}
static int is_source(void *s) { return s && ((cx_source_t *)s)->magic == SMAGIC; }

/* a fonte NULL (tipos ainda não suportados) continua ignorando as operações */
void dispatch_source_set_event_handler(void *s, void *block) {
    if (is_source(s)) ((cx_source_t *)s)->handler_block = block ? _Block_copy(block) : 0;
}
void dispatch_source_set_event_handler_f(void *s, dispatch_function_t fn) {
    if (is_source(s)) ((cx_source_t *)s)->handler_fn = fn;
}
void dispatch_source_set_cancel_handler(void *s, void *block) {
    if (is_source(s)) ((cx_source_t *)s)->cancel_block = block ? _Block_copy(block) : 0;
}
void dispatch_source_set_cancel_handler_f(void *s, dispatch_function_t fn) {
    if (is_source(s)) ((cx_source_t *)s)->cancel_fn = fn;
}
void dispatch_source_set_registration_handler(void *s, void *block) { (void)s; (void)block; }
void dispatch_source_set_timer(void *s, u64 start, u64 interval, u64 leeway) {
    (void)leeway;
    if (!is_source(s) || ((cx_source_t *)s)->type != _dispatch_source_type_timer) return;
    cx_source_t *x = s;
    x->start = start == ~0ull ? ~0ull : cx_dispatch_deadline_ns(start);
    x->interval = interval;
    x->armed = start != ~0ull;
}
void dispatch_source_cancel(void *s) { if (is_source(s)) ((cx_source_t *)s)->canceled = 1; }
long dispatch_source_testcancel(void *s) { return is_source(s) ? ((cx_source_t *)s)->canceled : (s == 0); }
unsigned long dispatch_source_get_handle(void *s) { return is_source(s) ? ((cx_source_t *)s)->port : 0; }
unsigned long dispatch_source_get_data(void *s) { (void)s; return 0; }
unsigned long dispatch_source_get_mask(void *s) { (void)s; return 0; }
void dispatch_source_merge_data(void *s, unsigned long v) { (void)s; (void)v; }

static void *source_pthread(void *a) {
    if (((cx_source_t *)a)->type == _dispatch_source_type_timer) timer_thread(a); else source_thread(a);
    return 0;
}

void *dispatch_get_specific(const void *key) { (void)key; return 0; }
void *dispatch_queue_get_specific(void *q, const void *key) { (void)q; (void)key; return 0; }
void dispatch_queue_set_specific(void *q, const void *key, void *ctx, void (*dtor)(void *)) { (void)q; (void)key; (void)ctx; (void)dtor; }
/* resume/activate de uma fonte MACH_RECV: liga a escuta e, na primeira vez, sobe a thread que espera
 * na porta. Em objetos que não são fonte (filas etc.), não há o que retomar. */
static void source_resume(cx_source_t *s) {
    s->active = 1;
    if (!s->tid) {
        unsigned long t = 0;
        if (pthread_create(&t, 0, source_pthread, s) == 0) { pthread_detach(t); s->tid = (int)t ? (int)t : 1; }
    }
}
void dispatch_activate(void *o) { if (is_source(o)) source_resume(o); }
void dispatch_resume(void *o) { if (is_source(o)) source_resume(o); }
void dispatch_suspend(void *o) { if (is_source(o)) ((cx_source_t *)o)->active = 0; }
