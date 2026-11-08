/*
 * mOS — tradutor arm64: ponto flutuante escalar (frente FP).
 * ARM ARM C4.1.96 "Data Processing -- Scalar Floating-Point and Advanced SIMD", classes com
 * op0 = x0x1 (bit 28 = 1, bit 30 = 0): conversões FP↔fixo, FP↔inteiro, 1 fonte, compare,
 * imediato, compare condicional, 2 fontes, seleção condicional e 3 fontes. H/S/D (FP16 e BF16
 * anunciados). A semântica numérica fica toda em a64_fp.c. Escritas escalares em V zeram os bits
 * altos (exceto FMOV Vd.D[1], Xn). Codificação não alocada → A64_EXIT_UDF.
 */
#include "a64.h"
#include "a64_mem.h"

/* ftype → tamanho do elemento (0 = não alocado) */
static inline int ftype_es(unsigned ftype) {
    static const int t[4] = {4, 8, 0, 2};
    return t[ftype];
}

static inline a64_u64 vget(const a64_cpu *c, unsigned n, int es) {
    switch (es) {
    case 2: return c->v[n].h[0];
    case 4: return c->v[n].s[0];
    default: return c->v[n].d[0];
    }
}
static inline void vset(a64_cpu *c, unsigned d, a64_u64 val, int es) {
    c->v[d].q = 0;
    switch (es) {
    case 2: c->v[d].h[0] = (a64_u16)val; break;
    case 4: c->v[d].s[0] = (a64_u32)val; break;
    default: c->v[d].d[0] = val; break;
    }
}
static inline a64_u64 signbit_es(int es) { return 1ull << (es * 8 - 1); }

static int cond_holds(unsigned cond, a64_u32 nzcv) {
    int n = (nzcv >> 31) & 1, z = (nzcv >> 30) & 1, cy = (nzcv >> 29) & 1, v = (nzcv >> 28) & 1, r;
    switch (cond >> 1) {
    case 0: r = z; break;
    case 1: r = cy; break;
    case 2: r = n; break;
    case 3: r = v; break;
    case 4: r = cy && !z; break;
    case 5: r = n == v; break;
    case 6: r = n == v && !z; break;
    default: r = 1; break;
    }
    if ((cond & 1) && cond != 15) r = !r;
    return r;
}

/* conversões FP↔fixo: sf 0 S 11110 ftype 0 rmode opcode scale Rn Rd */
static int exec_fixed(a64_cpu *c, a64_u32 insn) {
    unsigned sf = insn >> 31, S = (insn >> 29) & 1, ftype = (insn >> 22) & 3;
    unsigned rmode = (insn >> 19) & 3, opcode = (insn >> 16) & 7, scale = (insn >> 10) & 63;
    unsigned rn = (insn >> 5) & 31, rd = insn & 31;
    int es = ftype_es(ftype), ib = sf ? 64 : 32;
    if (S || !es || (!sf && !(scale & 32))) return A64_EXIT_UDF;
    int fbits = 64 - (int)scale;
    switch ((rmode << 3) | opcode) {
    case 002: case 003: {                                  /* SCVTF / UCVTF (fixo) */
        a64_u64 v = sf ? a64_xr(c, rn) : a64_wr(c, rn);
        vset(c, rd, a64_int_to_fp(c, v, ib, opcode & 1, fbits, es, A64_RM_FPCR), es);
        return A64_NEXT;
    }
    case 030: case 031: {                                  /* FCVTZS / FCVTZU (fixo) */
        a64_u64 r = a64_fp_to_int(c, vget(c, rn, es), es, fbits, ib, opcode & 1, A64_RM_ZERO);
        if (sf) a64_xw(c, rd, r); else a64_ww(c, rd, (a64_u32)r);
        return A64_NEXT;
    }
    default:
        return A64_EXIT_UDF;
    }
}

/* conversões FP↔inteiro: sf 0 S 11110 ftype 1 rmode opcode 000000 Rn Rd */
static int exec_int(a64_cpu *c, a64_u32 insn) {
    unsigned sf = insn >> 31, S = (insn >> 29) & 1, ftype = (insn >> 22) & 3;
    unsigned rmode = (insn >> 19) & 3, opcode = (insn >> 16) & 7;
    unsigned rn = (insn >> 5) & 31, rd = insn & 31;
    int es = ftype_es(ftype), ib = sf ? 64 : 32;
    if (S) return A64_EXIT_UDF;
    if (opcode < 6) {
        int rm;
        if (!es) return A64_EXIT_UDF;
        if (opcode < 2) rm = (int)rmode;                     /* N P M Z = TIEEVEN POSINF NEGINF ZERO */
        else if (rmode) return A64_EXIT_UDF;
        else if (opcode < 4) {                               /* SCVTF / UCVTF */
            a64_u64 v = sf ? a64_xr(c, rn) : a64_wr(c, rn);
            vset(c, rd, a64_int_to_fp(c, v, ib, opcode & 1, 0, es, A64_RM_FPCR), es);
            return A64_NEXT;
        } else rm = A64_RM_TIEAWAY;                          /* FCVTAS / FCVTAU */
        a64_u64 r = a64_fp_to_int(c, vget(c, rn, es), es, 0, ib, opcode & 1, rm);
        if (sf) a64_xw(c, rd, r); else a64_ww(c, rd, (a64_u32)r);
        return A64_NEXT;
    }
    /* opcode 110 (FP → geral) / 111 (geral → FP) */
    int tofp = opcode & 1;
    switch (rmode) {
    case 0: {
        int fes;
        if (ftype == 3) fes = 2;
        else if (ftype == 0 && !sf) fes = 4;
        else if (ftype == 1 && sf) fes = 8;
        else return A64_EXIT_UDF;
        if (tofp) vset(c, rd, sf ? a64_xr(c, rn) : a64_wr(c, rn), fes);
        else {
            a64_u64 v = vget(c, rn, fes);
            if (sf) a64_xw(c, rd, v); else a64_ww(c, rd, (a64_u32)v);
        }
        return A64_NEXT;
    }
    case 1:                                                  /* FMOV Xd, Vn.D[1] / Vd.D[1], Xn */
        if (!sf || ftype != 2) return A64_EXIT_UDF;
        if (tofp) c->v[rd].d[1] = a64_xr(c, rn);
        else a64_xw(c, rd, c->v[rn].d[1]);
        return A64_NEXT;
    case 3:                                                  /* FJCVTZS Wd, Dn */
        if (sf || ftype != 1 || tofp) return A64_EXIT_UDF;
        {
            int z;
            a64_u32 r = a64_fp_jcvtzs(c, c->v[rn].d[0], &z);
            a64_ww(c, rd, r);
            c->nzcv = z ? A64_Z : 0;
        }
        return A64_NEXT;
    default:
        return A64_EXIT_UDF;
    }
}

/* 1 fonte: M 0 S 11110 ftype 1 opcode(6) 10000 Rn Rd */
static int exec_1src(a64_cpu *c, a64_u32 insn, int es) {
    unsigned opcode = (insn >> 15) & 63, ftype = (insn >> 22) & 3;
    unsigned rn = (insn >> 5) & 31, rd = insn & 31;
    a64_u64 a = vget(c, rn, es), r;
    switch (opcode) {
    case 0: r = a; break;                                    /* FMOV */
    case 1: r = a & ~signbit_es(es); break;                  /* FABS */
    case 2: r = a ^ signbit_es(es); break;                   /* FNEG */
    case 3: r = a64_fp_sqrt(c, a, es); break;                /* FSQRT */
    case 4: case 5: case 7: {                                /* FCVT */
        static const int to[4] = {4, 8, 0, 2};
        int tes = to[opcode & 3];
        if (tes == es) return A64_EXIT_UDF;
        vset(c, rd, a64_fp_convert(c, a, es, tes, A64_RM_FPCR), tes);
        return A64_NEXT;
    }
    case 6:                                                  /* BFCVT Hd, Sn (ftype = 01) */
        if (ftype != 1) return A64_EXIT_UDF;
        vset(c, rd, a64_fp_to_bf16(c, c->v[rn].s[0]), 2);
        return A64_NEXT;
    case 8:  r = a64_fp_roundint(c, a, es, A64_RM_TIEEVEN, 0); break;   /* FRINTN */
    case 9:  r = a64_fp_roundint(c, a, es, A64_RM_POSINF, 0); break;    /* FRINTP */
    case 10: r = a64_fp_roundint(c, a, es, A64_RM_NEGINF, 0); break;    /* FRINTM */
    case 11: r = a64_fp_roundint(c, a, es, A64_RM_ZERO, 0); break;      /* FRINTZ */
    case 12: r = a64_fp_roundint(c, a, es, A64_RM_TIEAWAY, 0); break;   /* FRINTA */
    case 14: r = a64_fp_roundint(c, a, es, A64_RM_FPCR, 1); break;      /* FRINTX */
    case 15: r = a64_fp_roundint(c, a, es, A64_RM_FPCR, 0); break;      /* FRINTI */
    case 16: case 17: case 18: case 19:                      /* FRINT32Z/32X/64Z/64X */
        if (es == 2) return A64_EXIT_UDF;
        r = a64_fp_roundint_n(c, a, es, (opcode & 1) ? A64_RM_FPCR : A64_RM_ZERO, (opcode & 2) ? 64 : 32);
        break;
    default:
        return A64_EXIT_UDF;
    }
    vset(c, rd, r, es);
    return A64_NEXT;
}

