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
}
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
