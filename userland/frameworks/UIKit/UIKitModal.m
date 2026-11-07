/*
 * mOS — UIKit do Catalyst: apresentação modal e alertas.
 *   -presentViewController:animated:completion: põe a view do controlador apresentado num contêiner que cobre a janela
 *   (fundo escurecido; folha centralizada no estilo padrão do Catalyst, tela inteira em UIModalPresentationFullScreen/
 *   OverFullScreen); -dismissViewControllerAnimated:completion: tira o topo da pilha. As ligações apresentado/
 *   apresentador ficam em objetos associados (sem ivars novos no UIViewController: as subclasses dos apps não mudam).
 *   UIAlertController monta título, mensagem, campos de texto e um botão por UIAlertAction; tocar num botão chama o
 *   handler e fecha o alerta.
 */
#import "UIKitMOS.h"
#include <objc/runtime.h>
#include <objc/message.h>

static char k_presented, k_presenting, k_container, k_style;

@interface _UIModalContainerView : UIView
@end
@implementation _UIModalContainerView
@end

@implementation UIViewController (MOSModal)
- (UIViewController *)presentedViewController { return objc_getAssociatedObject(self, &k_presented); }
- (UIViewController *)presentingViewController { return objc_getAssociatedObject(self, &k_presenting); }
- (NSInteger)modalPresentationStyle {
    NSNumber *n = objc_getAssociatedObject(self, &k_style);
    return n ? [n integerValue] : 2;                                     /* UIModalPresentationPageSheet (padrão no Catalyst) */
}
- (void)setModalPresentationStyle:(NSInteger)s { objc_setAssociatedObject(self, &k_style, [NSNumber numberWithInteger:s], OBJC_ASSOCIATION_RETAIN); }
- (NSInteger)modalTransitionStyle { return 0; }
- (void)setModalTransitionStyle:(NSInteger)s { (void)s; }
- (BOOL)isModalInPresentation { return NO; }
- (void)setModalInPresentation:(BOOL)m { (void)m; }
- (BOOL)isBeingPresented { return NO; }
- (BOOL)isBeingDismissed { return NO; }
- (id)presentationController { return nil; }
- (id)popoverPresentationController { return nil; }
- (void)setTransitioningDelegate:(id)d { (void)d; }
- (CGSize)preferredContentSize { return CGSizeMake(0, 0); }
- (void)setPreferredContentSize:(CGSize)s { (void)s; }

/* a janela onde apresentar: a da própria view, senão a janela principal da aplicação */
- (UIWindow *)_mosPresentationWindow {
    for (UIViewController *v = self; v; v = [v presentingViewController]) {
        UIWindow *w = v->_view ? (UIWindow *)[(id)v->_view window] : nil;
        if (w) return w;
        for (UIViewController *p = v->_parent; p; p = p->_parent) if (p->_view && [(id)p->_view window]) return (UIWindow *)[(id)p->_view window];
    }
    return (UIWindow *)[(id)[UIApplication sharedApplication] keyWindow];
}
- (CGRect)_mosModalFrameIn:(CGRect)b {
    NSInteger st = [self modalPresentationStyle];
    if (st == 0 || st == 5) return b;                                    /* FullScreen, OverFullScreen */
    CGSize want = [self preferredContentSize];
    if ([self respondsToSelector:@selector(_mosAlertSize)]) want = ((CGSize (*)(id, SEL))objc_msgSend)(self, @selector(_mosAlertSize));
    CGFloat w = want.width > 0 ? want.width : 540, h = want.height > 0 ? want.height : 620;
    if (w > b.size.width - 40) w = b.size.width - 40;
    if (h > b.size.height - 40) h = b.size.height - 40;
    return CGRectMake(floor((b.size.width - w) / 2), floor((b.size.height - h) / 2), w, h);
}
- (void)presentViewController:(UIViewController *)vc animated:(BOOL)a completion:(void (^)(void))c {
    if (!vc) return;
    UIViewController *host = self;
    while ([host presentedViewController]) host = [host presentedViewController];   /* sempre sobre o topo */
    UIWindow *w = [host _mosPresentationWindow];
    objc_setAssociatedObject(host, &k_presented, vc, OBJC_ASSOCIATION_RETAIN);
    objc_setAssociatedObject(vc, &k_presenting, host, OBJC_ASSOCIATION_ASSIGN);
    if (w) {
        CGRect b = [w bounds];
        _UIModalContainerView *box = [[[_UIModalContainerView alloc] initWithFrame:b] autorelease];
        box->_autoresizingMask = 18;
        NSInteger st = [vc modalPresentationStyle];
        [box setBackgroundColor:(st == 0 || st == 5) ? [UIColor clearColor] : [UIColor colorWithRed:0 green:0 blue:0 alpha:0.3]];
        UIView *v = [vc view];
        [vc viewWillAppear:a];
        v.frame = [vc _mosModalFrameIn:b];
        [box addSubview:v];
        [w addSubview:box];
        objc_setAssociatedObject(vc, &k_container, box, OBJC_ASSOCIATION_RETAIN);
        [v setNeedsLayout];
        [v layoutIfNeeded];
        [vc viewDidAppear:a];
        [w setNeedsDisplay];
    }
    if (c) c();
}
- (void)dismissViewControllerAnimated:(BOOL)a completion:(void (^)(void))c {
    /* como no UIKit: quem apresenta fecha o que apresentou; o apresentado pede ao apresentador */
    UIViewController *top = [self presentedViewController] ? [self presentedViewController] : self;
    UIViewController *host = [top presentingViewController];
    if (!host) { if (c) c(); return; }
    if ([top presentedViewController]) [top dismissViewControllerAnimated:NO completion:nil];
    [top viewWillDisappear:a];
    UIView *box = objc_getAssociatedObject(top, &k_container);
    UIWindow *w = (UIWindow *)[(id)box window];
    [top->_view removeFromSuperview];
    [box removeFromSuperview];
    objc_setAssociatedObject(top, &k_container, nil, OBJC_ASSOCIATION_RETAIN);
    [top viewDidDisappear:a];
    [[top retain] autorelease];
    objc_setAssociatedObject(top, &k_presenting, nil, OBJC_ASSOCIATION_ASSIGN);
    objc_setAssociatedObject(host, &k_presented, nil, OBJC_ASSOCIATION_RETAIN);
    [w setNeedsDisplay];
    if (c) c();
}
- (void)showViewController:(UIViewController *)vc sender:(id)s {
    (void)s;
    id nav = [self respondsToSelector:@selector(navigationController)] ? [(id)self navigationController] : nil;
    if (nav) [nav pushViewController:vc animated:YES];
    else [self presentViewController:vc animated:YES completion:nil];
}
- (void)showDetailViewController:(UIViewController *)vc sender:(id)s { [self showViewController:vc sender:s]; }
@end

/* ================================================================ alertas */
@interface UIAlertAction : NSObject <NSCopying> {
@public
    NSString *_title;
    NSInteger _style;
    void (^_handler)(UIAlertAction *);
    BOOL _disabled;
}
@end
@implementation UIAlertAction
+ (instancetype)actionWithTitle:(NSString *)t style:(NSInteger)s handler:(void (^)(UIAlertAction *))h {
    UIAlertAction *a = [[[self alloc] init] autorelease];
    a->_title = [t copy]; a->_style = s; a->_handler = [h copy];
    return a;
}
- (void)dealloc { [_title release]; [_handler release]; [super dealloc]; }
- (id)copyWithZone:(NSZone *)z { (void)z; return [self retain]; }
- (NSString *)title { return _title; }
- (NSInteger)style { return _style; }
- (BOOL)isEnabled { return !_disabled; }
- (void)setEnabled:(BOOL)e { _disabled = !e; }
- (void)setImage:(id)i { (void)i; }
- (void)setAccessibilityIdentifier:(NSString *)s { (void)s; }
@end

