/*
 * mOS libSystem — NSOperation, NSBlockOperation, NSInvocationOperation e NSOperationQueue sobre a libdispatch.
 *
 * Como no macOS:
 *  - a fila respeita maxConcurrentOperationCount (padrão: sem limite fixo), suspended, prioridade e dependências;
 *  - +mainQueue roda na fila principal do GCD (thread principal), com uma operação por vez;
 *  - underlyingQueue escolhe a fila do GCD onde as operações rodam; sem ela, a fila global da QoS;
 *  - operações assíncronas (isAsynchronous / start próprio) terminam quando postam isFinished por KVO: a fila
 *    observa "isFinished" e "isReady" de cada operação;
 *  - +currentQueue é a fila da operação em execução nesta thread (e a principal na thread principal);
 *  - isExecuting/isFinished/isCancelled/isReady mudam com willChange/didChange (KVO), e o completionBlock roda
 *    numa fila global depois que a operação termina.
 */
#include "foundation.h"
#include "libc.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void *_Block_copy(const void *b);
void _Block_release(const void *b);
IMP cx_lookup(cx_class_t *cls, SEL sel);
const char *cx_method_types(cx_class_t *c, SEL sel);
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *));
void *dispatch_get_main_queue(void);
void *dispatch_get_global_queue(long id, unsigned long flags);
int pthread_main_np(void);
int pthread_key_create(unsigned long *k, void (*dtor)(void *));
void *pthread_getspecific(unsigned long k);
int pthread_setspecific(unsigned long k, const void *v);

typedef struct { void *isa; int flags, reserved; void (*invoke)(void *, ...); } blk_t;
static id cls(const char *n) { return (id)cx_class_named(n); }
static void spin(volatile int *l) { while (__atomic_exchange_n(l, 1, __ATOMIC_ACQUIRE)) { } }
static void unspin(volatile int *l) { __atomic_store_n(l, 0, __ATOMIC_RELEASE); }
static void kvo_will(id o, const char *k) { MSG(void, o, "willChangeValueForKey:", cx_str(k)); }
static void kvo_did(id o, const char *k) { MSG(void, o, "didChangeValueForKey:", cx_str(k)); }

cx_class_t NSOperation_c __asm__("OBJC_CLASS_$_NSOperation");
cx_class_t NSOperation_m __asm__("OBJC_METACLASS_$_NSOperation");
cx_class_t NSBlockOperation_c __asm__("OBJC_CLASS_$_NSBlockOperation");
cx_class_t NSBlockOperation_m __asm__("OBJC_METACLASS_$_NSBlockOperation");
cx_class_t NSInvocationOperation_c __asm__("OBJC_CLASS_$_NSInvocationOperation");
cx_class_t NSInvocationOperation_m __asm__("OBJC_METACLASS_$_NSInvocationOperation");
cx_class_t NSOperationQueue_c __asm__("OBJC_CLASS_$_NSOperationQueue");
cx_class_t NSOperationQueue_m __asm__("OBJC_METACLASS_$_NSOperationQueue");
static cx_class_t obs_c, obs_m;

typedef struct {
    cx_class_t *isa;
    volatile int state;      /* 0 parada, 1 executando, 2 terminada */
    volatile int cancelled, dispatched;
    volatile int lock;
    id deps, name, blocks, target, arg, result;
    SEL sel;
    void *completion;
    i64 priority, qos;
    id queue;                /* fila dona (fraca) */
} op_t;

typedef struct {
    cx_class_t *isa;
    id ops, name;
    i64 max, qos;
    int suspended, is_main, running;
    void *underlying;
    volatile int lock;
} q_t;

#define OP(o) ((op_t *)(o))
#define Q(o) ((q_t *)(o))
#define DEFAULT_MAX (-1)
#define HARD_CAP 64                /* o "sem limite" do GCD: tantas quanto o sistema der conta */

const long NSOperationQueueDefaultMaxConcurrentOperationCount = -1;
static id g_observer;              /* instância única do observador KVO das filas */
static unsigned long g_cur_key;
static int g_cur_key_ok;

/* ======================================================================== NSOperation */

static BOOL is_finished(id op) { return MSG0(BOOL, op, "isFinished"); }
static BOOL is_executing(id op) { return MSG0(BOOL, op, "isExecuting"); }
static BOOL is_cancelled(id op) { return MSG0(BOOL, op, "isCancelled"); }
static BOOL is_ready(id op) { return MSG0(BOOL, op, "isReady"); }

