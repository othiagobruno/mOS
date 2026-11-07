/*
 * mOS — UIKit.framework do Mac Catalyst: menus, ações e comandos (UIMenuElement, UIAction, UIMenu, UICommand,
 * UIKeyCommand), itens de barra (UIBarItem, UIBarButtonItem) e UIButton. O overlay Swift (libswiftUIKit) monta estes
 * objetos pelos inits Swift da Apple. Ainda não há barra de menus nem desenho de botão: os objetos guardam o estado e
 * o UIAction executa o seu bloco quando o controle dispara (-[UIControl sendActionsForControlEvents:]).
 */
#import "UIKitMOS.h"
#import <objc/runtime.h>

@implementation UIMenuElement
- (void)dealloc { [_title release]; [_subtitle release]; [_image release]; [super dealloc]; }
- (id)copyWithZone:(NSZone *)z { (void)z; return [self retain]; }
- (NSString *)title { return _title ? _title : @""; }
- (void)setTitle:(NSString *)t { [_title release]; _title = [t copy]; }
- (NSString *)subtitle { return _subtitle; }
- (void)setSubtitle:(NSString *)t { [_subtitle release]; _subtitle = [t copy]; }
- (UIImage *)image { return _image; }
- (void)setImage:(UIImage *)i { [_image release]; _image = [i retain]; }
@end

@implementation UIAction
+ (instancetype)actionWithHandler:(void (^)(UIAction *))h { UIAction *a = [[[self alloc] init] autorelease]; [a _mosSetHandler:h]; return a; }
+ (instancetype)actionWithTitle:(NSString *)t image:(UIImage *)i identifier:(NSString *)ident handler:(void (^)(UIAction *))h {
    UIAction *a = [self actionWithHandler:h];
    a.title = t; a.image = i; if (ident) a.identifier = ident;
    return a;
}
- (void)dealloc { [_identifier release]; [_discoverabilityTitle release]; [_handler release]; [super dealloc]; }
- (NSString *)identifier { if (!_identifier) _identifier = [[NSString alloc] initWithFormat:@"com.apple.action.dynamic.%p", self]; return _identifier; }
- (void)setIdentifier:(NSString *)i { [_identifier release]; _identifier = [i copy]; }
- (NSString *)discoverabilityTitle { return _discoverabilityTitle; }
- (void)setDiscoverabilityTitle:(NSString *)t { [_discoverabilityTitle release]; _discoverabilityTitle = [t copy]; }
- (NSUInteger)attributes { return _attributes; }
- (void)setAttributes:(NSUInteger)a { _attributes = a; }
- (NSInteger)state { return _state; }
- (void)setState:(NSInteger)s { _state = s; }
- (id)sender { return _sender; }
- (void)_mosSetHandler:(void (^)(UIAction *))h { [_handler release]; _handler = [h copy]; }
- (void)_mosPerform { if (_handler && !(_attributes & 1)) _handler(self); }
@end

@implementation UIMenu
+ (instancetype)menuWithTitle:(NSString *)t children:(NSArray *)c { UIMenu *m = [[[self alloc] init] autorelease]; m.title = t; m.children = c; return m; }
+ (instancetype)menuWithTitle:(NSString *)t image:(UIImage *)i identifier:(NSString *)ident options:(NSUInteger)o children:(NSArray *)c {
    UIMenu *m = [self menuWithTitle:t children:c];
    m.image = i; if (ident) m.identifier = ident; m.options = o;
    return m;
}
- (void)dealloc { [_identifier release]; [_children release]; [super dealloc]; }
- (NSString *)identifier { if (!_identifier) _identifier = [[NSString alloc] initWithFormat:@"com.apple.menu.dynamic.%p", self]; return _identifier; }
- (void)setIdentifier:(NSString *)i { [_identifier release]; _identifier = [i copy]; }
- (NSUInteger)options { return _options; }
- (void)setOptions:(NSUInteger)o { _options = o; }
- (NSArray *)children { return _children ? _children : [NSArray array]; }
- (void)setChildren:(NSArray *)c { [_children release]; _children = [c copy]; }
- (UIMenu *)menuByReplacingChildren:(NSArray *)c { UIMenu *m = [UIMenu menuWithTitle:self.title image:self.image identifier:self.identifier options:_options children:c]; return m; }
@end

@implementation UICommandAlternate
@end

@implementation UICommand
- (void)dealloc { [_propertyList release]; [_alternates release]; [_discoverabilityTitle release]; [super dealloc]; }
- (SEL)action { return _action; }
- (void)setAction:(SEL)a { _action = a; }
- (id)propertyList { return _propertyList; }
- (void)setPropertyList:(id)p { [_propertyList release]; _propertyList = [p retain]; }
- (NSArray *)alternates { return _alternates ? _alternates : [NSArray array]; }
- (void)setAlternates:(NSArray *)a { [_alternates release]; _alternates = [a copy]; }
- (NSString *)discoverabilityTitle { return _discoverabilityTitle; }
- (void)setDiscoverabilityTitle:(NSString *)t { [_discoverabilityTitle release]; _discoverabilityTitle = [t copy]; }
- (NSUInteger)attributes { return _attributes; }
- (void)setAttributes:(NSUInteger)a { _attributes = a; }
- (NSInteger)state { return _state; }
- (void)setState:(NSInteger)s { _state = s; }
@end

