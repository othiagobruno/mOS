/* mOS — corrotinas com pilha própria para o motor web (diálogos JS que esperam o app).
 *
 * Uma operação do WKWebView (PUMP, INPUT, EVAL...) roda numa costack; se um alert/confirm/prompt precisa esperar o
 * app, a execução cede (cs_yield) e a syscall volta ao usuário; quando o app responde, cs_resume continua do ponto
 * em que parou. A troca usa a convenção System V explicitamente (sysv_abi): no kernel (ABI da Microsoft) o
 * compilador salva sozinho rdi, rsi e xmm6-xmm15 em volta da chamada, e no teste de host é a convenção nativa.
 * Só rbx, rbp e r12-r15 precisam ir para a pilha aqui. */
#include "web.h"

struct costack {
    void *sp;                /* pilha da corrotina enquanto ela está parada */
    void *caller_sp;         /* pilha de quem retomou */
    u8 *stack;
    usize size;
    void (*fn)(void *);
    void *arg;
    bool done, running;
};

static costack_t *g_cs_cur;

__attribute__((sysv_abi)) void cs_switch(void **save_sp, void *to_sp);
__attribute__((sysv_abi)) void cs_tramp(void);

#ifdef __APPLE__
#define CS_SYM(x) "_" #x
#else
#define CS_SYM(x) #x
#endif
__asm__(
    ".text\n"
    ".globl " CS_SYM(cs_switch) "\n"
    CS_SYM(cs_switch) ":\n"
    "    pushq %rbp\n    pushq %rbx\n    pushq %r12\n    pushq %r13\n    pushq %r14\n    pushq %r15\n"
    "    movq %rsp, (%rdi)\n"
    "    movq %rsi, %rsp\n"
    "    popq %r15\n    popq %r14\n    popq %r13\n    popq %r12\n    popq %rbx\n    popq %rbp\n"
    "    ret\n"
    ".globl " CS_SYM(cs_tramp) "\n"
    CS_SYM(cs_tramp) ":\n"
    "    movq %r12, %rdi\n"
    "    andq $-16, %rsp\n"
    "    call " CS_SYM(cs_entry) "\n"
    "    ud2\n");

__attribute__((sysv_abi, used)) void cs_entry(costack_t *c) {
    c->fn(c->arg);
    c->done = true;
    c->running = false;
    g_cs_cur = NULL;
    cs_switch(&c->sp, c->caller_sp);
    for (;;) {}
}

/* stack: pilha já alocada (kmalloc) que a corrotina passa a possuir, ou NULL para alocar */
costack_t *cs_new_on(u8 *stack, usize size, void (*fn)(void *), void *arg) {
    costack_t *c = kcalloc(1, sizeof(costack_t));
    if (!c) return NULL;
    c->stack = stack ? stack : kmalloc(size);
    if (!c->stack) { kfree(c); return NULL; }
    c->size = size;
    c->fn = fn;
    c->arg = arg;
    /* quadro inicial: r15 r14 r13 r12(=c) rbx rbp, e o endereço de retorno = cs_tramp */
    u64 *top = (u64 *)(((uptr)c->stack + size) & ~15ull);
    *--top = 0;
    *--top = (u64)(uptr)cs_tramp;
    *--top = 0;                      /* rbp */
    *--top = 0;                      /* rbx */
    *--top = (u64)(uptr)c;           /* r12 */
    *--top = 0;                      /* r13 */
    *--top = 0;                      /* r14 */
    *--top = 0;                      /* r15 */
    c->sp = top;
    return c;
}

/* roda (ou retoma) até terminar ou ceder; devolve true se terminou */
bool cs_resume(costack_t *c) {
    if (!c || c->done || c->running) return c ? c->done : true;
    costack_t *prev = g_cs_cur;
    g_cs_cur = c;
    c->running = true;
    cs_switch(&c->caller_sp, c->sp);
    g_cs_cur = prev;
    return c->done;
}

/* de dentro da corrotina: volta para quem chamou cs_resume */
void cs_yield(void) {
    costack_t *c = g_cs_cur;
    if (!c) return;
    c->running = false;
    cs_switch(&c->sp, c->caller_sp);
    /* retomada: cs_resume já ajustou g_cs_cur e running */
}

costack_t *cs_new(usize size, void (*fn)(void *), void *arg) { return cs_new_on(NULL, size, fn, arg); }

/* libera a corrotina (terminada) e devolve a pilha para reuso */
u8 *cs_release(costack_t *c) {
    if (!c) return NULL;
    u8 *s = c->stack;
    kfree(c);
    return s;
}

costack_t *cs_current(void) { return g_cs_cur; }
bool cs_done(costack_t *c) { return !c || c->done; }

void cs_free(costack_t *c) {
    if (!c) return;
    kfree(c->stack);
    kfree(c);
}
