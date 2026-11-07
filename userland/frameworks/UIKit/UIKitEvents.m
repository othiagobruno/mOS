/*
 * mOS — UIKit.framework do Mac Catalyst: entrada. O mouse do AppKit vira um toque (UITouch/UIEvent) na UIView achada
 * por -hitTest:withEvent: na NSView hospedeira; touchesBegan/Moved/Ended sobem pela cadeia de resposta
 * (view -> view controller -> superview ... -> janela -> UIApplication -> delegate). Os gestos da view tocada e dos
 * ancestrais acompanham o toque (toque, arrastar, toque longo). UIControl traduz o toque em TouchDown/TouchUpInside/
 * PrimaryActionTriggered. O teclado vai para o primeiro respondedor da janela (UIKeyInput: insertText:/deleteBackward).
 */
#import "UIKitMOS.h"
#import <objc/runtime.h>

static NSTimeInterval mos_now(void) { return [[NSProcessInfo processInfo] systemUptime]; }

/* ================================================================ UITouch, UIEvent */
@implementation UITouch
- (void)dealloc { [_view release]; [super dealloc]; }
- (UIView *)view { return _view; }
- (UIWindow *)window { return _window; }
- (NSInteger)phase { return _phase; }
- (NSUInteger)tapCount { return (NSUInteger)_tapCount; }
- (NSTimeInterval)timestamp { return _timestamp; }
- (NSInteger)type { return 3; }                         /* UITouchTypeIndirectPointer (mouse no Mac) */
- (CGFloat)force { return 0; }
- (CGFloat)majorRadius { return 1; }
- (CGPoint)locationInView:(UIView *)v { return v ? [v convertPoint:_loc fromView:nil] : _loc; }
- (CGPoint)previousLocationInView:(UIView *)v { return v ? [v convertPoint:_prev fromView:nil] : _prev; }
- (NSArray *)gestureRecognizers { return [NSArray array]; }
@end

@implementation UIEvent
- (void)dealloc { [_touches release]; [super dealloc]; }
- (NSSet *)allTouches { return _touches; }
- (NSSet *)touchesForView:(UIView *)v { NSMutableSet *s = [NSMutableSet set]; for (UITouch *t in _touches) if (t->_view == v) [s addObject:t]; return s; }
- (NSSet *)touchesForWindow:(UIWindow *)w { (void)w; return _touches; }
- (NSInteger)type { return 0; }                         /* UIEventTypeTouches */
- (NSInteger)subtype { return 0; }
- (NSTimeInterval)timestamp { return _timestamp; }
- (NSInteger)buttonMask { return 1; }
- (NSInteger)modifierFlags { return 0; }
@end

/* ================================================================ cadeia de resposta */
@implementation UIResponder (MOSTouches)
- (void)touchesBegan:(NSSet *)t withEvent:(UIEvent *)e { [[self nextResponder] touchesBegan:t withEvent:e]; }
- (void)touchesMoved:(NSSet *)t withEvent:(UIEvent *)e { [[self nextResponder] touchesMoved:t withEvent:e]; }
- (void)touchesEnded:(NSSet *)t withEvent:(UIEvent *)e { [[self nextResponder] touchesEnded:t withEvent:e]; }
- (void)touchesCancelled:(NSSet *)t withEvent:(UIEvent *)e { [[self nextResponder] touchesCancelled:t withEvent:e]; }
- (void)pressesBegan:(NSSet *)p withEvent:(id)e { [[self nextResponder] pressesBegan:p withEvent:e]; }
- (void)pressesEnded:(NSSet *)p withEvent:(id)e { [[self nextResponder] pressesEnded:p withEvent:e]; }
- (NSArray *)keyCommands { return nil; }
- (id)inputView { return nil; }
- (id)inputAccessoryView { return nil; }
- (BOOL)canPerformAction:(SEL)a withSender:(id)s { (void)s; return [self respondsToSelector:a]; }
- (id)targetForAction:(SEL)a withSender:(id)s {
    for (UIResponder *r = self; r; r = [r nextResponder]) if ([r canPerformAction:a withSender:s]) return r;
    return nil;
}
@end

/* primeiro respondedor: guardado na janela da view */
@implementation UIView (MOSFirstResponder)
- (BOOL)becomeFirstResponder {
    if (![self canBecomeFirstResponder]) return NO;
    UIWindow *w = [self window];
    if (!w) return NO;
    if (w->_firstResponder && w->_firstResponder != self) [w->_firstResponder resignFirstResponder];
    w->_firstResponder = self;
    return YES;
}
- (BOOL)resignFirstResponder {
    UIWindow *w = [self window];
    if (w && w->_firstResponder == self) w->_firstResponder = nil;
    return YES;
}
- (BOOL)isFirstResponder { UIWindow *w = [self window]; return w && w->_firstResponder == self; }

