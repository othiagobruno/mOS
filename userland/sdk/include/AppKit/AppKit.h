/* mOS SDK — AppKit (subconjunto da API da Apple implementado pelo libSystem do mOS) */
#pragma once
#import <Foundation/Foundation.h>

@class NSWindow, NSView, NSEvent, NSColor, NSFont, NSMenu, NSScreen, NSImage, NSTableView, NSTableColumn;

typedef NS_ENUM(NSInteger, NSApplicationActivationPolicy) {
    NSApplicationActivationPolicyRegular = 0, NSApplicationActivationPolicyAccessory = 1,
    NSApplicationActivationPolicyProhibited = 2
};
typedef NS_OPTIONS(NSUInteger, NSWindowStyleMask) {
    NSWindowStyleMaskBorderless = 0, NSWindowStyleMaskTitled = 1, NSWindowStyleMaskClosable = 2,
    NSWindowStyleMaskMiniaturizable = 4, NSWindowStyleMaskResizable = 8
};
typedef NS_ENUM(NSUInteger, NSBackingStoreType) { NSBackingStoreBuffered = 2 };
typedef NS_OPTIONS(NSUInteger, NSAutoresizingMaskOptions) {
    NSViewNotSizable = 0, NSViewMinXMargin = 1, NSViewWidthSizable = 2, NSViewMaxXMargin = 4,
    NSViewMinYMargin = 8, NSViewHeightSizable = 16, NSViewMaxYMargin = 32
};
typedef NS_ENUM(NSUInteger, NSButtonType) { NSButtonTypeMomentaryPushIn = 7, NSButtonTypeSwitch = 3, NSButtonTypeRadio = 4 };
typedef NS_ENUM(NSInteger, NSControlStateValue) { NSControlStateValueOff = 0, NSControlStateValueOn = 1 };
#define NSOnState NSControlStateValueOn
#define NSOffState NSControlStateValueOff
typedef NS_ENUM(NSInteger, NSTextAlignment) { NSTextAlignmentLeft = 0, NSTextAlignmentRight = 1, NSTextAlignmentCenter = 2 };
typedef NS_ENUM(NSInteger, NSUserInterfaceLayoutOrientation) { NSUserInterfaceLayoutOrientationHorizontal = 0, NSUserInterfaceLayoutOrientationVertical = 1 };
typedef NS_ENUM(NSUInteger, NSEventType) {
    NSEventTypeLeftMouseDown = 1, NSEventTypeLeftMouseUp = 2, NSEventTypeRightMouseDown = 3,
    NSEventTypeLeftMouseDragged = 6, NSEventTypeKeyDown = 10, NSEventTypeKeyUp = 11,
    NSEventTypeFlagsChanged = 12, NSEventTypeScrollWheel = 22
};
typedef NS_OPTIONS(NSUInteger, NSEventModifierFlags) {
    NSEventModifierFlagShift = 1 << 17, NSEventModifierFlagControl = 1 << 18,
    NSEventModifierFlagOption = 1 << 19, NSEventModifierFlagCommand = 1 << 20
};
typedef NS_ENUM(NSInteger, NSModalResponse) { NSAlertFirstButtonReturn = 1000, NSAlertSecondButtonReturn = 1001, NSAlertThirdButtonReturn = 1002 };
typedef CGFloat NSFontWeight;
extern const NSFontWeight NSFontWeightRegular, NSFontWeightMedium, NSFontWeightSemibold, NSFontWeightBold, NSFontWeightLight;
typedef NSString *NSAttributedStringKey;
extern NSAttributedStringKey const NSFontAttributeName;
extern NSAttributedStringKey const NSForegroundColorAttributeName;
extern NSString *const NSApplicationDidFinishLaunchingNotification;
extern NSString *const NSWindowWillCloseNotification;

@interface NSResponder : NSObject
- (void)mouseDown:(NSEvent *)e;
- (void)mouseUp:(NSEvent *)e;
- (void)mouseDragged:(NSEvent *)e;
- (void)rightMouseDown:(NSEvent *)e;
- (void)scrollWheel:(NSEvent *)e;
- (void)keyDown:(NSEvent *)e;
- (void)keyUp:(NSEvent *)e;
- (void)flagsChanged:(NSEvent *)e;
@property (readonly) BOOL acceptsFirstResponder;
@property (weak) NSResponder *nextResponder;
@end

@protocol NSApplicationDelegate <NSObject>
@optional
- (void)applicationWillFinishLaunching:(NSNotification *)n;
- (void)applicationDidFinishLaunching:(NSNotification *)n;
- (void)applicationWillTerminate:(NSNotification *)n;
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(id)app;
@end

@protocol NSWindowDelegate <NSObject>
@optional
- (BOOL)windowShouldClose:(NSWindow *)w;
- (void)windowWillClose:(NSNotification *)n;
- (void)windowDidResize:(NSNotification *)n;
@end

@interface NSApplication : NSResponder
+ (NSApplication *)sharedApplication;
@property (weak) id<NSApplicationDelegate> delegate;
@property (strong) NSMenu *mainMenu;
@property (readonly, copy) NSArray<NSWindow *> *windows;
@property (readonly) NSWindow *keyWindow;
- (BOOL)setActivationPolicy:(NSApplicationActivationPolicy)p;
@property (readonly) NSApplicationActivationPolicy activationPolicy;
- (void)activateIgnoringOtherApps:(BOOL)b;
- (void)run;
- (void)stop:(id)sender;
- (void)terminate:(id)sender;
- (NSModalResponse)runModalForWindow:(NSWindow *)w;
- (void)stopModalWithCode:(NSModalResponse)c;
- (void)orderFrontStandardAboutPanel:(id)sender;
- (void)hide:(id)sender;
@end
extern NSApplication *NSApp;
int NSApplicationMain(int argc, const char *argv[]);

@interface NSWindow : NSResponder
- (instancetype)initWithContentRect:(NSRect)r styleMask:(NSWindowStyleMask)s backing:(NSBackingStoreType)b defer:(BOOL)d;
@property (copy) NSString *title;
@property (strong) __kindof NSView *contentView;
@property (weak) id<NSWindowDelegate> delegate;
@property (strong) NSColor *backgroundColor;
@property (readonly) NSRect frame;
@property (readonly, getter=isVisible) BOOL visible;
@property BOOL releasedWhenClosed;
@property NSSize minSize;
@property (readonly) NSView *firstResponder;
- (void)makeKeyAndOrderFront:(id)sender;
- (void)orderFront:(id)sender;
- (void)center;
- (void)close;
- (void)performClose:(id)sender;
- (BOOL)makeFirstResponder:(NSResponder *)r;
- (void)setFrame:(NSRect)r display:(BOOL)d;
- (void)setContentSize:(NSSize)s;
@end

@class NSLayoutXAxisAnchor, NSLayoutYAxisAnchor, NSLayoutDimension, NSLayoutConstraint;
@interface NSView : NSResponder
- (instancetype)initWithFrame:(NSRect)f;
@property NSRect frame;
@property (readonly) NSRect bounds;
@property (readonly) NSView *superview;
@property (copy) NSString *identifier;
@property (readonly, copy) NSArray<__kindof NSView *> *subviews;
@property (readonly) NSWindow *window;
@property (getter=isHidden) BOOL hidden;
@property BOOL needsDisplay;
@property NSAutoresizingMaskOptions autoresizingMask;
@property NSInteger tag;
@property (copy) NSString *toolTip;
@property BOOL wantsLayer;
@property (readonly, getter=isFlipped) BOOL flipped;
@property (readonly) NSSize intrinsicContentSize;
- (void)addSubview:(NSView *)v;
- (void)removeFromSuperview;
- (void)drawRect:(NSRect)dirty;
- (void)setFrameSize:(NSSize)s;
- (void)setFrameOrigin:(NSPoint)p;
- (NSPoint)convertPoint:(NSPoint)p fromView:(NSView *)v;
- (NSPoint)convertPoint:(NSPoint)p toView:(NSView *)v;
- (void)sizeToFit;
- (__kindof NSView *)viewWithTag:(NSInteger)t;
@property BOOL translatesAutoresizingMaskIntoConstraints;
@property (readonly, strong) NSLayoutXAxisAnchor *leadingAnchor, *trailingAnchor, *leftAnchor, *rightAnchor, *centerXAnchor;
@property (readonly, strong) NSLayoutYAxisAnchor *topAnchor, *bottomAnchor, *centerYAnchor, *firstBaselineAnchor, *lastBaselineAnchor;
@property (readonly, strong) NSLayoutDimension *widthAnchor, *heightAnchor;
@property (readonly, copy) NSArray<NSLayoutConstraint *> *constraints;
@property (readonly) NSSize fittingSize;
- (void)addConstraint:(NSLayoutConstraint *)c;
- (void)addConstraints:(NSArray<NSLayoutConstraint *> *)c;
- (void)removeConstraint:(NSLayoutConstraint *)c;
- (void)removeConstraints:(NSArray<NSLayoutConstraint *> *)c;
- (void)layoutSubtreeIfNeeded;
- (void)layout;
- (void)setContentHuggingPriority:(float)p forOrientation:(NSInteger)o;
- (void)setContentCompressionResistancePriority:(float)p forOrientation:(NSInteger)o;
@end

