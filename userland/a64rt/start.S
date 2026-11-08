/*
 * mOS — a64rt: rotinas de ABI especial do runtime do tradutor arm64 (x86_64, System V).
 *   _start         o kernel entra aqui com rdi = a64rt_boot_t*, rsp = topo da pilha do host (16n - 8)
 *   rt_sys         int 0x80 com 6 argumentos (rdi rsi rdx r10 r8 r9)
 *   rt_setjmp      salva o contexto de quem chama a64_run; o tratador de sinal "volta" para cá com rax = 1
 *   rt_host_tramp  trampolim de sinais do host (kernel: tramp(handler, estilo, sinal, siginfo*, ucontext*))
 *   rt_thread_start  entrada das threads novas do host (rdi = rt_thread_t*)
 *   rt_thread_exit_now  marca a thread livre e chama SYS_THREAD_EXIT sem tocar mais na pilha
 * Números de syscall literais aqui (35 THREAD_EXIT, 58 SIG, 6 SIGOP_RETURN): conferidos em rt.c
 */
    .text

    .globl _start
_start:
    and $-16, %rsp
    call rt_main
    ud2

    .globl rt_sys                   /* long rt_sys(n, a, b, c, d, e, f) */
rt_sys:
    mov %rdi, %rax
    mov %rsi, %rdi
    mov %rdx, %rsi
    mov %rcx, %rdx
    mov %r8, %r10
    mov %r9, %r8
    mov 8(%rsp), %r9
    int $0x80
    ret

    .globl rt_setjmp                /* int rt_setjmp(rt_jmp_t *j): rbx rbp r12 r13 r14 r15 rsp rip */
rt_setjmp:
    mov %rbx, 0(%rdi)
    mov %rbp, 8(%rdi)
    mov %r12, 16(%rdi)
    mov %r13, 24(%rdi)
    mov %r14, 32(%rdi)
    mov %r15, 40(%rdi)
    lea 8(%rsp), %rax
    mov %rax, 48(%rdi)
    mov (%rsp), %rax
    mov %rax, 56(%rdi)
    xor %eax, %eax
    ret

    .globl rt_host_tramp
rt_host_tramp:                      /* rdi handler, rsi estilo, rdx sinal, rcx siginfo*, r8 ucontext*; rsp = 16n + 8 */
    sub $8, %rsp
    mov %r8, %rbx                   /* o ucontext guardou o rbx da thread interrompida */
    mov %rdx, %rdi
    mov %rcx, %rsi
    mov %r8, %rdx
    call rt_host_signal
    mov $58, %eax                   /* SYS_SIG */
    mov $6, %edi                    /* MOS_SIGOP_RETURN */
    mov %rbx, %rsi
    int $0x80
    ud2

    .globl rt_thread_start
rt_thread_start:                    /* rsp = topo da pilha do host (16n - 8), rdi = rt_thread_t* */
    and $-16, %rsp
    call rt_thread_main
    ud2

    .globl rt_thread_exit_now       /* void rt_thread_exit_now(volatile int *state, long code): state = 0, exit */
rt_thread_exit_now:
    mov %rsi, %r8
    movl $0, (%rdi)                 /* livre: outra thread pode reusar esta pilha a partir daqui */
    mov $35, %eax                   /* SYS_THREAD_EXIT */
    mov %r8, %rdi
    int $0x80
    ud2
