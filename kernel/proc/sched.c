/*
 * mOS — threads e escalonador (SMP com trava grande do kernel).
 *
 * A troca de contexto acontece na saída de interrupções: isr_common salva todos os registradores
 * (e o estado FXSAVE logo abaixo) na pilha da thread interrompida e restaura o frame que
 * isr_dispatch devolver. Uma thread nova começa com um frame "falso" que o iretq consome.
 *
 * Núcleos: cada um tem o próprio estado (pcpu_t: thread atual, ociosa, fatia, CR3). O BSP roda a
 * thread 0 (desktop), as threads do kernel e threads de usuário; os APs rodam só threads de usuário
 * (e a própria thread ociosa, que também atende os trabalhos de par_for). A fatia dos APs vem do
 * temporizador do LAPIC (vetor 0xF0, 1 ms); a do BSP, do PIT.
 *
 * Trava grande (BKL, kernel.h): todo código do kernel roda com ela, menos os tratadores de IRQ do PIC,
 * os trabalhos de par_for e os laços ociosos. Quem entra vindo do ring 3 por syscall ou falta espera
 * por ela; temporizadores só a tentam (se estiver ocupada, a thread de usuário segue rodando). Ela é
 * solta só em sched_post, chamada pelo isr_common já na pilha da thread que vai rodar: até ali o núcleo
 * ainda está na pilha da thread anterior, que outro núcleo poderia pegar.
 *
 * Política do BSP (como antes): a thread 0 só cede o processador quando é interrompida num `hlt`; as
 * demais rodam em fatias de SCHED_SLICE_MS e, ao fim de cada fatia, o desktop volta. O desktop solta a
 * trava em todo hlt() que faz (cpu_idle) — é aí que as syscalls dos outros núcleos andam.
 *
 * Outros núcleos rodando um processo que mudou: smp_tlb_shootdown (TLB) e smp_kick_proc (threads que
 * morreram) marcam `pending` no núcleo alvo e, se ele está no ring 3, mandam o IPI 0xF2 e esperam ele
 * entrar no kernel. A marca e `in_user` seguem o protocolo de Dekker (seq_cst dos dois lados): o alvo
 * publica in_user = 1 e só então relê pending antes de voltar ao usuário.
 */
#include "proc.h"
#include "smp_perf.h"

#define KSTACK_SIZE (32u << 10)
#define PEND_FLUSH   1u        /* recarregar o CR3 antes de voltar ao ring 3 */
#define PEND_RESCHED 2u        /* a thread atual pode ter morrido: passar pelo escalonador */
#define VEC_KICK     0xF2

typedef struct {
    thread_t *cur, *idle, *last_user;
    u64 slice_start, last_account;
    u64 cr3;                   /* CR3 em uso (para o shootdown) */
    volatile u32 pending;
    volatile u32 in_user;      /* 1 = rodando (ou a caminho do) ring 3 */
    volatile u8 release;       /* sched_post solta a trava */
    volatile u8 spinning;      /* esperando a trava com interrupções abertas (BSP) */
    bool online;
    thread_t *rq_h[5], *rq_t[5];   /* filas por classe de QoS, maior prioridade no índice 0 */
    int rq_n;
    volatile u16 flush_pcid;       /* PCID a invalidar; 0xFFFF = o TLB inteiro */
} pcpu_t;

static thread_t thread0 = {.id = 0, .name = "desktop", .state = TH_RUNNING};
static pcpu_t pc[MAX_CPUS] = {[0] = {.cur = &thread0, .idle = &thread0, .online = true}};
static thread_t *threads = &thread0;
static int next_tid = 1;
static int ncpu = 1;                    /* índices de núcleo em uso: [0, ncpu) */
static u64 kernel_cr3;
static volatile int bkl_owner = 0;      /* o BSP nasce com a trava */
static volatile bool desk_idle;         /* a thread 0 está em cpu_idle (sem a trava) */

static bool thread_parked(const thread_t *t);
static inline pcpu_t *me_pc(void) { return &pc[this_cpu()]; }
thread_t *thread_current(void) { thread_t *t = me_pc()->cur; return t ? t : &thread0; }
thread_t *sched_threads(void) { return threads; }

