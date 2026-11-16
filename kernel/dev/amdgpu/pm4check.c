/* mOS — validação dos IBs PM4 (GFX9) que o userland manda pelo SYS_GPU (ABI revisão 3, docs/gpu-abi.md).
 * Lógica pura: não toca no hardware. Testada em tools/amdgpu_pm4_host.c.
 *
 * O IB roda no VMID do processo, então a VM já impede que ele leia ou escreva memória de outro processo. A
 * validação cuida do que a VM não cobre: escrita em registrador (só as faixas de contexto, de shader e a parte
 * de usuário do UCONFIG), pacotes privilegiados (que mexem em filas, VMIDs, memória do kernel ou interrompem) e
 * endereços diretos fora dos BOs do lote (a regra da ABI). Endereços relativos a uma base (DRAW_INDIRECT,
 * DISPATCH_INDIRECT) valem só dentro da VM do processo, como no amdgpu. */
#include "pm4check.h"

#define T3_OP(h)    (((h) >> 8) & 0xFFu)
#define T3_N(h)     (((h) >> 16) & 0x3FFFu)

static bool va_ok(const amd_pm4_ctx_t *c, u64 va, u64 bytes) {
    if (!bytes || va + bytes < va) return false;
    for (u32 i = 0; i < c->nbo; i++)
        if (va >= c->bo[i].va && va + bytes <= c->bo[i].va + c->bo[i].size) return true;
    return false;
}

/* faixas de registrador que o processo pode escrever (deslocamentos em dwords a partir do início de cada
 * janela). UCONFIG: só a partir de 0x201 (o 0x200 é o GRBM_GFX_INDEX; abaixo dele ficam os SCRATCH e os
 * registradores de coerência do CP). */
static bool reg_range_ok(u32 op, u32 off, u32 count) {
    if (!count || off + count < off) return false;
    switch (op) {
    case PM4C_SET_CONTEXT_REG: return off + count <= 0x400;
    case PM4C_SET_SH_REG:      return off + count <= 0x400;
    case PM4C_SET_UCONFIG_REG: return off >= 0x201 && off + count <= 0x400;
    default: return false;
    }
}

const char *amd_pm4_error(int e) {
    switch (e) {
    case PM4E_OK: return "ok";
    case PM4E_TRUNC: return "pacote passa do fim do IB";
    case PM4E_TYPE: return "pacote tipo 0/1 (escrita direta em registrador)";
    case PM4E_OPCODE: return "opcode não permitido";
    case PM4E_REG: return "registrador fora das faixas de usuário";
    case PM4E_VA: return "endereço fora dos BOs do lote";
    case PM4E_SEL: return "destino ou fonte que não é memória";
    case PM4E_INT: return "pedido de interrupção";
    case PM4E_IB2: return "INDIRECT_BUFFER inválido (aninhado, com VMID ou lista cheia)";
    case PM4E_EMPTY: return "IB vazio ou grande demais";
    default: return "erro desconhecido";
    }
}

