/* frente JIT: código modificado SEM DC/IC (fora do protocolo do ARM), poucas vezes: só a proteção
 * de escrita das páginas traduzidas detecta (a página ainda não vira volátil). Cada chamada tem
 * que ver o código novo, como no interpretador: soma 0+1+2 = 3, e x0 final = 2. */
    .text
    .globl main
main:
    stp x29, x30, [sp, #-16]!
    stp x19, x20, [sp, #-16]!
    adrp x21, smc_fn
    add x21, x21, :lo12:smc_fn
    mov x19, #0
    mov x20, #0
1:  movz w1, #0xD280, lsl #16
    orr w1, w1, w20, lsl #5
    str w1, [x21]
    blr x21
    add x19, x19, x0
    add x20, x20, #1
    cmp x20, #3
    b.ne 1b
    mov x0, x19
    bl a64t_dump
    mov x0, #0
    ldp x19, x20, [sp], #16
    ldp x29, x30, [sp], #16
    ret
    .data
    .balign 4096
smc_fn:
    movz x0, #0
    ret
