/*
 * mOS — tradutor arm64: frente INT — desvios, geração de exceção e sistema (ARM ARM C4.1.87,
 * op0 = 101x). EL0 do macOS: o que o kernel do Darwin deixa o EL0 executar; o resto é UDF.
 *
 * Escolhas documentadas:
 *  - PAC com algoritmo "identidade" (doc): PAC, AUT e XPAC (hints e formas com registrador) não
 *    alteram registradores; BRAA/BLRAA/RETAA... desviam para o endereço como está.
 *  - BTI aceito e não imposto; PSTATE.BTYPE não é acompanhado (o mOS não marca páginas guarded).
 *  - HINTs não alocados executam como NOP (exigência do ARM ARM), inclusive DGH/CHKFEAT/CLRBHB.
 *  - SEV/SEVL: NOP (não há registrador de evento; WFE sempre sai com YIELD, que é permitido).
 *  - DC CVAC/CVAU/CVAP/CVADP/CIVAC e IC IVAU: sem efeito nos dados, mas sondam a linha de 64 B
 *    (falha de tradução/permissão de leitura vira FAULT com exit_info = endereço da linha).
 *  - CTR_EL0 = 0x84448004 (valor lido em M1 Icestorm e Firestorm); DCZID_EL0 = 4 (64 bytes).
 *  - MSR DAIFSet/DAIFClr e MRS/MSR DAIF: UDF (SCTLR_EL1.UMA = 0 no Darwin).
 */
#include "interp_int.h"

#define CTR_EL0_M1   0x84448004ull
#define DCZID_EL0_M1 4ull

/* ------------------------------------------------------------------ desvios imediatos */

static int b_imm(a64_cpu *c, a64_u32 i) {
    a64_u64 off = a64i_sext((a64_u64)BITS(i, 25, 0) << 2, 28);
    if (BIT(i, 31)) c->x[30] = c->pc + 4;
    c->pc += off;
    return A64_BRANCH;
}

static int cb_tb(a64_cpu *c, a64_u32 i) {
    unsigned rt = BITS(i, 4, 0), op = BIT(i, 24);
    int take;
    a64_u64 off;
    if (BIT(i, 25)) {                                       /* TBZ/TBNZ */
        unsigned bit = (BIT(i, 31) << 5) | BITS(i, 23, 19);
        take = (int)((a64i_rd(c, rt) >> bit) & 1) == (int)op;
        off = a64i_sext((a64_u64)BITS(i, 18, 5) << 2, 16);
    } else {                                                /* CBZ/CBNZ */
        a64_u64 v = a64i_trunc(a64i_rd(c, rt), BIT(i, 31));
        take = op ? v != 0 : v == 0;
        off = a64i_sext((a64_u64)BITS(i, 23, 5) << 2, 21);
    }
    if (!take) return A64_NEXT;
    c->pc += off;
    return A64_BRANCH;
}

static int bcond(a64_cpu *c, a64_u32 i) {
    if (BIT(i, 24) || BIT(i, 4)) return A64_EXIT_UDF;       /* BC.cond (FEAT_HBC) não anunciado */
    if (!a64i_cond(c->nzcv, BITS(i, 3, 0))) return A64_NEXT;
    c->pc += a64i_sext((a64_u64)BITS(i, 23, 5) << 2, 21);
    return A64_BRANCH;
}

/* ------------------------------------------------------------------ desvio por registrador */

static int b_reg(a64_cpu *c, a64_u32 i) {
    unsigned opc = BITS(i, 24, 21), op3 = BITS(i, 15, 10), rn = BITS(i, 9, 5), op4 = BITS(i, 4, 0);
    if (BITS(i, 20, 16) != 31) return A64_EXIT_UDF;
    a64_u64 target;
    int link = 0;
    switch (opc) {
    case 0: case 1: case 2:
        if (op3 == 0 && op4 == 0) target = a64i_rd(c, rn);                      /* BR/BLR/RET */
        else if ((op3 & ~1u) == 2 && op4 == 31) {
            if (opc == 2) { if (rn != 31) return A64_EXIT_UDF; target = c->x[30]; } /* RETAA/RETAB */
            else target = a64i_rd(c, rn);                                       /* BRAAZ/BLRAAZ... */
        } else return A64_EXIT_UDF;
        link = opc == 1;
        break;
    case 8: case 9:                                                             /* BRAA/BRAB/BLRAA/BLRAB */
        if ((op3 & ~1u) != 2) return A64_EXIT_UDF;
        target = a64i_rd(c, rn);     /* modificador X[op4]|SP irrelevante com PAC identidade */
        link = opc == 9;
        break;
    default: return A64_EXIT_UDF;    /* ERET/ERETAA/ERETAB/DRPS: UNDEFINED em EL0 */
    }
    if (link) c->x[30] = c->pc + 4;
    c->pc = target;
    /* alvo desalinhado: o desvio se completa (link incluído) e a busca do alvo dá PC alignment
     * fault (ELR = FAR = alvo) antes de executar qualquer coisa lá → SIGBUS no runtime */
    if (target & 3) { c->exit_info = target; return A64_EXIT_FAULT; }
    return A64_BRANCH;
}

