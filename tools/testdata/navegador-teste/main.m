// mOS — "navegador" de teste para o login pelo navegador (userland/libsystem/openurl.c): declara http/https, então um
// app que chama -[NSWorkspace openURL:] / shell.openExternal() abre aqui quando não há Chrome na imagem. Mostra e
// registra (stderr, no log serial com -Diagnostico) cada URL recebida: na linha de comando (app iniciado pela URL)
// ou por -application:openURLs: (app já aberto). Só esquema e host vão para o log: a URL de login leva token.
// Compilar: tools\testdata\navegador-teste\build.ps1 (gera build\navegador-teste\Navegador de Teste.app)
#import <Cocoa/Cocoa.h>

// esquema e host, sem caminho nem consulta
static NSString *Curta(NSString *u) {
    NSRange sep = [u rangeOfString:@"://"];
    if (sep.location == NSNotFound) return u.length > 40 ? [[u substringToIndex:40] stringByAppendingString:@"…"] : u;
    NSUInteger i = sep.location + 3;
    while (i < u.length) { unsigned short c = [u characterAtIndex:i]; if (c == '/' || c == '?' || c == '#') break; i++; }
    return [[u substringToIndex:i] stringByAppendingString:i < u.length ? @"/…" : @""];
}
@protocol URLAbsoluta
- (NSString *)absoluteString;
@end

@interface AppDelegate : NSObject <NSApplicationDelegate>
@property (strong) NSWindow *window;
@property (strong) NSTextField *lista;
@property (strong) NSMutableArray<NSString *> *recebidas;
@end

@implementation AppDelegate
- (void)mostrar:(NSString *)u origem:(NSString *)origem {
    fprintf(stderr, "[navegador-teste] %s: %s\n", origem.UTF8String, Curta(u).UTF8String);
    [self.recebidas addObject:[NSString stringWithFormat:@"%@  (%@)", Curta(u), origem]];
    self.lista.stringValue = [self.recebidas componentsJoinedByString:@"\n"];
}
- (void)applicationDidFinishLaunching:(NSNotification *)n {
    self.recebidas = [NSMutableArray array];
    self.window = [[NSWindow alloc] initWithContentRect:NSMakeRect(80, 80, 560, 240)
                                              styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
    self.window.title = @"Navegador de Teste";
    NSTextField *t = [NSTextField labelWithString:@"URLs recebidas:"];
    t.font = [NSFont boldSystemFontOfSize:15];
    t.frame = NSMakeRect(20, 196, 500, 24);
    [self.window.contentView addSubview:t];
    self.lista = [NSTextField labelWithString:@"(nenhuma)"];
    self.lista.frame = NSMakeRect(20, 20, 520, 170);
    [self.window.contentView addSubview:self.lista];
    [self.window makeKeyAndOrderFront:nil];
    NSArray *args = NSProcessInfo.processInfo.arguments;
    for (NSUInteger i = 1; i < args.count; i++)
        if ([args[i] hasPrefix:@"http"]) [self mostrar:args[i] origem:@"linha de comando"];
}
- (void)application:(NSApplication *)app openURLs:(NSArray *)urls {
    for (id<URLAbsoluta> u in urls) [self mostrar:[u absoluteString] origem:@"application:openURLs:"];
}
@end

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        AppDelegate *d = [AppDelegate new];
        app.delegate = d;
        [app run];
    }
    return 0;
}
