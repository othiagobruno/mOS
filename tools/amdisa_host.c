/* mOS — teste de host do montador GCN/GFX9 (userland/libsystem/amdisa.c): palavras de instruções conhecidas, uso de
 * registradores e RSRC1/2, e um kernel de compute buf[gid] = gid montado por inteiro.
 *   gcc -std=gnu11 -O1 -I userland/libsystem tools/amdisa_host.c userland/libsystem/amdisa.c -o /tmp/amdisa && /tmp/amdisa */
#include "amdisa.h"
int printf(const char *, ...);
static int pass, total;
#define CHECK(c, ...) do { total++; if (c) pass++; else { printf("FALHA %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
static amdisa_u32 buf[256];
static amdasm_t A;
static amdisa_u32 one(void) { return A.w[A.n - 1]; }

int main(void) {
    amdasm_init(&A, buf, 256);
    amd_sopp(&A, AMD_S_ENDPGM, 0);
    CHECK(one() == 0xBF810000u, "s_endpgm %08x", one());
    amd_sopp(&A, AMD_S_WAITCNT, 0);
    CHECK(one() == 0xBF8C0000u, "s_waitcnt 0 %08x", one());
    amd_sop1(&A, AMD_S_MOV_B32, 0, AMD_S(1), 0);
    CHECK(one() == 0xBE800001u, "s_mov_b32 s0, s1 %08x", one());
    amd_vop1(&A, AMD_V_MOV_B32, 0, AMD_V(1), 0);
    CHECK(one() == 0x7E000301u, "v_mov_b32 v0, v1 %08x", one());
    amd_vop2(&A, AMD_V_ADD_F32, 0, AMD_V(1), 2, 0);
    CHECK(one() == 0x02000501u, "v_add_f32 v0, v1, v2 %08x", one());
    amd_vop2(&A, AMD_V_LSHLREV_B32, 0, AMD_IC(2), 0, 0);
    CHECK(one() == 0x24000082u, "v_lshlrev_b32 v0, 2, v0 %08x", one());
    unsigned n0 = A.n;
    amd_smem(&A, AMD_S_LOAD_DWORDX2, 0, 4, 0, 0);
    CHECK(A.n == n0 + 2 && A.w[n0] == 0xC0060002u && A.w[n0 + 1] == 0, "s_load_dwordx2 s[0:1], s[4:5], 0 %08x %08x", A.w[n0], A.w[n0 + 1]);
    n0 = A.n;
    amd_global(&A, AMD_GLOBAL_STORE_DWORD, 0, 0, 2, AMD_GLOBAL_OFF, 0, 0);
    CHECK(A.w[n0] == 0xDC708000u && A.w[n0 + 1] == 0x007F0200u, "global_store_dword v[0:1], v2, off %08x %08x", A.w[n0], A.w[n0 + 1]);
    n0 = A.n;
    amd_global(&A, AMD_GLOBAL_LOAD_DWORD, 1, 2, 0, AMD_GLOBAL_OFF, 0, 0);
    CHECK(A.w[n0] == 0xDC508000u && A.w[n0 + 1] == 0x017F0002u, "global_load_dword v1, v[2:3], off %08x %08x", A.w[n0], A.w[n0 + 1]);
    n0 = A.n;
    amd_vop2(&A, AMD_V_MUL_F32, 0, AMD_LIT, 1, 0x40490FDBu);   /* pi */
    CHECK(A.n == n0 + 2 && A.w[n0] == 0x0A0002FFu && A.w[n0 + 1] == 0x40490FDBu, "v_mul_f32 v0, lit, v1 + literal %08x", A.w[n0]);
    CHECK(amd_waitcnt(0, -1, -1) == 0x0F70u, "waitcnt vmcnt(0) %04x", amd_waitcnt(0, -1, -1));
    CHECK(amd_waitcnt(-1, -1, 0) == 0xC07Fu, "waitcnt lgkmcnt(0) %04x", amd_waitcnt(-1, -1, 0));
    CHECK(!A.err, "sem erro");
    n0 = A.n;
    amd_vop3(&A, AMD_V3_FMA_F32, 3, AMD_V(0), AMD_V(1), AMD_V(2), 0, 0, 0);
    CHECK(A.w[n0] == 0xD1CB0003u && A.w[n0 + 1] == 0x040A0300u, "v_fma_f32 v3, v0, v1, v2 %08x %08x", A.w[n0], A.w[n0 + 1]);
    amd_vop3(&A, AMD_V3_FMA_F32, 3, AMD_LIT, AMD_V(1), AMD_V(2), 0, 0, 0);
    CHECK(A.err, "VOP3 com literal é erro no GFX9");

    /* kernel: s[0:1] = endereço do buffer (dados de usuário), s2 = id do grupo x, v0 = id da thread; 64 por grupo */
    amdasm_init(&A, buf, 256);
    amd_sop2(&A, AMD_S_LSHL_B32, 3, AMD_S(2), AMD_IC(6), 0);              /* s3 = grupo * 64 */
    amd_vop2(&A, AMD_V_ADD_U32, 1, AMD_S(3), 0, 0);                        /* v1 = gid */
    amd_vop2(&A, AMD_V_LSHLREV_B32, 2, AMD_IC(2), 1, 0);                   /* v2 = gid * 4 */
    amd_vop2(&A, AMD_V_ADD_CO_U32, 2, AMD_S(0), 2, 0);                     /* v2 = base_lo + v2, vcc = carry */
    amd_vop1(&A, AMD_V_MOV_B32, 3, AMD_S(1), 0);
    amd_vop2(&A, AMD_V_ADDC_CO_U32, 3, AMD_ZERO, 3, 0);                    /* v3 = base_hi + carry */
    amd_global(&A, AMD_GLOBAL_STORE_DWORD, 0, 2, 1, AMD_GLOBAL_OFF, 0, 0);
    amd_sopp(&A, AMD_S_WAITCNT, amd_waitcnt(0, -1, -1));
    amd_sopp(&A, AMD_S_ENDPGM, 0);
    CHECK(!A.err && A.n == 10, "kernel montado: %u palavras", A.n);
    CHECK(A.max_vgpr == 4 && A.max_sgpr == 4, "uso: %u VGPRs, %u SGPRs", A.max_vgpr, A.max_sgpr);
    amdisa_u32 r1 = amd_rsrc1(A.max_vgpr, A.max_sgpr), r2 = amd_rsrc2(2, 1, 1);
    CHECK((r1 & 0x3F) == 0 && ((r1 >> 6) & 0xF) == 1 && (r1 >> 21 & 1) && (r1 >> 23 & 1), "RSRC1 %08x", r1);
    CHECK(((r2 >> 1) & 0x1F) == 2 && (r2 >> 7 & 1) && !((r2 >> 8) & 3) && !((r2 >> 11) & 3), "RSRC2 %08x", r2);
    printf("amdisa: %d/%d ok\n", pass, total);
    return pass != total;
}
