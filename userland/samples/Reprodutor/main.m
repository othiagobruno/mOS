// Reprodutor — app de exemplo no papel de um player de terceiros (como o VLC ou o IINA).
// O Info.plist declara filmes (public.movie, posto Default) e áudio (public.audio, Alternate). No mOS, ao instalar,
// os .mp4/.mov/.mkv passam a abrir com ele (nenhum app do sistema toca esses formatos); os .wav continuam no
// QuickTime, com o Reprodutor em "Abrir com" e em "Obter Informações > Abrir com > Alterar Tudo…".
#import <Cocoa/Cocoa.h>

@interface AppDelegate : NSObject <NSApplicationDelegate>
@property (strong) NSWindow *window;
@property (strong) NSTextField *status;
@property (strong) NSMutableArray<NSString *> *itens;
@end

#import <objc/message.h>

@implementation AppDelegate
// como players antigos (e o VLC): trata 'aevt'/'odoc' direto, no lugar do tratador padrão do AppKit
- (void)applicationWillFinishLaunching:(NSNotification *)n {
    id aem = [NSClassFromString(@"NSAppleEventManager") performSelector:@selector(sharedAppleEventManager)];
    ((void (*)(id, SEL, id, SEL, unsigned, unsigned))objc_msgSend)(aem, @selector(setEventHandler:andSelector:forEventClass:andEventID:),
        self, @selector(abrirDocumentos:resposta:), 'aevt', 'odoc');
}
- (void)abrirDocumentos:(id)evento resposta:(id)resposta {
    if (!self.itens) self.itens = [NSMutableArray array];
    id lista = ((id (*)(id, SEL, unsigned))objc_msgSend)(evento, @selector(paramDescriptorForKeyword:), '----');
    long n = ((long (*)(id, SEL))objc_msgSend)(lista, @selector(numberOfItems));
    for (long i = 1; i <= n; i++) {
        id item = ((id (*)(id, SEL, long))objc_msgSend)(lista, @selector(descriptorAtIndex:), i);
        id url = [item performSelector:@selector(fileURLValue)];
        NSString *p = [url performSelector:@selector(path)];
        [self.itens addObject:[@"odoc: " stringByAppendingString:p.lastPathComponent ?: @"?"]];
        FILE *f = fopen([[NSHomeDirectory() stringByAppendingString:@"/Downloads/reprodutor.txt"] UTF8String], "a");
        if (f) { fprintf(f, "odoc %ld/%ld %s\n", i, n, p.UTF8String); fclose(f); }
    }
    [self atualizar];
}
- (void)applicationDidFinishLaunching:(NSNotification *)n {
    if (!self.itens) self.itens = [NSMutableArray array];
    self.window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 420, 200)
                                              styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
    self.window.title = @"Reprodutor";
    self.status = [NSTextField labelWithString:@""];
    self.status.frame = NSMakeRect(20, 20, 380, 160);
    [self.window.contentView addSubview:self.status];
    [self atualizar];
    [self.window center];
    [self.window makeKeyAndOrderFront:nil];
}
- (void)atualizar {
    if (!self.status) return;
    self.status.stringValue = self.itens.count ? [@"Reproduzindo:\n" stringByAppendingString:[self.itens componentsJoinedByString:@"\n"]] : @"Nada para reproduzir.";
}
- (void)application:(NSApplication *)app openURLs:(NSArray *)urls {
    if (!self.itens) self.itens = [NSMutableArray array];
    for (id u in urls) {
        NSString *p = [u performSelector:@selector(path)];
        [self.itens addObject:p.lastPathComponent ?: p];
        FILE *f = fopen([[NSHomeDirectory() stringByAppendingString:@"/Downloads/reprodutor.txt"] UTF8String], "a");
        if (f) { fprintf(f, "%s\n", p.UTF8String); fclose(f); }
    }
    [self atualizar];
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
