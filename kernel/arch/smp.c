/*
 * mOS — LAPIC, HPET e partida dos processadores de aplicação (SMP).
 *
 * Os núcleos são descobertos pela tabela ACPI MADT e acordados com INIT-SIPI-SIPI. Cada AP passa
 * pelo trampolim (arch/smp_tramp.S), carrega a GDT/IDT do kernel, assume o modo do BSP (CR0/CR4/EFER/
 * XCR0), ganha um TSS próprio, liga o LAPIC com o temporizador em 1 ms e vira a thread ociosa daquele
 * núcleo: roda threads de usuário escolhidas pelo escalonador (proc/sched.c, trava grande do kernel)
 * e, quando não há nenhuma, atende os trabalhos de par_for (composição, desfoque, vidro, sombras, cópias
 * para o framebuffer). A UI, a rede, o USB, o áudio e as threads do kernel ficam no BSP. Também ficam
 * aqui o LAPIC (EOI, IPIs, temporizador calibrado) e o HPET.
 */
#include "proc.h"

#define LAPIC_ID    0x020
#define LAPIC_VER   0x030
#define LAPIC_EOI   0x0B0
#define LAPIC_SVR   0x0F0
#define LAPIC_ICRLO 0x300
#define LAPIC_ICRHI 0x310
#define LAPIC_LVT_T 0x320
#define LAPIC_TICR  0x380
#define LAPIC_TCCR  0x390
#define LAPIC_TDCR  0x3E0

typedef struct PACKED { char sig[4]; u32 len; u8 rev, sum; char oem[6], table[8]; u32 oemrev, creator, crev; } sdt_t;

static volatile u8 *lapic;
static u8 apic_ids[MAX_CPUS];
static int ncpus_found, ncpus_online = 1;
static u32 bsp_id;
static u64 lapic_hz;
static volatile u8 *hpet;
static u64 hpet_fs;          /* femtossegundos por tique do HPET */

static inline u32 le32_(const u8 *b) { return (u32)b[0] | (u32)b[1] << 8 | (u32)b[2] << 16 | (u32)b[3] << 24; }
static inline u64 le64_(const u8 *b) { return (u64)le32_(b) | (u64)le32_(b + 4) << 32; }
static inline u32 lr(u32 off) { return mmio_r32(lapic, off); }
static inline void lw(u32 off, u32 v) { mmio_w32(lapic, off, v); }

void lapic_eoi(void) { if (lapic) lw(LAPIC_EOI, 0); }
int smp_cpu_count(void) { return ncpus_online; }
u64 lapic_timer_hz(void) { return lapic_hz; }

/* ------------------------------------------------------------ ACPI */
static sdt_t *find_table(const char *sig) {
    const u8 *rsdp = g_boot->rsdp;
    if (!rsdp || memcmp(rsdp, "RSD PTR ", 8)) return NULL;
    bool x = rsdp[15] >= 2 && le64_(rsdp + 24);
    sdt_t *root = (sdt_t *)(x ? le64_(rsdp + 24) : (u64)le32_(rsdp + 16));
    if (!root) return NULL;
    u32 n = (root->len - sizeof(sdt_t)) / (x ? 8 : 4);
    const u8 *ents = (const u8 *)(root + 1);
    for (u32 i = 0; i < n; i++) {
        sdt_t *t = (sdt_t *)(x ? le64_(ents + i * 8) : (u64)le32_(ents + i * 4));
        if (t && !memcmp(t->sig, sig, 4)) return t;
    }
    return NULL;
}

/* ------------------------------------------------------------ HPET */
u64 hpet_ns(void) {
    if (!hpet) return 0;
    u64 c = mmio_r32(hpet, 0xF0) | (u64)mmio_r32(hpet, 0xF4) << 32;
    /* ns = tiques * fs / 10^6, dividido em duas partes para não estourar 64 bits */
    return c / 1000000 * hpet_fs + (c % 1000000) * hpet_fs / 1000000;
}

static void hpet_init(void) {
    sdt_t *t = find_table("HPET");
    if (!t) { kprintf("hpet: ausente\n"); return; }
    u64 base = le64_((const u8 *)t + 44);
    if (!base) return;
    vmm_map_mmio(base, 0x400);
    hpet = (volatile u8 *)base;
    u64 cap = mmio_r32(hpet, 0) | (u64)mmio_r32(hpet, 4) << 32;
    hpet_fs = cap >> 32;
    if (!hpet_fs || hpet_fs > 100000000) { hpet = NULL; return; }
    mmio_w32(hpet, 0x10, mmio_r32(hpet, 0x10) | 1);           /* ENABLE_CNF: contador rodando */
    kprintf("hpet: %llu MHz, %u temporizadores\n", 1000000000000000ULL / hpet_fs / 1000000, (u32)((cap >> 8) & 0x1F) + 1);
}

/* ------------------------------------------------------------ LAPIC */
static void lapic_enable(void) {
    lw(LAPIC_SVR, 0x100 | 0xFF);                               /* habilita; espúria = 0xFF */
    lw(LAPIC_LVT_T, 0x10000);                                  /* temporizador mascarado */
}

/* mede a frequência do temporizador do LAPIC contra o PIT (timer_ticks em ms) */
static void lapic_calibrate(void) {
    lw(LAPIC_TDCR, 0x3);                                       /* divide por 16 */
    u64 t = timer_ticks();
    while (timer_ticks() == t) __asm__ volatile("pause");
    lw(LAPIC_TICR, 0xFFFFFFFF);
    t = timer_ticks();
    while (timer_ticks() - t < 50) __asm__ volatile("pause");
    u32 used = 0xFFFFFFFF - lr(LAPIC_TCCR);
    lw(LAPIC_TICR, 0);
    lapic_hz = (u64)used * 16 * 20;
}

static void ipi(u32 apic, u32 lo) {
    lw(LAPIC_ICRHI, apic << 24);
    lw(LAPIC_ICRLO, lo);
    for (int i = 0; i < 100000 && (lr(LAPIC_ICRLO) & (1u << 12)); i++) __asm__ volatile("pause");
}

static void udelay(u32 us) {
    if (hpet) { u64 end = hpet_ns() + (u64)us * 1000; while (hpet_ns() < end) __asm__ volatile("pause"); return; }
    u64 end = timer_ticks() + (us + 999) / 1000 + 1;
    while (timer_ticks() < end) __asm__ volatile("pause");
}

/* ------------------------------------------------------------ trabalho paralelo (par_for)
 *
 * Os APs são operários: dormem em `sti; hlt` e acordam com um IPI no vetor VEC_WAKE (stub mínimo em
 * arch/x86.S que só faz o EOI, sem passar pelo escalonador). Depois de um trabalho ficam girando um
 * pouco antes de dormir, porque o compositor dispara vários par_for seguidos por quadro.
 *
 * Protocolo (Dekker com seq_cst): o BSP publica cur_job e só então olha ap_sleeping; o AP soma
 * ap_sleeping e só então relê cur_job. O BSP termina zerando cur_job e esperando ap_busy == 0; o
 * AP soma ap_busy antes de ler cur_job, então nunca toca num trabalho (na pilha do BSP) já encerrado. */
#define VEC_WAKE 0xF1
#define AP_SPIN  20000

typedef struct { par_fn_t fn; void *ctx; int n, grain; int next; u32 id; } job_t;
static job_t *volatile cur_job;
static volatile u32 job_posted;
static volatile int ap_busy, ap_sleeping, ap_ready;
static volatile u32 par_lock;
static u32 job_seq;
u64 g_lapic_eoi;                                               /* endereço do registrador EOI (x86.S) */

static bool par_off;                                          /* par_set_enabled(false): tudo no BSP (medições) */
int par_cpus(void) { return ap_ready && !par_off ? ncpus_online : 1; }   /* limita os índices `cpu` entregues */
void par_set_enabled(bool on) { par_off = !on; }

static void run_job(job_t *j, int cpu) {
    for (;;) {
        int b = __atomic_fetch_add(&j->next, j->grain, __ATOMIC_RELAXED);
        if (b >= j->n) break;
        j->fn(j->ctx, b, MIN(b + j->grain, j->n), cpu);
    }
}

static inline bool job_pending(u32 seen) { return __atomic_load_n(&cur_job, __ATOMIC_SEQ_CST) && job_posted != seen; }

