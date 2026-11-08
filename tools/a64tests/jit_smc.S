/* frente JIT: código auto-modificável com o protocolo do ARM (DC CVAU, DSB, IC IVAU, DSB, ISB).
 * Uma função numa página gravável é reescrita 40 vezes (movz x0, #k); a soma tem que ser 780.
 * Depois a mesma coisa sem DC/IC (só a rede de segurança do JIT: proteção de escrita e, quando a
 * página vira volátil, blocos que conferem o próprio código): soma 780 de novo. */
    .text
    .globl main
main:
    stp x29, x30, [sp, #-16]!
    stp x19, x20, [sp, #-16]!
    stp x21, x22, [sp, #-16]!
    adrp x21, smc_fn
    add x21, x21, :lo12:smc_fn
    mov x22, #0                  /* modo: 0 = com IC IVAU, 1 = sem */
2:  mov x19, #0
    mov x20, #0
1:  movz w1, #0xD280, lsl #16    /* movz x0, #k */
    orr w1, w1, w20, lsl #5
    str w1, [x21]
    cbnz x22, 3f
    dc cvau, x21
    dsb ish
    ic ivau, x21
    dsb ish
3:  isb
    blr x21
    add x19, x19, x0
    add x20, x20, #1
    cmp x20, #40
    b.ne 1b
    cmp x19, #780
    b.ne 9f
    add x22, x22, #1
    cmp x22, #2
    b.ne 2b
    mov x0, #1
    adr x1, okmsg
    mov x2, #3
    bl a64t_write
    mov x0, #0
    b 8f
9:  mov x0, x19
8:  ldp x21, x22, [sp], #16
    ldp x19, x20, [sp], #16
    ldp x29, x30, [sp], #16
    ret
okmsg: .ascii "ok\n"
    .data
    .balign 4096
smc_fn:
    movz x0, #0
    ret
