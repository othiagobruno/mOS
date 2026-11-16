/* mOS — parâmetros de busca do HUBP do DCN 1.0 (o DML1 da AMD: dml1_display_rq_dlg_calc.c, MIT), para a troca
 * de modo. Esta parte: o requestor (RQ) — tamanho de swath, chunks e grupos de PTE de uma superfície — para os
 * formatos que o mOS mostra (RGB 4:4:4 de 2, 4 ou 8 bytes por pixel, linear ou em tiles de 4/64/256 KiB, varredura
 * horizontal). Lógica pura; tools/amdgpu_dml1_host.c confere contra os registradores lidos no Picasso do Thiago.
 *
 * Parâmetros do IP do DCN 1.0 (dcn10_fpu.c): buffer de detile de 164 KiB, 42 pedidos de PTE no buffer, página de
 * VM de 4 KiB. */
#include "dml1.h"

#define DET_BUFFER_BYTES   (164u * 1024u)
#define DPTE_BUF_PTE_REQS  42u
#define LOG2_VMPG_BYTES    12u

static u32 ilog2(u32 v) { u32 r = 0; while (v >>= 1) r++; return r; }
static u32 round_up(u32 v, u32 m) { return (v + m - 1) / m * m; }

static void blk256(u32 bpe, bool linear, u32 *w, u32 *h) {
    if (linear) { *w = 256 / bpe; *h = 1; return; }
    if (bpe == 1) { *w = 16; *h = 16; }
    else if (bpe == 2) { *w = 16; *h = 8; }
    else if (bpe == 4) { *w = 8; *h = 8; }
    else { *w = 8; *h = 4; }
}

int amd_dml1_rq(const amd_dml1_surface_t *s, amd_dml1_rq_t *rq) {
    memset(rq, 0, sizeof *rq);
    u32 bpe = s->bytes_per_pixel;
    if ((bpe != 2 && bpe != 4 && bpe != 8) || !s->viewport_width || !s->pitch_px) return -1;
    bool linear = s->tile_bytes == 0;
    if (!linear && s->tile_bytes != 4096 && s->tile_bytes != 65536 && s->tile_bytes != 262144) return -1;
    u32 bw, bh;
    blk256(bpe, linear, &bw, &bh);
    u32 log2_bpe = ilog2(bpe), log2_bw = ilog2(bw), log2_bh = ilog2(bh);
    u32 blk_bytes = linear ? 256 : s->tile_bytes, log2_blk_bytes = ilog2(blk_bytes);
    u32 log2_blk_h = linear ? 0 : log2_bh + (log2_blk_bytes - 8 + 1) / 2;
    (void)log2_bw;

    /* swath: largura arredondada em blocos de 256 bytes, mais um bloco */
    rq->swath_width_ub = round_up(s->viewport_width - 1, bw) + bw;
    rq->req_per_swath_ub = rq->swath_width_ub / bw;
    u32 full_swath_bytes = rq->swath_width_ub * bh * bpe;

    /* detile: dois swaths precisam caber; senão pedidos de 128 bytes e meio swath */
    bool req128 = 2 * full_swath_bytes > DET_BUFFER_BYTES;
    rq->stored_swath_bytes = 2 * full_swath_bytes;
    u32 log2_swath_h = 0;
    if (!linear) { log2_swath_h = log2_bh; if (req128 && log2_swath_h) log2_swath_h--; }
    rq->swath_height = 1u << log2_swath_h;
    rq->req128 = req128;

    /* PTEs: altura da linha de PTE (para superfície linear vem do pitch) */
    u32 log2_vmpg_h = linear ? 0 : (LOG2_VMPG_BYTES - 8) / 2 + log2_bh;
    u32 log2_vmpg_w = LOG2_VMPG_BYTES - log2_bpe - log2_vmpg_h;
    u32 req_h_ptes;
    if (log2_blk_bytes <= LOG2_VMPG_BYTES) req_h_ptes = 0;
    else if (log2_blk_h - log2_vmpg_h >= 2) req_h_ptes = 2;
    else req_h_ptes = log2_blk_h - log2_vmpg_h;
    u32 req_w_ptes = 3 - req_h_ptes;
    u32 log2_dpte_req_h = log2_vmpg_h + req_h_ptes, log2_dpte_req_w = log2_vmpg_w + req_w_ptes;
    u32 dpte_req_w = 1u << log2_dpte_req_w;
    u32 log2_dpte_row_h, dpte_row_width_ub;
    if (linear) {
        u32 q = DPTE_BUF_PTE_REQS * dpte_req_w / s->pitch_px;
        log2_dpte_row_h = q ? ilog2(q) : 0;
        if (log2_dpte_row_h < 3) return -2;                       /* pitch grande demais para o buffer de PTE */
        if (log2_dpte_row_h > 7) log2_dpte_row_h = 7;
        dpte_row_width_ub = round_up(s->pitch_px * (1u << log2_dpte_row_h) - 1, dpte_req_w) + dpte_req_w;
    } else {
        log2_dpte_row_h = log2_dpte_req_h;
        dpte_row_width_ub = round_up(s->viewport_width - 1, dpte_req_w) + dpte_req_w;
    }
    if (s->viewport_width > 2560 + 16 && bpe >= 4 && log2_blk_bytes >= 16 && log2_dpte_row_h) log2_dpte_row_h--;
    rq->dpte_row_height = 1u << log2_dpte_row_h;
    rq->dpte_req_per_row_ub = dpte_row_width_ub / dpte_req_w;
    rq->dpte_bytes_per_row_ub = rq->dpte_req_per_row_ub * 64;
    u32 group_w = 1u << (ilog2(2048) - 6 + log2_dpte_req_w);
    rq->dpte_groups_per_row_ub = (dpte_row_width_ub + group_w - 1) / group_w;
    /* meta (DCC): o mOS não usa, mas o DML1 calcula a linha e os chunks de meta mesmo assim, e o DLG os programa */
    u32 log2_meta_req_h = log2_bh + 3, log2_meta_req_w = 6 + 8 - log2_bpe - log2_meta_req_h;
    u32 meta_req_w = 1u << log2_meta_req_w;
    u32 meta_row_width_ub = round_up(s->viewport_width - 1, meta_req_w) + meta_req_w;
    u32 meta_chunk_w = 1u << (11 + 8 - log2_bpe - log2_meta_req_h);       /* chunk de meta de 2 KiB */
    u32 min_meta_chunk_w = 1u << (8 + 8 - log2_bpe - log2_meta_req_h);    /* mínimo de 256 bytes */
    u32 per_row = meta_row_width_ub / meta_chunk_w, rem = meta_row_width_ub % meta_chunk_w;
    rq->meta_chunks_per_row_ub = rem <= 2 * min_meta_chunk_w - meta_req_w ? per_row + 1 : per_row + 2;
    rq->meta_row_height = 1u << log2_meta_req_h;

    /* registradores (dml1_extract_rq_regs) */
    rq->regs.chunk_size = ilog2(8192) - 10;
    rq->regs.min_chunk_size = ilog2(1024) - 8 + 1;
    rq->regs.meta_chunk_size = ilog2(2048) - 10;
    rq->regs.min_meta_chunk_size = ilog2(256) - 6 + 1;
    rq->regs.dpte_group_size = ilog2(2048) - 6;
    rq->regs.mpte_group_size = ilog2(2048) - 6;
    rq->regs.swath_height = log2_swath_h;
    rq->regs.pte_row_height_linear = 0;
    rq->regs.drq_expansion_mode = 2;                            /* chunk de 8 KiB (< 32 KiB) */
    rq->regs.prq_expansion_mode = rq->regs.mrq_expansion_mode = rq->regs.crq_expansion_mode = 1;
    return 0;
}

/* DCHUBP_REQ_SIZE_CONFIG (campos do dcn_1_0_sh_mask.h) */
u32 amd_dml1_req_size_config(const amd_dml1_rq_t *rq) {
    const amd_dml1_rq_regs_t *r = &rq->regs;
    return (r->swath_height & 0x7) | (r->pte_row_height_linear & 0x7) << 4 | (r->chunk_size & 0x7) << 8 |
           (r->min_chunk_size & 0x3) << 11 | (r->meta_chunk_size & 0x3) << 16 | (r->min_meta_chunk_size & 0x3) << 18 |
           (r->dpte_group_size & 0x7) << 20 | (r->mpte_group_size & 0x7) << 24;
}

/* ------------------------------------------------------------ DLG e TTU (dml1_rq_dlg_get_dlg_params) */
#define MIN_VBLANK_LINES      14u
#define DPPCLK_DELAY_SUBTOTAL 90u
#define DISPCLK_DELAY_SUBTOTAL 42u
#define URGENT_LATENCY_US     4.0
#define SR_ENTER_EXIT_US      11.0
#define DRAM_CLK_CHANGE_US    17.0

static double dmax(double a, double b) { return a > b ? a : b; }
static double dfloor(double v) { long long i = (long long)v; return (double)i > v ? (double)(i - 1) : (double)i; }
static double dceil(double v) { double f = dfloor(v); return f < v ? f + 1 : f; }

static double refcyc_per_delivery(double refclk, double pclk, u32 recout_width, double vratio, double hscale_rate,
                                  u32 delivery_width, u32 req_per_swath) {
    if (vratio <= 1.0) return refclk * recout_width / pclk / req_per_swath;
    return refclk * delivery_width / hscale_rate / req_per_swath;
}

void amd_dml1_dlg(const amd_dml1_dlg_in_t *in, const amd_dml1_rq_t *rq, u32 bpe, amd_dml1_dlg_t *o) {
    memset(o, 0, sizeof *o);
    double pclk = in->pixel_rate_mhz, refclk = in->refclk_mhz, ref2pix = refclk / pclk;
    u32 htotal = in->htotal, min_vblank = MIN_VBLANK_LINES;
    o->ref_freq_to_pix_freq = (u32)(ref2pix * (double)(1u << 19));
    o->refcyc_per_htotal = (u32)(ref2pix * htotal * 256.0);
    o->refcyc_h_blank_end = (u32)(in->hblank_end * ref2pix);
    o->dlg_vblank_end = in->interlaced ? in->vblank_end / 2 : in->vblank_end;

    double t_calc_us = 24.0 / in->deepsleep_dcfclk_mhz;            /* prefetch_xy_calc_in_dcfclk = 24 */
    double min_ttu_vblank = dmax(dmax(in->t_urg_wm_us, in->t_sr_wm_us), in->t_mclk_wm_us) + t_calc_us;
    double min_dst_y_ttu_vblank = min_ttu_vblank * pclk / htotal;
    u32 vblank_start = in->interlaced ? in->vblank_start / 2 : in->vblank_start;
    o->min_dst_y_next_start = (u32)((vblank_start + min_dst_y_ttu_vblank) * 4.0);

    double line_time_us = htotal / pclk;
    u32 pixel_rate_delay = (u32)(DPPCLK_DELAY_SUBTOTAL * pclk / in->dppclk_mhz + DISPCLK_DELAY_SUBTOTAL * pclk / in->dispclk_mhz);
    u32 vstartup = in->interlaced ? in->vstartup_start / 2 : in->vstartup_start;
    if (vstartup >= min_vblank) min_vblank = vstartup + 1;
    u32 dst_x_after = in->hsplit ? pixel_rate_delay + in->recout_width : pixel_rate_delay, dst_y_after = 0;
    if (dst_x_after >= htotal) { dst_x_after -= htotal; dst_y_after++; }

    double line_wait = dmax(dmax(URGENT_LATENCY_US, SR_ENTER_EXIT_US), DRAM_CLK_CHANGE_US + URGENT_LATENCY_US) / line_time_us;
    double line_o = dst_y_after + (double)dst_x_after / htotal;
    double line_setup = (double)(in->vupdate_offset + in->vupdate_width + in->vready_offset) / htotal;
    double line_calc = t_calc_us / line_time_us;
    double dst_y_prefetch = ((double)min_vblank - 1.0) - (line_setup + line_calc + line_wait + line_o);
    dst_y_prefetch = dfloor(4.0 * (dst_y_prefetch + 0.125)) / 4.0;
    double t_pre_us = dst_y_prefetch * line_time_us;

    /* quantas linhas de swath o prefetch precisa (get_swath_need, vinit = 1 sem escala) */
    u32 swath_h = rq->swath_height, max_num_sw = 1, max_partial_sw = swath_h - 1;
    if (max_partial_sw < 1) max_partial_sw = 1;
    double sw_bytes = (double)(max_num_sw * swath_h + max_partial_sw) * rq->swath_width_ub * bpe;
    double dpte_row_bytes = in->vm_en ? rq->dpte_bytes_per_row_ub : 0;
    double prefetch_bw = (2 * dpte_row_bytes + sw_bytes) / t_pre_us;
    double t_vm_us = line_time_us / 4.0;
    double t_r0_us = dmax(in->t_extra_us - t_vm_us, line_time_us - t_vm_us);
    if (in->vm_en) {
        t_r0_us = dmax(dpte_row_bytes / prefetch_bw, in->t_extra_us);
        t_r0_us = dmax(line_time_us - t_vm_us, t_r0_us);
        t_r0_us = dmax(URGENT_LATENCY_US * 2.0, t_r0_us);          /* iflip sem plano duplo */
    }
    o->dst_y_after_scaler = dst_y_after;
    o->refcyc_x_after_scaler = (u32)(dst_x_after * ref2pix);
    o->dst_y_prefetch = (u32)(dst_y_prefetch * 4.0);
    double dst_y_per_vm = dfloor(4.0 * (t_vm_us / line_time_us + 0.125)) / 4.0;
    double dst_y_per_row = dfloor(4.0 * (t_r0_us / line_time_us + 0.125)) / 4.0;
    o->dst_y_per_vm_vblank = (u32)(dst_y_per_vm * 4.0);
    o->dst_y_per_row_vblank = (u32)(dst_y_per_row * 4.0);

    double lsw = dst_y_prefetch - (dst_y_per_vm + dst_y_per_row);
    double vratio_pre = (max_num_sw * swath_h + max_partial_sw) / lsw;
    if (swath_h > 4) {
        double t = (double)(max_num_sw * swath_h) / (lsw - (1.0 - 3.0) / 2.0);   /* prefill = floor(vinit) = 1 */
        if (t > vratio_pre) vratio_pre = t;
    }
    if (vratio_pre < 1.0) vratio_pre = 1.0;
    if (vratio_pre > 4.0) vratio_pre = 4.0;
    o->vratio_prefetch = vratio_pre >= 4.0 ? (1u << 21) - 1 : (u32)(vratio_pre * (double)(1u << 19));
    o->refcyc_per_pte_group_vblank_l = (u32)(dst_y_per_row * htotal * ref2pix / rq->dpte_groups_per_row_ub);
    o->refcyc_per_meta_chunk_vblank_l = (u32)(dst_y_per_row * htotal * ref2pix / rq->meta_chunks_per_row_ub);

    /* ativo (vratio 1: sem escala) */
    o->dst_y_per_pte_row_nom_l = rq->dpte_row_height * 4;
    o->dst_y_per_meta_row_nom_l = rq->meta_row_height * 4;
    double nom = (double)rq->dpte_row_height * htotal * ref2pix / rq->dpte_groups_per_row_ub;
    o->refcyc_per_pte_group_nom_l = nom >= (double)(1u << 23) ? (1u << 23) - 1 : (u32)nom;
    double mnom = (double)rq->meta_row_height * htotal * ref2pix / rq->meta_chunks_per_row_ub;
    o->refcyc_per_meta_chunk_nom_l = mnom >= (double)(1u << 23) ? (1u << 23) - 1 : (u32)mnom;
    double hscale_rate = 2.0 * in->dppclk_mhz;                      /* htaps <= 1 */
    u32 full_recout = in->full_recout_width ? in->full_recout_width : in->recout_width;
    o->refcyc_per_line_delivery_pre_l = (u32)dfloor(refcyc_per_delivery(refclk, pclk, full_recout, vratio_pre, hscale_rate, rq->swath_width_ub, 1));
    o->refcyc_per_line_delivery_l = (u32)dfloor(refcyc_per_delivery(refclk, pclk, full_recout, 1.0, hscale_rate, rq->swath_width_ub, 1));
    o->chunk_hdl_adjust_cur0 = 3;

    /* TTU */
    u32 rec_in = in->recout_width;                                  /* sem escala: a largura lida = a mostrada */
    o->refcyc_per_req_delivery_pre_l = (u32)(refcyc_per_delivery(refclk, pclk, full_recout, vratio_pre, hscale_rate, rec_in, rq->req_per_swath_ub) * 1024.0);
    o->refcyc_per_req_delivery_l = (u32)(refcyc_per_delivery(refclk, pclk, full_recout, 1.0, hscale_rate, rec_in, rq->req_per_swath_ub) * 1024.0);
    o->qos_level_low_wm = 0;
    o->qos_level_high_wm = (u32)(4.0 * htotal * ref2pix);
    o->qos_level_flip = 14;
    o->qos_level_fixed_l = 8;
    o->min_ttu_vblank = (u32)(min_ttu_vblank * refclk);
    o->vratio_prefetch_c = 1u << 19;
    o->dst_y_per_meta_row_nom_c = o->dst_y_per_meta_row_nom_l;
    o->refcyc_per_meta_chunk_vblank_c = o->refcyc_per_meta_chunk_vblank_l;
    o->qos_level_fixed_c = 8;
    (void)dceil;
}
