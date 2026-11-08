/*
 * mOS — tradutor arm64: frente INT — loads/stores inteiros (ARM ARM C4.1.88, op0 = x1x0, V = 0).
 * Os de SIMD&FP (V = 1) vão para a64_exec_ldst_simd (frente SIMD).
 *
 * Regras seguidas (ver a64.h, "Contrato de execução"):
 *  - nenhum registrador muda antes do último acesso que pode falhar; writeback da base por último.
 *  - endereço do guest = endereço do host; falha de página = exceção do host (o runtime reinicia).
 *
 * Escolhas documentadas (CONSTRAINED UNPREDICTABLE / IMPLEMENTATION DEFINED):
 *  - load com writeback e Rn == Rt (Rn != SP): o registrador fica com o endereço do writeback
 *    (load primeiro, writeback depois — "valor UNKNOWN", igual ao QEMU). Store com writeback e
 *    Rn == Rt guarda o valor ANTIGO de Rt. LDP com Rt == Rt2: fica o valor de Rt2.
 *  - STXR/STXP/CAS* com Rs sobrepondo Rt/Rn: os registradores são lidos antes da escrita de Rs.
 *  - alinhamento: o SCTLR_EL1 do XNU (osfmk/arm64/proc_reg.h, SCTLR_EL1_REQUIRED) tem A = 0,
 *    SA0 = 1 e nAA = 0. Com FEAT_LSE2 (AArch64.CheckAlignment): loads/stores normais aceitam
 *    qualquer alinhamento; exclusivos, atômicos LSE e acquire/release (LDAR, STLR, LDAPR, LDAPUR,
 *    STLUR, LDLAR, STLLR) dão falha de alinhamento (A64_EXIT_FAULT, exit_info = endereço) só
 *    quando o acesso atravessa um bloco alinhado de 16 bytes. STXR* verifica o alinhamento antes
 *    do monitor (AArch64.ExclusiveMonitorsPass), então falha mesmo sem LDXR anterior.
 *  - SA0 = 1: load/store com base SP e SP não alinhado a 16 → falha de alinhamento do SP
 *    (A64_EXIT_FAULT, exit_info = SP), antes de qualquer acesso. (O QEMU não implementa SA0.)
 *  - atomicidade: acesso dentro de um bloco de 16 bytes vira um único acesso do x86 (atômico
 *    dentro da linha de cache); LDP/STP/LDXP/STXP de 2×64 bits alinhados a 16 usam movdqa
 *    (atômico em CPUs x86 com AVX) e cmpxchg16b, como o FEAT_LSE2 exige.
 *  - monitor exclusivo: LDXR guarda (endereço, tamanho, valor); STXR faz compare-exchange com o
 *    valor guardado; falha (status 1) se o monitor não bate. Falha espúria/ABA permitida (doc).
 */
#include "interp_int.h"

typedef long long a64i_v2di __attribute__((vector_size(16)));

static inline a64_u128 ld128_atomic(a64_u64 a) {
#if defined(__x86_64__)
    a64i_v2di v;
    __asm__ volatile("movdqa (%1), %0" : "=x"(v) : "r"((uintptr_t)a) : "memory");
    a64_u128 r;
    __builtin_memcpy(&r, &v, 16);
    return r;
#else
    return a64_ld128(a);
#endif
}

static inline void st128_atomic(a64_u64 a, a64_u128 val) {
#if defined(__x86_64__)
    a64i_v2di v;
    __builtin_memcpy(&v, &val, 16);
    __asm__ volatile("movdqa %0, (%1)" : : "x"(v), "r"((uintptr_t)a) : "memory");
#else
    a64_st128(a, val);
#endif
}

/* compare-exchange de 16 bytes; lo e hi recebem o valor antigo; devolve 1 se trocou */
static inline int cas128(a64_u64 a, a64_u64 *lo, a64_u64 *hi, a64_u64 nlo, a64_u64 nhi) {
#if defined(__x86_64__)
    unsigned char ok;
    __asm__ volatile("lock cmpxchg16b (%[p])\n\tsete %[ok]"
                     : [ok] "=q"(ok), "+a"(*lo), "+d"(*hi)
                     : [p] "r"((uintptr_t)a), "b"(nlo), "c"(nhi)
                     : "memory", "cc");
    return ok;
#else
    a64_u128 e = ((a64_u128)*hi << 64) | *lo, n = ((a64_u128)nhi << 64) | nlo;
    int ok = __atomic_compare_exchange_n((a64_u128 *)(uintptr_t)a, &e, n, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    *lo = (a64_u64)e; *hi = (a64_u64)(e >> 64);
    return ok;
#endif
}

/* compare-exchange de 1/2/4/8 bytes; *exp recebe o valor antigo */
static inline int casn(a64_u64 a, unsigned n, a64_u64 *exp, a64_u64 nv) {
    void *p = (void *)(uintptr_t)a;
    switch (n) {
    case 1: { a64_u8 e = (a64_u8)*exp; int ok = __atomic_compare_exchange_n((a64_u8 *)p, &e, (a64_u8)nv, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); *exp = e; return ok; }
    case 2: { a64_u16 e = (a64_u16)*exp; int ok = __atomic_compare_exchange_n((a64_u16 *)p, &e, (a64_u16)nv, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); *exp = e; return ok; }
    case 4: { a64_u32 e = (a64_u32)*exp; int ok = __atomic_compare_exchange_n((a64_u32 *)p, &e, (a64_u32)nv, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); *exp = e; return ok; }
    default: { a64_u64 e = *exp; int ok = __atomic_compare_exchange_n((a64_u64 *)p, &e, nv, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); *exp = e; return ok; }
    }
}

#define SP_BASE(c, rn) do { if (A64I_SA0 && (rn) == 31 && ((c)->x[31] & 15)) { (c)->exit_info = (c)->x[31]; return A64_EXIT_FAULT; } } while (0)

static inline int cross16(a64_u64 a, unsigned n) { return (a & 15) + n > 16; }

static inline int align_fault(a64_cpu *c, a64_u64 a) { c->exit_info = a; return A64_EXIT_FAULT; }

static int a64i_atomic(a64_cpu *c, a64_u32 i);
static int a64i_ldra(a64_cpu *c, a64_u32 i);

/* -------------------------------------------------------------- acesso simples (1 registrador) */

/* opc/size → tipo de acesso. Devolve 0 se não alocado. */
enum { K_ST, K_LDZ, K_LDS64, K_LDS32, K_PRFM };
static int ls_kind(unsigned size, unsigned opc) {
    switch (opc) {
    case 0: return K_ST;
    case 1: return K_LDZ;
    case 2: return size == 3 ? K_PRFM : K_LDS64;
    default: return size >= 2 ? -1 : K_LDS32;
    }
}

static int ls_single(a64_cpu *c, int kind, unsigned size, unsigned rn, unsigned rt,
                     a64_u64 off, int wback, int post) {
    SP_BASE(c, rn);
    a64_u64 base = c->x[rn];                    /* 31 = SP */
    a64_u64 addr = post ? base : base + off;
    unsigned n = 1u << size;
    if (kind == K_ST) {
        a64_stn(addr, a64i_rd(c, rt), (int)n);
    } else {
        a64_u64 v = a64_ldn(addr, (int)n);
        if (kind == K_LDS64) v = a64i_sext(v, 8 * n);
        else if (kind == K_LDS32) v = (a64_u32)a64i_sext(v, 8 * n);
        a64_xw(c, rt, v);
    }
    if (wback) c->x[rn] = post ? base + off : addr;
    return A64_NEXT;
}

static int ls_reg_group(a64_cpu *c, a64_u32 i) {
    unsigned size = BITS(i, 31, 30), opc = BITS(i, 23, 22), rn = BITS(i, 9, 5), rt = BITS(i, 4, 0);
    int kind = ls_kind(size, opc);
    if (BIT(i, 24)) {                                               /* offset sem sinal, escalado */
        if (kind < 0) return A64_EXIT_UDF;
        if (kind == K_PRFM) return A64_NEXT;
        return ls_single(c, kind, size, rn, rt, (a64_u64)BITS(i, 21, 10) << size, 0, 0);
    }
    if (!BIT(i, 21)) {                                              /* imm9: unscaled/post/unpriv/pre */
        unsigned mode = BITS(i, 11, 10);
        a64_u64 off = a64i_sext(BITS(i, 20, 12), 9);
        if (kind < 0) return A64_EXIT_UDF;
        if (kind == K_PRFM) return mode == 0 ? A64_NEXT : A64_EXIT_UDF;   /* PRFUM */
        switch (mode) {
        case 0: case 2: return ls_single(c, kind, size, rn, rt, off, 0, 0); /* LDUR/STUR, LDTR/STTR */
        case 1: return ls_single(c, kind, size, rn, rt, off, 1, 1);
        default: return ls_single(c, kind, size, rn, rt, off, 1, 0);
        }
    }
    switch (BITS(i, 11, 10)) {
    case 0: return a64i_atomic(c, i);
    case 2: {                                                       /* offset por registrador */
        unsigned option = BITS(i, 15, 13);
        if (!(option & 2) || kind < 0) return A64_EXIT_UDF;
        if (kind == K_PRFM) return A64_NEXT;
        a64_u64 off = a64i_extend(a64i_rd(c, BITS(i, 20, 16)), option, BIT(i, 12) ? size : 0);
        return ls_single(c, kind, size, rn, rt, off, 0, 0);
    }
    default: return a64i_ldra(c, i);
    }
}

/* LDRAA/LDRAB (FEAT_PAuth): autenticação "identidade" da base, depois LDR X com offset/pre-index */
static int a64i_ldra(a64_cpu *c, a64_u32 i) {
    if (BITS(i, 31, 30) != 3) return A64_EXIT_UDF;
    unsigned rn = BITS(i, 9, 5), rt = BITS(i, 4, 0);
    a64_u64 off = a64i_sext(((a64_u64)BIT(i, 22) << 9) | BITS(i, 20, 12), 10) << 3;
    SP_BASE(c, rn);
    a64_u64 addr = c->x[rn] + off;
    a64_u64 v = a64_ld64(addr);
    a64_xw(c, rt, v);
    if (BIT(i, 11)) c->x[rn] = addr;
    return A64_NEXT;
}

/* --------------------------------------------------------------------- LSE (atômicos) */

static int a64i_atomic(a64_cpu *c, a64_u32 i) {
    unsigned size = BITS(i, 31, 30), A = BIT(i, 23), R = BIT(i, 22), rs = BITS(i, 20, 16);
    unsigned o3 = BIT(i, 15), opc = BITS(i, 14, 12), rn = BITS(i, 9, 5), rt = BITS(i, 4, 0);
    if (o3 && !(opc == 0 || (opc == 4 && A && !R && rs == 31))) return A64_EXIT_UDF; /* LDAPR só A=1,R=0,Rs=31; LS64 etc. */
    SP_BASE(c, rn);
    unsigned n = 1u << size, bits = 8 * n;
    a64_u64 addr = c->x[rn];
    if (o3) {
        if (opc == 4) {                                             /* LDAPR (FEAT_LRCPC) */
            if (!A || R || rs != 31) return A64_EXIT_UDF;
            if (!A64I_NAA && cross16(addr, n)) return align_fault(c, addr);
            a64_xw(c, rt, a64_ldn(addr, (int)n));
            return A64_NEXT;
        }
        if (opc != 0) return A64_EXIT_UDF;                          /* LD64B/ST64B* (LS64) etc. */
    }
    if (cross16(addr, n)) return align_fault(c, addr);
    a64_u64 v = a64i_rd(c, rs) & a64i_ones(bits), old;
    if (o3) {                                                       /* SWP */
        void *p = (void *)(uintptr_t)addr;
        switch (n) {
        case 1: old = __atomic_exchange_n((a64_u8 *)p, (a64_u8)v, __ATOMIC_SEQ_CST); break;
        case 2: old = __atomic_exchange_n((a64_u16 *)p, (a64_u16)v, __ATOMIC_SEQ_CST); break;
        case 4: old = __atomic_exchange_n((a64_u32 *)p, (a64_u32)v, __ATOMIC_SEQ_CST); break;
        default: old = __atomic_exchange_n((a64_u64 *)p, v, __ATOMIC_SEQ_CST); break;
        }
    } else {
        old = a64_ldn(addr, (int)n);
        for (;;) {
            a64_u64 nv;
            a64_i64 so = (a64_i64)a64i_sext(old, bits), sv = (a64_i64)a64i_sext(v, bits);
            switch (opc) {
            case 0: nv = old + v; break;                            /* LDADD */
            case 1: nv = old & ~v; break;                           /* LDCLR */
            case 2: nv = old ^ v; break;                            /* LDEOR */
            case 3: nv = old | v; break;                            /* LDSET */
            case 4: nv = so > sv ? old : v; break;                  /* LDSMAX */
            case 5: nv = so < sv ? old : v; break;                  /* LDSMIN */
            case 6: nv = old > v ? old : v; break;                  /* LDUMAX */
            default: nv = old < v ? old : v; break;                 /* LDUMIN */
            }
            if (casn(addr, n, &old, nv & a64i_ones(bits))) break;
        }
    }
    a64_xw(c, rt, old);
    return A64_NEXT;
}

/* ------------------------------------------- exclusivos, acquire/release, CAS, CASP (001000) */

static int excl_group(a64_cpu *c, a64_u32 i) {
    unsigned size = BITS(i, 31, 30), o2 = BIT(i, 23), L = BIT(i, 22), o1 = BIT(i, 21);
    unsigned rs = BITS(i, 20, 16), rt2 = BITS(i, 14, 10), rn = BITS(i, 9, 5), rt = BITS(i, 4, 0);
    unsigned n = 1u << size;
    if (o2 ? (rt2 != 31 || (!o1 && rs != 31)) : (o1 && size < 2 && (rt2 != 31 || (rs & 1) || (rt & 1))))
        return A64_EXIT_UDF;                                    /* SBO de LDAR/STLR/CAS*, CASP com registrador ímpar */
    SP_BASE(c, rn);
    a64_u64 addr = c->x[rn];

    if (o2 && !o1) {                                            /* LDAR/LDLAR/STLR/STLLR */
        if (rs != 31 || rt2 != 31) return A64_EXIT_UDF;
        if (!A64I_NAA && cross16(addr, n)) return align_fault(c, addr);
        if (L) { a64_xw(c, rt, a64_ldn(addr, (int)n)); return A64_NEXT; }
        a64_stn(addr, a64i_rd(c, rt), (int)n);
        if (BIT(i, 15)) a64_fence_full();                       /* STLR: RCsc com LDAR seguinte */
        return A64_NEXT;
    }
    if (o2 && o1) {                                             /* CAS/CASA/CASL/CASAL */
        if (rt2 != 31) return A64_EXIT_UDF;
        if (cross16(addr, n)) return align_fault(c, addr);
        a64_u64 old = a64i_rd(c, rs) & a64i_ones(8 * n);
        casn(addr, n, &old, a64i_rd(c, rt));
        a64_xw(c, rs, old);
        return A64_NEXT;
    }
    if (o1 && size < 2) {                                       /* CASP/CASPA/CASPL/CASPAL */
        if (rt2 != 31 || (rs & 1) || (rt & 1)) return A64_EXIT_UDF;
        unsigned tot = size ? 16 : 8;
        if (cross16(addr, tot)) return align_fault(c, addr);
        a64_u64 lo, hi;
        if (size) {
            lo = a64i_rd(c, rs); hi = a64i_rd(c, rs + 1);
            cas128(addr, &lo, &hi, a64i_rd(c, rt), a64i_rd(c, rt + 1));
        } else {
            a64_u64 e = (a64_u32)a64i_rd(c, rs) | (a64i_rd(c, rs + 1) << 32);
            casn(addr, 8, &e, (a64_u32)a64i_rd(c, rt) | (a64i_rd(c, rt + 1) << 32));
            lo = (a64_u32)e; hi = e >> 32;
        }
        a64_xw(c, rs, lo);
        a64_xw(c, rs + 1, hi);
        return A64_NEXT;
    }
    if (o1) {                                                   /* LDXP/LDAXP/STXP/STLXP */
        unsigned esz = size == 3 ? 8 : 4, tot = 2 * esz;
        if (cross16(addr, tot) && (L || A64I_STXR_ALIGN_FIRST || (c->excl_valid && c->excl_addr == addr && c->excl_size == tot))) return align_fault(c, addr);
        if (L) {
            a64_u64 lo, hi, v0, v1;
            if (esz == 8) { a64_u128 q = ld128_atomic(addr); lo = (a64_u64)q; hi = (a64_u64)(q >> 64); v0 = lo; v1 = hi; }
            else { a64_u64 d = a64_ld64(addr); lo = (a64_u32)d; hi = d >> 32; v0 = d; v1 = 0; }
            c->excl_addr = addr; c->excl_size = tot; c->excl_val[0] = v0; c->excl_val[1] = v1; c->excl_valid = 1;
            a64_xw(c, rt, lo);
            a64_xw(c, rt2, hi);
            return A64_NEXT;
        }
        a64_u64 a = a64i_rd(c, rt), b = a64i_rd(c, rt2);
        a64_u64 status = 1;
        if (c->excl_valid && c->excl_addr == addr && c->excl_size == tot) {
            if (esz == 8) {
                a64_u64 lo = c->excl_val[0], hi = c->excl_val[1];
                if (cas128(addr, &lo, &hi, a, b)) status = 0;
            } else {
                a64_u64 e = c->excl_val[0];
                if (casn(addr, 8, &e, (a64_u32)a | (b << 32))) status = 0;
            }
        }
        c->excl_valid = 0;
        a64_ww(c, rs, (a64_u32)status);
        return A64_NEXT;
    }
    /* LDXR/LDAXR/STXR/STLXR */
    if (cross16(addr, n) && (L || A64I_STXR_ALIGN_FIRST || (c->excl_valid && c->excl_addr == addr && c->excl_size == n))) return align_fault(c, addr);
    if (L) {
        a64_u64 v = a64_ldn(addr, (int)n);
        c->excl_addr = addr; c->excl_size = n; c->excl_val[0] = v; c->excl_val[1] = 0; c->excl_valid = 1;
        a64_xw(c, rt, v);
        return A64_NEXT;
    }
    a64_u64 v = a64i_rd(c, rt), status = 1;
    if (c->excl_valid && c->excl_addr == addr && c->excl_size == n) {
        a64_u64 e = c->excl_val[0];
        if (casn(addr, n, &e, v)) status = 0;
    }
    c->excl_valid = 0;
    a64_ww(c, rs, (a64_u32)status);
    return A64_NEXT;
}

/* ------------------------------------------------------------------------- literal */

static int literal(a64_cpu *c, a64_u32 i) {
    unsigned opc = BITS(i, 31, 30), rt = BITS(i, 4, 0);
    a64_u64 addr = c->pc + a64i_sext((a64_u64)BITS(i, 23, 5) << 2, 21);
    switch (opc) {
    case 0: a64_xw(c, rt, a64_ld32(addr)); break;
    case 1: a64_xw(c, rt, a64_ld64(addr)); break;
    case 2: a64_xw(c, rt, a64i_sext(a64_ld32(addr), 32)); break;
    default: break;                                             /* PRFM (literal) */
    }
    return A64_NEXT;
}

/* ------------------------------------------------- LDAPUR/STLUR (FEAT_LRCPC2) — 011001 */

static int rcpc2(a64_cpu *c, a64_u32 i) {
    if (BIT(i, 21) || BITS(i, 11, 10) != 0) return A64_EXIT_UDF;   /* MOPS, MTE tags, GCS, LRCPC3 */
    unsigned size = BITS(i, 31, 30), opc = BITS(i, 23, 22), rn = BITS(i, 9, 5), rt = BITS(i, 4, 0);
    int kind = ls_kind(size, opc);
    if (kind < 0 || kind == K_PRFM) return A64_EXIT_UDF;
    unsigned n = 1u << size;
    SP_BASE(c, rn);
    a64_u64 addr = c->x[rn] + a64i_sext(BITS(i, 20, 12), 9);
    if (!A64I_NAA && cross16(addr, n)) return align_fault(c, addr);
    if (kind == K_ST) {
        a64_stn(addr, a64i_rd(c, rt), (int)n);
        a64_fence_full();
        return A64_NEXT;
    }
    a64_u64 v = a64_ldn(addr, (int)n);
    if (kind == K_LDS64) v = a64i_sext(v, 8 * n);
    else if (kind == K_LDS32) v = (a64_u32)a64i_sext(v, 8 * n);
    a64_xw(c, rt, v);
    return A64_NEXT;
}

/* ------------------------------------------------------------------------- pares */

static int pair(a64_cpu *c, a64_u32 i) {
    unsigned opc = BITS(i, 31, 30), idx = BITS(i, 24, 23), L = BIT(i, 22);
    unsigned rt2 = BITS(i, 14, 10), rn = BITS(i, 9, 5), rt = BITS(i, 4, 0);
    if (opc == 3) return A64_EXIT_UDF;
    if (opc == 1 && (!L || idx == 0)) return A64_EXIT_UDF;     /* STGP (MTE); LDNP com sinal */
    unsigned scale = opc == 2 ? 3 : 2, n = 1u << scale;
    a64_u64 off = a64i_sext(BITS(i, 21, 15), 7) << scale;
    SP_BASE(c, rn);
    a64_u64 base = c->x[rn];
    a64_u64 addr = idx == 1 ? base : base + off;
    int wback = idx == 1 || idx == 3;
    if (L) {
        a64_u64 a, b;
        if (n == 8 && (addr & 15) == 0) { a64_u128 q = ld128_atomic(addr); a = (a64_u64)q; b = (a64_u64)(q >> 64); }
        else if (n == 4 && !cross16(addr, 8)) { a64_u64 d = a64_ld64(addr); a = (a64_u32)d; b = d >> 32; }
        else { a = a64_ldn(addr, (int)n); b = a64_ldn(addr + n, (int)n); }
        if (opc == 1) { a = a64i_sext(a, 32); b = a64i_sext(b, 32); }
        a64_xw(c, rt, a);
        a64_xw(c, rt2, b);
    } else {
        a64_u64 a = a64i_rd(c, rt), b = a64i_rd(c, rt2);
        if (n == 8 && (addr & 15) == 0) st128_atomic(addr, ((a64_u128)b << 64) | a);
        else if (n == 4 && !cross16(addr, 8)) a64_st64(addr, (a64_u32)a | (b << 32));
        else { a64_stn(addr, a, (int)n); a64_stn(addr + n, b, (int)n); }
    }
    if (wback) c->x[rn] = idx == 1 ? base + off : addr;
    return A64_NEXT;
}

int a64_exec_ldst(a64_cpu *c, a64_u32 insn) {
    if (BIT(insn, 26)) return a64_exec_ldst_simd(c, insn);
    switch (BITS(insn, 29, 28)) {
    case 0: return BIT(insn, 24) ? A64_EXIT_UDF : excl_group(c, insn);
    case 1: return BIT(insn, 24) ? rcpc2(c, insn) : literal(c, insn);
    case 2: return pair(c, insn);
    default: return ls_reg_group(c, insn);
    }
}
