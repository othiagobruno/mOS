/* mOS — áudio pela saída de vídeo no DCN 1.0. Porte do display/dc da AMD (MIT):
 *   dce_audio.c: endpoint AZ (o codec que o controlador HDA 1002:15de enxerga): descritores de áudio do EDID,
 *   alocação de alto-falantes, nome do monitor, latências, HBR, liga/desliga do pino; DTO do relógio de áudio
 *   no DCCG (HDMI pelo DTO0 no relógio de pixel, DP pelo DTO1 no DPREFCLK)
 *   dcn10_stream_encoder.c: HDMI (ACR com N/CTS, canal 60958, fonte do AFMT, canais) e DP (SDP de áudio).
 * O controlador HDA e o driver de áudio do sistema são da Larissa; aqui só o lado da GPU. */
#include "amdgpu.h"
#include "regs.h"
#include "dcn_regs.h"
#include "az_regs.h"
#include "edid.h"
#include "ucode.h"
#ifndef MOS_HOST_TEST
#include "syscall.h"
#endif

#ifdef MOS_HOST_TEST
#define RR(a, o)    ((void)(a), amd_host_rreg(o))
#define WR(a, o, v) ((void)(a), amd_host_wreg(o, v))
#else
#define RR(a, o)    mmio_r32((a)->mmio, (o))
#define WR(a, o, v) mmio_w32((a)->mmio, (o), (v))
#endif

#define FV_(s, m, v)     ((((u32)(v)) << (s)) & (m))
#define FV(f, v)         FV_(f, v)
#define FG_(x, s, m)     (((x) & (m)) >> (s))
#define FG(x, f)         FG_(x, f)
#define UPD_(a, r, s, m, v) upd(a, r, m, ((u32)(v) << (s)) & (m))
#define UPD(a, r, f, v)  UPD_(a, r, f, v)
#define SETF_(x, s, m, v) ((x) = ((x) & ~(m)) | ((((u32)(v)) << (s)) & (m)))
#define SETF(x, f, v)    SETF_(x, f, v)
static void upd(amdgpu_t *a, u32 r, u32 mask, u32 bits) { WR(a, r, (RR(a, r) & ~mask) | bits); }

/* ------------------------------------------------------------ registradores indiretos do endpoint */
static void az_w(amdgpu_t *a, u32 ep, u32 ix, u32 v) {
    WR(a, D_AZF0ENDPOINT_AZALIA_F0_CODEC_ENDPOINT_INDEX(ep), FV(F_AZF0ENDPOINT_AZALIA_F0_CODEC_ENDPOINT_INDEX__AZALIA_ENDPOINT_REG_INDEX, ix));
    WR(a, D_AZF0ENDPOINT_AZALIA_F0_CODEC_ENDPOINT_DATA(ep), v);
}
static u32 az_r(amdgpu_t *a, u32 ep, u32 ix) {
    WR(a, D_AZF0ENDPOINT_AZALIA_F0_CODEC_ENDPOINT_INDEX(ep), FV(F_AZF0ENDPOINT_AZALIA_F0_CODEC_ENDPOINT_INDEX__AZALIA_ENDPOINT_REG_INDEX, ix));
    return RR(a, D_AZF0ENDPOINT_AZALIA_F0_CODEC_ENDPOINT_DATA(ep));
}

u32 amd_audio_endpoints(const amdgpu_t *a) { return a->variant == AMD_RAVEN2 ? 3 : 4; }   /* dcn10 res_cap num_audio */

/* dce_aud_hw_init (só a instância 0 programa os parâmetros da função) */
void amdgpu_audio_init(amdgpu_t *a) {
    u32 v = az_r(a, 0, X_AZALIA_F0_CODEC_PIN_CONTROL_HOT_PLUG_CONTROL);
    SETF(v, XF_AZALIA_F0_CODEC_PIN_CONTROL_HOT_PLUG_CONTROL__CLOCK_GATING_DISABLE, 1);
    az_w(a, 0, X_AZALIA_F0_CODEC_PIN_CONTROL_HOT_PLUG_CONTROL, v);
    UPD(a, D_AZALIA_F0_CODEC_FUNCTION_PARAMETER_SUPPORTED_SIZE_RATES,
        F_AZALIA_F0_CODEC_FUNCTION_PARAMETER_SUPPORTED_SIZE_RATES__AUDIO_RATE_CAPABILITIES, 0x70);
    upd(a, D_AZALIA_F0_CODEC_FUNCTION_PARAMETER_POWER_STATES,
        FV(F_AZALIA_F0_CODEC_FUNCTION_PARAMETER_POWER_STATES__CLKSTOP, ~0u) | FV(F_AZALIA_F0_CODEC_FUNCTION_PARAMETER_POWER_STATES__EPSS, ~0u),
        FV(F_AZALIA_F0_CODEC_FUNCTION_PARAMETER_POWER_STATES__CLKSTOP, 1) | FV(F_AZALIA_F0_CODEC_FUNCTION_PARAMETER_POWER_STATES__EPSS, 1));
    SETF(v, XF_AZALIA_F0_CODEC_PIN_CONTROL_HOT_PLUG_CONTROL__CLOCK_GATING_DISABLE, 0);
    az_w(a, 0, X_AZALIA_F0_CODEC_PIN_CONTROL_HOT_PLUG_CONTROL, v);
}

/* ------------------------------------------------------------ banda de áudio no HDMI (check_audio_bandwidth_hdmi) */
#define RATE_32  0x01
#define RATE_44  0x02
#define RATE_48  0x04
#define RATE_88  0x08
#define RATE_96  0x10
#define RATE_176 0x20
#define RATE_192 0x40

u8 amd_audio_hdmi_rates(const amd_timing_t *t, u32 bpc, u32 channels, u8 rates) {
    bool l48 = false, l88 = false, l96 = false, l176 = false;
    u32 pix100 = t->pix_clk_khz * 10;
    if (channels > 2) {
        if (pix100 <= 270000 && t->v_active <= 576) l48 = true;
        else if (pix100 <= 540000 && t->v_active <= 576) l176 = true;
    }
    u32 refresh = (u32)((u64)t->pix_clk_khz * 1000 / ((u64)t->h_total * t->v_total));
    u32 h_blank = t->h_total - t->h_active - 58 - 16;
    u64 samples = (u64)h_blank * 10 / 32 * t->v_active * refresh / 10;
    samples = samples * (bpc == 10 ? 5 : bpc == 12 ? 6 : 4) / 4;
    if (samples < 88200) l48 = true;
    else if (samples < 96000) l88 = true;
    else if (samples < 176400) l96 = true;
    else if (samples < 192000) l176 = true;
    if (l176) rates &= (u8)~RATE_192;
    if (l96) rates &= (u8)~(RATE_192 | RATE_176);
    if (l88) rates &= (u8)~(RATE_192 | RATE_176 | RATE_96);
    if (l48) rates &= (u8)~(RATE_192 | RATE_176 | RATE_96 | RATE_88);
    return rates;
}

