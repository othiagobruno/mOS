/* mOS libSystem arm64 — memcpy/memmove/memset (o rep movsb/stosb do libc.c no x86) e manutenção de cache.
 * Acessos desalinhados de 16 bytes (ldr/str q) são livres em memória normal no arm64. */
#if defined(__aarch64__)
    .text

/* void *memcpy(void *d, const void *s, size_t n): para frente; devolve d */
    .globl memcpy
    .p2align 4
memcpy:
    mov x3, x0
    cmp x2, #64
    b.lo 2f
1:  ldp q0, q1, [x1]
    ldp q2, q3, [x1, #32]
    add x1, x1, #64
    sub x2, x2, #64
    stp q0, q1, [x3]
    stp q2, q3, [x3, #32]
    add x3, x3, #64
    cmp x2, #64
    b.hs 1b
2:  cmp x2, #16
    b.lo 3f
    ldr q0, [x1], #16
    sub x2, x2, #16
    str q0, [x3], #16
    b 2b
3:  cbz x2, 4f
    ldrb w4, [x1], #1
    subs x2, x2, #1
    strb w4, [x3], #1
    b.ne 3b
4:  ret

/* void *memmove(void *d, const void *s, size_t n) */
    .globl memmove
    .p2align 4
memmove:
    sub x4, x0, x1
    cmp x4, x2                 /* (d - s) >= n (sem sinal): sem sobreposição destrutiva para frente */
    b.hs memcpy
    cbz x2, 4f
    add x1, x1, x2             /* de trás para frente */
    add x3, x0, x2
1:  cmp x2, #16
    b.lo 3f
    ldr q0, [x1, #-16]!
    sub x2, x2, #16
    str q0, [x3, #-16]!
    b 1b
3:  cbz x2, 4f
    ldrb w4, [x1, #-1]!
    subs x2, x2, #1
    strb w4, [x3, #-1]!
    b.ne 3b
4:  ret

/* void *memset(void *d, int c, size_t n) */
    .globl memset
    .p2align 4
memset:
    mov x3, x0
    dup v0.16b, w1
    cmp x2, #64
    b.lo 2f
1:  stp q0, q0, [x3]
    stp q0, q0, [x3, #32]
    add x3, x3, #64
    sub x2, x2, #64
    cmp x2, #64
    b.hs 1b
2:  cmp x2, #16
    b.lo 3f
    str q0, [x3], #16
    sub x2, x2, #16
    b 2b
3:  cbz x2, 4f
    strb w1, [x3], #1
    subs x2, x2, #1
    b.ne 3b
4:  ret

/* void sys_icache_invalidate(void *start, size_t len): DC CVAU em cada linha de dados, DSB, IC IVAU em cada linha de
 * instruções, DSB, ISB (as linhas vêm do CTR_EL0). O runtime do tradutor invalida as traduções no IC IVAU. */
    .globl sys_icache_invalidate
    .p2align 4
sys_icache_invalidate:
    cbz x1, 9f
    mrs x3, ctr_el0
    add x1, x0, x1             /* fim */
    ubfx x4, x3, #16, #4       /* DminLine (log2 de palavras) */
    mov x5, #4
    lsl x5, x5, x4             /* bytes por linha de dados */
    sub x6, x5, #1
    bic x2, x0, x6
1:  dc cvau, x2
    add x2, x2, x5
    cmp x2, x1
    b.lo 1b
    dsb ish
    and x4, x3, #15            /* IminLine */
    mov x5, #4
    lsl x5, x5, x4
    sub x6, x5, #1
    bic x2, x0, x6
2:  ic ivau, x2
    add x2, x2, x5
    cmp x2, x1
    b.lo 2b
    dsb ish
    isb
9:  ret

/* void sys_dcache_flush(void *start, size_t len): DC CIVAC em cada linha, DSB */
    .globl sys_dcache_flush
    .p2align 4
sys_dcache_flush:
    cbz x1, 9f
    mrs x3, ctr_el0
    add x1, x0, x1
    ubfx x4, x3, #16, #4
    mov x5, #4
    lsl x5, x5, x4
    sub x6, x5, #1
    bic x2, x0, x6
1:  dc civac, x2
    add x2, x2, x5
    cmp x2, x1
    b.lo 1b
    dsb ish
9:  ret
#endif
