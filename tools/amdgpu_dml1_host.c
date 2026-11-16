/* mOS — teste de host do DML1 (kernel/dev/amdgpu/dml1.c) contra registradores lidos no Picasso do Thiago
 * (tools/testdata/amdgpu/dcn-live-5120x1440.json: 5120x1440 em dois pipes de 2560, superfície 64KB_S_X).
 * Rodar da raiz:
 *   gcc -O2 -Wall -Wextra -DMOS_HOST_TEST -I kernel/dev/amdgpu tools/amdgpu_dml1_host.c kernel/dev/amdgpu/dml1.c -o /tmp/dml1 && /tmp/dml1 */
#include "dml1.h"

static int pass, total;
#define CHECK(c, ...) do { total++; if (c) pass++; else { printf("FALHOU %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main(void) {
    amd_dml1_rq_t rq;
    /* o desktop do Linux: 4 bytes por pixel, tiles de 64 KiB, cada pipe lê 2560 de uma superfície de 5120 */
    amd_dml1_surface_t tiled = {4, 65536, 2560, 5120};
    CHECK(!amd_dml1_rq(&tiled, &rq), "RQ do caso vivo");
    CHECK(amd_dml1_req_size_config(&rq) == 0x055D1B03u, "DCHUBP_REQ_SIZE_CONFIG = 0x%08x (vivo 0x055D1B03)", amd_dml1_req_size_config(&rq));
    CHECK(rq.swath_height == 8 && !rq.req128 && rq.swath_width_ub == 2568 && rq.req_per_swath_ub == 321, "swath de 8 linhas, 321 pedidos");
    /* o que o mOS mostra: linear, 4 bytes por pixel */
    amd_dml1_surface_t lin = {4, 0, 2560, 5120};
    CHECK(!amd_dml1_rq(&lin, &rq) && rq.swath_height == 1 && rq.regs.swath_height == 0 && rq.swath_width_ub == 2624 && rq.req_per_swath_ub == 41,
          "linear: swath de 1 linha, 41 pedidos de 256 bytes (%u %u)", rq.swath_width_ub, rq.req_per_swath_ub);
    CHECK(rq.dpte_row_height == 64 && (amd_dml1_req_size_config(&rq) & 0x7) == 0, "linear: linha de PTE de 64 (42 x 8192 / 5120) (%u)", rq.dpte_row_height);
    amd_dml1_surface_t small = {4, 0, 1920, 1920};
    CHECK(!amd_dml1_rq(&small, &rq) && rq.dpte_row_height == 128, "1920 linear: linha de PTE de 128 (limite 2^7) (%u)", rq.dpte_row_height);
    amd_dml1_surface_t huge = {4, 0, 8192, 45000};
    CHECK(amd_dml1_rq(&huge, &rq) == -2, "pitch grande demais recusado");
    amd_dml1_surface_t bad = {3, 0, 100, 100};
    CHECK(amd_dml1_rq(&bad, &rq) == -1, "3 bytes por pixel recusado");
    amd_dml1_surface_t wide = {4, 65536, 4096, 4096};
    CHECK(!amd_dml1_rq(&wide, &rq) && rq.req128 && rq.swath_height == 4, "swath que não cabe no detile: pedidos de 128 bytes");
    /* DLG/TTU do caso vivo. Timing do EDID do monitor (5120x1440, pclk 481,14 MHz, htotal 5400); vupdate/vready
     * dos registradores OTG0 vivos; marcas d'água do DCHUBBUB vivo (refclk 48 MHz: 592, 1312 e 1408 ciclos); os
     * relógios dppclk 244,25 / dispclk 500 MHz e o dcfclk de deep sleep 30,3 MHz foram deduzidos dos próprios
     * registradores (o modelo de banda do DCN que os calcula é o passo seguinte da F4b). */
    amd_dml1_surface_t live = {4, 65536, 2560, 5120};
    amd_dml1_rq(&live, &rq);
    amd_dml1_dlg_in_t in = {
        .htotal = 5400, .hblank_end = 192, .vblank_start = 1482, .vblank_end = 42, .interlaced = false,
        .pixel_rate_mhz = 481.14, .refclk_mhz = 48.0, .dppclk_mhz = 244.25, .dispclk_mhz = 500.0,
        .vstartup_start = 13, .vupdate_offset = 1350, .vupdate_width = 302, .vready_offset = 394,
        .recout_width = 2560, .full_recout_width = 2560, .hsplit = true,
        .t_urg_wm_us = 592 / 48.0, .t_sr_wm_us = 1312 / 48.0, .t_mclk_wm_us = 1408 / 48.0, .t_extra_us = 0,
        .deepsleep_dcfclk_mhz = 30.3, .vm_en = true,
    };
    amd_dml1_dlg_t d;
    amd_dml1_dlg(&in, &rq, 4, &d);
#define LIVE(f, v) CHECK(d.f == (v), #f " = %u (vivo %u)", d.f, (unsigned)(v))
    LIVE(ref_freq_to_pix_freq, 52304); LIVE(refcyc_per_htotal, 137912); LIVE(refcyc_h_blank_end, 19); LIVE(dlg_vblank_end, 42);
    LIVE(min_dst_y_next_start, 5938); LIVE(refcyc_x_after_scaler, 277); LIVE(dst_y_after_scaler, 0);
    LIVE(dst_y_prefetch, 41); LIVE(vratio_prefetch, 850196); LIVE(dst_y_per_vm_vblank, 1); LIVE(dst_y_per_row_vblank, 3);
    LIVE(refcyc_per_pte_group_vblank_l, 202); LIVE(refcyc_per_meta_chunk_vblank_l, 134);
    LIVE(dst_y_per_pte_row_nom_l, 512); LIVE(refcyc_per_pte_group_nom_l, 34478); LIVE(dst_y_per_meta_row_nom_l, 256);
    LIVE(refcyc_per_meta_chunk_nom_l, 11492); LIVE(refcyc_per_line_delivery_l, 255); LIVE(refcyc_per_line_delivery_pre_l, 252);
    LIVE(refcyc_per_req_delivery_l, 814); LIVE(refcyc_per_req_delivery_pre_l, 802); LIVE(qos_level_high_wm, 2154);
    LIVE(qos_level_flip, 14); LIVE(qos_level_fixed_l, 8); LIVE(min_ttu_vblank, 1446);
    printf("amdgpu-dml1-host: %d/%d ok\n", pass, total);
    return pass == total ? 0 : 1;
}
