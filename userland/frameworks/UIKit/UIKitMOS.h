/*
 * mOS — UIKit.framework do Mac Catalyst: interfaces internas das classes. Os nomes e a hierarquia são os do SDK da Apple
 * (os apps Catalyst herdam delas: OBJC_CLASS_$_UIView etc.). O comportamento de verdade está em UIKitCore.m
 * (UIApplicationMain, UIApplication, UIScene/UIWindowScene, UIWindow sobre NSWindow, UIView desenhada por uma NSView
 * hospedeira, UIViewController, UIColor, UIFont, UIImage, UIScreen, UIDevice); as demais classes (UIKitClasses.m)
 * existem com a hierarquia certa e os métodos herdados, e vão ganhando implementação aos poucos.
 */
#import <AppKit/AppKit.h>

typedef struct { CGFloat top, left, bottom, right; } UIEdgeInsets;

/* ---- raiz da cadeia de resposta */
@interface UIResponder : NSObject
- (UIResponder *)nextResponder;
- (BOOL)becomeFirstResponder;
- (BOOL)resignFirstResponder;
- (BOOL)canBecomeFirstResponder;
- (BOOL)isFirstResponder;
@end

@class UIView, UIWindow, UIViewController, UIScene, UIWindowScene, UISceneSession, UIColor, UIFont, UIImage, UIScreen;

@interface UIColor : NSObject <NSCopying> {
@public
    CGFloat _r, _g, _b, _a;
    void *_cg;
}
+ (UIColor *)colorWithRed:(CGFloat)r green:(CGFloat)g blue:(CGFloat)b alpha:(CGFloat)a;
+ (UIColor *)colorWithWhite:(CGFloat)w alpha:(CGFloat)a;
- (instancetype)initWithRed:(CGFloat)r green:(CGFloat)g blue:(CGFloat)b alpha:(CGFloat)a;
- (void *)CGColor;
- (void)setFill;
- (void)set;
@end

@interface UIFont : NSObject <NSCopying> {
@public
    NSString *_name;
    CGFloat _size, _weight;
}
+ (UIFont *)systemFontOfSize:(CGFloat)s;
+ (UIFont *)systemFontOfSize:(CGFloat)s weight:(CGFloat)w;
+ (UIFont *)boldSystemFontOfSize:(CGFloat)s;
+ (UIFont *)fontWithName:(NSString *)n size:(CGFloat)s;
+ (UIFont *)preferredFontForTextStyle:(NSString *)style;
- (CGFloat)pointSize;
- (NSString *)fontName;
- (NSString *)familyName;
- (UIFont *)fontWithSize:(CGFloat)s;
@end

@interface UIImage : NSObject {
@public
    void *_cg;          /* CGImageRef */
    CGFloat _scale;
}
+ (UIImage *)imageWithCGImage:(void *)cg;
+ (UIImage *)imageNamed:(NSString *)n;
+ (UIImage *)systemImageNamed:(NSString *)n;
- (instancetype)initWithCGImage:(void *)cg;
- (instancetype)initWithData:(NSData *)d;
- (void *)CGImage;
- (CGSize)size;
- (CGFloat)scale;
@end

@interface UIScreen : NSObject
+ (UIScreen *)mainScreen;
- (CGRect)bounds;
- (CGFloat)scale;
- (CGFloat)nativeScale;
@end

@interface UIDevice : NSObject
+ (UIDevice *)currentDevice;
@end

@interface UITraitCollection : NSObject
+ (UITraitCollection *)currentTraitCollection;
@end

@interface UIView : UIResponder {
@public
    CGRect _frame;
    CGPoint _boundsOrigin;
    UIView *_superview;
    NSMutableArray *_subviews;
    UIColor *_backgroundColor;
    BOOL _hidden, _userInteraction, _clipsToBounds, _tamic;
    CGFloat _alpha;
    NSInteger _tag;
    id _layer;
    UIColor *_tintColor;
    NSInteger _contentMode;
    NSUInteger _autoresizingMask;
}
- (instancetype)initWithFrame:(CGRect)frame;
- (CGRect)frame;
- (void)setFrame:(CGRect)f;
- (CGRect)bounds;
- (void)setBounds:(CGRect)b;
- (CGPoint)center;
- (void)setCenter:(CGPoint)c;
- (UIView *)superview;
- (NSArray *)subviews;
- (UIWindow *)window;
- (void)addSubview:(UIView *)v;
- (void)insertSubview:(UIView *)v atIndex:(NSInteger)i;
- (void)removeFromSuperview;
- (void)bringSubviewToFront:(UIView *)v;
- (void)sendSubviewToBack:(UIView *)v;
- (UIColor *)backgroundColor;
- (void)setBackgroundColor:(UIColor *)c;
- (BOOL)isHidden;
- (void)setHidden:(BOOL)h;
- (CGFloat)alpha;
- (void)setAlpha:(CGFloat)a;
- (void)setNeedsDisplay;
- (void)setNeedsLayout;
- (void)layoutIfNeeded;
- (void)layoutSubviews;
- (void)drawRect:(CGRect)r;
- (CGSize)sizeThatFits:(CGSize)s;
- (void)sizeToFit;
- (CGSize)intrinsicContentSize;
@end

@interface UIWindow : UIView {
@public
    NSWindow *_nsWindow;
    UIViewController *_rootViewController;
    UIWindowScene *_windowScene;
    BOOL _key;
}
- (instancetype)initWithWindowScene:(UIWindowScene *)scene;
- (void)makeKeyAndVisible;
- (UIViewController *)rootViewController;
- (void)setRootViewController:(UIViewController *)vc;
@end

@interface UIControl : UIView
@end

@interface UILabel : UIView {
@public
    NSString *_text;
    UIFont *_font;
    UIColor *_textColor;
    NSInteger _alignment, _lines;
}
@end

@interface UIViewController : UIResponder {
@public
    UIView *_view;
    NSString *_title;
    UIViewController *_parent;
    NSMutableArray *_children;
}
- (UIView *)view;
- (void)setView:(UIView *)v;
- (BOOL)isViewLoaded;
- (void)loadView;
- (void)viewDidLoad;
- (void)viewWillAppear:(BOOL)a;
- (void)viewDidAppear:(BOOL)a;
@end

@interface UIApplication : UIResponder {
@public
    id _delegate;
    NSMutableSet *_scenes;
}
+ (UIApplication *)sharedApplication;
- (id)delegate;
- (void)setDelegate:(id)d;
@end

@interface UISceneSession : NSObject {
@public
    NSString *_role;
    id _configuration;
    UIScene *_scene;
}
@end

@interface UISceneConfiguration : NSObject {
@public
    NSString *_name, *_role;
    Class _delegateClass, _sceneClass;
}
- (instancetype)initWithName:(NSString *)n sessionRole:(NSString *)r;
@end

@interface UIScene : UIResponder {
@public
    UISceneSession *_session;
    id _delegate;
    NSString *_title;
}
@end

@interface UIWindowScene : UIScene {
@public
    NSMutableArray *_windows;
}
@end
