/* mOS — GFXOFF e power gating do GFX na família Raven (como o amdgpu liga no Picasso: GFX_PG, CP e o handshake
 * RLC/SMU). O RLC desliga o GC quando ele fica ocioso e o religa quando um doorbell chega (o anel GFX e a fila
 * de compute andam por doorbell). Enquanto o GC está desligado, acesso a registrador dele se perde ou trava: todo
 * acesso do driver passa por amdgpu_gfxoff_get/put, que pede ao SMU para segurar o GC ligado (DisableGfxOff) e
 * devolve no último put (EnableGfxOff). Referência: gfx_v9_0.c (init_gfx_power_gating, set_powergating_state,
 * cp_table) e smu10_hwmgr.c, MIT. Testado no host (tools/amdgpu_hw_host.c). */
#include "amdgpu.h"
#include "regs.h"
#include "ucode.h"
#ifndef MOS_HOST_TEST
#include "syscall.h"
#endif

#ifdef MOS_HOST_TEST
#define RR(a, o)    ((void)(a), amd_host_rreg(o))
#define WR(a, o, v) ((void)(a), amd_host_wreg(o, v))
#define WMB()       ((void)0)
#else
#define RR(a, o)    mmio_r32((a)->mmio, (o))
#define WR(a, o, v) mmio_w32((a)->mmio, (o), (v))
#define WMB()       __asm__ volatile("sfence" ::: "memory")
#endif

#define SPIN_MAX 200000u

/* tabela de saltos do CP: as tabelas de CE, PFP, ME e MEC, uma depois da outra, num bloco da VRAM; o RLC as
 * recarrega quando religa o GC (amdgpu_gfx_rlc_setup_cp_table) */
static bool cp_table(amdgpu_t *a) {
    static const char *sfx[4] = {"ce", "pfp", "me", "mec"};
    if (!a->cp_table_phys) {
        a->cp_table_phys = amdgpu_vram_alloc(a, 64 * 1024);
        if (!a->cp_table_phys) return false;
#ifndef MOS_HOST_TEST
        vmm_map_wc(a->fb_phys + (a->cp_table_phys - a->bar0), 64 * 1024);
#endif
    }
    u32 *dst = amdgpu_vram_cpu(a, a->cp_table_phys), used = 0;
    for (int me = 0; me < 4; me++) {
        char name[40];
        snprintf(name, sizeof name, "%s_%s.bin", amd_raven_fw_prefix(a->variant), sfx[me]);
        u64 len = 0;
        const u8 *fw = amdgpu_fw_load(name, &len);
        amd_ucode_t u;
        if (!fw || amd_ucode_parse(fw, (usize)len, UC_KIND_GFX, &u) || used + u.jt_size > 16 * 1024) return false;
        for (u32 i = 0; i < u.jt_size; i++) dst[used + i] = amd_le32((const u8 *)(u.ucode + u.jt_offset + i));
        used += u.jt_size;
    }
    WMB();
    a->cp_table_dw = used;
    return true;
}

static void rlc_safe_mode(amdgpu_t *a, bool enter) {
    WR(a, R_RLC_SAFE_MODE, RLC_SAFE_MODE_CMD | (enter ? RLC_SAFE_MODE_ENTER : 0));
    for (u32 n = 0; n < SPIN_MAX && (RR(a, R_RLC_SAFE_MODE) & RLC_SAFE_MODE_CMD); n++) { }
}

/* chamado pelo rlc_resume (gfx.c) antes de ligar o RLC: tabela de saltos e atrasos do power gating */
bool amdgpu_gfx_pg_init(amdgpu_t *a) {
    if (!cp_table(a)) { kprintf("amdgpu: sem a tabela de saltos do CP; GFXOFF desligado\n"); a->gfxoff_cap = false; return false; }
    WR(a, R_RLC_JUMP_TABLE_RESTORE, (u32)(amdgpu_vram_to_mc(a, a->cp_table_phys) >> 8));
    WR(a, R_CP_RB_WPTR_POLL_CNTL, (RR(a, R_CP_RB_WPTR_POLL_CNTL) & ~IDLE_POLL_COUNT_MASK) | 0x60u << 16);
    WR(a, R_RLC_PG_DELAY, 0x10u | 0x10u << 8 | 0x10u << 16 | 0x40u << 24);
    WR(a, R_RLC_PG_DELAY_2, (RR(a, R_RLC_PG_DELAY_2) & ~PG_DELAY_2_SERDES_MASK) | 0x4u << 8);
    WR(a, R_RLC_PG_DELAY_3, (RR(a, R_RLC_PG_DELAY_3) & ~PG_DELAY_3_CGCG_MASK) | 0xFFu);
    WR(a, R_RLC_AUTO_PG_CTRL, (RR(a, R_RLC_AUTO_PG_CTRL) & ~AUTO_PG_IDLE_THRESH_MASK) | 0x55F0u << AUTO_PG_IDLE_THRESH_SHIFT);
    /* o GC entrega o controle do power gating ao RLC (pwr_10_0_gfxip_control_over_cgpg) */
    u32 p = RR(a, R_PWR_MISC_CNTL_STATUS) | PWR_GFX_RLC_CGPG_EN;
    WR(a, R_PWR_MISC_CNTL_STATUS, p);
    WR(a, R_PWR_MISC_CNTL_STATUS, (p & ~PWR_GFXOFF_STATUS_MASK) | 2u << PWR_GFXOFF_STATUS_SHIFT);
    a->gfxoff_cap = true;
    return true;
}

/* depois dos anéis no ar: liga o PG (gfx_v9_0_set_powergating_state, GATE) e entrega o GC ao SMU */
void amdgpu_gfx_pg_enable(amdgpu_t *a) {
    if (!a->gfxoff_cap || !a->gfx_db) return;              /* sem doorbell, nada acordaria o GC */
    u32 pg = RR(a, R_RLC_PG_CNTL) | PG_SMU_SLOWDOWN_PU | PG_SMU_SLOWDOWN_PD;
    pg &= ~PG_CP_PG_DISABLE;
    WR(a, R_RLC_PG_CNTL, pg);
    rlc_safe_mode(a, true);
    WR(a, R_RLC_PG_CNTL, RR(a, R_RLC_PG_CNTL) | PG_GFX_POWER_GATING);
    rlc_safe_mode(a, false);
    a->gfxoff_hold = 0;
    if (!amdgpu_smu_send(a, SMU_MSG_ENABLE_GFXOFF, 0, NULL)) { a->gfxoff_on = true; kprintf("amdgpu: GFXOFF ligado\n"); }
}

/* segura o GC ligado enquanto o driver mexe em registradores dele */
void amdgpu_gfxoff_get(amdgpu_t *a) {
    if (!a || !a->gfxoff_on) return;
    if (a->gfxoff_hold++ == 0) {
        amdgpu_smu_send(a, SMU_MSG_DISABLE_GFXOFF, 0, NULL);
        /* espera o GC ligado (smu10: PWR_GFXOFF_STATUS == 2) */
        for (u32 n = 0; n < SPIN_MAX && ((RR(a, R_PWR_MISC_CNTL_STATUS) & PWR_GFXOFF_STATUS_MASK) >> PWR_GFXOFF_STATUS_SHIFT) != 2; n++) { }
        a->gfxoff_wakes++;
    }
}

void amdgpu_gfxoff_put(amdgpu_t *a) {
    if (!a || !a->gfxoff_on || !a->gfxoff_hold) return;
    if (--a->gfxoff_hold == 0) amdgpu_smu_send(a, SMU_MSG_ENABLE_GFXOFF, 0, NULL);
}

/* antes do S3 ou de desligar o GFX: GC ligado de vez e o PG desfeito */
void amdgpu_gfx_pg_disable(amdgpu_t *a) {
    if (!a || !a->gfxoff_on) return;
    amdgpu_smu_send(a, SMU_MSG_DISABLE_GFXOFF, 0, NULL);
    a->gfxoff_on = false;
    a->gfxoff_hold = 0;
    rlc_safe_mode(a, true);
    WR(a, R_RLC_PG_CNTL, RR(a, R_RLC_PG_CNTL) & ~PG_GFX_POWER_GATING);
    rlc_safe_mode(a, false);
}
