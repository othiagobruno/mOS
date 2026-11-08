/*
 * Autoverificação dos testes int_*.S (frente INT do tradutor arm64). Incluir uma vez por teste.
 *   CHK(xR, valor)  confere um registrador X com uma constante (ou símbolo)
 *   CHKR(xA, xB)    confere dois registradores
 *   CHKF(valor)     confere NZCV (bits 31..28)
 * Usam x25, x26, x27 (as flags são preservadas). Falha: imprime "FAIL linha=... obtido=... esperado=..." e sai com 1.
 * Sucesso: o teste chama INT_OK (imprime "ok") e termina com o dump do estado (para o oráculo).
 */
#define CHK(reg, val) mrs x26, nzcv ; ldr x27, =val ; cmp reg, x27 ; b.eq 9999f ; mov x2, x27 ; mov x1, reg ; mov x0, #__LINE__ ; b int_fail ; 9999: msr nzcv, x26
#define CHKR(reg, reg2) mrs x26, nzcv ; cmp reg, reg2 ; b.eq 9999f ; mov x2, reg2 ; mov x1, reg ; mov x0, #__LINE__ ; b int_fail ; 9999: msr nzcv, x26
#define CHKF(val) mrs x25, nzcv ; CHK(x25, val)

    .text
    .p2align 2
int_hex:                    /* x3 = valor; escreve 16 dígitos em [x4], avança x4 */
    mov x5, #60
1:  lsr x6, x3, x5
    and x6, x6, #15
    cmp x6, #10
    add x7, x6, #'0'
    add x8, x6, #('a' - 10)
    csel x6, x7, x8, lo
    strb w6, [x4], #1
    subs x5, x5, #4
    b.ge 1b
    ret

int_put:                    /* copia a string em x9 para [x4] */
1:  ldrb w6, [x9], #1
    cbz w6, 2f
    strb w6, [x4], #1
    b 1b
2:  ret

int_fail:
    mov x20, x0
    mov x21, x1
    mov x22, x2
    adrp x4, int_buf
    add x4, x4, :lo12:int_buf
    adr x9, int_s_fail
    bl int_put
    mov x3, x20
    bl int_hex
    adr x9, int_s_got
    bl int_put
    mov x3, x21
    bl int_hex
    adr x9, int_s_exp
    bl int_put
    mov x3, x22
    bl int_hex
    mov w6, #10
    strb w6, [x4], #1
    adrp x1, int_buf
    add x1, x1, :lo12:int_buf
    sub x2, x4, x1
    mov x0, #2
    mov x16, #4
    svc #0x80
    mov x0, #1
    mov x16, #1
    svc #0x80
    brk #1

#define INT_OK adr x1, int_s_ok ; mov x0, #1 ; mov x2, #3 ; mov x16, #4 ; svc #0x80

int_s_fail: .asciz "FAIL linha=0x"
int_s_got:  .asciz " obtido=0x"
int_s_exp:  .asciz " esperado=0x"
int_s_ok:   .ascii "ok\n"
    .p2align 2

    .bss
    .p2align 4
int_buf: .space 128
    .text
