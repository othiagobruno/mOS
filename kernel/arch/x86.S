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
    ISR_NOERR 242               /* IPI "olhe o escalonador": TLB e threads mortas (proc/sched.c) */
    ISR_NOERR 255               /* interrupção espúria do LAPIC */
    .globl isr128, isr240, isr242, isr255

    /* IPI que acorda os APs operários (arch/smp.c, vetor 0xF1): só o EOI. Não passa pelo isr_common,
     * que chamaria o escalonador. Se pegar um AP em ring 3 (rodando uma thread de usuário), a pilha é a
     * rsp0 do TSS daquele núcleo e nada além de rax é tocado. */
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
    sub $1024, %rsp             /* FPU_AREA (proc.h): XSAVE (x87|SSE|AVX) ou FXSAVE, alinhada a 64 */
    and $-64, %rsp
    cmpb $0, g_use_xsave(%rip)
    je 81f
    /* cabeçalho XSAVE (bytes 512..575) zerado: o XSAVE só escreve os bits do XSTATE_BV que estão na máscara (o resto
     * fica como estava) e não toca no XCOMP_BV nem nos reservados; o XRSTOR dá #GP com qualquer um deles sujo, e a área
     * fica na pilha do kernel, com lixo de antes */
    movq $0, 512(%rsp)
    movq $0, 520(%rsp)
    movq $0, 528(%rsp)
    movq $0, 536(%rsp)
    movq $0, 544(%rsp)
    movq $0, 552(%rsp)
    movq $0, 560(%rsp)
    movq $0, 568(%rsp)
    mov $-1, %eax               /* rax/rdx já estão no quadro */
    mov $-1, %edx
    xsave (%rsp)
    jmp 82f
81: fxsave (%rsp)
82:
    sub $32, %rsp
    call isr_dispatch           /* devolve o frame a restaurar (troca de contexto se for outro) */
    mov %rax, %rbx
    mov %rbx, %rsp
    sub $1024, %rsp             /* a área de FPU de cada thread fica logo abaixo do seu frame (FPU_AREA) */
    and $-64, %rsp
    /* já na pilha da thread que vai rodar (abaixo da área FXSAVE): só agora a trava grande pode ser
     * solta (proc/sched.c, sched_post) — outro núcleo pode pegar a thread anterior e usar a pilha dela */
    sub $32, %rsp
    call sched_post
    add $32, %rsp
    cmpb $0, g_use_xsave(%rip)
    je 83f
    mov $-1, %eax               /* rax/rdx voltam do quadro logo abaixo */
    mov $-1, %edx
    xrstor (%rsp)
    jmp 84f
83: fxrstor (%rsp)
84:
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
