/* StoreKit no libSystem.
 *
 * Estende o stub de SKPaymentQueue / SKProductsRequest / SKMutablePayment
 * (as classes já existem em vscode_fwclasses.c). Não há compra, recibo, rede
 * com a Apple, FairPlay nem assinatura. canMakePayments é não. Um pedido
 * devolve zero produtos e a lista pedida como inválida, e termina na hora.
 * addPayment: entrega uma transação falha (estado 2) e volta. O estado
 * comprado (1) e o restaurado (3) não são produzidos.
 */
#include "foundation.h"
#include "libc.h"
#include "storekit_stub.inc"

void cx_mark_stub_class(cx_class_t *c);
id objc_autorelease(id o);
id objc_retain(id o);

extern cx_class_t SKPaymentQueue_c __asm__("OBJC_CLASS_$_SKPaymentQueue");
extern cx_class_t SKProductsRequest_c __asm__("OBJC_CLASS_$_SKProductsRequest");
extern cx_class_t SKMutablePayment_c __asm__("OBJC_CLASS_$_SKMutablePayment");
extern cx_class_t NSBundle_c __asm__("OBJC_CLASS_$_NSBundle");

#define SKC(N) cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N); static cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N);
SKC(SKProduct)
SKC(SKProductsResponse)
SKC(SKPaymentTransaction)

typedef struct {
    id key, ids, delegate, payment, error, username;
    int cancelled, state, qty;
    char pid[160];
} sk_slot;

static sk_slot slots[48];
static id g_queue, g_txs, g_obs[8];
static int nobs;

static int responds(id o, const char *sel) {
    return o && cx_responds(o->isa, sel_registerName(sel));
}
static sk_slot *slot_of(id o) {
    for (int i = 0; i < 48; i++) if (slots[i].key == o) return &slots[i];
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
        slots[i].state = sk_policy_fail_state();
        return &slots[i];
    }
    return 0;
}
static id txs(void) {
    if (!g_txs) g_txs = cx_arr_new(cx_class_named("NSMutableArray"));
    return g_txs;
}
static id sk_err(const char *msg) {
    id ui = MSG(id, (id)cx_class_named("NSDictionary"), "dictionaryWithObject:forKey:", cx_str(msg), cx_str("NSLocalizedDescription"));
    return MSG(id, (id)cx_class_named("NSError"), "errorWithDomain:code:userInfo:", cx_str("SKErrorDomain"), (long)2, ui);
}
static long safe_state(long st) {
    /* 1 = purchased, 3 = restored. A política não liga compra; se ligasse, ainda cairia aqui. */
    if (st == 1 || st == 3 || sk_policy_purchased()) return sk_policy_fail_state();
    return st;
}

static id sk_default(id cls, SEL c) {
    (void)c;
    if (!g_queue) g_queue = cx_alloc_instance((cx_class_t *)cls);
    return g_queue;
}
static BOOL sk_can(id self, SEL c) { (void)self; (void)c; return sk_policy_can_pay() ? YES : NO; }
static void sk_add_obs(id self, SEL c, id obs) {
    (void)self; (void)c;
    if (!obs || nobs >= 8) return;
    g_obs[nobs++] = objc_retain(obs);
}
static void sk_rm_obs(id self, SEL c, id obs) {
    int i;
    (void)self; (void)c;
    for (i = 0; i < nobs; i++) if (g_obs[i] == obs) {
        g_obs[i] = g_obs[--nobs];
        g_obs[nobs] = 0;
        return;
    }
}
static id sk_transactions(id self, SEL c) { (void)self; (void)c; return txs(); }
static id make_tx(id payment) {
    id tx = cx_alloc_instance(&SKPaymentTransaction_c);
    sk_slot *s = slot_make(tx);
    if (s) {
        s->state = safe_state(sk_policy_fail_state());
        s->payment = payment ? objc_retain(payment) : 0;
        s->error = objc_retain(sk_err("o mOS não conclui compra da App Store"));
        s->pid[0] = 0;
    }
    cx_arr_add(txs(), tx);
    return tx;
}
static void tell_updated(id queue, id tx) {
    id one = cx_arr_new(cx_class_named("NSMutableArray"));
    int i;
    cx_arr_add(one, tx);
    for (i = 0; i < nobs; i++)
        if (responds(g_obs[i], "paymentQueue:updatedTransactions:"))
            MSG(void, g_obs[i], "paymentQueue:updatedTransactions:", queue, one);
}
static void sk_add(id self, SEL c, id payment) {
    (void)c;
    tell_updated(self, make_tx(payment));
}
static void sk_finish(id self, SEL c, id tx) {
    (void)self; (void)c;
    if (txs() && tx) MSG(void, txs(), "removeObject:", tx);
}
static void sk_restore(id self, SEL c) {
    id err = sk_err("o mOS não restaura compras da App Store");
    int i;
    (void)c;
    for (i = 0; i < nobs; i++)
        if (responds(g_obs[i], "paymentQueue:restoreCompletedTransactionsFailedWithError:"))
            MSG(void, g_obs[i], "paymentQueue:restoreCompletedTransactionsFailedWithError:", self, err);
}