/* ------------------------------------------------------------ trava grande */
static inline bool bkl_try_(int me) {
    int e = -1;
    if (__atomic_load_n(&bkl_owner, __ATOMIC_RELAXED) == me) return true;
    return __atomic_compare_exchange_n(&bkl_owner, &e, me, false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
}
static inline void bkl_drop(void) { __atomic_store_n(&bkl_owner, -1, __ATOMIC_RELEASE); }
bool bkl_held(void) { return __atomic_load_n(&bkl_owner, __ATOMIC_RELAXED) == this_cpu(); }

static int have_pcid, have_invpcid;
static u8 pcid_cached[MAX_CPUS][512];   /* 4096 bits: este núcleo já carregou o PCID */

static void pcid_note(int cpu, unsigned id) {
    if (cpu < 0 || cpu >= MAX_CPUS || id >= 4096) return;
    pcid_cached[cpu][id >> 3] |= (u8)(1u << (id & 7));
}
static int pcid_has(int cpu, unsigned id) {
    if (cpu < 0 || cpu >= MAX_CPUS || id >= 4096) return 0;
    return (pcid_cached[cpu][id >> 3] >> (id & 7)) & 1;
}
static void pcid_clear_all(int cpu) {
    if (cpu >= 0 && cpu < MAX_CPUS) __builtin_memset(pcid_cached[cpu], 0, sizeof pcid_cached[cpu]);
}
static void pcid_clear_one(int cpu, unsigned id) {
    if (cpu < 0 || cpu >= MAX_CPUS || id >= 4096) return;
    pcid_cached[cpu][id >> 3] &= (u8)~(1u << (id & 7));
}

static void invpcid_ctx(unsigned id) {
    struct { u64 pcid, addr; } d;
    d.pcid = id;
    d.addr = 0;
    __asm__ volatile("invpcid %1, %0" :: "r"((u64)2), "m"(d) : "memory");
}

static inline void flush_if_pending(pcpu_t *c) {
    u16 id;
    int cpu;
    if (!(__atomic_load_n(&c->pending, __ATOMIC_SEQ_CST) & PEND_FLUSH)) return;
    id = __atomic_exchange_n(&c->flush_pcid, 0, __ATOMIC_SEQ_CST);
    __atomic_and_fetch(&c->pending, ~PEND_FLUSH, __ATOMIC_SEQ_CST);
    cpu = (int)(c - pc);
    if (!have_pcid || id == 0xFFFF || !have_invpcid) {
        u64 cr3 = read_cr3() & ~(1ULL << 63);
        __asm__ volatile("mov %0,%%cr3" :: "r"(cr3) : "memory");
        pcid_clear_all(cpu);
        if (have_pcid) pcid_note(cpu, (unsigned)(cr3 & 0xFFF));
        return;
    }
    if (id) invpcid_ctx(id);
    pcid_clear_one(cpu, id);
    if (((read_cr3() & 0xFFF) == id)) pcid_note(cpu, id);
}

void cpu_pcid_init(void) {
    u32 a = 1, b = 0, c = 0, d = 0;
    u64 cr4;
    __asm__ volatile("cpuid" : "+a"(a), "=b"(b), "+c"(c), "=d"(d));
    if (!(c & (1u << 17))) return;
    __asm__ volatile("mov %%cr4,%0" : "=r"(cr4));
    if (!(cr4 & (1ULL << 17))) __asm__ volatile("mov %0,%%cr4" :: "r"(cr4 | (1ULL << 17)) : "memory");
    have_pcid = 1;
    a = 7; b = 0; c = 0; d = 0;
    __asm__ volatile("cpuid" : "+a"(a), "=b"(b), "+c"(c), "=d"(d));
    if (b & (1u << 10)) have_invpcid = 1;
}

u16 sched_alloc_pcid(void) {
    static u16 n = 1;
    u16 id = n;
    if (n < 4095) n++;
    return id;
}

/* espera a trava. No BSP abre as interrupções entre as tentativas: o PIT continua contando (código
 * do kernel em outro núcleo pode estar medindo tempo) e o PS/2 não perde bytes; essas interrupções
 * aninhadas fazem só o mínimo (sched_nested) */
void sched_klock(void) {
    int me = this_cpu();
    pcpu_t *c = &pc[me];
    if (!bkl_try_(me)) {
        c->spinning = 1;
        while (!bkl_try_(me)) {
            if (me == 0) __asm__ volatile("sti; pause; cli" ::: "memory");
            else __asm__ volatile("pause");
        }
        c->spinning = 0;
    }
    flush_if_pending(c);
}
bool sched_ktry(void) {
    int me = this_cpu();
    if (!bkl_try_(me)) return false;
    flush_if_pending(&pc[me]);
    return true;
}
bool sched_nested(void) { return me_pc()->spinning; }

/* todo hlt() (kernel.h): se for o desktop no BSP com a trava e interrupções ligadas, dorme sem a trava, e as
 * syscalls de outros núcleos andam; senão é um hlt comum */
void cpu_idle(void) {
    u64 fl = irq_save();
    if (this_cpu() != 0 || pc[0].cur != &thread0 || !bkl_held() || !(fl & 0x200)) { irq_restore(fl); hlt_raw(); return; }
    desk_idle = true;
    bkl_drop();
    __asm__ volatile("sti; hlt; cli" ::: "memory");
    while (!bkl_try_(0)) __asm__ volatile("sti; pause; cli" ::: "memory");
    desk_idle = false;
    flush_if_pending(&pc[0]);
    irq_restore(fl);
}

/* GS de usuário por thread: o macOS guarda os dados específicos de cada thread (pthread_getspecific,
 * TLS do Chromium) em gs:[chave*8]. O kernel não usa GS, então só troca a base (MSR IA32_GS_BASE) na
 * troca de contexto; CR4.FSGSBASE deixa a libSystem definir a própria base com wrgsbase. */
#define MSR_GS_BASE 0xC0000101u
static bool gs_switch;
static u64 rd_msr(u32 m) { u32 lo, hi; __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(m)); return (u64)hi << 32 | lo; }
static void wr_msr(u32 m, u64 v) { __asm__ volatile("wrmsr" :: "c"(m), "a"((u32)v), "d"((u32)(v >> 32))); }

