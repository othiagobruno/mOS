/*
 * mOS libSystem — registro de unwind de código gerado em tempo de execução e __clear_cache.
 *
 * __register_frame / __deregister_frame: no macOS (libunwind da Apple, UnwindLevel1-gcc-ext.c) o argumento é
 * um FDE só, não uma seção .eh_frame inteira. O Electron (V8, wasm) e o LLVM ORC registram assim cada
 * função que geram. O FDE vai para a tabela dinâmica de cxx_unwind.c, que _Unwind_RaiseException consulta
 * quando o endereço não está em nenhuma imagem carregada. O ponteiro tem de continuar válido até o
 * __deregister_frame, como na Apple.
 *
 * __clear_cache: no x86_64 o cache de instruções é coerente com as escritas de dados (o compiler-rt da
 * LLVM não faz nada nessa arquitetura). A barreira de compilador impede que as escritas do código novo
 * sejam movidas para depois da chamada; o salto para o código já basta para o processador.
 *
 * Só x86_64: o libSystem arm64 liga o libunwind (que tem os dois primeiros) e o compiler-rt.
 */
#if defined(__x86_64__)
int mos_uw_dyn_add(const void *fde);
void mos_uw_dyn_remove(const void *fde);

void mos_register_frame(const void *fde) __asm__("__register_frame");
void mos_register_frame(const void *fde) { (void)mos_uw_dyn_add(fde); }

void mos_deregister_frame(const void *fde) __asm__("__deregister_frame");
void mos_deregister_frame(const void *fde) { mos_uw_dyn_remove(fde); }

void mos_clear_cache(void *start, void *end) __asm__("__clear_cache");
void mos_clear_cache(void *start, void *end) {
    (void)start;
    (void)end;
    __asm__ volatile("" ::: "memory");
}
#endif
