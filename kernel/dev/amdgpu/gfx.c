/* mOS — motor 3D da GPU AMD família Raven (GC 9.1 / 9.2.2): golden registers, constantes por VMID, RLC
 * (microcódigo, clear state, save/restore do Raven2, balanceamento de carga), CP gráfico (PFP, CE, ME) e o
 * anel GFX 0 com fences RELEASE_MEM e IBs. Ver amdgpu.h. Testado no host contra um CP simulado que executa
 * os pacotes PM4 e confere o microcódigo real (tools/amdgpu_hw_host.c).
 *
 * A ordem segue a do amdgpu (gfx_v9_0.c, licença MIT): golden -> constantes -> RLC (para, CSB, save/restore,
 * microcódigo, LBPW, liga) -> CP (microcódigo, anel, preâmbulo de clear state) -> teste do anel por registrador
 * -> teste de IB por memória. A família Raven carrega o microcódigo direto nos registradores (sem PSP).
 * O anel, o writeback e o CSB ficam na VRAM; a CPU escreve pelo carve-out e o CP lê pela GFXHUB (VMID 0). */
#include "amdgpu.h"
#include "regs.h"
#include "ucode.h"
#include "clearstate_gfx9.h"
#ifndef MOS_HOST_TEST
#include "syscall.h"
#endif

#ifdef MOS_HOST_TEST
#define RR(a, o)    ((void)(a), amd_host_rreg(o))
#define WR(a, o, v) ((void)(a), amd_host_wreg(o, v))
#define CPU_RELAX() ((void)0)
#define WMB()       ((void)0)
#define UDELAY(us)  ((void)(us))
#else
#define RR(a, o)    mmio_r32((a)->mmio, (o))
#define WR(a, o, v) mmio_w32((a)->mmio, (o), (v))
#define CPU_RELAX() __asm__ volatile("pause")
#define WMB()       __asm__ volatile("sfence" ::: "memory")
#define UDELAY(us)  do { for (u32 _i = 0; _i < (u32)(us); _i++) io_wait(); } while (0)   /* ~1 µs por acesso à porta 0x80 */
#endif

#define SPIN_MAX     2000000u
#define USEC_TIMEOUT 100000u

/* ------------------------------------------------------------ golden registers (gfx_v9_0.c) */
typedef struct { u32 reg, and_mask, or_mask; } golden_t;