void sched_init(void) {
    cpu_pcid_init();
    kernel_cr3 = read_cr3();
    pc[0].slice_start = pc[0].last_account = timer_ticks();
    pc[0].cr3 = kernel_cr3;
    u32 a = 7, b = 0, c = 0, d = 0;
    __asm__ volatile("cpuid" : "+a"(a), "=b"(b), "+c"(c), "=d"(d));
    if (b & 1) {                                     /* CPUID.7.EBX[0]: FSGSBASE */
        u64 cr4;
        __asm__ volatile("mov %%cr4,%0" : "=r"(cr4));
        __asm__ volatile("mov %0,%%cr4" :: "r"(cr4 | (1ULL << 16)));
    }
    gs_switch = true;
}

/* AP acordado (arch/smp.c): thread ociosa = o próprio laço do AP, na pilha de partida */
void sched_ap_init(int cpu) {
    if (cpu <= 0 || cpu >= MAX_CPUS) return;
    thread_t *t = kcalloc(1, sizeof *t);
    if (!t) return;
    t->id = -cpu;
    snprintf(t->name, sizeof t->name, "ocioso %d", cpu);
    t->state = TH_RUNNING;
    pcpu_t *c = &pc[cpu];
    c->idle = c->cur = t;
    c->cr3 = read_cr3();
    c->slice_start = c->last_account = timer_ticks();
    __atomic_store_n(&c->online, true, __ATOMIC_SEQ_CST);
    int n = cpu + 1, old = __atomic_load_n(&ncpu, __ATOMIC_RELAXED);
    while (old < n && !__atomic_compare_exchange_n(&ncpu, &old, n, false, __ATOMIC_SEQ_CST, __ATOMIC_RELAXED)) {}
}

/* a thread está em execução em algum núcleo */
static bool on_cpu(const thread_t *t) {
    for (int i = 0; i < ncpu; i++) if (pc[i].cur == t) return true;
    return false;
}

