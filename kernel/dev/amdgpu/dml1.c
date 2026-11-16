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
    /* meta (DCC): o mOS não usa, mas o DLG lê a altura da linha de meta */
    rq->meta_row_height = 1u << (log2_bh + 3);

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
