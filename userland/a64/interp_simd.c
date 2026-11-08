/*
 * mOS — tradutor arm64: AdvSIMD (frente SIMD). Decodificação dos grupos (ARM ARM C4.1.95/C4.1.96,
 * "Data Processing -- Scalar Floating-Point and Advanced SIMD") e loads/stores com V=1 (C4.1.94).
 * Os grupos ficam em interp_simd_int.c (inteiros, permutações), interp_simd_fp.c (lanes FP,
 * usando as primitivas a64_fp_* da frente FP) e interp_simd_crypto.c (AES/SHA/PMULL/SHA3).
 */
#include "interp_simd.h"

/* ---------------------------------------------------------------------------------------------
 * Decodificação: a64_interp_exec manda para cá op0 (28:25) = x111 exceto bit28=1/bit30=0 (FP).
 * ------------------------------------------------------------------------------------------- */

static int simd_vector(a64_cpu *c, a64_u32 i) {     /* 0xx0 111x */
    if (SBT(i, 24)) {
        if (SBT(i, 10)) {
            if (SBT(i, 23)) return UDF;
            if (SB(i, 22, 19) == 0) return a64s_modimm(c, i);
            return a64s_shift(c, i, 0);
        }
        return a64s_indexed(c, i, 0);
    }
    if (SBT(i, 21)) {
        if (SBT(i, 10)) return a64s_three_same(c, i, 0);
        if (!SBT(i, 11)) return a64s_three_diff(c, i, 0);
        switch (SB(i, 20, 17)) {
        case 0x0: return a64s_misc(c, i, 0);
        case 0x8: return a64s_across(c, i);
        case 0xC: return SBT(i, 22) ? a64s_fpmisc_h(c, i, 0) : UDF;
        case 0x4: return (SB(i, 31, 29) == 2 && SB(i, 23, 22) == 0) ? a64s_aes(c, i) : UDF;
        default: return UDF;
        }
    }
    if (SBT(i, 10)) {
        if (SBT(i, 15)) return a64s_three_ext(c, i, 0);
        if (SB(i, 23, 21) == 0) return a64s_copy(c, i, 0);
        if (SBT(i, 22) && !SBT(i, 14)) return a64s_fp3_h(c, i, 0);
        return UDF;
    }
    if (SBT(i, 15)) return UDF;
    if (!SBT(i, 29)) return SBT(i, 11) ? a64s_perm(c, i) : a64s_tbl(c, i);
    return a64s_ext(c, i);
}

static int simd_scalar(a64_cpu *c, a64_u32 i) {     /* 01x1 111x */
    if (SBT(i, 24)) {
        if (SBT(i, 10)) {
            if (SBT(i, 23) || SB(i, 22, 19) == 0) return UDF;
            return a64s_shift(c, i, 1);
        }
        return a64s_indexed(c, i, 1);
    }
    if (SBT(i, 21)) {
        if (SBT(i, 10)) return a64s_three_same(c, i, 1);
        if (!SBT(i, 11)) return a64s_three_diff(c, i, 1);
        switch (SB(i, 20, 17)) {
        case 0x0: return a64s_misc(c, i, 1);
        case 0x8: return a64s_pairwise(c, i);
        case 0xC: return SBT(i, 22) ? a64s_fpmisc_h(c, i, 1) : UDF;
        case 0x4: return (!SBT(i, 29) && SB(i, 23, 22) == 0) ? a64s_sha2reg(c, i) : UDF;
        default: return UDF;
        }
    }
    if (SBT(i, 10)) {
        if (SBT(i, 15)) return a64s_three_ext(c, i, 1);
        if (SB(i, 23, 21) == 0) return a64s_copy(c, i, 1);
        if (SBT(i, 22) && !SBT(i, 14)) return a64s_fp3_h(c, i, 1);
        return UDF;
    }
    if (!SBT(i, 29) && !SBT(i, 15) && !SBT(i, 11) && SB(i, 23, 22) == 0) return a64s_sha3reg(c, i);
    return UDF;
}

int a64_exec_simd(a64_cpu *c, a64_u32 i) {
    unsigned op0 = i >> 28;
    if (!(op0 & 1)) {
        if (op0 == 0xC) return a64s_crypto_c(c, i);
        if (op0 & 8) return UDF;
        return simd_vector(c, i);
    }
    if (!(op0 & 4) || (op0 & 8)) return UDF;       /* bit30=0 é da frente FP; 11x1 não alocado */
    return simd_scalar(c, i);
}

/* ---------------------------------------------------------------------------------------------
 * Loads/stores de SIMD&FP. Regra de reinício (a64.h): loads leem tudo para temporários antes de
 * escrever registradores; o writeback da base vem por último.
 * ------------------------------------------------------------------------------------------- */