static int admit(const thread_t *t, int cpu) {
    u64 m;
    if (!t || cpu < 0 || cpu >= 64) return 0;
    if (!t->proc && cpu != 0) return 0;
    m = t->affinity ? t->affinity : ~(u64)0;
    return (m & (1ULL << cpu)) != 0;
}
static int pick_cpu(const thread_t *t) {
    int best = 0, load = 0x7fffffff, i, n;
    n = __atomic_load_n(&ncpu, __ATOMIC_RELAXED);
    if (n < 1) n = 1;
    if (n > MAX_CPUS) n = MAX_CPUS;
    for (i = 0; i < n; i++) {
        if (i && !pc[i].online) continue;
        if (!admit(t, i)) continue;
        if (pc[i].rq_n < load) { load = pc[i].rq_n; best = i; }
    }
    return best;
}
static void rq_unlink(thread_t *t) {
    pcpu_t *c;
    int q;
    thread_t **pp, *prev = NULL;
    if (!t || !t->on_rq) return;
    q = t->qos < 5 ? t->qos : 2;
    c = &pc[t->rq_cpu < MAX_CPUS ? t->rq_cpu : 0];
    for (pp = &c->rq_h[q]; *pp && *pp != t; pp = &(*pp)->rq_next) prev = *pp;
    if (!*pp) { t->on_rq = 0; return; }
    *pp = t->rq_next;
    if (c->rq_t[q] == t) c->rq_t[q] = prev;
    t->rq_next = NULL;
    t->on_rq = 0;
    if (c->rq_n) c->rq_n--;
}
static void rq_put(thread_t *t) {
    pcpu_t *c;
    int q, cpu;
    if (!t || t->on_rq || t == &thread0 || t->state != TH_READY || thread_parked(t)) return;
    if (t->id < 0) return;                 /* ocioso do AP */
    q = t->qos < 5 ? t->qos : 2;
    cpu = pick_cpu(t);
    c = &pc[cpu];
    t->rq_next = NULL;
    t->rq_cpu = (u8)cpu;
    if (c->rq_t[q]) c->rq_t[q]->rq_next = t;
    else c->rq_h[q] = t;
    c->rq_t[q] = t;
    t->on_rq = 1;
    c->rq_n++;
}
void sched_make_ready(thread_t *t) {
    if (t->state == TH_DEAD || t->state == TH_RUNNING) return;
    t->state = TH_READY;
    rq_put(t);
}
void sched_set_qos(thread_t *t, unsigned darwin_qos) {
    if (!t) return;
    rq_unlink(t);
    t->qos = (u8)smp_qos_from_darwin(darwin_qos);
    if (t->state == TH_READY) rq_put(t);
}
void sched_set_affinity(thread_t *t, u64 cpu_mask) {
    if (!t) return;
    t->affinity = cpu_mask;
    rq_unlink(t);
    if (t->state == TH_READY) rq_put(t);
}

/* thread_suspend (SYS_THREADCTL, process.c): t já tem suspend > 0. Se estiver rodando em outro núcleo, o IPI de
 * reescalonamento a tira de lá (sched_dead_check); quem chamou espera com sched_thread_stopped antes de ler o frame */
static void kick_wait(int cpu, u32 bit);
void sched_kick_thread(thread_t *t) {
    int me = this_cpu();
    for (int i = 0; i < ncpu; i++)
        if (i != me && pc[i].online && pc[i].cur == t) kick_wait(i, PEND_RESCHED);
}
/* suspensa e fora de qualquer núcleo: o frame salvo (t->frame) tem os registradores de usuário dela */
bool sched_thread_stopped(const thread_t *t) { return t->suspend && !on_cpu(t); }

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
    u8 *fx = FPU_AREA_OF(f);                    /* XSAVE/FXSAVE (proc.h); o cabeçalho do XSAVE zerado = estado inicial */
    memset(fx, 0, FPU_AREA);
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
    t->qos = 2;                                 /* QOS_CLASS_DEFAULT */
    return t;
}

static void link_thread(thread_t *t) {
    u64 fl = irq_save();
    t->state = TH_READY;
    t->next = threads->next;                    /* depois da thread 0 */
    threads->next = t;
    rq_put(t);
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
    u64 *sp = (u64 *)(((t->kstack_top - 2048) & ~15ULL) - 40);   /* acima: o frame e a FPU_AREA (até ~1,3 KiB) */
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
    memcpy(FPU_AREA_OF(t->frame), FPU_AREA_OF(src), FPU_AREA);
    link_thread(t);
    return t;
}

/* libera threads mortas que nenhum núcleo está rodando (a atual de cada um ainda está na pilha dela) */
static void reap(void) {
    for (thread_t **pp = &threads->next; *pp;) {
        thread_t *t = *pp;
        if (t->state == TH_DEAD && !on_cpu(t)) {
            extern bool kheap_live(const void *p);
            if (!kheap_live(t) || !kheap_live(t->kstack))   /* diagnóstico do kfree duplo (docs/porte-brew.md) */
                kprintf("reap: thread %d \"%s\" (pid %d) já liberada: t=%p vivo=%d kstack=%p vivo=%d\n", t->id, t->name,
                        t->proc ? t->proc->pid : -1, t, kheap_live(t), t->kstack, kheap_live(t->kstack));
            *pp = t->next;
            rq_unlink(t);
            for (int i = 0; i < ncpu; i++) if (pc[i].last_user == t) pc[i].last_user = NULL;
            if (t->proc) proc_thread_exited(t);
            kfree(t->kstack);
            kfree(t);
        } else pp = &t->next;
    }
}

static void account(pcpu_t *c) {
    u64 now = timer_ticks();
    c->cur->cpu_ms += now - c->last_account;
    c->last_account = now;
}

static void wake_sleepers(void) {
    u64 now = timer_ticks();
    for (thread_t *t = threads; t; t = t->next) {
        if (t->state == TH_SLEEPING && now >= t->wake_at) { t->state = TH_READY; rq_put(t); }
        /* recepção Mach com prazo (machipc.c): o rax de "tempo esgotado" já está no frame */
        else if (t->state == TH_WAITING && t->wait_pid == -7 && t->mach_deadline && now >= t->mach_deadline) {
            t->mach_deadline = 0;
            t->mach_wait = NULL;
            t->mach_peek = false;
            t->state = TH_READY;
            rq_put(t);
        }
        /* SYS_FUTEX_WAIT com prazo (process.c): volta com -ETIMEDOUT */
        else if (t->state == TH_WAITING && t->wait_pid == -8 && t->wake_at && now >= t->wake_at) {
            ((int_frame_t *)t->frame)->rax = (u64)-60;      /* -MOS_ETIMEDOUT (syscall.h) */
            t->wake_at = 0;
            t->futex_va = 0;
            t->state = TH_READY;
            rq_put(t);
        }
        /* SYS_GPU MOS_GPU_FENCE_WAIT (wait_pid = -12): prazo em wake_at */
        else if (t->state == TH_WAITING && t->wait_pid == -12 && t->wake_at && now >= t->wake_at) {
            ((int_frame_t *)t->frame)->rax = (u64)-60;
            t->wake_at = 0;
            t->futex_va = 0;
            t->state = TH_READY;
        }
    }
}

/* thread_suspend do Mach, ou o processo inteiro parado (SIGSTOP e os de job control) */
static bool thread_parked(const thread_t *t) {
    return t->suspend || (t->proc && t->proc->stopped);
}

/* varre quem ficou READY fora da fila (acordado por outro subsistema) e o enfileira */
static void sweep_ready(void) {
    for (thread_t *t = threads; t; t = t->next)
        if (t->state == TH_READY && !t->on_rq && !on_cpu(t)) rq_put(t);
}
static thread_t *take_q(pcpu_t *c, int me) {
    for (int q = 0; q < 5; q++) {
        thread_t *t = c->rq_h[q];
        int guard = 0;
        while (t && guard++ < 100000) {
            thread_t *n = t->rq_next;
            if (t->state != TH_READY || thread_parked(t) || on_cpu(t)) {
                rq_unlink(t);
                if (t->state == TH_READY && !thread_parked(t) && !on_cpu(t)) rq_put(t);
                t = n;
                continue;
            }
            if (!admit(t, me)) { t = n; continue; }
            rq_unlink(t);
            return t;
        }
    }
    return NULL;
}
/* fila local, da classe mais urgente para a menos; se vazia, rouba da CPU mais carregada */
static thread_t *next_ready(int me, thread_t *from) {
    int best, bn, i, n;
    thread_t *t;
    (void)from;
    sweep_ready();
    t = take_q(&pc[me], me);
    if (t) return t;
    best = -1; bn = 0; n = ncpu;
    for (i = 0; i < n; i++) if (i != me && pc[i].rq_n > bn) { bn = pc[i].rq_n; best = i; }
    return best >= 0 ? take_q(&pc[best], me) : NULL;
}

