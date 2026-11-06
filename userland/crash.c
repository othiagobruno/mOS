/* mOS — processo que tenta escrever na memória do kernel: deve ser encerrado sem afetar o sistema */
#include "mos.h"

void _start(const char *arg) {
    (void)arg;
    puts("crash: tentando escrever no endereço 0x100000 (kernel)...\n");
    *(volatile u64 *)0x100000 = 0xDEAD;
    puts("crash: ERRO, a escrita passou!\n");
    exit(1);
}
