/*
 * mOS libSystem — libdispatch (GCD), implementação própria.
 *
 * Modelo:
 *  - Filas raiz (globais): 6 classes de QoS, cada uma normal e "overcommit". Um pool de threads de trabalho
 *    pega o item da raiz de QoS mais alta que tiver trabalho. O pool cresce sob demanda até o número de núcleos
 *    na hora; acima disso, o monitor (thread do timer) cria mais uma thread quando a fila não anda (trabalho
 *    bloqueado), até DP_MAX_THREADS. Thread ociosa por 5 s sai.
 *  - Fila comum (serial, concorrente, workloop): lista FIFO com largura (1 serial, ilimitada concorrente). Cada item
 *    liberado vira um "envelope" na fila de destino; a hierarquia de destinos (dispatch_set_target_queue) é
 *    respeitada porque o envelope passa pelas regras da fila de destino. Barreira: só roda sozinha.
 *  - Fila principal: lista executada pela thread principal (CFRunLoop em cfrunloop.c ou dispatch_main).
 *  - dispatch_sync: caminho rápido quando a fila está livre e o destino é raiz; senão, um item de espera ocupa o
 *    lugar na fila e o bloco roda na thread de quem chamou (na fila principal, na thread principal).
 *  - Timer: uma thread com a lista de prazos (dispatch_after, fontes de timer, monitor do pool).
 *  - Contagem de referências no cabeçalho do objeto (dp_obj_t); objetos estáticos não são liberados.
 * Fontes: dispatch_source.c. Dados: dispatch_data.c. Blocos com flags: dispatch_block.c.
 */
#include "dispatch_plat.h"

typedef void (*dispatch_function_t)(void *);

#ifdef MOS_HOST_TEST
#define CLS(n) extern char n##_c[] __asm__("OBJC_CLASS_$_" #n)
#else
#define CLS(n) extern char n##_c[] __asm__("OBJC_CLASS_$_" #n)
#endif
CLS(OS_dispatch_queue_main); CLS(OS_dispatch_queue_global); CLS(OS_dispatch_queue_serial);
CLS(OS_dispatch_queue_concurrent); CLS(OS_dispatch_group); CLS(OS_dispatch_semaphore);
CLS(OS_dispatch_queue_attr); CLS(OS_dispatch_workloop);

#define DP_MAX_THREADS 255
#define QOS_UI 0x21u
#define QOS_UIN 0x19u
#define QOS_DEF 0x15u
#define QOS_UT 0x11u
#define QOS_BG 0x09u
#define QOS_MT 0x05u

/* ------------------------------------------------------------------ itens */
enum { IT_BLOCK = 1, IT_BARRIER = 2, IT_SYNC = 4, IT_WRAP = 8, IT_APPLY = 16 };
/* registro de um dispatch_sync: no heap, com duas referências (quem chamou e quem executa o item) */
typedef struct dp_sync { volatile int go, done, ref; int run_here; dispatch_function_t fn; void *ctx; } dp_sync_t;
static void sync_put(dp_sync_t *s) { if (__sync_sub_and_fetch(&s->ref, 1) == 0) free(s); }
typedef struct dp_item {
    struct dp_item *next;
    dispatch_function_t fn;          /* NULL com IT_BLOCK: ctx é o bloco (copiado) */
    void *ctx;
    unsigned flags, qos;
    struct dp_group *group;
    struct dp_queue *owner;          /* IT_WRAP: a fila de onde o item interno saiu */
    struct dp_item *inner;
    dp_sync_t *sync;
} dp_item_t;

static dp_item_t *it_new(void) { dp_item_t *i = calloc(1, sizeof *i); if (!i) abort(); return i; }

/* ------------------------------------------------------------------ filas */
enum { QF_ROOT = 1, QF_MAIN = 2, QF_OVERCOMMIT = 4, QF_WORKLOOP = 8 };
typedef struct dp_spec { struct dp_spec *next; const void *key; void *ctx; void (*dtor)(void *); } dp_spec_t;
typedef struct dp_queue {
    dp_obj_t h;
    const char *label;
    char *label_owned;
    int width;
    unsigned qos;
    int rel;
    unsigned flags;
    dp_lock_t lock;
    dp_item_t *head, *tail;
    int inflight;
    int barrier;
    dp_spec_t *spec;
    volatile int nqueued;            /* raízes: itens esperando (monitor do pool) */
} dp_queue_t;

typedef struct { dp_obj_t h; int concurrent; unsigned qos; int rel; int inactive; unsigned long arf; } dp_attr_t;

/* raízes: índice = 2 * classe + overcommit; classe 0 = mais alta */
static const unsigned root_qos[6] = {QOS_UI, QOS_UIN, QOS_DEF, QOS_UT, QOS_BG, QOS_MT};
static const char *const root_label[12] = {
    "com.apple.root.user-interactive-qos", "com.apple.root.user-interactive-qos.overcommit",
    "com.apple.root.user-initiated-qos", "com.apple.root.user-initiated-qos.overcommit",
    "com.apple.root.default-qos", "com.apple.root.default-qos.overcommit",
    "com.apple.root.utility-qos", "com.apple.root.utility-qos.overcommit",
    "com.apple.root.background-qos", "com.apple.root.background-qos.overcommit",
    "com.apple.root.maintenance-qos", "com.apple.root.maintenance-qos.overcommit"};
#define ROOT_INIT(i) {{OS_dispatch_queue_global_c, DP_MAGIC, DK_QUEUE, -1, 0, 0, 0, 0, 0, 0}, 0, 0, 0x7fffffff, 0, 0, \
                      QF_ROOT | ((i) & 1 ? QF_OVERCOMMIT : 0), 0, 0, 0, 0, 0, 0, 0}
static dp_queue_t roots[12] = {ROOT_INIT(0), ROOT_INIT(1), ROOT_INIT(2), ROOT_INIT(3), ROOT_INIT(4), ROOT_INIT(5),
                               ROOT_INIT(6), ROOT_INIT(7), ROOT_INIT(8), ROOT_INIT(9), ROOT_INIT(10), ROOT_INIT(11)};
/* dispatch_get_main_queue() = &_dispatch_main_q, como no macOS */
dp_queue_t _dispatch_main_q = {{OS_dispatch_queue_main_c, DP_MAGIC, DK_QUEUE, -1, 0, 0, 0, 0, 0, 0},
                               "com.apple.main-thread", 0, 1, QOS_UIN, 0, QF_MAIN, 0, 0, 0, 0, 0, 0, 0};
/* DISPATCH_QUEUE_CONCURRENT = &_dispatch_queue_attr_concurrent */
dp_attr_t _dispatch_queue_attr_concurrent = {{OS_dispatch_queue_attr_c, DP_MAGIC, DK_ATTR, -1, 0, 0, 0, 0, 0, 0}, 1, 0, 0, 0, 0};

static void roots_init(void) {
    static volatile int done;
    if (done) return;
    for (int i = 0; i < 12; i++) { roots[i].label = root_label[i]; roots[i].qos = root_qos[i / 2]; }
    __sync_synchronize();
    done = 1;
}
static int qos_class_index(unsigned qos) {
    if (qos >= QOS_UI) return 0;
    if (qos >= QOS_UIN) return 1;
    if (qos >= QOS_DEF || qos == 0) return 2;
    if (qos >= QOS_UT) return 3;
    if (qos >= QOS_BG) return 4;
    return 5;
}
static dp_queue_t *root_for(unsigned qos, int overcommit) { roots_init(); return &roots[2 * qos_class_index(qos) + (overcommit ? 1 : 0)]; }
void *dp_default_target(void) { return root_for(QOS_DEF, 1); }

/* ------------------------------------------------------------------ objetos */
static int is_obj(void *o) { return o && ((dp_obj_t *)o)->magic == DP_MAGIC; }
int cx_dispatch_is_obj(void *o) { return is_obj(o); }
void dp_obj_init(void *o, void *isa, int kind, void (*dispose)(dp_obj_t *)) {
    dp_obj_t *h = o;
    h->isa = isa; h->magic = DP_MAGIC; h->kind = (unsigned short)kind; h->ref = 0;
    h->ctx = 0; h->fin = 0; h->tq = 0; h->susp = 0; h->inactive = 0; h->dispose = dispose;
}

/* objetos que não têm o cabeçalho dp_obj_t são os de dados (NSData/CFData, dispatch_data.c): contagem do ObjC */
#ifdef MOS_HOST_TEST
void *dp_host_data_retain(void *d);
void dp_host_data_release(void *d);
#define FOREIGN_RETAIN dp_host_data_retain
#define FOREIGN_RELEASE dp_host_data_release
#else
void *objc_retain(void *o);
void objc_release(void *o);
#define FOREIGN_RETAIN objc_retain
#define FOREIGN_RELEASE objc_release
#endif

static void obj_free(dp_obj_t *h) {
    if (h->fin) {                                   /* o finalizador roda na fila de destino, como no macOS */
        void (*fin)(void *) = h->fin;
        h->fin = 0;
        if (h->tq) dispatch_async_f(h->tq, h->ctx, fin);
        else fin(h->ctx);
    }
    if (h->dispose) h->dispose(h);
    if (h->tq) cx_dispatch_release(h->tq);
    h->magic = 0;
    free(h);
}
void cx_dispatch_retain(void *o) {
    if (!o) return;
    if (is_obj(o)) { if (((dp_obj_t *)o)->ref >= 0) __sync_fetch_and_add(&((dp_obj_t *)o)->ref, 1); return; }
    FOREIGN_RETAIN(o);
}
void cx_dispatch_release(void *o) {
    if (!o) return;
    if (is_obj(o)) {
        dp_obj_t *h = o;
        if (h->ref < 0) return;
        if (__sync_fetch_and_sub(&h->ref, 1) == 0) obj_free(h);
        return;
    }
    FOREIGN_RELEASE(o);
}
long cx_dispatch_retain_count(void *o) {
    if (!o) return 0;
    if (is_obj(o)) return ((dp_obj_t *)o)->ref < 0 ? 0x7FFFFFFF : 1 + ((dp_obj_t *)o)->ref;
    return 1;
}
void dispatch_retain(void *o) { cx_dispatch_retain(o); }
void dispatch_release(void *o) { cx_dispatch_release(o); }

/* contexto e finalizador: no cabeçalho; objetos de fora numa tabela */
#define DCTX_MAX 256
static struct { void *obj, *ctx; dispatch_function_t fin; } dctx[DCTX_MAX];
static dp_lock_t dctx_lock;
static int dctx_slot(void *o, int make) {
    for (int i = 0; i < DCTX_MAX; i++) if (dctx[i].obj == o) return i;
    if (!make) return -1;
    for (int i = 0; i < DCTX_MAX; i++) if (!dctx[i].obj) { dctx[i].obj = o; return i; }
    return -1;
}
void dispatch_set_context(void *o, void *ctx) {
    if (!o) return;
    if (is_obj(o)) { if (((dp_obj_t *)o)->ref >= 0) ((dp_obj_t *)o)->ctx = ctx; return; }
    dp_lock(&dctx_lock); int i = dctx_slot(o, 1); if (i >= 0) dctx[i].ctx = ctx; dp_unlock(&dctx_lock);
}
void *dispatch_get_context(void *o) {
    if (!o) return 0;
    if (is_obj(o)) return ((dp_obj_t *)o)->ctx;
    dp_lock(&dctx_lock); int i = dctx_slot(o, 0); void *c = i >= 0 ? dctx[i].ctx : 0; dp_unlock(&dctx_lock);
    return c;
}
void dispatch_set_finalizer_f(void *o, dispatch_function_t f) {
    if (!o) return;
    if (is_obj(o)) { if (((dp_obj_t *)o)->ref >= 0) ((dp_obj_t *)o)->fin = f; return; }
    dp_lock(&dctx_lock); int i = dctx_slot(o, 1); if (i >= 0) dctx[i].fin = f; dp_unlock(&dctx_lock);
}

/* ------------------------------------------------------------------ fila em execução por thread */
typedef struct dp_frame { dp_queue_t *q; struct dp_frame *up; } dp_frame_t;
#define TH_SLOTS 2048
static struct { volatile int tid; dp_frame_t *top; } th_tab[TH_SLOTS];
static dp_lock_t th_lock;
static int th_find(int tid, int make) {
    unsigned h = (unsigned)tid * 2654435761u % TH_SLOTS;
    int tomb = -1;
    for (int n = 0; n < TH_SLOTS; n++, h = (h + 1) % TH_SLOTS) {
        int t = th_tab[h].tid;
        if (t == tid) return (int)h;
        if (t == -1 && tomb < 0) tomb = (int)h;
        if (t == 0) {
            if (!make) return -1;
            dp_lock(&th_lock);
            int r = tomb >= 0 ? tomb : (int)h;
            if (th_tab[r].tid != 0 && th_tab[r].tid != -1) { dp_unlock(&th_lock); return th_find(tid, make); }
            th_tab[r].top = 0;
            __sync_synchronize();
            th_tab[r].tid = tid;
            dp_unlock(&th_lock);
            return r;
        }
    }
    return -1;
}
static dp_frame_t *frame_top(void) { int i = th_find(dp_tid(), 0); return i >= 0 ? th_tab[i].top : 0; }
static void frame_push(dp_frame_t *f, dp_queue_t *q) {
    int i = th_find(dp_tid(), 1);
    f->q = q;
    f->up = i >= 0 ? th_tab[i].top : 0;
    if (i >= 0) th_tab[i].top = f;
}
static void frame_pop(dp_frame_t *f) {
    int i = th_find(dp_tid(), 0);
    if (i < 0) return;
    th_tab[i].top = f->up;
    if (!f->up) th_tab[i].tid = -1;                 /* sem fila: o lugar vira lápide (só a dona escreve nele) */
}
/* fila atual: a do quadro de cima; sem quadro, a principal na thread principal */
static dp_queue_t *current_queue(void) {
    dp_frame_t *f = frame_top();
    if (f) return f->q;
    return dp_is_main() ? &_dispatch_main_q : 0;
}
void *cx_dispatch_current(void) { return current_queue(); }

/* ------------------------------------------------------------------ execução de um item */
static void group_leave_internal(struct dp_group *g);
static void item_invoke(dp_item_t *it) {
    if (it->flags & IT_BLOCK) { DP_RUN_BLOCK(it->ctx); _Block_release(it->ctx); }
    else it->fn(it->ctx);
    if (it->group) group_leave_internal(it->group);
}
static void q_push(dp_queue_t *q, dp_item_t *it);
static void q_wakeup(dp_queue_t *q);

/* item de dispatch_sync chegou à vez: libera quem espera e segura o lugar até ele terminar */
static void sync_handoff(dp_sync_t *s) {
    if (s->run_here) {                 /* fila presa à thread principal: o bloco roda aqui */
        s->fn(s->ctx);
        s->done = 1;
        dp_wake(&s->done, 1);
        sync_put(s);
        return;
    }
    s->go = 1;
    dp_wake(&s->go, 1);
    while (!s->done) dp_wait(&s->done, 0, -1);
    sync_put(s);
}
static void q_complete(dp_queue_t *q, unsigned flags) {
    dp_lock(&q->lock);
    q->inflight--;
    if (flags & IT_BARRIER) q->barrier = 0;
    dp_unlock(&q->lock);
    q_wakeup(q);
    cx_dispatch_release(q);
}
static void run_item(dp_queue_t *q, dp_item_t *it) {
    if (it->flags & IT_WRAP) {
        dp_queue_t *cq = it->owner;
        dp_item_t *x = it->inner;
        dp_frame_t f;
        frame_push(&f, cq);
        if (x->flags & IT_SYNC) sync_handoff(x->sync);
        else run_item(cq, x);
        frame_pop(&f);
        unsigned fl = x->flags;
        free(x);
        q_complete(cq, fl);
        return;
    }
    (void)q;
    if (it->flags & IT_SYNC) sync_handoff(it->sync);
    else item_invoke(it);
}

/* ------------------------------------------------------------------ pool de threads */
static dp_lock_t pool_lock;
static volatile int pool_seq, pool_idle, pool_threads, pool_progress;
static int pool_ncpu;
static volatile int worker_tids[DP_MAX_THREADS + 1];   /* vaga do pool: tid da thread, -1 reservada, 0 livre */
static dp_item_t *pool_pop(dp_queue_t **from) {
    for (int i = 0; i < 12; i++) {
        dp_queue_t *r = &roots[i];
        if (!r->head) continue;
        dp_lock(&r->lock);
        dp_item_t *it = r->head;
        if (it) { r->head = it->next; if (!r->head) r->tail = 0; r->nqueued--; }
        dp_unlock(&r->lock);
        if (it) { *from = r; return it; }
    }
    return 0;
}
static void *worker_main(void *arg) {
    int slot = (int)(long)arg;
    worker_tids[slot] = dp_tid();
    unsigned cur_qos = 0;
    for (;;) {
        dp_queue_t *r;
        dp_item_t *it = pool_pop(&r);
        if (it) {
            if (r->qos != cur_qos) { cur_qos = r->qos; dp_set_qos(cur_qos); }   /* a thread herda a QoS da raiz */
            dp_frame_t f;
            frame_push(&f, r);
            run_item(r, it);
            frame_pop(&f);
            free(it);
            __sync_fetch_and_add(&pool_progress, 1);
            continue;
        }
        int seq = pool_seq;
        __sync_fetch_and_add(&pool_idle, 1);
        int got = 0;
        for (int i = 0; i < 12; i++) if (roots[i].head) got = 1;
        u64 t0 = dp_now_ns();
        if (!got) dp_wait(&pool_seq, seq, 5000);
        __sync_fetch_and_sub(&pool_idle, 1);
        if (pool_seq == seq && dp_now_ns() - t0 >= 4900000000ull && pool_threads > pool_ncpu) {
            dp_lock(&pool_lock);
            int bye = pool_threads > pool_ncpu;
            int any = 0;
            for (int i = 0; i < 12; i++) if (roots[i].head) any = 1;
            if (bye && !any) { pool_threads--; worker_tids[slot] = 0; }
            dp_unlock(&pool_lock);
            if (bye && !any) return 0;
        }
    }
}
static void pool_spawn_locked(void) {
    int slot = -1;
    for (int i = 0; i <= DP_MAX_THREADS; i++) if (!worker_tids[i]) { slot = i; break; }
    if (slot < 0) return;
    worker_tids[slot] = -1;                       /* reservado até a thread se registrar */
    pool_threads++;
    if (dp_thread(worker_main, (void *)(long)slot) != 0) { pool_threads--; worker_tids[slot] = 0; }
}
static void pool_monitor_arm(void);
static void pool_push(dp_queue_t *r, dp_item_t *it) {
    it->next = 0;
    dp_lock(&r->lock);
    if (r->tail) r->tail->next = it; else r->head = it;
    r->tail = it;
    r->nqueued++;
    dp_unlock(&r->lock);
    __sync_fetch_and_add(&pool_seq, 1);
    if (pool_idle > 0) { dp_wake(&pool_seq, 1); return; }
    dp_lock(&pool_lock);
    if (!pool_ncpu) pool_ncpu = dp_ncpu() < 2 ? 2 : dp_ncpu();
    if (pool_threads < pool_ncpu || (r->flags & QF_OVERCOMMIT && pool_threads < DP_MAX_THREADS && pool_idle == 0 &&
                                     pool_threads < 4 * pool_ncpu))
        pool_spawn_locked();
    dp_unlock(&pool_lock);
    pool_monitor_arm();
}

/* ------------------------------------------------------------------ fila principal */
static volatile int main_seq;
volatile int cx_dispatch_main_used;   /* cfrunloop.c: a fila principal é fonte do run loop principal */
volatile int cx_main_queue_live;      /* cfrunloop.c liga quando o CFRunLoop principal roda */
/* Porta da fila principal para quem roda o próprio laço Mach (o CFRunLoop da Apple, e apps que imitam):
 * _dispatch_get_main_queue_port_4CF cria uma porta de recepção; cada bloco que chega na fila manda uma
 * mensagem vazia para ela (uma só pendente de cada vez), e quem a recebe chama _dispatch_main_queue_callback_4CF,
 * que roda a fila e manda outra se ainda sobrou trabalho. O CFRunLoop do mOS não precisa dela: drena direto. */
static volatile unsigned main_port;
static volatile int main_port_sent;
#ifdef MOS_HOST_TEST
static void main_port_poke(void) {}
unsigned _dispatch_get_main_queue_port_4CF(void) { return 0; }
#else
typedef struct { unsigned bits, size, remote, local, voucher; int id; } dp_mach_hdr_t;
int mach_msg(void *msg, int option, unsigned send_size, unsigned rcv_size, unsigned rcv_name, unsigned timeout, unsigned notify);
int mach_port_allocate(unsigned task, unsigned right, unsigned *name);
int mach_port_insert_right(unsigned task, unsigned name, unsigned poly, unsigned poly_type);
extern unsigned mach_task_self_;
static void main_port_poke(void) {
    unsigned port = main_port;
    if (!port || !__sync_bool_compare_and_swap(&main_port_sent, 0, 1)) return;
    /* remoto com MACH_MSG_TYPE_COPY_SEND (19); MACH_SEND_MSG|MACH_SEND_TIMEOUT com 0 ms: nunca bloqueia */
    dp_mach_hdr_t h = {19, sizeof h, port, 0, 0, 0x4d51};   /* id: "MQ" */
    if (mach_msg(&h, 0x1 | 0x10, sizeof h, 0, 0, 0, 0) != 0) main_port_sent = 0;
}
unsigned _dispatch_get_main_queue_port_4CF(void) {
    if (!main_port) {
        unsigned name = 0;
        if (mach_port_allocate(mach_task_self_, 1 /* RECEIVE */, &name) != 0) return 0;
        mach_port_insert_right(mach_task_self_, name, name, 20 /* MAKE_SEND */);
        if (!__sync_bool_compare_and_swap(&main_port, 0, name)) return main_port;   /* outra thread chegou antes */
        cx_dispatch_main_used = 1;
        if (_dispatch_main_q.head) main_port_poke();
    }
    return main_port;
}
#endif
static void main_push(dp_item_t *it) {
    dp_queue_t *m = &_dispatch_main_q;
    cx_dispatch_main_used = 1;
    it->next = 0;
    dp_lock(&m->lock);
    if (m->tail) m->tail->next = it; else m->head = it;
    m->tail = it;
    dp_unlock(&m->lock);
    __sync_fetch_and_add(&main_seq, 1);
    dp_wake(&main_seq, 1);
    dp_main_wakeup();
    main_port_poke();
}
int cx_dispatch_pending(void) { return _dispatch_main_q.head != 0 && !_dispatch_main_q.h.susp; }
/* executa o que estiver na fila principal (só na thread principal); devolve quantos itens rodaram */
static volatile int main_draining;
int cx_dispatch_drain_main(void) {
    dp_queue_t *m = &_dispatch_main_q;
    int ran = 0;
    if (!dp_is_main()) return 0;
    for (int guard = 0; guard < 1024; guard++) {
        if (m->h.susp) break;
        dp_lock(&m->lock);
        dp_item_t *it = m->head;
        if (it) { m->head = it->next; if (!m->head) m->tail = 0; }
        dp_unlock(&m->lock);
        if (!it) break;
        dp_frame_t f;
        frame_push(&f, m);
        run_item(m, it);
        frame_pop(&f);
        free(it);
        ran++;
    }
    return ran;
}
/* a mensagem da porta já foi recebida por quem chama; roda a fila e, se sobrou, avisa de novo */
void _dispatch_main_queue_callback_4CF(void *msg) {
    (void)msg;
    main_port_sent = 0;
    cx_main_queue_live = 1;
    cx_dispatch_drain_main();
    if (cx_dispatch_pending()) main_port_poke();
}
void cx_dispatch_drain(void) { if (dp_is_main() && !main_draining) { main_draining = 1; cx_dispatch_drain_main(); main_draining = 0; } }
/* dispatch_main: a thread principal vira o laço da fila principal e não volta */
__attribute__((noreturn)) void dispatch_main(void) {
    cx_main_queue_live = 1;
    cx_dispatch_main_used = 1;
    for (;;) {
        int seq = main_seq;
        if (!cx_dispatch_drain_main()) dp_wait(&main_seq, seq, 1000);
    }
}

