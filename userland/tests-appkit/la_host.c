/* gcc -std=c11 -Wall -Wextra -DCX_LA_HOST -Iuserland/libsystem \
      userland/libsystem/la_eval.c userland/tests-appkit/la_host.c -o /tmp/la-host
*/
#include "la_eval.h"
#include <stdio.h>

static int ok, total;
static void check(int cond, const char *what) {
    total++;
    if (cond) ok++;
    printf("%s %s\n", cond ? "  ok  " : "  FALHA", what);
}

int main(void) {
    int err = 0;
    check(cx_la_can_evaluate(CX_LA_POLICY_BIOMETRICS, &err) == 0 && err == CX_LA_ERR_BIOMETRY_NOT_AVAILABLE,
          "biometria ausente devolve biometryNotAvailable");
    err = 0;
    check(cx_la_can_evaluate(CX_LA_POLICY_BIOMETRICS_OR_WATCH, &err) == 0 && err == CX_LA_ERR_BIOMETRY_NOT_AVAILABLE,
          "biometria ou relógio também não existe");
    err = 0;
    check(cx_la_can_evaluate(CX_LA_POLICY_OWNER, &err) == 0 && err == CX_LA_ERR_PASSCODE_NOT_SET,
          "sem senha de aparelho devolve passcodeNotSet");
    err = 99;
    check(cx_la_evaluate(CX_LA_POLICY_OWNER, &err) == 0 && err == CX_LA_ERR_PASSCODE_NOT_SET,
          "evaluatePolicy repete o mesmo erro e não autentica");
    check(cx_la_can_evaluate(CX_LA_POLICY_WATCH, 0) == 0, "erro nulo não derruba a consulta");
    printf("la-host: %d/%d ok\n", ok, total);
    return ok == total ? 0 : 1;
}
