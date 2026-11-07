/*
 * mOS — threads e escalonador (um núcleo).
 *
 * A troca de contexto acontece na saída de interrupções: isr_common salva todos os registradores
 * (e o estado FXSAVE logo abaixo) na pilha da thread interrompida e restaura o frame que
 * isr_dispatch devolver. Uma thread nova começa com um frame "falso" que o iretq consome.
 *
 * Política (ver proc.h): a thread 0 (desktop) só cede o processador quando é interrompida num
 * `hlt`; as demais rodam em fatias de SCHED_SLICE_MS e, ao fim de cada fatia, o desktop volta.
 */
#include "proc.h"

#define KSTACK_SIZE (32u << 10)

static thread_t thread0 = {.id = 0, .name = "desktop", .state = TH_RUNNING};
static thread_t *threads = &thread0, *current = &thread0;
static thread_t *last_user;          /* última thread (não-desktop) que rodou: round-robin */
static int next_tid = 1;
static u64 slice_start, last_account;
static u64 kernel_cr3;

thread_t *thread_current(void) { return current; }
thread_t *sched_threads(void) { return threads; }

/* GS de usuário por thread: o macOS guarda os dados específicos de cada thread (pthread_getspecific,
 * TLS do Chromium) em gs:[chave*8]. O kernel não usa GS, então só troca a base (MSR IA32_GS_BASE) na
 * troca de contexto; CR4.FSGSBASE deixa a libSystem definir a própria base com wrgsbase. */
#define MSR_GS_BASE 0xC0000101u
static bool gs_switch;
static u64 rd_msr(u32 m) { u32 lo, hi; __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(m)); return (u64)hi << 32 | lo; }
static void wr_msr(u32 m, u64 v) { __asm__ volatile("wrmsr" :: "c"(m), "a"((u32)v), "d"((u32)(v >> 32))); }

void sched_init(void) {
    kernel_cr3 = read_cr3();
    slice_start = last_account = timer_ticks();
    u32 a = 7, b = 0, c = 0, d = 0;
    __asm__ volatile("cpuid" : "+a"(a), "=b"(b), "+c"(c), "=d"(d));
    if (b & 1) {                                     /* CPUID.7.EBX[0]: FSGSBASE */
        u64 cr4;
        __asm__ volatile("mov %%cr4,%0" : "=r"(cr4));
        __asm__ volatile("mov %0,%%cr4" :: "r"(cr4 | (1ULL << 16)));
    }
    gs_switch = true;
}

void sched_make_ready(thread_t *t) { if (t->state != TH_DEAD) t->state = TH_READY; }

/* frame inicial: o iretq "volta" para rip com os registradores dados; FXSAVE padrão logo abaixo */
static u64 build_frame(thread_t *t, u64 rip, u64 rsp, bool user, u64 rcx, u64 rdx) {
    u64 top = t->kstack_top;
    int_frame_t *f = (int_frame_t *)((top - sizeof(int_frame_t)) & ~15ULL);
    memset(f, 0, sizeof *f);
    f->rip = rip;
    f->rsp = rsp;
    f->rflags = 0x202;                          /* IF */
    f->cs = user ? 0x1B : 0x08;
    f->ss = user ? 0x23 : 0x10;
    f->rcx = rcx;
    f->rdx = rdx;
    u8 *fx = (u8 *)(((u64)f - 512) & ~15ULL);
    memset(fx, 0, 512);
    fx[0] = 0x7F; fx[1] = 0x03;                 /* FCW = 0x037F */
    fx[24] = 0x80; fx[25] = 0x1F;               /* MXCSR = 0x1F80 */
    return (u64)f;
}

static thread_t *new_thread(const char *name, process_t *p) {
    thread_t *t = kcalloc(1, sizeof *t);
    if (!t) return NULL;
    t->kstack = kmalloc(KSTACK_SIZE);
    if (!t->kstack) { kfree(t); return NULL; }
    t->kstack_top = ((u64)t->kstack + KSTACK_SIZE) & ~15ULL;
    t->id = next_tid++;
    strncpy(t->name, name, sizeof t->name - 1);
    t->proc = p;
    return t;
}

static void link_thread(thread_t *t) {
    u64 fl = irq_save();
    t->state = TH_READY;
    t->next = threads->next;                    /* depois da thread 0 */
    threads->next = t;
    irq_restore(fl);
}

static void kthread_start(void *arg, void (*fn)(void *)) {
    fn(arg);
    thread_exit();
}

thread_t *kthread_create(const char *name, void (*fn)(void *), void *arg) {
    thread_t *t = new_thread(name, NULL);
    if (!t) return NULL;
    /* O frame inicial e a área FXSAVE ocupam o topo da pilha; a thread começa abaixo deles com
     * rsp = 16n+8 (como logo após um call) e 32 bytes de "shadow space" (ABI x64 da Microsoft). */
    u64 *sp = (u64 *)(((t->kstack_top - 1024) & ~15ULL) - 40);
    sp[0] = 0;                                  /* endereço de retorno falso: kthread_start nunca volta */
    t->frame = build_frame(t, (u64)kthread_start, (u64)sp, false, (u64)arg, (u64)fn);
    link_thread(t);
    return t;
}

/* thread de usuário: o processo já tem o espaço de endereçamento montado */
thread_t *uthread_create(process_t *p, u64 entry, u64 user_rsp, u64 arg) {
    thread_t *t = new_thread(p->name, p);
    if (!t) return NULL;
    t->frame = build_frame(t, entry, user_rsp, true, 0, 0);
    ((int_frame_t *)t->frame)->rdi = arg;
    link_thread(t);
    return t;
}

