/* mOS — teste de host do caminho de pixels do DCN 1.0 (kernel/dev/amdgpu/dcn.c). Parte de um banco de registradores
 * zerado (como depois do reset, com a árvore MPC vazia), calcula o modo 5120x1440 do monitor do Thiago com o modelo
 * de banda e programa a tela do zero; os registradores do pipe 0 e do OTG 0 têm de dar os valores lidos no Picasso
 * com o Linux (tools/amdgpu_dcn_live.h, gerado de tools/testdata/amdgpu/dcn-live-5120x1440.json).
 * Rodar da raiz:
 *   gcc -O2 -Wall -Wextra -DMOS_HOST_TEST -I kernel/dev/amdgpu -I tools tools/amdgpu_dcn_host.c kernel/dev/amdgpu/dcn.c \
 *       kernel/dev/amdgpu/dcnbw.c kernel/dev/amdgpu/dcnbw_auto.c kernel/dev/amdgpu/dml1.c -o /tmp/dcn && /tmp/dcn */
#include "amdgpu.h"
#include "regs.h"
#include "dcn_regs.h"
#include "amdgpu_dcn_live.h"
#include "ucode.h"
#include <stdarg.h>
#include <stdlib.h>

static int pass, total;
#define CHECK(c, ...) do { total++; if (c) pass++; else { printf("FALHOU %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static u32 regs[AMD_MMIO_SIZE / 4];
static u8 written[AMD_MMIO_SIZE / 4];
#define FV_(s, m, v) ((((u32)(v)) << (s)) & (m))
#define FV(f, v) FV_(f, v)
#define MASK_(s, m) (m)
#define MASK(f) MASK_(f)

u32 amd_host_rreg(u32 off) { return off < AMD_MMIO_SIZE ? regs[off / 4] : 0; }
void amd_host_wreg(u32 off, u32 v) {
    if (off >= AMD_MMIO_SIZE) return;
    regs[off / 4] = v;
    written[off / 4] = 1;
    /* o pouco de hardware que as esperas leem */
    for (int o = 0; o < 4; o++) {
        if (off == D_ODM_OPTC_INPUT_CLOCK_CONTROL(o)) {
            u32 on = (v & MASK(F_ODM_OPTC_INPUT_CLOCK_CONTROL__OPTC_INPUT_CLK_EN)) ? 1 : 0;
            regs[off / 4] = (v & ~MASK(F_ODM_OPTC_INPUT_CLOCK_CONTROL__OPTC_INPUT_CLK_ON)) | FV(F_ODM_OPTC_INPUT_CLOCK_CONTROL__OPTC_INPUT_CLK_ON, on);
        }
        if (off == D_OTG_OTG_MASTER_UPDATE_LOCK(o))
            regs[off / 4] = (v & 1) ? (v | MASK(F_OTG_OTG_MASTER_UPDATE_LOCK__UPDATE_LOCK_STATUS)) : 0;
        if (off == D_OTG_OTG_CLOCK_CONTROL(o)) {
            u32 on = (v & MASK(F_OTG_OTG_CLOCK_CONTROL__OTG_CLOCK_EN)) ? 1 : 0;
            regs[off / 4] = (v & ~MASK(F_OTG_OTG_CLOCK_CONTROL__OTG_CLOCK_ON)) | FV(F_OTG_OTG_CLOCK_CONTROL__OTG_CLOCK_ON, on);
        }
    }
    static const u32 cfg[8] = { D_DOMAIN0_PG_CONFIG, D_DOMAIN1_PG_CONFIG, D_DOMAIN2_PG_CONFIG, D_DOMAIN3_PG_CONFIG,
                                D_DOMAIN4_PG_CONFIG, D_DOMAIN5_PG_CONFIG, D_DOMAIN6_PG_CONFIG, D_DOMAIN7_PG_CONFIG };
    static const u32 st[8] = { D_DOMAIN0_PG_STATUS, D_DOMAIN1_PG_STATUS, D_DOMAIN2_PG_STATUS, D_DOMAIN3_PG_STATUS,
                               D_DOMAIN4_PG_STATUS, D_DOMAIN5_PG_STATUS, D_DOMAIN6_PG_STATUS, D_DOMAIN7_PG_STATUS };
    for (int d = 0; d < 8; d++)
        if (off == cfg[d]) {
            u32 gate = (v & MASK(F_DOMAIN0_PG_CONFIG__DOMAIN0_POWER_GATE)) ? 2 : 0;
            regs[st[d] / 4] = FV(F_DOMAIN0_PG_STATUS__DOMAIN0_PGFSM_PWR_STATUS, gate);
        }
}
void kprintf(const char *fmt, ...) { va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap); }

/* registradores que o retrato vivo tem mas que não são do caminho de pixels do mOS, com o motivo */
static const char *const skip_prefix[] = {
    "DCCG_", "DISPCLK_", "DPPCLK_",                                   /* relógios: modeset.c */
    "OTG0_OTG_CRC", "OTG0_OTG_STATUS", "OTG0_OTG_NOM_VERT_POSITION", "OTG0_OTG_PIXEL_DATA_READBACK",
    "OTG0_OTG_COUNT", "OTG0_OTG_VERT_SYNC", "OTG0_OTG_TRIGA", "OTG0_OTG_TRIGB", "OTG0_OTG_FORCE_COUNT",
    "OTG0_OTG_SNAPSHOT", "OTG0_OTG_INTERRUPT", "OTG0_OTG_VERTICAL_INTERRUPT", "OTG0_OTG_AVSYNC",
    "OTG0_OTG_STATIC_SCREEN", "OTG0_OTG_UPDATE_LOCK", "OTG0_OTG_DOUBLE_BUFFER", "OTG0_OTG_MASTER_UPDATE",
    "OTG0_OTG_V_TOTAL_INT", "OTG0_OTG_VSYNC_NOM", "OTG0_OTG_GSL", "OTG0_OTG_GLOBAL", "OTG0_OTG_MANUAL",
    "OTG0_OTG_DRR", "OTG0_OTG_TEST", "OTG0_OTG_SPARE", "OTG0_OTG_3D", "OTG0_OTG_STEREO", "OTG0_OTG_V_UPDATE",
    "OTG0_OTG_GLOBAL_SYNC_STATUS", "OTG0_OTG_MASTER_EN", "OTG0_OTG_BLANK_DATA_COLOR", "OTG0_OTG_BLACK_COLOR_EXT",
    "OTG0_PHYPLL_PIXEL_RATE_CNTL", "OTG0_OTG_RANGE_TIMING",              /* fonte do relógio de pixel: modeset.c */
    "OTG0_OTG_V_TOTAL_MID", "OTG0_OTG_V_TOTAL_CONTROL", "OTG0_OTG_VSTARTUP_PARAM_DUMMY",
    "MPCC0_MPCC_STATUS", "MPCC0_MPCC_STALL", "MPC_CRC", "MPC_PERFMON", "MPC_SOFT_RESET", "MPC_STALL", "MPC_CLOCK",
    "MPC_BYPASS", "MPCC0_MPCC_SM", "MPCC0_MPCC_TOP_OFFSET", "MPCC0_MPCC_BOT_OFFSET", "MPCC0_MPCC_OFFSET",
    "HUBPREQ0_DCSURF_SURFACE_EARLIEST", "HUBPREQ0_DCSURF_SURFACE_INUSE", "HUBPREQ0_DCSURF_FRAME_PACING",
    "HUBPREQ0_DCSURF_SECONDARY", "HUBPREQ0_DCSURF_FLIP_CONTROL2",
    "HUBPREQ0_DCSURF_SURFACE_FLIP_INTERRUPT",                         /* interrupção do flip: irq.c */
    "HUBPREQ0_DCN_VM_", "HUBP0_DCHUBP_VMPG",                           /* VM: cópia da MMHUB, conferida à parte */
    "HUBPREQ0_DCN_CUR0", "HUBPREQ0_CURSOR", "HUBP0_HUBP_MEASURE", "HUBP0_HUBPREQ_DEBUG\"",
    "HUBPREQ0_VMID", "HUBPREQ0_DCN_PREFETCH", "HUBPREQ0_FLIP_PARAMETERS", "HUBPREQ0_DCSURF_PRIMARY_SURFACE_ADDRESS_C",
    "HUBPREQ0_DCSURF_PRIMARY_SURFACE_ADDRESS_HIGH_C", "HUBPREQ0_DCSURF_PRIMARY_META_SURFACE_ADDRESS_C", "HUBPREQ0_DCSURF_PRIMARY_META_SURFACE_ADDRESS_HIGH_C",
    "HUBPRET0_HUBPRET_READ", "HUBPRET0_HUBPRET_INTERRUPT", "HUBPRET0_HUBPRET_MEM",
    "DSCL0_DSCL_MEM", "DSCL0_LB_V_COUNTER", "DSCL0_OBUF", "DSCL0_SCL_COEF", "DSCL0_SCL_HORZ", "DSCL0_SCL_VERT",
    "DSCL0_SCL_TAP", "DSCL0_SCL_MANUAL", "DSCL0_SCL_BLACK", "DSCL0_DSCL_EXT", "DSCL0_OTG_", "DSCL0_DSCL_UPDATE",
    "DSCL0_DSCL_2TAP", "DSCL0_SCL_AUTOCAL",
    "DPP_TOP0_DPP_CRC", "DPP_TOP0_HOST", "DPP_TOP0_DPP_SOFT", "CNVC_CFG0_COLOR_KEYER", "CNVC_CFG0_DENORM",
    "CNVC_CFG0_FCNV", "FMT0_FMT_SIDE", "FMT0_FMT_MAP420", "FMT0_FMT_CLAMP_COMPONENT",
    "DCHUBBUB_", 0 };
static bool skipped(const char *n) {
    char q[96];
    snprintf(q, sizeof q, "%s\"", n);
    for (int i = 0; skip_prefix[i]; i++) {
        const char *p = skip_prefix[i];
        usize l = strlen(p);
        if (p[l - 1] == '"' ? !strcmp(q, p) : !strncmp(n, p, l)) return true;
    }
    return false;
}
static u32 live(const char *n) {
    for (usize i = 0; i < sizeof dcn_live / sizeof dcn_live[0]; i++) if (!strcmp(dcn_live[i].name, n)) return dcn_live[i].val;
    printf("sem %s no retrato\n", n);
    return 0xDEADBEEF;
}

static void reset_bank(u32 fill) {
    for (usize i = 0; i < AMD_MMIO_SIZE / 4; i++) regs[i] = fill;
    memset(written, 0, sizeof written);
    for (int i = 0; i < 4; i++) {
        regs[D_MPC_OUT_MUX(i) / 4] = 0xF;
        regs[D_MPCC_MPCC_TOP_SEL(i) / 4] = regs[D_MPCC_MPCC_BOT_SEL(i) / 4] = 0xF;
        regs[D_MPCC_MPCC_OPP_ID(i) / 4] = regs[D_MPCC_MPCC_UPDATE_LOCK_SEL(i) / 4] = 0xF;
        regs[D_MPCC_MPCC_STATUS(i) / 4] = 0x2;                    /* MPCC_IDLE */
        regs[D_CM_CM_TEST_DEBUG_DATA(i) / 4] = 0;                 /* banco da CSC de saída em uso: nenhum */
    }
    /* MMHUB: abertura do sistema e GART do contexto 0 como o engine.c deixa */
    regs[R_MC_VM_SYS_APERTURE_LOW / 4] = (u32)(0xF400000000ull >> 18);
    regs[R_MC_VM_SYS_APERTURE_HIGH / 4] = (u32)((0xF400000000ull + (256ull << 20) - 1) >> 18);
    regs[R_MC_VM_SYS_APERTURE_DEF_LSB / 4] = 0x3F0123; regs[R_MC_VM_SYS_APERTURE_DEF_MSB / 4] = 0;
    regs[R_VM_CTX0_PT_BASE_LO / 4] = 0x00800001; regs[R_VM_CTX0_PT_BASE_HI / 4] = 0xF4;
    regs[R_VM_CTX0_PT_START_LO / 4] = 0x100000; regs[R_VM_CTX0_PT_START_HI / 4] = 0;
    regs[R_VM_CTX0_PT_END_LO / 4] = 0x10FFFF; regs[R_VM_CTX0_PT_END_HI / 4] = 0;
    regs[R_VM_L2_FAULT_DEFAULT_LO / 4] = 0xDE0A1; regs[R_VM_L2_FAULT_DEFAULT_HI / 4] = 0;
    regs[D_DCHUBBUB_SDPIF_FB_BASE / 4] = 0xF4; regs[D_DCHUBBUB_SDPIF_FB_OFFSET / 4] = 0xF4;
}

/* campos que o hardware escreve sozinho (estado) e que o retrato traz ligados */
static u32 status_bits(const char *n) {
    if (!strcmp(n, "HUBP0_DCHUBP_CNTL")) return 0x2;                       /* HUBP_NO_OUTSTANDING_REQ */
    if (!strcmp(n, "HUBP0_HUBP_CLK_CNTL")) return 0xF00000;                /* *_CLOCK_ON */
    if (!strcmp(n, "OPP_PIPE0_OPP_PIPE_CONTROL")) return 0x2;              /* OPP_PIPE_CLOCK_ON */
    if (!strcmp(n, "OTG0_OTG_CLOCK_CONTROL")) return 0x10000;              /* OTG_BUSY */
    if (!strcmp(n, "OTG0_OTG_CONTROL")) return 0x80000000;                 /* OTG_CURRENT_MASTER_EN_STATE */
    if (!strcmp(n, "DSCL0_LB_MEMORY_CTRL")) return 0x7F7F0000;             /* LB_NUM_PARTITIONS(_C), calculado pelo LB */
    if (!strcmp(n, "OTG0_OTG_RANGE_TIMING_INT_STATUS")) return ~0u;
    return 0;
}

int main(void) {
    static amdgpu_t a;
    a.variant = AMD_PICASSO;
    amd_dcnbw_soc_defaults(&a.bw_soc);
    u32 f[4] = {400, 933, 1200, 1333};
    memcpy(a.bw_soc.fclk_mhz, f, sizeof f);
    a.bw_soc.nfclk = 4;
    for (int i = 0; i < 4; i++) a.dcn_pipe_owner[i] = -1;

    amd_dcnbw_stream_t bs = { .h_total = 5400, .h_active = 5120, .h_front_porch = 88, .h_sync_width = 44,
        .v_total = 1485, .v_active = 1440, .v_front_porch = 3, .v_sync_width = 5, .pixel_clock_mhz = 481.14,
        .hdmi = true, .bpc = 8, .has_plane = true, .bytes_per_pixel = 4, .sw_mode = 25, .src_width = 5120, .src_height = 1440 };
    amd_dcnbw_out_t bw;
    CHECK(amd_dcnbw_validate(&a.bw_soc, &bs, 1, &bw) == 0 && bw.s[0].dpp == 2, "modelo de banda");
    /* a tela do Linux: HDMI em YCbCr 4:4:4, sincronismos negativos, ARGB2101010 64KB_S_X com DCC (64B independentes) */
    amd_dcn_stream_t s = { .otg = 0, .t = { 5400, 5120, 88, 44, 1485, 1440, 3, 5, false, false, 481140 },
        .dp = false, .bpc = 8, .ycbcr444 = true, .addr = 0x05400000ull, .pitch_px = 5120, .format = AMD_FMT_ARGB2101010,
        .sw_mode = 25, .dcc = true, .dcc_ind_64b = true, .meta_addr = 0x03200000ull, .meta_pitch_px = 5120,
        .npipes = 2, .pipe = {0, 3}, .bw = &bw.s[0] };
    /* Duas passadas: banco todo em 0 e todo em 1. Bit igual nas duas = o mOS escreveu (é dele); bit diferente =
     * preservado do que o hardware tinha (padrão de reset, campo que o Linux também não toca). Os bits do mOS
     * têm de bater com o retrato vivo; os preservados só contam se o registrador inteiro for do mOS. */
    static u32 run0[AMD_MMIO_SIZE / 4];
    reset_bank(0);
    amdgpu_dcn_watermarks(&a, &bw);
    CHECK(amdgpu_dcn_enable_stream(&a, &s) == 0, "programa a tela (banco em 0)");
    memcpy(run0, regs, sizeof regs);
    reset_bank(~0u);
    amdgpu_dcn_watermarks(&a, &bw);
    CHECK(amdgpu_dcn_enable_stream(&a, &s) == 0, "programa a tela (banco em 1)");
    CHECK(a.dcn_timeouts == 0, "nenhuma espera estourou (%u)", a.dcn_timeouts);

    int cmp = 0, bad = 0, untouched = 0;
    for (usize i = 0; i < sizeof dcn_live / sizeof dcn_live[0]; i++) {
        const char *n = dcn_live[i].name;
        if (skipped(n)) continue;
        u32 r = dcn_live[i].off / 4;
        u32 owned = ~(run0[r] ^ regs[r]) & ~status_bits(n);
        if (!written[r]) { untouched++; if (getenv("DCN_VERBOSE")) printf("  não escrito: %s = 0x%08X\n", n, dcn_live[i].val); continue; }
        cmp++;
        if ((run0[r] & owned) != (dcn_live[i].val & owned)) {
            bad++;
            printf("  %-52s mOS 0x%08X  vivo 0x%08X  (bits do mOS 0x%08X)\n", n, run0[r] & owned, dcn_live[i].val & owned, owned);
        }
    }
    CHECK(bad == 0, "%d de %d registradores do pipe 0/OTG 0 diferentes do hardware vivo", bad, cmp);
    printf("comparados %d registradores vivos (%d do retrato o mOS não escreve)\n", cmp, untouched);
    /* marcas d'água do DCHUBBUB (os 4 conjuntos) */
    const char *wmn[] = { "DCHUBBUB_ARB_DATA_URGENCY_WATERMARK_A", "DCHUBBUB_ARB_PTE_META_URGENCY_WATERMARK_A",
        "DCHUBBUB_ARB_ALLOW_SR_ENTER_WATERMARK_A", "DCHUBBUB_ARB_ALLOW_SR_EXIT_WATERMARK_A", "DCHUBBUB_ARB_ALLOW_DRAM_CLK_CHANGE_WATERMARK_D", 0 };
    u32 wmo[] = { D_DCHUBBUB_ARB_DATA_URGENCY_WATERMARK_A, D_DCHUBBUB_ARB_PTE_META_URGENCY_WATERMARK_A,
        D_DCHUBBUB_ARB_ALLOW_SR_ENTER_WATERMARK_A, D_DCHUBBUB_ARB_ALLOW_SR_EXIT_WATERMARK_A, D_DCHUBBUB_ARB_ALLOW_DRAM_CLK_CHANGE_WATERMARK_D };
    for (int i = 0; wmn[i]; i++) CHECK(regs[wmo[i] / 4] == live(wmn[i]), "%s = %u (vivo %u)", wmn[i], regs[wmo[i] / 4], live(wmn[i]));
    CHECK(regs[D_HUBPREQ_DCSURF_PRIMARY_SURFACE_ADDRESS(0) / 4] == live("HUBPREQ0_DCSURF_PRIMARY_SURFACE_ADDRESS") &&
          regs[D_HUBPREQ_DCSURF_PRIMARY_META_SURFACE_ADDRESS(0) / 4] == live("HUBPREQ0_DCSURF_PRIMARY_META_SURFACE_ADDRESS") &&
          (regs[D_HUBPREQ_DCSURF_SURFACE_CONTROL(0) / 4] & 0x606) == (live("HUBPREQ0_DCSURF_SURFACE_CONTROL") & 0x606), "endereços da superfície e dos metadados, DCC");
    CHECK(regs[D_CM_CM_OCSC_CONTROL(0) / 4] == 4 && regs[D_CM_CM_OCSC_C11_C12(0) / 4] == (0xE04u | 0xF345u << 16) &&
          regs[D_CM_CM_OCSC_C33_C34(0) / 4] == (0xE04u | 0x1004u << 16), "CSC de saída BT.709 no banco OCSC");
    /* pipe 3: metade direita */
    CHECK(regs[D_HUBP_DCSURF_PRI_VIEWPORT_START(3) / 4] == 2560 && regs[D_DSCL_RECOUT_START(3) / 4] == 2560, "pipe 3 começa em x = 2560");
    CHECK(regs[D_MPC_OUT_MUX(0) / 4] == 0 && regs[D_MPCC_MPCC_BOT_SEL(0) / 4] == 3 && regs[D_MPCC_MPCC_BOT_SEL(3) / 4] == 0xF &&
          regs[D_MPCC_MPCC_OPP_ID(3) / 4] == 0, "árvore MPC: OPP0 -> MPCC0 -> MPCC3");
    CHECK(a.dcn_pipe_owner[0] == 0 && a.dcn_pipe_owner[3] == 0 && a.dcn_pipe_owner[1] == -1, "donos dos pipes");
    /* VM: cópia da MMHUB */
    CHECK(regs[D_HUBPREQ_DCN_VM_SYSTEM_APERTURE_LOW_ADDR_LSB(0) / 4] == (u32)(0xF400000000ull >> 12) &&
          regs[D_HUBPREQ_DCN_VM_CONTEXT0_PAGE_TABLE_BASE_ADDR_MSB(0) / 4] == 0xF4 &&
          regs[D_HUBPREQ_DCN_VM_SYSTEM_APERTURE_DEFAULT_ADDR_LSB(0) / 4] == 0x3F0123, "VM do HUBP copiada da MMHUB");
    /* desliga tudo */
    amdgpu_dcn_disable_pipe(&a, 3);
    CHECK(regs[D_MPCC_MPCC_BOT_SEL(0) / 4] == 0xF && (regs[D_MPCC_MPCC_CONTROL(0) / 4] & 3) == 2 && a.dcn_pipe_owner[3] == -1, "pipe 3 sai da árvore");
    amdgpu_dcn_disable_pipe(&a, 0);
    CHECK(regs[D_MPC_OUT_MUX(0) / 4] == 0xF, "árvore vazia");
    amdgpu_dcn_disable_otg(&a, 0);
    CHECK(!(regs[D_OTG_OTG_CONTROL(0) / 4] & 1) && !(regs[D_OTG_OTG_CLOCK_CONTROL(0) / 4] & 1), "OTG parado e sem relógio");
    /* entradas ruins */
    amd_dcn_stream_t bad1 = s; bad1.npipes = 1;
    CHECK(amdgpu_dcn_enable_stream(&a, &bad1) == -MOS_EINVAL, "pipes diferentes do modelo recusados");
    amd_dcn_stream_t bad2 = s; bad2.pitch_px = 100;
    CHECK(amdgpu_dcn_enable_stream(&a, &bad2) == -MOS_EINVAL, "pitch menor que a tela recusado");
    printf("amdgpu-dcn-host: %d/%d ok\n", pass, total);
    return pass == total ? 0 : 1;
}
