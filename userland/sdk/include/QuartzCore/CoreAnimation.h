/* mOS SDK — QuartzCore: animações e camadas especiais (libsystem/coreanimation.c) */
#pragma once
#import <QuartzCore/CAMetalLayer.h>
#ifndef CGPathRef   /* sem o CoreGraphics.h da ponte Swift: tipos mínimos */
typedef struct CGPath *CGMutablePathRef;
typedef const struct CGPath *CGPathRef;
typedef struct CGColor *CGColorRef;
FOUNDATION_EXPORT CGMutablePathRef CGPathCreateMutable(void);
FOUNDATION_EXPORT void CGPathMoveToPoint(CGMutablePathRef p, const void *m, CGFloat x, CGFloat y);
FOUNDATION_EXPORT void CGPathAddLineToPoint(CGMutablePathRef p, const void *m, CGFloat x, CGFloat y);
FOUNDATION_EXPORT void CGPathAddArc(CGMutablePathRef p, const void *m, CGFloat x, CGFloat y, CGFloat r, CGFloat a0, CGFloat a1, bool cw);
FOUNDATION_EXPORT void CGPathAddEllipseInRect(CGMutablePathRef p, const void *m, CGRect r);
FOUNDATION_EXPORT void CGPathCloseSubpath(CGMutablePathRef p);
FOUNDATION_EXPORT void CGPathRelease(CGPathRef p);
FOUNDATION_EXPORT CGColorRef CGColorCreateSRGB(CGFloat r, CGFloat g, CGFloat b, CGFloat a);
#endif
#ifndef MOS_CFTIMEINTERVAL
#define MOS_CFTIMEINTERVAL
typedef double CFTimeInterval;
#endif

typedef struct CATransform3D { CGFloat m11, m12, m13, m14, m21, m22, m23, m24, m31, m32, m33, m34, m41, m42, m43, m44; } CATransform3D;
FOUNDATION_EXPORT const CATransform3D CATransform3DIdentity;
FOUNDATION_EXPORT CATransform3D CATransform3DMakeTranslation(CGFloat x, CGFloat y, CGFloat z);
FOUNDATION_EXPORT CATransform3D CATransform3DMakeScale(CGFloat x, CGFloat y, CGFloat z);
FOUNDATION_EXPORT CATransform3D CATransform3DMakeRotation(CGFloat a, CGFloat x, CGFloat y, CGFloat z);
FOUNDATION_EXPORT CATransform3D CATransform3DRotate(CATransform3D t, CGFloat a, CGFloat x, CGFloat y, CGFloat z);
FOUNDATION_EXPORT CATransform3D CATransform3DConcat(CATransform3D a, CATransform3D b);
typedef NSString *CAMediaTimingFunctionName;
FOUNDATION_EXPORT NSString *const kCAMediaTimingFunctionLinear, *const kCAMediaTimingFunctionEaseIn, *const kCAMediaTimingFunctionEaseOut,
    *const kCAMediaTimingFunctionEaseInEaseOut, *const kCAMediaTimingFunctionDefault;
FOUNDATION_EXPORT NSString *const kCAFillModeForwards, *const kCAFillModeBackwards, *const kCAFillModeBoth, *const kCAFillModeRemoved;
FOUNDATION_EXPORT NSString *const kCALineCapButt, *const kCALineCapRound, *const kCALineCapSquare;
FOUNDATION_EXPORT NSString *const kCALineJoinMiter, *const kCALineJoinRound, *const kCALineJoinBevel;
FOUNDATION_EXPORT NSString *const kCAFillRuleNonZero, *const kCAFillRuleEvenOdd;
FOUNDATION_EXPORT NSString *const kCAGradientLayerAxial, *const kCAGradientLayerRadial, *const kCAGradientLayerConic;
FOUNDATION_EXPORT NSString *const kCAAlignmentNatural, *const kCAAlignmentLeft, *const kCAAlignmentRight, *const kCAAlignmentCenter;
FOUNDATION_EXPORT NSString *const kCATruncationNone, *const kCATruncationEnd;
FOUNDATION_EXPORT NSString *const kCAAnimationLinear, *const kCAAnimationDiscrete, *const kCAAnimationPaced;
FOUNDATION_EXPORT CFTimeInterval CACurrentMediaTime(void);

@protocol CAAnimationDelegate <NSObject>
@optional
- (void)animationDidStart:(id)anim;
- (void)animationDidStop:(id)anim finished:(BOOL)flag;
@end
@interface CAMediaTimingFunction : NSObject
+ (instancetype)functionWithName:(NSString *)name;
+ (instancetype)functionWithControlPoints:(float)c1x :(float)c1y :(float)c2x :(float)c2y;
- (void)getControlPointAtIndex:(size_t)i values:(float *)v;
@end
@interface CAAnimation : NSObject <NSCopying>
+ (instancetype)animation;
@property (strong) CAMediaTimingFunction *timingFunction;
@property (strong) id<CAAnimationDelegate> delegate;
@property (getter=isRemovedOnCompletion) BOOL removedOnCompletion;
@property CFTimeInterval duration, beginTime, timeOffset, repeatDuration;
@property float speed, repeatCount;
@property BOOL autoreverses;
@property (copy) NSString *fillMode;
@end
@interface CAPropertyAnimation : CAAnimation
+ (instancetype)animationWithKeyPath:(NSString *)path;
@property (copy) NSString *keyPath;
@property (getter=isAdditive) BOOL additive;
@property (getter=isCumulative) BOOL cumulative;
@end
@interface CABasicAnimation : CAPropertyAnimation
@property (strong) id fromValue, toValue, byValue;
@end
@interface CASpringAnimation : CABasicAnimation
@property CGFloat mass, stiffness, damping, initialVelocity;
@property (readonly) CFTimeInterval settlingDuration;
@end
@interface CAKeyframeAnimation : CAPropertyAnimation
@property (copy) NSArray *values;
@property CGPathRef path;
@property (copy) NSArray<NSNumber *> *keyTimes;
@property (copy) NSArray<CAMediaTimingFunction *> *timingFunctions;
@property (copy) NSString *calculationMode;
@end
@interface CAAnimationGroup : CAAnimation
@property (copy) NSArray<CAAnimation *> *animations;
@end
@interface CATransition : CAAnimation
@property (copy) NSString *type, *subtype;
@end
@interface CALayer (MOSAnimation)
- (void)addAnimation:(CAAnimation *)anim forKey:(NSString *)key;
- (void)removeAnimationForKey:(NSString *)key;
- (void)removeAllAnimations;
- (CAAnimation *)animationForKey:(NSString *)key;
@property CATransform3D transform;
@property CGColorRef backgroundColor, borderColor;
@property CGFloat cornerRadius, borderWidth;
@property (getter=isGeometryFlipped) BOOL geometryFlipped;
@property (readonly) CALayer *superlayer;
@end
@interface CAShapeLayer : CALayer
@property CGPathRef path;
@property CGColorRef fillColor, strokeColor;
@property (copy) NSString *fillRule, *lineCap, *lineJoin;
@property CGFloat strokeStart, strokeEnd, lineWidth, miterLimit, lineDashPhase;
@property (copy) NSArray<NSNumber *> *lineDashPattern;
@end
@interface CAGradientLayer : CALayer
@property (copy) NSArray *colors;
@property (copy) NSArray<NSNumber *> *locations;
@property CGPoint startPoint, endPoint;
@property (copy) NSString *type;
@end
@interface CATextLayer : CALayer
@property (copy) id string;
@property (strong) id font;
@property CGFloat fontSize;
@property CGColorRef foregroundColor;
@property (getter=isWrapped) BOOL wrapped;
@property (copy) NSString *truncationMode, *alignmentMode;
@end
@interface CADisplayLink : NSObject
+ (CADisplayLink *)displayLinkWithTarget:(id)target selector:(SEL)sel;
- (void)addToRunLoop:(id)runloop forMode:(NSString *)mode;
- (void)invalidate;
@property (readonly) CFTimeInterval timestamp, targetTimestamp, duration;
@property (getter=isPaused) BOOL paused;
@property NSInteger preferredFramesPerSecond;
@end
@interface NSValue : NSObject <NSCopying>
- (void)getValue:(void *)value;
@property (readonly) const char *objCType;
+ (NSValue *)valueWithPoint:(CGPoint)p;
+ (NSValue *)valueWithSize:(CGSize)s;
+ (NSValue *)valueWithRect:(CGRect)r;
+ (NSValue *)valueWithCATransform3D:(CATransform3D)t;
@property (readonly) CGPoint pointValue;
@property (readonly) CGRect rectValue;
@property (readonly) CATransform3D CATransform3DValue;
@end
