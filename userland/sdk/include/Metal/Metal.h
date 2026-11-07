/*
 * mOS SDK — Metal (subconjunto da API da Apple implementado pelo Metal por software do libSystem:
 * userland/libsystem/metal.c). Os valores dos enums e o layout das structs são os do macOS.
 * Shaders: só fonte MSL (newLibraryWithSource:, ou os .metal em Resources para newDefaultLibrary);
 * .metallib (AIR) não roda no mOS.
 */
#pragma once
#import <Foundation/Foundation.h>

/* ---- enums */
typedef NS_ENUM(NSUInteger, MTLPixelFormat) {
    MTLPixelFormatInvalid = 0, MTLPixelFormatA8Unorm = 1, MTLPixelFormatR8Unorm = 10, MTLPixelFormatR8Unorm_sRGB = 11,
    MTLPixelFormatR8Snorm = 12, MTLPixelFormatR8Uint = 13, MTLPixelFormatR8Sint = 14, MTLPixelFormatR16Unorm = 20,
    MTLPixelFormatR16Snorm = 22, MTLPixelFormatR16Uint = 23, MTLPixelFormatR16Sint = 24, MTLPixelFormatR16Float = 25,
    MTLPixelFormatRG8Unorm = 30, MTLPixelFormatRG8Unorm_sRGB = 31, MTLPixelFormatRG8Snorm = 32, MTLPixelFormatRG8Uint = 33,
    MTLPixelFormatRG8Sint = 34, MTLPixelFormatB5G6R5Unorm = 40, MTLPixelFormatA1BGR5Unorm = 41, MTLPixelFormatABGR4Unorm = 42,
    MTLPixelFormatBGR5A1Unorm = 43, MTLPixelFormatR32Uint = 53, MTLPixelFormatR32Sint = 54, MTLPixelFormatR32Float = 55,
    MTLPixelFormatRG16Unorm = 60, MTLPixelFormatRG16Snorm = 62, MTLPixelFormatRG16Uint = 63, MTLPixelFormatRG16Sint = 64,
    MTLPixelFormatRG16Float = 65, MTLPixelFormatRGBA8Unorm = 70, MTLPixelFormatRGBA8Unorm_sRGB = 71, MTLPixelFormatRGBA8Snorm = 72,
    MTLPixelFormatRGBA8Uint = 73, MTLPixelFormatRGBA8Sint = 74, MTLPixelFormatBGRA8Unorm = 80, MTLPixelFormatBGRA8Unorm_sRGB = 81,
    MTLPixelFormatRGB10A2Unorm = 90, MTLPixelFormatRGB10A2Uint = 91, MTLPixelFormatRG11B10Float = 92, MTLPixelFormatRGB9E5Float = 93,
    MTLPixelFormatBGR10A2Unorm = 94, MTLPixelFormatRG32Uint = 103, MTLPixelFormatRG32Sint = 104, MTLPixelFormatRG32Float = 105,
    MTLPixelFormatRGBA16Unorm = 110, MTLPixelFormatRGBA16Snorm = 112, MTLPixelFormatRGBA16Uint = 113, MTLPixelFormatRGBA16Sint = 114,
    MTLPixelFormatRGBA16Float = 115, MTLPixelFormatRGBA32Uint = 123, MTLPixelFormatRGBA32Sint = 124, MTLPixelFormatRGBA32Float = 125,
    MTLPixelFormatDepth16Unorm = 250, MTLPixelFormatDepth32Float = 252, MTLPixelFormatStencil8 = 253,
    MTLPixelFormatDepth24Unorm_Stencil8 = 255, MTLPixelFormatDepth32Float_Stencil8 = 260, MTLPixelFormatX32_Stencil8 = 261,
    MTLPixelFormatX24_Stencil8 = 262, MTLPixelFormatBGRA10_XR = 552, MTLPixelFormatBGRA10_XR_sRGB = 553,
    MTLPixelFormatBGR10_XR = 554, MTLPixelFormatBGR10_XR_sRGB = 555,
};
typedef NS_ENUM(NSUInteger, MTLPrimitiveType) { MTLPrimitiveTypePoint = 0, MTLPrimitiveTypeLine, MTLPrimitiveTypeLineStrip, MTLPrimitiveTypeTriangle, MTLPrimitiveTypeTriangleStrip };
typedef NS_ENUM(NSUInteger, MTLIndexType) { MTLIndexTypeUInt16 = 0, MTLIndexTypeUInt32 = 1 };
typedef NS_ENUM(NSUInteger, MTLLoadAction) { MTLLoadActionDontCare = 0, MTLLoadActionLoad = 1, MTLLoadActionClear = 2 };
typedef NS_ENUM(NSUInteger, MTLStoreAction) { MTLStoreActionDontCare = 0, MTLStoreActionStore, MTLStoreActionMultisampleResolve, MTLStoreActionStoreAndMultisampleResolve, MTLStoreActionUnknown, MTLStoreActionCustomSampleDepthStore };
typedef NS_ENUM(NSUInteger, MTLCPUCacheMode) { MTLCPUCacheModeDefaultCache = 0, MTLCPUCacheModeWriteCombined = 1 };
typedef NS_ENUM(NSUInteger, MTLStorageMode) { MTLStorageModeShared = 0, MTLStorageModeManaged = 1, MTLStorageModePrivate = 2, MTLStorageModeMemoryless = 3 };
typedef NS_ENUM(NSUInteger, MTLHazardTrackingMode) { MTLHazardTrackingModeDefault = 0, MTLHazardTrackingModeUntracked = 1, MTLHazardTrackingModeTracked = 2 };
typedef NS_OPTIONS(NSUInteger, MTLResourceOptions) {
    MTLResourceCPUCacheModeDefaultCache = 0, MTLResourceCPUCacheModeWriteCombined = 1,
    MTLResourceStorageModeShared = 0 << 4, MTLResourceStorageModeManaged = 1 << 4, MTLResourceStorageModePrivate = 2 << 4,
    MTLResourceStorageModeMemoryless = 3 << 4, MTLResourceHazardTrackingModeDefault = 0 << 8,
    MTLResourceHazardTrackingModeUntracked = 1 << 8, MTLResourceHazardTrackingModeTracked = 2 << 8,
    MTLResourceOptionCPUCacheModeDefault = 0, MTLResourceOptionCPUCacheModeWriteCombined = 1,
};
typedef NS_ENUM(NSUInteger, MTLTextureType) {
    MTLTextureType1D = 0, MTLTextureType1DArray, MTLTextureType2D, MTLTextureType2DArray, MTLTextureType2DMultisample,
    MTLTextureTypeCube, MTLTextureTypeCubeArray, MTLTextureType3D, MTLTextureType2DMultisampleArray, MTLTextureTypeTextureBuffer,
};
typedef NS_OPTIONS(NSUInteger, MTLTextureUsage) {
    MTLTextureUsageUnknown = 0, MTLTextureUsageShaderRead = 1, MTLTextureUsageShaderWrite = 2, MTLTextureUsageRenderTarget = 4,
    MTLTextureUsagePixelFormatView = 0x10, MTLTextureUsageShaderAtomic = 0x20,
};
typedef NS_ENUM(NSUInteger, MTLVertexFormat) {
    MTLVertexFormatInvalid = 0, MTLVertexFormatUChar2 = 1, MTLVertexFormatUChar3, MTLVertexFormatUChar4, MTLVertexFormatChar2,
    MTLVertexFormatChar3, MTLVertexFormatChar4, MTLVertexFormatUChar2Normalized, MTLVertexFormatUChar3Normalized,
    MTLVertexFormatUChar4Normalized, MTLVertexFormatChar2Normalized, MTLVertexFormatChar3Normalized, MTLVertexFormatChar4Normalized,
    MTLVertexFormatUShort2, MTLVertexFormatUShort3, MTLVertexFormatUShort4, MTLVertexFormatShort2, MTLVertexFormatShort3,
    MTLVertexFormatShort4, MTLVertexFormatUShort2Normalized, MTLVertexFormatUShort3Normalized, MTLVertexFormatUShort4Normalized,
    MTLVertexFormatShort2Normalized, MTLVertexFormatShort3Normalized, MTLVertexFormatShort4Normalized, MTLVertexFormatHalf2,
    MTLVertexFormatHalf3, MTLVertexFormatHalf4, MTLVertexFormatFloat, MTLVertexFormatFloat2, MTLVertexFormatFloat3,
    MTLVertexFormatFloat4, MTLVertexFormatInt, MTLVertexFormatInt2, MTLVertexFormatInt3, MTLVertexFormatInt4, MTLVertexFormatUInt,
    MTLVertexFormatUInt2, MTLVertexFormatUInt3, MTLVertexFormatUInt4, MTLVertexFormatInt1010102Normalized,
    MTLVertexFormatUInt1010102Normalized, MTLVertexFormatUChar4Normalized_BGRA = 42, MTLVertexFormatUChar = 45, MTLVertexFormatChar,
    MTLVertexFormatUCharNormalized, MTLVertexFormatCharNormalized, MTLVertexFormatUShort, MTLVertexFormatShort,
    MTLVertexFormatUShortNormalized, MTLVertexFormatShortNormalized, MTLVertexFormatHalf,
};
typedef NS_ENUM(NSUInteger, MTLVertexStepFunction) { MTLVertexStepFunctionConstant = 0, MTLVertexStepFunctionPerVertex, MTLVertexStepFunctionPerInstance };
typedef NS_ENUM(NSUInteger, MTLCompareFunction) {
    MTLCompareFunctionNever = 0, MTLCompareFunctionLess, MTLCompareFunctionEqual, MTLCompareFunctionLessEqual,
    MTLCompareFunctionGreater, MTLCompareFunctionNotEqual, MTLCompareFunctionGreaterEqual, MTLCompareFunctionAlways,
};
typedef NS_ENUM(NSUInteger, MTLStencilOperation) {
    MTLStencilOperationKeep = 0, MTLStencilOperationZero, MTLStencilOperationReplace, MTLStencilOperationIncrementClamp,
    MTLStencilOperationDecrementClamp, MTLStencilOperationInvert, MTLStencilOperationIncrementWrap, MTLStencilOperationDecrementWrap,
};
typedef NS_ENUM(NSUInteger, MTLCullMode) { MTLCullModeNone = 0, MTLCullModeFront, MTLCullModeBack };
typedef NS_ENUM(NSUInteger, MTLWinding) { MTLWindingClockwise = 0, MTLWindingCounterClockwise = 1 };
typedef NS_ENUM(NSUInteger, MTLTriangleFillMode) { MTLTriangleFillModeFill = 0, MTLTriangleFillModeLines = 1 };
typedef NS_ENUM(NSUInteger, MTLDepthClipMode) { MTLDepthClipModeClip = 0, MTLDepthClipModeClamp = 1 };
typedef NS_ENUM(NSUInteger, MTLBlendFactor) {
    MTLBlendFactorZero = 0, MTLBlendFactorOne, MTLBlendFactorSourceColor, MTLBlendFactorOneMinusSourceColor,
    MTLBlendFactorSourceAlpha, MTLBlendFactorOneMinusSourceAlpha, MTLBlendFactorDestinationColor,
    MTLBlendFactorOneMinusDestinationColor, MTLBlendFactorDestinationAlpha, MTLBlendFactorOneMinusDestinationAlpha,
    MTLBlendFactorSourceAlphaSaturated, MTLBlendFactorBlendColor, MTLBlendFactorOneMinusBlendColor, MTLBlendFactorBlendAlpha,
    MTLBlendFactorOneMinusBlendAlpha,
};
typedef NS_ENUM(NSUInteger, MTLBlendOperation) { MTLBlendOperationAdd = 0, MTLBlendOperationSubtract, MTLBlendOperationReverseSubtract, MTLBlendOperationMin, MTLBlendOperationMax };
typedef NS_OPTIONS(NSUInteger, MTLColorWriteMask) { MTLColorWriteMaskNone = 0, MTLColorWriteMaskRed = 8, MTLColorWriteMaskGreen = 4, MTLColorWriteMaskBlue = 2, MTLColorWriteMaskAlpha = 1, MTLColorWriteMaskAll = 15 };
typedef NS_ENUM(NSUInteger, MTLSamplerMinMagFilter) { MTLSamplerMinMagFilterNearest = 0, MTLSamplerMinMagFilterLinear = 1 };
typedef NS_ENUM(NSUInteger, MTLSamplerMipFilter) { MTLSamplerMipFilterNotMipmapped = 0, MTLSamplerMipFilterNearest, MTLSamplerMipFilterLinear };
typedef NS_ENUM(NSUInteger, MTLSamplerAddressMode) {
    MTLSamplerAddressModeClampToEdge = 0, MTLSamplerAddressModeMirrorClampToEdge, MTLSamplerAddressModeRepeat,
    MTLSamplerAddressModeMirrorRepeat, MTLSamplerAddressModeClampToZero, MTLSamplerAddressModeClampToBorderColor,
};
typedef NS_ENUM(NSUInteger, MTLSamplerBorderColor) { MTLSamplerBorderColorTransparentBlack = 0, MTLSamplerBorderColorOpaqueBlack, MTLSamplerBorderColorOpaqueWhite };
typedef NS_ENUM(NSUInteger, MTLCommandBufferStatus) {
    MTLCommandBufferStatusNotEnqueued = 0, MTLCommandBufferStatusEnqueued, MTLCommandBufferStatusCommitted,
    MTLCommandBufferStatusScheduled, MTLCommandBufferStatusCompleted, MTLCommandBufferStatusError,
};
typedef NS_ENUM(NSUInteger, MTLPurgeableState) { MTLPurgeableStateKeepCurrent = 1, MTLPurgeableStateNonVolatile, MTLPurgeableStateVolatile, MTLPurgeableStateEmpty };
typedef NS_ENUM(NSUInteger, MTLFunctionType) { MTLFunctionTypeVertex = 1, MTLFunctionTypeFragment, MTLFunctionTypeKernel, MTLFunctionTypeVisible = 5 };
typedef NS_ENUM(NSInteger, MTLGPUFamily) {
    MTLGPUFamilyApple1 = 1001, MTLGPUFamilyMac1 = 2001, MTLGPUFamilyMac2 = 2002, MTLGPUFamilyCommon1 = 3001,
    MTLGPUFamilyCommon2 = 3002, MTLGPUFamilyCommon3 = 3003, MTLGPUFamilyMetal3 = 5001,
};
typedef NS_ENUM(NSUInteger, MTLLanguageVersion) { MTLLanguageVersion2_0 = (2 << 16), MTLLanguageVersion3_0 = (3 << 16), MTLLanguageVersion3_1 = (3 << 16) + 1 };
typedef NS_OPTIONS(NSUInteger, MTLBarrierScope) { MTLBarrierScopeBuffers = 1, MTLBarrierScopeTextures = 2, MTLBarrierScopeRenderTargets = 4 };
typedef NS_OPTIONS(NSUInteger, MTLResourceUsage) { MTLResourceUsageRead = 1, MTLResourceUsageWrite = 2 };
typedef NS_ENUM(NSUInteger, MTLDispatchType) { MTLDispatchTypeSerial = 0, MTLDispatchTypeConcurrent = 1 };

