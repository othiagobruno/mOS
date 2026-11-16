/*
 * mOS SDK (variante com interop ObjC) — PhotosUI do Mac Catalyst, no formato do SDK da Apple, para o overlay Swift
 * (userland/sdk/swift-objc/PhotosUI -> /System/iOSSupport/usr/lib/swift/libswiftPhotosUI.dylib). As classes estão no
 * PhotosUI.framework do mOS (userland/frameworks/PhotosUI). Como na Apple, as classes que o Swift troca por structs
 * (filtro, configuração, resultado) e o delegate ObjC são NS_REFINED_FOR_SWIFT: no Swift viram __PHPicker...
 */
#pragma once
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

/* Photos e Foundation: o mínimo que o overlay usa (as classes estão no libSystem / Photos.framework do mOS) */
@interface PHPhotoLibrary : NSObject
+ (PHPhotoLibrary *)sharedPhotoLibrary NS_SWIFT_NAME(shared());
@end
@interface NSItemProvider : NSObject
@property (nonatomic, readonly, copy) NSArray<NSString *> *registeredTypeIdentifiers;
@property (nonatomic, copy, nullable) NSString *suggestedName;
- (BOOL)hasItemConformingToTypeIdentifier:(NSString *)typeIdentifier;
@end

typedef NS_OPTIONS(NSUInteger, PHPickerCapabilities) {
    PHPickerCapabilitiesNone = 0,
    PHPickerCapabilitiesSearch = 1 << 0,
    PHPickerCapabilitiesStagingArea = 1 << 1,
    PHPickerCapabilitiesCollectionNavigation = 1 << 2,
    PHPickerCapabilitiesSelectionActions = 1 << 3,
    PHPickerCapabilitiesSensitivityAnalysisIntervention = 1 << 4,
};

NS_REFINED_FOR_SWIFT
@interface PHPickerFilter : NSObject <NSCopying>
@property (class, nonatomic, readonly) PHPickerFilter *imagesFilter;
@property (class, nonatomic, readonly) PHPickerFilter *videosFilter;
@property (class, nonatomic, readonly) PHPickerFilter *livePhotosFilter;
@property (class, nonatomic, readonly) PHPickerFilter *depthEffectPhotosFilter;
@property (class, nonatomic, readonly) PHPickerFilter *burstsFilter;
@property (class, nonatomic, readonly) PHPickerFilter *panoramasFilter;
@property (class, nonatomic, readonly) PHPickerFilter *screenshotsFilter;
@property (class, nonatomic, readonly) PHPickerFilter *screenRecordingsFilter;
@property (class, nonatomic, readonly) PHPickerFilter *cinematicVideosFilter;
@property (class, nonatomic, readonly) PHPickerFilter *slomoVideosFilter;
@property (class, nonatomic, readonly) PHPickerFilter *timelapseVideosFilter;
@property (class, nonatomic, readonly) PHPickerFilter *spatialMediaFilter;
+ (PHPickerFilter *)anyFilterMatchingSubfilters:(NSArray<PHPickerFilter *> *)subfilters;
+ (PHPickerFilter *)allFilterMatchingSubfilters:(NSArray<PHPickerFilter *> *)subfilters;
+ (PHPickerFilter *)notFilterOfSubfilter:(PHPickerFilter *)subfilter;
- (BOOL)_mosMatchesMediaType:(NSInteger)mediaType subtypes:(NSUInteger)subtypes;
@end

NS_REFINED_FOR_SWIFT
@interface PHPickerConfiguration : NSObject <NSCopying>
@property (nonatomic) NSInteger preferredAssetRepresentationMode;
@property (nonatomic) NSInteger selection;
@property (nonatomic) NSInteger selectionLimit;
@property (nonatomic) NSInteger mode;
@property (nonatomic) PHPickerCapabilities disabledCapabilities;
@property (nonatomic) NSDirectionalRectEdge edgesWithoutContentMargins;
@property (nonatomic, copy, nullable) PHPickerFilter *filter;
@property (nonatomic, copy) NSArray<NSString *> *preselectedAssetIdentifiers;
- (instancetype)init;
- (instancetype)initWithPhotoLibrary:(PHPhotoLibrary *)photoLibrary;
- (nullable PHPhotoLibrary *)_mosLibrary;
@end

NS_REFINED_FOR_SWIFT
@interface PHPickerResult : NSObject
@property (nonatomic, readonly) NSItemProvider *itemProvider;
@property (nonatomic, readonly, nullable) NSString *assetIdentifier;
@end

@class PHPickerViewController;
NS_REFINED_FOR_SWIFT
@protocol PHPickerViewControllerDelegate <NSObject>
- (void)picker:(PHPickerViewController *)picker didFinishPicking:(NSArray<PHPickerResult *> *)results;
@end

@interface PHPickerViewController : UIViewController
- (instancetype)initWithConfiguration:(PHPickerConfiguration *)configuration NS_REFINED_FOR_SWIFT;
@property (nonatomic, readonly, copy) PHPickerConfiguration *configuration NS_REFINED_FOR_SWIFT;
@property (nonatomic, weak, nullable) id<PHPickerViewControllerDelegate> delegate NS_REFINED_FOR_SWIFT;
- (void)deselectAssetsWithIdentifiers:(NSArray<NSString *> *)identifiers;
- (void)moveAssetWithIdentifier:(NSString *)identifier afterAssetWithIdentifier:(nullable NSString *)afterIdentifier;
- (void)scrollToInitialPosition;
@end

@interface PHLivePhotoView : UIView
@property (nonatomic, getter=isMuted) BOOL muted;
@end

NS_ASSUME_NONNULL_END
