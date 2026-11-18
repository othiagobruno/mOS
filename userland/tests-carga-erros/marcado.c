/* mOS — rebase de ponteiro com o byte alto marcado (String do Swift -O em __DATA,__const: bit 63 + endereço;
 * RTTI não único da libc++): o carregador soma o slide nos 56 bits baixos e preserva a marca, nos dois slices. */
#include <stdio.h>
#include <stdint.h>
static const char texto[] = "a.b.c";
/* volatile: o valor tem de vir do __DATA,__const rebaseado, não de um lea dobrado pelo compilador */
static const char *const volatile marcado = texto + 0x8000000000000000ULL;
int main(void) {
    uintptr_t v = (uintptr_t)marcado;
    int ok = (v >> 63) == 1 && (v & 0x00FFFFFFFFFFFFFFULL) == (uintptr_t)texto;
    printf("marcado-teste: %s v=%#lx texto=%p\n", ok ? "ok" : "FALHOU", (unsigned long)v, (const void *)texto);
    return ok ? 0 : 1;
}