static a64_u64 ld_elem(a64_u64 a, int eb) {
    switch (eb) { case 1: return a64_ld8(a); case 2: return a64_ld16(a); case 4: return a64_ld32(a); default: return a64_ld64(a); }
}
static void st_elem(a64_u64 a, a64_u64 v, int eb) {
    switch (eb) { case 1: a64_st8(a, (a64_u8)v); break; case 2: a64_st16(a, (a64_u16)v); break;
                  case 4: a64_st32(a, (a64_u32)v); break; default: a64_st64(a, v); break; }
}

/* LDR/STR de um registrador de 'sz' bytes (1,2,4,8,16) */
static void ld_reg(a64_u64 a, int sz, vr *r) {
    r->d[0] = 0; r->d[1] = 0;
    if (sz == 16) r->q = a64_ld128(a);
    else vset(r, 0, sz * 8, ld_elem(a, sz));
}
static void st_reg(a64_cpu *c, a64_u64 a, int sz, int t) {
    if (sz == 16) a64_st128(a, c->v[t].q);
    else st_elem(a, vel(&c->v[t], 0, sz * 8), sz);
}

static int ld_multi(a64_cpu *c, a64_u32 i) {
    int q = SBT(i, 30), L = SBT(i, 22), post = SBT(i, 23);
    int m = SB(i, 20, 16), opc = SB(i, 15, 12), size = SB(i, 11, 10), n = SB(i, 9, 5), t = SB(i, 4, 0);
    if (SBT(i, 21) || (!post && m != 0)) return UDF;
    int rpt, selem;
    switch (opc) {
    case 0x0: rpt = 1; selem = 4; break;
    case 0x2: rpt = 4; selem = 1; break;
    case 0x4: rpt = 1; selem = 3; break;
    case 0x6: rpt = 3; selem = 1; break;
    case 0x7: rpt = 1; selem = 1; break;
    case 0x8: rpt = 1; selem = 2; break;
    case 0xA: rpt = 2; selem = 1; break;
    default: return UDF;
    }
    if (size == 3 && !q && selem != 1) return UDF;
    int eb = 1 << size, bits = 8 << size, elements = (q ? 16 : 8) >> size;
    a64_u64 addr = c->x[n], offs = 0;
    vr tmp[4];
    for (int k = 0; k < 4; k++) { tmp[k].d[0] = 0; tmp[k].d[1] = 0; }
    for (int r = 0; r < rpt; r++)
        for (int e = 0; e < elements; e++)
            for (int s = 0; s < selem; s++) {
                int k = r + s;                       /* rpt == 1 ou selem == 1 */
                if (L) vset(&tmp[k], e, bits, ld_elem(addr + offs, eb));
                else st_elem(addr + offs, vel(&c->v[(t + k) & 31], e, bits), eb);
                offs += eb;
            }
    if (L)
        for (int k = 0; k < rpt * selem; k++) wq(c, (t + k) & 31, &tmp[k], q);
    if (post) c->x[n] = addr + (m == 31 ? offs : c->x[m]);
    return NEXT;
}

static int ld_single(a64_cpu *c, a64_u32 i) {
    int q = SBT(i, 30), L = SBT(i, 22), R = SBT(i, 21), post = SBT(i, 23);
    int m = SB(i, 20, 16), opcode = SB(i, 15, 13), S = SBT(i, 12), size = SB(i, 11, 10);
    int n = SB(i, 9, 5), t = SB(i, 4, 0);
    if (!post && m != 0) return UDF;
    int scale = opcode >> 1, selem = (((opcode & 1) << 1) | R) + 1, replicate = 0, index = 0;
    switch (scale) {
    case 3:
        if (!L || S) return UDF;
        scale = size; replicate = 1;
        break;
    case 0: index = (q << 3) | (S << 2) | size; break;
    case 1:
        if (size & 1) return UDF;
        index = (q << 2) | (S << 1) | (size >> 1);
        break;
    default:
        if (size & 2) return UDF;
        if (!(size & 1)) index = (q << 1) | S;
        else { if (S) return UDF; index = q; scale = 3; }
        break;
    }
    int eb = 1 << scale, bits = 8 << scale;
    a64_u64 addr = c->x[n], offs = 0, val[4];
    for (int s = 0; s < selem; s++) {
        if (L) val[s] = ld_elem(addr + offs, eb);
        else st_elem(addr + offs, vel(&c->v[(t + s) & 31], index, bits), eb);
        offs += eb;
    }
    if (L) {
        for (int s = 0; s < selem; s++) {
            int tt = (t + s) & 31;
            if (replicate) {
                vr r;
                for (int e = 0; e < 16 / eb; e++) vset(&r, e, bits, val[s]);
                wq(c, tt, &r, q);
            } else {
                vset(&c->v[tt], index, bits, val[s]);
            }
        }
    }
    if (post) c->x[n] = addr + (m == 31 ? offs : c->x[m]);
    return NEXT;
}

