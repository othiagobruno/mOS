/* StoreKit no libSystem.
 *
 * A fila está em storekit_core.inc. addPayment entrega .purchasing e em
 * seguida .failed com SKErrorCloudServiceNetworkConnectionFailed (7), na
 * thread da fila (a main). Fora dela os avisos ficam pendentes e o drain
 * corre na main. SKProductsRequest chama request:didFailWithError: com o
 * mesmo código. Não há resposta com produto. restoreCompletedTransactions
 * chama restoreCompletedTransactionsFailedWithError:. StoreKit 2
 * (products, purchase, sync) completa a continuação com StoreKitError.networkError.
 * currentEntitlements e updates não emitem transação. Sem rede, sem recibo,
 * sem FairPlay.
 */
#include "foundation.h"
#include "libc.h"
#include "storekit_stub.inc"
#include "storekit_core.inc"

void cx_mark_stub_class(cx_class_t *c);
id objc_autorelease(id o);
id objc_retain(id o);
int pthread_main_np(void);
void *dispatch_get_main_queue(void);
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *));

extern cx_class_t SKPaymentQueue_c __asm__("OBJC_CLASS_$_SKPaymentQueue");
extern cx_class_t SKProductsRequest_c __asm__("OBJC_CLASS_$_SKProductsRequest");
extern cx_class_t SKMutablePayment_c __asm__("OBJC_CLASS_$_SKMutablePayment");
extern cx_class_t NSBundle_c __asm__("OBJC_CLASS_$_NSBundle");

#define SKC(N) cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N); static cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N);
SKC(SKProduct)
SKC(SKProductsResponse)
SKC(SKPaymentTransaction)

typedef struct {
    id key, delegate, username;
    int tx, cancelled, qty;
    char pid[160];
} sk_slot;

static sk_slot slots[48];
static id g_queue, g_pay_for_bind, g_tx_obj[SK_TX_MAX];
static int g_tx_id[SK_TX_MAX];

static int responds(id o, const char *sel) {
    return o && cx_responds(o->isa, sel_registerName(sel));
}
static sk_slot *slot_of(id o) {
    int i;
    for (i = 0; i < 48; i++) if (slots[i].key == o) return &slots[i];
    return 0;
}
static sk_slot *slot_make(id o) {
    sk_slot *s = slot_of(o);
    int i;
    if (s) return s;
    for (i = 0; i < 48; i++) if (!slots[i].key) {
        memset(&slots[i], 0, sizeof slots[i]);
        slots[i].key = o;
        slots[i].qty = 1;
        return &slots[i];
    }
    return 0;
}
static int fw_tid(void) { return pthread_main_np() ? 1 : 0; }
static void drain_main(void *unused) {
    (void)unused;
    sk_core_drain();
}
static void sk_flush(void) {
    if (!sk_core_pending()) return;
    if (pthread_main_np()) sk_core_drain();
    else dispatch_async_f(dispatch_get_main_queue(), 0, drain_main);
}
static id sk_err(long code, const char *msg) {
    id ui = MSG(id, (id)cx_class_named("NSDictionary"), "dictionaryWithObject:forKey:", cx_str(msg), cx_str("NSLocalizedDescription"));
    return MSG(id, (id)cx_class_named("NSError"), "errorWithDomain:code:userInfo:", cx_str("SKErrorDomain"), code, ui);
}
static id tx_obj(int tx) {
    int i;
    for (i = 0; i < SK_TX_MAX; i++) if (g_tx_id[i] == tx && g_tx_obj[i]) return g_tx_obj[i];
    return 0;
}
static void tx_bind(int tx, id obj) {
    int i;
    for (i = 0; i < SK_TX_MAX; i++) if (!g_tx_obj[i]) {
        g_tx_id[i] = tx;
        g_tx_obj[i] = obj;
        return;
    }
}
static void tx_unbind(int tx) {
    int i;
    for (i = 0; i < SK_TX_MAX; i++) if (g_tx_id[i] == tx) {
        g_tx_id[i] = 0;
        g_tx_obj[i] = 0;
    }
}

