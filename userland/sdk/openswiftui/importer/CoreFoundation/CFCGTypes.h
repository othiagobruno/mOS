/*
 * mOS SDK (importador) — tipos geométricos do CoreGraphics em C, como no CFCGTypes.h do swift-corelibs-foundation.
 * Vivem no módulo clang CoreFoundation para haver UMA declaração de cada tipo no Swift: o Foundation do mOS
 * estende CGPoint/CGSize/CGRect; os cabeçalhos C das deps (OpenRenderBox: OPENRENDERBOX_CF_CGTYPES, CoreGraphics.h do
 * SDK) usam a mesma guarda e ficam com estes; o OpenCoreGraphics sombreia CGVector/CGAffineTransform com os dele.
 * O typedef C CGFloat é mapeado pelo importador para CoreGraphics.CGFloat (módulo Swift CoreGraphics do mOS).
 */
#if !defined(__COREFOUNDATION_CFCGTYPES__)
#define __COREFOUNDATION_CFCGTYPES__ 1

#include <CoreFoundation/CoreFoundation.h>
#include <float.h>
#include <stdint.h>

#if defined(__swift__) && !defined(MOS_SWIFT_OBJC)   /* com interop ObjC (MOS_SWIFT_OBJC) os nomes são os da Apple */
/* Para o Swift, CGVector/CGAffineTransform em C têm outro nome: o OpenCoreGraphics define os seus em Swift (o C só
 * aparece nas assinaturas das APIs C, como MOSCGAffineTransform, com o mesmo layout). */
#define CGVector MOSCGVector
#define CGAffineTransform MOSCGAffineTransform
#define CGAffineTransformComponents MOSCGAffineTransformComponents
#endif

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
