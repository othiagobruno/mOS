/*
 * mOS — driver Intel Wi-Fi (família AX210/"So": AX203, AX211, AX210...), em construção por etapas
 * testadas no hardware real (o QEMU não emula Wi-Fi):
 *   etapa 1 (pronta): ler e validar o firmware .ucode               -> wifi/iwl_fw.c
 *   etapa 2 (pronta): achar a placa, ligar o MAC, ler HW_REV / RF_ID -> este arquivo
 *   etapa 3: context info gen3 + IML + seções na DRAM, esperar o "alive"
 *   etapa 4: NVM (endereço MAC), PHY/MAC context, varredura UMAC
 *   etapa 5: associação + chaves (o handshake WPA2 é da pilha comum, wifi/ieee80211.c)
 * Todo o diagnóstico vai para o log do kernel (/Sistema/Biblioteca/Logs/kernel.log no pendrive).
 */
#include "iwl.h"
#include "vfs.h"

static const struct { u16 dev; const char *name, *fw; } ids[] = {
    {0x51F1, "Intel Wi-Fi 6 AX203/AX211", "iwlwifi-so-a0-hr-b0-89.ucode"},
    {0x51F0, "Intel Wi-Fi 6E AX211", "iwlwifi-so-a0-gf-a0-89.ucode"},
    {0x54F0, "Intel Wi-Fi 6E AX211", "iwlwifi-so-a0-gf-a0-89.ucode"},
    {0x7A70, "Intel Wi-Fi 6E AX211", "iwlwifi-so-a0-gf-a0-89.ucode"},
    {0x2725, "Intel Wi-Fi 6E AX210", "iwlwifi-ty-a0-gf-a0-89.ucode"},
};

static volatile u8 *regs;
static inline u32 rd(u32 o) { return mmio_r32(regs, o); }
static inline void wr(u32 o, u32 v) { mmio_w32(regs, o, v); }
static bool poll_bit(u32 reg, u32 mask, u32 want, u32 ms) {
    u64 end = timer_ticks() + ms;
    while ((rd(reg) & mask) != want) if (timer_ticks() > end) return false;
    return true;
}

static iwl_fw_t fw;

bool wifi_iwl_probe(void) {
    pci_dev_t *pd = NULL;
    int which = -1;
    for (int i = 0; i < pci_count() && !pd; i++) {
        pci_dev_t *d = pci_get(i);
        if (d->vendor != 0x8086) continue;
        for (u32 k = 0; k < ARRAY_LEN(ids); k++) if (d->device == ids[k].dev) { pd = d; which = (int)k; }
    }
    if (!pd) return false;
    kprintf("iwl: %s (8086:%04x) em %02x:%02x.%x — etapa 2 de 5 (diagnóstico)\n", ids[which].name, pd->device, pd->bus, pd->dev, pd->fn);
    u64 base = pci_bar_addr(pd, 0);
    if (!base) { kprintf("iwl: sem BAR0\n"); return false; }
    pci_enable_bus_master(pd);
    vmm_map_mmio(base, 0x4000);
    regs = (volatile u8 *)base;
    u32 rev = rd(CSR_HW_REV), rf = rd(CSR_HW_RF_ID);
    kprintf("iwl: BAR0 0x%llx, HW_REV 0x%08x (tipo 0x%03x, passo %u), RF_ID 0x%08x (tipo 0x%03x)\n", base, rev,
            (rev >> 4) & 0xFFF, rev & 3, rf, (rf >> 12) & 0xFFF);
    if (rev == 0xFFFFFFFF) { kprintf("iwl: a placa não responde (MMIO 0xFFFFFFFF)\n"); return false; }

    /* prepara o hardware: NIC_READY (como iwl_pcie_prepare_card_hw) */
    wr(CSR_HW_IF_CONFIG_REG, rd(CSR_HW_IF_CONFIG_REG) | CSR_HW_IF_CONFIG_REG_NIC_READY);
    bool ready = poll_bit(CSR_HW_IF_CONFIG_REG, CSR_HW_IF_CONFIG_REG_NIC_READY, CSR_HW_IF_CONFIG_REG_NIC_READY, 50);
    if (!ready) {
        wr(CSR_HW_IF_CONFIG_REG, rd(CSR_HW_IF_CONFIG_REG) | CSR_HW_IF_CONFIG_REG_PREPARE);
        sleep_ms(20);
        wr(CSR_HW_IF_CONFIG_REG, rd(CSR_HW_IF_CONFIG_REG) | CSR_HW_IF_CONFIG_REG_NIC_READY);
        ready = poll_bit(CSR_HW_IF_CONFIG_REG, CSR_HW_IF_CONFIG_REG_NIC_READY, CSR_HW_IF_CONFIG_REG_NIC_READY, 200);
    }
    kprintf("iwl: NIC_READY %s\n", ready ? "ok" : "NÃO (a placa não saiu do estado de baixo consumo)");
    /* liga o relógio do MAC (INIT_DONE -> MAC_CLOCK_READY) */
    wr(CSR_GP_CNTRL, rd(CSR_GP_CNTRL) | CSR_GP_CNTRL_INIT_DONE);
    bool clk = poll_bit(CSR_GP_CNTRL, CSR_GP_CNTRL_MAC_CLOCK_READY, CSR_GP_CNTRL_MAC_CLOCK_READY, 100);
    kprintf("iwl: relógio do MAC %s (GP_CNTRL 0x%08x)\n", clk ? "ok" : "NÃO ficou pronto", rd(CSR_GP_CNTRL));

    /* firmware */
    char path[96], err[96];
    snprintf(path, sizeof path, "/Sistema/Firmware/intel/%s", ids[which].fw);
    vnode_t *v = vfs_lookup(path);
    if (!v || !v->data) { snprintf(path, sizeof path, "/Sistema/Firmware/%s", ids[which].fw); v = vfs_lookup(path); }
    if (!v || !v->data) kprintf("iwl: firmware %s não encontrado em /Sistema/Firmware\n", ids[which].fw);
    else if (!iwl_fw_parse(v->data, v->size, &fw, err, sizeof err)) kprintf("iwl: firmware inválido: %s\n", err);
    else {
        kprintf("iwl: firmware %u ok: %d seções (%llu KiB), IML %u bytes, %u CPUs\n", fw.major, fw.n_rt, fw.rt_bytes >> 10, fw.iml_len, fw.num_cpu);
        extern bool iwl_gen3_load(volatile u8 *regs, const iwl_fw_t *fw);
        if (iwl_gen3_load(regs, &fw))
            kprintf("iwl: próximas etapas (NVM/MAC, varredura, conexão) dependem deste log do hardware\n");
    }
    return false;    /* ainda não registra um rádio: o Wi-Fi aparece como "placa detectada, sem driver" */
}
