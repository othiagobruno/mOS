/* LocalAuthentication sem biometria e sem senha gravada.
 * O teste de host compila la_eval.c com -DCX_LA_HOST. */
#pragma once

enum {
    CX_LA_POLICY_BIOMETRICS = 1,
    CX_LA_POLICY_OWNER = 2,
    CX_LA_POLICY_WATCH = 3,
    CX_LA_POLICY_BIOMETRICS_OR_WATCH = 4
};

enum {
    CX_LA_ERR_PASSCODE_NOT_SET = -5,
    CX_LA_ERR_BIOMETRY_NOT_AVAILABLE = -6
};

/* 1 se a política pode ser avaliada. Neste sistema as quatro devolvem 0 e um erro. */
int cx_la_can_evaluate(int policy, int *error);
int cx_la_evaluate(int policy, int *error);
