/* frente JIT: palavra do espaço 3same_extra (bit 15 = 1) com size = 01 e U = 1, opcode 0011: não alocada,
 * tem que dar UDF nos dois motores. O despacho do JIT confundia com o 3same de FP half (FMUL 4H) por não
 * olhar o bit 15 e executava em silêncio. Achado pelo tools/a64fuzz_host.c (simdv.3same_extra). */
    .text
    .globl main
main:
    movi v0.16b, #1
    movi v1.16b, #2
    movi v12.16b, #3
    .inst 0x2e4e9cac
    mov x0, #0
    ret