/* 2 fontes: M 0 S 11110 ftype 1 Rm opcode(4) 10 Rn Rd */
static int exec_2src(a64_cpu *c, a64_u32 insn, int es) {
    unsigned opcode = (insn >> 12) & 15, rm = (insn >> 16) & 31, rn = (insn >> 5) & 31, rd = insn & 31;
    a64_u64 a = vget(c, rn, es), b = vget(c, rm, es), r;
    switch (opcode) {
    case 0: r = a64_fp_mul(c, a, b, es); break;
    case 1: r = a64_fp_div(c, a, b, es); break;
    case 2: r = a64_fp_add(c, a, b, es); break;
    case 3: r = a64_fp_sub(c, a, b, es); break;
    case 4: r = a64_fp_max(c, a, b, es); break;
    case 5: r = a64_fp_min(c, a, b, es); break;
    case 6: r = a64_fp_maxnum(c, a, b, es); break;
    case 7: r = a64_fp_minnum(c, a, b, es); break;
    case 8: r = a64_fp_mul(c, a, b, es) ^ signbit_es(es); break;   /* FNMUL = FPNeg(FPMul) */
    default: return A64_EXIT_UDF;
    }
    vset(c, rd, r, es);
    return A64_NEXT;
}

int a64_exec_fp(a64_cpu *c, a64_u32 insn) {
    unsigned M = insn >> 31, S = (insn >> 29) & 1, ftype = (insn >> 22) & 3;
    int es = ftype_es(ftype);

    if (insn & (1u << 24)) {                                 /* 3 fontes */
        if (M || S || !es) return A64_EXIT_UDF;
        unsigned rm = (insn >> 16) & 31, ra = (insn >> 10) & 31, rn = (insn >> 5) & 31, rd = insn & 31;
        unsigned o1 = (insn >> 21) & 1, o0 = (insn >> 15) & 1;
        a64_u64 sb = signbit_es(es);
        a64_u64 va = vget(c, ra, es), vn = vget(c, rn, es), vm = vget(c, rm, es);
        if (o1) va ^= sb;                                    /* FNMADD / FNMSUB: -a */
        if (o0 != o1) vn ^= sb;                              /* FMSUB / FNMADD: -n */
        vset(c, rd, a64_fp_muladd(c, va, vn, vm, es), es);
        return A64_NEXT;
    }
    if (!(insn & (1u << 21)))                                /* FP ↔ fixo */
        return exec_fixed(c, insn);

    switch ((insn >> 10) & 3) {
    case 1: {                                                /* FCCMP / FCCMPE */
        if (M || S || !es) return A64_EXIT_UDF;
        unsigned rm = (insn >> 16) & 31, cond = (insn >> 12) & 15, rn = (insn >> 5) & 31;
        if (cond_holds(cond, c->nzcv))
            c->nzcv = a64_fp_compare(c, vget(c, rn, es), vget(c, rm, es), es, (insn >> 4) & 1);
        else
            c->nzcv = (insn & 15) << 28;
        return A64_NEXT;
    }
    case 2:
        if (M || S || !es) return A64_EXIT_UDF;
        return exec_2src(c, insn, es);
    case 3: {                                                /* FCSEL */
        if (M || S || !es) return A64_EXIT_UDF;
        unsigned rm = (insn >> 16) & 31, cond = (insn >> 12) & 15, rn = (insn >> 5) & 31, rd = insn & 31;
        vset(c, rd, vget(c, cond_holds(cond, c->nzcv) ? rn : rm, es), es);
        return A64_NEXT;
    }
    default:
        break;
    }
    if (((insn >> 10) & 7) == 4) {                           /* FMOV (imediato) */
        if (M || S || !es || ((insn >> 5) & 31)) return A64_EXIT_UDF;
        vset(c, insn & 31, a64_fp_expand_imm8((a64_u8)((insn >> 13) & 0xFF), es), es);
        return A64_NEXT;
    }
    if (((insn >> 10) & 15) == 8) {                          /* FCMP / FCMPE */
        if (M || S || !es || ((insn >> 14) & 3) || (insn & 7)) return A64_EXIT_UDF;
        unsigned rm = (insn >> 16) & 31, rn = (insn >> 5) & 31;
        a64_u64 b = (insn & 8) ? 0 : vget(c, rm, es);
        c->nzcv = a64_fp_compare(c, vget(c, rn, es), b, es, (insn >> 4) & 1);
        return A64_NEXT;
    }
    if (((insn >> 10) & 31) == 16) {                         /* 1 fonte */
        if (M || S || !es) return A64_EXIT_UDF;
        return exec_1src(c, insn, es);
    }
    if (((insn >> 10) & 63) == 0)                            /* FP ↔ inteiro */
        return exec_int(c, insn);
    return A64_EXIT_UDF;
}
