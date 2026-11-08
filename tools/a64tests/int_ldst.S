/* frente INT: loads/stores inteiros — todos os modos de endereçamento, tamanhos, extensão de
 * sinal, pares, literal, LDTR/STTR, PRFM */
#include "int_chk.h"
    .text
    .globl main
main:
    stp x29, x30, [sp, #-16]!
    adrp x10, buf
    add x10, x10, :lo12:buf
    mov x0, #0                      /* buf[i] = i */
1:  strb w0, [x10, x0]
    add x0, x0, #1
    cmp x0, #256
    b.ne 1b

    /* offset sem sinal */
    ldrb w0, [x10, #0x7f]
    CHK(x0, 0x7f)
    ldrh w0, [x10, #0x10]
    CHK(x0, 0x1110)
    ldr w0, [x10, #0x20]
    CHK(x0, 0x23222120)
    ldr x0, [x10, #8]
    CHK(x0, 0x0f0e0d0c0b0a0908)
    ldrsb w0, [x10, #0x80]
    CHK(x0, 0xffffff80)
    ldrsb x0, [x10, #0x80]
    CHK(x0, 0xffffffffffffff80)
    ldrsb x0, [x10, #0x7f]
    CHK(x0, 0x7f)
    ldrsh w0, [x10, #0x80]
    CHK(x0, 0xffff8180)
    ldrsh x0, [x10, #0x80]
    CHK(x0, 0xffffffffffff8180)
    ldrsw x0, [x10, #0x80]
    CHK(x0, 0xffffffff83828180)
    ldrsw x0, [x10, #0x40]
    CHK(x0, 0x43424140)
    ldr x0, [x10, #0xf8]
    CHK(x0, 0xfffefdfcfbfaf9f8)

    /* não escalado, desalinhado, negativo */
    add x11, x10, #0x40
    ldur x0, [x11, #-3]
    CHK(x0, 0x44434241403f3e3d)
    ldur w0, [x11, #1]
    CHK(x0, 0x44434241)
    ldurh w0, [x11, #-1]
    CHK(x0, 0x403f)
    ldursb w0, [x11, #0x40]
    CHK(x0, 0xffffff80)
    ldursh x0, [x11, #0x3f]
    CHK(x0, 0xffffffffffff807f)
    ldursw x0, [x11, #-0x40]
    CHK(x0, 0x03020100)
    ldr x1, [x10, #0]
    ldr x0, [x10, #3]                /* LDR desalinhado (vira LDUR) */
    CHK(x0, 0x0a09080706050403)
    ldr w0, [x10, #1]
    CHK(x0, 0x04030201)

    /* stores */
    adrp x12, wbuf
    add x12, x12, :lo12:wbuf
    ldr x2, =0x8877665544332211
    str x2, [x12]
    strb w2, [x12, #8]
    strh w2, [x12, #10]
    str w2, [x12, #12]
    stur x2, [x12, #17]
    sturb w2, [x12, #16]
    sturh w2, [x12, #25]
    stur w2, [x12, #27]
    ldp x3, x4, [x12]
    CHK(x3, 0x8877665544332211)
    CHK(x4, 0x4433221122110011)
    ldp x3, x4, [x12, #16]
    CHK(x3, 0x7766554433221111)
    CHK(x4, 0x0044332211221188)
    ldrb w5, [x12, #31]
    CHK(x5, 0)

    /* pré e pós-indexado (incluindo SP) */
    mov x13, x10
    ldr x0, [x13, #8]!
    CHK(x0, 0x0f0e0d0c0b0a0908)
    add x14, x10, #8
    CHKR(x13, x14)
    ldrb w0, [x13], #-8
    CHK(x0, 0x08)
    CHKR(x13, x10)
    ldrsh x0, [x13, #0x80]!
    CHK(x0, 0xffffffffffff8180)
    ldrsw x0, [x13], #4
    CHK(x0, 0xffffffff83828180)
    add x14, x10, #0x84
    CHKR(x13, x14)
    ldr w0, [x13, #-4]!
    CHK(x0, 0x83828180)
    mov x15, sp
    str x2, [sp, #-16]!
    ldr x0, [sp], #16
    CHKR(x0, x2)
    mov x16, sp
    CHKR(x15, x16)
    mov x17, x12
    str w2, [x17], #4
    strb w2, [x17, #3]!
    add x14, x12, #7
    CHKR(x17, x14)
    ldr x0, [x12]
    CHK(x0, 0x1177665544332211)

    /* registrador com extend/shift */
    mov x18, #3
    ldr x0, [x10, x18, lsl #3]
    CHK(x0, 0x1f1e1d1c1b1a1918)
    ldr x0, [x10, x18]
    CHK(x0, 0x0a09080706050403)
    mov w19, #-8
    add x20, x10, #0x40
    ldr x0, [x20, w19, sxtw]
    CHK(x0, 0x3f3e3d3c3b3a3938)
    ldr w0, [x20, w19, sxtw #2]
    CHK(x0, 0x23222120)
    ldrh w0, [x20, w19, sxtw #1]
    CHK(x0, 0x3130)
    ldrsb x0, [x10, w18, uxtw]
    CHK(x0, 3)
    ldr x21, =0x100000002
    ldrb w0, [x10, w21, uxtw]
    CHK(x0, 2)
    mov x22, #-0x10
    ldr x0, [x20, x22, sxtx]
    CHK(x0, 0x3736353433323130)
    ldrsw x0, [x20, x22, sxtx #2]
    CHK(x0, 0x03020100)
    strb w2, [x12, x18]
    strh w2, [x12, x18, lsl #1]
    ldr x0, [x12]
    prfm pldl1keep, [x10, x18, lsl #3]
    prfm pstl2strm, [x10, w19, sxtw]

    /* literal */
    ldr w0, lit
    CHK(x0, 0x89abcdef)
    ldr x0, lit
    CHK(x0, 0x0123456789abcdef)
    ldrsw x0, lit
    CHK(x0, 0xffffffff89abcdef)
    ldrsw x0, lit + 4
    CHK(x0, 0x01234567)
    prfm pldl1keep, lit
    prfm #0x1f, lit

    /* pares */
    ldp w0, w1, [x10, #0x10]
    CHK(x0, 0x13121110)
    CHK(x1, 0x17161514)
    ldp x0, x1, [x10, #0x10]
    CHK(x0, 0x1716151413121110)
    CHK(x1, 0x1f1e1d1c1b1a1918)
    ldp x0, x1, [x10, #8]            /* não alinhado a 16 */
    CHK(x0, 0x0f0e0d0c0b0a0908)
    CHK(x1, 0x1716151413121110)
    ldp w0, w1, [x10, #0xc]          /* atravessa 16 bytes */
    CHK(x0, 0x0f0e0d0c)
    CHK(x1, 0x13121110)
    ldpsw x0, x1, [x10, #0x80]
    CHK(x0, 0xffffffff83828180)
    CHK(x1, 0xffffffff87868584)
    ldpsw x0, x1, [x10, #0x40]
    CHK(x0, 0x43424140)
    mov x23, x10
    ldp x0, x1, [x23, #0x20]!
    CHK(x0, 0x2726252423222120)
    add x14, x10, #0x20
    CHKR(x23, x14)
    ldp w0, w1, [x23], #-0x20
    CHK(x1, 0x27262524)
    CHKR(x23, x10)
    ldpsw x0, x1, [x23, #8]!
    ldpsw x0, x1, [x23], #-8
    CHKR(x23, x10)
    ldnp x0, x1, [x10, #0x30]
    CHK(x0, 0x3736353433323130)
    ldnp w0, w1, [x10, #-0]
    CHK(x1, 0x07060504)
    ldr x3, =0x1111111111111111
    ldr x4, =0x2222222222222222
    stp x3, x4, [x12]
    stp w4, w3, [x12, #16]
    stnp x4, x3, [x12, #24]
    stnp w3, w4, [x12, #40]
    ldp x0, x1, [x12]
    CHKR(x0, x3)
    CHKR(x1, x4)
    ldr x0, [x12, #16]
    CHK(x0, 0x1111111122222222)
    ldp x0, x1, [x12, #24]
    CHKR(x0, x4)
    CHKR(x1, x3)
    ldr x0, [x12, #40]
    CHK(x0, 0x2222222211111111)
    mov x15, sp
    stp x3, x4, [sp, #-32]!
    stp xzr, x3, [sp, #16]
    ldp x0, x1, [sp], #32
    CHKR(x0, x3)
    mov x16, sp
    CHKR(x15, x16)
    ldr x0, [sp, #-16]
    CHK(x0, 0)
    mov x17, x12
    stp w3, w4, [x17, #8]!
    stp x4, x3, [x17], #-8
    CHKR(x17, x12)
    ldp w0, w1, [x12, #8]
    CHK(x0, 0x22222222)

    /* não privilegiados (= normais em EL0) */
    ldtr x0, [x10, #8]
    CHK(x0, 0x0f0e0d0c0b0a0908)
    ldtrb w0, [x10, #0x81]
    CHK(x0, 0x81)
    ldtrsb x0, [x10, #0x81]
    CHK(x0, 0xffffffffffffff81)
    ldtrsh w0, [x10, #0x81]
    CHK(x0, 0xffff8281)
    ldtrsw x0, [x10, #0x81]
    CHK(x0, 0xffffffff84838281)
    ldtrh w0, [x10, #-0]
    CHK(x0, 0x0100)
    ldtr w0, [x10, #255]
    sttr x3, [x12]
    sttrb w4, [x12, #1]
    sttrh w4, [x12, #2]
    sttr w4, [x12, #4]
    ldr x0, [x12]
    CHK(x0, 0x2222222222222211)
    prfum pldl1keep, [x10, #-1]

    INT_OK
    bl a64t_dump
    mov x0, #0
    ldp x29, x30, [sp], #16
    ret

    .p2align 3
lit: .quad 0x0123456789abcdef
    .ltorg
    .bss
    .p2align 6
buf:  .space 256
wbuf: .space 128
