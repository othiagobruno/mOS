/*
 * mOS — QuickLookUI.framework (Mach-O, em /System/Library/Frameworks): a QLPreviewView que binários da Apple
 * referenciam. Sem geradores de prévia ainda: a view fica vazia.
 * tools/build-frameworks-objc.ps1
 */
#import <AppKit/AppKit.h>

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
