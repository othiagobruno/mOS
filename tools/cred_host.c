/* Teste de host: SETSIGDEF, SETSIGMASK, RESETIDS, modo do open e seteuid.
 * Compila kernel/proc/cred.c. Não sobe o kernel. */
#include "cred.h"
#include <stdio.h>

static int falhas;

static void check(int cond, const char *msg) {
    if (!cond) { printf("FALHA: %s\n", msg); falhas++; }
}

int main(void) {
    unsigned long long h[32] = {0};
    unsigned mask = 0;
    int uid = 501, euid = 0, suid = 0, gid = 20, egid = 0, sgid = 0;
    h[15] = 1;
    h[2] = 0x1000;
    cred_spawn_apply(h, &mask, &uid, &euid, &suid, &gid, &egid, &sgid, 1u << 15, (1u << 9) | (1u << 2), 1 | 2 | 4);
    check(h[15] == 0, "SETSIGDEF tira o ignorar");
    check(h[2] == 0x1000, "SETSIGDEF não mexe no tratador de outro sinal");
    check((mask & (1u << 9)) == 0 && (mask & (1u << 20)) == 0, "SETSIGMASK não bloqueia KILL nem STOP");
    check((mask & (1u << 2)) != 0, "SETSIGMASK grava o bit pedido");
    check(euid == 501 && suid == 501 && egid == 20 && sgid == 20, "RESETIDS volta o efetivo para o real");

    euid = 0;
    cred_spawn_apply(h, &mask, &uid, &euid, &suid, &gid, &egid, &sgid, 0, 0, 0);
    check(euid == 0, "sem RESETIDS o euid do pai fica");

    unsigned slot = 0;
    slot = mode_for_creat(0700);
    check(mode_stat(slot, 0) == 0700, "open O_CREAT 0700 volta no stat");
    slot = mode_for_creat(0);
    check(mode_stat(slot, 0) == 0644, "open O_CREAT sem mode vale 0644");
    mode_apply(&slot, 0);
    check(mode_stat(slot, 0) == 0, "chmod 0000 é lido de volta");
    mode_apply(&slot, 0750);
    check(mode_stat(slot, 0) == 0750, "chmod 0750 é lido de volta");
    check(mode_stat(0, 1) == 0755, "pasta sem mode explícito continua 0755");

    int eff = 501;
    check(cred_seteid(501, 501, &eff, 0) == -1 && eff == 501, "seteuid de outro id é recusado");
    check(cred_seteid(501, 0, &eff, 0) == 0 && eff == 0, "seteuid para o saved funciona");
    check(cred_seteid(501, 0, &eff, 501) == 0 && eff == 501, "seteuid volta para o real");

    if (falhas) { printf("cred_host: %d falhas\n", falhas); return 1; }
    printf("cred_host: ok\n");
    return 0;
}
