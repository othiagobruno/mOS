/*
 * mOS SDK — CoreGraphics (subconjunto implementado em userland/libsystem/coregraphics.c, appkit.c e
 * cgdisplay.c): tipos geométricos, CGAffineTransform, CGPath, CGColor e CGColorSpace.
 * Mesma ABI do macOS (x86_64, CGFloat = double); os objetos são objetos CF/ObjC do mOS (CFRetain funciona).
 */
#ifndef MOS_COREGRAPHICS_H
#define MOS_COREGRAPHICS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <float.h>
#include <CoreFoundation/CFBase.h>

/* Para o Swift (o importador clang define __swift__): os tipos CG em C ganham nomes MOS... para não colidir com as
 * classes/structs Swift do OpenCoreGraphics (CGPath, CGColor, CGContext, CGImage, CGPathElement...), e as referências
 * CG*Ref não são "tipos CF" (sem CF_BRIDGED_TYPE): os objetos do libSystem são ObjC e o ARC do Swift sem ObjC os
 * corromperia; chegam ao Swift como ponteiros opacos. Para C/C++ nada muda. */
#if defined(__swift__)
#define CGPath MOSCGPath
#define CGPathRef MOSCGPathRef
#define CGMutablePathRef MOSCGMutablePathRef
#define CGPathElement MOSCGPathElement
#define CGPathElementType MOSCGPathElementType
#define CGPathApplierFunction MOSCGPathApplierFunction
#define CGPathApplyBlock MOSCGPathApplyBlock
#define CGPathApplyBlock2 MOSCGPathApplyBlock2
#define CGPathFillRule MOSCGPathFillRule
#define CGLineJoin MOSCGLineJoin
#define CGLineCap MOSCGLineCap
#define CGColor MOSCGColor
#define CGColorRef MOSCGColorRef
#define CGColorSpace MOSCGColorSpace
#define CGColorSpaceRef MOSCGColorSpaceRef
#define CGColorSpaceModel MOSCGColorSpaceModel
#define CGColorRenderingIntent MOSCGColorRenderingIntent
#define CGContext MOSCGContext
#define CGContextRef MOSCGContextRef
#define CGImage MOSCGImage
#define CGImageRef MOSCGImageRef
#define CGFont MOSCGFont
#define CGFontRef MOSCGFontRef
#define CGBlendMode MOSCGBlendMode
#define MOS_CG_BRIDGED
#else
#define MOS_CG_BRIDGED CF_BRIDGED_TYPE(id)
#endif

#ifdef __cplusplus
#define CG_EXTERN_C_BEGIN extern "C" {
#define CG_EXTERN_C_END }
#else
#define CG_EXTERN_C_BEGIN
#define CG_EXTERN_C_END
#endif
#ifndef CG_EXTERN
#ifdef __cplusplus
#define CG_EXTERN extern "C" __attribute__((visibility("default")))
#else
#define CG_EXTERN extern __attribute__((visibility("default")))
#endif
#endif
#ifndef CG_INLINE
#define CG_INLINE static inline
#endif
#ifndef CG_LOCAL
#define CG_LOCAL __attribute__((visibility("hidden")))
#endif
#define CG_AVAILABLE_STARTING(...)
#define CG_AVAILABLE_BUT_DEPRECATED(...)
#define CG_ASSUME_NONNULL_BEGIN _Pragma("clang assume_nonnull begin")
#define CG_ASSUME_NONNULL_END _Pragma("clang assume_nonnull end")
#ifndef cg_nullable
#define cg_nullable _Nullable
#endif
#ifndef cg_nonnull
#define cg_nonnull _Nonnull
#endif
#ifndef CF_NOESCAPE
#define CF_NOESCAPE __attribute__((noescape))
#endif

/* ---- tipos base (os mesmos de CFCGTypes.h do swift-corelibs; quem incluir primeiro define) */
#if !defined(__COREFOUNDATION_CFCGTYPES__)
#define __COREFOUNDATION_CFCGTYPES__ 1
#define CF_DEFINES_CG_TYPES
#define CGFLOAT_TYPE double
#define CGFLOAT_IS_DOUBLE 1
#define CGFLOAT_MIN DBL_MIN
#define CGFLOAT_MAX DBL_MAX
#define CGFLOAT_EPSILON DBL_EPSILON
typedef CGFLOAT_TYPE CGFloat;
#define CGFLOAT_DEFINED 1
struct CGPoint { CGFloat x; CGFloat y; };
typedef struct CGPoint CGPoint;
struct CGSize { CGFloat width; CGFloat height; };
typedef struct CGSize CGSize;
#define CGVECTOR_DEFINED 1
struct CGVector { CGFloat dx; CGFloat dy; };
typedef struct CGVector CGVector;
struct CGRect { CGPoint origin; CGSize size; };
typedef struct CGRect CGRect;
typedef CF_CLOSED_ENUM(uint32_t, CGRectEdge) { CGRectMinXEdge, CGRectMinYEdge, CGRectMaxXEdge, CGRectMaxYEdge };
typedef struct CGAffineTransform CGAffineTransform;
struct CGAffineTransform { CGFloat a, b, c, d; CGFloat tx, ty; };
#define CF_DEFINES_CGAFFINETRANSFORMCOMPONENTS
typedef struct CGAffineTransformComponents CGAffineTransformComponents;
struct CGAffineTransformComponents { CGSize scale; CGFloat horizontalShear; CGFloat rotation; CGVector translation; };
#endif

CG_EXTERN_C_BEGIN
CF_ASSUME_NONNULL_BEGIN

/* ---- geometria */
CG_EXTERN const CGPoint CGPointZero;
CG_EXTERN const CGSize CGSizeZero;
CG_EXTERN const CGRect CGRectZero;
CG_EXTERN const CGRect CGRectNull;
CG_EXTERN const CGRect CGRectInfinite;

CG_INLINE CGPoint CGPointMake(CGFloat x, CGFloat y) { CGPoint p; p.x = x; p.y = y; return p; }
CG_INLINE CGSize CGSizeMake(CGFloat w, CGFloat h) { CGSize s; s.width = w; s.height = h; return s; }
CG_INLINE CGVector CGVectorMake(CGFloat dx, CGFloat dy) { CGVector v; v.dx = dx; v.dy = dy; return v; }
CG_INLINE CGRect CGRectMake(CGFloat x, CGFloat y, CGFloat w, CGFloat h) {
    CGRect r; r.origin.x = x; r.origin.y = y; r.size.width = w; r.size.height = h; return r;
}
CG_INLINE bool __CGPointEqualToPoint(CGPoint a, CGPoint b) { return a.x == b.x && a.y == b.y; }
#define CGPointEqualToPoint __CGPointEqualToPoint
CG_INLINE bool __CGSizeEqualToSize(CGSize a, CGSize b) { return a.width == b.width && a.height == b.height; }
#define CGSizeEqualToSize __CGSizeEqualToSize

CG_EXTERN CGFloat CGRectGetMinX(CGRect r);
CG_EXTERN CGFloat CGRectGetMidX(CGRect r);
CG_EXTERN CGFloat CGRectGetMaxX(CGRect r);
CG_EXTERN CGFloat CGRectGetMinY(CGRect r);
CG_EXTERN CGFloat CGRectGetMidY(CGRect r);
CG_EXTERN CGFloat CGRectGetMaxY(CGRect r);
CG_EXTERN CGFloat CGRectGetWidth(CGRect r);
CG_EXTERN CGFloat CGRectGetHeight(CGRect r);
CG_EXTERN bool CGRectEqualToRect(CGRect a, CGRect b);
CG_EXTERN CGRect CGRectStandardize(CGRect r);
CG_EXTERN bool CGRectIsEmpty(CGRect r);
CG_EXTERN bool CGRectIsNull(CGRect r);
CG_EXTERN bool CGRectIsInfinite(CGRect r);
CG_EXTERN CGRect CGRectInset(CGRect r, CGFloat dx, CGFloat dy);
CG_EXTERN CGRect CGRectIntegral(CGRect r);
CG_EXTERN CGRect CGRectUnion(CGRect a, CGRect b);
CG_EXTERN CGRect CGRectIntersection(CGRect a, CGRect b);
CG_EXTERN CGRect CGRectOffset(CGRect r, CGFloat dx, CGFloat dy);
CG_EXTERN void CGRectDivide(CGRect r, CGRect *slice, CGRect *remainder, CGFloat amount, CGRectEdge edge);
CG_EXTERN bool CGRectContainsPoint(CGRect r, CGPoint p);
CG_EXTERN bool CGRectContainsRect(CGRect a, CGRect b);
CG_EXTERN bool CGRectIntersectsRect(CGRect a, CGRect b);

/* ---- CGAffineTransform */
CG_EXTERN const CGAffineTransform CGAffineTransformIdentity;
CG_EXTERN CGAffineTransform CGAffineTransformMake(CGFloat a, CGFloat b, CGFloat c, CGFloat d, CGFloat tx, CGFloat ty);
CG_EXTERN CGAffineTransform CGAffineTransformMakeTranslation(CGFloat tx, CGFloat ty);
CG_EXTERN CGAffineTransform CGAffineTransformMakeScale(CGFloat sx, CGFloat sy);
CG_EXTERN CGAffineTransform CGAffineTransformMakeRotation(CGFloat angle);
CG_EXTERN bool CGAffineTransformIsIdentity(CGAffineTransform t);
CG_EXTERN CGAffineTransform CGAffineTransformTranslate(CGAffineTransform t, CGFloat tx, CGFloat ty);
CG_EXTERN CGAffineTransform CGAffineTransformScale(CGAffineTransform t, CGFloat sx, CGFloat sy);
CG_EXTERN CGAffineTransform CGAffineTransformRotate(CGAffineTransform t, CGFloat angle);
CG_EXTERN CGAffineTransform CGAffineTransformInvert(CGAffineTransform t);
CG_EXTERN CGAffineTransform CGAffineTransformConcat(CGAffineTransform t1, CGAffineTransform t2);
CG_EXTERN bool CGAffineTransformEqualToTransform(CGAffineTransform t1, CGAffineTransform t2);
CG_EXTERN CGPoint CGPointApplyAffineTransform(CGPoint p, CGAffineTransform t);
CG_EXTERN CGSize CGSizeApplyAffineTransform(CGSize s, CGAffineTransform t);
CG_EXTERN CGRect CGRectApplyAffineTransform(CGRect r, CGAffineTransform t);
CG_EXTERN CGAffineTransformComponents CGAffineTransformDecompose(CGAffineTransform t);
CG_EXTERN CGAffineTransform CGAffineTransformMakeWithComponents(CGAffineTransformComponents c);

/* ---- CGPath */
typedef struct MOS_CG_BRIDGED CGPath *CGMutablePathRef;
typedef const struct MOS_CG_BRIDGED CGPath *CGPathRef;

typedef CF_ENUM(int32_t, CGLineJoin) { kCGLineJoinMiter, kCGLineJoinRound, kCGLineJoinBevel };
typedef CF_ENUM(int32_t, CGLineCap) { kCGLineCapButt, kCGLineCapRound, kCGLineCapSquare };
typedef CF_ENUM(int32_t, CGPathFillRule) { kCGPathFillRuleWinding, kCGPathFillRuleEvenOdd };
typedef CF_ENUM(int32_t, CGPathElementType) {
    kCGPathElementMoveToPoint,
    kCGPathElementAddLineToPoint,
    kCGPathElementAddQuadCurveToPoint,
    kCGPathElementAddCurveToPoint,
    kCGPathElementCloseSubpath
};
struct CGPathElement { CGPathElementType type; CGPoint *points; };
typedef struct CGPathElement CGPathElement;
typedef void (*CGPathApplierFunction)(void *_Nullable info, const CGPathElement *element);
#if __BLOCKS__
typedef void (^CGPathApplyBlock)(const CGPathElement *element);
typedef void (^CGPathApplyBlock2)(const CGPathElement *element, bool *stop);
#endif

CG_EXTERN CFTypeID CGPathGetTypeID(void);
CG_EXTERN CGMutablePathRef CGPathCreateMutable(void);
CG_EXTERN CGPathRef _Nullable CGPathCreateCopy(CGPathRef _Nullable path);
CG_EXTERN CGPathRef _Nullable CGPathCreateCopyByTransformingPath(CGPathRef _Nullable path, const CGAffineTransform *_Nullable transform);
CG_EXTERN CGMutablePathRef _Nullable CGPathCreateMutableCopy(CGPathRef _Nullable path);
CG_EXTERN CGMutablePathRef _Nullable CGPathCreateMutableCopyByTransformingPath(CGPathRef _Nullable path, const CGAffineTransform *_Nullable transform);
CG_EXTERN CGPathRef CGPathCreateWithRect(CGRect rect, const CGAffineTransform *_Nullable transform);
CG_EXTERN CGPathRef CGPathCreateWithEllipseInRect(CGRect rect, const CGAffineTransform *_Nullable transform);
CG_EXTERN CGPathRef CGPathCreateWithRoundedRect(CGRect rect, CGFloat cornerWidth, CGFloat cornerHeight, const CGAffineTransform *_Nullable transform);
CG_EXTERN CGPathRef _Nullable CGPathRetain(CGPathRef _Nullable path);
CG_EXTERN void CGPathRelease(CGPathRef _Nullable path);
CG_EXTERN bool CGPathEqualToPath(CGPathRef _Nullable path1, CGPathRef _Nullable path2);
CG_EXTERN void CGPathMoveToPoint(CGMutablePathRef _Nullable path, const CGAffineTransform *_Nullable m, CGFloat x, CGFloat y);
CG_EXTERN void CGPathAddLineToPoint(CGMutablePathRef _Nullable path, const CGAffineTransform *_Nullable m, CGFloat x, CGFloat y);
CG_EXTERN void CGPathAddQuadCurveToPoint(CGMutablePathRef _Nullable path, const CGAffineTransform *_Nullable m, CGFloat cpx, CGFloat cpy, CGFloat x, CGFloat y);
CG_EXTERN void CGPathAddCurveToPoint(CGMutablePathRef _Nullable path, const CGAffineTransform *_Nullable m, CGFloat cp1x, CGFloat cp1y, CGFloat cp2x, CGFloat cp2y, CGFloat x, CGFloat y);
CG_EXTERN void CGPathCloseSubpath(CGMutablePathRef _Nullable path);
CG_EXTERN void CGPathAddRect(CGMutablePathRef _Nullable path, const CGAffineTransform *_Nullable m, CGRect rect);
CG_EXTERN void CGPathAddRects(CGMutablePathRef _Nullable path, const CGAffineTransform *_Nullable m, const CGRect *_Nullable rects, size_t count);
CG_EXTERN void CGPathAddLines(CGMutablePathRef _Nullable path, const CGAffineTransform *_Nullable m, const CGPoint *_Nullable points, size_t count);
CG_EXTERN void CGPathAddEllipseInRect(CGMutablePathRef _Nullable path, const CGAffineTransform *_Nullable m, CGRect rect);
CG_EXTERN void CGPathAddRoundedRect(CGMutablePathRef _Nullable path, const CGAffineTransform *_Nullable transform, CGRect rect, CGFloat cornerWidth, CGFloat cornerHeight);
CG_EXTERN void CGPathAddArc(CGMutablePathRef _Nullable path, const CGAffineTransform *_Nullable m, CGFloat x, CGFloat y, CGFloat radius, CGFloat startAngle, CGFloat endAngle, bool clockwise);
CG_EXTERN void CGPathAddRelativeArc(CGMutablePathRef _Nullable path, const CGAffineTransform *_Nullable matrix, CGFloat x, CGFloat y, CGFloat radius, CGFloat startAngle, CGFloat delta);
CG_EXTERN void CGPathAddArcToPoint(CGMutablePathRef _Nullable path, const CGAffineTransform *_Nullable m, CGFloat x1, CGFloat y1, CGFloat x2, CGFloat y2, CGFloat radius);
CG_EXTERN void CGPathAddPath(CGMutablePathRef _Nullable path1, const CGAffineTransform *_Nullable m, CGPathRef _Nullable path2);
CG_EXTERN bool CGPathIsEmpty(CGPathRef _Nullable path);
CG_EXTERN bool CGPathIsRect(CGPathRef _Nullable path, CGRect *_Nullable rect);
CG_EXTERN CGPoint CGPathGetCurrentPoint(CGPathRef _Nullable path);
CG_EXTERN CGRect CGPathGetBoundingBox(CGPathRef _Nullable path);
CG_EXTERN CGRect CGPathGetPathBoundingBox(CGPathRef _Nullable path);
CG_EXTERN bool CGPathContainsPoint(CGPathRef _Nullable path, const CGAffineTransform *_Nullable m, CGPoint point, bool eoFill);
CG_EXTERN void CGPathApply(CGPathRef _Nullable path, void *_Nullable info, CGPathApplierFunction _Nullable function);
#if __BLOCKS__
CG_EXTERN void CGPathApplyWithBlock(CGPathRef path, CGPathApplyBlock CF_NOESCAPE block);
CG_EXTERN void CGPathApplyWithBlock2(CGPathRef path, CGPathApplyBlock2 CF_NOESCAPE block);
#endif
CG_EXTERN size_t CGPathGetNumberOfPoints(CGPathRef _Nullable path);
CG_EXTERN size_t CGPathGetNumberOfElements(CGPathRef _Nullable path);

/* ---- CGColorSpace / CGColor */
typedef struct MOS_CG_BRIDGED CGColorSpace *CGColorSpaceRef;
typedef struct MOS_CG_BRIDGED CGColor *CGColorRef;
typedef CF_ENUM(int32_t, CGColorSpaceModel) {
    kCGColorSpaceModelUnknown = -1, kCGColorSpaceModelMonochrome, kCGColorSpaceModelRGB, kCGColorSpaceModelCMYK,
    kCGColorSpaceModelLab, kCGColorSpaceModelDeviceN, kCGColorSpaceModelIndexed, kCGColorSpaceModelPattern, kCGColorSpaceModelXYZ
};
typedef CF_ENUM(int32_t, CGColorRenderingIntent) {
    kCGRenderingIntentDefault, kCGRenderingIntentAbsoluteColorimetric, kCGRenderingIntentRelativeColorimetric,
    kCGRenderingIntentPerceptual, kCGRenderingIntentSaturation
};

CG_EXTERN const CFStringRef kCGColorSpaceGenericGray;
CG_EXTERN const CFStringRef kCGColorSpaceGenericRGB;
CG_EXTERN const CFStringRef kCGColorSpaceGenericGrayGamma2_2;
CG_EXTERN const CFStringRef kCGColorSpaceSRGB;
CG_EXTERN const CFStringRef kCGColorSpaceExtendedSRGB;
CG_EXTERN const CFStringRef kCGColorSpaceLinearSRGB;
CG_EXTERN const CFStringRef kCGColorSpaceExtendedLinearSRGB;
CG_EXTERN const CFStringRef kCGColorSpaceDisplayP3;
CG_EXTERN const CFStringRef kCGColorSpaceExtendedDisplayP3;
CG_EXTERN const CFStringRef kCGColorSpaceLinearDisplayP3;
CG_EXTERN const CFStringRef kCGColorSpaceExtendedLinearDisplayP3;
CG_EXTERN const CFStringRef kCGColorSpaceExtendedGray;
CG_EXTERN const CFStringRef kCGColorSpaceLinearGray;
CG_EXTERN const CFStringRef kCGColorSpaceExtendedLinearGray;
CG_EXTERN const CFStringRef kCGColorSpaceITUR_2100_PQ;
CG_EXTERN const CFStringRef kCGColorSpaceITUR_2100_HLG;

CG_EXTERN CFTypeID CGColorSpaceGetTypeID(void);
CG_EXTERN CGColorSpaceRef _Nullable CGColorSpaceCreateWithName(CFStringRef _Nullable name);
CG_EXTERN CGColorSpaceRef _Nullable CGColorSpaceCreateDeviceRGB(void);
CG_EXTERN CGColorSpaceRef _Nullable CGColorSpaceCreateDeviceGray(void);
/* no mOS os espaços calibrados viram sRGB / cinza gamma 2.2 (sem gerenciamento de cor) */
CG_EXTERN CGColorSpaceRef _Nullable CGColorSpaceCreateCalibratedRGB(const CGFloat whitePoint[_Nonnull 3], const CGFloat blackPoint[_Nullable 3], const CGFloat gamma[_Nullable 3], const CGFloat matrix[_Nullable 9]);
CG_EXTERN CGColorSpaceRef _Nullable CGColorSpaceCreateCalibratedGray(const CGFloat whitePoint[_Nonnull 3], const CGFloat blackPoint[_Nullable 3], CGFloat gamma);
CG_EXTERN CGColorSpaceRef _Nullable CGColorSpaceRetain(CGColorSpaceRef _Nullable space);
CG_EXTERN void CGColorSpaceRelease(CGColorSpaceRef _Nullable space);
CG_EXTERN CFStringRef _Nullable CGColorSpaceGetName(CGColorSpaceRef _Nullable space);
CG_EXTERN CFStringRef _Nullable CGColorSpaceCopyName(CGColorSpaceRef _Nullable space);
CG_EXTERN size_t CGColorSpaceGetNumberOfComponents(CGColorSpaceRef _Nullable space);
CG_EXTERN CGColorSpaceModel CGColorSpaceGetModel(CGColorSpaceRef _Nullable space);
CG_EXTERN bool CGColorSpaceUsesExtendedRange(CGColorSpaceRef space);
CG_EXTERN bool CGColorSpaceIsWideGamutRGB(CGColorSpaceRef space);

CG_EXTERN CFTypeID CGColorGetTypeID(void);
CG_EXTERN CGColorRef _Nullable CGColorCreate(CGColorSpaceRef _Nullable space, const CGFloat *_Nullable components);
CG_EXTERN CGColorRef CGColorCreateGenericRGB(CGFloat red, CGFloat green, CGFloat blue, CGFloat alpha);
CG_EXTERN CGColorRef CGColorCreateGenericGray(CGFloat gray, CGFloat alpha);
CG_EXTERN CGColorRef CGColorCreateSRGB(CGFloat red, CGFloat green, CGFloat blue, CGFloat alpha);
CG_EXTERN CGColorRef _Nullable CGColorCreateCopy(CGColorRef _Nullable color);
CG_EXTERN CGColorRef _Nullable CGColorCreateCopyWithAlpha(CGColorRef _Nullable color, CGFloat alpha);
CG_EXTERN CGColorRef _Nullable CGColorRetain(CGColorRef _Nullable color);
CG_EXTERN void CGColorRelease(CGColorRef _Nullable color);
CG_EXTERN bool CGColorEqualToColor(CGColorRef _Nullable color1, CGColorRef _Nullable color2);
CG_EXTERN size_t CGColorGetNumberOfComponents(CGColorRef _Nullable color);
CG_EXTERN const CGFloat *_Nullable CGColorGetComponents(CGColorRef _Nullable color);
CG_EXTERN CGFloat CGColorGetAlpha(CGColorRef _Nullable color);
CG_EXTERN CGColorSpaceRef _Nullable CGColorGetColorSpace(CGColorRef _Nullable color);

/* ---- tipos opacos citados por outras APIs (contexto, imagem, fonte): ainda sem API pública aqui */
typedef struct MOS_CG_BRIDGED CGContext *CGContextRef;
typedef struct MOS_CG_BRIDGED CGImage *CGImageRef;
typedef struct MOS_CG_BRIDGED CGFont *CGFontRef;
typedef uint16_t CGGlyph;

CF_ASSUME_NONNULL_END
CG_EXTERN_C_END

#endif /* MOS_COREGRAPHICS_H */
