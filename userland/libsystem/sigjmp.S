/* mOS libSystem — setjmp/longjmp e sigsetjmp/siglongjmp (bash, readline). O jmp_buf do macOS tem
 * 37 ints (148 bytes) e o sigjmp_buf 38; aqui usamos os 72 primeiros bytes: rbx rbp r12 r13 r14 r15 rsp rip,
 * depois "máscara salva?" (u32, +64) e a máscara da thread no formato do kernel (u32, +68).
 * Como no macOS: setjmp salva a máscara, _setjmp não, sigsetjmp(env, savemask) só se savemask != 0; longjmp e
 * siglongjmp restauram o que foi salvo, _longjmp nunca. Sem isso, sair de um tratador por siglongjmp (o ^C do
 * readline) deixava o sinal bloqueado para sempre (o kernel o bloqueia enquanto o tratador roda).
 * A máscara vem de SYS_SIG (58) / MOS_SIGOP_MASK (2) (kernel/include/syscall.h). */
    .text
    .globl setjmp, _setjmp, sigsetjmp, longjmp, _longjmp, siglongjmp
setjmp:
    mov $1, %esi
    jmp sigsetjmp
_setjmp:
    xor %esi, %esi
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
    movl %esi, 64(%rdi)
    test %esi, %esi
    jz 2f
    mov %rdi, %r9
    lea 68(%rdi), %r10         /* antiga: a máscara atual */
    mov $2, %edi               /* MOS_SIGOP_MASK */
    mov $1, %esi               /* SIG_BLOCK com conjunto NULL: só lê */
    xor %edx, %edx
    mov $58, %eax              /* SYS_SIG */
    int $0x80
    test %rax, %rax
    jns 1f
    movl $0, 64(%r9)           /* kernel sem SYS_SIG (harness): não restaura nada */
1:  mov %r9, %rdi
2:  xor %eax, %eax
    ret
longjmp:
siglongjmp:
    cmpl $0, 64(%rdi)
    je _longjmp
    mov %rdi, %r9
    mov %esi, %r8d
    mov $2, %edi               /* MOS_SIGOP_MASK */
    mov $3, %esi               /* SIG_SETMASK */
    lea 68(%r9), %rdx
    xor %r10d, %r10d
    mov $58, %eax              /* SYS_SIG */
    int $0x80
    mov %r9, %rdi
    mov %r8d, %esi
_longjmp:
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