/* gestos */
- (NSArray *)gestureRecognizers { return _gestures ? [[_gestures copy] autorelease] : nil; }
- (void)setGestureRecognizers:(NSArray *)g {
    for (UIGestureRecognizer *r in _gestures) r->_view = nil;
    [_gestures release];
    _gestures = [g mutableCopy];
    for (UIGestureRecognizer *r in _gestures) r->_view = self;
}
- (void)addGestureRecognizer:(UIGestureRecognizer *)g {
    if (!g) return;
    if (!_gestures) _gestures = [[NSMutableArray alloc] init];
    if (g->_view && g->_view != self) [g->_view removeGestureRecognizer:g];
    g->_view = self;
    [_gestures addObject:g];
}
- (void)removeGestureRecognizer:(UIGestureRecognizer *)g {
    for (NSInteger i = (NSInteger)_gestures.count - 1; i >= 0; i--) if ([_gestures objectAtIndex:(NSUInteger)i] == g) { g->_view = nil; [_gestures removeObjectAtIndex:(NSUInteger)i]; }
}
- (void)addInteraction:(id)i { (void)i; }
- (void)removeInteraction:(id)i { (void)i; }
- (NSArray *)interactions { return [NSArray array]; }
@end

/* ================================================================ gestos */
@interface _UIGestureTarget : NSObject {
@public
    id _target;                     /* não retido */
    SEL _action;
}
@end
@implementation _UIGestureTarget
@end

@implementation UIGestureRecognizer
- (instancetype)init { return [self initWithTarget:nil action:NULL]; }
- (instancetype)initWithTarget:(id)target action:(SEL)action {
    if ((self = [super init])) { _targets = [[NSMutableArray alloc] init]; if (target && action) [self addTarget:target action:action]; }
    return self;
}
- (void)dealloc { [_targets release]; [super dealloc]; }
- (void)addTarget:(id)t action:(SEL)a { _UIGestureTarget *g = [[_UIGestureTarget alloc] init]; g->_target = t; g->_action = a; [_targets addObject:g]; [g release]; }
- (void)removeTarget:(id)t action:(SEL)a {
    for (NSInteger i = (NSInteger)_targets.count - 1; i >= 0; i--) {
        _UIGestureTarget *g = [_targets objectAtIndex:(NSUInteger)i];
        if ((!t || g->_target == t) && (!a || g->_action == a)) [_targets removeObjectAtIndex:(NSUInteger)i];
    }
}
- (UIView *)view { return _view; }
- (NSInteger)state { return _state; }
- (void)setState:(NSInteger)s { _state = s; }
- (BOOL)isEnabled { return !_disabled; }
- (void)setEnabled:(BOOL)e { _disabled = !e; }
- (id)delegate { return _delegate; }
- (void)setDelegate:(id)d { _delegate = d; }
- (BOOL)cancelsTouchesInView { return YES; }
- (void)setCancelsTouchesInView:(BOOL)c { (void)c; }
- (BOOL)delaysTouchesBegan { return NO; }
- (void)setDelaysTouchesBegan:(BOOL)d { (void)d; }
- (BOOL)delaysTouchesEnded { return NO; }
- (void)setDelaysTouchesEnded:(BOOL)d { (void)d; }
- (void)requireGestureRecognizerToFail:(UIGestureRecognizer *)g { (void)g; }
- (NSString *)name { return nil; }
- (void)setName:(NSString *)n { (void)n; }
- (NSUInteger)numberOfTouches { return _state >= 1 && _state <= 2 ? 1 : 0; }
- (CGPoint)locationInView:(UIView *)v { return v ? [v convertPoint:_cur fromView:nil] : _cur; }
- (CGPoint)locationOfTouch:(NSUInteger)i inView:(UIView *)v { (void)i; return [self locationInView:v]; }
- (void)reset { _state = 0; }
- (void)_mosFire {
    if ([_delegate respondsToSelector:@selector(gestureRecognizerShouldBegin:)] && _state <= 1 && ![_delegate gestureRecognizerShouldBegin:self]) { _state = 5; return; }
    NSArray *ts = [[_targets copy] autorelease];
    for (_UIGestureTarget *g in ts) if (g->_target && [g->_target respondsToSelector:g->_action]) [g->_target performSelector:g->_action withObject:self];
}
- (void)_mosBegan:(UITouch *)t { _start = _cur = t->_loc; _t0 = mos_now(); _state = 0; }
- (void)_mosMoved:(UITouch *)t { _cur = t->_loc; }
- (void)_mosEnded:(UITouch *)t { _cur = t->_loc; _state = 0; }
@end

static CGFloat mos_dist(CGPoint a, CGPoint b) { CGFloat dx = a.x - b.x, dy = a.y - b.y; return sqrt(dx * dx + dy * dy); }

@implementation UITapGestureRecognizer
- (instancetype)initWithTarget:(id)t action:(SEL)a { if ((self = [super initWithTarget:t action:a])) _taps = 1; return self; }
- (NSUInteger)numberOfTapsRequired { return _taps; }
- (void)setNumberOfTapsRequired:(NSUInteger)n { _taps = n ? n : 1; }
- (NSUInteger)numberOfTouchesRequired { return 1; }
- (void)setNumberOfTouchesRequired:(NSUInteger)n { (void)n; }
- (void)_mosEnded:(UITouch *)t {
    _cur = t->_loc;
    if (mos_dist(_start, _cur) <= 10 && (NSUInteger)t->_tapCount >= _taps) { _state = 3; [self _mosFire]; }   /* .ended (reconhecido) */
    _state = 0;
}
@end

@implementation UILongPressGestureRecognizer
- (instancetype)initWithTarget:(id)t action:(SEL)a { if ((self = [super initWithTarget:t action:a])) _minDuration = 0.5; return self; }
- (NSTimeInterval)minimumPressDuration { return _minDuration; }
- (void)setMinimumPressDuration:(NSTimeInterval)d { _minDuration = d; }
- (CGFloat)allowableMovement { return 10; }
- (void)setAllowableMovement:(CGFloat)m { (void)m; }
- (void)_mosEnded:(UITouch *)t {
    _cur = t->_loc;
    if (mos_dist(_start, _cur) <= 10 && mos_now() - _t0 >= _minDuration) { _state = 1; [self _mosFire]; _state = 3; [self _mosFire]; }
    _state = 0;
}
@end

@implementation UIPanGestureRecognizer
- (void)_mosMoved:(UITouch *)t {
    _cur = t->_loc;
    if (_state == 0) { if (mos_dist(_start, _cur) < 5) return; _state = 1; [self _mosFire]; return; }   /* .began */
    _state = 2;                                                                                        /* .changed */
    [self _mosFire];
}
- (void)_mosEnded:(UITouch *)t { _cur = t->_loc; if (_state == 1 || _state == 2) { _state = 3; [self _mosFire]; } _state = 0; }
- (CGPoint)translationInView:(UIView *)v { (void)v; return CGPointMake(_cur.x - _start.x, _cur.y - _start.y); }
- (void)setTranslation:(CGPoint)p inView:(UIView *)v { (void)v; _start = CGPointMake(_cur.x - p.x, _cur.y - p.y); }
- (CGPoint)velocityInView:(UIView *)v { (void)v; NSTimeInterval dt = mos_now() - _t0; return dt > 0 ? CGPointMake((_cur.x - _start.x) / dt, (_cur.y - _start.y) / dt) : CGPointMake(0, 0); }
- (NSUInteger)minimumNumberOfTouches { return 1; }
- (void)setMinimumNumberOfTouches:(NSUInteger)n { (void)n; }
- (NSUInteger)maximumNumberOfTouches { return 1; }
- (void)setMaximumNumberOfTouches:(NSUInteger)n { (void)n; }
@end

/* ================================================================ UIControl: toque -> eventos de controle */
@implementation UIControl (MOSTouches)
- (void)touchesBegan:(NSSet *)t withEvent:(UIEvent *)e {
    (void)t; (void)e;
    if (_disabled) return;
    [self setHighlighted:YES];
    [self sendActionsForControlEvents:1 << 0];                          /* TouchDown */
}
- (void)touchesMoved:(NSSet *)t withEvent:(UIEvent *)e { (void)t; (void)e; }
- (void)touchesEnded:(NSSet *)t withEvent:(UIEvent *)e {
    (void)e;
    if (_disabled) return;
    [self setHighlighted:NO];
    UITouch *touch = [t anyObject];
    CGPoint p = touch ? [touch locationInView:self] : CGPointMake(0, 0);
    if ([self pointInside:p withEvent:nil]) [self _mosTouchUpInside];
    else [self sendActionsForControlEvents:1 << 7];                    /* TouchUpOutside */
}
- (void)touchesCancelled:(NSSet *)t withEvent:(UIEvent *)e { (void)t; (void)e; [self setHighlighted:NO]; [self sendActionsForControlEvents:1 << 8]; }
/* subclasses (UISwitch...) mudam o valor aqui antes; o padrão do UIKit dispara TouchUpInside e a ação primária */
- (void)_mosTouchUpInside { [self sendActionsForControlEvents:(1 << 6) | (1 << 13)]; }
@end

/* ================================================================ NSView hospedeira: mouse e teclado */
@implementation _UIHostingView (MOSEvents)
- (BOOL)acceptsFirstResponder { return YES; }
- (CGPoint)_mosPoint:(NSEvent *)e { NSPoint p = [self convertPoint:e.locationInWindow fromView:nil]; return CGPointMake(p.x, p.y); }

/* começa um toque num ponto (coordenadas da janela UIKit) — também usado pelos testes */
- (void)_mosTouchDownAt:(CGPoint)p clickCount:(NSInteger)n {
    UIView *hit = [_uiWindow hitTest:p withEvent:nil];
    if (!hit) return;
    [_touch release];
    _touch = [[UITouch alloc] init];
    _touch->_view = [hit retain];
    _touch->_window = _uiWindow;
    _touch->_loc = _touch->_prev = p;
    _touch->_phase = 0;                                                 /* UITouchPhaseBegan */
    _touch->_tapCount = n > 0 ? n : 1;
    _touch->_timestamp = mos_now();
    NSMutableArray *gs = [NSMutableArray array];
    for (UIView *v = hit; v; v = v->_superview) for (UIGestureRecognizer *g in v->_gestures) if (!g->_disabled) [gs addObject:g];
    [_touchGestures release];
    _touchGestures = [gs copy];
    for (UIGestureRecognizer *g in _touchGestures) [g _mosBegan:_touch];
    UIEvent *ev = [[[UIEvent alloc] init] autorelease];
    ev->_touches = [[NSSet setWithObject:_touch] retain];
    [hit touchesBegan:ev->_touches withEvent:ev];
}
- (void)_mosTouchMoveTo:(CGPoint)p {
    if (!_touch) return;
    _touch->_prev = _touch->_loc; _touch->_loc = p; _touch->_phase = 1; _touch->_timestamp = mos_now();
    for (UIGestureRecognizer *g in _touchGestures) [g _mosMoved:_touch];
    UIEvent *ev = [[[UIEvent alloc] init] autorelease];
    ev->_touches = [[NSSet setWithObject:_touch] retain];
    [_touch->_view touchesMoved:ev->_touches withEvent:ev];
}
- (void)_mosTouchUpAt:(CGPoint)p {
    if (!_touch) return;
    _touch->_prev = _touch->_loc; _touch->_loc = p; _touch->_phase = 3; _touch->_timestamp = mos_now();   /* Ended */
    UIEvent *ev = [[[UIEvent alloc] init] autorelease];
    ev->_touches = [[NSSet setWithObject:_touch] retain];
    [_touch->_view touchesEnded:ev->_touches withEvent:ev];
    for (UIGestureRecognizer *g in _touchGestures) [g _mosEnded:_touch];
    [_touchGestures release]; _touchGestures = nil;
    [_touch release]; _touch = nil;
    [self setNeedsDisplay:YES];
}
- (void)mouseDown:(NSEvent *)e { [self _mosTouchDownAt:[self _mosPoint:e] clickCount:e.clickCount]; }
- (void)mouseDragged:(NSEvent *)e { [self _mosTouchMoveTo:[self _mosPoint:e]]; }
- (void)mouseUp:(NSEvent *)e { [self _mosTouchUpAt:[self _mosPoint:e]]; }

/* teclado: o primeiro respondedor da janela recebe o texto (UIKeyInput) */
- (void)_mosTypeText:(NSString *)s {
    UIResponder *r = _uiWindow ? _uiWindow->_firstResponder : nil;
    if (!r) return;
    const char *u = [s UTF8String];
    if (!u) return;
    /* backspace/delete apagam; o resto entra como texto (a tecla inteira de uma vez) */
    if ((u[0] == 0x7f || u[0] == 8) && !u[1]) { if ([r respondsToSelector:@selector(deleteBackward)]) [(id)r deleteBackward]; }
    else if ([r respondsToSelector:@selector(insertText:)]) [(id)r insertText:s];
    [self setNeedsDisplay:YES];
}
- (void)keyDown:(NSEvent *)e { NSString *s = e.characters; if (s.length) [self _mosTypeText:s]; }
@end

/* roda do mouse: rola a UIScrollView sob o ponteiro (ou a mais próxima acima dela) */
@implementation _UIHostingView (MOSScroll)
- (void)_mosScrollAt:(CGPoint)p dy:(CGFloat)dy {
    for (UIView *v = [_uiWindow hitTest:p withEvent:nil]; v; v = v->_superview)
        if ([v isKindOfClass:[UIScrollView class]]) { [(UIScrollView *)v _mosScrollBy:dy dx:0]; break; }
    [self setNeedsDisplay:YES];
}
- (void)scrollWheel:(NSEvent *)e { [self _mosScrollAt:[self _mosPoint:e] dy:-e.scrollingDeltaY]; }
@end
