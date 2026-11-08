/*
 * mOS — tradutor arm64: escolha do motor e invalidação de traduções.
 * O JIT (frente JIT) entra definindo A64_HAVE_JIT na build e fornecendo jit*.c.
 */
#include "a64.h"

int a64_run(a64_cpu *c, int engine) {
#ifdef A64_HAVE_JIT
    if (engine == A64_ENGINE_JIT) return a64_jit_run(c);
#else
    (void)engine;
#endif
    return a64_interp_run(c, 0);
}

void a64_invalidate(a64_u64 addr, a64_u64 len) {
#ifdef A64_HAVE_JIT
    a64_jit_invalidate(addr, len);
#else
    (void)addr; (void)len;
#endif
}
