/* mOS — teste de host de kernel/dev/amdgpu/amdgpu.c contra um Picasso simulado.
 * O "chip" é o arquivo de registradores do BAR5 (512 KiB) com os efeitos que o driver usa: o OTG varre
 * linhas a cada leitura da posição e conta quadros, a trava de atualização responde no bit de status, e o
 * endereço novo do plano só vale (EARLIEST_INUSE) no vblank depois de soltar a trava.
 * Rodar da raiz:
 *   gcc -O2 -Wall -Wextra -DMOS_HOST_TEST -I kernel/dev/amdgpu -I tools tools/amdgpu_hw_host.c kernel/dev/amdgpu/engine.c kernel/dev/amdgpu/irq.c kernel/dev/amdgpu/gfx.c kernel/dev/amdgpu/compute.c kernel/dev/amdgpu/vm.c kernel/dev/amdgpu/abi.c kernel/dev/amdgpu/pm4check.c kernel/dev/amdgpu/pm.c kernel/dev/amdgpu/ddc.c \
 *       kernel/dev/amdgpu/amdgpu.c kernel/dev/amdgpu/edid.c kernel/dev/amdgpu/ucode.c -o /tmp/amdgpu_hw && /tmp/amdgpu_hw */
#include "amdgpu.h"
#include "regs.h"
#include "ucode.h"
#include "pm4check.h"
#include <stdarg.h>
#include <stdlib.h>

static int pass, total;
#define CHECK(c, ...) do { total++; if (c) pass++; else { printf("FALHOU %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* ------------------------------------------------------------ chip simulado */
static u32 regs[AMD_MMIO_SIZE / 4];
static u64 now_ms;
static u32 vline, frames;
static bool locked, armed_h[AMD_NUM_OTG];
static u32 pending_lo_h[AMD_NUM_OTG], pending_hi_h[AMD_NUM_OTG];
static bool verbose;
static int sim_otg = 0;

static void ih_push(u32 client, u32 src, u32 ctx);
static u32 vtotal_of(void) { return (regs[R_OTG_V_TOTAL(sim_otg) / 4] & OTG_TIMING_MASK) + 1; }
static u32 vbs_of(void) { return regs[R_OTG_V_BLANK_START_END(sim_otg) / 4] & OTG_TIMING_MASK; }

static void scan(u32 lines) {
    u32 vt = vtotal_of();
    for (u32 k = 0; k < lines; k++) {
        vline++;
        for (int h = 0; h < AMD_NUM_OTG; h++)               /* troca no começo do vblank (todos os HUBPs do OTG) */
            if (vline == vbs_of() && armed_h[h] && !locked) {
                regs[R_DCSURF_EARLIEST_INUSE(h) / 4] = pending_lo_h[h];
                regs[R_DCSURF_EARLIEST_INUSE_HI(h) / 4] = pending_hi_h[h];
                regs[R_DCSURF_FLIP_CONTROL(h) / 4] &= ~FLIP_PENDING;
                armed_h[h] = false;
                if (regs[R_DCSURF_FLIP_INTERRUPT(h) / 4] & FLIP_INT_ENABLE) ih_push(IH_CLIENT_DCE, IH_SRC_DC_FLIP0 + h, 0);
            }
        if (vline == vbs_of() && (regs[R_OTG_GLOBAL_SYNC_STATUS(sim_otg) / 4] & OTG_VSTARTUP_INT_EN)) {
            regs[R_OTG_GLOBAL_SYNC_STATUS(sim_otg) / 4] |= OTG_VSTARTUP_OCCURRED;
            ih_push(IH_CLIENT_DCE, IH_SRC_DC_VSTARTUP0 + sim_otg, 0);
        }
        if (vline >= vt) { vline = 0; frames = (frames + 1) & OTG_FRAME_COUNT_MASK; }
    }
}

#define CARVE 0x3F0000000ull     /* MC_VM_FB_OFFSET do boot_state */
/* ---------- motores simulados: SMU, MMHUB e SDMA que executa os pacotes ---------- */
static bool smu_alive = true, smu_sdma_on, sdma_dead;
static bool gfxoff_allowed; static u64 gfxoff_enables, gc_off_writes;
/* registradores do GC: segmento 0 até o começo do DCN e o segmento 1 inteiro */
static bool is_gc_reg(u32 off) { u32 dw = off / 4; return (dw >= 0x2000 && dw < 0x34C0) || (dw >= 0xA000 && dw < 0x10000); }
static u32 ucode_words, ucode_bad, ucode_loaded_ver;
static const u32 *ucode_ref; static u32 ucode_ref_dw;
static u64 sdma_packets, sdma_faults;
/* tradução de um endereço MC pela hub (0 = MMHUB, 1 = GFXHUB): abertura do sistema (VRAM) ou GART do contexto 0 */
static u64 cp_faults;
static u32 hv(int hub, u32 r) { return hub ? GFXHUB_VM(r) : r; }
static u32 hm(int hub, u32 r) { return hub ? GFXHUB_MC(r) : r; }
static u8 *mc_ptr_h(int hub, u64 mc, bool write) {
    u64 *faults = hub ? &cp_faults : &sdma_faults;
    u64 fb0 = (u64)regs[hm(hub, R_MC_VM_FB_LOCATION_BASE) / 4] << 24, fb1 = ((u64)regs[hm(hub, R_MC_VM_FB_LOCATION_TOP) / 4] << 24) | 0xFFFFFF;
    u64 lo = (u64)regs[hm(hub, R_MC_VM_SYS_APERTURE_LOW) / 4] << 18, hi = ((u64)regs[hm(hub, R_MC_VM_SYS_APERTURE_HIGH) / 4] << 18) | 0x3FFFF;
    if (mc >= fb0 && mc <= fb1) {
        if (mc < lo || mc > hi) { (*faults)++; return NULL; }       /* fora da abertura do sistema */
        return amd_host_phys(CARVE + (mc - fb0));
    }
    u64 gs = ((u64)regs[hv(hub, R_VM_CTX0_PT_START_HI) / 4] << 44) | ((u64)regs[hv(hub, R_VM_CTX0_PT_START_LO) / 4] << 12);
    u64 ge = ((u64)regs[hv(hub, R_VM_CTX0_PT_END_HI) / 4] << 44) | ((u64)regs[hv(hub, R_VM_CTX0_PT_END_LO) / 4] << 12) | 0xFFF;
    if (!(regs[hv(hub, R_VM_CONTEXT0_CNTL) / 4] & CTX0_ENABLE) || mc < gs || mc > ge) { (*faults)++; return NULL; }
    u64 pt = ((u64)regs[hv(hub, R_VM_CTX0_PT_BASE_HI) / 4] << 32) | regs[hv(hub, R_VM_CTX0_PT_BASE_LO) / 4];
    if (!(pt & 1)) { (*faults)++; return NULL; }                 /* base física com o bit de válido */
    u64 *tbl = amd_host_phys(pt & AMD_PTE_ADDR_MASK);
    u64 pte = tbl[(mc - gs) >> 12];
    if (!(pte & AMD_PTE_VALID) || !(pte & AMD_PTE_SYSTEM) || (write && !(pte & AMD_PTE_WRITE))) { (*faults)++; return NULL; }
    return (u8 *)(uintptr_t)((pte & AMD_PTE_ADDR_MASK) + (mc & 0xFFF));
}
static u8 *mc_ptr(u64 mc, bool write) { return mc_ptr_h(0, mc, write); }
/* VMID 1..15 na GFXHUB: tabelas de 4 níveis com endereços físicos; PTE da VRAM aponta para o carve-out */
static u64 vm_walks, vm_faults;
static u8 *vm_ptr(u32 vmid, u64 va, bool write) {
    if (!vmid) return mc_ptr_h(1, va, write);
    vm_walks++;
    u32 cntl = regs[GFXHUB_VM(R_VM_CTX_CNTL(vmid)) / 4];
    u64 end = (((u64)regs[GFXHUB_VM(R_VM_CTX_PT_END_HI(vmid)) / 4] << 32) | regs[GFXHUB_VM(R_VM_CTX_PT_END_LO(vmid)) / 4]) << 12 | 0xFFF;
    u64 e = ((u64)regs[GFXHUB_VM(R_VM_CTX_PT_BASE_HI(vmid)) / 4] << 32) | regs[GFXHUB_VM(R_VM_CTX_PT_BASE_LO(vmid)) / 4];
    if (!(cntl & CTX0_ENABLE) || (cntl & CTX0_DEPTH_MASK) != CTX_DEPTH_4LEVEL || va > end) { vm_faults++; return NULL; }
    for (int lvl = 3; lvl >= 0; lvl--) {
        if (!(e & 1)) { vm_faults++; return NULL; }
        u64 *t = amd_host_phys(e & AMD_PTE_ADDR_MASK);
        e = t[(va >> (12 + 9 * lvl)) & 511];
    }
    if (!(e & AMD_PTE_VALID) || !(e & AMD_PTE_READ) || (write && !(e & AMD_PTE_WRITE))) { vm_faults++; return NULL; }
    u64 pa = (e & AMD_PTE_ADDR_MASK) + (va & 0xFFF);
    if (e & AMD_PTE_SYSTEM) return (u8 *)(uintptr_t)pa;
    if (pa < CARVE || pa >= CARVE + (256ull << 20)) { vm_faults++; return NULL; }
    return amd_host_phys(pa);
}
static void mc_xfer(u64 dst, u64 src, u64 n, bool fill, u32 val) {
    while (n) {
        u64 step = 4096 - (dst & 0xFFF);
        if (!fill && 4096 - (src & 0xFFF) < step) step = 4096 - (src & 0xFFF);
        if (step > n) step = n;
        u8 *d = mc_ptr(dst, true), *s = fill ? NULL : mc_ptr(src, false);
        if (!d || (!fill && !s)) return;
        if (fill) for (u64 i = 0; i < step; i += 4) memcpy(d + i, &val, 4);
        else memmove(d, s, step);
        dst += step; src += step; n -= step;
    }
}
/* IH simulado: entradas de 32 bytes no anel em RAM (endereço de barramento = ponteiro do teste) */
static u32 ih_w;            /* bytes */
static bool ih_ovf;
static u64 ih_pushed, hpd_acks;
static amdgpu_t *msi_target;   /* entrega da MSI: o driver esvazia o anel logo depois de cada entrada */
static int in_msi;
static u32 ih_ring_id;      /* ME/pipe/fila de quem gerou a entrada (dw0[23:16]) */
static u32 ih_vmid;         /* VMID da entrada (dw0[27:24]) */
static void ih_push(u32 client, u32 src, u32 ctx) {
    u32 c = regs[R_IH_RB_CNTL / 4];
    if (!(c & IH_RB_ENABLE) || (c & IH_MC_SPACE_MASK) != IH_MC_SPACE_BUS) return;
    u32 bytes = 4u << ((c & IH_RB_SIZE_MASK) >> IH_RB_SIZE_SHIFT);
    u32 *ring = (u32 *)(uintptr_t)(((u64)(regs[R_IH_RB_BASE_HI / 4] & 0xFF) << 40) | ((u64)regs[R_IH_RB_BASE / 4] << 8));
    u32 *e = ring + ih_w / 4;
    memset(e, 0, 32);
    e[0] = client | src << 8 | ih_ring_id << 16 | ih_vmid << 24; e[1] = (u32)ih_pushed; e[4] = ctx;
    ih_w = (ih_w + 32) & (bytes - 1);
    if (ih_w == regs[R_IH_RB_RPTR / 4]) ih_ovf = true;   /* alcançou o leitor: as mais antigas se perdem */
    ih_pushed++;
    u32 wv = ih_w | (ih_ovf ? IH_WPTR_RB_OVERFLOW : 0);
    regs[R_IH_RB_WPTR / 4] = wv;
    if (c & IH_WPTR_WB_ENABLE) {
        u32 *wb = (u32 *)(uintptr_t)(((u64)regs[R_IH_RB_WPTR_ADDR_HI / 4] << 32) | regs[R_IH_RB_WPTR_ADDR_LO / 4]);
        *wb = wv;
    }
    if (msi_target && (c & IH_ENABLE_INTR) && !in_msi) { in_msi = 1; amdgpu_irq_process(msi_target); in_msi = 0; }
}
static void hpd_set(int i, bool on) {
    bool was = regs[R_DC_HPD_INT_STATUS(i) / 4] & DC_HPD_SENSE;
    if (on) regs[R_DC_HPD_INT_STATUS(i) / 4] |= DC_HPD_SENSE; else regs[R_DC_HPD_INT_STATUS(i) / 4] &= ~DC_HPD_SENSE;
    u32 h = regs[R_DC_HPD_INT_CONTROL(i) / 4];
    bool rising = h & DC_HPD_INT_POLARITY;
    if ((h & DC_HPD_INT_EN) && was != on && on == rising) ih_push(IH_CLIENT_DCE, IH_SRC_DC_HPD, (u32)i);
}

static void sdma_run(void) {
    u32 rb = regs[R_SDMA_GFX_RB_CNTL / 4];
    if (sdma_dead || !smu_sdma_on || (regs[R_SDMA_F32_CNTL / 4] & SDMA_F32_HALT) || !(rb & SDMA_RB_ENABLE) ||
        !ucode_loaded_ver || ucode_bad) return;
    u32 dw = 1u << ((rb & SDMA_RB_SIZE_MASK) >> SDMA_RB_SIZE_SHIFT);
    u64 base = ((u64)regs[R_SDMA_GFX_RB_BASE_HI / 4] << 40) | ((u64)regs[R_SDMA_GFX_RB_BASE / 4] << 8);
    u32 *ring = (u32 *)mc_ptr(base, false);
    if (!ring) return;
    u32 rp = regs[R_SDMA_GFX_RB_RPTR / 4] >> 2, wp = regs[R_SDMA_GFX_RB_WPTR / 4] >> 2;
    #define RD(k) ring[(rp + (k)) & (dw - 1)]
    while (rp != wp) {
        u32 h = RD(0), op = h & 0xFF;
        u32 len = 1;
        if (op == SDMA_OP_NOP) len = 1;
        else if (op == SDMA_OP_COPY) { mc_xfer(((u64)RD(6) << 32) | RD(5), ((u64)RD(4) << 32) | RD(3), (u64)RD(1) + 1, false, 0); len = 7; }
        else if (op == SDMA_OP_CONST_FILL) { mc_xfer(((u64)RD(2) << 32) | RD(1), 0, (u64)RD(4) + 1, true, RD(3)); len = 5; }
        else if (op == SDMA_OP_FENCE) { u8 *d = mc_ptr(((u64)RD(2) << 32) | RD(1), true); u32 v = RD(3); if (d) memcpy(d, &v, 4); len = 4; }
        else if (op == SDMA_OP_WRITE) {
            u32 cnt = RD(3) + 1; u64 a = ((u64)RD(2) << 32) | RD(1);
            for (u32 k = 0; k < cnt; k++) { u8 *d = mc_ptr(a + 4 * k, true); u32 v = RD(4 + k); if (d) memcpy(d, &v, 4); }
            len = 4 + cnt;
        } else if (op == SDMA_OP_TRAP) {
            if (regs[R_SDMA_CNTL / 4] & SDMA_CNTL_TRAP_ENABLE) ih_push(IH_CLIENT_SDMA0, IH_SRC_SDMA_TRAP, 0);
            len = 2;
        }
        else { printf("FALHOU: pacote SDMA desconhecido 0x%x\n", h); total++; break; }
        sdma_packets++;
        rp = (rp + len) & (dw - 1);
    }
    #undef RD
    regs[R_SDMA_GFX_RB_RPTR / 4] = rp << 2;
    if (rb & SDMA_RPTR_WB_ENABLE) {
        u64 wa = ((u64)regs[R_SDMA_GFX_RB_RPTR_ADDR_HI / 4] << 32) | regs[R_SDMA_GFX_RB_RPTR_ADDR_LO / 4];
        u8 *d = mc_ptr(wa, true); u32 v = rp << 2;
        if (d) memcpy(d, &v, 4);
    }
}
/* ---------- GC simulado: RLC e CP (PFP, CE, ME) que conferem o microcódigo e executam PM4 ---------- */
typedef struct { u32 addr, data, start; const u32 *ref; u32 refdw, words, bad, ver; } uc_sim_t;
static uc_sim_t uc_rlc = {R_RLC_GPM_UCODE_ADDR, R_RLC_GPM_UCODE_DATA, RLC_UCODE_START, 0, 0, 0, 0, 0},
                uc_pfp = {R_CP_PFP_UCODE_ADDR, R_CP_PFP_UCODE_DATA, 0, 0, 0, 0, 0, 0},
                uc_ce  = {R_CP_CE_UCODE_ADDR, R_CP_CE_UCODE_DATA, 0, 0, 0, 0, 0, 0},
                uc_me  = {R_CP_ME_RAM_WADDR, R_CP_ME_RAM_DATA, 0, 0, 0, 0, 0, 0};
static uc_sim_t *ucs[] = {&uc_rlc, &uc_pfp, &uc_ce, &uc_me};
static bool smu_gfx_on, cp_dead, gfxoff_off, in_preamble, clear_state_done;
static u32 sh_regs[0x400];
static u32 sh_cfg[16], sh_bases[16], ctx_regs[0x400], srm_aram_words, scratch_writes;
static u64 cp_packets, cp_bad, cp_ibs;
static bool uc_ok(const uc_sim_t *u) { return u->ver && !u->bad && u->words == u->refdw; }
static bool uc_wreg(u32 off, u32 v) {
    for (usize i = 0; i < ARRAY_LEN(ucs); i++) {
        uc_sim_t *u = ucs[i];
        if (off == u->addr) {
            if (v == u->start) { u->words = 0; u->bad = 0; u->ver = 0; }
            else if (u->words == u->refdw) u->ver = v;
            else u->bad++;                                  /* versão escrita antes do fim */
            regs[off / 4] = v; return true;
        }
        if (off == u->data) { if (u->words >= u->refdw || v != u->ref[u->words]) u->bad++; u->words++; return true; }
    }
    return false;
}
static bool cp_can_run(void) {
    return !cp_dead && smu_gfx_on && (regs[R_RLC_CNTL / 4] & RLC_ENABLE_F32) && uc_ok(&uc_rlc) && uc_ok(&uc_pfp) &&
           uc_ok(&uc_ce) && uc_ok(&uc_me) && !(regs[R_CP_ME_CNTL / 4] & (CP_ME_HALT_ALL | CP_ME_RESET_ALL));
}
static u32 cur_vmid;
static void mem_w32(u64 mc, u32 v) { u8 *d = vm_ptr(cur_vmid, mc, true); if (d) memcpy(d, &v, 4); }
/* executa n dwords de PM4 em buf (anel com máscara, ou IB linear com mask = 0xFFFFFFFF) */
static void cp_exec(const u32 *buf, u32 pos, u32 end, u32 mask, int depth) {
    #define P(k) buf[(pos + (k)) & mask]
    while (pos != end) {
        u32 h = P(0);
        if (h >> 30 != 3) { printf("FALHOU: pacote PM4 tipo %u (0x%x)\n", h >> 30, h); total++; cp_bad++; return; }
        u32 op = (h >> 8) & 0xFF, n = (h >> 16) & 0x3FFF, len = (op == PM4_NOP && n == 0x3FFF) ? 1 : n + 2;
        cp_packets++;
        switch (op) {
        case PM4_NOP: case PM4_CONTEXT_CONTROL: break;
        case PM4_PREAMBLE_CNTL: in_preamble = P(1) == PM4_PREAMBLE_BEGIN_CLEAR; break;
        case PM4_SET_CONTEXT_REG:
            if (P(1) + n > 0x400 || !n) { cp_bad++; break; }
            for (u32 i = 0; i < n; i++) ctx_regs[P(1) + i] = P(2 + i);
            break;
        case PM4_CLEAR_STATE: clear_state_done = !in_preamble; break;
        case PM4C_SET_SH_REG:
            if ((P(1) & 0xFFFF) + n > 0x400 || !n) { cp_bad++; break; }
            for (u32 i = 0; i < n; i++) sh_regs[(P(1) & 0xFFFF) + i] = P(2 + i);
            break;
        case PM4_SET_BASE: if (P(1) != PM4_CE_PARTITION_BASE) cp_bad++; break;
        case PM4_SET_UCONFIG_REG: {
            u32 o = P(1) & 0xFFFF;
            for (u32 i = 0; i < n; i++) { regs[PM4_UCONFIG_REG_START + o + i] = P(2 + i); if (PM4_UCONFIG_REG_START + o + i == R_SCRATCH_REG0 / 4) scratch_writes++; }
            break;
        }
        case PM4_WRITE_DATA: {
            u32 ctl = P(1), dst = (ctl >> 8) & 0xF;
            u64 ad = ((u64)P(3) << 32) | P(2);
            for (u32 i = 0; i + 2 < n; i++) {
                if (dst == 5) mem_w32(ad + 4 * i, P(4 + i));
                else if (dst == 0) regs[(u32)ad + i] = P(4 + i);
                else cp_bad++;
            }
            break;
        }
        case PM4_RELEASE_MEM: {
            u32 ev = P(1), sel = P(2);
            if ((ev & 0x3F) != PM4_EV_CACHE_FLUSH_INV_TS || ((ev >> 8) & 0xF) != 5) cp_bad++;
            mem_w32(((u64)P(4) << 32) | P(3), P(5));
            u32 icntl = ih_ring_id ? R_CP_ME1_PIPE0_INT_CNTL : R_CP_INT_CNTL_RING0;
            if (((sel >> 24) & 7) == 2 && (regs[icntl / 4] & CP_INT_TIME_STAMP)) ih_push(IH_CLIENT_GRBM_CP, IH_SRC_CP_EOP, 0);
            break;
        }
        case PM4_INDIRECT_BUFFER: {
            u64 ib = ((u64)(P(2) & 0xFFFF) << 32) | (P(1) & ~3u);
            u32 ctl = P(3), ndw = ctl & 0xFFFFF, vmid = depth ? cur_vmid : (ctl >> 24) & 0xF;   /* o IB2 herda o VMID */
            static u32 ibuf[2][1 << 16];
            if (depth > 1 || ndw > (1u << 16) || (depth && (ctl >> 24))) { cp_bad++; break; }
            bool ok = true;
            for (u32 i = 0; i < ndw && ok; i++) {            /* busca dword a dword pela VM do IB */
                const u8 *q = vm_ptr(vmid, ib + 4ull * i, false);
                if (q) memcpy(&ibuf[depth][i], q, 4); else ok = false;
            }
            if (!ok) { cp_bad++; break; }
            cp_ibs++;
            u32 saved = cur_vmid;
            cur_vmid = vmid;
            cp_exec(ibuf[depth], 0, ndw, 0xFFFFFFFFu, depth + 1);
            cur_vmid = saved;
            break;
        }
        default: printf("FALHOU: pacote PM4 desconhecido 0x%x\n", op); total++; cp_bad++; break;
        }
        pos = (pos + len) & mask;
    }
    #undef P
}
static void cp_run(void) {
    if (!cp_can_run()) return;
    u32 cntl = regs[R_CP_RB0_CNTL / 4], dw = 2u << (cntl & 0x3F);
    u64 base = ((u64)regs[R_CP_RB0_BASE_HI / 4] << 40) | ((u64)regs[R_CP_RB0_BASE / 4] << 8);
    const u32 *ring = (const u32 *)mc_ptr_h(1, base, false);
    if (!ring) return;
    u32 rp = regs[R_CP_RB0_RPTR / 4] & (dw - 1), wp = regs[R_CP_RB0_WPTR / 4] & (dw - 1);
    cp_exec(ring, rp, wp, dw - 1, 0);
    regs[R_CP_RB0_RPTR / 4] = wp;
    mem_w32(((u64)(regs[R_CP_RB0_RPTR_ADDR_HI / 4] & 0xFFFF) << 32) | regs[R_CP_RB0_RPTR_ADDR / 4], wp);
}
/* MEC1: a tabela de saltos vem por registrador e o código pelo IC_BASE (na VRAM); a fila anda pelo doorbell */
static uc_sim_t uc_mec = {R_CP_MEC_ME1_UCODE_ADDR, R_CP_MEC_ME1_UCODE_DATA, 0, 0, 0, 0, 0, 0};
static const u32 *mec_code; static u32 mec_code_dw;
static bool mec_dead, hqd_bad_select, hqd_was_active;
static u64 db_rings, db_ignored;
static bool is_hqd_reg(u32 off) { return off >= R_CP_MQD_BASE_ADDR && off <= R_CP_HQD_PQ_WPTR_HI; }
static u64 gfx_db_rings;
void amd_host_doorbell(u32 idx, u64 v) {
    u32 gdb = regs[R_CP_RB_DOORBELL_CONTROL / 4];
    if ((regs[R_RCC_DOORBELL_APER_EN / 4] & DOORBELL_APER_EN) && (gdb & CP_RB_DOORBELL_EN) && ((gdb & 0x0FFFFFFCu) >> 2) == idx) {
        gfx_db_rings++;
        regs[R_CP_RB0_WPTR / 4] = (u32)v;                     /* o CP aplica a máscara do anel */
        cp_run();
        return;
    }
    u32 dbc = regs[R_CP_HQD_PQ_DOORBELL_CONTROL / 4];
    const u32 *ic = (const u32 *)mc_ptr_h(1, ((u64)regs[R_CP_CPC_IC_BASE_HI / 4] << 32) | regs[R_CP_CPC_IC_BASE_LO / 4], false);
    bool ok = !mec_dead && smu_gfx_on && (regs[R_RCC_DOORBELL_APER_EN / 4] & DOORBELL_APER_EN) && (regs[R_CP_PQ_STATUS / 4] & CP_PQ_STATUS_DOORBELL_EN) &&
              (regs[R_CP_HQD_ACTIVE / 4] & 1) && (dbc & CP_HQD_DB_EN) && ((dbc & CP_HQD_DB_OFFSET_MASK) >> CP_HQD_DB_OFFSET_SHIFT) == idx &&
              idx * 4 >= regs[R_CP_MEC_DOORBELL_RANGE_LOWER / 4] && idx * 4 <= regs[R_CP_MEC_DOORBELL_RANGE_UPPER / 4] &&
              regs[R_CP_MEC_CNTL / 4] == 0 && uc_ok(&uc_mec) && ic && !memcmp(ic, mec_code, mec_code_dw * 4) && !hqd_bad_select;
    if (!ok) { db_ignored++; return; }
    db_rings++;
    u32 dw = 2u << (regs[R_CP_HQD_PQ_CONTROL / 4] & CP_HQD_PQ_QUEUE_SIZE_MASK);
    u64 base = ((u64)regs[R_CP_HQD_PQ_BASE_HI / 4] << 40) | ((u64)regs[R_CP_HQD_PQ_BASE / 4] << 8);
    const u32 *ring = (const u32 *)mc_ptr_h(1, base, false);
    if (!ring) return;
    u32 rp = regs[R_CP_HQD_PQ_RPTR / 4] & (dw - 1), wp = (u32)v & (dw - 1);
    ih_ring_id = 1u << 2;
    cp_exec(ring, rp, wp, dw - 1, 0);
    ih_ring_id = 0;
    regs[R_CP_HQD_PQ_RPTR / 4] = wp;
    mem_w32(((u64)(regs[R_CP_HQD_PQ_RPTR_REPORT_ADDR_HI / 4] & 0xFFFF) << 32) | regs[R_CP_HQD_PQ_RPTR_REPORT_ADDR / 4], wp);
}
static bool gc_wreg(u32 off, u32 v) {
    if (off == uc_mec.addr) {                    /* a tabela de saltos começa em jt_offset, não em 0 */
        if (v == uc_mec.start) { uc_mec.words = uc_mec.bad = uc_mec.ver = 0; }
        else if (uc_mec.words == uc_mec.refdw) uc_mec.ver = v;
        else uc_mec.bad++;
        regs[off / 4] = v; return true;
    }
    if (off == uc_mec.data) { if (uc_mec.words >= uc_mec.refdw || v != uc_mec.ref[uc_mec.words]) uc_mec.bad++; uc_mec.words++; return true; }
    if (is_hqd_reg(off) && regs[R_GRBM_GFX_CNTL / 4] != 1u << GRBM_GFX_CNTL_MEID_SHIFT) hqd_bad_select = true;
    if (off == R_CP_HQD_DEQUEUE_REQUEST && (v & 1)) { regs[R_CP_HQD_ACTIVE / 4] &= ~1u; hqd_was_active = true; regs[off / 4] = v; return true; }
    if (uc_wreg(off, v)) return true;
    if (off == R_CP_RB0_WPTR) { regs[off / 4] = v; cp_run(); return true; }
    if (off == R_CP_ME_CNTL) { regs[off / 4] = v; if (!(v & CP_ME_HALT_ALL)) cp_run(); return true; }
    if (off == R_SH_MEM_CONFIG || off == R_SH_MEM_BASES) {
        u32 vm = (regs[R_GRBM_GFX_CNTL / 4] >> GRBM_GFX_CNTL_VMID_SHIFT) & 0xF;
        if (off == R_SH_MEM_CONFIG) sh_cfg[vm] = v; else sh_bases[vm] = v;
        regs[off / 4] = v; return true;
    }
    if (off == R_RLC_SRM_ARAM_DATA) { srm_aram_words++; return true; }
    if (off == hv(1, R_VM_INV_ENG17_REQ)) { regs[off / 4] = v; regs[hv(1, R_VM_INV_ENG17_ACK) / 4] |= v & 0xFFFF; return true; }
    return false;
}

/* ---------- motor de I2C do DCN simulado: um monitor (EDID com 3 blocos, DDC/CI) numa linha ---------- */
static u64 vst_acks;
static u8 i2c_buf[256]; static u32 i2c_wlen, i2c_rp;
static int mon_ddc = 1;                 /* DDC2: a linha do HDMI no VBIOS do Thiago (I2C 0x91, HPD2) */
static u8 mon_edid[384]; static u8 mon_seg, mon_off;
static u16 mon_bright = 50; static u8 mon_reply[11]; static bool mon_has_reply, i2c_hw_owned;
static u64 i2c_gos;
static void mon_init(void) {
    static const u8 hdr[8] = {0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0};
    for (int i = 0; i < 384; i++) mon_edid[i] = (u8)(i * 13 + 7);
    memcpy(mon_edid, hdr, 8);
    mon_edid[126] = 2;
    for (int b = 0; b < 3; b++) { u8 sum = 0; for (int i = 0; i < 127; i++) sum = (u8)(sum + mon_edid[b * 128 + i]); mon_edid[b * 128 + 127] = (u8)(0x100 - sum); }
}
static bool i2c_dev_write(u32 ddc, u8 addr, const u8 *p, u32 n) {
    if ((int)ddc != mon_ddc) return false;
    if (addr == 0x30) { if (n) mon_seg = p[0]; return true; }
    if (addr == 0x50) { if (n) mon_off = p[0]; return true; }
    if (addr == 0x37) {
        u8 c = 0x6E; for (u32 i = 0; i + 1 < n; i++) c ^= p[i];
        if (n < 5 || c != p[n - 1] || p[0] != 0x51) return true;          /* lixo: o monitor ignora */
        if (p[1] == 0x82 && p[2] == 0x01) {
            u8 vcp = p[3];
            u8 r[11] = {0x6E, 0x88, 0x02, (u8)(vcp == 0x10 ? 0 : 1), vcp, 0, 0, 100, (u8)(mon_bright >> 8), (u8)mon_bright, 0};
            u8 k = 0x50; for (int i = 0; i < 10; i++) k ^= r[i];
            r[10] = k; memcpy(mon_reply, r, 11); mon_has_reply = true;
        } else if (p[1] == 0x84 && p[2] == 0x03 && p[3] == 0x10) mon_bright = (u16)(p[4] << 8 | p[5]);
        return true;
    }
    return false;
}
static bool i2c_dev_read(u32 ddc, u8 addr, u8 *out, u32 n) {
    if ((int)ddc != mon_ddc) return false;
    if (addr == 0x50) { for (u32 i = 0; i < n; i++) { u32 at = mon_seg * 256u + mon_off + i; out[i] = at < 384 ? mon_edid[at] : 0xFF; } mon_off = (u8)(mon_off + n); return true; }
    if (addr == 0x37) { if (!mon_has_reply) return false; memcpy(out, mon_reply, n < 11 ? n : 11); mon_has_reply = false; return true; }
    return false;
}
static void i2c_go(u32 ctl) {
    u32 ddc = (ctl & I2C_DDC_SELECT_MASK) >> I2C_DDC_SELECT_SHIFT, n = ((ctl & I2C_TRANS_COUNT_MASK) >> I2C_TRANS_COUNT_SHIFT) + 1;
    u32 arb = (regs[R_DC_I2C_ARBITRATION / 4] & I2C_ARB_STATUS_MASK) >> I2C_ARB_STATUS_SHIFT;
    u32 st = I2C_SW_DONE;
    if (arb != I2C_ST_USED_BY_SW || !(regs[R_DC_I2C_DDC_SETUP(ddc) / 4] & I2C_SETUP_ENABLE) || !(regs[R_DC_I2C_DDC_SPEED(ddc) / 4] >> 16)) st = I2C_SW_ABORTED;
    u32 pos = 0, out = i2c_wlen;
    for (u32 t = 0; t < n && st == I2C_SW_DONE; t++) {
        u32 tr = regs[R_DC_I2C_TRANSACTION(t) / 4], len = (tr & I2C_TR_COUNT_MASK) >> I2C_TR_COUNT_SHIFT;
        u8 ab = i2c_buf[pos++];
        bool rd = tr & I2C_TR_RW;
        if (rd != (ab & 1)) { st = I2C_SW_ABORTED; break; }
        if (rd) { if (out + len > AMD_I2C_BUFFER || !i2c_dev_read(ddc, ab >> 1, i2c_buf + out, len)) st = I2C_SW_NACK | I2C_SW_DONE; out += len; }
        else { if (!i2c_dev_write(ddc, ab >> 1, i2c_buf + pos, len)) st = I2C_SW_NACK | I2C_SW_DONE; pos += len; }
    }
    mon_seg = 0;                           /* o ponteiro de segmento do E-DDC volta a 0 depois do STOP */
    regs[R_DC_I2C_SW_STATUS / 4] = st;
    i2c_gos++;
}
static bool otg_irq_wreg(u32 off, u32 v) {
    for (int i = 0; i < AMD_NUM_OTG; i++) {
        if (off == R_OTG_GLOBAL_SYNC_STATUS(i)) { regs[off / 4] = (v & ~OTG_VSTARTUP_CLEAR) & ~((v & OTG_VSTARTUP_CLEAR) ? OTG_VSTARTUP_OCCURRED : 0); vst_acks++; return true; }
        if (off == R_DCSURF_FLIP_INTERRUPT(i)) { regs[off / 4] = v & ~FLIP_INT_CLEAR; return true; }
    }
    return false;
}
static bool i2c_wreg(u32 off, u32 v) {
    if (off == R_DC_I2C_ARBITRATION) {
        u32 r = v & ~(I2C_ARB_SW_USE_REQ | I2C_ARB_SW_DONE | I2C_ARB_STATUS_MASK);
        u32 st = (regs[off / 4] & I2C_ARB_STATUS_MASK) >> I2C_ARB_STATUS_SHIFT;
        if (i2c_hw_owned) st = I2C_ST_USED_BY_HW;
        else if (v & I2C_ARB_SW_DONE) st = I2C_ST_IDLE;
        else if (v & I2C_ARB_SW_USE_REQ) st = I2C_ST_USED_BY_SW;
        regs[off / 4] = r | st << I2C_ARB_STATUS_SHIFT;
        return true;
    }
    if (off == R_DC_I2C_DATA) {
        u32 d = (v & I2C_DATA_MASK) >> I2C_DATA_SHIFT;
        if (v & I2C_DATA_RW) { if (v & I2C_INDEX_WRITE) i2c_rp = (v >> I2C_INDEX_SHIFT) & 0x3FF; return true; }
        if (v & I2C_INDEX_WRITE) i2c_wlen = (v >> I2C_INDEX_SHIFT) & 0x3FF;
        if (i2c_wlen < sizeof i2c_buf) i2c_buf[i2c_wlen++] = (u8)d;
        return true;
    }
    if (off == R_DC_I2C_CONTROL) {
        if (v & I2C_SW_STATUS_RESET) regs[R_DC_I2C_SW_STATUS / 4] = 0;
        regs[off / 4] = v & ~(I2C_GO | I2C_SW_STATUS_RESET);
        if (v & I2C_GO) i2c_go(v);
        return true;
    }
    return false;
}

