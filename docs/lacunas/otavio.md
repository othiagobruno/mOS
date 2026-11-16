# Lacunas de Otavio (OpenGL, Metal, Core Animation)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Otavio (OpenGL, Metal, Core Animation) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**7** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| AppKit.framework | `_NSOpenGLGetOption` | 1 | mos |
| AppKit.framework | `_NSOpenGLGetVersion` | 1 | mos |
| AppKit.framework | `_NSOpenGLSetOption` | 1 | mos |
| CoreVideo.framework | `_CVBufferGetAttachments` | 1 | vlc |
| CoreVideo.framework | `_kCVImageBufferChromaLocation_DV420` | 1 | vlc |
| CoreVideo.framework | `_kCVImageBufferColorPrimaries_P22` | 1 | firefox |
| CoreVideo.framework | `_kCVPixelBufferIOSurfaceOpenGLTextureCompatibilityKey` | 1 | vlc |
