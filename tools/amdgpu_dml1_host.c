/* mOS — teste de host do DML1 (kernel/dev/amdgpu/dml1.c) contra registradores lidos no Picasso do Thiago
 * (tools/testdata/amdgpu/dcn-live-5120x1440.json: 5120x1440 em dois pipes de 2560, superfície 64KB_R_X).
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
    printf("amdgpu-dml1-host: %d/%d ok\n", pass, total);
    return pass == total ? 0 : 1;
}
