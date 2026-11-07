/*
 * mOS — UIKit.framework do Mac Catalyst: as funções C (contexto de imagem, acessibilidade, conversões de texto).
 * A acessibilidade do mOS ainda não tem leitor de tela: as consultas respondem "desligado".
 */
#import "UIKitMOS.h"

typedef void *CGContextRef;
extern CGContextRef UIGraphicsGetCurrentContext(void);
extern void UIGraphicsPushContext(CGContextRef c);
extern void UIGraphicsPopContext(void);
extern void *CGColorSpaceCreateDeviceRGB(void);
extern void CGColorSpaceRelease(void *s);
extern CGContextRef CGBitmapContextCreate(void *data, size_t w, size_t h, size_t bpc, size_t bpr, void *space, uint32_t info);
extern void *CGBitmapContextCreateImage(CGContextRef c);
extern void CGContextRelease(CGContextRef c);
extern void CGContextTranslateCTM(CGContextRef c, CGFloat x, CGFloat y);
extern void CGContextScaleCTM(CGContextRef c, CGFloat x, CGFloat y);
extern void CGImageRelease(void *img);

/* ---- contexto de imagem (UIGraphicsBeginImageContextWithOptions): bitmap RGBA, origem no canto de cima */
static CGContextRef g_img_ctx[8];
static CGFloat g_img_scale[8];
static int g_img_depth;
void UIGraphicsBeginImageContextWithOptions(CGSize size, BOOL opaque, CGFloat scale) {
    (void)opaque;
    if (scale <= 0) scale = 1;
    size_t w = (size_t)ceil(size.width * scale), h = (size_t)ceil(size.height * scale);
    if (!w) w = 1;
    if (!h) h = 1;
    void *cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef c = CGBitmapContextCreate(0, w, h, 8, w * 4, cs, 1 /* kCGImageAlphaPremultipliedLast */);
    CGColorSpaceRelease(cs);
    if (c) { CGContextTranslateCTM(c, 0, (CGFloat)h); CGContextScaleCTM(c, scale, -scale); }
    if (g_img_depth < 8) { g_img_ctx[g_img_depth] = c; g_img_scale[g_img_depth] = scale; g_img_depth++; }
    UIGraphicsPushContext(c);
}
void UIGraphicsBeginImageContext(CGSize size) { UIGraphicsBeginImageContextWithOptions(size, NO, 1); }
UIImage *UIGraphicsGetImageFromCurrentImageContext(void) {
    if (!g_img_depth || !g_img_ctx[g_img_depth - 1]) return nil;
    void *img = CGBitmapContextCreateImage(g_img_ctx[g_img_depth - 1]);
    if (!img) return nil;
    UIImage *u = [UIImage imageWithCGImage:img];
    u->_scale = g_img_scale[g_img_depth - 1];
    CGImageRelease(img);
    return u;
}
void UIGraphicsEndImageContext(void) {
    if (!g_img_depth) return;
    g_img_depth--;
    UIGraphicsPopContext();
    if (g_img_ctx[g_img_depth]) CGContextRelease(g_img_ctx[g_img_depth]);
}

/* ---- representações de imagem: o mOS ainda não tem codificador PNG/JPEG para apps (ImageIO de saída) */
NSData *UIImagePNGRepresentation(UIImage *i) { (void)i; return nil; }
NSData *UIImageJPEGRepresentation(UIImage *i, CGFloat q) { (void)i; (void)q; return nil; }
void UIImageWriteToSavedPhotosAlbum(UIImage *i, id t, SEL s, void *ctx) { (void)i; (void)t; (void)s; (void)ctx; }
BOOL UIVideoAtPathIsCompatibleWithSavedPhotosAlbum(NSString *p) { (void)p; return NO; }
void UISaveVideoAtPathToSavedPhotosAlbum(NSString *p, id t, SEL s, void *ctx) { (void)p; (void)t; (void)s; (void)ctx; }

/* ---- acessibilidade: tudo desligado */
void UIAccessibilityPostNotification(uint32_t n, id arg) { (void)n; (void)arg; }
BOOL UIAccessibilityIsVoiceOverRunning(void) { return NO; }
BOOL UIAccessibilityIsReduceMotionEnabled(void) { return NO; }
BOOL UIAccessibilityIsReduceTransparencyEnabled(void) { return NO; }
BOOL UIAccessibilityIsBoldTextEnabled(void) { return NO; }
BOOL UIAccessibilityIsVideoAutoplayEnabled(void) { return YES; }
BOOL UIAccessibilityDarkerSystemColorsEnabled(void) { return NO; }
BOOL UIAccessibilityIsInvertColorsEnabled(void) { return NO; }
BOOL UIAccessibilityIsSwitchControlRunning(void) { return NO; }
BOOL UIAccessibilityIsSpeakSelectionEnabled(void) { return NO; }
BOOL UIAccessibilityIsClosedCaptioningEnabled(void) { return NO; }
id UIAccessibilityFocusedElement(NSString *ident) { (void)ident; return nil; }
CGRect UIAccessibilityConvertFrameToScreenCoordinates(CGRect r, UIView *v) { if (v) r = [v convertRect:r toView:nil]; return r; }
id UIAccessibilityConvertPathToScreenCoordinates(id path, UIView *v) { (void)v; return path; }

/* ---- categorias de tamanho de texto */
static NSInteger size_rank(NSString *c) {
    static const char *order[] = {"XS", "S", "M", "L", "XL", "XXL", "XXXL", "AccessibilityM", "AccessibilityL",
                                  "AccessibilityXL", "AccessibilityXXL", "AccessibilityXXXL"};
    NSString *p = @"UICTContentSizeCategory";
    if (![c hasPrefix:p]) return 3;
    const char *s = [[c substringFromIndex:p.length] UTF8String];
    for (int i = 0; i < 12; i++) if (s && !strcmp(s, order[i])) return i;
    return 3;
}
BOOL UIContentSizeCategoryIsAccessibilityCategory(NSString *c) { return [c hasPrefix:@"UICTContentSizeCategoryAccessibility"]; }
NSComparisonResult UIContentSizeCategoryCompareToCategory(NSString *a, NSString *b) {
    NSInteger x = size_rank(a), y = size_rank(b);
    return x < y ? NSOrderedAscending : x > y ? NSOrderedDescending : NSOrderedSame;
}

/* UIImageSymbolWeight a partir do peso da fonte (UIFontWeight*) */
NSInteger UIImageSymbolWeightForFontWeight(CGFloat w) {
    if (w <= -0.7) return 1; if (w <= -0.5) return 2; if (w <= -0.2) return 3; if (w < 0.1) return 4;
    if (w < 0.27) return 5; if (w < 0.35) return 6; if (w < 0.5) return 7; if (w < 0.6) return 8; return 9;
}

/* ---- texto <-> estruturas */
typedef struct { CGFloat a, b, c, d, tx, ty; } mos_CGAffineTransform;
NSString *NSStringFromCGAffineTransform(mos_CGAffineTransform t) {
    return [NSString stringWithFormat:@"[%g, %g, %g, %g, %g, %g]", t.a, t.b, t.c, t.d, t.tx, t.ty];
}
CGRect CGRectFromString(NSString *s) {
    double v[4] = {0, 0, 0, 0};
    const char *p = [s UTF8String];
    int k = 0;
    while (p && *p && k < 4) {
        if ((*p >= '0' && *p <= '9') || *p == '-' || *p == '.') { char *e; v[k++] = strtod(p, &e); p = e; } else p++;
    }
    return CGRectMake(v[0], v[1], v[2], v[3]);
}