/* fork: thread nova que volta exatamente onde src (o frame do pai na syscall) voltaria, com o
 * mesmo estado de FPU/SSE (a área FXSAVE fica logo abaixo de cada frame) */
thread_t *uthread_clone(process_t *p, const int_frame_t *src) {
    thread_t *t = new_thread(p->name, p);
    if (!t) return NULL;
    t->frame = build_frame(t, src->rip, src->rsp, true, 0, 0);
    memcpy((void *)t->frame, src, sizeof *src);
    memcpy((void *)((t->frame - 512) & ~15ULL), (const void *)(((u64)src - 512) & ~15ULL), 512);
    link_thread(t);
    return t;
}

/* libera threads mortas (nunca a atual: ainda estamos na pilha dela) */
static void reap(void) {
    for (thread_t **pp = &threads->next; *pp;) {
        thread_t *t = *pp;
        if (t->state == TH_DEAD && t != current) {
            *pp = t->next;
            if (last_user == t) last_user = NULL;
            if (t->proc) proc_thread_exited(t);
            kfree(t->kstack);
            kfree(t);
        } else pp = &t->next;
    }
}

static void account(void) {
    u64 now = timer_ticks();
    current->cpu_ms += now - last_account;
    last_account = now;
}

static void wake_sleepers(void) {
    u64 now = timer_ticks();
    for (thread_t *t = threads; t; t = t->next) {
        if (t->state == TH_SLEEPING && now >= t->wake_at) t->state = TH_READY;
        /* recepção Mach com prazo (machipc.c): o rax de "tempo esgotado" já está no frame */
        else if (t->state == TH_WAITING && t->wait_pid == -7 && t->mach_deadline && now >= t->mach_deadline) {
            t->mach_deadline = 0;
            t->mach_wait = NULL;
            t->mach_peek = false;
            t->state = TH_READY;
        }
        /* SYS_FUTEX_WAIT com prazo (process.c): volta com -ETIMEDOUT */
        else if (t->state == TH_WAITING && t->wait_pid == -8 && t->wake_at && now >= t->wake_at) {
            ((int_frame_t *)t->frame)->rax = (u64)-60;      /* -MOS_ETIMEDOUT (syscall.h) */
            t->wake_at = 0;
            t->futex_va = 0;
            t->state = TH_READY;
        }
    }
}

/* próxima thread pronta depois de `from` (round-robin), sem contar a thread 0 */
static thread_t *next_ready(thread_t *from) {
    thread_t *t = from;
    for (int guard = 0; guard < 4096; guard++) {
        t = t->next ? t->next : threads->next;
        if (!t) return NULL;
        if (t != &thread0 && t->state == TH_READY) return t;
        if (t == from) break;
    }
    return NULL;
}

static u64 switch_to(int_frame_t *f, thread_t *to) {
    if (to == current) { current->state = TH_RUNNING; return (u64)f; }
    account();
    current->frame = (u64)f;
    if (current->state == TH_RUNNING) current->state = TH_READY;
    thread_t *from = current;
    if (from != &thread0) last_user = from;
    if (gs_switch) { from->gs_base = rd_msr(MSR_GS_BASE); wr_msr(MSR_GS_BASE, to->gs_base); }
    current = to;
    to->state = TH_RUNNING;
    slice_start = timer_ticks();
    u64 cr3 = to->proc ? (u64)to->proc->pml4 : kernel_cr3;
    u64 cur_cr3 = read_cr3();
    if (cr3 != cur_cr3) __asm__ volatile("mov %0,%%cr3" :: "r"(cr3) : "memory");
    if (to->kstack) tss_set_rsp0(to->kstack_top);
    (void)from;
    return to->frame;
}

/* chamado ao fim de toda interrupção de hardware */
static u64 irq_exit(int_frame_t *f, bool timer);
__attribute__((weak)) u64 proc_signal_check(int_frame_t *f) { return (u64)f; }

u64 sched_irq_exit(int_frame_t *f, bool timer) {
    u64 r = irq_exit(f, timer);
    /* voltando para código de usuário: entrega sinais pendentes (ex.: Ctrl+C num laço infinito) */
    if (((int_frame_t *)r)->cs & 3) r = proc_signal_check((int_frame_t *)r);
    return r;
}

static u64 irq_exit(int_frame_t *f, bool timer) {
    if (!timer || !threads->next) return (u64)f;   /* nenhuma outra thread: caminho de sempre */
    wake_sleepers();
    if (current == &thread0) {
        /* o desktop só cede quando a interrupção o pegou num hlt (ocioso); o hlt tem 1 byte */
        if ((f->cs & 3) || *(u8 *)(f->rip - 1) != 0xF4) return (u64)f;
        reap();
        thread_t *t = next_ready(last_user ? last_user : &thread0);
        return t ? switch_to(f, t) : (u64)f;
    }
    if (timer_ticks() - slice_start < SCHED_SLICE_MS) return (u64)f;
    reap();
    return switch_to(f, &thread0);              /* fim da fatia: o desktop roda */
}

/* a thread atual bloqueou ou terminou (chamado em syscalls, com IF = 0) */
u64 sched_switch(int_frame_t *f) {
    wake_sleepers();
    thread_t *t = next_ready(current);
    if (!t || t == current) t = current->state == TH_RUNNING ? current : &thread0;
    return switch_to(f, t);
}

/* threads do kernel encerram por aqui: marca como morta e entra num hlt até o escalonador trocar */
void thread_exit(void) {
    cli();
    current->state = TH_DEAD;
    sti();
    for (;;) hlt();   /* a próxima interrupção de timer troca de thread (fim de fatia) */
}
