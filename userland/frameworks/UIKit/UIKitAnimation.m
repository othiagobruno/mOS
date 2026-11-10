/*
 * mOS — UIKit do Catalyst: animações de UIView e UIViewPropertyAnimator.
 *   Como no UIKit, o bloco de animações muda o modelo na hora (frame, alpha, backgroundColor, contentOffset e
 *   transform lidos depois do bloco já são os finais) e só a apresentação anda no tempo. O mOS fotografa as views das janelas antes e depois do bloco
 *   (resolvendo o Auto Layout dos dois lados, então animar constantes de restrição + layoutIfNeeded funciona), guarda
 *   as diferenças e, a cada quadro, a NSView hospedeira desenha com os valores interpolados (mos_anim_present) e
 *   devolve o modelo logo depois (mos_anim_restore).
 *   Curvas: as do CAMediaTimingFunction do CoreAnimation do libSystem (pontos de controle), cúbicas próprias e mola
 *   amortecida (dampingRatio). Relógio: systemUptime; +[UIView _mosAdvanceAnimationClock:] adianta o relógio (testes).
 */
#import "UIKitMOS.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <objc/message.h>
#ifndef CGVECTOR_DEFINED
typedef struct CGVector { CGFloat dx; CGFloat dy; } CGVector;
#endif

static void rm_ident(NSMutableArray *a, id o) { for (NSUInteger i = a.count; i > 0; i--) if ([a objectAtIndex:i - 1] == o) { [a removeObjectAtIndex:i - 1]; return; } }

typedef struct { CGRect frame; CGPoint boundsOrigin; CATransform3D t3; CGFloat alpha; CGFloat rgba[4]; BOOL color; } mos_vs_t;

@interface _MOSAnimRun : NSObject {
@public
    UIView *_v;                      /* retida */
    mos_vs_t _from, _to, _saved;
    int _mask;                       /* 1 frame, 2 alpha, 4 cor, 8 contentOffset, 16 transform, 32 transição por imagem */
    int _kind;                       /* UIViewAnimationOptionTransition* (>> 20); 0 = só propriedades */
    UIColor *_savedColor, *_tmpColor;
    UIImage *_fromImg, *_toImg;      /* retidas: snapshots do transitionWithView: */
}
@end
@implementation _MOSAnimRun
- (void)dealloc { [_v release]; [_tmpColor release]; [_fromImg release]; [_toImg release]; [super dealloc]; }
@end

@interface _MOSAnimGroup : NSObject {
@public
    NSMutableArray *_runs;
    double _dur, _delayLeft, _fraction, _last;
    int _type;                       /* 0 bézier (_cp), 1 mola (_z) */
    double _cp[4], _z;
    BOOL _paused, _reversed, _done, _started;
    void (^_completion)(BOOL);
    id _animator;                    /* UIViewPropertyAnimator dono (não retido) */
}
@end
@implementation _MOSAnimGroup
- (instancetype)init { if ((self = [super init])) { _runs = [[NSMutableArray alloc] init]; _cp[0] = 0.42; _cp[1] = 0; _cp[2] = 0.58; _cp[3] = 1; } return self; }
- (void)dealloc { [_runs release]; [_completion release]; [super dealloc]; }
@end

static NSMutableArray *g_groups;
static double g_skew;
static int g_disabled, g_capturing;
static id g_driver;
static NSTimer *g_timer;
static double mos_clock(void) { return [[NSProcessInfo processInfo] systemUptime] + g_skew; }

/* ---- curvas */
static double bez(double a, double b, double s) { double u = 1 - s; return 3 * u * u * s * a + 3 * u * s * s * b + s * s * s; }
static double bez_d(double a, double b, double s) { double u = 1 - s; return 3 * u * u * a + 6 * u * s * (b - a) + 3 * s * s * (1 - b); }
static double curve_eval(_MOSAnimGroup *g, double p) {
    if (p <= 0) return 0;
    if (p >= 1) return 1;
    if (g->_type == 1) {
        double z = g->_z > 0.01 ? g->_z : 0.01;
        if (z >= 1) { double w = 9.2; return 1 - (1 + w * p) * exp(-w * p); }
        double w = 6.9 / z, wd = w * sqrt(1 - z * z);
        return 1 - exp(-z * w * p) * (cos(wd * p) + z * w / wd * sin(wd * p));
    }
    double s = p;                                                         /* Newton, com bisseção de reserva */
    for (int i = 0; i < 8; i++) {
        double x = bez(g->_cp[0], g->_cp[2], s) - p, d = bez_d(g->_cp[0], g->_cp[2], s);
        if (fabs(x) < 1e-6) break;
        if (fabs(d) < 1e-6) break;
        s -= x / d;
    }
    if (s < 0 || s > 1 || fabs(bez(g->_cp[0], g->_cp[2], s) - p) > 1e-4) {
        double lo = 0, hi = 1; s = p;
        for (int i = 0; i < 30; i++) { double x = bez(g->_cp[0], g->_cp[2], s); if (x < p) lo = s; else hi = s; s = (lo + hi) / 2; }
    }
    return bez(g->_cp[1], g->_cp[3], s);
}
/* os pontos de controle das curvas padrão vêm do CAMediaTimingFunction do libSystem */
static void curve_named(_MOSAnimGroup *g, NSInteger curve) {
    NSString *n = curve == 1 ? @"easeIn" : curve == 2 ? @"easeOut" : curve == 3 ? @"linear" : @"easeInEaseOut";
    Class tf = NSClassFromString(@"CAMediaTimingFunction");
    id f = tf ? [tf performSelector:@selector(functionWithName:) withObject:n] : nil;
    if (f && [f respondsToSelector:@selector(getControlPointAtIndex:values:)]) {
        float a[2] = {0, 0}, b[2] = {1, 1};
        ((void (*)(id, SEL, size_t, float *))objc_msgSend)(f, @selector(getControlPointAtIndex:values:), 1, a);
        ((void (*)(id, SEL, size_t, float *))objc_msgSend)(f, @selector(getControlPointAtIndex:values:), 2, b);
        g->_cp[0] = a[0]; g->_cp[1] = a[1]; g->_cp[2] = b[0]; g->_cp[3] = b[1];
        return;
    }
    static const double t[4][4] = {{0.42, 0, 0.58, 1}, {0.42, 0, 1, 1}, {0, 0, 0.58, 1}, {0, 0, 1, 1}};
    int k = curve >= 0 && curve <= 3 ? (int)curve : 0;
    for (int i = 0; i < 4; i++) g->_cp[i] = t[k][i];
}

/* ---- fotografia das views */
static void vs_read(UIView *v, mos_vs_t *s) {
    s->frame = v->_frame;
    s->boundsOrigin = v->_boundsOrigin;
    s->t3 = v->_t3;
    s->alpha = v->_alpha;
    s->color = v->_backgroundColor != nil;
    if (s->color) { UIColor *c = v->_backgroundColor; s->rgba[0] = c->_r; s->rgba[1] = c->_g; s->rgba[2] = c->_b; s->rgba[3] = c->_a; }
    else s->rgba[0] = s->rgba[1] = s->rgba[2] = s->rgba[3] = 0;
}
static void collect(UIView *v, NSMutableArray *out) { [out addObject:v]; for (UIView *s in v->_subviews) collect(s, out); }
static NSArray *all_windows(void) { return [[UIApplication sharedApplication] windows]; }
static void settle_layout(void) {
    for (UIWindow *w in all_windows()) {
        if ([w respondsToSelector:@selector(_mosSolveLayout)]) [w performSelector:@selector(_mosSolveLayout)];
        [w layoutIfNeeded];
    }
}
typedef struct { NSMutableArray *views; mos_vs_t *st; } mos_snap_t;
static mos_snap_t snap_take(void) {
    settle_layout();
    mos_snap_t s = { [[NSMutableArray alloc] init], NULL };
    for (UIWindow *w in all_windows()) collect(w, s.views);
    s.st = calloc(s.views.count + 1, sizeof(mos_vs_t));
    for (NSUInteger i = 0; i < s.views.count; i++) vs_read([s.views objectAtIndex:i], &s.st[i]);
    return s;
}
static BOOL rect_eq(CGRect a, CGRect b) { return a.origin.x == b.origin.x && a.origin.y == b.origin.y && a.size.width == b.size.width && a.size.height == b.size.height; }
/* roda o bloco e devolve um grupo com o que mudou (nil se as animações estão desligadas: o bloco só roda) */
static _MOSAnimGroup *capture(void (^block)(void)) {
    if (g_disabled || g_capturing) { if (block) block(); return nil; }
    mos_snap_t s = snap_take();
    g_capturing++;
    if (block) block();
    g_capturing--;
    settle_layout();
    _MOSAnimGroup *g = [[[_MOSAnimGroup alloc] init] autorelease];
    for (NSUInteger i = 0; i < s.views.count; i++) {
        UIView *v = [s.views objectAtIndex:i];
        mos_vs_t now; vs_read(v, &now);
        int m = 0;
        if (!rect_eq(now.frame, s.st[i].frame)) m |= 1;
        if (now.alpha != s.st[i].alpha) m |= 2;
        if (now.color != s.st[i].color || memcmp(now.rgba, s.st[i].rgba, sizeof now.rgba)) m |= 4;
        if (now.boundsOrigin.x != s.st[i].boundsOrigin.x || now.boundsOrigin.y != s.st[i].boundsOrigin.y) m |= 8;
        if (memcmp(&now.t3, &s.st[i].t3, sizeof now.t3)) m |= 16;
        if (!m) continue;
        _MOSAnimRun *r = [[[_MOSAnimRun alloc] init] autorelease];
        r->_v = [v retain]; r->_from = s.st[i]; r->_to = now; r->_mask = m;
        if ((m & 4) && !r->_from.color) { memcpy(r->_from.rgba, now.rgba, sizeof now.rgba); r->_from.rgba[3] = 0; }
        if ((m & 4) && !r->_to.color) { memcpy(r->_to.rgba, r->_from.rgba, sizeof now.rgba); r->_to.rgba[3] = 0; }
        [g->_runs addObject:r];
    }
    free(s.st);
    [s.views release];
    return g;
}

@interface _MOSAnimDriver : NSObject
- (void)tick:(id)t;
@end
static void mos_redraw(void) { for (UIWindow *w in all_windows()) [w setNeedsDisplay]; }
static void ensure_timer(void) {
    if (g_timer) return;
    if (!g_driver) g_driver = [[_MOSAnimDriver alloc] init];
    g_timer = [[NSTimer scheduledTimerWithTimeInterval:1.0 / 60 target:g_driver selector:@selector(tick:) userInfo:nil repeats:YES] retain];
}
static void group_add(_MOSAnimGroup *g) {
    if (!g_groups) g_groups = [[NSMutableArray alloc] init];
    g->_last = mos_clock();
    g->_started = YES;
    [g_groups addObject:g];
    ensure_timer();
    mos_redraw();
}
/* valores atuais (apresentação) de um passo */
static double lerp(double a, double b, double f) { return a + (b - a) * f; }
static void run_value(_MOSAnimRun *r, double f, mos_vs_t *o) {
    o->frame = CGRectMake(lerp(r->_from.frame.origin.x, r->_to.frame.origin.x, f), lerp(r->_from.frame.origin.y, r->_to.frame.origin.y, f),
                          lerp(r->_from.frame.size.width, r->_to.frame.size.width, f), lerp(r->_from.frame.size.height, r->_to.frame.size.height, f));
    o->boundsOrigin = CGPointMake(lerp(r->_from.boundsOrigin.x, r->_to.boundsOrigin.x, f), lerp(r->_from.boundsOrigin.y, r->_to.boundsOrigin.y, f));
    CGFloat *a = (CGFloat *)&r->_from.t3, *b = (CGFloat *)&r->_to.t3, *d = (CGFloat *)&o->t3;
    for (int i = 0; i < 16; i++) d[i] = lerp(a[i], b[i], f);
    o->alpha = lerp(r->_from.alpha, r->_to.alpha, f);
    for (int i = 0; i < 4; i++) o->rgba[i] = lerp(r->_from.rgba[i], r->_to.rgba[i], f);
}
/* põe o modelo de um grupo numa fração (stop/finish/reverse do animador) */
static void group_commit(_MOSAnimGroup *g, double f) {
    double e = curve_eval(g, f);
    for (_MOSAnimRun *r in g->_runs) {
        mos_vs_t o; run_value(r, e, &o);
        if (r->_mask & 1) [r->_v setFrame:o.frame];
        if (r->_mask & 8) {
            CGSize sz = (r->_mask & 1) ? o.frame.size : [r->_v bounds].size;
            [r->_v setBounds:CGRectMake(o.boundsOrigin.x, o.boundsOrigin.y, sz.width, sz.height)];
        }
        if (r->_mask & 16) [r->_v setTransform3D:o.t3];
        if (r->_mask & 2) [r->_v setAlpha:o.alpha];
        if (r->_mask & 4) [r->_v setBackgroundColor:(f >= 1 && !r->_to.color) ? nil : (f <= 0 && !r->_from.color) ? nil :
                              [UIColor colorWithRed:o.rgba[0] green:o.rgba[1] blue:o.rgba[2] alpha:o.rgba[3]]];
    }
}
static void group_finish(_MOSAnimGroup *g, NSInteger position);

@implementation _MOSAnimDriver
- (void)tick:(id)t {
    (void)t;
    double now = mos_clock();
    NSArray *gs = [[g_groups copy] autorelease];
    for (_MOSAnimGroup *g in gs) {
        double dt = now - g->_last;
        g->_last = now;
        if (g->_paused || g->_done) continue;
        if (g->_delayLeft > 0) { g->_delayLeft -= dt; if (g->_delayLeft > 0) continue; dt = -g->_delayLeft; g->_delayLeft = 0; }
        g->_fraction += (g->_dur > 0 ? dt / g->_dur : 1) * (g->_reversed ? -1 : 1);
        if (!g->_reversed && g->_fraction >= 1) { g->_fraction = 1; group_finish(g, 0); }
        else if (g->_reversed && g->_fraction <= 0) { g->_fraction = 0; group_finish(g, 1); }
    }
    if (!g_groups.count) { [g_timer invalidate]; [g_timer release]; g_timer = nil; }
    mos_redraw();
}
@end

/* ---- ganchos da NSView hospedeira (UIKitCore.m) */
void mos_anim_present(void) {
    for (_MOSAnimGroup *g in g_groups) {
        if (g->_done) continue;
        double e = curve_eval(g, g->_fraction);
        for (_MOSAnimRun *r in g->_runs) {
            UIView *v = r->_v;
            vs_read(v, &r->_saved);
            r->_savedColor = v->_backgroundColor;
            mos_vs_t o; run_value(r, e, &o);
            if (r->_mask & 1) v->_frame = o.frame;
            if (r->_mask & 8) v->_boundsOrigin = o.boundsOrigin;
            if (r->_mask & 16) v->_t3 = o.t3;
            if (r->_mask & 2) v->_alpha = o.alpha;
            if (r->_mask & 4) { [r->_tmpColor release]; r->_tmpColor = [[UIColor colorWithRed:o.rgba[0] green:o.rgba[1] blue:o.rgba[2] alpha:o.rgba[3]] retain]; v->_backgroundColor = r->_tmpColor; }
        }
    }
}
void mos_anim_restore(void) {
    for (NSInteger gi = (NSInteger)g_groups.count - 1; gi >= 0; gi--) {        /* ordem inversa: sobreposições voltam certas */
        _MOSAnimGroup *g = [g_groups objectAtIndex:(NSUInteger)gi];
        if (g->_done) continue;
        for (NSInteger ri = (NSInteger)g->_runs.count - 1; ri >= 0; ri--) {
            _MOSAnimRun *r = [g->_runs objectAtIndex:(NSUInteger)ri];
            UIView *v = r->_v;
            if (r->_mask & 1) v->_frame = r->_saved.frame;
            if (r->_mask & 8) v->_boundsOrigin = r->_saved.boundsOrigin;
            if (r->_mask & 16) v->_t3 = r->_saved.t3;
            if (r->_mask & 2) v->_alpha = r->_saved.alpha;
            if (r->_mask & 4) v->_backgroundColor = r->_savedColor;
        }
    }
}

static _MOSAnimGroup *start(double dur, double delay, NSUInteger options, void (^anim)(void), void (^completion)(BOOL)) {
    _MOSAnimGroup *g = capture(anim);
    if (!g) { if (completion) completion(YES); return nil; }
    curve_named(g, (NSInteger)((options >> 16) & 0xf));
    g->_dur = dur > 0 ? dur : 0;
    g->_delayLeft = delay > 0 ? delay : 0;
    g->_completion = [completion copy];
    group_add(g);
    return g;
}

/* ================================================================ API de UIView */
static double g_kf_dur;
static double g_kf_delay;
static _MOSAnimGroup *g_kf_last;
@implementation UIView (MOSAnimation)
+ (void)animateWithDuration:(NSTimeInterval)d animations:(void (^)(void))a { start(d, 0, 0, a, nil); }
+ (void)animateWithDuration:(NSTimeInterval)d animations:(void (^)(void))a completion:(void (^)(BOOL))c { start(d, 0, 0, a, c); }
+ (void)animateWithDuration:(NSTimeInterval)d delay:(NSTimeInterval)dl options:(NSUInteger)o animations:(void (^)(void))a completion:(void (^)(BOOL))c {
    start(d, dl, o, a, c);
}
+ (void)animateWithDuration:(NSTimeInterval)d delay:(NSTimeInterval)dl usingSpringWithDamping:(CGFloat)z initialSpringVelocity:(CGFloat)v
                    options:(NSUInteger)o animations:(void (^)(void))a completion:(void (^)(BOOL))c {
    (void)v;
    _MOSAnimGroup *g = start(d, dl, o, a, c);
    if (g) { g->_type = 1; g->_z = z; }
}
+ (void)transitionWithView:(UIView *)v duration:(NSTimeInterval)d options:(NSUInteger)o animations:(void (^)(void))a completion:(void (^)(BOOL))c {
    NSUInteger kind = (o >> 20) & 15;
    UIImage *from = (kind && v && [self areAnimationsEnabled]) ? [[v _mosSnapshotImage] retain] : nil;
    _MOSAnimGroup *g = start(d, 0, o, a, c);
    if (!(g && kind && v && from)) { [from release]; return; }
    UIImage *to = [v _mosSnapshotImage];
    _MOSAnimRun *r = nil;
    for (_MOSAnimRun *e in g->_runs) if (e->_v == v) { r = e; break; }
    if (!r) {
        r = [[[_MOSAnimRun alloc] init] autorelease];
        r->_v = [v retain];
        vs_read(v, &r->_from);
        r->_to = r->_from;
        [g->_runs addObject:r];
    }
    r->_fromImg = from;
    r->_toImg = [to retain];
    r->_mask |= 32;
    r->_kind = (int)kind;
}
+ (void)transitionFromView:(UIView *)from toView:(UIView *)to duration:(NSTimeInterval)d options:(NSUInteger)o completion:(void (^)(BOOL))c {
    (void)d; (void)o;
    UIView *sup = from->_superview;
    if (sup && to) { [sup insertSubview:to aboveSubview:from]; to.frame = from.frame; [from removeFromSuperview]; }
    if (c) c(YES);
}
+ (void)performWithoutAnimation:(void (^)(void))a { g_disabled++; if (a) a(); g_disabled--; }
+ (BOOL)areAnimationsEnabled { return !g_disabled; }
+ (void)setAnimationsEnabled:(BOOL)e { g_disabled = e ? 0 : 1; }
+ (NSTimeInterval)inheritedAnimationDuration { return g_capturing ? 0.25 : 0; }
+ (void)modifyAnimationsWithRepeatCount:(CGFloat)n autoreverses:(BOOL)r animations:(void (^)(void))a { (void)n; (void)r; if (a) a(); }
/* quadros-chave: cada addKeyframe vira um grupo com o seu atraso e duração relativos */
+ (void)animateKeyframesWithDuration:(NSTimeInterval)d delay:(NSTimeInterval)dl options:(NSUInteger)o animations:(void (^)(void))a completion:(void (^)(BOOL))c {
    (void)o;
    double sd = g_kf_dur, sl = g_kf_delay; _MOSAnimGroup *slast = g_kf_last;
    g_kf_dur = d; g_kf_delay = dl; g_kf_last = nil;
    if (a) a();
    _MOSAnimGroup *last = g_kf_last;
    g_kf_dur = sd; g_kf_delay = sl; g_kf_last = slast;
    if (last) last->_completion = [c copy];
    else if (c) c(YES);
}
+ (void)addKeyframeWithRelativeStartTime:(double)s relativeDuration:(double)r animations:(void (^)(void))a {
    if (g_kf_dur <= 0) { if (a) a(); return; }
    _MOSAnimGroup *g = start(g_kf_dur * r, g_kf_delay + g_kf_dur * s, 3 << 16, a, nil);
    if (g && (!g_kf_last || g->_delayLeft + g->_dur >= g_kf_last->_delayLeft + g_kf_last->_dur)) g_kf_last = g;
}
/* testes: adianta o relógio das animações e roda um quadro */
+ (void)_mosAdvanceAnimationClock:(double)s { g_skew += s; [g_driver tick:nil]; }
+ (NSUInteger)_mosRunningAnimationCount { NSUInteger n = 0; for (_MOSAnimGroup *g in g_groups) if (!g->_done) n++; return n; }
/* a apresentação atual desta view (como layer.presentationLayer.frame) */
- (CGRect)_mosPresentationFrame { mos_anim_present(); CGRect f = [self frame]; mos_anim_restore(); return f; }
- (CGFloat)_mosPresentationAlpha { mos_anim_present(); CGFloat a = _alpha; mos_anim_restore(); return a; }
@end

@implementation UIScrollView (MOSAnimOffset)
- (CGPoint)_mosPresentationContentOffset { mos_anim_present(); CGPoint p = _boundsOrigin; mos_anim_restore(); return p; }
@end

/* transição por imagem: no contexto bitmap, cruza os dois snapshots (flip e curl também; não há malha 3D).
 * Fora de bitmap devolve 0 e o desenho cai na interpolação de propriedades — CGContextDrawImage não pinta a janela. */
extern void *CGBitmapContextGetData(void *);
extern void *CGImageGetDataProvider(void *);
extern void *CGDataProviderCopyData(void *);
extern const unsigned char *CFDataGetBytePtr(void *);
extern void CFRelease(void *);
extern void *CGDataProviderCreateWithData(void *, const void *, size_t, void (*)(void *, const void *, size_t));
extern void CGDataProviderRelease(void *);
extern void *CGImageCreate(size_t, size_t, size_t, size_t, size_t, const void *, uint32_t, void *, const void *, int, int);
extern void *CGImageRetain(void *);
extern void CGImageRelease(void *);
extern void CGContextDrawImage(void *, CGRect, void *);
extern size_t CGImageGetWidth(void *), CGImageGetHeight(void *);

static const uint32_t *mos_img_px(UIImage *im, int *w, int *h, id *rel) {
    *rel = nil; *w = *h = 0;
    void *cg = im ? [im CGImage] : 0;
    if (!cg) return 0;
    *w = (int)CGImageGetWidth(cg); *h = (int)CGImageGetHeight(cg);
    if (*w < 1 || *h < 1) return 0;
    void *prov = CGImageGetDataProvider(cg);
    id data = prov ? (id)CGDataProviderCopyData(prov) : nil;
    if (!data) return 0;
    *rel = data;
    return (const uint32_t *)CFDataGetBytePtr(data);
}
/* CGContextDrawImage amostra v de baixo para cima; o buffer sai invertido para o bitmap y-down ficar em pé. */
static void *mos_crossfade(UIImage *a, UIImage *b, double f) {
    id ra = nil, rb = nil;
    int wa, ha, wb, hb;
    const uint32_t *pa = mos_img_px(a, &wa, &ha, &ra), *pb = mos_img_px(b, &wb, &hb, &rb);
    if (!pa || !pb || wa != wb || ha != hb) {
        void *one = [((f >= 1 && b) ? b : a) CGImage];
        if (one) CGImageRetain(one);
        if (ra) CFRelease(ra);
        if (rb) CFRelease(rb);
        return one;
    }
    size_t n = (size_t)wa * (size_t)ha;
    uint32_t *buf = malloc(n * 4);
    if (!buf) { CFRelease(ra); CFRelease(rb); return 0; }
    for (int y = 0; y < ha; y++) {
        for (int x = 0; x < wa; x++) {
            uint32_t A = pa[(size_t)y * wa + x], B = pb[(size_t)y * wa + x], o = 0;
            for (int k = 0; k < 4; k++) {
                int sa = (A >> (k * 8)) & 255, sb = (B >> (k * 8)) & 255;
                int v = (int)(sa + (sb - sa) * f + 0.5);
                if (v < 0) v = 0;
                if (v > 255) v = 255;
                o |= (uint32_t)v << (k * 8);
            }
            buf[(size_t)(ha - 1 - y) * wa + x] = o;
        }
    }
    CFRelease(ra); CFRelease(rb);
    void *prov = CGDataProviderCreateWithData(0, buf, n * 4, 0);
    void *img = CGImageCreate((size_t)wa, (size_t)ha, 8, 32, (size_t)wa * 4, 0, 2, prov, 0, 0, 0);
    CGDataProviderRelease(prov);
    free(buf);
    return img;
}
int mos_anim_draw_transition(UIView *v, void *c) {
    if (!v || !g_groups || !CGBitmapContextGetData(c)) return 0;
    for (_MOSAnimGroup *g in g_groups) {
        if (!g || g->_done) continue;
        double f = curve_eval(g, g->_fraction);
        if (f < 0) f = 0;
        if (f > 1) f = 1;
        for (_MOSAnimRun *r in g->_runs) {
            if (r->_v != v || !(r->_mask & 32) || !r->_fromImg || r->_kind == 0) continue;
            void *img = mos_crossfade(r->_fromImg, r->_toImg ? r->_toImg : r->_fromImg, f);
            if (!img) return 0;
            CGContextDrawImage(c, CGRectMake(0, 0, v->_frame.size.width, v->_frame.size.height), img);
            CGImageRelease(img);
            return 1;
        }
    }
    return 0;
}

/* ================================================================ parâmetros de tempo */
@interface UICubicTimingParameters : NSObject <NSCopying> {
@public
    NSInteger _curve;
    CGPoint _c1, _c2;
    BOOL _custom;
}
@end
@implementation UICubicTimingParameters
- (instancetype)init { return [self initWithAnimationCurve:0]; }
- (instancetype)initWithAnimationCurve:(NSInteger)c { if ((self = [super init])) _curve = c; return self; }
- (instancetype)initWithControlPoint1:(CGPoint)a controlPoint2:(CGPoint)b { if ((self = [super init])) { _c1 = a; _c2 = b; _custom = YES; } return self; }
- (id)copyWithZone:(NSZone *)z { (void)z; return [self retain]; }
- (NSInteger)animationCurve { return _curve; }
- (CGPoint)controlPoint1 { return _c1; }
- (CGPoint)controlPoint2 { return _c2; }
- (NSInteger)timingCurveType { return _custom ? 1 : 0; }
- (id)cubicTimingParameters { return self; }
- (id)springTimingParameters { return nil; }
@end
@interface UISpringTimingParameters : NSObject <NSCopying> {
@public
    CGFloat _z;
}
@end
@implementation UISpringTimingParameters
- (instancetype)init { return [self initWithDampingRatio:1]; }
- (instancetype)initWithDampingRatio:(CGFloat)z { if ((self = [super init])) _z = z; return self; }
- (instancetype)initWithDampingRatio:(CGFloat)z initialVelocity:(CGVector)v { (void)v; return [self initWithDampingRatio:z]; }
- (instancetype)initWithMass:(CGFloat)m stiffness:(CGFloat)k damping:(CGFloat)c initialVelocity:(CGVector)v {
    (void)v; return [self initWithDampingRatio:(m > 0 && k > 0) ? c / (2 * sqrt(k * m)) : 1];
}
- (id)copyWithZone:(NSZone *)z { (void)z; return [self retain]; }
- (CGVector)initialVelocity { CGVector v = {0, 0}; return v; }
- (NSInteger)timingCurveType { return 2; }
- (id)cubicTimingParameters { return nil; }
- (id)springTimingParameters { return self; }
@end

