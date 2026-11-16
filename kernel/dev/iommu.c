/* mOS — IOMMU AMD-Vi (Raven 1022:15d1, IVRS do firmware)
 *
 * O que faz: lê a IVRS (IVHD 0x10/0x11/0x40 e entradas de dispositivo, inclusive faixas e apelidos),
 * monta uma tabela de páginas identidade que cobre só a memória do mapa UEFI (RAM, ACPI, reservada:
 * nunca MMIO), aponta o DTE de cada requisitante listado para ela (domínio 1, leitura e escrita, Mode 3)
 * e liga o IOMMU com buffer de comandos e log de eventos. Um DMA de dispositivo para fora da memória
 * (BAR de outro dispositivo, endereço lixo) é bloqueado e aparece no log como IO_PAGE_FAULT.
 * Interrupções passam sem remapear (IV = 0): o mOS usa MSI direto ao LAPIC e o 8259.
 *
 * Os endereços de DMA do kernel são físicos (mapa de identidade), então IOVA = físico e nenhum driver
 * precisa mudar. Se o IOMMU não confirmar um COMPLETION_WAIT, é desligado e o boot segue sem ele.
 */
#include "kernel.h"
#include "iommu.h"

#define R_DEVTAB   0x0000
#define R_CMDBUF   0x0008
#define R_EVTLOG   0x0010
#define R_CONTROL  0x0018
#define R_EXCL_BASE 0x0020
#define R_EXCL_LIM 0x0028
#define R_EFR      0x0030
#define R_CMD_HEAD 0x2000
#define R_CMD_TAIL 0x2008
#define R_EVT_HEAD 0x2010
#define R_EVT_TAIL 0x2018
#define R_STATUS   0x2020

#define C_EN        (1ull << 0)
#define C_EVTLOG_EN (1ull << 2)
#define C_COHERENT  (1ull << 10)
#define C_CMDBUF_EN (1ull << 12)

#define S_EVT_OVF   (1u << 0)
#define S_EVT_RUN   (1u << 3)
#define S_CMD_RUN   (1u << 4)

#define NENT 256              /* comandos e eventos: 256 x 16 bytes = 4 KiB, Len = 8 */

static struct {
    bool ok;
    volatile u8 *mm;
    u64 base;
    u16 devid, cap;
    u64 efr;
    u64 *devtab; u64 devtab_p;
    u32 *cmd; u64 cmd_p; u32 cmd_tail;
    u32 *evt; u64 evt_p; u32 evt_head;
    volatile u64 *wait; u64 wait_p;
    u64 root, tables;
    u32 ndev, faults;
    char st[120];
} I;

static inline u64 r64(u32 o) { return *(volatile u64 *)(I.mm + o); }
static inline void w64(u32 o, u64 v) { *(volatile u64 *)(I.mm + o) = v; }

static void *alloc_page(u64 *phys) { return dma_alloc(4096, phys); }

static bool cmd_push(const u32 c[4]) {
    u32 next = (I.cmd_tail + 16) % (NENT * 16);
    for (int i = 0; i < 100000 && (u32)(r64(R_CMD_HEAD) & 0x7FFF0) == next; i++) __asm__ volatile("pause");
    memcpy((u8 *)I.cmd + I.cmd_tail, c, 16);
    __asm__ volatile("mfence" ::: "memory");
    I.cmd_tail = next;
    w64(R_CMD_TAIL, I.cmd_tail);
    return true;
}

/* COMPLETION_WAIT com escrita de 0x1 em I.wait: confirma que tudo o que veio antes foi executado */
static bool cmd_sync(void) {
    *I.wait = 0;
    u32 c[4] = { (u32)(I.wait_p & ~7ull) | 1u, (u32)((I.wait_p >> 32) & 0xFFFFF) | (1u << 28), 1, 0 };
    cmd_push(c);
    u64 end = timer_ticks() + 100;
    while (*I.wait != 1) {
        if (timer_ticks() > end) return false;
        __asm__ volatile("pause");
    }
    return true;
}

static void inv_dte(u16 id) { u32 c[4] = { id, 2u << 28, 0, 0 }; cmd_push(c); }
static void inv_all(void) {
    if (I.efr & (1ull << 6)) { u32 c[4] = { 0, 8u << 28, 0, 0 }; cmd_push(c); return; }   /* INVALIDATE_IOMMU_ALL */
    /* INVALIDATE_IOMMU_PAGES de todo o domínio 1: S = 1, PDE = 1, endereço 0x7FFFFFFFFFFFF000 */
    u32 c[4] = { 0, 1u | (3u << 28), 0xFFFFF000u | 3u, 0x7FFFFFFFu };
    cmd_push(c);
}

/* ------------------------------------------------------------------ IVRS */
static void set_dev(u32 id, void *ctx) {
    (void)ctx;
    if (id > 0xFFFF) return;
    if (I.devtab && !(I.devtab[id * 4] & 1)) { iommu_dte(&I.devtab[id * 4], I.root, 1); I.ndev++; }
}

static bool parse_ivrs(void) {
    const u8 *t = acpi_find_table("IVRS", 0);
    if (!t) return false;
    iommu_ivhd_t h;
    I.ndev = 0;
    if (!iommu_ivrs_parse(t, &h, set_dev, NULL)) return false;
    I.base = h.base; I.devid = h.devid; I.cap = h.cap; I.efr = h.efr;
    return I.base != 0;
}

/* ------------------------------------------------------------------ programação do hardware */
static bool program(void) {
    w64(R_CONTROL, 0);                                         /* firmware pode ter deixado ligado */
    for (int i = 0; i < 100000 && (r64(R_STATUS) & (S_CMD_RUN | S_EVT_RUN)); i++) __asm__ volatile("pause");
    w64(R_DEVTAB, (I.devtab_p & IOPTE_ADDR) | 511);            /* 2 MiB = 512 páginas */
    w64(R_CMDBUF, (I.cmd_p & IOPTE_ADDR) | (8ull << 56));
    w64(R_EVTLOG, (I.evt_p & IOPTE_ADDR) | (8ull << 56));
    w64(R_CMD_HEAD, 0); w64(R_CMD_TAIL, 0); I.cmd_tail = 0;
    w64(R_EVT_HEAD, 0); w64(R_EVT_TAIL, 0); I.evt_head = 0;
    w64(R_EXCL_BASE, 0); w64(R_EXCL_LIM, 0);
    w64(R_STATUS, S_EVT_OVF | 2 | 4);                          /* limpa (RW1C) */
    w64(R_CONTROL, C_CMDBUF_EN | C_EVTLOG_EN | C_COHERENT);
    if (!cmd_sync()) return false;                             /* o buffer de comandos anda? */
    if (I.efr & (1ull << 6)) inv_all();                        /* IASup: um comando invalida DTE e páginas */
    else { for (u32 d = 0; d <= 0xFFFF; d++) if (I.devtab[d * 4] & 1) inv_dte((u16)d); inv_all(); }
    if (!cmd_sync()) return false;
    w64(R_CONTROL, C_CMDBUF_EN | C_EVTLOG_EN | C_COHERENT | C_EN);
    return cmd_sync();
}

void iommu_init(void) {
    snprintf(I.st, sizeof I.st, "IOMMU: ausente");
    if (!parse_ivrs()) { kprintf("IOMMU: sem IVRS (AMD-Vi desligado)\n"); return; }
    vmm_map_mmio(I.base, 0x80000);
    I.mm = (volatile u8 *)I.base;
    if (r64(R_EFR) == ~0ull) { kprintf("IOMMU: MMIO em 0x%llx não responde\n", I.base); return; }
    if (!I.efr) I.efr = r64(R_EFR);
    /* faixas de memória do mapa UEFI que um dispositivo pode legitimamente alcançar */
    static iommu_range_t rr[512];
    int n = 0;
    for (u32 i = 0; i < g_boot->region_count && n < 512; i++) {
        mem_region_t *m = &g_boot->regions[i];
        if (m->type == MEM_MMIO) continue;
        rr[n].base = m->base; rr[n].len = m->pages * 4096; n++;
    }
    I.devtab = dma_alloc(2u << 20, &I.devtab_p);
    I.cmd = dma_alloc(NENT * 16, &I.cmd_p);
    I.evt = dma_alloc(NENT * 16, &I.evt_p);
    I.wait = dma_alloc(4096, &I.wait_p);
    if (!I.devtab || !I.cmd || !I.evt || !I.wait) { kprintf("IOMMU: sem memória\n"); return; }
    I.root = iommu_build_identity(rr, n, alloc_page, &I.tables);
    if (!I.root) { kprintf("IOMMU: tabela de páginas falhou\n"); return; }
    if (!parse_ivrs() || !I.ndev) { kprintf("IOMMU: IVRS sem dispositivos\n"); return; }   /* agora com a raiz: preenche os DTE */
    if (!program()) {
        w64(R_CONTROL, 0);
        kprintf("IOMMU: não confirmou os comandos; desligado (DMA sem proteção)\n");
        snprintf(I.st, sizeof I.st, "IOMMU: desligado (sem resposta)");
        return;
    }
    I.ok = true;
    snprintf(I.st, sizeof I.st, "IOMMU AMD-Vi %04x em 0x%llx: %u requisitantes, %llu tabelas, %u falhas",
             I.devid, I.base, I.ndev, I.tables, I.faults);
    kprintf("IOMMU: AMD-Vi em 0x%llx (dispositivo %02x:%02x.%x), EFR 0x%llx, %u requisitantes em tradução identidade sobre %d faixas de memória (%llu tabelas)\n",
            I.base, I.devid >> 8, (I.devid >> 3) & 31, I.devid & 7, I.efr, I.ndev, n, I.tables);
}

void iommu_resume(void) {
    if (!I.ok) return;
    if (!program()) { w64(R_CONTROL, 0); I.ok = false; kprintf("IOMMU: não voltou do S3; desligado\n"); }
}

static const char *evt_name(u32 code) {
    switch (code) {
    case 1: return "ILLEGAL_DEV_TABLE_ENTRY"; case 2: return "IO_PAGE_FAULT"; case 3: return "DEV_TAB_HARDWARE_ERROR";
    case 4: return "PAGE_TAB_HARDWARE_ERROR"; case 5: return "ILLEGAL_COMMAND_ERROR"; case 6: return "COMMAND_HARDWARE_ERROR";
    case 7: return "IOTLB_INV_TIMEOUT"; case 8: return "INVALID_DEVICE_REQUEST";
    }
    return "evento";
}

void iommu_poll(void) {
    if (!I.ok) return;
    u32 tail = (u32)(r64(R_EVT_TAIL) & 0x7FFF0);
    while (I.evt_head != tail) {
        const u32 *e = (const u32 *)((u8 *)I.evt + I.evt_head);
        u32 code = e[1] >> 28;
        u16 dev = (u16)e[0];
        u64 addr = (u64)e[2] | (u64)e[3] << 32;
        I.faults++;
        if (I.faults <= 32)
            kprintf("IOMMU: %s do dispositivo %02x:%02x.%x em 0x%llx (flags 0x%x)\n", evt_name(code), dev >> 8, (dev >> 3) & 31, dev & 7,
                    addr, (e[1] >> 16) & 0xFFF);
        I.evt_head = (I.evt_head + 16) % (NENT * 16);
    }
    w64(R_EVT_HEAD, I.evt_head);
    if (r64(R_STATUS) & S_EVT_OVF) {                              /* log cheio: reinicia o registro de eventos */
        w64(R_CONTROL, r64(R_CONTROL) & ~C_EVTLOG_EN);
        w64(R_EVT_HEAD, 0); w64(R_EVT_TAIL, 0); I.evt_head = 0;
        w64(R_STATUS, S_EVT_OVF);
        w64(R_CONTROL, r64(R_CONTROL) | C_EVTLOG_EN);
    }
    snprintf(I.st, sizeof I.st, "IOMMU AMD-Vi %04x em 0x%llx: %u requisitantes, %llu tabelas, %u falhas",
             I.devid, I.base, I.ndev, I.tables, I.faults);
}

const char *iommu_status(void) { return I.st; }
