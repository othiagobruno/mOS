/* mOS — processo filho de teste: mostra o argumento e sai com 42 */
#include "mos.h"

void _start(const char *arg) {
    print("filho: pid %d recebeu \"%s\"\n", (i64)getpid(), arg ? arg : "");
    exit(42);
}
