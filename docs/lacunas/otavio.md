# Lacunas de Otavio (OpenGL, Metal, Core Animation)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Otavio (OpenGL, Metal, Core Animation) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**136** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| CoreVideo.framework | `_kCVPixelBufferIOSurfaceOpenGLTextureCompatibilityKey` | 5 | iina, krita, obs, shotcut |
| OpenGL.framework | `_glGetIntegerv` | 5 | iina, krita, musescore, shotcut |
| OpenGL.framework | `_glOrtho` | 5 | firefox, krita, musescore, shotcut |
| CoreVideo.framework | `_CVBufferGetAttachments` | 4 | iina, krita, obs, vlc |
| OpenGL.framework | `_glGetString` | 4 | krita, musescore, shotcut, vlc |
| CoreVideo.framework | `_CVImageBufferGetEncodedSize` | 3 | iina, obs, shotcut |
| OpenGL.framework | `_glLoadIdentity` | 3 | krita, musescore, shotcut |
| OpenGL.framework | `_glLoadMatrixf` | 3 | krita, musescore, shotcut |
| OpenGL.framework | `_glMatrixMode` | 3 | krita, musescore, shotcut |
| CoreVideo.framework | `_CVDisplayLinkSetCurrentCGDisplayFromOpenGLContext` | 2 | krita, shotcut |
| CoreVideo.framework | `_kCVImageBufferColorPrimaries_P22` | 2 | firefox, thunderbird |
| OpenGL.framework | `_glClear` | 2 | iina, vlc |
| OpenGL.framework | `_glClearColor` | 2 | iina, vlc |
| OpenGL.framework | `_glDisable` | 2 | krita, vlc |
| OpenGL.framework | `_glEnable` | 2 | krita, vlc |
| OpenGL.framework | `_glFlush` | 2 | iina, vlc |
| OpenGL.framework | `_glGetError` | 2 | iina, vlc |
| OpenGL.framework | `_glPixelStorei` | 2 | iina, vlc |
| CoreVideo.framework | `_CVOpenGLTextureCacheCreate` | 1 | shotcut |
| CoreVideo.framework | `_CVOpenGLTextureCacheCreateTextureFromImage` | 1 | shotcut |
| CoreVideo.framework | `_CVOpenGLTextureCacheFlush` | 1 | shotcut |
| CoreVideo.framework | `_CVOpenGLTextureGetName` | 1 | shotcut |
| CoreVideo.framework | `_CVOpenGLTextureRelease` | 1 | shotcut |
| CoreVideo.framework | `_kCVImageBufferChromaLocation_DV420` | 1 | vlc |
| Metal.framework | `_OBJC_CLASS_$_MTLAccelerationStructureBoundingBoxGeometryDescriptor` | 1 | blender |
| Metal.framework | `_OBJC_CLASS_$_MTLAccelerationStructureTriangleGeometryDescriptor` | 1 | blender |
| Metal.framework | `_OBJC_CLASS_$_MTLInstanceAccelerationStructureDescriptor` | 1 | blender |
| Metal.framework | `_OBJC_CLASS_$_MTLIntersectionFunctionDescriptor` | 1 | blender |
| Metal.framework | `_OBJC_CLASS_$_MTLIntersectionFunctionTableDescriptor` | 1 | blender |
| Metal.framework | `_OBJC_CLASS_$_MTLPrimitiveAccelerationStructureDescriptor` | 1 | blender |
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
| OpenGL.framework | `_glBindFramebuffer` | 1 | iina |
| OpenGL.framework | `_glBindTexture` | 1 | vlc |
| OpenGL.framework | `_glBlendFunc` | 1 | vlc |
| OpenGL.framework | `_glDeleteTextures` | 1 | vlc |
| OpenGL.framework | `_glDepthMask` | 1 | vlc |
| OpenGL.framework | `_glDrawArrays` | 1 | vlc |
| OpenGL.framework | `_glDrawElements` | 1 | vlc |
| OpenGL.framework | `_glFinish` | 1 | vlc |
| OpenGL.framework | `_glFlushRenderAPPLE` | 1 | obs |
| OpenGL.framework | `_glGenTextures` | 1 | vlc |
| OpenGL.framework | `_glGetTexLevelParameteriv` | 1 | vlc |
| OpenGL.framework | `_glReadBuffer` | 1 | iina |
| OpenGL.framework | `_glReadPixels` | 1 | iina |
| OpenGL.framework | `_glTexEnvf` | 1 | vlc |
| OpenGL.framework | `_glTexImage2D` | 1 | vlc |
| OpenGL.framework | `_glTexParameterf` | 1 | vlc |
| OpenGL.framework | `_glTexParameteri` | 1 | vlc |
| OpenGL.framework | `_glTexSubImage2D` | 1 | vlc |
| OpenGL.framework | `_glViewport` | 1 | vlc |
| QuartzCore.framework | `_OBJC_CLASS_$_CIContext` | 1 | vlc |
| QuartzCore.framework | `_kCIContextOutputColorSpace` | 1 | vlc |
| QuartzCore.framework | `_kCIContextWorkingColorSpace` | 1 | vlc |
| QuartzCore.framework | `_kCIInputImageKey` | 1 | vlc |
| QuartzCore.framework | `_kCIOutputImageKey` | 1 | vlc |
| libswiftMetal.dylib | `_$sSo23MTLRenderCommandEncoderP5MetalE16setVertexBuffers_7offsets5rangeySaySo9MTLBuffer_pSgG_SaySiGSnySiGtF` | 1 | obs |
