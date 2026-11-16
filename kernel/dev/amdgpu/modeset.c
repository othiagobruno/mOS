/* mOS — troca de modo do DCN 1.0. Porte das sequências do display/dc da AMD (MIT) para o Raven/Picasso/Raven2:
 *   relógios: rv1_clk_mgr.c (dispclk pela caixa de mensagens do VBIOS no MP1, DPREFCLK pelo SetDCEClock, pisos de
 *   fclk/dcfclk/deep sleep e número de telas pelo SMU, faixas das marcas d'água em TABLE_WATERMARKS)
 *   pixel: dce_clock_source.c (SetPixelClock v1.7 no PLL do PHY, deep color no PIXCLK_RESYNC_CNTL)
 *   encoder: dcn10_link_encoder.c / dcn10_stream_encoder.c (DIGxEncoderControl v1.5, DIG1TransmitterControl v1.6,
 *   HDMI_CONTROL, pacotes genéricos do AFMT, DP: MSA, M/N do vídeo, padrões de treino)
 *   link: SCDC do HDMI 2.0 (write_scdc_data) e treino 8b/10b do DisplayPort (clock recovery + channel equalization,
 *   como na especificação DP 1.2) pelo AUX de aux.c.
 * A sequência é a do dc_commit_state para um modo completo: desliga as telas antigas, valida a banda, sobe os
 * relógios, marcas d'água, e para cada tela: relógio de pixel, OTG e pipes (dcn.c), encoder, enlace, vídeo. */
#include "amdgpu.h"
#include "regs.h"
#include "dcn.h"
#include "dcn_regs.h"
#include "modeset.h"
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

#define FV_(s, m, v)     ((((u32)(v)) << (s)) & (m))
#define FV(f, v)         FV_(f, v)
#define FG_(x, s, m)     (((x) & (m)) >> (s))
#define FG(x, f)         FG_(x, f)
#define UPD_(a, r, s, m, v) upd(a, r, m, ((u32)(v) << (s)) & (m))
#define UPD(a, r, f, v)  UPD_(a, r, f, v)
#define MASK_(s, m)      (m)
#define MASK(f)          MASK_(f)

static void upd(amdgpu_t *a, u32 r, u32 mask, u32 bits) { WR(a, r, (RR(a, r) & ~mask) | bits); }

/* ------------------------------------------------------------ relógios */
/* caixa de mensagens do VBIOS no MP1 (rv1_clk_mgr_vbios_smu.c): 91 = estado, 83 = argumento, 67 = mensagem */
#define R_VBSMC_RESP  AMD_SEG(AMD_MP1_BASE0, 0x029B)
#define R_VBSMC_ARG   AMD_SEG(AMD_MP1_BASE0, 0x0293)
#define R_VBSMC_MSG   AMD_SEG(AMD_MP1_BASE0, 0x0283)
#define VBSMC_SET_DISPCLK 0x4
#define VBSMC_OK      0x1

static int vbios_smu(amdgpu_t *a, u32 msg, u32 arg, u32 *out) {
    WR(a, R_VBSMC_RESP, 0);
    WR(a, R_VBSMC_ARG, arg);
    WR(a, R_VBSMC_MSG, msg);
    u32 r = 0;
    for (u32 i = 0; i <= 1000; i++) {                       /* 10 µs x 1000, como o Linux */
        r = RR(a, R_VBSMC_RESP);
        if (r) break;
        DELAY_US(10);
    }
    if (r != VBSMC_OK) { kprintf("amdgpu: SMU do VBIOS recusou a mensagem %u (0x%x)\n", msg, r); return -MOS_EIO; }
    if (out) *out = RR(a, R_VBSMC_ARG);
    return 0;
}

/* tabelas ATOM: parâmetros montados em bytes, passados como dwords (little endian) */
typedef struct { u32 dw[8]; } atom_ps_t;
static void ps_u8(atom_ps_t *p, u32 off, u8 v) { p->dw[off / 4] = (p->dw[off / 4] & ~(0xFFu << (off % 4 * 8))) | (u32)v << (off % 4 * 8); }
static void ps_u32(atom_ps_t *p, u32 off, u32 v) { p->dw[off / 4] = v; }

static int atom_run(amdgpu_t *a, int cmd, atom_ps_t *p) {
    if (!a->vbios_ok) return -MOS_ENXIO;
    if (amd_atom_execute(&a->atom, cmd, p->dw, 8)) {
        kprintf("amdgpu: tabela ATOM %d falhou (erro %u)\n", cmd, a->atom.last_error);
        return -MOS_EIO;
    }
    return 0;
}

#define SMU_MSG_SET_HARD_MIN_FCLK      0x12u
#define SMU_MSG_SET_DISPLAY_COUNT      0x18u
#define SMU_MSG_TRANSFER_DRAM2SMU      0x1Du
#define SMU_MSG_SET_HARD_MIN_DCEFCLK   0x20u
#define SMU_MSG_SET_MIN_DEEP_SLEEP_DCF 0x24u
#define SMU10_TABLE_WATERMARKS         1u

static u32 khz_to_mhz_ceil(u32 khz) { return (khz + 999) / 1000; }

/* dcn_bw_notify_pplib_of_wm_ranges + smu_set_watermarks_for_clocks_ranges: Watermarks_t para o SMU escolher o
 * conjunto de marcas d'água pelos relógios (o DCN 1.0 só usa o conjunto A, DEGVIDCN10-254) */
