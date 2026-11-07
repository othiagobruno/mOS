// Descompactador — app de exemplo no papel de um extrator de terceiros (como Keka ou The Unarchiver).
// O Info.plist declara .zip (Alternate: o Utilitário de Compactação continua o padrão), .rar e .7z (Owner) e um
// tipo exportado (.dcz). No mOS o Launch Services lê isso ao instalar o app em /Aplicativos: os arquivos
// .rar/.7z/.dcz passam a abrir com ele (com o ícone e o nome de tipo dele) e o .zip ganha a opção em "Abrir com".
// Os documentos chegam como no macOS: -application:openURLs: do delegate.
#import <Cocoa/Cocoa.h>

// do CoreServices (o SDK do mOS é mínimo)
typedef int OSStatus;
extern OSStatus LSSetDefaultRoleHandlerForContentType(id type, unsigned int roles, id bundleID);   // CFStringRef no SDK real
#define kLSRolesAll 0xFFFFFFFFu

@interface AppDelegate : NSObject <NSApplicationDelegate>
@property (strong) NSWindow *window;
@property (strong) NSTextField *status;
@property (strong) NSMutableArray<NSString *> *recebidos;
@end

@implementation AppDelegate
- (void)applicationDidFinishLaunching:(NSNotification *)n {
    self.recebidos = self.recebidos ?: [NSMutableArray array];
    self.window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 460, 220)
                                              styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
    self.window.title = @"Descompactador";
    NSView *c = self.window.contentView;
    NSTextField *t = [NSTextField labelWithString:@"Arraste ou abra um arquivo compactado."];
    t.font = [NSFont systemFontOfSize:15 weight:NSFontWeightSemibold];
    t.frame = NSMakeRect(20, 170, 420, 24);
    [c addSubview:t];
    self.status = [NSTextField labelWithString:@"Nenhum arquivo recebido."];
    self.status.frame = NSMakeRect(20, 40, 420, 110);
    [c addSubview:self.status];
    NSButton *b = [NSButton buttonWithTitle:@"Tornar padrão para .zip" target:self action:@selector(tornarPadrao:)];
    b.frame = NSMakeRect(20, 10, 220, 26);
    [c addSubview:b];
    [self atualizar];
    [self.window center];
    [self.window makeKeyAndOrderFront:nil];
}
- (void)atualizar {
    if (!self.status) return;
    self.status.stringValue = self.recebidos.count ? [@"Recebidos:\n" stringByAppendingString:[self.recebidos componentsJoinedByString:@"\n"]]
                                                   : @"Nenhum arquivo recebido.";
}
// macOS 10.13+: documentos abertos pelo Finder, por "Abrir com" ou por `open -a`
- (void)application:(NSApplication *)app openURLs:(NSArray *)urls {
    if (!self.recebidos) self.recebidos = [NSMutableArray array];
    for (id u in urls) {
        NSString *p = [u performSelector:@selector(path)];
        NSLog(@"Descompactador: recebeu %@", p);
        [self.recebidos addObject:p.lastPathComponent ?: p];
        // prova para os testes: a lista também vai para ~/Downloads/recebido.txt
        FILE *f = fopen([[NSHomeDirectory() stringByAppendingString:@"/Downloads/recebido.txt"] UTF8String], "a");
        if (f) { fprintf(f, "%s\n", p.UTF8String); fclose(f); }
    }
    [self atualizar];
}
// como o The Unarchiver/Keka: o próprio app pede para ser o padrão de um tipo
- (void)tornarPadrao:(id)sender {
    id ident = [[NSBundle mainBundle] performSelector:@selector(bundleIdentifier)];
    OSStatus r = LSSetDefaultRoleHandlerForContentType(@"public.zip-archive", kLSRolesAll, ident);
    NSLog(@"Descompactador: LSSetDefaultRoleHandlerForContentType -> %d", (int)r);
}
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
