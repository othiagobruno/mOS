/*
 * mOS — UIKit.framework do Mac Catalyst, o núcleo que leva um app UIKit até uma janela na tela:
 *   UIApplicationMain -> UIApplication + delegate (application:didFinishLaunchingWithOptions:) -> cena
 *   (application:configurationForConnectingSceneSession:options: ou a classe da cena do Info.plist) -> UIWindowScene +
 *   delegate da cena (scene:willConnectToSession:options:) -> UIWindow.makeKeyAndVisible -> NSWindow do AppKit do mOS,
 *   cujo conteúdo é uma NSView hospedeira que desenha a árvore de UIViews (fundo, -drawRect: e o texto do UILabel).
 * O laço de eventos é o do NSApplication do mOS. Ainda não há eventos de toque/mouse entregues às UIViews, Auto Layout,
 * animações nem as outras classes de controle (ver docs/uikit-catalyst.md).
 */
#import "UIKitMOS.h"

extern void _mos_uikit_constants_init(void);
extern NSString *const UIApplicationDidFinishLaunchingNotification, *const UIApplicationDidBecomeActiveNotification;
extern NSString *const UISceneWillConnectNotification, *const UISceneDidActivateNotification;
extern NSString *const UIWindowSceneSessionRoleApplication, *const UIWindowDidBecomeKeyNotification;
extern NSString *const UIWindowDidBecomeVisibleNotification;

/* CoreGraphics do libSystem */
typedef void *CGContextRef;
extern void CGContextFillRect(CGContextRef c, CGRect r);
extern void CGContextSetFillColorWithColor(CGContextRef c, void *color);
extern void CGContextSaveGState(CGContextRef c);
extern void CGContextRestoreGState(CGContextRef c);
extern void CGContextTranslateCTM(CGContextRef c, CGFloat x, CGFloat y);
extern void CGContextScaleCTM(CGContextRef c, CGFloat x, CGFloat y);
extern void CGContextClipToRect(CGContextRef c, CGRect r);

extern void CGContextDrawImage(CGContextRef c, CGRect r, void *img);
extern void *CGColorCreateSRGB(CGFloat r, CGFloat g, CGFloat b, CGFloat a);
extern size_t CGImageGetWidth(void *img), CGImageGetHeight(void *img);
extern void *CGImageRetain(void *img);
extern void CGImageRelease(void *img);

/* remove pelo ponteiro (o NSMutableArray do mOS não tem -removeObjectIdenticalTo:) */
static void mos_remove_identical(NSMutableArray *a, id o) {
    for (NSInteger i = (NSInteger)a.count - 1; i >= 0; i--) if ([a objectAtIndex:(NSUInteger)i] == o) [a removeObjectAtIndex:(NSUInteger)i];
}

/* ================================================================ contexto gráfico (UIGraphicsPushContext...) */
static CGContextRef g_ctx_stack[32];
static int g_ctx_depth;
void UIGraphicsPushContext(CGContextRef c) { if (g_ctx_depth < 32) g_ctx_stack[g_ctx_depth++] = c; }
void UIGraphicsPopContext(void) { if (g_ctx_depth > 0) g_ctx_depth--; }
CGContextRef UIGraphicsGetCurrentContext(void) {
    if (g_ctx_depth > 0) return g_ctx_stack[g_ctx_depth - 1];
    NSGraphicsContext *g = [NSGraphicsContext currentContext];
    return g ? (CGContextRef)g.CGContext : 0;
}
void UIRectFill(CGRect r) { CGContextRef c = UIGraphicsGetCurrentContext(); if (c) CGContextFillRect(c, r); }
void UIRectFillUsingBlendMode(CGRect r, int mode) { (void)mode; UIRectFill(r); }

/* ================================================================ UIResponder */
@implementation UIResponder
- (UIResponder *)nextResponder { return nil; }
- (BOOL)becomeFirstResponder { return [self canBecomeFirstResponder]; }
- (BOOL)resignFirstResponder { return YES; }
- (BOOL)canBecomeFirstResponder { return NO; }
- (BOOL)isFirstResponder { return NO; }
@end

/* ================================================================ UIColor */
@implementation UIColor
+ (UIColor *)colorWithRed:(CGFloat)r green:(CGFloat)g blue:(CGFloat)b alpha:(CGFloat)a {
    return [[[self alloc] initWithRed:r green:g blue:b alpha:a] autorelease];
}
+ (UIColor *)colorWithWhite:(CGFloat)w alpha:(CGFloat)a { return [self colorWithRed:w green:w blue:w alpha:a]; }
- (instancetype)initWithRed:(CGFloat)r green:(CGFloat)g blue:(CGFloat)b alpha:(CGFloat)a {
    if ((self = [super init])) { _r = r; _g = g; _b = b; _a = a; }
    return self;
}
- (instancetype)initWithWhite:(CGFloat)w alpha:(CGFloat)a { return [self initWithRed:w green:w blue:w alpha:a]; }
- (instancetype)initWithCGColor:(void *)c { (void)c; return [self initWithRed:0 green:0 blue:0 alpha:1]; }
+ (UIColor *)colorWithCGColor:(void *)c { return [[[self alloc] initWithCGColor:c] autorelease]; }
- (id)copyWithZone:(NSZone *)z { (void)z; return [self retain]; }
- (void *)CGColor { if (!_cg) _cg = CGColorCreateSRGB(_r, _g, _b, _a); return _cg; }
- (void)setFill { CGContextRef c = UIGraphicsGetCurrentContext(); if (c) CGContextSetFillColorWithColor(c, [self CGColor]); }
- (void)set { [self setFill]; }
- (void)setStroke {}
- (UIColor *)colorWithAlphaComponent:(CGFloat)a { return [UIColor colorWithRed:_r green:_g blue:_b alpha:a]; }
- (BOOL)getRed:(CGFloat *)r green:(CGFloat *)g blue:(CGFloat *)b alpha:(CGFloat *)a {
    if (r) *r = _r; if (g) *g = _g; if (b) *b = _b; if (a) *a = _a; return YES;
}
- (BOOL)getWhite:(CGFloat *)w alpha:(CGFloat *)a { if (w) *w = (_r + _g + _b) / 3; if (a) *a = _a; return YES; }
- (UIColor *)resolvedColorWithTraitCollection:(id)t { (void)t; return self; }
- (BOOL)isEqual:(id)o {
    if (![o isKindOfClass:[UIColor class]]) return NO;
    UIColor *c = o;
    return c->_r == _r && c->_g == _g && c->_b == _b && c->_a == _a;
}
- (NSUInteger)hash { return (NSUInteger)(_r * 255) << 24 | (NSUInteger)(_g * 255) << 16 | (NSUInteger)(_b * 255) << 8 | (NSUInteger)(_a * 255); }
- (NSString *)description { return [NSString stringWithFormat:@"UIColor(%.3f %.3f %.3f %.3f)", _r, _g, _b, _a]; }
#define C(N, R, G, B, A) + (UIColor *)N { return [self colorWithRed:R green:G blue:B alpha:A]; }
C(blackColor, 0, 0, 0, 1) C(whiteColor, 1, 1, 1, 1) C(clearColor, 0, 0, 0, 0) C(redColor, 1, 0, 0, 1)
C(greenColor, 0, 1, 0, 1) C(blueColor, 0, 0, 1, 1) C(grayColor, .5, .5, .5, 1) C(lightGrayColor, 2./3, 2./3, 2./3, 1)
C(darkGrayColor, 1./3, 1./3, 1./3, 1) C(yellowColor, 1, 1, 0, 1) C(orangeColor, 1, .5, 0, 1) C(purpleColor, .5, 0, .5, 1)
C(brownColor, .6, .4, .2, 1) C(cyanColor, 0, 1, 1, 1) C(magentaColor, 1, 0, 1, 1)
/* cores dinâmicas do sistema (tema claro do mOS) */
C(systemBackgroundColor, 1, 1, 1, 1) C(secondarySystemBackgroundColor, .95, .95, .97, 1)
C(tertiarySystemBackgroundColor, 1, 1, 1, 1) C(systemGroupedBackgroundColor, .95, .95, .97, 1)
C(secondarySystemGroupedBackgroundColor, 1, 1, 1, 1) C(labelColor, 0, 0, 0, 1) C(secondaryLabelColor, .24, .24, .26, .6)
C(tertiaryLabelColor, .24, .24, .26, .3) C(placeholderTextColor, .24, .24, .26, .3) C(separatorColor, .24, .24, .26, .29)
C(opaqueSeparatorColor, .78, .78, .78, 1) C(linkColor, 0, .48, 1, 1) C(systemBlueColor, 0, .48, 1, 1)
C(systemGreenColor, .2, .78, .35, 1) C(systemRedColor, 1, .23, .19, 1) C(systemOrangeColor, 1, .58, 0, 1)
C(systemYellowColor, 1, .8, 0, 1) C(systemGrayColor, .56, .56, .58, 1) C(systemGray2Color, .68, .68, .7, 1)
C(systemGray3Color, .78, .78, .8, 1) C(systemGray4Color, .82, .82, .84, 1) C(systemGray5Color, .9, .9, .92, 1)
C(systemGray6Color, .95, .95, .97, 1) C(systemFillColor, .47, .47, .5, .2) C(systemTealColor, .19, .69, .78, 1)
C(systemPinkColor, 1, .18, .33, 1) C(systemPurpleColor, .69, .32, .87, 1) C(systemIndigoColor, .35, .34, .84, 1)
#undef C
+ (UIColor *)tintColor { return [self systemBlueColor]; }
+ (UIColor *)colorNamed:(NSString *)n { (void)n; return [self systemBlueColor]; }
@end

/* ================================================================ UIFont */
@implementation UIFont
+ (UIFont *)fontWithName:(NSString *)n size:(CGFloat)s {
    UIFont *f = [[[self alloc] init] autorelease];
    f->_name = [n copy]; f->_size = s > 0 ? s : 13;
    return f;
}
+ (UIFont *)systemFontOfSize:(CGFloat)s { return [self fontWithName:@".AppleSystemUIFont" size:s]; }
+ (UIFont *)systemFontOfSize:(CGFloat)s weight:(CGFloat)w { UIFont *f = [self systemFontOfSize:s]; f->_weight = w; return f; }
+ (UIFont *)boldSystemFontOfSize:(CGFloat)s { return [self systemFontOfSize:s weight:0.4]; }
+ (UIFont *)italicSystemFontOfSize:(CGFloat)s { return [self systemFontOfSize:s]; }
+ (UIFont *)monospacedSystemFontOfSize:(CGFloat)s weight:(CGFloat)w { UIFont *f = [self fontWithName:@"Menlo" size:s]; f->_weight = w; return f; }
+ (UIFont *)monospacedDigitSystemFontOfSize:(CGFloat)s weight:(CGFloat)w { return [self systemFontOfSize:s weight:w]; }
+ (UIFont *)preferredFontForTextStyle:(NSString *)style {
    CGFloat s = 13;   /* tamanhos do Catalyst (escala do Mac) */
    if ([style hasSuffix:@"Title0"]) s = 26; else if ([style hasSuffix:@"Title1"]) s = 22; else if ([style hasSuffix:@"Title2"]) s = 17;
    else if ([style hasSuffix:@"Title3"]) s = 15; else if ([style hasSuffix:@"Caption1"] || [style hasSuffix:@"Caption2"]) s = 10;
    else if ([style hasSuffix:@"Footnote"]) s = 10; else if ([style hasSuffix:@"Subhead"] || [style hasSuffix:@"Callout"]) s = 12;
    UIFont *f = [self systemFontOfSize:s];
    if ([style hasSuffix:@"Headline"]) f->_weight = 0.3;
    return f;
}
+ (UIFont *)preferredFontForTextStyle:(NSString *)style compatibleWithTraitCollection:(id)t { (void)t; return [self preferredFontForTextStyle:style]; }
- (id)copyWithZone:(NSZone *)z { (void)z; return [self retain]; }
- (CGFloat)pointSize { return _size; }
- (NSString *)fontName { return _name; }
- (NSString *)familyName { return _name; }
- (CGFloat)lineHeight { return ceil(_size * 1.2); }
- (CGFloat)ascender { return _size * 0.95; }
- (CGFloat)descender { return -_size * 0.25; }
- (CGFloat)capHeight { return _size * 0.7; }
- (CGFloat)xHeight { return _size * 0.52; }
- (CGFloat)leading { return 0; }
- (UIFont *)fontWithSize:(CGFloat)s { UIFont *f = [UIFont fontWithName:_name size:s]; f->_weight = _weight; return f; }
@end

