/* frente INT: monitor exclusivo sem ABA. O STR do mesmo valor, o de outro tamanho que
 * sobrepõe e o par (STP) derrubam o STXR/STXP. Endereço que não sobrepõe não derruba. */
#include "int_chk.h"
    .text
    .globl main
main:
    stp x29, x30, [sp, #-16]!
    adrp x10, abuf
    add x10, x10, :lo12:abuf
    ldr x3, =0x1111111111111111
    str x3, [x10]

    ldxr x0, [x10]
    CHKR(x0, x3)
    str x3, [x10]                    /* mesmo valor: a época anda, o STXR falha */
    mov x2, #0x99
    stxr w1, x2, [x10]
    CHK(x1, 1)
    ldr x0, [x10]
    CHKR(x0, x3)

    ldxr x0, [x10]
    str x3, [x10, #8]                /* outra palavra: não derruba */
    stxr w1, x2, [x10]
    CHK(x1, 0)
    ldr x0, [x10]
    CHK(x0, 0x99)

    str x3, [x10]
    ldxr x0, [x10]
    str w3, [x10, #4]                /* metade sobreposta, mesmo padrão de bits */
    stxr w1, x2, [x10]
    CHK(x1, 1)

    ldxp x0, x1, [x10]
    stp x0, x1, [x10]                /* o par regrava os mesmos bits */
    mov x4, #1
    mov x5, #2
    stxp w6, x4, x5, [x10]
    CHK(x6, 1)

    ldxr w0, [x10]
    clrex
    stxr w1, w2, [x10]
    CHK(x1, 1)

    INT_OK
    bl a64t_dump
    mov x0, #0
    ldp x29, x30, [sp], #16
    ret
    .ltorg
    .bss
    .p2align 6
abuf: .space 128