/* ------------------------------------------------------------ endpoint (dce_aud_az_configure) */
static void az_configure(amdgpu_t *a, u32 ep, const amd_disp_t *d, const edid_info_t *e, const u8 *edid) {
    u32 v = az_r(a, ep, X_AZALIA_F0_CODEC_PIN_CONTROL_HOT_PLUG_CONTROL);
    SETF(v, XF_AZALIA_F0_CODEC_PIN_CONTROL_HOT_PLUG_CONTROL__CLOCK_GATING_DISABLE, 1);
    az_w(a, ep, X_AZALIA_F0_CODEC_PIN_CONTROL_HOT_PLUG_CONTROL, v);
    /* alto-falantes e tipo de conexão */
    v = az_r(a, ep, X_AZALIA_F0_CODEC_PIN_CONTROL_CHANNEL_SPEAKER);
    SETF(v, XF_AZALIA_F0_CODEC_PIN_CONTROL_CHANNEL_SPEAKER__SPEAKER_ALLOCATION, e->speaker_alloc & 0x7F);
    SETF(v, XF_AZALIA_F0_CODEC_PIN_CONTROL_CHANNEL_SPEAKER__LFE_PLAYBACK_LEVEL, 0);
    SETF(v, XF_AZALIA_F0_CODEC_PIN_CONTROL_CHANNEL_SPEAKER__HDMI_CONNECTION, d->hdmi);
    SETF(v, XF_AZALIA_F0_CODEC_PIN_CONTROL_CHANNEL_SPEAKER__DP_CONNECTION, d->dp);
    u32 extra = FG(v, XF_AZALIA_F0_CODEC_PIN_CONTROL_CHANNEL_SPEAKER__EXTRA_CONNECTION_INFO) & ~1u;
    SETF(v, XF_AZALIA_F0_CODEC_PIN_CONTROL_CHANNEL_SPEAKER__EXTRA_CONNECTION_INFO, extra);
    az_w(a, ep, X_AZALIA_F0_CODEC_PIN_CONTROL_CHANNEL_SPEAKER, v);
    v = az_r(a, ep, X_AZALIA_F0_CODEC_PIN_CONTROL_ACP_DATA);
    SETF(v, XF_AZALIA_F0_CODEC_PIN_CONTROL_ACP_DATA__SUPPORTS_AI, e->speaker_alloc >> 7);
    az_w(a, ep, X_AZALIA_F0_CODEC_PIN_CONTROL_ACP_DATA, v);
    /* um descritor por código de formato (1..15), com o SAD de mais canais de cada código */
    bool ac3 = false;
    for (u32 fi = 0; fi < 15; fi++) {
        u8 code = (u8)(fi + 1);
        u32 dv = 0;
        int best = -1;
        if (code != 9 && code != 13)                        /* 1-bit audio e DST ficam de fora */
            for (u32 k = 0; k < e->nsad; k++)
                if (e->sad[k].format == code && (best < 0 || e->sad[k].channels > e->sad[best].channels)) best = (int)k;
        if (best >= 0) {
            u8 ch = e->sad[best].channels, rates = e->sad[best].rates, b2 = e->sad[best].byte2;
            if (code == 1) {                                /* LPCM: taxas que cabem no blanking, byte 2 = tamanhos */
                if (d->hdmi) rates = amd_audio_hdmi_rates(&d->req.t, d->req.bpc, ch, rates);
                SETF(dv, XF_AZALIA_F0_CODEC_PIN_CONTROL_AUDIO_DESCRIPTOR0__SUPPORTED_FREQUENCIES_STEREO, rates);
            } else if (code == 2) ac3 = true;
            SETF(dv, XF_AZALIA_F0_CODEC_PIN_CONTROL_AUDIO_DESCRIPTOR0__MAX_CHANNELS, ch - 1);
            SETF(dv, XF_AZALIA_F0_CODEC_PIN_CONTROL_AUDIO_DESCRIPTOR0__SUPPORTED_FREQUENCIES, rates);
            SETF(dv, XF_AZALIA_F0_CODEC_PIN_CONTROL_AUDIO_DESCRIPTOR0__DESCRIPTOR_BYTE_2, b2);
        }
        az_w(a, ep, X_AZALIA_F0_CODEC_PIN_CONTROL_AUDIO_DESCRIPTOR0 + fi, dv);
    }
    if (ac3) WR(a, D_AZALIA_F0_CODEC_FUNCTION_PARAMETER_STREAM_FORMATS, 0x05);
    /* HBR (192 kHz em 8 canais) se o blanking leva */
    u8 hbr = d->hdmi ? amd_audio_hdmi_rates(&d->req.t, d->req.bpc, 8, RATE_192) : RATE_192;
    v = az_r(a, ep, X_AZALIA_F0_CODEC_PIN_CONTROL_RESPONSE_HBR);
    SETF(v, XF_AZALIA_F0_CODEC_PIN_CONTROL_RESPONSE_HBR__HBR_CAPABLE, hbr ? 1 : 0);
    az_w(a, ep, X_AZALIA_F0_CODEC_PIN_CONTROL_RESPONSE_HBR, v);
    v = az_r(a, ep, X_AZALIA_F0_CODEC_PIN_CONTROL_RESPONSE_LIPSYNC);
    SETF(v, XF_AZALIA_F0_CODEC_PIN_CONTROL_RESPONSE_LIPSYNC__VIDEO_LIPSYNC, e->video_latency);
    SETF(v, XF_AZALIA_F0_CODEC_PIN_CONTROL_RESPONSE_LIPSYNC__AUDIO_LIPSYNC, e->audio_latency);
    az_w(a, ep, X_AZALIA_F0_CODEC_PIN_CONTROL_RESPONSE_LIPSYNC, v);
    /* fabricante e produto crus do EDID (bytes 8..11), nome do monitor */
    v = 0;
    SETF(v, XF_AZALIA_F0_CODEC_PIN_CONTROL_SINK_INFO0__MANUFACTURER_ID, (u32)edid[8] | (u32)edid[9] << 8);
    SETF(v, XF_AZALIA_F0_CODEC_PIN_CONTROL_SINK_INFO0__PRODUCT_ID, (u32)edid[10] | (u32)edid[11] << 8);
    az_w(a, ep, X_AZALIA_F0_CODEC_PIN_CONTROL_SINK_INFO0, v);
    u8 name[20] = {0};
    u32 nlen = 0;
    while (nlen < 13 && e->name[nlen]) { name[nlen] = (u8)e->name[nlen]; nlen++; }
    u32 slen = 0;
    while (name[slen++] != 0) if (slen >= 18) break;        /* conta o terminador, como o dce_audio */
    v = 0;
    SETF(v, XF_AZALIA_F0_CODEC_PIN_CONTROL_SINK_INFO1__SINK_DESCRIPTION_LEN, slen);
    az_w(a, ep, X_AZALIA_F0_CODEC_PIN_CONTROL_SINK_INFO1, v);
    /* port_id padrão do dc sem container id */
    v = 0; SETF(v, XF_AZALIA_F0_CODEC_PIN_CONTROL_SINK_INFO2__PORT_ID0, 0x5558859eu); az_w(a, ep, X_AZALIA_F0_CODEC_PIN_CONTROL_SINK_INFO2, v);
    v = 0; SETF(v, XF_AZALIA_F0_CODEC_PIN_CONTROL_SINK_INFO3__PORT_ID1, 0x0d989449u); az_w(a, ep, X_AZALIA_F0_CODEC_PIN_CONTROL_SINK_INFO3, v);
    /* SINK_INFO4..7: 4 caracteres cada (DESCRIPTION0..15); SINK_INFO8: os 2 últimos */
    static const u32 si[4] = { X_AZALIA_F0_CODEC_PIN_CONTROL_SINK_INFO4, X_AZALIA_F0_CODEC_PIN_CONTROL_SINK_INFO5,
                               X_AZALIA_F0_CODEC_PIN_CONTROL_SINK_INFO6, X_AZALIA_F0_CODEC_PIN_CONTROL_SINK_INFO7 };
    for (u32 r = 0; r < 4; r++) {
        v = 0;
        SETF(v, XF_AZALIA_F0_CODEC_PIN_CONTROL_SINK_INFO4__DESCRIPTION0, name[4 * r]);
        SETF(v, XF_AZALIA_F0_CODEC_PIN_CONTROL_SINK_INFO4__DESCRIPTION1, name[4 * r + 1]);
        SETF(v, XF_AZALIA_F0_CODEC_PIN_CONTROL_SINK_INFO4__DESCRIPTION2, name[4 * r + 2]);
        SETF(v, XF_AZALIA_F0_CODEC_PIN_CONTROL_SINK_INFO4__DESCRIPTION3, name[4 * r + 3]);
        az_w(a, ep, si[r], v);
    }
    v = 0;
    SETF(v, XF_AZALIA_F0_CODEC_PIN_CONTROL_SINK_INFO8__DESCRIPTION16, name[16]);
    SETF(v, XF_AZALIA_F0_CODEC_PIN_CONTROL_SINK_INFO8__DESCRIPTION17, name[17]);
    az_w(a, ep, X_AZALIA_F0_CODEC_PIN_CONTROL_SINK_INFO8, v);
    v = az_r(a, ep, X_AZALIA_F0_CODEC_PIN_CONTROL_HOT_PLUG_CONTROL);
    SETF(v, XF_AZALIA_F0_CODEC_PIN_CONTROL_HOT_PLUG_CONTROL__CLOCK_GATING_DISABLE, 0);
    az_w(a, ep, X_AZALIA_F0_CODEC_PIN_CONTROL_HOT_PLUG_CONTROL, v);
}