/* ------------------------------------------------------------------ geração de exceção */

static int excgen(a64_cpu *c, a64_u32 i) {
    unsigned opc = BITS(i, 23, 21), ll = BITS(i, 1, 0);
    if (BITS(i, 4, 2) != 0) return A64_EXIT_UDF;
    a64_u64 imm = BITS(i, 20, 5);
    if (opc == 0 && ll == 1) { c->exit_info = imm; return A64_EXIT_SVC; }
    if (opc == 1 && ll == 0) { c->exit_info = imm; return A64_EXIT_BRK; }
    if (opc == 2 && ll == 0) { c->exit_info = imm; return A64_EXIT_HLT; }
    return A64_EXIT_UDF;   /* HVC/SMC (UNDEFINED em EL0), TCANCEL (TME), DCPS1..3, não alocados */
}

/* ------------------------------------------------------------------ sistema */

static int hint(a64_cpu *c, a64_u32 i) {
    (void)c;
    switch (BITS(i, 11, 5)) {
    case 1: case 2: case 3: return A64_EXIT_YIELD;      /* YIELD, WFE, WFI */
    default: return A64_NEXT;                           /* NOP, SEV(L), PAC/AUT/XPACLRI, BTI, ESB, CSDB... */
    }
}

static int barrier(a64_cpu *c, a64_u32 i) {
    unsigned crm = BITS(i, 11, 8), op2 = BITS(i, 7, 5);
    switch (op2) {
    case 2: c->excl_valid = 0; return A64_NEXT;                                 /* CLREX */
    case 4: a64_fence_full(); return A64_NEXT;                                  /* DSB, SSBB, PSSBB */
    case 5:                                                                     /* DMB */
        if ((crm & 3) == 1 || (crm & 3) == 2) __atomic_signal_fence(__ATOMIC_SEQ_CST); /* LD/ST: TSO já ordena */
        else a64_fence_full();
        return A64_NEXT;
    case 6: return A64_NEXT;                                                    /* ISB */
    case 7: return crm == 0 ? A64_NEXT : A64_EXIT_UDF;                          /* SB */
    default: return A64_EXIT_UDF;   /* DSB nXS (FEAT_XS), TCOMMIT (TME), não alocados */
    }
}

static int pstate_imm(a64_cpu *c, a64_u32 i) {
    unsigned op1 = BITS(i, 18, 16), crm = BITS(i, 11, 8), op2 = BITS(i, 7, 5);
    a64_u32 f = c->nzcv;
    unsigned N = BIT(f, 31), Z = BIT(f, 30), C = BIT(f, 29), V = BIT(f, 28);
    if (op1 == 0 && crm == 0) {
        switch (op2) {
        case 0: c->nzcv ^= A64_C; return A64_NEXT;                              /* CFINV */
        case 1: {                                                               /* XAFLAG */
            unsigned n = !C && !Z, z = Z && C, cc = C || Z, v = !C && Z;
            c->nzcv = (n << 31) | (z << 30) | (cc << 29) | (v << 28);
            return A64_NEXT;
        }
        case 2: {                                                               /* AXFLAG */
            unsigned z = Z || V, cc = C && !V;
            (void)N;
            c->nzcv = (z << 30) | (cc << 29);
            return A64_NEXT;
        }
        default: break;
        }
    }
    if (op1 == 3 && op2 == 2) { c->pstate = (c->pstate & ~A64_PS_DIT) | ((crm & 1) ? A64_PS_DIT : 0); return A64_NEXT; }
    if (op1 == 3 && op2 == 1) { c->pstate = (c->pstate & ~A64_PS_SSBS) | ((crm & 1) ? A64_PS_SSBS : 0); return A64_NEXT; }
    return A64_EXIT_UDF;  /* UAO/PAN/SPSel (EL1), TCO (MTE), DAIFSet/Clr (UMA=0), SVCR, ALLINT... */
}

static int sys_op(a64_cpu *c, a64_u32 i) {
    unsigned L = BIT(i, 21), op1 = BITS(i, 18, 16), crn = BITS(i, 15, 12), crm = BITS(i, 11, 8), op2 = BITS(i, 7, 5);
    unsigned rt = BITS(i, 4, 0);
    if (L || op1 != 3 || crn != 7 || op2 != 1) return A64_EXIT_UDF;
    a64_u64 a = a64i_rd(c, rt);
    switch (crm) {
    case 4: {                                                                   /* DC ZVA */
        a64_u64 b = a & ~63ull;
        for (int k = 0; k < 8; k++) a64_st64(b + 8 * k, 0);
        return A64_NEXT;
    }
    case 5: if (A64I_DC_PROBE_ALL) (void)a64_ld8(a & ~63ull); c->exit_info = a; return A64_EXIT_ICACHE;      /* IC IVAU */
    case 10: case 11: case 12: case 13: case 14:                                /* DC CVAC/CVAU/CVAP/CVADP/CIVAC */
        if (A64I_DC_PROBE_ALL || crm == 12 || crm == 13) (void)a64_ld8(a & ~63ull);   /* sem efeito, mas falha de tradução como no ARM (FAR = linha) */
        return A64_NEXT;
    default: return A64_EXIT_UDF;
    }
}

#define SR(o0, o1, n, m, o2) (((o0) << 14) | ((o1) << 11) | ((n) << 7) | ((m) << 3) | (o2))

static int sysreg(a64_cpu *c, a64_u32 i) {
    unsigned L = BIT(i, 21), rt = BITS(i, 4, 0);
    unsigned key = SR(BITS(i, 20, 19), BITS(i, 18, 16), BITS(i, 15, 12), BITS(i, 11, 8), BITS(i, 7, 5));
    if (L) {
        a64_u64 v;
        switch (key) {
        case SR(3, 3, 4, 2, 0): v = c->nzcv & 0xF0000000u; break;              /* NZCV */
        case SR(3, 3, 4, 2, 5): v = c->pstate & A64_PS_DIT; break;             /* DIT */
        case SR(3, 3, 4, 2, 6): v = c->pstate & A64_PS_SSBS; break;            /* SSBS */
        case SR(3, 3, 4, 4, 0): v = c->fpcr; break;                            /* FPCR */
        case SR(3, 3, 4, 4, 1): v = c->fpsr; break;                            /* FPSR */
        case SR(3, 3, 0, 0, 1): v = CTR_EL0_M1; break;                         /* CTR_EL0 */
        case SR(3, 3, 0, 0, 7): v = DCZID_EL0_M1; break;                       /* DCZID_EL0 */
        case SR(3, 3, 13, 0, 2): v = c->tpidr_el0; break;                      /* TPIDR_EL0 */
        case SR(3, 3, 13, 0, 3): v = c->tpidrro_el0; break;                    /* TPIDRRO_EL0 */
        case SR(3, 3, 14, 0, 0): v = A64_CNTFRQ; break;                        /* CNTFRQ_EL0 */
        case SR(3, 3, 14, 0, 1): case SR(3, 3, 14, 0, 2):                      /* CNTPCT/CNTVCT */
        case SR(3, 3, 14, 0, 5): case SR(3, 3, 14, 0, 6):                      /* CNTPCTSS/CNTVCTSS (ECV) */
            v = a64_host_cntvct(); break;
        default: return A64_EXIT_UDF;
        }
        a64_xw(c, rt, v);
        return A64_NEXT;
    }
    a64_u64 v = a64i_rd(c, rt);
    switch (key) {
    case SR(3, 3, 4, 2, 0): c->nzcv = (a64_u32)v & 0xF0000000u; break;
    case SR(3, 3, 4, 2, 5): c->pstate = (c->pstate & ~A64_PS_DIT) | ((a64_u32)v & A64_PS_DIT); break;
    case SR(3, 3, 4, 2, 6): c->pstate = (c->pstate & ~A64_PS_SSBS) | ((a64_u32)v & A64_PS_SSBS); break;
    case SR(3, 3, 4, 4, 0): c->fpcr = (a64_u32)v & A64_FPCR_MASK; break;
    case SR(3, 3, 4, 4, 1): c->fpsr = (a64_u32)v & A64_FPSR_MASK; break;
    case SR(3, 3, 13, 0, 2): c->tpidr_el0 = v; break;
    default: return A64_EXIT_UDF;    /* inclui TPIDRRO_EL0, CNT*, CTR, DCZID (só leitura em EL0) */
    }
    return A64_NEXT;
}

static int system_group(a64_cpu *c, a64_u32 i) {
    if (BIT(i, 22)) return A64_EXIT_UDF;            /* SYSP, MSRR/MRRS (FEAT_SYSINSTR128/D128) */
    unsigned op0 = BITS(i, 20, 19);
    if (op0 >= 2) return sysreg(c, i);
    if (op0 == 1) return sys_op(c, i);
    /* op0 = 0: hints, barreiras, PSTATE */
    if (BIT(i, 21)) return A64_EXIT_UDF;            /* TSTART/TTEST (TME) e não alocados */
    unsigned op1 = BITS(i, 18, 16), crn = BITS(i, 15, 12), rt = BITS(i, 4, 0);
    if (crn == 4) return rt == 31 ? pstate_imm(c, i) : A64_EXIT_UDF;
    if (op1 != 3 || rt != 31) return A64_EXIT_UDF; /* WFET/WFIT (FEAT_WFxT) caem aqui ou abaixo */
    if (crn == 2) return hint(c, i);
    if (crn == 3) return barrier(c, i);
    return A64_EXIT_UDF;
}

int a64_exec_branch(a64_cpu *c, a64_u32 insn) {
    switch (BITS(insn, 31, 29)) {
    case 0: case 4: return b_imm(c, insn);
    case 1: case 5: return cb_tb(c, insn);
    case 2: return BIT(insn, 25) ? A64_EXIT_UDF : bcond(c, insn);
    case 6:
        if (BIT(insn, 25)) return b_reg(c, insn);
        return BIT(insn, 24) ? system_group(c, insn) : excgen(c, insn);
    default: return A64_EXIT_UDF;                   /* CB<cc> etc. (FEAT_CMPBR) */
    }
}
