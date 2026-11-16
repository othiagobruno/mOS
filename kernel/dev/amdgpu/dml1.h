/* mOS — parâmetros de busca do HUBP do DCN 1.0 (dml1.c). Lógica pura. */
#pragma once
#include "amdgpu_base.h"

typedef struct {
    u32 bytes_per_pixel;      /* 2, 4 ou 8 (RGB 4:4:4) */
    u32 tile_bytes;           /* 0 = linear; 4096, 65536 ou 262144 */
    u32 viewport_width;       /* pixels lidos por este pipe */
    u32 pitch_px;
} amd_dml1_surface_t;

typedef struct {
    u32 chunk_size, min_chunk_size, meta_chunk_size, min_meta_chunk_size;
    u32 dpte_group_size, mpte_group_size, swath_height, pte_row_height_linear;
    u32 drq_expansion_mode, prq_expansion_mode, mrq_expansion_mode, crq_expansion_mode;
} amd_dml1_rq_regs_t;

typedef struct {
    u32  swath_width_ub, req_per_swath_ub, stored_swath_bytes, swath_height;
    bool req128;
    u32  dpte_row_height, dpte_req_per_row_ub, dpte_bytes_per_row_ub, dpte_groups_per_row_ub, meta_row_height;
    u32  meta_chunks_per_row_ub;
    amd_dml1_rq_regs_t regs;
} amd_dml1_rq_t;

/* 0 ou < 0 (formato fora do suportado, pitch grande demais para o buffer de PTE) */
int amd_dml1_rq(const amd_dml1_surface_t *s, amd_dml1_rq_t *rq);
u32 amd_dml1_req_size_config(const amd_dml1_rq_t *rq);

/* DLG/TTU: entradas (as do e2e_pipe_params e do dlg_sys_params do DML1, só o que o mOS usa: um plano RGB por
 * pipe, sem escala, sem DCC, sem cursor de hardware) */
typedef struct {
    u32    htotal, hblank_end, vblank_start, vblank_end;   /* hblank_end = htotal - hactive - hfront etc. */
    bool   interlaced;
    double pixel_rate_mhz, refclk_mhz, dppclk_mhz, dispclk_mhz;
    u32    vstartup_start, vupdate_offset, vupdate_width, vready_offset;
    u32    recout_width, full_recout_width;                /* largura deste pipe e a da tela inteira */
    bool   hsplit;                                         /* tela dividida em pipes */
    double t_urg_wm_us, t_sr_wm_us, t_mclk_wm_us, t_extra_us, deepsleep_dcfclk_mhz;
    bool   vm_en;
} amd_dml1_dlg_in_t;

typedef struct {
    u32 ref_freq_to_pix_freq, refcyc_per_htotal, refcyc_h_blank_end, dlg_vblank_end, min_dst_y_next_start;
    u32 refcyc_x_after_scaler, dst_y_after_scaler, dst_y_prefetch, dst_y_per_vm_vblank, dst_y_per_row_vblank;
    u32 vratio_prefetch, refcyc_per_pte_group_vblank_l, refcyc_per_meta_chunk_vblank_l;
    u32 dst_y_per_pte_row_nom_l, dst_y_per_meta_row_nom_l, refcyc_per_pte_group_nom_l, refcyc_per_meta_chunk_nom_l;
    u32 refcyc_per_line_delivery_pre_l, refcyc_per_line_delivery_l, chunk_hdl_adjust_cur0;
    /* TTU */
    u32 refcyc_per_req_delivery_pre_l, refcyc_per_req_delivery_l, qos_level_low_wm, qos_level_high_wm;
    u32 qos_level_flip, qos_level_fixed_l, min_ttu_vblank;
} amd_dml1_dlg_t;

void amd_dml1_dlg(const amd_dml1_dlg_in_t *in, const amd_dml1_rq_t *rq, u32 bytes_per_pixel, amd_dml1_dlg_t *o);