static void az_enable(amdgpu_t *a, u32 ep, bool on) {
    u32 v = az_r(a, ep, X_AZALIA_F0_CODEC_PIN_CONTROL_HOT_PLUG_CONTROL);
    SETF(v, XF_AZALIA_F0_CODEC_PIN_CONTROL_HOT_PLUG_CONTROL__CLOCK_GATING_DISABLE, 1);
    if (on) SETF(v, XF_AZALIA_F0_CODEC_PIN_CONTROL_HOT_PLUG_CONTROL__AUDIO_ENABLED, 1);
    az_w(a, ep, X_AZALIA_F0_CODEC_PIN_CONTROL_HOT_PLUG_CONTROL, v);
    if (!on) { SETF(v, XF_AZALIA_F0_CODEC_PIN_CONTROL_HOT_PLUG_CONTROL__AUDIO_ENABLED, 0); az_w(a, ep, X_AZALIA_F0_CODEC_PIN_CONTROL_HOT_PLUG_CONTROL, v); }
    SETF(v, XF_AZALIA_F0_CODEC_PIN_CONTROL_HOT_PLUG_CONTROL__CLOCK_GATING_DISABLE, 0);
    az_w(a, ep, X_AZALIA_F0_CODEC_PIN_CONTROL_HOT_PLUG_CONTROL, v);
}

