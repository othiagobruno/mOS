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
    amd_dml1_rq_regs_t regs;
} amd_dml1_rq_t;

/* 0 ou < 0 (formato fora do suportado, pitch grande demais para o buffer de PTE) */
int amd_dml1_rq(const amd_dml1_surface_t *s, amd_dml1_rq_t *rq);
u32 amd_dml1_req_size_config(const amd_dml1_rq_t *rq);