/* ---------- motor DP_AUX simulado: um sink DP na AUX0 (DPCD + EDID por I2C sobre AUX) ---------- */
static u8 aux_wbuf[6][32], aux_rbuf[6][32]; static u32 aux_wi[6], aux_ri[6];
static int aux_sink = 0, aux_defers;
static u8 dpcd[0x1000];
static u8 aux_i2c_seg, aux_i2c_off;
static u64 aux_gos;
static int aux_inst_of(u32 off) {
    for (int i = 0; i < 6; i++) if (off >= R_AUX_CONTROL(i) && off <= R_AUX_SW_DATA(i)) return i;
    return -1;
}
static void aux_go(int i) {
    u8 *b = aux_wbuf[i];
    u32 n = (regs[R_AUX_SW_CONTROL(i) / 4] & AUX_SW_WR_BYTES_MASK) >> AUX_SW_WR_BYTES_SHIFT;
    u8 cmd = b[0] & 0xF0; u32 addr = (u32)(b[0] & 0xF) << 16 | (u32)b[1] << 8 | b[2];
    u32 len = n > 3 ? (u32)b[3] + 1 : 0;
    bool rd = cmd & AUX_CMD_READ, native = cmd & 0x80;
    u32 st = AUX_SW_DONE, cnt = 1;
    aux_gos++;
    memset(aux_rbuf[i], 0, sizeof aux_rbuf[i]);
    if (i != aux_sink) st |= AUX_SW_HPD_DISCON;
    else if (aux_defers > 0) { aux_defers--; aux_rbuf[i][0] = (native ? AUX_REPLY_DEFER : AUX_REPLY_I2C_DEFER) << 4; }
    else if (native) {
        if (addr + len > sizeof dpcd) aux_rbuf[i][0] = AUX_REPLY_NACK << 4;
        else if (rd) { memcpy(aux_rbuf[i] + 1, dpcd + addr, len); cnt += len; }
        else memcpy(dpcd + addr, b + 4, len);
    } else {
        if (addr == 0x30 && !rd && len) aux_i2c_seg = b[4];
        else if (addr == 0x50 && !rd && len) aux_i2c_off = b[4];
        else if (addr == 0x50 && rd) { for (u32 k = 0; k < len; k++) { u32 at = aux_i2c_seg * 256u + aux_i2c_off + k; aux_rbuf[i][1 + k] = at < 384 ? mon_edid[at] : 0xFF; } aux_i2c_off = (u8)(aux_i2c_off + len); cnt += len; }
        else if (len) aux_rbuf[i][0] = AUX_REPLY_I2C_NACK << 4;
        if (!(cmd & AUX_CMD_MOT)) aux_i2c_seg = 0;      /* fim da transação I2C */
    }
    regs[R_AUX_SW_STATUS(i) / 4] = st | cnt << AUX_SW_REPLY_COUNT_SHIFT;
}
static bool aux_wreg(u32 off, u32 v) {
    int i = aux_inst_of(off);
    if (i < 0) return false;
    if (off == R_AUX_ARB_CONTROL(i)) {
        u32 st = (regs[off / 4] & AUX_ARB_STATUS_MASK) >> AUX_ARB_STATUS_SHIFT;
        if (v & AUX_SW_DONE_USING) st = 0; else if ((v & AUX_SW_USE_REQ) && st != AUX_ST_DMCU) st = AUX_ST_SW;
        regs[off / 4] = (v & ~(AUX_SW_DONE_USING | AUX_ARB_STATUS_MASK)) | st << AUX_ARB_STATUS_SHIFT;
        return true;
    }
    if (off == R_AUX_CONTROL(i)) { regs[off / 4] = (v & AUX_RESET) ? v | AUX_RESET_DONE : v & ~AUX_RESET_DONE; return true; }
    if (off == R_AUX_INTERRUPT_CONTROL(i)) { if (v & AUX_SW_DONE_ACK) regs[R_AUX_SW_STATUS(i) / 4] &= ~AUX_SW_DONE; regs[off / 4] = v & ~AUX_SW_DONE_ACK; return true; }
    if (off == R_AUX_SW_DATA(i)) {
        u32 d = (v & AUX_SW_DATA_MASK) >> AUX_SW_DATA_SHIFT, idx = (v >> AUX_SW_INDEX_SHIFT) & 0x1F;
        if (v & AUX_SW_DATA_RW) { if (v & AUX_SW_AUTOINC_DISABLE) aux_ri[i] = idx; return true; }
        if (v & AUX_SW_AUTOINC_DISABLE) aux_wi[i] = idx;
        if (aux_wi[i] < 32) aux_wbuf[i][aux_wi[i]++] = (u8)d;
        return true;
    }
    if (off == R_AUX_SW_CONTROL(i)) { regs[off / 4] = v & ~AUX_SW_GO; if (v & AUX_SW_GO) { aux_go(i); aux_wi[i] = 0; } return true; }
    return false;
}

static bool otg_irq_wreg(u32 off, u32 v);
static bool engine_wreg(u32 off, u32 v) {
    if (otg_irq_wreg(off, v)) return true;
    if (aux_wreg(off, v)) return true;
    if (i2c_wreg(off, v)) return true;
    if (gc_wreg(off, v)) return true;
    if (off == R_MP1_C2PMSG_66) {
        if (!smu_alive) return true;
        regs[R_MP1_C2PMSG_90 / 4] = SMU_RESP_OK;
        if (v == SMU_MSG_GET_VERSION) regs[R_MP1_C2PMSG_82 / 4] = 0x41E4D;
        else if (v == SMU_MSG_POWER_UP_SDMA) smu_sdma_on = true;
        else if (v == SMU_MSG_POWER_DOWN_SDMA) smu_sdma_on = false;
        else if (v == SMU_MSG_POWER_UP_GFX) smu_gfx_on = true;
        else if (v == SMU_MSG_DISABLE_GFXOFF) { gfxoff_off = true; gfxoff_allowed = false; }
        else if (v == SMU_MSG_ENABLE_GFXOFF) { gfxoff_allowed = true; gfxoff_enables++; }
        else regs[R_MP1_C2PMSG_90 / 4] = 0xFF;                        /* mensagem desconhecida */
        regs[R_MP1_C2PMSG_66 / 4] = v;
        return true;
    }
    if (off == R_VM_INV_ENG17_REQ) { regs[off / 4] = v; regs[R_VM_INV_ENG17_ACK / 4] |= v & 0xFFFF; return true; }
    if (off == R_SDMA_UCODE_ADDR) {
        if (v == 0) { ucode_words = 0; ucode_bad = 0; ucode_loaded_ver = 0; }
        else if (ucode_words == ucode_ref_dw) ucode_loaded_ver = v;
        regs[off / 4] = v; return true;
    }
    if (off == R_SDMA_UCODE_DATA) {
        if (ucode_words >= ucode_ref_dw || v != ucode_ref[ucode_words]) ucode_bad++;
        ucode_words++; return true;
    }
    if (off == R_SDMA_GFX_RB_WPTR) { regs[off / 4] = v; sdma_run(); return true; }
    if (off == R_IH_RB_CNTL) {
        regs[off / 4] = v & ~IH_WPTR_OVERFLOW_CLEAR;
        if (v & IH_WPTR_OVERFLOW_CLEAR) { ih_ovf = false; regs[R_IH_RB_WPTR / 4] &= ~IH_WPTR_RB_OVERFLOW; }
        return true;
    }
    if (off == R_IH_RB_WPTR) { regs[off / 4] = v; ih_w = v & IH_WPTR_OFFSET_MASK; return true; }
    for (int i = 0; i < AMD_NUM_HPD; i++)
        if (off == R_DC_HPD_INT_CONTROL(i)) { if (v & DC_HPD_INT_ACK) hpd_acks++; regs[off / 4] = v & ~DC_HPD_INT_ACK; return true; }
    if (off == R_SDMA_GFX_RB_RPTR) { regs[off / 4] = v; return true; }
    return false;
}

u32 amd_host_rreg(u32 off) {
    { int ai = aux_inst_of(off); if (ai >= 0 && off == R_AUX_SW_DATA(ai)) { u32 d = aux_ri[ai] < 32 ? aux_rbuf[ai][aux_ri[ai]] : 0; aux_ri[ai]++; return d << AUX_SW_DATA_SHIFT; } }
    if (off == R_DC_I2C_DATA) { u32 d = i2c_rp < sizeof i2c_buf ? i2c_buf[i2c_rp] : 0; i2c_rp++; return d << I2C_DATA_SHIFT; }
    if (off == R_VM_INV_ENG17_SEM) return 1;
    if (off >= AMD_MMIO_SIZE) { printf("FALHOU: leitura fora do BAR5 0x%x\n", off); total++; return 0xFFFFFFFFu; }
    if (off == R_OTG_STATUS_POSITION(sim_otg)) { scan(7); return vline; }
    if (off == R_OTG_STATUS(sim_otg)) { scan(3); return vline >= vbs_of() ? OTG_V_BLANK : 0; }
    if (off == R_OTG_STATUS_FRAME_COUNT(sim_otg)) { scan(1); return frames; }
    if (off == R_OTG_MASTER_UPDATE_LOCK(sim_otg)) return locked ? (OTG_UPDATE_LOCK | OTG_UPDATE_LOCK_STATUS) : 0;
    for (int h = 0; h < AMD_NUM_OTG; h++) if (off == R_DCSURF_FLIP_CONTROL(h)) scan(11);
    return regs[off / 4];
}

void amd_host_wreg(u32 off, u32 v) {
    if (off >= AMD_MMIO_SIZE) { printf("FALHOU: escrita fora do BAR5 0x%x\n", off); total++; return; }
    if (gfxoff_allowed && is_gc_reg(off)) gc_off_writes++;      /* com o GC dormindo a escrita se perderia */
    if (engine_wreg(off, v)) return;
    if (off == R_OTG_MASTER_UPDATE_LOCK(sim_otg)) { locked = v & OTG_UPDATE_LOCK; return; }
    regs[off / 4] = v;
    for (int h = 0; h < AMD_NUM_OTG; h++)
        if (off == R_DCSURF_PRIMARY_ADDR(h)) {
            pending_lo_h[h] = v; pending_hi_h[h] = regs[R_DCSURF_PRIMARY_ADDR_HI(h) / 4];
            armed_h[h] = true;
            regs[R_DCSURF_FLIP_CONTROL(h) / 4] |= FLIP_PENDING;
            if (!locked) printf("aviso: endereço escrito sem a trava\n");
        }
}

static u16 pci_cmd = 0x0006;
static u8 pci_rev = 0x75;
u32 pci_read(pci_dev_t *d, u8 off) {
    (void)d;
    if (off == 0x04) return pci_cmd;
    if (off == 0x08) return 0x03000000u | pci_rev;
    return 0;
}
u64 pci_bar_addr(pci_dev_t *d, int bar) {
    (void)d;
    return bar == 0 ? 0xE0000000ull : bar == 2 ? 0xF0000000ull : bar == 5 ? 0xFCC00000ull : 0;
}
/* VRAM simulada: os 256 MiB do BAR0 (alocados sob demanda) */
static u8 *vram;
static u64 wc_lo, wc_len;
/* VRAM pelo BAR0 (0xE0000000) ou pelo carve-out (MC_VM_FB_OFFSET = 0x3F0000000); o resto é RAM do teste */

void *amd_host_phys(u64 phys) {
    if (!vram) vram = calloc(1, 256u << 20);
    if (phys >= 0xE0000000ull && phys < 0xE0000000ull + (256ull << 20)) return vram + (phys - 0xE0000000ull);
    if (phys >= CARVE && phys < CARVE + (256ull << 20)) return vram + (phys - CARVE);
    return (void *)(uintptr_t)phys;
}
const u8 *amd_host_fw(const char *name, u64 *len) {
    static u8 *cache[8]; static char names[8][32]; static u64 lens[8];
    for (int i = 0; i < 8; i++) if (cache[i] && !strcmp(names[i], name)) { *len = lens[i]; return cache[i]; }
    char path[96];
    snprintf(path, sizeof path, "firmware/amdgpu/%s", name);
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    u8 *b = aligned_alloc(16, ((usize)n + 15) & ~(usize)15);
    if (fread(b, 1, (usize)n, f) != (usize)n) { fclose(f); return NULL; }
    fclose(f);
    for (int i = 0; i < 8; i++) if (!cache[i]) { cache[i] = b; snprintf(names[i], 32, "%s", name); lens[i] = (u64)n; break; }
    *len = (u64)n;
    return b;
}
u64 amd_host_ram_page(void) { return (u64)(uintptr_t)aligned_alloc(4096, 4096); }
u64 amd_host_ram_alloc(u64 bytes) { return (u64)(uintptr_t)aligned_alloc(4096, (bytes + 4095) & ~4095ull); }
void vmm_map_wc(u64 phys, u64 size) { wc_lo = phys; wc_len = size; }
void amd_host_idle(void) { scan(200); now_ms++; }
u64 timer_ticks(void) { return now_ms++; }
void sleep_ms(u64 ms) {
    /* 60 Hz em 5120x1440 (vtotal 1485): 89100 linhas por segundo */
    u64 lines = ms * 89100 / 1000;
    scan((u32)lines);
    now_ms += ms;
}
void kprintf(const char *fmt, ...) {
    if (!verbose) return;
    va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap);
}

/* estado deixado pela UEFI no PC do Thiago: 5120x1440@60 no OTG0, plano linear no começo da VRAM */
static const u64 FB_MC = 0xF400000000ull;
static void boot_state(const edid_mode_t *m, u32 rev_id) {
    memset(regs, 0, sizeof regs);
    vline = 0; frames = 1000; locked = false; memset(armed_h, 0, sizeof armed_h);
    regs[R_RCC_DEV0_EPF0_STRAP0 / 4] = rev_id << STRAP0_ATI_REV_ID_SHIFT | 0x1234;
    regs[R_MC_VM_FB_LOCATION_BASE / 4] = 0xF400;
    regs[R_MC_VM_FB_LOCATION_TOP / 4] = 0xF40F;      /* 256 MiB */
    regs[R_MC_VM_FB_OFFSET / 4] = 0x3F0;             /* física 0x3F0000000 */
    regs[GFXHUB_MC(R_MC_VM_FB_LOCATION_BASE) / 4] = 0xF400;   /* a GFXHUB vê a mesma VRAM */
    regs[GFXHUB_MC(R_MC_VM_FB_LOCATION_TOP) / 4] = 0xF40F;
    regs[GFXHUB_MC(R_MC_VM_FB_OFFSET) / 4] = 0x3F0;
    regs[R_CC_GC_SHADER_ARRAY_CONFIG / 4] = 0x07000000;
    regs[R_CP_HQD_ACTIVE / 4] = 1;                          /* fila deixada no ar: o driver tira antes */     /* CUs 8..10 desligados de fábrica: 8 CUs (Vega 8) */
    u32 t[8];
    amd_otg_encode(m, t);
    regs[R_OTG_H_TOTAL(0) / 4] = t[0]; regs[R_OTG_H_BLANK_START_END(0) / 4] = t[1];
    regs[R_OTG_H_SYNC_A(0) / 4] = t[2]; regs[R_OTG_H_SYNC_A_CNTL(0) / 4] = t[3];
    regs[R_OTG_V_TOTAL(0) / 4] = t[4]; regs[R_OTG_V_BLANK_START_END(0) / 4] = t[5];
    regs[R_OTG_V_SYNC_A(0) / 4] = t[6]; regs[R_OTG_V_SYNC_A_CNTL(0) / 4] = t[7];
    regs[R_OTG_CONTROL(0) / 4] = OTG_MASTER_EN;
    regs[R_DCSURF_SURFACE_CONFIG(0) / 4] = SURFACE_FMT_ARGB8888;
    regs[R_DCSURF_PRI_VIEWPORT_DIM(0) / 4] = m->hactive | (u32)m->vactive << 16;
    regs[R_DCSURF_SURFACE_PITCH(0) / 4] = m->hactive - 1;
    regs[R_DCSURF_PRIMARY_ADDR(0) / 4] = (u32)FB_MC;
    regs[R_DCSURF_PRIMARY_ADDR_HI(0) / 4] = (u32)(FB_MC >> 32);
    regs[R_DCSURF_EARLIEST_INUSE(0) / 4] = (u32)FB_MC;
    regs[R_DCSURF_EARLIEST_INUSE_HI(0) / 4] = (u32)(FB_MC >> 32);
    regs[R_HUBPRET_CONTROL(0) / 4] = 2u << XBAR_CB_B_SHIFT | 3u << XBAR_CR_R_SHIFT | 0x5;
    for (int i = 1; i < AMD_NUM_OTG; i++) regs[R_DCHUBP_CNTL(i) / 4] = HUBP_BLANK_EN;
    regs[R_DC_HPD_INT_STATUS(4) / 4] = DC_HPD_SENSE;  /* HDMI */
    regs[R_MP1_C2PMSG_90 / 4] = SMU_RESP_OK;          /* a UEFI/o SMU deixam a última resposta (1) */
    regs[R_SDMA_F32_CNTL / 4] = SDMA_F32_HALT;
    regs[R_MC_VM_MX_L1_TLB_CNTL / 4] = 0x2501;        /* valores de reset da MMHUB 1.0 */
    regs[R_VM_L2_CNTL / 4] = 0x00080602;
    regs[R_VM_CONTEXT0_CNTL / 4] = 0x007FFE80;
}

static edid_mode_t mode_5120(void) {
    edid_mode_t m = {0};
    m.pclk_khz = 481140; m.hactive = 5120; m.hfront = 88; m.hsync = 44; m.hback = 148;
    m.vactive = 1440; m.vfront = 3; m.vsync = 5; m.vback = 37; m.flags = EDID_MODE_HPOS;
    return m;
}

static void test_otg_roundtrip(void) {
    FILE *f = fopen("tools/testdata/amdgpu/edid-hdmi-thiago.bin", "rb");
    u8 b[384];
    CHECK(f && fread(b, 1, 384, f) == 384, "EDID");
    if (f) fclose(f);
    edid_info_t e;
    edid_parse(b, 384, &e);
    int ok = 0;
    for (int i = 0; i < e.nmodes; i++) {
        const edid_mode_t *m = &e.modes[i];
        if (m->flags & EDID_MODE_INTERLACE) continue;
        u32 r[8];
        amd_otg_encode(m, r);
        amd_otg_timing_t t;
        amd_otg_decode(r, &t);
        edid_mode_t back;
        amd_otg_to_mode(&t, &back);
        bool same = back.hactive == m->hactive && back.hfront == m->hfront && back.hsync == m->hsync && back.hback == m->hback &&
                    back.vactive == m->vactive && back.vfront == m->vfront && back.vsync == m->vsync && back.vback == m->vback &&
                    (back.flags & (EDID_MODE_HPOS | EDID_MODE_VPOS)) == (m->flags & (EDID_MODE_HPOS | EDID_MODE_VPOS));
        CHECK(same, "OTG ida e volta %ux%u", m->hactive, m->vactive);
        ok += same;
    }
    CHECK(ok >= 25, "%d modos no OTG", ok);
    /* registradores como o programador de timing do DCN grava para 1920x1080@60 (CTA VIC 16) */
    edid_mode_t m;
    edid_vic_lookup(16, &m);
    u32 r[8];
    amd_otg_encode(&m, r);
    CHECK(r[0] == 2199 && r[1] == (2112u | 192u << 16) && r[2] == 44u << 16 && r[3] == 0 &&
          r[4] == 1124 && r[5] == (1121u | 41u << 16) && r[6] == 5u << 16 && r[7] == 0, "VIC 16 no OTG");
}

static void test_probe_and_vsync(void) {
    edid_mode_t m = mode_5120();
    boot_state(&m, 1);
    pci_dev_t d = {0};
    d.vendor = 0x1002; d.device = 0x15D8;
    amdgpu_t a;
    CHECK(amdgpu_probe(&a, &d, 0xE0000000ull, 5120, 1440, 5120), "probe");
    CHECK(a.variant == AMD_PICASSO && !strcmp(a.rlc_name, "picasso_rlc.bin"), "variante %d rlc %s", a.variant, a.rlc_name);
    CHECK(a.fb_start == FB_MC && a.vram_size == (256ull << 20) && a.fb_phys == 0x3F0000000ull, "VRAM");
    CHECK(a.pipe == 0 && a.uefi_match && a.uefi_mc == FB_MC && a.uefi_pitch_px == 5120 && !a.uefi_rgba, "pipe da tela");
    CHECK(a.otg[0].hactive == 5120 && a.otg[0].vactive == 1440 && a.otg[0].htotal == 5400 && a.otg[0].vtotal == 1485 &&
          a.otg[0].hfront == 88 && a.otg[0].hsync == 44 && a.otg[0].hpos && !a.otg[0].vpos, "timing lido");
    CHECK(!a.otg[1].on && !a.plane[1].on, "OTG1 desligado");
    CHECK(a.hpd_mask == 0x10, "HPD 0x%x", a.hpd_mask);
    CHECK(a.vsync_ok && a.refresh_mhz >= 59000 && a.refresh_mhz <= 61000, "taxa %u", a.refresh_mhz);
    /* GOP ocupa 5120*1440*4 = 28,1 MiB: a VRAM livre começa em 29 MiB e para 16 MiB antes do topo */
    CHECK(a.vram_lo == 29 * 16 && a.vram_hi == 240 * 16, "faixa de VRAM %u..%u", a.vram_lo, a.vram_hi);
    amdgpu_set_main(&a);
    CHECK(amdgpu_main() == &a, "main");

    for (int k = 0; k < 20; k++) {
        a.last_vblank_ms = 0;
        vline = (u32)(k * 97) % 1485;
        amdgpu_vblank_wait(&a);
        CHECK(vline >= 1482 || vline < 16, "vblank_wait parou na linha %u", vline);
    }
    /* chamada repetida na mesma composição (menos de 5 ms) não espera de novo */
    a.last_vblank_ms = now_ms;
    vline = 10;
    amdgpu_vblank_wait(&a);
    CHECK(vline == 10, "segunda espera na mesma composição andou para %u", vline);

    /* o rev_id >= 8 é Raven2 mesmo com o ID 15D8; registradores mortos recusam */
    boot_state(&m, 9);
    amdgpu_t b;
    CHECK(amdgpu_probe(&b, &d, 0xE0000000ull, 5120, 1440, 5120) && b.variant == AMD_RAVEN2 && !strcmp(b.rlc_name, "raven2_rlc.bin"), "Raven2");
    regs[R_RCC_DEV0_EPF0_STRAP0 / 4] = 0xFFFFFFFFu;
    CHECK(!amdgpu_probe(&b, &d, 0xE0000000ull, 5120, 1440, 5120), "registradores mortos");
    pci_cmd = 0;
    boot_state(&m, 1);
    CHECK(!amdgpu_probe(&b, &d, 0xE0000000ull, 5120, 1440, 5120), "decodificação desligada");
    pci_cmd = 6;
    d.device = 0x1636;
    CHECK(!amdgpu_probe(&b, &d, 0xE0000000ull, 5120, 1440, 5120), "Renoir não é Raven");
    /* GOP fora do plano ligado: pipe achado, mas sem match e sem voltar ao GOP */
    d.device = 0x15D8;
    boot_state(&m, 1);
    CHECK(amdgpu_probe(&b, &d, 0xE1000000ull, 5120, 1440, 5120) && b.pipe == 0 && !b.uefi_match, "sem match");
    CHECK(amdgpu_flip(&b, 0, 5120, 1440, 5120 * 4, MOS_GPU_FMT_BGRA8, false) == -MOS_ENOENT, "sem GOP para voltar");
}

static void test_flip_and_vram(void) {
    edid_mode_t m = mode_5120();
    boot_state(&m, 1);
    pci_dev_t d = {0};
    d.vendor = 0x1002; d.device = 0x15D8;
    amdgpu_t a;
    amdgpu_probe(&a, &d, 0xE0000000ull, 5120, 1440, 5120);
    u64 sz = 5120ull * 4 * 1440;
    u64 b1 = amdgpu_vram_alloc(&a, sz), b2 = amdgpu_vram_alloc(&a, sz);
    CHECK(b1 == 0xE0000000ull + (29ull << 20) && b2 == b1 + ((sz + 0xFFFF) & ~0xFFFFull), "alocação 0x%llx 0x%llx", b1, b2);
    CHECK(amdgpu_vram_owns(&a, b1, sz) && !amdgpu_vram_owns(&a, 0xE0000000ull, 4096), "dono");
    u64 before = amdgpu_vram_free_bytes(&a);

    CHECK(amdgpu_flip(&a, b1, 5120, 1440, 5120 * 4, MOS_GPU_FMT_BGRA8, true) == 0, "flip para b1");
    u64 mc1 = FB_MC + (b1 - 0xE0000000ull);
    CHECK(regs[R_DCSURF_PRIMARY_ADDR(0) / 4] == (u32)mc1 && regs[R_DCSURF_PRIMARY_ADDR_HI(0) / 4] == (u32)(mc1 >> 32), "endereço MC");
    CHECK(regs[R_DCSURF_EARLIEST_INUSE(0) / 4] == (u32)mc1 && !(regs[R_DCSURF_FLIP_CONTROL(0) / 4] & FLIP_PENDING), "troca aconteceu");
    CHECK(!locked, "trava solta");
    CHECK((regs[R_DCSURF_SURFACE_PITCH(0) / 4] & PITCH_MASK) == 5119 && (regs[R_DCSURF_FLIP_CONTROL(0) / 4] & FLIP_TYPE_IMMEDIATE) == 0, "pitch e tipo");
    CHECK((regs[R_HUBPRET_CONTROL(0) / 4] & 0xF) == 0x5, "crossbar preservou os outros bits");

    CHECK(amdgpu_flip(&a, b2, 5120, 1440, 5376 * 4, MOS_GPU_FMT_RGBA8, true) == 0, "flip RGBA com pitch maior");
    CHECK((regs[R_DCSURF_SURFACE_PITCH(0) / 4] & PITCH_MASK) == 5375, "pitch 5376");
    u32 xb = regs[R_HUBPRET_CONTROL(0) / 4];
    CHECK(((xb >> XBAR_CB_B_SHIFT) & 3) == 3 && ((xb >> XBAR_CR_R_SHIFT) & 3) == 2, "crossbar RGBA");
    CHECK(a.flips == 2 && a.flip_timeouts == 0, "contadores");

    CHECK(amdgpu_flip(&a, b1, 1920, 1080, 1920 * 4, MOS_GPU_FMT_BGRA8, false) == -MOS_EINVAL, "outro tamanho");
    CHECK(amdgpu_flip(&a, b1, 5120, 1440, 5120 * 4 + 4, MOS_GPU_FMT_BGRA8, false) == -MOS_EINVAL, "stride desalinhado");
    CHECK(amdgpu_flip(&a, b1 + 4, 5120, 1440, 5120 * 4, MOS_GPU_FMT_BGRA8, false) == -MOS_EINVAL, "endereço desalinhado");
    CHECK(amdgpu_flip(&a, 0xD0000000ull, 5120, 1440, 5120 * 4, MOS_GPU_FMT_BGRA8, false) == -MOS_EINVAL, "fora da VRAM");
    CHECK(amdgpu_flip(&a, 0xE0000000ull + (250ull << 20), 5120, 1440, 5120 * 4, MOS_GPU_FMT_BGRA8, false) == -MOS_EINVAL, "passa do fim");
    CHECK(amdgpu_flip(&a, b1, 5120, 1440, 5120 * 4, 9, false) == -MOS_EINVAL, "formato");

    CHECK(amdgpu_flip(&a, 0, 0, 0, 0, MOS_GPU_FMT_BGRA8, true) == 0 && regs[R_DCSURF_PRIMARY_ADDR(0) / 4] == (u32)FB_MC &&
          (regs[R_DCSURF_SURFACE_PITCH(0) / 4] & PITCH_MASK) == 5119, "volta ao GOP");
    xb = regs[R_HUBPRET_CONTROL(0) / 4];
    CHECK(((xb >> XBAR_CB_B_SHIFT) & 3) == 2 && ((xb >> XBAR_CR_R_SHIFT) & 3) == 3, "crossbar do GOP");

    amdgpu_vram_free(&a, b1, sz);
    CHECK(amdgpu_vram_free_bytes(&a) == before + ((sz + 0xFFFF) & ~0xFFFFull), "free");
    u64 b3 = amdgpu_vram_alloc(&a, 4096);
    CHECK(b3 == b1, "reuso do buraco");
    CHECK(!amdgpu_vram_alloc(&a, 1ull << 30), "pedido maior que a VRAM");
    amdgpu_vram_free(&a, 0xE0000000ull, 4096);                 /* GOP não é do alocador: ignora */
    int n = 0;
    while (amdgpu_vram_alloc(&a, 1u << 20)) n++;
    CHECK(n > 150 && amdgpu_vram_free_bytes(&a) < (1u << 20), "esgota a VRAM (%d MiB)", n);

    /* troca que nunca acontece (OTG parado): timeout sem travar */
    memset(armed_h, 0, sizeof armed_h);
    regs[R_OTG_V_BLANK_START_END(0) / 4] = 0x7FFF;              /* vblank nunca chega na simulação */
    int r = amdgpu_flip(&a, b2, 5120, 1440, 5120 * 4, MOS_GPU_FMT_BGRA8, true);
    CHECK(r == -MOS_ETIMEDOUT && a.flip_timeouts == 1, "timeout %d", r);
}

static u32 *scr;
static void paint(int sw, int x, int y, int w, int h, u32 v) {
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) scr[(usize)(y + j) * sw + x + i] = v;
}
/* o que o monitor mostra agora: o buffer cujo endereço MC está em uso */
static bool shown_equals(const amdgpu_t *a, int sw, int ox, int oy, int w, int h) {
    u64 mc = (u64)regs[R_DCSURF_EARLIEST_INUSE_HI(0) / 4] << 32 | regs[R_DCSURF_EARLIEST_INUSE(0) / 4];
    u8 *base = amd_host_phys(0xE0000000ull + (mc - a->fb_start));
    u32 stride = (regs[R_DCSURF_SURFACE_PITCH(0) / 4] & PITCH_MASK) * 4 + 4;
    for (int j = 0; j < h; j++)
        if (memcmp(base + (usize)j * stride, scr + (usize)(oy + j) * sw + ox, (usize)w * 4)) return false;
    return true;
}

static void test_present(void) {
    edid_mode_t m = mode_5120();
    boot_state(&m, 1);
    pci_dev_t d = {0};
    d.vendor = 0x1002; d.device = 0x15D8;
    amdgpu_t a;
    amdgpu_probe(&a, &d, 0xE0000000ull, 5120, 1440, 5120);
    CHECK(wc_lo == 0xE0000000ull + (29ull << 20) && wc_len == (211ull << 20), "WC 0x%llx +%llu", wc_lo, wc_len);
    /* tela de 7680x1440 com a saída 0 em x=2560 (arranjo com outro monitor à esquerda) */
    int sw = 7680, sh = 1440, ox = 2560;
    scr = calloc((usize)sw * sh, 4);
    paint(sw, 0, 0, sw, sh, 0x00336699);
    CHECK(amdgpu_present(&a, scr, sw, sh, ox, 0, 0, 0, sw, sh) && a.scr_on && a.scr_front == 1, "primeiro present");
    scan(1485);
    CHECK(shown_equals(&a, sw, ox, 0, 5120, 1440), "quadro 1 na tela");
    u32 vfree = (u32)(amdgpu_vram_free_bytes(&a) >> 20);
    CHECK(vfree == 211 - 29, "buffer de trás ocupa 29 MiB (%u livres)", vfree);
    /* série de áreas sujas: a tela tem de ficar sempre igual à superfície */
    srand(7);
    for (int k = 0; k < 40; k++) {
        int x = rand() % sw, y = rand() % sh, w = 1 + rand() % 900, h = 1 + rand() % 500;
        if (x + w > sw) w = sw - x;
        if (y + h > sh) h = sh - y;
        paint(sw, x, y, w, h, (u32)rand());
        CHECK(amdgpu_present(&a, scr, sw, sh, ox, 0, x, y, w, h), "present %d", k);
        scan(1485);
        CHECK(shown_equals(&a, sw, ox, 0, 5120, 1440), "tela diferente da superfície no quadro %d", k);
    }
    CHECK(a.flip_timeouts == 0 && a.presents >= 20, "presents %llu", a.presents);
    /* área só no outro monitor: nada muda nesta saída */
    u64 flips = a.flips;
    paint(sw, 10, 10, 100, 100, 0xFF);
    CHECK(amdgpu_present(&a, scr, sw, sh, ox, 0, 10, 10, 100, 100) && a.flips == flips, "fora da saída");
    /* superfície menor que a saída: recusa */
    CHECK(!amdgpu_present(&a, scr, 1920, 1080, 0, 0, 0, 0, 10, 10), "superfície menor");
    /* troca travada: desliga o buffer duplo, devolve o GOP com o conteúdo atual */
    regs[R_OTG_V_BLANK_START_END(0) / 4] = 0x7FFF;
    paint(sw, ox, 0, 64, 64, 0x123456);
    CHECK(amdgpu_present(&a, scr, sw, sh, ox, 0, ox, 0, 64, 64), "present antes do travamento");
    paint(sw, ox + 100, 0, 64, 64, 0x654321);
    bool r = amdgpu_present(&a, scr, sw, sh, ox, 0, ox + 100, 0, 64, 64);
    CHECK(!r && a.scr_failed && !a.scr_on && a.scr_front == 0, "desliga no travamento");
    CHECK(regs[R_DCSURF_PRIMARY_ADDR(0) / 4] == (u32)FB_MC, "voltou ao GOP");
    CHECK(amdgpu_vram_free_bytes(&a) == 211ull << 20, "buffer de trás devolvido");
    CHECK(!amdgpu_present(&a, scr, sw, sh, ox, 0, 0, 0, 1, 1), "fica desligado");
    /* o GOP tem o quadro que estava na frente (sem a última área, que fica com a cópia direta de quem chamou) */
    u8 *gop = amd_host_phys(0xE0000000ull);
    CHECK(!memcmp(gop, scr + ox, 64 * 4), "GOP recebeu a frente");
    free(scr);
}

