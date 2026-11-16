/* mOS — motores da GPU AMD família Raven: SMU 10 (mensagens), GMC da MMHUB 1.0 e da GFXHUB 1.0 (aberturas, GART do contexto 0)
 * e SDMA 4.1 (anel, cópia, preenchimento, fences). Ver amdgpu.h. Testado no host contra um SDMA simulado que
 * executa os pacotes (tools/amdgpu_hw_host.c).
 *
 * Memória dos motores (anel, writeback, tabela da GART, página de rascunho) fica na VRAM. A CPU escreve nela
 * pelo endereço físico do carve-out (MC_VM_FB_OFFSET), como o amdgpu faz em APU x86_64: assim não passa pelo
 * HDP e não precisa de flush. A GPU enxerga a mesma memória pelo endereço MC (fb_start + deslocamento). */
#include "amdgpu.h"
#include "regs.h"
#include "ucode.h"
#ifndef MOS_HOST_TEST
#include "syscall.h"
#include "vfs.h"
#endif

#ifdef MOS_HOST_TEST
#define RR(a, o)    ((void)(a), amd_host_rreg(o))
#define WR(a, o, v) ((void)(a), amd_host_wreg(o, v))
#define PHYS_PTR(p) ((u8 *)amd_host_phys(p))
#define CPU_RELAX() ((void)0)
#define WMB()       ((void)0)
#else
#define RR(a, o)    mmio_r32((a)->mmio, (o))
#define WR(a, o, v) mmio_w32((a)->mmio, (o), (v))
#define PHYS_PTR(p) ((u8 *)(usize)(p))
#define CPU_RELAX() __asm__ volatile("pause")
#define WMB()       __asm__ volatile("sfence" ::: "memory")
#endif

#define SPIN_MAX 2000000u

/* endereço MC e ponteiro da CPU (pelo carve-out) de um bloco da VRAM alocado no BAR0 */
static u64 vram_mc(const amdgpu_t *a, u64 bar_phys) { return a->fb_start + (bar_phys - a->bar0); }
static void *vram_cpu(const amdgpu_t *a, u64 bar_phys) { return PHYS_PTR(a->fb_phys + (bar_phys - a->bar0)); }

/* ------------------------------------------------------------ firmware */
const u8 *amdgpu_fw_load(const char *name, u64 *len) {
#ifdef MOS_HOST_TEST
    return amd_host_fw(name, len);
#else
    char path[96];
    snprintf(path, sizeof path, "/Sistema/Firmware/amdgpu/%s", name);
    vnode_t *v = vfs_lookup(path);
    if (!v || v->dir || !v->data || !v->size) return NULL;
    *len = v->size;
    return v->data;
#endif
}

/* ------------------------------------------------------------ SMU 10 */
static bool smu_wait(amdgpu_t *a, u32 *resp) {
    for (u32 n = 0; n < SPIN_MAX; n++) {
        u32 r = RR(a, R_MP1_C2PMSG_90);
        if (r) { *resp = r; return true; }
        CPU_RELAX();
    }
    return false;
}

int amdgpu_smu_send(amdgpu_t *a, u32 msg, u32 arg, u32 *out) {
    u32 r;
    if (!smu_wait(a, &r)) return -MOS_ETIMEDOUT;      /* mensagem anterior ainda em curso */
    WR(a, R_MP1_C2PMSG_90, 0);
    WR(a, R_MP1_C2PMSG_82, arg);
    WR(a, R_MP1_C2PMSG_66, msg);
    if (!smu_wait(a, &r)) { kprintf("amdgpu: SMU não respondeu à mensagem 0x%x\n", msg); return -MOS_ETIMEDOUT; }
    if (r != SMU_RESP_OK) { kprintf("amdgpu: SMU recusou a mensagem 0x%x (0x%x)\n", msg, r); return -MOS_EIO; }
    if (out) *out = RR(a, R_MP1_C2PMSG_82);
    return 0;
}

static bool smu_init(amdgpu_t *a) {
    if (amdgpu_smu_send(a, SMU_MSG_GET_VERSION, 0, &a->smu_version)) return false;
    if (amdgpu_smu_send(a, SMU_MSG_POWER_UP_SDMA, 0, NULL)) return false;
    kprintf("amdgpu: SMU %u.%u.%u, SDMA ligado\n", a->smu_version >> 16, (a->smu_version >> 8) & 0xFF, a->smu_version & 0xFF);
    return true;
}

/* ------------------------------------------------------------ GMC (MMHUB 1.0) */
/* As duas hubs (MMHUB: display, SDMA, IH; GFXHUB: CP e shaders) têm o mesmo conjunto de registradores em
 * lugares diferentes. hub 0 = MMHUB, 1 = GFXHUB. */
static u32 HV(int hub, u32 r) { return hub ? GFXHUB_VM(r) : r; }
static u32 HM(int hub, u32 r) { return hub ? GFXHUB_MC(r) : r; }

static void hub_invalidate(amdgpu_t *a, int hub, u32 vmid_mask) {
    /* semáforo do motor 17 só na MMHUB fora da Raven1; pedido para os VMIDs, espera o ACK, solta */
    bool sem = !hub && a->variant != AMD_RAVEN;
    if (sem) for (u32 n = 0; n < SPIN_MAX && !(RR(a, HV(hub, R_VM_INV_ENG17_SEM)) & 1); n++) CPU_RELAX();
    WR(a, HV(hub, R_VM_INV_ENG17_REQ), vmid_mask | INV_REQ_ALL_LEVELS);
    u32 n = 0;
    while ((RR(a, HV(hub, R_VM_INV_ENG17_ACK)) & vmid_mask) != vmid_mask && n < SPIN_MAX) { n++; CPU_RELAX(); }
    if (sem) WR(a, HV(hub, R_VM_INV_ENG17_SEM), 0);
    if (n >= SPIN_MAX) a->gmc_timeouts++;
}

void amdgpu_tlb_flush(amdgpu_t *a, u32 vmid_mask) {
    hub_invalidate(a, 0, vmid_mask);
    if (a->gfxhub_ok) hub_invalidate(a, 1, vmid_mask);
}
static void gmc_invalidate(amdgpu_t *a) { amdgpu_tlb_flush(a, INV_REQ_VMID0); }

/* contextos 1..15 de uma hub: tabelas de 4 níveis, faltas vão para a página de rascunho, sem retry */
static void hub_setup_vmids(amdgpu_t *a, int hub) {
    for (u32 n = 1; n < 16; n++) {
        u32 t = RR(a, HV(hub, R_VM_CTX_CNTL(n))) & ~0x007FFFFFu;
        WR(a, HV(hub, R_VM_CTX_CNTL(n)), t | CTX0_ENABLE | CTX_DEPTH_4LEVEL | CTX_FAULT_DEFAULTS);
        WR(a, HV(hub, R_VM_CTX_PT_START_LO(n)), 0);
        WR(a, HV(hub, R_VM_CTX_PT_START_HI(n)), 0);
        WR(a, HV(hub, R_VM_CTX_PT_END_LO(n)), (u32)(AMD_VM_MAX_PFN - 1));
        WR(a, HV(hub, R_VM_CTX_PT_END_HI(n)), (u32)((AMD_VM_MAX_PFN - 1) >> 32));
        WR(a, HV(hub, R_VM_CTX_PT_BASE_LO(n)), 0);     /* sem tabela até um processo pegar o VMID */
        WR(a, HV(hub, R_VM_CTX_PT_BASE_HI(n)), 0);
    }
}

