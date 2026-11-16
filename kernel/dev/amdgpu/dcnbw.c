/* mOS — validação de banda do DCN 1.0: o equivalente de dcn_validate_bandwidth + calc_wm_sets_and_perf_params
 * (dml/calcs/dcn_calcs.c do display/dc da AMD, MIT) para as estruturas do mOS. Os limites do SoC são os do
 * dcn10_soc_defaults/dcn10_ip_defaults com as correções do dcn10_resource_construct_fp e as tabelas de relógio do
 * SMU (dcn_bw_update_from_pplib_*). Política igual à do Linux no Raven: divisão de pipe dinâmica, divisão forçada
 * com uma tela só (force_single_disp_pipe_split), dispclk mínimo de 100 MHz, LB de 36 bpp, fclk alinhado à tensão.
 * O modelo em si (dcnbw_auto.c) não foi mexido. Ver dcnbw.h. */
#include "dcnbw.h"
#include "dcnbw_vars.h"

void amd_dcnbw_scaler_settings(struct dcn_bw_internal_vars *v);
void amd_dcnbw_mode_support(struct dcn_bw_internal_vars *v);
void amd_dcnbw_pipe_configuration(struct dcn_bw_internal_vars *v);
void amd_dcnbw_clocks_prefetch_watermarks(struct dcn_bw_internal_vars *v);

/* dcn10_soc_defaults (latências em us, relógios em MHz, banda em GB/s) */
typedef struct {
    float sr_exit_time, sr_enter_plus_exit_time, urgent_latency, write_back_latency, dram_clock_change_latency;
    float percent_of_ideal_drambw_received_after_urg_latency;
    float dcfclkv_max0p9, dcfclkv_nom0p8, dcfclkv_mid0p72, dcfclkv_min0p65;
    float max_dispclk_vmax0p9, max_dispclk_vnom0p8, max_dispclk_vmid0p72, max_dispclk_vmin0p65;
    float max_dppclk_vmax0p9, max_dppclk_vnom0p8, max_dppclk_vmid0p72, max_dppclk_vmin0p65;
    float phyclkv_max0p9, phyclkv_nom0p8, phyclkv_mid0p72, phyclkv_min0p65;
    float bw_vmax0p9, bw_vnom0p8, bw_vmid0p72, bw_vmin0p65;
    float number_of_channels, socclk, downspreading, round_trip_ping_latency_cycles;
    float urgent_out_of_order_return_per_channel, vmm_page_size, return_bus_width, percent_disp_bw_limit;
    float max_num_dpp;
} soc_t;

static void soc_from(const amd_dcnbw_soc_t *in, soc_t *s) {
    *s = (soc_t){
        .sr_exit_time = 17, .sr_enter_plus_exit_time = 19, .urgent_latency = 4, .write_back_latency = 12,
        .dram_clock_change_latency = 17, .percent_of_ideal_drambw_received_after_urg_latency = 80,
        .dcfclkv_max0p9 = 655, .dcfclkv_nom0p8 = 626, .dcfclkv_mid0p72 = 600, .dcfclkv_min0p65 = 300,
        .max_dispclk_vmax0p9 = 1108, .max_dispclk_vnom0p8 = 1029, .max_dispclk_vmid0p72 = 960, .max_dispclk_vmin0p65 = 626,
        .max_dppclk_vmax0p9 = 720, .max_dppclk_vnom0p8 = 686, .max_dppclk_vmid0p72 = 626, .max_dppclk_vmin0p65 = 400,
        .phyclkv_max0p9 = 900, .phyclkv_nom0p8 = 847, .phyclkv_mid0p72 = 800, .phyclkv_min0p65 = 600,
        .bw_vmax0p9 = 38.4f, .bw_vnom0p8 = 34.133f, .bw_vmid0p72 = 29.866f, .bw_vmin0p65 = 12.8f,
        .number_of_channels = 2, .socclk = 208, .downspreading = 0.5f, .round_trip_ping_latency_cycles = 128,
        .urgent_out_of_order_return_per_channel = 256, .vmm_page_size = 4096, .return_bus_width = 64,
        .percent_disp_bw_limit = 0.3f, .max_num_dpp = 4,
    };
    /* dcn10_resource_construct_fp */
    s->max_num_dpp = (float)(in->num_pipes ? in->num_pipes : 4);
    if (in->dcn101) { s->dram_clock_change_latency = 23; if (s->max_num_dpp > 3) s->max_num_dpp = 3; }
    if (in->rv1_f0) { s->urgent_latency = 3; s->bw_vmax0p9 = 41.60f; }
    s->number_of_channels = (float)(in->channels ? in->channels : 2);
    if (in->channels == 1) {
        s->bw_vmax0p9 = in->rv1_f0 ? 20.80f : 19.2f; s->bw_vnom0p8 = 17.066f; s->bw_vmid0p72 = 14.933f; s->bw_vmin0p65 = 12.8f;
    }
    /* dcn_bw_update_from_pplib_fclks / _dcfclks */
    if (in->nfclk) {
        u32 n = in->nfclk;
        u32 imid = n > 2 ? n - 3 : 0, inom = n > 1 ? n - 2 : 0, imax = n - 1;
        s->bw_vmin0p65 = 32 * (float)in->fclk_mhz[0] / 1000.0f;
        s->bw_vmid0p72 = s->number_of_channels * (float)in->fclk_mhz[imid] * ddr4_dram_factor_single_Channel / 1000.0f;
        s->bw_vnom0p8 = s->number_of_channels * (float)in->fclk_mhz[inom] * ddr4_dram_factor_single_Channel / 1000.0f;
        s->bw_vmax0p9 = s->number_of_channels * (float)in->fclk_mhz[imax] * ddr4_dram_factor_single_Channel / 1000.0f;
    }
    if (in->ndcfclk >= 3) {
        u32 n = in->ndcfclk;
        s->dcfclkv_min0p65 = (float)in->dcfclk_mhz[0];
        s->dcfclkv_mid0p72 = (float)in->dcfclk_mhz[n - 3];
        s->dcfclkv_nom0p8 = (float)in->dcfclk_mhz[n - 2];
        s->dcfclkv_max0p9 = (float)in->dcfclk_mhz[n - 1];
    }
}

void amd_dcnbw_soc_defaults(amd_dcnbw_soc_t *soc) {
    memset(soc, 0, sizeof *soc);
    soc->channels = 2;
    soc->num_pipes = 4;
    soc->refclk_khz = 48000;
}

u32 amd_dcnbw_wm_cycles(u32 ns, u32 refclk_khz) {
    u64 v = (u64)ns * refclk_khz / 1000000u;          /* convert_and_clamp: ns * refclk_mhz / 1000 */
    return v > 0x1FFFFF ? 0x1FFFFF : (u32)v;
}

static enum dcn_bw_defs sw_mode_def(u32 sw) {
    switch (sw) {
    case 0:  return dcn_bw_sw_linear;
    case 5:  return dcn_bw_sw_4_kb_s;
    case 6:  return dcn_bw_sw_4_kb_d;
    case 9:  return dcn_bw_sw_64_kb_s;
    case 10: return dcn_bw_sw_64_kb_d;
    case 13: return dcn_bw_sw_var_s;
    case 14: return dcn_bw_sw_var_d;
    case 17: return dcn_bw_sw_64_kb_s_t;
    case 18: return dcn_bw_sw_64_kb_d_t;
    case 21: return dcn_bw_sw_4_kb_s_x;
    case 22: return dcn_bw_sw_4_kb_d_x;
    case 25: return dcn_bw_sw_64_kb_s_x;
    case 26: return dcn_bw_sw_64_kb_d_x;
    case 29: return dcn_bw_sw_var_s_x;
    case 30: return dcn_bw_sw_var_d_x;
    default: return dcn_bw_sw_4_kb_s;     /* modos _R e 256B ficam fora da fórmula, como no Linux */
    }
}

static enum dcn_bw_defs bpp_def(u32 bpp) {
    return bpp == 8 ? dcn_bw_rgb_sub_64 : bpp == 2 ? dcn_bw_rgb_sub_16 : dcn_bw_rgb_sub_32;
}

