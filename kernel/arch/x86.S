/* mOS — rotinas de baixo nível x86_64 (convenção de chamada Microsoft x64: rcx, rdx, r8, r9) */
    .text

    .globl boot_switch_stack
boot_switch_stack:              /* (rcx = topo da pilha, rdx = função) */
    mov %rcx, %rsp
    and $-16, %rsp
    xor %rbp, %rbp
    sub $32, %rsp
    call *%rdx
1:  hlt
    jmp 1b

    .globl gdt_load
gdt_load:                       /* (rcx = ponteiro GDTR, rdx = seletor TSS) */
    lgdt (%rcx)
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov %ax, %fs
    mov %ax, %gs
    lea 2f(%rip), %rax
    pushq $0x08
    pushq %rax
    lretq
2:  ltr %dx
    ret

    .globl idt_load
idt_load:
    lidt (%rcx)
    ret

.macro ISR_NOERR n
isr\n:
    pushq $0
    pushq $\n
    jmp isr_common
.endm

.macro ISR_ERR n
isr\n:
    pushq $\n
    jmp isr_common
.endm

.irp n, 0,1,2,3,4,5,6,7,9,15,16,18,19,20,22,23,24,25,26,27,28,31
    ISR_NOERR \n
.endr
.irp n, 8,10,11,12,13,14,17,21,29,30
    ISR_ERR \n
.endr
.irp n, 32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48
    ISR_NOERR \n
.endr
    ISR_NOERR 128               /* int 0x80: syscalls (proc/process.c) */
    ISR_NOERR 240               /* IPI de wake-up dos APs / temporizador do LAPIC (arch/smp.c) */
    ISR_NOERR 255               /* interrupção espúria do LAPIC */
    .globl isr128, isr240, isr255

    /* IPI que acorda os APs operários (arch/smp.c, vetor 0xF1): só o EOI. Não passa pelo isr_common,
     * que chamaria o escalonador (ele é do BSP). Os APs nunca estão em ring 3 nem usam a FPU aqui. */
    .globl isr_wake
isr_wake:
    push %rax
    mov g_lapic_eoi(%rip), %rax
    movl $0, (%rax)
    pop %rax
    iretq

isr_common:
    push %rax
    push %rbx
    push %rcx
    push %rdx
    push %rsi
    push %rdi
    push %rbp
    push %r8
    push %r9
    push %r10
    push %r11
    push %r12
    push %r13
    push %r14
    push %r15
    cld
    mov %rsp, %rcx              /* int_frame_t* */
    mov %rsp, %rbx
    sub $512, %rsp
    and $-16, %rsp
    fxsave (%rsp)
    sub $32, %rsp
    call isr_dispatch           /* devolve o frame a restaurar (troca de contexto se for outro) */
    mov %rax, %rbx
    mov %rbx, %rsp
    sub $512, %rsp              /* a área FXSAVE de cada thread fica logo abaixo do seu frame */
    and $-16, %rsp
    fxrstor (%rsp)
    mov %rbx, %rsp
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rbp
    pop %rdi
    pop %rsi
    pop %rdx
    pop %rcx
    pop %rbx
    pop %rax
    add $16, %rsp
    iretq

    .data
    .balign 8
    .globl isr_table
isr_table:
.irp n, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48
    .quad isr\n
.endr
