/* mOS SDK (variante com interop ObjC) — tipos do CoreGraphics/IOSurface que o SwiftUI.framework usa e que o
 * CoreGraphics.h do mOS (userland/sdk/include/CoreGraphics) ainda não declara, com as definições da Apple (os nomes
 * mangled saem iguais: So11CGBlendModeV,...). Módulo clang _MOSCoreGraphicsSwiftUI, reexportado pelo
 * MOSFoundationExtras (tools\build-swiftui-objc.ps1). */
#pragma once
#include <CoreGraphics/CoreGraphics.h>

CF_ASSUME_NONNULL_BEGIN

typedef CF_ENUM(int32_t, CGBlendMode) {
    kCGBlendModeNormal, kCGBlendModeMultiply, kCGBlendModeScreen, kCGBlendModeOverlay, kCGBlendModeDarken,
    kCGBlendModeLighten, kCGBlendModeColorDodge, kCGBlendModeColorBurn, kCGBlendModeSoftLight, kCGBlendModeHardLight,
    kCGBlendModeDifference, kCGBlendModeExclusion, kCGBlendModeHue, kCGBlendModeSaturation, kCGBlendModeColor,
    kCGBlendModeLuminosity, kCGBlendModeClear, kCGBlendModeCopy, kCGBlendModeSourceIn, kCGBlendModeSourceOut,
    kCGBlendModeSourceAtop, kCGBlendModeDestinationOver, kCGBlendModeDestinationIn, kCGBlendModeDestinationOut,
    kCGBlendModeDestinationAtop, kCGBlendModeXOR __attribute__((swift_name("xor"))), kCGBlendModePlusDarker, kCGBlendModePlusLighter
};

typedef CF_ENUM(uint32_t, CGImageAlphaInfo) {
    kCGImageAlphaNone, kCGImageAlphaPremultipliedLast, kCGImageAlphaPremultipliedFirst, kCGImageAlphaLast,
    kCGImageAlphaFirst, kCGImageAlphaNoneSkipLast, kCGImageAlphaNoneSkipFirst, kCGImageAlphaOnly
};

typedef CF_OPTIONS(uint32_t, CGBitmapInfo) {
    kCGBitmapAlphaInfoMask = 0x1F,
    kCGBitmapFloatInfoMask = 0xF00,
    kCGBitmapFloatComponents = (1 << 8),
    kCGBitmapByteOrderMask = 0x7000,
    kCGBitmapByteOrderDefault = (0 << 12),
    kCGBitmapByteOrder16Little = (1 << 12),
    kCGBitmapByteOrder32Little = (2 << 12),
    kCGBitmapByteOrder16Big = (3 << 12),
    kCGBitmapByteOrder32Big = (4 << 12)
};


typedef struct CF_BRIDGED_TYPE(id) __IOSurface *IOSurfaceRef;   /* IOSurface no Swift */

CF_ASSUME_NONNULL_END