/* aberturas, TLB L1, cache L2 e contexto 0 (a GART) de uma hub */
static void hub_setup(amdgpu_t *a, int hub, u64 pt, u64 scratch_phys) {
    WR(a, HV(hub, R_VM_CTX0_PT_BASE_LO), (u32)pt);
    WR(a, HV(hub, R_VM_CTX0_PT_BASE_HI), (u32)(pt >> 32));
    WR(a, HV(hub, R_VM_CTX0_PT_START_LO), (u32)(a->gart_start >> 12));
    WR(a, HV(hub, R_VM_CTX0_PT_START_HI), (u32)(a->gart_start >> 44));
    u64 gend = a->gart_start + a->gart_size - 1;
    WR(a, HV(hub, R_VM_CTX0_PT_END_LO), (u32)(gend >> 12));
    WR(a, HV(hub, R_VM_CTX0_PT_END_HI), (u32)(gend >> 44));

    /* abertura do sistema = a faixa da VRAM; AGP desligado (fundo acima do topo) */
    WR(a, HM(hub, R_MC_VM_AGP_BASE), 0);
    WR(a, HM(hub, R_MC_VM_AGP_BOT), 0xFFFFFF);
    WR(a, HM(hub, R_MC_VM_AGP_TOP), 0);
    WR(a, HM(hub, R_MC_VM_SYS_APERTURE_LOW), (u32)(a->fb_start >> 18));
    WR(a, HM(hub, R_MC_VM_SYS_APERTURE_HIGH), (u32)(a->variant == AMD_RAVEN2 ? (a->fb_top >> 18) + 1 : a->fb_top >> 18));
    WR(a, HM(hub, R_MC_VM_SYS_APERTURE_DEF_LSB), (u32)(scratch_phys >> 12));
    WR(a, HM(hub, R_MC_VM_SYS_APERTURE_DEF_MSB), (u32)(scratch_phys >> 44));
    WR(a, HV(hub, R_VM_L2_FAULT_DEF_LO), (u32)(a->dummy_page >> 12));
    WR(a, HV(hub, R_VM_L2_FAULT_DEF_HI), (u32)(a->dummy_page >> 44));

    u32 t = RR(a, HM(hub, R_MC_VM_MX_L1_TLB_CNTL));
    t |= L1_TLB_ENABLE | L1_TLB_ADV_DRIVER_MODEL | L1_TLB_ATC_EN;
    t = (t & ~(3u << 3)) | L1_TLB_SYS_ACCESS_MODE_3;
    t &= ~L1_TLB_SYS_APERTURE_UNMAPPED;
    t = (t & ~L1_TLB_MTYPE_MASK) | L1_TLB_MTYPE_UC;
    WR(a, HM(hub, R_MC_VM_MX_L1_TLB_CNTL), t);

    t = RR(a, HV(hub, R_VM_L2_CNTL));
    t |= L2_ENABLE_CACHE | L2_ENABLE_FRAG;
    t &= ~(L2_PDE0_TAG_GEN_MODE | L2_PDE_FAULT_CLASS | L2_IDENT_FRAG_MASK);
    t = (t & ~L2_CTX1_IDENT_MODE_MASK) | L2_CTX1_IDENT_MODE_1;
    WR(a, HV(hub, R_VM_L2_CNTL), t);
    WR(a, HV(hub, R_VM_L2_CNTL2), RR(a, HV(hub, R_VM_L2_CNTL2)) | L2_CNTL2_INV_L1_TLBS | L2_CNTL2_INV_L2_CACHE);
    t = (L2_CNTL3_DEFAULT & ~(L2_CNTL3_BANK_MASK | L2_CNTL3_BIGK_MASK)) | 9u | 6u << L2_CNTL3_BIGK_SHIFT;   /* sem translate_further */
    WR(a, HV(hub, R_VM_L2_CNTL3), t);
    WR(a, HV(hub, R_VM_L2_CNTL4), L2_CNTL4_DEFAULT & ~L2_CNTL4_PHYS_MASK);

    t = RR(a, HV(hub, R_VM_CONTEXT0_CNTL));
    t |= CTX0_ENABLE;
    t &= ~(CTX0_DEPTH_MASK | CTX0_BLOCK_MASK | CTX0_RETRY_FAULT);
    WR(a, HV(hub, R_VM_CONTEXT0_CNTL), t);

    WR(a, HV(hub, R_VM_L2_CTX1_IDENT_LOW_LO), 0xFFFFFFFFu);
    WR(a, HV(hub, R_VM_L2_CTX1_IDENT_LOW_HI), 0x0000000Fu);
    WR(a, HV(hub, R_VM_L2_CTX1_IDENT_HIGH_LO), 0);
    WR(a, HV(hub, R_VM_L2_CTX1_IDENT_HIGH_HI), 0);
    WR(a, HV(hub, R_VM_L2_CTX_IDENT_PHYS_LO), 0);
    WR(a, HV(hub, R_VM_L2_CTX_IDENT_PHYS_HI), 0);
    hub_setup_vmids(a, hub);
    hub_invalidate(a, hub, 0xFFFFu);
}

