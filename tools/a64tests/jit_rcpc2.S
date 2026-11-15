/* frente JIT: emissões nativas de docs/jit-pendencias.md item 1, com autoverificação.
 *   LDAPUR/STLUR (FEAT_LRCPC2) de 1, 2, 4 e 8 bytes, com sinal para W e X, deslocamento negativo, SP como base,
 *   WZR/XZR como origem e destino; CRC32 e CRC32C (B/H/W/X; valores de referência calculados bit a bit);
 *   PACIA/PACIZA/AUTIA/XPACI/XPACD/PACDB/AUTDZB com o algoritmo "identidade".
 * Tudo dentro de um laço de 3 voltas, para o JIT traduzir e reexecutar os mesmos blocos. */
#include "int_chk.h"
    .text
    .globl main
main:
    stp x29, x30, [sp, #-16]!
    mov x19, #3
volta:
    adrp x10, buf
    add x10, x10, :lo12:buf
    add x11, x10, #32                   /* base no meio do buffer para os deslocamentos negativos */

    /* STLUR de cada tamanho, LDAPUR de volta com e sem extensão de sinal */
    movz x0, #0xF1F2, lsl #48
    movk x0, #0xF3F4, lsl #32
    movk x0, #0x8586, lsl #16
    movk x0, #0x8788
    stlur x0, [x11, #-16]
    ldapur x1, [x11, #-16]
    CHKR(x1, x0)
    stlur w0, [x11, #-8]
    ldapur w2, [x11, #-8]
    CHK(x2, 0x85868788)
    ldapursw x3, [x11, #-8]
    CHK(x3, 0xFFFFFFFF85868788)
    stlurh w0, [x11, #-4]
    ldapurh w4, [x11, #-4]
    CHK(x4, 0x8788)
    ldapursh w5, [x11, #-4]
    CHK(x5, 0xFFFF8788)
    ldapursh x6, [x11, #-4]
    CHK(x6, 0xFFFFFFFFFFFF8788)
    stlurb w0, [x11, #-1]
    ldapurb w7, [x11, #-1]
    CHK(x7, 0x88)
    ldapursb w8, [x11, #-1]
    CHK(x8, 0xFFFFFF88)
    ldapursb x9, [x11, #-1]
    CHK(x9, 0xFFFFFFFFFFFFFF88)
    mov w12, #0x7F
    stlurb w12, [x11, #3]
    ldapursb x13, [x11, #3]
    CHK(x13, 0x7F)

    /* origem XZR/WZR zera; destino XZR descarta */
    stlur xzr, [x11, #-16]
    ldapur x14, [x11, #-16]
    CHK(x14, 0)
    stlur x0, [x11, #8]
    stlur wzr, [x11, #8]
    ldapur x15, [x11, #8]
    movz x16, #0xF1F2, lsl #48
    movk x16, #0xF3F4, lsl #32
    CHKR(x15, x16)
    ldapur xzr, [x11, #8]

    /* SP como base (alinhado: SA0 não dispara) */
    sub sp, sp, #32
    stlur x0, [sp, #8]
    ldapur x17, [sp, #8]
    CHKR(x17, x0)
    ldapursw x18, [sp, #12]
    CHK(x18, 0xFFFFFFFFF1F2F3F4)
    add sp, sp, #32

    /* CRC32 e CRC32C com acumulador 0xFFFFFFFF e dado 0x0123456789ABCDEF */
    mov w20, #-1
    movz x21, #0x0123, lsl #48
    movk x21, #0x4567, lsl #32
    movk x21, #0x89AB, lsl #16
    movk x21, #0xCDEF
    crc32cb w22, w20, w21
    CHK(x22, 0x10a13890)
    crc32ch w22, w20, w21
    CHK(x22, 0xee1d3738)
    crc32cw w22, w20, w21
    CHK(x22, 0xee8c8012)
    crc32cx w22, w20, x21
    CHK(x22, 0x9a4f27dc)
    crc32b w23, w20, w21
    CHK(x23, 0x1d48ef9b)
    crc32h w23, w20, w21
    CHK(x23, 0x8215bc2e)
    crc32w w23, w20, w21
    CHK(x23, 0x10472a38)
    crc32x w23, w20, x21
    CHK(x23, 0xbbc41db8)
    /* bits altos de Wn/Wm ignorados, resultado zera os 32 de cima; WZR como acumulador */
    movn x24, #0
    crc32cw w22, w24, w21
    CHK(x22, 0xee8c8012)
    crc32cb w22, wzr, w21
    mov w12, #0
    crc32cb w13, w12, w21
    CHKR(x22, x13)

    /* PAC "identidade" */
    mov x0, x10
    pacia x0, x11
    CHKR(x0, x10)
    autia x0, x11
    CHKR(x0, x10)
    paciza x0
    xpaci x0
    CHKR(x0, x10)
    pacdb x0, sp
    autdzb x0
    xpacd x0
    CHKR(x0, x10)

    subs x19, x19, #1
    b.ne volta
    INT_OK
    mov x0, #0
    ldp x29, x30, [sp], #16
    ret

    .data
    .balign 64
buf: .space 64
