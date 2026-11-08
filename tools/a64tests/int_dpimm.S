/* frente INT: dados com imediato — ADR/ADRP, ADD/SUB imm, lógicos imm, MOV*, BFM/SBFM/UBFM, EXTR */
#include "int_chk.h"
    .text
    .globl main
main:
    stp x29, x30, [sp, #-16]!

    /* ADR / ADRP */
    adr x0, main
    CHK(x0, main)
here:
    adr x0, .+0x40
    adr x1, here
    add x1, x1, #0x40
    CHKR(x0, x1)
    adrp x0, far_data
    ldr x1, =far_data
    and x1, x1, #~0xfff
    CHKR(x0, x1)
    adrp x0, main
    ldr x1, =main
    and x1, x1, #~0xfff
    CHKR(x0, x1)

    /* ADD/SUB imediato e flags */
    mov x1, #-1
    adds x0, x1, #1
    CHK(x0, 0)
    CHKF(0x60000000)
    mov x1, #0xffffffff
    adds w0, w1, #1
    CHK(x0, 0)
    CHKF(0x60000000)
    mov w1, #0x7fffffff
    adds w0, w1, #1
    CHK(x0, 0x80000000)
    CHKF(0x90000000)
    mov x1, #0
    subs x0, x1, #1
    CHK(x0, 0xffffffffffffffff)
    CHKF(0x80000000)
    subs x0, x1, #0
    CHKF(0x60000000)
    mov x1, #0x8000000000000000
    subs x0, x1, #1
    CHK(x0, 0x7fffffffffffffff)
    CHKF(0x30000000)
    mov x1, #0x1000
    add x0, x1, #0xfff, lsl #12
    CHK(x0, 0x1000000)
    sub w0, w1, #1, lsl #12
    CHK(x0, 0)
    sub w0, w1, #2, lsl #12
    CHK(x0, 0xfffff000)
    cmn x1, #0x1, lsl #12
    CHKF(0x00000000)
    mov x2, sp
    sub sp, sp, #0x20
    add x3, sp, #0x20
    CHKR(x2, x3)
    add sp, sp, #0x20
    mov x3, sp
    CHKR(x2, x3)
    adds x4, sp, #0              /* ADDS lê SP no Rn */
    CHKR(x4, x2)
    add w5, wsp, #0              /* 32 bits a partir do SP */
    mov w6, w2
    CHKR(x5, x6)

    /* lógicos com imediato */
    mov x0, #0x5555555555555555
    CHK(x0, 0x5555555555555555)
    ldr x1, =0x123456789abcdef0
    and x0, x1, #0xff00ff00ff00ff00
    CHK(x0, 0x120056009a00de00)
    orr w0, wzr, #0xf0f0f0f0
    CHK(x0, 0xf0f0f0f0)
    eor x0, x1, #0x8000000000000001
    CHK(x0, 0x923456789abcdef1)
    ands x0, x1, #0xf000000000000000
    CHK(x0, 0x1000000000000000)
    CHKF(0)
    mov x1, #-1
    ands x0, x1, #0x8000000000000000
    CHKF(0x80000000)
    tst w1, #0x1
    CHKF(0)
    mov x1, #0
    tst x1, #0x3ffff
    CHKF(0x40000000)
    and w0, w1, #0x7
    CHK(x0, 0)
    mov x1, #-1
    and w0, w1, #0x3c
    CHK(x0, 0x3c)
    eor w0, w1, #0x55555555
    CHK(x0, 0xaaaaaaaa)
    orr x0, xzr, #0x00ff00ff00ff00ff
    CHK(x0, 0x00ff00ff00ff00ff)
    orr x0, xzr, #0x1111111111111111
    CHK(x0, 0x1111111111111111)
    orr x0, xzr, #0x7ffffffffffffffe
    CHK(x0, 0x7ffffffffffffffe)
    orr x0, xzr, #0xfffff0fffffff0ff
    CHK(x0, 0xfffff0fffffff0ff)
    mov x2, sp
    and sp, x2, #0xfffffffffffffff0   /* Rd = SP */
    mov x3, sp
    CHKR(x2, x3)

    /* MOVZ/MOVN/MOVK */
    movz x0, #0x1234, lsl #48
    movk x0, #0x5678, lsl #16
    CHK(x0, 0x1234000056780000)
    movn w0, #0
    CHK(x0, 0xffffffff)
    movn x0, #0x1234, lsl #16
    CHK(x0, 0xffffffffedcbffff)
    mov x0, #-1
    movk w0, #1
    CHK(x0, 0xffff0001)
    movz w0, #0xabcd, lsl #16
    CHK(x0, 0xabcd0000)
    movk x0, #0xffff, lsl #48
    CHK(x0, 0xffff0000abcd0000)
    movn w0, #0x8000, lsl #16
    CHK(x0, 0x7fffffff)

    /* BFM/SBFM/UBFM */
    ldr x1, =0x8123456789abcdef
    sbfx x0, x1, #60, #4
    CHK(x0, 0xfffffffffffffff8)
    ubfx x0, x1, #4, #8
    CHK(x0, 0xde)
    lsl x0, x1, #4
    CHK(x0, 0x123456789abcdef0)
    lsr x0, x1, #60
    CHK(x0, 8)
    asr x0, x1, #60
    CHK(x0, 0xfffffffffffffff8)
    sxtb x0, w1
    CHK(x0, 0xffffffffffffffef)
    sxth w0, w1
    CHK(x0, 0xffffcdef)
    sxtw x0, w1
    CHK(x0, 0xffffffff89abcdef)
    uxtb w0, w1
    CHK(x0, 0xef)
    uxth w0, w1
    CHK(x0, 0xcdef)
    mov x0, #-1
    bfi x0, x1, #8, #16
    CHK(x0, 0xffffffffffcdefff)
    mov x0, #0
    bfxil x0, x1, #4, #8
    CHK(x0, 0xde)
    sbfiz x0, x1, #4, #8
    CHK(x0, 0xfffffffffffffef0)
    ubfiz w0, w1, #24, #8
    CHK(x0, 0xef000000)
    asr w0, w1, #31
    CHK(x0, 0xffffffff)
    lsl w0, w1, #31
    CHK(x0, 0x80000000)
    lsr w0, w1, #1
    CHK(x0, 0x44d5e6f7)
    mov x0, #-1
    bfi w0, w1, #0, #32
    CHK(x0, 0x89abcdef)
    mov x0, #-1
    bfc x0, #4, #56
    CHK(x0, 0xf00000000000000f)
    mov x0, #0
    bfi x0, x1, #63, #1
    CHK(x0, 0x8000000000000000)
    sbfx x0, x1, #0, #64
    CHKR(x0, x1)
    sbfm x0, x1, #8, #4         /* immr > imms: SBFIZ por 56 */
    CHK(x0, 0x0f00000000000000)
    ubfm w0, w1, #4, #2
    CHK(x0, 0x70000000)
    mov x0, #0x5555
    bfm x0, x1, #60, #2         /* BFI x0, x1, #4, #3 */
    CHK(x0, 0x5575)

    /* EXTR / ROR */
    ror x0, x1, #8
    CHK(x0, 0xef8123456789abcd)
    ldr x2, =0x1111222233334444
    extr x0, x1, x2, #16
    CHK(x0, 0xcdef111122223333)
    extr w0, w1, w2, #8
    CHK(x0, 0xef333344)
    ror w0, w1, #4
    CHK(x0, 0xf89abcde)
    extr x0, x1, x2, #0
    CHKR(x0, x2)
    extr w0, w1, w2, #0
    CHK(x0, 0x33334444)

    INT_OK
    bl a64t_dump
    mov x0, #0
    ldp x29, x30, [sp], #16
    ret

    .ltorg
    .data
    .p2align 12
    .space 0x3000
far_data: .quad 0
