/* Duas funções na mesma página. Reescreve só a primeira com DC CVAU + IC IVAU e confere que a
 * segunda continua valendo. Depois reescreve as duas sem manutenção (rede de segurança do JIT).
 * O harness oficial é o a64test no Windows; este arquivo entra no glob de tools/a64tests. */
    .text
    .globl main
main:
    stp x29, x30, [sp, #-16]!
    stp x19, x20, [sp, #-16]!
    stp x21, x22, [sp, #-16]!
    adrp x21, fn_a
    add x21, x21, :lo12:fn_a
    adrp x22, fn_b
    add x22, x22, :lo12:fn_b
    blr x21
    mov x19, x0
    blr x22
    add x19, x19, x0
    cmp x19, #11
    b.ne 9f
    movz w1, #0x0040
    movk w1, #0xD280, lsl #16
    str w1, [x21]
    dc cvau, x21
    dsb ish
    ic ivau, x21
    dsb ish
    isb
    blr x21
    mov x19, x0
    blr x22
    add x19, x19, x0
    cmp x19, #12
    b.ne 9f
    movz w1, #0x0060
    movk w1, #0xD280, lsl #16
    str w1, [x21]
    isb
    blr x21
    mov x19, x0
    blr x22
    add x19, x19, x0
    cmp x19, #13
    b.ne 9f
    movz w1, #0x0280
    movk w1, #0xD280, lsl #16
    str w1, [x22]
    isb
    blr x21
    mov x19, x0
    blr x22
    add x19, x19, x0
    cmp x19, #23
    b.ne 9f
    mov x0, #1
    adr x1, okmsg
    mov x2, #3
    bl a64t_write
    mov x0, #0
    b 8f
9:  mov x0, x19
8:  ldp x21, x22, [sp], #16
    ldp x19, x20, [sp], #16
    ldp x29, x30, [sp], #16
    ret
okmsg: .ascii "ok\n"
    .data
    .balign 4096
fn_a:
    movz x0, #1
    ret
    .balign 128
fn_b:
    movz x0, #10
    ret