static bool gmc_init(amdgpu_t *a) {
    a->gart_size = (u64)AMD_GART_PAGES * 4096;
    a->gart_start = a->fb_start >= a->gart_size ? 0 : ((a->fb_top + 1 + 0xFFFFFFFFull) & ~0xFFFFFFFFull);
    u64 tbl = amdgpu_vram_alloc(a, (u64)AMD_GART_PAGES * 8);
    u64 scratch = amdgpu_vram_alloc(a, 4096);
    if (!tbl || !scratch) { kprintf("amdgpu: sem VRAM para a GART\n"); return false; }
#ifndef MOS_HOST_TEST
    vmm_map_wc(a->fb_phys + (tbl - a->bar0), (u64)AMD_GART_PAGES * 8);
    vmm_map_wc(a->fb_phys + (scratch - a->bar0), 4096);
    u64 dummy = pmm_alloc(1);
    if (!dummy) return false;
    memset((void *)(usize)dummy, 0, 4096);
#else
    u64 dummy = amd_host_ram_page();
#endif
    a->gart_table = tbl;
    a->gart_cpu = vram_cpu(a, tbl);
    memset(a->gart_cpu, 0, (usize)AMD_GART_PAGES * 8);
    memset(a->gart_used, 0, sizeof a->gart_used);
    a->dummy_page = dummy;

    /* a base da tabela é o endereço físico da VRAM com o bit de válido (amdgpu_gmc_pd_addr), não o MC */
    u64 pt = (a->fb_phys + (tbl - a->bar0)) | AMD_PDE_VALID;
    u64 sp = a->fb_phys + (scratch - a->bar0);
    hub_setup(a, 0, pt, sp);                 /* MMHUB */
    hub_setup(a, 1, pt, sp);                 /* GFXHUB: o CP enxerga a mesma GART */
    a->gfxhub_ok = true;
    kprintf("amdgpu: GART de %llu MiB em MC 0x%llx (tabela na VRAM, física 0x%llx)\n", a->gart_size >> 20, a->gart_start, pt & ~1ull);
    return true;
}

static bool gart_used(const amdgpu_t *a, u32 i) { return a->gart_used[i >> 3] >> (i & 7) & 1; }
static void gart_mark(amdgpu_t *a, u32 i, bool on) {
    if (on) a->gart_used[i >> 3] |= (u8)(1u << (i & 7));
    else a->gart_used[i >> 3] &= (u8)~(1u << (i & 7));
}

/* Liga n páginas de RAM (endereços físicos de 4 KiB, contíguos a partir de phys) a uma faixa nova da GART.
 * Devolve o endereço MC da faixa, ou 0. A página 0 da GART nunca é entregue: com a GART em MC 0, o endereço 0
 * seria ambíguo com a falha. */
u64 amdgpu_gart_map(amdgpu_t *a, u64 phys, u32 npages, bool writable) {
    if (!a || !a->gmc_ok || !npages || (phys & 0xFFF)) return 0;
    for (u32 s = 1; s + npages <= AMD_GART_PAGES;) {
        u32 k = 0;
        while (k < npages && !gart_used(a, s + k)) k++;
        if (k < npages) { s += k + 1; continue; }
        u64 flags = AMD_PTE_VALID | AMD_PTE_SYSTEM | AMD_PTE_SNOOPED | AMD_PTE_EXEC | AMD_PTE_READ | AMD_PTE_MTYPE_UC;
        if (writable) flags |= AMD_PTE_WRITE;
        for (u32 j = 0; j < npages; j++) {
            gart_mark(a, s + j, true);
            a->gart_cpu[s + j] = ((phys + (u64)j * 4096) & AMD_PTE_ADDR_MASK) | flags;
        }
        WMB();
        gmc_invalidate(a);
        return a->gart_start + (u64)s * 4096;
    }
    return 0;
}

