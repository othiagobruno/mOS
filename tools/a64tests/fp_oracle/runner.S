/* mOS — oráculo FP: runner bare-metal para qemu-system-aarch64 -M virt -cpu max (EL1, MMU off).
 * Lê os vetores carregados em VEC_BASE (-device loader), executa cada um e grava as saídas por
 * semihosting no arquivo nomeado no cabeçalho. Formato em fpvec.h. */
    .equ VEC_BASE, 0x48000000

    .text
    .globl _start
_start:
    ldr x0, =stack_top
    mov sp, x0
    mov x0, #(3 << 20)              /* CPACR_EL1.FPEN = 11 */
    msr cpacr_el1, x0
    isb
    adr x0, vectors
    msr vbar_el1, x0
    isb
    ldr x20, =VEC_BASE
    ldr x21, [x20]                  /* count */
    mov x22, x21
    add x23, x20, #128              /* registros de entrada */
    add x24, x23, x21, lsl #7       /* saídas logo depois */
    mov x25, x24
loop:
    cbz x21, done
    movi v4.2d, #0
    movi v5.2d, #0
    movi v6.2d, #0
    movi v7.2d, #0
    movi v8.2d, #0
    movi v9.2d, #0
    movi v10.2d, #0
    movi v11.2d, #0
    movi v12.2d, #0
    movi v13.2d, #0
    movi v14.2d, #0
    movi v15.2d, #0
    movi v16.2d, #0
    movi v17.2d, #0
    movi v18.2d, #0
    movi v19.2d, #0
    movi v20.2d, #0
    movi v21.2d, #0
    movi v22.2d, #0
    movi v23.2d, #0
    movi v24.2d, #0
    movi v25.2d, #0
    movi v26.2d, #0
    movi v27.2d, #0
    movi v28.2d, #0
    movi v29.2d, #0
    movi v30.2d, #0
    movi v31.2d, #0
    ldr w7, [x23, #8]
    msr fpcr, x7
    msr fpsr, xzr
    ldp q0, q1, [x23, #48]
    ldp q2, q3, [x23, #80]
    ldp x0, x1, [x23, #16]
    ldp x2, x3, [x23, #32]
    mov x28, #0
    ldr w7, [x23, #12]
    msr nzcv, x7
    blr x23
    mrs x7, nzcv
    mrs x8, fpsr
    stp x0, x1, [x24, #0]
    stp x2, x3, [x24, #16]
    stp q0, q1, [x24, #32]
    stp q2, q3, [x24, #64]
    stp w8, w7, [x24, #96]
    str w28, [x24, #104]
    msr fpcr, xzr
    add x23, x23, #128
    add x24, x24, #128
    sub x21, x21, #1
    b loop

done:
    /* SYS_OPEN(nome, modo 5 = "wb", strlen) */
    add x9, x20, #16
    mov x10, #0
1:  ldrb w11, [x9, x10]
    cbz w11, 2f
    add x10, x10, #1
    b 1b
2:  adr x1, sh_block
    str x9, [x1]
    mov x11, #5
    str x11, [x1, #8]
    str x10, [x1, #16]
    mov w0, #0x01
    hlt #0xf000
    mov x26, x0                     /* handle */
    /* SYS_WRITE(handle, saídas, count*128) */
    adr x1, sh_block
    str x26, [x1]
    str x25, [x1, #8]
    lsl x11, x22, #7
    str x11, [x1, #16]
    mov w0, #0x05
    hlt #0xf000
    /* SYS_CLOSE */
    adr x1, sh_block
    str x26, [x1]
    mov w0, #0x02
    hlt #0xf000
    /* SYS_EXIT(ADP_Stopped_ApplicationExit, 0) */
    adr x1, sh_block
    ldr x11, =0x20026
    str x11, [x1]
    str xzr, [x1, #8]
    mov w0, #0x18
    hlt #0xf000
    b .

    .ltorg

/* exceção síncrona (instrução indefinida): marca e pula a instrução */
    .balign 2048
vectors:
    .rept 16
    mov x28, #1
    mrs x27, elr_el1
    add x27, x27, #4
    msr elr_el1, x27
    eret
    .balign 128
    .endr

    .data
    .balign 16
sh_block: .quad 0, 0, 0, 0

    .bss
    .balign 16
    .space 65536
stack_top:
