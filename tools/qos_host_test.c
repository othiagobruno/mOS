#include "host_qos.h"
#include "../kernel/include/syscall.h"
#include <stdio.h>
#include <stdlib.h>

static void die(const char *w, long r) { fprintf(stderr, "falhou %s (%ld)\n", w, r); exit(1); }

int main(void) {
    long r;
    host_thr_boot();
    if (host_thr_qos(1) != 2) die("padrao", host_thr_qos(1));
    r = host_thr_setqos(0, 1, 0x19);
    if (r || host_thr_qos(1) != 1) die("iniciado", r ? r : host_thr_qos(1));
    r = host_thr_setqos(1, 1, 0x09);
    if (r || host_thr_qos(1) != 4) die("fundo", r ? r : host_thr_qos(1));
    r = host_thr_setqos(0, 1, 0x99);
    if (r || host_thr_qos(1) != 2) die("desconhecida", r ? r : host_thr_qos(1));
    r = host_thr_setqos(7, 1, 0x21);
    if (r != -MOS_ESRCH) die("esrch", r);
    host_thr_add(4);
    r = host_thr_setqos(4, 1, 0x21);
    if (r || host_thr_qos(4) != 0) die("interativo", r ? r : host_thr_qos(4));
    host_thr_dead(4);
    r = host_thr_setqos(4, 1, 0x21);
    if (r != -MOS_ESRCH) die("morta", r);
    if (host_qos_from_darwin(0x15) != 2 || host_qos_from_darwin(0x11) != 3 || host_qos_from_darwin(0) != 2)
        die("mapa", 0);
    printf("setqos ok\n");
    return 0;
}
