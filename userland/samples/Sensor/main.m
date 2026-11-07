// Sensor — app de exemplo do CoreBluetooth: procura periféricos BLE, conecta no primeiro que aparecer, lista
// serviços e características, lê o fabricante e o nível de bateria e assina as notificações da bateria.
#import <Cocoa/Cocoa.h>
#include <stdio.h>

// ---- CoreBluetooth (o SDK enxuto do mOS não traz estes cabeçalhos)
@class CBCentralManager, CBPeripheral, CBService, CBCharacteristic, CBUUID, NSUUID;
@interface NSString (Dados)
- (instancetype)initWithData:(NSData *)d encoding:(NSUInteger)e;
@end
@interface CBUUID : NSObject
+ (CBUUID *)UUIDWithString:(NSString *)s;
@property (readonly) NSString *UUIDString;
@end
@interface CBService : NSObject
@property (readonly) CBUUID *UUID;
@property (readonly) NSArray<CBCharacteristic *> *characteristics;
@end
@interface CBCharacteristic : NSObject
@property (readonly) CBUUID *UUID;
@property (readonly) NSUInteger properties;
@property (readonly) NSData *value;
@property (readonly) BOOL isNotifying;
@end
@interface CBPeripheral : NSObject
@property (weak) id delegate;
@property (readonly) NSString *name;
@property (readonly) NSUUID *identifier;
@property (readonly) NSArray<CBService *> *services;
- (void)discoverServices:(NSArray *)u;
- (void)discoverCharacteristics:(NSArray *)u forService:(CBService *)s;
- (void)readValueForCharacteristic:(CBCharacteristic *)c;
- (void)setNotifyValue:(BOOL)on forCharacteristic:(CBCharacteristic *)c;
@end
@interface CBCentralManager : NSObject
- (instancetype)initWithDelegate:(id)d queue:(id)q;
@property (readonly) NSInteger state;
- (void)scanForPeripheralsWithServices:(NSArray *)s options:(NSDictionary *)o;
- (void)stopScan;
- (void)connectPeripheral:(CBPeripheral *)p options:(NSDictionary *)o;
- (void)cancelPeripheralConnection:(CBPeripheral *)p;
@end
extern NSString *const CBAdvertisementDataLocalNameKey;
@interface NSUUID : NSObject
@property (readonly) NSString *UUIDString;
@end

@interface Central : NSObject
@property (strong) CBCentralManager *cm;
@property (strong) CBPeripheral *per;
@property int pending;
@end
@implementation Central
- (void)centralManagerDidUpdateState:(CBCentralManager *)c {
    printf("[app] estado do Bluetooth: %ld\n", (long)c.state);
    if (c.state == 5) [c scanForPeripheralsWithServices:nil options:nil];
}
- (void)centralManager:(CBCentralManager *)c didDiscoverPeripheral:(CBPeripheral *)p advertisementData:(NSDictionary *)adv RSSI:(NSNumber *)rssi {
    printf("[app] achou \"%s\" (%s) %d dBm\n", [adv[CBAdvertisementDataLocalNameKey] UTF8String], p.identifier.UUIDString.UTF8String, rssi.intValue);
    if (self.per) return;
    self.per = p;
    [c stopScan];
    [c connectPeripheral:p options:nil];
}
- (void)centralManager:(CBCentralManager *)c didConnectPeripheral:(CBPeripheral *)p {
    printf("[app] conectado a %s\n", p.name.UTF8String);
    p.delegate = self;
    [p discoverServices:nil];
}
- (void)centralManager:(CBCentralManager *)c didFailToConnectPeripheral:(CBPeripheral *)p error:(NSError *)e {
    printf("[app] falhou: %s\n", e.localizedDescription.UTF8String);
}
- (void)centralManager:(CBCentralManager *)c didDisconnectPeripheral:(CBPeripheral *)p error:(NSError *)e {
    printf("[app] desconectado\n");
    [NSApp terminate:nil];
}
- (void)peripheral:(CBPeripheral *)p didDiscoverServices:(NSError *)e {
    for (CBService *s in p.services) {
        printf("[app] serviço %s\n", s.UUID.UUIDString.UTF8String);
        [p discoverCharacteristics:nil forService:s];
    }
}
- (void)peripheral:(CBPeripheral *)p didDiscoverCharacteristicsForService:(CBService *)s error:(NSError *)e {
    for (CBCharacteristic *c in s.characteristics) {
        printf("[app]   característica %s (propriedades 0x%02lx)\n", c.UUID.UUIDString.UTF8String, (unsigned long)c.properties);
        if (c.properties & 0x02) [p readValueForCharacteristic:c];
        if (c.properties & 0x10) [p setNotifyValue:YES forCharacteristic:c];
    }
}
- (void)peripheral:(CBPeripheral *)p didUpdateValueForCharacteristic:(CBCharacteristic *)c error:(NSError *)e {
    if (e) { printf("[app] erro lendo %s: %s\n", c.UUID.UUIDString.UTF8String, e.localizedDescription.UTF8String); return; }
    NSData *v = c.value;
    if ([c.UUID.UUIDString isEqualToString:@"2A19"]) printf("[app] bateria: %d%%\n", ((const unsigned char *)v.bytes)[0]);
    else {
        NSString *t = [[NSString alloc] initWithData:v encoding:NSUTF8StringEncoding];
        printf("[app] %s = \"%s\"\n", c.UUID.UUIDString.UTF8String, t.UTF8String);
    }
    if (++self.pending == 3) [self.cm cancelPeripheralConnection:p];   /* fabricante + bateria + notificação */
}
- (void)peripheral:(CBPeripheral *)p didUpdateNotificationStateForCharacteristic:(CBCharacteristic *)c error:(NSError *)e {
    printf("[app] notificações de %s: %s\n", c.UUID.UUIDString.UTF8String, c.isNotifying ? "ligadas" : "desligadas");
}
@end

@interface AppDelegate : NSObject <NSApplicationDelegate>
@property (strong) Central *central;
@end
@implementation AppDelegate
- (void)applicationDidFinishLaunching:(NSNotification *)n {
    self.central = [Central new];
    self.central.cm = [[CBCentralManager alloc] initWithDelegate:self.central queue:nil];
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