/* ------------------------------------------------------------------ fila comum */
static void q_push(dp_queue_t *q, dp_item_t *it) {
    if (q->flags & QF_ROOT) { pool_push(q, it); return; }
    if (q->flags & QF_MAIN) { main_push(it); return; }
    it->next = 0;
    cx_dispatch_retain(q);                                 /* o item segura a fila até completar */
    dp_lock(&q->lock);
    if (q->flags & QF_WORKLOOP) {                          /* workloop: QoS mais alta primeiro, FIFO dentro da classe */
        dp_item_t **pp = &q->head, *prev = 0;
        while (*pp && (*pp)->qos >= it->qos) { prev = *pp; pp = &(*pp)->next; }
        it->next = *pp;
        *pp = it;
        if (!it->next) q->tail = it;
        (void)prev;
    } else {
        if (q->tail) q->tail->next = it; else q->head = it;
        q->tail = it;
    }
    dp_unlock(&q->lock);
    q_wakeup(q);
}
static void q_wakeup(dp_queue_t *q) {
    dp_item_t *go = 0, **gt = &go;
    dp_lock(&q->lock);
    while (q->head && !q->h.susp && !q->h.inactive) {
        dp_item_t *it = q->head;
        if (it->flags & IT_BARRIER) {
            if (q->inflight) break;
            q->barrier = 1;
        } else {
            if (q->barrier || q->inflight >= q->width) break;
        }
        q->head = it->next;
        if (!q->head) q->tail = 0;
        q->inflight++;
        it->next = 0;
        *gt = it;
        gt = &it->next;
        if (it->flags & IT_BARRIER) break;
    }
    dp_unlock(&q->lock);
    dp_queue_t *t = q->h.tq ? q->h.tq : dp_default_target();
    while (go) {
        dp_item_t *x = go;
        go = go->next;
        x->next = 0;
        dp_item_t *w = it_new();
        w->flags = IT_WRAP;
        w->owner = q;
        w->inner = x;
        w->qos = x->qos;
        q_push(t, w);
    }
}

static dp_item_t *item_make(void *ctx, dispatch_function_t fn, void *block, unsigned flags) {
    dp_item_t *it = it_new();
    if (block) { it->ctx = _Block_copy(block); it->flags = IT_BLOCK | flags; }
    else { it->fn = fn; it->ctx = ctx; it->flags = flags; }
    return it;
}
static unsigned block_flags(void *block);
static unsigned q_effective_qos(dp_queue_t *q) {
    for (int n = 0; q && n < 32; n++, q = q->h.tq) if (q->qos) return q->qos;
    return QOS_DEF;
}
static void q_async(void *qq, void *ctx, dispatch_function_t fn, void *block, unsigned flags) {
    dp_queue_t *q = qq ? qq : &_dispatch_main_q;
    if (block) flags |= block_flags(block);
    dp_item_t *it = item_make(ctx, fn, block, flags);
    it->qos = q_effective_qos(q);
    q_push(q, it);
}

/* a fila (ou um destino dela) já está na pilha desta thread: dispatch_sync nela travaria; o macOS aborta, aqui roda direto */
static int queue_in_chain(dp_queue_t *want, dp_queue_t *q) {
    for (int n = 0; q && n < 64; n++, q = q->h.tq ? q->h.tq : ((q->flags & (QF_ROOT | QF_MAIN)) ? 0 : dp_default_target()))
        if (q == want) return 1;
    return 0;
}
static int on_queue(dp_queue_t *want) {
    for (dp_frame_t *f = frame_top(); f; f = f->up) if (queue_in_chain(want, f->q)) return 1;
    if (want == &_dispatch_main_q && dp_is_main()) return 1;
    return 0;
}
static int bound_to_main(dp_queue_t *q) {
    for (int n = 0; q && n < 64; n++, q = q->h.tq) if (q->flags & QF_MAIN) return 1;
    return 0;
}
static void sync_run(dp_queue_t *q, void *ctx, dispatch_function_t fn) {
    dp_frame_t f;
    frame_push(&f, q);
    fn(ctx);
    frame_pop(&f);
}
static void blk_tramp(void *b) { DP_RUN_BLOCK(b); }
static void q_sync(void *qq, void *ctx, dispatch_function_t fn, void *block, unsigned flags) {
    dp_queue_t *q = qq ? qq : &_dispatch_main_q;
    if (block) { ctx = block; fn = blk_tramp; flags |= block_flags(block) & IT_BARRIER; }
    if (q->flags & QF_ROOT) { sync_run(q, ctx, fn); return; }
    if (on_queue(q)) { sync_run(q, ctx, fn); return; }
    /* caminho rápido: fila livre cujo destino é uma raiz */
    dp_queue_t *t = q->h.tq ? q->h.tq : dp_default_target();
    if (!(q->flags & QF_MAIN) && (t->flags & QF_ROOT)) {
        int ok = 0;
        dp_lock(&q->lock);
        if (!q->head && !q->h.susp && !q->h.inactive && !q->barrier &&
            ((flags & IT_BARRIER) ? q->inflight == 0 : q->inflight < q->width)) {
            q->inflight++;
            if (flags & IT_BARRIER) q->barrier = 1;
            ok = 1;
        }
        dp_unlock(&q->lock);
        if (ok) {
            cx_dispatch_retain(q);
            sync_run(q, ctx, fn);
            q_complete(q, flags);
            return;
        }
    }
    /* fila presa à principal: o bloco roda na thread principal (quem executa o item); da própria thread principal,
     * ela escoa a fila principal até chegar a vez dele */
    dp_sync_t *s = malloc(sizeof *s);
    if (!s) abort();
    s->go = 0; s->done = 0; s->ref = 2; s->fn = fn; s->ctx = ctx;
    s->run_here = bound_to_main(q);
    dp_item_t *it = it_new();
    it->flags = IT_SYNC | (flags & IT_BARRIER);
    it->sync = s;
    it->qos = q_effective_qos(q);
    q_push(q, it);
    if (s->run_here) {
        while (!s->done) {
            if (dp_is_main()) { int seq = main_seq; if (!cx_dispatch_drain_main() && !s->done) dp_wait(&main_seq, seq, 10); }
            else dp_wait(&s->done, 0, -1);
        }
        sync_put(s);
        return;
    }
    while (!s->go) dp_wait(&s->go, 0, -1);
    sync_run(q, ctx, fn);
    s->done = 1;
    dp_wake(&s->done, 1);
    sync_put(s);
}

void dispatch_async_f(void *q, void *ctx, dispatch_function_t fn) { q_async(q, ctx, fn, 0, 0); }
void dispatch_async(void *q, void *block) { q_async(q, 0, 0, block, 0); }
void dispatch_barrier_async_f(void *q, void *ctx, dispatch_function_t fn) { q_async(q, ctx, fn, 0, IT_BARRIER); }
void dispatch_barrier_async(void *q, void *block) { q_async(q, 0, 0, block, IT_BARRIER); }
void dispatch_sync_f(void *q, void *ctx, dispatch_function_t fn) { q_sync(q, ctx, fn, 0, 0); }
void dispatch_sync(void *q, void *block) { q_sync(q, 0, 0, block, 0); }
void dispatch_barrier_sync_f(void *q, void *ctx, dispatch_function_t fn) { q_sync(q, ctx, fn, 0, IT_BARRIER); }
void dispatch_barrier_sync(void *q, void *block) { q_sync(q, 0, 0, block, IT_BARRIER); }
void dispatch_async_and_wait_f(void *q, void *ctx, dispatch_function_t fn) { q_sync(q, ctx, fn, 0, 0); }
void dispatch_async_and_wait(void *q, void *block) { q_sync(q, 0, 0, block, 0); }
void dispatch_barrier_async_and_wait_f(void *q, void *ctx, dispatch_function_t fn) { q_sync(q, ctx, fn, 0, IT_BARRIER); }
void dispatch_barrier_async_and_wait(void *q, void *block) { q_sync(q, 0, 0, block, IT_BARRIER); }

