// Lista — app Cocoa de exemplo: menus na barra do sistema, NSTableView com rolagem, NSImage do bundle.
#import <Cocoa/Cocoa.h>

@interface ListaDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>
@property (strong) NSWindow *window;
@property (strong) NSTableView *tabela;
@property (strong) NSTextField *status;
@property (strong) NSMutableArray<NSString *> *itens;
@property NSInteger contador;
@end

@implementation ListaDelegate

- (NSMenu *)menuPrincipal {
    NSMenu *barra = [[NSMenu alloc] initWithTitle:@""];
    NSMenuItem *appItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
    NSMenu *appMenu = [[NSMenu alloc] initWithTitle:@"Lista"];
    [appMenu addItemWithTitle:@"Sobre a Lista" action:@selector(orderFrontStandardAboutPanel:) keyEquivalent:@""];
    [appMenu addItemWithTitle:@"Encerrar Lista" action:@selector(terminate:) keyEquivalent:@"q"];
    appItem.submenu = appMenu;
    [barra addItem:appItem];

    NSMenuItem *arqItem = [[NSMenuItem alloc] initWithTitle:@"Arquivo" action:nil keyEquivalent:@""];
    NSMenu *arq = [[NSMenu alloc] initWithTitle:@"Arquivo"];
    [arq addItemWithTitle:@"Novo item" action:@selector(novoItem:) keyEquivalent:@"n"];
    [arq addItemWithTitle:@"Remover selecionado" action:@selector(remover:) keyEquivalent:@"r"];
    [arq addItem:[NSMenuItem separatorItem]];
    [arq addItemWithTitle:@"Recarregar pasta" action:@selector(recarregar:) keyEquivalent:@"R"];
    arqItem.submenu = arq;
    [barra addItem:arqItem];

    NSMenuItem *exItem = [[NSMenuItem alloc] initWithTitle:@"Exibir" action:nil keyEquivalent:@""];
    NSMenu *ex = [[NSMenu alloc] initWithTitle:@"Exibir"];
    [ex addItemWithTitle:@"Linhas alternadas" action:@selector(alternar:) keyEquivalent:@"l"];
    exItem.submenu = ex;
    [barra addItem:exItem];
    return barra;
}

- (void)applicationDidFinishLaunching:(NSNotification *)n {
    NSApp.mainMenu = [self menuPrincipal];
    self.itens = [NSMutableArray array];
    [self recarregar:nil];

    self.window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 460, 380)
                                              styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable
                                                backing:NSBackingStoreBuffered defer:NO];
    self.window.title = @"Lista — arquivos da pasta pessoal";
    self.window.delegate = self;
    NSView *c = self.window.contentView;

    NSImage *icone = [NSImage imageNamed:@"icone"];
    NSImageView *iv = [[NSImageView alloc] initWithFrame:NSMakeRect(16, 316, 48, 48)];
    iv.image = icone;
    iv.autoresizingMask = NSViewMinYMargin;
    [c addSubview:iv];
    NSTextField *titulo = [NSTextField labelWithString:icone ? @"Arquivos (ícone carregado do bundle)" : @"Arquivos (sem ícone)"];
    titulo.font = [NSFont boldSystemFontOfSize:15];
    titulo.frame = NSMakeRect(74, 330, 370, 22);
    titulo.autoresizingMask = NSViewMinYMargin | NSViewWidthSizable;
    [c addSubview:titulo];

    NSScrollView *sv = [[NSScrollView alloc] initWithFrame:NSMakeRect(16, 44, 428, 260)];
    sv.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    self.tabela = [[NSTableView alloc] initWithFrame:sv.bounds];
    NSTableColumn *nome = [[NSTableColumn alloc] initWithIdentifier:@"nome"];
    nome.title = @"Nome";
    nome.width = 300;
    NSTableColumn *num = [[NSTableColumn alloc] initWithIdentifier:@"num"];
    num.title = @"Nº";
    num.width = 100;
    [self.tabela addTableColumn:nome];
    [self.tabela addTableColumn:num];
    self.tabela.dataSource = (id)self;
    self.tabela.delegate = (id)self;
    self.tabela.target = self;
    self.tabela.doubleAction = @selector(abrir:);
    sv.documentView = self.tabela;
    [c addSubview:sv];

    self.status = [NSTextField labelWithString:@"Use o menu Arquivo (⌘N, ⌘R) ou clique numa linha."];
    self.status.frame = NSMakeRect(16, 14, 428, 20);
    self.status.textColor = [NSColor secondaryLabelColor];
    [c addSubview:self.status];
    [self.window makeKeyAndOrderFront:nil];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)t { return self.itens.count; }
- (id)tableView:(NSTableView *)t objectValueForTableColumn:(NSTableColumn *)col row:(NSInteger)row {
    if ([col.identifier isEqualToString:@"num"]) return @(row + 1);
    return self.itens[row];
}
- (void)tableViewSelectionDidChange:(NSNotification *)n {
    NSInteger r = self.tabela.selectedRow;
    self.status.stringValue = r >= 0 ? [NSString stringWithFormat:@"Selecionado: %@", self.itens[r]] : @"Nada selecionado";
}
- (void)abrir:(id)sender { NSLog(@"duplo clique em %@", self.itens[self.tabela.clickedRow]); }
- (void)novoItem:(id)sender {
    self.contador++;
    [self.itens addObject:[NSString stringWithFormat:@"Item novo %ld", (long)self.contador]];
    [self.tabela reloadData];
    self.status.stringValue = [NSString stringWithFormat:@"%lu itens", (unsigned long)self.itens.count];
}
- (void)remover:(id)sender {
    NSInteger r = self.tabela.selectedRow;
    if (r < 0) { NSBeep(); return; }
    [self.itens removeObjectAtIndex:r];
    [self.tabela reloadData];
    self.status.stringValue = @"Removido";
}
- (void)recarregar:(id)sender {
    [self.itens removeAllObjects];
    NSArray *arqs = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:NSHomeDirectory() error:nil];
    for (NSString *a in arqs) if (![a hasPrefix:@"."]) [self.itens addObject:a];
    [self.tabela reloadData];
    NSLog(@"pasta recarregada: %lu itens", (unsigned long)self.itens.count);
}
- (void)alternar:(id)sender { self.status.stringValue = @"Linhas alternadas"; }
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)app { return YES; }
@end

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        ListaDelegate *d = [ListaDelegate new];
        app.delegate = d;
        [app run];
    }
    return 0;
}
