// Entrada — app Cocoa de exemplo do método de entrada (japonês/chinês/coreano) e de arrastar e soltar
// entre janelas. ⌃Espaço troca a fonte de entrada; arraste o bloco azul da janela "Fonte" para o editor
// (ou para outro app) e solte texto/arquivos de outros apps no editor.
#import <Cocoa/Cocoa.h>
#include <stdio.h>
const char *sel_getName(SEL s);

// ---- partes do AppKit que o SDK enxuto do mOS não declara
typedef NSUInteger NSDragOperation;
enum { NSDragOperationNone = 0, NSDragOperationCopy = 1, NSDragOperationLink = 2, NSDragOperationGeneric = 4, NSDragOperationMove = 16 };
typedef NSInteger NSDraggingContext;
typedef NSString *NSPasteboardType;
extern NSPasteboardType const NSPasteboardTypeString;
@interface NSPasteboard : NSObject
- (NSString *)stringForType:(NSPasteboardType)t;
- (NSArray<NSPasteboardType> *)types;
@end
@interface NSPasteboardItem : NSObject
- (BOOL)setString:(NSString *)s forType:(NSPasteboardType)t;
@end
@interface NSDraggingItem : NSObject
- (instancetype)initWithPasteboardWriter:(id)w;
- (void)setDraggingFrame:(NSRect)f contents:(id)c;
@end
@interface NSDraggingSession : NSObject
@end
@protocol NSDraggingInfo <NSObject>
- (NSPasteboard *)draggingPasteboard;
- (NSPoint)draggingLocation;
- (NSDragOperation)draggingSourceOperationMask;
- (id)draggingSource;
@end
@interface NSTextInputContext : NSObject
- (BOOL)handleEvent:(NSEvent *)e;
- (NSString *)selectedKeyboardInputSource;
@end
@protocol NSTextInputClient
@end
@interface NSView (EntradaExtras)
- (void)registerForDraggedTypes:(NSArray<NSPasteboardType> *)t;
- (NSTextInputContext *)inputContext;
- (NSDraggingSession *)beginDraggingSessionWithItems:(NSArray *)items event:(NSEvent *)e source:(id)s;
- (NSRect)convertRect:(NSRect)r toView:(NSView *)v;
- (void)interpretKeyEvents:(NSArray<NSEvent *> *)events;
@end
@interface NSWindow (EntradaExtras)
- (NSRect)convertRectToScreen:(NSRect)r;
@end

// ---- editor: um NSTextInputClient de verdade, como o do Chromium, que também aceita soltar texto
@interface Editor : NSView <NSTextInputClient>
@property (strong) NSMutableString *text;
@property (copy) NSString *marked;
@end
@implementation Editor
- (instancetype)initWithFrame:(NSRect)r {
    self = [super initWithFrame:r];
    _text = [NSMutableString string];
    _marked = @"";
    [self registerForDraggedTypes:@[NSPasteboardTypeString]];
    return self;
}
- (BOOL)acceptsFirstResponder { return YES; }
- (BOOL)isFlipped { return YES; }
- (void)keyDown:(NSEvent *)e {
    NSTextInputContext *ctx = [self inputContext];
    if (ctx) [ctx handleEvent:e]; else [self interpretKeyEvents:@[e]];
}
static NSString *plain(id s) { return [s isKindOfClass:[NSString class]] ? s : [s string]; }
- (void)insertText:(id)s replacementRange:(NSRange)r {
    [_text appendString:plain(s)];
    _marked = @"";
    printf("[app] insertText \"%s\" -> \"%s\"\n", [plain(s) UTF8String], [_text UTF8String]);
    self.needsDisplay = YES;
}
- (void)setMarkedText:(id)s selectedRange:(NSRange)sel replacementRange:(NSRange)r {
    _marked = [plain(s) copy];
    printf("[app] setMarkedText \"%s\" sel=%lu\n", [_marked UTF8String], (unsigned long)sel.location);
    self.needsDisplay = YES;
}
- (void)unmarkText { [_text appendString:_marked]; _marked = @""; }
- (BOOL)hasMarkedText { return _marked.length > 0; }
- (NSRange)markedRange { return _marked.length ? NSMakeRange(_text.length, _marked.length) : NSMakeRange(NSNotFound, 0); }
- (NSRange)selectedRange { return NSMakeRange(_text.length + _marked.length, 0); }
- (NSArray *)validAttributesForMarkedText { return @[]; }
- (id)attributedSubstringForProposedRange:(NSRange)r actualRange:(NSRange *)a { return nil; }
- (NSUInteger)characterIndexForPoint:(NSPoint)p { return NSNotFound; }
- (NSRect)firstRectForCharacterRange:(NSRange)r actualRange:(NSRange *)a {
    NSSize sz = [_text sizeWithAttributes:@{NSFontAttributeName: [NSFont systemFontOfSize:16]}];
    NSRect local = NSMakeRect(12 + sz.width, 12, 20, 22);                 // logo depois do texto confirmado
    return [self.window convertRectToScreen:[self convertRect:local toView:nil]];
}
- (void)doCommandBySelector:(SEL)s {
    printf("[app] comando %s\n", sel_getName(s));
    if (s == @selector(deleteBackward:) && _text.length) [_text deleteCharactersInRange:NSMakeRange(_text.length - 1, 1)];
    if (s == @selector(insertNewline:)) [_text appendString:@"\n"];
    self.needsDisplay = YES;
}
- (void)drawRect:(NSRect)r {
    [[NSColor whiteColor] set];
    NSRectFill(self.bounds);
    NSDictionary *a = @{NSFontAttributeName: [NSFont systemFontOfSize:16]};
    [_text drawAtPoint:NSMakePoint(12, 12) withAttributes:a];
    if (_marked.length) {
        NSSize s = [_text sizeWithAttributes:a];
        [_marked drawAtPoint:NSMakePoint(12 + s.width, 12) withAttributes:a];
        NSSize m = [_marked sizeWithAttributes:a];
        [[NSColor blackColor] set];
        NSRectFill(NSMakeRect(12 + s.width, 12 + 20, m.width, 1));       // sublinhado da composição
    }
}
// destino do arrasto
- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)info {
    printf("[app] arrasto entrou (máscara %lu, fonte %s)\n", (unsigned long)[info draggingSourceOperationMask], [info draggingSource] ? "deste app" : "de fora");
    return NSDragOperationCopy;
}
- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)info { return NSDragOperationCopy; }
- (void)draggingExited:(id<NSDraggingInfo>)info { printf("[app] arrasto saiu\n"); }
- (BOOL)performDragOperation:(id<NSDraggingInfo>)info {
    NSString *s = [[info draggingPasteboard] stringForType:NSPasteboardTypeString];
    NSPoint p = [info draggingLocation];
    printf("[app] soltou \"%s\" em (%.0f, %.0f)\n", s ? [s UTF8String] : "(nada)", p.x, p.y);
    if (s) { [_text appendString:s]; self.needsDisplay = YES; }
    return s != nil;
}
@end