/* ================================================================ UIViewPropertyAnimator */
@interface UIViewPropertyAnimator : NSObject <NSCopying> {
@public
    double _dur, _delay, _pendingFraction;
    int _type; double _cp[4], _z; BOOL _cpSet;
    NSInteger _curve;
    NSMutableArray *_anims, *_completions;
    _MOSAnimGroup *_group;          /* retido */
    NSInteger _state;               /* 0 inativo, 1 ativo, 2 parado */
    BOOL _reversed, _pausesOnCompletion, _interruptible, _fractionSet;
}
@end
static void anim_finish_animator(UIViewPropertyAnimator *a, NSInteger pos);
static void group_finish(_MOSAnimGroup *g, NSInteger position) {
    if (g->_done) return;
    if (g->_animator && ((UIViewPropertyAnimator *)g->_animator)->_pausesOnCompletion) { g->_paused = YES; return; }
    g->_done = YES;
    [[g retain] autorelease];
    if (position == 1) group_commit(g, 0);                                /* terminou no começo (invertido) */
    rm_ident(g_groups, g);
    if (g->_completion) g->_completion(YES);
    if (g->_animator) anim_finish_animator(g->_animator, position);
}
@implementation UIViewPropertyAnimator
- (instancetype)init { if ((self = [super init])) { _anims = [[NSMutableArray alloc] init]; _completions = [[NSMutableArray alloc] init]; _interruptible = YES; } return self; }
- (instancetype)initWithDuration:(NSTimeInterval)d curve:(NSInteger)c animations:(void (^)(void))a {
    if ((self = [self init])) { _dur = d; _curve = c; if (a) [self addAnimations:a]; }
    return self;
}
- (instancetype)initWithDuration:(NSTimeInterval)d controlPoint1:(CGPoint)p1 controlPoint2:(CGPoint)p2 animations:(void (^)(void))a {
    if ((self = [self initWithDuration:d curve:0 animations:a])) { _cp[0] = p1.x; _cp[1] = p1.y; _cp[2] = p2.x; _cp[3] = p2.y; _cpSet = YES; }
    return self;
}
- (instancetype)initWithDuration:(NSTimeInterval)d dampingRatio:(CGFloat)z animations:(void (^)(void))a {
    if ((self = [self initWithDuration:d curve:0 animations:a])) { _type = 1; _z = z; }
    return self;
}
- (instancetype)initWithDuration:(NSTimeInterval)d timingParameters:(id)p {
    if ((self = [self initWithDuration:d curve:0 animations:nil])) {
        if ([p isKindOfClass:[UISpringTimingParameters class]]) { _type = 1; _z = ((UISpringTimingParameters *)p)->_z; }
        else if ([p isKindOfClass:[UICubicTimingParameters class]]) {
            UICubicTimingParameters *c = p;
            if (c->_custom) { _cp[0] = c->_c1.x; _cp[1] = c->_c1.y; _cp[2] = c->_c2.x; _cp[3] = c->_c2.y; _cpSet = YES; }
            else _curve = c->_curve;
        }
    }
    return self;
}
+ (instancetype)runningPropertyAnimatorWithDuration:(NSTimeInterval)d delay:(NSTimeInterval)dl options:(NSUInteger)o
                                         animations:(void (^)(void))a completion:(void (^)(NSInteger))c {
    UIViewPropertyAnimator *p = [[[self alloc] initWithDuration:d curve:(NSInteger)((o >> 16) & 0xf) animations:a] autorelease];
    if (c) [p addCompletion:c];
    [p startAnimationAfterDelay:dl];
    return p;
}
- (void)dealloc { [_anims release]; [_completions release]; if (_group) _group->_animator = nil; [_group release]; [super dealloc]; }
- (id)copyWithZone:(NSZone *)z { (void)z; return [self retain]; }
- (void)addAnimations:(void (^)(void))a {
    if (!a) return;
    if (_state == 1 && _group) {                                          /* já rodando: entra no grupo atual */
        _MOSAnimGroup *g = capture(a);
        for (_MOSAnimRun *r in g->_runs) {
            /* a fração já andada vira o ponto de partida */
            [_group->_runs addObject:r];
        }
        return;
    }
    [_anims addObject:[[a copy] autorelease]];
}
- (void)addAnimations:(void (^)(void))a delayFactor:(CGFloat)f { (void)f; [self addAnimations:a]; }
- (void)addCompletion:(void (^)(NSInteger))c { if (c) [_completions addObject:[[c copy] autorelease]]; }
- (void)_mosActivate {
    if (_group) return;
    NSArray *as = [[_anims copy] autorelease];
    [_anims removeAllObjects];
    _MOSAnimGroup *g = capture(^{ for (void (^b)(void) in as) b(); });
    if (!g) g = [[[_MOSAnimGroup alloc] init] autorelease];
    if (_type == 1) { g->_type = 1; g->_z = _z; }
    else if (_cpSet) memcpy(g->_cp, _cp, sizeof _cp);
    else curve_named(g, _curve);
    g->_dur = _dur;
    g->_animator = self;
    g->_reversed = _reversed;
    g->_paused = YES;
    if (_fractionSet) g->_fraction = _pendingFraction;
    _group = [g retain];
    group_add(g);
    _state = 1;
}
- (void)startAnimation { [self startAnimationAfterDelay:0]; }
- (void)startAnimationAfterDelay:(NSTimeInterval)d {
    if (_state == 2) return;
    [self _mosActivate];
    _group->_delayLeft = d > 0 ? d : 0;
    _group->_last = mos_clock();
    _group->_paused = NO;
}
- (void)pauseAnimation { if (_state == 2) return; [self _mosActivate]; _group->_paused = YES; }
- (void)stopAnimation:(BOOL)withoutFinishing {
    if (!_group || _state != 1) return;
    group_commit(_group, _group->_fraction);                              /* o modelo fica onde a apresentação estava */
    _group->_done = YES;
    _group->_animator = nil;
    rm_ident(g_groups, _group);
    _state = withoutFinishing ? 0 : 2;
    if (withoutFinishing) { [_group release]; _group = nil; }
    mos_redraw();
}
- (void)finishAnimationAtPosition:(NSInteger)pos {
    if (_state != 2 && _group) { [self stopAnimation:NO]; }
    if (_group) {
        if (pos == 0) group_commit(_group, 1); else if (pos == 1) group_commit(_group, 0);
    }
    anim_finish_animator(self, pos);
}
- (void)continueAnimationWithTimingParameters:(id)p durationFactor:(CGFloat)f {
    (void)p;
    if (!_group) return;
    if (f > 0) _group->_dur = _dur * f;
    _group->_last = mos_clock();
    _group->_paused = NO;
}
- (CGFloat)fractionComplete { return _group ? _group->_fraction : _pendingFraction; }
- (void)setFractionComplete:(CGFloat)f {
    f = f < 0 ? 0 : f > 1 ? 1 : f;
    if (_group) { _group->_fraction = f; mos_redraw(); } else { _pendingFraction = f; _fractionSet = YES; }
}
- (BOOL)isReversed { return _reversed; }
- (void)setReversed:(BOOL)r { _reversed = r; if (_group) _group->_reversed = r; }
- (NSInteger)state { return _state; }
- (BOOL)isRunning { return _group && _state == 1 && !_group->_paused && !_group->_done; }
- (NSTimeInterval)duration { return _dur; }
- (NSTimeInterval)delay { return _delay; }
- (BOOL)isInterruptible { return _interruptible; }
- (void)setInterruptible:(BOOL)i { _interruptible = i; }
- (BOOL)pausesOnCompletion { return _pausesOnCompletion; }
- (void)setPausesOnCompletion:(BOOL)p { _pausesOnCompletion = p; }
- (BOOL)scrubsLinearly { return YES; }
- (void)setScrubsLinearly:(BOOL)s { (void)s; }
- (BOOL)isUserInteractionEnabled { return YES; }
- (void)setUserInteractionEnabled:(BOOL)e { (void)e; }
- (BOOL)isManualHitTestingEnabled { return NO; }
- (void)setManualHitTestingEnabled:(BOOL)e { (void)e; }
- (id)timingParameters { return [[[UICubicTimingParameters alloc] initWithAnimationCurve:_curve] autorelease]; }
@end
static void anim_finish_animator(UIViewPropertyAnimator *a, NSInteger pos) {
    [[a retain] autorelease];
    a->_state = 0;
    if (a->_group) { a->_group->_animator = nil; [a->_group release]; a->_group = nil; }
    NSArray *cs = [[a->_completions copy] autorelease];
    [a->_completions removeAllObjects];
    for (void (^c)(NSInteger) in cs) c(pos);
    mos_redraw();
}
