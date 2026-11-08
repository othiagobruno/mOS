/* frente JIT: LDP/STP/LDPSW em todos os alinhamentos (16, 8, 4, atravessando 16) e com SP; o
 * padrão de acessos segue o do interpretador (FEAT_LSE2). Despeja o estado e o conteúdo do buffer. */
    .text
    .globl main
main:
    stp x29, x30, [sp, #-16]!
    adrp x10, buf
    add x10, x10, :lo12:buf
    movz x0, #0x1111, lsl #48
    movk x0, #0x2222
    movz x1, #0x3333, lsl #32
    movk x1, #0x4444
    mov x9, #0
1:  add x11, x10, x9
    stp x0, x1, [x11]
    ldp x2, x3, [x11]
    stp w1, w0, [x11, #16]
    ldp w4, w5, [x11, #16]
    ldpsw x6, x7, [x11, #12]
    add x20, x20, x2
    eor x21, x21, x3
    add x22, x22, x4
    eor x23, x23, x5
    add x24, x24, x6
    add x25, x25, x7
    add x9, x9, #4
    cmp x9, #48
    b.ne 1b
    stp x0, x1, [sp, #-32]!
    ldp x12, x13, [sp], #32
    stp w0, w1, [sp, #-16]!
    ldp w14, w15, [sp], #16
    ldp q0, q1, [x10]
    ldp q2, q3, [x10, #32]
    ldp d4, d5, [x10, #8]
    ldp s6, s7, [x10, #4]
    bl a64t_dump
    mov x0, #0
    ldp x29, x30, [sp], #16
    ret
    .data
    .balign 64
buf: .space 128
