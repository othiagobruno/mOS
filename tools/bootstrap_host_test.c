#include "host_bootstrap.h"
#include "../kernel/include/syscall.h"
#include <stdio.h>
#include <stdlib.h>

static unsigned next = 0x21000;
static unsigned mk(void) { return next++; }

static void die(const char *w, long r) { fprintf(stderr, "falhou %s (%ld)\n", w, r); exit(1); }

int main(void) {
    unsigned p = 0, q = 0;
    const char *svc = "BQR82RBBHL.com.tinyspeck.slackmacgap.MachPortRendezvousServer.42";
    long r = host_bootstrap(MACH_BS_CHECK_IN, svc, &p, mk);
    if (r || p < 0x21000) die("check_in", r ? r : (long)p);
    r = host_bootstrap(MACH_BS_CHECK_IN, svc, &q, mk);
    if (r != 1103) die("ativo", r);
    q = 99;
    r = host_bootstrap(MACH_BS_LOOK_UP, svc, &q, mk);
    if (r || q != p) die("look_up", r ? r : (long)q);
    q = 7;
    r = host_bootstrap(MACH_BS_LOOK_UP, "nao.existe", &q, mk);
    if (r != 1102 || q != 0) die("desconhecido", r);
    printf("bootstrap ok porto=%u\n", p);
    return 0;
}
