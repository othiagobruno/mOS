/* frente JIT: LDAPUR/STLUR que atravessam um bloco de 16 bytes (nAA = 0) dão FAULT de alinhamento com
 * exit_info = endereço, como no interpretador; os alinhados e os que só tocam o mesmo bloco seguem.
 * O STLUR de 4 bytes em +14 tem que falhar sem gravar nada (regra de reinício). */
    .text
    .globl main
main:
    adrp x10, buf
    add x10, x10, :lo12:buf
    mov x0, #0x55
    stlur x0, [x10, #8]                 /* 8..15: mesmo bloco */
    ldapur x1, [x10, #8]
    stlurh w1, [x10, #14]               /* 14..15 */
    ldapursh x2, [x10, #14]
    stlurb w2, [x10, #15]
    movn w3, #0
    stlur w3, [x10, #14]                /* 14..17: falha */
    mov x4, #1
    ret
    .data
    .balign 64
buf: .space 64
