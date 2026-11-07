/*
 * mOS libSystem — o resto do QuartzCore que os apps nativos usam (WhatsApp/Catalyst, SwiftUI), em cima do
 * CALayer do quartzcore.c:
 *
 *  - NSValue com structs (CGPoint/CGSize/CGRect/CATransform3D/CGAffineTransform/NSRange/ponteiros).
 *  - CAMediaTimingFunction (curvas de Bézier e os nomes padrão), CAValueFunction.
 *  - CAAnimation, CAPropertyAnimation, CABasicAnimation, CASpringAnimation, CAKeyframeAnimation (valores,
 *    keyTimes, timingFunctions, modos discreto/linear e caminho CGPath), CAAnimationGroup, CATransition.
 *    -[CALayer addAnimation:forKey:] passa a animar de verdade: um animador a 60 Hz calcula o valor a cada
 *    quadro (duração, beginTime, speed, timeOffset, repeatCount/repeatDuration, autoreverses, fillMode,
 *    removedOnCompletion, additive, byValue) e o aplica no layer pelo setter tipado; os delegates recebem
 *    animationDidStart: e animationDidStop:finished:.
 *  - CAShapeLayer (path, preenchimento nonzero/even-odd, traço com largura, junções e pontas, tracejado,
 *    strokeStart/strokeEnd), CAGradientLayer (axial, radial, cônico, locations) e CATextLayer (fonte,
 *    tamanho, cor, alinhamento, quebra de linha, reticências): desenhados com antisserrilhado num IOSurface
 *    BGRA pré-multiplicado que vira o contents do layer, refeito quando uma propriedade ou o tamanho muda.
 *  - CAReplicatorLayer, CATiledLayer, CAEmitterLayer/CAEmitterCell: as propriedades existem (o desenho é o
 *    do CALayer comum: réplicas e partículas não são geradas).
 *  - CADisplayLink (+displayLinkWithTarget:selector: e -[NSView/NSWindow/NSScreen displayLinkWithTarget:selector:]).
 *  - CARenderer (sem desenho), CATransform3DMakeRotation/Rotate, CAFrameRateRange, constantes kCA*.
 * As propriedades usam o mecanismo genérico do metal.c (mtl_def_desc).
 */
#include "metalint.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void *_Block_copy(const void *b);
void *class_replaceMethod(cx_class_t *c, SEL s, void *imp, const char *types);
void *objc_msgSend_stret(void *ret, id self, SEL op, ...);
id cx_color(double r, double g, double b, double a);
double CACurrentMediaTime(void);
double sqrt(double), sin(double), cos(double), atan2(double, double), exp(double), fabs(double), floor(double), ceil(double), pow(double, double), fmod(double, double);
typedef id IOSurfaceRef;
IOSurfaceRef IOSurfaceCreate(id props);
void *IOSurfaceGetBaseAddress(IOSurfaceRef s);
u64 IOSurfaceGetBytesPerRow(IOSurfaceRef s);
u64 IOSurfaceGetWidth(IOSurfaceRef s);
u64 IOSurfaceGetHeight(IOSurfaceRef s);
int IOSurfaceLock(IOSurfaceRef s, u32 opts, u32 *seed);
int IOSurfaceUnlock(IOSurfaceRef s, u32 opts, u32 *seed);
extern const void *kIOSurfaceWidth, *kIOSurfaceHeight, *kIOSurfaceBytesPerElement, *kIOSurfacePixelFormat;
typedef struct { double x, y; } CGP;
typedef struct { double w, h; } CGS;
typedef struct { CGP o; CGS s; } CGR;
typedef struct { double m[16]; } T3;
typedef struct { int type; CGP *pts; } cgelem_t;
void CGPathApply(const void *path, void *info, void (*fn)(void *, const cgelem_t *));
void *CGPathCreateMutable(void);
void CGPathMoveToPoint(void *p, const void *m, double x, double y);
void CGPathAddLineToPoint(void *p, const void *m, double x, double y);
void CGPathCloseSubpath(void *p);
id CTFontCreateWithName(id name, double size, const void *matrix);
double CTFontGetAscent(id f);
double CTFontGetDescent(id f);
BOOL CTFontGetGlyphsForCharacters(id f, const u16 *chars, u16 *glyphs, long n);
typedef struct { double x, y; } ct_pt_t;
typedef struct { double w, h; } ct_sz_t;
double CTFontGetAdvancesForGlyphs(id f, int orientation, const u16 *glyphs, ct_sz_t *adv, long n);
void CTFontDrawGlyphs(id f, const u16 *glyphs, const ct_pt_t *pos, size_t n, void *ctx);
void *CGBitmapContextCreate(void *data, size_t w, size_t h, size_t bpc, size_t bpr, void *space, u32 bitmapInfo);
void CGContextRelease(void *c);
void cx_bm_set_fill(const void *c, u32 argb);
T3 CATransform3DConcat(T3 a, T3 b);
extern const T3 CATransform3DIdentity;

#define CLASS(N) cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N); cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N);
CLASS(CAMediaTimingFunction) CLASS(CAValueFunction) CLASS(CAAnimation) CLASS(CAPropertyAnimation) CLASS(CABasicAnimation)
CLASS(CASpringAnimation) CLASS(CAKeyframeAnimation) CLASS(CAAnimationGroup) CLASS(CATransition)
CLASS(CAShapeLayer) CLASS(CAGradientLayer) CLASS(CATextLayer) CLASS(CAReplicatorLayer) CLASS(CATiledLayer)
CLASS(CAEmitterLayer) CLASS(CAEmitterCell) CLASS(CADisplayLink) CLASS(CARenderer) CLASS(CAScrollLayer)
static cx_class_t CAMosAnimator_c, CAMosAnimator_m, NSConcreteStructValue_c, NSConcreteStructValue_m;
extern cx_class_t CALayer_c __asm__("OBJC_CLASS_$_CALayer");

/* ---- constantes */
extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } ca_cfconst_t;
#define CFCONST(var, text) static ca_cfconst_t var##_s = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; const void *var = &var##_s;
CFCONST(kCAAlignmentNatural, "natural") CFCONST(kCAAlignmentLeft, "left") CFCONST(kCAAlignmentRight, "right")
CFCONST(kCAAlignmentCenter, "center") CFCONST(kCAAlignmentJustified, "justified")
CFCONST(kCAAnimationLinear, "linear") CFCONST(kCAAnimationDiscrete, "discrete") CFCONST(kCAAnimationPaced, "paced")
CFCONST(kCAAnimationCubic, "cubic") CFCONST(kCAAnimationCubicPaced, "cubicPaced")
CFCONST(kCAAnimationRotateAuto, "auto") CFCONST(kCAAnimationRotateAutoReverse, "autoReverse")
CFCONST(kCACornerCurveCircular, "circular") CFCONST(kCACornerCurveContinuous, "continuous")
CFCONST(kCAEmitterLayerPoint, "point") CFCONST(kCAEmitterLayerLine, "line") CFCONST(kCAEmitterLayerRectangle, "rectangle")
CFCONST(kCAEmitterLayerCuboid, "cuboid") CFCONST(kCAEmitterLayerCircle, "circle") CFCONST(kCAEmitterLayerSphere, "sphere")
CFCONST(kCAEmitterLayerPoints, "points") CFCONST(kCAEmitterLayerOutline, "outline") CFCONST(kCAEmitterLayerSurface, "surface")
CFCONST(kCAEmitterLayerVolume, "volume") CFCONST(kCAEmitterLayerUnordered, "unordered")
CFCONST(kCAEmitterLayerOldestFirst, "oldestFirst") CFCONST(kCAEmitterLayerOldestLast, "oldestLast")
CFCONST(kCAEmitterLayerBackToFront, "backToFront") CFCONST(kCAEmitterLayerAdditive, "additive")
CFCONST(kCAFillRuleNonZero, "non-zero") CFCONST(kCAFillRuleEvenOdd, "even-odd")
CFCONST(kCAGradientLayerAxial, "axial") CFCONST(kCAGradientLayerRadial, "radial") CFCONST(kCAGradientLayerConic, "conic")
CFCONST(kCALineCapButt, "butt") CFCONST(kCALineCapRound, "round") CFCONST(kCALineCapSquare, "square")
CFCONST(kCALineJoinMiter, "miter") CFCONST(kCALineJoinRound, "round") CFCONST(kCALineJoinBevel, "bevel")
CFCONST(kCARendererMetalCommandQueue, "MetalCommandQueue") CFCONST(kCARendererColorSpace, "ColorSpace")
CFCONST(kCATransitionFade, "fade") CFCONST(kCATransitionMoveIn, "moveIn") CFCONST(kCATransitionPush, "push")
CFCONST(kCATransitionReveal, "reveal") CFCONST(kCATransitionFromRight, "fromRight") CFCONST(kCATransitionFromLeft, "fromLeft")
CFCONST(kCATransitionFromTop, "fromTop") CFCONST(kCATransitionFromBottom, "fromBottom")
CFCONST(kCATruncationNone, "none") CFCONST(kCATruncationStart, "start") CFCONST(kCATruncationEnd, "end")
CFCONST(kCATruncationMiddle, "middle")
CFCONST(kCAValueFunctionRotateX, "rotateX") CFCONST(kCAValueFunctionRotateY, "rotateY") CFCONST(kCAValueFunctionRotateZ, "rotateZ")
CFCONST(kCAValueFunctionScale, "scale") CFCONST(kCAValueFunctionScaleX, "scaleX") CFCONST(kCAValueFunctionScaleY, "scaleY")
CFCONST(kCAValueFunctionScaleZ, "scaleZ") CFCONST(kCAValueFunctionTranslate, "translate")
CFCONST(kCAValueFunctionTranslateX, "translateX") CFCONST(kCAValueFunctionTranslateY, "translateY")
CFCONST(kCAValueFunctionTranslateZ, "translateZ")

/* ---- funções */
T3 CATransform3DMakeRotation(double a, double x, double y, double z) {
    double l = sqrt(x * x + y * y + z * z);
    T3 t = CATransform3DIdentity;
    if (l == 0) return t;
    x /= l; y /= l; z /= l;
    double c = cos(a), s = sin(a), k = 1 - c;
    t.m[0] = x * x * k + c;     t.m[1] = x * y * k + z * s; t.m[2] = x * z * k - y * s;
    t.m[4] = x * y * k - z * s; t.m[5] = y * y * k + c;     t.m[6] = y * z * k + x * s;
    t.m[8] = x * z * k + y * s; t.m[9] = y * z * k - x * s; t.m[10] = z * z * k + c;
    return t;
}
T3 CATransform3DRotate(T3 t, double a, double x, double y, double z) { return CATransform3DConcat(CATransform3DMakeRotation(a, x, y, z), t); }
typedef struct { float minimum, maximum, preferred; } CAFrameRateRange;
const CAFrameRateRange CAFrameRateRangeDefault = {0, 0, 0};
CAFrameRateRange CAFrameRateRangeMake(float mn, float mx, float pf) { CAFrameRateRange r = {mn, mx, pf}; return r; }
BOOL CAFrameRateRangeIsEqualToRange(CAFrameRateRange a, CAFrameRateRange b) { return a.minimum == b.minimum && a.maximum == b.maximum && a.preferred == b.preferred; }

