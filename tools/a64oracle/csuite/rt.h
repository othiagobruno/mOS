/*
 * mOS — suíte diferencial C do tradutor arm64: runtime mínimo comum.
 * O mesmo programa compila para x86_64-windows (referência, roda nativo) e para aarch64
 * freestanding (roda no a64test e no oráculo QEMU); as saídas têm que ser idênticas byte a byte.
 *
 * Regras para os testes continuarem portáveis (senão a diferença não é do tradutor):
 *  - só tipos de largura fixa (long tem 32 bits no Windows); char com sinal explícito;
 *  - nada de UB: overflow com sinal, divisão por zero, INT_MIN/-1, shift >= largura,
 *    conversão FP→inteiro fora da faixa;
 *  - NaN impresso como "nan" (o x86 gera NaN padrão negativo, o ARM positivo);
 *  - FP: -ffp-contract=off nos dois lados; sem long double; _Float16 só em conversões.
 * Cada programa define `static void run(void)` e inclui este arquivo.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

#if defined(__aarch64__)
long a64t_write(int fd, const void *b, unsigned long n);
static void rt_write(const char *b, size_t n) { a64t_write(1, b, n); }
/* o compilador pode emitir chamadas a estas (cópia de structs etc.); -fno-builtin impede que
 * os próprios laços virem chamadas recursivas */
void *memcpy(void *d, const void *s, size_t n) { unsigned char *p = d; const unsigned char *q = s; while (n--) *p++ = *q++; return d; }
void *memmove(void *d, const void *s, size_t n) {
    unsigned char *p = d; const unsigned char *q = s;
    if (p < q) while (n--) *p++ = *q++; else { p += n; q += n; while (n--) *--p = *--q; }
    return d;
}
void *memset(void *d, int c, size_t n) { unsigned char *p = d; while (n--) *p++ = (unsigned char)c; return d; }
int memcmp(const void *a, const void *b, size_t n) {
    const unsigned char *p = a, *q = b;
    for (; n; n--, p++, q++) if (*p != *q) return *p < *q ? -1 : 1;
    return 0;
}
/* divisão de 128 bits (o freestanding não traz o compiler-rt): restauração bit a bit */
static unsigned __int128 rt_udivmod128(unsigned __int128 n, unsigned __int128 d, unsigned __int128 *rem) {
    unsigned __int128 q = 0, r = 0;
    for (int i = 127; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1);
        if (r >= d) { r -= d; q |= (unsigned __int128)1 << i; }
    }
    if (rem) *rem = r;
    return q;
}
unsigned __int128 __udivti3(unsigned __int128 n, unsigned __int128 d) { return rt_udivmod128(n, d, 0); }
unsigned __int128 __umodti3(unsigned __int128 n, unsigned __int128 d) { unsigned __int128 r; rt_udivmod128(n, d, &r); return r; }
__int128 __divti3(__int128 n, __int128 d) {
    int neg = (n < 0) != (d < 0);
    unsigned __int128 un = n < 0 ? 0 - (unsigned __int128)n : (unsigned __int128)n, ud = d < 0 ? 0 - (unsigned __int128)d : (unsigned __int128)d;
    unsigned __int128 q = rt_udivmod128(un, ud, 0);
    return (__int128)(neg ? 0 - q : q);
}
__int128 __modti3(__int128 n, __int128 d) {
    unsigned __int128 un = n < 0 ? 0 - (unsigned __int128)n : (unsigned __int128)n, ud = d < 0 ? 0 - (unsigned __int128)d : (unsigned __int128)d, r;
    rt_udivmod128(un, ud, &r);
    return (__int128)(n < 0 ? 0 - r : r);
}
#else
__declspec(dllimport) void *__stdcall GetStdHandle(unsigned long);
__declspec(dllimport) int __stdcall WriteFile(void *, const void *, unsigned long, unsigned long *, void *);
static void rt_write(const char *b, size_t n) { unsigned long w; WriteFile(GetStdHandle((unsigned long)-11), b, (unsigned long)n, &w, 0); }
#endif

/* setjmp/longjmp próprios (o __builtin_setjmp não existe no aarch64): salvam os registradores
 * preservados de cada ABI (AAPCS64: x19..x30, sp, d8..d15; Win64: rbx rbp rdi rsi r12..r15,
 * rsp, rip, xmm6..xmm15) */
typedef uint64_t rt_jmp_buf[32] __attribute__((aligned(16)));
int rt_setjmp(rt_jmp_buf b) __attribute__((returns_twice));
__attribute__((noreturn)) void rt_longjmp(rt_jmp_buf b, int v);
#if defined(__aarch64__)
__asm__(".text\n.globl rt_setjmp\nrt_setjmp:\n"
        "stp x19, x20, [x0, #0]\n stp x21, x22, [x0, #16]\n stp x23, x24, [x0, #32]\n stp x25, x26, [x0, #48]\n"
        "stp x27, x28, [x0, #64]\n stp x29, x30, [x0, #80]\n mov x1, sp\n str x1, [x0, #96]\n"
        "stp d8, d9, [x0, #104]\n stp d10, d11, [x0, #120]\n stp d12, d13, [x0, #136]\n stp d14, d15, [x0, #152]\n"
        "mov w0, #0\n ret\n"
        ".globl rt_longjmp\nrt_longjmp:\n"
        "ldp x19, x20, [x0, #0]\n ldp x21, x22, [x0, #16]\n ldp x23, x24, [x0, #32]\n ldp x25, x26, [x0, #48]\n"
        "ldp x27, x28, [x0, #64]\n ldp x29, x30, [x0, #80]\n ldr x2, [x0, #96]\n mov sp, x2\n"
        "ldp d8, d9, [x0, #104]\n ldp d10, d11, [x0, #120]\n ldp d12, d13, [x0, #136]\n ldp d14, d15, [x0, #152]\n"
        "cmp w1, #0\n csinc w0, w1, wzr, ne\n ret\n");
#else
__asm__(".text\n.globl rt_setjmp\nrt_setjmp:\n"
        "movq %rbx, 0(%rcx)\n movq %rbp, 8(%rcx)\n movq %rdi, 16(%rcx)\n movq %rsi, 24(%rcx)\n"
        "movq %r12, 32(%rcx)\n movq %r13, 40(%rcx)\n movq %r14, 48(%rcx)\n movq %r15, 56(%rcx)\n"
        "leaq 8(%rsp), %rdx\n movq %rdx, 64(%rcx)\n movq (%rsp), %rdx\n movq %rdx, 72(%rcx)\n"
        "movdqu %xmm6, 80(%rcx)\n movdqu %xmm7, 96(%rcx)\n movdqu %xmm8, 112(%rcx)\n movdqu %xmm9, 128(%rcx)\n"
        "movdqu %xmm10, 144(%rcx)\n movdqu %xmm11, 160(%rcx)\n movdqu %xmm12, 176(%rcx)\n movdqu %xmm13, 192(%rcx)\n"
        "movdqu %xmm14, 208(%rcx)\n movdqu %xmm15, 224(%rcx)\n xorl %eax, %eax\n ret\n"
        ".globl rt_longjmp\nrt_longjmp:\n"
        "movq 0(%rcx), %rbx\n movq 8(%rcx), %rbp\n movq 16(%rcx), %rdi\n movq 24(%rcx), %rsi\n"
        "movq 32(%rcx), %r12\n movq 40(%rcx), %r13\n movq 48(%rcx), %r14\n movq 56(%rcx), %r15\n"
        "movdqu 80(%rcx), %xmm6\n movdqu 96(%rcx), %xmm7\n movdqu 112(%rcx), %xmm8\n movdqu 128(%rcx), %xmm9\n"
        "movdqu 144(%rcx), %xmm10\n movdqu 160(%rcx), %xmm11\n movdqu 176(%rcx), %xmm12\n movdqu 192(%rcx), %xmm13\n"
        "movdqu 208(%rcx), %xmm14\n movdqu 224(%rcx), %xmm15\n"
        "movl %edx, %eax\n testl %eax, %eax\n jnz 1f\n incl %eax\n1:\n movq 64(%rcx), %rsp\n jmpq *72(%rcx)\n");
#endif

static char rt_buf[8192];
static size_t rt_n;
static void rt_flush(void) { if (rt_n) { rt_write(rt_buf, rt_n); rt_n = 0; } }
static void put_c(char c) { if (rt_n == sizeof rt_buf) rt_flush(); rt_buf[rt_n++] = c; }
static void put_s(const char *s) { while (*s) put_c(*s++); }
static void put_u(uint64_t v) { char t[24]; int n = 0; do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v); while (n) put_c(t[--n]); }
static void put_i(int64_t v) { if (v < 0) { put_c('-'); put_u(0 - (uint64_t)v); } else put_u((uint64_t)v); }
static void put_x(uint64_t v, int digits) { for (int i = digits - 1; i >= 0; i--) put_c("0123456789abcdef"[(v >> (4 * i)) & 15]); }
static void put_d(double d) {
    if (d != d) { put_s("nan"); return; }
    uint64_t b; __builtin_memcpy(&b, &d, 8); put_s("d:"); put_x(b, 16);
}
static void put_f(float f) {
    if (f != f) { put_s("nan"); return; }
    uint32_t b; __builtin_memcpy(&b, &f, 4); put_s("f:"); put_x(b, 8);
}
static void kv_u(const char *k, uint64_t v) { put_s(k); put_s(" = "); put_u(v); put_c('\n'); }
static void kv_i(const char *k, int64_t v) { put_s(k); put_s(" = "); put_i(v); put_c('\n'); }
static void kv_x(const char *k, uint64_t v) { put_s(k); put_s(" = 0x"); put_x(v, 16); put_c('\n'); }
static void kv_d(const char *k, double v) { put_s(k); put_s(" = "); put_d(v); put_c('\n'); }

/* hash acumulado (FNV-1a 64) para laços grandes */
typedef struct { uint64_t h; } hsh;
static void h_init(hsh *h) { h->h = 0xcbf29ce484222325ull; }
static void h_u(hsh *h, uint64_t v) { for (int i = 0; i < 8; i++) { h->h ^= (v >> (8 * i)) & 0xFF; h->h *= 0x100000001b3ull; } }
static void h_d(hsh *h, double d) { if (d != d) h_u(h, 0x7FF8000000000000ull); else { uint64_t b; __builtin_memcpy(&b, &d, 8); h_u(h, b); } }
static void h_f(hsh *h, float f) { if (f != f) h_u(h, 0x7FC00000u); else { uint32_t b; __builtin_memcpy(&b, &f, 4); h_u(h, b); } }

/* gerador determinístico */
static uint64_t rt_seed = 0x243F6A8885A308D3ull;
static uint64_t rnd64(void) { uint64_t x = rt_seed; x ^= x >> 12; x ^= x << 25; x ^= x >> 27; rt_seed = x; return x * 0x2545F4914F6CDD1Dull; }

static void run(void);
int main(void) { run(); rt_flush(); return 0; }
