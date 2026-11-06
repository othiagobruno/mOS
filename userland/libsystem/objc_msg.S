/*
 * mOS libSystem — despacho de mensagens Objective-C (x86_64, System V).
 * Cada variante salva os registradores de argumento (inteiros, xmm0-7 e al), busca o IMP em C
 * (cx_msg_lookup, com cache por classe) e salta para ele com os argumentos intactos.
 */
    .text

.macro SAVE_ARGS
    push %rbp
    mov %rsp, %rbp
    sub $208, %rsp
    mov %rdi, 0(%rsp)
    mov %rsi, 8(%rsp)
    mov %rdx, 16(%rsp)
    mov %rcx, 24(%rsp)
    mov %r8, 32(%rsp)
    mov %r9, 40(%rsp)
    mov %rax, 48(%rsp)
    movdqu %xmm0, 64(%rsp)
    movdqu %xmm1, 80(%rsp)
    movdqu %xmm2, 96(%rsp)
    movdqu %xmm3, 112(%rsp)
    movdqu %xmm4, 128(%rsp)
    movdqu %xmm5, 144(%rsp)
    movdqu %xmm6, 160(%rsp)
    movdqu %xmm7, 176(%rsp)
.endm

.macro RESTORE_ARGS_AND_JUMP
    mov %rax, %r11
    mov 0(%rsp), %rdi
    mov 8(%rsp), %rsi
    mov 16(%rsp), %rdx
    mov 24(%rsp), %rcx
    mov 32(%rsp), %r8
    mov 40(%rsp), %r9
    mov 48(%rsp), %rax
    movdqu 64(%rsp), %xmm0
    movdqu 80(%rsp), %xmm1
    movdqu 96(%rsp), %xmm2
    movdqu 112(%rsp), %xmm3
    movdqu 128(%rsp), %xmm4
    movdqu 144(%rsp), %xmm5
    movdqu 160(%rsp), %xmm6
    movdqu 176(%rsp), %xmm7
    leave
    jmp *%r11
.endm

/* id objc_msgSend(id self, SEL op, ...) */
    .globl objc_msgSend
    .globl objc_msgSend_fpret
    .globl objc_msgSend_fp2ret
objc_msgSend:
objc_msgSend_fpret:
objc_msgSend_fp2ret:
    test %rdi, %rdi
    je 1f
    SAVE_ARGS
    mov (%rdi), %rdi            /* isa */
    call cx_msg_lookup          /* (cls, sel) */
    RESTORE_ARGS_AND_JUMP
1:  xor %eax, %eax
    xor %edx, %edx
    pxor %xmm0, %xmm0
    pxor %xmm1, %xmm1
    ret

/* void objc_msgSend_stret(void *ret, id self, SEL op, ...) */
    .globl objc_msgSend_stret
objc_msgSend_stret:
    test %rsi, %rsi
    je 2f
    SAVE_ARGS
    mov (%rsi), %rdi
    mov %rdx, %rsi
    call cx_msg_lookup_stret
    RESTORE_ARGS_AND_JUMP
2:  mov %rdi, %rax
    ret

/* id objc_msgSendSuper2(struct objc_super *sup, SEL op, ...): sup = {receiver, classe atual} */
    .globl objc_msgSendSuper2
objc_msgSendSuper2:
    SAVE_ARGS
    mov 8(%rdi), %rdi           /* classe atual */
    mov 8(%rdi), %rdi           /* ... sua superclasse */
    call cx_msg_lookup
    mov 0(%rsp), %r10
    mov (%r10), %r10            /* receiver */
    mov %r10, 0(%rsp)
    RESTORE_ARGS_AND_JUMP

/* id objc_msgSendSuper(struct objc_super *sup, SEL op, ...): sup = {receiver, superclasse} */
    .globl objc_msgSendSuper
objc_msgSendSuper:
    SAVE_ARGS
    mov 8(%rdi), %rdi
    call cx_msg_lookup
    mov 0(%rsp), %r10
    mov (%r10), %r10
    mov %r10, 0(%rsp)
    RESTORE_ARGS_AND_JUMP

/* void objc_msgSendSuper2_stret(void *ret, struct objc_super *sup, SEL op, ...) */
    .globl objc_msgSendSuper2_stret
objc_msgSendSuper2_stret:
    SAVE_ARGS
    mov 8(%rsi), %rdi
    mov 8(%rdi), %rdi
    mov %rdx, %rsi
    call cx_msg_lookup_stret
    mov 8(%rsp), %r10
    mov (%r10), %r10
    mov %r10, 8(%rsp)
    RESTORE_ARGS_AND_JUMP

    .globl objc_msgSendSuper_stret
objc_msgSendSuper_stret:
    SAVE_ARGS
    mov 8(%rsi), %rdi
    mov %rdx, %rsi
    call cx_msg_lookup_stret
    mov 8(%rsp), %r10
    mov (%r10), %r10
    mov %r10, 8(%rsp)
    RESTORE_ARGS_AND_JUMP
