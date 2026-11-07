/*
 * mOS — UIKit.framework do Mac Catalyst: controles com comportamento.
 *   UIButton: desenha o título (cor de destaque no tipo "system", escurece quando pressionado).
 *   UITextField: texto/placeholder, vira primeiro respondedor ao toque, recebe o teclado (UIKeyInput), manda
 *     EditingDidBegin/EditingChanged/EditingDidEnd e UITextFieldTextDidChangeNotification, consulta o delegate
 *     (textFieldShouldBeginEditing:, textField:shouldChangeCharactersInRange:replacementString:, textFieldShouldReturn:).
 *   UISwitch: alterna ao toque e manda ValueChanged; desenha trilho e botão.
 */
#import "UIKitMOS.h"
#import <objc/message.h>

extern NSString *const UITextFieldTextDidBeginEditingNotification, *const UITextFieldTextDidChangeNotification,
    *const UITextFieldTextDidEndEditingNotification;
typedef void *CGContextRef;
extern CGContextRef UIGraphicsGetCurrentContext(void);
extern void CGContextFillRect(CGContextRef c, CGRect r);
extern void CGContextSetFillColorWithColor(CGContextRef c, void *color);
extern void CGContextStrokeRect(CGContextRef c, CGRect r);
extern void CGContextSetStrokeColorWithColor(CGContextRef c, void *color);
extern void CGContextFillEllipseInRect(CGContextRef c, CGRect r);

/* texto com fonte e cor (AppKit do mOS), alinhado no retângulo */
static void mos_draw_text(NSString *t, UIFont *f, UIColor *col, CGRect r, NSInteger align) {
    if (!t.length) return;
    CGFloat size = f ? f->_size : 13;
    NSFont *nf = [NSFont systemFontOfSize:size];
    NSColor *nc = [NSColor colorWithRed:col->_r green:col->_g blue:col->_b alpha:col->_a];
    NSDictionary *a = [NSDictionary dictionaryWithObjectsAndKeys:nf, NSFontAttributeName, nc, NSForegroundColorAttributeName, nil];
    CGFloat w = ceil(t.length * size * 0.55), h = ceil(size * 1.2);
    CGFloat x = align == 1 ? r.origin.x + (r.size.width - w) / 2 : align == 2 ? r.origin.x + r.size.width - w : r.origin.x;
    [t drawAtPoint:NSMakePoint(x, r.origin.y + (r.size.height - h) / 2) withAttributes:a];
}

/* ================================================================ UIButton */
@implementation UIButton (MOSDraw)
- (void)drawRect:(CGRect)r {
    (void)r;
    NSString *t = [self titleForState:0];
    if (!t.length) return;
    UIColor *c = _buttonType == 0 ? [UIColor labelColor] : [self tintColor];
    if (_highlighted) c = [c colorWithAlphaComponent:c->_a * 0.5];
    if (_disabled) c = [UIColor colorWithWhite:0.6 alpha:1];
    mos_draw_text(t, [UIFont systemFontOfSize:13], c, CGRectMake(0, 0, _frame.size.width, _frame.size.height), 1);
}
- (CGSize)sizeThatFits:(CGSize)s { (void)s; NSString *t = [self titleForState:0]; return CGSizeMake(ceil(t.length * 13 * 0.55) + 20, 24); }
- (CGSize)intrinsicContentSize { return [self sizeThatFits:CGSizeMake(0, 0)]; }
- (UILabel *)titleLabel { return nil; }
@end

/* ================================================================ UITextField */
@implementation UITextField
- (instancetype)initWithFrame:(CGRect)f { if ((self = [super initWithFrame:f])) { _text = [@"" retain]; _borderStyle = 3; } return self; }
- (void)dealloc { [_text release]; [_placeholder release]; [_tfFont release]; [_tfColor release]; [super dealloc]; }
- (NSString *)text { return _text; }
- (void)setText:(NSString *)t { [_text release]; _text = [(t ? t : @"") copy]; [self setNeedsDisplay]; }
- (NSString *)placeholder { return _placeholder; }
- (void)setPlaceholder:(NSString *)p { [_placeholder release]; _placeholder = [p copy]; [self setNeedsDisplay]; }
- (NSAttributedString *)attributedText { return [[[NSAttributedString alloc] initWithString:_text] autorelease]; }
- (void)setAttributedText:(NSAttributedString *)a { [self setText:a.string]; }
- (UIFont *)font { return _tfFont ? _tfFont : [UIFont systemFontOfSize:13]; }
- (void)setFont:(UIFont *)f { [_tfFont release]; _tfFont = [f retain]; [self setNeedsDisplay]; }
- (UIColor *)textColor { return _tfColor ? _tfColor : [UIColor labelColor]; }
- (void)setTextColor:(UIColor *)c { [_tfColor release]; _tfColor = [c retain]; [self setNeedsDisplay]; }
- (NSInteger)textAlignment { return _tfAlignment; }
- (void)setTextAlignment:(NSInteger)a { _tfAlignment = a; [self setNeedsDisplay]; }
- (NSInteger)borderStyle { return _borderStyle; }
- (void)setBorderStyle:(NSInteger)b { _borderStyle = b; [self setNeedsDisplay]; }
- (id)delegate { return _tfDelegate; }
- (void)setDelegate:(id)d { _tfDelegate = d; }
- (BOOL)isSecureTextEntry { return _secure; }
- (void)setSecureTextEntry:(BOOL)s { _secure = s; [self setNeedsDisplay]; }
- (BOOL)isEditing { return _editing; }
- (BOOL)hasText { return _text.length > 0; }
- (BOOL)canBecomeFirstResponder { return !_disabled; }
- (BOOL)becomeFirstResponder {
    if (_editing) return YES;
    if ([_tfDelegate respondsToSelector:@selector(textFieldShouldBeginEditing:)] && ![_tfDelegate textFieldShouldBeginEditing:self]) return NO;
    if (![super becomeFirstResponder]) return NO;
    _editing = YES;
    if ([_tfDelegate respondsToSelector:@selector(textFieldDidBeginEditing:)]) [_tfDelegate textFieldDidBeginEditing:self];
    [self sendActionsForControlEvents:1 << 16];                     /* EditingDidBegin */
    [[NSNotificationCenter defaultCenter] postNotificationName:UITextFieldTextDidBeginEditingNotification object:self];
    [self setNeedsDisplay];
    return YES;
}
- (BOOL)resignFirstResponder {
    if (!_editing) return [super resignFirstResponder];
    _editing = NO;
    [super resignFirstResponder];
    if ([_tfDelegate respondsToSelector:@selector(textFieldDidEndEditing:)]) [_tfDelegate textFieldDidEndEditing:self];
    [self sendActionsForControlEvents:1 << 18];                     /* EditingDidEnd */
    [[NSNotificationCenter defaultCenter] postNotificationName:UITextFieldTextDidEndEditingNotification object:self];
    [self setNeedsDisplay];
    return YES;
}
- (void)_mosTouchUpInside { [self becomeFirstResponder]; [self sendActionsForControlEvents:1 << 6]; }
/* UIKeyInput */
- (BOOL)_mosShouldChange:(NSString *)rep at:(NSUInteger)loc len:(NSUInteger)len {
    if (![_tfDelegate respondsToSelector:@selector(textField:shouldChangeCharactersInRange:replacementString:)]) return YES;
    NSRange rg = NSMakeRange(loc, len);
    BOOL (*f)(id, SEL, id, NSRange, id) = (BOOL (*)(id, SEL, id, NSRange, id))objc_msgSend;
    return f(_tfDelegate, @selector(textField:shouldChangeCharactersInRange:replacementString:), self, rg, rep);
}
- (void)_mosChanged {
    [self sendActionsForControlEvents:1 << 17];                     /* EditingChanged */
    [[NSNotificationCenter defaultCenter] postNotificationName:UITextFieldTextDidChangeNotification object:self];
    [self setNeedsDisplay];
}
- (void)insertText:(NSString *)t {
    if ([t isEqualToString:@"\r"] || [t isEqualToString:@"\n"]) {
        if ([_tfDelegate respondsToSelector:@selector(textFieldShouldReturn:)]) [_tfDelegate textFieldShouldReturn:self];
        [self sendActionsForControlEvents:1 << 19];                 /* EditingDidEndOnExit */
        return;
    }
    if (![self _mosShouldChange:t at:_text.length len:0]) return;
    NSString *n = [_text stringByAppendingString:t];
    [self setText:n];
    [self _mosChanged];
}
- (void)deleteBackward {
    if (!_text.length) return;
    if (![self _mosShouldChange:@"" at:_text.length - 1 len:1]) return;
    [self setText:[_text substringToIndex:_text.length - 1]];
    [self _mosChanged];
}
- (void)drawRect:(CGRect)r {
    (void)r;
    CGContextRef c = UIGraphicsGetCurrentContext();
    CGRect b = CGRectMake(0, 0, _frame.size.width, _frame.size.height);
    if (c && _borderStyle == 3) {                                  /* UITextBorderStyleRoundedRect: fundo branco + borda */
        CGContextSetFillColorWithColor(c, [[UIColor whiteColor] CGColor]);
        CGContextFillRect(c, b);
        CGContextSetStrokeColorWithColor(c, [(_editing ? [self tintColor] : [UIColor opaqueSeparatorColor]) CGColor]);
        CGContextStrokeRect(c, b);
    }
    CGRect tr = CGRectMake(6, 0, b.size.width - 12, b.size.height);
    if (_text.length) {
        NSString *shown = _text;
        if (_secure) { NSMutableString *m = [NSMutableString string]; for (NSUInteger i = 0; i < _text.length; i++) [m appendString:@"•"]; shown = m; }
        mos_draw_text(_editing ? [shown stringByAppendingString:@"|"] : shown, [self font], [self textColor], tr, _tfAlignment);
    } else {
        if (_placeholder.length) mos_draw_text(_placeholder, [self font], [UIColor placeholderTextColor], tr, _tfAlignment);
        if (_editing) mos_draw_text(@"|", [self font], [self tintColor], tr, _tfAlignment);
    }
}
@end

/* ================================================================ UISwitch */
@implementation UISwitch
- (instancetype)initWithFrame:(CGRect)f { f.size = CGSizeMake(38, 22); return [super initWithFrame:f]; }
- (void)dealloc { [_onTint release]; [super dealloc]; }
- (BOOL)isOn { return _on; }
- (void)setOn:(BOOL)on { _on = on; [self setNeedsDisplay]; }
- (void)setOn:(BOOL)on animated:(BOOL)a { (void)a; [self setOn:on]; }
- (UIColor *)onTintColor { return _onTint; }
- (void)setOnTintColor:(UIColor *)c { [_onTint release]; _onTint = [c retain]; [self setNeedsDisplay]; }
- (CGSize)sizeThatFits:(CGSize)s { (void)s; return CGSizeMake(38, 22); }
- (void)_mosTouchUpInside {
    [self setOn:!_on];
    [self sendActionsForControlEvents:(1 << 6) | (1 << 12) | (1 << 13)];   /* TouchUpInside, ValueChanged, PrimaryActionTriggered */
}
- (void)drawRect:(CGRect)r {
    (void)r;
    CGContextRef c = UIGraphicsGetCurrentContext();
    if (!c) return;
    UIColor *track = _on ? (_onTint ? _onTint : [UIColor systemGreenColor]) : [UIColor systemGray4Color];
    CGContextSetFillColorWithColor(c, [track CGColor]);
    CGContextFillRect(c, CGRectMake(0, 0, _frame.size.width, _frame.size.height));
    CGFloat d = _frame.size.height - 4;
    CGContextSetFillColorWithColor(c, [[UIColor whiteColor] CGColor]);
    CGContextFillEllipseInRect(c, CGRectMake(_on ? _frame.size.width - d - 2 : 2, 2, d, d));
}
@end
