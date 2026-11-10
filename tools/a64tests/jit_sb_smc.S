/* superbloco: a instrução reescrita não é a primeira do bloco (está no fallthrough
 * de um B.cond). IC IVAU tem que invalidar o bloco inteiro. */
    .text
    .globl main
main:
    stp x29, x30, [sp, #-16]!
    adr x1, slot
    adr x3, patch
    blr x1
    cmp x0, #1
    b.ne 9f
    movz w2, #0x00A0          /* movz x0, #5 = 0xD28000A0 */
    movk w2, #0xD280, lsl #16
    str w2, [x3]
    dc cvau, x3
    dsb ish
    ic ivau, x3
    dsb ish
    isb
    blr x1
    cmp x0, #5
    b.ne 9f
    mov x0, #1
    adr x1, ok
    mov x2, #3
    bl a64t_write
    mov x0, #0
    b 8f
9:  mov x0, #1
8:  ldp x29, x30, [sp], #16
    ret
slot:
    cmp x1, #0
    b.eq 1f
patch:
    movz x0, #1
    ret
1:  movz x0, #2
    ret
ok: .ascii "ok\n"
