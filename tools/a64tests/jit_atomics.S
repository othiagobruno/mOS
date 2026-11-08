/* frente JIT: exclusivos e LSE nativos — laços LDXR/STXR que acertam, STXR que falha (monitor
 * desarmado, endereço/tamanho diferentes, CLREX), CAS que troca e que não troca, LDADD/LDCLR/
 * LDEOR/LDSET/SWP em todos os tamanhos. Despeja o estado e o buffer. */
#define A(off) add x11, x10, off
    .text
    .globl main
main:
    stp x29, x30, [sp, #-16]!
    adrp x10, buf
    add x10, x10, :lo12:buf
    mov x9, #20
1:  ldxr x0, [x10]               /* incremento atômico, 20 vezes */
    add x0, x0, #3
    stxr w1, x0, [x10]
    cbnz w1, 1b
    subs x9, x9, #1
    b.ne 1b
    A(8)
    ldaxrb w2, [x11]
    add w2, w2, #1
    stlxrb w3, w2, [x11]         /* acerta: w3 = 0 */
    stxrb w4, w2, [x11]          /* monitor desarmado: w4 = 1 */
    A(16)
    ldxrh w5, [x11]
    stxr w6, x5, [x11]           /* tamanho diferente: falha */
    A(24)
    ldxr w7, [x11]
    clrex
    stxr w8, w7, [x11]           /* depois de CLREX: falha */
    ldxr x12, [x10]
    A(32)
    stxr w9, x12, [x11]          /* endereço diferente: falha (w9 = status) */
    mov x13, #60
    mov x14, #0x77
    cas x13, x14, [x10]          /* troca (buf[0] = 60) */
    mov x15, #5
    casal x15, x14, [x10]        /* não troca: x15 = 0x77 */
    mov w16, #0x11
    A(40)
    ldaddb w16, w17, [x11]
    swpb w16, w25, [x11]
    A(42)
    ldaddh w16, w18, [x11]
    ldeorh w16, w23, [x11]
    A(44)
    ldadd w16, w19, [x11]
    ldset w16, w24, [x11]
    A(48)
    ldaddal x16, x20, [x11]
    mov x21, #0xf0
    ldclr x21, x22, [x11]
    A(56)
    swpal x16, x26, [x11]
    ldapr x27, [x11]
    stadd x16, [x11]
    movn x2, #0                  /* -1 */
    A(0)
    ldsmax x2, x3, [x11]         /* max(60, -1) = 60 */
    ldumax x2, x4, [x11]         /* umax = 0xff..ff */
    ldsminh w2, w5, [x11]
    lduminb w16, w6, [x11]
    A(24)
    ldsmaxal w16, w7, [x11]
    ldumin x16, x8, [x11]
    adds x28, x27, x26           /* flags vivas até o dump */
    ldp q0, q1, [x10]
    ldp q2, q3, [x10, #32]
    bl a64t_dump
    mov x0, #0
    ldp x29, x30, [sp], #16
    ret
    .data
    .balign 64
buf: .quad 0, 0xfe, 0x1234, 0x55, 0, 0x01020304050607f8, 0xffff00000000ff00, 9