/* ================================================================ UIImage */
@implementation UIImage
+ (UIImage *)imageWithCGImage:(void *)cg { return [[[self alloc] initWithCGImage:cg] autorelease]; }
+ (UIImage *)imageWithCGImage:(void *)cg scale:(CGFloat)s orientation:(NSInteger)o { (void)o; UIImage *i = [self imageWithCGImage:cg]; i->_scale = s; return i; }
+ (UIImage *)imageNamed:(NSString *)n { (void)n; return nil; }
+ (UIImage *)systemImageNamed:(NSString *)n { (void)n; return [[[self alloc] init] autorelease]; }
+ (UIImage *)systemImageNamed:(NSString *)n withConfiguration:(id)c { (void)c; return [self systemImageNamed:n]; }
- (instancetype)initWithCGImage:(void *)cg { if ((self = [super init])) { _cg = cg ? CGImageRetain(cg) : 0; _scale = 1; } return self; }
- (instancetype)initWithData:(NSData *)d { (void)d; [self release]; return nil; }
- (void)dealloc { if (_cg) CGImageRelease(_cg); [super dealloc]; }
- (void *)CGImage { return _cg; }
- (CGSize)size { return _cg ? CGSizeMake(CGImageGetWidth(_cg) / _scale, CGImageGetHeight(_cg) / _scale) : CGSizeMake(0, 0); }
- (CGFloat)scale { return _scale; }
- (UIImage *)imageWithRenderingMode:(NSInteger)m { (void)m; return self; }
- (UIImage *)imageWithTintColor:(UIColor *)c { (void)c; return self; }
- (UIImage *)imageWithConfiguration:(id)c { (void)c; return self; }
- (void)drawInRect:(CGRect)r { CGContextRef c = UIGraphicsGetCurrentContext(); if (c && _cg) CGContextDrawImage(c, r, _cg); }
- (void)drawAtPoint:(CGPoint)p { CGSize s = [self size]; [self drawInRect:CGRectMake(p.x, p.y, s.width, s.height)]; }
@end

/* ================================================================ UIScreen, UIDevice, UITraitCollection */
@implementation UIScreen
+ (UIScreen *)mainScreen { static UIScreen *s; if (!s) s = [[UIScreen alloc] init]; return s; }
- (CGRect)bounds { return CGRectMake(0, 0, 1280, 800); }
- (CGRect)nativeBounds { return [self bounds]; }
- (CGFloat)scale { return 1; }
- (CGFloat)nativeScale { return 1; }
- (CGFloat)brightness { return 1; }
- (id)traitCollection { return [UITraitCollection currentTraitCollection]; }
@end

@implementation UIDevice
+ (UIDevice *)currentDevice { static UIDevice *d; if (!d) d = [[UIDevice alloc] init]; return d; }
- (NSString *)systemName { return @"macOS"; }
- (NSString *)systemVersion { return @"26.0"; }
- (NSString *)model { return @"Mac"; }
- (NSString *)localizedModel { return @"Mac"; }
- (NSString *)name { return @"mOS"; }
- (NSInteger)userInterfaceIdiom { return 5; }             /* UIUserInterfaceIdiomMac (Catalyst "Optimized for Mac") */
- (NSInteger)orientation { return 0; }
- (BOOL)isBatteryMonitoringEnabled { return NO; }
- (void)setBatteryMonitoringEnabled:(BOOL)b { (void)b; }
- (float)batteryLevel { return -1; }
- (NSInteger)batteryState { return 0; }
- (void)beginGeneratingDeviceOrientationNotifications {}
- (void)endGeneratingDeviceOrientationNotifications {}
- (BOOL)isMultitaskingSupported { return YES; }
- (NSUUID *)identifierForVendor { static NSUUID *u; if (!u) u = [[NSUUID alloc] init]; return u; }
@end

@implementation UITraitCollection
+ (UITraitCollection *)currentTraitCollection { static UITraitCollection *t; if (!t) t = [[UITraitCollection alloc] init]; return t; }
+ (void)setCurrentTraitCollection:(UITraitCollection *)t { (void)t; }
- (NSInteger)userInterfaceStyle { return 1; }               /* claro */
- (NSInteger)userInterfaceIdiom { return 5; }
- (CGFloat)displayScale { return 1; }
- (NSInteger)horizontalSizeClass { return 2; }               /* regular */
- (NSInteger)verticalSizeClass { return 2; }
- (NSString *)preferredContentSizeCategory { return @"UICTContentSizeCategoryL"; }
- (NSInteger)layoutDirection { return 0; }
- (BOOL)hasDifferentColorAppearanceComparedToTraitCollection:(id)o { (void)o; return NO; }
- (void)performAsCurrentTraitCollection:(void (^)(void))b { if (b) b(); }
@end

/* ================================================================ UIView */
@implementation UIView
+ (Class)layerClass { return NSClassFromString(@"CALayer"); }
- (instancetype)init { return [self initWithFrame:CGRectMake(0, 0, 0, 0)]; }
- (instancetype)initWithFrame:(CGRect)frame {
    if ((self = [super init])) {
        _frame = frame; _subviews = [[NSMutableArray alloc] init]; _alpha = 1; _userInteraction = YES; _tamic = YES;
    }
    return self;
}
- (instancetype)initWithCoder:(NSCoder *)c { (void)c; return [self initWithFrame:CGRectMake(0, 0, 0, 0)]; }
- (void)dealloc { [_subviews release]; [_backgroundColor release]; [_tintColor release]; [_layer release]; [super dealloc]; }
- (UIResponder *)nextResponder { return _superview; }
- (CGRect)frame { return _frame; }
- (void)setFrame:(CGRect)f { _frame = f; [self setNeedsLayout]; [self setNeedsDisplay]; }
- (CGRect)bounds { return CGRectMake(_boundsOrigin.x, _boundsOrigin.y, _frame.size.width, _frame.size.height); }
- (void)setBounds:(CGRect)b { _boundsOrigin = b.origin; _frame.size = b.size; [self setNeedsLayout]; [self setNeedsDisplay]; }
- (CGPoint)center { return CGPointMake(_frame.origin.x + _frame.size.width / 2, _frame.origin.y + _frame.size.height / 2); }
- (void)setCenter:(CGPoint)c { _frame.origin = CGPointMake(c.x - _frame.size.width / 2, c.y - _frame.size.height / 2); [self setNeedsDisplay]; }
- (UIView *)superview { return _superview; }
- (NSArray *)subviews { return [[_subviews copy] autorelease]; }
- (UIWindow *)window { UIView *v = self; while (v->_superview) v = v->_superview; return [v isKindOfClass:[UIWindow class]] ? (UIWindow *)v : nil; }
- (void)willMoveToSuperview:(UIView *)s { (void)s; }
- (void)didMoveToSuperview {}
- (void)willMoveToWindow:(UIWindow *)w { (void)w; }
- (void)didMoveToWindow {}
- (void)didAddSubview:(UIView *)v { (void)v; }
- (void)willRemoveSubview:(UIView *)v { (void)v; }
- (void)insertSubview:(UIView *)v atIndex:(NSInteger)i {
    if (!v || v == self) return;
    [v retain];
    [v removeFromSuperview];
    [v willMoveToSuperview:self];
    if (i < 0 || (NSUInteger)i > _subviews.count) i = (NSInteger)_subviews.count;
    [_subviews insertObject:v atIndex:(NSUInteger)i];
    v->_superview = self;
    [v release];
    [v didMoveToSuperview];
    [v didMoveToWindow];
    [self didAddSubview:v];
    [self setNeedsLayout];
    [self setNeedsDisplay];
}
- (void)addSubview:(UIView *)v { [self insertSubview:v atIndex:-1]; }
- (void)insertSubview:(UIView *)v belowSubview:(UIView *)s { NSUInteger i = [_subviews indexOfObject:s]; [self insertSubview:v atIndex:i == NSNotFound ? 0 : (NSInteger)i]; }
- (void)insertSubview:(UIView *)v aboveSubview:(UIView *)s { NSUInteger i = [_subviews indexOfObject:s]; [self insertSubview:v atIndex:i == NSNotFound ? -1 : (NSInteger)i + 1]; }
- (void)removeFromSuperview {
    UIView *s = _superview;
    if (!s) return;
    [s willRemoveSubview:self];
    [self willMoveToSuperview:nil];
    _superview = nil;
    mos_remove_identical(s->_subviews, self);
    [self didMoveToSuperview];
    [s setNeedsDisplay];
}
- (void)bringSubviewToFront:(UIView *)v { if ([_subviews containsObject:v]) { [v retain]; mos_remove_identical(_subviews, v); [_subviews addObject:v]; [v release]; [self setNeedsDisplay]; } }
- (void)sendSubviewToBack:(UIView *)v { if ([_subviews containsObject:v]) { [v retain]; mos_remove_identical(_subviews, v); [_subviews insertObject:v atIndex:0]; [v release]; [self setNeedsDisplay]; } }
- (BOOL)isDescendantOfView:(UIView *)v { for (UIView *x = self; x; x = x->_superview) if (x == v) return YES; return NO; }
- (UIView *)viewWithTag:(NSInteger)t { if (_tag == t) return self; for (UIView *s in _subviews) { UIView *r = [s viewWithTag:t]; if (r) return r; } return nil; }
- (UIColor *)backgroundColor { return _backgroundColor; }
- (void)setBackgroundColor:(UIColor *)c { [_backgroundColor release]; _backgroundColor = [c retain]; [self setNeedsDisplay]; }
- (UIColor *)tintColor { return _tintColor ? _tintColor : (_superview ? [_superview tintColor] : [UIColor systemBlueColor]); }
- (void)setTintColor:(UIColor *)c { [_tintColor release]; _tintColor = [c retain]; [self setNeedsDisplay]; }
- (BOOL)isHidden { return _hidden; }
- (void)setHidden:(BOOL)h { _hidden = h; [self setNeedsDisplay]; }
- (CGFloat)alpha { return _alpha; }
- (void)setAlpha:(CGFloat)a { _alpha = a; [self setNeedsDisplay]; }
- (BOOL)isOpaque { return NO; }
- (void)setOpaque:(BOOL)o { (void)o; }
- (NSInteger)tag { return _tag; }
- (void)setTag:(NSInteger)t { _tag = t; }
- (BOOL)isUserInteractionEnabled { return _userInteraction; }
- (void)setUserInteractionEnabled:(BOOL)e { _userInteraction = e; }
- (BOOL)clipsToBounds { return _clipsToBounds; }
- (void)setClipsToBounds:(BOOL)c { _clipsToBounds = c; }
- (NSInteger)contentMode { return _contentMode; }
- (void)setContentMode:(NSInteger)m { _contentMode = m; }
- (NSUInteger)autoresizingMask { return _autoresizingMask; }
- (void)setAutoresizingMask:(NSUInteger)m { _autoresizingMask = m; }
- (BOOL)translatesAutoresizingMaskIntoConstraints { return _tamic; }
- (void)setTranslatesAutoresizingMaskIntoConstraints:(BOOL)t { _tamic = t; }
- (id)layer { if (!_layer) { Class c = [[self class] layerClass]; _layer = c ? [[c alloc] init] : nil; } return _layer; }
- (id)traitCollection { return [UITraitCollection currentTraitCollection]; }
- (void)setNeedsDisplay { UIWindow *w = [self window]; if (w && w->_nsWindow) [w->_nsWindow.contentView setNeedsDisplay:YES]; }
- (void)setNeedsDisplayInRect:(CGRect)r { (void)r; [self setNeedsDisplay]; }
- (void)setNeedsLayout {}
- (void)layoutIfNeeded { [self layoutSubviews]; for (UIView *s in _subviews) [s layoutIfNeeded]; }
- (void)layoutSubviews {}
- (void)drawRect:(CGRect)r { (void)r; }
- (CGSize)sizeThatFits:(CGSize)s { (void)s; return _frame.size; }
- (void)sizeToFit { CGSize s = [self sizeThatFits:_frame.size]; _frame.size = s; }
- (CGSize)intrinsicContentSize { return CGSizeMake(-1, -1); }
- (CGPoint)convertPoint:(CGPoint)p toView:(UIView *)v {
    for (UIView *x = self; x; x = x->_superview) { if (x == v) return p; p.x += x->_frame.origin.x - x->_boundsOrigin.x; p.y += x->_frame.origin.y - x->_boundsOrigin.y; }
    if (v) { CGPoint o = [v convertPoint:CGPointMake(0, 0) toView:nil]; p.x -= o.x; p.y -= o.y; }
    return p;
}
- (CGPoint)convertPoint:(CGPoint)p fromView:(UIView *)v {
    CGPoint a = v ? [v convertPoint:p toView:nil] : p, b = [self convertPoint:CGPointMake(0, 0) toView:nil];
    return CGPointMake(a.x - b.x, a.y - b.y);
}
- (CGRect)convertRect:(CGRect)r toView:(UIView *)v { r.origin = [self convertPoint:r.origin toView:v]; return r; }
- (CGRect)convertRect:(CGRect)r fromView:(UIView *)v { r.origin = [self convertPoint:r.origin fromView:v]; return r; }
- (UIView *)hitTest:(CGPoint)p withEvent:(id)e {
    if (_hidden || !_userInteraction || _alpha < 0.01 || ![self pointInside:p withEvent:e]) return nil;
    for (NSInteger k = (NSInteger)_subviews.count - 1; k >= 0; k--) {
        UIView *s = [_subviews objectAtIndex:(NSUInteger)k];
        UIView *h = [s hitTest:[self convertPoint:p toView:s] withEvent:e];
        if (h) return h;
    }
    return self;
}
- (BOOL)pointInside:(CGPoint)p withEvent:(id)e { (void)e; CGRect b = [self bounds]; return p.x >= b.origin.x && p.y >= b.origin.y && p.x < b.origin.x + b.size.width && p.y < b.origin.y + b.size.height; }
+ (void)animateWithDuration:(NSTimeInterval)d animations:(void (^)(void))a { (void)d; if (a) a(); }
+ (void)animateWithDuration:(NSTimeInterval)d animations:(void (^)(void))a completion:(void (^)(BOOL))c { (void)d; if (a) a(); if (c) c(YES); }
+ (void)animateWithDuration:(NSTimeInterval)d delay:(NSTimeInterval)dl options:(NSUInteger)o animations:(void (^)(void))a completion:(void (^)(BOOL))c {
    (void)d; (void)dl; (void)o; if (a) a(); if (c) c(YES);
}
+ (void)performWithoutAnimation:(void (^)(void))a { if (a) a(); }
+ (BOOL)areAnimationsEnabled { return NO; }

/* desenho da árvore (chamado pela NSView hospedeira com o contexto já transladado para a origem desta view) */
- (void)_mosRender:(CGContextRef)c {
    if (_hidden || _alpha < 0.01) return;
    CGRect b = [self bounds];
    CGContextSaveGState(c);
    /* alpha parcial: o CG do libSystem ainda não tem CGContextSetAlpha; a view desenha opaca */
    if (_clipsToBounds) CGContextClipToRect(c, CGRectMake(0, 0, b.size.width, b.size.height));
    if (_backgroundColor && _backgroundColor->_a > 0) {
        CGContextSetFillColorWithColor(c, [_backgroundColor CGColor]);
        CGContextFillRect(c, CGRectMake(0, 0, b.size.width, b.size.height));
    }
    UIGraphicsPushContext(c);
    [self drawRect:CGRectMake(b.origin.x, b.origin.y, b.size.width, b.size.height)];
    UIGraphicsPopContext();
    for (UIView *s in _subviews) {
        CGContextSaveGState(c);
        CGContextTranslateCTM(c, s->_frame.origin.x - b.origin.x, s->_frame.origin.y - b.origin.y);
        [s _mosRender:c];
        CGContextRestoreGState(c);
    }
    CGContextRestoreGState(c);
}
@end

/* ================================================================ UILabel e UIControl */
/* um alvo+ação ou um UIAction registrado num controle */
@interface _UIControlHandler : NSObject {
@public
    id _target;                 /* não retido, como no UIKit */
    SEL _sel;
    UIAction *_action;
    NSUInteger _events;
}
@end
@implementation _UIControlHandler
- (void)dealloc { [_action release]; [super dealloc]; }
@end

@implementation UIControl
- (void)dealloc { [_handlers release]; [super dealloc]; }
- (BOOL)isEnabled { return !_disabled; }
- (void)setEnabled:(BOOL)e { _disabled = !e; [self setNeedsDisplay]; }
- (BOOL)isSelected { return _selected; }
- (void)setSelected:(BOOL)s { _selected = s; [self setNeedsDisplay]; }
- (BOOL)isHighlighted { return _highlighted; }
- (void)setHighlighted:(BOOL)h { _highlighted = h; [self setNeedsDisplay]; }
- (_UIControlHandler *)_mosAdd:(NSUInteger)events {
    if (!_handlers) _handlers = [[NSMutableArray alloc] init];
    _UIControlHandler *h = [[_UIControlHandler alloc] init];
    h->_events = events;
    [_handlers addObject:h];
    [h release];
    return h;
}
- (void)addTarget:(id)t action:(SEL)a forControlEvents:(NSUInteger)e { _UIControlHandler *h = [self _mosAdd:e]; h->_target = t; h->_sel = a; }
- (void)addAction:(UIAction *)a forControlEvents:(NSUInteger)e { _UIControlHandler *h = [self _mosAdd:e]; h->_action = [a retain]; }
- (void)removeTarget:(id)t action:(SEL)a forControlEvents:(NSUInteger)e {
    for (NSInteger i = (NSInteger)_handlers.count - 1; i >= 0; i--) {
        _UIControlHandler *h = [_handlers objectAtIndex:(NSUInteger)i];
        if ((!t || h->_target == t) && (!a || h->_sel == a) && (h->_events & e)) [_handlers removeObjectAtIndex:(NSUInteger)i];
    }
}
- (void)removeAction:(UIAction *)a forControlEvents:(NSUInteger)e {
    for (NSInteger i = (NSInteger)_handlers.count - 1; i >= 0; i--) {
        _UIControlHandler *h = [_handlers objectAtIndex:(NSUInteger)i];
        if (h->_action == a && (h->_events & e)) [_handlers removeObjectAtIndex:(NSUInteger)i];
    }
}
- (NSUInteger)allControlEvents { NSUInteger m = 0; for (_UIControlHandler *h in _handlers) m |= h->_events; return m; }
- (NSSet *)allTargets { NSMutableSet *s = [NSMutableSet set]; for (_UIControlHandler *h in _handlers) if (h->_target) [s addObject:h->_target]; return s; }
/* dispara os alvos e ações dos eventos (o toque ainda não chega aqui sozinho: falta ligar os eventos do AppKit) */
- (void)sendActionsForControlEvents:(NSUInteger)e {
    NSArray *hs = [[_handlers copy] autorelease];
    for (_UIControlHandler *h in hs) {
        if (!(h->_events & e)) continue;
        if (h->_action) { h->_action->_sender = self; [h->_action _mosPerform]; h->_action->_sender = nil; }
        else if (h->_target && h->_sel && [h->_target respondsToSelector:h->_sel]) [h->_target performSelector:h->_sel withObject:self];
    }
}
- (void)sendAction:(SEL)a to:(id)t forEvent:(id)ev { (void)ev; if (t && [t respondsToSelector:a]) [t performSelector:a withObject:self]; }
- (NSUInteger)_mosHandlerCount { return _handlers.count; }
- (id)_mosHandlerTarget:(NSUInteger)i { return ((_UIControlHandler *)[_handlers objectAtIndex:i])->_target; }
- (SEL)_mosHandlerSelector:(NSUInteger)i { return ((_UIControlHandler *)[_handlers objectAtIndex:i])->_sel; }
- (UIAction *)_mosHandlerAction:(NSUInteger)i { return ((_UIControlHandler *)[_handlers objectAtIndex:i])->_action; }
- (NSUInteger)_mosHandlerEvents:(NSUInteger)i { return ((_UIControlHandler *)[_handlers objectAtIndex:i])->_events; }
@end

@implementation UILabel
- (instancetype)initWithFrame:(CGRect)f { if ((self = [super initWithFrame:f])) { _lines = 1; } return self; }
- (void)dealloc { [_text release]; [_font release]; [_textColor release]; [super dealloc]; }
- (NSString *)text { return _text; }
- (void)setText:(NSString *)t { [_text release]; _text = [t copy]; [self setNeedsDisplay]; }
- (UIFont *)font { return _font ? _font : [UIFont systemFontOfSize:13]; }
- (void)setFont:(UIFont *)f { [_font release]; _font = [f retain]; [self setNeedsDisplay]; }
- (UIColor *)textColor { return _textColor ? _textColor : [UIColor labelColor]; }
- (void)setTextColor:(UIColor *)c { [_textColor release]; _textColor = [c retain]; [self setNeedsDisplay]; }
- (NSInteger)textAlignment { return _alignment; }
- (void)setTextAlignment:(NSInteger)a { _alignment = a; [self setNeedsDisplay]; }
- (NSInteger)numberOfLines { return _lines; }
- (void)setNumberOfLines:(NSInteger)n { _lines = n; }
- (NSAttributedString *)attributedText { return _text ? [[[NSAttributedString alloc] initWithString:_text] autorelease] : nil; }
- (void)setAttributedText:(NSAttributedString *)a { [self setText:a.string]; }
- (CGSize)sizeThatFits:(CGSize)s { (void)s; CGFloat p = [[self font] pointSize]; return CGSizeMake(ceil(_text.length * p * 0.55), ceil(p * 1.2)); }
- (CGSize)intrinsicContentSize { return [self sizeThatFits:CGSizeMake(0, 0)]; }
- (void)drawRect:(CGRect)r {
    (void)r;
    if (!_text.length) return;
    UIFont *f = [self font];
    NSFont *nf = [NSFont systemFontOfSize:f->_size];
    UIColor *tc = [self textColor];
    NSColor *nc = [NSColor colorWithRed:tc->_r green:tc->_g blue:tc->_b alpha:tc->_a];
    NSDictionary *attrs = [NSDictionary dictionaryWithObjectsAndKeys:nf, NSFontAttributeName, nc, NSForegroundColorAttributeName, nil];
    CGSize s = [self sizeThatFits:CGSizeMake(0, 0)];
    CGFloat w = _frame.size.width, x = _alignment == 1 ? (w - s.width) / 2 : _alignment == 2 ? w - s.width : 0;
    [_text drawAtPoint:NSMakePoint(x, (_frame.size.height - s.height) / 2) withAttributes:attrs];
}
@end

/* ================================================================ NSView hospedeira: desenha a árvore de uma UIWindow */
@interface _UIHostingView : NSView {
@public
    UIWindow *_uiWindow;
}
@end
@implementation _UIHostingView
- (BOOL)isFlipped { return YES; }
- (void)drawRect:(NSRect)dirty {
    (void)dirty;
    CGContextRef c = (CGContextRef)[NSGraphicsContext currentContext].CGContext;
    if (!c || !_uiWindow) return;
    [_uiWindow layoutIfNeeded];
    CGContextSaveGState(c);
    [_uiWindow _mosRender:c];
    CGContextRestoreGState(c);
}
@end

/* ================================================================ UIWindow */
@implementation UIWindow
- (instancetype)initWithFrame:(CGRect)f {
    if ((self = [super initWithFrame:f])) _backgroundColor = [[UIColor systemBackgroundColor] retain];
    return self;
}
- (instancetype)initWithWindowScene:(UIWindowScene *)scene {
    if ((self = [self initWithFrame:CGRectMake(0, 0, 900, 640)])) [self setWindowScene:scene];
    return self;
}
- (UIWindowScene *)windowScene { return _windowScene; }
- (void)setWindowScene:(UIWindowScene *)s {
    if (_windowScene) mos_remove_identical(_windowScene->_windows, self);
    _windowScene = s;
    if (s) { if (!s->_windows) s->_windows = [[NSMutableArray alloc] init]; [s->_windows addObject:self]; }
}
- (UIWindow *)window { return self; }
- (UIViewController *)rootViewController { return _rootViewController; }
- (void)setRootViewController:(UIViewController *)vc {
    if (_rootViewController) [[_rootViewController view] removeFromSuperview];
    [_rootViewController release];
    _rootViewController = [vc retain];
    if (vc) {
        UIView *v = [vc view];
        v.frame = [self bounds];
        [self addSubview:v];
        [vc viewWillAppear:NO];
        if (_nsWindow) [vc viewDidAppear:NO];
    }
}
- (BOOL)isKeyWindow { return _key; }
- (void)makeKeyWindow { _key = YES; [_nsWindow makeKeyAndOrderFront:nil]; }
- (void)makeKeyAndVisible {
    if (!_nsWindow) {
        NSRect r = NSMakeRect(80, 80, _frame.size.width > 0 ? _frame.size.width : 900, _frame.size.height > 0 ? _frame.size.height : 640);
        _nsWindow = [[NSWindow alloc] initWithContentRect:r styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                                                         NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable)
                                                  backing:NSBackingStoreBuffered defer:NO];
        NSString *t = _windowScene ? _windowScene->_title : nil;
        if (!t.length) t = [[NSProcessInfo processInfo] processName];
        _nsWindow.title = t ? t : @"";
        _UIHostingView *h = [[_UIHostingView alloc] initWithFrame:NSMakeRect(0, 0, r.size.width, r.size.height)];
        h->_uiWindow = self;
        _nsWindow.contentView = h;
        [h release];
        _frame = CGRectMake(0, 0, r.size.width, r.size.height);
        if (_rootViewController) { [_rootViewController view].frame = [self bounds]; [_rootViewController viewDidAppear:NO]; }
    }
    _hidden = NO;
    _key = YES;
    [_nsWindow makeKeyAndOrderFront:nil];
    [[NSNotificationCenter defaultCenter] postNotificationName:UIWindowDidBecomeVisibleNotification object:self];
    [[NSNotificationCenter defaultCenter] postNotificationName:UIWindowDidBecomeKeyNotification object:self];
}
- (void)setNeedsDisplay { if (_nsWindow) [_nsWindow.contentView setNeedsDisplay:YES]; }
- (void)layoutSubviews { if (_rootViewController) [_rootViewController view].frame = [self bounds]; }
@end

