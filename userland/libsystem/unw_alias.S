/* unw_getcontext forte. O libunwind do zig exporta o nome fraco, e o machocheck
 * (e o carregador) ignoram símbolo fraco: o nome some mesmo com o corpo em
 * __unw_getcontext. Este salto é o mesmo contexto. */
#if !defined(__aarch64__)
    .text
    .globl unw_getcontext
    .type unw_getcontext, @function
    .p2align 4
unw_getcontext:
    jmp __unw_getcontext
    .size unw_getcontext, .-unw_getcontext
    .section .note.GNU-stack, "", @progbits
#endif
