/* mOS — teste de host do validador de PM4 do userland (kernel/dev/amdgpu/pm4check.c).
 * Rodar da raiz:
 *   gcc -O2 -Wall -Wextra -I kernel/dev/amdgpu tools/amdgpu_pm4_host.c kernel/dev/amdgpu/pm4check.c -o /tmp/pm4 && /tmp/pm4 */
#include "pm4check.h"

static int pass, total;
#define CHECK(c, ...) do { total++; if (c) pass++; else { printf("FALHOU %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define P3(op, n) (0xC0000000u | ((u32)(op) & 0xFFu) << 8 | ((u32)(n) & 0x3FFFu) << 16)

static const amd_pm4_bo_t bos[2] = {{0x100000000ull, 1u << 20}, {0x7F0000000000ull, 4096}};
static amd_pm4_ctx_t ctx(void) { amd_pm4_ctx_t c; memset(&c, 0, sizeof c); c.bo = bos; c.nbo = 2; return c; }
static int run(const u32 *ib, u32 n) { amd_pm4_ctx_t c = ctx(); return amd_pm4_check(&c, ib, n, false); }
#define RUN(...) ({ const u32 _b[] = {__VA_ARGS__}; run(_b, (u32)(sizeof _b / 4)); })

int main(void) {
    const u32 lo = 0x00000000u, hi = 0x1u;                  /* VA 0x1'0000'0000 (BO 0) */

    /* um desenho completo como o do libSystem: contexto, shader, índices, desenho, fence de usuário */
    u32 draw[] = {
        P3(PM4C_CONTEXT_CONTROL, 1), 0x80000000u, 0x80000000u,
        P3(PM4C_SET_CONTEXT_REG, 2), 0x0A0, 0x11, 0x22,        /* CB_*, 2 registradores */
        P3(PM4C_SET_SH_REG, 1), 0x48, 0xCAFE,                  /* SPI_SHADER_PGM_LO_PS */
        P3(PM4C_SET_UCONFIG_REG, 1), 0x242, 4,                 /* VGT_PRIMITIVE_TYPE */
        P3(PM4C_INDEX_TYPE, 0), 1,
        P3(PM4C_NUM_INSTANCES, 0), 1,
        P3(PM4C_INDEX_BASE, 1), lo + 0x1000, hi,
        P3(PM4C_INDEX_BUFFER_SIZE, 0), 300,
        P3(PM4C_DRAW_INDEX_2, 4), 300, lo + 0x1000, hi, 300, 0,
        P3(PM4C_DRAW_INDEX_AUTO, 1), 3, 2,
        P3(PM4C_ACQUIRE_MEM, 5), 0, 0, 0, 0, 0, 0,
        P3(PM4C_EVENT_WRITE, 0), 0x16,
        P3(PM4C_RELEASE_MEM, 6), 0x514, 1u << 29, lo + 0x2000, hi, 7, 0, 0,
        P3(PM4C_WRITE_DATA, 4), 5u << 8, lo + 0x3000, hi, 1, 2,
        P3(PM4C_COPY_DATA, 4), 5u | 5u << 8, 0x1234, 0, 0x00000000u, 0x7F00u,
        P3(PM4C_WAIT_REG_MEM, 5), 0x13, lo + 0x2000, hi, 7, 0xFFFFFFFFu, 0x20,
        P3(PM4C_SET_BASE, 2), 1, lo + 0x4000, hi,
        P3(PM4C_DRAW_INDIRECT, 3), 0, 0, 0, 0,
        P3(PM4C_DISPATCH_DIRECT, 3), 8, 8, 1, 1,
        P3(PM4C_NOP, 0x3FFF), 0x80000000u,
        P3(PM4C_NOP, 1), 0, 0,
    };
    amd_pm4_ctx_t c = ctx();
    int e = amd_pm4_check(&c, draw, (u32)(sizeof draw / 4), false);
    CHECK(e == 0, "desenho completo aceito (%s no dword %u)", amd_pm4_error(e), c.bad_dw);

    /* IB2 dentro de um BO: reportado para o chamador validar; aninhado ou com VMID recusado */
    c = ctx();
    u32 ib1[] = {P3(PM4C_INDIRECT_BUFFER, 2), lo + 0x8000, hi, 64};
    CHECK(amd_pm4_check(&c, ib1, 4, false) == 0 && c.nib2 == 1 && c.ib2[0].va == 0x100008000ull && c.ib2[0].ndw == 64, "IB2 reportado");
    c = ctx();
    CHECK(amd_pm4_check(&c, ib1, 4, true) == PM4E_IB2, "IB2 dentro de IB2");
    CHECK(RUN(P3(PM4C_INDIRECT_BUFFER, 2), lo + 0x8000, hi, 64 | 3u << 24) == PM4E_IB2, "IB2 com VMID");
    CHECK(RUN(P3(PM4C_INDIRECT_BUFFER, 2), lo + 0xFFF00, hi, 0x100) == PM4E_VA, "IB2 passa do fim do BO");
    c = ctx();
    u32 many[4 * 17];
    for (int i = 0; i < 17; i++) { many[4 * i] = P3(PM4C_INDIRECT_BUFFER, 2); many[4 * i + 1] = 0; many[4 * i + 2] = 1; many[4 * i + 3] = 4; }
    CHECK(amd_pm4_check(&c, many, 4 * 17, false) == PM4E_IB2 && c.nib2 == 16, "no máximo 16 IB2");

    /* registradores */
    CHECK(RUN(P3(PM4C_SET_UCONFIG_REG, 1), 0x200, 0xE0000000u) == PM4E_REG, "GRBM_GFX_INDEX recusado");
    CHECK(RUN(P3(PM4C_SET_UCONFIG_REG, 1), 0x40, 1) == PM4E_REG, "SCRATCH_REG0 recusado");
    CHECK(RUN(P3(PM4C_SET_CONTEXT_REG, 2), 0x3FF, 1, 2) == PM4E_REG, "contexto passa da janela");
    CHECK(RUN(P3(PM4C_SET_SH_REG, 1), 0x400, 1) == PM4E_REG, "SH fora da janela");
    CHECK(RUN(0x00001234u, 0) == PM4E_TYPE && RUN(0x40000000u) == PM4E_TYPE, "pacotes tipo 0 e 1 recusados");

    /* opcodes privilegiados */
    CHECK(RUN(P3(0x68, 1), 0x10, 1) == PM4E_OPCODE, "SET_CONFIG_REG recusado");
    CHECK(RUN(P3(0xA2, 5), 0, 0, 0, 0, 0, 0) == PM4E_OPCODE, "MAP_QUEUES recusado");
    CHECK(RUN(P3(0x50, 5), 0, 0, 0, 0, 0, 0) == PM4E_OPCODE, "DMA_DATA recusado");
    CHECK(RUN(P3(PM4C_SET_BASE, 2), 3, 0x8000, 0x8000) == PM4E_OPCODE, "SET_BASE do CE recusado");

    /* destinos e endereços */
    CHECK(RUN(P3(PM4C_WRITE_DATA, 3), 0u << 8, 0x2000, 0, 1) == PM4E_SEL, "WRITE_DATA em registrador");
    CHECK(RUN(P3(PM4C_WRITE_DATA, 3), 5u << 8, 0x1000, 0, 1) == PM4E_VA, "WRITE_DATA fora dos BOs");
    CHECK(RUN(P3(PM4C_WRITE_DATA, 4), 5u << 8, 0xFFFFC, hi, 1, 2) == PM4E_VA, "WRITE_DATA passa do fim do BO");
    CHECK(RUN(P3(PM4C_WRITE_DATA, 4), 5u << 8 | 1u << 16, 0xFFFFC, hi, 1, 2) == 0, "WRITE_DATA num endereço só");
    CHECK(RUN(P3(PM4C_COPY_DATA, 4), 0u | 5u << 8, 0x2000, 0, lo, hi) == PM4E_SEL, "COPY_DATA de registrador");
    CHECK(RUN(P3(PM4C_COPY_DATA, 4), 1u | 5u << 8, 0x10, 0, lo, hi) == PM4E_VA, "COPY_DATA de memória alheia");
    CHECK(RUN(P3(PM4C_COPY_DATA, 4), 1u | 0u << 8, lo, hi, 0x2000, 0) == PM4E_SEL, "COPY_DATA para registrador");
    CHECK(RUN(P3(PM4C_WAIT_REG_MEM, 5), 0x03, 0x2000, 0, 1, 1, 0x20) == PM4E_SEL, "WAIT_REG_MEM em registrador");
    CHECK(RUN(P3(PM4C_WAIT_REG_MEM, 5), 0x53, lo, hi, 1, 1, 0x20) == PM4E_SEL, "WAIT_REG_MEM que escreve");
    CHECK(RUN(P3(PM4C_RELEASE_MEM, 6), 0x514, 1u << 29 | 2u << 24, lo, hi, 1, 0, 0) == PM4E_INT, "RELEASE_MEM com interrupção");
    CHECK(RUN(P3(PM4C_RELEASE_MEM, 6), 0x514, 2u << 29, 0x0FFFFC, hi, 1, 0, 0) == PM4E_VA, "RELEASE_MEM de 64 bits no fim do BO");
    CHECK(RUN(P3(PM4C_EVENT_WRITE, 2), 0x101, 0x5000, 0) == PM4E_VA, "ZPASS_DONE fora dos BOs");
    CHECK(RUN(P3(PM4C_EVENT_WRITE, 2), 0x101, lo + 0x5000, hi) == 0, "ZPASS_DONE no BO");
    CHECK(RUN(P3(PM4C_DRAW_INDEX_2, 4), 0x1000, 0xFFFF0, hi, 3, 0) == PM4E_VA, "índices passam do fim do BO");

    /* forma */
    CHECK(RUN(P3(PM4C_SET_SH_REG, 3), 0x48, 1) == PM4E_TRUNC, "pacote cortado no fim");
    CHECK(run(draw, 0) == PM4E_EMPTY, "IB vazio");
    CHECK(RUN(P3(PM4C_NOP, 0x3FFF), P3(PM4C_NOP, 0x3FFF)) == 0, "só NOPs de um dword");
    CHECK(amd_pm4_error(PM4E_VA)[0] && amd_pm4_error(-99)[0], "mensagens");

    printf("amdgpu-pm4-host: %d/%d ok\n", pass, total);
    return pass == total ? 0 : 1;
}