/* Preempção de um processo arm64 traduzido: a reserva do LDXR não sobrevive à troca.
 * A rt_thread_t fica na base da pilha do host (1 MiB, alinhada). Só escreve com a página
 * presente. excl_valid = base+860 e reserved[8] = base+960, conferidos em a64_xmon.c.
 * O contador global fica para a thread recolher quando voltar a rodar: daqui não há
 * endereço estável dele, e zerar inflight antes do store abriria uma janela de ABA. */
static void a64_xmon_on_switch(thread_t *from, int_frame_t *f) {
    if (!from || !from->proc || !(f->cs & 3)) return;
    u64 sp = f->rsp;
    if (sp < USER_BASE || sp >= USER_END) return;
    u64 base = sp & ~0xFFFFFULL;
    if (base < USER_BASE || base + 968 > USER_END) return;
    if (!uvm_pte(from->proc, base)) return;
    volatile u64 *hdr = (volatile u64 *)base;
    if (hdr[0] != 0x4441455254343641ULL || hdr[1] != base) return;
    *(volatile u32 *)(base + 860) = 0;
    (*(volatile u64 *)(base + 960))++;
}

static u64 switch_to(pcpu_t *c, int_frame_t *f, thread_t *to) {
    thread_t *from = c->cur;
    if (to != from) a64_xmon_on_switch(from, f);
    if (to == from) { from->state = TH_RUNNING; return (u64)f; }
    account(c);
    from->frame = (u64)f;
    if (from->state == TH_RUNNING) { from->state = TH_READY; rq_put(from); }
    if (from != &thread0 && from != c->idle) c->last_user = from;
    if (gs_switch) { from->gs_base = rd_msr(MSR_GS_BASE); wr_msr(MSR_GS_BASE, to->gs_base); }
    c->cur = to;
    to->state = TH_RUNNING;
    c->slice_start = timer_ticks();
    {
        u64 phys = to->proc ? (u64)to->proc->pml4 : kernel_cr3;
        unsigned id = 0;
        u64 ncr3, cur;
        phys &= ~0xFFFULL;
        if (to->proc) {
            if (!to->proc->pcid) to->proc->pcid = sched_alloc_pcid();
            id = to->proc->pcid;
        }
        ncr3 = have_pcid ? (phys | id) : phys;
        cur = read_cr3() & ~(1ULL << 63);
        if ((cur & ~0xFFFULL) != phys || (have_pcid && (unsigned)(cur & 0xFFF) != id))
            __asm__ volatile("mov %0,%%cr3" :: "r"(ncr3) : "memory");
        c->cr3 = ncr3;
        if (have_pcid) pcid_note((int)(c - pc), id);
    }
    if (to->kstack) tss_set_rsp0(to->kstack_top);
    return to->frame;
}

/* chamado ao fim de interrupções de hardware, com a trava */
static u64 irq_exit(int_frame_t *f, bool timer);
__attribute__((weak)) u64 proc_signal_check(int_frame_t *f) { return (u64)f; }

u64 sched_irq_exit(int_frame_t *f, bool timer) {
    u64 r = irq_exit(f, timer);
    /* voltando para código de usuário: entrega sinais pendentes (ex.: Ctrl+C num laço infinito) */
    if (((int_frame_t *)r)->cs & 3) r = proc_signal_check((int_frame_t *)r);
    return r;
}

static u64 irq_exit(int_frame_t *f, bool timer) {
    int me = this_cpu();
    pcpu_t *c = &pc[me];
    thread_t *cur = c->cur;
    if (!timer || (!threads->next && cur == c->idle)) return (u64)f;   /* nenhuma outra thread */
    wake_sleepers();
    if (me == 0 && cur == &thread0) {
        /* o desktop só cede quando a interrupção o pegou num hlt (ocioso); o hlt tem 1 byte */
        if ((f->cs & 3) || *(u8 *)(f->rip - 1) != 0xF4) return (u64)f;
        reap();
        thread_t *t = next_ready(0, c->last_user);
        return t ? switch_to(c, f, t) : (u64)f;
    }
    if (cur == c->idle) {                       /* AP ocioso (parado no hlt do laço dele) */
        thread_t *t = next_ready(me, c->last_user);
        return t ? switch_to(c, f, t) : (u64)f;
    }
    if (cur->state == TH_RUNNING && !thread_parked(cur) &&
        timer_ticks() - c->slice_start < (u64)(cur->qos == 0 ? 8 : cur->qos == 1 ? 6 : cur->qos == 3 ? 2 : cur->qos == 4 ? 1 : SCHED_SLICE_MS))
        return (u64)f;
    reap();
    if (me == 0) return switch_to(c, f, &thread0);   /* fim da fatia: o desktop roda */
    thread_t *t = next_ready(me, cur);
    if (!t) t = cur->state == TH_RUNNING && !thread_parked(cur) ? cur : c->idle;
    return switch_to(c, f, t);
}

/* a thread atual bloqueou ou terminou (chamado em syscalls, com IF = 0 e a trava) */
u64 sched_switch(int_frame_t *f) {
    int me = this_cpu();
    pcpu_t *c = &pc[me];
    wake_sleepers();
    thread_t *t = next_ready(me, c->cur);
    if (!t || t == c->cur) t = c->cur->state == TH_RUNNING && !thread_parked(c->cur) ? c->cur : c->idle;
    return switch_to(c, f, t);
}

/* threads do kernel encerram por aqui: marca como morta e entra num hlt até o escalonador trocar */
void thread_exit(void) {
    cli();
    me_pc()->cur->state = TH_DEAD;
    sti();
    for (;;) hlt();   /* a próxima interrupção de timer troca de thread (fim de fatia) */
}

/* ------------------------------------------------------------ entrada e saída do kernel (arch/cpu.c) */
static inline bool is_kidle(pcpu_t *c, thread_t *t) { return t == &thread0 ? desk_idle : t == c->idle; }

void sched_kentry(int_frame_t *f) {
    if (!(f->cs & 3)) return;
    pcpu_t *c = me_pc();
    __atomic_store_n(&c->in_user, 0, __ATOMIC_SEQ_CST);
    flush_if_pending(c);
}

/* depois de pegar a trava numa entrada do ring 3: a thread pode ter sido morta por outro núcleo enquanto
 * rodava (finish/execve). Devolve o frame de outra thread, ou 0 se a atual segue viva. */
u64 sched_dead_check(int_frame_t *f) {
    pcpu_t *c = me_pc();
    __atomic_and_fetch(&c->pending, ~PEND_RESCHED, __ATOMIC_SEQ_CST);
    /* só a morta sai aqui. Uma suspensa (thread_suspend) não: isto roda na entrada da syscall, antes dela, e a thread
     * voltaria depois com rax = o número da syscall no lugar do resultado (o sampler do Chromium derrubava o Codex). Ela
     * sai da CPU em sched_kexit, na volta ao ring 3, ou no irq_exit. */
    return c->cur->state == TH_DEAD ? sched_switch(f) : 0;
}

/* decide o que fazer com a trava antes de voltar para o frame r */
u64 sched_kexit(u64 r) {
    int me = this_cpu();
    pcpu_t *c = &pc[me];
    for (;;) {
        int_frame_t *f = (int_frame_t *)r;
        bool held = __atomic_load_n(&bkl_owner, __ATOMIC_RELAXED) == me;
        if ((f->cs & 3) && c->cur->proc && (c->cur->suspend || c->cur->proc->stopped)) {
            /* thread_suspend (SYS_THREADCTL) ou processo parado (SIGSTOP): a syscall ou a falta já terminou; sai da
             * CPU antes de voltar ao usuário. O frame salvo é o que o thread_get_state lê no caso do suspend. */
            if (!held) sched_klock();
            r = sched_switch(f);
            continue;
        }
        if (f->cs & 3) {
            __atomic_store_n(&c->in_user, 1, __ATOMIC_SEQ_CST);
            u32 pend = __atomic_load_n(&c->pending, __ATOMIC_SEQ_CST);
            if (pend) {
                __atomic_store_n(&c->in_user, 0, __ATOMIC_SEQ_CST);
                flush_if_pending(c);
                if (pend & PEND_RESCHED) {
                    sched_klock();
                    u64 d = sched_dead_check(f);
                    if (d) r = d;
                }
                continue;
            }
            if (held) c->release = 1;
            return r;
        }
        if (is_kidle(c, c->cur)) { if (held) c->release = 1; return r; }
        if (!held) sched_klock();               /* thread do kernel (ou o desktop fora do hlt): roda com a trava */
        return r;
    }
}

