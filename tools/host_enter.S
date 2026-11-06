/* mOS — entra no código de usuário (System V) a partir do Windows e volta no exit (tools/machotest.c) */
    .text
    .globl host_enter
host_enter:                     /* rcx = entrada, rdx = rdi do app, r8 = rsp do app */
    push %rbp
    push %rbx
    push %rdi
    push %rsi
    push %r12
    push %r13
    push %r14
    push %r15
    sub $168, %rsp
    movdqu %xmm6, 0(%rsp)
    movdqu %xmm7, 16(%rsp)
    movdqu %xmm8, 32(%rsp)
    movdqu %xmm9, 48(%rsp)
    movdqu %xmm10, 64(%rsp)
    movdqu %xmm11, 80(%rsp)
    movdqu %xmm12, 96(%rsp)
    movdqu %xmm13, 112(%rsp)
    movdqu %xmm14, 128(%rsp)
    movdqu %xmm15, 144(%rsp)
    mov %rsp, host_saved_rsp(%rip)
    mov %rdx, %rdi
    mov %r8, %rsp
    jmp *%rcx

    .globl host_exit             /* o tratador de exceção põe rip aqui e rax = código */
host_exit:
    mov host_saved_rsp(%rip), %rsp
    movdqu 0(%rsp), %xmm6
    movdqu 16(%rsp), %xmm7
    movdqu 32(%rsp), %xmm8
    movdqu 48(%rsp), %xmm9
    movdqu 64(%rsp), %xmm10
    movdqu 80(%rsp), %xmm11
    movdqu 96(%rsp), %xmm12
    movdqu 112(%rsp), %xmm13
    movdqu 128(%rsp), %xmm14
    movdqu 144(%rsp), %xmm15
    add $168, %rsp
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rsi
    pop %rdi
    pop %rbx
    pop %rbp
    ret

    .globl host_jump             /* threads novas: rcx = entrada, rdx = rdi do app, r8 = rsp do app (não volta) */
host_jump:
    mov %rdx, %rdi
    mov %r8, %rsp
    jmp *%rcx

    .data
    .globl host_saved_rsp
host_saved_rsp:
    .quad 0
