/*
 * mOS — tradutor arm64: lanes de ponto flutuante do AdvSIMD (frente SIMD).
 * Toda aritmética FP IEEE passa pelas primitivas a64_fp_* da frente FP (a64.h). Exceções aqui,
 * por não terem primitiva: FMLAL/FMLSL (FPMulAddH: fontes half convertidas exatamente e
 * NaN processados como FPProcessNaNs3H antes da a64_fp_muladd de precisão simples) e a aritmética
 * "BFloat16 computation behaviors" de BFDOT/BFMMLA (BFMul/BFAdd/BFRound do ARM ARM: arredonda
 * para ímpar, denormais = 0, NaN padrão, sem flags) — implementadas em inteiros, bit-exatas.
 */
#include "interp_simd.h"

enum { F_NONE = 0, F_MAXNM, F_MINNM, F_MLA, F_MLS, F_ADD, F_SUB, F_MULX, F_MUL, F_DIV, F_ABD,
       F_CMEQ, F_CMGE, F_CMGT, F_ACGE, F_ACGT, F_MAX, F_MIN, F_RECPS, F_RSQRTS };
#define FPAIR 0x80

static a64_u64 fbin(a64_cpu *c, int op, a64_u64 x, a64_u64 y, a64_u64 acc, int es) {
    a64_u64 ones = sm(es * 8);
    switch (op) {
    case F_MAXNM: return a64_fp_maxnum(c, x, y, es);
    case F_MINNM: return a64_fp_minnum(c, x, y, es);
    case F_MLA: return a64_fp_muladd(c, acc, x, y, es);
    case F_MLS: return a64_fp_muladd(c, acc, fneg(x, es), y, es);
    case F_ADD: return a64_fp_add(c, x, y, es);
    case F_SUB: return a64_fp_sub(c, x, y, es);
    case F_MULX: return a64_fp_mulx(c, x, y, es);
    case F_MUL: return a64_fp_mul(c, x, y, es);
    case F_DIV: return a64_fp_div(c, x, y, es);
    case F_ABD: return a64_fp_abd(c, x, y, es);
    case F_CMEQ: return a64_fp_cmp_eq(c, x, y, es) ? ones : 0;
    case F_CMGE: return a64_fp_cmp_ge(c, x, y, es) ? ones : 0;
    case F_CMGT: return a64_fp_cmp_gt(c, x, y, es) ? ones : 0;
    case F_ACGE: return a64_fp_cmp_ge(c, fabsb(x, es), fabsb(y, es), es) ? ones : 0;
    case F_ACGT: return a64_fp_cmp_gt(c, fabsb(x, es), fabsb(y, es), es) ? ones : 0;
    case F_MAX: return a64_fp_max(c, x, y, es);
    case F_MIN: return a64_fp_min(c, x, y, es);
    case F_RECPS: return a64_fp_recps(c, x, y, es);
    default: return a64_fp_rsqrts(c, x, y, es);
    }
}

/* ---------------------------------------------------------------------------------------------
 * three same (FP32/FP64 em "three same", FP16 em "three same FP16"); mesma tabela [U][a][opc3]
 * ------------------------------------------------------------------------------------------- */

static const unsigned char fp3_tab[2][2][8] = {
    { { F_MAXNM, F_MLA, F_ADD, F_MULX, F_CMEQ, 0, F_MAX, F_RECPS },
      { F_MINNM, F_MLS, F_SUB, 0, 0, 0, F_MIN, F_RSQRTS } },
    { { F_MAXNM | FPAIR, 0, F_ADD | FPAIR, F_MUL, F_CMGE, F_ACGE, F_MAX | FPAIR, F_DIV },
      { F_MINNM | FPAIR, 0, F_ABD, 0, F_CMGT, F_ACGT, F_MIN | FPAIR, 0 } },
};

