// Farol — app de exemplo do CoreBluetooth no papel periférico: anuncia "Farol mOS" com o serviço de bateria (180F),
// responde às leituras do nível (2A19) pelo delegate e notifica quem assinar.
#import <Cocoa/Cocoa.h>
#include <stdio.h>

@class CBUUID, CBCentral;
@interface CBUUID : NSObject
+ (CBUUID *)UUIDWithString:(NSString *)s;
@property (readonly) NSString *UUIDString;
@end
@interface CBCharacteristic : NSObject
@property (readonly) CBUUID *UUID;
@end
@interface CBMutableCharacteristic : CBCharacteristic
- (instancetype)initWithType:(CBUUID *)t properties:(NSUInteger)p value:(NSData *)v permissions:(NSUInteger)perm;
@end
@interface CBMutableService : NSObject
- (instancetype)initWithType:(CBUUID *)t primary:(BOOL)p;
@property (strong) NSArray *characteristics;
@end
@interface CBATTRequest : NSObject
@property (readonly) CBCharacteristic *characteristic;
@property (strong) NSData *value;
@property (readonly) NSInteger offset;
@end
@interface CBPeripheralManager : NSObject
- (instancetype)initWithDelegate:(id)d queue:(id)q;
@property (readonly) NSInteger state;
- (void)addService:(CBMutableService *)s;
- (void)startAdvertising:(NSDictionary *)d;
- (void)respondToRequest:(CBATTRequest *)r withResult:(NSInteger)res;
- (BOOL)updateValue:(NSData *)v forCharacteristic:(CBMutableCharacteristic *)c onSubscribedCentrals:(NSArray *)cs;
@end
extern NSString *const CBAdvertisementDataLocalNameKey, *const CBAdvertisementDataServiceUUIDsKey;

@interface Farol : NSObject
@property (strong) CBPeripheralManager *pm;
@property (strong) CBMutableCharacteristic *nivel;
@end
@implementation Farol
- (void)peripheralManagerDidUpdateState:(CBPeripheralManager *)p {
    printf("[app] estado: %ld\n", (long)p.state);
    if (p.state != 5 || self.nivel) return;
    self.nivel = [[CBMutableCharacteristic alloc] initWithType:[CBUUID UUIDWithString:@"2A19"] properties:0x12 value:nil permissions:1];
    CBMutableService *s = [[CBMutableService alloc] initWithType:[CBUUID UUIDWithString:@"180F"] primary:YES];
    s.characteristics = @[self.nivel];
    [p addService:s];
}
- (void)peripheralManager:(CBPeripheralManager *)p didAddService:(id)s error:(NSError *)e {
    printf("[app] serviço adicionado\n");
    [p startAdvertising:@{CBAdvertisementDataLocalNameKey: @"Farol mOS", CBAdvertisementDataServiceUUIDsKey: @[[CBUUID UUIDWithString:@"180F"]]}];
}
- (void)peripheralManagerDidStartAdvertising:(CBPeripheralManager *)p error:(NSError *)e { printf("[app] anunciando%s\n", e ? " (erro)" : ""); }
- (void)peripheralManager:(CBPeripheralManager *)p didReceiveReadRequest:(CBATTRequest *)r {
    unsigned char v = 77;
    printf("[app] leitura de %s\n", r.characteristic.UUID.UUIDString.UTF8String);
    r.value = [NSData dataWithBytes:&v length:1];
    [p respondToRequest:r withResult:0];
}
- (void)peripheralManager:(CBPeripheralManager *)p central:(CBCentral *)c didSubscribeToCharacteristic:(CBCharacteristic *)ch {
    unsigned char v = 78;
    printf("[app] central assinou %s\n", ch.UUID.UUIDString.UTF8String);
    BOOL ok = [p updateValue:[NSData dataWithBytes:&v length:1] forCharacteristic:self.nivel onSubscribedCentrals:nil];
    printf("[app] notificação enviada: %s\n", ok ? "sim" : "não");
}
@end

@interface AppDelegate : NSObject <NSApplicationDelegate>
@property (strong) Farol *farol;
@end
@implementation AppDelegate
- (void)applicationDidFinishLaunching:(NSNotification *)n {
    self.farol = [Farol new];
    self.farol.pm = [[CBPeripheralManager alloc] initWithDelegate:self.farol queue:nil];
}
@end

int main(int argc, const char **argv) {
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        AppDelegate *d = [AppDelegate new];
        app.delegate = d;
        [app run];
    }
    return 0;
}