/* o modelo tem ~3,5 KiB de float por plano e estado; fica fora da pilha do kernel */
static struct dcn_bw_internal_vars g_vars;

static void wm_from(const struct dcn_bw_internal_vars *v, amd_dcnbw_wm_t *w) {
    w->sr_exit_ns = (u32)(v->stutter_exit_watermark * 1000.0f);
    w->sr_enter_plus_exit_ns = (u32)(v->stutter_enter_plus_exit_watermark * 1000.0f);
    w->pstate_change_ns = (u32)(v->dram_clock_change_watermark * 1000.0f);
    w->pte_meta_urgent_ns = (u32)(v->ptemeta_urgent_watermark * 1000.0f);
    w->urgent_ns = (u32)(v->urgent_watermark * 1000.0f);
}

int amd_dcnbw_validate(const amd_dcnbw_soc_t *soc_in, const amd_dcnbw_stream_t *st, u32 n, amd_dcnbw_out_t *out) {
    memset(out, 0, sizeof *out);
    out->voltage_level = number_of_states_plus_one;
    if (!soc_in || !st || !n || n > AMD_DCNBW_MAX_STREAMS) return -1;
    soc_t s;
    soc_from(soc_in, &s);
    struct dcn_bw_internal_vars *v = &g_vars;
    memset(v, 0, sizeof *v);

    v->sr_exit_time = s.sr_exit_time;
    v->sr_enter_plus_exit_time = s.sr_enter_plus_exit_time;
    v->urgent_latency = s.urgent_latency;
    v->write_back_latency = s.write_back_latency;
    v->percent_of_ideal_drambw_received_after_urg_latency = s.percent_of_ideal_drambw_received_after_urg_latency;
    v->dcfclkv_min0p65 = s.dcfclkv_min0p65; v->dcfclkv_mid0p72 = s.dcfclkv_mid0p72;
    v->dcfclkv_nom0p8 = s.dcfclkv_nom0p8; v->dcfclkv_max0p9 = s.dcfclkv_max0p9;
    v->max_dispclk_vmin0p65 = s.max_dispclk_vmin0p65; v->max_dispclk_vmid0p72 = s.max_dispclk_vmid0p72;
    v->max_dispclk_vnom0p8 = s.max_dispclk_vnom0p8; v->max_dispclk_vmax0p9 = s.max_dispclk_vmax0p9;
    v->max_dppclk_vmin0p65 = s.max_dppclk_vmin0p65; v->max_dppclk_vmid0p72 = s.max_dppclk_vmid0p72;
    v->max_dppclk_vnom0p8 = s.max_dppclk_vnom0p8; v->max_dppclk_vmax0p9 = s.max_dppclk_vmax0p9;
    v->socclk = s.socclk;
    v->fabric_and_dram_bandwidth_vmin0p65 = s.bw_vmin0p65; v->fabric_and_dram_bandwidth_vmid0p72 = s.bw_vmid0p72;
    v->fabric_and_dram_bandwidth_vnom0p8 = s.bw_vnom0p8; v->fabric_and_dram_bandwidth_vmax0p9 = s.bw_vmax0p9;
    v->phyclkv_min0p65 = s.phyclkv_min0p65; v->phyclkv_mid0p72 = s.phyclkv_mid0p72;
    v->phyclkv_nom0p8 = s.phyclkv_nom0p8; v->phyclkv_max0p9 = s.phyclkv_max0p9;
    v->downspreading = s.downspreading;
    v->round_trip_ping_latency_cycles = s.round_trip_ping_latency_cycles;
    v->urgent_out_of_order_return_per_channel = s.urgent_out_of_order_return_per_channel;
    v->number_of_channels = s.number_of_channels;
    v->vmm_page_size = s.vmm_page_size;
    v->dram_clock_change_latency = s.dram_clock_change_latency;
    v->return_bus_width = s.return_bus_width;
    /* dcn10_ip_defaults */
    v->rob_buffer_size_in_kbyte = 64;
    v->det_buffer_size_in_kbyte = 164;
    v->dpp_output_buffer_pixels = 2560;
    v->opp_output_buffer_lines = 1;
    v->pixel_chunk_size_in_kbyte = 8;
    v->pte_enable = dcn_bw_yes;
    v->pte_chunk_size = 2;
    v->meta_chunk_size = 2;
    v->writeback_chunk_size = 2;
    v->odm_capability = dcn_bw_no;
    v->dsc_capability = dcn_bw_no;
    v->line_buffer_size = 589824;
    v->is_line_buffer_bpp_fixed = dcn_bw_no;
    v->line_buffer_fixed_bpp = dcn_bw_na;
    v->max_line_buffer_lines = 12;
    v->writeback_luma_buffer_size = 12;
    v->writeback_chroma_buffer_size = 8;
    v->max_num_dpp = s.max_num_dpp;
    v->max_num_writeback = 2;
    v->max_dchub_topscl_throughput = 4;
    v->max_pscl_tolb_throughput = 2;
    v->max_lb_tovscl_throughput = 4;
    v->max_vscl_tohscl_throughput = 4;
    v->max_hscl_ratio = 4;
    v->max_vscl_ratio = 4;
    v->max_hscl_taps = 8;
    v->max_vscl_taps = 8;
    v->under_scan_factor = 1.11f;
    v->pte_buffer_size_in_requests = 42;
    v->dispclk_ramping_margin = 1;
    v->max_inter_dcn_tile_repeaters = 8;
    v->can_vstartup_lines_exceed_vsync_plus_back_porch_lines_minus_one = dcn_bw_no;
    v->bug_forcing_luma_and_chroma_request_to_same_size_fixed = dcn_bw_no;

    v->voltage[5] = dcn_bw_no_support;
    v->voltage[4] = dcn_bw_v_max0p9;
    v->voltage[3] = dcn_bw_v_max0p9;
    v->voltage[2] = dcn_bw_v_nom0p8;
    v->voltage[1] = dcn_bw_v_mid0p72;
    v->voltage[0] = dcn_bw_v_min0p65;
    float bw[6] = { s.bw_vmin0p65, s.bw_vmid0p72, s.bw_vnom0p8, s.bw_vmax0p9, s.bw_vmax0p9, s.bw_vmax0p9 };
    float dcf[6] = { s.dcfclkv_min0p65, s.dcfclkv_mid0p72, s.dcfclkv_nom0p8, s.dcfclkv_max0p9, s.dcfclkv_max0p9, s.dcfclkv_max0p9 };
    float dsp[6] = { s.max_dispclk_vmin0p65, s.max_dispclk_vmid0p72, s.max_dispclk_vnom0p8, s.max_dispclk_vmax0p9, s.max_dispclk_vmax0p9, s.max_dispclk_vmax0p9 };
    float dpp[6] = { s.max_dppclk_vmin0p65, s.max_dppclk_vmid0p72, s.max_dppclk_vnom0p8, s.max_dppclk_vmax0p9, s.max_dppclk_vmax0p9, s.max_dppclk_vmax0p9 };
    float phy[6] = { s.phyclkv_min0p65, s.phyclkv_mid0p72, s.phyclkv_nom0p8, s.phyclkv_max0p9, s.phyclkv_max0p9, s.phyclkv_max0p9 };
    for (int i = 0; i < 6; i++) {
        v->fabric_and_dram_bandwidth_per_state[i] = bw[i];
        v->dcfclk_per_state[i] = dcf[i];
        v->max_dispclk[i] = dsp[i];
        v->max_dppclk[i] = dpp[i];
        v->phyclk_per_state[i] = phy[i];
    }
    v->synchronized_vblank = dcn_bw_no;
    v->ta_pscalculation = dcn_bw_override;
    v->allow_different_hratio_vratio = dcn_bw_yes;

    bool small_plane = false;
    for (u32 k = 0; k < n; k++) {
        const amd_dcnbw_stream_t *t = &st[k];
        if (!t->h_total || !t->v_total || !t->h_active || !t->v_active || t->pixel_clock_mhz <= 0) return -1;
        v->underscan_output[k] = false;
        v->interlace_output[k] = false;
        v->htotal[k] = (float)t->h_total;
        v->vtotal[k] = (float)t->v_total;
        v->vactive[k] = (float)t->v_active;
        v->v_sync_plus_back_porch[k] = (float)(t->v_total - t->v_active - t->v_front_porch);
        v->pixel_clock[k] = (float)t->pixel_clock_mhz;
        if (!t->has_plane) {
            v->dcc_enable[k] = dcn_bw_yes;
            v->source_pixel_format[k] = dcn_bw_rgb_sub_32;
            v->source_surface_mode[k] = dcn_bw_sw_4_kb_s;
            v->lb_bit_per_pixel[k] = 30;
            v->viewport_width[k] = (float)(t->h_active > 1920 ? 1920 : t->h_active);
            v->viewport_height[k] = (float)(t->v_active > 1080 ? 1080 : t->v_active);
            v->scaler_rec_out_width[k] = v->viewport_width[k];
            v->scaler_recout_height[k] = v->viewport_height[k];
            v->override_hta_ps[k] = 1.0f;
            v->override_vta_ps[k] = 1.0f;
            v->override_hta_pschroma[k] = 1.0f;
            v->override_vta_pschroma[k] = 1.0f;
        } else {
            /* sem escala: o plano ocupa a tela; viewport = recout */
            v->viewport_width[k] = (float)t->src_width;
            v->viewport_height[k] = (float)t->src_height;
            v->scaler_rec_out_width[k] = (float)t->h_active;
            v->scaler_recout_height[k] = (float)t->v_active;
            /* dcc_support_pixel_format do HUBBUB 1.0: 16/32/64 bpp RGB; a validação ignora se o plano liga ou não */
            v->dcc_enable[k] = dcn_bw_yes;
            v->source_pixel_format[k] = bpp_def(t->bytes_per_pixel);
            v->source_surface_mode[k] = sw_mode_def(t->sw_mode);
            v->lb_bit_per_pixel[k] = 36;                              /* DCN: LB_PIXEL_DEPTH_36BPP */
            /* dpp1_get_optimal_number_of_taps: 4 com escala no eixo, 1 sem */
            v->override_hta_ps[k] = t->src_width != t->h_active ? 4.0f : 1.0f;
            v->override_vta_ps[k] = t->src_height != t->v_active ? 4.0f : 1.0f;
            v->override_hta_pschroma[k] = 2.0f;                       /* 1 vira 2, como no dcn_calcs */
            v->override_vta_pschroma[k] = 2.0f;
            if (t->src_width <= 16 || t->src_height <= 16 || t->h_active <= 16 || t->v_active <= 16) small_plane = true;
        }
        v->source_scan[k] = dcn_bw_hor;
        v->dcc_rate[k] = 1;
        v->output_format[k] = t->ycbcr420 ? dcn_bw_420 : dcn_bw_444;
        v->output[k] = t->hdmi ? dcn_bw_hdmi : dcn_bw_dp;
        v->output_deep_color[k] = dcn_bw_encoder_8bpc;
        if (t->hdmi) {
            if (t->bpc == 10) v->output_deep_color[k] = dcn_bw_encoder_10bpc;
            else if (t->bpc == 12) v->output_deep_color[k] = dcn_bw_encoder_12bpc;
            else if (t->bpc == 16) v->output_deep_color[k] = dcn_bw_encoder_16bpc;
        }
    }
    v->number_of_active_planes = (int)n;

    amd_dcnbw_scaler_settings(v);
    /* hack_bounding_box: planos minúsculos não dividem; MPC_SPLIT_DYNAMIC; divisão forçada com uma tela */
    if (small_plane) v->max_dispclk[0] = v->max_dppclk_vmin0p65;
    else if (n == 1 && st[0].pixel_clock_mhz < v->max_dppclk[0]) v->max_dppclk[0] = (float)st[0].pixel_clock_mhz;
    amd_dcnbw_mode_support(v);
    if (v->voltage_level != 0 && n == 1) {
        v->max_dppclk[0] = v->max_dppclk_vmin0p65;
        amd_dcnbw_mode_support(v);
    }
    amd_dcnbw_pipe_configuration(v);
    for (u32 k = 0; k < n; k++) {
        v->swath_width_y[k] = v->viewport_width[k] / v->dpp_per_plane[k];
        if (v->source_pixel_format[k] == dcn_bw_rgb_sub_64) { v->byte_per_pixel_dety[k] = 8.0f; v->byte_per_pixel_detc[k] = 0.0f; }
        else if (v->source_pixel_format[k] == dcn_bw_rgb_sub_16) { v->byte_per_pixel_dety[k] = 2.0f; v->byte_per_pixel_detc[k] = 0.0f; }
        else { v->byte_per_pixel_dety[k] = 4.0f; v->byte_per_pixel_detc[k] = 0.0f; }
    }
    v->total_data_read_bandwidth = 0.0f;
    for (u32 k = 0; k < n; k++) {
        v->read_bandwidth_plane_luma[k] = v->swath_width_y[k] * v->dpp_per_plane[k] *
            (float)(int)(v->byte_per_pixel_dety[k] + 0.99999f) / (v->htotal[k] / v->pixel_clock[k]) * v->v_ratio[k];
        v->read_bandwidth_plane_chroma[k] = 0.0f;
        v->total_data_read_bandwidth += v->read_bandwidth_plane_luma[k] + v->read_bandwidth_plane_chroma[k];
    }
    out->voltage_level = v->voltage_level;
    out->total_read_bw_mbps = v->total_data_read_bandwidth;
    if (v->voltage_level == number_of_states_plus_one) return -1;

    /* DC_VALIDATE_MODE_AND_PROGRAMMING */
    float bw_consumed = v->total_bandwidth_consumed_gbyte_per_second;
    if (bw_consumed < v->fabric_and_dram_bandwidth_vmin0p65) bw_consumed = v->fabric_and_dram_bandwidth_vmin0p65;
    else if (bw_consumed < v->fabric_and_dram_bandwidth_vmid0p72) bw_consumed = v->fabric_and_dram_bandwidth_vmid0p72;
    else if (bw_consumed < v->fabric_and_dram_bandwidth_vnom0p8) bw_consumed = v->fabric_and_dram_bandwidth_vnom0p8;
    else bw_consumed = v->fabric_and_dram_bandwidth_vmax0p9;
    if (bw_consumed < v->fabric_and_dram_bandwidth) bw_consumed = v->fabric_and_dram_bandwidth;   /* voltage_align_fclk */
    amd_dcnbw_pipe_configuration(v);
    amd_dcnbw_clocks_prefetch_watermarks(v);
    for (int i = 0; i < 4; i++) wm_from(v, &out->wm[i]);
    out->fclk_khz = (u32)(int)(bw_consumed * 1000000 / (ddr4_dram_factor_single_Channel * v->number_of_channels));
    if (bw_consumed == v->fabric_and_dram_bandwidth_vmin0p65) out->fclk_khz = (u32)(int)(bw_consumed * 1000000 / 32);
    out->dcfclk_deep_sleep_khz = (u32)(int)(v->dcf_clk_deep_sleep * 1000);
    out->dcfclk_khz = (u32)(int)(v->dcfclk * 1000);
    out->dispclk_khz = (u32)(int)(v->dispclk * 1000);
    if (out->dispclk_khz < 100000) out->dispclk_khz = 100000;                                  /* min_disp_clk_khz */
    out->dppclk_khz = out->dispclk_khz / (u32)v->dispclk_dppclk_ratio;
    out->phyclk_khz = (u32)(int)v->phyclk_per_state[v->voltage_level];
    switch (v->voltage_level) {
    case 0:  out->max_supported_dppclk_khz = (u32)(int)(s.max_dppclk_vmin0p65 * 1000); break;
    case 1:  out->max_supported_dppclk_khz = (u32)(int)(s.max_dppclk_vmid0p72 * 1000); break;
    case 2:  out->max_supported_dppclk_khz = (u32)(int)(s.max_dppclk_vnom0p8 * 1000); break;
    default: out->max_supported_dppclk_khz = (u32)(int)(s.max_dppclk_vmax0p9 * 1000); break;
    }
    out->dispclk_mhz = v->dispclk;
    out->dppclk_mhz = v->dppclk;
    out->dcfclk_mhz = v->dcfclk;
    out->dcfclk_deep_sleep_mhz = v->dcf_clk_deep_sleep;
    out->urgent_extra_latency_us = v->urgent_extra_latency;
    out->urgent_wm_us = v->urgent_watermark;
    out->sr_exit_wm_us = v->stutter_exit_watermark;
    out->sr_enter_plus_exit_wm_us = v->stutter_enter_plus_exit_watermark;
    out->dram_change_wm_us = v->dram_clock_change_watermark;
    out->pte_meta_urgent_wm_us = v->ptemeta_urgent_watermark;

    for (u32 k = 0; k < n; k++) {
        const amd_dcnbw_stream_t *t = &st[k];
        amd_dcnbw_pipe_t *p = &out->s[k];
        p->dpp = (u32)v->dpp_per_plane[k];
        p->vupdate_width = (u32)v->v_update_width_pix[k];
        p->vupdate_offset = (u32)v->v_update_offset_pix[k];
        p->vready_offset = (u32)v->v_ready_offset_pix[k];
        p->vstartup_start = (u32)v->v_startup[k];
        /* pipe_dlg_param.vblank_start/end: o dcn_validate_bandwidth multiplica por INTERLACE, então ficam 0 no
         * progressivo (só a hubp1_vready_workaround lê; o DLG calcula o próprio vblank pelo timing) */
        p->vblank_end = 0;
        p->vblank_start = 0;
        /* divisão do viewport e do recout (resource_build_scaling_params: a metade esquerda leva o pixel ímpar) */
        u32 w = t->has_plane ? t->src_width : t->h_active, rw = t->h_active;
        u32 vw[2], rcw[2];
        if (p->dpp == 2) { vw[0] = (w + 1) / 2; vw[1] = w / 2; rcw[0] = (rw + 1) / 2; rcw[1] = rw / 2; }
        else { vw[0] = w; vw[1] = 0; rcw[0] = rw; rcw[1] = 0; }
        p->viewport_width[0] = vw[0]; p->viewport_x[0] = 0;
        p->viewport_width[1] = vw[1]; p->viewport_x[1] = vw[0];
        /* dcn_bw_calc_rq_dlg_ttu: sys params iguais para todos os pipes */
        for (u32 i = 0; i < p->dpp; i++) {
            amd_dml1_dlg_in_t *d = &p->dlg[i];
            memset(d, 0, sizeof *d);
            d->htotal = t->h_total;
            u32 hblank_start = t->h_total - t->h_front_porch;
            d->hblank_end = hblank_start - t->h_active;
            u32 vblank_start = t->v_total - t->v_front_porch;
            d->vblank_start = vblank_start;
            d->vblank_end = vblank_start - t->v_active;
            d->interlaced = false;
            d->pixel_rate_mhz = t->pixel_clock_mhz;
            d->refclk_mhz = (soc_in->refclk_khz ? soc_in->refclk_khz : 48000) / 1000.0;
            d->dppclk_mhz = v->dppclk;
            d->dispclk_mhz = v->dispclk;
            d->vstartup_start = p->vstartup_start;
            d->vupdate_offset = p->vupdate_offset;
            d->vupdate_width = p->vupdate_width;
            d->vready_offset = p->vready_offset;
            d->recout_width = rcw[i];
            d->full_recout_width = rcw[i];
            d->hsplit = p->dpp == 2;
            d->t_urg_wm_us = v->urgent_watermark;
            d->t_sr_wm_us = v->stutter_enter_plus_exit_watermark;
            d->t_mclk_wm_us = v->dram_clock_change_watermark;
            d->t_extra_us = v->urgent_extra_latency;
            d->deepsleep_dcfclk_mhz = v->dcf_clk_deep_sleep;
            d->vm_en = v->pte_enable == dcn_bw_yes;
        }
    }
    float bw_limit = s.percent_disp_bw_limit * v->fabric_and_dram_bandwidth_vmax0p9;
    bool bw_limit_pass = (v->total_data_read_bandwidth / 1000.0f) < bw_limit;
    out->ok = bw_limit_pass && v->voltage_level <= 4;
    return out->ok ? 0 : -1;
}
