/* frente JIT: STP com pré-índice cuja segunda metade cai numa página não mapeada (fim do .bss,
 * alinhado a 64 KiB). Registradores (inclusive a base com writeback) não podem mudar; a primeira
 * metade pode ter sido escrita (o ARM permite). Flags do teste anterior vivas em memória. */
    .text
    .globl main
main:
    adrp x10, endbuf
    add x10, x10, :lo12:endbuf
    sub x10, x10, #24            /* [x10+16] = último dword mapeado; [x10+24] não mapeado */
    mov x0, #0x1111
    mov x1, #0x2222
    cmn x0, #1                   /* flags: N=0 Z=0 C=0 V=0 */
    stp x0, x1, [x10, #16]!      /* segunda store falha */
    mov x0, #0
    ret
    .bss
    .balign 65536
buf: .space 65536
endbuf:
