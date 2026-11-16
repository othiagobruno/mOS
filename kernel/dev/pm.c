/* mOS — gerência de energia: repouso por S3 (suspend to RAM) e s2idle
 *
 * acpi_sleep() escolhe o estado:
 *  - S3 quando o firmware tem \_S3, a FACS existe e todo componente que perde estado em S3 sabe voltar:
 *    a GPU (sem driver com retomada a tela não volta), os núcleos extras (o escalonador precisa tirá-los
 *    de linha: pm_cpu_hotplug) e nenhum pendrive montado (o xHCI pode reenumerar);
 *  - senão s2idle: dispositivos quietos, núcleos em hlt no P-state mais baixo, acorda por teclado, mouse,
 *    botão de energia ou tecla de repouso.
 *
 * S3, na ordem (ACPI 7.4 e 16.1): suspend dos drivers (ordem inversa de registro) → _TTS(3) → _PTS(3) →
 * _SI._SST(3) → salva PCI (todas as funções), MTRR, MSRs, ELCR, máscara do 8259, HPET → vetor de despertar
 * na FACS (trampolim de modo real em página baixa, o mesmo código dos APs) → GPEs de despertar (_PRW) e
 * PWRBTN_EN → contexto da CPU (__builtin_setjmp) → wbinvd → SLP_TYP|SLP_EN. Na volta: o trampolim leva o
 * BSP ao modo longo com a PML4 do kernel, s3_wake() refaz GDT/TSS/IDT/CR0/CR4/EFER/XCR0/MSRs/MTRR e volta
 * pelo longjmp; então 8259, PIT, ELCR, LAPIC, HPET, PCI, IOMMU, _WAK(3), GPEs e drivers (ordem de registro).
 */
#include "kernel.h"
#include "aml.h"
#include "iommu.h"
#include "pm.h"

#define MAX_OPS 24
static const pm_ops_t *g_ops[MAX_OPS];
static int g_nops;
static const pm_cpu_hotplug_t *g_cpu;
static u32 g_s3_count, g_idle_count;
static volatile bool g_wake_req;

void pm_register(const pm_ops_t *o) { if (o && g_nops < MAX_OPS) g_ops[g_nops++] = o; }
void pm_register_cpu_hotplug(const pm_cpu_hotplug_t *h) { g_cpu = h; }
void pm_wake_request(void) { g_wake_req = true; }

static inline u64 rdmsr(u32 m) { u32 lo, hi; __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(m)); return (u64)hi << 32 | lo; }
static inline void wrmsr(u32 m, u64 v) { __asm__ volatile("wrmsr" :: "c"(m), "a"((u32)v), "d"((u32)(v >> 32))); }

/* ------------------------------------------------------------------ o que impede o S3 */
static const char *s3_blocker(void) {
    u8 a, b;
    if (!acpi_sleep_type(3, &a, &b)) return "o firmware não tem \\_S3";
    if (!acpi_facs()) return "sem FACS";
    if (smp_cpu_count() > 1 && !g_cpu) return "o escalonador ainda não tira núcleos de linha";
    bool display_ok = false, has_display = false, net_ok = false, has_net = false;
    for (int i = 0; i < pci_count(); i++) {
        pci_dev_t *d = pci_get(i);
        if (d->class_ == 0x03) has_display = true;
        if (d->class_ == 0x02) has_net = true;
    }
    for (int i = 0; i < g_nops; i++) {
        if (g_ops[i]->flags & PM_RESTORES_DISPLAY) display_ok = true;
        if (g_ops[i]->flags & PM_RESTORES_NET) net_ok = true;
    }
    if (has_display && !display_ok) return "o driver de vídeo não sabe voltar do S3";
    if (has_net && !net_ok) return "os drivers de rede não sabem voltar do S3";
    for (int i = 0; i < g_nops; i++) {
        if (!g_ops[i]->can_s3) continue;
        const char *why = g_ops[i]->can_s3();
        if (why) return why;
    }
    return NULL;
}

const char *pm_s3_blocker(void) { return s3_blocker(); }

/* ------------------------------------------------------------------ PCI */
#define MAX_PCI 64
static struct { u32 cfg[16]; u8 msi, msix; u32 msi_r[4]; u16 msix_ctl; u32 ext[48]; bool ext_ok; } g_pci[MAX_PCI];

