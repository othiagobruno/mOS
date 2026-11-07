/* mOS — amostra Catalyst mínima para o UIKit.framework do mOS (userland/frameworks/UIKit): app com delegate de cena,
 * como um app UIKit moderno. UIApplicationMain -> application:didFinishLaunching -> configuração da cena ->
 * scene:willConnectToSession: -> UIWindow + UIViewController com uma faixa colorida e um UILabel.
 *   ver tools/test-catalyst.ps1 (compila contra o UIKit do Catalyst, roda no harness e grava a tela em BMP)
 * Imprime "catalyst: N/N ok" quando a cena fica ativa; o laço do NSApplication segue até o watchdog do harness. */
#import "../frameworks/UIKit/UIKitMOS.h"
#include <stdio.h>

int UIApplicationMain(int argc, char *argv[], NSString *principal, NSString *delegate);
extern NSString *const UIWindowSceneSessionRoleApplication;

static int ok, total;
#define CHECK(c, ...) do { total++; if (c) { ok++; printf("  ok   "); } else printf("  FALHA "); printf(__VA_ARGS__); printf("\n"); fflush(stdout); } while (0)
static int launched, connected;
static int toques, cliques, gestos, mudou;

/* toques e teclado como o AppKit os entrega à NSView hospedeira (UIKitEvents.m) */
@interface _UIHostingView (MOSEvents)
- (void)_mosTouchDownAt:(CGPoint)p clickCount:(NSInteger)n;
- (void)_mosTouchUpAt:(CGPoint)p;
- (void)_mosTypeText:(NSString *)s;
- (void)_mosScrollAt:(CGPoint)p dy:(CGFloat)dy;
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
@end
const char *object_getClassName(id o);
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
    printf("DBG hit tabela: %s\n", object_getClassName([w hitTest:CGPointMake(550, 338) withEvent:nil]));
    clicar(w, CGPointMake(550, 240 + 44 * 2 + 10));
    printf("DBG touchStart %g %g sel %d\n", tv->_touchStart.x, tv->_touchStart.y, (int)tv->_selRow);
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
    printf("DBG hit barra: %s\n", object_getClassName([w hitTest:CGPointMake(40, 315) withEvent:nil]));
    clicar(w, CGPointMake(40, 300 + 15));
    CHECK([nav topViewController] == v1 && v1.view.superview == nv, "botão voltar da barra faz pop");
    [w->_nsWindow.contentView setNeedsDisplay:YES];
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
