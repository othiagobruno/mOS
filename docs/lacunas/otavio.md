# Lacunas de Otavio (OpenGL, Metal, Core Animation)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Otavio (OpenGL, Metal, Core Animation) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**191** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| CoreVideo.framework | `_CVDisplayLinkSetCurrentCGDisplayFromOpenGLContext` | 6 | krita, love, openrct2, shotcut |
| CoreVideo.framework | `_kCVImageBufferColorPrimaries_P22` | 6 | firefox, floorp, mullvad-browser, thunderbird |
| CoreVideo.framework | `_kCVPixelBufferIOSurfaceOpenGLTextureCompatibilityKey` | 6 | iina, krita, obs, shotcut |
| CoreVideo.framework | `_CVBufferGetAttachments` | 5 | iina, krita, obs, vlc |
| AppKit.framework | `_NSOpenGLGetOption` | 3 | bitwarden, monitorcontrol, mos |
| AppKit.framework | `_NSOpenGLGetVersion` | 3 | bitwarden, monitorcontrol, mos |
| AppKit.framework | `_NSOpenGLSetOption` | 3 | bitwarden, monitorcontrol, mos |
| CoreVideo.framework | `_CVImageBufferGetEncodedSize` | 3 | iina, obs, shotcut |
| CoreVideo.framework | `_CVOpenGLTextureCacheCreate` | 2 | shotcut, wireshark-app |
| CoreVideo.framework | `_CVOpenGLTextureCacheCreateTextureFromImage` | 2 | shotcut, wireshark-app |
| CoreVideo.framework | `_CVOpenGLTextureCacheFlush` | 2 | shotcut, wireshark-app |
| CoreVideo.framework | `_CVOpenGLTextureGetName` | 2 | shotcut, wireshark-app |
| CoreVideo.framework | `_CVOpenGLTextureRelease` | 2 | shotcut, wireshark-app |
| Metal.framework | `_OBJC_CLASS_$_MTLAccelerationStructureBoundingBoxGeometryDescriptor` | 2 | blender, utm |
| Metal.framework | `_OBJC_CLASS_$_MTLAccelerationStructureTriangleGeometryDescriptor` | 2 | blender, utm |
| Metal.framework | `_OBJC_CLASS_$_MTLInstanceAccelerationStructureDescriptor` | 2 | blender, utm |
| Metal.framework | `_OBJC_CLASS_$_MTLIntersectionFunctionDescriptor` | 2 | blender, utm |
| Metal.framework | `_OBJC_CLASS_$_MTLIntersectionFunctionTableDescriptor` | 2 | blender, utm |
| Metal.framework | `_OBJC_CLASS_$_MTLPrimitiveAccelerationStructureDescriptor` | 2 | blender, utm |
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
| AppKit.framework | `_OBJC_CLASS_$_NSOpenGLLayer` | 1 | zoom |
| AppKit.framework | `_OBJC_METACLASS_$_NSOpenGLLayer` | 1 | zoom |
| CoreVideo.framework | `_kCVImageBufferChromaLocation_DV420` | 1 | vlc |
| Metal.framework | `_MTLMakeShaderCacheWritableByAllUsers` | 1 | utm |
| Metal.framework | `_MTLSetShaderCachePath` | 1 | utm |
| Metal.framework | `_MTLTextureSwizzleChannelsToKey` | 1 | utm |
| Metal.framework | `_MTLTextureSwizzleKeyToChannels` | 1 | utm |
| Metal.framework | `_OBJC_CLASS_$_MTLAccelerationStructureAllocationDescriptor` | 1 | utm |
| Metal.framework | `_OBJC_CLASS_$_MTLAccelerationStructurePassDescriptor` | 1 | utm |
| Metal.framework | `_OBJC_CLASS_$_MTLMeshRenderPipelineDescriptor` | 1 | utm |
| Metal.framework | `_OBJC_CLASS_$_MTLVisibleFunctionTableDescriptor` | 1 | utm |
| OpenCL.framework | `_clBuildProgram` | 1 | shotcut |
| OpenCL.framework | `_clCompileProgram` | 1 | shotcut |
| OpenCL.framework | `_clCreateBuffer` | 1 | shotcut |
| OpenCL.framework | `_clCreateCommandQueue` | 1 | shotcut |
| OpenCL.framework | `_clCreateContext` | 1 | shotcut |
| OpenCL.framework | `_clCreateContextFromType` | 1 | shotcut |
| OpenCL.framework | `_clCreateImage` | 1 | shotcut |
| OpenCL.framework | `_clCreateKernel` | 1 | shotcut |
| OpenCL.framework | `_clCreateKernelsInProgram` | 1 | shotcut |
| OpenCL.framework | `_clCreateProgramWithBinary` | 1 | shotcut |
| OpenCL.framework | `_clCreateProgramWithBuiltInKernels` | 1 | shotcut |
| OpenCL.framework | `_clCreateProgramWithSource` | 1 | shotcut |
| OpenCL.framework | `_clCreateSampler` | 1 | shotcut |
| OpenCL.framework | `_clCreateSubBuffer` | 1 | shotcut |
| OpenCL.framework | `_clCreateSubDevices` | 1 | shotcut |
| OpenCL.framework | `_clCreateUserEvent` | 1 | shotcut |
| OpenCL.framework | `_clEnqueueBarrierWithWaitList` | 1 | shotcut |
| OpenCL.framework | `_clEnqueueCopyBuffer` | 1 | shotcut |
| OpenCL.framework | `_clEnqueueCopyBufferRect` | 1 | shotcut |
| OpenCL.framework | `_clEnqueueCopyBufferToImage` | 1 | shotcut |
| OpenCL.framework | `_clEnqueueCopyImage` | 1 | shotcut |
| OpenCL.framework | `_clEnqueueCopyImageToBuffer` | 1 | shotcut |
| OpenCL.framework | `_clEnqueueFillBuffer` | 1 | shotcut |
| OpenCL.framework | `_clEnqueueFillImage` | 1 | shotcut |
| OpenCL.framework | `_clEnqueueMapBuffer` | 1 | shotcut |
| OpenCL.framework | `_clEnqueueMapImage` | 1 | shotcut |
| OpenCL.framework | `_clEnqueueMarkerWithWaitList` | 1 | shotcut |
| OpenCL.framework | `_clEnqueueMigrateMemObjects` | 1 | shotcut |
| OpenCL.framework | `_clEnqueueNDRangeKernel` | 1 | shotcut |
| OpenCL.framework | `_clEnqueueNativeKernel` | 1 | shotcut |
| OpenCL.framework | `_clEnqueueReadBuffer` | 1 | shotcut |
| OpenCL.framework | `_clEnqueueReadBufferRect` | 1 | shotcut |
| OpenCL.framework | `_clEnqueueReadImage` | 1 | shotcut |
| OpenCL.framework | `_clEnqueueTask` | 1 | shotcut |
| OpenCL.framework | `_clEnqueueUnmapMemObject` | 1 | shotcut |
| OpenCL.framework | `_clEnqueueWriteBuffer` | 1 | shotcut |
| OpenCL.framework | `_clEnqueueWriteBufferRect` | 1 | shotcut |
| OpenCL.framework | `_clEnqueueWriteImage` | 1 | shotcut |
| OpenCL.framework | `_clFinish` | 1 | shotcut |
| OpenCL.framework | `_clFlush` | 1 | shotcut |
| OpenCL.framework | `_clGetCommandQueueInfo` | 1 | shotcut |
| OpenCL.framework | `_clGetContextInfo` | 1 | shotcut |
| OpenCL.framework | `_clGetDeviceIDs` | 1 | shotcut |
| OpenCL.framework | `_clGetDeviceInfo` | 1 | shotcut |
| OpenCL.framework | `_clGetEventInfo` | 1 | shotcut |
| OpenCL.framework | `_clGetEventProfilingInfo` | 1 | shotcut |
| OpenCL.framework | `_clGetExtensionFunctionAddressForPlatform` | 1 | shotcut |
| OpenCL.framework | `_clGetImageInfo` | 1 | shotcut |
| OpenCL.framework | `_clGetKernelArgInfo` | 1 | shotcut |
| OpenCL.framework | `_clGetKernelInfo` | 1 | shotcut |
| OpenCL.framework | `_clGetKernelWorkGroupInfo` | 1 | shotcut |
| OpenCL.framework | `_clGetMemObjectInfo` | 1 | shotcut |
| OpenCL.framework | `_clGetPlatformIDs` | 1 | shotcut |
| OpenCL.framework | `_clGetPlatformInfo` | 1 | shotcut |
| OpenCL.framework | `_clGetProgramBuildInfo` | 1 | shotcut |
| OpenCL.framework | `_clGetProgramInfo` | 1 | shotcut |
| OpenCL.framework | `_clGetSamplerInfo` | 1 | shotcut |
| OpenCL.framework | `_clGetSupportedImageFormats` | 1 | shotcut |
| OpenCL.framework | `_clLinkProgram` | 1 | shotcut |
| OpenCL.framework | `_clReleaseCommandQueue` | 1 | shotcut |
| OpenCL.framework | `_clReleaseContext` | 1 | shotcut |
| OpenCL.framework | `_clReleaseDevice` | 1 | shotcut |
| OpenCL.framework | `_clReleaseEvent` | 1 | shotcut |
| OpenCL.framework | `_clReleaseKernel` | 1 | shotcut |
| OpenCL.framework | `_clReleaseMemObject` | 1 | shotcut |
| OpenCL.framework | `_clReleaseProgram` | 1 | shotcut |
| OpenCL.framework | `_clReleaseSampler` | 1 | shotcut |
| OpenCL.framework | `_clRetainCommandQueue` | 1 | shotcut |
| OpenCL.framework | `_clRetainContext` | 1 | shotcut |
| OpenCL.framework | `_clRetainDevice` | 1 | shotcut |
| OpenCL.framework | `_clRetainEvent` | 1 | shotcut |
| OpenCL.framework | `_clRetainKernel` | 1 | shotcut |
| OpenCL.framework | `_clRetainMemObject` | 1 | shotcut |
| OpenCL.framework | `_clRetainProgram` | 1 | shotcut |
| OpenCL.framework | `_clRetainSampler` | 1 | shotcut |
| OpenCL.framework | `_clSetEventCallback` | 1 | shotcut |
| OpenCL.framework | `_clSetKernelArg` | 1 | shotcut |
| OpenCL.framework | `_clSetMemObjectDestructorCallback` | 1 | shotcut |
| OpenCL.framework | `_clSetUserEventStatus` | 1 | shotcut |
| OpenCL.framework | `_clUnloadPlatformCompiler` | 1 | shotcut |
| OpenCL.framework | `_clWaitForEvents` | 1 | shotcut |
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
| OpenGL.framework | `_gluCheckExtension` | 1 | lmms |
| OpenGL.framework | `_gluCylinder` | 1 | kicad |
| OpenGL.framework | `_gluDeleteQuadric` | 1 | kicad |
| OpenGL.framework | `_gluDisk` | 1 | kicad |
| OpenGL.framework | `_gluLookAt` | 1 | openscad |
| OpenGL.framework | `_gluNewQuadric` | 1 | kicad |
| OpenGL.framework | `_gluProject` | 1 | openscad |
| OpenGL.framework | `_gluQuadricDrawStyle` | 1 | kicad |
| OpenGL.framework | `_gluQuadricNormals` | 1 | kicad |
| OpenGL.framework | `_gluSphere` | 1 | kicad |
| QuartzCore.framework | `_OBJC_CLASS_$_CABackdropLayer` | 1 | zoom |
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