/* job do Swift Concurrency: a metadata é uma DispatchClassMetadata (swift/ABI/Metadata.h); o invoke fica 48 bytes
 * depois do ponto de endereço (kind, opaque, opaqueObjC[3], vtableType, vtableInvoke) */
typedef void (*dp_swift_invoke_t)(void *job, void *ctx, u32 flags);
static void run_swift_job(void *job) {
    u8 *md = *(u8 **)job;
    ((dp_swift_invoke_t) * (void **)(md + 48))(job, 0, 0);
}
void dispatch_async_swift_job(void *q, void *job, u32 qos) {
    dp_queue_t *x = q ? q : &_dispatch_main_q;
    dp_item_t *it = item_make(job, run_swift_job, 0, 0);
    it->qos = qos ? qos : q_effective_qos(x);
    q_push(x, it);
}

/* ------------------------------------------------------------------ criação e atributos de filas */
void *dispatch_get_main_queue(void) { cx_dispatch_main_used = 1; return &_dispatch_main_q; }
void *dispatch_get_global_queue(long id, unsigned long flags) {
    unsigned qos;
    switch (id) {
    case 2: qos = QOS_UIN; break;                 /* DISPATCH_QUEUE_PRIORITY_HIGH */
    case 0: qos = QOS_DEF; break;
    case -2: qos = QOS_UT; break;                 /* LOW */
    case -32768: qos = QOS_BG; break;             /* BACKGROUND */
    case QOS_UI: case QOS_UIN: case QOS_DEF: case QOS_UT: case QOS_BG: case QOS_MT: qos = (unsigned)id; break;
    default:
        /* QoS intermediária (prioridade própria de uma Task do Swift): a classe imediatamente abaixo; o resto é inválido */
        if (id > 0 && id < 0x22) { qos = (unsigned)id; break; }
        return 0;
    }
    return root_for(qos, (flags & 2) != 0);       /* DISPATCH_QUEUE_OVERCOMMIT */
}
static int is_attr(void *a) { return is_obj(a) && ((dp_obj_t *)a)->kind == DK_ATTR; }
static void *attr_get(int conc, unsigned qos, int rel, int inactive, unsigned long arf) {
    if (conc && !qos && !inactive && !arf) return &_dispatch_queue_attr_concurrent;
    if (!conc && !qos && !inactive && !arf) return 0;
    static dp_attr_t *tab[128];
    static dp_lock_t lk;
    dp_attr_t *r = 0;
    dp_lock(&lk);
    for (int i = 0; i < 128 && tab[i] && !r; i++)
        if (tab[i]->concurrent == conc && tab[i]->qos == qos && tab[i]->rel == rel && tab[i]->inactive == inactive && tab[i]->arf == arf) r = tab[i];
    for (int i = 0; i < 128 && !r; i++)
        if (!tab[i]) {
            r = tab[i] = calloc(1, sizeof *r);
            if (!r) break;
            dp_obj_init(r, OS_dispatch_queue_attr_c, DK_ATTR, 0);
            r->h.ref = -1;
            r->concurrent = conc; r->qos = qos; r->rel = rel; r->inactive = inactive; r->arf = arf;
        }
    dp_unlock(&lk);
    return r;
}
static void attr_read(void *a, int *conc, unsigned *qos, int *rel, int *inactive, unsigned long *arf) {
    *conc = 0; *qos = 0; *rel = 0; *inactive = 0; *arf = 0;
    if (!is_attr(a)) return;
    dp_attr_t *x = a;
    *conc = x->concurrent; *qos = x->qos; *rel = x->rel; *inactive = x->inactive; *arf = x->arf;
}
void *dispatch_queue_attr_make_with_qos_class(void *attr, unsigned qos, int rel) {
    int c, in; unsigned q; int r; unsigned long arf;
    attr_read(attr, &c, &q, &r, &in, &arf);
    if (qos != 0 && qos != QOS_UI && qos != QOS_UIN && qos != QOS_DEF && qos != QOS_UT && qos != QOS_BG && qos != QOS_MT) return attr;
    if (rel > 0 || rel < -15) return attr;
    return attr_get(c, qos, rel, in, arf);
}
void *dispatch_queue_attr_make_initially_inactive(void *attr) {
    int c, in; unsigned q; int r; unsigned long arf;
    attr_read(attr, &c, &q, &r, &in, &arf);
    return attr_get(c, q, r, 1, arf);
}
void *dispatch_queue_attr_make_with_autorelease_frequency(void *attr, unsigned long f) {
    int c, in; unsigned q; int r; unsigned long arf;
    attr_read(attr, &c, &q, &r, &in, &arf);
    if (f > 2) return attr;
    return attr_get(c, q, r, in, f);
}
static void queue_dispose(dp_obj_t *h) {
    dp_queue_t *q = (dp_queue_t *)h;
    dp_spec_t *s = q->spec;
    q->spec = 0;
    while (s) { dp_spec_t *n = s->next; if (s->dtor && s->ctx) s->dtor(s->ctx); free(s); s = n; }
    free(q->label_owned);
}
static dp_queue_t *queue_new(const char *label, void *attr, void *target, int workloop) {
    int conc, inactive; unsigned qos; int rel; unsigned long arf;
    attr_read(attr, &conc, &qos, &rel, &inactive, &arf);
    dp_queue_t *q = calloc(1, sizeof *q);
    if (!q) return 0;
    dp_obj_init(q, workloop ? (void *)OS_dispatch_workloop_c : conc ? (void *)OS_dispatch_queue_concurrent_c : (void *)OS_dispatch_queue_serial_c,
                DK_QUEUE, queue_dispose);
    if (label) { q->label_owned = strdup(label); q->label = q->label_owned; } else q->label = "";
    q->width = conc && !workloop ? 0x7fffffff : 1;
    q->qos = qos;
    q->rel = rel;
    q->flags = workloop ? QF_WORKLOOP : 0;
    q->h.inactive = inactive;
    if (target) { cx_dispatch_retain(target); q->h.tq = target; }
    else q->h.tq = root_for(qos ? qos : QOS_DEF, !conc);   /* filas seriais vão para a raiz overcommit, como no macOS */
    return q;
}
void *dispatch_queue_create(const char *label, void *attr) { return queue_new(label, attr, 0, 0); }
void *dispatch_queue_create_with_target(const char *label, void *attr, void *target) { return queue_new(label, attr, target, 0); }
void *dispatch_queue_create_with_target_V2(const char *label, void *attr, void *target) __asm__("dispatch_queue_create_with_target$V2");
void *dispatch_queue_create_with_target_V2(const char *label, void *attr, void *target) { return queue_new(label, attr, target, 0); }
void *dispatch_workloop_create(const char *label) { return queue_new(label, 0, 0, 1); }
void *dispatch_workloop_create_inactive(const char *label) { return queue_new(label, dispatch_queue_attr_make_initially_inactive(0), 0, 1); }
void dispatch_workloop_set_autorelease_frequency(void *w, unsigned long f) { (void)w; (void)f; }
void dispatch_workloop_set_os_workgroup(void *w, void *wg) { (void)w; (void)wg; }
void dispatch_workloop_set_scheduler_priority(void *w, int prio, unsigned long flags) { (void)flags; if (is_obj(w)) ((dp_queue_t *)w)->rel = prio; }
const char *dispatch_queue_get_label(void *q) {
    if (!q) { dp_queue_t *c = current_queue(); return c ? c->label : ""; }
    return ((dp_queue_t *)q)->label ? ((dp_queue_t *)q)->label : "";
}
unsigned dispatch_queue_get_qos_class(void *q, int *rel) {
    dp_queue_t *c = q;
    if (!is_obj(c) || c->h.kind != DK_QUEUE) { if (rel) *rel = 0; return 0; }
    if (rel) *rel = c->rel;
    return c->qos;
}
void *dispatch_get_current_queue(void) { dp_queue_t *q = current_queue(); return q ? q : root_for(QOS_DEF, 0); }
void dispatch_set_target_queue(void *o, void *tq) {
    dp_obj_t *h = o;
    if (!is_obj(h) || h->ref < 0) return;
    if (h->kind == DK_QUEUE && (((dp_queue_t *)h)->flags & (QF_ROOT | QF_MAIN))) return;
    if (!tq) {
        if (h->kind == DK_QUEUE) { dp_queue_t *q = (dp_queue_t *)h; tq = root_for(q->qos ? q->qos : QOS_DEF, q->width == 1); }
        else tq = root_for(QOS_DEF, 0);
    }
    cx_dispatch_retain(tq);
    void *old = __sync_lock_test_and_set(&h->tq, tq);
    if (old) cx_dispatch_release(old);
}
void dispatch_set_qos_class_floor(void *o, unsigned qos, int rel) {
    dp_obj_t *h = o;
    if (is_obj(h) && h->kind == DK_QUEUE && h->ref >= 0) { ((dp_queue_t *)h)->qos = qos; ((dp_queue_t *)h)->rel = rel; }
}
void dispatch_set_qos_class(void *o, unsigned qos, int rel) { dispatch_set_qos_class_floor(o, qos, rel); }
/* SPI dispatch_queue_set_width: largura de uma fila concorrente; os negativos são DISPATCH_QUEUE_WIDTH_MAX_PHYSICAL_CPUS
 * (-1), _ACTIVE_CPUS (-2) e _MAX_LOGICAL_CPUS (-3). 1 vira serial. Filas raiz e a principal não mudam. */