/* ================================================================ UIViewController */
@implementation UIViewController
- (instancetype)initWithNibName:(NSString *)n bundle:(NSBundle *)b { (void)n; (void)b; return [self init]; }
- (instancetype)initWithCoder:(NSCoder *)c { (void)c; return [self init]; }
- (void)dealloc { [_view release]; [_title release]; [_children release]; [super dealloc]; }
- (UIResponder *)nextResponder { return _view ? _view->_superview : nil; }
- (BOOL)isViewLoaded { return _view != nil; }
- (void)loadView { _view = [[UIView alloc] initWithFrame:[[UIScreen mainScreen] bounds]]; _view.backgroundColor = [UIColor systemBackgroundColor]; }
- (UIView *)view { if (!_view) { [self loadView]; [self viewDidLoad]; } return _view; }
- (void)setView:(UIView *)v { [_view release]; _view = [v retain]; }
- (UIView *)viewIfLoaded { return _view; }
- (void)loadViewIfNeeded { (void)[self view]; }
- (void)viewDidLoad {}
- (void)viewWillAppear:(BOOL)a { (void)a; }
- (void)viewDidAppear:(BOOL)a { (void)a; }
- (void)viewWillDisappear:(BOOL)a { (void)a; }
- (void)viewDidDisappear:(BOOL)a { (void)a; }
- (void)viewWillLayoutSubviews {}
- (void)viewDidLayoutSubviews {}
- (NSString *)title { return _title; }
- (void)setTitle:(NSString *)t { [_title release]; _title = [t copy]; }
- (UIViewController *)parentViewController { return _parent; }
- (NSArray *)childViewControllers { return _children ? [[_children copy] autorelease] : [NSArray array]; }
- (void)addChildViewController:(UIViewController *)c { if (!_children) _children = [[NSMutableArray alloc] init]; [_children addObject:c]; c->_parent = self; }
- (void)removeFromParentViewController { if (_parent) { mos_remove_identical(_parent->_children, self); _parent = nil; } }
- (void)willMoveToParentViewController:(UIViewController *)p { (void)p; }
- (void)didMoveToParentViewController:(UIViewController *)p { (void)p; }
- (id)traitCollection { return [UITraitCollection currentTraitCollection]; }
- (void)setNeedsStatusBarAppearanceUpdate {}
- (void)presentViewController:(UIViewController *)vc animated:(BOOL)a completion:(void (^)(void))c { (void)vc; (void)a; if (c) c(); }
- (void)dismissViewControllerAnimated:(BOOL)a completion:(void (^)(void))c { (void)a; if (c) c(); }
@end

/* ================================================================ cenas */
@implementation UISceneSession
- (NSString *)role { return _role; }
- (id)configuration { return _configuration; }
- (UIScene *)scene { return _scene; }
- (NSString *)persistentIdentifier { return @"mOS-scene-1"; }
- (NSDictionary *)userInfo { return nil; }
@end

@implementation UISceneConfiguration
+ (instancetype)configurationWithName:(NSString *)n sessionRole:(NSString *)r { return [[[self alloc] initWithName:n sessionRole:r] autorelease]; }
- (instancetype)initWithName:(NSString *)n sessionRole:(NSString *)r { if ((self = [super init])) { _name = [n copy]; _role = [r copy]; } return self; }
- (NSString *)name { return _name; }
- (NSString *)role { return _role; }
- (Class)delegateClass { return _delegateClass; }
- (void)setDelegateClass:(Class)c { _delegateClass = c; }
- (Class)sceneClass { return _sceneClass; }
- (void)setSceneClass:(Class)c { _sceneClass = c; }
- (id)copyWithZone:(NSZone *)z { (void)z; return [self retain]; }
@end

@implementation UIScene
- (id)delegate { return _delegate; }
- (void)setDelegate:(id)d { [_delegate release]; _delegate = [d retain]; }
- (UISceneSession *)session { return _session; }
- (NSString *)title { return _title; }
- (void)setTitle:(NSString *)t { [_title release]; _title = [t copy]; }
- (NSInteger)activationState { return 0; }                   /* UISceneActivationStateForegroundActive */
- (instancetype)initWithSession:(UISceneSession *)s connectionOptions:(id)o { (void)o; if ((self = [super init])) _session = [s retain]; return self; }
@end

@implementation UIWindowScene
- (NSArray *)windows { return _windows ? [[_windows copy] autorelease] : [NSArray array]; }
- (UIScreen *)screen { return [UIScreen mainScreen]; }
- (id)traitCollection { return [UITraitCollection currentTraitCollection]; }
- (UIWindow *)keyWindow { for (UIWindow *w in _windows) if (w->_key) return w; return _windows.lastObject; }
- (id)sizeRestrictions { return nil; }
- (id)titlebar { return nil; }
- (NSInteger)interfaceOrientation { return 1; }
@end

/* ================================================================ UIApplication e UIApplicationMain */
static UIApplication *g_app;

