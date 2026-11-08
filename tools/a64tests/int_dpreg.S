/* frente INT: dados com registrador — shifted/extended, ADC/SBC, CCMP/CCMN, CSEL*, 1/2/3 fontes,
 * CRC32/CRC32C, RMIF, SETF8/16, CFINV/XAFLAG/AXFLAG */
#include "int_chk.h"
    .text
    .globl main
main:
    stp x29, x30, [sp, #-16]!

    /* lógicos shifted */
    ldr x1, =0x8123456789abcdef
    ldr x2, =0x00ff00ff00ff00ff
    and x0, x1, x2, lsl #8
    CHK(x0, 0x810045008900cd00)
    bic x0, x1, x2
    CHK(x0, 0x810045008900cd00)
    orn x0, xzr, x2, ror #4
    CHK(x0, 0x0ff00ff00ff00ff0)
    eon w0, w1, w2, lsr #4
    CHK(x0, 0x765bc21f)
    eor x0, x1, x1, asr #63
    CHK(x0, 0x7edcba9876543210)
    ands x0, x1, x2, lsl #63
    CHKF(0x80000000)
    CHK(x0, 0x8000000000000000)
    bics w0, w1, w1
    CHKF(0x40000000)
    mvn w0, w1
    CHK(x0, 0x76543210)
    orr w0, w1, w2, ror #31
    CHK(x0, 0x89ffcdff)

    /* add/sub shifted */
    mov x3, #0x10
    add x0, x1, x3, lsl #4
    CHK(x0, 0x8123456789abceef)
    sub x0, x3, x1, asr #63
    CHK(x0, 0x11)
    adds w0, w1, w1, lsr #31
    CHKF(0x80000000)
    CHK(x0, 0x89abcdf0)
    subs x0, x3, x3, lsl #1
    CHKF(0x80000000)
    CHK(x0, 0xfffffffffffffff0)
    neg w0, w1, lsl #1
    CHK(x0, 0xeca86422)
    negs x0, xzr
    CHKF(0x60000000)
    mov x4, #0x7fffffffffffffff
    adds x0, x4, x3, lsr #4
    CHKF(0x90000000)

    /* add/sub extended */
    mov x5, #-2
    add x0, x3, w5, sxtw
    CHK(x0, 0xe)
    add x0, x3, w5, uxtw #2
    CHK(x0, 0x3fffffff8 + 0x10)
    sub x0, x3, w5, uxtb
    CHK(x0, 0xffffffffffffff12)
    adds x0, x3, x5, sxtx #4
    CHKF(0x80000000)
    CHK(x0, 0xfffffffffffffff0)
    add w0, w3, w5, sxth #1
    CHK(x0, 0xc)
    add x0, x3, w5, sxtb #3
    CHK(x0, 0)
    mov x6, sp
    add x0, sp, x3, uxtx
    sub x0, x0, #0x10
    CHKR(x0, x6)
    sub sp, sp, x3, uxtx #1
    add x7, sp, #0x20
    CHKR(x7, x6)
    add sp, sp, w3, uxtw #1
    cmp sp, x6
    CHKF(0x60000000)
    cmn w5, w5, uxth
    CHKF(0x20000000)

    /* ADC/SBC */
    mov x8, #-1
    mov x9, #1
    cmp x9, #0              /* C = 1 */
    adc x0, x9, x9
    CHK(x0, 3)
    adcs x0, x8, xzr
    CHKF(0x60000000)
    CHK(x0, 0)
    adcs x0, x8, xzr        /* C = 1 de novo */
    CHKF(0x60000000)
    cmn xzr, xzr            /* C = 0 */
    adcs x0, x8, xzr
    CHKF(0x80000000)
    sbc x0, x9, x9          /* C = 0: 1 - 1 - 1 */
    CHK(x0, 0xffffffffffffffff)
    subs xzr, x9, #0        /* C = 1 */
    sbcs w0, w9, w9
    CHKF(0x60000000)
    CHK(x0, 0)
    ngc w0, w9
    CHK(x0, 0xffffffff)
    mov w10, #0x80000000
    cmn xzr, xzr
    sbcs w0, w10, wzr       /* 0x80000000 - 0 - 1 */
    CHKF(0x30000000)
    CHK(x0, 0x7fffffff)

    /* CCMP/CCMN */
    cmp x9, x9
    ccmp x9, #2, #0xf, eq   /* condição verdadeira: 1 - 2 */
    CHKF(0x80000000)
    cmp x9, x8
    ccmp x9, #2, #0x5, eq   /* falsa: nzcv = 0101 */
    CHKF(0x50000000)
    cmp x9, x9
    ccmn w8, #1, #0, ne
    CHKF(0)
    cmp x9, x9
    ccmn w8, #1, #0, eq
    CHKF(0x60000000)
    cmp x9, x9
    ccmp x8, x8, #0, al
    CHKF(0x60000000)
    ccmp x9, x8, #0xa, nv
    CHKF(0x00000000)

    /* CSEL e família */
    cmp x9, #1
    csel x0, x1, x2, eq
    CHKR(x0, x1)
    csel x0, x1, x2, ne
    CHKR(x0, x2)
    csinc w0, w1, w9, ne
    CHK(x0, 2)
    csinv x0, x1, x9, lt
    CHK(x0, 0xfffffffffffffffe)
    csneg x0, x1, x9, gt
    CHK(x0, 0xffffffffffffffff)
    cset w0, ge
    CHK(x0, 1)
    csetm x0, hs
    CHK(x0, 0xffffffffffffffff)
    cinc x0, x8, ls
    CHK(x0, 0)
    cneg w0, w9, hi
    CHK(x0, 1)
    csel w0, w1, w2, al
    CHK(x0, 0x89abcdef)

    /* 1 fonte */
    rbit x0, x1
    CHK(x0, 0xf7b3d591e6a2c481)
    rbit w0, w1
    CHK(x0, 0xf7b3d591)
    rev16 x0, x1
    CHK(x0, 0x23816745ab89efcd)
    rev16 w0, w1
    CHK(x0, 0xab89efcd)
    rev32 x0, x1
    CHK(x0, 0x67452381efcdab89)
    rev w0, w1
    CHK(x0, 0xefcdab89)
    rev x0, x1
    CHK(x0, 0xefcdab8967452381)
    clz x0, x1
    CHK(x0, 0)
    clz x0, xzr
    CHK(x0, 64)
    clz w0, w9
    CHK(x0, 31)
    clz x0, x2
    CHK(x0, 8)
    cls x0, x1
    CHK(x0, 0)
    cls x0, x8
    CHK(x0, 63)
    cls x0, xzr
    CHK(x0, 63)
    cls w0, w5
    CHK(x0, 30)
    mov x11, #0x0000ffff00000000
    cls x0, x11
    CHK(x0, 15)

    /* 2 fontes */
    mov x12, #100
    mov x13, #7
    udiv x0, x12, x13
    CHK(x0, 14)
    sdiv x0, x8, x13
    CHK(x0, 0)
    mov x14, #-100
    sdiv x0, x14, x13
    CHK(x0, 0xfffffffffffffff2)
    udiv x0, x12, xzr
    CHK(x0, 0)
    sdiv w0, w12, wzr
    CHK(x0, 0)
    mov x15, #0x8000000000000000
    sdiv x0, x15, x8
    CHKR(x0, x15)
    mov w15, #0x80000000
    sdiv w0, w15, w8
    CHK(x0, 0x80000000)
    udiv w0, w8, w13
    CHK(x0, 0x24924924)
    mov x16, #65
    lsl x0, x9, x16
    CHK(x0, 2)
    lslv w0, w9, w16
    CHK(x0, 2)
    lsr x0, x1, x16
    CHK(x0, 0x4091a2b3c4d5e6f7)
    mov x17, #33
    asr w0, w1, w17
    CHK(x0, 0xc4d5e6f7)
    asr x0, x1, x8
    CHK(x0, 0xffffffffffffffff)
    ror x0, x1, x16
    CHK(x0, 0xc091a2b3c4d5e6f7)
    ror w0, w1, w17
    CHK(x0, 0xc4d5e6f7)

    /* CRC32: "123456789" → 0xCBF43926 (CRC-32) e 0xE3069283 (CRC-32C) */
    adr x18, digits
    mov w0, #-1
    mov w19, #-1
    mov x20, #9
1:  ldrb w21, [x18], #1
    crc32b w0, w0, w21
    crc32cb w19, w19, w21
    subs x20, x20, #1
    b.ne 1b
    mvn w0, w0
    mvn w19, w19
    CHK(x0, 0xcbf43926)
    CHK(x19, 0xe3069283)
    adr x18, digits
    ldr x21, [x18]
    ldrb w22, [x18, #8]
    mov w0, #-1
    crc32x w0, w0, x21
    crc32b w0, w0, w22
    mvn w0, w0
    CHK(x0, 0xcbf43926)
    mov w0, #-1
    ldr w21, [x18]
    crc32cw w0, w0, w21
    ldr w21, [x18, #4]
    crc32cw w0, w0, w21
    crc32cb w0, w0, w22
    mvn w0, w0
    CHK(x0, 0xe3069283)
    mov w0, #-1
    ldrh w21, [x18]
    crc32h w0, w0, w21
    ldrh w21, [x18, #2]
    crc32h w0, w0, w21
    ldr w21, [x18, #4]
    crc32w w0, w0, w21
    crc32b w0, w0, w22
    mvn w0, w0
    CHK(x0, 0xcbf43926)
    mov w0, #-1
    crc32cx w0, w0, x21
    crc32ch w23, w0, w22
    /* (sem valor conhecido: fica para o oráculo via dump) */

    /* 3 fontes */
    madd x0, x12, x13, x9
    CHK(x0, 701)
    msub x0, x12, x13, x9
    CHK(x0, 0xfffffffffffffd45)
    mul w0, w8, w8
    CHK(x0, 1)
    mneg x0, x12, x13
    CHK(x0, 0xfffffffffffffd44)
    smaddl x0, w8, w13, x12
    CHK(x0, 93)
    umaddl x0, w8, w13, x12
    CHK(x0, 0x6fffffff9 + 100)
    smsubl x0, w8, w8, xzr
    CHK(x0, 0xffffffffffffffff)
    umsubl x0, w9, w8, x12
    CHK(x0, 0xffffffff00000065)
    smull x0, w15, w15
    CHK(x0, 0x4000000000000000)
    umull x0, w8, w8
    CHK(x0, 0xfffffffe00000001)
    mov x15, #0x8000000000000000
    mov x16, #2
    smulh x0, x15, x16
    CHK(x0, 0xffffffffffffffff)
    umulh x0, x15, x16
    CHK(x0, 1)
    umulh x0, x8, x8
    CHK(x0, 0xfffffffffffffffe)
    smulh x0, x8, x8
    CHK(x0, 0)
    madd w0, w1, w1, w1
    CHK(x0, 0x1a9e7310)

    /* FlagM: RMIF, SETF8/16, CFINV; FlagM2: XAFLAG/AXFLAG */
    msr nzcv, xzr
    mov x24, #0x5
    rmif x24, #0, #0xf
    CHKF(0x50000000)
    mov x24, #0xa0
    rmif x24, #4, #0x6
    CHKF(0x30000000)
    rmif x24, #4, #0x8
    CHKF(0xb0000000)
    mov x24, #1
    rmif x24, #63, #0x2       /* ror 63: bit 0 vai para o bit 1 */
    CHKF(0xb0000000)
    mov w24, #0x80
    msr nzcv, xzr
    setf8 w24
    CHKF(0x90000000)
    mov w24, #0x180
    setf8 w24
    CHKF(0x80000000)
    mov w24, #0x100
    setf8 w24
    CHKF(0x50000000)
    mov w24, #0x18000
    setf16 w24
    CHKF(0x80000000)
    mov w24, #0x8000
    mov x25, #0x20000000
    msr nzcv, x25
    setf16 w24
    CHKF(0xb0000000)
    cfinv
    CHKF(0x90000000)
    cfinv
    CHKF(0xb0000000)
    /* XAFLAG/AXFLAG: tabela inteira das 16 combinações vai pelo dump */
    mov x23, #0
    mov x24, #0
2:  lsl x25, x24, #28
    msr nzcv, x25
    xaflag
    mrs x25, nzcv
    lsr x25, x25, #28
    lsl x26, x24, #2
    lsl x25, x25, x26
    orr x23, x23, x25
    add x24, x24, #1
    cmp x24, #16
    b.ne 2b
    CHK(x23, 0x6633228866332288)
    mov x22, #0
    mov x24, #0
3:  lsl x25, x24, #28
    msr nzcv, x25
    axflag
    mrs x25, nzcv
    lsr x25, x25, #28
    lsl x26, x24, #2
    lsl x25, x25, x26
    orr x22, x22, x25
    add x24, x24, #1
    cmp x24, #16
    b.ne 3b
    CHK(x22, 0x4644424046444240)

    INT_OK
    bl a64t_dump
    mov x0, #0
    ldp x29, x30, [sp], #16
    ret

digits: .ascii "123456789"
    .p2align 2
    .ltorg
