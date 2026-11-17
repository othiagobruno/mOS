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

/* SKRequest: a base de SKProductsRequest e SKReceiptRefreshRequest, como na Apple */
@interface SKRequest : NSObject
@property (assign) id delegate;
- (void)start;
- (void)cancel;
@end
@interface SKProductsRequest : SKRequest
- (instancetype)initWithProductIdentifiers:(id)productIdentifiers;
@end
/* recibo: não há App Store no mOS, então o pedido falha como o de produtos (SKErrorCloudServiceNetworkConnectionFailed) */
@interface SKReceiptRefreshRequest : SKRequest
@property (readonly, copy) NSDictionary *receiptProperties;
- (instancetype)initWithReceiptProperties:(NSDictionary *)properties;
@end
@interface SKPaymentDiscount : NSObject
@property (readonly, copy) NSString *identifier, *keyIdentifier, *signature;
@property (readonly, copy) id nonce;
@property (readonly, copy) NSNumber *timestamp;
- (instancetype)initWithIdentifier:(NSString *)identifier keyIdentifier:(NSString *)keyIdentifier nonce:(id )nonce
                         signature:(NSString *)signature timestamp:(NSNumber *)timestamp;
@end
@interface SKProductDiscount : NSObject
@property (readonly) id price;
@property (readonly) id priceLocale;
@property (readonly) NSString *identifier;
@property (readonly) id subscriptionPeriod;
@property (readonly) NSUInteger numberOfPeriods;
@property (readonly) NSUInteger paymentMode, type;
@end
@interface SKStoreReviewController : NSObject
+ (void)requestReview;
+ (void)requestReviewInScene:(id)scene;
@end
@interface SKAdNetwork : NSObject
+ (void)registerAppForAdNetworkAttribution;
+ (void)updateConversionValue:(NSInteger)value;
+ (void)updatePostbackConversionValue:(NSInteger)value completionHandler:(void (^)(NSError *))completion;
+ (void)startImpression:(id)impression completionHandler:(void (^)(NSError *))completion;
+ (void)endImpression:(id)impression completionHandler:(void (^)(NSError *))completion;
@end
@interface SKAdImpression : NSObject
@property (copy) NSNumber *sourceAppStoreItemIdentifier, *advertisedAppStoreItemIdentifier, *adCampaignIdentifier, *sourceIdentifier;
@property (copy) NSString *adNetworkIdentifier, *adImpressionIdentifier, *adType, *adDescription, *adPurchaserName, *signature, *version;
@property (copy) NSNumber *timestamp;
@end
@interface SKDownload : NSObject
@property (readonly) NSInteger state, downloadState;
@property (readonly) float progress;
@property (readonly) id contentURL;
@property (readonly) NSError *error;
@property (readonly) NSString *contentIdentifier, *contentVersion;
@end
@interface SKOverlayAppConfiguration : NSObject
@property (copy) NSString *appIdentifier, *campaignToken, *providerToken;
@property NSInteger position;
@property BOOL userDismissible;
- (instancetype)initWithAppIdentifier:(NSString *)appIdentifier position:(NSInteger)position;
@end
@interface SKOverlay : NSObject
@property (readonly, copy) id configuration;
@property (assign) id delegate;
- (instancetype)initWithConfiguration:(id)configuration;
- (void)presentInScene:(id)scene;
+ (void)dismissOverlayInScene:(id)scene;
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
    SKRequest *self = (SKRequest *)ctx;
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

@implementation SKRequest {
    BOOL _cancelled;
}
- (void)cancel { _cancelled = YES; }
- (void)start {
    sk_ensure();
    sk_core_request((void *)self, on_req, _cancelled);
    sk_flush();
}
@end
@implementation SKProductsRequest
- (instancetype)initWithProductIdentifiers:(id)productIdentifiers {
    self = [super init];
    (void)productIdentifiers;
    return self;
}
@end
@implementation SKReceiptRefreshRequest {
    NSDictionary *_props;
}
- (instancetype)initWithReceiptProperties:(NSDictionary *)properties { self = [super init]; _props = [properties copy]; return self; }
- (NSDictionary *)receiptProperties { return _props; }
@end
@implementation SKPaymentDiscount {
    NSString *_id, *_kid, *_sig;
    id _nonce;
    NSNumber *_ts;
}
- (instancetype)initWithIdentifier:(NSString *)identifier keyIdentifier:(NSString *)keyIdentifier nonce:(id )nonce
                         signature:(NSString *)signature timestamp:(NSNumber *)timestamp {
    self = [super init];
    _id = [identifier copy]; _kid = [keyIdentifier copy]; _nonce = [nonce copy]; _sig = [signature copy]; _ts = [timestamp copy];
    return self;
}
- (NSString *)identifier { return _id; }
- (NSString *)keyIdentifier { return _kid; }
- (NSString *)signature { return _sig; }
- (id )nonce { return _nonce; }
- (NSNumber *)timestamp { return _ts; }
@end
@implementation SKProductDiscount
- (id )price { return nil; }
- (id )priceLocale { return nil; }
- (NSString *)identifier { return nil; }
- (id)subscriptionPeriod { return nil; }
- (NSUInteger)numberOfPeriods { return 0; }
- (NSUInteger)paymentMode { return 0; }
- (NSUInteger)type { return 0; }
@end
/* sem App Store no mOS: o pedido de avaliação não mostra nada (como a Apple, que pode recusar mostrar) */
@implementation SKStoreReviewController
+ (void)requestReview {}
+ (void)requestReviewInScene:(id)scene { (void)scene; }
@end
/* atribuição de anúncio: não há postbacks no mOS; as chamadas terminam sem erro e sem efeito */
@implementation SKAdNetwork
+ (void)registerAppForAdNetworkAttribution {}
+ (void)updateConversionValue:(NSInteger)value { (void)value; }
+ (void)updatePostbackConversionValue:(NSInteger)value completionHandler:(void (^)(NSError *))completion { (void)value; if (completion) completion(nil); }
+ (void)startImpression:(id)impression completionHandler:(void (^)(NSError *))completion { (void)impression; if (completion) completion(nil); }
+ (void)endImpression:(id)impression completionHandler:(void (^)(NSError *))completion { (void)impression; if (completion) completion(nil); }
@end
@implementation SKAdImpression
@end
@implementation SKDownload
- (NSInteger)state { return 0; }
- (NSInteger)downloadState { return 0; }
- (float)progress { return 0; }
- (id )contentURL { return nil; }
- (NSError *)error { return nil; }
- (NSString *)contentIdentifier { return nil; }
- (NSString *)contentVersion { return nil; }
@end
@implementation SKOverlayAppConfiguration
- (instancetype)initWithAppIdentifier:(NSString *)appIdentifier position:(NSInteger)position {
    self = [super init];
    _appIdentifier = [appIdentifier copy]; _position = position; _userDismissible = YES;
    return self;
}
@end
/* o painel da App Store (iOS) não existe no Mac nem no mOS: apresentar não faz nada */
@implementation SKOverlay {
    id _conf;
}
- (instancetype)initWithConfiguration:(id)configuration { self = [super init]; _conf = configuration; return self; }
- (id)configuration { return _conf; }
- (void)presentInScene:(id)scene { (void)scene; }
+ (void)dismissOverlayInScene:(id)scene { (void)scene; }
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