static id op_init(id self, SEL c) { (void)c; OP(self)->qos = -1; return self; }
static BOOL op_g_finished(id s, SEL c) { (void)c; return OP(s)->state == 2; }
static BOOL op_g_executing(id s, SEL c) { (void)c; return OP(s)->state == 1; }
static BOOL op_g_cancelled(id s, SEL c) { (void)c; return OP(s)->cancelled != 0; }
static BOOL op_g_async(id s, SEL c) { (void)s; (void)c; return NO; }
static BOOL op_g_ready(id s, SEL c) {
    (void)c;
    op_t *o = OP(s);
    if (o->cancelled) return YES;   /* cancelada fica pronta, para a fila poder tirá-la */
    spin(&o->lock);
    id deps = o->deps ? MSG0(id, o->deps, "copy") : 0;
    unspin(&o->lock);
    BOOL ok = YES;
    u64 n = deps ? MSG0(u64, deps, "count") : 0;
    for (u64 i = 0; i < n && ok; i++) if (!is_finished(MSG(id, deps, "objectAtIndex:", i))) ok = NO;
    if (deps) objc_release(deps);
    return ok;
}
static void run_completion(void *ctx) {
    void *b = ctx;
    ((void (*)(void *))((blk_t *)b)->invoke)(b);
    _Block_release(b);
}
static void finish_op(id s) {
    op_t *o = OP(s);
    int was = o->state;
    if (was == 2) return;
    kvo_will(s, "isFinished");
    if (was == 1) kvo_will(s, "isExecuting");
    o->state = 2;
    if (was == 1) kvo_did(s, "isExecuting");
    kvo_did(s, "isFinished");
    void *b = o->completion;
    if (b) { o->completion = 0; dispatch_async_f(dispatch_get_global_queue(0, 0), b, run_completion); }
}
static void op_start(id s, SEL c) {
    (void)c;
    op_t *o = OP(s);
    if (o->state != 0) return;   /* start duas vezes: o macOS lança; aqui não faz nada */
    if (is_cancelled(s)) { finish_op(s); return; }
    kvo_will(s, "isExecuting");
    o->state = 1;
    kvo_did(s, "isExecuting");
    MSG0(void, s, "main");
    finish_op(s);
}
static void op_main(id s, SEL c) { (void)s; (void)c; }
static void op_cancel(id s, SEL c) {
    (void)c;
    op_t *o = OP(s);
    if (o->cancelled || o->state == 2) return;
    kvo_will(s, "isCancelled");
    o->cancelled = 1;
    kvo_did(s, "isCancelled");
    /* cancelada fica pronta: avisa quem espera isReady */
    kvo_will(s, "isReady");
    kvo_did(s, "isReady");
}
static void op_add_dep(id s, SEL c, id d) {
    (void)c;
    op_t *o = OP(s);
    if (!d || d == s) return;
    kvo_will(s, "isReady");
    kvo_will(s, "dependencies");
    spin(&o->lock);
    if (!o->deps) o->deps = MSG0(id, MSG0(id, cls("NSMutableArray"), "alloc"), "init");
    if (!MSG(BOOL, o->deps, "containsObject:", d)) cx_arr_add(o->deps, d);
    unspin(&o->lock);
    kvo_did(s, "dependencies");
    kvo_did(s, "isReady");
    /* a dependência terminando torna esta pronta: a fila observa isFinished da outra */
    if (g_observer) MSG(void, d, "addObserver:forKeyPath:options:context:", g_observer, cx_str("isFinished"), (u64)0, (void *)s);
}
static void op_remove_dep(id s, SEL c, id d) {
    (void)c;
    op_t *o = OP(s);
    kvo_will(s, "isReady");
    kvo_will(s, "dependencies");
    spin(&o->lock);
    if (o->deps && d) MSG(void, o->deps, "removeObject:", d);
    unspin(&o->lock);
    kvo_did(s, "dependencies");
    kvo_did(s, "isReady");
}
static id op_deps(id s, SEL c) {
    (void)c;
    op_t *o = OP(s);
    spin(&o->lock);
    id r = o->deps ? MSG0(id, o->deps, "copy") : MSG0(id, MSG0(id, cls("NSArray"), "alloc"), "init");
    unspin(&o->lock);
    return objc_autorelease(r);
}
static void op_wait(id s, SEL c) {
    (void)c;
    /* a operação pode ser de subclasse com estado próprio: pergunta a ela */
    while (!is_finished(s)) usleep(500);
}
static void *op_completion(id s, SEL c) { (void)c; return OP(s)->completion; }
static void op_set_completion(id s, SEL c, void *b) {
    (void)c;
    op_t *o = OP(s);
    void *old = o->completion;
    o->completion = b ? _Block_copy(b) : 0;
    if (old) _Block_release(old);
}
static i64 op_priority(id s, SEL c) { (void)c; return OP(s)->priority; }
static void op_set_priority(id s, SEL c, i64 p) {
    (void)c;
    /* normaliza para os cinco níveis (-8, -4, 0, 4, 8) */
    i64 v = p <= -6 ? -8 : p <= -2 ? -4 : p < 2 ? 0 : p < 6 ? 4 : 8;
    OP(s)->priority = v;
}
static i64 op_qos(id s, SEL c) { (void)c; return OP(s)->qos; }
static void op_set_qos(id s, SEL c, i64 q) { (void)c; OP(s)->qos = q; }
static id op_name(id s, SEL c) { (void)c; return OP(s)->name; }
static void op_set_name(id s, SEL c, id n) { (void)c; id o = OP(s)->name; OP(s)->name = n ? MSG0(id, n, "copy") : 0; if (o) objc_release(o); }
static double op_thread_priority(id s, SEL c) { (void)s; (void)c; return 0.5; }
static void op_set_thread_priority(id s, SEL c, double p) { (void)s; (void)c; (void)p; }
static void super_dealloc(id self) {
    IMP sup = cx_lookup(cx_class_named("NSObject"), sel_registerName("dealloc"));
    if (sup) sup(self, sel_registerName("dealloc"));
}
static void op_dealloc(id s, SEL c) {
    (void)c;
    op_t *o = OP(s);
    if (o->deps) objc_release(o->deps);
    if (o->name) objc_release(o->name);
    if (o->blocks) objc_release(o->blocks);
    if (o->target) objc_release(o->target);
    if (o->arg) objc_release(o->arg);
    if (o->result) objc_release(o->result);
    if (o->completion) _Block_release(o->completion);
    super_dealloc(s);
}
static id op_desc(id s, SEL c) {
    (void)c;
    char b[200];
    op_t *o = OP(s);
    snprintf(b, sizeof b, "<%s %p>{name = '%s'}", cx_class_name(object_getClass(s)), (void *)s, o->name ? cx_cstr(o->name) : "(null)");
    return cx_str(b);
}

