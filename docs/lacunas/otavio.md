# Lacunas de Otavio (OpenGL, Metal, Core Animation)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Otavio (OpenGL, Metal, Core Animation) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**210** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| CoreVideo.framework | `_kCVPixelBufferIOSurfaceOpenGLTextureCompatibilityKey` | 12 | calibre, iina, krita, obs |
| CoreVideo.framework | `_CVBufferGetAttachments` | 8 | handbrake-app, iina, krita, obs |
| CoreVideo.framework | `_CVDisplayLinkSetCurrentCGDisplayFromOpenGLContext` | 8 | krita, love, moonlight, openrct2 |
| CoreVideo.framework | `_kCVImageBufferColorPrimaries_P22` | 8 | firefox, floorp, librewolf, mullvad-browser |
| AppKit.framework | `_NSOpenGLGetOption` | 7 | bitwarden, gemini, monitorcontrol, mos |
| AppKit.framework | `_NSOpenGLGetVersion` | 7 | bitwarden, gemini, monitorcontrol, mos |
| AppKit.framework | `_NSOpenGLSetOption` | 7 | bitwarden, gemini, monitorcontrol, mos |
| CoreVideo.framework | `_CVImageBufferGetEncodedSize` | 7 | adobe-acrobat-reader, calibre, iina, jellyfin |
| CoreVideo.framework | `_CVOpenGLTextureCacheCreate` | 5 | calibre, nextcloud, shotcut, telegram |
| CoreVideo.framework | `_CVOpenGLTextureCacheCreateTextureFromImage` | 5 | calibre, nextcloud, shotcut, telegram |
| CoreVideo.framework | `_CVOpenGLTextureGetName` | 5 | calibre, nextcloud, shotcut, telegram |
| CoreVideo.framework | `_CVOpenGLTextureCacheFlush` | 4 | calibre, shotcut, telegram, wireshark-app |
| CoreVideo.framework | `_CVOpenGLTextureRelease` | 3 | calibre, shotcut, wireshark-app |
| OpenCL.framework | `_clBuildProgram` | 3 | jellyfin, shotcut, stremio |
| OpenCL.framework | `_clCreateBuffer` | 3 | jellyfin, shotcut, stremio |
| OpenCL.framework | `_clCreateCommandQueue` | 3 | jellyfin, shotcut, stremio |
| OpenCL.framework | `_clCreateContext` | 3 | jellyfin, shotcut, stremio |
| OpenCL.framework | `_clCreateImage` | 3 | jellyfin, shotcut, stremio |
| OpenCL.framework | `_clCreateKernel` | 3 | jellyfin, shotcut, stremio |
| OpenCL.framework | `_clCreateProgramWithSource` | 3 | jellyfin, shotcut, stremio |
| OpenCL.framework | `_clEnqueueCopyImage` | 3 | jellyfin, shotcut, stremio |
| OpenCL.framework | `_clEnqueueFillBuffer` | 3 | jellyfin, shotcut, stremio |
| OpenCL.framework | `_clEnqueueMapBuffer` | 3 | jellyfin, shotcut, stremio |
| OpenCL.framework | `_clEnqueueMapImage` | 3 | jellyfin, shotcut, stremio |
| OpenCL.framework | `_clEnqueueNDRangeKernel` | 3 | jellyfin, shotcut, stremio |
| OpenCL.framework | `_clEnqueueReadBuffer` | 3 | jellyfin, shotcut, stremio |
| OpenCL.framework | `_clEnqueueReadImage` | 3 | jellyfin, shotcut, stremio |
| OpenCL.framework | `_clEnqueueUnmapMemObject` | 3 | jellyfin, shotcut, stremio |
| OpenCL.framework | `_clEnqueueWriteBuffer` | 3 | jellyfin, shotcut, stremio |
| OpenCL.framework | `_clEnqueueWriteImage` | 3 | jellyfin, shotcut, stremio |
| OpenCL.framework | `_clFinish` | 3 | jellyfin, shotcut, stremio |
| OpenCL.framework | `_clFlush` | 3 | jellyfin, shotcut, stremio |
| OpenCL.framework | `_clGetDeviceIDs` | 3 | jellyfin, shotcut, stremio |
| OpenCL.framework | `_clGetDeviceInfo` | 3 | jellyfin, shotcut, stremio |
| OpenCL.framework | `_clGetEventProfilingInfo` | 3 | jellyfin, shotcut, stremio |
| OpenCL.framework | `_clGetImageInfo` | 3 | jellyfin, shotcut, stremio |
| OpenCL.framework | `_clGetMemObjectInfo` | 3 | jellyfin, shotcut, stremio |
| OpenCL.framework | `_clGetPlatformIDs` | 3 | jellyfin, shotcut, stremio |
| OpenCL.framework | `_clGetPlatformInfo` | 3 | jellyfin, shotcut, stremio |
| OpenCL.framework | `_clGetProgramBuildInfo` | 3 | jellyfin, shotcut, stremio |
| OpenCL.framework | `_clGetSupportedImageFormats` | 3 | jellyfin, shotcut, stremio |
| OpenCL.framework | `_clReleaseCommandQueue` | 3 | jellyfin, shotcut, stremio |
| OpenCL.framework | `_clReleaseContext` | 3 | jellyfin, shotcut, stremio |
| OpenCL.framework | `_clReleaseEvent` | 3 | jellyfin, shotcut, stremio |
| OpenCL.framework | `_clReleaseKernel` | 3 | jellyfin, shotcut, stremio |
| OpenCL.framework | `_clReleaseMemObject` | 3 | jellyfin, shotcut, stremio |
| OpenCL.framework | `_clReleaseProgram` | 3 | jellyfin, shotcut, stremio |
| OpenCL.framework | `_clRetainCommandQueue` | 3 | jellyfin, shotcut, stremio |
| OpenCL.framework | `_clSetKernelArg` | 3 | jellyfin, shotcut, stremio |
| OpenCL.framework | `_clWaitForEvents` | 3 | jellyfin, shotcut, stremio |
| AppKit.framework | `_OBJC_CLASS_$_NSOpenGLLayer` | 2 | epic-games, zoom |
| AppKit.framework | `_OBJC_METACLASS_$_NSOpenGLLayer` | 2 | epic-games, zoom |
| CoreVideo.framework | `_CVImageBufferGetColorSpace` | 2 | cleanshot, microsoft-teams |
| CoreVideo.framework | `_kCVImageBufferFieldDetailTemporalBottomFirst` | 2 | handbrake-app, libndi |
| CoreVideo.framework | `_kCVImageBufferFieldDetailTemporalTopFirst` | 2 | handbrake-app, libndi |
| Metal.framework | `_OBJC_CLASS_$_MTLAccelerationStructureBoundingBoxGeometryDescriptor` | 2 | blender, utm |
| Metal.framework | `_OBJC_CLASS_$_MTLAccelerationStructureTriangleGeometryDescriptor` | 2 | blender, utm |
| Metal.framework | `_OBJC_CLASS_$_MTLInstanceAccelerationStructureDescriptor` | 2 | blender, utm |
| Metal.framework | `_OBJC_CLASS_$_MTLIntersectionFunctionDescriptor` | 2 | blender, utm |
| Metal.framework | `_OBJC_CLASS_$_MTLIntersectionFunctionTableDescriptor` | 2 | blender, utm |
| Metal.framework | `_OBJC_CLASS_$_MTLPrimitiveAccelerationStructureDescriptor` | 2 | blender, utm |
| OpenCL.framework | `_clEnqueueCopyBufferToImage` | 2 | jellyfin, shotcut |
| OpenGL.framework | `_gluCheckExtension` | 2 | cryptomator, lmms |
| OpenGL.framework | `_gluDeleteTess` | 2 | kicad, openscad |
| OpenGL.framework | `_gluErrorString` | 2 | kicad, openscad |
| OpenGL.framework | `_gluNewTess` | 2 | kicad, openscad |
| OpenGL.framework | `_gluPerspective` | 2 | kicad, openscad |
| OpenGL.framework | `_gluTessBeginContour` | 2 | kicad, openscad |
| OpenGL.framework | `_gluTessBeginPolygon` | 2 | kicad, openscad |
| OpenGL.framework | `_gluTessCallback` | 2 | kicad, openscad |
| OpenGL.framework | `_gluTessEndContour` | 2 | kicad, openscad |
| OpenGL.framework | `_gluTessEndPolygon` | 2 | kicad, openscad |
| OpenGL.framework | `_gluTessNormal` | 2 | kicad, openscad |
| OpenGL.framework | `_gluTessProperty` | 2 | kicad, openscad |
| OpenGL.framework | `_gluTessVertex` | 2 | kicad, openscad |
| OpenGL.framework | `_gluUnProject` | 2 | lmms, openscad |
| QuartzCore.framework | `_OBJC_CLASS_$_CABackdropLayer` | 2 | bettertouchtool, zoom |
| CoreVideo.framework | `_CVImageBufferGetDisplaySize` | 1 | adobe-acrobat-reader |
| CoreVideo.framework | `_CVOpenGLTextureGetTarget` | 1 | telegram |
| CoreVideo.framework | `_kCVImageBufferChromaLocation_DV420` | 1 | vlc |
| CoreVideo.framework | `_kCVImageBufferDisplayDimensionsKey` | 1 | handbrake-app |
| CoreVideo.framework | `_kCVImageBufferDisplayHeightKey` | 1 | handbrake-app |
| CoreVideo.framework | `_kCVImageBufferDisplayWidthKey` | 1 | handbrake-app |
| CoreVideo.framework | `_kCVImageBufferFieldCountKey` | 1 | libndi |
| CoreVideo.framework | `_kCVImageBufferFieldDetailKey` | 1 | libndi |
| CoreVideo.framework | `_kCVImageBufferFieldDetailSpatialFirstLineEarly` | 1 | libndi |
| CoreVideo.framework | `_kCVImageBufferFieldDetailSpatialFirstLineLate` | 1 | libndi |
| CoreVideo.framework | `_kCVPixelBufferExtendedPixelsBottomKey` | 1 | handbrake-app |
| CoreVideo.framework | `_kCVPixelBufferExtendedPixelsRightKey` | 1 | handbrake-app |
| CoreVideo.framework | `_kCVPixelFormatComponentRange_VideoRange` | 1 | handbrake-app |
| CoreVideo.framework | `_kCVPixelFormatOpenGLCompatibility` | 1 | telegram |
| Metal.framework | `_MTLMakeShaderCacheWritableByAllUsers` | 1 | utm |
| Metal.framework | `_MTLSetShaderCachePath` | 1 | utm |
| Metal.framework | `_MTLTextureSwizzleChannelsToKey` | 1 | utm |
| Metal.framework | `_MTLTextureSwizzleKeyToChannels` | 1 | utm |
| Metal.framework | `_OBJC_CLASS_$_MTLAccelerationStructureAllocationDescriptor` | 1 | utm |
| Metal.framework | `_OBJC_CLASS_$_MTLAccelerationStructurePassDescriptor` | 1 | utm |
| Metal.framework | `_OBJC_CLASS_$_MTLMeshRenderPipelineDescriptor` | 1 | utm |
| Metal.framework | `_OBJC_CLASS_$_MTLVisibleFunctionTableDescriptor` | 1 | utm |
| OpenCL.framework | `_clCompileProgram` | 1 | shotcut |
| OpenCL.framework | `_clCreateContextFromType` | 1 | shotcut |
| OpenCL.framework | `_clCreateKernelsInProgram` | 1 | shotcut |
| OpenCL.framework | `_clCreateProgramWithBinary` | 1 | shotcut |
| OpenCL.framework | `_clCreateProgramWithBuiltInKernels` | 1 | shotcut |
| OpenCL.framework | `_clCreateSampler` | 1 | shotcut |
| OpenCL.framework | `_clCreateSubBuffer` | 1 | shotcut |
| OpenCL.framework | `_clCreateSubDevices` | 1 | shotcut |
| OpenCL.framework | `_clCreateUserEvent` | 1 | shotcut |
| OpenCL.framework | `_clEnqueueBarrierWithWaitList` | 1 | shotcut |
| OpenCL.framework | `_clEnqueueCopyBuffer` | 1 | shotcut |
| OpenCL.framework | `_clEnqueueCopyBufferRect` | 1 | shotcut |
| OpenCL.framework | `_clEnqueueCopyImageToBuffer` | 1 | shotcut |
| OpenCL.framework | `_clEnqueueFillImage` | 1 | shotcut |
| OpenCL.framework | `_clEnqueueMarkerWithWaitList` | 1 | shotcut |
| OpenCL.framework | `_clEnqueueMigrateMemObjects` | 1 | shotcut |
| OpenCL.framework | `_clEnqueueNativeKernel` | 1 | shotcut |
| OpenCL.framework | `_clEnqueueReadBufferRect` | 1 | shotcut |
| OpenCL.framework | `_clEnqueueTask` | 1 | shotcut |
| OpenCL.framework | `_clEnqueueWriteBufferRect` | 1 | shotcut |
| OpenCL.framework | `_clGetCommandQueueInfo` | 1 | shotcut |
| OpenCL.framework | `_clGetContextInfo` | 1 | shotcut |
| OpenCL.framework | `_clGetEventInfo` | 1 | shotcut |
| OpenCL.framework | `_clGetExtensionFunctionAddressForPlatform` | 1 | shotcut |
| OpenCL.framework | `_clGetKernelArgInfo` | 1 | shotcut |
| OpenCL.framework | `_clGetKernelInfo` | 1 | shotcut |
| OpenCL.framework | `_clGetKernelWorkGroupInfo` | 1 | shotcut |
| OpenCL.framework | `_clGetProgramInfo` | 1 | shotcut |
| OpenCL.framework | `_clGetSamplerInfo` | 1 | shotcut |
| OpenCL.framework | `_clLinkProgram` | 1 | shotcut |
| OpenCL.framework | `_clReleaseDevice` | 1 | shotcut |
| OpenCL.framework | `_clReleaseSampler` | 1 | shotcut |
| OpenCL.framework | `_clRetainContext` | 1 | shotcut |
| OpenCL.framework | `_clRetainDevice` | 1 | shotcut |
| OpenCL.framework | `_clRetainEvent` | 1 | shotcut |
| OpenCL.framework | `_clRetainKernel` | 1 | shotcut |
| OpenCL.framework | `_clRetainMemObject` | 1 | shotcut |
| OpenCL.framework | `_clRetainProgram` | 1 | shotcut |
| OpenCL.framework | `_clRetainSampler` | 1 | shotcut |
| OpenCL.framework | `_clSetEventCallback` | 1 | shotcut |
| OpenCL.framework | `_clSetMemObjectDestructorCallback` | 1 | shotcut |
| OpenCL.framework | `_clSetUserEventStatus` | 1 | shotcut |
| OpenCL.framework | `_clUnloadPlatformCompiler` | 1 | shotcut |
| OpenGL.framework | `_CGLCopyContext` | 1 | xquartz |
| OpenGL.framework | `_glColorSubTable` | 1 | xquartz |
| OpenGL.framework | `_glColorTable` | 1 | xquartz |
| OpenGL.framework | `_glColorTableParameterfv` | 1 | xquartz |
| OpenGL.framework | `_glColorTableParameteriv` | 1 | xquartz |
| OpenGL.framework | `_glConvolutionFilter1D` | 1 | xquartz |
| OpenGL.framework | `_glConvolutionFilter2D` | 1 | xquartz |
| OpenGL.framework | `_glConvolutionParameterf` | 1 | xquartz |
| OpenGL.framework | `_glConvolutionParameterfv` | 1 | xquartz |
| OpenGL.framework | `_glConvolutionParameteri` | 1 | xquartz |
| OpenGL.framework | `_glConvolutionParameteriv` | 1 | xquartz |
| OpenGL.framework | `_glCopyColorSubTable` | 1 | xquartz |
| OpenGL.framework | `_glCopyColorTable` | 1 | xquartz |
| OpenGL.framework | `_glCopyConvolutionFilter1D` | 1 | xquartz |
| OpenGL.framework | `_glCopyConvolutionFilter2D` | 1 | xquartz |
| OpenGL.framework | `_glGetColorTable` | 1 | xquartz |
| OpenGL.framework | `_glGetColorTableParameterfv` | 1 | xquartz |
| OpenGL.framework | `_glGetColorTableParameteriv` | 1 | xquartz |
| OpenGL.framework | `_glGetConvolutionFilter` | 1 | xquartz |
| OpenGL.framework | `_glGetConvolutionParameterfv` | 1 | xquartz |
| OpenGL.framework | `_glGetConvolutionParameteriv` | 1 | xquartz |
| OpenGL.framework | `_glGetHistogram` | 1 | xquartz |
| OpenGL.framework | `_glGetHistogramParameterfv` | 1 | xquartz |
| OpenGL.framework | `_glGetHistogramParameteriv` | 1 | xquartz |
| OpenGL.framework | `_glGetMinmax` | 1 | xquartz |
| OpenGL.framework | `_glGetMinmaxParameterfv` | 1 | xquartz |
| OpenGL.framework | `_glGetMinmaxParameteriv` | 1 | xquartz |
| OpenGL.framework | `_glGetSeparableFilter` | 1 | xquartz |
| OpenGL.framework | `_glHistogram` | 1 | xquartz |
| OpenGL.framework | `_glMinmax` | 1 | xquartz |
| OpenGL.framework | `_glResetHistogram` | 1 | xquartz |
| OpenGL.framework | `_glResetMinmax` | 1 | xquartz |
| OpenGL.framework | `_glSeparableFilter2D` | 1 | xquartz |
| OpenGL.framework | `_gluBuild2DMipmaps` | 1 | adobe-acrobat-reader |
| OpenGL.framework | `_gluCylinder` | 1 | kicad |
| OpenGL.framework | `_gluDeleteQuadric` | 1 | kicad |
| OpenGL.framework | `_gluDisk` | 1 | kicad |
| OpenGL.framework | `_gluLookAt` | 1 | openscad |
| OpenGL.framework | `_gluNewQuadric` | 1 | kicad |
| OpenGL.framework | `_gluOrtho2D` | 1 | adobe-acrobat-reader |
| OpenGL.framework | `_gluProject` | 1 | openscad |
| OpenGL.framework | `_gluQuadricDrawStyle` | 1 | kicad |
| OpenGL.framework | `_gluQuadricNormals` | 1 | kicad |
| OpenGL.framework | `_gluSphere` | 1 | kicad |
| QuartzCore.framework | `_OBJC_CLASS_$_CAFilter` | 1 | zoom |
| QuartzCore.framework | `_kCAFilterColorBrightness` | 1 | zoom |
| QuartzCore.framework | `_kCAFilterColorBurnBlendMode` | 1 | zoom |
| QuartzCore.framework | `_kCAFilterColorDodgeBlendMode` | 1 | zoom |
| QuartzCore.framework | `_kCAFilterColorSaturate` | 1 | zoom |
| QuartzCore.framework | `_kCAFilterDarkenBlendMode` | 1 | zoom |
| QuartzCore.framework | `_kCAFilterDifferenceBlendMode` | 1 | zoom |
| QuartzCore.framework | `_kCAFilterDivideBlendMode` | 1 | zoom |
| QuartzCore.framework | `_kCAFilterExclusionBlendMode` | 1 | zoom |
| QuartzCore.framework | `_kCAFilterGaussianBlur` | 1 | zoom |
| QuartzCore.framework | `_kCAFilterHardLightBlendMode` | 1 | zoom |
| QuartzCore.framework | `_kCAFilterLightenBlendMode` | 1 | zoom |
| QuartzCore.framework | `_kCAFilterLinearBurnBlendMode` | 1 | zoom |
| QuartzCore.framework | `_kCAFilterLinearDodgeBlendMode` | 1 | zoom |
| QuartzCore.framework | `_kCAFilterLinearLightBlendMode` | 1 | zoom |
| QuartzCore.framework | `_kCAFilterMultiplyBlendMode` | 1 | zoom |
| QuartzCore.framework | `_kCAFilterOverlayBlendMode` | 1 | zoom |
| QuartzCore.framework | `_kCAFilterPinLightBlendMode` | 1 | zoom |
| QuartzCore.framework | `_kCAFilterScreenBlendMode` | 1 | zoom |
| QuartzCore.framework | `_kCAFilterSoftLightBlendMode` | 1 | zoom |
| QuartzCore.framework | `_kCAFilterSubtractBlendMode` | 1 | zoom |
| libswiftMetal.dylib | `_$sSo23MTLRenderCommandEncoderP5MetalE16setVertexBuffers_7offsets5rangeySaySo9MTLBuffer_pSgG_SaySiGSnySiGtF` | 1 | obs |
| libswiftMetal.dylib | `_$sSo23MTLRenderCommandEncoderP5MetalE19setFragmentTextures_5rangeySaySo10MTLTexture_pSgG_SnySiGtF` | 1 | raycast |
| libswiftQuartzCore.dylib | `_$sSo13CATransform3DV10QuartzCoreE34_conditionallyBridgeFromObjectiveC_6resultSbSo7NSValueC_ABSgztFZ` | 1 | mac-mouse-fix |