/* ================================================================ NSValue com structs */
typedef struct { cx_class_t *isa; char type[48]; u32 size; u8 data[128]; } sval_t;
static u32 enc_size(const char *t) {
    if (!t) return 0;
    if (!strcmp(t, "{CGPoint=dd}") || !strcmp(t, "{CGSize=dd}") || !strcmp(t, "{_NSPoint=dd}") || !strcmp(t, "{_NSSize=dd}") || !strcmp(t, "{_NSRange=QQ}")) return 16;
    if (!strncmp(t, "{CGRect=", 8) || !strncmp(t, "{_NSRect=", 9) || !strncmp(t, "{UIEdgeInsets=", 14) || !strncmp(t, "{NSEdgeInsets=", 14)) return 32;
    if (!strncmp(t, "{CATransform3D=", 15)) return 128;
    if (!strncmp(t, "{CGAffineTransform=", 19)) return 48;
    if (!strncmp(t, "{CGVector=", 10) || !strncmp(t, "{UIOffset=", 10)) return 16;
    if (t[0] == '^' || t[0] == '@' || t[0] == '*' || t[0] == '#' || t[0] == ':' || t[0] == 'q' || t[0] == 'Q' || t[0] == 'd' || t[0] == 'l' || t[0] == 'L') return 8;
    if (t[0] == 'i' || t[0] == 'I' || t[0] == 'f') return 4;
    if (t[0] == 's' || t[0] == 'S') return 2;
    if (t[0] == 'c' || t[0] == 'C' || t[0] == 'B') return 1;
    /* struct desconhecida: conta os campos d/q/Q/i/f */
    u32 n = 0;
    for (const char *p = strchr(t, '='); p && *p && *p != '}'; p++) {
        if (*p == 'd' || *p == 'q' || *p == 'Q' || *p == '^' || *p == '@') n = (n + 7) & ~7u, n += 8;
        else if (*p == 'f' || *p == 'i' || *p == 'I') n = (n + 3) & ~3u, n += 4;
    }
    return n > 128 ? 128 : (n ? n : 8);
}
static id sv_make(const void *bytes, const char *type) {
    sval_t *v = (sval_t *)cx_alloc_instance(&NSConcreteStructValue_c);
    strlcpy(v->type, type ? type : "?", sizeof v->type);
    v->size = enc_size(v->type);
    if (bytes && v->size) memcpy(v->data, bytes, v->size);
    return (id)v;
}
static id sv_with_bytes(id cls, SEL c, const void *b, const char *t) { (void)cls; (void)c; return objc_autorelease(sv_make(b, t)); }
static id sv_init_bytes(id self, SEL c, const void *b, const char *t) { (void)c; objc_release(self); return sv_make(b, t); }
static id sv_point(id cls, SEL c, CGP p) { (void)cls; (void)c; return objc_autorelease(sv_make(&p, "{CGPoint=dd}")); }
static id sv_size(id cls, SEL c, CGS s) { (void)cls; (void)c; return objc_autorelease(sv_make(&s, "{CGSize=dd}")); }
static id sv_rect(id cls, SEL c, CGR r) { (void)cls; (void)c; return objc_autorelease(sv_make(&r, "{CGRect={CGPoint=dd}{CGSize=dd}}")); }
static id sv_t3(id cls, SEL c, T3 t) { (void)cls; (void)c; return objc_autorelease(sv_make(&t, "{CATransform3D=dddddddddddddddd}")); }
typedef struct { double a, b, c, d, tx, ty; } AT;
static id sv_affine(id cls, SEL c, AT t) { (void)cls; (void)c; return objc_autorelease(sv_make(&t, "{CGAffineTransform=dddddd}")); }
typedef struct { u64 loc, len; } NR;
static id sv_range(id cls, SEL c, NR r) { (void)cls; (void)c; return objc_autorelease(sv_make(&r, "{_NSRange=QQ}")); }
static id sv_ptr(id cls, SEL c, void *p) { (void)cls; (void)c; return objc_autorelease(sv_make(&p, "^v")); }
static id sv_nonret(id cls, SEL c, id p) { (void)cls; (void)c; return objc_autorelease(sv_make(&p, "@")); }
typedef struct { double t, l, b, r; } EI;
static id sv_insets(id cls, SEL c, EI e) { (void)cls; (void)c; return objc_autorelease(sv_make(&e, "{UIEdgeInsets=dddd}")); }
static CGP sv_getp(id s, SEL c) { (void)c; CGP p; memcpy(&p, ((sval_t *)s)->data, sizeof p); return p; }
static CGS sv_gets(id s, SEL c) { (void)c; CGS p; memcpy(&p, ((sval_t *)s)->data, sizeof p); return p; }
static CGR sv_getr(id s, SEL c) { (void)c; CGR p; memcpy(&p, ((sval_t *)s)->data, sizeof p); return p; }
static T3 sv_gett(id s, SEL c) { (void)c; T3 p; memcpy(&p, ((sval_t *)s)->data, sizeof p); return p; }
static AT sv_geta(id s, SEL c) { (void)c; AT p; memcpy(&p, ((sval_t *)s)->data, sizeof p); return p; }
static NR sv_getrange(id s, SEL c) { (void)c; NR p; memcpy(&p, ((sval_t *)s)->data, sizeof p); return p; }
static EI sv_getei(id s, SEL c) { (void)c; EI p; memcpy(&p, ((sval_t *)s)->data, sizeof p); return p; }
static void *sv_getptr(id s, SEL c) { (void)c; void *p; memcpy(&p, ((sval_t *)s)->data, sizeof p); return p; }
static void sv_getvalue(id s, SEL c, void *out) { (void)c; if (out) memcpy(out, ((sval_t *)s)->data, ((sval_t *)s)->size); }
static void sv_getvalue_n(id s, SEL c, void *out, u64 n) { (void)c; sval_t *v = (sval_t *)s; if (out) memcpy(out, v->data, n < v->size ? n : v->size); }
static const char *sv_type(id s, SEL c) { (void)c; return ((sval_t *)s)->type; }
static BOOL sv_equal(id s, SEL c, id o) {
    (void)c;
    if (s == o) return YES;
    if (!o || o->isa != &NSConcreteStructValue_c) return NO;
    sval_t *a = (sval_t *)s, *b = (sval_t *)o;
    return !strcmp(a->type, b->type) && a->size == b->size && !memcmp(a->data, b->data, a->size);
}
static u64 sv_hash(id s, SEL c) { (void)c; sval_t *v = (sval_t *)s; u64 h = 1469598103934665603ull; for (u32 i = 0; i < v->size; i++) h = (h ^ v->data[i]) * 1099511628211ull; return h; }
static id sv_copy(id s, SEL c) { (void)c; return objc_retain(s); }
static id sv_desc(id s, SEL c) {
    (void)c;
    sval_t *v = (sval_t *)s;
    char b[256];
    const double *d = (const double *)v->data;
    if (v->size == 16 && v->type[0] == '{' && strchr(v->type, 'd')) snprintf(b, sizeof b, "NSPoint: {%g, %g}", d[0], d[1]);
    else if (v->size == 32) snprintf(b, sizeof b, "NSRect: {{%g, %g}, {%g, %g}}", d[0], d[1], d[2], d[3]);
    else snprintf(b, sizeof b, "<%s, %u bytes>", v->type, v->size);
    return objc_autorelease(cx_str(b));
}

/* ================================================================ propriedades (listas completas por classe) */
#define P_END {0, 0, 0, 0, 0}
#define ANIM_P {"timingFunction", 'o'}, {"delegate", 'o'}, {"removedOnCompletion", 'b', 1}, {"duration", 'd'}, {"beginTime", 'd'}, \
    {"speed", 'f', 0, 1.0}, {"timeOffset", 'd'}, {"repeatCount", 'f'}, {"repeatDuration", 'd'}, {"autoreverses", 'b'}, \
    {"fillMode", 'o'}, {"enabled", 'b', 1}
#define PROP_P {"keyPath", 'o'}, {"additive", 'b'}, {"cumulative", 'b'}, {"valueFunction", 'o'}
#define BASIC_P {"fromValue", 'o'}, {"toValue", 'o'}, {"byValue", 'o'}
static const pspec_t ps_anim[] = {ANIM_P, P_END};
static const pspec_t ps_panim[] = {ANIM_P, PROP_P, P_END};
static const pspec_t ps_basic[] = {ANIM_P, PROP_P, BASIC_P, P_END};
static const pspec_t ps_spring[] = {ANIM_P, PROP_P, BASIC_P, {"mass", 'd', 0, 1}, {"stiffness", 'd', 0, 100}, {"damping", 'd', 0, 10},
                                    {"initialVelocity", 'd'}, {"allowsOverdamping", 'b'}, P_END};
static const pspec_t ps_key[] = {ANIM_P, PROP_P, {"values", 'o'}, {"path", 'o'}, {"keyTimes", 'o'}, {"timingFunctions", 'o'},
                                 {"calculationMode", 'o'}, {"rotationMode", 'o'}, {"tensionValues", 'o'}, {"continuityValues", 'o'},
                                 {"biasValues", 'o'}, P_END};
static const pspec_t ps_group[] = {ANIM_P, {"animations", 'o'}, P_END};
static const pspec_t ps_trans[] = {ANIM_P, {"type", 'o'}, {"subtype", 'o'}, {"startProgress", 'f'}, {"endProgress", 'f', 0, 1.0}, {"filter", 'o'}, P_END};
static const pspec_t ps_shape[] = {{"path", 'o'}, {"fillColor", 'o'}, {"fillRule", 'o'}, {"strokeColor", 'o'}, {"strokeStart", 'd'},
                                   {"strokeEnd", 'd', 0, 1}, {"lineWidth", 'd', 0, 1}, {"miterLimit", 'd', 0, 10}, {"lineCap", 'o'},
                                   {"lineJoin", 'o'}, {"lineDashPhase", 'd'}, {"lineDashPattern", 'o'}, {"_mosSurface", 'o'},
                                   {"_mosInited", 'b'}, P_END};
static const pspec_t ps_grad[] = {{"colors", 'o'}, {"locations", 'o'}, {"startPoint", 'p', 0, 0.5}, {"endPoint", 'p', 1000, 0.5},
                                  {"type", 'o'}, {"_mosSurface", 'o'}, P_END};
static const pspec_t ps_text[] = {{"string", 'o'}, {"font", 'o'}, {"fontSize", 'd', 0, 36}, {"foregroundColor", 'o'}, {"wrapped", 'b'},
                                  {"truncationMode", 'o'}, {"alignmentMode", 'o'}, {"allowsFontSubpixelQuantization", 'b'},
                                  {"_mosSurface", 'o'}, {"_mosInited", 'b'}, P_END};
static const pspec_t ps_repl[] = {{"instanceCount", 'u', 1}, {"instanceDelay", 'd'}, {"instanceTransform", 't'}, {"preservesDepth", 'b'},
                                  {"instanceColor", 'o'}, {"instanceRedOffset", 'f'}, {"instanceGreenOffset", 'f'},
                                  {"instanceBlueOffset", 'f'}, {"instanceAlphaOffset", 'f'}, P_END};
static const pspec_t ps_tiled[] = {{"levelsOfDetail", 'u', 1}, {"levelsOfDetailBias", 'u'}, {"tileSize", 'p', 256000, 256}, P_END};
static const pspec_t ps_emitter[] = {{"emitterCells", 'o'}, {"birthRate", 'f', 0, 1}, {"lifetime", 'f', 0, 1}, {"emitterPosition", 'p'},
                                     {"emitterZPosition", 'd'}, {"emitterSize", 'p'}, {"emitterDepth", 'd'}, {"emitterShape", 'o'},
                                     {"emitterMode", 'o'}, {"renderMode", 'o'}, {"preservesDepth", 'b'}, {"velocity", 'f', 0, 1},
                                     {"scale", 'f', 0, 1}, {"spin", 'f', 0, 1}, {"seed", 'u'}, P_END};
static const pspec_t ps_cell[] = {{"name", 'o'}, {"enabled", 'b', 1}, {"birthRate", 'f'}, {"lifetime", 'f'}, {"lifetimeRange", 'f'},
                                  {"emissionLatitude", 'd'}, {"emissionLongitude", 'd'}, {"emissionRange", 'd'}, {"velocity", 'd'},
                                  {"velocityRange", 'd'}, {"xAcceleration", 'd'}, {"yAcceleration", 'd'}, {"zAcceleration", 'd'},
                                  {"scale", 'd', 0, 1}, {"scaleRange", 'd'}, {"scaleSpeed", 'd'}, {"spin", 'd'}, {"spinRange", 'd'},
                                  {"color", 'o'}, {"redRange", 'f'}, {"greenRange", 'f'}, {"blueRange", 'f'}, {"alphaRange", 'f'},
                                  {"redSpeed", 'f'}, {"greenSpeed", 'f'}, {"blueSpeed", 'f'}, {"alphaSpeed", 'f'}, {"contents", 'o'},
                                  {"contentsScale", 'd', 0, 1}, {"emitterCells", 'o'}, {"magnificationFilter", 'o'},
                                  {"minificationFilter", 'o'}, {"style", 'o'}, {"beginTime", 'd'}, {"duration", 'd'},
                                  {"speed", 'f', 0, 1}, {"timeOffset", 'd'}, {"repeatCount", 'f'}, {"repeatDuration", 'd'},
                                  {"autoreverses", 'b'}, {"fillMode", 'o'}, P_END};
static const pspec_t ps_dlink[] = {{"timestamp", 'd'}, {"targetTimestamp", 'd'}, {"duration", 'd', 0, 1.0 / 60}, {"paused", 'b'},
                                   {"preferredFramesPerSecond", 'u'}, {"frameInterval", 'u', 1},
                                   {"_target", 'o'}, {"_sel", 'u'}, {"_valid", 'b', 1}, {"_scheduled", 'b'}, P_END};
static const pspec_t ps_renderer[] = {{"layer", 'o'}, {"bounds", 'p'}, {"_boundsSize", 'p'}, {"_texture", 'o'}, P_END};
static const pspec_t ps_none[] = {P_END};

/* ================================================================ timing */
typedef struct { cx_class_t *isa; double p[4]; } tf_t;
static double bez(double a, double b, double t) { double u = 1 - t; return 3 * u * u * t * a + 3 * u * t * t * b + t * t * t; }
static double tf_eval(id f, double x) {
    if (!f || f->isa != &CAMediaTimingFunction_c) return x;
    tf_t *t = (tf_t *)f;
    if (x <= 0) return 0;
    if (x >= 1) return 1;
    double lo = 0, hi = 1, u = x;
    for (int i = 0; i < 40; i++) {
        double v = bez(t->p[0], t->p[2], u);
        if (fabs(v - x) < 1e-6) break;
        if (v < x) lo = u; else hi = u;
        u = (lo + hi) / 2;
    }
    return bez(t->p[1], t->p[3], u);
}
static id tf_make(double a, double b, double c, double d) {
    tf_t *t = (tf_t *)cx_alloc_instance(&CAMediaTimingFunction_c);
    t->p[0] = a; t->p[1] = b; t->p[2] = c; t->p[3] = d;
    return (id)t;
}
static id tf_named(id cls, SEL c, id name) {
    (void)cls; (void)c;
    const char *n = name ? cx_cstr(name) : "default";
    if (!strcmp(n, "linear")) return objc_autorelease(tf_make(0, 0, 1, 1));
    if (!strcmp(n, "easeIn")) return objc_autorelease(tf_make(0.42, 0, 1, 1));
    if (!strcmp(n, "easeOut")) return objc_autorelease(tf_make(0, 0, 0.58, 1));
    if (!strcmp(n, "easeInEaseOut")) return objc_autorelease(tf_make(0.42, 0, 0.58, 1));
    return objc_autorelease(tf_make(0.25, 0.1, 0.25, 1));
}
static id tf_points(id cls, SEL c, float a, float b, float d, float e) { (void)cls; (void)c; return objc_autorelease(tf_make(a, b, d, e)); }
static id tf_init_points(id self, SEL c, float a, float b, float d, float e) { (void)c; tf_t *t = (tf_t *)self; t->p[0] = a; t->p[1] = b; t->p[2] = d; t->p[3] = e; return self; }
static void tf_get(id self, SEL c, u64 i, float *v) {
    (void)c;
    tf_t *t = (tf_t *)self;
    if (!v) return;
    if (i == 0) { v[0] = 0; v[1] = 0; }
    else if (i == 1) { v[0] = (float)t->p[0]; v[1] = (float)t->p[1]; }
    else if (i == 2) { v[0] = (float)t->p[2]; v[1] = (float)t->p[3]; }
    else { v[0] = 1; v[1] = 1; }
}
static id tf_copy(id s, SEL c) { (void)c; return objc_retain(s); }
typedef struct { cx_class_t *isa; id name; } vf_t;
static id vf_named(id cls, SEL c, id name) { (void)cls; (void)c; vf_t *v = (vf_t *)cx_alloc_instance(&CAValueFunction_c); v->name = name ? objc_retain(name) : 0; return objc_autorelease((id)v); }
static id vf_name(id s, SEL c) { (void)c; return ((vf_t *)s)->name; }

/* ================================================================ valores dos layers por key path */
enum { VK_NONE = 0, VK_NUM, VK_PT, VK_RECT, VK_T3, VK_COLOR, VK_OBJ };
typedef struct { int kind, n; double v[16]; id obj; } val_t;
static SEL S(const char *s) { return sel_registerName(s); }
static int responds(id o, const char *s) { return o && cx_responds(o->isa, S(s)); }
static const char *float_keys[] = {"opacity", "shadowOpacity", "instanceRedOffset", "instanceGreenOffset", "instanceBlueOffset",
                                   "instanceAlphaOffset", "birthRate", "lifetime", "speed", 0};
static const char *pt_keys[] = {"position", "anchorPoint", "startPoint", "endPoint", "shadowOffset", "emitterPosition", "emitterSize", 0};
static const char *rect_keys[] = {"bounds", "frame", "contentsRect", "contentsCenter", 0};
static const char *t3_keys[] = {"transform", "sublayerTransform", "instanceTransform", 0};
static const char *color_keys[] = {"backgroundColor", "borderColor", "shadowColor", "fillColor", "strokeColor", "foregroundColor", "instanceColor", 0};
static int in_list(const char **l, const char *k) { for (int i = 0; l[i]; i++) if (!strcmp(l[i], k)) return 1; return 0; }
static char *setter_of(const char *k, char *buf, u64 n) {
    snprintf(buf, n, "set%s:", k);
    buf[3] = (char)toupper(buf[3]);
    return buf;
}
static void get_rect(id l, const char *k, CGR *r) { ((void (*)(CGR *, id, SEL))objc_msgSend_stret)(r, l, S(k)); }
static void get_t3(id l, const char *k, T3 *t) { ((void (*)(T3 *, id, SEL))objc_msgSend_stret)(t, l, S(k)); }
static int obj_to_val(id o, int kind, val_t *v);
/* lê o valor atual (modelo) de key path */
static int read_kp(id l, const char *kp, val_t *v) {
    memset(v, 0, sizeof *v);
    char base[64];
    const char *dot = strchr(kp, '.');
    u64 bl = dot ? (u64)(dot - kp) : strlen(kp);
    if (bl >= sizeof base) return 0;
    memcpy(base, kp, bl);
    base[bl] = 0;
    if (!responds(l, base)) return 0;
    if (in_list(t3_keys, base)) {
        T3 t;
        get_t3(l, base, &t);
        if (!dot) { v->kind = VK_T3; v->n = 16; memcpy(v->v, t.m, sizeof t.m); return 1; }
        const char *sub = dot + 1;
        v->kind = VK_NUM;
        if (!strcmp(sub, "scale") || !strcmp(sub, "scale.x")) { v->n = 1; v->v[0] = sqrt(t.m[0] * t.m[0] + t.m[1] * t.m[1]); }
        else if (!strcmp(sub, "scale.y")) { v->n = 1; v->v[0] = sqrt(t.m[4] * t.m[4] + t.m[5] * t.m[5]); }
        else if (!strcmp(sub, "scale.z")) { v->n = 1; v->v[0] = t.m[10]; }
        else if (!strcmp(sub, "rotation") || !strcmp(sub, "rotation.z")) { v->n = 1; v->v[0] = atan2(t.m[1], t.m[0]); }
        else if (!strcmp(sub, "rotation.x") || !strcmp(sub, "rotation.y")) { v->n = 1; v->v[0] = 0; }
        else if (!strcmp(sub, "translation")) { v->kind = VK_PT; v->n = 2; v->v[0] = t.m[12]; v->v[1] = t.m[13]; }
        else if (!strcmp(sub, "translation.x")) { v->n = 1; v->v[0] = t.m[12]; }
        else if (!strcmp(sub, "translation.y")) { v->n = 1; v->v[0] = t.m[13]; }
        else if (!strcmp(sub, "translation.z")) { v->n = 1; v->v[0] = t.m[14]; }
        else return 0;
        return 1;
    }
    if (in_list(rect_keys, base)) {
        CGR r;
        get_rect(l, base, &r);
        double a[4] = {r.o.x, r.o.y, r.s.w, r.s.h};
        if (!dot) { v->kind = VK_RECT; v->n = 4; memcpy(v->v, a, sizeof a); return 1; }
        const char *sub = dot + 1;
        v->kind = VK_NUM;
        if (!strcmp(sub, "origin")) { v->kind = VK_PT; v->n = 2; v->v[0] = a[0]; v->v[1] = a[1]; }
        else if (!strcmp(sub, "size")) { v->kind = VK_PT; v->n = 2; v->v[0] = a[2]; v->v[1] = a[3]; }
        else if (!strcmp(sub, "origin.x")) { v->n = 1; v->v[0] = a[0]; }
        else if (!strcmp(sub, "origin.y")) { v->n = 1; v->v[0] = a[1]; }
        else if (!strcmp(sub, "size.width")) { v->n = 1; v->v[0] = a[2]; }
        else if (!strcmp(sub, "size.height")) { v->n = 1; v->v[0] = a[3]; }
        else return 0;
        return 1;
    }
    if (in_list(pt_keys, base)) {
        CGP p = ((CGP(*)(id, SEL))objc_msgSend)(l, S(base));
        if (!dot) { v->kind = VK_PT; v->n = 2; v->v[0] = p.x; v->v[1] = p.y; return 1; }
        v->kind = VK_NUM;
        v->n = 1;
        const char *sub = dot + 1;
        v->v[0] = !strcmp(sub, "x") || !strcmp(sub, "width") ? p.x : p.y;
        return 1;
    }
    if (dot) return 0;
    if (in_list(color_keys, base)) {
        id c = MSG0(id, l, base);
        v->kind = VK_COLOR;
        v->n = 4;
        v->obj = c;
        if (c) { const double *k = (const double *)((u8 *)c + 8); for (int i = 0; i < 4; i++) v->v[i] = k[i]; }
        return 1;
    }
    if (in_list(float_keys, base)) { v->kind = VK_NUM; v->n = 1; v->v[0] = ((float (*)(id, SEL))objc_msgSend)(l, S(base)); return 1; }
    /* números CGFloat (cornerRadius, borderWidth, zPosition, lineWidth, strokeEnd...) */
    static const char *dbl_keys[] = {"cornerRadius", "borderWidth", "zPosition", "shadowRadius", "lineWidth", "strokeStart",
                                     "strokeEnd", "miterLimit", "lineDashPhase", "fontSize", "contentsScale", "instanceDelay", 0};
    if (in_list(dbl_keys, base)) { v->kind = VK_NUM; v->n = 1; v->v[0] = ((double (*)(id, SEL))objc_msgSend)(l, S(base)); return 1; }
    if (!strcmp(base, "hidden")) { v->kind = VK_NUM; v->n = 1; v->v[0] = MSG0(BOOL, l, "isHidden"); return 1; }
    v->kind = VK_OBJ;
    v->obj = MSG0(id, l, base);
    return 1;
}
static void write_kp(id l, const char *kp, const val_t *v) {
    char base[64], sb[80];
    const char *dot = strchr(kp, '.');
    u64 bl = dot ? (u64)(dot - kp) : strlen(kp);
    if (bl >= sizeof base) return;
    memcpy(base, kp, bl);
    base[bl] = 0;
    setter_of(base, sb, sizeof sb);
    if (!responds(l, sb)) return;
    SEL set = S(sb);
    if (in_list(t3_keys, base)) {
        T3 t;
        if (!dot) { if (v->n < 16) return; memcpy(t.m, v->v, sizeof t.m); }
        else {
            get_t3(l, base, &t);
            const char *sub = dot + 1;
            double x = v->v[0];
            if (!strcmp(sub, "scale")) { double a = atan2(t.m[1], t.m[0]); T3 r = CATransform3DMakeRotation(a, 0, 0, 1); for (int i = 0; i < 2; i++) { r.m[i] *= x; r.m[4 + i] *= x; } r.m[10] = x; r.m[12] = t.m[12]; r.m[13] = t.m[13]; r.m[14] = t.m[14]; t = r; }
            else if (!strcmp(sub, "scale.x")) { double s = sqrt(t.m[0] * t.m[0] + t.m[1] * t.m[1]); if (s) { t.m[0] *= x / s; t.m[1] *= x / s; } else t.m[0] = x; }
            else if (!strcmp(sub, "scale.y")) { double s = sqrt(t.m[4] * t.m[4] + t.m[5] * t.m[5]); if (s) { t.m[4] *= x / s; t.m[5] *= x / s; } else t.m[5] = x; }
            else if (!strcmp(sub, "scale.z")) t.m[10] = x;
            else if (!strcmp(sub, "rotation") || !strcmp(sub, "rotation.z")) {
                double sx = sqrt(t.m[0] * t.m[0] + t.m[1] * t.m[1]), sy = sqrt(t.m[4] * t.m[4] + t.m[5] * t.m[5]);
                t.m[0] = cos(x) * sx; t.m[1] = sin(x) * sx; t.m[4] = -sin(x) * sy; t.m[5] = cos(x) * sy;
            }
            else if (!strcmp(sub, "translation")) { t.m[12] = v->v[0]; t.m[13] = v->n > 1 ? v->v[1] : t.m[13]; }
            else if (!strcmp(sub, "translation.x")) t.m[12] = x;
            else if (!strcmp(sub, "translation.y")) t.m[13] = x;
            else if (!strcmp(sub, "translation.z")) t.m[14] = x;
            else return;
        }
        ((void (*)(id, SEL, T3))objc_msgSend)(l, set, t);
        return;
    }
    if (in_list(rect_keys, base)) {
        CGR r;
        if (!dot) { if (v->n < 4) return; r.o.x = v->v[0]; r.o.y = v->v[1]; r.s.w = v->v[2]; r.s.h = v->v[3]; }
        else {
            get_rect(l, base, &r);
            const char *sub = dot + 1;
            if (!strcmp(sub, "origin")) { r.o.x = v->v[0]; r.o.y = v->v[1]; }
            else if (!strcmp(sub, "size")) { r.s.w = v->v[0]; r.s.h = v->v[1]; }
            else if (!strcmp(sub, "origin.x")) r.o.x = v->v[0];
            else if (!strcmp(sub, "origin.y")) r.o.y = v->v[0];
            else if (!strcmp(sub, "size.width")) r.s.w = v->v[0];
            else if (!strcmp(sub, "size.height")) r.s.h = v->v[0];
            else return;
        }
        ((void (*)(id, SEL, CGR))objc_msgSend)(l, set, r);
        return;
    }
    if (in_list(pt_keys, base)) {
        CGP p;
        if (!dot) { p.x = v->v[0]; p.y = v->n > 1 ? v->v[1] : v->v[0]; }
        else {
            p = ((CGP(*)(id, SEL))objc_msgSend)(l, S(base));
            const char *sub = dot + 1;
            if (!strcmp(sub, "x") || !strcmp(sub, "width")) p.x = v->v[0]; else p.y = v->v[0];
        }
        ((void (*)(id, SEL, CGP))objc_msgSend)(l, set, p);
        return;
    }
    if (dot) return;
    if (v->kind == VK_OBJ) { MSG(void, l, sb, v->obj); return; }
    if (in_list(color_keys, base)) {
        id c = v->obj && v->kind == VK_OBJ ? v->obj : cx_color(v->v[0], v->v[1], v->v[2], v->v[3]);
        MSG(void, l, sb, c);
        return;
    }
    if (in_list(float_keys, base)) { ((void (*)(id, SEL, float))objc_msgSend)(l, set, (float)v->v[0]); return; }
    if (!strcmp(base, "hidden")) { ((void (*)(id, SEL, BOOL))objc_msgSend)(l, set, (BOOL)(v->v[0] >= 0.5)); return; }
    ((void (*)(id, SEL, double))objc_msgSend)(l, set, v->v[0]);
}
/* NSNumber / NSValue / CGColor -> valor */
static int obj_to_val(id o, int kind, val_t *v) {
    memset(v, 0, sizeof *v);
    v->kind = kind;
    if (!o) return 0;
    if (o->isa == &NSConcreteStructValue_c) {
        sval_t *s = (sval_t *)o;
        v->n = (int)(s->size / 8);
        if (v->n > 16) v->n = 16;
        memcpy(v->v, s->data, (u64)v->n * 8);
        if (kind == VK_NUM && v->n > 1) v->kind = v->n == 2 ? VK_PT : v->n == 4 ? VK_RECT : VK_T3;
        return v->n;
    }
    if (kind == VK_COLOR) {
        const double *k = (const double *)((u8 *)o + 8);
        v->n = 4;
        for (int i = 0; i < 4; i++) v->v[i] = k[i];
        return 4;
    }
    if (responds(o, "doubleValue")) { v->n = 1; v->v[0] = MSG0(double, o, "doubleValue"); if (kind != VK_NUM) { for (int i = 1; i < 16; i++) v->v[i] = v->v[0]; v->n = kind == VK_PT ? 2 : kind == VK_RECT ? 4 : kind == VK_T3 ? 16 : 1; } return v->n; }
    v->kind = VK_OBJ;
    v->obj = o;
    return 0;
}