NORETURN static void ap_loop(int cpu) {
    u32 seen = 0;
    for (;;) {
        for (int spin = 0; spin < AP_SPIN; spin++) {
            if (!job_pending(seen)) { __asm__ volatile("pause"); continue; }
            __atomic_add_fetch(&ap_busy, 1, __ATOMIC_SEQ_CST);
            job_t *j = __atomic_load_n(&cur_job, __ATOMIC_SEQ_CST);
            if (j && j->id != seen) { seen = j->id; run_job(j, cpu); spin = 0; }
            __atomic_sub_fetch(&ap_busy, 1, __ATOMIC_SEQ_CST);
        }
        cli();
        __atomic_add_fetch(&ap_sleeping, 1, __ATOMIC_SEQ_CST);
        if (!job_pending(seen)) __asm__ volatile("sti; hlt; cli" ::: "memory");   /* o sti cobre o hlt: o IPI não se perde */
        __atomic_sub_fetch(&ap_sleeping, 1, __ATOMIC_SEQ_CST);
    }
}

static void wake_aps(void) {
    u64 f = irq_save();
    for (int i = 0; i < 100000 && (lr(LAPIC_ICRLO) & (1u << 12)); i++) __asm__ volatile("pause");
    lw(LAPIC_ICRHI, 0);
    lw(LAPIC_ICRLO, 0xC4000 | VEC_WAKE);                       /* todos menos eu, fixo, asserção */
    irq_restore(f);
}

void par_for(int n, int grain, par_fn_t fn, void *ctx) {
    if (n <= 0) return;
    if (grain < 1) grain = 1;
    if (!ap_ready || par_off || n <= grain || __atomic_exchange_n(&par_lock, 1, __ATOMIC_ACQUIRE)) { fn(ctx, 0, n, 0); return; }
    job_t j = { fn, ctx, n, grain, 0, ++job_seq ? job_seq : ++job_seq };
    job_posted = j.id;
    __atomic_store_n(&cur_job, &j, __ATOMIC_SEQ_CST);
    if (__atomic_load_n(&ap_sleeping, __ATOMIC_SEQ_CST)) wake_aps();
    run_job(&j, 0);
    __atomic_store_n(&cur_job, (job_t *)NULL, __ATOMIC_SEQ_CST);
    while (__atomic_load_n(&ap_busy, __ATOMIC_SEQ_CST)) __asm__ volatile("pause");
    __atomic_store_n(&par_lock, 0, __ATOMIC_RELEASE);
}

/* ------------------------------------------------------------ APs */
extern u8 ap_tramp_start[], ap_tramp_end[], ap_tramp_pm32[], ap_tramp_lm64[];
typedef struct PACKED { u16 limit; u64 base; } dtr_t;
static dtr_t kgdt, kidt;
static volatile u8 *tramp;
static u64 bsp_cr0, bsp_cr4, bsp_efer, bsp_xcr0;
static u8 cpu_apic[MAX_CPUS];

u8 smp_apic_id(int cpu) { return cpu >= 0 && cpu < MAX_CPUS ? cpu_apic[cpu] : 0; }

/* IPI fixo para um núcleo (proc/sched.c: TLB shootdown e threads mortas) */
void smp_send_ipi(int cpu, u8 vec) {
    if (!lapic || cpu < 0 || cpu >= ncpus_online) return;
    /* a PTE (W^X do JIT, COW) tem que estar visível nos outros núcleos antes do IPI de TLB */
    __asm__ volatile("mfence" ::: "memory");
    u64 f = irq_save();
    for (int i = 0; i < 100000 && (lr(LAPIC_ICRLO) & (1u << 12)); i++) __asm__ volatile("pause");
    lw(LAPIC_ICRHI, (u32)cpu_apic[cpu] << 24);
    lw(LAPIC_ICRLO, 0x4000 | vec);                             /* fixo, asserção, destino físico */
    irq_restore(f);
}

static inline u64 rdmsr_(u32 m) { u32 lo, hi; __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(m)); return (u64)hi << 32 | lo; }
static inline void wrmsr_(u32 m, u64 v) { __asm__ volatile("wrmsr" :: "c"(m), "a"((u32)v), "d"((u32)(v >> 32))); }

extern void sched_ap_init(int cpu);

