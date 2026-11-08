/* frente INT: comportamento específico do mOS/M1 em EL0 (NÃO comparável com o QEMU):
 * CTR_EL0, DCZID_EL0, CNTFRQ_EL0, contadores, FPCR só com os bits graváveis do M1,
 * PAC com algoritmo "identidade" (PACGA = 0), WFE/WFI/YIELD. */
#include "int_chk.h"
    .text
    .globl main
main:
    stp x29, x30, [sp, #-16]!

    mrs x0, ctr_el0
    CHK(x0, 0x84448004)
    mrs x0, dczid_el0
    CHK(x0, 4)
    mrs x0, cntfrq_el0
    CHK(x0, 24000000)
    mrs x1, cntvct_el0
    mrs x2, s3_3_c14_c0_6          /* CNTVCTSS_EL0 */
    mrs x3, cntpct_el0
    mrs x4, s3_3_c14_c0_5          /* CNTPCTSS_EL0 */
    cmp x2, x1
    cset x5, hs
    CHK(x5, 1)
    cmp x3, x2
    cset x5, hs
    CHK(x5, 1)
    cmp x4, x3
    cset x5, hs
    CHK(x5, 1)
    mrs x0, tpidrro_el0
    CHK(x0, 0)

    /* FPCR: só FZ16, RMode, FZ, DN, AHP são graváveis (FIZ/AH/NEP do FEAT_AFP e traps = 0) */
    mov x0, #-1
    msr fpcr, x0
    mrs x1, fpcr
    CHK(x1, 0x07c80000)
    msr fpcr, xzr

    /* PAC "identidade" */
    ldr x0, =0x0000000123456789
    mov x9, x0
    mov x1, #0x55
    pacia x0, x1
    CHKR(x0, x9)
    pacib x0, sp
    pacda x0, x1
    pacdb x0, x1
    paciza x0
    pacizb x0
    pacdza x0
    pacdzb x0
    CHKR(x0, x9)
    autia x0, x1
    autib x0, sp
    autda x0, x1
    autdb x0, x1
    autiza x0
    autizb x0
    autdza x0
    autdzb x0
    CHKR(x0, x9)
    xpaci x0
    xpacd x0
    CHKR(x0, x9)
    ldr x2, =0xffff000000000000
    pacga x3, x0, x1
    CHK(x3, 0)
    mov x8, x30
    paciasp
    CHKR(x30, x8)
    autiasp
    pacibsp
    autibsp
    paciaz
    autiaz
    pacibz
    autibz
    xpaclri
    CHKR(x30, x8)
    mov x17, x9
    mov x16, x1
    pacia1716
    autia1716
    pacib1716
    autib1716
    CHKR(x17, x9)

    /* desvios autenticados */
    adr x4, 1f
    braaz x4
    brk #1
1:  adr x4, 2f
    braa x4, x1
    brk #2
2:  adr x4, 3f
    brab x4, sp
    brk #3
3:  adr x4, sub_ret
    blraaz x4
4:  adr x6, 4b
    CHKR(x0, x6)
    blrab x4, x1
5:  adr x6, 5b
    CHKR(x0, x6)
    blrabz x4
    blraa x4, sp
    bl sub_retaa
6:  adr x6, 6b
    CHKR(x0, x6)

    /* LDRAA/LDRAB */
    adrp x10, data
    add x10, x10, :lo12:data
    ldraa x0, [x10]
    CHK(x0, 0x1111111111111111)
    ldrab x0, [x10, #8]
    CHK(x0, 0x2222222222222222)
    mov x11, x10
    ldraa x0, [x11, #16]!
    CHK(x0, 0x3333333333333333)
    add x12, x10, #16
    CHKR(x11, x12)
    ldrab x0, [x11, #-8]!
    CHK(x0, 0x2222222222222222)
    add x12, x10, #8
    CHKR(x11, x12)
    add x13, x10, #4096
    ldraa x0, [x13, #-4096]
    CHK(x0, 0x1111111111111111)

    /* WFE/WFI/YIELD saem do motor com YIELD e continuam */
    yield
    wfe
    wfi

    INT_OK
    mov x0, #0
    ldp x29, x30, [sp], #16
    ret

sub_ret:
    mov x0, x30
    ret
sub_retaa:
    paciasp
    mov x0, x30
    autiasp
    retaa

    .ltorg
    .data
    .p2align 4
data: .quad 0x1111111111111111, 0x2222222222222222, 0x3333333333333333, 0x4444444444444444