/* ---- structs */
typedef struct { NSUInteger width, height, depth; } MTLSize;
typedef struct { NSUInteger x, y, z; } MTLOrigin;
typedef struct { MTLOrigin origin; MTLSize size; } MTLRegion;
typedef struct { double red, green, blue, alpha; } MTLClearColor;
typedef struct { double originX, originY, width, height, znear, zfar; } MTLViewport;
typedef struct { NSUInteger x, y, width, height; } MTLScissorRect;
typedef struct { NSUInteger size, align; } MTLSizeAndAlign;
static inline MTLSize MTLSizeMake(NSUInteger w, NSUInteger h, NSUInteger d) { MTLSize s = {w, h, d}; return s; }
static inline MTLOrigin MTLOriginMake(NSUInteger x, NSUInteger y, NSUInteger z) { MTLOrigin o = {x, y, z}; return o; }
static inline MTLRegion MTLRegionMake1D(NSUInteger x, NSUInteger w) { MTLRegion r = {{x, 0, 0}, {w, 1, 1}}; return r; }
static inline MTLRegion MTLRegionMake2D(NSUInteger x, NSUInteger y, NSUInteger w, NSUInteger h) { MTLRegion r = {{x, y, 0}, {w, h, 1}}; return r; }
static inline MTLRegion MTLRegionMake3D(NSUInteger x, NSUInteger y, NSUInteger z, NSUInteger w, NSUInteger h, NSUInteger d) { MTLRegion r = {{x, y, z}, {w, h, d}}; return r; }
static inline MTLClearColor MTLClearColorMake(double r, double g, double b, double a) { MTLClearColor c = {r, g, b, a}; return c; }