@implementation UIKeyCommand
+ (instancetype)keyCommandWithInput:(NSString *)i modifierFlags:(NSInteger)m action:(SEL)a {
    UIKeyCommand *k = [[[self alloc] init] autorelease]; k.input = i; k.modifierFlags = m; k.action = a; return k;
}
- (void)dealloc { [_input release]; [super dealloc]; }
- (NSString *)input { return _input; }
- (void)setInput:(NSString *)i { [_input release]; _input = [i copy]; }
- (NSInteger)modifierFlags { return _modifierFlags; }
- (void)setModifierFlags:(NSInteger)m { _modifierFlags = m; }
- (BOOL)wantsPriorityOverSystemBehavior { return NO; }
- (void)setWantsPriorityOverSystemBehavior:(BOOL)w { (void)w; }
@end

@implementation UIBarItem
- (instancetype)init { if ((self = [super init])) _enabled = YES; return self; }
- (void)dealloc { [_title release]; [_image release]; [super dealloc]; }
- (NSString *)title { return _title; }
- (void)setTitle:(NSString *)t { [_title release]; _title = [t copy]; }
- (UIImage *)image { return _image; }
- (void)setImage:(UIImage *)i { [_image release]; _image = [i retain]; }
- (BOOL)isEnabled { return _enabled; }
- (void)setEnabled:(BOOL)e { _enabled = e; }
- (NSInteger)tag { return _tag; }
- (void)setTag:(NSInteger)t { _tag = t; }
@end

@implementation UIBarButtonItem
- (instancetype)initWithTitle:(NSString *)t style:(NSInteger)s target:(id)tg action:(SEL)a { (void)s; if ((self = [self init])) { self.title = t; _target = tg; _action = a; } return self; }
- (instancetype)initWithImage:(UIImage *)i style:(NSInteger)s target:(id)tg action:(SEL)a { (void)s; if ((self = [self init])) { self.image = i; _target = tg; _action = a; } return self; }
- (instancetype)initWithBarButtonSystemItem:(NSInteger)si target:(id)tg action:(SEL)a { if ((self = [self init])) { _systemItem = si; _target = tg; _action = a; } return self; }
- (instancetype)initWithCustomView:(UIView *)v { (void)v; return [self init]; }
- (void)dealloc { [_primaryAction release]; [_menu release]; [super dealloc]; }
- (UIAction *)primaryAction { return _primaryAction; }
- (void)setPrimaryAction:(UIAction *)a { [_primaryAction release]; _primaryAction = [a retain]; if (a && !_title) self.title = a.title; }
- (UIMenu *)menu { return _menu; }
- (void)setMenu:(UIMenu *)m { [_menu release]; _menu = [m retain]; }
- (NSInteger)_mosSystemItem { return _systemItem; }
- (void)set_mosSystemItem:(NSInteger)s { _systemItem = s; }
- (id)target { return _target; }
- (void)setTarget:(id)t { _target = t; }
- (SEL)action { return _action; }
- (void)setAction:(SEL)a { _action = a; }
@end

@implementation UIButton
+ (instancetype)buttonWithType:(NSInteger)type { UIButton *b = [[[self alloc] initWithFrame:CGRectMake(0, 0, 0, 0)] autorelease]; b->_buttonType = type; return b; }
+ (instancetype)systemButtonWithPrimaryAction:(UIAction *)a { UIButton *b = [self buttonWithType:1]; if (a) [b addAction:a forControlEvents:1 << 13]; return b; }
- (void)dealloc { [_titles release]; [super dealloc]; }
- (NSInteger)buttonType { return _buttonType; }
- (void)setTitle:(NSString *)t forState:(NSUInteger)s {
    if (!_titles) _titles = [[NSMutableDictionary alloc] init];
    NSNumber *k = [NSNumber numberWithUnsignedInteger:s];
    if (t) [_titles setObject:t forKey:k]; else [_titles removeObjectForKey:k];
    [self setNeedsDisplay];
}
- (NSString *)titleForState:(NSUInteger)s { return [_titles objectForKey:[NSNumber numberWithUnsignedInteger:s]]; }
- (NSString *)currentTitle { NSString *t = [self titleForState:0]; return t; }
- (void)setImage:(UIImage *)i forState:(NSUInteger)s { (void)i; (void)s; }
- (void)setTitleColor:(UIColor *)c forState:(NSUInteger)s { (void)c; (void)s; }
@end

/* UISheetPresentationController.preferredCornerRadius (o overlay Swift usa NaN para "nil") */
static char k_corner;
@interface UISheetPresentationController : NSObject
@end
@implementation UISheetPresentationController (MOSCorner)
- (CGFloat)_mosPreferredCornerRadius { NSNumber *n = objc_getAssociatedObject(self, &k_corner); return n ? n.doubleValue : NAN; }
- (void)set_mosPreferredCornerRadius:(CGFloat)r { objc_setAssociatedObject(self, &k_corner, [NSNumber numberWithDouble:r], OBJC_ASSOCIATION_RETAIN); }
@end
