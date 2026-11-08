/* frente JIT: falha num store com as flags no EFLAGS em modo SUB (C do ARM = !CF) e o
 * registrador de destino de uma instrução anterior ainda só no cache. */
    .text
    .globl main
main:
    mov x0, #5
    mov x1, #7
    sub x2, x1, x0
    cmp x0, x1                   /* 5 - 7: N=1 C=0 (borrow) */
    mov x9, #0x40
    str x2, [x9, x0, lsl #3]     /* falha */
    mov x2, #0
    ret
