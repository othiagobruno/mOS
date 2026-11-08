/* frente JIT: o MXCSR acumula o PE dentro do bloco; zerar o FPSR (MSR) no meio do bloco não pode
 * deixar uma operação exata seguinte religar o IXC. Esperado: x1 = 0x10 (IXC da 1ª), x2 = 0. */
    .text
    .globl main
main:
    stp x29, x30, [sp, #-16]!
    fmov d0, #1.0
    fmov d1, #3.0
    fdiv d2, d0, d1              /* inexata: IXC */
    mrs x1, fpsr
    msr fpsr, xzr
    fadd d3, d0, d1              /* exata: FPSR tem que continuar 0 */
    mrs x2, fpsr
    bl a64t_dump
    mov x0, #0
    ldp x29, x30, [sp], #16
    ret
