/*
 * mOS libSystem arm64 — trampolins de imp_implementationWithBlock (objcrt.c), versão arm64 de objc_tramp.S.
 * imp(self, _cmd, args...) -> invoke(bloco, self, args...): x1 = self, x0 = bloco, br [bloco + 16]. Os demais
 * registradores (x2..x8, q0..q7) e a pilha ficam intactos; só x16/x17 (IP0/IP1, livres pelo AAPCS) são usados.
 * CX_TRAMP_N entradas de 32 bytes (CX_TRAMP_SZ em objcrt.c); cx_imp_blocks guarda o bloco de cada uma.
 */
#if defined(__aarch64__)
    .set CX_TRAMP_N, 4096

    .data
    .balign 16
    .globl cx_imp_blocks
cx_imp_blocks:
    .fill CX_TRAMP_N, 8, 0

    .text
    .balign 32
    .globl cx_imp_tramps
cx_imp_tramps:
    .set i, 0
    .rept CX_TRAMP_N
    mov x1, x0
    adrp x16, cx_imp_blocks + 8 * i
    ldr x0, [x16, :lo12:cx_imp_blocks + 8 * i]
    ldr x16, [x0, #16]
    br x16
    .balign 32
    .set i, i + 1
    .endr
    .globl cx_imp_tramps_end
cx_imp_tramps_end:
#endif