int a64s_fp3(a64_cpu *c, a64_u32 i, int sc, int u, int a, int opc3, int es, int fp16) {
    (void)fp16;
    int q = sc ? 0 : SBT(i, 30), m = SB(i, 20, 16), n = SB(i, 9, 5), d = SB(i, 4, 0);
    int op = fp3_tab[u][a][opc3], pair = op & FPAIR;
    op &= ~FPAIR;
    if (!op) return UDF;
    if (sc) {
        switch (op) {
        case F_MULX: case F_CMEQ: case F_RECPS: case F_RSQRTS: case F_ABD:
        case F_CMGE: case F_CMGT: case F_ACGE: case F_ACGT: break;
        default: return UDF;
        }
        if (pair) return UDF;
    } else if (es == 8 && !q) return UDF;
    int bits = es * 8, ne = sc ? 1 : (q ? 16 : 8) / es;
    vr x = c->v[n], y = c->v[m], z = c->v[d], r;
    r.d[0] = 0; r.d[1] = 0;
    if (pair) {
        vr cat[2] = { x, y };
        if (!q) cat[0].d[1] = y.d[0];
        int per = 16 / es;
        for (int e = 0; e < ne; e++) {
            int k0 = 2 * e, k1 = 2 * e + 1;
            vset(&r, e, bits, fbin(c, op, vel(&cat[k0 / per], k0 % per, bits), vel(&cat[k1 / per], k1 % per, bits), 0, es));
        }
    } else {
        for (int e = 0; e < ne; e++)
            vset(&r, e, bits, fbin(c, op, vel(&x, e, bits), vel(&y, e, bits), vel(&z, e, bits), es));
    }
    if (sc) wsc(c, d, r.d[0], bits);
    else wq(c, d, &r, q);
    return NEXT;
}

int a64s_fp3_h(a64_cpu *c, a64_u32 i, int sc) {
    return a64s_fp3(c, i, sc, SBT(i, 29), SBT(i, 23), SB(i, 13, 11), 2, 1);
}

/* ---------------------------------------------------------------------------------------------
 * two-register misc (FP), inclusive FCVTN/FCVTL/FCVTXN/BFCVTN e URECPE/URSQRTE
 * ------------------------------------------------------------------------------------------- */