@interface UIAlertController : UIViewController {
@public
    NSString *_message;
    NSInteger _alertStyle;
    NSMutableArray *_actions, *_fields;
    UIAlertAction *_preferred;
}
@end
@implementation UIAlertController
+ (instancetype)alertControllerWithTitle:(NSString *)t message:(NSString *)m preferredStyle:(NSInteger)s {
    UIAlertController *a = [[[self alloc] init] autorelease];
    [a setTitle:t];
    a->_message = [m copy];
    a->_alertStyle = s;
    return a;
}
- (instancetype)init {
    if ((self = [super init])) { _actions = [[NSMutableArray alloc] init]; _fields = [[NSMutableArray alloc] init]; }
    return self;
}
- (void)dealloc { [_message release]; [_actions release]; [_fields release]; [super dealloc]; }
- (NSString *)message { return _message; }
- (void)setMessage:(NSString *)m { [_message release]; _message = [m copy]; }
- (NSInteger)preferredStyle { return _alertStyle; }
- (NSArray *)actions { return [[_actions copy] autorelease]; }
- (void)addAction:(UIAlertAction *)a { if (a) [_actions addObject:a]; }
- (UIAlertAction *)preferredAction { return _preferred; }
- (void)setPreferredAction:(UIAlertAction *)a { _preferred = a; }
- (NSArray *)textFields { return [[_fields copy] autorelease]; }
- (void)addTextFieldWithConfigurationHandler:(void (^)(UITextField *))h {
    UITextField *f = [[[UITextField alloc] initWithFrame:CGRectMake(0, 0, 240, 26)] autorelease];
    [_fields addObject:f];
    if (h) h(f);
}
- (NSInteger)modalPresentationStyle { return 6; }                       /* sempre um cartão centralizado */
- (CGSize)_mosAlertSize {
    CGFloat h = 20 + 22;                                                 /* margem + título */
    if (_message.length) h += 20 * (1 + (CGFloat)(_message.length / 40));
    h += 34 * (CGFloat)_fields.count;
    h += 12 + 36 * (CGFloat)(_actions.count > 2 || _alertStyle == 0 ? _actions.count : 1);
    return CGSizeMake(_alertStyle == 0 ? 300 : 280, h + 12);
}
static UILabel *mos_label(NSString *t, CGRect r, UIFont *f) {
    UILabel *l = [[[UILabel alloc] initWithFrame:r] autorelease];
    [l setText:t];
    [(id)l setFont:(id)f];
    l->_alignment = 1;
    l->_lines = 0;
    return l;
}
- (void)loadView {
    CGSize sz = [self _mosAlertSize];
    UIView *v = [[[UIView alloc] initWithFrame:CGRectMake(0, 0, sz.width, sz.height)] autorelease];
    [v setBackgroundColor:[UIColor secondarySystemBackgroundColor]];
    [self setView:v];
    CGFloat y = 20, W = sz.width;
    if (_title.length) { [v addSubview:mos_label(_title, CGRectMake(16, y, W - 32, 20), [UIFont boldSystemFontOfSize:14])]; y += 22; }
    if (_message.length) {
        CGFloat mh = 20 * (1 + (CGFloat)(_message.length / 40));
        [v addSubview:mos_label(_message, CGRectMake(16, y, W - 32, mh), [UIFont systemFontOfSize:12])];
        y += mh;
    }
    for (UITextField *f in _fields) { f.frame = CGRectMake(20, y + 4, W - 40, 26); [v addSubview:f]; y += 34; }
    y += 12;
    BOOL lado = _actions.count <= 2 && _alertStyle != 0;                 /* dois botões lado a lado no alerta */
    NSUInteger n = _actions.count, i = 0;
    for (UIAlertAction *a in _actions) {
        UIButton *b = [UIButton buttonWithType:1];
        [b setTitle:a->_title forState:0];
        b.tag = 9000 + (NSInteger)i;
        b.frame = lado ? CGRectMake(12 + (W - 24) / (CGFloat)n * (CGFloat)i, y, (W - 24) / (CGFloat)n - (n > 1 ? 6 : 0), 30)
                       : CGRectMake(12, y + 36 * (CGFloat)i, W - 24, 30);
        if (a->_disabled) [b setEnabled:NO];
        [b addTarget:self action:@selector(_mosTapped:) forControlEvents:1 << 6];
        [v addSubview:b];
        i++;
    }
}
- (void)_mosTapped:(UIButton *)b {
    NSInteger i = b.tag - 9000;
    if (i < 0 || i >= (NSInteger)_actions.count) return;
    UIAlertAction *a = [[[_actions objectAtIndex:(NSUInteger)i] retain] autorelease];
    [self retain];
    [self dismissViewControllerAnimated:YES completion:nil];
    if (a->_handler) a->_handler(a);
    [self release];
}
@end
