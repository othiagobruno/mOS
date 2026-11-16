/* mOS — validação de banda e relógios do DCN 1.0 (Raven/Picasso/Raven2): decide quantos pipes cada tela usa, o nível
 * de tensão, dispclk/dppclk/dcfclk, as marcas d'água do DCHUBBUB e os parâmetros vstartup/vupdate/vready do OTG.
 * dcnbw_auto.c é a cópia literal do modelo da AMD (MIT); dcnbw.c é o equivalente do dcn_validate_bandwidth para
 * as estruturas do mOS. Lógica pura, testada no host (tools/amdgpu_dcnbw_host.c). */
#pragma once
#include "amdgpu_base.h"
#include "dml1.h"

#define AMD_DCNBW_MAX_STREAMS 4

/* Limites do SoC. As tabelas de relógio vêm do SMU (TABLE_DPMCLOCKS, amdgpu_smu_dpm_clocks); com 0 níveis ficam os
 * padrões do dcn10_soc_defaults. */
typedef struct {
    u32  fclk_mhz[4], nfclk;            /* FClocks em ordem crescente */
    u32  dcfclk_mhz[4], ndcfclk;        /* DcefClocks em ordem crescente */
    u32  channels;                      /* largura da VRAM / 64; 0 = 2 (BIOS antiga) */
    bool dcn101;                        /* Raven2: 3 DPPs e troca de relógio da DRAM em 23 us */
    bool rv1_f0;                        /* Raven1 F0: latência urgente 3 us e 41,6 GB/s no topo */
    u32  num_pipes;                     /* pipes do DCN (4 no Raven/Picasso, 3 no Raven2) */
    u32  refclk_khz;                    /* relógio de referência do DCHUB (48000 no Raven) */
} amd_dcnbw_soc_t;

typedef struct {
    /* timing do stream (dc_crtc_timing): bordas em 0 */
    u32    h_total, h_active, h_front_porch, h_sync_width;
    u32    v_total, v_active, v_front_porch, v_sync_width;
    double pixel_clock_mhz;
    bool   hdmi;                        /* senão DP */
    bool   ycbcr420;
    u32    bpc;                         /* 6, 8, 10, 12 ou 16 */
    /* plano: um por stream, sem escala */
    bool   has_plane;
    u32    bytes_per_pixel;             /* 2, 4 ou 8 */
    u32    sw_mode;                     /* SW_MODE do GFX9 (0 linear, 9 64KB_S, 25 64KB_S_X ...) */
    u32    src_width, src_height;       /* tamanho do framebuffer lido */
    bool   dcc;
    bool   flip_immediate;
} amd_dcnbw_stream_t;

typedef struct {
    u32 urgent_ns, pte_meta_urgent_ns, sr_exit_ns, sr_enter_plus_exit_ns, pstate_change_ns;
} amd_dcnbw_wm_t;

typedef struct {
    u32  dpp;                           /* 1 ou 2 pipes para este stream */
    u32  vstartup_start, vupdate_offset, vupdate_width, vready_offset;
    u32  vblank_start, vblank_end;      /* pipe_dlg_param: 0 em modo progressivo */
    u32  viewport_width[2], viewport_x[2]; /* parte de cada pipe */
    amd_dml1_dlg_in_t dlg[2];           /* entrada do DLG/TTU de cada pipe (dml1.c) */
} amd_dcnbw_pipe_t;

typedef struct {
    bool   ok;                          /* cabe na banda e no nível de tensão permitido */
    int    voltage_level;               /* 0..3; 5 = não suportado */
    u32    dispclk_khz, dppclk_khz, dcfclk_khz, dcfclk_deep_sleep_khz, fclk_khz, phyclk_khz;
    u32    max_supported_dppclk_khz;
    amd_dcnbw_wm_t wm[4];               /* conjuntos A..D */
    amd_dcnbw_pipe_t s[AMD_DCNBW_MAX_STREAMS];
    /* valores internos do modelo, para o DLG e para conferir */
    double dispclk_mhz, dppclk_mhz, dcfclk_mhz, dcfclk_deep_sleep_mhz, urgent_extra_latency_us;
    double urgent_wm_us, sr_exit_wm_us, sr_enter_plus_exit_wm_us, dram_change_wm_us, pte_meta_urgent_wm_us;
    double total_read_bw_mbps;
} amd_dcnbw_out_t;

void amd_dcnbw_soc_defaults(amd_dcnbw_soc_t *soc);
/* 0 se a configuração cabe; -1 se não (out->ok = false, o resto descreve a tentativa) */
int  amd_dcnbw_validate(const amd_dcnbw_soc_t *soc, const amd_dcnbw_stream_t *st, u32 n, amd_dcnbw_out_t *out);
/* marca d'água em ns -> valor do registrador do DCHUBBUB (ciclos do refclk, limitado a 0x1FFFFF) */
u32  amd_dcnbw_wm_cycles(u32 ns, u32 refclk_khz);
