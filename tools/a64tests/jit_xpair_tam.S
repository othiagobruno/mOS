/* a64cmp: nao-comparavel (STXP com tamanho diferente do LDXP é CONSTRAINED UNPREDICTABLE: o QEMU
 * só confere o endereço e grava; nós falhamos, o que o ARM também permite) */
/* frente JIT: STXP de W depois de LDXP de X no mesmo endereço — o JIT faz o mesmo que o
 * interpretador (status 1, nada gravado). */
    .text
    .globl main
main:
    stp x29, x30, [sp, #-16]!
    adrp x10, buf
    add x10, x10, :lo12:buf
    ldxp x7, x8, [x10]
    stxp w13, w7, w8, [x10]
    ldp x0, x1, [x10]
    bl a64t_dump
    mov x0, #0
    ldp x29, x30, [sp], #16
    ret
    .data
    .balign 64
buf: .quad 100, 200
