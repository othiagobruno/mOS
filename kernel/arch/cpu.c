/* mOS — GDT, TSS, IDT, PIC 8259, PIT 8254 e tratamento de exceções */
#include "kernel.h"

/* ------------------------------------------------------------------ GDT */
typedef struct PACKED { u16 limit; u64 base; } dtr_t;
typedef struct PACKED {
    u32 rsv0; u64 rsp[3]; u64 rsv1; u64 ist[7]; u64 rsv2; u16 rsv3; u16 iopb;
} tss_t;

/* 0x00..0x20: nulo, código/dados do kernel e do usuário; a partir de 0x28, um TSS (16 bytes) por núcleo */
static u64 gdt[5 + 2 * MAX_CPUS] ALIGNED(16);
static tss_t tss[MAX_CPUS] ALIGNED(16);
static u8 ist_stack[16384] ALIGNED(16);
extern void gdt_load(dtr_t *d, u16 tss_sel);
extern void idt_load(dtr_t *d);

static void tss_desc(int cpu, u64 ist_top) {
    tss_t *t = &tss[cpu];
    memset(t, 0, sizeof *t);
    t->ist[0] = ist_top;
    t->iopb = sizeof *t;
    u64 base = (u64)t, lim = sizeof *t - 1;
    gdt[5 + 2 * cpu] = (lim & 0xFFFF) | ((base & 0xFFFFFF) << 16) | (0x89ULL << 40) | (((lim >> 16) & 0xF) << 48) | (((base >> 24) & 0xFF) << 56);
    gdt[6 + 2 * cpu] = base >> 32;
}

void gdt_init(void) {
    gdt[0] = 0;
    gdt[1] = 0x00AF9A000000FFFFULL; /* 0x08 código kernel 64 bits */
    gdt[2] = 0x00CF92000000FFFFULL; /* 0x10 dados kernel */
    gdt[3] = 0x00AFFA000000FFFFULL; /* 0x18 código usuário */
    gdt[4] = 0x00CFF2000000FFFFULL; /* 0x20 dados usuário */
    tss_desc(0, (u64)ist_stack + sizeof(ist_stack));
    static dtr_t d;
    d.limit = sizeof(gdt) - 1;
    d.base = (u64)gdt;
    gdt_load(&d, 0x28);
}

/* AP: o próprio TSS (rsp0 das threads que ele roda; pilha IST para NMI e falta dupla) */
void cpu_tss_init(int cpu, u64 ist_top) {
    if (cpu <= 0 || cpu >= MAX_CPUS) return;
    tss_desc(cpu, ist_top);
    u16 sel = (u16)(0x28 + 16 * cpu);
    __asm__ volatile("ltr %0" :: "r"(sel));
}

/* ------------------------------------------------------------------ IDT */
typedef struct PACKED { u16 off0, sel; u8 ist, type; u16 off1; u32 off2, zero; } idt_gate_t;
static idt_gate_t idt[256] ALIGNED(16);
extern u64 isr_table[];
static irq_handler_t irq_handlers[17];

static const char *exc_names[32] = {
    "Divisão por zero", "Debug", "NMI", "Breakpoint", "Overflow", "Bound", "Opcode inválido",
    "Dispositivo indisponível", "Falta dupla", "Coprocessador", "TSS inválido", "Segmento ausente",
    "Falha de pilha", "Proteção geral", "Falta de página", "Reservado", "Erro x87", "Alinhamento",
    "Machine check", "Erro SIMD", "Virtualização", "Controle", "Res", "Res", "Res", "Res", "Res",
    "Res", "Hypervisor", "VMM", "Segurança", "Res"
};

static void set_gate(int n, u64 h, u8 ist) {
    idt[n].off0 = h & 0xFFFF;
    idt[n].sel = 0x08;
    idt[n].ist = ist;
    idt[n].type = 0x8E;
    idt[n].off1 = (h >> 16) & 0xFFFF;
    idt[n].off2 = h >> 32;
    idt[n].zero = 0;
}

extern void isr128(void), isr240(void), isr242(void), isr255(void), isr_wake(void);

void tss_set_rsp0(u64 rsp0) { tss[this_cpu()].rsp[0] = rsp0; }