static int smu_wm_ranges(amdgpu_t *a) {
    if (!a->smu_ok || !a->gmc_ok || !a->smu_table) return -MOS_ENXIO;
    u8 *t = (u8 *)amdgpu_vram_cpu(a, a->smu_table);
    memset(t, 0, 4096);
    u16 min_dcf = (u16)(a->bw_soc.ndcfclk ? a->bw_soc.dcfclk_mhz[0] : 300);
    u16 min_f = (u16)(a->bw_soc.nfclk ? a->bw_soc.fclk_mhz[0] : 400);
    u16 socclk = 208, od = 5000;
    /* WatermarkRowGeneric_t: MinClock, MaxClock, MinMclk, MaxMclk (u16), WmSetting, WmType, 2 de enchimento */
    #define ROW(clk, i, mn, mx, mmn, mmx, set) do { u8 *r = t + ((clk) * 4 + (i)) * 12; \
        r[0] = (u8)(mn); r[1] = (u8)((mn) >> 8); r[2] = (u8)(mx); r[3] = (u8)((mx) >> 8); \
        r[4] = (u8)(mmn); r[5] = (u8)((mmn) >> 8); r[6] = (u8)(mmx); r[7] = (u8)((mmx) >> 8); r[8] = (u8)(set); } while (0)
    ROW(1, 0, min_dcf, od, min_f, od, 0);                   /* leitor A: dcfclk drena, memória enche */
    for (u32 i = 1; i < 4; i++) ROW(1, i, min_f, od, socclk, od, i);   /* leitores B..D = o escritor A (como no Linux) */
    ROW(0, 0, socclk, od, min_f, od, 0);                    /* escritor A: socclk enche, memória drena */
    #undef ROW
    u64 mc = amdgpu_vram_to_mc(a, a->smu_table);
    int r = amdgpu_smu_send(a, SMU_MSG_SET_DRIVER_DRAM_HI, (u32)(mc >> 32), NULL);
    if (!r) r = amdgpu_smu_send(a, SMU_MSG_SET_DRIVER_DRAM_LO, (u32)mc, NULL);
    if (!r) r = amdgpu_smu_send(a, SMU_MSG_TRANSFER_DRAM2SMU, SMU10_TABLE_WATERMARKS, NULL);
    return r;
}

/* rv1_update_clocks para um modo completo (sobe antes de programar, baixa depois): dispclk pela caixa do VBIOS,
 * DPREFCLK pelo SetDCEClock, pisos no SMU */
static int clocks_set(amdgpu_t *a, const amd_dcnbw_out_t *bw, u32 ndisp) {
    int r = 0;
    if (a->smu_ok) {
        amdgpu_smu_send(a, SMU_MSG_SET_DISPLAY_COUNT, ndisp, NULL);
        if (ndisp) {
            amdgpu_smu_send(a, SMU_MSG_SET_HARD_MIN_FCLK, khz_to_mhz_ceil(bw->fclk_khz), NULL);
            amdgpu_smu_send(a, SMU_MSG_SET_HARD_MIN_DCEFCLK, khz_to_mhz_ceil(bw->dcfclk_khz), NULL);
            amdgpu_smu_send(a, SMU_MSG_SET_MIN_DEEP_SLEEP_DCF, khz_to_mhz_ceil(bw->dcfclk_deep_sleep_khz), NULL);
        }
    }
    if (!ndisp) return 0;
    u32 got = 0;
    r = vbios_smu(a, VBSMC_SET_DISPCLK, khz_to_mhz_ceil(bw->dispclk_khz), &got);
    if (r) return r;
    a->disp_dispclk_khz = got * 1000;
    a->disp_dppclk_khz = bw->dppclk_khz;
    /* dcn10_set_dprefclk: SetDCEClock v2.1, DPREFCLK pelo DFS, o VBIOS escolhe a frequência */
    atom_ps_t p = {0};
    ps_u32(&p, 0, 0);
    ps_u8(&p, 4, 1);                                         /* DCE_CLOCK_TYPE_DPREFCLK */
    ps_u8(&p, 5, 8);                                         /* ATOM_GCK_DFS */
    r = atom_run(a, ATOM_CMD_SET_DCE_CLOCK, &p);
    if (!r) a->disp_dprefclk_khz = p.dw[0] * 10;
    return r;
}

/* ------------------------------------------------------------ relógio de pixel */
static u32 deep_color(u32 bpc) { return bpc == 10 ? 1 : bpc == 12 ? 2 : bpc == 16 ? 3 : 0; }

static u32 phy_index(const amd_conn_t *c) { return (u32)c->transmitter * 2 + (u32)c->link; }

static const u32 resync[6] = { D_PHYPLLA_PIXCLK_RESYNC_CNTL, D_PHYPLLB_PIXCLK_RESYNC_CNTL, D_PHYPLLC_PIXCLK_RESYNC_CNTL,
                               D_PHYPLLD_PIXCLK_RESYNC_CNTL, D_PHYPLLE_PIXCLK_RESYNC_CNTL, D_PHYPLLF_PIXCLK_RESYNC_CNTL };

static int pixel_clock(amdgpu_t *a, amd_disp_t *d, int otg) {
    const amd_conn_t *c = &a->vbios.conn[d->req.conn];
    atom_ps_t p = {0};
    /* set_pixel_clock_parameter_v1_7 */
    ps_u32(&p, 0, d->req.t.pix_clk_khz * 10);                /* pixclk_100hz */
    ps_u8(&p, 4, (u8)(20 + d->phy));                         /* ATOM_COMBOPHY_PLL0 + PHY (find_matching_pll) */
    ps_u8(&p, 5, (u8)c->encoder);                            /* objeto do encoder (UNIPHY) */
    ps_u8(&p, 6, d->dp ? 0 : 3);                             /* ATOM_ENCODER_MODE_DP / _HDMI */
    ps_u8(&p, 7, 0);                                         /* referência pelo XTALIN */
    ps_u8(&p, 8, (u8)otg);                                   /* ATOM_CRTC1 + OTG */
    ps_u8(&p, 9, (u8)(d->hdmi ? deep_color(d->req.bpc) : 0));
    int r = atom_run(a, ATOM_CMD_SET_PIXEL_CLOCK, &p);
    if (r) return r;
    /* dce112_program_pixel_clk_resync: razão do deep color no PHYPLL */
    UPD(a, resync[d->phy], F_PHYPLLA_PIXCLK_RESYNC_CNTL__PHYPLLA_DCCG_DEEP_COLOR_CNTL, d->hdmi ? deep_color(d->req.bpc) : 0);
    return 0;
}

/* ------------------------------------------------------------ DIG e PHY */
static int dig_encoder(amdgpu_t *a, amd_disp_t *d, u8 action) {
    atom_ps_t p = {0};
    /* dig_encoder_stream_setup_parameters_v1_5 */
    ps_u8(&p, 0, (u8)d->phy);                                /* DIG preferido do transmissor */
    ps_u8(&p, 1, action);
    ps_u8(&p, 2, d->dp ? 0 : 3);
    ps_u8(&p, 3, d->dp ? d->link.lanes : 4);
    u32 pclk10 = d->req.t.pix_clk_khz / 10;
    if (d->hdmi) pclk10 = pclk10 * (d->req.bpc == 10 ? 30 : d->req.bpc == 12 ? 36 : 24) / 24;
    ps_u32(&p, 4, pclk10);
    ps_u8(&p, 8, (u8)(d->req.bpc == 10 ? 3 : d->req.bpc == 12 ? 4 : d->req.bpc == 16 ? 5 : 2));   /* PANEL_xBIT_PER_COLOR */
    ps_u8(&p, 9, d->dp ? d->link.rate : 0);
    return atom_run(a, ATOM_CMD_DIGX_ENCODER_CONTROL, &p);
}

