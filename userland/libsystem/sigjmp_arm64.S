/* mOS libSystem arm64 — setjmp/longjmp e sigsetjmp/siglongjmp, versão arm64 de sigjmp.S.
 * Layout do jmp_buf do Darwin arm64 (_JBLEN = 48 ints, 192 bytes; sigjmp_buf 49): x19..x28 em +0x00, fp/lr em +0x50,
 * sp em +0x60, d8..d15 em +0x70, a máscara (u32, no formato do kernel do mOS) em +0xB0 e "máscara salva?" (u32) em +0xB8.
 * A Apple embaralha fp/lr/sp com um segredo do processo; aqui não (ninguém fora do libSystem lê o jmp_buf).
 * Como no macOS: setjmp salva a máscara, _setjmp não, sigsetjmp(env, savemask) só se savemask != 0; longjmp e
 * siglongjmp restauram o que foi salvo, _longjmp nunca. A máscara vem de SYS_SIG (58) / MOS_SIGOP_MASK (2). */
#if defined(__aarch64__)
    .text
    .globl setjmp, _setjmp, sigsetjmp, longjmp, _longjmp, siglongjmp
    .p2align 2
setjmp:
    mov w1, #1
    b sigsetjmp
_setjmp:
    mov w1, #0
sigsetjmp:
    stp x19, x20, [x0, #0x00]
    stp x21, x22, [x0, #0x10]
    stp x23, x24, [x0, #0x20]
    stp x25, x26, [x0, #0x30]
    stp x27, x28, [x0, #0x40]
    stp x29, x30, [x0, #0x50]
    mov x9, sp
    str x9, [x0, #0x60]
    stp d8, d9, [x0, #0x70]
    stp d10, d11, [x0, #0x80]
    stp d12, d13, [x0, #0x90]
    stp d14, d15, [x0, #0xA0]
    str w1, [x0, #0xB8]
    cbz w1, 2f
    mov x10, x0
    mov x0, #2                 /* MOS_SIGOP_MASK */
    mov x1, #1                 /* SIG_BLOCK com conjunto NULL: só lê */
    mov x2, #0
    add x3, x10, #0xB0         /* antiga: a máscara atual */
    mov x8, #58                /* SYS_SIG */
    svc #0x4d4f
    tbz x0, #63, 1f
    str wzr, [x10, #0xB8]      /* kernel sem SYS_SIG (harness): não restaura nada */
1:  mov x0, x10
2:  mov w0, #0
    ret

longjmp:
siglongjmp:
    ldr w9, [x0, #0xB8]
    cbz w9, _longjmp
    mov x10, x0
    mov w11, w1
    mov x0, #2                 /* MOS_SIGOP_MASK */
    mov x1, #3                 /* SIG_SETMASK */
    add x2, x10, #0xB0
    mov x3, #0
    mov x8, #58                /* SYS_SIG */
    svc #0x4d4f
    mov x0, x10
    mov w1, w11
_longjmp:
    ldp x19, x20, [x0, #0x00]
    ldp x21, x22, [x0, #0x10]
    ldp x23, x24, [x0, #0x20]
    ldp x25, x26, [x0, #0x30]
    ldp x27, x28, [x0, #0x40]
    ldp x29, x30, [x0, #0x50]
    ldr x9, [x0, #0x60]
    mov sp, x9
    ldp d8, d9, [x0, #0x70]
    ldp d10, d11, [x0, #0x80]
    ldp d12, d13, [x0, #0x90]
    ldp d14, d15, [x0, #0xA0]
    cmp w1, #0
    csinc w0, w1, wzr, ne      /* longjmp(env, 0) volta 1 */
    ret
#endif
