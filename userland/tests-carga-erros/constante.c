/* mOS — constante (nome de dado na convenção da Apple) que nenhuma biblioteca exporta: o carregador liga o GOT a
 * uma área zerada, então a leitura dá 0/nil em vez de ler as instruções de um stub (data_like em macho.c). */
#include <stdio.h>
extern void *kMOSConstanteInexistente;
extern const char *MOSInexistenteNotification;
int main(void) {
    void *a = kMOSConstanteInexistente;
    const char *b = MOSInexistenteNotification;
    printf("constante-teste: k=%p notif=%p\n", a, (void *)b);
    return (a == NULL && b == NULL) ? 0 : 1;
}
