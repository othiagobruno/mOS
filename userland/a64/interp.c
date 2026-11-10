/*
 * mOS — tradutor arm64: laço e decodificação de primeiro nível do interpretador de referência.
 * Os grupos (ARM ARM C4.1, op0 = bits 28:25) ficam em interp_int.c, interp_fp.c e interp_simd.c.
 */
#include "a64.h"
#include "a64_mem.h"
#include "a64_xmon.h"

int a64_interp_exec(a64_cpu *c, a64_u32 insn) {
    a64_xmon_bind(c);
    switch ((insn >> 25) & 0xF) {
    case 0x0: case 0x1: case 0x2: case 0x3:
        return A64_EXIT_UDF;                    /* UDF, SME, SVE, não alocado */
    case 0x8: case 0x9:
        return a64_exec_dpimm(c, insn);
    case 0xA: case 0xB:
        return a64_exec_branch(c, insn);
    case 0x4: case 0x6: case 0xC: case 0xE:
        return a64_exec_ldst(c, insn);
    case 0x5: case 0xD:
        return a64_exec_dpreg(c, insn);
    default: /* 0x7, 0xF: dados — ponto flutuante escalar e AdvSIMD */
        if (((insn >> 28) & 1) && !((insn >> 30) & 1))
            return a64_exec_fp(c, insn);
        return a64_exec_simd(c, insn);
    }
}

int a64_interp_step(a64_cpu *c) {
    a64_u32 insn = a64_ld32(c->pc);
    int r = a64_interp_exec(c, insn);
    if (r == A64_NEXT) { c->pc += 4; return A64_NEXT; }
    if (r == A64_BRANCH) return A64_BRANCH;
    c->exit_reason = (a64_u32)r;
    c->exit_insn = insn;
    if (r == A64_EXIT_SVC || r == A64_EXIT_YIELD || r == A64_EXIT_ICACHE) c->pc += 4;
    return r;
}

static volatile int *a64_stop_flag(a64_cpu *c) { return (volatile int *)&c->reserved[15]; }

void a64_request_stop(a64_cpu *c) { __atomic_store_n(a64_stop_flag(c), 1, __ATOMIC_SEQ_CST); }

int a64_interp_run(a64_cpu *c, a64_u64 budget) {
    for (a64_u64 n = 0; budget == 0 || n < budget; n++) {
        if (__builtin_expect(*a64_stop_flag(c), 0)) {
            __atomic_store_n(a64_stop_flag(c), 0, __ATOMIC_SEQ_CST);
            c->exit_reason = A64_EXIT_STOP;
            return A64_EXIT_STOP;
        }
        int r = a64_interp_step(c);
        if (r <= A64_BRANCH) continue;
        if (r == A64_EXIT_ICACHE) { a64_invalidate(c->exit_info & ~63ull, 64); continue; }
        if (r == A64_EXIT_YIELD) { a64_host_yield(c); continue; }
        return r;
    }
    c->exit_reason = A64_EXIT_STOP;
    return A64_EXIT_STOP;
}
