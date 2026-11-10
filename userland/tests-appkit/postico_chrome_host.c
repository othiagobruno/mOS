/* Regra do indicador parado (NSProgressIndicator.displayedWhenStopped).
 * gcc -std=c11 -Wall -Wextra userland/tests-appkit/postico_chrome_host.c -o /tmp/postico-chrome-test && /tmp/postico-chrome-test
 */
#include "../libsystem/progress_draw.h"
#include <stdio.h>

static int ok, total;

static void check(int cond, const char *what) {
    total++;
    if (cond) ok++;
    printf("%s %s\n", cond ? "  ok  " : "  FALHA", what);
}

int main(void) {
    check(cx_progress_draws(1, 0, 0), "com displayedWhenStopped a barra parada aparece");
    check(cx_progress_draws(0, 0, 0) == 0, "sem a flag a barra parada some");
    check(cx_progress_draws(0, 40, 0), "sem a flag o valor no meio continua visível");
    check(cx_progress_draws(0, 10, 10) == 0, "no mínimo e sem a flag conta como parado");
    printf("postico-chrome: %d/%d %s\n", ok, total, ok == total ? "ok" : "FALHA");
    return ok == total ? 0 : 1;
}
