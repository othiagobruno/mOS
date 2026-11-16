/* mOS — programação do caminho de pixels do DCN 1.0 (dcn.c): HUBBUB (marcas d'água), HUBP (pedidos, DLG/TTU,
 * superfície, viewport), DPP (formato, LB, recout, cor em passagem direta), MPC (árvore de mistura), OPP (FMT) e
 * OTG (timing, sincronismo global, blank). O lado do encoder (DIG/PHY/relógio de pixel) fica em modeset.c. */
#pragma once
#include "amdgpu_base.h"
#include "dcnbw.h"

#define AMD_DCN_PIPES 4
#define AMD_DCN_OTGS  4

enum { AMD_FMT_ARGB8888, AMD_FMT_ABGR8888, AMD_FMT_ARGB2101010, AMD_FMT_ABGR2101010, AMD_FMT_RGB565,
       AMD_FMT_ARGB16161616F, AMD_FMT_ABGR16161616F };

typedef struct {
    u32  h_total, h_active, h_front_porch, h_sync_width;
    u32  v_total, v_active, v_front_porch, v_sync_width;
    bool hsync_pos, vsync_pos;
    u32  pix_clk_khz;
} amd_timing_t;

typedef struct {
    int          otg;                 /* OTG = OPP desta tela */
    amd_timing_t t;
    bool         dp;                  /* DP/eDP: OTG começa no ponto 1 */
    u32          bpc;                 /* 6, 8, 10 ou 12 na saída */
    bool         ycbcr444;            /* saída YCbCr 4:4:4 (HDMI com sink que aceita, como o amdgpu): CSC BT.709/601 */
    /* plano: ocupa a tela inteira, sem escala */
    u64          addr;                /* endereço MC da superfície */
    u32          pitch_px, format, sw_mode;
    bool         pipe_aligned, rb_aligned;  /* alinhamento dos tiles com que a superfície foi desenhada */
    bool         dcc;                 /* superfície comprimida: metadados em meta_addr */
    bool         dcc_ind_64b;
    u64          meta_addr;
    u32          meta_pitch_px;
    u32          npipes, pipe[2];     /* HUBP/DPP usados (2 = tela dividida ao meio) */
    const amd_dcnbw_pipe_t *bw;       /* parâmetros do modelo de banda para esta tela */
} amd_dcn_stream_t;

struct amdgpu;
u32  amd_dcn_bytes_per_pixel(u32 format);
void amdgpu_dcn_watermarks(struct amdgpu *a, const amd_dcnbw_out_t *bw);
void amdgpu_dcn_disable_pipe(struct amdgpu *a, u32 pipe);
void amdgpu_dcn_disable_otg(struct amdgpu *a, int otg);
/* liga o OTG com o timing e os pipes da tela; o relógio de pixel e o encoder já devem estar prontos. 0 ou -MOS_* */
int  amdgpu_dcn_enable_stream(struct amdgpu *a, const amd_dcn_stream_t *s);
/* troca o endereço da superfície de todos os pipes da tela no próximo vblank */
void amdgpu_dcn_flip(struct amdgpu *a, const amd_dcn_stream_t *s, u64 addr);
