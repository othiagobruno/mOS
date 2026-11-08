/* a64cmp: nao-comparavel (falha no meio: o ARM permite registradores UNKNOWN; o QEMU escreve v0) */
/* frente JIT: LDP de Q cuja segunda leitura cruza para página não mapeada: v0/v1 inalterados;
 * depois LD1 de dois registradores cruzando (vai ao interpretador) não pode ser alcançado. */
    .text
    .globl main
main:
    adrp x10, endbuf
    add x10, x10, :lo12:endbuf
    sub x10, x10, #24
    movi v0.16b, #0x11
    movi v1.16b, #0x22
    ldp q0, q1, [x10]            /* q0 = [x10..+16) ok; q1 = [x10+16..+32) cruza */
    ret
    .bss
    .balign 65536
buf: .space 65536
endbuf:
