/*
 * Teste de host: dispatch_block (cancelar, esperar, notify) e a fatia
 * swift_task / swift_job do runtime de concorrência.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int falhas;

static void ok(int cond, const char *msg) {
    if (cond) return;
    fprintf(stderr, "falhou: %s\n", msg);
    falhas++;
}

static char fila_principal;
void *dispatch_get_main_queue(void) { return &fila_principal; }

static int async_f_rodou;
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *)) {
    (void)q;
    async_f_rodou++;
    fn(ctx);
}

typedef struct { void *isa; int flags, reserved; void (*invoke)(void *); void *desc; } bloco_t;
typedef struct { unsigned long long reserved, size; void (*copy)(void *, const void *); void (*dispose)(const void *); } desc_t;

void *_Block_copy(const void *p) {
    bloco_t *b = (bloco_t *)p;
    if (!b) return 0;
    if (b->flags & (1 << 28)) return (void *)b;
    if (b->flags & (1 << 24)) {
        b->flags += 2;
        return (void *)b;
    }
    unsigned long sz = 32;
    if (b->desc) sz = ((desc_t *)b->desc)->size;
    bloco_t *n = calloc(1, sz);
    memcpy(n, b, sz);
    n->flags = (n->flags & ~0xffff) | (1 << 24) | 2;
    if ((b->flags & (1 << 25)) && b->desc && ((desc_t *)b->desc)->copy)
        ((desc_t *)b->desc)->copy(n, b);
    return n;
}
void _Block_release(const void *p) {
    bloco_t *b = (bloco_t *)p;
    if (!b || (b->flags & (1 << 28)) || !(b->flags & (1 << 24))) return;
    int c = b->flags & 0xfffe;
    if (c > 2) {
        b->flags -= 2;
        return;
    }
    if ((b->flags & (1 << 25)) && b->desc && ((desc_t *)b->desc)->dispose)
        ((desc_t *)b->desc)->dispose(b);
    free(b);
}

void dispatch_async(void *q, void *block) {
    (void)q;
    bloco_t *b = block;
    if (b && b->invoke) b->invoke(b);
}

void *dispatch_block_create(unsigned long flags, void *block);
void *dispatch_block_create_with_qos_class(unsigned long flags, unsigned qos, int rel, void *block);
void dispatch_block_perform(unsigned long flags, void *block);
void dispatch_block_cancel(void *block);
long dispatch_block_testcancel(void *block);
long dispatch_block_wait(void *block, unsigned long long timeout);
void dispatch_block_notify(void *block, void *queue, void *notification);

static int corpo_rodou;
static void corpo(void *b) { (void)b; corpo_rodou++; }
static bloco_t bloco_corpo = {0, 1 << 28, 0, corpo, 0};

static int aviso_rodou;
static void aviso(void *b) { (void)b; aviso_rodou++; }
static bloco_t bloco_aviso = {0, 1 << 28, 0, aviso, 0};

static void testa_bloco(void) {
    void *ruim = dispatch_block_create(0x40, &bloco_corpo);
    ok(ruim == 0, "flags desconhecidas devolvem nulo");
    void *qos = dispatch_block_create_with_qos_class(0, 0x21, 1, &bloco_corpo);
    ok(qos == 0, "prioridade relativa acima de zero devolve nulo");
    void *qos_ok = dispatch_block_create_with_qos_class(0, 0x15, 0, &bloco_corpo);
    ok(qos_ok != 0, "QoS default cria o bloco");
    if (qos_ok) _Block_release(qos_ok);

    corpo_rodou = 0;
    void *c = dispatch_block_create(0, &bloco_corpo);
    ok(c != 0, "create devolve bloco");
    dispatch_block_cancel(c);
    ok(dispatch_block_testcancel(c) != 0, "testcancel vê o cancelamento");
    bloco_t *hc = c;
    hc->invoke(c);
    ok(corpo_rodou == 0, "bloco cancelado não roda");
    ok(dispatch_block_wait(c, ~0ull) == 0, "wait volta depois da invocação cancelada");
    _Block_release(c);

    corpo_rodou = 0;
    aviso_rodou = 0;
    void *w = dispatch_block_create(0, &bloco_corpo);
    ok(dispatch_block_wait(w, 0) != 0, "wait com DISPATCH_TIME_NOW expira antes de rodar");
    dispatch_block_notify(w, &fila_principal, &bloco_aviso);
    dispatch_block_perform(0, &bloco_corpo);
    ok(corpo_rodou == 1, "perform roda o bloco na hora");
    ok(aviso_rodou == 0, "notify espera o bloco observado");
    hc = w;
    hc->invoke(w);
    ok(corpo_rodou == 2, "a invocação do bloco observado roda o corpo");
    ok(aviso_rodou == 1, "notify dispara na fila");
    ok(dispatch_block_wait(w, ~0ull) == 0, "wait volta depois do bloco ter rodado");
    _Block_release(w);
}

typedef struct { void *identity; unsigned long impl; } exec_t;
__attribute__((swiftcall)) void *swift_task_getCurrent(void);
__attribute__((swiftcall)) _Bool swift_task_isCancelled(void *task);
__attribute__((swiftcall)) void swift_task_cancel(void *task);
__attribute__((swiftcall)) exec_t swift_task_getMainExecutor(void);
__attribute__((swiftcall)) exec_t swift_task_getCurrentExecutor(void);
__attribute__((swiftcall)) _Bool swift_task_isCurrentExecutor(exec_t executor);
__attribute__((swiftcall)) void swift_job_run(void *job, exec_t executor);
__attribute__((swiftcall)) void swift_task_enqueueMainExecutor(void *job);

static int job_rodou;
static void *visto_atual;
static _Bool visto_na_main;
static exec_t main_visto;

__attribute__((swiftasynccall)) static void job_fn(void *ctx) {
    job_rodou++;
    visto_atual = swift_task_getCurrent();
    main_visto = swift_task_getMainExecutor();
    visto_na_main = swift_task_isCurrentExecutor(main_visto);
    (void)ctx;
}

static void testa_task(void) {
    unsigned char mem[256];
    memset(mem, 0, sizeof mem);
    ok(!swift_task_isCancelled(mem), "task nova não está cancelada");
    swift_task_cancel(mem);
    ok(swift_task_isCancelled(mem), "isCancelled reflete o cancel");

    exec_t a = swift_task_getMainExecutor();
    exec_t b = swift_task_getMainExecutor();
    ok(a.identity == &fila_principal && a.identity == b.identity && a.impl == b.impl,
       "executor principal é a fila principal e é estável");
    ok(swift_task_getCurrent() == 0, "sem job, a task atual é nula");

    unsigned char job[128];
    memset(job, 0, sizeof job);
    *(unsigned *)(job + 32) = 192; /* não é AsyncTask */
    *(void **)(job + 56) = (void *)job_fn;
    exec_t antes = swift_task_getCurrentExecutor();
    swift_job_run(job, a);
    ok(job_rodou == 1, "swift_job_run roda o job");
    ok(visto_atual == 0, "job que não é task não vira task atual");
    ok(visto_na_main, "durante o job o executor atual é o que foi pedido");
    exec_t depois = swift_task_getCurrentExecutor();
    ok(depois.identity == antes.identity && depois.impl == antes.impl,
       "swift_job_run restaura o executor");

    memset(job, 0, sizeof job);
    *(unsigned *)(job + 32) = 0; /* AsyncTask */
    *(void **)(job + 56) = (void *)job_fn;
    *(void **)(job + 64) = job;
    *(unsigned *)(job + 96) = 0;
    job_rodou = 0;
    swift_task_enqueueMainExecutor(job);
    ok(async_f_rodou == 1, "enqueueMainExecutor usa a fila principal");
    ok(job_rodou == 1, "o main executor roda o job");
    ok(visto_atual == (void *)job, "durante a task, getCurrent devolve a task");
    ok(swift_task_getCurrent() == 0, "depois do job a task atual volta a ser nula");
}

int main(void) {
    testa_bloco();
    testa_task();
    if (falhas) {
        fprintf(stderr, "runtime bloco/task: %d falha(s)\n", falhas);
        return 1;
    }
    printf("runtime bloco/task: ok\n");
    return 0;
}
