/* mOS — kjmp_set / kjmp_go: setjmp/longjmp do kernel (convenção Microsoft x64).
 * Buffer (kjmp_buf_t, 256 bytes, alinhado a 16): rbx rbp rdi rsi r12 r13 r14 r15 rsp rip, depois xmm6..xmm15
 * (preservados pelo chamado no ABI Win64) e o MXCSR. Usado por ui/appguard.c. */
    .text

    .globl kjmp_set
kjmp_set:                       /* (rcx = buf) -> 0; volta de novo com o valor de kjmp_go */
    mov %rbx, 0(%rcx)
    mov %rbp, 8(%rcx)
    mov %rdi, 16(%rcx)
    mov %rsi, 24(%rcx)
    mov %r12, 32(%rcx)
    mov %r13, 40(%rcx)
    mov %r14, 48(%rcx)
    mov %r15, 56(%rcx)
    lea 8(%rsp), %rdx           /* rsp de quem chamou, depois do ret */
    mov %rdx, 64(%rcx)
    mov (%rsp), %rdx            /* endereço de retorno */
    mov %rdx, 72(%rcx)
    movdqu %xmm6, 80(%rcx)
    movdqu %xmm7, 96(%rcx)
    movdqu %xmm8, 112(%rcx)
    movdqu %xmm9, 128(%rcx)
    movdqu %xmm10, 144(%rcx)
    movdqu %xmm11, 160(%rcx)
    movdqu %xmm12, 176(%rcx)
    movdqu %xmm13, 192(%rcx)
    movdqu %xmm14, 208(%rcx)
    movdqu %xmm15, 224(%rcx)
    stmxcsr 240(%rcx)
    xor %eax, %eax
    ret

    .globl kjmp_go
kjmp_go:                        /* (rcx = buf, edx = valor != 0): não volta */
    mov 0(%rcx), %rbx
    mov 8(%rcx), %rbp
    mov 16(%rcx), %rdi
    mov 24(%rcx), %rsi
    mov 32(%rcx), %r12
    mov 40(%rcx), %r13
    mov 48(%rcx), %r14
    mov 56(%rcx), %r15
    movdqu 80(%rcx), %xmm6
    movdqu 96(%rcx), %xmm7
    movdqu 112(%rcx), %xmm8
    movdqu 128(%rcx), %xmm9
    movdqu 144(%rcx), %xmm10
    movdqu 160(%rcx), %xmm11
    movdqu 176(%rcx), %xmm12
    movdqu 192(%rcx), %xmm13
    movdqu 208(%rcx), %xmm14
    movdqu 224(%rcx), %xmm15
    ldmxcsr 240(%rcx)
    mov %edx, %eax
    test %eax, %eax
    jnz 1f
    inc %eax
1:  mov 64(%rcx), %rsp
    jmp *72(%rcx)
