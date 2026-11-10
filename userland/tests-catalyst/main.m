/* mOS — amostra Catalyst mínima para o UIKit.framework do mOS (userland/frameworks/UIKit): app com delegate de cena,
 * como um app UIKit moderno. UIApplicationMain -> application:didFinishLaunching -> configuração da cena ->
 * scene:willConnectToSession: -> UIWindow + UIViewController com uma faixa colorida e um UILabel.
 *   ver tools/test-catalyst.ps1 (compila contra o UIKit do Catalyst, roda no harness e grava a tela em BMP)
 * Imprime "catalyst: N/N ok" quando a cena fica ativa; o laço do NSApplication segue até o watchdog do harness. */
#import "../frameworks/UIKit/UIKitMOS.h"
#include <stdio.h>
#include <math.h>

int UIApplicationMain(int argc, char *argv[], NSString *principal, NSString *delegate);
extern NSString *const UIWindowSceneSessionRoleApplication;

static int ok, total;
#define CHECK(c, ...) do { total++; if (c) { ok++; printf("  ok   "); } else printf("  FALHA "); printf(__VA_ARGS__); printf("\n"); fflush(stdout); } while (0)
static int launched, connected;
static int toques, cliques, gestos, mudou, hovers;
@interface HoverAlvo : NSObject
@end
@implementation HoverAlvo
- (void)mosHover:(id)g { (void)g; hovers++; }
@end

/* toques e teclado como o AppKit os entrega à NSView hospedeira (UIKitEvents.m) */
@interface _UIHostingView (MOSEvents)
- (void)_mosTouchDownAt:(CGPoint)p clickCount:(NSInteger)n;
- (void)_mosTouchUpAt:(CGPoint)p;
- (void)_mosTypeText:(NSString *)s;
- (void)_mosScrollAt:(CGPoint)p dy:(CGFloat)dy;
- (void)_mosPointerMovedTo:(CGPoint)p;
@end
static void clicar(UIWindow *w, CGPoint p) {
    _UIHostingView *h = (_UIHostingView *)w->_nsWindow.contentView;
    [h _mosTouchDownAt:p clickCount:1];
    [h _mosTouchUpAt:p];
}

/* Auto Layout por âncoras (as classes de restrição do AppKit do mOS) e o que o UIKitMOS.h não declara */
@interface NSIndexPath : NSObject
+ (instancetype)indexPathForRow:(NSInteger)r inSection:(NSInteger)s;
- (NSInteger)row;
@end
@interface NSObject (MOSAnchors)
- (instancetype)initWithStyle:(NSInteger)s reuseIdentifier:(NSString *)r;
- (instancetype)initWithRootViewController:(id)vc;
- (void)setTranslatesAutoresizingMaskIntoConstraints:(BOOL)t;
- (void)setDataSource:(id)d;
- (void)setDelegate:(id)d;
- (void)setTitle:(NSString *)t;
- (id)leadingAnchor; - (id)topAnchor; - (id)widthAnchor; - (id)heightAnchor;
- (id)constraintEqualToAnchor:(id)a constant:(CGFloat)k;
- (id)constraintEqualToAnchor:(id)a multiplier:(CGFloat)m;
- (id)constraintEqualToConstant:(CGFloat)k;
- (void)setActive:(BOOL)a;
- (void)_mosSolveLayout;
- (NSIndexPath *)indexPathForSelectedRow;
- (id)cellForRowAtIndexPath:(NSIndexPath *)ip;
- (id)topViewController;
- (id)navigationController;
- (id)hitTest:(CGPoint)p withEvent:(id)e;
- (id)trailingAnchor;
- (id)constraintLessThanOrEqualToConstant:(CGFloat)k;
- (void)setPriority:(float)p;
- (void)presentViewController:(id)vc animated:(BOOL)a completion:(void (^)(void))c;
- (void)dismissViewControllerAnimated:(BOOL)a completion:(void (^)(void))c;
- (id)presentedViewController;
- (id)presentingViewController;
+ (id)alertControllerWithTitle:(NSString *)t message:(NSString *)m preferredStyle:(NSInteger)s;
+ (id)actionWithTitle:(NSString *)t style:(NSInteger)s handler:(void (^)(id))h;
- (void)addAction:(id)a;
+ (void)animateWithDuration:(double)d animations:(void (^)(void))a completion:(void (^)(BOOL))c;
+ (void)animateWithDuration:(double)d delay:(double)dl options:(NSUInteger)o animations:(void (^)(void))a completion:(void (^)(BOOL))c;
+ (void)animateWithDuration:(double)d delay:(double)dl usingSpringWithDamping:(CGFloat)z initialSpringVelocity:(CGFloat)v options:(NSUInteger)o animations:(void (^)(void))a completion:(void (^)(BOOL))c;
+ (void)transitionWithView:(id)v duration:(double)d options:(NSUInteger)o animations:(void (^)(void))a completion:(void (^)(BOOL))c;
+ (void)_mosAdvanceAnimationClock:(double)s;
+ (NSUInteger)_mosRunningAnimationCount;
- (CGRect)_mosPresentationFrame;
- (CGFloat)_mosPresentationAlpha;
- (instancetype)initWithDuration:(double)d curve:(NSInteger)c animations:(void (^)(void))a;
- (void)addCompletion:(void (^)(NSInteger))c;
- (void)pauseAnimation;
- (void)startAnimation;
- (void)stopAnimation:(BOOL)w;
- (void)setFractionComplete:(CGFloat)f;
- (NSInteger)state;
@end
const char *object_getClassName(id o);
extern CGAffineTransform CGAffineTransformMakeRotation(CGFloat a);
extern CGAffineTransform CGAffineTransformMakeScale(CGFloat sx, CGFloat sy);
extern CGAffineTransform CGAffineTransformMakeTranslation(CGFloat tx, CGFloat ty);
extern CATransform3D CATransform3DMakeScale(CGFloat x, CGFloat y, CGFloat z);
extern CATransform3D CATransform3DMakeRotation(CGFloat a, CGFloat x, CGFloat y, CGFloat z);
extern void UIGraphicsBeginImageContextWithOptions(CGSize size, BOOL opaque, CGFloat scale);
extern void *UIGraphicsGetCurrentContext(void);
extern void UIGraphicsEndImageContext(void);
extern void *CGBitmapContextGetData(void *);
extern size_t CGBitmapContextGetBytesPerRow(void *);
@interface UIView (MOSRender)
- (void)_mosRender:(void *)c;
@end
@interface CALayer : NSObject
- (void)setTransform:(CATransform3D)t;
- (CATransform3D)transform;
@end
static int px_at(UIView *v, int x, int y, unsigned char o[4]) {
    o[0] = o[1] = o[2] = o[3] = 0;
    CGSize s = v.frame.size;
    if (s.width < 1 || s.height < 1 || x < 0 || y < 0 || x >= (int)s.width || y >= (int)s.height) return 0;
    UIGraphicsBeginImageContextWithOptions(s, NO, 1);
    void *c = UIGraphicsGetCurrentContext();
    if (c) [v _mosRender:c];
    unsigned char *d = c ? (unsigned char *)CGBitmapContextGetData(c) : 0;
    size_t bpr = c ? CGBitmapContextGetBytesPerRow(c) : 0;
    int okp = d && bpr;
    if (okp) { unsigned char *p = d + (size_t)y * bpr + (size_t)x * 4; o[0] = p[0]; o[1] = p[1]; o[2] = p[2]; o[3] = p[3]; }
    UIGraphicsEndImageContext();
    return okp;
}
static int linhasPedidas, selecionou;
@interface Fonte : NSObject
@end
@implementation Fonte
- (NSInteger)numberOfSectionsInTableView:(UITableView *)t { return 1; }
- (NSInteger)tableView:(UITableView *)t numberOfRowsInSection:(NSInteger)s { return 30; }
- (UITableViewCell *)tableView:(UITableView *)t cellForRowAtIndexPath:(NSIndexPath *)ip {
    linhasPedidas++;
    UITableViewCell *c = (UITableViewCell *)[[UITableViewCell alloc] initWithStyle:0 reuseIdentifier:@"c"];
    [c.textLabel performSelector:@selector(setText:) withObject:[NSString stringWithFormat:@"Linha %d", (int)[ip row]]];
    return c;
}
- (void)tableView:(UITableView *)t didSelectRowAtIndexPath:(NSIndexPath *)ip { selecionou = (int)[ip row] + 1; }
@end
static Fonte *fonte;

@interface Alvo : NSObject
@end
@implementation Alvo
- (void)botao:(id)s { cliques++; }
- (void)toque:(id)g { gestos++; }
- (void)mudanca:(id)s { mudou++; }
@end
static Alvo *alvo;

@interface Tela : UIViewController
@end
@implementation Tela
- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = [UIColor systemBackgroundColor];
    UIView *faixa = [[UIView alloc] initWithFrame:CGRectMake(20, 20, 300, 60)];
    faixa.backgroundColor = [UIColor systemGreenColor];
    faixa.tag = 7;
    [self.view addSubview:faixa];
    UILabel *l = [[UILabel alloc] initWithFrame:CGRectMake(20, 100, 400, 30)];
    [l performSelector:@selector(setText:) withObject:@"Olá do UIKit do mOS"];
    [self.view addSubview:l];
    alvo = [Alvo new];
    UIButton *b = [UIButton buttonWithType:1];
    b.frame = CGRectMake(20, 150, 120, 30);
    b.tag = 8;
    [b performSelector:@selector(setTitle:forState:) withObject:@"Enviar" withObject:nil];
    [b addTarget:alvo action:@selector(botao:) forControlEvents:1 << 6];
    [self.view addSubview:b];
    UITapGestureRecognizer *tap = [[UITapGestureRecognizer alloc] initWithTarget:alvo action:@selector(toque:)];
    [faixa performSelector:@selector(addGestureRecognizer:) withObject:tap];
    UITextField *tf = [[UITextField alloc] initWithFrame:CGRectMake(20, 200, 200, 26)];
    tf.tag = 9;
    [tf performSelector:@selector(setPlaceholder:) withObject:@"Mensagem"];
    [tf addTarget:alvo action:@selector(mudanca:) forControlEvents:1 << 17];
    [self.view addSubview:tf];
    UISwitch *sw = [[UISwitch alloc] initWithFrame:CGRectMake(20, 240, 0, 0)];
    sw.tag = 10;
    [self.view addSubview:sw];
    UIView *vazio = [[UIView alloc] initWithFrame:CGRectMake(400, 300, 50, 50)];
    vazio.tag = 11;
    vazio.alpha = 0.5;
    vazio.backgroundColor = [UIColor systemBlueColor];
    [self.view addSubview:vazio];
}
- (void)touchesBegan:(NSSet *)t withEvent:(UIEvent *)e { toques++; }
@end

@interface Cena : UIResponder
@property (strong) UIWindow *window;
@end
@implementation Cena
- (void)scene:(UIScene *)scene willConnectToSession:(UISceneSession *)s options:(id)o {
    connected = 1;
    UIWindow *w = [[UIWindow alloc] initWithWindowScene:(UIWindowScene *)scene];
    w.rootViewController = [[Tela alloc] init];
    self.window = w;
    [w makeKeyAndVisible];
}
- (void)sceneDidBecomeActive:(UIScene *)scene {
    UIWindow *w = self.window;
    UIApplication *app = [UIApplication sharedApplication];
    CHECK(launched, "application:didFinishLaunchingWithOptions:");
    CHECK(connected, "scene:willConnectToSession:options:");
    CHECK([app.delegate isKindOfClass:NSClassFromString(@"App")], "UIApplication.delegate");
    CHECK([[app performSelector:@selector(connectedScenes)] count] == 1, "uma cena conectada");
    CHECK(w && [w isKeyWindow] && w->_nsWindow != nil, "UIWindow visível sobre uma NSWindow");
    UIView *root = w.rootViewController.view;
    CHECK(root.superview == w && root.window == w, "a view do rootViewController está na janela");
    UIView *faixa = [root viewWithTag:7];
    CHECK(faixa && faixa.backgroundColor && faixa.frame.size.width == 300, "subview com cor de fundo");
    CGPoint p = [faixa convertPoint:CGPointMake(0, 0) toView:nil];
    CHECK(p.x == 20 && p.y == 20, "convertPoint:toView:nil (%g, %g)", p.x, p.y);
    CHECK([UIDevice currentDevice] && [[UIScreen mainScreen] bounds].size.width > 0, "UIDevice e UIScreen");
    /* entrada: cliques no botão e na faixa, cadeia de resposta, teclado no campo de texto, interruptor */
    UIButton *b = (UIButton *)[root viewWithTag:8];
    clicar(w, CGPointMake(60, 160));
    CHECK(cliques == 1 && !b->_highlighted, "clique no UIButton dispara TouchUpInside (alvo/ação)");
    clicar(w, CGPointMake(100, 40));
    CHECK(gestos == 1, "UITapGestureRecognizer na faixa");
    clicar(w, CGPointMake(420, 320));
    CHECK(toques >= 1, "touchesBegan sobe da view até o view controller (%d)", toques);
    UITextField *tf = (UITextField *)[root viewWithTag:9];
    clicar(w, CGPointMake(60, 210));
    CHECK(tf->_editing && w->_firstResponder == tf, "toque no UITextField o torna primeiro respondedor");
    _UIHostingView *h = (_UIHostingView *)w->_nsWindow.contentView;
    [h _mosTypeText:@"o"]; [h _mosTypeText:@"i"]; [h _mosTypeText:@"x"]; [h _mosTypeText:@"\x7f"];
    CHECK([tf->_text isEqualToString:@"oi"] && mudou == 4, "teclado no UITextField (texto '%s', %d mudanças)", [tf->_text UTF8String], mudou);
    UISwitch *sw = (UISwitch *)[root viewWithTag:10];
    clicar(w, CGPointMake(30, 250));
    CHECK(sw->_on, "clique alterna o UISwitch");
    /* Auto Layout: filho preso ao contêiner, altura = 2 x largura */
    UIView *caixa = [[UIView alloc] initWithFrame:CGRectMake(500, 20, 200, 200)];
    UIView *filho = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, 0)];
    [filho setTranslatesAutoresizingMaskIntoConstraints:NO];
    [caixa addSubview:filho];
    [root addSubview:caixa];
    [[[filho leadingAnchor] constraintEqualToAnchor:[caixa leadingAnchor] constant:10] setActive:YES];
    [[[filho topAnchor] constraintEqualToAnchor:[caixa topAnchor] constant:20] setActive:YES];
    [[[filho widthAnchor] constraintEqualToConstant:50] setActive:YES];
    [[[filho heightAnchor] constraintEqualToAnchor:[filho widthAnchor] multiplier:2] setActive:YES];
    [w _mosSolveLayout];
    CGRect ff = filho.frame;
    CHECK(ff.origin.x == 10 && ff.origin.y == 20 && ff.size.width == 50 && ff.size.height == 100,
          "Auto Layout por âncoras (%g, %g, %g, %g)", ff.origin.x, ff.origin.y, ff.size.width, ff.size.height);
    /* UITableView: só as células visíveis, rolagem pela roda, toque seleciona */
    fonte = [Fonte new];
    UITableView *tv = [[UITableView alloc] initWithFrame:CGRectMake(500, 240, 200, 220)];
    [root addSubview:tv];
    [tv setDataSource:fonte];
    [tv setDelegate:fonte];
    [tv layoutSubviews];
    CHECK(tv->_contentSize.height == 30 * 44 && linhasPedidas > 0 && linhasPedidas < 30,
          "UITableView pede só as células visíveis (%d de 30, conteúdo %g)", linhasPedidas, tv->_contentSize.height);
    [h _mosScrollAt:CGPointMake(550, 300) dy:440];
    CHECK([tv contentOffset].y == 440 && [tv cellForRowAtIndexPath:[NSIndexPath indexPathForRow:10 inSection:0]] != nil,
          "roda do mouse rola a tabela (offset %g)", [tv contentOffset].y);
    [h _mosScrollAt:CGPointMake(550, 300) dy:100000];
    CHECK([tv contentOffset].y == 30 * 44 - 220, "rolagem limitada ao conteúdo (%g)", [tv contentOffset].y);
    [tv setContentOffset:CGPointMake(0, 0)];
    clicar(w, CGPointMake(550, 240 + 44 * 2 + 10));
    CHECK(selecionou == 3 && [[tv indexPathForSelectedRow] row] == 2, "toque seleciona a linha (didSelectRow %d)", selecionou - 1);
    /* UINavigationController: push/pop e a barra com o botão voltar */
    UIViewController *v1 = [[UIViewController alloc] init], *v2 = [[UIViewController alloc] init];
    [v1 setTitle:@"Conversas"]; [v2 setTitle:@"Detalhe"];
    UINavigationController *nav = (UINavigationController *)[[UINavigationController alloc] initWithRootViewController:v1];
    UIView *nv = nav.view;
    nv.frame = CGRectMake(20, 300, 400, 200);
    [root addSubview:nv];
    [nav pushViewController:v2 animated:NO];
    CHECK([nav topViewController] == v2 && v2.view.superview == nv && v1.view.superview == nil && [v2 navigationController] == nav,
          "pushViewController troca a view do topo");
    CHECK(v2.view.frame.origin.y > 0 && nav->_bar.superview == nv, "conteúdo abaixo da UINavigationBar (y %g)", v2.view.frame.origin.y);
    clicar(w, CGPointMake(40, 300 + 15));
    CHECK([nav topViewController] == v1 && v1.view.superview == nv, "botão voltar da barra faz pop");
    /* prioridades: largura 150 opcional contra <= 100 obrigatório; altura 80 (250) perde para 40 (999) */
    UIView *pv = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, 0)];
    [pv setTranslatesAutoresizingMaskIntoConstraints:NO];
    [caixa addSubview:pv];
    id cw = [[pv widthAnchor] constraintEqualToConstant:150]; [cw setPriority:250]; [cw setActive:YES];
    [[[pv widthAnchor] constraintLessThanOrEqualToConstant:100] setActive:YES];
    id ch1 = [[pv heightAnchor] constraintEqualToConstant:80]; [ch1 setPriority:250]; [ch1 setActive:YES];
    id ch2 = [[pv heightAnchor] constraintEqualToConstant:40]; [ch2 setPriority:999]; [ch2 setActive:YES];
    [[[pv leadingAnchor] constraintEqualToAnchor:[caixa leadingAnchor] constant:0] setActive:YES];
    [[[pv topAnchor] constraintEqualToAnchor:[caixa topAnchor] constant:150] setActive:YES];
    [w _mosSolveLayout];
    CHECK(pv.frame.size.width == 100 && pv.frame.size.height == 40, "prioridades no Auto Layout (%g x %g)", pv.frame.size.width, pv.frame.size.height);
    /* modal: folha centralizada sobre a janela; dismiss tira e chama o completion */
    UIViewController *tela = w.rootViewController;
    UIViewController *folha = [[UIViewController alloc] init];
    __block int prontos = 0;
    [tela presentViewController:folha animated:YES completion:^{ prontos++; }];
    CGRect fr = folha.view.frame;
    CHECK([tela presentedViewController] == folha && [folha presentingViewController] == tela && (id)[(id)folha.view window] == (id)w
          && fr.size.width == 540 && fr.origin.x == (900 - 540) / 2 && prontos == 1,
          "presentViewController: folha na janela (%g, %g, %g, %g)", fr.origin.x, fr.origin.y, fr.size.width, fr.size.height);
    [folha dismissViewControllerAnimated:YES completion:^{ prontos++; }];
    CHECK(folha.view.superview == nil && [tela presentedViewController] == nil && prontos == 2, "dismissViewController fecha a folha");
    /* alerta: dois botões; o toque em "Apagar" chama o handler e fecha */
    __block int apagou = 0;
    id alerta = [NSClassFromString(@"UIAlertController") alertControllerWithTitle:@"Apagar conversa?" message:@"Não dá para desfazer." preferredStyle:1];
    Class acao = NSClassFromString(@"UIAlertAction");
    [alerta addAction:[acao actionWithTitle:@"Cancelar" style:1 handler:nil]];
    [alerta addAction:[acao actionWithTitle:@"Apagar" style:2 handler:^(id a) { apagou++; }]];
    [tela presentViewController:alerta animated:YES completion:nil];
    UIView *bApagar = (UIView *)[(UIView *)[alerta view] viewWithTag:9001];
    CGPoint pa = [bApagar convertPoint:CGPointMake(10, 10) toView:nil];
    clicar(w, pa);
    CHECK(bApagar && apagou == 1 && [tela presentedViewController] == nil, "UIAlertController: toque na ação chama o handler e fecha");
    /* animações: o modelo vai direto ao final; a apresentação anda no relógio */
    UIView *bola = [[UIView alloc] initWithFrame:CGRectMake(600, 500, 50, 50)];
    bola.backgroundColor = [UIColor systemBlueColor];
    [root addSubview:bola];
    __block int fim = 0;
    [UIView animateWithDuration:1 animations:^{ bola.frame = CGRectMake(700, 500, 50, 50); bola.alpha = 0; } completion:^(BOOL f) { fim = f ? 1 : -1; }];
    CHECK(bola.frame.origin.x == 700 && [bola _mosPresentationFrame].origin.x == 600, "animate: modelo final, apresentação no início");
    [UIView _mosAdvanceAnimationClock:0.5];
    CGFloat meio = [bola _mosPresentationFrame].origin.x;
    CHECK(fabs(meio - 650) < 1 && fabs([bola _mosPresentationAlpha] - 0.5) < 0.01 && fim == 0, "animate: meio da curva easeInOut (x %g)", meio);
    [UIView _mosAdvanceAnimationClock:0.6];
    CHECK(fim == 1 && [UIView _mosRunningAnimationCount] == 0 && [bola _mosPresentationFrame].origin.x == 700, "animate: completion no fim");
    [UIView animateWithDuration:0.5 delay:0 usingSpringWithDamping:0.5 initialSpringVelocity:0 options:0
                     animations:^{ bola.frame = CGRectMake(700, 600, 50, 50); } completion:nil];
    [UIView _mosAdvanceAnimationClock:0.125];
    CGFloat ym = [bola _mosPresentationFrame].origin.y;
    CHECK(ym > 600, "mola com dampingRatio 0,5 passa do alvo (y %g)", ym);
    [UIView _mosAdvanceAnimationClock:0.5];
    /* UIViewPropertyAnimator: fração manual, retomada, parada no meio */
    __block NSInteger posFim = -1;
    id an = [[NSClassFromString(@"UIViewPropertyAnimator") alloc] initWithDuration:1 curve:3 animations:^{ bola.alpha = 1; }];
    [an addCompletion:^(NSInteger p) { posFim = p; }];
    [an pauseAnimation];
    [an setFractionComplete:0.25];
    CGFloat a25 = [bola _mosPresentationAlpha];
    [an startAnimation];
    [UIView _mosAdvanceAnimationClock:0.8];
    CHECK(fabs(a25 - 0.25) < 0.01 && posFim == 0 && bola.alpha == 1, "UIViewPropertyAnimator: fração e conclusão (%g)", a25);
    id an2 = [[NSClassFromString(@"UIViewPropertyAnimator") alloc] initWithDuration:1 curve:3 animations:^{ bola.alpha = 0; }];
    [an2 startAnimation];
    [UIView _mosAdvanceAnimationClock:0.5];
    [an2 stopAnimation:YES];
    CHECK(fabs(bola.alpha - 0.5) < 0.05 && [an2 state] == 0 && [UIView _mosRunningAnimationCount] == 0,
          "stopAnimation deixa o modelo onde estava (alpha %g)", bola.alpha);
    /* transform: rotação e escala em volta do centro; o frame vira a caixa alinhada; a identidade continua exata */
    UIView *giro = [[UIView alloc] initWithFrame:CGRectMake(100, 100, 40, 20)];
    giro.transform = CGAffineTransformMakeRotation(3.141592653589793 / 2);
    CGRect gf = giro.frame;
    CHECK(fabs(giro.center.x - 120) < 0.01 && fabs(giro.center.y - 110) < 0.01 && giro.bounds.size.width == 40 && giro.bounds.size.height == 20,
          "rotação não mexe no centro nem nos bounds");
    CHECK(fabs(gf.origin.x - 110) < 0.01 && fabs(gf.origin.y - 90) < 0.01 && fabs(gf.size.width - 20) < 0.01 && fabs(gf.size.height - 40) < 0.01,
          "rotação de 90°: frame é a AABB (%g, %g, %g, %g)", gf.origin.x, gf.origin.y, gf.size.width, gf.size.height);
    giro.transform = CGAffineTransformMakeScale(2, 2);
    gf = giro.frame;
    CHECK(fabs(giro.center.x - 120) < 0.01 && fabs(gf.origin.x - 80) < 0.01 && fabs(gf.origin.y - 90) < 0.01 && fabs(gf.size.width - 80) < 0.01 && fabs(gf.size.height - 40) < 0.01,
          "escala 2: AABB 80x40 com o mesmo centro (%g, %g, %g, %g)", gf.origin.x, gf.origin.y, gf.size.width, gf.size.height);
    giro.transform = CGAffineTransformMakeTranslation(0, 0);
    CHECK(giro.frame.origin.x == 100 && giro.frame.size.width == 40, "transform identidade devolve o frame exato");
    /* translação entra na matriz e não no center; o hit-test acompanha o retângulo visual */
    UIView *host = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 400, 300)];
    UIView *caixaT = [[UIView alloc] initWithFrame:CGRectMake(100, 100, 40, 20)];
    [host addSubview:caixaT];
    caixaT.transform = CGAffineTransformMakeTranslation(50, 0);
    CGPoint qt = [caixaT convertPoint:CGPointMake(0, 0) toView:host];
    CHECK(fabs(caixaT.center.x - 120) < 0.01 && fabs(qt.x - 150) < 0.01 && fabs(qt.y - 100) < 0.01,
          "translação não muda o center; convertPoint aplica (%g, %g)", qt.x, qt.y);
    CHECK([host hitTest:CGPointMake(170, 110) withEvent:nil] == caixaT, "hitTest acerta o centro visual da translação");
    CHECK([host hitTest:CGPointMake(120, 110) withEvent:nil] == host, "hitTest erra o centro antigo depois da translação");
    /* CATransform3D no layer espelha a view */
    UIView *esc = [[UIView alloc] initWithFrame:CGRectMake(100, 100, 40, 20)];
    CATransform3D t3 = CATransform3DMakeScale(2, 3, 1);
    [(CALayer *)esc.layer setTransform:t3];
    CATransform3D volta = [(CALayer *)esc.layer transform];
    CATransform3D daView = esc.transform3D;
    gf = esc.frame;
    CHECK(fabs(gf.size.width - 80) < 0.01 && fabs(gf.size.height - 60) < 0.01 && fabs(esc.center.x - 120) < 0.01,
          "CATransform3D de escala: frame %g x %g", gf.size.width, gf.size.height);
    CHECK(fabs(volta.m11 - 2) < 0.01 && fabs(volta.m22 - 3) < 0.01 && fabs(daView.m11 - 2) < 0.01 && fabs(daView.m22 - 3) < 0.01,
          "layer.transform e transform3D da view batem (m11 %g m22 %g)", volta.m11, volta.m22);
    t3 = CATransform3DMakeRotation(3.141592653589793 / 2, 0, 0, 1);
    [esc setTransform3D:t3];
    volta = [(CALayer *)esc.layer transform];
    CHECK(fabs(volta.m11) < 0.01 && fabs(volta.m12 - 1) < 0.01 && fabs(volta.m21 + 1) < 0.01 && fabs(volta.m22) < 0.01,
          "rotação Z no layer (m11 %g m12 %g m21 %g m22 %g)", volta.m11, volta.m12, volta.m21, volta.m22);
    /* bitmap: a translação desloca o preenchimento; o alfa do ancestral entra no pixel */
    UIView *palco = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 80, 40)];
    UIView *miolo = [[UIView alloc] initWithFrame:CGRectMake(10, 10, 20, 20)];
    miolo.backgroundColor = [UIColor colorWithRed:1 green:0 blue:0 alpha:1];
    [palco addSubview:miolo];
    miolo.transform = CGAffineTransformMakeTranslation(30, 0);
    unsigned char px[4];
    int tem = px_at(palco, 15, 15, px);
    unsigned char px2[4];
    int tem2 = px_at(palco, 50, 20, px2);
    CHECK(tem && px[3] == 0 && tem2 && px2[0] > 200 && px2[3] > 200, "bitmap: translação tira o pixel antigo e pinta o visual (A %u, R %u)", px[3], px2[0]);
    UIView *pai = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 40, 40)];
    pai.alpha = 0.5;
    UIView *filhoA = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 40, 40)];
    filhoA.backgroundColor = [UIColor colorWithRed:1 green:0 blue:0 alpha:1];
    [pai addSubview:filhoA];
    unsigned char pxa[4];
    int tema = px_at(pai, 20, 20, pxa);
    CHECK(tema && pxa[0] > 200 && pxa[3] > 100 && pxa[3] < 160, "alfa do ancestral multiplica o preenchimento (A %u)", pxa[3]);
    /* animação do transform: o modelo já é a escala final; a apresentação anda */
    UIView *cresce = [[UIView alloc] initWithFrame:CGRectMake(800, 20, 40, 20)];
    [root addSubview:cresce];
    [UIView animateWithDuration:1 delay:0 options:(3u << 16) animations:^{ cresce.transform = CGAffineTransformMakeScale(3, 1); } completion:nil];
    CHECK(fabs(cresce.frame.size.width - 120) < 0.01 && fabs([cresce _mosPresentationFrame].size.width - 40) < 1,
          "animate do transform: modelo na escala final, apresentação no início");
    [UIView _mosAdvanceAnimationClock:0.5];
    CHECK(fabs([cresce _mosPresentationFrame].size.width - 80) < 2, "animate do transform no meio (largura %g)", [cresce _mosPresentationFrame].size.width);
    [UIView _mosAdvanceAnimationClock:0.6];
    /* contentOffset animado: 0,25 s linear, com teto do conteúdo */
    UIScrollView *sc = [[UIScrollView alloc] initWithFrame:CGRectMake(20, 400, 100, 100)];
    sc->_contentSize = CGSizeMake(100, 500);
    [root addSubview:sc];
    [sc setContentOffset:CGPointMake(0, 200) animated:YES];
    CHECK(sc.contentOffset.y == 200 && [sc _mosPresentationContentOffset].y == 0, "contentOffset animado: modelo no fim, apresentação no início");
    [UIView _mosAdvanceAnimationClock:0.125];
    CGFloat oy = [sc _mosPresentationContentOffset].y;
    CHECK(fabs(oy - 100) < 8, "contentOffset animado no meio (%g)", oy);
    [UIView _mosAdvanceAnimationClock:0.2];
    CHECK(fabs([sc _mosPresentationContentOffset].y - 200) < 0.01 && [UIView _mosRunningAnimationCount] == 0, "contentOffset animado termina");
    [sc setContentOffset:CGPointMake(0, 10000) animated:YES];
    CHECK(sc.contentOffset.y == 400, "contentOffset animado respeita o teto do conteúdo (%g)", sc.contentOffset.y);
    [UIView _mosAdvanceAnimationClock:0.3];
    [UIView animateWithDuration:1 delay:0 options:(3u << 16) animations:^{ [sc setContentOffset:CGPointMake(0, 0)]; } completion:nil];
    [UIView _mosAdvanceAnimationClock:0.5];
    CHECK(fabs([sc _mosPresentationContentOffset].y - 200) < 8, "animateWithDuration do contentOffset no meio (%g)", [sc _mosPresentationContentOffset].y);
    [UIView _mosAdvanceAnimationClock:0.6];
    /* transitionWithView: o quadro 0 é o snapshot de antes, não o modelo que já está no fim */
    UIView *troca = [[UIView alloc] initWithFrame:CGRectMake(20, 520, 40, 40)];
    troca.backgroundColor = [UIColor colorWithRed:1 green:0 blue:0 alpha:1];
    [root addSubview:troca];
    __block int trocou = 0;
    [UIView transitionWithView:troca duration:1 options:(3u << 16) | (5u << 20) animations:^{
        troca.backgroundColor = [UIColor colorWithRed:0 green:0 blue:1 alpha:1];
    } completion:^(BOOL f) { trocou = f ? 1 : -1; }];
    unsigned char p0[4], p1[4];
    int t0 = px_at(troca, 20, 20, p0);
    [UIView _mosAdvanceAnimationClock:0.5];
    int t1 = px_at(troca, 20, 20, p1);
    CHECK(t0 && p0[0] > 200 && p0[2] < 40, "transitionWithView no início é o snapshot vermelho (R %u B %u)", p0[0], p0[2]);
    CHECK(t1 && p1[0] > 80 && p1[0] < 180 && p1[2] > 80 && p1[2] < 180, "transitionWithView no meio cruza as cores (R %u B %u)", p1[0], p1[2]);
    [UIView _mosAdvanceAnimationClock:0.6];
    CHECK(trocou == 1 && [UIView _mosRunningAnimationCount] == 0, "transitionWithView chama o completion");
    [w->_nsWindow.contentView setNeedsDisplay:YES];
    extern const NSInteger UIUserInterfaceIdiomMac;
    CHECK(UIUserInterfaceIdiomMac == 5 && [[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomMac,
          "UIUserInterfaceIdiomMac");
    UIViewController *abaA = [[UIViewController alloc] init];
    [abaA setTitle:@"Um"];
    UIViewController *abaB = [[UIViewController alloc] init];
    [abaB setTitle:@"Dois"];
    id tabs = [[NSClassFromString(@"UITabBarController") alloc] init];
    [tabs setViewControllers:@[abaA, abaB] animated:NO];
    [[tabs view] setFrame:CGRectMake(0, 0, 400, 300)];
    [tabs viewDidLayoutSubviews];
    CHECK([tabs selectedIndex] == 0 && [tabs selectedViewController] == abaA && abaA.view.superview != nil,
          "UITabBarController mostra a primeira aba");
    [tabs setSelectedIndex:1];
    CHECK([tabs selectedViewController] == abaB && abaB.view.superview != nil && abaA.view.superview == nil, "troca de aba");
    UIViewController *pri = [[UIViewController alloc] init];
    UIViewController *sec = [[UIViewController alloc] init];
    id split = [[NSClassFromString(@"UISplitViewController") alloc] init];
    [split setViewControllers:@[pri, sec]];
    [[split view] setFrame:CGRectMake(0, 0, 900, 500)];
    [split viewDidLayoutSubviews];
    CHECK(![split isCollapsed] && pri.view.frame.size.width > 0 && sec.view.frame.origin.x + 1 >= pri.view.frame.size.width,
          "UISplitViewController lado a lado");
    [[split view] setFrame:CGRectMake(0, 0, 400, 500)];
    [split viewDidLayoutSubviews];
    CHECK([split isCollapsed] && fabs(sec.view.frame.size.width - 400) < 1, "split estreito fica collapsed no detalhe");
    hovers = 0;
    id hg = [[NSClassFromString(@"UIHoverGestureRecognizer") alloc] initWithTarget:[[HoverAlvo alloc] init] action:@selector(mosHover:)];
    [faixa addGestureRecognizer:hg];
    CGPoint hp = [faixa convertPoint:CGPointMake(10, 10) toView:nil];
    [(_UIHostingView *)w->_nsWindow.contentView _mosPointerMovedTo:hp];
    CHECK(hovers >= 1, "ponteiro vira UIHoverGestureRecognizer (%d)", hovers);
    printf("catalyst: %d/%d ok\n", ok, total);
    fflush(stdout);
}
@end

@interface App : UIResponder
@end
@implementation App
- (BOOL)application:(UIApplication *)a didFinishLaunchingWithOptions:(NSDictionary *)o { launched = 1; return YES; }
- (UISceneConfiguration *)application:(UIApplication *)a configurationForConnectingSceneSession:(UISceneSession *)s options:(id)o {
    UISceneConfiguration *c = [[UISceneConfiguration alloc] initWithName:@"Padrão" sessionRole:UIWindowSceneSessionRoleApplication];
    [c performSelector:@selector(setDelegateClass:) withObject:[Cena class]];
    return c;
}
@end

int main(int argc, char *argv[]) {
    return UIApplicationMain(argc, argv, nil, @"App");
}