/* isr_common, já na pilha da thread que vai rodar */
void sched_post(void) {
    pcpu_t *c = me_pc();
    if (c->release) { c->release = 0; bkl_drop(); }
}

/* ------------------------------------------------------------ IPIs para outros núcleos */
static void kick_mark(int cpu, u32 bit, u16 pcid) {
    pcpu_t *c = &pc[cpu];
    if (bit & PEND_FLUSH) {
        u16 old = __atomic_load_n(&c->flush_pcid, __ATOMIC_SEQ_CST);
        u16 neu = (!old || old == pcid) ? pcid : (u16)0xFFFF;
        __atomic_store_n(&c->flush_pcid, neu, __ATOMIC_SEQ_CST);
    }
    __atomic_or_fetch(&c->pending, bit, __ATOMIC_SEQ_CST);
}
static void kick_wait_marked(int cpu, u32 bit, int fire) {
    pcpu_t *c = &pc[cpu];
    if (fire && __atomic_load_n(&c->in_user, __ATOMIC_SEQ_CST)) smp_send_ipi(cpu, VEC_KICK);
    if (!fire) return;
    for (u64 spins = 0; __atomic_load_n(&c->in_user, __ATOMIC_SEQ_CST) && (__atomic_load_n(&c->pending, __ATOMIC_SEQ_CST) & bit);
         spins++) {
        __asm__ volatile("pause");
        if (spins == 200000000ULL) { kprintf("smp: núcleo %d não respondeu ao IPI (pendente %u)\n", cpu, c->pending); break; }
    }
}
static void kick_wait(int cpu, u32 bit) {
    kick_mark(cpu, bit, 0);
    kick_wait_marked(cpu, bit, 1);
}

void smp_tlb_shootdown(u64 cr3) {
    int me = this_cpu(), i, n;
    unsigned char cached[64], cur[64], online[64];
    unsigned long long mask;
    u16 pcid = 0;
    process_t *pr;
    cr3 &= ~0xFFFULL;
    for (pr = proc_list(); pr; pr = pr->next)
        if (pr->pml4 && ((u64)pr->pml4 & ~0xFFFULL) == cr3) { pcid = pr->pcid; break; }
    n = ncpu;
    if (n > 64) n = 64;
    for (i = 0; i < n; i++) {
        online[i] = pc[i].online ? 1 : 0;
        cur[i] = ((pc[i].cr3 & ~0xFFFULL) == cr3) ? 1 : 0;
        cached[i] = (have_pcid && pcid && pcid_has(i, pcid)) ? 1 : 0;
    }
    mask = smp_shoot_mask((unsigned)n, me, cached, cur, online);
    for (i = 0; i < n; i++) if (mask & (1ULL << i)) kick_mark(i, PEND_FLUSH, pcid ? pcid : (u16)0xFFFF);
    __asm__ volatile("mfence" ::: "memory");
    /* lote: marca todas, dispara os IPIs, só então espera. Quem está no kernel vê a marca na volta. */
    for (i = 0; i < n; i++)
        if ((mask & (1ULL << i)) && __atomic_load_n(&pc[i].in_user, __ATOMIC_SEQ_CST)) smp_send_ipi(i, VEC_KICK);
    for (i = 0; i < n; i++)
        if ((mask & (1ULL << i)) && __atomic_load_n(&pc[i].in_user, __ATOMIC_SEQ_CST)) kick_wait_marked(i, PEND_FLUSH, 1);
}

/* threads de p foram marcadas TH_DEAD (fim do processo, execve): tira dos outros núcleos as que estão rodando
 * no ring 3, antes de a memória do processo ser liberada */
void smp_kick_proc(process_t *p) {
    int me = this_cpu();
    for (int i = 0; i < ncpu; i++) {
        thread_t *t = pc[i].cur;
        if (i != me && pc[i].online && t && t->proc == p) kick_wait(i, PEND_RESCHED);
    }
}
