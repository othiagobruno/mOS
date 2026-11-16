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
@protocol NSCollectionViewDataSource <NSObject>
@required
- (long)collectionView:(nonnull NSCollectionView *)collectionView numberOfItemsInSection:(long)section;
- (nonnull NSCollectionViewItem *)collectionView:(nonnull NSCollectionView *)collectionView itemForRepresentedObjectAtIndexPath:(nonnull NSIndexPath *)indexPath;
@optional
- (long)numberOfSectionsInCollectionView:(nonnull NSCollectionView *)collectionView;
@end

@interface NSCollectionView : NSView
@property (nullable, weak) id<NSCollectionViewDataSource> dataSource;
- (void)reloadData;
@end