@interface NSControl : NSView
@property (weak) id target;
@property SEL action;
@property (getter=isEnabled) BOOL enabled;
@property (strong) NSFont *font;
@property NSTextAlignment alignment;
@property (copy) NSString *stringValue;
@property int intValue;
@property NSInteger integerValue;
@property double doubleValue;
@property float floatValue;
- (BOOL)sendAction:(SEL)a to:(id)t;
@end

@interface NSButton : NSControl
+ (instancetype)buttonWithTitle:(NSString *)t target:(id)target action:(SEL)a;
+ (instancetype)checkboxWithTitle:(NSString *)t target:(id)target action:(SEL)a;
+ (instancetype)radioButtonWithTitle:(NSString *)t target:(id)target action:(SEL)a;
@property (copy) NSString *title;
@property NSControlStateValue state;
@property (copy) NSString *keyEquivalent;
@property (getter=isBordered) BOOL bordered;
@property (strong) NSImage *image;
- (void)setButtonType:(NSButtonType)t;
- (void)setBezelStyle:(NSUInteger)s;
@end

@protocol NSTextFieldDelegate <NSObject>
@optional
- (void)controlTextDidChange:(NSNotification *)n;
@end

@interface NSTextField : NSControl
+ (instancetype)labelWithString:(NSString *)s;
+ (instancetype)wrappingLabelWithString:(NSString *)s;
+ (instancetype)textFieldWithString:(NSString *)s;
@property (getter=isEditable) BOOL editable;
@property (getter=isSelectable) BOOL selectable;
@property (getter=isBezeled) BOOL bezeled;
@property BOOL drawsBackground;
@property (strong) NSColor *textColor;
@property (strong) NSColor *backgroundColor;
@property (copy) NSString *placeholderString;
@property (weak) id<NSTextFieldDelegate> delegate;
@end

@interface NSSlider : NSControl
+ (instancetype)sliderWithValue:(double)v minValue:(double)mn maxValue:(double)mx target:(id)t action:(SEL)a;
@property double minValue;
@property double maxValue;
@end

@interface NSProgressIndicator : NSView
@property double doubleValue;
@property double minValue;
@property double maxValue;
@property (getter=isIndeterminate) BOOL indeterminate;
- (void)incrementBy:(double)d;
@end

@interface NSBox : NSView
@property (copy) NSString *title;
@end

@interface NSStackView : NSView
+ (instancetype)stackViewWithViews:(NSArray<NSView *> *)views;
@property NSUserInterfaceLayoutOrientation orientation;
@property CGFloat spacing;
- (void)addArrangedSubview:(NSView *)v;
@end

@interface NSColor : NSObject
+ (NSColor *)colorWithRed:(CGFloat)r green:(CGFloat)g blue:(CGFloat)b alpha:(CGFloat)a;
+ (NSColor *)colorWithCalibratedRed:(CGFloat)r green:(CGFloat)g blue:(CGFloat)b alpha:(CGFloat)a;
+ (NSColor *)colorWithSRGBRed:(CGFloat)r green:(CGFloat)g blue:(CGFloat)b alpha:(CGFloat)a;
+ (NSColor *)colorWithWhite:(CGFloat)w alpha:(CGFloat)a;
+ (NSColor *)colorWithHue:(CGFloat)h saturation:(CGFloat)s brightness:(CGFloat)b alpha:(CGFloat)a;
@property (class, readonly) NSColor *blackColor, *whiteColor, *redColor, *greenColor, *blueColor, *yellowColor, *orangeColor,
    *purpleColor, *grayColor, *lightGrayColor, *darkGrayColor, *clearColor, *systemBlueColor, *systemRedColor, *systemGreenColor,
    *systemOrangeColor, *systemPurpleColor, *systemPinkColor, *systemTealColor, *systemYellowColor, *controlAccentColor,
    *labelColor, *secondaryLabelColor, *windowBackgroundColor, *controlBackgroundColor, *separatorColor;
- (void)set;
- (void)setFill;
- (void)setStroke;
- (NSColor *)colorWithAlphaComponent:(CGFloat)a;
- (NSColor *)blendedColorWithFraction:(CGFloat)f ofColor:(NSColor *)c;
@property (readonly) CGFloat redComponent, greenComponent, blueComponent, alphaComponent;
@end

@interface NSFont : NSObject
+ (NSFont *)systemFontOfSize:(CGFloat)s;
+ (NSFont *)boldSystemFontOfSize:(CGFloat)s;
+ (NSFont *)systemFontOfSize:(CGFloat)s weight:(NSFontWeight)w;
+ (NSFont *)monospacedSystemFontOfSize:(CGFloat)s weight:(NSFontWeight)w;
+ (NSFont *)monospacedDigitSystemFontOfSize:(CGFloat)s weight:(NSFontWeight)w;
+ (NSFont *)fontWithName:(NSString *)n size:(CGFloat)s;
@property (readonly) CGFloat pointSize;
@property (class, readonly) CGFloat systemFontSize;
@end

@interface NSBezierPath : NSObject
+ (NSBezierPath *)bezierPath;
+ (NSBezierPath *)bezierPathWithRect:(NSRect)r;
+ (NSBezierPath *)bezierPathWithOvalInRect:(NSRect)r;
+ (NSBezierPath *)bezierPathWithRoundedRect:(NSRect)r xRadius:(CGFloat)x yRadius:(CGFloat)y;
+ (void)fillRect:(NSRect)r;
+ (void)strokeRect:(NSRect)r;
+ (void)strokeLineFromPoint:(NSPoint)a toPoint:(NSPoint)b;
@property CGFloat lineWidth;
@property (readonly) NSRect bounds;
- (void)moveToPoint:(NSPoint)p;
- (void)lineToPoint:(NSPoint)p;
- (void)curveToPoint:(NSPoint)p controlPoint1:(NSPoint)a controlPoint2:(NSPoint)b;
- (void)closePath;
- (void)appendBezierPathWithRect:(NSRect)r;
- (void)appendBezierPathWithOvalInRect:(NSRect)r;
- (void)fill;
- (void)stroke;
- (void)removeAllPoints;
@end

@interface NSString (NSStringDrawing)
- (void)drawAtPoint:(NSPoint)p withAttributes:(NSDictionary<NSAttributedStringKey, id> *)a;
- (void)drawInRect:(NSRect)r withAttributes:(NSDictionary<NSAttributedStringKey, id> *)a;
- (NSSize)sizeWithAttributes:(NSDictionary<NSAttributedStringKey, id> *)a;
@end

@interface NSEvent : NSObject
@property (readonly) NSEventType type;
@property (readonly) NSPoint locationInWindow;
@property (readonly) NSEventModifierFlags modifierFlags;
@property (readonly) unsigned short keyCode;
@property (readonly, copy) NSString *characters;
@property (readonly) NSInteger clickCount;
@property (readonly) CGFloat scrollingDeltaY;
@property (readonly) NSWindow *window;
@end

@interface NSMenuItem : NSObject
+ (NSMenuItem *)separatorItem;
- (instancetype)initWithTitle:(NSString *)t action:(SEL)a keyEquivalent:(NSString *)k;
@property (strong) NSMenu *submenu;
@property (weak) id target;
@property SEL action;
@property (copy) NSString *title;
@property (copy) NSString *keyEquivalent;
@property NSEventModifierFlags keyEquivalentModifierMask;
@property (copy) NSString *userKeyEquivalent;
@property NSEventModifierFlags userKeyEquivalentModifierMask;
@property NSInteger tag;
@property NSControlStateValue state;
@property (getter=isEnabled) BOOL enabled;
@end

@interface NSMenu : NSObject
- (instancetype)initWithTitle:(NSString *)t;
- (void)addItem:(NSMenuItem *)i;
- (NSMenuItem *)addItemWithTitle:(NSString *)t action:(SEL)a keyEquivalent:(NSString *)k;
@property (readonly, copy) NSArray<NSMenuItem *> *itemArray;
@property BOOL autoenablesItems;
- (void)removeAllItems;
@end

typedef NSString *NSImageName;
@interface NSImage : NSObject
+ (NSImage *)imageNamed:(NSImageName)n;
- (instancetype)initWithContentsOfFile:(NSString *)p;
- (instancetype)initWithData:(NSData *)d;
- (instancetype)initWithSize:(NSSize)s;
@property NSSize size;
@property (readonly, getter=isValid) BOOL valid;
@property BOOL template;
- (void)drawInRect:(NSRect)r;
- (void)drawInRect:(NSRect)r fromRect:(NSRect)f operation:(NSUInteger)op fraction:(CGFloat)a;
- (void)drawAtPoint:(NSPoint)p fromRect:(NSRect)f operation:(NSUInteger)op fraction:(CGFloat)a;
@end

@interface NSImageView : NSView
+ (instancetype)imageViewWithImage:(NSImage *)i;
@property (strong) NSImage *image;
@property NSUInteger imageScaling;
@end

@interface NSClipView : NSView
@end

typedef NS_ENUM(NSUInteger, NSBorderType) { NSNoBorder = 0, NSLineBorder = 1, NSBezelBorder = 2 };
@interface NSScrollView : NSView
@property (strong) __kindof NSView *documentView;
@property (readonly) NSClipView *contentView;
@property BOOL hasVerticalScroller, hasHorizontalScroller, autohidesScrollers, drawsBackground;
@property NSBorderType borderType;
@end

@interface NSTableColumn : NSObject
- (instancetype)initWithIdentifier:(NSString *)i;
@property (readonly, copy) NSString *identifier;
@property (copy) NSString *title;
@property CGFloat width, minWidth;
@property (getter=isEditable) BOOL editable;
@end

@interface NSTableCellView : NSView
@property (strong) NSTextField *textField;
@end

@protocol NSTableViewDataSource <NSObject>
@optional
- (NSInteger)numberOfRowsInTableView:(NSTableView *)t;
- (id)tableView:(NSTableView *)t objectValueForTableColumn:(NSTableColumn *)c row:(NSInteger)r;
@end
@protocol NSTableViewDelegate <NSObject>
@optional
- (NSView *)tableView:(NSTableView *)t viewForTableColumn:(NSTableColumn *)c row:(NSInteger)r;
- (void)tableViewSelectionDidChange:(NSNotification *)n;
- (BOOL)tableView:(NSTableView *)t shouldSelectRow:(NSInteger)r;
@end

@interface NSTableView : NSControl
- (void)addTableColumn:(NSTableColumn *)c;
@property (readonly, copy) NSArray<NSTableColumn *> *tableColumns;
@property (weak) id<NSTableViewDataSource> dataSource;
@property (weak) id<NSTableViewDelegate> delegate;
@property (readonly) NSInteger numberOfRows, selectedRow, clickedRow;
@property CGFloat rowHeight;
@property (strong) id headerView;
@property BOOL usesAlternatingRowBackgroundColors, allowsMultipleSelection;
@property SEL doubleAction;
@property NSInteger style;
@property NSUInteger gridStyleMask;
- (void)reloadData;
- (__kindof NSView *)makeViewWithIdentifier:(NSString *)i owner:(id)o;
- (void)registerNib:(id)nib forIdentifier:(NSString *)identifier;
- (void)scrollRowToVisible:(NSInteger)r;
- (void)selectRowIndexes:(NSIndexSet *)s byExtendingSelection:(BOOL)e;
- (void)deselectAll:(id)sender;
- (void)sizeLastColumnToFit;
@end

@interface NSOutlineView : NSTableView
@property CGFloat indentationPerLevel;
@property (strong) NSTableColumn *outlineTableColumn;
- (void)expandItem:(id)item;
- (void)expandItem:(id)item expandChildren:(BOOL)children;
- (void)collapseItem:(id)item;
- (void)collapseItem:(id)item collapseChildren:(BOOL)children;
- (BOOL)isItemExpanded:(id)item;
- (BOOL)isExpandable:(id)item;
- (id)itemAtRow:(NSInteger)row;
- (NSInteger)rowForItem:(id)item;
- (NSInteger)levelForRow:(NSInteger)row;
@end

@interface NSAlert : NSObject
@property (copy) NSString *messageText;
@property (copy) NSString *informativeText;
- (id)addButtonWithTitle:(NSString *)t;
- (NSModalResponse)runModal;
@end

@interface NSScreen : NSObject
+ (NSScreen *)mainScreen;
@property (readonly) NSRect frame;
@property (readonly) NSRect visibleFrame;
@end