@protocol MTLDevice, MTLBuffer, MTLTexture, MTLCommandBuffer, MTLFunction, MTLLibrary, MTLDrawable;
@class NSURL;
@class MTLTextureDescriptor, MTLSamplerDescriptor, MTLDepthStencilDescriptor, MTLRenderPassDescriptor,
    MTLRenderPipelineDescriptor, MTLComputePipelineDescriptor, MTLCompileOptions, MTLVertexDescriptor, MTLHeapDescriptor,
    MTLFunctionConstantValues;

/* ---- recursos */
@protocol MTLResource <NSObject>
@property (copy) NSString *label;
@property (readonly) id<MTLDevice> device;
@property (readonly) MTLCPUCacheMode cpuCacheMode;
@property (readonly) MTLStorageMode storageMode;
@property (readonly) MTLResourceOptions resourceOptions;
- (MTLPurgeableState)setPurgeableState:(MTLPurgeableState)state;
@end
@protocol MTLBuffer <MTLResource>
@property (readonly) NSUInteger length;
@property (readonly) uint64_t gpuAddress;
- (void *)contents;
- (void)didModifyRange:(NSRange)range;
- (id<MTLTexture>)newTextureWithDescriptor:(MTLTextureDescriptor *)d offset:(NSUInteger)offset bytesPerRow:(NSUInteger)bpr;
@end
@protocol MTLTexture <MTLResource>
@property (readonly) MTLTextureType textureType;
@property (readonly) MTLPixelFormat pixelFormat;
@property (readonly) NSUInteger width, height, depth, mipmapLevelCount, sampleCount, arrayLength;
@property (readonly) MTLTextureUsage usage;
@property (readonly, getter=isFramebufferOnly) BOOL framebufferOnly;
@property (readonly) id iosurface;
- (void)replaceRegion:(MTLRegion)region mipmapLevel:(NSUInteger)level withBytes:(const void *)bytes bytesPerRow:(NSUInteger)bpr;
- (void)replaceRegion:(MTLRegion)region mipmapLevel:(NSUInteger)level slice:(NSUInteger)slice withBytes:(const void *)bytes bytesPerRow:(NSUInteger)bpr bytesPerImage:(NSUInteger)bpi;
- (void)getBytes:(void *)bytes bytesPerRow:(NSUInteger)bpr fromRegion:(MTLRegion)region mipmapLevel:(NSUInteger)level;
- (void)getBytes:(void *)bytes bytesPerRow:(NSUInteger)bpr bytesPerImage:(NSUInteger)bpi fromRegion:(MTLRegion)region mipmapLevel:(NSUInteger)level slice:(NSUInteger)slice;
- (id<MTLTexture>)newTextureViewWithPixelFormat:(MTLPixelFormat)fmt;
- (id<MTLTexture>)newTextureViewWithPixelFormat:(MTLPixelFormat)fmt textureType:(MTLTextureType)t levels:(NSRange)levels slices:(NSRange)slices;
@end
@protocol MTLSamplerState <NSObject>
@property (readonly) id<MTLDevice> device;
@end
@protocol MTLDepthStencilState <NSObject>
@property (readonly) id<MTLDevice> device;
@end
@protocol MTLFunction <NSObject>
@property (copy) NSString *label;
@property (readonly) NSString *name;
@property (readonly) MTLFunctionType functionType;
@property (readonly) id<MTLDevice> device;
@end
@protocol MTLLibrary <NSObject>
@property (copy) NSString *label;
@property (readonly) NSArray<NSString *> *functionNames;
@property (readonly) id<MTLDevice> device;
- (id<MTLFunction>)newFunctionWithName:(NSString *)name;
- (id<MTLFunction>)newFunctionWithName:(NSString *)name constantValues:(MTLFunctionConstantValues *)v error:(NSError **)err;
@end
@protocol MTLRenderPipelineState <NSObject>
@property (readonly) NSString *label;
@property (readonly) id<MTLDevice> device;
@end
@protocol MTLComputePipelineState <NSObject>
@property (readonly) NSString *label;
@property (readonly) id<MTLDevice> device;
@property (readonly) NSUInteger maxTotalThreadsPerThreadgroup;
@property (readonly) NSUInteger threadExecutionWidth;
@property (readonly) NSUInteger staticThreadgroupMemoryLength;
@end
@protocol MTLFence <NSObject>
@end
@protocol MTLEvent <NSObject>
@end
@protocol MTLSharedEvent <MTLEvent>
@property uint64_t signaledValue;
@end
@protocol MTLHeap <NSObject>
@property (readonly) NSUInteger size, usedSize;
- (id<MTLBuffer>)newBufferWithLength:(NSUInteger)length options:(MTLResourceOptions)options;
- (id<MTLTexture>)newTextureWithDescriptor:(MTLTextureDescriptor *)d;
@end

/* ---- encoders */
@protocol MTLCommandEncoder <NSObject>
@property (copy) NSString *label;
@property (readonly) id<MTLDevice> device;
- (void)endEncoding;
- (void)pushDebugGroup:(NSString *)s;
- (void)popDebugGroup;
- (void)insertDebugSignpost:(NSString *)s;
@end
@protocol MTLRenderCommandEncoder <MTLCommandEncoder>
- (void)setRenderPipelineState:(id<MTLRenderPipelineState>)p;
- (void)setVertexBuffer:(id<MTLBuffer>)b offset:(NSUInteger)off atIndex:(NSUInteger)i;
- (void)setVertexBufferOffset:(NSUInteger)off atIndex:(NSUInteger)i;
- (void)setVertexBytes:(const void *)bytes length:(NSUInteger)len atIndex:(NSUInteger)i;
- (void)setVertexTexture:(id<MTLTexture>)t atIndex:(NSUInteger)i;
- (void)setVertexSamplerState:(id<MTLSamplerState>)s atIndex:(NSUInteger)i;
- (void)setFragmentBuffer:(id<MTLBuffer>)b offset:(NSUInteger)off atIndex:(NSUInteger)i;
- (void)setFragmentBufferOffset:(NSUInteger)off atIndex:(NSUInteger)i;
- (void)setFragmentBytes:(const void *)bytes length:(NSUInteger)len atIndex:(NSUInteger)i;
- (void)setFragmentTexture:(id<MTLTexture>)t atIndex:(NSUInteger)i;
- (void)setFragmentSamplerState:(id<MTLSamplerState>)s atIndex:(NSUInteger)i;
- (void)setViewport:(MTLViewport)v;
- (void)setScissorRect:(MTLScissorRect)r;
- (void)setFrontFacingWinding:(MTLWinding)w;
- (void)setCullMode:(MTLCullMode)m;
- (void)setTriangleFillMode:(MTLTriangleFillMode)m;
- (void)setDepthClipMode:(MTLDepthClipMode)m;
- (void)setDepthBias:(float)bias slopeScale:(float)slope clamp:(float)clamp;
- (void)setDepthStencilState:(id<MTLDepthStencilState>)s;
- (void)setStencilReferenceValue:(uint32_t)v;
- (void)setStencilFrontReferenceValue:(uint32_t)f backReferenceValue:(uint32_t)b;
- (void)setBlendColorRed:(float)r green:(float)g blue:(float)b alpha:(float)a;
- (void)drawPrimitives:(MTLPrimitiveType)t vertexStart:(NSUInteger)s vertexCount:(NSUInteger)n;
- (void)drawPrimitives:(MTLPrimitiveType)t vertexStart:(NSUInteger)s vertexCount:(NSUInteger)n instanceCount:(NSUInteger)i;
- (void)drawPrimitives:(MTLPrimitiveType)t vertexStart:(NSUInteger)s vertexCount:(NSUInteger)n instanceCount:(NSUInteger)i baseInstance:(NSUInteger)b;
- (void)drawIndexedPrimitives:(MTLPrimitiveType)t indexCount:(NSUInteger)n indexType:(MTLIndexType)it indexBuffer:(id<MTLBuffer>)ib indexBufferOffset:(NSUInteger)off;
- (void)drawIndexedPrimitives:(MTLPrimitiveType)t indexCount:(NSUInteger)n indexType:(MTLIndexType)it indexBuffer:(id<MTLBuffer>)ib indexBufferOffset:(NSUInteger)off instanceCount:(NSUInteger)i;
- (void)drawIndexedPrimitives:(MTLPrimitiveType)t indexCount:(NSUInteger)n indexType:(MTLIndexType)it indexBuffer:(id<MTLBuffer>)ib indexBufferOffset:(NSUInteger)off instanceCount:(NSUInteger)i baseVertex:(NSInteger)bv baseInstance:(NSUInteger)bi;
- (void)drawPrimitives:(MTLPrimitiveType)t indirectBuffer:(id<MTLBuffer>)b indirectBufferOffset:(NSUInteger)off;
@end
@protocol MTLComputeCommandEncoder <MTLCommandEncoder>
- (void)setComputePipelineState:(id<MTLComputePipelineState>)p;
- (void)setBuffer:(id<MTLBuffer>)b offset:(NSUInteger)off atIndex:(NSUInteger)i;
- (void)setBufferOffset:(NSUInteger)off atIndex:(NSUInteger)i;
- (void)setBytes:(const void *)bytes length:(NSUInteger)len atIndex:(NSUInteger)i;
- (void)setTexture:(id<MTLTexture>)t atIndex:(NSUInteger)i;
- (void)setSamplerState:(id<MTLSamplerState>)s atIndex:(NSUInteger)i;
- (void)setThreadgroupMemoryLength:(NSUInteger)len atIndex:(NSUInteger)i;
- (void)dispatchThreadgroups:(MTLSize)groups threadsPerThreadgroup:(MTLSize)tptg;
- (void)dispatchThreads:(MTLSize)threads threadsPerThreadgroup:(MTLSize)tptg;
- (void)dispatchThreadgroupsWithIndirectBuffer:(id<MTLBuffer>)b indirectBufferOffset:(NSUInteger)off threadsPerThreadgroup:(MTLSize)tptg;
- (void)memoryBarrierWithScope:(MTLBarrierScope)s;
@end
@protocol MTLBlitCommandEncoder <MTLCommandEncoder>
- (void)copyFromBuffer:(id<MTLBuffer>)s sourceOffset:(NSUInteger)so toBuffer:(id<MTLBuffer>)d destinationOffset:(NSUInteger)dof size:(NSUInteger)n;
- (void)copyFromBuffer:(id<MTLBuffer>)s sourceOffset:(NSUInteger)so sourceBytesPerRow:(NSUInteger)bpr sourceBytesPerImage:(NSUInteger)bpi sourceSize:(MTLSize)sz toTexture:(id<MTLTexture>)t destinationSlice:(NSUInteger)sl destinationLevel:(NSUInteger)lv destinationOrigin:(MTLOrigin)o;
- (void)copyFromTexture:(id<MTLTexture>)t sourceSlice:(NSUInteger)sl sourceLevel:(NSUInteger)lv sourceOrigin:(MTLOrigin)o sourceSize:(MTLSize)sz toBuffer:(id<MTLBuffer>)b destinationOffset:(NSUInteger)off destinationBytesPerRow:(NSUInteger)bpr destinationBytesPerImage:(NSUInteger)bpi;
- (void)copyFromTexture:(id<MTLTexture>)s sourceSlice:(NSUInteger)ss sourceLevel:(NSUInteger)sl sourceOrigin:(MTLOrigin)so sourceSize:(MTLSize)sz toTexture:(id<MTLTexture>)d destinationSlice:(NSUInteger)ds destinationLevel:(NSUInteger)dl destinationOrigin:(MTLOrigin)dorg;
- (void)copyFromTexture:(id<MTLTexture>)s toTexture:(id<MTLTexture>)d;
- (void)fillBuffer:(id<MTLBuffer>)b range:(NSRange)r value:(uint8_t)v;
- (void)generateMipmapsForTexture:(id<MTLTexture>)t;
- (void)synchronizeResource:(id<MTLResource>)r;
@end

