/*
 * StoreKit: STUB LOCAL. canMakePayments é NÃO. addPayment: não conclui compra.
 * cx_storekit_verify_receipt sempre falha. Não há recibo da Apple nem FairPlay.
 *
 * UserNotifications: autorização local no mesmo arquivo do TCC (serviço
 * "userNotifications"). Não há APNs. O completion do request só roda quando a
 * Interface chama cx_un_decide. Pedido, delegate e banner ficam em usernotifications.c.
 */
#include "foundation.h"
#include "libc.h"
#include "storekit_stub.inc"

void *_Block_copy(const void *b);
void _Block_release(const void *b);
id objc_autorelease(id o);
/* o nome de link é o OBJC_CLASS_$_… de vscode_fwclasses.c; sem o asm o zig não acha o símbolo */
extern cx_class_t SKPaymentQueue_c __asm__("OBJC_CLASS_$_SKPaymentQueue");
extern cx_class_t UNUserNotificationCenter_c __asm__("OBJC_CLASS_$_UNUserNotificationCenter");

int cx_storekit_verify_receipt(const void *bytes, size_t n, char *err, size_t cap) {
    return sk_verify(bytes, n, err, cap);
}

static id g_sk_queue;
static id sk_default(id cls, SEL c) {
    (void)c;
    if (!g_sk_queue) g_sk_queue = cx_alloc_instance((cx_class_t *)cls);
    return g_sk_queue;
}
static BOOL sk_can(id self, SEL c) { (void)self; (void)c; return NO; }
static void sk_add(id self, SEL c, id payment) { (void)self; (void)c; (void)payment; }
static id sk_tx(id self, SEL c) {
    (void)self; (void)c;
    return objc_autorelease(cx_arr_new(cx_class_named("NSMutableArray")));
}
static void sk_restore(id self, SEL c) { (void)self; (void)c; }

int cx_tcc_request(const char *service);
int cx_tcc_state(const char *service);
int cx_tcc_decide(const char *bundle, const char *service, int granted);

static void *g_un_block;

static id g_un;
static id un_current_real(id cls, SEL c) {
    (void)c;
    if (!g_un) g_un = cx_alloc_instance((cx_class_t *)cls);
    return g_un;
}

static void un_request(id self, SEL c, unsigned long opts, void *block) {
    (void)self; (void)c; (void)opts;
    cx_tcc_request("userNotifications");
    if (g_un_block) _Block_release(g_un_block);
    g_un_block = block ? _Block_copy(block) : 0;
}

static long un_auth(id self, SEL c) {
    (void)self; (void)c;
    int st = cx_tcc_state("userNotifications");
    if (st == 2) return 2; /* UNAuthorizationStatusAuthorized */
    if (st == 1) return 1; /* denied */
    return 0;              /* notDetermined, inclusive pending */
}

void cx_un_decide(int granted) {
    cx_tcc_decide(0, "userNotifications", granted);
    if (!g_un_block) return;
    typedef struct { void *isa; int flags; int reserved; void (*invoke)(void *, int, id); } un_blk_t;
    un_blk_t *b = (un_blk_t *)g_un_block;
    if (b->invoke) b->invoke(g_un_block, granted ? 1 : 0, 0);
}

void cx_userspace_services_init(void) {
    cx_add_cmethod(&SKPaymentQueue_c, "defaultQueue", (void *)sk_default);
    cx_add_cmethod(&SKPaymentQueue_c, "canMakePayments", (void *)sk_can);
    cx_add_method(&SKPaymentQueue_c, "addPayment:", (void *)sk_add);
    cx_add_method(&SKPaymentQueue_c, "transactions", (void *)sk_tx);
    cx_add_method(&SKPaymentQueue_c, "restoreCompletedTransactions", (void *)sk_restore);
    cx_add_cmethod(&UNUserNotificationCenter_c, "currentNotificationCenter", (void *)un_current_real);
    cx_add_method(&UNUserNotificationCenter_c, "requestAuthorizationWithOptions:completionHandler:", (void *)un_request);
    cx_add_method(&UNUserNotificationCenter_c, "authorizationStatus", (void *)un_auth);
}