@interface NSGraphicsContext : NSObject
+ (NSGraphicsContext *)currentContext;
+ (void)saveGraphicsState;
+ (void)restoreGraphicsState;
@property (readonly) void *CGContext;
@end

void NSRectFill(NSRect r);
void NSFrameRect(NSRect r);
void NSFrameRectWithWidth(NSRect r, CGFloat w);
void NSBeep(void);

/* ---- Auto Layout */
typedef NS_ENUM(NSInteger, NSLayoutAttribute) {
    NSLayoutAttributeLeft = 1, NSLayoutAttributeRight, NSLayoutAttributeTop, NSLayoutAttributeBottom, NSLayoutAttributeLeading,
    NSLayoutAttributeTrailing, NSLayoutAttributeWidth, NSLayoutAttributeHeight, NSLayoutAttributeCenterX, NSLayoutAttributeCenterY,
    NSLayoutAttributeLastBaseline, NSLayoutAttributeBaseline = NSLayoutAttributeLastBaseline, NSLayoutAttributeFirstBaseline,
    NSLayoutAttributeNotAnAttribute = 0
};
typedef NS_ENUM(NSInteger, NSLayoutRelation) { NSLayoutRelationLessThanOrEqual = -1, NSLayoutRelationEqual = 0, NSLayoutRelationGreaterThanOrEqual = 1 };
typedef float NSLayoutPriority;
static const NSLayoutPriority NSLayoutPriorityRequired = 1000, NSLayoutPriorityDefaultHigh = 750, NSLayoutPriorityDefaultLow = 250,
    NSLayoutPriorityFittingSizeCompression = 50, NSLayoutPriorityDragThatCanResizeWindow = 510, NSLayoutPriorityWindowSizeStayPut = 500;
typedef NS_ENUM(NSInteger, NSLayoutConstraintOrientation) { NSLayoutConstraintOrientationHorizontal = 0, NSLayoutConstraintOrientationVertical = 1 };
typedef NSUInteger NSLayoutFormatOptions;

@interface NSLayoutConstraint : NSObject
+ (instancetype)constraintWithItem:(id)a attribute:(NSLayoutAttribute)aa relatedBy:(NSLayoutRelation)r toItem:(id)b attribute:(NSLayoutAttribute)ba multiplier:(CGFloat)m constant:(CGFloat)k;
+ (NSArray<NSLayoutConstraint *> *)constraintsWithVisualFormat:(NSString *)f options:(NSLayoutFormatOptions)o metrics:(NSDictionary<NSString *, NSNumber *> *)m views:(NSDictionary<NSString *, id> *)v;
+ (void)activateConstraints:(NSArray<NSLayoutConstraint *> *)c;
+ (void)deactivateConstraints:(NSArray<NSLayoutConstraint *> *)c;
@property (getter=isActive) BOOL active;
@property CGFloat constant;
@property NSLayoutPriority priority;
@property (readonly) CGFloat multiplier;
@property (readonly, weak) id firstItem, secondItem;
@property (readonly) NSLayoutAttribute firstAttribute, secondAttribute;
@property (readonly) NSLayoutRelation relation;
@property (copy) NSString *identifier;
@end

@interface NSLayoutAnchor<AnchorType> : NSObject
- (NSLayoutConstraint *)constraintEqualToAnchor:(NSLayoutAnchor<AnchorType> *)a;
- (NSLayoutConstraint *)constraintEqualToAnchor:(NSLayoutAnchor<AnchorType> *)a constant:(CGFloat)k;
- (NSLayoutConstraint *)constraintGreaterThanOrEqualToAnchor:(NSLayoutAnchor<AnchorType> *)a;
- (NSLayoutConstraint *)constraintGreaterThanOrEqualToAnchor:(NSLayoutAnchor<AnchorType> *)a constant:(CGFloat)k;
- (NSLayoutConstraint *)constraintLessThanOrEqualToAnchor:(NSLayoutAnchor<AnchorType> *)a;
- (NSLayoutConstraint *)constraintLessThanOrEqualToAnchor:(NSLayoutAnchor<AnchorType> *)a constant:(CGFloat)k;
@end
@interface NSLayoutXAxisAnchor : NSLayoutAnchor<NSLayoutXAxisAnchor *>
- (NSLayoutConstraint *)constraintEqualToSystemSpacingAfterAnchor:(NSLayoutXAxisAnchor *)a multiplier:(CGFloat)m;
- (NSLayoutConstraint *)constraintGreaterThanOrEqualToSystemSpacingAfterAnchor:(NSLayoutXAxisAnchor *)a multiplier:(CGFloat)m;
@end
@interface NSLayoutYAxisAnchor : NSLayoutAnchor<NSLayoutYAxisAnchor *>
- (NSLayoutConstraint *)constraintEqualToSystemSpacingBelowAnchor:(NSLayoutYAxisAnchor *)a multiplier:(CGFloat)m;
- (NSLayoutConstraint *)constraintGreaterThanOrEqualToSystemSpacingBelowAnchor:(NSLayoutYAxisAnchor *)a multiplier:(CGFloat)m;
@end
@interface NSLayoutDimension : NSLayoutAnchor<NSLayoutDimension *>
- (NSLayoutConstraint *)constraintEqualToConstant:(CGFloat)k;
- (NSLayoutConstraint *)constraintGreaterThanOrEqualToConstant:(CGFloat)k;
- (NSLayoutConstraint *)constraintLessThanOrEqualToConstant:(CGFloat)k;
- (NSLayoutConstraint *)constraintEqualToAnchor:(NSLayoutDimension *)a multiplier:(CGFloat)m;
- (NSLayoutConstraint *)constraintEqualToAnchor:(NSLayoutDimension *)a multiplier:(CGFloat)m constant:(CGFloat)k;
- (NSLayoutConstraint *)constraintGreaterThanOrEqualToAnchor:(NSLayoutDimension *)a multiplier:(CGFloat)m;
- (NSLayoutConstraint *)constraintGreaterThanOrEqualToAnchor:(NSLayoutDimension *)a multiplier:(CGFloat)m constant:(CGFloat)k;
- (NSLayoutConstraint *)constraintLessThanOrEqualToAnchor:(NSLayoutDimension *)a multiplier:(CGFloat)m;
- (NSLayoutConstraint *)constraintLessThanOrEqualToAnchor:(NSLayoutDimension *)a multiplier:(CGFloat)m constant:(CGFloat)k;
@end
#define NSDictionaryOfVariableBindings(...) _NSDictionaryOfVariableBindings(@"" # __VA_ARGS__, __VA_ARGS__, nil)
NSDictionary *_NSDictionaryOfVariableBindings(NSString *names, id first, ...);
/* ---- nibs, storyboards e controllers */
typedef NSString *NSNibName;
@interface NSBundle (NSNibLoading)
- (BOOL)loadNibNamed:(NSNibName)n owner:(id)o topLevelObjects:(NSArray **)top;
+ (BOOL)loadNibNamed:(NSString *)n owner:(id)o;
@end
@interface NSNib : NSObject
- (instancetype)initWithNibNamed:(NSNibName)n bundle:(NSBundle *)b;
- (BOOL)instantiateWithOwner:(id)o topLevelObjects:(NSArray **)top;
@end
@interface NSObject (NSNibAwaking)
- (void)awakeFromNib;
@end
@class NSStoryboard;
@interface NSViewController : NSResponder
- (instancetype)initWithNibName:(NSNibName)n bundle:(NSBundle *)b;
- (instancetype)initWithCoder:(NSCoder *)c;
@property (strong) IBOutlet NSView *view;
@property (copy) NSString *title;
@property (strong) id representedObject;
@property (readonly, copy) NSNibName nibName;
@property (readonly, getter=isViewLoaded) BOOL viewLoaded;
@property (readonly, strong) NSStoryboard *storyboard;
- (void)loadView;
- (void)viewDidLoad;
- (void)viewWillAppear;
- (void)viewDidAppear;
@end
@interface NSWindowController : NSResponder
- (instancetype)initWithWindow:(NSWindow *)w;
- (instancetype)initWithWindowNibName:(NSNibName)n;
- (instancetype)initWithWindowNibName:(NSNibName)n owner:(id)o;
- (instancetype)initWithCoder:(NSCoder *)c;
@property (strong) IBOutlet NSWindow *window;
@property (strong) NSViewController *contentViewController;
@property (readonly, copy) NSNibName windowNibName;
@property (readonly, getter=isWindowLoaded) BOOL windowLoaded;
- (void)windowDidLoad;
- (void)windowWillLoad;
- (IBAction)showWindow:(id)sender;
- (void)close;
@end
@interface NSWindow (NSViewController)
@property (strong) NSViewController *contentViewController;
@end
typedef NSString *NSStoryboardName;
typedef NSString *NSStoryboardSceneIdentifier;
@interface NSStoryboard : NSObject
+ (instancetype)storyboardWithName:(NSStoryboardName)n bundle:(NSBundle *)b;
@property (class, readonly, strong) NSStoryboard *mainStoryboard;
- (id)instantiateInitialController;
- (id)instantiateControllerWithIdentifier:(NSStoryboardSceneIdentifier)i;
@end
/* ---- texto multilinha */
@protocol NSTextViewDelegate <NSObject>
@optional
- (void)textDidChange:(NSNotification *)n;
@end
@interface NSTextView : NSView
+ (NSScrollView *)scrollableTextView;
@property (copy) NSString *string;
@property (getter=isEditable) BOOL editable;
@property (getter=isSelectable) BOOL selectable;
@property (strong) NSFont *font;
@property (strong) NSColor *textColor, *backgroundColor;
@property BOOL drawsBackground, richText, verticallyResizable, horizontallyResizable, allowsUndo;
@property NSSize textContainerInset;
@property (weak) id<NSTextViewDelegate> delegate;
@property NSRange selectedRange;
@property (readonly) id textStorage;
- (void)insertText:(id)s;
- (void)scrollToEndOfDocument:(id)sender;
@end
@interface NSObject (mOSTextStorage)
- (void)appendAttributedString:(id)s;
- (void)setAttributedString:(id)s;
@end