/* ================================================================ animador */
typedef struct run {
    struct run *next;
    id layer, key, anim, group;   /* group: animação-mãe (o delegate dela é avisado no fim) */
    double t0;
    int started, child, done;
    val_t model;
    char kp[96];
} run_t;
static run_t *g_runs;
static id g_timer, g_animator;
static id *g_dirty;
static int g_ndirty, g_capdirty;
static void engine_start(void);

static int is_kind(id o, cx_class_t *c) { return o && cx_is_subclass(o->isa, c); }
static double A_d(id a, const char *k) { return mtl_dgetd(a, k); }
static float A_f(id a, const char *k) { return mtl_dgetf(a, k); }
static id A_o(id a, const char *k) { return (id)mtl_dget(a, k); }
static int A_b(id a, const char *k) { return (int)mtl_dget(a, k); }
static double anim_dur(id a) {
    double d = A_d(a, "duration");
    if (is_kind(a, &CASpringAnimation_c) && d <= 0) {
        double m = A_d(a, "mass"), k = A_d(a, "stiffness"), c = A_d(a, "damping");
        double w0 = sqrt(k / (m > 0 ? m : 1)), z = c / (2 * sqrt(k * (m > 0 ? m : 1)));
        d = z > 0 ? 6.9 / (z * w0) : 1;   /* até ~0,1 % */
    }
    return d > 0 ? d : 0.25;
}
/* tempo local -> fração [0,1] já com repetição, autoreverse e curva; *fin = terminou */
static double anim_frac(id a, double t, int *fin) {
    double d = anim_dur(a);
    float rc = A_f(a, "repeatCount");
    double rd = A_d(a, "repeatDuration");
    int ar = A_b(a, "autoreverses");
    double cyc = d * (ar ? 2 : 1);
    double total = rd > 0 ? rd : cyc * (rc > 0 ? rc : 1);
    int inf = rc >= 3.0e38f || rd >= 3.0e38;
    *fin = 0;
    if (t < 0) t = 0;
    if (!inf && t >= total) { *fin = 1; t = total; }
    double ct = fmod(t, cyc);
    if (*fin && ct == 0 && t > 0) ct = cyc;
    double f = ct / d;
    if (ar && f > 1) f = 2 - f;
    if (f > 1) f = 1;
    return f;
}
static void lerp_val(const val_t *a, const val_t *b, double f, val_t *o) {
    *o = *a;
    int n = a->n > b->n ? a->n : b->n;
    o->n = n;
    for (int i = 0; i < n; i++) o->v[i] = a->v[i] + (b->v[i] - a->v[i]) * f;
    if (a->kind == VK_OBJ || b->kind == VK_OBJ) { o->kind = VK_OBJ; o->obj = f < 1 ? a->obj : b->obj; }
}
static double spring_x(id a, double t) {
    double m = A_d(a, "mass"), k = A_d(a, "stiffness"), c = A_d(a, "damping"), v0 = A_d(a, "initialVelocity");
    if (m <= 0) m = 1;
    double w0 = sqrt(k / m), z = c / (2 * sqrt(k * m));
    if (z < 1) {
        double wd = w0 * sqrt(1 - z * z), A = 1, B = (z * w0 - v0) / wd;
        return 1 - exp(-z * w0 * t) * (A * cos(wd * t) + B * sin(wd * t));
    }
    double A = 1, B = w0 - v0;
    return 1 - exp(-w0 * t) * (A + B * t);
}
/* ponto em um CGPath por comprimento de arco (fração f) */
typedef struct { double *xy; int n, cap; } pl_t;
static void pl_add(pl_t *p, double x, double y) {
    if (p->n == p->cap) { p->cap = p->cap ? p->cap * 2 : 64; p->xy = realloc(p->xy, (u64)p->cap * 16); }
    p->xy[p->n * 2] = x; p->xy[p->n * 2 + 1] = y; p->n++;
}
static void flat_apply(void *info, const cgelem_t *e) {
    pl_t *p = info;
    double lx = p->n ? p->xy[p->n * 2 - 2] : 0, ly = p->n ? p->xy[p->n * 2 - 1] : 0;
    switch (e->type) {
    case 0: case 1: pl_add(p, e->pts[0].x, e->pts[0].y); break;
    case 2: for (int i = 1; i <= 12; i++) { double t = i / 12.0, u = 1 - t; pl_add(p, u * u * lx + 2 * u * t * e->pts[0].x + t * t * e->pts[1].x, u * u * ly + 2 * u * t * e->pts[0].y + t * t * e->pts[1].y); } break;
    case 3: for (int i = 1; i <= 16; i++) { double t = i / 16.0, u = 1 - t; pl_add(p, u * u * u * lx + 3 * u * u * t * e->pts[0].x + 3 * u * t * t * e->pts[1].x + t * t * t * e->pts[2].x, u * u * u * ly + 3 * u * u * t * e->pts[0].y + 3 * u * t * t * e->pts[1].y + t * t * t * e->pts[2].y); } break;
    }
}
static void path_point(id path, double f, double *x, double *y) {
    pl_t p = {0};
    CGPathApply(path, &p, flat_apply);
    *x = *y = 0;
    if (!p.n) return;
    double tot = 0;
    for (int i = 1; i < p.n; i++) tot += sqrt((p.xy[i * 2] - p.xy[i * 2 - 2]) * (p.xy[i * 2] - p.xy[i * 2 - 2]) + (p.xy[i * 2 + 1] - p.xy[i * 2 - 1]) * (p.xy[i * 2 + 1] - p.xy[i * 2 - 1]));
    double want = tot * f, acc = 0;
    *x = p.xy[0]; *y = p.xy[1];
    for (int i = 1; i < p.n; i++) {
        double dx = p.xy[i * 2] - p.xy[i * 2 - 2], dy = p.xy[i * 2 + 1] - p.xy[i * 2 - 1], l = sqrt(dx * dx + dy * dy);
        if (acc + l >= want && l > 0) { double t = (want - acc) / l; *x = p.xy[i * 2 - 2] + dx * t; *y = p.xy[i * 2 - 1] + dy * t; break; }
        acc += l;
        *x = p.xy[i * 2]; *y = p.xy[i * 2 + 1];
    }
    free(p.xy);
}
/* valor da animação na fração f */
static void anim_value(run_t *r, double f, double tl, val_t *out) {
    id a = r->anim;
    const val_t *m = &r->model;
    int kind = m->kind ? m->kind : VK_NUM;
    if (is_kind(a, &CAKeyframeAnimation_c)) {
        id path = A_o(a, "path");
        double ef = tf_eval(A_o(a, "timingFunction"), f);
        if (path) { *out = *m; out->kind = VK_PT; out->n = 2; path_point(path, ef, &out->v[0], &out->v[1]); return; }
        id vals = A_o(a, "values");
        u64 n = vals ? MSG0(u64, vals, "count") : 0;
        if (!n) { *out = *m; return; }
        id kt = A_o(a, "keyTimes");
        const char *mode = A_o(a, "calculationMode") ? cx_cstr(A_o(a, "calculationMode")) : "linear";
        int disc = !strcmp(mode, "discrete");
        u64 seg = 0;
        double lf = 0;
        if (n == 1) seg = 0;
        else {
            double prev = 0;
            for (u64 i = 1; i < n; i++) {
                double ti = kt && MSG0(u64, kt, "count") > i ? MSG0(double, MSG(id, kt, "objectAtIndex:", i), "doubleValue") : (double)i / (double)(n - 1);
                double tp = kt && MSG0(u64, kt, "count") > i - 1 ? MSG0(double, MSG(id, kt, "objectAtIndex:", i - 1), "doubleValue") : prev;
                prev = ti;
                seg = i - 1;
                if (ef <= ti || i == n - 1) { lf = ti > tp ? (ef - tp) / (ti - tp) : 1; break; }
            }
        }
        if (lf < 0) lf = 0;
        if (lf > 1) lf = 1;
        id tfs = A_o(a, "timingFunctions");
        if (tfs && MSG0(u64, tfs, "count") > seg) lf = tf_eval(MSG(id, tfs, "objectAtIndex:", seg), lf);
        val_t va, vb;
        obj_to_val(MSG(id, vals, "objectAtIndex:", seg), kind, &va);
        obj_to_val(MSG(id, vals, "objectAtIndex:", seg + 1 < n ? seg + 1 : seg), kind, &vb);
        if (disc) { *out = lf >= 1 && seg + 1 < n ? vb : va; return; }
        lerp_val(&va, &vb, lf, out);
        return;
    }
    val_t from, to, by;
    id fo = A_o(a, "fromValue"), to_o = A_o(a, "toValue"), bo = A_o(a, "byValue");
    if (!obj_to_val(fo, kind, &from) && from.kind != VK_OBJ) from = *m;
    if (!obj_to_val(to_o, kind, &to) && to.kind != VK_OBJ) {
        if (obj_to_val(bo, kind, &by)) { to = from; for (int i = 0; i < by.n; i++) to.v[i] += by.v[i]; }
        else to = *m;
    }
    if (fo == 0 && to_o != 0 && bo != 0 && obj_to_val(bo, kind, &by)) { from = to; for (int i = 0; i < by.n; i++) from.v[i] -= by.v[i]; }
    double ef = is_kind(a, &CASpringAnimation_c) ? spring_x(a, tl) : tf_eval(A_o(a, "timingFunction"), f);
    lerp_val(&from, &to, ef, out);
    if (A_b(a, "additive") && out->kind != VK_OBJ) for (int i = 0; i < out->n && i < m->n; i++) out->v[i] += m->v[i];
}
static void call_delegate(id a, const char *sel, int fin) {
    id d = A_o(a, "delegate");
    if (!d || !responds(d, sel)) return;
    if (fin < 0) MSG(void, d, sel, a);
    else MSG(void, d, sel, a, (BOOL)fin);
}
static void run_free(run_t *r) {
    if (r->layer) objc_release(r->layer);
    if (r->key) objc_release(r->key);
    if (r->anim) objc_release(r->anim);
    if (r->group) objc_release(r->group);
    free(r);
}
static void add_run(id layer, id key, id anim, id group, double t0, int child) {
    run_t *r = calloc(1, sizeof *r);
    r->layer = objc_retain(layer);
    r->key = key ? objc_retain(key) : 0;
    r->anim = objc_retain(anim);
    r->group = group ? objc_retain(group) : 0;
    r->t0 = t0;
    r->child = child;
    id kp = is_kind(anim, &CAPropertyAnimation_c) ? A_o(anim, "keyPath") : 0;
    if (kp) { strlcpy(r->kp, cx_cstr(kp), sizeof r->kp); read_kp(layer, r->kp, &r->model); if (r->model.kind == VK_OBJ && r->model.obj) objc_retain(r->model.obj); }
    run_t **pp = &g_runs;
    while (*pp) pp = &(*pp)->next;
    *pp = r;
}
static void remove_runs(id layer, id key, int all) {
    for (run_t **pp = &g_runs; *pp;) {
        run_t *r = *pp;
        int match = r->layer == layer && (all || (key && r->key && MSG(BOOL, r->key, "isEqualToString:", key)));
        if (match) {
            if (r->kp[0] && r->model.kind) write_kp(r->layer, r->kp, &r->model);
            *pp = r->next;
            if (!r->child && r->started) call_delegate(r->group ? r->group : r->anim, "animationDidStop:finished:", 0);
            if (r->model.kind == VK_OBJ && r->model.obj) objc_release(r->model.obj);
            run_free(r);
            continue;
        }
        pp = &r->next;
    }
}
static void ca_add_anim(id self, SEL c, id anim, id key) {
    (void)c;
    if (!anim) return;
    if (key) remove_runs(self, key, 0);
    id a = MSG0(id, anim, "copy");
    double now = CACurrentMediaTime(), bt = A_d(a, "beginTime");
    double t0 = bt > 0 ? bt : now;
    if (is_kind(a, &CAAnimationGroup_c)) {
        id kids = A_o(a, "animations");
        u64 n = kids ? MSG0(u64, kids, "count") : 0;
        add_run(self, key, a, 0, t0, 0);
        for (u64 i = 0; i < n; i++) {
            id k = MSG(id, kids, "objectAtIndex:", i);
            add_run(self, key, k, a, t0 + A_d(k, "beginTime"), 1);
        }
    } else add_run(self, key, a, 0, t0, 0);
    objc_release(a);
    engine_start();
}
static void ca_remove_anim(id self, SEL c, id key) { (void)c; remove_runs(self, key, 0); }
static void ca_remove_all(id self, SEL c) { (void)c; remove_runs(self, 0, 1); }
static id ca_anim_for_key(id self, SEL c, id key) {
    (void)c;
    for (run_t *r = g_runs; r; r = r->next)
        if (r->layer == self && !r->child && key && r->key && MSG(BOOL, r->key, "isEqualToString:", key)) return r->anim;
    return 0;
}
static id ca_anim_keys(id self, SEL c) {
    (void)c;
    id a = MSG0(id, (id)cx_class_named("NSMutableArray"), "array");
    for (run_t *r = g_runs; r; r = r->next) if (r->layer == self && !r->child && r->key) MSG(void, a, "addObject:", r->key);
    return MSG0(u64, a, "count") ? a : 0;
}

