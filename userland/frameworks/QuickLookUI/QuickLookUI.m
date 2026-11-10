/*
 * mOS — QuickLookUI.framework (Mach-O, em /System/Library/Frameworks).
 * QLPreviewPanel e QLPreviewView. O painel não abre janela nem entra em loop:
 * makeKeyAndOrderFront: só marca visível. Sem este arquivo no disco, o libSystem
 * (quicklook_fw.c) atende as mesmas classes.
 * tools/build-frameworks-objc.ps1
 */
#import <AppKit/AppKit.h>

extern id objc_msgSend(id self, SEL op, ...);

@interface NSPanel : NSWindow
@end

@interface QLPreviewPanel : NSPanel
@property (assign) id dataSource;
@property (assign) id delegate;
@property NSInteger currentPreviewItemIndex;
@property (readonly) id currentPreviewItem;
@property (readonly, getter=isVisible) BOOL visible;
+ (instancetype)sharedPreviewPanel;
+ (BOOL)sharedPreviewPanelExists;
- (void)reloadData;
- (void)refreshCurrentPreviewItem;
- (void)updateController;
@end

static QLPreviewPanel *g_panel;
static BOOL g_exists, g_visible;
static id g_ds, g_del;
static NSInteger g_index;

@implementation QLPreviewPanel
+ (instancetype)sharedPreviewPanel { g_exists = YES; if (!g_panel) g_panel = [QLPreviewPanel new]; return g_panel; }
+ (BOOL)sharedPreviewPanelExists { return g_exists; }
- (BOOL)isVisible { return g_visible; }
- (void)makeKeyAndOrderFront:(id)sender { (void)sender; g_visible = YES; }
- (void)orderFront:(id)sender { (void)sender; g_visible = YES; }
- (void)orderOut:(id)sender { (void)sender; g_visible = NO; }
- (void)close { g_visible = NO; }
- (void)setDataSource:(id)ds { g_ds = ds; }
- (id)dataSource { return g_ds; }
- (void)setDelegate:(id)d { g_del = d; }
- (id)delegate { return g_del; }
- (void)setCurrentPreviewItemIndex:(NSInteger)i { g_index = i < 0 ? 0 : i; }
- (NSInteger)currentPreviewItemIndex { return g_index; }
- (id)currentPreviewItem {
    if (g_ds && [g_ds respondsToSelector:@selector(previewItemAtIndex:)])
        return ((id (*)(id, SEL, NSInteger))objc_msgSend)(g_ds, @selector(previewItemAtIndex:), g_index);
    return nil;
}
- (void)reloadData {
    NSInteger n = 0;
    if (g_ds && [g_ds respondsToSelector:@selector(numberOfPreviewItemsInPreviewPanel:)])
        n = ((NSInteger (*)(id, SEL, id))objc_msgSend)(g_ds, @selector(numberOfPreviewItemsInPreviewPanel:), self);
    if (n > 0 && g_ds && [g_ds respondsToSelector:@selector(previewItemAtIndex:)]) {
        id item = ((id (*)(id, SEL, NSInteger))objc_msgSend)(g_ds, @selector(previewItemAtIndex:), (NSInteger)0);
        if (item && [item respondsToSelector:@selector(previewItemURL)])
            (void)((id (*)(id, SEL))objc_msgSend)(item, @selector(previewItemURL));
    }
}
- (void)refreshCurrentPreviewItem { [self reloadData]; }
- (void)updateController {}
- (BOOL)enterFullScreenMode:(id)sender withOptions:(id)opts { (void)sender; (void)opts; return NO; }
- (void)exitFullScreenModeWithOptions:(id)opts { (void)opts; g_visible = NO; }
- (BOOL)isInFullScreenMode { return NO; }
@end

@interface QLPreviewView : NSView
@property (retain) id previewItem;
@property BOOL autostarts;
@property BOOL shouldCloseWithWindow;
- (instancetype)initWithFrame:(NSRect)frame style:(NSInteger)style;
- (void)refreshPreviewItem;
- (void)close;
@end

@implementation QLPreviewView
- (instancetype)initWithFrame:(NSRect)frame style:(NSInteger)style { return [super initWithFrame:frame]; }
- (void)refreshPreviewItem {}
- (void)close {}
@end
