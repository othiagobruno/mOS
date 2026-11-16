/* mOS — teste de host do modelo de banda do DCN 1.0 (kernel/dev/amdgpu/dcnbw.c + dcnbw_auto.c) contra o Picasso do
 * Thiago com o Linux: 5120x1440 a 481,14 MHz no HDMI (tools/testdata/amdgpu/dcn-live-5120x1440.json). As tabelas de
 * fclk são as do SMU vivo (pp_dpm_mclk do Raven mostra as FClocks: 400/933/1200/1333 MHz); o dcfclk fica no padrão do
 * dcn10_soc_defaults. O DLG/TTU de dml1.c, alimentado só com a saída do modelo, tem de dar os 25 campos lidos no HUBP0.
 * Rodar da raiz:
 *   gcc -O2 -Wall -Wextra -DMOS_HOST_TEST -I kernel/dev/amdgpu tools/amdgpu_dcnbw_host.c kernel/dev/amdgpu/dcnbw.c \
 *       kernel/dev/amdgpu/dcnbw_auto.c kernel/dev/amdgpu/dml1.c -o /tmp/dcnbw && /tmp/dcnbw */
#include "dcnbw.h"

static int pass, total;
#define CHECK(c, ...) do { total++; if (c) pass++; else { printf("FALHOU %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static amd_dcnbw_stream_t live_stream(void) {
    return (amd_dcnbw_stream_t){ .h_total = 5400, .h_active = 5120, .h_front_porch = 88, .h_sync_width = 44,
        .v_total = 1485, .v_active = 1440, .v_front_porch = 3, .v_sync_width = 5, .pixel_clock_mhz = 481.14,
        .hdmi = true, .bpc = 8, .has_plane = true, .bytes_per_pixel = 4, .sw_mode = 25, .src_width = 5120, .src_height = 1440 };
}

int main(void) {
    amd_dcnbw_soc_t soc;
    amd_dcnbw_soc_defaults(&soc);
    u32 f[4] = {400, 933, 1200, 1333};
    memcpy(soc.fclk_mhz, f, sizeof f);
    soc.nfclk = 4;
    amd_dcnbw_stream_t s = live_stream();
    amd_dcnbw_out_t o;
    CHECK(amd_dcnbw_validate(&soc, &s, 1, &o) == 0 && o.ok, "5120x1440 cabe");
    CHECK(o.voltage_level == 0 && o.s[0].dpp == 2, "nível 0 com dois pipes (divisão forçada com uma tela): %d %u", o.voltage_level, o.s[0].dpp);
    /* OTG0 vivo */
    CHECK(o.s[0].vstartup_start == 13, "VSTARTUP %u (vivo 13)", o.s[0].vstartup_start);
    CHECK(o.s[0].vupdate_offset == 1350 && o.s[0].vupdate_width == 302, "VUPDATE %u+%u (vivo 1350+302)", o.s[0].vupdate_offset, o.s[0].vupdate_width);
    CHECK(o.s[0].vready_offset == 394, "VREADY %u (vivo 394)", o.s[0].vready_offset);
    CHECK(o.s[0].vblank_start == 0 && o.s[0].vblank_end == 0, "vblank do pipe_dlg_param zerado no progressivo (%u/%u)", o.s[0].vblank_start, o.s[0].vblank_end);
    /* DCHUBBUB vivo (conjuntos A..D iguais), refclk 48 MHz */
#define WM(f, live) for (int i = 0; i < 4; i++) CHECK(amd_dcnbw_wm_cycles(o.wm[i].f, 48000) == (live), #f "[%d] = %u (vivo %u)", i, amd_dcnbw_wm_cycles(o.wm[i].f, 48000), (unsigned)(live))
    WM(urgent_ns, 592); WM(pte_meta_urgent_ns, 976); WM(sr_exit_ns, 1232); WM(sr_enter_plus_exit_ns, 1312); WM(pstate_change_ns, 1408);
    /* relógios: o DENTIST arredonda o dispclk pedido para cima (DFS de ~496,55 MHz programado no vivo) */
    CHECK(o.dispclk_khz == 488381 && o.dppclk_khz == 244190, "dispclk/dppclk %u/%u kHz", o.dispclk_khz, o.dppclk_khz);
    CHECK(o.dcfclk_khz == 300000 && o.fclk_khz == 400000, "dcfclk/fclk %u/%u kHz", o.dcfclk_khz, o.fclk_khz);
    CHECK(o.dcfclk_deep_sleep_khz > 30000 && o.dcfclk_deep_sleep_khz < 30400, "deep sleep %u kHz", o.dcfclk_deep_sleep_khz);
    CHECK(o.s[0].viewport_width[0] == 2560 && o.s[0].viewport_width[1] == 2560 && o.s[0].viewport_x[1] == 2560, "viewport dividido 2560 + 2560");

    /* DLG/TTU do HUBP0 só com a saída do modelo */
    amd_dml1_rq_t rq;
    amd_dml1_surface_t surf = {4, 65536, o.s[0].viewport_width[0], 5120};
    CHECK(!amd_dml1_rq(&surf, &rq), "RQ");
    amd_dml1_dlg_t d;
    amd_dml1_dlg(&o.s[0].dlg[0], &rq, 4, &d);
#define LIVE(f, v) CHECK(d.f == (v), "DLG " #f " = %u (vivo %u)", d.f, (unsigned)(v))
    LIVE(ref_freq_to_pix_freq, 52304); LIVE(refcyc_per_htotal, 137912); LIVE(refcyc_h_blank_end, 19); LIVE(dlg_vblank_end, 42);
    LIVE(min_dst_y_next_start, 5938); LIVE(refcyc_x_after_scaler, 277); LIVE(dst_y_after_scaler, 0);
    LIVE(dst_y_prefetch, 41); LIVE(vratio_prefetch, 850196); LIVE(dst_y_per_vm_vblank, 1); LIVE(dst_y_per_row_vblank, 3);
    LIVE(refcyc_per_pte_group_vblank_l, 202); LIVE(refcyc_per_meta_chunk_vblank_l, 134);
    LIVE(dst_y_per_pte_row_nom_l, 512); LIVE(refcyc_per_pte_group_nom_l, 34478); LIVE(dst_y_per_meta_row_nom_l, 256);
    LIVE(refcyc_per_meta_chunk_nom_l, 11492); LIVE(refcyc_per_line_delivery_l, 255); LIVE(refcyc_per_line_delivery_pre_l, 252);
    LIVE(refcyc_per_req_delivery_l, 814); LIVE(refcyc_per_req_delivery_pre_l, 802); LIVE(qos_level_high_wm, 2154);
    LIVE(qos_level_flip, 14); LIVE(qos_level_fixed_l, 8); LIVE(min_ttu_vblank, 1446);

    /* outros casos: 1080p60 no DP também divide (uma tela), duas telas não dividem à toa, 3 telas 4K não cabem em 4 pipes */
    amd_dcnbw_stream_t hd = { .h_total = 2200, .h_active = 1920, .h_front_porch = 88, .h_sync_width = 44,
        .v_total = 1125, .v_active = 1080, .v_front_porch = 4, .v_sync_width = 5, .pixel_clock_mhz = 148.5,
        .hdmi = false, .bpc = 8, .has_plane = true, .bytes_per_pixel = 4, .sw_mode = 0, .src_width = 1920, .src_height = 1080 };
    CHECK(amd_dcnbw_validate(&soc, &hd, 1, &o) == 0 && o.s[0].dpp == 2 && o.voltage_level == 0, "1080p sozinho: dois pipes no nível 0 (%u %d)", o.s[0].dpp, o.voltage_level);
    CHECK(o.dispclk_khz >= 100000, "dispclk mínimo de 100 MHz (%u)", o.dispclk_khz);
    amd_dcnbw_stream_t two[2] = { live_stream(), hd };
    CHECK(amd_dcnbw_validate(&soc, two, 2, &o) == 0 && o.ok, "5120x1440 + 1080p cabem");
    CHECK(o.s[0].dpp + o.s[1].dpp <= 4, "pipes usados <= 4 (%u + %u)", o.s[0].dpp, o.s[1].dpp);
    amd_dcnbw_stream_t uhd = { .h_total = 4400, .h_active = 3840, .h_front_porch = 176, .h_sync_width = 88,
        .v_total = 2250, .v_active = 2160, .v_front_porch = 8, .v_sync_width = 10, .pixel_clock_mhz = 594.0,
        .hdmi = true, .bpc = 8, .has_plane = true, .bytes_per_pixel = 4, .sw_mode = 25, .src_width = 3840, .src_height = 2160 };
    CHECK(amd_dcnbw_validate(&soc, &uhd, 1, &o) == 0 && o.ok, "4K60 sozinho cabe (nível %d)", o.voltage_level);
    amd_dcnbw_stream_t many[4] = { uhd, uhd, uhd, uhd };
    CHECK(amd_dcnbw_validate(&soc, many, 4, &o) == 0 && o.s[0].dpp + o.s[1].dpp + o.s[2].dpp + o.s[3].dpp == 4, "quatro 4K60: um pipe cada (nível %d)", o.voltage_level);
    amd_dcnbw_stream_t uhd8k = { .h_total = 9000, .h_active = 7680, .h_front_porch = 352, .h_sync_width = 176,
        .v_total = 4400, .v_active = 4320, .v_front_porch = 16, .v_sync_width = 20, .pixel_clock_mhz = 2376.0,
        .hdmi = true, .bpc = 8, .has_plane = true, .bytes_per_pixel = 4, .sw_mode = 25, .src_width = 7680, .src_height = 4320 };
    CHECK(amd_dcnbw_validate(&soc, &uhd8k, 1, &o) == -1 && !o.ok && o.voltage_level == 5, "8K60 não cabe (nível %d)", o.voltage_level);
    amd_dcnbw_stream_t bad = hd; bad.h_total = 0;
    CHECK(amd_dcnbw_validate(&soc, &bad, 1, &o) == -1, "timing sem htotal recusado");
    CHECK(amd_dcnbw_validate(&soc, &hd, 0, &o) == -1 && amd_dcnbw_validate(&soc, many, 5, &o) == -1, "0 ou mais de 4 streams recusados");
    /* Raven2 (DCN 1.01): três pipes */
    amd_dcnbw_soc_t rv2 = soc; rv2.dcn101 = true; rv2.num_pipes = 3;
    amd_dcnbw_stream_t three[3] = { hd, hd, hd };
    CHECK(amd_dcnbw_validate(&rv2, three, 3, &o) == 0 && o.s[0].dpp + o.s[1].dpp + o.s[2].dpp == 3, "Raven2: três 1080p em três pipes");
    CHECK(amd_dcnbw_wm_cycles(0xFFFFFFFFu, 48000) == 0x1FFFFF, "marca d'água limitada a 21 bits");

    printf("amdgpu-dcnbw-host: %d/%d ok\n", pass, total);
    return pass == total ? 0 : 1;
}
