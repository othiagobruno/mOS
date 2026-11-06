// Contador — app Cocoa de exemplo para o mOS (Objective-C + AppKit, compilado para x86_64 macOS).
// Compila igual no macOS de verdade; no mOS roda sobre o AppKit do libSystem do mOS.
#import <Cocoa/Cocoa.h>

@interface Painel : NSView
@property (nonatomic) NSInteger cliques;
@property (nonatomic) double nivel;
@end

@implementation Painel
- (void)drawRect:(NSRect)dirty {
    NSRect b = self.bounds;
    [[NSColor colorWithCalibratedRed:0.95 green:0.96 blue:1.0 alpha:1] setFill];
    [[NSBezierPath bezierPathWithRoundedRect:b xRadius:10 yRadius:10] fill];
    // barras proporcionais ao nível do slider
    for (int i = 0; i < 8; i++) {
        CGFloat h = (NSHeight(b) - 40) * self.nivel * (0.4 + 0.6 * ((i * 37) % 10) / 10.0);
        [[NSColor colorWithHue:i / 8.0 saturation:0.65 brightness:0.95 alpha:1] setFill];
        NSRectFill(NSMakeRect(16 + i * 26, 14, 18, h));
    }
    // círculo que cresce com os cliques
    CGFloat r = 12 + (self.cliques % 10) * 4;
    [[NSColor systemPinkColor] setFill];
    [[NSBezierPath bezierPathWithOvalInRect:NSMakeRect(NSWidth(b) - 60 - r, NSHeight(b) / 2 - r, 2 * r, 2 * r)] fill];
    NSDictionary *attrs = @{NSFontAttributeName : [NSFont boldSystemFontOfSize:12],
                            NSForegroundColorAttributeName : [NSColor colorWithWhite:0.25 alpha:1]};
    [@"View personalizada (drawRect:)" drawAtPoint:NSMakePoint(16, NSHeight(b) - 24) withAttributes:attrs];
}
- (void)mouseDown:(NSEvent *)e {
    NSPoint p = [self convertPoint:e.locationInWindow fromView:nil];
    NSLog(@"clique no painel em (%.0f, %.0f)", p.x, p.y);
    self.cliques++;
    self.needsDisplay = YES;
}
@end

@interface AppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>
@property (strong) NSWindow *window;
@property (strong) NSTextField *contador, *relogio, *saudacao, *nome;
@property (strong) NSButton *dobro;
@property (strong) Painel *painel;
@property NSInteger valor;
@end

@implementation AppDelegate
- (void)applicationDidFinishLaunching:(NSNotification *)n {
    self.window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 520, 420)
                                              styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
    self.window.title = @"Contador — Cocoa no mOS";
    self.window.delegate = self;
    NSView *c = self.window.contentView;

    NSTextField *titulo = [NSTextField labelWithString:@"Olá do AppKit!"];
    titulo.font = [NSFont systemFontOfSize:22 weight:NSFontWeightBold];
    [titulo sizeToFit];
    titulo.frame = NSMakeRect(20, 370, 400, 30);
    [c addSubview:titulo];

    self.contador = [NSTextField labelWithString:@"Valor: 0"];
    self.contador.font = [NSFont systemFontOfSize:15];
    self.contador.frame = NSMakeRect(20, 335, 200, 22);
    [c addSubview:self.contador];

    NSButton *mais = [NSButton buttonWithTitle:@"+1" target:self action:@selector(somar:)];
    mais.frame = NSMakeRect(230, 332, 70, 26);
    mais.keyEquivalent = @"\r";
    [c addSubview:mais];
    NSButton *zerar = [NSButton buttonWithTitle:@"Zerar" target:self action:@selector(zerar:)];
    zerar.frame = NSMakeRect(310, 332, 80, 26);
    [c addSubview:zerar];
    self.dobro = [NSButton checkboxWithTitle:@"Somar de 2 em 2" target:self action:@selector(trocouModo:)];
    self.dobro.frame = NSMakeRect(340, 255, 170, 20);
    [c addSubview:self.dobro];

    self.nome = [NSTextField textFieldWithString:@""];
    self.nome.placeholderString = @"Seu nome";
    self.nome.frame = NSMakeRect(20, 292, 220, 24);
    self.nome.target = self;
    self.nome.action = @selector(saudar:);
    [c addSubview:self.nome];
    NSButton *saudar = [NSButton buttonWithTitle:@"Saudar" target:self action:@selector(saudar:)];
    saudar.frame = NSMakeRect(250, 291, 90, 26);
    [c addSubview:saudar];
    self.saudacao = [NSTextField labelWithString:@""];
    self.saudacao.frame = NSMakeRect(350, 294, 160, 20);
    self.saudacao.textColor = [NSColor systemBlueColor];
    [c addSubview:self.saudacao];

    NSSlider *s = [NSSlider sliderWithValue:0.5 minValue:0 maxValue:1 target:self action:@selector(mudouNivel:)];
    s.frame = NSMakeRect(20, 255, 300, 22);
    [c addSubview:s];

    self.painel = [[Painel alloc] initWithFrame:NSMakeRect(20, 60, 480, 180)];
    self.painel.nivel = 0.5;
    self.painel.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    [c addSubview:self.painel];

    self.relogio = [NSTextField labelWithString:@"--:--:--"];
    self.relogio.font = [NSFont monospacedDigitSystemFontOfSize:13 weight:NSFontWeightRegular];
    self.relogio.frame = NSMakeRect(20, 22, 200, 20);
    [c addSubview:self.relogio];
    NSButton *sobre = [NSButton buttonWithTitle:@"Sobre…" target:self action:@selector(sobre:)];
    sobre.frame = NSMakeRect(410, 18, 90, 26);
    [c addSubview:sobre];

    [NSTimer scheduledTimerWithTimeInterval:1.0 target:self selector:@selector(tique:) userInfo:nil repeats:YES];
    [self tique:nil];
    [self.window makeKeyAndOrderFront:nil];
    NSLog(@"Contador iniciado: %@ (%lu views)", self.window.title, (unsigned long)c.subviews.count);
}
- (void)atualizar { self.contador.stringValue = [NSString stringWithFormat:@"Valor: %ld", (long)self.valor]; }
- (void)somar:(id)sender { self.valor += self.dobro.state == NSControlStateValueOn ? 2 : 1; [self atualizar]; }
- (void)zerar:(id)sender { self.valor = 0; [self atualizar]; }
- (void)trocouModo:(NSButton *)b { NSLog(@"modo de 2 em 2: %@", b.state ? @"ligado" : @"desligado"); }
- (void)saudar:(id)sender {
    NSString *n = self.nome.stringValue.length ? self.nome.stringValue : @"visitante";
    self.saudacao.stringValue = [NSString stringWithFormat:@"Olá, %@!", n];
}
- (void)mudouNivel:(NSSlider *)s { self.painel.nivel = s.doubleValue; self.painel.needsDisplay = YES; }
- (void)tique:(NSTimer *)t {
    long seg = (long)[NSDate date].timeIntervalSince1970;
    self.relogio.stringValue = [NSString stringWithFormat:@"%02ld:%02ld:%02ld UTC", (seg / 3600) % 24, (seg / 60) % 60, seg % 60];
}
- (void)sobre:(id)sender {
    NSAlert *a = [[NSAlert alloc] init];
    a.messageText = @"Contador";
    a.informativeText = @"Um app Cocoa (Objective-C, AppKit) rodando no mOS.";
    [a addButtonWithTitle:@"Legal"];
    [a addButtonWithTitle:@"Cancelar"];
    NSModalResponse r = [a runModal];
    NSLog(@"alerta respondeu %ld", (long)r);
}
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)app { return YES; }
@end

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        AppDelegate *d = [[AppDelegate alloc] init];
        app.delegate = d;
        [app run];
    }
    return 0;
}
