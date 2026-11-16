/* mOS libSystem — driver de usuário da GPU AMD (GFX9) sobre a ABI de GPU revisão 3 (kernel/include/mos_gpu.h,
 * docs/gpu-abi.md): contexto com VM própria, BOs com VA, fluxo de comandos PM4 num BO, envio pelo SYS_GPU e
 * fence. O kernel valida cada IB (kernel/dev/amdgpu/pm4check.c); aqui só saem pacotes da lista aceita. */
#pragma once
#include "gpucall.h"

typedef unsigned int amd_u32;
typedef unsigned long long amd_u64;

typedef struct {
    unsigned ctx;              /* MOS_GPU_CTX_AMD */
    unsigned family, num_cu;
    amd_u64 vram, gtt;
} amdws_t;

typedef struct {
    unsigned handle;
    unsigned flags;            /* MOS_GPU_BO_* */
    amd_u64 va, size;
    void *map;                 /* mapeamento no processo (0 = não mapeado) */
} amdbo_t;

/* fluxo de comandos: dwords num BO mapeado e a lista de BOs que o lote usa */
#define AMDCS_MAX_BO 256
typedef struct {
    amdbo_t ib;
    amd_u32 *dw;
    amd_u32 n, cap;            /* em dwords */
    unsigned bo[AMDCS_MAX_BO];
    unsigned nbo;
    int ring;                  /* MOS_GPU_AMD_RING_* */
    int overflow;
} amdcs_t;

/* 0 ou -errno; -MOS_ENOENT sem MOS_GPU_CAP_AMD */
int amdws_open(amdws_t *w);
void amdws_close(amdws_t *w);
int amdws_bo_new(amdws_t *w, amdbo_t *b, amd_u64 size, unsigned flags);
void amdws_bo_free(amdws_t *w, amdbo_t *b);

int amdcs_init(amdws_t *w, amdcs_t *cs, int ring, amd_u32 cap_dw);
void amdcs_free(amdws_t *w, amdcs_t *cs);
void amdcs_reset(amdcs_t *cs);
void amdcs_use(amdcs_t *cs, const amdbo_t *b);          /* o lote lê ou escreve este BO */

/* pacotes */
#define AMD_PKT3(op, n) (0xC0000000u | ((amd_u32)(op) & 0xFFu) << 8 | ((amd_u32)(n) & 0x3FFFu) << 16)
void amdcs_dw(amdcs_t *cs, amd_u32 v);
void amdcs_pkt3(amdcs_t *cs, amd_u32 op, amd_u32 body_dw);    /* cabeçalho; seguem body_dw dwords */
void amdcs_set_sh(amdcs_t *cs, amd_u32 reg_byte, const amd_u32 *v, amd_u32 n);        /* 0xB000.. */
void amdcs_set_ctx(amdcs_t *cs, amd_u32 reg_byte, const amd_u32 *v, amd_u32 n);       /* 0x28000.. */
void amdcs_set_uconfig(amdcs_t *cs, amd_u32 reg_byte, const amd_u32 *v, amd_u32 n);   /* 0x30000.. (>= 0x30804) */
void amdcs_write_data(amdcs_t *cs, amd_u64 va, const amd_u32 *v, amd_u32 n);
void amdcs_acquire_all(amdcs_t *cs);                    /* invalida K$, I$, L1 e L2 antes do trabalho */
/* RELEASE_MEM no fim do pipe: grava value (32 bits) em va quando tudo antes terminou */
void amdcs_release_mem(amdcs_t *cs, int compute, amd_u64 va, amd_u32 value);
void amdcs_wait_mem(amdcs_t *cs, amd_u64 va, amd_u32 ref, amd_u32 mask);   /* espera *va & mask == ref */

/* compute GFX9 */
typedef struct {
    amd_u64 code_va;           /* alinhado a 256 bytes, BO com MOS_GPU_BO_EXEC */
    amd_u32 rsrc1, rsrc2;      /* COMPUTE_PGM_RSRC1/2 */
    amd_u32 user[16]; int nuser;   /* COMPUTE_USER_DATA_0.. (SGPRs de entrada) */
    amd_u32 local[3];          /* threads por grupo */
    amd_u32 groups[3];
    amd_u32 lds_bytes;
} amd_dispatch_t;
void amdcs_dispatch(amdcs_t *cs, const amd_dispatch_t *d);

/* envia o lote; fence 0 = sem fence. Devolve 0 ou -errno */
int amdws_submit(amdws_t *w, amdcs_t *cs, unsigned fence);
int amdws_fence_new(unsigned *fence);
int amdws_fence_wait(unsigned fence, unsigned timeout_ms);
void amdws_fence_free(unsigned fence);

/* registradores GFX9 (endereço em bytes) */
enum {
    AMD_COMPUTE_DISPATCH_INITIATOR = 0xB800, AMD_COMPUTE_START_X = 0xB810, AMD_COMPUTE_NUM_THREAD_X = 0xB81C,
    AMD_COMPUTE_PGM_LO = 0xB830, AMD_COMPUTE_PGM_RSRC1 = 0xB848, AMD_COMPUTE_PGM_RSRC2 = 0xB84C,
    AMD_COMPUTE_RESOURCE_LIMITS = 0xB854, AMD_COMPUTE_STATIC_THREAD_MGMT_SE0 = 0xB858, AMD_COMPUTE_TMPRING_SIZE = 0xB860,
    AMD_COMPUTE_STATIC_THREAD_MGMT_SE2 = 0xB864, AMD_COMPUTE_USER_DATA_0 = 0xB900,
};
