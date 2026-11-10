/* LAContext: canEvaluatePolicy e evaluatePolicy.
 * Não há sensor nem senha. Nada é gravado. */
#include "foundation.h"
#include "la_eval.h"

id objc_autorelease(id o);

extern cx_class_t LAContext_c __asm__("OBJC_CLASS_$_LAContext");

typedef struct { void *isa; u64 flags; const char *s; u64 n; } la_cfk_t;
extern char cx_cfconst_isa2[] __asm__("__CFConstantStringClassReference");
static la_cfk_t la_err_dom = {cx_cfconst_isa2, 0x7c8, "com.apple.LocalAuthentication", 29};
const void *LAErrorDomain = &la_err_dom;
const double LATouchIDAuthenticationMaximumAllowableReuseDuration = 0;

typedef struct { void *isa; int flags, reserved; void (*invoke)(void *, BOOL, id); } la_reply_t;

static id la_error(int code) {
    id e;
    cx_class_t *k = cx_class_named("NSError");
    (void)code;
    e = k ? cx_alloc_instance(k) : cx_str("LAError");
    return objc_autorelease(e);
}

static BOOL la_can(id self, SEL c, long policy, id *error) {
    int err = 0;
    int ok = cx_la_can_evaluate((int)policy, &err);
    (void)self; (void)c;
    if (!ok && error) *error = la_error(err);
    if (ok && error) *error = 0;
    return ok ? YES : NO;
}

static void la_eval(id self, SEL c, long policy, id reason, void *reply) {
    la_reply_t *b = reply;
    int err = 0;
    int ok;
    (void)self; (void)c; (void)reason;
    ok = cx_la_evaluate((int)policy, &err);
    if (b && b->invoke) b->invoke(reply, ok ? YES : NO, ok ? 0 : la_error(err));
}

void cx_la_init(void) {
    if (!cx_responds(&LAContext_c, sel_registerName("canEvaluatePolicy:error:")))
        cx_add_method(&LAContext_c, "canEvaluatePolicy:error:", (void *)la_can);
    if (!cx_responds(&LAContext_c, sel_registerName("evaluatePolicy:localizedReason:reply:")))
        cx_add_method(&LAContext_c, "evaluatePolicy:localizedReason:reply:", (void *)la_eval);
}
