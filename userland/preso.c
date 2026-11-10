/* mOS — filho do parateste. Se o pai o criou com POSIX_SPAWN_START_SUSPENDED, esta função
 * só roda depois do SIGCONT. A marca no arquivo é a prova de que o processo chegou a executar. */
#include "mos.h"

void _start(const char *arg) {
    (void)arg;
    mkdir("/tmp");
    static const char msg[] = "rodou";
    write_file("/tmp/preso.saiu", msg, sizeof msg - 1);
    exit(42);
}
