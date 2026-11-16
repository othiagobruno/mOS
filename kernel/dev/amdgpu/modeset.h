/* mOS — troca de modo do DCN 1.0 (modeset.c): relógios do display pelo SMU e pelo VBIOS, PLL de pixel, DIG e PHY
 * pelas tabelas ATOM, HDMI (infoframe AVI, SCDC do HDMI 2.0) e DisplayPort (treino de link pelo AUX), e a sequência
 * inteira de ligar/desligar telas com o modelo de banda (dcnbw.c) e o caminho de pixels (dcn.c). */
#pragma once
#include "amdgpu_base.h"
#include "dcn.h"

#define AMD_MAX_STREAMS 4

/* o que quem chama pede para uma saída */
typedef struct {
    u32          conn;              /* índice em a->vbios.conn */
    amd_timing_t t;
    u32          bpc;               /* 8, 10 ou 12 na saída */
    bool         ycbcr444;          /* só HDMI: o amdgpu usa quando o monitor aceita */
    u8           vic, aspect;       /* do EDID (CTA-861): VIC e 1 = 4:3, 2 = 16:9, 0 = sem */
    bool         rgb_quant_selectable;
    bool         lte340_scramble;   /* HF-VSDB: o sink quer embaralhamento também abaixo de 340 MHz */
    /* superfície que a tela mostra */
    u64          addr;              /* MC */
    u32          pitch_px, format, sw_mode;
    bool         dcc, dcc_ind_64b, pipe_aligned, rb_aligned;
    u64          meta_addr;
    u32          meta_pitch_px;
} amd_mode_req_t;

/* DisplayPort: o enlace escolhido e treinado */
typedef struct {
    u8  rate;                       /* 0x06 RBR, 0x0A HBR, 0x14 HBR2 (unidades de 0,27 Gbps) */
    u8  lanes;                      /* 1, 2 ou 4 */
    u8  vs[4], pe[4];               /* nível de tensão e pré-ênfase de cada via */
    bool enhanced_framing, tps3;
} amd_dp_link_t;

/* estado de uma tela ligada (por OTG) */
typedef struct {
    bool            on;
    amd_mode_req_t  req;
    amd_dcn_stream_t dcn;
    amd_dcnbw_pipe_t bw;
    u32             phy;            /* transmissor 0..5 (UNIPHY A..F) = DIG = PLL */
    bool            dp, hdmi, scrambled;
    amd_dp_link_t   link;
    u32             phy_clk_khz;    /* relógio de símbolo do PHY (TMDS com cor profunda, ou o do enlace DP) */
    int             audio_ep;       /* endpoint AZ do áudio desta tela; -1 = sem áudio */
} amd_disp_t;

struct amdgpu;
/* Liga as n telas pedidas (desliga o resto). 0 ou -MOS_*: -MOS_EINVAL pedido inválido, -MOS_ERANGE não cabe na
 * banda do DCN, -MOS_EIO falha do VBIOS/SMU ou treino de DP que não fechou. */
int  amdgpu_modeset(struct amdgpu *a, const amd_mode_req_t *req, u32 n);
void amdgpu_modeset_off(struct amdgpu *a);                       /* desliga todas as telas */
/* troca a superfície mostrada pela tela do OTG no próximo vblank */
int  amdgpu_modeset_flip(struct amdgpu *a, int otg, u64 addr);
/* lógica pura (testada no host) */
int  amd_hdmi_avi(const amd_mode_req_t *r, u8 hb[3], u8 sb[28]);
u32  amd_dp_mvid(u32 pix_khz, u8 rate);                          /* M do vídeo com N = 0x8000 */
bool amd_dp_pick_link(u32 pix_khz, u32 bpc, u8 max_rate, u8 max_lanes, u8 *rate, u8 *lanes);
