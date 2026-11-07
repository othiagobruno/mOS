// Jogo — app de exemplo do GameController e do CoreHaptics: mostra o controle conectado (Xbox, DualShock,
// DualSense ou HID genérico), os botões e analógicos ao vivo, vibra ao apertar A e toca um padrão AHAP.
// O teclado (GCKeyboard) move o quadrado com WASD; o controle, com o analógico esquerdo.
#import <Cocoa/Cocoa.h>
#include <stdio.h>

// ---- GameController / CoreHaptics (o SDK enxuto do mOS não traz estes cabeçalhos)
@class GCController, GCExtendedGamepad, GCControllerButtonInput, GCControllerDirectionPad, GCControllerElement, CHHapticEngine;
typedef void (^GCButtonHandler)(GCControllerButtonInput *b, float value, BOOL pressed);
typedef void (^GCDpadHandler)(GCControllerDirectionPad *d, float x, float y);
@interface GCControllerElement : NSObject
@property (readonly) NSString *localizedName;
@end
@interface GCControllerButtonInput : GCControllerElement
@property (readonly) float value;
@property (readonly, getter=isPressed) BOOL pressed;
@property (copy) GCButtonHandler pressedChangedHandler;
@end
@interface GCControllerAxisInput : GCControllerElement
@property (readonly) float value;
@end
@interface GCControllerDirectionPad : GCControllerElement
@property (readonly) GCControllerAxisInput *xAxis, *yAxis;
@property (copy) GCDpadHandler valueChangedHandler;
@end
@interface GCExtendedGamepad : NSObject
@property (readonly) GCControllerButtonInput *buttonA, *buttonB, *leftTrigger, *rightTrigger;
@property (readonly) GCControllerDirectionPad *leftThumbstick, *dpad;
@property (copy) void (^valueChangedHandler)(GCExtendedGamepad *g, GCControllerElement *e);
@end
@interface GCDeviceHaptics : NSObject
- (CHHapticEngine *)createEngineWithLocality:(NSString *)l;
@end
@interface GCController : NSObject
+ (NSArray<GCController *> *)controllers;
@property (readonly) NSString *vendorName, *productCategory;
@property (readonly) GCExtendedGamepad *extendedGamepad;
@property (readonly) GCDeviceHaptics *haptics;
@end
@interface GCKeyboardInput : NSObject
@property (copy) void (^keyChangedHandler)(GCKeyboardInput *k, GCControllerButtonInput *key, NSInteger code, BOOL pressed);
@end
@interface GCKeyboard : NSObject
+ (GCKeyboard *)coalescedKeyboard;
@property (readonly) GCKeyboardInput *keyboardInput;
@end
extern NSString *const GCControllerDidConnectNotification, *const GCControllerDidDisconnectNotification, *const GCHapticsLocalityDefault;
@protocol CHHapticPatternPlayer
- (BOOL)startAtTime:(double)t error:(NSError **)e;
@end
@interface CHHapticDeviceCapabilityMOS : NSObject
@property (readonly) BOOL supportsHaptics, supportsAudio;
@end
@interface CHHapticPattern : NSObject
- (instancetype)initWithDictionary:(NSDictionary *)d error:(NSError **)e;
@property (readonly) double duration;
@end
@interface NSNotificationCenter (Blocos)
- (id)addObserverForName:(NSString *)n object:(id)o queue:(id)q usingBlock:(void (^)(NSNotification *))b;
@end
@interface CHHapticEngine : NSObject
+ (CHHapticDeviceCapabilityMOS *)capabilitiesForHardware;
- (instancetype)initAndReturnError:(NSError **)e;
- (BOOL)startAndReturnError:(NSError **)e;
- (id<CHHapticPatternPlayer>)createPlayerWithPattern:(CHHapticPattern *)p error:(NSError **)e;
@end

static NSDictionary *ahap(BOOL audio) {
    NSMutableArray *list = [NSMutableArray array];
    [list addObject:@{@"Event": @{@"Time": @0.0, @"EventType": @"HapticTransient",
                                  @"EventParameters": @[@{@"ParameterID": @"HapticIntensity", @"ParameterValue": @1.0},
                                                        @{@"ParameterID": @"HapticSharpness", @"ParameterValue": @0.8}]}}];
    [list addObject:@{@"Event": @{@"Time": @0.1, @"EventType": @"HapticContinuous", @"EventDuration": @0.3,
                                  @"EventParameters": @[@{@"ParameterID": @"HapticIntensity", @"ParameterValue": @0.6}]}}];
    if (audio)
        [list addObject:@{@"Event": @{@"Time": @0.0, @"EventType": @"AudioContinuous", @"EventDuration": @0.25,
                                      @"EventParameters": @[@{@"ParameterID": @"AudioVolume", @"ParameterValue": @0.5}]}}];
    return @{@"Version": @1.0, @"Pattern": list};
}

@interface Palco : NSView
@property NSPoint pos;
@property (copy) NSString *status;
@end
@implementation Palco
- (BOOL)isFlipped { return YES; }
- (void)drawRect:(NSRect)r {
    [[NSColor whiteColor] set];
    NSRectFill(self.bounds);
    [[NSColor systemBlueColor] set];
    NSRectFill(NSMakeRect(self.pos.x, self.pos.y, 30, 30));
    [self.status ?: @"Conecte um controle" drawAtPoint:NSMakePoint(12, 12) withAttributes:@{NSFontAttributeName: [NSFont systemFontOfSize:14]}];
}
@end

@interface AppDelegate : NSObject <NSApplicationDelegate>
@property (strong) NSWindow *window;
@property (strong) Palco *palco;
@property (strong) CHHapticEngine *padEngine, *macEngine;
@end
@implementation AppDelegate
- (void)ligar:(GCController *)c {
    printf("[app] controle: %s (%s)\n", c.vendorName.UTF8String, c.productCategory.UTF8String);
    self.palco.status = [NSString stringWithFormat:@"%@ — A vibra, analógico move", c.vendorName];
    self.palco.needsDisplay = YES;
    __weak AppDelegate *me = self;
    GCExtendedGamepad *g = c.extendedGamepad;
    g.buttonA.pressedChangedHandler = ^(GCControllerButtonInput *b, float v, BOOL pressed) {
        printf("[app] A %s\n", pressed ? "apertado" : "solto");
        if (pressed && me.padEngine) {
            NSError *err = nil;
            CHHapticPattern *p = [[CHHapticPattern alloc] initWithDictionary:ahap(NO) error:&err];
            id<CHHapticPatternPlayer> pl = [me.padEngine createPlayerWithPattern:p error:&err];
            [pl startAtTime:0 error:&err];
            printf("[app] padrão de %.2f s tocando no controle\n", p.duration);
        }
    };
    g.leftThumbstick.valueChangedHandler = ^(GCControllerDirectionPad *d, float x, float y) {
        printf("[app] analógico esquerdo %.2f %.2f\n", x, y);
        NSPoint p = me.palco.pos;
        me.palco.pos = NSMakePoint(p.x + x * 10, p.y - y * 10);
        me.palco.needsDisplay = YES;
    };
    g.valueChangedHandler = ^(GCExtendedGamepad *gp, GCControllerElement *e) {
        printf("[app] mudou: %s\n", e.localizedName.UTF8String);
    };
    if (c.haptics) {
        self.padEngine = [c.haptics createEngineWithLocality:GCHapticsLocalityDefault];
        [self.padEngine startAndReturnError:nil];
    }
}
- (void)applicationDidFinishLaunching:(NSNotification *)n {
    self.window = [[NSWindow alloc] initWithContentRect:NSMakeRect(100, 100, 480, 320)
                                              styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                                                backing:NSBackingStoreBuffered defer:NO];
    self.window.title = @"Jogo";
    self.palco = [[Palco alloc] initWithFrame:NSMakeRect(0, 0, 480, 320)];
    self.palco.pos = NSMakePoint(220, 150);
    self.window.contentView = self.palco;
    [self.window makeKeyAndOrderFront:nil];

    [[NSNotificationCenter defaultCenter] addObserverForName:GCControllerDidConnectNotification object:nil queue:nil
                                                  usingBlock:^(NSNotification *note) { [self ligar:note.object]; }];
    [[NSNotificationCenter defaultCenter] addObserverForName:GCControllerDidDisconnectNotification object:nil queue:nil
                                                  usingBlock:^(NSNotification *note) { printf("[app] controle desconectado\n"); }];
    for (GCController *c in [GCController controllers]) [self ligar:c];

    __weak AppDelegate *me = self;
    [GCKeyboard coalescedKeyboard].keyboardInput.keyChangedHandler = ^(GCKeyboardInput *k, GCControllerButtonInput *key, NSInteger code, BOOL pressed) {
        printf("[app] tecla HID 0x%02lx %s\n", (long)code, pressed ? "apertada" : "solta");
        if (!pressed) return;
        NSPoint p = me.palco.pos;
        if (code == 0x1A) p.y -= 10; if (code == 0x16) p.y += 10; if (code == 0x04) p.x -= 10; if (code == 0x07) p.x += 10;
        me.palco.pos = p;
        me.palco.needsDisplay = YES;
    };

    CHHapticDeviceCapabilityMOS *cap = [CHHapticEngine capabilitiesForHardware];
    printf("[app] Mac: hápticos %s, áudio %s\n", cap.supportsHaptics ? "sim" : "não", cap.supportsAudio ? "sim" : "não");
    self.macEngine = [[CHHapticEngine alloc] initAndReturnError:nil];
    [self.macEngine startAndReturnError:nil];
    CHHapticPattern *p = [[CHHapticPattern alloc] initWithDictionary:ahap(YES) error:nil];
    [[self.macEngine createPlayerWithPattern:p error:nil] startAtTime:0 error:nil];
}
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)a { return YES; }
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