static void render_content(id l);
static void tick(id self, SEL c, id timer) {
    (void)self; (void)c; (void)timer;
    double now = CACurrentMediaTime();
    for (run_t **pp = &g_runs; *pp;) {
        run_t *r = *pp;
        double sp = A_f(r->anim, "speed");
        if (sp == 0) sp = 1;
        double tl = (now - r->t0) * sp + A_d(r->anim, "timeOffset");
        if (tl < 0) { pp = &r->next; continue; }
        if (!r->started) { r->started = 1; if (!r->child) call_delegate(r->anim, "animationDidStart:", -1); }
        int fin;
        double f = anim_frac(r->anim, tl, &fin);
        if (r->group) {   /* filho: corta na duração do grupo */
            double gd = anim_dur(r->group), gt = (now - r->t0 + A_d(r->anim, "beginTime")) * sp;
            if (gt >= gd) fin = 1;
        }
        if (r->kp[0]) {
            val_t v;
            anim_value(r, f, tl, &v);
            if (fin) {
                id fm = A_o(r->group ? r->group : r->anim, "fillMode");
                int keep = !A_b(r->group ? r->group : r->anim, "removedOnCompletion") && fm && (!strcmp(cx_cstr(fm), "forwards") || !strcmp(cx_cstr(fm), "both"));
                write_kp(r->layer, r->kp, keep ? &v : &r->model);
            } else write_kp(r->layer, r->kp, &v);
        }
        if (fin) {
            *pp = r->next;
            if (!r->child) call_delegate(r->anim, "animationDidStop:finished:", 1);
            if (r->model.kind == VK_OBJ && r->model.obj) objc_release(r->model.obj);
            run_free(r);
            continue;
        }
        pp = &r->next;
    }
    /* camadas de conteúdo com propriedades mudadas */
    while (g_ndirty) {
        id l = g_dirty[--g_ndirty];
        render_content(l);
        objc_release(l);
    }
    if (!g_runs && !g_ndirty && g_timer) { MSG0(void, g_timer, "invalidate"); objc_release(g_timer); g_timer = 0; }
}
static void engine_start(void) {
    if (g_timer) return;
    if (!g_animator) g_animator = cx_alloc_instance(&CAMosAnimator_c);
    id t = MSG(id, (id)cx_class_named("NSTimer"), "scheduledTimerWithTimeInterval:target:selector:userInfo:repeats:", 1.0 / 60, g_animator,
               S("tick:"), (id)0, (BOOL)YES);
    g_timer = t ? objc_retain(t) : 0;
}
static void mark_dirty(id l) {
    for (int i = 0; i < g_ndirty; i++) if (g_dirty[i] == l) return;
    if (g_ndirty == g_capdirty) { g_capdirty = g_capdirty ? g_capdirty * 2 : 16; g_dirty = realloc(g_dirty, (u64)g_capdirty * sizeof(id)); }
    g_dirty[g_ndirty++] = objc_retain(l);
    engine_start();
}
static void content_changed(id self, const char *prop) { if (prop[0] != '_') mark_dirty(self); }

/* ================================================================ rasterização (cobertura 4x vertical, exata em x) */
typedef struct { float *cov; int w, h; } canvas_t;
typedef struct { double x0, y0, x1, y1; int dir; } edge_t;
typedef struct { edge_t *e; int n, cap; } edges_t;
static void edge_add(edges_t *E, double x0, double y0, double x1, double y1) {
    if (y0 == y1) return;
    if (E->n == E->cap) { E->cap = E->cap ? E->cap * 2 : 256; E->e = realloc(E->e, (u64)E->cap * sizeof(edge_t)); }
    edge_t *e = &E->e[E->n++];
    if (y0 < y1) { e->x0 = x0; e->y0 = y0; e->x1 = x1; e->y1 = y1; e->dir = 1; }
    else { e->x0 = x1; e->y0 = y1; e->x1 = x0; e->y1 = y0; e->dir = -1; }
}
static void poly_edges(edges_t *E, const double *xy, int n) { for (int i = 0; i < n; i++) { int j = (i + 1) % n; edge_add(E, xy[i * 2], xy[i * 2 + 1], xy[j * 2], xy[j * 2 + 1]); } }
static int xcmp(const void *a, const void *b) { double x = ((const double *)a)[0], y = ((const double *)b)[0]; return x < y ? -1 : x > y; }
static void fill_edges(canvas_t *C, edges_t *E, int evenodd) {
    if (!E->n) return;
    double *xs = malloc((u64)E->n * 2 * sizeof(double));
    for (int py = 0; py < C->h; py++)
        for (int sub = 0; sub < 4; sub++) {
            double y = py + (sub + 0.5) / 4;
            int nx = 0;
            for (int i = 0; i < E->n; i++) {
                edge_t *e = &E->e[i];
                if (y < e->y0 || y >= e->y1) continue;
                xs[nx * 2] = e->x0 + (e->x1 - e->x0) * (y - e->y0) / (e->y1 - e->y0);
                xs[nx * 2 + 1] = e->dir;
                nx++;
            }
            if (nx < 2) continue;
            qsort(xs, nx, 16, xcmp);
            int wind = 0;
            float *row = C->cov + (u64)py * C->w;
            for (int i = 0; i + 1 < nx; i++) {
                wind += (int)xs[i * 2 + 1];
                int in = evenodd ? (wind & 1) : wind != 0;
                if (!in) continue;
                double a = xs[i * 2], b = xs[i * 2 + 2];
                if (a < 0) a = 0;
                if (b > C->w) b = C->w;
                if (b <= a) continue;
                int ia = (int)a, ib = (int)b;
                if (ia == ib) { row[ia] += (float)((b - a) * 0.25); continue; }
                row[ia] += (float)((ia + 1 - a) * 0.25);
                for (int x = ia + 1; x < ib && x < C->w; x++) row[x] += 0.25f;
                if (ib < C->w) row[ib] += (float)((b - ib) * 0.25);
            }
        }
    free(xs);
}
/* subcaminhos achatados em pixels */
typedef struct { double *xy; int n, cap, closed; } sp_t;
typedef struct { sp_t *s; int n, cap; double sx, sy, ox, oy; int flip; double h; } paths_t;
static sp_t *cur_sp(paths_t *P) { return P->n ? &P->s[P->n - 1] : 0; }
static void sp_new(paths_t *P) { if (P->n == P->cap) { P->cap = P->cap ? P->cap * 2 : 8; P->s = realloc(P->s, (u64)P->cap * sizeof(sp_t)); } memset(&P->s[P->n++], 0, sizeof(sp_t)); }
static void sp_pt(paths_t *P, double x, double y) {
    sp_t *s = cur_sp(P);
    if (!s) { sp_new(P); s = cur_sp(P); }
    if (s->n == s->cap) { s->cap = s->cap ? s->cap * 2 : 32; s->xy = realloc(s->xy, (u64)s->cap * 16); }
    s->xy[s->n * 2] = x; s->xy[s->n * 2 + 1] = y; s->n++;
}
static void tx(paths_t *P, double x, double y, double *ox, double *oy) {
    *ox = (x - P->ox) * P->sx;
    *oy = P->flip ? (y - P->oy) * P->sy : P->h - (y - P->oy) * P->sy;
}
static void path_apply(void *info, const cgelem_t *e) {
    paths_t *P = info;
    sp_t *s = cur_sp(P);
    double lx = s && s->n ? s->xy[s->n * 2 - 2] : 0, ly = s && s->n ? s->xy[s->n * 2 - 1] : 0, x, y;
    switch (e->type) {
    case 0: sp_new(P); tx(P, e->pts[0].x, e->pts[0].y, &x, &y); sp_pt(P, x, y); break;
    case 1: tx(P, e->pts[0].x, e->pts[0].y, &x, &y); sp_pt(P, x, y); break;
    case 2: {
        double cx, cy, ex, ey;
        tx(P, e->pts[0].x, e->pts[0].y, &cx, &cy);
        tx(P, e->pts[1].x, e->pts[1].y, &ex, &ey);
        for (int i = 1; i <= 16; i++) { double t = i / 16.0, u = 1 - t; sp_pt(P, u * u * lx + 2 * u * t * cx + t * t * ex, u * u * ly + 2 * u * t * cy + t * t * ey); }
        break;
    }
    case 3: {
        double ax, ay, bx, by, ex, ey;
        tx(P, e->pts[0].x, e->pts[0].y, &ax, &ay);
        tx(P, e->pts[1].x, e->pts[1].y, &bx, &by);
        tx(P, e->pts[2].x, e->pts[2].y, &ex, &ey);
        double len = fabs(ax - lx) + fabs(ay - ly) + fabs(bx - ax) + fabs(by - ay) + fabs(ex - bx) + fabs(ey - by);
        int n = len < 8 ? 4 : len > 400 ? 64 : (int)(len / 6);
        for (int i = 1; i <= n; i++) { double t = (double)i / n, u = 1 - t; sp_pt(P, u * u * u * lx + 3 * u * u * t * ax + 3 * u * t * t * bx + t * t * t * ex, u * u * u * ly + 3 * u * u * t * ay + 3 * u * t * t * by + t * t * t * ey); }
        break;
    }
    case 4: if (s) { s->closed = 1; if (s->n) { double fx = s->xy[0], fy = s->xy[1]; sp_new(P); sp_pt(P, fx, fy); P->s[P->n - 1].n = 0; sp_pt(P, fx, fy); P->s[P->n - 1].n = 1; } } break;
    }
}
static void poly_ccw(double *xy, int n) {      /* orienta o polígono (área positiva) para a união nonzero */
    double a = 0;
    for (int i = 0; i < n; i++) { int j = (i + 1) % n; a += xy[i * 2] * xy[j * 2 + 1] - xy[j * 2] * xy[i * 2 + 1]; }
    if (a >= 0) return;
    for (int i = 0; i < n / 2; i++) for (int k = 0; k < 2; k++) { double t = xy[i * 2 + k]; xy[i * 2 + k] = xy[(n - 1 - i) * 2 + k]; xy[(n - 1 - i) * 2 + k] = t; }
}
static void disc(edges_t *E, double cx, double cy, double r) {
    double xy[48];
    int n = r < 2 ? 8 : 24;
    for (int i = 0; i < n; i++) { double a = 6.283185307179586 * i / n; xy[i * 2] = cx + r * cos(a); xy[i * 2 + 1] = cy + r * sin(a); }
    poly_ccw(xy, n);
    poly_edges(E, xy, n);
}
static void stroke_seg(edges_t *E, double x0, double y0, double x1, double y1, double hw, int capstart, int capend, int cap) {
    double dx = x1 - x0, dy = y1 - y0, l = sqrt(dx * dx + dy * dy);
    if (l < 1e-9) { if (cap == 1) disc(E, x0, y0, hw); return; }
    double nx = -dy / l * hw, ny = dx / l * hw, ex = dx / l * hw, ey = dy / l * hw;
    if (cap == 2) { if (capstart) { x0 -= ex; y0 -= ey; } if (capend) { x1 += ex; y1 += ey; } }
    double q[8] = {x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny};
    poly_ccw(q, 4);
    poly_edges(E, q, 4);
    if (cap == 1) { if (capstart) disc(E, x0, y0, hw); if (capend) disc(E, x1, y1, hw); }
}
static void stroke_poly(edges_t *E, const double *xy, int n, int closed, double hw, int cap, int join, double miter) {
    if (n < 2) { if (n == 1 && cap == 1) disc(E, xy[0], xy[1], hw); return; }
    int segs = closed ? n : n - 1;
    for (int i = 0; i < segs; i++) {
        int j = (i + 1) % n;
        stroke_seg(E, xy[i * 2], xy[i * 2 + 1], xy[j * 2], xy[j * 2 + 1], hw, !closed && i == 0, !closed && i == segs - 1, closed ? 0 : cap);
    }
    /* junções */
    for (int i = closed ? 0 : 1; i < (closed ? n : n - 1); i++) {
        int p = (i - 1 + n) % n, q = (i + 1) % n;
        double x = xy[i * 2], y = xy[i * 2 + 1];
        if (join == 1) { disc(E, x, y, hw); continue; }
        double ax = x - xy[p * 2], ay = y - xy[p * 2 + 1], bx = xy[q * 2] - x, by = xy[q * 2 + 1] - y;
        double la = sqrt(ax * ax + ay * ay), lb = sqrt(bx * bx + by * by);
        if (la < 1e-9 || lb < 1e-9) continue;
        double n1x = -ay / la * hw, n1y = ax / la * hw, n2x = -by / lb * hw, n2y = bx / lb * hw;
        double cr = ax * by - ay * bx;
        double s = cr > 0 ? -1 : 1;      /* lado de fora da curva */
        double t[8] = {x, y, x + s * n1x, y + s * n1y, x + s * n2x, y + s * n2y, x, y};
        int cnt = 3;
        if (join == 0) {   /* miter: ponta se dentro do limite */
            double dot = (ax * bx + ay * by) / (la * lb);
            double ml = 1 / sqrt((1 + dot) / 2 + 1e-12);
            if (ml <= miter) {
                double mx = (n1x + n2x) / 2, my = (n1y + n2y) / 2, mlen = sqrt(mx * mx + my * my);
                if (mlen > 1e-9) { double k = hw * ml / mlen; t[6] = x + s * mx * k; t[7] = y + s * my * k; double tt[8] = {x, y, t[2], t[3], t[6], t[7], t[4], t[5]}; memcpy(t, tt, sizeof tt); cnt = 4; }
            }
        }
        poly_ccw(t, cnt);
        poly_edges(E, t, cnt);
    }
}
static u32 color_argb(id c, float alpha_mul) {
    if (!c) return 0;
    const double *k = (const double *)((u8 *)c + 8);
    double a = k[3] * alpha_mul;
    if (a < 0) a = 0;
    if (a > 1) a = 1;
    u32 A = (u32)(a * 255 + 0.5);
    u32 R = (u32)((k[0] < 0 ? 0 : k[0] > 1 ? 1 : k[0]) * 255 + 0.5), G = (u32)((k[1] < 0 ? 0 : k[1] > 1 ? 1 : k[1]) * 255 + 0.5), B = (u32)((k[2] < 0 ? 0 : k[2] > 1 ? 1 : k[2]) * 255 + 0.5);
    return A << 24 | R << 16 | G << 8 | B;
}
/* compõe a cobertura numa cor sobre o BGRA pré-multiplicado */
static void composite(u8 *px, u64 bpr, canvas_t *C, u32 argb) {
    double ca = (argb >> 24) / 255.0, cr = ((argb >> 16) & 255) * ca, cg = ((argb >> 8) & 255) * ca, cb = (argb & 255) * ca;
    for (int y = 0; y < C->h; y++)
        for (int x = 0; x < C->w; x++) {
            float cv = C->cov[(u64)y * C->w + x];
            if (cv <= 0.001f) continue;
            if (cv > 1) cv = 1;
            double a = ca * cv;
            u8 *p = px + (u64)y * bpr + (u64)x * 4;
            double ia = 1 - a;
            p[0] = (u8)(cb * cv + p[0] * ia + 0.5);
            p[1] = (u8)(cg * cv + p[1] * ia + 0.5);
            p[2] = (u8)(cr * cv + p[2] * ia + 0.5);
            p[3] = (u8)(a * 255 + p[3] * ia + 0.5);
        }
}

