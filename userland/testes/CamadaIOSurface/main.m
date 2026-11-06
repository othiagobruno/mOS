// CamadaIOSurface — teste do caminho do Chromium/Electron no mOS: NSView com layer (CALayer) cujo
// contents é um IOSurface pintado à mão (BGRA), mais um CALayerHost mostrando um CAContext (como o
// processo da GPU entrega o quadro ao processo do navegador). Compila igual no macOS de verdade.
// Build: tools/build-camada-teste.ps1 (zig cc -target x86_64-macos contra userland/sdk).
#import <Cocoa/Cocoa.h>

// ---- o SDK do mOS ainda não tem os cabeçalhos do QuartzCore e do IOSurface
#ifndef __APPLE_SDK_FULL__
typedef const struct __CFDictionary *CFDictionaryRef;
typedef const struct __CFString *CFStringRef;
typedef struct CGColor *CGColorRef;
typedef unsigned int mach_port_t;
#define CGPointZero CGPointMake(0, 0)
#endif
typedef struct __IOSurface *IOSurfaceRef;
extern const CFStringRef kIOSurfaceWidth, kIOSurfaceHeight, kIOSurfaceBytesPerElement, kIOSurfacePixelFormat, kIOSurfaceBytesPerRow;
IOSurfaceRef IOSurfaceCreate(CFDictionaryRef props);
int IOSurfaceLock(IOSurfaceRef s, uint32_t opts, uint32_t *seed);
int IOSurfaceUnlock(IOSurfaceRef s, uint32_t opts, uint32_t *seed);
void *IOSurfaceGetBaseAddress(IOSurfaceRef s);
size_t IOSurfaceGetBytesPerRow(IOSurfaceRef s), IOSurfaceGetWidth(IOSurfaceRef s), IOSurfaceGetHeight(IOSurfaceRef s);
uint32_t IOSurfaceGetID(IOSurfaceRef s);
IOSurfaceRef IOSurfaceLookup(uint32_t sid);
mach_port_t IOSurfaceCreateMachPort(IOSurfaceRef s);
IOSurfaceRef IOSurfaceLookupFromMachPort(mach_port_t p);
CGColorRef CGColorCreateSRGB(CGFloat r, CGFloat g, CGFloat b, CGFloat a);
extern NSString *const kCAGravityTopLeft;

@interface CALayer : NSObject
+ (instancetype)layer;
@property CGRect frame, bounds;
@property CGPoint position, anchorPoint;
@property (strong) id contents;
@property CGFloat contentsScale;
@property (copy) NSString *contentsGravity;
@property CGColorRef backgroundColor, borderColor;
@property CGFloat borderWidth;
@property float opacity;
@property BOOL geometryFlipped, masksToBounds, hidden;
- (void)addSublayer:(CALayer *)l;
- (void)setAutoresizingMask:(unsigned)m;
@property (readonly) CALayer *superlayer;
@property (copy) NSArray *sublayers;
@end
@interface CATransaction : NSObject
+ (void)begin;
+ (void)commit;
+ (void)setDisableActions:(BOOL)b;
@end
@interface CAContext : NSObject
+ (instancetype)contextWithCGSConnection:(uint32_t)c options:(NSDictionary *)o;
@property (readonly) uint32_t contextId;
@property (strong) CALayer *layer;
@end
@interface CALayerHost : CALayer
@property uint32_t contextId;
@end
uint32_t CGSMainConnectionID(void);
@interface NSView (CamadaSDK)
@property BOOL wantsLayer;
@property (strong) CALayer *layer;
@end

// ---- pintura à mão no IOSurface (BGRA pré-multiplicado, linha 0 em cima)
static void pintar(IOSurfaceRef s, int quadro) {
    IOSurfaceLock(s, 0, NULL);
    uint8_t *base = IOSurfaceGetBaseAddress(s);
    size_t bpr = IOSurfaceGetBytesPerRow(s), w = IOSurfaceGetWidth(s), h = IOSurfaceGetHeight(s);
    for (size_t y = 0; y < h; y++) {
        uint32_t *row = (uint32_t *)(base + y * bpr);
        for (size_t x = 0; x < w; x++) {
            // degradê: vermelho cresce para a direita, azul para baixo; faixa verde que anda
            uint32_t r = (uint32_t)(x * 255 / (w - 1)), b = (uint32_t)(y * 255 / (h - 1));
            uint32_t g = ((x + quadro * 6) / 24) % 2 ? 200 : 40;
            row[x] = 0xFF000000u | r << 16 | g << 8 | b;
        }
    }
    // quadrado branco no canto superior esquerdo (orientação: tem que aparecer em cima à esquerda)
    for (size_t y = 8; y < 40 && y < h; y++)
        for (size_t x = 8; x < 40 && x < w; x++) ((uint32_t *)(base + y * bpr))[x] = 0xFFFFFFFFu;
    IOSurfaceUnlock(s, 0, NULL);
}

static IOSurfaceRef novo_surface(int w, int h) {
    NSDictionary *p = @{(__bridge NSString *)kIOSurfaceWidth : @(w), (__bridge NSString *)kIOSurfaceHeight : @(h),
                        (__bridge NSString *)kIOSurfaceBytesPerElement : @4, (__bridge NSString *)kIOSurfacePixelFormat : @(0x42475241)};
    return IOSurfaceCreate((__bridge CFDictionaryRef)p);
}

@interface AppDelegate : NSObject <NSApplicationDelegate>
@property (strong) NSWindow *window;
@property (strong) CALayer *camada, *gpuRaiz;
@property IOSurfaceRef surface, surface2;
@property int quadro;
@end

@implementation AppDelegate
- (void)applicationDidFinishLaunching:(NSNotification *)n {
    self.window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 560, 400)
                                              styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                                                backing:NSBackingStoreBuffered defer:NO];
    self.window.title = @"CALayer + IOSurface";
    NSView *c = self.window.contentView;

    NSTextField *t = [NSTextField labelWithString:@"IOSurface via CALayer (esq.) e CALayerHost (dir.)"];
    t.frame = NSMakeRect(20, 360, 520, 24);
    [c addSubview:t];

    // view com layer: raiz com fundo cinza, sublayer com o IOSurface (como o DisplayCALayerTree do Chromium)
    NSView *v = [[NSView alloc] initWithFrame:NSMakeRect(20, 20, 520, 330)];
    v.wantsLayer = YES;
    [c addSubview:v];
    CGColorRef cinza = CGColorCreateSRGB(0.25, 0.25, 0.28, 1);
    v.layer.backgroundColor = cinza;

    self.surface = novo_surface(240, 180);
    pintar(self.surface, 0);
    NSLog(@"IOSurface id=%u %zux%zu bpr=%zu", IOSurfaceGetID(self.surface), IOSurfaceGetWidth(self.surface),
          IOSurfaceGetHeight(self.surface), IOSurfaceGetBytesPerRow(self.surface));

    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    self.camada = [CALayer layer];
    self.camada.anchorPoint = CGPointZero;
    self.camada.contentsGravity = kCAGravityTopLeft;
    self.camada.frame = CGRectMake(10, 140, 240, 180);       // y para cima: perto do topo da view
    self.camada.contents = (__bridge id)self.surface;
    self.camada.borderWidth = 2;
    self.camada.borderColor = CGColorCreateSRGB(1, 1, 0, 1);
    [v.layer addSublayer:self.camada];

    // processo "GPU" (aqui no mesmo processo): CAContext com um layer cujo contents é outro IOSurface,
    // passado por porta Mach e mostrado por um CALayerHost
    self.surface2 = novo_surface(200, 150);
    pintar(self.surface2, 7);
    mach_port_t porta = IOSurfaceCreateMachPort(self.surface2);
    IOSurfaceRef recebido = IOSurfaceLookupFromMachPort(porta);
    NSLog(@"porta Mach %u -> surface %u (lookup por id: %s)", porta, recebido ? IOSurfaceGetID(recebido) : 0,
          IOSurfaceLookup(IOSurfaceGetID(self.surface2)) == recebido ? "ok" : "FALHOU");
    CAContext *ctx = [CAContext contextWithCGSConnection:CGSMainConnectionID() options:[NSDictionary dictionary]];
    self.gpuRaiz = [CALayer layer];
    self.gpuRaiz.anchorPoint = CGPointZero;
    self.gpuRaiz.frame = CGRectMake(0, 0, 200, 150);
    self.gpuRaiz.contents = (__bridge id)recebido;
    ctx.layer = self.gpuRaiz;
    // como o DisplayCALayerTree do Chromium: layer invertido de tamanho 0 que acompanha a raiz
    // (autoresizingMask Width|HeightSizable) e, dentro dele, o CALayerHost em coordenadas y-para-baixo
    CALayer *invertido = [CALayer layer];
    invertido.geometryFlipped = YES;
    invertido.anchorPoint = CGPointZero;
    [invertido setAutoresizingMask:2 | 16];
    [v.layer addSublayer:invertido];
    CALayerHost *host = [CALayerHost layer];
    host.anchorPoint = CGPointZero;
    host.frame = CGRectMake(300, 120, 200, 150);        // 120 abaixo do topo da view
    host.contextId = ctx.contextId;
    [invertido addSublayer:host];
    // layer semitransparente por cima (opacidade)
    CALayer *veu = [CALayer layer];
    veu.frame = CGRectMake(200, 20, 160, 60);
    veu.backgroundColor = CGColorCreateSRGB(1, 1, 1, 1);
    veu.opacity = 0.5;
    [v.layer addSublayer:veu];
    [CATransaction commit];
    NSLog(@"contextId=%u", ctx.contextId);

    [self.window makeKeyAndOrderFront:nil];
    // anima: repinta o IOSurface (a faixa verde anda); o IOSurfaceUnlock avisa o QuartzCore
    [NSTimer scheduledTimerWithTimeInterval:0.5 target:self selector:@selector(tick:) userInfo:nil repeats:YES];
}
- (void)tick:(NSTimer *)t {
    self.quadro++;
    pintar(self.surface, self.quadro);
    if (self.quadro == 3) {
        CALayer *inv = self.camada.superlayer.sublayers.lastObject;
        for (CALayer *l in self.camada.superlayer.sublayers) if (l.geometryFlipped) inv = l;
        NSLog(@"quadro 3 pintado; layer invertido %@", NSStringFromRect(inv.frame));
    }
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
