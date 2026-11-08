/* frente JIT: ST1 de dois registradores com a segunda metade fora do mapa (interpretador dentro do
 * JIT: o estado tem que vir sincronizado), base com writeback inalterada. */
    .text
    .globl main
main:
    adrp x10, endbuf
    add x10, x10, :lo12:endbuf
    sub x10, x10, #16
    movi v2.16b, #0x33
    movi v3.16b, #0x44
    mov x4, #77
    adds x5, x4, x4
    st1 {v2.16b, v3.16b}, [x10], #32
    ret
    .bss
    .balign 65536
buf: .space 65536
endbuf:
