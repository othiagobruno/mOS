/*
 * mOS — tradutor arm64: inteiros (frente INT) — processamento de dados com imediato e com
 * registrador (ARM ARM C4.1.86 / C4.1.88). Desvios/sistema: interp_int_br.c; loads/stores:
 * interp_int_ls.c. Decodificação fiel ao ARM ARM; onde o ARM deixa CONSTRAINED UNPREDICTABLE
 * seguimos o QEMU (oráculo) e documentamos.
 */
#include "interp_int.h"

/* ============================== dados com imediato (100x) ============================== */

static int pcrel(a64_cpu *c, a64_u32 i) {
    unsigned rd = BITS(i, 4, 0);
    a64_u64 imm = a64i_sext(((a64_u64)BITS(i, 23, 5) << 2) | BITS(i, 30, 29), 21);
    if (BIT(i, 31)) a64_xw(c, rd, (c->pc & ~0xFFFull) + (imm << 12));   /* ADRP */
    else a64_xw(c, rd, c->pc + imm);                                      /* ADR */
    return A64_NEXT;
}

static int addsub_imm(a64_cpu *c, a64_u32 i) {
    int sf = BIT(i, 31), op = BIT(i, 30), S = BIT(i, 29);
    unsigned rn = BITS(i, 9, 5), rd = BITS(i, 4, 0);
    a64_u64 imm = BITS(i, 21, 10);
    if (BIT(i, 22)) imm <<= 12;
    a64_u64 x = a64i_trunc(a64i_rsp(c, rn), sf);
    a64_u32 f;
    a64_u64 r = op ? a64i_awc(x, ~imm, 1, sf, &f) : a64i_awc(x, imm, 0, sf, &f);
    if (S) { c->nzcv = f; a64i_wr(c, rd, r, sf); }
    else a64i_wsp(c, rd, r, sf);
    return A64_NEXT;
}

static int logic_imm(a64_cpu *c, a64_u32 i) {
    int sf = BIT(i, 31);
    unsigned opc = BITS(i, 30, 29), N = BIT(i, 22), rn = BITS(i, 9, 5), rd = BITS(i, 4, 0);
    if (!sf && N) return A64_EXIT_UDF;
    a64_u64 imm;
    if (!a64i_dbm(N, BITS(i, 15, 10), BITS(i, 21, 16), 1, sf ? 64 : 32, &imm, NULL)) return A64_EXIT_UDF;
    a64_u64 x = a64i_rd(c, rn), r;
    switch (opc) {
    case 0: r = x & imm; break;
    case 1: r = x | imm; break;
    case 2: r = x ^ imm; break;
    default:
        r = a64i_trunc(x & imm, sf);
        c->nzcv = a64i_nz(r, sf);
        a64i_wr(c, rd, r, sf);
        return A64_NEXT;
    }
    a64i_wsp(c, rd, r, sf);
    return A64_NEXT;
}

static int movewide(a64_cpu *c, a64_u32 i) {
    int sf = BIT(i, 31);
    unsigned opc = BITS(i, 30, 29), hw = BITS(i, 22, 21), rd = BITS(i, 4, 0);
    if (opc == 1 || (!sf && (hw & 2))) return A64_EXIT_UDF;
    unsigned pos = hw * 16;
    a64_u64 imm = (a64_u64)BITS(i, 20, 5) << pos, r;
    switch (opc) {
    case 0: r = ~imm; break;                                        /* MOVN */
    case 2: r = imm; break;                                         /* MOVZ */
    default: r = (a64i_rd(c, rd) & ~(0xFFFFull << pos)) | imm; break; /* MOVK */
    }
    a64i_wr(c, rd, r, sf);
    return A64_NEXT;
}

static int bitfield(a64_cpu *c, a64_u32 i) {
    int sf = BIT(i, 31);
    unsigned opc = BITS(i, 30, 29), N = BIT(i, 22), immr = BITS(i, 21, 16), imms = BITS(i, 15, 10);
    unsigned rn = BITS(i, 9, 5), rd = BITS(i, 4, 0), M = sf ? 64 : 32;
    if (opc == 3) return A64_EXIT_UDF;
    if (sf && !N) return A64_EXIT_UDF;
    if (!sf && (N || (immr & 32) || (imms & 32))) return A64_EXIT_UDF;
    a64_u64 wmask, tmask;
    if (!a64i_dbm(N, imms, immr, 0, M, &wmask, &tmask)) return A64_EXIT_UDF;
    a64_u64 src = a64i_trunc(a64i_rd(c, rn), sf), r;
    a64_u64 bot = a64i_ror(src, immr, M) & wmask;
    switch (opc) {
    case 0: { /* SBFM */
        a64_u64 top = ((src >> imms) & 1) ? a64i_ones(M) : 0;
        r = (top & ~tmask) | (bot & tmask);
        break;
    }
    case 1: { /* BFM */
        a64_u64 dst = a64i_rd(c, rd);
        bot = (dst & ~wmask) | (a64i_ror(src, immr, M) & wmask);
        r = (dst & ~tmask) | (bot & tmask);
        break;
    }
    default: r = bot & tmask; break; /* UBFM */
    }
    a64i_wr(c, rd, r, sf);
    return A64_NEXT;
}