static id pay_with_id(id cls, SEL c, id pid) {
    id p = cx_alloc_instance((cx_class_t *)cls);
    sk_slot *s = slot_make(p);
    (void)c;
    if (s && pid) snprintf(s->pid, sizeof s->pid, "%s", cx_cstr(pid));
    return objc_autorelease(p);
}
static id pay_with_product(id cls, SEL c, id product) {
    id pid = product && responds(product, "productIdentifier") ? MSG0(id, product, "productIdentifier") : 0;
    return pay_with_id(cls, c, pid);
}
static id pay_pid(id self, SEL c) { (void)c; sk_slot *s = slot_of(self); return s && s->pid[0] ? objc_autorelease(cx_str(s->pid)) : 0; }
static void pay_set_pid(id self, SEL c, id pid) {
    sk_slot *s = slot_make(self);
    (void)c;
    if (s) snprintf(s->pid, sizeof s->pid, "%s", pid ? cx_cstr(pid) : "");
}
static unsigned long pay_qty(id self, SEL c) { (void)c; sk_slot *s = slot_of(self); return s ? (unsigned long)s->qty : 1; }
static void pay_set_qty(id self, SEL c, unsigned long q) { (void)c; sk_slot *s = slot_make(self); if (s) s->qty = q ? (int)q : 1; }
static id pay_user(id self, SEL c) { (void)c; sk_slot *s = slot_of(self); return s ? s->username : 0; }
static void pay_set_user(id self, SEL c, id u) { (void)c; sk_slot *s = slot_make(self); if (s) s->username = u ? objc_retain(u) : 0; }

static id prod_pid(id self, SEL c) { return pay_pid(self, c); }
static id prod_title(id self, SEL c) { (void)self; (void)c; return objc_autorelease(cx_str("")); }
static id prod_price(id self, SEL c) { (void)self; (void)c; return objc_autorelease(cx_num_d(0)); }

static id resp_init(id self, SEL c) { (void)c; slot_make(self); return self; }
static id resp_products(id self, SEL c) {
    sk_slot *s = slot_of(self);
    (void)c;
    return s && s->ids ? s->ids : objc_autorelease(cx_arr_new(cx_class_named("NSMutableArray")));
}
static id resp_invalid(id self, SEL c) {
    sk_slot *s = slot_of(self);
    (void)c;
    return s && s->delegate ? s->delegate : objc_autorelease(cx_arr_new(cx_class_named("NSMutableArray")));
}

static id req_init(id self, SEL c, id ids) {
    sk_slot *s = slot_make(self);
    (void)c;
    if (s && ids) s->ids = objc_retain(ids);
    return self;
}
static void req_set_del(id self, SEL c, id del) { (void)c; sk_slot *s = slot_make(self); if (s) s->delegate = del; }
static id req_del(id self, SEL c) { (void)c; sk_slot *s = slot_of(self); return s ? s->delegate : 0; }
static void req_cancel(id self, SEL c) { (void)c; sk_slot *s = slot_make(self); if (s) s->cancelled = 1; }
static id invalid_ids(id ids) {
    if (ids && responds(ids, "allObjects")) return MSG0(id, ids, "allObjects");
    if (ids && responds(ids, "count")) return ids;
    return cx_arr_new(cx_class_named("NSMutableArray"));
}
static void req_start(id self, SEL c) {
    sk_slot *s = slot_of(self);
    id resp, products, invalid;
    int i;
    (void)c;
    if (!s || s->cancelled) return;
    products = cx_arr_new(cx_class_named("NSMutableArray"));
    for (i = 0; i < sk_policy_product_count(); i++) (void)products;
    invalid = invalid_ids(s->ids);
    resp = cx_alloc_instance(&SKProductsResponse_c);
    slot_make(resp);
    { sk_slot *r = slot_of(resp); if (r) { r->ids = objc_retain(products); r->delegate = objc_retain(invalid); } }
    if (s->delegate && responds(s->delegate, "productsRequest:didReceiveResponse:"))
        MSG(void, s->delegate, "productsRequest:didReceiveResponse:", self, resp);
    if (s->delegate && responds(s->delegate, "requestDidFinish:"))
        MSG(void, s->delegate, "requestDidFinish:", self);
}

static long tx_state(id self, SEL c) {
    sk_slot *s = slot_of(self);
    (void)c;
    return safe_state(s ? s->state : sk_policy_fail_state());
}
static id tx_err(id self, SEL c) { (void)c; sk_slot *s = slot_of(self); return s ? s->error : 0; }
static id tx_pay(id self, SEL c) { (void)c; sk_slot *s = slot_of(self); return s ? s->payment : 0; }
static id tx_nil(id self, SEL c) { (void)self; (void)c; (void)sk_policy_receipt_len; return 0; }

static id receipt_url(id self, SEL c) { (void)self; (void)c; return 0; }

int cx_storekit_verify_receipt(const void *bytes, size_t n, char *err, size_t cap) {
    return sk_verify(bytes, n, err, cap);
}

static void sk_def(cx_class_t *c, cx_class_t *m, const char *name) {
    cx_define_class(c, m, name, cx_class_named("NSObject"), 0);
    cx_mark_stub_class(c);
}

void cx_storekit_init(void) {
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
    cx_add_method(&SKProductsResponse_c, "init", (void *)resp_init);
    cx_add_method(&SKProductsResponse_c, "products", (void *)resp_products);
    cx_add_method(&SKProductsResponse_c, "invalidProductIdentifiers", (void *)resp_invalid);
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
