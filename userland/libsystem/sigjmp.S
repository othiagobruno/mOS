/* mOS libSystem — setjmp/longjmp e sigsetjmp/siglongjmp (bash, readline). O jmp_buf do macOS tem
 * 37 ints (148 bytes) e o sigjmp_buf 38; aqui usamos os 64 primeiros: rbx rbp r12 r13 r14 r15 rsp rip.
 * A máscara de sinais não é salva (o mOS não tem máscara por thread). */
    .text
    .globl setjmp, _setjmp, sigsetjmp, longjmp, _longjmp, siglongjmp
setjmp:
_setjmp:
sigsetjmp:
    mov %rbx, 0(%rdi)
    mov %rbp, 8(%rdi)
    mov %r12, 16(%rdi)
    mov %r13, 24(%rdi)
    mov %r14, 32(%rdi)
    mov %r15, 40(%rdi)
    lea 8(%rsp), %rdx          /* rsp do chamador depois do ret */
    mov %rdx, 48(%rdi)
    mov (%rsp), %rdx           /* endereço de volta */
    mov %rdx, 56(%rdi)
    xor %eax, %eax
    ret
longjmp:
_longjmp:
siglongjmp:
    mov %esi, %eax
    test %eax, %eax
    jnz 1f
    inc %eax                   /* longjmp(env, 0) volta 1 */
1:  mov 0(%rdi), %rbx
    mov 8(%rdi), %rbp
    mov 16(%rdi), %r12
    mov 24(%rdi), %r13
    mov 32(%rdi), %r14
    mov 40(%rdi), %r15
    mov 48(%rdi), %rsp
    jmp *56(%rdi)
