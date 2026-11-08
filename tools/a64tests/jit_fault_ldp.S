/* frente JIT: LDP cujo primeiro destino é a base e cuja segunda leitura falha: a base não pode
 * ter sido trocada pelo primeiro valor. */
    .text
    .globl main
main:
    adrp x10, endbuf
    add x10, x10, :lo12:endbuf
    sub x10, x10, #8
    movz x3, #0xbeef
    str x3, [x10]
    mov x11, x10
    ldp x10, x12, [x10]          /* x10 = base = 1º destino; [base+8] não mapeado */
    ret
    .bss
    .balign 65536
buf: .space 65536
endbuf:
