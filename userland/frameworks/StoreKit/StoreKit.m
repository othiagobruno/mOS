/*
 * mOS — StoreKit.framework (Mach-O, em /System/Library/Frameworks).
 * Mesma fila de userland/libsystem/storekit_core.inc. Sem compra, sem recibo,
 * sem rede com a Apple, sem FairPlay. canMakePayments é SIM (não há controle
 * parental). Produto e compra falham com SKErrorCloudServiceNetworkConnectionFailed.
 * addPayment avisa .purchasing e depois .failed, na thread da fila (a main).
 * tools/build-frameworks-objc.ps1
 */
#import <Foundation/Foundation.h>
#include "../../libsystem/storekit_core.inc"

extern int pthread_main_np(void);
extern void *dispatch_get_main_queue(void);
extern void dispatch_async_f(void *q, void *ctx, void (*fn)(void *));

@interface NSError (MOSStoreKit)
+ (instancetype)errorWithDomain:(NSString *)domain code:(NSInteger)code userInfo:(NSDictionary *)userInfo;
@end

typedef NS_ENUM(NSInteger, SKPaymentTransactionState) {
    SKPaymentTransactionStatePurchasing = 0,
    SKPaymentTransactionStatePurchased = 1,
    SKPaymentTransactionStateFailed = 2,
    SKPaymentTransactionStateRestored = 3,
    SKPaymentTransactionStateDeferred = 4,
};

@interface SKProduct : NSObject
@property (readonly, copy) NSString *productIdentifier;
@property (readonly, copy) NSString *localizedTitle;
@property (readonly, copy) NSString *localizedDescription;
@property (readonly, strong) NSNumber *price;
@end

@interface SKPayment : NSObject
@property (copy) NSString *productIdentifier;
@property NSUInteger quantity;
@property (copy) NSString *applicationUsername;
+ (instancetype)paymentWithProduct:(SKProduct *)product;
+ (instancetype)paymentWithProductIdentifier:(NSString *)identifier;
@end

@interface SKMutablePayment : SKPayment
@end

@interface SKPaymentTransaction : NSObject
@property (readonly) SKPaymentTransactionState transactionState;
@property (readonly, strong) NSError *error;
@property (readonly, strong) SKPayment *payment;
- (void)mos_setState:(SKPaymentTransactionState)state error:(NSError *)error payment:(SKPayment *)payment;
@end

@interface SKProductsRequest : NSObject
@property (assign) id delegate;
- (instancetype)initWithProductIdentifiers:(id)productIdentifiers;
- (void)start;
- (void)cancel;
@end

@interface SKPaymentQueue : NSObject
+ (instancetype)defaultQueue;
+ (BOOL)canMakePayments;
- (void)addPayment:(SKPayment *)payment;
- (void)addTransactionObserver:(id)observer;
- (void)removeTransactionObserver:(id)observer;
- (void)finishTransaction:(SKPaymentTransaction *)transaction;
- (void)restoreCompletedTransactions;
@property (readonly, copy) NSArray *transactions;
@end

@interface NSBundle (MOSStoreKit)
- (id)appStoreReceiptURL;
@end

static void bind_tx(int tx);
static SKPaymentQueue *g_queue;
static SKPayment *g_pay_for_bind;
static SKPaymentTransaction *g_tx_obj[SK_TX_MAX];
static int g_tx_id[SK_TX_MAX];

static NSError *sk_nserr(NSInteger code) {
    return [NSError errorWithDomain:@"SKErrorDomain" code:code userInfo:@{
        @"NSLocalizedDescription": @"não foi possível contatar a App Store (SKErrorCloudServiceNetworkConnectionFailed)"
    }];
}

static void tx_bind_obj(int tx, SKPaymentTransaction *obj) {
    for (int i = 0; i < SK_TX_MAX; i++) if (!g_tx_obj[i]) {
        g_tx_id[i] = tx;
        g_tx_obj[i] = obj;
        return;
    }
}

static SKPaymentTransaction *tx_obj(int tx) {
    for (int i = 0; i < SK_TX_MAX; i++) if (g_tx_id[i] == tx && g_tx_obj[i]) return g_tx_obj[i];
    return nil;
}

static void tx_unbind(int tx) {
    for (int i = 0; i < SK_TX_MAX; i++) if (g_tx_id[i] == tx) {
        g_tx_id[i] = 0;
        g_tx_obj[i] = nil;
    }
}

static void drain_main(void *unused) {
    (void)unused;
    sk_core_drain();
}

static void sk_ensure(void) {
    static int once;
    if (once) return;
    once = 1;
    sk_core_set_tid(pthread_main_np);
    sk_core_bind_queue(1);
    sk_core_set_bind(bind_tx);
}

static void sk_flush(void) {
    if (!sk_core_pending()) return;
    if (pthread_main_np()) sk_core_drain();
    else dispatch_async_f(dispatch_get_main_queue(), 0, drain_main);
}

