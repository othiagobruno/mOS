/*
 * mOS — UITabBarController e UISplitViewController (Catalyst / iPad). A barra de abas fica embaixo;
 * o split mostra primário e secundário lado a lado quando a largura cabe, senão só o detalhe.
 */
#import "UIKitMOS.h"
#import <objc/runtime.h>

static char k_tabitem;

@interface UITabBarItem : UIBarItem
@end
@implementation UITabBarItem
- (instancetype)initWithTitle:(NSString *)t image:(UIImage *)img tag:(NSInteger)tag {
    if ((self = [super init])) { _title = [t copy]; _image = [img retain]; _tag = tag; _enabled = YES; }
    return self;
}
- (NSString *)title { return _title; }
- (void)setTitle:(NSString *)t { [_title release]; _title = [t copy]; }
@end

@implementation UIViewController (MOSTabItem)
- (UITabBarItem *)tabBarItem {
    UITabBarItem *i = objc_getAssociatedObject(self, &k_tabitem);
    if (!i) {
        i = [[UITabBarItem alloc] initWithTitle:_title image:nil tag:0];
        objc_setAssociatedObject(self, &k_tabitem, i, OBJC_ASSOCIATION_RETAIN);
        [i release];
    }
    return i;
}
- (void)setTabBarItem:(UITabBarItem *)i { objc_setAssociatedObject(self, &k_tabitem, i, OBJC_ASSOCIATION_RETAIN); }
@end

@interface UITabBar : UIView {
@public
    id _tabs; /* UITabBarController, não retido */
}
@end

@interface UITabBarController : UIViewController {
@public
    NSMutableArray *_vcs;
    UITabBar *_bar;
    NSInteger _selected;
}
- (void)setViewControllers:(NSArray *)vcs animated:(BOOL)a;
- (NSInteger)selectedIndex;
- (void)setSelectedIndex:(NSInteger)i;
- (UIViewController *)selectedViewController;
@end

@implementation UITabBar
- (void)drawRect:(CGRect)r {
    (void)r;
    extern void *UIGraphicsGetCurrentContext(void);
    void *c = UIGraphicsGetCurrentContext();
    if (!c) return;
    extern void CGContextFillRect(void *c, CGRect r);
    extern void CGContextSetFillColorWithColor(void *c, void *color);
    CGContextSetFillColorWithColor(c, [[UIColor secondarySystemBackgroundColor] CGColor]);
    CGContextFillRect(c, [self bounds]);
    UITabBarController *tabs = _tabs;
    NSUInteger n = tabs ? tabs->_vcs.count : 0;
    if (!n) return;
    CGFloat w = _frame.size.width / (CGFloat)n;
    for (NSUInteger i = 0; i < n; i++) {
        UIViewController *vc = [tabs->_vcs objectAtIndex:i];
        NSString *t = vc.tabBarItem.title;
        if (!t.length) t = vc->_title;
        if (!t.length) t = @"";
        NSColor *col = ((NSInteger)i == tabs->_selected) ? [NSColor colorWithRed:0 green:0.48 blue:1 alpha:1] : [NSColor grayColor];
        [t drawAtPoint:NSMakePoint(w * (CGFloat)i + 8, 16) withAttributes:[NSDictionary dictionaryWithObjectsAndKeys:col, NSForegroundColorAttributeName, nil]];
    }
}
- (void)touchesEnded:(NSSet *)t withEvent:(UIEvent *)e {
    (void)e;
    UITabBarController *tabs = _tabs;
    UITouch *u = [t anyObject];
    if (!u || !tabs || tabs->_vcs.count == 0) return;
    CGFloat w = _frame.size.width / (CGFloat)tabs->_vcs.count;
    NSInteger i = (NSInteger)([u locationInView:self].x / (w > 1 ? w : 1));
    if (i < 0) i = 0;
    if (i >= (NSInteger)tabs->_vcs.count) i = (NSInteger)tabs->_vcs.count - 1;
    [tabs setSelectedIndex:i];
}
@end

