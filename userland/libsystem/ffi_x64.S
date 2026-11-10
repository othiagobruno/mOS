/* mOS libSystem x86_64 — chamada genérica com o quadro montado em C (NSInvocation).
 * void cx_x64_call(void *fn, cx_x64_frame_t *f);
 *   +0   x0..x5 (rdi..r9)   +48 nstack (múltiplo de 16)   +56 ponteiro da pilha
 *   +64  xmm0..xmm7        +192 rax   +200 rdx   +208 xmm0 na volta
 * al = 8 (variádico ou não: o callee não-variádico ignora). */
#if !defined(__aarch64__)
    .text
    .globl cx_x64_call
    .p2align 4
cx_x64_call:
    pushq %rbp
    movq %rsp, %rbp
    pushq %rbx
    pushq %r12
    movq %rdi, %r12
    movq %rsi, %rbx
    movq 48(%rbx), %rax
    subq %rax, %rsp
    testq %rax, %rax
    jz 1f
    cld
    movq 56(%rbx), %rsi
    movq %rsp, %rdi
    movq %rax, %rcx
    rep movsb
1:
    movups 64(%rbx), %xmm0
    movups 80(%rbx), %xmm1
    movups 96(%rbx), %xmm2
    movups 112(%rbx), %xmm3
    movups 128(%rbx), %xmm4
    movups 144(%rbx), %xmm5
    movups 160(%rbx), %xmm6
    movups 176(%rbx), %xmm7
    movq 0(%rbx), %rdi
    movq 8(%rbx), %rsi
    movq 16(%rbx), %rdx
    movq 24(%rbx), %rcx
    movq 32(%rbx), %r8
    movq 40(%rbx), %r9
    movb $8, %al
    callq *%r12
    movq %rax, 192(%rbx)
    movq %rdx, 200(%rbx)
    movups %xmm0, 208(%rbx)
    leaq -16(%rbp), %rsp
    popq %r12
    popq %rbx
    popq %rbp
    ret
    .section .note.GNU-stack,"",@progbits
#endif