static int transmitter(amdgpu_t *a, amd_disp_t *d, u8 action, u8 laneset) {
    const amd_conn_t *c = &a->vbios.conn[d->req.conn];
    atom_ps_t p = {0};
    /* dig_transmitter_control_parameters_v1_6 */
    ps_u8(&p, 0, (u8)d->phy);
    ps_u8(&p, 1, action);
    ps_u8(&p, 2, action == 11 ? laneset : (u8)(d->dp ? 0 : 3));   /* SETUP_VSEMPH leva o DP_LANE_SET */
    ps_u8(&p, 3, d->dp ? d->link.lanes : 4);
    ps_u32(&p, 4, d->phy_clk_khz / 10);
    ps_u8(&p, 8, (u8)(c->hpd >= 0 ? c->hpd + 1 : 0));        /* ATOM_TRANSMITTER_V6_HPDn_SEL */
    ps_u8(&p, 9, 0);                                         /* digfe_sel: 0 no DCN (dce112 helper) */
    ps_u8(&p, 10, (u8)c->object);                            /* id do conector */
    return atom_run(a, ATOM_CMD_DIG1_TRANSMITTER, &p);
}

/* dcn10_link_encoder_setup + connect_dig_be_to_fe */
static void dig_be_setup(amdgpu_t *a, amd_disp_t *d) {
    u32 i = d->phy;
    UPD(a, D_DIG_DIG_BE_CNTL(i), F_DIG_DIG_BE_CNTL__DIG_MODE, d->dp ? 0 : 3);
    u32 fe = FG(RR(a, D_DIG_DIG_BE_CNTL(i)), F_DIG_DIG_BE_CNTL__DIG_FE_SOURCE_SELECT) | (1u << i);
    UPD(a, D_DIG_DIG_BE_CNTL(i), F_DIG_DIG_BE_CNTL__DIG_FE_SOURCE_SELECT, fe);
}

/* ------------------------------------------------------------ HDMI */
/* set_avi_info_frame do dc_resource.c: versão 2, 13 bytes; o byte 0 de sb é a soma de verificação */
int amd_hdmi_avi(const amd_mode_req_t *r, u8 hb[3], u8 sb[28]) {
    memset(sb, 0, 28);
    hb[0] = 0x82; hb[1] = 2; hb[2] = 13;
    u8 y = r->ycbcr444 ? 2 : 0;
    u8 cmet = !r->ycbcr444 ? 0 : r->t.pix_clk_khz > 27030 ? 2 : 1;       /* sRGB: sem dado; YCbCr: 709/601 */
    u8 m = (r->aspect == 1 || r->aspect == 2) ? r->aspect : 0;
    u8 q = r->rgb_quant_selectable && !r->ycbcr444 ? 2 : 0;              /* faixa cheia declarada só em RGB */
    sb[1] = (u8)(y << 5 | 1 << 4 | 3 << 2 | 2);              /* Y, A0 = 1, barras válidas, underscan */
    sb[2] = (u8)(cmet << 6 | m << 4 | 8);                    /* C, M, R = igual à imagem */
    sb[3] = (u8)(1 << 7 | q << 2);                           /* ITC = 1 (sem tipo de conteúdo), Q, SC = uniforme */
    sb[4] = r->vic;
    sb[5] = 0;                                               /* YQ = limitada, CN = 0, PR = 0 */
    u16 bb = (u16)(r->t.v_total + 1), br = (u16)(r->t.h_total + 1);
    sb[6] = 0; sb[7] = 0;                                    /* barra de cima */
    sb[8] = (u8)bb; sb[9] = (u8)(bb >> 8);
    sb[10] = 0; sb[11] = 0;
    sb[12] = (u8)br; sb[13] = (u8)(br >> 8);
    if (r->vic >= 128) hb[1] = 3;
    u8 sum = (u8)(hb[0] + hb[1] + hb[2]);
    for (u32 i = 1; i <= hb[2]; i++) sum = (u8)(sum + sb[i]);
    sb[0] = (u8)(0x100 - sum);
    return 0;
}

/* enc1_update_generic_info_packet + enc1_update_hdmi_info_packet (pacote 0 = AVI, linha 2) */
static void hdmi_packet(amdgpu_t *a, u32 i, u32 idx, const u8 hb[3], const u8 sb[28], bool valid) {
    if (valid) {
        UPD(a, D_DIG_AFMT_CNTL(i), F_DIG_AFMT_CNTL__AFMT_AUDIO_CLOCK_EN, 1);
        for (u32 k = 0; k < 50 && FG(RR(a, D_DIG_AFMT_VBI_PACKET_CONTROL(i)), F_DIG_AFMT_VBI_PACKET_CONTROL__AFMT_GENERIC_CONFLICT); k++) DELAY_US(10);
        UPD(a, D_DIG_AFMT_VBI_PACKET_CONTROL(i), F_DIG_AFMT_VBI_PACKET_CONTROL__AFMT_GENERIC_CONFLICT_CLR, 1);
        UPD(a, D_DIG_AFMT_VBI_PACKET_CONTROL(i), F_DIG_AFMT_VBI_PACKET_CONTROL__AFMT_GENERIC_INDEX, idx);
        WR(a, D_DIG_AFMT_GENERIC_HDR(i), (u32)hb[0] | (u32)hb[1] << 8 | (u32)hb[2] << 16);
        const u32 g[8] = { D_DIG_AFMT_GENERIC_0(i), D_DIG_AFMT_GENERIC_1(i), D_DIG_AFMT_GENERIC_2(i), D_DIG_AFMT_GENERIC_3(i),
                           D_DIG_AFMT_GENERIC_4(i), D_DIG_AFMT_GENERIC_5(i), D_DIG_AFMT_GENERIC_6(i), D_DIG_AFMT_GENERIC_7(i) };
        for (u32 k = 0; k < 7; k++) WR(a, g[k], (u32)sb[4 * k] | (u32)sb[4 * k + 1] << 8 | (u32)sb[4 * k + 2] << 16 | (u32)sb[4 * k + 3] << 24);
        WR(a, g[7], 0);                                      /* sb tem 28 bytes; o oitavo dword fica 0 */
        UPD(a, D_DIG_AFMT_VBI_PACKET_CONTROL1(i), F_DIG_AFMT_VBI_PACKET_CONTROL1__AFMT_GENERIC0_IMMEDIATE_UPDATE, idx == 0);
    }
    u32 cont = valid, send = valid, line = valid ? 2 : 0;
    u32 reg = idx / 2 == 0 ? D_DIG_HDMI_GENERIC_PACKET_CONTROL0(i) : idx / 2 == 1 ? D_DIG_HDMI_GENERIC_PACKET_CONTROL1(i) :
              idx / 2 == 2 ? D_DIG_HDMI_GENERIC_PACKET_CONTROL2(i) : D_DIG_HDMI_GENERIC_PACKET_CONTROL3(i);
    if (idx % 2 == 0)
        upd(a, reg, MASK(F_DIG_HDMI_GENERIC_PACKET_CONTROL0__HDMI_GENERIC0_CONT) | MASK(F_DIG_HDMI_GENERIC_PACKET_CONTROL0__HDMI_GENERIC0_SEND) |
            MASK(F_DIG_HDMI_GENERIC_PACKET_CONTROL0__HDMI_GENERIC0_LINE),
            FV(F_DIG_HDMI_GENERIC_PACKET_CONTROL0__HDMI_GENERIC0_CONT, cont) | FV(F_DIG_HDMI_GENERIC_PACKET_CONTROL0__HDMI_GENERIC0_SEND, send) |
            FV(F_DIG_HDMI_GENERIC_PACKET_CONTROL0__HDMI_GENERIC0_LINE, line));
    else
        upd(a, reg, MASK(F_DIG_HDMI_GENERIC_PACKET_CONTROL0__HDMI_GENERIC1_CONT) | MASK(F_DIG_HDMI_GENERIC_PACKET_CONTROL0__HDMI_GENERIC1_SEND) |
            MASK(F_DIG_HDMI_GENERIC_PACKET_CONTROL0__HDMI_GENERIC1_LINE),
            FV(F_DIG_HDMI_GENERIC_PACKET_CONTROL0__HDMI_GENERIC1_CONT, cont) | FV(F_DIG_HDMI_GENERIC_PACKET_CONTROL0__HDMI_GENERIC1_SEND, send) |
            FV(F_DIG_HDMI_GENERIC_PACKET_CONTROL0__HDMI_GENERIC1_LINE, line));
}