static void on_tx(void *ctx, int tx, int state, int err) {
    id obs = (id)ctx;
    id obj = tx_obj(tx);
    sk_slot *s;
    id one;
    if (!obj) return;
    s = slot_make(obj);
    if (s) {
        s->tx = state;
        if (err) s->delegate = objc_retain(sk_err(err, "não foi possível contatar a App Store (SKErrorCloudServiceNetworkConnectionFailed)"));
    }
    if (!responds(obs, "paymentQueue:updatedTransactions:")) return;
    one = cx_arr_new(cx_class_named("NSMutableArray"));
    cx_arr_add(one, obj);
    MSG(void, obs, "paymentQueue:updatedTransactions:", g_queue, one);
}
static void on_restore(void *ctx, int err) {
    id obs = (id)ctx;
    if (!responds(obs, "paymentQueue:restoreCompletedTransactionsFailedWithError:")) return;
    MSG(void, obs, "paymentQueue:restoreCompletedTransactionsFailedWithError:", g_queue,
        sk_err(err, "não foi possível contatar a App Store (SKErrorCloudServiceNetworkConnectionFailed)"));
}

static id sk_default(id cls, SEL c) {
    (void)c;
    if (!g_queue) g_queue = cx_alloc_instance((cx_class_t *)cls);
    return g_queue;
}
static BOOL sk_can(id self, SEL c) {
    (void)self;
    (void)c;
    return sk_core_can_pay() ? YES : NO;
}
static void sk_add_obs(id self, SEL c, id obs) {
    (void)self;
    (void)c;
    if (!obs) return;
    if (sk_core_add_observer(obs, on_tx, on_restore) == 2) objc_retain(obs);
    sk_flush();
}
static void sk_rm_obs(id self, SEL c, id obs) {
    (void)self;
    (void)c;
    sk_core_remove_observer(obs);
}
static id sk_transactions(id self, SEL c) {
    id a = cx_arr_new(cx_class_named("NSMutableArray"));
    int i;
    (void)self;
    (void)c;
    for (i = 0; i < SK_TX_MAX; i++) if (g_tx_obj[i]) cx_arr_add(a, g_tx_obj[i]);
    return a;
}
static void bind_tx(int tok) {
    id tx = cx_alloc_instance(&SKPaymentTransaction_c);
    sk_slot *s = slot_make(tx);
    if (s) {
        s->tx = SK_TX_PURCHASING;
        if (g_pay_for_bind) s->username = objc_retain(g_pay_for_bind);
    }
    tx_bind(tok, tx);
}
static void sk_add(id self, SEL c, id payment) {
    sk_slot *ps = slot_of(payment);
    (void)self;
    (void)c;
    g_pay_for_bind = payment;
    sk_core_add_payment(ps ? ps->pid : "", ps ? ps->qty : 1);
    g_pay_for_bind = 0;
    sk_flush();
}
static void sk_finish(id self, SEL c, id tx) {
    sk_slot *s = slot_of(tx);
    int i;
    (void)self;
    (void)c;
    if (!s) return;
    for (i = 0; i < SK_TX_MAX; i++) if (g_tx_obj[i] == tx) {
        sk_core_finish(g_tx_id[i]);
        if (!sk_core_tx_get(g_tx_id[i], 0, 0, 0, 0)) tx_unbind(g_tx_id[i]);
        return;
    }
}
static void sk_restore(id self, SEL c) {
    (void)self;
    (void)c;
    sk_core_restore();
    sk_flush();
}