static void ap_entry(void) {
    /* GDT e IDT do kernel */
    __asm__ volatile("lgdt %0; lidt %1" :: "m"(kgdt), "m"(kidt));
    __asm__ volatile(
        "mov $0x10, %%ax; mov %%ax, %%ds; mov %%ax, %%es; mov %%ax, %%ss; mov %%ax, %%fs; mov %%ax, %%gs;"
        "lea 1f(%%rip), %%rax; pushq $0x08; pushq %%rax; lretq; 1:" ::: "rax", "memory");
    /* o mesmo modo de operação do BSP: o AP roda código de usuário (NX nas PTEs, SSE/XSAVE, FSGSBASE, WP) */
    __asm__ volatile("mov %0, %%cr0" :: "r"(bsp_cr0) : "memory");
    __asm__ volatile("mov %0, %%cr4" :: "r"(bsp_cr4) : "memory");
    wrmsr_(0xC0000080, rdmsr_(0xC0000080) | (bsp_efer & ((1ULL << 11) | 1ULL)));   /* EFER.NXE, EFER.SCE */
    if (bsp_cr4 & (1ULL << 18)) __asm__ volatile("xsetbv" :: "c"(0), "a"((u32)bsp_xcr0), "d"((u32)(bsp_xcr0 >> 32)));
    /* PAT igual ao do BSP (framebuffer em WC) */
    u64 pat = 0x0007010600070106ULL;
    wrmsr_(0x277, pat);
    /* FPU/SSE limpos: os trabalhos usam ponto flutuante e SSE2 */
    u32 mxcsr = 0x1F80;
    __asm__ volatile("fninit; ldmxcsr %0" :: "m"(mxcsr));
    lapic_enable();
    int cpu = __atomic_add_fetch(&ncpus_online, 1, __ATOMIC_SEQ_CST) - 1;
    if (cpu >= MAX_CPUS) for (;;) __asm__ volatile("cli; hlt");
    cpu_apic[cpu] = (u8)(lr(LAPIC_ID) >> 24);
    /* TSS próprio: rsp0 das threads de usuário que este núcleo roda e pilha IST para NMI/falta dupla */
    u8 *ist = kmalloc(16384);
    cpu_tss_init(cpu, ist ? (((u64)ist + 16384) & ~15ULL) : 0);
    wrmsr_(0xC0000101, 0);                                     /* GS de usuário: a troca de contexto define */
    sched_ap_init(cpu);
    /* fatia do escalonador neste núcleo: temporizador do LAPIC periódico, 1 ms, vetor 0xF0 */
    if (lapic_hz) {
        lw(LAPIC_TDCR, 0x3);                                   /* divide por 16 */
        lw(LAPIC_LVT_T, 0x20000 | 0xF0);
        lw(LAPIC_TICR, (u32)MAX(lapic_hz / 16 / 1000, 1));
    }
    __atomic_add_fetch(&ap_ready, 1, __ATOMIC_SEQ_CST);
    ap_loop(cpu);
}

/* O trampolim precisa de uma página abaixo de 1 MiB (o SIPI só aponta para lá). O PMM nunca
 * entrega o 1º MiB, então qualquer página livre ali, segundo o mapa do UEFI, é nossa. */
static u64 low_page(void) {
    for (u32 i = 0; i < g_boot->region_count; i++) {
        mem_region_t *r = &g_boot->regions[i];
        if (r->type != MEM_FREE) continue;
        for (u64 a = MAX(r->base, 0x8000ULL); a + 4096 <= r->base + r->pages * 4096 && a < 0x9F000; a += 4096)
            return a;
    }
    return 0;
}