int a64s_fpmisc(a64_cpu *c, a64_u32 i, int sc, int u, int a, int op, int es, int fp16) {
    int q = sc ? 0 : SBT(i, 30), part = SBT(i, 30), n = SB(i, 9, 5), d = SB(i, 4, 0);
    int key = (u << 6) | (a << 5) | op;
    vr x = c->v[n], r;
    r.d[0] = 0; r.d[1] = 0;
    switch (key) {
    case 0x16: {                                          /* FCVTN */
        if (sc || fp16) return UDF;
        int from = es, to = es / 2, ne = 16 / from;
        for (int e = 0; e < ne; e++) vset(&r, e, to * 8, a64_fp_convert(c, vel(&x, e, from * 8), from, to, A64_RM_FPCR));
        wpart(c, d, r.d[0], part);
        return NEXT;
    }
    case 0x17: {                                          /* FCVTL */
        if (sc || fp16) return UDF;
        int from = es / 2, to = es, ne = 8 / from, hi = part ? ne : 0;
        for (int e = 0; e < ne; e++) vset(&r, e, to * 8, a64_fp_convert(c, vel(&x, hi + e, from * 8), from, to, A64_RM_FPCR));
        wq(c, d, &r, 1);
        return NEXT;
    }
    case 0x56:                                            /* FCVTXN */
        if (fp16 || es != 8) return UDF;
        if (sc) { wsc(c, d, a64_fp_convert(c, x.d[0], 8, 4, A64_RM_ODD), 32); return NEXT; }
        for (int e = 0; e < 2; e++) r.s[e] = (a64_u32)a64_fp_convert(c, x.d[e], 8, 4, A64_RM_ODD);
        wpart(c, d, r.d[0], part);
        return NEXT;
    case 0x36:                                            /* BFCVTN */
        if (sc || fp16 || es != 4) return UDF;
        for (int e = 0; e < 4; e++) r.h[e] = a64_fp_to_bf16(c, x.s[e]);
        wpart(c, d, r.d[0], part);
        return NEXT;
    case 0x3C: case 0x7C:                                 /* URECPE URSQRTE */
        if (sc || fp16 || es != 4) return UDF;
        for (int e = 0; e < (q ? 4 : 2); e++) r.s[e] = u ? a64_rsqrte_u32(x.s[e]) : a64_recpe_u32(x.s[e]);
        wq(c, d, &r, q);
        return NEXT;
    }
    /* elemento a elemento: classe de validade 'v' = 1 só vetor, 2 vetor e escalar, 3 só escalar */
    int v, rm = 0, kind;                                  /* kind: 0 roundint 1 tointS 2 tointU 3 cvtf 4 cmp ... */
    switch (key) {
    case 0x18: v = 1; kind = 0; rm = A64_RM_TIEEVEN; break;   /* FRINTN */
    case 0x19: v = 1; kind = 0; rm = A64_RM_NEGINF; break;    /* FRINTM */
    case 0x38: v = 1; kind = 0; rm = A64_RM_POSINF; break;    /* FRINTP */
    case 0x39: v = 1; kind = 0; rm = A64_RM_ZERO; break;      /* FRINTZ */
    case 0x58: v = 1; kind = 0; rm = A64_RM_TIEAWAY; break;   /* FRINTA */
    case 0x59: v = 1; kind = 10; rm = A64_RM_FPCR; break;     /* FRINTX */
    case 0x79: v = 1; kind = 0; rm = A64_RM_FPCR; break;      /* FRINTI */
    case 0x1E: case 0x1F: case 0x5E: case 0x5F:               /* FRINT32Z FRINT64Z FRINT32X FRINT64X */
        if (fp16) return UDF;
        v = 1; kind = 11; rm = u ? A64_RM_FPCR : A64_RM_ZERO; break;
    case 0x1A: v = 2; kind = 1; rm = A64_RM_TIEEVEN; break;   /* FCVTNS */
    case 0x1B: v = 2; kind = 1; rm = A64_RM_NEGINF; break;    /* FCVTMS */
    case 0x1C: v = 2; kind = 1; rm = A64_RM_TIEAWAY; break;   /* FCVTAS */
    case 0x3A: v = 2; kind = 1; rm = A64_RM_POSINF; break;    /* FCVTPS */
    case 0x3B: v = 2; kind = 1; rm = A64_RM_ZERO; break;      /* FCVTZS */
    case 0x5A: v = 2; kind = 2; rm = A64_RM_TIEEVEN; break;   /* FCVTNU */
    case 0x5B: v = 2; kind = 2; rm = A64_RM_NEGINF; break;    /* FCVTMU */
    case 0x5C: v = 2; kind = 2; rm = A64_RM_TIEAWAY; break;   /* FCVTAU */
    case 0x7A: v = 2; kind = 2; rm = A64_RM_POSINF; break;    /* FCVTPU */
    case 0x7B: v = 2; kind = 2; rm = A64_RM_ZERO; break;      /* FCVTZU */
    case 0x1D: v = 2; kind = 3; break;                        /* SCVTF */
    case 0x5D: v = 2; kind = 4; break;                        /* UCVTF */
    case 0x2C: case 0x2D: case 0x2E: case 0x6C: case 0x6D:    /* FCMGT FCMEQ FCMLT FCMGE FCMLE #0 */
        v = 2; kind = 5; break;
    case 0x2F: v = 1; kind = 6; break;                        /* FABS */
    case 0x6F: v = 1; kind = 7; break;                        /* FNEG */
    case 0x7F: v = 1; kind = 8; break;                        /* FSQRT */
    case 0x3D: v = 2; kind = 9; break;                        /* FRECPE */
    case 0x7D: v = 2; kind = 12; break;                       /* FRSQRTE */
    case 0x3F: v = 3; kind = 13; break;                       /* FRECPX */
    default: return UDF;
    }
    if (sc ? v == 1 : v == 3) return UDF;
    if (!sc && es == 8 && !q) return UDF;
    int bits = es * 8, ne = sc ? 1 : (q ? 16 : 8) / es;
    a64_u64 ones = sm(bits);
    for (int e = 0; e < ne; e++) {
        a64_u64 t = vel(&x, e, bits), o;
        switch (kind) {
        case 0: o = a64_fp_roundint(c, t, es, rm, 0); break;
        case 10: o = a64_fp_roundint(c, t, es, rm, 1); break;
        case 11: o = a64_fp_roundint_n(c, t, es, rm, (op & 1) ? 64 : 32); break;
        case 1: o = a64_fp_to_int(c, t, es, 0, bits, 0, rm); break;
        case 2: o = a64_fp_to_int(c, t, es, 0, bits, 1, rm); break;
        case 3: o = a64_int_to_fp(c, t, bits, 0, 0, es, A64_RM_FPCR); break;
        case 4: o = a64_int_to_fp(c, t, bits, 1, 0, es, A64_RM_FPCR); break;
        case 5:
            switch (key) {
            case 0x2C: o = a64_fp_cmp_gt(c, t, 0, es); break;
            case 0x2D: o = a64_fp_cmp_eq(c, t, 0, es); break;
            case 0x2E: o = a64_fp_cmp_gt(c, 0, t, es); break;
            case 0x6C: o = a64_fp_cmp_ge(c, t, 0, es); break;
            default: o = a64_fp_cmp_ge(c, 0, t, es); break;
            }
            o = o ? ones : 0;
            break;
        case 6: o = fabsb(t, es); break;
        case 7: o = fneg(t, es); break;
        case 8: o = a64_fp_sqrt(c, t, es); break;
        case 9: o = a64_fp_recpe(c, t, es); break;
        case 12: o = a64_fp_rsqrte(c, t, es); break;
        default: o = a64_fp_recpx(c, t, es); break;
        }
        vset(&r, e, bits, o & ones);
    }
    if (sc) wsc(c, d, r.d[0], bits);
    else wq(c, d, &r, q);
    return NEXT;
}