/* tamanho em bytes do registrador para size(31:30)/opc(23:22) com V=1; 0 = UNDEFINED */
static int reg_size(a64_u32 i, int *load) {
    int size = SB(i, 31, 30), opc = SB(i, 23, 22);
    *load = opc & 1;
    if (opc & 2) return size == 0 ? 16 : 0;
    return 1 << size;
}

static int ld_pair(a64_cpu *c, a64_u32 i) {
    int opc = SB(i, 31, 30), mode = SB(i, 24, 23), L = SBT(i, 22);
    int t2 = SB(i, 14, 10), n = SB(i, 9, 5), t = SB(i, 4, 0);
    if (opc == 3) return UDF;
    int sz = 4 << opc;
    a64_i64 off = ssx(SB(i, 21, 15), 7) * sz;
    int wback = mode == 1 || mode == 3, postidx = mode == 1;
    a64_u64 addr = c->x[n];
    if (!postidx) addr += off;
    if (L) {
        vr r1, r2;
        ld_reg(addr, sz, &r1);
        ld_reg(addr + sz, sz, &r2);
        c->v[t] = r1;
        c->v[t2] = r2;
    } else {
        st_reg(c, addr, sz, t);
        st_reg(c, addr + sz, sz, t2);
    }
    if (wback) c->x[n] = postidx ? addr + off : addr;
    return NEXT;
}

static int ldst_reg(a64_cpu *c, a64_u32 i) {
    int load, sz = reg_size(i, &load);
    int n = SB(i, 9, 5), t = SB(i, 4, 0);
    if (!sz) return UDF;
    a64_u64 addr;
    int wback = 0, postidx = 0;
    a64_i64 off = 0;
    if (SBT(i, 24)) {                                   /* offset sem sinal escalado */
        addr = c->x[n] + (a64_u64)SB(i, 21, 10) * sz;
    } else if (!SBT(i, 21)) {
        off = ssx(SB(i, 20, 12), 9);
        switch (SB(i, 11, 10)) {
        case 0: addr = c->x[n] + off; break;              /* LDUR/STUR */
        case 1: addr = c->x[n]; wback = postidx = 1; break;
        case 3: addr = c->x[n] + off; wback = 1; break;
        default: return UDF;                              /* LDTR/STTR não têm forma SIMD */
        }
    } else {
        if (SB(i, 11, 10) != 2) return UDF;               /* atômicos/PAC: não há com V=1 */
        int m = SB(i, 20, 16), option = SB(i, 15, 13), S = SBT(i, 12);
        if (!(option & 2)) return UDF;
        a64_u64 rm = a64_xr(c, m);
        if (!(option & 1)) rm = (option & 4) ? (a64_u64)(a64_i64)(int32_t)rm : (a64_u32)rm;
        int shift = S ? __builtin_ctz(sz) : 0;
        addr = c->x[n] + (rm << shift);
    }
    if (load) {
        vr r;
        ld_reg(addr, sz, &r);
        c->v[t] = r;
    } else {
        st_reg(c, addr, sz, t);
    }
    if (wback) c->x[n] = postidx ? addr + off : addr;
    return NEXT;
}

static int ld_literal(a64_cpu *c, a64_u32 i) {
    int opc = SB(i, 31, 30), t = SB(i, 4, 0);
    if (opc == 3) return UDF;
    int sz = 4 << opc;
    a64_u64 addr = c->pc + (a64_u64)(ssx(SB(i, 23, 5), 19) * 4);
    vr r;
    ld_reg(addr, sz, &r);
    c->v[t] = r;
    return NEXT;
}

int a64_exec_ldst_simd(a64_cpu *c, a64_u32 i) {
    if (!SBT(i, 26) || !SBT(i, 27) || SBT(i, 25)) return UDF;
    switch (SB(i, 29, 28)) {
    case 0:
        if (SBT(i, 31)) return UDF;
        return SBT(i, 24) ? ld_single(c, i) : ld_multi(c, i);
    case 1:
        if (SBT(i, 24)) return UDF;                       /* LDAPUR/STLUR SIMD: FEAT_LRCPC3, não anunciado */
        return ld_literal(c, i);
    case 2:
        return ld_pair(c, i);
    default:
        return ldst_reg(c, i);
    }
}
