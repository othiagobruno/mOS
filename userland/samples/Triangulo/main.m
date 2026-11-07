// Triangulo — app Metal de exemplo: MTKView com um triângulo colorido que gira. Os shaders ficam em
// Resources/Shaders.metal e são compilados do fonte pelo newDefaultLibrary (Metal por software do mOS).
#import <Cocoa/Cocoa.h>
#import <MetalKit/MetalKit.h>

typedef struct { float angle, aspect; } Uniforms;

@interface Renderer : NSObject <MTKViewDelegate>
@property (strong) id<MTLDevice> device;
@property (strong) id<MTLCommandQueue> queue;
@property (strong) id<MTLRenderPipelineState> pipeline;
@property float angle;
@property NSUInteger frames;
@end

@implementation Renderer
- (instancetype)initWithView:(MTKView *)view {
    self = [super init];
    self.device = view.device;
    self.queue = [self.device newCommandQueue];
    id<MTLLibrary> lib = [self.device newDefaultLibrary];
    if (!lib) { NSLog(@"Triangulo: sem biblioteca de shaders"); return self; }
    MTLRenderPipelineDescriptor *d = [[MTLRenderPipelineDescriptor alloc] init];
    d.vertexFunction = [lib newFunctionWithName:@"vs"];
    d.fragmentFunction = [lib newFunctionWithName:@"fs"];
    d.colorAttachments[0].pixelFormat = view.colorPixelFormat;
    NSError *err = nil;
    self.pipeline = [self.device newRenderPipelineStateWithDescriptor:d error:&err];
    if (!self.pipeline) NSLog(@"Triangulo: pipeline: %@", err.localizedDescription);
    return self;
}
- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size { (void)view; (void)size; }
- (void)drawInMTKView:(MTKView *)view {
    MTLRenderPassDescriptor *rp = view.currentRenderPassDescriptor;
    if (!rp || !self.pipeline) return;
    id<MTLCommandBuffer> cb = [self.queue commandBuffer];
    id<MTLRenderCommandEncoder> e = [cb renderCommandEncoderWithDescriptor:rp];
    [e setRenderPipelineState:self.pipeline];
    CGSize s = view.drawableSize;
    Uniforms u = {self.angle, s.height > 0 ? (float)(s.width / s.height) : 1.0f};
    [e setVertexBytes:&u length:sizeof u atIndex:0];
    [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [e endEncoding];
    [cb presentDrawable:view.currentDrawable];
    [cb commit];
    self.angle += 0.05f;
    if (++self.frames % 30 == 1) NSLog(@"Triangulo: quadro %lu (%.0fx%.0f)", (unsigned long)self.frames, s.width, s.height);
}
@end

@interface TrianguloDelegate : NSObject <NSApplicationDelegate>
@property (strong) NSWindow *window;
@property (strong) Renderer *renderer;
@end

@implementation TrianguloDelegate
- (void)applicationDidFinishLaunching:(NSNotification *)n {
    NSMenu *barra = [[NSMenu alloc] initWithTitle:@""];
    NSMenuItem *app = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
    app.submenu = [[NSMenu alloc] initWithTitle:@"Triangulo"];
    [app.submenu addItemWithTitle:@"Encerrar Triangulo" action:@selector(terminate:) keyEquivalent:@"q"];
    [barra addItem:app];
    NSApp.mainMenu = barra;

    self.window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 420, 320)
                                              styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable
                                                backing:NSBackingStoreBuffered defer:NO];
    self.window.title = @"Triangulo (Metal)";
    id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
    if (!dev) {
        NSTextField *l = [NSTextField labelWithString:@"Sem Metal neste sistema."];
        l.frame = NSMakeRect(20, 140, 380, 24);
        [self.window.contentView addSubview:l];
    } else {
        MTKView *v = [[MTKView alloc] initWithFrame:NSMakeRect(0, 0, 420, 320) device:dev];
        v.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
        v.clearColor = MTLClearColorMake(0.08, 0.09, 0.12, 1);
        v.preferredFramesPerSecond = 30;
        self.renderer = [[Renderer alloc] initWithView:v];
        v.delegate = self.renderer;
        self.window.contentView = v;
    }
    [self.window makeKeyAndOrderFront:nil];
}
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)a { return YES; }
@end

int main(int argc, const char **argv) {
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        TrianguloDelegate *d = [[TrianguloDelegate alloc] init];
        app.delegate = d;
        [app run];
    }
    return 0;
}
