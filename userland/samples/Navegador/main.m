// Navegador — app de exemplo no papel de um navegador de terceiros (como o Chrome ou o Firefox).
// O Info.plist declara os esquemas http/https e as páginas HTML. "Definir como navegador padrão" faz o que o
// Chrome faz no macOS: LSSetDefaultHandlerForURLScheme para http e https; o sistema pergunta ao usuário
// ("Deseja alterar o navegador padrão para “Navegador” ou manter “Horizonte”?"). Links abertos por outros apps
// chegam pelo argv (app fechado) ou pelo evento kAEGetURL / -application:openURLs: (app aberto).
#import <Cocoa/Cocoa.h>

typedef int OSStatus;
extern OSStatus LSSetDefaultHandlerForURLScheme(id scheme, id bundleID);   // CFStringRef no SDK real
extern id LSCopyDefaultHandlerForURLScheme(id scheme);

@interface AppDelegate : NSObject <NSApplicationDelegate>
@property (strong) NSWindow *window;
@property (strong) NSTextField *status, *padrao;
@property (strong) NSMutableArray<NSString *> *links;
@end

static void registrar(NSString *s) {
    FILE *f = fopen([[NSHomeDirectory() stringByAppendingString:@"/Downloads/navegador.txt"] UTF8String], "a");
    if (f) { fprintf(f, "%s\n", s.UTF8String); fclose(f); }
}

@implementation AppDelegate
- (void)applicationDidFinishLaunching:(NSNotification *)n {
    if (!self.links) self.links = [NSMutableArray array];
    NSArray *args = [[NSProcessInfo processInfo] arguments];
    for (NSUInteger i = 1; i < args.count; i++)
        if ([args[i] hasPrefix:@"http"] || [args[i] hasPrefix:@"file:"]) { [self.links addObject:args[i]]; registrar([@"argv " stringByAppendingString:args[i]]); }
    self.window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 480, 240)
                                              styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
    self.window.title = @"Navegador";
    NSView *c = self.window.contentView;
    self.padrao = [NSTextField labelWithString:@""];
    self.padrao.frame = NSMakeRect(20, 196, 440, 22);
    [c addSubview:self.padrao];
    self.status = [NSTextField labelWithString:@""];
    self.status.frame = NSMakeRect(20, 50, 440, 136);
    [c addSubview:self.status];
    NSButton *b = [NSButton buttonWithTitle:@"Definir como navegador padrão" target:self action:@selector(definir:)];
    b.frame = NSMakeRect(20, 12, 260, 26);
    [c addSubview:b];
    [self atualizar];
    [self.window center];
    [self.window makeKeyAndOrderFront:nil];
}
- (void)atualizar {
    id cur = LSCopyDefaultHandlerForURLScheme(@"https");
    self.padrao.stringValue = [@"Navegador padrão do sistema: " stringByAppendingString:cur ?: @"—"];
    self.status.stringValue = self.links.count ? [@"Links recebidos:\n" stringByAppendingString:[self.links componentsJoinedByString:@"\n"]] : @"Nenhum link recebido.";
}
- (void)application:(NSApplication *)app openURLs:(NSArray *)urls {
    if (!self.links) self.links = [NSMutableArray array];
    for (id u in urls) {
        NSString *s = [u performSelector:@selector(absoluteString)];
        [self.links addObject:s];
        registrar([@"evento " stringByAppendingString:s]);
    }
    [self atualizar];
}
- (void)definir:(id)sender {
    id ident = [[NSBundle mainBundle] performSelector:@selector(bundleIdentifier)];
    OSStatus a = LSSetDefaultHandlerForURLScheme(@"http", ident), b = LSSetDefaultHandlerForURLScheme(@"https", ident);
    registrar([NSString stringWithFormat:@"definir %d %d", (int)a, (int)b]);
}
- (void)applicationDidBecomeActive:(NSNotification *)n { [self atualizar]; }
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)app { return YES; }
@end

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        AppDelegate *d = [AppDelegate new];
        app.delegate = d;
        [app run];
    }
    return 0;
}
