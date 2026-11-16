/*
 * mOS SDK — incluído (-include, depois do mos_openswiftui_prelude.h) em cada fonte C/C++ do OpenRenderBox.
 * Funções de CGPath do macOS 13 que o PathMapper do OpenRenderBox chama e o CoreGraphics.h do mOS ainda não declara:
 * stroke e dash já existem no libSystem (coregraphics_bm.inc); normalização e operações booleanas são resolvidas
 * no libSystem quando o SwiftUI.framework carrega.
 */
#pragma once
#include <OpenRenderBox/ORBBase.h>
#if OPENRENDERBOX_CF_CGTYPES
#include <CoreGraphics/CoreGraphics.h>
#ifdef __cplusplus
extern "C" {
#endif
CGPathRef CGPathCreateCopyByStrokingPath(CGPathRef path, const CGAffineTransform *transform, CGFloat lineWidth, CGLineCap lineCap,
                                         CGLineJoin lineJoin, CGFloat miterLimit);
CGPathRef CGPathCreateCopyByDashingPath(CGPathRef path, const CGAffineTransform *transform, CGFloat phase, const CGFloat *lengths, size_t count);
CGPathRef CGPathCreateCopyByNormalizing(CGPathRef path, bool evenOddFillRule);
CGPathRef CGPathCreateCopyByIntersectingPath(CGPathRef path, CGPathRef maskPath, bool evenOddFillRule);
CGPathRef CGPathCreateCopyByUnioningPath(CGPathRef path, CGPathRef maskPath, bool evenOddFillRule);
CGPathRef CGPathCreateCopyBySubtractingPath(CGPathRef path, CGPathRef maskPath, bool evenOddFillRule);
CGPathRef CGPathCreateCopyBySymmetricDifferenceOfPath(CGPathRef path, CGPathRef maskPath, bool evenOddFillRule);
CGPathRef CGPathCreateCopyOfLineByIntersectingPath(CGPathRef path, CGPathRef maskPath, bool evenOddFillRule);
CGPathRef CGPathCreateCopyOfLineBySubtractingPath(CGPathRef path, CGPathRef maskPath, bool evenOddFillRule);
#ifdef __cplusplus
}
#endif
#endif
