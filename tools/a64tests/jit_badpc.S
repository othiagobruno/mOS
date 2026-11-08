/* frente JIT: desvio indireto para endereço desalinhado e não canônico: a busca falha em pc
 * (FAULT com pc = alvo), igual ao interpretador; o JIT não pode ficar girando. */
    .text
    .globl main
main:
    mov x1, #5
    movn x0, #0x4c               /* 0xffffffffffffffb3 */
    br x0
