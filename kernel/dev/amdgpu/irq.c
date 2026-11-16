/* mOS — interrupções da GPU AMD família Raven: anel do IH (OSSSYS 4.x), MSI, trap do SDMA e hot-plug (HPD).
 * Ver amdgpu.h. Testado no host contra um IH simulado (tools/amdgpu_hw_host.c).
 *
 * O anel fica na RAM com endereço de barramento (MC_SPACE = 1) e snoop, como o anel 0 do amdgpu: não passa
 * pela MMHUB nem pela GART, e o IOMMU (identidade sobre a RAM do mapa UEFI) deixa passar. O ponteiro de escrita
 * volta por writeback numa página de RAM. Sem MSI (tabela de tratadores cheia), o anel é lido por polling
 * em amdgpu_sdma_wait e nas chamadas de quem precisar. */
#include "amdgpu.h"
#include "regs.h"

#ifdef MOS_HOST_TEST
#define RR(a, o)    ((void)(a), amd_host_rreg(o))
#define WR(a, o, v) ((void)(a), amd_host_wreg(o, v))
#else
#define RR(a, o)    mmio_r32((a)->mmio, (o))
#define WR(a, o, v) mmio_w32((a)->mmio, (o), (v))
#endif

/* ------------------------------------------------------------ MSI */
#ifndef MOS_HOST_TEST
static void amdgpu_msi(void) {
    amdgpu_t *a = amdgpu_main();
    if (a && a->ih_ok) { a->irqs++; amdgpu_irq_process(a); }
}

static bool arm_msi(amdgpu_t *a) {
    pci_dev_t *pd = a->pci;
    u8 off = pci_read(pd, 0x34) & 0xFC;
    for (int g = 0; off && g < 48; g++) {
        u32 w = pci_read(pd, off);
        u8 next = (w >> 8) & 0xFC, id = w & 0xFF;
        if (id == 0x05) {                                /* MSI: um vetor, 0xE0, no LAPIC do BSP */
            u16 mc = (u16)(w >> 16);
            bool is64 = mc & (1u << 7);
            pci_write(pd, (u8)(off + 4), 0xFEE00000u);
            if (is64) { pci_write(pd, (u8)(off + 8), 0); pci_write(pd, (u8)(off + 12), 0xE0); }
            else pci_write(pd, (u8)(off + 8), 0xE0);
            mc = (u16)((mc & ~0x70u) | 1u);              /* 1 vetor, ligado */
            pci_write(pd, off, (w & 0xFFFF) | ((u32)mc << 16));
            pci_write(pd, 0x04, pci_read(pd, 0x04) | (1u << 10) | (1u << 2));   /* INTx desligado, bus master */
            msi_add_handler(amdgpu_msi);
            return true;
        }
        off = next;
    }
    return false;
}
#endif

/* ------------------------------------------------------------ anel */
bool amdgpu_ih_init(amdgpu_t *a) {
    if (!a || !a->ready) return false;
#ifdef MOS_HOST_TEST
    u64 ring = amd_host_ram_alloc(AMD_IH_RING_BYTES);
    u64 wb = amd_host_ram_page();
#else
    u64 ring = pmm_alloc(AMD_IH_RING_BYTES / 4096), wb = pmm_alloc(1);
#endif
    if (!ring || !wb) { kprintf("amdgpu: sem RAM para o anel do IH\n"); return false; }
    a->ih_phys = ring; a->ih_cpu = (volatile u32 *)(usize)ring; a->ih_dw = AMD_IH_RING_BYTES / 4;
    a->ih_wb_phys = wb; a->ih_wb = (volatile u32 *)(usize)wb;
    memset((void *)(usize)ring, 0, AMD_IH_RING_BYTES);
    memset((void *)(usize)wb, 0, 4096);
    a->ih_rptr = 0;

    /* desliga, controle do NBIO (sem leitura falsa: MSI; anel com snoop), anel novo */
    WR(a, R_IH_RB_CNTL, RR(a, R_IH_RB_CNTL) & ~(IH_RB_ENABLE | IH_ENABLE_INTR));
    WR(a, R_IH_RB_RPTR, 0);
    WR(a, R_IH_RB_WPTR, 0);
    WR(a, R_NBIO_INTERRUPT_CNTL2, (u32)(a->dummy_page >> 8));
    WR(a, R_NBIO_INTERRUPT_CNTL, RR(a, R_NBIO_INTERRUPT_CNTL) & ~(NBIO_IH_DUMMY_RD_OVERRIDE | NBIO_IH_REQ_NONSNOOP_EN));
    WR(a, R_IH_RB_BASE, (u32)(ring >> 8));
    WR(a, R_IH_RB_BASE_HI, (u32)(ring >> 40) & 0xFF);
    u32 order = 0;
    while ((1u << order) < a->ih_dw) order++;
#ifndef MOS_HOST_TEST
    a->msi_on = arm_msi(a);
#else
    a->msi_on = true;
#endif
    u32 c = RR(a, R_IH_RB_CNTL);
    c &= ~(IH_RB_SIZE_MASK | IH_MC_SPACE_MASK | IH_MC_VMID_MASK | IH_MC_RO | IH_RPTR_REARM);
    c |= IH_MC_SPACE_BUS | IH_WPTR_OVERFLOW_CLEAR | IH_WPTR_OVERFLOW_ENABLE | order << IH_RB_SIZE_SHIFT |
         IH_WPTR_WB_ENABLE | IH_MC_SNOOP;
    if (a->msi_on) c |= IH_RPTR_REARM;
    WR(a, R_IH_RB_CNTL, c);
    WR(a, R_IH_RB_WPTR_ADDR_LO, (u32)wb);
    WR(a, R_IH_RB_WPTR_ADDR_HI, (u32)(wb >> 32) & 0xFFFF);
    WR(a, R_IH_RB_WPTR, 0);
    WR(a, R_IH_RB_RPTR, 0);
    WR(a, R_IH_DOORBELL_RPTR, 0);                      /* sem doorbell: rptr por registrador */
    c = (c & ~IH_WPTR_OVERFLOW_CLEAR) | IH_RB_ENABLE | IH_RB_GPU_TS_ENABLE | IH_ENABLE_INTR;
    WR(a, R_IH_RB_CNTL, c);
    a->ih_ok = true;

    /* fontes: trap do SDMA e hot-plug (interrompe na mudança: a polaridade é o contrário do estado atual) */
    WR(a, R_SDMA_CNTL, RR(a, R_SDMA_CNTL) | SDMA_CNTL_TRAP_ENABLE);
    for (int i = 0; i < AMD_NUM_HPD; i++) {
        bool on = RR(a, R_DC_HPD_INT_STATUS(i)) & DC_HPD_SENSE;
        u32 h = RR(a, R_DC_HPD_INT_CONTROL(i)) & ~DC_HPD_INT_POLARITY;
        if (!on) h |= DC_HPD_INT_POLARITY;
        WR(a, R_DC_HPD_INT_CONTROL(i), h | DC_HPD_INT_EN | DC_HPD_INT_ACK);
    }
    kprintf("amdgpu: IH com %u KiB na RAM, %s\n", AMD_IH_RING_BYTES / 1024, a->msi_on ? "MSI no vetor 0xE0" : "sem MSI (polling)");
    return true;
}

static void hpd_irq(amdgpu_t *a, u32 i) {
    if (i >= AMD_NUM_HPD) { a->ih_unknown++; return; }
    bool on = RR(a, R_DC_HPD_INT_STATUS(i)) & DC_HPD_SENSE;
    u32 h = RR(a, R_DC_HPD_INT_CONTROL(i)) & ~DC_HPD_INT_POLARITY;
    if (!on) h |= DC_HPD_INT_POLARITY;                 /* a próxima mudança é no sentido contrário */
    WR(a, R_DC_HPD_INT_CONTROL(i), h | DC_HPD_INT_ACK);
    u32 bit = 1u << i, was = a->hpd_mask & bit;
    if (on) a->hpd_mask |= bit; else a->hpd_mask &= ~bit;
    if (!!was != on) { a->hpd_changed |= bit; a->hpd_events++; }
}

u32 amdgpu_irq_process(amdgpu_t *a) {
    if (!a || !a->ih_ok) return 0;
    u32 mask = a->ih_dw * 4 - 1, n = 0;
    bool eop = false;
    u32 w = a->ih_wb[0];
    if (w & IH_WPTR_RB_OVERFLOW) {
        w = RR(a, R_IH_RB_WPTR);
        if (w & IH_WPTR_RB_OVERFLOW) {
            /* estouro: recomeça pela entrada mais antiga que não foi sobrescrita */
            a->ih_overflows++;
            a->ih_rptr = ((w & ~IH_WPTR_RB_OVERFLOW) + 32) & mask;
            u32 c = RR(a, R_IH_RB_CNTL);
            WR(a, R_IH_RB_CNTL, c | IH_WPTR_OVERFLOW_CLEAR);
            WR(a, R_IH_RB_CNTL, c & ~IH_WPTR_OVERFLOW_CLEAR);
        }
    }
    w &= IH_WPTR_OFFSET_MASK & mask;
    for (u32 guard = 0; a->ih_rptr != w && guard < a->ih_dw / 8; guard++) {
        const volatile u32 *e = a->ih_cpu + a->ih_rptr / 4;
        u32 client = e[0] & 0xFF, src = (e[0] >> 8) & 0xFF;
        if (client == IH_CLIENT_SDMA0 && src == IH_SRC_SDMA_TRAP) a->sdma_traps++;
        else if (client == IH_CLIENT_DCE && src == IH_SRC_DC_HPD) hpd_irq(a, e[4]);
        else if ((client == IH_CLIENT_VMC || client == IH_CLIENT_UTCL2) && src == IH_SRC_VM_FAULT) {
            u32 vmid = (e[0] >> 24) & 0xF;
            a->vm_faults++;
            amdgpu_vm_fault_ack(a, client == IH_CLIENT_UTCL2);
            if (vmid) amdgpu_abi_vm_fault(vmid);
            else kprintf("amdgpu: falha de página no VMID 0 (status 0x%x)\n", a->vm_fault_status);
        }
        else if (client == IH_CLIENT_GRBM_CP && src == IH_SRC_CP_EOP) {
            if (((e[0] >> 16) & 0x0C) >> 2) a->comp_eops++;   /* ring_id: ME 1 = compute */
            else a->gfx_eops++;
            eop = true;
        }
        else if (client == IH_CLIENT_GRBM_CP && (src == IH_SRC_CP_PRIV_REG || src == IH_SRC_CP_PRIV_INSTR || src == IH_SRC_CP_BAD_OPCODE)) {
            a->gfx_faults++;
            kprintf("amdgpu: o CP recusou um pacote (%s)\n", src == IH_SRC_CP_PRIV_REG ? "registrador privilegiado" :
                    src == IH_SRC_CP_PRIV_INSTR ? "comando privilegiado" : "opcode inválido");
        }
        else if (client == IH_CLIENT_GRBM_CP && (src == IH_SRC_CP_CTX_EMPTY || src == IH_SRC_CP_CTX_BUSY || src == IH_SRC_GRBM_GUI_IDLE))
            a->gfx_idle_irqs++;                 /* ocioso/ocupado: só alimenta o relógio de deep sleep da APU */
        else a->ih_unknown++;
        a->ih_entries++;
        n++;
        a->ih_rptr = (a->ih_rptr + 32) & mask;
    }
    WR(a, R_IH_RB_RPTR, a->ih_rptr);
    if (eop) amdgpu_abi_poll();                  /* fences dos processos cujos lotes terminaram */
    return n;
}

u32 amdgpu_hpd_take(amdgpu_t *a) {
    if (!a) return 0;
    return __atomic_exchange_n(&a->hpd_changed, 0, __ATOMIC_ACQ_REL);
}
