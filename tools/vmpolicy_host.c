/* Host: política de memória (kernel/include/vmpolicy.h): pressão, rlimit e jetsam.
 * gcc -O2 -Wall -Wextra -I kernel/include tools/vmpolicy_host.c -o /tmp/vmpolicy_host && /tmp/vmpolicy_host */
#include <stdio.h>
#include "vmpolicy.h"

static int falhas;
static void check(int c, const char *m) { if (!c) { printf("FALHA: %s\n", m); falhas++; } }

int main(void) {
    /* pressão: piso de 1000 páginas */
    check(vmp_pressure_level(5000, 1000) == 1 && vmp_pressure_level(999, 1000) == 2 && vmp_pressure_level(499, 1000) == 4,
          "normal / aviso abaixo do piso / crítico abaixo da metade");
    check(vmp_pressure_level(0, 0) == 1, "sem piso (sem swap): sempre normal");
    check(vmp_memorystatus_level(250, 1000) == 25 && vmp_memorystatus_level(1, 0) == 100, "memorystatus_level em %");

    /* rlimit */
    unsigned long long c, m;
    vmp_rlim_default(VMP_RLIMIT_NOFILE, 256, 2u << 20, &c, &m);
    check(c == 256 && m == VMP_RLIM_INF, "NOFILE = PROC_MAX_FDS, teto infinito como no macOS");
    vmp_rlim_default(VMP_RLIMIT_CORE, 256, 2u << 20, &c, &m);
    check(c == 0 && m == VMP_RLIM_INF, "CORE 0 com teto infinito");
    vmp_rlim_default(VMP_RLIMIT_STACK, 256, 2u << 20, &c, &m);
    check(c == (2u << 20), "STACK = pilha real do kernel");
    unsigned long long oc, om;
    check(vmp_rlim_set(VMP_RLIMIT_DATA, VMP_RLIM_INF, 1 << 20, 1 << 22, 0, 0, &oc, &om) == 0 && oc == 1 << 20 && om == 1 << 22,
          "baixar é livre");
    check(vmp_rlim_set(VMP_RLIMIT_DATA, 1 << 22, 1 << 20, 1 << 23, 0, 0, &oc, &om) == -1, "subir o max sem root: EPERM");
    check(vmp_rlim_set(VMP_RLIMIT_DATA, 1 << 22, 1 << 20, 1 << 23, 1, 0, &oc, &om) == 0, "root sobe o max");
    check(vmp_rlim_set(VMP_RLIMIT_DATA, VMP_RLIM_INF, 5, 4, 1, 0, &oc, &om) == -22, "cur > max: EINVAL");
    check(vmp_rlim_set(VMP_RLIMIT_N, VMP_RLIM_INF, 1, 1, 1, 0, &oc, &om) == -22, "recurso inválido");
    check(vmp_rlim_set(VMP_RLIMIT_NOFILE, 256, 10240, 10240, 0, 256, &oc, &om) == -1, "max já baixado não volta a subir sem root");
    check(vmp_rlim_set(VMP_RLIMIT_NOFILE, VMP_RLIM_INF, 10240, VMP_RLIM_INF, 0, 256, &oc, &om) == 0 && oc == 256 && om == VMP_RLIM_INF,
          "NOFILE pedido maior fica no teto do kernel");

    /* jetsam */
    int pid[5] = {1, 20, 21, 22, 23}, pr[5] = {0, 10, 3, 3, 180};
    unsigned long long fp[5] = {9999, 5000, 100, 800, 99999};
    check(vmp_jetsam_pick(5, pid, pr, fp) == 3, "menor prioridade, maior footprint; pid 1 e crítico nunca");
    int pr2[2] = {200, 190}, pid2[2] = {5, 6};
    unsigned long long fp2[2] = {10, 20};
    check(vmp_jetsam_pick(2, pid2, pr2, fp2) == -1, "só críticos: ninguém");
    check(vmp_over_limit(100, 20, 110) && !vmp_over_limit(100, 10, 110) && !vmp_over_limit(1u << 30, 0, 0),
          "limite conta residente + comprimido; 0 = sem limite");

    if (falhas) { printf("vmpolicy_host: %d falhas\n", falhas); return 1; }
    printf("vmpolicy_host: ok\n");
    return 0;
}