/* ================================================================ camadas de conteúdo */
static int layer_flipped(id l) {
    int f = 0;
    for (id p = l; p; p = MSG0(id, p, "superlayer")) if (responds(p, "isGeometryFlipped") && MSG0(BOOL, p, "isGeometryFlipped")) f ^= 1;
    return f;
}
static IMP g_ca_set_contents;
static IOSurfaceRef surface_for(id l, int w, int h) {
    IOSurfaceRef s = (IOSurfaceRef)mtl_dget(l, "_mosSurface");
    if (s && (int)IOSurfaceGetWidth(s) == w && (int)IOSurfaceGetHeight(s) == h) return s;
    id keys[4] = {(id)kIOSurfaceWidth, (id)kIOSurfaceHeight, (id)kIOSurfaceBytesPerElement, (id)kIOSurfacePixelFormat};
    id vals[4] = {cx_num_i(w), cx_num_i(h), cx_num_i(4), cx_num_i(0x42475241)};
    id d = MSG(id, (id)cx_class_named("NSDictionary"), "dictionaryWithObjects:forKeys:count:", vals, keys, (u64)4);
    for (int i = 0; i < 4; i++) objc_release(vals[i]);
    s = IOSurfaceCreate(d);
    mtl_dseto(l, "_mosSurface", s);
    if (s) objc_release(s);
    return s;
}
static void present_surface(id l, IOSurfaceRef s) {
    IOSurfaceLock(s, 0, 0);
    IOSurfaceUnlock(s, 0, 0);
    if (g_ca_set_contents) ((void (*)(id, SEL, id))g_ca_set_contents)(l, S("setContents:"), s);
}
static int layer_px(id l, CGR *b, double *scale, int *w, int *h) {
    get_rect(l, "bounds", b);
    double sc = MSG0(double, l, "contentsScale");
    if (sc <= 0) sc = 1;
    *scale = sc;
    *w = (int)ceil(b->s.w * sc);
    *h = (int)ceil(b->s.h * sc);
    if (*w <= 0 || *h <= 0 || *w > 8192 || *h > 8192) return 0;
    return 1;
}
/* rótulo de NSString (const char*) */
static const char *sval(id s) { return s ? cx_cstr(s) : ""; }

static void dash_poly(const double *xy, int n, int closed, const double *pat, int np, double phase, double t0, double t1, edges_t *E, double hw, int cap, int join, double miter) {
    /* comprimento total para strokeStart/End */
    int segs = closed ? n : n - 1;
    double tot = 0;
    for (int i = 0; i < segs; i++) { int j = (i + 1) % n; tot += sqrt((xy[j * 2] - xy[i * 2]) * (xy[j * 2] - xy[i * 2]) + (xy[j * 2 + 1] - xy[i * 2 + 1]) * (xy[j * 2 + 1] - xy[i * 2 + 1])); }
    double from = tot * t0, to = tot * t1;
    double plen = 0;
    for (int i = 0; i < np; i++) plen += pat[i];
    if (np == 0 || plen <= 0) { pat = 0; np = 0; }
    /* percorre gerando pedaços visíveis */
    double *buf = malloc((u64)(n + 2) * 16 * 2 + 64);
    int bn = 0;
    double acc = 0;
    int di = 0;
    double dleft = 0, dpos = 0;
    int on = 1;
    if (np) {
        dpos = fmod(phase, plen);
        if (dpos < 0) dpos += plen;
        while (dpos >= pat[di]) { dpos -= pat[di]; di = (di + 1) % np; on = !on; }
        dleft = pat[di] - dpos;
    }
#define FLUSH() do { if (bn >= 2) stroke_poly(E, buf, bn, 0, hw, cap, join, miter); bn = 0; } while (0)
    for (int i = 0; i < segs; i++) {
        int j = (i + 1) % n;
        double ax = xy[i * 2], ay = xy[i * 2 + 1], bx = xy[j * 2], by = xy[j * 2 + 1];
        double l = sqrt((bx - ax) * (bx - ax) + (by - ay) * (by - ay)), pos = 0;
        while (pos < l) {
            double step = l - pos;
            if (np && dleft < step) step = dleft;
            double s0 = acc + pos, s1 = s0 + step;
            int vis = (!np || on) && s1 > from && s0 < to;
            if (vis) {
                double c0 = s0 < from ? from : s0, c1 = s1 > to ? to : s1;
                double u0 = (c0 - acc) / l, u1 = (c1 - acc) / l;
                double px0 = ax + (bx - ax) * u0, py0 = ay + (by - ay) * u0, px1 = ax + (bx - ax) * u1, py1 = ay + (by - ay) * u1;
                if (!bn) { buf[0] = px0; buf[1] = py0; bn = 1; }
                buf[bn * 2] = px1; buf[bn * 2 + 1] = py1; bn++;
                if (bn > n + 1) { FLUSH(); buf[0] = px1; buf[1] = py1; bn = 1; }
            } else FLUSH();
            pos += step;
            if (np) { dleft -= step; if (dleft <= 1e-9) { if (on) FLUSH(); di = (di + 1) % np; on = !on; dleft = pat[di]; } }
        }
        acc += l;
    }
    FLUSH();
#undef FLUSH
    free(buf);
}
static void render_shape(id l) {
    CGR b;
    double sc;
    int w, h;
    if (!layer_px(l, &b, &sc, &w, &h)) { if (g_ca_set_contents) ((void (*)(id, SEL, id))g_ca_set_contents)(l, S("setContents:"), (id)0); return; }
    IOSurfaceRef s = surface_for(l, w, h);
    if (!s) return;
    u8 *px = IOSurfaceGetBaseAddress(s);
    u64 bpr = IOSurfaceGetBytesPerRow(s);
    for (int y = 0; y < h; y++) memset(px + (u64)y * bpr, 0, (u64)w * 4);
    id path = (id)mtl_dget(l, "path");
    if (path) {
        paths_t P = {0};
        P.sx = P.sy = sc;
        P.ox = b.o.x;
        P.oy = b.o.y;
        P.flip = layer_flipped(l);
        P.h = h;
        CGPathApply(path, &P, path_apply);
        canvas_t C = {calloc((u64)w * h, sizeof(float)), w, h};
        id fc = (id)mtl_dget(l, "fillColor");
        if (fc && C.cov) {
            edges_t E = {0};
            for (int i = 0; i < P.n; i++) if (P.s[i].n >= 3) poly_edges(&E, P.s[i].xy, P.s[i].n);
            id fr = (id)mtl_dget(l, "fillRule");
            fill_edges(&C, &E, fr && !strcmp(sval(fr), "even-odd"));
            composite(px, bpr, &C, color_argb(fc, 1));
            free(E.e);
        }
        id stc = (id)mtl_dget(l, "strokeColor");
        double lw = mtl_dgetd(l, "lineWidth") * sc;
        double t0 = mtl_dgetd(l, "strokeStart"), t1 = mtl_dgetd(l, "strokeEnd");
        if (stc && lw > 0 && t1 > t0 && C.cov) {
            memset(C.cov, 0, (u64)w * h * sizeof(float));
            const char *capn = sval((id)mtl_dget(l, "lineCap")), *joinn = sval((id)mtl_dget(l, "lineJoin"));
            int cap = !strcmp(capn, "round") ? 1 : !strcmp(capn, "square") ? 2 : 0;
            int join = !strcmp(joinn, "round") ? 1 : !strcmp(joinn, "bevel") ? 2 : 0;
            double miter = mtl_dgetd(l, "miterLimit");
            id dp = (id)mtl_dget(l, "lineDashPattern");
            double pat[16];
            int np = 0;
            u64 dn = dp ? MSG0(u64, dp, "count") : 0;
            for (u64 i = 0; i < dn && np < 16; i++) pat[np++] = MSG0(double, MSG(id, dp, "objectAtIndex:", i), "doubleValue") * sc;
            edges_t E = {0};
            for (int i = 0; i < P.n; i++) {
                sp_t *sp = &P.s[i];
                if (!sp->n) continue;
                if (np || t0 > 0 || t1 < 1) dash_poly(sp->xy, sp->n, sp->closed, pat, np, mtl_dgetd(l, "lineDashPhase") * sc, t0, t1, &E, lw / 2, cap, join, miter);
                else stroke_poly(&E, sp->xy, sp->n, sp->closed, lw / 2, cap, join, miter);
            }
            fill_edges(&C, &E, 0);
            composite(px, bpr, &C, color_argb(stc, 1));
            free(E.e);
        }
        free(C.cov);
        for (int i = 0; i < P.n; i++) free(P.s[i].xy);
        free(P.s);
    }
    present_surface(l, s);
}
static void color_comp(id c, double o[4]) {
    if (!c) { o[0] = o[1] = o[2] = o[3] = 0; return; }
    const double *k = (const double *)((u8 *)c + 8);
    for (int i = 0; i < 4; i++) o[i] = k[i] < 0 ? 0 : k[i] > 1 ? 1 : k[i];
}
static void render_gradient(id l) {
    CGR b;
    double sc;
    int w, h;
    if (!layer_px(l, &b, &sc, &w, &h)) return;
    id cols = (id)mtl_dget(l, "colors");
    u64 n = cols ? MSG0(u64, cols, "count") : 0;
    IOSurfaceRef s = surface_for(l, w, h);
    if (!s) return;
    u8 *px = IOSurfaceGetBaseAddress(s);
    u64 bpr = IOSurfaceGetBytesPerRow(s);
    if (!n) { for (int y = 0; y < h; y++) memset(px + (u64)y * bpr, 0, (u64)w * 4); present_surface(l, s); return; }
    if (n > 32) n = 32;
    double c[32][4], loc[32];
    id locs = (id)mtl_dget(l, "locations");
    for (u64 i = 0; i < n; i++) {
        color_comp(MSG(id, cols, "objectAtIndex:", i), c[i]);
        loc[i] = locs && MSG0(u64, locs, "count") > i ? MSG0(double, MSG(id, locs, "objectAtIndex:", i), "doubleValue") : (n > 1 ? (double)i / (double)(n - 1) : 0);
    }
    double sp[2], ep[2];
    mtl_dgetv(l, "startPoint", sp, 2);
    mtl_dgetv(l, "endPoint", ep, 2);
    const char *type = sval((id)mtl_dget(l, "type"));
    int kind = !strcmp(type, "radial") ? 1 : !strcmp(type, "conic") ? 2 : 0;
    int flip = layer_flipped(l);
    double dx = ep[0] - sp[0], dy = ep[1] - sp[1], dd = dx * dx + dy * dy;
    for (int y = 0; y < h; y++) {
        double uy = (y + 0.5) / h;
        if (!flip) uy = 1 - uy;
        u8 *row = px + (u64)y * bpr;
        for (int x = 0; x < w; x++) {
            double ux = (x + 0.5) / w, t;
            if (kind == 0) t = dd > 0 ? ((ux - sp[0]) * dx + (uy - sp[1]) * dy) / dd : 0;
            else if (kind == 1) { double rx = fabs(dx) > 1e-9 ? (ux - sp[0]) / dx : 0, ry = fabs(dy) > 1e-9 ? (uy - sp[1]) / dy : 0; t = sqrt(rx * rx + ry * ry); }
            else { double a = atan2(uy - sp[1], ux - sp[0]) - atan2(dy, dx); t = a / 6.283185307179586; t -= floor(t); }
            if (t < 0) t = 0;
            if (t > 1) t = 1;
            double o[4];
            if (t <= loc[0]) memcpy(o, c[0], sizeof o);
            else if (t >= loc[n - 1]) memcpy(o, c[n - 1], sizeof o);
            else {
                u64 i = 1;
                while (i < n - 1 && t > loc[i]) i++;
                double f = loc[i] > loc[i - 1] ? (t - loc[i - 1]) / (loc[i] - loc[i - 1]) : 1;
                for (int k = 0; k < 4; k++) o[k] = c[i - 1][k] + (c[i][k] - c[i - 1][k]) * f;
            }
            u8 *p = row + x * 4;
            p[0] = (u8)(o[2] * o[3] * 255 + 0.5);
            p[1] = (u8)(o[1] * o[3] * 255 + 0.5);
            p[2] = (u8)(o[0] * o[3] * 255 + 0.5);
            p[3] = (u8)(o[3] * 255 + 0.5);
        }
    }
    present_surface(l, s);
}
static id text_font(id l, double size) {
    id f = (id)mtl_dget(l, "font");
    id name = 0;
    if (f && cx_is_str(f)) name = f;
    else if (f && responds(f, "fontName")) name = MSG0(id, f, "fontName");
    id font = CTFontCreateWithName(name ? name : cx_str("Helvetica"), size, 0);
    return font;
}
static void render_text(id l) {
    CGR b;
    double sc;
    int w, h;
    if (!layer_px(l, &b, &sc, &w, &h)) return;
    IOSurfaceRef s = surface_for(l, w, h);
    if (!s) return;
    u8 *px = IOSurfaceGetBaseAddress(s);
    u64 bpr = IOSurfaceGetBytesPerRow(s);
    for (int y = 0; y < h; y++) memset(px + (u64)y * bpr, 0, (u64)w * 4);
    id str = (id)mtl_dget(l, "string");
    if (str && !cx_is_str(str) && responds(str, "string")) str = MSG0(id, str, "string");
    if (!str || !cx_is_str(str)) { present_surface(l, s); return; }
    double size = mtl_dgetd(l, "fontSize");
    if (size <= 0) size = 36;
    id font = text_font(l, size * sc);
    if (!font) { present_surface(l, s); return; }
    u64 len = MSG0(u64, str, "length");
    if (len > 4096) len = 4096;
    u16 *ch = malloc((len + 1) * 2), *gl = malloc((len + 1) * 2);
    ct_sz_t *adv = malloc((len + 1) * sizeof(ct_sz_t));
    ct_pt_t *pos = malloc((len + 1) * sizeof(ct_pt_t));
    for (u64 i = 0; i < len; i++) ch[i] = MSG(u16, str, "characterAtIndex:", i);
    CTFontGetGlyphsForCharacters(font, ch, gl, (long)len);
    CTFontGetAdvancesForGlyphs(font, 0, gl, adv, (long)len);
    double asc = CTFontGetAscent(font), desc = CTFontGetDescent(font), lh = asc + desc;
    int wrapped = (int)mtl_dget(l, "wrapped");
    const char *al = sval((id)mtl_dget(l, "alignmentMode"));
    const char *trunc = sval((id)mtl_dget(l, "truncationMode"));
    /* bitmap context sobre o IOSurface (BGRA pré-multiplicado: premultipliedFirst | byteOrder32Little) */
    void *ctx = CGBitmapContextCreate(px, w, h, 8, bpr, 0, 0x2002);
    id fg = (id)mtl_dget(l, "foregroundColor");
    if (ctx) cx_bm_set_fill(ctx, fg ? color_argb(fg, 1) : 0xFFFFFFFFu);
    u64 i = 0;
    double y = asc;
    while (i < len && ctx) {
        /* uma linha: até \n ou até caber */
        u64 j = i, lastsp = (u64)-1;
        double lw = 0;
        while (j < len && ch[j] != '\n') {
            if (wrapped && lw + adv[j].w > w && j > i) break;
            if (ch[j] == ' ') lastsp = j;
            lw += adv[j].w;
            j++;
        }
        if (wrapped && j < len && ch[j] != '\n' && lastsp != (u64)-1 && lastsp > i) j = lastsp + 1;
        u64 e = j;
        lw = 0;
        for (u64 k = i; k < e; k++) lw += adv[k].w;
        int ell = 0;
        if (!wrapped && lw > w && strcmp(trunc, "none") && trunc[0]) {
            u16 dot;
            u16 ellc = 0x2026;
            CTFontGetGlyphsForCharacters(font, &ellc, &dot, 1);
            ct_sz_t da;
            CTFontGetAdvancesForGlyphs(font, 0, &dot, &da, 1);
            while (e > i && lw + da.w > w) { e--; lw -= adv[e].w; }
            gl[e] = dot;
            adv[e] = da;
            lw += da.w;
            ell = 1;
        }
        double x0 = !strcmp(al, "center") ? (w - lw) / 2 : !strcmp(al, "right") ? w - lw : 0;
        double x = x0;
        u64 cnt = e - i + (u64)ell;
        for (u64 k = 0; k < cnt; k++) { pos[k].x = x; pos[k].y = h - y; x += adv[i + k].w; }
        CTFontDrawGlyphs(font, gl + i, pos, cnt, ctx);
        y += lh;
        if (y - asc > h || !wrapped) { if (!wrapped) { while (j < len && ch[j] != '\n') j++; } }
        i = j < len && ch[j] == '\n' ? j + 1 : j;
        if (!wrapped && ell) break;
        if (y - asc > h) break;
    }
    if (ctx) CGContextRelease(ctx);
    objc_release(font);
    free(ch); free(gl); free(adv); free(pos);
    present_surface(l, s);
}
static void render_content(id l) {
    if (is_kind(l, &CAShapeLayer_c)) render_shape(l);
    else if (is_kind(l, &CAGradientLayer_c)) render_gradient(l);
    else if (is_kind(l, &CATextLayer_c)) render_text(l);
}
/* mudanças de tamanho e escala também redesenham */
static void cl_set_bounds(id s, SEL c, CGR r) { IMP i = cx_lookup(&CALayer_c, c); if (i) ((void (*)(id, SEL, CGR))i)(s, c, r); mark_dirty(s); }
static void cl_set_scale(id s, SEL c, double v) { IMP i = cx_lookup(&CALayer_c, c); if (i) ((void (*)(id, SEL, double))i)(s, c, v); mark_dirty(s); }
static void cl_set_flip(id s, SEL c, BOOL v) { IMP i = cx_lookup(&CALayer_c, c); if (i) ((void (*)(id, SEL, BOOL))i)(s, c, v); mark_dirty(s); }
static void cl_display(id s, SEL c) { (void)c; render_content(s); }
static id shape_init(id s, SEL c);
static id cl_init_shape(id s, SEL c) {
    IMP i = cx_lookup(&CALayer_c, c);
    if (i) s = ((id (*)(id, SEL))i)(s, c);
    if (!s) return s;
    return shape_init(s, c);
}
static id shape_init(id s, SEL c) {
    (void)c;
    if (is_kind(s, &CAShapeLayer_c)) { id k = cx_color(0, 0, 0, 1); mtl_dseto(s, "fillColor", k); objc_release(k); }
    if (is_kind(s, &CATextLayer_c)) { id k = cx_color(1, 1, 1, 1); mtl_dseto(s, "foregroundColor", k); objc_release(k); }
    return s;
}

/* ================================================================ CADisplayLink */
static void dl_fire(id s, SEL c, id timer) {
    (void)c; (void)timer;
    if (mtl_dget(s, "paused") || !mtl_dget(s, "_valid")) return;
    double now = CACurrentMediaTime(), d = 1.0 / 60;
    u64 fps = mtl_dget(s, "preferredFramesPerSecond");
    if (fps) d = 1.0 / (double)fps;
    mtl_dsetv(s, "timestamp", &now, 1);
    double tt = now + d;
    mtl_dsetv(s, "targetTimestamp", &tt, 1);
    mtl_dsetv(s, "duration", &d, 1);
    id t = (id)mtl_dget(s, "_target");
    SEL sel = (SEL)mtl_dget(s, "_sel");
    if (t && sel) ((void (*)(id, SEL, id))objc_msgSend)(t, sel, s);
}
static id dl_make(id target, SEL sel) {
    id d = MSG0(id, MSG0(id, (id)&CADisplayLink_c, "alloc"), "init");
    mtl_dseto(d, "_target", target);
    u64 sv = (u64)sel;
    mtl_dsetv(d, "_sel", (double *)&sv, 1);
    return objc_autorelease(d);
}
static id dl_with(id cls, SEL c, id target, SEL sel) { (void)cls; (void)c; return dl_make(target, sel); }
static id v_dl_with(id self, SEL c, id target, SEL sel) { (void)self; (void)c; return dl_make(target, sel); }
static void dl_add(id s, SEL c, id rl, id mode) {
    (void)c; (void)rl; (void)mode;
    if (mtl_dget(s, "_scheduled")) return;
    u64 one = 1;
    mtl_dsetv(s, "_scheduled", (double *)&one, 1);
    /* o timer retém o link (como o run loop no macOS) */
    MSG(id, (id)cx_class_named("NSTimer"), "scheduledTimerWithTimeInterval:target:selector:userInfo:repeats:", 1.0 / 60, s, S("_mosFire:"), (id)0, (BOOL)YES);
}
static void dl_invalidate(id s, SEL c) { (void)c; u64 z = 0; mtl_dsetv(s, "_valid", (double *)&z, 1); mtl_dseto(s, "_target", 0); }
static void dl_remove(id s, SEL c, id rl, id mode) { (void)rl; (void)mode; dl_invalidate(s, c); }
static float dl_maxfps(id s, SEL c) { (void)s; (void)c; return 60; }