static const golden_t gc_9_1[] = {
    {R_CB_HW_CONTROL, 0xfffdf3cf, 0x00014104},
    {R_CPC_UTCL1_CNTL, 0x08000000, 0x08000080},
    {R_CPF_UTCL1_CNTL, 0x08000000, 0x08000080},
    {R_CPG_UTCL1_CNTL, 0x08000000, 0x08000080},
    {R_DB_DEBUG2, 0xf00fffff, 0x00000420},
    {R_GB_GPU_ID, 0x0000000f, 0x00000000},
    {R_IA_UTCL1_CNTL, 0x08000000, 0x08000080},
    {R_PA_SC_BINNER_EVENT_CNTL_3, 0x00000003, 0x82400024},
    {R_PA_SC_ENHANCE, 0x3fffffff, 0x00000001},
    {R_PA_SC_LINE_STIPPLE_STATE, 0x0000ff0f, 0x00000000},
    {R_RLC_GPM_UTCL1_CNTL_0, 0x08000000, 0x08000080},
    {R_RLC_GPM_UTCL1_CNTL_1, 0x08000000, 0x08000080},
    {R_RLC_GPM_UTCL1_CNTL_2, 0x08000000, 0x08000080},
    {R_RLC_PREWALKER_UTCL1_CNTL, 0x08000000, 0x08000080},
    {R_RLC_SPM_UTCL1_CNTL, 0x08000000, 0x08000080},
    {R_TA_CNTL_AUX, 0xfffffeef, 0x010b0000},
    {R_TCP_CHAN_STEER_HI, 0xffffffff, 0x00000000},
    {R_TCP_CHAN_STEER_LO, 0xffffffff, 0x00003120},
    {R_VGT_CACHE_INVALIDATION, 0x3fff3af3, 0x19200000},
    {R_VGT_GS_MAX_WAVE_ID, 0x00000fff, 0x000000ff},
    {R_WD_UTCL1_CNTL, 0x08000000, 0x08000080},
    {R_CP_MEC1_F32_INT_DIS, 0x00000800, 0x00000800},
    {R_CP_MEC2_F32_INT_DIS, 0x00000800, 0x00000800},
    {R_CP_DEBUG, 0x00008000, 0x00008000},
};
static const golden_t gc_9_1_rv1[] = {          /* Raven e Picasso */
    {R_CB_HW_CONTROL_3, 0x30000000, 0x10000000},
    {R_GB_ADDR_CONFIG, 0xffff77ff, 0x24000042},
    {R_GB_ADDR_CONFIG_READ, 0xffff77ff, 0x24000042},
    {R_PA_SC_ENHANCE_1, 0xffffffff, 0x04048000},
    {R_PA_SC_MODE_CNTL_1, 0x06000000, 0x06000000},
    {R_RMI_UTCL1_CNTL2, 0x00030000, 0x00020000},
    {R_TD_CNTL, 0x01bd9f33, 0x00000800},
};
static const golden_t gc_9_1_rv2[] = {
    {R_CB_DCC_CONFIG, 0xff7fffff, 0x04000000},
    {R_CB_HW_CONTROL, 0xfffdf3cf, 0x00014104},
    {R_CB_HW_CONTROL_2, 0xff7fffff, 0x0a000000},
    {R_CPC_UTCL1_CNTL, 0x7f0fffff, 0x08000080},
    {R_CPF_UTCL1_CNTL, 0xff8fffff, 0x08000080},
    {R_CPG_UTCL1_CNTL, 0x7f8fffff, 0x08000080},
    {R_GB_ADDR_CONFIG, 0xffff77ff, 0x26013041},
    {R_GB_ADDR_CONFIG_READ, 0xffff77ff, 0x26013041},
    {R_IA_UTCL1_CNTL, 0x3f8fffff, 0x08000080},
    {R_PA_SC_ENHANCE_1, 0xffffffff, 0x04040000},
    {R_RLC_GPM_UTCL1_CNTL_0, 0xff0fffff, 0x08000080},
    {R_RLC_GPM_UTCL1_CNTL_1, 0xff0fffff, 0x08000080},
    {R_RLC_GPM_UTCL1_CNTL_2, 0xff0fffff, 0x08000080},
    {R_RLC_PREWALKER_UTCL1_CNTL, 0xff0fffff, 0x08000080},
    {R_RLC_SPM_UTCL1_CNTL, 0xff0fffff, 0x08000080},
    {R_TCP_CHAN_STEER_HI, 0xffffffff, 0x00000000},
    {R_TCP_CHAN_STEER_LO, 0xffffffff, 0x00000010},
    {R_TD_CNTL, 0x01bd9f33, 0x01000000},
    {R_WD_UTCL1_CNTL, 0x3f8fffff, 0x08000080},
};
static const golden_t gc_9_x_common[] = {
    {R_CP_SD_CNTL, 0xffffffff, 0x000001ff},
    {R_GRBM_CAM_INDEX, 0xffffffff, 0x00000000},
    {R_GRBM_CAM_DATA, 0xffffffff, 0x2544c382},
};

static void program(amdgpu_t *a, const golden_t *g, usize n) {
    for (usize i = 0; i < n; i++) amdgpu_golden(a, g[i].reg, g[i].and_mask, g[i].or_mask);
}

/* ------------------------------------------------------------ preâmbulo de clear state */
u32 amdgpu_gfx_csb(u32 *buf) {
    u32 n = 0;
#define PUT(v) do { if (buf) buf[n] = (v); n++; } while (0)
    PUT(PM4_TYPE3(PM4_PREAMBLE_CNTL, 0));
    PUT(PM4_PREAMBLE_BEGIN_CLEAR);
    PUT(PM4_TYPE3(PM4_CONTEXT_CONTROL, 1));
    PUT(0x80000000u);
    PUT(0x80000000u);
    for (const amd_cs_extent_t *e = gfx9_cs_extents; e->extent; e++) {
        PUT(PM4_TYPE3(PM4_SET_CONTEXT_REG, e->reg_count));
        PUT(e->reg_index - PM4_CONTEXT_REG_START);
        for (u32 i = 0; i < e->reg_count; i++) PUT(e->extent[i]);
    }
    PUT(PM4_TYPE3(PM4_PREAMBLE_CNTL, 0));
    PUT(PM4_PREAMBLE_END_CLEAR);
    PUT(PM4_TYPE3(PM4_CLEAR_STATE, 0));
    PUT(0);
#undef PUT
    return n;
}

/* ------------------------------------------------------------ seleção de SE/SH e de VMID */
static void select_se_sh(amdgpu_t *a, u32 se, u32 sh) {
    u32 v = GRBM_IDX_INSTANCE_BCAST;
    if (se == 0xFFFFFFFFu) v |= 0x80000000u; else v |= se << GRBM_IDX_SE_SHIFT;
    if (sh == 0xFFFFFFFFu) v |= 0x20000000u; else v |= sh << GRBM_IDX_SH_SHIFT;
    WR(a, R_GRBM_GFX_INDEX, v);
}
static void select_vmid(amdgpu_t *a, u32 vmid) { WR(a, R_GRBM_GFX_CNTL, vmid << GRBM_GFX_CNTL_VMID_SHIFT); }

/* ------------------------------------------------------------ constantes (constants_init) */
static void constants_init(amdgpu_t *a, u32 cu_per_sh) {
    WR(a, R_GRBM_CNTL, (RR(a, R_GRBM_CNTL) & ~GRBM_READ_TIMEOUT_MASK) | 0xFFu);
    a->gb_addr_config = RR(a, R_GB_ADDR_CONFIG);

    /* CUs ativos (a família Raven tem 1 SE com 1 SH) */
    select_se_sh(a, 0, 0);
    u32 inactive = ((RR(a, R_CC_GC_SHADER_ARRAY_CONFIG) | RR(a, R_GC_USER_SHADER_ARRAY_CONFIG)) >> INACTIVE_CUS_SHIFT) & 0xFFFFu;
    select_se_sh(a, 0xFFFFFFFFu, 0xFFFFFFFFu);
    u32 all = cu_per_sh >= 32 ? 0xFFFFFFFFu : (1u << cu_per_sh) - 1;
    a->cu_mask = ~inactive & all;
    a->cu_count = (u32)__builtin_popcount(a->cu_mask);

    /* aberturas LDS/scratch de cada VMID: o 0 (kernel) sem bases; os processos com as aberturas privada e
     * compartilhada. Sem retry de falta de página: um endereço ruim lê a página de rascunho, não trava o CP. */
    u32 cfg = SH_MEM_ALIGN_UNALIGNED | SH_MEM_RETRY_DISABLE;
    for (u32 v = 0; v < 16; v++) {
        select_vmid(a, v);
        WR(a, R_SH_MEM_CONFIG, cfg);
        WR(a, R_SH_MEM_BASES, v ? (SH_MEM_PRIVATE_BASE | SH_MEM_SHARED_BASE << 16) : 0);
    }
    select_vmid(a, 0);
    /* GDS, GWS e OA só no VMID 0 */
    for (u32 v = 1; v < 16; v++) {
        WR(a, R_GDS_VMID0_BASE + 8 * v, 0);
        WR(a, R_GDS_VMID0_SIZE + 8 * v, 0);
        WR(a, R_GDS_GWS_VMID0 + 4 * v, 0);
        WR(a, R_GDS_OA_VMID0 + 4 * v, 0);
    }
}

/* ------------------------------------------------------------ RLC */
static void gui_idle_irq(amdgpu_t *a, bool on) {
    u32 t = RR(a, R_CP_INT_CNTL_RING0);
    t = on ? t | CP_INT_GUI_IDLE_ALL : t & ~CP_INT_GUI_IDLE_ALL;
    WR(a, R_CP_INT_CNTL_RING0, t);
}

static void rlc_stop(amdgpu_t *a) {
    WR(a, R_RLC_CNTL, RR(a, R_RLC_CNTL) & ~RLC_ENABLE_F32);
    gui_idle_irq(a, false);
    select_se_sh(a, 0, 0);
    for (u32 k = 0; k < USEC_TIMEOUT && RR(a, R_RLC_SERDES_CU_MASTER_BUSY); k++) UDELAY(1);
    select_se_sh(a, 0xFFFFFFFFu, 0xFFFFFFFFu);
    for (u32 k = 0; k < USEC_TIMEOUT && (RR(a, R_RLC_SERDES_NONCU_MASTER_BUSY) & SERDES_NONCU_BUSY); k++) UDELAY(1);
}

