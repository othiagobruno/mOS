/*
 * mOS — tradutor arm64: utilidades privadas da frente SIMD (interp_simd*.c).
 * Não faz parte do contrato (a64.h); outras frentes não devem incluir este arquivo.
 * Nomes seguem o pseudocódigo do ARM ARM (Elem, Vpart, SignedSatQ, UnsignedSatQ...).
 */
#pragma once
#include "a64.h"
#include "a64_mem.h"

#define SB(x, hi, lo) (((x) >> (lo)) & ((1u << ((hi) - (lo) + 1)) - 1))
#define SBT(x, n) (((x) >> (n)) & 1u)
#define UDF A64_EXIT_UDF
#define NEXT A64_NEXT

typedef a64_vreg vr;
typedef __int128 i128;
typedef unsigned __int128 u128;

static inline a64_u64 sm(int bits) { return bits >= 64 ? ~0ull : ((1ull << bits) - 1); }
static inline a64_i64 ssx(a64_u64 v, int bits) {
    if (bits >= 64) return (a64_i64)v;
    int s = 64 - bits;
    return (a64_i64)(v << s) >> s;
}
/* inteiro do elemento: com sinal ou sem */
static inline i128 shl128(i128 v, int s) { return (i128)((u128)v << s); }
static inline i128 sint(a64_u64 v, int bits, int uns) { return uns ? (i128)(v & sm(bits)) : (i128)ssx(v, bits); }

/* Elem[v, i, bits] */
static inline a64_u64 vel(const vr *v, int i, int bits) {
    switch (bits) {
    case 8: return v->b[i];
    case 16: return v->h[i];
    case 32: return v->s[i];
    default: return v->d[i];
    }
}
static inline void vset(vr *v, int i, int bits, a64_u64 x) {
    switch (bits) {
    case 8: v->b[i] = (a64_u8)x; break;
    case 16: v->h[i] = (a64_u16)x; break;
    case 32: v->s[i] = (a64_u32)x; break;
    default: v->d[i] = x; break;
    }
}
/* V[d] = r com datasize 64 (q=0: zera 127:64) ou 128 */
static inline void wq(a64_cpu *c, int d, const vr *r, int q) {
    c->v[d] = *r;
    if (!q) c->v[d].d[1] = 0;
}
/* escrita escalar de 'bits' bits: zera o resto do registrador */
static inline void wsc(a64_cpu *c, int d, a64_u64 x, int bits) {
    vr r;
    r.d[0] = 0; r.d[1] = 0;
    vset(&r, 0, bits, x);
    c->v[d] = r;
}
/* Vpart[d, 1] = r<63:0> (preserva 63:0 do destino); part 0 = escrita de 64 bits */
static inline void wpart(a64_cpu *c, int d, a64_u64 lo64, int part) {
    if (part) c->v[d].d[1] = lo64;
    else { c->v[d].d[0] = lo64; c->v[d].d[1] = 0; }
}

static inline void setqc(a64_cpu *c) { c->fpsr |= A64_FPSR_QC; }
static inline a64_u64 sats(a64_cpu *c, i128 v, int bits) {
    i128 mx = ((i128)1 << (bits - 1)) - 1, mn = -mx - 1;
    if (v > mx) { setqc(c); v = mx; }
    else if (v < mn) { setqc(c); v = mn; }
    return (a64_u64)v & sm(bits);
}
static inline a64_u64 satu(a64_cpu *c, i128 v, int bits) {
    i128 mx = ((i128)1 << bits) - 1;
    if (v > mx) { setqc(c); v = mx; }
    else if (v < 0) { setqc(c); v = 0; }
    return (a64_u64)v;
}

/* FPNeg/FPAbs (só bits; sem tratamento de NaN, como no ARM) — es em bytes */
static inline a64_u64 fneg(a64_u64 x, int es) { return x ^ (1ull << (es * 8 - 1)); }
static inline a64_u64 fabsb(a64_u64 x, int es) { return x & ~(1ull << (es * 8 - 1)); }

static inline a64_u64 ror64(a64_u64 x, unsigned r) { r &= 63; return r ? (x >> r) | (x << (64 - r)) : x; }
static inline a64_u32 ror32(a64_u32 x, unsigned r) { r &= 31; return r ? (x >> r) | (x << (32 - r)) : x; }
static inline a64_u32 rol32(a64_u32 x, unsigned r) { return ror32(x, 32 - (r & 31)); }

/* grupos (interp_simd_int.c) — 'sc' = forma escalar */
int a64s_three_same(a64_cpu *c, a64_u32 i, int sc);
int a64s_three_diff(a64_cpu *c, a64_u32 i, int sc);
int a64s_misc(a64_cpu *c, a64_u32 i, int sc);
int a64s_across(a64_cpu *c, a64_u32 i);
int a64s_pairwise(a64_cpu *c, a64_u32 i);
int a64s_shift(a64_cpu *c, a64_u32 i, int sc);
int a64s_indexed(a64_cpu *c, a64_u32 i, int sc);
int a64s_copy(a64_cpu *c, a64_u32 i, int sc);
int a64s_modimm(a64_cpu *c, a64_u32 i);
int a64s_perm(a64_cpu *c, a64_u32 i);
int a64s_ext(a64_cpu *c, a64_u32 i);
int a64s_tbl(a64_cpu *c, a64_u32 i);
int a64s_three_ext(a64_cpu *c, a64_u32 i, int sc);

/* ponto flutuante (interp_simd_fp.c) */
int a64s_fp3(a64_cpu *c, a64_u32 i, int sc, int u, int a, int opc3, int es, int fp16);
int a64s_fp3_h(a64_cpu *c, a64_u32 i, int sc);        /* three same FP16 */
int a64s_fpmisc(a64_cpu *c, a64_u32 i, int sc, int u, int a, int op, int es, int fp16);
int a64s_fpmisc_h(a64_cpu *c, a64_u32 i, int sc);     /* two-reg misc FP16 */
int a64s_fp_across(a64_cpu *c, a64_u32 i);
int a64s_fp_pairwise(a64_cpu *c, a64_u32 i);
int a64s_fp_indexed(a64_cpu *c, a64_u32 i, int sc);   /* FMLA/FMLS/FMUL/FMULX/FMLAL/FCMLA por elemento */
int a64s_fmlal_vec(a64_cpu *c, a64_u32 i, int sub, int part);
int a64s_fcmla_vec(a64_cpu *c, a64_u32 i);
int a64s_fcadd_vec(a64_cpu *c, a64_u32 i);
int a64s_bfdot(a64_cpu *c, a64_u32 i, int idx);       /* idx < 0: vetor */
int a64s_bfmmla(a64_cpu *c, a64_u32 i);
int a64s_bfmlal(a64_cpu *c, a64_u32 i, int idx);
int a64s_fp_fixcvt(a64_cpu *c, a64_u32 i, int sc);    /* SCVTF/UCVTF/FCVTZS/FCVTZU (#fbits) */

/* criptografia (interp_simd_crypto.c) */
int a64s_aes(a64_cpu *c, a64_u32 i);
int a64s_sha3reg(a64_cpu *c, a64_u32 i);
int a64s_sha2reg(a64_cpu *c, a64_u32 i);
int a64s_crypto_c(a64_cpu *c, a64_u32 i);              /* op0 = 1100: SHA512, SHA3 */
int a64s_pmull64(a64_cpu *c, a64_u32 i);
