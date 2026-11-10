/*
 * mOS — StoreKit.framework (Mach-O, em /System/Library/Frameworks).
 * Mesma política do libSystem (storekit_fw.c): sem compra, sem recibo, sem rede
 * com a Apple, sem FairPlay. canMakePayments é NÃO. Produtos vazios. Pagamento
 * termina em falha (estado 2). Sem este arquivo, o loader cai no libSystem.
 * tools/build-frameworks-objc.ps1
 */
#import <Foundation/Foundation.h>

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

@interface SKProductsResponse : NSObject
@property (readonly, copy) NSArray *products;
@property (readonly, copy) NSArray *invalidProductIdentifiers;
- (void)mos_setProducts:(NSArray *)products invalid:(NSArray *)invalid;
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
@property (readonly, copy) NSString *transactionIdentifier;
@property (readonly, strong) NSDate *transactionDate;
@end

@interface SKProductsRequest : NSObject
@property (assign) id delegate;
- (instancetype)initWithProductIdentifiers:(NSSet *)productIdentifiers;
- (void)start;
- (void)cancel;
@end

@interface NSSet (MOSStoreKit)
- (NSArray *)allObjects;
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
- (NSURL *)appStoreReceiptURL;
@end

static SKPaymentQueue *g_queue;
static NSMutableArray *g_obs;
static NSMutableArray *g_txs;

static NSError *sk_err(NSString *msg) {
    return [NSError errorWithDomain:@"SKErrorDomain" code:2 userInfo:@{ @"NSLocalizedDescription": msg }];
}

@implementation SKProduct
- (NSString *)localizedTitle { return @""; }
- (NSString *)localizedDescription { return @""; }
- (NSNumber *)price { return @0; }
@end

@implementation SKProductsResponse {
    NSArray *_products;
    NSArray *_invalid;
}
- (NSArray *)products { return _products ?: @[]; }
- (NSArray *)invalidProductIdentifiers { return _invalid ?: @[]; }
- (void)mos_setProducts:(NSArray *)products invalid:(NSArray *)invalid {
    _products = products ?: @[];
    _invalid = invalid ?: @[];
}
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
- (SKPaymentTransactionState)transactionState { return _state; }
- (NSError *)error { return _error; }
- (SKPayment *)payment { return _payment; }
- (NSString *)transactionIdentifier { return nil; }
- (NSDate *)transactionDate { return nil; }
- (NSData *)transactionReceipt { return nil; }
- (void)mos_fail:(SKPayment *)payment {
    _state = SKPaymentTransactionStateFailed;
    _payment = payment;
    _error = sk_err(@"o mOS não conclui compra da App Store");
}
@end

@implementation SKProductsRequest {
    NSSet *_ids;
    BOOL _cancelled;
}
- (instancetype)initWithProductIdentifiers:(NSSet *)productIdentifiers {
    self = [super init];
    if (self) _ids = productIdentifiers;
    return self;
}
- (void)cancel { _cancelled = YES; }
- (void)start {
    if (_cancelled) return;
    SKProductsResponse *resp = [SKProductsResponse new];
    NSArray *invalid = [_ids respondsToSelector:@selector(allObjects)] ? [_ids allObjects] : @[];
    [resp mos_setProducts:@[] invalid:invalid];
    id del = self.delegate;
    if ([del respondsToSelector:@selector(productsRequest:didReceiveResponse:)])
        [del performSelector:@selector(productsRequest:didReceiveResponse:) withObject:self withObject:resp];
    if ([del respondsToSelector:@selector(requestDidFinish:)])
        [del performSelector:@selector(requestDidFinish:) withObject:self];
}
@end

@implementation SKPaymentQueue
+ (instancetype)defaultQueue {
    if (!g_queue) g_queue = [SKPaymentQueue new];
    return g_queue;
}
+ (BOOL)canMakePayments { return NO; }
- (NSArray *)transactions { if (!g_txs) g_txs = [NSMutableArray new]; return g_txs; }
- (void)addTransactionObserver:(id)observer {
    if (!observer) return;
    if (!g_obs) g_obs = [NSMutableArray new];
    if (g_obs.count < 8) [g_obs addObject:observer];
}
- (void)removeTransactionObserver:(id)observer { [g_obs removeObject:observer]; }
- (void)finishTransaction:(SKPaymentTransaction *)transaction { [g_txs removeObject:transaction]; }
- (void)addPayment:(SKPayment *)payment {
    if (!g_txs) g_txs = [NSMutableArray new];
    SKPaymentTransaction *tx = [SKPaymentTransaction new];
    [tx mos_fail:payment];
    [g_txs addObject:tx];
    for (id obs in g_obs)
        if ([obs respondsToSelector:@selector(paymentQueue:updatedTransactions:)])
            [obs performSelector:@selector(paymentQueue:updatedTransactions:) withObject:self withObject:@[ tx ]];
}
- (void)restoreCompletedTransactions {
    NSError *err = sk_err(@"o mOS não restaura compras da App Store");
    for (id obs in g_obs)
        if ([obs respondsToSelector:@selector(paymentQueue:restoreCompletedTransactionsFailedWithError:)])
            [obs performSelector:@selector(paymentQueue:restoreCompletedTransactionsFailedWithError:) withObject:self withObject:err];
}
@end

@implementation NSBundle (MOSStoreKit)
- (NSURL *)appStoreReceiptURL { return nil; }
@end
