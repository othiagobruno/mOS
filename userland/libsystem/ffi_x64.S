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

/* libffi (ffi_mos.c): void cx_ffi_x64_call(void *fn, ffi_x64_frame_t *f). O quadro é o do cx_x64_call com mais
 *   +224 xmm1 na volta   +240 st(0) na volta (long double)   +256 al (nº de SSE)   +264 1 = guardar st(0) */
    .text
    .globl cx_ffi_x64_call
    .p2align 4
cx_ffi_x64_call:
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
    movq 256(%rbx), %rax
    callq *%r12
    movq %rax, 192(%rbx)
    movq %rdx, 200(%rbx)
    movups %xmm0, 208(%rbx)
    movups %xmm1, 224(%rbx)
    cmpq $0, 264(%rbx)
    je 2f
    fstpt 240(%rbx)
2:
    leaq -16(%rbp), %rsp
    popq %r12
    popq %rbx
    popq %rbp
    ret

/* entrada dos closures da libffi: o trampolim deixou o cabeçalho do closure em r10. Guarda rdi..r9 e xmm0..7 num
 * ffi_x64_regs_t na pilha, chama cx_ffi_x64_closure_inner(meta, regs, args da pilha) e carrega o retorno
 * (rax/rdx/xmm0/xmm1, ou st(0) quando ela devolve 2). */
    .globl cx_ffi_x64_closure_entry
    .p2align 4
cx_ffi_x64_closure_entry:
    pushq %rbp
    movq %rsp, %rbp
    subq $256, %rsp
    movq %rdi, 0(%rsp)
    movq %rsi, 8(%rsp)
    movq %rdx, 16(%rsp)
    movq %rcx, 24(%rsp)
    movq %r8, 32(%rsp)
    movq %r9, 40(%rsp)
    movups %xmm0, 48(%rsp)
    movups %xmm1, 64(%rsp)
    movups %xmm2, 80(%rsp)
    movups %xmm3, 96(%rsp)
    movups %xmm4, 112(%rsp)
    movups %xmm5, 128(%rsp)
    movups %xmm6, 144(%rsp)
    movups %xmm7, 160(%rsp)
    movq %r10, %rdi
    movq %rsp, %rsi
    leaq 16(%rbp), %rdx
    callq cx_ffi_x64_closure_inner
    cmpl $2, %eax
    je 3f
    movq 176(%rsp), %rax
    movq 184(%rsp), %rdx
    movups 192(%rsp), %xmm0
    movups 208(%rsp), %xmm1
    leave
    ret
3:
    fldt 224(%rsp)
    leave
    ret
    .section .note.GNU-stack,"",@progbits
#endif