/* ---- NSBlockOperation */
static id bop_with(id k, SEL c, void *b) {
    (void)c;
    id o = MSG0(id, MSG0(id, k, "alloc"), "init");
    if (b) MSG(void, o, "addExecutionBlock:", b);
    return objc_autorelease(o);
}
static void bop_add(id s, SEL c, void *b) {
    (void)c;
    op_t *o = OP(s);
    if (!b || o->state != 0) return;
    spin(&o->lock);
    if (!o->blocks) o->blocks = MSG0(id, MSG0(id, cls("NSMutableArray"), "alloc"), "init");
    void *cp = _Block_copy(b);
    cx_arr_add(o->blocks, (id)cp);
    _Block_release(cp);
    unspin(&o->lock);
}
static id bop_blocks(id s, SEL c) {
    (void)c;
    op_t *o = OP(s);
    spin(&o->lock);
    id r = o->blocks ? MSG0(id, o->blocks, "copy") : MSG0(id, MSG0(id, cls("NSArray"), "alloc"), "init");
    unspin(&o->lock);
    return objc_autorelease(r);
}
static void bop_main(id s, SEL c) {
    id blocks = bop_blocks(s, c);
    u64 n = MSG0(u64, blocks, "count");
    /* vários blocos rodam em paralelo no macOS; aqui em sequência na mesma thread, que é uma ordem válida */
    for (u64 i = 0; i < n; i++) {
        blk_t *b = (blk_t *)MSG(id, blocks, "objectAtIndex:", i);
        ((void (*)(void *))b->invoke)(b);
    }
}

/* ---- NSInvocationOperation */
static id iop_init_target(id s, SEL c, id target, SEL sel, id arg) {
    (void)c;
    op_init(s, 0);
    op_t *o = OP(s);
    if (!target || !cx_responds(object_getClass(target), sel)) { objc_release(s); return 0; }
    o->target = objc_retain(target);
    o->sel = sel;
    o->arg = arg ? objc_retain(arg) : 0;
    return s;
}
static id iop_init_inv(id s, SEL c, id inv) {
    (void)c;
    op_init(s, 0);
    OP(s)->arg = inv ? objc_retain(inv) : 0;   /* com sel 0, arg guarda a NSInvocation */
    return s;
}
static void iop_main(id s, SEL c) {
    (void)c;
    op_t *o = OP(s);
    if (o->sel && o->target) {
        const char *t = cx_method_types(object_getClass(o->target), o->sel);
        if (t && t[0] == '@') {
            id r = ((id (*)(id, SEL, id))objc_msgSend)(o->target, o->sel, o->arg);
            o->result = r ? objc_retain(r) : 0;
        } else ((void (*)(id, SEL, id))objc_msgSend)(o->target, o->sel, o->arg);
    } else if (o->arg) {
        MSG0(void, o->arg, "invoke");
    }
}
static id iop_invocation(id s, SEL c) {
    (void)c;
    op_t *o = OP(s);
    if (!o->sel) return o->arg;
    id sig = MSG(id, o->target, "methodSignatureForSelector:", o->sel);
    id inv = sig ? MSG(id, cls("NSInvocation"), "invocationWithMethodSignature:", sig) : 0;
    if (inv) {
        MSG(void, inv, "setTarget:", o->target);
        MSG(void, inv, "setSelector:", o->sel);
        if (MSG0(u64, sig, "numberOfArguments") > 2) { id a = o->arg; MSG(void, inv, "setArgument:atIndex:", (void *)&a, (i64)2); }
    }
    return inv;
}
static id iop_result(id s, SEL c) { (void)c; return OP(s)->result; }

/* ======================================================================== NSOperationQueue */

static void q_schedule(id q);
static void *q_target(q_t *q, id op) {
    if (q->is_main) return dispatch_get_main_queue();
    if (q->underlying) return q->underlying;
    i64 qos = OP(op)->qos >= 0 ? OP(op)->qos : q->qos;
    /* NSQualityOfService: 0x21 interativo, 0x19 iniciado pelo usuário, 0x11 utilidade, 0x09 fundo; o GCD usa os mesmos */
    long gq = qos == 0x21 ? 0x21 : qos == 0x19 ? 0x19 : qos == 0x11 ? 0x11 : qos == 0x09 ? 0x09 : 0;
    return dispatch_get_global_queue(gq, 0);
}
static void q_done(id q, id op) {
    q_t *qq = Q(q);
    int found = 0;
    spin(&qq->lock);
    u64 n = qq->ops ? MSG0(u64, qq->ops, "count") : 0;
    for (u64 i = 0; i < n; i++) if (MSG(id, qq->ops, "objectAtIndex:", i) == op) { found = 1; break; }
    if (found) {
        if (OP(op)->dispatched) qq->running--;
        OP(op)->dispatched = 0;
        unspin(&qq->lock);
        kvo_will(q, "operations"); kvo_will(q, "operationCount");
        spin(&qq->lock);
        MSG(void, qq->ops, "removeObjectIdenticalTo:", op);
        unspin(&qq->lock);
        kvo_did(q, "operationCount"); kvo_did(q, "operations");
        if (g_observer) {
            MSG(void, op, "removeObserver:forKeyPath:context:", g_observer, cx_str("isFinished"), (void *)q);
            MSG(void, op, "removeObserver:forKeyPath:context:", g_observer, cx_str("isReady"), (void *)q);
        }
        OP(op)->queue = 0;
        objc_release(op);
    } else unspin(&qq->lock);
    q_schedule(q);
}
typedef struct { id q, op; } run_ctx;
static void q_run(void *p) {
    run_ctx *r = p;
    id q = r->q, op = r->op;
    free(r);
    if (g_cur_key_ok) pthread_setspecific(g_cur_key, q);
    MSG0(void, op, "start");
    if (g_cur_key_ok) pthread_setspecific(g_cur_key, 0);
    /* síncrona: já terminou; assíncrona: termina quando postar isFinished (observador abaixo) */
    if (is_finished(op)) q_done(q, op);
    objc_release(q);
}
static void q_schedule(id q) {
    q_t *qq = Q(q);
    for (;;) {
        spin(&qq->lock);
        if (qq->suspended || !qq->ops) { unspin(&qq->lock); return; }
        i64 max = qq->is_main ? 1 : qq->max < 0 ? HARD_CAP : qq->max;
        if (qq->running >= max) { unspin(&qq->lock); return; }
        /* a pronta de maior prioridade, na ordem de chegada */
        id pick = 0;
        i64 best = -100;
        id snapshot = MSG0(id, qq->ops, "copy");
        unspin(&qq->lock);
        u64 n = MSG0(u64, snapshot, "count");
        for (u64 i = 0; i < n; i++) {
            id op = MSG(id, snapshot, "objectAtIndex:", i);
            if (OP(op)->dispatched || is_executing(op) || is_finished(op)) continue;
            if (!is_ready(op)) continue;
            if (OP(op)->priority > best) { best = OP(op)->priority; pick = op; }
        }
        objc_release(snapshot);
        if (!pick) return;
        spin(&qq->lock);
        if (OP(pick)->dispatched || qq->running >= max) { unspin(&qq->lock); continue; }
        OP(pick)->dispatched = 1;
        qq->running++;
        unspin(&qq->lock);
        run_ctx *r = malloc(sizeof *r);
        if (!r) return;
        r->q = objc_retain(q);
        r->op = pick;
        dispatch_async_f(q_target(qq, pick), r, q_run);
    }
}
/* observador: isFinished de uma operação da fila (context = fila) ou de uma dependência (context = operação) */
static void obs_observe(id self, SEL c, id path, id obj, id change, void *ctx) {
    (void)self; (void)c; (void)change;
    id x = (id)ctx;
    if (!x) return;
    if (cx_is_subclass(object_getClass(x), &NSOperationQueue_c)) {
        if (!strcmp(cx_cstr(path), "isFinished") && is_finished(obj)) q_done(x, obj);
        else q_schedule(x);
    } else if (cx_is_subclass(object_getClass(x), &NSOperation_c)) {
        /* dependência terminou: a dependente pode ter ficado pronta */
        kvo_will(x, "isReady");
        kvo_did(x, "isReady");
        id owner = OP(x)->queue;
        if (owner) q_schedule(owner);
    }
}

static id q_init(id s, SEL c) { (void)c; q_t *q = Q(s); q->max = DEFAULT_MAX; q->qos = -1; q->ops = MSG0(id, MSG0(id, cls("NSMutableArray"), "alloc"), "init"); return s; }
static id q_s_main(id k, SEL c) {
    (void)c;
    static id m;
    if (!m) {
        m = q_init(cx_alloc_instance((cx_class_t *)k), 0);
        Q(m)->is_main = 1;
        Q(m)->max = 1;
        Q(m)->name = objc_retain(cx_str("NSOperationQueue Main Queue"));
        Q(m)->qos = 0x21;
    }
    return m;
}
static id q_s_current(id k, SEL c) {
    id q = g_cur_key_ok ? (id)pthread_getspecific(g_cur_key) : 0;
    if (q) return q;
    return pthread_main_np() ? q_s_main(k, c) : 0;
}
static void q_add_op(id s, SEL c, id op) {
    (void)c;
    q_t *q = Q(s);
    if (!op || OP(op)->queue || is_finished(op) || is_executing(op)) return;   /* o macOS lança nesses casos */
    OP(op)->queue = s;
    kvo_will(s, "operations"); kvo_will(s, "operationCount");
    spin(&q->lock);
    cx_arr_add(q->ops, op);
    unspin(&q->lock);
    objc_retain(op);
    kvo_did(s, "operationCount"); kvo_did(s, "operations");
    if (g_observer) {
        MSG(void, op, "addObserver:forKeyPath:options:context:", g_observer, cx_str("isFinished"), (u64)0, (void *)s);
        MSG(void, op, "addObserver:forKeyPath:options:context:", g_observer, cx_str("isReady"), (u64)0, (void *)s);
    }
    q_schedule(s);
}
static void q_add_block(id s, SEL c, void *b) { q_add_op(s, c, bop_with((id)&NSBlockOperation_c, 0, b)); }
static void q_add_ops(id s, SEL c, id ops, BOOL wait) {
    u64 n = ops ? MSG0(u64, ops, "count") : 0;
    for (u64 i = 0; i < n; i++) q_add_op(s, c, MSG(id, ops, "objectAtIndex:", i));
    if (wait) for (u64 i = 0; i < n; i++) op_wait(MSG(id, ops, "objectAtIndex:", i), 0);
}
static void q_add_barrier(id s, SEL c, void *b) {
    /* barreira: depende de tudo o que já está na fila, e o que vier depois depende dela */
    q_t *q = Q(s);
    id bop = bop_with((id)&NSBlockOperation_c, 0, b);
    spin(&q->lock);
    id snap = MSG0(id, q->ops, "copy");
    unspin(&q->lock);
    u64 n = MSG0(u64, snap, "count");
    for (u64 i = 0; i < n; i++) op_add_dep(bop, 0, MSG(id, snap, "objectAtIndex:", i));
    objc_release(snap);
    q_add_op(s, c, bop);
}
static id q_ops(id s, SEL c) {
    (void)c;
    q_t *q = Q(s);
    spin(&q->lock);
    id r = MSG0(id, q->ops, "copy");
    unspin(&q->lock);
    return objc_autorelease(r);
}
static i64 q_count(id s, SEL c) { (void)c; q_t *q = Q(s); spin(&q->lock); i64 n = (i64)MSG0(u64, q->ops, "count"); unspin(&q->lock); return n; }
static void q_cancel_all(id s, SEL c) {
    id ops = q_ops(s, c);
    u64 n = MSG0(u64, ops, "count");
    for (u64 i = 0; i < n; i++) MSG0(void, MSG(id, ops, "objectAtIndex:", i), "cancel");
    q_schedule(s);
}
static void q_wait_all(id s, SEL c) {
    (void)c;
    q_t *q = Q(s);
    for (;;) {
        spin(&q->lock);
        u64 n = MSG0(u64, q->ops, "count");
        unspin(&q->lock);
        if (!n) return;
        usleep(500);
    }
}
static i64 q_max(id s, SEL c) { (void)c; return Q(s)->max; }
static void q_set_max(id s, SEL c, i64 m) {
    (void)c;
    if (Q(s)->is_main) return;
    kvo_will(s, "maxConcurrentOperationCount");
    Q(s)->max = m < 0 ? DEFAULT_MAX : m;
    kvo_did(s, "maxConcurrentOperationCount");
    q_schedule(s);
}
static BOOL q_suspended(id s, SEL c) { (void)c; return (BOOL)Q(s)->suspended; }
static void q_set_suspended(id s, SEL c, BOOL v) {
    (void)c;
    kvo_will(s, "isSuspended");
    Q(s)->suspended = v;
    kvo_did(s, "isSuspended");
    if (!v) q_schedule(s);
}
static id q_name(id s, SEL c) {
    (void)c;
    if (Q(s)->name) return Q(s)->name;
    char b[64];
    snprintf(b, sizeof b, "NSOperationQueue %p", (void *)s);
    return cx_str(b);
}
static void q_set_name(id s, SEL c, id n) { (void)c; id o = Q(s)->name; Q(s)->name = n ? MSG0(id, n, "copy") : 0; if (o) objc_release(o); }
static i64 q_qos(id s, SEL c) { (void)c; return Q(s)->qos; }
static void q_set_qos(id s, SEL c, i64 v) { (void)c; Q(s)->qos = v; }
static void *q_underlying(id s, SEL c) { (void)c; return Q(s)->is_main ? dispatch_get_main_queue() : Q(s)->underlying; }
static void q_set_underlying(id s, SEL c, void *dq) {
    (void)c;
    if (Q(s)->is_main) return;   /* a principal não aceita outra fila (o macOS lança) */
    Q(s)->underlying = dq;
}
static void q_dealloc(id s, SEL c) {
    (void)c;
    if (Q(s)->ops) objc_release(Q(s)->ops);
    if (Q(s)->name) objc_release(Q(s)->name);
    super_dealloc(s);
}