void dispatch_queue_set_width(void *o, long width) {
    dp_queue_t *q = o;
    if (!is_obj(q) || q->h.kind != DK_QUEUE || (q->flags & (QF_ROOT | QF_MAIN | QF_WORKLOOP))) return;
    int w = width > 0 ? (width > 0x7fffffff ? 0x7fffffff : (int)width) : width >= -3 ? dp_ncpu() : 1;
    dp_lock(&q->lock);
    q->width = w;
    dp_unlock(&q->lock);
    q_wakeup(q);
}
/* suspender/retomar/ativar: filas aqui; fontes em dispatch_source.c (o gancho é chamado depois) */
void cx_dispatch_source_resumed(void *s);
void dispatch_suspend(void *o) {
    dp_obj_t *h = o;
    if (!is_obj(h) || h->ref < 0) return;
    __sync_fetch_and_add(&h->susp, 1);
}
static void obj_wake(dp_obj_t *h) {
    if (h->kind == DK_QUEUE) q_wakeup((dp_queue_t *)h);
    else if (h->kind == DK_SOURCE) cx_dispatch_source_resumed(h);
}
void dispatch_activate(void *o) {
    dp_obj_t *h = o;
    if (!is_obj(h) || h->ref < 0) return;
    if (__sync_lock_test_and_set(&h->inactive, 0)) obj_wake(h);
}
void dispatch_resume(void *o) {
    dp_obj_t *h = o;
    if (!is_obj(h) || h->ref < 0) return;
    if (h->inactive) { dispatch_activate(o); return; }
    int v = __sync_fetch_and_sub(&h->susp, 1);
    if (v <= 0) { __sync_fetch_and_add(&h->susp, 1); dp_write_err("BUG IN CLIENT OF LIBDISPATCH: Over-resume of an object\n"); abort(); }
    if (v == 1) obj_wake(h);
}

/* ------------------------------------------------------------------ valores por fila */
void dispatch_queue_set_specific(void *qq, const void *key, void *ctx, void (*dtor)(void *)) {
    dp_queue_t *q = qq;
    if (!is_obj(q) || !key) return;
    void *old = 0; void (*odtor)(void *) = 0;
    dp_lock(&q->lock);
    dp_spec_t **pp = &q->spec;
    while (*pp && (*pp)->key != key) pp = &(*pp)->next;
    if (*pp) {
        old = (*pp)->ctx; odtor = (*pp)->dtor;
        if (ctx) { (*pp)->ctx = ctx; (*pp)->dtor = dtor; }
        else { dp_spec_t *d = *pp; *pp = d->next; free(d); }
    } else if (ctx) {
        dp_spec_t *s = malloc(sizeof *s);
        if (s) { s->key = key; s->ctx = ctx; s->dtor = dtor; s->next = q->spec; q->spec = s; }
    }
    dp_unlock(&q->lock);
    if (old && odtor) odtor(old);
}
static void *spec_lookup(dp_queue_t *q, const void *key, int *found) {
    void *r = 0;
    *found = 0;
    dp_lock(&q->lock);
    for (dp_spec_t *s = q->spec; s; s = s->next) if (s->key == key) { r = s->ctx; *found = 1; break; }
    dp_unlock(&q->lock);
    return r;
}
void *dispatch_queue_get_specific(void *q, const void *key) {
    int f;
    if (!is_obj(q) || !key) return 0;
    return spec_lookup(q, key, &f);
}
/* a fila atual e a cadeia de destinos dela, como no macOS */
void *dispatch_get_specific(const void *key) {
    if (!key) return 0;
    dp_queue_t *q = current_queue();
    for (int n = 0; q && n < 64; n++, q = q->h.tq) {
        int f;
        void *r = spec_lookup(q, key, &f);
        if (f) return r;
    }
    return 0;
}

/* ------------------------------------------------------------------ asserções */
static __attribute__((noreturn)) void assert_fail(const char *what, dp_queue_t *q) {
    dp_write_err("BUG IN CLIENT OF LIBDISPATCH: ");
    dp_write_err(what);
    dp_write_err(" \"");
    dp_write_err(q && q->label ? q->label : "");
    dp_write_err("\"\n");
    abort();
}
void dispatch_assert_queue(void *q) { if (q && !on_queue(q)) assert_fail("Block was expected to execute on queue", q); }
void dispatch_assert_queue_not(void *q) { if (q && on_queue(q)) assert_fail("Block was expected not to execute on queue", q); }
void dispatch_assert_queue_barrier(void *qq) {
    dp_queue_t *q = qq;
    dispatch_assert_queue(q);
    if (q->width > 1 && !(q->flags & QF_ROOT) && !q->barrier) assert_fail("Block was expected to act as a barrier on queue", q);
}
void dispatch_assert_queue_V2(void *q) __asm__("dispatch_assert_queue$V2");
void dispatch_assert_queue_V2(void *q) { dispatch_assert_queue(q); }
void dispatch_assert_queue_not_V2(void *q) __asm__("dispatch_assert_queue_not$V2");
void dispatch_assert_queue_not_V2(void *q) { dispatch_assert_queue_not(q); }

/* ------------------------------------------------------------------ once */
void dispatch_once_f(long *pred, void *ctx, dispatch_function_t fn) {
    volatile long *p = pred;
    if (*p == ~0l) return;
    if (__sync_bool_compare_and_swap(p, 0, 1)) {
        fn(ctx);
        __sync_synchronize();
        long was = __sync_lock_test_and_set(p, ~0l);
        if (was == 2) dp_wake((volatile int *)p, 0x7fffffff);
        return;
    }
    for (;;) {
        long v = *p;
        if (v == ~0l) return;
        if (v == 1) __sync_bool_compare_and_swap(p, 1, 2);
        dp_wait((volatile int *)p, 2, 10);           /* x86: palavra baixa do predicado */
    }
}
void dispatch_once(long *pred, void *block) { dispatch_once_f(pred, block, blk_tramp); }

/* ------------------------------------------------------------------ tempo */
/* dispatch_time_t como no libdispatch da Apple (shims/time.h): NOW 0, FOREVER ~0, WALLTIME_NOW ~1;
 *  bit 63 = 0            -> uptime (a base do mach_absolute_time, ns no x86_64)
 *  bits 63 e 62 = 1      -> relógio de parede, codificado como -(ns desde 1970)
 *  bit 63 = 1, bit 62 = 0 -> relógio contínuo (monotônico com o tempo dormindo): valor | 1<<63.
 * O _Concurrency real manda os prazos do Task.sleep no relógio contínuo; antes este caso era lido como parede e o
 * prazo virava décadas. O mOS não suspende a máquina, então contínuo e uptime têm a mesma base. */
