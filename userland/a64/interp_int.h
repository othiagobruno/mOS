/*
 * mOS — tradutor arm64: utilidades privadas da frente INT (interp_int*.c). Pseudocódigo do ARM ARM
 * (shared/functions): AddWithCarry, ConditionHolds, DecodeBitMasks, ExtendReg, ShiftReg.
 * Não faz parte do contrato (a64.h); outras frentes não devem incluir este arquivo.
 */
#pragma once
#include "a64.h"
#include "a64_mem.h"

#define BITS(x, hi, lo) (((x) >> (lo)) & ((1u << ((hi) - (lo) + 1)) - 1))
#define BIT(x, n) (((x) >> (n)) & 1u)

static inline a64_u64 a64i_ones(unsigned n) { return n >= 64 ? ~0ull : ((1ull << n) - 1); }
static inline a64_u64 a64i_sext(a64_u64 v, unsigned bits) { unsigned s = 64 - bits; return (a64_u64)((a64_i64)(v << s) >> s); }
static inline a64_u64 a64i_trunc(a64_u64 v, int sf) { return sf ? v : (a64_u32)v; }

static inline a64_u64 a64i_ror(a64_u64 x, unsigned r, unsigned w) {
    if (w == 32) { a64_u32 v = (a64_u32)x; r &= 31; return r ? (a64_u32)((v >> r) | (v << (32 - r))) : v; }
    r &= 63; return r ? (x >> r) | (x << (64 - r)) : x;
}

/* ConditionHolds(cond) sobre nzcv (bits 31..28) */
static inline int a64i_cond(a64_u32 nzcv, unsigned cond) {
    int n = BIT(nzcv, 31), z = BIT(nzcv, 30), cf = BIT(nzcv, 29), v = BIT(nzcv, 28), r;
    switch (cond >> 1) {
    case 0: r = z; break;
    case 1: r = cf; break;
    case 2: r = n; break;
    case 3: r = v; break;
    case 4: r = cf && !z; break;
    case 5: r = n == v; break;
    case 6: r = n == v && !z; break;
    default: r = 1; break;
    }
    if ((cond & 1) && cond != 15) r = !r;
    return r;
}

/* AddWithCarry(x, y, carry) no tamanho sf ? 64 : 32; *flags recebe NZCV (bits 31..28) */
static inline a64_u64 a64i_awc(a64_u64 x, a64_u64 y, unsigned cin, int sf, a64_u32 *flags) {
    a64_u64 r; a64_u32 f = 0;
    if (sf) {
        a64_u128 us = (a64_u128)x + y + cin;
        r = (a64_u64)us;
        __int128 ss = (__int128)(a64_i64)x + (a64_i64)y + cin;
        if (r >> 63) f |= A64_N;
        if (r == 0) f |= A64_Z;
        if ((us >> 64) != 0) f |= A64_C;
        if (ss != (__int128)(a64_i64)r) f |= A64_V;
    } else {
        a64_u64 us = (a64_u64)(a64_u32)x + (a64_u32)y + cin;
        a64_u32 r32 = (a64_u32)us;
        a64_i64 ss = (a64_i64)(int32_t)x + (int32_t)y + cin;
        if (r32 >> 31) f |= A64_N;
        if (r32 == 0) f |= A64_Z;
        if (us >> 32) f |= A64_C;
        if (ss != (a64_i64)(int32_t)r32) f |= A64_V;
        r = r32;
    }
    *flags = f;
    return r;
}

/* flags NZ de um resultado lógico (C = V = 0) */
static inline a64_u32 a64i_nz(a64_u64 r, int sf) {
    a64_u32 f = 0;
    if (sf ? (r >> 63) : ((r >> 31) & 1)) f |= A64_N;
    if ((sf ? r : (a64_u32)r) == 0) f |= A64_Z;
    return f;
}