static amdgpu_t eng;      /* grande (bitmap da GART): fora da pilha */
static bool set_ref(uc_sim_t *u, const char *name, int kind) {
    u64 n; amd_ucode_t uc;
    const u8 *fw = amd_host_fw(name, &n);
    if (!fw || amd_ucode_parse(fw, (usize)n, kind, &uc)) { printf("FALHOU: firmware %s\n", name); total++; return false; }
    u->ref = uc.ucode; u->refdw = uc.ucode_dwords; u->words = u->bad = u->ver = 0;
    return true;
}
static bool corrupt_me;   /* o teste troca o microcódigo esperado do ME: o que o driver carrega não confere */
static bool engines_up_rev(u32 rev_id) {
    edid_mode_t m = mode_5120();
    boot_state(&m, rev_id);
    pci_dev_t d = {0};
    d.vendor = 0x1002; d.device = 0x15D8;
    if (!amdgpu_probe(&eng, &d, 0xE0000000ull, 5120, 1440, 5120)) return false;
    const char *pre = amd_raven_fw_prefix(eng.variant);
    char nm[48];
    u64 n;
    snprintf(nm, sizeof nm, "%s_sdma.bin", pre);
    const u8 *fw = amd_host_fw(nm, &n);
    amd_ucode_t uc;
    if (!fw || amd_ucode_parse(fw, (usize)n, UC_KIND_SDMA, &uc)) return false;
    ucode_ref = uc.ucode; ucode_ref_dw = uc.ucode_dwords;
    amd_raven_rlc_name(eng.variant, pci_rev, 0x41E4D, nm, sizeof nm);
    set_ref(&uc_rlc, nm, UC_KIND_RLC);
    snprintf(nm, sizeof nm, "%s_pfp.bin", pre); set_ref(&uc_pfp, nm, UC_KIND_GFX);
    snprintf(nm, sizeof nm, "%s_ce.bin", pre);  set_ref(&uc_ce, nm, UC_KIND_GFX);
    snprintf(nm, sizeof nm, "%s_me.bin", pre);  set_ref(&uc_me, nm, UC_KIND_GFX);
    if (corrupt_me) uc_me.ref = uc_pfp.ref;
    {
        amd_ucode_t mu; u64 mn;
        snprintf(nm, sizeof nm, "%s_mec.bin", pre);
        const u8 *mf = amd_host_fw(nm, &mn);
        if (mf && !amd_ucode_parse(mf, (usize)mn, UC_KIND_GFX, &mu)) {
            uc_mec.start = mu.jt_offset; uc_mec.ref = mu.ucode + mu.jt_offset; uc_mec.refdw = mu.jt_size;
            mec_code = mu.ucode; mec_code_dw = mu.ucode_dwords;
        } else { printf("FALHOU: firmware %s\n", nm); total++; }
        uc_mec.words = uc_mec.bad = uc_mec.ver = 0;
    }
    hqd_bad_select = hqd_was_active = false; db_rings = db_ignored = 0;
    smu_sdma_on = false; ucode_loaded_ver = 0; sdma_faults = 0; ih_w = 0; ih_ovf = false; ih_pushed = 0; hpd_acks = 0;
    smu_gfx_on = gfxoff_off = in_preamble = clear_state_done = gfxoff_allowed = false;
    gc_off_writes = 0; gfxoff_enables = 0;
    memset(sh_cfg, 0xEE, sizeof sh_cfg); memset(sh_bases, 0xEE, sizeof sh_bases); memset(ctx_regs, 0, sizeof ctx_regs);
    srm_aram_words = scratch_writes = 0; cp_packets = cp_bad = cp_ibs = cp_faults = 0;
    msi_target = &eng;
    return amdgpu_engines_init(&eng);
}
static bool engines_up(void) { return engines_up_rev(1); }
static u8 *bar_ptr(u64 bar) { return amd_host_phys(bar); }

static void test_engines(void) {
    smu_alive = true; sdma_dead = false;
    CHECK(engines_up(), "motores");
    amdgpu_t *a = &eng;
    CHECK(a->smu_ok && a->smu_version == 0x41E4D && smu_sdma_on, "SMU");
    CHECK(a->gmc_ok && a->sdma_ok && ucode_loaded_ver == 0xA9 && !ucode_bad && ucode_words == ucode_ref_dw, "microcódigo %u/%u", ucode_words, ucode_ref_dw);
    CHECK(regs[R_MC_VM_SYS_APERTURE_LOW / 4] == (u32)(FB_MC >> 18) && regs[R_MC_VM_SYS_APERTURE_HIGH / 4] == (u32)((FB_MC + (256ull << 20) - 1) >> 18), "abertura do sistema");
    CHECK((regs[R_VM_CONTEXT0_CNTL / 4] & (CTX0_ENABLE | CTX0_DEPTH_MASK)) == CTX0_ENABLE, "contexto 0 com tabela plana");
    CHECK((regs[R_MC_VM_MX_L1_TLB_CNTL / 4] & (L1_TLB_ENABLE | L1_TLB_SYS_APERTURE_UNMAPPED | L1_TLB_MTYPE_MASK)) == (L1_TLB_ENABLE | L1_TLB_MTYPE_UC), "TLB L1");
    CHECK(a->gart_start == 0 && a->gart_size == (256ull << 20), "GART em MC 0");
    CHECK((regs[R_SDMA_GFX_RB_CNTL / 4] & (SDMA_RB_ENABLE | SDMA_RPTR_WB_ENABLE)) == (SDMA_RB_ENABLE | SDMA_RPTR_WB_ENABLE) &&
          ((regs[R_SDMA_GFX_RB_CNTL / 4] & SDMA_RB_SIZE_MASK) >> SDMA_RB_SIZE_SHIFT) == 14, "anel de 16 Ki dwords");
    CHECK((regs[R_SDMA_GB_ADDR_CONFIG / 4] & 0x0018773F) == 2 && (regs[R_SDMA_POWER_CNTL / 4] & 0xFC3FFFFF) == 0x40000051, "golden");
    CHECK((regs[R_SDMA_POWER_CNTL / 4] & (SDMA_PG_CNTL_ENABLE | SDMA_PG_HOLD_MASK | SDMA_PG_DURATION_MASK)) == (SDMA_PG_CNTL_ENABLE | (SDMA_POWER_CNTL_DEFAULT & (SDMA_PG_HOLD_MASK | SDMA_PG_DURATION_MASK))) &&
          (regs[R_SDMA_CNTL / 4] & SDMA_CNTL_CTXEMPTY_INT), "power gating do SDMA");
    CHECK(a->wb_cpu[8] == 0xDEADBEEF && sdma_faults == 0, "teste do anel");

    /* preenchimento e cópia na VRAM (9 MiB: passa do limite de 4 MiB por pacote) */
    u64 b1 = amdgpu_vram_alloc(a, 9u << 20), b2 = amdgpu_vram_alloc(a, 9u << 20);
    u32 s = amdgpu_sdma_fill(a, amdgpu_vram_to_mc(a, b1), 0x5A5A1234u, 9u << 20);
    CHECK(s && amdgpu_sdma_wait(a, s), "fill");
    u32 *p1 = (u32 *)bar_ptr(b1);
    CHECK(p1[0] == 0x5A5A1234u && p1[(9u << 18) - 1] == 0x5A5A1234u, "fill na VRAM");
    for (u32 i = 0; i < (9u << 18); i += 977) p1[i] = i * 2654435761u;
    s = amdgpu_sdma_copy(a, amdgpu_vram_to_mc(a, b2), amdgpu_vram_to_mc(a, b1), 9u << 20);
    CHECK(s && amdgpu_sdma_wait(a, s) && !memcmp(bar_ptr(b1), bar_ptr(b2), 9u << 20), "cópia VRAM -> VRAM");

    /* GART: RAM do sistema <-> VRAM */
    u8 *ram = aligned_alloc(4096, 64 * 4096);
    for (u32 i = 0; i < 64 * 4096; i++) ram[i] = (u8)(i * 7 + 3);
    u64 g = amdgpu_gart_map(a, (u64)(uintptr_t)ram, 64, true);
    CHECK(g == 4096 && !a->gart_cpu[0] && (a->gart_cpu[1] & AMD_PTE_ADDR_MASK) == (u64)(uintptr_t)ram, "GART map (a página 0 fica reservada)");
    u64 g2 = amdgpu_gart_map(a, (u64)(uintptr_t)ram, 1, false);
    CHECK(g2 == 65 * 4096ull, "segunda faixa logo depois");
    s = amdgpu_sdma_copy(a, amdgpu_vram_to_mc(a, b1), g, 64 * 4096);
    CHECK(s && amdgpu_sdma_wait(a, s) && !memcmp(bar_ptr(b1), ram, 64 * 4096), "RAM -> VRAM pela GART");
    memset(bar_ptr(b2), 0xC3, 64 * 4096);
    s = amdgpu_sdma_copy(a, g + 4096, amdgpu_vram_to_mc(a, b2), 8192);
    CHECK(s && amdgpu_sdma_wait(a, s) && ram[4096] == 0xC3 && ram[3 * 4096 - 1] == 0xC3 && ram[4095] != 0xC3 && ram[3 * 4096] != 0xC3, "VRAM -> RAM");
    u64 f0 = sdma_faults;
    s = amdgpu_sdma_copy(a, g2, amdgpu_vram_to_mc(a, b2), 16);
    CHECK(s && amdgpu_sdma_wait(a, s) && sdma_faults == f0 + 1, "página só de leitura recusa escrita");
    amdgpu_gart_unmap(a, g, 64);
    CHECK(a->gart_cpu[1] == 0 && a->gart_cpu[64] == 0 && a->gart_cpu[65], "unmap");
    f0 = sdma_faults;
    s = amdgpu_sdma_copy(a, amdgpu_vram_to_mc(a, b1), g, 4096);
    CHECK(s && amdgpu_sdma_wait(a, s) && sdma_faults == f0 + 1, "página desligada dá falta");
    CHECK(amdgpu_gart_map(a, (u64)(uintptr_t)ram, 64, true) == 4096, "faixa reaproveitada");

    /* retângulo: superfície do compositor (7680 px) -> buffer da tela (5120 px) */
    u32 sw = 7680, rows = 300, rw = 1000, ox = 2560 + 37, oy = 11;
    u32 *surf = aligned_alloc(4096, (usize)sw * 4 * 320);
    for (u32 i = 0; i < sw * 320; i++) surf[i] = i ^ 0xA5A5A5A5u;
    u32 sp = (u32)(((usize)sw * 4 * 320 + 4095) / 4096);
    u64 gs = amdgpu_gart_map(a, (u64)(uintptr_t)surf, sp, false);
    CHECK(gs, "GART da superfície");
    s = amdgpu_sdma_copy_rect(a, amdgpu_vram_to_mc(a, b2) + (u64)oy * 5120 * 4 + 37 * 4, 5120 * 4,
                              gs + (u64)oy * sw * 4 + ox * 4, sw * 4, rw * 4, rows);
    CHECK(s && amdgpu_sdma_wait(a, s), "copy_rect");
    bool ok = true;
    u32 *dst = (u32 *)bar_ptr(b2);
    for (u32 r = 0; r < rows && ok; r++) ok = !memcmp(dst + (usize)(oy + r) * 5120 + 37, surf + (usize)(oy + r) * sw + ox, rw * 4);
    CHECK(ok && dst[(usize)oy * 5120 + 36] == 0xC3C3C3C3u, "retângulo no lugar certo");

    /* volta do anel: 6000 pacotes (mais de 5 voltas de 16 Ki dwords) */
    u32 last = 0;
    for (u32 k = 0; k < 6000; k++) last = amdgpu_sdma_fill(a, amdgpu_vram_to_mc(a, b1) + (k % 1024) * 4, k, 4);
    CHECK(last && amdgpu_sdma_wait(a, last) && amdgpu_sdma_done(a) == last && p1[5999 % 1024] == 5999 && a->sdma_timeouts == 0, "anel deu a volta");
    CHECK(sdma_faults == 2, "só as duas faltas provocadas (%llu)", sdma_faults);
    CHECK(!amdgpu_sdma_fill(a, amdgpu_vram_to_mc(a, b1) + 2, 0, 4) && !amdgpu_sdma_fill(a, amdgpu_vram_to_mc(a, b1), 0, 6), "fill desalinhado");
    CHECK(!amdgpu_engines_init(a) || a->sdma_ok, "segunda chamada não refaz");

    /* IH: um trap por fence, HPD, estouro e fonte desconhecida */
    amdgpu_irq_process(a);
    u32 c = regs[R_IH_RB_CNTL / 4];
    CHECK(a->ih_ok && (c & (IH_RB_ENABLE | IH_ENABLE_INTR | IH_WPTR_WB_ENABLE | IH_MC_SNOOP | IH_RPTR_REARM)) ==
          (IH_RB_ENABLE | IH_ENABLE_INTR | IH_WPTR_WB_ENABLE | IH_MC_SNOOP | IH_RPTR_REARM) && (c & IH_MC_SPACE_MASK) == IH_MC_SPACE_BUS &&
          ((c & IH_RB_SIZE_MASK) >> IH_RB_SIZE_SHIFT) == 14, "IH ligado (0x%x)", c);
    CHECK(a->sdma_traps == a->sdma_seq && a->ih_overflows == 0 && a->ih_unknown == 0, "traps %llu de %u fences", a->sdma_traps, a->sdma_seq);
    CHECK(regs[R_IH_RB_RPTR / 4] == ih_w, "rptr do IH em dia");
    CHECK((regs[R_DC_HPD_INT_CONTROL(4) / 4] & (DC_HPD_INT_EN | DC_HPD_INT_POLARITY)) == DC_HPD_INT_EN &&
          (regs[R_DC_HPD_INT_CONTROL(1) / 4] & (DC_HPD_INT_EN | DC_HPD_INT_POLARITY)) == (DC_HPD_INT_EN | DC_HPD_INT_POLARITY), "polaridade inicial do HPD");
    amdgpu_hpd_take(a);
    hpd_set(1, true);
    CHECK(amdgpu_irq_process(a) == 0 && a->hpd_mask == 0x12 && amdgpu_hpd_take(a) == 0x2 && amdgpu_hpd_take(a) == 0, "monitor plugado no HPD 1");
    CHECK(!(regs[R_DC_HPD_INT_CONTROL(1) / 4] & DC_HPD_INT_POLARITY), "espera a saída agora");
    hpd_set(4, false);
    CHECK(amdgpu_irq_process(a) == 0 && a->hpd_mask == 0x2 && amdgpu_hpd_take(a) == 0x10 && a->hpd_events == 2, "HDMI desplugado");
    hpd_set(4, false);
    CHECK(amdgpu_irq_process(a) == 0, "sem mudança, sem interrupção");
    ih_push(0x1B, 77, 0);
    CHECK(amdgpu_irq_process(a) == 0 && a->ih_unknown == 1, "fonte desconhecida contada");
    msi_target = NULL;                                  /* interrupções paradas: o anel enche */
    for (int k = 0; k < 2500; k++) ih_push(IH_CLIENT_SDMA0, IH_SRC_SDMA_TRAP, 0);   /* anel de 2048 entradas */
    u64 t0 = a->sdma_traps;
    u32 got = amdgpu_irq_process(a);
    CHECK(a->ih_overflows == 1 && got > 0 && got < 2048 && a->sdma_traps == t0 + got && !(regs[R_IH_RB_WPTR / 4] & IH_WPTR_RB_OVERFLOW),
          "estouro do IH (%u entradas lidas)", got);
    msi_target = a;
    hpd_set(4, true);
    CHECK(a->hpd_mask == 0x12 && amdgpu_irq_process(a) == 0 && a->hpd_mask == 0x12 && regs[R_IH_RB_RPTR / 4] == ih_w, "anel volta ao normal depois do estouro");
    CHECK(hpd_acks >= 3 + 6, "ACKs do HPD");

    /* falhas: SMU mudo, SDMA morto */
    smu_alive = false;
    CHECK(!engines_up() && !eng.smu_ok && !eng.sdma_ok, "SMU mudo");
    smu_alive = true; sdma_dead = true;
    CHECK(!engines_up() && eng.gmc_ok && !eng.sdma_ok && (regs[R_SDMA_F32_CNTL / 4] & SDMA_F32_HALT), "SDMA morto fica parado");
    CHECK(!amdgpu_sdma_copy(&eng, 0, 0, 4), "sem SDMA não envia");
    sdma_dead = false;
    free(ram); free(surf);
}