void idt_init(void) {
    for (int i = 0; i < 49; i++) set_gate(i, isr_table[i], (i == 8 || i == 2) ? 1 : 0);
    set_gate(0x80, (u64)isr128, 0);
    idt[0x80].type = 0xEE;          /* DPL 3: o ring 3 pode chamar int 0x80 */
    set_gate(0xF0, (u64)isr240, 0);
    set_gate(0xF1, (u64)isr_wake, 0);  /* acorda os APs de par_for (arch/smp.c) */
    set_gate(0xF2, (u64)isr242, 0);    /* "olhe o escalonador": TLB e threads mortas em outro núcleo (proc/sched.c) */
    set_gate(0xFF, (u64)isr255, 0);
    static dtr_t d;
    d.limit = sizeof(idt) - 1;
    d.base = (u64)idt;
    idt_load(&d);
}

void irq_install(int irq, irq_handler_t h) {
    if (irq >= 0 && irq < 17) irq_handlers[irq] = h;
    if (irq < 16) pic_unmask(irq);
}

extern void panic_screen(const char *msg, int_frame_t *f);
/* proc/: troca de contexto, syscalls e falhas de processos (fracos = kernel sem processos) */
__attribute__((weak)) u64 sched_irq_exit(int_frame_t *f, bool timer) { (void)timer; return (u64)f; }
__attribute__((weak)) u64 proc_syscall(int_frame_t *f) { f->rax = (u64)-1; return (u64)f; }
__attribute__((weak)) u64 proc_fault(int_frame_t *f, u64 cr2) { (void)cr2; return (u64)f; }
__attribute__((weak)) void lapic_eoi(void) {}
/* trava grande do kernel e estado por núcleo (proc/sched.c); fracos = um núcleo, sem trava */
__attribute__((weak)) bool sched_nested(void) { return false; }
__attribute__((weak)) void sched_kentry(int_frame_t *f) { (void)f; }
__attribute__((weak)) void sched_klock(void) {}
__attribute__((weak)) bool sched_ktry(void) { return true; }
__attribute__((weak)) u64 sched_dead_check(int_frame_t *f) { (void)f; return 0; }
__attribute__((weak)) u64 sched_kexit(u64 r) { return r; }
__attribute__((weak)) void sched_post(void) {}

static void exc_panic(int_frame_t *f) {
    char msg[160];
    u64 cr2;
    __asm__ volatile("mov %%cr2,%0" : "=r"(cr2));
    snprintf(msg, sizeof(msg), "%s (vetor %llu, erro 0x%llx) em RIP=0x%llx CR2=0x%llx [núcleo %d]",
             exc_names[f->vector], f->vector, f->error, f->rip, cr2, this_cpu());
    kprintf("\n*** EXCEÇÃO: %s\n", msg);
    {   /* para traduzir em funções (build com /map): base de carga, registradores e endereços do kernel na pilha */
        extern char __ImageBase;
        u64 kb = (u64)&__ImageBase;
        kprintf("*** kernel em 0x%llx (RIP-base = 0x%llx) rax=%llx rbx=%llx rcx=%llx rdx=%llx rsi=%llx rdi=%llx\n",
                kb, f->rip - kb, f->rax, f->rbx, f->rcx, f->rdx, f->rsi, f->rdi);
        const u64 *sp = (const u64 *)(f->rsp & ~7ULL);
        int shown = 0;
        for (int i = 0; i < 512 && shown < 16; i++) {
            u64 v = sp[i];
            if (kb && v > kb && v < kb + 0x1000000ULL) { kprintf("***   [rsp+%d] base+0x%llx\n", i * 8, v - kb); shown++; }
        }
    }
    panic_screen(msg, f);
    for (;;) { cli(); hlt(); }
}

/* IRQs do PIC (PIT, PS/2; só chegam ao BSP): rodam sem a trava grande — só mexem nos próprios buffers */
static int pic_irq(int_frame_t *f) {
    int irq = (int)f->vector - 32;
    if (irq >= 0 && irq < 17 && irq_handlers[irq]) irq_handlers[irq](f);
    if (irq >= 8 && irq < 16) outb(0xA0, 0x20);
    if (irq < 16) outb(0x20, 0x20);
    return irq;
}

/* devolve o frame a ser restaurado por isr_common (outro = troca de contexto).
 * 0xF0 = temporizador do LAPIC (fatia nos APs), 0xF2 = IPI de "olhe o escalonador/TLB" (proc/sched.c). */
