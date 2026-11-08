/*
 * mOS — oráculo do tradutor arm64: entrada, vetores de exceção e troca de contexto do monitor
 * bare-metal (EL1) que roda os testes em EL0 no qemu-system-aarch64 -M virt. Ver mon.c.
 */
    .section .text.boot, "ax"
    .globl _start
_start:
    msr daifset, #0xf
    ldr x0, =__stack_top
    mov sp, x0
    mov x0, #(3 << 20)              /* CPACR_EL1.FPEN = 11: FP/SIMD sem trap em EL0/EL1 */
    msr cpacr_el1, x0
    isb
    ldr x0, =__bss_start
    ldr x1, =__bss_end
1:  cmp x0, x1
    b.hs 2f
    str xzr, [x0], #8
    b 1b
2:  ldr x0, =vectors
    msr vbar_el1, x0
    isb
    bl mon_main
3:  wfi
    b 3b

/* long semihost(long op, void *param) */
    .text
    .globl semihost
semihost:
    hlt #0xf000
    ret

/* ---- vetores ---- */
    .balign 2048
vectors:
    .irp n, 0, 1, 2, 3, 4, 5, 6, 7
    .balign 0x80
    b bad_vector
    .endr
    .balign 0x80
    b ex_sync                       /* 0x400: EL0 AArch64 síncrona */
    .balign 0x80
    b ex_irq                        /* 0x480: IRQ vindo de EL0 */
    .balign 0x80
    b ex_other                      /* FIQ */
    .balign 0x80
    b ex_other                      /* SError */
    .irp n, 0, 1, 2, 3
    .balign 0x80
    b bad_vector                    /* EL0 AArch32 */
    .endr

/* contexto (ver struct mon_ctx em mon.c):
 *   0 x[31]  248 sp_el0  256 elr  264 spsr  272 esr  280 far  288 fpcr  296 fpsr  304 tpidr_el0
 *   320 v[32] (16 bytes cada) */
.macro SAVE_CTX kind
    msr tpidr_el1, x0
    adrp x0, mon_ctx
    add x0, x0, :lo12:mon_ctx
    stp x1, x2, [x0, #8]
    stp x3, x4, [x0, #24]
    stp x5, x6, [x0, #40]
    stp x7, x8, [x0, #56]
    stp x9, x10, [x0, #72]
    stp x11, x12, [x0, #88]
    stp x13, x14, [x0, #104]
    stp x15, x16, [x0, #120]
    stp x17, x18, [x0, #136]
    stp x19, x20, [x0, #152]
    stp x21, x22, [x0, #168]
    stp x23, x24, [x0, #184]
    stp x25, x26, [x0, #200]
    stp x27, x28, [x0, #216]
    stp x29, x30, [x0, #232]
    mrs x1, tpidr_el1
    str x1, [x0]
    mrs x1, sp_el0
    mrs x2, elr_el1
    stp x1, x2, [x0, #248]
    mrs x1, spsr_el1
    mrs x2, esr_el1
    stp x1, x2, [x0, #264]
    mrs x1, far_el1
    mrs x2, fpcr
    stp x1, x2, [x0, #280]
    mrs x1, fpsr
    mrs x2, tpidr_el0
    stp x1, x2, [x0, #296]
    add x1, x0, #320
    stp q0, q1, [x1], #32
    stp q2, q3, [x1], #32
    stp q4, q5, [x1], #32
    stp q6, q7, [x1], #32
    stp q8, q9, [x1], #32
    stp q10, q11, [x1], #32
    stp q12, q13, [x1], #32
    stp q14, q15, [x1], #32
    stp q16, q17, [x1], #32
    stp q18, q19, [x1], #32
    stp q20, q21, [x1], #32
    stp q22, q23, [x1], #32
    stp q24, q25, [x1], #32
    stp q26, q27, [x1], #32
    stp q28, q29, [x1], #32
    stp q30, q31, [x1], #32
    ldr x1, =__stack_top
    mov sp, x1
    mov x0, #\kind
    bl mon_exception
    b .
.endm

ex_sync:
    SAVE_CTX 0
ex_irq:
    SAVE_CTX 1
ex_other:
    SAVE_CTX 2
bad_vector:
    mrs x0, esr_el1
    mrs x1, elr_el1
    mrs x2, far_el1
    ldr x3, =__stack_top
    mov sp, x3
    bl mon_bad
    b .

/* void mon_resume(void): restaura mon_ctx inteiro e volta para EL0 */
    .globl mon_resume
mon_resume:
    adrp x0, mon_ctx
    add x0, x0, :lo12:mon_ctx
    add x1, x0, #320
    ldp q0, q1, [x1], #32
    ldp q2, q3, [x1], #32
    ldp q4, q5, [x1], #32
    ldp q6, q7, [x1], #32
    ldp q8, q9, [x1], #32
    ldp q10, q11, [x1], #32
    ldp q12, q13, [x1], #32
    ldp q14, q15, [x1], #32
    ldp q16, q17, [x1], #32
    ldp q18, q19, [x1], #32
    ldp q20, q21, [x1], #32
    ldp q22, q23, [x1], #32
    ldp q24, q25, [x1], #32
    ldp q26, q27, [x1], #32
    ldp q28, q29, [x1], #32
    ldp q30, q31, [x1], #32
    ldp x1, x2, [x0, #248]
    msr sp_el0, x1
    msr elr_el1, x2
    ldr x1, [x0, #264]
    msr spsr_el1, x1
    ldp x1, x2, [x0, #288]
    msr fpcr, x1
    msr fpsr, x2
    ldr x1, [x0, #304]
    msr tpidr_el0, x1
    ldp x29, x30, [x0, #232]
    ldp x27, x28, [x0, #216]
    ldp x25, x26, [x0, #200]
    ldp x23, x24, [x0, #184]
    ldp x21, x22, [x0, #168]
    ldp x19, x20, [x0, #152]
    ldp x17, x18, [x0, #136]
    ldp x15, x16, [x0, #120]
    ldp x13, x14, [x0, #104]
    ldp x11, x12, [x0, #88]
    ldp x9, x10, [x0, #72]
    ldp x7, x8, [x0, #56]
    ldp x5, x6, [x0, #40]
    ldp x3, x4, [x0, #24]
    ldp x1, x2, [x0, #8]
    ldr x0, [x0]
    eret
    .ltorg
