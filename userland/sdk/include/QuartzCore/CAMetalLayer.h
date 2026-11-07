/* mOS SDK — QuartzCore: CALayer (mínimo) e CAMetalLayer/CAMetalDrawable (libsystem/quartzcore.c e metal.c) */
#pragma once
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#ifndef MOS_SDK_CALAYER
#define MOS_SDK_CALAYER
@interface CALayer : NSObject
+ (instancetype)layer;
@property CGRect bounds, frame;
@property CGPoint position, anchorPoint;
@property CGFloat contentsScale;
@property (strong) id contents;
@property float opacity;
@property (getter=isHidden) BOOL hidden;
@property (getter=isOpaque) BOOL opaque;
- (void)setNeedsDisplay;
- (void)addSublayer:(CALayer *)l;
- (void)removeFromSuperlayer;
@end
#endif

@class CAMetalLayer;
@protocol CAMetalDrawable <MTLDrawable>
@property (readonly) id<MTLTexture> texture;
@property (readonly) CAMetalLayer *layer;
@end
@interface CAMetalLayer : CALayer
@property (strong) id<MTLDevice> device;
@property (readonly) id<MTLDevice> preferredDevice;
@property MTLPixelFormat pixelFormat;
@property BOOL framebufferOnly;
@property CGSize drawableSize;
@property NSUInteger maximumDrawableCount;
@property BOOL presentsWithTransaction;
@property BOOL displaySyncEnabled;
@property BOOL allowsNextDrawableTimeout;
@property BOOL wantsExtendedDynamicRangeContent;
@property void *colorspace;
- (id<CAMetalDrawable>)nextDrawable;
@end