static void test_gfx(void) {
    cp_dead = false;
    CHECK(engines_up(), "motores (Picasso)");
    amdgpu_t *a = &eng;
    CHECK(a->gfx_ok && a->gfxhub_ok, "GFX no ar");
    CHECK(smu_gfx_on && gfxoff_off, "SMU: PowerUpGfx e DisableGfxOff");
    /* GFXHUB igual à MMHUB */
    CHECK(regs[GFXHUB_VM(R_VM_CTX0_PT_BASE_LO) / 4] == regs[R_VM_CTX0_PT_BASE_LO / 4] && regs[R_VM_CTX0_PT_BASE_LO / 4] &&
          regs[GFXHUB_VM(R_VM_CTX0_PT_END_LO) / 4] == regs[R_VM_CTX0_PT_END_LO / 4] &&
          (regs[GFXHUB_VM(R_VM_CONTEXT0_CNTL) / 4] & CTX0_ENABLE) &&
          regs[GFXHUB_MC(R_MC_VM_SYS_APERTURE_LOW) / 4] == regs[R_MC_VM_SYS_APERTURE_LOW / 4], "GFXHUB com a GART");
    /* golden e constantes */
    CHECK(regs[R_GB_ADDR_CONFIG / 4] == 0x24000042 && a->gb_addr_config == 0x24000042 && regs[R_GRBM_CAM_DATA / 4] == 0x2544c382 &&
          regs[R_TCP_CHAN_STEER_LO / 4] == 0x3120 && regs[R_CP_SD_CNTL / 4] == 0x1FF && (regs[R_TD_CNTL / 4] & 0x01bd9f33) == 0x800,
          "golden do Picasso (GB_ADDR_CONFIG 0x%x)", regs[R_GB_ADDR_CONFIG / 4]);
    CHECK((regs[R_GRBM_CNTL / 4] & 0xFF) == 0xFF && regs[R_GRBM_GFX_CNTL / 4] == 0, "GRBM: timeout e VMID 0 de volta");
    bool shm = sh_cfg[0] == (SH_MEM_ALIGN_UNALIGNED | SH_MEM_RETRY_DISABLE) && sh_bases[0] == 0;
    for (int v = 1; v < 16; v++) shm = shm && sh_cfg[v] == sh_cfg[0] && sh_bases[v] == 0x20001000u;
    CHECK(shm, "SH_MEM de cada VMID");
    CHECK(a->cu_mask == 0xFF && a->cu_count == 8, "8 CUs (máscara 0x%x)", a->cu_mask);
    /* RLC */
    CHECK(uc_ok(&uc_rlc) && uc_ok(&uc_pfp) && uc_ok(&uc_ce) && uc_ok(&uc_me), "microcódigo do RLC %u/%u, PFP %u/%u, CE %u/%u, ME %u/%u",
          uc_rlc.words, uc_rlc.refdw, uc_pfp.words, uc_pfp.refdw, uc_ce.words, uc_ce.refdw, uc_me.words, uc_me.refdw);
    CHECK(a->rlc_fw_version == uc_rlc.ver && a->me_fw_version == uc_me.ver && a->pfp_fw_version && a->ce_fw_version, "versões");
    CHECK(regs[R_RLC_CNTL / 4] & RLC_ENABLE_F32, "RLC ligado");
    u32 csb[1024];
    u32 cdw = amdgpu_gfx_csb(csb);
    u64 cmc = ((u64)regs[R_RLC_CSIB_ADDR_HI / 4] << 32) | regs[R_RLC_CSIB_ADDR_LO / 4];
    const u32 *cpu = (const u32 *)mc_ptr_h(1, cmc, false);
    CHECK(cdw == 5 + (212 + 282 + 4 + 157 + 2 + 1 + 66 + 155) + 2 * 8 + 4 && regs[R_RLC_CSIB_LENGTH / 4] == cdw && cpu && !memcmp(cpu, csb, cdw * 4),
          "CSB de %u dwords na VRAM", cdw);
    CHECK(regs[R_RLC_LB_CNTL / 4] == 0x80000095u && regs[R_RLC_LB_PARAMS / 4] == 0x033F1006u && regs[R_RLC_LB_ALWAYS_ACTIVE_CU_MASK / 4] == 0xF &&
          regs[R_RLC_PG_ALWAYS_ON_CU_MASK / 4] == 0x3 && (regs[R_RLC_GPM_GENERAL_7 / 4] >> 16) == 0xC0, "LBPW e CUs sempre ligados");
    CHECK((regs[R_RLC_SPM_MC_CNTL / 4] & 0xF) == 0xF && regs[R_RLC_CGCG_CGLS_CTRL / 4] == 0, "SPM e clock gating");
    /* CP */
    u32 rb = regs[R_CP_RB0_CNTL / 4];
    CHECK((rb & 0x3F) == 13 && ((rb >> 8) & 0x3F) == 11 && (regs[R_CP_RB_DOORBELL_CONTROL / 4] & (CP_RB_DOORBELL_EN | 0x0FFFFFFCu)) == (CP_RB_DOORBELL_EN | 0x116u << 2) &&
          regs[R_CP_RB_DOORBELL_RANGE_LOWER / 4] == 0x116u << 2 && a->gfx_db && gfx_db_rings >= 2 &&
          regs[R_CP_MAX_CONTEXT / 4] == 7 && regs[R_CP_DEVICE_ID / 4] == 1 && regs[R_CP_MEC_CNTL / 4] == 0, "anel 0 configurado");
    CHECK(clear_state_done && ctx_regs[13] == 0x40004000u && regs[R_VGT_INDEX_TYPE / 4] == 0, "preâmbulo de clear state executado (%d 0x%x)", clear_state_done, ctx_regs[13]);
    CHECK(regs[R_SCRATCH_REG0 / 4] == 0xDEADBEEFu && scratch_writes == 1 && a->gwb_cpu[8] == 0xDEADBEEFu, "teste do anel e do IB");
    CHECK((regs[R_CP_INT_CNTL_RING0 / 4] & (CP_INT_TIME_STAMP | CP_INT_PRIV_REG | CP_INT_GUI_IDLE_ALL)) == (CP_INT_TIME_STAMP | CP_INT_PRIV_REG | CP_INT_GUI_IDLE_ALL), "interrupções do CP");
    CHECK(cp_bad == 0 && cp_faults == 0 && a->gfx_eops == a->gfx_seq, "sem erro (%llu ruins, %llu faltas, %llu EOPs de %u)", cp_bad, cp_faults, a->gfx_eops, a->gfx_seq);

    /* IB na RAM pela GART escrevendo em outra página de RAM */
    u32 *ibp = aligned_alloc(4096, 4096), *dst = aligned_alloc(4096, 4096);
    memset(dst, 0, 4096);
    u64 gi = amdgpu_gart_map(a, (u64)(uintptr_t)ibp, 1, false), gd = amdgpu_gart_map(a, (u64)(uintptr_t)dst, 1, true);
    CHECK(gi && gd, "GART do IB");
    u32 k = 0;
    for (u32 i = 0; i < 16; i++) {
        ibp[k++] = PM4_TYPE3(PM4_WRITE_DATA, 2 + 4);
        ibp[k++] = PM4_WD_DST_MEM | PM4_WR_CONFIRM;
        ibp[k++] = (u32)(gd + 16 * i); ibp[k++] = (u32)((gd + 16 * i) >> 32);
        for (u32 j = 0; j < 4; j++) ibp[k++] = i * 100 + j;
    }
    u32 s = amdgpu_gfx_submit(a, gi, k);
    CHECK(s && amdgpu_gfx_wait(a, s) && amdgpu_gfx_done(a) == s, "IB da GART");
    bool ok = true;
    for (u32 i = 0; i < 16; i++) for (u32 j = 0; j < 4; j++) ok = ok && dst[4 * i + j] == i * 100 + j;
    CHECK(ok, "64 dwords escritos na RAM pelo CP");
    /* IB na VRAM escrevendo na VRAM; o anel dá várias voltas (cada envio ocupa 256 dwords) */
    u64 vb = amdgpu_vram_alloc(a, 64 * 1024);
    u32 *vp = (u32 *)amd_host_phys(vb), last = 0;
    for (u32 n = 0; n < 300; n++) {
        u32 *ib = vp + 1024 + (n % 8) * 8;
        u64 to = amdgpu_vram_to_mc(a, vb) + 4 * (n % 512);
        ib[0] = PM4_TYPE3(PM4_WRITE_DATA, 3); ib[1] = PM4_WD_DST_MEM; ib[2] = (u32)to; ib[3] = (u32)(to >> 32); ib[4] = 0xA000 + n;
        last = amdgpu_gfx_submit(a, amdgpu_vram_to_mc(a, vb) + 4096 + (n % 8) * 32, 5);
        if (!last) break;
    }
    CHECK(last && amdgpu_gfx_wait(a, last) && vp[299] == 0xA000 + 299 && vp[0] == 0xA000 && a->gfx_timeouts == 0, "300 IBs, anel deu a volta");
    CHECK(a->gfx_eops == a->gfx_seq && a->gfx_ibs == 302 && cp_ibs == 302 + 1 && cp_bad == 0 && cp_faults == 0, "EOP por fence (%llu de %u)", a->gfx_eops, a->gfx_seq);
    CHECK(!amdgpu_gfx_submit(a, gi + 2, 4) && !amdgpu_gfx_submit(a, gi, 0), "IB desalinhado ou vazio recusado");
    /* página só de leitura: a escrita do CP vira falta (vai para a página de rascunho) */
    ibp[0] = PM4_TYPE3(PM4_WRITE_DATA, 3); ibp[1] = PM4_WD_DST_MEM; ibp[2] = (u32)gi; ibp[3] = (u32)(gi >> 32); ibp[4] = 1;
    s = amdgpu_gfx_submit(a, gi, 5);
    CHECK(s && amdgpu_gfx_wait(a, s) && cp_faults == 1 && ibp[4] == 1, "escrita em página só de leitura recusada");
    /* falta do CP chega pelo IH */
    ih_push(IH_CLIENT_GRBM_CP, IH_SRC_CP_PRIV_REG, 0);
    ih_push(IH_CLIENT_GRBM_CP, IH_SRC_CP_CTX_EMPTY, 0);
    CHECK(a->gfx_faults == 1 && a->gfx_idle_irqs == 1 && a->ih_unknown == 0, "falta e ocioso do CP no IH");
    amdgpu_gart_unmap(a, gi, 1); amdgpu_gart_unmap(a, gd, 1);
    free(ibp); free(dst);

    /* compute */
    CHECK(a->comp_ok && uc_ok(&uc_mec) && a->mec_fw_version == uc_mec.ver && !hqd_bad_select && hqd_was_active, "fila de compute no ar (MEC jt %u/%u)", uc_mec.words, uc_mec.refdw);
    CHECK(regs[R_RCC_DOORBELL_APER_EN / 4] & DOORBELL_APER_EN && regs[R_CP_MEC_DOORBELL_RANGE_UPPER / 4] == 0x450 &&
          (regs[R_CP_HQD_PQ_DOORBELL_CONTROL / 4] & (CP_HQD_DB_EN | CP_HQD_DB_OFFSET_MASK)) == (CP_HQD_DB_EN | 6u << 2) &&
          (regs[R_CP_HQD_PQ_CONTROL / 4] & 0x3F) == 13 && (regs[R_CP_HQD_PQ_CONTROL / 4] & (CP_HQD_PQ_PRIV_STATE | CP_HQD_PQ_KMD_QUEUE)) == (CP_HQD_PQ_PRIV_STATE | CP_HQD_PQ_KMD_QUEUE) &&
          (regs[R_CP_HQD_EOP_CONTROL / 4] & 0x3F) == 9 && regs[R_GRBM_GFX_CNTL / 4] == 0, "HQD e doorbell");
    const u32 *mq = (const u32 *)mc_ptr_h(1, a->mqd_mc, false);
    CHECK(mq && mq[MQD_header] == 0xC0310800u && mq[MQD_pq_base_lo] == regs[R_CP_HQD_PQ_BASE / 4] && mq[MQD_pq_control] == regs[R_CP_HQD_PQ_CONTROL / 4] &&
          mq[MQD_hqd_active] == 1 && ((u64)regs[R_CP_MQD_BASE_ADDR_HI / 4] << 32 | regs[R_CP_MQD_BASE_ADDR / 4]) == a->mqd_mc, "MQD na VRAM");
    CHECK(a->cwb_cpu[8] == 0xC0FFEE00u && a->comp_eops == a->comp_seq && a->comp_seq == 1 && db_ignored == 0, "IB de teste na fila de compute");
    u32 clast = 0;
    for (u32 n = 0; n < 300; n++) {
        u32 *ib = vp + 2048 + (n % 8) * 8;
        u64 to = amdgpu_vram_to_mc(a, vb) + 4 * (512 + n % 256);
        ib[0] = PM4_TYPE3(PM4_WRITE_DATA, 3); ib[1] = PM4_WD_DST_MEM; ib[2] = (u32)to; ib[3] = (u32)(to >> 32); ib[4] = 0xC000 + n;
        clast = amdgpu_compute_submit(a, amdgpu_vram_to_mc(a, vb) + 8192 + (n % 8) * 32, 5);
        if (!clast) break;
    }
    CHECK(clast && amdgpu_compute_wait(a, clast) && amdgpu_compute_done(a) == clast && vp[512 + 299 % 256] == 0xC000 + 299 && a->comp_timeouts == 0,
          "300 IBs de compute, anel deu a volta");
    CHECK(a->comp_eops == a->comp_seq && cp_bad == 0 && db_rings == 301, "EOP de compute pelo ME 1 (%llu de %u)", a->comp_eops, a->comp_seq);
    CHECK(!amdgpu_compute_submit(a, 2, 4), "IB desalinhado recusado");
    amdgpu_vram_free(a, vb, 64 * 1024);

    /* VM por processo */
    u64 vfree0 = amdgpu_vram_free_bytes(a);
    amd_vm_t *vm = amdgpu_vm_create(a);
    CHECK(vm && vm->vmid == 1 && (regs[R_VM_CTX_CNTL(1) / 4] & (CTX0_ENABLE | CTX0_DEPTH_MASK)) == (CTX0_ENABLE | CTX_DEPTH_4LEVEL) &&
          (regs[GFXHUB_VM(R_VM_CTX_CNTL(15)) / 4] & CTX_FAULT_DEFAULTS) == CTX_FAULT_DEFAULTS &&
          regs[GFXHUB_VM(R_VM_CTX_PT_BASE_LO(1)) / 4] == (u32)((CARVE + (vm->root - 0xE0000000ull)) | 1) &&
          regs[R_VM_CTX_PT_BASE_LO(1) / 4] == regs[GFXHUB_VM(R_VM_CTX_PT_BASE_LO(1)) / 4] && regs[R_VM_CTX_PT_END_LO(1) / 4] == 0xFFFFFFFFu, "VM 1 criada");
    CHECK((regs[R_VM_CTX0_PT_BASE_LO / 4] & 1) && (regs[R_VM_CTX0_PT_BASE_LO / 4] & ~0xFFFu) >= (u32)CARVE, "base da GART é física");
    u64 bo = amdgpu_vram_alloc(a, 8u << 20);
    u32 *ram2 = aligned_alloc(4096, 2 * 4096), *ibr = aligned_alloc(4096, 4096);
    memset(ram2, 0, 2 * 4096);
    const u64 VA_BO = 0x100000000ull, VA_RAM = 0x7FFF00000000ull, VA_IB = 0x200000ull;
    CHECK(!amdgpu_vm_map(a, vm, VA_BO, bo, 8u << 20, AMD_VM_VRAM | AMD_VM_WRITE) &&
          !amdgpu_vm_map(a, vm, VA_RAM, (u64)(uintptr_t)ram2, 8192, AMD_VM_WRITE) &&
          !amdgpu_vm_map(a, vm, VA_IB, (u64)(uintptr_t)ibr, 4096, AMD_VM_EXEC), "mapas da VM");
    CHECK(vm->pages == 2048 + 2 + 1 && vm->tables == 1 + (2 + 4) + 3 + 2, "tabelas de 4 níveis (%u)", vm->tables);
    u64 pte = 0;
    CHECK(amdgpu_vm_lookup(a, vm, VA_BO + 0x123456, &pte) == CARVE + (bo - 0xE0000000ull) + 0x123456 && !(pte & AMD_PTE_SYSTEM) && (pte & AMD_PTE_WRITE) &&
          amdgpu_vm_lookup(a, vm, VA_RAM + 4100, &pte) == (u64)(uintptr_t)ram2 + 4100 && (pte & (AMD_PTE_SYSTEM | AMD_PTE_SNOOPED)) == (AMD_PTE_SYSTEM | AMD_PTE_SNOOPED) &&
          !amdgpu_vm_lookup(a, vm, VA_BO + (8u << 20), NULL), "tradução pela CPU");
    CHECK(amdgpu_vm_map(a, vm, VA_BO + 4096, bo, 4096, AMD_VM_VRAM) == -MOS_EINVAL && amdgpu_vm_map(a, vm, 0x5000, bo + 1, 4096, AMD_VM_VRAM) == -MOS_EINVAL &&
          amdgpu_vm_map(a, vm, 0x5000, 0x1000, 4096, AMD_VM_VRAM) == -MOS_EINVAL && amdgpu_vm_map(a, vm, 1ull << 48, bo, 4096, AMD_VM_VRAM) == -MOS_EINVAL,
          "sobreposição, desalinhado, VRAM alheia e fora dos 48 bits recusados");
    k = 0;
    u64 targets[3] = {VA_BO + (8u << 20) - 4, VA_RAM + 4096 + 8, VA_BO};
    for (u32 i = 0; i < 3; i++) {
        ibr[k++] = PM4_TYPE3(PM4_WRITE_DATA, 3); ibr[k++] = PM4_WD_DST_MEM;
        ibr[k++] = (u32)targets[i]; ibr[k++] = (u32)(targets[i] >> 32); ibr[k++] = 0x51D0 + i;
    }
    u64 w0 = vm_walks;
    s = amdgpu_gfx_submit_vm(a, vm, VA_IB, k);
    u32 *bop = (u32 *)amd_host_phys(bo);
    CHECK(s && amdgpu_gfx_wait(a, s) && bop[(8u << 18) - 1] == 0x51D0 && ram2[1026] == 0x51D1 && bop[0] == 0x51D2 && vm_faults == 0 && vm_walks > w0,
          "IB do processo no anel GFX pela VM 1");
    ibr[4] = 0x7777; ibr[9] = 0x8888; ibr[14] = 0x9999;
    s = amdgpu_compute_submit_vm(a, vm, VA_IB, k);
    CHECK(s && amdgpu_compute_wait(a, s) && bop[0] == 0x9999 && ram2[1026] == 0x8888 && vm_faults == 0, "IB do processo na fila de compute");
    amdgpu_vm_unmap(a, vm, VA_RAM, 8192);
    ibr[9] = 0xBAD;
    s = amdgpu_gfx_submit_vm(a, vm, VA_IB, k);
    CHECK(s && amdgpu_gfx_wait(a, s) && ram2[1026] == 0x8888 && vm_faults == 1 && vm->pages == 2049, "página desmapeada vira falta");
    ibr[1] = PM4_WD_DST_MEM; ibr[2] = (u32)VA_IB; ibr[3] = (u32)(VA_IB >> 32);   /* escrever no próprio IB: só leitura */
    s = amdgpu_gfx_submit_vm(a, vm, VA_IB, 5);
    CHECK(s && amdgpu_gfx_wait(a, s) && vm_faults == 2, "IB só de leitura recusa escrita");
    CHECK(!amdgpu_gfx_submit_vm(a, NULL, VA_IB, 5) && !amdgpu_compute_submit_vm(a, NULL, VA_IB, 5), "sem VM não envia");
    amd_vm_t *more[15]; int nm = 0;
    while (nm < 15 && (more[nm] = amdgpu_vm_create(a))) nm++;
    CHECK(nm == 14 && more[13]->vmid == 15 && a->vmid_used == 0xFFFE, "15 VMIDs, o 16º recusado");
    for (int i = 0; i < nm; i++) amdgpu_vm_destroy(a, more[i]);
    amdgpu_vm_destroy(a, vm);
    amdgpu_vram_free(a, bo, 8u << 20);
    CHECK(a->vmid_used == 0 && regs[GFXHUB_VM(R_VM_CTX_PT_BASE_LO(1)) / 4] == 0 && amdgpu_vram_free_bytes(a) == vfree0, "VMs destruídas devolvem a VRAM");
    free(ram2); free(ibr);

    u64 free_ok = amdgpu_vram_free_bytes(a), mec_bytes = a->mec_bytes;
    /* MEC morto: só o compute fica de fora, e a VRAM dele volta */
    mec_dead = true;
    engines_up();
    CHECK(eng.gfx_ok && !eng.comp_ok && regs[R_CP_MEC_CNTL / 4] == CP_MEC_RESET_ALL && !eng.cring_cpu &&
          amdgpu_vram_free_bytes(&eng) == free_ok + 128 * 1024 + mec_bytes, "MEC morto: compute desligado, VRAM devolvida");
    CHECK(!amdgpu_compute_submit(&eng, 4096, 4), "sem compute não envia");
    mec_dead = false;

    /* Raven2: outro golden e a lista de save/restore do RLC v2.1 */
    pci_rev = 0x81;
    CHECK(engines_up_rev(8) && eng.variant == AMD_RAVEN2 && eng.gfx_ok, "Raven2 no ar");
    u64 n; amd_ucode_t r2;
    const u8 *fw = amd_host_fw("raven2_rlc.bin", &n);
    CHECK(fw && !amd_ucode_parse(fw, (usize)n, UC_KIND_RLC, &r2) && r2.hdr_minor >= 1, "RLC v2.1");
    CHECK(regs[R_GB_ADDR_CONFIG / 4] == 0x26013041 && regs[R_TCP_CHAN_STEER_LO / 4] == 0x10, "golden do Raven2");
    CHECK(srm_aram_words == r2.rlc_reg_list_dwords && (regs[R_RLC_SRM_CNTL / 4] & (SRM_ENABLE | SRM_AUTO_INCR)) == (SRM_ENABLE | SRM_AUTO_INCR) &&
          regs[R_RLC_SRM_INDEX_CNTL_ADDR_0 / 4], "lista de save/restore (%u de %u dwords)", srm_aram_words, r2.rlc_reg_list_dwords);
    CHECK(uc_ok(&uc_rlc) && uc_ok(&uc_me) && regs[R_SCRATCH_REG0 / 4] == 0xDEADBEEFu && cp_bad == 0, "Raven2: anel e IB");
    pci_rev = 0x75;

    /* CP morto: o GFX fica desligado, o resto segue e a VRAM volta */
    cp_dead = true;
    engines_up();
    CHECK(!eng.gfx_ok && eng.sdma_ok && (regs[R_CP_ME_CNTL / 4] & CP_ME_HALT_ALL) && !(regs[R_RLC_CNTL / 4] & RLC_ENABLE_F32) && !eng.gring_cpu,
          "CP morto: GFX desligado, SDMA no ar");
    CHECK(!amdgpu_gfx_submit(&eng, 0, 4), "sem GFX não envia");
    cp_dead = false;
    /* microcódigo corrompido: o CP não sobe */
    corrupt_me = true;
    CHECK(engines_up() && !eng.gfx_ok && uc_me.bad, "ME errado detectado");
    corrupt_me = false;
}