void amdgpu_gart_unmap(amdgpu_t *a, u64 mc, u32 npages) {
    if (!a || !a->gmc_ok || mc < a->gart_start || mc >= a->gart_start + a->gart_size) return;
    u32 s = (u32)((mc - a->gart_start) >> 12);
    for (u32 j = 0; j < npages && s + j < AMD_GART_PAGES; j++) { a->gart_cpu[s + j] = 0; gart_mark(a, s + j, false); }
    WMB();
    gmc_invalidate(a);
}

/* ------------------------------------------------------------ SDMA 4.1 */
void amdgpu_golden(amdgpu_t *a, u32 reg, u32 and_mask, u32 or_mask) {
    u32 v = and_mask == 0xFFFFFFFFu ? or_mask : (RR(a, reg) & ~and_mask) | (or_mask & and_mask);
    WR(a, reg, v);
}
#define golden amdgpu_golden

static u32 ring_rptr(amdgpu_t *a) { return a->wb_cpu[0] >> 2; }   /* writeback em bytes */

static bool ring_room(amdgpu_t *a, u32 ndw) {
    for (u32 n = 0; n < SPIN_MAX; n++) {
        u32 used = (a->ring_wptr - ring_rptr(a)) & (a->ring_dw - 1);
        if (a->ring_dw - used > ndw + 8) return true;
        CPU_RELAX();
    }
    a->sdma_timeouts++;
    return false;
}

static void ring_put(amdgpu_t *a, u32 v) { a->ring_cpu[a->ring_wptr & (a->ring_dw - 1)] = v; a->ring_wptr++; }

static void ring_commit(amdgpu_t *a) {
    while (a->ring_wptr & 7) ring_put(a, SDMA_HDR(SDMA_OP_NOP, 0));   /* o SDMA busca de 8 em 8 dwords */
    WMB();
    u64 w = (u64)(a->ring_wptr & (a->ring_dw - 1)) << 2;
    WR(a, R_SDMA_GFX_RB_WPTR, (u32)w);
    WR(a, R_SDMA_GFX_RB_WPTR_HI, (u32)(w >> 32));
}

static u32 emit_fence(amdgpu_t *a) {
    u32 seq = ++a->sdma_seq;
    u64 addr = a->wb_mc + 16;
    ring_put(a, SDMA_HDR(SDMA_OP_FENCE, 0));
    ring_put(a, (u32)addr);
    ring_put(a, (u32)(addr >> 32));
    ring_put(a, seq);
    ring_put(a, SDMA_HDR(SDMA_OP_TRAP, 0));          /* interrupção: o IH recebe o trap (fonte 224) */
    ring_put(a, 0);
    return seq;
}

u32 amdgpu_sdma_done(amdgpu_t *a) { return a && a->sdma_ok ? a->wb_cpu[4] : 0; }

bool amdgpu_sdma_wait(amdgpu_t *a, u32 seq) {
    if (!a || !a->sdma_ok) return false;
    for (u32 n = 0; n < SPIN_MAX * 4; n++) {
        if ((i32)(a->wb_cpu[4] - seq) >= 0) return true;
        if (!a->msi_on && !(n & 1023)) amdgpu_irq_process(a);   /* sem MSI o anel do IH anda por aqui */
        CPU_RELAX();
    }
    a->sdma_timeouts++;
    return false;
}