void smp_init(void) {
    hpet_init();
    sdt_t *madt = find_table("APIC");
    if (!madt) { kprintf("smp: sem tabela MADT, um núcleo\n"); return; }
    u64 lbase = le32_((const u8 *)madt + 36);
    const u8 *p = (const u8 *)madt + 44, *end = (const u8 *)madt + madt->len;
    for (; p + 2 <= end && p[1] >= 2; p += p[1]) {
        if (p[0] == 0 && (le32_(p + 4) & 3) && ncpus_found < MAX_CPUS) apic_ids[ncpus_found++] = p[3];
        if (p[0] == 5 && p[1] >= 12) lbase = le64_(p + 4);
    }
    if (!lbase) return;
    vmm_map_mmio(lbase, 0x1000);
    lapic = (volatile u8 *)lbase;
    g_lapic_eoi = lbase + LAPIC_EOI;
    bsp_id = lr(LAPIC_ID) >> 24;
    cpu_apic[0] = (u8)bsp_id;
    lapic_enable();
    lapic_calibrate();
    kprintf("smp: LAPIC v%u em 0x%llx, BSP %u, temporizador %llu MHz, %d núcleo(s) na MADT\n",
            lr(LAPIC_VER) & 0xFF, lbase, bsp_id, lapic_hz / 1000000, ncpus_found);
    if (ncpus_found < 2) return;

    u64 cr3 = read_cr3();
    u64 page = low_page();
    if (!page || cr3 >> 32) { kprintf("smp: sem página baixa para o trampolim, só o BSP\n"); return; }
    __asm__ volatile("sgdt %0; sidt %1" : "=m"(kgdt), "=m"(kidt));
    __asm__ volatile("mov %%cr0, %0; mov %%cr4, %1" : "=r"(bsp_cr0), "=r"(bsp_cr4));
    {   /* FSGSBASE: o sched_init do BSP só liga depois (proc_init); os APs já nascem com ela */
        u32 a = 7, b = 0, c = 0, d = 0;
        __asm__ volatile("cpuid" : "+a"(a), "=b"(b), "+c"(c), "=d"(d));
        if (b & 1) bsp_cr4 |= 1ULL << 16;
        bsp_cr4 &= ~(1ULL << 12);                             /* LA57 não muda em modo longo: fica o do trampolim */
    }
    bsp_efer = rdmsr_(0xC0000080);
    if (bsp_cr4 & (1ULL << 18)) { u32 lo, hi; __asm__ volatile("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0)); bsp_xcr0 = (u64)hi << 32 | lo; }
    tramp = (volatile u8 *)page;
    memset((void *)tramp, 0, 4096);
    memcpy((void *)tramp, ap_tramp_start, ap_tramp_end - ap_tramp_start);
    static const u64 gdt[4] = {0, 0x00CF9A000000FFFFULL, 0x00CF92000000FFFFULL, 0x00AF9A000000FFFFULL};
    memcpy((void *)(tramp + 0xF00), gdt, sizeof gdt);
    *(volatile u16 *)(tramp + 0xF20) = sizeof gdt - 1;
    *(volatile u32 *)(tramp + 0xF22) = (u32)page + 0xF00;
    *(volatile u32 *)(tramp + 0xF30) = (u32)cr3;
    *(volatile u64 *)(tramp + 0xF40) = (u64)ap_entry;
    *(volatile u32 *)(tramp + 0xF48) = (u32)page + (u32)(ap_tramp_pm32 - ap_tramp_start);
    *(volatile u16 *)(tramp + 0xF4C) = 0x08;
    *(volatile u32 *)(tramp + 0xF50) = (u32)page + (u32)(ap_tramp_lm64 - ap_tramp_start);
    *(volatile u16 *)(tramp + 0xF54) = 0x18;

    u64 t0 = timer_ticks();
    for (int i = 0; i < ncpus_found; i++) {
        if (apic_ids[i] == bsp_id) continue;
        u8 *stack = kmalloc(65536);
        if (!stack) break;
        *(volatile u64 *)(tramp + 0xF38) = ((u64)stack + 65536) & ~15ULL;
        u32 before = *(volatile u32 *)(tramp + 0xF58);
        int online0 = ncpus_online;
        kprintf("smp: acordando o núcleo APIC %u (trampolim em 0x%llx)\n", apic_ids[i], page);
        ipi(apic_ids[i], 0x4500);                              /* INIT */
        udelay(10000);
        for (int k = 0; k < 2 && *(volatile u32 *)(tramp + 0xF58) == before; k++) {
            ipi(apic_ids[i], 0x4600 | (u32)(page >> 12));      /* STARTUP */
            udelay(200);
        }
        u64 until = timer_ticks() + 200;
        while (ncpus_online == online0 && timer_ticks() < until) __asm__ volatile("pause");
        if (ncpus_online == online0)
            kprintf("smp: o núcleo APIC %u não respondeu (parou no estágio %u do trampolim)\n", apic_ids[i], tramp[0xF5C]);
    }
    kprintf("smp: %d de %d núcleos ativos em %llu ms (%d operário(s) para par_for)\n", ncpus_online, ncpus_found,
            timer_ticks() - t0, ap_ready);
}