/* lista de save/restore do RLC v2.1 (Raven2): gfx_v9_1_init_rlc_save_restore_list */
static bool rlc_save_restore_list(amdgpu_t *a, const amd_ucode_t *u) {
    const u32 *f = u->rlc_reg_list_format;
    u32 fdw = u->rlc_reg_list_format_dwords, direct = u->rlc_direct_reg_list_length;
    u32 uniq[8] = {0}, starts[10] = {0}, nstarts = 0;
    if (direct > fdw) return false;
    for (u32 off = direct; off < fdw; off++) {
        if (nstarts >= 10) return false;
        starts[nstarts++] = off;
        while (off < fdw && f[off] != 0xFFFFFFFFu) {
            off += 2;
            if (off >= fdw) return false;
            u32 j = 0;
            while (j < 8 && uniq[j] && uniq[j] != f[off]) j++;
            if (j >= 8) return false;
            if (!uniq[j]) uniq[j] = f[off];
            off++;
        }
    }
    WR(a, R_RLC_SRM_CNTL, RR(a, R_RLC_SRM_CNTL) | SRM_AUTO_INCR);
    WR(a, R_RLC_SRM_ARAM_ADDR, 0);
    for (u32 i = 0; i < u->rlc_reg_list_dwords; i++) WR(a, R_RLC_SRM_ARAM_DATA, u->rlc_reg_list[i]);
    WR(a, R_RLC_GPM_SCRATCH_ADDR, u->rlc_reg_list_format_start);
    u32 i = 0;
    for (; i < direct; i++) WR(a, R_RLC_GPM_SCRATCH_DATA, f[i]);
    while (i < fdw) {
        if (f[i] == 0xFFFFFFFFu) { WR(a, R_RLC_GPM_SCRATCH_DATA, f[i++]); continue; }
        if (i + 2 >= fdw) return false;
        WR(a, R_RLC_GPM_SCRATCH_DATA, f[i++]);
        WR(a, R_RLC_GPM_SCRATCH_DATA, f[i++]);
        u32 j = 0;
        while (j < 8 && uniq[j] != f[i]) j++;
        if (j >= 8) return false;
        WR(a, R_RLC_GPM_SCRATCH_DATA, j);
        i++;
    }
    WR(a, R_RLC_GPM_SCRATCH_ADDR, u->rlc_reg_restore_list_size);
    WR(a, R_RLC_GPM_SCRATCH_DATA, u->rlc_reg_list_dwords >> 1);
    WR(a, R_RLC_GPM_SCRATCH_ADDR, u->rlc_starting_offsets_start);
    for (u32 k = 0; k < 10; k++) WR(a, R_RLC_GPM_SCRATCH_DATA, starts[k]);
    for (u32 k = 0; k < 8; k++) {
        if (!uniq[k]) continue;
        WR(a, R_RLC_SRM_INDEX_CNTL_ADDR_0 + 4 * k, uniq[k] & 0x3FFFFu);
        WR(a, R_RLC_SRM_INDEX_CNTL_DATA_0 + 4 * k, uniq[k] >> 20);
    }
    return true;
}

/* balanceamento de carga (LBPW) da família Raven e a máscara de CUs sempre ligados (4 na APU, 2 no PG) */
static void rlc_lbpw(amdgpu_t *a, u32 cu_per_sh) {
    WR(a, R_RLC_LB_THR_CONFIG_1, 0x0000007Fu);
    WR(a, R_RLC_LB_THR_CONFIG_2, 0x0333A5A7u);
    WR(a, R_RLC_LB_THR_CONFIG_3, 0x00000077u);
    WR(a, R_RLC_LB_THR_CONFIG_4, 0x30u | 0x40u << 8 | 0x02FAu << 16);
    WR(a, R_RLC_LB_CNTR_INIT, 0);
    WR(a, R_RLC_LB_CNTR_MAX, 0x00000500u);
    select_se_sh(a, 0xFFFFFFFFu, 0xFFFFFFFFu);
    WR(a, R_RLC_LB_INIT_CU_MASK, 0xFFFFFFFFu);
    WR(a, R_RLC_LB_PARAMS, 3u << 1 | 0x10u << 8 | 0x033Fu << 16);
    WR(a, R_RLC_GPM_GENERAL_7, (RR(a, R_RLC_GPM_GENERAL_7) & 0x0000FFFFu) | 0x00C00000u);
    WR(a, R_RLC_LB_CNTL, 0x80000094u);           /* LB_CNT_SPIM_ACTIVE, histerese 9, bit 31 reservado */

    select_se_sh(a, 0, 0);
    u32 mask = 1, bitmap = 0, counter = 0;
    for (u32 k = 0; k < cu_per_sh; k++, mask <<= 1) {
        if (!(a->cu_mask & mask)) continue;
        if (counter == 2) WR(a, R_RLC_PG_ALWAYS_ON_CU_MASK, bitmap);
        if (counter >= 4) break;
        bitmap |= mask;
        counter++;
    }
    WR(a, R_RLC_LB_ALWAYS_ACTIVE_CU_MASK, bitmap);
    select_se_sh(a, 0xFFFFFFFFu, 0xFFFFFFFFu);
    WR(a, R_RLC_LB_CNTL, RR(a, R_RLC_LB_CNTL) | RLC_LB_ENABLE);
}

static bool load_fw(amdgpu_t *a, const char *suffix, int kind, amd_ucode_t *u) {
    char name[40];
    if (kind == UC_KIND_RLC) amd_raven_rlc_name(a->variant, a->pci_rev, a->smu_version, name, sizeof name);
    else snprintf(name, sizeof name, "%s_%s.bin", amd_raven_fw_prefix(a->variant), suffix);
    u64 len = 0;
    const u8 *fw = amdgpu_fw_load(name, &len);
    int e = fw ? amd_ucode_parse(fw, (usize)len, kind, u) : -1;
    if (e) { kprintf("amdgpu: firmware %s %s\n", name, fw ? amd_ucode_error(e) : "não encontrado"); return false; }
    return true;
}

static void put_ucode(amdgpu_t *a, u32 addr_reg, u32 data_reg, u32 start, const amd_ucode_t *u) {
    WR(a, addr_reg, start);
    for (u32 i = 0; i < u->ucode_dwords; i++) WR(a, data_reg, amd_le32((const u8 *)(u->ucode + i)));
    WR(a, addr_reg, u->ucode_version);
}

static bool rlc_resume(amdgpu_t *a, u32 cu_per_sh) {
    amd_ucode_t rlc;
    if (!load_fw(a, "rlc", UC_KIND_RLC, &rlc)) return false;
    rlc_stop(a);
    WR(a, R_RLC_CGCG_CGLS_CTRL, 0);              /* sem clock gating durante a subida */

    /* CSB: o RLC refaz o clear state a partir dele (depois de um reset do CP ou ao sair do GFXOFF) */
    WR(a, R_RLC_CSIB_ADDR_HI, (u32)(a->csb_mc >> 32));
    WR(a, R_RLC_CSIB_ADDR_LO, (u32)a->csb_mc & 0xFFFFFFFCu);
    WR(a, R_RLC_CSIB_LENGTH, a->csb_dw);
    if (rlc.hdr_minor >= 1) {
        if (a->variant == AMD_RAVEN2 && !rlc_save_restore_list(a, &rlc)) {
            kprintf("amdgpu: lista de save/restore do RLC malformada\n");
            return false;
        }
        WR(a, R_RLC_SRM_CNTL, RR(a, R_RLC_SRM_CNTL) | SRM_ENABLE);
    }
    amdgpu_gfx_pg_init(a);                        /* tabela de saltos e atrasos do power gating (GFXOFF) */
    put_ucode(a, R_RLC_GPM_UCODE_ADDR, R_RLC_GPM_UCODE_DATA, RLC_UCODE_START, &rlc);
    a->rlc_fw_version = rlc.ucode_version;
    rlc_lbpw(a, cu_per_sh);
    WR(a, R_RLC_SPM_MC_CNTL, (RR(a, R_RLC_SPM_MC_CNTL) & ~RLC_SPM_VMID_MASK) | 0xFu);
    WR(a, R_RLC_CNTL, RR(a, R_RLC_CNTL) | RLC_ENABLE_F32);
    UDELAY(50);
    return true;
}

/* ------------------------------------------------------------ CP gráfico */
static void cp_gfx_enable(amdgpu_t *a, bool on) {
    u32 t = RR(a, R_CP_ME_CNTL);
    u32 bits = CP_ME_HALT_ALL | CP_ME_RESET_ALL | CP_ME_ICACHE_INV;
    t = on ? t & ~bits : t | bits;
    WR(a, R_CP_ME_CNTL, t);
    UDELAY(50);
}

#define GFX_DB_INDEX (AMD_DB_GFX_RING0 << 1)          /* em dwords */
static u32 gring_rptr(amdgpu_t *a) { return a->gwb_cpu[0] & (a->gring_dw - 1); }

static bool gring_room(amdgpu_t *a, u32 ndw) {
    for (u32 n = 0; n < SPIN_MAX; n++) {
        u32 used = ((u32)a->gring_wptr - gring_rptr(a)) & (a->gring_dw - 1);
        if (a->gring_dw - used > ndw + 256 + 8) return true;
        if (!a->msi_on && !(n & 1023)) amdgpu_irq_process(a);
        CPU_RELAX();
    }
    a->gfx_timeouts++;
    return false;
}

static void gput(amdgpu_t *a, u32 v) { a->gring_cpu[(u32)a->gring_wptr & (a->gring_dw - 1)] = v; a->gring_wptr++; }

static void gcommit(amdgpu_t *a) {
    while (a->gring_wptr & 0xFF) gput(a, PM4_NOP_1DW);      /* o CP busca em blocos de 256 dwords */
    WMB();
    if (a->gfx_db) {                                /* doorbell: acorda o GFX mesmo com o GFXOFF ligado */
        a->gwb_cpu[2] = (u32)a->gring_wptr;
        a->gwb_cpu[3] = (u32)(a->gring_wptr >> 32);
        WMB();
        amdgpu_doorbell64(a, GFX_DB_INDEX, a->gring_wptr);
    } else {
        WR(a, R_CP_RB0_WPTR, (u32)a->gring_wptr & (a->gring_dw - 1));
        WR(a, R_CP_RB0_WPTR_HI, 0);
    }
}

static u32 gfence(amdgpu_t *a) {
    u32 seq = ++a->gfx_seq;
    u64 addr = a->gwb_mc + 16;
    gput(a, PM4_TYPE3(PM4_RELEASE_MEM, 6));
    gput(a, PM4_EOP_TCL1 | PM4_EOP_TC | PM4_EOP_TC_MD | PM4_EOP_TC_WB | PM4_EV_CACHE_FLUSH_INV_TS | PM4_EVENT_INDEX(5));
    gput(a, PM4_DATA_SEL_32 | PM4_INT_SEL_WAIT);
    gput(a, (u32)addr);
    gput(a, (u32)(addr >> 32));
    gput(a, seq);
    gput(a, 0);
    gput(a, 0);
    return seq;
}

u32 amdgpu_gfx_done(amdgpu_t *a) { return a && a->gfx_ok ? a->gwb_cpu[4] : 0; }

static bool gwait(amdgpu_t *a, u32 seq) {
    for (u32 n = 0; n < SPIN_MAX * 4; n++) {
        if ((i32)(a->gwb_cpu[4] - seq) >= 0) return true;
        if (!a->msi_on && !(n & 1023)) amdgpu_irq_process(a);
        CPU_RELAX();
    }
    a->gfx_timeouts++;
    return false;
}

bool amdgpu_gfx_wait(amdgpu_t *a, u32 seq) { return a && a->gfx_ok && gwait(a, seq); }

static u32 gsubmit(amdgpu_t *a, u64 ib_mc, u32 ndw, u32 vmid) {
    if (!ndw || ndw > 0xFFFFFu || (ib_mc & 3) || !gring_room(a, 4 + 8)) return 0;
    gput(a, PM4_TYPE3(PM4_INDIRECT_BUFFER, 2));
    gput(a, (u32)ib_mc);
    gput(a, (u32)(ib_mc >> 32) & 0xFFFFu);
    gput(a, ndw | vmid << 24);
    u32 seq = gfence(a);
    gcommit(a);
    a->gfx_ibs++;
    return seq;
}

u32 amdgpu_gfx_submit(amdgpu_t *a, u64 ib_mc, u32 ndw) {
    if (!a || !a->gfx_ok) return 0;
    return gsubmit(a, ib_mc, ndw, 0);
}

u32 amdgpu_gfx_submit_vm(amdgpu_t *a, amd_vm_t *vm, u64 ib_va, u32 ndw) {
    if (!a || !a->gfx_ok || !vm || !vm->vmid) return 0;
    return gsubmit(a, ib_va, ndw, vm->vmid);
}

static bool cp_resume(amdgpu_t *a) {
    amd_ucode_t pfp, ce, me;
    if (!load_fw(a, "pfp", UC_KIND_GFX, &pfp) || !load_fw(a, "ce", UC_KIND_GFX, &ce) || !load_fw(a, "me", UC_KIND_GFX, &me))
        return false;
    cp_gfx_enable(a, false);
    put_ucode(a, R_CP_PFP_UCODE_ADDR, R_CP_PFP_UCODE_DATA, 0, &pfp);
    put_ucode(a, R_CP_CE_UCODE_ADDR, R_CP_CE_UCODE_DATA, 0, &ce);
    put_ucode(a, R_CP_ME_RAM_WADDR, R_CP_ME_RAM_DATA, 0, &me);
    a->pfp_fw_version = pfp.ucode_version; a->ce_fw_version = ce.ucode_version; a->me_fw_version = me.ucode_version;
    WR(a, R_CP_MEC_CNTL, CP_MEC_RESET_ALL);       /* compute parado até a fila de compute subir (compute.c) */

    /* anel 0 */
    WR(a, R_CP_RB_WPTR_DELAY, 0);
    WR(a, R_CP_RB_VMID, 0);
    u32 order = 0;
    while ((1u << order) < AMD_GFX_RING_BYTES / 8) order++;
    u32 cntl = order << CP_RB_BUFSZ_SHIFT | (order - 2) << CP_RB_BLKSZ_SHIFT;
    WR(a, R_CP_RB0_CNTL, cntl);
    a->gring_wptr = 0;
    WR(a, R_CP_RB0_WPTR, 0);
    WR(a, R_CP_RB0_WPTR_HI, 0);
    WR(a, R_CP_RB0_RPTR_ADDR, (u32)a->gwb_mc);
    WR(a, R_CP_RB0_RPTR_ADDR_HI, (u32)(a->gwb_mc >> 32) & 0xFFFFu);
    u64 poll = a->gwb_mc + 8;
    WR(a, R_CP_RB_WPTR_POLL_ADDR_LO, (u32)poll);
    WR(a, R_CP_RB_WPTR_POLL_ADDR_HI, (u32)(poll >> 32));
    UDELAY(1000);
    WR(a, R_CP_RB0_CNTL, cntl);
    WR(a, R_CP_RB0_BASE, (u32)(a->gring_mc >> 8));
    WR(a, R_CP_RB0_BASE_HI, (u32)(a->gring_mc >> 40));
    a->gfx_db = amdgpu_doorbell_init(a);
    u32 dbc = RR(a, R_CP_RB_DOORBELL_CONTROL) & ~(CP_RB_DOORBELL_EN | 0x0FFFFFFCu);
    if (a->gfx_db) dbc |= GFX_DB_INDEX << 2 | CP_RB_DOORBELL_EN;     /* sem BAR2: WPTR por registrador */
    WR(a, R_CP_RB_DOORBELL_CONTROL, dbc);
    WR(a, R_CP_RB_DOORBELL_RANGE_LOWER, a->gfx_db ? GFX_DB_INDEX << 2 : 0);
    WR(a, R_CP_RB_DOORBELL_RANGE_UPPER, CP_RB_DOORBELL_UPPER_MASK);

    /* liga e manda o preâmbulo de clear state, a base do CE e o tipo de índice */
    WR(a, R_CP_MAX_CONTEXT, AMD_GFX_MAX_HW_CTX - 1);
    WR(a, R_CP_DEVICE_ID, 1);
    cp_gfx_enable(a, true);
    if (!gring_room(a, a->csb_dw + 4 + 3)) return false;
    for (u32 i = 0; i < a->csb_dw; i++) gput(a, ((const u32 *)amdgpu_vram_cpu(a, a->csb_phys))[i]);
    gput(a, PM4_TYPE3(PM4_SET_BASE, 2));
    gput(a, PM4_CE_PARTITION_BASE);
    gput(a, 0x8000);
    gput(a, 0x8000);
    gput(a, PM4_TYPE3(PM4_SET_UCONFIG_REG, 1));
    gput(a, PM4_UCONFIG_INDEX_TYPE | (R_VGT_INDEX_TYPE / 4 - PM4_UCONFIG_REG_START));
    gput(a, 0);
    gcommit(a);
    return true;
}

/* teste do anel: SET_UCONFIG_REG em SCRATCH_REG0; depois um IB que escreve na memória, com a fence */
static bool gfx_tests(amdgpu_t *a, u64 ib_phys) {
    WR(a, R_SCRATCH_REG0, 0xCAFEDEADu);
    if (!gring_room(a, 3)) return false;
    gput(a, PM4_TYPE3(PM4_SET_UCONFIG_REG, 1));
    gput(a, R_SCRATCH_REG0 / 4 - PM4_UCONFIG_REG_START);
    gput(a, 0xDEADBEEFu);
    gcommit(a);
    u32 v = 0;
    for (u32 i = 0; i < USEC_TIMEOUT && (v = RR(a, R_SCRATCH_REG0)) != 0xDEADBEEFu; i++) UDELAY(1);
    if (v != 0xDEADBEEFu) { kprintf("amdgpu: o CP não executou o anel (SCRATCH_REG0 = 0x%x)\n", v); return false; }

    u32 *ib = amdgpu_vram_cpu(a, ib_phys);
    u64 dst = a->gwb_mc + 32;
    a->gwb_cpu[8] = 0xCAFEDEADu;
    ib[0] = PM4_TYPE3(PM4_WRITE_DATA, 3);
    ib[1] = PM4_WD_DST_MEM | PM4_WR_CONFIRM;
    ib[2] = (u32)dst;
    ib[3] = (u32)(dst >> 32);
    ib[4] = 0xDEADBEEFu;
    WMB();
    u32 seq = gsubmit(a, amdgpu_vram_to_mc(a, ib_phys), 5, 0);
    if (!seq || !gwait(a, seq) || a->gwb_cpu[8] != 0xDEADBEEFu) {
        kprintf("amdgpu: o IB de teste não rodou (writeback 0x%x, fence %u de %u)\n", a->gwb_cpu[8], a->gwb_cpu[4], seq);
        return false;
    }
    return true;
}

