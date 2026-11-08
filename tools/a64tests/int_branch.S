/* frente INT: desvios e sistema (parte comparável com o oráculo: nada de PAC, contadores ou
 * registradores com valor IMPLEMENTATION DEFINED — esses ficam em int_sys.S) */
#include "int_chk.h"
    .text
    .globl main
main:
    stp x29, x30, [sp, #-16]!

    /* B.cond: tabela 16 flags × 16 condições */
    adrp x10, table
    add x10, x10, :lo12:table
    mov x11, #0
1:  mov x20, #0
    lsl x12, x11, #28
    .irp cc, eq, ne, cs, cc, mi, pl, vs, vc, hi, ls, ge, lt, gt, le, al, nv
    msr nzcv, x12
    b.\cc 2f
    b 3f
2:  add x20, x20, #1
3:  ror x20, x20, #1
    .endr
    lsr x20, x20, #48
    strh w20, [x10, x11, lsl #1]
    add x11, x11, #1
    cmp x11, #16
    b.ne 1b
    ldp x0, x1, [x10]
    ldp x2, x3, [x10, #16]
    CHK(x0, 0xe966d5a6ea6ad6aa)
    CHK(x1, 0xea65e6a5ea69e6a9)
    CHK(x2, 0xd556e996d65aea9a)
    CHK(x3, 0xe655ea95e659ea99)

    /* B / BL */
    mov x0, #0
    b 4f
    mov x0, #1
4:  CHK(x0, 0)
    bl 5f
6:  b 7f
5:  adr x1, 6b
    CHKR(x30, x1)
    ret
7:  b 9f
8:  mov x0, #0x88
    b 10f
9:  b 8b
10: CHK(x0, 0x88)

    /* CBZ/CBNZ/TBZ/TBNZ */
    mov x0, #0x100000000
    mov x1, #0
    cbz w0, 11f
    mov x1, #1
11: CHK(x1, 0)
    cbz x0, 12f
    mov x1, #2
12: CHK(x1, 2)
    cbnz w0, 13f
    mov x1, #3
13: CHK(x1, 3)
    cbnz x0, 14f
    mov x1, #4
14: CHK(x1, 3)
    mov x2, #0x8000000000000000
    tbnz x2, #63, 15f
    mov x1, #5
15: CHK(x1, 3)
    tbz x2, #63, 16f
    mov x1, #6
16: CHK(x1, 6)
    tbz w2, #31, 17f
    mov x1, #7
17: CHK(x1, 6)
    mov w3, #0x80000000
    tbnz w3, #31, 18f
    mov x1, #8
18: CHK(x1, 6)
    tbz x3, #0, 19f
    mov x1, #9
19: CHK(x1, 6)

    /* BR / BLR / RET */
    adr x4, 20f
    br x4
    mov x1, #10
20: CHK(x1, 6)
    adr x5, sub_ret
    blr x5
21: adr x6, 21b
    CHKR(x0, x6)
    adr x30, sub_ret
    blr x30                     /* alvo lido antes do link */
22: adr x6, 22b
    CHKR(x0, x6)
    adr x7, 23f
    ret x7
    mov x1, #11
23: CHK(x1, 6)

    /* hints e barreiras (NOP para o estado) */
    nop
    hint #0x7f
    hint #6                      /* DGH */
    hint #0x28                   /* CLRBHB */
    bti
    bti c
    bti j
    bti jc
    csdb
    esb
    sev
    sevl
    dmb ish
    dmb ishld
    dmb ishst
    dmb sy
    dsb sy
    dsb ish
    dsb nsh
    isb
    sb
    ssbb
    pssbb
    clrex
    clrex #5

    /* MSR/MRS NZCV, FPSR, FPCR (bits do M1), TPIDR_EL0, DIT, SSBS */
    mov x0, #0xffffffff
    msr nzcv, x0
    mrs x1, nzcv
    CHK(x1, 0xf0000000)
    msr fpsr, x0
    mrs x1, fpsr
    CHK(x1, 0xf800009f)
    msr fpsr, xzr
    mrs x1, fpsr
    CHK(x1, 0)
    mov x0, #0x07c80000
    msr fpcr, x0
    mrs x1, fpcr
    CHK(x1, 0x07c80000)
    msr fpcr, xzr
    ldr x0, =0x1122334455667788
    msr tpidr_el0, x0
    mrs x1, tpidr_el0
    CHKR(x1, x0)
    msr tpidr_el0, xzr
    msr dit, #1
    mrs x1, dit
    CHK(x1, 0x01000000)
    msr dit, #0
    mrs x1, dit
    CHK(x1, 0)
    mov x0, #-1
    msr dit, x0
    mrs x1, dit
    CHK(x1, 0x01000000)
    msr dit, xzr
    msr ssbs, #1
    mrs x1, ssbs
    CHK(x1, 0x1000)
    msr ssbs, xzr
    mrs x1, ssbs
    CHK(x1, 0)
    msr nzcv, xzr

    /* DC ZVA (bloco de 64 bytes) e manutenção de cache sem efeito */
    adrp x10, zbuf
    add x10, x10, :lo12:zbuf
    mov x0, #-1
    mov x1, #0
2:  str x0, [x10, x1, lsl #3]
    add x1, x1, #1
    cmp x1, #24
    b.ne 2b
    add x2, x10, #64 + 13
    dc zva, x2
    ldr x3, [x10, #56]
    CHK(x3, 0xffffffffffffffff)
    ldr x3, [x10, #64]
    CHK(x3, 0)
    ldr x3, [x10, #120]
    CHK(x3, 0)
    ldr x3, [x10, #128]
    CHK(x3, 0xffffffffffffffff)
    dc cvau, x10
    dc cvac, x10
    dc civac, x10
    dc cvap, x10
    dc cvadp, x10
    ic ivau, x10
    dsb ish
    isb

    INT_OK
    bl a64t_dump
    mov x0, #0
    ldp x29, x30, [sp], #16
    ret

sub_ret:
    mov x0, x30
    ret

    .ltorg
    .bss
    .p2align 6
table: .space 64
zbuf:  .space 192
