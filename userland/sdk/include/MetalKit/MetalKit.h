/* mOS SDK — MetalKit: MTKView (libsystem/metal.c). O laço de desenho é um NSTimer a preferredFramesPerSecond. */
#pragma once
#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

@class MTKView;
@protocol MTKViewDelegate <NSObject>
- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size;
- (void)drawInMTKView:(MTKView *)view;
@end
@interface MTKView : NSView
- (instancetype)initWithFrame:(CGRect)frame device:(id<MTLDevice>)device;
@property (weak) id<MTKViewDelegate> delegate;
@property (strong) id<MTLDevice> device;
@property (readonly) id<CAMetalDrawable> currentDrawable;
@property (readonly) MTLRenderPassDescriptor *currentRenderPassDescriptor;
@property (readonly) id<MTLTexture> depthStencilTexture;
@property (readonly) id<MTLTexture> multisampleColorTexture;
@property MTLPixelFormat colorPixelFormat;
@property MTLPixelFormat depthStencilPixelFormat;
@property NSUInteger sampleCount;
@property MTLClearColor clearColor;
@property double clearDepth;
@property uint32_t clearStencil;
@property NSInteger preferredFramesPerSecond;
@property (getter=isPaused) BOOL paused;
@property BOOL enableSetNeedsDisplay;
@property BOOL autoResizeDrawable;
@property BOOL framebufferOnly;
@property BOOL presentsWithTransaction;
@property CGSize drawableSize;
@property (readonly) CGSize preferredDrawableSize;
- (void)draw;
- (void)releaseDrawables;
@end
