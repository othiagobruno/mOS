/* smoke test do pipeline: escreve "ok\n" e sai com 0 */
    .text
    .globl main
main:
    stp x29, x30, [sp, #-16]!
    mov x0, #1
    adr x1, msg
    mov x2, #3
    bl a64t_write
    mov x0, #0
    ldp x29, x30, [sp], #16
    ret
msg: .ascii "ok\n"