u32 amdgpu_sdma_copy(amdgpu_t *a, u64 dst_mc, u64 src_mc, u64 bytes) {
    if (!a || !a->sdma_ok || !bytes) return 0;
    while (bytes) {
        u32 n = bytes > SDMA_COPY_MAX ? SDMA_COPY_MAX : (u32)bytes;
        if (!ring_room(a, 7 + 6)) return 0;
        ring_put(a, SDMA_HDR(SDMA_OP_COPY, 0));               /* COPY_LINEAR */
        ring_put(a, n - 1);
        ring_put(a, 0);                                        /* sem troca de endianness */
        ring_put(a, (u32)src_mc); ring_put(a, (u32)(src_mc >> 32));
        ring_put(a, (u32)dst_mc); ring_put(a, (u32)(dst_mc >> 32));
        src_mc += n; dst_mc += n; bytes -= n;
    }
    u32 seq = emit_fence(a);
    ring_commit(a);
    return seq;
}

/* cópia de retângulo linha a linha (passo de origem e destino em bytes) */
u32 amdgpu_sdma_copy_rect(amdgpu_t *a, u64 dst_mc, u32 dst_pitch, u64 src_mc, u32 src_pitch, u32 row_bytes, u32 rows) {
    if (!a || !a->sdma_ok || !row_bytes || !rows || row_bytes > SDMA_COPY_MAX) return 0;
    if (dst_pitch == row_bytes && src_pitch == row_bytes) return amdgpu_sdma_copy(a, dst_mc, src_mc, (u64)row_bytes * rows);
    for (u32 r = 0; r < rows; r++) {
        if (!ring_room(a, 7 + 6)) return 0;
        u64 s = src_mc + (u64)r * src_pitch, d = dst_mc + (u64)r * dst_pitch;
        ring_put(a, SDMA_HDR(SDMA_OP_COPY, 0));
        ring_put(a, row_bytes - 1);
        ring_put(a, 0);
        ring_put(a, (u32)s); ring_put(a, (u32)(s >> 32));
        ring_put(a, (u32)d); ring_put(a, (u32)(d >> 32));
        if ((r & 63) == 63) ring_commit(a);                    /* não deixa o anel encher sem avisar o motor */
    }
    u32 seq = emit_fence(a);
    ring_commit(a);
    return seq;
}

u32 amdgpu_sdma_fill(amdgpu_t *a, u64 dst_mc, u32 value, u64 bytes) {
    if (!a || !a->sdma_ok || !bytes || (bytes & 3) || (dst_mc & 3)) return 0;
    while (bytes) {
        u32 n = bytes > SDMA_COPY_MAX ? SDMA_COPY_MAX : (u32)bytes;
        if (!ring_room(a, 5 + 6)) return 0;
        ring_put(a, SDMA_HDR(SDMA_OP_CONST_FILL, 0));
        ring_put(a, (u32)dst_mc); ring_put(a, (u32)(dst_mc >> 32));
        ring_put(a, value);
        ring_put(a, n - 1);
        dst_mc += n; bytes -= n;
    }
    u32 seq = emit_fence(a);
    ring_commit(a);
    return seq;
}

u64 amdgpu_vram_to_mc(const amdgpu_t *a, u64 bar_phys) { return vram_mc(a, bar_phys); }
void *amdgpu_vram_cpu(const amdgpu_t *a, u64 bar_phys) { return vram_cpu(a, bar_phys); }

