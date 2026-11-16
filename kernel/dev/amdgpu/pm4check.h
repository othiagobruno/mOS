/* mOS — validação dos IBs PM4 do userland (GFX9, ABI de GPU revisão 3). Ver pm4check.c. */
#pragma once
#include "amdgpu_base.h"

enum {
    PM4C_NOP = 0x10, PM4C_SET_BASE = 0x11, PM4C_CLEAR_STATE = 0x12, PM4C_INDEX_BUFFER_SIZE = 0x13,
    PM4C_DISPATCH_DIRECT = 0x15, PM4C_DISPATCH_INDIRECT = 0x16, PM4C_DRAW_INDIRECT = 0x24,
    PM4C_DRAW_INDEX_INDIRECT = 0x25, PM4C_INDEX_BASE = 0x26, PM4C_DRAW_INDEX_2 = 0x27,
    PM4C_CONTEXT_CONTROL = 0x28, PM4C_INDEX_TYPE = 0x2A, PM4C_DRAW_INDIRECT_MULTI = 0x2C,
    PM4C_DRAW_INDEX_AUTO = 0x2D, PM4C_NUM_INSTANCES = 0x2F, PM4C_DRAW_INDEX_MULTI_AUTO = 0x30,
    PM4C_DRAW_INDEX_OFFSET_2 = 0x35, PM4C_WRITE_DATA = 0x37, PM4C_DRAW_INDEX_INDIRECT_MULTI = 0x38,
    PM4C_WAIT_REG_MEM = 0x3C, PM4C_INDIRECT_BUFFER = 0x3F, PM4C_COPY_DATA = 0x40, PM4C_PFP_SYNC_ME = 0x42,
    PM4C_EVENT_WRITE = 0x46, PM4C_RELEASE_MEM = 0x49, PM4C_ACQUIRE_MEM = 0x58,
    PM4C_SET_CONTEXT_REG = 0x69, PM4C_SET_SH_REG = 0x76, PM4C_SET_UCONFIG_REG = 0x79,
};

enum { PM4E_OK = 0, PM4E_TRUNC = -1, PM4E_TYPE = -2, PM4E_OPCODE = -3, PM4E_REG = -4, PM4E_VA = -5,
       PM4E_SEL = -6, PM4E_INT = -7, PM4E_IB2 = -8, PM4E_EMPTY = -9 };

#define AMD_PM4_MAX_DW  (1u << 20)      /* tamanho máximo de um IB (campo de 20 bits) */
#define AMD_PM4_MAX_IB2 16u

typedef struct { u64 va, size; } amd_pm4_bo_t;
typedef struct {
    const amd_pm4_bo_t *bo;  u32 nbo;     /* BOs do lote (VA na VM do processo) */
    struct { u64 va; u32 ndw; } ib2[AMD_PM4_MAX_IB2];
    u32 nib2;                             /* IB2 achados: quem chama valida cada um com is_ib2 = true */
    u32 bad_dw;                           /* onde parou (para o log) */
} amd_pm4_ctx_t;

/* 0 = o IB pode ir para o anel; < 0 = PM4E_*. */
int         amd_pm4_check(amd_pm4_ctx_t *c, const u32 *ib, u32 ndw, bool is_ib2);
const char *amd_pm4_error(int e);