u64 isr_dispatch(int_frame_t *f) {
    if (sched_nested()) {           /* interrupção no meio da espera pela trava grande: só o mínimo */
        if (f->vector < 32) exc_panic(f);
        if (f->vector == 0xF0 || f->vector == 0xF2) lapic_eoi();
        else if (f->vector != 0xFF) pic_irq(f);
        return (u64)f;
    }
    sched_kentry(f);
    u64 r;
    if (f->vector == 0x80) {
        sched_klock();
        r = sched_dead_check(f);
        if (!r) r = proc_syscall(f);
    } else if (f->vector == 0xF0 || f->vector == 0xF2) {
        lapic_eoi();
        r = sched_ktry() ? sched_irq_exit(f, this_cpu() != 0 || f->vector == 0xF2) : (u64)f;
    } else if (f->vector == 0xFF) {
        r = (u64)f;                                           /* espúria: sem EOI */
    } else if (f->vector < 32) {
        if ((f->cs & 3) != 3 || f->vector == 8 || f->vector == 2 || f->vector == 18) exc_panic(f);
        u64 cr2;
        __asm__ volatile("mov %%cr2,%0" : "=r"(cr2));
        sched_klock();
        r = sched_dead_check(f);
        if (!r) r = proc_fault(f, cr2);                       /* só o processo morre */
    } else {
        int irq = pic_irq(f);
        r = sched_ktry() ? sched_irq_exit(f, irq == 0) : (u64)f;
    }
    return sched_kexit(r);
}

/* ------------------------------------------------------------------ PIC */
static u16 pic_mask = 0xFFFB; /* tudo mascarado exceto a cascata (IRQ2) */

void pic_init(void) {
    outb(0x20, 0x11); io_wait(); outb(0xA0, 0x11); io_wait();
    outb(0x21, 32);   io_wait(); outb(0xA1, 40);   io_wait();
    outb(0x21, 4);    io_wait(); outb(0xA1, 2);    io_wait();
    outb(0x21, 1);    io_wait(); outb(0xA1, 1);    io_wait();
    outb(0x21, pic_mask & 0xFF);
    outb(0xA1, pic_mask >> 8);
}

void pic_unmask(int irq) {
    pic_mask &= ~(1 << irq);
    outb(0x21, pic_mask & 0xFF);
    outb(0xA1, pic_mask >> 8);
}

/* ------------------------------------------------------------------ PIT */
static volatile u64 ticks_ms;
static u32 pit_hz = 1000;

static void pit_irq(int_frame_t *f) { (void)f; ticks_ms += 1000 / pit_hz; }

void pit_init(u32 hz) {
    pit_hz = hz;
    u32 div = 1193182 / hz;
    outb(0x43, 0x36);
    outb(0x40, div & 0xFF);
    outb(0x40, div >> 8);
    irq_install(0, pit_irq);
}

u64 timer_ticks(void) { return ticks_ms; }

void sleep_ms(u64 ms) {
    u64 end = ticks_ms + ms;
    while (ticks_ms < end) hlt();
}

u64 rdtsc(void) {
    u32 lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((u64)hi << 32) | lo;
}

static void cpuid(u32 leaf, u32 *a, u32 *b, u32 *c, u32 *d) {
    __asm__ volatile("cpuid" : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d) : "a"(leaf), "c"(0));
}

void cpu_info(char *vendor, char *brand) {
    u32 r[4];
    cpuid(0, &r[0], &r[1], &r[2], &r[3]);
    memcpy(vendor, &r[1], 4); memcpy(vendor + 4, &r[3], 4); memcpy(vendor + 8, &r[2], 4);
    vendor[12] = 0;
    cpuid(0x80000000, &r[0], &r[1], &r[2], &r[3]);
    if (r[0] >= 0x80000004) {
        for (u32 i = 0; i < 3; i++) {
            cpuid(0x80000002 + i, &r[0], &r[1], &r[2], &r[3]);
            memcpy(brand + i * 16, r, 16);
        }
        brand[48] = 0;
        char *s = brand;
        while (*s == ' ') s++;
        memmove(brand, s, strlen(s) + 1);
    } else {
        strcpy(brand, "Processador x86_64");
    }
}