static bool sdma_init(amdgpu_t *a) {
    char name[32];
    snprintf(name, sizeof name, "%s_sdma.bin", amd_raven_fw_prefix(a->variant));
    u64 len = 0;
    const u8 *fw = amdgpu_fw_load(name, &len);
    amd_ucode_t uc;
    int e = fw ? amd_ucode_parse(fw, (usize)len, UC_KIND_SDMA, &uc) : -1;
    if (e) { kprintf("amdgpu: firmware %s %s\n", name, fw ? amd_ucode_error(e) : "não encontrado"); return false; }

    u64 ring = amdgpu_vram_alloc(a, AMD_SDMA_RING_BYTES), wb = amdgpu_vram_alloc(a, 4096);
    if (!ring || !wb) { kprintf("amdgpu: sem VRAM para o anel do SDMA\n"); return false; }
#ifndef MOS_HOST_TEST
    vmm_map_wc(a->fb_phys + (ring - a->bar0), AMD_SDMA_RING_BYTES);
    vmm_map_wc(a->fb_phys + (wb - a->bar0), 4096);
#endif
    a->ring_phys = ring; a->ring_cpu = vram_cpu(a, ring); a->ring_mc = vram_mc(a, ring);
    a->ring_dw = AMD_SDMA_RING_BYTES / 4; a->ring_wptr = 0;
    a->wb_phys = wb; a->wb_cpu = vram_cpu(a, wb); a->wb_mc = vram_mc(a, wb);
    memset((void *)a->ring_cpu, 0, AMD_SDMA_RING_BYTES);
    memset((void *)a->wb_cpu, 0, 4096);

    /* golden da família 4.1 e o GB_ADDR_CONFIG da variante */
    golden(a, R_SDMA_CHICKEN_BITS, 0xFE931F07u, 0x02831D07u);
    golden(a, R_SDMA_CLK_CTRL, 0xFFFFFFFFu, 0x3F000100u);
    golden(a, R_SDMA_GFX_IB_CNTL, 0x800F0111u, 0x00000100u);
    golden(a, R_SDMA_GFX_RB_WPTR_POLL_CNTL, 0xFFFFFFF7u, 0x00403000u);
    golden(a, R_SDMA_POWER_CNTL, 0xFC3FFFFFu, 0x40000051u);
    golden(a, R_SDMA_RLC0_IB_CNTL, 0x800F0111u, 0x00000100u);
    golden(a, R_SDMA_RLC0_RB_WPTR_POLL_CNTL, 0xFFFFFFF7u, 0x00403000u);
    golden(a, R_SDMA_RLC1_IB_CNTL, 0x800F0111u, 0x00000100u);
    golden(a, R_SDMA_RLC1_RB_WPTR_POLL_CNTL, 0xFFFFFFF7u, 0x00403000u);
    golden(a, R_SDMA_UTCL1_PAGE, 0x000003FFu, 0x000003E0u);
    golden(a, R_SDMA_UTCL1_WATERMK, 0xFC000000u, 0x00000000u);
    u32 gb = a->variant == AMD_RAVEN2 ? 0x00003001u : 0x00000002u;
    golden(a, R_SDMA_GB_ADDR_CONFIG, 0x0018773Fu, gb);
    golden(a, R_SDMA_GB_ADDR_CONFIG_READ, 0x0018773Fu, gb);

    /* microcódigo: motor parado, endereço 0, dwords, endereço = versão */
    WR(a, R_SDMA_F32_CNTL, RR(a, R_SDMA_F32_CNTL) | SDMA_F32_HALT);
    WR(a, R_SDMA_UCODE_ADDR, 0);
    for (u32 i = 0; i < uc.ucode_dwords; i++) WR(a, R_SDMA_UCODE_DATA, amd_le32((const u8 *)(uc.ucode + i)));
    WR(a, R_SDMA_UCODE_ADDR, uc.ucode_version);
    WR(a, R_SDMA_F32_CNTL, RR(a, R_SDMA_F32_CNTL) & ~SDMA_F32_HALT);

    /* anel GFX */
    WR(a, R_SDMA_SEM_WAIT_FAIL_TIMER, 0);
    u32 order = 0;
    while ((1u << order) < a->ring_dw) order++;
    u32 rb = (RR(a, R_SDMA_GFX_RB_CNTL) & ~(SDMA_RB_SIZE_MASK | SDMA_RB_ENABLE)) | order << SDMA_RB_SIZE_SHIFT;
    WR(a, R_SDMA_GFX_RB_CNTL, rb);
    WR(a, R_SDMA_GFX_RB_RPTR, 0); WR(a, R_SDMA_GFX_RB_RPTR_HI, 0);
    WR(a, R_SDMA_GFX_RB_WPTR, 0); WR(a, R_SDMA_GFX_RB_WPTR_HI, 0);
    WR(a, R_SDMA_GFX_RB_RPTR_ADDR_HI, (u32)(a->wb_mc >> 32));
    WR(a, R_SDMA_GFX_RB_RPTR_ADDR_LO, (u32)a->wb_mc & 0xFFFFFFFCu);
    rb |= SDMA_RPTR_WB_ENABLE;
    WR(a, R_SDMA_GFX_RB_BASE, (u32)(a->ring_mc >> 8));
    WR(a, R_SDMA_GFX_RB_BASE_HI, (u32)(a->ring_mc >> 40));
    WR(a, R_SDMA_GFX_MINOR_PTR_UPDATE, 1);
    WR(a, R_SDMA_GFX_DOORBELL, RR(a, R_SDMA_GFX_DOORBELL) & ~SDMA_DOORBELL_ENABLE);   /* WPTR por registrador */
    WR(a, R_SDMA_GFX_RB_WPTR, 0); WR(a, R_SDMA_GFX_RB_WPTR_HI, 0);
    WR(a, R_SDMA_GFX_MINOR_PTR_UPDATE, 0);
    WR(a, R_SDMA_GFX_RB_WPTR_POLL_CNTL, RR(a, R_SDMA_GFX_RB_WPTR_POLL_CNTL) & ~SDMA_WPTR_POLL_F32);
    WR(a, R_SDMA_GFX_RB_CNTL, rb | SDMA_RB_ENABLE);
    WR(a, R_SDMA_GFX_IB_CNTL, RR(a, R_SDMA_GFX_IB_CNTL) | SDMA_IB_ENABLE);
    WR(a, R_SDMA_CNTL, RR(a, R_SDMA_CNTL) | SDMA_CNTL_UTC_L1);
    WR(a, R_SDMA_F32_CNTL, RR(a, R_SDMA_F32_CNTL) & ~SDMA_F32_HALT);
    a->sdma_ok = true;
    a->sdma_fw_version = uc.ucode_version;

    /* teste do anel: WRITE de 0xDEADBEEF na página de writeback e a fence */
    a->wb_cpu[8] = 0xCAFEDEAD;
    WMB();
    u64 t = a->wb_mc + 32;
    ring_put(a, SDMA_HDR(SDMA_OP_WRITE, 0));
    ring_put(a, (u32)t); ring_put(a, (u32)(t >> 32));
    ring_put(a, 0);                                            /* count - 1 = 0: um dword */
    ring_put(a, 0xDEADBEEFu);
    u32 seq = emit_fence(a);
    ring_commit(a);
    if (!amdgpu_sdma_wait(a, seq) || a->wb_cpu[8] != 0xDEADBEEFu) {
        kprintf("amdgpu: SDMA não executou o anel (writeback 0x%x, fence %u de %u)\n", a->wb_cpu[8], a->wb_cpu[4], seq);
        a->sdma_ok = false;
        WR(a, R_SDMA_F32_CNTL, RR(a, R_SDMA_F32_CNTL) | SDMA_F32_HALT);
        return false;
    }
    kprintf("amdgpu: SDMA 4.1 no ar (firmware 0x%x, anel de %u KiB na VRAM)\n", uc.ucode_version, AMD_SDMA_RING_BYTES / 1024);
    return true;
}

/* ------------------------------------------------------------ entrada: depois do VFS (firmware) */
bool amdgpu_engines_init(amdgpu_t *a) {
    if (!a || !a->ready || a->engines_tried) return a && a->sdma_ok;
    a->engines_tried = true;
    a->smu_ok = smu_init(a);
    if (!a->smu_ok) { kprintf("amdgpu: sem SMU; motores da GPU ficam desligados\n"); return false; }
    a->gmc_ok = gmc_init(a);
    if (!a->gmc_ok) return false;
    if (!amdgpu_ih_init(a)) kprintf("amdgpu: sem IH; fences seguem por polling\n");
    bool sdma = sdma_init(a);
    a->gfx_ok = amdgpu_gfx_init(a);          /* motor 3D: independe do SDMA */
    return sdma;
}