// ---- fonte do arrasto
@interface Fonte : NSView
@end
@implementation Fonte
- (void)drawRect:(NSRect)r { [[NSColor systemBlueColor] set]; NSRectFill(NSInsetRect(self.bounds, 20, 20)); }
- (void)mouseDragged:(NSEvent *)e {
    NSPasteboardItem *it = [NSPasteboardItem new];
    [it setString:@"Olá, 世界" forType:NSPasteboardTypeString];
    NSDraggingItem *di = [[NSDraggingItem alloc] initWithPasteboardWriter:it];
    [di setDraggingFrame:self.bounds contents:nil];
    [self beginDraggingSessionWithItems:@[di] event:e source:self];
}
- (NSDragOperation)draggingSession:(NSDraggingSession *)s sourceOperationMaskForDraggingContext:(NSDraggingContext)c {
    return NSDragOperationCopy | NSDragOperationMove;
}
- (void)draggingSession:(NSDraggingSession *)s willBeginAtPoint:(NSPoint)p { printf("[app] arrasto começou\n"); }
- (void)draggingSession:(NSDraggingSession *)s endedAtPoint:(NSPoint)p operation:(NSDragOperation)op {
    printf("[app] arrasto terminou: operação %lu\n", (unsigned long)op);
}
@end

@interface AppDelegate : NSObject <NSApplicationDelegate>
@property (strong) NSWindow *editor, *fonte;
@property (strong) Editor *box;
@end
@implementation AppDelegate
- (void)applicationDidFinishLaunching:(NSNotification *)n {
    self.editor = [[NSWindow alloc] initWithContentRect:NSMakeRect(80, 300, 520, 260)
                                              styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable
                                                backing:NSBackingStoreBuffered defer:NO];
    self.editor.title = @"Entrada — editor";
    self.box = [[Editor alloc] initWithFrame:NSMakeRect(0, 60, 520, 200)];
    [self.editor.contentView addSubview:self.box];
    NSTextField *campo = [NSTextField textFieldWithString:@""];          // campo nativo do AppKit do mOS
    campo.frame = NSMakeRect(12, 14, 496, 26);
    campo.placeholderString = @"Campo nativo";
    [self.editor.contentView addSubview:campo];
    [self.editor makeKeyAndOrderFront:nil];
    [self.editor makeFirstResponder:self.box];

    self.fonte = [[NSWindow alloc] initWithContentRect:NSMakeRect(640, 300, 220, 220)
                                             styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                                               backing:NSBackingStoreBuffered defer:NO];
    self.fonte.title = @"Fonte";
    self.fonte.contentView = [[Fonte alloc] initWithFrame:NSMakeRect(0, 0, 220, 220)];
    [self.fonte orderFront:nil];
    printf("[app] fonte de entrada: %s\n", [[[self.box inputContext] selectedKeyboardInputSource] UTF8String]);
}
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)a { return YES; }
@end

int main(int argc, const char **argv) {
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        AppDelegate *d = [AppDelegate new];
        app.delegate = d;
        [app run];
    }
    return 0;
}
