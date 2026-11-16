/* mOS libSystem — montador da ISA GCN do GFX9 (Vega), a partir do manual público "Vega Instruction Set Architecture"
 * da AMD: os formatos SOP1/SOP2/SOPK/SOPC/SOPP, SMEM, VOP1/VOP2/VOPC/VOP3, DS e FLAT/GLOBAL, os operandos (SGPR, VGPR,
 * constantes embutidas, literal, VCC, EXEC, M0) e os registradores de programa do compute (COMPUTE_PGM_RSRC1/2).
 * Quem gera código (o compilador do IR do interpretador) escreve num amdasm_t e entrega as palavras a um BO
 * com MOS_GPU_BO_EXEC. */
#pragma once

typedef unsigned int amdisa_u32;

typedef struct {
    amdisa_u32 *w;
    unsigned n, cap;
    int err;                   /* 1 = estourou o buffer ou operando inválido */
    unsigned max_sgpr, max_vgpr;   /* maior registrador usado + 1 (para RSRC1) */
} amdasm_t;

/* operandos de origem (campo de 9 bits) */
#define AMD_S(n)       ((unsigned)(n))            /* SGPR 0..101 */
#define AMD_VCC_LO     106u
#define AMD_VCC_HI     107u
#define AMD_M0         124u
#define AMD_EXEC_LO    126u
#define AMD_EXEC_HI    127u
#define AMD_ZERO       128u
#define AMD_IC(i)      ((i) >= 0 ? 128u + (unsigned)(i) : 192u + (unsigned)(-(i)))   /* inteiros -16..64 */
#define AMD_F_HALF     240u                       /* 0.5 */
#define AMD_F_M_HALF   241u
#define AMD_F_ONE      242u
#define AMD_F_M_ONE    243u
#define AMD_F_TWO      244u
#define AMD_F_M_TWO    245u
#define AMD_F_FOUR     246u
#define AMD_F_M_FOUR   247u
#define AMD_LIT        255u                       /* literal de 32 bits depois da instrução */
#define AMD_V(n)       (256u + (unsigned)(n))     /* VGPR como origem */
#define AMD_GLOBAL_OFF 0x7Fu                      /* saddr "off" do GLOBAL */

void amdasm_init(amdasm_t *a, amdisa_u32 *buf, unsigned cap);
void amdasm_word(amdasm_t *a, amdisa_u32 w);

/* escalar */
void amd_sop2(amdasm_t *a, unsigned op, unsigned sdst, unsigned s0, unsigned s1, amdisa_u32 lit);
void amd_sop1(amdasm_t *a, unsigned op, unsigned sdst, unsigned s0, amdisa_u32 lit);
void amd_sopk(amdasm_t *a, unsigned op, unsigned sdst, unsigned simm16);
void amd_sopc(amdasm_t *a, unsigned op, unsigned s0, unsigned s1, amdisa_u32 lit);
void amd_sopp(amdasm_t *a, unsigned op, unsigned simm16);
/* SMEM: s_load_dword(x2/x4/x8/x16) sdata, sbase (par de SGPRs), deslocamento em bytes */
void amd_smem(amdasm_t *a, unsigned op, unsigned sdata, unsigned sbase, unsigned offset, int glc);
/* vetor; vsrc1/vdst são índices de VGPR (sem o +256) */
void amd_vop2(amdasm_t *a, unsigned op, unsigned vdst, unsigned src0, unsigned vsrc1, amdisa_u32 lit);
void amd_vop1(amdasm_t *a, unsigned op, unsigned vdst, unsigned src0, amdisa_u32 lit);
void amd_vopc(amdasm_t *a, unsigned op, unsigned src0, unsigned vsrc1, amdisa_u32 lit);
/* VOP3a: op de 10 bits (VOP2 = 0x100 + op, VOP1 = 0x140 + op, VOPC = op), neg/abs por operando, clamp */
void amd_vop3(amdasm_t *a, unsigned op, unsigned vdst, unsigned src0, unsigned src1, unsigned src2, unsigned neg, unsigned abs, int clamp);
/* VOP3b (com SGPR de carry/condição): v_add_co_u32 e afins em forma de 3 operandos */
void amd_vop3b(amdasm_t *a, unsigned op, unsigned vdst, unsigned sdst, unsigned src0, unsigned src1, unsigned src2);
/* DS (LDS) */
void amd_ds(amdasm_t *a, unsigned op, unsigned vdst, unsigned addr, unsigned data0, unsigned data1, unsigned offset0, unsigned offset1);
/* GLOBAL: endereço de 64 bits em VGPRs (saddr = AMD_GLOBAL_OFF) ou SGPR base + deslocamento de 32 bits em VGPR */
void amd_global(amdasm_t *a, unsigned op, unsigned vdst, unsigned vaddr, unsigned vdata, unsigned saddr, int offset13, int glc);

