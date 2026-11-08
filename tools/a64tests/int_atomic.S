/* frente INT: exclusivos, LSE (CAS/CASP/SWP/LD<op>), acquire/release, RCpc/RCpc2, LORegions */
#include "int_chk.h"
    .text
    .globl main
main:
    stp x29, x30, [sp, #-16]!
    adrp x10, abuf
    add x10, x10, :lo12:abuf
    add x16, x10, #2
    add x17, x10, #4
    add x18, x10, #6
    add x19, x10, #7
    add x20, x10, #16
    add x21, x10, #24
    add x22, x10, #32
    ldr x3, =0x8000000000000005

    /* LDXR/STXR */
    str x3, [x10]
    ldxr x0, [x10]
    CHKR(x0, x3)
    mov x2, #0x77
    stxr w1, x2, [x10]
    CHK(x1, 0)
    ldr x0, [x10]
    CHK(x0, 0x77)
    stxr w1, x3, [x10]               /* monitor já consumido */
    CHK(x1, 1)
    ldr x0, [x10]
    CHK(x0, 0x77)
    ldaxr x0, [x10]
    clrex
    stlxr w1, x3, [x10]
    CHK(x1, 1)
    ldaxrb w0, [x10]
    CHK(x0, 0x77)
    mov w2, #0x1ff
    stlxrb w1, w2, [x10]
    CHK(x1, 0)
    ldr x0, [x10]
    CHK(x0, 0xff)
    ldxrh w0, [x16]
    stxrh w1, w2, [x16]
    CHK(x1, 0)
    ldxr w0, [x17]
    mov w2, #-1
    stxr w1, w2, [x17]
    CHK(x1, 0)
    ldr x0, [x10]
    CHK(x0, 0xffffffff01ff00ff)
    ldxr w0, [x10]                   /* outro escritor muda o valor: STXR falha */
    str wzr, [x10]
    stxr w1, w2, [x10]
    CHK(x1, 1)
    ldr w0, [x10]
    CHK(x0, 0)

    /* LDXP/STXP 32 e 64 bits */
    ldr x4, =0x1111111111111111
    ldr x5, =0x2222222222222222
    stp x4, x5, [x10, #16]
    ldxp x0, x1, [x20]
    CHKR(x0, x4)
    CHKR(x1, x5)
    stxp w6, x5, x4, [x20]
    CHK(x6, 0)
    ldp x0, x1, [x10, #16]
    CHKR(x0, x5)
    CHKR(x1, x4)
    ldaxp w0, w1, [x21]
    CHK(x0, 0x11111111)
    stlxp w6, w4, wzr, [x21]
    CHK(x6, 0)
    ldr x0, [x10, #24]
    CHK(x0, 0x11111111)
    stlxp w6, w4, w4, [x21]
    CHK(x6, 1)

    /* LDAR/STLR/LDAPR/LDLAR/STLLR e RCpc2 */
    ldr x7, =0xfedcba9876543210
    stlr x7, [x22]
    ldar x0, [x22]
    CHKR(x0, x7)
    add x11, x10, #32
    ldarb w0, [x11]
    CHK(x0, 0x10)
    ldarh w0, [x11]
    CHK(x0, 0x3210)
    ldar w0, [x11]
    CHK(x0, 0x76543210)
    ldapr x0, [x11]
    CHKR(x0, x7)
    ldaprb w0, [x11]
    ldaprh w0, [x11]
    ldapr w0, [x11]
    CHK(x0, 0x76543210)
    stlrb w7, [x11, #0]
    stlrh w7, [x11]
    stlr w7, [x11]
    add x12, x11, #3
    ldar w0, [x12]                   /* desalinhado dentro de 16 bytes: permitido (LSE2) */
    CHK(x0, 0xdcba9876)
    ldlar x0, [x11]
    CHKR(x0, x7)
    ldlarb w0, [x11]
    stllr x4, [x11]
    stllrh w5, [x11]
    stllrb w7, [x11]
    stllr w5, [x11, #0]
    ldr x0, [x11]
    CHK(x0, 0x1111111122222222)
    add x13, x11, #8
    stlur x7, [x13, #-8]
    ldapur x0, [x13, #-8]
    CHKR(x0, x7)
    ldapursb x0, [x13, #-1]
    CHK(x0, 0xfffffffffffffffe)
    ldapursb w0, [x13, #-1]
    CHK(x0, 0xfffffffe)
    ldapursh x0, [x13, #-2]
    CHK(x0, 0xfffffffffffffedc)
    ldapursh w0, [x13, #-2]
    CHK(x0, 0xfffffedc)
    ldapursw x0, [x13, #-4]
    CHK(x0, 0xfffffffffedcba98)
    ldapurb w0, [x13, #-8]
    CHK(x0, 0x10)
    ldapurh w0, [x13, #-7]
    CHK(x0, 0x5432)
    ldapur w0, [x13, #-4]
    CHK(x0, 0xfedcba98)
    stlurb w4, [x13, #-8]
    stlurh w4, [x13, #-6]
    stlur w4, [x13, #-4]
    ldr x0, [x11]
    CHK(x0, 0x1111111111113211)

    /* CAS */
    str x3, [x10]
    mov x5, x3
    mov x6, #0x99
    cas x5, x6, [x10]
    CHKR(x5, x3)
    ldr x0, [x10]
    CHK(x0, 0x99)
    mov x5, #0x12
    casa x5, x6, [x10]
    CHK(x5, 0x99)
    mov x5, #0x99
    mov x6, #0x55
    casl x5, x6, [x10]
    casal x5, x6, [x10]
    ldr x0, [x10]
    CHK(x0, 0x55)
    mov w5, #0xffffff55               /* CASB compara só o byte */
    mov w6, #0x166
    casb w5, w6, [x10]
    CHK(x5, 0x55)
    ldr x0, [x10]
    CHK(x0, 0x66)
    mov w5, #0x66
    caslh w5, w6, [x10]
    CHK(x5, 0x66)
    ldr x0, [x10]
    CHK(x0, 0x166)
    mov w5, #0x166
    mov w6, #-1
    casal w5, w6, [x10]
    ldr x0, [x10]
    CHK(x0, 0xffffffff)
    casab w5, w6, [x10]
    CHK(x5, 0xff)
    casalb w5, w6, [x10]
    casah w5, w6, [x10]
    CHK(x5, 0xffff)
    cas x5, x6, [x10]                /* falha: x5 recebe o valor da memória */
    CHK(x5, 0xffffffff)

    /* CASP */
    stp x4, x7, [x10, #16]
    mov x8, x4
    mov x9, x7
    mov x0, #1
    mov x1, #2
    casp x8, x9, x0, x1, [x20]
    CHKR(x8, x4)
    CHKR(x9, x7)
    ldp x14, x15, [x10, #16]
    CHK(x14, 1)
    CHK(x15, 2)
    caspa x8, x9, x0, x1, [x20] /* falha */
    CHK(x8, 1)
    CHK(x9, 2)
    mov x8, #1
    mov x9, #2
    ldr x0, =0x3333333344444444
    mov x1, x0
    caspl x8, x9, x0, x1, [x20]
    ldp x14, x15, [x10, #16]
    CHKR(x14, x0)
    CHKR(x15, x0)
    mov w8, #0x44444444
    mov w9, #0x33333333
    mov w0, #7
    mov w1, #8
    caspal w8, w9, w0, w1, [x20]
    CHK(x8, 0x44444444)
    CHK(x9, 0x33333333)
    ldr x14, [x10, #16]
    CHK(x14, 0x0000000800000007)
    casp w8, w9, w0, w1, [x21]  /* sucesso: mesma palavra */
    CHK(x8, 0x44444444)
    CHK(x9, 0x33333333)
    mov w8, #0
    caspa w8, w9, w0, w1, [x21]
    CHK(x8, 7)
    CHK(x9, 8)

    /* SWP */
    ldr x4, =0x1111111111111111
    ldr x5, =0x2222222222222222
    str x3, [x10]
    swp x4, x0, [x10]
    CHKR(x0, x3)
    swpal x5, x1, [x10]
    CHKR(x1, x4)
    swpb w7, w0, [x10]
    CHK(x0, 0x22)
    swplh w7, w0, [x16]
    CHK(x0, 0x2222)
    swpa w7, w0, [x17]
    CHK(x0, 0x22222222)
    ldr x0, [x10]
    CHK(x0, 0x7654321032102210)

    /* LD<op> */
    ldr x3, =0x8000000000000005
    mov x4, #3
    str x3, [x10]
    ldadd x4, x0, [x10]
    CHKR(x0, x3)
    ldr x0, [x10]
    CHK(x0, 0x8000000000000008)
    ldclral x4, x0, [x10]
    ldr x0, [x10]
    CHK(x0, 0x8000000000000008)
    mov x4, #0xc
    ldclra x4, x0, [x10]
    ldr x0, [x10]
    CHK(x0, 0x8000000000000000)
    ldeorl x4, x0, [x10]
    ldseta x4, x0, [x10]
    ldr x0, [x10]
    CHK(x0, 0x800000000000000c)
    ldsmax x4, x0, [x10]             /* max(negativo, 12) = 12 */
    ldr x0, [x10]
    CHK(x0, 0xc)
    ldsmin x3, x0, [x10]
    ldr x0, [x10]
    CHKR(x0, x3)
    ldumin x4, x0, [x10]
    ldr x0, [x10]
    CHK(x0, 0xc)
    ldumax x3, x0, [x10]
    ldr x0, [x10]
    CHKR(x0, x3)
    stadd x4, [x10]
    stclr x4, [x10]
    steor x4, [x10]
    stset x4, [x10]
    stsmax x4, [x10]
    stsmin x4, [x10]
    stumax x4, [x10]
    stumin x4, [x10]
    ldr x0, [x10]
    CHK(x0, 0xc)
    str x3, [x10]
    mov w4, #2
    mov w5, #-1
    strb w5, [x10]
    ldaddb w4, w0, [x10]
    CHK(x0, 0xff)
    ldrb w0, [x10]
    CHK(x0, 1)
    mov w5, #0x80
    strb w5, [x10]
    mov w4, #5
    ldsmaxb w4, w0, [x10]
    CHK(x0, 0x80)
    ldrb w0, [x10]
    CHK(x0, 5)
    ldsminb w5, w0, [x10]
    ldrb w0, [x10]
    CHK(x0, 0x80)
    ldumaxb w4, w0, [x10]
    ldrb w0, [x10]
    CHK(x0, 0x80)
    lduminab w4, w0, [x10]
    ldrb w0, [x10]
    CHK(x0, 5)
    mov w5, #0x8000
    strh w5, [x10, #2]
    mov w4, #1
    ldsminh w4, w0, [x16]
    CHK(x0, 0x8000)
    ldrh w0, [x10, #2]
    CHK(x0, 0x8000)
    ldsmaxah w4, w0, [x16]
    ldrh w0, [x10, #2]
    CHK(x0, 1)
    ldaddalh w5, w0, [x16]
    ldeorh w5, w0, [x16]
    ldrh w0, [x10, #2]
    CHK(x0, 1)
    mov w6, #0x80000000
    str w6, [x10, #4]
    ldsmax w4, w0, [x17]
    ldr w0, [x10, #4]
    CHK(x0, 1)
    ldumax w6, w0, [x17]
    ldr w0, [x10, #4]
    CHK(x0, 0x80000000)
    ldsminl w4, w0, [x17]
    ldr w0, [x10, #4]
    CHK(x0, 0x80000000)
    ldaddal w6, w0, [x17]
    CHK(x0, 0x80000000)
    ldr w0, [x10, #4]
    CHK(x0, 0)
    ldsetlb w4, w0, [x19]
    ldclrh w4, w0, [x18]
    ldr w0, [x10, #4]
    CHK(x0, 0x01000000)

    INT_OK
    bl a64t_dump
    mov x0, #0
    ldp x29, x30, [sp], #16
    ret

    .ltorg
    .bss
    .p2align 6
abuf: .space 128
