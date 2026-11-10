/* _objc_msgForward: tira uma foto dos argumentos e só então entra em C.
 * O corpo antigo em C lia self/_cmd e chamava objc_msgSend sem o resto, então
 * rdi..r9 e xmm0..7 já tinham sido perdidos. Os deslocamentos são os
 * _Static_assert de objc_forward.c (gpr 0, fpr 80, stack 208, ret 464).
 */
#if defined(__aarch64__)
    .text
    .globl _objc_msgForward
    .p2align 2
_objc_msgForward:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #512
    stp x0, x1, [sp, #0]
    stp x2, x3, [sp, #16]
    stp x4, x5, [sp, #32]
    stp x6, x7, [sp, #48]
    str x8, [sp, #64]
    stp q0, q1, [sp, #80]
    stp q2, q3, [sp, #112]
    stp q4, q5, [sp, #144]
    stp q6, q7, [sp, #176]
    add x1, x29, #16
    add x2, sp, #208
    mov x3, #256
1:
    cbz x3, 2f
    ldrb w4, [x1], #1
    strb w4, [x2], #1
    sub x3, x3, #1
    b 1b
2:
    mov x0, sp
    bl cx_objc_forward
    ldr x0, [sp, #464]
    ldr x1, [sp, #472]
    ldr q0, [sp, #480]
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
    .section .note.GNU-stack, "", @progbits
#else
    .text
    .globl _objc_msgForward
    .type _objc_msgForward, @function
    .p2align 4
_objc_msgForward:
    pushq %rbp
    movq %rsp, %rbp
    subq $512, %rsp
    movq %rdi, 0(%rsp)
    movq %rsi, 8(%rsp)
    movq %rdx, 16(%rsp)
    movq %rcx, 24(%rsp)
    movq %r8, 32(%rsp)
    movq %r9, 40(%rsp)
    movdqu %xmm0, 80(%rsp)
    movdqu %xmm1, 96(%rsp)
    movdqu %xmm2, 112(%rsp)
    movdqu %xmm3, 128(%rsp)
    movdqu %xmm4, 144(%rsp)
    movdqu %xmm5, 160(%rsp)
    movdqu %xmm6, 176(%rsp)
    movdqu %xmm7, 192(%rsp)
    cld
    leaq 208(%rsp), %rdi
    leaq 16(%rbp), %rsi
    movq $256, %rcx
    rep movsb
    movq %rsp, %rdi
    call cx_objc_forward
    movq 464(%rsp), %rax
    movq 472(%rsp), %rdx
    movdqu 480(%rsp), %xmm0
    leave
    ret
    .size _objc_msgForward, .-_objc_msgForward
    .section .note.GNU-stack, "", @progbits
#endif
