/*
 * mOS — trampolim dos processadores de aplicação (APs). É copiado para uma página P abaixo de 1 MiB;
 * o SIPI começa a execução em modo real em CS = P >> 4, IP = 0. Leva o AP até o modo longo usando
 * a mesma PML4 do kernel e chama ap_entry(apic) numa pilha própria.
 *
 * Área de dados no fim da página (preenchida por arch/smp.c antes de cada SIPI):
 *   0xF00 GDT temporária (nulo, código 32, dados, código 64)   0xF20 GDTR (limite, base = P + 0xF00)
 *   0xF30 CR3 (32 bits)  0xF38 pilha (64)  0xF40 entrada C (64)  0xF48 alvo modo protegido (ofs32, sel)
 *   0xF50 alvo modo longo (ofs32, sel)  0xF58 contador de APs que chegaram (32)
 */
    .text
    .globl ap_tramp_start, ap_tramp_end, ap_tramp_pm32, ap_tramp_lm64
    .code16
ap_tramp_start:
    cli
    cld
    mov %cs, %ax
    mov %ax, %ds
    xor %ebx, %ebx
    mov %cs, %bx
    shl $4, %ebx                    /* ebx = P (endereço linear da página) */
    movb $1, 0xF5C                  /* estágio 1: modo real */
    lgdtl 0xF20
    mov %cr0, %eax
    or $1, %eax
    mov %eax, %cr0
    ljmpl *0xF48                    /* -> ap_tramp_pm32, seletor 0x08 */

    .code32
ap_tramp_pm32:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    movb $2, 0xF5C(%ebx)            /* estágio 2: modo protegido */
    mov %cr4, %eax
    or $0x6A0, %eax                 /* PAE | PGE | OSFXSR | OSXMMEXCPT */
    mov %eax, %cr4
    mov 0xF30(%ebx), %eax
    mov %eax, %cr3
    mov $0xC0000080, %ecx           /* EFER.LME */
    rdmsr
    or $0x100, %eax
    wrmsr
    mov %cr0, %eax
    and $0x9FFFFFFB, %eax           /* sem CD/NW/EM */
    or $0x80000022, %eax            /* PG | NE | MP */
    mov %eax, %cr0
    ljmpl *0xF50(%ebx)              /* -> ap_tramp_lm64, seletor 0x18 */

    .code64
ap_tramp_lm64:
    mov %ebx, %ebx                  /* zera a metade alta */
    movb $3, 0xF5C(%rbx)            /* estágio 3: modo longo */
    mov 0xF38(%rbx), %rsp
    mov 0xF40(%rbx), %rax
    lock incl 0xF58(%rbx)
    and $-16, %rsp
    sub $32, %rsp                   /* shadow space (ABI x64 da Microsoft); o call deixa rsp = 16n+8 */
    call *%rax
1:  cli
    hlt
    jmp 1b
ap_tramp_end:
