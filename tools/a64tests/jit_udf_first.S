/* frente JIT: palavra não alocada como 1ª instrução de um bloco → UDF com pc nela */
    .text
    .globl main
main:
    mov x1, #3
    b 1f
1:  .inst 0x02a96ea9
    .inst 0x1a0f0077
    ret
