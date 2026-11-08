/* mOS libSystem arm64 — chamada genérica com o quadro montado em C (NSInvocation, foundation_extra.c).
 * void cx_a64_call(void *fn, cx_a64_frame_t *f); quadro:
 *   +0 x0..x7   +64 x8 (resultado indireto)   +72 bytes de pilha (múltiplo de 16)   +80 ponteiro da pilha montada
 *   +96 v0..v7 (16 bytes cada)   +224 x0/x1 na volta   +240 q0..q3 na volta */
#if defined(__aarch64__)
    .text
    .globl cx_a64_call
    .p2align 4
cx_a64_call:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    stp x19, x20, [sp, #16]
    mov x19, x1
    mov x20, x0
    ldr x9, [x19, #72]
    sub sp, sp, x9
    ldr x10, [x19, #80]
    mov x11, sp
1:  cbz x9, 2f
    ldp x12, x13, [x10], #16
    stp x12, x13, [x11], #16
    sub x9, x9, #16
    b 1b
2:  ldp q0, q1, [x19, #96]
    ldp q2, q3, [x19, #128]
    ldp q4, q5, [x19, #160]
    ldp q6, q7, [x19, #192]
    ldp x0, x1, [x19, #0]
    ldp x2, x3, [x19, #16]
    ldp x4, x5, [x19, #32]
    ldp x6, x7, [x19, #48]
    ldr x8, [x19, #64]
    blr x20
    stp x0, x1, [x19, #224]
    stp q0, q1, [x19, #240]
    stp q2, q3, [x19, #272]
    mov sp, x29
    ldp x19, x20, [sp, #16]
    ldp x29, x30, [sp], #32
    ret
#endif