static int extract(a64_cpu *c, a64_u32 i) {
    int sf = BIT(i, 31);
    unsigned N = BIT(i, 22), rm = BITS(i, 20, 16), lsb = BITS(i, 15, 10), rn = BITS(i, 9, 5), rd = BITS(i, 4, 0);
    if (BITS(i, 30, 29) != 0 || BIT(i, 21) || N != (unsigned)sf) return A64_EXIT_UDF;
    if (!sf && (lsb & 32)) return A64_EXIT_UDF;
    a64_u64 hi = a64i_rd(c, rn), lo = a64i_rd(c, rm), r;
    if (sf) r = lsb ? (lo >> lsb) | (hi << (64 - lsb)) : lo;
    else r = (a64_u32)(((((a64_u64)(a64_u32)hi) << 32) | (a64_u32)lo) >> lsb);
    a64i_wr(c, rd, r, sf);
    return A64_NEXT;
}

int a64_exec_dpimm(a64_cpu *c, a64_u32 insn) {
    switch (BITS(insn, 25, 23)) {
    case 0: case 1: return pcrel(c, insn);
    case 2: return addsub_imm(c, insn);
    case 3: return A64_EXIT_UDF;          /* ADDG/SUBG (MTE), min/max imediato (CSSC): não anunciados */
    case 4: return logic_imm(c, insn);
    case 5: return movewide(c, insn);
    case 6: return bitfield(c, insn);
    default: return extract(c, insn);
    }
}

/* ============================ dados com registrador (x101) ============================ */

static int logic_shifted(a64_cpu *c, a64_u32 i) {
    int sf = BIT(i, 31);
    unsigned opc = BITS(i, 30, 29), type = BITS(i, 23, 22), N = BIT(i, 21);
    unsigned rm = BITS(i, 20, 16), amt = BITS(i, 15, 10), rn = BITS(i, 9, 5), rd = BITS(i, 4, 0);
    if (!sf && (amt & 32)) return A64_EXIT_UDF;
    a64_u64 op2 = a64i_shift(a64i_rd(c, rm), type, amt, sf);
    if (N) op2 = ~op2;
    a64_u64 x = a64i_rd(c, rn), r;
    switch (opc) {
    case 0: case 3: r = x & op2; break;
    case 1: r = x | op2; break;
    default: r = x ^ op2; break;
    }
    r = a64i_trunc(r, sf);
    if (opc == 3) c->nzcv = a64i_nz(r, sf);
    a64i_wr(c, rd, r, sf);
    return A64_NEXT;
}

static int addsub_shifted(a64_cpu *c, a64_u32 i) {
    int sf = BIT(i, 31), op = BIT(i, 30), S = BIT(i, 29);
    unsigned type = BITS(i, 23, 22), rm = BITS(i, 20, 16), amt = BITS(i, 15, 10), rn = BITS(i, 9, 5), rd = BITS(i, 4, 0);
    if (type == 3 || (!sf && (amt & 32))) return A64_EXIT_UDF;
    a64_u64 op2 = a64i_shift(a64i_rd(c, rm), type, amt, sf);
    a64_u64 x = a64i_trunc(a64i_rd(c, rn), sf);
    a64_u32 f;
    a64_u64 r = op ? a64i_awc(x, ~op2, 1, sf, &f) : a64i_awc(x, op2, 0, sf, &f);
    if (S) c->nzcv = f;
    a64i_wr(c, rd, r, sf);
    return A64_NEXT;
}

static int addsub_ext(a64_cpu *c, a64_u32 i) {
    int sf = BIT(i, 31), op = BIT(i, 30), S = BIT(i, 29);
    unsigned rm = BITS(i, 20, 16), option = BITS(i, 15, 13), sh = BITS(i, 12, 10), rn = BITS(i, 9, 5), rd = BITS(i, 4, 0);
    if (BITS(i, 23, 22) != 0 || sh > 4) return A64_EXIT_UDF;
    a64_u64 op2 = a64i_trunc(a64i_extend(a64i_rd(c, rm), option, sh), sf);
    a64_u64 x = a64i_trunc(a64i_rsp(c, rn), sf);
    a64_u32 f;
    a64_u64 r = op ? a64i_awc(x, ~op2, 1, sf, &f) : a64i_awc(x, op2, 0, sf, &f);
    if (S) { c->nzcv = f; a64i_wr(c, rd, r, sf); }
    else a64i_wsp(c, rd, r, sf);
    return A64_NEXT;
}

static int adc_group(a64_cpu *c, a64_u32 i) {
    int sf = BIT(i, 31), op = BIT(i, 30), S = BIT(i, 29);
    unsigned rm = BITS(i, 20, 16), op3 = BITS(i, 15, 10), rn = BITS(i, 9, 5), rd = BITS(i, 4, 0);
    if (op3 == 0) {                                                   /* ADC/ADCS/SBC/SBCS */
        a64_u64 y = a64i_rd(c, rm);
        if (op) y = ~y;
        a64_u32 f;
        a64_u64 r = a64i_awc(a64i_trunc(a64i_rd(c, rn), sf), a64i_trunc(y, sf), BIT(c->nzcv, 29), sf, &f);
        if (S) c->nzcv = f;
        a64i_wr(c, rd, r, sf);
        return A64_NEXT;
    }
    if ((op3 & 0x1F) == 1) {                                          /* RMIF (FlagM) */
        if (!sf || op || !S || BIT(i, 4)) return A64_EXIT_UDF;
        a64_u64 t = a64i_ror(a64i_rd(c, rn), BITS(i, 20, 15), 64);
        unsigned mask = BITS(i, 3, 0);
        a64_u32 nz = c->nzcv;
        for (int b = 0; b < 4; b++)
            if (mask & (1u << b)) nz = (nz & ~(1u << (28 + b))) | ((a64_u32)((t >> b) & 1) << (28 + b));
        c->nzcv = nz;
        return A64_NEXT;
    }
    if ((op3 & 0xF) == 2) {                                           /* SETF8/SETF16 (FlagM) */
        if (sf || op || !S || BITS(i, 20, 15) != 0 || BITS(i, 13, 10) != 2 || BITS(i, 4, 0) != 0xD) return A64_EXIT_UDF;
        unsigned msb = BIT(i, 14) ? 15 : 7;
        a64_u64 x = a64i_rd(c, rn);
        a64_u32 nz = c->nzcv & ~(A64_N | A64_Z | A64_V);
        if ((x >> msb) & 1) nz |= A64_N;
        if ((x & a64i_ones(msb + 1)) == 0) nz |= A64_Z;
        if ((((x >> (msb + 1)) ^ (x >> msb)) & 1)) nz |= A64_V;
        c->nzcv = nz;
        return A64_NEXT;
    }
    return A64_EXIT_UDF;
}

static int condcmp(a64_cpu *c, a64_u32 i) {
    int sf = BIT(i, 31), op = BIT(i, 30);
    unsigned cond = BITS(i, 15, 12), rn = BITS(i, 9, 5);
    if (!BIT(i, 29) || BIT(i, 10) || BIT(i, 4)) return A64_EXIT_UDF;
    if (!a64i_cond(c->nzcv, cond)) { c->nzcv = BITS(i, 3, 0) << 28; return A64_NEXT; }
    a64_u64 y = BIT(i, 11) ? BITS(i, 20, 16) : a64i_rd(c, BITS(i, 20, 16));
    a64_u64 x = a64i_trunc(a64i_rd(c, rn), sf);
    y = a64i_trunc(y, sf);
    a64_u32 f;
    if (op) a64i_awc(x, ~y, 1, sf, &f); else a64i_awc(x, y, 0, sf, &f);
    c->nzcv = f;
    return A64_NEXT;
}

static int condsel(a64_cpu *c, a64_u32 i) {
    int sf = BIT(i, 31), op = BIT(i, 30);
    unsigned rm = BITS(i, 20, 16), cond = BITS(i, 15, 12), o2 = BITS(i, 11, 10), rn = BITS(i, 9, 5), rd = BITS(i, 4, 0);
    if (BIT(i, 29) || (o2 & 2)) return A64_EXIT_UDF;
    a64_u64 r;
    if (a64i_cond(c->nzcv, cond)) r = a64i_rd(c, rn);
    else {
        r = a64i_rd(c, rm);
        if (op) r = ~r;
        if (o2 & 1) r += 1;
    }
    a64i_wr(c, rd, r, sf);
    return A64_NEXT;
}

static a64_u64 rbit64(a64_u64 x) {
    x = ((x >> 1) & 0x5555555555555555ull) | ((x & 0x5555555555555555ull) << 1);
    x = ((x >> 2) & 0x3333333333333333ull) | ((x & 0x3333333333333333ull) << 2);
    x = ((x >> 4) & 0x0F0F0F0F0F0F0F0Full) | ((x & 0x0F0F0F0F0F0F0F0Full) << 4);
    return __builtin_bswap64(x);
}

static int dp1src(a64_cpu *c, a64_u32 i) {
    int sf = BIT(i, 31);
    unsigned op2 = BITS(i, 20, 16), opc = BITS(i, 15, 10), rn = BITS(i, 9, 5), rd = BITS(i, 4, 0);
    if (BIT(i, 29)) return A64_EXIT_UDF;
    if (op2 == 1) {
        /* PAC/AUT/XPAC de 1 fonte (FEAT_PAuth, algoritmo "identidade": o ponteiro não muda e a
         * autenticação sempre passa). Rd fica como está; só validamos a codificação. */
        if (!sf) return A64_EXIT_UDF;
        if (opc < 8) return A64_NEXT;                         /* PACIA..AUTDB (Rn = modificador) */
        if (opc < 16) return rn == 31 ? A64_NEXT : A64_EXIT_UDF; /* PACIZA..AUTDZB */
        if (opc == 16 || opc == 17) return rn == 31 ? A64_NEXT : A64_EXIT_UDF; /* XPACI, XPACD */
        return A64_EXIT_UDF;
    }
    if (op2 != 0) return A64_EXIT_UDF;
    a64_u64 x = a64i_rd(c, rn), r;
    switch (opc) {
    case 0: r = sf ? rbit64(x) : (rbit64(x) >> 32); break;                     /* RBIT */
    case 1:                                                                     /* REV16 */
        r = ((x >> 8) & 0x00FF00FF00FF00FFull) | ((x & 0x00FF00FF00FF00FFull) << 8);
        break;
    case 2:                                                                     /* REV32 / REV (W) */
        r = sf ? (((a64_u64)__builtin_bswap32((a64_u32)(x >> 32)) << 32) | __builtin_bswap32((a64_u32)x))
               : __builtin_bswap32((a64_u32)x);
        break;
    case 3: if (!sf) return A64_EXIT_UDF; r = __builtin_bswap64(x); break;      /* REV (X) */
    case 4:                                                                     /* CLZ */
        if (sf) r = x ? (a64_u64)__builtin_clzll(x) : 64;
        else r = (a64_u32)x ? (a64_u64)__builtin_clz((a64_u32)x) : 32;
        break;
    case 5: {                                                                   /* CLS */
        unsigned M = sf ? 64 : 32;
        a64_u64 v = a64i_trunc(x, sf);
        a64_u64 y = ((v >> 1) ^ v) & a64i_ones(M - 1);
        r = y ? (M - 2 - (63 - __builtin_clzll(y))) : (M - 1);
        break;
    }
    default: return A64_EXIT_UDF;                                               /* CTZ/CNT/ABS (CSSC) etc. */
    }
    a64i_wr(c, rd, r, sf);
    return A64_NEXT;
}

static a64_u32 crc32(a64_u32 acc, a64_u64 val, unsigned bytes, a64_u32 poly) {
    for (unsigned b = 0; b < bytes; b++) {
        acc ^= (a64_u8)(val >> (8 * b));
        for (int k = 0; k < 8; k++) acc = (acc >> 1) ^ (poly & (0u - (acc & 1)));
    }
    return acc;
}

