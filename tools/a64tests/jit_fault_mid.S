/* frente JIT: falha de página no meio de um bloco com registradores sujos no cache e flags no
 * EFLAGS (modo ADD). O estado despejado depois de "FAULT" tem que ser idêntico ao do interpretador:
 * pc = o ldr, x2/x3/x4 já escritos, x5 não, NZCV do adds. */
    .text
    .globl main
main:
    mov x0, #1
    mov x1, #2
    add x2, x0, x1
    movn x3, #0
    adds x4, x3, #5              /* carry: C=1, Z=0 */
    mov x10, #8                  /* página nula: falha */
    ldr x6, [x10, #16]
    mov x5, #99
    ret