static void test_ddc(void) {
    edid_mode_t m = mode_5120();
    boot_state(&m, 1);
    pci_dev_t d = {0};
    d.vendor = 0x1002; d.device = 0x15D8;
    static amdgpu_t dd;
    CHECK(amdgpu_probe(&dd, &d, 0xE0000000ull, 5120, 1440, 5120), "probe para o DDC");
    regs[R_MICROSECOND_TIME_BASE_DIV / 4] = 48 | 2u << XTAL_REF_DIV_SHIFT;   /* 48 MHz / 2 */
    mon_init();
    CHECK(amdgpu_ddc_probe(&dd) == 1u << mon_ddc && dd.ddc_edid_len[mon_ddc] == 384 && !memcmp(dd.ddc_edid[mon_ddc], mon_edid, 384),
          "EDID ao vivo pela linha do HDMI (3 blocos, segmento E-DDC) (máscara 0x%x, %u bytes)", dd.ddc_mask, dd.ddc_edid_len[mon_ddc]);
    CHECK((regs[R_DC_I2C_DDC_SPEED(mon_ddc) / 4] >> 16) == 240 && !(regs[R_DC_I2C_DDC_SETUP(mon_ddc) / 4] & I2C_SETUP_ENABLE) &&
          ((regs[R_DC_I2C_ARBITRATION / 4] & I2C_ARB_STATUS_MASK) >> I2C_ARB_STATUS_SHIFT) == I2C_ST_IDLE, "100 kHz e motor devolvido");
    u8 buf[512];
    CHECK(amdgpu_ddc_read_edid(&dd, 0, buf, sizeof buf) == -MOS_ENXIO, "linha vazia: ninguém responde");
    CHECK(amdgpu_ddc_read_edid(&dd, mon_ddc, buf, 256) == 256, "para no limite do buffer");
    mon_edid[100] ^= 1;
    CHECK(amdgpu_ddc_read_edid(&dd, mon_ddc, buf, sizeof buf) == -MOS_EIO, "soma do bloco 0 errada");
    mon_edid[100] ^= 1;
    mon_edid[300] ^= 1;
    CHECK(amdgpu_ddc_read_edid(&dd, mon_ddc, buf, sizeof buf) == 256, "bloco de extensão ruim fica de fora");
    mon_edid[300] ^= 1;
    u16 cur = 0, max = 0;
    CHECK(!amdgpu_ddcci_get_vcp(&dd, mon_ddc, 0x10, &cur, &max) && cur == 50 && max == 100, "brilho pelo DDC/CI (%u/%u)", cur, max);
    CHECK(!amdgpu_ddcci_set_vcp(&dd, mon_ddc, 0x10, 80) && !amdgpu_ddcci_get_vcp(&dd, mon_ddc, 0x10, &cur, NULL) && cur == 80 && mon_bright == 80, "brilho ajustado");
    CHECK(amdgpu_ddcci_get_vcp(&dd, mon_ddc, 0xF0, &cur, NULL) == -MOS_ENOENT, "controle que o monitor não tem");
    CHECK(amdgpu_ddcci_get_vcp(&dd, 3, 0x10, &cur, NULL) == -MOS_ENXIO, "sem DDC/CI na linha");
    i2c_hw_owned = true;
    regs[R_DC_I2C_ARBITRATION / 4] = I2C_ST_USED_BY_HW << I2C_ARB_STATUS_SHIFT;
    CHECK(amdgpu_ddc_read_edid(&dd, mon_ddc, buf, sizeof buf) == -MOS_EBUSY, "motor com o firmware");
    i2c_hw_owned = false;
    regs[R_DC_I2C_ARBITRATION / 4] = 0;
    CHECK(dd.i2c_errors > 0 && dd.i2c_ops > 10, "contadores");
}

static void test_aux(void) {
    edid_mode_t m = mode_5120();
    boot_state(&m, 1);
    pci_dev_t d = {0};
    d.vendor = 0x1002; d.device = 0x15D8;
    static amdgpu_t ax;
    CHECK(amdgpu_probe(&ax, &d, 0xE0000000ull, 5120, 1440, 5120), "probe para o AUX");
    mon_init();
    memset(dpcd, 0, sizeof dpcd);
    dpcd[0] = 0x12; dpcd[1] = 0x14; dpcd[2] = 0x84;              /* DPCD 1.2, HBR2, 4 vias com enhanced framing */
    u8 cap[16];
    CHECK(amdgpu_dpcd_read(&ax, 0, 0, cap, 3) == 3 && cap[0] == 0x12 && cap[1] == 0x14 && cap[2] == 0x84, "DPCD: capacidades do sink");
    CHECK((regs[R_AUX_CONTROL(0) / 4] & AUX_EN) && ((regs[R_AUX_ARB_CONTROL(0) / 4] & AUX_ARB_STATUS_MASK) >> AUX_ARB_STATUS_SHIFT) == 0, "motor ligado e devolvido");
    u8 w[2] = {0x14, 0x84};
    CHECK(amdgpu_dpcd_write(&ax, 0, 0x100, w, 2) == 2 && dpcd[0x100] == 0x14 && dpcd[0x101] == 0x84, "DPCD: escrita (LINK_BW_SET, LANE_COUNT_SET)");
    u8 big[40];
    for (int k = 0; k < 40; k++) dpcd[0x200 + k] = (u8)(k * 3);
    CHECK(amdgpu_dpcd_read(&ax, 0, 0x200, big, 40) == 40 && big[39] == 117, "DPCD: leitura em 3 transações");
    aux_defers = 3;
    u64 rt = ax.aux_retries;
    CHECK(amdgpu_dpcd_read(&ax, 0, 0, cap, 1) == 1 && cap[0] == 0x12 && ax.aux_retries == rt + 3, "DEFER: repete até o ACK");
    aux_defers = 50;
    CHECK(amdgpu_dpcd_read(&ax, 0, 0, cap, 1) == -MOS_EAGAIN, "DEFER sem fim desiste");
    aux_defers = 0;
    u8 e[512];
    CHECK(amdgpu_aux_read_edid(&ax, 0, e, sizeof e) == 384 && !memcmp(e, mon_edid, 384), "EDID por I2C sobre AUX (3 blocos)");
    CHECK(amdgpu_dpcd_read(&ax, 2, 0, cap, 1) == -MOS_ENXIO, "AUX sem nada plugado");
    CHECK(amdgpu_dpcd_read(&ax, 0, 0xFFF, cap, 2) == -MOS_ENXIO, "NACK do sink");
    regs[R_AUX_ARB_CONTROL(1) / 4] = AUX_ST_DMCU << AUX_ARB_STATUS_SHIFT;
    CHECK(amdgpu_dpcd_read(&ax, 1, 0, cap, 1) == -MOS_EBUSY, "AUX com o DMCU");
    CHECK(amdgpu_aux_xfer(&ax, 0, AUX_CMD_NATIVE_READ, 0, cap, 17) == -MOS_EINVAL && amdgpu_aux_xfer(&ax, 6, AUX_CMD_NATIVE_READ, 0, cap, 1) == -MOS_EINVAL, "limites");
}

/* vblank e flip pela interrupção do display */
static void test_vblank_irq(void) {
    cp_dead = mec_dead = false;
    CHECK(engines_up(), "motores para o vblank por interrupção");
    amdgpu_t *a = &eng;
    CHECK((regs[R_OTG_GLOBAL_SYNC_STATUS(0) / 4] & OTG_VSTARTUP_INT_EN) && (regs[R_DCSURF_FLIP_INTERRUPT(0) / 4] & FLIP_INT_ENABLE), "VSTARTUP e FLIP ligados");
    u64 v0 = a->vblank_irqs;
    scan(1485 * 3);
    CHECK(a->vblank_irqs == v0 + 3 && vst_acks >= 3 && !(regs[R_OTG_GLOBAL_SYNC_STATUS(0) / 4] & OTG_VSTARTUP_OCCURRED), "um vblank por quadro, reconhecido (%llu)", a->vblank_irqs - v0);
    u32 s0 = a->vblank_seq;
    a->last_vblank_ms = 0;
    amdgpu_vblank_wait(a);
    CHECK(a->vblank_seq == s0 + 1, "a espera acorda na interrupção do vblank");
    u64 f0 = a->flip_irqs;
    u64 b = amdgpu_vram_alloc(a, 5120u * 1440 * 4);
    CHECK(b && !amdgpu_flip(a, b, 5120, 1440, 5120 * 4, MOS_GPU_FMT_BGRA8, true) && a->flip_irqs == f0 + 1 && amdgpu_flip_done(a), "flip avisa pela interrupção");
    amdgpu_flip(a, 0, 5120, 1440, 5120 * 4, MOS_GPU_FMT_BGRA8, true);
    amdgpu_vram_free(a, b, 5120u * 1440 * 4);
}

static void test_gfxoff(void) {
    cp_dead = mec_dead = false;
    CHECK(engines_up() && eng.gfx_ok && eng.comp_ok, "motores para o GFXOFF");
    amdgpu_t *a = &eng;
    CHECK(a->gfxoff_cap && a->gfxoff_on && gfxoff_allowed && gfxoff_enables == 1, "GFXOFF entregue ao SMU");
    u32 pg = regs[R_RLC_PG_CNTL / 4];
    CHECK((pg & (PG_GFX_POWER_GATING | PG_SMU_SLOWDOWN_PU | PG_SMU_SLOWDOWN_PD)) == (PG_GFX_POWER_GATING | PG_SMU_SLOWDOWN_PU | PG_SMU_SLOWDOWN_PD) && !(pg & PG_CP_PG_DISABLE),
          "power gating do GFX e do CP (0x%x)", pg);
    CHECK(regs[R_RLC_PG_DELAY / 4] == 0x40101010u && ((regs[R_RLC_AUTO_PG_CTRL / 4] & AUTO_PG_IDLE_THRESH_MASK) >> AUTO_PG_IDLE_THRESH_SHIFT) == 0x55F0 &&
          (regs[R_CP_RB_WPTR_POLL_CNTL / 4] >> 16) == 0x60 && (regs[R_PWR_MISC_CNTL_STATUS / 4] & PWR_GFX_RLC_CGPG_EN), "atrasos e controle do PG com o RLC");
    /* tabela de saltos: CE, PFP, ME, MEC em sequência */
    const u32 *jt = (const u32 *)mc_ptr_h(1, (u64)regs[R_RLC_JUMP_TABLE_RESTORE / 4] << 8, false);
    static const char *sfx[4] = {"picasso_ce.bin", "picasso_pfp.bin", "picasso_me.bin", "picasso_mec.bin"};
    bool ok = jt != NULL;
    u32 at = 0;
    for (int k = 0; k < 4 && ok; k++) {
        u64 n; amd_ucode_t u;
        const u8 *fw = amd_host_fw(sfx[k], &n);
        ok = fw && !amd_ucode_parse(fw, (usize)n, UC_KIND_GFX, &u) && !memcmp(jt + at, u.ucode + u.jt_offset, 4ull * u.jt_size);
        at += ok ? u.jt_size : 0;
    }
    CHECK(ok && at == a->cp_table_dw, "tabela de saltos do CP (%u dwords)", at);
    /* com o GFXOFF ligado: envios por doorbell e acesso ao GC só segurando o GC ligado */
    u64 w0 = gc_off_writes;
    amd_vm_t *vm = amdgpu_vm_create(a);
    u64 bo = amdgpu_vram_alloc(a, 64 * 1024);
    CHECK(vm && !amdgpu_vm_map(a, vm, 0x500000000ull, bo, 64 * 1024, AMD_VM_VRAM | AMD_VM_WRITE), "VM com o GFXOFF ligado");
    u32 *bp = (u32 *)amd_host_phys(bo);
    bp[0] = PM4_TYPE3(PM4_WRITE_DATA, 3); bp[1] = PM4_WD_DST_MEM; bp[2] = 0x100u; bp[3] = 5; bp[4] = 0x0FF;
    u32 s1 = amdgpu_gfx_submit_vm(a, vm, 0x500000000ull, 5), s2 = amdgpu_compute_submit_vm(a, vm, 0x500000000ull, 5);
    CHECK(s1 && s2 && amdgpu_gfx_wait(a, s1) && amdgpu_compute_wait(a, s2) && bp[64] == 0x0FF, "envios acordam o GC pelo doorbell");
    ih_vmid = vm->vmid; ih_push(IH_CLIENT_UTCL2, IH_SRC_VM_FAULT, 0); ih_vmid = 0;
    amdgpu_vm_destroy(a, vm);
    amdgpu_vram_free(a, bo, 64 * 1024);
    CHECK(gc_off_writes == w0 && a->gfxoff_wakes > 0 && a->gfxoff_hold == 0 && gfxoff_allowed, "nenhum acesso ao GC com ele dormindo (%llu)", gc_off_writes - w0);
    CHECK(!amdgpu_suspend(a, 3) && !a->gfxoff_on && !gfxoff_allowed && !(regs[R_RLC_PG_CNTL / 4] & PG_GFX_POWER_GATING), "S3 desliga o GFXOFF antes de parar o GC");
}

