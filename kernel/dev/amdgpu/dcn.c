/* mOS — caminho de pixels do DCN 1.0 (Raven/Picasso/Raven2). Porte das sequências do display/dc da AMD (MIT):
 * dcn10_hwseq.c (enable_stream_timing, program_pipe, enable_plane, update_dchubp_dpp, plane_atomic_disable,
 * reset_back_end), dcn10_hubbub.c (marcas d'água), dcn10_hubp.c (requestor, deadline, superfície, VM, viewport),
 * dcn10_dpp*.c (CNVC, LB/recout, CM), dcn10_mpc.c (árvore MPCC), dcn10_opp.c (FMT) e dcn10_optc.c (timing).
 * Um plano por tela, sem escala; a tela pode usar dois pipes (divisão ao meio, como o Linux faz com uma tela só).
 * Cor: degama, gamut e regama em bypass (a superfície sRGB sai como está); a CSC de saída converte para YCbCr
 * 4:4:4 quando a saída é YCbCr (BT.709 acima de 27,03 MHz, BT.601 abaixo), como o amdgpu faz no HDMI. Registradores e campos: dcn_regs.h, gerado de dcn_1_0_offset.h/sh_mask.h. */
#include "amdgpu.h"
#include "regs.h"
#include "dcn.h"
#include "dcn_regs.h"
#include "dml1.h"
#include "ucode.h"
#ifndef MOS_HOST_TEST
#include "syscall.h"
#endif

#ifdef MOS_HOST_TEST
#define RR(a, o)    ((void)(a), amd_host_rreg(o))
#define WR(a, o, v) ((void)(a), amd_host_wreg(o, v))
#define DELAY_US(n) ((void)(n))
#else
#define RR(a, o)    mmio_r32((a)->mmio, (o))
#define WR(a, o, v) mmio_w32((a)->mmio, (o), (v))
#define DELAY_US(n) do { for (u32 _i = 0; _i < (n); _i++) io_wait(); } while (0)
#endif

/* campo = "shift, máscara" (dcn_regs.h) */
#define FV_(s, m, v)     ((((u32)(v)) << (s)) & (m))
#define FV(f, v)         FV_(f, v)
#define FG_(x, s, m)     (((x) & (m)) >> (s))
#define FG(x, f)         FG_(x, f)
#define UPD_(a, r, s, m, v) upd(a, r, m, ((u32)(v) << (s)) & (m))
#define UPD(a, r, f, v)  UPD_(a, r, f, v)
#define WAIT_(a, r, s, m, v, n) wait_field(a, r, s, m, v, n)
#define WAIT(a, r, f, v, n) WAIT_(a, r, f, v, n)

static void upd(amdgpu_t *a, u32 r, u32 mask, u32 bits) { WR(a, r, (RR(a, r) & ~mask) | bits); }
static bool wait_field(amdgpu_t *a, u32 r, u32 s, u32 m, u32 v, u32 tries) {
    for (u32 i = 0; i < tries; i++) {
        if (((RR(a, r) & m) >> s) == v) return true;
        DELAY_US(1);
    }
    a->dcn_timeouts++;
    return false;
}

u32 amd_dcn_bytes_per_pixel(u32 f) {
    return f == AMD_FMT_RGB565 ? 2 : (f == AMD_FMT_ARGB16161616F || f == AMD_FMT_ABGR16161616F) ? 8 : 4;
}

/* ------------------------------------------------------------ HUBBUB */
static u32 wm(u32 ns, u32 refclk_khz) { return amd_dcnbw_wm_cycles(ns, refclk_khz); }

void amdgpu_dcn_watermarks(amdgpu_t *a, const amd_dcnbw_out_t *bw) {
    u32 ref = a->bw_soc.refclk_khz ? a->bw_soc.refclk_khz : 48000;
    const amd_dcnbw_wm_t *w = bw->wm;
    /* hubbub1_program_urgent/stutter/pstate_watermarks com safe_to_lower: os quatro conjuntos */
    WR(a, D_DCHUBBUB_ARB_DATA_URGENCY_WATERMARK_A, wm(w[0].urgent_ns, ref));
    WR(a, D_DCHUBBUB_ARB_PTE_META_URGENCY_WATERMARK_A, wm(w[0].pte_meta_urgent_ns, ref));
    WR(a, D_DCHUBBUB_ARB_DATA_URGENCY_WATERMARK_B, wm(w[1].urgent_ns, ref));
    WR(a, D_DCHUBBUB_ARB_PTE_META_URGENCY_WATERMARK_B, wm(w[1].pte_meta_urgent_ns, ref));
    WR(a, D_DCHUBBUB_ARB_DATA_URGENCY_WATERMARK_C, wm(w[2].urgent_ns, ref));
    WR(a, D_DCHUBBUB_ARB_PTE_META_URGENCY_WATERMARK_C, wm(w[2].pte_meta_urgent_ns, ref));
    WR(a, D_DCHUBBUB_ARB_DATA_URGENCY_WATERMARK_D, wm(w[3].urgent_ns, ref));
    WR(a, D_DCHUBBUB_ARB_PTE_META_URGENCY_WATERMARK_D, wm(w[3].pte_meta_urgent_ns, ref));
    WR(a, D_DCHUBBUB_ARB_ALLOW_SR_ENTER_WATERMARK_A, wm(w[0].sr_enter_plus_exit_ns, ref));
    WR(a, D_DCHUBBUB_ARB_ALLOW_SR_EXIT_WATERMARK_A, wm(w[0].sr_exit_ns, ref));
    WR(a, D_DCHUBBUB_ARB_ALLOW_SR_ENTER_WATERMARK_B, wm(w[1].sr_enter_plus_exit_ns, ref));
    WR(a, D_DCHUBBUB_ARB_ALLOW_SR_EXIT_WATERMARK_B, wm(w[1].sr_exit_ns, ref));
    WR(a, D_DCHUBBUB_ARB_ALLOW_SR_ENTER_WATERMARK_C, wm(w[2].sr_enter_plus_exit_ns, ref));
    WR(a, D_DCHUBBUB_ARB_ALLOW_SR_EXIT_WATERMARK_C, wm(w[2].sr_exit_ns, ref));
    WR(a, D_DCHUBBUB_ARB_ALLOW_SR_ENTER_WATERMARK_D, wm(w[3].sr_enter_plus_exit_ns, ref));
    WR(a, D_DCHUBBUB_ARB_ALLOW_SR_EXIT_WATERMARK_D, wm(w[3].sr_exit_ns, ref));
    WR(a, D_DCHUBBUB_ARB_ALLOW_DRAM_CLK_CHANGE_WATERMARK_A, wm(w[0].pstate_change_ns, ref));
    WR(a, D_DCHUBBUB_ARB_ALLOW_DRAM_CLK_CHANGE_WATERMARK_B, wm(w[1].pstate_change_ns, ref));
    WR(a, D_DCHUBBUB_ARB_ALLOW_DRAM_CLK_CHANGE_WATERMARK_C, wm(w[2].pstate_change_ns, ref));
    WR(a, D_DCHUBBUB_ARB_ALLOW_DRAM_CLK_CHANGE_WATERMARK_D, wm(w[3].pstate_change_ns, ref));
    UPD(a, D_DCHUBBUB_ARB_SAT_LEVEL, F_DCHUBBUB_ARB_SAT_LEVEL__DCHUBBUB_ARB_SAT_LEVEL, 60 * (ref / 1000));
    UPD(a, D_DCHUBBUB_ARB_DF_REQ_OUTSTAND, F_DCHUBBUB_ARB_DF_REQ_OUTSTAND__DCHUBBUB_ARB_MIN_REQ_OUTSTAND, 68);
    /* stutter permitido (disable_stutter = false no Raven) */
    upd(a, D_DCHUBBUB_ARB_DRAM_STATE_CNTL,
        FV(F_DCHUBBUB_ARB_DRAM_STATE_CNTL__DCHUBBUB_ARB_ALLOW_SELF_REFRESH_FORCE_VALUE, ~0u) |
        FV(F_DCHUBBUB_ARB_DRAM_STATE_CNTL__DCHUBBUB_ARB_ALLOW_SELF_REFRESH_FORCE_ENABLE, ~0u), 0);
}

/* ------------------------------------------------------------ energia de um pipe (DOMAIN 2i = HUBP, 2i+1 = DPP) */
static const u32 *pg_cfg(u32 dom) {
    static const u32 cfg[8] = { D_DOMAIN0_PG_CONFIG, D_DOMAIN1_PG_CONFIG, D_DOMAIN2_PG_CONFIG, D_DOMAIN3_PG_CONFIG,
                                D_DOMAIN4_PG_CONFIG, D_DOMAIN5_PG_CONFIG, D_DOMAIN6_PG_CONFIG, D_DOMAIN7_PG_CONFIG };
    return &cfg[dom];
}
static u32 pg_status(u32 dom) {
    static const u32 st[8] = { D_DOMAIN0_PG_STATUS, D_DOMAIN1_PG_STATUS, D_DOMAIN2_PG_STATUS, D_DOMAIN3_PG_STATUS,
                               D_DOMAIN4_PG_STATUS, D_DOMAIN5_PG_STATUS, D_DOMAIN6_PG_STATUS, D_DOMAIN7_PG_STATUS };
    return st[dom];
}
static void domain_power(amdgpu_t *a, u32 dom, bool on) {
    /* DOMAINn_POWER_GATE e DOMAINn_PGFSM_PWR_STATUS ficam nos mesmos bits em todos os domínios */
    UPD(a, *pg_cfg(dom), F_DOMAIN0_PG_CONFIG__DOMAIN0_POWER_GATE, on ? 0 : 1);
    WAIT(a, pg_status(dom), F_DOMAIN0_PG_STATUS__DOMAIN0_PGFSM_PWR_STATUS, on ? 0 : 2, 1000);
}
static void pipe_power(amdgpu_t *a, u32 p, bool on) {
    WR(a, D_DC_IP_REQUEST_CNTL, FV(F_DC_IP_REQUEST_CNTL__IP_REQUEST_EN, 1));
    domain_power(a, 2 * p + 1, on);      /* DPP */
    domain_power(a, 2 * p, on);          /* HUBP */
    WR(a, D_DC_IP_REQUEST_CNTL, 0);
}