#define DT_FOREVER (~0ull)
#define DT_MONO (1ull << 63)
#define DT_WALL (1ull << 62)
#define DT_MAX (DT_WALL - 1)
enum { DC_UP, DC_MONO, DC_WALL };
static int dt_decode(u64 t, u64 *v) {
    if (!(t & DT_MONO)) { *v = t; return DC_UP; }
    if (t & DT_WALL) { *v = t == ~1ull ? dp_wall_ns() : (u64)(-(i64)t); return DC_WALL; }
    *v = t & ~DT_MONO;
    return DC_MONO;
}
static u64 dt_encode(int clk, u64 v) {
    if (v >= DT_MAX) return DT_FOREVER;
    if (clk == DC_WALL) return v <= 1 ? (u64)(-2ll) : (u64)(-(i64)v);
    if (clk == DC_MONO) return v | DT_MONO;
    return v;
}
u64 cx_dispatch_deadline_ns(u64 t) {
    if (t == 0) return dp_now_ns();
    if (t == DT_FOREVER) return DT_FOREVER;
    u64 v;
    if (dt_decode(t, &v) == DC_WALL) {
        u64 nw = dp_wall_ns(), now = dp_now_ns();
        return v > nw ? now + (v - nw) : now;
    }
    return v;
}
u64 dispatch_time(u64 when, i64 delta) {
    if (when == DT_FOREVER) return DT_FOREVER;
    u64 v;
    int clk = DC_UP;
    if (when == 0) v = dp_now_ns();
    else clk = dt_decode(when, &v);
    if (delta < 0) v = (u64)(-delta) >= v ? 1 : v - (u64)(-delta);
    else if (v + (u64)delta < v) return DT_FOREVER;
    else v += (u64)delta;
    return dt_encode(clk, v);
}
typedef struct { long tv_sec, tv_nsec; } dt_timespec_t;
u64 dispatch_walltime(const dt_timespec_t *when, i64 delta) {
    u64 ns = when ? (u64)when->tv_sec * 1000000000ull + (u64)when->tv_nsec : dp_wall_ns();
    ns += (u64)delta;
    if ((i64)ns <= 1) ns = 2;
    return dt_encode(DC_WALL, ns);
}

/* ------------------------------------------------------------------ timer */
struct dp_timer { struct dp_timer *next; u64 when; void (*cb)(void *); void *arg; };
static dp_timer_t *tm_head;
static dp_lock_t tm_lock;
static volatile int tm_seq, tm_started;
static void *timer_main(void *a) {
    (void)a;
    for (;;) {
        dp_lock(&tm_lock);
        u64 now = dp_now_ns();
        dp_timer_t *t = tm_head;
        if (t && t->when <= now) {
            tm_head = t->next;
            dp_unlock(&tm_lock);
            t->cb(t->arg);
            free(t);
            continue;
        }
        long ms = t ? (long)((t->when - now + 999999) / 1000000) : -1;
        int seq = tm_seq;
        dp_unlock(&tm_lock);
        dp_wait(&tm_seq, seq, ms);
    }
    return 0;
}
dp_timer_t *dp_timer_add(u64 when, void (*cb)(void *), void *arg) {
    dp_timer_t *t = calloc(1, sizeof *t);
    if (!t) abort();
    t->when = when; t->cb = cb; t->arg = arg;
    dp_lock(&tm_lock);
    dp_timer_t **pp = &tm_head;
    while (*pp && (*pp)->when <= when) pp = &(*pp)->next;
    t->next = *pp;
    *pp = t;
    int first = !tm_started;
    tm_started = 1;
    dp_unlock(&tm_lock);
    if (first && dp_thread(timer_main, 0) != 0) abort();
    __sync_fetch_and_add(&tm_seq, 1);
    dp_wake(&tm_seq, 1);
    return t;
}
/* tira o timer se ele ainda não disparou; 1 = tirado */
int dp_timer_cancel(dp_timer_t *t) {
    int r = 0;
    dp_lock(&tm_lock);
    for (dp_timer_t **pp = &tm_head; *pp; pp = &(*pp)->next)
        if (*pp == t) { *pp = t->next; r = 1; break; }
    dp_unlock(&tm_lock);
    if (r) free(t);
    return r;
}

/* monitor do pool: se há trabalho esperando e nada andou desde a última olhada, as threads estão bloqueadas: mais uma */
static volatile int mon_armed, mon_last;
static void pool_monitor(void *a) {
    (void)a;
    int waiting = 0;
    for (int i = 0; i < 12; i++) waiting += roots[i].nqueued;
    if (!waiting) { mon_armed = 0; return; }
    if (pool_progress == mon_last && pool_idle == 0) {
        dp_lock(&pool_lock);
        if (pool_threads < DP_MAX_THREADS) pool_spawn_locked();
        dp_unlock(&pool_lock);
    }
    mon_last = pool_progress;
    dp_timer_add(dp_now_ns() + 50000000ull, pool_monitor, 0);
}
static void pool_monitor_arm(void) {
    if (mon_armed || !__sync_bool_compare_and_swap(&mon_armed, 0, 1)) return;
    mon_last = pool_progress;
    dp_timer_add(dp_now_ns() + 50000000ull, pool_monitor, 0);
}

/* dispatch_after: no prazo, o item entra na fila */
typedef struct { dp_queue_t *q; dp_item_t *it; } dp_after_t;
static void after_fire(void *a) {
    dp_after_t *x = a;
    q_push(x->q, x->it);
    cx_dispatch_release(x->q);
    free(x);
}
static void after_common(u64 when, void *qq, void *ctx, dispatch_function_t fn, void *block) {
    dp_queue_t *q = qq ? qq : &_dispatch_main_q;
    if (when == DT_FOREVER) return;                       /* nunca: o macOS também descarta */
    dp_item_t *it = item_make(ctx, fn, block, block ? block_flags(block) : 0);
    it->qos = q_effective_qos(q);
    u64 at = cx_dispatch_deadline_ns(when);
    if (at <= dp_now_ns()) { q_push(q, it); return; }
    dp_after_t *x = malloc(sizeof *x);
    if (!x) abort();
    cx_dispatch_retain(q);
    x->q = q; x->it = it;
    dp_timer_add(at, after_fire, x);
}
void dispatch_after_f(u64 when, void *q, void *ctx, dispatch_function_t fn) { after_common(when, q, ctx, fn, 0); }
void dispatch_after(u64 when, void *q, void *block) { after_common(when, q, 0, 0, block); }