/* ================================================================ CARenderer (sem desenho) */
static id rd_with_tex(id cls, SEL c, id tex, id opts) {
    (void)c; (void)opts;
    id r = MSG0(id, MSG0(id, cls, "alloc"), "init");
    mtl_dseto(r, "_texture", tex);
    return objc_autorelease(r);
}
static void rd_noop(id s, SEL c) { (void)s; (void)c; }
static double rd_next(id s, SEL c) { (void)s; (void)c; return CACurrentMediaTime() + 1.0 / 60; }
static CGR rd_bounds(id s, SEL c) { (void)c; CGR r = {{0, 0}, {0, 0}}; double v[2]; mtl_dgetv(s, "_boundsSize", v, 2); r.s.w = v[0]; r.s.h = v[1]; return r; }
static void rd_set_bounds(id s, SEL c, CGR r) { (void)c; double v[2] = {r.s.w, r.s.h}; mtl_dsetv(s, "_boundsSize", v, 2); }
static double tl_fade(id cls, SEL c) { (void)cls; (void)c; return 0.25; }
static id an_new(id cls, SEL c) { (void)c; return objc_autorelease(MSG0(id, MSG0(id, cls, "alloc"), "init")); }
static id an_kp(id cls, SEL c, id kp) { (void)c; id a = an_new(cls, c); mtl_dseto(a, "keyPath", kp); return a; }
static id an_default(id cls, SEL c, id k) { (void)cls; (void)c; (void)k; return 0; }
static BOOL an_should_archive(id s, SEL c, id k) { (void)s; (void)c; (void)k; return YES; }
static double sp_settle(id s, SEL c) { (void)c; double d = A_d(s, "duration"); u64 save; memcpy(&save, &d, 8); double z = 0; mtl_dsetv(s, "duration", &z, 1); double r = anim_dur(s); mtl_dsetv(s, "duration", &d, 1); (void)save; return r; }
static id tf_dummy_init(id s, SEL c) { (void)c; tf_t *t = (tf_t *)s; t->p[0] = 0.25; t->p[1] = 0.1; t->p[2] = 0.25; t->p[3] = 1; return s; }

void cx_coreanimation_init(void) {
    static int done;
    if (done || !CALayer_c.vtable) return;
    done = 1;
    cx_class_t *ob = cx_class_named("NSObject"), *nv = cx_class_named("NSValue");
    /* NSValue */
    cx_define_class(&NSConcreteStructValue_c, &NSConcreteStructValue_m, "NSConcreteStructValue", nv ? nv : ob, (u32)(sizeof(sval_t) - 8));
#define VM(s, f) cx_add_method(&NSConcreteStructValue_c, s, (void *)(f))
    VM("getValue:", sv_getvalue); VM("getValue:size:", sv_getvalue_n); VM("objCType", sv_type); VM("isEqual:", sv_equal);
    VM("isEqualToValue:", sv_equal); VM("hash", sv_hash); VM("copy", sv_copy); VM("copyWithZone:", sv_copy); VM("description", sv_desc);
    VM("pointValue", sv_getp); VM("CGPointValue", sv_getp); VM("sizeValue", sv_gets); VM("CGSizeValue", sv_gets);
    VM("rectValue", sv_getr); VM("CGRectValue", sv_getr); VM("CATransform3DValue", sv_gett); VM("CGAffineTransformValue", sv_geta);
    VM("rangeValue", sv_getrange); VM("pointerValue", sv_getptr); VM("nonretainedObjectValue", sv_getptr);
    VM("UIEdgeInsetsValue", sv_getei); VM("edgeInsetsValue", sv_getei); VM("CGVectorValue", sv_getp); VM("UIOffsetValue", sv_getp);
#undef VM
    if (nv) {
#define VC(s, f) cx_add_cmethod(nv, s, (void *)(f))
        VC("valueWithBytes:objCType:", sv_with_bytes); VC("value:withObjCType:", sv_with_bytes);
        VC("valueWithPoint:", sv_point); VC("valueWithCGPoint:", sv_point); VC("valueWithSize:", sv_size); VC("valueWithCGSize:", sv_size);
        VC("valueWithRect:", sv_rect); VC("valueWithCGRect:", sv_rect); VC("valueWithCATransform3D:", sv_t3);
        VC("valueWithCGAffineTransform:", sv_affine); VC("valueWithRange:", sv_range); VC("valueWithPointer:", sv_ptr);
        VC("valueWithNonretainedObject:", sv_nonret); VC("valueWithUIEdgeInsets:", sv_insets); VC("valueWithEdgeInsets:", sv_insets);
        VC("valueWithCGVector:", sv_point); VC("valueWithUIOffset:", sv_point);
#undef VC
        cx_add_method(nv, "initWithBytes:objCType:", (void *)sv_init_bytes);
    }

    /* timing */
    cx_define_class(&CAMediaTimingFunction_c, &CAMediaTimingFunction_m, "CAMediaTimingFunction", ob, (u32)(sizeof(tf_t) - 8));
    cx_add_cmethod(&CAMediaTimingFunction_c, "functionWithName:", (void *)tf_named);
    cx_add_cmethod(&CAMediaTimingFunction_c, "functionWithControlPoints::::", (void *)tf_points);
    cx_add_method(&CAMediaTimingFunction_c, "initWithControlPoints::::", (void *)tf_init_points);
    cx_add_method(&CAMediaTimingFunction_c, "getControlPointAtIndex:values:", (void *)tf_get);
    cx_add_method(&CAMediaTimingFunction_c, "copy", (void *)tf_copy);
    cx_add_method(&CAMediaTimingFunction_c, "copyWithZone:", (void *)tf_copy);
    cx_add_method(&CAMediaTimingFunction_c, "init", (void *)tf_dummy_init);
    cx_define_class(&CAValueFunction_c, &CAValueFunction_m, "CAValueFunction", ob, (u32)(sizeof(vf_t) - 8));
    cx_add_cmethod(&CAValueFunction_c, "functionWithName:", (void *)vf_named);
    cx_add_method(&CAValueFunction_c, "name", (void *)vf_name);

    /* animações */
    mtl_def_desc(&CAAnimation_c, &CAAnimation_m, "CAAnimation", ps_anim, ob, 0);
    mtl_def_desc(&CAPropertyAnimation_c, &CAPropertyAnimation_m, "CAPropertyAnimation", ps_panim, &CAAnimation_c, 0);
    mtl_def_desc(&CABasicAnimation_c, &CABasicAnimation_m, "CABasicAnimation", ps_basic, &CAPropertyAnimation_c, 0);
    mtl_def_desc(&CASpringAnimation_c, &CASpringAnimation_m, "CASpringAnimation", ps_spring, &CABasicAnimation_c, 0);
    mtl_def_desc(&CAKeyframeAnimation_c, &CAKeyframeAnimation_m, "CAKeyframeAnimation", ps_key, &CAPropertyAnimation_c, 0);
    mtl_def_desc(&CAAnimationGroup_c, &CAAnimationGroup_m, "CAAnimationGroup", ps_group, &CAAnimation_c, 0);
    mtl_def_desc(&CATransition_c, &CATransition_m, "CATransition", ps_trans, &CAAnimation_c, 0);
    cx_add_cmethod(&CAAnimation_c, "animation", (void *)an_new);
    cx_add_cmethod(&CAAnimation_c, "defaultValueForKey:", (void *)an_default);
    cx_add_method(&CAAnimation_c, "shouldArchiveValueForKey:", (void *)an_should_archive);
    cx_add_cmethod(&CAPropertyAnimation_c, "animationWithKeyPath:", (void *)an_kp);
    cx_add_method(&CASpringAnimation_c, "settlingDuration", (void *)sp_settle);
    cx_add_method(&CASpringAnimation_c, "perceptualDuration", (void *)sp_settle);

    /* camadas */
    mtl_def_desc(&CAShapeLayer_c, &CAShapeLayer_m, "CAShapeLayer", ps_shape, &CALayer_c, content_changed);
    mtl_def_desc(&CAGradientLayer_c, &CAGradientLayer_m, "CAGradientLayer", ps_grad, &CALayer_c, content_changed);
    mtl_def_desc(&CATextLayer_c, &CATextLayer_m, "CATextLayer", ps_text, &CALayer_c, content_changed);
    cx_class_t *content[3] = {&CAShapeLayer_c, &CAGradientLayer_c, &CATextLayer_c};
    for (int i = 0; i < 3; i++) {
        cx_add_method(content[i], "setBounds:", (void *)cl_set_bounds);
        cx_add_method(content[i], "setFrame:", (void *)cl_set_bounds);
        cx_add_method(content[i], "setContentsScale:", (void *)cl_set_scale);
        cx_add_method(content[i], "setGeometryFlipped:", (void *)cl_set_flip);
        cx_add_method(content[i], "display", (void *)cl_display);
        class_replaceMethod(content[i], S("init"), (void *)cl_init_shape, "@@:");
    }
    mtl_def_desc(&CAReplicatorLayer_c, &CAReplicatorLayer_m, "CAReplicatorLayer", ps_repl, &CALayer_c, 0);
    mtl_def_desc(&CATiledLayer_c, &CATiledLayer_m, "CATiledLayer", ps_tiled, &CALayer_c, 0);
    cx_add_cmethod(&CATiledLayer_c, "fadeDuration", (void *)tl_fade);
    mtl_def_desc(&CAEmitterLayer_c, &CAEmitterLayer_m, "CAEmitterLayer", ps_emitter, &CALayer_c, 0);
    mtl_def_desc(&CAScrollLayer_c, &CAScrollLayer_m, "CAScrollLayer", ps_none, &CALayer_c, 0);
    mtl_def_desc(&CAEmitterCell_c, &CAEmitterCell_m, "CAEmitterCell", ps_cell, ob, 0);
    cx_add_cmethod(&CAEmitterCell_c, "emitterCell", (void *)an_new);

    /* CALayer: animações de verdade */
    g_ca_set_contents = cx_lookup(&CALayer_c, S("setContents:"));
#define LR(s, f) class_replaceMethod(&CALayer_c, S(s), (void *)(f), "@@:")
    LR("addAnimation:forKey:", ca_add_anim); LR("removeAnimationForKey:", ca_remove_anim);
    LR("removeAllAnimations", ca_remove_all); LR("animationForKey:", ca_anim_for_key); LR("animationKeys", ca_anim_keys);
#undef LR
    cx_define_class(&CAMosAnimator_c, &CAMosAnimator_m, "CAMosAnimator", ob, 0);
    cx_add_method(&CAMosAnimator_c, "tick:", (void *)tick);

    /* CADisplayLink */
    mtl_def_desc(&CADisplayLink_c, &CADisplayLink_m, "CADisplayLink", ps_dlink, ob, 0);
    cx_add_cmethod(&CADisplayLink_c, "displayLinkWithTarget:selector:", (void *)dl_with);
    cx_add_method(&CADisplayLink_c, "addToRunLoop:forMode:", (void *)dl_add);
    cx_add_method(&CADisplayLink_c, "removeFromRunLoop:forMode:", (void *)dl_remove);
    cx_add_method(&CADisplayLink_c, "invalidate", (void *)dl_invalidate);
    cx_add_method(&CADisplayLink_c, "_mosFire:", (void *)dl_fire);
    cx_add_method(&CADisplayLink_c, "maximumFramesPerSecond", (void *)dl_maxfps);
    const char *hosts[] = {"NSView", "NSWindow", "NSScreen"};
    for (int i = 0; i < 3; i++) { cx_class_t *h = cx_class_named(hosts[i]); if (h && !cx_responds(h, S("displayLinkWithTarget:selector:"))) cx_add_method(h, "displayLinkWithTarget:selector:", (void *)v_dl_with); }

    /* CARenderer */
    mtl_def_desc(&CARenderer_c, &CARenderer_m, "CARenderer", ps_renderer, ob, 0);
    cx_add_cmethod(&CARenderer_c, "rendererWithMTLTexture:options:", (void *)rd_with_tex);
    cx_add_method(&CARenderer_c, "beginFrameAtTime:timeStamp:", (void *)rd_noop);
    cx_add_method(&CARenderer_c, "addUpdateRect:", (void *)rd_noop);
    cx_add_method(&CARenderer_c, "render", (void *)rd_noop);
    cx_add_method(&CARenderer_c, "endFrame", (void *)rd_noop);
    cx_add_method(&CARenderer_c, "setDestination:", (void *)rd_noop);
    cx_add_method(&CARenderer_c, "nextFrameTime", (void *)rd_next);
    cx_add_method(&CARenderer_c, "updateBounds", (void *)rd_bounds);
    cx_add_method(&CARenderer_c, "bounds", (void *)rd_bounds);
    cx_add_method(&CARenderer_c, "setBounds:", (void *)rd_set_bounds);
    { void cx_coreanimation_frr(void); cx_coreanimation_frr(); }
}

/* preferredFrameRateRange (struct de 3 floats): guardada à parte, só para ler de volta */
static CAFrameRateRange g_frr;
static CAFrameRateRange frr_get(id s, SEL c) { (void)s; (void)c; return g_frr; }
static void frr_set(id s, SEL c, CAFrameRateRange r) { (void)s; (void)c; g_frr = r; }
void cx_coreanimation_frr(void) {
    cx_class_t *k[2] = {&CAAnimation_c, &CADisplayLink_c};
    for (int i = 0; i < 2; i++) {
        cx_add_method(k[i], "preferredFrameRateRange", (void *)frr_get);
        cx_add_method(k[i], "setPreferredFrameRateRange:", (void *)frr_set);
    }
}
