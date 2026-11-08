/* mOS SDK (variante com interop ObjC) — o subconjunto do AppKit que o SwiftUI.framework do mOS usa
 * (NSHostingView/NSHostingController, Color(nsColor:)), com as declarações da Apple. As classes são as do
 * libSystem do mOS (userland/libsystem/appkit.c). Copiado por tools\build-swiftui-objc.ps1 para
 * build\swift\objc\sdk\include\AppKit. */
#pragma once
#import <Foundation/Foundation.h>
#include <CoreGraphics/CoreGraphics.h>

NS_ASSUME_NONNULL_BEGIN

typedef CGRect NSRect;
typedef CGSize NSSize;
typedef CGPoint NSPoint;

@class NSWindow, NSEvent;

@interface NSResponder : NSObject
- (BOOL)acceptsFirstResponder;
- (BOOL)becomeFirstResponder;
- (void)mouseDown:(NSEvent *)event;
- (void)mouseUp:(NSEvent *)event;
- (void)mouseDragged:(NSEvent *)event;
- (void)keyDown:(NSEvent *)event;
- (void)scrollWheel:(NSEvent *)event;
@end

@interface NSView : NSResponder
- (instancetype)initWithFrame:(NSRect)frameRect NS_DESIGNATED_INITIALIZER;
- (nullable instancetype)initWithCoder:(NSCoder *)coder NS_DESIGNATED_INITIALIZER;
@property NSRect frame;
@property NSRect bounds;
@property (readonly, getter=isFlipped) BOOL flipped;
@property (nullable, readonly, assign) NSWindow *window;
@property (nullable, readonly, assign) NSView *superview;
@property (copy) NSArray<__kindof NSView *> *subviews;
@property BOOL needsDisplay;
@property BOOL needsLayout;
@property (getter=isHidden) BOOL hidden;
@property (readonly) NSSize intrinsicContentSize;
@property NSSize fittingSize;
- (void)addSubview:(NSView *)view;
- (void)removeFromSuperview;
- (void)setFrameSize:(NSSize)newSize;
- (void)setFrameOrigin:(NSPoint)newOrigin;
- (void)layout;
- (void)drawRect:(NSRect)dirtyRect;
- (void)viewDidMoveToWindow;
- (void)invalidateIntrinsicContentSize;
@end

@interface NSViewController : NSResponder
- (instancetype)initWithNibName:(nullable NSString *)nibNameOrNil bundle:(nullable NSBundle *)nibBundleOrNil NS_DESIGNATED_INITIALIZER;
- (nullable instancetype)initWithCoder:(NSCoder *)coder NS_DESIGNATED_INITIALIZER;
@property (strong) NSView *view;
@property (nullable, copy) NSString *title;
@property NSSize preferredContentSize;
- (void)loadView;
- (void)viewDidLoad;
@end

@interface NSColor : NSObject
+ (NSColor *)colorWithSRGBRed:(CGFloat)red green:(CGFloat)green blue:(CGFloat)blue alpha:(CGFloat)alpha;
+ (NSColor *)colorWithCalibratedRed:(CGFloat)red green:(CGFloat)green blue:(CGFloat)blue alpha:(CGFloat)alpha;
- (nullable NSColor *)colorUsingColorSpaceName:(NSString *)name;
@property (readonly) CGFloat redComponent;
@property (readonly) CGFloat greenComponent;
@property (readonly) CGFloat blueComponent;
@property (readonly) CGFloat alphaComponent;
@property (readonly) CGColorRef CGColor;
@end

@interface NSImage : NSObject
- (nullable instancetype)initWithContentsOfFile:(NSString *)fileName;
+ (nullable NSImage *)imageNamed:(NSString *)name;
@property NSSize size;
@end

@interface NSFont : NSObject
+ (NSFont *)systemFontOfSize:(CGFloat)fontSize;
@property (readonly) CGFloat pointSize;
@end

NS_ASSUME_NONNULL_END
