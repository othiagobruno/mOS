/* Chamada SysV x86_64 usada pelo NSInvocation (foundation_extra.c).
 * No arm64 o caminho é cx_a64_call (ffi_arm64.S). */
#pragma once
#if !defined(__aarch64__)
typedef struct {
    unsigned long long x[6];     /* rdi rsi rdx rcx r8 r9 */
    unsigned long long nstack;   /* bytes, múltiplo de 16 */
    unsigned char *stack;
    unsigned char v[8][16];      /* xmm0..xmm7 */
    unsigned long long rax, rdx;
    unsigned char rv[16];        /* xmm0 na volta */
} cx_x64_frame_t;

void cx_x64_call(void *fn, cx_x64_frame_t *frame);

/* types[i] é o @encode do argumento i; vals[i] aponta para os bytes.
 * rettype/retlen descrevem o retorno. ret recebe o valor (sret > 16 bytes é escrito pelo callee).
 * Limitação: retorno long double ('D') fica em st(0) e não é copiado.
 * Argumento agregado maior que 32 bytes é truncado, como o slot do NSInvocation. */
void cx_sysv_invoke(void *fn, const char *rettype, unsigned long long retlen, const char **types,
                    const void *const *vals, unsigned nargs, void *ret);
#endif