/* ------------------------------------------------------------ OTG */
static void otg_global_sync(amdgpu_t *a, int o, const amd_dcnbw_pipe_t *bw) {
    WR(a, D_OTG_OTG_VSTARTUP_PARAM(o), FV(F_OTG_OTG_VSTARTUP_PARAM__VSTARTUP_START, bw->vstartup_start));
    WR(a, D_OTG_OTG_VUPDATE_PARAM(o), FV(F_OTG_OTG_VUPDATE_PARAM__VUPDATE_OFFSET, bw->vupdate_offset) |
                                      FV(F_OTG_OTG_VUPDATE_PARAM__VUPDATE_WIDTH, bw->vupdate_width));
    WR(a, D_OTG_OTG_VREADY_PARAM(o), FV(F_OTG_OTG_VREADY_PARAM__VREADY_OFFSET, bw->vready_offset));
}

static void otg_vtg_params(amdgpu_t *a, int o, const amd_timing_t *t, u32 vstartup) {
    u32 vfp = t->v_front_porch < 1 ? 1 : t->v_front_porch;            /* apply_front_porch_workaround */
    u32 v_init = t->v_total - vfp;
    i32 asic_blank_end = (i32)(v_init - t->v_active);
    i32 line_start = asic_blank_end - (i32)vstartup + 1;
    u32 fp2 = line_start < 0 ? (u32)-line_start : 0;
    upd(a, D_VTG_CONTROL(o), FV(F_VTG_CONTROL__VTG0_FP2, ~0u) | FV(F_VTG_CONTROL__VTG0_VCOUNT_INIT, ~0u),
        FV(F_VTG_CONTROL__VTG0_FP2, fp2) | FV(F_VTG_CONTROL__VTG0_VCOUNT_INIT, v_init));
}

static void otg_clock(amdgpu_t *a, int o, bool on) {
    if (on) {
        upd(a, D_ODM_OPTC_INPUT_CLOCK_CONTROL(o), FV(F_ODM_OPTC_INPUT_CLOCK_CONTROL__OPTC_INPUT_CLK_EN, ~0u) |
            FV(F_ODM_OPTC_INPUT_CLOCK_CONTROL__OPTC_INPUT_CLK_GATE_DIS, ~0u),
            FV(F_ODM_OPTC_INPUT_CLOCK_CONTROL__OPTC_INPUT_CLK_EN, 1) | FV(F_ODM_OPTC_INPUT_CLOCK_CONTROL__OPTC_INPUT_CLK_GATE_DIS, 1));
        WAIT(a, D_ODM_OPTC_INPUT_CLOCK_CONTROL(o), F_ODM_OPTC_INPUT_CLOCK_CONTROL__OPTC_INPUT_CLK_ON, 1, 1000);
        upd(a, D_OTG_OTG_CLOCK_CONTROL(o), FV(F_OTG_OTG_CLOCK_CONTROL__OTG_CLOCK_EN, ~0u) | FV(F_OTG_OTG_CLOCK_CONTROL__OTG_CLOCK_GATE_DIS, ~0u),
            FV(F_OTG_OTG_CLOCK_CONTROL__OTG_CLOCK_EN, 1) | FV(F_OTG_OTG_CLOCK_CONTROL__OTG_CLOCK_GATE_DIS, 1));
        WAIT(a, D_OTG_OTG_CLOCK_CONTROL(o), F_OTG_OTG_CLOCK_CONTROL__OTG_CLOCK_ON, 1, 1000);
    } else {
        /* última chance de limpar o underflow antes do relógio parar */
        if (FG(RR(a, D_ODM_OPTC_INPUT_GLOBAL_CONTROL(o)), F_ODM_OPTC_INPUT_GLOBAL_CONTROL__OPTC_UNDERFLOW_OCCURRED_STATUS))
            UPD(a, D_ODM_OPTC_INPUT_GLOBAL_CONTROL(o), F_ODM_OPTC_INPUT_GLOBAL_CONTROL__OPTC_UNDERFLOW_CLEAR, 1);
        upd(a, D_OTG_OTG_CLOCK_CONTROL(o), FV(F_OTG_OTG_CLOCK_CONTROL__OTG_CLOCK_EN, ~0u) | FV(F_OTG_OTG_CLOCK_CONTROL__OTG_CLOCK_GATE_DIS, ~0u), 0);
        upd(a, D_ODM_OPTC_INPUT_CLOCK_CONTROL(o), FV(F_ODM_OPTC_INPUT_CLOCK_CONTROL__OPTC_INPUT_CLK_EN, ~0u) |
            FV(F_ODM_OPTC_INPUT_CLOCK_CONTROL__OPTC_INPUT_CLK_GATE_DIS, ~0u), 0);
    }
}

static void otg_blank(amdgpu_t *a, int o, bool blank) {
    upd(a, D_OTG_OTG_BLANK_CONTROL(o), FV(F_OTG_OTG_BLANK_CONTROL__OTG_BLANK_DATA_EN, ~0u) | FV(F_OTG_OTG_BLANK_CONTROL__OTG_BLANK_DE_MODE, ~0u),
        FV(F_OTG_OTG_BLANK_CONTROL__OTG_BLANK_DATA_EN, blank ? 1 : 0));
    if (blank) UPD(a, D_OTG_OTG_DOUBLE_BUFFER_CONTROL(o), F_OTG_OTG_DOUBLE_BUFFER_CONTROL__OTG_BLANK_DATA_DOUBLE_BUFFER_EN, 0);
    else if (FG(RR(a, D_ODM_OPTC_INPUT_GLOBAL_CONTROL(o)), F_ODM_OPTC_INPUT_GLOBAL_CONTROL__OPTC_UNDERFLOW_OCCURRED_STATUS))
        UPD(a, D_ODM_OPTC_INPUT_GLOBAL_CONTROL(o), F_ODM_OPTC_INPUT_GLOBAL_CONTROL__OPTC_UNDERFLOW_CLEAR, 1);
}

static void otg_program_timing(amdgpu_t *a, int o, const amd_dcn_stream_t *s) {
    const amd_timing_t *t = &s->t;
    u32 vfp = t->v_front_porch < 1 ? 1 : t->v_front_porch;
    WR(a, D_OTG_OTG_H_TOTAL(o), FV(F_OTG_OTG_H_TOTAL__OTG_H_TOTAL, t->h_total - 1));
    upd(a, D_OTG_OTG_H_SYNC_A(o), FV(F_OTG_OTG_H_SYNC_A__OTG_H_SYNC_A_START, ~0u) | FV(F_OTG_OTG_H_SYNC_A__OTG_H_SYNC_A_END, ~0u),
        FV(F_OTG_OTG_H_SYNC_A__OTG_H_SYNC_A_END, t->h_sync_width));
    u32 hbs = t->h_total - t->h_front_porch, hbe = hbs - t->h_active;
    upd(a, D_OTG_OTG_H_BLANK_START_END(o), FV(F_OTG_OTG_H_BLANK_START_END__OTG_H_BLANK_START, ~0u) | FV(F_OTG_OTG_H_BLANK_START_END__OTG_H_BLANK_END, ~0u),
        FV(F_OTG_OTG_H_BLANK_START_END__OTG_H_BLANK_START, hbs) | FV(F_OTG_OTG_H_BLANK_START_END__OTG_H_BLANK_END, hbe));
    UPD(a, D_OTG_OTG_H_SYNC_A_CNTL(o), F_OTG_OTG_H_SYNC_A_CNTL__OTG_H_SYNC_A_POL, t->hsync_pos ? 0 : 1);
    u32 vt = t->v_total - 1;
    WR(a, D_OTG_OTG_V_TOTAL(o), FV(F_OTG_OTG_V_TOTAL__OTG_V_TOTAL, vt));
    WR(a, D_OTG_OTG_V_TOTAL_MAX(o), FV(F_OTG_OTG_V_TOTAL_MAX__OTG_V_TOTAL_MAX, vt));
    WR(a, D_OTG_OTG_V_TOTAL_MIN(o), FV(F_OTG_OTG_V_TOTAL_MIN__OTG_V_TOTAL_MIN, vt));
    upd(a, D_OTG_OTG_V_SYNC_A(o), FV(F_OTG_OTG_V_SYNC_A__OTG_V_SYNC_A_START, ~0u) | FV(F_OTG_OTG_V_SYNC_A__OTG_V_SYNC_A_END, ~0u),
        FV(F_OTG_OTG_V_SYNC_A__OTG_V_SYNC_A_END, t->v_sync_width));
    u32 vbs = t->v_total - vfp, vbe = vbs - t->v_active;
    upd(a, D_OTG_OTG_V_BLANK_START_END(o), FV(F_OTG_OTG_V_BLANK_START_END__OTG_V_BLANK_START, ~0u) | FV(F_OTG_OTG_V_BLANK_START_END__OTG_V_BLANK_END, ~0u),
        FV(F_OTG_OTG_V_BLANK_START_END__OTG_V_BLANK_START, vbs) | FV(F_OTG_OTG_V_BLANK_START_END__OTG_V_BLANK_END, vbe));
    UPD(a, D_OTG_OTG_V_SYNC_A_CNTL(o), F_OTG_OTG_V_SYNC_A_CNTL__OTG_V_SYNC_A_POL, t->vsync_pos ? 0 : 1);
    UPD(a, D_OTG_OTG_INTERLACE_CONTROL(o), F_OTG_OTG_INTERLACE_CONTROL__OTG_INTERLACE_ENABLE, 0);
    UPD(a, D_VTG_CONTROL(o), F_VTG_CONTROL__VTG0_ENABLE, 0);
    upd(a, D_OTG_OTG_CONTROL(o), FV(F_OTG_OTG_CONTROL__OTG_START_POINT_CNTL, ~0u) | FV(F_OTG_OTG_CONTROL__OTG_FIELD_NUMBER_CNTL, ~0u),
        FV(F_OTG_OTG_CONTROL__OTG_START_POINT_CNTL, s->dp ? 1 : 0));
    otg_global_sync(a, o, s->bw);
    otg_vtg_params(a, o, t, s->bw->vstartup_start);
    /* dois pixels por contêiner só no 4:2:0 */
    UPD(a, D_OTG_OTG_H_TIMING_CNTL(o), F_OTG_OTG_H_TIMING_CNTL__OTG_H_TIMING_DIV_BY2, 0);
}

