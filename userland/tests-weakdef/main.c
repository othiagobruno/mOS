/* mOS — coalescência de definições weak entre o executável e uma dylib (kernel/compat/macho.c, coalesced_export).
 * Imprime "weakdef-teste: N/N ok". */
#include <stdio.h>
__attribute__((weak, visibility("default"))) int coalesce_val = 10;   /* weak aqui e na lib: ganha a do executável */
/* forte aqui, weak na lib: ganha a forte. used/noinline: o linker do zig descarta do trie de exportação do
 * executável o global que só é chamado inline (o ld64 exportaria). */
__attribute__((used, noinline, visibility("default"))) int quem(void) { return 1; }
int lib_quem(void);
int *lib_val(void);
int lib_so(void);
static int ok, total;
#define CHECK(c, m) do { total++; if (c) ok++; else fprintf(stderr, "FALHOU: %s\n", m); } while (0)
int main(void) {
    CHECK(lib_val() == &coalesce_val, "variável weak: a lib usa a do executável");
    CHECK(*lib_val() == 10, "variável weak: valor do executável");
    CHECK(lib_quem() == 1, "função weak da lib sobrescrita pela forte do executável");
    CHECK(quem() == 1, "executável usa a própria");
    CHECK(lib_so() == 30, "weak só na lib continua da lib");
    printf("weakdef-teste: %d/%d ok\n", ok, total);
    return ok == total ? 0 : 1;
}