/* ---- fila, command buffer, drawable */
typedef void (^MTLCommandBufferHandler)(id<MTLCommandBuffer> buffer);
@protocol MTLDrawable <NSObject>
- (void)present;
- (void)presentAtTime:(double)t;
@property (readonly) NSUInteger drawableID;
@end
@protocol MTLCommandBuffer <NSObject>
@property (copy) NSString *label;
@property (readonly) id<MTLDevice> device;
@property (readonly) MTLCommandBufferStatus status;
@property (readonly) NSError *error;
@property (readonly) double GPUStartTime, GPUEndTime;
- (void)enqueue;
- (void)commit;
- (void)waitUntilScheduled;
- (void)waitUntilCompleted;
- (void)addScheduledHandler:(MTLCommandBufferHandler)block;
- (void)addCompletedHandler:(MTLCommandBufferHandler)block;
- (void)presentDrawable:(id<MTLDrawable>)d;
- (void)encodeSignalEvent:(id<MTLEvent>)e value:(uint64_t)v;
- (void)encodeWaitForEvent:(id<MTLEvent>)e value:(uint64_t)v;
- (id<MTLRenderCommandEncoder>)renderCommandEncoderWithDescriptor:(MTLRenderPassDescriptor *)d;
- (id<MTLComputeCommandEncoder>)computeCommandEncoder;
- (id<MTLBlitCommandEncoder>)blitCommandEncoder;
@end
@protocol MTLCommandQueue <NSObject>
@property (copy) NSString *label;
@property (readonly) id<MTLDevice> device;
- (id<MTLCommandBuffer>)commandBuffer;
- (id<MTLCommandBuffer>)commandBufferWithUnretainedReferences;
@end

