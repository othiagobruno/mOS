#include "la_eval.h"

int cx_la_can_evaluate(int policy, int *error) {
    int err;
    if (policy == CX_LA_POLICY_OWNER) err = CX_LA_ERR_PASSCODE_NOT_SET;
    else err = CX_LA_ERR_BIOMETRY_NOT_AVAILABLE;
    if (error) *error = err;
    return 0;
}

int cx_la_evaluate(int policy, int *error) {
    return cx_la_can_evaluate(policy, error);
}

static int la_fill(int (*fn)(int, int *), int policy, int *code, const char **domain) {
    int err = 0;
    int ok = fn(policy, &err);
    if (code) *code = ok ? 0 : err;
    if (domain) *domain = ok ? 0 : CX_LA_ERROR_DOMAIN;
    return ok;
}

int cx_la_can_evaluate_policy(int policy, int *code, const char **domain) {
    return la_fill(cx_la_can_evaluate, policy, code, domain);
}

int cx_la_evaluate_policy(int policy, int *code, const char **domain) {
    return la_fill(cx_la_evaluate, policy, code, domain);
}