static void otg_enable(amdgpu_t *a, int o) {
    UPD(a, D_ODM_OPTC_DATA_SOURCE_SELECT(o), F_ODM_OPTC_DATA_SOURCE_SELECT__OPTC_SRC_SEL, (u32)o);
    UPD(a, D_VTG_CONTROL(o), F_VTG_CONTROL__VTG0_ENABLE, 1);
    upd(a, D_OTG_OTG_CONTROL(o), FV(F_OTG_OTG_CONTROL__OTG_DISABLE_POINT_CNTL, ~0u) | FV(F_OTG_OTG_CONTROL__OTG_MASTER_EN, ~0u),
        FV(F_OTG_OTG_CONTROL__OTG_DISABLE_POINT_CNTL, 3) | FV(F_OTG_OTG_CONTROL__OTG_MASTER_EN, 1));
}

/* optc1_lock/unlock: segura as atualizações de duplo buffer do OTG enquanto os pipes são programados */
static void otg_lock(amdgpu_t *a, int o, bool lock) {
    if (lock) {
        WR(a, D_OTG_OTG_GLOBAL_CONTROL0(o), FV(F_OTG_OTG_GLOBAL_CONTROL0__OTG_MASTER_UPDATE_LOCK_SEL, (u32)o));
        WR(a, D_OTG_OTG_MASTER_UPDATE_LOCK(o), FV(F_OTG_OTG_MASTER_UPDATE_LOCK__OTG_MASTER_UPDATE_LOCK, 1));
        WAIT(a, D_OTG_OTG_MASTER_UPDATE_LOCK(o), F_OTG_OTG_MASTER_UPDATE_LOCK__UPDATE_LOCK_STATUS, 1, 10);
    } else {
        WR(a, D_OTG_OTG_MASTER_UPDATE_LOCK(o), 0);
    }
}

void amdgpu_dcn_disable_otg(amdgpu_t *a, int o) {
    if (!a || o < 0 || o >= AMD_DCN_OTGS) return;
    otg_blank(a, o, true);
    upd(a, D_OTG_OTG_CONTROL(o), FV(F_OTG_OTG_CONTROL__OTG_DISABLE_POINT_CNTL, ~0u) | FV(F_OTG_OTG_CONTROL__OTG_MASTER_EN, ~0u),
        FV(F_OTG_OTG_CONTROL__OTG_DISABLE_POINT_CNTL, 3));
    UPD(a, D_VTG_CONTROL(o), F_VTG_CONTROL__VTG0_ENABLE, 0);
    WAIT(a, D_OTG_OTG_CLOCK_CONTROL(o), F_OTG_OTG_CLOCK_CONTROL__OTG_BUSY, 0, 100000);
    otg_clock(a, o, false);
}

/* ------------------------------------------------------------ MPC */
static void mpcc_remove(amdgpu_t *a, u32 id) {
    /* tira o MPCC de qualquer árvore em que esteja (mpc1_remove_mpcc) */
    for (u32 opp = 0; opp < AMD_DCN_OTGS; opp++)
        if (FG(RR(a, D_MPC_OUT_MUX(opp)), F_MPC_OUT_MUX__MPC_OUT_MUX) == id) {
            u32 bot = FG(RR(a, D_MPCC_MPCC_BOT_SEL(id)), F_MPCC_MPCC_BOT_SEL__MPCC_BOT_SEL);
            UPD(a, D_MPC_OUT_MUX(opp), F_MPC_OUT_MUX__MPC_OUT_MUX, bot);
        }
    for (u32 m = 0; m < AMD_DCN_PIPES; m++)
        if (m != id && FG(RR(a, D_MPCC_MPCC_BOT_SEL(m)), F_MPCC_MPCC_BOT_SEL__MPCC_BOT_SEL) == id) {
            u32 bot = FG(RR(a, D_MPCC_MPCC_BOT_SEL(id)), F_MPCC_MPCC_BOT_SEL__MPCC_BOT_SEL);
            WR(a, D_MPCC_MPCC_BOT_SEL(m), FV(F_MPCC_MPCC_BOT_SEL__MPCC_BOT_SEL, bot));
            if (bot == 0xF) UPD(a, D_MPCC_MPCC_CONTROL(m), F_MPCC_MPCC_CONTROL__MPCC_MODE, 2);   /* TOP_LAYER_ONLY */
        }
    WR(a, D_MPCC_MPCC_TOP_SEL(id), FV(F_MPCC_MPCC_TOP_SEL__MPCC_TOP_SEL, 0xF));
    WR(a, D_MPCC_MPCC_BOT_SEL(id), FV(F_MPCC_MPCC_BOT_SEL__MPCC_BOT_SEL, 0xF));
    WR(a, D_MPCC_MPCC_OPP_ID(id), FV(F_MPCC_MPCC_OPP_ID__MPCC_OPP_ID, 0xF));
    WR(a, D_MPCC_MPCC_UPDATE_LOCK_SEL(id), FV(F_MPCC_MPCC_UPDATE_LOCK_SEL__MPCC_UPDATE_LOCK_SEL, 0xF));
}

/* mpc1_insert_plane no fim da árvore da OPP, com mistura por alfa global 0xFF (dcn10_update_mpcc) */
static void mpcc_append(amdgpu_t *a, u32 opp, u32 id, u32 dpp) {
    WR(a, D_MPCC_MPCC_BOT_SEL(id), FV(F_MPCC_MPCC_BOT_SEL__MPCC_BOT_SEL, 0xF));
    UPD(a, D_MPCC_MPCC_CONTROL(id), F_MPCC_MPCC_CONTROL__MPCC_MODE, 2);                 /* TOP_LAYER_ONLY */
    WR(a, D_MPCC_MPCC_TOP_SEL(id), FV(F_MPCC_MPCC_TOP_SEL__MPCC_TOP_SEL, dpp));
    WR(a, D_MPCC_MPCC_OPP_ID(id), FV(F_MPCC_MPCC_OPP_ID__MPCC_OPP_ID, opp));
    WR(a, D_MPCC_MPCC_UPDATE_LOCK_SEL(id), FV(F_MPCC_MPCC_UPDATE_LOCK_SEL__MPCC_UPDATE_LOCK_SEL, opp));
    u32 head = FG(RR(a, D_MPC_OUT_MUX(opp)), F_MPC_OUT_MUX__MPC_OUT_MUX);
    if (head == 0xF) {
        UPD(a, D_MPC_OUT_MUX(opp), F_MPC_OUT_MUX__MPC_OUT_MUX, id);
    } else {
        u32 last = head;
        for (u32 n = 0; n < AMD_DCN_PIPES; n++) {
            u32 b = FG(RR(a, D_MPCC_MPCC_BOT_SEL(last)), F_MPCC_MPCC_BOT_SEL__MPCC_BOT_SEL);
            if (b == 0xF || b == id) break;
            last = b;
        }
        WR(a, D_MPCC_MPCC_BOT_SEL(last), FV(F_MPCC_MPCC_BOT_SEL__MPCC_BOT_SEL, id));
        UPD(a, D_MPCC_MPCC_CONTROL(last), F_MPCC_MPCC_CONTROL__MPCC_MODE, 3);           /* TOP_BOT_BLENDING */
    }
    upd(a, D_MPCC_MPCC_CONTROL(id),
        FV(F_MPCC_MPCC_CONTROL__MPCC_ALPHA_BLND_MODE, ~0u) | FV(F_MPCC_MPCC_CONTROL__MPCC_ALPHA_MULTIPLIED_MODE, ~0u) |
        FV(F_MPCC_MPCC_CONTROL__MPCC_BLND_ACTIVE_OVERLAP_ONLY, ~0u) | FV(F_MPCC_MPCC_CONTROL__MPCC_GLOBAL_ALPHA, ~0u) |
        FV(F_MPCC_MPCC_CONTROL__MPCC_GLOBAL_GAIN, ~0u),
        FV(F_MPCC_MPCC_CONTROL__MPCC_ALPHA_BLND_MODE, 2) | FV(F_MPCC_MPCC_CONTROL__MPCC_GLOBAL_ALPHA, 0xFF) |
        FV(F_MPCC_MPCC_CONTROL__MPCC_GLOBAL_GAIN, 0xFF));
    /* fundo preto no último MPCC da árvore (mpc1_set_bg_color) */
    WR(a, D_MPCC_MPCC_BG_R_CR(id), 0);
    WR(a, D_MPCC_MPCC_BG_G_Y(id), 0);
    WR(a, D_MPCC_MPCC_BG_B_CB(id), 0);
}

/* ------------------------------------------------------------ HUBP */
static u32 tile_bytes(u32 sw) {
    if (sw == 0) return 0;
    if (sw >= 1 && sw <= 3) return 256;
    if ((sw >= 5 && sw <= 7) || (sw >= 21 && sw <= 23)) return 4096;
    if ((sw >= 9 && sw <= 11) || sw == 17 || sw == 18 || (sw >= 25 && sw <= 27)) return 65536;
    return 262144;
}

static u32 surface_format_code(u32 f) {
    switch (f) {
    case AMD_FMT_RGB565:        return 3;
    case AMD_FMT_ARGB2101010:
    case AMD_FMT_ABGR2101010:   return 10;
    case AMD_FMT_ARGB16161616F: return 24;
    case AMD_FMT_ABGR16161616F: return 24;   /* HUBP: o crossbar troca R e B */
    default:                    return 8;
    }
}

static void hubp_vm_settings(amdgpu_t *a, u32 p) {
    /* mmhub_read_vm_system_aperture_settings + hubp1_set_vm_system_aperture_settings */
    u64 def = ((u64)RR(a, R_MC_VM_SYS_APERTURE_DEF_MSB) << 32 | RR(a, R_MC_VM_SYS_APERTURE_DEF_LSB)) & 0xFFFFFFFFFull;
    u64 low = (u64)(RR(a, R_MC_VM_SYS_APERTURE_LOW) & 0x3FFFFFFF) << 18;
    u64 high = (u64)(RR(a, R_MC_VM_SYS_APERTURE_HIGH) & 0x3FFFFFFF) << 18;
    u64 dpn = def, lpn = low >> 12, hpn = high >> 12;
    WR(a, D_HUBPREQ_DCN_VM_SYSTEM_APERTURE_DEFAULT_ADDR_MSB(p),
       FV(F_HUBPREQ_DCN_VM_SYSTEM_APERTURE_DEFAULT_ADDR_MSB__MC_VM_SYSTEM_APERTURE_DEFAULT_SYSTEM, 1) |
       FV(F_HUBPREQ_DCN_VM_SYSTEM_APERTURE_DEFAULT_ADDR_MSB__MC_VM_SYSTEM_APERTURE_DEFAULT_ADDR_MSB, (u32)(dpn >> 32)));
    WR(a, D_HUBPREQ_DCN_VM_SYSTEM_APERTURE_DEFAULT_ADDR_LSB(p), (u32)dpn);
    WR(a, D_HUBPREQ_DCN_VM_SYSTEM_APERTURE_LOW_ADDR_MSB(p), (u32)(lpn >> 32));
    WR(a, D_HUBPREQ_DCN_VM_SYSTEM_APERTURE_LOW_ADDR_LSB(p), (u32)lpn);
    WR(a, D_HUBPREQ_DCN_VM_SYSTEM_APERTURE_HIGH_ADDR_MSB(p), (u32)(hpn >> 32));
    WR(a, D_HUBPREQ_DCN_VM_SYSTEM_APERTURE_HIGH_ADDR_LSB(p), (u32)hpn);
    /* mmhub_read_vm_context0_settings + hubp1_set_vm_context0_settings: a GART do contexto 0 */
    u64 base = (u64)RR(a, R_VM_CTX0_PT_BASE_HI) << 32 | RR(a, R_VM_CTX0_PT_BASE_LO);
    u64 fb_base = (u64)FG(RR(a, D_DCHUBBUB_SDPIF_FB_BASE), F_DCHUBBUB_SDPIF_FB_BASE__SDPIF_FB_BASE) << 24;
    u64 fb_off = (u64)FG(RR(a, D_DCHUBBUB_SDPIF_FB_OFFSET), F_DCHUBBUB_SDPIF_FB_OFFSET__SDPIF_FB_OFFSET) << 24;
    base = base + fb_base - fb_off;
    WR(a, D_HUBPREQ_DCN_VM_CONTEXT0_PAGE_TABLE_BASE_ADDR_MSB(p), (u32)(base >> 32));
    WR(a, D_HUBPREQ_DCN_VM_CONTEXT0_PAGE_TABLE_BASE_ADDR_LSB(p), (u32)base);
    WR(a, D_HUBPREQ_DCN_VM_CONTEXT0_PAGE_TABLE_START_ADDR_MSB(p), RR(a, R_VM_CTX0_PT_START_HI) & 0xF);
    WR(a, D_HUBPREQ_DCN_VM_CONTEXT0_PAGE_TABLE_START_ADDR_LSB(p), RR(a, R_VM_CTX0_PT_START_LO));
    WR(a, D_HUBPREQ_DCN_VM_CONTEXT0_PAGE_TABLE_END_ADDR_MSB(p), RR(a, R_VM_CTX0_PT_END_HI) & 0xF);
    WR(a, D_HUBPREQ_DCN_VM_CONTEXT0_PAGE_TABLE_END_ADDR_LSB(p), RR(a, R_VM_CTX0_PT_END_LO));
    WR(a, D_HUBPREQ_DCN_VM_CONTEXT0_PROTECTION_FAULT_DEFAULT_ADDR_MSB(p),
       FV(F_HUBPREQ_DCN_VM_CONTEXT0_PROTECTION_FAULT_DEFAULT_ADDR_MSB__VM_CONTEXT0_PROTECTION_FAULT_DEFAULT_ADDR_MSB,
          RR(a, R_VM_L2_FAULT_DEFAULT_HI) & 0xF));
    WR(a, D_HUBPREQ_DCN_VM_CONTEXT0_PROTECTION_FAULT_DEFAULT_ADDR_LSB(p), RR(a, R_VM_L2_FAULT_DEFAULT_LO));
    WR(a, D_HUBPREQ_DCN_VM_MX_L1_TLB_CNTL(p), FV(F_HUBPREQ_DCN_VM_MX_L1_TLB_CNTL__ENABLE_L1_TLB, 1) |
                                              FV(F_HUBPREQ_DCN_VM_MX_L1_TLB_CNTL__SYSTEM_ACCESS_MODE, 3));
}

static void hubp_set_blank(amdgpu_t *a, u32 p, bool blank) {
    upd(a, D_HUBP_DCHUBP_CNTL(p), FV(F_HUBP_DCHUBP_CNTL__HUBP_BLANK_EN, ~0u) | FV(F_HUBP_DCHUBP_CNTL__HUBP_TTU_DISABLE, ~0u),
        FV(F_HUBP_DCHUBP_CNTL__HUBP_BLANK_EN, blank) | FV(F_HUBP_DCHUBP_CNTL__HUBP_TTU_DISABLE, blank));
    if (blank && RR(a, D_HUBP_DCHUBP_CNTL(p)))
        WAIT(a, D_HUBP_DCHUBP_CNTL(p), F_HUBP_DCHUBP_CNTL__HUBP_NO_OUTSTANDING_REQ, 1, 200);
}

static void hubp_requestor(amdgpu_t *a, u32 p, const amd_dml1_rq_t *rq) {
    UPD(a, D_HUBPRET_HUBPRET_CONTROL(p), F_HUBPRET_HUBPRET_CONTROL__DET_BUF_PLANE1_BASE_ADDRESS, 0);   /* um plano só */
    WR(a, D_HUBPREQ_DCN_EXPANSION_MODE(p), FV(F_HUBPREQ_DCN_EXPANSION_MODE__DRQ_EXPANSION_MODE, rq->regs.drq_expansion_mode) |
       FV(F_HUBPREQ_DCN_EXPANSION_MODE__PRQ_EXPANSION_MODE, rq->regs.prq_expansion_mode) |
       FV(F_HUBPREQ_DCN_EXPANSION_MODE__MRQ_EXPANSION_MODE, rq->regs.mrq_expansion_mode) |
       FV(F_HUBPREQ_DCN_EXPANSION_MODE__CRQ_EXPANSION_MODE, rq->regs.crq_expansion_mode));
    WR(a, D_HUBP_DCHUBP_REQ_SIZE_CONFIG(p), amd_dml1_req_size_config(rq));
    WR(a, D_HUBP_DCHUBP_REQ_SIZE_CONFIG_C(p), 0);
}

static void hubp_deadline(amdgpu_t *a, u32 p, const amd_dml1_dlg_t *d) {
    WR(a, D_HUBPREQ_BLANK_OFFSET_0(p), FV(F_HUBPREQ_BLANK_OFFSET_0__REFCYC_H_BLANK_END, d->refcyc_h_blank_end) |
                                       FV(F_HUBPREQ_BLANK_OFFSET_0__DLG_V_BLANK_END, d->dlg_vblank_end));
    WR(a, D_HUBPREQ_BLANK_OFFSET_1(p), FV(F_HUBPREQ_BLANK_OFFSET_1__MIN_DST_Y_NEXT_START, d->min_dst_y_next_start));
    WR(a, D_HUBPREQ_DST_DIMENSIONS(p), FV(F_HUBPREQ_DST_DIMENSIONS__REFCYC_PER_HTOTAL, d->refcyc_per_htotal));
    WR(a, D_HUBPREQ_DST_AFTER_SCALER(p), FV(F_HUBPREQ_DST_AFTER_SCALER__REFCYC_X_AFTER_SCALER, d->refcyc_x_after_scaler) |
                                         FV(F_HUBPREQ_DST_AFTER_SCALER__DST_Y_AFTER_SCALER, d->dst_y_after_scaler));
    WR(a, D_HUBPREQ_REF_FREQ_TO_PIX_FREQ(p), FV(F_HUBPREQ_REF_FREQ_TO_PIX_FREQ__REF_FREQ_TO_PIX_FREQ, d->ref_freq_to_pix_freq));
    WR(a, D_HUBPREQ_VBLANK_PARAMETERS_1(p), FV(F_HUBPREQ_VBLANK_PARAMETERS_1__REFCYC_PER_PTE_GROUP_VBLANK_L, d->refcyc_per_pte_group_vblank_l));
    WR(a, D_HUBPREQ_NOM_PARAMETERS_0(p), FV(F_HUBPREQ_NOM_PARAMETERS_0__DST_Y_PER_PTE_ROW_NOM_L, d->dst_y_per_pte_row_nom_l));
    WR(a, D_HUBPREQ_NOM_PARAMETERS_1(p), FV(F_HUBPREQ_NOM_PARAMETERS_1__REFCYC_PER_PTE_GROUP_NOM_L, d->refcyc_per_pte_group_nom_l));
    WR(a, D_HUBPREQ_NOM_PARAMETERS_4(p), FV(F_HUBPREQ_NOM_PARAMETERS_4__DST_Y_PER_META_ROW_NOM_L, d->dst_y_per_meta_row_nom_l));
    WR(a, D_HUBPREQ_NOM_PARAMETERS_5(p), FV(F_HUBPREQ_NOM_PARAMETERS_5__REFCYC_PER_META_CHUNK_NOM_L, d->refcyc_per_meta_chunk_nom_l));
    WR(a, D_HUBPREQ_PER_LINE_DELIVERY(p), FV(F_HUBPREQ_PER_LINE_DELIVERY__REFCYC_PER_LINE_DELIVERY_L, d->refcyc_per_line_delivery_l));
    WR(a, D_HUBPREQ_VBLANK_PARAMETERS_2(p), 0);
    WR(a, D_HUBPREQ_NOM_PARAMETERS_2(p), 0);
    WR(a, D_HUBPREQ_NOM_PARAMETERS_3(p), 0);
    WR(a, D_HUBPREQ_NOM_PARAMETERS_6(p), FV(F_HUBPREQ_NOM_PARAMETERS_6__DST_Y_PER_META_ROW_NOM_C, d->dst_y_per_meta_row_nom_c));
    WR(a, D_HUBPREQ_NOM_PARAMETERS_7(p), 0);
    WR(a, D_HUBPREQ_DCN_TTU_QOS_WM(p), FV(F_HUBPREQ_DCN_TTU_QOS_WM__QoS_LEVEL_LOW_WM, d->qos_level_low_wm) |
                                       FV(F_HUBPREQ_DCN_TTU_QOS_WM__QoS_LEVEL_HIGH_WM, d->qos_level_high_wm));
    WR(a, D_HUBPREQ_DCN_SURF0_TTU_CNTL0(p), FV(F_HUBPREQ_DCN_SURF0_TTU_CNTL0__REFCYC_PER_REQ_DELIVERY, d->refcyc_per_req_delivery_l) |
                                            FV(F_HUBPREQ_DCN_SURF0_TTU_CNTL0__QoS_LEVEL_FIXED, d->qos_level_fixed_l));
    WR(a, D_HUBPREQ_DCN_SURF1_TTU_CNTL0(p), FV(F_HUBPREQ_DCN_SURF1_TTU_CNTL0__QoS_LEVEL_FIXED, d->qos_level_fixed_c));
    WR(a, D_HUBPREQ_DCN_CUR0_TTU_CNTL0(p), FV(F_HUBPREQ_DCN_CUR0_TTU_CNTL0__QoS_LEVEL_FIXED, 8));     /* sem cursor de hardware */
}

static void hubp_interdependent(amdgpu_t *a, u32 p, const amd_dml1_dlg_t *d) {
    WR(a, D_HUBPREQ_PREFETCH_SETTINS(p), FV(F_HUBPREQ_PREFETCH_SETTINS__DST_Y_PREFETCH, d->dst_y_prefetch) |
                                         FV(F_HUBPREQ_PREFETCH_SETTINS__VRATIO_PREFETCH, d->vratio_prefetch));
    WR(a, D_HUBPREQ_PREFETCH_SETTINS_C(p), FV(F_HUBPREQ_PREFETCH_SETTINS_C__VRATIO_PREFETCH_C, d->vratio_prefetch_c));
    WR(a, D_HUBPREQ_VBLANK_PARAMETERS_0(p), FV(F_HUBPREQ_VBLANK_PARAMETERS_0__DST_Y_PER_VM_VBLANK, d->dst_y_per_vm_vblank) |
                                            FV(F_HUBPREQ_VBLANK_PARAMETERS_0__DST_Y_PER_ROW_VBLANK, d->dst_y_per_row_vblank));
    WR(a, D_HUBPREQ_VBLANK_PARAMETERS_3(p), FV(F_HUBPREQ_VBLANK_PARAMETERS_3__REFCYC_PER_META_CHUNK_VBLANK_L, d->refcyc_per_meta_chunk_vblank_l));
    WR(a, D_HUBPREQ_VBLANK_PARAMETERS_4(p), FV(F_HUBPREQ_VBLANK_PARAMETERS_4__REFCYC_PER_META_CHUNK_VBLANK_C, d->refcyc_per_meta_chunk_vblank_c));
    WR(a, D_HUBPREQ_PER_LINE_DELIVERY_PRE(p), FV(F_HUBPREQ_PER_LINE_DELIVERY_PRE__REFCYC_PER_LINE_DELIVERY_PRE_L, d->refcyc_per_line_delivery_pre_l));
    WR(a, D_HUBPREQ_DCN_SURF0_TTU_CNTL1(p), FV(F_HUBPREQ_DCN_SURF0_TTU_CNTL1__REFCYC_PER_REQ_DELIVERY_PRE, d->refcyc_per_req_delivery_pre_l));
    WR(a, D_HUBPREQ_DCN_SURF1_TTU_CNTL1(p), 0);
    WR(a, D_HUBPREQ_DCN_CUR0_TTU_CNTL1(p), 0);
    WR(a, D_HUBPREQ_DCN_GLOBAL_TTU_CNTL(p), FV(F_HUBPREQ_DCN_GLOBAL_TTU_CNTL__MIN_TTU_VBLANK, d->min_ttu_vblank) |
                                            FV(F_HUBPREQ_DCN_GLOBAL_TTU_CNTL__QoS_LEVEL_FLIP, d->qos_level_flip));
}

/* hubp1_vready_workaround */
static void hubp_vready_wa(amdgpu_t *a, u32 p, const amd_dml1_dlg_in_t *in, u32 dlg_vblank_end) {
    u32 v = RR(a, D_HUBP_HUBPREQ_DEBUG_DB(p));
    v |= 0x100;
    v &= ~0x1000u;
    if ((i32)in->vstartup_start - (i32)(2 * (in->vready_offset + in->vupdate_width + in->vupdate_offset) / in->htotal) <= (i32)dlg_vblank_end)
        v |= 0x1000;
    WR(a, D_HUBP_HUBPREQ_DEBUG_DB(p), v);
}

static void hubp_surface(amdgpu_t *a, u32 p, const amd_dcn_stream_t *s) {
    /* hubp1_dcc_control */
    u32 dcc = s->dcc ? 1 : 0, ind = s->dcc && s->dcc_ind_64b ? 1 : 0;
    upd(a, D_HUBPREQ_DCSURF_SURFACE_CONTROL(p),
        FV(F_HUBPREQ_DCSURF_SURFACE_CONTROL__PRIMARY_SURFACE_DCC_EN, ~0u) | FV(F_HUBPREQ_DCSURF_SURFACE_CONTROL__PRIMARY_SURFACE_DCC_IND_64B_BLK, ~0u) |
        FV(F_HUBPREQ_DCSURF_SURFACE_CONTROL__SECONDARY_SURFACE_DCC_EN, ~0u) | FV(F_HUBPREQ_DCSURF_SURFACE_CONTROL__SECONDARY_SURFACE_DCC_IND_64B_BLK, ~0u),
        FV(F_HUBPREQ_DCSURF_SURFACE_CONTROL__PRIMARY_SURFACE_DCC_EN, dcc) | FV(F_HUBPREQ_DCSURF_SURFACE_CONTROL__PRIMARY_SURFACE_DCC_IND_64B_BLK, ind) |
        FV(F_HUBPREQ_DCSURF_SURFACE_CONTROL__SECONDARY_SURFACE_DCC_EN, dcc) | FV(F_HUBPREQ_DCSURF_SURFACE_CONTROL__SECONDARY_SURFACE_DCC_IND_64B_BLK, ind));
    /* hubp1_program_tiling: GB_ADDR_CONFIG do GC (mesmo valor dourado do gfx.c se o GC não subiu) */
    u32 gb = a->gb_addr_config ? a->gb_addr_config : (a->variant == AMD_RAVEN2 ? 0x26013041u : 0x24000042u);
    upd(a, D_HUBP_DCSURF_ADDR_CONFIG(p),
        FV(F_HUBP_DCSURF_ADDR_CONFIG__NUM_PIPES, ~0u) | FV(F_HUBP_DCSURF_ADDR_CONFIG__NUM_BANKS, ~0u) |
        FV(F_HUBP_DCSURF_ADDR_CONFIG__PIPE_INTERLEAVE, ~0u) | FV(F_HUBP_DCSURF_ADDR_CONFIG__NUM_SE, ~0u) |
        FV(F_HUBP_DCSURF_ADDR_CONFIG__NUM_RB_PER_SE, ~0u) | FV(F_HUBP_DCSURF_ADDR_CONFIG__MAX_COMPRESSED_FRAGS, ~0u),
        FV(F_HUBP_DCSURF_ADDR_CONFIG__NUM_PIPES, gb & 7) | FV(F_HUBP_DCSURF_ADDR_CONFIG__NUM_BANKS, (gb >> 12) & 7) |
        FV(F_HUBP_DCSURF_ADDR_CONFIG__PIPE_INTERLEAVE, 0) | FV(F_HUBP_DCSURF_ADDR_CONFIG__NUM_SE, (gb >> 19) & 3) |
        FV(F_HUBP_DCSURF_ADDR_CONFIG__NUM_RB_PER_SE, (gb >> 26) & 3) | FV(F_HUBP_DCSURF_ADDR_CONFIG__MAX_COMPRESSED_FRAGS, (gb >> 6) & 3));
    upd(a, D_HUBP_DCSURF_TILING_CONFIG(p),
        FV(F_HUBP_DCSURF_TILING_CONFIG__SW_MODE, ~0u) | FV(F_HUBP_DCSURF_TILING_CONFIG__META_LINEAR, ~0u) |
        FV(F_HUBP_DCSURF_TILING_CONFIG__RB_ALIGNED, ~0u) | FV(F_HUBP_DCSURF_TILING_CONFIG__PIPE_ALIGNED, ~0u),
        FV(F_HUBP_DCSURF_TILING_CONFIG__SW_MODE, s->sw_mode) | FV(F_HUBP_DCSURF_TILING_CONFIG__RB_ALIGNED, s->rb_aligned) |
        FV(F_HUBP_DCSURF_TILING_CONFIG__PIPE_ALIGNED, s->pipe_aligned));
    /* hubp1_program_size */
    upd(a, D_HUBPREQ_DCSURF_SURFACE_PITCH(p), FV(F_HUBPREQ_DCSURF_SURFACE_PITCH__PITCH, ~0u) | FV(F_HUBPREQ_DCSURF_SURFACE_PITCH__META_PITCH, ~0u),
        FV(F_HUBPREQ_DCSURF_SURFACE_PITCH__PITCH, s->pitch_px - 1) |
        FV(F_HUBPREQ_DCSURF_SURFACE_PITCH__META_PITCH, s->dcc && s->meta_pitch_px ? s->meta_pitch_px - 1 : 0));
    upd(a, D_HUBPREQ_DCSURF_SURFACE_PITCH_C(p), FV(F_HUBPREQ_DCSURF_SURFACE_PITCH_C__PITCH_C, ~0u) | FV(F_HUBPREQ_DCSURF_SURFACE_PITCH_C__META_PITCH_C, ~0u), 0);
    /* rotação 0, sem espelho; formato e crossbar */
    upd(a, D_HUBP_DCSURF_SURFACE_CONFIG(p), FV(F_HUBP_DCSURF_SURFACE_CONFIG__ROTATION_ANGLE, ~0u) | FV(F_HUBP_DCSURF_SURFACE_CONFIG__H_MIRROR_EN, ~0u), 0);
    bool abgr = s->format == AMD_FMT_ABGR8888 || s->format == AMD_FMT_ABGR2101010 || s->format == AMD_FMT_ABGR16161616F;
    upd(a, D_HUBPRET_HUBPRET_CONTROL(p), FV(F_HUBPRET_HUBPRET_CONTROL__CROSSBAR_SRC_CB_B, ~0u) | FV(F_HUBPRET_HUBPRET_CONTROL__CROSSBAR_SRC_CR_R, ~0u),
        FV(F_HUBPRET_HUBPRET_CONTROL__CROSSBAR_SRC_CB_B, abgr ? 3 : 2) | FV(F_HUBPRET_HUBPRET_CONTROL__CROSSBAR_SRC_CR_R, abgr ? 2 : 3));
    UPD(a, D_HUBP_DCSURF_SURFACE_CONFIG(p), F_HUBP_DCSURF_SURFACE_CONFIG__SURFACE_PIXEL_FORMAT, surface_format_code(s->format));
}

static void hubp_address(amdgpu_t *a, u32 p, u64 addr, u64 meta) {
    upd(a, D_HUBPREQ_DCSURF_FLIP_CONTROL(p),
        FV(F_HUBPREQ_DCSURF_FLIP_CONTROL__SURFACE_FLIP_TYPE, ~0u) | FV(F_HUBPREQ_DCSURF_FLIP_CONTROL__SURFACE_FLIP_MODE_FOR_STEREOSYNC, ~0u) |
        FV(F_HUBPREQ_DCSURF_FLIP_CONTROL__SURFACE_FLIP_IN_STEREOSYNC, ~0u), 0);           /* troca no vsync */
    upd(a, D_HUBPREQ_DCSURF_SURFACE_CONTROL(p),
        FV(F_HUBPREQ_DCSURF_SURFACE_CONTROL__PRIMARY_SURFACE_TMZ, ~0u) | FV(F_HUBPREQ_DCSURF_SURFACE_CONTROL__PRIMARY_META_SURFACE_TMZ, ~0u), 0);
    if (meta) {
        WR(a, D_HUBPREQ_DCSURF_PRIMARY_META_SURFACE_ADDRESS_HIGH(p), (u32)(meta >> 32) & 0xFFFF);
        WR(a, D_HUBPREQ_DCSURF_PRIMARY_META_SURFACE_ADDRESS(p), (u32)meta);
    }
    WR(a, D_HUBPREQ_DCSURF_PRIMARY_SURFACE_ADDRESS_HIGH(p), (u32)(addr >> 32) & 0xFFFF);
    WR(a, D_HUBPREQ_DCSURF_PRIMARY_SURFACE_ADDRESS(p), (u32)addr);   /* o baixo por último: dispara a troca */
}

static void hubp_viewport(amdgpu_t *a, u32 p, u32 x, u32 w, u32 h) {
    u32 dim = FV(F_HUBP_DCSURF_PRI_VIEWPORT_DIMENSION__PRI_VIEWPORT_WIDTH, w) | FV(F_HUBP_DCSURF_PRI_VIEWPORT_DIMENSION__PRI_VIEWPORT_HEIGHT, h);
    u32 st = FV(F_HUBP_DCSURF_PRI_VIEWPORT_START__PRI_VIEWPORT_X_START, x);
    WR(a, D_HUBP_DCSURF_PRI_VIEWPORT_DIMENSION(p), dim);
    WR(a, D_HUBP_DCSURF_PRI_VIEWPORT_START(p), st);
    WR(a, D_HUBP_DCSURF_SEC_VIEWPORT_DIMENSION(p), dim);
    WR(a, D_HUBP_DCSURF_SEC_VIEWPORT_START(p), st);
    /* RGB: o viewport do croma é o mesmo (pipe_ctx_to_e2e/resource_build_scaling_params) */
    WR(a, D_HUBP_DCSURF_PRI_VIEWPORT_DIMENSION_C(p), dim);
    WR(a, D_HUBP_DCSURF_PRI_VIEWPORT_START_C(p), st);
    WR(a, D_HUBP_DCSURF_SEC_VIEWPORT_DIMENSION_C(p), dim);
    WR(a, D_HUBP_DCSURF_SEC_VIEWPORT_START_C(p), st);
}

/* ------------------------------------------------------------ DPP */
static void dpp_clock(amdgpu_t *a, u32 p, bool div2, bool on) {
    if (on)
        upd(a, D_DPP_TOP_DPP_CONTROL(p), FV(F_DPP_TOP_DPP_CONTROL__DPPCLK_RATE_CONTROL, ~0u) | FV(F_DPP_TOP_DPP_CONTROL__DPP_CLOCK_ENABLE, ~0u),
            FV(F_DPP_TOP_DPP_CONTROL__DPPCLK_RATE_CONTROL, div2) | FV(F_DPP_TOP_DPP_CONTROL__DPP_CLOCK_ENABLE, 1));
    else
        UPD(a, D_DPP_TOP_DPP_CONTROL(p), F_DPP_TOP_DPP_CONTROL__DPP_CLOCK_ENABLE, 0);
}

