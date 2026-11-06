// Bloco — editor de texto Cocoa de exemplo: NSTextView rolável, Auto Layout com layoutMarginsGuide,
// contador de palavras (textDidChange:) e menu Arquivo para abrir/salvar ~/Bloco.txt.
#import <Cocoa/Cocoa.h>

@interface BlocoDelegate : NSObject <NSApplicationDelegate, NSTextViewDelegate>
@property (strong) NSWindow *window;
@property (strong) NSTextView *texto;
@property (strong) NSTextField *status;
@end

@implementation BlocoDelegate
- (NSString *)arquivo { return [NSHomeDirectory() stringByAppendingPathComponent:@"Bloco.txt"]; }
- (void)applicationDidFinishLaunching:(NSNotification *)n {
    NSMenu *barra = [[NSMenu alloc] initWithTitle:@""];
    NSMenuItem *app = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
    app.submenu = [[NSMenu alloc] initWithTitle:@"Bloco"];
    [app.submenu addItemWithTitle:@"Encerrar Bloco" action:@selector(terminate:) keyEquivalent:@"q"];
    [barra addItem:app];
    NSMenuItem *arq = [[NSMenuItem alloc] initWithTitle:@"Arquivo" action:nil keyEquivalent:@""];
    arq.submenu = [[NSMenu alloc] initWithTitle:@"Arquivo"];
    [arq.submenu addItemWithTitle:@"Abrir Bloco.txt" action:@selector(abrir:) keyEquivalent:@"o"];
    [arq.submenu addItemWithTitle:@"Salvar" action:@selector(salvar:) keyEquivalent:@"s"];
    [barra addItem:arq];
    NSApp.mainMenu = barra;

    self.window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 520, 360)
                                              styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable
                                                backing:NSBackingStoreBuffered defer:NO];
    self.window.title = @"Bloco";
    NSView *c = self.window.contentView;

    NSScrollView *rolagem = [NSTextView scrollableTextView];
    rolagem.translatesAutoresizingMaskIntoConstraints = NO;
    self.texto = rolagem.documentView;
    self.texto.font = [NSFont monospacedSystemFontOfSize:13 weight:0];
    self.texto.delegate = self;
    self.texto.string = @"Bem-vindo ao Bloco!\nDigite à vontade: o texto quebra as linhas sozinho e a área rola quando passa da janela.";
    [c addSubview:rolagem];
    self.status = [NSTextField labelWithString:@""];
    self.status.translatesAutoresizingMaskIntoConstraints = NO;
    [c addSubview:self.status];

    NSLayoutGuide *m = c.layoutMarginsGuide;
    [NSLayoutConstraint activateConstraints:@[
        [rolagem.topAnchor constraintEqualToAnchor:m.topAnchor],
        [rolagem.leadingAnchor constraintEqualToAnchor:m.leadingAnchor],
        [rolagem.trailingAnchor constraintEqualToAnchor:m.trailingAnchor],
        [rolagem.bottomAnchor constraintEqualToAnchor:self.status.topAnchor constant:-8],
        [self.status.leadingAnchor constraintEqualToAnchor:m.leadingAnchor],
        [self.status.bottomAnchor constraintEqualToAnchor:m.bottomAnchor],
    ]];
    [self atualizar];
    [self.window makeKeyAndOrderFront:nil];
    [self.window makeFirstResponder:self.texto];
}
- (void)atualizar {
    NSArray *palavras = [self.texto.string componentsSeparatedByString:@" "];
    NSUInteger n = 0;
    for (NSString *p in palavras) if (p.length) n++;
    self.status.stringValue = [NSString stringWithFormat:@"%lu caracteres · %lu palavras", (unsigned long)self.texto.string.length, (unsigned long)n];
}
- (void)textDidChange:(NSNotification *)n { [self atualizar]; }
- (void)salvar:(id)s {
    BOOL ok = [self.texto.string writeToFile:[self arquivo] atomically:YES encoding:NSUTF8StringEncoding error:nil];
    self.status.stringValue = ok ? @"Salvo em ~/Bloco.txt" : @"Falha ao salvar";
    NSLog(@"salvar: %d (%lu bytes)", ok, (unsigned long)self.texto.string.length);
}
- (void)abrir:(id)s {
    NSString *t = [NSString stringWithContentsOfFile:[self arquivo] encoding:NSUTF8StringEncoding error:nil];
    if (t) { self.texto.string = t; [self atualizar]; } else self.status.stringValue = @"~/Bloco.txt ainda não existe";
}
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)a { return YES; }
@end

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        BlocoDelegate *d = [BlocoDelegate new];
        app.delegate = d;
        [app run];
    }
    return 0;
}
