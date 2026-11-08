/*
 * mOS libSystem arm64 — despacho de mensagens Objective-C (Darwin PCS). Versão arm64 de objc_msg.S.
 * Cada variante guarda os registradores de argumento (x0..x8, q0..q7; x8 = endereço do resultado indireto), busca o IMP
 * em C (cx_msg_lookup, com cache por classe) e salta para ele com os argumentos intactos. Os variádicos do Darwin estão
 * na pilha do chamador, que não muda. Em arm64 não existe _stret nem _fpret: struct grande volta por x8 com o
 * objc_msgSend comum. objc_msgSend_stret/_fpret ficam como sinônimos do objc_msgSend para o código do próprio libSystem
 * que os chama com o tipo de retorno de struct (a forma "NSRect (*)(id, SEL)").
 */
#if defined(__aarch64__)
    .text

.macro SAVE_ARGS
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #208
    stp x0, x1, [sp]
    stp x2, x3, [sp, #16]
    stp x4, x5, [sp, #32]
    stp x6, x7, [sp, #48]
    str x8, [sp, #64]
    stp q0, q1, [sp, #80]
    stp q2, q3, [sp, #112]
    stp q4, q5, [sp, #144]
    stp q6, q7, [sp, #176]
.endm

/* x0 = IMP */
.macro RESTORE_ARGS_AND_JUMP
    mov x17, x0
    ldp q0, q1, [sp, #80]
    ldp q2, q3, [sp, #112]
    ldp q4, q5, [sp, #144]
    ldp q6, q7, [sp, #176]
    ldp x0, x1, [sp]
    ldp x2, x3, [sp, #16]
    ldp x4, x5, [sp, #32]
    ldp x6, x7, [sp, #48]
    ldr x8, [sp, #64]
    mov sp, x29
    ldp x29, x30, [sp], #16
    br x17
.endm

/* id objc_msgSend(id self, SEL op, ...) */
    .globl objc_msgSend
    .globl objc_msgSend_fpret
    .globl objc_msgSend_fp2ret
    .globl objc_msgSend_stret
    .p2align 4
objc_msgSend:
objc_msgSend_fpret:
objc_msgSend_fp2ret:
objc_msgSend_stret:
    cbz x0, 1f
    SAVE_ARGS
    ldr x0, [x0]                /* isa */
    bl cx_msg_lookup            /* (cls, sel) */
    RESTORE_ARGS_AND_JUMP
1:  mov x1, #0                  /* nil: x0/x1 e d0..d3 zerados, como no runtime da Apple */
    movi d0, #0
    movi d1, #0
    movi d2, #0
    movi d3, #0
    ret

/* id objc_msgSendSuper2(struct objc_super *sup, SEL op, ...): sup = {receiver, classe atual} */
    .globl objc_msgSendSuper2
    .globl objc_msgSendSuper2_stret
    .p2align 4
objc_msgSendSuper2:
objc_msgSendSuper2_stret:
    SAVE_ARGS
    ldr x0, [x0, #8]            /* classe atual */
    ldr x0, [x0, #8]            /* ... sua superclasse */
    bl cx_msg_lookup
    ldr x9, [sp]
    ldr x9, [x9]                /* receiver */
    str x9, [sp]
    RESTORE_ARGS_AND_JUMP

/* id objc_msgSendSuper(struct objc_super *sup, SEL op, ...): sup = {receiver, superclasse} */
    .globl objc_msgSendSuper
    .globl objc_msgSendSuper_stret
    .p2align 4
objc_msgSendSuper:
objc_msgSendSuper_stret:
    SAVE_ARGS
    ldr x0, [x0, #8]
    bl cx_msg_lookup
    ldr x9, [sp]
    ldr x9, [x9]
    str x9, [sp]
    RESTORE_ARGS_AND_JUMP

/* IMP objc_msgLookup(id self, SEL op): a Apple exporta em arm64 (devolve o IMP em x17 e os argumentos intactos) */
    .globl objc_msgLookup
    .p2align 4
objc_msgLookup:
    cbz x0, 2f
    SAVE_ARGS
    ldr x0, [x0]
    bl cx_msg_lookup
    mov x17, x0
    ldp q0, q1, [sp, #80]
    ldp q2, q3, [sp, #112]
    ldp q4, q5, [sp, #144]
    ldp q6, q7, [sp, #176]
    ldp x0, x1, [sp]
    ldp x2, x3, [sp, #16]
    ldp x4, x5, [sp, #32]
    ldp x6, x7, [sp, #48]
    ldr x8, [sp, #64]
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
2:  adr x17, 3f
    ret
3:  mov x0, #0
    mov x1, #0
    movi d0, #0
    movi d1, #0
    movi d2, #0
    movi d3, #0
    ret
#endif