static void on_tx(void *ctx, int tx, int state, int err) {
    id obs = (id)ctx;
    SKPaymentTransaction *obj = tx_obj(tx);
    if (!obj) return;
    NSError *e = err ? sk_nserr(err) : nil;
    [obj mos_setState:(SKPaymentTransactionState)state error:e payment:obj.payment];
    if ([obs respondsToSelector:@selector(paymentQueue:updatedTransactions:)])
        [obs performSelector:@selector(paymentQueue:updatedTransactions:) withObject:g_queue withObject:@[ obj ]];
}

static void on_restore(void *ctx, int err) {
    id obs = (id)ctx;
    if ([obs respondsToSelector:@selector(paymentQueue:restoreCompletedTransactionsFailedWithError:)])
        [obs performSelector:@selector(paymentQueue:restoreCompletedTransactionsFailedWithError:) withObject:g_queue withObject:sk_nserr(err)];
}

static void bind_tx(int tx) {
    SKPaymentTransaction *obj = [SKPaymentTransaction new];
    [obj mos_setState:SKPaymentTransactionStatePurchasing error:nil payment:g_pay_for_bind];
    tx_bind_obj(tx, obj);
}

static void on_req(void *ctx, int err) {
    SKProductsRequest *self = (SKProductsRequest *)ctx;
    id del = self.delegate;
    if ([del respondsToSelector:@selector(request:didFailWithError:)])
        [del performSelector:@selector(request:didFailWithError:) withObject:self withObject:sk_nserr(err)];
}

@implementation SKProduct
- (NSString *)localizedTitle { return @""; }
- (NSString *)localizedDescription { return @""; }
- (NSNumber *)price { return @0; }
@end

@implementation SKPayment
+ (instancetype)paymentWithProductIdentifier:(NSString *)identifier {
    SKPayment *p = [SKPayment new];
    p.productIdentifier = identifier;
    p.quantity = 1;
    return p;
}
+ (instancetype)paymentWithProduct:(SKProduct *)product {
    return [self paymentWithProductIdentifier:product.productIdentifier];
}
@end

@implementation SKMutablePayment
@end

@implementation SKPaymentTransaction {
    SKPaymentTransactionState _state;
    NSError *_error;
    SKPayment *_payment;
}
- (SKPaymentTransactionState)transactionState {
    if (_state == SKPaymentTransactionStatePurchased || _state == SKPaymentTransactionStateRestored)
        return SKPaymentTransactionStateFailed;
    return _state;
}
- (NSError *)error { return _error; }
- (SKPayment *)payment { return _payment; }
- (NSString *)transactionIdentifier { return nil; }
- (NSDate *)transactionDate { return nil; }
- (NSData *)transactionReceipt { return nil; }
- (void)mos_setState:(SKPaymentTransactionState)state error:(NSError *)error payment:(SKPayment *)payment {
    _state = state;
    _error = error;
    if (payment) _payment = payment;
}
@end

@implementation SKProductsRequest {
    BOOL _cancelled;
}
- (instancetype)initWithProductIdentifiers:(id)productIdentifiers {
    self = [super init];
    (void)productIdentifiers;
    return self;
}
- (void)cancel { _cancelled = YES; }
- (void)start {
    sk_ensure();
    sk_core_request((void *)self, on_req, _cancelled);
    sk_flush();
}
@end

@implementation SKPaymentQueue
+ (instancetype)defaultQueue {
    if (!g_queue) g_queue = [SKPaymentQueue new];
    sk_ensure();
    return g_queue;
}
+ (BOOL)canMakePayments { return sk_core_can_pay() ? YES : NO; }
- (NSArray *)transactions {
    NSMutableArray *a = [NSMutableArray new];
    for (int i = 0; i < SK_TX_MAX; i++) if (g_tx_obj[i]) [a addObject:g_tx_obj[i]];
    return a;
}
- (void)addTransactionObserver:(id)observer {
    sk_ensure();
    if (!observer) return;
    sk_core_add_observer((void *)observer, on_tx, on_restore);
    sk_flush();
}
- (void)removeTransactionObserver:(id)observer {
    sk_core_remove_observer((void *)observer);
}
- (void)finishTransaction:(SKPaymentTransaction *)transaction {
    for (int i = 0; i < SK_TX_MAX; i++) if (g_tx_obj[i] == transaction) {
        int idn = g_tx_id[i];
        sk_core_finish(idn);
        if (!sk_core_tx_get(idn, 0, 0, 0, 0)) tx_unbind(idn);
        return;
    }
}
- (void)addPayment:(SKPayment *)payment {
    const char *pid = payment.productIdentifier.UTF8String;
    sk_ensure();
    g_pay_for_bind = payment;
    sk_core_add_payment(pid, (int)payment.quantity);
    g_pay_for_bind = nil;
    sk_flush();
}
- (void)restoreCompletedTransactions {
    sk_ensure();
    sk_core_restore();
    sk_flush();
}
@end

@implementation NSBundle (MOSStoreKit)
- (id)appStoreReceiptURL { return nil; }
@end
