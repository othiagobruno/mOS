/* mOS — dependências com caminho absoluto que não existem no disco. Fortes: a carga é recusada com
 * "requer a, b, c, d e mais N, que o mOS ainda não tem". Fracas (LC_LOAD_WEAK_DYLIB): o processo abre e o
 * símbolo fraco vale NULL. */
#include <stdio.h>
extern int mos_lib_ausente(void) __attribute__((weak_import));
int main(void) {
    printf("dylib-teste: aberto, simbolo=%s\n", mos_lib_ausente ? "presente" : "NULL");
    return mos_lib_ausente ? 1 : 0;
}