/* ======================================================================== registro */

#define CM(cl, s, f) cx_add_cmethod(&cl##_c, s, (void *)(f))
#define IM(cl, s, f) cx_add_method(&cl##_c, s, (void *)(f))

void cx_fdn_operation_init(void) {
    cx_class_t *ob = cx_class_named("NSObject");
    if (!ob || cx_class_named("NSOperationQueue") == &NSOperationQueue_c) return;
    g_cur_key_ok = pthread_key_create(&g_cur_key, 0) == 0;
    cx_define_class(&obs_c, &obs_m, "__mOSOperationQueueObserver", ob, 0);
    cx_add_method(&obs_c, "observeValueForKeyPath:ofObject:change:context:", (void *)obs_observe);
    g_observer = cx_alloc_instance(&obs_c);

    cx_define_class(&NSOperation_c, &NSOperation_m, "NSOperation", ob, (u32)(sizeof(op_t) - 8));
    IM(NSOperation, "init", op_init); IM(NSOperation, "start", op_start); IM(NSOperation, "main", op_main);
    IM(NSOperation, "cancel", op_cancel);
    IM(NSOperation, "isFinished", op_g_finished); IM(NSOperation, "isExecuting", op_g_executing);
    IM(NSOperation, "isCancelled", op_g_cancelled); IM(NSOperation, "isReady", op_g_ready);
    IM(NSOperation, "isAsynchronous", op_g_async); IM(NSOperation, "isConcurrent", op_g_async);
    IM(NSOperation, "addDependency:", op_add_dep); IM(NSOperation, "removeDependency:", op_remove_dep);
    IM(NSOperation, "dependencies", op_deps); IM(NSOperation, "waitUntilFinished", op_wait);
    IM(NSOperation, "completionBlock", op_completion); IM(NSOperation, "setCompletionBlock:", op_set_completion);
    IM(NSOperation, "queuePriority", op_priority); IM(NSOperation, "setQueuePriority:", op_set_priority);
    IM(NSOperation, "qualityOfService", op_qos); IM(NSOperation, "setQualityOfService:", op_set_qos);
    IM(NSOperation, "threadPriority", op_thread_priority); IM(NSOperation, "setThreadPriority:", op_set_thread_priority);
    IM(NSOperation, "name", op_name); IM(NSOperation, "setName:", op_set_name);
    IM(NSOperation, "description", op_desc); IM(NSOperation, "dealloc", op_dealloc);

    cx_define_class(&NSBlockOperation_c, &NSBlockOperation_m, "NSBlockOperation", &NSOperation_c, 0);
    CM(NSBlockOperation, "blockOperationWithBlock:", bop_with);
    IM(NSBlockOperation, "addExecutionBlock:", bop_add); IM(NSBlockOperation, "executionBlocks", bop_blocks);
    IM(NSBlockOperation, "main", bop_main);

    cx_define_class(&NSInvocationOperation_c, &NSInvocationOperation_m, "NSInvocationOperation", &NSOperation_c, 0);
    IM(NSInvocationOperation, "initWithTarget:selector:object:", iop_init_target);
    IM(NSInvocationOperation, "initWithInvocation:", iop_init_inv);
    IM(NSInvocationOperation, "main", iop_main); IM(NSInvocationOperation, "invocation", iop_invocation);
    IM(NSInvocationOperation, "result", iop_result);

    cx_define_class(&NSOperationQueue_c, &NSOperationQueue_m, "NSOperationQueue", ob, (u32)(sizeof(q_t) - 8));
    CM(NSOperationQueue, "mainQueue", q_s_main); CM(NSOperationQueue, "currentQueue", q_s_current);
    IM(NSOperationQueue, "init", q_init);
    IM(NSOperationQueue, "addOperation:", q_add_op); IM(NSOperationQueue, "addOperationWithBlock:", q_add_block);
    IM(NSOperationQueue, "addOperations:waitUntilFinished:", q_add_ops); IM(NSOperationQueue, "addBarrierBlock:", q_add_barrier);
    IM(NSOperationQueue, "operations", q_ops); IM(NSOperationQueue, "operationCount", q_count);
    IM(NSOperationQueue, "cancelAllOperations", q_cancel_all); IM(NSOperationQueue, "waitUntilAllOperationsAreFinished", q_wait_all);
    IM(NSOperationQueue, "maxConcurrentOperationCount", q_max); IM(NSOperationQueue, "setMaxConcurrentOperationCount:", q_set_max);
    IM(NSOperationQueue, "isSuspended", q_suspended); IM(NSOperationQueue, "setSuspended:", q_set_suspended);
    IM(NSOperationQueue, "name", q_name); IM(NSOperationQueue, "setName:", q_set_name);
    IM(NSOperationQueue, "qualityOfService", q_qos); IM(NSOperationQueue, "setQualityOfService:", q_set_qos);
    IM(NSOperationQueue, "underlyingQueue", q_underlying); IM(NSOperationQueue, "setUnderlyingQueue:", q_set_underlying);
    IM(NSOperationQueue, "dealloc", q_dealloc);
}