static u8 find_cap(pci_dev_t *d, u8 id) {
    if (!(pci_read(d, 0x04) >> 16 & 0x10)) return 0;
    u8 p = (u8)(pci_read(d, 0x34) & 0xFC);
    for (int g = 0; p && g < 48; g++) {
        u32 v = pci_read(d, p);
        if ((v & 0xFF) == id) return p;
        p = (u8)((v >> 8) & 0xFC);
    }
    return 0;
}

static void pci_save(void) {
    int n = pci_count() < MAX_PCI ? pci_count() : MAX_PCI;
    for (int i = 0; i < n; i++) {
        pci_dev_t *d = pci_get(i);
        for (int k = 0; k < 16; k++) g_pci[i].cfg[k] = pci_read(d, (u8)(k * 4));
        g_pci[i].msi = find_cap(d, 0x05);
        g_pci[i].msix = find_cap(d, 0x11);
        if (g_pci[i].msi) for (int k = 0; k < 4; k++) g_pci[i].msi_r[k] = pci_read(d, (u8)(g_pci[i].msi + 4 * k));
        if (g_pci[i].msix) g_pci[i].msix_ctl = (u16)(pci_read(d, g_pci[i].msix) >> 16);
        /* o IOMMU guarda a base do MMIO na própria capacidade (0x40..): salva o resto do espaço também */
        g_pci[i].ext_ok = d->vendor == 0x1022 && d->class_ == 0x08 && d->subclass == 0x06;
        if (g_pci[i].ext_ok) for (int k = 0; k < 48; k++) g_pci[i].ext[k] = pci_read(d, (u8)(0x40 + k * 4));
    }
}

static void pci_restore(void) {
    int n = pci_count() < MAX_PCI ? pci_count() : MAX_PCI;
    /* pontes primeiro: números de barramento e janelas, senão os dispositivos de baixo somem */
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < n; i++) {
            pci_dev_t *d = pci_get(i);
            u32 *c = g_pci[i].cfg;
            bool bridge = ((c[3] >> 16) & 0x7F) == 1;
            if (bridge != (pass == 0)) continue;
            pci_write(d, 0x04, c[1] & 0xFFFF0000u);                   /* desliga decodificação enquanto mexe */
            if (bridge) { for (int k = 4; k <= 12; k++) pci_write(d, (u8)(k * 4), c[k]); pci_write(d, 0x3C, c[15]); }
            else { for (int k = 4; k <= 9; k++) pci_write(d, (u8)(k * 4), c[k]); pci_write(d, 0x30, c[12]); pci_write(d, 0x3C, c[15]); }
            pci_write(d, 0x0C, c[3] & 0x0000FFFFu);                    /* cache line e latência */
            if (g_pci[i].ext_ok) for (int k = 0; k < 48; k++) pci_write(d, (u8)(0x40 + k * 4), g_pci[i].ext[k]);
            if (g_pci[i].msi) {
                u8 m = g_pci[i].msi;
                pci_write(d, (u8)(m + 4), g_pci[i].msi_r[1]);
                pci_write(d, (u8)(m + 8), g_pci[i].msi_r[2]);
                pci_write(d, (u8)(m + 12), g_pci[i].msi_r[3]);
                pci_write(d, m, g_pci[i].msi_r[0]);                    /* controle (Enable) por último */
            }
            if (g_pci[i].msix) {
                u32 v = pci_read(d, g_pci[i].msix);
                pci_write(d, g_pci[i].msix, (v & 0xFFFF) | (u32)g_pci[i].msix_ctl << 16);
            }
            pci_write(d, 0x04, c[1] & 0xFFFF);                          /* comando: memória, E/S, bus master */
        }
    }
}

/* ------------------------------------------------------------------ CPU do BSP */
static const u32 g_msrs[] = { 0xC0000081, 0xC0000082, 0xC0000083, 0xC0000084, 0xC0000100, 0xC0000101, 0xC0000102,
                               0x277, 0xC0010062 };
static struct {
    u64 cr0, cr3, cr4, efer, xcr0, apic_base;
    u64 msr[sizeof g_msrs / sizeof g_msrs[0]];
    u64 mtrr_def, mtrr_var[16], mtrr_fix[11]; u32 nvar;
    struct PACKED { u16 l; u64 b; } gdt, idt;
    u16 tr;
    u8 elcr0, elcr1, pic1, pic2;
    u64 hpet_cnt, hpet_cfg; volatile u8 *hpet;
    void *jmp[5];
} C;
static const u32 g_fix[] = { 0x250, 0x258, 0x259, 0x268, 0x269, 0x26A, 0x26B, 0x26C, 0x26D, 0x26E, 0x26F };

static void cpu_save(void) {
    __asm__ volatile("mov %%cr0, %0" : "=r"(C.cr0)); __asm__ volatile("mov %%cr3, %0" : "=r"(C.cr3));
    __asm__ volatile("mov %%cr4, %0" : "=r"(C.cr4));
    __asm__ volatile("sgdt %0" : "=m"(C.gdt)); __asm__ volatile("sidt %0" : "=m"(C.idt));
    __asm__ volatile("str %0" : "=r"(C.tr));
    C.efer = rdmsr(0xC0000080);
    C.apic_base = rdmsr(0x1B);
    if (C.cr4 & (1ull << 18)) { u32 lo, hi; __asm__ volatile("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0)); C.xcr0 = (u64)hi << 32 | lo; }
    for (u32 i = 0; i < sizeof g_msrs / sizeof g_msrs[0]; i++) C.msr[i] = rdmsr(g_msrs[i]);
    C.nvar = (u32)(rdmsr(0xFE) & 0xFF); if (C.nvar > 8) C.nvar = 8;
    for (u32 i = 0; i < C.nvar * 2; i++) C.mtrr_var[i] = rdmsr(0x200 + i);
    for (u32 i = 0; i < 11; i++) C.mtrr_fix[i] = rdmsr(g_fix[i]);
    C.mtrr_def = rdmsr(0x2FF);
    C.elcr0 = inb(0x4D0); C.elcr1 = inb(0x4D1);
    C.pic1 = inb(0x21); C.pic2 = inb(0xA1);
    const u8 *h = acpi_find_table("HPET", 0);
    if (h) {
        u64 base; memcpy(&base, h + 44, 8);
        C.hpet = (volatile u8 *)base;
        C.hpet_cfg = mmio_r32(C.hpet, 0x10);
        C.hpet_cnt = mmio_r32(C.hpet, 0xF0) | (u64)mmio_r32(C.hpet, 0xF4) << 32;
    }
}

static void mtrr_restore(void) {
    bool same = rdmsr(0x2FF) == C.mtrr_def;
    for (u32 i = 0; same && i < C.nvar * 2; i++) same = rdmsr(0x200 + i) == C.mtrr_var[i];
    for (u32 i = 0; same && i < 11; i++) same = rdmsr(g_fix[i]) == C.mtrr_fix[i];
    if (same) return;                                             /* o firmware já restaurou (o usual) */
    u64 cr0; __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    __asm__ volatile("mov %0, %%cr0; wbinvd" :: "r"((cr0 | (1ull << 30)) & ~(1ull << 29)) : "memory");   /* CD=1, NW=0 */
    wrmsr(0x2FF, C.mtrr_def & ~(3ull << 10));
    u64 syscfg = rdmsr(0xC0010010);
    wrmsr(0xC0010010, syscfg | (1ull << 19));                    /* MtrrFixDramModEn: grava RdDram/WrDram */
    for (u32 i = 0; i < 11; i++) wrmsr(g_fix[i], C.mtrr_fix[i]);
    wrmsr(0xC0010010, syscfg);
    for (u32 i = 0; i < C.nvar * 2; i++) wrmsr(0x200 + i, C.mtrr_var[i]);
    wrmsr(0x2FF, C.mtrr_def);
    __asm__ volatile("wbinvd; mov %0, %%cr0" :: "r"(cr0) : "memory");
}

/* entrada C do trampolim de despertar: pilha própria, modo longo com a PML4 do kernel */
static u8 g_wake_stack[16384] ALIGNED(16);
static void s3_wake(void) {
    __asm__ volatile("lgdt %0" :: "m"(C.gdt));
    __asm__ volatile(
        "mov $0x10, %%ax; mov %%ax, %%ds; mov %%ax, %%es; mov %%ax, %%ss; mov %%ax, %%fs; mov %%ax, %%gs;"
        "lea 1f(%%rip), %%rax; pushq $0x08; pushq %%rax; lretq; 1:" ::: "rax", "memory");
    /* TSS: o descritor ainda está "ocupado" na GDT; ltr exige livre */
    u8 *desc = (u8 *)C.gdt.b + (C.tr & ~7u);
    desc[5] &= (u8)~0x02;
    __asm__ volatile("ltr %0" :: "r"(C.tr));
    __asm__ volatile("lidt %0" :: "m"(C.idt));
    wrmsr(0xC0000080, C.efer);
    __asm__ volatile("mov %0, %%cr0" :: "r"(C.cr0) : "memory");
    __asm__ volatile("mov %0, %%cr4" :: "r"(C.cr4) : "memory");
    __asm__ volatile("mov %0, %%cr3" :: "r"(C.cr3) : "memory");
    if (C.cr4 & (1ull << 18)) __asm__ volatile("xsetbv" :: "c"(0), "a"((u32)C.xcr0), "d"((u32)(C.xcr0 >> 32)));
    u32 mxcsr = 0x1F80;
    __asm__ volatile("fninit; ldmxcsr %0" :: "m"(mxcsr));
    mtrr_restore();
    for (u32 i = 0; i < sizeof g_msrs / sizeof g_msrs[0]; i++) wrmsr(g_msrs[i], C.msr[i]);
    wrmsr(0x1B, C.apic_base);
    __builtin_longjmp(C.jmp, 1);
}

/* trampolim: o mesmo código de modo real dos APs (arch/smp_tramp.S), noutra página baixa */
extern u8 ap_tramp_start[], ap_tramp_end[], ap_tramp_pm32[], ap_tramp_lm64[];
static u64 wake_page(void) {
    u64 best = 0;
    for (u32 i = 0; i < g_boot->region_count; i++) {
        mem_region_t *r = &g_boot->regions[i];
        if (r->type != MEM_FREE) continue;
        for (u64 a = MAX(r->base, 0x8000ULL); a + 4096 <= r->base + r->pages * 4096 && a < 0x9F000; a += 4096) best = a;
    }
    return best;                                                   /* a mais alta: os APs usam a mais baixa */
}

static bool setup_wake_vector(void) {
    u64 page = wake_page();
    if (!page || C.cr3 >> 32) return false;
    volatile u8 *t = (volatile u8 *)page;
    memset((void *)t, 0, 4096);
    memcpy((void *)t, ap_tramp_start, (usize)(ap_tramp_end - ap_tramp_start));
    static const u64 gdt[4] = { 0, 0x00CF9A000000FFFFull, 0x00CF92000000FFFFull, 0x00AF9A000000FFFFull };
    memcpy((void *)(t + 0xF00), gdt, sizeof gdt);
    *(volatile u16 *)(t + 0xF20) = sizeof gdt - 1;
    *(volatile u32 *)(t + 0xF22) = (u32)page + 0xF00;
    *(volatile u32 *)(t + 0xF30) = (u32)(C.cr3 & ~0xFFFull);
    *(volatile u64 *)(t + 0xF38) = ((u64)g_wake_stack + sizeof g_wake_stack) & ~15ull;
    *(volatile u64 *)(t + 0xF40) = (u64)s3_wake;
    *(volatile u32 *)(t + 0xF48) = (u32)page + (u32)(ap_tramp_pm32 - ap_tramp_start);
    *(volatile u16 *)(t + 0xF4C) = 0x08;
    *(volatile u32 *)(t + 0xF50) = (u32)page + (u32)(ap_tramp_lm64 - ap_tramp_start);
    *(volatile u16 *)(t + 0xF54) = 0x18;
    u32 *facs = acpi_facs();
    facs[3] = (u32)page;                                           /* Firmware_Waking_Vector: CS = page >> 4, IP = 0 */
    u32 flen = facs[1];
    if (flen >= 32) { facs[6] = 0; facs[7] = 0; }                  /* X_Firmware_Waking_Vector = 0: usa o de 32 bits */
    return true;
}

static void platform_resume(void) {
    pic_init();
    outb(0x21, C.pic1); outb(0xA1, C.pic2);
    outb(0x4D0, C.elcr0); outb(0x4D1, C.elcr1);
    pit_init(1000);
    u64 lb = smp_lapic_base();
    if (lb) mmio_w32((volatile void *)lb, 0xF0, 0x1FF);            /* LAPIC ligado, espúria 0xFF */
    if (C.hpet) {                                                  /* o contador recomeça de onde parou: hpet_ns() não volta */
        mmio_w32(C.hpet, 0x10, (u32)(C.hpet_cfg & ~1ull));
        mmio_w32(C.hpet, 0xF0, (u32)C.hpet_cnt); mmio_w32(C.hpet, 0xF4, (u32)(C.hpet_cnt >> 32));
        mmio_w32(C.hpet, 0x10, (u32)C.hpet_cfg);
    }
    pci_restore();
    iommu_resume();
}

/* ------------------------------------------------------------------ S3 */
static bool enter_s3(void) {
    for (int i = g_nops - 1; i >= 0; i--)
        if (g_ops[i]->suspend && g_ops[i]->suspend(3) != 0) {
            kprintf("pm: %s recusou o S3\n", g_ops[i]->name);
            for (int k = i + 1; k < g_nops; k++) if (g_ops[k]->resume) g_ops[k]->resume(3);
            return false;
        }
    if (g_cpu && smp_cpu_count() > 1 && !g_cpu->offline()) {
        kprintf("pm: núcleos não saíram de linha\n");
        for (int k = 0; k < g_nops; k++) if (g_ops[k]->resume) g_ops[k]->resume(3);
        return false;
    }
    acpi_prepare_sleep(3);                                         /* _TTS, _PTS, _SST, GPEs de despertar */
    u64 fl = irq_save();
    cpu_save();
    pci_save();
    if (!setup_wake_vector()) {
        irq_restore(fl);
        acpi_finish_wake(3);
        kprintf("pm: sem página baixa para o vetor de despertar\n");
        for (int k = 0; k < g_nops; k++) if (g_ops[k]->resume) g_ops[k]->resume(3);
        return false;
    }
    kprintf("pm: entrando em S3\n");
    if (__builtin_setjmp(C.jmp) == 0) {
        cli();
        acpi_enter_sleep_regs(3);
        for (int i = 0; i < 4000000; i++) __asm__ volatile("pause");
        /* o hardware não dormiu: segue como se tivesse acordado, refazendo só o que a escrita mexeu */
        kprintf("pm: o chipset não entrou em S3\n");
    }
    platform_resume();
    acpi_finish_wake(3);
    irq_restore(fl);
    if (g_cpu && smp_cpu_count() > 1) g_cpu->online();
    for (int k = 0; k < g_nops; k++) if (g_ops[k]->resume) g_ops[k]->resume(3);
    g_s3_count++;
    kprintf("pm: de volta do S3 (%u)\n", g_s3_count);
    return true;
}

/* ------------------------------------------------------------------ s2idle */
static void enter_s2idle(void) {
    for (int i = g_nops - 1; i >= 0; i--) if (g_ops[i]->suspend) g_ops[i]->suspend(0);
    acpi_prepare_sleep(0);
    cpufreq_force_lowest(true);
    kprintf("pm: repouso (s2idle)\n");
    g_wake_req = false;
    /* entrada limpa: o que já estava na fila não acorda */
    input_event_t e;
    while (input_pop(&e)) {}
    u64 last = timer_ticks();
    for (;;) {
        hlt();                                                     /* acorda no PIT (1 ms), SCI, PS/2 */
        u64 now = timer_ticks();
        if (now - last >= 20) {                                    /* teclado e mouse USB são consultados, não interrompem */
            last = now;
            usb_poll();
            acpi_poll();
        }
        if (g_wake_req) break;
        if (input_pop(&e)) break;                                  /* tecla, clique ou movimento */
    }
    cpufreq_force_lowest(false);
    acpi_finish_wake(0);
    for (int k = 0; k < g_nops; k++) if (g_ops[k]->resume) g_ops[k]->resume(0);
    g_idle_count++;
    kprintf("pm: acordou do repouso (%u)\n", g_idle_count);
}

void acpi_sleep(void) {
    static bool busy;
    if (busy) return;
    busy = true;
    extern void persist_sync(void);
    persist_sync();                                                /* nada pendente no disco antes de dormir */
    const char *why = s3_blocker();
    if (why) kprintf("pm: S3 indisponível (%s); usando s2idle\n", why);
    if (why || !enter_s3()) enter_s2idle();
    busy = false;
}

const char *pm_status(void) {
    static char b[160];
    const char *why = s3_blocker();
    snprintf(b, sizeof b, "Repouso: %s%s%s; %u S3, %u s2idle; %d drivers", why ? "s2idle (" : "S3", why ? why : "", why ? ")" : "",
             g_s3_count, g_idle_count, g_nops);
    return b;
}