/* ------------------------------------------------------------------ grupos */
typedef struct dp_group { dp_obj_t h; volatile int n; volatile int seq; dp_lock_t lock; dp_item_t *notes; dp_queue_t **nq; int nnq, cap; } dp_group_t;
static void group_dispose(dp_obj_t *h) {
    dp_group_t *g = (dp_group_t *)h;
    free(g->nq);
}
void *dispatch_group_create(void) {
    dp_group_t *g = calloc(1, sizeof *g);
    if (!g) return 0;
    dp_obj_init(g, OS_dispatch_group_c, DK_GROUP, group_dispose);
    return g;
}
void dispatch_group_enter(void *gg) {
    dp_group_t *g = gg;
    if (__sync_fetch_and_add(&g->n, 1) == 0) cx_dispatch_retain(g);   /* grupo com trabalho se segura */
}
static void group_fire(dp_group_t *g) {
    dp_lock(&g->lock);
    dp_item_t *n = g->notes;
    dp_queue_t **qs = g->nq;
    int nq = g->nnq;
    g->notes = 0; g->nq = 0; g->nnq = 0; g->cap = 0;
    dp_unlock(&g->lock);
    for (int i = 0; n; i++) {
        dp_item_t *x = n;
        n = n->next;
        x->next = 0;
        q_push(qs[i], x);
        cx_dispatch_release(qs[i]);
    }
    (void)nq;
    free(qs);
}
static void group_leave_internal(dp_group_t *g) {
    int v = __sync_sub_and_fetch(&g->n, 1);
    if (v < 0) { dp_write_err("BUG IN CLIENT OF LIBDISPATCH: Unbalanced call to dispatch_group_leave()\n"); abort(); }
    if (v == 0) {
        __sync_fetch_and_add(&g->seq, 1);
        dp_wake(&g->seq, 0x7fffffff);
        group_fire(g);
        cx_dispatch_release(g);
    }
}
void dispatch_group_leave(void *g) { group_leave_internal(g); }
long dispatch_group_wait(void *gg, u64 timeout) {
    dp_group_t *g = gg;
    u64 limit = timeout == DT_FOREVER ? DT_FOREVER : cx_dispatch_deadline_ns(timeout);
    for (;;) {
        int seq = g->seq;
        if (g->n == 0) return 0;
        long ms = -1;
        if (limit != DT_FOREVER) {
            u64 now = dp_now_ns();
            if (now >= limit) return 1;
            ms = (long)((limit - now + 999999) / 1000000);
        }
        dp_wait(&g->seq, seq, ms);
    }
}
static void group_add_note(dp_group_t *g, dp_queue_t *q, dp_item_t *it) {
    cx_dispatch_retain(q);
    dp_lock(&g->lock);
    if (g->n == 0) {
        dp_unlock(&g->lock);
        q_push(q, it);
        cx_dispatch_release(q);
        return;
    }
    if (g->nnq == g->cap) {
        int c = g->cap ? g->cap * 2 : 4;
        dp_queue_t **n = realloc(g->nq, (size_t)c * sizeof *n);
        if (!n) abort();
        g->nq = n; g->cap = c;
    }
    dp_item_t **pp = &g->notes;
    while (*pp) pp = &(*pp)->next;
    *pp = it;
    g->nq[g->nnq++] = q;
    dp_unlock(&g->lock);
}
void dispatch_group_notify_f(void *g, void *q, void *ctx, dispatch_function_t fn) {
    dp_queue_t *x = q ? q : &_dispatch_main_q;
    dp_item_t *it = item_make(ctx, fn, 0, 0);
    it->qos = q_effective_qos(x);
    group_add_note(g, x, it);
}
void dispatch_group_notify(void *g, void *q, void *block) {
    dp_queue_t *x = q ? q : &_dispatch_main_q;
    dp_item_t *it = item_make(0, 0, block, 0);
    it->qos = q_effective_qos(x);
    group_add_note(g, x, it);
}
static void group_async_common(void *g, void *qq, void *ctx, dispatch_function_t fn, void *block) {
    dp_queue_t *q = qq ? qq : &_dispatch_main_q;
    dispatch_group_enter(g);
    dp_item_t *it = item_make(ctx, fn, block, block ? block_flags(block) : 0);
    it->group = g;
    it->qos = q_effective_qos(q);
    q_push(q, it);
}
void dispatch_group_async_f(void *g, void *q, void *ctx, dispatch_function_t fn) { group_async_common(g, q, ctx, fn, 0); }
void dispatch_group_async(void *g, void *q, void *block) { group_async_common(g, q, 0, 0, block); }

/* ------------------------------------------------------------------ semáforos */
typedef struct { dp_obj_t h; volatile long count; long orig; volatile int seq; volatile int waiters; } dp_sema_t;
void *dispatch_semaphore_create(long v) {
    if (v < 0) return 0;
    dp_sema_t *s = calloc(1, sizeof *s);
    if (!s) return 0;
    dp_obj_init(s, OS_dispatch_semaphore_c, DK_SEMA, 0);
    s->count = v;
    s->orig = v;
    return s;
}
long dispatch_semaphore_signal(void *ss) {
    dp_sema_t *s = ss;
    __sync_fetch_and_add(&s->count, 1);
    __sync_fetch_and_add(&s->seq, 1);
    if (s->waiters) { dp_wake(&s->seq, 1); return 1; }
    return 0;
}
long dispatch_semaphore_wait(void *ss, u64 timeout) {
    dp_sema_t *s = ss;
    u64 limit = timeout == DT_FOREVER ? DT_FOREVER : cx_dispatch_deadline_ns(timeout);
    for (;;) {
        long c = s->count;
        if (c > 0) { if (__sync_bool_compare_and_swap(&s->count, c, c - 1)) return 0; continue; }
        long ms = -1;
        if (limit != DT_FOREVER) {
            u64 now = dp_now_ns();
            if (now >= limit) return 1;
            ms = (long)((limit - now + 999999) / 1000000);
        }
        int seq = s->seq;
        __sync_fetch_and_add(&s->waiters, 1);
        if (s->count <= 0) dp_wait(&s->seq, seq, ms);
        __sync_fetch_and_sub(&s->waiters, 1);
    }
}

/* ------------------------------------------------------------------ apply */
typedef struct { void *ctx; void (*fn)(void *, size_t); size_t n; volatile long next; volatile int pending; dp_queue_t *q; } dp_apply_t;
static void apply_work(dp_apply_t *a) {
    for (;;) {
        size_t i = (size_t)__sync_fetch_and_add(&a->next, 1);
        if (i >= a->n) break;
        a->fn(a->ctx, i);
    }
}
static void apply_helper(void *p) {
    dp_apply_t *a = p;
    apply_work(a);
    if (__sync_sub_and_fetch(&a->pending, 1) == 0) dp_wake(&a->pending, 1);
}
void dispatch_apply_f(size_t n, void *qq, void *ctx, void (*fn)(void *, size_t)) {
    if (!n) return;
    dp_queue_t *q = qq;
    if (!q) { dp_queue_t *c = current_queue(); q = root_for(c ? q_effective_qos(c) : QOS_DEF, 0); }   /* DISPATCH_APPLY_AUTO */
    if (q->width == 1 || n == 1) {                       /* fila serial: em ordem, como dispatch_sync em laço */
        dp_frame_t f;
        frame_push(&f, q);
        for (size_t i = 0; i < n; i++) fn(ctx, i);
        frame_pop(&f);
        return;
    }
    dp_queue_t *root = q;
    while (!(root->flags & QF_ROOT) && root->h.tq) root = root->h.tq;
    if (!(root->flags & QF_ROOT)) root = dp_default_target();
    if (!pool_ncpu) pool_ncpu = dp_ncpu() < 2 ? 2 : dp_ncpu();
    int helpers = (int)(n - 1 < (size_t)pool_ncpu - 1 ? n - 1 : (size_t)pool_ncpu - 1);
    dp_apply_t a = {ctx, fn, n, 0, helpers, q};
    for (int i = 0; i < helpers; i++) {
        dp_item_t *it = item_make(&a, apply_helper, 0, IT_APPLY);
        it->qos = root->qos;
        pool_push(root, it);
    }
    dp_frame_t f;
    frame_push(&f, q);
    apply_work(&a);
    frame_pop(&f);
    for (;;) { int p = a.pending; if (!p) break; dp_wait(&a.pending, p, -1); }
}
static void apply_blk(void *b, size_t i) { ((void (*)(void *, size_t))((dp_block_t *)b)->invoke)(b, i); }
void dispatch_apply(size_t n, void *q, void *block) { dispatch_apply_f(n, q, block, apply_blk); }

/* ------------------------------------------------------------------ blocos com flags (dispatch_block.c) */
unsigned cx_dispatch_block_flags(void *block);   /* bit 0 = DISPATCH_BLOCK_BARRIER */
static unsigned block_flags(void *block) { return (cx_dispatch_block_flags(block) & 1) ? IT_BARRIER : 0; }

/* SPI do libpthread que o libdispatch da Apple usa para deixar (ou não) o kill de sinais chegar às threads do pool:
 * no mOS os workers são threads comuns e sempre recebem sinais, então ligar ou desligar não muda nada */
int __pthread_workqueue_setkill(int enable) { (void)enable; return 0; }

/* depuração */
void dispatch_debug(void *o, const char *msg, ...) {
    (void)o;
    dp_write_err("libdispatch: ");
    dp_write_err(msg ? msg : "");
    dp_write_err("\n");
}
