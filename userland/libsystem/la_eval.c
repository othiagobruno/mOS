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
