/* Host: a política de encerrar (GEV, SIGTERM, SIGKILL) sem o kernel. */
#include <stdio.h>
#include "../kernel/include/quitpol.h"

static int fails;
static void expect(int cond, const char *what) {
    if (cond) return;
    fprintf(stderr, "FALHOU %s\n", what);
    fails++;
}

int main(void) {
    expect(quitpol_step(QUIT_IDLE, 0, 0, 0) == QUIT_GEV, "primeiro pedido é o evento");
    expect(quitpol_step(QUIT_GEV, 0, 0, 2999) == QUIT_GEV, "dentro da folga fica no evento");
    expect(quitpol_step(QUIT_GEV, 0, 0, 3000) == QUIT_TERM, "3 s sobe para SIGTERM");
    expect(quitpol_step(QUIT_TERM, 0, 0, 1999) == QUIT_TERM, "dentro da espera do SIGTERM");
    expect(quitpol_step(QUIT_TERM, 0, 0, 2000) == QUIT_KILL, "2 s sobe para SIGKILL");
    expect(quitpol_step(QUIT_GEV, 0, 1, 0) == QUIT_TERM, "segundo pedido na folga escala");
    expect(quitpol_step(QUIT_TERM, 0, 1, 0) == QUIT_KILL, "segundo pedido no SIGTERM escala");
    expect(quitpol_step(QUIT_GEV, 1, 0, 0) == QUIT_KILL, "forçar vai ao SIGKILL");
    expect(quitpol_step(QUIT_IDLE, 1, 0, 0) == QUIT_KILL, "forçar do início vai ao SIGKILL");
    if (fails) { fprintf(stderr, "%d falha(s)\n", fails); return 1; }
    printf("quitpol: ok\n");
    return 0;
}