/* ------------------------------------------------------------ entrada */
bool amdgpu_gfx_init(amdgpu_t *a) {
    if (!a || !a->smu_ok || !a->gmc_ok || !a->gfxhub_ok) return false;
    /* GFX ligado e sem GFXOFF enquanto o driver mexe no GC (o SMU só desliga depois de AllowGfxOff) */
    if (amdgpu_smu_send(a, SMU_MSG_POWER_UP_GFX, 0, NULL)) kprintf("amdgpu: SMU recusou PowerUpGfx; segue\n");
    if (amdgpu_smu_send(a, SMU_MSG_DISABLE_GFXOFF, 0, NULL)) kprintf("amdgpu: SMU recusou DisableGfxOff; segue\n");

    amd_ucode_t gi;
    u32 cu_per_sh = 11;
    if (load_fw(a, "gpu_info", UC_KIND_GPU_INFO, &gi) && gi.gi_num_cu_per_sh && gi.gi_num_cu_per_sh <= 16) cu_per_sh = gi.gi_num_cu_per_sh;

    /* memória: anel (64 KiB), writeback (4 KiB) e um bloco com o CSB e o IB de teste */
    bool again = a->gring_phys != 0;         /* volta do S3: anel, writeback e CSB continuam na VRAM */
    u64 ring = again ? a->gring_phys : amdgpu_vram_alloc(a, AMD_GFX_RING_BYTES), wb = again ? a->gwb_phys : amdgpu_vram_alloc(a, 4096),
        aux = again ? a->csb_phys : amdgpu_vram_alloc(a, 64 * 1024);
    if (!ring || !wb || !aux) {
        kprintf("amdgpu: sem VRAM para o anel do GFX\n");
        if (ring) amdgpu_vram_free(a, ring, AMD_GFX_RING_BYTES);
        if (wb) amdgpu_vram_free(a, wb, 4096);
        if (aux) amdgpu_vram_free(a, aux, 64 * 1024);
        return false;
    }
#ifndef MOS_HOST_TEST
    if (!again) {
        vmm_map_wc(a->fb_phys + (ring - a->bar0), AMD_GFX_RING_BYTES);
        vmm_map_wc(a->fb_phys + (wb - a->bar0), 4096);
        vmm_map_wc(a->fb_phys + (aux - a->bar0), 64 * 1024);
    }
#endif
    a->gring_phys = ring; a->gring_cpu = amdgpu_vram_cpu(a, ring); a->gring_mc = amdgpu_vram_to_mc(a, ring);
    a->gring_dw = AMD_GFX_RING_BYTES / 4;
    a->gwb_phys = wb; a->gwb_cpu = amdgpu_vram_cpu(a, wb); a->gwb_mc = amdgpu_vram_to_mc(a, wb);
    memset((void *)a->gring_cpu, 0, AMD_GFX_RING_BYTES);
    if (!again) memset((void *)a->gwb_cpu, 0, 4096);
    else { a->gwb_cpu[0] = 0; a->gwb_cpu[4] = a->gfx_seq; }
    a->csb_phys = aux; a->csb_mc = amdgpu_vram_to_mc(a, aux);
    a->csb_dw = amdgpu_gfx_csb(amdgpu_vram_cpu(a, aux));

    program(a, gc_9_1, ARRAY_LEN(gc_9_1));
    if (a->variant == AMD_RAVEN2) program(a, gc_9_1_rv2, ARRAY_LEN(gc_9_1_rv2));
    else program(a, gc_9_1_rv1, ARRAY_LEN(gc_9_1_rv1));
    program(a, gc_9_x_common, ARRAY_LEN(gc_9_x_common));
    constants_init(a, cu_per_sh);

    bool ok = rlc_resume(a, cu_per_sh) && cp_resume(a);
    if (ok) {
        /* EOP e faltas do CP no IH; ocioso/ocupado para o deep sleep da APU (gfx_v9_0_cp_resume) */
        WR(a, R_CP_INT_CNTL_RING0, RR(a, R_CP_INT_CNTL_RING0) | CP_INT_TIME_STAMP | CP_INT_PRIV_REG | CP_INT_PRIV_INSTR | CP_INT_OPCODE_ERROR);
        gui_idle_irq(a, true);
        ok = gfx_tests(a, aux + 32 * 1024);
    }
    if (!ok) {
        cp_gfx_enable(a, false);
        WR(a, R_RLC_CNTL, RR(a, R_RLC_CNTL) & ~RLC_ENABLE_F32);
        amdgpu_vram_free(a, ring, AMD_GFX_RING_BYTES);
        amdgpu_vram_free(a, wb, 4096);
        amdgpu_vram_free(a, aux, 64 * 1024);
        a->gring_cpu = a->gwb_cpu = NULL;
        a->gring_phys = a->gwb_phys = a->csb_phys = 0;
        kprintf("amdgpu: motor 3D desligado; tela e SDMA seguem\n");
        return false;
    }
    a->gfx_ok = true;
    if (!amdgpu_compute_init(a)) kprintf("amdgpu: sem fila de compute; o anel GFX segue\n");
    amdgpu_gfx_pg_enable(a);                       /* GFXOFF por último: daqui em diante o GC dorme quando ocioso */
    kprintf("amdgpu: GFX 9 no ar: %u CUs (máscara 0x%x), GB_ADDR_CONFIG 0x%x, PFP 0x%x CE 0x%x ME 0x%x RLC 0x%x\n",
            a->cu_count, a->cu_mask, a->gb_addr_config, a->pfp_fw_version, a->ce_fw_version, a->me_fw_version, a->rlc_fw_version);
    return true;
}
