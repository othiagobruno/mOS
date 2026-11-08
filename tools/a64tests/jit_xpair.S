/* frente JIT: LDXP/STXP (W e X, laço que acerta e casos que falham) e CASP/CASPAL (W e X, troca
 * e não troca) nativos com lock cmpxchg16b / cmpxchg de 8 bytes. Despeja estado e buffer. */
    .text
    .globl main
main:
    stp x29, x30, [sp, #-16]!
    adrp x10, buf
    add x10, x10, :lo12:buf
    mov x9, #10
1:  ldaxp x0, x1, [x10]          /* incrementa o par 10 vezes */
    add x0, x0, #1
    add x1, x1, #2
    stlxp w2, x0, x1, [x10]
    cbnz w2, 1b
    subs x9, x9, #1
    b.ne 1b
    add x11, x10, #16
    ldxp w3, w4, [x11]
    add w3, w3, #5
    stxp w5, w3, w4, [x11]       /* acerta */
    stxp w6, w3, w4, [x11]       /* monitor desarmado: falha */
    ldxp x7, x8, [x10]
    stxp w12, x7, x8, [x11]      /* endereço diferente: falha */
    mov x14, x0
    mov x15, x1
    mov x16, #0x111
    mov x17, #0x222
    caspal x14, x15, x16, x17, [x10]   /* troca */
    mov x18, #1
    mov x19, #2
    casp x18, x19, x16, x17, [x10]     /* não troca: x18:x19 = 0x111:0x222 */
    ldr w20, [x11]
    ldr w21, [x11, #4]
    casp w20, w21, w16, w17, [x11]     /* troca (32 bits) */
    casp w22, w23, w16, w17, [x11]     /* não troca */
    adds x24, x18, x19
    ldp q0, q1, [x10]
    bl a64t_dump
    mov x0, #0
    ldp x29, x30, [sp], #16
    ret
    .data
    .balign 64
buf: .quad 100, 200, 0x0000000700000003, 9