/* tela dividida como a do Picasso no 5120x1440: HUBP0 com x 0..2559 e HUBP3 com 2560..5119 */
static void test_split(void) {
    edid_mode_t m = mode_5120();
    boot_state(&m, 1);
    regs[R_DCSURF_PRI_VIEWPORT_DIM(0) / 4] = 2560 | 1440u << 16;
    regs[R_DCHUBP_CNTL(3) / 4] = 0;
    regs[R_DCSURF_SURFACE_CONFIG(3) / 4] = SURFACE_FMT_ARGB8888;
    regs[R_DCSURF_PRI_VIEWPORT_START(3) / 4] = 2560;
    regs[R_DCSURF_PRI_VIEWPORT_DIM(3) / 4] = 2560 | 1440u << 16;
    regs[R_DCSURF_SURFACE_PITCH(3) / 4] = 5120 - 1;
    regs[R_DCSURF_PRIMARY_ADDR(3) / 4] = (u32)FB_MC; regs[R_DCSURF_PRIMARY_ADDR_HI(3) / 4] = (u32)(FB_MC >> 32);
    regs[R_DCSURF_EARLIEST_INUSE(3) / 4] = (u32)FB_MC; regs[R_DCSURF_EARLIEST_INUSE_HI(3) / 4] = (u32)(FB_MC >> 32);
    regs[R_HUBPRET_CONTROL(3) / 4] = regs[R_HUBPRET_CONTROL(0) / 4];
    pci_dev_t d = {0};
    d.vendor = 0x1002; d.device = 0x15D8;
    static amdgpu_t sp;
    CHECK(amdgpu_probe(&sp, &d, 0xE0000000ull, 5120, 1440, 5120) && sp.pipe == 0 && sp.uefi_match && sp.split_mask == 0x9, "tela dividida em HUBP0 + HUBP3 (máscara 0x%x)", sp.split_mask);
    u64 b = amdgpu_vram_alloc(&sp, 5120u * 1440 * 4), mc = sp.fb_start + (b - sp.bar0);
    CHECK(b && !amdgpu_flip(&sp, b, 5120, 1440, 5120 * 4, MOS_GPU_FMT_BGRA8, true) && amdgpu_flip_done(&sp), "troca nos dois HUBPs");
    CHECK(regs[R_DCSURF_EARLIEST_INUSE(0) / 4] == (u32)mc && regs[R_DCSURF_EARLIEST_INUSE(3) / 4] == (u32)mc &&
          regs[R_DCSURF_PRI_VIEWPORT_START(3) / 4] == 2560, "as duas metades na superfície nova, cada uma com a sua janela");
    CHECK(!amdgpu_flip(&sp, 0, 5120, 1440, 5120 * 4, MOS_GPU_FMT_BGRA8, true) && regs[R_DCSURF_EARLIEST_INUSE(3) / 4] == (u32)FB_MC, "volta ao GOP nos dois");
    regs[R_DCSURF_PRI_VIEWPORT_START(3) / 4] = 2000;                 /* metades que não se encostam: não casa */
    static amdgpu_t sq;
    CHECK(amdgpu_probe(&sq, &d, 0xE0000000ull, 5120, 1440, 5120) && !sq.uefi_match && sq.split_mask == 0x1, "divisão estranha não casa (só o pipe 0, sem apresentar)");
}

static int pm_regs_count;
void amdgpu_pm_register(amdgpu_t *a) { (void)a; pm_regs_count++; }

/* repouso: s2idle desliga o sincronismo; S3 apaga os registradores (a VRAM fica) e os motores voltam */
static void test_pm(void) {
    cp_dead = mec_dead = false;
    pm_regs_count = 0;
    CHECK(engines_up() && eng.gfx_ok && eng.comp_ok && pm_regs_count == 1, "motores e registro no pm");
    amdgpu_t *a = &eng;
    CHECK(!amdgpu_suspend(a, 0) && !(regs[R_OTG_CONTROL(0) / 4] & OTG_MASTER_EN) && a->dpms_off, "s2idle: saída sem sincronismo");
    amdgpu_resume(a, 0);
    CHECK((regs[R_OTG_CONTROL(0) / 4] & OTG_MASTER_EN) && !a->dpms_off, "s2idle: saída de volta");
    /* trabalho antes do S3: uma VM com um BO na VRAM */
    amd_vm_t *vm = amdgpu_vm_create(a);
    u64 bo = amdgpu_vram_alloc(a, 64 * 1024);
    CHECK(vm && !amdgpu_vm_map(a, vm, 0x400000000ull, bo, 64 * 1024, AMD_VM_VRAM | AMD_VM_WRITE), "VM antes do S3");
    u32 *bp = (u32 *)amd_host_phys(bo);
    bp[0] = PM4_TYPE3(PM4_WRITE_DATA, 3); bp[1] = PM4_WD_DST_MEM; bp[2] = 0x100u; bp[3] = 4; bp[4] = 0x5E5E;
    u32 s1 = amdgpu_gfx_submit_vm(a, vm, 0x400000000ull, 5);
    CHECK(s1 && amdgpu_gfx_wait(a, s1) && bp[64] == 0x5E5E, "IB antes do S3");
    u32 gseq = a->gfx_seq, cseq = a->comp_seq, sseq = a->sdma_seq;
    CHECK(!amdgpu_suspend(a, 3) && !a->gfx_ok && !a->sdma_ok && (regs[R_CP_ME_CNTL / 4] & CP_ME_HALT_ALL) &&
          (regs[R_SDMA_F32_CNTL / 4] & SDMA_F32_HALT) && !(regs[R_IH_RB_CNTL / 4] & IH_RB_ENABLE), "S3: motores parados");
    CHECK(!amdgpu_gfx_submit_vm(a, vm, 0x400000000ull, 5) && !amdgpu_sdma_fill(a, 0, 0, 4), "nada é enviado durante o S3");
    /* o S3 apaga o chip: registradores de volta ao estado de boot, microcódigos e SMU zerados; a VRAM fica */
    edid_mode_t m = mode_5120();
    boot_state(&m, 1);
    smu_sdma_on = smu_gfx_on = false; ucode_loaded_ver = 0; ucode_words = 0;
    for (usize i = 0; i < ARRAY_LEN(ucs); i++) ucs[i]->words = ucs[i]->ver = ucs[i]->bad = 0;
    uc_mec.words = uc_mec.ver = uc_mec.bad = 0;
    amdgpu_resume(a, 3);
    CHECK(a->smu_ok && a->gmc_ok && a->ih_ok && a->sdma_ok && a->gfx_ok && a->comp_ok && smu_gfx_on && smu_sdma_on, "S3: motores de volta (smu %d gmc %d ih %d sdma %d gfx %d comp %d)", a->smu_ok, a->gmc_ok, a->ih_ok, a->sdma_ok, a->gfx_ok, a->comp_ok);
    CHECK(regs[GFXHUB_VM(R_VM_CTX_PT_BASE_LO(vm->vmid)) / 4] == (u32)((CARVE + (vm->root - 0xE0000000ull)) | 1) &&
          (regs[R_VM_CTX0_PT_BASE_LO / 4] & 1), "S3: tabelas da GART e da VM religadas");
    bp[4] = 0x6F6F;
    u32 s2 = amdgpu_gfx_submit_vm(a, vm, 0x400000000ull, 5);
    CHECK(s2 == gseq + 2 && amdgpu_gfx_wait(a, s2) && bp[64] == 0x6F6F, "S3: o processo continua de onde estava (fence %u)", s2);
    u32 s3 = amdgpu_compute_submit_vm(a, vm, 0x400000000ull, 5);
    u32 s4 = amdgpu_sdma_fill(a, amdgpu_vram_to_mc(a, bo) + 512, 0x77, 4);
    CHECK(s3 > cseq && amdgpu_compute_wait(a, s3) && s4 > sseq && amdgpu_sdma_wait(a, s4) && bp[128] == 0x77, "S3: compute e SDMA de volta");
    CHECK(a->s3_count == 1 && pm_regs_count == 1, "registro feito uma vez só");
    amdgpu_vm_destroy(a, vm);
    amdgpu_vram_free(a, bo, 64 * 1024);
}

/* fences da ABI: o que abi.c sinalizou */
static int fen_pid[64], fen_id[64], nfen;
void gpu_fence_from_irq(int pid, unsigned fence) { if (nfen < 64) { fen_pid[nfen] = pid; fen_id[nfen] = (int)fence; nfen++; } }

static void test_abi(void) {
    cp_dead = mec_dead = false;
    CHECK(engines_up() && eng.gfx_ok && eng.comp_ok, "motores para a ABI");
    amdgpu_t *a = &eng;
    amdgpu_set_main(a);
    CHECK(amdgpu_abi_available(), "ABI AMD disponível");
    u64 free0 = amdgpu_vram_free_bytes(a);
    const int PID = 7;
    u64 vbo = amdgpu_vram_alloc(a, 1u << 20);
    u8 *ram = aligned_alloc(4096, 64 * 1024);
    memset(ram, 0, 64 * 1024);
    u64 va_v = 0, va_r = 0;
    CHECK(!amdgpu_abi_bo_map(PID, vbo, 1u << 20, true, false, &va_v) && !amdgpu_abi_bo_map(PID, (u64)(uintptr_t)ram, 64 * 1024, false, false, &va_r) &&
          va_v && va_r > va_v + (1u << 20), "BOs com VA na VM do processo");
    amd_abi_bo_t bl[2] = {{va_v, 1u << 20, vbo, true}, {va_r, 64 * 1024, (u64)(uintptr_t)ram, false}};
    /* IB1 na RAM: escreve na VRAM e chama um IB2 (também na RAM) que escreve na RAM */
    u32 *ib = (u32 *)ram, *ib2 = (u32 *)(ram + 4096);
    u32 k = 0;
    ib[k++] = PM4_TYPE3(PM4C_SET_SH_REG, 1); ib[k++] = 0x48; ib[k++] = 0xABCD;
    ib[k++] = PM4_TYPE3(PM4_WRITE_DATA, 3); ib[k++] = PM4_WD_DST_MEM; ib[k++] = (u32)(va_v + 64); ib[k++] = (u32)((va_v + 64) >> 32); ib[k++] = 0x1111;
    ib[k++] = PM4_TYPE3(PM4_INDIRECT_BUFFER, 2); ib[k++] = (u32)(va_r + 4096); ib[k++] = (u32)((va_r + 4096) >> 32); ib[k++] = 5;
    ib2[0] = PM4_TYPE3(PM4_WRITE_DATA, 3); ib2[1] = PM4_WD_DST_MEM; ib2[2] = (u32)(va_r + 8192); ib2[3] = (u32)((va_r + 8192) >> 32); ib2[4] = 0x2222;
    amd_abi_ib_t il = {va_r, k};
    nfen = 0;
    u64 ticket = 0;
    u64 cpb = cp_bad, vf0 = vm_faults;
    int e = amdgpu_abi_submit(PID, 0, &il, 1, bl, 2, 41, &ticket);
    u32 *vv = (u32 *)amd_host_phys(vbo);
    CHECK(!e && ticket && amdgpu_abi_done(ticket), "lote enviado (%d)", e);
    CHECK(sh_regs[0x48] == 0xABCD && vv[16] == 0x1111 && ((u32 *)ram)[2048] == 0x2222 && cp_bad == cpb && vm_faults == vf0, "IB1 e IB2 rodaram pela VM do processo (sh 0x%x v 0x%x r 0x%x bad %llu->%llu faltas %llu)", sh_regs[0x48], vv[16], ((u32 *)ram)[2048], cpb, cp_bad, vm_faults);
    amdgpu_irq_process(a);
    CHECK(nfen == 1 && fen_pid[0] == PID && fen_id[0] == 41, "fence sinalizada pelo EOP");
    /* recusas: registrador privilegiado, IB fora dos BOs, IB2 aninhado, anel inválido */
    ib[1] = 0x400;
    CHECK(amdgpu_abi_submit(PID, 0, &il, 1, bl, 2, 42, NULL) == -MOS_EINVAL && nfen == 1, "SET_SH_REG fora da janela recusado, fence não sinaliza");
    ib[1] = 0x48;
    amd_abi_ib_t fora = {va_r + 64 * 1024 - 8, 4};
    CHECK(amdgpu_abi_submit(PID, 0, &fora, 1, bl, 2, 0, NULL) == -MOS_EINVAL, "IB passa do fim do BO");
    ib2[0] = PM4_TYPE3(PM4_INDIRECT_BUFFER, 2); ib2[1] = (u32)va_r; ib2[2] = (u32)(va_r >> 32); ib2[3] = 4;
    CHECK(amdgpu_abi_submit(PID, 0, &il, 1, bl, 2, 0, NULL) == -MOS_EINVAL, "IB2 aninhado recusado");
    ib2[0] = PM4_TYPE3(PM4_WRITE_DATA, 3); ib2[1] = PM4_WD_DST_MEM; ib2[2] = (u32)(va_r + 8192); ib2[3] = (u32)((va_r + 8192) >> 32); ib2[4] = 0x3333;
    CHECK(amdgpu_abi_submit(PID, 2, &il, 1, bl, 2, 0, NULL) == -MOS_EINVAL, "anel inexistente");
    /* compute, e a cópia: o processo troca o IB depois do envio e não muda o que a GPU leu */
    e = amdgpu_abi_submit(PID, 1, &il, 1, bl, 2, 43, &ticket);
    ib[7] = 0xBAD;
    amdgpu_irq_process(a);
    CHECK(!e && (ticket >> 32) == 1 && vv[16] == 0x1111 && ((u32 *)ram)[2048] == 0x3333 && nfen == 2 && fen_id[1] == 43, "lote na fila de compute");
    /* o pool dá a volta: 3000 lotes de ~1 KiB */
    u32 big[260];
    memcpy(ram + 16384, ib, 4 * k);
    for (u32 i = 0; i < 250; i++) ((u32 *)(ram + 16384))[k + i] = PM4_NOP_1DW;
    (void)big;
    amd_abi_ib_t il2 = {va_r + 16384, k + 250};
    ((u32 *)(ram + 16384))[7] = 0x4444;
    bool all = true;
    for (int i = 0; i < 3000 && all; i++) all = !amdgpu_abi_submit(PID, i & 1, &il2, 1, bl, 2, 0, NULL);
    CHECK(all && vv[16] == 0x4444 && a->gfx_timeouts == 0 && a->comp_timeouts == 0, "3000 lotes: o pool deu a volta");
    /* falha de página no VMID do processo: contexto perdido */
    u32 vmid = 0;
    for (u32 v = 1; v < 16; v++) if (a->vmid_used >> v & 1) vmid = v;
    ih_vmid = vmid;
    ih_push(IH_CLIENT_UTCL2, IH_SRC_VM_FAULT, 0);
    ih_vmid = 0;
    CHECK(amdgpu_abi_lost(PID) && a->vm_faults == 1 && amdgpu_abi_submit(PID, 0, &il, 1, bl, 2, 0, NULL) == -MOS_EIO &&
          (regs[GFXHUB_VM(R_VM_L2_PROT_FAULT_CNTL2) / 4] & PROT_FAULT_CLEAR_STATUS), "falha de página: contexto perdido e status limpo");
    CHECK((regs[GFXHUB_VM(R_VM_CTX_CNTL(3)) / 4] & CTX_FAULT_INTERRUPTS) == CTX_FAULT_INTERRUPTS, "faltas avisam pelo IH");
    /* saída do processo: VM e pool voltam; outro processo ganha VM nova */
    amdgpu_abi_proc_exit(PID);
    amdgpu_vram_free(a, vbo, 1u << 20);
    CHECK(!amdgpu_abi_lost(PID) && a->vmid_used == 0 && amdgpu_vram_free_bytes(a) == free0, "saída do processo devolve VM e VRAM");
    u64 va2 = 0;
    CHECK(!amdgpu_abi_bo_map(8, (u64)(uintptr_t)ram, 4096, false, true, &va2) && va2 == va_v, "processo novo começa do zero");
    amdgpu_abi_proc_exit(8);
    free(ram);
}

int main(int argc, char **argv) {
    verbose = argc > 1 && argv[1][0] == '-';
    setvbuf(stdout, NULL, _IONBF, 0);
    test_otg_roundtrip();
    test_probe_and_vsync();
    test_flip_and_vram();
    test_present();
    test_engines();
    test_gfx();
    test_abi();
    test_pm();
    test_ddc();
    test_aux();
    test_vblank_irq();
    test_gfxoff();
    test_split();
    printf("amdgpu-hw-host: %d/%d ok\n", pass, total);
    return pass == total ? 0 : 1;
}