/* enc1_stream_encoder_hdmi_set_stream_attribute (o DIGxEncoderControl SETUP vem antes, por quem chama) */
static void hdmi_stream(amdgpu_t *a, amd_disp_t *d) {
    u32 i = d->phy;
    UPD(a, D_DIG_DIG_FE_CNTL(i), F_DIG_DIG_FE_CNTL__TMDS_PIXEL_ENCODING, 0);
    UPD(a, D_DIG_DIG_FE_CNTL(i), F_DIG_DIG_FE_CNTL__TMDS_COLOR_FORMAT, 0);
    u32 dc = d->req.bpc == 10 ? 1 : d->req.bpc == 12 ? 2 : d->req.bpc == 16 ? 3 : 0;
    bool over340 = d->phy_clk_khz >= 340000, lte = !over340 && d->req.lte340_scramble;
    upd(a, D_DIG_HDMI_CONTROL(i),
        MASK(F_DIG_HDMI_CONTROL__HDMI_PACKET_GEN_VERSION) | MASK(F_DIG_HDMI_CONTROL__HDMI_KEEPOUT_MODE) |
        MASK(F_DIG_HDMI_CONTROL__HDMI_DEEP_COLOR_ENABLE) | MASK(F_DIG_HDMI_CONTROL__HDMI_DATA_SCRAMBLE_EN) |
        MASK(F_DIG_HDMI_CONTROL__HDMI_NO_EXTRA_NULL_PACKET_FILLED) | MASK(F_DIG_HDMI_CONTROL__HDMI_CLOCK_CHANNEL_RATE) |
        MASK(F_DIG_HDMI_CONTROL__HDMI_DEEP_COLOR_DEPTH),
        FV(F_DIG_HDMI_CONTROL__HDMI_PACKET_GEN_VERSION, 1) | FV(F_DIG_HDMI_CONTROL__HDMI_KEEPOUT_MODE, 1) |
        FV(F_DIG_HDMI_CONTROL__HDMI_NO_EXTRA_NULL_PACKET_FILLED, 1) | FV(F_DIG_HDMI_CONTROL__HDMI_DEEP_COLOR_DEPTH, dc) |
        FV(F_DIG_HDMI_CONTROL__HDMI_DEEP_COLOR_ENABLE, dc ? 1 : 0) |
        FV(F_DIG_HDMI_CONTROL__HDMI_DATA_SCRAMBLE_EN, over340 || lte) | FV(F_DIG_HDMI_CONTROL__HDMI_CLOCK_CHANNEL_RATE, over340));
    d->scrambled = over340 || lte;
    upd(a, D_DIG_HDMI_VBI_PACKET_CONTROL(i), MASK(F_DIG_HDMI_VBI_PACKET_CONTROL__HDMI_GC_CONT) | MASK(F_DIG_HDMI_VBI_PACKET_CONTROL__HDMI_GC_SEND) |
        MASK(F_DIG_HDMI_VBI_PACKET_CONTROL__HDMI_NULL_SEND) | MASK(F_DIG_HDMI_VBI_PACKET_CONTROL__HDMI_ACP_SEND),
        FV(F_DIG_HDMI_VBI_PACKET_CONTROL__HDMI_GC_CONT, 1) | FV(F_DIG_HDMI_VBI_PACKET_CONTROL__HDMI_GC_SEND, 1) |
        FV(F_DIG_HDMI_VBI_PACKET_CONTROL__HDMI_NULL_SEND, 1));
    UPD(a, D_DIG_HDMI_INFOFRAME_CONTROL0(i), F_DIG_HDMI_INFOFRAME_CONTROL0__HDMI_AUDIO_INFO_SEND, 1);
    UPD(a, D_DIG_AFMT_INFOFRAME_CONTROL0(i), F_DIG_AFMT_INFOFRAME_CONTROL0__AFMT_AUDIO_INFO_UPDATE, 1);
    UPD(a, D_DIG_HDMI_INFOFRAME_CONTROL1(i), F_DIG_HDMI_INFOFRAME_CONTROL1__HDMI_AUDIO_INFO_LINE, 2);
    UPD(a, D_DIG_HDMI_GC(i), F_DIG_HDMI_GC__HDMI_GC_AVMUTE, 0);
    /* enc1_stream_encoder_update_hdmi_info_packets: só o AVI; os outros pacotes desligados */
    UPD(a, D_DIG_HDMI_DB_CONTROL(i), F_DIG_HDMI_DB_CONTROL__HDMI_DB_DISABLE, 1);
    u8 hb[3], sb[28];
    amd_hdmi_avi(&d->req, hb, sb);
    hdmi_packet(a, i, 0, hb, sb, true);
    for (u32 k = 1; k < 6; k++) hdmi_packet(a, i, k, hb, sb, false);
}

static void hdmi_stop_packets(amdgpu_t *a, u32 i) {
    WR(a, D_DIG_HDMI_GENERIC_PACKET_CONTROL0(i), 0);
    WR(a, D_DIG_HDMI_GENERIC_PACKET_CONTROL1(i), 0);
    WR(a, D_DIG_HDMI_GENERIC_PACKET_CONTROL2(i), 0);
    WR(a, D_DIG_HDMI_GENERIC_PACKET_CONTROL3(i), 0);
}

/* write_scdc_data: versão da fonte se o sink é SCDC 1, e o TMDS_CONFIG (embaralhamento + razão 1/40 acima de 340 MHz) */
static void hdmi_scdc(amdgpu_t *a, amd_disp_t *d) {
    const amd_conn_t *c = &a->vbios.conn[d->req.conn];
    if (c->ddc < 0) return;
    u8 ver = 0;
    if (amdgpu_i2c_read_reg(a, (u32)c->ddc, 0x54, 0x01, &ver, 1) == 0 && ver == 1) {
        u8 w[2] = { 0x02, 1 };
        amdgpu_i2c_write(a, (u32)c->ddc, 0x54, w, 2);
    }
    u8 w[2] = { 0x20, (u8)(d->phy_clk_khz > 340000 ? 3 : d->req.lte340_scramble ? 1 : 0) };
    amdgpu_i2c_write(a, (u32)c->ddc, 0x54, w, 2);
}

/* ------------------------------------------------------------ DisplayPort */
u32 amd_dp_mvid(u32 pix_khz, u8 rate) {
    u64 m = 0x8000ull * pix_khz;
    return (u32)(m / ((u64)rate * 27000u));
}

/* decide_dp_link_settings: o menor enlace (taxa, depois vias) que leva a banda do modo */
bool amd_dp_pick_link(u32 pix_khz, u32 bpc, u8 max_rate, u8 max_lanes, u8 *rate, u8 *lanes) {
    static const u8 rates[3] = { 0x06, 0x0A, 0x14 };
    u64 need = (u64)pix_khz * bpc * 3;
    for (u32 r = 0; r < 3 && rates[r] <= max_rate; r++)
        for (u8 l = 1; l <= max_lanes && l <= 4; l *= 2)
            if ((u64)rates[r] * 27000u * 8u * l >= need) { *rate = rates[r]; *lanes = l; return true; }
    return false;
}

static void dp_training_pattern(amdgpu_t *a, u32 i, u32 tps) {
    /* dcn10_link_encoder_set_dp_phy_pattern_training_pattern: TPS1..4 = 0..3 */
    WR(a, D_DP_DP_DPHY_TRAINING_PATTERN_SEL(i), tps);
    UPD(a, D_DP_DP_LINK_CNTL(i), F_DP_DP_LINK_CNTL__DP_LINK_TRAINING_COMPLETE, 0);
    UPD(a, D_DP_DP_DPHY_CNTL(i), F_DP_DP_DPHY_CNTL__DPHY_BYPASS, 0);
    UPD(a, D_DP_DP_DPHY_PRBS_CNTL(i), F_DP_DP_DPHY_PRBS_CNTL__DPHY_PRBS_EN, 0);
}

static void dp_video_mode(amdgpu_t *a, u32 i) {
    /* set_dp_phy_pattern_passthrough_mode (sem DP_DPHY_INTERNAL_CTRL no DCN 1.0) */
    upd(a, D_DP_DP_LINK_FRAMING_CNTL(i), MASK(F_DP_DP_LINK_FRAMING_CNTL__DP_IDLE_BS_INTERVAL) |
        MASK(F_DP_DP_LINK_FRAMING_CNTL__DP_VBID_DISABLE) | MASK(F_DP_DP_LINK_FRAMING_CNTL__DP_VID_ENHANCED_FRAME_MODE),
        FV(F_DP_DP_LINK_FRAMING_CNTL__DP_IDLE_BS_INTERVAL, 0x2000) | FV(F_DP_DP_LINK_FRAMING_CNTL__DP_VID_ENHANCED_FRAME_MODE, 1));
    UPD(a, D_DP_DP_DPHY_SCRAM_CNTL(i), F_DP_DP_DPHY_SCRAM_CNTL__DPHY_SCRAMBLER_BS_COUNT, 0x1FF);
    UPD(a, D_DP_DP_LINK_CNTL(i), F_DP_DP_LINK_CNTL__DP_LINK_TRAINING_COMPLETE, 1);
    UPD(a, D_DP_DP_DPHY_CNTL(i), F_DP_DP_DPHY_CNTL__DPHY_BYPASS, 0);
    UPD(a, D_DP_DP_DPHY_PRBS_CNTL(i), F_DP_DP_DPHY_PRBS_CNTL__DPHY_PRBS_EN, 0);
}

static int dp_lane_set(amdgpu_t *a, amd_disp_t *d, u32 aux) {
    u8 set[4];
    for (u32 l = 0; l < d->link.lanes; l++) {
        set[l] = (u8)(d->link.vs[l] | d->link.pe[l] << 3 | (d->link.vs[l] == 3 ? 4 : 0) | (d->link.pe[l] == 3 ? 0x20 : 0));
        int r = transmitter(a, d, 11, set[l]);               /* SETUP_VSEMPH */
        if (r) return r;
    }
    return amdgpu_dpcd_write(a, aux, 0x103, set, d->link.lanes);
}

static bool lanes_ok(const u8 st[6], u32 lanes, u8 mask) {
    for (u32 l = 0; l < lanes; l++) if (((st[l / 2] >> (4 * (l % 2))) & mask) != mask) return false;
    return true;
}

static void adjust(amd_disp_t *d, const u8 st[6]) {
    for (u32 l = 0; l < d->link.lanes; l++) {
        u8 req = (u8)(st[4 + l / 2] >> (4 * (l % 2)));
        d->link.vs[l] = req & 3;
        d->link.pe[l] = (req >> 2) & 3;
    }
}

/* treino 8b/10b (DP 1.2, seções 3.5.1.2/3): clock recovery com TPS1, equalização com TPS2 ou TPS3 */
static int dp_train(amdgpu_t *a, amd_disp_t *d, u32 aux) {
    u32 i = d->phy;
    u8 v[2] = { d->link.rate, (u8)(d->link.lanes | (d->link.enhanced_framing ? 0x80 : 0)) };
    if (amdgpu_dpcd_write(a, aux, 0x100, v, 2)) return -MOS_EIO;
    memset(d->link.vs, 0, 4);
    memset(d->link.pe, 0, 4);
    u8 rd_int = 0;
    amdgpu_dpcd_read(a, aux, 0x00E, &rd_int, 1);
    u32 eq_wait = (rd_int & 0x7F) ? (u32)(rd_int & 0x7F) * 4000 : 400;
    /* clock recovery */
    dp_training_pattern(a, i, 0);
    u8 tp = 0x21;                                            /* TPS1, embaralhamento desligado */
    if (amdgpu_dpcd_write(a, aux, 0x102, &tp, 1) || dp_lane_set(a, d, aux)) return -MOS_EIO;
    u8 st[6];
    u32 same = 0, tries = 0;
    u8 last_vs = 0xFF;
    for (;;) {
        DELAY_US(100);
        if (amdgpu_dpcd_read(a, aux, 0x202, st, 6)) return -MOS_EIO;
        if (lanes_ok(st, d->link.lanes, 0x1)) break;
        if (++tries > 10 || d->link.vs[0] == 3) return -MOS_EIO;
        adjust(d, st);
        if (d->link.vs[0] == last_vs) { if (++same >= 5) return -MOS_EIO; } else { same = 0; last_vs = d->link.vs[0]; }
        if (dp_lane_set(a, d, aux)) return -MOS_EIO;
    }
    /* equalização */
    u32 tps = d->link.tps3 ? 2 : 1;
    dp_training_pattern(a, i, tps);
    tp = (u8)(0x20 | (tps + 1));
    if (amdgpu_dpcd_write(a, aux, 0x102, &tp, 1)) return -MOS_EIO;
    for (u32 n = 0; n < 5; n++) {
        DELAY_US(eq_wait);
        if (amdgpu_dpcd_read(a, aux, 0x202, st, 6)) return -MOS_EIO;
        if (!lanes_ok(st, d->link.lanes, 0x1)) return -MOS_EIO;          /* perdeu o clock recovery */
        if (lanes_ok(st, d->link.lanes, 0x7) && (st[2] & 1)) {           /* CR, EQ, símbolo e alinhamento */
            tp = 0;
            amdgpu_dpcd_write(a, aux, 0x102, &tp, 1);
            dp_video_mode(a, i);
            return 0;
        }
        adjust(d, st);
        if (dp_lane_set(a, d, aux)) return -MOS_EIO;
    }
    return -MOS_EIO;
}

/* enc1_stream_encoder_dp_set_stream_attribute */
static void dp_stream(amdgpu_t *a, amd_disp_t *d) {
    u32 i = d->phy;
    const amd_timing_t *t = &d->req.t;
    u32 depth = d->req.bpc == 6 ? 0 : d->req.bpc == 8 ? 1 : d->req.bpc == 10 ? 2 : d->req.bpc == 12 ? 3 : 4;
    upd(a, D_DP_DP_PIXEL_FORMAT(i), MASK(F_DP_DP_PIXEL_FORMAT__DP_PIXEL_ENCODING) | MASK(F_DP_DP_PIXEL_FORMAT__DP_COMPONENT_DEPTH),
        FV(F_DP_DP_PIXEL_FORMAT__DP_PIXEL_ENCODING, 0) | FV(F_DP_DP_PIXEL_FORMAT__DP_COMPONENT_DEPTH, depth));
    u32 cbpc = d->req.bpc == 8 ? 1 : d->req.bpc == 10 ? 2 : d->req.bpc == 12 ? 3 : 0;
    WR(a, D_DP_DP_MSA_COLORIMETRY(i), FV(F_DP_DP_MSA_COLORIMETRY__DP_MSA_MISC0, cbpc << 5));   /* RGB sRGB */
    WR(a, D_DP_DP_MSA_MISC(i), RR(a, D_DP_DP_MSA_MISC(i)) & ~0xC0u);
    WR(a, D_DP_DP_MSA_TIMING_PARAM1(i), FV(F_DP_DP_MSA_TIMING_PARAM1__DP_MSA_HTOTAL, t->h_total) | FV(F_DP_DP_MSA_TIMING_PARAM1__DP_MSA_VTOTAL, t->v_total));
    u32 hbp = t->h_total - t->h_active - t->h_front_porch - t->h_sync_width;
    u32 vstart = t->v_total - t->v_active - t->v_front_porch;
    WR(a, D_DP_DP_MSA_TIMING_PARAM2(i), FV(F_DP_DP_MSA_TIMING_PARAM2__DP_MSA_HSTART, t->h_sync_width + hbp) |
       FV(F_DP_DP_MSA_TIMING_PARAM2__DP_MSA_VSTART, vstart));
    WR(a, D_DP_DP_MSA_TIMING_PARAM3(i), FV(F_DP_DP_MSA_TIMING_PARAM3__DP_MSA_HSYNCWIDTH, t->h_sync_width) |
       FV(F_DP_DP_MSA_TIMING_PARAM3__DP_MSA_HSYNCPOLARITY, !t->hsync_pos) | FV(F_DP_DP_MSA_TIMING_PARAM3__DP_MSA_VSYNCWIDTH, t->v_sync_width) |
       FV(F_DP_DP_MSA_TIMING_PARAM3__DP_MSA_VSYNCPOLARITY, !t->vsync_pos));
    WR(a, D_DP_DP_MSA_TIMING_PARAM4(i), FV(F_DP_DP_MSA_TIMING_PARAM4__DP_MSA_HWIDTH, t->h_active) | FV(F_DP_DP_MSA_TIMING_PARAM4__DP_MSA_VHEIGHT, t->v_active));
}

