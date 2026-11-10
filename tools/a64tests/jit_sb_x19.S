/* superbloco: X19 (callee-saved) sobrevive a BL de uma folha que não o toca,
 * e o valor de retorno em X0 é o da folha. */
    .text
    .globl main
main:
    stp x29, x30, [sp, #-16]!
    mov x19, #42
    bl leaf
    cmp x0, #7
    b.ne 9f
    cmp x19, #42
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
leaf:
    mov x0, #7
    add x0, x0, #0
    ret
ok: .ascii "ok\n"
