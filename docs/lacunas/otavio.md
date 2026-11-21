# Lacunas de Otavio (OpenGL, Metal, Core Animation)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Otavio (OpenGL, Metal, Core Animation) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**280** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| CoreML.framework | `_OBJC_CLASS_$_MLFeatureDescription` | 6 | fluidvoice, macwhisper, openclaw, opensuperwhisper |
| CoreML.framework | `_OBJC_CLASS_$_MLArrayBatchProvider` | 5 | feishu, fluidvoice, macwhisper, opensuperwhisper |
| OpenCL.framework | `_clBuildProgram` | 5 | affinity, crossover, jellyfin, shotcut |
| OpenCL.framework | `_clCreateBuffer` | 5 | affinity, crossover, jellyfin, shotcut |
| OpenCL.framework | `_clCreateCommandQueue` | 5 | affinity, crossover, jellyfin, shotcut |
| OpenCL.framework | `_clCreateContext` | 5 | affinity, crossover, jellyfin, shotcut |
| OpenCL.framework | `_clCreateKernel` | 5 | affinity, crossover, jellyfin, shotcut |
| OpenCL.framework | `_clCreateProgramWithSource` | 5 | affinity, crossover, jellyfin, shotcut |
| OpenCL.framework | `_clEnqueueFillBuffer` | 5 | affinity, crossover, jellyfin, shotcut |
| OpenCL.framework | `_clEnqueueMapBuffer` | 5 | affinity, crossover, jellyfin, shotcut |
| OpenCL.framework | `_clEnqueueNDRangeKernel` | 5 | affinity, crossover, jellyfin, shotcut |
| OpenCL.framework | `_clEnqueueUnmapMemObject` | 5 | affinity, crossover, jellyfin, shotcut |
| OpenCL.framework | `_clFlush` | 5 | affinity, crossover, jellyfin, shotcut |
| OpenCL.framework | `_clGetDeviceIDs` | 5 | affinity, crossover, jellyfin, shotcut |
| OpenCL.framework | `_clGetDeviceInfo` | 5 | affinity, crossover, jellyfin, shotcut |
| OpenCL.framework | `_clGetMemObjectInfo` | 5 | affinity, crossover, jellyfin, shotcut |
| OpenCL.framework | `_clGetPlatformIDs` | 5 | affinity, crossover, jellyfin, shotcut |
| OpenCL.framework | `_clGetPlatformInfo` | 5 | affinity, crossover, jellyfin, shotcut |
| OpenCL.framework | `_clGetProgramBuildInfo` | 5 | affinity, crossover, jellyfin, shotcut |
| OpenCL.framework | `_clReleaseCommandQueue` | 5 | affinity, crossover, jellyfin, shotcut |
| OpenCL.framework | `_clReleaseContext` | 5 | affinity, crossover, jellyfin, shotcut |
| OpenCL.framework | `_clReleaseEvent` | 5 | affinity, crossover, jellyfin, shotcut |
| OpenCL.framework | `_clReleaseKernel` | 5 | affinity, crossover, jellyfin, shotcut |
| OpenCL.framework | `_clReleaseMemObject` | 5 | affinity, crossover, jellyfin, shotcut |
| OpenCL.framework | `_clReleaseProgram` | 5 | affinity, crossover, jellyfin, shotcut |
| OpenCL.framework | `_clSetKernelArg` | 5 | affinity, crossover, jellyfin, shotcut |
| OpenCL.framework | `_clWaitForEvents` | 5 | affinity, crossover, jellyfin, shotcut |
| MetalPerformanceShaders.framework | `_OBJC_CLASS_$_MPSImageAreaMax` | 4 | microsoft-excel, microsoft-outlook, microsoft-powerpoint, microsoft-word |
| MetalPerformanceShaders.framework | `_OBJC_CLASS_$_MPSImageAreaMin` | 4 | microsoft-excel, microsoft-outlook, microsoft-powerpoint, microsoft-word |
| MetalPerformanceShaders.framework | `_OBJC_CLASS_$_MPSImageBox` | 4 | microsoft-excel, microsoft-outlook, microsoft-powerpoint, microsoft-word |
| MetalPerformanceShaders.framework | `_OBJC_CLASS_$_MPSImageConvolution` | 4 | microsoft-excel, microsoft-outlook, microsoft-powerpoint, microsoft-word |
| OpenCL.framework | `_clCreateImage` | 4 | crossover, jellyfin, shotcut, stremio |
| OpenCL.framework | `_clEnqueueCopyImage` | 4 | crossover, jellyfin, shotcut, stremio |
| OpenCL.framework | `_clEnqueueMapImage` | 4 | crossover, jellyfin, shotcut, stremio |
| OpenCL.framework | `_clEnqueueReadBuffer` | 4 | crossover, jellyfin, shotcut, stremio |
| OpenCL.framework | `_clEnqueueReadImage` | 4 | crossover, jellyfin, shotcut, stremio |
| OpenCL.framework | `_clEnqueueWriteBuffer` | 4 | crossover, jellyfin, shotcut, stremio |
| OpenCL.framework | `_clEnqueueWriteImage` | 4 | crossover, jellyfin, shotcut, stremio |
| OpenCL.framework | `_clFinish` | 4 | crossover, jellyfin, shotcut, stremio |
| OpenCL.framework | `_clGetEventProfilingInfo` | 4 | crossover, jellyfin, shotcut, stremio |
| OpenCL.framework | `_clGetImageInfo` | 4 | crossover, jellyfin, shotcut, stremio |
| OpenCL.framework | `_clGetSupportedImageFormats` | 4 | crossover, jellyfin, shotcut, stremio |
| OpenCL.framework | `_clRetainCommandQueue` | 4 | crossover, jellyfin, shotcut, stremio |
| OpenGL.framework | `_gluCheckExtension` | 4 | crossover, cryptomator, freecad, lmms |
| CoreML.framework | `_$sSf6CoreML19MLShapedArrayScalarAAWP` | 3 | fluidvoice, openclaw, voiceink |
| CoreML.framework | `_OBJC_CLASS_$_MLModelAsset` | 3 | macwhisper, microsoft-teams, voiceink |
| OpenCL.framework | `_clEnqueueCopyBufferRect` | 3 | affinity, crossover, shotcut |
| OpenCL.framework | `_clEnqueueCopyBufferToImage` | 3 | crossover, jellyfin, shotcut |
| OpenCL.framework | `_clEnqueueReadBufferRect` | 3 | affinity, crossover, shotcut |
| OpenCL.framework | `_clEnqueueWriteBufferRect` | 3 | affinity, crossover, shotcut |
| OpenGL.framework | `_gluUnProject` | 3 | freecad, lmms, openscad |
| QuartzCore.framework | `_OBJC_CLASS_$_CABackdropLayer` | 3 | bettertouchtool, orion, zoom |
| libswiftCoreML.dylib | `_$s6CoreML13MLShapedArrayVMn` | 3 | macwhisper, opensuperwhisper, superwhisper |
| libswiftCoreML.dylib | `_$s6CoreML13MLShapedArrayVyxGAA0cD8ProtocolAAMc` | 3 | macwhisper, opensuperwhisper, superwhisper |
| libswiftCoreML.dylib | `_$s6CoreML21MLShapedArrayProtocolPAAE7scalarsSay6ScalarQzGvg` | 3 | macwhisper, opensuperwhisper, superwhisper |
| libswiftCoreML.dylib | `_$sSf6CoreML19MLShapedArrayScalarAAWP` | 3 | macwhisper, opensuperwhisper, superwhisper |
| libswiftCoreML.dylib | `_$sSo12MLMultiArrayC6CoreMLE30withUnsafeMutableBufferPointer6ofType_q_xm_q_SryxG_SaySiGtKXEtKAC08MLShapedB6ScalarRzr0_lF` | 3 | macwhisper, opensuperwhisper, superwhisper |
| libswiftCoreML.dylib | `_$sSo7MLModelC6CoreMLE4load10contentsOf13configurationAB10Foundation3URLV_So0A13ConfigurationCtYaKFZ` | 3 | macwhisper, opensuperwhisper, superwhisper |
| libswiftCoreML.dylib | `_$sSo7MLModelC6CoreMLE4load10contentsOf13configurationAB10Foundation3URLV_So0A13ConfigurationCtYaKFZTu` | 3 | macwhisper, opensuperwhisper, superwhisper |
| libswiftCoreML.dylib | `_$ss5Int32V6CoreML19MLShapedArrayScalarACWP` | 3 | macwhisper, opensuperwhisper, superwhisper |
| AppKit.framework | `_OBJC_CLASS_$_NSOpenGLLayer` | 2 | epic-games, zoom |
| AppKit.framework | `_OBJC_METACLASS_$_NSOpenGLLayer` | 2 | epic-games, zoom |
| CoreML.framework | `_$s6CoreML13MLShapedArrayVMn` | 2 | fluidvoice, openclaw |
| CoreML.framework | `_$s6CoreML13MLShapedArrayVyxGAA0cD8ProtocolAAMc` | 2 | fluidvoice, openclaw |
| CoreML.framework | `_$s6CoreML21MLShapedArrayProtocolPAAE7scalarsSay6ScalarQzGvg` | 2 | fluidvoice, openclaw |
| CoreML.framework | `_$sSo12MLMultiArrayC6CoreMLE30withUnsafeMutableBufferPointer6ofType_q_xm_q_SryxG_SaySiGtKXEtKAC08MLShapedB6ScalarRzr0_lF` | 2 | fluidvoice, voiceink |
| CoreML.framework | `_$sSo7MLModelC6CoreMLE10prediction4from7optionsSo17MLFeatureProvider_pSoAG_p_So19MLPredictionOptionsCtYaKF` | 2 | fluidvoice, voiceink |
| CoreML.framework | `_$sSo7MLModelC6CoreMLE10prediction4from7optionsSo17MLFeatureProvider_pSoAG_p_So19MLPredictionOptionsCtYaKFTu` | 2 | fluidvoice, voiceink |
| CoreML.framework | `_$sSo7MLModelC6CoreMLE12compileModel2at10Foundation3URLVAH_tYaKFZ` | 2 | fluidvoice, voiceink |
| CoreML.framework | `_$sSo7MLModelC6CoreMLE12compileModel2at10Foundation3URLVAH_tYaKFZTu` | 2 | fluidvoice, voiceink |
| CoreML.framework | `_$sSo7MLModelC6CoreMLE4load10contentsOf13configurationAB10Foundation3URLV_So0A13ConfigurationCtYaKFZ` | 2 | fluidvoice, voiceink |
| CoreML.framework | `_$sSo7MLModelC6CoreMLE4load10contentsOf13configurationAB10Foundation3URLV_So0A13ConfigurationCtYaKFZTu` | 2 | fluidvoice, voiceink |
| CoreML.framework | `_$ss5Int32V6CoreML19MLShapedArrayScalarACWP` | 2 | fluidvoice, openclaw |
| CoreML.framework | `_MLModelCreatorDefinedKey` | 2 | microsoft-teams, openclaw |
| CoreVideo.framework | `_CVImageBufferGetColorSpace` | 2 | cleanshot, microsoft-teams |
| CoreVideo.framework | `_kCVImageBufferFieldDetailTemporalBottomFirst` | 2 | crossover, handbrake-app |
| CoreVideo.framework | `_kCVImageBufferFieldDetailTemporalTopFirst` | 2 | crossover, handbrake-app |
| CoreVideo.framework | `_kCVPixelBufferExtendedPixelsBottomKey` | 2 | handbrake-app, orion |
| CoreVideo.framework | `_kCVPixelBufferExtendedPixelsRightKey` | 2 | handbrake-app, orion |
| CoreVideo.framework | `_kCVPixelFormatOpenGLCompatibility` | 2 | feishu, telegram |
| Metal.framework | `_MTLMakeShaderCacheWritableByAllUsers` | 2 | crossover, utm |
| Metal.framework | `_MTLSetShaderCachePath` | 2 | crossover, utm |
| Metal.framework | `_MTLTextureSwizzleChannelsToKey` | 2 | crossover, utm |
| Metal.framework | `_MTLTextureSwizzleKeyToChannels` | 2 | crossover, utm |
| Metal.framework | `_OBJC_CLASS_$_MTLAccelerationStructureAllocationDescriptor` | 2 | crossover, utm |
| Metal.framework | `_OBJC_CLASS_$_MTLMeshRenderPipelineDescriptor` | 2 | crossover, utm |
| MetalFX.framework | `_OBJC_CLASS_$_MTLFXSpatialScalerDescriptor` | 2 | crossover, utm |
| MetalFX.framework | `_OBJC_CLASS_$_MTLFXTemporalScalerDescriptor` | 2 | crossover, utm |
| MetalPerformanceShaders.framework | `_OBJC_CLASS_$_MPSImageGaussianBlur` | 2 | capcut, feishu |
| OpenCL.framework | `_clCreateContextFromType` | 2 | crossover, shotcut |
| OpenCL.framework | `_clCreateKernelsInProgram` | 2 | crossover, shotcut |
| OpenCL.framework | `_clCreateProgramWithBinary` | 2 | crossover, shotcut |
| OpenCL.framework | `_clCreateProgramWithBuiltInKernels` | 2 | crossover, shotcut |
| OpenCL.framework | `_clCreateSampler` | 2 | crossover, shotcut |
| OpenCL.framework | `_clCreateSubBuffer` | 2 | crossover, shotcut |
| OpenCL.framework | `_clCreateSubDevices` | 2 | crossover, shotcut |
| OpenCL.framework | `_clCreateUserEvent` | 2 | crossover, shotcut |
| OpenCL.framework | `_clEnqueueBarrierWithWaitList` | 2 | crossover, shotcut |
| OpenCL.framework | `_clEnqueueCopyBuffer` | 2 | crossover, shotcut |
| OpenCL.framework | `_clEnqueueCopyImageToBuffer` | 2 | crossover, shotcut |
| OpenCL.framework | `_clEnqueueFillImage` | 2 | crossover, shotcut |
| OpenCL.framework | `_clEnqueueMarkerWithWaitList` | 2 | crossover, shotcut |
| OpenCL.framework | `_clEnqueueMigrateMemObjects` | 2 | crossover, shotcut |
| OpenCL.framework | `_clEnqueueTask` | 2 | crossover, shotcut |
| OpenCL.framework | `_clGetCommandQueueInfo` | 2 | crossover, shotcut |
| OpenCL.framework | `_clGetContextInfo` | 2 | crossover, shotcut |
| OpenCL.framework | `_clGetEventInfo` | 2 | crossover, shotcut |
| OpenCL.framework | `_clGetKernelArgInfo` | 2 | crossover, shotcut |
| OpenCL.framework | `_clGetKernelInfo` | 2 | crossover, shotcut |
| OpenCL.framework | `_clGetKernelWorkGroupInfo` | 2 | crossover, shotcut |
| OpenCL.framework | `_clGetProgramInfo` | 2 | crossover, shotcut |
| OpenCL.framework | `_clGetSamplerInfo` | 2 | crossover, shotcut |
| OpenCL.framework | `_clReleaseDevice` | 2 | crossover, shotcut |
| OpenCL.framework | `_clReleaseSampler` | 2 | crossover, shotcut |
| OpenCL.framework | `_clRetainContext` | 2 | crossover, shotcut |
| OpenCL.framework | `_clRetainDevice` | 2 | crossover, shotcut |
| OpenCL.framework | `_clRetainEvent` | 2 | crossover, shotcut |
| OpenCL.framework | `_clRetainKernel` | 2 | crossover, shotcut |
| OpenCL.framework | `_clRetainMemObject` | 2 | crossover, shotcut |
| OpenCL.framework | `_clRetainProgram` | 2 | crossover, shotcut |
| OpenCL.framework | `_clRetainSampler` | 2 | crossover, shotcut |
| OpenCL.framework | `_clSetEventCallback` | 2 | affinity, shotcut |
| OpenCL.framework | `_clSetUserEventStatus` | 2 | crossover, shotcut |
| OpenCL.framework | `_clUnloadPlatformCompiler` | 2 | crossover, shotcut |
| OpenGL.framework | `_gluCylinder` | 2 | freecad, kicad |
| OpenGL.framework | `_gluDeleteQuadric` | 2 | freecad, kicad |
| OpenGL.framework | `_gluDeleteTess` | 2 | kicad, openscad |
| OpenGL.framework | `_gluDisk` | 2 | freecad, kicad |
| OpenGL.framework | `_gluErrorString` | 2 | kicad, openscad |
| OpenGL.framework | `_gluLookAt` | 2 | freecad, openscad |
| OpenGL.framework | `_gluNewQuadric` | 2 | freecad, kicad |
| OpenGL.framework | `_gluNewTess` | 2 | kicad, openscad |
| OpenGL.framework | `_gluPerspective` | 2 | kicad, openscad |
| OpenGL.framework | `_gluProject` | 2 | freecad, openscad |
| OpenGL.framework | `_gluSphere` | 2 | freecad, kicad |
| OpenGL.framework | `_gluTessBeginContour` | 2 | kicad, openscad |
| OpenGL.framework | `_gluTessBeginPolygon` | 2 | kicad, openscad |
| OpenGL.framework | `_gluTessCallback` | 2 | kicad, openscad |
| OpenGL.framework | `_gluTessEndContour` | 2 | kicad, openscad |
| OpenGL.framework | `_gluTessEndPolygon` | 2 | kicad, openscad |
| OpenGL.framework | `_gluTessNormal` | 2 | kicad, openscad |
| OpenGL.framework | `_gluTessProperty` | 2 | kicad, openscad |
| OpenGL.framework | `_gluTessVertex` | 2 | kicad, openscad |
| QuartzCore.framework | `_OBJC_CLASS_$_CAFilter` | 2 | orion, zoom |
| QuartzCore.framework | `_kCAFilterColorBurnBlendMode` | 2 | orion, zoom |
| QuartzCore.framework | `_kCAFilterColorDodgeBlendMode` | 2 | orion, zoom |
| QuartzCore.framework | `_kCAFilterColorSaturate` | 2 | orion, zoom |
| QuartzCore.framework | `_kCAFilterDarkenBlendMode` | 2 | orion, zoom |
| QuartzCore.framework | `_kCAFilterDifferenceBlendMode` | 2 | orion, zoom |
| QuartzCore.framework | `_kCAFilterExclusionBlendMode` | 2 | orion, zoom |
| QuartzCore.framework | `_kCAFilterGaussianBlur` | 2 | orion, zoom |
| QuartzCore.framework | `_kCAFilterHardLightBlendMode` | 2 | orion, zoom |
| QuartzCore.framework | `_kCAFilterLightenBlendMode` | 2 | orion, zoom |
| QuartzCore.framework | `_kCAFilterMultiplyBlendMode` | 2 | orion, zoom |
| QuartzCore.framework | `_kCAFilterOverlayBlendMode` | 2 | orion, zoom |
| QuartzCore.framework | `_kCAFilterScreenBlendMode` | 2 | orion, zoom |
| QuartzCore.framework | `_kCAFilterSoftLightBlendMode` | 2 | orion, zoom |
| SceneKit.framework | `_OBJC_CLASS_$_SCNNode` | 2 | istat-menus, telegram |
| SceneKit.framework | `_OBJC_CLASS_$_SCNScene` | 2 | istat-menus, telegram |
| SceneKit.framework | `_OBJC_CLASS_$_SCNView` | 2 | istat-menus, telegram |
| libswiftCoreML.dylib | `_$sSo12MLMultiArrayC6CoreMLE15withUnsafeBytesyxxSWKXEKlF` | 2 | macwhisper, superwhisper |
| libswiftCoreML.dylib | `_$sSo12MLMultiArrayC6CoreMLE22withUnsafeMutableBytesyxxSw_SaySiGtKXEKlF` | 2 | macwhisper, superwhisper |
| libswiftCoreML.dylib | `_$sSo12MLMultiArrayC6CoreMLEyABxcAC08MLShapedB8ProtocolRzlufC` | 2 | macwhisper, superwhisper |
| CoreML.framework | `_$s6CoreML13MLShapedArrayV5shapeSaySiGvg` | 1 | openclaw |
| CoreML.framework | `_$s6CoreML21MLShapedArrayProtocolPAAE7scalarsSay6ScalarQzGvM` | 1 | openclaw |
| CoreML.framework | `_$sSo12MLMultiArrayC6CoreMLE15withUnsafeBytesyxxSWKXEKlF` | 1 | voiceink |
| CoreML.framework | `_$sSo12MLMultiArrayC6CoreMLE22withUnsafeMutableBytesyxxSw_SaySiGtKXEKlF` | 1 | voiceink |
| CoreML.framework | `_$sSo12MLMultiArrayC6CoreMLE23withUnsafeBufferPointer6ofType_q_xm_q_SRyxGKXEtKAC08MLShapedB6ScalarRzr0_lF` | 1 | voiceink |
| CoreML.framework | `_$sSo14MLFeatureValueC6CoreMLE011shapedArrayB02ofAC08MLShapedF0VyxGSgxm_tAC0hF6ScalarRzlF` | 1 | fluidvoice |
| CoreML.framework | `_MLModelDescriptionKey` | 1 | openclaw |
| CoreVideo.framework | `_CVPixelFormatDescriptionArrayCreateWithAllPixelFormatTypes` | 1 | crossover |
| CoreVideo.framework | `_kCVImageBufferDisplayDimensionsKey` | 1 | handbrake-app |
| CoreVideo.framework | `_kCVImageBufferDisplayHeightKey` | 1 | handbrake-app |
| CoreVideo.framework | `_kCVImageBufferDisplayWidthKey` | 1 | handbrake-app |
| CoreVideo.framework | `_kCVPixelBufferExtendedPixelsLeftKey` | 1 | orion |
| CoreVideo.framework | `_kCVPixelBufferExtendedPixelsTopKey` | 1 | orion |
| CoreVideo.framework | `_kCVPixelFormatComponentRange_VideoRange` | 1 | handbrake-app |
| CoreVideo.framework | `_kCVPixelFormatConstant` | 1 | crossover |
| GameplayKit.framework | `_OBJC_CLASS_$_GKNoise` | 1 | shottr |
| GameplayKit.framework | `_OBJC_CLASS_$_GKNoiseMap` | 1 | shottr |
| GameplayKit.framework | `_OBJC_CLASS_$_GKPerlinNoiseSource` | 1 | shottr |
| IOSurface.framework | `_IOSurfaceSetOwnershipIdentity` | 1 | orion |
| IOSurface.framework | `_kIOSurfaceContentHeadroom` | 1 | orion |
| MetalPerformanceShaders.framework | `_OBJC_CLASS_$_MPSImage` | 1 | wechat |
| MetalPerformanceShaders.framework | `_OBJC_CLASS_$_MPSImageDescriptor` | 1 | wechat |
| MetalPerformanceShaders.framework | `_OBJC_CLASS_$_MPSTemporaryImage` | 1 | wechat |
| OpenCL.framework | `_clCompileProgram` | 1 | shotcut |
| OpenCL.framework | `_clEnqueueNativeKernel` | 1 | shotcut |
| OpenCL.framework | `_clGetExtensionFunctionAddressForPlatform` | 1 | shotcut |
| OpenCL.framework | `_clLinkProgram` | 1 | shotcut |
| OpenCL.framework | `_clSetMemObjectDestructorCallback` | 1 | shotcut |
| OpenGL.framework | `_CGLCopyContext` | 1 | xquartz |
| OpenGL.framework | `_CGLCreatePBuffer` | 1 | crossover |
| OpenGL.framework | `_CGLReleasePBuffer` | 1 | crossover |
| OpenGL.framework | `_CGLSetPBuffer` | 1 | crossover |
| OpenGL.framework | `_CGLTexImagePBuffer` | 1 | crossover |
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
| OpenGL.framework | `_gluOrtho2D` | 1 | adobe-acrobat-reader |
| OpenGL.framework | `_gluPickMatrix` | 1 | freecad |
| OpenGL.framework | `_gluQuadricDrawStyle` | 1 | kicad |
| OpenGL.framework | `_gluQuadricNormals` | 1 | kicad |
| QuartzCore.framework | `_CABackingStoreCollectBlocking` | 1 | orion |
| QuartzCore.framework | `_CARenderServerGetServerPort` | 1 | orion |
| QuartzCore.framework | `_CAToneMapModeIfSupported` | 1 | orion |
| QuartzCore.framework | `_OBJC_CLASS_$_CAPresentationModifier` | 1 | orion |
| QuartzCore.framework | `_OBJC_CLASS_$_CAPresentationModifierGroup` | 1 | orion |
| QuartzCore.framework | `_kCAFilterColorBlendMode` | 1 | orion |
| QuartzCore.framework | `_kCAFilterColorBrightness` | 1 | zoom |
| QuartzCore.framework | `_kCAFilterColorHueRotate` | 1 | orion |
| QuartzCore.framework | `_kCAFilterColorInvert` | 1 | orion |
| QuartzCore.framework | `_kCAFilterColorMatrix` | 1 | orion |
| QuartzCore.framework | `_kCAFilterColorMonochrome` | 1 | orion |
| QuartzCore.framework | `_kCAFilterDivideBlendMode` | 1 | zoom |
| QuartzCore.framework | `_kCAFilterHueBlendMode` | 1 | orion |
| QuartzCore.framework | `_kCAFilterInputColorMatrix` | 1 | orion |
| QuartzCore.framework | `_kCAFilterLinearBurnBlendMode` | 1 | zoom |
| QuartzCore.framework | `_kCAFilterLinearDodgeBlendMode` | 1 | zoom |
| QuartzCore.framework | `_kCAFilterLinearLightBlendMode` | 1 | zoom |
| QuartzCore.framework | `_kCAFilterLuminosityBlendMode` | 1 | orion |
| QuartzCore.framework | `_kCAFilterPinLightBlendMode` | 1 | zoom |
| QuartzCore.framework | `_kCAFilterPlusD` | 1 | orion |
| QuartzCore.framework | `_kCAFilterPlusL` | 1 | orion |
| QuartzCore.framework | `_kCAFilterSaturationBlendMode` | 1 | orion |
| QuartzCore.framework | `_kCAFilterSubtractBlendMode` | 1 | zoom |
| QuartzCore.framework | `_kCAFilterVibrantColorMatrix` | 1 | orion |
| QuartzCore.framework | `_kCAScrollBoth` | 1 | microsoft-outlook |
| SceneKit.framework | `_OBJC_CLASS_$_SCNAction` | 1 | telegram |
| SceneKit.framework | `_OBJC_CLASS_$_SCNCamera` | 1 | istat-menus |
| SceneKit.framework | `_OBJC_CLASS_$_SCNFloor` | 1 | istat-menus |
| SceneKit.framework | `_OBJC_CLASS_$_SCNLight` | 1 | istat-menus |
| SceneKit.framework | `_OBJC_CLASS_$_SCNMaterial` | 1 | telegram |
| SceneKit.framework | `_OBJC_CLASS_$_SCNParticlePropertyController` | 1 | telegram |
| SceneKit.framework | `_OBJC_CLASS_$_SCNParticleSystem` | 1 | telegram |
| SceneKit.framework | `_OBJC_CLASS_$_SCNPlane` | 1 | istat-menus |
| SceneKit.framework | `_OBJC_CLASS_$_SCNTransaction` | 1 | telegram |
| SceneKit.framework | `_OBJC_METACLASS_$_SCNNode` | 1 | istat-menus |
| SceneKit.framework | `_SCNLightTypeArea` | 1 | istat-menus |
| SceneKit.framework | `_SCNParticlePropertyColor` | 1 | telegram |
| SceneKit.framework | `_SCNParticlePropertyOpacity` | 1 | telegram |
| SceneKit.framework | `_SCNParticlePropertySize` | 1 | telegram |
| libswiftCoreML.dylib | `_$s6CoreML21MLShapedArrayProtocolPAAE10convertingxSo07MLMultiD0C_tcfC` | 1 | superwhisper |
| libswiftCoreML.dylib | `_$sSo14MLFeatureValueC6CoreMLE011shapedArrayB02ofAC08MLShapedF0VyxGSgxm_tAC0hF6ScalarRzlF` | 1 | opensuperwhisper |
| libswiftCoreML.dylib | `_$sSo7MLModelC6CoreMLE10prediction4from7optionsSo17MLFeatureProvider_pSoAG_p_So19MLPredictionOptionsCtYaKF` | 1 | opensuperwhisper |
| libswiftCoreML.dylib | `_$sSo7MLModelC6CoreMLE10prediction4from7optionsSo17MLFeatureProvider_pSoAG_p_So19MLPredictionOptionsCtYaKFTu` | 1 | opensuperwhisper |
| libswiftCoreML.dylib | `_$sSo7MLModelC6CoreMLE12compileModel2at10Foundation3URLVAH_tYaKFZ` | 1 | opensuperwhisper |
| libswiftCoreML.dylib | `_$sSo7MLModelC6CoreMLE12compileModel2at10Foundation3URLVAH_tYaKFZTu` | 1 | opensuperwhisper |
| libswiftMetal.dylib | `_$sSo23MTLRenderCommandEncoderP5MetalE19setFragmentTextures_5rangeySaySo10MTLTexture_pSgG_SnySiGtF` | 1 | raycast |
| libswiftQuartzCore.dylib | `_$sSo13CATransform3DV10QuartzCoreE34_conditionallyBridgeFromObjectiveC_6resultSbSo7NSValueC_ABSgztFZ` | 1 | mac-mouse-fix |