static id pay_with_id(id cls, SEL c, id pid) {
    id p = cx_alloc_instance((cx_class_t *)cls);
    sk_slot *s = slot_make(p);
    (void)c;
    if (s && pid) {
        const char *z = cx_cstr(pid);
        int i = 0;
        if (!z) z = "";
        for (; z[i] && i + 1 < (int)sizeof s->pid; i++) s->pid[i] = z[i];
        s->pid[i] = 0;
    }
    return objc_autorelease(p);
}
static id pay_with_product(id cls, SEL c, id product) {
    id pid = product && responds(product, "productIdentifier") ? MSG0(id, product, "productIdentifier") : 0;
    return pay_with_id(cls, c, pid);
}
static id pay_pid(id self, SEL c) {
    sk_slot *s = slot_of(self);
    (void)c;
    return s && s->pid[0] ? objc_autorelease(cx_str(s->pid)) : 0;
}
static void pay_set_pid(id self, SEL c, id pid) {
    sk_slot *s = slot_make(self);
    const char *z = pid ? cx_cstr(pid) : "";
    int i = 0;
    (void)c;
    if (!s) return;
    if (!z) z = "";
    for (; z[i] && i + 1 < (int)sizeof s->pid; i++) s->pid[i] = z[i];
    s->pid[i] = 0;
}
static unsigned long pay_qty(id self, SEL c) {
    sk_slot *s = slot_of(self);
    (void)c;
    return s ? (unsigned long)s->qty : 1;
}
static void pay_set_qty(id self, SEL c, unsigned long q) {
    sk_slot *s = slot_make(self);
    (void)c;
    if (s) s->qty = q ? (int)q : 1;
}
static id pay_user(id self, SEL c) {
    sk_slot *s = slot_of(self);
    (void)c;
    return s ? s->username : 0;
}
static void pay_set_user(id self, SEL c, id u) {
    sk_slot *s = slot_make(self);
    (void)c;
    if (s) s->username = u ? objc_retain(u) : 0;
}

static id prod_pid(id self, SEL c) { return pay_pid(self, c); }
static id prod_title(id self, SEL c) {
    (void)self;
    (void)c;
    return objc_autorelease(cx_str(""));
}
static id prod_price(id self, SEL c) {
    (void)self;
    (void)c;
    return objc_autorelease(cx_num_d(0));
}

static long tx_state(id self, SEL c) {
    sk_slot *s = slot_of(self);
    (void)c;
    if (!s) return SK_TX_FAILED;
    if (s->tx == SK_TX_PURCHASED || s->tx == SK_TX_RESTORED) return SK_TX_FAILED;
    return s->tx;
}
static id tx_err(id self, SEL c) {
    sk_slot *s = slot_of(self);
    (void)c;
    return s ? s->delegate : 0;
}
static id tx_pay(id self, SEL c) {
    sk_slot *s = slot_of(self);
    (void)c;
    return s ? s->username : 0;
}
static id tx_nil(id self, SEL c) {
    (void)self;
    (void)c;
    (void)sk_policy_receipt_len;
    return 0;
}

static id req_init(id self, SEL c, id ids) {
    sk_slot *s = slot_make(self);
    (void)c;
    (void)ids;
    if (s) s->delegate = 0;
    return self;
}
static void req_set_del(id self, SEL c, id del) {
    sk_slot *s = slot_make(self);
    (void)c;
    if (s) s->delegate = del;
}
static id req_del(id self, SEL c) {
    sk_slot *s = slot_of(self);
    (void)c;
    return s ? s->delegate : 0;
}
static void req_cancel(id self, SEL c) {
    sk_slot *s = slot_make(self);
    (void)c;
    if (s) s->cancelled = 1;
}
static void on_req(void *ctx, int err) {
    id self = (id)ctx;
    sk_slot *s = slot_of(self);
    if (!s || !s->delegate || !responds(s->delegate, "request:didFailWithError:")) return;
    MSG(void, s->delegate, "request:didFailWithError:", self, sk_err(err, "não foi possível contatar a App Store (SKErrorCloudServiceNetworkConnectionFailed)"));
}
static void req_start(id self, SEL c) {
    sk_slot *s = slot_of(self);
    (void)c;
    sk_core_request(self, on_req, s ? s->cancelled : 1);
    sk_flush();
}

static id receipt_url(id self, SEL c) {
    (void)self;
    (void)c;
    return 0;
}

int cx_storekit_verify_receipt(const void *bytes, size_t n, char *err, size_t cap) {
    return sk_verify(bytes, n, err, cap);
}

void sk2_products_for(int n_ids, sk2_cont cont, void *ctx) { sk_core_products(n_ids, cont, ctx); }
void sk2_purchase(const char *pid, sk2_cont cont, void *ctx) { sk_core_purchase(pid, cont, ctx); }
void sk2_appstore_sync(sk2_cont cont, void *ctx) { sk_core_sync(cont, ctx); }
int sk2_entitlements_next(void) { return sk_core_entitlements_next(); }
int sk2_updates_next(void) { return sk_core_updates_next(); }

static void sk_def(cx_class_t *c, cx_class_t *m, const char *name) {
    cx_define_class(c, m, name, cx_class_named("NSObject"), 0);
    cx_mark_stub_class(c);
}

void cx_storekit_init(void) {
    sk_core_set_tid(fw_tid);
    sk_core_set_bind(bind_tx);
    sk_core_bind_queue(1);
    sk_def(&SKProduct_c, &SKProduct_m, "SKProduct");
    sk_def(&SKProductsResponse_c, &SKProductsResponse_m, "SKProductsResponse");
    sk_def(&SKPaymentTransaction_c, &SKPaymentTransaction_m, "SKPaymentTransaction");
    cx_add_cmethod(&SKPaymentQueue_c, "defaultQueue", (void *)sk_default);
    cx_add_cmethod(&SKPaymentQueue_c, "canMakePayments", (void *)sk_can);
    cx_add_method(&SKPaymentQueue_c, "addPayment:", (void *)sk_add);
    cx_add_method(&SKPaymentQueue_c, "transactions", (void *)sk_transactions);
    cx_add_method(&SKPaymentQueue_c, "restoreCompletedTransactions", (void *)sk_restore);
    cx_add_method(&SKPaymentQueue_c, "addTransactionObserver:", (void *)sk_add_obs);
    cx_add_method(&SKPaymentQueue_c, "removeTransactionObserver:", (void *)sk_rm_obs);
    cx_add_method(&SKPaymentQueue_c, "finishTransaction:", (void *)sk_finish);
    cx_add_cmethod(&SKMutablePayment_c, "paymentWithProduct:", (void *)pay_with_product);
    cx_add_cmethod(&SKMutablePayment_c, "paymentWithProductIdentifier:", (void *)pay_with_id);
    cx_add_method(&SKMutablePayment_c, "productIdentifier", (void *)pay_pid);
    cx_add_method(&SKMutablePayment_c, "setProductIdentifier:", (void *)pay_set_pid);
    cx_add_method(&SKMutablePayment_c, "quantity", (void *)pay_qty);
    cx_add_method(&SKMutablePayment_c, "setQuantity:", (void *)pay_set_qty);
    cx_add_method(&SKMutablePayment_c, "applicationUsername", (void *)pay_user);
    cx_add_method(&SKMutablePayment_c, "setApplicationUsername:", (void *)pay_set_user);
    cx_add_method(&SKProduct_c, "productIdentifier", (void *)prod_pid);
    cx_add_method(&SKProduct_c, "localizedTitle", (void *)prod_title);
    cx_add_method(&SKProduct_c, "localizedDescription", (void *)prod_title);
    cx_add_method(&SKProduct_c, "price", (void *)prod_price);
    cx_add_method(&SKProductsRequest_c, "initWithProductIdentifiers:", (void *)req_init);
    cx_add_method(&SKProductsRequest_c, "setDelegate:", (void *)req_set_del);
    cx_add_method(&SKProductsRequest_c, "delegate", (void *)req_del);
    cx_add_method(&SKProductsRequest_c, "start", (void *)req_start);
    cx_add_method(&SKProductsRequest_c, "cancel", (void *)req_cancel);
    cx_add_method(&SKPaymentTransaction_c, "transactionState", (void *)tx_state);
    cx_add_method(&SKPaymentTransaction_c, "error", (void *)tx_err);
    cx_add_method(&SKPaymentTransaction_c, "payment", (void *)tx_pay);
    cx_add_method(&SKPaymentTransaction_c, "transactionIdentifier", (void *)tx_nil);
    cx_add_method(&SKPaymentTransaction_c, "transactionDate", (void *)tx_nil);
    cx_add_method(&SKPaymentTransaction_c, "transactionReceipt", (void *)tx_nil);
    cx_add_method(&NSBundle_c, "appStoreReceiptURL", (void *)receipt_url);
}
