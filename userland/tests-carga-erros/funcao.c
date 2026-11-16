/* mOS — função que nenhuma biblioteca exporta (ligada com -undefined dynamic_lookup): o carregador põe um stub
 * que chama cx_unresolved, que imprime o nome e sai com 127 (kernel/compat/macho.c, userland/libsystem/libc.c). */
#include <stdio.h>
extern int mos_funcao_inexistente(int);
int main(void) {
    printf("antes da chamada\n");
    fflush(stdout);
    int r = mos_funcao_inexistente(7);
    printf("não devia voltar: %d\n", r);
    return 0;
}
