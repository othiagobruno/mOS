/* superbloco: laço contado. A soma 1..1000 tem que ser 500500 nos dois motores. */
    .text
    .globl main
main:
    str x30, [sp, #-16]!            /* o bl a64t_write abaixo troca o x30 */
    mov x0, #0
    mov x1, #1000
1:  add x0, x0, x1
    subs x1, x1, #1
    b.ne 1b
    movz x2, #0xA314
    movk x2, #0x7, lsl #16          /* 500500 = 0x7A314 */
    cmp x0, x2
    b.ne 9f
    mov x0, #1
    adr x1, ok
    mov x2, #3
    bl a64t_write
    mov x0, #0
    ldr x30, [sp], #16
    ret
9:  mov x0, #1
    ldr x30, [sp], #16
    ret
ok: .ascii "ok\n"