/* ------------------------------------------------------------ HDMI: N/CTS (tabelas do dcn10_stream_encoder.c) */
typedef struct { u32 pclk10k, n32, cts32, n44, cts44, n48, cts48; } acr_t;
static const acr_t acr24[16] = {
    {2517, 4576, 28125, 7007, 31250, 6864, 28125}, {2518, 4576, 28125, 7007, 31250, 6864, 28125},
    {2520, 4096, 25200, 6272, 28000, 6144, 25200}, {2700, 4096, 27000, 6272, 30000, 6144, 27000},
    {2702, 4096, 27027, 6272, 30030, 6144, 27027}, {2703, 4096, 27027, 6272, 30030, 6144, 27027},
    {5400, 4096, 54000, 6272, 60000, 6144, 54000}, {5405, 4096, 54054, 6272, 60060, 6144, 54054},
    {7417, 11648, 210937, 17836, 234375, 11648, 140625}, {7425, 4096, 74250, 6272, 82500, 6144, 74250},
    {14835, 11648, 421875, 8918, 234375, 5824, 140625}, {14850, 4096, 148500, 6272, 165000, 6144, 148500},
    {29670, 5824, 421875, 4459, 234375, 5824, 281250}, {29700, 3072, 222750, 4704, 247500, 5120, 247500},
    {59340, 5824, 843750, 8918, 937500, 5824, 562500}, {59400, 3072, 445500, 9408, 990000, 6144, 594000} };
static const acr_t acr36[14] = {
    {2517, 9152, 84375, 7007, 48875, 9152, 56250}, {2518, 9152, 84375, 7007, 48875, 9152, 56250},
    {2520, 4096, 37800, 6272, 42000, 6144, 37800}, {2700, 4096, 40500, 6272, 45000, 6144, 40500},
    {2702, 8192, 81081, 6272, 45045, 8192, 54054}, {2703, 8192, 81081, 6272, 45045, 8192, 54054},
    {5400, 4096, 81000, 6272, 90000, 6144, 81000}, {5405, 4096, 81081, 6272, 90090, 6144, 81081},
    {7417, 11648, 316406, 17836, 351562, 11648, 210937}, {7425, 4096, 111375, 6272, 123750, 6144, 111375},
    {14835, 11648, 632812, 17836, 703125, 11648, 421875}, {14850, 4096, 222750, 6272, 247500, 6144, 222750},
    {29670, 5824, 632812, 8918, 703125, 5824, 421875}, {29700, 4096, 445500, 4704, 371250, 5120, 371250} };

/* get_audio_clock_info: valores da tabela para os relógios da norma; senão N padrão e CTS = relógio em kHz.
 * (Como no Linux, 10 bpc usa a tabela de 8 bpc e 16 bpc não tem tabela aqui.) */
void amd_audio_acr(u32 pix_khz, u32 bpc, u32 out[6]) {
    const acr_t *t = bpc == 12 ? acr36 : acr24;
    u32 n = bpc == 12 ? 14 : 16, p10 = pix_khz / 10;
    for (u32 i = 0; i < n; i++) {
        if (t[i].pclk10k > p10) break;
        if (t[i].pclk10k == p10) {
            out[0] = t[i].n32; out[1] = t[i].cts32; out[2] = t[i].n44; out[3] = t[i].cts44; out[4] = t[i].n48; out[5] = t[i].cts48;
            return;
        }
    }
    out[0] = 4096; out[2] = 6272; out[4] = 6144;
    out[1] = out[3] = out[5] = pix_khz;
}

/* speakers_to_channels: alocação de alto-falantes -> canais CEA do AFMT */
static u32 cea_channels(u8 spk) {
    u32 fl_fr = spk & 1, lfe = (spk >> 1) & 1, fc = (spk >> 2) & 1, rl_rr = (spk >> 3) & 1, rc = (spk >> 4) & 1;
    u32 flc = (spk >> 5) & 1, rlc = (spk >> 6) & 1;
    u32 ch = fl_fr | fl_fr << 1 | lfe << 2 | fc << 3;
    if (rl_rr) ch |= rl_rr << 4 | rl_rr << 5 | rc << 6;
    else ch |= rc << 4;
    if (flc) ch |= flc << 6 | flc << 7;
    else ch |= rlc << 6 | rlc << 7;
    return ch;
}

static void se_audio_setup(amdgpu_t *a, u32 dig, u32 ep, u8 spk) {
    WR(a, D_DIG_AFMT_AUDIO_SRC_CONTROL(dig), FV(F_DIG_AFMT_AUDIO_SRC_CONTROL__AFMT_AUDIO_SRC_SELECT, ep));
    UPD(a, D_DIG_AFMT_AUDIO_PACKET_CONTROL2(dig), F_DIG_AFMT_AUDIO_PACKET_CONTROL2__AFMT_AUDIO_CHANNEL_ENABLE, cea_channels(spk));
}

static void hdmi_audio(amdgpu_t *a, u32 dig, const amd_disp_t *d) {
    UPD(a, D_DIG_AFMT_CNTL(dig), F_DIG_AFMT_CNTL__AFMT_AUDIO_CLOCK_EN, 1);
    UPD(a, D_DIG_HDMI_AUDIO_PACKET_CONTROL(dig), F_DIG_HDMI_AUDIO_PACKET_CONTROL__HDMI_AUDIO_DELAY_EN, 1);
    UPD(a, D_DIG_AFMT_AUDIO_PACKET_CONTROL(dig), F_DIG_AFMT_AUDIO_PACKET_CONTROL__AFMT_60958_CS_UPDATE, 1);
    upd(a, D_DIG_AFMT_AUDIO_PACKET_CONTROL2(dig), FV(F_DIG_AFMT_AUDIO_PACKET_CONTROL2__AFMT_AUDIO_LAYOUT_OVRD, ~0u) |
        FV(F_DIG_AFMT_AUDIO_PACKET_CONTROL2__AFMT_60958_OSF_OVRD, ~0u), 0);
    upd(a, D_DIG_HDMI_ACR_PACKET_CONTROL(dig), FV(F_DIG_HDMI_ACR_PACKET_CONTROL__HDMI_ACR_AUTO_SEND, ~0u) |
        FV(F_DIG_HDMI_ACR_PACKET_CONTROL__HDMI_ACR_SOURCE, ~0u) | FV(F_DIG_HDMI_ACR_PACKET_CONTROL__HDMI_ACR_AUDIO_PRIORITY, ~0u),
        FV(F_DIG_HDMI_ACR_PACKET_CONTROL__HDMI_ACR_AUTO_SEND, 1));
    u32 acr[6];
    amd_audio_acr(d->req.t.pix_clk_khz, d->req.bpc, acr);
    UPD(a, D_DIG_HDMI_ACR_32_0(dig), F_DIG_HDMI_ACR_32_0__HDMI_ACR_CTS_32, acr[1]);
    UPD(a, D_DIG_HDMI_ACR_32_1(dig), F_DIG_HDMI_ACR_32_1__HDMI_ACR_N_32, acr[0]);
    UPD(a, D_DIG_HDMI_ACR_44_0(dig), F_DIG_HDMI_ACR_44_0__HDMI_ACR_CTS_44, acr[3]);
    UPD(a, D_DIG_HDMI_ACR_44_1(dig), F_DIG_HDMI_ACR_44_1__HDMI_ACR_N_44, acr[2]);
    UPD(a, D_DIG_HDMI_ACR_48_0(dig), F_DIG_HDMI_ACR_48_0__HDMI_ACR_CTS_48, acr[5]);
    UPD(a, D_DIG_HDMI_ACR_48_1(dig), F_DIG_HDMI_ACR_48_1__HDMI_ACR_N_48, acr[4]);
    upd(a, D_DIG_AFMT_60958_0(dig), FV(F_DIG_AFMT_60958_0__AFMT_60958_CS_CHANNEL_NUMBER_L, ~0u) | FV(F_DIG_AFMT_60958_0__AFMT_60958_CS_CLOCK_ACCURACY, ~0u),
        FV(F_DIG_AFMT_60958_0__AFMT_60958_CS_CHANNEL_NUMBER_L, 1));
    UPD(a, D_DIG_AFMT_60958_1(dig), F_DIG_AFMT_60958_1__AFMT_60958_CS_CHANNEL_NUMBER_R, 2);
    upd(a, D_DIG_AFMT_60958_2(dig),
        FV(F_DIG_AFMT_60958_2__AFMT_60958_CS_CHANNEL_NUMBER_2, ~0u) | FV(F_DIG_AFMT_60958_2__AFMT_60958_CS_CHANNEL_NUMBER_3, ~0u) |
        FV(F_DIG_AFMT_60958_2__AFMT_60958_CS_CHANNEL_NUMBER_4, ~0u) | FV(F_DIG_AFMT_60958_2__AFMT_60958_CS_CHANNEL_NUMBER_5, ~0u) |
        FV(F_DIG_AFMT_60958_2__AFMT_60958_CS_CHANNEL_NUMBER_6, ~0u) | FV(F_DIG_AFMT_60958_2__AFMT_60958_CS_CHANNEL_NUMBER_7, ~0u),
        FV(F_DIG_AFMT_60958_2__AFMT_60958_CS_CHANNEL_NUMBER_2, 3) | FV(F_DIG_AFMT_60958_2__AFMT_60958_CS_CHANNEL_NUMBER_3, 4) |
        FV(F_DIG_AFMT_60958_2__AFMT_60958_CS_CHANNEL_NUMBER_4, 5) | FV(F_DIG_AFMT_60958_2__AFMT_60958_CS_CHANNEL_NUMBER_5, 6) |
        FV(F_DIG_AFMT_60958_2__AFMT_60958_CS_CHANNEL_NUMBER_6, 7) | FV(F_DIG_AFMT_60958_2__AFMT_60958_CS_CHANNEL_NUMBER_7, 8));
}

static void dp_audio(amdgpu_t *a, u32 dig) {
    UPD(a, D_DIG_AFMT_CNTL(dig), F_DIG_AFMT_CNTL__AFMT_AUDIO_CLOCK_EN, 1);
    WR(a, D_DP_DP_SEC_AUD_N(dig), FV(F_DP_DP_SEC_AUD_N__DP_SEC_AUD_N, 0x8000));          /* DP_SEC_AUD_N__DEFAULT */
    WR(a, D_DP_DP_SEC_TIMESTAMP(dig), FV(F_DP_DP_SEC_TIMESTAMP__DP_SEC_TIMESTAMP_MODE, 1)); /* AUTO_CALC */
    UPD(a, D_DIG_AFMT_AUDIO_PACKET_CONTROL(dig), F_DIG_AFMT_AUDIO_PACKET_CONTROL__AFMT_60958_CS_UPDATE, 1);
    upd(a, D_DIG_AFMT_AUDIO_PACKET_CONTROL2(dig), FV(F_DIG_AFMT_AUDIO_PACKET_CONTROL2__AFMT_AUDIO_LAYOUT_OVRD, ~0u) |
        FV(F_DIG_AFMT_AUDIO_PACKET_CONTROL2__AFMT_60958_OSF_OVRD, ~0u), 0);
    UPD(a, D_DIG_AFMT_INFOFRAME_CONTROL0(dig), F_DIG_AFMT_INFOFRAME_CONTROL0__AFMT_AUDIO_INFO_UPDATE, 1);
    UPD(a, D_DIG_AFMT_60958_0(dig), F_DIG_AFMT_60958_0__AFMT_60958_CS_CLOCK_ACCURACY, 0);
}

/* dce_aud_wall_dto_setup */
static void wall_dto(amdgpu_t *a, const amd_disp_t *d, int otg) {
    if (d->hdmi) {
        upd(a, D_DCCG_AUDIO_DTO_SOURCE, FV(F_DCCG_AUDIO_DTO_SOURCE__DCCG_AUDIO_DTO0_SOURCE_SEL, ~0u) | FV(F_DCCG_AUDIO_DTO_SOURCE__DCCG_AUDIO_DTO_SEL, ~0u),
            FV(F_DCCG_AUDIO_DTO_SOURCE__DCCG_AUDIO_DTO0_SOURCE_SEL, (u32)otg));
        UPD(a, D_DCCG_AUDIO_DTO0_MODULE, F_DCCG_AUDIO_DTO0_MODULE__DCCG_AUDIO_DTO0_MODULE, d->req.t.pix_clk_khz * 10);
        UPD(a, D_DCCG_AUDIO_DTO0_PHASE, F_DCCG_AUDIO_DTO0_PHASE__DCCG_AUDIO_DTO0_PHASE, 24 * 10000);
    } else {
        UPD(a, D_DCCG_AUDIO_DTO_SOURCE, F_DCCG_AUDIO_DTO_SOURCE__DCCG_AUDIO_DTO_SEL, 1);
        UPD(a, D_DCCG_AUDIO_DTO1_MODULE, F_DCCG_AUDIO_DTO1_MODULE__DCCG_AUDIO_DTO1_MODULE, (a->disp_dprefclk_khz ? a->disp_dprefclk_khz : 600000) * 10);
        UPD(a, D_DCCG_AUDIO_DTO1_PHASE, F_DCCG_AUDIO_DTO1_PHASE__DCCG_AUDIO_DTO1_PHASE, 24 * 10000);
    }
}