/* ---- guias de layout */
@interface NSLayoutGuide : NSObject
@property (readonly, weak) NSView *owningView;
@property (readonly) NSRect frame;
@property (readonly, strong) NSLayoutXAxisAnchor *leadingAnchor, *trailingAnchor, *leftAnchor, *rightAnchor, *centerXAnchor;
@property (readonly, strong) NSLayoutYAxisAnchor *topAnchor, *bottomAnchor, *centerYAnchor;
@property (readonly, strong) NSLayoutDimension *widthAnchor, *heightAnchor;
@end
@interface NSView (NSLayoutGuideSupport)
- (void)addLayoutGuide:(NSLayoutGuide *)g;
- (void)removeLayoutGuide:(NSLayoutGuide *)g;
@property (readonly, strong) NSLayoutGuide *safeAreaLayoutGuide;
@property (readonly, strong) NSLayoutGuide *layoutMarginsGuide;
@end

/* ---- barra de menus e popover (Usagebar, Setapp, apps LSUIElement) */
typedef NS_ENUM(NSUInteger, NSRectEdge) {
    NSRectEdgeMinX = 0, NSRectEdgeMinY = 1, NSRectEdgeMaxX = 2, NSRectEdgeMaxY = 3
};
typedef NS_ENUM(NSInteger, NSPopoverBehavior) {
    NSPopoverBehaviorApplicationDefined = 0, NSPopoverBehaviorTransient = 1, NSPopoverBehaviorSemitransient = 2
};
extern const CGFloat NSStatusItemLength;       /* -1, largura do título */
extern const CGFloat NSStatusItemSquareLength; /* -2, quadrado da espessura da barra */
@class NSStatusBar, NSStatusItem, NSStatusBarButton, NSPopover, NSViewController;
@interface NSStatusBar : NSObject
+ (NSStatusBar *)systemStatusBar;
- (NSStatusItem *)statusItemWithLength:(CGFloat)length;
- (void)removeStatusItem:(NSStatusItem *)item;
@property (readonly) CGFloat thickness;
@end
@interface NSStatusItem : NSObject
@property (strong) NSMenu *menu;
@property (copy) NSString *title;
@property (strong) NSImage *image;
@property (copy) NSString *toolTip;
@property (getter=isVisible) BOOL visible;
@property CGFloat length;
@property (readonly, strong) NSStatusBarButton *button;
@property (readonly, weak) NSStatusBar *statusBar;
- (void)popUpStatusItemMenu:(NSMenu *)menu;
@end
@interface NSStatusBarButton : NSButton
@end
@interface NSPopover : NSResponder
@property NSPopoverBehavior behavior;
@property (strong) NSView *contentView;
@property (strong) NSViewController *contentViewController;
@property (readonly, getter=isShown) BOOL shown;
- (void)showRelativeToRect:(NSRect)rect ofView:(NSView *)view preferredEdge:(NSRectEdge)edge;
- (void)close;
@end