/* opcodes (GFX9) */
enum {
    /* SOPP */
    AMD_S_NOP = 0, AMD_S_ENDPGM = 1, AMD_S_BRANCH = 2, AMD_S_CBRANCH_SCC0 = 4, AMD_S_CBRANCH_SCC1 = 5,
    AMD_S_CBRANCH_VCCZ = 6, AMD_S_CBRANCH_VCCNZ = 7, AMD_S_CBRANCH_EXECZ = 8, AMD_S_CBRANCH_EXECNZ = 9,
    AMD_S_BARRIER = 10, AMD_S_WAITCNT = 12,
    /* SOP1 */
    AMD_S_MOV_B32 = 0, AMD_S_MOV_B64 = 1, AMD_S_AND_SAVEEXEC_B64 = 32,
    /* SOP2 */
    AMD_S_ADD_U32 = 0, AMD_S_SUB_U32 = 1, AMD_S_ADD_I32 = 2, AMD_S_SUB_I32 = 3, AMD_S_ADDC_U32 = 4, AMD_S_SUBB_U32 = 5,
    AMD_S_AND_B32 = 12, AMD_S_AND_B64 = 13, AMD_S_OR_B32 = 14, AMD_S_OR_B64 = 15, AMD_S_XOR_B32 = 16,
    AMD_S_ANDN2_B64 = 19, AMD_S_LSHL_B32 = 28, AMD_S_LSHR_B32 = 30, AMD_S_MUL_I32 = 36,
    /* SOPK */
    AMD_S_MOVK_I32 = 0,
    /* SMEM */
    AMD_S_LOAD_DWORD = 0, AMD_S_LOAD_DWORDX2 = 1, AMD_S_LOAD_DWORDX4 = 2, AMD_S_LOAD_DWORDX8 = 3, AMD_S_LOAD_DWORDX16 = 4,
    /* VOP1 */
    AMD_V_MOV_B32 = 1, AMD_V_CVT_F32_I32 = 5, AMD_V_CVT_F32_U32 = 6, AMD_V_CVT_U32_F32 = 7, AMD_V_CVT_I32_F32 = 8,
    AMD_V_RCP_F32 = 34, AMD_V_SQRT_F32 = 39,
    /* VOP2 */
    AMD_V_CNDMASK_B32 = 0, AMD_V_ADD_F32 = 1, AMD_V_SUB_F32 = 2, AMD_V_MUL_F32 = 5, AMD_V_MIN_F32 = 10, AMD_V_MAX_F32 = 11,
    AMD_V_LSHRREV_B32 = 16, AMD_V_LSHLREV_B32 = 18, AMD_V_AND_B32 = 19, AMD_V_OR_B32 = 20, AMD_V_XOR_B32 = 21,
    AMD_V_ADD_CO_U32 = 25, AMD_V_SUB_CO_U32 = 26, AMD_V_ADDC_CO_U32 = 28, AMD_V_ADD_U32 = 52, AMD_V_SUB_U32 = 53,
    /* VOP3 */
    AMD_V3_MAD_U32_U24 = 0x1C3, AMD_V3_FMA_F32 = 0x1CB, AMD_V3_MUL_LO_U32 = 0x285,
    /* VOPC */
    AMD_V_CMP_LT_F32 = 0x41, AMD_V_CMP_GT_F32 = 0x44, AMD_V_CMP_LT_U32 = 0xC9, AMD_V_CMP_GT_U32 = 0xCC,
    /* DS */
    AMD_DS_WRITE_B32 = 13, AMD_DS_READ_B32 = 54,
    /* GLOBAL */
    AMD_GLOBAL_LOAD_DWORD = 20, AMD_GLOBAL_LOAD_DWORDX2 = 21, AMD_GLOBAL_LOAD_DWORDX4 = 23,
    AMD_GLOBAL_STORE_DWORD = 28, AMD_GLOBAL_STORE_DWORDX2 = 29, AMD_GLOBAL_STORE_DWORDX4 = 31,
};
/* s_waitcnt: contadores que esperar (o máximo de cada campo = não esperar) */
unsigned amd_waitcnt(int vmcnt, int expcnt, int lgkmcnt);

/* COMPUTE_PGM_RSRC1/2 a partir do uso de registradores e do que o kernel recebe */
amdisa_u32 amd_rsrc1(unsigned nvgpr, unsigned nsgpr);
/* tgid: bits 0..2 = recebe o id do grupo x/y/z em SGPRs logo depois dos de usuário; tid_dims: 1..3 (v0, v1, v2) */
amdisa_u32 amd_rsrc2(unsigned nuser_sgpr, unsigned tgid, unsigned tid_dims);
