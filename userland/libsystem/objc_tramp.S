/*
 * mOS libSystem — trampolins de imp_implementationWithBlock (objcrt.c).
 * Um IMP é chamado como imp(self, _cmd, args...); o bloco quer invoke(bloco, self, args...). Cada trampolim troca
 * os dois primeiros argumentos e salta para o invoke: rsi = self, rdi = bloco, jmp [bloco + 16]. Os demais
 * registradores (rdx, rcx, r8, r9, xmm0-7, al) e a pilha ficam intactos. O pool é fixo e pré-montado (sem gerar
 * código em tempo de execução): CX_TRAMP_N entradas de 16 bytes; a tabela cx_imp_blocks guarda o bloco de cada uma.
 */
    .set CX_TRAMP_N, 4096

    .data
    .balign 16
    .globl cx_imp_blocks
cx_imp_blocks:
    .fill CX_TRAMP_N, 8, 0

    .text
    .balign 16
    .globl cx_imp_tramps
cx_imp_tramps:
    .set i, 0
    .rept CX_TRAMP_N
    mov %rdi, %rsi
    movq cx_imp_blocks + 8 * i(%rip), %rdi
    jmp *16(%rdi)
    .balign 16, 0xcc
    .set i, i + 1
    .endr
    .globl cx_imp_tramps_end
cx_imp_tramps_end:
