/* superbloco: a falta está no fallthrough, não na primeira instrução do bloco.
 * pc do FAULT tem que ser o ldr (igual ao interpretador). */
    .text
    .globl main
main:
    mov x0, #1
    mov x2, #11
    cmp x0, #0
    b.eq 9f
    mov x3, #22
    mov x10, #0
    ldr x4, [x10]
    mov x5, #33
9:  ret
