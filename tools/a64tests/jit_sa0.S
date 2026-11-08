/* a64cmp: nao-comparavel (o QEMU não implementa SCTLR.SA0) */
/* frente JIT: SA0 = 1 — load com base SP desalinhado falha ANTES do acesso (FAULT com o SP);
 * o JIT tem que dar o mesmo estado (flags do cmp vivas, x1 sujo no cache). */
    .text
    .globl main
main:
    mov x1, #42
    cmp x1, #50
    sub sp, sp, #8
    ldr x0, [sp, #8]
    add sp, sp, #8
    ret