/* ------------------------------------------------------------ por tela */
/* endpoint da tela: o de mesmo índice do DIG se existir e estiver livre; senão o primeiro livre */
static int pick_endpoint(amdgpu_t *a, int otg, u32 dig) {
    u32 n = amd_audio_endpoints(a), used = 0;
    for (int o = 0; o < AMD_DCN_OTGS; o++) if (o != otg && a->disp[o].on && a->disp[o].audio_ep >= 0) used |= 1u << a->disp[o].audio_ep;
    if (dig < n && !(used >> dig & 1)) return (int)dig;
    for (u32 i = 0; i < n; i++) if (!(used >> i & 1)) return (int)i;
    return -1;
}

/* depois do vídeo ligado (dce110_enable_audio_stream): configura e liga o áudio se o monitor tem áudio */
void amdgpu_audio_on(amdgpu_t *a, int otg) {
    amd_disp_t *d = &a->disp[otg];
    d->audio_ep = -1;
    int ddc = a->vbios.conn[d->req.conn].ddc;
    if (ddc < 0 || ddc >= AMD_NUM_DDC || a->ddc_edid_len[ddc] < 128) return;
    static edid_info_t e;
    if (!edid_parse(a->ddc_edid[ddc], a->ddc_edid_len[ddc], &e) || !e.nsad) return;
    if (d->hdmi && !e.hdmi) return;                          /* DVI pelo conector HDMI: sem áudio */
    int ep = pick_endpoint(a, otg, d->phy);
    if (ep < 0) return;
    d->audio_ep = ep;
    az_configure(a, (u32)ep, d, &e, a->ddc_edid[ddc]);
    wall_dto(a, d, otg);
    if (d->hdmi) hdmi_audio(a, d->phy, d); else dp_audio(a, d->phy);
    se_audio_setup(a, d->phy, (u32)ep, e.speaker_alloc);
    az_enable(a, (u32)ep, true);
    if (d->dp) {                                             /* enc1_se_enable_dp_audio */
        UPD(a, D_DP_DP_SEC_CNTL(d->phy), F_DP_DP_SEC_CNTL__DP_SEC_ASP_ENABLE, 1);
        upd(a, D_DP_DP_SEC_CNTL(d->phy), FV(F_DP_DP_SEC_CNTL__DP_SEC_ATP_ENABLE, ~0u) | FV(F_DP_DP_SEC_CNTL__DP_SEC_AIP_ENABLE, ~0u),
            FV(F_DP_DP_SEC_CNTL__DP_SEC_ATP_ENABLE, 1) | FV(F_DP_DP_SEC_CNTL__DP_SEC_AIP_ENABLE, 1));
        UPD(a, D_DP_DP_SEC_CNTL(d->phy), F_DP_DP_SEC_CNTL__DP_SEC_STREAM_ENABLE, 1);
    }
    UPD(a, D_DIG_AFMT_AUDIO_PACKET_CONTROL(d->phy), F_DIG_AFMT_AUDIO_PACKET_CONTROL__AFMT_AUDIO_SAMPLE_SEND, 1);   /* sem mudo */
}

/* antes de desligar o vídeo (dce110_disable_audio_stream) */
void amdgpu_audio_off(amdgpu_t *a, int otg) {
    amd_disp_t *d = &a->disp[otg];
    if (d->audio_ep < 0) return;
    UPD(a, D_DIG_AFMT_AUDIO_PACKET_CONTROL(d->phy), F_DIG_AFMT_AUDIO_PACKET_CONTROL__AFMT_AUDIO_SAMPLE_SEND, 0);
    if (d->dp) {
        upd(a, D_DP_DP_SEC_CNTL(d->phy), FV(F_DP_DP_SEC_CNTL__DP_SEC_ASP_ENABLE, ~0u) | FV(F_DP_DP_SEC_CNTL__DP_SEC_ATP_ENABLE, ~0u) |
            FV(F_DP_DP_SEC_CNTL__DP_SEC_AIP_ENABLE, ~0u) | FV(F_DP_DP_SEC_CNTL__DP_SEC_ACM_ENABLE, ~0u) | FV(F_DP_DP_SEC_CNTL__DP_SEC_STREAM_ENABLE, ~0u), 0);
        if (RR(a, D_DP_DP_SEC_CNTL(d->phy))) UPD(a, D_DP_DP_SEC_CNTL(d->phy), F_DP_DP_SEC_CNTL__DP_SEC_STREAM_ENABLE, 1);
    }
    UPD(a, D_DIG_AFMT_CNTL(d->phy), F_DIG_AFMT_CNTL__AFMT_AUDIO_CLOCK_EN, 0);
    az_enable(a, (u32)d->audio_ep, false);
    d->audio_ep = -1;
}