/* enc1_stream_encoder_dp_unblank */
static void dp_unblank(amdgpu_t *a, amd_disp_t *d) {
    u32 i = d->phy;
    UPD(a, D_DP_DP_VID_TIMING(i), F_DP_DP_VID_TIMING__DP_VID_M_N_GEN_EN, 0);
    UPD(a, D_DP_DP_VID_N(i), F_DP_DP_VID_N__DP_VID_N, 0x8000);
    UPD(a, D_DP_DP_VID_M(i), F_DP_DP_VID_M__DP_VID_M, amd_dp_mvid(d->req.t.pix_clk_khz, d->link.rate));
    upd(a, D_DP_DP_VID_TIMING(i), MASK(F_DP_DP_VID_TIMING__DP_VID_M_N_GEN_EN) | MASK(F_DP_DP_VID_TIMING__DP_VID_N_MUL),
        FV(F_DP_DP_VID_TIMING__DP_VID_M_N_GEN_EN, 1));
    UPD(a, D_DIG_DIG_FE_CNTL(i), F_DIG_DIG_FE_CNTL__DIG_START, 1);
    UPD(a, D_DP_DP_STEER_FIFO(i), F_DP_DP_STEER_FIFO__DP_STEER_FIFO_RESET, 0);
    DELAY_US(100);
    UPD(a, D_DP_DP_VID_STREAM_CNTL(i), F_DP_DP_VID_STREAM_CNTL__DP_VID_STREAM_ENABLE, 1);
}

/* enc1_stream_encoder_dp_blank */
static void dp_blank(amdgpu_t *a, u32 i) {
    if (!FG(RR(a, D_DP_DP_VID_STREAM_CNTL(i)), F_DP_DP_VID_STREAM_CNTL__DP_VID_STREAM_ENABLE)) return;
    UPD(a, D_DP_DP_VID_STREAM_CNTL(i), F_DP_DP_VID_STREAM_CNTL__DP_VID_STREAM_DIS_DEFER, 2);
    UPD(a, D_DP_DP_VID_STREAM_CNTL(i), F_DP_DP_VID_STREAM_CNTL__DP_VID_STREAM_ENABLE, 0);
    for (u32 k = 0; k < 10 * 501 && FG(RR(a, D_DP_DP_VID_STREAM_CNTL(i)), F_DP_DP_VID_STREAM_CNTL__DP_VID_STREAM_STATUS); k++) DELAY_US(10);
    UPD(a, D_DP_DP_STEER_FIFO(i), F_DP_DP_STEER_FIFO__DP_STEER_FIFO_RESET, 1);
}

/* caps do sink e escolha do enlace; liga o PHY e treina, caindo de taxa/vias se não fechar */
static int dp_link_up(amdgpu_t *a, amd_disp_t *d) {
    const amd_conn_t *c = &a->vbios.conn[d->req.conn];
    u32 aux = (u32)(c->ddc >= 0 ? c->ddc : 0);
    u8 cap[16];
    if (amdgpu_dpcd_read(a, aux, 0x000, cap, 16)) return -MOS_EIO;
    u8 max_rate = cap[1] > 0x14 ? 0x14 : cap[1], max_lanes = cap[2] & 0x1F;
    d->link.enhanced_framing = (cap[2] & 0x80) != 0;
    d->link.tps3 = (cap[2] & 0x40) != 0;
    u8 on = 1;
    amdgpu_dpcd_write(a, aux, 0x600, &on, 1);                /* sink em D0 */
    u8 rate, lanes;
    if (!amd_dp_pick_link(d->req.t.pix_clk_khz, d->req.bpc, max_rate, max_lanes, &rate, &lanes)) return -MOS_ERANGE;
    for (;;) {
        d->link.rate = rate; d->link.lanes = lanes;
        d->phy_clk_khz = (u32)rate * 27000u;
        dig_be_setup(a, d);
        /* enc1_configure_encoder */
        WR(a, D_DP_DP_CONFIG(d->phy), FV(F_DP_DP_CONFIG__DP_UDI_LANES, lanes - 1));
        UPD(a, D_DP_DP_DPHY_SCRAM_CNTL(d->phy), F_DP_DP_DPHY_SCRAM_CNTL__DPHY_SCRAMBLER_ADVANCE, 1);
        if (dig_encoder(a, d, 0x0F) || transmitter(a, d, 1, 0)) return -MOS_EIO;
        if (!dp_train(a, d, aux)) return 0;
        transmitter(a, d, 0, 0);
        /* reduz: menos vias na mesma taxa só se ainda couber; senão taxa menor */
        u8 nr = rate, nl = lanes;
        if (rate == 0x14) nr = 0x0A; else if (rate == 0x0A) nr = 0x06; else return -MOS_EIO;
        if (!amd_dp_pick_link(d->req.t.pix_clk_khz, d->req.bpc, nr, max_lanes, &nr, &nl)) return -MOS_EIO;
        rate = nr; lanes = nl;
    }
}

/* ------------------------------------------------------------ sequência */
static void disp_off(amdgpu_t *a, int otg) {
    amd_disp_t *d = &a->disp[otg];
    if (!d->on) return;
    u32 i = d->phy;
    amdgpu_audio_off(a, otg);
    if (d->dp) {
        dp_blank(a, i);
        const amd_conn_t *c = &a->vbios.conn[d->req.conn];
        u8 off = 2;
        amdgpu_dpcd_write(a, (u32)(c->ddc >= 0 ? c->ddc : 0), 0x600, &off, 1);   /* sink em D3 */
    } else {
        UPD(a, D_DIG_HDMI_GC(i), F_DIG_HDMI_GC__HDMI_GC_AVMUTE, 1);
        hdmi_stop_packets(a, i);
    }
    /* dcn10_link_encoder_disable_output */
    transmitter(a, d, 0, 0);
    if (d->dp) {
        WR(a, D_DP_DP_DPHY_TRAINING_PATTERN_SEL(i), 0);
        UPD(a, D_DP_DP_LINK_CNTL(i), F_DP_DP_LINK_CNTL__DP_LINK_TRAINING_COMPLETE, 0);
    }
    u32 fe = FG(RR(a, D_DIG_DIG_BE_CNTL(i)), F_DIG_DIG_BE_CNTL__DIG_FE_SOURCE_SELECT) & ~(1u << i);
    UPD(a, D_DIG_DIG_BE_CNTL(i), F_DIG_DIG_BE_CNTL__DIG_FE_SOURCE_SELECT, fe);
    for (u32 k = 0; k < d->dcn.npipes; k++) amdgpu_dcn_disable_pipe(a, d->dcn.pipe[k]);
    amdgpu_dcn_disable_otg(a, otg);
    d->on = false;
}

void amdgpu_modeset_off(amdgpu_t *a) {
    if (!a) return;
    for (int o = 0; o < AMD_DCN_OTGS; o++) disp_off(a, o);
    clocks_set(a, NULL, 0);
}

static int disp_on(amdgpu_t *a, int otg, const amd_mode_req_t *r, const amd_dcnbw_pipe_t *bw, const u32 *pipes) {
    amd_disp_t *d = &a->disp[otg];
    const amd_conn_t *c = &a->vbios.conn[r->conn];
    memset(d, 0, sizeof *d);
    d->audio_ep = -1;
    d->req = *r;
    d->bw = *bw;
    d->phy = phy_index(c);
    d->dp = c->type == AMD_CONN_DP || c->type == AMD_CONN_EDP;
    d->hdmi = c->type == AMD_CONN_HDMI;
    if (d->dp) d->req.ycbcr444 = false;                    /* DP: RGB */
    u32 tmds_num = r->bpc == 10 ? 30 : r->bpc == 12 ? 36 : 24;
    d->phy_clk_khz = d->hdmi ? r->t.pix_clk_khz * tmds_num / 24 : r->t.pix_clk_khz;
    if (d->hdmi && d->phy_clk_khz > 600000) return -MOS_ERANGE;   /* além do HDMI 2.0 */
    d->dcn = (amd_dcn_stream_t){ .otg = otg, .t = r->t, .dp = d->dp, .bpc = r->bpc, .ycbcr444 = d->req.ycbcr444,
        .addr = r->addr, .pitch_px = r->pitch_px, .format = r->format, .sw_mode = r->sw_mode,
        .pipe_aligned = r->pipe_aligned, .rb_aligned = r->rb_aligned, .dcc = r->dcc, .dcc_ind_64b = r->dcc_ind_64b,
        .meta_addr = r->meta_addr, .meta_pitch_px = r->meta_pitch_px, .npipes = bw->dpp, .pipe = { pipes[0], pipes[1] },
        .bw = &d->bw };
    /* enable_stream_timing: PLL de pixel, OTG e pipes */
    int e = pixel_clock(a, d, otg);
    if (e) return e;
    e = amdgpu_dcn_enable_stream(a, &d->dcn);
    if (e) return e;
    d->on = true;                                            /* daqui em diante disp_off desfaz o que foi feito */
    /* setup_stream_encoder: DIG do front end ligado ao OTG */
    UPD(a, D_DIG_DIG_FE_CNTL(d->phy), F_DIG_DIG_FE_CNTL__DIG_SOURCE_SELECT, (u32)otg);
    if (d->dp) {
        e = dp_link_up(a, d);
        if (e) return e;
        dp_stream(a, d);
        dp_unblank(a, d);
    } else {
        dig_be_setup(a, d);
        e = dig_encoder(a, d, 0x0F);                         /* STREAM_SETUP */
        if (e) return e;
        hdmi_stream(a, d);
        hdmi_scdc(a, d);
        e = transmitter(a, d, 1, 0);                         /* ENABLE */
        if (e) return e;
    }
    amdgpu_audio_on(a, otg);                                 /* áudio pelo HDMI/DP se o monitor tem */
    return 0;
}

int amdgpu_modeset(amdgpu_t *a, const amd_mode_req_t *req, u32 n) {
    if (!a || n > AMD_MAX_STREAMS || (n && !req)) return -MOS_EINVAL;
    for (u32 k = 0; k < n; k++) {
        if (req[k].conn >= a->vbios.nconn || !req[k].t.pix_clk_khz) return -MOS_EINVAL;
        for (u32 j = 0; j < k; j++) if (req[j].conn == req[k].conn) return -MOS_EINVAL;
        u8 ty = a->vbios.conn[req[k].conn].type;
        if (ty != AMD_CONN_DP && ty != AMD_CONN_EDP && ty != AMD_CONN_HDMI) return -MOS_EINVAL;
    }
    /* valida antes de mexer no hardware */
    amd_dcnbw_stream_t bs[AMD_MAX_STREAMS];
    for (u32 k = 0; k < n; k++) {
        const amd_mode_req_t *r = &req[k];
        bool dp = a->vbios.conn[r->conn].type != AMD_CONN_HDMI;
        bs[k] = (amd_dcnbw_stream_t){ .h_total = r->t.h_total, .h_active = r->t.h_active, .h_front_porch = r->t.h_front_porch,
            .h_sync_width = r->t.h_sync_width, .v_total = r->t.v_total, .v_active = r->t.v_active,
            .v_front_porch = r->t.v_front_porch, .v_sync_width = r->t.v_sync_width, .pixel_clock_mhz = r->t.pix_clk_khz / 1000.0,
            .hdmi = !dp, .bpc = r->bpc, .ycbcr420 = false, .has_plane = true, .bytes_per_pixel = amd_dcn_bytes_per_pixel(r->format),
            .sw_mode = r->sw_mode, .src_width = r->t.h_active, .src_height = r->t.v_active, .dcc = r->dcc };
    }
    static amd_dcnbw_out_t bw;
    if (n && amd_dcnbw_validate(&a->bw_soc, bs, n, &bw)) return -MOS_ERANGE;
    u32 pipes_needed = 0;
    for (u32 k = 0; k < n; k++) pipes_needed += bw.s[k].dpp;
    if (pipes_needed > (a->bw_soc.num_pipes ? a->bw_soc.num_pipes : 4)) return -MOS_ERANGE;

    /* desliga tudo o que está ligado (troca de modo completa, como a do dc com full_update) */
    for (int o = 0; o < AMD_DCN_OTGS; o++) disp_off(a, o);
    if (!n) { clocks_set(a, NULL, 0); return 0; }
    int e = clocks_set(a, &bw, n);
    if (e) return e;
    amdgpu_dcn_watermarks(a, &bw);
    smu_wm_ranges(a);
    /* pipes: a tela k usa o OTG k; os pipes vão em ordem, a segunda metade de uma tela dividida no último livre
     * (o Linux põe o par no pipe 3 com uma tela só: resource_find_free_secondary_pipe_legacy) */
    u32 nextp = 0, lastp = (a->bw_soc.num_pipes ? a->bw_soc.num_pipes : 4) - 1;
    for (u32 k = 0; k < n; k++) {
        u32 p[2] = { nextp++, 0 };
        if (bw.s[k].dpp == 2) p[1] = lastp--;
        e = disp_on(a, (int)k, &req[k], &bw.s[k], p);
        if (e) { kprintf("amdgpu: tela %u não ligou (%d)\n", k, e); amdgpu_modeset_off(a); return e; }
    }
    a->modeset_count++;
    return 0;
}

int amdgpu_modeset_flip(amdgpu_t *a, int otg, u64 addr) {
    if (!a || otg < 0 || otg >= AMD_DCN_OTGS || !a->disp[otg].on) return -MOS_EINVAL;
    amdgpu_dcn_flip(a, &a->disp[otg].dcn, addr);
    a->disp[otg].req.addr = addr;
    return 0;
}