int a64s_fpmisc_h(a64_cpu *c, a64_u32 i, int sc) {
    return a64s_fpmisc(c, i, sc, SBT(i, 29), SBT(i, 23), SB(i, 16, 12), 2, 1);
}

/* ---------------------------------------------------------------------------------------------
 * across lanes (FMAXNMV/FMINNMV/FMAXV/FMINV) e scalar pairwise FP
 * ------------------------------------------------------------------------------------------- */

int a64s_fp_across(a64_cpu *c, a64_u32 i) {
    int q = SBT(i, 30), u = SBT(i, 29), a = SBT(i, 23), sz = SBT(i, 22), op = SB(i, 16, 12);
    int n = SB(i, 9, 5), d = SB(i, 4, 0), es;
    if (!u) { if (sz) return UDF; es = 2; }
    else { if (sz || !q) return UDF; es = 4; }
    int k = op == 0x0C ? (a ? F_MINNM : F_MAXNM) : (a ? F_MIN : F_MAX);
    int bits = es * 8, ne = (q ? 16 : 8) / es;
    a64_u64 t[8];
    for (int e = 0; e < ne; e++) t[e] = vel(&c->v[n], e, bits);
    for (; ne > 1; ne /= 2)                               /* Reduce(): op(Reduce(baixo), Reduce(alto)) */
        for (int e = 0; e < ne / 2; e++) t[e] = fbin(c, k, t[2 * e], t[2 * e + 1], 0, es);
    wsc(c, d, t[0], bits);
    return NEXT;
}

int a64s_fp_pairwise(a64_cpu *c, a64_u32 i) {
    int u = SBT(i, 29), size = SB(i, 23, 22), op = SB(i, 16, 12), n = SB(i, 9, 5), d = SB(i, 4, 0), es, k;
    if (!u) { if (size & 1) return UDF; es = 2; }
    else es = (size & 1) ? 8 : 4;
    int a = size >> 1;
    switch (op) {
    case 0x0C: k = a ? F_MINNM : F_MAXNM; break;
    case 0x0D: if (a) return UDF; k = F_ADD; break;
    case 0x0F: k = a ? F_MIN : F_MAX; break;
    default: return UDF;
    }
    int bits = es * 8;
    wsc(c, d, fbin(c, k, vel(&c->v[n], 0, bits), vel(&c->v[n], 1, bits), 0, es), bits);
    return NEXT;
}

/* ---------------------------------------------------------------------------------------------
 * SCVTF/UCVTF/FCVTZS/FCVTZU (vetor e escalar, ponto fixo)
 * ------------------------------------------------------------------------------------------- */

int a64s_fp_fixcvt(a64_cpu *c, a64_u32 i, int sc) {
    int q = sc ? 0 : SBT(i, 30), u = SBT(i, 29), immh = SB(i, 22, 19), hb = (int)SB(i, 22, 16), op = SB(i, 15, 11);
    int n = SB(i, 9, 5), d = SB(i, 4, 0), eb;
    if (immh & 8) eb = 64;
    else if (immh & 4) eb = 32;
    else if (immh & 2) eb = 16;
    else return UDF;
    if (!sc && eb == 64 && !q) return UDF;
    int fbits = 2 * eb - hb, es = eb / 8, ne = sc ? 1 : (q ? 16 : 8) / es;
    vr x = c->v[n], r;
    r.d[0] = 0; r.d[1] = 0;
    for (int e = 0; e < ne; e++) {
        a64_u64 t = vel(&x, e, eb);
        vset(&r, e, eb, op == 0x1C ? a64_int_to_fp(c, t, eb, u, fbits, es, A64_RM_FPCR)
                                   : a64_fp_to_int(c, t, es, fbits, eb, u, A64_RM_ZERO));
    }
    if (sc) wsc(c, d, r.d[0], eb);
    else wq(c, d, &r, q);
    return NEXT;
}

/* ---------------------------------------------------------------------------------------------
 * FMLAL/FMLSL (FEAT_FHM): FPMulAddH(addend32, op1_16, op2_16)
 * ------------------------------------------------------------------------------------------- */

static int h_nan(a64_u16 h) { return ((h & 0x7C00) == 0x7C00 && (h & 0x3FF)) ? ((h & 0x200) ? 1 : 2) : 0; }
static int s_nan(a64_u32 s) { return ((s & 0x7F800000u) == 0x7F800000u && (s & 0x7FFFFF)) ? ((s & 0x400000) ? 1 : 2) : 0; }
static a64_u32 h_nan_to_s(a64_u16 h) { return ((a64_u32)(h >> 15) << 31) | 0x7FC00000u | ((a64_u32)(h & 0x1FF) << 13); }
static int h_zero(a64_u16 h, int fz16) { return (h & 0x7C00) == 0 && (!(h & 0x3FF) || fz16); }
static int h_inf(a64_u16 h) { return (h & 0x7FFF) == 0x7C00; }
static a64_u32 h_to_s_exact(a64_u16 h, int fz16) {      /* sem NaN; FZ16 zera denormais */
    a64_u32 s = (a64_u32)(h >> 15) << 31;
    int ex = (h >> 10) & 31, fr = h & 0x3FF;
    if (ex == 0) {
        if (!fr || fz16) return s;
        int e = -14;
        while (!(fr & 0x400)) { fr <<= 1; e--; }
        return s | ((a64_u32)(e + 127) << 23) | ((a64_u32)(fr & 0x3FF) << 13);
    }
    if (ex == 31) return s | 0x7F800000u;
    return s | ((a64_u32)(ex - 15 + 127) << 23) | ((a64_u32)fr << 13);
}

static a64_u32 fmuladdh(a64_cpu *c, a64_u32 acc, a64_u16 h1, a64_u16 h2) {
    int fz16 = (c->fpcr & A64_FPCR_FZ16) != 0, dn = (c->fpcr & A64_FPCR_DN) != 0;
    int ta = s_nan(acc), t1 = h_nan(h1), t2 = h_nan(h2);
    if (ta || t1 || t2) {                                 /* FPProcessNaNs3H(addend, op1, op2) */
        a64_u32 res;
        if ((c->fpcr & A64_FPCR_FZ) && !(acc & 0x7F800000u) && (acc & 0x7FFFFF))
            c->fpsr |= A64_FPSR_IDC;                      /* FPUnpack(addend) vem antes dos NaN */
        if (ta == 2) { res = acc | 0x400000u; c->fpsr |= A64_FPSR_IOC; }
        else if (t1 == 2) { res = h_nan_to_s(h1); c->fpsr |= A64_FPSR_IOC; }
        else if (t2 == 2) { res = h_nan_to_s(h2); c->fpsr |= A64_FPSR_IOC; }
        else if (ta) res = acc;
        else if (t1) res = h_nan_to_s(h1);
        else res = h_nan_to_s(h2);
        if (dn) res = 0x7FC00000u;
        if (ta == 1 && ((h_inf(h1) && h_zero(h2, fz16)) || (h_zero(h1, fz16) && h_inf(h2)))) {
            res = 0x7FC00000u;
            c->fpsr |= A64_FPSR_IOC;
        }
        return res;
    }
    return (a64_u32)a64_fp_muladd(c, acc, h_to_s_exact(h1, fz16), h_to_s_exact(h2, fz16), 4);
}

static int fmlal_core(a64_cpu *c, int d, int n, int m, int q, int sub, int part, int idx) {
    vr x = c->v[n], y = c->v[m], z = c->v[d], r;
    r.d[0] = 0; r.d[1] = 0;
    int ne = q ? 4 : 2, hi = part ? ne : 0;
    for (int e = 0; e < ne; e++) {
        a64_u16 a = x.h[hi + e], b = idx >= 0 ? y.h[idx] : y.h[hi + e];
        if (sub) a ^= 0x8000;
        r.s[e] = fmuladdh(c, z.s[e], a, b);
    }
    wq(c, d, &r, q);
    return NEXT;
}

int a64s_fmlal_vec(a64_cpu *c, a64_u32 i, int sub, int part) {
    return fmlal_core(c, SB(i, 4, 0), SB(i, 9, 5), SB(i, 20, 16), SBT(i, 30), sub, part, -1);
}

/* ---------------------------------------------------------------------------------------------
 * FCMLA / FCADD (FEAT_FCMA)
 * ------------------------------------------------------------------------------------------- */

static void fcmla_core(a64_cpu *c, int d, int n, const vr *mv, int q, int es, int rot, int idx) {
    vr x = c->v[n], z = c->v[d], r;
    r.d[0] = 0; r.d[1] = 0;
    int bits = es * 8, ne = (q ? 16 : 8) / es;
    for (int p = 0; p < ne / 2; p++) {
        int mi = idx >= 0 ? idx : p;
        a64_u64 m0 = vel(mv, 2 * mi, bits), m1 = vel(mv, 2 * mi + 1, bits);
        a64_u64 n0 = vel(&x, 2 * p, bits), n1 = vel(&x, 2 * p + 1, bits), e1, e2, e3, e4;
        switch (rot) {
        case 0: e1 = m0; e2 = n0; e3 = m1; e4 = n0; break;
        case 1: e1 = fneg(m1, es); e2 = n1; e3 = m0; e4 = n1; break;
        case 2: e1 = fneg(m0, es); e2 = n0; e3 = fneg(m1, es); e4 = n0; break;
        default: e1 = m1; e2 = n1; e3 = fneg(m0, es); e4 = n1; break;
        }
        vset(&r, 2 * p, bits, a64_fp_muladd(c, vel(&z, 2 * p, bits), e2, e1, es));
        vset(&r, 2 * p + 1, bits, a64_fp_muladd(c, vel(&z, 2 * p + 1, bits), e4, e3, es));
    }
    wq(c, d, &r, q);
}

int a64s_fcmla_vec(a64_cpu *c, a64_u32 i) {
    int q = SBT(i, 30), size = SB(i, 23, 22);
    if (size == 0 || (size == 3 && !q)) return UDF;
    vr mv = c->v[SB(i, 20, 16)];
    fcmla_core(c, SB(i, 4, 0), SB(i, 9, 5), &mv, q, 1 << size, SB(i, 12, 11), -1);
    return NEXT;
}

int a64s_fcadd_vec(a64_cpu *c, a64_u32 i) {
    int q = SBT(i, 30), size = SB(i, 23, 22), rot = SBT(i, 12), d = SB(i, 4, 0);
    if (size == 0 || (size == 3 && !q)) return UDF;
    int es = 1 << size, bits = es * 8, ne = (q ? 16 : 8) / es;
    vr x = c->v[SB(i, 9, 5)], y = c->v[SB(i, 20, 16)], r;
    r.d[0] = 0; r.d[1] = 0;
    for (int p = 0; p < ne / 2; p++) {
        a64_u64 m0 = vel(&y, 2 * p, bits), m1 = vel(&y, 2 * p + 1, bits), e1, e3;
        if (!rot) { e1 = fneg(m1, es); e3 = m0; }
        else { e1 = m1; e3 = fneg(m0, es); }
        vset(&r, 2 * p, bits, a64_fp_add(c, vel(&x, 2 * p, bits), e1, es));
        vset(&r, 2 * p + 1, bits, a64_fp_add(c, vel(&x, 2 * p + 1, bits), e3, es));
    }
    wq(c, d, &r, q);
    return NEXT;
}

/* ---------------------------------------------------------------------------------------------
 * FMLA/FMLS/FMUL/FMULX/FMLAL(2)/FMLSL(2)/FCMLA por elemento
 * ------------------------------------------------------------------------------------------- */

int a64s_fp_indexed(a64_cpu *c, a64_u32 i, int sc) {
    int q = sc ? 0 : SBT(i, 30), u = SBT(i, 29), size = SB(i, 23, 22), L = SBT(i, 21), M = SBT(i, 20);
    int rm = SB(i, 19, 16), op = SB(i, 15, 12), H = SBT(i, 11), n = SB(i, 9, 5), d = SB(i, 4, 0);
    if ((!u && (op == 0 || op == 4)) || (u && (op == 8 || op == 12))) {   /* FMLAL FMLSL FMLAL2 FMLSL2 */
        if (sc || size != 2) return UDF;
        return fmlal_core(c, d, n, rm, q, (op & 4) != 0, u, (H << 2) | (L << 1) | M);
    }
    if (u && (op & 1) && op < 8) {                        /* FCMLA (elemento) */
        int idx;
        if (sc) return UDF;
        if (size == 1) { if (!q && H) return UDF; idx = (H << 1) | L; }
        else if (size == 2) { if (L || !q) return UDF; idx = H; }
        else return UDF;
        vr mv = c->v[(M << 4) | rm];
        fcmla_core(c, d, n, &mv, q, 1 << size, (op >> 1) & 3, idx);
        return NEXT;
    }
    int es, idx, mreg;
    switch (size) {
    case 0: es = 2; idx = (H << 2) | (L << 1) | M; mreg = rm; break;
    case 2: es = 4; idx = (H << 1) | L; mreg = (M << 4) | rm; break;
    case 3:
        if (L || (!sc && !q)) return UDF;
        es = 8; idx = H; mreg = (M << 4) | rm; break;
    default: return UDF;
    }
    int k;
    if (!u) k = op == 1 ? F_MLA : op == 5 ? F_MLS : F_MUL;   /* op 1 5 9 */
    else k = F_MULX;                                          /* op 9 */
    int bits = es * 8, ne = sc ? 1 : (q ? 16 : 8) / es;
    a64_u64 y = vel(&c->v[mreg], idx, bits);
    vr x = c->v[n], z = c->v[d], r;
    r.d[0] = 0; r.d[1] = 0;
    for (int e = 0; e < ne; e++) vset(&r, e, bits, fbin(c, k, vel(&x, e, bits), y, vel(&z, e, bits), es));
    if (sc) wsc(c, d, r.d[0], bits);
    else wq(c, d, &r, q);
    return NEXT;
}

/* ---------------------------------------------------------------------------------------------
 * BF16 (FEAT_BF16). BFDOT/BFMMLA: BFDotAdd com "BFloat16 computation behaviors" (sem FPCR,
 * denormais = 0, arredonda para ímpar, NaN padrão, sem flags). BFMLALB/T: FPMulAdd normal.
 * ------------------------------------------------------------------------------------------- */

typedef struct { int t; int s; a64_u64 m; int e; } bfv;  /* t: 0 zero, 1 número, 2 inf, 3 NaN; valor = m·2^e */

static bfv bf_unpack(a64_u32 x) {
    bfv v;
    int ex = (x >> 23) & 0xFF;
    a64_u32 fr = x & 0x7FFFFF;
    v.s = x >> 31; v.m = 0; v.e = 0;
    if (ex == 0) v.t = 0;
    else if (ex == 255) v.t = fr ? 3 : 2;
    else { v.t = 1; v.m = (1u << 23) | fr; v.e = ex - 150; }
    return v;
}

static int msb128(u128 x) {
    a64_u64 hi = (a64_u64)(x >> 64), lo = (a64_u64)x;
    return hi ? 127 - __builtin_clzll(hi) : 63 - __builtin_clzll(lo);
}

/* BFRound(valor = mant·2^e, + resto não nulo se sticky) */
static a64_u32 bf_round(int s, u128 mant, int e, int sticky) {
    if (!mant) return 0;
    int p = msb128(mant), E = p + e;
    if (E < -126) return (a64_u32)s << 31;
    if (E > 127) return ((a64_u32)s << 31) | 0x7F800000u;
    a64_u64 im;
    if (p >= 23) {
        im = (a64_u64)(mant >> (p - 23));
        if (mant & (((u128)1 << (p - 23)) - 1)) sticky = 1;
    } else {
        im = (a64_u64)(mant << (23 - p));
    }
    if (sticky) im |= 1;
    return ((a64_u32)s << 31) | ((a64_u32)(E + 127) << 23) | ((a64_u32)im & 0x7FFFFF);
}

static a64_u32 bf_mul(a64_u32 a, a64_u32 b) {
    bfv x = bf_unpack(a), y = bf_unpack(b);
    if (x.t == 3 || y.t == 3) return 0x7FC00000u;
    if ((x.t == 2 && y.t == 0) || (x.t == 0 && y.t == 2)) return 0x7FC00000u;
    int s = x.s ^ y.s;
    if (x.t == 2 || y.t == 2) return ((a64_u32)s << 31) | 0x7F800000u;
    if (x.t == 0 || y.t == 0) return (a64_u32)s << 31;
    return bf_round(s, (u128)x.m * y.m, x.e + y.e, 0);
}

static a64_u32 bf_add(a64_u32 a, a64_u32 b) {
    bfv x = bf_unpack(a), y = bf_unpack(b);
    if (x.t == 3 || y.t == 3) return 0x7FC00000u;
    if (x.t == 2 && y.t == 2 && x.s != y.s) return 0x7FC00000u;
    if ((x.t == 2 && !x.s) || (y.t == 2 && !y.s)) return 0x7F800000u;
    if (x.t == 2 || y.t == 2) return 0xFF800000u;
    if (x.t == 0 && y.t == 0) return x.s == y.s ? (a64_u32)x.s << 31 : 0;
    if (x.t == 0) return bf_round(y.s, y.m, y.e, 0);
    if (y.t == 0) return bf_round(x.s, x.m, x.e, 0);
    /* big = maior magnitude */
    bfv big = x, sml = y;
    if (y.e > x.e || (y.e == x.e && y.m > x.m)) { big = y; sml = x; }
    int dd = big.e - sml.e, sticky = 0;
    u128 X = (u128)big.m << 64, Y;
    if (dd >= 100) { Y = 0; sticky = 1; }
    else {
        Y = ((u128)sml.m << 64) >> dd;
        if ((((u128)sml.m << 64) & (((u128)1 << dd) - 1)) != 0) sticky = 1;
    }
    u128 R;
    if (big.s == sml.s) R = X + Y;
    else R = X - Y - (u128)sticky;
    if (!R && !sticky) return 0;                          /* soma exata zero → +0 */
    return bf_round(big.s, R, big.e - 64, sticky);
}

static a64_u32 bf_dotadd(a64_u32 acc, a64_u16 n0, a64_u16 n1, a64_u16 m0, a64_u16 m1) {
    a64_u32 p = bf_add(bf_mul((a64_u32)n0 << 16, (a64_u32)m0 << 16), bf_mul((a64_u32)n1 << 16, (a64_u32)m1 << 16));
    return bf_add(acc, p);
}

int a64s_bfdot(a64_cpu *c, a64_u32 i, int idx) {
    int q = SBT(i, 30), d = SB(i, 4, 0);
    vr x = c->v[SB(i, 9, 5)], y = c->v[SB(i, 20, 16)], r = c->v[d];
    for (int e = 0; e < (q ? 4 : 2); e++) {
        int k = idx >= 0 ? idx : e;
        r.s[e] = bf_dotadd(r.s[e], x.h[2 * e], x.h[2 * e + 1], y.h[2 * k], y.h[2 * k + 1]);
    }
    wq(c, d, &r, q);
    return NEXT;
}

int a64s_bfmmla(a64_cpu *c, a64_u32 i) {
    int d = SB(i, 4, 0);
    vr x = c->v[SB(i, 9, 5)], y = c->v[SB(i, 20, 16)], r = c->v[d];
    for (int a = 0; a < 2; a++)
        for (int b = 0; b < 2; b++) {
            a64_u32 s = r.s[2 * a + b];
            for (int k = 0; k < 2; k++)
                s = bf_dotadd(s, x.h[4 * a + 2 * k], x.h[4 * a + 2 * k + 1], y.h[4 * b + 2 * k], y.h[4 * b + 2 * k + 1]);
            r.s[2 * a + b] = s;
        }
    c->v[d] = r;
    return NEXT;
}

int a64s_bfmlal(a64_cpu *c, a64_u32 i, int idx) {
    int sel = SBT(i, 30), d = SB(i, 4, 0);
    int m = idx >= 0 ? (int)SB(i, 19, 16) : (int)SB(i, 20, 16);
    vr x = c->v[SB(i, 9, 5)], y = c->v[m], r = c->v[d];
    for (int e = 0; e < 4; e++) {
        a64_u32 a = (a64_u32)x.h[2 * e + sel] << 16;
        a64_u32 b = (a64_u32)(idx >= 0 ? y.h[idx] : y.h[2 * e + sel]) << 16;
        r.s[e] = (a64_u32)a64_fp_muladd(c, r.s[e], a, b, 4);
    }
    c->v[d] = r;
    return NEXT;
}