static int dp2src(a64_cpu *c, a64_u32 i) {
    int sf = BIT(i, 31);
    unsigned rm = BITS(i, 20, 16), opc = BITS(i, 15, 10), rn = BITS(i, 9, 5), rd = BITS(i, 4, 0);
    if (BIT(i, 29)) return A64_EXIT_UDF;                     /* SUBPS (MTE) e não alocados */
    a64_u64 x = a64i_trunc(a64i_rd(c, rn), sf), y = a64i_trunc(a64i_rd(c, rm), sf), r;
    unsigned M = sf ? 64 : 32;
    switch (opc) {
    case 2: r = y ? x / y : 0; break;                                            /* UDIV */
    case 3:                                                                      /* SDIV */
        if (!y) r = 0;
        else if (sf) r = (x == 0x8000000000000000ull && y == ~0ull) ? x : (a64_u64)((a64_i64)x / (a64_i64)y);
        else r = ((a64_u32)x == 0x80000000u && (a64_u32)y == 0xFFFFFFFFu) ? x : (a64_u32)((int32_t)x / (int32_t)y);
        break;
    case 8: r = a64i_shift(x, 0, y % M, sf); break;                              /* LSLV */
    case 9: r = a64i_shift(x, 1, y % M, sf); break;                              /* LSRV */
    case 10: r = a64i_shift(x, 2, y % M, sf); break;                             /* ASRV */
    case 11: r = a64i_shift(x, 3, y % M, sf); break;                             /* RORV */
    case 12:                                                                     /* PACGA */
        /* algoritmo "identidade": ComputePAC() = 0 → resultado = PAC<63:32>:Zeros(32) = 0 */
        if (!sf) return A64_EXIT_UDF;
        r = 0;
        break;
    case 16: case 17: case 18: case 19: case 20: case 21: case 22: case 23: {    /* CRC32/CRC32C */
        unsigned sz = opc & 3;
        if ((sz == 3) != (sf == 1)) return A64_EXIT_UDF;
        r = crc32((a64_u32)a64i_rd(c, rn), a64i_rd(c, rm), 1u << sz, (opc & 4) ? 0x82F63B78u : 0xEDB88320u);
        sf = 0;
        break;
    }
    default: return A64_EXIT_UDF;  /* SUBP/IRG/GMI (MTE), SMAX/UMAX/SMIN/UMIN (CSSC), não alocados */
    }
    a64i_wr(c, rd, r, sf);
    return A64_NEXT;
}

static int dp3src(a64_cpu *c, a64_u32 i) {
    int sf = BIT(i, 31);
    unsigned op31 = BITS(i, 23, 21), o0 = BIT(i, 15);
    unsigned rm = BITS(i, 20, 16), ra = BITS(i, 14, 10), rn = BITS(i, 9, 5), rd = BITS(i, 4, 0);
    if (BITS(i, 30, 29) != 0) return A64_EXIT_UDF;
    a64_u64 n = a64i_rd(c, rn), m = a64i_rd(c, rm), a = a64i_rd(c, ra), r;
    if (op31 == 0) {                                                             /* MADD/MSUB */
        a64_u64 p = n * m;
        r = o0 ? a - p : a + p;
        a64i_wr(c, rd, r, sf);
        return A64_NEXT;
    }
    if (!sf) return A64_EXIT_UDF;
    switch (op31) {
    case 1: { a64_u64 p = (a64_u64)((a64_i64)(int32_t)n * (int32_t)m); r = o0 ? a - p : a + p; break; } /* SMADDL/SMSUBL */
    case 5: { a64_u64 p = (a64_u64)(a64_u32)n * (a64_u32)m; r = o0 ? a - p : a + p; break; }           /* UMADDL/UMSUBL */
    case 2:                                                                      /* SMULH (Ra = 11111) */
        if (o0 || ra != 31) return A64_EXIT_UDF;
        r = (a64_u64)(((__int128)(a64_i64)n * (a64_i64)m) >> 64);
        break;
    case 6:                                                                      /* UMULH */
        if (o0 || ra != 31) return A64_EXIT_UDF;
        r = (a64_u64)(((a64_u128)n * m) >> 64);
        break;
    default: return A64_EXIT_UDF;
    }
    a64i_wr(c, rd, r, 1);
    return A64_NEXT;
}

int a64_exec_dpreg(a64_cpu *c, a64_u32 insn) {
    unsigned op1 = BIT(insn, 28), op2 = BITS(insn, 24, 21);
    if (!op1) {
        if (!(op2 & 8)) return logic_shifted(c, insn);
        return (op2 & 1) ? addsub_ext(c, insn) : addsub_shifted(c, insn);
    }
    if (op2 & 8) return dp3src(c, insn);
    switch (op2) {
    case 0: return adc_group(c, insn);
    case 2: return condcmp(c, insn);
    case 4: return condsel(c, insn);
    case 6: return BIT(insn, 30) ? dp1src(c, insn) : dp2src(c, insn);
    default: return A64_EXIT_UDF;
    }
}