/* ---- device */
@protocol MTLDevice <NSObject>
@property (readonly) NSString *name;
@property (readonly) uint64_t registryID;
@property (readonly) MTLSize maxThreadsPerThreadgroup;
@property (readonly) NSUInteger maxThreadgroupMemoryLength;
@property (readonly) NSUInteger maxBufferLength;
@property (readonly) uint64_t recommendedMaxWorkingSetSize;
@property (readonly) NSUInteger currentAllocatedSize;
@property (readonly) BOOL hasUnifiedMemory;
@property (readonly, getter=isLowPower) BOOL lowPower;
@property (readonly, getter=isHeadless) BOOL headless;
- (BOOL)supportsFamily:(MTLGPUFamily)f;
- (BOOL)supportsTextureSampleCount:(NSUInteger)n;
- (id<MTLCommandQueue>)newCommandQueue;
- (id<MTLCommandQueue>)newCommandQueueWithMaxCommandBufferCount:(NSUInteger)n;
- (id<MTLBuffer>)newBufferWithLength:(NSUInteger)length options:(MTLResourceOptions)options;
- (id<MTLBuffer>)newBufferWithBytes:(const void *)p length:(NSUInteger)length options:(MTLResourceOptions)options;
- (id<MTLBuffer>)newBufferWithBytesNoCopy:(void *)p length:(NSUInteger)length options:(MTLResourceOptions)options deallocator:(void (^)(void *p, NSUInteger len))d;
- (id<MTLTexture>)newTextureWithDescriptor:(MTLTextureDescriptor *)d;
- (id<MTLTexture>)newTextureWithDescriptor:(MTLTextureDescriptor *)d iosurface:(id)surface plane:(NSUInteger)plane;
- (id<MTLSamplerState>)newSamplerStateWithDescriptor:(MTLSamplerDescriptor *)d;
- (id<MTLDepthStencilState>)newDepthStencilStateWithDescriptor:(MTLDepthStencilDescriptor *)d;
- (id<MTLLibrary>)newDefaultLibrary;
- (id<MTLLibrary>)newLibraryWithSource:(NSString *)src options:(MTLCompileOptions *)opts error:(NSError **)err;
- (void)newLibraryWithSource:(NSString *)src options:(MTLCompileOptions *)opts completionHandler:(void (^)(id<MTLLibrary> lib, NSError *err))h;
- (id<MTLLibrary>)newLibraryWithURL:(NSURL *)url error:(NSError **)err;
- (id<MTLRenderPipelineState>)newRenderPipelineStateWithDescriptor:(MTLRenderPipelineDescriptor *)d error:(NSError **)err;
- (id<MTLComputePipelineState>)newComputePipelineStateWithFunction:(id<MTLFunction>)f error:(NSError **)err;
- (id<MTLHeap>)newHeapWithDescriptor:(MTLHeapDescriptor *)d;
- (id<MTLFence>)newFence;
- (id<MTLEvent>)newEvent;
- (id<MTLSharedEvent>)newSharedEvent;
@end
FOUNDATION_EXPORT id<MTLDevice> MTLCreateSystemDefaultDevice(void);
FOUNDATION_EXPORT NSArray<id<MTLDevice>> *MTLCopyAllDevices(void);

/* ---- descritores */
@interface MTLTextureDescriptor : NSObject <NSCopying>
+ (MTLTextureDescriptor *)texture2DDescriptorWithPixelFormat:(MTLPixelFormat)f width:(NSUInteger)w height:(NSUInteger)h mipmapped:(BOOL)m;
+ (MTLTextureDescriptor *)textureCubeDescriptorWithPixelFormat:(MTLPixelFormat)f size:(NSUInteger)s mipmapped:(BOOL)m;
@property MTLTextureType textureType;
@property MTLPixelFormat pixelFormat;
@property NSUInteger width, height, depth, mipmapLevelCount, sampleCount, arrayLength;
@property MTLResourceOptions resourceOptions;
@property MTLCPUCacheMode cpuCacheMode;
@property MTLStorageMode storageMode;
@property MTLTextureUsage usage;
@end
@interface MTLSamplerDescriptor : NSObject <NSCopying>
@property MTLSamplerMinMagFilter minFilter, magFilter;
@property MTLSamplerMipFilter mipFilter;
@property NSUInteger maxAnisotropy;
@property MTLSamplerAddressMode sAddressMode, tAddressMode, rAddressMode;
@property MTLSamplerBorderColor borderColor;
@property BOOL normalizedCoordinates;
@property float lodMinClamp, lodMaxClamp;
@property MTLCompareFunction compareFunction;
@property (copy) NSString *label;
@end
@interface MTLStencilDescriptor : NSObject <NSCopying>
@property MTLCompareFunction stencilCompareFunction;
@property MTLStencilOperation stencilFailureOperation, depthFailureOperation, depthStencilPassOperation;
@property uint32_t readMask, writeMask;
@end
@interface MTLDepthStencilDescriptor : NSObject <NSCopying>
@property MTLCompareFunction depthCompareFunction;
@property (getter=isDepthWriteEnabled) BOOL depthWriteEnabled;
@property (copy) MTLStencilDescriptor *frontFaceStencil, *backFaceStencil;
@property (copy) NSString *label;
@end
@interface MTLRenderPassAttachmentDescriptor : NSObject <NSCopying>
@property (strong) id<MTLTexture> texture;
@property NSUInteger level, slice, depthPlane;
@property (strong) id<MTLTexture> resolveTexture;
@property NSUInteger resolveLevel, resolveSlice, resolveDepthPlane;
@property MTLLoadAction loadAction;
@property MTLStoreAction storeAction;
@end
@interface MTLRenderPassColorAttachmentDescriptor : MTLRenderPassAttachmentDescriptor
@property MTLClearColor clearColor;
@end
@interface MTLRenderPassDepthAttachmentDescriptor : MTLRenderPassAttachmentDescriptor
@property double clearDepth;
@end
@interface MTLRenderPassStencilAttachmentDescriptor : MTLRenderPassAttachmentDescriptor
@property uint32_t clearStencil;
@end
@interface MTLRenderPassColorAttachmentDescriptorArray : NSObject
- (MTLRenderPassColorAttachmentDescriptor *)objectAtIndexedSubscript:(NSUInteger)i;
- (void)setObject:(MTLRenderPassColorAttachmentDescriptor *)a atIndexedSubscript:(NSUInteger)i;
@end
@interface MTLRenderPassDescriptor : NSObject <NSCopying>
+ (MTLRenderPassDescriptor *)renderPassDescriptor;
@property (readonly) MTLRenderPassColorAttachmentDescriptorArray *colorAttachments;
@property (copy) MTLRenderPassDepthAttachmentDescriptor *depthAttachment;
@property (copy) MTLRenderPassStencilAttachmentDescriptor *stencilAttachment;
@property NSUInteger renderTargetWidth, renderTargetHeight, renderTargetArrayLength;
@end
@interface MTLRenderPipelineColorAttachmentDescriptor : NSObject <NSCopying>
@property MTLPixelFormat pixelFormat;
@property (getter=isBlendingEnabled) BOOL blendingEnabled;
@property MTLBlendFactor sourceRGBBlendFactor, destinationRGBBlendFactor, sourceAlphaBlendFactor, destinationAlphaBlendFactor;
@property MTLBlendOperation rgbBlendOperation, alphaBlendOperation;
@property MTLColorWriteMask writeMask;
@end
@interface MTLRenderPipelineColorAttachmentDescriptorArray : NSObject
- (MTLRenderPipelineColorAttachmentDescriptor *)objectAtIndexedSubscript:(NSUInteger)i;
- (void)setObject:(MTLRenderPipelineColorAttachmentDescriptor *)a atIndexedSubscript:(NSUInteger)i;
@end
@interface MTLVertexAttributeDescriptor : NSObject <NSCopying>
@property MTLVertexFormat format;
@property NSUInteger offset, bufferIndex;
@end
@interface MTLVertexAttributeDescriptorArray : NSObject
- (MTLVertexAttributeDescriptor *)objectAtIndexedSubscript:(NSUInteger)i;
- (void)setObject:(MTLVertexAttributeDescriptor *)a atIndexedSubscript:(NSUInteger)i;
@end
@interface MTLVertexBufferLayoutDescriptor : NSObject <NSCopying>
@property NSUInteger stride;
@property MTLVertexStepFunction stepFunction;
@property NSUInteger stepRate;
@end
@interface MTLVertexBufferLayoutDescriptorArray : NSObject
- (MTLVertexBufferLayoutDescriptor *)objectAtIndexedSubscript:(NSUInteger)i;
- (void)setObject:(MTLVertexBufferLayoutDescriptor *)a atIndexedSubscript:(NSUInteger)i;
@end
@interface MTLVertexDescriptor : NSObject <NSCopying>
+ (MTLVertexDescriptor *)vertexDescriptor;
@property (readonly) MTLVertexAttributeDescriptorArray *attributes;
@property (readonly) MTLVertexBufferLayoutDescriptorArray *layouts;
- (void)reset;
@end
@interface MTLRenderPipelineDescriptor : NSObject <NSCopying>
@property (copy) NSString *label;
@property (strong) id<MTLFunction> vertexFunction, fragmentFunction;
@property (copy) MTLVertexDescriptor *vertexDescriptor;
@property NSUInteger sampleCount, rasterSampleCount;
@property (getter=isAlphaToCoverageEnabled) BOOL alphaToCoverageEnabled;
@property (getter=isRasterizationEnabled) BOOL rasterizationEnabled;
@property (readonly) MTLRenderPipelineColorAttachmentDescriptorArray *colorAttachments;
@property MTLPixelFormat depthAttachmentPixelFormat, stencilAttachmentPixelFormat;
- (void)reset;
@end
@interface MTLComputePipelineDescriptor : NSObject <NSCopying>
@property (copy) NSString *label;
@property (strong) id<MTLFunction> computeFunction;
@property NSUInteger maxTotalThreadsPerThreadgroup;
@end
@interface MTLCompileOptions : NSObject <NSCopying>
@property (copy) NSDictionary<NSString *, NSObject *> *preprocessorMacros;
@property BOOL fastMathEnabled;
@property MTLLanguageVersion languageVersion;
@end
@interface MTLFunctionConstantValues : NSObject <NSCopying>
@end
@interface MTLHeapDescriptor : NSObject <NSCopying>
@property NSUInteger size;
@property MTLStorageMode storageMode;
@end