@implementation UITabBarController
- (instancetype)init { if ((self = [super init])) { _vcs = [[NSMutableArray alloc] init]; } return self; }
- (void)dealloc { [_vcs release]; [_bar release]; [super dealloc]; }
- (void)loadView {
    [super loadView];
    _bar = [[UITabBar alloc] initWithFrame:CGRectMake(0, 0, 0, 50)];
    _bar->_tabs = self;
    [_view addSubview:_bar];
    [self _mosShow];
}
- (NSArray *)viewControllers { return [[_vcs copy] autorelease]; }
- (void)setViewControllers:(NSArray *)vcs { [self setViewControllers:vcs animated:NO]; }
- (void)setViewControllers:(NSArray *)vcs animated:(BOOL)a {
    (void)a;
    UIViewController *old = (_selected >= 0 && (NSUInteger)_selected < _vcs.count) ? [_vcs objectAtIndex:(NSUInteger)_selected] : nil;
    if (old && old->_view) [old->_view removeFromSuperview];
    for (UIViewController *v in _vcs) [v removeFromParentViewController];
    [_vcs removeAllObjects];
    for (UIViewController *v in vcs) { [self addChildViewController:v]; [_vcs addObject:v]; (void)v.tabBarItem; }
    _selected = _vcs.count ? 0 : -1;
    if (_view) [self _mosShow];
}
- (NSInteger)selectedIndex { return _selected; }
- (UIViewController *)selectedViewController {
    if (_selected < 0 || (NSUInteger)_selected >= _vcs.count) return nil;
    return [_vcs objectAtIndex:(NSUInteger)_selected];
}
- (void)setSelectedViewController:(UIViewController *)vc {
    NSUInteger n = _vcs.count, i;
    for (i = 0; i < n; i++) if ([_vcs objectAtIndex:i] == vc) { [self setSelectedIndex:(NSInteger)i]; return; }
}
- (void)setSelectedIndex:(NSInteger)i {
    if (i < 0 || (NSUInteger)i >= _vcs.count || i == _selected) return;
    UIViewController *old = [self selectedViewController];
    if (old && old->_view) { [old viewWillDisappear:NO]; [old->_view removeFromSuperview]; [old viewDidDisappear:NO]; }
    _selected = i;
    [self _mosShow];
    [_bar setNeedsDisplay];
}
- (void)_mosLayout {
    if (!_view) return;
    CGRect b = [_view bounds];
    CGFloat barH = 50;
    _bar.frame = CGRectMake(0, b.size.height - barH, b.size.width, barH);
    UIViewController *cur = [self selectedViewController];
    if (cur) [cur view].frame = CGRectMake(0, 0, b.size.width, b.size.height - barH);
}
- (void)_mosShow {
    UIViewController *cur = [self selectedViewController];
    if (!cur || !_view) return;
    UIView *v = [cur view];
    [cur viewWillAppear:NO];
    [_view insertSubview:v atIndex:0];
    [self _mosLayout];
    [cur viewDidAppear:NO];
    [_bar setNeedsDisplay];
}
- (void)viewDidLayoutSubviews { [self _mosLayout]; }
- (UITabBar *)tabBar { (void)[self view]; return _bar; }
@end

@interface UISplitViewController : UIViewController {
@public
    NSMutableArray *_vcs;
    BOOL _collapsed;
    CGFloat _primaryW;
}
- (void)setViewControllers:(NSArray *)vcs;
- (BOOL)isCollapsed;
@end

@implementation UISplitViewController
- (instancetype)init { if ((self = [super init])) { _vcs = [[NSMutableArray alloc] init]; _primaryW = 280; } return self; }
- (void)dealloc { [_vcs release]; [super dealloc]; }
- (NSArray *)viewControllers { return [[_vcs copy] autorelease]; }
- (void)setViewControllers:(NSArray *)vcs {
    for (UIViewController *v in _vcs) {
        if (v->_view) [v->_view removeFromSuperview];
        [v removeFromParentViewController];
    }
    [_vcs removeAllObjects];
    for (UIViewController *v in vcs) { [self addChildViewController:v]; [_vcs addObject:v]; }
    if (_view) [self _mosShow];
}
- (BOOL)isCollapsed { return _collapsed; }
- (CGFloat)preferredPrimaryColumnWidth { return _primaryW; }
- (void)setPreferredPrimaryColumnWidth:(CGFloat)w { _primaryW = w > 0 ? w : 280; [self _mosLayout]; }
- (void)_mosLayout {
    if (!_view) return;
    CGRect b = [_view bounds];
    _collapsed = b.size.width < 600 || _vcs.count < 2;
    UIViewController *pri = _vcs.count ? [_vcs objectAtIndex:0] : nil;
    UIViewController *sec = _vcs.count > 1 ? [_vcs objectAtIndex:1] : nil;
    if (_collapsed) {
        if (pri && pri->_view) pri->_view.hidden = YES;
        if (sec) { [sec view].hidden = NO; [sec view].frame = b; }
        else if (pri) { [pri view].hidden = NO; [pri view].frame = b; }
    } else {
        CGFloat pw = _primaryW;
        if (pw > b.size.width - 80) pw = b.size.width * 0.4;
        if (pri) { [pri view].hidden = NO; [pri view].frame = CGRectMake(0, 0, pw, b.size.height); }
        if (sec) { [sec view].hidden = NO; [sec view].frame = CGRectMake(pw, 0, b.size.width - pw, b.size.height); }
    }
}
- (void)_mosShow {
    if (!_view) return;
    for (UIViewController *v in _vcs) {
        UIView *view = [v view];
        if (view.superview != _view) { [v viewWillAppear:NO]; [_view addSubview:view]; [v viewDidAppear:NO]; }
    }
    [self _mosLayout];
}
- (void)viewDidLayoutSubviews { [self _mosLayout]; }
- (void)showDetailViewController:(UIViewController *)vc sender:(id)s {
    (void)s;
    if (!vc) return;
    if (_vcs.count == 0) [self setViewControllers:[NSArray arrayWithObject:vc]];
    else if (_vcs.count == 1) [self setViewControllers:[NSArray arrayWithObjects:[_vcs objectAtIndex:0], vc, nil]];
    else {
        UIViewController *old = [_vcs objectAtIndex:1];
        if (old->_view) [old->_view removeFromSuperview];
        [old removeFromParentViewController];
        [_vcs replaceObjectAtIndex:1 withObject:vc];
        [self addChildViewController:vc];
        [self _mosShow];
    }
}
@end