int amd_pm4_check(amd_pm4_ctx_t *c, const u32 *ib, u32 ndw, bool is_ib2) {
    c->bad_dw = 0;
    if (!ndw || ndw > AMD_PM4_MAX_DW) return PM4E_EMPTY;
    for (u32 p = 0; p < ndw;) {
        u32 h = ib[p];
        c->bad_dw = p;
        u32 type = h >> 30;
        if (type == 2) { p++; continue; }                       /* preenchimento */
        if (type != 3) return PM4E_TYPE;
        u32 op = T3_OP(h), n = T3_N(h);
        if (op == PM4C_NOP && n == 0x3FFF) { p++; continue; }   /* NOP de um dword */
        u32 len = n + 2;
        if (len > ndw - p) return PM4E_TRUNC;
        const u32 *b = ib + p + 1;                               /* corpo: n + 1 dwords */
        switch (op) {
        case PM4C_NOP: case PM4C_CONTEXT_CONTROL: case PM4C_CLEAR_STATE:
        case PM4C_INDEX_BUFFER_SIZE: case PM4C_DISPATCH_DIRECT: case PM4C_DISPATCH_INDIRECT:
        case PM4C_DRAW_INDIRECT: case PM4C_DRAW_INDEX_INDIRECT: case PM4C_INDEX_TYPE:
        case PM4C_DRAW_INDIRECT_MULTI: case PM4C_DRAW_INDEX_AUTO: case PM4C_NUM_INSTANCES:
        case PM4C_DRAW_INDEX_MULTI_AUTO: case PM4C_DRAW_INDEX_OFFSET_2: case PM4C_DRAW_INDEX_INDIRECT_MULTI:
        case PM4C_ACQUIRE_MEM: case PM4C_PFP_SYNC_ME:
            break;
        case PM4C_SET_BASE: {                                    /* base de DRAW/DISPATCH indireto */
            if (n < 2) return PM4E_TRUNC;
            u32 idx = b[0] & 0xF;
            if (idx != 1 && idx != 2) return PM4E_OPCODE;       /* 3 = partição do CE: do kernel */
            u64 va = (u64)b[1] | (u64)(b[2] & 0xFFFF) << 32;
            if (va && !va_ok(c, va, 4)) return PM4E_VA;
            break;
        }
        case PM4C_INDEX_BASE: {
            if (n < 1) return PM4E_TRUNC;
            if (!va_ok(c, (u64)(b[0] & ~1u) | (u64)(b[1] & 0xFFFF) << 32, 2)) return PM4E_VA;
            break;
        }
        case PM4C_DRAW_INDEX_2: {                                /* max_size, base lo, base hi, count, initiator */
            if (n < 4) return PM4E_TRUNC;
            u64 va = (u64)b[1] | (u64)(b[2] & 0xFFFF) << 32;
            if (!va_ok(c, va, (u64)(b[0] ? b[0] : 1) * 2)) return PM4E_VA;
            break;
        }
        case PM4C_SET_CONTEXT_REG: case PM4C_SET_SH_REG: case PM4C_SET_UCONFIG_REG:
            if (!reg_range_ok(op, b[0] & 0xFFFFu, n)) return PM4E_REG;
            break;
        case PM4C_WRITE_DATA: {
            if (n < 3) return PM4E_TRUNC;
            u32 dst = (b[0] >> 8) & 0xF;
            if (dst != 2 && dst != 5) return PM4E_SEL;
            bool one = b[0] & (1u << 16);
            u64 va = (u64)b[1] | (u64)b[2] << 32;
            if (!va_ok(c, va, one ? 4 : 4ull * (n - 2))) return PM4E_VA;
            break;
        }
        case PM4C_COPY_DATA: {                                   /* ctl, src lo, src hi, dst lo, dst hi */
            if (n < 4) return PM4E_TRUNC;
            u32 src = b[0] & 0xF, dst = (b[0] >> 8) & 0xF, bytes = (b[0] & (1u << 16)) ? 8 : 4;
            if (dst != 2 && dst != 5) return PM4E_SEL;
            if (src == 1 || src == 2) { if (!va_ok(c, (u64)b[1] | (u64)b[2] << 32, bytes)) return PM4E_VA; }
            else if (src != 5 && src != 9) return PM4E_SEL;      /* dado imediato ou relógio da GPU */
            if (!va_ok(c, (u64)b[3] | (u64)b[4] << 32, bytes)) return PM4E_VA;
            break;
        }
        case PM4C_WAIT_REG_MEM: {                                /* ctl, addr lo, addr hi, ref, mask, intervalo */
            if (n < 5) return PM4E_TRUNC;
            if (!((b[0] >> 4) & 1) || ((b[0] >> 6) & 1)) return PM4E_SEL;   /* só memória, só esperar */
            if (!va_ok(c, (u64)(b[1] & ~3u) | (u64)(b[2] & 0xFFFF) << 32, 4)) return PM4E_VA;
            break;
        }
        case PM4C_EVENT_WRITE: {
            /* eventos com índice 1..3 gravam na memória (ZPASS_DONE, estatísticas): endereço no corpo */
            u32 idx = (b[0] >> 8) & 0xF;
            if (n >= 2 && idx >= 1 && idx <= 3 && !va_ok(c, (u64)(b[1] & ~7u) | (u64)(b[2] & 0xFFFF) << 32, 8)) return PM4E_VA;
            break;
        }
        case PM4C_RELEASE_MEM: {                                 /* ev, sel, addr lo, addr hi, data lo, data hi, ctx */
            if (n < 5) return PM4E_TRUNC;
            if ((b[1] >> 24) & 7) return PM4E_INT;               /* interrupção é do kernel (a fence do lote) */
            u32 data = (b[1] >> 29) & 7;
            if (data && !va_ok(c, (u64)(b[2] & ~3u) | (u64)(b[3] & 0xFFFF) << 32, data == 1 ? 4 : 8)) return PM4E_VA;
            break;
        }
        case PM4C_INDIRECT_BUFFER: {
            if (n != 2) return PM4E_TRUNC;
            u32 ctl = b[2], len = ctl & 0xFFFFFu;
            u64 va = (u64)(b[0] & ~3u) | (u64)(b[1] & 0xFFFF) << 32;
            if (is_ib2 || (ctl >> 24) || c->nib2 >= AMD_PM4_MAX_IB2 || !len || len > AMD_PM4_MAX_DW) return PM4E_IB2;
            if (!va_ok(c, va, 4ull * len)) return PM4E_VA;
            c->ib2[c->nib2].va = va;
            c->ib2[c->nib2].ndw = len;
            c->nib2++;
            break;
        }
        default:
            return PM4E_OPCODE;
        }
        p += len;
    }
    return PM4E_OK;
}
