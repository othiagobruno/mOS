// Formulario — app Cocoa de exemplo com Auto Layout (âncoras e Visual Format Language), sem frames fixos.
#import <Cocoa/Cocoa.h>

@interface FormDelegate : NSObject <NSApplicationDelegate>
@property (strong) NSWindow *window;
@property (strong) NSTextField *nome, *email, *resumo;
@end

@implementation FormDelegate
- (NSTextField *)rotulo:(NSString *)t {
    NSTextField *l = [NSTextField labelWithString:t];
    l.translatesAutoresizingMaskIntoConstraints = NO;
    l.alignment = NSTextAlignmentRight;
    return l;
}
- (NSTextField *)campo:(NSString *)ph {
    NSTextField *f = [NSTextField textFieldWithString:@""];
    f.placeholderString = ph;
    f.translatesAutoresizingMaskIntoConstraints = NO;
    return f;
}
- (void)applicationDidFinishLaunching:(NSNotification *)n {
    self.window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 480, 260)
                                              styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable
                                                backing:NSBackingStoreBuffered defer:NO];
    self.window.title = @"Formulário — Auto Layout";
    NSView *c = self.window.contentView;

    NSTextField *titulo = [NSTextField labelWithString:@"Cadastro"];
    titulo.font = [NSFont boldSystemFontOfSize:18];
    titulo.translatesAutoresizingMaskIntoConstraints = NO;
    NSTextField *lNome = [self rotulo:@"Nome:"], *lEmail = [self rotulo:@"E-mail:"];
    self.nome = [self campo:@"Seu nome"];
    self.email = [self campo:@"voce@exemplo.com"];
    NSButton *ok = [NSButton buttonWithTitle:@"Enviar" target:self action:@selector(enviar:)];
    ok.keyEquivalent = @"\r";
    ok.translatesAutoresizingMaskIntoConstraints = NO;
    NSButton *limpar = [NSButton buttonWithTitle:@"Limpar" target:self action:@selector(limpar:)];
    limpar.translatesAutoresizingMaskIntoConstraints = NO;
    self.resumo = [NSTextField labelWithString:@"Preencha e clique em Enviar."];
    self.resumo.translatesAutoresizingMaskIntoConstraints = NO;
    for (NSView *v in @[titulo, lNome, lEmail, self.nome, self.email, ok, limpar, self.resumo]) [c addSubview:v];

    // âncoras: título no topo, rótulos alinhados à direita numa coluna de 70pt
    [NSLayoutConstraint activateConstraints:@[
        [titulo.topAnchor constraintEqualToAnchor:c.topAnchor constant:20],
        [titulo.leadingAnchor constraintEqualToAnchor:c.leadingAnchor constant:20],
        [lNome.widthAnchor constraintEqualToConstant:70],
        [lEmail.widthAnchor constraintEqualToAnchor:lNome.widthAnchor],
        [lNome.centerYAnchor constraintEqualToAnchor:self.nome.centerYAnchor],
        [lEmail.centerYAnchor constraintEqualToAnchor:self.email.centerYAnchor],
        [self.nome.topAnchor constraintEqualToAnchor:titulo.bottomAnchor constant:16],
        [self.email.topAnchor constraintEqualToAnchor:self.nome.bottomAnchor constant:10],
        [self.nome.heightAnchor constraintEqualToConstant:24],
        [self.email.heightAnchor constraintEqualToAnchor:self.nome.heightAnchor],
        [ok.bottomAnchor constraintEqualToAnchor:c.bottomAnchor constant:-20],
        [limpar.centerYAnchor constraintEqualToAnchor:ok.centerYAnchor],
        [self.resumo.topAnchor constraintEqualToAnchor:self.email.bottomAnchor constant:16],
    ]];
    // VFL: linhas horizontais que esticam com a janela
    NSDictionary *views = NSDictionaryOfVariableBindings(lNome, lEmail, ok, limpar);
    NSMutableDictionary *vv = [views mutableCopy];
    vv[@"nome"] = self.nome; vv[@"email"] = self.email; vv[@"resumo"] = self.resumo;
    NSDictionary *m = @{@"margem": @20};
    for (NSString *f in @[@"H:|-margem-[lNome]-8-[nome(>=120)]-margem-|", @"H:|-margem-[lEmail]-8-[email]-margem-|",
                          @"H:[limpar]-12-[ok(==90)]-margem-|", @"H:|-margem-[resumo]-margem-|"])
        [NSLayoutConstraint activateConstraints:[NSLayoutConstraint constraintsWithVisualFormat:f options:0 metrics:m views:vv]];
    [self.window makeKeyAndOrderFront:nil];
    [c layoutSubtreeIfNeeded];
    NSLog(@"layout: nome %@  ok %@", NSStringFromRect(self.nome.frame), NSStringFromRect(ok.frame));
}
- (void)enviar:(id)s {
    self.resumo.stringValue = [NSString stringWithFormat:@"Enviado: %@ <%@>", self.nome.stringValue, self.email.stringValue];
    NSLog(@"%@ | nome %@", self.resumo.stringValue, NSStringFromRect(self.nome.frame));
}
- (void)limpar:(id)s { self.nome.stringValue = @""; self.email.stringValue = @""; self.resumo.stringValue = @"Limpo."; }
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)a { return YES; }
@end

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        FormDelegate *d = [FormDelegate new];
        app.delegate = d;
        [app run];
    }
    return 0;
}
