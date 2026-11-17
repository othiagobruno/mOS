/* mOS — o mínimo das interfaces do AppKit para o overlay Swift (userland/frameworks/AppKit/AppKit.swift) ter os nomes
 * mangled da Apple (__C.NSEvent, __C.NSLayoutPriority, __C.NSCollectionView...). As classes existem no libSystem.mos. */
#pragma once
#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>

typedef float NSLayoutPriority __attribute__((swift_wrapper(struct)));

@interface NSEvent : NSObject
@property (readonly) unsigned long type;
@property (readonly, nullable, copy) NSString *charactersIgnoringModifiers;
@end

@interface NSImage : NSObject
+ (nullable instancetype)imageNamed:(nonnull NSString *)name;
- (nonnull instancetype)initWithSize:(CGSize)size;
@end

@interface NSResponder : NSObject
@end
@interface NSView : NSResponder
@end
@interface NSViewController : NSResponder
@end
@interface NSCollectionViewItem : NSViewController
@end

@class NSCollectionView;
@protocol NSCollectionViewElement;
@protocol NSCollectionViewDataSource <NSObject>
@required
- (long)collectionView:(nonnull NSCollectionView *)collectionView numberOfItemsInSection:(long)section;
- (nonnull NSCollectionViewItem *)collectionView:(nonnull NSCollectionView *)collectionView itemForRepresentedObjectAtIndexPath:(nonnull NSIndexPath *)indexPath;
@optional
- (long)numberOfSectionsInCollectionView:(nonnull NSCollectionView *)collectionView;
- (nonnull NSView<NSCollectionViewElement> *)collectionView:(nonnull NSCollectionView *)collectionView viewForSupplementaryElementOfKind:(nonnull NSString *)kind atIndexPath:(nonnull NSIndexPath *)indexPath;
@end

@interface NSCollectionView : NSView
@property (nullable, weak) id<NSCollectionViewDataSource> dataSource;
- (void)reloadData;
@end

/* ---- acréscimos para AppKit_Modern.swift (APIs do macOS 14 a 26). Os métodos marcados swift_private aparecem com
 * "__" no Swift; o overlay põe a forma da Apple por cima. */
typedef long NSWindowLevel __attribute__((swift_wrapper(struct)));
typedef struct NSEdgeInsets { CGFloat top; CGFloat left; CGFloat bottom; CGFloat right; } NSEdgeInsets;

@interface NSColorSpace : NSObject
+ (nonnull NSColorSpace *)sRGBColorSpace;
@end
@interface NSColor : NSObject
+ (nonnull NSColor *)colorWithSRGBRed:(CGFloat)red green:(CGFloat)green blue:(CGFloat)blue alpha:(CGFloat)alpha;
+ (nullable NSColor *)colorNamed:(nonnull NSString *)name bundle:(nullable NSBundle *)bundle;
@end
@interface NSParagraphStyle : NSObject
@end
@interface NSImage (MOSModern)
- (nullable instancetype)initWithContentsOfFile:(nonnull NSString *)path;
- (nullable instancetype)initWithData:(nonnull NSData *)data;
@property (readonly, nullable) NSData *TIFFRepresentation;
@end
@interface NSBundle (MOSAppKitImages)
- (nullable NSImage *)imageForResource:(nonnull NSString *)name;
- (nullable NSString *)pathForImageResource:(nonnull NSString *)name;
@end

@interface NSView (MOSModern)
@property BOOL needsDisplay;
@property BOOL needsLayout;
@property BOOL needsUpdateConstraints;
- (void)invalidateIntrinsicContentSize;
- (void)invalidateRestorableState;
@property (readonly) NSEdgeInsets safeAreaInsets;
@end
@interface NSViewController (MOSModern)
@property (strong, nonnull) NSView *view;
@property (readonly, getter=isViewLoaded) BOOL viewLoaded;
- (void)loadViewIfNeeded;
@end
@interface NSWindowController : NSResponder
@end
@interface NSStoryboard : NSObject
- (nullable id)instantiateInitialController __attribute__((swift_private));
- (nullable id)instantiateInitialControllerWithCreator:(id _Nullable (^ _Nullable)(NSCoder * _Nonnull coder))block __attribute__((swift_private));
@end
@interface NSGradient : NSObject
- (nullable instancetype)initWithColors:(nonnull NSArray *)colors atLocations:(const CGFloat * _Nullable)locations
                             colorSpace:(nonnull NSColorSpace *)colorSpace __attribute__((swift_private));
@end
@interface NSMenuItem : NSObject
+ (nonnull NSMenuItem *)sectionHeaderWithTitle:(nonnull NSString *)title __attribute__((swift_private));
@end
@interface NSPasteboard : NSObject
+ (nonnull NSPasteboard *)generalPasteboard;
- (nullable NSString *)stringForType:(nonnull NSString *)type;
@end

/* DataDetection (macOS 15); as classes são do libSystem.mos (appkit_view_more.c) */
@interface DDMatch : NSObject
@property (readonly, nonnull) NSString *matchedString;
@property (readonly) NSRange range;
@end
@interface DDMatchLink : DDMatch
@property (readonly, nonnull) NSURL *URL;
@end
DDMatch * _Nonnull cx_ddmatch_new(int kind, NSString * _Nonnull matched, NSRange range, id _Nullable value);

/* tabela e coleção para as fontes de dados "diffable" */
@class NSTableView;
@interface NSTableColumn : NSObject
@end
@interface NSTableRowView : NSView
@end
@protocol NSTableViewDataSource <NSObject>
@optional
- (long)numberOfRowsInTableView:(nonnull NSTableView *)tableView;
- (nullable NSView *)tableView:(nonnull NSTableView *)tableView viewForTableColumn:(nullable NSTableColumn *)tableColumn row:(long)row;
@end
@interface NSTableView : NSView
@property (nullable, weak) id<NSTableViewDataSource> dataSource;
- (void)reloadData;
@end
@protocol NSCollectionViewElement <NSObject>
@end
