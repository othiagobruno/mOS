// Camadas — exemplo do QuartzCore do mOS: CAShapeLayer (estrela preenchida com contorno, círculo tracejado
// com strokeEnd), CAGradientLayer, CATextLayer, animações (CABasicAnimation com autoreverse, keyframes,
// mola) e um CADisplayLink contando quadros.
#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>

@interface NSView (MOSLayer)
@property (strong) CALayer *layer;
@end

@interface CamadasDelegate : NSObject <NSApplicationDelegate, CAAnimationDelegate>
@property (strong) NSWindow *window;
@property (strong) CADisplayLink *link;
@property NSUInteger ticks;
@property (strong) CALayer *quadrado;
@end

@implementation CamadasDelegate
- (void)tick:(CADisplayLink *)l {
    self.ticks++;
    if (self.ticks % 60 == 0) {
        NSLog(@"Camadas: displayLink %lu quadros; quadrado x=%.1f opacidade=%.2f", (unsigned long)self.ticks, self.quadrado.position.x, self.quadrado.opacity);
    }
}
- (void)animationDidStart:(CAAnimation *)a { NSLog(@"Camadas: animationDidStart"); }
- (void)animationDidStop:(CAAnimation *)a finished:(BOOL)f { NSLog(@"Camadas: animationDidStop finished=%d", f); }

- (void)applicationDidFinishLaunching:(NSNotification *)n {
    self.window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 480, 360)
                                              styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                                                backing:NSBackingStoreBuffered defer:NO];
    self.window.title = @"Camadas (QuartzCore)";
    NSView *v = self.window.contentView;
    v.wantsLayer = YES;
    CALayer *root = v.layer;
    root.backgroundColor = CGColorCreateSRGB(0.95, 0.95, 0.97, 1);

    CAGradientLayer *g = [CAGradientLayer layer];
    g.frame = CGRectMake(20, 220, 440, 120);
    g.colors = @[(__bridge id)CGColorCreateSRGB(0.9, 0.2, 0.3, 1), (__bridge id)CGColorCreateSRGB(0.95, 0.6, 0.1, 1),
                 (__bridge id)CGColorCreateSRGB(0.2, 0.3, 0.9, 1)];
    g.startPoint = CGPointMake(0, 0.5);
    g.endPoint = CGPointMake(1, 0.5);
    g.cornerRadius = 12;
    [root addSublayer:g];

    CATextLayer *t = [CATextLayer layer];
    t.frame = CGRectMake(20, 255, 440, 50);
    t.string = @"Olá, QuartzCore!";
    t.fontSize = 30;
    t.alignmentMode = kCAAlignmentCenter;
    t.foregroundColor = CGColorCreateSRGB(1, 1, 1, 1);
    [root addSublayer:t];

    CAShapeLayer *estrela = [CAShapeLayer layer];
    estrela.frame = CGRectMake(20, 40, 160, 160);
    CGMutablePathRef p = CGPathCreateMutable();
    for (int i = 0; i < 10; i++) {
        double a = M_PI / 2 + i * M_PI / 5, r = (i % 2) ? 32 : 72;
        double x = 80 + r * cos(a), y = 80 + r * sin(a);
        if (i == 0) CGPathMoveToPoint(p, NULL, x, y); else CGPathAddLineToPoint(p, NULL, x, y);
    }
    CGPathCloseSubpath(p);
    estrela.path = p;
    estrela.fillColor = CGColorCreateSRGB(1, 0.75, 0.1, 1);
    estrela.strokeColor = CGColorCreateSRGB(0.15, 0.2, 0.45, 1);
    estrela.lineWidth = 4;
    estrela.lineJoin = kCALineJoinRound;
    [root addSublayer:estrela];

    CAShapeLayer *circ = [CAShapeLayer layer];
    circ.frame = CGRectMake(200, 40, 160, 160);
    CGMutablePathRef c = CGPathCreateMutable();
    CGPathAddEllipseInRect(c, NULL, CGRectMake(15, 15, 130, 130));
    circ.path = c;
    circ.fillColor = nil;
    circ.strokeColor = CGColorCreateSRGB(0.1, 0.6, 0.35, 1);
    circ.lineWidth = 10;
    circ.lineCap = kCALineCapRound;
    circ.lineDashPattern = @[@18, @14];
    circ.strokeEnd = 0.8;
    [root addSublayer:circ];

    self.quadrado = [CALayer layer];
    self.quadrado.frame = CGRectMake(380, 90, 40, 40);
    self.quadrado.backgroundColor = CGColorCreateSRGB(0.55, 0.2, 0.75, 1);
    self.quadrado.cornerRadius = 8;
    [root addSublayer:self.quadrado];

    CABasicAnimation *mov = [CABasicAnimation animationWithKeyPath:@"position.y"];
    mov.fromValue = @60;
    mov.toValue = @180;
    mov.duration = 1.0;
    mov.autoreverses = YES;
    mov.repeatCount = HUGE_VALF;
    mov.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseInEaseOut];
    [self.quadrado addAnimation:mov forKey:@"sobe"];

    CAKeyframeAnimation *op = [CAKeyframeAnimation animationWithKeyPath:@"opacity"];
    op.values = @[@1, @0.3, @1];
    op.duration = 2;
    op.repeatCount = HUGE_VALF;
    [self.quadrado addAnimation:op forKey:@"pisca"];

    CABasicAnimation *uma = [CABasicAnimation animationWithKeyPath:@"cornerRadius"];
    uma.fromValue = @0;
    uma.toValue = @20;
    uma.duration = 0.5;
    uma.delegate = self;
    [self.quadrado addAnimation:uma forKey:@"canto"];

    self.link = [CADisplayLink displayLinkWithTarget:self selector:@selector(tick:)];
    [self.link addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
    [self.window makeKeyAndOrderFront:nil];
}
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)a { return YES; }
@end

int main(int argc, const char **argv) {
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        CamadasDelegate *d = [[CamadasDelegate alloc] init];
        app.delegate = d;
        [app run];
    }
    return 0;
}