/* DecodeBitMasks(N, imms, immr, immediate, M). Devolve 0 se reservado (UNDEFINED). */
static inline int a64i_dbm(unsigned N, unsigned imms, unsigned immr, int immediate, unsigned M,
                           a64_u64 *wmask, a64_u64 *tmask) {
    unsigned v = (N << 6) | (~imms & 0x3F);
    if (v == 0) return 0;
    int len = 31 - __builtin_clz(v);
    if (len < 1) return 0;
    unsigned esize = 1u << len;
    if (esize > M) return 0;
    unsigned levels = esize - 1;
    if (immediate && (imms & levels) == levels) return 0;
    unsigned S = imms & levels, R = immr & levels, d = (S - R) & levels;
    a64_u64 emask = a64i_ones(esize);
    a64_u64 welem = a64i_ones(S + 1), telem = a64i_ones(d + 1);
    if (R) welem = ((welem >> R) | (welem << (esize - R))) & emask;
    a64_u64 w = 0, t = 0;
    for (unsigned i = 0; i < M; i += esize) { w |= welem << i; t |= telem << i; }
    if (wmask) *wmask = w;
    if (tmask) *tmask = t;
    return 1;
}

/* ExtendReg: option 0..7 = UXTB UXTH UXTW UXTX SXTB SXTH SXTW SXTX, depois << shift (64 bits) */
static inline a64_u64 a64i_extend(a64_u64 v, unsigned option, unsigned shift) {
    unsigned len = 8u << (option & 3);
    a64_u64 e = (option & 4) ? a64i_sext(v & a64i_ones(len), len) : (v & a64i_ones(len));
    return e << shift;
}

/* ShiftReg: 0 LSL, 1 LSR, 2 ASR, 3 ROR; amt < tamanho */
static inline a64_u64 a64i_shift(a64_u64 v, unsigned type, unsigned amt, int sf) {
    if (!sf) {
        a64_u32 w = (a64_u32)v;
        switch (type) {
        case 0: return (a64_u32)(w << amt);
        case 1: return w >> amt;
        case 2: return (a64_u32)((int32_t)w >> amt);
        default: return a64i_ror(w, amt, 32);
        }
    }
    switch (type) {
    case 0: return v << amt;
    case 1: return v >> amt;
    case 2: return (a64_u64)((a64_i64)v >> amt);
    default: return a64i_ror(v, amt, 64);
    }
}

/* escrita de resultado de 32/64 bits: ZR ou SP no registrador 31 */
static inline void a64i_wr(a64_cpu *c, unsigned r, a64_u64 v, int sf) { if (r != 31) c->x[r] = sf ? v : (a64_u32)v; }
static inline void a64i_wsp(a64_cpu *c, unsigned r, a64_u64 v, int sf) { c->x[r] = sf ? v : (a64_u32)v; }
static inline a64_u64 a64i_rd(const a64_cpu *c, unsigned r) { return r == 31 ? 0 : c->x[r]; }
static inline a64_u64 a64i_rsp(const a64_cpu *c, unsigned r) { return c->x[r]; }

/* Configuração de EL0 que o interpretador reproduz (SCTLR_EL1 do XNU: SA0 = 1, nAA = 0) e as
 * simplificações do QEMU usadas SÓ para comparar com o oráculo (-DA64I_ORACLE_COMPAT):
 *   - o QEMU não implementa a verificação de alinhamento do SP (SCTLR.SA0);
 *   - o oráculo roda com nAA = 1 (acquire/release desalinhados não falham);
 *   - o QEMU só verifica o alinhamento do STXR* quando o monitor bate (o ARM verifica antes);
 *   - o QEMU só sonda o endereço em DC CVAP/CVADP (no ARM, DC CVAC/CVAU/CIVAC e IC IVAU também
 *     podem dar falha de tradução). */
#ifdef A64I_ORACLE_COMPAT
#define A64I_DEF 0
#else
#define A64I_DEF 1
#endif
#ifndef A64I_SA0
#define A64I_SA0 A64I_DEF
#endif
#ifndef A64I_NAA
#define A64I_NAA (!A64I_DEF)
#endif
#ifndef A64I_STXR_ALIGN_FIRST
#define A64I_STXR_ALIGN_FIRST A64I_DEF
#endif
#ifndef A64I_DC_PROBE_ALL
#define A64I_DC_PROBE_ALL A64I_DEF
#endif