static void dpp_input(amdgpu_t *a, u32 p, u32 fmt) {
    bool fp = fmt == AMD_FMT_ARGB16161616F || fmt == AMD_FMT_ABGR16161616F;
    /* dpp1_cnv_setup com EXPANSION_MODE_ZERO */
    WR(a, D_CNVC_CFG_FORMAT_CONTROL(p), FV(F_CNVC_CFG_FORMAT_CONTROL__CNVC_BYPASS, 0) |
       FV(F_CNVC_CFG_FORMAT_CONTROL__FORMAT_EXPANSION_MODE, 1) | FV(F_CNVC_CFG_FORMAT_CONTROL__OUTPUT_FP, fp));
    upd(a, D_CM_CM_IGAM_CONTROL(p), FV(F_CM_CM_IGAM_CONTROL__CM_IGAM_INPUT_FORMAT, ~0u) | FV(F_CM_CM_IGAM_CONTROL__CM_IGAM_LUT_MODE, ~0u),
        FV(F_CM_CM_IGAM_CONTROL__CM_IGAM_INPUT_FORMAT, fp ? 3 : 2) | FV(F_CM_CM_IGAM_CONTROL__CM_IGAM_LUT_MODE, fp ? 1 : 0));
    u32 pf = fmt == AMD_FMT_RGB565 ? 3 : (fmt == AMD_FMT_ARGB2101010 || fmt == AMD_FMT_ABGR2101010) ? 10 :
             fmt == AMD_FMT_ARGB16161616F ? 24 : fmt == AMD_FMT_ABGR16161616F ? 25 : 8;
    WR(a, D_CNVC_CFG_CNVC_SURFACE_PIXEL_FORMAT(p), FV(F_CNVC_CFG_CNVC_SURFACE_PIXEL_FORMAT__CNVC_SURFACE_PIXEL_FORMAT, pf));
    UPD(a, D_CNVC_CFG_FORMAT_CONTROL(p), F_CNVC_CFG_FORMAT_CONTROL__ALPHA_EN, fmt == AMD_FMT_RGB565 ? 0 : 1);
    WR(a, D_CM_CM_ICSC_CONTROL(p), 0);                                               /* RGB: CSC de entrada em bypass */
    /* build_prescale_params: escala 1.0 (0x2000), sem viés */
    u32 bns = FV(F_CM_CM_BNS_VALUES_R__CM_BNS_SCALE_R, 0x2000);
    WR(a, D_CM_CM_BNS_VALUES_R(p), bns);
    WR(a, D_CM_CM_BNS_VALUES_G(p), bns);
    WR(a, D_CM_CM_BNS_VALUES_B(p), bns);
}

static void dpp_scaler(amdgpu_t *a, u32 p, u32 rx, u32 rw, u32 vw, u32 h, u32 mpc_w) {
    WR(a, D_DSCL_DSCL_AUTOCAL(p), 0);
    WR(a, D_DSCL_DSCL_CONTROL(p), 0);
    WR(a, D_DSCL_RECOUT_START(p), FV(F_DSCL_RECOUT_START__RECOUT_START_X, rx));
    WR(a, D_DSCL_RECOUT_SIZE(p), FV(F_DSCL_RECOUT_SIZE__RECOUT_WIDTH, rw) | FV(F_DSCL_RECOUT_SIZE__RECOUT_HEIGHT, h));
    WR(a, D_DSCL_MPC_SIZE(p), FV(F_DSCL_MPC_SIZE__MPC_WIDTH, mpc_w) | FV(F_DSCL_MPC_SIZE__MPC_HEIGHT, h));
    UPD(a, D_DSCL_SCL_MODE(p), F_DSCL_SCL_MODE__DSCL_MODE, 0);                       /* SCALING_444_BYPASS */
    /* dpp1_dscl_find_lb_memory_config sem escala (1 tap): a primeira config cabe sempre que houver 1 partição */
    u32 line = vw < rw ? vw : rw;
    if (!line) line = 1;
    u32 mem_y = (line * 12 + 71) / 72;                                                /* LB de 36 bpp: 12 bpc */
    u32 cfg = (816 / mem_y >= 1) ? 1 : (1088 / mem_y >= 1) ? 2 : 0;
    WR(a, D_DSCL_LB_DATA_FORMAT(p), FV(F_DSCL_LB_DATA_FORMAT__PIXEL_DEPTH, 3) | FV(F_DSCL_LB_DATA_FORMAT__PIXEL_REDUCE_MODE, 1));
    WR(a, D_DSCL_LB_MEMORY_CTRL(p), FV(F_DSCL_LB_MEMORY_CTRL__MEMORY_CONFIG, cfg) | FV(F_DSCL_LB_MEMORY_CTRL__LB_MAX_PARTITIONS, 63));
}

/* output_csc_matrix do dc_hw_sequencer.c (RGB faixa cheia, YCbCr BT.601, YCbCr BT.709), formato S2.13 */
static const u16 ocsc_rgb[12]    = { 0x2000, 0, 0, 0, 0, 0x2000, 0, 0, 0, 0, 0x2000, 0 };
static const u16 ocsc_ycc601[12] = { 0xE04, 0xF444, 0xFDB9, 0x1004, 0x831, 0x1016, 0x320, 0x201, 0xFB45, 0xF6B7, 0xE04, 0x1004 };
static const u16 ocsc_ycc709[12] = { 0xE04, 0xF345, 0xFEB7, 0x1004, 0x5D3, 0x1399, 0x1FA, 0x201, 0xFCCA, 0xF533, 0xE04, 0x1004 };

/* dpp1_cm_program_color_matrix: escreve no banco que não está em uso (OCSC = modo 4, COMB = modo 5). O modo em uso
 * sai do índice 9 do CM_TEST_DEBUG_DATA, bits 16..18 (dcn10_dpp.h: CM_TEST_DEBUG_DATA_ID9_OCSC_MODE) */
#define CM_DBG_ID9_OCSC_MODE 16u, 0x70000u
static void dpp_output_csc(amdgpu_t *a, u32 p, const u16 *m) {
    WR(a, D_CM_CM_TEST_DEBUG_INDEX(p), FV(F_CM_CM_TEST_DEBUG_INDEX__CM_TEST_DEBUG_INDEX, 9));
    u32 cur = FG(RR(a, D_CM_CM_TEST_DEBUG_DATA(p)), CM_DBG_ID9_OCSC_MODE);
    u32 mode = cur != 4 ? 4 : 5;
    u32 r0 = mode == 4 ? D_CM_CM_OCSC_C11_C12(p) : D_CM_CM_COMB_C11_C12(p);
    for (u32 i = 0; i < 6; i++)
        WR(a, r0 + i * 4, FV(F_CM_CM_OCSC_C11_C12__CM_OCSC_C11, m[2 * i]) | FV(F_CM_CM_OCSC_C11_C12__CM_OCSC_C12, m[2 * i + 1]));
    WR(a, D_CM_CM_OCSC_CONTROL(p), FV(F_CM_CM_OCSC_CONTROL__CM_OCSC_MODE, mode));
}

static void dpp_color(amdgpu_t *a, u32 p, const amd_dcn_stream_t *s) {
    WR(a, D_CM_CM_GAMUT_REMAP_CONTROL(p), 0);                                         /* gamut em bypass */
    /* dcn10_program_output_csc -> dpp1_cm_set_output_csc_default(espaço de cor da saída) */
    dpp_output_csc(a, p, !s->ycbcr444 ? ocsc_rgb : s->t.pix_clk_khz > 27030 ? ocsc_ycc709 : ocsc_ycc601);
    WR(a, D_CM_CM_HDR_MULT_COEF(p), FV(F_CM_CM_HDR_MULT_COEF__CM_HDR_MULT_COEF, 0x1F000));   /* 1.0 */
    UPD(a, D_CM_CM_CMOUT_CONTROL(p), F_CM_CM_CMOUT_CONTROL__CM_CMOUT_ROUND_TRUNC_MODE, 8);  /* dpp1_enable_cm_block */
    UPD(a, D_CM_CM_CONTROL(p), F_CM_CM_CONTROL__CM_BYPASS_EN, 0);
    UPD(a, D_CM_CM_DGAM_CONTROL(p), F_CM_CM_DGAM_CONTROL__CM_DGAM_LUT_MODE, 0);
    UPD(a, D_CM_CM_RGAM_CONTROL(p), F_CM_CM_RGAM_CONTROL__CM_RGAM_LUT_MODE, 0);
}

/* ------------------------------------------------------------ OPP */
static void opp_fmt(amdgpu_t *a, int o, const amd_dcn_stream_t *s) {
    /* resource_build_bit_depth_reduction_params com DITHER_OPTION_DEFAULT: SPATIAL6/8/10 pela profundidade da saída */
    u32 depth = s->bpc == 6 ? 0 : s->bpc == 8 ? 1 : s->bpc == 10 ? 2 : 3;
    bool dither = depth < 3;
    upd(a, D_FMT_FMT_BIT_DEPTH_CONTROL(o), FV(F_FMT_FMT_BIT_DEPTH_CONTROL__FMT_TRUNCATE_EN, ~0u) |
        FV(F_FMT_FMT_BIT_DEPTH_CONTROL__FMT_TRUNCATE_DEPTH, ~0u) | FV(F_FMT_FMT_BIT_DEPTH_CONTROL__FMT_TRUNCATE_MODE, ~0u),
        s->bpc == 6 ? FV(F_FMT_FMT_BIT_DEPTH_CONTROL__FMT_TRUNCATE_EN, 1) | FV(F_FMT_FMT_BIT_DEPTH_CONTROL__FMT_TRUNCATE_DEPTH, 2) |
                      FV(F_FMT_FMT_BIT_DEPTH_CONTROL__FMT_TRUNCATE_MODE, 1) : 0);
    u32 dm = FV(F_FMT_FMT_BIT_DEPTH_CONTROL__FMT_SPATIAL_DITHER_EN, ~0u) | FV(F_FMT_FMT_BIT_DEPTH_CONTROL__FMT_SPATIAL_DITHER_MODE, ~0u) |
             FV(F_FMT_FMT_BIT_DEPTH_CONTROL__FMT_SPATIAL_DITHER_DEPTH, ~0u) | FV(F_FMT_FMT_BIT_DEPTH_CONTROL__FMT_TEMPORAL_DITHER_EN, ~0u) |
             FV(F_FMT_FMT_BIT_DEPTH_CONTROL__FMT_HIGHPASS_RANDOM_ENABLE, ~0u) | FV(F_FMT_FMT_BIT_DEPTH_CONTROL__FMT_FRAME_RANDOM_ENABLE, ~0u) |
             FV(F_FMT_FMT_BIT_DEPTH_CONTROL__FMT_RGB_RANDOM_ENABLE, ~0u);
    upd(a, D_FMT_FMT_BIT_DEPTH_CONTROL(o), dm, 0);
    if (dither) {
        upd(a, D_FMT_FMT_CONTROL(o), FV(F_FMT_FMT_CONTROL__FMT_SPATIAL_DITHER_FRAME_COUNTER_MAX, ~0u) |
            FV(F_FMT_FMT_CONTROL__FMT_SPATIAL_DITHER_FRAME_COUNTER_BIT_SWAP, ~0u), 0);
        WR(a, D_FMT_FMT_DITHER_RAND_R_SEED(o), 0);
        WR(a, D_FMT_FMT_DITHER_RAND_G_SEED(o), 0);
        WR(a, D_FMT_FMT_DITHER_RAND_B_SEED(o), 0);
        upd(a, D_FMT_FMT_BIT_DEPTH_CONTROL(o), dm,
            FV(F_FMT_FMT_BIT_DEPTH_CONTROL__FMT_SPATIAL_DITHER_EN, 1) | FV(F_FMT_FMT_BIT_DEPTH_CONTROL__FMT_SPATIAL_DITHER_DEPTH, depth) |
            FV(F_FMT_FMT_BIT_DEPTH_CONTROL__FMT_HIGHPASS_RANDOM_ENABLE, 1) | FV(F_FMT_FMT_BIT_DEPTH_CONTROL__FMT_RGB_RANDOM_ENABLE, !s->ycbcr444));
    }
    /* RGB de faixa cheia: CLAMPING_FULL_RANGE; codificação RGB */
    upd(a, D_FMT_FMT_CLAMP_CNTL(o), FV(F_FMT_FMT_CLAMP_CNTL__FMT_CLAMP_DATA_EN, ~0u) | FV(F_FMT_FMT_CLAMP_CNTL__FMT_CLAMP_COLOR_FORMAT, ~0u),
        FV(F_FMT_FMT_CLAMP_CNTL__FMT_CLAMP_DATA_EN, 1));
    upd(a, D_FMT_FMT_CONTROL(o), FV(F_FMT_FMT_CONTROL__FMT_PIXEL_ENCODING, ~0u) | FV(F_FMT_FMT_CONTROL__FMT_SUBSAMPLING_MODE, ~0u) |
        FV(F_FMT_FMT_CONTROL__FMT_CBCR_BIT_REDUCTION_BYPASS, ~0u), 0);
    /* opp1_set_dyn_expansion: HDMI/DP */
    u32 en = s->bpc == 8 || s->bpc == 10 || s->bpc == 12, mode = s->bpc == 8 ? 1 : 0;
    upd(a, D_FMT_FMT_DYNAMIC_EXP_CNTL(o), FV(F_FMT_FMT_DYNAMIC_EXP_CNTL__FMT_DYNAMIC_EXP_EN, ~0u) | FV(F_FMT_FMT_DYNAMIC_EXP_CNTL__FMT_DYNAMIC_EXP_MODE, ~0u),
        FV(F_FMT_FMT_DYNAMIC_EXP_CNTL__FMT_DYNAMIC_EXP_EN, en) | FV(F_FMT_FMT_DYNAMIC_EXP_CNTL__FMT_DYNAMIC_EXP_MODE, mode));
}

/* ------------------------------------------------------------ pipe */
void amdgpu_dcn_disable_pipe(amdgpu_t *a, u32 p) {
    if (!a || p >= AMD_DCN_PIPES) return;
    /* dcn10_plane_atomic_disconnect + plane_atomic_disable + plane_atomic_power_down */
    mpcc_remove(a, p);
    UPD(a, D_HUBP_DCHUBP_CNTL(p), F_HUBP_DCHUBP_CNTL__HUBP_TTU_DISABLE, 1);
    UPD(a, D_CURSOR_CURSOR_CONTROL(p), F_CURSOR_CURSOR_CONTROL__CURSOR_ENABLE, 0);
    WAIT(a, D_MPCC_MPCC_STATUS(p), F_MPCC_MPCC_STATUS__MPCC_IDLE, 1, 100000);
    hubp_set_blank(a, p, true);
    UPD(a, D_HUBP_HUBP_CLK_CNTL(p), F_HUBP_HUBP_CLK_CNTL__HUBP_CLOCK_ENABLE, 0);
    dpp_clock(a, p, false, false);
    pipe_power(a, p, false);
    a->dcn_pipe_owner[p] = -1;
}

int amdgpu_dcn_enable_stream(amdgpu_t *a, const amd_dcn_stream_t *s) {
    if (!a || !s || !s->bw || s->otg < 0 || s->otg >= AMD_DCN_OTGS || !s->npipes || s->npipes > 2) return -MOS_EINVAL;
    const amd_timing_t *t = &s->t;
    if (!t->h_total || !t->v_total || t->h_active + t->h_front_porch > t->h_total || t->v_active + t->v_front_porch > t->v_total ||
        !s->pitch_px || s->pitch_px < t->h_active || s->format > AMD_FMT_ABGR16161616F || s->bw->dpp != s->npipes)
        return -MOS_EINVAL;
    for (u32 i = 0; i < s->npipes; i++) if (s->pipe[i] >= AMD_DCN_PIPES) return -MOS_EINVAL;
    int o = s->otg;
    u32 bpp = amd_dcn_bytes_per_pixel(s->format);

    /* dcn10_enable_stream_timing (o relógio de pixel já foi programado por quem chamou) */
    otg_clock(a, o, true);
    otg_program_timing(a, o, s);
    /* color_space_to_black_color: RGB faixa cheia (0,0,0) ou YCbCr (Cb 0x1F4, Y 0x40, Cr 0x1F4) */
    WR(a, D_OTG_OTG_BLACK_COLOR(o), s->ycbcr444 ? FV(F_OTG_OTG_BLACK_COLOR__OTG_BLACK_COLOR_B_CB, 0x1F4) |
       FV(F_OTG_OTG_BLACK_COLOR__OTG_BLACK_COLOR_G_Y, 0x40) | FV(F_OTG_OTG_BLACK_COLOR__OTG_BLACK_COLOR_R_CR, 0x1F4) : 0);
    otg_blank(a, o, true);
    otg_enable(a, o);

    otg_lock(a, o, true);
    for (u32 i = 0; i < s->npipes; i++) {
        u32 p = s->pipe[i];
        const amd_dml1_dlg_in_t *in = &s->bw->dlg[i];
        u32 vx = s->bw->viewport_x[i], vw = s->bw->viewport_width[i];
        amd_dml1_surface_t surf = { bpp, tile_bytes(s->sw_mode), vw, s->pitch_px };
        amd_dml1_rq_t rq;
        if (amd_dml1_rq(&surf, &rq) < 0) { otg_lock(a, o, false); return -MOS_EINVAL; }
        amd_dml1_dlg_t d;
        amd_dml1_dlg(in, &rq, bpp, &d);

        /* dcn10_enable_plane */
        pipe_power(a, p, true);
        UPD(a, D_HUBP_HUBP_CLK_CNTL(p), F_HUBP_HUBP_CLK_CNTL__HUBP_CLOCK_ENABLE, 1);
        UPD(a, D_OPP_PIPE_OPP_PIPE_CONTROL(o), F_OPP_PIPE_OPP_PIPE_CONTROL__OPP_PIPE_CLOCK_EN, 1);
        hubp_vm_settings(a, p);
        /* dcn10_update_dchubp_dpp */
        dpp_clock(a, p, s->bw->dlg[i].dppclk_mhz * 2.0 <= s->bw->dlg[i].dispclk_mhz + 0.5, true);
        UPD(a, D_HUBP_DCHUBP_CNTL(p), F_HUBP_DCHUBP_CNTL__HUBP_VTG_SEL, (u32)o);
        hubp_requestor(a, p, &rq);
        hubp_deadline(a, p, &d);
        hubp_vready_wa(a, p, in, s->bw->vblank_end);
        hubp_interdependent(a, p, &d);
        dpp_input(a, p, s->format);
        mpcc_remove(a, p);
        mpcc_append(a, (u32)o, p, p);
        dpp_scaler(a, p, i ? s->bw->dlg[0].recout_width : 0, in->recout_width, vw, t->v_active, t->h_active);
        hubp_viewport(a, p, vx, vw, t->v_active);
        dpp_color(a, p, s);
        hubp_surface(a, p, s);
        hubp_address(a, p, s->addr, s->dcc ? s->meta_addr : 0);
        hubp_set_blank(a, p, false);
        a->dcn_pipe_owner[p] = o;
    }
    opp_fmt(a, o, s);
    otg_lock(a, o, false);
    otg_blank(a, o, false);
    return 0;
}

void amdgpu_dcn_flip(amdgpu_t *a, const amd_dcn_stream_t *s, u64 addr) {
    if (!a || !s) return;
    otg_lock(a, s->otg, true);                    /* os pipes de uma tela dividida trocam no mesmo vblank */
    for (u32 i = 0; i < s->npipes && i < 2; i++) hubp_address(a, s->pipe[i], addr, 0);
    otg_lock(a, s->otg, false);
}