@implementation UIApplication
+ (void)initialize { _mos_uikit_constants_init(); }
+ (UIApplication *)sharedApplication { return g_app; }
- (id)delegate { return _delegate; }
- (void)setDelegate:(id)d { _delegate = [d retain]; }
- (NSSet *)connectedScenes { return _scenes ? [[_scenes copy] autorelease] : [NSSet set]; }
- (NSSet *)openSessions { NSMutableSet *s = [NSMutableSet set]; for (UIScene *sc in _scenes) [s addObject:sc->_session]; return s; }
- (NSArray *)windows { NSMutableArray *a = [NSMutableArray array]; for (UIScene *s in _scenes) if ([s isKindOfClass:[UIWindowScene class]]) [a addObjectsFromArray:[(UIWindowScene *)s windows]]; return a; }
- (UIWindow *)keyWindow { for (UIWindow *w in [self windows]) if (w->_key) return w; return nil; }
- (NSInteger)applicationState { return 0; }                    /* UIApplicationStateActive */
- (BOOL)supportsMultipleScenes { return NO; }
- (NSInteger)applicationIconBadgeNumber { return 0; }
- (void)setApplicationIconBadgeNumber:(NSInteger)n { (void)n; }
- (BOOL)isIdleTimerDisabled { return NO; }
- (void)setIdleTimerDisabled:(BOOL)d { (void)d; }
- (NSInteger)userInterfaceLayoutDirection { return 0; }
- (NSString *)preferredContentSizeCategory { return @"UICTContentSizeCategoryL"; }
- (NSUInteger)beginBackgroundTaskWithExpirationHandler:(void (^)(void))h { (void)h; return 1; }
- (NSUInteger)beginBackgroundTaskWithName:(NSString *)n expirationHandler:(void (^)(void))h { (void)n; (void)h; return 1; }
- (void)endBackgroundTask:(NSUInteger)t { (void)t; }
- (void)registerForRemoteNotifications {}
- (void)unregisterForRemoteNotifications {}
- (BOOL)isRegisteredForRemoteNotifications { return NO; }
- (BOOL)canOpenURL:(NSURL *)u { (void)u; return NO; }
- (void)openURL:(NSURL *)u options:(NSDictionary *)o completionHandler:(void (^)(BOOL))c { (void)u; (void)o; if (c) c(NO); }
- (BOOL)sendAction:(SEL)a to:(id)t from:(id)s forEvent:(id)e { (void)e; if (t && [t respondsToSelector:a]) { [t performSelector:a withObject:s]; return YES; } return NO; }
- (void)requestSceneSessionActivation:(UISceneSession *)s userActivity:(id)u options:(id)o errorHandler:(void (^)(NSError *))h { (void)s; (void)u; (void)o; (void)h; }
- (void)setShortcutItems:(NSArray *)i { (void)i; }
- (NSArray *)shortcutItems { return nil; }
- (BOOL)isProtectedDataAvailable { return YES; }
- (NSInteger)backgroundRefreshStatus { return 2; }

/* liga uma cena: a configuração vem do delegate do app (ou do UIApplicationSceneManifest do Info.plist) */
- (void)_mosConnectScene {
    UISceneSession *session = [[UISceneSession alloc] init];
    session->_role = [UIWindowSceneSessionRoleApplication copy];
    UISceneConfiguration *cfg = nil;
    if ([_delegate respondsToSelector:@selector(application:configurationForConnectingSceneSession:options:)])
        cfg = [_delegate application:self configurationForConnectingSceneSession:session options:nil];
    if (!cfg) {
        NSDictionary *man = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"UIApplicationSceneManifest"];
        NSArray *roles = [[man objectForKey:@"UISceneConfigurations"] objectForKey:UIWindowSceneSessionRoleApplication];
        NSDictionary *c0 = roles.count ? [roles objectAtIndex:0] : nil;
        if (c0) {
            cfg = [UISceneConfiguration configurationWithName:[c0 objectForKey:@"UISceneConfigurationName"] sessionRole:session->_role];
            NSString *dn = [c0 objectForKey:@"UISceneDelegateClassName"], *sn = [c0 objectForKey:@"UISceneClassName"];
            if (dn) cfg->_delegateClass = NSClassFromString(dn);
            if (sn) cfg->_sceneClass = NSClassFromString(sn);
        }
    }
    if (!cfg || (!cfg->_delegateClass && !cfg->_sceneClass)) { [session release]; return; }   /* app sem cenas */
    session->_configuration = [cfg retain];
    Class sc = cfg->_sceneClass ? cfg->_sceneClass : [UIWindowScene class];
    UIWindowScene *scene = [[sc alloc] initWithSession:session connectionOptions:nil];
    session->_scene = scene;
    if (!_scenes) _scenes = [[NSMutableSet alloc] init];
    [_scenes addObject:scene];
    if (cfg->_delegateClass) {
        id d = [[cfg->_delegateClass alloc] init];
        scene.delegate = d;
        [d release];
    }
    [[NSNotificationCenter defaultCenter] postNotificationName:UISceneWillConnectNotification object:scene];
    id d = scene.delegate;
    if ([d respondsToSelector:@selector(scene:willConnectToSession:options:)]) [d scene:scene willConnectToSession:session options:nil];
    if ([d respondsToSelector:@selector(sceneWillEnterForeground:)]) [d sceneWillEnterForeground:scene];
    if ([d respondsToSelector:@selector(sceneDidBecomeActive:)]) [d sceneDidBecomeActive:scene];
    [[NSNotificationCenter defaultCenter] postNotificationName:UISceneDidActivateNotification object:scene];
}
@end

int UIApplicationMain(int argc, char *argv[], NSString *principalClassName, NSString *delegateClassName) {
    (void)argc; (void)argv;
    _mos_uikit_constants_init();
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    [NSApplication sharedApplication];                  /* o laço de eventos e as janelas são os do AppKit do mOS */
    Class pc = principalClassName ? NSClassFromString(principalClassName) : Nil;
    g_app = [[(pc ? pc : [UIApplication class]) alloc] init];
    if (!delegateClassName) delegateClassName = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"UIApplicationDelegateClassName"];
    Class dc = delegateClassName ? NSClassFromString(delegateClassName) : Nil;
    if (dc) { id d = [[dc alloc] init]; g_app.delegate = d; [d release]; }
    else fprintf(stderr, "UIKit[mOS]: classe do delegate '%s' não encontrada\n", delegateClassName ? [delegateClassName UTF8String] : "(nenhuma)");
    id d = g_app.delegate;
    if ([d respondsToSelector:@selector(application:willFinishLaunchingWithOptions:)]) [d application:g_app willFinishLaunchingWithOptions:nil];
    if ([d respondsToSelector:@selector(application:didFinishLaunchingWithOptions:)]) [d application:g_app didFinishLaunchingWithOptions:nil];
    [[NSNotificationCenter defaultCenter] postNotificationName:UIApplicationDidFinishLaunchingNotification object:g_app];
    [g_app _mosConnectScene];
    if ([d respondsToSelector:@selector(applicationDidBecomeActive:)]) [d applicationDidBecomeActive:g_app];
    [[NSNotificationCenter defaultCenter] postNotificationName:UIApplicationDidBecomeActiveNotification object:g_app];
    [pool drain];
    [NSApp run];
    return 0;
}
