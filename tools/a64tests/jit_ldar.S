/* frente JIT: LDAR de 8 bytes que atravessa um bloco de 16 bytes (nAA = 0) → FAULT de alinhamento
 * com exit_info = endereço; antes disso, LDAR/STLR alinhados funcionam. */
    .text
    .globl main
main:
    adrp x10, buf
    add x10, x10, :lo12:buf
    mov x0, #0x55
    stlr x0, [x10]
    ldar x1, [x10]
    add x11, x10, #4
    stlr w1, [x11]
    ldar w2, [x11]
    add x12, x10, #12
    adds x3, x1, x2
    ldar x4, [x12]               /* 12 + 8 > 16: falha */
    ret
    .data
    .balign 64
buf: .space 64
